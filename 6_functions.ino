// X Engineering Alternator Regulator
// Copyright (C) 2026 X Engineering LLC
// Contact: joe@xengineering.net

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, version 3 of the License.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.

// You should have received a copy of the GNU General Public License
// along with this program. If not, see <https://www.gnu.org/licenses/>.
// Forward declarations

void applyImmediateCut(const TickSnapshot &tick, FieldEventReason reason);
// ==================== FIELD CONTROL HELPER FUNCTION DECLARATIONS ====================
// Snapshot builder
TickSnapshot buildTickSnapshot(uint32_t currentMillis, uint32_t dt_ms);
// Mode selection (pure functions)
FieldControlMode selectFieldControlMode(const TickSnapshot &tick);
FieldEventReason selectFieldEventReason(const TickSnapshot &tick);
// Sustained timer functions
bool isTempSustainedWarning(uint32_t nowMs, float tempToUseF, float tempLimitF,
                            float tempWarnExcessF, bool ignoreTemperature);
bool isVoltageDisagreementWarning(uint32_t nowMs, float batteryV, float ibv,
                                  bool voltagePlausible, bool voltageDisagreementCritical);
// Voltage validation
bool isVoltageSensorPlausible();
bool isVoltageDisagreementCritical();
// RPM table shared segment resolver and interpolator
int findRPMSegment(float rpm);
float interpolateRPMTable(float rpm, const float *table);
// RPM-dependent table lookups (all delegate to interpolateRPMTable)
float getMinimumFieldForRPM(float rpm);
float getCapCurrentForRPM(float rpm);
// RPM table index (for UI highlighting and bucket history)
void updateCurrentRPMTableIndex(float rpm);
// GPIO4 cut decision logic
bool shouldImmediatelyCutGPIO4(FieldEventReason reason);
bool shouldCutGPIO4AfterSettle(FieldEventReason reason, uint32_t nowMs, float appliedDuty);
// Telemetry and reporting
void updateFieldTelemetry(float duty, float voltage, float fieldResistance);
void reportFieldModeEvent(uint32_t nowMs, FieldControlMode mode, FieldEventReason reason,
                          const TickSnapshot &tick, bool gpio4Low, float appliedDuty);

// String conversion for logging
const char *modeToString(FieldControlMode mode);
const char *reasonToString(FieldEventReason r);

// ====================================================================================
// FIELD CONTROL MODULE - Refactored with Unified Actuator Governor
// ====================================================================================
//
// NOTE: All enums (GovernorMode, SystemMode), constants (actualDtSec, etc.),
// and global variables (sysMode, govMode, setpointLimited, etc.) are defined
// in the main .ino file. This file contains only function implementations.
//
// ====================================================================================

// ==================== HELPER FUNCTIONS ====================

/**
 * clamp_f - Clamp float to range
 */
float clamp_f(float x, float lo, float hi) {
  if (x < lo) return lo;
  if (x > hi) return hi;
  return x;
}

/**
 * clamp_i - Clamp int to range
 */
int clamp_i(int x, int lo, int hi) {
  if (x < lo) return lo;
  if (x > hi) return hi;
  return x;
}

/**
 * slew_limit_f - Asymmetric slew limiter for floats
 * @param prev     Previous value
 * @param target   Desired value
 * @param rise_per_s  Max rise rate (units/sec)
 * @param fall_per_s  Max fall rate (units/sec)
 * @param dt_sec   Time step (seconds)
 * @return         Slew-limited value
 */
float slew_limit_f(float prev, float target, float rise_per_s, float fall_per_s, float dt_sec) {
  float max_rise = rise_per_s * dt_sec;
  float max_fall = fall_per_s * dt_sec;
  float delta = target - prev;

  if (delta > max_rise) {
    return prev + max_rise;
  }
  if (delta < -max_fall) {
    return prev - max_fall;
  }
  return target;
}

// ==================== PWM OUTPUT ====================

/**
 * apply_pwm_float - Write PWM duty to hardware
 * This is the ONLY function that should call setDutyPercent()
 * (except for hard-cut which bypasses everything)
 */
void apply_pwm_float(float dutyPercent) {
  setDutyPercent(dutyPercent);
}



// ==================== ACTUATOR GOVERNOR ====================
/**
 * governor_apply - Central actuator control
 * 
 * ALL duty changes flow through here (except hard-cut).
 * Applies: clamps → slew (or bypass) → PWM write
 * 
 * @param lastAppliedDuty  Previous applied duty (float)
 * @param requestDutyFloat Requested duty from PID/manual/fault
 * @param gmode            Governor mode (slew/bypass/hold)
 * @param effectiveMinDuty RPM-dependent minimum (0.0 for shutdown modes)
 * @param writeToHardware  false if GPIO4 already LOW (skip PWM write)
 * @return                 The actual duty applied (float)
 */
float governor_apply(float lastAppliedDuty, float requestDutyFloat, int gmode,
                     float effectiveMinDuty, bool writeToHardware, float dtSec) {

  // Hard bounds
  float hardMin = MinDuty;
  float hardMax = MaxDuty;

  // Dynamic bounds (RPM minimum, etc.)
  float dynMin = effectiveMinDuty;
  float dynMax = MaxDuty;

  // Effective bounds: most restrictive
  // For shutdown (effectiveMinDuty=0), allow going below MinDuty
  float finalMin, finalMax;
  if (effectiveMinDuty < 0.01f) {  // Essentially zero (shutdown mode)
    // Shutdown mode: allow 0 to MaxDuty
    finalMin = 0.0f;
    finalMax = (dynMax < hardMax) ? dynMax : hardMax;
  } else {
    // Normal mode: enforce RPM minimum
    finalMin = (dynMin > hardMin) ? dynMin : hardMin;
    finalMax = (dynMax < hardMax) ? dynMax : hardMax;
  }

  // Sanity check
  if (finalMax < finalMin) {
    finalMax = finalMin;
  }

  // Pre-clamp the request
  float requestClamped = clamp_f(requestDutyFloat, finalMin, finalMax);

  // Apply governor mode
  float nextFloat;
  switch (gmode) {
    case GOV_HOLD:
      nextFloat = lastAppliedDuty;
      break;

    case GOV_BYPASS_SLEW:
      nextFloat = requestClamped;
      break;

    case GOV_NORMAL_SLEW:
    default:
      // Slew from last applied duty
      nextFloat = slew_limit_f(lastAppliedDuty, requestClamped,
                               DutyRampRate, DutyRampRate, dtSec);
      // Re-clamp after slew (in case of edge effects)
      nextFloat = clamp_f(nextFloat, finalMin, finalMax);
      break;
  }

  // Keep fractional duty - final clamp
  float nextDuty = clamp_f(nextFloat, finalMin, finalMax);

  // Write to hardware if requested
  if (writeToHardware) {
    apply_pwm_float(nextDuty);
  }

  return nextDuty;
}

// ==================== MODE ENTRY FUNCTIONS (BUMPLESS TRANSFER) ====================
/**
 * enter_sys_off - Transition to OFF mode
 */
void enter_sys_off() {
  sysMode = SYS_MODE_OFF;
  govMode = GOV_NORMAL_SLEW;

  // Put PID in manual, align integrator to current state
  currentPID.SetMode(MANUAL);
  pidOutput = (double)lastAppliedDuty;
  currentPID.ResetIntegratorTo((double)lastAppliedDuty);
}

/**
 * applyImmediateCut - Shared state reset for all immediate GPIO4 cut paths.
 * Every cut path leaves identical state: GPIO4 LOW, PWM 0, PID manual,
 * telemetry updated, fieldActiveStatus cleared.
 */
void applyImmediateCut(const TickSnapshot &tick, FieldEventReason reason) {
  digitalWrite(4, LOW);
  gpio4IsLow = true;
  apply_pwm_float(0.0f);
  lastAppliedDuty = 0.0f;
  dutyCycle = 0.0f;
  sysMode = SYS_MODE_FAULT;
  updateFieldTelemetry(0.0f, tick.currentBatteryVoltage, FieldResistance);
  fieldActiveStatus = 0;
  currentPID.SetMode(MANUAL);
  pidOutput = 0.0;
  currentPID.ResetIntegratorTo(0.0);
  setpointLimited = 0.0f;
  shutdownPhase = SHUTDOWN_PHASE_4;
  shutdownPhaseEntryMs = tick.nowMs;
  prevMode = MODE_CRITICAL_RAMP;
  queueConsoleMessageF("Field cut immediately: %s | ADS=%.2fV INA=%.2fV D=%.3fV",
                       reasonToString(reason), BatteryV, IBV, fabsf(BatteryV - IBV));
}

/**
 * enter_sys_fault - Transition to FAULT mode
 */
void enter_sys_fault() {
  sysMode = SYS_MODE_FAULT;
  // govMode set by caller based on fault severity

  currentPID.SetMode(MANUAL);
  pidOutput = (double)lastAppliedDuty;
  currentPID.ResetIntegratorTo((double)lastAppliedDuty);
}

/**
 * enter_sys_manual - Transition to MANUAL mode
 */
void enter_sys_manual() {
  sysMode = SYS_MODE_MANUAL;
  govMode = GOV_NORMAL_SLEW;  // Manual still respects slew for tach protection

  currentPID.SetMode(MANUAL);
  pidOutput = (double)lastAppliedDuty;
  currentPID.ResetIntegratorTo((double)lastAppliedDuty);
}

/**
 * enter_sys_auto - Transition to AUTO mode
 */
void enter_sys_auto() {
  sysMode = SYS_MODE_AUTO;
  govMode = GOV_NORMAL_SLEW;

  currentPID.SetOutputLimits((double)MinDuty, (double)MaxDuty);
  currentPID.SetSampleTime(100);
  currentPID.SetTunings(PidKp, PidKi, PidKd);
  currentPID.SetTrackingGain(PIDTrackingGain);
  currentPID.SetMode(AUTOMATIC);

  pidOutput = (double)lastAppliedDuty;
  currentPID.ResetIntegratorTo((double)lastAppliedDuty);

  // ---- Charging stage selection on AUTO entry ----
  //
  // Always restart from Bulk. If the battery is already full, voltage rises
  // quickly and the overshoot detection in updateChargingStage() fires within
  // seconds, jumping directly to Absorption or Float — no meaningful spike.
  // Attempting to skip Bulk based on entry voltage caused premature float
  // transitions because terminal voltage under load is not a reliable indicator
  // of state of charge.

  uint32_t now = millis();

  bulkVoltageHoldTimer = 0;
  absorptionTailTimer = 0;
  rebulkTimer = 0;

  inBulkStage = true;
  inAbsorptionStage = false;
  inIdleStage = false;
  floatStartTime = 0;
  queueConsoleMessageF(
    "enter_sys_auto: starting in BULK (%.2fV)", getBatteryVoltage());

  // Always seed from alternator current regardless of MaintainMode
  float actualCurrent = max(0.0f, getTargetAmps());
  setpointLimited = actualCurrent;
  setpointInitialized = true;
}

// ==================== MAIN CONTROL FUNCTION ====================


/**
 * AdjustFieldLearnMode - Main field control
 *
 * Architecture:
 *  1.  Build TickSnapshot and timing state
 *  2.  Pre-gate immediate-cut fault check
 *  3.  Fast voltage safety override (runs every loop, before CH1 gate)
 *  4.  Limp-home handling
 *  5.  Gate on fresh CH1 ADC data, with optional PidSampleDivisor
 *  6.  Re-check critical faults and determine control/system mode
 *  7.  Handle mode transitions
 *  8.  Non-normal path: shutdown/fault state machine, then return
//  9.  Normal path: iExcess supervisor, fastOvCurrentCap application,
//                   setpoint management and PID compute
 *  10. Build duty request
 *  11. Apply through governor
 *  12. Tell PID what actually happened
 *  13. Update state, telemetry, and logging
 */


// ============================================================
// PID / CONTROL LOOP TUNING CONSTANTS — ALL LOOPS
// Web-configurable values stored in LittleFS; compile-time
// defaults shown. LittleFS values override on boot.
// "hardcoded" = not exposed in web UI, only changeable here.
// ============================================================
//
// ── INNER CURRENT PID ────────────────────────────────────────
//   Runs every CH1 ADS1115 sample (÷ PidSampleDivisor).
//   ADS sequence {1,0,1,2,1,3}: CH1 fires 3× per 6-step cycle.
//   Back-to-back trigger in ADS_READ_RESULT saves one loop() call per channel.
//
//   Measured performance (preliminary — full hardware validation pending):
//   CH1 interval: ~5ms typical; worst-case not yet characterised.
//   PidSampleDivisor=1 (default) → PID runs every CH1 hit.
//   PidSampleDivisor=2           → every other CH1 hit.
//
//   Kp (proportional)          0.500       web UI default
//   Ki (integral)              2.000       web UI default
//   Kd (derivative)            0.010       web UI default
//   Tracking gain (1/s)        4.00        anti-windup back-calculation
//   Max duty output            MinDuty=1% … MaxDuty=99%
//
// ── VOLTAGE / CV OUTER LOOP ──────────────────────────────────
//   Kp (A/V)                 25.00        web UI  (globals default: 20.0)
//   Ki                       2.500        web UI  (globals default: 2.0)
//   Loop interval            100 ms        web UI

//   Asymmetric Ki: unwind rate = 7× Ki above target (K_DOWN — hardcoded in CV loop body)
//   History: baseline 3× (too slow, cv_I drifted high), tried 10× (over-wound, integrator
//   froze), settled at 7× as interpolated middle. Do not reduce below 5× or cv_I
//   accumulates during blips; do not exceed 10× or steady-state centering degrades.
//
// ── FAST OV VOLTAGE SUPERVISOR (backstop only) ───────────────
//   Runs every loop before the CH1 gate. Now secondary to the fast
//   current rise supervisor for CV-mode transient protection.
//   Still active as a last-resort voltage backstop (sensor glitch,
//   non-CV modes, cases where iExcess detection misses).
// battVFreshFlag fires at ~15ms cadence (CH0 gets 1 of 6 ADS slots; CH1 interval ~5ms).
// dvdt EMA alpha=0.08 → effective time constant ~190ms.
//   Prediction horizon      80 ms        (TD_PRED = 0.08f)
//   Soft correction zone    ChargingVoltageTarget + 0.08 V  (V_SOFT)
//   Hard correction zone    ChargingVoltageTarget + 0.15 V  (V_HARD)
//   Soft gain               12.0 A/V     (K_SOFT)
//   Hard gain               35.0 A/V     (K_HARD)
//   Predictive guard band   0.06 V       (PRED_GUARD)
//   Hard clamp hysteresis   0.08 V       (HARD_CLAMP_HYST)
//
// ── TEMPERATURE PID (outer thermal loop) ─────────────────────
//   Kp (A/°F)                  3.000       web UI
//   Ki                         0.025       web UI
//   Kd (library term)          0.000       web UI  (effectively disabled)
//   Kd External (A/°F/s)     200.000       web UI  (globals default: 200.0 — this basically creates noise, needs updating)
//   Setpoint margin           15.00 °F     web UI  (SP = TemperatureLimitF - margin)
//   Loop interval           5000 ms        web UI (may need toa djust this to fix the Kd External noise mentioned above??)
//   Filter alpha               0.200       web UI  (IIR on raw temp; 0=frozen, 1=raw)
//   Stale hold threshold    15000 ms        (hardcoded TempPIDStaleMs)
//   Thermal penalty rise rate 60.0 A/s     (hardcoded ThermalPenaltyRiseRate)
//   Thermal penalty fall rate 20.0 A/s     (hardcoded ThermalPenaltyFallRate)
//   Temperature limit         150 °F        web UI  (TemperatureLimitF), changes frequently while i tune
//   Temp warn excess          10.0 °F       above limit → WARNING ramp + lockout
//   Temp crit excess          30.0 °F       above limit → immediate GPIO4 cut
//   Sustained warn timeout 120000 ms        WARNING held this long → GPIO4 cut
//
// ── SETPOINT / GOVERNOR RATES ────────────────────────────────
//   Setpoint rise rate        30.00 A/s     web UI
//   Setpoint fall rate        50.00 A/s     web UI
//   Duty ramp rate            80.00 %/s     web UI  (globals default: 50.0)
//   Shutdown slow ramp         1.00 %/s     web UI  (DutySlowRampRate, phase 3)
//   Shutdown phase 2 hold      0    ms      web UI  (0 = skip phase 2)
//   Settle time before GPIO4 cut 1000 ms    (hardcoded SettleTimeBeforeCut)
//
// ── RPM / CURRENT TABLE (learning baseline) ──────────────────
//   Table size: 10 points  (hardcoded RPM_TABLE_SIZE)
//   RPM breakpoints: {100, 600, 1100, 1600, 2100, 2600, 3100, 3600, 4100, 4600}
//   Default current targets:  {0, 70, 70, 80, 80, 90, 90, 90, 90, 90} A
//   Default cap current:      {120 × 10} A  (flat 120A ceiling, all RPM points)
//   Default min duty table:   {18, 18, 18, 10, 10, 5, 5, 5, 5, 5} %
//   MaxTableValue             150.0 A       (absolute ceiling, web UI)
//   MinTableValue               0.0 A       (hardcoded)
//
// ── VOLTAGE SPIKE / DISAGREEMENT THRESHOLDS ─────────────────
//   Voltage spike margin       0.30 V       above BulkVoltage → warning
//   Voltage disagree threshold 0.15 V       between BatteryV and IBV
//   Voltage disagree timeout  10000 ms      → warning
//   Voltage disagree critical  3000 ms      → immediate cut
//
// ── LEARNING / PENALTY (thermal overheat response) ──────────
//   Max penalty %             15.0          of AlternatorNominalAmps
//   Max penalty duration   60000 ms
//   Min learning interval  30000 ms
//   Safe operation threshold 30000 ms       before upward learning allowed
//   Neighbor learning factor   0.25         adjacent RPM point reduction
//   Learning up step           1.0 A
//   Learning down step         2.0 A
//   Learning settling period 30000 ms       after RPM change > 500 RPM
//
// ── FIELD COLLAPSE / LOCKOUT ─────────────────────────────────
//   Field collapse delay      30000 ms      before restart after fault
//
//// ── ADS1115 TIMING (inner loop clock source) ─────────────────
//   Conversion time    ~1.16 ms theoretical at 860 SPS.
//   Sequence {1,0,1,2,1,3}: CH1 at positions 0,2,4 → 3× per cycle.
//   Back-to-back trigger fires next conversion at end of ADS_READ_RESULT,
//   collapsing 3 loop() calls per channel to 2.
//
//   Measured performance (preliminary — full hardware validation pending):
//   loop() duration:  3–14ms typical; one-time spike ~150ms on first client page load.
//   ADS read time:    ~2ms avg.
//   CH1 interval:     ~5ms typical.
//   CH0/CH2/CH3:      1× per cycle → ~3× CH1 interval avg cadence.
//   Worst-case intervals not yet characterised under full load with all hardware present.
//   Verify via ch1IntervalMs in cvLog after any loop() cadence changes.
//
//   Channel assignments: CH0=BatteryV  CH1=MeasuredAmps  CH2=RPM  CH3=Thermistor
//   PidSampleDivisor=1 (default) → inner PID runs every CH1 hit.
//   ADS_TIMEOUT_MS = 50          → conversion timeout before retry.
//
// ============================================================

void AdjustFieldLearnMode() {

  // ========== TIMING ==========
  static uint32_t lastControlTickMs = 0;
  uint32_t currentMillis = millis();

  uint32_t actualDtMs = (lastControlTickMs == 0) ? 62 : (currentMillis - lastControlTickMs);
  if (actualDtMs > 500) actualDtMs = 500;
  float actualDtSec = (float)actualDtMs / 1000.0f;

  static float uTargetRaw_cached = 50.0f;   // pre-OV ceiling from last normal tick; used by fast OV supervisor
  float uTargetRaw = (float)MaxTableValue;  // pre-OV ceiling this tick, set in AUTO path
  float fastOvBaseCap = clamp_f(uTargetRaw_cached, 0.0f, (float)MaxTableValue);
  float fastOvCurrentCap = fastOvBaseCap;
  bool fastOvClampActive = false;
  static uint32_t ocTripStartMs = 0;

  updateCurrentRPMTableIndex(RPM);
  updateRPMBucketHistory(currentMillis);

  TickSnapshot tick = buildTickSnapshot(currentMillis, actualDtMs);
  bool hardOCActive = false;

  if (MeasuredAmps > HardOCTripAmps) {
    if (hardOCStartMs == 0) hardOCStartMs = currentMillis;
    if ((currentMillis - hardOCStartMs) >= HardOCDebounceMs) {
      hardOCActive = true;
    }
  } else {
    hardOCStartMs = 0;
  }
  // pidLog_tick() runs at the END of the normal control path, after all state is final.

  // thermalLog_tick() is called after tempPID_tick() in the normal-mode path so that
  // outerAntiWindupFired and outerTermI reflect the current tick's state rather than
  // the previous tick's.

  isTempSustainedWarning(tick.nowMs, tick.tempToUseF, tick.tempLimitF,
                         tick.tempWarnExcessF, tick.ignoreTemperature);

  if (innerPIDResetRequested) {
    innerPIDResetRequested = false;
    currentPID.SetMode(MANUAL);
    pidOutput = 0.0;
    currentPID.ResetIntegratorTo(0.0);
    lastAppliedDuty = 0.0f;
    setpointInitialized = false;
    queueConsoleMessage("InnerPID: manual reset requested - integrator cleared");
  }

  // ========== PRE-GATE IMMEDIATE CUT CHECK ==========
  // Runs before the CH1 gate so INA overvoltage always cuts regardless of sensor freshness.
  FieldEventReason preReason = selectFieldEventReason(tick);
  if (shouldImmediatelyCutGPIO4(preReason)) {
    applyImmediateCut(tick, preReason);
    return;
  }


  // ===== FAST VOLTAGE SAFETY OVERRIDE ==========
  // Runs every loop before the CH1 gate.
  // Computes fastOvCurrentCap — a per-tick hard ceiling on commanded current.
  // Applied to uTargetAmps after RPM/thermal/user overrides in the AUTO path.
  // Direct cv_I clamp kept here because the CV loop only runs every 100ms;
  // without it cv_I builds positive for up to 100ms while battV is above target.

  {
    static float vPrev = 0.0f;
    static uint32_t vPrevMs = 0;
    static float dvdt = 0.0f;
    static bool ovActive = false;
    static float preEventIcv = 0.0f;

    g_fastOvSoftActive = false;
    g_fastOvHardActive = false;
    g_fastOvVpred = BatteryV;

    if (battVFreshFlag) {
      battVFreshFlag = false;
      if (vPrevMs > 0) {
        float dtV = (currentMillis - vPrevMs) / 1000.0f;
        if (dtV > 0.001f && dtV < 0.1f) {
          float raw = (BatteryV - vPrev) / dtV;
          dvdt = 0.08 * raw + 0.92 * dvdt;
        }
      }
      vPrev = BatteryV;
      vPrevMs = currentMillis;
    }
    g_fastOvDvdt = dvdt;

    if (voltageControlActive) {
      const float TD_PRED = 0.08f;
      const float V_SOFT = ChargingVoltageTarget + 0.08f;
      const float V_HARD = ChargingVoltageTarget + 0.15f;
      const float K_SOFT = 12.0f;
      const float K_HARD = 35.0f;
      const float PRED_GUARD = 0.06f;

      float Vpred = BatteryV + TD_PRED * fmaxf(0.0f, dvdt);
      g_fastOvVpred = Vpred;

      if (!ovActive) {
        preEventIcv = Icv;
      }

      if (BatteryV > ChargingVoltageTarget - PRED_GUARD) {
        if (Vpred > V_SOFT) {
          float softCap = fmaxf(0.0f, fastOvBaseCap - K_SOFT * (Vpred - V_SOFT));
          fastOvCurrentCap = fminf(fastOvCurrentCap, softCap);
          fastOvClampActive = true;
          ovActive = true;
          g_fastOvSoftActive = true;
        }
        if (Vpred > V_HARD) {
          float hardCap = fmaxf(0.0f, fastOvBaseCap - K_HARD * (Vpred - V_HARD));
          fastOvCurrentCap = fminf(fastOvCurrentCap, hardCap);
          fastOvClampActive = true;
          g_fastOvHardActive = true;
        }
      }

      const float HARD_CLAMP_HYST = 0.08f;
      if (BatteryV > ChargingVoltageTarget + HARD_CLAMP_HYST) {
        float ovExcess = BatteryV - (ChargingVoltageTarget + HARD_CLAMP_HYST);
        float hystCap = fmaxf(0.0f, fastOvBaseCap - K_HARD * ovExcess);
        fastOvCurrentCap = fminf(fastOvCurrentCap, hystCap);
        fastOvClampActive = true;
        ovActive = true;
        g_fastOvHardActive = true;
      }

      // Recovery seed: fires once when clamp de-asserts.
      if (ovActive
          && (BatteryV <= ChargingVoltageTarget)
          && (Vpred <= V_SOFT)) {

        float e = ChargingVoltageTarget - BatteryV;
        float icvHi = clamp_f(uTargetRaw_cached, 0.0f, (float)MaxTableValue);

        // cv_I = clamp_f(preEventIcv - VoltageKp * e, 0.0f, icvHi);  // likely to remove permanently
        Icv = clamp_f(VoltageKp * e + cv_I, 0.0f, icvHi);
        setpointLimited = Icv;

        ovActive = false;

        // queueConsoleMessageF(
        //   "FastOV release | BattV=%.3fV e=%.3fV preEventIcv=%.2fA "
        //   "cv_I_seed=%.2fA Icv=%.2fA spLim=%.2fA",
        //   BatteryV, e, preEventIcv, cv_I, Icv, setpointLimited);
        // Serial.printf(
        //   "FastOV release | BattV=%.3fV e=%.3fV preEventIcv=%.2fA "
        //   "cv_I_seed=%.2fA Icv=%.2fA spLim=%.2fA\n",
        //   BatteryV, e, preEventIcv, cv_I, Icv, setpointLimited);
      }
    } else {
      ovActive = false;
      // fastOvCurrentCap stays at MaxTableValue (per-tick default)
    }
  }

  // ── Fast OV telemetry export ──────────────────────────────────────────────
  static bool g_fastOvSoftActive_prev = false;
  static bool g_fastOvHardActive_prev = false;

  g_fastOvCurrentCap = fastOvCurrentCap;

  if (fastOvClampActive && !g_fastOvClampActive) {
    g_fastOvClampCount++;  // rising-edge only — count each new FastOV activation
  }
  if (g_fastOvSoftActive && !g_fastOvSoftActive_prev) {
    g_fastOvSoftCount++;  // rising-edge only — count each new soft FastOV activation
  }
  if (g_fastOvHardActive && !g_fastOvHardActive_prev) {
    g_fastOvHardCount++;  // rising-edge only — count each new hard FastOV activation
  }

  g_fastOvClampActive = fastOvClampActive;
  g_fastOvSoftActive_prev = g_fastOvSoftActive;
  g_fastOvHardActive_prev = g_fastOvHardActive;

  // ========== EMERGENCY LIMP HOME MODE (runs every loop) ==========
  // WARNING: BYPASSES ALL SAFETY SYSTEMS
  if (LimpHome == 1) {
    if (OnOff == 0) {
      digitalWrite(4, LOW);
      gpio4IsLow = true;
      apply_pwm_float(0.0f);
      lastAppliedDuty = 0.0f;
      dutyCycle = 0.0f;
      return;
    }
    static uint32_t lastLimpTick = 0;
    if (currentMillis - lastLimpTick >= 100) {
      lastLimpTick = currentMillis;
      digitalWrite(4, HIGH);
      gpio4IsLow = false;
      apply_pwm_float(30.0f);
      lastAppliedDuty = 30.0f;
      dutyCycle = 30.0f;
      currentPID.SetMode(MANUAL);
      pidOutput = 30.0;
      currentPID.ResetIntegratorTo(30.0);
      digitalWrite(21, LOW);
      updateFieldTelemetry(30.0f, tick.currentBatteryVoltage, FieldResistance);
      fieldActiveStatus = 1;
      static unsigned long lastLimpReport = 0;
      if (currentMillis - lastLimpReport >= 30000) {
        lastLimpReport = currentMillis;
        queueConsoleMessage("LIMP HOME MODE: 30% duty, all safeties bypassed");
        Serial.println("LIMP HOME MODE: 30% duty, all safeties bypassed");
      }
    }
    return;
  }

  // ========== GATE ON FRESH CH1 DATA ==========
  // PidSampleDivisor=1: PID runs every CH1 sample (~16Hz)
  // PidSampleDivisor=2: every other sample (~8Hz), etc.
  static uint8_t ch1SampleCount = 0;

  if (!ch1FreshFlag) {
    // Manual mode does not need current sensor data — let it through.
    if (!tick.manualMode) {
      return;
    }
  } else {
    ch1SampleCount++;
    if (ch1SampleCount < PidSampleDivisor) {
      ch1FreshFlag = false;
      return;
    }
    ch1FreshFlag = false;
    ch1SampleCount = 0;
  }
  lastControlTickMs = currentMillis;

  // ── Reset per-tick pidLog capture globals ────────────────────────────────
  // Written at specific points below; read by pidLog_tick() at end of normal
  // path. Reset here so a skipped branch leaves a clean zero, not a stale value.
  // pidLog_vError is always fresh — computed every tick regardless of loop interval.
  pidLog_vError = ChargingVoltageTarget - tick.currentBatteryVoltage;
  pidLog_uTargetBeforeVoltCap = 0.0f;
  pidLog_uTargetAfterVoltCap = 0.0f;
  pidLog_dutyRequest = 0.0f;
  pidLog_dutyApplied = 0.0f;
  pidLog_voltageLoopRanThisTick = 0;
  pidLog_enteringCV = 0;
  pidLog_enteringTargetVoltageMode = 0;

  // ========== FAST-PATH: critical fault check ==========
  FieldControlMode mode = selectFieldControlMode(tick);
  FieldEventReason reason = selectFieldEventReason(tick);

  if (shouldImmediatelyCutGPIO4(reason) && !gpio4IsLow) {
    applyImmediateCut(tick, reason);
    return;
  }

  // ========== PERIODIC STATE DUMP (30 min) ==========
  static unsigned long lastDebugDump = 0;
  if (currentMillis - lastDebugDump >= 1800000) {
    lastDebugDump = currentMillis;
    char msg1[100], msg2[80], msg3[100];
    snprintf(msg1, sizeof(msg1), "System State: Ignition=%d OnOff=%d Manual=%d",
             Ignition, OnOff, ManualFieldToggle);
    snprintf(msg2, sizeof(msg2), "Charging: %s Mode=%s",
             tick.chargingEnabled ? "ENABLED" : "DISABLED",
             tick.manualMode ? "MANUAL" : "AUTO");
    snprintf(msg3, sizeof(msg3), "Sensors: RPM=%.0f Volts=%.2f Temp=%.0f°F",
             RPM, tick.currentBatteryVoltage, tick.tempToUseF);
    Serial.println("=== STATUS ===");
    Serial.println(msg1);
    Serial.println(msg2);
    Serial.println(msg3);
    Serial.println("==============");
    queueConsoleMessage(msg1);
    queueConsoleMessage(msg2);
    queueConsoleMessage(msg3);
  }

  // ========== OVERRIDE MODE ENTRY DETECTION ==========
  // Rising-edge detection so one-shot actions (log messages, integrator seeds)
  // fire exactly once on activation, not every tick.
  static bool lastMaintainMode = false;
  static bool lastTargetVoltageMode = false;
  bool enteringMaintainMode = (MaintainMode == 1) && !lastMaintainMode;
  bool enteringTargetVoltageMode = (TargetVoltageMode == 1) && !lastTargetVoltageMode;
  lastMaintainMode = (MaintainMode == 1);
  lastTargetVoltageMode = (TargetVoltageMode == 1);

  // ========== CHARGING STAGE (bulk/absorption/float) ==========
  // Suppressed while either override is active — letting it run would fire
  // spurious re-bulk transitions and absorption timeouts. On override exit,
  // enter_sys_auto() re-evaluates stage from current voltage.
  if (MaintainMode != 1 && TargetVoltageMode != 1) {
    updateChargingStage();
  }

  // Report mode changes
  static FieldControlMode lastDebugMode = (FieldControlMode)255;
  static FieldEventReason lastDebugReason = (FieldEventReason)255;
  if (mode != lastDebugMode || reason != lastDebugReason) {
    char modeMsg[100];
    snprintf(modeMsg, sizeof(modeMsg), "Control Mode: %s - %s",
             modeToString(mode), reasonToString(reason));
    Serial.println(modeMsg);
    queueConsoleMessage(modeMsg);
    lastDebugMode = mode;
    lastDebugReason = reason;
  }

  if ((mode != MODE_NORMAL_AUTO_PID && mode != MODE_NORMAL_MANUAL) && reason == REASON_NONE) {
    Serial.println("ERROR: Shutdown mode with no reason specified");
  }

  chargingEnabled = tick.chargingEnabled;

  // ========== DETERMINE GOVERNOR MODE ==========
  govMode = GOV_NORMAL_SLEW;

  // Major overvoltage: bypass slew for fast field collapse
  if (tick.currentBatteryVoltage > (tick.bulkVoltage + tick.voltageSpikeMargin + 0.5f)) {
    govMode = GOV_BYPASS_SLEW;
  }
  // Voltage sensor failure: bypass slew
  if (!tick.voltagePlausible || tick.voltageDisagreementCritical) {
    govMode = GOV_BYPASS_SLEW;
  }

  // CV overshoot: bypass duty slew so the inner PID can reduce field current without
  // the 80%/s governor rate limit holding it back.
  //
  // Trigger: fastOvClampActive (fast OV supervisor in soft zone, Vpred > V_SOFT).
  // This inherits the dvdt EMA (alpha=0.08) + PRED_GUARD noise filtering already
  // present in the fast OV supervisor, so it does not fire on measurement noise
  // near target — addressing the original nuisance-trigger concern that led to the
  // old +0.12 V raw-voltage threshold.
  //
  // Why fastOvClampActive beats a raw voltage threshold:
  // Log analysis (pidlog__27__0_5s_5s.csv, 2026-04-15) showed the old +0.12 V
  // threshold triggered at the exact voltage peak (first tick above +0.12 V = peak
  // itself), providing zero benefit on the ascent. fastOvClampActive triggered 90 ms
  // earlier at +0.05–0.09 V over target — the window where the inner PID was being
  // held 8–9% duty below its desired output by the slew limiter.
  //
  // Hysteresis: latch stays set until battV falls back to within 0.02 V of target.
  // Without this, bypass would toggle on/off rapidly as battV oscillates through the
  // V_SOFT boundary during descent, causing duty chatter.
  //
  // Only active in CV stages (voltageControlActive). In bulk, voltage rising above
  // target is expected and govBypass is not wanted.
  {
    static bool cvGovBypassLatch = false;
    if (voltageControlActive) {
      if (fastOvClampActive) {
        cvGovBypassLatch = true;
      } else if (BatteryV < ChargingVoltageTarget + 0.02f) {
        cvGovBypassLatch = false;
      }
      if (cvGovBypassLatch) {
        govMode = GOV_BYPASS_SLEW;
      }
    } else {
      cvGovBypassLatch = false;  // reset on CV exit so it doesn't carry into next CV entry
    }
  }

  // ========== MODE TRANSITION HANDLING ==========
  bool isNormalMode = (mode == MODE_NORMAL_AUTO_PID || mode == MODE_NORMAL_MANUAL);
  bool wasNormalMode = (prevMode == MODE_NORMAL_AUTO_PID || prevMode == MODE_NORMAL_MANUAL);
  bool enteringNormal = !wasNormalMode && isNormalMode;
  bool exitingNormal = wasNormalMode && !isNormalMode;

  SystemMode newSysMode;
  if (!tick.chargingEnabled) newSysMode = SYS_MODE_OFF;
  else if (mode == MODE_NORMAL_MANUAL) newSysMode = SYS_MODE_MANUAL;
  else if (mode == MODE_NORMAL_AUTO_PID) newSysMode = SYS_MODE_AUTO;
  else newSysMode = SYS_MODE_FAULT;

  if (newSysMode != sysMode) {
    switch (newSysMode) {
      case SYS_MODE_OFF:
        enter_sys_off();
        queueConsoleMessage("Charging disabled");
        Serial.println("Charging disabled");
        break;
      case SYS_MODE_MANUAL:
        enter_sys_manual();
        if (enteringNormal) {
          shutdownPhase = SHUTDOWN_PHASE_NONE;
          shutdownPhaseEntryMs = 0;
          shutdownPhase2EntryMs = 0;
          settledAtZeroDutyMs = 0;
          queueConsoleMessage("Charging enabled (MANUAL)");
          Serial.println("Charging enabled (MANUAL)");
        }
        break;
      case SYS_MODE_AUTO:
        enter_sys_auto();
        pidInitialized = true;
        if (enteringNormal) {
          shutdownPhase = SHUTDOWN_PHASE_NONE;
          shutdownPhaseEntryMs = 0;
          shutdownPhase2EntryMs = 0;
          settledAtZeroDutyMs = 0;
          queueConsoleMessage("Charging enabled (AUTO)");
          Serial.println("Charging enabled (AUTO)");
        }
        break;
      case SYS_MODE_FAULT:
        enter_sys_fault();
        if (exitingNormal) {
          queueConsoleMessageF("Charging stopped: %s | ADS=%.2fV INA=%.2fV D=%.3fV",
                               reasonToString(reason), BatteryV, IBV, fabsf(BatteryV - IBV));
        }
        break;
    }
  }

  // ========== LOCKOUT TRANSITION TRACKING ==========
  bool lockoutActiveNow = tick.inLockout;
  if (lockoutWasActive && !lockoutActiveNow) {
    queueConsoleMessage(isNormalMode ? "Cooldown complete - charging resumed" : "Cooldown complete");
    Serial.println(isNormalMode ? "Cooldown complete - charging resumed" : "Cooldown complete");
  }
  lockoutWasActive = lockoutActiveNow;

  if (fieldCollapseTime > 0 && (tick.nowMs - fieldCollapseTime) >= FIELD_COLLAPSE_DELAY) {
    fieldCollapseTime = 0;
  }
  float dutyNewFloat = 0.0f;

  // ========== NON-NORMAL MODE: SHUTDOWN / FAULT HANDLING ==========
  if (!isNormalMode) {
    voltageControlActive = false;

    digitalWrite(21, (mode == MODE_DISABLED_RAMP) ? LOW : HIGH);

    uTargetAmps = 0;
    setpointLimited = 0.0f;

    if (exitingNormal) {
      shutdownPhase = SHUTDOWN_PHASE_1;
      shutdownPhaseEntryMs = tick.nowMs;
    }
    if (shutdownPhase == SHUTDOWN_PHASE_NONE) {
      shutdownPhase = SHUTDOWN_PHASE_1;
      shutdownPhaseEntryMs = tick.nowMs;
    }

    float effectiveMin = 0.0f;
    float dutyRequest = 0.0f;

    // Critical faults skip the slow ramp entirely
    bool isCriticalFault = (reason == REASON_VOLTAGE_DISAGREE_CRITICAL || reason == REASON_VOLTAGE_IMPLAUSIBLE);

    if (isCriticalFault) {
      shutdownPhase = SHUTDOWN_PHASE_4;
      effectiveMin = 0.0f;
      dutyRequest = 0.0f;
    } else {
      // Phase 1: ramp to rpmMinDuty at DutyRampRate
      if (shutdownPhase == SHUTDOWN_PHASE_1) {
        effectiveMin = 0.0f;
        dutyRequest = tick.rpmMinDuty;
        if (lastAppliedDuty <= tick.rpmMinDuty + 0.1f) {
          if (ShutdownPhase2HoldMs > 0) {
            if (shutdownPhase2EntryMs == 0) shutdownPhase2EntryMs = tick.nowMs;
            if (tick.nowMs - shutdownPhase2EntryMs >= ShutdownPhase2HoldMs) {
              shutdownPhase = SHUTDOWN_PHASE_3;
              shutdownPhaseEntryMs = tick.nowMs;
              shutdownPhase2EntryMs = 0;
            }
          } else {
            shutdownPhase = SHUTDOWN_PHASE_3;
            shutdownPhaseEntryMs = tick.nowMs;
            shutdownPhase2EntryMs = 0;
          }
        }
      }

      // Phase 3: slow ramp from rpmMinDuty to 0
      if (shutdownPhase == SHUTDOWN_PHASE_3) {
        float slowDuty = slew_limit_f(lastAppliedDuty, 0.0f,
                                      DutySlowRampRate, DutySlowRampRate, actualDtSec);
        effectiveMin = 0.0f;
        dutyRequest = slowDuty;
        if (lastAppliedDuty <= 0.01f) {
          shutdownPhase = SHUTDOWN_PHASE_4;
          shutdownPhaseEntryMs = tick.nowMs;
        }
      }

      // Phase 4: hold at 0, wait for flux collapse, then cut GPIO4
      if (shutdownPhase == SHUTDOWN_PHASE_4) {
        effectiveMin = 0.0f;
        dutyRequest = 0.0f;
      }
    }

    bool writeToHardware = !gpio4IsLow;
    // Phase 3: slew already applied above — bypass governor's own slew,
    // but keep clamping.
    GovernorMode shutdownGovMode = (shutdownPhase == SHUTDOWN_PHASE_3) ? GOV_BYPASS_SLEW : govMode;

    dutyNewFloat = governor_apply(lastAppliedDuty, dutyRequest, shutdownGovMode,
                                  effectiveMin, writeToHardware, actualDtSec);
    currentPID.ResetIntegratorTo((double)dutyNewFloat);
    pidOutput = (double)dutyNewFloat;

    if (writeToHardware) lastAppliedDuty = dutyNewFloat;
    dutyCycle = dutyNewFloat;

    if (!gpio4IsLow) {
      if (shutdownPhase == SHUTDOWN_PHASE_4 && shouldCutGPIO4AfterSettle(reason, tick.nowMs, dutyNewFloat)) {
        digitalWrite(4, LOW);
        gpio4IsLow = true;
        queueConsoleMessageF("Field disabled: %s | ADS=%.2fV INA=%.2fV D=%.3fV",
                             reasonToString(reason), BatteryV, IBV, fabsf(BatteryV - IBV));
      } else {
        digitalWrite(4, HIGH);
      }
    }

    updateFieldTelemetry(dutyCycle, tick.currentBatteryVoltage, FieldResistance);

    if (gpio4IsLow) {
      fieldActiveStatus = 0;
    } else if (shutdownPhase == SHUTDOWN_PHASE_1 || shutdownPhase == SHUTDOWN_PHASE_3) {
      fieldActiveStatus = 2;
    } else if (lastAppliedDuty > 0.01f) {
      fieldActiveStatus = 1;
    } else {
      fieldActiveStatus = 0;
    }
    chargeStageDisplay = getChargeStageDisplayCode();

    if ((mode == MODE_CRITICAL_RAMP || mode == MODE_WARNING_RAMP_AND_LOCKOUT) && fieldCollapseTime == 0) {
      fieldCollapseTime = tick.nowMs;
      queueConsoleMessageF("Charging stopped - %.1fs cooldown before restart",
                           FIELD_COLLAPSE_DELAY / 1000.0f);
    }

    reportFieldModeEvent(tick.nowMs, mode, reason, tick, gpio4IsLow, dutyCycle);
    prevMode = mode;
    return;
    // NOTE: pidLog_tick() is NOT called in the shutdown/fault path.
  }

  // ========== NORMAL MODES (MANUAL or AUTO) ==========
  // Re-check before enabling GPIO4 — fault could have arrived this tick.
  if (shouldImmediatelyCutGPIO4(reason)) {
    applyImmediateCut(tick, reason);
    return;
  }
  digitalWrite(4, HIGH);
  gpio4IsLow = false;
  digitalWrite(21, LOW);

  // Seed setpoint tracking on first normal tick after startup or re-enable.
  if (!setpointInitialized) {
    setpointLimited = max(0.0f, getTargetAmps());
    setpointInitialized = true;
  }

  // ========== SYSTEM ID OVERRIDE ==========
  // When a plant delay measurement test is running, bypass all setpoint
  // machinery and PID computation. Duty is commanded directly by
  // systemID_tick(). govMode is forced to GOV_BYPASS_SLEW so the
  // governor does not slew or clamp the step transitions.
  // The integrator is parked at the commanded duty so the PID resumes
  // smoothly from the operating point when the test ends.
  // ========== SYSTEM ID OVERRIDE ==========
  float sysIDDutyOut = lastAppliedDuty;

  static bool prevSysIDRunning = false;
  bool sysIDRunning = systemID_tick(sysIDDutyOut, MeasuredAmps, tick.nowMs);

  bool sysIDJustStarted = !prevSysIDRunning && sysIDRunning;
  bool sysIDJustCompleted = prevSysIDRunning && !sysIDRunning;
  prevSysIDRunning = sysIDRunning;

  // ── Snapshot on test start (fires once, first tick sysIDRunning goes true) ──
  // All pre-test values are still clean at this point — the override block below
  // hasn't touched PID or setpoint yet this tick.
  if (sysIDJustStarted) {
    g_sysIDResume = {
      .valid = true,
      .sysMode = sysMode,
      .setpointLimited_snap = setpointLimited,
      .lastAppliedDuty_snap = lastAppliedDuty,
      .cv_I_snap = cv_I,
      .voltageControlActive_snap = voltageControlActive,
    };
    queueConsoleMessageF(
      "SystemID: pre-test state captured | mode=%s duty=%.1f%% setpoint=%.2fA cv_I=%.2fA cvActive=%d",
      (sysMode == SYS_MODE_MANUAL) ? "MANUAL" : "AUTO",
      g_sysIDResume.lastAppliedDuty_snap,
      g_sysIDResume.setpointLimited_snap,
      g_sysIDResume.cv_I_snap,
      g_sysIDResume.voltageControlActive_snap);
  }

  // ── While test is running: bypass all setpoint machinery ────────────────────
  if (sysIDRunning) {
    govMode = GOV_BYPASS_SLEW;
    currentPID.SetMode(MANUAL);
    currentPID.ResetIntegratorTo((double)sysIDDutyOut);
    pidOutput = (double)sysIDDutyOut;
  }

  // ── Restore on test completion ───────────────────────────────────────────────
  // Hierarchy:
  //   1. Safety wins — only restore if still in a legal normal-running state.
  //   2. Restore control architecture (mode, setpoint tracker, CV integrator).
  //   3. Bumpless seed — integrator gets CURRENT applied duty, NOT the pre-test
  //      duty. Pre-test duty is stale; conditions (voltage, RPM, temp) may have
  //      changed during the test. Current applied duty is the actual operating point.
  //   4. Let the normal control path compute the next command immediately.
  if (sysIDJustCompleted && g_sysIDResume.valid) {
    bool legalToResume = (mode == MODE_NORMAL_AUTO_PID || mode == MODE_NORMAL_MANUAL);

    if (!legalToResume) {
      // A fault or disable arrived during the test. Do not force old state back;
      // let the current fault path win on its own terms this tick and beyond.
      queueConsoleMessage(
        "SystemID: pre-test state NOT restored — system no longer in normal running state");
    } else {

      // Restore setpoint tracker, clamped to the current valid ceiling.
      // This prevents re-asserting demand that was legal pre-test but is now
      // above the RPM/thermal/user cap.
      float restoredSetpoint = clamp_f(
        g_sysIDResume.setpointLimited_snap, 0.0f, (float)MaxTableValue);
      setpointLimited = restoredSetpoint;

      // Restore CV integrator only when both pre- and post-test state are in CV.
      // If we left or entered CV during the test, let the bumpless tracker
      // re-derive cv_I naturally on the next tick instead of seeding stale state.
      bool cvContextUnchanged = (g_sysIDResume.voltageControlActive_snap && voltageControlActive);
      if (cvContextUnchanged) {
        cv_I = g_sysIDResume.cv_I_snap;
      }

      // Reseed PID integrator to current applied duty (bumpless transfer).
      if (g_sysIDResume.sysMode == SYS_MODE_AUTO) {
        currentPID.SetMode(AUTOMATIC);
      }
      currentPID.ResetIntegratorTo((double)lastAppliedDuty);
      pidOutput = (double)lastAppliedDuty;

      queueConsoleMessageF(
        "SystemID: restored to %s | duty=%.1f%% setpoint=%.2fA cv_I=%.2fA (cvRestored=%d)",
        (g_sysIDResume.sysMode == SYS_MODE_MANUAL) ? "MANUAL" : "AUTO",
        lastAppliedDuty,
        restoredSetpoint,
        cvContextUnchanged ? g_sysIDResume.cv_I_snap : cv_I,
        cvContextUnchanged);
    }

    g_sysIDResume.valid = false;
  }

  if (!sysIDRunning) {

    // ========== SETPOINT COMPUTATION (AUTO mode only) ==========
    float setpointCommand = 0.0f;

    if (sysMode == SYS_MODE_AUTO) {

      static bool lastTuningMode = false;

      if (TuningMode) {
        // ===== TUNING MODE (square-wave setpoint generator) =====
        static bool tuningWaveHigh = false;
        static uint32_t lastTuningWaveToggle = 0;

        uint32_t halfPeriodMs = ((uint32_t)wavePeriod * 1000) / 2;
        if (tick.nowMs - lastTuningWaveToggle >= halfPeriodMs) {
          tuningWaveHigh = !tuningWaveHigh;
          lastTuningWaveToggle = tick.nowMs;
        }

        uTargetAmps = tuningWaveHigh ? (5 + waveAmplitude) : 5;
        setpointCommand = (float)uTargetAmps;

        setpointLimited = slew_limit_f(setpointLimited, setpointCommand,
                                       SetpointRiseRate, SetpointFallRate, actualDtSec);

        voltageControlActive = false;

        targetCurrent = getFiltI();
        pidInput = (double)targetCurrent;
        pidSetpoint = (double)setpointLimited;
        pidError = setpointLimited - targetCurrent;
        currentPID.Compute();
        // TrackAppliedOutput() is NOT needed here — falls through to the shared
        // call at the end of the normal-mode section.

        lastTuningMode = true;

      } else {
        // ===== NORMAL AUTO =====

        // Detect TuningMode exit — fires exactly once.
        if (lastTuningMode) {
          tempPIDActive = false;
          tempFilterNeedsReseed = true;
        }
        lastTuningMode = false;

        // Outer thermal PID. Library timer governs Compute() cadence.
        tempPID_tick(currentMillis, actualDtSec);

        // Thermal log runs after tempPID_tick() so outerAntiWindupFired and
        // outerTermI reflect this tick's state, not the previous tick's.
        thermalLog_tick(currentMillis);

        // --- Command architecture ---
        //
        //   I_cap        RPM-dependent mechanical/electrical ceiling (table lookup).
        //   thermalPenalty  Outer thermal PID output. Derates I_cap when hot;
        //                   zero-floored in CV stages (enforced in tempPID_tick).
        //   uTargetAmps  I_cap minus thermal penalty, clamped to [0, MaxTableValue],
        //                with user overrides applied. This is the table+thermal limit
        //                and the upper bound passed to the CV controller.
        //   Icv          CV position-form PI output — the direct current setpoint in
        //                absorption, float, and TargetVoltageMode. Clamped to
        //                [0, uTargetAmps]. Never written back to thermalPenaltyAmps
        //                or the thermal integrator.
        //
        // Execution order:
        //   1. Subtract thermal penalty from I_cap; clamp to [0, MaxTableValue].
        //   2. Apply user overrides (HiLow, MaintainMode).
        //   In CV modes, far from target (inCCPhase): setpointCommand = uTargetAmps directly;
        //      bumpless tracker keeps cv_I warm for handoff.
        //   In CV modes, within CV_ENGAGE_MARGIN of target: position-form PI produces Icv;
        //      setpointCommand = Icv.
        //   In bulk/idle/MaintainMode: setpointCommand = uTargetAmps directly.

        float I_cap;
        if (capLimitMode == 1 && tick.currentBatteryVoltage > 0.5f) {
          I_cap = interpolateRPMTable(RPM, rpmCapPowerTable) / tick.currentBatteryVoltage;
        } else {
          I_cap = getCapCurrentForRPM(RPM);
        }

        float I_cmd = I_cap - thermalPenaltyAmps;
        I_cmd = fminf(I_cmd, (float)MaxTableValue);
        I_cmd = fmaxf(I_cmd, 0.0f);

        uTargetAmps = I_cmd;

        // User overrides
        if (HiLow == 0) uTargetAmps = uTargetAmps / 2;
        if (MaintainMode == 1) uTargetAmps = 0;

        // ── iExcess supervisor ─────────────────────────────────────────
        {
          const float IEXCESS_K = 5.0f;
          const float IEXCESS_GATE = 0.10f;
          const float IEXCESS_HYST = 2.0f;
          const float K_IE = 1.0f;

          static bool iExcessActive = false;

          if (voltageControlActive && (BatteryV > ChargingVoltageTarget - IEXCESS_GATE)) {
            float excess = g_iMA2 - setpointLimited - IEXCESS_K;  // setpointLimited = previous tick — acceptable
            bool aboveThreshold = (excess > 0.0f);
            bool belowHysteresis = (g_iMA2 < setpointLimited + IEXCESS_K - IEXCESS_HYST);

            if (aboveThreshold) {
              float ieCap = fmaxf(0.0f, fastOvBaseCap - K_IE * excess);
              fastOvCurrentCap = fminf(fastOvCurrentCap, ieCap);
              fastOvClampActive = true;
              iExcessActive = true;
            } else if (iExcessActive && !belowHysteresis) {
              fastOvClampActive = true;  // hold govBypass during hysteresis
            } else {
              iExcessActive = false;
            }
          } else {
            iExcessActive = false;
          }
          g_iExcessActive = iExcessActive;
          g_iExcessDutyCap = 100.0f;  // retired
        }

        // ── Apply fastOvCurrentCap to uTargetAmps (fastOV + iExcess combined) ──
        uTargetAmps = fminf((float)uTargetAmps, fastOvCurrentCap);
        // This line was missing — fastOvCurrentCap was computed but never applied.

        // TargetVoltageMode: run CV at a user-specified voltage target.
        // Forces float-equivalent stage flags so voltageControlActive goes true
        // below, then overrides ChargingVoltageTarget with the user value.
        // All current limits (RPM cap, thermal penalty, MaxTableValue, user
        // overrides) remain fully active — this only changes the voltage target.
        // cv_I is seeded by bumpless transfer tracking on CV entry — no explicit reseed needed.
        if (TargetVoltageMode == 1) {
          inBulkStage = false;
          inAbsorptionStage = false;
          ChargingVoltageTarget = TargetVoltageSetpoint;
          if (enteringTargetVoltageMode) {
            pidLog_enteringTargetVoltageMode = 1;
            queueConsoleMessageF("TargetVoltageMode: active, target=%.2fV", TargetVoltageSetpoint);
          }
        }

        // voltageControlActive: true in absorption, float, and TargetVoltageMode.
        // False in idle and MaintainMode. CC phase (inCCPhase) handles bulk approach.
        voltageControlActive = !inIdleStage;
        if (MaintainMode == 1) {
          voltageControlActive = false;
          if (enteringMaintainMode) {
            queueConsoleMessage("MaintainMode: active, targeting 0 net battery amps");
          }
        }
        // Detect CV entry so the voltage loop fires immediately on the first CV tick.
        static bool lastVoltageControlActive = false;
        bool enteringCV = (!lastVoltageControlActive && voltageControlActive);
        lastVoltageControlActive = voltageControlActive;

        // Voltage target rise governor.
        // Now only active in the final CV_ENGAGE_MARGIN window before target (vGap <= 0.15V).
        // During bulk approach, CC phase commands uTargetAmps directly and this governor
        // is a mathematical no-op (e_needed >> vGap, so voltageTargetSlewed saturates to
        // Falls are always instantaneous.
        static float voltageTargetSlewed = 0.0f;
        if (enteringCV) {
          voltageTargetSlewed = ChargingVoltageTarget;
        }
        if (voltageControlActive) {
          if (ChargingVoltageTarget > voltageTargetSlewed + 0.01f) {
            float icvHi_gov = clamp_f((float)uTargetAmps, 0.0f, (float)MaxTableValue);
            float e_needed = (icvHi_gov - cv_I) / VoltageKp;
            e_needed = fmaxf(e_needed, 0.02f);
            voltageTargetSlewed = fminf(ChargingVoltageTarget,
                                        getFiltV() + e_needed);  // filtered — control path
          } else {
            voltageTargetSlewed = ChargingVoltageTarget;
          }
        }

        // CC/CV phase determination — must be after voltageTargetSlewed is updated.
        const float CV_ENGAGE_MARGIN = 0.15f;
        float vGap = voltageTargetSlewed - getFiltV();  // filtered — control path
        bool inCCPhase = voltageControlActive && (vGap > CV_ENGAGE_MARGIN);

        // Bumpless transfer: track cv_I toward the operating-point value when CV is
        // inactive OR when in CC phase (far from target, vGap > CV_ENGAGE_MARGIN).
        // By the time vGap drops below the margin and the PI takes over, cv_I already
        // reflects what the alternator is producing at that voltage — no wind-up on entry.
        // While CV is active and within margin, cv_I_track stays in sync for seamless re-entry.
        {
          static float cv_I_track = 0.0f;
          float icvHi_bt = clamp_f((float)uTargetAmps, 0.0f, (float)MaxTableValue);
          if (!voltageControlActive || inCCPhase) {
            float e_bt = ChargingVoltageTarget - getFiltV();  // filtered — control path
            float cv_I_target = clamp_f(getFiltI() - VoltageKp * e_bt, 0.0f, icvHi_bt);
            const float Kt = 2.0f;
            cv_I_track += Kt * (cv_I_target - cv_I_track) * actualDtSec;
            cv_I_track = clamp_f(cv_I_track, 0.0f, icvHi_bt);
            cv_I = cv_I_track;
          } else {
            cv_I_track = cv_I;
          }
        }

        if (voltageControlActive) {
          if (inCCPhase) lastVoltageLoopMs = currentMillis;  // keep timestamp fresh so first CV tick gets correct dtSec
          bool cvLoopFired = !inCCPhase && (enteringCV || ((currentMillis - lastVoltageLoopMs) >= VoltageLoopInterval));

          if (cvLoopFired) {
            uint32_t prevVoltageLoopMs = lastVoltageLoopMs;
            lastVoltageLoopMs = currentMillis;
            pidLog_voltageLoopRanThisTick = 1;
            pidLog_enteringCV = enteringCV ? 1 : 0;

            float e = voltageTargetSlewed - getFiltV();  // filtered — control path
            float dtSec = (prevVoltageLoopMs == 0)
                            ? ((float)VoltageLoopInterval / 1000.0f)
                            : ((float)(currentMillis - prevVoltageLoopMs) / 1000.0f);
            dtSec = constrain(dtSec, 0.001f, 0.5f);

            float icvHi = clamp_f((float)uTargetAmps, 0.0f, (float)MaxTableValue);
            float icvLo = 0.0f;

            if (enteringCV) {
              const char *stageName = inAbsorptionStage ? "ABSORPTION"
                                                        : (TargetVoltageMode ? "TARGET_V" : "FLOAT");
              Serial.printf(
                "CVLoop ENTER %s | target=%.2fV slewed=%.2fV battV=%.2fV e=%.3fV "
                "cv_I(tracked)=%.2fA limit=%.1fA\n",
                stageName,
                ChargingVoltageTarget, voltageTargetSlewed,
                tick.currentBatteryVoltage, e, cv_I, icvHi);
            } else {
              float p = VoltageKp * e;
              float unsat = p + cv_I;
              Icv = clamp_f(unsat, icvLo, icvHi);

              bool satHi = (Icv >= icvHi);
              bool satLo = (Icv <= icvLo);
              float KiDown = 7.0f * VoltageKi;
              float dI = (e >= 0.0f ? VoltageKi : KiDown) * e * dtSec;

              bool supervisorLimiting = fastOvClampActive && ((float)uTargetAmps < uTargetRaw_cached - 0.01f);
              if (supervisorLimiting && dI > 0.0f) {
                // fast OV supervisor is actively capping ceiling; freeze upward integration
              } else if (!(satHi && dI > 0.0f) && !(satLo && dI < 0.0f)) {
                cv_I += dI;
              }

              Icv = clamp_f(VoltageKp * e + cv_I, icvLo, icvHi);

              const char *stageName = inAbsorptionStage ? "ABSORPTION"
                                                        : (TargetVoltageMode ? "TARGET_V" : "FLOAT");
            }
          }

          pidLog_uTargetBeforeVoltCap = uTargetRaw;
          pidLog_uTargetAfterVoltCap = Icv;

        } else {
          pidLog_uTargetBeforeVoltCap = uTargetRaw;
          pidLog_uTargetAfterVoltCap = (float)uTargetAmps;
        }

        // Per-tick Icv recompute — proportional path responds every inner-loop tick;
        // cv_I still updates only on VoltageLoopInterval cadence.
        {
          float e_now = voltageTargetSlewed - getFiltV();  // filtered — control path
          float icvHi_tick = clamp_f((float)uTargetAmps, 0.0f, (float)MaxTableValue);
          if (!enteringCV) {
            Icv = clamp_f(VoltageKp * e_now + cv_I, 0.0f, icvHi_tick);
          }
        }

        if (inCCPhase) {
          // Far from target: command ceiling directly; bumpless tracker keeps cv_I warm.
          setpointCommand = (float)uTargetAmps;
        } else {
          setpointCommand = voltageControlActive ? Icv : (float)uTargetAmps;
        }

        float effectiveFallRate = fastOvClampActive ? 1.0e9f : SetpointFallRate;
        setpointLimited = slew_limit_f(setpointLimited, setpointCommand,
                                       SetpointRiseRate, effectiveFallRate, actualDtSec);

        // Inner current PID compute.
        // MaintainMode uses getBatteryCurrent() (net battery amps — no filtered
        // equivalent yet). Normal AUTO uses filtered current for control.
        targetCurrent = (MaintainMode == 1) ? getBatteryCurrent() : getFiltI();
        pidInput = (double)targetCurrent;
        pidSetpoint = (double)setpointLimited;
        pidError = setpointLimited - targetCurrent;
        currentPID.Compute();
      }

    } else {
      // ===== MANUAL mode: no setpoint management =====
      voltageControlActive = false;
      uTargetAmps = 0;
      setpointLimited = 0.0f;
      pidInput = (double)getFiltI();
    }  // end of MANUAL mode else
  }    // end if (!sysIDRunning)


  // ========== BUILD DUTY REQUEST ==========
  float dutyRequest;
  if (sysMode == SYS_MODE_MANUAL) {
    dutyRequest = constrain((float)ManualDutyTarget, 0.0f, 100.0f);
  } else {
    dutyRequest = (float)pidOutput;
  }

  pidLog_dutyRequest = dutyRequest;

  // ========== APPLY THROUGH GOVERNOR ==========
  dutyNewFloat = governor_apply(lastAppliedDuty, dutyRequest, govMode,
                                sysIDRunning ? 0.0f : tick.rpmMinDuty,
                                true, actualDtSec);
  pidLog_dutyApplied = dutyNewFloat;

  // ========== TELL PID WHAT ACTUALLY HAPPENED ==========
  if (sysMode == SYS_MODE_AUTO) {
    currentPID.TrackAppliedOutput((double)dutyNewFloat, actualDtSec);
  } else {
    currentPID.ResetIntegratorTo((double)dutyNewFloat);
  }

  // ========== UPDATE STATE ==========
  lastAppliedDuty = dutyNewFloat;
  dutyCycle = dutyNewFloat;

  if (sysMode == SYS_MODE_AUTO) {
    innerTermP = (float)currentPID.GetPterm();
    innerTermI = (float)currentPID.GetIterm();
    innerTermD = (float)currentPID.GetDterm();
  } else {
    innerTermP = innerTermI = innerTermD = 0.0f;
  }

  updateFieldTelemetry(dutyCycle, tick.currentBatteryVoltage, FieldResistance);

  fieldActiveStatus = (!gpio4IsLow && lastAppliedDuty > 0.01f)
                        ? ((sysMode == SYS_MODE_MANUAL) ? 3 : 1)
                        : 0;
  chargeStageDisplay = getChargeStageDisplayCode();

  // All outer loop, inner PID, and duty pipeline state is now final for this tick.
  // Called only in the normal-mode path — shutdown/fault paths do not log here.
  pidLog_tick(currentMillis);
  prevMode = mode;
}

void setDutyPercent(float percent) {
  static uint32_t lastFrequency = 0;
  static bool pwmInitialized = false;

  percent = constrain(percent, 0.0f, 100.0f);

  // Calculate duty with full resolution
  uint32_t duty = (uint32_t)((((1UL << pwmResolution) - 1) * percent) / 100.0f);

  if (!pwmInitialized) {
    Serial.printf("Initializing PWM on pin %d...\n", (int)pwmPin);
    pwmInitialized = ledcAttach(pwmPin, (uint32_t)SwitchingFrequency, pwmResolution);
    if (!pwmInitialized) {
      queueConsoleMessage("ERROR: PWM attach failed");
      return;
    }
    lastFrequency = (uint32_t)SwitchingFrequency;
    Serial.println("PWM initialized successfully");
  }

  if ((uint32_t)SwitchingFrequency != lastFrequency) {
    uint32_t actualFreq = ledcChangeFrequency(pwmPin, (uint32_t)SwitchingFrequency, pwmResolution);
    lastFrequency = (uint32_t)SwitchingFrequency;
    delayMicroseconds(100);
    queueConsoleMessageF("PWM frequency applied to hardware: %luHz", (unsigned long)actualFreq);
  }

  ledcWrite(pwmPin, duty);
}


void updateChargingStage() {
  const uint32_t now = millis();
  const float v = getBatteryVoltage();
  const int soc = SOC_percent;

  const float BULK_V_BAND = 0.05f;

  if (inBulkStage && !inAbsorptionStage) {
    // ===== BULK (CC) =====
    ChargingVoltageTarget = BulkVoltage;

    const bool v_at_bulk = (v >= (BulkVoltage - BULK_V_BAND));
    const bool v_overshot = (v >= (BulkVoltage + 0.1f));

    if (v_overshot) {
      inAbsorptionStage = true;
      ChargingVoltageTarget = AbsorptionVoltage;
      absorptionStartTime = now;
      absorptionTailTimer = 0;
      bulkVoltageHoldTimer = 0;
      queueConsoleMessageF(
        "Stage: BULK→ABSORPTION (overshoot) | battV=%.2fV bulkTarget=%.2fV absTarget=%.2fV tailCurrent=%.1fA timeout=%.0fmin",
        v, BulkVoltage, AbsorptionVoltage, TailCurrent_A,
        (float)AbsorptionTimeoutMs / 60000.0f);
      Serial.printf(
        "Stage: BULK→ABSORPTION (overshoot) | battV=%.2fV bulkTarget=%.2fV absTarget=%.2fV tailCurrent=%.1fA timeout=%.0fmin\n",
        v, BulkVoltage, AbsorptionVoltage, TailCurrent_A,
        (float)AbsorptionTimeoutMs / 60000.0f);

    } else if (v_at_bulk) {
      if (bulkVoltageHoldTimer == 0) {
        bulkVoltageHoldTimer = now;
      } else if ((uint32_t)(now - bulkVoltageHoldTimer) >= bulkVoltageHoldMs) {
        inAbsorptionStage = true;
        ChargingVoltageTarget = AbsorptionVoltage;
        absorptionStartTime = now;
        absorptionTailTimer = 0;
        bulkVoltageHoldTimer = 0;
        queueConsoleMessageF(
          "Stage: BULK→ABSORPTION (hold timer) | battV=%.2fV bulkTarget=%.2fV absTarget=%.2fV tailCurrent=%.1fA timeout=%.0fmin",
          v, BulkVoltage, AbsorptionVoltage, TailCurrent_A,
          (float)AbsorptionTimeoutMs / 60000.0f);
        Serial.printf(
          "Stage: BULK→ABSORPTION (hold timer) | battV=%.2fV bulkTarget=%.2fV absTarget=%.2fV tailCurrent=%.1fA timeout=%.0fmin\n",
          v, BulkVoltage, AbsorptionVoltage, TailCurrent_A,
          (float)AbsorptionTimeoutMs / 60000.0f);
      }
    } else {
      bulkVoltageHoldTimer = 0;
    }

  } else if (inBulkStage && inAbsorptionStage) {
    // ===== ABSORPTION (CV) =====
    ChargingVoltageTarget = AbsorptionVoltage;

    const bool thermallyConstrained = (thermalPenaltyAmps > 2.0f) && (uTargetAmps <= TailCurrent_A * 2.0f);  //if you ever want CV-awareness here you'd change it to Icv <= TailCurrent_A * 2.0f.
    const bool tailReached = !thermallyConstrained && (Bcur <= TailCurrent_A);
    const bool timedOut = ((uint32_t)(now - absorptionStartTime) >= AbsorptionTimeoutMs);

    static bool lastThermallyConstrained = false;
    if (thermallyConstrained && !lastThermallyConstrained) {
      queueConsoleMessageF(
        "Absorption: tail detection suppressed (thermal) | penalty=%.1fA uTarget=%.1fA tailThresh=%.1fA",
        thermalPenaltyAmps, uTargetAmps, TailCurrent_A);
      Serial.printf(
        "Absorption: tail detection suppressed (thermal) | penalty=%.1fA uTarget=%.1fA tailThresh=%.1fA\n",
        thermalPenaltyAmps, uTargetAmps, TailCurrent_A);
    }
    if (!thermallyConstrained && lastThermallyConstrained) {
      queueConsoleMessageF(
        "Absorption: tail detection resumed | penalty=%.1fA uTarget=%.1fA tailThresh=%.1fA",
        thermalPenaltyAmps, uTargetAmps, TailCurrent_A);
      Serial.printf(
        "Absorption: tail detection resumed | penalty=%.1fA uTarget=%.1fA tailThresh=%.1fA\n",
        thermalPenaltyAmps, uTargetAmps, TailCurrent_A);
    }
    lastThermallyConstrained = thermallyConstrained;

    if (tailReached) {
      if (absorptionTailTimer == 0) {
        absorptionTailTimer = now;
      } else if ((uint32_t)(now - absorptionTailTimer) >= absorptionCompleteTime) {
        inBulkStage = false;
        inAbsorptionStage = false;
        inIdleStage = (UseFloat == 0);
        floatStartTime = now;
        absorptionTailTimer = 0;
        rebulkTimer = 0;
        const char *nextStage = (UseFloat == 0) ? "IDLE" : "FLOAT";
        queueConsoleMessageF(
          "Stage: ABSORPTION→%s (tail current) | battV=%.2fV Bcur=%.1fA tailThresh=%.1fA",
          nextStage, v, Bcur, TailCurrent_A);
        Serial.printf(
          "Stage: ABSORPTION→%s (tail current) | battV=%.2fV Bcur=%.1fA tailThresh=%.1fA\n",
          nextStage, v, Bcur, TailCurrent_A);
      }
    } else {
      absorptionTailTimer = 0;
    }

    if (timedOut && inAbsorptionStage) {
      inBulkStage = false;
      inAbsorptionStage = false;
      inIdleStage = (UseFloat == 0);
      floatStartTime = now;
      absorptionTailTimer = 0;
      rebulkTimer = 0;
      const char *nextStage = (UseFloat == 0) ? "IDLE" : "FLOAT";
      queueConsoleMessageF(
        "Stage: ABSORPTION→%s (timeout %.0f min) | battV=%.2fV Bcur=%.1fA",
        nextStage, (float)AbsorptionTimeoutMs / 60000.0f, v, Bcur);
      Serial.printf(
        "Stage: ABSORPTION→%s (timeout %.0f min) | battV=%.2fV Bcur=%.1fA\n",
        nextStage, (float)AbsorptionTimeoutMs / 60000.0f, v, Bcur);
    }

  } else if (inIdleStage) {
    // ===== IDLE (UseFloat=0, post-absorption rest) =====
    const uint32_t tIdle = (uint32_t)(now - floatStartTime);

    static uint32_t lastIdleDebugMs = 0;
    if ((uint32_t)(now - lastIdleDebugMs) >= 30000) {
      lastIdleDebugMs = now;
      Serial.printf(
        "Idle status | battV=%.2fV Bcur=%.1fA tIdle=%lus rebulkV=%.2fV rebulkI=%.1fA\n",
        v, Bcur, (unsigned long)(tIdle / 1000), RebulkVoltage, RebulkCurrent_A);
    }

    const bool rebulkCondition = (v < RebulkVoltage) || (RebulkCurrent_A > 0.0f && Bcur < -RebulkCurrent_A);

    bool allowRebulk = true;
    if (socInfoAvailable) {
      if (soc >= SOC_BlockRebulk_percent) allowRebulk = false;
      if (soc <= SOC_AllowRebulk_percent) allowRebulk = true;
    }

    if (tIdle >= MinFloatTime) {
      if (allowRebulk && rebulkCondition) {
        if (rebulkTimer == 0) rebulkTimer = now;
        else if ((uint32_t)(now - rebulkTimer) >= rebulkDebounceTime) {
          inBulkStage = true;
          inAbsorptionStage = false;
          inIdleStage = false;
          bulkVoltageHoldTimer = 0;
          absorptionTailTimer = 0;
          rebulkTimer = 0;
          floatStartTime = now;
          const char *why = (Bcur < -RebulkCurrent_A) ? "discharge current threshold"
                                                      : "voltage sag confirmed";
          queueConsoleMessageF(
            "Stage: IDLE→BULK (%s) | battV=%.2fV Bcur=%.1fA tIdle=%lus",
            why, v, Bcur, (unsigned long)(tIdle / 1000));
          Serial.printf(
            "Stage: IDLE→BULK (%s) | battV=%.2fV Bcur=%.1fA tIdle=%lus\n",
            why, v, Bcur, (unsigned long)(tIdle / 1000));
        }
      } else {
        rebulkTimer = 0;
      }
    } else {
      rebulkTimer = 0;
    }

  } else {
    // ===== FLOAT =====
    ChargingVoltageTarget = FloatVoltage;

    const uint32_t tFloat = (uint32_t)(now - floatStartTime);
    const bool floatTimedOut = (tFloat >= (uint32_t)(FLOAT_DURATION * 1000UL));

    static uint32_t lastFloatDebugMs = 0;
    if ((uint32_t)(now - lastFloatDebugMs) >= 30000) {
      lastFloatDebugMs = now;
      float vErr = FloatVoltage - v;
      Serial.printf(
        "Float status | battV=%.2fV floatTarget=%.2fV vErr=%.3fV Bcur=%.1fA tFloat=%lus rebulkV=%.2fV minFloatTime=%lus\n",
        v, FloatVoltage, vErr, Bcur,
        (unsigned long)(tFloat / 1000),
        RebulkVoltage,
        (unsigned long)(MinFloatTime / 1000));
    }

    const bool rebulkCondition = (v < RebulkVoltage) || (RebulkCurrent_A > 0.0f && Bcur < -RebulkCurrent_A);

    bool sagConfirmed = false;

    bool allowRebulk = true;
    if (socInfoAvailable) {
      if (soc >= SOC_BlockRebulk_percent) allowRebulk = false;
      if (soc <= SOC_AllowRebulk_percent) allowRebulk = true;
    }

    if (tFloat >= MinFloatTime) {
      if (allowRebulk && rebulkCondition) {  // add allowRebulk here
        if (rebulkTimer == 0) rebulkTimer = now;
        else if ((uint32_t)(now - rebulkTimer) >= rebulkDebounceTime) {
          sagConfirmed = true;
        }
      } else {
        rebulkTimer = 0;  // also resets timer if SoC starts blocking mid-debounce- design decision
      }
    }

    if (allowRebulk && (sagConfirmed || floatTimedOut)) {
      inBulkStage = true;
      inAbsorptionStage = false;
      inIdleStage = false;
      bulkVoltageHoldTimer = 0;
      absorptionTailTimer = 0;
      rebulkTimer = 0;
      floatStartTime = now;
      const char *why = floatTimedOut               ? "float duration expired"
                        : (Bcur < -RebulkCurrent_A) ? "discharge current threshold"
                                                    : "voltage sag confirmed";
      queueConsoleMessageF(
        "Stage: FLOAT→BULK (%s) | battV=%.2fV rebulkV=%.2fV tFloat=%lus",
        why, v, RebulkVoltage, (unsigned long)(tFloat / 1000));
      Serial.printf(
        "Stage: FLOAT→BULK (%s) | battV=%.2fV rebulkV=%.2fV tFloat=%lus\n",
        why, v, RebulkVoltage, (unsigned long)(tFloat / 1000));
    }
  }
}

uint8_t getChargeStageDisplayCode() {
  // Manual mode gets its own display regardless of bulk/absorption flags
  if (fieldActiveStatus == 3) {
    return CHARGE_STAGE_MANUAL;
  }

  // Only auto-active field should show any stage
  if (fieldActiveStatus != 1) {
    return CHARGE_STAGE_NONE;
  }

  // Override modes take priority over the normal stage display.
  // TargetVoltageMode checked first: if both flags were somehow set simultaneously,
  // TARGET_V is the more specific and intentional state to show.
  if (TargetVoltageMode == 1) {
    return CHARGE_STAGE_TARGET_V;
  }
  if (MaintainMode == 1) {
    return CHARGE_STAGE_MAINTAIN;
  }
  if (inIdleStage) {
    return CHARGE_STAGE_IDLE;  // add CHARGE_STAGE_IDLE to your enum
  }

  // Auto-active charging stages
  if (inBulkStage && !inAbsorptionStage) {
    return CHARGE_STAGE_BULK;
  }

  if (inBulkStage && inAbsorptionStage) {
    return CHARGE_STAGE_ABSORPTION;
  }

  return CHARGE_STAGE_FLOAT;
}


bool isVoltageDisagreementWarning(uint32_t nowMs, float batteryV, float ibv,
                                  bool voltagePlausible, bool voltageDisagreementCritical) {
  static bool voltageDisagreementActive = false;  // FIXED: Track if timer is running

  if (!voltagePlausible || voltageDisagreementCritical) {
    voltageDisagreementStart = 0;
    voltageDisagreementActive = false;
    return false;
  }

  if (fabsf(batteryV - ibv) > VoltageDisagreeThreshold) {
    if (!voltageDisagreementActive) {
      voltageDisagreementStart = nowMs;
      voltageDisagreementActive = true;
    } else if (nowMs - voltageDisagreementStart > VoltageDisagreeTimeout) {
      return true;
    }
  } else {
    voltageDisagreementStart = 0;
    voltageDisagreementActive = false;
  }

  return false;
}
/**
 * isVoltageDisagreementCritical()
 * 
 * Automatically scales critical disagreement threshold based on system voltage:
 * - 12V system: >1.0V difference is critical
 * - 24V system: >2.0V difference is critical
 * - 48V system: >4.0V difference is critical
 * 
 * A critical disagreement means one sensor has completely failed and we
 * cannot trust voltage readings for field control decisions.
 * 
 * @return true if sensors disagree by critical amount or either is invalid
 */
bool isVoltageDisagreementCritical() {
  float criticalThreshold;
  if (BulkVoltage < 18.0f) {
    criticalThreshold = 1.0f;
  } else if (BulkVoltage < 36.0f) {
    criticalThreshold = 2.0f;
  } else {
    criticalThreshold = 4.0f;
  }

  if (isnan(BatteryV) || isnan(IBV)) return true;
  if (BatteryV < 0.1f || IBV < 0.1f) return true;

  float difference = fabsf(BatteryV - IBV);

  static uint32_t critDisagreeStartMs = 0;
  static bool critDisagreeActive = false;

  if (difference > criticalThreshold) {
    if (!critDisagreeActive) {
      critDisagreeStartMs = millis();
      critDisagreeActive = true;
    } else if ((uint32_t)(millis() - critDisagreeStartMs) > VoltageDisagreeCriticalTimeoutMs) {
      return true;
    }
  } else {
    critDisagreeStartMs = 0;
    critDisagreeActive = false;
  }

  return false;
}
/**
 * isTempSustainedWarning()
 * Returns true if temp has exceeded warning threshold for TempSustainedTimeout
 * Resets if temp drops below threshold (must be continuously above)
 */
bool isTempSustainedWarning(uint32_t nowMs, float tempToUseF, float tempLimitF,
                            float tempWarnExcessF, bool ignoreTemperature) {
  if (ignoreTemperature) {
    tempWarningStartMs = 0;
    return false;
  }

  if (tempToUseF > (tempLimitF + tempWarnExcessF)) {
    if (tempWarningStartMs == 0) {
      tempWarningStartMs = nowMs;
    } else if (nowMs - tempWarningStartMs > TempSustainedTimeout) {
      return true;
    }
  } else {
    tempWarningStartMs = 0;
  }

  return false;
}

/**
 * modeToString()
 */
const char *modeToString(FieldControlMode mode) {
  switch (mode) {
    case MODE_CRITICAL_RAMP: return "CRITICAL_RAMP";
    case MODE_WARNING_RAMP_AND_LOCKOUT: return "WARNING_RAMP_LOCKOUT";
    case MODE_LOCKOUT_RAMP: return "LOCKOUT_RAMP";
    case MODE_DISABLED_RAMP: return "DISABLED_RAMP";
    case MODE_NORMAL_MANUAL: return "NORMAL_MANUAL";
    case MODE_NORMAL_AUTO_PID: return "NORMAL_AUTO_PID";
    default: return "UNKNOWN";
  }
}

/**
 * reasonToString()
 */
const char *reasonToString(FieldEventReason r) {
  switch (r) {
    case REASON_NONE: return "NONE";
    case REASON_AUTOZERO_ACTIVE: return "AUTOZERO";
    case REASON_TEMP_STALE: return "TEMP_STALE";
    case REASON_TEMP_CRITICAL: return "TEMP_CRITICAL";
    case REASON_TEMP_WARNING: return "TEMP_WARNING";
    case REASON_TEMP_SUSTAINED: return "TEMP_SUSTAINED";
    case REASON_VOLTAGE_IMPLAUSIBLE: return "VOLT_IMPLAUSIBLE";
    case REASON_VOLTAGE_DISAGREE_CRITICAL: return "VOLT_DISAGREE_CRIT";
    case REASON_VOLTAGE_SPIKE: return "VOLT_SPIKE";
    case REASON_VOLTAGE_DISAGREE_WARNING: return "VOLT_DISAGREE_WARN";
    case REASON_LOCKOUT_ACTIVE: return "LOCKOUT";
    case REASON_CHARGING_DISABLED: return "DISABLED";
    case REASON_MANUAL_MODE: return "MANUAL";
    case REASON_INA_OVERVOLTAGE: return "INA228 hardware overvoltage";
    case REASON_HARD_OVERCURRENT: return "HARD_OVERCURRENT";

    default: return "UNKNOWN";
  }
}

/**
 * selectFieldControlMode()
 * PURE function - determines mode from tick snapshot only
 * Priority: Disabled (user off switch) > Manual (unrestricted) > Critical > Warning > AutoZero > Lockout > Auto
 */
FieldControlMode selectFieldControlMode(const TickSnapshot &tick) {

  // PRIORITY 1: DISABLED (user On/Off switch - always respected)
  if (!tick.chargingEnabled) {
    return MODE_DISABLED_RAMP;
  }

  // PRIORITY 2: MANUAL MODE (UNRESTRICTED - bypasses all safeties when user wants manual control)
  if (tick.manualMode) {
    return MODE_NORMAL_MANUAL;
  }

  // PRIORITY 3: CRITICAL CONDITIONS (auto mode only)
  if (tick.tempDataVeryStale && !tick.ignoreTemperature) {
    return MODE_CRITICAL_RAMP;
  }
  if (!tick.voltagePlausible || tick.voltageDisagreementCritical) {
    return MODE_CRITICAL_RAMP;
  }
  if (!tick.ignoreTemperature && tick.tempToUseF > (tick.tempLimitF + tick.tempCritExcessF)) {
    return MODE_CRITICAL_RAMP;
  }

  // PRIORITY 4: WARNING CONDITIONS (all start lockout)
  if (tick.currentBatteryVoltage > (tick.bulkVoltage + tick.voltageSpikeMargin)) {
    return MODE_WARNING_RAMP_AND_LOCKOUT;
  }
  if (tick.voltageDisagreementWarning) {
    return MODE_WARNING_RAMP_AND_LOCKOUT;
  }
  if (!tick.ignoreTemperature && tick.tempToUseF > (tick.tempLimitF + tick.tempWarnExcessF)) {
    return MODE_WARNING_RAMP_AND_LOCKOUT;
  }

  // PRIORITY 5: AUTO-ZERO
  if (tick.autoZeroActive) {
    return MODE_LOCKOUT_RAMP;
  }

  // PRIORITY 6: LOCKOUT
  if (tick.inLockout) {
    return MODE_LOCKOUT_RAMP;
  }

  // PRIORITY 7: NORMAL AUTO
  return MODE_NORMAL_AUTO_PID;
}

/**
 * selectFieldEventReason()
 * PURE function - MUST match selectFieldControlMode() priority exactly
 */
FieldEventReason selectFieldEventReason(const TickSnapshot &tick) {
  // Priority 1: Hardware overvoltage - overrides everything
  if (tick.inaOvervoltageLatched) return REASON_INA_OVERVOLTAGE;

  if (MeasuredAmps > HardOCTripAmps) {
    if (hardOCStartMs == 0) hardOCStartMs = tick.nowMs;
    if ((tick.nowMs - hardOCStartMs) >= HardOCDebounceMs) {
      return REASON_HARD_OVERCURRENT;
    }
  } else {
    hardOCStartMs = 0;
  }

  // Priority 1a: Disabled (user control - always show this reason if off)
  if (!tick.chargingEnabled) return REASON_CHARGING_DISABLED;

  // Priority 2: Manual mode
  if (tick.manualMode) return REASON_MANUAL_MODE;

  // Priority 3: Critical (auto mode only)
  if (tick.tempDataVeryStale && !tick.ignoreTemperature) return REASON_TEMP_STALE;
  if (!tick.voltagePlausible) return REASON_VOLTAGE_IMPLAUSIBLE;
  if (tick.voltageDisagreementCritical) return REASON_VOLTAGE_DISAGREE_CRITICAL;
  if (!tick.ignoreTemperature && tick.tempToUseF > (tick.tempLimitF + tick.tempCritExcessF)) {
    return REASON_TEMP_CRITICAL;
  }

  // Priority 4: Warning
  if (tick.currentBatteryVoltage > (tick.bulkVoltage + tick.voltageSpikeMargin)) return REASON_VOLTAGE_SPIKE;
  if (tick.voltageDisagreementWarning) return REASON_VOLTAGE_DISAGREE_WARNING;
  if (!tick.ignoreTemperature && tick.tempToUseF > (tick.tempLimitF + tick.tempWarnExcessF)) {
    if (tempWarningStartMs > 0 && (tick.nowMs - tempWarningStartMs > TempSustainedTimeout)) {
      return REASON_TEMP_SUSTAINED;
    }
    return REASON_TEMP_WARNING;
  }

  // Priority 5: Auto-zero
  if (tick.autoZeroActive) return REASON_AUTOZERO_ACTIVE;

  // Priority 6: Lockout
  if (tick.inLockout) return REASON_LOCKOUT_ACTIVE;

  return REASON_NONE;
}

void reportFieldModeEvent(uint32_t nowMs, FieldControlMode mode, FieldEventReason reason,
                          const TickSnapshot &tick, bool gpio4Low, float appliedDuty) {
  // Only report when mode or reason actually changes
  if (mode == lastReportedMode && reason == lastReportedReason) {
    return;  // Same state - skip
  }

  lastReportedMode = mode;
  lastReportedReason = reason;
  lastReportMs = nowMs;

  char msg[300];

  // Calculate lockout time remaining
  char lockoutStatus[32] = "none";
  if (tick.inLockout) {
    float remaining = (FIELD_COLLAPSE_DELAY - (tick.nowMs - fieldCollapseTime)) / 1000.0f;
    snprintf(lockoutStatus, sizeof(lockoutStatus), "%.1fs remaining", remaining);
  }

  const char *fieldStatus = gpio4Low ? "OFF" : "ON";

  // Build message based on reason type
  if (reason == REASON_TEMP_STALE || reason == REASON_TEMP_CRITICAL || reason == REASON_TEMP_WARNING || reason == REASON_TEMP_SUSTAINED) {
    // Temperature-related events
    snprintf(msg, sizeof(msg),
             "FIELD: %s | %s | Temp=%.1f°F (Limit=%.1f°F) | PWM=%.1f%% | Field=%s | Lockout=%s",
             modeToString(mode),
             reasonToString(reason),
             tick.tempToUseF,
             tick.tempLimitF,
             appliedDuty,
             fieldStatus,
             lockoutStatus);
  } else {
    // Voltage-related or other events
    float vDelta = fabsf(tick.batteryV - tick.ibv);
    snprintf(msg, sizeof(msg),
             "FIELD: %s | %s | Vbat=%.2fV IBV=%.2fV (Δ=%.3fV) | PWM=%.1f%% | Field=%s | Lockout=%s",
             modeToString(mode),
             reasonToString(reason),
             tick.currentBatteryVoltage,
             tick.ibv,
             vDelta,
             appliedDuty,
             fieldStatus,
             lockoutStatus);
  }

  Serial.println(msg);
  queueConsoleMessage(msg);
}

/**
 * updateFieldTelemetry()
 * Centralized telemetry update
 * Note: When GPIO4 is LOW, PWM is electrically disconnected, but we still
 * report duty-based values for logging consistency
 */
void updateFieldTelemetry(float duty, float voltage, float fieldResistance) {
  vvout = duty / 100.0f * voltage;
  iiout = (fieldResistance > 0.001f) ? (vvout / fieldResistance) : 0.0f;
  MARK_FRESH(IDX_DUTY_CYCLE);
  MARK_FRESH(IDX_FIELD_VOLTS);
  MARK_FRESH(IDX_FIELD_AMPS);
}


/**
 * shouldImmediatelyCutGPIO4()
 * Returns true if conditions warrant skipping Phase 1 ramp and cutting immediately
 * 
 * NOTE: When this returns true, the subsequent applyDutyCycle() call is for
 * rate-limiter state tracking and telemetry only. The PWM output is electrically
 * irrelevant since GPIO4 will already be LOW.
 */
bool shouldImmediatelyCutGPIO4(FieldEventReason reason) {
  if (reason == REASON_INA_OVERVOLTAGE) return true;
  if (reason == REASON_HARD_OVERCURRENT) return true;
  if (reason == REASON_TEMP_CRITICAL) {
    return true;
  }
  return false;
}

/**
 * shouldCutGPIO4AfterSettle()
 * Returns true if GPIO4 should go LOW after duty has settled at 0%
 * 
 * IMPORTANT: Call this AFTER governor_apply() with the duty that was
 * computed (even if not written to hardware). This ensures settle detection
 * matches what would be sent to hardware.
 * 
 * Does NOT reset settledAtZeroDutyMs on mode changes if duty is still at 0.
 * This prevents mode churn from delaying the cut indefinitely.
 * 
 * @param reason       Current fault/event reason
 * @param nowMs        Current timestamp (use consistently, not millis())
 * @param appliedDuty  The duty that was applied (float)
 * @return             true if GPIO4 should be cut LOW
 */
bool shouldCutGPIO4AfterSettle(FieldEventReason reason, uint32_t nowMs, float appliedDuty) {
  // Check if duty has reached 0% (use small threshold for float comparison)
  if (appliedDuty > 0.01f) {
    settledAtZeroDutyMs = 0;  // Not settled - reset timer
    return false;
  }

  // Duty is at 0%, start or continue settle timer
  if (settledAtZeroDutyMs == 0) {
    settledAtZeroDutyMs = nowMs;
    return false;
  }

  // Check if settled long enough
  if (nowMs - settledAtZeroDutyMs < SettleTimeBeforeCut) {
    return false;
  }

  // Settled at 0% for SettleTimeBeforeCut - check if cut is warranted
  switch (reason) {
    // Fast-responding faults: cut if fault persists after ramp-down
    case REASON_VOLTAGE_SPIKE:
    case REASON_VOLTAGE_DISAGREE_WARNING:
    case REASON_VOLTAGE_IMPLAUSIBLE:
    case REASON_VOLTAGE_DISAGREE_CRITICAL:
    case REASON_TEMP_STALE:
    case REASON_HARD_OVERCURRENT:
      return true;

    // Temperature sustained: cut after 2-minute timeout
    case REASON_TEMP_SUSTAINED:
      return true;

    // Temperature warning: check if sustained timeout reached
    case REASON_TEMP_WARNING:
      if (tempWarningStartMs > 0 && (nowMs - tempWarningStartMs > TempSustainedTimeout)) {
        return true;
      }
      return false;

      // Intentional shutdown: cut after settle
    case REASON_AUTOZERO_ACTIVE:
    case REASON_LOCKOUT_ACTIVE:
      return true;

      // Normal user shutdown: cut after settle (Phase 4 handles the slow ramp, we're done)
    case REASON_CHARGING_DISABLED:
      return true;

    default:
      return false;
  }
}



/**
 * buildTickSnapshot()
 * Constructs immutable snapshot of system state for pure decision functions
 */
TickSnapshot buildTickSnapshot(uint32_t currentMillis, uint32_t dt_ms) {
  TickSnapshot tick = {};

  tick.nowMs = currentMillis;
  tick.dt_ms = dt_ms;

  tick.inaOvervoltageLatched = inaOvervoltageLatched;

  // Voltage sensors
  tick.batteryV = BatteryV;
  tick.ibv = IBV;
  tick.currentBatteryVoltage = getBatteryVoltage();

  // Current sensor
  tick.batteryCurrentA = Bcur;

  // RPM-dependent minimum field
  tick.rpmMinDuty = getMinimumFieldForRPM(RPM);

  // Control state
  tick.manualMode = (ManualFieldToggle == 1);
  tick.autoZeroActive = (autoZeroStartTime > 0);
  tick.ignoreTemperature = (IgnoreTemperature != 0);

  // Charging enabled (with BMS and weather mode overrides)
  bool chargingEnabledLocal = (Ignition == 1 && OnOff == 1);

  // Weather mode: disable charging when solar forecast is sufficient
  if (weatherModeEnabled == 1 && currentWeatherMode == 1) {
    chargingEnabledLocal = false;
  }

  if (bmsLogic == 1) {
    bool bmsSignalActiveLocal = !digitalRead(36);
    if (bmsLogicLevelOff == 0) {
      chargingEnabledLocal = chargingEnabledLocal && bmsSignalActiveLocal;
    } else {
      chargingEnabledLocal = chargingEnabledLocal && !bmsSignalActiveLocal;
    }
  }
  tick.chargingEnabled = chargingEnabledLocal;

  // Temperature selection
  bool tempFromAlt = (TempSource == 0);
  float tempSelected = tempFromAlt ? AlternatorTemperatureF : temperatureThermistor;

  tick.tempToUseF = tempSelected;
  tick.tempLimitF = TemperatureLimitF;
  tick.tempWarnExcessF = TempWarnExcess;
  tick.tempCritExcessF = TempCritExcess;
  tick.tempSourceIsAlt = tempFromAlt;

  // Mirror to global for legacy displays
  TempToUse = tick.tempToUseF;

  // Temperature staleness - check SELECTED source
  uint32_t tempTimestampIdx = tempFromAlt ? IDX_ALTERNATOR_TEMP : IDX_THERMISTOR_TEMP;
  uint32_t tempTimestamp = dataTimestamps[tempTimestampIdx];
  bool tempDataVeryStale = false;

  if (tempTimestamp == 0) {
    if (tick.nowMs > 60000) {
      tempDataVeryStale = true;
    }
  } else {
    uint32_t tempAge = tick.nowMs - tempTimestamp;
    tempDataVeryStale = (tempAge > 30000);
  }

  if (isnan(tempSelected) || tempSelected < -50.0f || tempSelected > 400.0f) {
    tempDataVeryStale = true;
  }

  tick.tempDataVeryStale = tempDataVeryStale;

// Voltage validation
  tick.voltagePlausible = isVoltageSensorPlausible();
  tick.voltageDisagreementCritical = isVoltageDisagreementCritical();
  tick.voltageDisagreementWarning = isVoltageDisagreementWarning(
    tick.nowMs, tick.batteryV, tick.ibv,
    tick.voltagePlausible, tick.voltageDisagreementCritical);

  // ── Suppress disagreement check during and after INA OV event ───────────
  // When the ALERT pin cuts the field, ADS1115 and INA228 will diverge
  // naturally as the field collapses — ADS responds faster. Without
  // suppression, VOLT_SPIKE fires on top of an already-handled hardware cut.
  // Suppression holds for INA_OV_DISAGREE_SUPPRESS_MS after the latch clears
  // to allow both sensors to resettle. 10s is deliberate — the INA228
  // averaged value can lag for several update cycles during transients.
  bool inaOvSuppressActive = inaOvervoltageLatched ||
    (inaOvervoltageClearedMs > 0 &&
     (tick.nowMs - inaOvervoltageClearedMs) < INA_OV_DISAGREE_SUPPRESS_MS);

  if (inaOvSuppressActive) {
    tick.voltageDisagreementWarning  = false;
    tick.voltageDisagreementCritical = false;
    // Log once on entry to suppression so it's visible in the console
    static bool suppressLoggedThisCycle = false;
    if (!suppressLoggedThisCycle) {
      suppressLoggedThisCycle = true;
      queueConsoleMessageF(
        "VOLT_SPIKE check suppressed: INA OV %s | "
        "ADS=%.2fV INA=%.2fV | Suppression holds for %ds after latch clears.",
        inaOvervoltageLatched ? "latch active" : "recently cleared",
        tick.batteryV, tick.ibv,
        INA_OV_DISAGREE_SUPPRESS_MS / 1000);
    }
  } else {
    static bool suppressLoggedThisCycle = false;
    suppressLoggedThisCycle = false;  // reset for next OV event
  }

  // Lockout
  tick.inLockout = (fieldCollapseTime > 0 && (tick.nowMs - fieldCollapseTime) < FIELD_COLLAPSE_DELAY);

  // Thresholds
  tick.bulkVoltage = BulkVoltage;
  tick.voltageSpikeMargin = VoltageSpikeMargin;
  tick.inAbsorptionStage = inAbsorptionStage;

  return tick;
}


void resetLearningTableToDefaults() {
  for (int i = 0; i < RPM_TABLE_SIZE; i++) {
    rpmTableRPMPoints[i] = defaultRPMValues[i];
    rpmCapCurrentTable[i] = defaultCapCurrentValues[i];
    rpmCapPowerTable[i] = defaultCapPowerValues[i];
    rpmMinDutyTable[i] = defaultMinDutyValues[i];
    lastOverheatTime[i] = 0;
    overheatCount[i] = 0;
    cumulativeNoOverheatTime[i] = 0;
  }
  capLimitMode = 0;
  overheatPenaltyEndMs = 0;
  overheatingPenaltyAmps = 0;
  totalSafeMs = 0;
  // Reset diagnostics
  totalLearningEvents = 0;
  totalOverheats = 0;
  saveUserTableEdits();  // User-initiated reset, save immediately
  queueConsoleMessage("Learning: All tables reset to factory defaults");
}
void loadLearningTableFromNVS() {
  nvs_handle_t nvs_handle;
  esp_err_t err = nvs_open("learning", NVS_READONLY, &nvs_handle);
  if (err != ESP_OK) {
    queueConsoleMessage("Learning: No saved table, using defaults");
    resetLearningTableToDefaults();
    return;
  }

  bool allValid = true;
  size_t required_size;

  // Load RPM breakpoints
  required_size = sizeof(rpmTableRPMPoints);
  err = nvs_get_blob(nvs_handle, "rpmPoints", rpmTableRPMPoints, &required_size);
  if (err != ESP_OK || required_size != sizeof(rpmTableRPMPoints)) {
    allValid = false;
  }

  // Load cap current table
  required_size = sizeof(rpmCapCurrentTable);
  err = nvs_get_blob(nvs_handle, "capTable", rpmCapCurrentTable, &required_size);
  if (err != ESP_OK || required_size != sizeof(rpmCapCurrentTable)) {
    // Not critical - just use defaults
    for (int i = 0; i < RPM_TABLE_SIZE; i++) {
      rpmCapCurrentTable[i] = defaultCapCurrentValues[i];
    }
  }

  required_size = sizeof(rpmCapPowerTable);
  err = nvs_get_blob(nvs_handle, "capPowerTable", rpmCapPowerTable, &required_size);
  if (err != ESP_OK || required_size != sizeof(rpmCapPowerTable)) {
    for (int i = 0; i < RPM_TABLE_SIZE; i++) {
      rpmCapPowerTable[i] = 0.0f;
    }
  }

  // ADD: Load cap limit mode (non-critical, default to amps)
  err = nvs_get_u8(nvs_handle, "capLimitMode", &capLimitMode);
  if (err != ESP_OK) {
    capLimitMode = 0;
  }

  // Load min duty table
  required_size = sizeof(rpmMinDutyTable);
  err = nvs_get_blob(nvs_handle, "minDutyTable", rpmMinDutyTable, &required_size);
  if (err != ESP_OK || required_size != sizeof(rpmMinDutyTable)) {
    // Not critical - just use defaults
    for (int i = 0; i < RPM_TABLE_SIZE; i++) {
      rpmMinDutyTable[i] = defaultMinDutyValues[i];
    }
  }

  // If core data invalid, reset everything
  if (!allValid) {
    queueConsoleMessage("Learning: Core data invalid, resetting");
    resetLearningTableToDefaults();
    nvs_close(nvs_handle);
    return;
  }

  // Load historical data (optional)
  bool historicalDataValid = true;

  required_size = sizeof(overheatCount);
  err = nvs_get_blob(nvs_handle, "overheatCount", overheatCount, &required_size);
  if (err != ESP_OK || required_size != sizeof(overheatCount)) {
    memset(overheatCount, 0, sizeof(overheatCount));
    historicalDataValid = false;
  }

  required_size = sizeof(lastOverheatTime);
  err = nvs_get_blob(nvs_handle, "lastOverheat", lastOverheatTime, &required_size);
  if (err != ESP_OK || required_size != sizeof(lastOverheatTime)) {
    memset(lastOverheatTime, 0, sizeof(lastOverheatTime));
    historicalDataValid = false;
  }

  required_size = sizeof(cumulativeNoOverheatTime);
  err = nvs_get_blob(nvs_handle, "cumulativeTime", cumulativeNoOverheatTime, &required_size);
  if (err != ESP_OK || required_size != sizeof(cumulativeNoOverheatTime)) {
    memset(cumulativeNoOverheatTime, 0, sizeof(cumulativeNoOverheatTime));
    historicalDataValid = false;
  }

  // Load diagnostic counters
  uint32_t temp_uint32;
  if (nvs_get_u32(nvs_handle, "totalEvents", &temp_uint32) == ESP_OK) {
    totalLearningEvents = temp_uint32;
  } else {
    totalLearningEvents = 0;
  }

  if (nvs_get_u32(nvs_handle, "totalOverheats", &temp_uint32) == ESP_OK) {
    totalOverheats = temp_uint32;
  } else {
    totalOverheats = 0;
  }

  required_size = sizeof(totalSafeMs);
  err = nvs_get_blob(nvs_handle, "totalSafeMs", &totalSafeMs, &required_size);
  if (err != ESP_OK || required_size != sizeof(totalSafeMs)) {
    totalSafeMs = 0;
  }

  if (!historicalDataValid) {
    totalOverheats = 0;
    totalLearningEvents = 0;
    queueConsoleMessage("Learning: Historical data invalid, counters reset");
  }

  nvs_close(nvs_handle);
  queueConsoleMessage("Learning: Table loaded from NVS");
}

// Immediate save of all user-editable columns (no throttle)
// Used when user clicks Save button or Reset button
void saveUserTableEdits() {
  nvs_handle_t nvs_handle;
  esp_err_t err = nvs_open("learning", NVS_READWRITE, &nvs_handle);
  if (err != ESP_OK) {
    queueConsoleMessageF("Learning: Failed to open NVS (err=%d)", (int)err);
    return;
  }

  bool success = true;

  // Save all user-editable columns
  err = nvs_set_blob(nvs_handle, "rpmPoints", rpmTableRPMPoints, sizeof(rpmTableRPMPoints));
  if (err != ESP_OK) success = false;

  err = nvs_set_blob(nvs_handle, "capTable", rpmCapCurrentTable, sizeof(rpmCapCurrentTable));
  if (err != ESP_OK) success = false;

  err = nvs_set_blob(nvs_handle, "capPowerTable", rpmCapPowerTable, sizeof(rpmCapPowerTable));
  if (err != ESP_OK) success = false;

  err = nvs_set_u8(nvs_handle, "capLimitMode", capLimitMode);
  if (err != ESP_OK) success = false;

  err = nvs_set_blob(nvs_handle, "minDutyTable", rpmMinDutyTable, sizeof(rpmMinDutyTable));
  if (err != ESP_OK) success = false;

  err = nvs_commit(nvs_handle);
  if (err != ESP_OK) {
    queueConsoleMessageF("Learning: NVS commit failed (err=%d)", (int)err);
    success = false;
  }

  nvs_close(nvs_handle);

  if (!success) {
    queueConsoleMessage("Learning: Save completed with errors");
  }
}

// Immediate save of historical data (no throttle)
// Used by clearOverheatHistoryAction() - user-initiated
void saveHistoricalDataImmediate() {
  nvs_handle_t nvs_handle;
  esp_err_t err = nvs_open("learning", NVS_READWRITE, &nvs_handle);
  if (err != ESP_OK) {
    queueConsoleMessageF("Learning: Failed to open NVS (%d)", (int)err);
    return;
  }

  bool success = true;

  // Save historical data only
  err = nvs_set_blob(nvs_handle, "overheatCount", overheatCount, sizeof(overheatCount));
  if (err != ESP_OK) success = false;

  err = nvs_set_blob(nvs_handle, "lastOverheat", lastOverheatTime, sizeof(lastOverheatTime));
  if (err != ESP_OK) success = false;

  err = nvs_set_blob(nvs_handle, "cumulativeTime", cumulativeNoOverheatTime, sizeof(cumulativeNoOverheatTime));
  if (err != ESP_OK) success = false;

  // Save diagnostic counters
  err = nvs_set_u32(nvs_handle, "totalEvents", (uint32_t)totalLearningEvents);
  if (err != ESP_OK) success = false;

  err = nvs_set_u32(nvs_handle, "totalOverheats", (uint32_t)totalOverheats);
  if (err != ESP_OK) success = false;

  err = nvs_set_blob(nvs_handle, "totalSafeMs", &totalSafeMs, sizeof(totalSafeMs));
  if (err != ESP_OK) success = false;

  err = nvs_commit(nvs_handle);
  if (err != ESP_OK) {
    success = false;
  }

  nvs_close(nvs_handle);

  if (!success) {
    queueConsoleMessage("Learning: Historical save completed with errors");
  }
}

void updateRPMBucketHistory(uint32_t nowMs) {
  static uint32_t lastHistoryMs = 0;
  uint32_t dtMs = (lastHistoryMs == 0) ? 0 : (uint32_t)(nowMs - lastHistoryMs);
  if (dtMs > 500) dtMs = 500;
  lastHistoryMs = nowMs;

  if (isnan(TempToUse) || TempToUse < -50.0f || TempToUse > 400.0f) return;
  if (dtMs == 0) return;

  int bucket = currentRPMTableIndex;
  if (bucket < 0 || bucket >= RPM_TABLE_SIZE) return;

  static bool inOverheat = false;
  static uint32_t lastNVSSaveMs = 0;

  if (!inOverheat) {
    if (TempToUse >= TemperatureLimitF) {
      inOverheat = true;
      overheatCount[bucket]++;
      totalOverheats++;
      timeSinceLastOverheat = 0;
      saveHistoricalDataImmediate();
    } else {
      cumulativeNoOverheatTime[bucket] += dtMs;
      totalSafeMs += (uint64_t)dtMs;
      totalSafeHours = (float)(totalSafeMs / 3600000ULL);
      timeSinceLastOverheat += dtMs;

      if (AutoSaveLearningTable && (nowMs - lastNVSSaveMs >= 300000UL)) {
        lastNVSSaveMs = nowMs;
        saveHistoricalDataImmediate();
      }
    }
  } else {
    if (TempToUse < (TemperatureLimitF - 2.0f)) {
      inOverheat = false;
    }
  }
}

void clearOverheatHistoryAction() {
  for (int i = 0; i < RPM_TABLE_SIZE; i++) {
    overheatCount[i] = 0;
    lastOverheatTime[i] = 0;
    cumulativeNoOverheatTime[i] = 0;  // ADD
  }
  totalOverheats = 0;
  totalSafeMs = 0;        // ADD
  totalSafeHours = 0.0f;  // ADD
  saveHistoricalDataImmediate();
  queueConsoleMessage("Overheat History: All history and safe time cleared");
}
// ==================== RPM TABLE LOOKUPS ====================
//
// All segment walking and interpolation is centralized here.
// findRPMSegment() is the single source of truth for boundary conditions —
// edit it once and all lookups, the index tracker, and bucket history
// stay consistent automatically.

/**
 * findRPMSegment()
 * Returns the lower-bound segment index for a given RPM.
 *
 *  -1              : rpm is below the first breakpoint (clamp to index 0)
 *   0..N-2         : rpm falls within segment [i, i+1]
 *   RPM_TABLE_SIZE-1 : rpm is at or above the last breakpoint (clamp to last)
 *
 * All table lookup functions and updateCurrentRPMTableIndex() call this
 * so boundary logic can never diverge between them.
 */
int findRPMSegment(float rpm) {
  if (rpm < rpmTableRPMPoints[0]) return -1;
  if (rpm >= rpmTableRPMPoints[RPM_TABLE_SIZE - 1]) return RPM_TABLE_SIZE - 1;
  for (int i = 0; i < RPM_TABLE_SIZE - 1; i++) {
    if (rpm >= rpmTableRPMPoints[i] && rpm < rpmTableRPMPoints[i + 1]) return i;
  }
  return RPM_TABLE_SIZE - 1;  // Fallback — should never reach
}

/**
 * interpolateRPMTable()
 * Linearly interpolates any float table at the given RPM.
 * Clamps to first/last entry when RPM is out of range.
 */
float interpolateRPMTable(float rpm, const float *table) {
  int seg = findRPMSegment(rpm);
  if (seg < 0) return table[0];
  if (seg >= RPM_TABLE_SIZE - 1) return table[RPM_TABLE_SIZE - 1];
  float rpm1 = (float)rpmTableRPMPoints[seg];
  float rpm2 = (float)rpmTableRPMPoints[seg + 1];
  float fraction = (rpm - rpm1) / (rpm2 - rpm1);
  return table[seg] + fraction * (table[seg + 1] - table[seg]);
}

/**
 * updateCurrentRPMTableIndex()
 * Sets currentRPMTableIndex to the segment that owns this RPM.
 * Clamps to 0 when below range so the first row is always highlighted.
 * Call once per tick before updateRPMBucketHistory().
 */
void updateCurrentRPMTableIndex(float rpm) {
  int seg = findRPMSegment(rpm);
  currentRPMTableIndex = (seg < 0) ? 0 : seg;
}

// One-liner lookups — all boundary logic lives in findRPMSegment/interpolateRPMTable
float getMinimumFieldForRPM(float rpm) {
  return interpolateRPMTable(rpm, rpmMinDutyTable);
}
float getCapCurrentForRPM(float rpm) {
  return interpolateRPMTable(rpm, rpmCapCurrentTable);
}

/**
 * isVoltageSensorPlausible()
 * Voltage ranges (with 0.5V safety buffer):
 * - 12V system: 4.5V to 15.5V
 * - 24V system: 9.0V to 30.5V  
 * - 48V system: 18V to 60.5V
 * 
 * @return true if at least one voltage sensor shows plausible reading
 */
bool isVoltageSensorPlausible() {  // may want to update this later to go off the users' entered Nominal Voltage rather than some autodetect that can F up
  float minPlausible, maxPlausible;

  // Auto-detect system voltage from BulkVoltage setting
  if (BulkVoltage < 18.0f) {
    // 12V system (normal bulk = 13.8-14.4V)
    minPlausible = 4.5f;   // Dead battery - 0.5V buffer
    maxPlausible = 15.5f;  // Max charging + 0.5V buffer
  } else if (BulkVoltage < 36.0f) {
    // 24V system (normal bulk = 27.6-28.8V)
    minPlausible = 9.0f;   // Dead battery - 0.5V buffer
    maxPlausible = 30.5f;  // Max charging + 0.5V buffer
  } else {
    // 48V system (normal bulk = 55.2-57.6V)
    minPlausible = 18.0f;  // Dead battery - 0.5V buffer
    maxPlausible = 60.5f;  // Max charging + 0.5V buffer
  }

  // Check BatteryV sensor
  bool batteryVPlausible = (BatteryV >= minPlausible && BatteryV <= maxPlausible && !isnan(BatteryV));

  // Check IBV sensor
  bool ibvPlausible = (IBV >= minPlausible && IBV <= maxPlausible && !isnan(IBV));

  // Valid if at least one sensor is plausible
  // If BOTH are implausible, that's a critical fault
  return (batteryVPlausible || ibvPlausible);
}

// ===========================================================================
// tempPID_init()
// Call from setup() after NVS and sensors are initialized.
// ===========================================================================
void tempPID_init() {
  // Output limits: 0 to MaxTableValue amps (penalty range)
  tempPID.SetOutputLimits(-(double)MaxTableValue, (double)MaxTableValue);  //  set a reasonable static default for startup
  tempPID.SetSampleTime((int)TempPIDIntervalMs);

  // Start with zero penalty — PID will accumulate penalty only if temperature demands it
  thermalPenaltyAmps_d = 0.0;
  thermalPenaltyAmps = 0.0f;
  tempPIDActive = false;

  // Don't enable AUTO yet — wait for first valid temp reading in tempPID_tick()
  tempPID.SetMode(MANUAL);

  Serial.printf("TempPID: Init | Kp=%.2f Ki=%.3f Kd=%.2f Margin=%.1f°F Interval=%lums\n",
                TempPIDKp, TempPIDKi, TempPIDKd, TempPIDMarginF, (unsigned long)TempPIDIntervalMs);
}
void tempPID_tick(uint32_t nowMs, float actualDtSec) {

  // --- Hard reset path (requested externally, e.g. from web UI command) ---
  if (tempPIDResetRequested) {
    tempPIDResetRequested = false;
    thermalPenaltyAmps = 0.0f;
    thermalPenaltyAmps_d = 0.0;
    tempPID.SetMode(MANUAL);
    tempPID.ResetIntegratorTo(0.0);
    tempPIDActive = false;
    tempFilterNeedsReseed = true;
    for (int i = 0; i < 6; i++) dBuf[i] = NAN;
    dHead = 0;
    queueConsoleMessage("ThermalPID: manual reset requested - integrator and filter cleared");
    return;
  }

  // ---------------------------------------------------------------------------
  //  Stage-aware penalty bounds — computed ONCE, referenced everywhere below.
  //
  //  Derived from inBulkStage / inAbsorptionStage directly, NOT from
  //  voltageControlActive. Reason: in AdjustFieldLearnMode(), the assignment
  //
  //      voltageControlActive = (!inBulkStage || inAbsorptionStage);
  //
  //  executes AFTER tempPID_tick() returns. On a stage-transition tick,
  //  voltageControlActive still carries the previous stage's value when we
  //  arrive here. inBulkStage and inAbsorptionStage are written by
  //  updateChargingStage(), which runs before tempPID_tick(), so they are
  //  always current. Stage policy must come from stage variables, not from a
  //  derived flag that may be one tick behind.
  //
  //  Policy:
  //    Pure bulk (CC):    cold boost allowed — penaltyMin may be negative.
  //    Absorption / float (CV): cold boost forbidden — penaltyMin = 0.
  //
  //  inPureBulk and penaltyMin are used in:
  //    - SetOutputLimits() at re-enable
  //    - SetOutputLimits() every tick
  //    - clamp after external D
  //    - clamp after slew limiter
  //    - CV integrator bleed gate
  //    - outerImpliedPenalty telemetry condition
  //  No other lower-bound formula exists in this function.
  // ---------------------------------------------------------------------------
  const float capCurrent = getCapCurrentForRPM(RPM);
  const float penaltyMax = (float)MaxTableValue;

  const bool inPureBulk = (inBulkStage && !inAbsorptionStage);
  const float penaltyMin = 0.0f;  //Cold boost is gone as a concept now. You're already commanding I_cap at thermal neutral — there's nowhere to boost to. Penalty only derates downward from there. The inPureBulk distinction for penaltyMin is now meaningless.

  // ---------------------------------------------------------------------------
  //  Temperature freshness
  // ---------------------------------------------------------------------------
  uint32_t tempTimestampIdx = (TempSource == 0) ? IDX_ALTERNATOR_TEMP : IDX_THERMISTOR_TEMP;
  bool tempTimestampValid = (dataTimestamps[tempTimestampIdx] != 0);
  uint32_t tempAge = tempTimestampValid
                       ? (nowMs - dataTimestamps[tempTimestampIdx])
                       : 999999;
  bool tempFresh = tempTimestampValid && (tempAge <= TempPIDStaleMs);
  bool tempValueSane = !isnan(TempToUse) && (TempToUse > -50.0f) && (TempToUse < 400.0f);

  if (!tempFresh || !tempValueSane) {
    if (tempPIDActive) {
      tempPID.SetMode(MANUAL);
      tempPIDActive = false;
      queueConsoleMessageF("TempPID: temp stale (age=%lums), holding penalty at %.1fA",
                           (unsigned long)tempAge, thermalPenaltyLastValid);
    }
    return;  // Hold last valid penalty — do not touch thermalPenaltyAmps.
  }

  // ---------------------------------------------------------------------------
  //  IIR filter
  // ---------------------------------------------------------------------------
  if (isnan(tempFiltered) || tempFilterNeedsReseed) {
    tempFiltered = TempToUse;
    tempFilterNeedsReseed = false;
  } else {
    tempFiltered = TempPIDFilterAlpha * TempToUse + (1.0f - TempPIDFilterAlpha) * tempFiltered;
  }

  // ---------------------------------------------------------------------------
  //  Re-enable after stale period — bumpless transfer
  //
  //  resumePenalty is clamped to current stage bounds before seeding the
  //  integrator. If we enter CV carrying negative bulk bias, the integrator
  //  starts at 0, not at a cold-boost value that is illegal in this stage.
  //
  //  All three penalty state variables are written to resumePenalty here so
  //  that the command chain, slew limiter, and stale-hold path all start from
  //  the same consistent value on the first post-resume tick:
  //    thermalPenaltyAmps    — what the command chain reads this tick
  //    prevThermalPenalty    — what the slew limiter uses as its prev on the
  //                            next pidComputed tick (avoids spurious first step)
  //    thermalPenaltyLastValid — what the stale-hold path returns if temp goes
  //                              stale immediately after re-enable
  // ---------------------------------------------------------------------------
  if (!tempPIDActive) {
    tempPID.SetOutputLimits((double)penaltyMin, (double)penaltyMax);
    tempPID.SetTunings((double)TempPIDKp, (double)TempPIDKi, (double)TempPIDKd);
    tempPIDInput_d = (double)tempFiltered;
    tempPIDSetpoint_d = (double)(TemperatureLimitF - TempPIDMarginF);
    thermalPenaltyAmps_d = (double)thermalPenaltyLastValid;
    tempPID.SetMode(AUTOMATIC);

    float resumePenalty = clamp_f(thermalPenaltyLastValid, penaltyMin, penaltyMax);
    tempPID.ResetIntegratorTo((double)resumePenalty);

    thermalPenaltyAmps = resumePenalty;
    prevThermalPenalty = resumePenalty;
    thermalPenaltyLastValid = resumePenalty;

    tempPIDActive = true;
    for (int i = 0; i < 6; i++) dBuf[i] = tempFiltered;
    dHead = 0;
    queueConsoleMessageF("TempPID: resumed | temp=%.1f°F setpoint=%.1f°F penalty=%.1fA stage=%s",
                         tempFiltered, TemperatureLimitF - TempPIDMarginF, resumePenalty,
                         inPureBulk ? "bulk" : "CV");
  }

  // ---------------------------------------------------------------------------
  //  Update setpoint and output limits every tick.
  //  Both use the same penaltyMin computed at function entry.
  // ---------------------------------------------------------------------------
  tempPIDSetpoint_d = (double)(TemperatureLimitF - TempPIDMarginF);
  tempPIDInput_d = (double)tempFiltered;

  tempPID.SetTunings((double)TempPIDKp, (double)TempPIDKi, (double)TempPIDKd);
  tempPID.SetOutputLimits((double)penaltyMin, (double)penaltyMax);

  bool pidComputed = tempPID.Compute();

  // ---------------------------------------------------------------------------
  //  Post-compute: external D, slew limiter, final clamp.
  //  All use penaltyMin / penaltyMax — no re-derivation.
  // ---------------------------------------------------------------------------
  if (pidComputed) {
    thermalPenaltyAmps = (float)thermalPenaltyAmps_d;

    // External 20-second derivative.
    // Ring buffer always advances; gain gate controls whether it is applied.
    float oldest = dBuf[dHead];
    dBuf[dHead] = tempFiltered;
    dHead = (dHead + 1) % 6;

    if (TempPIDKdExternal != 0.0f && !isnan(oldest)) {
      float dWindowSec = (TempPIDIntervalMs * 6) / 1000.0f;
      float dTdt = (tempFiltered - oldest) / dWindowSec;

      outerTermDExternal = TempPIDKdExternal * dTdt;
      thermalPenaltyAmps += outerTermDExternal;
      thermalPenaltyAmps = clamp_f(thermalPenaltyAmps, penaltyMin, penaltyMax);
    } else {
      outerTermDExternal = 0.0f;
    }

    // Asymmetric slew limiter, then re-clamp with same bounds.
    thermalPenaltyAmps = slew_limit_f(prevThermalPenalty, thermalPenaltyAmps,
                                      ThermalPenaltyRiseRate, ThermalPenaltyFallRate,
                                      actualDtSec);
    thermalPenaltyAmps = clamp_f(thermalPenaltyAmps, penaltyMin, penaltyMax);

    prevThermalPenalty = thermalPenaltyAmps;
    thermalPenaltyLastValid = thermalPenaltyAmps;

    outerTermP = (float)tempPID.GetPterm();
    outerTermD = (float)tempPID.GetDterm();
  }

  // outerTermI is updated every tick (not just on pidComputed) because
  // GetIterm() returns the live unclamped integrator state. The CV bleed
  // below may advance it between compute ticks, and the log must reflect
  // that movement — otherwise the log shows a stale bulk-compute value
  // while antiWindupFired shows 0, making the bleed appear inert when it
  // isn't. outerTermP and outerTermD are only valid after a Compute() call
  // so they remain inside the pidComputed block above.
  outerTermI = (float)tempPID.GetIterm();

  // ---------------------------------------------------------------------------
  //  CV integrator bleed
  //
  //  New behavior: in CV modes only, if the integrator is negative (stale
  //  cold-boost bias carried in from bulk), TrackAppliedOutput(0) gently
  //  nudges it toward zero. The decay rate is governed by the library's
  //  back-calculation mechanism (TrackingGain * ki * dt), which is slow by
  //  design — the goal is smooth erasure of illegal state, not a hard snap.
  //
  //  Asymmetric: positive thermal derate (genuinely hot alternator) is never
  //  touched by this block. It only fires when iterm < 0, which in CV is
  //  always stale state rather than valid thermal information.
  //
  //  Cadence: runs every tempPID_tick() call (~16 Hz inner loop rate), not
  //  only on Compute() ticks (5-second interval). TrackAppliedOutput() is
  //  dt-scaled so the per-call movement is tiny and rate-correct regardless
  //  of call frequency. This is intentional — we want the bleed to be active
  //  across the full time between compute ticks, not only on them.
  //
  //  outerAntiWindupFired is repurposed: now means "CV bleed was active this
  //  tick." Log schema meaning has changed; see comment in thermalLog_tick().
  // ---------------------------------------------------------------------------
  outerAntiWindupFired = false;

  if (!inPureBulk) {
    double iterm = tempPID.GetIterm();
    // On the first bleed tick after CV entry, log the live iterm so it can be
    // cross-checked against outerTermI in the thermal log. This verifies that
    // GetIterm() returns the live unclamped integrator state rather than the
    // stale last-compute P+I contribution stored in outerTermI.
    static bool cvBleedFired = false;
    if (iterm < 0.0) {
      if (!cvBleedFired) {
        cvBleedFired = true;
        queueConsoleMessageF(
          "TempPID CV bleed ENTRY: live iterm=%.1fA | outerTermI_col=%.1fA | "
          "if these differ, GetIterm() may return last-compute snapshot, not live state",
          (float)iterm, outerTermI);
      }
      tempPID.TrackAppliedOutput(0.0, (double)actualDtSec);
      outerAntiWindupFired = true;
    } else {
      cvBleedFired = false;  // Reset so the entry log fires again next CV entry
    }
  }

  // outerImpliedPenalty: voltage cap expressed as a downstream penalty
  // equivalent, for log debugging only. Does not drive any control action.
  // Keyed off inPureBulk (stage-derived) for the same timing reason as
  // penaltyMin — voltageControlActive may be one tick stale at transitions.
  if (!inPureBulk && Icv > 1.0f) {
    outerImpliedPenalty = fmaxf(0.0f, capCurrent - Icv);
  } else {
    outerImpliedPenalty = 0.0f;
  }
}
void pidLog_init() {
  if (!psramFound()) {
    Serial.println("FATAL: PSRAM not found, pidLog disabled");
    pidLogReady = false;
    pidLog = nullptr;
    return;
  }

  pidLog = (PidLogEntry *)heap_caps_malloc(
    PID_LOG_SIZE * sizeof(PidLogEntry),
    MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);

  if (!pidLog) {
    Serial.println("FATAL: Failed to allocate pidLog in PSRAM");
    pidLogReady = false;
    return;
  }

  pidLogHead = 0;
  pidLogCount = 0;
  pidLogReady = true;

  Serial.printf("PidLog: %d entries x %u bytes = %u KB in PSRAM\n",
                PID_LOG_SIZE,
                (unsigned)sizeof(PidLogEntry),
                (unsigned)(PID_LOG_SIZE * sizeof(PidLogEntry) / 1024));
}
// pidLog_tick()
// Call ONCE, at the END of the normal control path in AdjustFieldLearnMode(),
// after governor_apply() and all state updates. Captures a snapshot of the
// complete outer→inner→duty pipeline for that tick.
// NOT called during shutdown/fault paths — those are not the instability target.
void pidLog_tick(uint32_t nowMs) {
  if (!pidLogReady || !pidLog) return;

  // Pause watchdog: auto-resume if download stalled or was aborted
  if (pidLogPaused) {
    if ((uint32_t)(nowMs - pidLogPausedAtMs) > THERMAL_LOG_PAUSE_TIMEOUT_MS) {
      Serial.println("pidLog: pause watchdog triggered - connection likely aborted");
      pidLogPaused = false;
    } else {
      return;
    }
  }

  // No interval gate here — tick rate is governed by the CH1 data gate upstream.

  PidLogEntry &e = pidLog[pidLogHead];

  // ── Timestamp ─────────────────────────────────────────────────────────────
  e.ts = nowMs;

  // ── Mode / Stage ──────────────────────────────────────────────────────────
  e.chargeStageDisplay = getChargeStageDisplayCode();
  e.TargetVoltageMode = (uint8_t)TargetVoltageMode;
  e.flags = 0;
  if (sysMode == SYS_MODE_AUTO) e.flags |= (1 << 0);
  if (voltageControlActive) e.flags |= (1 << 1);
  if (govMode != GOV_NORMAL_SLEW) e.flags |= (1 << 4);
  e.pad0 = 0;

  // ── Voltage outer loop ────────────────────────────────────────────────────
  e.battV = BatteryV;
  e.ChargingVoltageTarget = ChargingVoltageTarget;
  e.vError = pidLog_vError;
  e.Icv = Icv;
  e.cv_I = cv_I;
  e.tableThermalLimit = pidLog_uTargetBeforeVoltCap;
  e.setpointCmd = pidLog_uTargetAfterVoltCap;

  // ── Voltage loop event flags ──────────────────────────────────────────────
  e.voltageLoopRanThisTick = pidLog_voltageLoopRanThisTick;
  e.enteringCV = pidLog_enteringCV;
  e.enteringTargetVoltageMode = pidLog_enteringTargetVoltageMode;
  e.pad1 = 0;

  // ── Inner current PID ─────────────────────────────────────────────────────
  e.pidSetpoint = (float)pidSetpoint;
  e.pidInput = (float)pidInput;
  e.pidUnsatOutput = (sysMode == SYS_MODE_AUTO)
                       ? (float)currentPID.GetLastUnsatOutput()
                       : dutyCycle;
  e.pidOutput = (float)pidOutput;
  e.innerTermP = innerTermP;
  e.innerTermI = innerTermI;
  e.innerTermD = innerTermD;

  // ── Duty pipeline ─────────────────────────────────────────────────────────
  e.dutyRequest = pidLog_dutyRequest;
  e.dutyApplied = pidLog_dutyApplied;

  // ── Context ───────────────────────────────────────────────────────────────
  e.rpm = RPM;
  e.measAmps = MeasuredAmps;
  e.gainKp = (float)PidKp;
  e.gainKi = (float)PidKi;
  e.gainKd = (float)PidKd;

  e.battV_filt = BatteryV_filtered;
  e.iMeas_filt = MeasuredAmps_filtered;

  pidLogHead = (pidLogHead + 1) % PID_LOG_SIZE;
  if (pidLogCount < PID_LOG_SIZE) pidLogCount++;
}

void thermalLog_init() {
  Serial.printf("thermalLog_init: PSRAM found=%d\n", psramFound());

  if (!psramFound()) {
    Serial.println("FATAL: PSRAM not found");
    thermalLogReady = false;
    thermalLog = nullptr;
    return;
  }

  thermalLog = (ThermalLogEntry *)heap_caps_malloc(
    THERMAL_LOG_SIZE * sizeof(ThermalLogEntry),
    MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);

  Serial.printf("thermalLog_init: alloc %d bytes = %p\n",
                THERMAL_LOG_SIZE * sizeof(ThermalLogEntry), thermalLog);

  if (!thermalLog) {
    Serial.println("FATAL: malloc failed");
    thermalLogReady = false;
    return;
  }

  thermalLogReady = true;
  Serial.printf("thermalLog_init: OK, %d entries x %d bytes = %d KB\n",
                THERMAL_LOG_SIZE, sizeof(ThermalLogEntry),
                THERMAL_LOG_SIZE * sizeof(ThermalLogEntry) / 1024);
}

void thermalLog_tick(uint32_t nowMs) {
  if (!thermalLogReady || !thermalLog) return;

  // Watchdog: auto-unpause if download stalled/aborted
  if (thermalLogPaused) {
    if ((uint32_t)(nowMs - thermalLogPausedAtMs) > THERMAL_LOG_PAUSE_TIMEOUT_MS) {
      Serial.println("thermalLog: pause watchdog triggered - connection likely aborted");
      thermalLogPaused = false;
    } else {
      return;
    }
  }

  static uint32_t lastLogMs = 0;
  if ((uint32_t)(nowMs - lastLogMs) < THERMAL_LOG_INTERVAL_MS) return;
  lastLogMs = nowMs;

  ThermalLogEntry &e = thermalLog[thermalLogHead];

  e.ts = nowMs;
  e.tempFiltered = thermalLogScale10((float)tempPIDInput_d);
  e.tempSetpoint = thermalLogScale10((float)tempPIDSetpoint_d);
  e.nominalTarget = thermalLogScale10(getCapCurrentForRPM(RPM));
  e.rpmCap = thermalLogScale10(getCapCurrentForRPM(RPM));
  e.voltCap = thermalLogScale10(Icv);
  e.uTarget = thermalLogScale10((float)uTargetAmps);
  e.spLimited = thermalLogScale10(setpointLimited);
  e.pidErr = thermalLogScale10(pidError);
  e.pidOut = thermalLogScale10((float)pidOutput);
  e.duty = thermalLogScale10(dutyCycle);
  e.rpm = thermalLogScaleRPM(RPM);
  e.battV = thermalLogScale10(BatteryV);
  e.measAmps = thermalLogScale10(MeasuredAmps);
  e.penaltyAmps = thermalLogScale10(thermalPenaltyAmps);

  e.flags = 0;
  if (tempPIDActive) e.flags |= (1 << 0);
  if (sysMode == SYS_MODE_AUTO) e.flags |= (1 << 4);
  if (shutdownPhase != SHUTDOWN_PHASE_NONE) e.flags |= (1 << 5);

  e.antiWindupFired = outerAntiWindupFired ? 1 : 0;
  e.chargeStageDisplay = thermalLogGetStageCode();
  e.pad = 0;

  e.outerTermP = thermalLogScale10(outerTermP);
  e.outerTermI = thermalLogScale10(outerTermI);
  e.outerTermD = thermalLogScale10(outerTermD);
  e.impliedPenalty = thermalLogScale10(outerImpliedPenalty);
  e.outerTermDExternal = thermalLogScale10(outerTermDExternal);

  thermalLogHead = (thermalLogHead + 1) % THERMAL_LOG_SIZE;
  if (thermalLogCount < THERMAL_LOG_SIZE) thermalLogCount++;
}
