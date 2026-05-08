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
void updateProtectionCounters(FieldEventReason reason);
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
// Control loop sub-path helpers
void handleLimpHome(uint32_t currentMillis, const TickSnapshot& tick);
void runShutdownPath(const TickSnapshot& tick, FieldControlMode mode, FieldEventReason reason,
                    float actualDtSec, bool exitingNormal);

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
  bool alreadyCut = gpio4IsLow;
  digitalWrite(4, LOW);
  gpio4IsLow = true;
  if (alreadyCut) return;  // hardware already cut — skip logging, don't spam
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
  // inaOvervoltageLatched means HW ALERT pin fired first; SW is catching up.
  // !inaOvervoltageLatched means SW caught it before hardware (e.g. hard OC, temp).
  const char *caughtBy = inaOvervoltageLatched ? "HW ALERT pin fired first; SW latch active"
                                               : "SW caught first (no HW alert)";
  queueConsoleMessageF("Field cut immediately: %s | %s | ADS=%.2fV INA=%.2fV D=%.3fV",
                       reasonToString(reason), caughtBy, BatteryV, IBV, fabsf(BatteryV - IBV));
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

// ==================== CONTROL LOOP SUB-PATH HELPERS ====================
// Each function handles one complete early-exit case from AdjustFieldLearnMode.
// None of these touch the CV integrator (cv_I), bumpless tracker, or iExcess state.

// handleLimpHome — Emergency limp-home path. Bypasses all safeties except INA228.
// Call only when LimpHome==1. Caller returns immediately after.
void handleLimpHome(uint32_t currentMillis, const TickSnapshot& tick) {
  if (OnOff == 0) {
    digitalWrite(4, LOW);
    gpio4IsLow = true;
    apply_pwm_float(0.0f);
    lastAppliedDuty = 0.0f;
    dutyCycle = 0.0f;
    return;
  }
  static uint32_t lastLimpTick = 0;
  static uint32_t lastLimpReport = 0;
  if (currentMillis - lastLimpTick >= 100) {
    lastLimpTick = currentMillis;
    // Respect INA228 hardware OV latch even in limp home — prevents
    // oscillation against the PRE-GATE cut. Hardware ALERT pin already
    // pulled GPIO4 low; don't fight it.
    if (!inaOvervoltageLatched) {
      digitalWrite(4, HIGH);
      gpio4IsLow = false;
      apply_pwm_float(30.0f);
    }
    lastAppliedDuty = 30.0f;
    dutyCycle = 30.0f;
    currentPID.SetMode(MANUAL);
    pidOutput = 30.0;
    currentPID.ResetIntegratorTo(30.0);
    // alarmOutputState cleared by CheckAlarms — do not write GPIO38 here
    updateFieldTelemetry(30.0f, tick.currentBatteryVoltage, FieldResistance);
    fieldActiveStatus = 1;
    if (currentMillis - lastLimpReport >= 30000) {
      lastLimpReport = currentMillis;
      queueConsoleMessage("LIMP HOME MODE: 30% duty, all safeties bypassed");
      Serial.println("LIMP HOME MODE: 30% duty, all safeties bypassed");
    }
  }
}

// runShutdownPath — Fault / non-normal mode state machine.
// Runs the full shutdown ramp (phases 1→3→4→GPIO4 cut) and returns.
// Never reaches the CV control path. prevMode and return are handled by caller.
void runShutdownPath(const TickSnapshot& tick, FieldControlMode mode, FieldEventReason reason,
                     float actualDtSec, bool exitingNormal) {
  voltageControlActive = false;

  // GPIO38 driven solely by CheckAlarms — do not write here

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
  // Phase 3: slew already applied above — bypass governor's own slew, but keep clamping.
  GovernorMode shutdownGovMode = (shutdownPhase == SHUTDOWN_PHASE_3) ? GOV_BYPASS_SLEW : govMode;

  float dutyNewFloat = governor_apply(lastAppliedDuty, dutyRequest, shutdownGovMode,
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
}

// ==================== MAIN CONTROL FUNCTION ====================


/**
 * AdjustFieldLearnMode - Main field control
 *
 * Architecture:
 *  1.  Build TickSnapshot and timing state
 *  2.  Pre-gate immediate-cut fault check
 *  3.  Fast voltage safety override (runs every loop, before CH1 gate)
 *  4.  Limp-home: handleLimpHome() → return
 *  5.  Gate on fresh CH1 ADC data, with optional PidSampleDivisor
 *  6.  Re-check critical faults and determine control/system mode
 *  7.  Handle mode transitions
 *  8.  Non-normal path: runShutdownPath() → return
 *  9.  Normal path: iExcess supervisor, fastOvCurrentCap application,
 *                   setpoint management and PID compute
 *  10. Build duty request
 *  11. Apply through governor
 *  12. Tell PID what actually happened
 *  13. Update state, telemetry, and logging
 */

// ── OUTPUT CURRENT PID ───────────────────────────────────────
//   Runs every CH1 ADS1115 sample (÷ PidSampleDivisor).
//   ADS sequence {1,0,1,2,1,3}: CH1 fires 3× per 6-step cycle.
//   Back-to-back trigger in ADS_READ_RESULT saves one loop() call per channel.
//
//   Measured performance (preliminary — full hardware validation pending):
//   CH1 interval: ~5ms typical; worst-case not yet characterised.
//   PidSampleDivisor=1 (default) → PID runs every CH1 hit.
//   PidSampleDivisor=2           → every other CH1 hit.
//
// ── VOLTAGE / CV LOOP ────────────────────────────────────────
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
//   ibvFreshFlag fires at ~5ms cadence (field on, INA228 fast mode) or ~1100ms (field off).
//   dvdt EMA alpha=0.08 → effective time constant ~190ms.
//
// ── TEMPERATURE LOOP PID ─────────────────────────────────────
//   Kd External creates noise at current defaults — needs tuning/updating.
//   Loop interval may need adjustment to address Kd External noise.
//
// ── ADS1115 TIMING (output current loop clock source) ────────
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
//   PidSampleDivisor=1 (default) → output current PID runs every CH1 hit.
//   ADS_TIMEOUT_MS = 50          → conversion timeout before retry.

// ============================================================================
// PID TUNING SCORE — helpers called from AdjustFieldLearnMode
// ============================================================================

// Called from setup() after PSRAM allocations and loadTuningLog() are done.
// Prints sizing info; allocation and load happen in setup() proper.
void tuningScore_init() {
  Serial.printf("TuningScore: %d record slots × %u bytes = %u bytes in PSRAM\n",
                50, (unsigned)sizeof(TuningRecord),
                (unsigned)(50 * sizeof(TuningRecord)));
  Serial.printf("TuningScore: live buckets = 4 windows × %d × %u bytes = %u bytes in PSRAM\n",
                (int)LIVE_BUCKET_N, (unsigned)sizeof(ScoreBucket),
                (unsigned)(4 * LIVE_BUCKET_N * sizeof(ScoreBucket)));
}

void saveTuningLog() {
  if (!tuningLog) return;
  File f = LittleFS.open("/tuninglog.bin", "w");
  if (!f) return;
  f.write((uint8_t *)&tuningLogCount,   sizeof(tuningLogCount));
  f.write((uint8_t *)&tuningLogHead,    sizeof(tuningLogHead));
  f.write((uint8_t *)&tuningRunCounter, sizeof(tuningRunCounter));
  f.write((uint8_t *)tuningLog, 50 * sizeof(TuningRecord));
  f.close();
}

void loadTuningLog() {
  if (!tuningLog) return;
  File f = LittleFS.open("/tuninglog.bin", "r");
  if (!f) return;
  f.read((uint8_t *)&tuningLogCount,   sizeof(tuningLogCount));
  f.read((uint8_t *)&tuningLogHead,    sizeof(tuningLogHead));
  f.read((uint8_t *)&tuningRunCounter, sizeof(tuningRunCounter));
  f.read((uint8_t *)tuningLog, 50 * sizeof(TuningRecord));
  f.close();
  Serial.printf("TuningLog: loaded %d records, counter=%d\n", tuningLogCount, tuningRunCounter);
}

void commitTuningRecord() {
  if (!tuningLog) return;
  if (tuningScore.activeTimeSec < 0.5f) {
    tuningScore = {};
    return;
  }
  TuningRecord rec = {};
  rec.runNumber    = ++tuningRunCounter;
  rec.score        = tuningScore.errorAccum / tuningScore.activeTimeSec;
  rec.activeTimeSec = tuningScore.activeTimeSec;
  rec.kp           = PidKp;
  rec.ki           = PidKi;
  rec.kd           = PidKd;
  rec.sampleDivisor = PidSampleDivisor;
  rec.trackingGain  = PIDTrackingGain;
  rec.dutyRampRate  = DutyRampRate;
  rec.waveAmplitude = (int16_t)waveAmplitude;
  rec.wavePeriod    = (int16_t)wavePeriod;
  rec.avgRPM        = (tuningScore.avgSampleCount > 0)
                        ? (tuningScore.rpmSum / tuningScore.avgSampleCount) : 0.0f;
  rec.avgAltTempF   = (tuningScore.avgSampleCount > 0)
                        ? (tuningScore.tempSum / tuningScore.avgSampleCount) : 0.0f;
  rec.worstErrorA   = tuningScore.worstErrorA;

  tuningLog[tuningLogHead] = rec;
  tuningLogHead = (tuningLogHead + 1) % 50;
  if (tuningLogCount < 50) tuningLogCount++;

  saveTuningLog();
  queueConsoleMessageF("TuningScore: run#%d score=%.2f kp=%.3f ki=%.3f kd=%.4f t=%.1fs",
    rec.runNumber, rec.score, rec.kp, rec.ki, rec.kd, rec.activeTimeSec);

  tuningScore = {};  // reset for next test
}

void saveCVTuningLog() {
  if (!cvTuningLog) return;
  File f = LittleFS.open("/cvtuninglog.bin", "w");
  if (!f) return;
  f.write((uint8_t *)&cvTuningLogCount,   sizeof(cvTuningLogCount));
  f.write((uint8_t *)&cvTuningLogHead,    sizeof(cvTuningLogHead));
  f.write((uint8_t *)&cvTuningRunCounter, sizeof(cvTuningRunCounter));
  f.write((uint8_t *)cvTuningLog, 50 * sizeof(CVTuningRecord));
  f.close();
}

void loadCVTuningLog() {
  if (!cvTuningLog) return;
  File f = LittleFS.open("/cvtuninglog.bin", "r");
  if (!f) return;
  f.read((uint8_t *)&cvTuningLogCount,   sizeof(cvTuningLogCount));
  f.read((uint8_t *)&cvTuningLogHead,    sizeof(cvTuningLogHead));
  f.read((uint8_t *)&cvTuningRunCounter, sizeof(cvTuningRunCounter));
  f.read((uint8_t *)cvTuningLog, 50 * sizeof(CVTuningRecord));
  f.close();
  Serial.printf("CVTuningLog: loaded %d records, counter=%d\n", cvTuningLogCount, cvTuningRunCounter);
}

void commitCVTuningRecord() {
  if (!cvTuningLog || cvTuningScore.scoredHighCount < 1) {
    cvTuningScore = {};
    return;
  }
  CVTuningRecord rec = {};
  float n                        = (float)cvTuningScore.scoredHighCount;
  rec.runNumber                  = ++cvTuningRunCounter;
  rec.avgSettlingTimeSec         = cvTuningScore.totalSettlingTimeSec / n;
  rec.avgIntegratedOvershootVs   = cvTuningScore.totalIntegratedOvershootVs / n;
  rec.worstOvershootV            = cvTuningScore.worstOvershootV;
  rec.activeTimeSec              = cvTuningScore.activeTimeSec;
  rec.score                      = rec.avgSettlingTimeSec + cvKOvershoot * rec.avgIntegratedOvershootVs;
  rec.fastOvFires                = cvTuningScore.fastOvFires;
  rec.iExcessFires               = cvTuningScore.iExcessFires;
  rec.loadDumpFires              = cvTuningScore.loadDumpFires;
  rec.hardOcFires                = cvTuningScore.hardOcFires;
  rec.voltageKp                  = VoltageKp;
  rec.voltageKi                  = VoltageKi;
  rec.voltageKd                  = VoltageKd;
  rec.setpointRiseRate           = SetpointRiseRate;
  rec.setpointFallRate           = SetpointFallRate;
  rec.awBleedRate                = AwBleedRate;
  rec.awRecoverRate              = AwRecoverRate;
  rec.awSeedProtectMs            = AwSeedProtectMs;
  rec.iExcessReseedFrac          = IExcessReseedFrac;
  rec.kSoft                      = KSoft;
  rec.kHard                      = KHard;
  rec.iExcessK                   = IExcessK;
  rec.iExcessN                   = IExcessN;
  rec.iExcessKBleed              = IExcessKBleed;
  rec.loadDumpDtThresh           = LoadDumpDtThresh;
  rec.loadDumpCurrentDrop        = LoadDumpCurrentDrop;
  rec.inputFilterTC              = InputFilterTC;
  rec.waveAmplitudeV             = cvWaveAmplitudeV;
  rec.wavePeriodSec              = (uint16_t)cvWavePeriodSec;
  rec.kOvershoot                 = cvKOvershoot;
  rec.consecutiveReads           = cvConsecutiveReads;
  rec.avgRPM                     = (cvTuningScore.avgSampleCount > 0) ? (cvTuningScore.rpmSum  / cvTuningScore.avgSampleCount) : 0.0f;
  rec.avgAltTempF                = (cvTuningScore.avgSampleCount > 0) ? (cvTuningScore.tempSum / cvTuningScore.avgSampleCount) : 0.0f;
  rec.battVAtStart               = cvTuningScore.battVAtStart;
  rec.socAtStart                 = cvTuningScore.socAtStart;
  rec.chargingVoltageTarget      = cvBaseTarget;
  float nl = (float)cvTuningScore.scoredLowCount;
  if (nl > 0.0f) {
    rec.avgLowSettlingTimeSec = cvTuningScore.totalLowSettlingTimeSec / nl;
    rec.avgLowIntOvVs         = cvTuningScore.totalLowIntOvVs / nl;
  } else {
    rec.avgLowSettlingTimeSec = 0.0f;
    rec.avgLowIntOvVs         = 0.0f;
  }
  rec.worstLowOvV = cvTuningScore.worstLowOvV;
  rec.lowScore    = rec.avgLowSettlingTimeSec + cvKOvershoot * rec.avgLowIntOvVs;

  cvTuningLog[cvTuningLogHead] = rec;
  cvTuningLogHead = (cvTuningLogHead + 1) % 50;
  if (cvTuningLogCount < 50) cvTuningLogCount++;

  saveCVTuningLog();
  queueConsoleMessageF("CVTuningScore: run#%d score=%.2f settle=%.1fs overshoot=%.3fV n=%d",
    rec.runNumber, rec.score, rec.avgSettlingTimeSec, rec.worstOvershootV, (int)n);
  cvTuningScore = {};
}

static float computeLiveScore(int w) {
  float e = 0.0f, t = 0.0f;
  if (!liveScoreBuckets[w]) return 0.0f;
  for (int i = 0; i < LIVE_BUCKET_N; i++) {
    e += liveScoreBuckets[w][i].errorAccum;
    t += liveScoreBuckets[w][i].activeTimeSec;
  }
  return (t > 0.1f) ? (e / t) : 0.0f;
}

static void accumulateLiveScore(float e, float dtSec, uint32_t nowMs) {
  float contribution = e * e * dtSec;
  for (int w = 0; w < 4; w++) {
    if (!liveScoreBuckets[w]) continue;
    // Rotate bucket if the current one has expired
    if (liveBucketStartMs[w] == 0) liveBucketStartMs[w] = nowMs;
    if ((nowMs - liveBucketStartMs[w]) >= LIVE_BUCKET_MS[w]) {
      liveScoreHead[w] = (liveScoreHead[w] + 1) % LIVE_BUCKET_N;
      liveBucketStartMs[w] = nowMs;
      liveScoreBuckets[w][liveScoreHead[w]] = {0.0f, 0.0f};
    }
    liveScoreBuckets[w][liveScoreHead[w]].errorAccum    += contribution;
    liveScoreBuckets[w][liveScoreHead[w]].activeTimeSec += dtSec;
    liveScoreVal[w] = computeLiveScore(w);
  }
}

static float computeCVLiveScore(int w) {
  float e = 0.0f, t = 0.0f;
  if (!cvLiveScoreBuckets[w]) return 0.0f;
  for (int i = 0; i < LIVE_BUCKET_N; i++) {
    e += cvLiveScoreBuckets[w][i].errorAccum;
    t += cvLiveScoreBuckets[w][i].activeTimeSec;
  }
  return (t > 0.1f) ? (e / t) : 0.0f;
}

// Asymmetric ISE: overvoltage (filtV > target) weighted by cvKOvershoot; undervoltage by 1.0
static void accumulateCVLiveScore(float vErr, float dtSec, uint32_t nowMs) {
  float weight = (vErr > 0.0f) ? cvKOvershoot : 1.0f;
  float contribution = weight * vErr * vErr * dtSec;
  for (int w = 0; w < 4; w++) {
    if (!cvLiveScoreBuckets[w]) continue;
    if (cvLiveBucketStartMs[w] == 0) cvLiveBucketStartMs[w] = nowMs;
    if ((nowMs - cvLiveBucketStartMs[w]) >= LIVE_BUCKET_MS[w]) {
      cvLiveScoreHead[w] = (cvLiveScoreHead[w] + 1) % LIVE_BUCKET_N;
      cvLiveBucketStartMs[w] = nowMs;
      cvLiveScoreBuckets[w][cvLiveScoreHead[w]] = {0.0f, 0.0f};
    }
    cvLiveScoreBuckets[w][cvLiveScoreHead[w]].errorAccum    += contribution;
    cvLiveScoreBuckets[w][cvLiveScoreHead[w]].activeTimeSec += dtSec;
    cvLiveScoreVal[w] = computeCVLiveScore(w);
  }
}

// ===== THERMAL STEP TEST TUNING — save / load / commit / live score / wave tick =====

void saveThermalTuningLog() {
  if (!thermalTuningLog) return;
  File f = LittleFS.open("/thermaltuninglog.bin", "w");
  if (!f) return;
  f.write((uint8_t *)&thermalTuningLogCount,   sizeof(thermalTuningLogCount));
  f.write((uint8_t *)&thermalTuningLogHead,    sizeof(thermalTuningLogHead));
  f.write((uint8_t *)&thermalTuningRunCounter, sizeof(thermalTuningRunCounter));
  f.write((uint8_t *)thermalTuningLog, 50 * sizeof(ThermalTuningRecord));
  f.close();
}

void loadThermalTuningLog() {
  if (!thermalTuningLog) return;
  File f = LittleFS.open("/thermaltuninglog.bin", "r");
  if (!f) return;
  f.read((uint8_t *)&thermalTuningLogCount,   sizeof(thermalTuningLogCount));
  f.read((uint8_t *)&thermalTuningLogHead,    sizeof(thermalTuningLogHead));
  f.read((uint8_t *)&thermalTuningRunCounter, sizeof(thermalTuningRunCounter));
  f.read((uint8_t *)thermalTuningLog, 50 * sizeof(ThermalTuningRecord));
  f.close();
  Serial.printf("ThermalTuningLog: loaded %d records, counter=%d\n", thermalTuningLogCount, thermalTuningRunCounter);
}

void commitThermalTuningRecord() {
  if (!thermalTuningLog || thermalTuningScore.scoredStepCount < 1) {
    thermalTuningScore = {};
    return;
  }
  float n = (float)thermalTuningScore.scoredStepCount;
  float avgSettle = thermalTuningScore.totalSettlingTimeSec / n;
  float avgOver   = thermalTuningScore.totalIntOverFs / n;
  float avgUnder  = thermalTuningScore.totalIntUnderFs / n;

  ThermalTuningRecord rec = {};
  rec.runNumber          = ++thermalTuningRunCounter;
  rec.avgSettlingTimeSec = avgSettle;
  rec.avgIntOverFs       = avgOver;
  rec.avgIntUnderFs      = avgUnder;
  rec.worstOvershootF    = thermalTuningScore.worstOvAll;
  rec.scoredStepCount    = thermalTuningScore.scoredStepCount;
  rec.activeTimeSec      = thermalTuningScore.activeTimeSec;
  rec.score              = thermalKOvershoot * avgOver + thermalKUndershoot * avgUnder;  // settling time is informational only
  rec.kp                 = TempPIDKp;
  rec.ki                 = TempPIDKi;
  rec.lookaheadSec       = ThermalLookaheadSec;
  rec.filterAlpha        = TempPIDFilterAlpha;
  rec.intervalMs         = (uint16_t)TempPIDIntervalMs;
  rec.waveLowF           = thermalWaveLowF;
  rec.waveHighF          = thermalWaveHighF;
  rec.waveHalfPeriodMin  = thermalWaveHalfPeriodMin;
  rec.avgRPM    = (thermalTuningScore.avgSampleCount > 0) ? (thermalTuningScore.rpmSum / thermalTuningScore.avgSampleCount) : 0.0f;
  rec.avgAmbientF = (thermalTuningScore.avgSampleCount > 0) ? (thermalTuningScore.ambientSum / thermalTuningScore.avgSampleCount) : 0.0f;
  rec.riseRate  = ThermalPenaltyRiseRate;
  rec.fallRate  = ThermalPenaltyFallRate;

  thermalTuningLog[thermalTuningLogHead] = rec;
  thermalTuningLogHead = (thermalTuningLogHead + 1) % 50;
  if (thermalTuningLogCount < 50) thermalTuningLogCount++;

  saveThermalTuningLog();
  queueConsoleMessageF("ThermalTuning: run#%d score=%.2f settle=%.0fs overshoot=%.1f°F n=%d",
                       rec.runNumber, rec.score, rec.avgSettlingTimeSec, rec.worstOvershootF, rec.scoredStepCount);
  thermalTuningScore = {};
}

static float computeThermalLiveScore(int w) {
  float e = 0.0f, t = 0.0f;
  if (!thermalLiveScoreBuckets[w]) return 0.0f;
  for (int i = 0; i < LIVE_BUCKET_N; i++) {
    e += thermalLiveScoreBuckets[w][i].errorAccum;
    t += thermalLiveScoreBuckets[w][i].activeTimeSec;
  }
  return (t > 0.1f) ? (e / t) : 0.0f;
}

static void accumulateThermalLiveScore(float err, float dtSec, uint32_t nowMs) {
  // Asymmetric ISE: K_high × e² when above setpoint, K_low × e² when below
  float contribution = (err > 0.0f ? thermalKOvershoot : thermalKUndershoot) * err * err * dtSec;
  for (int w = 0; w < 4; w++) {
    if (!thermalLiveScoreBuckets[w]) continue;
    if (thermalLiveBucketStartMs[w] == 0) thermalLiveBucketStartMs[w] = nowMs;
    if ((nowMs - thermalLiveBucketStartMs[w]) >= THERMAL_LIVE_BUCKET_MS[w]) {
      thermalLiveScoreHead[w] = (thermalLiveScoreHead[w] + 1) % LIVE_BUCKET_N;
      thermalLiveBucketStartMs[w] = nowMs;
      thermalLiveScoreBuckets[w][thermalLiveScoreHead[w]] = {0.0f, 0.0f};
    }
    thermalLiveScoreBuckets[w][thermalLiveScoreHead[w]].errorAccum    += contribution;
    thermalLiveScoreBuckets[w][thermalLiveScoreHead[w]].activeTimeSec += dtSec;
    thermalLiveScoreVal[w] = computeThermalLiveScore(w);
  }
}

// Called from tempPID_tick() on every tick (16 Hz).
// Manages the thermal step-test wave generator, scores each step-up transition,
// and feeds the always-on asymmetric ISE live score (gated on penalty > threshold).
void thermalTuning_tick(uint32_t nowMs, float dtSec) {
  static bool lastThermalTuningMode = false;

  // Commit on mode turn-off
  if (lastThermalTuningMode && !ThermalTuningMode) {
    if (thermalTuningScore.scoredStepCount >= 1) commitThermalTuningRecord();
    else thermalTuningScore = {};
    thermalWaveCurrentSetpointF = 0.0f;
  }
  lastThermalTuningMode = (ThermalTuningMode != 0);

  // ===== Step-test wave generator =====
  if (ThermalTuningMode) {
    // Initialize test on first tick
    if (!thermalTuningScore.testStarted) {
      thermalWaveCurrentSetpointF        = thermalWaveLowF;
      thermalTuningScore.waveHigh        = false;
      thermalTuningScore.lowPhaseStable  = false;
      thermalTuningScore.lowConsecInBand = 0;
      thermalTuningScore.testStarted     = true;
      queueConsoleMessageF("ThermalTuning: started — low=%.0f°F high=%.0f°F halfPeriod=%.1fmin — waiting for LOW stability",
                           thermalWaveLowF, thermalWaveHighF, thermalWaveHalfPeriodMin);
    }

    // Commit on parameter change — reset to LOW phase and wait for stability again
    if (thermalTuningParamChanged) {
      if (thermalTuningScore.scoredStepCount >= 1) commitThermalTuningRecord();
      else thermalTuningScore = {};
      thermalTuningParamChanged          = false;
      thermalWaveCurrentSetpointF        = thermalWaveLowF;
      thermalTuningScore.waveHigh        = false;
      thermalTuningScore.lowPhaseStable  = false;
      thermalTuningScore.lowConsecInBand = 0;
    }

    uint32_t halfPeriodMs = (uint32_t)(thermalWaveHalfPeriodMin * 60000.0f);

    if (!thermalTuningScore.waveHigh) {
      // LOW phase: wait for temp to stabilize at thermalWaveLowF before scoring a step
      if (!isnan(tempFiltered) && fabsf(tempFiltered - thermalWaveLowF) <= thermalSettleThreshF) {
        if (++thermalTuningScore.lowConsecInBand >= thermalConsecutiveReads && !thermalTuningScore.lowPhaseStable) {
          thermalTuningScore.lowPhaseStable = true;
          queueConsoleMessageF("ThermalTuning: LOW stable at %.1f°F — stepping up to %.0f°F now",
                               tempFiltered, thermalWaveHighF);
          // Step up immediately once stable — scored HIGH phase starts now
          thermalTuningScore.waveHigh          = true;
          thermalTuningScore.lastToggleMs       = nowMs;
          thermalWaveCurrentSetpointF           = thermalWaveHighF;  // controller targets this - 5°F internally
          thermalTuningScore.phaseStartMs       = nowMs;
          thermalTuningScore.phaseSettled       = false;
          thermalTuningScore.phaseSettledMs     = 0;
          thermalTuningScore.consecutiveInBand  = 0;
          thermalTuningScore.intOverFs          = 0.0f;
          thermalTuningScore.intUnderFs         = 0.0f;
          thermalTuningScore.worstOvershootF    = 0.0f;
        }
      } else {
        thermalTuningScore.lowConsecInBand = 0;
      }
    } else {
      // HIGH phase: run for halfPeriodMs then step down
      if ((uint32_t)(nowMs - thermalTuningScore.lastToggleMs) >= halfPeriodMs) {
        // Finalize scored HIGH phase
        float settleTime = thermalTuningScore.phaseSettled
                           ? ((float)(thermalTuningScore.phaseSettledMs - thermalTuningScore.phaseStartMs) / 1000.0f)
                           : ((float)halfPeriodMs / 1000.0f);  // informational — not in score formula
        thermalTuningScore.totalSettlingTimeSec += settleTime;
        thermalTuningScore.totalIntOverFs       += thermalTuningScore.intOverFs;
        thermalTuningScore.totalIntUnderFs      += thermalTuningScore.intUnderFs;
        thermalTuningScore.worstOvAll = fmaxf(thermalTuningScore.worstOvAll, thermalTuningScore.worstOvershootF);
        thermalTuningScore.scoredStepCount++;
        thermalTuningScore.activeTimeSec += (float)halfPeriodMs / 1000.0f;
        queueConsoleMessageF("ThermalTuning: step DOWN — scored step #%d over=%.2f°F·s under=%.2f°F·s settle=%.0fs",
                             (int)thermalTuningScore.scoredStepCount,
                             thermalTuningScore.intOverFs, thermalTuningScore.intUnderFs, settleTime);
        // Step down and wait for LOW stability before next step
        thermalTuningScore.waveHigh        = false;
        thermalTuningScore.lastToggleMs    = nowMs;
        thermalTuningScore.lowPhaseStable  = false;
        thermalTuningScore.lowConsecInBand = 0;
        thermalWaveCurrentSetpointF        = thermalWaveLowF;
      }
    }

    // Per-tick scoring accumulation during HIGH phase
    // Error measured against thermalWaveHighF (user's declared limit, not the -5°F internal target).
    // Undershoot accumulates from the moment of step-up — no waiting for settle.
    if (thermalTuningScore.waveHigh && thermalTuningScore.phaseStartMs > 0) {
      float tempNow = isnan(tempFiltered) ? 0.0f : tempFiltered;
      float e = tempNow - thermalWaveHighF;

      if (e > 0.0f) {
        thermalTuningScore.intOverFs += e * dtSec;
        thermalTuningScore.worstOvershootF = fmaxf(thermalTuningScore.worstOvershootF, e);
      } else {
        // Full undershoot from step-up — rewards fast approach and no overshoot
        thermalTuningScore.intUnderFs += (-e) * dtSec;
      }

      // Settling check (informational only — does not gate scoring)
      if (fabsf(e) <= thermalSettleThreshF) {
        thermalTuningScore.consecutiveInBand++;
        if (thermalTuningScore.consecutiveInBand >= thermalConsecutiveReads && !thermalTuningScore.phaseSettled) {
          thermalTuningScore.phaseSettled   = true;
          thermalTuningScore.phaseSettledMs = nowMs;
          queueConsoleMessageF("ThermalTuning: SETTLED at %.1f°F in %.0fs",
                               tempNow, (float)(nowMs - thermalTuningScore.phaseStartMs) / 1000.0f);
        }
      } else {
        thermalTuningScore.consecutiveInBand = 0;
      }

      // Conditions snapshot
      thermalTuningScore.rpmSum     += RPM;
      thermalTuningScore.ambientSum += isnan(ambientTemp) ? 0.0f : ambientTemp;
      thermalTuningScore.avgSampleCount++;
    }
  }

  // ===== Always-on thermal live score =====
  // Fair gate: only score when thermal is the binding constraint with no other limiter active.
  //   voltageControlActive  — CV/absorption mode; 3-min blanking after it clears (sustained bias)
  //   g_fastOvClampActive   — OV supervisor cutting current for voltage, not thermal
  //   MaintainMode          — output forced to zero; thermal loop does nothing
  //   thermalPenaltyAmps    — must be > 2A; if near zero, RPM table or cool conditions are
  //                           the real ceiling, not thermal management
  //   g_I_cap               — RPM table ceiling must be > 10A; below that we're at near-idle
  //                           RPM and thermal management is irrelevant
  if (voltageControlActive) thermalScoreLastExternalMs = nowMs;
  bool liveScoreActive = tempPIDActive &&
                         thermalSlopeBufFull &&
                         !isnan(tempFiltered) &&
                         !ThermalTuningMode &&
                         !g_fastOvClampActive &&
                         (MaintainMode == 0) &&
                         thermalPenaltyAmps > 2.0f &&
                         g_I_cap > 10.0f &&
                         (uint32_t)(nowMs - thermalScoreLastExternalMs) > 180000UL;
  if (liveScoreActive) {
    float liveErr = tempFiltered - TemperatureLimitF;
    accumulateThermalLiveScore(liveErr, dtSec, nowMs);
  }
}

void thermalScore_init() {
  Serial.printf("ThermalTuning: %d record slots × %u bytes = %u bytes in PSRAM\n",
                50, (unsigned)sizeof(ThermalTuningRecord),
                (unsigned)(50 * sizeof(ThermalTuningRecord)));
  Serial.printf("ThermalTuning: live buckets = 4 windows × %d × %u bytes = %u bytes in PSRAM\n",
                (int)LIVE_BUCKET_N, (unsigned)sizeof(ScoreBucket),
                (unsigned)(4 * LIVE_BUCKET_N * sizeof(ScoreBucket)));
}

void AdjustFieldLearnMode() {

  // ========== TIMING ==========
  static uint32_t lastControlTickMs = 0;
  uint32_t currentMillis = millis();

  uint32_t actualDtMs = (lastControlTickMs == 0) ? 62 : (currentMillis - lastControlTickMs);
  if (actualDtMs > 500) actualDtMs = 500;
  float actualDtSec = (float)actualDtMs / 1000.0f;

  static float uTargetRaw_cached = 50.0f;   // always MaxTableValue (assigned from uTargetRaw); used only for supervisorLimiting gate
  float uTargetRaw = (float)MaxTableValue;  // always MaxTableValue; kept for uTargetRaw_cached lineage only
  float fastOvBaseCap = clamp_f(uTargetRaw_cached, 0.0f, (float)MaxTableValue);
  float fastOvCurrentCap = fastOvBaseCap;
  bool fastOvClampActive = false;
  static uint32_t ocTripStartMs = 0;

  updateCurrentRPMTableIndex(RPM);
  updateRPMBucketHistory(currentMillis);

  TickSnapshot tick = buildTickSnapshot(currentMillis, actualDtMs);
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

  if (cvLoopResetRequested) {
    cvLoopResetRequested = false;
    cv_I = 0.0f;
    lastVoltageLoopMs = 0;  // force voltage loop to fire on the very next tick
    // cv_I_aw_cap (static inside bumpless block) recovers to MaxTableValue at AwRecoverRate/s
    // automatically once fastOV is not active. No explicit reset needed.
    queueConsoleMessage("CV loop: integrator reset to zero");
  }

  // ========== PRE-GATE IMMEDIATE CUT CHECK ==========
  // Runs before the CH1 gate so INA overvoltage always cuts regardless of sensor freshness.
  FieldEventReason preReason = selectFieldEventReason(tick);
  updateProtectionCounters(preReason);
  if (shouldImmediatelyCutGPIO4(preReason) && !gpio4IsLow) {
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
    g_fastOvVpred = IBV;

    if (ibvFreshFlag) {
      ibvFreshFlag = false;
      if (vPrevMs > 0) {
        float dtV = (currentMillis - vPrevMs) / 1000.0f;
        if (dtV > 0.001f && dtV < 0.1f) {
          float raw = (IBV - vPrev) / dtV;
          dvdt = 0.08 * raw + 0.92 * dvdt;
        }
      }
      vPrev = IBV;
      vPrevMs = currentMillis;
    }
    g_fastOvDvdt = dvdt;

    if (voltageControlActive) {
      const float TD_PRED = 0.08f;
      const float V_SOFT = ChargingVoltageTarget + 0.08f;
      const float V_HARD = ChargingVoltageTarget + 0.15f;
      const float PRED_GUARD = 0.06f;

      float Vpred = IBV + TD_PRED * fmaxf(0.0f, dvdt);
      g_fastOvVpred = Vpred;

      if (!ovActive) {
        preEventIcv = Icv;
      }

      if (IBV > ChargingVoltageTarget - PRED_GUARD) {
        if (Vpred > V_SOFT) {
          // H1: baseline anchored to setpointLimited (actual operating point) not fastOvBaseCap
          // (theoretical ceiling). Cap is now binding and proportional to overshoot magnitude.
          float softCap = fmaxf(0.0f, setpointLimited - KSoft * (Vpred - V_SOFT));
          fastOvCurrentCap = fminf(fastOvCurrentCap, softCap);
          fastOvClampActive = true;
          ovActive = true;
          g_fastOvSoftActive = true;
        }
        if (Vpred > V_HARD) {
          float hardCap = fmaxf(0.0f, setpointLimited - KHard * (Vpred - V_HARD));
          fastOvCurrentCap = fminf(fastOvCurrentCap, hardCap);
          fastOvClampActive = true;
          g_fastOvHardActive = true;
        }
      }

      const float HARD_CLAMP_HYST = 0.08f;
      if (IBV > ChargingVoltageTarget + HARD_CLAMP_HYST) {
        float ovExcess = IBV - (ChargingVoltageTarget + HARD_CLAMP_HYST);
        float hystCap = fmaxf(0.0f, setpointLimited - KHard * ovExcess);
        fastOvCurrentCap = fminf(fastOvCurrentCap, hystCap);
        fastOvClampActive = true;
        ovActive = true;
        g_fastOvHardActive = true;
      }

      // Recovery seed: fires once when clamp de-asserts.
      if (ovActive
          && (IBV <= ChargingVoltageTarget)
          && (Vpred <= V_SOFT)) {

        float e = ChargingVoltageTarget - IBV;
        float icvHi = clamp_f(uTargetRaw_cached, 0.0f, (float)MaxTableValue);

        // cv_I = clamp_f(preEventIcv - VoltageKp * e, 0.0f, icvHi);  // likely to remove permanently
        // cv_I here is already bled down by AwBleed during the fastOV period.
        // That makes this seed conservative (below pre-event level), which is intentional —
        // current rebuilds via the bumpless tracker rather than jumping straight back.
        // If recovery looks sluggish, tune AwRecoverRate rather than changing this seed.
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
  // WARNING: BYPASSES ALL SAFETY SYSTEMS EXCEPT INA228 HARDCODED
  if (LimpHome == 1) {
    handleLimpHome(currentMillis, tick);
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

  // ========== OVERRIDE MODE ENTRY/EXIT DETECTION ==========
  // Edge detection so one-shot actions fire exactly once on each transition.
  static bool lastMaintainMode = false;
  static bool lastTargetVoltageMode = false;
  bool enteringMaintainMode      = (MaintainMode == 1)      && !lastMaintainMode;
  bool exitingMaintainMode       = (MaintainMode == 0)      &&  lastMaintainMode;
  bool enteringTargetVoltageMode = (TargetVoltageMode == 1) && !lastTargetVoltageMode;
  bool exitingTargetVoltageMode  = (TargetVoltageMode == 0) &&  lastTargetVoltageMode;
  lastMaintainMode = (MaintainMode == 1);
  lastTargetVoltageMode = (TargetVoltageMode == 1);

  // ========== CHARGING STAGE (bulk/absorption/float) ==========
  // Suppressed while either override is active — letting it run would fire
  // spurious re-bulk transitions and absorption timeouts.
  // Stage tracking only meaningful in AUTO. In MANUAL, voltageControlActive=false
  // and no CV loop runs, so stage state is meaningless.
  if (MaintainMode != 1 && TargetVoltageMode != 1 && !tick.manualMode) {
    updateChargingStage();
  }

  // On override exit: call enter_sys_auto() to reset stage to BULK rather than
  // resuming pre-override stage state that may be stale. updateChargingStage()
  // will fast-forward to the correct stage on the first resumed tick if the
  // battery is already charged.
  if ((exitingMaintainMode || exitingTargetVoltageMode) && sysMode == SYS_MODE_AUTO) {
    enter_sys_auto();
    if (exitingMaintainMode)       queueConsoleMessage("MaintainMode exit: resuming charge from bulk");
    if (exitingTargetVoltageMode)  queueConsoleMessage("TargetVoltageMode exit: resuming charge from bulk");
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
    static uint32_t lastNoReasonWarnMs = 0;
    if ((uint32_t)(currentMillis - lastNoReasonWarnMs) >= 5000) {
      lastNoReasonWarnMs = currentMillis;
      Serial.println("ERROR: Shutdown mode with no reason specified");
    }
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

  // CV overshoot: bypass duty slew so the output current PID can reduce field current without
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
  // earlier at +0.05–0.09 V over target — the window where the output current PID was being
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
      } else if (IBV < ChargingVoltageTarget + 0.02f) {
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
  bool isNormalMode = (mode == MODE_NORMAL_AUTO_PID || mode == MODE_NORMAL_MANUAL) && !shouldImmediatelyCutGPIO4(reason);
  bool wasNormalMode = (prevMode == MODE_NORMAL_AUTO_PID || prevMode == MODE_NORMAL_MANUAL);
  bool enteringNormal = !wasNormalMode && isNormalMode;
  bool exitingNormal = wasNormalMode && !isNormalMode;

  SystemMode newSysMode;
  if (!tick.chargingEnabled) newSysMode = SYS_MODE_OFF;
  else if (shouldImmediatelyCutGPIO4(reason)) newSysMode = SYS_MODE_FAULT;
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
        tuningScore        = {};   // discard partial data — cannot score across a non-AUTO gap
        tuningParamChanged = false;
        pidInitialized = true;
        if (enteringNormal) {
          shutdownPhase = SHUTDOWN_PHASE_NONE;
          shutdownPhaseEntryMs = 0;
          shutdownPhase2EntryMs = 0;
          settledAtZeroDutyMs = 0;
          warmupCeiling = 0.0f;
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
  // ========== NON-NORMAL MODE: SHUTDOWN / FAULT HANDLING ==========
  // NOTE: pidLog_tick() is NOT called in the shutdown/fault path.
  if (!isNormalMode) {
    runShutdownPath(tick, mode, reason, actualDtSec, exitingNormal);
    prevMode = mode;
    return;
  }

  // ========== NORMAL MODES (MANUAL or AUTO) ==========
  // Re-check before enabling GPIO4 — fault could have arrived this tick.
  if (shouldImmediatelyCutGPIO4(reason) && !gpio4IsLow) {
    applyImmediateCut(tick, reason);
    return;
  }
  // Hold field off while Core 0 internet operation or NTP sync is in progress.
  // Field was off for ≥60s before any internet activity fires, so this only
  // prevents the field from turning back on mid-transaction.
  if (core0Busy) return;
  digitalWrite(4, HIGH);
  gpio4IsLow = false;
  // GPIO38 driven solely by CheckAlarms — do not write here

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

        // Commit on parameter change if enough scored cycles have accumulated
        if (tuningParamChanged) {
          if (tuningScore.scoredToggleCount >= 4 && tuningScore.activeTimeSec > 0.5f) {
            commitTuningRecord();  // resets tuningScore internally
          } else {
            tuningScore = {};  // discard partial data, re-ring-in with new params
          }
          tuningParamChanged = false;
        }

        uint32_t halfPeriodMs = ((uint32_t)wavePeriod * 1000) / 2;
        if (tick.nowMs - lastTuningWaveToggle >= halfPeriodMs) {
          tuningWaveHigh = !tuningWaveHigh;
          lastTuningWaveToggle = tick.nowMs;

          tuningScore.toggleCount++;
          if (tuningScore.toggleCount > 4) {
            tuningScore.ringInDone = true;  // 2 full ring-in cycles (4 half-periods) complete
          }
          if (tuningScore.ringInDone) {
            tuningScore.pendingWindowOpen = true;  // open window once slew settles, not at toggle time
            tuningScore.inScoringWindow   = false;
            tuningScore.scoredToggleCount++;
          }
        }

        // Close scoring window 5s after it opened (lastToggleMs is set when window opens, not at toggle)
        if (tuningScore.inScoringWindow && (tick.nowMs - tuningScore.lastToggleMs > 5000)) {
          tuningScore.inScoringWindow = false;
        }

        uTargetAmps = tuningWaveHigh ? (5 + waveAmplitude) : 5;
        setpointCommand = (float)uTargetAmps;
        liveScore_thisCmd = setpointCommand;

        setpointLimited = slew_limit_f(setpointLimited, setpointCommand,
                                       SetpointRiseRate, SetpointFallRate, actualDtSec);

        // Open scoring window once slew has settled (rate < 1 A/s) — fair regardless of SetpointRiseRate.
        // lastToggleMs is set here so the 5s timeout starts from when scoring actually begins.
        {
          static float tuning_prevSlewed = 0.0f;
          static bool  tuning_slewInit   = false;
          float tuning_slewRate = 0.0f;
          if (tuning_slewInit) {
            tuning_slewRate = fabsf(setpointLimited - tuning_prevSlewed) / actualDtSec;
          }
          tuning_prevSlewed = setpointLimited;
          tuning_slewInit   = true;
          if (tuningScore.pendingWindowOpen && tuning_slewRate < 1.0f) {
            tuningScore.inScoringWindow   = true;
            tuningScore.pendingWindowOpen = false;
            tuningScore.lastToggleMs      = tick.nowMs;
          }
        }

        voltageControlActive = false;

        targetCurrent = getFiltI();
        pidInput = (double)targetCurrent;
        pidSetpoint = (double)setpointLimited;
        pidError = setpointLimited - targetCurrent;
        currentPID.Compute();

        // Accumulate test score while inside a scoring window
        if (tuningScore.inScoringWindow) {
          float e = pidError;
          tuningScore.errorAccum    += e * e * actualDtSec;
          tuningScore.activeTimeSec += actualDtSec;
          if (fabsf(e) > tuningScore.worstErrorA) tuningScore.worstErrorA = fabsf(e);
          tuningScore.rpmSum  += RPM_filtered;
          float tempSample = isnan(AlternatorTemperatureF) ? TempToUse : AlternatorTemperatureF;
          if (!isnan(tempSample)) tuningScore.tempSum += tempSample;
          tuningScore.avgSampleCount++;
          if (tuningScore.activeTimeSec > 0.0f) {
            tuningScore.score = tuningScore.errorAccum / tuningScore.activeTimeSec;
          }
        }

        // TrackAppliedOutput() is NOT needed here — falls through to the shared
        // call at the end of the normal-mode section.

        lastTuningMode = true;

      } else {
        // ===== NORMAL AUTO =====

        // Detect TuningMode exit — fires exactly once.
        if (lastTuningMode) {
          tempPIDActive = false;
          tempFilterNeedsReseed = true;
          // Commit tuning score if enough cycles were scored (≥2 cycles = ≥4 scored toggles)
          if (tuningScore.scoredToggleCount >= 4 && tuningScore.activeTimeSec > 0.5f) {
            commitTuningRecord();
          } else {
            tuningScore = {};  // discard partial — not enough data
          }
        }
        lastTuningMode = false;

        // Temperature loop PID. Library timer governs Compute() cadence.
        tempPID_tick(currentMillis, actualDtSec);

        // Thermal log runs after tempPID_tick() so outerAntiWindupFired and
        // outerTermI reflect this tick's state, not the previous tick's.
        thermalLog_tick(currentMillis);

        // --- Command architecture ---
        //
        //   I_cap        RPM-dependent mechanical/electrical ceiling (table lookup).
        //   thermalPenalty  Temperature loop PID output. Derates I_cap when hot;
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
        //   2. Apply user overrides (MaintainMode). HiLow mode is handled at
        //      table-load time via loadCapTablesForMode() — no runtime halving.
        //   In CV modes: position-form PID (P+I+D) produces Icv; setpointCommand = Icv.
        //      D term = VoltageKd * dvdt, subtracted to preemptively reduce current as voltage rises.
        //      Integrator anti-windup: upward integration frozen when P+I saturates at uTargetAmps ceiling.
        //   In idle/MaintainMode: setpointCommand = uTargetAmps directly.

        float I_cap;
        if (capLimitMode == 1 && tick.currentBatteryVoltage > 0.5f) {
          I_cap = interpolateRPMTable(RPM, rpmCapPowerTable) / tick.currentBatteryVoltage;
        } else {
          I_cap = getCapCurrentForRPM(RPM);
        }

        g_I_cap = I_cap;  // expose RPM table ceiling globally for thermal scoring gate
        float I_cmd = I_cap - thermalPenaltyAmps;
        I_cmd = fminf(I_cmd, (float)MaxTableValue);
        I_cmd = fmaxf(I_cmd, 0.0f);

        uTargetAmps = I_cmd;

        // Warmup ramp: advance ceiling each tick, apply as cap on uTargetAmps
        if (WarmupRampRate > 0.0f) {
          warmupCeiling = fminf(warmupCeiling + WarmupRampRate * actualDtSec, (float)MaxTableValue);
          uTargetAmps = fminf(uTargetAmps, warmupCeiling);
        }

        // User overrides
        if (MaintainMode == 1) uTargetAmps = 0;

        // Actual RPM/thermal/override ceiling — the true pre-OV current limit for logging.
        // uTargetRaw remains MaxTableValue and is not used for telemetry.
        float i_ceiling_pre_ov = (float)uTargetAmps;

        // Hoisted here so iExcess block can reset it on event onset.
        static float cv_I_aw_cap = 100.0f;

        // ── iExcess supervisor ─────────────────────────────────────────
        // IExcessK and IExcessN are user-adjustable globals (LittleFS-persisted).
        // N consecutive ticks above threshold required before response fires —
        // separates brief resonance peaks (~3-4 ticks) from sustained RPM-step excess.
        {
          const float IEXCESS_GATE = 0.10f;
          const float IEXCESS_HYST = 2.0f;
          const float K_IE = 1.0f;

          static bool iExcessActive = false;
          static int iExcessPersistCount = 0;
          static float preEventCvI = 0.0f;  // cv_I captured just before snap, used to seed recovery

          if (voltageControlActive && (IBV > ChargingVoltageTarget - IEXCESS_GATE)) {
            float excess = g_iMA2 - setpointLimited - IExcessK;  // setpointLimited = previous tick — acceptable
            bool aboveThreshold = (excess > 0.0f);
            bool belowHysteresis = (g_iMA2 < setpointLimited + IExcessK - IEXCESS_HYST);

            // Post-fastOV mismatch gate: block iExcess counting while g_iMA2 is still
            // elevated from the fastOV event itself. When fastOV fires, setpointLimited
            // instantly collapses (1e9 A/s fall rate) but field inductance keeps g_iMA2
            // high for ~1 field TC. That mismatch looks identical to a real iExcess event
            // but is entirely caused by fastOV's own action — firing here cascades into
            // a spurious cv_I snap and a full oscillation cycle.
            //
            // Arm on fastOV rising edge. Release when g_iMA2 has actually fallen back to
            // within IExcessK of setpointLimited — i.e., when the mismatch is physically
            // gone. This is installation-agnostic: slow-TC alternators hold the gate
            // longer naturally; fast-TC alternators release sooner. If AwBleed permanently
            // lowers the operating point, the gate still releases once g_iMA2 matches the
            // new level — a subsequent real RPM step is then correctly detected.
            static bool postFastOvMismatch = false;
            static bool prevFastOvActive_ie = false;
            if (fastOvClampActive && !prevFastOvActive_ie) {
              postFastOvMismatch = true;  // rising edge: arm the gate
            }
            prevFastOvActive_ie = fastOvClampActive;
            if (postFastOvMismatch && !fastOvClampActive
                && (g_iMA2 <= setpointLimited + IExcessK)) {
              postFastOvMismatch = false;  // mismatch gone — release gate
            }

            // Only count persistence when fastOV is NOT already active and the
            // post-fastOV mismatch has cleared — iExcess is a pre-voltage detector;
            // it must not fire on conditions created by fastOV's own action.
            if (aboveThreshold && !fastOvClampActive && !postFastOvMismatch) {
              iExcessPersistCount++;
            } else {
              iExcessPersistCount = 0;
            }

            if (iExcessPersistCount >= IExcessN) {
              if (iExcessPersistCount == IExcessN) {
                preEventCvI = cv_I;         // rising edge — capture before any snap or bleed
                cv_I_aw_cap = cv_I;         // cap the bumpless tracker ceiling to pre-event level — prevents current-limited rewind
                postFastOvMismatch = true;  // iExcess collapses setpointLimited the same way fastOV does; block re-trigger during field TC wind-down
                g_iExcessCount++;
              }
              float ieCap = fmaxf(0.0f, fastOvBaseCap - K_IE * excess);
              fastOvCurrentCap = fminf(fastOvCurrentCap, ieCap);
              fastOvClampActive = true;
              iExcessActive = true;
              // IExcessKBleed = 0: snap to zero (maximum response — field falls at 100ms physics TC).
              // IExcessKBleed > 0: proportional bleed at K_bleed × excess A/s each 5ms tick.
              // Both modes drive output current loop to minimum duty within one tick; difference is recovery depth.
              // Recovery is seeded with preEventCvI on release regardless of mode.
              if (IExcessKBleed <= 0.0f) {
                cv_I = 0.0f;
              } else {
                cv_I = fmaxf(0.0f, cv_I - IExcessKBleed * excess * actualDtSec);
              }
            } else if (iExcessActive && !belowHysteresis) {
              fastOvClampActive = true;  // hold govBypass during hysteresis
            } else {
              if (iExcessActive) {
                // Seed recovery to IExcessReseedFrac of the pre-event level so the PI
                // rebuilds from below target rather than bouncing straight back through
                // the setpoint and re-triggering.
                cv_I = preEventCvI * IExcessReseedFrac;
              }
              iExcessActive = false;
            }
          } else {
            // Gate closed (battV dropped below targV - IEXCESS_GATE) — still seed recovery
            // if an event was active, otherwise cv_I stays at zero and the trap persists.
            if (iExcessActive) {
              cv_I = preEventCvI * IExcessReseedFrac;
            }
            iExcessPersistCount = 0;
            iExcessActive = false;
          }
          g_iExcessActive = iExcessActive;
          g_iExcessDutyCap = 100.0f;  // retired
        }

        // ── Load dump detection — dBcur/dt positive spike in CV mode ─────────────────
        // Positive g_dBcur_dt means battery is suddenly absorbing more current (loads dropped).
        // Voltage will rise; act before it crosses the fastOV threshold.
        // Gate: CV mode only, fast INA228 reads active (5ms cadence).
        // No risk of false trigger on commanded reductions — those cause dBcur_dt ≤ 0.
        if (voltageControlActive && inaFastModeActive) {
          static bool ldWasActive = false;
          bool ldNow = (g_dBcur_dt > LoadDumpDtThresh);
          if (ldNow) {
            float ldCap = fmaxf(0.0f, setpointLimited - LoadDumpCurrentDrop);
            fastOvCurrentCap = fminf(fastOvCurrentCap, ldCap);
            if (!ldWasActive) g_loadDumpCount++;
          }
          g_loadDumpActive = ldNow;
          ldWasActive = ldNow;
        } else {
          g_loadDumpActive = false;
        }

        // ── Apply fastOvCurrentCap to uTargetAmps (fastOV + iExcess + load dump) ──
        uTargetAmps = fminf((float)uTargetAmps, fastOvCurrentCap);

        // TargetVoltageMode: run CV at a user-specified voltage target.
        // Forces float-equivalent stage flags so voltageControlActive goes true
        // below, then overrides ChargingVoltageTarget with the user value.
        // All current limits (RPM cap, thermal penalty, MaxTableValue, user
        // overrides) remain fully active — this only changes the voltage target.
        // cv_I is seeded by bumpless transfer tracking on CV entry — no explicit reseed needed.
        if (TargetVoltageMode == 1) {
          // Do NOT touch inBulkStage / inAbsorptionStage here. updateChargingStage() is
          // suppressed while TVMode=1, so those flags are irrelevant during execution.
          // Mutating them caused all three stage flags to be false on TVMode exit, which
          // fell through to the FLOAT branch on the first updateChargingStage() call back.
          ChargingVoltageTarget = TargetVoltageSetpoint;
          if (enteringTargetVoltageMode) {
            pidLog_enteringTargetVoltageMode = 1;
            queueConsoleMessageF("TargetVoltageMode: active, target=%.2fV", TargetVoltageSetpoint);
          }
        }

        // voltageControlActive: true in absorption, float, and TargetVoltageMode.
        // False in idle and MaintainMode. PID runs continuously while true.
        voltageControlActive = !inIdleStage;
        if (TargetVoltageMode == 1) voltageControlActive = true;  // force CV active even from idle
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

        // ===== CV TUNING MODE: voltage square-wave generator =====
        // Dithers ChargingVoltageTarget between base (HIGH) and base−amp (LOW) so the
        // CV loop step response (settling time, overshoot) can be measured and scored.
        // Only runs in the NORMAL AUTO path — incompatible with inner-loop TuningMode.
        {
          static bool lastCVTuningMode = false;

          // Commit on CVTuningMode turn-off
          if (lastCVTuningMode && !CVTuningMode) {
            if (cvTuningScore.scoredHighCount >= 1) commitCVTuningRecord();
            else cvTuningScore = {};
          }
          lastCVTuningMode = (CVTuningMode != 0);

          if (CVTuningMode && voltageControlActive) {
            // Capture base target and initial conditions once per test
            if (!cvTuningScore.testStarted) {
              cvBaseTarget                = ChargingVoltageTarget;
              cvTuningScore.battVAtStart  = IBV;
              cvTuningScore.socAtStart    = (float)SOC_percent / 100.0f;
              cvTuningScore.lastToggleMs  = currentMillis;
              cvTuningScore.waveHigh      = true;   // start in HIGH phase (at target)
              cvTuningScore.phaseStartMs  = currentMillis;
              cvTuningScore.testStarted   = true;
            }

            // Commit on parameter change if enough cycles have scored
            if (cvTuningParamChanged) {
              if (cvTuningScore.scoredHighCount >= 1) commitCVTuningRecord();
              else cvTuningScore = {};
              cvTuningParamChanged = false;
            }

            // Half-period toggle — cvWavePeriodSec is the full period, so each half is / 2
            uint32_t halfPeriodMs = (uint32_t)cvWavePeriodSec * 500UL;
            if (currentMillis - cvTuningScore.lastToggleMs >= halfPeriodMs) {
              bool goingHigh = !cvTuningScore.waveHigh;
              cvTuningScore.waveHigh     = goingHigh;
              cvTuningScore.lastToggleMs = currentMillis;
              cvTuningScore.halfPeriodCount++;
              // 1 half-period ring-in: initial HIGH is the only unscored phase.
              // ringInDone becomes true after the first toggle (end of initial HIGH).
              // Guard on phaseStartMs > 0 ensures the initial HIGH→LOW transition
              // doesn't spuriously finalize a never-started scored HIGH phase.
              if (cvTuningScore.halfPeriodCount >= 1) cvTuningScore.ringInDone = true;

              if (goingHigh && cvTuningScore.ringInDone && cvTuningScore.phaseStartMs > 0) {
                // Finalize scored LOW phase (step-down response)
                if (!cvTuningScore.lowPhaseSettled) {
                  cvTuningScore.totalLowSettlingTimeSec += (float)cvWavePeriodSec / 2.0f;
                }
                cvTuningScore.scoredLowCount++;
                // (totalLowIntOvVs accumulated tick-by-tick in LOW phase block below)
              }
              if (goingHigh && cvTuningScore.ringInDone) {
                // Start of a new scored HIGH phase
                cvBaseTarget                    = ChargingVoltageTarget;  // refresh real target
                cvTuningScore.phaseStartMs      = currentMillis;
                cvTuningScore.phaseSettled      = false;
                cvTuningScore.consecutiveInBand = 0;
                cvTuningScore.fastOvSnap        = g_fastOvClampCount;
                cvTuningScore.iExcessSnap       = g_iExcessCount;
                cvTuningScore.loadDumpSnap      = g_loadDumpCount;
                cvTuningScore.hardOcSnap        = g_hardOCCount;
              }
              if (!goingHigh && cvTuningScore.ringInDone && cvTuningScore.phaseStartMs > 0) {
                // End of a scored HIGH phase — finalize settling time
                if (!cvTuningScore.phaseSettled) {
                  cvTuningScore.totalSettlingTimeSec += (float)cvWavePeriodSec / 2.0f;  // half-period penalty
                }
                cvTuningScore.scoredHighCount++;
                cvTuningScore.fastOvFires   += (uint16_t)(g_fastOvClampCount - cvTuningScore.fastOvSnap);
                cvTuningScore.iExcessFires  += (uint16_t)(g_iExcessCount     - cvTuningScore.iExcessSnap);
                cvTuningScore.loadDumpFires += (uint16_t)(g_loadDumpCount    - cvTuningScore.loadDumpSnap);
                cvTuningScore.hardOcFires   += (uint16_t)(g_hardOCCount      - cvTuningScore.hardOcSnap);
                // Start of scored LOW phase (step-down response)
                cvTuningScore.lowPhaseStartMs = currentMillis;
                cvTuningScore.lowPhaseSettled = false;
                cvTuningScore.lowConsecInBand = 0;
                cvTuningScore.lowFastOvSnap   = g_fastOvClampCount;
                cvTuningScore.lowIExSnap      = g_iExcessCount;
                cvTuningScore.lowLdSnap       = g_loadDumpCount;
                cvTuningScore.lowHocSnap      = g_hardOCCount;
              }
            }

            // Override ChargingVoltageTarget for this tick
            ChargingVoltageTarget = cvTuningScore.waveHigh ? cvBaseTarget
                                                           : (cvBaseTarget - cvWaveAmplitudeV);
          }
        }

        // Voltage target rise governor.
        // Now only active in the final CV_ENGAGE_MARGIN window before target (vGap <= 0.15V).
        // During bulk approach, current-limited state commands uTargetAmps directly and this governor
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
        // Bumpless transfer: track cv_I toward the operating-point value when CV is inactive.
        // While CV is active, cv_I_track stays in sync for seamless re-entry.
        {
          static float cv_I_track = 0.0f;
          static uint32_t awSeedProtectStartMs = 0; // timestamp when last bumpless seed fired

          // ── CV entry bumpless seed ─────────────────────────────────────────────────
          // Seeds cv_I at voltageControlActive entry so that Icv = Kp*e + cv_I ≈ getFiltI(),
          // preventing a step change in setpointLimited. Resets cv_I_aw_cap so a stale
          // post-event cap does not constrain the CV loop on entry.
          //   seed = getFiltI() - Kp*e  →  Icv = getFiltI()  →  no step in setpointLimited.
          if (voltageControlActive && enteringCV) {
            float e_cv = ChargingVoltageTarget - getFiltV();
            float seed = clamp_f(getFiltI() - VoltageKp * e_cv, 0.0f, (float)uTargetAmps);
            cv_I = seed;
            cv_I_track = seed;
            cv_I_aw_cap = (float)MaxTableValue;  // clear AW cap — stale values constrain CV entry
            awSeedProtectStartMs = currentMillis; // start seed-protection window
          }
          bool seedProtected = (AwSeedProtectMs > 0) && ((currentMillis - awSeedProtectStartMs) < (uint32_t)AwSeedProtectMs);

          // Anti-windup ceiling: bleeds down while fastOV is active so the bumpless
          // tracker cannot immediately re-wind cv_I to the bulk-charging level after
          // each overshoot. Recovers gradually after fastOV clears, giving the battery
          // time to settle before full current ramps back up.
          // cv_I_aw_cap is declared above the iExcess block so both blocks share it.
          // AwBleedRate and AwRecoverRate are user-adjustable globals (LittleFS-persisted).
          // Scale bleed/recover rates by MaxTableValue so a fixed fraction-per-second
          // applies the same proportional aggression regardless of alternator size.
          // AwBleedRate=2.0 at 50A table → 100 A/s; at 150A table → 300 A/s.
          // AwRecoverRate=0.1 at 50A table → 5 A/s; at 150A table → 15 A/s.
          float awBleedAmpS = AwBleedRate * (float)MaxTableValue;
          float awRecoverAmpS = AwRecoverRate * (float)MaxTableValue;

          if (fastOvClampActive && voltageControlActive) {
            cv_I_aw_cap = fmaxf(0.0f, cv_I_aw_cap - awBleedAmpS * actualDtSec);
          } else {
            cv_I_aw_cap = fminf(cv_I_aw_cap + awRecoverAmpS * actualDtSec, (float)MaxTableValue);
          }

          float icvHi_bt = fminf(clamp_f((float)uTargetAmps, 0.0f, (float)MaxTableValue), cv_I_aw_cap);
          if (!voltageControlActive) {
            if (!seedProtected) {
              float e_bt = ChargingVoltageTarget - getFiltV();  // filtered — control path
              float cv_I_target = clamp_f(getFiltI() - VoltageKp * e_bt, 0.0f, icvHi_bt);
              const float Kt = 2.0f;
              cv_I_track += Kt * (cv_I_target - cv_I_track) * actualDtSec;
              cv_I_track = clamp_f(cv_I_track, 0.0f, icvHi_bt);
              cv_I = cv_I_track;
            } else {
              cv_I_track = cv_I;  // keep tracker in sync during seed-protection window
            }
          } else {
            cv_I_track = cv_I;
            // Per-tick cv_I bleed during active fastOV: voltage PI fires at 100ms but
            // the integrator needs to reduce every 5ms tick to counteract bulk-phase wind-up.
            // Suppressed during seed-protection window to preserve the seeded cv_I value.
            if (fastOvClampActive && !seedProtected) {
              cv_I = fmaxf(0.0f, cv_I - awBleedAmpS * actualDtSec);
              cv_I_track = cv_I;
            }
          }
        }

        if (voltageControlActive) {
          bool cvLoopFired = enteringCV || ((currentMillis - lastVoltageLoopMs) >= VoltageLoopInterval);

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

            if (!enteringCV) {
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

              // D term: subtract Kd × dvdt so rising voltage preemptively reduces current setpoint.
              Icv = clamp_f(VoltageKp * e + cv_I - VoltageKd * g_fastOvDvdt, icvLo, icvHi);
            }
          }

          pidLog_uTargetBeforeVoltCap = i_ceiling_pre_ov;
          pidLog_uTargetAfterVoltCap = Icv;

        } else {
          pidLog_uTargetBeforeVoltCap = i_ceiling_pre_ov;
          pidLog_uTargetAfterVoltCap = (float)uTargetAmps;
        }

        // Per-tick Icv recompute — proportional path responds every output current loop tick;
        // cv_I still updates only on VoltageLoopInterval cadence.
        {
          float e_now = voltageTargetSlewed - getFiltV();  // filtered — control path
          float icvHi_tick = clamp_f((float)uTargetAmps, 0.0f, (float)MaxTableValue);
          if (!enteringCV) {
            Icv = clamp_f(VoltageKp * e_now + cv_I - VoltageKd * g_fastOvDvdt, 0.0f, icvHi_tick);
          }
        }

        // ===== CV TUNING SCORE ACCUMULATION =====
        // Scores settling time and overshoot during each HIGH phase after ring-in.
        if (CVTuningMode && voltageControlActive && cvTuningScore.waveHigh && cvTuningScore.ringInDone) {
          float overshoot = fmaxf(0.0f, IBV - cvBaseTarget);
          cvTuningScore.totalIntegratedOvershootVs += overshoot * actualDtSec;
          if (overshoot > cvTuningScore.worstOvershootV) cvTuningScore.worstOvershootV = overshoot;

          if (!cvTuningScore.phaseSettled) {
            float vErr = fabsf(getFiltV() - cvBaseTarget);
            if (vErr <= CV_SETTLE_V_THRESH) {
              if (++cvTuningScore.consecutiveInBand >= cvConsecutiveReads) {
                cvTuningScore.phaseSettled = true;
                cvTuningScore.totalSettlingTimeSec +=
                  (float)(currentMillis - cvTuningScore.phaseStartMs) / 1000.0f;
              }
            } else {
              cvTuningScore.consecutiveInBand = 0;
            }
          }

          cvTuningScore.activeTimeSec += actualDtSec;
          cvTuningScore.rpmSum        += RPM_filtered;
          float tempSample = isnan(AlternatorTemperatureF) ? TempToUse : AlternatorTemperatureF;
          if (!isnan(tempSample)) cvTuningScore.tempSum += tempSample;
          cvTuningScore.avgSampleCount++;
        }

        // ===== CV TUNING SCORE ACCUMULATION — LOW phase (step-down response) =====
        if (CVTuningMode && voltageControlActive && !cvTuningScore.waveHigh && cvTuningScore.ringInDone) {
          float lowTarget = cvBaseTarget - cvWaveAmplitudeV;
          float ov = fmaxf(0.0f, getFiltV() - lowTarget);  // above lowTarget = still too high
          cvTuningScore.totalLowIntOvVs += ov * actualDtSec;
          if (ov > cvTuningScore.worstLowOvV) cvTuningScore.worstLowOvV = ov;

          if (!cvTuningScore.lowPhaseSettled) {
            float vErr = fabsf(getFiltV() - lowTarget);
            if (vErr <= CV_SETTLE_V_THRESH) {
              if (++cvTuningScore.lowConsecInBand >= cvConsecutiveReads) {
                cvTuningScore.lowPhaseSettled = true;
                cvTuningScore.totalLowSettlingTimeSec +=
                  (float)(currentMillis - cvTuningScore.lowPhaseStartMs) / 1000.0f;
              }
            } else {
              cvTuningScore.lowConsecInBand = 0;
            }
          }
        }

        setpointCommand = voltageControlActive ? Icv : (float)uTargetAmps;
        liveScore_thisCmd = setpointCommand;

        float effectiveFallRate = fastOvClampActive ? 1.0e9f : SetpointFallRate;
        setpointLimited = slew_limit_f(setpointLimited, setpointCommand,
                                       SetpointRiseRate, effectiveFallRate, actualDtSec);

        // Output current PID compute.
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
  float dutyNewFloat = governor_apply(lastAppliedDuty, dutyRequest, govMode,
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

    // Live score accumulation — gate on slewed setpoint rate of change.
    // Using the slewed signal is fair regardless of SetpointRiseRate: slow slew = slow
    // rate = window opens slowly but isn't falsely penalized for the slew itself.
    // Window extends 3s beyond last tick where slewed rate >= 5 A/s.
    {
      static float liveScore_prevSlewed = 0.0f;
      static bool  liveScore_slewInit   = false;
      float slewedRate = 0.0f;
      if (liveScore_slewInit) {
        slewedRate = fabsf(setpointLimited - liveScore_prevSlewed) / actualDtSec;
      }
      liveScore_prevSlewed = setpointLimited;
      liveScore_slewInit   = true;
      if (slewedRate >= 5.0f) {
        liveScore_lastStepMs = tick.nowMs;
        liveScore_inWindow   = true;
      }
      if (liveScore_inWindow && (tick.nowMs - liveScore_lastStepMs > 3000)) {
        liveScore_inWindow = false;
      }
      if (liveScore_inWindow) {
        accumulateLiveScore(pidError, actualDtSec, tick.nowMs);
      }
    }

    // CV live score — gate on battery current rate-of-change; 12s scoring window
    if (voltageControlActive) {
      if (fabsf(g_dBcur_dt) >= CV_LIVE_GATE_APS) {
        cvLiveScore_lastDtMs = tick.nowMs;
        cvLiveScore_inWindow = true;
      }
      if (cvLiveScore_inWindow && (tick.nowMs - cvLiveScore_lastDtMs > 12000)) {
        cvLiveScore_inWindow = false;
      }
      if (cvLiveScore_inWindow) {
        float vErr = getFiltV() - ChargingVoltageTarget;  // positive = overvoltage
        accumulateCVLiveScore(vErr, actualDtSec, tick.nowMs);
      }
    }
  } else {
    innerTermP = innerTermI = innerTermD = 0.0f;
  }

  updateFieldTelemetry(dutyCycle, tick.currentBatteryVoltage, FieldResistance);

  fieldActiveStatus = (!gpio4IsLow && lastAppliedDuty > 0.01f)
                        ? ((sysMode == SYS_MODE_MANUAL) ? 3 : 1)
                        : 0;
  chargeStageDisplay = getChargeStageDisplayCode();

  uTargetRaw_cached = uTargetRaw;  // update pre-OV ceiling for next tick's fastOvBaseCap

  // All temperature loop, output current PID, and duty pipeline state is now final for this tick.
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
    }
    if (!thermallyConstrained && lastThermallyConstrained) {
      queueConsoleMessageF(
        "Absorption: tail detection resumed | penalty=%.1fA uTarget=%.1fA tailThresh=%.1fA",
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
    }

  } else if (inIdleStage) {
    // ===== IDLE (UseFloat=0, post-absorption rest) =====
    const uint32_t tIdle = (uint32_t)(now - floatStartTime);

    static uint32_t lastIdleDebugMs = 0;
    if ((uint32_t)(now - lastIdleDebugMs) >= 30000) {
      lastIdleDebugMs = now;
      queueConsoleMessageF("Idle status | battV=%.2fV Bcur=%.1fA tIdle=%lus rebulkV=%.2fV rebulkI=%.1fA",
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
        }
      } else {
        rebulkTimer = 0;
      }
    } else {
      rebulkTimer = 0;
    }

  } else {
    // ===== FLOAT =====
    // UseFloat may have been disabled at runtime while already in float, or all
    // stage flags may be false for another reason (e.g. override mode exit).
    // Either way: if float is not wanted, move to IDLE now rather than charging
    // an already-full battery at an unregulated float voltage.
    if (UseFloat == 0) {
      inIdleStage = true;
      floatStartTime = now;
      rebulkTimer = 0;
      queueConsoleMessage("Stage: FLOAT→IDLE (UseFloat disabled)");
      return;
    }

    ChargingVoltageTarget = FloatVoltage;

    const uint32_t tFloat = (uint32_t)(now - floatStartTime);
    const bool floatTimedOut = (tFloat >= (uint32_t)(FLOAT_DURATION * 1000UL));

    static uint32_t lastFloatDebugMs = 0;
    if ((uint32_t)(now - lastFloatDebugMs) >= 30000) {
      lastFloatDebugMs = now;
      float vErr = FloatVoltage - v;
      queueConsoleMessageF("Float status | battV=%.2fV floatTarget=%.2fV vErr=%.3fV Bcur=%.1fA tFloat=%lus rebulkV=%.2fV minFloatTime=%lus",
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
    case REASON_RPM_TOO_LOW: return "RPM_TOO_LOW";
    case REASON_CURRENT_STALE: return "CURRENT_STALE";

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

  // PRIORITY 3: RPM GATE (field must be cut when engine is not running)
  if (tick.rpmBelowMinimum) {
    return MODE_CRITICAL_RAMP;
  }

  // PRIORITY 4: CRITICAL CONDITIONS (auto mode only)
  if (tick.tempDataVeryStale && !tick.ignoreTemperature) {
    return MODE_CRITICAL_RAMP;
  }
  if (!tick.voltagePlausible || tick.voltageDisagreementCritical) {
    return MODE_CRITICAL_RAMP;
  }
  if (!tick.ignoreTemperature && tick.tempToUseF > (tick.tempLimitF + tick.tempCritExcessF)) {
    return MODE_CRITICAL_RAMP;
  }

  // PRIORITY 5: WARNING CONDITIONS (all start lockout)
  if (tick.currentBatteryVoltage > (tick.bulkVoltage + tick.voltageSpikeMargin)) {
    return MODE_WARNING_RAMP_AND_LOCKOUT;
  }
  if (tick.voltageDisagreementWarning) {
    return MODE_WARNING_RAMP_AND_LOCKOUT;
  }
  if (!tick.ignoreTemperature && tick.tempToUseF > (tick.tempLimitF + tick.tempWarnExcessF)) {
    return MODE_WARNING_RAMP_AND_LOCKOUT;
  }

  // PRIORITY 6: AUTO-ZERO
  if (tick.autoZeroActive) {
    return MODE_LOCKOUT_RAMP;
  }

  // PRIORITY 7: LOCKOUT
  if (tick.inLockout) {
    return MODE_LOCKOUT_RAMP;
  }

  // PRIORITY 8: NORMAL AUTO
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

  // Priority 3: RPM gate
  if (tick.rpmBelowMinimum) return REASON_RPM_TOO_LOW;

  // Priority 4: Critical (auto mode only)
  if (tick.tempDataVeryStale && !tick.ignoreTemperature) return REASON_TEMP_STALE;
  if (tick.currentDataStale) return REASON_CURRENT_STALE;
  if (!tick.voltagePlausible) return REASON_VOLTAGE_IMPLAUSIBLE;
  if (tick.voltageDisagreementCritical) return REASON_VOLTAGE_DISAGREE_CRITICAL;
  if (!tick.ignoreTemperature && tick.tempToUseF > (tick.tempLimitF + tick.tempCritExcessF)) {
    return REASON_TEMP_CRITICAL;
  }

  // Priority 5: Warning
  if (tick.currentBatteryVoltage > (tick.bulkVoltage + tick.voltageSpikeMargin)) return REASON_VOLTAGE_SPIKE;
  if (tick.voltageDisagreementWarning) return REASON_VOLTAGE_DISAGREE_WARNING;
  if (!tick.ignoreTemperature && tick.tempToUseF > (tick.tempLimitF + tick.tempWarnExcessF)) {
    if (tempWarningStartMs > 0 && (tick.nowMs - tempWarningStartMs > TempSustainedTimeout)) {
      return REASON_TEMP_SUSTAINED;
    }
    return REASON_TEMP_WARNING;
  }

  // Priority 6: Auto-zero
  if (tick.autoZeroActive) return REASON_AUTOZERO_ACTIVE;

  // Priority 7: Lockout
  if (tick.inLockout) return REASON_LOCKOUT_ACTIVE;

  return REASON_NONE;
}

// Tracks rising edges on reasons that map to protection counters.
// Called once per loop tick after selectFieldEventReason().
void updateProtectionCounters(FieldEventReason reason) {
  static FieldEventReason prevReason = REASON_NONE;
  if (reason != prevReason) {
    switch (reason) {
      case REASON_INA_OVERVOLTAGE:           g_inaOVCount++;             break;
      case REASON_HARD_OVERCURRENT:          g_hardOCCount++;            break;
      case REASON_VOLTAGE_SPIKE:             g_voltSpikeCount++;         break;
      case REASON_VOLTAGE_DISAGREE_CRITICAL: g_voltDisagreeCritCount++;  break;
      case REASON_VOLTAGE_DISAGREE_WARNING:  g_voltDisagreeWarnCount++;  break;
      case REASON_VOLTAGE_IMPLAUSIBLE:       g_voltImplausibleCount++;   break;
      case REASON_TEMP_CRITICAL:             g_tempCritCount++;          break;
      case REASON_TEMP_SUSTAINED:            g_tempSustainedCount++;     break;
      case REASON_TEMP_STALE:                g_tempStaleCount++;         break;
      case REASON_CURRENT_STALE:             g_currentStaleCount++;      break;
      default: break;
    }
    prevReason = reason;
  }
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
  if (reason == REASON_RPM_TOO_LOW) return true;
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
    case REASON_CURRENT_STALE:
    case REASON_HARD_OVERCURRENT:
    case REASON_RPM_TOO_LOW:
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
  tick.ignoreTemperature = (IgnoreTemperature != 0) || (ThermalTuningMode != 0);
  tick.ignoreRPM = (IgnoreRPM != 0);
  tick.rpmBelowMinimum = (!tick.ignoreRPM && RPM < (float)MinRPMForField);

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
  // Idle stage (UseFloat=0, post-absorption): battery is full, stop charging.
  // Rebulk logic in updateChargingStage() still runs and clears inIdleStage when
  // voltage sags or discharge current threshold is hit — chargingEnabled recovers
  // automatically on the next tick.
  if (inIdleStage) chargingEnabledLocal = false;

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
    tempDataVeryStale = (tempAge > 20000);
  }

  if (isnan(tempSelected) || tempSelected < -50.0f || tempSelected > 400.0f) {
    tempDataVeryStale = true;
  }

  tick.tempDataVeryStale = tempDataVeryStale;

  // Current sensor staleness — same freshness system as temperature
  {
    bool curStale = false;
    if (dataTimestamps[IDX_MEASURED_AMPS] == 0) {
      if (tick.nowMs > 30000) curStale = true;
    } else {
      curStale = (tick.nowMs - dataTimestamps[IDX_MEASURED_AMPS]) > 10000;
    }
    tick.currentDataStale = curStale;
  }

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
  bool inaOvSuppressActive = inaOvervoltageLatched || (inaOvervoltageClearedMs > 0 && (tick.nowMs - inaOvervoltageClearedMs) < INA_OV_DISAGREE_SUPPRESS_MS);

  if (inaOvSuppressActive) {
    tick.voltageDisagreementWarning = false;
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
  // Reset to Normal mode so saveUserTableEdits() writes Normal defaults to "capTable"
  HiLow = 1;
  writeFile(LittleFS, "/HiLow.txt", "1");
  saveUserTableEdits();  // saves Normal defaults to capTable / capPowerTable
  // Also write Lo defaults to capTableLo so that mode starts clean
  {
    float loDefaults[RPM_TABLE_SIZE];
    float loDefaultsPwr[RPM_TABLE_SIZE] = { 0 };
    for (int i = 0; i < RPM_TABLE_SIZE; i++) loDefaults[i] = defaultCapCurrentValues[i] * 0.5f;
    nvs_handle_t nvs_lo;
    if (nvs_open("learning", NVS_READWRITE, &nvs_lo) == ESP_OK) {
      nvs_set_blob(nvs_lo, "capTableLo", loDefaults, sizeof(loDefaults));
      nvs_set_blob(nvs_lo, "capPowerTableLo", loDefaultsPwr, sizeof(loDefaultsPwr));
      nvs_commit(nvs_lo);
      nvs_close(nvs_lo);
    }
  }
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

  // Load cap current table — key depends on HiLow mode (already loaded from LittleFS before this call)
  {
    const char *capKey = (HiLow == 1) ? "capTable" : "capTableLo";
    const char *capPwrKey = (HiLow == 1) ? "capPowerTable" : "capPowerTableLo";
    required_size = sizeof(rpmCapCurrentTable);
    err = nvs_get_blob(nvs_handle, capKey, rpmCapCurrentTable, &required_size);
    if (err != ESP_OK || required_size != sizeof(rpmCapCurrentTable)) {
      for (int i = 0; i < RPM_TABLE_SIZE; i++)
        rpmCapCurrentTable[i] = (HiLow == 1) ? defaultCapCurrentValues[i] : defaultCapCurrentValues[i] * 0.5f;
    }
    required_size = sizeof(rpmCapPowerTable);
    err = nvs_get_blob(nvs_handle, capPwrKey, rpmCapPowerTable, &required_size);
    if (err != ESP_OK || required_size != sizeof(rpmCapPowerTable)) {
      for (int i = 0; i < RPM_TABLE_SIZE; i++) rpmCapPowerTable[i] = 0.0f;
    }
  }

  // Load cap limit mode (non-critical, default to amps)
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

// Loads the correct cap tables from NVS into the active arrays based on HiLow mode.
// HiLow=1 → Normal (key "capTable"), HiLow=0 → Low Charge Rate (key "capTableLo").
// Falls back to defaults if no saved data exists for that mode.
void loadCapTablesForMode(int mode) {
  nvs_handle_t nvs_handle;
  esp_err_t err = nvs_open("learning", NVS_READONLY, &nvs_handle);
  if (err != ESP_OK) {
    for (int i = 0; i < RPM_TABLE_SIZE; i++) {
      rpmCapCurrentTable[i] = (mode == 1) ? defaultCapCurrentValues[i] : defaultCapCurrentValues[i] * 0.5f;
      rpmCapPowerTable[i] = 0.0f;
    }
    return;
  }
  const char *capKey = (mode == 1) ? "capTable" : "capTableLo";
  const char *capPwrKey = (mode == 1) ? "capPowerTable" : "capPowerTableLo";

  size_t sz = sizeof(rpmCapCurrentTable);
  err = nvs_get_blob(nvs_handle, capKey, rpmCapCurrentTable, &sz);
  if (err != ESP_OK || sz != sizeof(rpmCapCurrentTable)) {
    for (int i = 0; i < RPM_TABLE_SIZE; i++)
      rpmCapCurrentTable[i] = (mode == 1) ? defaultCapCurrentValues[i] : defaultCapCurrentValues[i] * 0.5f;
  }
  sz = sizeof(rpmCapPowerTable);
  err = nvs_get_blob(nvs_handle, capPwrKey, rpmCapPowerTable, &sz);
  if (err != ESP_OK || sz != sizeof(rpmCapPowerTable)) {
    for (int i = 0; i < RPM_TABLE_SIZE; i++) rpmCapPowerTable[i] = 0.0f;
  }
  nvs_close(nvs_handle);
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

  {
    const char *capKey = (HiLow == 1) ? "capTable" : "capTableLo";
    const char *capPwrKey = (HiLow == 1) ? "capPowerTable" : "capPowerTableLo";
    err = nvs_set_blob(nvs_handle, capKey, rpmCapCurrentTable, sizeof(rpmCapCurrentTable));
    if (err != ESP_OK) success = false;
    err = nvs_set_blob(nvs_handle, capPwrKey, rpmCapPowerTable, sizeof(rpmCapPowerTable));
    if (err != ESP_OK) success = false;
  }

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

  if (ThermalTuningMode) return;  // don't record overheat history during step tests
  if (isnan(TempToUse) || TempToUse < -50.0f || TempToUse > 400.0f) return;
  if (dtMs == 0) return;

  int bucket = currentRPMTableIndex;
  if (bucket < 0 || bucket >= RPM_TABLE_SIZE) return;

  static bool inOverheat = false;

  if (!inOverheat) {
    if (TempToUse >= TemperatureLimitF) {
      inOverheat = true;
      overheatCount[bucket]++;
      totalOverheats++;
      timeSinceLastOverheat = 0;
      saveHistoricalDataImmediate();  // save immediately on overheat entry
    } else {
      cumulativeNoOverheatTime[bucket] += dtMs;
      totalSafeMs += (uint64_t)dtMs;
      totalSafeHours = (float)(totalSafeMs / 3600000ULL);
      timeSinceLastOverheat += dtMs;
      // periodic 5-min save removed — was OBSOLETE, caused 65-70ms nvs_commit stall in AdjustFieldLearnMode every 5 min
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
  timeSinceLastOverheat = 0;
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

  Serial.printf("TempPID: Init | Kp=%.2f Ki=%.3f Lookahead=%.1fs Interval=%lums\n",
                TempPIDKp, TempPIDKi, ThermalLookaheadSec, (unsigned long)TempPIDIntervalMs);
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
    memset(thermalSlopeBuffer, 0, sizeof(thermalSlopeBuffer));
    thermalSlopeBufIdx = 0;
    thermalSlopeBufFull = false;
    thermalSlopeLastPushMs = 0;
    thermalSlopeFPerSec = 0.0f;
    projectedTempF = NAN;
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
  const float penaltyMin = 0.0f;  //Cold boost is gone as a concept now.

  // ---------------------------------------------------------------------------
  //  Temperature sanity guard — only stops PID on invalid value (NaN / out of range).
  //  Stale data (no new reading) is handled at 20s by tempDataVeryStale → field cut;
  //  no intermediate hold needed here.
  // ---------------------------------------------------------------------------
  bool tempValueSane = !isnan(TempToUse) && (TempToUse > -50.0f) && (TempToUse < 400.0f);

  if (!tempValueSane) {
    if (tempPIDActive) {
      tempPID.SetMode(MANUAL);
      tempPIDActive = false;
      queueConsoleMessageF("TempPID: temp value invalid, holding penalty at %.1fA",
                           thermalPenaltyLastValid);
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
    float alpha = (TempSource == 0) ? TempPIDFilterAlpha : 0.02f;  // thermistor alpha hardcoded — not user-configurable
    tempFiltered = alpha * TempToUse + (1.0f - alpha) * tempFiltered;
  }

  // ---------------------------------------------------------------------------
  //  Slope estimator: long-window backward difference.
  //  Runs at TempPIDIntervalMs cadence (same as Compute). Buffer holds
  //  THERMAL_SLOPE_BUF readings; window = (THERMAL_SLOPE_BUF - 1) × 5s = 60s.
  //  Slope only valid once buffer is full. Hard clamp catches sensor garbage.
  // ---------------------------------------------------------------------------
  if ((uint32_t)(nowMs - thermalSlopeLastPushMs) >= TempPIDIntervalMs) {
    thermalSlopeLastPushMs = nowMs;
    float tempSample = (TempSource == 0) ? TempToUse : tempFiltered;
    thermalSlopeBuffer[thermalSlopeBufIdx] = tempSample;
    thermalSlopeBufIdx = (thermalSlopeBufIdx + 1) % THERMAL_SLOPE_BUF;
    if (thermalSlopeBufIdx == 0) thermalSlopeBufFull = true;

    if (thermalSlopeBufFull) {
      float oldest = thermalSlopeBuffer[thermalSlopeBufIdx];
      const float windowSec = (float)(THERMAL_SLOPE_BUF - 1) * (TempPIDIntervalMs / 1000.0f);
      float rawSlope = (tempSample - oldest) / windowSec;
      const float SLOPE_CLAMP = 0.5f;  // °F/sec — beyond this is sensor noise or fault
      if (fabsf(rawSlope) > SLOPE_CLAMP) {
        rawSlope = clamp_f(rawSlope, -SLOPE_CLAMP, SLOPE_CLAMP);
        static uint32_t slopeClampLastLogMs = 0;
        if ((uint32_t)(nowMs - slopeClampLastLogMs) >= 60000) {
          slopeClampLastLogMs = nowMs;
          queueConsoleMessageF("TempPID: slope clamped to %.3f °F/s — check sensor", rawSlope);
        }
      }
      thermalSlopeFPerSec = rawSlope;
    } else {
      thermalSlopeFPerSec = 0.0f;  // buffer not yet full — no prediction yet
    }
  }
  // Update projected temperature every call so PID input is always fresh.
  {
    float tempNow = (TempSource == 0) ? TempToUse : tempFiltered;
    projectedTempF = isnan(tempNow) ? tempFiltered : (tempNow + thermalSlopeFPerSec * ThermalLookaheadSec);
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
  // ===== THERMAL TUNING MODE: use wave setpoint if test is active =====
  // thermalTuning_tick() runs at the END of this function and updates
  // thermalWaveCurrentSetpointF for the NEXT tick (one-tick lag is fine at 5s PID intervals).
  const float activeTempLimit = (ThermalTuningMode && thermalTuningScore.testStarted)
                                ? thermalWaveCurrentSetpointF : TemperatureLimitF;

  if (!tempPIDActive) {
    tempPID.SetOutputLimits((double)penaltyMin, (double)penaltyMax);

    // Clear slope buffer first — old trend data is stale after a gap, and a stale
    // slope would inflate projectedTempF used for the bumpless seed below.
    memset(thermalSlopeBuffer, 0, sizeof(thermalSlopeBuffer));
    thermalSlopeBufIdx = 0;
    thermalSlopeBufFull = false;
    thermalSlopeLastPushMs = 0;
    thermalSlopeFPerSec = 0.0f;
    // Recompute projectedTempF without stale slope — this is what the seed actually sees.
    {
      float tempNow = (TempSource == 0) ? TempToUse : tempFiltered;
      projectedTempF = isnan(tempNow) ? tempFiltered : tempNow;
    }

    // Warmup setpoint: buffer just cleared so slope = 0 for 60s. Apply fallback margin
    // so the PID reacts before temp reaches the hard limit during the unprotected window.
    const float reEnableSetpoint = activeTempLimit - 20.0f;

    tempPIDInput_d = (double)projectedTempF;
    thermalPenaltyAmps_d = (double)thermalPenaltyLastValid;
    tempPID.SetMode(AUTOMATIC);

    // Seed integrator from P-only at current temperature, not stale held value.
    // thermalPenaltyLastValid reflects conditions when the sensor went stale —
    // if temp was high then but has since recovered, seeding at the old value
    // overpunishes and takes many minutes to wind down via integrator alone.
    float stalePenalty = thermalPenaltyLastValid;
    float e_resume = projectedTempF - reEnableSetpoint;
    float resumePenalty = clamp_f(e_resume > 0.0f ? TempPIDKp * e_resume : 0.0f,
                                  penaltyMin, penaltyMax);
    tempPID.ResetIntegratorTo((double)resumePenalty);

    thermalPenaltyAmps = resumePenalty;
    prevThermalPenalty = resumePenalty;
    thermalPenaltyLastValid = resumePenalty;

    tempPIDActive = true;
    queueConsoleMessageF("TempPID: resumed | projTemp=%.1f°F setpoint=%.1f°F penalty=%.1fA (was %.1fA) stage=%s",
                         projectedTempF, reEnableSetpoint, resumePenalty,
                         stalePenalty, inPureBulk ? "bulk" : "CV");
  }

  // ---------------------------------------------------------------------------
  //  Update setpoint and output limits every tick.
  //  Both use the same penaltyMin computed at function entry.
  //  effectiveSetpoint applies a 20°F fallback margin during the 60s warmup
  //  window (thermalSlopeBufFull == false) so the PID starts reacting before
  //  projected temp reaches the hard limit. Once the buffer fills, margin = 0.
  // ---------------------------------------------------------------------------
  const float effectiveSetpoint = thermalSlopeBufFull ? (activeTempLimit - 5.0f) : (activeTempLimit - 20.0f);
  tempPIDSetpoint_d = (double)effectiveSetpoint;
  tempPIDInput_d = (double)projectedTempF;

  tempPID.SetTunings((double)TempPIDKp, (double)TempPIDKi, 0.0);
  tempPID.SetOutputLimits((double)penaltyMin, (double)penaltyMax);

  bool pidComputed = tempPID.Compute();

  // ---------------------------------------------------------------------------
  //  Post-compute: slew limiter, final clamp.
  //  All use penaltyMin / penaltyMax — no re-derivation.
  // ---------------------------------------------------------------------------
  if (pidComputed) {
    thermalPenaltyAmps = (float)thermalPenaltyAmps_d;

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
  //  Cadence: runs every tempPID_tick() call (~16 Hz output current loop rate), not
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

  // Wave generator + scoring + always-on live score
  thermalTuning_tick(nowMs, actualDtSec);
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

  // ── Voltage loop ─────────────────────────────────────────────────────────
  e.battV = IBV;
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

  // ── Output current PID ────────────────────────────────────────────────────
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

  e.battV_filt = IBV_filtered;
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
  e.tempFiltered  = thermalLogScale10(tempFiltered);
  e.tempProjected = thermalLogScale10(projectedTempF);
  // effective setpoint: mirrors the logic in tempPID_tick (slopeBufFull = 5°F margin, else 20°F warmup margin)
  {
    float logLimit = (ThermalTuningMode && thermalTuningScore.testStarted)
                     ? thermalWaveCurrentSetpointF : TemperatureLimitF;
    float logSetpoint = thermalSlopeBufFull ? (logLimit - 5.0f) : (logLimit - 20.0f);
    e.nominalTarget = thermalLogScale10(logSetpoint);
  }
  e.rpmCap = thermalLogScale10(getCapCurrentForRPM(RPM));
  e.voltCap = thermalLogScale10(Icv);
  e.uTarget = thermalLogScale10((float)uTargetAmps);
  e.spLimited = thermalLogScale10(setpointLimited);
  e.pidErr = thermalLogScale10(pidError);
  e.pidOut = thermalLogScale10((float)pidOutput);
  e.duty = thermalLogScale10(dutyCycle);
  e.rpm = thermalLogScaleRPM(RPM);
  e.battV = thermalLogScale10(IBV);
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
  e.thermalSlope = (int16_t)(thermalSlopeFPerSec * 1000.0f);

  thermalLogHead = (thermalLogHead + 1) % THERMAL_LOG_SIZE;
  if (thermalLogCount < THERMAL_LOG_SIZE) thermalLogCount++;
}
