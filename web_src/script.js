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
let isDeviceRegistered = false; // Tracks if device is registered for Cloud Features
//let isDeviceRegistered = true; // TEMP: bypass for local testing

let source; // Declare at broader scope

//let vesselInfoComplete = false; // Tracks if vessel info is filled out
let vesselInfoComplete = true; // TEMP: bypass for local testing

window.vesselInfo = null; // Cache for vessel data

// Demo mode for App Store testing (no ESP32 required)
let DEMO_MODE = false;
let demoInterval = null;

//prevent EventSource Infinite Reconnection
let sseReconnectAttempts = 0;
const MAX_SSE_RECONNECTS = 10;
let sseReconnectTimer = null;
let isAppInBackground = false;

let activeTimers = []; // Track all active timers

let g_lastCsv3 = null; // Last received CSV3 data object — used by cvBinToCsv for header constants

const CSV1_FIELDS = [
    "AlternatorTemperatureF",     // 0
    "dutyCycle",                  // 1
    "BatteryV",                   // 2
    "MeasuredAmps",               // 3
    "RPM",                        // 4
    "Channel3V",                  // 5
    "IBV",                        // 6
    "Bcur",                       // 7
    "VictronVoltage",             // 8
    "LoopTime",                   // 9
    "WifiHeartBeat",              // 10
    "vvout",                      // 11
    "iiout",                      // 12
    "FreeHeap",                   // 13
    "Alarm_Status",               // 14
    "fieldActiveStatus",          // 15
    "currentMode",                // 16
    "stateRevision",              // 17
    "setpointLimited",            // 18
    "uTargetAmps",                // 19
    "pidInput",                   // 20
    "pidOutput",                  // 21
    "pidError",                   // 22
    "imu_heel_deg",               // 23
    "imu_pitch_deg",              // 24
    "imu_vertical_accel_g",       // 25
    "imu_yaw_rate_dps",           // 26
    "imu_total_accel_g",          // 27
    "imu_hf_vibration_energy",    // 28
    "shutdownPhase",              // 29
    "BatteryV_raw",               // 30
    "MeasuredAmps_filtered",      // 31
    "voltageTarget",              // 32
    "Icv",                        // 33
];
const CSV2_FIELDS = [
    "IBVMax",                              // 0
    "MeasuredAmpsMax",                     // 1
    "RPMMax",                              // 2
    "SOC_percent",                         // 3
    "EngineRunTime",                       // 4
    "AlternatorOnTime",                    // 5
    "AlternatorFuelUsed",                  // 6
    "ChargedEnergy",                       // 7
    "DischargedEnergy",                    // 8
    "AlternatorChargedEnergy",             // 9
    "MaxAlternatorTemperatureF",           // 10
    "temperatureThermistor",               // 11
    "MaxTemperatureThermistor",            // 12
    "VictronCurrent",                      // 13
    "timeToFullChargeMin",                 // 14
    "timeToFullDischargeMin",              // 15
    "LatitudeNMEA",                        // 16
    "LongitudeNMEA",                       // 17
    "SatelliteCountNMEA",                  // 18
    "LastSessionDuration",                 // 19
    "LastSessionMaxLoopTime",              // 20
    "lastSessionMinHeap",                  // 21
    "wifiReconnectsTotal",                 // 22
    "LastResetReason",                     // 23
    "ancientResetReason",                  // 24
    "totalPowerCycles",                    // 25
    "MinFreeHeap",                         // 26
    "currentWeatherMode",                  // 27
    "UVToday",                             // 28
    "UVTomorrow",                          // 29
    "UVDay2",                              // 30
    "weatherDataValid",                    // 31
    "SolarWatts",                          // 32
    "performanceRatio",                    // 33
    "VeData",                              // 34
    "NMEA0183Data",                        // 35
    "NMEA2KData",                          // 36
    "alarmLatch",                          // 37
    "ResetAlarmLatch",                     // 38
    "ResetLearningTable",                  // 39
    "ClearOverheatHistory",                // 40
    "DynamicShuntGainFactor",              // 41
    "DynamicAltCurrentZero",               // 42
    "InsulationLifePercent",               // 43
    "GreaseLifePercent",                   // 44
    "BrushLifePercent",                    // 45
    "PredictedLifeHours",                  // 46
    "LifeIndicatorColor",                  // 47
    "pKwHrToday",                          // 48
    "pKwHrTomorrow",                       // 49
    "pKwHr2days",                          // 50
    "ambientTemp",                         // 51
    "baroPressure",                        // 52
    "firmwareVersionInt",                  // 53
    "deviceIdUpper",                       // 54
    "deviceIdLower",                       // 55
    "ChargedEnergy_AllTime",               // 56
    "AlternatorFuelUsed_AllTime",          // 57
    "PeakVoltage_AllTime",                 // 58
    "EngineRunTime_AllTime",               // 59
    "MinVoltage",                          // 60
    "MinVoltage_AllTime",                  // 61
    "ChargeCycles",                        // 62
    "ChargeCycles_AllTime",                // 63
    "EngineFuelUsed",                      // 64
    "EngineFuelUsed_AllTime",              // 65
    "TotalDistance",                       // 66
    "TotalDistance_AllTime",               // 67
    "MaxSpeed",                            // 68
    "MaxSpeed_AllTime",                    // 69
    "SolarChargedEnergy",                  // 70
    "SolarChargedEnergy_AllTime",          // 71
    "AlternatorChargedEnergy_AllTime",     // 72
    "DischargedEnergy_AllTime",            // 73
    "AvgSOC_AllTime",                      // 74
    "AvgSpeed_AllTime",                    // 75
    "AvgSpeed",                            // 76
    "AlternatorOnTime_AllTime",            // 77
    "EngineCycles_AllTime",                // 78
    "MaxAlternatorTemperatureF_AllTime",   // 79
    "MaxTemperatureThermistor_AllTime",    // 80
    "MeasuredAmpsMax_AllTime",             // 81
    "RPMMax_AllTime",                      // 82
    "Ignition",                            // 83
    "BulkStage",                           // 84
    "WifiWakeSecondsRemaining",            // 85
    "BufferedRecordCount",                 // 86
    "BufferedRecordPercent",               // 87
    "MAX_BUFFERED_RECORDS",                // 88
    "COGNMEA",                             // 89
    "SOGNMEA",                             // 90
    "ApparentWindSpeedNMEA",               // 91
    "ApparentWindAngleNMEA",               // 92
    "TrueWindSpeedNMEA",                   // 93
    "TrueWindAngleNMEA",                   // 94
    "LeewayNMEA",                          // 95
    "VMGNMEA",                             // 96
    "VMGTargetBearing",                    // 97
    "VMGUseTrueWind",                      // 98
    "cpuLoadCore0",                        // 99
    "cpuLoadCore0Max",                     // 100
    "cpuLoadCore1",                        // 101
    "cpuLoadCore1Max",                     // 102
    "hasForcedUpdate",                     // 103
    "forcedFwVersionInt",                  // 104
    "forcedUpdateDeadline",                // 105
    "stateRevision",                       // 106
    "hardwarePresent",                     // 107
    "imu_accel_x_raw",                     // 108
    "imu_accel_y_raw",                     // 109
    "imu_accel_z_raw",                     // 110
    "imu_gyro_x_raw",                      // 111
    "imu_gyro_y_raw",                      // 112
    "imu_gyro_z_raw",                      // 113
    "accel_x_min",                         // 114
    "accel_x_max",                         // 115
    "accel_x_avg",                         // 116
    "accel_y_min",                         // 117
    "accel_y_max",                         // 118
    "accel_y_avg",                         // 119
    "accel_z_min",                         // 120
    "accel_z_max",                         // 121
    "accel_z_avg",                         // 122
    "gyro_x_min",                          // 123
    "gyro_x_max",                          // 124
    "gyro_x_avg",                          // 125
    "gyro_y_min",                          // 126
    "gyro_y_max",                          // 127
    "gyro_y_avg",                          // 128
    "gyro_z_min",                          // 129
    "gyro_z_max",                          // 130
    "gyro_z_avg",                          // 131
    "heel_min",                            // 132
    "heel_max",                            // 133
    "heel_avg",                            // 134
    "pitch_min",                           // 135
    "pitch_max",                           // 136
    "pitch_avg",                           // 137
    "vertical_accel_min",                  // 138
    "vertical_accel_max",                  // 139
    "vertical_accel_avg",                  // 140
    "total_accel_min",                     // 141
    "total_accel_max",                     // 142
    "total_accel_avg",                     // 143
    "imu_slam_count",                      // 144
    "imu_slam_peak_max",                   // 145
    "imu_slam_count_lifetime",             // 146
    "imu_capsize_count",                   // 147
    "imu_pitchpole_count",                 // 148
    "imu_heel_change_60s",                 // 149
    "imu_heel_deviation_60s",              // 150
    "imu_pitch_change_60s",                // 151
    "imu_pitch_deviation_60s",             // 152
    "imu_wave_period_sec",                 // 153
    "imu_heel_max_lifetime",               // 154
    "imu_pitch_max_lifetime",              // 155
    "imu_slam_peak_lifetime",              // 156
    "imu_fifo_overrun_count",              // 157
    "imu_i2c_error_count",                 // 158
    "imu_unknown_tag_count",               // 159
    "imu_accel_dropped",                   // 160
    "imu_gyro_dropped",                    // 161
    "imu_total_samples_accel",             // 162
    "imu_total_samples_gyro",              // 163
    "IMUReadTime2",                        // 164
    "IMUReadTime",                         // 165
    "adsI2CErrorCount",                    // 166
    "tempPIDActive",                       // 167
    "tempPIDInput_d",                      // 168
    "tempPIDSetpoint_d",                   // 169
    "thermalPenaltyAmps",                  // 170
    "innerTermP",                          // 171
    "innerTermI",                          // 172
    "innerTermD",                          // 173
    "outerTermP",                          // 174
    "outerTermI",                          // 175
    "outerTermD",                          // 176
    "thermalSlopeFPerSec",                 // 177
    "chargeStageDisplay",                  // 178
    "voltageControlActive",                // 179
    "voltageError",                        // 180
    "cv_I",                                // 181
    "inIdleStage",                         // 182
    "referenceFinalized",                  // 183
    "sessionErrorCount",                   // 184
    "ft_rai_total_win",                    // 185
    "ft_rai_total_ses",                    // 186
    "ft_rai_ina228_win",                   // 187
    "ft_rai_ina228_ses",                   // 188
    "ft_rai_ads_state_win",                // 189
    "ft_rai_ads_state_ses",                // 190
    "ft_rai_bmp_state_win",                // 191
    "ft_rai_bmp_state_ses",                // 192
    "ft_rai_imu_win",                      // 193
    "ft_rai_imu_ses",                      // 194
    "fsWriteQueueDrops",                   // 195
    "reserved196",                         // 196 reserved — was cv_D (D term removed)
    "tempReadFailCount",                   // 197
    "tempCrcFailCount",                    // 198
    "tempCrcRecoveredCount",               // 199
    "tempAllFFCount",                      // 200
    "tempPowerOn85Count",                  // 201
    "tempOutOfRangeCount",                 // 202
    "tempRequestFailCount",                // 203
    "tempConnectedFailCount",              // 204
    "tempResolutionFixCount",              // 205
    "tempRereadFailCount",                 // 206
    "tempResolutionFixCrcFailCount",       // 207
    "tempEnumerateFailCount",              // 208
    "warmupCeiling",                       // 209
    "imu_min_moving_gentle",               // 210
    "imu_min_moving_moderate",             // 211
    "imu_min_moving_rough",                // 212
    "imu_min_moving_extreme",              // 213
    "imu_min_stat_gentle",                 // 214
    "imu_min_stat_moderate",               // 215
    "imu_min_stat_rough",                  // 216
    "imu_min_stat_extreme",                // 217
    "imu_heel_deviation_120s",             // 218
    "imu_pitch_deviation_120s",            // 219
    "imu_heading_swing_120s",              // 220
    "dBcur_dt",                            // 221
    "loadDumpActive",                      // 222
    "thermalLiveScore0",                   // 223
    "thermalLiveScore1",                   // 224
    "thermalLiveScore2",                   // 225
    "thermalLiveScore3",                   // 226
    "thermalTuningTestPhase",              // 227
    "ft_updateAccelMetrics_win",           // 228
    "ft_updateAccelMetrics_ses",           // 229
    "WifiStrength",                        // 230
    "SendWifiTime",                        // 231
    "AnalogReadTime",                      // 232
    "VeTime",                              // 233
    "MaximumLoopTime",                     // 234
    "HeadingNMEA",                         // 235
    "EngineCycles",                        // 236
    "CurrentSessionDuration",              // 237
    "timeAxisModeChanging",                // 238
    "currentPartitionType",                // 239
    "fastOvCurrentCap",                    // 240
    "fastOvClampCount",                    // 241
    "fastOvHardCount",                     // 242 (was 243; 242 reserved — was fastOvSoftCount)
    "ch1_last_ms",                         // 244
    "ch1_avg_10s",                         // 245
    "ch1_worst_10s",                       // 246
    "ch1_over2x_10s",                      // 247
    "ch1_n_10s",                           // 248
    "ch1_avg_2m",                          // 249
    "ch1_worst_2m",                        // 250
    "ch1_over2x_2m",                       // 251
    "ch1_n_2m",                            // 252
    "ch1_avg_at",                          // 253
    "ch1_worst_at",                        // 254
    "ch1_over2x_at",                       // 255
    "ch1_n_at",                            // 256
    "iExcessCount",                        // 257
    "inaOVCount",                          // 258
    "hardOCCount",                         // 259
    "voltSpikeCount",                      // 260
    "voltDisagreeCritCount",               // 261
    "voltDisagreeWarnCount",               // 262
    "voltImplausibleCount",                // 263
    "tempCritCount",                       // 264
    "tempSustainedCount",                  // 265
    "tempStaleCount",                      // 266
    "currentStaleCount",                   // 267
    "imu_msi_score",                       // 268
    "imu_vomit_pct",                       // 269
    "imu_anchorage_comfort",               // 270
    "ina_last_ms",                         // 271
    "ina_avg_10s",                         // 272
    "ina_worst_10s",                       // 273
    "ina_over2x_10s",                      // 274
    "ina_avg_2m",                          // 275
    "ina_worst_2m",                        // 276
    "ina_over2x_2m",                       // 277
    "ina_avg_at",                          // 278
    "ina_worst_at",                        // 279
    "ina_over2x_at",                       // 280
    "loopTime5sWindow_ms",                 // 281
    "MaximumLoopTime_ms",                  // 282
    "ft_SendWifiData_win",                 // 283
    "ft_SendWifiData_ses",                 // 284
    "ft_CheckAlarms_win",                  // 285
    "ft_CheckAlarms_ses",                  // 286
    "ft_calculateDerivedMetrics_win",      // 287
    "ft_calculateDerivedMetrics_ses",      // 288
    "ft_logDashboardValues_win",           // 289
    "ft_logDashboardValues_ses",           // 290
    "ft_updateSystemHealthStats_win",      // 291
    "ft_updateSystemHealthStats_ses",      // 292
    "ft_checkWiFiConnection_win",          // 293
    "ft_checkWiFiConnection_ses",          // 294
    "ft_ch1_compute_stats_win",            // 295
    "ft_ch1_compute_stats_ses",            // 296
    "ft_UpdateEngineRuntime_win",          // 297
    "ft_UpdateEngineRuntime_ses",          // 298
    "ft_UpdateEngineFuel_win",             // 299
    "ft_UpdateEngineFuel_ses",             // 300
    "ft_UpdateBatterySOC_win",             // 301
    "ft_UpdateBatterySOC_ses",             // 302
    "ft_UpdateTravelStatistics_win",       // 303
    "ft_UpdateTravelStatistics_ses",       // 304
    "ft_UpdateDistanceThisInterval_win",   // 305
    "ft_UpdateDistanceThisInterval_ses",   // 306
    "ft_UpdateBoardTempPressureMaximums_win", // 307
    "ft_UpdateBoardTempPressureMaximums_ses", // 308
    "ft_handleSocGainReset_win",           // 309
    "ft_handleSocGainReset_ses",           // 310
    "ft_handleAltZeroReset_win",           // 311
    "ft_handleAltZeroReset_ses",           // 312
    "ft_calculateChargeTimes_win",         // 313
    "ft_calculateChargeTimes_ses",         // 314
    "ft_UpdateSailingMetrics_win",         // 315
    "ft_UpdateSailingMetrics_ses",         // 316
    "ft_updateWeatherMode_win",            // 317
    "ft_updateWeatherMode_ses",            // 318
    "ft_updateSensorWindow_win",           // 319
    "ft_updateSensorWindow_ses",           // 320
    "ft_checkTimeSync_win",                // 321
    "ft_checkTimeSync_ses",                // 322
    "currentRPMTableIndex",                // 323
    "pidInitialized",                      // 324
    "pidSetpoint",                         // 325
    "TempToUse",                           // 326
    "learningTargetFromRPM",               // 327
    "ambientTempCorrection",               // 328
    "finalLearningTarget",                 // 329
    "overheatingPenaltyTimer",             // 330
    "overheatingPenaltyAmps",              // 331
    "averageTableValue",                   // 332
    "timeSinceLastOverheat",               // 333
    "socInfoAvailable",                    // 334
    "overheatCount0",                      // 335
    "overheatCount1",                      // 336
    "overheatCount2",                      // 337
    "overheatCount3",                      // 338
    "overheatCount4",                      // 339
    "overheatCount5",                      // 340
    "overheatCount6",                      // 341
    "overheatCount7",                      // 342
    "overheatCount8",                      // 343
    "overheatCount9",                      // 344
    "cumulativeNoOverheatTime0",           // 345
    "cumulativeNoOverheatTime1",           // 346
    "cumulativeNoOverheatTime2",           // 347
    "cumulativeNoOverheatTime3",           // 348
    "cumulativeNoOverheatTime4",           // 349
    "cumulativeNoOverheatTime5",           // 350
    "cumulativeNoOverheatTime6",           // 351
    "cumulativeNoOverheatTime7",           // 352
    "cumulativeNoOverheatTime8",           // 353
    "cumulativeNoOverheatTime9",           // 354
    "learningUpCount0",                    // 355
    "learningUpCount1",                    // 356
    "learningUpCount2",                    // 357
    "learningUpCount3",                    // 358
    "learningUpCount4",                    // 359
    "learningUpCount5",                    // 360
    "learningUpCount6",                    // 361
    "learningUpCount7",                    // 362
    "learningUpCount8",                    // 363
    "learningUpCount9",                    // 364
    "totalLearningEvents",                 // 365
    "totalOverheats",                      // 366
    "totalSafeHours",                      // 367
    "FreeInternalRam",                     // 368
    "TotalInternalRam",                    // 369
    "LargestInternalBlock",                // 370
    "FreePSRAM",                           // 371
    "TotalPSRAM",                          // 372
    "Heapfrag",                            // 373
    "ft_ReadAnalogInputs_win",             // 374
    "ft_ReadAnalogInputs_ses",             // 375
    "ft_AdjustFieldLearnMode_win",         // 376
    "ft_AdjustFieldLearnMode_ses",         // 377
    "ft_uploadSensorHistory_win",          // 378
    "ft_uploadSensorHistory_ses",          // 379
    "ft_uploadBufferedRecords_win",        // 380
    "ft_uploadBufferedRecords_ses",        // 381
    "ft_buildConfigPayload_win",           // 382
    "ft_buildConfigPayload_ses",           // 383
    "VeTime2",                             // 384
    "systemIDRiseDelay_0",                 // 385
    "systemIDRiseDelay_1",                 // 386
    "systemIDRiseDelay_2",                 // 387
    "systemIDFallDelay_0",                 // 388
    "systemIDFallDelay_1",                 // 389
    "systemIDFallDelay_2",                 // 390
    "systemIDRiseAvg",                     // 391
    "systemIDFallAvg",                     // 392
    "nvsPhase",                            // 393
    "ft_saveNVSData_win",                  // 394
    "ft_saveNVSData_ses",                  // 395
    "ft_FlushFileWriteQueue_win",          // 396
    "ft_FlushFileWriteQueue_ses",          // 397
    "ft_efficiencyTracker_win",            // 398
    "ft_efficiencyTracker_ses",            // 399
    "systemIDActive",                      // 400
    "systemIDResultsReady",                // 401
    "systemIDStepAmp_0",                   // 402
    "systemIDStepAmp_1",                   // 403
    "systemIDStepAmp_2",                   // 404
    "systemIDQuietPP_0",                   // 405
    "systemIDQuietPP_1",                   // 406
    "systemIDQuietPP_2",                   // 407
    "nvsCycleMs",                          // 408 — ms elapsed for last complete NVS drain cycle
    "voltLoopWorstInterval_5s",            // 409 — worst voltage loop actual interval 5s window (ms)
    "voltLoopWorstInterval_ses",           // 410 — worst voltage loop actual interval since boot (ms)
    "fsFlushDeferred",                     // 411 — times FS flush skipped by co-fire guard (count)
];
const CSV3_FIELDS = [
    "TemperatureLimitF",               // 0
    "BulkVoltage",                     // 1
    "wavePeriod",                      // 2
    "FloatVoltage",                    // 3
    "SwitchingFrequency",              // 4
    "yyMin",                           // 5
    "FieldAdjustmentInterval",         // 6
    "ManualDutyTarget",                // 7
    "SwitchControlOverride",           // 8
    "waveAmplitude",                   // 9
    "CurrentThreshold",                // 10
    "PeukertExponent_scaled",          // 11
    "ChargeEfficiency_scaled",         // 12
    "ChargedVoltage_Scaled",           // 13
    "TailCurrent",                     // 14
    "ChargedDetectionTime",            // 15
    "IgnoreTemperature",               // 16
    "bmsLogic",                        // 17
    "bmsLogicLevelOff",                // 18
    "RPMScalingFactor",                // 19
    "MaximumAllowedBatteryAmps",       // 20
    "BatteryVoltageSource",            // 21
    "LearningUpwardEnabled",           // 22
    "LearningDownwardEnabled",         // 23
    "AlternatorNominalAmps",           // 24
    "LearningUpStep",                  // 25
    "LearningDownStep",                // 26
    "AmbientTempCorrectionFactor",     // 27
    "xTime",                           // 28
    "MinLearningInterval",             // 29
    "SafeOperationThreshold",          // 30
    "PidKp",                           // 31
    "PidKi",                           // 32
    "PidKd",                           // 33
    "PidSampleDivisor",                // 34
    "MaxTableValue",                   // 35
    "MaxPenaltyPercent",               // 36
    "MaxPenaltyDuration",              // 37
    "NeighborLearningFactor",          // 38
    "yyMax",                           // 39
    "LearningMemoryDuration",          // 40
    "EnableNeighborLearning",          // 41
    "EnableAmbientCorrection",         // 42
    "TuningMode",                      // 43
    "rpmCurrentTable0",                // 44
    "rpmCurrentTable1",                // 45
    "rpmCurrentTable2",                // 46
    "rpmCurrentTable3",                // 47
    "rpmCurrentTable4",                // 48
    "rpmCurrentTable5",                // 49
    "rpmCurrentTable6",                // 50
    "rpmCurrentTable7",                // 51
    "rpmCurrentTable8",                // 52
    "rpmCurrentTable9",                // 53
    "ShuntResistanceMicroOhm",         // 54
    "InvertAltAmps",                   // 55
    "InvertBattAmps",                  // 56
    "MaxDuty",                         // 57
    "MinDuty",                         // 58
    "FieldResistance",                 // 59
    "maxPoints",                       // 60
    "AlternatorCOffset",               // 61
    "BatteryCOffset",                  // 62
    "BatteryCapacity_Ah",              // 63
    "AmpSensorRange",                  // 64
    "R_fixed",                         // 65
    "Beta",                            // 66
    "T0_C",                            // 67
    "TempSource",                      // 68
    "IgnitionOverride",                // 69
    "FLOAT_DURATION",                  // 70
    "PulleyRatio",                     // 71
    "BatteryCurrentSource",            // 72
    "rpmTableRPMPoints0",              // 73
    "rpmTableRPMPoints1",              // 74
    "rpmTableRPMPoints2",              // 75
    "rpmTableRPMPoints3",              // 76
    "rpmTableRPMPoints4",              // 77
    "rpmTableRPMPoints5",              // 78
    "rpmTableRPMPoints6",              // 79
    "rpmTableRPMPoints7",              // 80
    "rpmTableRPMPoints8",              // 81
    "rpmTableRPMPoints9",              // 82
    "LearningSettlingPeriod",          // 83
    "LearningRPMChangeThreshold",      // 84
    "LearningTempHysteresis",          // 85
    "fuelTableRPM0",                   // 86
    "fuelTableRPM1",                   // 87
    "fuelTableRPM2",                   // 88
    "fuelTableRPM3",                   // 89
    "fuelTableRPM4",                   // 90
    "fuelTableRPM5",                   // 91
    "fuelTableRPM6",                   // 92
    "fuelTableRPM7",                   // 93
    "fuelTableRPM8",                   // 94
    "fuelTableRPM9",                   // 95
    "fuelTableGPH0",                   // 96
    "fuelTableGPH1",                   // 97
    "fuelTableGPH2",                   // 98
    "fuelTableGPH3",                   // 99
    "fuelTableGPH4",                   // 100
    "fuelTableGPH5",                   // 101
    "fuelTableGPH6",                   // 102
    "fuelTableGPH7",                   // 103
    "fuelTableGPH8",                   // 104
    "fuelTableGPH9",                   // 105
    "stateRevision",                   // 106
    "SetpointRampRate",                // 107
    "DutyRampRate",                    // 108
    "SettleTimeBeforeCut",             // 109
    "TempWarnExcess",                  // 110
    "TempCritExcess",                  // 111
    "TempSustainedTimeout",            // 112
    "AlternatorHardShutdownV",         // 113
    "VoltageDisagreeThreshold",        // 114
    "VoltageDisagreeTimeout",          // 115
    "rpmMinDutyTable0",                // 116
    "rpmMinDutyTable1",                // 117
    "rpmMinDutyTable2",                // 118
    "rpmMinDutyTable3",                // 119
    "rpmMinDutyTable4",                // 120
    "rpmMinDutyTable5",                // 121
    "rpmMinDutyTable6",                // 122
    "rpmMinDutyTable7",                // 123
    "rpmMinDutyTable8",                // 124
    "rpmMinDutyTable9",                // 125
    "rpmCapCurrentTable0",             // 126
    "rpmCapCurrentTable1",             // 127
    "rpmCapCurrentTable2",             // 128
    "rpmCapCurrentTable3",             // 129
    "rpmCapCurrentTable4",             // 130
    "rpmCapCurrentTable5",             // 131
    "rpmCapCurrentTable6",             // 132
    "rpmCapCurrentTable7",             // 133
    "rpmCapCurrentTable8",             // 134
    "rpmCapCurrentTable9",             // 135
    "VoltageKp",                       // 136
    "VoltageLoopInterval",             // 137
    "FIELD_COLLAPSE_DELAY",            // 138
    "SetpointRiseRate",                // 139
    "SetpointFallRate",                // 140
    "PIDTrackingGain",                 // 141
    "CAPSIZE_THRESHOLD_DEG",           // 142
    "PITCHPOLE_THRESHOLD_DEG",         // 143
    "SLAM_THRESHOLD_G",                // 144
    "imuMountOrientation",             // 145
    "TailCurrent_A",                   // 146
    "RebulkVoltage",                   // 147
    "rebulkDebounceTime",              // 148
    "MinFloatTime",                    // 149
    "SOC_BlockRebulk_percent",         // 150
    "SOC_AllowRebulk_percent",         // 151
    "accelEnabled",                    // 152
    "DutySlowRampRate",                // 153
    "ShutdownPhase2HoldMs",            // 154
    "TempPIDKp",                       // 155
    "TempPIDKi",                       // 156
    "ThermalLookaheadSec",             // 157
    "TempPIDIntervalMs",               // 158
    "TempPIDFilterAlpha",              // 159
    "VoltageKi",                       // 160
    "rpmCapPowerTable0",               // 161
    "rpmCapPowerTable1",               // 162
    "rpmCapPowerTable2",               // 163
    "rpmCapPowerTable3",               // 164
    "rpmCapPowerTable4",               // 165
    "rpmCapPowerTable5",               // 166
    "rpmCapPowerTable6",               // 167
    "rpmCapPowerTable7",               // 168
    "rpmCapPowerTable8",               // 169
    "rpmCapPowerTable9",               // 170
    "VoltageTrimLimit",                // 171
    "InputFilterTC",                   // 172
    "SystemIDStepAmplitude",           // 173
    "HardOCTripAmps",                  // 174
    "HardOCDebounceMs",                // 175
    "IExcessK",                        // 176
    "IExcessN",                        // 177
    "IExcessKBleed",                   // 178
    "IgnoreRPM",                       // 179
    "MinRPMForField",                  // 180
    "AwBleedRate",                     // 181
    "AwRecoverRate",                   // 182
    "KHard",                           // 183 (was 184; 183 reserved — was KSoft)
    "IExcessReseedFrac",               // 185
    "AwSeedProtectMs",                 // 186
    "reserved187",                     // 187 reserved — was VoltageKd (D term removed)
    "displayTempUnit",                 // 188
    "WarmupRampRate",                  // 189 (shifted -1 from prev)
    "OvGroup1Enable",                  // 190 (was 191; 190 reserved — was OvLayer1Enable)
    "OvGroup2Enable",                  // 192
    "IExcessSigSrc",                   // 193
    "IExcessMA_N",                     // 194
    "OutputPIDSigSrc",                 // 195
    "TdPred",                          // 196 — raw float (%.3f)
    "OvMeasMarginV",                   // 197 — raw float (%.3f)
    "OvPredMarginV",                   // 198 — raw float (%.3f)
    "OutputPIDMA_N",                   // 199
    "OutputPIDFilterTC",               // 200
    "VoltageFilterTC",                 // 201
    "ProtectionProxGateV",             // 202
    "SlopeBleedThresh",                // 203
    "SlopeBleedK",                     // 204
    "DvdtAlpha",                       // 205
    "SlopeBleedProxV",                 // 206
    "StartupRiseRate",                 // 207
    "absorptionCompleteTime",          // 208
    "OnOff",                           // 209
    "ManualFieldToggle",               // 210
    "HiLow",                           // 211
    "LimpHome",                        // 212
    "AlarmActivate",                   // 213
    "TempAlarm",                       // 214
    "VoltageAlarmHigh",                // 215
    "VoltageAlarmLow",                 // 216
    "CurrentAlarmHigh",                // 217
    "AlarmTest",                       // 218
    "AlarmLatchEnabled",               // 219
    "MaintainMode",                    // 220
    "ManualSOCPoint",                  // 221
    "LearningMode",                    // 222
    "LearningPaused",                  // 223
    "IgnoreLearningDuringPenalty",     // 224
    "ShowLearningDebugMessages",       // 225
    "LogAllLearningEvents",            // 226
    "CloudFeatures",                   // 227
    "LearningDryRunMode",              // 228
    "AutoShuntGainCorrection",         // 229
    "AutoAltCurrentZero",              // 230
    "WindingTempOffset",               // 231
    "ManualLifePercentage",            // 232
    "UVThresholdHigh",                 // 233
    "weatherModeEnabled",              // 234
    "SENSOR_UPLOAD_INTERVAL",          // 235
    "imuEnabled",                      // 236
    "AbsorptionVoltage",               // 237
    "AbsorptionTimeoutMs",             // 238
    "bulkVoltageHoldMs",               // 239
    "capLimitMode",                    // 240
    "TargetVoltageMode",               // 241
    "TargetVoltageSetpoint",           // 242
    "RebulkCurrent_A",                 // 243
    "UseFloat",                        // 244
    "anomalyMarginAmps",               // 245
    "anomalyAlarmThreshold",           // 246
    "anomalyAlarmEnable",              // 247
    "degradationThreshold",            // 248
    "TempAlarmLow",                    // 249
    "LoadDumpDtThresh",                // 250
    "LoadDumpDtThresh1",               // 251
    "CVTuningMode",                    // 252
    "cvWaveAmplitudeV",                // 253
    "cvWavePeriodSec",                 // 254
    "cvKOvershoot",                    // 255
    "cvConsecutiveReads",              // 256
    "ThermalTuningMode",               // 257
    "thermalWaveLowF",                 // 258
    "thermalWaveHighF",                // 259
    "thermalWaveHalfPeriodMin",        // 260
    "thermalKOvershoot",               // 261
    "thermalKUndershoot",              // 262
    "thermalSettleThreshF",            // 263
    "thermalConsecutiveReads",         // 264
    "webgaugesinterval",               // 265
    "plotTimeWindow",                  // 266
    "Ymin1",                           // 267
    "Ymax1",                           // 268
    "Ymin2",                           // 269
    "Ymax2",                           // 270
    "Ymin3",                           // 271
    "Ymax3",                           // 272
    "Ymin4",                           // 273
    "Ymax4",                           // 274
    "LoadDumpDtThresh3",               // 275
];
const TS_FIELDS = [
    "ts_HeadingNMEA",      // 0
    "ts_LatitudeNMEA",     // 1
    "ts_LongitudeNMEA",    // 2
    "ts_SatelliteCount",   // 3
    "ts_VictronVoltage",   // 4
    "ts_VictronCurrent",   // 5
    "ts_AlternatorTemp",   // 6
    "ts_ThermistorTemp",   // 7
    "ts_RPM",              // 8
    "ts_MeasuredAmps",     // 9
    "ts_BatteryV",         // 10
    "ts_IBV",              // 11
    "ts_Bcur",             // 12
    "ts_Channel3V",        // 13
    "ts_DutyCycle",        // 14
    "ts_FieldVolts",       // 15
    "ts_FieldAmps",        // 16
    "ts_CogNMEA",          // 17
    "ts_SogNMEA",          // 18
    "ts_AppWindSpeed",     // 19
    "ts_AppWindAngle",     // 20
    "ts_TrueWindSpeed",    // 21
    "ts_TrueWindAngle",    // 22
    "ts_Leeway",           // 23
    "ts_VMG",              // 24
    "ts_BaroPressure",     // 25
    "ts_AmbientTemp",      // 26
    "ts_IMU",              // 27
];

// Detect if running in Capacitor (iOS/Android) vs web browser
const IS_CAPACITOR = !!window.Capacitor;
const API_BASE_URL = IS_CAPACITOR ? 'http://alternator.local' : '';
// const API_BASE_URL = IS_CAPACITOR ? 'http://10.0.0.207' : ''; // worked first
// Alternative: Use mDNS hostname or fallback to IP
// const API_BASE_URL = IS_CAPACITOR ? 'http://192.168.4.1' : ''; // For AP mode
// const API_BASE_URL = IS_CAPACITOR ? 'http://alternator.local' : ''; // For Client mode with mDNS



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

        // Show reconnect button if it exists
        const reconnectBtn = document.getElementById('reconnect-button');
        if (reconnectBtn) {
            reconnectBtn.style.display = 'block';
        } else {
            console.error('reconnect-button element not found in HTML - button will not appear');
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
        }, false);


        // ── SSE: EffMatrix ─────────────────────────────────────────────
        // Payload: state,rBucket,tBucket,fBucket,rLabel,tLabel,fLabel,
        //          ss_seconds,avg_amps,min_amps,max_amps,is_reference_bin,sessionErrorCount

        source.addEventListener('EffMatrix', function (e) {
            const parts = e.data.split(',');
            if (parts.length < 13) return;

            effMatrixState.state = parseInt(parts[0]);
            effMatrixState.rBucket = parseInt(parts[1]);
            effMatrixState.tBucket = parseInt(parts[2]);
            effMatrixState.fBucket = parseInt(parts[3]);
            effMatrixState.rLabel = parts[4];
            effMatrixState.tLabel = parts[5];
            effMatrixState.fLabel = parts[6];
            effMatrixState.ss_seconds = parseInt(parts[7]);
            effMatrixState.avg_amps = parseFloat(parts[8]);
            effMatrixState.min_amps = parseFloat(parts[9]);
            effMatrixState.max_amps = parseFloat(parts[10]);
            effMatrixState.is_reference_bin = parseInt(parts[11]);
            effMatrixState.sessionErrorCount = parseInt(parts[12]);

            queueEffPlotUpdate();
            updateEffAnomalyDisplay();
        }, false);

        // ── SSE: EffRed ────────────────────────────────────────────────
        // Payload: valid,fieldVolts,amps,rpmBucket,tempBucket,fieldBucket

        source.addEventListener('EffRed', function (e) {
            const parts = e.data.split(',').map(Number);
            if (parts.length < 6) return;

            effRedDot.valid = parts[0] === 1;
            effRedDot.fieldVolts = parts[1];
            effRedDot.amps = parts[2];
            effRedDot.rpmBucket = parts[3];
            effRedDot.tempBucket = parts[4];
            effRedDot.fieldBucket = parts[5];

            queueEffPlotUpdate();
            updateEffAnomalyDisplay();
        }, false);

        source.addEventListener('EffHistory', function (e) {
            const parts = e.data.split(',');
            if (parts.length < 32) return;

            effHistory.count = parseInt(parts[0]);
            effHistory.head = parseInt(parts[1]);
            for (let i = 0; i < 30; i++) {
                effHistory.values[i] = parseFloat(parts[2 + i]) || 0;
            }

            renderEffSparkline();
        }, false);

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

    // Add visual demo banner
    const banner = document.createElement('div');
    banner.id = 'demo-banner';
    banner.style.cssText = 'position:fixed;top:0;left:0;right:0;background:#ff9800;color:#000;padding:10px;text-align:center;z-index:10000;font-weight:bold;font-size:14px;';
    banner.textContent = '⚠️ DEMO MODE - Simulated Data (No Hardware Connected)';
    document.body.insertBefore(banner, document.body.firstChild);

    // Adjust body padding to account for banner
    document.body.style.paddingTop = '40px';

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


// Also update header display values directly for demo mode
const headerElements = {
    'header-voltage': (12.5 + Math.random() * 0.8).toFixed(1),
    'header-soc': Math.floor(75 + Math.random() * 15),
    'header-alt-current': (40 + Math.random() * 30).toFixed(1),
    'header-batt-current': (35 + Math.random() * 25).toFixed(1),
    'header-alt-temp': Math.floor(75 + Math.random() * 20),
    'header-rpm': Math.floor(1800 + Math.random() * 600)
};

// Update each header element
Object.keys(headerElements).forEach(id => {
    const element = document.getElementById(id);
    if (element) {
        element.textContent = headerElements[id];
    }
});

// Update ignition status
const ignitionStatus = document.getElementById('ignition-status');
if (ignitionStatus) {
    ignitionStatus.textContent = 'ON';
    ignitionStatus.className = 'duo-num ignition-on';
}


function checkForDemoMode() {
    // Wait 3 seconds after load, then check if ESP32 connected
    setTimeout(() => {
        // If no EventSource connection established, enable demo mode
        if (!source || source.readyState === EventSource.CLOSED || source.readyState === EventSource.CONNECTING) {
            console.log('[DEMO MODE] No ESP32 detected after 3 seconds - enabling demo mode');
            enableDemoMode();
        }
    }, 3000);
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

        // Same endpoint ESP32 uses: Cloudflare's trace
        const response = await fetch('http://cloudflare.com/cdn-cgi/trace', {
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

        const response = await fetch('https://ota.xengineering.net/api/firmware/versions.php', {
            signal: controller.signal
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
                    <form action="/get" method="GET" target="hidden-form" onsubmit="return confirmUpdate('${version}')">
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
    return confirm(`⚠️ ALTERNATOR WILL BE AUTOMATICALLY DISABLED FOR SAFETY ⚠️\n\nUpdate process takes 2-3 minutes. Do not interfere with auto-reboots. When finished, web interface will be accessible in the usual way, and the Software Update sub-tab in Cloud Features will confirm the new version.\n\nIf process fails, you may try again with better internet. If the whole thing bricks, you may start fresh with the factory golden image, which will never force updates.\n\nAlternator will remain OFF after update - you must manually re-enable it.`);
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

        // Update only the profile display
        document.getElementById('profile-device-uid').textContent = macAddress;

        prevDeviceIdUpper = deviceIdUpper;
        prevDeviceIdLower = deviceIdLower;
    }
}

//forced OTA stuff
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
        padding: 28px 36px;
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
    const isPastDeadline = now >= deadline;

    if (isPastDeadline) {
        // BLOCK EVERYTHING - show overlay
        banner.style.display = 'none';
        overlay.style.display = 'flex';

        overlay.innerHTML = `
          <div class="settings-card" style="max-width: 520px; width: 100%; text-align: center;">
              <div class="section-title" style="margin-bottom: 12px; text-align:center;">
                  Forced Firmware Update
              </div>
              <p style="margin: 8px 0 4px 0; font-size: 15px;">
                  This device must upgrade to firmware v<strong>${versionStr}</strong>.
              </p>
              <p style="margin: 4px 0 16px 0; font-size: 13px; color: #666;">
                  The deadline has passed!
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
    const confirmed = confirm('Update process begins in ~7 seconds and takes 2-3 minutes, includes re-boots.  Do not interfere.  Web interface will then be accessible in the usual way, and Software Update sub-tab in Cloud Features will show the new version #.  If process fails, you may try again with better internet.  If the whole thing bricks, you may start fresh with the factory golden image.  Continue?');
    if (confirmed) {
        const form = document.createElement('form');
        form.action = '/get';
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
        // Destroy and recreate all plots
        if (currentTempPlot) { currentTempPlot.destroy(); initCurrentTempPlot(); }
        if (voltagePlot) { voltagePlot.destroy(); initVoltagePlot(); }
        if (rpmPlot) { rpmPlot.destroy(); initRPMPlot(); }
        if (temperaturePlot) { temperaturePlot.destroy(); initTemperaturePlot(); }

        configChanged = true;
    }

    // Check for buffer size changes (requires fresh start)
    if (data.plotTimeWindow !== cachedPlotTimeWindow ||
        data.webgaugesinterval !== cachedWebgaugesInterval) {

        cachedPlotTimeWindow = data.plotTimeWindow;
        cachedWebgaugesInterval = data.webgaugesinterval;

        // Reinitialize plots with new timing parameters
        reinitializePlotsWithNewTiming(data);

        // Destroy and recreate all plots to fix X-axis labels
        if (currentTempPlot) { currentTempPlot.destroy(); initCurrentTempPlot(); }
        if (voltagePlot) { voltagePlot.destroy(); initVoltagePlot(); }
        if (rpmPlot) { rpmPlot.destroy(); initRPMPlot(); }
        if (temperaturePlot) { temperaturePlot.destroy(); initTemperaturePlot(); }

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

        // Destroy and recreate PID tuning plot
        if (pidTuningPlot) {
            pidTuningPlot.destroy();
            initPidTuningPlot();
        }

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
            cvTuningPlot.destroy();
            initCVTuningDataStructures();
            initCVTuningPlot();
        }

        configChanged = true;
    }

    if (configChanged) {
    }
}

// Function to reinitialize plots when timing parameters change
function reinitializePlotsWithNewTiming(data) {
    // Calculate new buffer size
    const newMaxPoints = Math.ceil((data.plotTimeWindow * 1000) / data.webgaugesinterval);
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
        { key: 'ManualSOCPoint', id: 'ManualSOCPoint_echo', transform: v => v },
        { key: 'BatteryVoltageSource', id: 'BatteryVoltageSource_echo', transform: v => v },
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
        { key: 'webgaugesinterval', id: 'webgaugesinterval_echo', transform: v => v },
        { key: 'plotTimeWindow', id: 'plotTimeWindow_echo', transform: v => v },
        { key: 'Ymin1', id: 'Ymin1_echo', transform: v => v },
        { key: 'Ymax1', id: 'Ymax1_echo', transform: v => v },
        { key: 'Ymin2', id: 'Ymin2_echo', transform: v => (v / 100).toFixed(2) },
        { key: 'Ymax2', id: 'Ymax2_echo', transform: v => (v / 100).toFixed(2) },
        { key: 'Ymin3', id: 'Ymin3_echo', transform: v => v },
        { key: 'Ymax3', id: 'Ymax3_echo', transform: v => v },
        { key: 'Ymin4', id: 'Ymin4_echo', transform: v => v },
        { key: 'Ymax4', id: 'Ymax4_echo', transform: v => v },
        { key: 'weatherModeEnabled', id: 'weatherModeEnabled_echo', transform: v => v == 1 ? 'On' : 'Off' },
        { key: 'SolarWatts', id: 'SolarWatts_echo', transform: v => v },
        { key: 'performanceRatio', id: 'performanceRatio_echo', transform: v => (v / 100).toFixed(2) },
        { key: 'UVThresholdHigh', id: 'UVThresholdHigh_echo', transform: v => v },
        { key: 'accelEnabled', id: 'accelEnabled_echo', transform: v => v == 1 ? 'On' : 'Off' },
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
        { key: 'SENSOR_UPLOAD_INTERVAL', id: 'SENSOR_UPLOAD_INTERVAL_echo', transform: v => (v / 60000).toFixed(2) },
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
        { key: 'AwRecoverRate',     id: 'AwRecoverRate_echo',     transform: v => (v / 10).toFixed(2) },
        { key: 'KHard',             id: 'KHard_echo',             transform: v => (v / 10).toFixed(1) },
        { key: 'IExcessReseedFrac', id: 'IExcessReseedFrac_echo', transform: v => (v / 100).toFixed(2) },
        { key: 'AwSeedProtectMs',   id: 'AwSeedProtectMs_echo',   transform: v => v },
        { key: 'OvGroup1Enable',    id: 'OvGroup1Enable_echo',    transform: v => v == 1 ? 'ON' : 'OFF' },
        { key: 'OvGroup2Enable',    id: 'OvGroup2Enable_echo',    transform: v => v == 1 ? 'ON' : 'OFF' },
        { key: 'TdPred',            id: 'TdPred_echo',            transform: v => v.toFixed(3) },
        { key: 'OvMeasMarginV',     id: 'OvMeasMarginV_echo',     transform: v => v.toFixed(3) },
        { key: 'OvPredMarginV',     id: 'OvPredMarginV_echo',     transform: v => v.toFixed(3) },
        { key: 'KHard',             id: 'KHard_echo2',            transform: v => v.toFixed(1) },
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
        { key: 'anomalyMarginAmps', id: 'anomalyMarginAmps_echo', transform: v => (v / 10).toFixed(1) },
        { key: 'anomalyAlarmThreshold', id: 'anomalyAlarmThreshold_echo', transform: v => v },
        { key: 'anomalyAlarmEnable', id: 'anomalyAlarmEnable_echo', transform: v => v == 1 ? 'ON' : 'OFF' },
        { key: 'degradationThreshold', id: 'degradationThreshold_echo', transform: v => (v).toFixed(2) },
        { key: 'fsWriteQueueDropsID', id: 'fsWriteQueueDrops', transform: v => v },
        { key: 'InputFilterTC', id: 'InputFilterTC_echo',      transform: v => v },
        { key: 'InputFilterTC', id: 'InputFilterTC_ID',        transform: v => v },
        { key: 'InputFilterTC', id: 'InputFilterTC_echo_grp3', transform: v => v },
        { key: 'OutputPIDFilterTC', id: 'OutputPIDFilterTC_echo_pid', transform: v => v },
        { key: 'ProtectionProxGateV',   id: 'ProtectionProxGateV_echo',       transform: v => (v / 100).toFixed(2) },
        { key: 'SlopeBleedThresh',      id: 'SlopeBleedThresh_echo',          transform: v => (v / 100).toFixed(2) },
        { key: 'SlopeBleedK',           id: 'SlopeBleedK_echo',               transform: v => v },
        { key: 'DvdtAlpha',             id: 'DvdtAlpha_echo',                 transform: v => (v / 1000).toFixed(3) },
        { key: 'SlopeBleedProxV',       id: 'SlopeBleedProxV_echo',           transform: v => (v / 100).toFixed(2) },
        { key: 'StartupRiseRate',       id: 'StartupRiseRate_echo',           transform: v => (v / 100).toFixed(2) },
        { key: 'SystemIDStepAmplitude', id: 'SystemIDStepAmplitude_echo', transform: v => v },
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
        { key: 'BatteryCurrentSource',  name: 'BatteryCurrentSource' },
    ];
    selectSyncs.forEach(({ key, name }) => {
        if (key in data) {
            const sel = document.querySelector(`select[name="${name}"]`);
            if (sel && sel.value !== String(data[key])) sel.value = String(data[key]);
        }
    });

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
    const plotControls = document.getElementById('plots-controls');
    if (plotControls) plotControls.classList.add("locked");
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

    try {
        const response = await fetchWithTimeout(buildURL('/checkRegistration'), {
            method: 'POST',
            body: formData
        }, 8000);

        if (!response.ok) throw new Error(`HTTP ${response.status}`);

        const data = await response.json();

        if (data.registered && data.valid) {
            isDeviceRegistered = true; // Device already registered
            populateProfileForm(data.profile);
            document.getElementById('profile-form').querySelector('input[type="submit"]').value = 'Update Profile';
        } else {
            document.getElementById('profile-form').querySelector('input[type="submit"]').value = 'Register Device';
        }

    } catch (error) {
        diagError('Error in initializeProfileTab:', error);
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
        form.BOAT_YEAR.value = data.boat_year || 2025;
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

    } catch (error) {
        diagLog('Vessel info not found or invalid:', error);
    }
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

            messageDiv.style.backgroundColor = '#e8f5e9';
            messageDiv.style.color = '#2e7d32';
            messageDiv.textContent = 'Vessel info saved successfully! You can now access other tabs.';

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
        .then(response => {
            return response.json().then(data => ({ httpStatus: response.status, data }));
        })
        .then(({ httpStatus, data }) => {
            if (data.success) {
                messageDiv.style.backgroundColor = '#e8f5e9';
                messageDiv.style.color = '#2e7d32';
                messageDiv.textContent = 'Profile saved successfully!';
                isDeviceRegistered = true;
            } else {
                messageDiv.style.backgroundColor = '#ffebee';
                messageDiv.style.color = '#c62828';
                messageDiv.textContent = 'Error: ' + (data.error || `HTTP ${httpStatus}`);
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

    if (!confirm('⚠️ WARNING: This will permanently delete ALL your cloud data including history, profile, and statistics. This CANNOT be undone.\n\nYour device will continue to work locally, but all cloud features is reset.\n\nType DELETE in the next prompt to confirm.')) {
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
        .then(response => {
            if (!response.ok) throw new Error(`HTTP ${response.status}`);
            return response.json();
        })
        .then(data => {
            if (data.success) {
                alert('All data deleted successfully. Cloud features have been reset.');
                location.reload();
            } else {
                alert('Error: ' + (data.error || 'Deletion failed'));
            }
        })
        .catch(err => {
            alert('Network error: ' + err.message);
        });
}

function resetThermalPID() {
    if (!confirm('Reset thermal PID? Integrator and filter will be cleared and rebuilt from scratch.')) return;
    fetch('/resetThermalPID', { method: 'POST' })
        .then(r => r.ok ? console.log('Thermal PID reset') : console.warn('Reset failed'))
        .catch(err => console.warn('Reset error:', err));
}

function resetInnerPID() {
    if (!confirm('Reset output current PID? Integrator will be zeroed and duty will ramp up from 0 via slew limiter.')) return;
    fetch('/resetInnerPID', { method: 'POST' })
        .then(r => r.ok ? console.log('Output current PID reset') : console.warn('Reset failed'))
        .catch(err => console.warn('Reset error:', err));
}

function resetVoltageLoop() {
    if (!confirm('Reset CV integrator (cv_I)? The voltage loop will rebuild from zero on the next tick.')) return;
    fetch('/resetVoltageLoop', { method: 'POST' })
        .then(r => r.ok ? console.log('Voltage loop reset') : console.warn('Reset failed'))
        .catch(err => console.warn('Reset error:', err));
}

// ============================================================================
// PID TUNING SCORE LOG
// ============================================================================

let _tuningLogPollTimer = null;

function fetchTuningLog() {
    fetch('/tuninglog')
        .then(r => r.ok ? r.json() : null)
        .then(data => { if (data) renderTuningLog(data); })
        .catch(() => {});
}

function renderTuningLog(data) {
    // Update live score displays (Settings panel + Live Data → Alternator mirror)
    const liveLabels = ['1m', '10m', '100m', '1000m'];
    (data.live || []).forEach((v, i) => {
        const txt = v > 0 ? liveLabels[i] + ': ' + v.toFixed(2) : liveLabels[i] + ': —';
        const el = document.getElementById('liveScore' + i);
        if (el) el.textContent = txt;
        const elAlt = document.getElementById('liveScoreAlt' + i);
        if (elAlt) elAlt.textContent = txt;
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
    fetch(buildURL('/get?commitTuningScore=1&password=' + encodeURIComponent(pw)))
        .then(r => {
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
    fetch('/resettuninglog', { method: 'POST' })
        .then(() => fetchTuningLog())
        .catch(() => {});
}

function commitCVTuningScore() {
    const btn = document.getElementById('commitCVTuningBtn');
    const status = document.getElementById('cvTuningCommitStatus');
    if (btn) btn.disabled = true;
    if (status) status.textContent = 'Sending…';
    const pw = currentAdminPassword || '';
    fetch(buildURL('/get?commitCVTuningScore=1&password=' + encodeURIComponent(pw)))
        .then(r => {
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
        .then(() => {
            if (status) status.textContent = 'Restarted.';
            setTimeout(() => { if (status) status.textContent = ''; }, 3000);
            fetchCVTuningLog();
        })
        .catch(() => {
            if (status) status.textContent = 'Send failed.';
        });
}

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
    fetch('/cvtuninglog')
        .then(r => r.ok ? r.json() : null)
        .then(data => { if (data) renderCVTuningLog(data); })
        .catch(() => {});
}

function renderCVTuningLog(data) {
    // Update CV live score displays (Score Log + Live Data mirror)
    const cvLiveLabels = ['1m', '10m', '100m', '1000m'];
    (data.live || []).forEach((v, i) => {
        const txt = v > 0 ? cvLiveLabels[i] + ': ' + v.toFixed(2) : cvLiveLabels[i] + ': —';
        const el = document.getElementById('cvLiveScore' + i);
        if (el) el.textContent = txt;
        const elAlt = document.getElementById('cvLiveScoreAlt' + i);
        if (elAlt) elAlt.textContent = txt;
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
    fetch('/resetcvtuninglog', { method: 'POST' })
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
    fetch('/thermaltuninglog')
        .then(r => r.ok ? r.json() : null)
        .then(data => { if (data) renderThermalTuningLog(data); })
        .catch(() => {});
}

function renderThermalTuningLog(data) {
    // Update thermal live score displays (4 windows: 30m, 3h, 24h, 7d)
    const labels = ['30m', '3h', '24h', '7d'];
    (data.live || []).forEach((v, i) => {
        const txt = v > 0 ? labels[i] + ': ' + v.toFixed(4) : labels[i] + ': —';
        const el = document.getElementById('thermalLiveScore' + i);
        if (el) el.textContent = txt;
        // Also update the always-on Alternator Live Data tab spans
        const altEl = document.getElementById('thermalLiveScoreAlt' + i);
        if (altEl) altEl.textContent = txt;
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
    fetch('/resetthermaltuninglog', { method: 'POST' })
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

// ── CV Voltage Loop Tuning Plot ────────────────────────────────────────────
let cvTuningPlot = null;
let cvTuningData = null;
let cvTuningPlotResizeObserver = null;

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
            },
            {
                scale: 'amps',
                label: 'Current (A)',
                side:  1,
                grid:  { show: false },
            },
        ],
        scales: {
            x:     { time: false, auto: false, range: [cvTuningData[0][0], cvTuningData[0][cvTuningData[0].length - 1]] },
            volts: (cvVoltsMin !== null && cvVoltsMax !== null) ? { auto: false, range: [cvVoltsMin, cvVoltsMax] } : {},
            amps:  (cvAmpsMin  !== null && cvAmpsMax  !== null) ? { auto: false, range: [cvAmpsMin,  cvAmpsMax]  } : {},
        },
    };

    if (cvTuningPlot) cvTuningPlot.destroy();
    cvTuningPlot = new uPlot(opts, cvTuningData, plotEl);
    if (document.body.classList.contains('dark-mode')) updateUplotTheme(cvTuningPlot);
    createCVTuningLegend();

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

function setCVXTime(val) {
    const v = parseFloat(val);
    if (!isFinite(v) || v <= 0) return;
    cvXTime = v;
    if (cvTuningPlot) cvTuningPlot.destroy();
    initCVTuningDataStructures();
    initCVTuningPlot();
}

function setCVVoltsRange(minVal, maxVal) {
    const mn = parseFloat(minVal), mx = parseFloat(maxVal);
    if (!isFinite(mn) || !isFinite(mx) || mn >= mx) return;
    cvVoltsMin = mn; cvVoltsMax = mx;
    if (cvTuningPlot) cvTuningPlot.setScale('volts', { min: mn, max: mx });
}

function setCVAmpsRange(minVal, maxVal) {
    const mn = parseFloat(minVal), mx = parseFloat(maxVal);
    if (!isFinite(mn) || !isFinite(mx) || mn >= mx) return;
    cvAmpsMin = mn; cvAmpsMax = mx;
    if (cvTuningPlot) cvTuningPlot.setScale('amps', { min: mn, max: mx });
}

function resetCVAxisRanges() {
    cvVoltsMin = null; cvVoltsMax = null;
    cvAmpsMin  = null; cvAmpsMax  = null;
    if (cvTuningPlot) cvTuningPlot.destroy();
    initCVTuningDataStructures();
    initCVTuningPlot();
}

function resetVoltageProtectionCounters() {
    if (!confirm('Reset all voltage & current protection counters? FastOV, iExcess, INA OV, hard OC, spike, and disagree counts will be cleared.')) return;
    fetch('/resetVoltageProtectionCounters', { method: 'POST' })
        .then(r => r.ok ? console.log('Voltage protection counters reset') : console.warn('Reset failed'))
        .catch(err => console.warn('Reset error:', err));
}

function resetThermalProtectionCounters() {
    if (!confirm('Reset thermal protection event counters? Temp critical, sustained, and stale counts will be cleared.')) return;
    fetch('/resetThermalProtectionCounters', { method: 'POST' })
        .then(r => r.ok ? console.log('Thermal protection counters reset') : console.warn('Reset failed'))
        .catch(err => console.warn('Reset error:', err));
}

function resetTempTaskCounters() {
    if (!confirm('Reset DS18B20 sensor health counters? All read/CRC/fail counts will be cleared.')) return;
    fetch('/resetTempTaskCounters', { method: 'POST' })
        .then(r => r.ok ? console.log('TempTask counters reset') : console.warn('Reset failed'))
        .catch(err => console.warn('Reset error:', err));
}


// ============================================
// CLOUD FEATURES - My History
// ============================================
async function redirectToHistory() {
    const statusEl = document.getElementById('history-status');
    const iframe = document.getElementById('history-iframe');

    if (!iframe) {
        diagError("history-iframe element not found");
        return;
    }

    if (statusEl) statusEl.textContent = 'Retrieving authentication token...';

    try {
        const response = await fetchWithTimeout(buildURL('/getAuthToken'), {}, 8000);
        if (!response.ok) {
            throw new Error(`HTTP ${response.status}`);
        }

        const data = await response.json();
        if (data.registered && data.token) {
            if (statusEl) statusEl.textContent = 'Loading history viewer...';
            // IMPORTANT: only set src if it hasn't been set to the history URL yet
            const targetUrl = `https://supabase-nine-ashy.vercel.app/?token=${encodeURIComponent(data.token)}`;
            // iframe.src is always an absolute URL once set; handle initial about:blank
            const currentSrc = iframe.getAttribute('src') || '';

            if (
                !currentSrc ||                       // empty
                currentSrc === 'about:blank' ||      // default
                currentSrc === '#'                   // any placeholder you might be using
            ) {
                iframe.src = targetUrl;
            } else {
            }

            iframe.style.display = 'block';

            iframe.onload = function () {
                if (statusEl) statusEl.style.display = 'none';
            };
        } else {
            if (statusEl) {
                statusEl.innerHTML =
                    'Device not registered. Please complete registration in <strong>My Profile</strong> tab first.';
                statusEl.style.color = '#ff6b6b';
            }
        }

    } catch (error) {
        diagError('Error in redirectToHistory:', error);
        if (statusEl) {
            statusEl.textContent = 'Error: Could not connect to device';
            statusEl.style.color = '#ff6b6b';
        }
    }
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
            iframe.src = `https://supabase-nine-ashy.vercel.app/leaderboards.html?token=${data.token}`;
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
        iframe.src = 'https://supabase-nine-ashy.vercel.app/fleet-stats.html';
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
    const timeWindowSec = window._lastKnownTimeWindow || 8; // This is in SECONDS
    const timeWindowMs = timeWindowSec * 1000; // Convert to milliseconds
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
            x: { time: true },
            current: { auto: false, range: [Ymin1, Ymax1] },
            pct: { auto: false, range: [0, 100] }
        } : {
            x: {
                time: false,
                auto: false,
                range: [xAxisData[0], xAxisData[xAxisData.length - 1]]
            },
            current: { auto: false, range: [Ymin1, Ymax1] },
            pct: { auto: false, range: [0, 100] }
        },
        axes: useTimestamps ? [
            { grid: { show: true } },
            {
                scale: "current",
                label: "Amperes",
                grid: { show: true },
                side: 3
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
                side: 3
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
                        ]);

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
                stroke: "#607D8B",
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
            x: { time: true },
            voltage: { auto: false, range: [Ymin2 / 100, Ymax2 / 100] },
            pct: { auto: false, range: [0, 100] }
        } : {
            x: {
                time: false,
                auto: false,
                range: [xAxisData[0], xAxisData[xAxisData.length - 1]]
            },
            voltage: { auto: false, range: [Ymin2 / 100, Ymax2 / 100] },
            pct: { auto: false, range: [0, 100] }
        },
        axes: useTimestamps ? [
            { grid: { show: true } },
            {
                scale: "voltage",
                label: "Volts",
                grid: { show: true },
                side: 3
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
                side: 3
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
                            { label: "INA Battery (V)", color: "#607D8B" },
                            { label: "Field %", color: "#9E9E9E" }
                        ]);

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
            x: { time: true },
            rpm: { auto: false, range: [Ymin3, Ymax3] },
            pct: { auto: false, range: [0, 100] }
        } : {
            x: {
                time: false,
                auto: false,
                range: [xAxisData[0], xAxisData[xAxisData.length - 1]]
            },
            rpm: { auto: false, range: [Ymin3, Ymax3] },
            pct: { auto: false, range: [0, 100] }
        },

        axes: useTimestamps ? [
            { grid: { show: true } },
            {
                scale: "rpm",
                label: "revs/min",
                grid: { show: true },
                side: 3
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
                side: 3
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
                        ]);

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
            x: { time: true },
            temperature: { auto: false, range: [Ymin4, Ymax4] },
            pct: { auto: false, range: [0, 100] }
        } : {
            x: {
                time: false,
                auto: false,
                range: [xAxisData[0], xAxisData[xAxisData.length - 1]]
            },
            temperature: { auto: false, range: [Ymin4, Ymax4] },
            pct: { auto: false, range: [0, 100] }
        },
        axes: useTimestamps ? [
            { grid: { show: true } },
            {
                scale: "temperature",
                label: "F",
                grid: { show: true },
                side: 3
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
                side: 3
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
                        ]);

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
    applyStaleStyleByAge("AltTempID", sa.alternatorTemp, STALE_THRESHOLD_TEMP_MS);
    applyStaleStyleByAge("temperatureThermistorID", sa.thermistorTemp, STALE_THRESHOLD_TEMP_MS);
    applyStaleStyleByAge("RPMID", sa.rpm);
    applyStaleStyleByAge("MeasAmpsID", sa.measuredAmps);
    applyStaleStyleByAge("BatteryVID", sa.batteryV);
    applyStaleStyleByAge("IBVID", sa.ibv);
    applyStaleStyleByAge("BCurrID", sa.bcur);
    applyStaleStyleByAge("ADS3ID", sa.channel3V);
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
    applyStaleStyleByAge("VMGNMEA_ID", sa.vmg);
    applyStaleStyleByAge("LeewayNMEA_ID", sa.leeway);
    applyStaleStyleByAge("ApparentWindSpeedNMEA_ID", sa.appWindSpeed);
    applyStaleStyleByAge("ApparentWindAngleNMEA_ID", sa.appWindAngle);
    applyStaleStyleByAge("TrueWindSpeedNMEA_ID", sa.trueWindSpeed);
    applyStaleStyleByAge("TrueWindAngleNMEA_ID", sa.trueWindAngle);

    // --- Baro / ambient — dedicated timestamps ---
    applyStaleStyleByAge("baroPressureID", sa.baroPressure);
    applyStaleStyleByAge("ambientTempID", sa.ambientTemp, STALE_THRESHOLD_TEMP_MS);

    // --- IMU — all displays share one timestamp ---
    applyStaleStyleByAge("imu_heel_deg_ID", sa.imu);
    applyStaleStyleByAge("imu_pitch_deg_ID", sa.imu);
    applyStaleStyleByAge("imu_vertical_accel_g_ID", sa.imu);
    applyStaleStyleByAge("imu_total_accel_g_ID", sa.imu);
    applyStaleStyleByAge("imu_yaw_rate_dps_ID", sa.imu);
    applyStaleStyleByAge("imu_wave_period_sec_ID", sa.imu);
    applyStaleStyleByAge("imu_hf_vibration_energy_ID", sa.imu);
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



// Custom legend creation function
function createCustomLegend(plotId, legendItems) {
    const plotContainer = document.getElementById(plotId);
    if (!plotContainer) return;

    // Remove any existing custom legend
    const existingLegend = plotContainer.querySelector('.custom-legend');
    if (existingLegend) {
        existingLegend.remove();
    }

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

    legendItems.forEach(item => {
        const legendItem = document.createElement('div');
        legendItem.style.cssText = `
  display: flex;
  align-items: center;
  gap: 6px;
  font-size: 12px;
`;

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

        legendItem.appendChild(colorBox);
        legendItem.appendChild(label);
        legendDiv.appendChild(legendItem);
    });

    plotContainer.appendChild(legendDiv);
}


function setAdminPassword() {
    const button = document.getElementById('admin_password_set');
    const input = document.getElementById('admin_password');
    const msg = document.getElementById('admin_password_msg');
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
                const plotControls = document.getElementById('plots-controls');
                if (plotControls) plotControls.classList.remove("locked");
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
            }


            else {
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

    const source = document.getElementById('gps-source');
    if (lat === 0.0 && lon === 0.0) {
        source.textContent = '(No GPS - manual entry required)';
        source.style.color = '#ff6464';
    } else {
        source.textContent = '(from GPS compass or manual)';
        source.style.color = 'var(--accent)';
    }
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
        updateCheckbox("timeAxisModeChanging_checkbox", data.timeAxisModeChanging, "timeAxisModeChanging");
        updateCheckbox("weatherModeEnabled_checkbox", data.weatherModeEnabled, "weatherModeEnabled");
        updateCheckbox("accelEnabled_checkbox", data.accelEnabled, "accelEnabled");
        updateCheckbox("UseFloat_checkbox", data.UseFloat, "UseFloat");


        updateCheckbox("anomalyAlarmEnable_checkbox", data.anomalyAlarmEnable, "anomalyAlarmEnable");
        updateCheckbox("TuningMode_checkbox", data.TuningMode, "TuningMode");
        updateCheckbox("socInfoAvailable_checkbox", data.socInfoAvailable, "socInfoAvailable");
        updateCheckbox("CloudFeatures_checkbox", data.CloudFeatures, "CloudFeatures");
        if (data.CloudFeatures !== undefined) {
            updateCloudFeaturesTabVisibility(data.CloudFeatures === 1);
        }
        updateCheckbox("VMGUseTrueWind_checkbox", data.VMGUseTrueWind, "VMGUseTrueWind");
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

    document.getElementById = function (id) {
        const element = originalGetElementById.call(document, id);
        if (!element && !warnedElements.has(id)) {
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
    const plotControls = document.getElementById('plots-controls');
    if (plotControls) plotControls.classList.add("locked");
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

    // Load Settings > Vessel Info if incomplete, otherwise Live Data > Alternator
    if (!vesselInfoComplete) {
        showMainTab('settings');
        showSubTab('settings', 'vessel-info');
    } else {
        showMainTab('livedata');  // safety net inside showMainTab activates alternator sub-tab
    }
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
    initEffPlot();
    startInterpLoop();
    //initEffPlotAxisListeners();

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

    // Add event listeners to source after initialization
    if (source) {
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
        setTrackedInterval(function () {
            const timeSinceLastEvent = Date.now() - lastEventTime;
            if (timeSinceLastEvent > 9000) { // 9 seconds without data = disconnected (was working well at 8s for a long time)
                updateInlineStatus(false);
                markAllReadingsStale(); //gray out
            }
        }, 2000); // Check every 1 seconds (was working well at 2s for a long time)


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
            }
            window._debugData = data;


            const fieldIndicator = document.getElementById('field-status');
            const fieldWrapper = fieldIndicator ? fieldIndicator.closest('.reading-value') : null;
            const dutyCycleDisplay = document.getElementById('dutyCycleID3');

            if (fieldIndicator) {
                if (data.fieldActiveStatus === 1) {
                    fieldIndicator.textContent = 'ACTIVE';
                    if (fieldWrapper) fieldWrapper.className = 'reading-value header-field-cluster field-status-active';
                    if (dutyCycleDisplay) dutyCycleDisplay.style.display = 'inline';
                } else if (data.fieldActiveStatus === 2) {
                    fieldIndicator.textContent = 'RAMP DOWN';
                    if (fieldWrapper) fieldWrapper.className = 'reading-value header-field-cluster field-status-rampdown';
                    if (dutyCycleDisplay) dutyCycleDisplay.style.display = 'inline';
                } else if (data.fieldActiveStatus === 3) {
                    fieldIndicator.textContent = 'MANUAL';
                    if (fieldWrapper) fieldWrapper.className = 'reading-value header-field-cluster field-status-manual';
                    if (dutyCycleDisplay) dutyCycleDisplay.style.display = 'inline';
                } else {
                    fieldIndicator.textContent = 'OFF';
                    if (fieldWrapper) fieldWrapper.className = 'reading-value header-field-cluster field-status-inactive';
                    if (dutyCycleDisplay) dutyCycleDisplay.style.display = 'none';
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
                    else if (["imu_vertical_accel_g", "imu_total_accel_g", "imu_hf_vibration_energy"].includes(key)) {
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
                    else if (["BatteryV", "uTargetAmps", "Ymin2", "Ymax2", "setpointLimited", "pidInput", "pidOutput", "pidError", "Channel3V", "IBV", "VictronVoltage", "vvout", "imu_heel_deg", "imu_pitch_deg", "imu_yaw_rate_dps", "fastOvCurrentCap", "ch1_avg_10s", "ch1_avg_2m", "ch1_avg_at", "ina_avg_10s", "ina_avg_2m", "ina_avg_at", "BatteryV_raw", "MeasuredAmps_filtered"].includes(key)) {
                        newTextContent = (value / 100).toFixed(2);
                    }
                    else if (key === "dutyCycle") {
                        newTextContent = (value / 100).toFixed(2);
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
                ["ADS3ID", "Channel3V"],
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
                ["imu_hf_vibration_energy_ID", "imu_hf_vibration_energy"],
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

                    const weatherTab = document.querySelector('.sub-tab[onclick*="weather"]');
                    if (weatherTab) {
                        weatherTab.style.display = 'none';
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
        source.addEventListener('CSVData', handleCSVData, false);

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

            // voltageTarget and Icv moved to CSV1 (fast stream) — cvPlotCache is updated in handleCSVData

            if (data.stateRevision !== undefined) {
                lastSeenRev = data.stateRevision;
            }

            handleForcedUpdate(data);

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
                    else if (["anomalyMarginAmps"].includes(key)) {
                        newTextContent = (value / 10).toFixed(1);
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
                        "BrushLifePercent", "pKwHrToday", "pKwHrTomorrow", "pKwHr2days", "AvgSpeed", "MeasuredAmpsMax_AllTime", "SOGNMEA", "ApparentWindSpeedNMEA", "TrueWindSpeedNMEA", "VMGNMEA",
                        "fastOvCurrentCap", "ch1_avg_10s", "ch1_avg_2m", "ch1_avg_at", "ina_avg_10s", "ina_avg_2m", "ina_avg_at"].includes(key)) {
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
                    else if (["innerTermP", "innerTermI", "innerTermD", "outerTermP", "outerTermI", "outerTermD"].includes(key)) {
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
                ["MAX_BUFFERED_RECORDSID", "MAX_BUFFERED_RECORDS"],
                ["COGNMEA_ID", "COGNMEA"],
                ["SOGNMEA_ID", "SOGNMEA"],
                ["ApparentWindSpeedNMEA_ID", "ApparentWindSpeedNMEA"],
                ["ApparentWindAngleNMEA_ID", "ApparentWindAngleNMEA"],
                ["TrueWindSpeedNMEA_ID", "TrueWindSpeedNMEA"],
                ["TrueWindAngleNMEA_ID", "TrueWindAngleNMEA"],
                ["LeewayNMEA_ID", "LeewayNMEA"],
                ["VMGNMEA_ID", "VMGNMEA"],
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
                ["imu_i2c_error_count_ID", "imu_i2c_error_count"],
                ["imu_unknown_tag_count_ID", "imu_unknown_tag_count"],
                ["imu_accel_dropped_ID", "imu_accel_dropped"],
                ["imu_gyro_dropped_ID", "imu_gyro_dropped"],
                ["imu_total_samples_accel_ID", "imu_total_samples_accel"],
                ["imu_total_samples_gyro_ID", "imu_total_samples_gyro"],
                ["IMUReadTime2_ID", "IMUReadTime2"],
                ["IMUReadTime_ID", "IMUReadTime"],
                ["adsI2CErrorCount_ID", "adsI2CErrorCount"],

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
                ["ft_uploadBufferedRecords_win_ID", "ft_uploadBufferedRecords_win"],
                ["ft_uploadBufferedRecords_ses_ID", "ft_uploadBufferedRecords_ses"],
                ["ft_buildConfigPayload_win_ID", "ft_buildConfigPayload_win"],
                ["ft_buildConfigPayload_ses_ID", "ft_buildConfigPayload_ses"],
                ["ft_saveNVSData_win_ID", "ft_saveNVSData_win"],
                ["ft_saveNVSData_ses_ID", "ft_saveNVSData_ses"],
                ["nvsCycleMs_ID", "nvsCycleMs"],             // last full NVS drain cycle (ms)
                ["ft_FlushFileWriteQueue_win_ID", "ft_FlushFileWriteQueue_win"],
                ["ft_FlushFileWriteQueue_ses_ID", "ft_FlushFileWriteQueue_ses"],
                ["ft_efficiencyTracker_win_ID", "ft_efficiencyTracker_win"],
                ["ft_efficiencyTracker_ses_ID", "ft_efficiencyTracker_ses"],
                ["VeTime2_ID", "VeTime2"],
                ["systemIDActive_ID", "systemIDActive"],
                ["systemIDResultsReady_ID", "systemIDResultsReady"],
                ["systemIDStepAmp_0_ID", "systemIDStepAmp_0"],
                ["systemIDStepAmp_1_ID", "systemIDStepAmp_1"],
                ["systemIDStepAmp_2_ID", "systemIDStepAmp_2"],
                ["systemIDQuietPP_0_ID", "systemIDQuietPP_0"],
                ["systemIDQuietPP_1_ID", "systemIDQuietPP_1"],
                ["systemIDQuietPP_2_ID", "systemIDQuietPP_2"],
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
                ["ft_UpdateDistanceThisInterval_win_ID", "ft_UpdateDistanceThisInterval_win"],
                ["ft_UpdateDistanceThisInterval_ses_ID", "ft_UpdateDistanceThisInterval_ses"],
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
                ["voltLoopWorstInterval_5s_ID", "voltLoopWorstInterval_5s"],
                ["voltLoopWorstInterval_ses_ID", "voltLoopWorstInterval_ses"],
                ["fsFlushDeferred_ID", "fsFlushDeferred"],

            ];

            // Update other fields every cycle
            updateFields(otherFields);

            // Temperature PID terms displayed as current contributions (sign-flipped:
            // positive = adding amps, negative = removing amps)
            for (const [id, key] of [
                ["outerTermP_display", "outerTermP"],
                ["outerTermI_display", "outerTermI"],
                ["outerTermD_display", "outerTermD"],
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

            // Update thermal live score spans in Alternator Live Data tab (always-on)
            // thermalLiveScore0-3 are ×10000 in CSV2; display with window labels
            {
                const thermalAltLabels = ['30m', '3h', '24h', '7d'];
                for (let i = 0; i < 4; i++) {
                    const raw = data['thermalLiveScore' + i];
                    if (raw === undefined) continue;
                    const v = raw / 10000;
                    const txt = v > 0 ? thermalAltLabels[i] + ': ' + v.toFixed(4) : thermalAltLabels[i] + ': —';
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
            // systemIDActive is in CSV2_FIELDS (index 400), not CSV3 — read from DOM,
            // which CSV2 otherFields already keeps current via ["systemIDActive_ID", "systemIDActive"].
            const sysidPhaseNum = parseInt(getField("systemIDActive_ID") ?? 0);
            _testActiveCSV3 = sysidPhaseNum > 0 ? 'sysid' : null;
            if (_testPanelCurrentTest === 'sysid') {
                updateTestPanelScore(undefined, undefined, SYSID_PHASE_NAMES[sysidPhaseNum] ?? ('Phase ' + sysidPhaseNum));
            }
            updateTestActivePanel();

            // Update learning table inputs
            // CRITICAL: Read mode from incoming data, not DOM (which may be stale)
            const learningModeActive = data.LearningMode === 1;

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
                imu: data.ts_IMU
            };
        }, false);



    }




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
    document.getElementById("anomalyAlarmEnable_checkbox").checked = (document.getElementById("anomalyAlarmEnable").value === "1");
    document.getElementById("TuningMode_checkbox").checked = (document.getElementById("TuningMode").value === "1");
    document.getElementById("socInfoAvailable_checkbox").checked = (document.getElementById("socInfoAvailable").value === "1");
    document.getElementById("accelEnabled_checkbox").checked = (document.getElementById("accelEnabled").value === "1");
    document.getElementById("CloudFeatures_checkbox").checked = (document.getElementById("CloudFeatures").value === "1");
    document.getElementById("AutoAltCurrentZero_checkbox").checked = (document.getElementById("AutoAltCurrentZero").value === "1");
    document.getElementById("HardwarePresent_checkbox").checked = (document.getElementById("hardwarePresent").value === "1");

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
                'ft_uploadBufferedRecords_ses_ID', 'ft_buildConfigPayload_ses_ID',
                'ft_saveNVSData_ses_ID', 'ft_FlushFileWriteQueue_ses_ID',
                'cpuLoadCore0Max_display', 'cpuLoadCore1Max_display',
                'MaximumLoopTimeID',
                'ft_loop_win_ID', 'ft_loop_ses_ID',
                'ft_SendWifiData_win_ID', 'ft_SendWifiData_ses_ID',
                'ch1_worst_10s_ID', 'ch1_over2x_10s_ID', 'ch1_avg_10s_ID',
                'ch1_worst_2m_ID', 'ch1_over2x_2m_ID', 'ch1_avg_2m_ID',
                'ch1_worst_at_ID', 'ch1_over2x_at_ID', 'ch1_avg_at_ID',
                'ina_last_ms_ID', 'ina_avg_10s_ID', 'ina_worst_10s_ID', 'ina_over2x_10s_ID',
                'ina_avg_2m_ID', 'ina_worst_2m_ID', 'ina_over2x_2m_ID',
                'ina_avg_at_ID', 'ina_worst_at_ID', 'ina_over2x_at_ID',
                'voltLoopWorstInterval_5s_ID', 'voltLoopWorstInterval_ses_ID'
            ];
            ids.forEach(id => {
                const el = document.getElementById(id);
                if (el) el.textContent = '0';
            });
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
            ids.forEach(id => { const el = document.getElementById(id); if (el) el.textContent = '0'; });
        })
        .catch(err => diagError('Reset accel session failed:', err));
}

function handleResetAccelLifetime() {
    if (!currentAdminPassword) { alert("Please unlock settings first"); return; }
    if (!confirm("Reset ALL lifetime accel stats? This clears max heel/pitch, slam records, capsize and pitchpole counts from NVS. Cannot be undone.")) return;
    const params = new URLSearchParams({ password: currentAdminPassword, ResetAccelLifetime: '1' });
    fetchWithTimeout(buildURL('/get?' + params.toString()), {}, 8000)
        .then(() => {
            const ids = [
                'imu_heel_max_lifetime_ID', 'imu_pitch_max_lifetime_ID',
                'imu_slam_peak_lifetime_ID', 'imu_slam_count_lifetime_ID',
                'imu_capsize_count_ID', 'imu_pitchpole_count_ID'
            ];
            ids.forEach(id => { const el = document.getElementById(id); if (el) el.textContent = '0'; });
        })
        .catch(err => diagError('Reset accel lifetime failed:', err));
}

function handleClearToken() {
    const confirmation = confirm(
        "🔴 CLEAR AUTH TOKEN 🔴\n\n" +
        "This will force the device to re-register.\n\n" +
        "What will happen:\n" +
        "• Device will show 'Not Registered' screen\n" +
        "• All uploads will stop until re-registered\n" +
        "• You'll need to enter vessel info again\n\n" +
        "Only do this if:\n" +
        "• You're troubleshooting registration issues\n" +
        "• Device is stuck in bad state\n" +
        "• You suspect token corruption\n\n" +
        "ARE YOU SURE?"
    );
    return confirmation;
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

    // Rebuild efficiency plot when Live Data tab becomes visible
    if (tabName === 'livedata') {
        setTrackedTimeout(() => {
            if (typeof initEffPlot === 'function') {
                initEffPlot();
            }
        }, 0);
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

    // Block access to Cloud Features subtabs if not registered
    if (parentTab === 'cloudfeatures' && subTabName !== 'myprofile' && !isDeviceRegistered) {
        const featureNames = {
            'mydashboard': 'Statistics',
            'myhistory': 'Long Term Plots',
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

    // Refresh matrix stats whenever the Alternator live data tab is opened
    if (parentTab === 'livedata' && subTabName === 'alternator') {
        fetchMatrixStats();
    }

    // Initialize profile tab when switching to My Profile
    if (parentTab === 'cloudfeatures' && subTabName === 'myprofile') {
        if (typeof initializeProfileTab === 'function') {
            initializeProfileTab();
        }
    }

    // Redirect to Vercel for My History
    if (parentTab === 'cloudfeatures' && subTabName === 'myhistory') {
        redirectToHistory();
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
        if (subTabName === 'myhistory') {
            header.classList.remove('permanent-header-sticky');
        } else {
            header.classList.add('permanent-header-sticky');
        }
    }
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
    // Don't show multiple recovery dialogs
    if (document.getElementById('recoveryDialog')) return;

    const recoveryDiv = document.createElement('div');
    recoveryDiv.id = 'recoveryDialog';
    recoveryDiv.innerHTML = `
<div style="position: fixed; top: 50%; left: 50%; transform: translate(-50%, -50%); 
            background: var(--card-light); padding: 20px; border-radius: var(--radius); 
            box-shadow: 0 4px 8px rgba(0,0,0,0.3); z-index: 10000; border: 2px solid var(--accent);">
  <h3 style="margin-top: 0; color: var(--text-dark);">Connection Lost</h3>
  <p style="color: var(--text-dark);">Lost connection to alternator regulator.</p>
  <button onclick="retryConnection()" style="background-color: var(--accent); color: white; border: none; padding: 8px 16px; border-radius: var(--radius); cursor: pointer; margin-right: 10px;">Retry Connection</button>
  <button onclick="enterOfflineMode()" style="background-color: #555555; color: white; border: none; padding: 8px 16px; border-radius: var(--radius); cursor: pointer;">Continue Offline</button>
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
    // Don't run multiple times
    if (document.getElementById('offlineBanner')) return;
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
                range: [yyMin, yyMax]
            },
            duty: {
                auto: false,
                range: [-25, 105]
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
                side: 3
            },
            {
                scale: "duty",
                label: "Duty Cycle (%)",
                grid: { show: true },
                side: 1
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
    let configChanged = false;

    // Check for Y-axis range changes
    if (data.yyMin !== undefined && data.yyMin !== yyMin) {
        yyMin = data.yyMin;
        configChanged = true;
    }
    if (data.yyMax !== undefined && data.yyMax !== yyMax) {
        yyMax = data.yyMax;
        configChanged = true;
    }

    // Check for time window changes
    // Check for time window changes
    if (data.xTime !== undefined && !isNaN(data.xTime) && data.xTime > 0 && data.xTime !== xTime) {
        xTime = data.xTime;
        configChanged = true;
    }

    if (configChanged) {
        // Destroy and recreate
        if (pidTuningPlot) {
            pidTuningPlot.destroy();
        }
        initPidTuningDataStructures();
        initPidTuningPlot();
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
        for (let year = 2200; year >= 1700; year--) {
            const option = document.createElement('option');
            option.value = year;
            option.textContent = year;
            if (year === 2025) {
                option.selected = true;
            }
            yearSelect.appendChild(option);
        }
    }
}

function clearVesselData() {
    if (!confirm('Are you sure? This will reset all vessel information to defaults.')) {
        return;
    }

    const form = this.form || this.closest('form');
    const password = form.querySelector('.password_field').value;

    fetch('/clearVesselInfo', {
        method: 'POST',
        headers: {
            'Content-Type': 'application/x-www-form-urlencoded',
        },
        body: `password=${encodeURIComponent(password)}`
    })
        .then(response => response.json())
        .then(data => {
            const messageDiv = document.getElementById('vessel-info-message');
            if (data.success) {
                messageDiv.textContent = 'Vessel data cleared successfully. Refreshing...';
                messageDiv.style.display = 'block';
                messageDiv.style.backgroundColor = '#d4edda';
                messageDiv.style.color = '#155724';
                setTimeout(() => location.reload(), 1000);
            } else {
                messageDiv.textContent = 'Error: ' + (data.error || 'Unknown error');
                messageDiv.style.display = 'block';
                messageDiv.style.backgroundColor = '#f8d7da';
                messageDiv.style.color = '#721c24';
            }
        })
        .catch(err => {
            console.error('Clear vessel data error:', err);
            const messageDiv = document.getElementById('vessel-info-message');
            messageDiv.textContent = 'Network error clearing vessel data';
            messageDiv.style.display = 'block';
            messageDiv.style.backgroundColor = '#f8d7da';
            messageDiv.style.color = '#721c24';
        });
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

// Returns a human-readable reason string if the fetch should be blocked,
// or null if it is safe to proceed.
// Whitelist: only Off/None (0), Idle, Bulk (not near target), and Manual are safe.
function matrixStatsFetchBlockReason() {
    const stage = gLastChargeStage;

    // Off/None and Idle are always safe
    if (stage === 0 || stage === CS_IDLE) return null;

    // Manual: user accepts responsibility in this mode
    if (stage === CS_MANUAL) return null;

    // Bulk is safe unless battery is close to the bulk target
    if (stage === CS_BULK) {
        const battV = getLiveBatteryV();
        const bulkV = getEchoNumber('BulkVoltage_echo');
        const proxV = getEchoNumber('ProtectionProxGateV_echo') || 0.5;
        if (!isNaN(bulkV) && battV >= bulkV - proxV) {
            return `Battery voltage (${battV.toFixed(2)} V) is within ${proxV.toFixed(2)} V of `
                 + `the bulk voltage target (${bulkV.toFixed(2)} V). Refresh blocked when near bulk.`;
        }
        return null; // Bulk but not near target — safe
    }

    // Everything else: Absorption, Float, Maintain, Target V
    const names = {
        [CS_ABSORPTION]: 'Absorption', [CS_FLOAT]: 'Float',
        [CS_MAINTAIN]: 'Maintain',     [CS_TARGET_V]: 'Target Voltage'
    };
    return `System is in ${names[stage] || 'stage ' + stage} mode. `
         + 'Matrix stats refresh is only allowed when Off, Idle, Manual, or in early Bulk charge.';
}

function renderMatrixBars(containerId, labels, values, color) {
    const container = document.getElementById(containerId);
    if (!container) return;
    const maxVal = Math.max(...values, 1);
    container.innerHTML = labels.map((lbl, i) => {
        const pct    = Math.round((values[i] / maxVal) * 100);
        const hrs    = (values[i] / 3600).toFixed(1);
        const dimmed = values[i] === 0 ? 'opacity:0.35;' : '';
        return `<div style="display:flex;align-items:center;gap:6px;margin-bottom:5px;${dimmed}">` +
               `<div style="width:82px;font-size:10px;color:var(--text-muted);text-align:right;flex-shrink:0;white-space:nowrap;">${lbl}</div>` +
               `<div style="flex:1;background:rgba(128,128,128,0.15);border-radius:3px;height:13px;overflow:hidden;">` +
               `<div style="width:${pct}%;background:${color};height:100%;border-radius:3px;transition:width 0.4s ease;"></div></div>` +
               `<div style="width:34px;font-size:10px;color:var(--text-muted);text-align:right;flex-shrink:0;">${hrs}h</div>` +
               `</div>`;
    }).join('');
}

// explicit=true  → called by the user clicking Refresh; shows alert on block.
// explicit=false → called automatically (tab switch, page load); silently skips.
function fetchMatrixStats(explicit = false) {
    const blockReason = matrixStatsFetchBlockReason();
    if (blockReason) {
        if (explicit) {
            alert('Matrix stats refresh blocked:\n\n' + blockReason
                + '\n\nTry again when the system is in Float, Idle, or off.');
        } else {
            const el = document.getElementById('matrix-stats-age');
            if (el) el.textContent = 'Skipped — charging active';
        }
        return;
    }

    fetch('/effmatrixstats')
        .then(r => r.json())
        .then(d => {
            if (d.error) return;
            const ss     = d.total_ss;
            const ssStr  = `${Math.floor(ss / 3600)}h ${Math.floor((ss % 3600) / 60)}m`;
            const popPct = d.total_cells > 0
                ? ((d.pop_cells / d.total_cells) * 100).toFixed(1) : '0.0';

            const el = id => document.getElementById(id);
            if (el('matrix-stat-cells')) el('matrix-stat-cells').textContent = d.total_cells;
            if (el('matrix-stat-pop'))   el('matrix-stat-pop').textContent   = `${d.pop_cells} (${popPct}%)`;
            if (el('matrix-stat-ref'))   el('matrix-stat-ref').textContent   = d.ref_bins;
            if (el('matrix-stat-ss'))    el('matrix-stat-ss').textContent    = ssStr;

            renderMatrixBars('matrix-bar-rpm',   d.rpm_labels,   d.rpm_ss,   '#4a90d9');
            renderMatrixBars('matrix-bar-temp',  d.temp_labels,  d.temp_ss,  '#e07b39');
            const fieldCurrentLabels = d.field_min_amps
                ? d.field_min_amps.map((minA, i) => {
                    const maxA = d.field_max_amps[i];
                    return (maxA < 0) ? '—' : `${Math.round(minA)}–${Math.round(maxA)}A`;
                  })
                : d.field_labels;
            renderMatrixBars('matrix-bar-field', fieldCurrentLabels, d.field_ss, '#5aab61');

            const now = new Date();
            const ts  = now.getHours().toString().padStart(2, '0') + ':' +
                        now.getMinutes().toString().padStart(2, '0');
            if (el('matrix-stats-age')) el('matrix-stats-age').textContent = `Updated ${ts}`;
        })
        .catch(() => {
            const el = document.getElementById('matrix-stats-age');
            if (el) el.textContent = 'Load failed';
        });
}

function downloadEffMatrix() {
    const ts = getLogTimestamp();
    const a = document.createElement('a');
    a.href = '/effmatrix.csv';
    a.download = `AltHealthMatrix_${ts}.csv`;
    document.body.appendChild(a);
    a.click();
    document.body.removeChild(a);
}

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
        fetch('/resetlogs', { method: 'POST' })
            .catch(err => console.warn('Log reset failed:', err));
    }, 5000);
}


function resetLogs() {
    fetch('/resetlogs', { method: 'POST' })
        .catch(() => { });
}



// ==================== EFFICIENCY MATRIX PLOT ====================
//
//   EffMatrix SSE → bin reference data (avg, min, max, state)
//   EffRed    SSE → live operating point
//
//   Plot shows:
//     Gray band   = reference min/max for active field bucket
//     Gray line   = reference average
//     Red dot     = live operating point
//   Watermark shows active bin labels + SS seconds + confidence state.
//   Anomaly banner shown when live point deviates from reference.
// ================================================================

// Field bucket boundaries — must match firmware FIELD_BOUNDS[]
// If you change NUM_FIELD_BUCKETS or EFF_FIELD_MAX in firmware,
// update this array to match and wipe the matrix.
const EFF_FIELD_BOUNDS = [0, 2.14, 4.29, 6.43, 8.57, 10.71, 12.86, 15.0];
const EFF_FIELD_MIN = 0;
const EFF_FIELD_MAX = 15;
const EFF_AMP_MIN = 0;
const EFF_AMP_MAX = 150;

let effPlot = null;
let effPlotData = null;
let effPlotResizeObserver = null;
let effUpdateScheduled = false;


// Current matrix state for active bin — populated by EffMatrix SSE
let effMatrixState = {
    state: 0,   // 0 = weak/empty, 1 = populated non-ref, 2 = reference bin
    rBucket: -1,
    tBucket: -1,
    fBucket: -1,
    rLabel: '--',
    tLabel: '--',
    fLabel: '--',
    ss_seconds: 0,
    avg_amps: 0,
    min_amps: 0,
    max_amps: 0,
    is_reference_bin: 0,
    sessionErrorCount: 0
};

// Live red dot state — populated by EffRed SSE
let effRedDot = {
    valid: false,
    fieldVolts: 0,
    amps: 0,
    rpmBucket: -1,
    tempBucket: -1,
    fieldBucket: -1
};

// Session history — populated by EffHistory SSE
let effHistory = {
    count: 0,
    head: 0,
    values: new Array(30).fill(0)
};



// ── Plot Data Builder ──────────────────────────────────────────
// Only the red dot is a uPlot series.
// The reference band is drawn via canvas hook in drawEffReferenceBand().

function buildEffPlotData() {
    if (effRedDot.valid) {
        effPlotData = [
            [effRedDot.fieldVolts],
            [effRedDot.amps]
        ];
    } else {
        effPlotData = [[0], [null]];
    }
}

// ── Reference Band Canvas Draw ─────────────────────────────────
// Called from drawClear hook — runs before uPlot draws series.
// Draws filled gray band (min/max) and avg line for active field bucket.
// Full opacity when state=2 (reference), faded when state=1 (low confidence).
// Nothing drawn when state=0 (no data).

function drawEffReferenceBand(u) {
    const state = effMatrixState.state;
    if (state === 0 || effMatrixState.fBucket < 0) return;
    if (effMatrixState.max_amps <= 0) return;   // Not yet populated

    const f = effMatrixState.fBucket;
    const ctx = u.ctx;
    const isRef = (state === 2);

    // X: field bucket bounds → canvas pixels
    const xLeft = u.valToPos(EFF_FIELD_BOUNDS[f], 'x', true);
    const xRight = u.valToPos(EFF_FIELD_BOUNDS[f + 1], 'x', true);

    // Y: reference amps → canvas pixels (higher amps = lower pixel y)
    const yBandTop = u.valToPos(effMatrixState.max_amps, 'y', true);
    const yBandBot = u.valToPos(effMatrixState.min_amps, 'y', true);
    const yAvg = u.valToPos(effMatrixState.avg_amps, 'y', true);

    ctx.save();

    // Filled min/max band
    ctx.globalAlpha = isRef ? 0.22 : 0.08;
    ctx.fillStyle = '#888888';
    ctx.fillRect(xLeft, yBandTop, xRight - xLeft, yBandBot - yBandTop);

    // Average line
    ctx.globalAlpha = isRef ? 0.55 : 0.22;
    ctx.strokeStyle = isRef ? '#555555' : '#888888';
    ctx.lineWidth = 2;
    ctx.setLineDash(isRef ? [] : [5, 4]);
    ctx.beginPath();
    ctx.moveTo(xLeft, yAvg);
    ctx.lineTo(xRight, yAvg);
    ctx.stroke();
    ctx.setLineDash([]);

    ctx.restore();
}

// ── Watermark ─────────────────────────────────────────────────
// Top-right overlay showing active bin identity, SS seconds, and confidence state.

function drawEffWatermark(u) {
    const state = effMatrixState.state;
    const stateLabel = state === 2 ? 'Reference' :
        state === 1 ? 'Low Confidence' : 'No Reference';
    const ss_min = effMatrixState.ss_seconds > 0
        ? Math.round(effMatrixState.ss_seconds / 60) + ' min SS'
        : '0 min SS';

    const lines = [
        `RPM:   ${effMatrixState.rLabel}`,
        `Temp:  ${effMatrixState.tLabel}`,
        `Field: ${effMatrixState.fLabel}`,
        ss_min,
        stateLabel
    ];

    const ctx = u.ctx;
    const x1 = u.bbox.left + u.bbox.width;
    const y0 = u.bbox.top;

    ctx.save();
    ctx.globalAlpha = 0.28;
    ctx.fillStyle = '#666';
    ctx.font = 'bold 14px monospace';
    ctx.textBaseline = 'top';
    ctx.textAlign = 'right';
    ctx.shadowColor = 'rgba(255,255,255,0.6)';
    ctx.shadowBlur = 2;

    let y = y0 + 10;
    for (const line of lines) {
        ctx.fillText(line, x1 - 14, y);
        y += 20;
    }
    ctx.restore();
}

// ── Plot Init ─────────────────────────────────────────────────

function initEffPlot() {
    const plotEl = document.getElementById('eff-scatter-plot');
    if (!plotEl) return;

    buildEffPlotData();

    const opts = {
        width: Math.min(plotEl.clientWidth || 800, 800),
        height: 400,
        series: [
            { label: null },
            {
                label: 'Live Operating Point',
                stroke: '#FF4444',
                width: 0,
                points: { show: true, size: 18, fill: '#FF4444', stroke: '#FFFFFF' }
            }
        ],
        scales: {
            x: { time: false, auto: false, range: [EFF_FIELD_MIN, EFF_FIELD_MAX] },
            y: { auto: false, range: [EFF_AMP_MIN, EFF_AMP_MAX] }
        },
        axes: [
            { label: 'Field Volts (V)', grid: { show: true } },
            { label: 'Output Current (A)', grid: { show: true }, side: 3 }
        ],
        legend: { show: false },
        plugins: [{
            hooks: {
                init: [(u) => {
                    createEffLegend();
                    const resizePlot = debounce(() => {
                        const el = document.getElementById('eff-scatter-plot');
                        if (el && effPlot) effPlot.setSize({ width: el.clientWidth, height: 400 });
                    }, 1000);
                    if (effPlotResizeObserver) effPlotResizeObserver.disconnect();
                    effPlotResizeObserver = new ResizeObserver(resizePlot);
                    effPlotResizeObserver.observe(plotEl);
                }],
                // drawClear fires after canvas clear, before series draw.
                // Band is drawn here so red dot renders on top of it.
                drawClear: [(u) => {
                    if (!u.root || u.root.offsetParent === null) return;
                    drawEffReferenceBand(u);
                    drawEffWatermark(u);
                }]
            }
        }]
    };

    if (effPlot) effPlot.destroy();
    effPlot = new uPlot(opts, effPlotData, plotEl);
    if (document.body.classList.contains('dark-mode')) updateUplotTheme(effPlot);
}

// ── Legend ────────────────────────────────────────────────────

function createEffLegend() {
    const plotContainer = document.getElementById('eff-scatter-plot');
    if (!plotContainer) return;
    const existing = plotContainer.querySelector('.custom-legend');
    if (existing) existing.remove();

    const legendDiv = document.createElement('div');
    legendDiv.className = 'custom-legend';
    legendDiv.style.cssText = `display:flex;justify-content:center;gap:20px;margin-top:10px;flex-wrap:wrap;`;

    const items = [
        { label: 'Reference Range (min/max)', color: '#888888', shape: 'rect' },
        { label: 'Reference Average', color: '#555555', shape: 'line' },
        { label: 'Live Operating Point', color: '#FF4444', shape: 'dot' }
    ];

    items.forEach(item => {
        const wrap = document.createElement('label');
        wrap.style.cssText = `display:flex;align-items:center;gap:6px;font-size:12px;user-select:none;`;

        let icon = document.createElement('div');
        if (item.shape === 'dot') {
            icon.style.cssText = `width:12px;height:12px;border-radius:50%;background:${item.color};`;
        } else if (item.shape === 'line') {
            icon.style.cssText = `width:22px;height:3px;background:${item.color};border-radius:2px;margin-top:1px;`;
        } else {
            // rect = band swatch
            icon.style.cssText = `width:22px;height:10px;background:${item.color};opacity:0.35;border-radius:2px;`;
        }

        const span = document.createElement('span');
        span.textContent = item.label;
        span.style.cssText = `color:var(--text-dark);`;
        wrap.appendChild(icon);
        wrap.appendChild(span);
        legendDiv.appendChild(wrap);
    });

    plotContainer.appendChild(legendDiv);
}

// ── Update Scheduler ──────────────────────────────────────────

function queueEffPlotUpdate() {
    if (effUpdateScheduled) return;
    effUpdateScheduled = true;

    requestAnimationFrame(() => {
        effUpdateScheduled = false;
        if (!effPlot) return;
        buildEffPlotData();
        // setData triggers full draw cycle, which fires drawClear hook,
        // which redraws the reference band with current effMatrixState.
        effPlot.setData(effPlotData);
    });
}

// ── Anomaly Warning Display ───────────────────────────────────
// Updates:
//   #eff-anomaly-banner  — warning block on efficiency tab
//   #eff-anomaly-message — detail text inside banner
//   #eff-health-pct      — large health % number in settings card

function updateEffAnomalyDisplay() {
    const banner = document.getElementById('eff-anomaly-banner');
    const msgEl = document.getElementById('eff-anomaly-message');

    const state = effMatrixState.state;
    const errCount = effMatrixState.sessionErrorCount;

    // ── Health pct element ──
    const healthEl = document.getElementById('eff-health-pct');
    if (healthEl) {
        const pct = getEffHealthPct();
        const noData = effHistory.count === 0 && pct === null;
        if (noData) {
            // No history and no live reading — show demo value so the
            // number isn't empty under the X overlay
            healthEl.textContent = '92%';
            healthEl.style.color = '#4CAF50';
        } else if (pct === null) {
            healthEl.textContent = '--';
            healthEl.style.color = '#888';
        } else {
            healthEl.textContent = pct.toFixed(1) + '%';
            healthEl.style.color = pct >= 95 ? '#4CAF50'
                : pct >= 85 ? '#FFC107'
                    : '#F44336';
        }
    }
    renderEffSparkline();

    if (!banner || !msgEl) return;

    // Only show banner when: reference bin is active, errors exist,
    // and live point is actually outside the reference band right now.
    const liveOutside = effRedDot.valid &&
        state === 2 &&
        (effRedDot.amps < effMatrixState.min_amps ||
            effRedDot.amps > effMatrixState.max_amps);

    if (!liveOutside && errCount === 0) {
        banner.style.display = 'none';
        return;
    }

    banner.style.display = 'block';
    banner.className = errCount >= 5
        ? 'eff-anomaly-banner banner-red'
        : 'eff-anomaly-banner banner-yellow';

    if (!liveOutside && errCount > 0) {
        // Errors accumulated earlier but live point currently normal
        msgEl.innerHTML =
            `<strong>Alternator anomalies recorded this session</strong><br>` +
            `${errCount} out-of-range steady-state point${errCount > 1 ? 's' : ''} recorded. ` +
            `Current operating point is within reference. Monitor for recurrence.`;
        return;
    }

    // Live point is currently outside reference — give full detail
    const avg = effMatrixState.avg_amps.toFixed(1);
    const minA = effMatrixState.min_amps.toFixed(1);
    const maxA = effMatrixState.max_amps.toFixed(1);
    const actual = effRedDot.amps.toFixed(1);
    const ss_min = Math.round(effMatrixState.ss_seconds / 60);
    const isLow = effRedDot.amps < effMatrixState.min_amps;
    const delta = isLow
        ? (effMatrixState.min_amps - effRedDot.amps).toFixed(1)
        : (effRedDot.amps - effMatrixState.max_amps).toFixed(1);

    msgEl.innerHTML =
        `<strong>⚠ Alternator output ${isLow ? 'below' : 'above'} reference</strong><br>` +
        `Conditions: <strong>${effMatrixState.rLabel} RPM</strong> · ` +
        `<strong>${effMatrixState.tLabel}</strong> · ` +
        `<strong>${effMatrixState.fLabel}</strong> field drive<br>` +
        `Reference: <strong>${avg}A avg</strong> &nbsp;` +
        `(${minA}–${maxA}A range · ${ss_min} min of reference data)<br>` +
        `Current output: <strong>${actual}A</strong> — ` +
        `${delta}A ${isLow ? 'below minimum' : 'above maximum'}<br>` +
        `Session anomaly count: <strong>${errCount}</strong>`;

    const healthPct = getEffHealthPct();
    if (healthPct !== null) {
        msgEl.innerHTML +=
            `<br>Live health: <strong>${healthPct.toFixed(1)}%</strong> of reference average`;
    }
}

// ── Reset Button ──────────────────────────────────────────────

document.getElementById('eff-reset-btn')?.addEventListener('click', () => {
    if (!currentAdminPassword) {
        alert("Please unlock settings first");
        return;
    }
    if (!confirm(
        'Clear ALL learned efficiency matrix data and reference bins?\n\n' +
        'This cannot be undone. The system will need to re-learn from scratch.'
    )) return;

    const formData = new FormData();
    formData.append("password", currentAdminPassword);
    formData.append("ResetEfficiencyMatrix", "1");

    fetchWithTimeout(buildURL("/get?" + new URLSearchParams(formData).toString()), {}, 5000)
        .then(() => {
            effMatrixState = {
                state: 0, rBucket: -1, tBucket: -1, fBucket: -1,
                rLabel: '--', tLabel: '--', fLabel: '--',
                ss_seconds: 0, avg_amps: 0, min_amps: 0, max_amps: 0,
                is_reference_bin: 0, sessionErrorCount: 0
            };
            effHistory = { count: 0, head: 0, values: new Array(30).fill(0) };
            effRedDot = { valid: false, fieldVolts: 0, amps: 0, rpmBucket: -1, tempBucket: -1, fieldBucket: -1 };
            queueEffPlotUpdate();
            updateEffAnomalyDisplay();
        })
        .catch(err => diagError('Efficiency reset failed:', err));
});

// ── Voltage Mode Greyout (unchanged) ─────────────────────────
// stage: 1=bulk, 2=absorption, 3=float, 4=manual, 5=maintain, 6=target voltage, 7=idle, 0/other=off

function updateVoltageModeGreyout(stage) {
    const isCVMode = (stage === 2 || stage === 3 || stage === 6);
    document.querySelectorAll('[data-mode="cv"]').forEach(el => {
        el.classList.toggle('mode-dimmed', !isCVMode);
    });
}

// ── Live Health Percentage ────────────────────────────────────
// Computed from live SSE data — no firmware changes needed.
// Only meaningful when in a finalized reference bin with valid red dot.

function getEffHealthPct() {
    if (!effRedDot.valid) return null;
    if (effMatrixState.state !== 2) return null;
    if (effMatrixState.avg_amps <= 0) return null;
    return (effRedDot.amps / effMatrixState.avg_amps) * 100;
}

// ── Sparkline Renderer ────────────────────────────────────────
// Draws session history as inline SVG in #eff-sparkline-container.
// Each point = one power session's average health ratio × 100.
// Green ≥ 95%, Yellow 85–95%, Red < 85%.
// Reference line at 100%. Oldest left, newest right.

function renderEffSparkline() {
    const container = document.getElementById('eff-sparkline-container');
    if (!container) return;

    const W = container.clientWidth || 300;
    const H = 84;
    const PAD_L = 30, PAD_R = 8, PAD_T = 8, PAD_B = 14;
    const plotW = W - PAD_L - PAD_R;
    const plotH = H - PAD_T - PAD_B;
    const Y_MIN = 80, Y_MAX = 112;

    // Demo data shown when no real sessions exist yet
    const DEMO_PTS = [
        103, 105, 102, 104, 101, 100, 102, 99, 98, 100,
        97, 99, 96, 95, 97, 94, 93, 92, 94, 91,
        93, 90, 92, 89, 88, 90, 87, 86, 88, 92
    ];

    // Reconstruct chronological order from circular buffer
    const ordered = [];
    if (effHistory.count > 0) {
        const oldest = effHistory.count < 30 ? 0 : effHistory.head;
        for (let i = 0; i < effHistory.count; i++) {
            const idx = (oldest + i) % 30;
            const v = effHistory.values[idx];
            if (v > 0.1) ordered.push(v * 100);
        }
    }

    const livePct = getEffHealthPct();
    const showLive = (effMatrixState.state === 2 && effRedDot.valid && livePct !== null);
    const realPoints = showLive ? [...ordered, livePct] : ordered;
    const noData = realPoints.length === 0;
    const pts = noData ? DEMO_PTS : realPoints;
    const nPts = pts.length;

    function toPixelX(i, total) {
        return PAD_L + (i / Math.max(total - 1, 1)) * plotW;
    }
    function toPixelY(v) {
        const c = Math.max(Y_MIN, Math.min(Y_MAX, v));
        return PAD_T + plotH - ((c - Y_MIN) / (Y_MAX - Y_MIN)) * plotH;
    }
    function colorFor(v) {
        if (v >= 95) return '#4CAF50';
        if (v >= 85) return '#FFC107';
        return '#F44336';
    }

    // Catmull-Rom bezier path
    function smoothPath(points, close) {
        const tension = 0.35;
        const px = points.map((v, i) => [toPixelX(i, points.length), toPixelY(v)]);
        let d = `M${px[0][0].toFixed(1)},${px[0][1].toFixed(1)}`;
        for (let i = 0; i < px.length - 1; i++) {
            const p0 = px[Math.max(0, i - 1)];
            const p1 = px[i];
            const p2 = px[i + 1];
            const p3 = px[Math.min(px.length - 1, i + 2)];
            const cp1x = p1[0] + (p2[0] - p0[0]) * tension;
            const cp1y = p1[1] + (p2[1] - p0[1]) * tension;
            const cp2x = p2[0] - (p3[0] - p1[0]) * tension;
            const cp2y = p2[1] - (p3[1] - p1[1]) * tension;
            d += ` C${cp1x.toFixed(1)},${cp1y.toFixed(1)} ${cp2x.toFixed(1)},${cp2y.toFixed(1)} ${p2[0].toFixed(1)},${p2[1].toFixed(1)}`;
        }
        if (close) {
            const last = px[px.length - 1];
            const first = px[0];
            const btm = (PAD_T + plotH).toFixed(1);
            d += ` L${last[0].toFixed(1)},${btm} L${first[0].toFixed(1)},${btm} Z`;
        }
        return d;
    }

    const avg = pts.reduce((a, v) => a + v, 0) / pts.length;
    const lineColor = colorFor(avg);

    const refY = toPixelY(100).toFixed(1);
    const refLine = `<line x1="${PAD_L}" y1="${refY}" x2="${W - PAD_R}" y2="${refY}" stroke="#aaa" stroke-width="1" stroke-dasharray="4,3" opacity="0.5"/>`;

    const linePath = smoothPath(pts, false);
    const areaPath = smoothPath(pts, true);

    const yLabels = [85, 95, 100, 105].map(v => {
        const y = toPixelY(v).toFixed(1);
        const bold = v === 100;
        return `<text x="${PAD_L - 4}" y="${y}" text-anchor="end" dominant-baseline="middle" font-size="9" fill="${bold ? '#999' : '#ccc'}" font-weight="${bold ? '600' : '400'}">${v}</text>`;
    }).join('');

    // Pick up to 6 evenly-spaced x-axis tick indices, always including first and last
    const maxTicks = 6;
    const tickIndices = new Set([0, nPts - 1]);
    if (nPts > 2) {
        const step = Math.max(1, Math.round((nPts - 1) / (maxTicks - 1)));
        for (let i = step; i < nPts - 1; i += step) tickIndices.add(i);
    }
    const xLabels = [...tickIndices].sort((a, b) => a - b).map(i => {
        const x = toPixelX(i, nPts).toFixed(1);
        const lbl = i === nPts - 1 ? 'now' : `S${i + 1}`;
        return `<line x1="${x}" y1="${(PAD_T + plotH).toFixed(1)}" x2="${x}" y2="${(PAD_T + plotH + 3).toFixed(1)}" stroke="#ddd"/>` +
               `<text x="${x}" y="${H - 1}" text-anchor="middle" font-size="9" fill="#bbb">${lbl}</text>`;
    }).join('');

    const dots = pts.map((v, i) => {
        const x = toPixelX(i, nPts).toFixed(1);
        const y = toPixelY(v).toFixed(1);
        const col = colorFor(v);
        const isLiveDot = !noData && showLive && i === nPts - 1;
        if (isLiveDot) {
            return `<circle cx="${x}" cy="${y}" r="5" fill="${col}" opacity="0.2"/>` +
                   `<circle cx="${x}" cy="${y}" r="3.5" fill="${col}"/>`;
        }
        return `<circle cx="${x}" cy="${y}" r="2.5" fill="${col}" stroke="white" stroke-width="1"/>`;
    }).join('');

    const defs = `<defs><linearGradient id="effSparkGrad" x1="0" y1="0" x2="0" y2="1">` +
        `<stop offset="0%" stop-color="${lineColor}" stop-opacity="0.18"/>` +
        `<stop offset="100%" stop-color="${lineColor}" stop-opacity="0"/>` +
        `</linearGradient></defs>`;

    container.innerHTML =
        `<svg width="${W}" height="${H}" xmlns="http://www.w3.org/2000/svg">` +
        defs + refLine +
        `<path d="${areaPath}" fill="url(#effSparkGrad)"/>` +
        `<path d="${linePath}" fill="none" stroke="${lineColor}" stroke-width="2.5" stroke-linecap="round" stroke-linejoin="round"/>` +
        dots + yLabels + xLabels +
        `</svg>`;

    // Show/hide demo overlay and update subtitle
    const demoX    = document.getElementById('eff-demo-x');
    const demoNote = document.getElementById('eff-demo-note');
    const subtitle = document.getElementById('eff-history-subtitle');
    if (demoX)    demoX.style.display    = noData ? 'block' : 'none';
    if (demoNote) demoNote.style.display = noData ? 'flex'  : 'none';
    if (subtitle) subtitle.textContent   = noData
        ? '(no sessions yet · demo)'
        : `(${effHistory.count} session${effHistory.count !== 1 ? 's' : ''})`;
}

// ==================== THERMAL LOG PLOTS ====================
let _thermalStateArrays = {
    flagsArr: [], antiWindupArr: [], stageArr: [], tArr: []
};
let thermalLogAutoRefreshTimer = null;
let thermalLogPlots = [null, null, null];
let thermalLogResizeObservers = [null, null, null];
let thermalWindowMin = 30;
let thermalFetchInProgress = false;

// Default visibility — hide the four requested series
let thermalSeriesVisible = {
    tempFilt: true, tempProjected: true, tempSetpoint: true, dCorrection: true,
    penaltyAmps: true, measAmps: true, uTarget: true,
    outerP: true, outerI: true, outerD: true, impliedPenalty: true,
};

// ---------------------------------------------------------------------------
// Window buttons
// ---------------------------------------------------------------------------
function highlightThermalWindowBtn(minutes) {
    [5, 10, 30, 60, 120].forEach(v => {
        const btn = document.getElementById(`tw-${v}`);
        if (btn) btn.classList.toggle('btn-primary', v === minutes);
        if (btn) btn.classList.toggle('btn-secondary', v !== minutes);
    });
}

function setThermalWindow(minutes) {
    thermalWindowMin = minutes;
    highlightThermalWindowBtn(minutes);
    thermalLogPlots.forEach((p, i) => { if (p) { p.destroy(); thermalLogPlots[i] = null; } });
    fetchAndRenderThermalLog();
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

// ---------------------------------------------------------------------------
// Fetch
// ---------------------------------------------------------------------------
async function fetchAndRenderThermalLog() {
    if (thermalFetchInProgress) return;
    thermalFetchInProgress = true;
    const statusEl = document.getElementById('thermallog-status');
    if (statusEl) statusEl.textContent = 'Fetching…';
    try {
        const resp = await fetch('/thermallog.bin');
        if (!resp.ok) throw new Error(`HTTP ${resp.status}`);
        const buf = await resp.arrayBuffer();
        // Capture scroll position only when we're about to do real DOM work (chart rebuild).
        // Moving this inside the try block means failed fetches never clobber user scroll position.
        const scrollY = window.scrollY;
        parseThermalBin(buf);
        if (statusEl) statusEl.textContent = `Updated ${new Date().toLocaleTimeString()}`;
        requestAnimationFrame(() => {
            requestAnimationFrame(() => {
                if (Math.abs(window.scrollY - scrollY) > 2) window.scrollTo(0, scrollY);
            });
        });
    } catch (err) {
        if (statusEl) statusEl.textContent = `Fetch error: ${err.message}`;
        console.error('thermallog fetch:', err);
    } finally {
        thermalFetchInProgress = false;
    }
}

// ---------------------------------------------------------------------------
// Parse binary
// ---------------------------------------------------------------------------
function parseThermalBin(buf) {
    const view = new DataView(buf);
    if (buf.byteLength < 8) {
        const el = document.getElementById('thermallog-status');
        if (el) el.textContent = 'No log data yet — waiting for AUTO control tick…';
        return;
    }

    const count = view.getUint32(0, true);
    const intervalMs = view.getUint32(4, true);

    if (count === 0) {
        const el = document.getElementById('thermallog-status');
        if (el) el.textContent = 'Log empty — data will appear once alternator is running in AUTO mode.';
        return;
    }

    const ENTRY_SIZE = 48;
    const HEADER_SIZE = 8;

    if (buf.byteLength < HEADER_SIZE + count * ENTRY_SIZE) {
        console.error('thermallog.bin: truncated response', buf.byteLength, 'need', HEADER_SIZE + count * ENTRY_SIZE);
        return;
    }

    const intervalMin = intervalMs / 60000.0;

    const t = new Array(count);
    const tempFilt = new Array(count);
    const tempProjected = new Array(count);
    const tempSetpoint = new Array(count);
    const penalty = new Array(count);
    const outerP = new Array(count);
    const outerI = new Array(count);
    const outerD = new Array(count);
    const implied = new Array(count);
    const outerDExt = new Array(count);
    const measAmps = new Array(count);
    const uTarget = new Array(count);
    const spLimited = new Array(count);
    const voltCap = new Array(count);
    const flagsArr = new Array(count);
    const antiWindup = new Array(count);
    const stageArr = new Array(count);

    for (let i = 0; i < count; i++) {
        t[i] = -(count - 1 - i) * intervalMin;
        const b = HEADER_SIZE + i * ENTRY_SIZE;

        tempFilt[i]      = view.getInt16(b + 4, true) / 10.0;
        tempProjected[i] = view.getInt16(b + 6, true) / 10.0;
        tempSetpoint[i]  = view.getInt16(b + 8, true) / 10.0;
        voltCap[i] = view.getInt16(b + 12, true) / 10.0;
        uTarget[i] = view.getInt16(b + 14, true) / 10.0;
        spLimited[i] = view.getInt16(b + 16, true) / 10.0;
        measAmps[i] = view.getInt16(b + 28, true) / 10.0;
        penalty[i] = view.getInt16(b + 30, true) / 10.0;

        flagsArr[i] = view.getUint8(b + 32);
        antiWindup[i] = view.getUint8(b + 33);
        stageArr[i] = view.getUint8(b + 34);

        outerP[i] = view.getInt16(b + 36, true) / 10.0;
        outerI[i] = view.getInt16(b + 38, true) / 10.0;
        outerD[i] = view.getInt16(b + 40, true) / 10.0;
        implied[i] = view.getInt16(b + 42, true) / 10.0;
        outerDExt[i] = view.getInt16(b + 44, true) / 1000.0;  // thermalSlopeFPerSec (°F/s) — unused in plots, kept for future use
    }

    const dCorrection = tempProjected.map((tp, i) => tp - tempFilt[i]);

    renderThermalPlot1([t, tempFilt, tempProjected, tempSetpoint, dCorrection, penalty, measAmps, uTarget], t[0]);
    renderThermalPlotState([t, new Array(count).fill(null)], t[0], flagsArr, antiWindup, stageArr, t);
    renderThermalPlot2([t, outerP, outerI, outerD, implied], t[0]);
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
const THERMAL_MODE_COLORS = {
    shutdown: '#d9534f',
    bulk: '#00c853',
    absorption: '#7e57c2',
    float: '#ffb300',
    manual: '#ef5350',
    maintain: '#66bb6a',
    targetV: '#42a5f5',
    idle: '#666666',
    antiWindup: '#d9534f'
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
            {
                label: 'Temp Filtered (°F)', stroke: '#e74c3c', width: 2,
                scale: 'temp',
                show: thermalSeriesVisible.tempFilt !== false
            },
            {
                label: 'Temp Projected (°F)', stroke: '#e67e22', width: 1.5,
                scale: 'temp', dash: [4, 3],
                show: thermalSeriesVisible.tempProjected !== false
            },
            {
                label: 'Setpoint (°F)', stroke: '#f39c12', width: 1.5,
                scale: 'temp', dash: [8, 4],
                show: thermalSeriesVisible.tempSetpoint !== false
            },
            {
                label: 'D Correction (°F)', stroke: '#1abc9c', width: 1.5,
                scale: 'temp', dash: [3, 3],
                show: thermalSeriesVisible.dCorrection !== false
            },
            {
                label: 'Penalty Amps (A)', stroke: '#2ecc71', width: 1.5,
                scale: 'amps',
                show: thermalSeriesVisible.penaltyAmps !== false
            },
            {
                label: 'Measured Amps (A)', stroke: '#3498db', width: 1.5,
                scale: 'amps',
                show: thermalSeriesVisible.measAmps !== false
            },
            {
                label: 'U Target (A)', stroke: '#9b59b6', width: 2,
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
            { scale: 'temp', label: 'Temperature (°F)', side: 3, grid: { show: true } },
            { scale: 'amps', label: 'Amps (A)', side: 1, grid: { show: false } }
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
        { key: 'tempFilt',      label: 'Temp Filtered',    color: '#e74c3c', idx: 1 },
        { key: 'tempProjected', label: 'Temp Projected',   color: '#e67e22', idx: 2 },
        { key: 'tempSetpoint',  label: 'Setpoint',         color: '#f39c12', idx: 3 },
        { key: 'dCorrection',   label: 'D Correction (°F)',color: '#1abc9c', idx: 4 },
        { key: 'penaltyAmps',   label: 'Penalty Amps',     color: '#2ecc71', idx: 5 },
        { key: 'measAmps',      label: 'Measured Amps',    color: '#3498db', idx: 6 },
        { key: 'uTarget',       label: 'U Target',         color: '#9b59b6', idx: 7 }
    ]);
    requestAnimationFrame(() => {
        if (thermalLogPlots[0] && el.clientWidth > 0)
            thermalLogPlots[0].setSize({ width: el.clientWidth, height: H });
    });
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
            { label: 'Outer P', stroke: '#3498db', width: 1.5, scale: 'amps' },
            { label: 'Outer I', stroke: '#e67e22', width: 1.5, scale: 'amps' },
            { label: 'Outer D', stroke: '#9b59b6', width: 1.5, scale: 'amps' },
            { label: 'Implied Penalty', stroke: '#2ecc71', width: 2, scale: 'amps' }
        ],
        scales: {
            x: { time: false, auto: false, range: [-thermalWindowMin, 0] },
            amps: { auto: true }
        },
        axes: [
            { label: 'Minutes Ago', grid: { show: true } },
            { scale: 'amps', label: 'PID Terms (A)', side: 3, grid: { show: true } }
        ],
        legend: { show: false },
        plugins: [
            {
                hooks: {
                    init: [(u) => _thermalResizeObserver(2, elId, H)],
                    drawClear: [(u) => { if (u.root?.offsetParent) drawThermalWatermark(u); }]
                }
            },
            thermalZoomPlugin
        ]
    };
    thermalLogPlots[2] = new uPlot(opts, data, el);
    if (document.body.classList.contains('dark-mode')) updateUplotTheme(thermalLogPlots[2]);
    _createThermalLegend(el, 2, [
        { key: 'outerP',        label: 'Outer P',         color: '#3498db', idx: 1 },
        { key: 'outerI',        label: 'Outer I',         color: '#e67e22', idx: 2 },
        { key: 'outerD',        label: 'Outer D',         color: '#9b59b6', idx: 3 },
        { key: 'impliedPenalty',label: 'Implied Penalty', color: '#2ecc71', idx: 4 },
    ]);
    requestAnimationFrame(() => {
        if (thermalLogPlots[2] && el.clientWidth > 0)
            thermalLogPlots[2].setSize({ width: el.clientWidth, height: H });
    });
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
                    init: [(u) => _thermalResizeObserver(1, elId, H)],
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
                        ctx.strokeStyle = THERMAL_MODE_COLORS.antiWindup;
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
            ? `width:2px;height:14px;background:${item.color};border-radius:1px;`
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
    if (thermalLogPlots[1]) {
        thermalLogPlots[1].scales.amps.range = auto
            ? (u, min, max) => [min, max]
            : () => [get('p2-min'), get('p2-max')];
        thermalLogPlots[1].redraw();
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

        det.addEventListener('toggle', () => {
            if (det.open) {
                setTimeout(() => { fetchAndRenderThermalLog(); }, 50);
                thermalLogAutoRefreshTimer = setInterval(fetchAndRenderThermalLog, 15000);
            } else {
                clearInterval(thermalLogAutoRefreshTimer);
                thermalLogAutoRefreshTimer = null;
            }
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
//   offset  4  uint32  entrySize (= 50)
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
// ===========================================================================

const CV_LOG_HEADER_SIZE = 36;
const CV_LOG_ENTRY_SIZE = 50;

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
        slopeBleedAmps,
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
        ` TdPred=${fmtRaw(c.TdPred, 3)}s DvdtAlpha=${fmtDiv(c.DvdtAlpha, 1000, 3)}` +
        ` KHard=${fmtDiv(c.KHard, 10, 1)}A/V` +
        ` AwBleedRate=${fmtDiv(c.AwBleedRate, 10, 1)}A/s AwRecoverRate=${fmtDiv(c.AwRecoverRate, 10, 2)}A/s`
    );
    lines.push(
        `# SlopeBleed: SlopeBleedThresh=${d.sbThresh.toFixed(3)}V/s SlopeBleedK=${d.sbK.toFixed(1)}A/(V/s) SlopeBleedProxV=${d.sbProxV.toFixed(3)}V`
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
        'slopeBleedAmps_A',
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
            d.slopeBleedAmps[i].toFixed(4),
        ].join(','));
    }

    return lines.join('\n');
}


// ---------------------------------------------------------------------------
// downloadCvLog()
// Fetches /cvlog.bin, decodes, saves as timestamped CSV.
// ---------------------------------------------------------------------------
async function downloadCvLog() {
    const statusEl = document.getElementById('cvlog-status');
    if (statusEl) statusEl.textContent = 'Downloading…';

    let buf;
    try {
        const resp = await fetch('/cvlog.bin');
        if (!resp.ok) throw new Error(`HTTP ${resp.status}`);
        buf = await resp.arrayBuffer();
    } catch (err) {
        const msg = `CV log download failed: ${err}`;
        if (statusEl) statusEl.textContent = msg;
        console.error(msg);
        return;
    }

    if (!buf || buf.byteLength < CV_LOG_HEADER_SIZE + CV_LOG_ENTRY_SIZE) {
        const msg = 'CV log empty — run in AUTO mode with voltage control active first.';
        if (statusEl) statusEl.textContent = msg;
        return;
    }

    const d = parseCvBin(buf);
    if (!d) {
        if (statusEl) statusEl.textContent = 'CV log parse failed.';
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

    if (statusEl) statusEl.textContent =
        `Downloaded ${d.count} rows (Kp=${d.voltKp.toFixed(2)} Ki=${d.voltKi.toFixed(3)} interval=${d.voltInterval}ms)`;
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
    if (gLastChargeStage !== CS_BULK) {
        checkbox.checked = false;
        alert('Current Waveform Test requires BULK stage.\n\nThe inner current loop only has full authority when the system is bulk charging. In absorption / float / CV, the voltage loop caps current and the score is meaningless.\n\nWait for BULK, then enable the test.');
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
}

function openSystemIDModal() {
    if (!currentAdminPassword) {
        alert("Please unlock settings first.");
        return;
    }
    // Reset panel position to default top-right on each open
    const panel = document.getElementById('sysid-modal-panel');
    if (panel) { panel.style.left = ''; panel.style.top = '80px'; panel.style.right = '20px'; }
    sysidShowScreen('preflight');
    document.getElementById('sysid-modal-overlay').style.display = 'block';
    sysidInitDrag();
    sysidUpdatePreflight();
    sysidPreflightInterval = setInterval(sysidUpdatePreflight, 1000);
}

function closeSystemIDModal() {
    document.getElementById('sysid-modal-overlay').style.display = 'none';
    if (sysidPreflightInterval) { clearInterval(sysidPreflightInterval); sysidPreflightInterval = null; }
    if (sysidPollInterval)      { clearInterval(sysidPollInterval);      sysidPollInterval = null; }
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

    const allOK   = rpmOK && ampsOK && voltOK && modeOK;

    document.getElementById('sysid-check-rpm').textContent  = (rpmOK  ? '✅' : '❌') + ' Engine running (RPM: ' + rpm.toFixed(0) + ' / min ' + minRpm.toFixed(0) + ')';
    document.getElementById('sysid-check-amps').textContent = (ampsOK ? '✅' : '❌') + ' Alternator producing current (' + amps.toFixed(1) + 'A)';
    document.getElementById('sysid-check-volt').textContent = (voltOK ? '✅' : '❌') + ' Battery voltage OK (' + battV.toFixed(2) + 'V)';
    document.getElementById('sysid-check-mode').textContent = modeMsg;

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
            if (vesselInfoComplete) showMainTab('plots');
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

        if (ready === 1) {
            clearInterval(sysidPollInterval); sysidPollInterval = null;
            // Mark all phases complete
            for (let i = 1; i <= 9; i++) {
                const el = document.getElementById('sysid-p' + i);
                if (el) { el.textContent = '✅ ' + SYSID_PHASE_NAMES[i]; el.style.color = '#4caf50'; }
            }
            document.getElementById('sysid-phase-bar').style.width = '100%';
            setTimeout(showSystemIDResults, 400);
            return;
        }

        // Protection layer fired mid-test — firmware aborted it, results are invalid
        if (phase === 0 && ready !== 1 && sysidEverActive) {
            clearInterval(sysidPollInterval); sysidPollInterval = null;
            sysidShowAborted(
                '⚠ Test aborted — a protection layer (RPM drop, overcurrent, or overvoltage) fired mid-test. ' +
                'Check the serial console for details. Bring the engine to stable RPM and run again.'
            );
            return;
        }

        if (elapsed > maxWaitMs) {
            clearInterval(sysidPollInterval); sysidPollInterval = null;
            alert("SystemID timed out after " + (elapsed / 1000).toFixed(0) + "s. Check serial console for details.");
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

    document.getElementById('sysid-results-summary').innerHTML =
        'Recommended TC: <strong style="color:#4a9eff;">' + sysidSuggestedTC.toFixed(0) + ' ms</strong>' +
        ' (highest single trial)';

    const applyBtn = document.getElementById('sysid-apply-btn');
    if (applyBtn) { applyBtn.style.display = ''; applyBtn.textContent = 'Set All Filters = ' + sysidSuggestedTC.toFixed(0) + ' ms'; }

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
    const tc = encodeURIComponent(sysidSuggestedTC);
    const pw = encodeURIComponent(currentAdminPassword);
    fetch(buildURL("/get?InputFilterTC=" + tc + "&password=" + pw))
        .then(() => fetch(buildURL("/get?OutputPIDFilterTC=" + tc + "&password=" + pw)))
        .then(() => fetch(buildURL("/get?VoltageFilterTC=" + tc + "&password=" + pw)))
        .then(() => {
            console.log("All filter TCs updated to " + sysidSuggestedTC + " ms");
            closeSystemIDModal();
        })
        .catch(err => console.error("Filter TC update failed:", err));
}


// Fetch matrix stats once on page load (also refreshes whenever Alternator tab is opened)
document.addEventListener('DOMContentLoaded', () => fetchMatrixStats());

// Auto-login via URL parameter for local automation (e.g. SwiftBar shortcut)
window.addEventListener('load', function () {
  const autopass = new URLSearchParams(window.location.search).get('autopass');
  if (autopass) {
    const el = document.getElementById('admin_password');
    if (el) { el.value = autopass; setAdminPassword(); }
  }
});

/* XREG_END */