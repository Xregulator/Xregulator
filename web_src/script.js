/* XREG_START */
// Prevent browser from restoring previous scroll position on reload/reflash
if ('scrollRestoration' in history) { history.scrollRestoration = 'manual'; }


/*  * AI_SUMMARY: Alternator Regulator Project - This file is the majority of the JS served by the ESP32
This JS file is structured as a streaming data pipeline, not a collection of independent UI handlers. CSV messages arrive 
(CSVData1/2/3), are parsed once into a central `data` object that holds the full live system state, and everything downstream 
reads from that single source of truth. The UI never pulls directly from the raw CSV. Instead, display is controlled by small 
“whitelist” arrays (like `otherFields`) that select which subset of values should be rendered. `updateFields()` is the core 
rendering engine: it loops over the whitelist, pulls the current value from `data`, applies scaling/formatting rules, and updates 
the DOM. Additional helpers (like GPS display logic, plots, min/max, etc.) hang off this same flow and should be triggered at the 
point where CSV values are parsed or immediately after `updateFields()` runs, since that is the single synchronization point where 
fresh data is guaranteed.  Each CSV payload is self-describing: the ESP32 prepends a field count that is validated against the matching JS CSV*_FIELDS 
schema array on arrival, so length mismatches between firmware and UI are caught immediately at parse time rather than 
silently producing garbage values.
 * AI_PURPOSE: Realtime control of GPIO (settings always have echos) and viewing of sensor data and calculated values.  
 * AI_INPUTS: Payloads from the ESP32, including some variables used in this javascript such as webgaugesinterval (the ESP32 data delivery interval),timeAxisModeChanging (toggles a different style of X axis on plots generated here), plotTimeWindow (length of time to plot on X axis).  Server-Sent Events via /events endpoint
 * AI_OUTPUTS: Values submitted back to ESP32 via HTTP GET/POST requests to various endpoints
 * AI_DEPENDENCIES: 
* AI_RISKS: Variable naming is inconsistent, need to be careful not to assume consistent patterns. Unit conversion / scaling 
can be confusing and propagate to many places, have to trace dependencies in variables FULLY to every end point. When adding 
a new field to any CSV payload, it must be added in three places in sync: the ESP32 enum, the ESP32 snprintf args, and the 
matching JS CSV*_FIELDS array — the runtime schema mismatch warning will fire if these get out of step.
 * AI_OPTIMIZE: When adding new code, try to first use or modify existing code whenever possible, to avoid bloat.  When impossible, always mimik my style and coding patterns.
 * CRITICAL_INSTRUCTION_FOR_AI:: When adding new code, try to first use or modify existing code whenever possible, to avoid bloat. When impossible, always mimick my style and coding patterns. If you have a performance improvement idea, tell me. When giving me new code, I prefer complete copy and paste functions when they are short, or for you to give step by step instructions for me to edit if function is long, to conserve tokens. Always specify which option you chose.  Never re-write the entire file, this just wastes my tokens.
 */

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


// ============================================================
// STALENESS DISPLAY THRESHOLDS
// These control when sensor readings gray out in the UI only.
// They have zero effect on the regulator or field control logic —
// that staleness is governed by the hardcoded 20s temp-stale threshold
// and the hardcoded 30s in buildTickSnapshot() on the ESP32.
//
// The timestamp payload sends every 3s, so any threshold below
// ~6s will cause false stale flashes even with healthy sensors.
// ============================================================
const STALE_THRESHOLD_DEFAULT_MS = 6000;   // All sensors except temperature
const STALE_THRESHOLD_TEMP_MS = 12000;  // Temp sensors read every 5s — allows one failed read

// Numerical-gauge render throttle. CSV1 LiveStream arrives every webgaugesinterval (~100ms)
// for smooth plots, but rendering noisy gauges that fast makes the last digit unreadable.
// Plots, setting echoes, alignment indicators, and field-status banner are NOT affected —
// only the criticalFields DOM writes (battery V/A, alt amps, RPM, dutyCycle, PID, IMU live).
const GAUGE_RENDER_INTERVAL_MS = 500;


// PID Tuning Plot 
let yyMin = -5;    // Default until CSVData3 updates it
let yyMax = 105;
let xTime = 60;
let cachedYyMin = null;
let cachedYyMax = null;
let cachedXTime = null;

// Temperature unit preference — 0=°F (default), 1=°C. Echoed from CSV3.
let displayTempUnit = 0;

function toDisplayTemp(val_f) {
    return displayTempUnit === 1 ? (val_f - 32) * 5 / 9 : val_f;
}
function toDisplayTempDelta(delta_f) {
    return displayTempUnit === 1 ? delta_f * 5 / 9 : delta_f;
}
function tempUnitLabel() {
    return displayTempUnit === 1 ? '°C' : '°F';
}
function updateAllTempUnitLabels() {
    const lbl = tempUnitLabel();
    document.querySelectorAll('.temp-unit-label').forEach(el => { el.textContent = lbl; });
    document.querySelectorAll('.temp-rate-label').forEach(el => { el.textContent = lbl + '/s'; });
    // Update toggle button states
    const fBtn = document.getElementById('tempUnitF_btn');
    const cBtn = document.getElementById('tempUnitC_btn');
    if (fBtn && cBtn) {
        if (displayTempUnit === 1) {
            fBtn.className = 'btn-secondary';
            cBtn.className = 'btn-primary';
        } else {
            fBtn.className = 'btn-primary';
            cBtn.className = 'btn-secondary';
        }
    }
}

function setTempUnit(unit) {
    displayTempUnit = unit;
    updateAllTempUnitLabels();
    // Persist to device via GET
    const pw = document.querySelector('.password_field');
    const pwVal = pw ? pw.value : '';
    const url = `/get?displayTempUnit=${unit}&password=${encodeURIComponent(pwVal)}`;
    fetch(url).catch(() => {});
}

// Convert a form input value from display unit back to °F before GET submission.
// Temporarily rewrites the input value, lets the form submit, then restores the display value.
function convertTempFormIfNeeded(form, inputName, isDelta) {
    if (displayTempUnit !== 1) return true;
    const input = form.querySelector(`[name="${inputName}"]`);
    if (!input) return true;
    const displayVal = parseFloat(input.value);
    if (isNaN(displayVal)) return true;
    const nativeVal = isDelta
        ? (displayVal * 9 / 5)           // delta: °C span → °F span
        : (displayVal * 9 / 5 + 32);     // absolute: °C → °F
    input.value = Math.round(nativeVal * 10) / 10;  // round to 1 decimal
    const originalDisplay = String(displayVal);
    setTimeout(() => { input.value = originalDisplay; }, 500);
    return true;
}

//  UPDATE LATER
// Tracks if device is registered for Cloud Features. Hydrated from sessionStorage so a
// page reload mid-session doesn't drop the flag and re-prompt for registration. Cleared
// when the app/tab fully closes → a fresh session re-validates against the cloud.
let isDeviceRegistered = (() => { try { return sessionStorage.getItem('deviceRegistered') === '1'; } catch (e) { return false; } })();
//let isDeviceRegistered = true; // TEMP: bypass for local testing

let source; // Declare at broader scope

let vesselInfoComplete = false; // Tracks if vessel info is filled out

window.vesselInfo = null; // Cache for vessel data

// Demo mode for App Store testing (no ESP32 required)
let DEMO_MODE = false;
let demoInterval = null;

//prevent EventSource Infinite Reconnection
let sseReconnectAttempts = 0;
const MAX_SSE_RECONNECTS = 10;
let sseReconnectTimer = null;
let isAppInBackground = false;
let isOfflineMode = false; // True after user clicks "Continue Offline"; cleared on next successful SSE open

let activeTimers = []; // Track all active timers

let g_lastCsv3 = null; // Last received CSV3 data object — used by cvBinToCsv for header constants

const CSV1_FIELDS = [
    "AlternatorTemperatureF",
    "dutyCycle",
    "BatteryV",
    "MeasuredAmps",
    "RPM",
    "Channel3V",
    "IBV",
    "Bcur",
    "VictronVoltage",
    "LoopTime",
    "WifiHeartBeat",
    "vvout",
    "iiout",
    "FreeHeap",
    "Alarm_Status",
    "fieldActiveStatus",
    "currentMode",
    "stateRevision",
    "setpointLimited",
    "uTargetAmps",
    "pidInput",
    "pidOutput",
    "pidError",
    "imu_heel_deg",
    "imu_pitch_deg",
    "imu_vertical_accel_g",
    "imu_yaw_rate_dps",
    "imu_total_accel_g",
    "perfCountersResetElapsedS",  // seconds since "Reset Peak Values" press (0 = boot)
    "shutdownPhase",
    "BatteryV_raw",
    "MeasuredAmps_filtered",
    "voltageTarget",
    "Icv",
    "WaterDepth_ft",
];

// Format elapsed seconds since "Reset Peak Values" press into a short window descriptor.
// Used to populate every .session-window-label span on the diagnostics page so labels
// like "Worst — last 12 min (ms)" / "Worst — last 1.4 hr (ms)" stay in sync with the
// firmware's perfCountersResetMs timestamp.
function formatSessionWindow(elapsedS) {
    const s = Math.max(0, Math.floor(Number(elapsedS) || 0));
    if (s < 3600) {
        return `Worst — last ${Math.max(0, Math.floor(s / 60))} min`;
    }
    return `Worst — last ${(s / 3600).toFixed(1)} hr`;
}

const CSV2_FIELDS = [
    "IBVMax",
    "MeasuredAmpsMax",
    "RPMMax",
    "SOC_percent",
    "EngineRunTime",
    "AlternatorOnTime",
    "AlternatorFuelUsed",
    "ChargedEnergy",
    "DischargedEnergy",
    "AlternatorChargedEnergy",
    "MaxAlternatorTemperatureF",
    "temperatureThermistor",
    "MaxTemperatureThermistor",
    "VictronCurrent",
    "timeToFullChargeMin",
    "timeToFullDischargeMin",
    "LatitudeNMEA",
    "LongitudeNMEA",
    "SatelliteCountNMEA",
    "LastSessionDuration",
    "LastSessionMaxLoopTime",
    "lastSessionMinHeap",
    "wifiReconnectsTotal",
    "LastResetReason",
    "ancientResetReason",
    "totalPowerCycles",
    "MinFreeHeap",
    "currentWeatherMode",
    "UVToday",
    "UVTomorrow",
    "UVDay2",
    "weatherDataValid",
    "reserved_SolarWatts",        // moved to CSV3
    "reserved_performanceRatio",  // moved to CSV3
    "reserved_VeData",            // moved to CSV3
    "reserved_NMEA0183Data",      // moved to CSV3
    "reserved_NMEA2KData",        // moved to CSV3
    "AlarmLatchState",
    "ResetAlarmLatch",
    "reserved_ResetLearningTable",    // action-only, echo global removed
    "reserved_ClearOverheatHistory",  // action-only, echo global removed
    "DynamicShuntGainFactor",
    "DynamicAltCurrentZero",
    "InsulationLifePercent",
    "GreaseLifePercent",
    "BrushLifePercent",
    "PredictedLifeHours",
    "LifeIndicatorColor",
    "pKwHrToday",
    "pKwHrTomorrow",
    "pKwHr2days",
    "ambientTemp",
    "baroPressure",
    "firmwareVersionInt",
    "deviceIdUpper",
    "deviceIdLower",
    "ChargedEnergy_AllTime",
    "AlternatorFuelUsed_AllTime",
    "PeakVoltage_AllTime",
    "EngineRunTime_AllTime",
    "MinVoltage",
    "MinVoltage_AllTime",
    "ChargeCycles",
    "ChargeCycles_AllTime",
    "EngineFuelUsed",
    "EngineFuelUsed_AllTime",
    "TotalDistance",
    "TotalDistance_AllTime",
    "MaxSpeed",
    "MaxSpeed_AllTime",
    "SolarChargedEnergy",
    "SolarChargedEnergy_AllTime",
    "AlternatorChargedEnergy_AllTime",
    "DischargedEnergy_AllTime",
    "AvgSOC_AllTime",
    "AvgSpeed_AllTime",
    "AvgSpeed",
    "AlternatorOnTime_AllTime",
    "EngineCycles_AllTime",
    "MaxAlternatorTemperatureF_AllTime",
    "MaxTemperatureThermistor_AllTime",
    "MeasuredAmpsMax_AllTime",
    "RPMMax_AllTime",
    "Ignition",
    "BulkStage",
    "WifiWakeSecondsRemaining",
    "BufferedRecordCount",
    "BufferedRecordPercent",
    "BufferedRecordCap",
    "COGNMEA",
    "SOGNMEA",
    "ApparentWindSpeedNMEA",
    "ApparentWindAngleNMEA",
    "TrueWindSpeedNMEA",
    "TrueWindAngleNMEA",
    "LeewayNMEA",
    "VMGNMEA",
    "VMGTargetBearing",
    "reserved_VMGUseTrueWind",             // 98 reserved — moved to CSV3
    "cpuLoadCore0",
    "cpuLoadCore0Max",
    "cpuLoadCore1",
    "cpuLoadCore1Max",
    "hasForcedUpdate",
    "forcedFwVersionInt",
    "forcedUpdateDeadline",
    "stateRevision",
    "reserved_hardwarePresent",            // 107 reserved — moved to CSV3
    "imu_accel_x_raw",
    "imu_accel_y_raw",
    "imu_accel_z_raw",
    "imu_gyro_x_raw",
    "imu_gyro_y_raw",
    "imu_gyro_z_raw",
    "accel_x_min",
    "accel_x_max",
    "accel_x_avg",
    "accel_y_min",
    "accel_y_max",
    "accel_y_avg",
    "accel_z_min",
    "accel_z_max",
    "accel_z_avg",
    "gyro_x_min",
    "gyro_x_max",
    "gyro_x_avg",
    "gyro_y_min",
    "gyro_y_max",
    "gyro_y_avg",
    "gyro_z_min",
    "gyro_z_max",
    "gyro_z_avg",
    "heel_min",
    "heel_max",
    "heel_avg",
    "pitch_min",
    "pitch_max",
    "pitch_avg",
    "vertical_accel_min",
    "vertical_accel_max",
    "vertical_accel_avg",
    "total_accel_min",
    "total_accel_max",
    "total_accel_avg",
    "imu_slam_count",
    "imu_slam_peak_max",
    "imu_slam_count_lifetime",
    "imu_capsize_count",
    "imu_pitchpole_count",
    "imu_heel_change_60s",
    "imu_heel_deviation_60s",
    "imu_pitch_change_60s",
    "imu_pitch_deviation_60s",
    "imu_wave_period_sec",
    "imu_heel_max_lifetime",
    "imu_pitch_max_lifetime",
    "imu_slam_peak_lifetime",
    "imu_fifo_overrun_count",
    "imu_i2c_error_count",
    "imu_unknown_tag_count",
    "imu_accel_dropped",
    "imu_gyro_dropped",
    "imu_total_samples_accel",
    "imu_total_samples_gyro",
    "IMUReadTime2",
    "IMUReadTime",
    "adsI2CErrorCount",
    "tempPIDActive",
    "tempPIDInput_d",
    "tempPIDSetpoint_d",
    "thermalPenaltyAmps",
    "innerTermP",
    "innerTermI",
    "innerTermD",
    "outerTermP",
    "outerTermI",
    "outerTermLookahead",
    "thermalSlopeFPerSec",
    "chargeStageDisplay",
    "voltageControlActive",
    "voltageError",
    "cv_I",
    "inIdleStage",
    "altBaselineFrozen",
    "ft_rai_total_win",
    "ft_rai_total_ses",
    "ft_rai_ina228_win",
    "ft_rai_ina228_ses",
    "ft_rai_ads_state_win",
    "ft_rai_ads_state_ses",
    "ft_rai_bmp_state_win",
    "ft_rai_bmp_state_ses",
    "ft_rai_imu_win",
    "ft_rai_imu_ses",
    "reserved_cv_D",                       // 194 reserved — was cv_D (D term removed)
    "tempReadFailCount",
    "tempCrcFailCount",
    "tempCrcRecoveredCount",
    "tempAllFFCount",
    "tempPowerOn85Count",
    "tempOutOfRangeCount",
    "tempRequestFailCount",
    "tempConnectedFailCount",
    "tempResolutionFixCount",
    "tempRereadFailCount",
    "tempResolutionFixCrcFailCount",
    "tempEnumerateFailCount",
    "warmupCeiling",
    "imu_min_moving_gentle",
    "imu_min_moving_moderate",
    "imu_min_moving_rough",
    "imu_min_moving_extreme",
    "imu_min_stat_gentle",
    "imu_min_stat_moderate",
    "imu_min_stat_rough",
    "imu_min_stat_extreme",
    "imu_heel_deviation_120s",
    "imu_pitch_deviation_120s",
    "imu_heading_swing_120s",
    "dBcur_dt",
    "loadDumpActive",
    "thermalLiveScore0",
    "thermalLiveScore1",
    "thermalLiveScore2",
    "thermalLiveScore3",
    "thermalTuningTestPhase",
    "ft_updateAccelMetrics_win",
    "ft_updateAccelMetrics_ses",
    "WifiStrength",
    "SendWifiTime",
    "AnalogReadTime",
    "VeTime",
    "MaximumLoopTime",
    "HeadingNMEA",
    "EngineCycles",
    "CurrentSessionDuration",
    "reserved_timeAxisModeChanging",  // moved to CSV3
    "currentPartitionType",
    "fastOvCurrentCap",
    "fastOvClampCount",
    "fastOvHardCount",                     // 240 (was 243 before fastOvSoftCount removal)
    "ch1_last_ms",
    "ch1_avg_10s",
    "ch1_worst_10s",
    "ch1_over2x_10s",
    "ch1_n_10s",
    "ch1_avg_2m",
    "ch1_worst_2m",
    "ch1_over2x_2m",
    "ch1_n_2m",
    "ch1_avg_at",
    "ch1_worst_at",
    "ch1_over2x_at",
    "ch1_n_at",
    "iExcessCount",
    "inaOVCount",
    "hardOCCount",
    "voltSpikeCount",
    "voltDisagreeCritCount",
    "voltDisagreeWarnCount",
    "voltImplausibleCount",
    "tempCritCount",
    "tempSustainedCount",
    "tempStaleCount",
    "currentStaleCount",
    "imu_msi_score",
    "imu_vomit_pct",
    "imu_anchorage_comfort",
    "ina_last_ms",
    "ina_avg_10s",
    "ina_worst_10s",
    "ina_over2x_10s",
    "ina_avg_2m",
    "ina_worst_2m",
    "ina_over2x_2m",
    "ina_avg_at",
    "ina_worst_at",
    "ina_over2x_at",
    "loopTime5sWindow_ms",
    "MaximumLoopTime_ms",
    "ft_SendWifiData_win",
    "ft_SendWifiData_ses",
    "ft_CheckAlarms_win",
    "ft_CheckAlarms_ses",
    "ft_calculateDerivedMetrics_win",
    "ft_calculateDerivedMetrics_ses",
    "ft_logDashboardValues_win",
    "ft_logDashboardValues_ses",
    "ft_updateSystemHealthStats_win",
    "ft_updateSystemHealthStats_ses",
    "ft_checkWiFiConnection_win",
    "ft_checkWiFiConnection_ses",
    "ft_ch1_compute_stats_win",
    "ft_ch1_compute_stats_ses",
    "ft_UpdateEngineRuntime_win",
    "ft_UpdateEngineRuntime_ses",
    "ft_UpdateEngineFuel_win",
    "ft_UpdateEngineFuel_ses",
    "ft_UpdateBatterySOC_win",
    "ft_UpdateBatterySOC_ses",
    "ft_UpdateTravelStatistics_win",
    "ft_UpdateTravelStatistics_ses",
    "ft_UpdateBoardTempPressureMaximums_win",
    "ft_UpdateBoardTempPressureMaximums_ses",
    "ft_handleSocGainReset_win",
    "ft_handleSocGainReset_ses",
    "ft_handleAltZeroReset_win",
    "ft_handleAltZeroReset_ses",
    "ft_calculateChargeTimes_win",
    "ft_calculateChargeTimes_ses",
    "ft_UpdateSailingMetrics_win",
    "ft_UpdateSailingMetrics_ses",
    "ft_updateWeatherMode_win",
    "ft_updateWeatherMode_ses",
    "ft_updateSensorWindow_win",
    "ft_updateSensorWindow_ses",
    "ft_checkTimeSync_win",
    "ft_checkTimeSync_ses",
    "currentRPMTableIndex",
    "pidInitialized",
    "pidSetpoint",
    "TempToUse",
    "learningTargetFromRPM",
    "ambientTempCorrection",
    "finalLearningTarget",
    "overheatingPenaltyTimer",
    "overheatingPenaltyAmps",
    "averageTableValue",
    "timeSinceLastOverheat",
    "socInfoAvailable",
    "overheatCount0",
    "overheatCount1",
    "overheatCount2",
    "overheatCount3",
    "overheatCount4",
    "overheatCount5",
    "overheatCount6",
    "overheatCount7",
    "overheatCount8",
    "overheatCount9",
    "cumulativeNoOverheatTime0",
    "cumulativeNoOverheatTime1",
    "cumulativeNoOverheatTime2",
    "cumulativeNoOverheatTime3",
    "cumulativeNoOverheatTime4",
    "cumulativeNoOverheatTime5",
    "cumulativeNoOverheatTime6",
    "cumulativeNoOverheatTime7",
    "cumulativeNoOverheatTime8",
    "cumulativeNoOverheatTime9",
    "learningUpCount0",
    "learningUpCount1",
    "learningUpCount2",
    "learningUpCount3",
    "learningUpCount4",
    "learningUpCount5",
    "learningUpCount6",
    "learningUpCount7",
    "learningUpCount8",
    "learningUpCount9",
    "totalLearningEvents",
    "totalOverheats",
    "totalSafeHours",
    "FreeInternalRam",
    "TotalInternalRam",
    "LargestInternalBlock",
    "FreePSRAM",
    "TotalPSRAM",
    "Heapfrag",
    "ft_ReadAnalogInputs_win",
    "ft_ReadAnalogInputs_ses",
    "ft_AdjustFieldLearnMode_win",
    "ft_AdjustFieldLearnMode_ses",
    "ft_uploadSensorHistory_win",
    "ft_uploadSensorHistory_ses",
    "ft_uploadBufferedRecords_win",
    "ft_uploadBufferedRecords_ses",
    "ft_buildConfigPayload_win",
    "ft_buildConfigPayload_ses",
    "VeTime2",
    "systemIDRiseDelay_0",
    "systemIDRiseDelay_1",
    "systemIDRiseDelay_2",
    "systemIDFallDelay_0",
    "systemIDFallDelay_1",
    "systemIDFallDelay_2",
    "systemIDRiseAvg",
    "systemIDFallAvg",
    "ft_altHealth_win",
    "ft_altHealth_ses",
    "ft_altFold_win",
    "ft_altFold_ses",
    "ft_boatPerf_win",
    "ft_boatPerf_ses",
    "systemIDActive",
    "systemIDResultsReady",
    "systemIDStepAmp_0",
    "systemIDStepAmp_1",
    "systemIDStepAmp_2",
    "systemIDQuietPP_0",
    "systemIDQuietPP_1",
    "systemIDQuietPP_2",
    "systemIDAbortReason",                 // FieldEventReason code if protection aborted last test; 0=no abort
    "systemIDAbortPhase",                  // phase 1-9 at moment of protection abort; 0=no abort
    "vl_last_ms",                          // CV voltage-loop firing-interval ladder (CH1/pf-style)
    "vl_avg_10s",
    "vl_worst_10s",
    "vl_over2x_10s",
    "vl_avg_2m",
    "vl_worst_2m",
    "vl_over2x_2m",
    "vl_avg_at",
    "vl_worst_at",
    "vl_over2x_at",
    "nvsSecsSinceLastSave",                // seconds since last successful saveNVSDataFull() (0 = never)
    "nvsFullSaveLastMs",                   // wall-clock duration of most recent saveNVSDataFull() (ms)
    "nvsFullSaveWorstMs",                  // worst saveNVSDataFull() duration since boot (ms)
    "nvsFullSaveCount",                    // total saveNVSDataFull() calls since boot
    // 28 ignition-cycle watermarks (lo + hi pairs, reset every boot). Order MUST match firmware CSV2 enum.
    "wmIgn_amps_lo",     "wmIgn_amps_hi",      // MeasuredAmps (A, int)
    "wmIgn_altTempF_lo", "wmIgn_altTempF_hi",  // AlternatorTemperatureF (°F, int)
    "wmIgn_IBV_lo",      "wmIgn_IBV_hi",       // INA228 battery V (×10, 1 decimal)
    "wmIgn_Bcur_lo",     "wmIgn_Bcur_hi",      // INA228 battery A (int)
    "wmIgn_SOC_lo",      "wmIgn_SOC_hi",       // SOC percent (0..100, int)
    "wmIgn_RPM_lo",      "wmIgn_RPM_hi",       // Engine RPM (int)
    "wmIgn_SOG_lo",      "wmIgn_SOG_hi",       // SOGNMEA knots (int)
    "wmIgn_AWS_lo",      "wmIgn_AWS_hi",       // ApparentWindSpeedNMEA knots (int)
    "wmIgn_TWS_lo",      "wmIgn_TWS_hi",       // TrueWindSpeedNMEA knots (int)
    "wmIgn_heel_lo",     "wmIgn_heel_hi",      // imu_heel_deg (int)
    "wmIgn_pitch_lo",    "wmIgn_pitch_hi",     // imu_pitch_deg (int)
    "wmIgn_vacc_lo",     "wmIgn_vacc_hi",      // imu_vertical_accel_g (×10, 1 decimal)
    "wmIgn_baro_lo",     "wmIgn_baro_hi",      // baroPressure mbar (int)
    "wmIgn_ambient_lo",  "wmIgn_ambient_hi",   // ambientTemp °F (int)
    "restartRemainingSec",                     // seconds until scheduled reboot (0 = banner hidden)
    "currentGpsSource",                        // 0=none, 1=NMEA, 2=Phone, 3=Manual
    "currentTimeSource",                       // 0=none, 1=GPS, 2=Phone, 3=NTP, 4=drifting
    "loggingActive",                           // 439 — 1=logging active, 0=stopped (Stop/Start Logs)
    "VMGUpwind",                               // VMG to windward, knots ×100
    "sustainedTWS",                            // 2-min sustained true wind, knots ×10 (Beaufort + gale basis)
    "currentGaleMinutes",                      // live minutes continuously in a gale (sustained ≥34kt), int
    "wmIgn_VMGman_lo",   "wmIgn_VMGman_hi",    // VMG manual session min/max (knots ×10)
    "wmIgn_VMGup_lo",    "wmIgn_VMGup_hi",     // VMG upwind session min/max (knots ×10)
    "altHealthPct",        // health % ×10
    "altHealthStatus",     // 0 learn,1 healthy,2 drift-hi,3 drift-lo,4 low-coverage
    "altCoveragePct",      // frozen/with-data % ×10
    "altObsCount",         // scored observations since freeze
    "imuHeelOffset",       // IMU zero rest heel offset (deg ×100)
    "imuPitchOffset",      // IMU zero rest pitch offset (deg ×100)
    // Victron VE.Direct solar/MPPT live block (10 fields)
    "VictronSolarPower_W",        // PPV panel power (W ×1)
    "VictronSolarVoltage_V",      // VPV panel voltage (V ×100)
    "VictronSolarCurrent_A",      // derived panel current (A ×100)
    "VictronChargeState",         // CS code (×1)
    "VictronMPPTMode",            // MPPT tracker code (×1)
    "VictronError",               // ERR code (×1)
    "VictronYieldToday_kWh",      // H20 yield today (kWh ×100)
    "VictronMaxPowerToday_W",     // H21 max power today (W ×1)
    "VictronYieldYesterday_kWh",  // H22 yield yesterday (kWh ×100)
    "VictronMaxPowerYesterday_W", // H23 max power yesterday (W ×1)
    "currentFuelGPH",             // live fuel flow (gal/hr ×100)
    "currentNMPG",                // live fuel economy (naut mi/gal ×100)
    // session fuel-economy curve: mpg per RPM bin (18 bins spanning 0..fuelCurveTopRPM), naut mi/gal ×100, 0 = empty
    "fuelCurveNMPG_0", "fuelCurveNMPG_1", "fuelCurveNMPG_2", "fuelCurveNMPG_3", "fuelCurveNMPG_4", "fuelCurveNMPG_5",
    "fuelCurveNMPG_6", "fuelCurveNMPG_7", "fuelCurveNMPG_8", "fuelCurveNMPG_9", "fuelCurveNMPG_10", "fuelCurveNMPG_11",
    "fuelCurveNMPG_12", "fuelCurveNMPG_13", "fuelCurveNMPG_14", "fuelCurveNMPG_15", "fuelCurveNMPG_16", "fuelCurveNMPG_17",
    "fuelCurveTopRPM",            // top configured fuel-table RPM -> chart x-axis scale (×1)
    // 80MHz low-power loop instrumentation (4 fields)
    "loopWorst80Win_ms",          // worst 80MHz loop pass, rolling 5s (ms)
    "loopWorst80Ses_ms",          // worst 80MHz loop pass since Reset Peak Values (ms)
    "loopOver80ImuLimitCount",    // # 80MHz passes over ~38ms accel-drain limit since reset
    "loop80IterCount",            // total 80MHz passes since reset
    // field-ON loop instrumentation (2 fields) — worst pass while actually regulating
    "loopFieldOnWin_ms",          // worst field-ON loop pass, rolling 5s (ms)
    "loopFieldOnSes_ms",          // worst field-ON loop pass since Reset Peak Values (ms)
    "STWNMEA",                    // Speed Through Water (SOW, knots ×100); -1 = NAN/no log
    // thermal tuning plot live-stream fields (replaces the old /thermallog.bin pull)
    "tempFiltered",               // IIR-filtered alt temp (°F ×100); distinct from raw AlternatorTemperatureF
    "outerImpliedPenalty",        // voltage cap as downstream amps penalty (A ×100); still streamed but no longer plotted (voltage-loop info, dropped from the thermal subtab)
    "thermalFlags",               // state-strip bitfield: bit0 tempPIDActive, bit4 AUTO, bit5 shutdown
    "outerAntiWindupFired",       // 1 = CV-bleed anti-windup fired since last frame (red ticks)
    // +10: Inner Current PID firing interval (field-on-gated), CH1-style stats (avg ÷100)
    "pf_last_ms",
    "pf_avg_10s",
    "pf_worst_10s",
    "pf_over2x_10s",
    "pf_avg_2m",
    "pf_worst_2m",
    "pf_over2x_2m",
    "pf_avg_at",
    "pf_worst_at",
    "pf_over2x_at",
    "inaBusReadWorstUs",
    "inaBusSlowCount",
    "ina228ErrorCount",
    "imuFifoFetchWorstUs",
    "imuFifoWorstSamples",
    "ft_dumpLongTermRing_win",
    "ft_dumpLongTermRing_ses",
    // +12: fast alternator-current channel (GPIO3)
    "ft_fastAltDrain_win",     // bounded DMA drain worst, rolling 5s (µs, displayed ms)
    "ft_fastAltDrain_ses",     // ...since Reset Peak Values
    "ft_faMatrixFlush_win",    // disturbance-matrix/flipbook flash flush worst, rolling 5s
    "ft_faMatrixFlush_ses",    // ...since Reset Peak Values
    "ft_faDetector_win",       // failure-detector analysis slice worst, rolling 5s
    "ft_faDetector_ses",       // ...since Reset Peak Values
    "ft_faWindowFinalize_win", // per-2s-window finalize worst, rolling 5s
    "ft_faWindowFinalize_ses", // ...since Reset Peak Values
    "faChanState",             // 0 = off, 1 = sampling, 2 = railed/dormant
    "faCellsUsed",             // matrix cells with ≥1 qualified window
    "faDetectK",               // detector fault class of last FAULT verdict, 0 = quiet
    "faSesPkpkWorst",          // session-worst broadband pk-pk, A ×100
    "faSesPeakWorst",          // session-worst spectral peak, A ×100
    "faSesPeakWorstHz",        // ...its frequency, Hz ×10
    "faAnomalyCount",          // lifetime detector FAULT-verdict count
    "faDomFreqHz",             // Highest Tone in Map: frequency, Hz ×10
    "faDomAmp",                // ...amplitude, A ×100
    "faDomRpm",                // ...RPM where it occurs
    // gate-tuning 10s live readouts (ROLL_EMPTY sentinel = no sample in window; see attachLiveReadout)
    "faRpmEdge10sMin",         // RPM edge margin, 10s trough (RPM ×10)
    "faAmpsDrift10sMax",       // amps-drift EMA spread, 10s peak (A ×100)
    "faTonePk10sMax",          // largest spectral peak, 10s peak (A ×100)
    "ldSlew10sMax",            // current slew, 10s peak (A/s ×10)
    "cvSlope10sMax",           // voltage rise, 10s peak (V/s ×10000)
];

// ── Gate-tuning live readouts ───────────────────────────────────────────────
// Several thresholds gate on a live/windowed quantity the user can't otherwise see (RPM edge
// margin, current-drift spread, spectral tone peak, current slew, voltage slope). The firmware
// streams a 10s rolling extreme of each on CSV2 (ROLL_EMPTY = no sample in the window); IExcessK's
// quantity rides CSV1 at 10Hz so its 10s peak is computed here. Each readout shows the number to
// set the threshold relative to. Spans live next to each threshold input in index.html.
const ROLL_EMPTY_SENTINEL = -1999999999;   // firmware sends -2000000000 when a 10s window had no sample
const GATE_READOUTS_CSV2 = [
    { spans: ['faRpmEdgeMargin_live'],                                                       f: 'faRpmEdge10sMin',  s: 10,    lbl: '10s worst margin', u: 'RPM', d: 1 },
    { spans: ['faAmpsDriftFloorA_live', 'faAmpsDriftPct_live'],                              f: 'faAmpsDrift10sMax', s: 100,  lbl: '10s peak drift',   u: 'A',   d: 2 },
    { spans: ['faPeakMinA_live'],                                                            f: 'faTonePk10sMax',   s: 100,   lbl: '10s peak tone',    u: 'A',   d: 2 },
    { spans: ['LoadDumpDtThresh1_live', 'LoadDumpDtThresh_live', 'LoadDumpDtThresh3_live'],  f: 'ldSlew10sMax',     s: 10,    lbl: '10s peak slew',    u: 'A/s', d: 1 },
    { spans: ['SlopeBleedThresh_live'],                                                      f: 'cvSlope10sMax',    s: 10000, lbl: '10s peak rise',    u: 'V/s', d: 3 },
];
function gateReadoutOnCsv2(data) {
    for (const r of GATE_READOUTS_CSV2) {
        const raw = Number(data[r.f]);
        const txt = (!isFinite(raw) || raw <= ROLL_EMPTY_SENTINEL)
            ? r.lbl + ': —'
            : `${r.lbl}: ${(raw / r.s).toFixed(r.d)} ${r.u}`;
        for (const id of r.spans) { const el = document.getElementById(id); if (el) el.textContent = txt; }
    }
}
// IExcessK: peak of (measured amps − active setpoint) over the last 10s, from CSV1 (both sent A×100).
let _iExcessRing10s = [];
function gateReadoutOnCsv1(data) {
    const el = document.getElementById('IExcessK_live');
    if (!el) return;
    const ma = Number(data.MeasuredAmps), sp = Number(data.setpointLimited);
    if (isFinite(ma) && isFinite(sp)) {
        const now = (window.performance && performance.now) ? performance.now() : Date.now();
        _iExcessRing10s.push([now, (ma - sp) / 100]);
        const cut = now - 10000;
        while (_iExcessRing10s.length && _iExcessRing10s[0][0] < cut) _iExcessRing10s.shift();
    }
    if (!_iExcessRing10s.length) { el.textContent = '10s peak over setpoint: —'; return; }
    let pk = -Infinity;
    for (const s of _iExcessRing10s) if (s[1] > pk) pk = s[1];
    el.textContent = `10s peak over setpoint: ${pk.toFixed(2)} A`;
}

// ── Charging-system health (v2): schema-driven live + settings, perf-vs-engine-hours trend ──
// Schema-driven (fetch /altschema ONCE, zip names against AltLive/AltSettings) — no hardcoded array.
let altSchema = null;
let altSettings = {};
let altLive = { valid:false, rpm:0, exc:0, amps:0, pred:0, pct:0, worstPct:0, overallPct:0,
                status:0, steady:false, engHours:0, coverage:0, haveCurve:0, ptCount:0,
                source:0, paused:0, refOk:1, refDist:0, state:3,
                sessionMean:0, sessionP10:0, sessionN:0, hiFieldAlert:0, sim:0 };
let altTrend = [];     // committed trend points: [{eng, worst, overall}]
let _altTrendPending = false, _altTrendLastFetch = 0;

function fetchAltSchema(){ return fetch('/altschema').then(r=>r.json()).then(j=>{ altSchema=j; }).catch(()=>{}); }

// Confidence-state labels + colors (firmware FrontStore::classify; OUTPUT-BLIND \u2014 position +
// record-book geometry only). 0/1 show the live %; 2/3 deliberately show no number.
const ALT_STATE_LABEL = ['MEASURED','ESTIMATED','Learning this operating region','No reference here yet'];
const ALT_STATE_COLOR = ['#5cb85c','#3a7bd5','#888','#888'];

function updateAltHealth() {
  const pctEl = document.getElementById('alt-health-pct');
  const statEl = document.getElementById('alt-health-status');
  const covEl = document.getElementById('alt-coverage');
  // Headline = LIVE output % vs best-ever (NO clamp \u2014 can read > 100 vs a stale front) + the
  // confidence state. No verdict strings \u2014 trends are read from the plots, not editorialized here.
  const st = altLive.state|0;
  const graded = altLive.valid && (st===0 || st===1) && altLive.pct>0;
  if (pctEl) {
    pctEl.textContent = graded ? Math.round(altLive.pct)+'%' : '\u2014';
    pctEl.style.color = graded ? ALT_STATE_COLOR[st] : '';
  }
  if (statEl) {
    const fixed = altLive.source >= 1;
    let txt = altLive.status===3 ? 'Disabled (Ignore Temperature)'
            : !altLive.valid ? 'Not running'
            : (ALT_STATE_LABEL[st] || '');
    if (altLive.status!==3 && fixed) txt += ' \u00b7 FIXED reference';
    else if (altLive.status!==3 && altLive.paused>=1) txt += ' \u00b7 paused';
    statEl.textContent = txt;
    statEl.style.color = graded ? ALT_STATE_COLOR[st] : '#888';
  }
  // High-field-low-output alert (independent safety net \u2014 fires regardless of the record book)
  const hfEl = document.getElementById('alt-hifield-alert');
  if (hfEl) hfEl.style.display = altLive.hiFieldAlert>=1 ? '' : 'none';
  // Session statistics over the graded 1 Hz samples (mean + P10) \u2014 a real ~5% decline shows here
  // before the hourly trend buckets can.
  const sessEl = document.getElementById('alt-session-stats');
  if (sessEl) sessEl.textContent = (altLive.sessionN>=10)
    ? ('this session: mean '+Math.round(altLive.sessionMean)+'% \u00b7 P10 '+Math.round(altLive.sessionP10)+'% \u00b7 '+Math.round(altLive.sessionN)+' graded samples')
    : '';
  // Front size + engine-hours of data gathered (no live "now %": the plot's dot already shows it).
  if (covEl) {
    covEl.textContent = (altLive.ptCount>0)
      ? (Math.round(altLive.ptCount)+' front points \u00b7 '+Math.round(altLive.engHours)+' engine-hr')
      : 'no front points yet';
  }
  const dot = document.getElementById('alt-steady-dot');
  if (dot) { dot.style.background = altLive.steady ? '#5cb85c' : '#ccc'; dot.title = altLive.steady?'in a steady run (folding)':'not steady'; }
  const modeLbl = document.getElementById('alt-mode-label');
  if (modeLbl) modeLbl.textContent = altLive.source>=1 ? 'FIXED' : (altLive.paused>=1 ? 'LEARNED (paused)' : 'LEARNED');
  setSeg(['alt-sim-off','alt-sim-on'], altLive.sim>=1?1:0);   // simulator now lives in Settings (segmented, mirrors Vessel Performance)
}

// Simulator Off(0)/On(1) — segmented toggle in Settings, parallels Vessel Performance's perfSimMode.
function altSetSim(v){
  if(!currentAdminPassword){ alert('Please unlock settings first'); return; }
  fetchWithTimeout(buildURL('/get?password='+encodeURIComponent(currentAdminPassword)+'&altSimMode='+(v?1:0)),{},5000).catch(()=>{});
}
// LEARNED↔FIXED reference toggle: FIXED (1) freezes + pauses learning; LEARNED (0) resumes.
function altSetSource(src){
  if(!currentAdminPassword){ alert('Please unlock settings first'); return; }
  fetchWithTimeout(buildURL('/get?password='+encodeURIComponent(currentAdminPassword)+'&altSource='+(src?1:0)),{},5000).catch(()=>{});
}
// Fetch a CSV endpoint and SAVE it to the browser's Downloads as a dated, human-named file
// (e.g. "Alternator Health Data 2026-06-05 12-47-00.csv") rather than rendering it inline. The
// saved file round-trips straight back through the matching Load CSV button. Shared by both the
// alternator-health and boat-performance download buttons. (Browser flow; Capacitor file-save is
// a separate path.)
function downloadCsv(url, baseName){
  fetchWithTimeout(buildURL(url),{},10000).then(r=>r.text()).then(txt=>{
    if(!txt || txt.trim().length < 8){ alert('No '+baseName+' to download yet.'); return; }
    const d=new Date(), p=n=>String(n).padStart(2,'0');
    const stamp=d.getFullYear()+'-'+p(d.getMonth()+1)+'-'+p(d.getDate())+' '+p(d.getHours())+'-'+p(d.getMinutes())+'-'+p(d.getSeconds());
    const a=document.createElement('a');
    a.href=URL.createObjectURL(new Blob([txt],{type:'text/csv'}));
    a.download=baseName+' '+stamp+'.csv';
    document.body.appendChild(a); a.click();
    setTimeout(()=>{ URL.revokeObjectURL(a.href); a.remove(); }, 1000);
  }).catch(e=>alert('Download failed: '+(e&&e.message?e.message:e)));
}
// Import a health curve from a file in Downloads (BEFRONT1). Opens the native picker;
// altUploadCsvFile() reads it and POSTs the raw text to /altUploadFront. Mirrors perfLoadCsv.
function altLoadCsv(){
  if(!currentAdminPassword){ alert('Please unlock settings first'); return; }
  const inp=document.getElementById('alt-load-file'); if(inp){ inp.value=''; inp.click(); }   // freeze/learn is asked after the file is chosen
}
function altUploadCsvFile(inp){
  const f=inp.files && inp.files[0]; if(!f) return;
  // Ask how to use the imported curve, right after the file is chosen: freeze (hold as-is) vs learn (adopt + refine).
  const freeze = confirm('Import "'+f.name+'"\n\nHow should this health curve be used?\n\nOK = FREEZE — hold it exactly as imported (FIXED, learning paused). Stays on this device.\nCancel = LEARN — adopt it as your own and keep refining from your engine. Uploaded to the cloud as this device\'s data (tagged as imported).');
  const rd=new FileReader();
  rd.onload=function(){
    const txt=String(rd.result||'');
    if(txt.indexOf('BEFRONT1')<0){ alert('That file is not an alternator health (BEFRONT1) CSV.'); inp.value=''; return; }
    fetchWithTimeout(buildURL('/altUploadFront?password='+encodeURIComponent(currentAdminPassword)+'&fixed='+(freeze?1:0)),{method:'POST',body:txt},10000)
      .then(r=>{ if(r.ok){ alert('Health data imported — '+(freeze?'FIXED (learning paused)':'LEARNED (adopting to cloud; uploads at the next field-off)')+'.'); if(typeof fetchAltTrend==='function') fetchAltTrend(); }
                 else { r.text().then(t=>alert('Import failed: '+(t||('HTTP '+r.status)))).catch(()=>alert('Import failed: HTTP '+r.status)); } })
      .catch(e=>alert('Import failed: '+(e&&e.message?e.message:e)));
    inp.value='';
  };
  rd.onerror=function(){ alert('Could not read that file.'); inp.value=''; };
  rd.readAsText(f);
}

function fetchAltTrend() {
  fetch('/alttrend.csv').then(r=>r.text()).then(txt=>{
    const ln = txt.trim().split('\n'); altTrend = [];
    for (let i=1;i<ln.length;i++){ const c=ln[i].split(','); if(c.length<3) continue;
      altTrend.push({eng:+c[0], worst:+c[1], overall:+c[2]}); }
    drawAltTrend();
  }).catch(()=>{});
}

function queueAltTrendUpdate() {
  const now = Date.now();
  if (now - _altTrendLastFetch > 15000) { _altTrendLastFetch = now; fetchAltTrend(); }
  if (_altTrendPending) return;
  _altTrendPending = true;
  requestAnimationFrame(()=>{ _altTrendPending = false; drawAltTrend(); });
}

// Hi-DPI canvas: size the backing store to the on-screen CSS box × devicePixelRatio, then
// map a fixed logical coordinate space (W×H) onto it. Kills the blurry/greyed "retina" look
// while letting every draw fn keep its existing W/H-based layout math unchanged.
function hidpiCtx(cv, W, H) {
  const dpr = window.devicePixelRatio || 1;
  const dispW = cv.clientWidth || W, dispH = cv.clientHeight || H;
  cv.width = Math.round(dispW * dpr);
  cv.height = Math.round(dispH * dpr);
  const ctx = cv.getContext('2d');
  ctx.setTransform(cv.width / W, 0, 0, cv.height / H, 0, 0);
  return ctx;
}

// Performance-%-vs-engine-hours trend. Bold line = worst operating region (early warning);
// faint line = overall. Live "now" dot appended once at least one bucket has committed. Empty \u2192
// local "no trend yet" notice for the whole first engine-hour
// (the trend is built on-device from engine-hours \u2014 NOT from the cloud; don't reintroduce a cloud message).
function drawAltTrend() {
  const cv = document.getElementById('alt-trend');
  if (!cv || !cv.getContext) return;
  const W = 520, H = 300, padL = 56, padR = 18, padT = 16, padB = 40, ctx = hidpiCtx(cv, W, H);
  ctx.clearRect(0,0,W,H);
  const pts = altTrend.slice();
  // "now" dot = the LIVE % with its state color (only while graded MEASURED/ESTIMATED) — not the
  // ratcheted bucket-worst, which froze one bad reading on the dot for a whole engine-hour.
  // Suppressed until the first committed bucket exists: before then the plot's only content would
  // be a wandering/recoloring/vanishing dot, which reads as broken. The session plot covers live %.
  const liveSt = altLive.state|0;
  const liveGraded = altLive.valid && (liveSt===0 || liveSt===1) && altLive.pct>0;
  if (liveGraded && pts.length > 0)
    pts.push({eng:altLive.engHours, worst:altLive.pct, overall:altLive.overallPct, live:true});
  if (pts.length === 0){
    ctx.fillStyle='#999'; ctx.font='13px sans-serif'; ctx.textAlign='center';
    ctx.fillText('No trend yet — one point logs per engine-hour of running', W/2, H/2); ctx.textAlign='left'; return;
  }
  let maxE = 0; pts.forEach(p=>{ if(p.eng>maxE)maxE=p.eng; }); if(maxE<1)maxE=1;
  let minY = 70; const maxY = 105;
  pts.forEach(p=>{ if(p.worst<minY)minY=Math.max(0,Math.floor(p.worst/5)*5); });
  const X = e => padL + (e/maxE)*(W-padL-padR);
  const Y = v => H-padB - ((v-minY)/(maxY-minY))*(H-padT-padB);
  // Y gridlines + % labels
  ctx.font='10px sans-serif'; ctx.textAlign='right'; ctx.textBaseline='middle';
  for (let v=Math.ceil(minY/10)*10; v<=maxY; v+=10){
    const y=Y(v); ctx.strokeStyle='#eee'; ctx.lineWidth=1; ctx.beginPath(); ctx.moveTo(padL,y); ctx.lineTo(W-padR,y); ctx.stroke();
    ctx.fillStyle='#999'; ctx.fillText(v+'%', padL-7, y);
  }
  if (100>=minY && 100<=maxY){ const y=Y(100); ctx.strokeStyle='#cde'; ctx.setLineDash([4,4]); ctx.beginPath(); ctx.moveTo(padL,y); ctx.lineTo(W-padR,y); ctx.stroke(); ctx.setLineDash([]); }
  // X gridlines + engine-hour labels. Integer labels only when the range gives each tick its own
  // integer (maxE ≥ 4); small ranges get one-decimal labels (Math.round alone produced "0 0 1 1 1").
  ctx.textAlign='center'; ctx.textBaseline='top';
  for (let k=0;k<=4;k++){ const e=maxE*k/4, x=X(e);
    ctx.strokeStyle='#f3f3f3'; ctx.beginPath(); ctx.moveTo(x,padT); ctx.lineTo(x,H-padB); ctx.stroke();
    ctx.fillStyle='#999'; ctx.fillText(maxE>=4 ? String(Math.round(e)) : (Math.round(e*10)/10).toFixed(1), x, H-padB+5); }
  // axes
  ctx.strokeStyle='#ccc'; ctx.lineWidth=1; ctx.beginPath(); ctx.moveTo(padL,padT); ctx.lineTo(padL,H-padB); ctx.lineTo(W-padR,H-padB); ctx.stroke();
  // axis titles \u2014 centered, no arrows, clear of the % tick labels
  ctx.fillStyle='#888'; ctx.font='11px sans-serif';
  ctx.textAlign='center'; ctx.textBaseline='alphabetic'; ctx.fillText('engine-hours', (padL+W-padR)/2, H-6);
  ctx.save(); ctx.translate(13,(padT+H-padB)/2); ctx.rotate(-Math.PI/2); ctx.textAlign='center'; ctx.textBaseline='middle'; ctx.fillText('% of best-ever',0,0); ctx.restore();
  // lines \u2014 break across engine-hour gaps > GAP so missing data (e.g. engine on but alternator off) shows
  //         as a gap rather than a misleading slide toward 0. Firmware never commits 0-points (eligibility gate).
  // Break only across REAL data gaps, not normal point spacing (which grows when the firmware
  // decimates a long history). Threshold = 3x the median step between points (floor 3 engine-hours).
  let steps=[]; for(let i=1;i<pts.length;i++){ const d=pts[i].eng-pts[i-1].eng; if(d>0) steps.push(d); }
  steps.sort((a,b)=>a-b);
  const GAP = Math.max(3, (steps.length?steps[Math.floor(steps.length/2)]:1)*3);
  const line=(key,color,width)=>{ ctx.strokeStyle=color; ctx.lineWidth=width; ctx.beginPath();
    let pen=false, prevE=0;
    pts.forEach(p=>{ const x=X(p.eng), y=Y(Math.max(minY,Math.min(maxY,p[key])));
      if(pen && (p.eng-prevE)<=GAP) ctx.lineTo(x,y); else ctx.moveTo(x,y); pen=true; prevE=p.eng; }); ctx.stroke(); };
  line('overall','rgba(58,123,213,0.35)',1.5);   // faint overall
  line('worst','#3a7bd5',2.2);                    // bold worst-region
  const last = pts[pts.length-1];
  if (last.live){ const x=X(last.eng), y=Y(Math.max(minY,Math.min(maxY,last.worst)));
    ctx.beginPath(); ctx.arc(x,y,5,0,6.2832);
    ctx.fillStyle = ALT_STATE_COLOR[liveSt] || '#5cb85c'; ctx.fill();   // state color (MEASURED green / ESTIMATED blue)
    ctx.strokeStyle='#fff'; ctx.lineWidth=2; ctx.stroke(); }
}

// ── Charging-system health session plot: every 1 Hz live % since this dashboard session began,
//    uPlot-styled like the Plots tab. Points colored by confidence state (MEASURED green,
//    ESTIMATED blue). LEARNING/NO-REFERENCE periods carry NO value — they render as gaps with a
//    light background tint (the suppressed number is deliberately never plotted). ──
let altSessPlot = null;
const ALT_SESS_MAX = 28800;   // 8 h of 1 Hz samples, then the oldest roll off
const ALT_SESS_WINDOW_S = 1800;  // X axis shows a fixed last-30-minutes window, labelled "minutes ago"
let altSessT = [], altSessMeas = [], altSessEst = [], altSessGap = [];
let altSessYManual = null;
try { altSessYManual = JSON.parse(localStorage.getItem('altSessY') || 'null'); } catch(e){}

function altSessYRange(){
  if (altSessYManual) return altSessYManual;
  let lo = Infinity, hi = -Infinity;
  const scan = a => { for (const v of a) if (v != null) { if (v < lo) lo = v; if (v > hi) hi = v; } };
  scan(altSessMeas); scan(altSessEst);
  if (!isFinite(lo)) return [80, 110];
  const pad = Math.max(2, (hi - lo) * 0.15);
  return [Math.floor(lo - pad), Math.ceil(hi + pad)];
}

function buildAltSessPlot(){
  const el = document.getElementById('alt-session-plot');
  if (!el || typeof uPlot === 'undefined') return;
  el.innerHTML = '';
  const opts = {
    width: Math.max(el.clientWidth, 320),
    height: 200,
    series: [
      { label: 'Time' },
      { label: 'MEASURED %',  stroke: 'transparent', paths: () => null,
        points: { show: true, size: 5, width: 0, fill: '#5cb85c' } },
      { label: 'ESTIMATED %', stroke: 'transparent', paths: () => null,
        points: { show: true, size: 5, width: 0, fill: '#3a7bd5' } },
    ],
    scales: {
      x: { time: false, range: () => { const now = Date.now() / 1000; return [now - ALT_SESS_WINDOW_S, now]; } },
      y: { auto: false, range: () => altSessYRange() }
    },
    axes: [
      // Fixed last-30-minutes window labelled "minutes ago" — a static axis that doesn't crawl like a clock.
      { grid: { show: true },
        splits: () => { const now = Date.now() / 1000, a = []; for (let m = 30; m >= 0; m -= 5) a.push(now - m * 60); return a; },
        values: (u, sp) => { const now = Date.now() / 1000; return sp.map(t => { const m = Math.round((now - t) / 60); return m === 0 ? 'now' : m + 'm'; }); } },
      { scale: 'y', grid: { show: true }, side: 3, values: (u, t) => t.map(v => v + '%') },
    ],
    legend: { show: false },
    cursor: { drag: { x: false, y: false } },
    hooks: { draw: [u => {
      // light tint over spans where the % was suppressed (learning / no reference / not running)
      const ctx = u.ctx; ctx.save();
      ctx.beginPath(); ctx.rect(u.bbox.left, u.bbox.top, u.bbox.width, u.bbox.height); ctx.clip();  // keep tint inside the plot when old points fall left of the 30-min window
      ctx.fillStyle = 'rgba(150,150,150,0.10)';
      let runStart = null;
      for (let i = 0; i < altSessGap.length; i++) {
        const gap = altSessGap[i] === 1;
        if (gap && runStart === null) runStart = altSessT[i];
        if ((!gap || i === altSessGap.length - 1) && runStart !== null) {
          const x0 = u.valToPos(runStart, 'x', true);
          const x1 = u.valToPos(altSessT[i], 'x', true);
          ctx.fillRect(x0, u.bbox.top, Math.max(x1 - x0, 1), u.bbox.height);
          runStart = null;
        }
      }
      ctx.restore();
    }] }
  };
  altSessPlot = new uPlot(opts, [altSessT, altSessMeas, altSessEst], el);
  new ResizeObserver(() => {
    if (altSessPlot) altSessPlot.setSize({ width: Math.max(el.clientWidth, 320), height: 200 });
  }).observe(el);
  attachYAxisEdit(altSessPlot, [{
    scale: 'y', decimals: 0,
    apply: (mn, mx) => { altSessYManual = [mn, mx]; localStorage.setItem('altSessY', JSON.stringify(altSessYManual)); altSessPlot.redraw(); },
    auto:  ()       => { altSessYManual = null;     localStorage.removeItem('altSessY');                              altSessPlot.redraw(); }
  }]);
}

function altSessionPush(){
  const st = altLive.state|0;
  const graded = altLive.valid && (st===0 || st===1) && altLive.pct>0;
  altSessT.push(Date.now()/1000);
  altSessMeas.push(graded && st===0 ? altLive.pct : null);
  altSessEst.push (graded && st===1 ? altLive.pct : null);
  altSessGap.push(graded ? 0 : 1);
  if (altSessT.length > ALT_SESS_MAX){ altSessT.shift(); altSessMeas.shift(); altSessEst.shift(); altSessGap.shift(); }
  if (!altSessPlot) buildAltSessPlot();
  if (altSessPlot) altSessPlot.setData([altSessT, altSessMeas, altSessEst]);
}

// ── Boat performance (Phase 3): schema-driven live + settings, sailing polar plot ──
// PILOT of the self-describing telemetry pattern: fetch /perfschema ONCE and zip the
// returned field names against the PerfLive / PerfSettings payloads — no hardcoded array,
// so this can't fall out of sync with the firmware tables.
let perfSchema = null;
let perfLive = { valid:false, pct:0, spd:0, wa:0, ws:0, best:0, pitchStd:0, src:0,
                 coverage:0, ptCount:0, source:0, paused:0, sim:0, state:3 };
let perfSettings = {};
let perfCells = [];
let _perfPlotPending = false, _perfModelLastFetch = 0;

function fetchPerfSchema(){ return fetch('/perfschema').then(r=>r.json()).then(j=>{ perfSchema=j; }).catch(()=>{}); }

function perfSet(key, val){
  if(!currentAdminPassword){ alert('Please unlock settings first'); return; }
  fetchWithTimeout(buildURL('/get?password='+encodeURIComponent(currentAdminPassword)+'&'+key+'='+val),{},5000).catch(()=>{});
}

let perfView = 0;   // 0 = sailing, 1 = motoring (client-side display toggle)
function setTxt(id, t){ const el=document.getElementById(id); if(el) el.textContent=t; }
function seaWord(s){ return s<0.5?'calm':s<1.5?'slight':s<3?'choppy':s<5?'rough':'very rough'; }
function setPerfView(v){
  perfView = v;
  const a=document.getElementById('perf-view-sail'), b=document.getElementById('perf-view-motor');
  if(a) a.classList.toggle('cap-mode-active', v===0);
  if(b) b.classList.toggle('cap-mode-active', v===1);
  const pp=document.getElementById('perf-plot'), mp=document.getElementById('motor-plot');
  if(pp) pp.style.display = v===0?'block':'none';
  if(mp) mp.style.display = v===1?'block':'none';
  // Speed hub: centered in the sailing polar (open hub); upper-left on the motoring map (clears the rising curve)
  const hub=document.getElementById('perf-speed-hub');
  if(hub){
    if(v===1){ hub.style.left='9%'; hub.style.top='11%'; hub.style.transform='none'; hub.style.textAlign='left'; }
    else { hub.style.left='50%'; hub.style.top='54%'; hub.style.transform='translate(-50%,-50%)'; hub.style.textAlign='center'; }
  }
  const symRow=document.getElementById('perf-sym-row'); if(symRow) symRow.style.display = v===0?'':'none';
  const windTog=document.getElementById('perf-wind-toggle'); if(windTog) windTog.style.display = v===0?'':'none';
  renderPerf();
  if(v===0) queuePerfPlotUpdate(); else queueMotorPlotUpdate();
}
// Unified gauge/conditions updater — reads the active view's live object.
function renderPerf(){
  const L = perfView ? motorLive : perfLive;
  // Center-hub live speed — the dominant readable number on the plot (SOG sailing / STW motoring)
  setTxt('perf-speed-num', (L.valid && typeof L.spd === 'number') ? L.spd.toFixed(1) : '—');
  setTxt('perf-speed-unit', perfView ? 'KT · STW' : 'KT · SOG');
  // % vs best-ever (NO clamp) — shown only in the graded confidence states (MEASURED/ESTIMATED);
  // LEARNING/NO-REFERENCE deliberately show no number, just the state label. Same engine + labels
  // as the charging-system panel (ALT_STATE_LABEL/ALT_STATE_COLOR).
  const pst = (L.state==null) ? 3 : L.state|0;
  const pGraded = L.valid && (pst===0 || pst===1) && L.pct>0;
  setTxt('perf-pct', pGraded ? Math.round(L.pct)+'%' : '—');
  const pctEl = document.getElementById('perf-pct');
  if (pctEl) pctEl.style.color = pGraded ? ALT_STATE_COLOR[pst] : '';
  const pstEl = document.getElementById('perf-state');
  if (pstEl){ pstEl.textContent = L.valid ? (ALT_STATE_LABEL[pst]||'') : ''; pstEl.style.color = ALT_STATE_COLOR[pst]||'#888'; }
  // data maturity (bottom-right quadrant): learned front points + hours spent moving in this mode
  setTxt('perf-pts', (L.ptCount|0));
  const hrs = perfView ? (motorLive.motorHours||0) : (perfLive.sailHours||0);
  setTxt('perf-hours-val', hrs>=10 ? Math.round(hrs) : hrs.toFixed(1));
  setTxt('perf-hours-label', perfView ? 'motoring hours' : 'sailing hours');
  setTxt('perf-legend-curve', perfView ? 'best curve' : 'best polar');
  // flanking conditions
  if(perfView){
    setTxt('perf-condL-label','Engine RPM');
    setTxt('perf-condL-val', L.valid ? Math.round(L.rpm) : '—');
    const hwv = L.headwind||0;   // fore-aft apparent wind: + = headwind, - = tailwind; show magnitude + word, never a negative
    setTxt('perf-condL-sub', Math.round(Math.abs(hwv))+(hwv<0?' kt tailwind':' kt headwind'));
  } else {
    setTxt('perf-condL-label','Apparent wind');
    setTxt('perf-condL-val', L.valid ? Math.round(L.ws||0)+' kt' : '—');
    setTxt('perf-condL-sub', Math.round(L.wa||0)+'°');
  }
  const sea=L.pitchStd||0;
  setTxt('perf-sea-val', sea.toFixed(1)+'°'); setTxt('perf-sea-sub', seaWord(sea));
  // steady-state is now shown by the "now" dot itself (solid = banking data, hollow = not steady) — see drawPerfPlot/drawMotorPlot
  // Learning switch (Live Data) + simulator/reference segmented toggles (Settings → Vessel Performance).
  // All driven by perfLive, which always carries paused+sim+source.
  const paused=perfLive.paused===1;
  const sw=document.getElementById('perf-learn-switch'); if(sw) sw.checked = !paused;
  const lbl=document.getElementById('perf-learn-label');
  if(lbl){ lbl.textContent = paused?'(paused)':'(active)'; lbl.style.color = paused?'#d9534f':'#5cb85c'; }
  setSeg(['perf-sim-off','perf-sim-on'], perfLive.sim===1?1:0);
  setSeg(['perf-ref-0','perf-ref-1'], (L.source>=1)?1:0);
}
function updatePerf(){ renderPerf(); }
function updateMotor(){ renderPerf(); }
function setSeg(ids, activeIdx){
  ids.forEach((id,i)=>{ const el=document.getElementById(id); if(el) el.classList.toggle('cap-mode-active', i===activeIdx); });
}
function updatePerfControls(){
  const s=perfSettings;
  if(s.perfSpeedSrc!=null) setSeg(['perf-src-1','perf-src-2'], (s.perfSpeedSrc>=1.5)?1:0);   // <1.5 Water, ≥1.5 GPS (matches firmware threshold; never blank)
  if(s.perfFoldSymmetric!=null) setSeg(['perf-sym-1','perf-sym-0'], (s.perfFoldSymmetric>=0.5)?0:1);
}
// Switching STW↔SOG invalidates every learned point → the firmware does a Clear-All. Warn first.
function perfSetSpeedSrc(v){
  if(!currentAdminPassword){ alert('Please unlock settings first'); return; }
  if((perfSettings.perfSpeedSrc|0) === v) return;
  if(!confirm('Switching speed source (STW ↔ SOG) clears ALL learned boat-performance data — same as Clear All. Continue?')) return;
  perfSet('perfSpeedSrc', v);
}
// LEARNED↔FIXED reference toggle.
function perfSetSource(src){ if(!currentAdminPassword){ alert('Please unlock settings first'); return; } perfSet('perfSource', src?1:0); }
// Import a boat polar from a file (a shared BEFRONT1 CSV, or a Download-CSV backup). Opens the
// native file picker; perfUploadCsvFile() reads it and POSTs the raw text to /perfUploadFront.
function perfLoadCsv(){
  if(!currentAdminPassword){ alert('Please unlock settings first'); return; }
  const inp=document.getElementById('perf-load-file'); if(inp){ inp.value=''; inp.click(); }   // freeze/learn is asked after the file is chosen
}
function perfUploadCsvFile(inp){
  const f=inp.files && inp.files[0]; if(!f) return;
  // Ask how to use the imported polar, right after the file is chosen: freeze (hold as-is) vs learn (refine from it).
  const freeze = confirm('Import "'+f.name+'"\n\nHow should this polar be used?\n\nOK = FREEZE — hold it exactly as imported (FIXED, learning paused).\nCancel = LEARN — use it as a starting point; your own sailing keeps refining it (LEARNED).');
  const rd=new FileReader();
  rd.onload=function(){
    const txt=String(rd.result||'');
    if(txt.indexOf('BEFRONT1')<0){ alert('That file is not a boat polar (BEFRONT1) CSV.'); inp.value=''; return; }
    fetchWithTimeout(buildURL('/perfUploadFront?password='+encodeURIComponent(currentAdminPassword)+'&fixed='+(freeze?1:0)),{method:'POST',body:txt},10000)
      .then(r=>{ if(r.ok){ alert('Polar imported — '+(freeze?'FIXED (learning paused)':'LEARNED (still refining from your sailing)')+'. The plot will refresh.'); fetchPerfCurve(); }
                 else { r.text().then(t=>alert('Import failed: '+(t||('HTTP '+r.status)))).catch(()=>alert('Import failed: HTTP '+r.status)); } })
      .catch(e=>alert('Import failed: '+(e&&e.message?e.message:e)));
    inp.value='';
  };
  rd.onerror=function(){ alert('Could not read that file.'); inp.value=''; };
  rd.readAsText(f);
}

// ── v2 curve-driven render: the polar is the cloud-fitted curve sliced at the CURRENT wind speed +
//    sea state, with faint record dots over it. (Bin scatter retired.) ──
// ── Boat-performance plots fed by the raw best-ever FRONT (BEFRONT1) the
// firmware serves at /perfcurve.csv. We render directly from the front using
// the SAME inverse-distance eval the firmware uses for the live "% of best"
// (FrontStore::eval), so the plotted line always agrees with the gauge.
// Each point: {x:[axis0, axis1, sea], y:speed, n:nSamp}.
let sailFrontPts = [], motorFrontPts = [], perfRecs = [];
let _perfCurveLastFetch = 0, _perfRecLastFetch = 0;
// Per-axis distance normalization — MUST match the firmware axisScale[] (7_functions.ino).
const SAIL_SCALE = [2.0, 12.0, 1.0];     // AWS, AWA, sea
const MOTOR_SCALE = [100.0, 2.0, 1.0];   // RPM, headwind, sea
function perfIdw(){ return (perfSettings.perfIdwPower != null && perfSettings.perfIdwPower > 0) ? perfSettings.perfIdwPower : 2.0; }

// IDW surface eval — mirrors FrontStore::eval: dᵢ=√Σ((x-pt)/scale)²; exact hit→y; else Σwᵢyᵢ/Σwᵢ, w=1/(dᵢ^p+eps).
function frontEval(pts, q, scale){
  if(!pts.length) return 0;
  const p=perfIdw(); let wsum=0,num=0;
  for(const pt of pts){
    let d2=0;
    for(let a=0;a<q.length;a++){ const sc=scale[a]>1e-9?scale[a]:1; const dx=(q[a]-pt.x[a])/sc; d2+=dx*dx; }
    if(d2<1e-12) return pt.y;
    const dp = (p===2) ? d2 : Math.pow(Math.sqrt(d2), p);
    const w = 1/(dp+1e-9); wsum+=w; num+=w*pt.y;
  }
  return wsum>0 ? num/wsum : 0;
}
// Fold |AWA| to [0,180] when symmetric — mirrors perfFoldAwa (the front is stored folded only when symmetric).
function foldAwaJS(a){
  if(!(perfSettings.perfFoldSymmetric>=0.5)) return a;
  let pa=a; while(pa<0)pa+=360; while(pa>=360)pa-=360; if(pa>180)pa=360-pa; return pa;
}
// Apparent→true wind: standard transform from (AWS, AWA, boat speed). Returns {twa,tws}; TWA keeps AWA's side.
function appToTrue(awa, bs, aws){
  const r=Math.PI/180, ca=Math.cos(Math.abs(awa)*r);
  const tws=Math.sqrt(Math.max(0, aws*aws + bs*bs - 2*aws*bs*ca));
  let twa=0; if(tws>1e-6){ let c=(aws*ca - bs)/tws; c=Math.max(-1,Math.min(1,c)); twa=Math.acos(c)/r; }
  return { twa: (awa<0?-twa:twa), tws };
}
// Polar display frame: 0 = apparent wind (the front's native frame), 1 = true wind (display-layer transform).
let perfWindRef = 0;
function setPerfWindRef(v){
  perfWindRef = v?1:0;
  const a=document.getElementById('perf-wind-0'), b=document.getElementById('perf-wind-1');
  if(a) a.classList.toggle('cap-mode-active', perfWindRef===0);
  if(b) b.classList.toggle('cap-mode-active', perfWindRef===1);
  drawPerfPlot();
}

// Parse the BEFRONT1 sail+motor pair. Header: BEFRONT1,<SAIL|MOTOR>,<naxis>,<source>,<unit…>;
// rows: x0..x(naxis-1),y,nSamp,tEmit. (sail x=[aws,awa,sea]; motor x=[rpm,headwind,sea]; y=boat speed.)
function parseFrontCsv(txt){
  const sail=[], motor=[]; let cur=null, naxis=3;
  txt.split('\n').forEach(line=>{
    const t=line.trim(); if(!t) return;
    const c=t.split(',');
    if(c[0]==='BEFRONT1'){ cur=(c[1]==='MOTOR')?motor:sail; naxis=(+c[2])||3; return; }
    if(!cur || c.length < naxis+1) return;
    const x=[]; for(let a=0;a<naxis;a++) x.push(+c[a]);
    const y=+c[naxis], n=+c[naxis+1]||1;
    if(x.some(Number.isNaN) || Number.isNaN(y)) return;
    cur.push({x:x, y:y, n:n});
  });
  sailFrontPts=sail; motorFrontPts=motor;
}
function fetchPerfCurve(){ fetch('/perfcurve.csv').then(r=>r.text()).then(txt=>{ parseFrontCsv(txt); drawPerfPlot(); drawMotorPlot(); }).catch(()=>{}); }
function fetchPerfRecords(){ fetch('/perfrecords.csv').then(r=>r.text()).then(txt=>{ const ln=txt.trim().split('\n'); perfRecs=[]; for(let i=1;i<ln.length;i++){ const c=ln[i].split(','); if(c.length<8)continue; perfRecs.push({mode:+c[0],tws:+c[1],twa:+c[2],chop:+c[3],rpm:+c[4],hw:+c[5],stw:+c[6],sog:+c[7]}); } }).catch(()=>{}); }
function queuePerfPlotUpdate(){
  const now=Date.now();
  if(now-_perfCurveLastFetch>8000){ _perfCurveLastFetch=now; fetchPerfCurve(); }
  if(now-_perfRecLastFetch>15000){ _perfRecLastFetch=now; fetchPerfRecords(); }
  if(_perfPlotPending) return;
  _perfPlotPending=true;
  requestAnimationFrame(()=>{ _perfPlotPending=false; drawPerfPlot(); });
}
function drawPerfPlot(){
  const cv=document.getElementById('perf-plot'); if(!cv||!cv.getContext) return;
  const W=360, H=360, ctx=hidpiCtx(cv, W, H); ctx.clearRect(0,0,W,H);
  const cx=W/2, cy=H*0.54, R=Math.min(W,H)*0.42;
  const aws=perfLive.ws||0, sea=perfLive.pitchStd||0;
  const symFold = (perfSettings.perfFoldSymmetric==null) ? true : (perfSettings.perfFoldSymmetric>=0.5);
  const trueWind = (perfWindRef===1);
  const haveData = sailFrontPts.length >= 2;

  // Build the best-ever line: sweep the apparent-wind-angle range we've actually
  // sailed, eval the front at the CURRENT wind speed + sea, and (optionally) map
  // each point to true-wind coordinates for display. Points: [plotAngle, speed].
  let curve=[], maxS=4;
  if(haveData){
    let lo=999, hi=-999; sailFrontPts.forEach(p=>{ if(p.x[1]<lo)lo=p.x[1]; if(p.x[1]>hi)hi=p.x[1]; });
    const step=Math.max(1, (hi-lo)/90);
    for(let awa=lo; awa<=hi+1e-6; awa+=step){
      const s=frontEval(sailFrontPts, [aws, foldAwaJS(awa), sea], SAIL_SCALE);
      if(s<=0) continue;
      const ang = trueWind ? appToTrue(awa, s, aws).twa : awa;
      curve.push([ang, s]); if(s>maxS) maxS=s;
    }
  }
  // present point (apparent → true if toggled)
  let nowPt=null;
  if(perfLive.valid){
    const ang = trueWind ? appToTrue(perfLive.wa, perfLive.spd, aws).twa : perfLive.wa;
    nowPt=[ang, perfLive.spd]; if(perfLive.spd>maxS) maxS=perfLive.spd;
  }
  maxS=Math.ceil(maxS);
  const P=(ang,s)=>{ const r=(s/maxS)*R, a=ang*Math.PI/180; return [cx+r*Math.sin(a), cy-r*Math.cos(a)]; };

  // rings + spokes
  ctx.lineWidth=1; ctx.font='10px sans-serif';
  for(let k=1;k<=4;k++){ const r=R*k/4; ctx.strokeStyle='#ddd'; ctx.beginPath(); ctx.arc(cx,cy,r,0,6.2832); ctx.stroke();
    ctx.fillStyle='#aaa'; ctx.fillText(Math.round(maxS*k/4)+'kt', cx+3, cy-r+11); }
  ctx.strokeStyle='#eee';
  [0,45,90,135,180].forEach(d=>{ let p=P(d,maxS); ctx.beginPath(); ctx.moveTo(cx,cy); ctx.lineTo(p[0],p[1]); ctx.stroke();
    let q=P(-d,maxS); ctx.beginPath(); ctx.moveTo(cx,cy); ctx.lineTo(q[0],q[1]); ctx.stroke(); });
  ctx.fillStyle='#888'; ctx.fillText('0° into wind', cx-26, cy-R-3); ctx.fillText('180°', cx-11, cy+R+13);
  ctx.fillStyle='#aaa'; ctx.textAlign='center'; ctx.fillText(trueWind?'true wind':'apparent wind', cx, cy+R+26); ctx.textAlign='left';

  // best-ever line (bold), mirrored when symmetric
  if(haveData && curve.length>1){
    const drawLine=(sign)=>{ ctx.strokeStyle='#3a7bd5'; ctx.lineWidth=2.2; ctx.beginPath();
      curve.forEach((pt,i)=>{ const p=P(sign*pt[0], pt[1]); i===0?ctx.moveTo(p[0],p[1]):ctx.lineTo(p[0],p[1]); }); ctx.stroke(); };
    drawLine(1); if(symFold) drawLine(-1);
  } else { ctx.fillStyle='#999'; ctx.textAlign='center'; ctx.fillText('No sailing data yet', cx, cy); ctx.textAlign='left'; }

  // present point ("now") — solid green while a steady run is banking data into the front, hollow grey when not steady
  if(nowPt){
    let p=P(nowPt[0], nowPt[1]); ctx.beginPath(); ctx.arc(p[0],p[1],6,0,6.2832);
    if(perfLive.steady>=0.5){ ctx.fillStyle='#5cb85c'; ctx.fill(); ctx.strokeStyle='#fff'; ctx.lineWidth=2; ctx.stroke(); }
    else                    { ctx.fillStyle='#fff';    ctx.fill(); ctx.strokeStyle='#999'; ctx.lineWidth=2; ctx.stroke(); }
  }
}

// ── Motoring map (Phase 3): speed-vs-RPM, schema-driven (MotorLive) ──
let motorLive = { valid:false, rpm:0, headwind:0, spd:0, best:0, pct:0, src:0,
                  coverage:0, ptCount:0, source:0, paused:0, pitchStd:0, state:3 };
let motorCells = [];
let _motorPlotPending = false, _motorModelLastFetch = 0;

// updateMotor() is defined above as a thin wrapper → renderPerf() (unified card).
function fetchMotorModel(){ fetchPerfCurve(); }   // one curve covers both views
function queueMotorPlotUpdate(){
  const now=Date.now();
  if(now-_perfCurveLastFetch>8000){ _perfCurveLastFetch=now; fetchPerfCurve(); }
  if(now-_perfRecLastFetch>15000){ _perfRecLastFetch=now; fetchPerfRecords(); }
  if(_motorPlotPending) return;
  _motorPlotPending=true;
  requestAnimationFrame(()=>{ _motorPlotPending=false; drawMotorPlot(); });
}
function drawMotorPlot(){
  const cv=document.getElementById('motor-plot'); if(!cv||!cv.getContext) return;
  const W=520, H=300, pad=38, ctx=hidpiCtx(cv, W, H); ctx.clearRect(0,0,W,H);
  const hw=motorLive.headwind||0, sea=motorLive.pitchStd||0;
  const haveData = motorFrontPts.length >= 2;
  // best-ever line: sweep the RPM range we've actually motored, eval at current headwind + sea.
  let line=[], loR=0, hiR=0;
  if(haveData){
    loR=1e9; hiR=-1e9; motorFrontPts.forEach(p=>{ if(p.x[0]<loR)loR=p.x[0]; if(p.x[0]>hiR)hiR=p.x[0]; });
    const step=Math.max(20, (hiR-loR)/120);
    for(let r=loR; r<=hiR+1e-6; r+=step){ const v=frontEval(motorFrontPts,[r,hw,sea],MOTOR_SCALE); if(v>0) line.push([r,v]); }
  }
  let maxR=3600, maxS=4;
  line.forEach(p=>{ if(p[0]>maxR)maxR=p[0]; if(p[1]>maxS)maxS=p[1]; });
  if(motorLive.valid){ if(motorLive.rpm>maxR)maxR=motorLive.rpm; if(motorLive.spd>maxS)maxS=motorLive.spd; }
  maxR=Math.ceil(maxR/500)*500; maxS=Math.ceil(maxS);
  const X=r=>pad+(r/maxR)*(W-2*pad), Y=s=>H-pad-(s/maxS)*(H-2*pad);
  ctx.strokeStyle='#ccc'; ctx.lineWidth=1; ctx.beginPath(); ctx.moveTo(pad,pad); ctx.lineTo(pad,H-pad); ctx.lineTo(W-pad,H-pad); ctx.stroke();
  ctx.font='10px sans-serif';
  const rStep=Math.max(500, Math.ceil(maxR/6/500)*500);
  ctx.textAlign='center'; ctx.textBaseline='top';
  for(let r=0;r<=maxR+1;r+=rStep){ const x=X(r); ctx.strokeStyle='#eee'; ctx.beginPath(); ctx.moveTo(x,pad); ctx.lineTo(x,H-pad); ctx.stroke(); ctx.fillStyle='#999'; ctx.fillText(r,x,H-pad+5); }
  const sStep=Math.max(1, Math.ceil(maxS/5));
  ctx.textAlign='right'; ctx.textBaseline='middle';
  for(let s=0;s<=maxS+0.01;s+=sStep){ const y=Y(s); ctx.strokeStyle='#eee'; ctx.beginPath(); ctx.moveTo(pad,y); ctx.lineTo(W-pad,y); ctx.stroke(); ctx.fillStyle='#999'; ctx.fillText(s+'kt',pad-4,y); }
  ctx.fillStyle='#888'; ctx.font='11px sans-serif';
  ctx.textAlign='center'; ctx.textBaseline='alphabetic';
  ctx.fillText('RPM', W/2, H-pad+24);
  ctx.save(); ctx.translate(pad-26,H/2); ctx.rotate(-Math.PI/2); ctx.textAlign='center'; ctx.fillText('boat speed',0,0); ctx.restore();
  // best-ever speed-vs-RPM line at current headwind + sea state (clipped to the RPM range actually motored)
  if(haveData && line.length>1){
    ctx.strokeStyle='#3a7bd5'; ctx.lineWidth=2.2; ctx.beginPath();
    line.forEach((p,i)=>{ i===0?ctx.moveTo(X(p[0]),Y(p[1])):ctx.lineTo(X(p[0]),Y(p[1])); });
    ctx.stroke();
  } else { ctx.fillStyle='#999'; ctx.textAlign='center'; ctx.fillText('No motoring data yet',W/2,H/2); ctx.textAlign='left'; }
  // present point ("now") — solid green while a steady run is banking data into the front, hollow grey when not steady
  if(motorLive.valid){
    ctx.beginPath(); ctx.arc(X(motorLive.rpm),Y(motorLive.spd),6,0,6.2832);
    if(motorLive.steady>=0.5){ ctx.fillStyle='#5cb85c'; ctx.fill(); ctx.strokeStyle='#fff'; ctx.lineWidth=2; ctx.stroke(); }
    else                     { ctx.fillStyle='#fff';    ctx.fill(); ctx.strokeStyle='#999'; ctx.lineWidth=2; ctx.stroke(); }
  }
}
const CSV3_FIELDS = [
    "TemperatureLimitF",
    "BulkVoltage",
    "wavePeriod",
    "FloatVoltage",
    "SwitchingFrequency",
    "yyMin",
    "FieldAdjustmentInterval",
    "ManualDutyTarget",
    "SwitchControlOverride",
    "waveAmplitude",
    "CurrentThreshold",
    "PeukertExponent_scaled",
    "ChargeEfficiency_scaled",
    "ChargedVoltage_Scaled",
    "TailCurrent",
    "ChargedDetectionTime",
    "IgnoreTemperature",
    "bmsLogic",
    "bmsLogicLevelOff",
    "RPMScalingFactor",
    "MaximumAllowedBatteryAmps",
    "reserved_BatteryVoltageSource",  // obsolete setting removed — dead slot
    "AlternatorNominalAmps",
    "LearningUpStep",
    "LearningDownStep",
    "AmbientTempCorrectionFactor",
    "xTime",
    "MinLearningInterval",
    "SafeOperationThreshold",
    "PidKp",
    "PidKi",
    "PidKd",
    "PidSampleDivisor",
    "MaxTableValue",
    "MaxPenaltyPercent",
    "MaxPenaltyDuration",
    "NeighborLearningFactor",
    "yyMax",
    "LearningMemoryDuration",
    "EnableAmbientCorrection",
    "TuningMode",
    "rpmCurrentTable0",
    "rpmCurrentTable1",
    "rpmCurrentTable2",
    "rpmCurrentTable3",
    "rpmCurrentTable4",
    "rpmCurrentTable5",
    "rpmCurrentTable6",
    "rpmCurrentTable7",
    "rpmCurrentTable8",
    "rpmCurrentTable9",
    "ShuntResistanceMicroOhm",
    "InvertAltAmps",
    "InvertBattAmps",
    "MaxDuty",
    "MinDuty",
    "FieldResistance",
    "maxPoints",
    "AlternatorCOffset",
    "BatteryCOffset",
    "BatteryCapacity_Ah",
    "AmpSensorRange",
    "R_fixed",
    "Beta",
    "T0_C",
    "TempSource",
    "IgnitionOverride",
    "FLOAT_DURATION",
    "PulleyRatio",
    "BatteryCurrentSource",
    "rpmTableRPMPoints0",
    "rpmTableRPMPoints1",
    "rpmTableRPMPoints2",
    "rpmTableRPMPoints3",
    "rpmTableRPMPoints4",
    "rpmTableRPMPoints5",
    "rpmTableRPMPoints6",
    "rpmTableRPMPoints7",
    "rpmTableRPMPoints8",
    "rpmTableRPMPoints9",
    "LearningSettlingPeriod",
    "LearningRPMChangeThreshold",
    "LearningTempHysteresis",
    "fuelTableRPM0",
    "fuelTableRPM1",
    "fuelTableRPM2",
    "fuelTableRPM3",
    "fuelTableRPM4",
    "fuelTableRPM5",
    "fuelTableRPM6",
    "fuelTableRPM7",
    "fuelTableRPM8",
    "fuelTableRPM9",
    "fuelTableGPH0",
    "fuelTableGPH1",
    "fuelTableGPH2",
    "fuelTableGPH3",
    "fuelTableGPH4",
    "fuelTableGPH5",
    "fuelTableGPH6",
    "fuelTableGPH7",
    "fuelTableGPH8",
    "fuelTableGPH9",
    "stateRevision",
    "reserved_SetpointRampRate",  // obsolete setting removed — dead slot
    "DutyRampRate",
    "SettleTimeBeforeCut",
    "TempWarnExcess",
    "TempCritExcess",
    "TempSustainedTimeout",
    "AlternatorHardShutdownV",
    "VoltageDisagreeThreshold",
    "VoltageDisagreeTimeout",
    "rpmMinDutyTable0",
    "rpmMinDutyTable1",
    "rpmMinDutyTable2",
    "rpmMinDutyTable3",
    "rpmMinDutyTable4",
    "rpmMinDutyTable5",
    "rpmMinDutyTable6",
    "rpmMinDutyTable7",
    "rpmMinDutyTable8",
    "rpmMinDutyTable9",
    "rpmCapCurrentTable0",
    "rpmCapCurrentTable1",
    "rpmCapCurrentTable2",
    "rpmCapCurrentTable3",
    "rpmCapCurrentTable4",
    "rpmCapCurrentTable5",
    "rpmCapCurrentTable6",
    "rpmCapCurrentTable7",
    "rpmCapCurrentTable8",
    "rpmCapCurrentTable9",
    "VoltageKp",
    "VoltageLoopInterval",
    "FIELD_COLLAPSE_DELAY",
    "SetpointRiseRate",
    "SetpointFallRate",
    "PIDTrackingGain",
    "CAPSIZE_THRESHOLD_DEG",
    "PITCHPOLE_THRESHOLD_DEG",
    "SLAM_THRESHOLD_G",
    "imuMountOrientation",
    "TailCurrent_A",
    "RebulkVoltage",
    "rebulkDebounceTime",
    "MinFloatTime",
    "SOC_BlockRebulk_percent",
    "SOC_AllowRebulk_percent",
    "reserved_accelEnabled",  // RESERVED — was accelEnabled; accelerometer now always-on, no UI toggle
    "DutySlowRampRate",
    "ShutdownPhase2HoldMs",
    "TempPIDKp",
    "TempPIDKi",
    "ThermalLookaheadSec",
    "TempPIDIntervalMs",
    "TempPIDFilterAlpha",
    "VoltageKi",
    "rpmCapPowerTable0",
    "rpmCapPowerTable1",
    "rpmCapPowerTable2",
    "rpmCapPowerTable3",
    "rpmCapPowerTable4",
    "rpmCapPowerTable5",
    "rpmCapPowerTable6",
    "rpmCapPowerTable7",
    "rpmCapPowerTable8",
    "rpmCapPowerTable9",
    "reserved_VoltageTrimLimit",  // obsolete setting removed — dead slot
    "InputFilterTC",
    "SystemIDStepAmplitude",
    "HardOCTripAmps",
    "HardOCDebounceMs",
    "IExcessK",
    "IExcessN",
    "IExcessKBleed",
    "IgnoreRPM",
    "MinRPMForField",
    "AwBleedRate",
    "reserved_AwRecoverRate",          // RESERVED — was AwRecoverRate (hardcoded to 0.1 in firmware; free slot for future use)
    "KHard",                           // 183 (was 184; 183 reserved — was KSoft)
    "ReseedFrac",                      // shared across all four protections (was IExcessReseedFrac)
    "AwSeedProtectMs",
    "reserved187",                     // 187 reserved — was VoltageKd (D term removed)
    "displayTempUnit",
    "WarmupRampRate",                  // 189 (shifted -1 from prev)
    "OvGroup1Enable",                  // 190 (was 191; 190 reserved — was OvLayer1Enable)
    "OvGroup2Enable",
    "IExcessSigSrc",
    "IExcessMA_N",
    "OutputPIDSigSrc",
    "TdPred",                          // raw float (%.3f)
    "OvMeasMarginV",                   // raw float (%.3f)
    "OvPredMarginV",                   // raw float (%.3f)
    "OutputPIDMA_N",
    "OutputPIDFilterTC",
    "VoltageFilterTC",
    "reserved_ProtectionProxGateV",    // 202 reserved — variable removed 2026-05-22
    "SlopeBleedThresh",
    "SlopeBleedK",
    "DvdtTC",
    "SlopeBleedProxV",
    "StartupRiseRate",
    "absorptionCompleteTime",
    "OnOff",
    "ManualFieldToggle",
    "HiLow",
    "LimpHome",
    "AlarmActivate",
    "TempAlarm",
    "VoltageAlarmHigh",
    "VoltageAlarmLow",
    "CurrentAlarmHigh",
    "AlarmTest",
    "AlarmLatchEnabled",
    "MaintainMode",
    "ManualSOCPoint",
    "IgnoreLearningDuringPenalty",
    "LogAllLearningEvents",
    "CloudFeatures",
    "AutoShuntGainCorrection",
    "AutoAltCurrentZero",
    "WindingTempOffset",
    "ManualLifePercentage",
    "UVThresholdHigh",
    "weatherModeEnabled",
    "reserved_SENSOR_UPLOAD_INTERVAL",  // RESERVED — was SENSOR_UPLOAD_INTERVAL; now firmware-only constant (edit + reflash)
    "imuEnabled",
    "AbsorptionVoltage",
    "AbsorptionTimeoutMs",
    "bulkVoltageHoldMs",
    "capLimitMode",
    "TargetVoltageMode",
    "TargetVoltageSetpoint",
    "RebulkCurrent_A",
    "UseFloat",
    "altSpare0",
    "altSpare1",
    "altSpare2",
    "altSpare3",
    "TempAlarmLow",
    "LoadDumpDtThresh",
    "LoadDumpDtThresh1",
    "CVTuningMode",
    "cvWaveAmplitudeV",
    "cvWavePeriodSec",
    "cvKOvershoot",
    "cvConsecutiveReads",
    "ThermalTuningMode",
    "thermalWaveLowF",
    "thermalWaveHighF",
    "thermalWaveHalfPeriodMin",
    "thermalKOvershoot",
    "thermalKUndershoot",
    "thermalSettleThreshF",
    "thermalConsecutiveReads",
    "webgaugesinterval",
    "plotTimeWindow",
    "Ymin1",
    "Ymax1",
    "Ymin2",
    "Ymax2",
    "Ymin3",
    "Ymax3",
    "Ymin4",
    "Ymax4",
    "LoadDumpDtThresh3",
    "reserved_VMGUseTrueWind_c3",      // dead slot — Target-mode toggle removed (firmware sends 0); kept to preserve CSV3 indices
    "hardwarePresent",                 // moved from CSV2
    "testProtectionsEnabled",         // runtime flag — not persisted, resets true (enabled) on boot
    "IExcessArmMarginV",              // raw float (%.3f) — iExcess voltage gate margin
    "FastSetpointRiseRate",           // ×100, 1 decimal — multiplier on setpoint rise slew during post-protection recovery
    "FastSetpointRiseWindowMs",       // raw ms — hard upper bound on fast-rise window
    "FastSetpointRiseHeadroomV",      // ×100, 2 decimal — V below target at which fast-rise gate stays open
    "SolarWatts",                     // moved from CSV2
    "performanceRatio",               // moved from CSV2 (÷100 for display)
    "VeData",                         // moved from CSV2 (0/1)
    "NMEA0183Data",                   // moved from CSV2 (0/1)
    "NMEA2KData",                     // moved from CSV2 (0/1)
    "timeAxisModeChanging",           // moved from CSV2 (0/1)
    "gpsTimeSourceMode",              // 0=auto, 1=NMEA, 2=Phone, 3=NTP (time only)
    // Fast alt-current diagnostic knobs (Pattern B echo)
    "faEnabled",                     // 0/1 — global ON/OFF
    "faAlarmEnable",                 // 0/1 — FAULT drives audible alarm
    "faAnomPause",                   // 0/1 — freeze anomaly flipbook slots
    "faRpmEdgeMargin",               // RPM ×10
    "faAmpsDriftFloorA",             // A ×100
    "faAmpsDriftPct",                // percent ×10
    "faAttenUpAmps",                 // A ×10
    "faAttenDownAmps",               // A ×10
    "faPeakMinA",                    // A ×100
    "wifiNapEnabled",                // 0/1 — WiFi Napping standby toggle (Client only)
];
const TS_FIELDS = [
    "ts_HeadingNMEA",
    "ts_LatitudeNMEA",
    "ts_LongitudeNMEA",
    "ts_SatelliteCount",
    "ts_VictronVoltage",
    "ts_VictronCurrent",
    "ts_AlternatorTemp",
    "ts_ThermistorTemp",
    "ts_RPM",
    "ts_MeasuredAmps",
    "ts_BatteryV",
    "ts_IBV",
    "ts_Bcur",
    "ts_Channel3V",
    "ts_DutyCycle",
    "ts_FieldVolts",
    "ts_FieldAmps",
    "ts_CogNMEA",
    "ts_SogNMEA",
    "ts_AppWindSpeed",
    "ts_AppWindAngle",
    "ts_TrueWindSpeed",
    "ts_TrueWindAngle",
    "ts_Leeway",
    "ts_VMG",
    "ts_BaroPressure",
    "ts_AmbientTemp",
    "ts_IMU",
    "ts_VictronSolar",
    "ts_StwNMEA",
];

// Detect if running in Capacitor (iOS/Android) vs web browser
const IS_CAPACITOR = !!window.Capacitor;
const API_BASE_URL = IS_CAPACITOR ? 'http://alternator.local' : '';
// const API_BASE_URL = IS_CAPACITOR ? 'http://10.0.0.207' : ''; // worked first
// Alternative: Use mDNS hostname or fallback to IP
// const API_BASE_URL = IS_CAPACITOR ? 'http://192.168.4.1' : ''; // For AP mode
// const API_BASE_URL = IS_CAPACITOR ? 'http://alternator.local' : ''; // For Client mode with mDNS

// Supabase project — used by the silent log-relay (pollLogRequest) to fulfil an
// admin "pull" window. Public anon key (same one already shipped in firmware + Vercel).
// Device-LAN calls use buildURL(); these cloud calls use the absolute URL on all platforms.
const SUPABASE_URL = 'https://qnbekuaoweuteylitzvo.supabase.co';
const SUPABASE_ANON_KEY = 'eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJpc3MiOiJzdXBhYmFzZSIsInJlZiI6InFuYmVrdWFvd2V1dGV5bGl0enZvIiwicm9sZSI6ImFub24iLCJpYXQiOjE3NTE5NzY1MzUsImV4cCI6MjA2NzU1MjUzNX0.k2S_kzkdAyN1Azs_7enxLun9LouB1bA_q7Sw8x1Cp0o';

// ---------------------------------------------------------------
// Biometric (Face ID / Touch ID) admin-password autofill — iOS Capacitor only
// ---------------------------------------------------------------
// Admin password is stored in the iOS Keychain behind a biometric gate.
// Cold launch fires Face ID automatically; on success the password is
// filled and the existing /checkPassword flow runs, so the user lands on
// the unlocked dashboard without typing. Browser builds: full no-op.
const BIO_SERVER_KEY = 'xreg-admin';
const Bio = {
    plugin() {
        if (!IS_CAPACITOR) return null;
        const p = window.Capacitor.Plugins && window.Capacitor.Plugins.NativeBiometric;
        return p || null;
    },
    async available() {
        const p = this.plugin();
        if (!p) return false;
        try {
            const r = await p.isAvailable();
            return !!(r && r.isAvailable);
        } catch (e) { return false; }
    },
    async getSaved() {
        // Returns the saved password or null. Skips the Face ID prompt
        // entirely if nothing is saved yet — important for first-launch UX.
        const p = this.plugin();
        if (!p) return null;
        try {
            const saved = await p.isCredentialsSaved({ server: BIO_SERVER_KEY });
            if (!saved || !saved.isSaved) return null;
            await p.verifyIdentity({
                reason: 'Unlock regulator settings',
                title: 'X Regulator',
                subtitle: 'Authenticate to unlock settings'
            });
            const c = await p.getCredentials({ server: BIO_SERVER_KEY });
            return (c && c.password) ? c.password : null;
        } catch (e) { return null; }
    },
    async save(password) {
        const p = this.plugin();
        if (!p || !password) return;
        try {
            await p.setCredentials({
                username: 'admin',
                password: String(password),
                server: BIO_SERVER_KEY
            });
        } catch (e) { /* non-fatal */ }
    },
    async clear() {
        const p = this.plugin();
        if (!p) return;
        try { await p.deleteCredentials({ server: BIO_SERVER_KEY }); }
        catch (e) { /* non-fatal */ }
    }
};

// Flag tracks whether the in-flight /checkPassword attempt came from Face ID
// autofill, so the Wrong-Password branch can drop a stale Keychain entry.
let __bioAutofillInFlight = false;

async function tryBiometricUnlock() {
    if (!await Bio.available()) return;
    const pw = await Bio.getSaved();
    if (!pw) return;
    const input = document.getElementById('admin_password');
    if (!input || typeof setAdminPassword !== 'function') return;
    input.value = pw;
    __bioAutofillInFlight = true;
    setAdminPassword();
}

document.addEventListener('DOMContentLoaded', () => {
    // Defer one tick so other init code registers first.
    setTimeout(tryBiometricUnlock, 0);
});



if (typeof window.gpsManualOverride === 'undefined') {
    window.gpsManualOverride = false;
}

function buildURL(path) {// Helper function to build absolute URLs

    // Ensure path starts with /
    if (!path.startsWith('/')) {
        path = '/' + path;
    }
    return `${API_BASE_URL}${path}`;
}

// =====================================================================
// Phone GPS + time backup poster
// =====================================================================
// When the boat's NMEA2K GPS is unavailable (transducer fault, bus issue,
// or just not installed), the device falls back to phone-provided location
// and time. We POST every 60s; firmware ignores it if NMEA is fresh, uses
// it as a fallback otherwise. See /set_phone_data handler in 3_functions.ino
// and the priority resolution functions consumePhoneGps() / syncTimeFromPhone().

const PHONE_DATA_POST_INTERVAL_MS = 60000;  // every 60 sec when foregrounded
let phoneDataPosterTimer = null;

async function getPhoneLocation() {
    // Returns { latitude, longitude } or null. Works in both browser
    // (navigator.geolocation) and Capacitor (Geolocation plugin).
    if (IS_CAPACITOR && window.Capacitor.Plugins && window.Capacitor.Plugins.Geolocation) {
        try {
            const pos = await window.Capacitor.Plugins.Geolocation.getCurrentPosition({
                enableHighAccuracy: true,
                timeout: 10000,
                maximumAge: 60000
            });
            return { latitude: pos.coords.latitude, longitude: pos.coords.longitude };
        } catch (e) {
            console.warn('[PhoneGPS] Capacitor Geolocation failed:', e && e.message);
            return null;
        }
    }
    if (typeof navigator !== 'undefined' && navigator.geolocation) {
        // Browser geolocation requires a secure context (HTTPS / localhost). The
        // ESP32 serves over plain HTTP on the LAN, so a desktop browser will always
        // be refused — skip the call (and the guaranteed console error) outright.
        // Capacitor uses the native plugin branch above and isn't affected.
        if (typeof window !== 'undefined' && window.isSecureContext === false) {
            diagLog('[PhoneGPS] insecure origin — skipping browser geolocation');
            return null;
        }
        return new Promise(resolve => {
            navigator.geolocation.getCurrentPosition(
                pos => resolve({ latitude: pos.coords.latitude, longitude: pos.coords.longitude }),
                err => { diagWarn('[PhoneGPS] navigator.geolocation failed:', err && err.message); resolve(null); },
                { enableHighAccuracy: true, timeout: 10000, maximumAge: 60000 }
            );
        });
    }
    return null;  // no geolocation API available
}

async function postPhoneDataToDevice() {
    const pwField = document.querySelector('.password_field');
    const pw = pwField ? pwField.value : '';
    if (!pw) return;  // settings still locked; don't POST without auth

    const loc = await getPhoneLocation();
    const epochMs = Date.now();

    const params = new URLSearchParams();
    params.set('password', pw);
    params.set('epochMs', String(epochMs));
    if (loc) {
        params.set('lat', loc.latitude.toFixed(6));
        params.set('lon', loc.longitude.toFixed(6));
    }
    try {
        await fetch(buildURL('/set_phone_data') + '?' + params.toString(), { method: 'GET' });
    } catch (e) {
        // Device unreachable — fine, we'll try again on the next tick.
    }
}

function startPhoneDataPoster() {
    if (phoneDataPosterTimer) return;  // already running
    // Fire once after a short delay (let SSE establish first), then on interval.
    setTimeout(postPhoneDataToDevice, 5000);
    phoneDataPosterTimer = setInterval(postPhoneDataToDevice, PHONE_DATA_POST_INTERVAL_MS);
}

// Kick off poster on page load. Function is safe to call before unlock —
// it no-ops until the password field has a value.
document.addEventListener('DOMContentLoaded', startPhoneDataPoster);

function setupDemoPasswordHandler() {
    // Intercept the password form submission
    const passwordForm = document.querySelector('#settings-access-section form');
    const passwordInput = document.getElementById('admin_password');
    const lockStatus = document.getElementById('lock-status');

    if (passwordForm && passwordInput) {
        passwordForm.addEventListener('submit', function (e) {
            e.preventDefault(); // Stop normal form submission

            // Accept any non-empty password in demo mode
            if (passwordInput.value.trim() !== '') {
                // Simulate successful unlock
                if (lockStatus) {
                    lockStatus.textContent = "Settings are Unlocked";
                    lockStatus.className = "lock-status-unlocked";
                }

                // Hide password section
                const settingsAccess = document.getElementById('settings-access-section');
                if (settingsAccess) {
                    settingsAccess.style.display = 'none';
                }

                // Unlock settings section if it exists
                const settingsSection = document.getElementById('settings-section');
                if (settingsSection) {
                    settingsSection.classList.remove('locked');
                }

                console.log('[DEMO MODE] Password accepted (demo only)');
            }

            return false;
        });
    }
}

// ============================================================================
// CRITICAL MOBILE FIXES (Required for iOS)
// ============================================================================

// ReconnectButton
function manualReconnect() {
    sseReconnectAttempts = 0; // Reset attempts
    isAppInBackground = false;
    initializeEventSource();
    document.getElementById('reconnect-button').style.display = 'none';
    closeRecovery(); // Dismiss the Connection Lost dialog if user used the in-page button instead
}

// ============================================================================
// DIAGNOSTIC LOGGING SYSTEM
// ============================================================================
let SHOW_DIAGNOSTIC_MESSAGES = false; // Initial value (gets overridden below)

/**
 * Enable or disable diagnostic logging globally
 * @param {boolean} enabled - True to show diagnostic messages
 */
function setDiagnosticMode(enabled) {
    SHOW_DIAGNOSTIC_MESSAGES = enabled;
    console.log(`[DIAGNOSTIC MODE] ${enabled ? 'ENABLED' : 'DISABLED'}`);
}

/**
 * Check if diagnostic mode is currently enabled
 * @returns {boolean} Current diagnostic mode state
 */
function isDiagnosticMode() {
    return SHOW_DIAGNOSTIC_MESSAGES;
}

/**
 * Replacement for console.log - only logs if diagnostic mode enabled
 * @param {...any} args - Arguments to log
 */
function diagLog(...args) {
    if (SHOW_DIAGNOSTIC_MESSAGES) {
        console.log('[DIAG]', ...args);
    }
}

/**
 * Replacement for console.warn - only logs if diagnostic mode enabled
 * @param {...any} args - Arguments to log
 */
function diagWarn(...args) {
    if (SHOW_DIAGNOSTIC_MESSAGES) {
        console.warn('[DIAG WARN]', ...args);
    }
}

/**
 * Replacement for console.error - ALWAYS logs errors
 * In diagnostic mode, adds [DIAG ERROR] prefix
 * @param {...any} args - Arguments to log
 */
function diagError(...args) {
    if (SHOW_DIAGNOSTIC_MESSAGES) {
        console.error('[DIAG ERROR]', ...args);
    } else {
        console.error(...args);
    }
}

/**
 * Development logging - ALWAYS logs regardless of diagnostic mode
 * Use sparingly - only for critical debugging
 * @param {...any} args - Arguments to log
 */
function devLog(...args) {
    console.log('[DEV]', ...args);
}

// ============================================================================
// TOGGLE DIAGNOSTIC MODE HERE - SET TO false FOR APP STORE SUBMISSION
// ============================================================================
setDiagnosticMode(true); // ← CHANGE THIS LINE: true=ON, false=OFF
//What gets hidden when false:
//diagLog() - YES, hidden when false
//diagWarn() - YES, hidden when false
//diagError() - NO, always shows (even when false)


// ============================================================================
// END DIAGNOSTIC LOGGING SYSTEM
// ============================================================================


// 1. Fetch with timeout - prevents infinite hangs
function fetchWithTimeout(url, options = {}, timeout = 8000) {
    return Promise.race([
        fetch(url, options),
        new Promise((_, reject) =>
            setTrackedTimeout(() => reject(new Error('Request timeout')), timeout)
        )
    ]);
}



// 2. Cleanup function - prevents memory leaks

// Helper function to create tracked timers:
function setTrackedInterval(callback, delay) {
    const id = setInterval(callback, delay);
    activeTimers.push({ type: 'interval', id });
    return id;
}

function setTrackedTimeout(callback, delay) {
    const id = setTimeout(callback, delay);
    activeTimers.push({ type: 'timeout', id });
    return id;
}

function cleanupResources() {
    diagLog("Cleaning up resources");

    // Clear all tracked timers
    activeTimers.forEach(timer => {
        if (timer.type === 'interval') {
            clearInterval(timer.id);
        } else {
            clearTimeout(timer.id);
        }
    });
    activeTimers = [];

    // Close SSE connection
    if (source) {
        source.close();
        source = null;
    }

    // Clear SSE reconnection timer specifically
    if (sseReconnectTimer) {
        clearTimeout(sseReconnectTimer);
        sseReconnectTimer = null;
    }

    // Clear the adaptive log-relay poll timer (self-rescheduling, not in activeTimers)
    if (g_logPollTimer) {
        clearTimeout(g_logPollTimer);
        g_logPollTimer = null;
    }

    // Destroy plots
    if (typeof currentTempPlot !== 'undefined' && currentTempPlot) {
        currentTempPlot.destroy();
        currentTempPlot = null;
    }
    if (typeof voltagePlot !== 'undefined' && voltagePlot) {
        voltagePlot.destroy();
        voltagePlot = null;
    }
    if (typeof rpmPlot !== 'undefined' && rpmPlot) {
        rpmPlot.destroy();
        rpmPlot = null;
    }
    if (typeof temperaturePlot !== 'undefined' && temperaturePlot) {
        temperaturePlot.destroy();
        temperaturePlot = null;
    }

    // Clear data arrays (CRITICAL for memory management)
    if (typeof currentTempData !== 'undefined') currentTempData.length = 0;
    if (typeof voltageData !== 'undefined') voltageData.length = 0;
    if (typeof rpmData !== 'undefined') rpmData.length = 0;
    if (typeof temperatureData !== 'undefined') temperatureData.length = 0;
    // Add any other data arrays you have
}
// uplot plot minor details like dark mode and gentle space
function updateUplotTheme(plot) {
    if (!plot || !plot.root) return; // Safety check

    const textColor = getComputedStyle(document.body).getPropertyValue('--text-dark').trim();
    const gridColor = getComputedStyle(document.body).getPropertyValue('--border').trim();

    // Update axis label divs
    plot.root.querySelectorAll('.u-label').forEach(el => {
        el.style.color = textColor;
    });

    // Update tick label text (SVG)
    plot.root.querySelectorAll('.u-axis text').forEach(el => {
        el.setAttribute('fill', textColor);
    });

    // Update axis strokes (SVG)
    plot.root.querySelectorAll('.u-axis path').forEach(el => {
        el.setAttribute('stroke', textColor);
    });

    // Update grid lines (SVG)
    plot.root.querySelectorAll('.u-grid line').forEach(el => {
        el.setAttribute('stroke', gridColor);
    });
}

// 3. Cleanup on page unload
window.addEventListener('beforeunload', cleanupResources);

// 4. Handle app lifecycle (Capacitor-specific)
if (IS_CAPACITOR && window.Capacitor.Plugins && window.Capacitor.Plugins.App) {
    window.Capacitor.Plugins.App.addListener('appStateChange', ({ isActive }) => {
        if (isActive) {
            diagLog("App foregrounded");
            isAppInBackground = false;

            // Reset reconnection attempts when user returns
            sseReconnectAttempts = 0;

            // Only reconnect if connection is actually dead
            if (!source || source.readyState === EventSource.CLOSED) {
                diagLog("Connection was closed, reconnecting...");
                initializeEventSource();
            } else {
                diagLog("Connection still alive, resuming normal operation");
            }
        } else {
            diagLog("App backgrounded - keeping connection alive");
            isAppInBackground = true;

            // Stop auto-reconnect attempts while backgrounded
            if (sseReconnectTimer) {
                clearTimeout(sseReconnectTimer);
                sseReconnectTimer = null;
            }

            // DON'T call cleanupResources() - let SSE connection persist
            // iOS may close it after ~30 seconds, but we'll reconnect when foregrounded
        }
    });
}

// 5. EventSource initialization with reconnection
function initializeEventSource() {

    if (DEMO_MODE) {    // DEMO MODE: Skip real connection in demo mode
        return;
    }

    // Don't reconnect if app is backgrounded
    if (isAppInBackground) {
        diagLog("App backgrounded, skipping SSE reconnection");
        return;
    }
    // Check retry limit
    if (sseReconnectAttempts >= MAX_SSE_RECONNECTS) {
        diagLog("Max SSE reconnection attempts reached (10 attempts over 20 seconds). Manual reconnect required.");

        // Show the Connection Lost dialog (gives user the choice of full-reload retry
        // or "Continue Offline" which disables inputs). Hide the in-page reconnect-button
        // while the dialog is up to avoid two competing recovery affordances on screen.
        showRecoveryOptions();
        const reconnectBtn = document.getElementById('reconnect-button');
        if (reconnectBtn) {
            reconnectBtn.style.display = 'none';
        }

        return;
    }

    if (source) {
        source.close();
        source = null;
    }

    if (!window.EventSource) {
        diagError("EventSource not supported");
        return;
    }

    try {
        source = new EventSource(buildURL('/events'));

        source.addEventListener('open', function () {
            sseReconnectAttempts = 0; // Reset on successful connection
            updateInlineStatus(true);  // Flip indicator green on SSE connect
            closeRecovery();           // Dismiss Connection Lost dialog if it's open
            if (isOfflineMode) exitOfflineMode(); // Re-enable inputs if user had gone offline
        }, false);


        // \u2500\u2500 SSE: AltLive \u2500 charging-system health live + trend (schema-driven via /altschema) \u2500\u2500
        source.addEventListener('AltLive', function (e) {
            if (!altSchema || !altSchema.live) { if (!altSchema) fetchAltSchema(); return; }
            const v = e.data.split(',').map(Number);
            altSchema.live.forEach((k, i) => { altLive[k] = v[i]; });
            altLive.valid = altLive.valid === 1;
            altLive.steady = altLive.steady === 1;
            updateAltHealth();
            try { altSessionPush(); } catch (err) {}   // 1 Hz session plot feed
            queueAltTrendUpdate();
            updateCloudStatus();
        }, false);

        // \u2500\u2500 SSE: AltSettings \u2500 GUI-tunable values \u2192 echo spans (schema-driven) \u2500\u2500
        source.addEventListener('AltSettings', function (e) {
            if (!altSchema || !altSchema.settings) { if (!altSchema) fetchAltSchema(); return; }
            const v = e.data.split(',').map(Number);
            const ALT_NOECHO = { altPaused: 1 };   // driven by the LEARNED/FIXED toggle, no echo span
            altSchema.settings.forEach((k, i) => {
                altSettings[k] = v[i];
                if (ALT_NOECHO[k]) return;
                const el = document.getElementById(k + '_echo');
                if (el) el.textContent = (Math.round(v[i] * 1000) / 1000);
            });
        }, false);

        // \u2500\u2500 SSE: PerfLive / PerfSettings \u2500 schema-driven (names from /perfschema; no hardcoded array) \u2500\u2500
        source.addEventListener('PerfLive', function (e) {
            if (!perfSchema || !perfSchema.live) { if (!perfSchema) fetchPerfSchema(); return; }
            const v = e.data.split(',').map(Number);
            perfSchema.live.forEach((k, i) => { perfLive[k] = v[i]; });
            perfLive.valid = perfLive.valid === 1;
            updatePerf(); queuePerfPlotUpdate();
            updateCloudStatus();
        }, false);
        source.addEventListener('PerfSettings', function (e) {
            if (!perfSchema || !perfSchema.settings) { if (!perfSchema) fetchPerfSchema(); return; }
            const v = e.data.split(',').map(Number);
            const PERF_NOECHO = { perfFoldSymmetric: 1, perfSpeedSrc: 1, perfPaused: 1 };  // segmented/switch controls, no echo span
            perfSchema.settings.forEach((k, i) => {
                perfSettings[k] = v[i];
                if (PERF_NOECHO[k]) return;
                const el = document.getElementById(k + '_echo');
                if (el) el.textContent = (Math.round(v[i] * 1000) / 1000);
            });
            updatePerfControls(); queuePerfPlotUpdate(); queueMotorPlotUpdate();
        }, false);
        source.addEventListener('MotorLive', function (e) {
            if (!perfSchema || !perfSchema.motorLive) { if (!perfSchema) fetchPerfSchema(); return; }
            const v = e.data.split(',').map(Number);
            perfSchema.motorLive.forEach((k, i) => { motorLive[k] = v[i]; });
            motorLive.valid = motorLive.valid === 1;
            updateMotor(); queueMotorPlotUpdate();
        }, false);
        fetchPerfSchema();
        fetchAltSchema();

        source.addEventListener('error', function (e) {
            const state = this.readyState;
            let reason = '';

            if (state === EventSource.CONNECTING) {
                reason = 'Network hiccup or temporary stall (CONNECTING)';
            } else if (state === EventSource.CLOSED) {
                reason = 'Connection closed — likely WiFi drop, ESP32 reboot, or stalled SSE stream';
            } else {
                reason = 'Unknown SSE error state';
            }

            // ALWAYS visible — not suppressed by diagnostic mode
            console.error('[SSE ERROR]', reason, 'readyState=', state, 'event=', e);
            updateInlineStatus(false);  // Flip indicator red on any SSE error

            if (state === EventSource.CLOSED) {
                sseReconnectAttempts++;

                const backoffDelay = 2000; // Fixed 2 second retry interval

                // ALSO always visible
                console.error('[SSE RECONNECT]',
                    `Attempt ${sseReconnectAttempts}/${MAX_SSE_RECONNECTS}`,
                    `delay=${backoffDelay}ms`);

                if (sseReconnectTimer) {
                    clearTimeout(sseReconnectTimer);
                }

                sseReconnectTimer = setTrackedTimeout(() => {
                    initializeEventSource();
                }, backoffDelay);
            }
        }, false);

        // Re-bind stream listeners (CSVData/CSVData2/CSVData3/TimestampData/console)
        // on every reconnect. Without this, only the open/AltLive/AltSettings/error
        // listeners survive a reconnect and the main telemetry streams go silent.
        // window.attachStreamListeners is defined inside the window 'load' callback,
        // so it is undefined on the very first call to initializeEventSource() — the
        // initial attach is performed by the load callback itself once.
        if (typeof window.attachStreamListeners === 'function') {
            window.attachStreamListeners(source);
        }

    } catch (error) {
        diagError("Failed to create EventSource:", error);
    }
}

// ============================================================================
// DEMO MODE FOR APP STORE (No ESP32 hardware required)
// ============================================================================

function enableDemoMode() {
    if (DEMO_MODE) return; // Already enabled

    DEMO_MODE = true;
    console.log('[DEMO MODE] Enabled - Generating simulated data');

    // Demo feeds handleCSVData() directly (bypassing the CSVData listener that
    // normally hides the boat splash), so clear the splash here too.
    hideWaitingForRegulator();

    // Add visual demo banner. padding-top includes env(safe-area-inset-top) so
    // the warning isn't hidden under the iPhone notch / Dynamic Island.
    const banner = document.createElement('div');
    banner.id = 'demo-banner';
    banner.style.cssText = 'position:fixed;top:0;left:0;right:0;background:#ff9800;color:#000;padding:calc(10px + env(safe-area-inset-top)) 10px 10px;text-align:center;z-index:10000;font-weight:bold;font-size:14px;';
    banner.textContent = '⚠️ DEMO MODE - Simulated Data (No Hardware Connected)';
    document.body.insertBefore(banner, document.body.firstChild);

    // Adjust body padding to account for banner (also notch-aware).
    document.body.style.paddingTop = 'calc(40px + env(safe-area-inset-top))';

    // Start generating fake data
    startDemoData();
    // Handle password entry in demo mode
    setupDemoPasswordHandler();

}

function startDemoData() {
    // Send initial data immediately
    sendFakeCSVData();

    // Continue every 2 seconds (same as typical ESP32 update rate)
    demoInterval = setInterval(sendFakeCSVData, 2000);
}

function sendFakeCSVData() {
    window.lastEventTime = Date.now();

    if (window.sensorAges) {
        window.sensorAges.ibv = 0;
        window.sensorAges.soc = 0;
        window.sensorAges.measuredAmps = 0;
        window.sensorAges.bcur = 0;
        window.sensorAges.alternatorTemp = 0;
        window.sensorAges.rpm = 0;
    }

    if (typeof updateInlineStatus === 'function') {
        updateInlineStatus(true);
    }

    const values = new Array(50).fill(0);

    values[0] = 75 + Math.random() * 20;
    values[1] = 50 + Math.random() * 30;
    values[2] = 12.5 + Math.random() * 0.8;
    values[3] = 40 + Math.random() * 30;
    values[4] = 1800 + Math.random() * 600;
    values[21] = 0;
    values[22] = 1;
    values[25] = 2000;

    const csvHandler = window._csvDataHandler;
    if (csvHandler) {
        csvHandler({ data: values.join(',') });
    }
}


// (Removed: top-level "demo header values" block — used to overwrite the HTML
// defaults of "-" / "?" with random fake voltage/SOC/current/RPM + red "ON"
// ignition at script-load time, BEFORE the SSE connect or demo-mode decision.
// Cold-start showed ~10s of fake-looking real data. Real demo mode runs via
// sendFakeCSVData()+csvHandler path, which routes through normal CSV1 dispatch
// and updates the same headers correctly. Initial state now stays at "-" / "?".)


function checkForDemoMode() {
    // Wait 10 seconds after load, then check if ESP32 connected.
    // CONNECTING is intentionally NOT a trigger — slow-WiFi users can sit in
    // CONNECTING for many seconds, and tripping demo on that state pinned a
    // permanent demo banner over real data.
    setTimeout(() => {
        if (!source || source.readyState === EventSource.CLOSED) {
            console.log('[DEMO MODE] No ESP32 detected after 10 seconds - enabling demo mode');
            enableDemoMode();
        }
    }, 10000);
}

// ============================================================================
// END DEMO MODE
// ============================================================================


// ============================================================================
// END CRITICAL MOBILE FIXES
// ============================================================================


//Profiling system
// Call every 30 seconds to see performance trends
//setInterval(showPerformanceReport, 30000);
const ENABLE_DETAILED_PROFILING = true;
let performanceMetrics = {
    plotUpdates: [],
    domUpdates: [],
    echoUpdates: [],
    dataProcessing: []
};

// To add/remove Unix time labels on X axis.  No labels = cleaner render
// Time axis mode toggle
let useTimestamps = false; // Default to relative time (faster)
let timeAxisModeChanging = false; // Prevent conflicts during switch



function toggleTimeAxisMode(checkbox) {
    if (timeAxisModeChanging) return;

    timeAxisModeChanging = true;

    try {
        // Update the local variable immediately (don't wait for ESP32)
        useTimestamps = checkbox.checked;
        // Reinitialize data structures with new mode
        initPlotDataStructures();

        // Destroy and recreate all plots with new configuration
        if (currentTempPlot) { currentTempPlot.destroy(); initCurrentTempPlot(); }
        if (voltagePlot) { voltagePlot.destroy(); initVoltagePlot(); }
        if (rpmPlot) { rpmPlot.destroy(); initRPMPlot(); }
        if (temperaturePlot) { temperaturePlot.destroy(); initTemperaturePlot(); }

        // Reinitialize X-axis data for the new mode
        reinitializeXAxisForNewMode();
    } catch (error) {
        diagError("Error toggling time axis mode:", error);
    } finally {
        timeAxisModeChanging = false;
    }
}

//helper function
function reinitializeXAxisForNewMode() {
    // Get current interval from the last known data or use default
    const intervalMs = window._lastKnownInterval || 200; // fallback to 200ms
    const intervalSec = intervalMs / 1000;

    if (useTimestamps) {
        // SWITCHING TO TIMESTAMP MODE
        const now = Math.floor(Date.now() / 1000);

        // Initialize all plot X-axes with proper timestamps going back in time
        if (currentTempData && currentTempData[0]) {
            for (let i = 0; i < currentTempData[0].length; i++) {
                currentTempData[0][i] = now - (currentTempData[0].length - 1 - i) * intervalSec;
            }
        }

        if (voltageData && voltageData[0]) {
            for (let i = 0; i < voltageData[0].length; i++) {
                voltageData[0][i] = now - (voltageData[0].length - 1 - i) * intervalSec;
            }
        }

        if (rpmData && rpmData[0]) {
            for (let i = 0; i < rpmData[0].length; i++) {
                rpmData[0][i] = now - (rpmData[0].length - 1 - i) * intervalSec;
            }
        }

        if (temperatureData && temperatureData[0]) {
            for (let i = 0; i < temperatureData[0].length; i++) {
                temperatureData[0][i] = now - (temperatureData[0].length - 1 - i) * intervalSec;
            }
        }
    } else {
        // SWITCHING TO RELATIVE MODE
        // Initialize all plot X-axes with relative time (seconds ago)
        if (currentTempData && currentTempData[0]) {
            for (let i = 0; i < currentTempData[0].length; i++) {
                currentTempData[0][i] = -(currentTempData[0].length - 1 - i) * intervalSec;
            }
        }

        if (voltageData && voltageData[0]) {
            for (let i = 0; i < voltageData[0].length; i++) {
                voltageData[0][i] = -(voltageData[0].length - 1 - i) * intervalSec;
            }
        }

        if (rpmData && rpmData[0]) {
            for (let i = 0; i < rpmData[0].length; i++) {
                rpmData[0][i] = -(rpmData[0].length - 1 - i) * intervalSec;
            }
        }

        if (temperatureData && temperatureData[0]) {
            for (let i = 0; i < temperatureData[0].length; i++) {
                temperatureData[0][i] = -(temperatureData[0].length - 1 - i) * intervalSec;
            }
        }
    }
}



// Plot Rendering Tracker -
let plotRenderTracker = {
    interval: 10000, // 10 seconds
    startTime: performance.now(),
    lastReportTime: performance.now(),

    // Per-plot counters
    plots: {
        current: { count: 0, totalTime: 0, maxTime: 0 },
        voltage: { count: 0, totalTime: 0, maxTime: 0 },
        rpm: { count: 0, totalTime: 0, maxTime: 0 },
        temperature: { count: 0, totalTime: 0, maxTime: 0 }
    },

    // Overall stats
    totalRenderTime: 0,
    peakRenderTime: 0,
    dataPointsProcessed: 0,
    queueCalls: 0
};

// Plot Y-axis limits
let Ymin1 = -20, Ymax1 = 20; // Current plot
let Ymin2 = -20, Ymax2 = 20; // Voltage plot  
let Ymin3 = -20, Ymax3 = 20; // RPM plot
let Ymin4 = -20, Ymax4 = 20; // Temperature plot

// Circular buffer indices for efficiency
let currentTempIndex = 0;
let voltageIndex = 0;
let rpmIndex = 0;
let temperatureIndex = 0;

// Pre-calculated X-axis arrays
let xAxisData = [];

const LIVE_BUFFER_SEC = 300;   // short-term ring always holds 5 min so changing the X window never loses history
let liveWindowSec = 8;         // currently-visible X span (sec); driven by plotTimeWindow, applied via the x-scale range fn

// Efficient circular buffer structure - no timestamps needed
let currentTempData, voltageData, rpmData, temperatureData;

//from chatgpt: Every call to init*Plot() attaches a new ResizeObserver with a 1000 ms debounce. 
//But these functions are reentrant if called twice, and you don't unregister observers. This can stack up and cause redundant resizing logic at runtime.
//Fix: Store and disconnect previous observers before creating new ones.
let currentTempResizeObserver = null;
let voltageResizeObserver = null;
let rpmResizeObserver = null;
let temperatureResizeObserver = null;

let currentTempPlot;
let voltagePlot;
let rpmPlot;
let temperaturePlot;

// Autoscale state — computed at data rate, applied in rAF; persisted in localStorage
let autoScaleCurrent = localStorage.getItem('autoScaleCurrent') === 'true';
let autoScaleCurrentLocked = false;
let _autoScaleCurrentLeft  = null;  // { min, max }
let _autoScaleCurrentRight = null;

let autoScaleVoltage = localStorage.getItem('autoScaleVoltage') === 'true';
let autoScaleVoltageLocked = false;
let _autoScaleVoltageLeft  = null;
let _autoScaleVoltageRight = null;

let autoScaleRPM = localStorage.getItem('autoScaleRPM') === 'true';
let autoScaleRPMLocked = false;
let _autoScaleRPMLeft  = null;
let _autoScaleRPMRight = null;

let autoScaleTemp = localStorage.getItem('autoScaleTemp') === 'true';
let autoScaleTempLocked = false;
let _autoScaleTempLeft  = null;
let _autoScaleTempRight = null;


// Plot update batching system
const plotUpdateQueue = new Set();
let plotUpdateScheduled = false;


function queuePlotUpdate(plotName) {
    // Lightweight tracking - just increment counter
    plotRenderTracker.queueCalls++;

    plotUpdateQueue.add(plotName);

    if (!plotUpdateScheduled) {
        plotUpdateScheduled = true;
        requestAnimationFrame(() => {
            // Track render start
            const totalStart = performance.now();

            if (plotUpdateQueue.has('current') && currentTempPlot) {
                const start = performance.now();
                currentTempPlot.setData(currentTempData);
                const duration = performance.now() - start;

                // Update tracker (minimal overhead)
                const plot = plotRenderTracker.plots.current;
                plot.count++;
                plot.totalTime += duration;
                plot.maxTime = Math.max(plot.maxTime, duration);
            }

            if (plotUpdateQueue.has('voltage') && voltagePlot) {
                const start = performance.now();
                voltagePlot.setData(voltageData);
                const duration = performance.now() - start;

                const plot = plotRenderTracker.plots.voltage;
                plot.count++;
                plot.totalTime += duration;
                plot.maxTime = Math.max(plot.maxTime, duration);
            }

            if (plotUpdateQueue.has('rpm') && rpmPlot) {
                const start = performance.now();
                rpmPlot.setData(rpmData);
                const duration = performance.now() - start;

                const plot = plotRenderTracker.plots.rpm;
                plot.count++;
                plot.totalTime += duration;
                plot.maxTime = Math.max(plot.maxTime, duration);
            }

            if (plotUpdateQueue.has('temperature') && temperaturePlot) {
                const start = performance.now();
                temperaturePlot.setData(temperatureData);
                const duration = performance.now() - start;

                const plot = plotRenderTracker.plots.temperature;
                plot.count++;
                plot.totalTime += duration;
                plot.maxTime = Math.max(plot.maxTime, duration);
            }

            const totalDuration = performance.now() - totalStart;

            // Update overall tracker
            plotRenderTracker.totalRenderTime += totalDuration;
            plotRenderTracker.peakRenderTime = Math.max(plotRenderTracker.peakRenderTime, totalDuration);

            plotUpdateQueue.clear();
            plotUpdateScheduled = false;
        });
    }
}

// ── 60-fps interpolation ──────────────────────────────────────────────────
function lerp(a, b, t) {
    if (a === null || b === null) return b;
    return a + (b - a) * t;
}

const plotInterp = {
    current:     { prevY: [0, 0, 0, 0],                nextY: [0, 0, 0, 0],                arrivalTime: 0, lerpDuration: 200 },
    voltage:     { prevY: [0, 0, 0],                  nextY: [0, 0, 0],                  arrivalTime: 0, lerpDuration: 200 },
    rpm:         { prevY: [0, 0],                     nextY: [0, 0],                     arrivalTime: 0, lerpDuration: 200 },
    temperature: { prevY: [0, 0],                     nextY: [0, 0],                     arrivalTime: 0, lerpDuration: 200 },
    pid:         { prevY: [0, 0, 0, 0, 0, 0, 0],      nextY: [0, 0, 0, 0, 0, 0, 0],      arrivalTime: 0, lerpDuration: 200 },
    cv:          { prevY: [null, null, null, null],    nextY: [null, null, null, null],    arrivalTime: 0, lerpDuration: 200 },
};

let interpLoopRunning = false;

function startInterpLoop() {
    if (interpLoopRunning) return;
    interpLoopRunning = true;

    // ── Smoothness diagnostics ────────────────────────────────────────────
    const diag = {
        frameCount: 0,
        worstFrameGapMs: 0,   // largest gap between rAF calls — visible as a glitch when >33ms
        glitchFrames: 0,      // frames where gap >33ms (below 30fps)
        totalRenderMs: 0,     // cumulative setData time this period
        worstRenderMs: 0,     // single worst setData batch
        lerpDurationSum: 0,   // for computing average adaptive interval
        lastFrameTime: performance.now(),
        nextReportTime: performance.now() + 10000,
    };

    function applyInterp(state, dataArray, seriesCount) {
        if (state.arrivalTime === 0) return false;
        const elapsedMs = performance.now() - state.arrivalTime;
        const tLinear = Math.min(1, elapsedMs / state.lerpDuration);
        // Smoothstep easing: slow-start → fast-middle → slow-end.
        // Looks more organic than linear; also softens the "square step" on slow-updating sensors.
        const t = tLinear * tLinear * (3 - 2 * tLinear);
        const last = dataArray[1].length - 1;
        const elapsedSec = elapsedMs / 1000;

        // Smooth x-axis scrolling — eliminates discrete jump when y-data shifts at packet arrival.
        // Relative (seconds-ago) mode: reconstruct entire x-array each frame, offsetting left by elapsed.
        // Absolute (epoch) mode: pin both x[0] and x[last] so range width stays constant — prevents
        //   left-edge jump when the shift-buffer drops the oldest sample at each SSE arrival.
        if (dataArray[0][last] > 1e9) {
            const now = Date.now() / 1000;
            const intervalSec = (window._lastKnownInterval || 200) / 1000;
            dataArray[0][last] = now;
            dataArray[0][0]    = now - last * intervalSec;  // anchor left edge; prevents 4px tick snap at 5Hz
        } else {
            const intervalSec = (window._lastKnownInterval || 200) / 1000;
            for (let i = 0; i <= last; i++) {
                dataArray[0][i] = -(last - i) * intervalSec - elapsedSec;
            }
        }

        for (let s = 0; s < seriesCount; s++) {
            dataArray[s + 1][last] = lerp(state.prevY[s], state.nextY[s], t);
        }
        return true;
    }

    function plotsTabVisible() {
        const el = document.getElementById('plots');
        return el && el.classList.contains('active');
    }
    function tuningTabVisible() {
        const el = document.getElementById('tuning');
        return el && el.classList.contains('active');
    }

    function frame() {
        const frameStart = performance.now();

        // ── Frame timing ─────────────────────────────────────────────────
        const frameGap = frameStart - diag.lastFrameTime;
        diag.lastFrameTime = frameStart;
        diag.frameCount++;
        if (frameGap > diag.worstFrameGapMs) diag.worstFrameGapMs = frameGap;
        if (frameGap > 33) {
            diag.glitchFrames++;
            if (frameGap > 50) diagWarn(`[INTERP] Dropped frame: ${frameGap.toFixed(1)}ms gap`);
        }

        // ── Plot updates ──────────────────────────────────────────────────
        const renderStart = performance.now();

        if (plotsTabVisible()) {
            if (currentTempPlot && applyInterp(plotInterp.current, currentTempData, 4)) {
                if (autoScaleCurrent) {
                    if (_autoScaleCurrentLeft)  currentTempPlot.setScale('current', _autoScaleCurrentLeft);
                    if (_autoScaleCurrentRight) currentTempPlot.setScale('pct',     _autoScaleCurrentRight);
                }
                currentTempPlot.setData(currentTempData);
                plotRenderTracker.plots.current.count++;
            }
            if (voltagePlot && applyInterp(plotInterp.voltage, voltageData, 3)) {
                if (autoScaleVoltage) {
                    if (_autoScaleVoltageLeft)  voltagePlot.setScale('voltage', _autoScaleVoltageLeft);
                    if (_autoScaleVoltageRight) voltagePlot.setScale('pct',     _autoScaleVoltageRight);
                }
                voltagePlot.setData(voltageData);
                plotRenderTracker.plots.voltage.count++;
            }
            if (rpmPlot && applyInterp(plotInterp.rpm, rpmData, 2)) {
                if (autoScaleRPM) {
                    if (_autoScaleRPMLeft)  rpmPlot.setScale('rpm', _autoScaleRPMLeft);
                    if (_autoScaleRPMRight) rpmPlot.setScale('pct', _autoScaleRPMRight);
                }
                rpmPlot.setData(rpmData);
                plotRenderTracker.plots.rpm.count++;
            }
            if (temperaturePlot && applyInterp(plotInterp.temperature, temperatureData, 2)) {
                if (autoScaleTemp) {
                    if (_autoScaleTempLeft)  temperaturePlot.setScale('temperature', _autoScaleTempLeft);
                    if (_autoScaleTempRight) temperaturePlot.setScale('pct',         _autoScaleTempRight);
                }
                temperaturePlot.setData(temperatureData);
                plotRenderTracker.plots.temperature.count++;
            }
        }
        if (tuningTabVisible()) {
            if (pidTuningPlot && pidTuningData && applyInterp(plotInterp.pid, pidTuningData, 7)) {
                pidTuningPlot.setData(pidTuningData);
            }
            if (cvTuningPlot && cvTuningData && applyInterp(plotInterp.cv, cvTuningData, 4)) {
                cvTuningPlot.setData(cvTuningData);
            }
        }

        const renderMs = performance.now() - renderStart;
        diag.totalRenderMs += renderMs;
        if (renderMs > diag.worstRenderMs) diag.worstRenderMs = renderMs;
        diag.lerpDurationSum += plotInterp.current.lerpDuration;

        // ── Periodic smoothness report ────────────────────────────────────
        if (frameStart >= diag.nextReportTime && diag.frameCount > 0) {
            const periodMs = frameStart - (diag.nextReportTime - 10000);
            const fps = (diag.frameCount / (periodMs / 1000)).toFixed(1);
            const avgRender = (diag.totalRenderMs / diag.frameCount).toFixed(2);
            const avgLerp = (diag.lerpDurationSum / diag.frameCount).toFixed(0);
            diagLog(
                `[INTERP] fps:${fps} | worstGap:${diag.worstFrameGapMs.toFixed(1)}ms` +
                ` | render/frame:${avgRender}ms | worstRender:${diag.worstRenderMs.toFixed(1)}ms` +
                ` | glitches:${diag.glitchFrames} | avgLerp:${avgLerp}ms`
            );
            diag.frameCount = 0;
            diag.worstFrameGapMs = 0;
            diag.glitchFrames = 0;
            diag.totalRenderMs = 0;
            diag.worstRenderMs = 0;
            diag.lerpDurationSum = 0;
            diag.nextReportTime = frameStart + 10000;
        }

        requestAnimationFrame(frame);
    }

    requestAnimationFrame(frame);
}

//ota update stuff

// Software Update functionality
let availableVersions = {};

// Quick internet connectivity test (runs in browser)
// Uses same endpoint as ESP32 testInternetSpeed() for consistency
async function testInternetConnectivity() {
    try {
        const controller = new AbortController();
        const timeoutId = setTrackedTimeout(() => controller.abort(), 3000); // 3 second timeout

        // Cloudflare's trace endpoint over HTTPS (iOS WKWebView blocks mixed content)
        const response = await fetch('https://cloudflare.com/cdn-cgi/trace', {
            method: 'GET',
            signal: controller.signal,
            cache: 'no-store'
        });

        clearTimeout(timeoutId);
        return response.ok;
    } catch (error) {
        return false;
    }
}

// Load available versions from server
async function loadAvailableVersions() {
    const versionList = document.getElementById('version-list');
    const versionLoading = document.getElementById('version-loading');

    if (window._lastKnownMode === 1) { // MODE_AP
        document.getElementById('version-loading').style.display = 'block';
        document.getElementById('version-loading').innerHTML =
            '<div style="color: #ff9800; text-align: center; padding: 20px;">' +
            'Software updates unavailable in AP mode<br>' +
            '<small>Connect device to ship\'s WiFi for updates</small>' +
            '</div>';
        document.getElementById('version-list').style.display = 'none';
        return;
    }

    document.getElementById('version-loading').style.display = 'block';
    document.getElementById('version-list').style.display = 'none';

    // Show checking message
    versionLoading.innerHTML = '<div style="text-align: center; padding: 20px;">Testing internet connection...</div>';

    // Quick connectivity test
    const hasInternet = await testInternetConnectivity();

    if (!hasInternet) {
        versionLoading.innerHTML =
            '<div style="color: #ff6b6b; padding: 20px; text-align: center; background: #fff3f3; border-radius: 5px;">' +
            '<strong>⚠️ No Internet Access</strong><br><br>' +
            'WiFi is connected but cannot reach internet.<br>' +
            '<small>Check if ship\'s network has WAN connection.</small>' +
            '</div>';
        return;
    }

    // Internet check passed, now fetch versions
    versionLoading.innerHTML = '<div style="text-align: center; padding: 20px;">Loading available versions...</div>';

    try {
        // Fetch with reasonable timeout
        const controller = new AbortController();
        const timeoutId = setTrackedTimeout(() => controller.abort(), 15000); // 15 second timeout

        // ota.xengineering.net is our stable proxy (forwards to the Supabase Storage
        // OTA bucket); read-only mirror of versions.json. Deploys write to Supabase.
        const response = await fetch('https://ota.xengineering.net/versions.json', {
            signal: controller.signal,
            cache: 'no-store'
        });

        clearTimeout(timeoutId);

        if (!response.ok) {
            throw new Error('Server returned ' + response.status);
        }

        const data = await response.json();
        availableVersions = data.versions || {};
        displayAvailableVersions();

    } catch (error) {
        diagError('Error loading versions:', error);
        let errorMsg = 'Cannot load available versions.';

        if (error.name === 'AbortError') {
            errorMsg = 'Connection timeout (15s) - slow or unstable connection';
        } else if (error.message.includes('Failed to fetch') || error.message.includes('NetworkError')) {
            errorMsg = 'Network error - connection lost during fetch';
        } else {
            errorMsg = 'Error: ' + error.message;
        }

        versionLoading.innerHTML =
            '<div style="color: #ff6b6b; padding: 20px; text-align: center; background: #fff3f3; border-radius: 5px;">' +
            '<strong>⚠️ ' + errorMsg + '</strong><br><br>' +
            '<small>Try again or check network connection</small>' +
            '</div>';
    }
}

function getStoredPassword() {
    // Get password from an existing form that already has it populated
    const existingPasswordField = document.querySelector('.password_field');
    return existingPasswordField ? existingPasswordField.value : '';
}

// Display available versions
function displayAvailableVersions() {
    const versionList = document.getElementById('version-list');
    const versionLoading = document.getElementById('version-loading');

    if (Object.keys(availableVersions).length === 0) {
        versionLoading.innerHTML = '<div>No versions available</div>';
        return;
    }

    // Get current version for comparison
    const currentVersionStr = document.getElementById('current-version-display').textContent;

    // Filter out the current version, but allow all others (including downgrades)
    const sortedVersions = Object.keys(availableVersions)
        .filter(version => version !== currentVersionStr)
        .sort((a, b) => {
            // Parse version strings like "0.2.12" into comparable numbers
            const parseVersion = (v) => {
                const parts = v.split('.').map(Number);
                return parts[0] * 1000000 + parts[1] * 1000 + parts[2];
            };
            return parseVersion(b) - parseVersion(a); // Sort descending (highest first)
        });

    if (sortedVersions.length === 0) {
        versionLoading.innerHTML = '<div>No other versions available</div>';
        return;
    }

    // Forms are rendered dynamically AFTER the user unlocks settings, so
    // .password_field elements here won't get auto-populated by the unlock flow.
    // Grab the current password value from any existing populated password_field
    // and inject it directly into each version's form.
    const existingPwField = document.querySelector('.password_field');
    const passwordValue = existingPwField ? existingPwField.value : '';

    let html = '<div style="margin: 10px 0;">Click a version to update:</div>';
    sortedVersions.forEach(version => {
        const versionData = availableVersions[version];
        const notes = versionData.notes || 'No description available';
        const size = versionData.file_size ? (versionData.file_size / (1024 * 1024)).toFixed(1) + ' MB' : '';

        html += `
            <div style="border: 1px solid #ccc; margin: 10px 0; padding: 15px; border-radius: 5px;">
                <div style="display: flex; justify-content: space-between; align-items: center;">
                    <div>
                        <strong>Version ${version}</strong><br>
                        <span style="color: #666; font-size: 0.9em;">${notes}</span><br>
                        <span style="color: #888; font-size: 0.8em;">${size}</span>
                    </div>
                    <form action="${buildURL('/get')}" method="GET" target="hidden-form" onsubmit="return confirmUpdate('${version}')">
                        <input type="hidden" name="password" class="password_field" value="${passwordValue}">
                        <input type="hidden" name="UpdateToVersion" value="${version}">
                        <input type="submit" value="Change to ${version}" class="btn-primary">
                    </form>
                </div>
            </div>
        `;
    });

    versionLoading.style.display = 'none';
    versionList.innerHTML = html;
    versionList.style.display = 'block';
}

// Confirm update
function confirmUpdate(version) {
    // Password is guaranteed to exist because button can only be clicked after unlock
    const confirmed = confirm(`⚠️ ALTERNATOR WILL BE AUTOMATICALLY DISABLED FOR SAFETY ⚠️\n\nUpdate process takes 2-3 minutes. Do not interfere with auto-reboots. When finished, the web interface will be accessible in the usual way, but you must HARD-REFRESH your browser (Cmd+Shift+R on Mac, Ctrl+Shift+R on Windows/Linux) to load the new web files — otherwise your browser will keep showing the old cached UI. The Software Update sub-tab in Cloud Features will then confirm the new version.\n\nIf process fails, you may try again with better internet. If the whole thing bricks, you may always start fresh with the factory golden image (connect FactoryReset wire, pin 9 in RJ3, Green/White to GND), which will never force updates.\n\nAlternator will remain OFF after update - you must manually re-enable it.`);
    if (confirmed) {
        kickOffAppWebUpdate(version);  // No-op in browser; in iOS app downloads matching web bundle in parallel
        showUpdateInProgressOverlay(version);  // Same modal the forced-update path shows
    }
    return confirmed;
}

// Shows the "Update in progress…" full-screen overlay used by both the manual
// "Change to X.X.XX" flow and the forced-update flow. The message text tells
// mobile users to fully close + reopen the app once the device finishes — that's
// the standard live-update apply trigger (matches Capgo/CodePush/Expo pattern).
function showUpdateInProgressOverlay(versionStr) {
    forcedUpdateInProgressUntil = Date.now() + 6 * 60 * 1000;
    const overlay = document.getElementById('forced-update-overlay');
    if (!overlay) return;
    overlay.style.display = 'flex';
    overlay.innerHTML = `
      <div class="settings-card" style="max-width: 520px; width: 100%; text-align: center;">
          <div style="margin-bottom: 12px; text-align:center; font-weight: bold; font-size: 16px;">
              Update in progress…
          </div>
          <p style="margin: 8px 0 4px 0; font-size: 15px;">
              Downloading firmware v<strong>${versionStr}</strong> and rebooting.
          </p>
          <p style="margin: 12px 0 4px 0; font-size: 13px; color: #666;">
              Takes 2-3 minutes total. When it finishes, <strong>hard-refresh</strong> this tab to load the updated dashboard — Cmd+Shift+R on Mac, Ctrl+Shift+R on Windows/Linux, or fully close and reopen the app on mobile. A regular refresh may show stale cached files.
          </p>
          <p style="margin-top: 12px; font-size: 11px; color: #888;">
              Do not power-cycle the device during the update.
          </p>
      </div>`;
}

// When running inside the iOS (Capacitor) app, kick off a parallel download of
// the matching web bundle. The LiveUpdate plugin fetches it via the stable
// ota.xengineering.net proxy (otaBaseUrl in LiveUpdate.swift), which forwards to
// the Supabase Storage OTA bucket. The device firmware update is still triggered
// by the form submit; this just keeps the app's local web UI in sync. Silently
// no-ops in a regular browser.
async function kickOffAppWebUpdate(version) {
    if (!window.Capacitor || !window.Capacitor.isNativePlatform || !window.Capacitor.isNativePlatform()) {
        return;
    }
    const plugin = window.Capacitor.Plugins && window.Capacitor.Plugins.LiveUpdate;
    if (!plugin) {
        console.warn('[LiveUpdate] Plugin unavailable, skipping app web bundle download');
        return;
    }
    try {
        if (plugin.addListener) {
            plugin.addListener('downloadProgress', (data) => {
                console.log(`[LiveUpdate] ${data.version}: ${Math.round(data.progress * 100)}%`);
            });
        }
        const result = await plugin.downloadVersion({ version });
        console.log('[LiveUpdate] App bundle ready, applies on next cold launch:', result);
    } catch (err) {
        console.error('[LiveUpdate] App web bundle download failed:', err);
    }
}

// Track previous values to detect changes
let prevVersionInt = -1;
let prevDeviceIdUpper = -1;
let prevDeviceIdLower = -1;

// Separate function for firmware version
function updateFirmwareVersion(versionInt) {
    versionInt = parseInt(versionInt, 10) || 0;

    if (versionInt !== prevVersionInt) {
        const major = Math.floor(versionInt / 10000);
        const minor = Math.floor((versionInt % 10000) / 100);
        const patch = versionInt % 100;
        const versionStr = `${major}.${minor}.${patch}`;

        document.getElementById('current-version-display').textContent = versionStr;
        document.getElementById('current-version-display-2').textContent = versionStr;
        prevVersionInt = versionInt;
    }
}

function updateDeviceId() {
    const deviceIdUpper = parseInt(document.getElementById('deviceIdUpperID').textContent) >>> 0;
    const deviceIdLower = parseInt(document.getElementById('deviceIdLowerID').textContent) >>> 0;

    // Only update if device ID changed
    if (deviceIdUpper !== prevDeviceIdUpper || deviceIdLower !== prevDeviceIdLower) {
        // Decode device ID (16-char hex from upper and lower 32 bits)
        const deviceIdHex = deviceIdUpper.toString(16).padStart(8, '0') +
            deviceIdLower.toString(16).padStart(8, '0');
        const macAddress = deviceIdHex.toUpperCase();  // Use all 16 chars

        // Update profile display + System panel (latter shown when cloud features locked)
        document.getElementById('profile-device-uid').textContent = macAddress;
        const sysDeviceUid = document.getElementById('systemDeviceUID_ID');
        if (sysDeviceUid) sysDeviceUid.textContent = macAddress;

        prevDeviceIdUpper = deviceIdUpper;
        prevDeviceIdLower = deviceIdLower;
    }
}

//forced OTA stuff
// Set when user clicks "Update Now" on the forced-update modal. Suppresses the
// modal from re-rendering on subsequent CSV2 ticks while the download is in
// flight. Page reload after device reboot clears it naturally; the 6-minute
// timeout below is a fallback for failed updates so user can retry.
let forcedUpdateInProgressUntil = 0;

// Convert firmware version int to string (e.g., 35 → "0.0.35")
function firmwareIntToString(versionInt) {
    const major = Math.floor(versionInt / 10000);
    const minor = Math.floor((versionInt % 10000) / 100);
    const patch = versionInt % 100;
    return `${major}.${minor}.${patch}`;
}

// Format Unix timestamp to readable time
function formatDeadline(unixTimestamp) {
    const date = new Date(unixTimestamp * 1000);
    const now = new Date();
    const diffMs = date - now;
    const diffSecs = Math.floor(diffMs / 1000);

    if (diffSecs < 0) {
        return "EXPIRED";
    } else if (diffSecs < 60) {
        return `${diffSecs} seconds`;
    } else if (diffSecs < 3600) {
        const mins = Math.floor(diffSecs / 60);
        const secs = diffSecs % 60;
        return `${mins}m ${secs}s`;
    } else {
        const hours = Math.floor(diffSecs / 3600);
        const mins = Math.floor((diffSecs % 3600) / 60);
        return `${hours}h ${mins}m`;
    }
}

// Main forced update handler
function handleForcedUpdate(data) {
    const hasForcedUpdate = data.hasForcedUpdate === 1;
    const forcedVersionInt = data.forcedFwVersionInt || 0;
    if (!hasForcedUpdate || forcedVersionInt === 0) {
        // Nothing to do, no elements needed, no diagnostics needed
        return;
    }
    const deadline = data.forcedUpdateDeadline || 0;
    // Get or create banner element
    let banner = document.getElementById('forced-update-banner');
    if (!banner) {
        banner = document.createElement('div');
        banner.id = 'forced-update-banner';
        banner.style.cssText = `
        position: fixed;
        top: 0;
        left: 0;
        right: 0;
        z-index: 10000;
        padding: calc(28px + env(safe-area-inset-top)) 36px 28px;
        text-align: center;
        font-weight: 600;
        display: none;
        border-bottom: 4px solid var(--accent);
        background: var(--card-light);
        color: var(--text-dark);
        box-shadow: 0 4px 12px rgba(0,0,0,0.15);
        font-size: 28px;
        line-height: 1.4;
    `;
        document.body.insertBefore(banner, document.body.firstChild);
    }

    // Get or create overlay element
    let overlay = document.getElementById('forced-update-overlay');
    if (!overlay) {
        overlay = document.createElement('div');
        overlay.id = 'forced-update-overlay';
        overlay.style.cssText = `
            position: fixed;
            top: 0;
            left: 0;
            right: 0;
            bottom: 0;
            background: rgba(0, 0, 0, 0.75);
            z-index: 9999;
            display: none;
            justify-content: center;
            align-items: center;
            padding: 16px;
        `;
        document.body.appendChild(overlay);
    }

    if (!hasForcedUpdate || forcedVersionInt === 0) {
        // No forced update - hide everything
        banner.style.display = 'none';
        overlay.style.display = 'none';
        enableAllInputs();
        return;
    }

    const versionStr = firmwareIntToString(forcedVersionInt);
    const now = Math.floor(Date.now() / 1000); // Current Unix timestamp
    // deadline === 0 means none was set in cloud — treat as "upgrade required now"
    // rather than "deadline passed" which falsely implies the user missed a window.
    const noDeadlineSet = !deadline || deadline === 0;
    const isPastDeadline = !noDeadlineSet && now >= deadline;
    const requireBlocking = noDeadlineSet || isPastDeadline;

    if (requireBlocking) {
        // BLOCK EVERYTHING - show overlay
        banner.style.display = 'none';
        overlay.style.display = 'flex';

        // If user already pressed Update Now within the last few minutes, show
        // a progress message instead of re-rendering the prompt every 5 s.
        const updateActive = Date.now() < forcedUpdateInProgressUntil;
        if (updateActive) {
            overlay.innerHTML = `
              <div class="settings-card" style="max-width: 520px; width: 100%; text-align: center;">
                  <div style="margin-bottom: 12px; text-align:center; font-weight: bold; font-size: 16px;">
                      Update in progress…
                  </div>
                  <p style="margin: 8px 0 4px 0; font-size: 15px;">
                      Downloading firmware v<strong>${versionStr}</strong> and rebooting.
                  </p>
                  <p style="margin: 12px 0 4px 0; font-size: 13px; color: #666;">
                      Takes 2-3 minutes total. When it finishes, <strong>hard-refresh</strong> this tab to load the updated dashboard — Cmd+Shift+R on Mac, Ctrl+Shift+R on Windows/Linux, or fully close and reopen the app on mobile. A regular refresh may show stale cached files.
                  </p>
                  <p style="margin-top: 12px; font-size: 11px; color: #888;">
                      Do not power-cycle the device during the update.
                  </p>
              </div>
            `;
        } else {
            overlay.innerHTML = `
              <div class="settings-card" style="max-width: 520px; width: 100%; text-align: center;">
                  <div style="margin-bottom: 12px; text-align:center; font-weight: bold; font-size: 16px;">
                      Forced Firmware Update
                  </div>
                  <p style="margin: 8px 0 16px 0; font-size: 15px;">
                      This device must upgrade to firmware v<strong>${versionStr}</strong>.
                  </p>
                  <button onclick="triggerForcedUpdate('${versionStr}')"
                          class="forced-update-btn btn-primary"
                          style="margin-top: 4px;">
                      Update Now
                  </button>
                  <p style="margin-top: 12px; font-size: 11px; color: #888;">
                      To postpone, restart with WiFi disconnected or in Factory Mode.
                  </p>
              </div>
            `;
        }

        disableAllInputs();

    } else {
        // Show warning banner (not past deadline yet)
        overlay.style.display = 'none';
        banner.style.display = 'block';

        const timeRemaining = formatDeadline(deadline);

        banner.innerHTML = `
        Mandatory update to firmware version ${versionStr} will be required in ${timeRemaining}.  
        <button onclick="triggerForcedUpdate('${versionStr}')"
                class="forced-update-btn btn-secondary"
                style="margin-left: 24px;">
            Update Now
        </button>
    `;

        enableAllInputs(); // Still allow interaction before deadline
    }
}

// Disable all form inputs (when past deadline)
function disableAllInputs() {
    const inputs = document.querySelectorAll('input, button, select, textarea');
    inputs.forEach(input => {
        // Don't disable the forced update button
        if (input.type !== 'hidden' && !input.classList.contains('forced-update-btn')) {
            input.disabled = true;
            input.style.opacity = '0.5';
            input.style.cursor = 'not-allowed';
        }
    });
}

// Re-enable all form inputs
function enableAllInputs() {
    const inputs = document.querySelectorAll('input, button, select, textarea');
    inputs.forEach(input => {
        if (input.type !== 'hidden') {
            input.disabled = false;
            input.style.opacity = '1';
            input.style.cursor = '';
        }
    });
}

// Trigger forced update manually
function triggerForcedUpdate(versionStr) {
    const confirmed = confirm('Update process begins in ~7 seconds and takes 2-3 minutes, includes re-boots.  Do not interfere.  Web interface will then be accessible in the usual way, but you must HARD-REFRESH your browser (Cmd+Shift+R on Mac, Ctrl+Shift+R on Windows/Linux) to load the new web files — otherwise your browser will keep showing the old cached UI.  The Software Update sub-tab in Cloud Features will then show the new version #.  If process fails, you may try again with better internet.  If the whole thing bricks, you may start fresh with the factory golden image.  Continue?');
    if (confirmed) {
        kickOffAppWebUpdate(versionStr);  // No-op in browser; in iOS app downloads matching web bundle in parallel
        // Suppress the prompt modal from re-rendering on subsequent CSV2 ticks
        // while the update is in flight. 6 minutes = headroom for the 2-3 min
        // typical OTA + reboot, then fall back to the prompt if it didn't take.
        forcedUpdateInProgressUntil = Date.now() + 6 * 60 * 1000;
        // Immediately re-render the modal as "in progress" so the user gets
        // feedback before the next CSV2 tick (~5 s away).
        const overlay = document.getElementById('forced-update-overlay');
        if (overlay) {
            overlay.innerHTML = `
              <div class="settings-card" style="max-width: 520px; width: 100%; text-align: center;">
                  <div style="margin-bottom: 12px; text-align:center; font-weight: bold; font-size: 16px;">
                      Update in progress…
                  </div>
                  <p style="margin: 8px 0 4px 0; font-size: 15px;">
                      Downloading firmware v<strong>${versionStr}</strong> and rebooting.
                  </p>
                  <p style="margin: 12px 0 4px 0; font-size: 13px; color: #666;">
                      Takes 2-3 minutes total. When it finishes, <strong>hard-refresh</strong> this tab to load the updated dashboard — Cmd+Shift+R on Mac, Ctrl+Shift+R on Windows/Linux, or fully close and reopen the app on mobile. A regular refresh may show stale cached files.
                  </p>
                  <p style="margin-top: 12px; font-size: 11px; color: #888;">
                      Do not power-cycle the device during the update.
                  </p>
              </div>`;
        }

        const form = document.createElement('form');
        form.action = buildURL('/get');
        form.method = 'GET';
        form.target = 'hidden-form';

        const versionInput = document.createElement('input');
        versionInput.type = 'hidden';
        versionInput.name = 'UpdateToVersion';
        versionInput.value = versionStr;

        form.appendChild(versionInput);
        document.body.appendChild(form);
        form.submit();
        document.body.removeChild(form);
    }
}

//profiling stuff
function profileOperation(operationName, fn) {
    if (!ENABLE_DETAILED_PROFILING) return fn();

    const startTime = performance.now();
    const result = fn();
    const duration = performance.now() - startTime;

    if (duration > 1) { // Only log operations taking >1ms
        if (!performanceMetrics[operationName]) {
            performanceMetrics[operationName] = [];
        }
        performanceMetrics[operationName].push(duration);

        // Keep only last 50 measurements
        if (performanceMetrics[operationName].length > 50) {
            performanceMetrics[operationName].shift();
        }

        diagLog(`[PROF] ${operationName}: ${duration.toFixed(2)}ms`);
    }

    return result;
}
function showPerformanceReport() {
    diagLog('\n=== PERFORMANCE REPORT ===');
    Object.entries(performanceMetrics).forEach(([operation, times]) => {
        if (times.length > 0) {
            const avg = times.reduce((a, b) => a + b) / times.length;
            const max = Math.max(...times);
            diagLog(`${operation}: avg=${avg.toFixed(2)}ms, max=${max.toFixed(2)}ms, samples=${times.length}`);
        }
    });
}
// ECHO UPDATE - Only update changed values
let lastEchoValues = new Map();

function updateEchoIfChanged(elementId, newValue) {
    const cacheKey = elementId;
    const lastValue = lastEchoValues.get(cacheKey);

    if (lastValue !== newValue) {
        lastEchoValues.set(cacheKey, newValue);
        const element = document.getElementById(elementId);
        if (element) {  // Add this null check
            element.textContent = newValue;
        } else {
            diagWarn(`Element not found: ${elementId}`);  // Debug which element is missing
        }
        return true;
    }
    return false;
}

//DOM Updates
// Track what we last wrote to each element to avoid redundant updates
let lastWrittenValues = new Map();

function scheduleDOMUpdateOptimized(elementId, newContent) {
    // Check if we already wrote this exact content to this element
    if (lastWrittenValues.get(elementId) === newContent) {
        return; // Skip - we already wrote this exact value
    }

    // Queue the update
    pendingDOMUpdates.set(elementId, newContent);

    // Batch all updates in a single animation frame
    if (!domUpdateScheduled) {
        domUpdateScheduled = true;
        requestAnimationFrame(() => {
            profileOperation('domUpdates', () => {
                let updateCount = 0;
                for (const [id, content] of pendingDOMUpdates) {
                    const element = document.getElementById(id);
                    // Final safety check before writing
                    if (element && element.textContent !== content) {
                        element.textContent = content;
                        lastWrittenValues.set(id, content); // Track what we wrote
                        updateCount++;
                    }
                }
            });

            pendingDOMUpdates.clear();
            domUpdateScheduled = false;
        });
    }
}

// PERFORMANCE MONITORING
let frameTimeTracker = {
    lastFrameTime: performance.now(),
    frameTimes: [],
    worstFrame: 0
};

function trackFrameTime() {
    const now = performance.now();
    const frameTime = now - frameTimeTracker.lastFrameTime;
    frameTimeTracker.lastFrameTime = now;

    if (frameTimeTracker.frameTimes.length > 0) { // Skip first frame
        frameTimeTracker.frameTimes.push(frameTime);
        frameTimeTracker.worstFrame = Math.max(frameTimeTracker.worstFrame, frameTime);

        // Keep last 100 frames
        if (frameTimeTracker.frameTimes.length > 100) {
            frameTimeTracker.frameTimes.shift();
        }

        // Log if frame took too long
        if (frameTime > 20) { // More than 20ms = under 50fps
            diagWarn(`[PERF] Slow frame: ${frameTime.toFixed(2)}ms`);
        }
    }
}

//globals for axes configuration from ESP32
const CONFIG_CHECK_INTERVAL_SECONDS = 1.0;  // How often to check for config changes (seconds)

// Cache variables for change detection
let cachedPlotTimeWindow = null;
let cachedWebgaugesInterval = null;
let cachedYmin1 = null, cachedYmax1 = null;
let cachedYmin2 = null, cachedYmax2 = null;
let cachedYmin3 = null, cachedYmax3 = null;
let cachedYmin4 = null, cachedYmax4 = null;

// Counters for timing
let configCheckCounter = 0;
// Function to calculate how many loops equal the config check interval
function getConfigCheckInterval(webgaugesIntervalMs) {
    if (!webgaugesIntervalMs || webgaugesIntervalMs <= 0) return 5; // fallback
    return Math.max(1, Math.round((CONFIG_CHECK_INTERVAL_SECONDS * 1000) / webgaugesIntervalMs));
}

// Computes a padded min/max range from selected series in a data array.
// minSpan prevents zooming in too far; marginFrac adds padding; hardMin clamps the lower bound.
function computeScaleRange(dataArray, seriesIndices, minSpan, marginFrac, hardMin) {
    let lo = Infinity, hi = -Infinity;
    for (const idx of seriesIndices) {
        const arr = dataArray[idx];
        for (let i = 0; i < arr.length; i++) {
            const v = arr[i];
            if (isFinite(v)) { if (v < lo) lo = v; if (v > hi) hi = v; }
        }
    }
    if (!isFinite(lo) || !isFinite(hi)) return null;
    let span = hi - lo;
    if (span < minSpan) {
        const mid = (lo + hi) / 2;
        lo = mid - minSpan / 2;
        hi = mid + minSpan / 2;
        span = minSpan;
    }
    const margin = span * marginFrac;
    lo -= margin;
    hi += margin;
    if (hardMin !== undefined) lo = Math.max(hardMin, lo);
    return { min: lo, max: hi };
}

// wrapper function
function processCSVDataOptimized(data) {
    return profileOperation('dataProcessing', () => {

        // Increment data points counter
        plotRenderTracker.dataPointsProcessed++;

        const now = useTimestamps ? Math.floor(Date.now() / 1000) : null;

        // Measure actual inter-arrival time and update lerpDuration for all plots.
        // Clamp to [100, 1000]ms to avoid wild values at startup or after long gaps.
        const _arrivalNow = performance.now();
        if (processCSVDataOptimized._lastArrival > 0) {
            const measured = _arrivalNow - processCSVDataOptimized._lastArrival;
            const clamped = Math.max(100, Math.min(1000, measured));
            plotInterp.current.lerpDuration     = clamped;
            plotInterp.voltage.lerpDuration     = clamped;
            plotInterp.rpm.lerpDuration         = clamped;
            plotInterp.temperature.lerpDuration = clamped;
            plotInterp.pid.lerpDuration         = clamped;
            plotInterp.cv.lerpDuration          = clamped;
        }
        processCSVDataOptimized._lastArrival = _arrivalNow;

        // ALWAYS UPDATE DATA STRUCTURES - Current/Temperature plot data
        const battCurrent = 'Bcur' in data ? parseFloat(data.Bcur) / 100 : 0;
        const altCurrent = 'MeasuredAmps' in data ? parseFloat(data.MeasuredAmps) / 100 : 0;
        const fieldCurrent = 'iiout' in data ? parseFloat(data.iiout) / 100 : 0;
        const fieldPct = 'dutyCycle' in data ? parseFloat(data.dutyCycle) / 100 : 0;

        // Shift all current data left and add new data at the end
        const prevY_current = [
            currentTempData[1][currentTempData[1].length - 1],
            currentTempData[2][currentTempData[2].length - 1],
            currentTempData[3][currentTempData[3].length - 1],
            currentTempData[4][currentTempData[4].length - 1],
        ];
        for (let i = 1; i < currentTempData[1].length; i++) {
            if (useTimestamps) {
                currentTempData[0][i - 1] = currentTempData[0][i];
            }
            currentTempData[1][i - 1] = currentTempData[1][i];
            currentTempData[2][i - 1] = currentTempData[2][i];
            currentTempData[3][i - 1] = currentTempData[3][i];
            currentTempData[4][i - 1] = currentTempData[4][i];
        }
        const lastCurrentIndex = currentTempData[1].length - 1;
        if (useTimestamps) {
            currentTempData[0][lastCurrentIndex] = now;
        }
        currentTempData[1][lastCurrentIndex] = battCurrent;
        currentTempData[2][lastCurrentIndex] = altCurrent;
        currentTempData[3][lastCurrentIndex] = fieldCurrent;
        currentTempData[4][lastCurrentIndex] = fieldPct;
        plotInterp.current.prevY = prevY_current;
        plotInterp.current.nextY = [battCurrent, altCurrent, fieldCurrent, fieldPct];
        plotInterp.current.arrivalTime = performance.now();
        if (autoScaleCurrent && !autoScaleCurrentLocked) {
            _autoScaleCurrentLeft  = computeScaleRange(currentTempData, [1, 2, 3], 5, 0.10);
            _autoScaleCurrentRight = computeScaleRange(currentTempData, [4], 20, 0.10, 0);
        }

        // ALWAYS UPDATE DATA STRUCTURES - Voltage plot data
        const adsBattV = 'BatteryV' in data ? parseFloat(data.BatteryV) / 100 : 0;
        const inaBattV = 'IBV' in data ? parseFloat(data.IBV) / 100 : 0;

        const prevY_voltage = [
            voltageData[1][voltageData[1].length - 1],
            voltageData[2][voltageData[2].length - 1],
            voltageData[3][voltageData[3].length - 1],
        ];
        for (let i = 1; i < voltageData[1].length; i++) {
            if (useTimestamps) {
                voltageData[0][i - 1] = voltageData[0][i];
            }
            voltageData[1][i - 1] = voltageData[1][i];
            voltageData[2][i - 1] = voltageData[2][i];
            voltageData[3][i - 1] = voltageData[3][i];
        }
        const lastVoltageIndex = voltageData[1].length - 1;
        if (useTimestamps) {
            voltageData[0][lastVoltageIndex] = now;
        }
        voltageData[1][lastVoltageIndex] = adsBattV;
        voltageData[2][lastVoltageIndex] = inaBattV;
        voltageData[3][lastVoltageIndex] = fieldPct;
        plotInterp.voltage.prevY = prevY_voltage;
        plotInterp.voltage.nextY = [adsBattV, inaBattV, fieldPct];
        plotInterp.voltage.arrivalTime = performance.now();
        if (autoScaleVoltage && !autoScaleVoltageLocked) {
            _autoScaleVoltageLeft  = computeScaleRange(voltageData, [1, 2], 0.5, 0.10);
            _autoScaleVoltageRight = computeScaleRange(voltageData, [3], 20, 0.10, 0);
        }

        // ALWAYS UPDATE DATA STRUCTURES - RPM plot data
        const rpmValue = 'RPM' in data ? parseFloat(data.RPM) : 0;

        const prevY_rpm = [
            rpmData[1][rpmData[1].length - 1],
            rpmData[2][rpmData[2].length - 1],
        ];
        for (let i = 1; i < rpmData[1].length; i++) {
            if (useTimestamps) {
                rpmData[0][i - 1] = rpmData[0][i];
            }
            rpmData[1][i - 1] = rpmData[1][i];
            rpmData[2][i - 1] = rpmData[2][i];
        }
        const lastRPMIndex = rpmData[1].length - 1;
        if (useTimestamps) {
            rpmData[0][lastRPMIndex] = now;
        }
        rpmData[1][lastRPMIndex] = rpmValue;
        rpmData[2][lastRPMIndex] = fieldPct;
        plotInterp.rpm.prevY = prevY_rpm;
        plotInterp.rpm.nextY = [rpmValue, fieldPct];
        plotInterp.rpm.arrivalTime = performance.now();
        if (autoScaleRPM && !autoScaleRPMLocked) {
            _autoScaleRPMLeft  = computeScaleRange(rpmData, [1], 200, 0.10);
            _autoScaleRPMRight = computeScaleRange(rpmData, [2], 20, 0.10, 0);
        }

        // ALWAYS UPDATE DATA STRUCTURES - Temperature plot data
        const altTemp = 'AlternatorTemperatureF' in data ? parseFloat(data.AlternatorTemperatureF) / 100 : 0;

        const prevY_temp = [
            temperatureData[1][temperatureData[1].length - 1],
            temperatureData[2][temperatureData[2].length - 1],
        ];
        for (let i = 1; i < temperatureData[1].length; i++) {
            if (useTimestamps) {
                temperatureData[0][i - 1] = temperatureData[0][i];
            }
            temperatureData[1][i - 1] = temperatureData[1][i];
            temperatureData[2][i - 1] = temperatureData[2][i];
        }
        const lastTempIndex = temperatureData[1].length - 1;
        if (useTimestamps) {
            temperatureData[0][lastTempIndex] = now;
        }
        temperatureData[1][lastTempIndex] = altTemp;
        temperatureData[2][lastTempIndex] = fieldPct;
        plotInterp.temperature.prevY = prevY_temp;
        plotInterp.temperature.nextY = [altTemp, fieldPct];
        plotInterp.temperature.arrivalTime = performance.now();
        if (autoScaleTemp && !autoScaleTempLocked) {
            _autoScaleTempLeft  = computeScaleRange(temperatureData, [1], 10, 0.10);
            _autoScaleTempRight = computeScaleRange(temperatureData, [2], 20, 0.10, 0);
        }

        // ALWAYS UPDATE DATA STRUCTURES - PID Tuning plot data
        // ALWAYS UPDATE DATA STRUCTURES - PID Tuning plot data
        if (pidTuningData) {
            const setpointLimited = 'setpointLimited' in data ? parseFloat(data.setpointLimited) / 100 : 0;
            const pidInput = 'pidInput' in data ? parseFloat(data.pidInput) / 100 : 0;
            const iMeasFilt = 'MeasuredAmps_filtered' in data ? parseFloat(data.MeasuredAmps_filtered) / 100 : 0;
            const uTargetAmps = 'uTargetAmps' in data ? parseFloat(data.uTargetAmps) / 100 : 0;
            const dutyCycle = 'dutyCycle' in data ? parseFloat(data.dutyCycle) / 100 : 0;
            const pidOutput = 'pidOutput' in data ? parseFloat(data.pidOutput) / 100 : 0;
            const rpmScaled = rpmValue / 100;

            // Keep latest real RPM for watermark
            window._lastKnownRPM = rpmValue;

            const last_pid = pidTuningData[1].length - 1;
            const prevY_pid = [
                pidTuningData[1][last_pid], pidTuningData[2][last_pid],
                pidTuningData[3][last_pid], pidTuningData[4][last_pid],
                pidTuningData[5][last_pid], pidTuningData[6][last_pid],
                pidTuningData[7][last_pid],
            ];
            for (let i = 1; i < pidTuningData[1].length; i++) {
                pidTuningData[1][i - 1] = pidTuningData[1][i]; // setpointLimited
                pidTuningData[2][i - 1] = pidTuningData[2][i]; // pidInput (raw)
                pidTuningData[3][i - 1] = pidTuningData[3][i]; // iMeas_filt
                pidTuningData[4][i - 1] = pidTuningData[4][i]; // uTargetAmps
                pidTuningData[5][i - 1] = pidTuningData[5][i]; // dutyCycle
                pidTuningData[6][i - 1] = pidTuningData[6][i]; // pidOutput
                pidTuningData[7][i - 1] = pidTuningData[7][i]; // RPM / 100
            }

            const lastPidIndex = pidTuningData[1].length - 1;
            pidTuningData[1][lastPidIndex] = setpointLimited;
            pidTuningData[2][lastPidIndex] = pidInput;
            pidTuningData[3][lastPidIndex] = iMeasFilt;
            pidTuningData[4][lastPidIndex] = uTargetAmps;
            pidTuningData[5][lastPidIndex] = dutyCycle;
            pidTuningData[6][lastPidIndex] = pidOutput;
            pidTuningData[7][lastPidIndex] = rpmScaled;
            plotInterp.pid.prevY = prevY_pid;
            plotInterp.pid.nextY = [setpointLimited, pidInput, iMeasFilt, uTargetAmps, dutyCycle, pidOutput, rpmScaled];
            plotInterp.pid.arrivalTime = performance.now();
        }

        // CV voltage tuning plot data — all four series now come from CSV1 (fast rate).
        // Always appends; the corner "lock" button only freezes the Y axes, never the data.
        if (cvTuningData) {
            const voltTarget = 'voltageTarget' in data ? parseFloat(data.voltageTarget) / 100 : null;
            const battV      = 'BatteryV_raw' in data ? parseFloat(data.BatteryV_raw) / 100 : null;
            const ibvRaw     = 'IBV' in data ? parseFloat(data.IBV) / 100 : null;
            const icv        = 'Icv' in data ? parseFloat(data.Icv) / 100 : null;
            const last_cv = cvTuningData[1].length - 1;
            const prevY_cv = [
                cvTuningData[1][last_cv], cvTuningData[2][last_cv],
                cvTuningData[3][last_cv], cvTuningData[4][last_cv],
            ];
            for (let i = 1; i < cvTuningData[1].length; i++) {
                cvTuningData[1][i - 1] = cvTuningData[1][i];
                cvTuningData[2][i - 1] = cvTuningData[2][i];
                cvTuningData[3][i - 1] = cvTuningData[3][i];
                cvTuningData[4][i - 1] = cvTuningData[4][i];
            }
            const last = cvTuningData[1].length - 1;
            cvTuningData[1][last] = voltTarget;
            cvTuningData[2][last] = battV;
            cvTuningData[3][last] = ibvRaw;
            cvTuningData[4][last] = icv;
            plotInterp.cv.prevY = prevY_cv;
            plotInterp.cv.nextY = [voltTarget, battV, ibvRaw, icv];
            plotInterp.cv.arrivalTime = performance.now();
        }

        return 1;
    });
}


function updatePlotConfiguration(data) {

    let configChanged = false;

    // Check for any axis limit changes
    const axisChanged = (data.Ymin1 !== cachedYmin1 || data.Ymax1 !== cachedYmax1 ||
        data.Ymin2 !== cachedYmin2 || data.Ymax2 !== cachedYmax2 ||
        data.Ymin3 !== cachedYmin3 || data.Ymax3 !== cachedYmax3 ||
        data.Ymin4 !== cachedYmin4 || data.Ymax4 !== cachedYmax4);

    if (axisChanged) {
        // Update all global axis variables
        Ymin1 = data.Ymin1; Ymax1 = data.Ymax1;
        Ymin2 = data.Ymin2; Ymax2 = data.Ymax2;
        Ymin3 = data.Ymin3; Ymax3 = data.Ymax3;
        Ymin4 = data.Ymin4; Ymax4 = data.Ymax4;
        // Update cached values
        cachedYmin1 = data.Ymin1; cachedYmax1 = data.Ymax1;
        cachedYmin2 = data.Ymin2; cachedYmax2 = data.Ymax2;
        cachedYmin3 = data.Ymin3; cachedYmax3 = data.Ymax3;
        cachedYmin4 = data.Ymin4; cachedYmax4 = data.Ymax4;
        // Re-range in place — destroying/recreating the plots here made the
        // page jump every time a Y limit was edited and echoed back. Plots in
        // autoscale mode skip the set; the per-frame autoscale pass owns them.
        if (currentTempPlot && !autoScaleCurrent) currentTempPlot.setScale('current', { min: Ymin1, max: Ymax1 });
        if (voltagePlot && !autoScaleVoltage) voltagePlot.setScale('voltage', { min: Ymin2 / 100, max: Ymax2 / 100 });
        if (rpmPlot && !autoScaleRPM) rpmPlot.setScale('rpm', { min: Ymin3, max: Ymax3 });
        if (temperaturePlot && !autoScaleTemp) temperaturePlot.setScale('temperature', { min: Ymin4, max: Ymax4 });

        configChanged = true;
    }

    // Sample interval changed → re-grid the fixed 5-min buffer (point spacing changed) and rebuild plots.
    if (data.webgaugesinterval !== cachedWebgaugesInterval) {
        cachedWebgaugesInterval = data.webgaugesinterval;
        cachedPlotTimeWindow = data.plotTimeWindow;   // reinit captures liveWindowSec below

        reinitializePlotsWithNewTiming(data);

        // Destroy and recreate all plots to fix X-axis labels
        if (currentTempPlot) { currentTempPlot.destroy(); initCurrentTempPlot(); }
        if (voltagePlot) { voltagePlot.destroy(); initVoltagePlot(); }
        if (rpmPlot) { rpmPlot.destroy(); initRPMPlot(); }
        if (temperaturePlot) { temperaturePlot.destroy(); initTemperaturePlot(); }

        configChanged = true;
    }

    // Window changed → just slide the visible X range over the fixed buffer. No realloc, no data loss.
    if (data.plotTimeWindow !== cachedPlotTimeWindow) {
        cachedPlotTimeWindow = data.plotTimeWindow;
        liveWindowSec = data.plotTimeWindow;
        applyLiveWindowToPlots();
        configChanged = true;
    }

    // Check for PID tuning plot configuration changes
    const pidTuningAxisChanged = (data.yyMin !== cachedYyMin || data.yyMax !== cachedYyMax);
    const pidTuningTimeChanged = (data.xTime !== cachedXTime);

    if (pidTuningAxisChanged) {
        yyMin = data.yyMin;
        yyMax = data.yyMax;
        cachedYyMin = data.yyMin;
        cachedYyMax = data.yyMax;

        // Re-range in place — recreate caused a page jump (same as the live plots above)
        if (pidTuningPlot) pidTuningPlot.setScale('amps', { min: yyMin, max: yyMax });

        configChanged = true;
    }

    if (pidTuningTimeChanged) {
        if (data.xTime !== undefined && !isNaN(data.xTime) && data.xTime > 0) {
            xTime = data.xTime;
        }
        cachedXTime = data.xTime;

        // Reinitialize data structures and recreate both tuning plots
        if (pidTuningPlot) {
            pidTuningPlot.destroy();
        }
        initPidTuningDataStructures();
        initPidTuningPlot();

        // CV plot uses xTime as its fallback window when cvXTime is null
        if (cvTuningPlot && cvXTime === null) {
            rebuildCVTuningWindow();
        }

        configChanged = true;
    }

    if (configChanged) {
    }
}

// Re-apply the visible X window to all four short-term plots by re-running their x range fn (no data change).
function applyLiveWindowToPlots() {
    [currentTempPlot, voltagePlot, rpmPlot, temperaturePlot].forEach(p => {
        if (p && p.data) { try { p.setData(p.data, true); } catch (e) {} }
    });
}

// Function to reinitialize plots when timing parameters change
function reinitializePlotsWithNewTiming(data) {
    // Calculate new buffer size
    liveWindowSec = data.plotTimeWindow;                                  // visible span follows the setting
    const newMaxPoints = Math.ceil((LIVE_BUFFER_SEC * 1000) / data.webgaugesinterval);   // buffer always 5 min
    const now = Math.floor(Date.now() / 1000);
    const intervalSec = data.webgaugesinterval / 1000;

    // Recalculate X-axis (timestamps going back in time)
    xAxisData = [];
    if (useTimestamps) {
        const now = Math.floor(Date.now() / 1000);
        for (let i = 0; i < newMaxPoints; i++) {
            xAxisData[i] = now - (newMaxPoints - 1 - i) * intervalSec;
        }
    } else {
        for (let i = 0; i < newMaxPoints; i++) {
            xAxisData[i] = -(newMaxPoints - 1 - i) * intervalSec;
        }
    }

    // Reinitialize circular buffers with new size
    currentTempData = [
        [...xAxisData], // X values (timestamps)
        new Array(newMaxPoints).fill(0), // Battery current
        new Array(newMaxPoints).fill(0), // Alt current
        new Array(newMaxPoints).fill(0), // Field current
        new Array(newMaxPoints).fill(0)  // Field% (duty cycle)
    ];

    voltageData = [
        [...xAxisData], // X values
        new Array(newMaxPoints).fill(0), // ADS voltage
        new Array(newMaxPoints).fill(0), // INA voltage
        new Array(newMaxPoints).fill(0)  // Field% (duty cycle)
    ];

    rpmData = [
        [...xAxisData], // X values
        new Array(newMaxPoints).fill(0), // RPM
        new Array(newMaxPoints).fill(0)  // Field% (duty cycle)
    ];

    temperatureData = [
        [...xAxisData], // X values
        new Array(newMaxPoints).fill(0), // Temperature
        new Array(newMaxPoints).fill(0)  // Field% (duty cycle)
    ];

    // Reset circular buffer indices
    currentTempIndex = 0;
    voltageIndex = 0;
    rpmIndex = 0;
    temperatureIndex = 0;

    // Reset interp states so rAF loop doesn't apply stale prev/next to new arrays
    plotInterp.current.arrivalTime = 0;
    plotInterp.voltage.arrivalTime = 0;
    plotInterp.rpm.arrivalTime = 0;
    plotInterp.temperature.arrivalTime = 0;

    // Update plots - they'll auto-scale with time: true
    if (currentTempPlot) {
        currentTempPlot.setData(currentTempData);
    }
    if (voltagePlot) {
        voltagePlot.setData(voltageData);
    }
    if (rpmPlot) {
        rpmPlot.setData(rpmData);
    }
    if (temperaturePlot) {
        temperaturePlot.setData(temperatureData);
    }
}

function updateWeatherAlerts() {
    const lat = parseFloat(document.getElementById('LatitudeNMEA_display')?.textContent || '0');
    const lng = parseFloat(document.getElementById('LongitudeNMEA_display')?.textContent || '0');
    const weatherValid = parseInt(document.getElementById('weatherDataValidID')?.textContent || '0');

    const gpsAlert = document.getElementById('gps-missing-alert');
    const gpsStaleAlert = document.getElementById('gps-stale-alert');
    const weatherAlert = document.getElementById('weather-invalid-alert');

    if (!gpsAlert || !gpsStaleAlert || !weatherAlert) return; // Elements not loaded yet

    // Check if GPS coordinates are 0,0 (missing)
    const gpsMissing = (lat === 0 && lng === 0);

    // Check if GPS data is stale (>60 seconds old) and not manually overridden
    let gpsStale = false;
    if (window.sensorAges && !window.gpsManualOverride && !gpsMissing) {
        const latAge = window.sensorAges.latitude || 0;
        const lngAge = window.sensorAges.longitude || 0;
        const maxAge = Math.max(latAge, lngAge);
        gpsStale = maxAge > 60000; // 60 seconds
    }

    // If we get fresh GPS data, clear the manual override flag
    if (window.sensorAges && !gpsMissing) {
        const latAge = window.sensorAges.latitude || 0;
        const lngAge = window.sensorAges.longitude || 0;
        const maxAge = Math.max(latAge, lngAge);
        if (maxAge < 5000) { // Fresh data (<5 seconds)
            window.gpsManualOverride = false;
        }
    }

    // Show appropriate alerts
    gpsAlert.style.display = gpsMissing ? 'block' : 'none';
    gpsStaleAlert.style.display = gpsStale ? 'block' : 'none';

    // Show weather alert if data is invalid (and GPS is present and not stale)
    if (weatherValid === 0 && !gpsMissing && !gpsStale) {
        weatherAlert.style.display = 'block';
    } else {
        weatherAlert.style.display = 'none';
    }
}


// Function to update all echo values
function updateAllEchosOptimized(data) {
    let updatesCount = 0;

    // All echo updates with change detection        
    //THIS IS WHERE SCALING HAPPENS FOR THE ECHOS!!!

    const echoUpdates = [
        { key: 'TemperatureLimitF', id: 'TemperatureLimitF_echo', transform: v => Math.round(toDisplayTemp(v)) },
        { key: 'BulkVoltage', id: 'BulkVoltage_echo', transform: v => (v / 100).toFixed(2) },
        { key: 'wavePeriod', id: 'wavePeriod_echo', transform: v => v },
        { key: 'FloatVoltage', id: 'FloatVoltage_echo', transform: v => (v / 100).toFixed(2) },
        { key: 'SwitchingFrequency', id: 'SwitchingFrequency_echo', transform: v => v },
        { key: 'yyMin', id: 'yyMin_echo', transform: v => v },
        { key: 'FieldAdjustmentInterval', id: 'FieldAdjustmentInterval_echo', transform: v => v },
        { key: 'ManualDutyTarget', id: 'ManualDutyTarget_echo', transform: v => v },
        { key: 'SwitchControlOverride', id: 'SwitchControlOverride_echo', transform: v => v == 1 ? 'Override' : 'Normal' },
        { key: 'OnOff', id: 'OnOff_echo', transform: v => v == 1 ? 'On' : 'Off' },
        { key: 'ManualFieldToggle', id: 'ManualFieldToggle_echo', transform: v => v === 0 ? 1 : 0 },
        { key: 'LimpHome', id: 'LimpHome_echo', transform: v => v == 1 ? 'On' : 'Off' },
        { key: 'VeData', id: 'VeData_echo', transform: v => v == 1 ? 'Enabled' : 'Disabled' },
        { key: 'NMEA0183Data', id: 'NMEA0183Data_echo', transform: v => v == 1 ? 'Enabled' : 'Disabled' },
        { key: 'NMEA2KData', id: 'NMEA2KData_echo', transform: v => v == 1 ? 'Enabled' : 'Disabled' },
        { key: 'waveAmplitude', id: 'waveAmplitude_echo', transform: v => v },
        { key: 'CurrentThreshold', id: 'CurrentThreshold_echo', transform: v => v / 100 },
        { key: 'PeukertExponent_scaled', id: 'PeukertExponent_echo', transform: v => (v / 100).toFixed(2) },
        { key: 'ChargeEfficiency_scaled', id: 'ChargeEfficiency_echo', transform: v => (v / 10).toFixed(1) + '%' },
        { key: 'ChargedVoltage_Scaled', id: 'ChargedVoltage_echo', transform: v => (v / 100).toFixed(2) },
        { key: 'TailCurrent', id: 'TailCurrent_echo', transform: v => (v / 10).toFixed(1) },
        { key: 'ChargedDetectionTime', id: 'ChargedDetectionTime_echo', transform: v => v },
        { key: 'IgnoreTemperature', id: 'IgnoreTemperature_echo', transform: v => v == 1 ? 'Yes' : 'No' },
        { key: 'IgnoreRPM', id: 'IgnoreRPM_echo', transform: v => v == 1 ? 'Yes' : 'No' },
        { key: 'MinRPMForField', id: 'MinRPMForField_echo', transform: v => v },
        { key: 'bmsLogic', id: 'bmsLogic_echo', transform: v => v == 1 ? 'Yes' : 'No' },
        { key: 'bmsLogicLevelOff', id: 'bmsLogicLevelOff_echo', transform: v => v == 0 ? 'Low' : 'High' },
        { key: 'AlarmActivate', id: 'AlarmActivate_echo', transform: v => v == 1 ? 'On' : 'Off' },
        { key: 'TempAlarm', id: 'TempAlarm_echo', transform: v => Math.round(toDisplayTemp(v)) },
        { key: 'TempAlarmLow', id: 'TempAlarmLow_echo', transform: v => Math.round(toDisplayTemp(v)) },
        { key: 'VoltageAlarmHigh', id: 'VoltageAlarmHigh_echo', transform: v => v },
        { key: 'VoltageAlarmLow', id: 'VoltageAlarmLow_echo', transform: v => v },
        { key: 'CurrentAlarmHigh', id: 'CurrentAlarmHigh_echo', transform: v => v },
        { key: 'RPMScalingFactor', id: 'RPMScalingFactor_echo', transform: v => v },
        { key: 'MaximumAllowedBatteryAmps', id: 'MaximumAllowedBatteryAmps_echo', transform: v => v },
        { key: 'LoadDumpDtThresh1',   id: 'LoadDumpDtThresh1_echo',   transform: v => v },
        { key: 'LoadDumpDtThresh',    id: 'LoadDumpDtThresh_echo',    transform: v => v },
        { key: 'LoadDumpDtThresh3',   id: 'LoadDumpDtThresh3_echo',   transform: v => v },
        { key: 'ManualSOCPoint', id: 'ManualSOCPoint_echo', transform: v => v / 100 },
        { key: 'ShuntResistanceMicroOhm', id: 'ShuntResistanceMicroOhm_echo', transform: v => v },
        { key: 'InvertAltAmps', id: 'InvertAltAmps_echo', transform: v => v == 1 ? 'Yes' : 'No' },
        { key: 'InvertBattAmps', id: 'InvertBattAmps_echo', transform: v => v == 1 ? 'Yes' : 'No' },
        { key: 'MaxDuty', id: 'MaxDuty_echo', transform: v => v },
        { key: 'MinDuty', id: 'MinDuty_echo', transform: v => v },
        { key: 'FieldResistance', id: 'FieldResistance_echo', transform: v => (v / 100).toFixed(2) },
        { key: 'maxPoints', id: 'maxPoints_echo', transform: v => v },
        { key: 'AlternatorCOffset', id: 'AlternatorCOffset_echo', transform: v => (v / 100).toFixed(2) },
        { key: 'BatteryCOffset', id: 'BatteryCOffset_echo', transform: v => (v / 100).toFixed(2) },
        { key: 'BatteryCapacity_Ah', id: 'BatteryCapacity_Ah_echo', transform: v => v },
        { key: 'R_fixed', id: 'R_fixed_echo', transform: v => (v / 100).toFixed(2) },
        { key: 'Beta', id: 'Beta_echo', transform: v => (v / 100).toFixed(2) },
        { key: 'T0_C', id: 'T0_C_echo', transform: v => (v / 100).toFixed(2) },
        { key: 'TempSource', id: 'TempSource_echo', transform: v => v == 0 ? 'Digital' : 'Thermistor' },
        { key: 'IgnitionOverride', id: 'IgnitionOverride_echo', transform: v => v == 1 ? 'On' : 'Off' },
        { key: 'AmpSensorRange', id: 'AmpSensorRange_echo', transform: v => (['±200A', '±300A', '±500A'][v] ?? v) },
        { key: 'AlarmLatchEnabled', id: 'AlarmLatchEnabled_echo', transform: v => v == 1 ? 'Enabled' : 'Disabled' },
        { key: 'AlarmTest', id: 'AlarmTest_echo', transform: v => v == 1 ? 'Active' : '---' },
        { key: 'MaintainMode', id: 'MaintainMode_echo', transform: v => v == 1 ? 'On' : 'Off' },
        { key: 'UseFloat', id: 'UseFloat_echo', transform: v => v == 1 ? 'Yes' : 'No' },
        { key: 'RebulkCurrent_A', id: 'RebulkCurrent_A_echo', transform: v => (v / 100).toFixed(2) },
        { key: 'TargetVoltageMode', id: 'TargetVoltageMode_echo', transform: v => v == 1 ? 'ON' : 'OFF' },
        { key: 'absorptionCompleteTime', id: 'absorptionCompleteTime_echo', transform: v => Math.round(v / 1000) },
        { key: 'FLOAT_DURATION', id: 'FLOAT_DURATION_echo', transform: v => (v / 3600).toFixed(2) },
        { key: 'AutoShuntGainCorrection', id: 'AutoShuntGainCorrection_echo', transform: v => v == 1 ? 'On' : 'Off' },
        { key: 'AutoAltCurrentZero', id: 'AutoAltCurrentZero_echo', transform: v => v == 1 ? 'On' : 'Off' },
        { key: 'WindingTempOffset', id: 'WindingTempOffset_echo', transform: v => Math.round(toDisplayTempDelta(v)) },
        { key: 'PulleyRatio', id: 'PulleyRatio_echo', transform: v => (v / 100).toFixed(2) },
        { key: 'ManualLifePercentage', id: 'ManualLifePercentage_echo', transform: v => v },
        { key: 'BatteryCurrentSource', id: 'BatteryCurrentSource_echo', transform: v => ({0: 'INA228 Shunt', 1: 'NMEA2K', 2: 'NMEA0183', 3: 'Victron VE.Direct'}[v] ?? v) },
        { key: 'timeAxisModeChanging', id: 'timeAxisModeChanging_echo', transform: v => v == 1 ? 'UNIX' : 'Elapsed' },
        { key: 'gpsTimeSourceMode', id: 'gpsTimeSourceMode_echo', transform: v => ({0:'Auto', 1:'NMEA only', 2:'Phone only', 3:'NTP time only'}[v] ?? '?') },
        { key: 'webgaugesinterval', id: 'webgaugesinterval_echo', transform: v => v },
        // plotTimeWindow has no text echo — its buttons highlight the active value instead (below)
        { key: 'weatherModeEnabled', id: 'weatherModeEnabled_echo', transform: v => v == 1 ? 'On' : 'Off' },
        { key: 'SolarWatts', id: 'SolarWatts_echo', transform: v => v },
        { key: 'performanceRatio', id: 'performanceRatio_echo', transform: v => (v / 100).toFixed(2) },
        { key: 'UVThresholdHigh', id: 'UVThresholdHigh_echo', transform: v => v },
        { key: 'TuningMode', id: 'TuningMode_echo', transform: v => v == 1 ? 'On' : 'Off' },
        { key: 'CloudFeatures', id: 'CloudFeatures_echo', transform: v => v == 1 ? 'On' : 'Off' },
        { key: 'PidKp', id: 'PidKp_echo', transform: v => (v / 1000).toFixed(3) },
        { key: 'PidKi', id: 'PidKi_echo', transform: v => (v / 1000).toFixed(3) },
        { key: 'PidKd', id: 'PidKd_echo', transform: v => (v / 1000).toFixed(3) },
        { key: 'DutySlowRampRate', id: 'DutySlowRampRate_echo', transform: v => (v / 100).toFixed(2) },
        { key: 'ShutdownPhase2HoldMs', id: 'ShutdownPhase2HoldMs_echo', transform: v => Math.round(v) },
        { key: 'PidSampleDivisor', id: 'PidSampleDivisor_echo', transform: v => v },
        { key: 'xTime', id: 'xTime_echo', transform: v => v },
        { key: 'MaxTableValue', id: 'MaxTableValue_echo', transform: v => (v / 100).toFixed(2) },
        { key: 'yyMax', id: 'yyMax_echo', transform: v => v },
        { key: 'VMGTargetBearing', id: 'VMGTargetBearing_echo', transform: v => v },
        { key: 'DutyRampRate', id: 'DutyRampRate_echo', transform: v => (v / 100).toFixed(2) },
        { key: 'SettleTimeBeforeCut', id: 'SettleTimeBeforeCut_echo', transform: v => Math.round(v) },
        { key: 'TempWarnExcess', id: 'TempWarnExcess_echo', transform: v => toDisplayTempDelta(v / 100).toFixed(1) },
        { key: 'TempCritExcess', id: 'TempCritExcess_echo', transform: v => toDisplayTempDelta(v / 100).toFixed(1) },
        { key: 'TempSustainedTimeout', id: 'TempSustainedTimeout_echo', transform: v => Math.round(v) },
        { key: 'AlternatorHardShutdownV', id: 'AlternatorHardShutdownV_echo', transform: v => (v / 100).toFixed(2) },
        { key: 'HardOCTripAmps', id: 'HardOCTripAmps_echo', transform: v => (v / 10).toFixed(1) },
        { key: 'HardOCDebounceMs', id: 'HardOCDebounceMs_echo', transform: v => Math.round(v) },
        { key: 'IExcessK', id: 'IExcessK_echo', transform: v => (v / 10).toFixed(1) },
        { key: 'IExcessN', id: 'IExcessN_echo', transform: v => Math.round(v) },
        { key: 'IExcessKBleed', id: 'IExcessKBleed_echo', transform: v => (v / 100).toFixed(2) },
        { key: 'IExcessArmMarginV', id: 'IExcessArmMarginV_echo', transform: v => v.toFixed(3) },
        { key: 'VoltageDisagreeThreshold', id: 'VoltageDisagreeThreshold_echo', transform: v => (v / 100).toFixed(2) },
        { key: 'VoltageDisagreeTimeout', id: 'VoltageDisagreeTimeout_echo', transform: v => Math.round(v) },
        { key: 'VoltageKp', id: 'VoltageKp_echo', transform: v => (v / 100).toFixed(2) },
        { key: 'VoltageLoopInterval', id: 'VoltageLoopInterval_echo', transform: v => Math.round(v) },
        { key: 'FIELD_COLLAPSE_DELAY', id: 'FIELD_COLLAPSE_DELAY_echo', transform: v => Math.round(v / 1000) },
        { key: 'hardwarePresent', id: 'HardwarePresent_echo', transform: v => v },
        { key: 'VoltageKi', id: 'VoltageKi_echo', transform: v => (v / 100).toFixed(2) },
        // VoltageKd echo removed — D term removed
        { key: 'SetpointRiseRate', id: 'SetpointRiseRate_echo', transform: v => (v / 100).toFixed(2) },
        { key: 'SetpointFallRate', id: 'SetpointFallRate_echo', transform: v => (v / 100).toFixed(2) },
        { key: 'PIDTrackingGain', id: 'PIDTrackingGain_echo', transform: v => (v / 100).toFixed(2) },
        { key: 'CAPSIZE_THRESHOLD_DEG', id: 'CAPSIZE_THRESHOLD_DEG_echo', transform: v => v },
        { key: 'PITCHPOLE_THRESHOLD_DEG', id: 'PITCHPOLE_THRESHOLD_DEG_echo', transform: v => v },
        { key: 'SLAM_THRESHOLD_G', id: 'SLAM_THRESHOLD_G_echo', transform: v => (v / 10).toFixed(1) },
        { key: 'socInfoAvailable', id: 'socInfoAvailable_echo', transform: v => v == 1 ? 'Yes' : 'No' },
        { key: 'TailCurrent_A', id: 'TailCurrent_A_echo', transform: v => (v / 100).toFixed(2) },
        { key: 'RebulkVoltage', id: 'RebulkVoltage_echo', transform: v => (v / 100).toFixed(2) },
        { key: 'rebulkDebounceTime', id: 'rebulkDebounceTime_echo', transform: v => Math.round(v / 1000) },
        { key: 'MinFloatTime', id: 'MinFloatTime_echo', transform: v => Math.round(v / 60000) },
        { key: 'SOC_BlockRebulk_percent', id: 'SOC_BlockRebulk_percent_echo', transform: v => v.toFixed(1) },
        { key: 'SOC_AllowRebulk_percent', id: 'SOC_AllowRebulk_percent_echo', transform: v => v.toFixed(1) },
        { key: 'TempPIDKp', id: 'TempPIDKp_echo', transform: v => (v / 1000).toFixed(3) },
        { key: 'TempPIDKi', id: 'TempPIDKi_echo', transform: v => (v / 1000).toFixed(3) },
        { key: 'ThermalLookaheadSec', id: 'ThermalLookaheadSec_echo', transform: v => v },
        { key: 'TempPIDIntervalMs', id: 'TempPIDIntervalMs_echo', transform: v => v },
        { key: 'TempPIDFilterAlpha', id: 'TempPIDFilterAlpha_echo', transform: v => (v / 1000).toFixed(3) },
        { key: 'AwBleedRate',       id: 'AwBleedRate_echo',       transform: v => (v / 10).toFixed(1) },
        { key: 'KHard',             id: 'KHard_echo',             transform: v => (v / 10).toFixed(1) },
        { key: 'ReseedFrac',        id: 'ReseedFrac_echo',        transform: v => (v / 100).toFixed(2) },
        { key: 'AwSeedProtectMs',   id: 'AwSeedProtectMs_echo',   transform: v => v },
        { key: 'FastSetpointRiseRate', id: 'FastSetpointRiseRate_echo', transform: v => (v / 100).toFixed(1) },
        { key: 'FastSetpointRiseWindowMs', id: 'FastSetpointRiseWindowMs_echo', transform: v => v },
        { key: 'FastSetpointRiseHeadroomV', id: 'FastSetpointRiseHeadroomV_echo', transform: v => (v / 100).toFixed(2) },
        { key: 'OvGroup1Enable',    id: 'OvGroup1Enable_echo',    transform: v => v == 1 ? 'ON' : 'OFF' },
        { key: 'OvGroup2Enable',    id: 'OvGroup2Enable_echo',    transform: v => v == 1 ? 'ON' : 'OFF' },
        { key: 'TdPred',            id: 'TdPred_echo',            transform: v => v.toFixed(3) },
        { key: 'OvMeasMarginV',     id: 'OvMeasMarginV_echo',     transform: v => v.toFixed(3) },
        { key: 'OvPredMarginV',     id: 'OvPredMarginV_echo',     transform: v => v.toFixed(3) },
        { key: 'IExcessSigSrc',       id: 'IExcessSigSrc_echo',       transform: v => (['MA(N)', 'EMA(TC)', 'Raw'][v] ?? v) },
        { key: 'IExcessMA_N',         id: 'IExcessMA_N_echo',         transform: v => Math.round(v) },
        { key: 'OutputPIDSigSrc',     id: 'OutputPIDSigSrc_echo',     transform: v => (['EMA(TC)', 'MA(N)', 'Raw'][v] ?? v) },
        { key: 'OutputPIDMA_N',       id: 'OutputPIDMA_N_echo',       transform: v => Math.round(v) },
        { key: 'OutputPIDFilterTC',   id: 'OutputPIDFilterTC_echo',   transform: v => v },
        { key: 'VoltageFilterTC',     id: 'VoltageFilterTC_echo',     transform: v => v },
        { key: 'VoltageFilterTC',     id: 'VoltageFilterTC_echo_cv',  transform: v => v },
        { key: 'AbsorptionVoltage', id: 'AbsorptionVoltage_echo', transform: v => (v / 100).toFixed(2) },
        { key: 'TargetVoltageSetpoint', id: 'TargetVoltageSetpoint_echo', transform: v => (v / 100).toFixed(2) },
        { key: 'AbsorptionTimeoutMs', id: 'AbsorptionTimeoutMs_echo', transform: v => Math.round(v / 60000) },
        { key: 'bulkVoltageHoldMs', id: 'bulkVoltageHoldMs_echo', transform: v => (v / 1000).toFixed(2) },
        { key: 'capLimitMode', id: 'capLimitMode_echo', transform: v => v },
        { key: 'InputFilterTC', id: 'InputFilterTC_echo',      transform: v => v },
        { key: 'InputFilterTC', id: 'InputFilterTC_ID',        transform: v => v },
        { key: 'InputFilterTC', id: 'InputFilterTC_echo_grp3', transform: v => v },
        { key: 'OutputPIDFilterTC', id: 'OutputPIDFilterTC_echo_pid', transform: v => v },
        { key: 'SlopeBleedThresh',      id: 'SlopeBleedThresh_echo',          transform: v => (v / 100).toFixed(2) },
        { key: 'SlopeBleedK',           id: 'SlopeBleedK_echo',               transform: v => v },
        { key: 'DvdtTC',                id: 'DvdtTC_echo',                    transform: v => (v / 10).toFixed(1) },
        { key: 'SlopeBleedProxV',       id: 'SlopeBleedProxV_echo',           transform: v => (v / 100).toFixed(2) },
        { key: 'StartupRiseRate',       id: 'StartupRiseRate_echo',           transform: v => (v / 100).toFixed(2) },
        { key: 'SystemIDStepAmplitude', id: 'SystemIDStepAmplitude_echo', transform: v => (v / 10).toFixed(1) },
        { key: 'WarmupRampRate', id: 'WarmupRampRate_echo', transform: v => (v / 10).toFixed(1) },
        { key: 'CVTuningMode',      id: 'CVTuningMode_echo',      transform: v => v == 1 ? 'On' : 'Off' },
        { key: 'cvWaveAmplitudeV',  id: 'cvWaveAmplitudeV_echo',  transform: v => (v / 100).toFixed(2) },
        { key: 'cvWavePeriodSec',   id: 'cvWavePeriodSec_echo',   transform: v => v },
        { key: 'cvKOvershoot',      id: 'cvKOvershoot_echo',      transform: v => (v / 10).toFixed(1) },
        { key: 'cvConsecutiveReads', id: 'cvConsecutiveReads_echo', transform: v => v },
        { key: 'ThermalTuningMode',         id: 'ThermalTuningMode_echo',         transform: v => v == 1 ? 'On' : 'Off' },
        { key: 'thermalWaveLowF',           id: 'thermalWaveLowF_echo',           transform: v => (v / 10).toFixed(1) },
        { key: 'thermalWaveHighF',          id: 'thermalWaveHighF_echo',          transform: v => (v / 10).toFixed(1) },
        { key: 'thermalWaveHalfPeriodMin',  id: 'thermalWaveHalfPeriodMin_echo',  transform: v => (v / 10).toFixed(1) },
        { key: 'thermalKOvershoot',         id: 'thermalKOvershoot_echo',         transform: v => (v / 100).toFixed(2) },
        { key: 'thermalKUndershoot',        id: 'thermalKUndershoot_echo',        transform: v => (v / 100).toFixed(2) },
        { key: 'thermalSettleThreshF',      id: 'thermalSettleThreshF_echo',      transform: v => (v / 10).toFixed(1) },
        { key: 'thermalConsecutiveReads',   id: 'thermalConsecutiveReads_echo',   transform: v => v },
        { key: 'wifiNapEnabled',            id: 'wifiNapEnabled_echo',            transform: v => v == 1 ? 'On' : 'Off' },
        // Fast alt-current diagnostic knob echoes (Pattern B). Scales mirror the SafeInt() factors in firmware.
        { key: 'faEnabled',                 id: 'faEnabled_echo',                 transform: v => v == 1 ? 'On' : 'Off' },
        { key: 'faAlarmEnable',             id: 'faAlarmEnable_echo',             transform: v => v == 1 ? 'On' : 'Off' },
        { key: 'faRpmEdgeMargin',           id: 'faRpmEdgeMargin_echo',           transform: v => (v / 10) },
        { key: 'faAmpsDriftFloorA',         id: 'faAmpsDriftFloorA_echo',         transform: v => (v / 100) },
        { key: 'faAmpsDriftPct',            id: 'faAmpsDriftPct_echo',            transform: v => (v / 10) },
        { key: 'faAttenUpAmps',             id: 'faAttenUpAmps_echo',             transform: v => (v / 10) },
        { key: 'faAttenDownAmps',           id: 'faAttenDownAmps_echo',           transform: v => (v / 10) },
        { key: 'faPeakMinA',                id: 'faPeakMinA_echo',                transform: v => (v / 100) },

    ];

    // Special display elements (not echos but similar update pattern)
    const specialDisplays = [
        { key: 'totalPowerCycles', id: 'totalPowerCyclesID', transform: v => v },
        {
            key: 'AlarmLatchState',
            id: 'AlarmLatchState_display',
            transform: v => v,
            special: (el, value) => {
                if (value === 1) {
                    el.textContent = 'LATCHED';
                    el.style.color = '#ff0000';
                } else {
                    el.textContent = 'Normal';
                    el.style.color = 'var(--accent)';
                }
            }
        }
    ];

    // Update only changed echo values
    echoUpdates.forEach(update => {
        if (update.key in data) {
            const newValue = update.transform(data[update.key]);
            if (updateEchoIfChanged(update.id, newValue)) {
                updatesCount++;
            }
        }
    });

    // Sync <select> elements to current firmware values
    const selectSyncs = [
        { key: 'AmpSensorRange',        name: 'AmpSensorRange' },
        { key: 'gpsTimeSourceMode',     name: 'gpsTimeSourceMode' },
    ];
    selectSyncs.forEach(({ key, name }) => {
        if (key in data) {
            const sel = document.querySelector(`select[name="${name}"]`);
            if (sel && sel.value !== String(data[key])) sel.value = String(data[key]);
        }
    });

    // BatteryCurrentSource is a segmented A/B control (not a <select>)
    if ('BatteryCurrentSource' in data) syncSegmentedSelect('BatteryCurrentSource', data.BatteryCurrentSource);

    // Time-window buttons: the current setting is shown by highlighting its button
    // (replaces the old "(current: N s)" text echo).
    if ('plotTimeWindow' in data) {
        document.querySelectorAll('.time-window-input button[name="plotTimeWindow"]').forEach(b => {
            b.classList.toggle('active', Number(b.value) === Number(data.plotTimeWindow));
        });
    }

    // Update special displays
    specialDisplays.forEach(display => {
        if (display.key in data) {
            const el = document.getElementById(display.id);
            if (el) {
                if (display.special) {
                    display.special(el, data[display.key]);
                    updatesCount++;
                } else {
                    const newValue = display.transform(data[display.key]);
                    if (updateEchoIfChanged(display.id, newValue)) {
                        updatesCount++;
                    }
                }
            }
        }
    });

    return updatesCount;
};


// end axes configuration functions
let rpmAmpsInitialized = false; // this is for the RPM/AMP speed based charging table
// for faster wifi reconnects after scheduled shut downs:
let reconnectInterval;
let reconnectionAttempts = 0;
const maxReconnectionAttempts = 15; // Try for 30 seconds (15 attempts × 2 seconds)
// Connection monitoring variables
let lastEventTime = Date.now();
const connectionCheckInterval = 10000; // Check every 10 seconds

// to prevent double toggling
let toggleStates = {}; // Track current toggle states
let userInitiatedToggles = new Map();
let lastValues = new Map(); // Cache for DOM updates
// DOM update batching system
let pendingDOMUpdates = new Map();
let lastSeenRev = 0;
let pendingToggles = new Map(); // {dataKey: {desiredValue, baseRev}}
let domUpdateScheduled = false;


//feedback on button clicking
function submitMessage() {
    // Use 'this' which refers to the element that called the function
    if (this && this.classList) {
        this.classList.add('button-pulse');
        setTrackedTimeout(() => this.classList.remove('button-pulse'), 600);
    }
}

//this allows user to turn the alternator OFF even without entering a passoword, for safety reasons, but not ON
function handleAlternatorToggle(checkbox) {
    const isGoingOn = checkbox.checked; // true = turning ON, false = turning OFF
    const checkboxId = checkbox.id;
    const isLocked = !currentAdminPassword; // System is locked if no password

    // Always allow turning OFF (safety feature)
    if (!isGoingOn) {
        if (handleUserToggle(checkboxId, 'OnOff', 'OnOff')) {
            return true; // Allow form submission
        }
    }

    // For turning ON, require unlocked system
    if (isGoingOn && isLocked) {
        // alert("Settings must be unlocked to turn alternator ON");   // i find this intrusive
        checkbox.checked = false; // Revert the toggle
        return false; // Prevent form submission
    }

    // System is unlocked and we're turning ON, or we're turning OFF - proceed normally
    return handleUserToggle(checkboxId, 'OnOff', 'OnOff');
}
//something to do with passwords
function togglePassword(inputId) {
    const input = document.getElementById(inputId);
    const checkbox = event.target;
    input.type = checkbox.checked ? 'text' : 'password';
}
//password hiding stuff
function hideSettingsAccess() {
    document.getElementById('settings-access-section').style.display = 'none';
}
function showSettingsAccess() {
    document.getElementById('settings-access-section').style.display = 'block';
}
function lockSettingsManually() {
    const section = document.getElementById('settings-section');
    if (section) section.classList.add("locked");
    // DON'T lock the header control - we want to allow turning OFF the alternator
    // const headerControl = document.querySelector('.alternator-control');
    // if (headerControl) headerControl.classList.add("locked");
    showSettingsAccess();
    const lockStatus = document.getElementById('lock-status');
    if (lockStatus) {
        lockStatus.textContent = "Settings are Locked";
        lockStatus.className = "lock-status-locked";
    }
    currentAdminPassword = "";
}
// Touch support: tap once to show tooltip, tap elsewhere to hide, for tooltips tool tip tooltip tips
document.addEventListener("click", function (e) {
    document.querySelectorAll(".tooltip").forEach(el => el.classList.remove("active"));
    const tip = e.target.closest(".tooltip");
    if (tip) {
        tip.classList.add("active");
        e.stopPropagation();
    }
});



const resetReasonLookup = {
    0: "Power-on (plugged in)",
    1: "Software reset (unscheduled)",
    2: "Woke from deep sleep",
    3: "External reset (button)",
    4: "Task watchdog (loop blocked)",  // ← More accurate
    5: "Panic/Exception (crash)",       // ← This also gets backtraces
    6: "Brownout (power issue)",
    7: "Interrupt watchdog",
    8: "Unknown reset",
    9: "Other watchdog",
    10: "SDIO reset",
    11: "Scheduled maintenance restart"
};




function debounce(fn, delay) {
    let timer = null;
    return function (...args) {
        clearTimeout(timer);
        timer = setTrackedTimeout(() => fn.apply(this, args), delay);
    };
}

//UPDATE LATER
let currentAdminPassword = "";
//let currentAdminPassword = "admin"; // TEMP: bypass for local testing


function updatePasswordFields() {
    const password = currentAdminPassword || document.getElementById("admin_password").value;
    const passwordFields = document.querySelectorAll('.password_field');
    passwordFields.forEach(field => {
        field.value = password;
    });
}

// ============================================
// CLOUD FEATURES - Profile Management
// ============================================

async function initializeProfileTab() {
    if (!currentAdminPassword) {
        diagLog("No password, returning early");
        return;
    }
    const formData = new FormData();
    formData.append('password', currentAdminPassword);

    // Show "Checking..." banner so the UI isn't blank while the cloud round-trip is in flight.
    const banner = document.getElementById('profile-loading-banner');
    const bannerText = document.getElementById('profile-loading-text');
    if (banner) {
        banner.style.display = 'block';
        banner.style.background = '#e3f2fd';
        banner.style.color = '#1976d2';
        if (bannerText) bannerText.textContent = 'Checking cloud registration…';
    }

    try {
        const response = await fetchWithTimeout(buildURL('/checkRegistration'), {
            method: 'POST',
            body: formData
        }, 8000);

        if (!response.ok) throw new Error(`HTTP ${response.status}`);

        const data = await response.json();

        if (data.registered && data.valid) {
            isDeviceRegistered = true; // Device already registered
            try { sessionStorage.setItem('deviceRegistered', '1'); } catch (e) {} // cache for session — survives reloads
            populateProfileForm(data.profile);
            document.getElementById('profile-form').querySelector('input[type="submit"]').value = 'Update Profile';
        } else {
            document.getElementById('profile-form').querySelector('input[type="submit"]').value = 'Register Device';
        }
        if (banner) banner.style.display = 'none';
    } catch (error) {
        diagError('Error in initializeProfileTab:', error);
        if (banner && bannerText) {
            banner.style.background = '#ffebee';
            banner.style.color = '#c62828';
            bannerText.textContent = 'Could not reach cloud — check WiFi and try again.';
            // Leave the error message visible; next tab open will reset it.
        }
    }
}

// Fetch vessel info from ESP32 and populate form
async function fetchAndPopulateVesselInfo() {
    try {
        const response = await fetchWithTimeout(buildURL('/vessel_info.json'), {}, 5000);
        if (!response.ok) {

            // UPDATE LATER comment below out for local file previewing ability
            vesselInfoComplete = false;

            return;
        }

        const data = await response.json();
        window.vesselInfo = data;

        const form = document.getElementById('vessel-info-form');
        if (!form) return;

        form.BOAT_LENGTH_FT.value = data.boat_length_ft || '';
        form.BOAT_TYPE.value = data.boat_type || 'monohull';
        form.BOAT_MAKE_MODEL.value = data.boat_make_model || '';
        form.BOAT_YEAR.value = data.boat_year || new Date().getFullYear();
        form.HOME_PORT.value = data.home_port || '';  // ← ADD THIS LINE
        form.ENGINE_MAKE.value = data.engine_make || '';
        form.ENGINE_HP.value = data.engine_hp || '';
        form.BATTERY_VOLTAGE.value = data.battery_voltage || 12;
        form.BATTERY_CAPACITY_AH.value = data.battery_capacity_ah || '';
        form.BATTERY_TYPE.value = data.battery_type || '';
        form.ALTERNATOR_BRAND_MODEL.value = data.alternator_brand_model || '';
        form.SOLAR_WATTS.value = data.solar_watts || '';

        const orientationRadios = form.querySelectorAll('input[name="imuMountOrientation"]');
        orientationRadios.forEach(radio => {
            if (parseInt(radio.value) === data.imu_mount_orientation) {
                radio.checked = true;
            }
        });

        form.IMU_DIST_BOW_FT.value = data.imu_dist_bow_ft || '';
        form.IMU_DIST_CL_FT.value = data.imu_dist_cl_ft || '';
        form.IMU_HEIGHT_WL_FT.value = data.imu_height_wl_ft || '';

        // Check if all required fields are filled
        const allFieldsFilled =
            data.boat_length_ft &&
            data.boat_type &&
            data.boat_make_model &&
            data.boat_year &&
            data.home_port &&  // ← ADD THIS LINE TOO
            data.engine_make !== undefined &&
            data.engine_hp !== undefined &&
            data.battery_voltage &&
            data.battery_capacity_ah &&
            data.battery_type &&
            data.alternator_brand_model !== undefined &&
            data.solar_watts !== undefined &&
            data.imu_mount_orientation !== undefined &&
            data.imu_dist_bow_ft !== undefined &&
            data.imu_dist_cl_ft !== undefined &&
            data.imu_height_wl_ft !== undefined;

        vesselInfoComplete = allFieldsFilled;
        if (allFieldsFilled) setVesselButtonUpdateMode();

    } catch (error) {
        diagLog('Vessel info not found or invalid:', error);
    } finally {
        window._vesselInfoLoaded = true;
        maybeApplyLanding();
    }
}

// Flip the Save button to "Update Vessel Info" and reveal its tooltip once info is on file.
function setVesselButtonUpdateMode() {
    const btn = document.getElementById('saveVesselInfoBtn');
    if (btn) btn.value = 'Update Vessel Info';
    const tip = document.getElementById('vesselUpdateTooltip');
    if (tip) tip.style.display = 'inline-flex';
}

// Handle vessel info form submission
async function handleVesselInfoSave(event) {
    event.preventDefault();

    const form = document.getElementById('vessel-info-form');
    const messageDiv = document.getElementById('vessel-info-message');

    // Get selected radio value
    const selectedOrientation = form.querySelector('input[name="imuMountOrientation"]:checked');
    if (!selectedOrientation) {
        alert('Please select a mounting orientation');
        return;
    }

    // Build vessel data object
    const vesselData = {
        boat_length_ft: parseFloat(form.BOAT_LENGTH_FT.value),
        boat_type: form.BOAT_TYPE.value,
        boat_make_model: form.BOAT_MAKE_MODEL.value,
        boat_year: parseInt(form.BOAT_YEAR.value),
        home_port: form.HOME_PORT.value,  // ← ADD THIS LINE
        engine_make: form.ENGINE_MAKE.value,
        engine_hp: parseInt(form.ENGINE_HP.value),
        battery_voltage: parseInt(form.BATTERY_VOLTAGE.value),
        battery_capacity_ah: parseInt(form.BATTERY_CAPACITY_AH.value),
        battery_type: form.BATTERY_TYPE.value,
        alternator_brand_model: form.ALTERNATOR_BRAND_MODEL.value,
        solar_watts: parseInt(form.SOLAR_WATTS.value),
        imu_mount_orientation: parseInt(selectedOrientation.value),
        imu_dist_bow_ft: parseFloat(form.IMU_DIST_BOW_FT.value),
        imu_dist_cl_ft: parseFloat(form.IMU_DIST_CL_FT.value),
        imu_height_wl_ft: parseFloat(form.IMU_HEIGHT_WL_FT.value)
    };

    messageDiv.style.display = 'block';
    messageDiv.style.backgroundColor = '#e3f2fd';
    messageDiv.style.color = '#1976d2';
    messageDiv.textContent = 'Saving vessel info...';

    try {
        // Save to ESP32
        const formData = new FormData();
        formData.append('password', currentAdminPassword);
        formData.append('vesselData', JSON.stringify(vesselData));

        const response = await fetchWithTimeout(buildURL('/saveVesselInfo'), {
            method: 'POST',
            body: formData
        }, 8000);

        if (!response.ok) throw new Error(`HTTP ${response.status}`);

        const result = await response.json();

        if (result.success) {
            window.vesselInfo = vesselData;
            vesselInfoComplete = true;
            setVesselButtonUpdateMode();  // first save → becomes "Update Vessel Info" + tooltip

            messageDiv.style.backgroundColor = '#e8f5e9';
            messageDiv.style.color = '#2e7d32';
            messageDiv.innerHTML = 'Vessel info saved successfully! You can now access other tabs.<br>To push this profile to the cloud, go to Cloud Features &rarr; Registration (cloud features are on by default).';

            // Update dual-control echoes immediately
            updateEchoIfChanged('SolarWatts_echo', vesselData.solar_watts);
            updateEchoIfChanged('BatteryCapacity_Ah_echo', vesselData.battery_capacity_ah);

        } else {
            throw new Error(result.error || 'Save failed');
        }

    } catch (error) {
        messageDiv.style.backgroundColor = '#ffebee';
        messageDiv.style.color = '#c62828';
        messageDiv.textContent = 'Error: ' + error.message;
    }
}

function populateProfileForm(profile) {
    document.getElementById('profile-username').value = profile.username || '';
    document.getElementById('profile-email').value = profile.email || '';
}


// Handle profile update/registration form submission
// Profile Update Flow:
// First time: /getAuthToken returns registered:false → calls /registerProfile → register-device edge function creates account & token
// Updates: /getAuthToken returns registered:true → calls /updateProfile → update-profile edge function validates token & updates profile
function handleProfileUpdate(event) {
    event.preventDefault();

    if (!currentAdminPassword) {
        alert("Please unlock settings first");
        return;
    }

    updatePasswordFields();

    const form = document.getElementById('profile-form');
    const messageDiv = document.getElementById('profile-message');
    const formData = new FormData(form);
    formData.append('password', currentAdminPassword);

    // Add deviceUID from ESP32
    const deviceUidSpan = document.getElementById('profile-device-uid');
    if (deviceUidSpan) {
        formData.append('deviceUID', deviceUidSpan.textContent);
    }

    // Add all vessel data from cache
    if (window.vesselInfo) {
        Object.keys(window.vesselInfo).forEach(key => {
            formData.append(key, window.vesselInfo[key]);
        });
    }

    messageDiv.style.display = 'block';
    messageDiv.style.backgroundColor = '#e3f2fd';
    messageDiv.style.color = '#1976d2';
    messageDiv.textContent = 'Saving profile...';

    fetchWithTimeout(buildURL('/getAuthToken'), {}, 8000)
        .then(response => {
            if (!response.ok) throw new Error(`HTTP ${response.status}`);
            return response.json();
        })
        .then(data => {
            const endpoint = data.registered ? buildURL('/updateProfile') : buildURL('/registerProfile');
            return fetchWithTimeout(endpoint, {
                method: 'POST',
                body: formData
            }, 8000);
        })
        // Text-first parse: cloud sends JSON {success,error} on 4xx/409, but the firmware
        // can also reply with plain text (403 "Forbidden" on bad password, empty body on
        // 502/503). Reading text and tolerating a parse failure surfaces every error path
        // instead of throwing a misleading "Network error" on non-JSON replies.
        .then(response => response.text().then(text => {
            let data = {};
            try { data = text ? JSON.parse(text) : {}; } catch (e) { data = {}; }
            return { httpStatus: response.status, data, raw: text };
        }))
        .then(({ httpStatus, data, raw }) => {
            if (data.success) {
                messageDiv.style.backgroundColor = '#e8f5e9';
                messageDiv.style.color = '#2e7d32';
                messageDiv.textContent = 'Profile saved successfully!';
                isDeviceRegistered = true;
                try { sessionStorage.setItem('deviceRegistered', '1'); } catch (e) {} // cache for session — survives reloads
            } else {
                messageDiv.style.backgroundColor = '#ffebee';
                messageDiv.style.color = '#c62828';
                // Prefer the cloud's error string; fall back to raw body (e.g. "Forbidden") then status
                messageDiv.textContent = 'Error: ' + (data.error || (raw && raw.trim()) || `HTTP ${httpStatus}`);
            }
        })
        .catch(err => {
            messageDiv.style.backgroundColor = '#ffebee';
            messageDiv.style.color = '#c62828';
            messageDiv.textContent = 'Network error: ' + err.message;
        });
}

// Handle delete all data button
function handleDeleteAllData() {
    if (!currentAdminPassword) {
        alert("Please unlock settings first");
        return;
    }

    if (!confirm('WARNING: This permanently deletes your cloud account and ALL associated cloud data including history, profile, and statistics. This CANNOT be undone.\n\nYour hardware device will continue working locally, but all cloud features are reset.\n\nType DELETE in the next prompt to confirm.')) {
        return;
    }

    const confirmation = prompt('Type DELETE to confirm:');
    if (confirmation !== 'DELETE') {
        alert('Deletion cancelled');
        return;
    }

    updatePasswordFields();
    const formData = new FormData();
    formData.append('password', currentAdminPassword);

    fetchWithTimeout(buildURL('/deleteAllData'), {
        method: 'POST',
        body: formData
    }, 8000)
        // Text-first parse so a non-2xx cloud reply (e.g. 409, or a 403 "Forbidden" from the
        // firmware) surfaces its real error string instead of being swallowed as a bare status.
        .then(response => response.text().then(text => {
            let data = {};
            try { data = text ? JSON.parse(text) : {}; } catch (e) { data = {}; }
            return { httpStatus: response.status, data, raw: text };
        }))
        .then(({ httpStatus, data, raw }) => {
            if (data.success) {
                alert('Your account and all cloud data were deleted. Your device keeps working locally; cloud features have been reset.');
                location.reload();
            } else {
                alert('Error: ' + (data.error || (raw && raw.trim()) || `HTTP ${httpStatus}` || 'Deletion failed'));
            }
        })
        .catch(err => {
            alert('Network error: ' + err.message);
        });
}

function resetThermalPID() {
    if (!confirm('Reset the temperature controller? Its integrator and filter will be cleared and rebuilt.')) return;
    fetch(buildURL('/resetThermalPID'), { method: 'POST' })
        .then(r => r.ok ? console.log('Thermal PID reset') : console.warn('Reset failed'))
        .catch(err => console.warn('Reset error:', err));
}

function resetInnerPID() {
    if (!confirm('Reset the output current controller? Its integrator will be zeroed and field output will ramp back up from zero under the slew limit.')) return;
    fetch(buildURL('/resetInnerPID'), { method: 'POST' })
        .then(r => r.ok ? console.log('Output current PID reset') : console.warn('Reset failed'))
        .catch(err => console.warn('Reset error:', err));
}

function resetVoltageLoop() {
    if (!confirm('Reset the voltage controller integrator? The voltage loop will rebuild from zero.')) return;
    fetch(buildURL('/resetVoltageLoop'), { method: 'POST' })
        .then(r => r.ok ? console.log('Voltage loop reset') : console.warn('Reset failed'))
        .catch(err => console.warn('Reset error:', err));
}

// ============================================================================
// PID TUNING SCORE LOG
// ============================================================================

let _tuningLogPollTimer = null;

function fetchTuningLog() {
    fetch(buildURL('/tuninglog'))
        .then(r => r.ok ? r.json() : null)
        .then(data => { if (data) renderTuningLog(data); })
        .catch(() => {});
}

function renderTuningLog(data) {
    // Update live score displays (Settings panel + Live Data → Alternator mirror)
    const liveLabels = ['1m', '10m', '100m', '1000m'];
    (data.live || []).forEach((v, i) => {
        const val = v > 0 ? v.toFixed(2) : '—';
        const el = document.getElementById('liveScore' + i);
        if (el) el.textContent = liveLabels[i] + ': ' + val;
        const elAlt = document.getElementById('liveScoreAlt' + i);
        if (elAlt) elAlt.textContent = val;   // Diag row header already lists the windows — no per-value prefix
    });

    // Active test score
    const testRow = document.getElementById('testScoreRow');
    if (testRow) {
        if (data.ta) {
            testRow.style.display = '';
            const tsel = document.getElementById('currentTestScore');
            const ttgl = document.getElementById('testToggleCount');
            if (tsel) tsel.textContent = data.ts > 0 ? data.ts.toFixed(2) : '—';
            if (ttgl) ttgl.textContent = data.tt;
            // Push live score into the floating test panel
            if (_testPanelCurrentTest === 'curr') {
                const scored = Math.max(0, (data.tt || 0) - 4);
                updateTestPanelScore(data.ts, scored + ' scored',
                    (data.tt || 0) < 4 ? 'Ring-in (' + (data.tt || 0) + '/4)' : 'Scoring');
            }
        } else {
            testRow.style.display = 'none';
        }
    }

    // Commit section: show when TuningMode is active, update progress
    const commitSection = document.getElementById('tuningCommitSection');
    if (commitSection) {
        commitSection.style.display = data.ta ? '' : 'none';
        if (data.ta) {
            const scored  = Math.max(0, (data.tt || 0) - 4);   // ring-in is first 4 toggles
            const pct     = Math.min(100, (scored / 4) * 100);
            const ready   = scored >= 4;
            const cyclesEl = document.getElementById('tuningCyclesCount');
            const barEl    = document.getElementById('tuningCyclesBar');
            const badgeEl  = document.getElementById('tuningReadyBadge');
            const btnEl    = document.getElementById('commitTuningBtn');
            if (cyclesEl) cyclesEl.textContent = scored;
            if (barEl)    barEl.style.width = pct.toFixed(0) + '%';
            if (badgeEl)  { badgeEl.textContent = ready ? 'Ready to commit' : 'waiting…';
                            badgeEl.style.color  = ready ? '#22c55e' : '#888'; }
            if (btnEl)    btnEl.disabled = !ready;
        }
    }

    // Current settings for row highlighting
    const curKp  = parseFloat(document.getElementById('PidKp_echo')?.textContent);
    const curKi  = parseFloat(document.getElementById('PidKi_echo')?.textContent);
    const curKd  = parseFloat(document.getElementById('PidKd_echo')?.textContent);
    const curAmp = parseInt(document.getElementById('waveAmplitude_echo')?.textContent);
    const curPer = parseInt(document.getElementById('wavePeriod_echo')?.textContent);

    const records = (data.rec || []).slice();  // already sorted best-first by firmware
    const tbody = document.getElementById('tuningLogBody');
    if (!tbody) return;

    tbody.innerHTML = records.map(r => {
        const scoreColor = r.s < 2 ? '#22c55e' : r.s < 6 ? '#eab308' : '#ef4444';
        const isMatch = !isNaN(curKp) &&
            Math.abs(r.kp - curKp) < 0.0001 &&
            Math.abs(r.ki - curKi) < 0.0001 &&
            Math.abs(r.kd - curKd) < 0.00001 &&
            r.wa === curAmp && r.wp === curPer;
        const rowStyle = isMatch ? 'background:rgba(99,102,241,0.18);' : '';

        return `<tr style="${rowStyle}">
            <td style="padding:2px 5px;">${r.n}</td>
            <td style="padding:2px 5px;color:${scoreColor};font-weight:bold;">${r.s.toFixed(2)}</td>
            <td style="padding:2px 5px;">${r.kp.toFixed(3)}</td>
            <td style="padding:2px 5px;">${r.ki.toFixed(3)}</td>
            <td style="padding:2px 5px;">${r.kd.toFixed(4)}</td>
            <td style="padding:2px 5px;">${r.sd}</td>
            <td style="padding:2px 5px;">${r.tg.toFixed(2)}</td>
            <td style="padding:2px 5px;">${r.dr.toFixed(1)}</td>
            <td style="padding:2px 5px;">${r.wa}</td>
            <td style="padding:2px 5px;">${r.wp}</td>
            <td style="padding:2px 5px;">${r.rpm.toFixed(0)}</td>
            <td style="padding:2px 5px;">${r.temp.toFixed(1)}</td>
            <td style="padding:2px 5px;">${r.worst.toFixed(1)}</td>
            <td style="padding:2px 5px;">${r.t.toFixed(0)}</td>
        </tr>`;
    }).join('');
}

function commitTuningScore() {
    const btn = document.getElementById('commitTuningBtn');
    const status = document.getElementById('tuningCommitStatus');
    if (btn) btn.disabled = true;
    if (status) status.textContent = 'Sending…';
    const pw = currentAdminPassword || '';
    // Note: r.ok must be checked — firmware returns 403 when the dashboard is
    // locked (empty/wrong password) before the commit flag is ever set.
    fetch(buildURL('/get?commitTuningScore=1&password=' + encodeURIComponent(pw)))
        .then(r => {
            if (!r.ok) {
                if (btn) btn.disabled = false;
                if (status) status.textContent = (r.status === 403)
                    ? 'Rejected — dashboard locked. Unlock first.'
                    : ('Rejected — HTTP ' + r.status);
                return;
            }
            if (status) status.textContent = 'Committed — check Score Log.';
            setTimeout(() => { if (status) status.textContent = ''; }, 4000);
            fetchTuningLog();
        })
        .catch(() => {
            if (btn) btn.disabled = false;
            if (status) status.textContent = 'Send failed.';
        });
}

function resetTuningLog() {
    if (!confirm('Reset all tuning scores and live windows?')) return;
    fetch(buildURL('/resettuninglog'), { method: 'POST' })
        .then(() => fetchTuningLog())
        .catch(() => {});
}

function commitCVTuningScore() {
    const btn = document.getElementById('commitCVTuningBtn');
    const status = document.getElementById('cvTuningCommitStatus');
    if (btn) btn.disabled = true;
    if (status) status.textContent = 'Sending…';
    const pw = currentAdminPassword || '';
    // Note: r.ok must be checked — firmware returns 403 when the dashboard is
    // locked (empty/wrong password) before the commit flag is ever set.
    fetch(buildURL('/get?commitCVTuningScore=1&password=' + encodeURIComponent(pw)))
        .then(r => {
            if (!r.ok) {
                if (btn) btn.disabled = false;
                if (status) status.textContent = (r.status === 403)
                    ? 'Rejected — dashboard locked. Unlock first.'
                    : ('Rejected — HTTP ' + r.status);
                return;
            }
            if (status) status.textContent = 'Committed — check Score Log.';
            setTimeout(() => { if (status) status.textContent = ''; }, 4000);
            fetchCVTuningLog();
        })
        .catch(() => {
            if (btn) btn.disabled = false;
            if (status) status.textContent = 'Send failed.';
        });
}

function restartCVTest() {
    const status = document.getElementById('cvTuningCommitStatus');
    if (status) status.textContent = 'Restarting…';
    const pw = currentAdminPassword || '';
    fetch(buildURL('/get?restartCVTest=1&password=' + encodeURIComponent(pw)))
        .then(r => {
            if (!r.ok) {
                if (status) status.textContent = (r.status === 403)
                    ? 'Rejected — dashboard locked. Unlock first.'
                    : ('Rejected — HTTP ' + r.status);
                return;
            }
            if (status) status.textContent = 'Restarted.';
            setTimeout(() => { if (status) status.textContent = ''; }, 3000);
            fetchCVTuningLog();
        })
        .catch(() => {
            if (status) status.textContent = 'Send failed.';
        });
}

// Boat loading splash: the inline script in index.html shows it immediately on a
// Capacitor cold launch (themed, flash-free). It hides on the first CSVData packet
// (CSVData listener below) or when demo mode starts. hideWaitingForRegulator() is
// idempotent and fades the overlay out. The safety timeout below guarantees it can
// never stick if neither real nor demo telemetry ever arrives.
window._firstCsvPacketReceived = false;
function hideWaitingForRegulator() {
    const el = document.getElementById('waiting-for-regulator');
    if (!el || el._wfrHidden) return;
    el._wfrHidden = true;
    el.classList.add('wfr-hiding');
    setTimeout(() => { el.style.display = 'none'; }, 480);
}

// Landing tab, applied once on open: field on → Alternator, field off → Boat Performance
// (Motoring plot if engine running); Vessel Info incomplete overrides to Settings.
function applyLandingTab() {
    try {
        if (!vesselInfoComplete) {
            showMainTab('settings');
            showSubTab('settings', 'vessel-info');
            return;
        }
        const d = window._debugData || {};
        const fieldOn = Number(d.fieldActiveStatus) !== 0;  // OFF (0) is the only "field off" state
        showMainTab('livedata');
        if (fieldOn) {
            showSubTab('livedata', 'alternator');
        } else {
            showSubTab('livedata', 'boatperformance');
            const rpm = Number(d.RPM);
            if (isFinite(rpm) && rpm > 50 && typeof setPerfView === 'function') {
                setPerfView(1);  // engine running → Motoring view
            }
        }
    } catch (err) {}
}

// Decide only when both signals are ready: first telemetry packet (field/engine) and the
// vessel-info fetch (vesselInfoComplete). Guards the fetch-vs-packet race; runs once.
window._vesselInfoLoaded = false;
window._landingApplied = false;
function maybeApplyLanding() {
    if (window._landingApplied) return;
    if (!window._firstCsvPacketReceived || !window._vesselInfoLoaded) return;
    window._landingApplied = true;
    applyLandingTab();
}
document.addEventListener('DOMContentLoaded', () => {
    // The splash is visible by default (see index.html) and hides on the first
    // CSVData packet or when demo mode starts. This safety net guarantees it can
    // never stick if neither ever arrives. In a normal browser the page is served
    // by the device, so the first packet clears it almost immediately.
    setTimeout(() => { if (!window._firstCsvPacketReceived) hideWaitingForRegulator(); }, 20000);
});

// Poll while the tuning score section is open
document.addEventListener('DOMContentLoaded', () => {
    const section = document.getElementById('tuningScoreSection');
    if (!section) return;
    section.addEventListener('toggle', e => {
        if (e.target.open) {
            fetchTuningLog();
            _tuningLogPollTimer = setInterval(fetchTuningLog, 1000);
        } else {
            clearInterval(_tuningLogPollTimer);
            _tuningLogPollTimer = null;
        }
    });
});

// ── CV Tuning Log ──────────────────────────────────────────────────────────
let _cvTuningLogPollTimer = null;

function fetchCVTuningLog() {
    fetch(buildURL('/cvtuninglog'))
        .then(r => r.ok ? r.json() : null)
        .then(data => { if (data) renderCVTuningLog(data); })
        .catch(() => {});
}

function renderCVTuningLog(data) {
    // Update CV live score displays (Score Log + Live Data mirror)
    const cvLiveLabels = ['1m', '10m', '100m', '1000m'];
    (data.live || []).forEach((v, i) => {
        const val = v > 0 ? v.toFixed(2) : '—';
        const el = document.getElementById('cvLiveScore' + i);
        if (el) el.textContent = cvLiveLabels[i] + ': ' + val;
        const elAlt = document.getElementById('cvLiveScoreAlt' + i);
        if (elAlt) elAlt.textContent = val;   // Diag row header already lists the windows — no per-value prefix
    });

    // Active test score banner
    const testRow = document.getElementById('cvTestScoreRow');
    if (testRow) {
        if (data.ta) {
            testRow.style.display = '';
            const tsel = document.getElementById('cvCurrentTestScore');
            const ttgl = document.getElementById('cvTestCycleCount');
            if (tsel) tsel.textContent = data.ts > 0 ? data.ts.toFixed(2) : '—';
            if (ttgl) ttgl.textContent = data.tc;
            // Push cycle count into the floating test panel (phase slot is used for Δ voltage)
            if (_testPanelCurrentTest === 'cv') {
                updateTestPanelScore(undefined, (data.tc || 0) + ' cycles', undefined);
            }
        } else {
            testRow.style.display = 'none';
        }
    }

    // Commit section: show when CV test is active, enable button when ≥1 scored HIGH cycle
    const cvCommitSection = document.getElementById('cvTuningCommitSection');
    if (cvCommitSection) {
        cvCommitSection.style.display = data.ta ? '' : 'none';
        if (data.ta) {
            const countEl = document.getElementById('cvScoredHighCount');
            const btnEl   = document.getElementById('commitCVTuningBtn');
            if (countEl) countEl.textContent = data.tc || 0;
            if (btnEl)   btnEl.disabled = !((data.tc || 0) >= 1);
        }
    }

    // Current settings for row highlighting — read from echo spans
    const curVkp = parseFloat(document.getElementById('VoltageKp_echo')?.textContent);
    const curVki = parseFloat(document.getElementById('VoltageKi_echo')?.textContent);
    // curVkd removed — D term removed
    const curWa  = parseFloat(document.getElementById('cvWaveAmplitudeV_echo')?.textContent);
    const curWp  = parseInt(document.getElementById('cvWavePeriodSec_echo')?.textContent);
    const curCr  = parseInt(document.getElementById('cvConsecutiveReads_echo')?.textContent);

    const records = (data.rec || []).slice();
    const tbody = document.getElementById('cvTuningLogBody');
    if (!tbody) return;

    tbody.innerHTML = records.map(r => {
        const scoreColor = r.s < 2 ? '#22c55e' : r.s < 10 ? '#eab308' : '#ef4444';
        const isMatch = !isNaN(curVkp) &&
            Math.abs(r.vkp - curVkp) < 0.001 &&
            Math.abs(r.vki - curVki) < 0.001 &&
            Math.abs(r.wa  - curWa)  < 0.005 &&
            r.wp === curWp && r.cr === curCr;
        const rowStyle = isMatch ? 'background:rgba(99,102,241,0.18);' : '';

        const lowScoreColor = (r.ls || 0) < 2 ? '#22c55e' : (r.ls || 0) < 10 ? '#eab308' : '#ef4444';
        return `<tr style="${rowStyle}">
            <td style="padding:2px 4px;">${r.n}</td>
            <td style="padding:2px 4px;color:${scoreColor};font-weight:bold;">${r.s.toFixed(2)}</td>
            <td style="padding:2px 4px;">${r.st.toFixed(1)}</td>
            <td style="padding:2px 4px;">${r.wo.toFixed(3)}</td>
            <td style="padding:2px 4px;">${r.io.toFixed(4)}</td>
            <td style="padding:2px 4px;color:${lowScoreColor};font-weight:bold;">${(r.ls || 0).toFixed(2)}</td>
            <td style="padding:2px 4px;">${(r.lst || 0).toFixed(1)}</td>
            <td style="padding:2px 4px;">${(r.lwo || 0).toFixed(3)}</td>
            <td style="padding:2px 4px;">${(r.lio || 0).toFixed(4)}</td>
            <td style="padding:2px 4px;">${(r.lus || 0).toFixed(3)}</td>
            <td style="padding:2px 4px;">${r.vkp.toFixed(3)}</td>
            <td style="padding:2px 4px;">${r.vki.toFixed(3)}</td>
            <td style="padding:2px 4px;">${r.srr.toFixed(1)}</td>
            <td style="padding:2px 4px;">${r.sfr.toFixed(1)}</td>
            <td style="padding:2px 4px;">${r.abl.toFixed(2)}</td>
            <td style="padding:2px 4px;">${r.arl.toFixed(3)}</td>
            <td style="padding:2px 4px;">${r.asp}</td>
            <td style="padding:2px 4px;">${r.irf.toFixed(2)}</td>
            <td style="padding:2px 4px;">${r.ks.toFixed(1)}</td>
            <td style="padding:2px 4px;">${r.kh.toFixed(1)}</td>
            <td style="padding:2px 4px;">${r.iek.toFixed(1)}</td>
            <td style="padding:2px 4px;">${r.ien}</td>
            <td style="padding:2px 4px;">${r.iekb.toFixed(2)}</td>
            <td style="padding:2px 4px;">${r.lddt.toFixed(0)}</td>
            <td style="padding:2px 4px;">${r.ldt1.toFixed(0)}</td>
            <td style="padding:2px 4px;">${r.ldt3.toFixed(0)}</td>
            <td style="padding:2px 4px;">${r.tc.toFixed(0)}</td>
            <td style="padding:2px 4px;">${r.wa.toFixed(2)}</td>
            <td style="padding:2px 4px;">${r.wp}</td>
            <td style="padding:2px 4px;">${r.ko.toFixed(1)}</td>
            <td style="padding:2px 4px;">${r.cr}</td>
            <td style="padding:2px 4px;">${r.fov}</td>
            <td style="padding:2px 4px;">${r.iex}</td>
            <td style="padding:2px 4px;">${r.ld}</td>
            <td style="padding:2px 4px;">${r.hoc}</td>
            <td style="padding:2px 4px;">${r.rpm.toFixed(0)}</td>
            <td style="padding:2px 4px;">${r.tmp.toFixed(1)}</td>
            <td style="padding:2px 4px;">${r.bv.toFixed(2)}</td>
            <td style="padding:2px 4px;">${r.soc.toFixed(1)}</td>
            <td style="padding:2px 4px;">${r.cvt.toFixed(2)}</td>
        </tr>`;
    }).join('');
}

function resetCVTuningLog() {
    if (!confirm('Reset all CV tuning records?')) return;
    fetch(buildURL('/resetcvtuninglog'), { method: 'POST' })
        .then(() => fetchCVTuningLog())
        .catch(() => {});
}

// Poll while the CV tuning section is open
document.addEventListener('DOMContentLoaded', () => {
    const section = document.getElementById('cvTuningScoreSection');
    if (!section) return;
    section.addEventListener('toggle', e => {
        if (e.target.open) {
            fetchCVTuningLog();
            _cvTuningLogPollTimer = setInterval(fetchCVTuningLog, 4000);
        } else {
            clearInterval(_cvTuningLogPollTimer);
            _cvTuningLogPollTimer = null;
        }
    });
});

// ── Thermal Step Test Tuning Log ──────────────────────────────────────────
let _thermalTuningLogPollTimer = null;

function fetchThermalTuningLog() {
    fetch(buildURL('/thermaltuninglog'))
        .then(r => r.ok ? r.json() : null)
        .then(data => { if (data) renderThermalTuningLog(data); })
        .catch(() => {});
}

function renderThermalTuningLog(data) {
    // Update thermal live score displays (4 windows: 30m, 3h, 24h, 7d)
    const labels = ['30m', '3h', '24h', '7d'];
    (data.live || []).forEach((v, i) => {
        const val = v > 0 ? v.toFixed(4) : '—';
        const el = document.getElementById('thermalLiveScore' + i);
        if (el) el.textContent = labels[i] + ': ' + val;
        // Diag mirror: row header already lists the windows — show the bare value, no per-value prefix
        const altEl = document.getElementById('thermalLiveScoreAlt' + i);
        if (altEl) altEl.textContent = val;
    });

    // Active test score banner
    const testRow = document.getElementById('thermalTestScoreRow');
    if (testRow) {
        if (data.ta) {
            testRow.style.display = '';
            const tsel = document.getElementById('thermalCurrentTestScore');
            const ttgl = document.getElementById('thermalTestStepCount');
            if (tsel) tsel.textContent = data.ts > 0 ? data.ts.toFixed(2) : '—';
            if (ttgl) ttgl.textContent = data.tc;
            // Push live score into the floating test panel
            if (_testPanelCurrentTest === 'thermal') {
                updateTestPanelScore(data.ts, (data.tc || 0) + ' steps',
                    (data.tc || 0) === 0 ? 'Waiting for LOW stable' : 'Scoring');
            }
        } else {
            testRow.style.display = 'none';
        }
    }

    // Current settings for row highlighting
    const curKp  = parseFloat(document.getElementById('TempPIDKp_echo')?.textContent);
    const curKi  = parseFloat(document.getElementById('TempPIDKi_echo')?.textContent);
    const curLa  = parseFloat(document.getElementById('ThermalLookaheadSec_echo')?.textContent);
    const curWl  = parseFloat(document.getElementById('thermalWaveLowF_echo')?.textContent);
    const curWh  = parseFloat(document.getElementById('thermalWaveHighF_echo')?.textContent);
    const curWp  = parseFloat(document.getElementById('thermalWaveHalfPeriodMin_echo')?.textContent);

    const records = (data.rec || []).slice();
    const tbody = document.getElementById('thermalTuningLogBody');
    if (!tbody) return;

    tbody.innerHTML = records.map(r => {
        const scoreColor = r.s < 200 ? '#22c55e' : r.s < 600 ? '#eab308' : '#ef4444';
        const isMatch = !isNaN(curKp) &&
            Math.abs(r.kp - curKp) < 0.0001 &&
            Math.abs(r.ki - curKi) < 0.00001 &&
            Math.abs(r.la - curLa) < 1 &&
            Math.abs(r.wl - curWl) < 0.5 &&
            Math.abs(r.wh - curWh) < 0.5 &&
            Math.abs(r.wp - curWp) < 0.2;
        const rowStyle = isMatch ? 'background:rgba(99,102,241,0.18);' : '';
        return `<tr style="${rowStyle}">
            <td style="padding:2px 4px;">${r.n}</td>
            <td style="padding:2px 4px;color:${scoreColor};font-weight:bold;">${r.s.toFixed(2)}</td>
            <td style="padding:2px 4px;">${r.st.toFixed(0)}</td>
            <td style="padding:2px 4px;">${r.wo.toFixed(1)}</td>
            <td style="padding:2px 4px;">${r.io.toFixed(2)}</td>
            <td style="padding:2px 4px;">${r.iu.toFixed(2)}</td>
            <td style="padding:2px 4px;">${r.ns}</td>
            <td style="padding:2px 4px;">${r.kp.toFixed(4)}</td>
            <td style="padding:2px 4px;">${r.ki.toFixed(5)}</td>
            <td style="padding:2px 4px;">${r.la.toFixed(0)}</td>
            <td style="padding:2px 4px;">${r.fa.toFixed(3)}</td>
            <td style="padding:2px 4px;">${r.im}</td>
            <td style="padding:2px 4px;">${r.wl.toFixed(0)}</td>
            <td style="padding:2px 4px;">${r.wh.toFixed(0)}</td>
            <td style="padding:2px 4px;">${r.wp.toFixed(1)}</td>
            <td style="padding:2px 4px;">${r.rpm.toFixed(0)}</td>
            <td style="padding:2px 4px;">${(r.amb || 0).toFixed(1)}</td>
            <td style="padding:2px 4px;">${(r.rr || 0).toFixed(1)}</td>
            <td style="padding:2px 4px;">${(r.fr || 0).toFixed(1)}</td>
        </tr>`;
    }).join('');
}

function resetThermalTuningLog() {
    if (!confirm('Reset all thermal tuning records and live scores?')) return;
    fetch(buildURL('/resetthermaltuninglog'), { method: 'POST' })
        .then(() => fetchThermalTuningLog())
        .catch(() => {});
}

document.addEventListener('DOMContentLoaded', () => {
    const section = document.getElementById('thermalTuningScoreSection');
    if (!section) return;
    section.addEventListener('toggle', e => {
        if (e.target.open) {
            fetchThermalTuningLog();
            _thermalTuningLogPollTimer = setInterval(fetchThermalTuningLog, 8000);
        } else {
            clearInterval(_thermalTuningLogPollTimer);
            _thermalTuningLogPollTimer = null;
        }
    });
});

// ── SystemID (plant-delay) log ────────────────────────────────────────────
let _systemIDLogPollTimer = null;

function fetchSystemIDLog() {
    fetch(buildURL('/systemidlog'))
        .then(r => r.ok ? r.json() : null)
        .then(data => { if (data) renderSystemIDLog(data); })
        .catch(() => {});
}

function renderSystemIDLog(data) {
    const records = (data.rec || []).slice();
    const tbody = document.getElementById('systemIDLogBody');
    if (!tbody) return;

    if (records.length === 0) {
        tbody.innerHTML = '<tr><td colspan="11" style="padding:12px;text-align:center;color:var(--text-muted);">No plant-delay runs logged yet.</td></tr>';
        return;
    }

    const fmtMs = v => (v == null || v < 0) ? '—' : v.toFixed(0);
    const fmtA  = v => (v == null || !isFinite(v)) ? '—' : v.toFixed(2);

    tbody.innerHTML = records.map(r => {
        const aborted = (r.ar > 0) || (r.s < 0);
        const scoreColor = aborted
            ? '#ef4444'
            : (r.ra < 50 ? '#22c55e' : r.ra < 150 ? '#eab308' : '#ef4444');
        const abortLabel = aborted ? `R${r.ar}/P${r.ap}` : '—';
        const rd = r.rd || [-1, -1, -1];
        const fd = r.fd || [-1, -1, -1];
        const sa = r.sa || [0, 0, 0];
        const qp = r.qp || [0, 0, 0];
        return `<tr style="${aborted ? 'opacity:0.55;' : ''}">
            <td style="padding:2px 4px;">${r.n}</td>
            <td style="padding:2px 4px;color:${scoreColor};font-weight:bold;">${fmtMs(r.ra)}</td>
            <td style="padding:2px 4px;">${fmtMs(r.fa)}</td>
            <td style="padding:2px 4px;">${fmtMs(rd[0])} / ${fmtMs(rd[1])} / ${fmtMs(rd[2])}</td>
            <td style="padding:2px 4px;">${fmtMs(fd[0])} / ${fmtMs(fd[1])} / ${fmtMs(fd[2])}</td>
            <td style="padding:2px 4px;">${fmtA(sa[0])} / ${fmtA(sa[1])} / ${fmtA(sa[2])}</td>
            <td style="padding:2px 4px;">${fmtA(qp[0])} / ${fmtA(qp[1])} / ${fmtA(qp[2])}</td>
            <td style="padding:2px 4px;">${(r.amp || 0).toFixed(1)}</td>
            <td style="padding:2px 4px;">${(r.rpm || 0).toFixed(0)}</td>
            <td style="padding:2px 4px;">${(r.temp || 0).toFixed(0)}</td>
            <td style="padding:2px 4px;color:${aborted ? '#ef4444' : 'inherit'};">${abortLabel}</td>
        </tr>`;
    }).join('');
}

function resetSystemIDLog() {
    if (!confirm('Reset all Plant Delay (SystemID) records?')) return;
    fetch(buildURL('/resetsystemidlog'), { method: 'POST' })
        .then(() => fetchSystemIDLog())
        .catch(() => {});
}

document.addEventListener('DOMContentLoaded', () => {
    const section = document.getElementById('systemIDLogSection');
    if (!section) return;
    section.addEventListener('toggle', e => {
        if (e.target.open) {
            fetchSystemIDLog();
            _systemIDLogPollTimer = setInterval(fetchSystemIDLog, 8000);
        } else {
            clearInterval(_systemIDLogPollTimer);
            _systemIDLogPollTimer = null;
        }
    });
});

// ── CV Voltage Loop Tuning Plot ────────────────────────────────────────────
let cvTuningPlot = null;
let cvTuningData = null;
let cvTuningPlotResizeObserver = null;
// Corner "lock" button — same semantics as the Plots tab locks: freeze the Y
// autoscale at its current ranges while data keeps streaming. _cvLockWasAuto
// remembers which scales were auto at lock time so unlock returns only those
// to auto (explicit manual ranges survive a lock/unlock cycle).
let cvYAxesLocked = false;
let _cvLockWasAuto = { volts: false, amps: false };

// Cache of last-seen CSV2 values for the CV tuning plot.
// voltageTarget and Icv are CSV2 — not available in processCSVDataOptimized's data object.
// BatteryV_raw is CSV1 and is read directly from data; it is NOT cached here.
let cvPlotCache = { voltageTarget: null, Icv: null };
let _lastBatteryV = null;  // last CSV1 BatteryV_raw in volts (÷100 applied)

// Local axis range state — JS only, not firmware-persisted.
let cvXTime    = null;   // null = use global xTime
let cvVoltsMin = null;
let cvVoltsMax = null;
let cvAmpsMin  = null;
let cvAmpsMax  = null;

let cvTuningSeriesVisible = {
    voltTarget: true,
    battV:      true,
    ibv:        true,
    icv:        true,
};

function initCVTuningDataStructures() {
    const intervalMs = window._lastKnownInterval || 200;
    const cvT = (cvXTime && cvXTime > 0 && !isNaN(cvXTime)) ? cvXTime : xTime;
    const timeWindowSec = (cvT && cvT > 0 && !isNaN(cvT)) ? cvT : 30;
    const maxPoints = Math.ceil(timeWindowSec * 1000 / intervalMs);
    const intervalSec = intervalMs / 1000;
    const xAxisData = [];
    for (let i = 0; i < maxPoints; i++) xAxisData[i] = -(maxPoints - 1 - i) * intervalSec;

    cvTuningData = [
        [...xAxisData],
        new Array(maxPoints).fill(null),   // [1] voltageTarget (V)
        new Array(maxPoints).fill(null),   // [2] BatteryV_raw / battV (V)
        new Array(maxPoints).fill(null),   // [3] IBV raw (V)
        new Array(maxPoints).fill(null),   // [4] Icv (A) — right axis
    ];
    plotInterp.cv.arrivalTime = 0;
}

function initCVTuningPlot() {
    const plotEl = document.getElementById('cv-tuning-plot');
    if (!plotEl) return;

    if (!cvTuningData) initCVTuningDataStructures();

    const opts = {
        width:  Math.min(plotEl.clientWidth, 800),
        height: 350,
        series: [
            {},
            {
                label:  'Voltage Target',
                stroke: cvTuningSeriesVisible.voltTarget ? '#FF6B6B' : 'transparent',
                width:  2,
                scale:  'volts',
                dash:   [6, 3],
            },
            {
                label:  'Filtered Voltage',
                stroke: cvTuningSeriesVisible.battV ? '#4CAF50' : 'transparent',
                width:  2,
                scale:  'volts',
            },
            {
                label:  'IBV (raw)',
                stroke: cvTuningSeriesVisible.ibv ? '#B0BEC5' : 'transparent',
                width:  1,
                scale:  'volts',
                dash:   [2, 2],
            },
            {
                label:  'Icv (A)',
                stroke: cvTuningSeriesVisible.icv ? '#FFA726' : 'transparent',
                width:  2,
                scale:  'amps',
            },
        ],
        axes: [
            {},
            {
                scale: 'volts',
                label: 'Voltage (V)',
                side:  3,
                grid:  { show: true },
                splits: edgeLabeledSplits(() => cvVoltsMin !== null),
            },
            {
                scale: 'amps',
                label: 'Current (A)',
                side:  1,
                grid:  { show: false },
                splits: edgeLabeledSplits(() => cvAmpsMin !== null),
            },
        ],
        scales: {
            // ALL ranges are fns, not arrays: re-read each redraw, so window changes and
            // manual Y edits take effect via plain setData — no destroy/recreate, no
            // layout jump. Null Y globals = default autoscale fit.
            x:     { time: false, auto: false, range: () => [cvTuningData[0][0], cvTuningData[0][cvTuningData[0].length - 1]] },
            volts: { range: (u, mn, mx) => (cvVoltsMin !== null && cvVoltsMax !== null) ? [cvVoltsMin, cvVoltsMax] : (mn == null ? [0, 1] : uPlot.rangeNum(mn, mx, 0.1, true)) },
            amps:  { range: (u, mn, mx) => (cvAmpsMin  !== null && cvAmpsMax  !== null) ? [cvAmpsMin,  cvAmpsMax]  : (mn == null ? [0, 1] : uPlot.rangeNum(mn, mx, 0.1, true)) },
        },
    };

    if (cvTuningPlot) cvTuningPlot.destroy();
    cvTuningPlot = new uPlot(opts, cvTuningData, plotEl);
    if (document.body.classList.contains('dark-mode')) updateUplotTheme(cvTuningPlot);
    createCVTuningLegend();

    // Corner lock button — locks the Y axes at their current ranges (same
    // semantics as the Plots tab lock buttons). Data keeps streaming.
    plotEl.style.position = 'relative';
    const existingLock = plotEl.querySelector('.autoscale-ctrl');
    if (existingLock) existingLock.remove();
    cvYAxesLocked = false;
    const lockDiv = document.createElement('div');
    lockDiv.className = 'autoscale-ctrl';
    lockDiv.style.cssText = 'position:absolute;top:6px;right:8px;z-index:10;display:flex;flex-direction:column;align-items:flex-end;gap:2px;font-size:11px;';
    lockDiv.innerHTML = '<button id="lock-cv-tuning-btn" style="font-size:10px;padding:0 5px;cursor:pointer;border:1px solid #999;border-radius:2px;background:transparent;opacity:0.6;line-height:16px;">lock</button>';
    plotEl.appendChild(lockDiv);
    const lockBtnCV = document.getElementById('lock-cv-tuning-btn');
    lockBtnCV.addEventListener('click', () => {
        cvYAxesLocked = !cvYAxesLocked;
        if (cvYAxesLocked) {
            // Freeze each auto scale at its current range; manual ranges are already fixed
            _cvLockWasAuto.volts = (cvVoltsMin === null);
            _cvLockWasAuto.amps  = (cvAmpsMin === null);
            if (_cvLockWasAuto.volts) { cvVoltsMin = cvTuningPlot.scales.volts.min; cvVoltsMax = cvTuningPlot.scales.volts.max; }
            if (_cvLockWasAuto.amps)  { cvAmpsMin  = cvTuningPlot.scales.amps.min;  cvAmpsMax  = cvTuningPlot.scales.amps.max; }
        } else {
            // Unlock returns previously-auto scales to auto; manual ones stay manual
            if (_cvLockWasAuto.volts) { cvVoltsMin = null; cvVoltsMax = null; }
            if (_cvLockWasAuto.amps)  { cvAmpsMin  = null; cvAmpsMax  = null; }
            queueCVTuningPlotUpdate();
        }
        lockBtnCV.textContent = cvYAxesLocked ? 'unlock' : 'lock';
        lockBtnCV.style.opacity = cvYAxesLocked ? '1' : '0.6';
    });

    // Click-to-edit Y limits — the on-plot boxes are the only Y range controls.
    // Auto (clear + Enter) just nulls the globals: the range fns re-read them on the
    // next redraw, so a queued setData is all it takes — never destroy/recreate.
    attachYAxisEdit(cvTuningPlot, [
        {
            scale: 'volts', decimals: 2,
            apply: (mn, mx) => setCVVoltsRange(mn, mx),
            auto: () => { cvVoltsMin = null; cvVoltsMax = null; queueCVTuningPlotUpdate(); }
        },
        {
            scale: 'amps', decimals: 1,
            apply: (mn, mx) => setCVAmpsRange(mn, mx),
            auto: () => { cvAmpsMin = null; cvAmpsMax = null; queueCVTuningPlotUpdate(); }
        }
    ]);

    // Show the active X window in the input as a placeholder hint
    const xInp = document.getElementById('cvXTimeInput');
    if (xInp) xInp.placeholder = String((cvXTime && cvXTime > 0) ? cvXTime : (xTime || 30));

    if (cvTuningPlotResizeObserver) cvTuningPlotResizeObserver.disconnect();
    cvTuningPlotResizeObserver = new ResizeObserver(() => {
        if (cvTuningPlot) cvTuningPlot.setSize({ width: plotEl.clientWidth, height: 350 });
    });
    cvTuningPlotResizeObserver.observe(plotEl);
}

function createCVTuningLegend() {
    const plotEl = document.getElementById('cv-tuning-plot');
    if (!plotEl) return;
    const existing = plotEl.querySelector('.cv-custom-legend');
    if (existing) existing.remove();

    const legendDiv = document.createElement('div');
    legendDiv.className = 'cv-custom-legend';
    legendDiv.style.cssText = 'display:flex;justify-content:center;gap:15px;margin-top:8px;flex-wrap:wrap;';

    const items = [
        { key: 'voltTarget', label: 'Voltage Target', color: '#FF6B6B', idx: 1 },
        { key: 'battV',      label: 'Battery Voltage',  color: '#4CAF50', idx: 2 },
        { key: 'ibv',        label: 'IBV (raw)',         color: '#B0BEC5', idx: 3 },
        { key: 'icv',        label: 'Icv (A)',           color: '#FFA726', idx: 4 },
    ];

    items.forEach(item => {
        const lbl = document.createElement('label');
        lbl.style.cssText = 'display:flex;align-items:center;gap:6px;font-size:12px;cursor:pointer;user-select:none;';
        const cb = document.createElement('input');
        cb.type = 'checkbox';
        cb.checked = cvTuningSeriesVisible[item.key];
        cb.style.cssText = 'cursor:pointer;margin:0;';
        cb.addEventListener('change', () => {
            cvTuningSeriesVisible[item.key] = cb.checked;
            if (cvTuningPlot) cvTuningPlot.setSeries(item.idx, { show: cb.checked });
        });
        const box = document.createElement('div');
        box.style.cssText = `width:16px;height:3px;background:${item.color};border-radius:1px;`;
        const span = document.createElement('span');
        span.textContent = item.label;
        span.style.cssText = 'color:var(--text-dark);';
        lbl.appendChild(cb); lbl.appendChild(box); lbl.appendChild(span);
        legendDiv.appendChild(lbl);
    });
    plotEl.appendChild(legendDiv);
}

let cvTuningPlotUpdateScheduled = false;
function queueCVTuningPlotUpdate() {
    if (cvTuningPlotUpdateScheduled) return;
    cvTuningPlotUpdateScheduled = true;
    requestAnimationFrame(() => {
        if (cvTuningPlot && cvTuningData) cvTuningPlot.setData(cvTuningData);
        cvTuningPlotUpdateScheduled = false;
    });
}

// Resize the rolling window WITHOUT losing history or recreating the plot:
// rebuild the x axis at the new length, right-align the newest overlapping
// samples into it (shrink keeps the newest tail; grow pads the past with
// nulls), and let setData re-run the range fns. The plot DOM, lock button,
// legend, and Y-edit boxes all survive untouched.
function rebuildCVTuningWindow() {
    const old = cvTuningData;
    initCVTuningDataStructures();
    if (old) {
        const oldN = old[1].length, newN = cvTuningData[1].length;
        const n = Math.min(oldN, newN);
        for (let s = 1; s <= 4; s++)
            for (let i = 0; i < n; i++)
                cvTuningData[s][newN - 1 - i] = old[s][oldN - 1 - i];
    }
    if (cvTuningPlot) cvTuningPlot.setData(cvTuningData);
    const xInp = document.getElementById('cvXTimeInput');
    if (xInp) xInp.placeholder = String((cvXTime && cvXTime > 0) ? cvXTime : (xTime || 30));
}

function setCVXTime(val) {
    const v = parseFloat(val);
    if (!isFinite(v) || v <= 0) return;
    cvXTime = v;
    rebuildCVTuningWindow();
}

function setCVVoltsRange(minVal, maxVal) {
    const mn = parseFloat(minVal), mx = parseFloat(maxVal);
    if (!isFinite(mn) || !isFinite(mx) || mn >= mx) return;
    cvVoltsMin = mn; cvVoltsMax = mx;
    _cvLockWasAuto.volts = false;   // an explicit edit while locked must survive unlock
    if (cvTuningPlot) cvTuningPlot.setScale('volts', { min: mn, max: mx });
}

function setCVAmpsRange(minVal, maxVal) {
    const mn = parseFloat(minVal), mx = parseFloat(maxVal);
    if (!isFinite(mn) || !isFinite(mx) || mn >= mx) return;
    cvAmpsMin = mn; cvAmpsMax = mx;
    _cvLockWasAuto.amps = false;   // an explicit edit while locked must survive unlock
    if (cvTuningPlot) cvTuningPlot.setScale('amps', { min: mn, max: mx });
}

function resetVoltageProtectionCounters() {
    if (!confirm('Reset all voltage & current protection counters? Fast-overvoltage, excess-current, hardware overvoltage, hard overcurrent, spike, and sensor-disagreement counts will be cleared.')) return;
    fetch(buildURL('/resetVoltageProtectionCounters'), { method: 'POST' })
        .then(r => r.ok ? console.log('Voltage protection counters reset') : console.warn('Reset failed'))
        .catch(err => console.warn('Reset error:', err));
}

function resetThermalProtectionCounters() {
    if (!confirm('Reset thermal protection event counters? Critical-temperature, sustained-temperature, and stale-sensor counts will be cleared.')) return;
    fetch(buildURL('/resetThermalProtectionCounters'), { method: 'POST' })
        .then(r => r.ok ? console.log('Thermal protection counters reset') : console.warn('Reset failed'))
        .catch(err => console.warn('Reset error:', err));
}

function resetTempTaskCounters() {
    if (!confirm('Reset DS18B20 sensor health counters? All read/CRC/fail counts will be cleared.')) return;
    fetch(buildURL('/resetTempTaskCounters'), { method: 'POST' })
        .then(r => r.ok ? console.log('TempTask counters reset') : console.warn('Reset failed'))
        .catch(err => console.warn('Reset error:', err));
}


// ============================================
// CLOUD FEATURES - Long Term Plots is now native (Plots → Long Term); the old
// iframe viewer + redirectToHistory() were removed 2026-05-31.
// ============================================
// LONG TERM PLOTS — Dashboard-hosted brush
// Lives in parent (NOT in iframe) so it sticks naturally to dashboard viewport.
// Two-way postMessage sync with the iframe.
// ============================================
const _brushState = {
    dataMin: null,
    dataMax: null,
    from: null,
    to: null,
    pinnedCrosshair: null,
    latestSample: null,
    stalenessTimerId: null,
    wired: false,
    iframeReady: false,
    nativeSink: null   // when set (Long Term native charts), range changes route here, not the iframe
};

function _brushEl(id) { return document.getElementById(id); }

function _brushTimeToPixel(ms) {
    const track = _brushEl('dashboard-brush-track');
    if (!track || _brushState.dataMax === _brushState.dataMin) return 0;
    const w = track.clientWidth || 0;
    return (ms - _brushState.dataMin) / (_brushState.dataMax - _brushState.dataMin) * w;
}
function _brushPixelToTime(px) {
    const track = _brushEl('dashboard-brush-track');
    if (!track) return _brushState.dataMin;
    const w = track.clientWidth || 1;
    return _brushState.dataMin + (px / w) * (_brushState.dataMax - _brushState.dataMin);
}

function _brushFormatTime(ms) {
    if (ms == null || !isFinite(ms)) return '—';
    const d = new Date(ms);
    const date = `${d.getMonth() + 1}/${d.getDate()}/${String(d.getFullYear()).slice(-2)}`;
    const time = `${String(d.getHours()).padStart(2, '0')}:${String(d.getMinutes()).padStart(2, '0')}`;
    return `${date} ${time}`;
}
function _brushFormatDuration(ms) {
    if (ms == null || ms <= 0) return '';
    const s = Math.floor(ms / 1000);
    if (s < 60) return `(${s}s)`;
    const m = Math.floor(s / 60);
    if (m < 60) return `(${m}m)`;
    const h = Math.floor(m / 60);
    if (h < 48) return `(${h}h ${m % 60}m)`;
    const d = Math.floor(h / 24);
    return `(${d}d ${h % 24}h)`;
}

function _brushClamp(from, to) {
    const minSpan = 60 * 1000;
    if (to - from < minSpan) to = from + minSpan;
    if (from < _brushState.dataMin) {
        const span = to - from;
        from = _brushState.dataMin;
        to = Math.min(_brushState.dataMax, from + span);
    }
    if (to > _brushState.dataMax) {
        const span = to - from;
        to = _brushState.dataMax;
        from = Math.max(_brushState.dataMin, to - span);
    }
    return { from, to };
}

function _brushUpdateSelectionUI() {
    const sel = _brushEl('dashboard-brush-selection');
    if (!sel || _brushState.dataMin == null) return;
    const left = Math.max(0, _brushTimeToPixel(_brushState.from));
    const right = _brushTimeToPixel(_brushState.to);
    sel.style.left = left + 'px';
    // Minimum 20px wide so even tiny selections (e.g. "1h" on 30-day-wide data)
    // are visually obvious as "I selected something" instead of a 1px sliver.
    sel.style.width = Math.max(20, right - left) + 'px';
    _brushEl('dashboard-brush-duration').textContent =
        _brushFormatDuration(_brushState.to - _brushState.from);
}

function _brushSendRangeToIframe() {
    if (!_brushState.iframeReady) return;
    const iframe = document.getElementById('history-iframe');
    if (!iframe || !iframe.contentWindow) return;
    iframe.contentWindow.postMessage({
        type: 'BRUSH_RANGE_CHANGED',
        from: _brushState.from,
        to: _brushState.to
    }, '*');
}

// Route the current brush range to whoever owns the brush: native LT charts if a
// sink is registered, else the (legacy) iframe.
function _brushEmit() {
    if (_brushState.nativeSink) { _brushState.nativeSink(_brushState.from, _brushState.to); return; }
    _brushSendRangeToIframe();
}

function _brushApplyRange(from, to, opts) {
    opts = opts || {};
    const c = _brushClamp(from, to);
    _brushState.from = c.from;
    _brushState.to = c.to;
    _brushUpdateSelectionUI();
    if (!opts.skipIframe) _brushEmit();
}

function _brushWireDrag() {
    const track = _brushEl('dashboard-brush-track');
    const sel = _brushEl('dashboard-brush-selection');
    const handleLeft = sel.querySelector('.dashboard-brush-handle-left');
    const handleRight = sel.querySelector('.dashboard-brush-handle-right');

    let mode = null;
    let startPx = 0, startFrom = 0, startTo = 0, createAnchorMs = 0;

    const ptrX = (ev) => {
        if (ev.touches && ev.touches.length) return ev.touches[0].clientX;
        if (ev.changedTouches && ev.changedTouches.length) return ev.changedTouches[0].clientX;
        return ev.clientX;
    };

    const onDown = (ev, kind) => {
        ev.preventDefault();
        const rect = track.getBoundingClientRect();
        startPx = ptrX(ev) - rect.left;
        startFrom = _brushState.from;
        startTo = _brushState.to;
        mode = kind;
        if (kind === 'create') createAnchorMs = _brushPixelToTime(startPx);
        document.addEventListener('mousemove', onMove);
        document.addEventListener('mouseup', onUp);
        document.addEventListener('touchmove', onMove, { passive: false });
        document.addEventListener('touchend', onUp);
    };
    const onMove = (ev) => {
        if (!mode) return;
        ev.preventDefault();
        const rect = track.getBoundingClientRect();
        const px = ptrX(ev) - rect.left;
        const dpx = px - startPx;
        const span = _brushState.dataMax - _brushState.dataMin;
        const trackW = track.clientWidth || 1;
        const dms = (dpx / trackW) * span;
        // Visual-only during drag — iframe rebuild on drag end keeps things snappy.
        // Pre-clamp the dragged edge so _brushClamp (designed for pan) doesn't shift
        // the fixed edge when the dragged edge hits a data boundary. That was the
        // un-physical feeling: drag the left handle off the left edge → right handle
        // also moved. Now resize feels like a real edge-grab.
        const opts = { skipIframe: true };
        const MIN_SPAN = 60 * 1000; // 1 minute
        const dMin = _brushState.dataMin;
        const dMax = _brushState.dataMax;
        if (mode === 'pan') {
            _brushApplyRange(startFrom + dms, startTo + dms, opts);
        } else if (mode === 'resize-left') {
            const newFrom = Math.max(dMin, Math.min(startFrom + dms, startTo - MIN_SPAN));
            _brushApplyRange(newFrom, startTo, opts);
        } else if (mode === 'resize-right') {
            const newTo = Math.min(dMax, Math.max(startTo + dms, startFrom + MIN_SPAN));
            _brushApplyRange(startFrom, newTo, opts);
        } else if (mode === 'create') {
            const ptr = Math.max(dMin, Math.min(dMax, _brushPixelToTime(px)));
            _brushApplyRange(Math.min(createAnchorMs, ptr), Math.max(createAnchorMs, ptr), opts);
        }
    };
    const onUp = () => {
        const wasDragging = mode != null;
        mode = null;
        document.removeEventListener('mousemove', onMove);
        document.removeEventListener('mouseup', onUp);
        document.removeEventListener('touchmove', onMove);
        document.removeEventListener('touchend', onUp);
        if (wasDragging) _brushEmit();
    };

    sel.addEventListener('mousedown', (e) => {
        if (e.target === handleLeft || e.target === handleRight) return;
        onDown(e, 'pan');
    });
    sel.addEventListener('touchstart', (e) => {
        if (e.target === handleLeft || e.target === handleRight) return;
        onDown(e, 'pan');
    }, { passive: false });
    handleLeft.addEventListener('mousedown', (e) => onDown(e, 'resize-left'));
    handleLeft.addEventListener('touchstart', (e) => onDown(e, 'resize-left'), { passive: false });
    handleRight.addEventListener('mousedown', (e) => onDown(e, 'resize-right'));
    handleRight.addEventListener('touchstart', (e) => onDown(e, 'resize-right'), { passive: false });
    track.addEventListener('mousedown', (e) => {
        if (e.target === track) onDown(e, 'create');
    });
    track.addEventListener('touchstart', (e) => {
        if (e.target === track) onDown(e, 'create');
    }, { passive: false });

    window.addEventListener('resize', _brushUpdateSelectionUI);
}

function _brushWireSnap() {
    const allBtns = document.querySelectorAll('.dashboard-brush-snap-btn');
    allBtns.forEach(btn => {
        btn.addEventListener('click', () => {
            // Visual feedback: highlight the clicked snap, clear others. Helps user
            // confirm "1h" or "All" actually fired (selection on the brush may be
            // very narrow when the data range is wide).
            allBtns.forEach(b => b.classList.remove('active'));
            btn.classList.add('active');
            if (btn.getAttribute('data-all') === 'true') {
                _brushApplyRange(_brushState.dataMin, _brushState.dataMax);
                if (_brushState.ltSaveWindowPref) _brushState.ltSaveWindowPref({ all: true });   // persist choice (host localStorage)
                return;
            }
            const hours = parseInt(btn.getAttribute('data-hours'), 10);
            if (!isFinite(hours) || hours <= 0) return;
            const span = hours * 3600 * 1000;
            const nowMs = Date.now();
            const to = Math.min(_brushState.dataMax, nowMs);
            const from = Math.max(_brushState.dataMin, to - span);
            _brushApplyRange(from, to);
            if (_brushState.ltSaveWindowPref) _brushState.ltSaveWindowPref({ hours: hours });   // persist choice (host localStorage)
        });
    });
}

// Render the "Last data: HH:MM (Nm ago)" indicator. Color-graded:
// green ≤5min, yellow ≤30min, red >30min. Called on BRUSH_DATA_RANGE
// + every 10s via a tracked timer so "X ago" stays current.
function _brushUpdateStaleness() {
    const ago = _brushEl('dashboard-staleness-ago');
    const tEl = _brushEl('dashboard-staleness-time');
    if (!ago || !tEl) return;
    const t = _brushState.latestSample;
    if (t == null || !isFinite(t)) {
        tEl.textContent = '—';
        ago.textContent = '—';
        ago.classList.remove('stale-warn', 'stale-err');
        return;
    }
    tEl.textContent = _brushFormatTime(t);
    const deltaMs = Date.now() - t;
    const s = Math.max(0, Math.floor(deltaMs / 1000));
    let label;
    if (s < 60) label = s + 's ago';
    else if (s < 3600) label = Math.floor(s / 60) + 'm ago';
    else if (s < 86400) label = Math.floor(s / 3600) + 'h ' + (Math.floor(s / 60) % 60) + 'm ago';
    else label = Math.floor(s / 86400) + 'd ago';
    ago.textContent = label;
    ago.classList.remove('stale-warn', 'stale-err');
    if (s > 1800) ago.classList.add('stale-err');
    else if (s > 300) ago.classList.add('stale-warn');
}

function _brushUpdateCrosshairIndicator() {
    const ind = _brushEl('dashboard-crosshair-indicator');
    const txt = _brushEl('dashboard-crosshair-time');
    if (!ind || !txt) return;
    if (_brushState.pinnedCrosshair == null) {
        ind.style.display = 'none';
        txt.textContent = '—';
    } else {
        ind.style.display = 'inline-block';
        txt.textContent = _brushFormatTime(_brushState.pinnedCrosshair);
    }
}

function _brushWireCrosshairClear() {
    const btn = _brushEl('dashboard-crosshair-clear');
    if (!btn) return;
    btn.addEventListener('click', () => {
        _brushState.pinnedCrosshair = null;
        _brushUpdateCrosshairIndicator();
        const iframe = document.getElementById('history-iframe');
        if (iframe && iframe.contentWindow) {
            iframe.contentWindow.postMessage({ type: 'BRUSH_CROSSHAIR_CLEAR' }, '*');
        }
    });
}

function _brushHandleMessage(e) {
    // Origin check matches the IFRAME_HEIGHT handler's allowed list
    if (e.origin !== 'https://supabase-nine-ashy.vercel.app') return;
    if (!e.data || !e.data.type) return;
    if (_brushState.nativeSink) return;   // native Long Term charts own the brush now
    if (e.data.type === 'BRUSH_DATA_RANGE') {
        _brushState.dataMin = Number(e.data.min);
        _brushState.dataMax = Number(e.data.max);
        // Newest raw sample time (NOT the synthetic nowMs extension) for staleness display
        if (e.data.latestSample != null) {
            _brushState.latestSample = Number(e.data.latestSample);
            _brushUpdateStaleness();
            // Refresh the "X ago" text every 10 s so it stays current while the user
            // leaves the tab open. setTrackedInterval handles cleanup on page unload.
            if (_brushState.stalenessTimerId == null) {
                _brushState.stalenessTimerId = (typeof setTrackedInterval === 'function'
                    ? setTrackedInterval : setInterval)(_brushUpdateStaleness, 10000);
            }
        }
        _brushState.iframeReady = true;
        if (!isFinite(_brushState.dataMin) || !isFinite(_brushState.dataMax)) return;
        // Initial selection: left edge at ~1/3 of the total data span, right edge
        // at the newest data. Keeps the selection visibly smaller than the full
        // track so the brush reads as a draggable selector, not as the whole bar.
        const fullSpan = _brushState.dataMax - _brushState.dataMin;
        _brushState.from = _brushState.dataMin + fullSpan / 3;
        _brushState.to = _brushState.dataMax;
        if (!_brushState.wired) {
            _brushWireDrag();
            _brushWireSnap();
            _brushWireCrosshairClear();
            _brushState.wired = true;
        }
        _brushEl('dashboard-brush').style.display = 'block';
        // Populate the always-visible data bounds labels above the track
        const minEl = _brushEl('dashboard-brush-data-min');
        const maxEl = _brushEl('dashboard-brush-data-max');
        if (minEl) minEl.textContent = _brushFormatTime(_brushState.dataMin);
        if (maxEl) maxEl.textContent = _brushFormatTime(_brushState.dataMax);
        _brushUpdateSelectionUI();
        _brushSendRangeToIframe();
    } else if (e.data.type === 'BRUSH_CROSSHAIR_SET') {
        const t = Number(e.data.time);
        if (!isFinite(t)) return;
        _brushState.pinnedCrosshair = t;
        _brushUpdateCrosshairIndicator();
    }
}
window.addEventListener('message', _brushHandleMessage);


// ============================================
// CLOUD FEATURES - Dark mode for embedded Vercel pages
// The cloud pages live in iframes and can't see this dashboard's dark-mode toggle.
// We tell them: ?dark=N on the URL for flash-free first paint, plus a SET_THEME
// postMessage when the user flips the toggle while a cloud tab is open.
// ============================================
function cloudDarkParam() {
    return document.body.classList.contains('dark-mode') ? '1' : '0';
}
function broadcastThemeToCloudIframes() {
    const dark = document.body.classList.contains('dark-mode');
    ['leaderboards-iframe', 'fleetstats-iframe'].forEach(id => {
        const f = document.getElementById(id);
        if (f && f.contentWindow) {
            f.contentWindow.postMessage({ type: 'SET_THEME', dark }, 'https://supabase-nine-ashy.vercel.app');
        }
    });
}


// ============================================
// CLOUD FEATURES - Leaderboards
async function loadLeaderboardsInIframe() {
    const statusEl = document.getElementById('leaderboards-status');
    const iframe = document.getElementById('leaderboards-iframe');

    if (statusEl) statusEl.textContent = 'Loading leaderboards...';

    try {
        const response = await fetchWithTimeout(buildURL('/getAuthToken'), {}, 8000);
        if (!response.ok) throw new Error(`HTTP ${response.status}`);

        const data = await response.json();

        if (data.registered && data.token) {
            iframe.src = `https://supabase-nine-ashy.vercel.app/leaderboards.html?token=${encodeURIComponent(data.token)}&dark=${cloudDarkParam()}`;
            iframe.style.display = 'block';
            iframe.onload = function () {
                if (statusEl) statusEl.style.display = 'none';
            };
        } else {
            if (statusEl) {
                statusEl.innerHTML = 'Device not registered. Please complete registration in <strong>My Profile</strong> tab first.';
                statusEl.style.color = '#ff6b6b';
            }
        }
    } catch (error) {
        if (statusEl) {
            statusEl.textContent = 'Error: Could not connect to device';
            statusEl.style.color = '#ff6b6b';
        }
    }
}
// Get current user's device UID for highlighting
// Highlight user's own entry

// ============================================
// CLOUD FEATURES - Fleet Stats
// ============================================

async function loadFleetStatsInIframe() {
    const statusEl = document.getElementById('fleetstats-status');
    const iframe = document.getElementById('fleetstats-iframe');

    if (statusEl) statusEl.textContent = 'Loading fleet statistics...';

    try {
        iframe.src = `https://supabase-nine-ashy.vercel.app/fleet-stats.html?dark=${cloudDarkParam()}`;
        iframe.style.display = 'block';

        iframe.onload = function () {
            if (statusEl) statusEl.style.display = 'none';
        };

    } catch (error) {
        if (statusEl) {
            statusEl.textContent = 'Error: Could not load fleet statistics';
            statusEl.style.color = '#ff6b6b';
        }
    }
}


//Alarm Momentery Button Logic
function handleAlarmTest() {
    const button = document.getElementById('alarm-test-btn');
    button.disabled = true;
    button.textContent = 'Testing...';
    button.style.backgroundColor = '#6c757d';
    setTrackedTimeout(() => {
        button.disabled = false;
        button.textContent = 'Test Buzzer (2s)';
        button.style.backgroundColor = '#555555';
    }, 3000);
    submitMessage();
    return true; // Allow form submission
}
function handleResetLatch() {
    const button = document.getElementById('reset-latch-btn');
    button.disabled = true;
    button.textContent = 'Resetting...';
    button.style.backgroundColor = '#6c757d';

    setTrackedTimeout(() => {
        button.disabled = false;
        button.textContent = 'Reset Latch';
        button.style.backgroundColor = '#555555';
    }, 2000);

    submitMessage();
    return true; // Allow form submission
}


//Factory Reset Logic
function factoryReset() {
    if (!currentAdminPassword) {
        alert("You must be logged in to perform a factory reset.");
        return;
    }
    const confirmation = confirm(
        "⚠️ FACTORY RESET WARNING ⚠️\n\n" +
        "This will restore settings to factory defaults.\n" +
        "Device will restart.\n" +
        "All your settings will be lost.\n\n" +
        "Are you sure you want to continue?"
    );
    if (!confirmation) return;

    const button = document.getElementById('factory-reset-btn');
    button.disabled = true;
    button.textContent = 'Resetting...';
    button.style.backgroundColor = '#6c757d';

    const formData = new FormData();
    formData.append("password", currentAdminPassword);

    fetchWithTimeout(buildURL("/factoryReset"), {
        method: "POST",
        body: formData
    }, 10000)
        .then(response => {
            if (!response.ok) {
                throw new Error(`Server returned ${response.status}: ${response.statusText}`);
            }
            return response.text();
        })
        .then(text => {
            if (text.trim() === "OK") {
                alert("Factory reset complete!\n\nThe device will restart.\nPage will reload in 5 seconds.");
                setTrackedTimeout(() => {
                    window.location.reload();
                }, 5000);
            } else {
                throw new Error("Unexpected response: " + text);
            }
        })
        .catch(err => {
            diagError("Factory reset error:", err);

            // Timeout or network error means the device is resetting (expected behavior)
            if (err.message === 'Request timeout' || err.message.includes('Failed to fetch')) {
                alert("Factory reset complete!\n\nThe device is restarting.\n\nPage will reload in some seconds.");
                setTrackedTimeout(() => {
                    window.location.reload();
                }, 8000);
            } else {
                // Unexpected error
                alert("Error during factory reset:\n\n" + err.message);
                button.disabled = false;
                button.textContent = 'Restore Defaults';
                button.style.backgroundColor = '#555555';
            }
        });
}

function updateInlineStatus(isConnected) {
    const cornerStatus = document.getElementById('corner-status');

    if (cornerStatus) {
        if (isConnected) {
            cornerStatus.className = 'corner-status corner-status-connected';
            cornerStatus.textContent = 'WIFI CONNECTED';
        } else {
            cornerStatus.className = 'corner-status corner-status-disconnected';
            cornerStatus.textContent = 'WIFI DISCONNECTED';
        }
    }
}


//Reset buttons
// Single unified reset function
function resetParameter(parameterName) {
    if (!currentAdminPassword) {
        alert("Please unlock settings first");
        return;
    }

    updatePasswordFields();

    // Find the hidden input and button by parameter name
    const hiddenInput = document.getElementById(parameterName);
    const button = document.getElementById(parameterName + '_button');

    if (!hiddenInput || !button) {
        diagError(`Reset elements not found for parameter: ${parameterName}`);
        return;
    }

    hiddenInput.value = '1';
    button.closest('form').submit();
    submitMessage();
}

//Console
// Console state
let consolePaused = false;

function copyConsole() {
    const consoleDiv = document.getElementById("consoleOutput");
    if (!consoleDiv) {
        return;
    }

    const text = Array.from(consoleDiv.children)
        .map(div => div.textContent)
        .join('\n');

    if (!text) {
        return;
    }

    // Try modern clipboard API first
    if (navigator.clipboard && navigator.clipboard.writeText) {
        navigator.clipboard.writeText(text).then(() => {
        }).catch(err => {
            console.error('Clipboard error:', err);
            fallbackCopy(text);
        });
    } else {
        // Fallback for older browsers or non-HTTPS
        fallbackCopy(text);
    }
}

function fallbackCopy(text) {
    const textarea = document.createElement('textarea');
    textarea.value = text;
    textarea.style.position = 'fixed';
    textarea.style.opacity = '0';
    document.body.appendChild(textarea);
    textarea.select();
    try {
        document.execCommand('copy');
    } catch (err) {
    }
    document.body.removeChild(textarea);
}


function clearConsole() {
    const el = document.getElementById("consoleOutput");
    if (el) el.innerHTML = "";
}

function toggleConsolePause() {
    consolePaused = !consolePaused;
    const btn = document.getElementById("pauseConsoleBtn");
    if (btn) {
        btn.textContent = consolePaused ? "Resume" : "Pause";
        btn.style.backgroundColor = consolePaused ? "#ff6b6b" : "";
    }
}

//Mirror for Alarm
function updateAlarmStatus(data) {
    const alarmLed = document.getElementById('alarm-led');
    const alarmText = document.getElementById('alarm-status-text');

    if (!alarmLed || !alarmText) return;

    const isAlarming = data.Alarm_Status === 1; // Mirror Alarm directly

    if (isAlarming) {
        // ALARMING: Red light and text
        alarmLed.style.backgroundColor = '#ff0000';
        alarmLed.style.boxShadow = '0 0 15px rgba(255, 0, 0, 0.8)';
        alarmText.textContent = 'Alarming!';
        alarmText.style.color = '#ff0000';
    } else {
        // SILENT: Green light and text (using your theme's green)
        alarmLed.style.backgroundColor = '#00a19a';
        alarmLed.style.boxShadow = '0 0 10px rgba(0, 161, 154, 0.5)';
        alarmText.textContent = 'Silent';
        alarmText.style.color = '#00a19a';
    }
}

//Graying when wifi disconnects
function markAllReadingsStale() {
    // Find all elements with the "reading" class (your sensor values)
    document.querySelectorAll('.reading span, .reading-value').forEach(element => {
        element.style.opacity = "0.4";
        element.style.color = "#999999";
        element.style.fontStyle = "italic";
    });
}

function initPlotDataStructures() {
    // Get actual values from ESP32 data, or use defaults
    const intervalMs = window._lastKnownInterval || 200;
    liveWindowSec = window._lastKnownTimeWindow || 8; // visible span (SECONDS)
    const timeWindowMs = LIVE_BUFFER_SEC * 1000; // buffer always 5 min
    // Calculate correct buffer size
    const maxPoints = Math.ceil(timeWindowMs / intervalMs);
    const intervalSec = intervalMs / 1000;
    if (useTimestamps) {
        // TIMESTAMP MODE
        const now = Math.floor(Date.now() / 1000);
        xAxisData = [];
        for (let i = 0; i < maxPoints; i++) {
            xAxisData[i] = now - (maxPoints - 1 - i) * intervalSec;
        }
    } else {
        // RELATIVE TIME MODE
        xAxisData = [];
        for (let i = 0; i < maxPoints; i++) {
            xAxisData[i] = -(maxPoints - 1 - i) * intervalSec;
        }
    }

    // Initialize all buffers with correct size
    currentTempData = [
        [...xAxisData],
        new Array(maxPoints).fill(0),
        new Array(maxPoints).fill(0),
        new Array(maxPoints).fill(0),
        new Array(maxPoints).fill(0)  // Field% (duty cycle)
    ];

    voltageData = [
        [...xAxisData],
        new Array(maxPoints).fill(0),
        new Array(maxPoints).fill(0),
        new Array(maxPoints).fill(0)  // Field% (duty cycle)
    ];

    rpmData = [
        [...xAxisData],
        new Array(maxPoints).fill(0),
        new Array(maxPoints).fill(0)  // Field% (duty cycle)
    ];

    temperatureData = [
        [...xAxisData],
        new Array(maxPoints).fill(0),
        new Array(maxPoints).fill(0)  // Field% (duty cycle)
    ];
}

// Splits factory for scales whose range can be user-pinned (manual). Auto mode
// reproduces uPlot's default numeric splits exactly (multiples of the increment
// it chose for the space), so autoscale look and perf are unchanged. Manual mode
// keeps those nice interior ticks but labels the EXACT min/max at the plot
// corners, dropping any interior tick that would crowd an endpoint label — the
// printed corner values are always the user's pinned bounds, i.e. exactly what
// the click-to-edit boxes (attachYAxisEdit) read and write.
function edgeLabeledSplits(isManual) {
    return (u, axisIdx, mn, mx, incr) => {
        const ticks = [];
        for (let i = Math.ceil(mn / incr - 1e-9); i * incr <= mx + incr * 1e-9; i++)
            ticks.push(+(i * incr).toFixed(12));
        if (!isManual || !isManual()) return ticks;
        const guard = incr * 0.5;   // half a tick step keeps endpoint labels readable
        return [mn, ...ticks.filter(v => v - mn >= guard && mx - v >= guard), mx];
    };
}

// =====================================================================
// On-plot Y-axis editing — makes each Y axis's extreme tick labels
// directly editable. uPlot draws tick labels on canvas, so they can't
// take click handlers themselves; instead an invisible click zone sits
// over each axis's min and max label. Clicking it summons an edit box
// in place, prefilled with the current bound. Enter (or tap away)
// applies, Escape cancels, and where the chart supports automatic
// ranging, clearing the value and pressing Enter returns it to auto.
// cfgs: [{ scale, apply(min,max), auto() optional, decimals optional }]
function attachYAxisEdit(u, cfgs) {
    if (!u || !u.over) return;
    const wrap = u.over.parentElement;   // .u-wrap — contains plot area AND axis strips
    if (!wrap) return;
    wrap.querySelectorAll('.yaxis-hot, .yaxis-edit').forEach(n => n.remove());

    const fmt = (v, cfg) => {
        if (v == null || !isFinite(v)) return '';
        let d = cfg.decimals;
        if (d === undefined) {
            const sc = u.scales[cfg.scale];
            const span = (sc && isFinite(sc.max - sc.min)) ? Math.abs(sc.max - sc.min) : 0;
            d = span >= 100 ? 0 : span >= 10 ? 1 : 2;
        }
        return String(parseFloat(v.toFixed(d)));
    };

    // Touch: taller tap targets and a wider edit box (the box also gets a
    // 16px font from CSS — anything smaller makes iOS auto-zoom the page)
    const coarse = window.matchMedia && window.matchMedia('(pointer: coarse)').matches;
    const HOT_H = coarse ? 28 : 18, HOT_W = 46, INP_W = coarse ? 76 : 58;
    const zones = [];

    // One shared edit box, summoned to whichever label was clicked
    const inp = document.createElement('input');
    inp.type = 'number';
    inp.step = 'any';
    inp.className = 'yaxis-edit';
    ['mousedown', 'mouseup', 'touchstart', 'click', 'dblclick'].forEach(ev =>
        inp.addEventListener(ev, e => e.stopPropagation()));
    wrap.appendChild(inp);
    let editing = null;   // active zone, null while the box is hidden

    const hideInput = () => { editing = null; inp.style.display = 'none'; };

    const commit = () => {
        if (!editing) return;
        const z = editing;
        const sc = u.scales[z.cfg.scale];
        const raw = inp.value.trim();
        if (raw === '' && z.cfg.auto) { z.cfg.auto(); hideInput(); return; }
        const v = parseFloat(raw);
        if (isFinite(v) && sc) {
            const mn = z.isMax ? sc.min : v;
            const mx = z.isMax ? v : sc.max;
            if (mx > mn) z.cfg.apply(mn, mx);
        }
        hideInput();
    };

    inp.addEventListener('keydown', e => {
        e.stopPropagation();
        if (e.key === 'Enter') { e.preventDefault(); commit(); }
        else if (e.key === 'Escape') hideInput();
    });
    inp.addEventListener('blur', () => commit());

    cfgs.forEach(cfg => {
        const axis = u.axes.find(a => a.scale === cfg.scale);
        if (!axis) return;
        const onRight = axis.side === 1;

        [true, false].forEach(isMax => {
            const hot = document.createElement('div');
            hot.className = 'yaxis-hot';
            hot.title = 'Click to edit axis ' + (isMax ? 'maximum' : 'minimum')
                + (cfg.auto ? ' — clear + Enter for auto' : '');
            // Keep clicks/drags away from uPlot's cursor + zoom handling
            ['mousedown', 'touchstart', 'dblclick', 'click'].forEach(ev =>
                hot.addEventListener(ev, e => e.stopPropagation()));
            const zone = { cfg, isMax, onRight, hot };
            // Open on mouseup, NOT click: uPlot installs a capture-phase click
            // listener on .u-wrap (its drag-end click suppressor) whose moved-cursor
            // check is always true over the axis strip, so it swallows every click
            // before it can reach this element — a click handler here never fires.
            // The armed flag ignores mouseups from drags that started elsewhere
            // (e.g. releasing a plot-area zoom drag on top of an axis label).
            let armed = false;
            hot.addEventListener('mousedown', () => { armed = true; });
            hot.addEventListener('mouseleave', () => { armed = false; });
            hot.addEventListener('mouseup', e => {
                e.stopPropagation();
                if (!armed) return;
                armed = false;
                const sc = u.scales[cfg.scale];
                if (!sc) return;
                editing = zone;
                inp.value = fmt(isMax ? sc.max : sc.min, cfg);
                const w = Math.max(INP_W, hot.offsetWidth);
                inp.style.width = w + 'px';
                inp.style.left = (onRight ? hot.offsetLeft : (hot.offsetLeft + hot.offsetWidth - w)) + 'px';
                inp.style.top = hot.offsetTop + 'px';
                inp.style.display = 'block';
                inp.focus();
                inp.select();
            });
            wrap.appendChild(hot);
            zones.push(zone);
        });
    });

    // Place the zones over the extreme tick labels: vertically centered on the
    // plot area's top/bottom edge (where uPlot centers those labels), anchored
    // to the plot-edge side of the axis strip so a rotated axis title at the
    // strip's far edge stays unclickable. Re-run every draw — offsets move on
    // resize and axis-width changes.
    const position = () => {
        const oL = u.over.offsetLeft, oT = u.over.offsetTop,
            oW = u.over.offsetWidth, oH = u.over.offsetHeight;
        zones.forEach(z => {
            const strip = z.onRight ? (wrap.clientWidth - oL - oW) : oL;
            const w = Math.min(HOT_W, Math.max(30, strip - 2));
            z.hot.style.width = w + 'px';
            z.hot.style.height = HOT_H + 'px';
            z.hot.style.left = (z.onRight ? (oL + oW + 1) : (oL - w - 1)) + 'px';
            z.hot.style.top = Math.max(0, (z.isMax ? oT : oT + oH) - HOT_H / 2) + 'px';
        });
    };

    (u.hooks.draw = u.hooks.draw || []).push(position);
    position();
}

// Persist a Y-axis range to the regulator (same params the settings forms use).
// Settings are password-gated — while the dashboard is locked the new range
// still applies, it just stays local to this browser session.
function sendYAxisSetting(params) {
    if (!currentAdminPassword) return;
    const q = new URLSearchParams(params);
    q.set('password', currentAdminPassword);
    fetchWithTimeout(buildURL('/get?' + q.toString()), {}, 5000).catch(() => { });
}

function initCurrentTempPlot() {
    const plotEl = document.getElementById('current-temp-plot');
    if (!plotEl) {
        diagError("Current & Temperature plot element not found");
        return;
    }

    const opts = {
        width: Math.min(plotEl.clientWidth, 800),
        height: 300,
        title: "Current History",
        series: [
            { label: null, points: { show: false }, stroke: "transparent", width: 0 },
            {
                label: "Battery Current (A)",
                stroke: "#4CAF50",
                width: 2,
                scale: "current"
            },
            {
                label: "Alternator Current (A)",
                stroke: "#2196F3",
                width: 2,
                scale: "current"
            },
            {
                label: "Field Current (A)",
                stroke: "#9C27B0",
                width: 2,
                scale: "current"
            },
            {
                label: "Field %",
                stroke: "#9E9E9E",
                width: 2,
                scale: "pct",
                dash: [4, 2]
            }
        ],
        scales: useTimestamps ? {
            x: { time: true, range: (u, dMin, dMax) => [dMax - liveWindowSec, dMax] },
            current: { auto: false, range: () => [Ymin1, Ymax1] },   // fn, not array: re-read each redraw so Y edits survive setData re-ranging
            pct: { auto: false, range: [0, 100] }
        } : {
            x: { time: false, range: (u, dMin, dMax) => [-liveWindowSec, 0] },
            current: { auto: false, range: () => [Ymin1, Ymax1] },   // fn, not array: re-read each redraw so Y edits survive setData re-ranging
            pct: { auto: false, range: [0, 100] }
        },
        axes: useTimestamps ? [
            { grid: { show: true } },
            {
                scale: "current",
                label: "Amperes",
                grid: { show: true },
                side: 3,
                splits: edgeLabeledSplits(() => !autoScaleCurrent)
            },
            {
                scale: "pct",
                label: "Field %",
                grid: { show: false },
                side: 1,
                values: (u, ticks) => ticks.map(v => Math.round(v) + '%')
            }
        ] : [
            {
                label: "Seconds Ago",
                grid: { show: true }
            },
            {
                scale: "current",
                label: "Amperes",
                grid: { show: true },
                side: 3,
                splits: edgeLabeledSplits(() => !autoScaleCurrent)
            },
            {
                scale: "pct",
                label: "Field %",
                grid: { show: false },
                side: 1,
                values: (u, ticks) => ticks.map(v => Math.round(v) + '%')
            }
        ],
        legend: {
            show: false
        },
        plugins: [{
            hooks: {
                init: [
                    (u) => {
                        createCustomLegend('current-temp-plot', [
                            { label: "Battery Current (A)", color: "#4CAF50" },
                            { label: "Alternator Current (A)", color: "#2196F3" },
                            { label: "Field Current (A)", color: "#9C27B0" },
                            { label: "Field %", color: "#9E9E9E" }
                        ], u);

                        const resizePlot = debounce(() => {
                            const plotEl = document.getElementById("current-temp-plot");
                            if (plotEl && currentTempPlot) {
                                currentTempPlot.setSize({ width: plotEl.clientWidth, height: 300 });
                            }
                        }, 1000);

                        if (currentTempResizeObserver) {
                            currentTempResizeObserver.disconnect();
                        }
                        currentTempResizeObserver = new ResizeObserver(resizePlot);
                        currentTempResizeObserver.observe(plotEl);
                    }
                ]
            }
        }]
    };

    currentTempPlot = new uPlot(opts, currentTempData, plotEl);
    if (document.body.classList.contains('dark-mode')) updateUplotTheme(currentTempPlot);

    // Autoscale checkbox + lock button — re-injected on each init so re-inits stay clean
    plotEl.style.position = 'relative';
    const existingAs = plotEl.querySelector('.autoscale-ctrl');
    if (existingAs) existingAs.remove();
    autoScaleCurrentLocked = false;
    const asDiv = document.createElement('div');
    asDiv.className = 'autoscale-ctrl';
    asDiv.style.cssText = 'position:absolute;top:6px;right:8px;z-index:10;display:flex;flex-direction:column;align-items:flex-end;gap:2px;font-size:11px;';
    asDiv.innerHTML = '<div style="display:flex;align-items:center;gap:3px;opacity:0.6;"><input type="checkbox" id="autoscale-current-cb" style="cursor:pointer;width:12px;height:12px;margin:0;"><label for="autoscale-current-cb" style="cursor:pointer;user-select:none;">auto</label></div><button id="lock-current-btn" style="display:none;font-size:10px;padding:0 5px;cursor:pointer;border:1px solid #999;border-radius:2px;background:transparent;opacity:0.6;line-height:16px;">lock</button>';
    plotEl.appendChild(asDiv);
    const asCb = document.getElementById('autoscale-current-cb');
    const lockBtnC = document.getElementById('lock-current-btn');
    asCb.checked = autoScaleCurrent;
    if (autoScaleCurrent) lockBtnC.style.display = 'block';
    lockBtnC.addEventListener('click', () => {
        autoScaleCurrentLocked = !autoScaleCurrentLocked;
        lockBtnC.textContent = autoScaleCurrentLocked ? 'unlock' : 'lock';
        lockBtnC.style.opacity = autoScaleCurrentLocked ? '1' : '0.6';
    });
    asCb.addEventListener('change', e => {
        autoScaleCurrent = e.target.checked;
        localStorage.setItem('autoScaleCurrent', autoScaleCurrent);
        lockBtnC.style.display = autoScaleCurrent ? 'block' : 'none';
        if (!autoScaleCurrent) {
            autoScaleCurrentLocked = false;
            lockBtnC.textContent = 'lock';
            lockBtnC.style.opacity = '0.6';
            _autoScaleCurrentLeft = null;
            _autoScaleCurrentRight = null;
            currentTempPlot.setScale('current', { min: Ymin1, max: Ymax1 });
            currentTempPlot.setScale('pct', { min: 0, max: 100 });
        }
    });

    // Click-to-edit Y limits, saved to the regulator as Ymin1/Ymax1
    attachYAxisEdit(currentTempPlot, [{
        scale: 'current', decimals: 0,
        apply: (mn, mx) => {
            Ymin1 = Math.round(mn); Ymax1 = Math.round(mx);
            if (asCb.checked) { asCb.checked = false; asCb.dispatchEvent(new Event('change')); }
            else currentTempPlot.setScale('current', { min: Ymin1, max: Ymax1 });
            sendYAxisSetting({ Ymin1: Ymin1, Ymax1: Ymax1 });
        }
    }]);
}

function initVoltagePlot() {
    const plotEl = document.getElementById('voltage-plot');
    if (!plotEl) {
        diagError("Voltage plot element not found");
        return;
    }

    const opts = {
        width: Math.min(plotEl.clientWidth, 800),
        height: 300,
        title: "Battery Voltage History",
        series: [
            { label: null, points: { show: false }, stroke: "transparent", width: 0 },
            {
                label: "ADS Battery (V)",
                stroke: "#FF9800",
                width: 2,
                scale: "voltage"
            },
            {
                label: "INA Battery (V)",
                stroke: "#4CAF50",
                width: 2,
                scale: "voltage"
            },
            {
                label: "Field %",
                stroke: "#9E9E9E",
                width: 2,
                scale: "pct",
                dash: [4, 2]
            }
        ],
        scales: useTimestamps ? {
            x: { time: true, range: (u, dMin, dMax) => [dMax - liveWindowSec, dMax] },
            voltage: { auto: false, range: () => [Ymin2 / 100, Ymax2 / 100] },   // fn, not array: re-read each redraw so Y edits survive setData re-ranging
            pct: { auto: false, range: [0, 100] }
        } : {
            x: { time: false, range: (u, dMin, dMax) => [-liveWindowSec, 0] },
            voltage: { auto: false, range: () => [Ymin2 / 100, Ymax2 / 100] },   // fn, not array: re-read each redraw so Y edits survive setData re-ranging
            pct: { auto: false, range: [0, 100] }
        },
        axes: useTimestamps ? [
            { grid: { show: true } },
            {
                scale: "voltage",
                label: "Volts",
                grid: { show: true },
                side: 3,
                splits: edgeLabeledSplits(() => !autoScaleVoltage)
            },
            {
                scale: "pct",
                label: "Field %",
                grid: { show: false },
                side: 1,
                values: (u, ticks) => ticks.map(v => Math.round(v) + '%')
            }
        ] : [
            {
                label: "Seconds Ago",
                grid: { show: true }
            },
            {
                scale: "voltage",
                label: "Volts",
                grid: { show: true },
                side: 3,
                splits: edgeLabeledSplits(() => !autoScaleVoltage)
            },
            {
                scale: "pct",
                label: "Field %",
                grid: { show: false },
                side: 1,
                values: (u, ticks) => ticks.map(v => Math.round(v) + '%')
            }
        ],
        legend: {
            show: false
        },
        plugins: [{
            hooks: {
                init: [
                    (u) => {
                        createCustomLegend('voltage-plot', [
                            { label: "ADS Battery (V)", color: "#FF9800" },
                            { label: "INA Battery (V)", color: "#4CAF50" },
                            { label: "Field %", color: "#9E9E9E" }
                        ], u);

                        const resizePlot = debounce(() => {
                            const plotEl = document.getElementById("voltage-plot");
                            if (plotEl && voltagePlot) {
                                voltagePlot.setSize({ width: plotEl.clientWidth, height: 300 });
                            }
                        }, 1000);

                        if (voltageResizeObserver) {
                            voltageResizeObserver.disconnect();
                        }
                        voltageResizeObserver = new ResizeObserver(resizePlot);
                        voltageResizeObserver.observe(plotEl);
                    }
                ]
            }
        }]
    };

    voltagePlot = new uPlot(opts, voltageData, plotEl);
    if (document.body.classList.contains('dark-mode')) updateUplotTheme(voltagePlot);

    plotEl.style.position = 'relative';
    const existingAsV = plotEl.querySelector('.autoscale-ctrl');
    if (existingAsV) existingAsV.remove();
    autoScaleVoltageLocked = false;
    const asDivV = document.createElement('div');
    asDivV.className = 'autoscale-ctrl';
    asDivV.style.cssText = 'position:absolute;top:6px;right:8px;z-index:10;display:flex;flex-direction:column;align-items:flex-end;gap:2px;font-size:11px;';
    asDivV.innerHTML = '<div style="display:flex;align-items:center;gap:3px;opacity:0.6;"><input type="checkbox" id="autoscale-voltage-cb" style="cursor:pointer;width:12px;height:12px;margin:0;"><label for="autoscale-voltage-cb" style="cursor:pointer;user-select:none;">auto</label></div><button id="lock-voltage-btn" style="display:none;font-size:10px;padding:0 5px;cursor:pointer;border:1px solid #999;border-radius:2px;background:transparent;opacity:0.6;line-height:16px;">lock</button>';
    plotEl.appendChild(asDivV);
    const asCbV = document.getElementById('autoscale-voltage-cb');
    const lockBtnV = document.getElementById('lock-voltage-btn');
    asCbV.checked = autoScaleVoltage;
    if (autoScaleVoltage) lockBtnV.style.display = 'block';
    lockBtnV.addEventListener('click', () => {
        autoScaleVoltageLocked = !autoScaleVoltageLocked;
        lockBtnV.textContent = autoScaleVoltageLocked ? 'unlock' : 'lock';
        lockBtnV.style.opacity = autoScaleVoltageLocked ? '1' : '0.6';
    });
    asCbV.addEventListener('change', e => {
        autoScaleVoltage = e.target.checked;
        localStorage.setItem('autoScaleVoltage', autoScaleVoltage);
        lockBtnV.style.display = autoScaleVoltage ? 'block' : 'none';
        if (!autoScaleVoltage) {
            autoScaleVoltageLocked = false;
            lockBtnV.textContent = 'lock';
            lockBtnV.style.opacity = '0.6';
            _autoScaleVoltageLeft = null;
            _autoScaleVoltageRight = null;
            voltagePlot.setScale('voltage', { min: Ymin2 / 100, max: Ymax2 / 100 });
            voltagePlot.setScale('pct', { min: 0, max: 100 });
        }
    });

    // Click-to-edit Y limits in volts, saved to the regulator as Ymin2/Ymax2
    attachYAxisEdit(voltagePlot, [{
        scale: 'voltage', decimals: 2,
        apply: (mn, mx) => {
            Ymin2 = mn * 100; Ymax2 = mx * 100;
            if (asCbV.checked) { asCbV.checked = false; asCbV.dispatchEvent(new Event('change')); }
            else voltagePlot.setScale('voltage', { min: mn, max: mx });
            sendYAxisSetting({ Ymin2: mn, Ymax2: mx });
        }
    }]);
}

function initRPMPlot() {
    const plotEl = document.getElementById('rpm-plot');
    if (!plotEl) {
        diagError("RPM plot element not found");
        return;
    }

    const opts = {
        width: Math.min(plotEl.clientWidth, 800),
        height: 300,
        title: "RPM History",
        series: [
            { label: null, points: { show: false }, stroke: "transparent", width: 0 },
            {
                label: "RPM",
                stroke: "#E91E63",
                width: 2,
                scale: "rpm"
            },
            {
                label: "Field %",
                stroke: "#9E9E9E",
                width: 2,
                scale: "pct",
                dash: [4, 2]
            }
        ],
        scales: useTimestamps ? {
            x: { time: true, range: (u, dMin, dMax) => [dMax - liveWindowSec, dMax] },
            rpm: { auto: false, range: () => [Ymin3, Ymax3] },   // fn, not array: re-read each redraw so Y edits survive setData re-ranging
            pct: { auto: false, range: [0, 100] }
        } : {
            x: { time: false, range: (u, dMin, dMax) => [-liveWindowSec, 0] },
            rpm: { auto: false, range: () => [Ymin3, Ymax3] },   // fn, not array: re-read each redraw so Y edits survive setData re-ranging
            pct: { auto: false, range: [0, 100] }
        },

        axes: useTimestamps ? [
            { grid: { show: true } },
            {
                scale: "rpm",
                label: "revs/min",
                grid: { show: true },
                side: 3,
                splits: edgeLabeledSplits(() => !autoScaleRPM)
            },
            {
                scale: "pct",
                label: "Field %",
                grid: { show: false },
                side: 1,
                values: (u, ticks) => ticks.map(v => Math.round(v) + '%')
            }
        ] : [
            {
                label: "Seconds Ago",
                grid: { show: true }
            },
            {
                scale: "rpm",
                label: "revs/min",
                grid: { show: true },
                side: 3,
                splits: edgeLabeledSplits(() => !autoScaleRPM)
            },
            {
                scale: "pct",
                label: "Field %",
                grid: { show: false },
                side: 1,
                values: (u, ticks) => ticks.map(v => Math.round(v) + '%')
            }
        ],
        legend: {
            show: false
        },
        plugins: [{
            hooks: {
                init: [
                    (u) => {
                        createCustomLegend('rpm-plot', [
                            { label: "RPM", color: "#E91E63" },
                            { label: "Field %", color: "#9E9E9E" }
                        ], u);

                        const resizePlot = debounce(() => {
                            const plotEl = document.getElementById("rpm-plot");
                            if (plotEl && rpmPlot) {
                                rpmPlot.setSize({ width: plotEl.clientWidth, height: 300 });
                            }
                        }, 1000);

                        if (rpmResizeObserver) {
                            rpmResizeObserver.disconnect();
                        }
                        rpmResizeObserver = new ResizeObserver(resizePlot);
                        rpmResizeObserver.observe(plotEl);
                    }
                ]
            }
        }]
    };

    rpmPlot = new uPlot(opts, rpmData, plotEl);
    if (document.body.classList.contains('dark-mode')) updateUplotTheme(rpmPlot);

    plotEl.style.position = 'relative';
    const existingAsR = plotEl.querySelector('.autoscale-ctrl');
    if (existingAsR) existingAsR.remove();
    autoScaleRPMLocked = false;
    const asDivR = document.createElement('div');
    asDivR.className = 'autoscale-ctrl';
    asDivR.style.cssText = 'position:absolute;top:6px;right:8px;z-index:10;display:flex;flex-direction:column;align-items:flex-end;gap:2px;font-size:11px;';
    asDivR.innerHTML = '<div style="display:flex;align-items:center;gap:3px;opacity:0.6;"><input type="checkbox" id="autoscale-rpm-cb" style="cursor:pointer;width:12px;height:12px;margin:0;"><label for="autoscale-rpm-cb" style="cursor:pointer;user-select:none;">auto</label></div><button id="lock-rpm-btn" style="display:none;font-size:10px;padding:0 5px;cursor:pointer;border:1px solid #999;border-radius:2px;background:transparent;opacity:0.6;line-height:16px;">lock</button>';
    plotEl.appendChild(asDivR);
    const asCbR = document.getElementById('autoscale-rpm-cb');
    const lockBtnR = document.getElementById('lock-rpm-btn');
    asCbR.checked = autoScaleRPM;
    if (autoScaleRPM) lockBtnR.style.display = 'block';
    lockBtnR.addEventListener('click', () => {
        autoScaleRPMLocked = !autoScaleRPMLocked;
        lockBtnR.textContent = autoScaleRPMLocked ? 'unlock' : 'lock';
        lockBtnR.style.opacity = autoScaleRPMLocked ? '1' : '0.6';
    });
    asCbR.addEventListener('change', e => {
        autoScaleRPM = e.target.checked;
        localStorage.setItem('autoScaleRPM', autoScaleRPM);
        lockBtnR.style.display = autoScaleRPM ? 'block' : 'none';
        if (!autoScaleRPM) {
            autoScaleRPMLocked = false;
            lockBtnR.textContent = 'lock';
            lockBtnR.style.opacity = '0.6';
            _autoScaleRPMLeft = null;
            _autoScaleRPMRight = null;
            rpmPlot.setScale('rpm', { min: Ymin3, max: Ymax3 });
            rpmPlot.setScale('pct', { min: 0, max: 100 });
        }
    });

    // Click-to-edit Y limits, saved to the regulator as Ymin3/Ymax3
    attachYAxisEdit(rpmPlot, [{
        scale: 'rpm', decimals: 0,
        apply: (mn, mx) => {
            Ymin3 = Math.round(mn); Ymax3 = Math.round(mx);
            if (asCbR.checked) { asCbR.checked = false; asCbR.dispatchEvent(new Event('change')); }
            else rpmPlot.setScale('rpm', { min: Ymin3, max: Ymax3 });
            sendYAxisSetting({ Ymin3: Ymin3, Ymax3: Ymax3 });
        }
    }]);
}

function initTemperaturePlot() {
    const plotEl = document.getElementById('temperature-plot');
    if (!plotEl) {
        diagError("Temperature plot element not found");
        return;
    }

    const opts = {
        width: Math.min(plotEl.clientWidth, 800),
        height: 300,
        title: "Alternator Temperature History",
        series: [
            { label: null, points: { show: false }, stroke: "transparent", width: 0 },
            {
                label: "Alt. Temp (°F)",
                stroke: "#FF5722",
                width: 2,
                scale: "temperature"
            },
            {
                label: "Field %",
                stroke: "#9E9E9E",
                width: 2,
                scale: "pct",
                dash: [4, 2]
            }
        ],
        scales: useTimestamps ? {
            x: { time: true, range: (u, dMin, dMax) => [dMax - liveWindowSec, dMax] },
            temperature: { auto: false, range: () => [Ymin4, Ymax4] },   // fn, not array: re-read each redraw so Y edits survive setData re-ranging
            pct: { auto: false, range: [0, 100] }
        } : {
            x: { time: false, range: (u, dMin, dMax) => [-liveWindowSec, 0] },
            temperature: { auto: false, range: () => [Ymin4, Ymax4] },   // fn, not array: re-read each redraw so Y edits survive setData re-ranging
            pct: { auto: false, range: [0, 100] }
        },
        axes: useTimestamps ? [
            { grid: { show: true } },
            {
                scale: "temperature",
                label: "F",
                grid: { show: true },
                side: 3,
                splits: edgeLabeledSplits(() => !autoScaleTemp)
            },
            {
                scale: "pct",
                label: "Field %",
                grid: { show: false },
                side: 1,
                values: (u, ticks) => ticks.map(v => Math.round(v) + '%')
            }
        ] : [
            {
                label: "Seconds Ago",
                grid: { show: true }
            },
            {
                scale: "temperature",
                label: "F",
                grid: { show: true },
                side: 3,
                splits: edgeLabeledSplits(() => !autoScaleTemp)
            },
            {
                scale: "pct",
                label: "Field %",
                grid: { show: false },
                side: 1,
                values: (u, ticks) => ticks.map(v => Math.round(v) + '%')
            }
        ],
        legend: {
            show: false
        },
        plugins: [{
            hooks: {
                init: [
                    (u) => {
                        createCustomLegend('temperature-plot', [
                            { label: "Alt. Temp (°F)", color: "#FF5722" },
                            { label: "Field %", color: "#9E9E9E" }
                        ], u);

                        const resizePlot = debounce(() => {
                            const plotEl = document.getElementById("temperature-plot");
                            if (plotEl && temperaturePlot) {
                                temperaturePlot.setSize({ width: plotEl.clientWidth, height: 300 });
                            }
                        }, 1000);

                        if (temperatureResizeObserver) {
                            temperatureResizeObserver.disconnect();
                        }
                        temperatureResizeObserver = new ResizeObserver(resizePlot);
                        temperatureResizeObserver.observe(plotEl);
                    }
                ]
            }
        }]
    };

    temperaturePlot = new uPlot(opts, temperatureData, plotEl);
    if (document.body.classList.contains('dark-mode')) updateUplotTheme(temperaturePlot);

    plotEl.style.position = 'relative';
    const existingAsT = plotEl.querySelector('.autoscale-ctrl');
    if (existingAsT) existingAsT.remove();
    autoScaleTempLocked = false;
    const asDivT = document.createElement('div');
    asDivT.className = 'autoscale-ctrl';
    asDivT.style.cssText = 'position:absolute;top:6px;right:8px;z-index:10;display:flex;flex-direction:column;align-items:flex-end;gap:2px;font-size:11px;';
    asDivT.innerHTML = '<div style="display:flex;align-items:center;gap:3px;opacity:0.6;"><input type="checkbox" id="autoscale-temp-cb" style="cursor:pointer;width:12px;height:12px;margin:0;"><label for="autoscale-temp-cb" style="cursor:pointer;user-select:none;">auto</label></div><button id="lock-temp-btn" style="display:none;font-size:10px;padding:0 5px;cursor:pointer;border:1px solid #999;border-radius:2px;background:transparent;opacity:0.6;line-height:16px;">lock</button>';
    plotEl.appendChild(asDivT);
    const asCbT = document.getElementById('autoscale-temp-cb');
    const lockBtnT = document.getElementById('lock-temp-btn');
    asCbT.checked = autoScaleTemp;
    if (autoScaleTemp) lockBtnT.style.display = 'block';
    lockBtnT.addEventListener('click', () => {
        autoScaleTempLocked = !autoScaleTempLocked;
        lockBtnT.textContent = autoScaleTempLocked ? 'unlock' : 'lock';
        lockBtnT.style.opacity = autoScaleTempLocked ? '1' : '0.6';
    });
    asCbT.addEventListener('change', e => {
        autoScaleTemp = e.target.checked;
        localStorage.setItem('autoScaleTemp', autoScaleTemp);
        lockBtnT.style.display = autoScaleTemp ? 'block' : 'none';
        if (!autoScaleTemp) {
            autoScaleTempLocked = false;
            lockBtnT.textContent = 'lock';
            lockBtnT.style.opacity = '0.6';
            _autoScaleTempLeft = null;
            _autoScaleTempRight = null;
            temperaturePlot.setScale('temperature', { min: Ymin4, max: Ymax4 });
            temperaturePlot.setScale('pct', { min: 0, max: 100 });
        }
    });

    // Click-to-edit Y limits, saved to the regulator as Ymin4/Ymax4
    attachYAxisEdit(temperaturePlot, [{
        scale: 'temperature', decimals: 0,
        apply: (mn, mx) => {
            Ymin4 = Math.round(mn); Ymax4 = Math.round(mx);
            if (asCbT.checked) { asCbT.checked = false; asCbT.dispatchEvent(new Event('change')); }
            else temperaturePlot.setScale('temperature', { min: Ymin4, max: Ymax4 });
            sendYAxisSetting({ Ymin4: Ymin4, Ymax4: Ymax4 });
        }
    }]);
}

//Staleness stuff
// Dims an entire metric-row when the metric doesn't apply in the current GPS mode (anchored vs underway).
// Targets the closest .metric-row ancestor so label and value both gray together.
// Distinct from stale styling: stale = data is old; mode-inactive = data isn't contextually relevant.
function applyModeStyle(valueElementId, isApplicable, inactiveTitle) {
    const el = document.getElementById(valueElementId);
    if (!el) return;
    const row = el.closest('.metric-row') || el;
    const isDark = document.body.classList.contains('dark-mode');
    const cacheKey = isApplicable ? 'mode-active' : (isDark ? 'mode-inactive-dark' : 'mode-inactive-light');
    if (row._modeState === cacheKey) return;
    row._modeState = cacheKey;
    if (!isApplicable) {
        row.style.opacity = '0.35';
        row.style.fontStyle = 'italic';
        row.title = inactiveTitle || 'Not active in current mode';
    } else {
        row.style.opacity = '1.0';
        row.style.fontStyle = 'normal';
        row.title = '';
    }
}

// window.imuMovingState: null = GPS unknown (show all), true = underway, false = anchored.
// Mirrors firmware hysteresis: ON above 1.7 kt, OFF below 1.3 kt.
window.imuMovingState = null;
function updateIMUMovingState(sogRaw) {
    const sog = sogRaw / 100;
    if (window.imuMovingState === null) {
        window.imuMovingState = sog > 1.5;
    } else if (window.imuMovingState && sog < 1.3) {
        window.imuMovingState = false;
    } else if (!window.imuMovingState && sog > 1.7) {
        window.imuMovingState = true;
    }
}

// Placeholder thresholds — adjust once real-world data is available
// Roll (heel deviation 2min): warn=5°, bad=12°
// Pitch (pitch deviation 2min): warn=3°, bad=8°
// Yaw (heading swing 2min): warn=20°, bad=45°
function updateAnchorColorCoding(data) {
    function applyAnchorColor(elementId, rawValue, scale, warnThresh, badThresh) {
        const el = document.getElementById(elementId);
        if (!el) return;
        el.classList.remove('anchor-good', 'anchor-warn', 'anchor-bad');
        const value = rawValue / scale;
        if (value < 0) return;  // sentinel — no data, leave unstyled
        if (value >= badThresh) {
            el.classList.add('anchor-bad');
        } else if (value >= warnThresh) {
            el.classList.add('anchor-warn');
        } else {
            el.classList.add('anchor-good');
        }
    }

    if (data.imu_heel_deviation_120s !== undefined)
        applyAnchorColor('imu_heel_deviation_120s_ID',  data.imu_heel_deviation_120s,  100, 5,  12);  // TODO: tune thresholds
    if (data.imu_pitch_deviation_120s !== undefined)
        applyAnchorColor('imu_pitch_deviation_120s_ID', data.imu_pitch_deviation_120s, 100, 3,  8);   // TODO: tune thresholds
    if (data.imu_heading_swing_120s !== undefined)
        applyAnchorColor('imu_heading_swing_120s_ID',   data.imu_heading_swing_120s,   10,  20, 45);  // TODO: tune thresholds
}

// Reorders the two context-sensitive card-groups inside the IMU grid so
// the relevant mode's section appears first. Runs only when moving state changes.
window._lastIMUMovingStateForReorder = undefined;
function reorderIMUSections(moving) {
    const neutralGroup = document.getElementById('imu-current-motion-group');
    const underwayGroup = document.getElementById('imu-underway-group');
    const anchorGroup   = document.getElementById('imu-anchor-group');
    if (!neutralGroup || !underwayGroup || !anchorGroup) return;

    if (moving === true) {
        neutralGroup.after(underwayGroup, anchorGroup);
    } else {
        neutralGroup.after(anchorGroup, underwayGroup);
    }
}

function updateIMUModeStyles() {
    if (window.imuMovingState === null) return;  // no GPS yet — leave everything at full opacity
    const moving = window.imuMovingState;

    // These metrics are only meaningful underway — gray them when anchored
    [
        'imu_msi_score_ID',
        'imu_vomit_pct_ID',
        'imu_wave_period_sec_ID',
        'imu_heel_deviation_60s_ID',
        'imu_heel_change_60s_ID',
        'imu_pitch_deviation_60s_ID',
        'imu_pitch_change_60s_ID',
    ].forEach(id => applyModeStyle(id, moving, 'Active underway only (SOG > 1.7 kt)'));

    // These metrics are only meaningful at anchor — gray them when underway
    [
        'imu_anchorage_comfort_ID',
        'imu_heel_deviation_120s_ID',
        'imu_pitch_deviation_120s_ID',
        'imu_heading_swing_120s_ID',
    ].forEach(id => applyModeStyle(id, !moving, 'Active at anchor only (SOG < 1.3 kt)'));

    // Reorder card-groups so the contextually active section is on top
    if (window._lastIMUMovingStateForReorder !== moving) {
        window._lastIMUMovingStateForReorder = moving;
        reorderIMUSections(moving);
    }
}

function applyStaleStyleByAge(elementId, ageMs, staleThreshold = STALE_THRESHOLD_DEFAULT_MS) {
    const element = document.getElementById(elementId);
    if (!element) {
        diagWarn(`Element ${elementId} not found for stale styling`);
        return;
    }

    const isStale = ageMs > staleThreshold;
    const isDark = document.body.classList.contains('dark-mode');

    // Cache key includes dark mode so toggling dark mode forces re-apply
    const cacheKey = isStale ? (isDark ? 'stale-dark' : 'stale-light') : 'fresh';
    if (element._lastStaleState === cacheKey) {
        return; // No change needed
    }

    element._lastStaleState = cacheKey;

    if (isStale) {
        // Light mode: fade toward light gray (harder to see on white bg)
        // Dark mode: fade toward dark gray (harder to see on dark bg)
        element.style.opacity = "0.5";
        element.style.color = isDark ? "#444444" : "#999999";
        element.style.fontStyle = "italic";
        element.title = `Data is ${Math.round(ageMs / 1000)} seconds old`;
    } else {
        element.style.opacity = "1.0";
        element.style.color = "var(--reading)";
        element.style.fontStyle = "normal";
        element.title = "";
    }
}
// Function to update all staleness styling
function updateAllStalenessStyles() {
    if (!window.sensorAges) return;
    const sa = window.sensorAges;

    // --- Primary sensor readings ---
    applyStaleStyleByAge("HeadingNMEAID", sa.heading);
    applyStaleStyleByAge("LatitudeNMEA_ID", sa.latitude);
    applyStaleStyleByAge("LongitudeNMEA_ID", sa.longitude);
    applyStaleStyleByAge("SatelliteCountNMEA_ID", sa.satellites);
    applyStaleStyleByAge("VictronVoltageID", sa.victronVoltage);
    applyStaleStyleByAge("VictronCurrentID", sa.victronCurrent);
    applyStaleStyleByAge("VictronSolarPowerID", sa.victronSolar);
    applyStaleStyleByAge("VictronSolarVoltageID", sa.victronSolar);
    applyStaleStyleByAge("VictronSolarCurrentID", sa.victronSolar);
    applyStaleStyleByAge("VictronChargeStateID", sa.victronSolar);
    applyStaleStyleByAge("VictronMPPTModeID", sa.victronSolar);
    applyStaleStyleByAge("VictronErrorID", sa.victronSolar);
    applyStaleStyleByAge("VictronYieldTodayID", sa.victronSolar);
    applyStaleStyleByAge("VictronMaxPowerTodayID", sa.victronSolar);
    applyStaleStyleByAge("VictronYieldYesterdayID", sa.victronSolar);
    applyStaleStyleByAge("VictronMaxPowerYesterdayID", sa.victronSolar);
    applyStaleStyleByAge("AltTempID", sa.alternatorTemp, STALE_THRESHOLD_TEMP_MS);
    applyStaleStyleByAge("temperatureThermistorID", sa.thermistorTemp, STALE_THRESHOLD_TEMP_MS);
    applyStaleStyleByAge("RPMID", sa.rpm);
    applyStaleStyleByAge("MeasAmpsID", sa.measuredAmps);
    applyStaleStyleByAge("BatteryVID", sa.batteryV);
    applyStaleStyleByAge("IBVID", sa.ibv);
    applyStaleStyleByAge("BCurrID", sa.bcur);
    applyStaleStyleByAge("dutyCycleID", sa.dutyCycle);
    applyStaleStyleByAge("FieldVoltsID", sa.fieldVolts);
    applyStaleStyleByAge("FieldAmpsID", sa.fieldAmps);

    // --- Header bar ---
    applyStaleStyleByAge("header-voltage", sa.ibv);
    applyStaleStyleByAge("header-soc", Math.max(sa.ibv, sa.bcur));      // fixed: was sensorAges.soc which was never populated
    applyStaleStyleByAge("header-alt-current", sa.measuredAmps);
    applyStaleStyleByAge("header-batt-current", sa.bcur);
    applyStaleStyleByAge("header-alt-temp", sa.alternatorTemp, STALE_THRESHOLD_TEMP_MS);
    applyStaleStyleByAge("header-rpm", sa.rpm);
    applyStaleStyleByAge("dutyCycleID3", sa.dutyCycle);

    // --- Battery / SoC — piggyback on ibv+bcur ---
    const socAge = Math.max(sa.ibv, sa.bcur);
    applyStaleStyleByAge("SOC_percentID", socAge);
    applyStaleStyleByAge("timeToFullChargeMinID", socAge);
    applyStaleStyleByAge("timeToFullDischargeMinID", socAge);

    // --- PID debug panel — piggyback on primary sensor ages ---
    applyStaleStyleByAge("pidInput_display", sa.measuredAmps);
    applyStaleStyleByAge("dutyCycleID2", sa.dutyCycle);
    applyStaleStyleByAge("FieldVoltsID2", sa.fieldVolts);
    applyStaleStyleByAge("FieldAmpsID2", sa.fieldAmps);

    // --- Thermal PID panel — piggyback on alternator temp ---
    applyStaleStyleByAge("tempPIDInput_display", sa.alternatorTemp, STALE_THRESHOLD_TEMP_MS);
    applyStaleStyleByAge("thermalPenaltyAmps_display", sa.alternatorTemp, STALE_THRESHOLD_TEMP_MS);

    // --- NMEA nav — dedicated timestamps ---
    applyStaleStyleByAge("COGNMEA_ID", sa.cogNMEA);
    applyStaleStyleByAge("SOGNMEA_ID", sa.sogNMEA);
    applyStaleStyleByAge("STWNMEA_ID", sa.stwNMEA);
    applyStaleStyleByAge("VMGNMEA_ID", sa.vmg);
    applyStaleStyleByAge("LeewayNMEA_ID", sa.leeway);
    applyStaleStyleByAge("ApparentWindSpeedNMEA_ID", sa.appWindSpeed);
    applyStaleStyleByAge("ApparentWindAngleNMEA_ID", sa.appWindAngle);
    applyStaleStyleByAge("TrueWindSpeedNMEA_ID", sa.trueWindSpeed);
    applyStaleStyleByAge("TrueWindAngleNMEA_ID", sa.trueWindAngle);

    // --- Baro / ambient — dedicated timestamps ---
    // Both come from the BMP388 on an 8s read cycle. Use the 12s threshold for
    // both so a single missed read does not flip the UI stale.
    applyStaleStyleByAge("baroPressureID", sa.baroPressure, STALE_THRESHOLD_TEMP_MS);
    applyStaleStyleByAge("ambientTempID", sa.ambientTemp, STALE_THRESHOLD_TEMP_MS);

    // --- IMU — all displays share one timestamp ---
    applyStaleStyleByAge("imu_heel_deg_ID", sa.imu);
    applyStaleStyleByAge("imu_pitch_deg_ID", sa.imu);
    applyStaleStyleByAge("imu_vertical_accel_g_ID", sa.imu);
    applyStaleStyleByAge("imu_total_accel_g_ID", sa.imu);
    applyStaleStyleByAge("imu_yaw_rate_dps_ID", sa.imu);
    applyStaleStyleByAge("imu_wave_period_sec_ID", sa.imu);
    applyStaleStyleByAge("imu_msi_score_ID", sa.imu);
    applyStaleStyleByAge("imu_vomit_pct_ID", sa.imu);
    applyStaleStyleByAge("imu_anchorage_comfort_ID", sa.imu);
    applyStaleStyleByAge("imu_heel_deviation_120s_ID", sa.imu);
    applyStaleStyleByAge("imu_pitch_deviation_120s_ID", sa.imu);
    applyStaleStyleByAge("imu_heading_swing_120s_ID", sa.imu);
    applyStaleStyleByAge("imu_min_moving_gentle_ID", sa.imu);
    applyStaleStyleByAge("imu_min_moving_moderate_ID", sa.imu);
    applyStaleStyleByAge("imu_min_moving_rough_ID", sa.imu);
    applyStaleStyleByAge("imu_min_moving_extreme_ID", sa.imu);
    applyStaleStyleByAge("imu_min_stat_gentle_ID", sa.imu);
    applyStaleStyleByAge("imu_min_stat_moderate_ID", sa.imu);
    applyStaleStyleByAge("imu_min_stat_rough_ID", sa.imu);
    applyStaleStyleByAge("imu_min_stat_extreme_ID", sa.imu);
    applyStaleStyleByAge("imu_accel_x_raw_ID", sa.imu);
    applyStaleStyleByAge("imu_accel_y_raw_ID", sa.imu);
    applyStaleStyleByAge("imu_accel_z_raw_ID", sa.imu);
    applyStaleStyleByAge("imu_gyro_x_raw_ID", sa.imu);
    applyStaleStyleByAge("imu_gyro_y_raw_ID", sa.imu);
    applyStaleStyleByAge("imu_gyro_z_raw_ID", sa.imu);
    applyStaleStyleByAge("imu_heel_change_60s_ID", sa.imu);
    applyStaleStyleByAge("imu_heel_deviation_60s_ID", sa.imu);
    applyStaleStyleByAge("imu_pitch_change_60s_ID", sa.imu);
    applyStaleStyleByAge("imu_pitch_deviation_60s_ID", sa.imu);
    applyStaleStyleByAge("accel_x_min_ID", sa.imu);
    applyStaleStyleByAge("accel_x_avg_ID", sa.imu);
    applyStaleStyleByAge("accel_x_max_ID", sa.imu);
    applyStaleStyleByAge("accel_y_min_ID", sa.imu);
    applyStaleStyleByAge("accel_y_avg_ID", sa.imu);
    applyStaleStyleByAge("accel_y_max_ID", sa.imu);
    applyStaleStyleByAge("accel_z_min_ID", sa.imu);
    applyStaleStyleByAge("accel_z_avg_ID", sa.imu);
    applyStaleStyleByAge("accel_z_max_ID", sa.imu);
    applyStaleStyleByAge("gyro_x_min_ID", sa.imu);
    applyStaleStyleByAge("gyro_x_avg_ID", sa.imu);
    applyStaleStyleByAge("gyro_x_max_ID", sa.imu);
    applyStaleStyleByAge("gyro_y_min_ID", sa.imu);
    applyStaleStyleByAge("gyro_y_avg_ID", sa.imu);
    applyStaleStyleByAge("gyro_y_max_ID", sa.imu);
    applyStaleStyleByAge("gyro_z_min_ID", sa.imu);
    applyStaleStyleByAge("gyro_z_avg_ID", sa.imu);
    applyStaleStyleByAge("gyro_z_max_ID", sa.imu);
    applyStaleStyleByAge("heel_min_ID", sa.imu);
    applyStaleStyleByAge("heel_avg_ID", sa.imu);
    applyStaleStyleByAge("heel_max_ID", sa.imu);
    applyStaleStyleByAge("pitch_min_ID", sa.imu);
    applyStaleStyleByAge("pitch_avg_ID", sa.imu);
    applyStaleStyleByAge("pitch_max_ID", sa.imu);
    applyStaleStyleByAge("vertical_accel_min_ID", sa.imu);
    applyStaleStyleByAge("vertical_accel_avg_ID", sa.imu);
    applyStaleStyleByAge("vertical_accel_max_ID", sa.imu);
    applyStaleStyleByAge("total_accel_min_ID", sa.imu);
    applyStaleStyleByAge("total_accel_avg_ID", sa.imu);
    applyStaleStyleByAge("total_accel_max_ID", sa.imu);

    updateWeatherAlerts();
}

// Start the staleness detection system - call this from window.load
function startStalenessDetection() {
    // Update staleness styling every 2 seconds
    setTrackedInterval(() => { updateAllStalenessStyles(); updateIMUModeStyles(); }, 2000);
}



// Custom legend creation function. Legend item order must match series order
// (item i → series i+1; slot 0 is the x axis). Pass the uPlot instance to make
// entries click-to-toggle; visibility persists per plot in localStorage so it
// survives reloads and the re-inits triggered by time-axis mode changes.
function createCustomLegend(plotId, legendItems, u) {
    const plotContainer = document.getElementById(plotId);
    if (!plotContainer) return;

    // Remove any existing custom legend
    const existingLegend = plotContainer.querySelector('.custom-legend');
    if (existingLegend) {
        existingLegend.remove();
    }

    const visKey = 'plotSeriesVis_' + plotId;
    let vis = {};
    try { vis = JSON.parse(localStorage.getItem(visKey)) || {}; } catch (e) { }

    // Create new custom legend
    const legendDiv = document.createElement('div');
    legendDiv.className = 'custom-legend';
    legendDiv.style.cssText = `
display: flex;
justify-content: center;
gap: 15px;
margin-top: 10px;
flex-wrap: wrap;
`;

    const hiddenIdxs = [];

    legendItems.forEach((item, i) => {
        const seriesIdx = i + 1;
        let shown = vis[item.label] !== false;

        const legendItem = document.createElement('div');
        legendItem.style.cssText = `
  display: flex;
  align-items: center;
  gap: 6px;
  font-size: 12px;
  ${u ? 'cursor: pointer; user-select: none;' : ''}
`;
        if (u) legendItem.title = 'Click to show/hide';

        const colorBox = document.createElement('div');
        colorBox.style.cssText = `
  width: 16px;
  height: 3px;
  background-color: ${item.color};
  border-radius: 1px;
`;

        const label = document.createElement('span');
        label.textContent = item.label;
        label.style.color = 'var(--text-dark)';

        const paint = () => {
            colorBox.style.opacity = shown ? '1' : '0.3';
            label.style.opacity = shown ? '1' : '0.5';
        };
        paint();

        if (u) {
            if (!shown) hiddenIdxs.push(seriesIdx);
            legendItem.addEventListener('click', () => {
                shown = !shown;
                u.setSeries(seriesIdx, { show: shown });
                vis[item.label] = shown;
                try { localStorage.setItem(visKey, JSON.stringify(vis)); } catch (e) { }
                paint();
            });
        }

        legendItem.appendChild(colorBox);
        legendItem.appendChild(label);
        legendDiv.appendChild(legendItem);
    });

    plotContainer.appendChild(legendDiv);

    // Apply saved hidden state after the constructor finishes — this runs from
    // the init hook, where an immediate setSeries redraw is unsafe.
    if (u && hiddenIdxs.length) {
        requestAnimationFrame(() => hiddenIdxs.forEach(idx => u.setSeries(idx, { show: false })));
    }
}


function setAdminPassword() {
    const button = document.getElementById('admin_password_set');
    const input = document.getElementById('admin_password');
    const msg = document.getElementById('admin_password_msg');
    const warning = document.getElementById('unlock-warning');
    if (warning) warning.style.display = 'none';
    const password = input.value;

    if (!password) {
        msg.textContent = "Missing Password";
        msg.style.color = "#00a19a";
        setTrackedTimeout(() => { msg.textContent = ""; }, 2000);
        return false;
    }

    button.disabled = true;

    const formData = new FormData();
    formData.append("password", password);

    fetchWithTimeout(buildURL("/checkPassword"), {
        method: "POST",
        body: formData
    }, 8000)
        .then(response => {
            if (!response.ok) throw new Error(`HTTP ${response.status}`);
            return response.text();
        })
        .then(text => {
            if (text.trim() === "OK") {

                currentAdminPassword = password;
                // Unlock settings
                const section = document.getElementById('settings-section');
                if (section) section.classList.remove("locked");
                document.querySelector('.permanent-header').classList.remove('locked');  // remove this bullshit probably

                // Also unlock header controls
                const headerControl = document.querySelector('.alternator-control');
                if (headerControl) headerControl.classList.remove("locked");

                const lockStatus = document.getElementById('lock-status');
                if (lockStatus) {
                    lockStatus.textContent = "Settings are Unlocked";
                    lockStatus.className = "lock-status-unlocked";
                }

                updatePasswordFields();
                msg.textContent = "Password Accepted";
                msg.style.color = "#00a19a";

                hideSettingsAccess();

                // Save (or refresh) the Keychain copy so the next cold launch
                // unlocks via Face ID. No-op outside Capacitor iOS.
                Bio.save(password).catch(() => { });
            }


            else {
                msg.textContent = "Wrong Password";
                msg.style.color = "#00a19a";

                // If this attempt came from a Face ID autofill, the saved
                // password is stale (e.g. user changed it via System Settings
                // on another device). Drop the Keychain copy so the user can
                // type the new one and re-save.
                if (__bioAutofillInFlight) {
                    Bio.clear().catch(() => { });
                }
            }
            __bioAutofillInFlight = false;
            input.value = "";
            setTrackedTimeout(() => { msg.textContent = ""; button.disabled = false; }, 2000);
        })
        .catch(err => {
            msg.textContent = "Error";
            msg.style.color = "#00a19a";
            setTrackedTimeout(() => { msg.textContent = ""; button.disabled = false; }, 2000);
        });

    return false;
}

function setNewPassword() {
    const button = document.getElementById('newpassword_set');
    const input = document.getElementById('newpassword');
    const msg = document.getElementById('newpassword_msg');
    const currentPassword = currentAdminPassword;

    if (!currentPassword || !input.value) {
        msg.textContent = "Missing fields";
        msg.style.color = "#00a19a";
        setTrackedTimeout(() => { msg.textContent = ""; }, 2000);
        return false;
    }

    button.disabled = true;

    const formData = new FormData();
    formData.append("password", currentPassword);
    formData.append("newpassword", input.value);

    fetchWithTimeout(buildURL("/setPassword"), {
        method: "POST",
        body: formData
    }, 8000)
        .then(response => {
            if (!response.ok) throw new Error(`HTTP ${response.status}`);
            return response.text();
        })
        .then(text => {
            if (text.trim() === "OK") {
                msg.textContent = "Password Changed";
                msg.style.color = "#00a19a";

                // Keep the Keychain copy in sync so next cold launch's Face ID
                // unlock uses the new password.
                Bio.save(input.value).catch(() => { });
            } else {
                msg.textContent = "Wrong Password";
                msg.style.color = "#00a19a";
            }
            input.value = "";
            setTrackedTimeout(() => { msg.textContent = ""; button.disabled = false; }, 2000);
        })
        .catch(err => {
            msg.textContent = "Error";
            msg.style.color = "#00a19a";
            setTrackedTimeout(() => { msg.textContent = ""; button.disabled = false; }, 2000);
        });

    return false; // prevent form submit
}

function triggerWeatherUpdate() {
    if (!currentAdminPassword) {
        alert("Please unlock settings first");
        return;
    }
    if (window._debugData && window._debugData.fieldActiveStatus > 0) {
        alert("The field must be disabled before updating weather data.");
        return;
    }

    const formData = new FormData();
    formData.append("password", currentAdminPassword);
    formData.append("TriggerWeatherUpdate", "1");

    fetchWithTimeout(buildURL("/get?" + new URLSearchParams(formData).toString()), {}, 8000)
        .then(() => diagLog("Weather update triggered"))
        .catch(err => diagError("Weather update failed:", err));
}

function updateGPSDisplay(lat, lon) {
    document.getElementById('LatitudeNMEA_display').textContent = lat.toFixed(5);
    document.getElementById('LongitudeNMEA_display').textContent = lon.toFixed(5);

    // Reflect the firmware's resolved source (stashed by the CSV2 dispatcher).
    // 0=none, 1=NMEA(boat), 2=Phone, 3=Manual(sticky override).
    const src = Number(window.currentGpsSource);
    const source = document.getElementById('gps-source');
    const clearBtn = document.getElementById('gps-clear-manual-row');
    const manualActive = (src === 3);

    // Suppress the client-side "stale" banner while a sticky manual override is
    // in force — the value is intentional, not stale telemetry.
    window.gpsManualOverride = manualActive;

    if (source) {
        if (lat === 0.0 && lon === 0.0) {
            source.textContent = '(No GPS — enter coordinates manually below)';
            source.style.color = '#ff6464';
        } else if (manualActive) {
            source.textContent = '(manual override — sticky, beats boat & phone GPS)';
            source.style.color = '#ff9800';
        } else if (src === 1) {
            source.textContent = '(from boat GPS / NMEA2000)';
            source.style.color = 'var(--accent)';
        } else if (src === 2) {
            source.textContent = '(from phone GPS)';
            source.style.color = 'var(--accent)';
        } else {
            source.textContent = '';
            source.style.color = 'var(--accent)';
        }
    }
    // Show the "Use automatic GPS" clear button only while manual is engaged.
    if (clearBtn) clearBtn.style.display = manualActive ? 'block' : 'none';
}



// Cap mode uses same pending/revision system as checkboxes
async function submitCapLimitModeImmediately(desiredValue) {
    const passwordField = document.querySelector('.password_field');
    const password = passwordField ? passwordField.value : '';

    const params = new URLSearchParams();
    params.set('capLimitMode', String(desiredValue));
    if (password) params.set('password', password);

    return fetchWithTimeout(buildURL(`/get?${params.toString()}`), {
        method: 'GET',
        cache: 'no-store'
    }, 4000);
}

function handleCapModeToggle(mode) {
    const desiredValue = (mode === 'kw') ? 1 : 0;

    pendingToggles.set('capLimitMode', {
        desiredValue: desiredValue,
        baseRev: lastSeenRev
    });

    const modeInput = document.getElementById('capLimitMode_input');
    if (modeInput) modeInput.value = String(desiredValue);

    setCapMode(mode); // optimistic UI immediately

    submitCapLimitModeImmediately(desiredValue).catch(err => {
        diagLog('capLimitMode submit failed: ' + err);
        // Do not manually revert — let SSE authoritative state resolve it
    });
}


function updateTogglesFromData(data) {
    try {
        if (!data) return;

        const now = Date.now();
        // Update time axis mode when ESP32 value changes
        if (data.timeAxisModeChanging !== undefined) {
            const newTimeAxisMode = (data.timeAxisModeChanging === 1);
            if (useTimestamps !== newTimeAxisMode) {
                useTimestamps = newTimeAxisMode;
                // Reinitialize plots with new mode
                initPlotDataStructures();
                if (currentTempPlot) { currentTempPlot.destroy(); initCurrentTempPlot(); }
                if (voltagePlot) { voltagePlot.destroy(); initVoltagePlot(); }
                if (rpmPlot) { rpmPlot.destroy(); initRPMPlot(); }
                if (temperaturePlot) { temperaturePlot.destroy(); initTemperaturePlot(); }
                reinitializeXAxisForNewMode();
            }
        }



        const updateCheckbox = (id, value, dataKey) => {
            if (value === undefined) return;

            // Pending toggle handling (prevent bounce / double-toggle)
            const pending = pendingToggles.get(dataKey);
            if (pending) {
                // Initialize timeout once
                if (pending.deadlineMs === undefined) {
                    pending.deadlineMs = Date.now() + 2500; // ms to wait for ESP32 echo
                }

                // ESP32 has NOT echoed desired value yet
                if (value !== pending.desiredValue) {
                    // Still waiting → keep optimistic UI, do not overwrite
                    if (Date.now() <= pending.deadlineMs) {
                        return;
                    }

                    // Timed out → accept ESP32 state, clear pending
                    pendingToggles.delete(dataKey);
                    // fall through and apply ESP32 state
                } else {
                    // Echo matches desired value → success, clear pending
                    pendingToggles.delete(dataKey);
                    // fall through (UI already matches)
                }
            }

            const checkbox = document.getElementById(id);
            if (!checkbox) return;

            const shouldBeChecked = (value === 1);
            if (checkbox.checked !== shouldBeChecked) {
                checkbox.checked = shouldBeChecked;
                toggleStates[dataKey] = value;
            }
            syncSegmented(id); // keep any segmented A/B control mirrored to firmware state (no-op if none)
        };

        // Update all toggle checkboxes with their data keys (keep existing calls)
        // ESP32 sends 0=PID, 1=Manual — invert so checked=PID matches the label order
        const invertedManual = data.ManualFieldToggle === undefined ? undefined : (data.ManualFieldToggle === 0 ? 1 : 0);
        updateCheckbox("ManualFieldToggle_checkbox", invertedManual, "ManualFieldToggle");
        updateCheckbox("SwitchControlOverride_checkbox", data.SwitchControlOverride, "SwitchControlOverride");
        updateCheckbox("header-alternator-enable", data.OnOff, "OnOff");
        updateCheckbox("LimpHome_checkbox", data.LimpHome, "LimpHome");
        // HiLow / charge rate mode handled via pendingToggles in CSVData3 handler
        updateCheckbox("VeData_checkbox", data.VeData, "VeData");
        updateCheckbox("NMEA0183Data_checkbox", data.NMEA0183Data, "NMEA0183Data");
        updateCheckbox("NMEA2KData_checkbox", data.NMEA2KData, "NMEA2KData");
        updateCheckbox("IgnoreTemperature_checkbox", data.IgnoreTemperature, "IgnoreTemperature");
        updateCheckbox("IgnoreRPM_checkbox", data.IgnoreRPM, "IgnoreRPM");
        updateCheckbox("bmsLogic_checkbox", data.bmsLogic, "bmsLogic");
        updateCheckbox("bmsLogicLevelOff_checkbox", data.bmsLogicLevelOff, "bmsLogicLevelOff");
        updateCheckbox("AlarmActivate_checkbox", data.AlarmActivate, "AlarmActivate");
        updateCheckbox("InvertAltAmps_checkbox", data.InvertAltAmps, "InvertAltAmps");
        updateCheckbox("InvertBattAmps_checkbox", data.InvertBattAmps, "InvertBattAmps");
        updateCheckbox("IgnitionOverride_checkbox", data.IgnitionOverride, "IgnitionOverride");
        updateCheckbox("TempSource_checkbox", data.TempSource, "TempSource");
        updateCheckbox("AlarmLatchEnabled_checkbox", data.AlarmLatchEnabled, "AlarmLatchEnabled");
        updateCheckbox("MaintainMode_checkbox", data.MaintainMode, "MaintainMode");
        updateCheckbox("TargetVoltageMode_checkbox", data.TargetVoltageMode, "TargetVoltageMode");
        updateCheckbox("AutoShuntGainCorrection_checkbox", data.AutoShuntGainCorrection, "AutoShuntGainCorrection");
        updateCheckbox("AutoAltCurrentZero_checkbox", data.AutoAltCurrentZero, "AutoAltCurrentZero");
        updateCheckbox("CVTuningMode_checkbox", data.CVTuningMode, "CVTuningMode");
        updateCheckbox("ThermalTuningMode_checkbox", data.ThermalTuningMode, "ThermalTuningMode");
        // Fast alt-current diagnostic toggles (Pattern B)
        updateSegToggle("faEnabled", data.faEnabled);
        updateSegToggle("faAlarmEnable", data.faAlarmEnable);
        faUpdatePauseBtn(data.faAnomPause);
        // Three checkboxes share the same firmware flag — all three reflect data.testProtectionsEnabled.
        updateCheckbox("testProtectionsEnabled_plant_checkbox",   data.testProtectionsEnabled, "testProtectionsEnabled");
        updateCheckbox("testProtectionsEnabled_current_checkbox", data.testProtectionsEnabled, "testProtectionsEnabled");
        updateCheckbox("testProtectionsEnabled_voltage_checkbox", data.testProtectionsEnabled, "testProtectionsEnabled");
        // Global banner — visible on every page when protections are DISABLED (value === 0).
        const protBanner = document.getElementById('protections-banner');
        if (protBanner && data.testProtectionsEnabled !== undefined) {
            protBanner.style.display = (data.testProtectionsEnabled === 0) ? 'block' : 'none';
        }
        updateCheckbox("timeAxisModeChanging_checkbox", data.timeAxisModeChanging, "timeAxisModeChanging");
        updateCheckbox("weatherModeEnabled_checkbox", data.weatherModeEnabled, "weatherModeEnabled");
        updateCheckbox("UseFloat_checkbox", data.UseFloat, "UseFloat");


        updateCheckbox("TuningMode_checkbox", data.TuningMode, "TuningMode");
        updateCheckbox("socInfoAvailable_checkbox", data.socInfoAvailable, "socInfoAvailable");
        updateCheckbox("CloudFeatures_checkbox", data.CloudFeatures, "CloudFeatures");
        updateCheckbox("wifiNapEnabled_checkbox", data.wifiNapEnabled, "wifiNapEnabled");
        // WiFi Napping only applies in Client mode (a SoftAP can't sleep) — hide the row in AP mode.
        const napRow = document.getElementById("wifiNapEnabled_row");
        if (napRow) napRow.style.display = (window._lastKnownMode === 1) ? "none" : "";  // 1 = MODE_AP
        updateCloudStatus();
        if (data.CloudFeatures !== undefined) {
            updateCloudFeaturesTabVisibility(data.CloudFeatures === 1);
        }
        updateCheckbox("HardwarePresent_checkbox", data.hardwarePresent, "hardwarePresent");
        updateCheckbox("OvGroup1Enable_checkbox", data.OvGroup1Enable, "OvGroup1Enable");
        updateCheckbox("OvGroup2Enable_checkbox", data.OvGroup2Enable, "OvGroup2Enable");
        if (data.IExcessSigSrc !== undefined)   updateTripleBtn('iExcessSigSrc_',   data.IExcessSigSrc);
        if (data.OutputPIDSigSrc !== undefined)  updateTripleBtn('outputPIDSigSrc_', data.OutputPIDSigSrc);
        // // Apply the ESP32 state to the plot system
        // if (data.timeAxisModeChanging !== undefined) {
        //     useTimestamps = (data.timeAxisModeChanging === 1);
        //     // Optionally reinitialize plots here if you want real-time updates
        // }
        updateFloatVisibility();
    } catch (e) {
        diagLog("Error updating toggle states: " + e.message);
    }
}
//function to handle user toggle changes without double toggle
function handleUserToggle(checkboxId, hiddenInputId, dataKey) {
    const checkbox = document.getElementById(checkboxId);
    const hiddenInput = document.getElementById(hiddenInputId);

    if (checkbox && hiddenInput) {
        const newValue = checkbox.checked ? '1' : '0';
        hiddenInput.value = newValue;

        // Track pending toggle with revision
        pendingToggles.set(dataKey, {
            desiredValue: parseInt(newValue),
            baseRev: lastSeenRev
        });

        return true;
    }
    return false;
}

// Initialize learning table flag BEFORE any event handlers
if (typeof window.learningTableInitialized === 'undefined') {
    window.learningTableInitialized = false;
}

// ===========================================================================
// TEST-MODE PROTECTION DISABLE — shared firmware flag exposed via three toggles
// (one on each of Plant Delay / Current Tuning / Voltage Tuning pages). When the
// user flips one, this helper mirrors the new state to the other two checkboxes
// optimistically so a mid-flight page switch doesn't show stale state, then
// delegates to handleUserToggle for the actual pending/echo plumbing.
// ===========================================================================
const TEST_PROT_CHECKBOXES = [
    'testProtectionsEnabled_plant_checkbox',
    'testProtectionsEnabled_current_checkbox',
    'testProtectionsEnabled_voltage_checkbox',
];
function mirrorTestProtectionsToggle(srcCheckboxId) {
    const src = document.getElementById(srcCheckboxId);
    if (!src) return;
    TEST_PROT_CHECKBOXES.forEach(id => {
        if (id === srcCheckboxId) return;
        const cb = document.getElementById(id);
        if (cb && cb.checked !== src.checked) cb.checked = src.checked;
    });
}



// ===========================================================================
// CAP TABLE — kW / AMPS MODE
// ===========================================================================

let currentCapMode = 'amps'; // tracks active mode; updated by setCapMode()

// ===========================================================================
// CAP TABLE — NORMAL / LOW CHARGE RATE MODE  (HiLow: 1=Normal, 0=Low)
// ===========================================================================

let currentChargeRateMode = 'high'; // tracks active mode; updated by setChargeRateMode()

async function submitChargeRateModeImmediately(desiredValue) {
    const passwordField = document.querySelector('.password_field');
    const password = passwordField ? passwordField.value : '';
    const params = new URLSearchParams();
    params.set('HiLow', String(desiredValue));
    if (password) params.set('password', password);
    return fetchWithTimeout(buildURL(`/get?${params.toString()}`), { method: 'GET', cache: 'no-store' }, 4000);
}

function setChargeRateMode(mode) {
    currentChargeRateMode = mode;
    const highBtn = document.getElementById('chargeRateHighBtn');
    const lowBtn = document.getElementById('chargeRateLowBtn');
    if (highBtn) highBtn.classList.toggle('cap-mode-active', mode === 'high');
    if (lowBtn) lowBtn.classList.toggle('cap-mode-active', mode === 'low');
    const container = document.getElementById('ltScroll');
    if (container) {
        container.classList.toggle('rate-normal', mode === 'high');
        container.classList.toggle('rate-low', mode === 'low');
    }
    const capRow = document.getElementById('capModeRow');
    if (capRow) {
        capRow.classList.toggle('rate-low', mode === 'low');
    }
}

function handleChargeRateModeToggle(mode) {
    const desiredValue = (mode === 'low') ? 0 : 1;
    pendingToggles.set('HiLow', { desiredValue: desiredValue, baseRev: lastSeenRev });
    setChargeRateMode(mode); // optimistic UI immediately
    submitChargeRateModeImmediately(desiredValue).catch(err => diagLog('chargeRateMode submit failed: ' + err));
}

function updateTripleBtn(prefix, val) {
    [0, 1, 2].forEach(i => {
        const btn = document.getElementById(prefix + i + '_btn');
        if (btn) btn.classList.toggle('cap-mode-active', i === val);
    });
}

function submitSimpleParam(paramName, val) {
    const pw = (document.querySelector('.password_field') || {}).value || '';
    const params = new URLSearchParams();
    params.set(paramName, String(val));
    if (pw) params.set('password', pw);
    fetchWithTimeout(buildURL('/get?' + params.toString()), { method: 'GET', cache: 'no-store' }, 4000)
        .catch(err => diagLog(paramName + ' submit failed: ' + err));
}

function setIExcessSigSrc(val) {
    updateTripleBtn('iExcessSigSrc_', val);
    submitSimpleParam('IExcessSigSrc', val);
}

function setOutputPIDSigSrc(val) {
    updateTripleBtn('outputPIDSigSrc_', val);
    submitSimpleParam('OutputPIDSigSrc', val);
    const nRow  = document.getElementById('outputPIDMA_N_row');
    const tcRow = document.getElementById('outputPIDFilterTC_row');
    if (nRow)  nRow.style.display  = (val === 1) ? '' : 'none';
    if (tcRow) tcRow.style.display = (val === 0) ? '' : 'none';
}

function getLiveBatteryV() {
    // IBV (INA228) is the primary voltage source; fall back to BatteryV (ADS1115), then 12V
    return ((window._debugData?.IBV) || (window._debugData?.BatteryV) || 1200) / 100;
}

function ampsToKW(amps) {
    if (!amps || amps === 0) return 0;
    return (getLiveBatteryV() * amps) / 1000;
}

function kwToAmps(kw) {
    if (!kw || kw === 0) return 0;
    const v = getLiveBatteryV();
    if (v < 0.5) return 0;
    return (kw * 1000) / v;
}


function setCapMode(mode) {
    currentCapMode = mode;

    // Update toggle button states
    const ampsBtn = document.getElementById('capModeAmpsBtn');
    const kwBtn = document.getElementById('capModeKWBtn');
    if (ampsBtn) ampsBtn.classList.toggle('cap-mode-active', mode === 'amps');
    if (kwBtn) kwBtn.classList.toggle('cap-mode-active', mode === 'kw');

    // Update hidden form field so mode is submitted with the table save
    const modeInput = document.getElementById('capLimitMode_input');
    if (modeInput) modeInput.value = (mode === 'kw') ? '1' : '0';

    // Apply derived/editable styling to all 10 rows
    for (let i = 0; i < 10; i++) {
        const ampInput = document.getElementById(`rpmCapCurrentTable${i}_input`);
        const kwInput = document.getElementById(`rpmCapKW${i}_input`);
        if (!ampInput || !kwInput) continue;

        if (mode === 'kw') {
            kwInput.readOnly = false;
            kwInput.classList.remove('cap-derived');
            ampInput.readOnly = true;
            ampInput.classList.add('cap-derived');
        } else {
            ampInput.readOnly = false;
            ampInput.classList.remove('cap-derived');
            kwInput.readOnly = true;
            kwInput.classList.add('cap-derived');
        }
    }

    for (let i = 0; i < 10; i++) {
        const ampInput = document.getElementById(`rpmCapCurrentTable${i}_input`);
        const kwInput = document.getElementById(`rpmCapKW${i}_input`);
        if (!ampInput || !kwInput) continue;
        if (mode === 'kw') {
            ampInput.value = Math.round(kwToAmps(parseFloat(kwInput.value) || 0));
        } else {
            kwInput.value = ampsToKW(parseFloat(ampInput.value) || 0).toFixed(2);
        }
    }
}

// this is known as the window.load event in AI speak, all this stuff executes on page load
window.addEventListener("load", function () {
    window.scrollTo(0, 0);  // always start at top — browser scroll restoration disabled above
    // DEBUG CODE
    const originalGetElementById = document.getElementById;
    let warnedElements = new Set();
    // IDs that are intentionally absent at times (created on demand, or only present
    // while a transient dialog is open) — these are checked-then-created/guarded, so a
    // null lookup is expected, not a stale reference. Don't flag them.
    const optionalElementIds = new Set(['recoveryDialog', 'lt-cloud-hint', 'faflip-pause-btn']);

    document.getElementById = function (id) {
        const element = originalGetElementById.call(document, id);
        if (!element && !warnedElements.has(id) && !optionalElementIds.has(id)) {
            diagError(`MISSING ELEMENT: ${id}`);
            console.trace();
            warnedElements.add(id);
        }
        return element;
    };
    // END DEBUG CODE

    // Fetch and populate vessel info on page load
    fetchAndPopulateVesselInfo();

    interceptDualControlSubmissions();

    // Mobile: every <input type="number"> gets inputmode="decimal" so iOS shows
    // the decimal keypad (with a `.` key) instead of the telephone-style numeric pad.
    // Done at runtime to avoid editing 201 individual inputs in HTML.
    document.querySelectorAll('input[type="number"]').forEach(el => {
        if (!el.hasAttribute('inputmode')) el.setAttribute('inputmode', 'decimal');
    });

    populateYearDropdown();

    // Attach vessel info form handler
    const vesselForm = document.getElementById('vessel-info-form');
    if (vesselForm) {
        vesselForm.addEventListener('submit', handleVesselInfoSave);
    }

    startStalenessDetection(); // stalness detectioin detect stale values

    initPlotDataStructures(); // Initialize efficient data structures
    //give initial values that will cause graying until proven otherwise
    window.sensorAges = {
        heading: 999999,
        latitude: 999999,
        longitude: 999999,
        satellites: 999999,
        victronVoltage: 999999,
        victronCurrent: 999999,
        alternatorTemp: 999999,
        thermistorTemp: 999999,
        rpm: 999999,
        measuredAmps: 999999,
        batteryV: 999999,
        ibv: 999999,
        bcur: 999999,
        channel3V: 999999,
        dutyCycle: 999999,
        fieldVolts: 999999,
        fieldAmps: 999999,
        cogNMEA: 999999,
        sogNMEA: 999999,
        appWindSpeed: 999999,
        appWindAngle: 999999,
        trueWindSpeed: 999999,
        trueWindAngle: 999999,
        leeway: 999999,
        vmg: 999999,
        baroPressure: 999999,
        ambientTemp: 999999,
        imu: 999999
    };

    document.getElementById("AlarmLatchEnabled_checkbox").checked = (document.getElementById("AlarmLatchEnabled").value === "1");
    document.getElementById("MaintainMode_checkbox").checked = (document.getElementById("MaintainMode").value === "1");
    document.getElementById("TargetVoltageMode_checkbox").checked = (document.getElementById("TargetVoltageMode").value === "1");
    document.getElementById("HardwarePresent_checkbox").checked = (document.getElementById("hardwarePresent").value === "1");
    document.getElementById("UseFloat_checkbox").checked = (document.getElementById("UseFloat").value === "1");
    document.getElementById("CVTuningMode_checkbox").checked = (document.getElementById("CVTuningMode").value === "1");
    document.getElementById("ThermalTuningMode_checkbox").checked = (document.getElementById("ThermalTuningMode").value === "1");


    //gray out Settings on a reload
    const settingsSection = document.getElementById('settings-section');
    if (settingsSection) {
        settingsSection.classList.add("locked");
    }
    const lockStatus = document.getElementById('lock-status');
    if (lockStatus) {
        lockStatus.textContent = "Settings are Locked";
        lockStatus.className = "lock-status-locked";
    }
    if (localStorage.getItem('darkMode') === '1') {
        document.body.classList.add('dark-mode');
        const darkToggle = document.getElementById("DarkMode_checkbox");
        if (darkToggle) darkToggle.checked = true;
    }

    // Pre-telemetry placeholder view only: Settings with its static sub-tab default
    // (Alternator > Setup). Do NOT force Vessel Info here — vesselInfoComplete is still
    // its initial false at this point (the vessel-info fetch is async and hasn't resolved),
    // and forcing vessel-info would leave it as the active settings sub-tab even after a
    // complete device lands on Live Data. The real landing (Live Data when complete, or
    // Settings > Vessel Info when not) is applied by maybeApplyLanding() once both the
    // vessel-info fetch and first telemetry packet have resolved.
    showMainTab('settings');
    // // Simple WiFi disconnect tracking - ping every 10 seconds
    // setTrackedInterval(() => {
    //     fetchWithTimeout(buildURL('/ping?t=' + Date.now()), {}, 5000).catch(() => { });
    // }, 10000);

    initCurrentTempPlot();
    initVoltagePlot();
    initRPMPlot();
    initTemperaturePlot();
    initPidTuningDataStructures();
    initPidTuningPlot();
    initCVTuningDataStructures();
    initCVTuningPlot();
    startInterpLoop();

    // Add change listener for manual CloudFeatures toggle
    const cloudFeaturesCheckbox = document.getElementById("CloudFeatures_checkbox");
    if (cloudFeaturesCheckbox) {
        cloudFeaturesCheckbox.addEventListener('change', function () {
            updateCloudFeaturesTabVisibility(this.checked);
        });
    }

    // Initialize CloudFeatures toggle state based on current mode
    const initialMode = parseInt(document.getElementById("currentModeID")?.value || "0");
    updateCloudFeaturesToggleState(initialMode);

    // Set up listener for when actual currentMode arrives from ESP32
    let modeInitialized = false;
    const modeCheck = setTrackedInterval(() => {
        if (!modeInitialized) {
            const modeElement = document.getElementById("currentModeID");
            if (modeElement && modeElement.value !== "") {
                const mode = parseInt(modeElement.value);
                updateCloudFeaturesToggleState(mode);
                modeInitialized = true;
                clearInterval(modeCheck);
            }
        }
    }, 100); // Check every 100ms until CSVData2 arrives

    // Clean up if taking too long
    setTrackedTimeout(() => clearInterval(modeCheck), 5000);

    // setupKWInputListeners stays here — needs dirtyInputs, learningTableHasChanges,
    // and showLearningTableSaveButton which are defined in this scope
    function setupKWInputListeners() {
        for (let i = 0; i < 10; i++) {
            const kwInput = document.getElementById(`rpmCapKW${i}_input`);
            if (kwInput) {
                kwInput.addEventListener('input', function () {
                    dirtyInputs.add(kwInput.id);
                    if (currentCapMode === 'kw') {
                        const ampInput = document.getElementById(`rpmCapCurrentTable${i}_input`);
                        if (ampInput) ampInput.value = Math.round(kwToAmps(parseFloat(kwInput.value) || 0));
                    }
                    learningTableHasChanges = true;
                    showLearningTableSaveButton();
                });
            }
        }
    }

    // Set up weather alert observers
    const latEl = document.getElementById('LatitudeNMEA_display');
    const lngEl = document.getElementById('LongitudeNMEA_display');
    const validEl = document.getElementById('weatherDataValidID');

    if (latEl && lngEl && validEl) {
        const weatherObserver = new MutationObserver(updateWeatherAlerts);
        weatherObserver.observe(latEl, { childList: true, characterData: true, subtree: true });
        weatherObserver.observe(lngEl, { childList: true, characterData: true, subtree: true });
        weatherObserver.observe(validEl, { childList: true, characterData: true, subtree: true });

        // Initial call
        updateWeatherAlerts();
    }

    // // Set up manual GPS form handler
    // if (gpsForm) {
    //     gpsForm.addEventListener('submit', function () {
    //         window.gpsManualOverride = true;
    //     });
    // }

    // Track if learning table has unsaved changes
    let learningTableHasChanges = false;
    let dirtyInputs = new Set();
    window.resetLearningTableUI = function () {
        dirtyInputs.clear();
        pendingTableValues.clear();
        learningTableHasChanges = false;
        document.querySelectorAll('#learning-table-form input[type="number"]').forEach(el => {
            el.classList.remove('table-input-pending');
        });
        const saveBtn = document.getElementById('learning-table-save-button');
        if (saveBtn) saveBtn.style.display = 'none';
        setTableStatus('learning-table-status', '');
    };
    let pendingTableValues = new Map(); // inputId -> {value, deadlineMs}
    let fuelTableHasChanges = false;
    let dirtyFuelInputs = new Set();
    let pendingFuelTableValues = new Map(); // inputId -> {value, deadlineMs}

    function setTableStatus(statusId, text) {
        const el = document.getElementById(statusId);
        if (!el) return;
        el.textContent = text;
        el.style.display = text ? 'inline-block' : 'none';
        el.classList.toggle('unsaved', text === 'Unsaved changes');
    }

    function showLearningTableSaveButton() {
        const saveBtn = document.getElementById('learning-table-save-button');
        if (saveBtn) saveBtn.style.display = 'inline-block';
        setTableStatus('learning-table-status', 'Unsaved changes');
    }

    function showFuelTableSaveButton() {
        const saveBtn = document.getElementById('fuel-table-save-button');
        if (saveBtn) saveBtn.style.display = 'inline-block';
        setTableStatus('fuel-table-status', 'Unsaved changes');
    }

    // Add change listeners to all learning table inputs
    document.querySelectorAll('#learning-table-form input[type="number"]').forEach(input => {
        input.addEventListener('input', function () {
            learningTableHasChanges = true;
            dirtyInputs.add(input.id);
            showLearningTableSaveButton();

            if (input.id.startsWith('rpmCapCurrentTable')) {
                const index = parseInt(input.id.match(/\d+/)[0]);
                if (currentCapMode === 'amps') {
                    const kwInput = document.getElementById(`rpmCapKW${index}_input`);
                    if (kwInput) kwInput.value = ampsToKW(parseFloat(input.value) || 0).toFixed(2);
                }
            }

            if (input.id.startsWith('rpmTableRPMPoints')) {
                const tempData = {};
                for (let i = 0; i < 10; i++) {
                    const rpmInput = document.getElementById(`rpmTableRPMPoints${i}_input`);
                    if (rpmInput) {
                        tempData[`rpmTableRPMPoints${i}`] = parseFloat(rpmInput.value) || 0;
                    }
                }
                updateRPMRangeDisplays(tempData);
            }
        });
    });


    document.querySelectorAll('#fuel-table-form input[type="number"]').forEach(input => {
        input.addEventListener('input', function () {
            fuelTableHasChanges = true;
            dirtyFuelInputs.add(input.id);
            showFuelTableSaveButton();
        });
    });

    // Hide save button after form submission
    document.getElementById('learning-table-form').addEventListener('submit', function (e) {
        if (!validateLearningTable()) {
            e.preventDefault();
            return false;
        }

        dirtyInputs.forEach(id => {
            const el = document.getElementById(id);
            if (el) {
                el.classList.add('table-input-pending');
                pendingTableValues.set(id, {
                    value: el.value,
                    deadlineMs: Date.now() + 3000
                });
            }
        });

        learningTableHasChanges = false;
        dirtyInputs.clear();

        const saveBtn = document.getElementById('learning-table-save-button');
        if (saveBtn) saveBtn.style.display = 'none';

        setTableStatus('learning-table-status', 'Saving...');
    });

    document.getElementById('fuel-table-form').addEventListener('submit', function (e) {
        if (!validateFuelTable()) {
            e.preventDefault();
            return false;
        }

        dirtyFuelInputs.forEach(id => {
            const el = document.getElementById(id);
            if (el) {
                el.classList.add('table-input-pending');
                pendingFuelTableValues.set(id, {
                    value: el.value,
                    deadlineMs: Date.now() + 3000
                });
            }
        });

        fuelTableHasChanges = false;
        dirtyFuelInputs.clear();

        setTableStatus('fuel-table-status', 'Saving...');
    });

    // Warn before leaving page with unsaved changes
    window.addEventListener('beforeunload', function (e) {
        if (learningTableHasChanges || fuelTableHasChanges) {
            e.preventDefault();
            e.returnValue = 'You have unsaved table changes. Are you sure you want to leave?';
            return e.returnValue;
        }
    });

    // Initialize EventSource connection with mobile-safe reconnection
    initializeEventSource();

    if (IS_CAPACITOR) {     // Check if we need demo mode (App Store testing without ESP32)
        checkForDemoMode();
    }

    // Stream listeners — extracted so initializeEventSource() can re-bind them on
    // every reconnect. Before this refactor, after the first SSE reconnect the
    // new EventSource had no CSV/console listeners and telemetry went silent.
    window.attachStreamListeners = function (source) {
        // Console event listener
        // Timestamp = time received by app (not time sent by regulator).
        // Messages throttled by firmware: max 5 per 700ms (adjustable).
        source.addEventListener("console", function (event) {
            if (consolePaused) return; // Don't add messages while paused

            const timestamp = new Date().toLocaleTimeString();
            const consoleDiv = document.getElementById("consoleOutput");
            if (!consoleDiv) return;

            const line = document.createElement("div");
            line.textContent = `[${timestamp}] ${event.data}`;
            consoleDiv.appendChild(line);

            // Defer scroll to avoid forced synchronous reflow on every message
            requestAnimationFrame(() => { consoleDiv.scrollTop = consoleDiv.scrollHeight; });

            while (consoleDiv.children.length > 100) {
                consoleDiv.removeChild(consoleDiv.firstChild);
            }
        });

        // Staleness detection - check if data is still flowing
        // Created ONCE per page life: attachStreamListeners re-runs on every SSE
        // reconnect attempt, and an unguarded interval here leaked one stacked
        // copy per attempt (a day of flaky boat WiFi = dozens of 2s sweeps).
        if (!window.stalenessWatchdogStarted) {
            window.stalenessWatchdogStarted = true;
            setTrackedInterval(function () {
                const timeSinceLastEvent = Date.now() - lastEventTime;
                if (timeSinceLastEvent > 9000) { // 9 seconds without data = disconnected (was working well at 8s for a long time)
                    updateInlineStatus(false);
                    markAllReadingsStale(); //gray out
                }
            }, 2000); // Check every 1 seconds (was working well at 2s for a long time)
        }


        const handleCSVData = function (e) {

            // Update timestamp when data is received
            lastEventTime = Date.now();

            // Diagnostic: track inter-event gap to distinguish firmware/WiFi delays from client-side issues
            if (window._lastCSV1Arrival) {
                const delta = lastEventTime - window._lastCSV1Arrival;
                const expected = window._lastKnownInterval || 200;
                if (delta > expected * 1.75) diagWarn(`[SSE GAP] CSVData gap: ${delta}ms (expected ~${expected}ms)`);
            }
            window._lastCSV1Arrival = lastEventTime;

            // Immediately mark as connected when data arrives
            updateInlineStatus(true);

            const raw = e.data.split(',').map(Number);
            const declaredCount = raw[0];
            const values = raw.slice(1);

            if (values.length !== declaredCount) {
                if (!window.lastCsv1WarnTime || Date.now() - window.lastCsv1WarnTime > 10000) {
                    console.warn(`[CSV1] length mismatch: declared=${declaredCount}, actual=${values.length}`, raw.slice(0, 5));
                    window.lastCsv1WarnTime = Date.now();
                }
                return;
            }
            if (declaredCount !== CSV1_FIELDS.length) {
                if (!window.lastCsv1WarnTime || Date.now() - window.lastCsv1WarnTime > 10000) {
                    console.warn(`[CSV1] schema mismatch: ESP32=${declaredCount}, UI=${CSV1_FIELDS.length}`);
                    window.lastCsv1WarnTime = Date.now();
                }
                return;
            }

            const data = Object.fromEntries(CSV1_FIELDS.map((key, i) => [key, values[i]]));

            // Thermal tuning plot: cache the two fast (CSV1) series for the live ring.
            try { thermalLiveOnCsv1(data); } catch (e) { }

            // Gate-tuning readout: IExcessK rides CSV1 (amps − setpoint); compute its 10s peak here.
            try { gateReadoutOnCsv1(data); } catch (e) { }

            // Update all "session window" labels with the current elapsed time since
            // "Reset Peak Values" was pressed (or boot). One CSV1 field drives every
            // .session-window-label span on the diagnostics page in one pass.
            if (data.perfCountersResetElapsedS !== undefined) {
                const sessionWindowText = formatSessionWindow(data.perfCountersResetElapsedS);
                document.querySelectorAll('.session-window-label').forEach(el => {
                    el.textContent = sessionWindowText;
                });
            }

            updateIMUAlignmentDisplayFromData(data);

            if (data.stateRevision !== undefined) {
                lastSeenRev = data.stateRevision;
            }

            //i'm in the CSV handler here (webgaugesinterval/plotTimeWindow now come from CSVData3)

            // Feed CV tuning plot cache — voltageTarget and Icv are now in CSV1 (fast stream)
            if (data.voltageTarget !== undefined) {
                cvPlotCache.voltageTarget = parseFloat(data.voltageTarget);
                if (_testPanelCurrentTest === 'cv') updateCVPanelDelta();
            }
            if (data.Icv !== undefined) cvPlotCache.Icv = parseFloat(data.Icv);

            // Cache mode for other functions to use
            if (data.currentMode !== undefined) {
                window._lastKnownMode = data.currentMode;
                updateCloudStatus();
            }
            window._debugData = data;


            const fieldIndicator = document.getElementById('field-status');
            const fieldWrapper = fieldIndicator ? fieldIndicator.closest('.reading-value') : null;
            const dutyCycleDisplay = document.getElementById('dutyCycleID3');
            const dutyPercentSign  = document.getElementById('dutyCyclePercentSign');

            if (fieldIndicator) {
                if (data.fieldActiveStatus === 1) {
                    fieldIndicator.textContent = 'ACTIVE';
                    if (fieldWrapper) fieldWrapper.className = 'reading-value header-field-cluster field-status-active';
                    if (dutyCycleDisplay) dutyCycleDisplay.style.display = 'inline';
                    if (dutyPercentSign)  dutyPercentSign.style.display  = 'inline';
                } else if (data.fieldActiveStatus === 2) {
                    fieldIndicator.textContent = 'RAMP DOWN';
                    if (fieldWrapper) fieldWrapper.className = 'reading-value header-field-cluster field-status-rampdown';
                    if (dutyCycleDisplay) dutyCycleDisplay.style.display = 'inline';
                    if (dutyPercentSign)  dutyPercentSign.style.display  = 'inline';
                } else if (data.fieldActiveStatus === 3) {
                    fieldIndicator.textContent = 'MANUAL';
                    if (fieldWrapper) fieldWrapper.className = 'reading-value header-field-cluster field-status-manual';
                    if (dutyCycleDisplay) dutyCycleDisplay.style.display = 'inline';
                    if (dutyPercentSign)  dutyPercentSign.style.display  = 'inline';
                } else if (data.fieldActiveStatus === 4) {
                    fieldIndicator.textContent = 'WAITING CLOUD';
                    if (fieldWrapper) fieldWrapper.className = 'reading-value header-field-cluster field-status-waiting-cloud';
                    if (dutyCycleDisplay) dutyCycleDisplay.style.display = 'none';
                    if (dutyPercentSign)  dutyPercentSign.style.display  = 'none';
                } else {
                    fieldIndicator.textContent = 'OFF';
                    if (fieldWrapper) fieldWrapper.className = 'reading-value header-field-cluster field-status-inactive';
                    if (dutyCycleDisplay) dutyCycleDisplay.style.display = 'none';
                    if (dutyPercentSign)  dutyPercentSign.style.display  = 'none';
                }
            }

            //  updateFields          
            const updateFields = (fieldArray) => {
                for (const [elementId, key] of fieldArray) {
                    const value = data[key];
                    if (value === undefined) continue;
                    // Calculate what the new text content would be
                    let newTextContent;
                    const num = Number(value);
                    if (!Number.isFinite(num)) {

                        newTextContent = "—";
                    }
                    // Values scaled by 1000 on server
                    else if (["imu_vertical_accel_g", "imu_total_accel_g"].includes(key)) {
                        newTextContent = (value / 1000).toFixed(3);
                    }
                    // Values scaled by 100 on server
                    else if (["imu_msi_score", "imu_vomit_pct", "imu_anchorage_comfort"].includes(key)) {
                        newTextContent = (value / 100).toFixed(1);
                    }
                    // Sea state minutes — display as hours (raw minutes, no server scaling)
                    else if (["imu_min_moving_gentle", "imu_min_moving_moderate", "imu_min_moving_rough", "imu_min_moving_extreme",
                              "imu_min_stat_gentle", "imu_min_stat_moderate", "imu_min_stat_rough", "imu_min_stat_extreme"].includes(key)) {
                        newTextContent = (value / 60).toFixed(1);
                    }
                    else if (key === "AlternatorTemperatureF") {
                        newTextContent = toDisplayTemp(value / 100).toFixed(1);
                    }
                    // Currents (Alt + Batt) — 3 sig figs: integer at ≥100, 1 dec at ≥10, 2 dec below.
                    else if (key === "MeasuredAmps" || key === "Bcur") {
                        const amps = value / 100;
                        const abs = Math.abs(amps);
                        if (abs >= 100)      newTextContent = amps.toFixed(0);
                        else if (abs >= 10)  newTextContent = amps.toFixed(1);
                        else                 newTextContent = amps.toFixed(2);
                    }
                    else if (["BatteryV", "uTargetAmps", "Ymin2", "Ymax2", "setpointLimited", "pidInput", "pidOutput", "pidError", "Channel3V", "IBV", "VictronVoltage", "vvout", "imu_heel_deg", "imu_pitch_deg", "imu_yaw_rate_dps", "fastOvCurrentCap", "ch1_avg_10s", "ch1_avg_2m", "ch1_avg_at", "ina_avg_10s", "ina_avg_2m", "ina_avg_at", "pf_avg_10s", "pf_avg_2m", "pf_avg_at", "vl_avg_10s", "vl_avg_2m", "vl_avg_at", "BatteryV_raw", "MeasuredAmps_filtered"].includes(key)) {
                        newTextContent = (value / 100).toFixed(2);
                    }
                    else if (key === "dutyCycle") {
                        newTextContent = (value / 100).toFixed(1);
                    }

                    // Values scaled by 100 on server
                    else if (["iiout"].includes(key)) {
                        newTextContent = (value / 100).toFixed(2);
                    }

                    // Value scaled by 1000000 on server  
                    else if (["MaximumLoopTime"].includes(key)) {
                        newTextContent = (value / 1000000).toFixed(3);
                    }

                    // Special handling for currentMode
                    else if (key === "currentMode") {
                        const modeNames = ["Configuration", "Access Point", "Client"];
                        newTextContent = modeNames[value] || `Unknown (${value})`;
                        updateCloudFeaturesToggleState(value);
                    }
                    // Special handling for currentPartitionType
                    else if (key === "currentPartitionType") {
                        const partitionNames = ["Factory Golden", "User Updateable"];
                        newTextContent = partitionNames[value] || `Unknown (${value})`;
                    }
                    // CV loop fields (voltageTarget/Icv moved from CSV2 → CSV1)
                    else if (key === "voltageTarget" || key === "voltageError") {
                        newTextContent = (value / 100).toFixed(3);
                    }
                    else if (key === "WaterDepth_ft") {
                        newTextContent = (value === 0) ? "—" : (value / 10).toFixed(1);
                    }
                    else if (key === "Icv") {
                        newTextContent = (value / 100).toFixed(2);
                    }
                    // Default: display as integer
                    else {
                        newTextContent = Math.round(value);
                    }
                    // ONLY UPDATE DOM IF VALUE ACTUALLY CHANGED
                    const cacheKey = `${elementId}_${key}`;
                    if (lastValues.get(cacheKey) !== newTextContent) {
                        lastValues.set(cacheKey, newTextContent);
                        scheduleDOMUpdateOptimized(elementId, newTextContent);
                    }
                }
            };

            // Counter for echo throttling (separate from display throttling)
            if (typeof window.echoUpdateCounter === 'undefined') window.echoUpdateCounter = 0;

            // Add counter for throttling
            if (typeof window.updateCounter === 'undefined') window.updateCounter = 0;
            window.updateCounter++;

            // Critical fields - update every cycle (real-time) - ONLY original critical fields that are in payload1
            const criticalFields = [
                ["MeasAmpsID", "MeasuredAmps"],          // Alternator Current 
                ["BatteryVID", "BatteryV"],              // ADS Battery Voltage 
                ["IBVID", "IBV"],                        // INA Battery Voltage 
                ["BCurrID", "Bcur"],                     // Battery Current 
                ["RPMID", "RPM"],                        // Engine Speed 
                ["WifiHeartBeatID", "WifiHeartBeat"],     // WifiHeartbeat
                ["WaterDepthID", "WaterDepth_ft"],
                ["FieldVoltsID", "vvout"],
                ["FieldVoltsID2", "vvout"], // have to use 2 because it appears in two HTML displays (Dumb rule)
                ["FieldAmpsID", "iiout"],
                ["FieldAmpsID2", "iiout"],// have to use 2 because it appears in two HTML displays (Dumb rule)
                ["FreeHeapID", "FreeHeap"],
                ["dutyCycleID", "dutyCycle"],
                ["dutyCycleID2", "dutyCycle"], // have to use 2 because it appears in two HTML displays (Dumb rule)
                ["dutyCycleID3", "dutyCycle"], // have to use 3 because it appears in two HTML displays (Dumb rule)
                ["pidOutput_display", "pidOutput"],
                ["pidInput_display", "pidInput"],
                ["pidError_display", "pidError"],

                // IMU live data (critical - update every cycle at 150ms)
                ["imu_heel_deg_ID", "imu_heel_deg"],
                ["imu_pitch_deg_ID", "imu_pitch_deg"],
                ["imu_vertical_accel_g_ID", "imu_vertical_accel_g"],
                ["imu_yaw_rate_dps_ID", "imu_yaw_rate_dps"],
                ["imu_total_accel_g_ID", "imu_total_accel_g"],
                // imu_msi_score, imu_vomit_pct, imu_anchorage_comfort moved to CSV2 otherFields — those fields are in CSV2
            ];

            // Update alarm status, this is GPIO21 buzzer/alarm
            updateAlarmStatus(data);

            // Other fields - update every 4th cycle - fields that are in CSV1_FIELDS
            // (fields that moved to CSV2 were relocated to CSV2 otherFields)
            const otherFields = [
                ["AltTempID", "AlternatorTemperatureF"],
                ["VictronVoltageID", "VictronVoltage"],
                ["header-voltage", "IBV"],
                ["header-alt-current", "MeasuredAmps"],
                ["header-batt-current", "Bcur"],
                ["header-alt-temp", "AlternatorTemperatureF"],
                ["header-rpm", "RPM"],
                ["currentModeID", "currentMode"],
                ["BatteryV_rawID", "BatteryV_raw"],
                ["MeasuredAmps_filtered_ID", "MeasuredAmps_filtered"],
                // CV loop (voltageTarget/Icv moved from CSV2 → CSV1 at indices 32, 33)
                ["voltageTarget_display", "voltageTarget"],
                ["Icv_display", "Icv"],
            ];

            // Throttle noisy gauge DOM writes to GAUGE_RENDER_INTERVAL_MS so the last digit
            // is readable. Plots, IMU alignment, and echoes still update every cycle below.
            const _gaugeNow = performance.now();
            if (!window._lastGaugeRender || (_gaugeNow - window._lastGaugeRender) >= GAUGE_RENDER_INTERVAL_MS) {
                updateFields(criticalFields);
                window._lastGaugeRender = _gaugeNow;
            }
            processCSVDataOptimized(data); // this is for plotting
            updateIMUAlignmentDisplayFromData(data);
            if (data.BatteryV_raw !== undefined) {
                _lastBatteryV = data.BatteryV_raw / 100;
                if (_testPanelCurrentTest === 'cv') updateCVPanelDelta();
            }

            // Update other stuff every 4th cycle
            if (window.updateCounter % 4 === 0) {
                updateFields(otherFields);
                updateAllEchosOptimized(data);
                updateTogglesFromData(data);
            }

            // One-time check to hide tabs in AP mode
            if (typeof window.tabsProcessed === 'undefined') {
                window.tabsProcessed = false;
            }

            if (!window.tabsProcessed && data.currentMode !== undefined) {
                window.tabsProcessed = true;

                // MODE_AP = 1, hide internet-dependent features
                if (data.currentMode === 1) {
                    const softwareTab = document.querySelector('.sub-tab[onclick*="softwareupdate"]');
                    if (softwareTab) {
                        softwareTab.style.display = 'none';
                    }

                    // Visible offline on purpose: config is worth setting; only the forecast needs internet.
                    const wxNote = document.getElementById('weathersolar-offline-note');
                    if (wxNote) {
                        wxNote.style.display = 'block';
                    }
                }
            }

            // Keep critical state updates on every cycle
            if (currentAdminPassword === "") {
                document.getElementById('settings-section').classList.add("locked");
            }

            // Connection status check
            const nowMs = Date.now();
            const timeSinceLastEvent = nowMs - lastEventTime;
            if (timeSinceLastEvent > 5000) {
                updateInlineStatus(false);
            } else {
                updateInlineStatus(true);
            }

            // Diagnostic: flag if this handler itself took too long (client-side bottleneck)
            const handlerDuration = Date.now() - lastEventTime;
            if (handlerDuration > 30) diagWarn(`[PERF] handleCSVData slow: ${handlerDuration}ms`);
        };
        window._csvDataHandler = handleCSVData; // Store for demo mode
        source.addEventListener('CSVData', function (e) {
            const isFirstPacket = !window._firstCsvPacketReceived;
            if (isFirstPacket) {
                window._firstCsvPacketReceived = true;
                hideWaitingForRegulator();
            }
            handleCSVData(e);
            if (isFirstPacket) {
                maybeApplyLanding();
            }
        }, false);

        source.addEventListener('CSVData2', function (e) {
            const raw = e.data.split(',').map(Number);

            const declaredCount = raw[0];
            const values = raw.slice(1);

            if (values.length !== declaredCount) {
                if (!window.lastCsv2WarnTime || Date.now() - window.lastCsv2WarnTime > 10000) {
                    console.warn(`[CSV2] length mismatch: declared=${declaredCount}, actual=${values.length}`);
                    window.lastCsv2WarnTime = Date.now();
                }
                return;
            }
            if (declaredCount !== CSV2_FIELDS.length) {
                if (!window.lastCsv2WarnTime || Date.now() - window.lastCsv2WarnTime > 10000) {
                    console.warn(`[CSV2] schema mismatch: ESP32=${declaredCount}, UI=${CSV2_FIELDS.length}`);
                    window.lastCsv2WarnTime = Date.now();
                }
                return;
            }

            const data = Object.fromEntries(CSV2_FIELDS.map((key, i) => [key, values[i]]));

            // Thermal tuning plot: sample the live ring off this CSV2 frame (~5s).
            try { thermalLiveOnCsv2(data); } catch (e) { }

            // Gate-tuning readouts: render the firmware 10s extremes next to each threshold.
            try { gateReadoutOnCsv2(data); } catch (e) { }

            // IMU zero/level calibration status (offsets sent deg ×100)
            try {
                const hOff = Number(data.imuHeelOffset) / 100;
                const pOff = Number(data.imuPitchOffset) / 100;
                const el = document.getElementById("imuZeroStatus_ID");
                if (el) {
                    el.textContent = (Number.isFinite(hOff) && Number.isFinite(pOff) && (hOff !== 0 || pOff !== 0))
                        ? `heel ${hOff.toFixed(1)}° / pitch ${pOff.toFixed(1)}°`
                        : "not set";
                }
            } catch (e) { }

            // Beaufort scale + gale duration: derived from the 2-min SUSTAINED true wind (data.sustainedTWS,
            // sent ×10) — never the instantaneous gust — so a gust isn't a gale and a lull doesn't end one.
            try {
                const twsKt = Number(data.sustainedTWS) / 10;
                const bEl = document.getElementById('BeaufortID');
                if (bEl) {
                    if (Number.isFinite(twsKt) && twsKt > 0) {
                        let n, nm;
                        if (twsKt < 1) { n = 0; nm = 'Calm'; }
                        else if (twsKt <= 3) { n = 1; nm = 'Light air'; }
                        else if (twsKt <= 6) { n = 2; nm = 'Light breeze'; }
                        else if (twsKt <= 10) { n = 3; nm = 'Gentle breeze'; }
                        else if (twsKt <= 16) { n = 4; nm = 'Moderate breeze'; }
                        else if (twsKt <= 21) { n = 5; nm = 'Fresh breeze'; }
                        else if (twsKt <= 27) { n = 6; nm = 'Strong breeze'; }
                        else if (twsKt <= 33) { n = 7; nm = 'Near gale'; }
                        else if (twsKt <= 40) { n = 8; nm = 'Gale'; }
                        else if (twsKt <= 47) { n = 9; nm = 'Strong gale'; }
                        else if (twsKt <= 55) { n = 10; nm = 'Storm'; }
                        else if (twsKt <= 63) { n = 11; nm = 'Violent storm'; }
                        else { n = 12; nm = 'Hurricane'; }
                        bEl.textContent = n + ' (' + nm + ')';
                    } else {
                        bEl.textContent = '—';
                    }
                }
                const gEl = document.getElementById('GaleDurationID');
                if (gEl) {
                    const gm = Math.round(Number(data.currentGaleMinutes));
                    if (!Number.isFinite(gm) || gm <= 0) gEl.textContent = '0';
                    else if (gm < 60) gEl.textContent = gm + ' min';
                    else gEl.textContent = Math.floor(gm / 60) + 'h ' + (gm % 60 < 10 ? '0' : '') + (gm % 60) + 'm';
                }
            } catch (e) { /* derived wind display is best-effort */ }

            // voltageTarget and Icv moved to CSV1 (fast stream) — cvPlotCache is updated in handleCSVData

            if (data.stateRevision !== undefined) {
                lastSeenRev = data.stateRevision;
            }

            handleForcedUpdate(data);

            // Scheduled-restart warning: banner + one-shot popup per page-load cycle
            try {
                const restSec = Number(data.restartRemainingSec);
                if (Number.isFinite(restSec) && restSec > 0) {
                    const banner = document.getElementById('restart-countdown-banner');
                    const txt = document.getElementById('restart-countdown-text');
                    if (banner && txt) {
                        const m = Math.floor(restSec / 60);
                        const s = restSec % 60;
                        txt.textContent = 'Scheduled restart in ' + m + ':' + (s < 10 ? '0' : '') + s + ' — device will reboot cleanly';
                        banner.style.display = 'block';
                    }
                    if (!window.restartPopupShownThisCycle) {
                        const overlay = document.getElementById('restart-popup-overlay');
                        if (overlay) overlay.style.display = 'flex';
                        window.restartPopupShownThisCycle = true;
                    }
                } else {
                    const banner = document.getElementById('restart-countdown-banner');
                    if (banner) banner.style.display = 'none';
                    window.restartPopupShownThisCycle = false;
                }
            } catch (err) { /* never let banner logic break CSV2 dispatch */ }

            // Live GPS / time source indicator for the System Settings panel.
            // Reads two CSV2 ints just published by resolveSources() in firmware.
            try {
                const ind = document.getElementById('liveSourceIndicator');
                if (data.currentGpsSource !== undefined) {
                    // Stash for the Weather Mode source label (lives on the CSV1
                    // dispatcher, which doesn't see currentGpsSource directly).
                    window.currentGpsSource = Number(data.currentGpsSource);
                    const gpsBadge = document.getElementById('gps-source-badge');
                    if (gpsBadge) {
                        gpsBadge.textContent = ({ 0: 'no GPS', 1: 'NMEA 2000', 2: 'iOS app', 3: 'Manual' })[Number(data.currentGpsSource)] ?? '—';
                    }
                }
                if (ind && (data.currentGpsSource !== undefined || data.currentTimeSource !== undefined)) {
                    const gpsLbl  = ({0:'no GPS', 1:'NMEA GPS', 2:'Phone GPS', 3:'Manual'})[Number(data.currentGpsSource)] ?? '?';
                    const timeLbl = ({0:'no time', 1:'GPS time', 2:'phone time', 3:'NTP time', 4:'drifting'})[Number(data.currentTimeSource)] ?? '?';
                    ind.textContent = gpsLbl + ' · ' + timeLbl;
                }
            } catch (err) { /* never let indicator break CSV2 dispatch */ }

            // Logging status pill + Stop/Start toggle (CSV2 slot 439)
            try {
                if (data.loggingActive !== undefined) {
                    updateLoggingStatus(data.loggingActive);
                }
            } catch (err) { /* never let pill logic break CSV2 dispatch */ }

            // Update GPS moving state for IMU mode graying (SOGNMEA is scaled ×100)
            if (data.SOGNMEA !== undefined) {
                updateIMUMovingState(data.SOGNMEA);
                updateIMUModeStyles();
            }

            updateAnchorColorCoding(data);

            //  updateFields for CSVData2
            const updateFields = (fieldArray) => {
                for (const [elementId, key] of fieldArray) {
                    const value = data[key];
                    if (value === undefined) continue;
                    // Calculate what the new text content would be
                    let newTextContent;
                    const num = Number(value);
                    if (!Number.isFinite(num)) {
                        newTextContent = "—";
                    }
                    // Special handling for reset reason lookups
                    else if (key === "LastResetReason" || key === "ancientResetReason") {
                        newTextContent = resetReasonLookup[value] || `Unknown (${value})`;
                    }
                    // Special handling for imuEnabled
                    else if (key === "imuEnabled") {
                        newTextContent = value === 1 ? "Enabled" : "Disabled";
                    }
                    // Special handling for imuMountOrientation
                    else if (key === "imuMountOrientation") {
                        const orientations = ["Fwd Bulkhead", "Aft Bulkhead", "Port Wall", "Stbd Wall"];
                        newTextContent = orientations[value] || `Unknown (${value})`;
                    }

                    // Special handling for wave period (-1 = invalid)
                    else if (key === "imu_wave_period_sec") {
                        if (value === -1000) {  // Remember it's scaled by 1000
                            newTextContent = "--";
                        } else {
                            newTextContent = (value / 1000).toFixed(2);
                        }
                    }
                    // Heading swing: sentinel -10 means no compass data
                    else if (key === "imu_heading_swing_120s") {
                        if (value < 0) {
                            newTextContent = "--";
                        } else {
                            newTextContent = (value / 10).toFixed(1);
                        }
                    }
                    // Speed Through Water (knots ×100): -1 sentinel = NAN / no paddlewheel log
                    else if (key === "STWNMEA") {
                        newTextContent = value < 0 ? "—" : (value / 100).toFixed(2);
                    }
                    // IMU FIFO drain durations: firmware sends µs; display in ms to match the rest of this page
                    else if (key === "IMUReadTime" || key === "IMUReadTime2") {
                        newTextContent = (value / 1000).toFixed(2);
                    }
                    // Bus-only worst read timers: firmware sends µs; display in ms to match the rest of this page
                    else if (key === "inaBusReadWorstUs" || key === "imuFifoFetchWorstUs") {
                        newTextContent = (value / 1000).toFixed(2);
                    }
                    // Fast alt-current diagnostics status strip (item 10)
                    else if (key === "faChanState") {
                        newTextContent = value === 1 ? "Sampling" : (value === 2 ? "Dormant (no signal)" : "Off");
                    }
                    else if (key === "faDetectK") {
                        newTextContent = value === 0 ? "None" : ("k=" + value);
                    }
                    else if (key === "faSesPkpkWorst" || key === "faSesPeakWorst") {
                        newTextContent = (value / 100).toFixed(2);  // A ×100
                    }
                    else if (key === "faSesPeakWorstHz" || key === "faDomFreqHz") {
                        newTextContent = (value / 10).toFixed(1);   // Hz ×10
                    }
                    else if (key === "faDomAmp") {
                        newTextContent = (value / 100).toFixed(2);  // A ×100
                    }
                    // Time values that need conversion from minutes to days/hours/minutes
                    else if (["timeToFullChargeMin", "timeToFullDischargeMin"].includes(key)) {
                        newTextContent = formatMinutesToDHM(value);
                    }
                    // IMU raw accel/gyro values scaled by 1000 or 100
                    else if (["imu_accel_x_raw", "imu_accel_y_raw", "imu_accel_z_raw",
                        "accel_x_min", "accel_x_max", "accel_x_avg",
                        "accel_y_min", "accel_y_max", "accel_y_avg",
                        "accel_z_min", "accel_z_max", "accel_z_avg",
                        "vertical_accel_min", "vertical_accel_max", "vertical_accel_avg",
                        "total_accel_min", "total_accel_max", "total_accel_avg",
                        "imu_slam_peak_max", "imu_slam_peak_lifetime"].includes(key)) {
                        newTextContent = (value / 1000).toFixed(3);
                    }
                    else if (["imu_gyro_x_raw", "imu_gyro_y_raw", "imu_gyro_z_raw",
                        "gyro_x_min", "gyro_x_max", "gyro_x_avg",
                        "gyro_y_min", "gyro_y_max", "gyro_y_avg",
                        "gyro_z_min", "gyro_z_max", "gyro_z_avg",
                        "heel_min", "heel_max", "heel_avg",
                        "pitch_min", "pitch_max", "pitch_avg",
                        "imu_heel_change_60s", "imu_heel_deviation_60s",
                        "imu_pitch_change_60s", "imu_pitch_deviation_60s",
                        "imu_heel_max_lifetime", "imu_pitch_max_lifetime",
                        "imu_heel_deviation_120s", "imu_pitch_deviation_120s"].includes(key)) {
                        newTextContent = (value / 100).toFixed(2);
                    }
                    // Thermistor -99 sentinel = no sensor connected (must precede the temperature
                    // branch below or toDisplayTemp turns it into -73 in °C mode)
                    else if (key === "temperatureThermistor" && num === -99) {
                        newTextContent = "—";
                    }
                    // Barometric pressure mbar ×10 — whole-mbar rounding made it look frozen
                    else if (key === "baroPressure") {
                        newTextContent = (value / 10).toFixed(1);
                    }
                    // Temperature fields (raw integer °F from firmware — convert to display unit)
                    else if (["MaxAlternatorTemperatureF", "temperatureThermistor", "MaxTemperatureThermistor",
                        "ambientTemp", "MaxAlternatorTemperatureF_AllTime", "MaxTemperatureThermistor_AllTime"].includes(key)) {
                        newTextContent = Math.round(toDisplayTemp(value));
                    }
                    // Temperature PID input/setpoint scaled ×100 — convert to display unit
                    else if (key === "tempPIDInput_d" || key === "tempPIDSetpoint_d") {
                        newTextContent = toDisplayTemp(value / 100).toFixed(1);
                    }
                    // Values scaled by 100 on server (existing)
                    else if (["IBVMax", "ChargeCycles", "ChargeCycles_AllTime", "thermalPenaltyAmps", "MeasuredAmpsMax", "SOC_percent", "VictronCurrent", "performanceRatio", "UVThresholdHigh",
                        "PeakVoltage_AllTime", "MinVoltage", "MinVoltage_AllTime", "AvgSOC_AllTime", "AvgSpeed_AllTime", "InsulationLifePercent", "GreaseLifePercent",
                        "BrushLifePercent", "pKwHrToday", "pKwHrTomorrow", "pKwHr2days", "AvgSpeed", "MeasuredAmpsMax_AllTime", "SOGNMEA", "ApparentWindSpeedNMEA", "TrueWindSpeedNMEA", "VMGNMEA", "VMGUpwind",
                        "fastOvCurrentCap", "ch1_avg_10s", "ch1_avg_2m", "ch1_avg_at", "ina_avg_10s", "ina_avg_2m", "ina_avg_at",
                        "pf_avg_10s", "pf_avg_2m", "pf_avg_at",
                        "vl_avg_10s", "vl_avg_2m", "vl_avg_at",
                        "VictronSolarVoltage_V", "VictronSolarCurrent_A", "VictronYieldToday_kWh", "VictronYieldYesterday_kWh"].includes(key)) {
                        // NOTE: ch1_worst_* and ina_worst_* are NOT in this list — firmware sends them as raw integer ms
                        // (only the matching *_avg_* values are scaled ×100). They fall through to the default integer render below.
                        newTextContent = (value / 100).toFixed(2);
                    }
                    // GPS coordinates scaled by 1,000,000 on server
                    else if (["LatitudeNMEA", "LongitudeNMEA"].includes(key)) {
                        newTextContent = (value / 1000000).toFixed(6);
                    }
                    // Value scaled by 1000000 on server  
                    else if (["LastSessionMaxLoopTime", "MaximumLoopTime"].includes(key)) {
                        newTextContent = (value / 1000000).toFixed(3);
                    }
                    // absorptionCompleteTime: stored/sent as ms -> display seconds
                    else if (["bulkVoltageHoldMs"].includes(key)) {
                        newTextContent = (value / 1000).toFixed(2);  // ms -> seconds
                    }
                    // absorptionCompleteTime: stored/sent as ms -> display seconds
                    else if (["absorptionCompleteTime"].includes(key)) {
                        newTextContent = (value / 1000).toFixed(2);  // ms -> seconds
                    }

                    else if (["RebulkCurrent_A"].includes(key)) {
                        newTextContent = (value / 100).toFixed(2);
                    }
                    else if (["AbsorptionTimeoutMs"].includes(key)) {
                        newTextContent = (value / 60000).toFixed(2);  // ms -> minutes
                    }
                    else if (["AbsorptionVoltage"].includes(key)) {
                        newTextContent = (value / 100).toFixed(2);    // *100 int -> volts
                    }
                    else if (["TargetVoltageSetpoint"].includes(key)) {
                        newTextContent = (value / 100).toFixed(2);    // *100 int -> volts
                    }
                    else if (["voltageTarget", "voltageError"].includes(key)) {
                        newTextContent = (value / 100).toFixed(3);    // *100 int -> volts, 3dp for error
                    }
                    else if (["Icv", "cv_I"].includes(key)) {
                        newTextContent = (value / 100).toFixed(2);    // *100 int -> amps
                    }
                    else if (key === "voltageControlActive" || key === "loadDumpActive") {
                        newTextContent = value === 1 ? "YES" : "NO";
                    }
                    else if (key === "currentPartitionType") {
                        const partitionNames = ["Factory Golden", "User Updateable"];
                        newTextContent = partitionNames[value] || `Unknown (${value})`;
                    }
                    else if (key === "dBcur_dt") {
                        newTextContent = (value / 10).toFixed(1);  // ×10 -> A/s, 1dp
                    }
                    // FLOAT_DURATION: stored/sent as seconds -> display hours
                    else if (["FLOAT_DURATION"].includes(key)) {
                        newTextContent = (value / 3600).toFixed(2);
                    }
                    // Values scaled by 1000 on server (existing)
                    else if (["DynamicShuntGainFactor", "DynamicAltCurrentZero", "ChargedEnergy", "DischargedEnergy", "AlternatorChargedEnergy", "ChargedEnergy_AllTime",
                        "DischargedEnergy_AllTime", "AlternatorChargedEnergy_AllTime", "SolarChargedEnergy",
                        "SolarChargedEnergy_AllTime"].includes(key)) {
                        newTextContent = (value / 1000).toFixed(3);
                    }
                    // Fuel display handling
                    else if (key === "EngineFuelUsed" || key === "EngineFuelUsed_AllTime") {
                        newTextContent = (value / 100).toFixed(2);
                    }
                    else if (key.startsWith("ft_") || key === "VeTime2") {
                        newTextContent = (value / 1000).toFixed(1);
                    }
                    else if (key === "AlternatorFuelUsed" || key === "AlternatorFuelUsed_AllTime") {
                        newTextContent = (value / 100).toFixed(2);
                    }
                    else if (["EngineRunTime", "AlternatorOnTime", "EngineRunTime_AllTime", "AlternatorOnTime_AllTime"].includes(key)) {
                        const totalSeconds = (value / 100) * 3600;
                        const hours = Math.floor(totalSeconds / 3600);
                        const minutes = Math.floor((totalSeconds % 3600) / 60);
                        const seconds = Math.floor(totalSeconds % 60);
                        newTextContent = `${hours.toString().padStart(2, '0')}:${minutes.toString().padStart(2, '0')}:${seconds.toString().padStart(2, '0')}`;
                    }
                    else if (["innerTermP", "innerTermI", "innerTermD", "outerTermP", "outerTermI", "outerTermLookahead"].includes(key)) {
                        newTextContent = (value / 100).toFixed(2);
                    }
                    else if (key === "thermalSlopeFPerSec") {
                        newTextContent = toDisplayTempDelta(value / 1000).toFixed(3);
                    }
                    // Session duration in seconds (firmware sends raw seconds; formatter auto-scales)
                    else if (key === "LastSessionDuration" || key === "CurrentSessionDuration") {
                        newTextContent = formatSecondsAuto(value);
                    }
                    // Distance values scaled by 10 on server
                    else if (["TotalDistance", "TotalDistance_AllTime"].includes(key)) {
                        newTextContent = (value / 10).toFixed(1);
                    }
                    // Speed values scaled by 100 on server
                    else if (["MaxSpeed", "UVToday", "UVTomorrow", "UVDay2", "MaxSpeed_AllTime"].includes(key)) {
                        newTextContent = (value / 100).toFixed(2);
                    }
                    else if (["deviceIdUpper", "deviceIdLower"].includes(key)) {
                        newTextContent = value;
                    }
                    // timeSinceLastOverheat: pre-divided by 1000 in C++ (seconds) — convert to hours for display
                    else if (key === "timeSinceLastOverheat") {
                        newTextContent = (value / 3600).toFixed(2);
                    }
                    // overheatingPenaltyTimer: pre-divided by 1000 in C++ — display raw seconds
                    else if (key === "overheatingPenaltyTimer") {
                        newTextContent = (value).toFixed(0);
                    }
                    // Values scaled by 100 on server (pid and learning fields)
                    else if (["pidSetpoint", "FieldResistance", "averageTableValue", "TailCurrent_A", "RebulkVoltage", "SOC_BlockRebulk_percent", "SOC_AllowRebulk_percent", "DutySlowRampRate", "VoltageKi", "VoltageKp"].includes(key)) {
                        newTextContent = (value / 100).toFixed(2);
                    }
                    // safeHours: stored as seconds — display as hours
                    else if (key.startsWith("safeHours")) {
                        newTextContent = (value / 3600).toFixed(2);
                    }
                    // Ignition-cycle watermarks sent ×10 (1 decimal): IBV, vertical accel, and both VMG sidebars
                    else if (key === "wmIgn_IBV_lo" || key === "wmIgn_IBV_hi"
                          || key === "wmIgn_vacc_lo" || key === "wmIgn_vacc_hi"
                          || key === "wmIgn_VMGman_lo" || key === "wmIgn_VMGman_hi"
                          || key === "wmIgn_VMGup_lo" || key === "wmIgn_VMGup_hi") {
                        newTextContent = (value / 10).toFixed(1);
                    }
                    // Default: display as integer
                    else {
                        newTextContent = Math.round(value);
                    }

                    // Banner SOC is tight against the % sign — show 1 decimal instead of 2 to avoid overlap.
                    if (elementId === "header-soc" && key === "SOC_percent") {
                        newTextContent = (value / 100).toFixed(1);
                    }

                    // ONLY UPDATE DOM IF VALUE ACTUALLY CHANGED
                    const cacheKey = `${elementId}_${key}`;
                    if (lastValues.get(cacheKey) !== newTextContent) {
                        lastValues.set(cacheKey, newTextContent);
                        scheduleDOMUpdateOptimized(elementId, newTextContent);
                    }

                    if (data.deviceIdUpper !== undefined && data.deviceIdLower !== undefined) {
                        updateDeviceId(data.deviceIdUpper, data.deviceIdLower);
                    }
                }
            };

            //stuff for wifi wake countdown, charging mode, and ignition indicator:
            // Update ignition status
            const ignitionStatus = document.getElementById('ignition-status');
            const ignitionValue = document.getElementById('IgnitionID');
            if (ignitionStatus && ignitionValue) {
                const ign = parseInt(ignitionValue.textContent);
                if (ign === 1) {
                    ignitionStatus.textContent = 'ON';
                    ignitionStatus.className = 'duo-num ignition-on';
                } else {
                    ignitionStatus.textContent = 'OFF';
                    ignitionStatus.className = 'duo-num ignition-off';
                }
            }
            // Update charging mode
            const chargingMode = document.getElementById('charging-mode');
            const bulkStageValue = document.getElementById('BulkStageID');
            if (chargingMode && bulkStageValue) {
                const bulkStage = parseInt(bulkStageValue.textContent);
                chargingMode.textContent = bulkStage === 1 ? 'BULK' : 'FLOAT';
            }
            // Update WiFi countdown banner — shown during WiFi wake mode OR shutdown drain window
            const wifiWakeStatus = document.getElementById('wifi-wake-status');
            const wifiWakeSeconds = document.getElementById('wifi-wake-seconds');
            const wifiWakeValue = document.getElementById('WifiWakeSecondsRemainingID');
            if (wifiWakeStatus && wifiWakeSeconds && wifiWakeValue) {
                const seconds = parseInt(wifiWakeValue.textContent);
                if (seconds > 0) {
                    const m = Math.floor(seconds / 60);
                    const s = seconds % 60;
                    wifiWakeSeconds.textContent = m + ':' + String(s).padStart(2, '0');
                    wifiWakeStatus.style.display = 'block';
                } else {
                    wifiWakeStatus.style.display = 'none';
                }
            }

            // Other fields for CSVData2 - update every cycle
            const otherFields = [
                ["IBVMaxID", "IBVMax"],
                ["MeasuredAmpsMaxID", "MeasuredAmpsMax"],
                ["RPMMaxID", "RPMMax"],
                ["EngineRunTimeID", "EngineRunTime"],
                ["AlternatorOnTimeID", "AlternatorOnTime"],
                ["AlternatorFuelUsedID", "AlternatorFuelUsed"],
                ["ChargedEnergyID", "ChargedEnergy"],
                ["DischargedEnergyID", "DischargedEnergy"],
                ["AlternatorChargedEnergyID", "AlternatorChargedEnergy"],
                ["MaxAlternatorTemperatureF_ID", "MaxAlternatorTemperatureF"],
                ["temperatureThermistorID", "temperatureThermistor"],
                ["MaxTemperatureThermistorID", "MaxTemperatureThermistor"],
                ["VictronCurrentID", "VictronCurrent"],
                ["LatitudeNMEA_ID", "LatitudeNMEA"],
                ["LongitudeNMEA_ID", "LongitudeNMEA"],
                ["SatelliteCountNMEA_ID", "SatelliteCountNMEA"],
                ["timeToFullChargeMinID", "timeToFullChargeMin"],
                ["timeToFullDischargeMinID", "timeToFullDischargeMin"],
                ["LastSessionDurationID", "LastSessionDuration"],
                ["LastSessionMaxLoopTimeID", "LastSessionMaxLoopTime"],
                ["lastSessionMinHeapID", "lastSessionMinHeap"],
                ["wifiReconnectsTotalID", "wifiReconnectsTotal"],
                ["LastResetReasonID", "LastResetReason"],
                ["ancientResetReasonID", "ancientResetReason"],
                ["totalPowerCyclesID", "totalPowerCycles"],
                ["MinFreeHeapID", "MinFreeHeap"],
                ["UVTodayID", "UVToday"],
                ["UVTomorrowID", "UVTomorrow"],
                ["UVDay2ID", "UVDay2"],
                ["weatherDataValidID", "weatherDataValid"],
                ["DynamicShuntGainFactor_display", "DynamicShuntGainFactor"],
                ["DynamicAltCurrentZero_display", "DynamicAltCurrentZero"],
                ["InsulationLifePercentID", "InsulationLifePercent"],
                ["GreaseLifePercentID", "GreaseLifePercent"],
                ["BrushLifePercentID", "BrushLifePercent"],
                ["PredictedLifeHoursID", "PredictedLifeHours"],
                ["pKwHrToday_ID", "pKwHrToday"],
                ["pKwHrTomorrow_ID", "pKwHrTomorrow"],
                ["pKwHr2days_ID", "pKwHr2days"],
                ["SOC_percentID", "SOC_percent"],
                ["header-soc", "SOC_percent"],
                ["ambientTempID", "ambientTemp"],
                ["baroPressureID", "baroPressure"],
                ["firmwareVersionIntID", "firmwareVersionInt"],
                ["deviceIdUpperID", "deviceIdUpper"],
                ["deviceIdLowerID", "deviceIdLower"],
                ["ChargedEnergy_AllTimeID", "ChargedEnergy_AllTime"],
                ["AlternatorFuelUsed_AllTimeID", "AlternatorFuelUsed_AllTime"],
                ["PeakVoltage_AllTimeID", "PeakVoltage_AllTime"],
                ["EngineRunTime_AllTimeID", "EngineRunTime_AllTime"],
                ["MinVoltageID", "MinVoltage"],
                ["MinVoltage_AllTimeID", "MinVoltage_AllTime"],
                ["ChargeCyclesID", "ChargeCycles"],
                ["ChargeCycles_AllTimeID", "ChargeCycles_AllTime"],
                ["EngineFuelUsedID", "EngineFuelUsed"],
                ["EngineFuelUsed_AllTimeID", "EngineFuelUsed_AllTime"],
                ["TotalDistanceID", "TotalDistance"],
                ["TotalDistance_AllTimeID", "TotalDistance_AllTime"],
                ["MaxSpeedID", "MaxSpeed"],
                ["MaxSpeed_AllTimeID", "MaxSpeed_AllTime"],
                ["SolarChargedEnergyID", "SolarChargedEnergy"],
                ["SolarChargedEnergy_AllTimeID", "SolarChargedEnergy_AllTime"],
                ["AlternatorChargedEnergy_AllTimeID", "AlternatorChargedEnergy_AllTime"],
                ["DischargedEnergy_AllTimeID", "DischargedEnergy_AllTime"],
                ["AvgSOC_AllTimeID", "AvgSOC_AllTime"],
                ["AvgSpeed_AllTimeID", "AvgSpeed_AllTime"],
                ["AvgSpeedID", "AvgSpeed"],
                ["AlternatorOnTime_AllTimeID", "AlternatorOnTime_AllTime"],
                ["EngineCycles_AllTimeID", "EngineCycles_AllTime"],
                ["MaxAlternatorTemperatureF_AllTimeID", "MaxAlternatorTemperatureF_AllTime"],
                ["MaxTemperatureThermistor_AllTimeID", "MaxTemperatureThermistor_AllTime"],
                ["MeasuredAmpsMax_AllTimeID", "MeasuredAmpsMax_AllTime"],
                ["RPMMax_AllTimeID", "RPMMax_AllTime"],
                ["IgnitionID", "Ignition"],
                ["BulkStageID", "BulkStage"],
                ["WifiWakeSecondsRemainingID", "WifiWakeSecondsRemaining"],
                ["BufferedRecordCountID", "BufferedRecordCount"],
                ["BufferedRecordPercentID", "BufferedRecordPercent"],
                ["BufferedRecordCapID", "BufferedRecordCap"],
                ["COGNMEA_ID", "COGNMEA"],
                ["SOGNMEA_ID", "SOGNMEA"],
                ["STWNMEA_ID", "STWNMEA"],
                ["ApparentWindSpeedNMEA_ID", "ApparentWindSpeedNMEA"],
                ["ApparentWindAngleNMEA_ID", "ApparentWindAngleNMEA"],
                ["TrueWindSpeedNMEA_ID", "TrueWindSpeedNMEA"],
                ["TrueWindAngleNMEA_ID", "TrueWindAngleNMEA"],
                ["LeewayNMEA_ID", "LeewayNMEA"],
                ["VMGNMEA_ID", "VMGNMEA"],
                ["VMGUpwind_ID", "VMGUpwind"],
                ["VMGTargetBearing_echo", "VMGTargetBearing"],
                ["cpuLoadCore0_display", "cpuLoadCore0"],
                ["cpuLoadCore0Max_display", "cpuLoadCore0Max"],
                ["cpuLoadCore1_display", "cpuLoadCore1"],
                ["cpuLoadCore1Max_display", "cpuLoadCore1Max"],

                // IMU Raw Signals
                ["imu_accel_x_raw_ID", "imu_accel_x_raw"],
                ["imu_accel_y_raw_ID", "imu_accel_y_raw"],
                ["imu_accel_z_raw_ID", "imu_accel_z_raw"],
                ["imu_gyro_x_raw_ID", "imu_gyro_x_raw"],
                ["imu_gyro_y_raw_ID", "imu_gyro_y_raw"],
                ["imu_gyro_z_raw_ID", "imu_gyro_z_raw"],

                // IMU Window Stats - Accel
                ["accel_x_min_ID", "accel_x_min"],
                ["accel_x_max_ID", "accel_x_max"],
                ["accel_x_avg_ID", "accel_x_avg"],
                ["accel_y_min_ID", "accel_y_min"],
                ["accel_y_max_ID", "accel_y_max"],
                ["accel_y_avg_ID", "accel_y_avg"],
                ["accel_z_min_ID", "accel_z_min"],
                ["accel_z_max_ID", "accel_z_max"],
                ["accel_z_avg_ID", "accel_z_avg"],

                // IMU Window Stats - Gyro
                ["gyro_x_min_ID", "gyro_x_min"],
                ["gyro_x_max_ID", "gyro_x_max"],
                ["gyro_x_avg_ID", "gyro_x_avg"],
                ["gyro_y_min_ID", "gyro_y_min"],
                ["gyro_y_max_ID", "gyro_y_max"],
                ["gyro_y_avg_ID", "gyro_y_avg"],
                ["gyro_z_min_ID", "gyro_z_min"],
                ["gyro_z_max_ID", "gyro_z_max"],
                ["gyro_z_avg_ID", "gyro_z_avg"],

                // IMU Window Stats - Calculated Metrics
                ["heel_min_ID", "heel_min"],
                ["heel_max_ID", "heel_max"],
                ["heel_avg_ID", "heel_avg"],
                ["pitch_min_ID", "pitch_min"],
                ["pitch_max_ID", "pitch_max"],
                ["pitch_avg_ID", "pitch_avg"],
                ["vertical_accel_min_ID", "vertical_accel_min"],
                ["vertical_accel_max_ID", "vertical_accel_max"],
                ["vertical_accel_avg_ID", "vertical_accel_avg"],
                ["total_accel_min_ID", "total_accel_min"],
                ["total_accel_max_ID", "total_accel_max"],
                ["total_accel_avg_ID", "total_accel_avg"],

                // IMU Event Counters
                ["imu_slam_count_ID", "imu_slam_count"],
                ["imu_slam_peak_max_ID", "imu_slam_peak_max"],
                ["imu_slam_count_lifetime_ID", "imu_slam_count_lifetime"],
                ["imu_capsize_count_ID", "imu_capsize_count"],
                ["imu_pitchpole_count_ID", "imu_pitchpole_count"],

                // IMU 60s Rolling Metrics
                ["imu_heel_change_60s_ID", "imu_heel_change_60s"],
                ["imu_heel_deviation_60s_ID", "imu_heel_deviation_60s"],
                ["imu_pitch_change_60s_ID", "imu_pitch_change_60s"],
                ["imu_pitch_deviation_60s_ID", "imu_pitch_deviation_60s"],

                // IMU Wave Period
                ["imu_wave_period_sec_ID", "imu_wave_period_sec"],

                // IMU 120s Anchor Metrics (CSV2 fields 270-272)
                ["imu_heel_deviation_120s_ID", "imu_heel_deviation_120s"],
                ["imu_pitch_deviation_120s_ID", "imu_pitch_deviation_120s"],
                ["imu_heading_swing_120s_ID", "imu_heading_swing_120s"],

                // IMU Lifetime Maximums
                ["imu_heel_max_lifetime_ID", "imu_heel_max_lifetime"],
                ["imu_pitch_max_lifetime_ID", "imu_pitch_max_lifetime"],
                ["imu_slam_peak_lifetime_ID", "imu_slam_peak_lifetime"],

                // Sea state lifetime hours (stored as minutes, displayed as hours)
                ["imu_min_moving_gentle_ID",   "imu_min_moving_gentle"],
                ["imu_min_moving_moderate_ID", "imu_min_moving_moderate"],
                ["imu_min_moving_rough_ID",    "imu_min_moving_rough"],
                ["imu_min_moving_extreme_ID",  "imu_min_moving_extreme"],
                ["imu_min_stat_gentle_ID",     "imu_min_stat_gentle"],
                ["imu_min_stat_moderate_ID",   "imu_min_stat_moderate"],
                ["imu_min_stat_rough_ID",      "imu_min_stat_rough"],
                ["imu_min_stat_extreme_ID",    "imu_min_stat_extreme"],

                // IMU Diagnostics (imuEnabled/imuMountOrientation moved to CSV3 handler — those fields are in CSV3)
                ["imu_fifo_overrun_count_ID", "imu_fifo_overrun_count"],
                // 80MHz low-power loop health — paired with the FIFO overrun count above
                ["loopWorst80Win_ID", "loopWorst80Win_ms"],
                ["loopWorst80Ses_ID", "loopWorst80Ses_ms"],
                ["loopOver80ImuLimit_ID", "loopOver80ImuLimitCount"],
                ["loop80IterCount_ID", "loop80IterCount"],
                ["imu_i2c_error_count_ID", "imu_i2c_error_count"],
                ["imu_unknown_tag_count_ID", "imu_unknown_tag_count"],
                ["imu_accel_dropped_ID", "imu_accel_dropped"],
                ["imu_gyro_dropped_ID", "imu_gyro_dropped"],
                ["imu_total_samples_accel_ID", "imu_total_samples_accel"],
                ["imu_total_samples_gyro_ID", "imu_total_samples_gyro"],
                ["IMUReadTime2_ID", "IMUReadTime2"],
                ["IMUReadTime_ID", "IMUReadTime"],
                ["adsI2CErrorCount_ID", "adsI2CErrorCount"],
                ["inaBusReadWorstUs_ID", "inaBusReadWorstUs"],
                ["inaBusSlowCount_ID", "inaBusSlowCount"],
                ["ina228ErrorCount_ID", "ina228ErrorCount"],
                ["imuFifoFetchWorstUs_ID", "imuFifoFetchWorstUs"],
                ["imuFifoWorstSamples_ID", "imuFifoWorstSamples"],

                //Temperature PID loop
                ["tempPIDActive_display", "tempPIDActive"],
                ["tempPIDInput_display", "tempPIDInput_d"],
                ["tempPIDSetpoint_display", "tempPIDSetpoint_d"],
                ["thermalPenaltyAmps_display", "thermalPenaltyAmps"],
                // PID Term Contributions (P/I/D)
                ["innerTermP_display", "innerTermP"],
                ["innerTermI_display", "innerTermI"],
                ["innerTermP_display", "innerTermP"],
                ["innerTermI_display", "innerTermI"],
                ["innerTermD_display", "innerTermD"],

                ["voltageCtrlActive_display", "voltageControlActive"],
                // voltageTarget and Icv moved to CSV1 otherFields (those fields are in CSV1 at indices 32, 33)
                ["voltageError_display", "voltageError"],
                ["cv_I_display", "cv_I"],
                // cv_D_display removed — D term removed
                // IMU comfort/comfort metrics (in CSV2)
                ["imu_msi_score_ID", "imu_msi_score"],
                ["imu_vomit_pct_ID", "imu_vomit_pct"],
                ["imu_anchorage_comfort_ID", "imu_anchorage_comfort"],
                ["ft_rai_total_win_ID", "ft_rai_total_win"],
                ["ft_rai_total_ses_ID", "ft_rai_total_ses"],
                ["ft_rai_ina228_win_ID", "ft_rai_ina228_win"],
                ["ft_rai_ina228_ses_ID", "ft_rai_ina228_ses"],
                ["ft_rai_ads_state_win_ID", "ft_rai_ads_state_win"],
                ["ft_rai_ads_state_ses_ID", "ft_rai_ads_state_ses"],
                ["ft_rai_bmp_state_win_ID", "ft_rai_bmp_state_win"],
                ["ft_rai_bmp_state_ses_ID", "ft_rai_bmp_state_ses"],
                ["ft_rai_imu_win_ID", "ft_rai_imu_win"],
                ["ft_rai_imu_ses_ID", "ft_rai_imu_ses"],
                ["ft_updateAccelMetrics_win_ID", "ft_updateAccelMetrics_win"],
                ["ft_updateAccelMetrics_ses_ID", "ft_updateAccelMetrics_ses"],
                ["tempReadFailCountID", "tempReadFailCount"],
                ["tempCrcFailCountID", "tempCrcFailCount"],
                ["tempCrcRecoveredCountID", "tempCrcRecoveredCount"],
                ["tempAllFFCountID", "tempAllFFCount"],
                ["tempPowerOn85CountID", "tempPowerOn85Count"],
                ["tempOutOfRangeCountID", "tempOutOfRangeCount"],
                ["tempRequestFailCountID", "tempRequestFailCount"],
                ["tempConnectedFailCountID", "tempConnectedFailCount"],
                ["tempResolutionFixCountID", "tempResolutionFixCount"],
                ["tempRereadFailCountID", "tempRereadFailCount"],
                ["tempResolutionFixCrcFailCountID", "tempResolutionFixCrcFailCount"],
                ["tempEnumerateFailCountID", "tempEnumerateFailCount"],

                // Firmware diagnostics (moved from CSV3 otherFields — all in CSV2_FIELDS)
                ["currentRPMTableIndex_display", "currentRPMTableIndex"],
                ["pidInitialized_display", "pidInitialized"],
                ["overheatCount0_display", "overheatCount0"],
                ["overheatCount1_display", "overheatCount1"],
                ["overheatCount2_display", "overheatCount2"],
                ["overheatCount3_display", "overheatCount3"],
                ["overheatCount4_display", "overheatCount4"],
                ["overheatCount5_display", "overheatCount5"],
                ["overheatCount6_display", "overheatCount6"],
                ["overheatCount7_display", "overheatCount7"],
                ["overheatCount8_display", "overheatCount8"],
                ["overheatCount9_display", "overheatCount9"],
                // safeHours0-9 removed — not in any CSV field array (stale references)
                ["pidSetpoint_display", "pidSetpoint"],
                ["timeSinceLastOverheat_display", "timeSinceLastOverheat"],
                ["FreeInternalRamID", "FreeInternalRam"],
                ["TotalInternalRamID", "TotalInternalRam"],
                ["LargestInternalBlockID", "LargestInternalBlock"],
                ["FreePSRAMID", "FreePSRAM"],
                ["TotalPSRAMID", "TotalPSRAM"],
                ["HeapfragID", "Heapfrag"],
                ["ft_ReadAnalogInputs_win_ID", "ft_ReadAnalogInputs_win"],
                ["ft_ReadAnalogInputs_ses_ID", "ft_ReadAnalogInputs_ses"],
                ["ft_AdjustFieldLearnMode_win_ID", "ft_AdjustFieldLearnMode_win"],
                ["ft_AdjustFieldLearnMode_ses_ID", "ft_AdjustFieldLearnMode_ses"],
                ["ft_uploadSensorHistory_win_ID", "ft_uploadSensorHistory_win"],
                ["ft_uploadSensorHistory_ses_ID", "ft_uploadSensorHistory_ses"],
                ["ft_dumpLongTermRing_win_ID", "ft_dumpLongTermRing_win"],
                ["ft_dumpLongTermRing_ses_ID", "ft_dumpLongTermRing_ses"],
                ["ft_fastAltDrain_win_ID", "ft_fastAltDrain_win"],
                ["ft_fastAltDrain_ses_ID", "ft_fastAltDrain_ses"],
                ["ft_faMatrixFlush_win_ID", "ft_faMatrixFlush_win"],
                ["ft_faMatrixFlush_ses_ID", "ft_faMatrixFlush_ses"],
                ["ft_faDetector_win_ID", "ft_faDetector_win"],
                ["ft_faDetector_ses_ID", "ft_faDetector_ses"],
                ["ft_faWindowFinalize_win_ID", "ft_faWindowFinalize_win"],
                ["ft_faWindowFinalize_ses_ID", "ft_faWindowFinalize_ses"],
                ["faChanState_ID", "faChanState"],
                ["faCellsUsed_ID", "faCellsUsed"],
                ["faDetectK_ID", "faDetectK"],
                ["faAnomalyCount_ID", "faAnomalyCount"],
                ["faSesPkpkWorst_ID", "faSesPkpkWorst"],
                ["faDomAmp_ID", "faDomAmp"],
                ["faDomFreq_ID", "faDomFreqHz"],
                ["faDomRpm_ID", "faDomRpm"],
                ["ft_uploadBufferedRecords_win_ID", "ft_uploadBufferedRecords_win"],
                ["ft_uploadBufferedRecords_ses_ID", "ft_uploadBufferedRecords_ses"],
                ["ft_buildConfigPayload_win_ID", "ft_buildConfigPayload_win"],
                ["ft_buildConfigPayload_ses_ID", "ft_buildConfigPayload_ses"],
                ["ft_altHealth_win_ID", "ft_altHealth_win"],
                ["ft_altHealth_ses_ID", "ft_altHealth_ses"],
                ["ft_altFold_win_ID", "ft_altFold_win"],
                ["ft_altFold_ses_ID", "ft_altFold_ses"],
                ["ft_boatPerf_win_ID", "ft_boatPerf_win"],
                ["ft_boatPerf_ses_ID", "ft_boatPerf_ses"],
                ["VeTime2_ID", "VeTime2"],
                ["systemIDActive_ID", "systemIDActive"],
                ["systemIDResultsReady_ID", "systemIDResultsReady"],
                ["systemIDStepAmp_0_ID", "systemIDStepAmp_0"],
                ["systemIDStepAmp_1_ID", "systemIDStepAmp_1"],
                ["systemIDStepAmp_2_ID", "systemIDStepAmp_2"],
                ["systemIDQuietPP_0_ID", "systemIDQuietPP_0"],
                ["systemIDQuietPP_1_ID", "systemIDQuietPP_1"],
                ["systemIDQuietPP_2_ID", "systemIDQuietPP_2"],
                ["systemIDAbortReason_ID", "systemIDAbortReason"],
                ["systemIDAbortPhase_ID", "systemIDAbortPhase"],
                ["systemIDRiseDelay_0_ID", "systemIDRiseDelay_0"],
                ["systemIDRiseDelay_1_ID", "systemIDRiseDelay_1"],
                ["systemIDRiseDelay_2_ID", "systemIDRiseDelay_2"],
                ["systemIDFallDelay_0_ID", "systemIDFallDelay_0"],
                ["systemIDFallDelay_1_ID", "systemIDFallDelay_1"],
                ["systemIDFallDelay_2_ID", "systemIDFallDelay_2"],
                ["systemIDRiseAvg_ID", "systemIDRiseAvg"],
                ["systemIDFallAvg_ID", "systemIDFallAvg"],

                // Fields moved from CSV1 otherFields (were in old 137-field CSV1; now in CSV2)
                ["VeTimeID", "VeTime"],
                ["MaximumLoopTimeID", "MaximumLoopTime"],
                ["ft_loop_win_ID", "loopTime5sWindow_ms"],
                ["ft_loop_ses_ID", "MaximumLoopTime_ms"],
                ["ft_loopFieldOn_win_ID", "loopFieldOnWin_ms"],
                ["ft_loopFieldOn_ses_ID", "loopFieldOnSes_ms"],
                ["ft_SendWifiData_win_ID", "ft_SendWifiData_win"],
                ["ft_SendWifiData_ses_ID", "ft_SendWifiData_ses"],
                ["ft_CheckAlarms_win_ID", "ft_CheckAlarms_win"],
                ["ft_CheckAlarms_ses_ID", "ft_CheckAlarms_ses"],
                ["ft_calculateDerivedMetrics_win_ID", "ft_calculateDerivedMetrics_win"],
                ["ft_calculateDerivedMetrics_ses_ID", "ft_calculateDerivedMetrics_ses"],
                ["ft_logDashboardValues_win_ID", "ft_logDashboardValues_win"],
                ["ft_logDashboardValues_ses_ID", "ft_logDashboardValues_ses"],
                ["ft_updateSystemHealthStats_win_ID", "ft_updateSystemHealthStats_win"],
                ["ft_updateSystemHealthStats_ses_ID", "ft_updateSystemHealthStats_ses"],
                ["ft_checkWiFiConnection_win_ID", "ft_checkWiFiConnection_win"],
                ["ft_checkWiFiConnection_ses_ID", "ft_checkWiFiConnection_ses"],
                ["ft_ch1_compute_stats_win_ID", "ft_ch1_compute_stats_win"],
                ["ft_ch1_compute_stats_ses_ID", "ft_ch1_compute_stats_ses"],
                ["ft_UpdateEngineRuntime_win_ID", "ft_UpdateEngineRuntime_win"],
                ["ft_UpdateEngineRuntime_ses_ID", "ft_UpdateEngineRuntime_ses"],
                ["ft_UpdateEngineFuel_win_ID", "ft_UpdateEngineFuel_win"],
                ["ft_UpdateEngineFuel_ses_ID", "ft_UpdateEngineFuel_ses"],
                ["ft_UpdateBatterySOC_win_ID", "ft_UpdateBatterySOC_win"],
                ["ft_UpdateBatterySOC_ses_ID", "ft_UpdateBatterySOC_ses"],
                ["ft_UpdateTravelStatistics_win_ID", "ft_UpdateTravelStatistics_win"],
                ["ft_UpdateTravelStatistics_ses_ID", "ft_UpdateTravelStatistics_ses"],
                ["ft_UpdateBoardTempPressureMaximums_win_ID", "ft_UpdateBoardTempPressureMaximums_win"],
                ["ft_UpdateBoardTempPressureMaximums_ses_ID", "ft_UpdateBoardTempPressureMaximums_ses"],
                ["ft_handleSocGainReset_win_ID", "ft_handleSocGainReset_win"],
                ["ft_handleSocGainReset_ses_ID", "ft_handleSocGainReset_ses"],
                ["ft_handleAltZeroReset_win_ID", "ft_handleAltZeroReset_win"],
                ["ft_handleAltZeroReset_ses_ID", "ft_handleAltZeroReset_ses"],
                ["ft_calculateChargeTimes_win_ID", "ft_calculateChargeTimes_win"],
                ["ft_calculateChargeTimes_ses_ID", "ft_calculateChargeTimes_ses"],
                ["ft_UpdateSailingMetrics_win_ID", "ft_UpdateSailingMetrics_win"],
                ["ft_UpdateSailingMetrics_ses_ID", "ft_UpdateSailingMetrics_ses"],
                ["ft_updateWeatherMode_win_ID", "ft_updateWeatherMode_win"],
                ["ft_updateWeatherMode_ses_ID", "ft_updateWeatherMode_ses"],
                ["ft_updateSensorWindow_win_ID", "ft_updateSensorWindow_win"],
                ["ft_updateSensorWindow_ses_ID", "ft_updateSensorWindow_ses"],
                ["ft_checkTimeSync_win_ID", "ft_checkTimeSync_win"],
                ["ft_checkTimeSync_ses_ID", "ft_checkTimeSync_ses"],
                ["HeadingNMEAID", "HeadingNMEA"],
                ["EngineCyclesID", "EngineCycles"],
                ["WifiStrengthID", "WifiStrength"],
                ["CurrentSessionDurationID", "CurrentSessionDuration"],
                ["currentPartitionTypeID", "currentPartitionType"],
                // Voltage & Current Protection events
                ["fastOvCurrentCapID", "fastOvCurrentCap"],
                ["fastOvClampCountID", "fastOvClampCount"],
                ["fastOvHardCountID", "fastOvHardCount"],
                ["iExcessCountID", "iExcessCount"],
                ["inaOVCountID", "inaOVCount"],
                ["hardOCCountID", "hardOCCount"],
                ["voltSpikeCountID", "voltSpikeCount"],
                ["voltDisagreeCritCountID", "voltDisagreeCritCount"],
                ["voltDisagreeWarnCountID", "voltDisagreeWarnCount"],
                ["voltImplausibleCountID", "voltImplausibleCount"],
                ["tempCritCountID", "tempCritCount"],
                ["tempSustainedCountID", "tempSustainedCount"],
                ["tempStaleCountID", "tempStaleCount"],
                ["currentStaleCountID", "currentStaleCount"],
                // CH1 / INA timing stats
                ["ch1_last_ms_ID", "ch1_last_ms"],
                ["ch1_avg_10s_ID", "ch1_avg_10s"],
                ["ch1_worst_10s_ID", "ch1_worst_10s"],
                ["ch1_over2x_10s_ID", "ch1_over2x_10s"],
                ["ch1_avg_2m_ID", "ch1_avg_2m"],
                ["ch1_worst_2m_ID", "ch1_worst_2m"],
                ["ch1_over2x_2m_ID", "ch1_over2x_2m"],
                ["ch1_avg_at_ID", "ch1_avg_at"],
                ["ch1_worst_at_ID", "ch1_worst_at"],
                ["ch1_over2x_at_ID", "ch1_over2x_at"],
                // Inner Current PID firing interval (field-on)
                ["pf_last_ms_ID", "pf_last_ms"],
                ["pf_avg_10s_ID", "pf_avg_10s"],
                ["pf_worst_10s_ID", "pf_worst_10s"],
                ["pf_over2x_10s_ID", "pf_over2x_10s"],
                ["pf_avg_2m_ID", "pf_avg_2m"],
                ["pf_worst_2m_ID", "pf_worst_2m"],
                ["pf_over2x_2m_ID", "pf_over2x_2m"],
                ["pf_avg_at_ID", "pf_avg_at"],
                ["pf_worst_at_ID", "pf_worst_at"],
                ["pf_over2x_at_ID", "pf_over2x_at"],
                ["ina_last_ms_ID", "ina_last_ms"],
                ["ina_avg_10s_ID", "ina_avg_10s"],
                ["ina_worst_10s_ID", "ina_worst_10s"],
                ["ina_over2x_10s_ID", "ina_over2x_10s"],
                ["ina_avg_2m_ID", "ina_avg_2m"],
                ["ina_worst_2m_ID", "ina_worst_2m"],
                ["ina_over2x_2m_ID", "ina_over2x_2m"],
                ["ina_avg_at_ID", "ina_avg_at"],
                ["ina_worst_at_ID", "ina_worst_at"],
                ["ina_over2x_at_ID", "ina_over2x_at"],
                ["vl_last_ms_ID", "vl_last_ms"],
                ["vl_avg_10s_ID", "vl_avg_10s"],
                ["vl_worst_10s_ID", "vl_worst_10s"],
                ["vl_over2x_10s_ID", "vl_over2x_10s"],
                ["vl_avg_2m_ID", "vl_avg_2m"],
                ["vl_worst_2m_ID", "vl_worst_2m"],
                ["vl_over2x_2m_ID", "vl_over2x_2m"],
                ["vl_avg_at_ID", "vl_avg_at"],
                ["vl_worst_at_ID", "vl_worst_at"],
                ["vl_over2x_at_ID", "vl_over2x_at"],
                ["nvsSecsSinceLastSave_ID", "nvsSecsSinceLastSave"],
                ["nvsFullSaveLastMs_ID", "nvsFullSaveLastMs"],
                ["nvsFullSaveWorstMs_ID", "nvsFullSaveWorstMs"],
                ["nvsFullSaveCount_ID", "nvsFullSaveCount"],

                // 28 ignition-cycle watermarks — hi/lo spans next to each primary value on the dashboard.
                // IBV and vacc are sent ×10 and rendered with toFixed(1) in the formatter chain above.
                ["MeasAmpsID_hi",                "wmIgn_amps_hi"],     ["MeasAmpsID_lo",                "wmIgn_amps_lo"],
                ["AltTempID_hi",                 "wmIgn_altTempF_hi"], ["AltTempID_lo",                 "wmIgn_altTempF_lo"],
                ["IBVID_hi",                     "wmIgn_IBV_hi"],      ["IBVID_lo",                     "wmIgn_IBV_lo"],
                ["BCurrID_hi",                   "wmIgn_Bcur_hi"],     ["BCurrID_lo",                   "wmIgn_Bcur_lo"],
                ["SOC_percentID_hi",             "wmIgn_SOC_hi"],      ["SOC_percentID_lo",             "wmIgn_SOC_lo"],
                ["RPMID_hi",                     "wmIgn_RPM_hi"],      ["RPMID_lo",                     "wmIgn_RPM_lo"],
                ["SOGNMEA_ID_hi",                "wmIgn_SOG_hi"],      ["SOGNMEA_ID_lo",                "wmIgn_SOG_lo"],
                ["VMGNMEA_ID_hi",                "wmIgn_VMGman_hi"],   ["VMGNMEA_ID_lo",                "wmIgn_VMGman_lo"],
                ["VMGUpwind_ID_hi",              "wmIgn_VMGup_hi"],    ["VMGUpwind_ID_lo",              "wmIgn_VMGup_lo"],
                ["ApparentWindSpeedNMEA_ID_hi",  "wmIgn_AWS_hi"],      ["ApparentWindSpeedNMEA_ID_lo",  "wmIgn_AWS_lo"],
                ["TrueWindSpeedNMEA_ID_hi",      "wmIgn_TWS_hi"],      ["TrueWindSpeedNMEA_ID_lo",      "wmIgn_TWS_lo"],
                ["imu_heel_deg_ID_hi",           "wmIgn_heel_hi"],     ["imu_heel_deg_ID_lo",           "wmIgn_heel_lo"],
                ["imu_pitch_deg_ID_hi",          "wmIgn_pitch_hi"],    ["imu_pitch_deg_ID_lo",          "wmIgn_pitch_lo"],
                ["imu_vertical_accel_g_ID_hi",   "wmIgn_vacc_hi"],     ["imu_vertical_accel_g_ID_lo",   "wmIgn_vacc_lo"],
                ["baroPressureID_hi",            "wmIgn_baro_hi"],     ["baroPressureID_lo",            "wmIgn_baro_lo"],
                ["ambientTempID_hi",             "wmIgn_ambient_hi"],  ["ambientTempID_lo",             "wmIgn_ambient_lo"],

                // Victron VE.Direct solar/MPPT live (CS/MPPT/ERR codes decoded separately below)
                ["VictronSolarPowerID",         "VictronSolarPower_W"],
                ["VictronSolarVoltageID",       "VictronSolarVoltage_V"],
                ["VictronSolarCurrentID",       "VictronSolarCurrent_A"],
                ["VictronYieldTodayID",         "VictronYieldToday_kWh"],
                ["VictronMaxPowerTodayID",      "VictronMaxPowerToday_W"],
                ["VictronYieldYesterdayID",     "VictronYieldYesterday_kWh"],
                ["VictronMaxPowerYesterdayID",  "VictronMaxPowerYesterday_W"],

            ];

            // Update other fields every cycle
            updateFields(otherFields);

            // Victron MPPT status codes -> human text (CS charge state, MPPT tracker mode, ERR)
            (function () {
                const cs = { 0: 'Off', 2: 'Fault', 3: 'Bulk', 4: 'Absorption', 5: 'Float', 7: 'Equalize', 245: 'Starting', 247: 'Auto equalize', 252: 'Ext control' };
                const mp = { 0: 'Off', 1: 'V/I limited', 2: 'Active MPPT' };
                const setText = (id, txt) => { const el = document.getElementById(id); if (el && el.textContent !== txt) el.textContent = txt; };
                if (data.VictronChargeState !== undefined) {
                    const c = Number(data.VictronChargeState);
                    setText('VictronChargeStateID', c < 0 ? '—' : (cs[c] || ('Code ' + c)));
                }
                if (data.VictronMPPTMode !== undefined) {
                    const m = Number(data.VictronMPPTMode);
                    setText('VictronMPPTModeID', m < 0 ? '—' : (mp[m] || ('Code ' + m)));
                }
                if (data.VictronError !== undefined) {
                    const e = Number(data.VictronError);
                    setText('VictronErrorID', e < 0 ? '—' : (e === 0 ? 'OK' : ('Err ' + e)));
                }
            })();

            // Live engine fuel flow + economy (both ×100; firmware sends 0 when engine off / no GPS speed)
            (function () {
                const setText = (id, txt) => { const el = document.getElementById(id); if (el && el.textContent !== txt) el.textContent = txt; };
                if (data.currentFuelGPH !== undefined) {
                    const gph = Number(data.currentFuelGPH) / 100;
                    setText('fuelGPHID', gph > 0 ? gph.toFixed(2) : '—');
                }
                if (data.currentNMPG !== undefined) {
                    const nmpg = Number(data.currentNMPG) / 100;
                    setText('fuelNMPGID', nmpg > 0 ? nmpg.toFixed(2) : '—');
                }
            })();

            // Session fuel-economy curve (mpg vs RPM). try/catch so a chart bug can't break the dispatcher.
            try { if (typeof window.updateFuelCurve === 'function') window.updateFuelCurve(data); }
            catch (e) { console.warn('fuel curve update failed:', e); }

            // Refresh the barometer panel (Other tab) each CSV2 cycle. Cheap no-op if the
            // panel hasn't been initialized yet (tab never opened). try/catch so a bug in
            // the baro module can never break the rest of the dispatcher (firmware version,
            // device id, etc. live downstream of this line).
            if (typeof window.updateBaroDisplay === 'function') {
                try { window.updateBaroDisplay(data, window.sensorAges); }
                catch (e) { console.warn('baro update failed:', e); }
            }

            // Temperature PID terms displayed as current contributions (sign-flipped:
            // positive = adding amps, negative = removing amps)
            for (const [id, key] of [
                ["outerTermP_display", "outerTermP"],
                ["outerTermI_display", "outerTermI"],
                ["outerTermLookahead_display", "outerTermLookahead"],
                ["thermalSlopeFPerSec_display", "thermalSlopeFPerSec"]
            ]) {
                const raw = data[key];
                if (raw === undefined) continue;
                const newText = key === "thermalSlopeFPerSec"
                    ? toDisplayTempDelta(raw / 1000).toFixed(3)
                    : (-raw / 100).toFixed(2);
                const cacheKey = `${id}_${key}`;
                if (lastValues.get(cacheKey) !== newText) {
                    lastValues.set(cacheKey, newText);
                    scheduleDOMUpdateOptimized(id, newText);
                }
            }

            // Update thermal live score spans in the Diag Control Accuracy table (always-on).
            // thermalLiveScore0-3 are ×10000 in CSV2; show the bare value — the row header lists the windows.
            {
                for (let i = 0; i < 4; i++) {
                    const raw = data['thermalLiveScore' + i];
                    if (raw === undefined) continue;
                    const v = raw / 10000;
                    const txt = v > 0 ? v.toFixed(4) : '—';
                    const cacheKey = 'thermalLiveScoreAlt_' + i;
                    if (lastValues.get(cacheKey) !== txt) {
                        lastValues.set(cacheKey, txt);
                        const el = document.getElementById('thermalLiveScoreAlt' + i);
                        if (el) el.textContent = txt;
                    }
                }
            }

            // Update GPS display and manual entry form visibility
            if (data.LatitudeNMEA !== undefined && data.LongitudeNMEA !== undefined) {
                const latDegrees = data.LatitudeNMEA / 1000000;
                const lonDegrees = data.LongitudeNMEA / 1000000;
                updateGPSDisplay(latDegrees, lonDegrees);
            }

            // Update echos every cycle
            updateAllEchosOptimized(data);

            // Update toggle states
            updateTogglesFromData(data);
            // capLimitMode / HiLow pending toggle confirmation handled in CSVData3 handler

            // Update life indicators
            updateLifeIndicators(data);

            gLastChargeStage = data.chargeStageDisplay; // keep gate functions up to date

            const chargeStageEl = document.getElementById('charge-stage');
            if (chargeStageEl) {
                const stage = data.chargeStageDisplay;
                // Active test modes override the normal charge stage label
                if (sysidPollInterval !== null) {
                    chargeStageEl.textContent = 'PLANT TEST';
                    chargeStageEl.className = 'charge-stage charge-stage-test';
                } else if (data.TuningMode) {
                    chargeStageEl.textContent = 'CURR TEST';
                    chargeStageEl.className = 'charge-stage charge-stage-test';
                } else if (data.CVTuningMode) {
                    chargeStageEl.textContent = 'CV TEST';
                    chargeStageEl.className = 'charge-stage charge-stage-test';
                } else if (data.ThermalTuningMode) {
                    chargeStageEl.textContent = 'THERM TEST';
                    chargeStageEl.className = 'charge-stage charge-stage-test';
                } else if (stage === 1) {
                    chargeStageEl.textContent = 'BULK';
                    chargeStageEl.className = 'charge-stage charge-stage-bulk';
                } else if (stage === 2) {
                    chargeStageEl.textContent = 'ABSORPTION';
                    chargeStageEl.className = 'charge-stage charge-stage-absorption';
                } else if (stage === 3) {
                    chargeStageEl.textContent = 'FLOAT';
                    chargeStageEl.className = 'charge-stage charge-stage-float';
                } else if (stage === 4) {
                    // Field status word already shows MANUAL — suppress duplicate badge
                    chargeStageEl.textContent = '';
                    chargeStageEl.className = 'charge-stage charge-stage-hidden';
                } else if (stage === 5) {
                    chargeStageEl.textContent = 'MAINTAIN';
                    chargeStageEl.className = 'charge-stage charge-stage-maintain';
                } else if (stage === 6) {
                    chargeStageEl.textContent = 'TARGET V';
                    chargeStageEl.className = 'charge-stage charge-stage-target-v';
                } else if (stage === 7) {
                    chargeStageEl.textContent = 'IDLE';
                    chargeStageEl.className = 'charge-stage charge-stage-idle';
                } else {
                    chargeStageEl.textContent = '';
                    chargeStageEl.className = 'charge-stage charge-stage-hidden';
                }
            }
            updateVoltageModeGreyout(data.chargeStageDisplay);

            // Update the test-active floating panel from the CSV1/2 stream
            _testActiveCSV1 = data.TuningMode ? 'curr' : data.CVTuningMode ? 'cv' : data.ThermalTuningMode ? 'thermal' : null;
            updateTestActivePanel();

            updateFirmwareVersion(data.firmwareVersionInt);
            updateDeviceId();
        }, false);

        source.addEventListener('CSVData3', function (e) {
            const raw = e.data.split(',').map(Number);

            const declaredCount = raw[0];
            const values = raw.slice(1);

            if (values.length !== declaredCount) {
                if (!window.lastCsv3WarnTime || Date.now() - window.lastCsv3WarnTime > 10000) {
                    console.warn(`[CSV3] length mismatch: declared=${declaredCount}, actual=${values.length}`);
                    window.lastCsv3WarnTime = Date.now();
                }
                return;
            }
            if (declaredCount !== CSV3_FIELDS.length) {
                if (!window.lastCsv3WarnTime || Date.now() - window.lastCsv3WarnTime > 10000) {
                    console.warn(`[CSV3] schema mismatch: ESP32=${declaredCount}, UI=${CSV3_FIELDS.length}`);
                    window.lastCsv3WarnTime = Date.now();
                }
                return;
            }

            const data = Object.fromEntries(CSV3_FIELDS.map((key, i) => [key, values[i]]));
            g_lastCsv3 = data;  // cache for cvBinToCsv header

            //CSVData3
            /*             ## 🎯 **Summary**
            ```
            ┌─────────────────────┐
            │ data = {...}        │  ← 200+ values stored here
            │ (all CSV values)    │
            └─────────────────────┘
                      │
                      │  ┌──────────────────────────────┐
                      └─→│ otherFields = [...]          │  ← Whitelist of ~40 items
                         │ (which ones to display)      │
                         └──────────────────────────────┘
                                    │
                                    ↓
                         ┌──────────────────────────────┐
                         │ updateFields(otherFields)    │
                         │                              │
                         │ For each item in whitelist:  │
                         │  - Get value from data       │
                         │  - Apply scaling if needed   │
                         │  - Update HTML element       │
                         └──────────────────────────────┘ */
            if (data.stateRevision !== undefined) {
                lastSeenRev = data.stateRevision;
            }
            if (data.displayTempUnit !== undefined && data.displayTempUnit !== displayTempUnit) {
                displayTempUnit = data.displayTempUnit;
                updateAllTempUnitLabels();
            }
            // Cache interval/window from CSV3 (settings stream) for gap detection and plot sizing
            if (data.webgaugesinterval) {
                window._lastKnownInterval = data.webgaugesinterval;
            }
            if (data.plotTimeWindow) {
                window._lastKnownTimeWindow = data.plotTimeWindow;
            }
            // Update plot axes/configuration whenever settings arrive
            updatePlotConfiguration(data);
            // Update all setting echo labels and toggles from CSV3 data
            updateAllEchosOptimized(data);
            updateTogglesFromData(data);
            // capLimitMode pending toggle confirmation (moved from CSV2 handler — capLimitMode is now in CSV3)
            if (data.capLimitMode !== undefined) {
                const pending = pendingToggles.get('capLimitMode');
                if (pending) {
                    if (data.capLimitMode === pending.desiredValue) {
                        pendingToggles.delete('capLimitMode');
                        setCapMode(data.capLimitMode === 1 ? 'kw' : 'amps');
                    } else if (
                        (data.stateRevision !== undefined && data.stateRevision > pending.baseRev) ||
                        (pending.deadlineMs !== undefined && Date.now() > pending.deadlineMs)
                    ) {
                        pendingToggles.delete('capLimitMode');
                        setCapMode(data.capLimitMode === 1 ? 'kw' : 'amps');
                    } else {
                        if (pending.deadlineMs === undefined) {
                            pending.deadlineMs = Date.now() + 2500;
                        }
                    }
                } else {
                    setCapMode(data.capLimitMode === 1 ? 'kw' : 'amps');
                }
            }
            // HiLow pending toggle confirmation (moved from CSV2 handler — HiLow is now in CSV3)
            if (data.HiLow !== undefined) {
                const pending = pendingToggles.get('HiLow');
                if (pending) {
                    if (data.HiLow === pending.desiredValue) {
                        pendingToggles.delete('HiLow');
                        setChargeRateMode(data.HiLow === 0 ? 'low' : 'high');
                    } else if (
                        (data.stateRevision !== undefined && data.stateRevision > pending.baseRev) ||
                        (pending.deadlineMs !== undefined && Date.now() > pending.deadlineMs)
                    ) {
                        pendingToggles.delete('HiLow');
                        setChargeRateMode(data.HiLow === 0 ? 'low' : 'high');
                    } else {
                        if (pending.deadlineMs === undefined) {
                            pending.deadlineMs = Date.now() + 2500;
                        }
                    }
                } else {
                    setChargeRateMode(data.HiLow === 0 ? 'low' : 'high');
                }
            }
            // updateFields for CSVData3     the only time a user setting needs to be displayed via updateFields() is if its shown somewhere other than an Echo (like a status indicator, for example).  Echos are updated elsewhere (updateAllEchosOptimized).
            const updateFields = (fieldArray) => {
                for (const [elementId, key] of fieldArray) {
                    const value = data[key];
                    if (value === undefined) continue;

                    let newTextContent;
                    const num = Number(value);
                    if (!Number.isFinite(num)) {

                        newTextContent = "—";
                    }
                    // timeSinceLastOverheat is pre-divided by 1000 in C++ (seconds) — convert to hours for display
                    else if (key === "timeSinceLastOverheat") {
                        newTextContent = (value / 3600).toFixed(2);
                    }
                    // overheatingPenaltyTimer is pre-divided by 1000 in C++ — display raw seconds
                    else if (key === "overheatingPenaltyTimer") {
                        newTextContent = (value).toFixed(0);
                    }
                    // Special handling for imuEnabled (in CSV3)
                    else if (key === "imuEnabled") {
                        newTextContent = value === 1 ? "Enabled" : "Disabled";
                    }
                    // Special handling for imuMountOrientation (in CSV3)
                    else if (key === "imuMountOrientation") {
                        const orientations = ["Fwd Bulkhead", "Aft Bulkhead", "Port Wall", "Stbd Wall"];
                        newTextContent = orientations[value] || `Unknown (${value})`;
                    }
                    // Values sent as raw milliseconds — divide by 1000 to display in seconds
                    else if (["LearningSettlingPeriod", "rebulkDebounceTime", "MinFloatTime"].includes(key)) {
                        newTextContent = (value / 1000).toFixed(0);
                    }
                    // Values scaled by 100 
                    else if (["pidSetpoint", "FieldResistance", "averageTableValue", "TailCurrent_A", "RebulkVoltage", "SOC_BlockRebulk_percent", "SOC_AllowRebulk_percent", "DutySlowRampRate", "VoltageKi", "VoltageKp"].includes(key)) {
                        newTextContent = (value / 100).toFixed(2);
                    }

                    // Values scaled by 3600000 for hours
                    else if (key.startsWith("safeHours")) {
                        newTextContent = (value / 3600).toFixed(2);
                    }
                    // Function timing values sent as raw µs — divide by 1000 to display ms
                    else if (key.startsWith("ft_") || key === "VeTime2") {
                        newTextContent = (value / 1000).toFixed(1);
                    }
                    else {
                        newTextContent = Math.round(value);
                    }

                    const cacheKey = `${elementId}_${key}`;
                    if (lastValues.get(cacheKey) !== newTextContent) {
                        lastValues.set(cacheKey, newTextContent);
                        scheduleDOMUpdateOptimized(elementId, newTextContent);
                    }
                }
            };


            // Other fields for CSVData3 - update every cycle (no throttling since server controls timing)
            const otherFields = [
                // Step 2: This array is a whitelist - it says "only update these specific HTML elements with these specific data values".
                // Note: Many of these might not have corresponding HTML elements yet
                // This list includes potential element IDs that might exist
                // IMU settings (these fields live in CSV3)
                ["imuEnabled_ID", "imuEnabled"],
                ["imuMountOrientation_ID", "imuMountOrientation"],

            ];

            // CSVData3
            updateFields(otherFields);   // Step 3: Process the whitelist
            updatePidTuningConfiguration(data);  // ADD THIS LINE - Update PID plot config

            // Update test-active panel with sysid phase.
            // systemIDActive is in CSV2_FIELDS, not CSV3 — read from DOM,
            // which CSV2 otherFields already keeps current via ["systemIDActive_ID", "systemIDActive"].
            const sysidPhaseNum = parseInt(getField("systemIDActive_ID") ?? 0);
            _testActiveCSV3 = sysidPhaseNum > 0 ? 'sysid' : null;
            if (_testPanelCurrentTest === 'sysid') {
                updateTestPanelScore(undefined, undefined, SYSID_PHASE_NAMES[sysidPhaseNum] ?? ('Phase ' + sysidPhaseNum));
            }
            updateTestActivePanel();


            // =====================
            // FUEL TABLE INIT 
            // =====================

            // Initialize fuel table inputs from ESP32 data exactly once
            if (!window.fuelTableInitialized) {
                for (let i = 0; i < 10; i++) {
                    const rpmInput = document.getElementById(`fuelTableRPM${i}_input`);
                    if (rpmInput && data[`fuelTableRPM${i}`] !== undefined) {
                        rpmInput.value = data[`fuelTableRPM${i}`];
                    }

                    const gphInput = document.getElementById(`fuelTableGPH${i}_input`);
                    if (gphInput && data[`fuelTableGPH${i}`] !== undefined) {
                        gphInput.value = (data[`fuelTableGPH${i}`] / 100).toFixed(2);
                    }
                }

                // Lock initialization only if real data arrived
                const hasValidData =
                    data.fuelTableRPM0 !== undefined &&
                    data.fuelTableGPH0 !== undefined;

                if (hasValidData) {
                    window.fuelTableInitialized = true;
                }
            }

            // Fuel table live update / save confirmation
            for (let i = 0; i < 10; i++) {
                const rpmInput = document.getElementById(`fuelTableRPM${i}_input`);
                if (rpmInput && rpmInput !== document.activeElement && !dirtyFuelInputs.has(rpmInput.id)) {
                    const incomingRPM = data[`fuelTableRPM${i}`];
                    const pendingRPM = pendingFuelTableValues.get(rpmInput.id);

                    if (pendingRPM) {
                        if (incomingRPM !== undefined && Number(incomingRPM) === Number(pendingRPM.value)) {
                            rpmInput.classList.remove('table-input-pending');
                            pendingFuelTableValues.delete(rpmInput.id);
                            rpmInput.value = incomingRPM;
                        } else if (Date.now() > pendingRPM.deadlineMs) {
                            rpmInput.classList.remove('table-input-pending');
                            pendingFuelTableValues.delete(rpmInput.id);
                            setTableStatus('fuel-table-status', 'Save not confirmed');
                            if (incomingRPM !== undefined) rpmInput.value = incomingRPM;
                        }
                    } else if (window.fuelTableInitialized && incomingRPM !== undefined) {
                        rpmInput.value = incomingRPM;
                    }
                }

                const gphInput = document.getElementById(`fuelTableGPH${i}_input`);
                if (gphInput && gphInput !== document.activeElement && !dirtyFuelInputs.has(gphInput.id)) {
                    const incomingGPH = data[`fuelTableGPH${i}`] !== undefined ? (data[`fuelTableGPH${i}`] / 100).toFixed(2) : undefined;
                    const pendingGPH = pendingFuelTableValues.get(gphInput.id);

                    if (pendingGPH) {
                        if (incomingGPH !== undefined && Math.abs(Number(incomingGPH) - Number(pendingGPH.value)) < 0.011) {
                            gphInput.classList.remove('table-input-pending');
                            pendingFuelTableValues.delete(gphInput.id);
                            gphInput.value = incomingGPH;
                        } else if (Date.now() > pendingGPH.deadlineMs) {
                            gphInput.classList.remove('table-input-pending');
                            pendingFuelTableValues.delete(gphInput.id);
                            setTableStatus('fuel-table-status', 'Save not confirmed');
                            if (incomingGPH !== undefined) gphInput.value = incomingGPH;
                        }
                    } else if (window.fuelTableInitialized && incomingGPH !== undefined) {
                        gphInput.value = incomingGPH;
                    }
                }
            }

            if (pendingFuelTableValues.size === 0 && !fuelTableHasChanges) {
                const fuelStatus = document.getElementById('fuel-table-status');
                if (fuelStatus && fuelStatus.textContent === 'Saving...') {
                    setTableStatus('fuel-table-status', 'Saved');
                    setTrackedTimeout(() => {
                        if (!fuelTableHasChanges && pendingFuelTableValues.size === 0) {
                            setTableStatus('fuel-table-status', '');
                        }
                    }, 1200);
                }
            }

            // =========================
            // LEARNING TABLE - update every cycle, skip focused inputs
            // =========================
            const hasValidData =
                data.rpmTableRPMPoints0 !== undefined &&
                data.rpmCapCurrentTable0 !== undefined;

            if (hasValidData) {
                const focused = document.activeElement;
                for (let i = 0; i < 10; i++) {
                    const rpmInput = document.getElementById(`rpmTableRPMPoints${i}_input`);
                    if (rpmInput && rpmInput !== focused && !dirtyInputs.has(rpmInput.id)) {
                        const p = pendingTableValues.get(rpmInput.id);
                        if (p) {
                            const incoming = data[`rpmTableRPMPoints${i}`];
                            const confirmed = incoming !== undefined && Number(incoming) === Number(p.value);
                            if (confirmed || Date.now() >= p.deadlineMs) {
                                rpmInput.classList.remove('table-input-pending');
                                pendingTableValues.delete(rpmInput.id);
                                if (incoming !== undefined) rpmInput.value = incoming;
                            }
                        } else if (data[`rpmTableRPMPoints${i}`] !== undefined) {
                            rpmInput.value = data[`rpmTableRPMPoints${i}`];
                        }
                    }
                    const capInput = document.getElementById(`rpmCapCurrentTable${i}_input`);
                    const kwInput = document.getElementById(`rpmCapKW${i}_input`);

                    if (currentCapMode === 'amps') {
                        if (capInput && capInput !== focused && !dirtyInputs.has(capInput.id)) {
                            const p = pendingTableValues.get(capInput.id);
                            if (p) {
                                const incoming = data[`rpmCapCurrentTable${i}`];
                                const confirmed = incoming !== undefined && Math.round(incoming / 100) === Number(p.value);
                                if (confirmed || Date.now() >= p.deadlineMs) {
                                    capInput.classList.remove('table-input-pending');
                                    pendingTableValues.delete(capInput.id);
                                    if (incoming !== undefined) capInput.value = Math.round(incoming / 100);
                                }
                            } else if (data[`rpmCapCurrentTable${i}`] !== undefined) {
                                capInput.value = Math.round(data[`rpmCapCurrentTable${i}`] / 100);
                            }
                        }
                        if (kwInput) kwInput.value = ampsToKW(parseFloat(capInput?.value) || 0).toFixed(2);
                    } else {
                        if (kwInput && kwInput !== focused && !dirtyInputs.has(kwInput.id)) {
                            const p = pendingTableValues.get(kwInput.id);
                            if (p) {
                                const incoming = data[`rpmCapPowerTable${i}`] !== undefined
                                    ? (data[`rpmCapPowerTable${i}`] / 1000).toFixed(2)
                                    : undefined;
                                const confirmed = incoming !== undefined && Math.abs(Number(incoming) - Number(p.value)) < 0.011;
                                if (confirmed || Date.now() >= p.deadlineMs) {
                                    kwInput.classList.remove('table-input-pending');
                                    pendingTableValues.delete(kwInput.id);
                                    if (incoming !== undefined) kwInput.value = incoming;
                                }
                            } else if (data[`rpmCapPowerTable${i}`] !== undefined) {
                                kwInput.value = (data[`rpmCapPowerTable${i}`] / 1000).toFixed(2);
                            }
                        }
                        if (capInput) capInput.value = Math.round(kwToAmps(parseFloat(kwInput?.value) || 0));
                    }

                    const minDutyInput = document.getElementById(`rpmMinDutyTable${i}_input`);
                    if (minDutyInput && minDutyInput !== focused && !dirtyInputs.has(minDutyInput.id)) {
                        const p = pendingTableValues.get(minDutyInput.id);
                        if (p) {
                            const incoming = data[`rpmMinDutyTable${i}`] !== undefined
                                ? (data[`rpmMinDutyTable${i}`] / 100).toFixed(1)
                                : undefined;
                            const confirmed = incoming !== undefined && Math.abs(Number(incoming) - Number(p.value)) < 0.05;
                            if (confirmed || Date.now() >= p.deadlineMs) {
                                minDutyInput.classList.remove('table-input-pending');
                                pendingTableValues.delete(minDutyInput.id);
                                if (incoming !== undefined) minDutyInput.value = incoming;
                            }
                        } else if (data[`rpmMinDutyTable${i}`] !== undefined) {
                            minDutyInput.value = (data[`rpmMinDutyTable${i}`] / 100).toFixed(1);
                        }
                    }
                }

                if (pendingTableValues.size === 0 && !learningTableHasChanges) {
                    const learningStatus = document.getElementById('learning-table-status');
                    if (learningStatus && learningStatus.textContent === 'Saving...') {
                        setTableStatus('learning-table-status', 'Saved');
                        setTrackedTimeout(() => {
                            if (!learningTableHasChanges && pendingTableValues.size === 0) {
                                setTableStatus('learning-table-status', '');
                            }
                        }, 1200);
                    }
                }

                if (!window.learningTableInitialized) {
                    window.learningTableInitialized = true;
                    window.minDutyInitialized = true;
                    updateRPMRangeDisplays(data);
                }
            }

            // Update echos every cycle
            updateAllEchosOptimized(data);
            updateTogglesFromData(data);

            // Update PID stuff
            updateLearningTableHighlight(data);
            drawGlyphs();
            updatePidInitializedDisplay(data);


        }, false);

        source.addEventListener('TimestampData', function (e) {
            const raw = e.data.split(',').map(Number);

            const declaredCount = raw[0];
            const values = raw.slice(1);

            if (values.length !== declaredCount) {
                if (!window.lastTsWarnTime || Date.now() - window.lastTsWarnTime > 10000) {
                    console.warn(`[TimestampData] length mismatch: declared=${declaredCount}, actual=${values.length}`);
                    window.lastTsWarnTime = Date.now();
                }
                return;
            }
            if (declaredCount !== TS_FIELDS.length) {
                if (!window.lastTsWarnTime || Date.now() - window.lastTsWarnTime > 10000) {
                    console.warn(`[TimestampData] schema mismatch: ESP32=${declaredCount}, UI=${TS_FIELDS.length}`);
                    window.lastTsWarnTime = Date.now();
                }
                return;
            }

            const data = Object.fromEntries(TS_FIELDS.map((key, i) => [key, values[i]]));

            window.sensorAges = {
                heading: data.ts_HeadingNMEA,
                latitude: data.ts_LatitudeNMEA,
                longitude: data.ts_LongitudeNMEA,
                satellites: data.ts_SatelliteCount,
                victronVoltage: data.ts_VictronVoltage,
                victronCurrent: data.ts_VictronCurrent,
                alternatorTemp: data.ts_AlternatorTemp,
                thermistorTemp: data.ts_ThermistorTemp,
                rpm: data.ts_RPM,
                measuredAmps: data.ts_MeasuredAmps,
                batteryV: data.ts_BatteryV,
                ibv: data.ts_IBV,
                bcur: data.ts_Bcur,
                channel3V: data.ts_Channel3V,
                dutyCycle: data.ts_DutyCycle,
                fieldVolts: data.ts_FieldVolts,
                fieldAmps: data.ts_FieldAmps,
                cogNMEA: data.ts_CogNMEA,
                sogNMEA: data.ts_SogNMEA,
                appWindSpeed: data.ts_AppWindSpeed,
                appWindAngle: data.ts_AppWindAngle,
                trueWindSpeed: data.ts_TrueWindSpeed,
                trueWindAngle: data.ts_TrueWindAngle,
                leeway: data.ts_Leeway,
                vmg: data.ts_VMG,
                baroPressure: data.ts_BaroPressure,
                ambientTemp: data.ts_AmbientTemp,
                imu: data.ts_IMU,
                victronSolar: data.ts_VictronSolar,
                stwNMEA: data.ts_StwNMEA
            };
        }, false);



    };
    // Initial attach (subsequent attaches happen automatically inside initializeEventSource on reconnect).
    if (source) window.attachStreamListeners(source);




    // Improve legend appearance with clean lines and responsive spacing
    const style = document.createElement('style');
    style.textContent = `
.u-legend {
display: flex;
flex-wrap: wrap;
justify-content: center;
gap: 10px;
max-width: 100%;
box-sizing: border-box;
}

.u-legend .u-series {
display: flex;
align-items: center;
gap: 6px;
}

.u-legend .u-marker {
display: inline-block;
width: 18px;
height: 3px;
background-color: currentColor;
border-radius: 1px;
}

.plot-container {
padding: 10px;
box-sizing: border-box;
max-width: 1000px;     /* NEW: limit width on wide screens */
margin: 0 auto;       /* center it */
}

@media (max-width: 768px) {
.plot-container {
padding: 5px;
max-width: 100%;     /* allow full width on mobile */
}
}

`;
    document.head.appendChild(style);

    // Initialize toggle states on page load
    // Invert because labels are swapped
    document.getElementById("ManualFieldToggle_checkbox").checked = (document.getElementById("ManualFieldToggle").value === "0");
    document.getElementById("SwitchControlOverride_checkbox").checked = (document.getElementById("SwitchControlOverride").value === "1");
    //document.getElementById("OnOff_checkbox").checked = (document.getElementById("OnOff").value === "1");
    document.getElementById("LimpHome_checkbox").checked = (document.getElementById("LimpHome").value === "1");
    document.getElementById("VeData_checkbox").checked = (document.getElementById("VeData").value === "1");
    document.getElementById("NMEA0183Data_checkbox").checked = (document.getElementById("NMEA0183Data").value === "1");
    document.getElementById("NMEA2KData_checkbox").checked = (document.getElementById("NMEA2KData").value === "1");
    document.getElementById("IgnoreTemperature_checkbox").checked = (document.getElementById("IgnoreTemperature").value === "1");
    document.getElementById("IgnoreRPM_checkbox").checked = (document.getElementById("IgnoreRPM").value === "1");
    document.getElementById("bmsLogic_checkbox").checked = (document.getElementById("bmsLogic").value === "1");
    document.getElementById("bmsLogicLevelOff_checkbox").checked = (document.getElementById("bmsLogicLevelOff").value === "1");
    document.getElementById("AlarmActivate_checkbox").checked = (document.getElementById("AlarmActivate").value === "1");
    document.getElementById("InvertAltAmps_checkbox").checked = (document.getElementById("InvertAltAmps").value === "1");
    document.getElementById("InvertBattAmps_checkbox").checked = (document.getElementById("InvertBattAmps").value === "1");
    document.getElementById("IgnitionOverride_checkbox").checked = (document.getElementById("IgnitionOverride").value === "1");
    document.getElementById("TempSource_checkbox").checked = (document.getElementById("TempSource").value === "1");
    document.getElementById("admin_password").addEventListener("change", updatePasswordFields);
    document.getElementById("timeAxisModeChanging_checkbox").checked = (document.getElementById("timeAxisModeChanging").value === "1");
    document.getElementById("weatherModeEnabled_checkbox").checked = (document.getElementById("weatherModeEnabled").value === "1");
    document.getElementById("TuningMode_checkbox").checked = (document.getElementById("TuningMode").value === "1");
    document.getElementById("socInfoAvailable_checkbox").checked = (document.getElementById("socInfoAvailable").value === "1");
    document.getElementById("CloudFeatures_checkbox").checked = (document.getElementById("CloudFeatures").value === "1");
    document.getElementById("AutoAltCurrentZero_checkbox").checked = (document.getElementById("AutoAltCurrentZero").value === "1");
    document.getElementById("HardwarePresent_checkbox").checked = (document.getElementById("hardwarePresent").value === "1");

    // Mirror the segmented A/B controls to their hidden checkbox state on load
    ['ManualFieldToggle_checkbox', 'TempSource_checkbox', 'bmsLogicLevelOff_checkbox',
        'AlarmLatchEnabled_checkbox', 'timeAxisModeChanging_checkbox'].forEach(syncSegmented);

    setupInputValidation(); // Client side input validation of settings
    setupKWInputListeners();

    updateFloatVisibility();



});// <-- end of the window load event listener



// Function to handle Reset Learning Table button with confirmation
function handleResetLearningTable() {
    const confirmation = confirm(
        "⚠️ RESET LEARNING TABLE ⚠️\n\n" +
        "This will reset all learned and user-entered values to defaults.\n" +
        "Are you sure you want to continue?"
    );

    if (!confirmation) {
        return false;
    }

    // CRITICAL: Force re-initialization on next CSVData3
    window.learningTableInitialized = false;
    window.minDutyInitialized = false;
    window.resetLearningTableUI();
    return true; // Allow form submission to ESP32
}

function handleClearBuffer() {
    const confirmation = confirm(
        "⚠️ CLEAR UPLOAD BUFFER ⚠️\n\n" +
        "This will delete all queued sensor uploads waiting to sync to the cloud.\n\n" +
        "Use this if you have corrupted data or want to clear failed uploads.\n\n" +
        "Are you sure?"
    );
    return confirmation;
}

// Zero a set of dashboard fields AND invalidate both update caches so the next
// CSV frame is guaranteed to refresh them. Without the cache invalidation,
// if the next firmware value coincidentally matches what was cached pre-reset,
// scheduleDOMUpdateOptimized short-circuits and the DOM stays at '0' forever.
function resetDisplayValuesAndCaches(ids) {
    ids.forEach(id => {
        const el = document.getElementById(id);
        if (el) el.textContent = '0';
        lastWrittenValues.delete(id);
    });
    // lastValues is keyed `${elementId}_${key}` — drop any prefix match.
    for (const k of Array.from(lastValues.keys())) {
        for (const id of ids) {
            if (k.startsWith(id + '_')) { lastValues.delete(k); break; }
        }
    }
}

function handleResetPerfCounters() {
    if (!currentAdminPassword) {
        alert("Please unlock settings first");
        return;
    }

    const params = new URLSearchParams({ password: currentAdminPassword, ResetPerfCounters: '1' });
    fetchWithTimeout(buildURL('/get?' + params.toString()), {}, 8000)
        .then(() => {
            const ids = [
                'ft_ReadAnalogInputs_ses_ID', 'ft_rai_total_ses_ID', 'ft_rai_ina228_ses_ID',
                'ft_rai_ads_state_ses_ID', 'ft_rai_bmp_state_ses_ID', 'ft_rai_imu_ses_ID', 'ft_updateAccelMetrics_ses_ID',
                'VeTime2_ID', 'ft_AdjustFieldLearnMode_ses_ID', 'ft_uploadSensorHistory_ses_ID',
                'ft_dumpLongTermRing_ses_ID',
                'ft_uploadBufferedRecords_ses_ID', 'ft_buildConfigPayload_ses_ID',
                'cpuLoadCore0Max_display', 'cpuLoadCore1Max_display',
                'MaximumLoopTimeID',
                'ft_loop_win_ID', 'ft_loop_ses_ID',
                'ft_loopFieldOn_win_ID', 'ft_loopFieldOn_ses_ID',
                'ft_SendWifiData_win_ID', 'ft_SendWifiData_ses_ID',
                'ch1_worst_10s_ID', 'ch1_over2x_10s_ID', 'ch1_avg_10s_ID',
                'ch1_worst_2m_ID', 'ch1_over2x_2m_ID', 'ch1_avg_2m_ID',
                'ch1_worst_at_ID', 'ch1_over2x_at_ID', 'ch1_avg_at_ID',
                'ina_last_ms_ID', 'ina_avg_10s_ID', 'ina_worst_10s_ID', 'ina_over2x_10s_ID',
                'ina_avg_2m_ID', 'ina_worst_2m_ID', 'ina_over2x_2m_ID',
                'ina_avg_at_ID', 'ina_worst_at_ID', 'ina_over2x_at_ID',
                'vl_last_ms_ID', 'vl_avg_10s_ID', 'vl_worst_10s_ID', 'vl_over2x_10s_ID',
                'vl_avg_2m_ID', 'vl_worst_2m_ID', 'vl_over2x_2m_ID',
                'vl_avg_at_ID', 'vl_worst_at_ID', 'vl_over2x_at_ID'
            ];
            resetDisplayValuesAndCaches(ids);
        })
        .catch(err => diagError('Reset peaks failed:', err));
}

function handleResetAccelSession() {
    if (!currentAdminPassword) { alert("Please unlock settings first"); return; }
    const params = new URLSearchParams({ password: currentAdminPassword, ResetAccelSession: '1' });
    fetchWithTimeout(buildURL('/get?' + params.toString()), {}, 8000)
        .then(() => {
            const ids = [
                'imu_total_samples_accel_ID', 'imu_total_samples_gyro_ID',
                'imu_accel_dropped_ID', 'imu_gyro_dropped_ID',
                'imu_fifo_overrun_count_ID', 'imu_i2c_error_count_ID', 'imu_unknown_tag_count_ID',
                'imu_slam_count_ID'
            ];
            resetDisplayValuesAndCaches(ids);
        })
        .catch(err => diagError('Reset accel session failed:', err));
}

function handleResetAccelLifetime() {
    if (!currentAdminPassword) { alert("Please unlock settings first"); return; }
    if (!confirm("Reset ALL lifetime motion stats? This clears max heel/pitch, slam records, and capsize/pitchpole counts from the device's saved memory. Cannot be undone.")) return;
    const params = new URLSearchParams({ password: currentAdminPassword, ResetAccelLifetime: '1' });
    fetchWithTimeout(buildURL('/get?' + params.toString()), {}, 8000)
        .then(() => {
            const ids = [
                'imu_heel_max_lifetime_ID', 'imu_pitch_max_lifetime_ID',
                'imu_slam_peak_lifetime_ID', 'imu_slam_count_lifetime_ID',
                'imu_capsize_count_ID', 'imu_pitchpole_count_ID'
            ];
            resetDisplayValuesAndCaches(ids);
        })
        .catch(err => diagError('Reset accel lifetime failed:', err));
}

function handleClearOverheatHistory() {
    const confirmation = confirm(
        "⚠️ CLEAR OVERHEAT HISTORY ⚠️\n\n" +
        "This will reset all overheat counters, timestamps, and accumulated safe time to zero.\n\n"
    );
    return confirmation;
}

// Function to update learning table highlighting
function updateLearningTableHighlight(data) {
    // Remove all active row classes
    document.querySelectorAll('.learning-table-row-active').forEach(row => {
        row.classList.remove('learning-table-row-active');
    });
    document.querySelectorAll('.history-row-active').forEach(row => {
        row.classList.remove('history-row-active');
    });

    // Highlight 3 rows: lower RPM point, bucket, upper RPM point
    if (data.currentRPMTableIndex >= 0 && data.currentRPMTableIndex <= 8) {
        const allRows = document.querySelectorAll('#learning-table-body tr');
        const bucketIndex = data.currentRPMTableIndex;

        // Calculate row indices (accounting for history rows between data rows)
        const lowerDataRowIndex = bucketIndex * 2;
        const historyRowIndex = bucketIndex * 2 + 1;
        const upperDataRowIndex = (bucketIndex + 1) * 2;

        if (allRows[lowerDataRowIndex]) {
            allRows[lowerDataRowIndex].classList.add('learning-table-row-active');
        }
        if (allRows[historyRowIndex]) {
            allRows[historyRowIndex].classList.add('history-row-active');
        }
        if (allRows[upperDataRowIndex]) {
            allRows[upperDataRowIndex].classList.add('learning-table-row-active');
        }
    }
}

// Function to update PID initialized display
function updatePidInitializedDisplay(data) {
    const element = document.getElementById('pidInitialized_display');
    if (element && data.pidInitialized !== undefined) {
        element.textContent = data.pidInitialized === 1 ? 'Yes' : 'No';
        element.style.color = data.pidInitialized === 1 ? 'var(--accent)' : '#999';
    }
}

function showMainTab(tabName) {
    // Check if trying to access Cloud Features without unlocking
    if (tabName === 'cloudfeatures' && !currentAdminPassword) {
        alert('Please unlock settings first to access Cloud Features');
        return;
    }

    // Block all tabs except Settings until vessel info complete
    if (tabName !== 'settings' && !vesselInfoComplete) {
        // Only alert if user is actively trying to switch (not on initial load)
        if (document.getElementById(tabName)) {
            alert('Please complete Vessel Info in Settings tab first');
        }
        showMainTab('settings');
        showSubTab('settings', 'vessel-info');
        return;
    }

    // Hide all tab contents
    const tabContents = document.querySelectorAll('.tab-content');
    tabContents.forEach(tab => tab.classList.remove('active'));

    // Remove active class from all main tabs
    const mainTabs = document.querySelectorAll('.main-tab');
    mainTabs.forEach(tab => tab.classList.remove('active'));

    // Show selected tab content
    document.getElementById(tabName).classList.add('active');

    // Find and activate the correct button by looking for the one that calls this tab
    mainTabs.forEach(tab => {
        if (tab.getAttribute('onclick').includes(tabName)) {
            tab.classList.add('active');
        }
    });

    // Initialize My Profile when Cloud Features tab is opened
    if (tabName === 'cloudfeatures') {
        setTrackedTimeout(() => {
            if (typeof initializeProfileTab === 'function') {
                initializeProfileTab();
            }
        }, 100);
    }

    if (tabName === 'livedata') {
        // Ensure a sub-tab is always visible; on first visit none may be active yet
        if (!document.querySelector('#livedata .sub-tab-content.active')) {
            showSubTab('livedata', 'alternator');
        }
    }

    // Control sticky header based on active tab
    const header = document.querySelector('.permanent-header');
    if (header) {
        if (tabName === 'plots') {
            // Remove sticky on Plots main tab
            header.classList.remove('permanent-header-sticky');
        } else {
            // Add sticky for all other main tabs
            header.classList.add('permanent-header-sticky');
        }
    }
}


function showRegistrationRequiredModal(featureName) {
    const message = `Please complete Registration first to access ${featureName}.  If you've already registered, try waiting a few seconds for the Registration info to populate before switching subtabs`;
    alert(message);
}

function showSubTab(parentTab, subTabName, evt = null) {

    // Block all subtabs except Settings > Vessel Info until vessel info complete
    if (!vesselInfoComplete && !(parentTab === 'settings' && subTabName === 'vessel-info')) {
        alert('Please complete Vessel Info in Settings tab first');
        showMainTab('settings');
        showSubTab('settings', 'vessel-info');
        return;
    }

    // Block access to Cloud Features subtabs if not registered.
    // 'mydashboard' (Statistics) was relocated to Live Data → Statistics; content is local data so the gate no longer applies.
    if (parentTab === 'cloudfeatures' && subTabName !== 'myprofile' && !isDeviceRegistered) {
        const featureNames = {
            'leaderboards': 'Leaderboards',
            'fleetstats': 'Fleet Stats',
            'softwareupdate': 'Software Update'
        };
        showRegistrationRequiredModal(featureNames[subTabName] || 'this feature');
        return;
    }

    // Hide all sub-tab contents for this parent
    const subTabContents = document.querySelectorAll(`#${parentTab} .sub-tab-content`);
    subTabContents.forEach(tab => tab.classList.remove('active'));

    // Remove active class from all sub-tabs
    const subTabs = document.querySelectorAll(`#${parentTab} .sub-tab`);
    subTabs.forEach(tab => tab.classList.remove('active'));

    // Show selected sub-tab content
    const contentEl = document.getElementById(`${parentTab}-${subTabName}`);
    if (contentEl) {
        contentEl.classList.add('active');
    }

    // Add active class to clicked sub-tab, or find matching button if called programmatically
    if (evt && evt.target) {
        evt.target.classList.add('active');
    } else {
        subTabs.forEach(tab => {
            const onclick = tab.getAttribute('onclick') || '';
            if (onclick.includes(`showSubTab('${parentTab}', '${subTabName}'`) ||
                onclick.includes(`showSubTab("${parentTab}", "${subTabName}"`)) {
                tab.classList.add('active');
            }
        });
    }


    // Initialize / refresh the barometer panel whenever the Weather/Solar tab is opened (baro lives there now)
    if (parentTab === 'livedata' && subTabName === 'weathersolar') {
        if (typeof window.initBaroPanel === 'function') window.initBaroPanel();
    }

    // Resume the Fast Alt-Current scope only if its (collapsed-by-default) section is already expanded
    if (parentTab === 'livedata' && subTabName === 'diag') {
        const faDet = document.getElementById('fa-monitor-details');
        if (faDet && faDet.open && typeof fastDiagOnOpen === 'function') fastDiagOnOpen();
    }

    // Initialize profile tab when switching to My Profile
    if (parentTab === 'cloudfeatures' && subTabName === 'myprofile') {
        if (typeof initializeProfileTab === 'function') {
            initializeProfileTab();
        }
    }

    // Native Long Term Plots — lazy-init on first open of Plots → Long Term sub-tab.
    if (parentTab === 'plots' && subTabName === 'longterm' && typeof initLongTermPlots === 'function') {
        initLongTermPlots();
    }

    // Redirect to Vercel for Fleet Stats
    if (parentTab === 'cloudfeatures' && subTabName === 'fleetstats') {
        loadFleetStatsInIframe();
    }

    // Redirect to Vercel for Leaderboards
    if (parentTab === 'cloudfeatures' && subTabName === 'leaderboards') {
        loadLeaderboardsInIframe();
    }

    // Control sticky header for Cloud Features subtabs
    const header = document.querySelector('.permanent-header');
    if (header && parentTab === 'cloudfeatures') {
        header.classList.add('permanent-header-sticky');
    }

    updateFlushPillVisibility(parentTab, subTabName);
}

// Floating Cloud Upload pill — only visible on the cloud iframe sub-tabs.
const FLUSH_PILL_TABS = new Set(['leaderboards', 'fleetstats']);
function updateFlushPillVisibility(parentTab, subTabName) {
    const pill = document.getElementById('cloud-flush-pill');
    if (!pill) return;
    const visible = (parentTab === 'cloudfeatures' && FLUSH_PILL_TABS.has(subTabName));
    pill.style.display = visible ? 'block' : 'none';
    closeFlushPillConfirm();  // always reset to collapsed state on tab switch
}
function openFlushPillConfirm() {
    const c = document.getElementById('cloud-flush-pill-confirm');
    if (c) c.style.display = 'block';
}
function closeFlushPillConfirm() {
    const c = document.getElementById('cloud-flush-pill-confirm');
    if (c) c.style.display = 'none';
}

function showAltTab(group, panelId) {
    // Deactivate all panels and buttons in this group
    document.querySelectorAll('#settings-alternator .alt-panel-' + group)
        .forEach(p => p.classList.remove('active'));
    document.querySelectorAll('#settings-alternator .alt-tab-btn-' + group)
        .forEach(t => t.classList.remove('active'));
    // Show selected panel
    const panel = document.getElementById(panelId);
    if (panel) panel.classList.add('active');
    // Activate matching button
    document.querySelectorAll('#settings-alternator .alt-tab-btn-' + group)
        .forEach(t => {
            if ((t.getAttribute('onclick') || '').includes(panelId)) {
                t.classList.add('active');
            }
        });

}

// Frozen page response stuff
function showRecoveryOptions() {
    // Don't show multiple recovery dialogs.
    // querySelector (not getElementById) so the load-time MISSING-ELEMENT debug
    // shim doesn't false-flag this pre-creation existence check.
    if (document.querySelector('#recoveryDialog')) return;

    const recoveryDiv = document.createElement('div');
    recoveryDiv.id = 'recoveryDialog';
    recoveryDiv.innerHTML = `
<div style="position: fixed; top: 50%; left: 50%; transform: translate(-50%, -50%); 
            background: var(--card-light); padding: 20px; border-radius: var(--radius); 
            box-shadow: 0 4px 8px rgba(0,0,0,0.3); z-index: 10000; border: 2px solid var(--accent);">
  <h3 style="margin-top: 0; color: var(--text-dark);">Connection Lost</h3>
  <p style="color: var(--text-dark);">Lost connection to alternator regulator.</p>
  <button class="btn-primary" onclick="retryConnection()" style="margin-right: 10px;">Retry Connection</button>
  <button class="btn-secondary" onclick="enterOfflineMode()">Continue Offline</button>
  <p style="font-size: 12px; color: var(--text-dark);"><small>If problem persists, the device may need to be power-cycled.</small></p>
</div>
`;
    document.body.appendChild(recoveryDiv);
}
function retryConnection() {
    closeRecovery();
    location.reload();
}
function closeRecovery() {
    const dialog = document.getElementById('recoveryDialog');
    if (dialog) {
        dialog.remove();
    }
}
function enterOfflineMode() {
    closeRecovery();
    if (isOfflineMode) return;
    isOfflineMode = true;
    // Disable all form controls and buttons except reconnect and tab navigation
    document.querySelectorAll('input[type="submit"], button, input[type="checkbox"], input[type="number"], input[type="text"], input[type="password"], select').forEach(el => {
        // Skip the dark mode toggle, reconnect buttons, and tab navigation
        if (!el.id.includes('DarkMode') &&
            !el.onclick?.toString().includes('location.reload') &&
            !el.classList.contains('main-tab') &&
            !el.classList.contains('sub-tab')) {
            el.disabled = true;
            el.style.opacity = '0.5';
            el.style.cursor = 'not-allowed';
        }
    });
    // Disable toggles in offline mode
    document.querySelectorAll('.switch input').forEach(el => {
        el.disabled = true;
        el.style.opacity = '0.5';
    });
    // Update corner status to show offline
    const cornerStatus = document.getElementById('corner-status');
    if (cornerStatus) {
        cornerStatus.className = 'corner-status corner-status-disconnected';
        cornerStatus.textContent = 'OFFLINE MODE';
        cornerStatus.style.backgroundColor = '#ff6600';
    }
}

// Called from the SSE 'open' handler when a reconnect succeeds after the user
// had clicked "Continue Offline". Mirrors enterOfflineMode's selectors so the
// same set of elements is re-enabled. Corner status itself is reset by
// updateInlineStatus(true) which the 'open' handler already calls.
function exitOfflineMode() {
    if (!isOfflineMode) return;
    isOfflineMode = false;
    document.querySelectorAll('input[type="submit"], button, input[type="checkbox"], input[type="number"], input[type="text"], input[type="password"], select').forEach(el => {
        if (!el.id.includes('DarkMode') &&
            !el.onclick?.toString().includes('location.reload') &&
            !el.classList.contains('main-tab') &&
            !el.classList.contains('sub-tab')) {
            el.disabled = false;
            el.style.opacity = '';
            el.style.cursor = '';
        }
    });
    document.querySelectorAll('.switch input').forEach(el => {
        el.disabled = false;
        el.style.opacity = '';
    });
    // Override the inline backgroundColor set by enterOfflineMode so the CSS class
    // takes back over. updateInlineStatus(true) already flipped the class to connected.
    const cornerStatus = document.getElementById('corner-status');
    if (cornerStatus) {
        cornerStatus.style.backgroundColor = '';
    }
}


function validateSettings() {
    const validations = [
        {
            inputs: ['FloatVoltage', 'BulkVoltage'],
            check: (float, bulk) => !isNaN(float) && !isNaN(bulk) && float >= bulk,
            error: 'Float voltage must be less than bulk voltage'
        },
        {
            inputs: ['MinDuty', 'MaxDuty'],
            check: (min, max) => !isNaN(min) && !isNaN(max) && min >= max,
            error: 'Minimum duty cycle must be less than maximum duty cycle'
        }
    ];

    // Clear all previous error states
    document.querySelectorAll('.input-error').forEach(el => {
        el.classList.remove('input-error');
        el.title = '';
    });

    validations.forEach(validation => {
        const [input1Name, input2Name] = validation.inputs;
        const input1 = document.querySelector(`input[name="${input1Name}"]`);
        const input2 = document.querySelector(`input[name="${input2Name}"]`);

        if (!input1 || !input2) return;

        const value1 = input1.valueAsNumber;
        const value2 = input2.valueAsNumber;
        const button1 = input1.closest('form')?.querySelector('input[type="submit"]');
        const button2 = input2.closest('form')?.querySelector('input[type="submit"]');

        if (validation.check(value1, value2)) {
            // Add error styling
            input1.classList.add('input-error');
            input2.classList.add('input-error');
            input1.title = validation.error;
            input2.title = validation.error;

            // Disable buttons
            if (button1) {
                button1.disabled = true;
                button1.title = validation.error;
            }
            if (button2) {
                button2.disabled = true;
                button2.title = validation.error;
            }
        } else {
            // Re-enable buttons if no error
            if (button1) {
                button1.disabled = false;
                button1.title = '';
            }
            if (button2) {
                button2.disabled = false;
                button2.title = '';
            }
        }
    });
}


function validateFuelTable() {
    const errors = [];
    const rpmValues = [];
    const gphValues = [];

    // Collect all values
    for (let i = 0; i < 10; i++) {
        const rpmInput = document.getElementById(`fuelTableRPM${i}_input`);
        const gphInput = document.getElementById(`fuelTableGPH${i}_input`);

        if (rpmInput && gphInput) {
            rpmValues[i] = parseFloat(rpmInput.value) || 0;
            gphValues[i] = parseFloat(gphInput.value) || 0;
        }
    }

    // Check that RPM values are strictly increasing (ignoring trailing zeros)
    let lastNonZeroIndex = -1;
    for (let i = 9; i >= 0; i--) {
        if (rpmValues[i] !== 0) {
            lastNonZeroIndex = i;
            break;
        }
    }

    // Validate RPM increases up to last non-zero entry
    for (let i = 0; i <= lastNonZeroIndex; i++) {
        if (i > 0 && rpmValues[i] <= rpmValues[i - 1]) {
            errors.push(`RPM values must increase. Row ${i + 1} (${rpmValues[i]}) must be greater than Row ${i} (${rpmValues[i - 1]})`);
            break;
        }
    }

    // Check that zeros only appear at the end (but allow first row to be 0,0)
    let foundNonZero = false;
    for (let i = 9; i >= 1; i--) {  // Start at i=1 to skip first row
        if (rpmValues[i] !== 0) {
            foundNonZero = true;
        } else if (foundNonZero) {
            errors.push(`Zero RPM values are only allowed at the end of the table. Found zero at row ${i + 1}`);
            break;
        }
    }

    if (errors.length > 0) {
        alert('Fuel Table Validation Error:\n\n' + errors.join('\n'));
        return false;
    }

    return true;
}
function validateLearningTable() {
    const errors = [];
    const rpmValues = [];

    // Collect all RPM values
    for (let i = 0; i < 10; i++) {
        const rpmInput = document.getElementById(`rpmTableRPMPoints${i}_input`);
        if (rpmInput) {
            rpmValues[i] = parseFloat(rpmInput.value) || 0;
        }
    }

    // Find last non-zero index
    let lastNonZeroIndex = -1;
    for (let i = 9; i >= 0; i--) {
        if (rpmValues[i] !== 0) {
            lastNonZeroIndex = i;
            break;
        }
    }

    // Validate RPM strictly increases up to last non-zero entry
    for (let i = 0; i <= lastNonZeroIndex; i++) {
        if (i > 0 && rpmValues[i] <= rpmValues[i - 1]) {
            errors.push(`RPM values must increase. Row ${i + 1} (${rpmValues[i]}) must be greater than Row ${i} (${rpmValues[i - 1]})`);
            break;
        }
    }

    // Check that zeros only appear at the end
    let foundNonZero = false;
    for (let i = 9; i >= 0; i--) {
        if (rpmValues[i] !== 0) {
            foundNonZero = true;
        } else if (foundNonZero) {
            errors.push(`Zero RPM values are only allowed at the end of the table. Found zero at row ${i + 1}`);
            break;
        }
    }

    if (errors.length > 0) {
        alert('Learning Table Validation Error:\n\n' + errors.join('\n'));
        return false;
    }
    return true;
}

// Calculate and display RPM ranges below each breakpoint
function updateRPMRangeDisplays(data) {
    if (!document.querySelector('#rpm-range-0')) return;
    for (let i = 0; i < 10; i++) {
        const rangeSpan = document.querySelector(`#rpm-range-${i}`);
        if (rangeSpan) {
            if (i < 9) {
                const nextRPM = data[`rpmTableRPMPoints${i + 1}`];
                rangeSpan.textContent = nextRPM || '-';
            } else {
                rangeSpan.textContent = '∞';
            }
        }
    }
}
//  validation called in the window load event, for client side checking of input values
function setupInputValidation() {
    // Settings validation
    const settingsInputs = ["FloatVoltage", "BulkVoltage", "MinDuty", "MaxDuty"];

    settingsInputs.forEach(name => {
        const input = document.querySelector(`input[name="${name}"]`);
        if (input) {
            input.addEventListener("input", validateSettings);
            input.addEventListener("blur", validateSettings); // Also validate on blur
        } else {
            diagWarn(`Validation input not found: ${name}`);
        }
    });
    // Run initial validation
    setTrackedTimeout(() => {
        validateSettings();
    }, 100); // Small delay to ensure all data is loaded
}
//Dynamic adjustment factors
function resetDynamicShuntGain() {
    if (!currentAdminPassword) {
        alert("Please unlock settings first");
        return;
    }
    updatePasswordFields();

    const formData = new FormData();
    formData.append("password", currentAdminPassword);
    formData.append("ResetDynamicShuntGain", "1");

    fetchWithTimeout(buildURL("/get?" + new URLSearchParams(formData).toString()), {}, 8000)
        .then(() => {
            //   diagLog("SOC gain factor reset requested");
        })
        .catch(err => diagError("Reset failed:", err));
}

function resetDynamicAltZero() {
    if (!currentAdminPassword) {
        alert("Please unlock settings first");
        return;
    }
    updatePasswordFields();

    const formData = new FormData();
    formData.append("password", currentAdminPassword);
    formData.append("ResetDynamicAltZero", "1");

    fetchWithTimeout(buildURL("/get?" + new URLSearchParams(formData).toString()), {}, 8000)
        .then(() => {
            //  diagLog("Alternator zero offset reset requested");
        })
        .catch(err => diagError("Reset failed:", err));
}


// Small cloud-state badge on the Charging-System Health + Speed (boat performance) panels. Uses signals the
// dashboard already has — currentMode (AP=1) and the CloudFeatures toggle. Local-only states are grey;
// cloud-on is green. (A "synced N ago" upgrade would need a firmware sync-timestamp — not wired yet.)
function updateCloudStatus() {
    const ap = (window._lastKnownMode === 1);   // 1 = Access Point
    const cb = document.getElementById('CloudFeatures_checkbox');
    const on = cb ? !!cb.checked : false;
    const fmtAgo = (s) => {                       // seconds since last successful sync (this boot); <0 = none
        if (s == null || s < 0) return '';
        if (s < 90)   return ', synced just now';
        if (s < 5400) return ', synced ' + Math.round(s / 60) + 'm ago';
        return ', synced ' + Math.round(s / 3600) + 'h ago';
    };
    const set = (id, agoS) => {
        const el = document.getElementById(id);
        if (!el) return;
        if (ap)       { el.textContent = '· AP mode (local only)';   el.style.color = '#888'; }
        else if (!on) { el.textContent = '· cloud off (local only)'; el.style.color = '#888'; }
        else          { el.textContent = '· cloud on' + fmtAgo(agoS); el.style.color = '#5cb85c'; }
    };
    set('alt-cloud-status',  (typeof altLive  !== 'undefined') ? altLive.syncAgoS  : null);
    set('perf-cloud-status', (typeof perfLive !== 'undefined') ? perfLive.syncAgoS : null);
}

function updateCloudFeaturesTabVisibility(enabled) {
    const cloudTab = document.querySelector('.main-tab[onclick*="cloudfeatures"]');
    if (cloudTab) {
        cloudTab.style.display = enabled ? '' : 'none';

        // If hiding and currently on Cloud Features tab, switch to gauges
        if (!enabled) {
            const cloudTabContent = document.getElementById('cloudfeatures');
            if (cloudTabContent && cloudTabContent.classList.contains('active')) {
                showMainTab('gauges');
            }
        }
    }
}
function updateCloudFeaturesToggleState(currentMode) {
    const checkbox = document.getElementById("CloudFeatures_checkbox");
    const formRow = checkbox?.closest('.form-row');
    const isAPMode = (currentMode === 1); // 1 = Access Point mode

    if (checkbox) {
        if (isAPMode) {
            // Gray out and disable in AP mode
            checkbox.disabled = true;
            checkbox.checked = false;
            if (formRow) formRow.style.opacity = '0.5';

            // Also hide the tab if visible
            updateCloudFeaturesTabVisibility(false);
        } else {
            // Enable in Configuration or Client mode
            checkbox.disabled = false;
            if (formRow) formRow.style.opacity = '1';
        }
    }
}


// ============================================================================
// LEARNING TABLE - GLYPH SYSTEM & STICKY HEADER
// ============================================================================

// Draw the SVG glyphs connecting RPM ranges to history buckets
function drawGlyphs() {
    const wrap = document.getElementById('ltWrap');
    const svg = document.getElementById('glyphOverlay');
    if (!wrap || !svg) return;

    // Read ALL rects before any DOM write to avoid repeated forced reflows.
    // getBoundingClientRect after innerHTML='' forces a synchronous layout on
    // every call because the DOM is dirty — reading first keeps it one reflow.
    const br = wrap.getBoundingClientRect();
    const glyphTeal = getComputedStyle(document.documentElement).getPropertyValue('--glyph-teal');
    const glyphBlack = getComputedStyle(document.documentElement).getPropertyValue('--glyph-black');

    const buckets = [];
    for (let i = 0; i < 9; i++) {
        const aEl = document.getElementById('rpmTableRPMPoints' + i + '_input');
        const bEl = document.getElementById('rpmTableRPMPoints' + (i + 1) + '_input');
        const ohEl = document.getElementById('overheatCount' + i + '_display');
        if (!aEl || !bEl) { buckets.push(null); continue; }
        const ra = aEl.getBoundingClientRect();
        const rb = bEl.getBoundingClientRect();
        const roh = ohEl ? ohEl.getBoundingClientRect() : null;
        buckets.push({ ra, rb, roh });
    }

    // DOM writes happen after all reads — single reflow above, zero below
    svg.setAttribute('viewBox', `0 0 ${br.width} ${br.height}`);
    const defs = svg.querySelector('defs');
    svg.innerHTML = '';
    if (defs) svg.appendChild(defs);

    const ypad = 8;
    const w = 14;

    for (let i = 0; i < 9; i++) {
        const b = buckets[i];
        if (!b) continue;

        const y1 = (b.ra.top - br.top) - ypad;
        const y2 = (b.rb.bottom - br.top) + ypad;
        const mid = (y1 + y2) / 2;
        const x = (i % 2 === 0) ? 0 : 12;
        const cy = Math.max(16, (y2 - y1) * 0.28);
        const color = (i % 2 === 0) ? glyphTeal : glyphBlack;

        const p = document.createElementNS("http://www.w3.org/2000/svg", "path");
        const d = `M ${x + w} ${y1} Q ${x} ${y1 + cy} ${x + w} ${mid} Q ${x} ${y2 - cy} ${x + w} ${y2}`;
        p.setAttribute('d', d);
        p.setAttribute('fill', 'none');
        p.setAttribute('stroke-linecap', 'round');
        p.setAttribute('stroke-linejoin', 'round');
        p.setAttribute('stroke-width', '3.4');
        p.setAttribute('stroke', color);
        svg.appendChild(p);

        if (b.roh) {
            const tx = b.roh.left - br.left;
            const ty = (b.roh.top + b.roh.bottom) / 2 - br.top;

            const line = document.createElementNS("http://www.w3.org/2000/svg", "line");
            line.setAttribute('x1', x + w);
            line.setAttribute('y1', mid);
            line.setAttribute('x2', Math.max(tx - 10, (x + w) + 6));
            line.setAttribute('y2', ty);
            line.setAttribute('stroke-width', '2.6');
            line.setAttribute('stroke', color);
            line.setAttribute('marker-end', (i % 2 === 0) ? 'url(#arrowG)' : 'url(#arrowK)');
            svg.appendChild(line);
        }
    }
}

// Sticky header functionality
(function () {
    const table = document.getElementById('learning-table');
    const thead = table ? table.querySelector('thead') : null;
    const sticky = document.getElementById('stickyHeader');
    const stickyThead = document.querySelector('#stickyHeaderTable thead');
    const xscroller = document.getElementById('ltX');

    if (thead && sticky && stickyThead) {
        stickyThead.innerHTML = thead.innerHTML;
    }

    function syncHeader() {
        if (!table || !sticky) return;
        const r = table.getBoundingClientRect();
        const headerH = 44;
        const show = (r.top < 0) && (r.bottom > headerH);
        sticky.style.display = show ? 'block' : 'none';

        // Match widths by reading the real header cells
        const srcTh = table.querySelectorAll('thead th');
        const dstTh = document.querySelectorAll('#stickyHeaderTable thead th');
        if (srcTh.length === dstTh.length) {
            for (let i = 0; i < srcTh.length; i++) {
                const w = srcTh[i].getBoundingClientRect().width;
                dstTh[i].style.width = w + 'px';
            }
        }

        // Horizontal sync
        if (xscroller) {
            const sx = xscroller.scrollLeft || 0;
            const sht = document.getElementById('stickyHeaderTable');
            if (sht) sht.style.transform = 'translateX(' + (-sx) + 'px)';
        }
    }

    window.addEventListener('scroll', syncHeader, { passive: true });
    window.addEventListener('resize', syncHeader, { passive: true });
    if (xscroller) xscroller.addEventListener('scroll', syncHeader, { passive: true });
    setTrackedTimeout(syncHeader, 0);
})();

// Initialize glyphs
function initLearningGlyphs() {
    drawGlyphs();
}

// Call on page load
if (document.readyState === 'loading') {
    document.addEventListener('DOMContentLoaded', initLearningGlyphs);
} else {
    initLearningGlyphs();
}

// Initialize sticky header on page load
if (document.readyState === 'loading') {
    document.addEventListener('DOMContentLoaded', function () {
        const header = document.querySelector('.permanent-header');
        if (header) {
            // Default to sticky unless on plots tab
            header.classList.add('permanent-header-sticky');
        }
    });
} else {
    const header = document.querySelector('.permanent-header');
    if (header) {
        header.classList.add('permanent-header-sticky');
    }
}

// Redraw on window events
window.addEventListener('resize', drawGlyphs, { passive: true });
window.addEventListener('scroll', drawGlyphs, { passive: true });
const ltX = document.getElementById('ltX');
if (ltX) {
    ltX.addEventListener('scroll', drawGlyphs, { passive: true });
}

// ============================================================================
// END LEARNING TABLE GLYPH SYSTEM
// ============================================================================


function updateLifeIndicators(data) {
    const indicatorColors = ['#00a19a', '#ff9800', '#f44336']; // Green, Yellow, Red
    const colorNames = ['Good', 'Caution', 'Critical'];

    const indicators = [
        { id: 'insulation-indicator', percent: data.InsulationLifePercent },
        { id: 'grease-indicator', percent: data.GreaseLifePercent },
        { id: 'brush-indicator', percent: data.BrushLifePercent },
        { id: 'predicted-indicator', hours: data.PredictedLifeHours }
    ];

    indicators.forEach(indicator => {
        const element = document.getElementById(indicator.id);
        if (!element) return;

        let colorIndex = 0; // Default green

        if (indicator.percent !== undefined) {
            // For percentage indicators
            if (indicator.percent < 20) colorIndex = 2;      // Red below 20%
            else if (indicator.percent < 50) colorIndex = 1; // Yellow below 50%
        } else if (indicator.hours !== undefined) {
            // For predicted hours indicator
            if (indicator.hours < 1000) colorIndex = 2;      // Red below 1000 hours
            else if (indicator.hours < 5000) colorIndex = 1; // Yellow below 5000 hours
        }

        element.style.color = indicatorColors[colorIndex];
        element.textContent = colorNames[colorIndex];
    });
}


function handleManualFieldToggle(checkboxId) {
    const cb = document.getElementById(checkboxId);
    if (!cb) return false;

    // ESP32 convention is inverted: 0 = PID, 1 = Manual
    // So when checkbox is checked (PID), we send 0; unchecked (Manual), we send 1
    const invertedVal = cb.checked ? '0' : '1';

    const result = handleUserToggle(checkboxId, 'ManualFieldToggle', 'ManualFieldToggle');

    const pending = pendingToggles.get('ManualFieldToggle');
    if (pending) {
        pending.desiredValue = cb.checked ? 1 : 0;  // checked=PID=invertedManual=1
    }

    const hiddenInput = document.getElementById('ManualFieldToggle');
    if (hiddenInput) hiddenInput.value = invertedVal;

    return result;
}

// ===== Segmented A/B controls =====
// Render peer-choice settings (mode A vs mode B) as a segmented control instead of
// an on/off slider, reusing the existing hidden checkbox plumbing untouched. The
// visible buttons drive the hidden checkbox; firmware sync, pendingToggles, the
// onchange handler and form submit all still flow through that checkbox.
// data-seg-for names the hidden checkbox; each button's data-checked is the
// checked-state ("0"/"1") that button represents.
function syncSegmented(checkboxId) {
    const cb = document.getElementById(checkboxId);
    if (!cb) return;
    const wrap = document.querySelector('.ab-seg[data-seg-for="' + checkboxId + '"]');
    if (!wrap) return;
    wrap.querySelectorAll('.cap-mode-btn').forEach(b => {
        b.classList.toggle('cap-mode-active', (b.dataset.checked === '1') === cb.checked);
    });
}

function abSegClick(btn) {
    const wrap = btn.closest('.ab-seg');
    if (!wrap) return;
    const cbId = wrap.dataset.segFor;
    const cb = document.getElementById(cbId);
    if (!cb) return;
    const want = (btn.dataset.checked === '1');
    if (cb.checked !== want) {
        cb.checked = want;
        cb.dispatchEvent(new Event('change')); // runs the checkbox's existing onchange (handler + form submit)
    }
    syncSegmented(cbId);
}

// Segmented control over a hidden value input (no checkbox) for multi-value peer
// settings like BatteryCurrentSource. data-seg-name names the hidden input
// (id="<name>_val", name="<name>"); each button's data-val is the value to send.
function syncSegmentedSelect(name, val) {
    const wrap = document.querySelector('.ab-seg-select[data-seg-name="' + name + '"]');
    if (!wrap) return;
    wrap.querySelectorAll('.cap-mode-btn').forEach(b => {
        b.classList.toggle('cap-mode-active', b.dataset.val === String(val));
    });
}

function abSegSelectClick(btn) {
    const wrap = btn.closest('.ab-seg-select');
    if (!wrap) return;
    const name = wrap.dataset.segName;
    const hidden = document.getElementById(name + '_val');
    if (hidden) hidden.value = btn.dataset.val;
    syncSegmentedSelect(name, btn.dataset.val);
    const form = btn.closest('form');
    if (form) { updatePasswordFields(); form.submit(); }
}

// ==================== PID TUNING PLOT (SEPARATE SYSTEM) ====================

//Background text display stuff
function getEchoText(id, fallback = '?') {
    const el = document.getElementById(id);
    if (!el) return fallback;
    const t = (el.textContent || '').trim();
    return t.length ? t : fallback;
}

function drawPidWatermark(u) {
    const kp = getEchoText('PidKp_echo');
    const ki = getEchoText('PidKi_echo');
    const kd = getEchoText('PidKd_echo');
    const kb = getEchoText('PIDTrackingGain_echo');
    const riseRate = getEchoText('SetpointRiseRate_echo');
    const fallRate = getEchoText('SetpointFallRate_echo');
    const dutyRate = getEchoText('DutyRampRate_echo');
    const sampleTime = getEchoText('PidSampleDivisor_echo');
    const vLoopInt = getEchoText('VoltageLoopInterval_echo');
    const vKp = getEchoText('VoltageKp_echo');
    const wavePer = getEchoText('wavePeriod_echo');
    const waveAmp = getEchoText('waveAmplitude_echo');
    const rpm = typeof window._lastKnownRPM !== 'undefined' ? window._lastKnownRPM : '?';

    const col1 = [
        `Kp: ${kp}`,
        `Ki: ${ki}`,
        `Kd: ${kd}`,
        `kb: ${kb}`,
    ];

    const col2 = [
        `Rise: ${riseRate} A/s`,
        `Fall: ${fallRate} A/s`,
        `Duty: ${dutyRate} %/s`,
        `RPM: ${rpm}`,
    ];

    const col3 = [
        `PID: ${sampleTime} ms`,
        `VLoop: ${vLoopInt} ms`,
        `VKp: ${vKp} A/V`,
        `Wave: ${wavePer}s @ ${waveAmp}A`,
    ];

    const ctx = u.ctx;
    const x0 = u.bbox.left;
    const y0 = u.bbox.top;
    ctx.save();
    ctx.globalAlpha = 0.28;
    ctx.fillStyle = '#666';
    ctx.font = 'bold 18px monospace';
    ctx.textBaseline = 'top';
    ctx.shadowColor = 'rgba(255,255,255,0.6)';
    ctx.shadowBlur = 2;

    const lineH = 24;
    const colSpacing = 220;

    let x = x0 + 14;
    let y = y0 + 14;
    for (let i = 0; i < col1.length; i++) {
        ctx.fillText(col1[i], x, y);
        y += lineH;
    }

    x += colSpacing;
    y = y0 + 14;
    for (let i = 0; i < col2.length; i++) {
        ctx.fillText(col2[i], x, y);
        y += lineH;
    }

    x += colSpacing;
    y = y0 + 14;
    for (let i = 0; i < col3.length; i++) {
        ctx.fillText(col3[i], x, y);
        y += lineH;
    }

    ctx.restore();
}
// Global variables for PID tuning plot
let pidTuningPlot = null;
let pidTuningData = null;
let pidTuningResizeObserver = null;
let pidTuningIndex = 0;


// Series visibility state (persistent across redraws)
let pidTuningSeriesVisible = {
    setpointLimited: true,
    pidInput: true,
    iMeasFilt: true,
    uTargetAmps: true,
    dutyCycle: true,
    pidOutput: true,
    rpm: true
};

// Initialize PID tuning data structures
function initPidTuningDataStructures() {
    const intervalMs = window._lastKnownInterval || 200;
    let timeWindowSec = xTime;

    if (!timeWindowSec || timeWindowSec <= 0 || isNaN(timeWindowSec)) {
        diagError(`Invalid xTime: ${timeWindowSec}, using default 30s`);
        timeWindowSec = 30;
    }

    const timeWindowMs = timeWindowSec * 1000;
    const maxPoints = Math.ceil(timeWindowMs / intervalMs);

    if (maxPoints <= 0 || maxPoints > 10000 || isNaN(maxPoints)) {
        diagError(`Invalid maxPoints: ${maxPoints} (interval=${intervalMs}, window=${timeWindowSec}s)`);
        return;
    }

    const intervalSec = intervalMs / 1000;

    const xAxisData = [];
    for (let i = 0; i < maxPoints; i++) {
        xAxisData[i] = -(maxPoints - 1 - i) * intervalSec;
    }

    pidTuningData = [
        [...xAxisData],                      // [0] X-axis (seconds ago)
        new Array(maxPoints).fill(0),        // [1] setpointLimited
        new Array(maxPoints).fill(0),        // [2] pidInput (raw)
        new Array(maxPoints).fill(0),        // [3] iMeas_filt  ← new, shift everything down
        new Array(maxPoints).fill(0),        // [4] uTargetAmps
        new Array(maxPoints).fill(0),        // [5] dutyCycle
        new Array(maxPoints).fill(0),        // [6] pidOutput
        new Array(maxPoints).fill(0)         // [7] RPM / 100
    ];

    pidTuningIndex = 0;
    plotInterp.pid.arrivalTime = 0;
}
// Initialize PID tuning plot
// Manual override for the PID plot's right-hand duty axis (browser-side only;
// the left amps axis persists on the regulator as yyMin/yyMax).
let pidDutyRange = null;
try { pidDutyRange = JSON.parse(localStorage.getItem('pidDutyRange')) || null; } catch (e) { }

function initPidTuningPlot() {
    const plotEl = document.getElementById('pid-tuning-plot');
    if (!plotEl) {
        diagError("PID tuning plot element not found");
        return;
    }

    const opts = {
        width: Math.min(plotEl.clientWidth, 800),
        height: 400,
        series: [
            {},
            {
                label: "Setpoint (slewed)",
                stroke: pidTuningSeriesVisible.setpointLimited ? "#FF6B6B" : "transparent",
                width: 2,
                scale: "amps"
            },
            {
                label: "Measured Current (raw)",
                stroke: pidTuningSeriesVisible.pidInput ? "#4CAF50" : "transparent",
                width: 1,
                scale: "amps",
                dash: [4, 2]
            },
            {
                label: "Measured Current (filtered)",
                stroke: pidTuningSeriesVisible.iMeasFilt ? "#0D47A1" : "transparent",
                width: 2,
                scale: "amps"
            },
            {
                label: "Setpoint (raw)",
                stroke: pidTuningSeriesVisible.uTargetAmps ? "#FFA726" : "transparent",
                width: 1,
                scale: "amps",
                dash: [2, 2]
            },
            {
                label: "Duty Applied",
                stroke: pidTuningSeriesVisible.dutyCycle ? "#2196F3" : "transparent",
                width: 2,
                scale: "duty"
            },
            {
                label: "PID Output",
                stroke: pidTuningSeriesVisible.pidOutput ? "#9C27B0" : "transparent",
                width: 2,
                scale: "duty",
                dash: [6, 4]
            },
            {
                label: "RPM / 100",
                stroke: pidTuningSeriesVisible.rpm ? "#00BCD4" : "transparent",
                width: 1,
                scale: "amps"
            }
        ],
        scales: {
            x: {
                time: false,
                auto: false,
                range: [pidTuningData[0][0], pidTuningData[0][pidTuningData[0].length - 1]]
            },
            amps: {
                auto: false,
                range: () => [yyMin, yyMax]   // fn, not array: re-read each redraw so Y edits survive setData re-ranging
            },
            duty: {
                auto: false,
                range: () => pidDutyRange || [-25, 105]   // fn, not array: same reason
            }
        },
        axes: [
            {
                label: "Seconds Ago",
                grid: { show: true }
            },
            {
                scale: "amps",
                label: "Current (A) / RPM (/100)",
                grid: { show: true },
                side: 3,
                // always pinned to yyMin/yyMax (firmware-persisted) → corners always labeled
                splits: edgeLabeledSplits(() => true)
            },
            {
                scale: "duty",
                label: "Duty Cycle (%)",
                grid: { show: true },
                side: 1,
                // default [-25,105] is style padding, not a user range — nice ticks for it
                splits: edgeLabeledSplits(() => pidDutyRange != null)
            }
        ],
        legend: {
            show: false
        },
        plugins: [{
            hooks: {
                init: [
                    (u) => {
                        createPidTuningLegend();

                        const resizePlot = debounce(() => {
                            const plotEl = document.getElementById("pid-tuning-plot");
                            if (plotEl && pidTuningPlot) {
                                pidTuningPlot.setSize({ width: plotEl.clientWidth, height: 400 });
                            }
                        }, 1000);

                        if (pidTuningResizeObserver) {
                            pidTuningResizeObserver.disconnect();
                        }
                        pidTuningResizeObserver = new ResizeObserver(resizePlot);
                        pidTuningResizeObserver.observe(plotEl);
                    }
                ],
                drawClear: [
                    (u) => {
                        if (!u.root || u.root.offsetParent === null) return;
                        drawPidWatermark(u);
                    }
                ]
            }
        }]
    };

    pidTuningPlot = new uPlot(opts, pidTuningData, plotEl);
    if (document.body.classList.contains('dark-mode')) updateUplotTheme(pidTuningPlot);

    // Click-to-edit Y limits: amps axis saves to the regulator (yyMin/yyMax,
    // same as the form fields below), duty axis is a local override.
    attachYAxisEdit(pidTuningPlot, [
        {
            scale: 'amps', decimals: 0,
            apply: (mn, mx) => {
                yyMin = Math.round(mn); yyMax = Math.round(mx);
                pidTuningPlot.setScale('amps', { min: yyMin, max: yyMax });
                sendYAxisSetting({ yyMin: yyMin, yyMax: yyMax });
            }
        },
        {
            scale: 'duty', decimals: 0,
            apply: (mn, mx) => {
                pidDutyRange = [mn, mx];
                localStorage.setItem('pidDutyRange', JSON.stringify(pidDutyRange));
                pidTuningPlot.setScale('duty', { min: mn, max: mx });
            },
            auto: () => {
                pidDutyRange = null;
                localStorage.removeItem('pidDutyRange');
                pidTuningPlot.setScale('duty', { min: -25, max: 105 });
            }
        }
    ]);
}

// Custom legend with checkboxes for PID tuning plot
function createPidTuningLegend() {
    const plotContainer = document.getElementById('pid-tuning-plot');
    if (!plotContainer) return;

    const existingLegend = plotContainer.querySelector('.custom-legend');
    if (existingLegend) {
        existingLegend.remove();
    }

    const legendDiv = document.createElement('div');
    legendDiv.className = 'custom-legend';
    legendDiv.style.cssText = `
        display: flex;
        justify-content: center;
        gap: 15px;
        margin-top: 10px;
        flex-wrap: wrap;
    `;

    const legendItems = [
        { key: 'setpointLimited', label: 'Setpoint (slewed)', color: '#FF6B6B', seriesIdx: 1 },
        { key: 'pidInput', label: 'Measured Current (raw)', color: '#4CAF50', seriesIdx: 2 },
        { key: 'iMeasFilt', label: 'Measured Current (filtered)', color: '#0D47A1', seriesIdx: 3 },
        { key: 'uTargetAmps', label: 'Setpoint (raw)', color: '#FFA726', seriesIdx: 4 },
        { key: 'dutyCycle', label: 'Duty Applied', color: '#2196F3', seriesIdx: 5 },
        { key: 'pidOutput', label: 'PID Output', color: '#9C27B0', seriesIdx: 6 },
        { key: 'rpm', label: 'RPM / 100', color: '#00BCD4', seriesIdx: 7 },
    ];

    legendItems.forEach(item => {
        const legendItem = document.createElement('label');
        legendItem.style.cssText = `
            display: flex;
            align-items: center;
            gap: 6px;
            font-size: 12px;
            cursor: pointer;
            user-select: none;
        `;

        const checkbox = document.createElement('input');
        checkbox.type = 'checkbox';
        checkbox.checked = pidTuningSeriesVisible[item.key];
        checkbox.style.cssText = `
            cursor: pointer;
            margin: 0;
        `;

        checkbox.addEventListener('change', () => {
            pidTuningSeriesVisible[item.key] = checkbox.checked;
            togglePidTuningSeries(item.seriesIdx, item.color, checkbox.checked);
        });

        const colorBox = document.createElement('div');
        colorBox.style.cssText = `
            width: 16px;
            height: 3px;
            background-color: ${item.color};
            border-radius: 1px;
            opacity: ${pidTuningSeriesVisible[item.key] ? 1 : 0.3};
        `;

        const span = document.createElement('span');
        span.textContent = item.label;
        span.style.cssText = `
            color: var(--text-dark);
            opacity: ${pidTuningSeriesVisible[item.key] ? 1 : 0.5};
        `;

        legendItem.appendChild(checkbox);
        legendItem.appendChild(colorBox);
        legendItem.appendChild(span);
        legendDiv.appendChild(legendItem);
    });

    plotContainer.appendChild(legendDiv);
}

// Toggle series visibility
function togglePidTuningSeries(seriesIdx, color, visible) {
    if (!pidTuningPlot) {
        return;
    }
    // Use show property instead of stroke
    pidTuningPlot.setSeries(seriesIdx, {
        show: visible
    });
    // Update legend visual appearance
    const legendItems = document.querySelectorAll('.custom-legend label');
    const itemIndex = seriesIdx - 1;

    if (legendItems[itemIndex]) {
        const colorBox = legendItems[itemIndex].querySelector('div');
        const label = legendItems[itemIndex].querySelector('span');
        if (colorBox) colorBox.style.opacity = visible ? 1 : 0.3;
        if (label) label.style.opacity = visible ? 1 : 0.5;
    }
}

// Plot update batching for PID tuning
let pidTuningUpdateScheduled = false;

function queuePidTuningPlotUpdate() {
    if (pidTuningUpdateScheduled) return;

    pidTuningUpdateScheduled = true;
    requestAnimationFrame(() => {
        if (pidTuningPlot && pidTuningData) {
            pidTuningPlot.setData(pidTuningData);
        }
        pidTuningUpdateScheduled = false;
    });
}

// Update PID tuning configuration when parameters change
function updatePidTuningConfiguration(data) {
    let axisChanged = false;
    let timeChanged = false;

    // Check for Y-axis range changes
    if (data.yyMin !== undefined && data.yyMin !== yyMin) {
        yyMin = data.yyMin;
        axisChanged = true;
    }
    if (data.yyMax !== undefined && data.yyMax !== yyMax) {
        yyMax = data.yyMax;
        axisChanged = true;
    }

    // Check for time window changes
    if (data.xTime !== undefined && !isNaN(data.xTime) && data.xTime > 0 && data.xTime !== xTime) {
        xTime = data.xTime;
        timeChanged = true;
    }

    if (timeChanged) {
        // Buffer length depends on xTime — full rebuild required
        if (pidTuningPlot) {
            pidTuningPlot.destroy();
        }
        initPidTuningDataStructures();
        initPidTuningPlot();
    } else if (axisChanged && pidTuningPlot) {
        // Re-range in place — recreate caused a page jump on every Y edit echo
        pidTuningPlot.setScale('amps', { min: yyMin, max: yyMax });
    }
}

//IMU
function updateIMUAlignmentDisplayFromData(data) {
    const gx = Number(data.imu_accel_x_raw);
    const gy = Number(data.imu_accel_y_raw);
    const gz = Number(data.imu_accel_z_raw);

    const gravityVectorEl = document.getElementById("imu_gravity_vector_display");
    const statusEl = document.getElementById("imu_alignment_status_display");
    if (!gravityVectorEl || !statusEl) return;

    // Handle invalid/missing data
    if (!Number.isFinite(gx) || !Number.isFinite(gy) || !Number.isFinite(gz)) {
        gravityVectorEl.innerHTML = `
            <td>--</td>
            <td>--</td>
            <td>--</td>
        `;
        statusEl.textContent = "Waiting for sensor data...";
        statusEl.style.borderLeftColor = "#ffa726";
        statusEl.style.color = "#666666";
        return;
    }

    // Calculate total magnitude and tilt
    const totalG = Math.sqrt(gx * gx + gy * gy + gz * gz);
    const deviation = Math.abs(totalG - 1.0);
    const tiltDegrees = Math.asin(Math.min(deviation, 1.0)) * 180 / Math.PI;

    // Determine status based on tilt
    let color, statusText, borderColor;
    if (tiltDegrees < 5) {
        color = "#00a19a";
        borderColor = "#00a19a";
        statusText = `Good alignment (${tiltDegrees.toFixed(1)}° tilt)`;
    } else if (tiltDegrees < 10) {
        color = "#ff9800";
        borderColor = "#ff9800";
        statusText = `Acceptable alignment (${tiltDegrees.toFixed(1)}° tilt)`;
    } else {
        color = "#d32f2f";
        borderColor = "#d32f2f";
        statusText = `Poor alignment (${tiltDegrees.toFixed(1)}° tilt) - remount squarely`;
    }

    // Update table cells
    gravityVectorEl.innerHTML = `
        <td style="color: ${color};">${gx.toFixed(2)}</td>
        <td style="color: ${color};">${gy.toFixed(2)}</td>
        <td style="color: ${color};">${gz.toFixed(2)}</td>
    `;

    // Update status message
    statusEl.textContent = statusText;
    statusEl.style.borderLeftColor = borderColor;
    statusEl.style.color = "#333333";
}


// Sync dual-control parameters between tabs
function syncDualControlParameters(batteryAh, solarWatts) {
    const vesselForm = document.getElementById('vessel-info-form');
    if (vesselForm) {
        if (batteryAh !== undefined) vesselForm.BATTERY_CAPACITY_AH.value = batteryAh;
        if (solarWatts !== undefined) vesselForm.SOLAR_WATTS.value = solarWatts;
    }

    // Update cache
    if (window.vesselInfo) {
        if (batteryAh !== undefined) window.vesselInfo.battery_capacity_ah = batteryAh;
        if (solarWatts !== undefined) window.vesselInfo.solar_watts = solarWatts;
    }
}


// Intercept dual-control parameter submissions
function interceptDualControlSubmissions() {
    // Find all forms that submit BatteryCapacity_Ah or SolarWatts
    document.querySelectorAll('form[action="/get"]').forEach(form => {
        const batteryInput = form.querySelector('input[name="BatteryCapacity_Ah"]');
        const solarInput = form.querySelector('input[name="SolarWatts"]');

        if (batteryInput || solarInput) {
            form.addEventListener('submit', function (e) {
                // Let form submit normally (to ESP32)
                // But ALSO update our cache and other tab
                setTimeout(() => {
                    if (batteryInput) {
                        const value = parseInt(batteryInput.value);
                        syncDualControlParameters(value, undefined);
                    }
                    if (solarInput) {
                        const value = parseInt(solarInput.value);
                        syncDualControlParameters(undefined, value);
                    }
                }, 100); // Small delay to let form submit
            });
        }
    });
}

function populateYearDropdown() {
    const yearSelect = document.querySelector('[name="BOAT_YEAR"]');
    if (yearSelect) {
        // Range: 1900 floor (no real recreational boats older), current year + 5
        // ceiling so the dropdown is still valid for the next 5 calendar years
        // without a code change. Descending order puts the most-likely selections
        // near the top. Default selection is the current calendar year.
        const currentYear = new Date().getFullYear();
        for (let year = currentYear + 5; year >= 1900; year--) {
            const option = document.createElement('option');
            option.value = year;
            option.textContent = year;
            if (year === currentYear) {
                option.selected = true;
            }
            yearSelect.appendChild(option);
        }
    }
}

// Orientation card click handling
document.querySelectorAll('.pri-orientation-card').forEach(card => {
    card.addEventListener('click', function () {
        const radio = this.querySelector('input[type="radio"]');
        radio.checked = true;
    });
});

// TODO: Update this validation logic based on actual IMU axis conventions
// For now, just checking that values are reasonably close to 0 or 1
function validateIMUAlignment(x, y, z) {
    const tolerance = 0.2;

    // Check if each value is close to 0 or close to ±1
    const isValid = (val) => {
        return (Math.abs(val) < tolerance) ||
            (Math.abs(Math.abs(val) - 1.0) < tolerance);
    };

    return isValid(x) && isValid(y) && isValid(z);
}

function formatMinutesToDHM(minutes) {
    if (isNaN(minutes) || minutes === null || minutes === undefined) {
        return '-';
    }

    // Handle negative values (shouldn't happen, but just in case)
    if (minutes < 0) {
        return '-';
    }

    const days = Math.floor(minutes / 1440);  // 1440 minutes in a day
    const hours = Math.floor((minutes % 1440) / 60);
    const mins = Math.floor(minutes % 60);

    let parts = [];
    if (days > 0) parts.push(`${days}d`);
    if (hours > 0) parts.push(`${hours}h`);
    if (mins > 0 || parts.length === 0) parts.push(`${mins}m`);

    return parts.join(' ');
}

// Auto-scale a seconds count into the most readable two-unit form.
// < 60s → "45s", < 60m → "12m 34s", < 24h → "3h 45m", otherwise → "2d 3h".
function formatSecondsAuto(seconds) {
    if (isNaN(seconds) || seconds === null || seconds === undefined || seconds < 0) return '-';
    const s = Math.floor(seconds);
    if (s < 60)    return `${s}s`;
    if (s < 3600)  return `${Math.floor(s / 60)}m ${s % 60}s`;
    if (s < 86400) return `${Math.floor(s / 3600)}h ${Math.floor((s % 3600) / 60)}m`;
    return `${Math.floor(s / 86400)}d ${Math.floor((s % 86400) / 3600)}h`;
}


function getLogTimestamp() {
    const now = new Date();
    const pad = n => String(n).padStart(2, '0');
    return `${now.getFullYear()}${pad(now.getMonth() + 1)}${pad(now.getDate())}_${pad(now.getHours())}${pad(now.getMinutes())}`;
}

// ── Matrix stats summary (Live Data → Alternator card) ────────────────────





function downloadLogs() {
    const ts = getLogTimestamp();
    const files = [
        { href: '/thermallog.csv', name: `thermallog_${ts}.csv` },
        { href: '/pidlog.csv', name: `pidlog_${ts}.csv` },
    ];
    files.forEach(f => {
        const a = document.createElement('a');
        a.href = f.href;
        a.download = f.name;
        document.body.appendChild(a);
        a.click();
        document.body.removeChild(a);
    });

    // CV log is binary→CSV converted client-side
    downloadCvLog();

    setTimeout(() => {
        fetch(buildURL('/resetlogs'), { method: 'POST' })
            .catch(err => console.warn('Log reset failed:', err));
    }, 5000);
}


function resetLogs() {
    fetch(buildURL('/resetlogs'), { method: 'POST' })
        .catch(() => { });
}

// Stop/Start Logs — freeze or resume the thermal/PID/CV ring buffers on the device.
function stopLogs() {
    fetch(buildURL('/stoplogs'), { method: 'POST' })
        .catch(() => { });
}
function startLogs() {
    fetch(buildURL('/startlogs'), { method: 'POST' })
        .catch(() => { });
}

// Reflect live logging state (CSV2 slot 439) in the single pause/resume toggle —
// its dot carries the state (pulsing green = recording, amber = paused), the label names the action.
function updateLoggingStatus(loggingActive) {
    const toggle = document.getElementById('logToggleBtn');
    g_loggingActive = Number(loggingActive);  // remembered for the log-relay capture-state stamp
    if (!toggle) return;
    const on = Number(loggingActive) === 1;
    toggle.className = 'btn-log-toggle ' + (on ? 'is-recording' : 'is-paused');
    toggle.innerHTML = '<span class="log-dot"></span>' + (on ? '⏸ Pause Logs' : '▶ Resume Logs');
    toggle.dataset.recording = on ? '1' : '0';   // remembered so toggleLogs() knows which call to make
}

// Single button toggles pause/resume based on the last-known state from updateLoggingStatus().
function toggleLogs() {
    const toggle = document.getElementById('logToggleBtn');
    if (toggle && toggle.dataset.recording === '1') { stopLogs(); } else { startLogs(); }
}

// ── Silent cloud log relay ────────────────────────────────────────────────
// Fulfils an admin "pull" window. The admin opens a window by flagging the
// device's cloud row (SQL). On its next poll this app fetches all logs from the
// device over LAN, bundles + gzips them, and uploads to a private Supabase
// Storage bucket via a signed URL. Fully silent, gated on cloud registration.
// A failed upload leaves the window open → retried on the next poll.
let g_cloudToken = null;
let g_logRelayBusy = false;
let g_loggingActive = 1;  // last-known Stop/Start state (1=recording, 0=user-paused); stamped into bundles

// Adaptive poll cadence: 60s baseline while the app is open, ramps to 10s once a
// log request is seen, and stays fast until 1hr passes with no new request. Lets a
// rare support session (5-6 back-to-back pulls) feel snappy without 24/7 fast polling.
const LOG_POLL_SLOW_MS = 60000;           // baseline cadence
const LOG_POLL_FAST_MS = 10000;           // burst cadence after a request is seen
const LOG_POLL_FAST_WINDOW_MS = 3600000;  // stay fast until 1hr with no new request
let g_logLastRequestSeenMs = 0;           // Date.now() of last pending request seen
let g_logPollTimer = null;                // dedicated timer (not in activeTimers — self-reschedules)

// Schedule the next poll at the adaptive cadence. Dedicated timer ref so the
// self-rescheduling chain doesn't bloat activeTimers; cleared in cleanupResources.
function scheduleNextLogPoll() {
    const fast = g_logLastRequestSeenMs
        && (Date.now() - g_logLastRequestSeenMs) < LOG_POLL_FAST_WINDOW_MS;
    g_logPollTimer = setTimeout(pollLogRequest, fast ? LOG_POLL_FAST_MS : LOG_POLL_SLOW_MS);
}

function cloudHeaders() {
    return {
        'Content-Type': 'application/json',
        'Authorization': 'Bearer ' + SUPABASE_ANON_KEY,
        'apikey': SUPABASE_ANON_KEY,
    };
}

async function ensureCloudToken() {
    if (g_cloudToken) return g_cloudToken;
    const resp = await fetchWithTimeout(buildURL('/getAuthToken'), {}, 8000);
    if (!resp.ok) return null;
    const data = await resp.json();
    if (!data.registered || !data.token) return null;  // not registered → feature off
    g_cloudToken = data.token;
    return g_cloudToken;
}

// Chunked base64 — avoids call-stack blowups on large (~300 KB) binary buffers.
function arrayBufferToBase64(buf) {
    const bytes = new Uint8Array(buf);
    let binary = '';
    const chunk = 0x8000;
    for (let i = 0; i < bytes.length; i += chunk) {
        binary += String.fromCharCode.apply(null, bytes.subarray(i, i + chunk));
    }
    return btoa(binary);
}

async function fetchLogText(path) {
    try {
        const r = await fetchWithTimeout(buildURL(path), {}, 20000);
        return r.ok ? await r.text() : null;
    } catch (e) { return null; }
}
async function fetchLogJson(path) {
    try {
        const r = await fetchWithTimeout(buildURL(path), {}, 20000);
        return r.ok ? await r.json() : null;
    } catch (e) { return null; }
}
async function fetchLogBase64(path) {
    try {
        const r = await fetchWithTimeout(buildURL(path), {}, 20000);
        if (!r.ok) return null;
        const buf = await r.arrayBuffer();
        return (buf && buf.byteLength) ? arrayBufferToBase64(buf) : null;
    } catch (e) { return null; }
}

async function pollLogRequest() {
    if (g_logRelayBusy) return;
    try {
        const token = await ensureCloudToken();
        if (!token) return;

        const checkResp = await fetch(`${SUPABASE_URL}/functions/v1/check-log-request`, {
            method: 'POST', headers: cloudHeaders(), body: JSON.stringify({ token })
        });
        if (checkResp.status === 401) { g_cloudToken = null; return; }  // refresh token next poll
        if (!checkResp.ok) return;
        const check = await checkResp.json();
        if (!check.pending) return;
        g_logLastRequestSeenMs = Date.now();  // mark activity → keep fast cadence for 1hr
        const requestId = check.request_id;

        g_logRelayBusy = true;

        // Fetch every log from the device over LAN (tolerate individual failures).
        const [thermal, pid, cvB64, systemid, tuning, cvtuning, thermaltuning, famatrix, fascopeB64, faflipB64] = await Promise.all([
            fetchLogText('/thermallog.csv'),
            fetchLogText('/pidlog.csv'),
            fetchLogBase64('/cvlog.bin'),
            fetchLogJson('/systemidlog'),
            fetchLogJson('/tuninglog'),
            fetchLogJson('/cvtuninglog'),
            fetchLogJson('/thermaltuninglog'),
            fetchLogText('/famatrix.csv'),    // Resonance & Ripple Map (alt-current disturbance matrix) rides along
            fetchLogBase64('/fastscope.bin'), // Live Oscilloscope capture (raw FSC1 blob, like cvlog)
            fetchLogBase64('/faflip.bin'),    // Reference Flipbook — reference pages + anomaly captures (FFLP blob)
        ]);

        const logsIncluded = [];
        const logs = {};
        if (thermal != null) { logs.thermal = thermal; logsIncluded.push('thermal'); }
        if (pid != null) { logs.pid = pid; logsIncluded.push('pid'); }
        if (cvB64 != null) { logs.cv_base64 = cvB64; logsIncluded.push('cv'); }
        if (systemid != null) { logs.systemid = systemid; logsIncluded.push('systemid'); }
        if (tuning != null) { logs.tuning = tuning; logsIncluded.push('tuning'); }
        if (cvtuning != null) { logs.cvtuning = cvtuning; logsIncluded.push('cvtuning'); }
        if (thermaltuning != null) { logs.thermaltuning = thermaltuning; logsIncluded.push('thermaltuning'); }
        if (famatrix != null) { logs.famatrix = famatrix; logsIncluded.push('famatrix'); }
        if (fascopeB64 != null) { logs.fastscope_base64 = fascopeB64; logsIncluded.push('fastscope'); }
        if (faflipB64 != null) { logs.faflip_base64 = faflipB64; logsIncluded.push('faflip'); }

        const deviceUid = ((document.getElementById('profile-device-uid') || {}).textContent
            || (document.getElementById('systemDeviceUID_ID') || {}).textContent || 'unknown').trim();
        const fwVersion = ((document.getElementById('current-version-display') || {}).textContent || '').trim();
        const tsLabel = getLogTimestamp();

        // CompressionStream is absent on iOS WKWebView < 16.4 → uncompressed fallback.
        const canGzip = (typeof CompressionStream !== 'undefined');

        const bundle = {
            meta: {
                device_uid: deviceUid,
                firmware_version: fwVersion,
                submitted_at: new Date().toISOString(),
                timestamp_label: tsLabel,
                app_source: IS_CAPACITOR ? 'capacitor' : 'browser',
                logs_included: logsIncluded,
                compressed: canGzip,
                logging_paused: (Number(g_loggingActive) !== 1),  // true = user froze logs (deliberate capture); false = live rolling window
            },
            config_echo: g_lastCsv3 || null,
            logs: logs,
        };
        const jsonStr = JSON.stringify(bundle);

        let blob, ext;
        if (canGzip) {
            const cs = new Blob([jsonStr]).stream().pipeThrough(new CompressionStream('gzip'));
            blob = await new Response(cs).blob();
            ext = 'json.gz';
        } else {
            blob = new Blob([jsonStr], { type: 'application/json' });
            ext = 'json';
        }
        const filename = `${deviceUid}_${tsLabel}.${ext}`;

        // Phase 1 — signed upload URL.
        const urlResp = await fetch(`${SUPABASE_URL}/functions/v1/submit-logs`, {
            method: 'POST', headers: cloudHeaders(),
            body: JSON.stringify({ phase: 'url', token, request_id: requestId, filename })
        });
        if (!urlResp.ok) return;
        const urlData = await urlResp.json();
        if (!urlData.success || !urlData.upload_url) return;

        // Upload. On failure, leave the window open so the next poll retries.
        const putResp = await fetch(urlData.upload_url, {
            method: 'PUT', body: blob,
            headers: { 'Content-Type': canGzip ? 'application/gzip' : 'application/json' }
        });
        if (!putResp.ok) { console.warn('Log upload PUT failed:', putResp.status); return; }

        // Phase 2 — confirm: records metadata + clears the window.
        await fetch(`${SUPABASE_URL}/functions/v1/submit-logs`, {
            method: 'POST', headers: cloudHeaders(),
            body: JSON.stringify({
                phase: 'confirm', token, request_id: requestId,
                storage_path: urlData.storage_path, byte_size: blob.size,
                logs_included: logsIncluded, firmware_version: fwVersion, compressed: canGzip
            })
        });
        console.log(`Log bundle submitted (${logsIncluded.length} logs, ${blob.size} bytes).`);
    } catch (err) {
        console.warn('Log relay error (will retry):', err);
    } finally {
        g_logRelayBusy = false;
        scheduleNextLogPoll();  // self-rescheduling chain at the adaptive cadence
    }
}

// First run after SSE settles; pollLogRequest reschedules itself adaptively thereafter.
document.addEventListener('DOMContentLoaded', () => {
    g_logPollTimer = setTimeout(pollLogRequest, 8000);
});
// ── Voltage Mode Greyout (unchanged) ─────────────────────────
// stage: 1=bulk, 2=absorption, 3=float, 4=manual, 5=maintain, 6=target voltage, 7=idle, 0/other=off

function updateVoltageModeGreyout(stage) {
    const isCVMode = (stage === 2 || stage === 3 || stage === 6);
    document.querySelectorAll('[data-mode="cv"]').forEach(el => {
        el.classList.toggle('mode-dimmed', !isCVMode);
    });
}





// ==================== THERMAL LOG PLOTS ====================
let _thermalStateArrays = {
    flagsArr: [], antiWindupArr: [], stageArr: [], tArr: []
};
// Index 1 is retired (old middle plot deleted) — slots: 0 temp/penalty, 2 PID terms, 3 mode strip
let thermalLogPlots = [null, null, null, null];
let thermalLogResizeObservers = [null, null, null, null];
let thermalWindowMin = 30;

// Legend checkbox visibility state (session only, not persisted)
let thermalSeriesVisible = {
    tempFilt: true, tempProjected: true, tempSetpoint: true,
    penaltyAmps: true, measAmps: true, uTarget: true,
    outerP: true, lookahead: true, outerI: true,
};

// ---------------------------------------------------------------------------
// Window buttons
// ---------------------------------------------------------------------------
function highlightThermalWindowBtn(minutes) {
    [5, 10, 30, 60].forEach(v => {
        const btn = document.getElementById(`tw-${v}`);
        if (btn) btn.classList.toggle('btn-primary', v === minutes);
        if (btn) btn.classList.toggle('btn-secondary', v !== minutes);
    });
}

function setThermalWindow(minutes) {
    thermalWindowMin = minutes;
    highlightThermalWindowBtn(minutes);
    // Destroy so each plot re-creates with the new x-range; re-render from the live ring.
    thermalLogPlots.forEach((p, i) => { if (p) { p.destroy(); thermalLogPlots[i] = null; } });
    thermalRenderAll();
}

function resetThermalZoom() {
    thermalLogPlots.forEach(p => {
        if (p) p.setScale('x', { min: -thermalWindowMin, max: 0 });
    });
}

// ---------------------------------------------------------------------------
// Watermark
// ---------------------------------------------------------------------------
function drawThermalWatermark(u) {
    const lines = [
        `Kp: ${getEchoText('TempPIDKp_echo')}   Ki: ${getEchoText('TempPIDKi_echo')}`,
        `Lookahead: ${getEchoText('ThermalLookaheadSec_echo')}s   Interval: ${getEchoText('TempPIDIntervalMs_echo')}ms`
    ];
    const ctx = u.ctx;
    ctx.save();
    ctx.globalAlpha = 0.28;
    ctx.fillStyle = '#666';
    ctx.font = 'bold 18px monospace';
    ctx.textBaseline = 'top';
    ctx.textAlign = 'left';
    ctx.shadowColor = 'rgba(255,255,255,0.6)';
    ctx.shadowBlur = 2;
    let y = u.bbox.top + 14;
    for (const line of lines) {
        ctx.fillText(line, u.bbox.left + 14, y);
        y += 24;
    }
    ctx.restore();
}

// Direction bands for the (sign-flipped) PID decomposition plot: teal above zero =
// more amps allowed, red below = amps being cut. Tints clamp at the zero crossing so
// an autoscaled view that excludes zero tints as one regime; the corner labels are
// pinned to the plot box (not the zero line) because the direction holds at any zoom.
function drawThermalDirectionBands(u) {
    const ctx = u.ctx;
    const { left, top, width, height } = u.bbox;
    const bot = top + height;
    const y0 = Math.max(top, Math.min(bot, u.valToPos(0, 'amps', true)));
    ctx.save();
    ctx.fillStyle = 'rgba(0,161,154,0.06)';
    ctx.fillRect(left, top, width, y0 - top);
    ctx.fillStyle = 'rgba(214,39,40,0.06)';
    ctx.fillRect(left, y0, width, bot - y0);
    ctx.globalAlpha = 0.4;
    ctx.fillStyle = '#666';
    ctx.font = 'bold 18px monospace';
    ctx.textAlign = 'right';
    ctx.shadowColor = 'rgba(255,255,255,0.6)';
    ctx.shadowBlur = 2;
    ctx.textBaseline = 'top';
    ctx.fillText('↑ MORE AMPS', left + width - 14, top + 14);
    ctx.textBaseline = 'bottom';
    ctx.fillText('↓ LESS AMPS', left + width - 14, bot - 14);
    ctx.restore();
}

// ---------------------------------------------------------------------------
// Fetch
// ---------------------------------------------------------------------------
// Sampled off the live CSV stream (replaces the old /thermallog.bin pull). One
// column per THERMAL_SAMPLE_MS, driven by the CSV2 frame (~5s) so it can never
// need a manual refresh. measAmps/uTarget ride CSV1 (cached every 10Hz frame);
// the rest ride CSV2. Anti-windup is OR-accumulated across frames so
// sub-sample CV-bleed events still show as ticks.
const THERMAL_SAMPLE_MS = 5000;          // one column per 5s (~720 cols/hr ≈ 1 pt/px on an 800px plot)
const THERMAL_MAX_WINDOW_MIN = 60;       // longest selectable window
const THERMAL_RING_CAP = 900;            // hard cap (>720 for cadence jitter headroom)

let _thermalRing = {
    ts: [], tempFilt: [], tempProjected: [], tempSetpoint: [], penalty: [],
    measAmps: [], uTarget: [], outerP: [], outerI: [], lookahead: [],
    flags: [], antiWindup: [], stage: []
};
let _thermalCsv1 = { measAmps: 0, uTarget: 0 };   // latest CSV1 values (10Hz)
let _thermalAntiWindupAccum = 0;                  // OR-accumulated between samples
let _thermalLastSampleMs = 0;

// Called from the CSV1 frame handler — cache the two fast series the plot needs.
function thermalLiveOnCsv1(data) {
    if ('MeasuredAmps' in data) _thermalCsv1.measAmps = Number(data.MeasuredAmps) / 100;
    if ('uTargetAmps' in data) _thermalCsv1.uTarget = Number(data.uTargetAmps) / 100;
}

// Called from the CSV2 frame handler (~5s). Accumulates the anti-windup latch
// across frames, then appends one ring column per THERMAL_SAMPLE_MS.
function thermalLiveOnCsv2(data) {
    if (Number(data.outerAntiWindupFired)) _thermalAntiWindupAccum = 1;

    const now = Date.now();
    if (now - _thermalLastSampleMs < THERMAL_SAMPLE_MS) return;
    _thermalLastSampleMs = now;

    const r = _thermalRing;
    r.ts.push(now);
    r.tempFilt.push(Number(data.tempFiltered) / 100);
    r.tempProjected.push(Number(data.tempPIDInput_d) / 100);
    r.tempSetpoint.push(Number(data.tempPIDSetpoint_d) / 100);
    r.penalty.push(Number(data.thermalPenaltyAmps) / 100);
    r.measAmps.push(_thermalCsv1.measAmps);
    r.uTarget.push(_thermalCsv1.uTarget);
    r.outerP.push(Number(data.outerTermP) / 100);
    r.outerI.push(Number(data.outerTermI) / 100);
    r.lookahead.push(Number(data.outerTermLookahead) / 100);
    r.flags.push(Number(data.thermalFlags) | 0);
    r.antiWindup.push(_thermalAntiWindupAccum);
    r.stage.push(Number(data.chargeStageDisplay) | 0);
    _thermalAntiWindupAccum = 0;

    // Trim: drop columns older than the max window (+ slack), then hard-cap length.
    const cutoff = now - (THERMAL_MAX_WINDOW_MIN * 60000 + THERMAL_SAMPLE_MS);
    while (r.ts.length && r.ts[0] < cutoff) { for (const k in r) r[k].shift(); }
    while (r.ts.length > THERMAL_RING_CAP) { for (const k in r) r[k].shift(); }

    // Only touch the DOM/charts when the section is actually open.
    const det = document.getElementById('thermallog-details');
    if (det && det.open) thermalRenderAll();
}

// Build the uPlot data arrays from the ring (x = minutes ago) and render all 3.
function thermalRenderAll() {
    const r = _thermalRing;
    const n = r.ts.length;
    if (n === 0) return;
    const now = Date.now();
    const t = new Array(n);
    for (let i = 0; i < n; i++) {
        t[i] = -(now - r.ts[i]) / 60000;          // minutes ago (ascending, oldest first)
    }
    renderThermalPlot1([t, r.tempFilt, r.tempProjected, r.tempSetpoint, r.penalty, r.measAmps, r.uTarget], t[0]);
    // State strip gets snapshot copies so a resize-redraw can't index a grown ring against a stale time axis.
    renderThermalPlotState([t, new Array(n).fill(null)], t[0], r.flags.slice(), r.antiWindup.slice(), r.stage.slice(), t);
    // Display-layer sign flip: firmware terms are penalty-signed (+ = cut amps); the plot
    // shows effect on the current target (+ = more amps). thermallog.csv keeps penalty sign.
    // The firmware P term includes the look-ahead share; subtracting it out makes the three
    // plotted series additive: present-temp + look-ahead + integrator = total penalty.
    const flip = a => a.map(v => v == null ? null : -v);
    const pPresent = r.outerP.map((v, i) => v == null ? null : v - (r.lookahead[i] || 0));
    renderThermalPlot2([t, flip(pPresent), flip(r.lookahead), flip(r.outerI)], t[0]);
}

// ---------------------------------------------------------------------------
// Zoom plugin
// ---------------------------------------------------------------------------
const thermalZoomPlugin = {
    hooks: {
        setScale: [(u, key) => {
            if (key !== 'x') return;
            const min = u.scales.x.min;
            const max = u.scales.x.max;
            thermalLogPlots.forEach(p => {
                if (p && p !== u) p.setScale('x', { min, max });
            });
        }]
    }
};

// ---------------------------------------------------------------------------
// Mode decode
// ---------------------------------------------------------------------------
// Tableau 10 — one hue per stage, red reserved for Shutdown alone (old palette
// had three warm bands: shutdown red, absorption orange, manual magenta).
// antiWindup is the tick marker; it sits on the background strip above the
// bands, so it's drawn theme-aware (near-black on light, white on dark)
// rather than from this value.
const THERMAL_MODE_COLORS = {
    shutdown: '#d62728',    // red — alarm
    bulk: '#1f77b4',        // blue
    absorption: '#ff7f0e',  // orange
    float: '#2ca02c',       // green
    maintain: '#17becf',    // cyan
    targetV: '#9467bd',     // purple
    manual: '#e377c2',      // pink
    idle: '#7f7f7f',        // gray
    antiWindup: '#111111'   // tick (light mode); dark mode overrides to white
};
function modeFromStage(stage, flags) {
    if (flags & (1 << 5)) {
        return { label: 'Shutdown', color: THERMAL_MODE_COLORS.shutdown };
    }

    switch (stage) {
        case 1: return { label: 'Bulk', color: THERMAL_MODE_COLORS.bulk };
        case 2: return { label: 'Absorption', color: THERMAL_MODE_COLORS.absorption };
        case 3: return { label: 'Float', color: THERMAL_MODE_COLORS.float };
        case 4: return { label: 'Manual', color: THERMAL_MODE_COLORS.manual };
        case 5: return { label: 'Maintain', color: THERMAL_MODE_COLORS.maintain };
        case 6: return { label: 'Target V', color: THERMAL_MODE_COLORS.targetV };
        case 7: return { label: 'Idle', color: THERMAL_MODE_COLORS.idle };
        default: return { label: '', color: 'transparent' };
    }
}

// ---------------------------------------------------------------------------
// Resize helper
// ---------------------------------------------------------------------------
function _thermalResizeObserver(plotIdx, elId, h) {
    if (thermalLogResizeObservers[plotIdx])
        thermalLogResizeObservers[plotIdx].disconnect();
    const el = document.getElementById(elId);
    if (!el) return;
    const obs = new ResizeObserver(debounce(() => {
        const p = thermalLogPlots[plotIdx];
        const e = document.getElementById(elId);
        if (p && e) p.setSize({ width: e.clientWidth, height: h });
    }, 1000));
    obs.observe(el);
    thermalLogResizeObservers[plotIdx] = obs;
}

// ---------------------------------------------------------------------------
// Plot 1 
// ---------------------------------------------------------------------------
function renderThermalPlot1(data, tMin) {
    if (thermalLogPlots[0]) {
        if (data.every(d => d !== undefined)) thermalLogPlots[0].setData(data);
        return;
    }
    const elId = 'thermallog-plot1';
    const el = document.getElementById(elId);
    if (!el) return;

    const H = 300;
    const opts = {
        width: el.clientWidth || 800, height: H,
        cursor: { drag: { x: true, y: false, setScale: true } },
        select: { show: true },
        series: [
            { label: null },
            // Tableau 10 palette — one warm color per plot (filtered temp = the
            // headline signal gets red); setpoint is a neutral gray reference line.
            {
                label: 'Temp Filtered (°F)', stroke: '#d62728', width: 2,
                scale: 'temp',
                show: thermalSeriesVisible.tempFilt !== false
            },
            {
                label: 'Temp Projected (°F)', stroke: '#e377c2', width: 1.5,
                scale: 'temp', dash: [4, 3],
                show: thermalSeriesVisible.tempProjected !== false
            },
            {
                label: 'Setpoint (°F)', stroke: '#7f7f7f', width: 1.5,
                scale: 'temp', dash: [8, 4],
                show: thermalSeriesVisible.tempSetpoint !== false
            },
            {
                label: 'Penalty Amps (A)', stroke: '#2ca02c', width: 1.5,
                scale: 'amps',
                show: thermalSeriesVisible.penaltyAmps !== false
            },
            {
                label: 'Measured Amps (A)', stroke: '#1f77b4', width: 1.5,
                scale: 'amps',
                show: thermalSeriesVisible.measAmps !== false
            },
            {
                label: 'U Target (A)', stroke: '#9467bd', width: 2,
                scale: 'amps', dash: [4, 3],
                show: thermalSeriesVisible.uTarget !== false
            }
        ],
        scales: {
            x: { time: false, auto: false, range: [-thermalWindowMin, 0] },
            temp: { auto: true },
            amps: { auto: true }
        },
        axes: [
            { label: 'Minutes Ago', grid: { show: true } },
            { scale: 'temp', label: 'Temperature (°F)', side: 3, grid: { show: true },
              splits: edgeLabeledSplits(thermalManualRange('p1-temp-min', 'p1-temp-max')) },
            { scale: 'amps', label: 'Amps (A)', side: 1, grid: { show: false },
              splits: edgeLabeledSplits(thermalManualRange('p1-amps-min', 'p1-amps-max')) }
        ],
        legend: { show: false },
        plugins: [
            {
                hooks: {
                    init: [(u) => _thermalResizeObserver(0, elId, H)],
                    drawClear: [(u) => { if (u.root?.offsetParent) drawThermalWatermark(u); }]
                }
            },
            thermalZoomPlugin
        ]
    };
    thermalLogPlots[0] = new uPlot(opts, data, el);
    if (document.body.classList.contains('dark-mode')) updateUplotTheme(thermalLogPlots[0]);
    _createThermalLegend(el, 0, [
        { key: 'tempFilt',      label: 'Temp Filtered',    color: '#d62728', idx: 1 },
        { key: 'tempProjected', label: 'Temp Projected',   color: '#e377c2', idx: 2 },
        { key: 'tempSetpoint',  label: 'Setpoint',         color: '#7f7f7f', idx: 3 },
        { key: 'penaltyAmps',   label: 'Penalty Amps',     color: '#2ca02c', idx: 4 },
        { key: 'measAmps',      label: 'Measured Amps',    color: '#1f77b4', idx: 5 },
        { key: 'uTarget',       label: 'U Target',         color: '#9467bd', idx: 6 }
    ]);
    requestAnimationFrame(() => {
        if (thermalLogPlots[0] && el.clientWidth > 0)
            thermalLogPlots[0].setSize({ width: el.clientWidth, height: H });
    });

    // Click-to-edit Y limits (clear both boxes + Enter returns to autoscale)
    attachYAxisEdit(thermalLogPlots[0], [
        { scale: 'temp', decimals: 0, apply: (mn, mx) => setThermalManualRange('p1-temp-min', 'p1-temp-max', mn, mx), auto: thermalRangesToAuto },
        { scale: 'amps', decimals: 0, apply: (mn, mx) => setThermalManualRange('p1-amps-min', 'p1-amps-max', mn, mx), auto: thermalRangesToAuto }
    ]);
}

// ---------------------------------------------------------------------------
// Plot 2 
// ---------------------------------------------------------------------------
function renderThermalPlot2(data, tMin) {
    if (thermalLogPlots[2]) {
        if (data.every(d => d !== undefined)) thermalLogPlots[2].setData(data);
        return;
    }
    const elId = 'thermallog-plot2';
    const el = document.getElementById(elId);
    if (!el) return;

    const H = 300;
    const opts = {
        width: el.clientWidth || 800, height: H,
        cursor: { drag: { x: true, y: false, setScale: true } },
        select: { show: true },
        series: [
            { label: null },
            // Additive decomposition (display-flipped, see updateThermalPlots):
            // present-temp + look-ahead + integrator = total penalty.
            { label: 'Present Temp (P)', stroke: '#1f77b4', width: 1.5, scale: 'amps' },
            { label: 'Look-Ahead', stroke: '#2ca02c', width: 1.5, scale: 'amps' },
            { label: 'Integrator (I)', stroke: '#ff7f0e', width: 1.5, scale: 'amps' }
        ],
        scales: {
            x: { time: false, auto: false, range: [-thermalWindowMin, 0] },
            amps: { auto: true }
        },
        axes: [
            { label: 'Minutes Ago', grid: { show: true } },
            { scale: 'amps', label: 'Effect on Current Target (A)', side: 3, grid: { show: true },
              splits: edgeLabeledSplits(thermalManualRange('p3-min', 'p3-max')) }
        ],
        legend: { show: false },
        plugins: [
            {
                hooks: {
                    init: [(u) => _thermalResizeObserver(2, elId, H)],
                    drawClear: [(u) => { if (u.root?.offsetParent) { drawThermalDirectionBands(u); drawThermalWatermark(u); } }]
                }
            },
            thermalZoomPlugin
        ]
    };
    thermalLogPlots[2] = new uPlot(opts, data, el);
    if (document.body.classList.contains('dark-mode')) updateUplotTheme(thermalLogPlots[2]);
    _createThermalLegend(el, 2, [
        { key: 'outerP',    label: 'Present Temp (P)', color: '#1f77b4', idx: 1 },
        { key: 'lookahead', label: 'Look-Ahead',       color: '#2ca02c', idx: 2 },
        { key: 'outerI',    label: 'Integrator (I)',   color: '#ff7f0e', idx: 3 },
    ]);
    requestAnimationFrame(() => {
        if (thermalLogPlots[2] && el.clientWidth > 0)
            thermalLogPlots[2].setSize({ width: el.clientWidth, height: H });
    });

    // Click-to-edit Y limits (clear both boxes + Enter returns to autoscale)
    attachYAxisEdit(thermalLogPlots[2], [
        { scale: 'amps', decimals: 0, apply: (mn, mx) => setThermalManualRange('p3-min', 'p3-max', mn, mx), auto: thermalRangesToAuto }
    ]);
}

// ---------------------------------------------------------------------------
// State strip
// ---------------------------------------------------------------------------
function renderThermalPlotState(data, tMin, flagsArr, antiWindupArr, stageArr, tArr) {
    // Always update BEFORE the early return — this is the fix
    _thermalStateArrays.flagsArr = flagsArr;
    _thermalStateArrays.antiWindupArr = antiWindupArr;
    _thermalStateArrays.stageArr = stageArr;
    _thermalStateArrays.tArr = tArr;

    if (thermalLogPlots[3]) {
        thermalLogPlots[3].setData(data);
        // setData already triggers a full redraw; redundant redraw() removed
        return;
    }

    const elId = 'thermallog-plot-state';
    const el = document.getElementById(elId);
    if (!el) return;

    const H = 120;
    const opts = {
        width: el.clientWidth || 800,
        height: H,
        cursor: { drag: { x: true, y: false, setScale: true } },
        select: { show: true },
        series: [
            { label: null },
            { label: null, stroke: 'transparent', width: 0, points: { show: false } }
        ],
        scales: {
            x: { time: false, auto: false, range: [-thermalWindowMin, 0] },
            y: { auto: false, range: [0, 1] }
        },
        axes: [
            { label: 'Minutes Ago', grid: { show: false } },
            { show: false }
        ],
        legend: { show: false },
        plugins: [
            {
                hooks: {
                    init: [(u) => _thermalResizeObserver(3, elId, H)],
                    drawClear: [(u) => {
                        // Destructure fresh arrays every draw — never use closure copies
                        const { flagsArr, antiWindupArr, stageArr, tArr } = _thermalStateArrays;
                        if (!tArr || tArr.length === 0) return;

                        const ctx = u.ctx;
                        const plotLeft = u.bbox.left;
                        const plotTop = u.bbox.top;
                        const plotWidth = u.bbox.width;
                        const plotHeight = u.bbox.height;

                        const barTop = plotTop + plotHeight * 0.12;
                        const barH = plotHeight * 0.56;
                        const antiTop = plotTop;
                        const antiBottom = barTop - 4;

                        ctx.save();
                        // Clip to plot area — prevents drawing over axes when
                        // data extends outside the current window
                        ctx.beginPath();
                        ctx.rect(plotLeft, plotTop, plotWidth, plotHeight);
                        ctx.clip();

                        let segStart = 0;
                        let segFlags = flagsArr[0];
                        let segStage = stageArr[0];

                        for (let i = 1; i <= flagsArr.length; i++) {
                            const last = (i === flagsArr.length);

                            if (last || flagsArr[i] !== segFlags || stageArr[i] !== segStage) {
                                const x0 = u.valToPos(tArr[segStart], 'x', true);
                                const x1 = last
                                    ? (plotLeft + plotWidth)
                                    : u.valToPos(tArr[i], 'x', true);

                                ctx.globalAlpha = 0.88;
                                ctx.fillStyle = modeFromStage(segStage, segFlags).color;
                                ctx.fillRect(x0, barTop, Math.max(1, x1 - x0), barH);

                                segStart = i;
                                if (!last) {
                                    segFlags = flagsArr[i];
                                    segStage = stageArr[i];
                                }
                            }
                        }

                        ctx.globalAlpha = 0.95;
                        // Ticks sit on the background strip above the bands — contrast with the theme, not a band color.
                        ctx.strokeStyle = document.body.classList.contains('dark-mode') ? '#ffffff' : THERMAL_MODE_COLORS.antiWindup;
                        ctx.lineWidth = 1.5;

                        for (let i = 0; i < antiWindupArr.length; i++) {
                            if (antiWindupArr[i]) {
                                const x = u.valToPos(tArr[i], 'x', true);
                                ctx.beginPath();
                                ctx.moveTo(x, antiTop);
                                ctx.lineTo(x, antiBottom);
                                ctx.stroke();
                            }
                        }

                        ctx.restore();
                    }]
                }
            },
            thermalZoomPlugin
        ]
    };

    thermalLogPlots[3] = new uPlot(opts, data, el);
    if (document.body.classList.contains('dark-mode')) updateUplotTheme(thermalLogPlots[3]);

    const existing = el.querySelector('.custom-legend');
    if (existing) existing.remove();
    const legend = document.createElement('div');
    legend.className = 'custom-legend';
    legend.style.cssText = 'display:flex;justify-content:center;gap:15px;margin-top:6px;flex-wrap:wrap;';
    [
        { label: 'Shutdown', color: THERMAL_MODE_COLORS.shutdown },
        { label: 'Bulk', color: THERMAL_MODE_COLORS.bulk },
        { label: 'Absorption', color: THERMAL_MODE_COLORS.absorption },
        { label: 'Float', color: THERMAL_MODE_COLORS.float },
        { label: 'Maintain', color: THERMAL_MODE_COLORS.maintain },
        { label: 'Target V', color: THERMAL_MODE_COLORS.targetV },
        { label: 'Manual', color: THERMAL_MODE_COLORS.manual },
        { label: 'Idle', color: THERMAL_MODE_COLORS.idle },
        { label: 'Anti-Windup Fired', color: THERMAL_MODE_COLORS.antiWindup, tick: true }
    ].forEach(item => {
        const wrap = document.createElement('div');
        wrap.style.cssText = 'display:flex;align-items:center;gap:5px;font-size:12px;';
        const swatch = document.createElement('div');
        swatch.style.cssText = item.tick
            ? `width:2px;height:14px;background:var(--text-dark);border-radius:1px;`   // tick adapts to theme like the on-plot marker
            : `width:14px;height:10px;background:${item.color};border-radius:2px;opacity:0.85;`;
        const span = document.createElement('span');
        span.textContent = item.label;
        span.style.cssText = 'color:var(--text-dark);';
        wrap.appendChild(swatch);
        wrap.appendChild(span);
        legend.appendChild(wrap);
    });
    el.appendChild(legend);

    requestAnimationFrame(() => {
        if (thermalLogPlots[3] && el.clientWidth > 0)
            thermalLogPlots[3].setSize({ width: el.clientWidth, height: H });
    });
}

// ---------------------------------------------------------------------------
// Shared legend builder
// ---------------------------------------------------------------------------
function _createThermalLegend(container, plotIdx, items) {
    const existing = container.querySelector('.custom-legend');
    if (existing) existing.remove();

    const div = document.createElement('div');
    div.className = 'custom-legend';
    div.style.cssText = 'display:flex;justify-content:center;gap:15px;margin-top:6px;flex-wrap:wrap;';

    items.forEach(item => {
        const lbl = document.createElement('label');
        lbl.style.cssText = 'display:flex;align-items:center;gap:6px;font-size:12px;cursor:pointer;user-select:none;';

        const cb = document.createElement('input');
        cb.type = 'checkbox';
        cb.checked = thermalSeriesVisible[item.key] !== false;
        cb.style.cssText = 'cursor:pointer;margin:0;';

        const bar = document.createElement('div');
        bar.style.cssText = `width:16px;height:3px;background:${item.color};border-radius:1px;`;

        const span = document.createElement('span');
        span.textContent = item.label;
        span.style.cssText = 'color:var(--text-dark);';

        // Apply initial opacity to match default visibility
        bar.style.opacity = cb.checked ? '1' : '0.3';
        span.style.opacity = cb.checked ? '1' : '0.5';

        cb.addEventListener('change', () => {
            thermalSeriesVisible[item.key] = cb.checked;
            thermalLogPlots[plotIdx]?.setSeries(item.idx, { show: cb.checked });
            bar.style.opacity = cb.checked ? '1' : '0.3';
            span.style.opacity = cb.checked ? '1' : '0.5';
        });

        lbl.appendChild(cb);
        lbl.appendChild(bar);
        lbl.appendChild(span);
        div.appendChild(lbl);
    });

    container.appendChild(div);
}

// ---------------------------------------------------------------------------
// Y axis range controls
// ---------------------------------------------------------------------------
// On-plot Y boxes write into the existing manual-range fields, switch the
// thermal autoscale checkbox off, and re-apply — so both UIs stay in agreement.
function setThermalManualRange(minId, maxId, mn, mx) {
    const minEl = document.getElementById(minId), maxEl = document.getElementById(maxId);
    if (minEl) minEl.value = mn;
    if (maxEl) maxEl.value = mx;
    const cb = document.getElementById('thermal-autoscale');
    if (cb && cb.checked) { cb.checked = false; cb.dispatchEvent(new Event('change')); }
    else applyThermalRanges();
}

function thermalRangesToAuto() {
    const cb = document.getElementById('thermal-autoscale');
    if (cb && !cb.checked) { cb.checked = true; cb.dispatchEvent(new Event('change')); }
}

// Manual-range predicate for edgeLabeledSplits: a thermal scale is pinned when
// the autoscale checkbox is off AND its min/max fields actually hold numbers.
function thermalManualRange(minId, maxId) {
    return () => {
        if (document.getElementById('thermal-autoscale')?.checked ?? true) return false;
        return isFinite(parseFloat(document.getElementById(minId)?.value))
            && isFinite(parseFloat(document.getElementById(maxId)?.value));
    };
}

function applyThermalRanges() {
    const auto = document.getElementById('thermal-autoscale')?.checked ?? true;
    const get = (id) => parseFloat(document.getElementById(id)?.value);

    if (thermalLogPlots[0]) {
        thermalLogPlots[0].scales.temp.range = auto
            ? (u, min, max) => [min, max]
            : () => [get('p1-temp-min'), get('p1-temp-max')];
        thermalLogPlots[0].scales.amps.range = auto
            ? (u, min, max) => [min, max]
            : () => [get('p1-amps-min'), get('p1-amps-max')];
        thermalLogPlots[0].redraw();
    }
    if (thermalLogPlots[2]) {
        thermalLogPlots[2].scales.amps.range = auto
            ? (u, min, max) => [min, max]
            : () => [get('p3-min'), get('p3-max')];
        thermalLogPlots[2].redraw();
    }
}

// ---------------------------------------------------------------------------
// Auto-refresh tied to <details> open/close  — MUST BE LAST
// ---------------------------------------------------------------------------
(function attachThermalLogToggle() {
    function attach() {
        const det = document.getElementById('thermallog-details');
        if (!det) return;

        highlightThermalWindowBtn(30);

        // Live-streamed off CSV2 — no fetch, no auto-refresh timer. Just draw the
        // current ring on open; thermalLiveOnCsv2() keeps it updating while open.
        det.addEventListener('toggle', () => {
            if (det.open) setTimeout(() => { thermalRenderAll(); }, 50);
        });

        document.getElementById('thermal-autoscale')?.addEventListener('change', function () {
            document.getElementById('thermal-manual-ranges').style.display =
                this.checked ? 'none' : 'flex';
            applyThermalRanges();
        });
    }

    if (document.readyState === 'loading')
        document.addEventListener('DOMContentLoaded', attach);
    else
        attach();
})();





function getEchoNumber(id) {
    const el = document.getElementById(id);
    if (!el) return NaN;
    const raw = (el.textContent || '').trim();
    const n = parseFloat(raw);
    return Number.isFinite(n) ? n : NaN;
}

function showValidationError(message) {
    alert(message);
}

function showValidationWarning(message) {
    return confirm(message + '\n\nProceed anyway?');
}

function validateBatterySettings(proposedChanges) {
    const state = {
        BulkVoltage: getEchoNumber('BulkVoltage_echo'),
        AbsorptionVoltage: getEchoNumber('AbsorptionVoltage_echo'),
        RebulkCurrent_A: getEchoNumber('RebulkCurrent_A_echo'),
        FloatVoltage: getEchoNumber('FloatVoltage_echo'),
        RebulkVoltage: getEchoNumber('RebulkVoltage_echo'),
        TargetVoltageSetpoint: getEchoNumber('TargetVoltageSetpoint_echo'),
        TargetVoltageMode: getEchoNumber('TargetVoltageMode_echo'),
        MaintainMode: getEchoNumber('MaintainMode_echo'),
        UseFloat: getEchoNumber('UseFloat_echo'),
        SOC_BlockRebulk_percent: getEchoNumber('SOC_BlockRebulk_percent_echo'),
        SOC_AllowRebulk_percent: getEchoNumber('SOC_AllowRebulk_percent_echo'),
        TailCurrent_A: getEchoNumber('TailCurrent_A_echo'),
        FLOAT_DURATION: getEchoNumber('FLOAT_DURATION_echo'),
        MinFloatTime: getEchoNumber('MinFloatTime_echo'),
        absorptionCompleteTime: getEchoNumber('absorptionCompleteTime_echo'),
        AbsorptionTimeoutMs: getEchoNumber('AbsorptionTimeoutMs_echo'),
        rebulkDebounceTime: getEchoNumber('rebulkDebounceTime_echo'),
    };

    Object.assign(state, proposedChanges);

    const changed = (name) => Object.prototype.hasOwnProperty.call(proposedChanges, name);
    const changedAny = (...names) => names.some(name => changed(name));

    // Hard blocks

    if (changedAny('AbsorptionVoltage', 'BulkVoltage') &&
        Number.isFinite(state.AbsorptionVoltage) &&
        Number.isFinite(state.BulkVoltage) &&
        state.AbsorptionVoltage > state.BulkVoltage) {
        return { valid: false, error: 'Absorption Voltage cannot be higher than Bulk Voltage.' };
    }

    if (changedAny('UseFloat', 'FloatVoltage', 'AbsorptionVoltage') &&
        state.UseFloat == 1 &&
        Number.isFinite(state.FloatVoltage) &&
        Number.isFinite(state.AbsorptionVoltage) &&
        state.FloatVoltage > state.AbsorptionVoltage) {
        return { valid: false, error: 'Float Voltage cannot be higher than Absorption Voltage.' };
    }

    if (changedAny('UseFloat', 'RebulkVoltage', 'FloatVoltage') &&
        state.UseFloat == 1 &&
        Number.isFinite(state.RebulkVoltage) &&
        Number.isFinite(state.FloatVoltage) &&
        state.RebulkVoltage >= state.FloatVoltage) {
        return { valid: false, error: 'Rebulk Voltage must be lower than Float Voltage.' };
    }

    if (changedAny('SOC_AllowRebulk_percent', 'SOC_BlockRebulk_percent') &&
        Number.isFinite(state.SOC_AllowRebulk_percent) &&
        Number.isFinite(state.SOC_BlockRebulk_percent) &&
        state.SOC_AllowRebulk_percent >= state.SOC_BlockRebulk_percent) {
        return {
            valid: false,
            error: 'SoC Allow Rebulk Below must be strictly less than SoC Block Rebulk Above. Equal or reversed values collapse the hysteresis band.'
        };
    }

if (changedAny('TargetVoltageSetpoint', 'TargetVoltageMode') &&
    state.TargetVoltageMode === 1) {
    // No range restrictions — TargetVoltageMode is independent of bulk/rebulk logic
}

    if (changedAny('MaintainMode', 'TargetVoltageMode') &&
        state.MaintainMode === 1 &&
        state.TargetVoltageMode === 1) {
        return {
            valid: false,
            error: 'Force Maintain Mode and Target Voltage Mode cannot both be active. One targets 0 amps in/out; the other regulates to a defined voltage. They conflict.'
        };
    }

    // Soft warnings

    if (changedAny('MinFloatTime', 'FLOAT_DURATION') &&
        Number.isFinite(state.MinFloatTime) &&
        Number.isFinite(state.FLOAT_DURATION) &&
        state.FLOAT_DURATION > 0 &&
        state.MinFloatTime >= state.FLOAT_DURATION * 60) {
        return {
            valid: true,
            warning: `Minimum Float Time (${state.MinFloatTime} min) is ≥ Float Duration (${state.FLOAT_DURATION} hrs = ${state.FLOAT_DURATION * 60} min). The duration-based return to Bulk will never trigger.`
        };
    }

    if (changedAny('absorptionCompleteTime', 'AbsorptionTimeoutMs') &&
        Number.isFinite(state.absorptionCompleteTime) &&
        Number.isFinite(state.AbsorptionTimeoutMs) &&
        state.AbsorptionTimeoutMs > 0 &&
        state.absorptionCompleteTime >= state.AbsorptionTimeoutMs * 60 * 0.9) {
        return {
            valid: true,
            warning: `Absorption Completion Time (${state.absorptionCompleteTime}s) is close to or exceeds Absorption Timeout (${state.AbsorptionTimeoutMs} min = ${state.AbsorptionTimeoutMs * 60}s). The safety timeout may fire before current-taper completion is confirmed.`
        };
    }

    if (changed('TailCurrent_A') &&
        Number.isFinite(state.TailCurrent_A) &&
        state.TailCurrent_A <= 0) {
        return {
            valid: true,
            warning: 'Tail Current is 0 or negative. Absorption will never complete via current taper and will always exit by timeout fallback.'
        };
    }

    if (changed('RebulkCurrent_A') &&
        Number.isFinite(state.RebulkCurrent_A) &&
        state.RebulkCurrent_A <= 0) {
        return {
            valid: true,
            warning: 'Rebulk Current Threshold is 0 or negative. Current-based rebulk detection will never trigger — the system will rely on voltage sag alone to detect a discharged battery.'
        };
    }

    if (changedAny('rebulkDebounceTime', 'MinFloatTime') &&
        Number.isFinite(state.rebulkDebounceTime) &&
        Number.isFinite(state.MinFloatTime) &&
        state.rebulkDebounceTime === 0 &&
        state.MinFloatTime === 0) {
        return {
            valid: true,
            warning: 'Rebulk Debounce Time and Minimum Float Time are both 0. The system may oscillate rapidly between Float and Bulk with no damping.'
        };
    }

    return { valid: true };
}
function validateAndSubmitBatteryNumber(form, fieldName) {
    const input = form.querySelector(`[name="${fieldName}"]`);
    if (!input) return true;

    const value = parseFloat(input.value);
    if (!Number.isFinite(value)) {
        showValidationError('Please enter a valid number.');
        return false;
    }

    const result = validateBatterySettings({ [fieldName]: value });

    if (!result.valid) {
        showValidationError(result.error);
        return false;
    }

    if (result.warning) {
        if (!showValidationWarning(result.warning)) return false;
    }

    submitMessage();
    return true;
}

function validateAndSubmitBatteryToggle(checkbox, fieldName) {
    const proposedValue = checkbox.checked ? 1 : 0;
    const result = validateBatterySettings({ [fieldName]: proposedValue });

    if (!result.valid) {
        checkbox.checked = !checkbox.checked;
        showValidationError(result.error);
        return false;
    }

    if (result.warning) {
        if (!showValidationWarning(result.warning)) {
            checkbox.checked = !checkbox.checked;
            return false;
        }
    }

    return true;
}

function validateAndSubmitFieldCollapseDelay(form) {
    const input = form.querySelector('[name="FIELD_COLLAPSE_DELAY"]');
    if (!input) return true;
    const seconds = parseFloat(input.value);
    if (!Number.isFinite(seconds) || seconds < 2) {
        showValidationError('Field Collapse Delay must be at least 2 seconds. Values lower than this allow rapid field re-engagement after a fault, which can damage the alternator, field driver, and battery.');
        return false;
    }
    submitMessage();
    return true;
}

function updateFloatVisibility() {
    const useFloat = document.getElementById('UseFloat_checkbox') &&
        document.getElementById('UseFloat_checkbox').checked;
    const el = document.getElementById('float-gated-fields');
    if (el) el.style.display = useFloat ? '' : 'none';
}









// ===========================================================================
// CV / Voltage Tuner Log — JavaScript
//
// Decodes /cvlog.bin and downloads as CSV.
// Binary layout: 36-byte header + N × 50-byte CvLogEntry structs (little-endian).
//
// Header (36 bytes):
//   offset  0  uint32  count
//   offset  4  uint32  entrySize (= 51)
//   offset  8  float32 VoltageKp
//   offset 12  float32 VoltageKi
//   offset 16  uint32  VoltageLoopInterval (ms)
//   offset 20  float32 reserved (was VoltageKd — D term removed; always 0.0)
//   offset 24  float32 SlopeBleedThresh (V/s)
//   offset 28  float32 SlopeBleedK (A/(V/s))
//   offset 32  float32 SlopeBleedProxV (V)
//
// Entry (50 bytes):
//   offset  0  uint32   ts
//   offset  4  int16    battV       / 100  → V
//   offset  6  int16    targV       / 100  → V
//   offset  8  int16    vErrorMv    / 1000 → V
//   offset 10  int16    dvdt_x1000  / 1000 → V/s
//   offset 12  int16    vPred       / 100  → V
//   offset 14  int16    fastOvCap   / 10   → A
//   offset 16  int16    cv_I_x10    / 10   → A
//   offset 18  int16    Icv_x10     / 10   → A
//   offset 20  int16    uTarget     / 10   → A
//   offset 22  int16    spLimited   / 10   → A
//   offset 24  int16    iMeas       / 10   → A
//   offset 26  int16    duty        / 10   → %
//   offset 28  uint8    flags       (b0=fastOvActive b1=voltLoopFired b2=cvActive
//                                    b3=soft b4=hard b5=iExcess b6=loadDumpActive)
//   offset 29  uint8    awState     (0=normal 1=frozen(supervisor) 2=saturated 3=bleeding 4=bumpless)
//   offset 30  int16    rpm
//   offset 32  int16    battV_filt_x100 / 100 → V  (IBV, raw battery voltage)
//   offset 34  int16    iMeas_filt_x10  / 10  → A  (MeasuredAmps_filtered, EMA)
//   offset 36  int16    ch1IntervalMs        → ms  (last CH1 inter-sample gap)
//   offset 38  int16    cvDSlope_x10000 / 10000 → V/s (g_fastOvDvdt — fastOV EMA signal)
//   offset 40  int16    battI_x10       / 10  → A  (getBatteryCurrent — INA228 or Victron)
//   offset 42  int16    dBcur_dt_Aps    raw A/s   (g_dBcur_dt clamped to int16)
//   offset 44  int16    voltLoopIntervalMs  ms    actual voltage loop interval when fired (0 if not)
//   offset 46  int16    inaIntervalMs       ms    ina_last_ms at log time — INA228 read freshness
//   offset 48  int16    slopeBleedAmps_x1000 / 1000 → A  cv_I drain this VL tick (0 on non-VL ticks)
//   offset 50  uint8    capReason   0=none 1=KHard_G1 2=KHard_G2 3=iExcess 4=loadDump (binding cap this tick)
// ===========================================================================

const CV_LOG_HEADER_SIZE = 36;
const CV_LOG_ENTRY_SIZE = 51;

// ---------------------------------------------------------------------------
// parseCvBin(buf)
// Returns a decoded object with arrays for each field plus the header params.
// Returns null on bad input.
// ---------------------------------------------------------------------------
function parseCvBin(buf) {
    if (!buf || buf.byteLength < CV_LOG_HEADER_SIZE + CV_LOG_ENTRY_SIZE) {
        return null;
    }

    const view = new DataView(buf);

    // --- Header ---
    const count = view.getUint32(0, true);
    const entrySize = view.getUint32(4, true);
    const voltKp = view.getFloat32(8, true);
    const voltKi = view.getFloat32(12, true);
    const voltInterval = view.getUint32(16, true);
    const voltKd = view.getFloat32(20, true);
    const sbThresh = view.getFloat32(24, true);   // SlopeBleedThresh (V/s)
    const sbK      = view.getFloat32(28, true);   // SlopeBleedK (A/(V/s))
    const sbProxV  = view.getFloat32(32, true);   // SlopeBleedProxV (V)

    if (count === 0) return null;

    const need = CV_LOG_HEADER_SIZE + count * CV_LOG_ENTRY_SIZE;
    if (buf.byteLength < need) {
        console.error('cvlog.bin truncated: have', buf.byteLength, 'need', need);
        return null;
    }

    // --- Arrays ---
    const ts = new Array(count);
    const battV = new Array(count);
    const targV = new Array(count);
    const vError = new Array(count);   // V (positive = below target)
    const dvdt = new Array(count);
    const vPred = new Array(count);
    const fastOvCap = new Array(count);
    const cv_I = new Array(count);
    const Icv = new Array(count);
    const uTarget = new Array(count);
    const spLimited = new Array(count);
    const iMeas = new Array(count);
    const duty = new Array(count);
    const flags = new Array(count);
    const fastOvActive = new Array(count);
    const voltLoopFired = new Array(count);
    const cvActive = new Array(count);
    const hardClamp = new Array(count);
    const rpm = new Array(count);
    const battV_filt = new Array(count);
    const iMeas_filt = new Array(count);
    const ch1Interval = new Array(count);
    const cvDSlope = new Array(count);
    const iExcess = new Array(count);
    const battI = new Array(count);
    const dBcur_dt = new Array(count);
    const loadDumpActive = new Array(count);
    const awState = new Array(count);
    const voltLoopInterval = new Array(count);
    const inaInterval = new Array(count);
    const slopeBleedAmps = new Array(count);
    const capReason = new Array(count);

    const tsBase = view.getUint32(CV_LOG_HEADER_SIZE, true);

    for (let i = 0; i < count; i++) {
        const b = CV_LOG_HEADER_SIZE + i * CV_LOG_ENTRY_SIZE;

        ts[i] = (view.getUint32(b, true) - tsBase) / 1000.0;  // seconds from first entry
        battV[i] = view.getInt16(b + 4, true) / 100.0;
        targV[i] = view.getInt16(b + 6, true) / 100.0;
        vError[i] = view.getInt16(b + 8, true) / 1000.0;
        dvdt[i] = view.getInt16(b + 10, true) / 1000.0;
        vPred[i] = view.getInt16(b + 12, true) / 100.0;
        fastOvCap[i] = view.getInt16(b + 14, true) / 10.0;
        cv_I[i] = view.getInt16(b + 16, true) / 10.0;
        Icv[i] = view.getInt16(b + 18, true) / 10.0;
        uTarget[i] = view.getInt16(b + 20, true) / 10.0;
        spLimited[i] = view.getInt16(b + 22, true) / 10.0;
        iMeas[i] = view.getInt16(b + 24, true) / 10.0;
        duty[i] = view.getInt16(b + 26, true) / 10.0;
        const f = view.getUint8(b + 28);
        flags[i] = f;
        fastOvActive[i] = (f >> 0) & 1;
        voltLoopFired[i] = (f >> 1) & 1;
        cvActive[i] = (f >> 2) & 1;
        // bit 3 reserved (was softClamp — old soft-cap removed)
        hardClamp[i] = (f >> 4) & 1;
        awState[i] = view.getUint8(b + 29);
        rpm[i] = view.getInt16(b + 30, true);
        battV_filt[i] = view.getInt16(b + 32, true) / 100.0;
        iMeas_filt[i] = view.getInt16(b + 34, true) / 10.0;
       ch1Interval[i] = view.getInt16(b + 36, true);
        cvDSlope[i] = view.getInt16(b + 38, true) / 10000.0;
        battI[i] = view.getInt16(b + 40, true) / 10.0;
        dBcur_dt[i] = view.getInt16(b + 42, true);
        voltLoopInterval[i] = view.getInt16(b + 44, true);
        inaInterval[i] = view.getInt16(b + 46, true);
        slopeBleedAmps[i] = view.getInt16(b + 48, true) / 1000.0;
        capReason[i] = view.getUint8(b + 50);
        iExcess[i] = (f >> 5) & 1;
        loadDumpActive[i] = (f >> 6) & 1;
    }

    return {
        count, voltKp, voltKi, voltKd, voltInterval,
        sbThresh, sbK, sbProxV,
        ts, battV, targV, vError, dvdt, vPred,
        fastOvCap, cv_I, Icv, uTarget, spLimited,
        iMeas, duty, flags,
        fastOvActive, voltLoopFired, cvActive, hardClamp,
        rpm, battV_filt, iMeas_filt, ch1Interval, cvDSlope, iExcess, battI,
        dBcur_dt, loadDumpActive, awState, voltLoopInterval, inaInterval,
        slopeBleedAmps, capReason,
    };
}


// ---------------------------------------------------------------------------
// cvBinToCsv(d)
// Converts a parsed cvlog object to a CSV string.
// First row is a settings comment; second row is column headers.
// ---------------------------------------------------------------------------
function cvBinToCsv(d, csv3) {
    const lines = [];

    // Settings header — PID gains from binary log; fastOV/slope constants from last CSV3 frame.
    // Scaling matches echo registry transforms (same raw CSV3 values, same divisors).
    const c = csv3 || {};
    const fmtRaw = (v, dec) => (v !== undefined && !isNaN(v)) ? Number(v).toFixed(dec) : 'N/A';
    const fmtDiv = (v, div, dec) => (v !== undefined && !isNaN(v)) ? (Number(v) / div).toFixed(dec) : 'N/A';
    lines.push(
        `# VoltageKp=${d.voltKp.toFixed(2)} VoltageKi=${d.voltKi.toFixed(3)} VoltageLoopInterval=${d.voltInterval}ms VoltageFilterTC=${fmtRaw(c.VoltageFilterTC, 0)}ms`
    );
    lines.push(
        `# FastOV: OvMeasMarginV=${fmtRaw(c.OvMeasMarginV, 3)}V OvPredMarginV=${fmtRaw(c.OvPredMarginV, 3)}V` +
        ` TdPred=${fmtRaw(c.TdPred, 3)}s DvdtTC=${fmtDiv(c.DvdtTC, 10, 1)}ms` +
        ` KHard=${fmtDiv(c.KHard, 10, 1)}A/V` +
        ` AwBleedRate=${fmtDiv(c.AwBleedRate, 10, 1)}A/s` +  // AwRecoverRate removed from header — hardcoded to 0.1 in firmware
        ` IExcessArmMarginV=${fmtRaw(c.IExcessArmMarginV, 3)}V`
    );
    lines.push(
        `# SlopeBleed: SlopeBleedThresh=${d.sbThresh.toFixed(3)}V/s SlopeBleedK=${d.sbK.toFixed(1)}A/(V/s) SlopeBleedProxV=${d.sbProxV.toFixed(3)}V`
    );
    lines.push(
        `# capReason codes: 0=none(unclamped) 1=KHard_G1(predictive) 2=KHard_G2(measured) 3=iExcess 4=loadDump`
    );

    // Column headers
    lines.push([
        't_s',
        'battV', 'targV', 'vError_V', 'dvdt_Vs', 'vPred',
        'fastOvCap_A', 'cv_I_A', 'Icv_A', 'uTarget_A', 'spLimited_A',
        'iMeas_A', 'duty_pct',
        'fastOvActive', 'voltLoopFired', 'cvActive', 'hardClamp',
        'rpm',
        'battV_filt_V', 'iMeas_filt_A', 'ch1_last_ms', 'iExcess',
        'battI_A', 'dBcur_dt_Aps', 'loadDumpActive',
        'cvDSlope_Vps', 'awState',
        'voltLoopInterval_ms', 'inaInterval_ms',
        'slopeBleedAmps_A', 'capReason',
    ].join(','));

    for (let i = 0; i < d.count; i++) {
        lines.push([
            d.ts[i].toFixed(3),
            d.battV[i].toFixed(2), d.targV[i].toFixed(2),
            d.vError[i].toFixed(4), d.dvdt[i].toFixed(4),
            d.vPred[i].toFixed(2),
            d.fastOvCap[i].toFixed(1), d.cv_I[i].toFixed(1),
            d.Icv[i].toFixed(1), d.uTarget[i].toFixed(1),
            d.spLimited[i].toFixed(1),
            d.iMeas[i].toFixed(1), d.duty[i].toFixed(1),
            d.fastOvActive[i], d.voltLoopFired[i], d.cvActive[i],
            d.hardClamp[i],
            d.rpm[i],
            d.battV_filt[i].toFixed(2), d.iMeas_filt[i].toFixed(1),
            d.ch1Interval[i], d.iExcess[i],
            d.battI[i].toFixed(1), d.dBcur_dt[i], d.loadDumpActive[i],
            d.cvDSlope[i].toFixed(4), d.awState[i],
            d.voltLoopInterval[i], d.inaInterval[i],
            d.slopeBleedAmps[i].toFixed(4), d.capReason[i],
        ].join(','));
    }

    return lines.join('\n');
}


// ---------------------------------------------------------------------------
// downloadCvLog()
// Fetches /cvlog.bin, decodes, saves as timestamped CSV.
// ---------------------------------------------------------------------------
async function downloadCvLog() {
    let buf;
    try {
        const resp = await fetch(buildURL('/cvlog.bin'));
        if (!resp.ok) throw new Error(`HTTP ${resp.status}`);
        buf = await resp.arrayBuffer();
    } catch (err) {
        console.error(`CV log download failed: ${err}`);
        return;
    }

    if (!buf || buf.byteLength < CV_LOG_HEADER_SIZE + CV_LOG_ENTRY_SIZE) {
        console.warn('CV log empty — run in AUTO mode with voltage control active first.');
        return;
    }

    const d = parseCvBin(buf);
    if (!d) {
        console.error('CV log parse failed.');
        return;
    }

    const csv = cvBinToCsv(d, g_lastCsv3);
    const blob = new Blob([csv], { type: 'text/csv' });
    const url = URL.createObjectURL(blob);
    const ts = getLogTimestamp();
    const a = document.createElement('a');
    a.href = url;
    a.download = `cvlog_${ts}.csv`;
    document.body.appendChild(a);
    a.click();
    document.body.removeChild(a);
    URL.revokeObjectURL(url);
}

//SYSTEMID System ID section

function getField(id) {
    const el = document.getElementById(id);
    if (!el) return null;
    if (el.value !== undefined && el.value !== "") return el.value.trim();
    return (el.textContent ?? el.innerText ?? "").trim();
}

// ── Test mode gate helpers ──────────────────────────────────────────────────
// Last-known chargeStageDisplay from CSVData2 (updated each frame)
let gLastChargeStage = 0;

// Stage codes (must match firmware CHARGE_STAGE_* enum)
const CS_BULK        = 1;
const CS_ABSORPTION  = 2;
const CS_FLOAT       = 3;
const CS_MANUAL      = 4;
const CS_MAINTAIN    = 5;
const CS_TARGET_V    = 6;
const CS_IDLE        = 7;

// ── Test Active Floating Panel ──────────────────────────────────────────────
// Panel sits outside .tab-content in the HTML so position:fixed keeps it
// visible regardless of which tab is active.

let _testActiveCSV1 = null;   // 'curr' | 'cv' | 'thermal' | null  (updated from CSV1/2 stream)
let _testActiveCSV3 = null;   // 'sysid' | null                     (updated from CSV3 stream)
let _testPanelCurrentTest = null;

const TEST_PANEL_META = {
    curr:    { title: 'Current Waveform Test', dot: '#f97316' },
    cv:      { title: 'CV Step Test',          dot: '#4a9eff' },
    thermal: { title: 'Thermal Step Test',     dot: '#ef4444' },
    sysid:   { title: 'Plant Delay Test',      dot: '#a855f7' },
};

function getActiveTestKey() {
    return _testActiveCSV3 || _testActiveCSV1 || null;
}

function testPanelInitDrag() {
    const panel  = document.getElementById('test-panel');
    const handle = document.getElementById('test-panel-drag');
    if (!handle || handle._dragInit) return;
    handle._dragInit = true;
    let startX, startY, origLeft, origTop;
    handle.addEventListener('mousedown', e => {
        startX = e.clientX; startY = e.clientY;
        origLeft = panel.offsetLeft; origTop = panel.offsetTop;
        function onMove(e2) {
            panel.style.left  = (origLeft + e2.clientX - startX) + 'px';
            panel.style.top   = (origTop  + e2.clientY - startY) + 'px';
            panel.style.right = 'auto';
        }
        function onUp() {
            document.removeEventListener('mousemove', onMove);
            document.removeEventListener('mouseup',   onUp);
        }
        document.addEventListener('mousemove', onMove);
        document.addEventListener('mouseup',   onUp);
    });
}

function updateTestActivePanel() {
    const testName = getActiveTestKey();
    const overlay  = document.getElementById('test-panel-overlay');
    if (!overlay) return;

    if (!testName) {
        overlay.style.display = 'none';
        _testPanelCurrentTest = null;
        return;
    }

    // Plant Delay test uses its own modal — keep the floating panel hidden
    if (testName === 'sysid') {
        overlay.style.display = 'none';
        _testPanelCurrentTest = testName;
        return;
    }

    overlay.style.display = '';
    testPanelInitDrag();

    // Only reset content when the active test changes identity
    if (testName !== _testPanelCurrentTest) {
        _testPanelCurrentTest = testName;
        const meta = TEST_PANEL_META[testName] || { title: 'Test Active', dot: '#fff' };
        document.getElementById('test-panel-title').textContent  = meta.title;
        document.getElementById('test-panel-dot').style.color    = meta.dot;
        document.getElementById('test-panel-cycles').textContent = 'Cycles: —';
        document.getElementById('test-panel-score').textContent  = 'Score: —';
        const isSysid = (testName === 'sysid');
        const isCv    = (testName === 'cv');
        // CV uses the phase slot for live Δ voltage; sysid/cv don't use score
        document.getElementById('test-panel-phase').textContent  = isCv ? 'Δ  — V' : 'Phase: —';
        document.getElementById('test-panel-cycles').style.display = isSysid ? 'none' : '';
        document.getElementById('test-panel-score').style.display  = (isSysid || isCv) ? 'none' : '';
        updateTestPanelParams(testName);
        ensureTestPollRunning(testName);
    }
}

function updateTestPanelParams(testName) {
    const el = document.getElementById('test-panel-params');
    if (!el) return;
    const g = id => document.getElementById(id)?.textContent ?? '—';
    if (testName === 'curr') {
        el.textContent = 'Amp: ' + g('waveAmplitude_echo') + ' A  •  Period: ' + g('wavePeriod_echo') + ' s';
    } else if (testName === 'cv') {
        el.textContent = 'Amp: ' + g('cvWaveAmplitudeV_echo') + ' V  •  Period: ' + g('cvWavePeriodSec_echo') + ' s';
    } else if (testName === 'thermal') {
        el.textContent = g('thermalWaveLowF_echo') + '°F → ' + g('thermalWaveHighF_echo') + '°F\n' + g('thermalWaveHalfPeriodMin_echo') + ' min half-period';
    } else {
        el.innerHTML = '<button onclick="document.getElementById(\'sysid-modal-overlay\').style.display=\'block\'" class="btn-secondary" style="width:100%;font-size:0.88em;">Show test details ↗</button>';
    }
}

function updateCVPanelDelta() {
    if (cvPlotCache.voltageTarget === null || _lastBatteryV === null) return;
    const el = document.getElementById('test-panel-phase');
    if (!el) return;
    const delta = _lastBatteryV - (cvPlotCache.voltageTarget / 100);
    const sign  = delta >= 0 ? '+' : '';
    const abs   = Math.abs(delta);
    const color = abs < 0.05 ? '#22c55e' : abs < 0.15 ? '#eab308' : '#ef4444';
    el.innerHTML = 'Δ <span style="color:' + color + ';font-weight:600;">' + sign + delta.toFixed(3) + ' V</span>';
}

// Called by the three render functions to push live score data into the panel
function updateTestPanelScore(score, cycles, phase) {
    const scoreEl  = document.getElementById('test-panel-score');
    const cyclesEl = document.getElementById('test-panel-cycles');
    const phaseEl  = document.getElementById('test-panel-phase');
    if (scoreEl  && score   !== undefined) scoreEl.textContent  = score > 0 ? 'Score: ' + score.toFixed(3) : 'Score: —';
    if (cyclesEl && cycles  !== undefined) cyclesEl.textContent = 'Cycles: ' + cycles;
    if (phaseEl  && phase   !== undefined) phaseEl.textContent  = 'Phase: '  + phase;
}

// Ensure the tuning-log poll is running so score data reaches the panel even off-tab
function ensureTestPollRunning(testName) {
    if (testName === 'curr' && !_tuningLogPollTimer) {
        fetchTuningLog();
        _tuningLogPollTimer = setInterval(fetchTuningLog, 1000);
    } else if (testName === 'cv' && !_cvTuningLogPollTimer) {
        fetchCVTuningLog();
        _cvTuningLogPollTimer = setInterval(fetchCVTuningLog, 4000);
    } else if (testName === 'thermal' && !_thermalTuningLogPollTimer) {
        fetchThermalTuningLog();
        _thermalTuningLogPollTimer = setInterval(fetchThermalTuningLog, 8000);
    }
}

// Turn off whichever test is currently shown — called by the ✕ and "Turn Off" button
function turnOffActiveTest() {
    const test = _testPanelCurrentTest;
    if (!test) return;
    if (test === 'sysid') { abortSystemIDTest(); return; }
    const pw = currentAdminPassword;
    if (!pw) { alert('Enter your password first.'); return; }
    const paramMap = { curr: 'TuningMode', cv: 'CVTuningMode', thermal: 'ThermalTuningMode' };
    const param = paramMap[test];
    if (!param) return;
    fetchWithTimeout(buildURL('/get?' + param + '=0&password=' + encodeURIComponent(pw)), {}, 4000)
        .then(() => {
            const cb = document.getElementById(param + '_checkbox');
            if (cb) cb.checked = false;
        })
        .catch(e => console.warn('turnOffActiveTest failed:', e));
}

// Returns false (and shows alert) when test-ON gate fails; always passes for test-OFF.
function checkCurrTestGate(checkbox) {
    if (!checkbox.checked) return true; // turning OFF: always allow
    const conflict = getActiveTestKey();
    if (conflict && conflict !== 'curr') {
        checkbox.checked = false;
        alert((TEST_PANEL_META[conflict]?.title ?? conflict) + ' is already running.\n\nTurn it off before starting another test.');
        return false;
    }
    return true;
}

function checkCVTestGate(checkbox) {
    if (!checkbox.checked) return true;
    const conflict = getActiveTestKey();
    if (conflict && conflict !== 'cv') {
        checkbox.checked = false;
        alert((TEST_PANEL_META[conflict]?.title ?? conflict) + ' is already running.\n\nTurn it off before starting another test.');
        return false;
    }
    const voltageStages = [CS_ABSORPTION, CS_FLOAT, CS_MAINTAIN, CS_TARGET_V];
    if (!voltageStages.includes(gLastChargeStage)) {
        checkbox.checked = false;
        alert('CV Step Test requires voltage-controlled mode.\n\nThe CV loop is only active in ABSORPTION, FLOAT, MAINTAIN, or TARGET V. In BULK the voltage loop has no authority and the test produces no useful data.\n\nEnter a voltage-controlled stage, then enable the test.');
        return false;
    }

    // Guard: HIGH-phase target must not exceed BulkVoltage.
    // updateChargingStage() is NOT suppressed during CVTuningMode. If battV rises
    // above BulkVoltage+0.1V while the system is in the BULK branch, a BULK→ABSORPTION
    // transition fires, which resets the absorption tail timer. If Bcur is reading 0
    // (shunt not active), tailReached is immediately true and the absorption stage will
    // time out and kill the output mid-test.
    if (gLastChargeStage === CS_ABSORPTION || gLastChargeStage === CS_FLOAT) {
        const amplitude = getEchoNumber('cvWaveAmplitudeV_echo');
        const bulkV     = getEchoNumber('BulkVoltage_echo');
        const absV      = getEchoNumber('AbsorptionVoltage_echo');
        if (Number.isFinite(amplitude) && Number.isFinite(bulkV) && Number.isFinite(absV)) {
            const stepTarget = absV + amplitude;
            if (stepTarget > bulkV) {
                checkbox.checked = false;
                alert(
                    `CV Step Test blocked: the HIGH-phase target (${stepTarget.toFixed(2)} V) exceeds BulkVoltage (${bulkV.toFixed(2)} V).\n\n` +
                    `The charging stage machine runs continuously during this test. Stepping the voltage target above BulkVoltage can cause a stage transition that cuts the output mid-test.\n\n` +
                    `Reduce Wave Amplitude, raise BulkVoltage, or use TARGET V mode (which suppresses the stage machine) to run this step test safely.`
                );
                return false;
            }
        }
    }

    return true;
}

function checkThermalTestGate(checkbox) {
    if (!checkbox.checked) return true;
    const conflict = getActiveTestKey();
    if (conflict && conflict !== 'thermal') {
        checkbox.checked = false;
        alert((TEST_PANEL_META[conflict]?.title ?? conflict) + ' is already running.\n\nTurn it off before starting another test.');
        return false;
    }
    const amps = parseFloat(getField('MeasAmpsID') ?? 0);
    if (amps < 5) {
        checkbox.checked = false;
        alert('Thermal Step Test requires the alternator to be actively producing current (> 5 A).\n\nStart the engine, bring the alternator under load, then enable the test.');
        return false;
    }
    return true;
}

// ── System ID Modal ────────────────────────────────────────────────────────

let sysidPollInterval = null;
let sysidPhaseStartWall = 0;   // wall-clock ms when current phase began
let sysidLastPhase = -1;
let sysidSuggestedTC = 0;
let sysidPreflightInterval = null;

// Phase numbers from firmware enum
const SYSID_PHASE_NAMES = {
    1: 'Stabilizing at 10A',
    2: 'Baseline measurement',
    3: 'Step up 1/3',
    4: 'Step down 1/3',
    5: 'Step up 2/3',
    6: 'Step down 2/3',
    7: 'Step up 3/3',
    8: 'Step down 3/3',
    9: 'Processing results'
};

// FieldEventReason codes from firmware enum — must match Xregulator.ino enum FieldEventReason
const SYSID_ABORT_REASONS = {
    0:  'no abort recorded',
    1:  'auto-zero active',
    2:  'temperature data stale',
    3:  'temperature critical',
    4:  'temperature warning',
    5:  'temperature sustained over limit',
    6:  'battery voltage implausible',
    7:  'voltage sensor disagreement (critical)',
    8:  'voltage spike',
    9:  'voltage sensor disagreement (warning)',
    10: 'lockout active',
    11: 'charging disabled',
    12: 'switched to manual mode',
    13: 'hard overvoltage shutdown (INA228)',
    14: 'hard overcurrent shutdown',
    15: 'RPM dropped below minimum',
    16: 'current sensor data stale'
};

function sysidInitDrag() {
    const panel  = document.getElementById('sysid-modal-panel');
    const handle = document.getElementById('sysid-drag-handle');
    if (!handle || handle._dragInit) return;
    handle._dragInit = true;
    let startX, startY, startL, startT;
    function getPos() {
        const r = panel.getBoundingClientRect();
        return { left: r.left, top: r.top };
    }
    function onDown(e) {
        if (window.innerWidth <= 600) return;   // mobile = fixed bottom sheet, not draggable
        const pt = e.touches ? e.touches[0] : e;
        const pos = getPos();
        startX = pt.clientX; startY = pt.clientY;
        startL = pos.left;   startT = pos.top;
        panel.style.left  = startL + 'px';
        panel.style.top   = startT + 'px';
        panel.style.right = 'auto';
        document.addEventListener('mousemove', onMove);
        document.addEventListener('mouseup',   onUp);
        document.addEventListener('touchmove', onMove, { passive: false });
        document.addEventListener('touchend',  onUp);
        e.preventDefault();
    }
    function onMove(e) {
        const pt = e.touches ? e.touches[0] : e;
        const newL = Math.max(0, Math.min(window.innerWidth  - panel.offsetWidth,  startL + pt.clientX - startX));
        const newT = Math.max(0, Math.min(window.innerHeight - panel.offsetHeight, startT + pt.clientY - startY));
        panel.style.left = newL + 'px';
        panel.style.top  = newT + 'px';
        e.preventDefault();
    }
    function onUp() {
        document.removeEventListener('mousemove', onMove);
        document.removeEventListener('mouseup',   onUp);
        document.removeEventListener('touchmove', onMove);
        document.removeEventListener('touchend',  onUp);
    }
    handle.addEventListener('mousedown',  onDown);
    handle.addEventListener('touchstart', onDown, { passive: false });
    // Re-clamp on window resize (e.g. laptop browser shrunk or device rotated)
    // so the panel can't end up partly below the new viewport bottom.
    window.addEventListener('resize', sysidClampPanel);
}

function openSystemIDModal() {
    if (!currentAdminPassword) {
        alert("Please unlock settings first.");
        return;
    }
    // Reset panel position to default top-right on each open
    const panel = document.getElementById('sysid-modal-panel');
    if (panel) { panel.style.left = ''; panel.style.top = '80px'; panel.style.right = '20px'; }
    const overlay = document.getElementById('sysid-modal-overlay');
    overlay.classList.remove('sysid-min', 'sysid-running');   // always open expanded
    sysidShowScreen('preflight');
    overlay.style.display = 'block';
    sysidInitDrag();
    sysidUpdatePreflight();
    sysidPreflightInterval = setInterval(sysidUpdatePreflight, 1000);
}

function closeSystemIDModal() {
    const overlay = document.getElementById('sysid-modal-overlay');
    overlay.style.display = 'none';
    overlay.classList.remove('sysid-min', 'sysid-running');
    if (sysidPreflightInterval) { clearInterval(sysidPreflightInterval); sysidPreflightInterval = null; }
    if (sysidPollInterval)      { clearInterval(sysidPollInterval);      sysidPollInterval = null; }
}

// Minimize (mobile) — collapse the sheet to a progress pill and reveal the live
// current plot behind it (same Plots-tab switch the test-start already does).
function sysidMinimize() {
    const ov = document.getElementById('sysid-modal-overlay');
    if (!ov) return;
    ov.classList.add('sysid-min');
    if (!ov.classList.contains('sysid-running')) {
        const resultsScreen = document.getElementById('sysid-screen-results');
        const onResults = resultsScreen && resultsScreen.style.display !== 'none';
        sysidUpdatePill(onResults ? 'Results ready · tap to expand' : 'tap to expand',
                        onResults ? 100 : null);
    }
    if (vesselInfoComplete && typeof showMainTab === 'function') showMainTab('plots');
}

function sysidRestore() {
    const ov = document.getElementById('sysid-modal-overlay');
    if (ov) ov.classList.remove('sysid-min');
}

// Update the minimized pill's phase label + progress bar/percent.
// pct null -> blank bar/percent (idle/preflight); otherwise 0-100.
function sysidUpdatePill(phaseText, pct) {
    const ph   = document.getElementById('sysid-pill-phase');
    const fill = document.getElementById('sysid-pill-bar-fill');
    const pctEl = document.getElementById('sysid-pill-pct');
    if (ph)   ph.textContent = phaseText;
    if (fill) fill.style.width = (pct == null ? 0 : pct) + '%';
    if (pctEl) pctEl.textContent = (pct == null ? '' : Math.round(pct) + '%');
}

function sysidRunAgain() {
    if (sysidPollInterval) { clearInterval(sysidPollInterval); sysidPollInterval = null; }
    const applyBtn = document.getElementById('sysid-apply-btn');
    if (applyBtn) applyBtn.style.display = '';
    sysidShowScreen('preflight');
    sysidUpdatePreflight();
    if (!sysidPreflightInterval) {
        sysidPreflightInterval = setInterval(sysidUpdatePreflight, 1000);
    }
}

function sysidShowAborted(msg) {
    document.getElementById('sysid-modal-overlay').classList.remove('sysid-running');
    sysidRestore();   // auto-expand so the abort reason is visible, not buried in the pill
    const tbody = document.getElementById('sysid-results-body');
    if (tbody) tbody.innerHTML = '';
    document.getElementById('sysid-results-summary').textContent = '';
    const warnEl = document.getElementById('sysid-results-warning');
    if (warnEl) { warnEl.textContent = msg; warnEl.style.display = ''; }
    const applyBtn = document.getElementById('sysid-apply-btn');
    if (applyBtn) applyBtn.style.display = 'none';
    sysidShowScreen('results');
}

function sysidShowScreen(name) {
    document.getElementById('sysid-screen-preflight').style.display = (name === 'preflight') ? '' : 'none';
    document.getElementById('sysid-screen-progress').style.display  = (name === 'progress')  ? '' : 'none';
    document.getElementById('sysid-screen-results').style.display   = (name === 'results')   ? '' : 'none';
    // Results is the tallest screen; re-clamp so a panel dragged low (while a
    // short screen was showing) can't grow off the bottom of the viewport.
    sysidClampPanel();
}

// Keep the floating panel fully on screen. max-height caps its height to the
// viewport; this nudges its top (and left, once dragged) back inside the edges
// so the bottom — and the action buttons there — never fall below the fold.
function sysidClampPanel() {
    if (window.innerWidth <= 600) return;   // mobile = CSS bottom sheet, no JS positioning
    const panel = document.getElementById('sysid-modal-panel');
    if (!panel || panel.offsetParent === null) return;   // not visible
    const maxTop = Math.max(0, window.innerHeight - panel.offsetHeight);
    let top = parseFloat(panel.style.top);
    if (isNaN(top)) top = panel.getBoundingClientRect().top;
    panel.style.top = Math.min(Math.max(0, top), maxTop) + 'px';
    // Only manage left once the panel has been dragged (drag sets right:auto);
    // otherwise leave it anchored to the right edge.
    if (panel.style.right === 'auto') {
        const maxLeft = Math.max(0, window.innerWidth - panel.offsetWidth);
        let left = parseFloat(panel.style.left);
        if (isNaN(left)) left = panel.getBoundingClientRect().left;
        panel.style.left = Math.min(Math.max(0, left), maxLeft) + 'px';
    }
}

function sysidUpdatePreflight() {
    const rpm     = parseFloat(getField("RPMID") ?? 0);
    const minRpm  = parseFloat(getField("MinRPMForField_echo") ?? 500);
    const amps    = parseFloat(getField("MeasAmpsID") ?? 0);
    const battV   = parseFloat(getField("IBVID") ?? getField("BatteryVID") ?? 0);
    const bulkV   = parseFloat(getField("BulkVoltage_echo") ?? 14.8);

    const rpmOK   = rpm >= minRpm;
    const ampsOK  = amps > 2.0;
    const voltOK  = battV > 11.0 && battV < (bulkV - 0.3);

    const stage   = gLastChargeStage;
    const modeOK  = (stage === CS_BULK || stage === CS_ABSORPTION || stage === CS_FLOAT || stage === CS_TARGET_V);
    const stageLabel = {
        [CS_BULK]:       'Bulk',
        [CS_ABSORPTION]: 'Absorption',
        [CS_FLOAT]:      'Float',
        [CS_TARGET_V]:   'Target Voltage',
        [CS_MAINTAIN]:   'Maintain',
        [CS_IDLE]:       'Idle',
        [CS_MANUAL]:     'Manual',
    }[stage] ?? null;
    const modeMsg = modeOK
        ? '✅ Mode: ' + stageLabel + ' — OK to run (no limits enforced during test)'
        : (stage === CS_MANUAL
            ? '❌ Mode: Manual — not available in manual mode (duty is fixed; test cannot drive the field)'
            : (stageLabel
                ? '❌ Mode: ' + stageLabel + ' — charging must be active (not idle or maintain)'
                : '❌ Mode: Charging not active'));

    // Mutex check: refuse start if any square-wave tuning test is on. Firmware enforces this too;
    // this preflight row just makes the reason visible before the user clicks Start. Read the raw
    // hidden <input> value (not the _echo span, which contains "On"/"Off" text).
    const tuningOn = parseInt(getField("TuningMode") ?? 0) === 1;
    const cvTuningOn = parseInt(getField("CVTuningMode") ?? 0) === 1;
    const thermalTuningOn = parseInt(getField("ThermalTuningMode") ?? 0) === 1;
    const testsOK = !tuningOn && !cvTuningOn && !thermalTuningOn;
    const activeTestName = tuningOn ? 'Current tuning'
                                    : (cvTuningOn ? 'Voltage tuning'
                                                  : (thermalTuningOn ? 'Thermal tuning' : null));
    const testsMsg = testsOK
        ? '✅ No other tuning tests active'
        : '❌ ' + activeTestName + ' is on — turn it off first';

    const allOK   = rpmOK && ampsOK && voltOK && modeOK && testsOK;

    document.getElementById('sysid-check-rpm').textContent   = (rpmOK  ? '✅' : '❌') + ' Engine running (RPM: ' + rpm.toFixed(0) + ' / min ' + minRpm.toFixed(0) + ')';
    document.getElementById('sysid-check-amps').textContent  = (ampsOK ? '✅' : '❌') + ' Alternator producing current (' + amps.toFixed(1) + 'A)';
    document.getElementById('sysid-check-volt').textContent  = (voltOK ? '✅' : '❌') + ' Battery voltage OK (' + battV.toFixed(2) + 'V)';
    document.getElementById('sysid-check-mode').textContent  = modeMsg;
    document.getElementById('sysid-check-tests').textContent = testsMsg;

    // Estimated duration: 20s stabilize overhead + 7 hold phases
    const tcMs   = parseFloat(getField("InputFilterTC_echo") ?? 1000);
    const holdMs = Math.max(15 * tcMs, 5000);
    const estSec = Math.round(20 + 7 * holdMs / 1000);
    const estStr = estSec >= 90 ? (estSec / 60).toFixed(1) + ' min' : estSec + ' sec';
    document.getElementById('sysid-est-time').textContent = 'Estimated test duration: ~' + estStr;

    document.getElementById('sysid-start-btn').disabled = !allOK;
}

function confirmSystemIDStart() {
    if (sysidPreflightInterval) { clearInterval(sysidPreflightInterval); sysidPreflightInterval = null; }

    const formData = new URLSearchParams();
    formData.append("password", currentAdminPassword);
    formData.append("startSystemID", "1");

    fetchWithTimeout(buildURL("/get?" + formData.toString()), {}, 8000)
        .then(() => {
            console.log("SystemID test requested");
            sysidShowScreen('progress');
            sysidLastPhase = -1;
            sysidPhaseStartWall = Date.now();
            sysidStartProgressPoll();
            // Switch to Plots tab so user can watch the current waveform during the test.
            if (vesselInfoComplete) {
                showMainTab('plots');
                // On a phone, auto-minimize to the progress pill so the waveform is
                // visible by default; it auto-expands again when results land.
                if (window.innerWidth <= 600) sysidMinimize();
            }
        })
        .catch(err => {
            alert("Failed to send start command: " + err);
        });
}

function abortSystemIDTest() {
    if (sysidPollInterval) { clearInterval(sysidPollInterval); sysidPollInterval = null; }
    const formData = new URLSearchParams();
    formData.append("password", currentAdminPassword);
    formData.append("cancelSystemID", "1");
    fetchWithTimeout(buildURL("/get?" + formData.toString()), {}, 5000)
        .then(() => console.log("SystemID: abort sent"))
        .catch(err => console.warn("SystemID abort send failed:", err));
    closeSystemIDModal();
}

function sysidStartProgressPoll() {
    const tcMs   = parseFloat(getField("InputFilterTC_echo") ?? 1000);
    const holdMs = Math.max(15 * tcMs, 5000);
    const maxWaitMs = (SYSID_STABILIZE_TIMEOUT_HINT + 7 * holdMs + 10000);
    let elapsed = 0;
    let sysidEverActive = false;
    const pollMs = 400;

    // Zero out the results-ready DOM field so a stale 1 from the previous run
    // can't short-circuit the poll before the firmware's first CSV3 update arrives.
    const rrEl = document.getElementById('systemIDResultsReady_ID');
    if (rrEl) rrEl.textContent = '0';

    // Reset all phase rows
    for (let i = 1; i <= 9; i++) {
        const el = document.getElementById('sysid-p' + i);
        if (el) { el.textContent = '⬜ ' + SYSID_PHASE_NAMES[i]; el.style.color = ''; }
    }
    document.getElementById('sysid-phase-bar').style.width = '0%';
    document.getElementById('sysid-modal-overlay').classList.add('sysid-running');   // pulses the pill dot

    // Wall-clock phase advancement: once we know a phase started, we advance the bar
    // and labels by timer without waiting for the next CSV2 packet.
    // CSV2 is the authoritative sync signal — if it shows a higher phase we jump ahead.
    let wallPhase = 0;       // phase we're currently displaying (may be ahead of last CSV2)
    let wallPhaseStart = 0;  // Date.now() when wallPhase was last set

    function sysidSetWallPhase(p) {
        if (p > wallPhase) {
            wallPhase = p;
            wallPhaseStart = Date.now();
            for (const [pStr, name] of Object.entries(SYSID_PHASE_NAMES)) {
                const pNum = parseInt(pStr);
                const el = document.getElementById('sysid-p' + pNum);
                if (!el) continue;
                if (wallPhase > pNum) {
                    el.textContent = '✅ ' + name; el.style.color = '#4caf50';
                } else if (wallPhase === pNum) {
                    el.textContent = '▶ ' + name + '…'; el.style.color = '#4a9eff';
                } else {
                    el.textContent = '⬜ ' + name; el.style.color = '';
                }
            }
        }
    }

    sysidPollInterval = setInterval(() => {
        elapsed += pollMs;

        const phase = parseInt(getField("systemIDActive_ID") ?? 0);
        const ready = parseInt(getField("systemIDResultsReady_ID") ?? 0);

        if (phase > 0) sysidEverActive = true;

        // Sync to firmware phase if it's ahead of our wall-clock estimate
        if (phase > wallPhase) sysidSetWallPhase(phase);

        // Once a non-stabilise phase is active, advance by holdMs wall time.
        // This means the UI advances immediately even if the next CSV2 is delayed.
        if (wallPhase >= 2 && wallPhase <= 8) {
            const elapsed_in_phase = Date.now() - wallPhaseStart;
            if (elapsed_in_phase >= holdMs && wallPhase < 9) {
                sysidSetWallPhase(wallPhase + 1);
            }
        }

        // Phase bar: for stabilize use 30s estimate, for measured phases use holdMs
        const phaseMs = (wallPhase === 1) ? 30000 : holdMs;
        const phasePct = Math.min(100, ((Date.now() - wallPhaseStart) / phaseMs) * 100);
        document.getElementById('sysid-phase-bar').style.width = phasePct.toFixed(0) + '%';

        document.getElementById('sysid-elapsed').textContent = 'Elapsed: ' + (elapsed / 1000).toFixed(0) + 's';

        // Mirror progress into the minimized pill (8-phase measured sequence; 9 = processing)
        const overallPct = Math.min(100, Math.max(0, (wallPhase - 1 + phasePct / 100) / 8 * 100));
        sysidUpdatePill((SYSID_PHASE_NAMES[wallPhase] || 'Running') + '…', overallPct);

        if (ready === 1) {
            clearInterval(sysidPollInterval); sysidPollInterval = null;
            document.getElementById('sysid-modal-overlay').classList.remove('sysid-running');
            // Mark all phases complete
            for (let i = 1; i <= 9; i++) {
                const el = document.getElementById('sysid-p' + i);
                if (el) { el.textContent = '✅ ' + SYSID_PHASE_NAMES[i]; el.style.color = '#4caf50'; }
            }
            document.getElementById('sysid-phase-bar').style.width = '100%';
            sysidUpdatePill('Results ready · tap to expand', 100);
            sysidRestore();   // auto-expand the sheet so the results are front-and-centre
            setTimeout(showSystemIDResults, 400);
            return;
        }

        // Protection layer fired mid-test — firmware aborted it, results are invalid
        if (phase === 0 && ready !== 1 && sysidEverActive) {
            clearInterval(sysidPollInterval); sysidPollInterval = null;
            document.getElementById('sysid-modal-overlay').classList.remove('sysid-running');
            const reasonCode = parseInt(getField("systemIDAbortReason_ID") ?? 0);
            const abortPhase = parseInt(getField("systemIDAbortPhase_ID") ?? 0);
            const reasonText = SYSID_ABORT_REASONS[reasonCode] ?? ('reason code ' + reasonCode);
            const phaseText  = SYSID_PHASE_NAMES[abortPhase] ?? ('phase ' + abortPhase);
            const msg = (reasonCode === 0)
                ? '⚠ Test aborted — a protection layer fired mid-test. Check the Console tab for details.'
                : '⚠ Aborted at phase ' + abortPhase + ' (' + phaseText + '): ' + reasonText + '.';
            sysidShowAborted(msg);
            return;
        }

        if (elapsed > maxWaitMs) {
            clearInterval(sysidPollInterval); sysidPollInterval = null;
            alert("Plant Delay test timed out after " + (elapsed / 1000).toFixed(0) + "s. Check the Console tab for details.");
            closeSystemIDModal();
        }
    }, pollMs);
}

// Hint used by progress poll for stabilize phase timeout window (must match firmware #define)
const SYSID_STABILIZE_TIMEOUT_HINT = 30000;

function showSystemIDResults() {
    const r = [
        parseFloat(getField("systemIDRiseDelay_0_ID") ?? -1),
        parseFloat(getField("systemIDRiseDelay_1_ID") ?? -1),
        parseFloat(getField("systemIDRiseDelay_2_ID") ?? -1)
    ];
    const f = [
        parseFloat(getField("systemIDFallDelay_0_ID") ?? -1),
        parseFloat(getField("systemIDFallDelay_1_ID") ?? -1),
        parseFloat(getField("systemIDFallDelay_2_ID") ?? -1)
    ];
    // Step amplitude and noise: firmware sends ×10 integers, divide to get amps with 0.1A res
    const stepAmp = [
        parseFloat(getField("systemIDStepAmp_0_ID") ?? 0) / 10,
        parseFloat(getField("systemIDStepAmp_1_ID") ?? 0) / 10,
        parseFloat(getField("systemIDStepAmp_2_ID") ?? 0) / 10,
    ];
    const quietPP = [
        parseFloat(getField("systemIDQuietPP_0_ID") ?? 0) / 10,
        parseFloat(getField("systemIDQuietPP_1_ID") ?? 0) / 10,
        parseFloat(getField("systemIDQuietPP_2_ID") ?? 0) / 10,
    ];
    const ra = parseFloat(getField("systemIDRiseAvg_ID") ?? -1);
    const fa = parseFloat(getField("systemIDFallAvg_ID") ?? -1);

    // Recommended TC = highest single trial across all rise and fall measurements.
    // Using the worst-case individual reading (not the average) ensures the filter
    // is always long enough to cover the slowest response the plant actually showed.
    const allValid = [...r, ...f].filter(v => v >= 0);
    sysidSuggestedTC = allValid.length > 0 ? Math.max(...allValid) : 0;

    // ── Quality table ────────────────────────────────────────────────────────────
    // Step amplitude: pass > 10A. Noise peak-to-peak: green < 1A, yellow 1–2A, red ≥ 2A.
    const hasQuality = stepAmp.some(v => v > 0);
    const qTbody = document.getElementById('sysid-quality-body');
    if (qTbody) {
        qTbody.innerHTML = '';
        for (let i = 0; i < 3; i++) {
            const ampOk    = stepAmp[i] > 10;
            const noiseOk  = quietPP[i] < 1.0;
            const noiseMed = quietPP[i] < 2.0;
            const ampColor   = ampOk  ? '#4caf50' : '#ef4444';
            const noiseColor = noiseOk ? '#4caf50' : (noiseMed ? '#f0a500' : '#ef4444');
            const ampLabel   = hasQuality ? stepAmp[i].toFixed(1) + ' A' : '—';
            const noiseLabel = hasQuality ? quietPP[i].toFixed(1) + ' A' : '—';
            const tr = document.createElement('tr');
            tr.style.borderBottom = '1px solid #2a2a2a';
            tr.innerHTML =
                '<td style="padding:5px 4px; color:#aaa;">Trial ' + (i + 1) + '</td>' +
                '<td style="padding:5px 4px; color:' + ampColor   + '; font-weight:600;">' + ampLabel   + '</td>' +
                '<td style="padding:5px 4px; color:' + noiseColor + '; font-weight:600;">' + noiseLabel + '</td>';
            qTbody.appendChild(tr);
        }

        // Quality advisory
        const qNote = document.getElementById('sysid-quality-note');
        if (qNote && hasQuality) {
            const noiseHigh  = quietPP.some(v => v >= 2.0);
            const noiseMedAny = quietPP.some(v => v >= 1.0 && v < 2.0);
            const ampLow     = stepAmp.some(v => v > 0 && v <= 10);
            const msgs = [];
            if (noiseHigh)   msgs.push('⚠ High noise on one or more trials — belt or alternator resonance at this RPM is inflating the measured time constant. Re-run the test at a slightly different engine speed for a cleaner result.');
            else if (noiseMedAny) msgs.push('⚠ Moderate noise detected. Results are usable but try a different RPM if you want a tighter reading.');
            if (ampLow)      msgs.push('⚠ Step amplitude is small (≤10 A) — consider increasing SystemIDStepAmplitude for a stronger signal.');
            qNote.innerHTML = msgs.length ? msgs.join('<br>') : '✅ Signal quality is good.';
            qNote.style.display = '';
        } else if (qNote) {
            qNote.style.display = 'none';
        }
        document.getElementById('sysid-quality-section').style.display = hasQuality ? '' : 'none';
    }

    // ── Timing results table ─────────────────────────────────────────────────────
    const tbody = document.getElementById('sysid-results-body');
    tbody.innerHTML = '';
    for (let i = 0; i < 3; i++) {
        const tr = document.createElement('tr');
        tr.style.borderBottom = '1px solid #333';
        tr.innerHTML = '<td style="padding:6px 4px;">Trial ' + (i + 1) + '</td>' +
            '<td style="padding:6px 4px;">' + (r[i] >= 0 ? r[i].toFixed(0) : '—') + '</td>' +
            '<td style="padding:6px 4px;">' + (f[i] >= 0 ? f[i].toFixed(0) : '—') + '</td>';
        tbody.appendChild(tr);
    }
    const avgRow = document.createElement('tr');
    avgRow.style.fontWeight = 'bold';
    avgRow.innerHTML = '<td style="padding:8px 4px; color:#aaa;">Average</td>' +
        '<td style="padding:8px 4px;">' + (ra >= 0 ? ra.toFixed(0) : '—') + '</td>' +
        '<td style="padding:8px 4px;">' + (fa >= 0 ? fa.toFixed(0) : '—') + '</td>';
    tbody.appendChild(avgRow);

    const tcFast = Math.max(1, Math.round(sysidSuggestedTC / 3));
    const tcSlow = Math.max(1, Math.round(sysidSuggestedTC));
    document.getElementById('sysid-results-summary').innerHTML =
        'Measured plant delay: <strong style="color:#4a9eff;">' + sysidSuggestedTC.toFixed(0) + ' ms</strong>' +
        ' (highest single trial)<br>' +
        '<span style="font-size:0.85em; color:#aaa;">Suggested filter TCs: ' +
        '<strong>' + tcFast + ' ms</strong> for excess-current detection &amp; PID feedback (plant/3), ' +
        '<strong>' + tcSlow + ' ms</strong> for voltage smoothing (full plant delay)</span>';

    const applyBtn = document.getElementById('sysid-apply-btn');
    if (applyBtn) { applyBtn.style.display = ''; applyBtn.textContent = 'Set All Filters (' + tcFast + ' / ' + tcFast + ' / ' + tcSlow + ' ms)'; }

    // Variance check: if spread within rise or fall trials > 25%, recommend re-run
    const warnEl = document.getElementById('sysid-results-warning');
    const validR = r.filter(v => v >= 0);
    const validF = f.filter(v => v >= 0);
    function spreadPct(arr) {
        if (arr.length < 2) return 0;
        const mn = Math.min(...arr), mx = Math.max(...arr), avg = arr.reduce((a,b)=>a+b,0)/arr.length;
        return avg > 0 ? (mx - mn) / avg * 100 : 0;
    }
    const rSpread = spreadPct(validR);
    const fSpread = spreadPct(validF);
    if (rSpread > 25 || fSpread > 25) {
        warnEl.textContent = '⚠ High variance between trials (rise: ' + rSpread.toFixed(0) + '%, fall: ' + fSpread.toFixed(0) + '%). ' +
            'Results are unreliable — run the test again.';
        warnEl.style.display = '';
    } else {
        warnEl.style.display = 'none';
    }

    sysidShowScreen('results');
}

function applySystemIDResults() {
    if (!currentAdminPassword) { alert("Please unlock settings first."); return; }
    // Excess-current detection and PID feedback get plant/3 to preserve phase margin
    // inside the control loop. Voltage smoothing gets the full plant delay because its
    // consumer (slope bleed dV/dt) runs on the same timescale as the voltage loop tick.
    const tcFast = Math.max(1, Math.round(sysidSuggestedTC / 3));
    const tcSlow = Math.max(1, Math.round(sysidSuggestedTC));
    const tcFastEnc = encodeURIComponent(tcFast);
    const tcSlowEnc = encodeURIComponent(tcSlow);
    const pw = encodeURIComponent(currentAdminPassword);
    fetch(buildURL("/get?InputFilterTC=" + tcFastEnc + "&password=" + pw))
        .then(() => fetch(buildURL("/get?OutputPIDFilterTC=" + tcFastEnc + "&password=" + pw)))
        .then(() => fetch(buildURL("/get?VoltageFilterTC=" + tcSlowEnc + "&password=" + pw)))
        .then(() => {
            console.log("Filter TCs updated: iExcess=" + tcFast + "ms, PID=" + tcFast + "ms, Voltage=" + tcSlow + "ms");
            closeSystemIDModal();
        })
        .catch(err => console.error("Filter TC update failed:", err));
}


// Fetch matrix stats once on page load (also refreshes whenever Alternator tab is opened)

// Auto-login via URL parameter for local automation (e.g. SwiftBar shortcut)
window.addEventListener('load', function () {
  const autopass = new URLSearchParams(window.location.search).get('autopass');
  if (autopass) {
    const el = document.getElementById('admin_password');
    if (el) { el.value = autopass; setAdminPassword(); }
  }
});

// Protections alt-tab — filter pills + intro toggling + empty-phase hiding.
// Scoped to #alt-panel-protections so it can't affect the rest of the site.
(function initProtectionsFilters() {
  const panel = document.getElementById('alt-panel-protections');
  if (!panel) return;

  const pills = panel.querySelectorAll('.protections-filters button[data-filter]');
  let activeFilter = 'all';

  function applyFilter() {
    panel.querySelectorAll('.protections-param').forEach(card => {
      const groups = (card.dataset.groups || '').split(/\s+/).filter(Boolean);
      const show = (activeFilter === 'all') || groups.includes(activeFilter);
      card.style.display = show ? '' : 'none';
    });
    panel.querySelectorAll('.protections-intro p').forEach(p => {
      p.classList.toggle('active', p.dataset.intro === activeFilter);
    });
    panel.querySelectorAll('.protections-phase').forEach(sec => {
      const anyCard = Array.from(sec.querySelectorAll('.protections-param'))
        .some(a => a.style.display !== 'none');
      const anyIntro = sec.querySelector('.protections-intro p.active') !== null;
      sec.classList.toggle('empty', !anyCard && !anyIntro);
      const grid = sec.querySelector('.protections-cards');
      if (grid) grid.classList.toggle('empty', !anyCard);
    });
  }

  pills.forEach(btn => {
    btn.addEventListener('click', () => {
      pills.forEach(b => b.classList.remove('active'));
      btn.classList.add('active');
      activeFilter = btn.dataset.filter;
      applyFilter();
    });
  });

  applyFilter();
})();

// ========================================================================
// BAROMETER PANEL (Other tab) — Zambretti forecast + 14-day history plot
// Lives in its own IIFE so locals don't leak into the global namespace.
// Public hooks (window.*) are how the rest of the app pokes us:
//   window.initBaroPanel()              — call on Other-tab activation (idempotent)
//   window.updateBaroDisplay(data, sa)  — call from CSV2 dispatcher each cycle
// ========================================================================
(function () {
  'use strict';

  const BARO_HISTORY_SIZE = 1008;              // must match firmware (7 days × 6 samples/h)
  const SAMPLE_INTERVAL_MIN = 10;              // ditto — firmware samples every 10 min
  const FETCH_INTERVAL_MS = 10 * 60 * 1000;    // refresh history every 10 min while tab visible
  const STALE_MS_NMEA = 10000;                 // mirrors STALE_THRESHOLD_TEMP_MS-style logic for NMEA fields

  // 26-slot Zambretti forecast table (slot 1 = best, slot 26 = worst).
  // Wording follows the conventional 1985 Beteljuice reproduction commonly cited online.
  const Z_FORECASTS = [
    'Settled fine',
    'Fine weather',
    'Becoming fine',
    'Fine, becoming less settled',
    'Fine, possible showers',
    'Fairly fine, improving',
    'Fairly fine, possible showers early',
    'Fairly fine, showery later',
    'Showery early, improving',
    'Changeable, mending',
    'Fairly fine, showers likely',
    'Rather unsettled, clearing later',
    'Unsettled, probably improving',
    'Showery, bright intervals',
    'Showery, becoming less settled',
    'Changeable, some rain',
    'Unsettled, short fine intervals',
    'Unsettled, rain later',
    'Unsettled, rain at times',
    'Very unsettled, rain',
    'Stormy, may improve',
    'Stormy, much rain',
    'Rain at times, worse later',
    'Rain at times, very unsettled',
    'Rain, becoming heavy and squally',
    'Stormy, possibly worse'
  ];

  // Pressure zones (mbar). Colors mirror the prototype palette and degrade gracefully in dark mode.
  const ZONES = [
    { min: -Infinity, max: 980,      name: 'STORMY',   color: '#c62828', dark: '#e57373', range: 'below 980 mbar' },
    { min: 980,       max: 1000,     name: 'RAIN',     color: '#ef6c00', dark: '#ffb74d', range: '980–1000 mbar' },
    { min: 1000,      max: 1010,     name: 'CHANGE',   color: '#c0a000', dark: '#ddcc66', range: '1000–1010 mbar' },
    { min: 1010,      max: 1025,     name: 'FAIR',     color: '#2e7d32', dark: '#81c784', range: '1010–1025 mbar' },
    { min: 1025,      max: Infinity, name: 'VERY DRY', color: '#1565c0', dark: '#64b5f6', range: 'above 1025 mbar' }
  ];

  // State — buffer is held chronologically (oldest at index 0, newest at end).
  // NaN slots = empty (no sample yet, or wiped because too old to display).
  let baroPlot = null;
  let baroBuffer = new Float32Array(BARO_HISTORY_SIZE);
  let baroBufferEpochAtEnd = 0;          // unix seconds of the newest non-NaN sample (0 = unknown)
  let currentRangeHours = 24;
  let autoscaleY = false;
  let baroYManual = null;          // [min,max] typed on the plot; beats autoscale + fixed default
  try { baroYManual = JSON.parse(localStorage.getItem('baroYManual')) || null; } catch (e) { }
  let fetchTimer = null;
  let initialized = false;
  let panelVisible = false;

  for (let i = 0; i < BARO_HISTORY_SIZE; i++) baroBuffer[i] = NaN;

  // ---------- PUBLIC API ---------------------------------------------------
  window.initBaroPanel = function () {
    panelVisible = true;
    if (!initialized) {
      wireControls();
      initialized = true;
    }
    fetchBaroHistory();
    if (fetchTimer) clearInterval(fetchTimer);
    fetchTimer = setInterval(() => { if (panelVisible) fetchBaroHistory(); }, FETCH_INTERVAL_MS);
  };

  // Note: we don't have a "tab hidden" hook so panelVisible stays true. Fetches are cheap (~8 KB)
  // so re-fetching every 5 min in the background is fine.

  window.updateBaroDisplay = function (data, sa) {
    if (!data) return;
    const p = parseFloat(data.baroPressure) / 10;   // CSV2 sends mbar ×10
    if (!isFinite(p) || p < 800 || p > 1100) {
      setText('baroZoneLabel', '—');
      setText('baroZoneSub', 'sensor offline');
      return;
    }

    // Zone
    const dark = document.body.classList.contains('dark-mode');
    const zone = pressureZone(p);
    const zoneColor = dark ? zone.dark : zone.color;
    const zEl = document.getElementById('baroZoneLabel');
    if (zEl) { zEl.textContent = zone.name; zEl.style.color = zoneColor; }
    setText('baroZoneSub', zone.range);

    // 3-hour tendency from buffer
    const samplesPer3h = Math.round((3 * 60) / SAMPLE_INTERVAL_MIN);  // 36
    let delta = null;
    if (baroBufferEpochAtEnd > 0) {
      // Newest non-NaN sample is at the last filled slot; walk back samplesPer3h slots.
      const lastIdx = lastFilledIdx();
      if (lastIdx >= samplesPer3h) {
        const oldP = baroBuffer[lastIdx - samplesPer3h];
        if (isFinite(oldP)) delta = p - oldP;
      }
    }

    if (delta === null) {
      setText('baroTendencyLabel', 'Building…');
      setText('baroTendencyDelta', 'need 3 h of samples');
      setHTML('baroTendencyArrow', makeTendencyArrow(0));
    } else {
      setText('baroTendencyLabel', tendencyLabel(delta));
      setText('baroTendencyDelta', `${delta >= 0 ? '+' : ''}${delta.toFixed(1)} mbar / 3h`);
      setHTML('baroTendencyArrow', makeTendencyArrow(delta));
    }

    // Forecast — Zambretti when wind & GPS fresh, fallback otherwise
    const headingFresh    = sa && sa.heading != null && sa.heading < STALE_MS_NMEA;
    const trueWindFresh   = sa && sa.trueWindAngle != null && sa.trueWindAngle < STALE_MS_NMEA;
    const latFresh        = isFinite(parseFloat(data.LatitudeNMEA)) && data.LatitudeNMEA !== 0;

    let forecastStr, methodLabel, methodSub;
    if (delta !== null && headingFresh && trueWindFresh && latFresh) {
      const twdEarth = (parseFloat(data.HeadingNMEA) + parseFloat(data.TrueWindAngleNMEA) + 360) % 360;
      const z = zambretti(p, delta, twdEarth, parseFloat(data.LatitudeNMEA), new Date());
      forecastStr = z.text;
      methodLabel = 'Zambretti';
      methodSub = `wind ${twdToCompass(twdEarth)} · slot ${z.Z}`;
    } else if (delta !== null) {
      forecastStr = fallbackForecast(p, delta);
      methodLabel = 'Tendency only';
      const missing = [];
      if (!headingFresh)  missing.push('heading');
      if (!trueWindFresh) missing.push('true wind');
      if (!latFresh)      missing.push('GPS');
      methodSub = missing.length ? `no ${missing.join(', ')}` : '';
    } else {
      forecastStr = 'Collecting history — full forecast available after 3 h of samples.';
      methodLabel = 'Warming up';
      methodSub = '';
    }
    setText('baroForecastText', forecastStr);
    setText('baroMethodLabel', methodLabel);
    setText('baroMethodSub', methodSub);
  };

  // ---------- HISTORY FETCH ------------------------------------------------
  function fetchBaroHistory() {
    fetch('/baroHistory.bin', { cache: 'no-cache' })
      .then(r => { if (!r.ok) throw new Error('http ' + r.status); return r.arrayBuffer(); })
      .then(buf => parseHistory(buf))
      .then(() => { if (baroPlot) refreshPlot(); else buildBaroPlot(); })
      .catch(e => { /* offline / not yet implemented in older firmware — quietly skip */ });
  }

  function parseHistory(arrayBuf) {
    // Header (8 B): u16 head, u32 epoch, u16 count.  Samples: u16 mbar×10 (0 = empty).
    const view = new DataView(arrayBuf);
    if (arrayBuf.byteLength < 8) return;
    const head  = view.getUint16(0, true);
    const epoch = view.getUint32(2, true);
    const count = view.getUint16(6, true);
    if (count !== BARO_HISTORY_SIZE) return;
    if (arrayBuf.byteLength < 8 + count * 2) return;

    // Firmware stores samples circularly with `head` = next write slot. The newest sample
    // is at (head - 1) mod count; oldest unwritten slots are zero. Walk back chronologically.
    for (let i = 0; i < count; i++) {
      const ringIdx = (head - count + i + count) % count;
      const raw = view.getUint16(8 + ringIdx * 2, true);
      baroBuffer[i] = raw === 0 ? NaN : raw / 10.0;
    }
    baroBufferEpochAtEnd = epoch;
  }

  function lastFilledIdx() {
    for (let i = BARO_HISTORY_SIZE - 1; i >= 0; i--) if (isFinite(baroBuffer[i])) return i;
    return -1;
  }

  // ---------- PLOT ---------------------------------------------------------
  function buildBaroPlot() {
    const el = document.getElementById('baroPlot');
    if (!el || typeof uPlot === 'undefined') return;
    el.innerHTML = '';

    const accent = getCss('--accent') || '#00a19a';
    const ink    = getCss('--text-dark') || '#333';
    const slice  = computePlotSlice();

    const opts = {
      width: Math.max(el.clientWidth, 320),
      height: el.clientHeight || 280,
      series: [
        { label: 'Seconds ago' },
        { label: 'Pressure (mbar)', stroke: accent, width: 2, fill: hexAlpha(accent, 0.12) }
      ],
      scales: {
        x: { time: false, auto: false, range: [slice.xAxis[0], slice.xAxis[slice.xAxis.length - 1] || 0] },
        y: { auto: false, range: () => computeYRange() }
      },
      axes: [
        { label: 'Time', grid: { show: true }, values: (u, ticks) => ticks.map(formatAgoLabel) },
        { scale: 'y', label: 'mbar', grid: { show: true }, side: 3,
          splits: edgeLabeledSplits(() => baroYManual !== null) }
      ],
      legend: { show: false },
      cursor: { drag: { x: false, y: false } }
    };

    baroPlot = new uPlot(opts, [slice.xAxis, slice.values], el);
    new ResizeObserver(() => {
      if (baroPlot) baroPlot.setSize({ width: el.clientWidth, height: el.clientHeight || 280 });
    }).observe(el);

    // Click-to-edit Y limits (clear both boxes + Enter returns to the
    // default/autoscale behavior picked by the checkbox below the plot)
    attachYAxisEdit(baroPlot, [{
      scale: 'y', decimals: 0,
      apply: (mn, mx) => {
        baroYManual = [mn, mx];
        localStorage.setItem('baroYManual', JSON.stringify(baroYManual));
        refreshPlot();
      },
      auto: () => {
        baroYManual = null;
        localStorage.removeItem('baroYManual');
        refreshPlot();
      }
    }]);
  }

  function refreshPlot() {
    const slice = computePlotSlice();
    baroPlot.setData([slice.xAxis, slice.values]);
    baroPlot.setScale('x', { min: slice.xAxis[0], max: slice.xAxis[slice.xAxis.length - 1] || 0 });
  }

  function computePlotSlice() {
    // X axis = seconds ago (negative). Newest sample is at x=0.
    const samples = currentRangeHours * 60 / SAMPLE_INTERVAL_MIN;
    const startIdx = Math.max(0, BARO_HISTORY_SIZE - samples);
    const xAxis = new Array(BARO_HISTORY_SIZE - startIdx);
    const values = new Array(BARO_HISTORY_SIZE - startIdx);
    for (let i = startIdx; i < BARO_HISTORY_SIZE; i++) {
      const ago = (BARO_HISTORY_SIZE - 1 - i) * SAMPLE_INTERVAL_MIN * 60;  // seconds
      const k = i - startIdx;
      xAxis[k]  = -ago;
      values[k] = isFinite(baroBuffer[i]) ? baroBuffer[i] : null;
    }
    return { xAxis, values };
  }

  function computeYRange() {
    if (baroYManual) return baroYManual;
    if (!autoscaleY) return [940, 1050];
    // Middle 2/3 of plot height: pad = 25% of data range above and below.
    const slice = computePlotSlice().values;
    let lo = Infinity, hi = -Infinity;
    for (const v of slice) { if (v != null) { if (v < lo) lo = v; if (v > hi) hi = v; } }
    if (!isFinite(lo) || !isFinite(hi) || lo >= hi) return [940, 1050];
    const range = Math.max(hi - lo, 2);
    const pad = range * 0.25;
    return [lo - pad, hi + pad];
  }

  function formatAgoLabel(secNeg) {
    const s = Math.abs(secNeg);
    if (s < 60) return 'now';
    if (s < 3600) return '−' + Math.round(s / 60) + 'm';
    const h = s / 3600;
    if (h < 24) return '−' + (h % 1 ? h.toFixed(1) : h.toFixed(0)) + 'h';
    const d = h / 24;
    return '−' + (d % 1 ? d.toFixed(1) : d.toFixed(0)) + 'd';
  }

  // ---------- ZAMBRETTI ----------------------------------------------------
  function zambretti(pressureMb, delta3h, windDirDeg, latDeg, dateObj) {
    let tendency;
    if (delta3h > 1.6)       tendency = 'rising';
    else if (delta3h < -1.6) tendency = 'falling';
    else                     tendency = 'steady';

    // Base index: 1050 mbar → slot 1 (best), 950 mbar → slot 26 (worst). Clamp.
    const p = Math.max(950, Math.min(1050, pressureMb));
    let Z = Math.round(((1050 - p) / 100) * 25) + 1;

    if (tendency === 'rising')  Z -= 5;
    if (tendency === 'falling') Z += 5;

    // Wind direction adjustment (NH; mirror N↔S for SH). Easterly winds in temperate latitudes
    // are associated with pre-frontal weather (worse Z); westerlies with post-frontal clearing.
    // Sector index from windDirDeg (FROM direction): 0=N, 1=NE, 2=E, 3=SE, 4=S, 5=SW, 6=W, 7=NW.
    const adj = [0, 1, 2, 1, -1, 0, -1, -2];
    const mirrorSH = [4, 3, 2, 1, 0, 7, 6, 5];
    const sector = Math.round((((windDirDeg % 360) + 360) % 360) / 45) % 8;
    Z += latDeg >= 0 ? adj[sector] : adj[mirrorSH[sector]];

    // Season adjustment — Zambretti was developed for temperate latitudes
    const month = dateObj.getMonth() + 1;
    const summerN = month >= 4 && month <= 9;
    const summer = latDeg >= 0 ? summerN : !summerN;
    if (summer && tendency === 'rising')  Z -= 2;
    if (summer && tendency === 'falling') Z += 1;

    Z = Math.max(1, Math.min(26, Z));
    return { Z, text: Z_FORECASTS[Z - 1], tendency };
  }

  // Fallback: UK Met Office Shipping Forecast wording, tendency-only.
  function fallbackForecast(p, d) {
    const a = Math.abs(d);
    const falling = d < -0.3, rising = d > 0.3, steady = !falling && !rising;
    const high = p > 1018, low = p < 1005;
    if (falling && a > 6 && low)    return 'Storm approaching — secure gear and reduce sail.';
    if (falling && a > 3.5)         return low
      ? 'Front passing — wind and rain likely within hours.'
      : 'Wind backing — rain likely within 12 h, reef early.';
    if (falling && a > 1.5)         return high
      ? 'Pressure easing — weather may turn, watch the sky.'
      : 'Unsettled spell developing — stow loose gear.';
    if (falling)                    return 'Slow decline — fair for now, watch the trend.';
    if (steady && high)             return 'Settled high — light winds, fair skies.';
    if (steady && low)              return 'Stalled low — unsettled, showers may persist.';
    if (steady)                     return 'No notable change — conditions should hold.';
    if (rising && a > 6)            return 'Sharp rise — gusty winds as the front clears.';
    if (rising && a > 3.5)          return low
      ? 'Clearing — winds easing, skies opening within hours.'
      : 'Building — fair weather strengthening, light to moderate breeze.';
    if (rising && a > 1.5)          return 'Improving — fair weather building over next 12 h.';
    return 'Slow recovery — trending toward fair.';
  }

  function tendencyLabel(delta) {
    const a = Math.abs(delta);
    const dir = delta > 0 ? 'Rising' : (delta < 0 ? 'Falling' : '');
    if (a < 0.1)  return 'Steady';
    if (a < 1.5)  return dir + ' slowly';
    if (a < 3.5)  return dir;
    if (a < 6)    return dir + ' quickly';
    return dir + ' very rapidly';
  }

  function pressureZone(p) {
    for (const z of ZONES) if (p < z.max) return z;
    return ZONES[ZONES.length - 1];
  }

  function makeTendencyArrow(delta) {
    const abs = Math.abs(delta);
    let rotation;
    if (abs < 0.1) rotation = 90;
    else {
      const steep = abs < 1.5 ? 60 : (abs < 3.5 ? 35 : (abs < 6 ? 18 : 6));
      rotation = delta > 0 ? steep : (180 - steep);
    }
    return `<svg viewBox="0 0 28 28" width="28" height="28">
      <g transform="rotate(${rotation} 14 14)">
        <line x1="14" y1="23" x2="14" y2="6" stroke="currentColor" stroke-width="2.4" stroke-linecap="round"/>
        <path d="M 8 11 L 14 5 L 20 11" fill="none" stroke="currentColor" stroke-width="2.4" stroke-linecap="round" stroke-linejoin="round"/>
      </g>
    </svg>`;
  }

  function twdToCompass(deg) {
    const dirs = ['N','NE','E','SE','S','SW','W','NW'];
    return dirs[Math.round((((deg % 360) + 360) % 360) / 45) % 8];
  }

  // ---------- CONTROLS -----------------------------------------------------
  function wireControls() {
    const pills = document.querySelectorAll('#baroRangePills .sub-tab');
    pills.forEach(btn => btn.addEventListener('click', () => {
      pills.forEach(b => b.classList.remove('active'));
      btn.classList.add('active');
      currentRangeHours = parseInt(btn.dataset.range, 10);
      if (baroPlot) refreshPlot();
    }));
    const cb = document.getElementById('baroAutoscaleY');
    if (cb) cb.addEventListener('change', e => {
      autoscaleY = e.target.checked;
      baroYManual = null;                       // the checkbox always wins over a typed range
      localStorage.removeItem('baroYManual');
      if (baroPlot) refreshPlot();
    });
  }

  // ---------- HELPERS ------------------------------------------------------
  function setText(id, t) { const e = document.getElementById(id); if (e) e.textContent = t; }
  function setHTML(id, h) { const e = document.getElementById(id); if (e) e.innerHTML = h; }
  function getCss(name) { return getComputedStyle(document.documentElement).getPropertyValue(name).trim(); }
  function hexAlpha(hex, a) {
    if (!hex || !hex.startsWith('#')) return hex;
    const r = parseInt(hex.slice(1,3), 16), g = parseInt(hex.slice(3,5), 16), b = parseInt(hex.slice(5,7), 16);
    return `rgba(${r},${g},${b},${a})`;
  }
})();

// ========================================================================
// LONG TERM PLOTS — native render of the on-device month-long ring.
// Mirrors the barometer pattern: lazy fetch of a binary ring on tab open,
// parse header + records, derive each record's time from a single lastEpoch +
// ring position (no per-record timestamp), render with uPlot. Supabase data
// tier (older than the local ring) stitches in later. Public: initLongTermPlots().
// Record layout MUST match firmware LongTermRecord (116 B).
// ========================================================================
(function () {
  'use strict';

  let ltLoaded = false;   // session cache: fetch the ring once per session
  let ltData = null;      // { t:[], n, lastEpoch, interval, fields:{ key:{min,max,avg}|{avg} } }

  function getCss(name) { return getComputedStyle(document.documentElement).getPropertyValue(name).trim(); }
  function hexAlpha(hex, a) {
    if (!hex || !hex.startsWith('#')) return hex || '#888';
    const r = parseInt(hex.slice(1,3),16), g = parseInt(hex.slice(3,5),16), b = parseInt(hex.slice(5,7),16);
    return `rgba(${r},${g},${b},${a})`;
  }

  // Envelope fields in struct order with the scale divisor to reach real units.
  const ENV = [
    ['battVolt',100], ['battCurr',10], ['altCurr',10], ['victronCurr',10],
    ['rpm',1], ['duty',100], ['altTemp',10], ['tempTherm',10],
    ['sog',100], ['tws',100], ['vmg',100], ['aws',100],
    ['awa',10], ['twa',10], ['heel',100], ['pitch',100]
  ];
  const AVG = [
    ['soc',10], ['baro',10], ['ambTemp',10], ['cog',10], ['heading',10],
    ['leeway',10], ['altZero',100]
  ];

  // Only a genuine power-off should break the trace. Cadence jitter or a few missed
  // samples must NOT insert a gap-marker — ltBin nulls a whole bin that contains one,
  // so over-eager gaps blank the chart at low zoom. Break only on big gaps (≥ ~20 min).
  function ltGapThreshold(interval) { return Math.max((interval || 600) * 4, 1200); }

  // Header (16 B LE): u16 head, u16 count, u16 capacity, u16 recordSize,
  // u32 lastEpoch, u32 intervalSec. Then capacity × recordSize raw records.
  function parseLT(buf) {
    const dv = new DataView(buf);
    if (buf.byteLength < 16) return null;
    const head      = dv.getUint16(0, true);
    const count     = dv.getUint16(2, true);
    const capacity  = dv.getUint16(4, true);
    const recSize   = dv.getUint16(6, true);
    const lastEpoch = dv.getUint32(8, true);
    const interval  = dv.getUint32(12, true) || 600;   // seconds between records
    // Body is `count` records in chronological order (the endpoint no longer sends the
    // whole ring), so the buffer is 16 + count*recSize.
    if (!recSize || buf.byteLength < 16 + count * recSize) return null;

    const fields = {};
    ENV.forEach(([k]) => fields[k] = { min:[], max:[], avg:[] });
    AVG.forEach(([k]) => fields[k] = { avg:[] });
    const t = [], isGap = [], stage = [];
    // Synthetic null entry to break the line across a real time gap (power-off period).
    const pushGap = (gt) => {
      t.push(gt); isGap.push(true); stage.push(null);
      ENV.forEach(([k]) => { fields[k].min.push(null); fields[k].max.push(null); fields[k].avg.push(null); });
      AVG.forEach(([k]) => fields[k].avg.push(null));
    };

    // Record layout (128 B): u32 timestamp @0, i32 lat @4, i32 lon @8, u32 validMask @12,
    // envelope [min,max,avg]×16 from @16, avg-only ×7 after, u8 chargeStage + pad.
    // Records arrive in chronological order (oldest first), so read them linearly.
    let prevTs = null;
    for (let i = 0; i < count; i++) {
      const base = 16 + i * recSize;
      const tsRaw = dv.getUint32(base, true);
      const valid = dv.getUint32(base + 12, true);
      // Use the record's real timestamp (legit axis); fall back to position-derived if unsynced.
      const ts = tsRaw > 0 ? tsRaw : (lastEpoch ? lastEpoch - (count - 1 - i) * interval : i * interval);
      if (prevTs != null && (ts - prevTs) > ltGapThreshold(interval)) pushGap((prevTs + ts) / 2);  // gap → break
      prevTs = ts;

      t.push(ts); isGap.push(false);
      let off = base + 16;   // envelope block: [min,max,avg] int16 each
      ENV.forEach(([k, scale], fi) => {
        const f = fields[k];
        if (valid & (1 << fi)) {
          f.min.push(dv.getInt16(off, true) / scale);
          f.max.push(dv.getInt16(off + 2, true) / scale);
          f.avg.push(dv.getInt16(off + 4, true) / scale);
        } else { f.min.push(null); f.max.push(null); f.avg.push(null); }
        off += 6;
      });
      let ao = base + 16 + ENV.length * 6;   // avg-only block: one int16 each
      AVG.forEach(([k, scale], ai) => {
        fields[k].avg.push((valid & (1 << (ENV.length + ai))) ? dv.getInt16(ao, true) / scale : null);
        ao += 2;
      });
      // chargeStage u8 sits right after the avg-only block (display code 0-7).
      stage.push(dv.getUint8(base + 16 + ENV.length * 6 + AVG.length * 2));
    }
    return { t, n: t.length, lastEpoch, interval, fields, isGap, stage };
  }

  // Charge-stage palette — matches the live `.charge-stage-*` badge colors in styles.css.
  // Codes per firmware LongTermRecord.chargeStage: 1 bulk,2 absorption,3 float,4 manual,
  // 5 maintain,6 targetV,7 idle (0 = off/none → bare track).
  const LT_STAGE_COLORS = { 1:'#00c853', 2:'#7e57c2', 3:'#ffb300', 4:'#ef5350', 5:'#66bb6a', 6:'#42a5f5', 7:'#78909c' };
  const LT_STAGE_NAMES  = { 1:'Bulk', 2:'Absorption', 3:'Float', 4:'Manual', 5:'Maintain', 6:'Target V', 7:'Idle' };

  // ---- Decimation (ported from the cloud viewer's applyBinning) ----------------
  const LT_MAX_BINS = 120;   // matches the viewer's cap: ≤120 points drawn per chart
  const ltCharts = [];       // [{ id, plot, spec }] — built once, fed binned data
  let ltFullRange = null;    // [minSec, maxSec] of the whole ring
  let ltCurRange = null;     // [minSec, maxSec] currently rendered (for re-render after Y-axis edits)
  let ltRendering = false;   // re-entrancy guard so our setScale doesn't re-fire the zoom hook

  // Per-chart manual Y-axis overrides, persisted: { chartId: { scaleName: [min,max] } }.
  // Enforced every render in ltRenderAll; absent → that axis auto-fits to visible data.
  let ltYOverride = {};
  try { ltYOverride = JSON.parse(localStorage.getItem('lt_yaxis') || '{}') || {}; } catch (e) { ltYOverride = {}; }
  function ltSaveYOverride() { try { localStorage.setItem('lt_yaxis', JSON.stringify(ltYOverride)); } catch (e) {} }

  // Persist the long-term time-window CHOICE (re-anchored to newest data on reload, not an absolute range).
  let ltWindowPref = null;   // { hours: <n> } | { all: true } | null
  try { ltWindowPref = JSON.parse(localStorage.getItem('lt_window') || 'null'); } catch (e) { ltWindowPref = null; }
  function ltSaveWindowPref(p) { ltWindowPref = p; try { localStorage.setItem('lt_window', JSON.stringify(p)); } catch (e) {} }

  // Persist per-chart legend series visibility: { chartId: { seriesKey: true } } where true = hidden.
  let ltSeriesHidden = {};
  try { ltSeriesHidden = JSON.parse(localStorage.getItem('lt_series_hidden') || '{}') || {}; } catch (e) { ltSeriesHidden = {}; }
  function ltSaveSeriesHidden() { try { localStorage.setItem('lt_series_hidden', JSON.stringify(ltSeriesHidden)); } catch (e) {} }

  // Equal-width bins over [fromSec,toSec], capped at maxBins, ENVELOPE-PRESERVING:
  // bin min = min of mins, bin max = max of maxes, bin avg = avg of avgs (nulls skipped).
  // Re-run per visible range so zoomed views show finer detail with bounded point count.
  function ltBin(fromSec, toSec, maxBins) {
    const t = ltData.t, N = ltData.n;
    let lo = 0, hi = N;
    while (lo < N && t[lo] < fromSec) lo++;
    while (hi > lo && t[hi - 1] > toSec) hi--;
    const out = { t: [], fields: {} };
    ENV.forEach(([k]) => out.fields[k] = { min: [], max: [], avg: [] });
    AVG.forEach(([k]) => out.fields[k] = { avg: [] });
    const vis = hi - lo;
    if (vis <= 0) return out;
    const binSize = Math.max(1, Math.ceil(vis / maxBins));
    const ig = ltData.isGap || [];
    for (let i = lo; i < hi; i += binSize) {
      const end = Math.min(i + binSize, hi);
      out.t.push(t[i + ((end - i) >> 1)]);   // mid-bin timestamp
      // A bin spanning a gap marker is nulled wholesale → the line breaks there.
      let hasGap = false;
      for (let j = i; j < end; j++) if (ig[j]) { hasGap = true; break; }
      if (hasGap) {
        ENV.forEach(([k]) => { const o = out.fields[k]; o.min.push(null); o.max.push(null); o.avg.push(null); });
        AVG.forEach(([k]) => out.fields[k].avg.push(null));
        continue;
      }
      ENV.forEach(([k]) => {
        const f = ltData.fields[k], o = out.fields[k];
        let mn = Infinity, mx = -Infinity, sum = 0, cnt = 0;
        for (let j = i; j < end; j++) if (f.avg[j] != null) {
          const lo = f.min[j], hi = f.max[j];
          if (lo != null && lo < mn) mn = lo;   // skip null min/max (sentinel-cleaned cloud rows)
          if (hi != null && hi > mx) mx = hi;    // so Infinity never leaks into the band → uPlot scale
          sum += f.avg[j]; cnt++;
        }
        o.avg.push(cnt ? sum / cnt : null);
        o.min.push(isFinite(mn) ? mn : null);    // finite-or-null only
        o.max.push(isFinite(mx) ? mx : null);
      });
      AVG.forEach(([k]) => {
        const f = ltData.fields[k], o = out.fields[k];
        let sum = 0, cnt = 0;
        for (let j = i; j < end; j++) if (f.avg[j] != null) { sum += f.avg[j]; cnt++; }
        o.avg.push(cnt ? sum / cnt : null);
      });
    }
    return out;
  }

  // Keep compass/wind angles continuous so a 360→0 wrap doesn't draw a vertical crash.
  function unwrapAngles(arr) {
    const out = []; let prev = null;
    for (const v of arr) {
      if (v == null) { out.push(null); prev = null; continue; }
      let x = v;
      if (prev != null) { while (x - prev > 180) x -= 360; while (x - prev < -180) x += 360; }
      out.push(x); prev = x;
    }
    return out;
  }

  // Re-bin the ring to [fromSec,toSec] and push to every chart (synced X; Y auto-fits to
  // the visible binned data). The guard stops our setScale from re-triggering the hook.
  function ltRenderAll(fromSec, toSec) {
    if (!ltData || !ltCharts.length) return;
    // A zero/negative-width x-range makes uPlot null out its scales and draw nothing.
    if (!(toSec > fromSec)) { const c = fromSec; fromSec = c - 1800; toSec = c + 1800; }
    ltRendering = true;
    ltCurRange = [fromSec, toSec];
    const b = ltBin(fromSec, toSec, LT_MAX_BINS);
    ltCharts.forEach(c => {
      const data = [ b.t ];
      const scaleVals = {};   // scaleName → arrays feeding that scale (for explicit range calc)
      c.spec.series.forEach(s => {
        const f = b.fields[s.key];
        const avg = s.unwrap ? unwrapAngles(f.avg) : f.avg;
        data.push(avg);
        (scaleVals[s.scale] = scaleVals[s.scale] || []).push(avg);
        if (s.band) { data.push(f.max); data.push(f.min); scaleVals[s.scale].push(f.max, f.min); }
      });
      c.plot.setData(data);          // rebuilds series paths (auto-scale may null out; we override below)
      c.plot.setScale('x', { min: fromSec, max: toSec });
      // Set every Y-scale EXPLICITLY from the visible data. uPlot's own auto-range was
      // intermittently leaving scales null (→ nothing drawn), so we compute min/max here.
      const ov = ltYOverride[c.id] || {};
      c.spec.scales.forEach(sc => {
        if (ov[sc.name]) { c.plot.setScale(sc.name, { min: ov[sc.name][0], max: ov[sc.name][1] }); return; }
        if (sc.range) { c.plot.setScale(sc.name, { min: sc.range[0], max: sc.range[1] }); return; }  // fixed (e.g. SOC 0-100)
        let mn = Infinity, mx = -Infinity;
        (scaleVals[sc.name] || []).forEach(arr => { for (const v of arr) { if (v != null && isFinite(v)) { if (v < mn) mn = v; if (v > mx) mx = v; } } });
        if (isFinite(mn) && isFinite(mx)) {
          if (mn === mx) { mn -= 1; mx += 1; }
          const pad = (mx - mn) * 0.05;
          c.plot.setScale(sc.name, { min: mn - pad, max: mx + pad });
        }
      });
    });
    ltRendering = false;
    ltDrawStageBar();   // repaint the charge-stage strip over the same range
    // Reflect the rendered range back onto the brush selection (keeps the brush in sync
    // whether the range came from the brush itself or from a chart drag-zoom).
    if (_brushState && _brushState.nativeSink) {
      _brushState.from = fromSec * 1000; _brushState.to = toSec * 1000;
      if (typeof _brushUpdateSelectionUI === 'function') _brushUpdateSelectionUI();
    }
    ltCloudMaybePull(fromSec);   // brushed past the loaded edge → stitch in older cloud data
  }
  const ltZoomDebounced = debounce((min, max) => { if (min != null && max != null) ltRenderAll(min, max); }, 120);

  // Hand the relocated dashboard-brush its data bounds + a sink into ltRenderAll, wire
  // it once, and show it. Brush works in ms; ltRenderAll in seconds.
  function ltInitBrush(minSec, maxSec, latestSec) {
    if (typeof _brushState === 'undefined' || !_brushState) return;
    _brushState.nativeSink = (fromMs, toMs) => ltRenderAll(fromMs / 1000, toMs / 1000);
    _brushState.ltSaveWindowPref = ltSaveWindowPref;   // bridge: snap handler is top-level, can't see the IIFE-private fn
    _brushState.dataMin = minSec * 1000;
    _brushState.dataMax = maxSec * 1000;
    _brushState.latestSample = (latestSec || maxSec) * 1000;
    _brushState.from = _brushState.dataMin;
    _brushState.to = _brushState.dataMax;
    if (!_brushState.wired) { _brushWireDrag(); _brushWireSnap(); _brushState.wired = true; }
    const bEl = document.getElementById('dashboard-brush'); if (bEl) bEl.style.display = 'block';
    const mn = document.getElementById('dashboard-brush-data-min'); if (mn) mn.textContent = _brushFormatTime(_brushState.dataMin);
    const mx = document.getElementById('dashboard-brush-data-max'); if (mx) mx.textContent = _brushFormatTime(_brushState.dataMax);
    // Cadence note in the sticky brush bar — true rate from the .bin header, not a hardcode
    const sn = document.getElementById('dashboard-sample-note');
    if (sn && ltData && ltData.interval) sn.textContent = 'Sampled every ' + Math.round(ltData.interval / 60) + ' min';
    _brushUpdateSelectionUI();
    // Re-anchored restore: apply the saved window choice relative to the NEWEST data, not an absolute old range.
    if (ltWindowPref) {
      let from = _brushState.dataMin, to = _brushState.dataMax;
      if (ltWindowPref.hours) {
        const t = Math.min(_brushState.dataMax, Date.now());
        from = Math.max(_brushState.dataMin, t - ltWindowPref.hours * 3600 * 1000);
        to = t;
      }
      // Reflect the active snap button visually if one matches the saved choice.
      try {
        document.querySelectorAll('.dashboard-brush-snap-btn').forEach(b => {
          const isAll = b.getAttribute('data-all') === 'true';
          const h = parseInt(b.getAttribute('data-hours'), 10);
          const match = ltWindowPref.all ? isAll : (isFinite(h) && h === ltWindowPref.hours);
          if (match) b.classList.add('active');
        });
      } catch (e) {}
      _brushApplyRange(from, to);   // same path the snap buttons use → drives ltRenderAll
    }
    _brushUpdateStaleness();
    if (_brushState.stalenessTimerId == null) {
      _brushState.stalenessTimerId = (typeof setTrackedInterval === 'function' ? setTrackedInterval : setInterval)(_brushUpdateStaleness, 10000);
    }
    const clr = document.getElementById('dashboard-crosshair-clear');
    if (clr && !clr._ltWired) { clr._ltWired = true; clr.addEventListener('click', () => { ltPinnedTime = null; ltRedrawMarkers(); ltUpdateCrosshairIndicator(); }); }
  }

  // ---- Markers: live-edge "now" line + a tap-to-pin crosshair synced across charts ----
  let ltPinnedTime = null;   // pinned crosshair time (sec); null = none
  function ltDrawMarkers(u) {
    const ctx = u.ctx, xmin = u.scales.x.min, xmax = u.scales.x.max;
    const vline = (tSec, color, dash) => {
      if (tSec == null || !isFinite(tSec) || tSec < xmin || tSec > xmax) return;
      const x = Math.round(u.valToPos(tSec, 'x', true)) + 0.5;
      ctx.save();
      ctx.strokeStyle = color; ctx.lineWidth = 1; ctx.setLineDash(dash);
      ctx.beginPath(); ctx.moveTo(x, u.bbox.top); ctx.lineTo(x, u.bbox.top + u.bbox.height); ctx.stroke();
      ctx.restore();
    };
    vline(Math.floor(Date.now() / 1000), 'rgba(244,67,54,0.55)', [4, 3]);   // live edge "now"
    vline(ltPinnedTime, 'rgba(60,60,60,0.85)', []);                          // pinned crosshair
  }
  function ltRedrawMarkers() { ltCharts.forEach(c => { try { c.plot.redraw(false); } catch (e) {} }); }
  function ltUpdateCrosshairIndicator() {
    const ind = document.getElementById('dashboard-crosshair-indicator');
    const txt = document.getElementById('dashboard-crosshair-time');
    if (!ind || !txt) return;
    if (ltPinnedTime == null) { ind.style.display = 'none'; txt.textContent = '—'; }
    else { ind.style.display = 'inline-block'; txt.textContent = (typeof _brushFormatTime === 'function') ? _brushFormatTime(ltPinnedTime * 1000) : new Date(ltPinnedTime * 1000).toLocaleString(); }
  }

  // Build one chart ONCE (empty); ltRenderAll feeds it binned data. drag-x zoom →
  // re-bins ALL charts (synced); dbl-click resets the range; click pins a crosshair.
  // Bottom legend shows live values at the cursor (setCursor hook).
  function buildLtChart(id, spec) {
    const el = document.getElementById(id);
    if (!el || typeof uPlot === 'undefined') return;
    el.innerHTML = '';
    const series = [ { label: 'Time' } ], bands = [], initData = [ [] ], avgIdx = [];
    const seriesGroups = [];   // per spec-series: uPlot series indices (avg + band edges) to toggle together
    let di = 1;
    spec.series.forEach(s => {
      const group = [di];
      avgIdx.push(di);
      series.push({ label: s.label, scale: s.scale, stroke: s.color, width: 2, points: { show: false }, spanGaps: false });
      initData.push([]); di++;
      if (s.band) {
        const maxIdx = series.length; series.push({ scale: s.scale, stroke: 'transparent', width: 0, points: { show: false } });
        const minIdx = series.length; series.push({ scale: s.scale, stroke: 'transparent', width: 0, points: { show: false } });
        bands.push({ series: [maxIdx, minIdx], fill: hexAlpha(s.color, 0.14) });
        initData.push([]); initData.push([]); di += 2;
        group.push(maxIdx, minIdx);
      }
      seriesGroups.push(group);
    });
    const scales = { x: { time: true } };
    const axes = [ { grid: { show: true } } ];
    spec.scales.forEach(sc => {
      scales[sc.name] = sc.range ? { auto: false, range: sc.range } : { auto: true };
      axes.push({ scale: sc.name, label: sc.label, side: sc.side, grid: { show: sc.side === 3 },
                  splits: edgeLabeledSplits(() => !!(ltYOverride[id] && ltYOverride[id][sc.name])),
                  values: sc.fmt ? (u, ticks) => ticks.map(sc.fmt) : undefined });
    });

    // Bottom legend with live value readouts (filled by the setCursor hook on hover).
    const legendEl = document.createElement('div');
    legendEl.style.cssText = 'display:flex;justify-content:center;gap:14px;flex-wrap:wrap;margin-top:8px;font-size:12px;';
    const valSpans = [];
    spec.series.forEach((s, i) => {
      const item = document.createElement('span');
      item.style.cssText = 'display:inline-flex;align-items:center;gap:5px;cursor:pointer;user-select:none;';
      item.title = 'Click to show/hide';
      const sw = document.createElement('span');
      sw.style.cssText = 'width:11px;height:11px;border-radius:2px;display:inline-block;background:' + s.color + ';';
      const val = document.createElement('b');
      item.appendChild(sw); item.appendChild(document.createTextNode(s.label + ': ')); item.appendChild(val);
      // Click the legend item to toggle that series (and its envelope band) on/off.
      item.addEventListener('click', () => {
        const show = !plot.series[seriesGroups[i][0]].show;
        seriesGroups[i].forEach(idx => plot.setSeries(idx, { show }));
        item.style.opacity = show ? '1' : '0.4';
        // Persist visibility by stable series key (true = hidden).
        ltSeriesHidden[id] = ltSeriesHidden[id] || {};
        if (show) delete ltSeriesHidden[id][s.key]; else ltSeriesHidden[id][s.key] = true;
        ltSaveSeriesHidden();
      });
      // Restore persisted visibility (keyed by series key). Dim the legend item now;
      // the matching plot.setSeries() runs after the plot is created (see below).
      if (ltSeriesHidden[id] && ltSeriesHidden[id][s.key]) {
        item.style.opacity = '0.4';
      }
      legendEl.appendChild(item);
      valSpans.push(val);
    });

    const opts = {
      title: spec.title, width: Math.max(el.clientWidth, 320), height: 300,
      series, scales, axes, bands, legend: { show: false },
      cursor: { sync: { key: 'ltsync' }, y: false, drag: { x: true, y: false } },   // x crosshair synced across charts; no horizontal y line
      hooks: {
        setScale: [ (u, key) => { if (key === 'x' && !ltRendering) { const sx = u.scales.x; ltZoomDebounced(sx.min, sx.max); } } ],
        setCursor: [ (u) => {
          const idx = u.cursor.idx;
          for (let i = 0; i < valSpans.length; i++) {
            const arr = u.data[avgIdx[i]];
            const v = (idx != null && arr) ? arr[idx] : null;
            valSpans[i].textContent = (v == null) ? '—' : v.toFixed(Math.abs(v) >= 100 ? 0 : 1);
          }
        } ],
        draw: [ (u) => ltDrawMarkers(u) ]
      }
    };
    const plot = new uPlot(opts, initData, el);
    el.appendChild(legendEl);
    ltCharts.push({ id, plot, spec });

    // Apply persisted legend hides now that the plot exists (opacity was set in the loop above).
    if (ltSeriesHidden[id]) {
      spec.series.forEach((s, i) => {
        if (ltSeriesHidden[id][s.key]) seriesGroups[i].forEach(idx => plot.setSeries(idx, { show: false }));
      });
    }

    // Y-axis controls: per-scale manual Min/Max + Apply + Auto (reset). Persisted; absent
    // → that axis auto-fits to the visible binned data (enforced in ltRenderAll).
    const ctrl = document.createElement('div');
    ctrl.style.cssText = 'display:flex;justify-content:center;align-items:center;gap:8px;flex-wrap:wrap;margin-top:4px;font-size:11px;color:#888;';
    ctrl.appendChild(document.createTextNode('Y-axis:'));
    const saved = ltYOverride[id] || {};
    const inputs = {};
    spec.scales.forEach(sc => {
      const wrap = document.createElement('span');
      wrap.style.cssText = 'display:inline-flex;align-items:center;gap:3px;';
      const lbl = document.createElement('span'); lbl.textContent = sc.label;
      const mn = document.createElement('input'); mn.type = 'number'; mn.placeholder = 'min'; mn.style.width = '70px';
      const mx = document.createElement('input'); mx.type = 'number'; mx.placeholder = 'max'; mx.style.width = '70px';
      if (saved[sc.name]) { mn.value = saved[sc.name][0]; mx.value = saved[sc.name][1]; }
      wrap.appendChild(lbl); wrap.appendChild(mn); wrap.appendChild(mx);
      ctrl.appendChild(wrap);
      inputs[sc.name] = { mn, mx };
    });
    const applyBtn = document.createElement('button'); applyBtn.textContent = 'Apply'; applyBtn.className = 'dashboard-brush-snap-btn';
    const resetBtn = document.createElement('button'); resetBtn.textContent = '⤢ Auto'; resetBtn.className = 'dashboard-brush-snap-btn';
    applyBtn.addEventListener('click', () => {
      const ov = {};
      spec.scales.forEach(sc => {
        const a = parseFloat(inputs[sc.name].mn.value), b2 = parseFloat(inputs[sc.name].mx.value);
        if (isFinite(a) && isFinite(b2) && b2 > a) ov[sc.name] = [a, b2];
      });
      if (Object.keys(ov).length) ltYOverride[id] = ov; else delete ltYOverride[id];
      ltSaveYOverride();
      if (ltCurRange) ltRenderAll(ltCurRange[0], ltCurRange[1]);
    });
    resetBtn.addEventListener('click', () => {
      delete ltYOverride[id];
      spec.scales.forEach(sc => { inputs[sc.name].mn.value = ''; inputs[sc.name].mx.value = ''; });
      ltSaveYOverride();
      if (ltCurRange) ltRenderAll(ltCurRange[0], ltCurRange[1]);
    });
    ctrl.appendChild(applyBtn); ctrl.appendChild(resetBtn);
    el.appendChild(ctrl);

    // On-plot Y boxes mirror the Min/Max fields above — same persistence path
    // (clear both boxes + Enter returns that axis to auto-fit).
    attachYAxisEdit(plot, spec.scales.map(sc => ({
      scale: sc.name,
      apply: (mn, mx) => { inputs[sc.name].mn.value = mn; inputs[sc.name].mx.value = mx; applyBtn.click(); },
      auto: () => { inputs[sc.name].mn.value = ''; inputs[sc.name].mx.value = ''; applyBtn.click(); }
    })));

    if (document.body.classList.contains('dark-mode') && typeof updateUplotTheme === 'function') updateUplotTheme(plot);
    plot.over.addEventListener('click', () => {   // tap to pin a crosshair time across all charts
      const idx = plot.cursor.idx;
      if (idx != null && plot.data[0]) { ltPinnedTime = plot.data[0][idx]; ltRedrawMarkers(); ltUpdateCrosshairIndicator(); }
    });
    el.addEventListener('dblclick', () => { if (ltFullRange) ltRenderAll(ltFullRange[0], ltFullRange[1]); });
    if (!el._ltRO) {
      el._ltRO = new ResizeObserver(debounce(() => plot.setSize({ width: el.clientWidth, height: 300 }), 500));
      el._ltRO.observe(el);
    }
  }

  function buildAllLtPlots() {
    if (!ltCharts.length) {
    // Groupings mirror the cloud "My History" viewer minus the eliminated fields
    // (u_target_amps, temp_margin, ign_duty, eng_duty). Every envelope-recorded field
    // gets band:true; line-only series (SOC, Board temp, Baro, COG, Heading, Leeway)
    // are avg-only in the 128 B record itself, so a band would need a record-layout
    // change. heel/pitch (Motion) is a local-only bonus.
    buildLtChart('lt-current-plot', {
      title: 'Currents (A)',
      scales: [ { name:'A', label:'Amps', side:3 } ],
      series: [ { key:'battCurr',    scale:'A', color:'#FF9800', label:'Battery (A)',       band:true },
                { key:'altCurr',     scale:'A', color:'#4CAF50', label:'Alternator (A)',    band:true },
                { key:'victronCurr', scale:'A', color:'#2196F3', label:'Victron/Solar (A)', band:true } ]
    });
    buildLtChart('lt-voltage-plot', {
      title: 'Battery Voltage & SOC',
      scales: [ { name:'V', label:'Volts', side:3 },
                { name:'pct', label:'SOC %', side:1, range:[0,100], fmt:v=>Math.round(v)+'%' } ],
      series: [ { key:'battVolt', scale:'V', color:'#4CAF50', label:'Battery (V)', band:true },
                { key:'soc', scale:'pct', color:'#2196F3', label:'SOC (%)' } ]
    });
    buildLtChart('lt-temp-plot', {
      title: 'Temperatures & Barometric Pressure',
      scales: [ { name:'F', label:'°F', side:3 },
                { name:'mb', label:'mbar', side:1 } ],
      series: [ { key:'altTemp',   scale:'F',  color:'#F44336', label:'Alternator (°F)', band:true },
                { key:'tempTherm', scale:'F',  color:'#FF9800', label:'Thermistor (°F)', band:true },
                { key:'ambTemp',   scale:'F',  color:'#2196F3', label:'Board (°F)' },
                { key:'baro',      scale:'mb', color:'#9C27B0', label:'Baro (mbar)' } ]
    });
    buildLtChart('lt-rpm-plot', {
      title: 'Engine RPM & Alternator Duty',
      scales: [ { name:'rpm', label:'RPM', side:3 },
                { name:'pct', label:'Duty %', side:1, range:[0,100], fmt:v=>Math.round(v)+'%' } ],
      series: [ { key:'rpm',  scale:'rpm', color:'#9C27B0', label:'RPM', band:true },
                { key:'duty', scale:'pct', color:'#9E9E9E', label:'Duty (%)', band:true } ]
    });
    buildLtChart('lt-wind-plot', {
      title: 'Speeds (kt)',
      scales: [ { name:'kt', label:'Knots', side:3 } ],
      series: [ { key:'sog', scale:'kt', color:'#4CAF50', label:'SOG (kt)',        band:true },
                { key:'tws', scale:'kt', color:'#2196F3', label:'True Wind (kt)',  band:true },
                { key:'vmg', scale:'kt', color:'#9C27B0', label:'VMG (kt)',        band:true },
                { key:'aws', scale:'kt', color:'#00BCD4', label:'App. Wind (kt)',  band:true } ]
    });
    buildLtChart('lt-angles-plot', {
      title: 'Angles (°)',
      scales: [ { name:'deg', label:'Degrees', side:3 } ],
      series: [ { key:'cog',     scale:'deg', color:'#2196F3', label:'COG (°)',     unwrap:true },
                { key:'heading', scale:'deg', color:'#FF9800', label:'Heading (°)', unwrap:true },
                { key:'awa',     scale:'deg', color:'#4CAF50', label:'AWA (°)',  band:true },
                { key:'twa',     scale:'deg', color:'#E91E63', label:'TWA (°)',  band:true },
                { key:'leeway',  scale:'deg', color:'#00BCD4', label:'Leeway (°)' } ]
    });
    buildLtChart('lt-motion-plot', {
      title: 'Motion — Heel & Pitch (°)',
      scales: [ { name:'deg', label:'Degrees', side:3 } ],
      series: [ { key:'heel',  scale:'deg', color:'#3F51B5', label:'Heel (°)',  band:true },
                { key:'pitch', scale:'deg', color:'#FF5722', label:'Pitch (°)', band:true } ]
    });
    ltBuildStageLegend();

    }
    // When the local ring holds little (< ~1h span — always true right after an OTA
    // reboot until the field-off dump persists it), lean on cloud: anchor the view to the
    // last 24h and auto-pull cloud history. Otherwise show the full local span.
    const nowSec = Math.floor(Date.now() / 1000);
    const localSpan = (ltData.n >= 2) ? (ltData.t[ltData.n - 1] - ltData.t[0]) : 0;
    const sparse = localSpan < 3600;
    const oldestBoundary = (ltData.n > 0) ? ltData.t[0] : nowSec;   // cloud fills below this
    const loSec = sparse ? (nowSec - 86400) : ltData.t[0];
    const hiSec = sparse ? nowSec : ltData.t[ltData.n - 1];
    ltFullRange = [loSec, hiSec];
    // Order matters: brush (sets nativeSink) → cloud state (reach) → render. The render's
    // ltCloudMaybePull then fires exactly ONE pull (loSec < oldest in the sparse case).
    ltInitBrush(loSec, hiSec, sparse ? nowSec : (ltData.lastEpoch || hiSec));
    ltCloudInit(oldestBoundary);
    ltRenderAll(loSec, hiSec);
    // Force each chart to its real container width + repaint. Charts can get stuck at
    // the 320px fallback if first built before the sub-tab had laid out; the per-chart
    // ResizeObserver never corrects it because `el` itself doesn't change size. Run on
    // every open (idempotent) so a reopen also fixes any stale narrow sizing.
    requestAnimationFrame(() => requestAnimationFrame(ltResizeAll));
    setTimeout(ltResizeAll, 250);   // fallback for late layout (card width applied after activate)
  }

  function ltResizeAll() {
    ltCharts.forEach(c => {
      const el = document.getElementById(c.id);
      if (!el) return;
      // Size to the parent CARD's content width, not the .plot-container's — uPlot's root
      // is `width: min-content`, so the container collapses to ~the canvas width (e.g. 324px)
      // and reading it just re-pins the narrow size. The card is a full-width block.
      const card = el.closest('.settings-card');
      const w = Math.floor((card ? card.clientWidth : el.clientWidth)) - 22;  // minus card padding
      if (w > 50) c.plot.setSize({ width: w, height: 300 });
    });
    ltDrawStageBar();   // chart widths/x-gutter just changed → realign the stage strip
  }

  // Fill the charge-stage legend once (swatch + label per code) from the shared palette.
  function ltBuildStageLegend() {
    const el = document.getElementById('lt-stage-legend');
    if (!el || el.childElementCount) return;
    Object.keys(LT_STAGE_NAMES).forEach(code => {
      const item = document.createElement('span');
      item.style.cssText = 'display:inline-flex;align-items:center;gap:4px;';
      const sw = document.createElement('span');
      sw.style.cssText = 'width:11px;height:11px;border-radius:2px;display:inline-block;background:' + LT_STAGE_COLORS[code] + ';';
      item.appendChild(sw); item.appendChild(document.createTextNode(LT_STAGE_NAMES[code]));
      el.appendChild(item);
    });
  }

  // Paint the charge-stage strip over the currently-rendered range. One bar (not per-chart
  // shading); its painted region is x-aligned to a reference chart's plot area so it sits
  // under the curves, past the y-axis gutter. Stage 0/off and gaps are left as bare track.
  function ltDrawStageBar() {
    const cv = document.getElementById('lt-stage-bar');
    if (!cv || !ltData || !ltData.stage || !ltCurRange) return;
    const fromSec = ltCurRange[0], toSec = ltCurRange[1];
    const card = cv.closest('.settings-card');
    const cssW = Math.max((card ? card.clientWidth : cv.clientWidth) - 22, 50);
    const cssH = 18, dpr = window.devicePixelRatio || 1;
    cv.style.width = cssW + 'px'; cv.style.height = cssH + 'px';
    cv.width = Math.round(cssW * dpr); cv.height = Math.round(cssH * dpr);
    const ctx = cv.getContext('2d');
    ctx.setTransform(dpr, 0, 0, dpr, 0, 0);
    ctx.clearRect(0, 0, cssW, cssH);
    // Align the painted span to a reference chart's plotting area (uPlot adds a y-axis gutter).
    let x0 = 0, x1 = cssW;
    const ref = ltCharts.length ? ltCharts[0].plot : null;
    if (ref && ref.over) {
      const oRect = ref.over.getBoundingClientRect(), cRect = cv.getBoundingClientRect();
      const a = oRect.left - cRect.left, b = oRect.right - cRect.left;
      if (b > a && a >= -2 && b <= cssW + 2) { x0 = Math.max(a, 0); x1 = Math.min(b, cssW); }
    }
    const span = (toSec - fromSec) || 1;
    ctx.fillStyle = 'rgba(128,128,128,0.15)';   // bare track behind everything
    ctx.fillRect(x0, 0, x1 - x0, cssH);
    // Per-pixel DOMINANT stage (no binning, never microscopic): accumulate each stage run's
    // time-overlap into the pixel column(s) it covers, then paint each column the stage that
    // held it longest. Stage 0/off (and no-data) winning a column → bare track. Zooming in
    // shrinks each column's time slice, so short stages emerge instead of aliasing at zoom-out.
    const t = ltData.t, st = ltData.stage, ig = ltData.isGap || [], N = ltData.n;
    const interval = ltData.interval || 600;
    const W = x1 - x0, pxN = Math.max(1, Math.round(W));
    const acc = new Float32Array(pxN * 8);   // [pixel*8 + stageCode] → accumulated width (∝ time)
    let i = 0;
    while (i < N) {
      if (ig[i] || st[i] == null) { i++; continue; }
      const s = st[i];
      let j = i;   // merge consecutive same-stage records to cover many columns in one pass
      while (j + 1 < N && !ig[j + 1] && st[j + 1] === s) j++;
      let segStart = t[i], segEnd = (j + 1 < N) ? t[j + 1] : t[j] + interval;
      i = j + 1;
      if (s < 0 || s > 7 || segEnd <= fromSec || segStart >= toSec) continue;
      if (segStart < fromSec) segStart = fromSec;
      if (segEnd > toSec) segEnd = toSec;
      const pa = (segStart - fromSec) / span * W, pb = (segEnd - fromSec) / span * W;
      const p0 = Math.max(0, Math.floor(pa)), p1 = Math.min(pxN - 1, Math.ceil(pb) - 1);
      for (let p = p0; p <= p1; p++) {
        const lo = Math.max(pa, p), hi = Math.min(pb, p + 1);
        if (hi > lo) acc[p * 8 + s] += (hi - lo);
      }
    }
    const dom = new Uint8Array(pxN);   // dominant stage per column (0 = off/no-data → track)
    for (let p = 0; p < pxN; p++) {
      let best = 0, bestV = 0;
      for (let sc = 0; sc <= 7; sc++) { const v = acc[p * 8 + sc]; if (v > bestV) { bestV = v; best = sc; } }
      dom[p] = bestV > 0 ? best : 0;
    }
    let p = 0;   // merge adjacent same-stage columns into one rect (no internal seams)
    while (p < pxN) {
      const d = dom[p];
      if (d < 1 || !LT_STAGE_COLORS[d]) { p++; continue; }
      let q = p; while (q + 1 < pxN && dom[q + 1] === d) q++;
      ctx.fillStyle = LT_STAGE_COLORS[d];
      ctx.fillRect(x0 + p, 0, q - p + 1, cssH);
      p = q + 1;
    }
  }

  function ltStatus(msg) {
    const status = document.getElementById('longterm-status');
    if (!status) return;
    const card = status.closest('.settings-card') || status;
    if (msg) { status.textContent = msg; card.style.display = ''; }
    else { card.style.display = 'none'; }   // hide the whole card, not just the text
  }

  // ======================================================================
  // CLOUD STITCH — history older than the local ring (LOCAL_DATA_SYSTEMS_PLAN 1b).
  // Lazy + cloud-gated: when Cloud Features is on and the device is registered,
  // brushing past the oldest local record pulls a matching 10-min min/max/avg
  // aggregate from Supabase (get-history, date-range) and PREPENDS it into ltData,
  // so it renders as one continuous band+line across the SAME 7 charts. Pulled in
  // 30-day chunks, each older than the last; reaching the loaded edge opens + fetches
  // the next chunk. When cloud is off/unregistered the pre-ring region simply stays
  // empty (one-line hint), never an error, and the in-ring month is never greyed.
  // ======================================================================
  const LT_CLOUD_CHUNK_S        = 30 * 86400;    // pull 30 days of older history per request
  const LT_CLOUD_MAX_LOOKBACK_S = 365 * 86400;   // stop pulling beyond ~1 year back
  // JS field key → sensor_history column prefix (envelope: _min/_max/_avg; avg-only: _avg).
  const LT_CLOUD_COL = {
    battVolt:'batt_volt', battCurr:'batt_curr', altCurr:'alt_curr', victronCurr:'victron_curr',
    rpm:'rpm', duty:'duty_cycle', altTemp:'alt_temp', tempTherm:'temp_therm',
    sog:'sog', tws:'tws', vmg:'vmg', aws:'aws', awa:'awa', twa:'twa',
    heel:'imu_heel', pitch:'imu_pitch',
    soc:'soc', baro:'baro', ambTemp:'amb_temp', cog:'cog', heading:'heading',
    leeway:'leeway', altZero:'alt_zero'
  };
  let ltCloud = { avail: undefined, fetching: false, oldest: null, exhausted: false };

  function ltCloudEnabled() {
    // Source of truth is the checkbox — the settings echo (updateCheckbox) keeps
    // CloudFeatures_checkbox.checked current but does NOT write the hidden
    // #CloudFeatures input's .value (it stays "0"), so reading that gave false negatives.
    const cb = document.getElementById('CloudFeatures_checkbox');
    if (cb) return !!cb.checked;
    const cf = document.getElementById('CloudFeatures');
    return !!cf && cf.value === '1';
  }
  function ltIso(sec) { return new Date(sec * 1000).toISOString().replace(/\.\d+Z$/, 'Z'); }

  // One-line note below the brush, shown when cloud history isn't available. mode:
  //   'off' — Cloud Features off but available (Client mode) → tell them they can enable it
  //   'ap'  — Access Point mode → Cloud Features can't be enabled, so don't suggest it
  //   null  — cloud on → hide the note
  function ltCloudHint(mode) {
    let el = document.getElementById('lt-cloud-hint');
    if (!el) {
      const host = document.getElementById('dashboard-brush');
      if (!host || !host.parentNode) return;
      el = document.createElement('div');
      el.id = 'lt-cloud-hint';
      el.style.cssText = 'font-size:12px;opacity:0.7;margin:6px 2px;';
      host.parentNode.insertBefore(el, host.nextSibling);
    }
    if (mode === 'off') el.textContent = 'Showing on-device history (~1 month). Enable Cloud Features in Settings to view older data.';
    else if (mode === 'ap') el.textContent = 'Showing on-device history (~1 month). Older data needs Cloud Features, which is unavailable in Access Point mode.';
    el.style.display = mode ? '' : 'none';
  }

  // Map get-history rows (must be ascending) into parallel arrays in ltData's field
  // shape, dropping any overlap with already-loaded data and breaking the line where
  // the 10-min cadence is interrupted (power-off period).
  function ltCloudRowsToSlice(rows, interval, boundarySec) {
    const t = [], isGap = [], stage = [], fields = {};
    ENV.forEach(([k]) => fields[k] = { min:[], max:[], avg:[] });
    AVG.forEach(([k]) => fields[k] = { avg:[] });
    const num = (v) => (v == null ? null : Number(v));
    const pushGap = (gt) => {
      t.push(gt); isGap.push(true); stage.push(null);
      ENV.forEach(([k]) => { fields[k].min.push(null); fields[k].max.push(null); fields[k].avg.push(null); });
      AVG.forEach(([k]) => fields[k].avg.push(null));
    };
    let prevTs = null;
    for (const r of rows) {
      const ts = Math.floor(Date.parse(r.timestamp) / 1000);
      if (!isFinite(ts) || ts >= boundarySec) continue;   // drop overlap with loaded data
      // Skip invalid/boot windows: a battery can't read 0 V, so batt_volt_avg<=0 means
      // the device sampled before sensors were ready (each reboot leaves one). Bridge over
      // it rather than letting every trace dive to 0. Legit zeros (SOG, VMG) are kept.
      const bv = num(r.batt_volt_avg);
      if (bv == null || bv <= 0) continue;
      if (prevTs != null && (ts - prevTs) > ltGapThreshold(interval)) pushGap((prevTs + ts) / 2);
      prevTs = ts;
      t.push(ts); isGap.push(false);
      // Fields where 0 means "no reading" (a battery/baro can't be 0): null the garbage so
      // boot-window transients don't pull lines/bands to 0. Legit zeros (speeds, angles) kept.
      const nz = (k, v) => ((k === 'battVolt' || k === 'baro') && v != null && v <= 0) ? null : v;
      ENV.forEach(([k]) => {
        const c = LT_CLOUD_COL[k], f = fields[k];
        f.min.push(nz(k, num(r[c + '_min']))); f.max.push(nz(k, num(r[c + '_max']))); f.avg.push(nz(k, num(r[c + '_avg'])));
      });
      AVG.forEach(([k]) => fields[k].avg.push(nz(k, num(r[LT_CLOUD_COL[k] + '_avg']))));
      stage.push(r.charge_stage == null ? null : Number(r.charge_stage));
    }
    return { t, fields, isGap, stage, n: t.length };
  }

  // Reorder ltData strictly by ascending timestamp. A single out-of-order point makes
  // uPlot draw a line segment that jumps backward across the chart (looks like an extra
  // crossing line). Reorders all parallel arrays (t, isGap, every field) by one index map.
  function ltSortData() {
    if (!ltData || ltData.n < 2) return;
    const ot = ltData.t;
    // Sort by timestamp AND drop exact-duplicate timestamps (keep first). Dedup guards
    // against any double-prepend; a backward-jumping point or duplicate both draw wrong.
    const sortIdx = ot.map((_, i) => i).sort((a, b) => ot[a] - ot[b]);
    const idx = []; let last = null;
    for (const i of sortIdx) { if (ot[i] === last) continue; idx.push(i); last = ot[i]; }
    if (idx.length === ot.length) {   // already sorted, no dups → nothing to do
      let clean = true;
      for (let j = 0; j < idx.length; j++) if (idx[j] !== j) { clean = false; break; }
      if (clean) return;
    }
    const og = ltData.isGap || [], ost = ltData.stage || [];
    ltData.t = idx.map(i => ot[i]);
    ltData.isGap = idx.map(i => og[i]);
    ltData.stage = idx.map(i => ost[i]);
    ENV.forEach(([k]) => { const f = ltData.fields[k], mn = f.min, mx = f.max, av = f.avg;
      f.min = idx.map(i => mn[i]); f.max = idx.map(i => mx[i]); f.avg = idx.map(i => av[i]); });
    AVG.forEach(([k]) => { const av = ltData.fields[k].avg; ltData.fields[k].avg = idx.map(i => av[i]); });
    ltData.n = ltData.t.length;
  }

  // Prepend an older slice into ltData chronologically, breaking the line at the
  // cloud↔local seam if the cadence gaps there.
  function ltCloudPrepend(slice, interval) {
    if (!slice.n) return;
    const t = slice.t.slice(), isGap = slice.isGap.slice(), stage = (slice.stage || []).slice(), fields = {};
    ENV.forEach(([k]) => fields[k] = { min: slice.fields[k].min.slice(), max: slice.fields[k].max.slice(), avg: slice.fields[k].avg.slice() });
    AVG.forEach(([k]) => fields[k] = { avg: slice.fields[k].avg.slice() });
    if (ltData.t.length && (ltData.t[0] - slice.t[slice.n - 1]) > ltGapThreshold(interval)) {
      const gt = (slice.t[slice.n - 1] + ltData.t[0]) / 2;   // seam gap
      t.push(gt); isGap.push(true); stage.push(null);
      ENV.forEach(([k]) => { fields[k].min.push(null); fields[k].max.push(null); fields[k].avg.push(null); });
      AVG.forEach(([k]) => fields[k].avg.push(null));
    }
    ltData.t = t.concat(ltData.t);
    ltData.isGap = isGap.concat(ltData.isGap || []);
    ltData.stage = stage.concat(ltData.stage || []);
    ENV.forEach(([k]) => {
      ltData.fields[k].min = fields[k].min.concat(ltData.fields[k].min);
      ltData.fields[k].max = fields[k].max.concat(ltData.fields[k].max);
      ltData.fields[k].avg = fields[k].avg.concat(ltData.fields[k].avg);
    });
    AVG.forEach(([k]) => { ltData.fields[k].avg = fields[k].avg.concat(ltData.fields[k].avg); });
    ltData.n = ltData.t.length;
    ltSortData();   // guarantee strict monotonic order (no backward-jumping line segments)
  }

  // Open one chunk of reachable past so the brush can be dragged into cloud territory
  // (the actual fetch stays lazy — triggered only when the view passes the loaded edge).
  function ltCloudOldestSec() {   // oldest loaded boundary, valid even with an empty local ring
    if (ltCloud.oldest != null) return ltCloud.oldest;
    if (ltData && ltData.n > 0) return ltData.t[0];
    return Math.floor(Date.now() / 1000);
  }
  function ltCloudExtendReach() {
    if (!_brushState || !_brushState.nativeSink) return;
    const reachMs = (ltCloudOldestSec() - LT_CLOUD_CHUNK_S) * 1000;
    _brushState.dataMin = reachMs;
    const mn = document.getElementById('dashboard-brush-data-min');
    if (mn && typeof _brushFormatTime === 'function') mn.textContent = _brushFormatTime(reachMs);
  }
  function ltCloudClampReach() {   // exhausted: stop the brush at real data
    if (!_brushState || !_brushState.nativeSink) return;
    _brushState.dataMin = ltCloudOldestSec() * 1000;
    const mn = document.getElementById('dashboard-brush-data-min');
    if (mn && typeof _brushFormatTime === 'function') mn.textContent = _brushFormatTime(_brushState.dataMin);
  }

  // Pull the next older 30-day chunk (cloud-gated, deduped, bounded, chains while the
  // brushed range still extends past the loaded edge).
  async function ltCloudPull() {
    if (!ltData || ltCloud.fetching || ltCloud.exhausted) return;
    if (!ltCloudEnabled()) { ltCloud.avail = false; return; }
    // local had little (≤1 pt or <1h span) → after the fill, show the whole loaded range
    const wasSparse = (ltData.n < 2) || ((ltData.t[ltData.n - 1] - ltData.t[0]) < 3600);
    const boundary = (ltCloud.oldest != null) ? ltCloud.oldest : ltData.t[0];
    if (boundary <= Math.floor(Date.now() / 1000) - LT_CLOUD_MAX_LOOKBACK_S) { ltCloud.exhausted = true; ltCloudClampReach(); return; }
    ltCloud.fetching = true;
    try {
      const token = await ensureCloudToken();
      if (!token) { ltCloud.avail = false; ltCloudClampReach(); return; }  // not registered → no cloud tier
      ltCloud.avail = true;
      const startSec = boundary - LT_CLOUD_CHUNK_S;
      const resp = await fetch(`${SUPABASE_URL}/functions/v1/get-history`, {
        method: 'POST', headers: cloudHeaders(),
        body: JSON.stringify({ token, start: ltIso(startSec), end: ltIso(boundary) })
      });
      if (!resp.ok) { if (resp.status === 401) g_cloudToken = null; return; }   // leave state → retry later
      const json = await resp.json();
      const rows = (json && json.data) ? json.data.slice() : [];
      rows.sort((a, b) => Date.parse(a.timestamp) - Date.parse(b.timestamp));   // ascending
      const interval = ltData.interval || 600;
      const slice = ltCloudRowsToSlice(rows, interval, boundary);
      if (slice.n === 0) { ltCloud.exhausted = true; ltCloudClampReach(); return; }  // no older data
      ltCloudPrepend(slice, interval);
      ltCloud.oldest = slice.t[0];
      ltFullRange = [ ltData.t[0], ltFullRange ? ltFullRange[1] : ltData.t[ltData.n - 1] ];
      ltCloudExtendReach();
    } catch (e) {
      // Network/parse failure — leave state so a later brush retries.
    } finally {
      ltCloud.fetching = false;
    }
    // Re-render so the stitched data appears. If the ring was sparse (no real local
    // span), show the whole freshly-loaded range; otherwise keep the current range (and
    // if it still passes the new loaded edge, that re-triggers a pull → next chunk).
    if (wasSparse && ltData.n >= 2 && ltData.t[ltData.n - 1] > ltData.t[0]) {
      ltRenderAll(ltData.t[0], ltData.t[ltData.n - 1]);
    } else if (ltCurRange) {
      ltRenderAll(ltCurRange[0], ltCurRange[1]);
    }
  }

  // Called from ltRenderAll: pull when the view passes the oldest loaded record.
  function ltCloudMaybePull(fromSec) {
    if (!ltData || ltCloud.exhausted || ltCloud.fetching || ltCloud.avail === false) return;
    if (isFinite(fromSec) && fromSec < ltCloudOldestSec()) ltCloudPull();
  }

  // oldestSec = the boundary below which cloud fills (oldest local record, or `now` when
  // the local ring is empty). No auto-pull here — the initial ltRenderAll's
  // ltCloudMaybePull triggers exactly one pull when the view reaches past `oldest`
  // (which it does in the sparse case, where loSec is well before oldest). Calling a pull
  // here too caused a concurrent double-fetch (fetching flag was reset) → duplicate data.
  function ltCloudInit(oldestSec) {
    ltCloud = { avail: undefined, fetching: false, oldest: oldestSec, exhausted: false };
    if (!ltCloudEnabled()) {
      ltCloud.avail = false;
      const apMode = (document.getElementById('currentModeID')?.value === '1');  // 1 = Access Point
      ltCloudHint(apMode ? 'ap' : 'off');
      return;
    }
    ltCloudHint(null);
    ltCloudExtendReach();
  }

  // Re-fetch the on-device ring on EVERY tab open (the endpoint now sends only the
  // populated records, so it's cheap) → the local portion is always current. Cloud stays
  // lazy: re-stitched on brush-past-horizon. Charts are built once; subsequent opens
  // refresh their data.
  window.initLongTermPlots = function () {
    fetch('/longTermPlots.bin', { cache: 'no-cache' })
      .then(r => { if (!r.ok) throw new Error('http ' + r.status); return r.arrayBuffer(); })
      .then(buf => {
        ltData = parseLT(buf);
        if (!ltData) throw new Error('parse failed');
        ltSortData();   // strict monotonic order before any render
        ltLoaded = true;
        // Empty local ring: if Cloud Features is on, still build the charts so the cloud
        // stitch can fill the timeline; only show the "no data" notice when cloud is off.
        if (ltData.n === 0 && !ltCloudEnabled()) {
          ltStatus('No long-term data yet — records accumulate locally every ~10 min. Without WiFi/Cloud, history is on-device only (about 30 days).');
          return;
        }
        ltStatus(null);
        buildAllLtPlots();
      })
      // On a transient re-fetch failure, keep whatever's already on screen rather than blanking.
      .catch(e => { if (!ltLoaded) ltStatus('Long-term history unavailable (' + e.message + ').'); });
  };
})();

/* ---- Session fuel-economy curve (mpg vs RPM), driven each CSV2 tick by window.updateFuelCurve ---- */
(function () {
  const NBINS = 18;                 // must match firmware FUELCURVE_BINS
  const DEFAULT_TOP = 4500;         // fallback x-scale until firmware reports the configured top RPM
  let plot = null;
  let yManual = null;               // [min,max] typed on the plot; null = auto-fit
  try { yManual = JSON.parse(localStorage.getItem('fuelCurveYRange')) || null; } catch (e) { }

  function make(el) {
    const opts = {
      width: el.clientWidth || 600,
      height: 300,
      scales: {
        x: { time: false },
        mpg: { range: (u, mn, mx) => yManual ? yManual : (mn == null ? [0, 1] : uPlot.rangeNum(mn, mx, 0.1, true)) }
      },
      series: [
        { label: "RPM" },
        { label: "mpg", scale: "mpg", stroke: "#2e7d32", width: 2, points: { show: true, size: 6 }, spanGaps: false }
      ],
      axes: [
        { scale: "x", label: "RPM", values: (u, t) => t.map(v => Math.round(v)) },
        { scale: "mpg", label: "naut mi / gal", side: 3,
          splits: edgeLabeledSplits(() => yManual !== null) }
      ],
      legend: { show: false }
    };
    plot = new uPlot(opts, [[0], [null]], el);
    if (document.body.classList.contains('dark-mode')) updateUplotTheme(plot);
    const ro = new ResizeObserver(() => { if (plot) plot.setSize({ width: el.clientWidth || 600, height: 300 }); });
    ro.observe(el);

    // Click-to-edit Y limits (clear both boxes + Enter returns to auto-fit)
    attachYAxisEdit(plot, [{
      scale: 'mpg', decimals: 2,
      apply: (mn, mx) => {
        yManual = [mn, mx];
        localStorage.setItem('fuelCurveYRange', JSON.stringify(yManual));
        plot.setData(plot.data);   // re-runs the range function
      },
      auto: () => {
        yManual = null;
        localStorage.removeItem('fuelCurveYRange');
        plot.setData(plot.data);
      }
    }]);
  }

  window.updateFuelCurve = function (data) {
    if (data.fuelCurveNMPG_0 === undefined) return;        // not in this payload
    const el = document.getElementById('fuelCurvePlot');
    if (!el) return;
    if (!plot) { if (typeof uPlot === 'undefined') return; make(el); }
    // Bin width is universal: it scales with the engine's configured fuel-table top RPM.
    const topRPM = Number(data.fuelCurveTopRPM) > 0 ? Number(data.fuelCurveTopRPM) : DEFAULT_TOP;
    const binW = topRPM / NBINS;
    const X = Array.from({ length: NBINS }, (_, i) => i * binW + binW / 2);  // bin centers
    const y = new Array(NBINS);
    let bestV = 0, bestI = -1;
    for (let i = 0; i < NBINS; i++) {
      const v = Number(data['fuelCurveNMPG_' + i]) / 100;
      y[i] = v > 0 ? v : null;                              // 0 = empty bin -> gap
      if (v > bestV) { bestV = v; bestI = i; }
    }
    plot.setData([X, y]);
    const best = document.getElementById('fuelCurveBestID');
    if (best) {
      const txt = bestI >= 0
        ? bestV.toFixed(2) + ' naut mi/gal at ~' + Math.round(bestI * binW) + '–' + Math.round((bestI + 1) * binW) + ' rpm'
        : '—';
      if (best.textContent !== txt) best.textContent = txt;
    }
  };
})();

// ─────────────────────────────────────────────────────────────────────────────
// Fast alternator-current scope (Plots → Scope). Pulls /fastscope.bin — a 500 ms
// raw capture at 20 kSPS — and renders it as amps vs milliseconds. Zoom buttons
// show the LAST N ms of the capture (ms is ground truth); the hint reports how many engine
// and alternator revolutions that window spans, from the live RPM and PulleyRatio.
// ─────────────────────────────────────────────────────────────────────────────
let fastScopePlot = null;
let fastScopeData = null;
let fastScopeViewMs = 250;
let fastScopeAmpsMin = null, fastScopeAmpsMax = null;  // null = auto-scale; set via the click-to-edit Y widget, persisted to localStorage like the other plots
let fastScopePaused = false;
// Raw and Filtered are independent show/hide toggles (plot series 1 and 2), not an A/B switch.
// Raw = the full 20 kHz samples; Filtered = the boxcar-16 the analysis uses (≈550 Hz bandwidth).
// Any combination is allowed, including both or neither. Default: Raw on, Filtered off.
let fastScopeShowRaw = true;
let fastScopeShowFilt = false;
const FA_SCOPE_BOXCAR_N = 16;   // matches the firmware FA_DECIM — first null 20 kHz/16 = 1.25 kHz, −3 dB ≈ 550 Hz

// Trailing N-sample moving average (same shape as the firmware boxcar-16 decimation, but kept
// at full sample positions so the time axis is unchanged — a single-sample glitch shrinks ~Nx).
function fastScopeBoxcar(amps, N) {
    const n = amps.length, out = new Array(n);
    let acc = 0;
    for (let i = 0; i < n; i++) {
        acc += amps[i];
        if (i >= N) acc -= amps[i - N];
        out[i] = acc / Math.min(i + 1, N);
    }
    return out;
}
// Plot data is always [tMs, rawAmps, filteredAmps] — both series columns are present every
// frame; the toggles control series visibility (show), not which column is computed. The
// boxcar over ~5 k samples is cheap, so we compute it unconditionally.
function fastScopeViewData() {
    if (!fastScopeData) return [[], [], []];
    return [fastScopeData.tMs, fastScopeData.amps, fastScopeBoxcar(fastScopeData.amps, FA_SCOPE_BOXCAR_N)];
}
function fastScopeLoadAmpsRange() {
    try { const s = JSON.parse(localStorage.getItem('faScopeAmpsRange')); if (s && isFinite(s.min) && isFinite(s.max)) { fastScopeAmpsMin = s.min; fastScopeAmpsMax = s.max; } } catch (e) { }
}
function fastScopeSaveAmpsRange() {
    try {
        if (fastScopeAmpsMin != null && fastScopeAmpsMax != null) localStorage.setItem('faScopeAmpsRange', JSON.stringify({ min: fastScopeAmpsMin, max: fastScopeAmpsMax }));
        else localStorage.removeItem('faScopeAmpsRange');
    } catch (e) { }
}
fastScopeLoadAmpsRange();
let fastScopeResizeObserver = null;

function parseFastScope(buf) {
    const dv = new DataView(buf);
    if (buf.byteLength < 16 || dv.getUint32(0, true) !== 0x46534331) return null;  // 'FSC1'
    const rate = dv.getUint16(4, true) * 10;
    const count = dv.getUint16(6, true);
    const zeroMv = dv.getUint16(8, true);
    const apv = dv.getUint16(10, true);
    const atten = dv.getUint8(12);
    const state = dv.getUint8(13);
    if (buf.byteLength < 16 + count * 2) return null;
    const tMs = new Array(count), amps = new Array(count);
    for (let i = 0; i < count; i++) {
        const mv = dv.getInt16(16 + i * 2, true);
        amps[i] = (mv - zeroMv) * apv / 1000;
        tMs[i] = i * 1000 / rate;
    }
    return { rate, count, zeroMv, apv, atten, state, tMs, amps };
}

// How many engine and alternator turns the currently-shown window spans. Engine speed is the
// live tach reading (window._debugData.RPM); the alternator turns PulleyRatio times faster
// (Alt_RPM = RPM × PulleyRatio, matching the firmware's Alt_RPM). The count scales with the View
// buttons because the window is fastScopeViewMs wide. Approximate — RPM is a live reading, not
// measured off this trace, so a sudden RPM change mid-capture won't be reflected exactly.
function fastScopeRevHint() {
    const rpm = parseFloat((window._debugData || {}).RPM);
    if (!isFinite(rpm) || rpm < 1) return 'Engine stopped — no revolutions to show.';
    const fmt = v => v < 1 ? v.toFixed(2) : v.toFixed(1);
    const engineRevs = rpm / 60 * (fastScopeViewMs / 1000);
    const pulleyEl = document.getElementById('PulleyRatio_echo');
    const pulley = pulleyEl ? parseFloat(pulleyEl.textContent) : NaN;
    let s = '≈ ' + fmt(engineRevs) + ' engine revs';
    if (isFinite(pulley) && pulley > 0) s += ' · ' + fmt(engineRevs * pulley) + ' alternator revs';
    return s + ' in this ' + fastScopeViewMs + ' ms window';
}

function initFastScopePlot() {
    const plotEl = document.getElementById('fastscope-plot');
    if (!plotEl || fastScopePlot) return;
    const opts = {
        width: Math.min(plotEl.clientWidth || 360, 800),
        height: 300,
        // No title: the view buttons re-window one capture, so any fixed "250 ms" label is wrong once zoomed. Signal is named on the Y axis instead.
        series: [
            {},
            { label: "Raw (A)", stroke: "#2196F3", width: 1, points: { show: false }, scale: "amps", show: fastScopeShowRaw },
            { label: "Filtered (A)", stroke: "#FF9800", width: 1.5, points: { show: false }, scale: "amps", show: fastScopeShowFilt }
        ],
        scales: {
            // x window pinned by a range fn (re-read each redraw) to the last fastScopeViewMs of the
            // capture — so a refresh or zoom never flashes through the full 250 ms range. auto:false
            // keeps setData from re-fitting x. dEnd = newest sample time.
            x: {
                time: false,
                auto: false,
                range: () => {
                    const d = fastScopeData;
                    const tEnd = (d && d.count > 0) ? d.tMs[d.count - 1] : fastScopeViewMs;
                    return [Math.max(0, tEnd - fastScopeViewMs), tEnd];
                }
            },
            // Default auto (NOT auto:false): uPlot only recomputes the data min/max it passes to
            // this fn when the scale is auto, so the auto-fit branch AND the "auto" checkbox need
            // fresh extents on each setData. Manual pin returns the saved range. Mirrors the CV plot.
            amps: {
                range: (u, mn, mx) => (fastScopeAmpsMin != null && fastScopeAmpsMax != null)
                    ? [fastScopeAmpsMin, fastScopeAmpsMax]
                    : (mn == null ? [-1, 1] : uPlot.rangeNum(mn, mx, 0.1, true))
            }
        },
        axes: [
            { label: "Milliseconds", grid: { show: true } },
            {
                scale: "amps", label: "Alternator Current (A)", grid: { show: true }, side: 3,
                splits: edgeLabeledSplits(() => fastScopeAmpsMin != null)  // show the editable min/max at the edges when manually ranged
            }
        ],
        legend: { show: false }
    };
    fastScopePlot = new uPlot(opts, [[], [], []], plotEl);
    if (document.body.classList.contains('dark-mode')) updateUplotTheme(fastScopePlot);

    // Auto checkbox (top-right) — same convention as the other plots. No "lock" button:
    // this is a one-shot capture, not a streaming autoscale loop. Checked = auto-fit (null
    // range vars); unchecked = pin to whatever is currently shown.
    plotEl.style.position = 'relative';
    const existingAs = plotEl.querySelector('.autoscale-ctrl');
    if (existingAs) existingAs.remove();
    const asDiv = document.createElement('div');
    asDiv.className = 'autoscale-ctrl';
    asDiv.style.cssText = 'position:absolute;top:6px;right:8px;z-index:10;display:flex;align-items:center;gap:3px;font-size:11px;opacity:0.6;';
    asDiv.innerHTML = '<input type="checkbox" id="autoscale-fastscope-cb" style="cursor:pointer;width:12px;height:12px;margin:0;"><label for="autoscale-fastscope-cb" style="cursor:pointer;user-select:none;">auto</label>';
    plotEl.appendChild(asDiv);
    const faCb = document.getElementById('autoscale-fastscope-cb');
    faCb.checked = (fastScopeAmpsMin == null);  // null range vars = auto-fit
    faCb.addEventListener('change', e => {
        if (e.target.checked) { fastScopeAmpsMin = null; fastScopeAmpsMax = null; }
        else if (fastScopePlot) { fastScopeAmpsMin = fastScopePlot.scales.amps.min; fastScopeAmpsMax = fastScopePlot.scales.amps.max; }
        fastScopeSaveAmpsRange();
        if (fastScopePlot) fastScopePlot.setData(fastScopePlot.data);
    });

    // Click-to-edit Y limits — the same shared widget every other plot uses; persisted to localStorage.
    // Editing pins the range (uncheck auto); clearing the boxes returns to auto-fit (re-check).
    attachYAxisEdit(fastScopePlot, [{
        scale: 'amps', decimals: 2,
        apply: (mn, mx) => { fastScopeAmpsMin = mn; fastScopeAmpsMax = mx; faCb.checked = false; fastScopeSaveAmpsRange(); if (fastScopePlot) fastScopePlot.setData(fastScopePlot.data); },
        auto: () => { fastScopeAmpsMin = null; fastScopeAmpsMax = null; faCb.checked = true; fastScopeSaveAmpsRange(); if (fastScopePlot) fastScopePlot.setData(fastScopePlot.data); }
    }]);
    const resizePlot = debounce(() => {
        const el = document.getElementById('fastscope-plot');
        if (el && fastScopePlot) fastScopePlot.setSize({ width: Math.min(el.clientWidth || 360, 800), height: 300 });
    }, 1000);
    if (fastScopeResizeObserver) fastScopeResizeObserver.disconnect();
    fastScopeResizeObserver = new ResizeObserver(resizePlot);
    fastScopeResizeObserver.observe(plotEl);
}

function fastScopeStatusText(d) {
    if (!d) return 'Bad response from regulator.';
    if (d.state === 0) return 'Channel off (sampler failed to start).';
    if (d.state === 2) return 'Channel dormant — input reads railed full-scale (sense jumper open?).';
    const range = d.atten ? 'high range' : 'standard range';
    return 'Captured ' + d.count + ' samples at ' + (d.rate / 1000) + ' kHz, ' + range + '. Auto-updates every 5 seconds while this panel is open.';
}

function fastScopeApplyZoom() {
    // Re-run the x range fn with the current fastScopeViewMs — a queued setData is all it takes.
    if (fastScopePlot && fastScopeData && fastScopeData.count > 0) fastScopePlot.setData(fastScopePlot.data);
}

function fastScopeZoom(ms, btn) {
    fastScopeViewMs = ms;
    document.querySelectorAll('.fastscope-zoom').forEach(b => b.classList.remove('active'));
    if (btn) btn.classList.add('active');
    fastScopeApplyZoom();
    // Revs shown scale with the window width, so refresh the hint on every view change.
    const hint = document.getElementById('fastscope-cycle-hint');
    if (hint && fastScopeData && fastScopeData.count > 0) hint.textContent = fastScopeRevHint();
}

// silent=true (the 5 s auto-refresh) skips the "Fetching…" status flash so the panel updates in
// place. The x range fn pins the view, so setData alone re-windows without a full-range flash.
function fetchFastScope(silent) {
    const status = document.getElementById('fastscope-status');
    if (status && !silent) status.textContent = 'Fetching…';
    fetch(buildURL('/fastscope.bin'), { cache: 'no-cache' })
        .then(r => { if (!r.ok) throw new Error('http ' + r.status); return r.arrayBuffer(); })
        .then(buf => {
            const d = parseFastScope(buf);
            fastScopeData = d;
            if (status) status.textContent = fastScopeStatusText(d);
            const hint = document.getElementById('fastscope-cycle-hint');
            if (hint) hint.textContent = (d && d.count > 0) ? fastScopeRevHint() : '';
            if (!d || d.count === 0) return;
            initFastScopePlot();
            if (fastScopePlot) fastScopePlot.setData(fastScopeViewData());  // x range fn pins the window — no flash
        })
        .catch(e => { if (status) status.textContent = 'Capture unavailable (' + e.message + ').'; });
}

function fastScopeOnOpen() {
    if (!fastScopeData) fetchFastScope();
    else if (fastScopePlot) setTimeout(() => fastScopePlot.setSize({ width: Math.min(document.getElementById('fastscope-plot').clientWidth || 360, 800), height: 300 }), 50);
    fetchFastFlip();   // always re-pull on open so freshly-frozen reference pages show without a manual Refresh
}

// Pause freezes the displayed capture by halting the ~5 s auto-refresh; the manual Refresh
// Capture button still works while paused so you can step to a fresh frame on demand.
function fastScopePauseToggle(btn) {
    fastScopePaused = !fastScopePaused;
    if (btn) { btn.textContent = fastScopePaused ? 'Resume' : 'Pause'; btn.classList.toggle('active', fastScopePaused); }
    const status = document.getElementById('fastscope-status');
    if (status && fastScopePaused) status.textContent = 'Paused — showing a frozen capture.';
}

// Export the currently-shown scope capture as CSV. Always includes BOTH the raw 20 kHz samples
// and the boxcar-16 filtered version (what the analysis sees), regardless of the Raw/Filtered view.
function downloadFaScopeCsv() {
    if (!fastScopeData || !fastScopeData.count) { alert('No scope capture yet — press Refresh Capture first.'); return; }
    const d = fastScopeData;
    const filt = fastScopeBoxcar(d.amps, FA_SCOPE_BOXCAR_N);
    let csv = 'ms,amps_raw,amps_filtered\n';
    for (let i = 0; i < d.count; i++) csv += d.tMs[i].toFixed(4) + ',' + d.amps[i].toFixed(3) + ',' + filt[i].toFixed(3) + '\n';
    const dt = new Date(), p = n => String(n).padStart(2, '0');
    const stamp = dt.getFullYear() + '-' + p(dt.getMonth() + 1) + '-' + p(dt.getDate()) + ' ' + p(dt.getHours()) + '-' + p(dt.getMinutes()) + '-' + p(dt.getSeconds());
    const a = document.createElement('a');
    a.href = URL.createObjectURL(new Blob([csv], { type: 'text/csv' }));
    a.download = 'Alt Scope Capture ' + stamp + '.csv';
    document.body.appendChild(a); a.click();
    setTimeout(() => { URL.revokeObjectURL(a.href); a.remove(); }, 1000);
}

// Live Data → Diag sub-tab (item 1). Opens the relocated scope/flipbook and starts the
// ~5 s scope auto-refresh (item 2). The refresh polls /fastscope.bin only while the Diag
// panel is the active sub-tab AND the page is visible; it stops itself otherwise. The fetch
// runs on the Core-0 web task — the only control-loop cost is the ~0.1–0.2 ms ring-snapshot
// spinlock wait (verified safe in the audit).
let fastDiagAutoTimer = null;
function fastDiagIsVisible() {
    if (document.visibilityState !== 'visible') return false;
    const panel = document.getElementById('livedata-diag');
    if (!panel || !panel.classList.contains('active')) return false;
    // Fast Alt-Current Monitor now lives in a collapsed-by-default <details>; only live when expanded
    const det = document.getElementById('fa-monitor-details');
    return !!(det && det.open);
}
let fastDiagFlipTick = 0;
function fastDiagStartAuto() {
    if (fastDiagAutoTimer) return;
    fastDiagFlipTick = 0;
    fastDiagAutoTimer = setInterval(() => {
        if (!fastDiagIsVisible()) { fastDiagStopAuto(); return; }
        if (!fastScopePaused) fetchFastScope(true);  // silent: no "Fetching…" flash on the 5 s tick. Pause freezes auto-refresh; manual Refresh still works
        // Flipbook poll every 6th tick (~30 s): reference bands freeze themselves, so their
        // buttons light up here with no manual press. Silent + non-disruptive (see fetchFastFlip).
        if (++fastDiagFlipTick % 6 === 0) fetchFastFlip(true);
    }, 5000);
}
function fastDiagStopAuto() {
    if (fastDiagAutoTimer) { clearInterval(fastDiagAutoTimer); fastDiagAutoTimer = null; }
}
function fastDiagOnOpen() {
    if (!fastScopeData) fetchFastScope();
    else if (fastScopePlot) setTimeout(() => fastScopePlot.setSize({ width: Math.min(document.getElementById('fastscope-plot').clientWidth || 360, 800), height: 300 }), 50);
    fetchFastFlip();   // always re-pull on open so freshly-frozen reference pages show without a manual Refresh
    fastDiagStartAuto();
}
document.addEventListener('visibilitychange', () => {
    if (document.visibilityState === 'visible') { if (fastDiagIsVisible()) fastDiagStartAuto(); }
    else fastDiagStopAuto();
});

// Per-page CSV export of the currently-shown flipbook waveform (item 8).
function downloadFaFlipPageCsv() {
    if (!fastFlipPages || fastFlipSelected < 0) { alert('Select a flipbook page first.'); return; }
    const pg = fastFlipPages[fastFlipSelected];
    if (!pg || !pg.used) { alert('That flipbook page is empty.'); return; }
    let csv = 'ms,amps\n';
    for (let s = 0; s < pg.t_ms.length; s++) csv += pg.t_ms[s].toFixed(3) + ',' + pg.amps_t[s].toFixed(3) + '\n';
    const d = new Date(), p = n => String(n).padStart(2, '0');
    const stamp = d.getFullYear() + '-' + p(d.getMonth() + 1) + '-' + p(d.getDate()) + ' ' + p(d.getHours()) + '-' + p(d.getMinutes()) + '-' + p(d.getSeconds());
    const tag = pg.isAnomaly ? ('anomaly k' + pg.patternK) : (pg.band + 'k-' + (pg.band + 1) + 'k RPM');
    const a = document.createElement('a');
    a.href = URL.createObjectURL(new Blob([csv], { type: 'text/csv' }));
    a.download = 'Alt Waveform ' + tag + ' ' + stamp + '.csv';
    document.body.appendChild(a); a.click();
    setTimeout(() => { URL.revokeObjectURL(a.href); a.remove(); }, 1000);
}

// ─────────────────────────────────────────────────────────────────────────────
// Reference flipbook (Plots → Scope, second card). Pulls /faflip.bin: 8-byte header
// then fixed 2020-byte pages — slots 0..refSlots-1 are the per-1000-RPM reference
// pages, the rest are anomaly captures. Layout pinned by static_assert in firmware.
// ─────────────────────────────────────────────────────────────────────────────
// ── Current Ripple Analyzer: segmented Off/On toggles + the chart Pause/Resume (faAnomPause) ──
// updateSegToggle mirrors updateCheckbox's pending-toggle reconciliation so an in-flight
// optimistic press isn't reverted by a stale echo before the regulator confirms.
function updateSegToggle(key, value) {
    if (value === undefined) return;
    const pending = pendingToggles.get(key);
    if (pending) {
        if (pending.deadlineMs === undefined) pending.deadlineMs = Date.now() + 2500;
        if (value !== pending.desiredValue) { if (Date.now() <= pending.deadlineMs) return; pendingToggles.delete(key); }
        else pendingToggles.delete(key);
    }
    const off = document.getElementById(key + '_off');
    const on = document.getElementById(key + '_on');
    const v = value | 0;
    if (off) off.classList.toggle('cap-mode-active', v === 0);
    if (on) on.classList.toggle('cap-mode-active', v === 1);
}
function faSegSet(key, value, form) {
    const inp = document.getElementById(key);
    if (inp) inp.value = value;
    pendingToggles.set(key, { desiredValue: value, baseRev: lastSeenRev });
    const off = document.getElementById(key + '_off');
    const on = document.getElementById(key + '_on');
    if (off) off.classList.toggle('cap-mode-active', value === 0);
    if (on) on.classList.toggle('cap-mode-active', value === 1);
    if (form) form.submit();
    submitMessage();
}
let _faAnomPause = 0;
function faUpdatePauseBtn(value) {
    if (value === undefined) return;
    const pending = pendingToggles.get('faAnomPause');
    if (pending) {
        if (pending.deadlineMs === undefined) pending.deadlineMs = Date.now() + 2500;
        if (value !== pending.desiredValue) { if (Date.now() <= pending.deadlineMs) return; pendingToggles.delete('faAnomPause'); }
        else pendingToggles.delete('faAnomPause');
    }
    _faAnomPause = value | 0;
    const b = document.getElementById('faflip-pause-btn');
    if (b) {
        b.textContent = _faAnomPause ? 'Resume' : 'Pause';
        b.title = _faAnomPause ? 'Anomaly captures paused — click to resume saving new fault snapshots'
            : 'Pause — stop new fault snapshots from overwriting the saved ones';
    }
}
function faAnomPauseToggle() {
    if (!currentAdminPassword) { alert('Please unlock settings first'); return; }
    const next = _faAnomPause ? 0 : 1;
    pendingToggles.set('faAnomPause', { desiredValue: next, baseRev: lastSeenRev });
    faUpdatePauseBtn(next);
    fetchWithTimeout(buildURL('/get?password=' + encodeURIComponent(currentAdminPassword) + '&faAnomPause=' + next), {}, 5000).then(() => {}).catch(() => {});
}
// Live Oscilloscope: Raw and Filtered are independent show/hide toggles (depress/return per
// click) — any combination including both traces at once or neither. Each button drives its
// own plot series' visibility; the button's cap-mode-active class mirrors the shown state.
function fastScopeViewToggle(which, btn) {
    if (which === 'raw') fastScopeShowRaw = !fastScopeShowRaw;
    else fastScopeShowFilt = !fastScopeShowFilt;
    const raw = document.getElementById('faScopeRawBtn');
    const filt = document.getElementById('faScopeFiltBtn');
    if (raw) raw.classList.toggle('cap-mode-active', fastScopeShowRaw);
    if (filt) filt.classList.toggle('cap-mode-active', fastScopeShowFilt);
    if (fastScopePlot) {
        fastScopePlot.setSeries(1, { show: fastScopeShowRaw });
        fastScopePlot.setSeries(2, { show: fastScopeShowFilt });
    }
}

let fastFlipPages = null;
let fastFlipPlot = null;
let fastFlipSelected = -1;
let faFlipAxis = {};   // per-slot Y-axis state, in-session only: { auto, locked, min, max }
const FASTFLIP_PAGE_BYTES = 2020;
const FASTFLIP_NSAMP = 1000;

function parseFastFlip(buf) {
    const dv = new DataView(buf);
    if (buf.byteLength < 8 || dv.getUint32(0, true) !== 0x46464C50) return null;  // 'FFLP'
    const refSlots = dv.getUint8(4);
    const anomSlots = dv.getUint8(5);
    const rate = dv.getUint16(6, true) * 10;
    const total = refSlots + anomSlots;
    if (buf.byteLength < 8 + total * FASTFLIP_PAGE_BYTES) return null;
    const pages = [];
    for (let i = 0; i < total; i++) {
        const o = 8 + i * FASTFLIP_PAGE_BYTES;
        const used = dv.getUint8(o + 8);
        const pg = {
            slot: i,
            isRef: i < refSlots,
            band: i < refSlots ? i : null,
            used: used === 1,
            rpm: dv.getUint16(o, true),
            amps: dv.getUint16(o + 2, true) / 10,
            epoch: dv.getUint32(o + 4, true),
            isAnomaly: dv.getUint8(o + 9),
            patternK: dv.getUint8(o + 10),
            score: dv.getFloat32(o + 12, true),
            zeroMv: dv.getUint16(o + 16, true),
            apv: dv.getUint16(o + 18, true),
            rate: rate,
            amps_t: null, t_ms: null
        };
        if (pg.used) {
            pg.amps_t = new Array(FASTFLIP_NSAMP);
            pg.t_ms = new Array(FASTFLIP_NSAMP);
            for (let s = 0; s < FASTFLIP_NSAMP; s++) {
                const mv = dv.getInt16(o + 20 + s * 2, true);
                pg.amps_t[s] = (mv - pg.zeroMv) * pg.apv / 1000;
                pg.t_ms[s] = s * 1000 / rate;
            }
        }
        pages.push(pg);
    }
    return pages;
}

function initFastFlipPlot() {
    const plotEl = document.getElementById('fastflip-plot');
    if (!plotEl || fastFlipPlot) return;
    fastFlipPlot = new uPlot({
        width: Math.min(plotEl.clientWidth || 360, 800),
        height: 260,
        title: "Reference Waveform — 200 ms @ 5 kHz",
        series: [
            {},
            { label: "Alternator Current (A)", stroke: "#9C27B0", width: 1, points: { show: false }, scale: "amps" }
        ],
        scales: { x: { time: false }, amps: { auto: true } },
        axes: [
            { label: "Milliseconds", grid: { show: true } },
            { scale: "amps", label: "Amperes", grid: { show: true }, side: 3 }
        ],
        legend: { show: false }
    }, [[], []], plotEl);
    if (document.body.classList.contains('dark-mode')) updateUplotTheme(fastFlipPlot);

    // Per-page Y-axis controls — same auto / lock / click-to-edit widget as the other plots,
    // but each flipbook slot remembers its own state for the session (faFlipAxis keyed by slot).
    // The waveform is absolute current (not AC-coupled), so each page sits at its own DC level
    // and is auto-fit independently rather than sharing one scale.
    plotEl.style.position = 'relative';
    const existing = plotEl.querySelector('.autoscale-ctrl');
    if (existing) existing.remove();
    const ctrl = document.createElement('div');
    ctrl.className = 'autoscale-ctrl';
    ctrl.style.cssText = 'position:absolute;top:6px;right:8px;z-index:10;display:flex;flex-direction:column;align-items:flex-end;gap:2px;font-size:11px;';
    ctrl.innerHTML = '<button id="faflip-pause-btn" title="Pause — stop new fault snapshots from overwriting the saved ones" style="font-size:10px;padding:0 6px;cursor:pointer;border:1px solid #999;border-radius:2px;background:transparent;line-height:16px;" onclick="faAnomPauseToggle()">Pause</button><div style="display:flex;align-items:center;gap:3px;opacity:0.6;"><input type="checkbox" id="faflip-auto-cb" style="cursor:pointer;width:12px;height:12px;margin:0;"><label for="faflip-auto-cb" style="cursor:pointer;user-select:none;">auto</label></div><button id="faflip-lock-btn" style="display:none;font-size:10px;padding:0 5px;cursor:pointer;border:1px solid #999;border-radius:2px;background:transparent;opacity:0.6;line-height:16px;">lock</button>';
    plotEl.appendChild(ctrl);
    faUpdatePauseBtn(_faAnomPause);  // reflect last-known pause state on (re)build

    document.getElementById('faflip-auto-cb').addEventListener('change', e => {
        const st = faFlipAxisState(fastFlipSelected);
        st.auto = e.target.checked;
        if (!st.auto) { const sc = fastFlipPlot.scales.amps; st.min = sc.min; st.max = sc.max; }  // seed manual from what's shown
        st.locked = false;
        faFlipApplyAxis(fastFlipSelected);
        faFlipSyncControls(fastFlipSelected);
    });
    document.getElementById('faflip-lock-btn').addEventListener('click', () => {
        const st = faFlipAxisState(fastFlipSelected);
        if (!st.auto) return;               // lock only applies while auto is on
        st.locked = !st.locked;
        if (st.locked) { const sc = fastFlipPlot.scales.amps; st.min = sc.min; st.max = sc.max; }  // freeze current auto range
        faFlipApplyAxis(fastFlipSelected);
        faFlipSyncControls(fastFlipSelected);
    });

    attachYAxisEdit(fastFlipPlot, [{
        scale: 'amps', decimals: 1,
        apply: (mn, mx) => {                 // typing a number → manual for this page only
            const st = faFlipAxisState(fastFlipSelected);
            st.auto = false; st.locked = false; st.min = mn; st.max = mx;
            faFlipApplyAxis(fastFlipSelected);
            faFlipSyncControls(fastFlipSelected);
        },
        auto: () => {                        // clear + Enter → back to auto for this page
            const st = faFlipAxisState(fastFlipSelected);
            st.auto = true; st.locked = false;
            faFlipApplyAxis(fastFlipSelected);
            faFlipSyncControls(fastFlipSelected);
        }
    }]);
}

function faFlipAxisState(slot) {
    if (!faFlipAxis[slot]) faFlipAxis[slot] = { auto: true, locked: false, min: null, max: null };
    return faFlipAxis[slot];
}

function faFlipApplyAxis(slot) {
    if (!fastFlipPlot) return;
    const st = faFlipAxisState(slot);
    if (st.auto && !st.locked) {             // auto-fit to this page's own data
        const pg = fastFlipPages && fastFlipPages[slot];
        let lo = Infinity, hi = -Infinity;
        if (pg && pg.amps_t) for (const v of pg.amps_t) { if (v < lo) lo = v; if (v > hi) hi = v; }
        if (!isFinite(lo)) { lo = 0; hi = 1; }
        const pad = Math.max(1, (hi - lo) * 0.1);
        fastFlipPlot.setScale('amps', { min: lo - pad, max: hi + pad });
    } else if (isFinite(st.min) && isFinite(st.max) && st.max > st.min) {
        fastFlipPlot.setScale('amps', { min: st.min, max: st.max });
    }
}

function faFlipSyncControls(slot) {
    const autoCb = document.getElementById('faflip-auto-cb');
    const lockBtn = document.getElementById('faflip-lock-btn');
    if (!autoCb || !lockBtn) return;
    const st = faFlipAxisState(slot);
    autoCb.checked = st.auto;
    lockBtn.style.display = st.auto ? 'block' : 'none';
    lockBtn.textContent = st.locked ? 'unlock' : 'lock';
    lockBtn.style.opacity = st.locked ? '1' : '0.6';
}

function fastFlipRender(slot) {
    if (!fastFlipPages) return;
    const pg = fastFlipPages[slot];
    if (!pg || !pg.used) return;
    fastFlipSelected = slot;
    document.querySelectorAll('.fastflip-band').forEach(b => b.classList.toggle('active', +b.dataset.slot === slot));
    const meta = document.getElementById('fastflip-meta');
    if (meta) {
        const when = pg.epoch ? new Date(pg.epoch * 1000).toLocaleDateString() : 'clock not set';
        // Band 0 reads "100–1000" (engines don't idle below ~100 RPM); higher bands span full 1000-RPM ranges
        const bandLabel = pg.band === 0 ? '100–1000' : (pg.band * 1000) + '–' + (pg.band * 1000 + 999);
        meta.textContent = (pg.isAnomaly
            ? 'Anomaly capture (class k=' + pg.patternK + ', score ' + pg.score.toFixed(2) + ')'
            : 'Reference, ' + bandLabel + ' RPM band')
            + ' — captured at ' + pg.rpm + ' RPM, ' + pg.amps.toFixed(1) + ' A, ' + when + '.';
    }
    initFastFlipPlot();
    if (fastFlipPlot) {
        fastFlipPlot.setData([pg.t_ms, pg.amps_t]);
        faFlipApplyAxis(slot);
        faFlipSyncControls(slot);
    }
}

function fastFlipBuildButtons() {
    const wrap = document.getElementById('fastflip-bands');
    const anomWrap = document.getElementById('fastflip-anoms');
    if (!wrap || !fastFlipPages) return;
    wrap.innerHTML = '';
    if (anomWrap) anomWrap.innerHTML = '';
    const nRef = fastFlipPages.filter(p => p.isRef).length;   // ref-slot count straight from the payload header
    fastFlipPages.forEach(pg => {
        const b = document.createElement('button');
        b.type = 'button';
        // Both rows share the same class — only one button is ever .active (green) across both,
        // because fastFlipRender toggles it on every .fastflip-band by matching slot.
        b.className = 'btn-secondary btn-sm fastscope-zoom fastflip-band';
        b.dataset.slot = pg.slot;
        if (pg.isRef) {
            // Band 0 reads "100–1000 RPM" (engines don't idle below ~100); higher bands "N–N+1k RPM"
            b.textContent = pg.band === 0 ? '100–1000 RPM' : pg.band + '–' + (pg.band + 1) + 'k RPM';
        } else {
            b.textContent = 'Anomaly ' + (pg.slot - nRef);
        }
        if (!pg.used) {
            b.disabled = true;
            b.style.opacity = '0.4';
            b.title = pg.isRef ? 'No steady run captured in this band yet' : 'Empty anomaly slot';
        } else {
            b.onclick = () => fastFlipRender(pg.slot);
        }
        (pg.isRef ? wrap : (anomWrap || wrap)).appendChild(b);
    });
}

// auto=true is the silent background poll (from the Diag auto-loop): it refreshes the page
// buttons so freshly-frozen reference bands light up on their own, but never flashes the
// status line and never yanks the plot away from a page you're currently examining. Manual
// Refresh / open / re-baseline call it without auto, so they always re-render.
function fetchFastFlip(auto) {
    const status = document.getElementById('fastflip-status');
    if (status && !auto) status.textContent = 'Fetching…';
    fetch(buildURL('/faflip.bin'), { cache: 'no-cache' })
        .then(r => { if (!r.ok) throw new Error('http ' + r.status); return r.arrayBuffer(); })
        .then(buf => {
            fastFlipPages = parseFastFlip(buf);
            if (!fastFlipPages) { if (status && !auto) status.textContent = 'Bad response from regulator.'; return; }
            const nUsed = fastFlipPages.filter(p => p.used).length;
            if (status) status.textContent = nUsed === 0
                ? 'No pages captured yet — references freeze automatically on steady runs (20–100 A).'
                : nUsed + ' page' + (nUsed > 1 ? 's' : '') + ' captured.';
            fastFlipBuildButtons();
            const cur = (fastFlipSelected >= 0) ? fastFlipPages.find(p => p.slot === fastFlipSelected && p.used) : null;
            if (!(auto && cur)) {   // auto poll leaves an open page alone; otherwise show the selected-or-first used page
                const first = cur || fastFlipPages.find(p => p.used);
                if (first) fastFlipRender(first.slot);
            } else {
                // Buttons were just rebuilt (highlight cleared) but we skipped the re-render — re-mark the open page.
                document.querySelectorAll('.fastflip-band').forEach(b => b.classList.toggle('active', +b.dataset.slot === fastFlipSelected));
            }
        })
        .catch(e => { if (status && !auto) status.textContent = 'Flipbook unavailable (' + e.message + ').'; });
}

/* XREG_END */