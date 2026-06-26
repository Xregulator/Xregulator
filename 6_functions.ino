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
// Auto Min% learning ("knee tracker") — observer + persistence (defined lower in this file)
void kneeLearnObserve(float rpm, float appliedDuty, float vbus, float tF, float amps,
                      float dutyRequest, float rpmFloorDuty, bool modeOk);
void kneeLearnInit();
void kneeLearnService(bool fieldOff);
void saveKneeLearnState();
void kneeLearnResetDefaults();
String kneeLearnStateJson();
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
void handleLimpHome(uint32_t currentMillis, const TickSnapshot &tick);
void runShutdownPath(const TickSnapshot &tick, FieldControlMode mode, FieldEventReason reason,
                     float actualDtSec, bool exitingNormal);
void runCommissionIdle(const TickSnapshot &tick, FieldEventReason reason, float actualDtSec);

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

// CV AUTO-tune design constants — see Working Markdown Docs/CV_AUTOTUNE_PLAN.md §E (2026-06-25).
// The old λ/SIMC fit (cvPlantTau, cvPlantL, cvLambdaMult) is RETIRED: a real step test showed the
// loop is stability-robust (86–95° phase margin even at the "disaster" gains), so neither phase
// margin nor a precise τ/L fit binds. The only per-install unknown that matters is the DC gain
// K_dc (cvPlantK, settled ΔV/ΔI). Set a deliberately slow target crossover and scale Kp ∝ 1/K_dc;
// this stays well below the polarization pole (~0.7 rad/s) and the dead-time limit, so we never
// need to measure them. ω_target/ρ are bench-tuned by disturbance-rejection (no protection trips), NOT
// to match any prior hand tune — they are the user-adjustable settings cvOmega / cvKiRatio (Tuning ▸
// Voltage; were #defines, defaults 0.20 / 0.70). cvPlantTau/cvPlantL are still captured for diagnostics
// but no longer drive the gains.

// computeCvTempScale — battery-temperature gain derate factor (see the globals block in Xregulator.ino).
// Board temp (ambientTemp, °F) is a proxy for battery temp; the battery's internal resistance — which IS
// the CV plant gain K_dc — rises as it cools, so gains set at the commissioning temperature run too hot
// when colder. Returns R(T_commission)/R(T_now) = exp(coeff·(T_now−T_comm)) (coeff = fractional resistance
// rise per °C): colder now → exponent<0 → factor<1 → gains scaled DOWN (the safe direction). Returns 1.0
// (no change) when disabled, never commissioned, or the proxy is stale/invalid — never amplifies blindly.
// Temps clamped to [0,100]°C; result clamped to [0.30,1.20] so a wild proxy reading can't make the gains
// dangerous. This is a Kp+Ki scale, NOT a λ/ω change — ω and ρ are held fixed.
float computeCvTempScale() {
  if (!battTempDerateEnable) return 1.0f;
  if (isnan(CommissionTempF)) return 1.0f;
  if (IS_STALE(IDX_AMBIENT_TEMP) || !isfinite(ambientTemp)) return 1.0f;
  float tCommC = clamp_f((CommissionTempF - 32.0f) / 1.8f, 0.0f, 100.0f);
  float tNowC  = clamp_f((ambientTemp     - 32.0f) / 1.8f, 0.0f, 100.0f);
  float s = expf(battTempCoeff * (tNowC - tCommC));
  return clamp_f(s, 0.30f, 1.20f);
}

// recomputeCvGains — derive the gains the CV loop actually uses (VoltageKp_active / VoltageKi_active)
// from the selected gain mode, the measured DC gain K_dc, and the 12V-block normalization. Call after
// any related setting change (cvGainMode, manual VoltageKp/Ki, a plant fit, or BATTERY_VOLTAGE), on a
// board-temp drift (for the temp derate), and once at boot. Everything is computed in 12V-equivalent
// ("normalized") space — so the same gain numbers work on 12/24/48 V — then ×(12/BATTERY_VOLTAGE) bakes
// it back to the pack-space gain the loop multiplies by the raw pack-volt error. The battery-temp derate
// (computeCvTempScale) is the final multiplier on the active gains and applies in BOTH modes (the plant
// changes with temperature regardless of how the base gains were chosen). See CV_AUTOTUNE_PLAN.md §E.
void recomputeCvGains() {
  float vNorm = 12.0f / (float)BATTERY_VOLTAGE;     // 1, 0.5, 0.25 for 12/24/48 V
  bool plantValid = (cvPlantK > 1e-6f);             // only K_dc is required now (τ/L no longer used)
  float kpNorm, kiNorm;                             // 12V-equivalent gains (what the user sees)
  if (cvGainMode == 1 && plantValid) {
    // AUTO (measured-K_dc rule). Normalize the measured pack-space K_dc into 12V-equivalent space so
    // the resulting gain is system-voltage-independent like the manual numbers, then set the gains
    // from the conservative target crossover. Kp ∝ 1/K_dc is the per-install gain schedule.
    float Knorm = cvPlantK * vNorm;                 // V per 12V-equivalent, per A
    kpNorm = cvOmega / Knorm;                        // Kp = ω_target / K_dc
    kiNorm = cvKiRatio * kpNorm;                     // Ki = ρ · Kp
    // Safety bounds — a bad fit must never produce dangerous gains.
    kpNorm = clamp_f(kpNorm, 2.0f, 120.0f);
    kiNorm = clamp_f(kiNorm, 1.0f, 80.0f);
  } else {
    // MANUAL, or AUTO with no valid fit yet → use the typed / conservative gains.
    kpNorm = VoltageKp;
    kiNorm = VoltageKi;
  }
  cvComputedKp = kpNorm;                            // expose for the dashboard — BASE design gain (no temp derate)
  cvComputedKi = kiNorm;
  cvTempDerateScale = computeCvTempScale();         // battery-temp correction (1.0 unless commissioned + enabled)
  VoltageKp_active = kpNorm * vNorm * cvTempDerateScale;   // pack-space gains the loop uses with raw pack-volt error
  VoltageKi_active = kiNorm * vNorm * cvTempDerateScale;
}

// recomputeCcGains — CC (output-current) analog of recomputeCvGains. PidKp/Ki/Kd are 12V-equivalent;
// ×(12/BATTERY_VOLTAGE) bakes them into the duty-space gains the inner current PID actually applies,
// so one set of tunings behaves identically on 12/24/48 V (field current per duty-% scales with bus
// voltage). Call after any PidK* change and after a BATTERY_VOLTAGE change. currentPID is a global
// object (constructed before setup), so SetTunings is safe to call unconditionally.
void recomputeCcGains() {
  float vNorm = 12.0f / (float)BATTERY_VOLTAGE;     // 1, 0.5, 0.25 for 12/24/48 V
  PidKp_active = PidKp * vNorm;
  PidKi_active = PidKi * vNorm;
  PidKd_active = PidKd * vNorm;
  currentPID.SetTunings(PidKp_active, PidKi_active, PidKd_active);
}

// ccDutyCeiling — the upper duty bound the CC loop is allowed to reach. This is simply MaxDuty (Max
// Field %), which is now the REAL per-bus cap: its default is scaled down on higher-voltage banks
// (~50%@24V, 25%@48V) and rescaled on a voltage change, so the user-visible Max Field box IS the cap
// the loop respects — no hidden ×12/Vbatt. A user wanting full duty just sets Max Field to 99.
float ccDutyCeiling() {
  return MaxDuty;
}

// applyCcOutputLimits — set the inner current PID's output bounds to [MinDuty, ccDutyCeiling()]. Making
// the ceiling the PID's REAL upper limit lets its anti-windup track it: otherwise the loop would wind
// the integrator toward MaxDuty while setDutyPercent() clamps the applied duty far lower, then lag on
// the way down. setDutyPercent() still applies the same clamp as the catch-all for the open-loop paths
// (manual/limp/fault) that bypass the PID. Call on AUTO entry, on a ceiling toggle, and on a voltage
// change — SetOutputLimits also re-clamps the live output + integrator, which is the desired anti-windup.
void applyCcOutputLimits() {
  currentPID.SetOutputLimits((double)MinDuty, (double)ccDutyCeiling());
}

// applyNominalVoltageChange — single entry point for a system-voltage class change (12/24/48 V),
// triggered from the Vessel Info save (BATTERY_VOLTAGE is the sole source of truth). Call AFTER
// setting BATTERY_VOLTAGE = newV. When the class actually changes it persists the new class to NVS
// (NK_BatteryVoltage — authoritative; vessel_info.json is only a mirror), then rescales the PERSISTED
// charge-voltage profile by newV/oldV (Bulk/Float/Absorption/Rebulk/Target/Charged/alarms), re-derives
// the hard-shutdown trip (newBulk + 0.3) and refreshes the INA228 hardware OV limit. Writing the class
// and the rescaled profile to NVS in the SAME call (synchronously) is what closes the old atomicity
// gap: if vessel_info.json (LittleFS, formatOnFail) is ever lost, NVS still holds a consistent
// class+profile pair, so the overvoltage trips can't strand at the wrong voltage. It always re-derives
// both control loops' normalized gains. It also rescales the field-duty knobs (knee margin/step/
// maxfloor + DutyRampRate + MaxDuty/Max Field %) in place by oldV/newV and persists them, so they
// stay WYSIWYG in real duty-% / %/s (the live paths no longer multiply by 12/Vbatt at use). Nothing
// reads BATTERY_VOLTAGE at duty-clamp time anymore. Currents/times/gains are voltage-independent.
void applyNominalVoltageChange(int oldV, int newV) {
  if (newV != oldV && oldV > 0 && (newV == 12 || newV == 24 || newV == 48)) {
    settingWrite(NK_BatteryVoltage, String(newV).c_str());  // persist class FIRST, same transaction as the profile below
    float ratio = (float)newV / (float)oldV;
    BulkVoltage           *= ratio;
    FloatVoltage          *= ratio;
    AbsorptionVoltage     *= ratio;
    RebulkVoltage         *= ratio;
    TargetVoltageSetpoint *= ratio;
    ChargedVoltage_Scaled = (int)lroundf(ChargedVoltage_Scaled * ratio);
    VoltageAlarmHigh      = (int)lroundf(VoltageAlarmHigh * ratio);
    VoltageAlarmLow       = (int)lroundf(VoltageAlarmLow * ratio);
    AlternatorHardShutdownV = BulkVoltage + 0.3f;
    // Knee duty-domain knobs are stored in REAL duty-% for the bus, so rescale them by the INVERSE
    // ratio (oldV/newV): a 5% margin at 12V becomes 1.25% at 48V. Persist so the dashboard box shows
    // the new value — the math is visible, never hidden behind a runtime multiply.
    float dutyRatio = (float)oldV / (float)newV;
    kneeMarginPct   *= dutyRatio;
    kneeStepPct     *= dutyRatio;
    kneeMaxFloorPct *= dutyRatio;
    DutyRampRate    *= dutyRatio;
    MaxDuty          = (int)lroundf(MaxDuty * dutyRatio);  // Max Field %: real per-bus cap, scales down on higher banks
    settingWrite(NK_kneeMarginPct,   String(kneeMarginPct, 2).c_str());
    settingWrite(NK_kneeStepPct,     String(kneeStepPct, 2).c_str());
    settingWrite(NK_kneeMaxFloorPct, String(kneeMaxFloorPct, 2).c_str());
    settingWrite(NK_DutyRampRate,    String(DutyRampRate, 1).c_str());
    settingWrite(NK_MaxDuty,         String(MaxDuty).c_str());
    settingWrite(NK_BulkVoltage, String(BulkVoltage, 2).c_str());
    settingWrite(NK_FloatVoltage, String(FloatVoltage, 2).c_str());
    settingWrite(NK_AbsorptionVoltage, String(AbsorptionVoltage, 2).c_str());
    settingWrite(NK_RebulkVoltage, String(RebulkVoltage, 2).c_str());
    settingWrite(NK_TargetVoltageSetpoint, String(TargetVoltageSetpoint, 2).c_str());
    settingWrite(NK_ChargedVoltage, String(ChargedVoltage_Scaled).c_str());
    settingWrite(NK_VoltageAlarmHigh, String(VoltageAlarmHigh).c_str());
    settingWrite(NK_VoltageAlarmLow, String(VoltageAlarmLow).c_str());
    settingWrite(NK_AlternatorHardShutdownV, String(AlternatorHardShutdownV, 2).c_str());
    updateINA228OvervoltageThreshold();
  }
  recomputeCvGains();  // always re-derive the normalized active gains for the (possibly) new class
  recomputeCcGains();
  applyCcOutputLimits();  // ceiling tracks the new class so the PID limit + anti-windup stay correct
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
  if (dt_sec <= 0.0f) return prev;  // no time elapsed → no change; also guards negative/NaN dt
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
      // Slew from last applied duty. DutyRampRate is stored in REAL %/s for this bus (its default was
      // scaled by 12/Vbatt at first boot and it's rescaled in place on a voltage change), so it's used
      // as-is here — WYSIWYG with the dashboard box, no hidden runtime multiply.
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
  // Fast OV: arm the cooldown lockout so the field can't re-engage for FIELD_COLLAPSE_DELAY (30s).
  // applyImmediateCut returns before runShutdownPath's line-482 lockout-arm ever runs, so set it here.
  if (reason == REASON_FAST_OVERVOLTAGE && fieldCollapseTime == 0) { fieldCollapseTime = tick.nowMs; activeCollapseDelay = FIELD_COLLAPSE_DELAY; }
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
  // If a plant delay test is running, abort it — results from an interrupted test are invalid
  if (systemIDActive != 0 && !systemIDAbortRequested) {
    systemIDAbortReason = (uint8_t)reason;
    systemIDAbortPhase = systemIDActive;
    systemIDAbortRequested = true;
    queueConsoleMessageF("SystemID: ABORTED — protection fired (%s) during phase %d — run the test again",
                         reasonToString(reason), (int)systemIDActive);
  }
  // Commissioning field-curve / knee ramp shares the same override path — flag it for a clean abort
  // AND latch why, so the dashboard can report it instead of the dialog sitting there silently.
  // fieldCurve_tick resets its static phase when it next runs (after the cooldown lockout clears).
  if (fieldCurveActive != 0) {
    fieldCurveAbortRequested = true;
    fieldCurveAbortReason = (uint8_t)reason;
    strncpy(fieldCurveAbortMsg, reasonToString(reason), sizeof(fieldCurveAbortMsg) - 1);
    fieldCurveAbortMsg[sizeof(fieldCurveAbortMsg) - 1] = '\0';
  }
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

  applyCcOutputLimits();  // [MinDuty, voltage-scaled duty ceiling] so PID anti-windup respects the ceiling
  currentPID.SetSampleTime(100);
  recomputeCcGains();  // apply voltage-normalized PidK*_active (was: SetTunings(PidKp,PidKi,PidKd))
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
void handleLimpHome(uint32_t currentMillis, const TickSnapshot &tick) {
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
void runShutdownPath(const TickSnapshot &tick, FieldControlMode mode, FieldEventReason reason,
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
    activeCollapseDelay = (reason == REASON_RPM_TOO_LOW) ? RPM_RECOVERY_DELAY : FIELD_COLLAPSE_DELAY;
    queueConsoleMessageF("Charging stopped - %.1fs cooldown before restart",
                         activeCollapseDelay / 1000.0f);
  }

  reportFieldModeEvent(tick.nowMs, mode, reason, tick, gpio4IsLow, dutyCycle);
}

// runCommissionIdle — "rest" hold between commissioning steps. Holds the field at a flat voltage-scaled
// minimum (COMMISSION_REST_FLOOR_PCT × 12/Vbatt), eased in at a slow dedicated rate
// (COMMISSION_REST_RAMP_PCT × 12/Vbatt). Deliberately does NOT max() in the per-RPM onset floor
// (rpmMinDuty): that floor lives in rpmMinDutyTable, the very table commissioning is (re)learning, so
// resting on it means leaning on a possibly stale/half-written table; it also lets a held high-RPM-low
// onset duty briefly exceed the (lower) onset after RPM rises, producing a real output blip until the
// slew catches up. A flat floor is predictable and table-independent. GPIO4 stays ENABLED — a little
// field load keeps the RPM pickup alive (going fully to 0 can drop the signal). Deliberately bypasses
// the whole AUTO/MANUAL/fault/stage machinery (the caller returns right after this), so no charging
// stage runs, no "Charging stopped/enabled" spam, and no GPIO4 cut. A real fault never lands here —
// selectFieldControlMode returns the fault mode instead, so this is only reached when nominal.
void runCommissionIdle(const TickSnapshot &tick, FieldEventReason reason, float actualDtSec) {
  voltageControlActive = false;
  uTargetAmps = 0;
  setpointLimited = 0.0f;
  shutdownPhase = SHUTDOWN_PHASE_NONE;   // so a later real shutdown starts its ramp fresh from here

  const float vNorm = 12.0f / fmaxf(1.0f, (float)BATTERY_VOLTAGE);
  const float restFloor = COMMISSION_REST_FLOOR_PCT * vNorm;   // 4 / 2 / 1 % @ 12 / 24 / 48 V
  const float restRamp = COMMISSION_REST_RAMP_PCT * vNorm;     // 5 / 2.5 / 1.25 %/s
  const float restTarget = restFloor;

  // Dedicated slow slew toward the target (both directions), THEN governor in bypass-slew so it only
  // clamps (duty ceiling) and writes the PWM — we already did the slewing at the rest rate.
  float slowDuty = slew_limit_f(lastAppliedDuty, restTarget, restRamp, restRamp, actualDtSec);
  bool writeToHardware = !gpio4IsLow;
  float dutyNewFloat = governor_apply(lastAppliedDuty, slowDuty, GOV_BYPASS_SLEW,
                                      0.0f, writeToHardware, actualDtSec);
  currentPID.ResetIntegratorTo((double)dutyNewFloat);
  pidOutput = (double)dutyNewFloat;
  if (writeToHardware) {
    lastAppliedDuty = dutyNewFloat;
    digitalWrite(4, HIGH);   // keep the field enable asserted — never cut during rest
  }
  dutyCycle = dutyNewFloat;

  updateFieldTelemetry(dutyCycle, tick.currentBatteryVoltage, FieldResistance);
  fieldActiveStatus = (dutyCycle > 0.01f) ? 1 : 0;
  chargeStageDisplay = getChargeStageDisplayCode();
  reportFieldModeEvent(tick.nowMs, MODE_COMMISSION_IDLE, reason, tick, gpio4IsLow, dutyCycle);
}

// ==================== MAIN CONTROL FUNCTION ====================

// ============================================================================
// PID TUNING SCORE — helpers called from AdjustFieldLearnMode
// ============================================================================

// Called from setup() after PSRAM allocations and loadTuningLog() are done.
// Prints sizing info; allocation and load happen in setup() proper.
void tuningScore_init() {
  Serial.printf("TuningScore: %d record slots × %u bytes = %u bytes in PSRAM\n",
                50, (unsigned)sizeof(TuningRecord),
                (unsigned)(50 * sizeof(TuningRecord)));
}

void saveTuningLog() {
  if (!tuningLog) return;
  File f = LittleFS.open("/tuninglog.bin", "w");
  if (!f) return;
  f.write((uint8_t *)&tuningLogCount, sizeof(tuningLogCount));
  f.write((uint8_t *)&tuningLogHead, sizeof(tuningLogHead));
  f.write((uint8_t *)&tuningRunCounter, sizeof(tuningRunCounter));
  f.write((uint8_t *)tuningLog, 50 * sizeof(TuningRecord));
  f.close();
}

void loadTuningLog() {
  if (!tuningLog) return;
  File f = LittleFS.open("/tuninglog.bin", "r");
  if (!f) return;
  // Struct size is part of the wire format. Bail cleanly on mismatch (e.g. after
  // TuningRecord layout changed) instead of reading garbage into the array.
  const size_t expected = sizeof(tuningLogCount) + sizeof(tuningLogHead)
                          + sizeof(tuningRunCounter) + 50 * sizeof(TuningRecord);
  if (f.size() != expected) {
    Serial.printf("TuningLog: size mismatch (%u vs %u expected) — discarding old log\n",
                  (unsigned)f.size(), (unsigned)expected);
    f.close();
    LittleFS.remove("/tuninglog.bin");
    return;
  }
  f.read((uint8_t *)&tuningLogCount, sizeof(tuningLogCount));
  f.read((uint8_t *)&tuningLogHead, sizeof(tuningLogHead));
  f.read((uint8_t *)&tuningRunCounter, sizeof(tuningRunCounter));
  f.read((uint8_t *)tuningLog, 50 * sizeof(TuningRecord));
  f.close();
  // Corrupt count/head would index past the 50-slot ring (commit writes BEFORE
  // the %50 wrap) — discard rather than trust a bad header.
  if (tuningLogCount > 50 || tuningLogHead >= 50) {
    Serial.println("TuningLog: corrupt count/head — discarding old log");
    tuningLogCount = 0;
    tuningLogHead = 0;
    LittleFS.remove("/tuninglog.bin");
    return;
  }
  Serial.printf("TuningLog: loaded %d records, counter=%d\n", tuningLogCount, tuningRunCounter);
}

void commitTuningRecord() {
  if (!tuningLog) return;
  if (tuningScore.activeTimeSec < 0.5f) {
    tuningScore = {};
    return;
  }
  TuningRecord rec = {};
  rec.runNumber = ++tuningRunCounter;
  rec.score = tuningScore.errorAccum / tuningScore.activeTimeSec;
  rec.activeTimeSec = tuningScore.activeTimeSec;
  rec.kp = PidKp;
  rec.ki = PidKi;
  rec.kd = PidKd;
  rec.sampleDivisor = PidSampleDivisor;
  rec.trackingGain = PIDTrackingGain;
  rec.dutyRampRate = DutyRampRate;
  rec.waveAmplitude = (int16_t)waveAmplitude;
  rec.wavePeriod = (int16_t)wavePeriod;
  rec.waveFloor = (int16_t)tuningWaveFloor;
  rec.avgRPM = (tuningScore.avgSampleCount > 0)
                 ? (tuningScore.rpmSum / tuningScore.avgSampleCount)
                 : 0.0f;
  rec.avgAltTempF = (tuningScore.avgSampleCount > 0)
                      ? (tuningScore.tempSum / tuningScore.avgSampleCount)
                      : 0.0f;
  rec.worstErrorA = tuningScore.worstErrorA;
  rec.battV = BatteryV;
  rec.chargeStage = getChargeStageDisplayCode();
  rec.epoch = getCurrentTimestamp();

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
  f.write((uint8_t *)&cvTuningLogCount, sizeof(cvTuningLogCount));
  f.write((uint8_t *)&cvTuningLogHead, sizeof(cvTuningLogHead));
  f.write((uint8_t *)&cvTuningRunCounter, sizeof(cvTuningRunCounter));
  f.write((uint8_t *)cvTuningLog, 50 * sizeof(CVTuningRecord));
  f.close();
}

void loadCVTuningLog() {
  if (!cvTuningLog) return;
  File f = LittleFS.open("/cvtuninglog.bin", "r");
  if (!f) return;
  // Struct size is part of the wire format. Bail cleanly on mismatch (e.g. after
  // CVTuningRecord layout changed) instead of reading garbage into the array.
  const size_t expected = sizeof(cvTuningLogCount) + sizeof(cvTuningLogHead)
                          + sizeof(cvTuningRunCounter) + 50 * sizeof(CVTuningRecord);
  if (f.size() != expected) {
    Serial.printf("CVTuningLog: size mismatch (%u vs %u expected) — discarding old log\n",
                  (unsigned)f.size(), (unsigned)expected);
    f.close();
    LittleFS.remove("/cvtuninglog.bin");
    return;
  }
  f.read((uint8_t *)&cvTuningLogCount, sizeof(cvTuningLogCount));
  f.read((uint8_t *)&cvTuningLogHead, sizeof(cvTuningLogHead));
  f.read((uint8_t *)&cvTuningRunCounter, sizeof(cvTuningRunCounter));
  f.read((uint8_t *)cvTuningLog, 50 * sizeof(CVTuningRecord));
  f.close();
  // Corrupt count/head would index past the 50-slot ring (commit writes BEFORE
  // the %50 wrap) — discard rather than trust a bad header.
  if (cvTuningLogCount > 50 || cvTuningLogHead >= 50) {
    Serial.println("CVTuningLog: corrupt count/head — discarding old log");
    cvTuningLogCount = 0;
    cvTuningLogHead = 0;
    LittleFS.remove("/cvtuninglog.bin");
    return;
  }
  Serial.printf("CVTuningLog: loaded %d records, counter=%d\n", cvTuningLogCount, cvTuningRunCounter);
}

// TODO (future): the CV tuning score (ISE/T, settling, overshoot) currently does NOT
// distinguish runs taken with testProtectionsEnabled=true vs =false. Scores under
// protections-on are loop+protection composite; under protections-off they are pure
// loop. Comparing them directly is misleading. Future redo: tag each record with the
// flag state at the time of the run so the UI can group/filter, and rethink whether
// fastOvFires/iExcessFires/loadDumpFires belong in the score formula when protections
// were disabled (they can't fire then, so the counts will always be zero).
void commitCVTuningRecord() {
  if (!cvTuningLog || cvTuningScore.scoredHighCount < 1) {
    cvTuningScore = {};
    return;
  }
  CVTuningRecord rec = {};
  float n = (float)cvTuningScore.scoredHighCount;
  rec.runNumber = ++cvTuningRunCounter;
  rec.avgSettlingTimeSec = cvTuningScore.totalSettlingTimeSec / n;
  rec.avgIntegratedOvershootVs = cvTuningScore.totalIntegratedOvershootVs / n;
  rec.worstOvershootV = cvTuningScore.worstOvershootV;
  rec.activeTimeSec = cvTuningScore.activeTimeSec;
  // ISE/T: (HIGH ISE + LOW re-overshoot ISE + LOW undershoot ISE) ÷ total active time, ×1000.
  // HIGH overshoot above 25mV dead-band weighted ×4; LOW undershoot weighted ×0.15 with time ramp.
  rec.score = (rec.activeTimeSec > 0.0f)
                ? (1000.0f * (cvTuningScore.totalIntegratedOvershootVs + cvTuningScore.totalLowIntOvVs + cvTuningScore.totalLowUndershootVs)
                   / rec.activeTimeSec)
                : 0.0f;
  rec.fastOvFires = cvTuningScore.fastOvFires;
  rec.iExcessFires = cvTuningScore.iExcessFires;
  rec.loadDumpFires = cvTuningScore.loadDumpFires;
  rec.hardOcFires = cvTuningScore.hardOcFires;
  rec.voltageKp = VoltageKp_active;  // gain actually in effect (Manual or Auto-λ, normalized)
  rec.voltageKi = VoltageKi_active;
  rec.voltageKd = 0.0f;  // D term removed; field kept for struct layout compatibility
  rec.setpointRiseRate = SetpointRiseRate;
  rec.setpointFallRate = SetpointFallRate;
  rec.awBleedRate = AwBleedRate;
  rec.awRecoverRate = AwRecoverRate;
  rec.awSeedProtectMs = AwSeedProtectMs;
  rec.reseedFrac = ReseedFrac;
  rec.slopeBleedK = SlopeBleedK;
  rec.kHard = KHard;
  rec.iExcessFrac = IExcessFrac;
  rec.iExcessTau = IExcessTau;
  rec.iExcessKBleed = IExcessKBleed;
  rec.loadDumpDtThresh = LoadDumpDtThresh;
  rec.loadDumpDtThresh1 = LoadDumpDtThresh1;
  rec.loadDumpDtThresh3 = LoadDumpDtThresh3;
  rec.inputFilterTC = InputFilterTC;
  rec.waveAmplitudeV = cvWaveAmplitudeV;
  rec.wavePeriodSec = (uint16_t)cvWavePeriodSec;
  rec.kOvershoot = cvKOvershoot;
  rec.consecutiveReads = cvConsecutiveReads;
  rec.avgRPM = (cvTuningScore.avgSampleCount > 0) ? (cvTuningScore.rpmSum / cvTuningScore.avgSampleCount) : 0.0f;
  rec.avgAltTempF = (cvTuningScore.avgSampleCount > 0) ? (cvTuningScore.tempSum / cvTuningScore.avgSampleCount) : 0.0f;
  rec.battVAtStart = cvTuningScore.battVAtStart;
  rec.socAtStart = cvTuningScore.socAtStart;
  rec.chargingVoltageTarget = cvBaseTarget;
  rec.chargeStage = getChargeStageDisplayCode();
  rec.epoch = getCurrentTimestamp();
  float nl = (float)cvTuningScore.scoredLowCount;
  if (nl > 0.0f) {
    rec.avgLowSettlingTimeSec = cvTuningScore.totalLowSettlingTimeSec / nl;
    rec.avgLowIntOvVs = cvTuningScore.totalLowIntOvVs / nl;
  } else {
    rec.avgLowSettlingTimeSec = 0.0f;
    rec.avgLowIntOvVs = 0.0f;
  }
  rec.worstLowOvV = cvTuningScore.worstLowOvV;
  rec.worstLowUndershootV = cvTuningScore.worstLowUndershootV;
  rec.lowScore = (rec.activeTimeSec > 0.0f)
                   ? (1000.0f * (cvTuningScore.totalLowIntOvVs + cvTuningScore.totalLowUndershootVs)
                      / rec.activeTimeSec)
                   : 0.0f;

  cvTuningLog[cvTuningLogHead] = rec;
  cvTuningLogHead = (cvTuningLogHead + 1) % 50;
  if (cvTuningLogCount < 50) cvTuningLogCount++;

  saveCVTuningLog();
  queueConsoleMessageF("CVTuningScore: run#%d score=%.2f settle=%.1fs overshoot=%.3fV n=%d",
                       rec.runNumber, rec.score, rec.avgSettlingTimeSec, rec.worstOvershootV, (int)n);
  cvTuningScore = {};
}

// ===== Control Accuracy Scores — accumulate-since-reset engine (2026-06-18) =====
// One running RMS-error accumulator + one worst-overshoot high-water mark per loop. No bucket ring,
// no window rotation. The accumulators are zeroed by resetAccuracyScores() — fired automatically
// right after each successful config-snapshot upload (≈daily) and by the manual dashboard button.

// RMS tracking error in physical units = sqrt(Σ(e²·dt) / Σdt). Guard avoids a divide before any
// authority time has accrued. Takes the raw double accumulators (NOT the struct) so the auto-
// generated cross-file prototype doesn't reference AccuracyScore before it's defined. Returns float.
float accScoreRms(double errAccum, double timeAccum) {
  return (timeAccum > 0.1) ? sqrtf((float)(errAccum / timeAccum)) : 0.0f;
}

// Add one tick of error + overshoot. err/dtSec are this tick's tracking error and elapsed time;
// overshoot is the excursion in the damaging direction (already floored at 0 by the caller).
// Takes the accumulator members by primitive reference (not the AccuracyScore struct) so Arduino's
// auto-generated prototype — inserted above the struct's definition — references only built-in types.
static void accScoreAdd(double &errAccum, double &timeAccum, float &worstOver,
                        float err, float overshoot, float dtSec) {
  errAccum  += (double)err * (double)err * (double)dtSec;
  timeAccum += (double)dtSec;
  if (overshoot > worstOver) worstOver = overshoot;
}

// Settle/debounce gate: returns true only once the loop's authority condition has held continuously
// for settleMs. The false→true edge stamps bindingStartMs; any false tick clears it (restart timer).
static bool accBindingReady(uint32_t &bindingStartMs, bool binding, uint32_t nowMs, uint32_t settleMs) {
  if (!binding) { bindingStartMs = 0; return false; }
  if (bindingStartMs == 0) bindingStartMs = nowMs;
  return (uint32_t)(nowMs - bindingStartMs) >= settleMs;
}

// Zero all three loops' accumulators (RMS sums, worst-overshoot, settle timers). Called by the
// /resetAccuracyScores button handler and automatically after each config-snapshot upload.
void resetAccuracyScores() {
  accCurrent = {};
  accVoltage = {};
  accThermal = {};
}

// ── SystemID (plant-delay) ring buffer log — mirrors tuning logs above ────
void saveSystemIDLog() {
  if (!systemIDLog) return;
  File f = LittleFS.open("/systemidlog.bin", "w");
  if (!f) return;
  f.write((uint8_t *)&systemIDLogCount,   sizeof(systemIDLogCount));
  f.write((uint8_t *)&systemIDLogHead,    sizeof(systemIDLogHead));
  f.write((uint8_t *)&systemIDRunCounter, sizeof(systemIDRunCounter));
  f.write((uint8_t *)systemIDLog,         50 * sizeof(SystemIDRecord));
  f.close();
}

void loadSystemIDLog() {
  if (!systemIDLog) return;
  File f = LittleFS.open("/systemidlog.bin", "r");
  if (!f) return;
  // Struct size is part of the wire format. Bail cleanly on mismatch (e.g. after
  // SystemIDRecord layout changed) instead of reading garbage into the array.
  const size_t expected = sizeof(systemIDLogCount) + sizeof(systemIDLogHead)
                          + sizeof(systemIDRunCounter) + 50 * sizeof(SystemIDRecord);
  if (f.size() != expected) {
    Serial.printf("SystemIDLog: size mismatch (%u vs %u expected) — discarding old log\n",
                  (unsigned)f.size(), (unsigned)expected);
    f.close();
    LittleFS.remove("/systemidlog.bin");
    return;
  }
  f.read((uint8_t *)&systemIDLogCount,   sizeof(systemIDLogCount));
  f.read((uint8_t *)&systemIDLogHead,    sizeof(systemIDLogHead));
  f.read((uint8_t *)&systemIDRunCounter, sizeof(systemIDRunCounter));
  f.read((uint8_t *)systemIDLog,         50 * sizeof(SystemIDRecord));
  f.close();
  // Corrupt count/head would index past the 50-slot ring (commit writes BEFORE
  // the %50 wrap) — discard rather than trust a bad header.
  if (systemIDLogCount > 50 || systemIDLogHead >= 50) {
    Serial.println("SystemIDLog: corrupt count/head — discarding old log");
    systemIDLogCount = 0;
    systemIDLogHead = 0;
    LittleFS.remove("/systemidlog.bin");
    return;
  }
  Serial.printf("SystemIDLog: loaded %d records, counter=%d\n",
                systemIDLogCount, systemIDRunCounter);
}

// Push the current SystemID result globals into the ring buffer.
// `aborted == true` means the test never produced rise/fall numbers — we
// still record so users see the abort in the history.
void commitSystemIDRecord(bool aborted) {
  if (!systemIDLog) return;

  SystemIDRecord rec = {};
  rec.runNumber          = ++systemIDRunCounter;
  rec.setupStepAmplitude = SystemIDStepAmplitude;
  rec.abortReason        = systemIDAbortReason;
  rec.abortPhase         = systemIDAbortPhase;
  // Conditions snapshot at commit (no per-test accumulator exists for SystemID)
  rec.avgRPM       = (float)RPM;
  rec.avgAltTempF  = isnan(AlternatorTemperatureF) ? 0.0f : AlternatorTemperatureF;
  rec.battV        = BatteryV;
  rec.chargeStage  = getChargeStageDisplayCode();
  rec.epoch        = getCurrentTimestamp();

  if (aborted) {
    rec.score = -1.0f;
    rec.riseAvg_ms = -1.0f;
    rec.fallAvg_ms = -1.0f;
    for (int i = 0; i < 3; i++) {
      rec.riseDelays[i] = -1.0f;
      rec.fallDelays[i] = -1.0f;
      rec.stepAmps[i]   = 0.0f;
      rec.quietPP[i]    = 0.0f;
    }
  } else {
    // Score = rise average (lower = faster plant). If rise didn't measure, fall back to fall.
    rec.score = (systemIDRiseAvg_ms > 0.0f) ? systemIDRiseAvg_ms
              : (systemIDFallAvg_ms > 0.0f) ? systemIDFallAvg_ms
              : -1.0f;
    rec.riseAvg_ms = systemIDRiseAvg_ms;
    rec.fallAvg_ms = systemIDFallAvg_ms;
    for (int i = 0; i < 3; i++) {
      rec.riseDelays[i] = systemIDRiseDelay_ms[i];
      rec.fallDelays[i] = systemIDFallDelay_ms[i];
      rec.stepAmps[i]   = systemIDStepAmp_A[i];
      rec.quietPP[i]    = systemIDQuietPP_A[i];
    }
  }

  systemIDLog[systemIDLogHead] = rec;
  systemIDLogHead = (systemIDLogHead + 1) % 50;
  if (systemIDLogCount < 50) systemIDLogCount++;

  pendingSaveSystemIDLog = true;  // deferred to Core 1 — avoids blocking Core 0

  queueConsoleMessageF("SystemID: run#%d %s rise=%.0f fall=%.0f stepAmp=%.1f%%",
                       rec.runNumber,
                       aborted ? "ABORTED" : "logged",
                       rec.riseAvg_ms, rec.fallAvg_ms, rec.setupStepAmplitude);
}

void saveSysidSweepLog() {
  if (!sysidSweepLog) return;
  File f = LittleFS.open("/sysidsweeplog.bin", "w");
  if (!f) return;
  f.write((uint8_t *)&sysidSweepLogCount,   sizeof(sysidSweepLogCount));
  f.write((uint8_t *)&sysidSweepLogHead,    sizeof(sysidSweepLogHead));
  f.write((uint8_t *)&sysidSweepRunCounter, sizeof(sysidSweepRunCounter));
  f.write((uint8_t *)sysidSweepLog,         50 * sizeof(SysIDSweepRecord));
  f.close();
}

void loadSysidSweepLog() {
  if (!sysidSweepLog) return;
  File f = LittleFS.open("/sysidsweeplog.bin", "r");
  if (!f) return;
  const size_t expected = sizeof(sysidSweepLogCount) + sizeof(sysidSweepLogHead)
                          + sizeof(sysidSweepRunCounter) + 50 * sizeof(SysIDSweepRecord);
  if (f.size() != expected) {
    Serial.printf("SysidSweepLog: size mismatch (%u vs %u expected) — discarding old log\n",
                  (unsigned)f.size(), (unsigned)expected);
    f.close();
    LittleFS.remove("/sysidsweeplog.bin");
    return;
  }
  f.read((uint8_t *)&sysidSweepLogCount,   sizeof(sysidSweepLogCount));
  f.read((uint8_t *)&sysidSweepLogHead,    sizeof(sysidSweepLogHead));
  f.read((uint8_t *)&sysidSweepRunCounter, sizeof(sysidSweepRunCounter));
  f.read((uint8_t *)sysidSweepLog,         50 * sizeof(SysIDSweepRecord));
  f.close();
  if (sysidSweepLogCount > 50 || sysidSweepLogHead >= 50) {
    Serial.println("SysidSweepLog: corrupt count/head — discarding old log");
    sysidSweepLogCount = 0;
    sysidSweepLogHead = 0;
    LittleFS.remove("/sysidsweeplog.bin");
    return;
  }
  Serial.printf("SysidSweepLog: loaded %d records, counter=%d\n",
                sysidSweepLogCount, sysidSweepRunCounter);
}

void commitSysidSweepRecord() {
  if (!sysidSweepLog) return;
  if (systemIDBodeCount < 2) return;

  SysIDSweepRecord rec = {};
  rec.runNumber      = ++sysidSweepRunCounter;
  rec.setupAmplitude = SystemIDStepAmplitude;
  rec.stabilizeAmps  = SystemIDStabilizeAmps;
  rec.sweepStartHz   = systemIDSineFreqStart;
  rec.sweepEndHz     = systemIDSineFreqEnd;
  rec.cycles         = systemIDSineCycles;
  rec.nPoints        = systemIDBodeCount;
  rec.avgRPM         = (float)RPM;
  rec.avgAltTempF    = isnan(AlternatorTemperatureF) ? 0.0f : AlternatorTemperatureF;
  rec.battV          = BatteryV;
  rec.chargeStage    = getChargeStageDisplayCode();
  rec.epoch          = getCurrentTimestamp();

  for (int i = 0; i < systemIDBodeCount && i < SYSID_SINE_NPOINTS; i++)
    rec.curve[i] = systemIDBode[i];

  float dcGain = systemIDBode[0].gainApPct;
  rec.dcGainApPct = dcGain;
  rec.rolloffHz = -1.0f;
  if (dcGain > 0.0f) {
    float thr = 0.707f * dcGain;
    rec.rolloffHz = systemIDBode[systemIDBodeCount - 1].freqHz;  // no roll-off in range
    for (int i = 1; i < systemIDBodeCount; i++) {
      if (systemIDBode[i].gainApPct < thr) {
        float f0 = systemIDBode[i - 1].freqHz, g0 = systemIDBode[i - 1].gainApPct;
        float f1 = systemIDBode[i].freqHz,     g1 = systemIDBode[i].gainApPct;
        // Log-frequency interpolation of the crossing (sweep points are log-spaced) — matches the
        // dashboard FOPDT fit so the live readout and this logged value agree.
        float t = (g0 - thr) / (g0 - g1);
        rec.rolloffHz = (g0 != g1) ? expf(logf(f0) + (logf(f1) - logf(f0)) * t) : f0;
        break;
      }
    }
  }
  float wp = 0.0f, wpf = 0.0f;
  for (int i = 0; i < systemIDBodeCount; i++) {
    float ap = fabsf(systemIDBode[i].phaseDeg);
    if (ap > wp) { wp = ap; wpf = systemIDBode[i].freqHz; }
  }
  rec.worstPhaseDeg = wp;
  rec.worstPhaseFreqHz = wpf;

  sysidSweepLog[sysidSweepLogHead] = rec;
  sysidSweepLogHead = (sysidSweepLogHead + 1) % 50;
  if (sysidSweepLogCount < 50) sysidSweepLogCount++;
  pendingSaveSysidSweepLog = true;  // deferred to Core 1

  queueConsoleMessageF("SysID sweep: run#%d rolloff=%.1fHz dcGain=%.3f worstLag=%.0fdeg",
                       rec.runNumber, rec.rolloffHz, rec.dcGainApPct, rec.worstPhaseDeg);
}

void saveTuningSweepLog() {
  if (!tuningSweepLog) return;
  File f = LittleFS.open("/tuningsweeplog.bin", "w");
  if (!f) return;
  f.write((uint8_t *)&tuningSweepLogCount,   sizeof(tuningSweepLogCount));
  f.write((uint8_t *)&tuningSweepLogHead,    sizeof(tuningSweepLogHead));
  f.write((uint8_t *)&tuningSweepRunCounter, sizeof(tuningSweepRunCounter));
  f.write((uint8_t *)tuningSweepLog,         50 * sizeof(TuningSweepRecord));
  f.close();
}

void loadTuningSweepLog() {
  if (!tuningSweepLog) return;
  File f = LittleFS.open("/tuningsweeplog.bin", "r");
  if (!f) return;
  const size_t expected = sizeof(tuningSweepLogCount) + sizeof(tuningSweepLogHead)
                          + sizeof(tuningSweepRunCounter) + 50 * sizeof(TuningSweepRecord);
  if (f.size() != expected) {
    Serial.printf("TuningSweepLog: size mismatch (%u vs %u expected) — discarding old log\n",
                  (unsigned)f.size(), (unsigned)expected);
    f.close();
    LittleFS.remove("/tuningsweeplog.bin");
    return;
  }
  f.read((uint8_t *)&tuningSweepLogCount,   sizeof(tuningSweepLogCount));
  f.read((uint8_t *)&tuningSweepLogHead,    sizeof(tuningSweepLogHead));
  f.read((uint8_t *)&tuningSweepRunCounter, sizeof(tuningSweepRunCounter));
  f.read((uint8_t *)tuningSweepLog,         50 * sizeof(TuningSweepRecord));
  f.close();
  if (tuningSweepLogCount > 50 || tuningSweepLogHead >= 50) {
    Serial.println("TuningSweepLog: corrupt count/head — discarding old log");
    tuningSweepLogCount = 0;
    tuningSweepLogHead = 0;
    LittleFS.remove("/tuningsweeplog.bin");
    return;
  }
  Serial.printf("TuningSweepLog: loaded %d records, counter=%d\n",
                tuningSweepLogCount, tuningSweepRunCounter);
}

void commitTuningSweepRecord() {
  if (!tuningSweepLog) return;
  if (tuningBodeCount < 2) return;

  TuningSweepRecord rec = {};
  rec.runNumber    = ++tuningSweepRunCounter;
  rec.kp = PidKp; rec.ki = PidKi; rec.kd = PidKd;
  rec.sweepStartHz = tuningSweepStart;
  rec.sweepEndHz   = tuningSweepEnd;
  rec.cycles       = tuningSweepCycles;
  rec.nPoints      = tuningBodeCount;
  rec.avgRPM       = (float)RPM;
  rec.avgAltTempF  = isnan(AlternatorTemperatureF) ? 0.0f : AlternatorTemperatureF;
  // Run-condition snapshot — waveform params + how trustworthy the sweep was.
  rec.sineAmpA       = tuningSweepAmpA;
  rec.baseA          = tuningSweepBaseA;
  rec.battV          = tuningSweepBattV;
  rec.rpmMin         = tuningSweepRpmMin;
  rec.rpmMax         = tuningSweepRpmMax;
  rec.worstCoherence = tuningSweepWorstCoh;
  rec.dutyRailed     = tuningSweepDutyRailed ? 1 : 0;
  rec.chargeStage    = getChargeStageDisplayCode();
  rec.epoch          = getCurrentTimestamp();

  for (int i = 0; i < tuningBodeCount && i < TUNING_SWEEP_NPOINTS; i++)
    rec.curve[i] = tuningBode[i];

  // -3 dB bandwidth from the low end.
  rec.bandwidthHz = -1.0f;
  if (tuningBode[0].gain >= 0.707f) {
    rec.bandwidthHz = tuningBode[tuningBodeCount - 1].freqHz;  // tracks full range
    for (int i = 1; i < tuningBodeCount; i++) {
      if (tuningBode[i].gain < 0.707f) {
        float f0 = tuningBode[i - 1].freqHz, g0 = tuningBode[i - 1].gain;
        float f1 = tuningBode[i].freqHz,     g1 = tuningBode[i].gain;
        // Log-frequency interpolation of the crossing (sweep points are log-spaced) — matches the
        // dashboard FOPDT fit so the live readout and this logged value agree.
        float t = (g0 - 0.707f) / (g0 - g1);
        rec.bandwidthHz = (g0 != g1) ? expf(logf(f0) + (logf(f1) - logf(f0)) * t) : f0;
        break;
      }
    }
  }
  float pk = 0.0f, pkf = 0.0f, wp = 0.0f, wpf = 0.0f;
  for (int i = 0; i < tuningBodeCount; i++) {
    if (tuningBode[i].gain > pk) { pk = tuningBode[i].gain; pkf = tuningBode[i].freqHz; }
    float ap = fabsf(tuningBode[i].phaseDeg);
    if (ap > wp) { wp = ap; wpf = tuningBode[i].freqHz; }
  }
  rec.peakGain = pk; rec.peakGainFreqHz = pkf;
  rec.worstPhaseDeg = wp; rec.worstPhaseFreqHz = wpf;

  tuningSweepLog[tuningSweepLogHead] = rec;
  tuningSweepLogHead = (tuningSweepLogHead + 1) % 50;
  if (tuningSweepLogCount < 50) tuningSweepLogCount++;
  pendingSaveTuningSweepLog = true;  // deferred to Core 1

  queueConsoleMessageF("Tuning sweep: run#%d BW=%.1fHz peak=%.2f worstLag=%.0fdeg",
                       rec.runNumber, rec.bandwidthHz, rec.peakGain, rec.worstPhaseDeg);
}

// Called from tempPID_tick() on every tick (16 Hz). Feeds the Thermal Control
// Accuracy live score (accumulate-since-reset), authority-gated.
void thermalAccuracyScore_tick(uint32_t nowMs, float dtSec) {
  // ===== Thermal Control Accuracy score (accumulate-since-reset) =====
  // Authority gate: only score when the thermal loop is the binding constraint with no other
  // limiter in charge. The actuator here is the penalty-amps derate; a SUSTAINED penalty IS the
  // definition of "thermal is controlling" — the REVERSE PID floors penalty at 0 when cool, so a
  // penalty held > 2A for 120 s (ACC_SETTLE_THERMAL_MS) can only happen while the loop is actively
  // holding temperature at the limit. That sustained-penalty requirement is why g_I_cap > 10A is no
  // longer needed: penalty can't stay up without sustained current, which needs adequate RPM.
  //   voltage-binding stage  — absorption/float/TargetVoltage: there the CV loop pulls current to
  //                           hold voltage, so thermal error no longer reflects thermal control
  //                           quality. 3-min blanking after it clears (sustained bias). BULK is NOT
  //                           blanked: bulk is current-limited at the RPM/thermal ceiling, so
  //                           thermal IS the binding constraint and we DO want to score it.
  //   g_fastOvClampActive   — OV supervisor cutting current for voltage, not thermal
  //   MaintainMode          — output forced to zero; thermal loop does nothing
  //   thermalPenaltyAmps    — must be > 2A (the binding-constraint signal; see above)
  bool voltageBindingStage = voltageControlActive && (getChargeStageDisplayCode() != CHARGE_STAGE_BULK);
  if (voltageBindingStage) thermalScoreLastExternalMs = nowMs;
  bool thermalBinding = tempPIDActive && thermalSlopeBufFull && !isnan(tempFiltered)
                        && !g_fastOvClampActive && (MaintainMode == 0) && thermalPenaltyAmps > 2.0f
                        && (uint32_t)(nowMs - thermalScoreLastExternalMs) > 180000UL;
  if (accBindingReady(accThermal.bindingStartMs, thermalBinding, nowMs, ACC_SETTLE_THERMAL_MS)) {
    float err  = tempFiltered - TemperatureLimitF;              // °F; positive = over the limit
    float over = err > 0.0f ? err : 0.0f;                       // over-temp side (alternator-damaging)
    accScoreAdd(accThermal.errAccum, accThermal.timeAccum, accThermal.worstOver, err, over, dtSec);
  }
}

void AdjustFieldLearnMode() {

  // ========== TIMING ==========
  static uint32_t lastControlTickMs = 0;
  uint32_t currentMillis = millis();
  uint32_t aflT0 = micros();  // section profiler entry mark (see aflWorstSecUs globals)

  // Thermal log runs FIRST so temperature history accumulates regardless of mode.
  // Every early-return below (immediate cut, LimpHome, stale CH1, non-normal mode,
  // sysID, cloud-busy hold) used to skip the log; hoisting fixes that. Internal
  // 1 Hz throttle keeps cadence. Control-side fields (pidOut, outerTermP/I/D,
  // cv_I) freeze when their owners aren't ticking; temperature fields keep moving.
  thermalLog_tick(currentMillis);
  uint32_t aflM0 = micros();  // end of section 0: thermal log

  uint32_t actualDtMs = (lastControlTickMs == 0) ? 62 : (currentMillis - lastControlTickMs);
  if (actualDtMs > 500) actualDtMs = 500;
  if (actualDtMs == 0) actualDtMs = 1;  // floor prevents 0-div in slew calcs when tick runs twice in same ms
  float actualDtSec = (float)actualDtMs / 1000.0f;

  static float uTargetRaw_cached = 50.0f;   // always MaxTableValue (assigned from uTargetRaw); seeds fastOvBaseCap (next line) and the supervisorLimiting gate
  float uTargetRaw = (float)MaxTableValue;  // always MaxTableValue; kept for uTargetRaw_cached lineage only
  float fastOvBaseCap = clamp_f(uTargetRaw_cached, 0.0f, (float)MaxTableValue);
  float fastOvCurrentCap = fastOvBaseCap;
  // Binding-reason tracker — LOCAL through the tick, exported to g_fastOvCapReason
  // alongside g_fastOvCurrentCap after all supervisors. Must NOT write the global
  // directly here: the iExcess/LoadDump supervisors sit BELOW the fresh-CH1 early
  // return, so a global reset on every no-CH1 pass wiped their reason while the
  // stale cap stayed exported — the CV log showed iExcess-lowered caps with
  // capReason=0 forever (found 2026-06-10, cvlog_20260610_2115).
  uint8_t capReasonTick = CAP_REASON_NONE;
  bool fastOvClampActive = false;
  static uint32_t ocTripStartMs = 0;

  updateCurrentRPMTableIndex(RPM);
  updateRPMBucketHistory(currentMillis);

  TickSnapshot tick = buildTickSnapshot(currentMillis, actualDtMs);
  uint32_t aflM1 = micros();  // end of section 1: RPM tables + tick snapshot
  // pidLog_tick() runs at the END of the normal control path, after all state is final.
  // thermalLog_tick() runs at the TOP of this function (above) — unconditional,
  // so temperature history continues during off/fault/shutdown/sysID.

  // tempFilterUpdate() hoisted here (2026-06-26) so it runs on EVERY tick in EVERY mode,
  // not just SYS_MODE_AUTO. It used to live in the AUTO branch below — but an over-temp
  // event drops the system to SYS_MODE_FAULT to cut the field, which skips that branch,
  // so the filtered temp / slope / projection froze for the whole cooldown and the plot
  // and thermal log showed a dead-flat line while the alternator was actually cooling.
  // TempToUse is fresh here (buildTickSnapshot above mirrors tick.tempToUseF to it). Pure
  // display/estimator math — no PID/field/persisted-state side effects — and it still runs
  // BEFORE tempPID_tick (in the AUTO branch), so the PID input stays as fresh as before.
  // Same fix class as the 2026-06-23 hoist out of tempPID_tick that killed the TuningMode freeze.
  tempFilterUpdate(currentMillis);

  isTempSustainedWarning(tick.nowMs, tick.tempToUseF, tick.tempLimitF,
                         tick.tempWarnExcessF, tick.ignoreTemperature);

  if (innerPIDResetRequested) {
    innerPIDResetRequested = false;
    currentPID.SetMode(MANUAL);
    pidOutput = 0.0;
    currentPID.ResetIntegratorTo(0.0);
    lastAppliedDuty = 0.0f;
    setpointInitialized = false;
    // Restore AUTOMATIC so Compute() resumes; otherwise the UI "Reset Inner PID"
    // button leaves the PID stuck in MANUAL until charging is cycled off/on.
    // Only restore if sysMode is AUTO — MANUAL/FAULT/OFF should keep PID in MANUAL.
    // SetMode(AUTOMATIC) triggers library Initialize() which re-confirms a clean state
    // (outputSum=pidOutput=0, lastUnsatOutput=0) consistent with the reset above.
    if (sysMode == SYS_MODE_AUTO) {
      currentPID.SetMode(AUTOMATIC);
    }
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

  // Engine confirmed stopped (RPM held at 0 for >= RPM_ZERO_CUT_MS): cut the field
  // immediately, overriding any graceful shutdown ramp. CHARGING_DISABLED (priority 1a)
  // outranks the RPM gate, so on a normal key-off + engine-stop the field would otherwise
  // slow-ramp for ~30s while still energized — coupling PWM noise into the LM2907 RPM sense
  // (phantom RPM spikes). Placed pre-CH1-gate so it fires at full loop rate regardless of
  // current-sensor freshness. Forced reason RPM_TOO_LOW so the log/telemetry name the cause.
  if (tick.engineFullyStopped && !gpio4IsLow) {
    applyImmediateCut(tick, REASON_RPM_TOO_LOW);
    return;
  }


  // ===== FAST VOLTAGE SAFETY OVERRIDE ==========
  // Runs every loop before the CH1 gate.
  // Computes fastOvCurrentCap — a per-tick hard ceiling on commanded current.
  // Applied to uTargetAmps after RPM/thermal/user overrides in the AUTO path.
  // Direct cv_I clamp kept here because the CV loop only runs every 100ms;
  // without it cv_I builds positive for up to 100ms while battV is above target.

  // Pre-event cv_I snapshot for the protection-release reseed (rationale: CV_Loop_Dev_Summary.md).
  // Timing note: g_fastOvClampActive here holds LAST tick's final value — its update was moved to
  // the end of the bumpless block, after every supervisor has voted, so this read reflects the
  // unified flag.
  static float preEventCvI = 0.0f;
  if (!g_fastOvClampActive) {
    preEventCvI = cv_I;  // refresh while no protection is clamping
  }
  // Post-protection fast-rise window — opened by the falling-edge handler below.
  // 0 = window inactive. Gated again at slew site by FastSetpointRiseWindowMs cap
  // and FastSetpointRiseHeadroomV vs ChargingVoltageTarget.
  static uint32_t postProtectRiseStartMs = 0;

  {
    static float vPrev = 0.0f;
    static uint32_t vPrevMs = 0;
    static float dvdt = 0.0f;
    static bool ovActive = false;

    g_fastOvHardActive = false;
    g_fastOvVpred = IBV;

    if (ibvFreshFlag) {
      ibvFreshFlag = false;
      if (vPrevMs > 0) {
        uint32_t dtMs = currentMillis - vPrevMs;
        if (dtMs >= 1 && dtMs <= 100) {
          float raw = (IBV - vPrev) / ((float)dtMs * 0.001f);  // V/s
          float alpha = (float)dtMs / (DvdtTC + (float)dtMs);   // dt-aware EMA: TC stays constant across sample-rate jitter
          dvdt = alpha * raw + (1.0f - alpha) * dvdt;
        }
      }
      vPrev = IBV;
      vPrevMs = currentMillis;
    }
    g_fastOvDvdt = dvdt;

    if (voltageControlActive) {  // Groups 1/2 target-relative; gated only on voltageControlActive
      const float TD_PRED = TdPred;
      const float V_HARD = ChargingVoltageTarget + OvPredMarginV;
      const float PRED_GUARD = 0.06f;

      float Vpred = IBV + TD_PRED * fmaxf(0.0f, dvdt);
      g_fastOvVpred = Vpred;

      // (pre-event cv_I capture now centralised in preEventCvI above —
      //  this block no longer maintains its own snapshot.)

      // Test-mode bypass: when testProtectionsEnabled is false (user toggled it off on a
      // tuning page) OR TuningMode is active (current-waveform step test), G1 and G2 are
      // inhibited from firing so a step-test can characterise the plant without protection
      // layers fighting the input. The release condition below is not gated — if ovActive
      // was already set before the user disabled, it can still de-assert cleanly.
      if (testProtectionsEnabled && !TuningMode && IBV > ChargingVoltageTarget - PRED_GUARD) {
        if (OvGroup1Enable && Vpred > V_HARD) {
          float hardCap = fmaxf(0.0f, setpointLimited - KHard * (Vpred - V_HARD));
          // record reason only when this layer actually lowers the cap (equiv. to fminf)
          if (hardCap < fastOvCurrentCap) { fastOvCurrentCap = hardCap; capReasonTick = CAP_REASON_KHARD_G1; }
          fastOvClampActive = true;
          g_fastOvHardActive = true;
        }
      }

      if (testProtectionsEnabled && !TuningMode && OvGroup2Enable && IBV > ChargingVoltageTarget + OvMeasMarginV) {
        float ovExcess = IBV - (ChargingVoltageTarget + OvMeasMarginV);
        float hystCap = fmaxf(0.0f, setpointLimited - KHard * ovExcess);
        if (hystCap < fastOvCurrentCap) { fastOvCurrentCap = hystCap; capReasonTick = CAP_REASON_KHARD_G2; }
        fastOvClampActive = true;
        ovActive = true;
        g_fastOvHardActive = true;
      }
      // Hysteresis-band hold: keep the Group 2 clamp continuous while ovActive is latched
      // but IBV has dipped below the firing threshold (between target+OvMeasMarginV and target).
      // Without this, fastOvClampActive drops in the band, setpointLimited recovers at
      // SetpointRiseRate, voltage rises, Group 2 re-fires — producing on/off flicker.
      // Softer reference (IBV - target) so the cap relaxes linearly as voltage falls toward
      // the release point. No g_fastOvHardActive — this is the soft hold, not a fresh fire.
      else if (testProtectionsEnabled && !TuningMode && OvGroup2Enable && ovActive && IBV > ChargingVoltageTarget) {
        float ovExcessSoft = IBV - ChargingVoltageTarget;
        float hystCap = fmaxf(0.0f, setpointLimited - KHard * ovExcessSoft);
        if (hystCap < fastOvCurrentCap) { fastOvCurrentCap = hystCap; capReasonTick = CAP_REASON_KHARD_G2; }
        fastOvClampActive = true;
      }

      // Group 1/2 release condition: battV back at/under target AND prediction safe.
      // cv_I reseed itself is now handled by the unified falling-edge reseed in the
      // bumpless tracker block — fires only when ALL protection paths (G1/2, iExcess,
      // LoadDump) have cleared, using the single preEventCvI snapshot.
      if (ovActive
          && (IBV <= ChargingVoltageTarget)
          && (Vpred <= V_HARD)) {
        ovActive = false;
      }
    } else {
      ovActive = false;  // !voltageControlActive (idle only — MaintainMode now sets voltageControlActive=true)
    }
  }

  // ── Group 1/2 hard-OV telemetry export ────────────────────────────────────
  // g_fastOvHardActive is set only by Group 1 (predictive K_HARD) and Group 2
  // (measurement hysteresis) in the FAST OV block above — iExcess and LoadDump
  // never raise it. So this rising-edge counter and the inner-PID integrator
  // reset belong here, right after the Group 1/2 supervisor finishes.
  // The unified telemetry (g_fastOvClampActive, g_fastOvClampCount, g_fastOvCurrentCap)
  // is exported later in the bumpless tracker block, after iExcess and LoadDump
  // have also had a chance to vote — otherwise it would only reflect Group 1/2.
  static bool g_fastOvHardActive_prev = false;
  if (g_fastOvHardActive && !g_fastOvHardActive_prev) {
    g_fastOvHardCount++;  // rising-edge only — count each new hard FastOV activation
    // Collapse inner PID integrator on hard OV onset. fastOvCap already drives
    // setpoint to near-zero on this tick; without this, a wound-up integrator
    // resists the setpoint collapse and grinds duty down at ~40%/s instead of
    // reaching MinDuty in 1–2 inner PID cycles. PID stays in AUTOMATIC —
    // recovery rebuilds from integrator=0 once fastOV clears.
    currentPID.ResetIntegratorTo(0.0);
    queueConsoleMessageF("FastOV hard #%lu: V=%.2fV target=%.2fV — inner PID integrator reset",
                         (unsigned long)g_fastOvHardCount, IBV, ChargingVoltageTarget);
  }
  g_fastOvHardActive_prev = g_fastOvHardActive;

  // ========== EMERGENCY LIMP HOME MODE (runs every loop) ==========
  // WARNING: BYPASSES ALL SAFETY SYSTEMS EXCEPT INA228 HARDCODED
  if (LimpHome == 1) {
    handleLimpHome(currentMillis, tick);
    return;
  }
  uint32_t aflM2 = micros();  // end of section 2: temp warning + fast-OV supervisor + limp gate

  // ========== GATE ON FRESH CH1 DATA ==========
  // PidSampleDivisor=1: PID runs every CH1 sample (~200Hz, ~5ms interval)
  // PidSampleDivisor=2: every other sample (~100Hz), etc.
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

  // ========== COMMISSIONING IDLE REST ==========
  // Handled here, before the AUTO/MANUAL/fault/stage machinery, so it neither runs a charging stage
  // nor logs mode transitions. sysMode is intentionally left as-is (last AUTO), so resuming a test or
  // ending the session slips back to NORMAL_AUTO with no SYS_MODE transition spam.
  if (mode == MODE_COMMISSION_IDLE) {
    runCommissionIdle(tick, reason, actualDtSec);
    prevMode = mode;
    return;
  }

  // ========== OVERRIDE MODE ENTRY/EXIT DETECTION ==========
  // Edge detection so one-shot actions fire exactly once on each transition.
  static bool lastMaintainMode = false;
  static bool lastTargetVoltageMode = false;
  bool enteringMaintainMode = (MaintainMode == 1) && !lastMaintainMode;
  bool exitingMaintainMode = (MaintainMode == 0) && lastMaintainMode;
  bool enteringTargetVoltageMode = (TargetVoltageMode == 1) && !lastTargetVoltageMode;
  bool exitingTargetVoltageMode = (TargetVoltageMode == 0) && lastTargetVoltageMode;
  lastMaintainMode = (MaintainMode == 1);
  lastTargetVoltageMode = (TargetVoltageMode == 1);

  // ========== CHARGING STAGE (bulk/absorption/float) ==========
  // Suppressed while any override is active — letting it run would fire
  // spurious re-bulk transitions and absorption timeouts.
  // Stage tracking only meaningful in AUTO. In MANUAL, voltageControlActive=false
  // and no CV loop runs, so stage state is meaningless.
  // CVTuningMode suppressed for same reason as TargetVoltageMode: the step test
  // temporarily raises ChargingVoltageTarget above the real stage target, which
  // can trigger bulk→absorption transitions and cause the absorption tail timer
  // to expire mid-test, killing voltageControlActive.
  if (MaintainMode != 1 && TargetVoltageMode != 1 && !CVTuningMode && !tick.manualMode) {
    updateChargingStage();
  }

  // On override exit: call enter_sys_auto() to reset stage to BULK rather than
  // resuming pre-override stage state that may be stale. updateChargingStage()
  // will fast-forward to the correct stage on the first resumed tick if the
  // battery is already charged.
  if ((exitingMaintainMode || exitingTargetVoltageMode) && sysMode == SYS_MODE_AUTO) {
    enter_sys_auto();
    if (exitingMaintainMode) queueConsoleMessage("MaintainMode exit: resuming charge from bulk");
    if (exitingTargetVoltageMode) queueConsoleMessage("TargetVoltageMode exit: resuming charge from bulk");
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

  // Major overvoltage: bypass slew for fast field collapse.
  // Triggers when battery is 0.5V above the hard-shutdown threshold — by this point the
  // fault path is already ramping; this just removes the slew limit so the ramp is instant.
  if (tick.currentBatteryVoltage > (tick.alternatorHardShutdownV + 0.5f)) {
    govMode = GOV_BYPASS_SLEW;
  }
  // Voltage sensor failure: bypass slew
  if (!tick.voltagePlausible || tick.voltageDisagreementCritical) {
    govMode = GOV_BYPASS_SLEW;
  }

  // CV overshoot: bypass duty slew so the output current PID can drop field current without the
  // governor rate limit holding it back. Triggered by fastOvClampActive, latched until battV is
  // back within 0.02 V of target (anti-chatter), CV stages only.
  // Rationale + "do not revert" history: CV_Loop_Dev_Summary.md (govBypass latch section).
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
        tuningScore = {};  // discard partial data — cannot score across a non-AUTO gap
        tuningParamChanged = false;
        pidInitialized = true;
        if (enteringNormal) {
          shutdownPhase = SHUTDOWN_PHASE_NONE;
          shutdownPhaseEntryMs = 0;
          shutdownPhase2EntryMs = 0;
          settledAtZeroDutyMs = 0;
          warmupCeiling = 0.0f;
          inStartupRamp = (StartupRiseRate > 0.0f);  // slow-ramp setpoint on field turn-on; cleared once setpointLimited catches command
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

  if (fieldCollapseTime > 0 && (tick.nowMs - fieldCollapseTime) >= activeCollapseDelay) {
    fieldCollapseTime = 0;
  }
  uint32_t aflM3 = micros();  // end of section 3: CH1 gate + stage/governor/mode transitions
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
  // Hold field off while Core 0 internet operation is in progress OR while
  // there are still pending HTTPS uploads in the queue (so the field can't
  // "blip" through between consecutive queued uploads). Sets fieldActiveStatus
  // to 4 (WAITING_CLOUD) so the header banner shows why the field isn't
  // engaging despite charging being enabled.
  //
  // 10-second safety timeout: if WiFi is hosed or an upload hangs, flush the
  // queue and proceed with field-on so charging isn't blocked indefinitely.
  // The in-flight Core 0 upload finishes naturally; sensorRingInFlightIndex=-1
  // means its success won't pop the ring slot (record stays for retry).
  {
    static unsigned long fieldHoldStartMs = 0;
    bool cloudBusy = (core0Busy || uxQueueMessagesWaiting(httpsQueue) > 0);
    if (cloudBusy) {
      if (fieldHoldStartMs == 0) fieldHoldStartMs = millis();
      if (millis() - fieldHoldStartMs < 10000UL) {
        fieldActiveStatus = 4;  // WAITING_CLOUD
        return;
      }
      // Timeout: drop queued requests and let charging proceed.
      // Drain item-by-item, NOT xQueueReset — each queued request OWNS a
      // ps_malloc'd payload that only the receiver frees; a raw reset
      // leaks 4-128KB of PSRAM per flushed request.
      HttpsRequest droppedReq;
      while (xQueueReceive(httpsQueue, &droppedReq, 0) == pdTRUE) {
        if (droppedReq.payload) free(droppedReq.payload);
      }
      sensorRingInFlightIndex = -1;
      queueConsoleMessage("Field-on wait timeout (10s) — flushed cloud queue, proceeding");
      fieldHoldStartMs = 0;
    } else {
      fieldHoldStartMs = 0;
    }
  }
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
  // Commissioning field-% ramp shares the identical duty-override + bumpless-resume path
  // (mutually exclusive with SystemID via the start-handler mutex). OR it in so the snapshot,
  // override, and restore logic below cover it too.
  sysIDRunning = fieldCurve_tick(sysIDDutyOut, MeasuredAmps, tick.nowMs) || sysIDRunning;

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
    vlHasPrev = false;  // CV voltage loop is bypassed during the sweep (whole control path is under
                        // !sysIDRunning), so voltLoop_record never fires. Re-baseline the CV-interval
                        // tracker so the dead time across the test isn't logged as one giant gap (was
                        // saturating vl_worst_at to 65535). Mirrors the CV-inactive re-baseline
                        // (vlHasPrev = false) in the voltageControlActive else-branch further below.
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
    setpointCommand = 0.0f;   // global (declared in Xregulator.ino) — read by the Control Accuracy score gate

    // Tracks voltageControlActive across both AUTO and MANUAL branches so AUTO
    // re-entry from MANUAL correctly fires the bumpless CV seed. Declared here
    // (outside the AUTO branch) so the MANUAL branch can also update it.
    static bool lastVoltageControlActive = false;

    if (sysMode == SYS_MODE_AUTO) {

      static bool lastTuningMode = false;

      // tempFilterUpdate() now runs unconditionally at the TOP of this function (hoisted
      // 2026-06-26) so the filtered temp / slope / projection stay live in FAULT/MANUAL/OFF
      // cooldown too, not just AUTO. It used to be called here, gated by SYS_MODE_AUTO, which
      // is why the plot flat-lined the moment an over-temp event dropped the system to FAULT.
      // Kept earlier-than-tempPID_tick ordering, so the PID input is unchanged. (was: 2026-06-23)

      if (TuningMode) {
        // ===== TUNING MODE (square-wave setpoint generator) =====
        static bool tuningWaveHigh = false;
        static uint32_t lastTuningWaveToggle = 0;

        // Parameter changed — discard accumulator and re-ring-in under new params
        if (tuningParamChanged) {
          tuningScore = {};
          tuningParamChanged = false;
        }

        // Manual commit requested from UI
        if (manualCommitTuningRequested) {
          manualCommitTuningRequested = false;
          if (tuningScore.scoredToggleCount >= 4 && tuningScore.activeTimeSec > 0.5f) {
            commitTuningRecord();
          } else {
            queueConsoleMessageF("TuningScore: commit rejected — only %d/4 scored half-cycles accumulated",
                                 (int)tuningScore.scoredToggleCount);
          }
        }

        // ── Waveform: square (0) | sine manual (1) | sine auto-sweep (2) ──
        static float tuningSinePhase = 0.0f;
        if (tuningWaveform != 0) {
          // SINE (closed-loop reference). Bypass slew so the PID sees the true sine; no ISE scoring.
          tuningScore.inScoringWindow = false;
          float baseA = (float)tuningWaveFloor + waveAmplitude * 0.5f;   // midpoint; swings tuningWaveFloor .. tuningWaveFloor+waveAmplitude
          float ampA  = waveAmplitude * 0.5f;
          tuningSineStep(tick.nowMs, actualDtSec, tuningSinePhase, baseA, ampA, MeasuredAmps, setpointCommand);
          setpointLimited = setpointCommand;           // no slew limiting → clean sine reference
        } else {
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
              tuningScore.inScoringWindow = false;
              tuningScore.scoredToggleCount++;
            }
          }

          // Close scoring window 5s after it opened (lastToggleMs is set when window opens, not at toggle)
          if (tuningScore.inScoringWindow && (tick.nowMs - tuningScore.lastToggleMs > 5000)) {
            tuningScore.inScoringWindow = false;
          }

          uTargetAmps = tuningWaveHigh ? (tuningWaveFloor + waveAmplitude) : tuningWaveFloor;
          setpointCommand = (float)uTargetAmps;

          // CV plant fit sets tuningSquareAbrupt so the edge is a true step (slew smears the step instant
          // the edge-gain fit keys off, and was only kept for the now-retired dead-time read). Otherwise
          // the manual current-tuning square test keeps its slew + scoring window.
          if (tuningSquareAbrupt)
            setpointLimited = setpointCommand;
          else
            setpointLimited = slew_limit_f(setpointLimited, setpointCommand,
                                           SetpointRiseRate, SetpointFallRate, actualDtSec);

          // Open scoring window once slew has settled (rate < 1 A/s) — fair regardless of SetpointRiseRate.
          // lastToggleMs is set here so the 5s timeout starts from when scoring actually begins.
          {
            static float tuning_prevSlewed = 0.0f;
            static bool tuning_slewInit = false;
            float tuning_slewRate = 0.0f;
            if (tuning_slewInit) {
              tuning_slewRate = fabsf(setpointLimited - tuning_prevSlewed) / actualDtSec;
            }
            tuning_prevSlewed = setpointLimited;
            tuning_slewInit = true;
            if (tuningScore.pendingWindowOpen && tuning_slewRate < 1.0f) {
              tuningScore.inScoringWindow = true;
              tuningScore.pendingWindowOpen = false;
              tuningScore.lastToggleMs = tick.nowMs;
            }
          }
        }
        voltageControlActive = false;

        targetCurrent = (OutputPIDSigSrc == 2) ? MeasuredAmps : (OutputPIDSigSrc == 1) ? g_pidMA_N
                                                                                       : g_pidI_filtered;
        pidInput = (double)targetCurrent;
        pidSetpoint = (double)setpointLimited;
        pidError = setpointLimited - targetCurrent;
        currentPID.Compute();

        // Accumulate test score while inside a scoring window
        if (tuningScore.inScoringWindow) {
          float e = pidError;
          tuningScore.errorAccum += e * e * actualDtSec;
          tuningScore.activeTimeSec += actualDtSec;
          if (fabsf(e) > tuningScore.worstErrorA) tuningScore.worstErrorA = fabsf(e);
          tuningScore.rpmSum += RPM;
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
          tempPIDActive = false;  // PID was dormant during tuning — force the bumpless re-seed
          // tempFilterUpdate() kept BOTH the IIR filter and the slope buffer live throughout
          // tuning (no tempFilterNeedsReseed → IIR preserved). Set thermalPreserveSlopeOnResume
          // so the re-enable path also KEEPS the slope buffer instead of wiping it. Without this,
          // the re-enable wipe sets thermalSlopeBufFull=false → the setpoint drops to limit-20 for
          // 60s the instant tuning exits (TuningMode→0 turns off suppressWarmupMargin), derating
          // the field right after a test. We have a live trend, so the loop is NOT blind — keep
          // the projection and stay at limit-5. (2026-06-23, slope-preserve fix added later.)
          thermalPreserveSlopeOnResume = true;
          tuningScore = {};  // discard accumulator — commit is always manual
          tuningSquareAbrupt = false;  // never let the abrupt-step bypass leak past a tuning session
        }
        lastTuningMode = false;

        // Temperature loop PID. Library timer governs Compute() cadence.
        tempPID_tick(currentMillis, actualDtSec);

        // --- Command architecture ---
        //
        //   I_cap        RPM-dependent mechanical/electrical ceiling (table lookup).
        //   thermalPenalty  Temperature loop PID output. Derates I_cap when hot;
        //                   zero-floored in CV stages (enforced in tempPID_tick).
        //   uTargetAmps  I_cap minus thermal penalty, clamped to [0, MaxTableValue],
        //                with user overrides applied. This is the table+thermal limit
        //                and the upper bound passed to the CV controller.
        //   Icv          CV position-form PID output — the direct current setpoint in
        //                absorption, float, and TargetVoltageMode. Clamped to
        //                [0, uTargetAmps]. Never written back to thermalPenaltyAmps
        //                or the thermal integrator.
        //
        // Execution order:
        //   1. Subtract thermal penalty from I_cap; clamp to [0, MaxTableValue].
        //   2. Apply user overrides (MaintainMode). HiLow mode is handled at
        //      table-load time via loadCapTablesForMode() — no runtime halving.
        //   In CV modes: position-form PI (P+I) produces Icv; setpointCommand = Icv.
        //      Integrator anti-windup: upward integration frozen when P+I saturates at uTargetAmps ceiling;
        //      slope-aware bleed (SlopeBleedK) also drains cv_I when voltage rises fast, scaled by
        //      proximity to setpoint (SlopeBleedProxV) so it is inactive far below target.
        //   In idle: setpointCommand = uTargetAmps directly. (MaintainMode runs the CV path
        //      instead: uTargetAmps=0 caps Icv to 0 — see the MaintainMode branch below.)

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

        // Charge-rate Hi->Lo ceiling glide. Armed by the HiLow handler when the user picks Low: the
        // table has already dropped to the Low cap, but the alternator is still at the old output. Hold
        // the ceiling up and ramp it down to the new (lower) uTargetAmps at MaxTableValue A/s (~1s full
        // scale) so the field tracks it instead of stepping. The ramp shrinks the drop but cannot by
        // itself stop an iExcess trip: both iExcess detectors compare actual current against a reference
        // that descends during the glide (CV: setpointLimited; bulk: i_ceiling_pre_ov), and the field
        // current lags ABOVE any descending reference — so the glide ALSO drives modeCapGlideSuppress
        // (glide window + a settling-tail grace), which both iExcess detectors read to hold their EMA
        // at 0 for the duration (see those blocks below).
        // The HARD protections stay live throughout — hardware OV, fast OV, and the HardOCTripAmps trip.
        // The glide self-clears once the held ceiling reaches the new cap (or an up-switch raises it past).
        if (modeCapSlewActive) {
          if (modeCapSlew > (float)uTargetAmps) {
            // Ramp at MaxTableValue / MODE_CAP_GLIDE_SEC A/s (~2.5s full-scale) — gentle enough that the
            // field stays close to the descending ceiling, so the post-glide settling tail stays tiny.
            modeCapSlew = fmaxf((float)uTargetAmps, modeCapSlew - (MaxTableValue / MODE_CAP_GLIDE_SEC) * actualDtSec);
            uTargetAmps = modeCapSlew;
          } else {
            modeCapSlewActive = false;
            modeCapSlewEndMs = currentMillis;   // start the post-glide iExcess grace window
          }
        }
        // True while the glide runs AND for MODE_CAP_GLIDE_GRACE_MS after it self-clears, so iExcess
        // ignores both the deliberate command descent and the field-current settling tail that follows.
        // Both iExcess detectors below read this. (uint32_t subtraction wraps cleanly; once past the
        // grace window the difference is large and the term is false.)
        bool modeCapGlideSuppress = modeCapSlewActive
                                    || (modeCapSlewEndMs != 0 && (uint32_t)(currentMillis - modeCapSlewEndMs) < MODE_CAP_GLIDE_GRACE_MS);

        // User overrides
        if (MaintainMode == 1) uTargetAmps = 0;

        // Actual RPM/thermal/override ceiling — the true pre-OV current limit for logging.
        // uTargetRaw remains MaxTableValue and is not used for telemetry.
        float i_ceiling_pre_ov = (float)uTargetAmps;

        // Hoisted here so iExcess block can reset it on event onset.
        static float cv_I_aw_cap = 100.0f;

        // ── iExcess supervisor (EMA / leaky-integral detector) ──────────────
        // Fires on a SUSTAINED current excess over the CV command: an EMA of
        // (MeasuredAmps − setpointLimited) crossing E = clamp(IExcessFrac × setpointLimited,
        // floor, ceil). Voltage-gated to near target (IBV > target − IExcessArmMarginV) so it
        // can't fire during ramp-up; testProtectionsEnabled=false or TuningMode=1 inhibit it
        // (else branch releases the latch and reseeds the EMA).
        // Full math + rationale: Working Markdown Docs/iExcess_Redesign_Spec.md.
        {
          const float K_IE = 1.0f;
          static bool iExcessActive = false;   // latched fire state (held until the average clears)
          static float mExcessEma = 0.0f;      // EMA of signed deviation iActual − setpointLimited (A)
          // (pre-event cv_I capture and reseed are centralised — see preEventCvI above
          //  and the unified falling-edge reseed in the bumpless tracker block.)

          if (testProtectionsEnabled && !TuningMode && voltageControlActive && (IBV > ChargingVoltageTarget - IExcessArmMarginV)) {
            // Threshold: fraction of command, floor/ceiling guarded.
            float E = fmaxf(IExcessFloorA, fminf(IExcessFrac * setpointLimited, IExcessCeilA));

            if (!iExcessActive && (fastOvClampActive || modeCapGlideSuppress)) {
              // Another protection (fastOV/hardOV) already owns the clamp and has collapsed
              // setpointLimited. The resulting actual-vs-command mismatch is THAT protection's
              // own doing, not a real over-current — and dev would jump to ~full current,
              // crossing E in a few ms. Hold the EMA at 0 so we don't fire a redundant iExcess
              // during the field-TC wind-down. This subsumes the old postFastOvMismatch gate:
              // once the other protection releases, the EMA restarts from 0 and only a genuinely
              // sustained post-release excess can fire. (Deliberate keep vs spec §7 — verified
              // necessary: deleting it outright re-fires ~8 ms after every fastOV event.)
              // modeCapGlideSuppress: the Hi->Lo ceiling glide is deliberately ramping setpointLimited
              // down — the field current lags ABOVE the descending command (plant can't fall as fast
              // as the ramp), and that lag is the glide's own doing, not a fault. Hold the EMA at 0
              // for the glide AND a grace tail (settling), then it restarts from 0.
              mExcessEma = 0.0f;
            } else {
              // dt-aware EMA of the raw-current deviation (raw MeasuredAmps; the EMA does all
              // the filtering). dt = real elapsed control-tick seconds, so I²C jitter stretching
              // a tick can't corrupt the time constant — same pattern as g_fastOvDvdt.
              float tauSec = IExcessTau * 0.001f;
              float alpha  = actualDtSec / (tauSec + actualDtSec);
              mExcessEma  += alpha * ((MeasuredAmps - setpointLimited) - mExcessEma);  // setpointLimited = previous tick — acceptable

              // Rising edge — fire once.
              if (!iExcessActive && mExcessEma > E) {
                cv_I_aw_cap = cv_I;         // cap the bumpless tracker ceiling to pre-event level — prevents current-limited rewind
                g_iExcessCount++;
                // Collapse inner PID integrator (same as hard-OV reset) — without it duty authority
                // is only innerKp × measured current, weak from low setpoints. See CV_Loop_Dev_Summary.md.
                currentPID.ResetIntegratorTo(0.0);
                queueConsoleMessageF("iExcess #%lu: excess=%.1fA over %.1fA cmd — inner PID integrator reset",
                                     (unsigned long)g_iExcessCount, mExcessEma, setpointLimited);
                // One-shot cv_I drain on the rising edge. IExcessKBleed = 0: snap cv_I to zero
                // (deepest starting point for recovery). > 0: subtract K_bleed × averaged-excess ×
                // dtSec from cv_I once. Both zero the current COMMAND in one tick (1e9 fall-rate
                // override on setpointLimited); the duty collapses via the inner-PID reset above.
                // Sustained per-tick drain for the rest of the event comes from awBleedAmpS in the
                // bumpless tracker block. Final reseed is the unified falling-edge reseed when ALL
                // protections clear. The IExcessKBleed knob only sets post-event recovery depth.
                if (IExcessKBleed <= 0.0f) {
                  cv_I = 0.0f;
                } else {
                  cv_I = fmaxf(0.0f, cv_I - IExcessKBleed * mExcessEma * actualDtSec);
                }
                iExcessActive = true;
              }

              // While latched: re-apply the proportional cap and hold govBypass each tick;
              // release on hysteresis. Cap input is mExcessEma (the averaged signal) so capReason
              // is honest without chattering on ripple. Deliberately not a hard 0-cap — that
              // would deepen recovery undershoot. Release when the average falls below
              // E × IExcessRelFrac (the scale-aware replacement for the old fixed 2 A IEXCESS_HYST).
              if (iExcessActive) {
                float ieCap = fmaxf(0.0f, fastOvBaseCap - K_IE * mExcessEma);
                if (ieCap < fastOvCurrentCap) { fastOvCurrentCap = ieCap; capReasonTick = CAP_REASON_IEXCESS; }
                fastOvClampActive = true;
                if (mExcessEma < E * IExcessRelFrac) {
                  iExcessActive = false;   // release; unified reseed handles cv_I
                }
              }
            }
            g_mExcessEma = mExcessEma;       // tuning traces (CV detector owns the export when its gate is open)
            g_iExcessThreshold = E;
          } else {
            // Gate closed (battV below target − ArmMargin, or protections off / tuning) — release
            // and reseed the EMA so a later CV entry or setpoint step doesn't carry stale charge
            // into a startup fire. Unified reseed fires on the falling edge of fastOvClampActive.
            iExcessActive = false;
            mExcessEma = 0.0f;
            g_mExcessEma = 0.0f;
            g_iExcessThreshold = 0.0f;
          }
          g_iExcessActive = iExcessActive;
          g_iExcessDutyCap = 100.0f;  // retired
        }

        // ── iExcess supervisor — BULK sub-mode (Group 3, current-control phase) ──────
        // Sibling of the CV iExcess above, for battV FAR below target (bulk). Gate is the
        // strict complement (IBV ≤ target − IExcessArmMarginV), so the two hand off at that
        // line with no gap/overlap. Compares actual against i_ceiling_pre_ov (the commanded
        // ceiling) not setpointLimited, with a looser IExcessFracBulk, so a normal RPM ramp
        // produces no excess — only a true overshoot above the mechanical ceiling fires it.
        // Response binds via fastOvCurrentCap RELATIVE TO THE CEILING (zeroing cv_I can't
        // collapse a saturated bulk setpoint); recovery shares the unified fastOvClampActive
        // path. Full rationale: Working Markdown Docs/iExcess_Redesign_Spec.md.
        {
          const float K_IE = 1.0f;
          static bool iExBulkActive = false;     // latched fire state
          static float mExcessEmaBulk = 0.0f;    // EMA of signed deviation iActual − i_ceiling_pre_ov (A)

          if (testProtectionsEnabled && !TuningMode && voltageControlActive
              && (IBV <= ChargingVoltageTarget - IExcessArmMarginV)) {
            // Threshold: fraction of the commanded ceiling, floor/ceiling guarded. Looser than the
            // CV fraction (IExcessFracBulk > IExcessFrac) — tolerate more command-vs-actual error
            // far from the voltage limit, catching only absurd RPM-blip overshoots above ceiling.
            float E = fmaxf(IExcessFloorA, fminf(IExcessFracBulk * i_ceiling_pre_ov, IExcessCeilA));

            if (!iExBulkActive && (fastOvClampActive || modeCapGlideSuppress)) {
              // Another protection (a load dump, the only other fast supervisor active in bulk)
              // owns the clamp and has collapsed the command. Hold the EMA at 0 so we don't fire
              // a redundant bulk iExcess during the field-TC wind-down — subsumes the old
              // postBulkMismatch self-gate.
              // modeCapGlideSuppress: the Hi->Lo ceiling glide ramps i_ceiling_pre_ov DOWN, but the
              // field current lags above the descending ceiling (it can't fall as fast as the
              // ramp) — that lag reads as over-current and was the bulk false-trip on a mode switch.
              // The glide owns this drop, so hold the EMA at 0 for the glide plus a grace tail.
              mExcessEmaBulk = 0.0f;
            } else {
              float tauSec = IExcessTau * 0.001f;
              float alpha  = actualDtSec / (tauSec + actualDtSec);
              mExcessEmaBulk += alpha * ((MeasuredAmps - i_ceiling_pre_ov) - mExcessEmaBulk);  // vs commanded ceiling, not slewed setpoint

              if (!iExBulkActive && mExcessEmaBulk > E) {
                cv_I_aw_cap = cv_I;         // cap bumpless tracker ceiling to pre-event level
                g_iExcessCount++;           // shared Group 3 trip counter
                currentPID.ResetIntegratorTo(0.0);
                queueConsoleMessageF("iExcess (bulk) #%lu: excess=%.1fA over %.1fA ceiling — inner PID integrator reset",
                                     (unsigned long)g_iExcessCount, mExcessEmaBulk, i_ceiling_pre_ov);
                if (IExcessKBleed <= 0.0f) {
                  cv_I = 0.0f;
                } else {
                  cv_I = fmaxf(0.0f, cv_I - IExcessKBleed * mExcessEmaBulk * actualDtSec);
                }
                iExBulkActive = true;
              }

              if (iExBulkActive) {
                float ieCap = fmaxf(0.0f, i_ceiling_pre_ov - K_IE * mExcessEmaBulk);  // ceiling-relative — must bite below the bulk ceiling
                if (ieCap < fastOvCurrentCap) { fastOvCurrentCap = ieCap; capReasonTick = CAP_REASON_IEXCESS_BULK; }
                fastOvClampActive = true;
                if (mExcessEmaBulk < E * IExcessRelFrac) {
                  iExBulkActive = false;    // release; unified reseed handles cv_I
                }
              }
            }
            g_mExcessEma = mExcessEmaBulk;   // bulk owns the tuning export while its gate is open
            g_iExcessThreshold = E;
          } else {
            // Gate closed (near/above target — CV iExcess owns this regime, or protections off).
            // Leave g_mExcessEma / g_iExcessThreshold untouched so the CV detector's export stands.
            iExBulkActive = false;
            mExcessEmaBulk = 0.0f;
          }
          g_iExcessBulkActive = iExBulkActive;  // export for PID/CV log flag + dashboard
        }

        // ── iExcess live-sparkline aggregation (every control tick) ──────────────────
        // Accumulate the per-CSV1-frame worst case: peak averaged excess and min threshold E
        // over the ticks where a detector gate is open (g_iExcessThreshold > 0 = armed). The
        // CSV1 sender ships and resets these. The "fired" colour comes from protEventMask
        // (bit PROT_EVT_IX, latched above on a real cap), not from these aggregates.
        if (g_iExcessThreshold > 0.0f) {
          if (!g_iExcessArmedWin) {
            g_mExcessEmaPeak = g_mExcessEma;
            g_iExcessThreshWinMin = g_iExcessThreshold;
            g_iExcessArmedWin = true;
          } else {
            if (g_mExcessEma > g_mExcessEmaPeak) g_mExcessEmaPeak = g_mExcessEma;
            if (g_iExcessThreshold < g_iExcessThreshWinMin) g_iExcessThreshWinMin = g_iExcessThreshold;
          }
        }

        // ── Load dump detection — dBcur/dt positive spike in CV mode ─────────────────
        // Three-tier cascade: N=1 above LoadDumpDtThresh1, N=2 above LoadDumpDtThresh, N=3 above LoadDumpDtThresh3.
        // INA228 noise is alternating-sign (high/low/high/low) — two consecutive same-direction crossings
        // cannot be measurement noise. Tier 1 catches hard-switched FET disconnects; tiers 2/3 catch
        // relay-contact events spread over multiple samples.
        // Gate: voltageControlActive (= !inIdleStage; bulk/absorption/float/TVMode/MaintainMode) and fast INA228 reads active (5ms cadence).
        // On detection: snaps cv_I = 0 on rising edge and collapses setpointLimited + fastOvCurrentCap to 0.
        // Recovery: AW bleed drives recovery naturally — same path as fastOV.
        {
          // Statics moved one scope up so the gate-closed else can clear them.
          // Without this, a load dump active at the moment CV exits leaves
          // ldWasActive=true; on CV re-entry the rising-edge cv_I=0 snap and
          // g_loadDumpCount increment are skipped for the next event.
          static bool ldWasActive = false;
          static int ldCount1 = 0;  // consecutive samples above LoadDumpDtThresh1
          static int ldCount2 = 0;  // consecutive samples above LoadDumpDtThresh
          static int ldCount3 = 0;  // consecutive samples above LoadDumpDtThresh3
          // (pre-event cv_I capture and reseed now centralised — see preEventCvI
          //  above and the unified falling-edge reseed in the bumpless tracker block.)
          if (voltageControlActive && inaFastModeActive) {
            if (g_dBcur_dt > LoadDumpDtThresh1) {
              ldCount1++;
            } else {
              ldCount1 = 0;
            }
            if (g_dBcur_dt > LoadDumpDtThresh) {
              ldCount2++;
            } else {
              ldCount2 = 0;
            }
            if (g_dBcur_dt > LoadDumpDtThresh3) {
              ldCount3++;
            } else {
              ldCount3 = 0;
            }
            bool ldNow = (ldCount1 >= 1) || (ldCount2 >= 2) || (ldCount3 >= 3);
            if (ldNow) {
              fastOvCurrentCap = 0.0f;
              capReasonTick = CAP_REASON_LOADDUMP;  // hard cutoff — always the binding cap
              setpointLimited = 0.0f;
              fastOvClampActive = true;
              if (!ldWasActive) {
                cv_I = 0.0f;            // snap voltage-loop integrator on rising edge
                // Collapse inner PID integrator too (same as iExcess / hard-OV): without
                // it a wound-up integrator resists the setpoint collapse and grinds duty
                // down at ~40%/s instead of reaching MinDuty in 1-2 cycles. Load dump is
                // the most catastrophic over-current, so it gets the fastest field collapse.
                currentPID.ResetIntegratorTo(0.0);
                g_loadDumpCount++;
                queueConsoleMessageF("Load dump #%lu detected — output cut, inner PID integrator reset (dBcur/dt=%.0f A/s)",
                                     (unsigned long)g_loadDumpCount, g_dBcur_dt);
              }
            }
            // Falling-edge cv_I reseed handled by the unified reseed in the bumpless
            // tracker block — fires once all protection paths have cleared.
            g_loadDumpActive = ldNow;
            ldWasActive = ldNow;
          } else {
            // Gate closed — clean state so next gate-open starts fresh.
            ldCount1 = 0;
            ldCount2 = 0;
            ldCount3 = 0;
            ldWasActive = false;
            g_loadDumpActive = false;
          }
        }

        // ── Re-evaluate govMode now that iExcess + load-dump have had a chance to
        // set fastOvClampActive. The initial govMode decision (above) only saw the
        // fastOV supervisor's contribution; without this, a pure iExcess or load-dump
        // event collapses setpointLimited instantly (via the fall-rate override at
        // effectiveFallRate=1e9) but the governor still slews duty at DutyRampRate
        // (80%/s) instead of snapping to the rpmMinDuty/MinDuty floor in one tick.
        // BYPASS still respects rpmMinDuty and MinDuty via governor_apply's pre-clamp,
        // so the tach floor is preserved — only the descent speed changes.
        if (fastOvClampActive && voltageControlActive) {
          govMode = GOV_BYPASS_SLEW;
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
          ChargingVoltageTargetReq = TargetVoltageSetpoint;   // slewed into ChargingVoltageTarget below
          if (enteringTargetVoltageMode) {
            pidLog_enteringTargetVoltageMode = 1;
            queueConsoleMessageF("TargetVoltageMode: active, target=%.2fV", TargetVoltageSetpoint);
          }
        }

        // voltageControlActive: true in bulk, absorption, float, TargetVoltageMode (= !inIdleStage),
        // and MaintainMode (forced true below — ceiling-enforcer pattern, Groups 1/2 stay armed).
        // False only in idle (UseFloat=0 post-absorption).
        voltageControlActive = !inIdleStage;
        if (TargetVoltageMode == 1) voltageControlActive = true;  // force CV active even from idle
        if (MaintainMode == 1) {
          // Ceiling enforcer: PI runs at BulkVoltage but uTargetAmps=0 caps Icv→0, so setpoint stays 0; Groups 1/2 arm.
          voltageControlActive = true;
          ChargingVoltageTargetReq = BulkVoltage;
          if (enteringMaintainMode) {
            queueConsoleMessage("MaintainMode: active, targeting 0 net battery amps");
          }
        }
        // Detect CV entry so the voltage loop fires immediately on the first CV tick.
        // (lastVoltageControlActive is declared above the AUTO/MANUAL branches so the
        // MANUAL branch also resets it — see voltageControlActive=false in MANUAL below.)
        bool enteringCV = (!lastVoltageControlActive && voltageControlActive);
        lastVoltageControlActive = voltageControlActive;

        // ===== WAVEFORM GENERATOR: voltage square-wave generator (CVTuningMode) =====
        // Dithers ChargingVoltageTarget between base (HIGH) and base−amp (LOW) so the
        // CV loop step response (settling time, overshoot) can be measured and scored.
        // Only runs in the NORMAL AUTO path — incompatible with inner-loop TuningMode.
        {
          static bool lastCVTuningMode = false;

          // Discard accumulator on CVTuningMode turn-off — commit is always manual
          if (lastCVTuningMode && !CVTuningMode) {
            cvTuningScore = {};
          }
          lastCVTuningMode = (CVTuningMode != 0);

          // Manual commit requested from UI
          if (manualCommitCVTuningRequested) {
            manualCommitCVTuningRequested = false;
            if (cvTuningScore.scoredHighCount >= 1) {
              commitCVTuningRecord();
            } else {
              queueConsoleMessage("CVTuningScore: commit rejected — no scored HIGH phases yet");
            }
          }

          if (CVTuningMode && voltageControlActive) {
            // Capture base target and initial conditions once per test
            if (!cvTuningScore.testStarted) {
              cvBaseTarget = ChargingVoltageTarget;
              cvTuningScore.battVAtStart = IBV;
              cvTuningScore.socAtStart = (float)SOC_percent / 100.0f;
              cvTuningScore.lastToggleMs = currentMillis;
              cvTuningScore.waveHigh = false;  // start in LOW phase (at normal setpoint); test steps UP
              cvTuningScore.phaseStartMs = currentMillis;
              cvTuningScore.testStarted = true;
            }

            // Commit on parameter change if enough cycles have scored.
            // Preserve current phase and restart the half-period timer from now so the
            // next toggle is a full symmetric half-period away regardless of when the
            // param change arrived.
            if (cvTuningParamChanged) {
              bool wasHigh = cvTuningScore.waveHigh;
              if (cvTuningScore.scoredHighCount >= 1) commitCVTuningRecord();
              else cvTuningScore = {};
              cvTuningScore.waveHigh = wasHigh;
              cvTuningScore.lastToggleMs = currentMillis;
              cvTuningScore.testStarted = true;
              cvTuningParamChanged = false;
            }

            // Half-period toggle — cvWavePeriodSec is the full period, so each half is / 2
            uint32_t halfPeriodMs = (uint32_t)cvWavePeriodSec * 500UL;
            if (currentMillis - cvTuningScore.lastToggleMs >= halfPeriodMs) {
              bool goingHigh = !cvTuningScore.waveHigh;
              cvTuningScore.waveHigh = goingHigh;
              cvTuningScore.lastToggleMs = currentMillis;
              cvTuningScore.halfPeriodCount++;
              // 1 half-period ring-in: initial LOW (resting at normal setpoint) is unscored.
              // ringInDone becomes true on the first LOW→HIGH toggle.
              // Guard on phaseStartMs > 0 prevents the initial HIGH→LOW from finalizing
              // a never-started scored HIGH phase.
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
                cvBaseTarget = ChargingVoltageTarget;  // refresh real target
                cvTuningScore.phaseStartMs = currentMillis;
                cvTuningScore.phaseSettled = false;
                cvTuningScore.consecutiveInBand = 0;
                cvTuningScore.fastOvSnap = g_fastOvClampCount;
                cvTuningScore.iExcessSnap = g_iExcessCount;
                cvTuningScore.loadDumpSnap = g_loadDumpCount;
                cvTuningScore.hardOcSnap = g_hardOCCount;
              }
              if (!goingHigh && cvTuningScore.ringInDone && cvTuningScore.phaseStartMs > 0) {
                // End of a scored HIGH phase — finalize settling time
                if (!cvTuningScore.phaseSettled) {
                  cvTuningScore.totalSettlingTimeSec += (float)cvWavePeriodSec / 2.0f;  // half-period penalty
                }
                cvTuningScore.scoredHighCount++;
                cvTuningScore.fastOvFires += (uint16_t)(g_fastOvClampCount - cvTuningScore.fastOvSnap);
                cvTuningScore.iExcessFires += (uint16_t)(g_iExcessCount - cvTuningScore.iExcessSnap);
                cvTuningScore.loadDumpFires += (uint16_t)(g_loadDumpCount - cvTuningScore.loadDumpSnap);
                cvTuningScore.hardOcFires += (uint16_t)(g_hardOCCount - cvTuningScore.hardOcSnap);
                // Start of scored LOW phase (step-down response)
                cvTuningScore.lowPhaseStartMs = currentMillis;
                cvTuningScore.lowPhaseSettled = false;
                cvTuningScore.lowConsecInBand = 0;
                cvTuningScore.lowCrossedBelow = false;
                cvTuningScore.lowFastOvSnap = g_fastOvClampCount;
                cvTuningScore.lowIExSnap = g_iExcessCount;
                cvTuningScore.lowLdSnap = g_loadDumpCount;
                cvTuningScore.lowHocSnap = g_hardOCCount;
              }
            }

            // Override ChargingVoltageTarget for this tick.
            // LOW phase = normal setpoint (battery rests here naturally).
            // HIGH phase = normal setpoint + amplitude (the step-up response being scored).
            ChargingVoltageTargetReq = cvTuningScore.waveHigh ? (cvBaseTarget + cvWaveAmplitudeV)
                                                              : cvBaseTarget;   // slew below applies if vTgtRampEnable (study on/off)
          }
        }

        // ── Voltage-target slew (bidirectional) — NEW outer layer; master switch vTgtRampEnable ──
        // The commanded target lives in ChargingVoltageTargetReq (set THIS tick by stage logic /
        // TargetVoltageMode / MaintainMode above, and by updateChargingStage()). We rate-limit the REAL
        // ChargingVoltageTarget toward it at vTgtRampUp / vTgtRampDn (V/s). ChargingVoltageTarget is what
        // BOTH the CV PI error AND the *relative* over-voltage protections read, so a commanded DROP
        // (absorption→float, manual lower) now glides down instead of stepping — fast-OV / iExcess no
        // longer trip on a deliberate target change.
        //   PROTECTIONS ARE UNCHANGED: no protection code, margin, or timing was touched. They still fire
        //   at the same speed relative to whatever the target currently is — we only limit how fast the
        //   target itself may move. The ABSOLUTE backstops are fully independent of the target and
        //   untouched: AlternatorHardShutdownV (software hard-cut) and the INA228 hardware comparator at
        //   VoltageHardwareLimit (cuts before any software runs) both fire at fixed voltages.
        //   vTgtRampEnable=0 → instant target (byte-identical to pre-ramp behaviour). The ramp DOES apply
        //   in CVTuningMode when enabled, so the square-wave test can be studied with the limiter on/off.
        //   Always snap (no ramp) when not doing voltage control, or on the first CV tick (bumpless seed —
        //   start the target at the right value rather than ramping it up from a stale one). 0 = instant.
        if (vTgtRampEnable == 0 || !voltageControlActive || enteringCV) {
          ChargingVoltageTarget = ChargingVoltageTargetReq;            // snap (disabled / idle / CV entry)
        } else if (ChargingVoltageTargetReq > ChargingVoltageTarget) {
          float step = (vTgtRampUp > 0.0f) ? (vTgtRampUp * actualDtSec) : 1.0e9f;
          ChargingVoltageTarget = fminf(ChargingVoltageTargetReq, ChargingVoltageTarget + step);
        } else if (ChargingVoltageTargetReq < ChargingVoltageTarget) {
          float step = (vTgtRampDn > 0.0f) ? (vTgtRampDn * actualDtSec) : 1.0e9f;
          ChargingVoltageTarget = fmaxf(ChargingVoltageTargetReq, ChargingVoltageTarget - step);
        }

        // ── Voltage target rise governor (RESTORED — inner windup guard, unchanged from original) ──
        // Clamps voltageTargetSlewed to IBV + e_needed, where e_needed is the voltage error the current
        // cv_I can support at the present current cap. Prevents the integrator from seeing a large up-step
        // when the target jumps. Falls are instantaneous. It now operates on the (slew-limited)
        // ChargingVoltageTarget and feeds the CV PI error below — kept so the windup protection that was
        // here before the target ramp is not lost.
        static float voltageTargetSlewed = 0.0f;
        if (enteringCV) {
          voltageTargetSlewed = ChargingVoltageTarget;
        }
        if (voltageControlActive) {
          if (ChargingVoltageTarget > voltageTargetSlewed + 0.01f) {
            float icvHi_gov = clamp_f((float)uTargetAmps, 0.0f, (float)MaxTableValue);
            float e_needed = (icvHi_gov - cv_I) / VoltageKp_active;
            e_needed = fmaxf(e_needed, 0.02f);
            voltageTargetSlewed = fminf(ChargingVoltageTarget,
                                        IBV + e_needed);  // raw INA228 — no filter lag on governor
          } else {
            voltageTargetSlewed = ChargingVoltageTarget;
          }
        }

        // CC/CV phase determination — must be after the target slew above.
        // Bumpless transfer: track cv_I toward the operating-point value when CV is inactive.
        // While CV is active, cv_I_track stays in sync for seamless re-entry.
        {
          static float cv_I_track = 0.0f;
          static uint32_t awSeedProtectStartMs = 0;  // timestamp when last bumpless seed fired

          // ── CV entry bumpless seed ─────────────────────────────────────────────────
          // Seeds cv_I at voltageControlActive entry so that Icv = Kp*e + cv_I ≈ g_pidI_filtered,
          // preventing a step change in setpointLimited. Resets cv_I_aw_cap so a stale
          // post-event cap does not constrain the CV loop on entry.
          //   seed = g_pidI_filtered - Kp*e  →  Icv = g_pidI_filtered  →  no step in setpointLimited.
          if (voltageControlActive && enteringCV) {
            float e_cv = ChargingVoltageTarget - IBV;  // raw INA228 — bumpless seed uses true voltage
            float seed = clamp_f(g_pidI_filtered - VoltageKp_active * e_cv, 0.0f, (float)uTargetAmps);
            cv_I = seed;
            cv_I_track = seed;
            cv_I_aw_cap = (float)MaxTableValue;    // clear AW cap — stale values constrain CV entry
            awSeedProtectStartMs = currentMillis;  // start seed-protection window
          }
          // ── Unified protection-release reseed + unified telemetry export ───────────
          // Single falling-edge handler for ALL three protection paths (Group 1/2 OV,
          // iExcess, LoadDump). Fires when fastOvClampActive goes 1 → 0 — i.e., every
          // protection has cleared. Uses preEventCvI captured back when no protection
          // was clamping, so chained/overlapping events don't burn intermediate
          // snapshots. Engages AwSeedProtectMs so the per-tick cv_I bleed below
          // cannot wipe the reseed if a new event re-fires within the protect window.
          //
          // g_fastOvClampActive (read here, written at end of this block) is the
          // unified flag — every supervisor has voted by the time we reach this point.
          if (voltageControlActive && g_fastOvClampActive && !fastOvClampActive) {
            float icvHi_seed = clamp_f((float)uTargetAmps, 0.0f, (float)MaxTableValue);
            cv_I = clamp_f(preEventCvI * ReseedFrac, 0.0f, icvHi_seed);
            cv_I_track = cv_I;
            awSeedProtectStartMs = currentMillis;     // engage seed-protection window
            postProtectRiseStartMs = currentMillis;   // open fast-rise window (closed by either time cap or voltage gate at slew site)
          }
          // Unified-flag rising-edge counter — counts every distinct activation of
          // ANY protection (G1/2 OV, iExcess, LoadDump). Must be incremented BEFORE
          // g_fastOvClampActive is updated for next tick.
          if (fastOvClampActive && !g_fastOvClampActive) {
            g_fastOvClampCount++;
          }
          g_fastOvCurrentCap = fastOvCurrentCap;  // export unified cap (post all supervisors)
          g_fastOvCapReason = capReasonTick;      // export reason atomically with the cap — CV log reads a coherent pair
          g_fastOvClampActive = fastOvClampActive;  // commit unified flag for next tick
          // Latch which protection bound the cap this tick into the Plots-tab marker
          // bitmask (consumed + cleared by the CSV1 sender). capReasonTick is a faithful
          // proxy for fastOvClampActive — every clamp site sets both.
          switch (capReasonTick) {
            case CAP_REASON_KHARD_G1:
            case CAP_REASON_KHARD_G2:     g_protEventLatch |= PROT_EVT_OV; break;
            case CAP_REASON_IEXCESS:
            case CAP_REASON_IEXCESS_BULK: g_protEventLatch |= PROT_EVT_IX; break;
            case CAP_REASON_LOADDUMP:     g_protEventLatch |= PROT_EVT_LD; break;
            default: break;
          }
          bool seedProtected = (AwSeedProtectMs > 0) && ((currentMillis - awSeedProtectStartMs) < (uint32_t)AwSeedProtectMs);

          // Anti-windup ceiling: bleeds down while fastOV is active so the bumpless
          // tracker cannot immediately re-wind cv_I to the bulk-charging level after
          // each overshoot. Recovers gradually after fastOV clears, giving the battery
          // time to settle before full current ramps back up.
          // cv_I_aw_cap is declared above the iExcess block so both blocks share it.
          // AwBleedRate is a user-adjustable global (NVS-persisted, settings namespace). AwRecoverRate is hardcoded (0.1f).
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
              float e_bt = ChargingVoltageTarget - IBV;  // raw INA228 — no filter lag on bumpless tracker
              float cv_I_target = clamp_f(g_pidI_filtered - VoltageKp_active * e_bt, 0.0f, icvHi_bt);
              const float Kt = 2.0f;
              cv_I_track += Kt * (cv_I_target - cv_I_track) * actualDtSec;
              cv_I_track = clamp_f(cv_I_track, 0.0f, icvHi_bt);
              cv_I = cv_I_track;
            } else {
              cv_I_track = cv_I;  // keep tracker in sync during seed-protection window
            }
            g_awState = 4;  // not in CV mode; bumpless tracker owns cv_I
          } else {
            cv_I_track = cv_I;
            // Per-tick cv_I bleed during active fastOV: voltage PI fires at 100ms but
            // the integrator needs to reduce every 5ms tick to counteract bulk-phase wind-up.
            // Suppressed during seed-protection window to preserve the seeded cv_I value.
            if (fastOvClampActive && !seedProtected) {
              cv_I = fmaxf(0.0f, cv_I - awBleedAmpS * actualDtSec);
              cv_I_track = cv_I;
              g_awState = 3;  // actively bleeding cv_I due to fastOV protection
            }
          }
        }

        if (voltageControlActive) {
          bool cvLoopFired = enteringCV || ((currentMillis - lastVoltageLoopMs) >= VoltageLoopInterval);

          if (cvLoopFired) {
            uint32_t prevVoltageLoopMs = lastVoltageLoopMs;
            lastVoltageLoopMs = currentMillis;
            if (prevVoltageLoopMs != 0) {
              uint32_t actualIv = currentMillis - prevVoltageLoopMs;
              g_voltLoopActualIntervalMs = (uint16_t)(actualIv < 65535u ? actualIv : 65535u);
            } else {
              g_voltLoopActualIntervalMs = 0;
            }
            voltLoop_record(currentMillis);  // feed the CV-interval ladder (vl_*); vlHasPrev re-baselines on CV exit
            pidLog_voltageLoopRanThisTick = 1;
            pidLog_enteringCV = enteringCV ? 1 : 0;

            float e = voltageTargetSlewed - IBV;  // raw INA228 — no filter lag on PI error (governor output)
            float dtSec = (prevVoltageLoopMs == 0)
                            ? ((float)VoltageLoopInterval / 1000.0f)
                            : ((float)(currentMillis - prevVoltageLoopMs) / 1000.0f);
            dtSec = constrain(dtSec, 0.001f, 0.5f);

            float icvHi = clamp_f((float)uTargetAmps, 0.0f, (float)MaxTableValue);
            float icvLo = 0.0f;

            // cvDSlope: backward diff of getFiltV() over one voltage loop interval (V/s).
            // Uses filtered IBV so slope bleed does not react to measurement noise.
            // D term removed — this signal now feeds SlopeBleedK only.
            {
              static float vPrevCV = 0.0f;
              if (enteringCV) {
                cvDSlope = 0.0f;
                vPrevCV = getFiltV();
              } else {
                float vNow = getFiltV();
                if (dtSec > 0.001f) cvDSlope = constrain((vNow - vPrevCV) / dtSec, -4.0f, 4.0f);
                rollUpdate(ROLL_CVSLOPE, cvDSlope);   // slope-bleed gate-tuning readout
                vPrevCV = vNow;
              }
            }

            if (!enteringCV) {
              float p = VoltageKp_active * e;
              float unsat = p + cv_I;
              Icv = clamp_f(unsat, icvLo, icvHi);

              bool satHi = (Icv >= icvHi);
              bool satLo = (Icv <= icvLo);
              // cvHelpersEnabled OFF → symmetric plain PI (integrator unwinds at the same VoltageKi rate it builds);
              // ON → asymmetric 7× faster unwind above target (aggressive overshoot recovery). See "CV tuning helpers" toggle.
              float KiDown = cvHelpersEnabled ? 7.0f * VoltageKi_active : VoltageKi_active;
              float dI = (e >= 0.0f ? VoltageKi_active : KiDown) * e * dtSec;

              bool supervisorLimiting = fastOvClampActive && ((float)uTargetAmps < uTargetRaw_cached - 0.01f);
              if (supervisorLimiting && dI > 0.0f) {
                g_awState = 1;
                // fast OV supervisor is actively capping ceiling; freeze upward integration
              } else if (!(satHi && dI > 0.0f) && !(satLo && dI < 0.0f)) {
                cv_I += dI;
                g_awState = 0;
              } else {
                g_awState = 2;  // PID output at ceiling or floor; standard anti-windup
              }

              // Slope-aware integrator bleed — drains cv_I when voltage is rising faster than
              // SlopeBleedThresh (V/s). proxGain scales bleed linearly with proximity to setpoint:
              // zero when e >= SlopeBleedProxV (far below target), full when e <= 0 (at or above).
              // Prevents bleed from firing during a legitimate fast rise toward a distant target.
              // KiDown still handles steady-state correction above setpoint independently.
              // Gated by cvHelpersEnabled — OFF disables slope bleed entirely for clean symmetric-PI tuning.
              if (cvHelpersEnabled && cvDSlope > SlopeBleedThresh) {
                float proxGain = clamp_f(1.0f - e / SlopeBleedProxV, 0.0f, 1.0f);
                float slopeBleedAmps = SlopeBleedK * (cvDSlope - SlopeBleedThresh) * dtSec * proxGain;
                cv_I = fmaxf(0.0f, cv_I - slopeBleedAmps);
                g_slopeBleedAmpsThisTick = slopeBleedAmps;  // captured for cvLog; cleared by cvLog_tick after logging
                // cv_I_track synced on next tick by bumpless tracker (out of scope here)
              }

              Icv = clamp_f(VoltageKp_active * e + cv_I, icvLo, icvHi);
            }
          }

          pidLog_uTargetBeforeVoltCap = i_ceiling_pre_ov;
          pidLog_uTargetAfterVoltCap = Icv;

        } else {
          pidLog_uTargetBeforeVoltCap = i_ceiling_pre_ov;
          pidLog_uTargetAfterVoltCap = (float)uTargetAmps;
          vlHasPrev = false;  // CV inactive — re-baseline the CV-interval ladder so the off-gap isn't logged
        }

        // Per-tick Icv recompute — proportional path responds every output current loop tick;
        // cv_I still updates only on VoltageLoopInterval cadence.
        {
          float e_now = voltageTargetSlewed - IBV;  // raw INA228 — no filter lag on per-tick proportional (governor output)
          float icvHi_tick = clamp_f((float)uTargetAmps, 0.0f, (float)MaxTableValue);
          if (!enteringCV) {
            Icv = clamp_f(VoltageKp_active * e_now + cv_I, 0.0f, icvHi_tick);
          }
        }

        // ===== CV TUNING SCORE ACCUMULATION =====
        // ISE/T scoring — same shape as current waveform test (errorAccum / activeTimeSec).
        // HIGH phase: asymmetric squared error — overshoot (above target) weighted by cvKOvershoot
        // ("Overshoot Penalty (K)" on the Voltage tab) vs undershoot weighted ×1.
        // LOW phase: one-sided squared error, normal weight (battery decay is physics-limited).
        // HIGH target = cvBaseTarget + cvWaveAmplitudeV (the step-up we're testing).
        if (CVTuningMode && voltageControlActive && cvTuningScore.waveHigh && cvTuningScore.ringInDone) {
          float highTarget = cvBaseTarget + cvWaveAmplitudeV;
          float e_high = IBV - highTarget;  // raw INA228 — no filter lag on tuning score
          float e_scored, w_high;
          if (e_high > 0.0f) {
            e_scored = fmaxf(0.0f, e_high - CV_HIGH_DEADBAND_V);  // dead-band: first 25mV of overshoot is free
            w_high = cvKOvershoot;
          } else {
            e_scored = e_high;  // approach (undershoot during rise) scored normally, no dead-band
            w_high = 1.0f;
          }
          cvTuningScore.totalIntegratedOvershootVs += e_scored * e_scored * w_high * actualDtSec;
          float peakOv = fmaxf(0.0f, e_high);
          if (peakOv > cvTuningScore.worstOvershootV) cvTuningScore.worstOvershootV = peakOv;

          if (!cvTuningScore.phaseSettled) {
            float vErr = fabsf(IBV - highTarget);  // raw INA228 — settle detection on true voltage
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
          cvTuningScore.rpmSum += RPM;
          float tempSample = isnan(AlternatorTemperatureF) ? TempToUse : AlternatorTemperatureF;
          if (!isnan(tempSample)) cvTuningScore.tempSum += tempSample;
          cvTuningScore.avgSampleCount++;
        }

        // ===== CV TUNING SCORE ACCUMULATION — LOW phase (return to normal setpoint after step-up) =====
        if (CVTuningMode && voltageControlActive && !cvTuningScore.waveHigh && cvTuningScore.ringInDone) {
          float lowTarget = cvBaseTarget;  // LOW phase is the normal setpoint; track voltage return from high step
          // Zero-crossing: voltage descending from HIGH must cross below lowTarget before re-overshoot counts
          if (!cvTuningScore.lowCrossedBelow && IBV < lowTarget) cvTuningScore.lowCrossedBelow = true;
          // Overshoot (above target): only after zero crossing — avoids penalising the natural descent from HIGH
          if (cvTuningScore.lowCrossedBelow) {
            float e_low = fmaxf(0.0f, IBV - lowTarget);
            cvTuningScore.totalLowIntOvVs += e_low * e_low * actualDtSec;
            if (e_low > cvTuningScore.worstLowOvV) cvTuningScore.worstLowOvV = e_low;
          }
          // Undershoot (below target): 1s grace from phase start, then weight ramps 0→1 over 10s, capped at 1
          float undershootErr = fmaxf(0.0f, lowTarget - IBV);
          if (undershootErr > cvTuningScore.worstLowUndershootV) cvTuningScore.worstLowUndershootV = undershootErr;
          if (undershootErr > 0.0f) {
            float tPhaseSec = (currentMillis - cvTuningScore.lastToggleMs) / 1000.0f;
            if (tPhaseSec > CV_LOW_GRACE_SEC) {
              float w_under = fminf(1.0f, (tPhaseSec - CV_LOW_GRACE_SEC) / CV_LOW_RAMP_SEC);
              cvTuningScore.totalLowUndershootVs += w_under * undershootErr * undershootErr * CV_UNDERSHOOT_SCALE * actualDtSec;
            }
          }
          cvTuningScore.activeTimeSec += actualDtSec;  // LOW time counts toward the shared denominator

          if (!cvTuningScore.lowPhaseSettled) {
            float vErr = fabsf(IBV - lowTarget);  // raw INA228 — settle detection on true voltage
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

        float effectiveFallRate = fastOvClampActive ? 1.0e9f : SetpointFallRate;
        // Post-protection fast-rise: while the window is open AND battV still has
        // clear headroom below the active charge target, multiply the normal rise
        // slew so the alternator crosses its deadband and starts producing again
        // quickly. Window opens on the unified fastOvClampActive falling edge above;
        // closes when EITHER the time cap expires OR battV climbs into the headroom.
        bool postProtectRiseActive =
            voltageControlActive &&
            (postProtectRiseStartMs != 0) &&
            ((currentMillis - postProtectRiseStartMs) < FastSetpointRiseWindowMs) &&
            (IBV < ChargingVoltageTarget - FastSetpointRiseHeadroomV);
        float effectiveRiseRate;
        if (inStartupRamp)              effectiveRiseRate = StartupRiseRate;
        else if (postProtectRiseActive) effectiveRiseRate = SetpointRiseRate * FastSetpointRiseRate;
        // Large up-step gentling: when the commanded current jumps well above the current
        // slew-limited setpoint, ramp at the slower SetpointBigStepRiseRate until the remaining
        // gap closes to within SetpointBigStepThresh — then fall back to the normal fast rate for
        // the final approach (preserves snappy small-error corrections). Down-moves untouched.
        // Sits below the post-protection fast-rise so it never slows recovery out of a trip.
        else if ((setpointCommand - setpointLimited) > SetpointBigStepThresh)
                                        effectiveRiseRate = SetpointBigStepRiseRate;
        else                            effectiveRiseRate = SetpointRiseRate;
        setpointLimited = slew_limit_f(setpointLimited, setpointCommand,
                                       effectiveRiseRate, effectiveFallRate, actualDtSec);
        // Clear startup ramp once setpointLimited has caught up to command
        if (inStartupRamp && setpointLimited >= setpointCommand - 0.5f) {
          inStartupRamp = false;
        }

        // Output current PID compute.
        // MaintainMode regulates to 0 net battery amps. Feedback is always INA228 (Bcur),
        // never getBatteryCurrent() — picking Victron as Battery Current Source would add
        // ~1–2 s of lag that destabilizes this loop. The dropdown only governs SoC display.
        // Normal AUTO uses signal selected by OutputPIDSigSrc.
        {
          float pidSig = (OutputPIDSigSrc == 2) ? MeasuredAmps : (OutputPIDSigSrc == 1) ? g_pidMA_N
                                                                                        : g_pidI_filtered;
          targetCurrent = (MaintainMode == 1) ? Bcur : pidSig;
        }
        pidInput = (double)targetCurrent;
        pidSetpoint = (double)setpointLimited;
        pidError = setpointLimited - targetCurrent;
        currentPID.Compute();
      }

    } else {
      // ===== MANUAL mode: no setpoint management =====
      voltageControlActive = false;
      lastVoltageControlActive = false;  // keep tracker in sync so AUTO re-entry from MANUAL fires the bumpless CV seed
      uTargetAmps = 0;
      setpointLimited = 0.0f;
      pidInput = (double)((OutputPIDSigSrc == 2) ? MeasuredAmps : (OutputPIDSigSrc == 1) ? g_pidMA_N
                                                                                         : g_pidI_filtered);
    }  // end of MANUAL mode else
  }  // end if (!sysIDRunning)


  uint32_t aflM4 = micros();  // end of section 4: normal-mode control body (PID/CV/sysID)
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

  // Auto Min% learning: observe the applied floor vs. output to walk rpmMinDutyTable toward
  // (knee - margin). Observer only; gated to normal AUTO charging (no fault/shutdown/manual/sysID).
  {
    bool kneeModeOk = (sysMode == SYS_MODE_AUTO) && !sysIDRunning && tick.chargingEnabled
                      && !tick.inLockout && !IgnoreTemperature && (hardwarePresent == 1)
                      && !tick.currentDataStale;
    kneeLearnObserve(RPM, dutyNewFloat, TempToUse, MeasuredAmps,
                     dutyRequest, tick.rpmMinDuty, kneeModeOk);
  }

  if (sysMode == SYS_MODE_AUTO) {
    innerTermP = (float)currentPID.GetPterm();
    innerTermI = (float)currentPID.GetIterm();
    innerTermD = (float)currentPID.GetDterm();

    // Inner current loop Control Accuracy score — authority gate: the field actuator must be
    // actively modulating (duty off both rails — not pinned at 0% nor 100%), no protection owning
    // the output, past the startup ramp, and genuinely commanding current. A duty pinned at 100%
    // means we can't make more current (low RPM / out of headroom), so that gap is not a tuning
    // fault and isn't counted.
    // Slew gate (setpointCommand ≈ setpointLimited): while the slew-limited command is still
    // travelling toward a new target (up to SetpointRiseRate/SetpointFallRate per sec), MeasuredAmps
    // lags the moving command — on a commanded step-DOWN that lag reads as tens of amps of phantom
    // "over-current" that is physics, not tuning. Scoring is suppressed for the whole slew, then
    // held only ACC_SETTLE_CURRENT_MS (100 ms) after the command settles — short enough that a
    // genuinely slow or ringing loop (the current loop should settle in ~200 ms) is still caught.
    {
      bool binding = !g_fastOvClampActive && !inStartupRamp
                     && dutyCycle > 1.0f && dutyCycle < 99.0f
                     && setpointLimited > 2.0f
                     && fabsf(setpointCommand - setpointLimited) <= 2.0f;  // command caught up — not mid-slew
      if (accBindingReady(accCurrent.bindingStartMs, binding, tick.nowMs, ACC_SETTLE_CURRENT_MS)) {
        float err  = setpointLimited - MeasuredAmps;   // A; positive = under target
        float over = MeasuredAmps - setpointLimited;   // over-current side (damaging)
        accScoreAdd(accCurrent.errAccum, accCurrent.timeAccum, accCurrent.worstOver, err, over > 0.0f ? over : 0.0f, actualDtSec);
      }
    }

    // CV voltage loop Control Accuracy score — authority gate: voltage is genuinely the binding
    // constraint, i.e. the CV PID output Icv is off both rails — above zero and strictly below the
    // current ceiling uTargetAmps (if Icv is pinned at the ceiling we're current-limited, not
    // voltage-regulating). No protection clamp. Held 2 s before scoring (ACC_SETTLE_VOLTAGE_MS).
    // Target-step mirror of the current loop's slew gate: ChargingVoltageTarget steps instantly on
    // a stage transition (e.g. absorption→float), so right after a step-DOWN IBV sits above the new
    // target for the few ticks before Icv collapses below the binding floor — phantom over-voltage
    // that is the stage change, not tuning. Restart the settle timer on any target change so those
    // ticks are skipped. (ACC_SETTLE_VOLTAGE_MS stays at 2 s — the CV loop genuinely settles slowly.)
    {
      static float accPrevVTarget = 0.0f;
      if (fabsf(ChargingVoltageTarget - accPrevVTarget) > 0.01f) {
        accVoltage.bindingStartMs = 0;   // target stepped → restart settle
        accPrevVTarget = ChargingVoltageTarget;
      }
    }
    if (voltageControlActive) {
      bool binding = !g_fastOvClampActive && Icv > 0.5f && Icv < (uTargetAmps - 0.5f);
      if (accBindingReady(accVoltage.bindingStartMs, binding, tick.nowMs, ACC_SETTLE_VOLTAGE_MS)) {
        float vErr  = IBV - ChargingVoltageTarget;     // V (raw INA228); positive = overvoltage
        float vOver = vErr > 0.0f ? vErr : 0.0f;       // over-voltage side (battery-damaging)
        accScoreAdd(accVoltage.errAccum, accVoltage.timeAccum, accVoltage.worstOver, vErr, vOver, actualDtSec);
      }
    } else {
      accVoltage.bindingStartMs = 0;  // left CV → restart the settle timer next time it engages
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

  uint32_t aflM5 = micros();  // end of section 5: duty build + governor + state/telemetry
  // All temperature loop, output current PID, and duty pipeline state is now final for this tick.
  // Called only in the normal-mode path — shutdown/fault paths do not log here.
  pidLog_tick(currentMillis);
  // Inner Current PID firing-interval sample — reached only on a normal field-on tick
  // (past digitalWrite(4,HIGH)). loop() clears pfHasPrev while the field is off so the
  // first firing after a field-off stretch re-baselines instead of logging the off-gap.
  pidFire_record(currentMillis);
  // Best-Ever Front: fold one alternator-health sample per control tick (~200 Hz). Live only —
  // bench-sim folds at 1 Hz from altHealth_tick instead. Off/fault/shutdown early-return above this.
  if (altSimMode < 0.5f) TIMED_CALL(ft_altFold, altFold_tick(currentMillis));
  // Section profiler latch — full passes only; early returns above never reach here.
  // A whole-core preemption is charged to whichever section it landed in, same caveat
  // as the Function Timing table. Breakdown surfaces in /debug, resets with Reset Peak Values.
  {
    uint32_t aflEnd = micros();
    uint32_t aflTot = aflEnd - aflT0;
    if (aflTot > aflWorstTotalUs) {
      aflWorstTotalUs = aflTot;
      aflWorstSecUs[0] = aflM0 - aflT0;   // thermal log
      aflWorstSecUs[1] = aflM1 - aflM0;   // RPM tables + tick snapshot
      aflWorstSecUs[2] = aflM2 - aflM1;   // temp warning + fast-OV supervisor + limp gate
      aflWorstSecUs[3] = aflM3 - aflM2;   // CH1 gate + stage/governor/mode transitions
      aflWorstSecUs[4] = aflM4 - aflM3;   // normal-mode control body (PID/CV/sysID)
      aflWorstSecUs[5] = aflM5 - aflM4;   // duty build + governor + state/telemetry
      aflWorstSecUs[6] = aflEnd - aflM5;  // pidLog + PID-interval sample + alt-health fold
    }
  }
  prevMode = mode;
}

void setDutyPercent(float percent) {
  static uint32_t lastFrequency = 0;
  static bool pwmInitialized = false;

  percent = constrain(percent, 0.0f, 100.0f);

  // Field-duty safety net for higher-voltage banks. Every duty path (Auto/manual/limp/fault) lands
  // here, so this is the one place that hard-bounds field duty even on the open-loop paths that bypass
  // the PID's MaxDuty limit (manual/limp/fault). MaxDuty is the real per-bus cap (its default is scaled
  // down on 24/48V so worst-case field current never exceeds the 12V case); clamp to it. Gated to >12V
  // so 12V manual mode keeps its full-duty bypass. This is a duty-ratio proxy, not a measured amp limit.
  if (BATTERY_VOLTAGE > 12 && percent > MaxDuty) {
    percent = MaxDuty;
  }

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
  const float v = getFiltV();
  const int soc = SOC_percent;

  // Two-sided hysteresis: timer arms when V reaches BulkVoltage − ENTER, resets only when V falls below BulkVoltage − EXIT. Prevents 30–50 mV idle noise from constantly resetting the hold timer.
  const float BULK_V_BAND_ENTER = 0.05f;
  const float BULK_V_BAND_EXIT  = 0.10f;

  if (inBulkStage && !inAbsorptionStage) {
    // ===== BULK (CC) =====
    ChargingVoltageTargetReq = BulkVoltage;

    if (bulkVoltageHoldTimer == 0) {
      if (v >= (BulkVoltage - BULK_V_BAND_ENTER)) bulkVoltageHoldTimer = now;
    } else if (v < (BulkVoltage - BULK_V_BAND_EXIT)) {
      bulkVoltageHoldTimer = 0;
    } else if ((uint32_t)(now - bulkVoltageHoldTimer) >= bulkVoltageHoldMs) {
      inAbsorptionStage = true;
      ChargingVoltageTargetReq = AbsorptionVoltage;
      absorptionStartTime = now;
      absorptionTailTimer = 0;
      bulkVoltageHoldTimer = 0;
      queueConsoleMessageF(
        "Stage: BULK→ABSORPTION (hold timer) | battV=%.2fV bulkTarget=%.2fV absTarget=%.2fV tailCurrent=%.1fA timeout=%.0fmin",
        v, BulkVoltage, AbsorptionVoltage, TailCurrent_A,
        (float)AbsorptionTimeoutMs / 60000.0f);
    }

  } else if (inBulkStage && inAbsorptionStage) {
    // ===== ABSORPTION (CV) =====
    ChargingVoltageTargetReq = AbsorptionVoltage;

    const bool thermallyConstrained = (thermalPenaltyAmps > 2.0f) && (uTargetAmps <= TailCurrent_A * 2.0f);  //if you ever want CV-awareness here you'd change it to Icv <= TailCurrent_A * 2.0f.
    const bool tailReached = !thermallyConstrained && (Bcur <= TailCurrent_A);
    const bool timedOut = ((uint32_t)(now - absorptionStartTime) >= AbsorptionTimeoutMs);

    static bool lastThermallyConstrained = false;
    static uint32_t lastTailConstraintLogMs = 0;
    // Throttle to once per 10s — thermal constraint can flap rapidly under oscillating penalty
    if (thermallyConstrained != lastThermallyConstrained && (uint32_t)(now - lastTailConstraintLogMs) >= 10000) {
      if (thermallyConstrained) {
        queueConsoleMessageF(
          "Absorption: tail detection suppressed (thermal) | penalty=%.1fA uTarget=%.1fA tailThresh=%.1fA",
          thermalPenaltyAmps, uTargetAmps, TailCurrent_A);
      } else {
        queueConsoleMessageF(
          "Absorption: tail detection resumed | penalty=%.1fA uTarget=%.1fA tailThresh=%.1fA",
          thermalPenaltyAmps, uTargetAmps, TailCurrent_A);
      }
      lastTailConstraintLogMs = now;
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

    ChargingVoltageTargetReq = FloatVoltage;

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
  // Commissioning outranks every charge stage: whenever the wizard dialog is OPEN (its
  // commissionHeartbeat is still fresh) the device is being commissioned, so both the live
  // displays and the logged stage read COMMISSIONING — regardless of field state, including
  // the low-duty idle-rest hold between guided steps. A closed/crashed dialog goes stale within
  // COMMISSION_HEARTBEAT_TIMEOUT_MS and the stage reverts to whatever charging is actually doing.
  // Deliberately NOT keyed off commissionState: a paused-but-closed commission is not "commissioning".
  if (lastCommissionHeartbeatMs != 0 &&
      (uint32_t)(millis() - lastCommissionHeartbeatMs) < COMMISSION_HEARTBEAT_TIMEOUT_MS) {
    return CHARGE_STAGE_COMMISSION;
  }

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

  // Scale the warning threshold by bank class (12V-equivalent → pack-space), exactly like the
  // critical detector below. A fixed 0.15V is normal sensor spread on a 48V bank and would otherwise
  // false-trip MODE_WARNING_RAMP_AND_LOCKOUT (which DISABLES charging) on a healthy 24/48V system.
  if (fabsf(batteryV - ibv) > VoltageDisagreeThreshold * (float)BATTERY_VOLTAGE / 12.0f) {
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
    case MODE_COMMISSION_IDLE: return "COMMISSION_IDLE";
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
    case REASON_FAST_OVERVOLTAGE: return "FAST_OVERVOLTAGE";
    case REASON_HARD_OVERCURRENT: return "HARD_OVERCURRENT";
    case REASON_RPM_TOO_LOW: return "RPM_TOO_LOW";
    case REASON_CURRENT_STALE: return "CURRENT_STALE";
    case REASON_BATTERY_TOO_COLD: return "Battery too cold to charge";
    case REASON_COMMISSION_REST: return "Commissioning idle (field resting)";

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

  // PRIORITY 1.5: FAST OVER-VOLTAGE — absolute ceiling, fires in ALL modes incl. MANUAL.
  // Live per-tick voltage vs AlternatorHardShutdownV. Reason REASON_FAST_OVERVOLTAGE is in
  // shouldImmediatelyCutGPIO4 → instant field cut + 30s lockout. Deliberately ABOVE the manual
  // branch (manual otherwise bypasses all safeties) and NOT gated by testProtectionsEnabled,
  // because over-voltage is always disastrous. Must mirror selectFieldEventReason.
  if (tick.currentBatteryVoltage > tick.alternatorHardShutdownV) {
    return MODE_WARNING_RAMP_AND_LOCKOUT;
  }

  // PRIORITY 2: MANUAL MODE (UNRESTRICTED - bypasses all safeties when user wants manual control)
  if (tick.manualMode) {
    return MODE_NORMAL_MANUAL;
  }

  // PRIORITY 3: RPM GATE (field must be cut when engine is not running)
  if (tick.rpmBelowMinimum) {
    return MODE_CRITICAL_RAMP;
  }

  // PRIORITY 3.5: COLD-CHARGE LOCKOUT (opt-in lithium protection — board temp proxy below floor).
  // Graceful ramp-to-zero + lockout (not an instantaneous emergency, so no hard chop); the field
  // settle-cut path finishes the disconnect. Below MANUAL (manual stays unrestricted), so it only
  // applies in AUTO — mirrors selectFieldEventReason. Hysteresis lives in buildTickSnapshot.
  if (tick.batteryTooCold) {
    return MODE_WARNING_RAMP_AND_LOCKOUT;
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
  // (The AlternatorHardShutdownV over-voltage check moved up to PRIORITY 1.5 as the fast-OV
  //  immediate cut — armed in every mode and ungated — so it is no longer repeated here.)
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

  // PRIORITY 7.6: COMMISSIONING IDLE REST — between guided wizard steps (session live, dialog alive,
  // no test running), hold the field at a low duty instead of resuming bulk/CV charging. Below every
  // safety/lockout gate above (a real fault still wins and ramps to 0); above NORMAL AUTO. Only ever
  // reached in AUTO, since MANUAL returns at PRIORITY 2. Mirrors selectFieldEventReason.
  if (tick.commissioningResting) {
    return MODE_COMMISSION_IDLE;
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

  // Priority 1.5: Fast over-voltage — absolute ceiling, above the manual bypass and ungated.
  // Mirrors selectFieldControlMode PRIORITY 1.5. Live voltage → immediate cut + 30s lockout.
  if (tick.currentBatteryVoltage > tick.alternatorHardShutdownV) return REASON_FAST_OVERVOLTAGE;

  // Priority 2: Manual mode
  if (tick.manualMode) return REASON_MANUAL_MODE;

  // Priority 3: RPM gate
  if (tick.rpmBelowMinimum) return REASON_RPM_TOO_LOW;

  // Priority 3.5: Cold-charge lockout (opt-in lithium protection). Mirrors selectFieldControlMode.
  if (tick.batteryTooCold) return REASON_BATTERY_TOO_COLD;

  // Priority 4: Critical (auto mode only)
  if (tick.tempDataVeryStale && !tick.ignoreTemperature) return REASON_TEMP_STALE;
  if (tick.currentDataStale) return REASON_CURRENT_STALE;
  if (!tick.voltagePlausible) return REASON_VOLTAGE_IMPLAUSIBLE;
  if (tick.voltageDisagreementCritical) return REASON_VOLTAGE_DISAGREE_CRITICAL;
  if (!tick.ignoreTemperature && tick.tempToUseF > (tick.tempLimitF + tick.tempCritExcessF)) {
    return REASON_TEMP_CRITICAL;
  }

  // Priority 5: Warning
  // (AlternatorHardShutdownV over-voltage moved up to Priority 1.5 as REASON_FAST_OVERVOLTAGE.)
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

  // Priority 7.6: Commissioning idle-rest hold (not a fault). Mirrors selectFieldControlMode.
  if (tick.commissioningResting) return REASON_COMMISSION_REST;

  return REASON_NONE;
}

// Tracks rising edges on reasons that map to protection counters.
// Called once per loop tick after selectFieldEventReason().
void updateProtectionCounters(FieldEventReason reason) {
  static FieldEventReason prevReason = REASON_NONE;
  if (reason != prevReason) {
    switch (reason) {
      case REASON_INA_OVERVOLTAGE: g_inaOVCount++; break;
      case REASON_HARD_OVERCURRENT: g_hardOCCount++; break;
      case REASON_FAST_OVERVOLTAGE: g_voltSpikeCount++; break;  // reuses the OV-spike counter (REASON_VOLTAGE_SPIKE retired)
      case REASON_VOLTAGE_SPIKE: g_voltSpikeCount++; break;
      case REASON_VOLTAGE_DISAGREE_CRITICAL: g_voltDisagreeCritCount++; break;
      case REASON_VOLTAGE_DISAGREE_WARNING: g_voltDisagreeWarnCount++; break;
      case REASON_VOLTAGE_IMPLAUSIBLE: g_voltImplausibleCount++; break;
      case REASON_TEMP_CRITICAL: g_tempCritCount++; break;
      case REASON_TEMP_SUSTAINED: g_tempSustainedCount++; break;
      case REASON_TEMP_STALE: g_tempStaleCount++; break;
      case REASON_CURRENT_STALE: g_currentStaleCount++; break;
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
    float remaining = (activeCollapseDelay - (tick.nowMs - fieldCollapseTime)) / 1000.0f;
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
  if (reason == REASON_FAST_OVERVOLTAGE) return true;   // absolute OV ceiling — instant cut in every mode
  if (reason == REASON_INA_OVERVOLTAGE) return true;
  if (reason == REASON_HARD_OVERCURRENT) return true;
  if (reason == REASON_RPM_TOO_LOW) return true;
  // Stale current must cut here: a dead CH1 means ch1FreshFlag never sets, so
  // AdjustFieldLearnMode returns at its freshness gate before the ramp-down /
  // settle-cut path ever runs — without this the field freezes at last duty.
  // (CURRENT_STALE also outranks TEMP_CRITICAL in selectFieldEventReason, so
  // a non-cutting stale reason would mask the temp-critical cut too.)
  if (reason == REASON_CURRENT_STALE) return true;
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
    case REASON_FAST_OVERVOLTAGE:   // normally cut immediately; defensive if it ever reaches settle
    case REASON_VOLTAGE_SPIKE:
    case REASON_VOLTAGE_DISAGREE_WARNING:
    case REASON_VOLTAGE_IMPLAUSIBLE:
    case REASON_VOLTAGE_DISAGREE_CRITICAL:
    case REASON_TEMP_STALE:
    case REASON_CURRENT_STALE:
    case REASON_HARD_OVERCURRENT:
    case REASON_RPM_TOO_LOW:
    case REASON_BATTERY_TOO_COLD:   // cold-charge lockout: ramp to zero, then cut once settled
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
  tick.ignoreRPM = (IgnoreRPM != 0);
  tick.rpmBelowMinimum = (!tick.ignoreRPM && RPM < (float)MinRPMForField);

  // Engine confirmed stopped: RPM held at exactly 0 for >= RPM_ZERO_CUT_MS. RPM is already
  // floored to 0 below 100 in ReadAnalogInputs, so RPM <= 0 means a true zero. Skipped when
  // IgnoreRPM is set (no trustworthy RPM signal). Drives the immediate-cut override below.
  if (!tick.ignoreRPM && RPM <= 0.0f) {
    if (rpmZeroSinceMs == 0) rpmZeroSinceMs = currentMillis;
  } else {
    rpmZeroSinceMs = 0;
  }
  tick.engineFullyStopped = (rpmZeroSinceMs != 0 && (currentMillis - rpmZeroSinceMs) >= RPM_ZERO_CUT_MS);

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

  // Cold-charge lockout: latched with ColdChargeHysteresisF to stop chatter at the threshold.
  // Fail-open on a stale/NaN board sensor so a dead BMP388 can't brick charging.
  {
    static bool coldLatched = false;
    if (coldChargeLockoutEnable && !IS_STALE(IDX_AMBIENT_TEMP) && isfinite(ambientTemp)) {
      if (ambientTemp < MinChargeTempF) {
        coldLatched = true;
      } else if (ambientTemp >= MinChargeTempF + ColdChargeHysteresisF) {
        coldLatched = false;
      }
    } else {
      coldLatched = false;
    }
    tick.batteryTooCold = coldLatched;
  }

  // Commissioning idle-rest: session IN_PROGRESS + the wizard dialog still pinging + no commissioning
  // test currently driving the field. Stale heartbeat (closed/crashed dialog, dropped Wi-Fi) clears
  // it within COMMISSION_HEARTBEAT_TIMEOUT_MS so normal charging resumes. The Disturbances sweep
  // (faCommissionGate) needs live charging, so it suppresses rest like the other tests.
  {
    bool dialogAlive = (lastCommissionHeartbeatMs != 0) &&
                       ((uint32_t)(currentMillis - lastCommissionHeartbeatMs) < COMMISSION_HEARTBEAT_TIMEOUT_MS);
    // Include the PENDING request flags, not just the active ones. selectFieldControlMode returns
    // MODE_COMMISSION_IDLE → AdjustField early-returns at runCommissionIdle() BEFORE fieldCurve_tick/
    // systemID_tick run, so a freshly-queued request would never be consumed and fieldCurveActive/
    // systemIDActive would never rise — a permanent rest deadlock (the field ramp only started once the
    // dialog closed and resting cleared). Treating a pending request as "test active" lets the control
    // path fall through and consume it on the same tick.
    bool anyTestActive = (fieldCurveActive != 0) || (systemIDActive != 0) ||
                         fieldCurveRequested || systemIDRequested ||
                         TuningMode || CVTuningMode || faCommissionGate;
    tick.commissioningResting = (commissionState == 1) && dialogAlive && !anyTestActive;
  }

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
  tick.inLockout = (fieldCollapseTime > 0 && (tick.nowMs - fieldCollapseTime) < activeCollapseDelay);

  // Thresholds
  tick.bulkVoltage = BulkVoltage;
  tick.alternatorHardShutdownV = AlternatorHardShutdownV;
  tick.testProtectionsEnabled = testProtectionsEnabled;
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
  // Persist factory defaults for BOTH charge-rate modes explicitly, WITHOUT touching HiLow.
  // The old code force-set HiLow=1 here purely so saveUserTableEdits() would target the "capTable"
  // (Normal) key — but that silently kicked a Low-mode user back to Normal on every reset, including
  // the auto-reset that fires at boot when learning NVS is missing/invalid. We now write both mode
  // blobs directly and leave the user's selected mode alone.
  {
    float loDefaults[RPM_TABLE_SIZE];
    float loDefaultsPwr[RPM_TABLE_SIZE] = { 0 };
    for (int i = 0; i < RPM_TABLE_SIZE; i++) loDefaults[i] = defaultCapCurrentValues[i] * 0.25f;
    nvs_handle_t nvs_h;
    if (nvs_open("learning", NVS_READWRITE, &nvs_h) == ESP_OK) {
      nvs_set_blob(nvs_h, "rpmPoints", rpmTableRPMPoints, sizeof(rpmTableRPMPoints));
      nvs_set_blob(nvs_h, "capTable", defaultCapCurrentValues, sizeof(defaultCapCurrentValues));   // Normal/High
      nvs_set_blob(nvs_h, "capPowerTable", defaultCapPowerValues, sizeof(defaultCapPowerValues));  // Normal/High
      nvs_set_blob(nvs_h, "capTableLo", loDefaults, sizeof(loDefaults));                            // Low (25%)
      nvs_set_blob(nvs_h, "capPowerTableLo", loDefaultsPwr, sizeof(loDefaultsPwr));                 // Low
      nvs_set_u8(nvs_h, "capLimitMode", capLimitMode);
      nvs_set_blob(nvs_h, "minDutyTable", rpmMinDutyTable, sizeof(rpmMinDutyTable));
      nvs_commit(nvs_h);
      nvs_close(nvs_h);
    }
  }
  // Refresh the live arrays for the user's ACTUAL mode (Normal or Low) — not a forced Normal.
  loadCapTablesForMode(HiLow);
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
        rpmCapCurrentTable[i] = (HiLow == 1) ? defaultCapCurrentValues[i] : defaultCapCurrentValues[i] * 0.25f;
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
      rpmCapCurrentTable[i] = (mode == 1) ? defaultCapCurrentValues[i] : defaultCapCurrentValues[i] * 0.25f;
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
      rpmCapCurrentTable[i] = (mode == 1) ? defaultCapCurrentValues[i] : defaultCapCurrentValues[i] * 0.25f;
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
  float floorV = interpolateRPMTable(rpm, rpmMinDutyTable);
  // A zero floor means "no minimum field" (above the commissioned RPM ceiling, or bin 0): keep it a
  // hard zero. Never let a temp correction lift it off zero, or the field could be forced on past the
  // ceiling where it must be able to shut fully off.
  if (floorV <= 0.0f) return 0.0f;
  // Copper temp correction — only while the learner owns the table (off = literal hand-entered Min%).
  // Only the RESISTIVE part of the knee scales with field-winding resistance (~0.218 %/degF); the
  // constant brush/rectifier threshold (kneeFitA) does NOT. Stored floors are referenced to
  // kneeTempRefF; shift the resistive part (knee − kneeFitA) to the live case temp. knee = floor+margin.
  // (kneeFitA = 0 when no commissioning fit exists → falls back to scaling the whole knee, as before.)
  if (kneeLearnEnable && kneeTempComp && !isnan(AlternatorTemperatureF)) {
    float knee = floorV + kneeMarginPct;
    float resistive = knee - kneeFitA; if (resistive < 0) resistive = 0;
    floorV -= resistive * 0.00218f * (kneeTempRefF - AlternatorTemperatureF);
    if (floorV < 0) floorV = 0;
    if (floorV > kneeMaxFloorPct) floorV = kneeMaxFloorPct;
  }
  return floorV;
}
float getCapCurrentForRPM(float rpm) {
  return interpolateRPMTable(rpm, rpmCapCurrentTable);
}

// ====================================================================================
// AUTO MIN% LEARNING ("knee tracker")
// ====================================================================================
// Learns, per RPM bin, the field duty where output amps begin to build (the "knee") and parks
// rpmMinDutyTable a margin below it. Observer only: governor_apply is untouched — this rewrites
// rpmMinDutyTable like a user edit, and getMinimumFieldForRPM adds the live temp correction.
// Spec: Working Markdown Docs/MinDuty_Knee_Learning.md.
static bool kneeStateDirty = false;

// Called every control tick from the governor site (observer; cheap). Per RPM bin it runs a
// one-time probe: while parked at the floor and steady, capture a fresh zero, then step the floor
// up kneeStepPct each dwell until output rises kneeOnsetA above that zero (the knee), then lock the
// floor at knee - margin. A locked bin re-learns only if current (> kneeReArmA) later shows at its
// floor (knee dropped) — then the floor drops one margin step. Bin 0 is permanently locked at 0%.
// Stored floors are RAW duty @ kneeTempRefF; getMinimumFieldForRPM does the live temp correction.
void kneeLearnObserve(float rpm, float appliedDuty, float tF, float amps,
                      float dutyRequest, float rpmFloorDuty, bool modeOk) {
  static uint32_t lastMs = 0, steadySince = 0, lastStepMs = 0;
  static float fRpm = 0, fTemp = 0, fDuty = 0;
  static bool fInit = false;
  static int   probeBin = -1;     // bin whose probe is in progress (-1 = none)
  static bool  haveZero = false;  // fresh zero captured for the current probe
  static float probeZero = 0.0f;  // amps baseline captured at probe start (below the knee)

  uint32_t now = millis();
  uint32_t dtMs = (lastMs == 0) ? 0 : (now - lastMs);
  lastMs = now;

  kneeActiveBin = -1; kneeFloorActive = false; kneeSteadyNow = false;

  bool valid = modeOk && !isnan(rpm) && !isnan(tF) && !isnan(amps)
               && !isnan(appliedDuty) && rpm > 0;
  if (!valid) { fInit = false; steadySince = 0; probeBin = -1; haveZero = false; return; }

  // Detector-input EMA (~0.5 s) so the steady bands reject jitter, not real operating-point moves.
  if (!fInit || dtMs == 0 || dtMs > 3000) {
    fRpm = rpm; fTemp = tF; fDuty = appliedDuty; fInit = true;
    steadySince = 0;
  } else {
    float a = (float)dtMs / (500.0f + (float)dtMs);
    fRpm += a * (rpm - fRpm); fTemp += a * (tF - fTemp); fDuty += a * (appliedDuty - fDuty);
  }

  // Attribute to the nearest RPM breakpoint.
  int b = 0; float best = 1e9f;
  for (int i = 0; i < RPM_TABLE_SIZE; i++) {
    float d = fabsf(fRpm - (float)rpmTableRPMPoints[i]);
    if (d < best) { best = d; b = i; }
  }
  kneeActiveBin = b;

  // Bin 0 (lowest RPM) is permanently locked at 0% — no usable knee that slowly. Never probe it.
  if (b == 0) {
    kneeFloor[0] = 0; if (kneeLearnEnable) rpmMinDutyTable[0] = 0;
    if (probeBin == 0) { probeBin = -1; haveZero = false; }
    return;
  }

  // Floor active = the controller wants no more field than the floor — the low-output / topped /
  // thermally-limited regime where the floor (not the PID) is setting the field. dutyRequest is the
  // inner PID output, which is clamped to >= MinDuty and (via bumpless tracking) sits AT the applied
  // floor while the floor is binding. So "floor binding" reads as dutyRequest <= floorEff, NOT
  // < floorEff: the old "< floorEff - 0.05f" could never be true once floorEff == MinDuty (the PID
  // output can't go below MinDuty), so every bin sat at 0% forever and the learner never left "watching".
  float floorEff = (rpmFloorDuty > MinDuty) ? rpmFloorDuty : MinDuty;
  bool floorActive = (dutyRequest <= floorEff + 0.05f);
  kneeFloorActive = floorActive;

  // Steady-band check vs. the EMA (RPM, temp, applied duty — voltage intentionally not gated).
  float rpmTolAbs = fmaxf(20.0f, fRpm * (kneeRpmTolPct / 100.0f));
  bool steady = (fabsf(rpm - fRpm) <= rpmTolAbs)
              && (fabsf(tF - fTemp) <= kneeTempTolF)
              && (fabsf(appliedDuty - fDuty) <= kneeDutyTolPct);
  if (!steady) { steadySince = 0; if (probeBin == b) { probeBin = -1; haveZero = false; } return; }
  if (steadySince == 0) { steadySince = now; lastStepMs = now; }
  kneeSteadyNow = true;

  if (!kneeLearnEnable || !floorActive) return;
  uint32_t dwellMs = (uint32_t)(kneeDwellSec * 1000.0f); if (dwellMs < 250) dwellMs = 250;
  if ((now - steadySince) < dwellMs) return;   // initial settle after entering a steady window
  if ((now - lastStepMs) < dwellMs) return;    // one action per dwell
  lastStepMs = now;

  // The duty-domain knobs — kneeStepPct (staircase), kneeMarginPct (park margin / step-down),
  // kneeMaxFloorPct (ceiling) — are stored in REAL duty-% for THIS bus, so they're used as-is here
  // (no runtime normalization — WYSIWYG with the dashboard box). Their defaults are scaled by
  // ×(12/BATTERY_VOLTAGE) at first boot (kneeLearnInit) and rescaled in place on a system-voltage
  // change (applyNominalVoltageChange) so a 5% 12V margin shows/uses as 1.25% on a 48V bank — keeping
  // the field-CURRENT margin constant without hiding the math. Amps-domain knobs (kneeOnsetA,
  // kneeReArmA) and the learned floors are voltage-independent (the probe observes real onset).
  float stepEff = kneeStepPct;
  float maxFloorEff = kneeMaxFloorPct;
  float margin = (kneeMarginPct > 0) ? kneeMarginPct : 0;
  float caseF = isnan(AlternatorTemperatureF) ? 0.0f : AlternatorTemperatureF;

  if (kneeFrozen[b]) {
    // Locked: only act if the knee dropped below the floor (real current at the floor). Lower one step.
    if (amps > kneeReArmA) {
      kneeFloor[b] -= margin; if (kneeFloor[b] < 0) kneeFloor[b] = 0;
      kneeKnee[b] = kneeFloor[b] + margin;
      rpmMinDutyTable[b] = kneeFloor[b];
      kneeLastMs[b] = now;
      kneeStateDirty = true; settingsDirty = true;
    }
    probeBin = -1; haveZero = false;
    return;
  }

  // Probing this bin.
  if (probeBin != b || !haveZero) {
    // (Re)start the probe: capture a fresh zero at the current floor (we are below the knee here).
    probeBin = b; probeZero = amps; haveZero = true;
    rpmMinDutyTable[b] = kneeFloor[b];   // ensure the applied floor matches our probe state
    return;
  }

  if ((amps - probeZero) >= kneeOnsetA) {
    // Knee found at the current commanded floor. Lock at knee - margin (raw duty @ kneeTempRefF).
    kneeKnee[b]  = kneeFloor[b];
    kneeFloor[b] = kneeKnee[b] - margin; if (kneeFloor[b] < 0) kneeFloor[b] = 0;
    kneeFrozen[b] = true; kneeLearnTempF[b] = caseF;
    rpmMinDutyTable[b] = kneeFloor[b];
    probeBin = -1; haveZero = false;
    kneeLastMs[b] = now;
  } else {
    // No output yet: step the floor up one increment and keep hunting.
    kneeFloor[b] += stepEff;
    if (kneeFloor[b] >= maxFloorEff) {
      // Hit the ceiling without onset → no usable knee at this RPM. Lock at the ceiling.
      kneeFloor[b] = maxFloorEff;
      kneeKnee[b] = maxFloorEff + margin;
      kneeFrozen[b] = true; kneeLearnTempF[b] = caseF;
      probeBin = -1; haveZero = false;
      kneeLastMs[b] = now;
    }
    rpmMinDutyTable[b] = kneeFloor[b];
  }
  kneeStateDirty = true;
  settingsDirty = true;   // push the CSV3 Min% echo promptly so the table cells track the learned floor (not the 60s heartbeat)
}

// Least-squares fit of the physical onset model  knee = a + C/RPM  over the committed anchors.
// Physics: in the linear field region the onset duty (where output current just begins) goes as
// R_field/(k·RPM) — inversely with RPM, battery-voltage-independent to first order. `a` absorbs the
// small constant rectifier/brush threshold; `C` is the lumped R_field/k at kneeTempRefF. Only the
// resistive C/RPM term carries temperature (copper R, ~0.218 %/°F of itself); the `a` threshold does
// NOT. So instead of temp-normalizing the whole onset (which wrongly dragged `a` with temperature),
// each anchor is regressed against x = (R(T)/R(Tref))/RPM with y = the RAW measured onset — the fitted
// `a` then comes out temperature-neutral and a + C/RPM evaluates directly as the kneeTempRefF knee.
// With a 2-parameter model, 3 anchors give the fit PLUS one residual to spot a bad point — hence the UI
// captures exactly 3. Outputs the worst |measured − fit| residual and which anchor it was, for the
// review screen's outlier flag. Returns false if < 2 anchors or the RPM span is degenerate.
bool kneeFitModel(float &outA, float &outC, float &outResidPct, int &outWorstIdx) {
  outA = 0.0f; outC = 0.0f; outResidPct = -1.0f; outWorstIdx = -1;
  if (kneeAnchorN < 2) return false;
  // Regress y = a + C·x with y = RAW onset and x = (R(T)/R(Tref))/RPM, so the copper-resistance ratio
  // lives in the regressor and `a` (the temp-independent threshold) is not temperature-scaled.
  double sx = 0, sy = 0, sxx = 0, sxy = 0;
  int n = kneeAnchorN;
  for (int i = 0; i < n; i++) {
    float tCorr = 1.0f + 0.00218f * (kneeAnchorTempF[i] - kneeTempRefF);   // R(T)/R(Tref)
    double x = (kneeAnchorRPM[i] > 1.0f) ? (double)tCorr / kneeAnchorRPM[i] : 0.0;
    double y = kneeAnchorDuty[i];
    sx += x; sy += y; sxx += x * x; sxy += x * y;
  }
  double denom = (double)n * sxx - sx * sx;
  if (fabs(denom) < 1e-12) return false;       // all anchors at ~same RPM — no leverage on C
  double C = ((double)n * sxy - sx * sy) / denom;
  double a = (sy - C * sx) / (double)n;
  if (C < 0) C = 0;                            // onset must rise as RPM falls; clamp a perverse fit
  outA = (float)a; outC = (float)C;
  // Worst residual (in % duty): predict each anchor's onset at ITS OWN temp (x carries R(T)/R(Tref))
  // and compare to the raw measured onset.
  float worst = 0.0f; int worstIdx = -1;
  for (int i = 0; i < n; i++) {
    float tCorr = 1.0f + 0.00218f * (kneeAnchorTempF[i] - kneeTempRefF);
    float pred = (float)(a + C * ((kneeAnchorRPM[i] > 1.0f) ? (double)tCorr / kneeAnchorRPM[i] : 0.0));
    float r = fabsf(kneeAnchorDuty[i] - pred);
    if (r > worst) { worst = r; worstIdx = i; }
  }
  outResidPct = worst; outWorstIdx = worstIdx;
  return true;
}

// Fit the Min% column from the committed onset-knee anchors (commissioning) via the kneeFitModel()
// physical curve above, then write it into rpmMinDutyTable (the learner's FROZEN baseline, so the
// background re-arm just maintains drift from here). Bin 0 stays locked at 0%. Returns false if < 3
// anchors (the UI captures 3 before Apply; this is the matching firmware guard).
//
// END BEHAVIOR (SAFETY-CRITICAL — do not change to flat-hold):
//   - BELOW the lowest anchor RPM: the model continues (onset keeps rising), clamped to maxKnee. Safe
//     — a floor at low RPM never prevents the field from shutting off.
//   - ABOVE the highest anchor RPM: force the floor to ZERO. The floor is a MINIMUM field the
//     regulator is forced to hold; carrying any floor past the commissioned range would force minimum
//     excitation the regulator could never turn off at high RPM → overvoltage/runaway. The UI captures
//     the user's MAXIMUM working RPM as the top anchor for exactly this reason — it is the zero-ceiling.
//     (A 1/RPM model is monotone-decreasing by construction, so it can never PRODUCE a dangerous high
//     floor at high RPM either — but the hard zero above the ceiling is the guarantee.)
bool kneeCurveApply() {
  if (kneeAnchorN < 3) return false;

  // Sort anchors ascending by RPM (tiny n — insertion sort). The fit is order-independent, but the
  // ceiling (= last anchor's RPM) and the diagnostic temp average read cleanest from sorted anchors.
  for (int i = 1; i < kneeAnchorN; i++) {
    float r = kneeAnchorRPM[i], d = kneeAnchorDuty[i], t = kneeAnchorTempF[i];
    int j = i - 1;
    while (j >= 0 && kneeAnchorRPM[j] > r) {
      kneeAnchorRPM[j + 1] = kneeAnchorRPM[j];
      kneeAnchorDuty[j + 1] = kneeAnchorDuty[j];
      kneeAnchorTempF[j + 1] = kneeAnchorTempF[j];
      j--;
    }
    kneeAnchorRPM[j + 1] = r; kneeAnchorDuty[j + 1] = d; kneeAnchorTempF[j + 1] = t;
  }

  float a, C, resid; int worstIdx;
  if (!kneeFitModel(a, C, resid, worstIdx)) return false;
  kneeFitA = a; kneeFitC = C; kneeFitResidPct = resid; kneeFitWorstIdx = worstIdx;

  // Average measured case temp across the anchors — diagnostic only (the "Learn °F" column); the live
  // correction uses the global kneeTempRefF, and the stored floors ARE normalized to kneeTempRefF.
  float tempAvg = 0.0f;
  for (int i = 0; i < kneeAnchorN; i++) tempAvg += kneeAnchorTempF[i];
  tempAvg /= (float)kneeAnchorN;

  const float maxKnee = kneeMaxFloorPct + kneeMarginPct;
  const float ceilingRPM = kneeAnchorRPM[kneeAnchorN - 1];  // highest commissioned RPM = the zero-ceiling
  float prev = 1e9f;  // running cap for monotone non-increasing knee-vs-RPM
  for (int b = 0; b < RPM_TABLE_SIZE; b++) {
    float rpmB = (float)rpmTableRPMPoints[b];
    float kneeB;
    // SAFETY: above the highest commissioned RPM force the floor to ZERO (see header). Otherwise
    // evaluate the fitted onset = a + C/RPM at this bin.
    if (rpmB > ceilingRPM) kneeB = 0.0f;
    else kneeB = a + C / fmaxf(rpmB, 1.0f);
    if (kneeB < 0) kneeB = 0;
    if (kneeB > prev) kneeB = prev;  // enforce non-increasing with RPM
    prev = kneeB;
    if (kneeB > maxKnee) kneeB = maxKnee;

    float floorB = (b == 0) ? 0.0f : fmaxf(0.0f, fminf(kneeB - kneeMarginPct, kneeMaxFloorPct));
    kneeKnee[b] = kneeB;
    kneeFloor[b] = floorB;
    kneeFrozen[b] = true;
    kneeLearnTempF[b] = tempAvg;
    rpmMinDutyTable[b] = floorB;
  }

  kneeStateDirty = true;            // persist knee blobs on the next field-off flush
  settingsDirty = true;             // push the CSV3 Min% echo so the table cells update now
  pendingSaveUserTableEdits = true; // persist rpmMinDutyTable via the same path as a manual edit
  queueConsoleMessageF("Min%% curve applied: %d anchors, fit a=%.1f%% C=%.0f, worst resid %.2f%%",
                       kneeAnchorN, a, C, resid);
  return true;
}

// Persist learned per-bin state (learning namespace blobs). Knobs persist separately via settingWrite.
void saveKneeLearnState() {
  nvs_handle_t h;
  if (nvs_open("learning", NVS_READWRITE, &h) != ESP_OK) return;
  nvs_set_blob(h, "kneeFloor", kneeFloor, sizeof(kneeFloor));
  nvs_set_blob(h, "kneeKnee", kneeKnee, sizeof(kneeKnee));
  nvs_set_blob(h, "kneeFrozen", kneeFrozen, sizeof(kneeFrozen));
  nvs_set_blob(h, "kneeFitA", &kneeFitA, sizeof(kneeFitA));  // temp-neutral threshold for the live temp correction
  nvs_commit(h);
  nvs_close(h);
  kneeStateDirty = false;
}

// ── Min%-floor snapshot for the commissioning ABORT path ──────────────────────
// The pre-commissioning tune snapshot (commissionSnapshot) covers gains/filters/thresholds but
// NOT the Min% floor table, which the Min% step rewrites. These three back up / restore / clear
// the persistent Min% state (minDutyTable + the knee tracker: floor, knee, frozen) as parallel
// "bk_*" blobs in the "learning" namespace so an abort (or a reboot mid-run) reverts Min% too.
// kneeLearnTempF is diagnostic-only and never persisted, so it is not backed up.
void commissionBackupMinPct() {
  nvs_handle_t h;
  if (nvs_open("learning", NVS_READWRITE, &h) != ESP_OK) return;
  nvs_set_blob(h, "bk_minDuty", rpmMinDutyTable, sizeof(rpmMinDutyTable));
  nvs_set_blob(h, "bk_kneeFlr", kneeFloor, sizeof(kneeFloor));
  nvs_set_blob(h, "bk_kneeKn", kneeKnee, sizeof(kneeKnee));
  nvs_set_blob(h, "bk_kneeFrz", kneeFrozen, sizeof(kneeFrozen));
  nvs_set_blob(h, "bk_kneeFitA", &kneeFitA, sizeof(kneeFitA));  // restore the live-correction threshold on abort too
  nvs_commit(h);
  nvs_close(h);
}

// Restore the Min% state from the Phase-0 backup, copy it into the live keys + RAM arrays, then
// delete the backups. Returns false (and changes nothing) if no complete backup is present.
bool commissionRestoreMinPct() {
  nvs_handle_t h;
  if (nvs_open("learning", NVS_READWRITE, &h) != ESP_OK) return false;
  float fMin[RPM_TABLE_SIZE], fFlr[RPM_TABLE_SIZE], fKn[RPM_TABLE_SIZE];
  bool  fFrz[RPM_TABLE_SIZE];
  size_t sz;
  bool ok = true;
  sz = sizeof(fMin); ok &= (nvs_get_blob(h, "bk_minDuty", fMin, &sz) == ESP_OK && sz == sizeof(fMin));
  sz = sizeof(fFlr); ok &= (nvs_get_blob(h, "bk_kneeFlr", fFlr, &sz) == ESP_OK && sz == sizeof(fFlr));
  sz = sizeof(fKn);  ok &= (nvs_get_blob(h, "bk_kneeKn",  fKn,  &sz) == ESP_OK && sz == sizeof(fKn));
  sz = sizeof(fFrz); ok &= (nvs_get_blob(h, "bk_kneeFrz", fFrz, &sz) == ESP_OK && sz == sizeof(fFrz));
  if (ok) {
    memcpy(rpmMinDutyTable, fMin, sizeof(fMin));
    memcpy(kneeFloor, fFlr, sizeof(fFlr));
    memcpy(kneeKnee, fKn, sizeof(fKn));
    memcpy(kneeFrozen, fFrz, sizeof(fFrz));
    nvs_set_blob(h, "minDutyTable", rpmMinDutyTable, sizeof(rpmMinDutyTable));
    nvs_set_blob(h, "kneeFloor", kneeFloor, sizeof(kneeFloor));
    nvs_set_blob(h, "kneeKnee", kneeKnee, sizeof(kneeKnee));
    nvs_set_blob(h, "kneeFrozen", kneeFrozen, sizeof(kneeFrozen));
    // Restore the live-correction threshold. Optional (old backups lack it) → 0 = whole-knee fallback.
    float fFitA = 0.0f; size_t szA = sizeof(fFitA);
    if (nvs_get_blob(h, "bk_kneeFitA", &fFitA, &szA) != ESP_OK || szA != sizeof(fFitA)) fFitA = 0.0f;
    kneeFitA = fFitA;
    nvs_set_blob(h, "kneeFitA", &kneeFitA, sizeof(kneeFitA));
  }
  nvs_erase_key(h, "bk_minDuty"); nvs_erase_key(h, "bk_kneeFlr");
  nvs_erase_key(h, "bk_kneeKn");  nvs_erase_key(h, "bk_kneeFrz");
  nvs_erase_key(h, "bk_kneeFitA");
  nvs_commit(h);
  nvs_close(h);
  return ok;
}

// Discard the Min% backup (commit path — the new tune stays).
void commissionClearMinPctBackup() {
  nvs_handle_t h;
  if (nvs_open("learning", NVS_READWRITE, &h) != ESP_OK) return;
  nvs_erase_key(h, "bk_minDuty"); nvs_erase_key(h, "bk_kneeFlr");
  nvs_erase_key(h, "bk_kneeKn");  nvs_erase_key(h, "bk_kneeFrz");
  nvs_erase_key(h, "bk_kneeFitA");
  nvs_commit(h);
  nvs_close(h);
}

// Throttled saver — called from loop() (NOT the control tick). Flash commit is gated on field-off
// (like dumpLongTermRing) so the NVS write never stalls the control loop while the field is on.
// Learned state accrues to RAM during field-on float/low-demand and is persisted once the field
// drops; losing an in-progress session on a power cut is harmless (slow-learned, safe defaults).
void kneeLearnService(bool fieldOff) {
  static uint32_t lastSaveMs = 0;
  if (!kneeStateDirty || !fieldOff) return;
  uint32_t now = millis();
  if (lastSaveMs != 0 && (now - lastSaveMs) < 300000UL) return;  // <= every 5 min
  lastSaveMs = now;
  saveKneeLearnState();
}

// Reset learned state: every bin back to 0% and unlocked, so learning restarts from scratch.
void kneeLearnResetDefaults() {
  for (int i = 0; i < RPM_TABLE_SIZE; i++) {
    kneeFloor[i] = 0; kneeKnee[i] = 0; kneeFrozen[i] = false; kneeLearnTempF[i] = 0; kneeLastMs[i] = 0;
    if (kneeLearnEnable) rpmMinDutyTable[i] = 0;
  }
  kneeFitA = 0.0f; kneeFitC = 0.0f;   // no commissioning fit any more → live correction reverts to whole-knee
  saveKneeLearnState();
}

// Boot init: load knobs (settings namespace, create defaults if absent) + learned state blobs.
// Call AFTER loadLearningTableFromNVS() so rpmMinDutyTable is already populated.
void kneeLearnInit() {
  if (!settingExists(NK_kneeLearnEnable)) settingWrite(NK_kneeLearnEnable, kneeLearnEnable ? "1" : "0");
  else kneeLearnEnable = (settingRead(NK_kneeLearnEnable).toInt() != 0);
  if (!settingExists(NK_kneeTempComp)) settingWrite(NK_kneeTempComp, kneeTempComp ? "1" : "0");
  else kneeTempComp = (settingRead(NK_kneeTempComp).toInt() != 0);
#define KNEE_LD_F(key, var) do { if (!settingExists(key)) settingWrite(key, String(var).c_str()); else var = settingRead(key).toFloat(); } while (0)
  // Duty-domain knobs are stored in REAL duty-% for this bus. Their hardcoded globals are 12V values,
  // so on FIRST creation scale the default by ×(12/BATTERY_VOLTAGE) (5%→1.25% at 48V) before persisting.
  // Once a key exists the stored value wins verbatim (no scaling) — that's the WYSIWYG dashboard value.
#define KNEE_LD_DUTY(key, var) do { if (!settingExists(key)) { var = var * 12.0f / (float)BATTERY_VOLTAGE; settingWrite(key, String(var).c_str()); } else var = settingRead(key).toFloat(); } while (0)
  KNEE_LD_DUTY(NK_kneeMarginPct,   kneeMarginPct);
  KNEE_LD_F(NK_kneeOnsetA,      kneeOnsetA);
  KNEE_LD_F(NK_kneeReArmA,      kneeReArmA);
  KNEE_LD_DUTY(NK_kneeStepPct,     kneeStepPct);
  KNEE_LD_F(NK_kneeDwellSec,    kneeDwellSec);
  KNEE_LD_F(NK_kneeTempRefF,    kneeTempRefF);
  KNEE_LD_DUTY(NK_kneeMaxFloorPct, kneeMaxFloorPct);
  KNEE_LD_F(NK_kneeRpmTolPct,   kneeRpmTolPct);
  KNEE_LD_F(NK_kneeTempTolF,    kneeTempTolF);
  KNEE_LD_F(NK_kneeDutyTolPct,  kneeDutyTolPct);
#undef KNEE_LD_F
#undef KNEE_LD_DUTY

  nvs_handle_t h;
  bool haveState = false;
  if (nvs_open("learning", NVS_READONLY, &h) == ESP_OK) {
    size_t sz = sizeof(kneeFloor);
    bool okF = (nvs_get_blob(h, "kneeFloor", kneeFloor, &sz) == ESP_OK && sz == sizeof(kneeFloor));
    sz = sizeof(kneeKnee);
    bool okK = (nvs_get_blob(h, "kneeKnee", kneeKnee, &sz) == ESP_OK && sz == sizeof(kneeKnee));
    sz = sizeof(kneeFrozen);
    bool okZ = (nvs_get_blob(h, "kneeFrozen", kneeFrozen, &sz) == ESP_OK && sz == sizeof(kneeFrozen));
    haveState = okF && okK && okZ;
    // Temp-neutral threshold for getMinimumFieldForRPM's live correction. Optional (absent on tables
    // built before this existed, or never commissioned) → fall back to 0 = scale the whole knee.
    sz = sizeof(kneeFitA);
    if (nvs_get_blob(h, "kneeFitA", &kneeFitA, &sz) != ESP_OK || sz != sizeof(kneeFitA)) kneeFitA = 0.0f;
    nvs_close(h);
  }
  if (!haveState)
    for (int i = 0; i < RPM_TABLE_SIZE; i++) { kneeFloor[i] = 0; kneeKnee[i] = 0; kneeFrozen[i] = false; kneeLearnTempF[i] = 0; }

  // If learning is enabled, the floor owns the table from boot (bin 0 always 0%).
  if (kneeLearnEnable)
    for (int i = 0; i < RPM_TABLE_SIZE; i++) {
      float f = (i == 0) ? 0.0f : kneeFloor[i];
      if (f < 0) f = 0; if (f > kneeMaxFloorPct) f = kneeMaxFloorPct;
      rpmMinDutyTable[i] = f;
    }
}

// Build the /kneeLearnState JSON (knobs + live status + per-bin learned state).
String kneeLearnStateJson() {
  String j = "{";
  j += "\"enable\":";        j += (kneeLearnEnable ? 1 : 0);
  j += ",\"marginPct\":";    j += String(kneeMarginPct, 2);
  j += ",\"onsetA\":";       j += String(kneeOnsetA, 2);
  j += ",\"reArmA\":";       j += String(kneeReArmA, 2);
  j += ",\"stepPct\":";      j += String(kneeStepPct, 2);
  j += ",\"dwellSec\":";     j += String(kneeDwellSec, 1);
  j += ",\"tempRefF\":";     j += String(kneeTempRefF, 1);
  j += ",\"tempComp\":";     j += (kneeTempComp ? 1 : 0);
  j += ",\"maxFloorPct\":";  j += String(kneeMaxFloorPct, 2);
  j += ",\"rpmTolPct\":";    j += String(kneeRpmTolPct, 1);
  j += ",\"tempTolF\":";     j += String(kneeTempTolF, 1);
  j += ",\"dutyTolPct\":";   j += String(kneeDutyTolPct, 2);
  j += ",\"activeBin\":";    j += String(kneeActiveBin);
  j += ",\"floorActive\":";  j += String(kneeFloorActive ? 1 : 0);
  j += ",\"steady\":";       j += String(kneeSteadyNow ? 1 : 0);
  j += ",\"bins\":[";
  uint32_t now = millis();
  for (int i = 0; i < RPM_TABLE_SIZE; i++) {
    if (i) j += ",";
    long ageS = (kneeLastMs[i] == 0) ? -1 : (long)((now - kneeLastMs[i]) / 1000UL);
    j += "{\"rpm\":";    j += String(rpmTableRPMPoints[i]);
    j += ",\"floor\":";  j += String(rpmMinDutyTable[i], 2);
    j += ",\"knee\":";   j += String(kneeKnee[i], 2);
    j += ",\"frozen\":"; j += (kneeFrozen[i] ? 1 : 0);
    j += ",\"learnT\":"; j += String(kneeLearnTempF[i], 0);
    j += ",\"ageS\":";   j += String(ageS);
    j += "}";
  }
  j += "]}";
  return j;
}

/**
 * isVoltageSensorPlausible()
 * Voltage ranges (with safety buffer):
 * - 12V system: 4.5V to 15.5V
 * - 24V system: 9.0V to 32.5V   (upper allows a 24V AGM equalization ~31-32V)
 * - 48V system: 18V to 60.5V
 *
 * @return true if at least one voltage sensor shows plausible reading
 */
bool isVoltageSensorPlausible() {
  float minPlausible, maxPlausible;

  // Use the user-entered nominal bank class (BATTERY_VOLTAGE) directly — not an autodetect from
  // BulkVoltage, which can mis-bucket and "F up" near class boundaries.
  if (BATTERY_VOLTAGE >= 48) {
    // 48V system (normal bulk = 55.2-57.6V)
    minPlausible = 18.0f;  // Dead battery - 0.5V buffer
    maxPlausible = 60.5f;  // Max charging + 0.5V buffer
  } else if (BATTERY_VOLTAGE >= 24) {
    // 24V system (normal bulk = 27.6-28.8V)
    minPlausible = 9.0f;   // Dead battery - 0.5V buffer
    maxPlausible = 32.5f;  // Max charging/equalization + buffer
  } else {
    // 12V system (normal bulk = 13.8-14.4V)
    minPlausible = 4.5f;   // Dead battery - 0.5V buffer
    maxPlausible = 15.5f;  // Max charging + 0.5V buffer
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
  // Velocity form (2026-06-25): no PID-library object. Start with zero penalty — the loop
  // accumulates penalty (prevThermalPenalty) only if temperature demands it. eP_prev starts
  // at 0 so the first dP is well-defined. tempPID_tick() does the real bumpless seed when it
  // first activates on a valid temp reading.
  thermalPenaltyAmps = 0.0f;
  prevThermalPenalty = 0.0f;
  eP_prev = 0.0f;
  tempPIDActive = false;

  Serial.printf("TempPID: Init | Kp=%.2f Ki=%.3f Lookahead=%.1fs Interval=%lums\n",
                TempPIDKp, TempPIDKi, ThermalLookaheadSec, (unsigned long)TempPIDIntervalMs);
}
// ---------------------------------------------------------------------------
//  tempFilterUpdate — IIR filter + slope estimator + projected-temp lookahead.
//  Pure display/estimator math: updates tempFiltered, the slope buffer,
//  thermalSlopeFPerSec, and projectedTempF. NO PID, NO field, NO persisted state.
//  Split out of tempPID_tick (2026-06-23) and called every AUTO tick — including
//  TuningMode and the commissioning sub-steps that toggle it — so the dashboard
//  plot and thermal log show the REAL temperature during tuning instead of a value
//  frozen at whatever it was when the tuning step began. Skips on invalid temp so a
//  NaN / out-of-range sensor read never poisons the filter (same guard tempPID_tick
//  already applied before it ran the filter inline).
// ---------------------------------------------------------------------------
void tempFilterUpdate(uint32_t nowMs) {
  bool tempValueSane = !isnan(TempToUse) && (TempToUse > -50.0f) && (TempToUse < 400.0f);
  if (!tempValueSane) return;  // hold last filtered value; do not poison with garbage

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
        // Reject as sensor noise; hold the previous slope so a real fast rise still has
        // predictive signal while one outlier sample rolls through the 60s window.
        // Old behavior was to clamp to ±0.5 — that injected up to ±15 °F false lookahead.
        static uint32_t slopeClampLastLogMs = 0;
        if ((uint32_t)(nowMs - slopeClampLastLogMs) >= 60000) {
          slopeClampLastLogMs = nowMs;
          queueConsoleMessageF("TempPID: raw slope %.3f °F/s rejected as sensor noise — holding previous", rawSlope);
        }
        rawSlope = thermalSlopeFPerSec;  // hold previous good value
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
}

void tempPID_tick(uint32_t nowMs, float actualDtSec) {

  // --- Hard reset path (requested externally, e.g. from web UI command) ---
  if (tempPIDResetRequested) {
    tempPIDResetRequested = false;
    thermalPenaltyAmps = 0.0f;
    prevThermalPenalty = 0.0f;          // velocity-form accumulator (the loop's only memory) — zero it
    eP_prev = 0.0f;                     // first post-reset dP starts from a clean zero
    tempPIDActive = false;
    tempFilterNeedsReseed = true;
    memset(thermalSlopeBuffer, 0, sizeof(thermalSlopeBuffer));
    thermalSlopeBufIdx = 0;
    thermalSlopeBufFull = false;
    thermalSlopeLastPushMs = 0;
    thermalSlopeFPerSec = 0.0f;
    projectedTempF = NAN;
    thermalIntegratorReleased = false;  // fresh approach — dI held off until present temp reaches setpoint again
    tempInvalidSinceMs = 0;             // clear the invalid-temp debounce window
    queueConsoleMessage("ThermalPID: manual reset requested - penalty accumulator and filter cleared");
    return;
  }

  // Stage-aware penalty bounds — computed ONCE, referenced everywhere below.
  // Derived from inBulkStage / inAbsorptionStage directly, NOT voltageControlActive:
  // the assignment `voltageControlActive = (!inBulkStage || inAbsorptionStage)` runs
  // AFTER tempPID_tick() returns, so voltageControlActive still carries the previous
  // stage's value on a transition tick. Stage variables are written before this fn.
  const float capCurrent = getCapCurrentForRPM(RPM);
  const float penaltyMax = (float)MaxTableValue;

  const bool inPureBulk = (inBulkStage && !inAbsorptionStage);
  // Thermal penalty is derate-only: PID output range is [0, MaxTableValue].
  // It can only SUBTRACT from the RPM-table ceiling (I_cmd = I_cap - thermalPenaltyAmps,
  // see command chain in AdjustFieldLearnMode), never add to it. So a cold alternator
  // runs at penalty 0 = the full RPM-table current with no derate; the loop cannot push
  // current above the table cap. There is no separate battery-temperature voltage comp.
  const float penaltyMin = 0.0f;

  // ---------------------------------------------------------------------------
  //  Temperature sanity guard — only stops PID on invalid value (NaN / out of range).
  //  Stale data (no new reading) is handled at 20s by tempDataVeryStale → field cut;
  //  no intermediate hold needed here.
  // ---------------------------------------------------------------------------
  bool tempValueSane = !isnan(TempToUse) && (TempToUse > -50.0f) && (TempToUse < 400.0f);

  if (!tempValueSane) {
    // Debounce (2026-06-23): a SINGLE bad sample used to deactivate the loop immediately, and
    // the resume then cleared the slope buffer → 65s warmup → setpoint dropped to limit−20 and
    // the approach gate re-armed (the spurious "140°F setpoint" artifact seen in therm.csv).
    // Ride through BRIEF glitches instead: hold the last penalty but stay ACTIVE with the slope
    // buffer intact for up to THERMAL_INVALID_DEBOUNCE_MS. Only a SUSTAINED dropout deactivates,
    // so the eventual resume still clears a genuinely-stale buffer. Safety unchanged: the penalty
    // is held (never reduced) here, the separate over-temp protections still fire on real
    // temperature, and tempDataVeryStale still cuts the field at 20s — far beyond this 3s window.
    const uint32_t THERMAL_INVALID_DEBOUNCE_MS = 3000;
    if (tempInvalidSinceMs == 0) tempInvalidSinceMs = nowMs;
    // Any invalid sample voids a pending preserve-on-resume intent (conservative) — e.g. a
    // tuning exit that coincided with a dropout must re-enable as a real stale gap.
    thermalPreserveSlopeOnResume = false;
    if (tempPIDActive && (uint32_t)(nowMs - tempInvalidSinceMs) >= THERMAL_INVALID_DEBOUNCE_MS) {
      tempPIDActive = false;
      queueConsoleMessageF("TempPID: temp value invalid >%lums, holding penalty at %.1fA",
                           (unsigned long)THERMAL_INVALID_DEBOUNCE_MS, thermalPenaltyLastValid);
    }
    return;  // Hold last valid penalty — do not touch thermalPenaltyAmps.
  }
  tempInvalidSinceMs = 0;  // valid sample — reset the debounce window

  // tempFiltered, the slope buffer / thermalSlopeFPerSec, and projectedTempF are updated
  // by tempFilterUpdate() — called unconditionally at the TOP of AdjustFieldLearnMode every
  // tick in every mode (hoisted 2026-06-26) BEFORE this function runs — so they are already
  // fresh here. Split out 2026-06-23 so the plot/thermal log keep showing real temperature
  // during tuning/commissioning, then hoisted again so they also stay live during FAULT-mode
  // over-temp cooldown instead of freezing. See tempFilterUpdate() above.

  // Re-enable after stale period — bumpless transfer. All three penalty state
  // vars are seeded to resumePenalty (clamped to current stage bounds) so the
  // command chain, slew limiter, and stale-hold path agree on the first tick:
  //   thermalPenaltyAmps      — read by command chain this tick
  //   prevThermalPenalty      — the velocity-form accumulator / slew "prev" on the next tick
  //   thermalPenaltyLastValid — returned by stale-hold path if temp goes stale
  //   eP_prev                 — seeded to current eP so the first post-resume dP is ~0
  const float activeTempLimit = TemperatureLimitF;

  // Suppress the 60s warmup margin (−20°F) during commissioning and any tuning. Those flows
  // toggle the field / TuningMode repeatedly, re-seeding the slope buffer over and over, so the
  // margin would keep stepping the setpoint 155<->140 and derate current mid-test (corrupting
  // sysID/stabilization/CV/thermal-tuning measurements). The hard trips (limit+TempWarnExcess
  // warning ramp, limit+TempCritExcess critical) are NOT gated by this, so no damage protection
  // is lost — only the soft "react-early-while-blind" margin, and only while supervised. Normal
  // operation and post-trip resume keep the full −20°F margin. See Thermal_Loop_Dev_Summary.md.
  const bool suppressWarmupMargin = (commissionState == 1) || (TuningMode != 0);

  if (!tempPIDActive) {

    // Preserve the slope buffer when re-enabling from a DORMANT-but-not-stale period (TuningMode
    // exit): tempFilterUpdate() kept it live the whole time, so the trend is good and clearing it
    // would force a 60s blind warmup (setpoint → limit-20) that derates the field right after a
    // test. The one-shot flag is the gate, NOT a freshness timestamp: on a true sensor gap,
    // tempFilterUpdate() pushes a fresh sample spanning the gap the same tick we re-enable, so a
    // time-since-last-push test would look "fresh" while the buffer holds garbage — only the
    // caller knows the data was actually continuous. Default path (stale gap, cold start) clears.
    const bool preserveSlope = thermalPreserveSlopeOnResume;
    thermalPreserveSlopeOnResume = false;  // consume the one-shot

    if (!preserveSlope) {
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
    }
    // When preserved, the slope buffer, thermalSlopeFPerSec, and projectedTempF are left exactly
    // as tempFilterUpdate() computed them this tick — live trend, no warmup blindness.

    // Warmup setpoint: keyed off thermalSlopeBufFull so it tracks the clear above. If the buffer
    // was preserved (still full) we are NOT blind → normal −5°F. Only a genuinely cleared buffer
    // (and no active suppression) takes the −20°F blind-warmup margin. suppressWarmupMargin
    // (commissioning + tuning) also forces −5 so a re-seed mid-test seeds no spurious penalty.
    const float reEnableSetpoint = activeTempLimit -
        ((suppressWarmupMargin || thermalSlopeBufFull) ? 5.0f : 20.0f);

    tempPIDInput_d = (double)projectedTempF;

    // Bumpless resume into velocity form: seed the accumulator from P-only at the
    // current temperature, not the stale held value. thermalPenaltyLastValid reflects
    // conditions when the sensor went stale — if temp was high then but has since
    // recovered, seeding at the old value overpunishes and takes minutes to bleed off.
    // Seed eP_prev to the current eP so the first post-resume dP is ~0 (no spike).
    float stalePenalty = thermalPenaltyLastValid;
    float e_resume = projectedTempF - reEnableSetpoint;
    float resumePenalty = clamp_f(e_resume > 0.0f ? TempPIDKp * e_resume : 0.0f,
                                  penaltyMin, penaltyMax);

    thermalPenaltyAmps = resumePenalty;
    prevThermalPenalty = resumePenalty;
    thermalPenaltyLastValid = resumePenalty;
    eP_prev = fmaxf(0.0f, fmaxf(projectedTempF, (TempSource == 0) ? TempToUse : tempFiltered) - reEnableSetpoint);

    // Re-init the approach gate from present state: resume hot = released, resume
    // cool = treat as a fresh approach. A positive seed can still unwind via dP/dI —
    // the gate only holds OFF the up-driving dI (see velocity update below).
    thermalIntegratorReleased = (projectedTempF >= activeTempLimit - 5.0f);

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
  //  suppressWarmupMargin (commissioning + tuning, computed above) forces the
  //  margin to the normal −5°F so the live setpoint holds steady instead of
  //  stepping 155<->140 and derating mid-test. Hard trips are unaffected.
  // ---------------------------------------------------------------------------
  const float warmupMargin = suppressWarmupMargin ? 5.0f : 20.0f;
  const float effectiveSetpoint = thermalSlopeBufFull ? (activeTempLimit - 5.0f) : (activeTempLimit - warmupMargin);
  tempPIDSetpoint_d = (double)effectiveSetpoint;
  // VELOCITY FORM (2026-06-25): there is no separate PID-library integrator anymore. The
  // penalty is updated incrementally each tick (penalty += dP + dI) and the *clamped penalty
  // itself* is the only accumulator, so windup is impossible by construction. The two error
  // signals feeding it:
  //   eP = max(projectedTempF, present) − setpoint   — projection-led SAFETY term (drives dP).
  //                                                     Keeps the cold-approach cut + hot-side
  //                                                     aggression. Used floored at 0 so a cold
  //                                                     alternator (eP<0) contributes no penalty.
  //   eI = present − setpoint                          — PRESENT temp only, the holding-level
  //                                                     driver (drives dI). Never sees the
  //                                                     projection (that was the case-4 windup).
  // lookaheadDeltaF and tempNowPid below are kept for LOGGING decomposition (outerTermLookahead/
  // outerTermP) and for tempPIDInput_d (CSV2 telemetry); they no longer feed a library object.
  float lookaheadDeltaF;
  float tempNowPid;
  {
    tempNowPid = (TempSource == 0) ? TempToUse : tempFiltered;
    // Projection's amps ABOVE max(present, setpoint) — the share of eP attributable to the
    // lookahead lead. outerTermP + outerTermLookahead = Kp·max(0, max(proj,present)−setpoint),
    // so the logged decomposition still sums to the P-driven part of the penalty.
    lookaheadDeltaF = fmaxf(0.0f, projectedTempF - fmaxf(tempNowPid, effectiveSetpoint));
    tempPIDInput_d = (double)tempNowPid;
  }

  // One-shot approach gate: released the first time PRESENT temp (not the projection)
  // reaches the regulation setpoint. Until then dI is held off — P + the projection (dP)
  // handle the approach cut on their own. Winding a holding level before there is one to
  // hold is what bought the ~2.5× overbuild + deep post-peak sag (measured 2026-06-11).
  if (!thermalIntegratorReleased && tempNowPid >= (activeTempLimit - 5.0f)) {
    thermalIntegratorReleased = true;
    queueConsoleMessageF("TempPID: integrator released — present temp %.1f°F reached setpoint", tempNowPid);
  }

  // ---------------------------------------------------------------------------
  //  VELOCITY-FORM PENALTY UPDATE (2026-06-25 refactor — see Thermal_Loop_Dev_Summary.md
  //  "VELOCITY-FORM REFACTOR SPEC"). Replaces the positional PID library + the freeze-gate
  //  cluster (old cases 1/2/4/5/5b + the thermalHoldEstimate EMA/leak estimator). The penalty
  //  is updated INCREMENTALLY (penalty += dP + dI) and prevThermalPenalty — always the clamped
  //  output — is the SOLE loop memory, so windup is impossible by construction: the penalty
  //  cannot wind past the live cap nor below 0, and there is no separate integrator state to
  //  seed wrong (the 2026-06-25 false-zero-seed trip), starve, overbuild, self-lock, or leak.
  //  Only ONE gate survives — case 3 (descent): suppress the up-driving dI while above setpoint
  //  AND cooling, else dI overbuilds the holding level into an already-falling alternator and
  //  buys a post-peak undershoot (the −5°F sag, 2026-06-14, routine on every cycle per BigTeharm).
  // ---------------------------------------------------------------------------
  const float ePpos = fmaxf(0.0f, fmaxf(projectedTempF, tempNowPid) - effectiveSetpoint);  // floored eP (projection-led) — drives dP
  const float eI    = tempNowPid - effectiveSetpoint;                                       // present-only — drives dI

  // Integral-accumulation gate (the only surviving freeze): approach holds dI off until present
  // temp first reaches setpoint (was case 1); descent holds dI off while above setpoint AND
  // cooling (case 3 — kept). dP is never gated — P unwinds on the way down regardless.
  const bool descentHold = (eI > 0.0f) && (thermalSlopeFPerSec < 0.0f);
  const bool applyDI     = thermalIntegratorReleased && !descentHold;

  const float dP = TempPIDKp * (ePpos - eP_prev);
  // Asymmetric bleed (2026-06-26): below setpoint (eI<0) the integral releases at TempPIDKi×
  // TempPIDKiDownFrac, NOT full Ki. A deep transient undershoot therefore bleeds the holding
  // penalty only slowly instead of collapsing it (~80A→~12A in fridaytherm.csv), so the next
  // heat-soak climb starts near the real hold and does not run away. A SUSTAINED cold alternator
  // still releases derate (the bleed is slowed, not frozen). Above setpoint (eI≥0) full Ki — no
  // change to build speed or to the windup-proofing. Frac=1.0 reproduces the symmetric old law.
  const float kiEff = (eI < 0.0f) ? (TempPIDKi * TempPIDKiDownFrac) : TempPIDKi;
  const float dI = applyDI ? (kiEff * eI * actualDtSec) : 0.0f;
  const float penaltyRaw = prevThermalPenalty + dP + dI;  // pre-clamp/slew → logged as outerPenaltyRaw

  // Anti-windup BY CONSTRUCTION: clamp to the LIVE rpm cap (not MaxTableValue). If penaltyRaw
  // exceeds capCurrent the next tick starts from the cap — it cannot wind past; if it floors at
  // 0 it holds at 0. Replaces the old case-2 saturation freeze (untriggered/grazing in 6 runs).
  const bool satClamp = (penaltyRaw > capCurrent);
  const float penaltyClamped = clamp_f(penaltyRaw, 0.0f, capCurrent);

  // Existing asymmetric slew, then final clamp to the SAME live cap.
  thermalPenaltyAmps = slew_limit_f(prevThermalPenalty, penaltyClamped,
                                    ThermalPenaltyRiseRate, ThermalPenaltyFallRate, actualDtSec);
  thermalPenaltyAmps = clamp_f(thermalPenaltyAmps, 0.0f, capCurrent);

  // Commit memory — prevThermalPenalty IS the accumulator.
  prevThermalPenalty      = thermalPenaltyAmps;
  thermalPenaltyLastValid = thermalPenaltyAmps;
  eP_prev                 = ePpos;

  // freezeWhy re-enumed (2026-06-25): 0 = dI applied (winding normally); 1 = approach (dI held,
  // present below setpoint); 2 = saturation (penaltyRaw clamped to the live cap this tick);
  // 3 = descent (above setpoint and cooling, dI held). Values 4/5 retired. Priority: approach
  // > saturation > descent.
  if (!thermalIntegratorReleased)  thermalFreezeReason = 1;
  else if (satClamp)               thermalFreezeReason = 2;
  else if (descentHold)            thermalFreezeReason = 3;
  else                             thermalFreezeReason = 0;
  {
    static bool satClampLogged = false;  // one-shot per saturation episode
    if (satClamp && thermalIntegratorReleased) {
      if (!satClampLogged) {
        satClampLogged = true;
        queueConsoleMessageF("TempPID: penalty clamped to live rpm cap %.1fA (requested %.1fA)", capCurrent, penaltyRaw);
      }
    } else {
      satClampLogged = false;
    }
  }

  // ---------------------------------------------------------------------------
  //  Logging decomposition (preserve every thermal-log column's meaning — see the
  //  "Thermal log decoding" table in Thermal_Loop_Dev_Summary.md). Diagnostic only; the
  //  accumulator above is the actual control state.
  // ---------------------------------------------------------------------------
  outerTermP         = TempPIDKp * fmaxf(0.0f, tempNowPid - effectiveSetpoint);  // present-error P share — invert to recover setpoint when hot
  outerTermLookahead = TempPIDKp * lookaheadDeltaF;                              // projection share (logging-only now)
  // Persistent holding share the accumulator carries beyond instantaneous P + lookahead —
  // preserves the doc-wide "outerI = holding level" reading (builds toward ~83A on this load).
  // May read slightly negative on a projection spike; left raw (diagnostic).
  outerTermI         = prevThermalPenalty - outerTermP - outerTermLookahead;
  outerPenaltyRaw    = penaltyRaw;                                              // pre-clamp/slew, vs rpmCap = requested-vs-applied signal

  // CV-bleed (the old back-calculation anti-windup) is deleted with the library object: the
  // penalty is derate-only and the accumulator is clamped to [0, capCurrent] every tick, so
  // there is no negative integrator bias to bleed. The thermal-log antiWindupFired column
  // therefore always reads 0 — already its real meaning (zero fires in every log to date).
  outerAntiWindupFired = false;

  // outerImpliedPenalty: voltage cap expressed as a downstream penalty equivalent, for log
  // debugging only. Does not drive any control action. Keyed off inPureBulk (stage-derived)
  // for the same timing reason as penaltyMin — voltageControlActive may be one tick stale.
  if (!inPureBulk && Icv > 1.0f) {
    outerImpliedPenalty = fmaxf(0.0f, capCurrent - Icv);
  } else {
    outerImpliedPenalty = 0.0f;
  }

  // Wave generator + scoring + always-on live score
  thermalAccuracyScore_tick(nowMs, actualDtSec);
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
  if (!loggingActive) return;  // Stop Logs: skip append, freeze buffer
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
  e.ovFlags = 0;
  if (g_fastOvClampActive) e.ovFlags |= (1 << 0);
  if (g_iExcessBulkActive) e.ovFlags |= (1 << 1);  // iExcess BULK sub-mode (current-control phase)
  if (g_fastOvHardActive) e.ovFlags |= (1 << 2);
  if (g_iExcessActive) e.ovFlags |= (1 << 3);
  if (g_loadDumpActive) e.ovFlags |= (1 << 4);

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
  e.innerKp = (float)PidKp;  // inner output-current PID
  e.innerKi = (float)PidKi;
  e.innerKd = (float)PidKd;
  e.voltageKp = (float)VoltageKp_active;  // outer voltage loop — gain actually in effect (Manual or Auto-λ)
  e.voltageKi = (float)VoltageKi_active;
  e.voltageKd = 0.0f;  // D term removed; field kept for struct layout compatibility

  e.battV_filt = IBV_filtered;
  e.iMeas_filt = MeasuredAmps_filtered;
  e.dBcur_dt = g_dBcur_dt;
  e.battI = getBatteryCurrent();
  e.ch1IntervalMs = (int16_t)g_ch1LastIntervalMs;
  e.voltLoopIntervalMs = pidLog_voltageLoopRanThisTick ? (int16_t)g_voltLoopActualIntervalMs : 0;
  e.inaIntervalMs = (int16_t)ina_last_ms;
  e.pad2 = 0;
  e.mExcessEma = g_mExcessEma;             // iExcess detector traces — averaged excess vs its
  e.iExcessThreshold = g_iExcessThreshold; // computed fire threshold E (both A), for offline tuning

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
  if (!loggingActive) return;  // Stop Logs: skip append, freeze buffer
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
  e.tempFiltered = thermalLogScale10(tempFiltered);
  e.tempProjected = thermalLogScale10(projectedTempF);
  // effective setpoint: mirrors the logic in tempPID_tick (slopeBufFull = 5°F margin, else 20°F warmup margin)
  {
    float logLimit = TemperatureLimitF;
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
  e.freezeWhy = thermalFreezeReason;

  e.outerTermP = thermalLogScale10(outerTermP);
  e.outerTermI = thermalLogScale10(outerTermI);
  e.outerTermLookahead = thermalLogScale10(outerTermLookahead);
  e.impliedPenalty = thermalLogScale10(outerImpliedPenalty);
  e.thermalSlope = (int16_t)(thermalSlopeFPerSec * 1000.0f);
  e.penaltyRaw = thermalLogScale10(outerPenaltyRaw);
  e.holdEstimate = (int16_t)-10;  // DEAD since the 2026-06-25 velocity-form refactor (no holding estimator); always the -1.0 sentinel to keep the log layout / sizeof / analysis scripts stable

  thermalLogHead = (thermalLogHead + 1) % THERMAL_LOG_SIZE;
  if (thermalLogCount < THERMAL_LOG_SIZE) thermalLogCount++;
}
