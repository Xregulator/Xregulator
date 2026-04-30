/* XREG_START */


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
// that staleness is governed by TempPIDStaleMs (15s, outer PID)
// and the hardcoded 30s in buildTickSnapshot() on the ESP32.
//
// The timestamp payload sends every 3s, so any threshold below
// ~6s will cause false stale flashes even with healthy sensors.
// ============================================================
const STALE_THRESHOLD_DEFAULT_MS = 6000;   // All sensors except temperature
const STALE_THRESHOLD_TEMP_MS = 12000;  // Temp sensors read every 5s — allows one failed read


// PID Tuning Plot 
let yyMin = -5;    // Default until CSVData3 updates it
let yyMax = 105;
let xTime = 60;
let cachedYyMin = null;
let cachedYyMax = null;
let cachedXTime = null;


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
    "WifiStrength",               // 10
    "WifiHeartBeat",              // 11
    "SendWifiTime",               // 12
    "AnalogReadTime",             // 13
    "VeTime",                     // 14
    "MaximumLoopTime",            // 15
    "HeadingNMEA",                // 16
    "vvout",                      // 17
    "iiout",                      // 18
    "FreeHeap",                   // 19
    "EngineCycles",               // 20
    "Alarm_Status",               // 21
    "fieldActiveStatus",          // 22
    "CurrentSessionDuration",     // 23
    "timeAxisModeChanging",       // 24
    "webgaugesinterval",          // 25
    "plotTimeWindow",             // 26
    "Ymin1",                      // 27
    "Ymax1",                      // 28
    "Ymin2",                      // 29
    "Ymax2",                      // 30
    "Ymin3",                      // 31
    "Ymax3",                      // 32
    "Ymin4",                      // 33
    "Ymax4",                      // 34
    "currentMode",                // 35
    "currentPartitionType",       // 36
    "stateRevision",              // 37
    "setpointLimited",            // 38
    "uTargetAmps",                // 39
    "pidInput",                   // 40
    "pidOutput",                  // 41
    "pidError",                   // 42
    "imu_heel_deg",               // 43
    "imu_pitch_deg",              // 44
    "imu_vertical_accel_g",       // 45
    "imu_yaw_rate_dps",           // 46
    "imu_total_accel_g",          // 47
    "imu_hf_vibration_energy",    // 48
    "shutdownPhase",              // 49
    "fastOvCurrentCap",           // 50  ← no g_ prefix, matches downstream usage
    "fastOvClampCount",           // 51
    "fastOvSoftCount",            // 52
    "fastOvHardCount",            // 53
    "ch1_last_ms",                // 54
    "ch1_avg_10s",                // 55
    "ch1_worst_10s",              // 56
    "ch1_over2x_10s",             // 57
    "ch1_n_10s",                  // 58
    "ch1_avg_2m",                 // 59
    "ch1_worst_2m",               // 60
    "ch1_over2x_2m",              // 61
    "ch1_n_2m",                   // 62
    "ch1_avg_at",                 // 63
    "ch1_worst_at",               // 64
    "ch1_over2x_at",              // 65
    "ch1_n_at",                   // 66
    "BatteryV_filtered",          // 67
    "MeasuredAmps_filtered",      // 68
];
const CSV2_FIELDS = [
    "IBVMax",                           // 0
    "MeasuredAmpsMax",                  // 1
    "RPMMax",                           // 2
    "SOC_percent",                      // 3
    "EngineRunTime",                    // 4
    "AlternatorOnTime",                 // 5
    "AlternatorFuelUsed",               // 6
    "ChargedEnergy",                    // 7
    "DischargedEnergy",                 // 8
    "AlternatorChargedEnergy",          // 9
    "MaxAlternatorTemperatureF",        // 10
    "temperatureThermistor",            // 11
    "MaxTemperatureThermistor",         // 12
    "VictronCurrent",                   // 13
    "timeToFullChargeMin",              // 14
    "timeToFullDischargeMin",           // 15
    "LatitudeNMEA",                     // 16
    "LongitudeNMEA",                    // 17
    "SatelliteCountNMEA",               // 18
    "absorptionCompleteTime",           // 19
    "LastSessionDuration",              // 20
    "LastSessionMaxLoopTime",           // 21
    "lastSessionMinHeap",               // 22
    "wifiReconnectsTotal",              // 23
    "LastResetReason",                  // 24
    "ancientResetReason",               // 25
    "totalPowerCycles",                 // 26
    "MinFreeHeap",                      // 27
    "currentWeatherMode",               // 28
    "UVToday",                          // 29
    "UVTomorrow",                       // 30
    "UVDay2",                           // 31
    "weatherDataValid",                 // 32
    "SolarWatts",                       // 33
    "performanceRatio",                 // 34
    "OnOff",                            // 35
    "ManualFieldToggle",                // 36
    "HiLow",                            // 37
    "LimpHome",                         // 38
    "VeData",                           // 39
    "NMEA0183Data",                     // 40
    "NMEA2KData",                       // 41
    "AlarmActivate",                    // 42
    "TempAlarm",                        // 43
    "VoltageAlarmHigh",                 // 44
    "VoltageAlarmLow",                  // 45
    "CurrentAlarmHigh",                 // 46
    "AlarmTest",                        // 47
    "AlarmLatchEnabled",                // 48
    "AlarmLatchState",                  // 49
    "ResetAlarmLatch",                  // 50
    "MaintainMode",                     // 51
    "ResetTemp",                        // 52
    "ResetVoltage",                     // 53
    "ResetCurrent",                     // 54
    "ResetEngineRunTime",               // 55
    "ResetAlternatorOnTime",            // 56
    "ResetEnergy",                      // 57
    "ManualSOCPoint",                   // 58
    "LearningMode",                     // 59
    "LearningPaused",                   // 60
    "IgnoreLearningDuringPenalty",      // 61
    "ShowLearningDebugMessages",        // 62
    "LogAllLearningEvents",             // 63
    "CloudFeatures",                    // 64
    "LearningDryRunMode",               // 65
    "AutoSaveLearningTable",            // 66
    "ResetLearningTable",               // 67
    "ClearOverheatHistory",             // 68
    "AutoShuntGainCorrection",          // 69
    "DynamicShuntGainFactor",           // 70
    "AutoAltCurrentZero",               // 71
    "DynamicAltCurrentZero",            // 72
    "InsulationLifePercent",            // 73
    "GreaseLifePercent",                // 74
    "BrushLifePercent",                 // 75
    "PredictedLifeHours",               // 76
    "LifeIndicatorColor",               // 77
    "WindingTempOffset",                // 78
    "ManualLifePercentage",             // 79
    "UVThresholdHigh",                  // 80
    "weatherModeEnabled",               // 81
    "pKwHrToday",                       // 82
    "pKwHrTomorrow",                    // 83
    "pKwHr2days",                       // 84
    "ambientTemp",                      // 85
    "baroPressure",                     // 86
    "firmwareVersionInt",               // 87
    "deviceIdUpper",                    // 88
    "deviceIdLower",                    // 89
    "ChargedEnergy_AllTime",            // 90
    "AlternatorFuelUsed_AllTime",       // 91
    "PeakVoltage_AllTime",              // 92
    "EngineRunTime_AllTime",            // 93
    "MinVoltage",                       // 94
    "MinVoltage_AllTime",               // 95
    "ChargeCycles",                     // 96
    "ChargeCycles_AllTime",             // 97
    "EngineFuelUsed",                   // 98
    "EngineFuelUsed_AllTime",           // 99
    "TotalDistance",                    // 100
    "TotalDistance_AllTime",            // 101
    "MaxSpeed",                         // 102
    "MaxSpeed_AllTime",                 // 103
    "SolarChargedEnergy",               // 104
    "SolarChargedEnergy_AllTime",       // 105
    "AlternatorChargedEnergy_AllTime",  // 106
    "DischargedEnergy_AllTime",         // 107
    "AvgSOC_AllTime",                   // 108
    "AvgSpeed_AllTime",                 // 109
    "AvgSpeed",                         // 110
    "AlternatorOnTime_AllTime",         // 111
    "EngineCycles_AllTime",             // 112
    "MaxAlternatorTemperatureF_AllTime",// 113
    "MaxTemperatureThermistor_AllTime", // 114
    "MeasuredAmpsMax_AllTime",          // 115
    "RPMMax_AllTime",                   // 116
    "Ignition",                         // 117
    "BulkStage",                        // 118
    "WifiWakeSecondsRemaining",         // 119
    "BufferedRecordCount",              // 120
    "BufferedRecordPercent",            // 121
    "MAX_BUFFERED_RECORDS",             // 122
    "COGNMEA",                          // 123
    "SOGNMEA",                          // 124
    "ApparentWindSpeedNMEA",            // 125
    "ApparentWindAngleNMEA",            // 126
    "TrueWindSpeedNMEA",                // 127
    "TrueWindAngleNMEA",                // 128
    "LeewayNMEA",                       // 129
    "VMGNMEA",                          // 130
    "VMGTargetBearing",                 // 131
    "VMGUseTrueWind",                   // 132
    "SENSOR_UPLOAD_INTERVAL",           // 133
    "cpuLoadCore0",                     // 134
    "cpuLoadCore0Max",                  // 135
    "cpuLoadCore1",                     // 136
    "cpuLoadCore1Max",                  // 137
    "hasForcedUpdate",                  // 138
    "forcedFwVersionInt",               // 139
    "forcedUpdateDeadline",             // 140
    "stateRevision",                    // 141
    "hardwarePresent",                  // 142
    "imu_accel_x_raw",                  // 143
    "imu_accel_y_raw",                  // 144
    "imu_accel_z_raw",                  // 145
    "imu_gyro_x_raw",                   // 146
    "imu_gyro_y_raw",                   // 147
    "imu_gyro_z_raw",                   // 148
    "accel_x_min",                      // 149
    "accel_x_max",                      // 150
    "accel_x_avg",                      // 151
    "accel_y_min",                      // 152
    "accel_y_max",                      // 153
    "accel_y_avg",                      // 154
    "accel_z_min",                      // 155
    "accel_z_max",                      // 156
    "accel_z_avg",                      // 157
    "gyro_x_min",                       // 158
    "gyro_x_max",                       // 159
    "gyro_x_avg",                       // 160
    "gyro_y_min",                       // 161
    "gyro_y_max",                       // 162
    "gyro_y_avg",                       // 163
    "gyro_z_min",                       // 164
    "gyro_z_max",                       // 165
    "gyro_z_avg",                       // 166
    "heel_min",                         // 167
    "heel_max",                         // 168
    "heel_avg",                         // 169
    "pitch_min",                        // 170
    "pitch_max",                        // 171
    "pitch_avg",                        // 172
    "vertical_accel_min",               // 173
    "vertical_accel_max",               // 174
    "vertical_accel_avg",               // 175
    "total_accel_min",                  // 176
    "total_accel_max",                  // 177
    "total_accel_avg",                  // 178
    "imu_slam_count",                   // 179
    "imu_slam_peak_max",                // 180
    "imu_slam_count_lifetime",          // 181
    "imu_capsize_count",                // 182
    "imu_pitchpole_count",              // 183
    "imu_heel_change_60s",              // 184
    "imu_heel_deviation_60s",           // 185
    "imu_pitch_change_60s",             // 186
    "imu_pitch_deviation_60s",          // 187
    "imu_wave_period_sec",              // 188
    "imu_heel_max_lifetime",            // 189
    "imu_pitch_max_lifetime",           // 190
    "imu_slam_peak_lifetime",           // 191
    "imuEnabled",                       // 192
    "imuMountOrientation",              // 193
    "imu_fifo_overrun_count",           // 194
    "imu_i2c_error_count",              // 195
    "imu_unknown_tag_count",            // 196
    "imu_accel_dropped",                // 197
    "imu_gyro_dropped",                 // 198
    "imu_total_samples_accel",          // 199
    "imu_total_samples_gyro",           // 200
    "IMUReadTime2",                     // 201
    "IMUReadTime",                      // 202
    "adsI2CErrorCount",                 // 203
    "tempPIDActive",                    // 204
    "tempPIDInput_d",                   // 205
    "tempPIDSetpoint_d",                // 206
    "thermalPenaltyAmps",               // 207
    "innerTermP",                       // 208
    "innerTermI",                       // 209
    "innerTermD",                       // 210
    "outerTermP",                       // 211
    "outerTermI",                       // 212
    "outerTermD",                       // 213
    "outerTermDExternal",               // 214
    "AbsorptionVoltage",                // 215
    "AbsorptionTimeoutMs",              // 216
    "bulkVoltageHoldMs",                // 217
    "chargeStageDisplay",               // 218
    "voltageControlActive",             // 219
    "voltageTarget",                    // 220
    "voltageError",                     // 221
    "Icv",                              // 222
    "cv_I",                             // 223
    "capLimitMode",                     // 224
    "TargetVoltageMode",                // 225
    "TargetVoltageSetpoint",            // 226
    "RebulkCurrent_A",                  // 227
    "UseFloat",                         // 228
    "inIdleStage",                      // 229
    "referenceFinalized",               // 230
    "sessionErrorCount",                // 231
    "anomalyMarginAmps",                // 232
    "anomalyAlarmThreshold",            // 233
    "anomalyAlarmEnable",               // 234
    "degradationThreshold",             // 235
    "ft_rai_total_win",                 // 236
    "ft_rai_total_ses",                 // 237
    "ft_rai_ina228_win",                // 238
    "ft_rai_ina228_ses",                // 239
    "ft_rai_ads_state_win",             // 240
    "ft_rai_ads_state_ses",             // 241
    "ft_rai_bmp_state_win",             // 242
    "ft_rai_bmp_state_ses",             // 243
    "ft_rai_imu_win",                   // 244
    "ft_rai_imu_ses",                   // 245
    "fsWriteQueueDrops",                // 246
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
    "FourWay",                         // 19
    "RPMScalingFactor",                // 20
    "MaximumAllowedBatteryAmps",       // 21
    "BatteryVoltageSource",            // 22
    "LearningUpwardEnabled",           // 23
    "LearningDownwardEnabled",         // 24
    "AlternatorNominalAmps",           // 25
    "LearningUpStep",                  // 26
    "LearningDownStep",                // 27
    "AmbientTempCorrectionFactor",     // 28
    "xTime",                           // 29
    "MinLearningInterval",             // 30
    "SafeOperationThreshold",          // 31
    "PidKp",                           // 32
    "PidKi",                           // 33
    "PidKd",                           // 34
    "PidSampleDivisor",                // 35
    "MaxTableValue",                   // 36
    "MinTableValue",                   // 37
    "MaxPenaltyPercent",               // 38
    "MaxPenaltyDuration",              // 39
    "NeighborLearningFactor",          // 40
    "yyMax",                           // 41
    "LearningMemoryDuration",          // 42
    "EnableNeighborLearning",          // 43
    "EnableAmbientCorrection",         // 44
    "TuningMode",                      // 45
    "LearningTableSaveInterval",       // 46
    "rpmCurrentTable0",                // 47
    "rpmCurrentTable1",                // 48
    "rpmCurrentTable2",                // 49
    "rpmCurrentTable3",                // 50
    "rpmCurrentTable4",                // 51
    "rpmCurrentTable5",                // 52
    "rpmCurrentTable6",                // 53
    "rpmCurrentTable7",                // 54
    "rpmCurrentTable8",                // 55
    "rpmCurrentTable9",                // 56
    "currentRPMTableIndex",            // 57
    "pidInitialized",                  // 58
    "ShuntResistanceMicroOhm",         // 59
    "InvertAltAmps",                   // 60
    "InvertBattAmps",                  // 61
    "MaxDuty",                         // 62
    "MinDuty",                         // 63
    "FieldResistance",                 // 64
    "maxPoints",                       // 65
    "AlternatorCOffset",               // 66
    "BatteryCOffset",                  // 67
    "BatteryCapacity_Ah",              // 68
    "AmpSrc",                          // 69
    "R_fixed",                         // 70
    "Beta",                            // 71
    "T0_C",                            // 72
    "TempSource",                      // 73
    "IgnitionOverride",                // 74
    "FLOAT_DURATION",                  // 75
    "PulleyRatio",                     // 76
    "BatteryCurrentSource",            // 77
    "overheatCount0",                  // 78
    "overheatCount1",                  // 79
    "overheatCount2",                  // 80
    "overheatCount3",                  // 81
    "overheatCount4",                  // 82
    "overheatCount5",                  // 83
    "overheatCount6",                  // 84
    "overheatCount7",                  // 85
    "overheatCount8",                  // 86
    "overheatCount9",                  // 87
    "cumulativeNoOverheatTime0",       // 88
    "cumulativeNoOverheatTime1",       // 89
    "cumulativeNoOverheatTime2",       // 90
    "cumulativeNoOverheatTime3",       // 91
    "cumulativeNoOverheatTime4",       // 92
    "cumulativeNoOverheatTime5",       // 93
    "cumulativeNoOverheatTime6",       // 94
    "cumulativeNoOverheatTime7",       // 95
    "cumulativeNoOverheatTime8",       // 96
    "cumulativeNoOverheatTime9",       // 97
    "totalLearningEvents",             // 98
    "totalOverheats",                  // 99
    "totalSafeHours",                  // 100
    "averageTableValue",               // 101
    "timeSinceLastOverheat",           // 102
    "learningTargetFromRPM",           // 103
    "ambientTempCorrection",           // 104
    "finalLearningTarget",             // 105
    "overheatingPenaltyTimer",         // 106
    "overheatingPenaltyAmps",          // 107
    "pidSetpoint",                     // 108
    "TempToUse",                       // 109
    "rpmTableRPMPoints0",              // 110
    "rpmTableRPMPoints1",              // 111
    "rpmTableRPMPoints2",              // 112
    "rpmTableRPMPoints3",              // 113
    "rpmTableRPMPoints4",              // 114
    "rpmTableRPMPoints5",              // 115
    "rpmTableRPMPoints6",              // 116
    "rpmTableRPMPoints7",              // 117
    "rpmTableRPMPoints8",              // 118
    "rpmTableRPMPoints9",              // 119
    "LearningSettlingPeriod",          // 120
    "LearningRPMChangeThreshold",      // 121
    "LearningTempHysteresis",          // 122
    "fuelTableRPM0",                   // 123
    "fuelTableRPM1",                   // 124
    "fuelTableRPM2",                   // 125
    "fuelTableRPM3",                   // 126
    "fuelTableRPM4",                   // 127
    "fuelTableRPM5",                   // 128
    "fuelTableRPM6",                   // 129
    "fuelTableRPM7",                   // 130
    "fuelTableRPM8",                   // 131
    "fuelTableRPM9",                   // 132
    "fuelTableGPH0",                   // 133
    "fuelTableGPH1",                   // 134
    "fuelTableGPH2",                   // 135
    "fuelTableGPH3",                   // 136
    "fuelTableGPH4",                   // 137
    "fuelTableGPH5",                   // 138
    "fuelTableGPH6",                   // 139
    "fuelTableGPH7",                   // 140
    "fuelTableGPH8",                   // 141
    "fuelTableGPH9",                   // 142
    "stateRevision",                   // 143
    "SetpointRampRate",                // 144
    "DutyRampRate",                    // 145
    "SettleTimeBeforeCut",             // 146
    "TempWarnExcess",                  // 147
    "TempCritExcess",                  // 148
    "TempSustainedTimeout",            // 149
    "VoltageSpikeMargin",              // 150
    "VoltageDisagreeThreshold",        // 151
    "VoltageDisagreeTimeout",          // 152
    "rpmMinDutyTable0",                // 153
    "rpmMinDutyTable1",                // 154
    "rpmMinDutyTable2",                // 155
    "rpmMinDutyTable3",                // 156
    "rpmMinDutyTable4",                // 157
    "rpmMinDutyTable5",                // 158
    "rpmMinDutyTable6",                // 159
    "rpmMinDutyTable7",                // 160
    "rpmMinDutyTable8",                // 161
    "rpmMinDutyTable9",                // 162
    "rpmCapCurrentTable0",             // 163
    "rpmCapCurrentTable1",             // 164
    "rpmCapCurrentTable2",             // 165
    "rpmCapCurrentTable3",             // 166
    "rpmCapCurrentTable4",             // 167
    "rpmCapCurrentTable5",             // 168
    "rpmCapCurrentTable6",             // 169
    "rpmCapCurrentTable7",             // 170
    "rpmCapCurrentTable8",             // 171
    "rpmCapCurrentTable9",             // 172
    "VoltageKp",                       // 173
    "VoltageLoopInterval",             // 174
    "FIELD_COLLAPSE_DELAY",            // 175
    "SetpointRiseRate",                // 176
    "SetpointFallRate",                // 177
    "PIDTrackingGain",                 // 178
    "CAPSIZE_THRESHOLD_DEG",           // 179
    "PITCHPOLE_THRESHOLD_DEG",         // 180
    "SLAM_THRESHOLD_G",                // 181
    "imuMountOrientation",             // 182
    "socInfoAvailable",                // 183
    "TailCurrent_A",                   // 184
    "RebulkVoltage",                   // 185
    "rebulkDebounceTime",              // 186
    "MinFloatTime",                    // 187
    "SOC_BlockRebulk_percent",         // 188
    "SOC_AllowRebulk_percent",         // 189
    "accelEnabled",                    // 190
    "DutySlowRampRate",                // 191
    "ShutdownPhase2HoldMs",            // 192
    "learningUpCount0",                // 193
    "learningUpCount1",                // 194
    "learningUpCount2",                // 195
    "learningUpCount3",                // 196
    "learningUpCount4",                // 197
    "learningUpCount5",                // 198
    "learningUpCount6",                // 199
    "learningUpCount7",                // 200
    "learningUpCount8",                // 201
    "learningUpCount9",                // 202
    "TempPIDKp",                       // 203
    "TempPIDKi",                       // 204
    "TempPIDKd",                       // 205
    "TempPIDMarginF",                  // 206
    "TempPIDIntervalMs",               // 207
    "TempPIDFilterAlpha",              // 208
    "TempPIDStaleMs",                  // 209
    "TempPIDAntiWindupMarginA",        // 210
    "FreeInternalRam",                 // 211
    "TotalInternalRam",                // 212
    "LargestInternalBlock",            // 213
    "FreePSRAM",                       // 214
    "TotalPSRAM",                      // 215
    "Heapfrag",                        // 216
    "TempPIDKdExternal",               // 217
    "VoltageKi",                       // 218
    "rpmCapPowerTable0",               // 219
    "rpmCapPowerTable1",               // 220
    "rpmCapPowerTable2",               // 221
    "rpmCapPowerTable3",               // 222
    "rpmCapPowerTable4",               // 223
    "rpmCapPowerTable5",               // 224
    "rpmCapPowerTable6",               // 225
    "rpmCapPowerTable7",               // 226
    "rpmCapPowerTable8",               // 227
    "rpmCapPowerTable9",               // 228
    "VoltageTrimLimit",                // 229
    "ft_ReadAnalogInputs_win",         // 230
    "ft_ReadAnalogInputs_ses",         // 231
    "ft_AdjustFieldLearnMode_win",     // 232
    "ft_AdjustFieldLearnMode_ses",     // 233
    "ft_uploadSensorHistory_win",      // 234
    "ft_uploadSensorHistory_ses",      // 235
    "ft_uploadBufferedRecords_win",    // 236
    "ft_uploadBufferedRecords_ses",    // 237
    "ft_buildConfigPayload_win",       // 238
    "ft_buildConfigPayload_ses",       // 239
    "VeTime2",                         // 240
    "systemIDActive",           // 241
    "systemIDResultsReady",     // 242
    "systemIDRiseDelay_0",      // 243
    "systemIDRiseDelay_1",      // 244
    "systemIDRiseDelay_2",      // 245
    "systemIDFallDelay_0",      // 246
    "systemIDFallDelay_1",      // 247
    "systemIDFallDelay_2",      // 248
    "systemIDRiseAvg",          // 249
    "systemIDFallAvg",          // 250
    "InputFilterTC",            // 251
    "SystemIDStepAmplitude",    // 252
    "HardOCTripAmps",           // 253
    "HardOCDebounceMs",         // 254


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
];

// Detect if running in Capacitor (iOS/Android) vs web browser
const IS_CAPACITOR = !!window.Capacitor;
const API_BASE_URL = IS_CAPACITOR ? 'http://alternator.local' : '';
// const API_BASE_URL = IS_CAPACITOR ? 'http://10.0.0.207' : ''; // worked first
// Alternative: Use mDNS hostname or fallback to IP
// const API_BASE_URL = IS_CAPACITOR ? 'http://192.168.4.1' : ''; // For AP mode
// const API_BASE_URL = IS_CAPACITOR ? 'http://alternator.local' : ''; // For Client mode with mDNS


// Hide the main header when running as a Capacitor app
if (IS_CAPACITOR) {
    document.addEventListener('DOMContentLoaded', function () {
        const header = document.getElementById('main-header');
        if (header) {
            header.style.display = 'none';
        }
    });
}


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
setDiagnosticMode(false); // ← CHANGE THIS LINE: true=ON, false=OFF
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
    ignitionStatus.style.color = '#00a19a'; // Green
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

function reportPlotRenderingStats() {
    const now = performance.now();
    const intervalMs = now - plotRenderTracker.lastReportTime;
    const intervalSec = intervalMs / 1000;

    // Calculate stats for each plot
    const plotStats = {};
    let totalUpdates = 0;

    Object.entries(plotRenderTracker.plots).forEach(([name, plot]) => {
        const avgTime = plot.count > 0 ? (plot.totalTime / plot.count) : 0;
        plotStats[name] = {
            count: plot.count,
            avgTime: avgTime,
            maxTime: plot.maxTime,
            frequency: plot.count / intervalSec
        };
        totalUpdates += plot.count;
    });

    // Calculate overall metrics
    const totalRenderPercent = (plotRenderTracker.totalRenderTime / intervalMs) * 100;
    const avgDataRate = plotRenderTracker.dataPointsProcessed / intervalSec;
    const queueEfficiency = totalUpdates > 0 ? (totalUpdates / plotRenderTracker.queueCalls * 100) : 0;

    // Format the report
    const statsText = Object.entries(plotStats)
        .map(([name, stats]) =>
            `${name.charAt(0).toUpperCase()}=${stats.count} (${stats.avgTime.toFixed(1)}ms avg, ${stats.frequency.toFixed(1)}/s)`
        ).join(', ');

    diagLog(`[PLOT RENDER REPORT] ${intervalSec.toFixed(1)}s: ${statsText} | Total: ${totalRenderPercent.toFixed(1)}% render time | Peak: ${plotRenderTracker.peakRenderTime.toFixed(1)}ms | Data: ${avgDataRate.toFixed(1)}/s | Queue eff: ${queueEfficiency.toFixed(0)}%`);

    // Reset counters for next interval
    Object.values(plotRenderTracker.plots).forEach(plot => {
        plot.count = 0;
        plot.totalTime = 0;
        plot.maxTime = 0;
    });

    plotRenderTracker.totalRenderTime = 0;
    plotRenderTracker.peakRenderTime = 0;
    plotRenderTracker.dataPointsProcessed = 0;
    plotRenderTracker.queueCalls = 0;
    plotRenderTracker.lastReportTime = now;
}

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

function startFrameTimeMonitoring() {
    function frame() {
        trackFrameTime();
        requestAnimationFrame(frame);
    }
    requestAnimationFrame(frame);
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

// wrapper function
function processCSVDataOptimized(data) {
    return profileOperation('dataProcessing', () => {

        // Increment data points counter
        plotRenderTracker.dataPointsProcessed++;

        // Batch all plot updates
        const plotUpdates = [];
        const now = useTimestamps ? Math.floor(Date.now() / 1000) : null;

        // ALWAYS UPDATE DATA STRUCTURES - Current/Temperature plot data
        const battCurrent = 'Bcur' in data ? parseFloat(data.Bcur) / 100 : 0;
        const altCurrent = 'MeasuredAmps' in data ? parseFloat(data.MeasuredAmps) / 100 : 0;
        const fieldCurrent = 'iiout' in data ? parseFloat(data.iiout) / 10 : 0;

        // Shift all current data left and add new data at the end
        for (let i = 1; i < currentTempData[1].length; i++) {
            if (useTimestamps) {
                currentTempData[0][i - 1] = currentTempData[0][i];
            }
            currentTempData[1][i - 1] = currentTempData[1][i];
            currentTempData[2][i - 1] = currentTempData[2][i];
            currentTempData[3][i - 1] = currentTempData[3][i];
        }
        const lastCurrentIndex = currentTempData[1].length - 1;
        if (useTimestamps) {
            currentTempData[0][lastCurrentIndex] = now;
        }
        currentTempData[1][lastCurrentIndex] = battCurrent;
        currentTempData[2][lastCurrentIndex] = altCurrent;
        currentTempData[3][lastCurrentIndex] = fieldCurrent;

        if (typeof currentTempPlot !== 'undefined') {
            plotUpdates.push('current');
        }

        // ALWAYS UPDATE DATA STRUCTURES - Voltage plot data
        const adsBattV = 'BatteryV' in data ? parseFloat(data.BatteryV) / 100 : 0;
        const inaBattV = 'IBV' in data ? parseFloat(data.IBV) / 100 : 0;

        for (let i = 1; i < voltageData[1].length; i++) {
            if (useTimestamps) {
                voltageData[0][i - 1] = voltageData[0][i];
            }
            voltageData[1][i - 1] = voltageData[1][i];
            voltageData[2][i - 1] = voltageData[2][i];
        }
        const lastVoltageIndex = voltageData[1].length - 1;
        if (useTimestamps) {
            voltageData[0][lastVoltageIndex] = now;
        }
        voltageData[1][lastVoltageIndex] = adsBattV;
        voltageData[2][lastVoltageIndex] = inaBattV;

        if (typeof voltagePlot !== 'undefined') {
            plotUpdates.push('voltage');
        }

        // ALWAYS UPDATE DATA STRUCTURES - RPM plot data
        const rpmValue = 'RPM' in data ? parseFloat(data.RPM) : 0;

        for (let i = 1; i < rpmData[1].length; i++) {
            if (useTimestamps) {
                rpmData[0][i - 1] = rpmData[0][i];
            }
            rpmData[1][i - 1] = rpmData[1][i];
        }
        const lastRPMIndex = rpmData[1].length - 1;
        if (useTimestamps) {
            rpmData[0][lastRPMIndex] = now;
        }
        rpmData[1][lastRPMIndex] = rpmValue;

        if (typeof rpmPlot !== 'undefined') {
            plotUpdates.push('rpm');
        }

        // ALWAYS UPDATE DATA STRUCTURES - Temperature plot data
        const altTemp = 'AlternatorTemperatureF' in data ? parseFloat(data.AlternatorTemperatureF) / 100 : 0;

        for (let i = 1; i < temperatureData[1].length; i++) {
            if (useTimestamps) {
                temperatureData[0][i - 1] = temperatureData[0][i];
            }
            temperatureData[1][i - 1] = temperatureData[1][i];
        }
        const lastTempIndex = temperatureData[1].length - 1;
        if (useTimestamps) {
            temperatureData[0][lastTempIndex] = now;
        }
        temperatureData[1][lastTempIndex] = altTemp;

        if (typeof temperaturePlot !== 'undefined') {
            plotUpdates.push('temperature');
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

            if (typeof pidTuningPlot !== 'undefined') {
                queuePidTuningPlotUpdate();
            }
        }

        // Queue all plot updates at once (only for existing plots)
        plotUpdates.forEach(plotName => queuePlotUpdate(plotName));

        return plotUpdates.length;
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

        // Reinitialize data structures and recreate plot
        if (pidTuningPlot) {
            pidTuningPlot.destroy();
        }
        initPidTuningDataStructures();
        initPidTuningPlot();

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
        new Array(newMaxPoints).fill(0)  // Field current
    ];

    voltageData = [
        [...xAxisData], // X values
        new Array(newMaxPoints).fill(0), // ADS voltage
        new Array(newMaxPoints).fill(0)  // INA voltage
    ];

    rpmData = [
        [...xAxisData], // X values
        new Array(newMaxPoints).fill(0)  // RPM
    ];

    temperatureData = [
        [...xAxisData], // X values  
        new Array(newMaxPoints).fill(0)  // Temperature
    ];

    // Reset circular buffer indices
    currentTempIndex = 0;
    voltageIndex = 0;
    rpmIndex = 0;
    temperatureIndex = 0;

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
        { key: 'TemperatureLimitF', id: 'TemperatureLimitF_echo', transform: v => v },
        { key: 'BulkVoltage', id: 'BulkVoltage_echo', transform: v => (v / 100).toFixed(2) },
        { key: 'wavePeriod', id: 'wavePeriod_echo', transform: v => v },
        { key: 'FloatVoltage', id: 'FloatVoltage_echo', transform: v => (v / 100).toFixed(2) },
        { key: 'SwitchingFrequency', id: 'SwitchingFrequency_echo', transform: v => v },
        { key: 'yyMin', id: 'yyMin_echo', transform: v => v },
        { key: 'FieldAdjustmentInterval', id: 'FieldAdjustmentInterval_echo', transform: v => v },
        { key: 'ManualDutyTarget', id: 'ManualDutyTarget_echo', transform: v => v },
        { key: 'SwitchControlOverride', id: 'SwitchControlOverride_echo', transform: v => v },
        { key: 'OnOff', id: 'OnOff_echo', transform: v => v },
        { key: 'ManualFieldToggle', id: 'ManualFieldToggle_echo', transform: v => v === 0 ? 1 : 0 },
        { key: 'LimpHome', id: 'LimpHome_echo', transform: v => v },
        { key: 'VeData', id: 'VeData_echo', transform: v => v },
        { key: 'NMEA0183Data', id: 'NMEA0183Data_echo', transform: v => v },
        { key: 'NMEA2KData', id: 'NMEA2KData_echo', transform: v => v },
        { key: 'waveAmplitude', id: 'waveAmplitude_echo', transform: v => v },
        { key: 'CurrentThreshold', id: 'CurrentThreshold_echo', transform: v => v / 100 },
        { key: 'PeukertExponent_scaled', id: 'PeukertExponent_echo', transform: v => (v / 100).toFixed(2) },
        { key: 'ChargeEfficiency_scaled', id: 'ChargeEfficiency_echo', transform: v => (v / 10).toFixed(1) + '%' },
        { key: 'ChargedVoltage_Scaled', id: 'ChargedVoltage_echo', transform: v => (v / 100).toFixed(2) },
        { key: 'TailCurrent', id: 'TailCurrent_echo', transform: v => (v / 10).toFixed(1) },
        { key: 'ChargedDetectionTime', id: 'ChargedDetectionTime_echo', transform: v => v },
        { key: 'IgnoreTemperature', id: 'IgnoreTemperature_echo', transform: v => v },
        { key: 'bmsLogic', id: 'bmsLogic_echo', transform: v => v },
        { key: 'bmsLogicLevelOff', id: 'bmsLogicLevelOff_echo', transform: v => v },
        { key: 'AlarmActivate', id: 'AlarmActivate_echo', transform: v => v },
        { key: 'TempAlarm', id: 'TempAlarm_echo', transform: v => v },
        { key: 'VoltageAlarmHigh', id: 'VoltageAlarmHigh_echo', transform: v => v },
        { key: 'VoltageAlarmLow', id: 'VoltageAlarmLow_echo', transform: v => v },
        { key: 'CurrentAlarmHigh', id: 'CurrentAlarmHigh_echo', transform: v => v },
        { key: 'FourWay', id: 'FourWay_echo', transform: v => v },
        { key: 'RPMScalingFactor', id: 'RPMScalingFactor_echo', transform: v => v },
        { key: 'ResetTemp', id: 'ResetTemp_echo', transform: v => v },
        { key: 'ResetVoltage', id: 'ResetVoltage_echo', transform: v => v },
        { key: 'ResetCurrent', id: 'ResetCurrent_echo', transform: v => v },
        { key: 'ResetEngineRunTime', id: 'ResetEngineRunTime_echo', transform: v => v },
        { key: 'ResetAlternatorOnTime', id: 'ResetAlternatorOnTime_echo', transform: v => v },
        { key: 'ResetEnergy', id: 'ResetEnergy_echo', transform: v => v },
        { key: 'MaximumAllowedBatteryAmps', id: 'MaximumAllowedBatteryAmps_echo', transform: v => v },
        { key: 'ManualSOCPoint', id: 'ManualSOCPoint_echo', transform: v => v },
        { key: 'BatteryVoltageSource', id: 'BatteryVoltageSource_echo', transform: v => v },
        { key: 'ShuntResistanceMicroOhm', id: 'ShuntResistanceMicroOhm_echo', transform: v => v },
        { key: 'InvertAltAmps', id: 'InvertAltAmps_echo', transform: v => v },
        { key: 'InvertBattAmps', id: 'InvertBattAmps_echo', transform: v => v },
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
        { key: 'TempSource', id: 'TempSource_echo', transform: v => v },
        { key: 'IgnitionOverride', id: 'IgnitionOverride_echo', transform: v => v },
        { key: 'AmpSrc', id: 'AmpSrc_echo', transform: v => v },
        { key: 'AlarmLatchEnabled', id: 'AlarmLatchEnabled_echo', transform: v => v },
        { key: 'AlarmTest', id: 'AlarmTest_echo', transform: v => v },
        { key: 'ResetAlarmLatch', id: 'ResetAlarmLatch_echo', transform: v => v },
        { key: 'MaintainMode', id: 'MaintainMode_echo', transform: v => v },
        { key: 'UseFloat', id: 'UseFloat_echo', transform: v => v },
        { key: 'RebulkCurrent_A', id: 'RebulkCurrent_A_echo', transform: v => (v / 100).toFixed(2) },
        { key: 'TargetVoltageMode', id: 'TargetVoltageMode_echo', transform: v => v },
        { key: 'absorptionCompleteTime', id: 'absorptionCompleteTime_echo', transform: v => Math.round(v / 1000) },
        { key: 'FLOAT_DURATION', id: 'FLOAT_DURATION_echo', transform: v => (v / 3600).toFixed(2) },
        { key: 'AutoShuntGainCorrection', id: 'AutoShuntGainCorrection_echo', transform: v => v },
        { key: 'AutoAltCurrentZero', id: 'AutoAltCurrentZero_echo', transform: v => v },
        { key: 'WindingTempOffset', id: 'WindingTempOffset_echo', transform: v => v },
        { key: 'PulleyRatio', id: 'PulleyRatio_echo', transform: v => (v / 100).toFixed(2) },
        { key: 'ManualLifePercentage', id: 'ManualLifePercentage_echo', transform: v => v },
        { key: 'BatteryCurrentSource', id: 'BatteryCurrentSource_echo', transform: v => v },
        { key: 'timeAxisModeChanging', id: 'timeAxisModeChanging_echo', transform: v => v },
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
        { key: 'weatherModeEnabled', id: 'weatherModeEnabled_echo', transform: v => v },
        { key: 'SolarWatts', id: 'SolarWatts_echo', transform: v => v },
        { key: 'performanceRatio', id: 'performanceRatio_echo', transform: v => (v / 100).toFixed(2) },
        { key: 'UVThresholdHigh', id: 'UVThresholdHigh_echo', transform: v => v },
        { key: 'accelEnabled', id: 'accelEnabled_echo', transform: v => v },
        { key: 'TuningMode', id: 'TuningMode_echo', transform: v => v },
        { key: 'AutoSaveLearningTable', id: 'AutoSaveLearningTable_echo', transform: v => v },
        { key: 'CloudFeatures', id: 'CloudFeatures_echo', transform: v => v },
        { key: 'PidKp', id: 'PidKp_echo', transform: v => (v / 1000).toFixed(3) },
        { key: 'PidKi', id: 'PidKi_echo', transform: v => (v / 1000).toFixed(3) },
        { key: 'PidKd', id: 'PidKd_echo', transform: v => (v / 1000).toFixed(3) },
        { key: 'DutySlowRampRate', id: 'DutySlowRampRate_echo', transform: v => (v / 100).toFixed(2) },
        { key: 'ShutdownPhase2HoldMs', id: 'ShutdownPhase2HoldMs_echo', transform: v => Math.round(v) },
        { key: 'PidSampleDivisor', id: 'PidSampleDivisor_echo', transform: v => v },
        { key: 'xTime', id: 'xTime_echo', transform: v => v },
        { key: 'MaxTableValue', id: 'MaxTableValue_echo', transform: v => (v / 100).toFixed(2) },
        { key: 'MinTableValue', id: 'MinTableValue_echo', transform: v => (v / 100).toFixed(2) },
        { key: 'yyMax', id: 'yyMax_echo', transform: v => v },
        { key: 'LearningTableSaveInterval', id: 'LearningTableSaveInterval_echo', transform: v => v },
        { key: 'VMGTargetBearing', id: 'VMGTargetBearing_echo', transform: v => v },
        { key: 'SENSOR_UPLOAD_INTERVAL', id: 'SENSOR_UPLOAD_INTERVAL_echo', transform: v => (v / 60000).toFixed(2) },
        { key: 'DutyRampRate', id: 'DutyRampRate_echo', transform: v => (v / 100).toFixed(2) },
        { key: 'SettleTimeBeforeCut', id: 'SettleTimeBeforeCut_echo', transform: v => Math.round(v) },
        { key: 'TempWarnExcess', id: 'TempWarnExcess_echo', transform: v => (v / 100).toFixed(1) },
        { key: 'TempCritExcess', id: 'TempCritExcess_echo', transform: v => (v / 100).toFixed(1) },
        { key: 'TempSustainedTimeout', id: 'TempSustainedTimeout_echo', transform: v => Math.round(v) },
        { key: 'VoltageSpikeMargin', id: 'VoltageSpikeMargin_echo', transform: v => (v / 100).toFixed(2) },
        { key: 'HardOCTripAmps', id: 'HardOCTripAmps_echo', transform: v => (v / 10).toFixed(1) },
        { key: 'HardOCDebounceMs', id: 'HardOCDebounceMs_echo', transform: v => Math.round(v) },
        { key: 'VoltageDisagreeThreshold', id: 'VoltageDisagreeThreshold_echo', transform: v => (v / 100).toFixed(2) },
        { key: 'VoltageDisagreeTimeout', id: 'VoltageDisagreeTimeout_echo', transform: v => Math.round(v) },
        { key: 'VoltageKp', id: 'VoltageKp_echo', transform: v => (v / 100).toFixed(2) },
        { key: 'VoltageLoopInterval', id: 'VoltageLoopInterval_echo', transform: v => Math.round(v) },
        { key: 'FIELD_COLLAPSE_DELAY', id: 'FIELD_COLLAPSE_DELAY_echo', transform: v => Math.round(v / 1000) },
        { key: 'hardwarePresent', id: 'HardwarePresent_echo', transform: v => v },
        { key: 'VoltageKi', id: 'VoltageKi_echo', transform: v => (v / 100).toFixed(2) },
        { key: 'SetpointRiseRate', id: 'SetpointRiseRate_echo', transform: v => (v / 100).toFixed(2) },
        { key: 'SetpointFallRate', id: 'SetpointFallRate_echo', transform: v => (v / 100).toFixed(2) },
        { key: 'PIDTrackingGain', id: 'PIDTrackingGain_echo', transform: v => (v / 100).toFixed(2) },
        { key: 'CAPSIZE_THRESHOLD_DEG', id: 'CAPSIZE_THRESHOLD_DEG_echo', transform: v => v },
        { key: 'PITCHPOLE_THRESHOLD_DEG', id: 'PITCHPOLE_THRESHOLD_DEG_echo', transform: v => v },
        { key: 'SLAM_THRESHOLD_G', id: 'SLAM_THRESHOLD_G_echo', transform: v => (v / 10).toFixed(1) },
        { key: 'socInfoAvailable', id: 'socInfoAvailable_echo', transform: v => v },
        { key: 'TailCurrent_A', id: 'TailCurrent_A_echo', transform: v => (v / 100).toFixed(2) },
        { key: 'RebulkVoltage', id: 'RebulkVoltage_echo', transform: v => (v / 100).toFixed(2) },
        { key: 'rebulkDebounceTime', id: 'rebulkDebounceTime_echo', transform: v => Math.round(v / 1000) },
        { key: 'MinFloatTime', id: 'MinFloatTime_echo', transform: v => Math.round(v / 60000) },
        { key: 'SOC_BlockRebulk_percent', id: 'SOC_BlockRebulk_percent_echo', transform: v => v.toFixed(1) },
        { key: 'SOC_AllowRebulk_percent', id: 'SOC_AllowRebulk_percent_echo', transform: v => v.toFixed(1) },
        { key: 'TempPIDKp', id: 'TempPIDKp_echo', transform: v => (v / 1000).toFixed(3) },
        { key: 'TempPIDKi', id: 'TempPIDKi_echo', transform: v => (v / 1000).toFixed(3) },
        { key: 'TempPIDKd', id: 'TempPIDKd_echo', transform: v => (v / 1000).toFixed(3) },
        { key: 'TempPIDMarginF', id: 'TempPIDMarginF_echo', transform: v => (v / 100).toFixed(2) },
        { key: 'TempPIDIntervalMs', id: 'TempPIDIntervalMs_echo', transform: v => v },
        { key: 'TempPIDFilterAlpha', id: 'TempPIDFilterAlpha_echo', transform: v => (v / 1000).toFixed(3) },
        { key: 'TempPIDKdExternal', id: 'TempPIDKdExternal_echo', transform: v => (v / 1000).toFixed(3) },
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
        { key: 'InputFilterTC', id: 'InputFilterTC_echo', transform: v => v },
        { key: 'InputFilterTC', id: 'InputFilterTC_ID', transform: v => v },
        { key: 'SystemIDStepAmplitude', id: 'SystemIDStepAmplitude_echo', transform: v => v },

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
            if (!response.ok) throw new Error(`HTTP ${response.status}`);
            return response.json();
        })
        .then(data => {
            if (data.success) {
                messageDiv.style.backgroundColor = '#e8f5e9';
                messageDiv.style.color = '#2e7d32';
                messageDiv.textContent = 'Profile saved successfully!';
                isDeviceRegistered = true;
            } else {
                messageDiv.style.backgroundColor = '#ffebee';
                messageDiv.style.color = '#c62828';
                messageDiv.textContent = 'Error: ' + (data.error || 'Save failed');
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
    if (!confirm('Reset inner PID? Integrator will be zeroed and duty will ramp up from 0 via slew limiter.')) return;
    fetch('/resetInnerPID', { method: 'POST' })
        .then(r => r.ok ? console.log('Inner PID reset') : console.warn('Reset failed'))
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
        new Array(maxPoints).fill(0)
    ];

    voltageData = [
        [...xAxisData],
        new Array(maxPoints).fill(0),
        new Array(maxPoints).fill(0)
    ];

    rpmData = [
        [...xAxisData],
        new Array(maxPoints).fill(0)
    ];

    temperatureData = [
        [...xAxisData],
        new Array(maxPoints).fill(0)
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
            }
        ],
        scales: useTimestamps ? {
            x: { time: true },
            current: { auto: false, range: [Ymin1, Ymax1] }
        } : {
            x: {
                time: false,
                auto: false,
                range: [xAxisData[0], xAxisData[xAxisData.length - 1]]
            },
            current: { auto: false, range: [Ymin1, Ymax1] }
        },
        axes: useTimestamps ? [
            { grid: { show: true } },
            {
                scale: "current", // Use appropriate scale name for each plot
                label: "Amperes", // Use appropriate label for each plot
                grid: { show: true },
                side: 3
            }
        ] : [
            {
                label: "Seconds Ago",
                grid: { show: true }
            },
            {
                scale: "current", // Use appropriate scale name for each plot
                label: "Amperes", // Use appropriate label for each plot
                grid: { show: true },
                side: 3
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
                            { label: "Field Current (A)", color: "#9C27B0" }
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
            }
        ],
        scales: useTimestamps ? {
            x: { time: true },
            voltage: { auto: false, range: [Ymin2 / 100, Ymax2 / 100] }
        } : {
            x: {
                time: false,
                auto: false,
                range: [xAxisData[0], xAxisData[xAxisData.length - 1]]
            },
            voltage: { auto: false, range: [Ymin2 / 100, Ymax2 / 100] }
        },
        axes: useTimestamps ? [
            { grid: { show: true } },
            {
                scale: "voltage", // Use appropriate scale name for each plot
                label: "Volts", // Use appropriate label for each plot
                grid: { show: true },
                side: 3
            }
        ] : [
            {
                label: "Seconds Ago",
                grid: { show: true }
            },
            {
                scale: "voltage", // Use appropriate scale name for each plot
                label: "Volts", // Use appropriate label for each plot
                grid: { show: true },
                side: 3
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
                            { label: "INA Battery (V)", color: "#607D8B" }
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
            }
        ],
        scales: useTimestamps ? {
            x: { time: true },
            rpm: { auto: false, range: [Ymin3, Ymax3] }
        } : {
            x: {
                time: false,
                auto: false,
                range: [xAxisData[0], xAxisData[xAxisData.length - 1]]
            },
            rpm: { auto: false, range: [Ymin3, Ymax3] }
        },

        axes: useTimestamps ? [
            { grid: { show: true } },
            {
                scale: "rpm", // Use appropriate scale name for each plot
                label: "revs/min", // Use appropriate label for each plot
                grid: { show: true },
                side: 3
            }
        ] : [
            {
                label: "Seconds Ago",
                grid: { show: true }
            },
            {
                scale: "rpm", // Use appropriate scale name for each plot
                label: "revs/min", // Use appropriate label for each plot
                grid: { show: true },
                side: 3
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
                            { label: "RPM", color: "#E91E63" }
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
            }
        ],
        scales: useTimestamps ? {
            x: { time: true },
            temperature: { auto: false, range: [Ymin4, Ymax4] }
        } : {
            x: {
                time: false,
                auto: false,
                range: [xAxisData[0], xAxisData[xAxisData.length - 1]]
            },
            temperature: { auto: false, range: [Ymin4, Ymax4] }
        },
        axes: useTimestamps ? [
            { grid: { show: true } },
            {
                scale: "temperature", // Use appropriate scale name for each plot
                label: "F", // Use appropriate label for each plot
                grid: { show: true },
                side: 3
            }
        ] : [
            {
                label: "Seconds Ago",
                grid: { show: true }
            },
            {
                scale: "temperature", // Use appropriate scale name for each plot
                label: "F", // Use appropriate label for each plot
                grid: { show: true },
                side: 3
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
                            { label: "Alt. Temp (°F)", color: "#FF5722" }
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
}

//Staleness stuff
function applyStaleStyleByAge(elementId, ageMs, staleThreshold = STALE_THRESHOLD_DEFAULT_MS) {
    const element = document.getElementById(elementId);
    if (!element) {
        diagWarn(`Element ${elementId} not found for stale styling`);
        return;
    }

    const isStale = ageMs > staleThreshold;

    // Only update DOM if stale state changed
    if (element._lastStaleState === isStale) {
        return; // No change needed
    }

    element._lastStaleState = isStale;

    if (isStale) {
        element.style.opacity = "0.5";
        element.style.color = "#999999";
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

    // Apply staleness using ages directly 
    applyStaleStyleByAge("HeadingNMEAID", window.sensorAges.heading);                      // GPS heading
    applyStaleStyleByAge("LatitudeNMEA_ID", window.sensorAges.latitude);            // GPS latitude
    applyStaleStyleByAge("LongitudeNMEA_ID", window.sensorAges.longitude);          // GPS longitude
    applyStaleStyleByAge("SatelliteCountNMEA_ID", window.sensorAges.satellites);    // GPS satellite count
    applyStaleStyleByAge("VictronVoltageID", window.sensorAges.victronVoltage);     // Victron voltage
    applyStaleStyleByAge("VictronCurrentID", window.sensorAges.victronCurrent);     // Victron current
    applyStaleStyleByAge("AltTempID", window.sensorAges.alternatorTemp, STALE_THRESHOLD_TEMP_MS);
    applyStaleStyleByAge("temperatureThermistorID", window.sensorAges.thermistorTemp, STALE_THRESHOLD_TEMP_MS);
    applyStaleStyleByAge("RPMID", window.sensorAges.rpm);                           // Engine RPM
    applyStaleStyleByAge("MeasAmpsID", window.sensorAges.measuredAmps);             // Alternator current
    applyStaleStyleByAge("BatteryVID", window.sensorAges.batteryV);                 // ADS battery voltage
    applyStaleStyleByAge("IBVID", window.sensorAges.ibv);                           // INA battery voltage
    applyStaleStyleByAge("BCurrID", window.sensorAges.bcur);                        // Battery current
    applyStaleStyleByAge("ADS3ID", window.sensorAges.channel3V);                    // ADS Channel 3 voltage
    applyStaleStyleByAge("dutyCycleID", window.sensorAges.dutyCycle);               // Field duty cycle
    applyStaleStyleByAge("FieldVoltsID", window.sensorAges.fieldVolts);             // Field voltage (calculated)
    applyStaleStyleByAge("FieldAmpsID", window.sensorAges.fieldAmps);               // Field current (calculated)

    //banner
    applyStaleStyleByAge("header-voltage", window.sensorAges.ibv);
    applyStaleStyleByAge("header-soc", window.sensorAges.soc);
    applyStaleStyleByAge("header-alt-current", window.sensorAges.measuredAmps);
    applyStaleStyleByAge("header-batt-current", window.sensorAges.bcur);
    applyStaleStyleByAge("header-alt-temp", window.sensorAges.alternatorTemp, STALE_THRESHOLD_TEMP_MS);
    applyStaleStyleByAge("header-rpm", window.sensorAges.rpm);
    applyStaleStyleByAge("dutyCycleID3", window.sensorAges.dutyCycle);               // Field duty cycle

    updateWeatherAlerts();
}

// Start the staleness detection system - call this from window.load
function startStalenessDetection() {
    // Update staleness styling every 2 seconds
    setTrackedInterval(updateAllStalenessStyles, 2000);
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

    const formData = new FormData();
    formData.append("password", currentAdminPassword);
    formData.append("TriggerWeatherUpdate", "1");

    fetchWithTimeout(buildURL("/get?" + new URLSearchParams(formData).toString()), {}, 8000)
        .then(() => diagLog("Weather update triggered"))
        .catch(err => diagError("Weather update failed:", err));
}

function updateGPSDisplay(lat, lon) {
    document.getElementById('LatitudeNMEA_display').textContent = lat.toFixed(6);
    document.getElementById('LongitudeNMEA_display').textContent = lon.toFixed(6);

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
        // HiLow / charge rate mode handled via pendingToggles in CSVData2 handler
        updateCheckbox("VeData_checkbox", data.VeData, "VeData");
        updateCheckbox("NMEA0183Data_checkbox", data.NMEA0183Data, "NMEA0183Data");
        updateCheckbox("NMEA2KData_checkbox", data.NMEA2KData, "NMEA2KData");
        updateCheckbox("IgnoreTemperature_checkbox", data.IgnoreTemperature, "IgnoreTemperature");
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
        updateCheckbox("timeAxisModeChanging_checkbox", data.timeAxisModeChanging, "timeAxisModeChanging");
        updateCheckbox("weatherModeEnabled_checkbox", data.weatherModeEnabled, "weatherModeEnabled");
        updateCheckbox("accelEnabled_checkbox", data.accelEnabled, "accelEnabled");
        updateCheckbox("UseFloat_checkbox", data.UseFloat, "UseFloat");


        updateCheckbox("anomalyAlarmEnable_checkbox", data.anomalyAlarmEnable, "anomalyAlarmEnable");
        updateCheckbox("TuningMode_checkbox", data.TuningMode, "TuningMode");
        updateCheckbox("AutoSaveLearningTable_checkbox", data.AutoSaveLearningTable, "AutoSaveLearningTable");
        updateCheckbox("socInfoAvailable_checkbox", data.socInfoAvailable, "socInfoAvailable");
        updateCheckbox("CloudFeatures_checkbox", data.CloudFeatures, "CloudFeatures");
        if (data.CloudFeatures !== undefined) {
            updateCloudFeaturesTabVisibility(data.CloudFeatures === 1);
        }
        updateCheckbox("VMGUseTrueWind_checkbox", data.VMGUseTrueWind, "VMGUseTrueWind");
        updateCheckbox("HardwarePresent_checkbox", data.hardwarePresent, "hardwarePresent");
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

let currentChargeRateMode = 'normal'; // tracks active mode; updated by setChargeRateMode()

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
    const normalBtn = document.getElementById('chargeRateNormalBtn');
    const lowBtn = document.getElementById('chargeRateLowBtn');
    if (normalBtn) normalBtn.classList.toggle('cap-mode-active', mode === 'normal');
    if (lowBtn) lowBtn.classList.toggle('cap-mode-active', mode === 'low');
}

function handleChargeRateModeToggle(mode) {
    const desiredValue = (mode === 'low') ? 0 : 1;
    pendingToggles.set('HiLow', { desiredValue: desiredValue, baseRev: lastSeenRev });
    setChargeRateMode(mode); // optimistic UI immediately
    submitChargeRateModeImmediately(desiredValue).catch(err => diagLog('chargeRateMode submit failed: ' + err));
}

function getLiveBatteryV() {
    // BatteryV in CSVData1 is scaled ×100; fall back to 12V if not yet received
    return ((window._debugData?.BatteryV) || 1200) / 100;
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
        fieldAmps: 999999
    };

    document.getElementById("AlarmLatchEnabled_checkbox").checked = (document.getElementById("AlarmLatchEnabled").value === "1");
    document.getElementById("MaintainMode_checkbox").checked = (document.getElementById("MaintainMode").value === "1");
    document.getElementById("TargetVoltageMode_checkbox").checked = (document.getElementById("TargetVoltageMode").value === "1");
    document.getElementById("HardwarePresent_checkbox").checked = (document.getElementById("hardwarePresent").value === "1");
    document.getElementById("UseFloat_checkbox").checked = (document.getElementById("UseFloat").value === "1");


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
        setTimeout(() => showSubTab('settings', 'vessel-info'), 100);
    } else {
        showMainTab('livedata');
        setTimeout(() => {
            const defaultLiveDataSubTab = document.querySelector('#livedata .sub-tab[onclick*="alternator"]');
            if (defaultLiveDataSubTab) defaultLiveDataSubTab.click();
        }, 100);
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
    initEffPlot();
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
        // Console event listener (in your source event listeners section)
        source.addEventListener("console", function (event) {
            if (consolePaused) return; // Don't add messages while paused

            const timestamp = new Date().toLocaleTimeString();
            const consoleDiv = document.getElementById("consoleOutput");
            if (!consoleDiv) return;

            const line = document.createElement("div");
            line.textContent = `[${timestamp}] ${event.data}`;
            consoleDiv.appendChild(line);
            consoleDiv.scrollTop = consoleDiv.scrollHeight;

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

            // Immediately mark as connected when data arrives
            updateInlineStatus(true);

            const raw = e.data.split(',').map(Number);
            const declaredCount = raw[0];
            const values = raw.slice(1);

            if (values.length !== declaredCount) {
                console.log('CSV1 bail:', declaredCount, values.length, raw.slice(0, 5));
                if (!window.lastCsvWarningTime) window.lastCsvWarningTime = 0;
                const now = Date.now();
                if ((now - window.lastCsvWarningTime) > 10000) {
                    diagWarn(`CSV1 length mismatch: declared=${declaredCount}, actual=${values.length}`);
                    window.lastCsvWarningTime = now;
                }
                return;
            }
            if (declaredCount !== CSV1_FIELDS.length) {
                if (!window.lastCsvWarningTime) window.lastCsvWarningTime = 0;
                const now = Date.now();
                if ((now - window.lastCsvWarningTime) > 10000) {
                    diagWarn(`CSV1 schema mismatch: ESP32=${declaredCount}, UI=${CSV1_FIELDS.length}`);
                    window.lastCsvWarningTime = now;
                }
                return;
            }

            const data = Object.fromEntries(CSV1_FIELDS.map((key, i) => [key, values[i]]));

            updateIMUAlignmentDisplayFromData(data);

            if (data.stateRevision !== undefined) {
                lastSeenRev = data.stateRevision;
            }

            // Track the interval for toggle functionality
            if (data.webgaugesinterval) {
                window._lastKnownInterval = data.webgaugesinterval;
            }
            if (data.plotTimeWindow) {
                window._lastKnownTimeWindow = data.plotTimeWindow;
            }
            //i'm in the CSV handler here
            // Configuration check logic 
            configCheckCounter++;
            const checkInterval = getConfigCheckInterval(data.webgaugesinterval);

            if (configCheckCounter >= checkInterval) {
                configCheckCounter = 0; // Reset counter
                updatePlotConfiguration(data);
            }

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
                    if (fieldWrapper) fieldWrapper.className = 'reading-value field-status-active';
                    if (dutyCycleDisplay) dutyCycleDisplay.style.display = 'inline';
                } else if (data.fieldActiveStatus === 2) {
                    fieldIndicator.textContent = 'RAMP DOWN';
                    if (fieldWrapper) fieldWrapper.className = 'reading-value field-status-rampdown';
                    if (dutyCycleDisplay) dutyCycleDisplay.style.display = 'inline';
                } else if (data.fieldActiveStatus === 3) {
                    fieldIndicator.textContent = 'MANUAL';
                    if (fieldWrapper) fieldWrapper.className = 'reading-value field-status-manual';
                    if (dutyCycleDisplay) dutyCycleDisplay.style.display = 'inline';
                } else {
                    fieldIndicator.textContent = 'OFF';
                    if (fieldWrapper) fieldWrapper.className = 'reading-value field-status-inactive';
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
                    else if (["BatteryV", "uTargetAmps", "AlternatorTemperatureF", "MeasuredAmps", "Ymin2", "Ymax2", "setpointLimited", "pidInput", "pidOutput", "pidError", "Bcur", "Channel3V", "IBV", "VictronVoltage", "vvout", "imu_heel_deg", "imu_pitch_deg", "imu_yaw_rate_dps", "fastOvCurrentCap", "ch1_avg_10s", "ch1_avg_2m", "ch1_avg_at", "BatteryV_filtered", "MeasuredAmps_filtered"].includes(key)) {
                        newTextContent = (value / 100).toFixed(2);
                    }
                    else if (key === "dutyCycle") {
                        newTextContent = (value / 100).toFixed(2) + "%";
                    }

                    // Values scaled by 10 on server  
                    else if (["iiout"].includes(key)) {
                        newTextContent = (value / 10).toFixed(1);
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
                ["LoopTimeID", "LoopTime"],              // Loop Time 
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
                ["imu_hf_vibration_energy_ID", "imu_hf_vibration_energy"]
            ];

            // Update alarm status, this is GPIO21 buzzer/alarm
            updateAlarmStatus(data);

            // Other fields - update every 4th cycle - ALL remaining payload1 variables (were non-critical OR new)
            const otherFields = [
                ["AltTempID", "AlternatorTemperatureF"],
                ["VictronVoltageID", "VictronVoltage"],
                ["SendWifiTimeID", "SendWifiTime"],
                ["VeTimeID", "VeTime"],
                ["MaximumLoopTimeID", "MaximumLoopTime"],
                ["HeadingNMEAID", "HeadingNMEA"],
                ["EngineCyclesID", "EngineCycles"],
                ["header-voltage", "IBV"],
                ["header-alt-current", "MeasuredAmps"],
                ["header-batt-current", "Bcur"],
                ["header-alt-temp", "AlternatorTemperatureF"],
                ["header-rpm", "RPM"],
                ["WifiStrengthID", "WifiStrength"],
                ["CurrentSessionDurationID", "CurrentSessionDuration"],
                ["currentModeID", "currentMode"],
                ["currentPartitionTypeID", "currentPartitionType"],
                ["fastOvCurrentCapID", "fastOvCurrentCap"],
                ["fastOvClampCountID", "fastOvClampCount"],
                ["fastOvSoftCountID", "fastOvSoftCount"],
                ["fastOvHardCountID", "fastOvHardCount"],
                ["ch1_last_ms_ID", "ch1_last_ms"],
                ["ch1_avg_10s_ID", "ch1_avg_10s"],
                ["ch1_worst_10s_ID", "ch1_worst_10s"],
                ["ch1_over2x_10s_ID", "ch1_over2x_10s"],
                ["ch1_n_10s_ID", "ch1_n_10s"],
                ["ch1_avg_2m_ID", "ch1_avg_2m"],
                ["ch1_worst_2m_ID", "ch1_worst_2m"],
                ["ch1_over2x_2m_ID", "ch1_over2x_2m"],
                ["ch1_n_2m_ID", "ch1_n_2m"],
                ["ch1_avg_at_ID", "ch1_avg_at"],
                ["ch1_worst_at_ID", "ch1_worst_at"],
                ["ch1_over2x_at_ID", "ch1_over2x_at"],
                ["ch1_n_at_ID", "ch1_n_at"],
                ["BatteryV_filtered_ID", "BatteryV_filtered"],
                ["MeasuredAmps_filtered_ID", "MeasuredAmps_filtered"]
            ];

            // Update critical fields every cycle
            updateFields(criticalFields);
            processCSVDataOptimized(data); // this is for plotting
            updateIMUAlignmentDisplayFromData(data);

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
        };
        window._csvDataHandler = handleCSVData; // Store for demo mode
        source.addEventListener('CSVData', handleCSVData, false);

        source.addEventListener('CSVData2', function (e) {
            const raw = e.data.split(',').map(Number);

            const declaredCount = raw[0];
            const values = raw.slice(1);

            if (values.length !== declaredCount) {
                diagWarn(`CSV2 length mismatch: declared=${declaredCount}, actual=${values.length}`);
                return;
            }
            if (declaredCount !== CSV2_FIELDS.length) {
                diagWarn(`CSV2 schema mismatch: ESP32=${declaredCount}, UI=${CSV2_FIELDS.length}`);
                return;
            }

            const data = Object.fromEntries(CSV2_FIELDS.map((key, i) => [key, values[i]]));


            if (data.stateRevision !== undefined) {
                lastSeenRev = data.stateRevision;
            }

            handleForcedUpdate(data);

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
                        "imu_heel_max_lifetime", "imu_pitch_max_lifetime"].includes(key)) {
                        newTextContent = (value / 100).toFixed(2);
                    }
                    // Values scaled by 100 on server (existing)
                    else if (["IBVMax", "ChargeCycles", "ChargeCycles_AllTime", "tempPIDInput_d", "tempPIDSetpoint_d", "thermalPenaltyAmps", "MeasuredAmpsMax", "SOC_percent", "VictronCurrent", "performanceRatio", "UVThresholdHigh",
                        "PeakVoltage_AllTime", "MinVoltage", "MinVoltage_AllTime", "AvgSOC_AllTime", "AvgSpeed_AllTime", "InsulationLifePercent", "GreaseLifePercent",
                        "BrushLifePercent", "pKwHrToday", "pKwHrTomorrow", "pKwHr2days", "AvgSpeed", "MeasuredAmpsMax_AllTime", "SOGNMEA", "ApparentWindSpeedNMEA", "TrueWindSpeedNMEA", "VMGNMEA"].includes(key)) {
                        newTextContent = (value / 100).toFixed(2);
                    }
                    // GPS coordinates scaled by 1,000,000 on server
                    else if (["LatitudeNMEA", "LongitudeNMEA"].includes(key)) {
                        newTextContent = (value / 1000000).toFixed(6);
                    }
                    // Value scaled by 1000000 on server  
                    else if (["LastSessionMaxLoopTime"].includes(key)) {
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
                    else if (key === "voltageControlActive") {
                        newTextContent = value === 1 ? "YES" : "NO";
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
                    else if (key.startsWith("ft_rai_")) {
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
                    else if (["innerTermP", "innerTermI", "innerTermD", "outerTermP", "outerTermI", "outerTermD", "outerTermDExternal"].includes(key)) {
                        newTextContent = (value / 100).toFixed(2);
                    }
                    // Session duration in minutes
                    else if (["LastSessionDuration"].includes(key)) {
                        newTextContent = formatMinutesToDHM(value);
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
                    ignitionStatus.textContent = 'IGN ON';
                    ignitionStatus.className = 'reading-value ignition-on';
                } else {
                    ignitionStatus.textContent = 'IGN OFF';
                    ignitionStatus.className = 'reading-value ignition-off';
                }
            }
            // Update charging mode
            const chargingMode = document.getElementById('charging-mode');
            const bulkStageValue = document.getElementById('BulkStageID');
            if (chargingMode && bulkStageValue) {
                const bulkStage = parseInt(bulkStageValue.textContent);
                chargingMode.textContent = bulkStage === 1 ? 'BULK' : 'FLOAT';
            }
            // Update WiFi wake notification
            const wifiWakeStatus = document.getElementById('wifi-wake-status');
            const wifiWakeSeconds = document.getElementById('wifi-wake-seconds');
            const wifiWakeValue = document.getElementById('WifiWakeSecondsRemainingID');
            if (wifiWakeStatus && wifiWakeSeconds && wifiWakeValue) {
                const seconds = parseInt(wifiWakeValue.textContent);
                if (seconds > 0) {
                    wifiWakeSeconds.textContent = seconds;
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

                // IMU Lifetime Maximums
                ["imu_heel_max_lifetime_ID", "imu_heel_max_lifetime"],
                ["imu_pitch_max_lifetime_ID", "imu_pitch_max_lifetime"],
                ["imu_slam_peak_lifetime_ID", "imu_slam_peak_lifetime"],

                // IMU Diagnostics
                ["imuEnabled_ID", "imuEnabled"],
                ["imuMountOrientation_ID", "imuMountOrientation"],
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

                //Outer PID loop (temp)
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
                ["voltageTarget_display", "voltageTarget"],
                ["voltageError_display", "voltageError"],
                ["Icv_display", "Icv"],
                ["cv_I_display", "cv_I"],
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

            ];

            // Update other fields every cycle
            updateFields(otherFields);

            // Outer PID terms displayed as current contributions (sign-flipped:
            // positive = adding amps, negative = removing amps)
            for (const [id, key] of [
                ["outerTermP_display", "outerTermP"],
                ["outerTermI_display", "outerTermI"],
                ["outerTermD_display", "outerTermD"],
                ["outerTermDExternal_display", "outerTermDExternal"]
            ]) {
                const raw = data[key];
                if (raw === undefined) continue;
                const newText = (-raw / 100).toFixed(2);
                const cacheKey = `${id}_${key}`;
                if (lastValues.get(cacheKey) !== newText) {
                    lastValues.set(cacheKey, newText);
                    scheduleDOMUpdateOptimized(id, newText);
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
            if (data.capLimitMode !== undefined) {
                const pending = pendingToggles.get('capLimitMode');
                if (pending) {
                    if (data.capLimitMode === pending.desiredValue) {
                        // ESP32 confirmed our change
                        pendingToggles.delete('capLimitMode');
                        setCapMode(data.capLimitMode === 1 ? 'kw' : 'amps');
                    } else if (
                        (data.stateRevision !== undefined && data.stateRevision > pending.baseRev) ||
                        (pending.deadlineMs !== undefined && Date.now() > pending.deadlineMs)
                    ) {
                        // Newer revision arrived and still disagrees, or deadline expired — revert
                        pendingToggles.delete('capLimitMode');
                        setCapMode(data.capLimitMode === 1 ? 'kw' : 'amps');
                    } else {
                        // Still waiting — set deadline if not set, keep optimistic UI
                        if (pending.deadlineMs === undefined) {
                            pending.deadlineMs = Date.now() + 2500;
                        }
                    }
                } else {
                    setCapMode(data.capLimitMode === 1 ? 'kw' : 'amps');
                }
            }
            if (data.HiLow !== undefined) {
                const pending = pendingToggles.get('HiLow');
                if (pending) {
                    if (data.HiLow === pending.desiredValue) {
                        pendingToggles.delete('HiLow');
                        setChargeRateMode(data.HiLow === 0 ? 'low' : 'normal');
                    } else if (
                        (data.stateRevision !== undefined && data.stateRevision > pending.baseRev) ||
                        (pending.deadlineMs !== undefined && Date.now() > pending.deadlineMs)
                    ) {
                        pendingToggles.delete('HiLow');
                        setChargeRateMode(data.HiLow === 0 ? 'low' : 'normal');
                    } else {
                        if (pending.deadlineMs === undefined) {
                            pending.deadlineMs = Date.now() + 2500;
                        }
                    }
                } else {
                    setChargeRateMode(data.HiLow === 0 ? 'low' : 'normal');
                }
            }

            // Update life indicators
            updateLifeIndicators(data);

            const chargeStageEl = document.getElementById('charge-stage');
            if (chargeStageEl) {
                const stage = data.chargeStageDisplay;
                if (stage === 1) {
                    chargeStageEl.textContent = 'BULK';
                    chargeStageEl.className = 'charge-stage charge-stage-bulk';
                } else if (stage === 2) {
                    chargeStageEl.textContent = 'ABSORPTION';
                    chargeStageEl.className = 'charge-stage charge-stage-absorption';
                } else if (stage === 3) {
                    chargeStageEl.textContent = 'FLOAT';
                    chargeStageEl.className = 'charge-stage charge-stage-float';
                } else if (stage === 4) {
                    chargeStageEl.textContent = 'MANUAL';
                    chargeStageEl.className = 'charge-stage charge-stage-manual';
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

            updateFirmwareVersion(data.firmwareVersionInt);
            updateDeviceId();
        }, false);

        source.addEventListener('CSVData3', function (e) {
            const raw = e.data.split(',').map(Number);

            const declaredCount = raw[0];
            const values = raw.slice(1);

            if (values.length !== declaredCount) {
                diagWarn(`CSV3 length mismatch: declared=${declaredCount}, actual=${values.length}`);
                return;
            }
            if (declaredCount !== CSV3_FIELDS.length) {
                diagWarn(`CSV3 schema mismatch: ESP32=${declaredCount}, UI=${CSV3_FIELDS.length}`);
                return;
            }

            const data = Object.fromEntries(CSV3_FIELDS.map((key, i) => [key, values[i]]));

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
                    // Values already pre-divided by 1000 in C++ — multiply to restore if needed, or just display raw
                    else if (["timeSinceLastOverheat", "overheatingPenaltyTimer"].includes(key)) {
                        newTextContent = (value).toFixed(0);  // already in seconds from C++
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
                ["safeHours0_display", "safeHours0"],
                ["safeHours1_display", "safeHours1"],
                ["safeHours2_display", "safeHours2"],
                ["safeHours3_display", "safeHours3"],
                ["safeHours4_display", "safeHours4"],
                ["safeHours5_display", "safeHours5"],
                ["safeHours6_display", "safeHours6"],
                ["safeHours7_display", "safeHours7"],
                ["safeHours8_display", "safeHours8"],
                ["safeHours9_display", "safeHours9"],
                ["pidSetpoint_display", "pidSetpoint"],
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
                ["VeTime2_ID", "VeTime2"],
                ["systemIDActive_ID", "systemIDActive"],
                ["systemIDResultsReady_ID", "systemIDResultsReady"],
                ["systemIDRiseDelay_0_ID", "systemIDRiseDelay_0"],
                ["systemIDRiseDelay_1_ID", "systemIDRiseDelay_1"],
                ["systemIDRiseDelay_2_ID", "systemIDRiseDelay_2"],
                ["systemIDFallDelay_0_ID", "systemIDFallDelay_0"],
                ["systemIDFallDelay_1_ID", "systemIDFallDelay_1"],
                ["systemIDFallDelay_2_ID", "systemIDFallDelay_2"],
                ["systemIDRiseAvg_ID", "systemIDRiseAvg"],
                ["systemIDFallAvg_ID", "systemIDFallAvg"]

            ];

            // CSVData3
            updateFields(otherFields);   // Step 3: Process the whitelist
            updateSystemIDTelemetryLabels(data.systemIDActive, data.systemIDResultsReady);
            updatePidTuningConfiguration(data);  // ADD THIS LINE - Update PID plot config

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
                diagWarn(`TimestampData length mismatch: declared=${declaredCount}, actual=${values.length}`);
                return;
            }
            if (declaredCount !== TS_FIELDS.length) {
                diagWarn(`TimestampData schema mismatch: ESP32=${declaredCount}, UI=${TS_FIELDS.length}`);
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
                fieldAmps: data.ts_FieldAmps
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
    document.getElementById("AutoSaveLearningTable_checkbox").checked = (document.getElementById("AutoSaveLearningTable").value === "1");
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
                'ft_rai_ads_state_ses_ID', 'ft_rai_bmp_state_ses_ID', 'ft_rai_imu_ses_ID',
                'VeTime2_ID', 'ft_AdjustFieldLearnMode_ses_ID', 'ft_uploadSensorHistory_ses_ID',
                'ft_uploadBufferedRecords_ses_ID', 'ft_buildConfigPayload_ses_ID',
                'cpuLoadCore0Max_display', 'cpuLoadCore1Max_display',
                'MaximumLoopTimeID',
                'ch1_worst_10s_ID', 'ch1_over2x_10s_ID', 'ch1_avg_10s_ID', 'ch1_n_10s_ID',
                'ch1_worst_2m_ID', 'ch1_over2x_2m_ID', 'ch1_avg_2m_ID', 'ch1_n_2m_ID',
                'ch1_worst_at_ID', 'ch1_over2x_at_ID', 'ch1_avg_at_ID', 'ch1_n_at_ID'
            ];
            ids.forEach(id => {
                const el = document.getElementById(id);
                if (el) el.textContent = '0';
            });
        })
        .catch(err => diagError('Reset peaks failed:', err));
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
    console.log('[DEBUG] showSubTab called:', parentTab, subTabName, 'vesselInfoComplete:', vesselInfoComplete);

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

    const br = wrap.getBoundingClientRect();

    svg.setAttribute('viewBox', `0 0 ${br.width} ${br.height}`);

    // Keep defs, clear everything else
    const defs = svg.querySelector('defs');
    svg.innerHTML = '';
    if (defs) svg.appendChild(defs);

    const ypad = 8;
    const w = 14;

    function rect(el) {
        const r = el.getBoundingClientRect();
        return {
            l: r.left - br.left,
            t: r.top - br.top,
            b: r.bottom - br.top
        };
    }

    // 10 RPM points => 9 buckets (0..8)
    for (let i = 0; i < 9; i++) {
        const aEl = document.getElementById('rpmTableRPMPoints' + i + '_input');
        const bEl = document.getElementById('rpmTableRPMPoints' + (i + 1) + '_input');
        const ohEl = document.getElementById('overheatCount' + i + '_display');
        if (!aEl || !bEl) continue;

        const A = rect(aEl), B = rect(bEl);
        const y1 = A.t - ypad;
        const y2 = B.b + ypad;
        const mid = (y1 + y2) / 2;

        // Two x-tracks so glyphs never overlap
        const x = (i % 2 === 0) ? 0 : 12;

        // Keep curvature stable; symmetric top/bottom
        const cy = Math.max(16, (y2 - y1) * 0.28);

        // Draw the curved path
        const p = document.createElementNS("http://www.w3.org/2000/svg", "path");
        const d = `M ${x + w} ${y1} Q ${x} ${y1 + cy} ${x + w} ${mid} Q ${x} ${y2 - cy} ${x + w} ${y2}`;
        p.setAttribute('d', d);
        p.setAttribute('fill', 'none');
        p.setAttribute('stroke-linecap', 'round');
        p.setAttribute('stroke-linejoin', 'round');
        p.setAttribute('stroke-width', '3.4');
        p.setAttribute('stroke', (i % 2 === 0) ?
            getComputedStyle(document.documentElement).getPropertyValue('--glyph-teal') :
            getComputedStyle(document.documentElement).getPropertyValue('--glyph-black'));
        svg.appendChild(p);

        // Arrow from glyph centerpoint to start of Overheats number
        if (ohEl) {
            const ob = ohEl.getBoundingClientRect();
            const tx = ob.left - br.left;
            const ty = (ob.top + ob.bottom) / 2 - br.top;

            const line = document.createElementNS("http://www.w3.org/2000/svg", "line");
            line.setAttribute('x1', x + w);
            line.setAttribute('y1', mid);
            const endX = Math.max(tx - 10, (x + w) + 6);
            line.setAttribute('x2', endX);
            line.setAttribute('y2', ty);
            line.setAttribute('stroke-width', '2.6');
            line.setAttribute('stroke', (i % 2 === 0) ?
                getComputedStyle(document.documentElement).getPropertyValue('--glyph-teal') :
                getComputedStyle(document.documentElement).getPropertyValue('--glyph-black'));
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


function getLogTimestamp() {
    const now = new Date();
    const pad = n => String(n).padStart(2, '0');
    return `${now.getFullYear()}${pad(now.getMonth() + 1)}${pad(now.getDate())}_${pad(now.getHours())}${pad(now.getMinutes())}`;
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
//   #alt-health-badge    — small colored badge on main dashboard

function updateEffAnomalyDisplay() {
    const banner = document.getElementById('eff-anomaly-banner');
    const msgEl = document.getElementById('eff-anomaly-message');
    const badgeEl = document.getElementById('alt-health-badge');

    const state = effMatrixState.state;
    const errCount = effMatrixState.sessionErrorCount;

    // ── Dashboard health badge ──
    if (badgeEl) {
        const healthPct = getEffHealthPct();
        if (!effRedDot.valid || effMatrixState.state === 0) {
            badgeEl.textContent = 'ALT ◌';
            badgeEl.className = 'alt-health-badge badge-neutral';
        } else if (effMatrixState.state !== 2) {
            badgeEl.textContent = 'ALT ?';
            badgeEl.className = 'alt-health-badge badge-neutral';
        } else if (healthPct === null) {
            badgeEl.textContent = 'ALT --';
            badgeEl.className = 'alt-health-badge badge-neutral';
        } else if (errCount === 0 && healthPct >= 95) {
            badgeEl.textContent = `ALT ${healthPct.toFixed(0)}%`;
            badgeEl.className = 'alt-health-badge badge-green';
        } else if (errCount < 3 && healthPct >= 85) {
            badgeEl.textContent = `ALT ${healthPct.toFixed(0)}%`;
            badgeEl.className = 'alt-health-badge badge-yellow';
        } else {
            badgeEl.textContent = `ALT ${healthPct !== null ? healthPct.toFixed(0) + '%' : '⚠'}`;
            badgeEl.className = 'alt-health-badge badge-red';
        }
    }

    // ── Always update health pct element and sparkline ──
    // Done here so they update even when banner is hidden
    const healthEl = document.getElementById('eff-health-pct');
    if (healthEl) {
        const pct = getEffHealthPct();
        if (pct === null) {
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
    const H = 70;
    const PAD_L = 28, PAD_R = 8, PAD_T = 8, PAD_B = 18;
    const plotW = W - PAD_L - PAD_R;
    const plotH = H - PAD_T - PAD_B;

    // Reconstruct chronological order from circular buffer
    const ordered = [];
    if (effHistory.count > 0) {
        const oldest = effHistory.count < 30
            ? 0
            : effHistory.head;
        for (let i = 0; i < effHistory.count; i++) {
            const idx = (oldest + i) % 30;
            const v = effHistory.values[idx];
            if (v > 0.1) ordered.push(v * 100);  // Convert ratio to pct
        }
    }

    // Also append live current session if meaningful
    const livePct = getEffHealthPct();
    // sessionHealthCount not directly available in JS — use effMatrixState as proxy
    // Show live point if we're in a reference bin with a valid reading
    const showLive = (effMatrixState.state === 2 && effRedDot.valid && livePct !== null);

    const Y_MIN = 70;
    const Y_MAX = 120;

    function toPixelX(i, total) {
        return PAD_L + (i / Math.max(total - 1, 1)) * plotW;
    }
    function toPixelY(pct) {
        const clamped = Math.max(Y_MIN, Math.min(Y_MAX, pct));
        return PAD_T + plotH - ((clamped - Y_MIN) / (Y_MAX - Y_MIN)) * plotH;
    }
    function colorForPct(pct) {
        if (pct >= 95) return '#4CAF50';
        if (pct >= 85) return '#FFC107';
        return '#F44336';
    }

    const allPoints = showLive ? [...ordered, livePct] : ordered;
    const nPts = allPoints.length;

    let svgLines = '';
    let svgDots = '';

    if (nPts >= 2) {
        // Draw colored line segments
        for (let i = 0; i < nPts - 1; i++) {
            const x1 = toPixelX(i, nPts);
            const x2 = toPixelX(i + 1, nPts);
            const y1 = toPixelY(allPoints[i]);
            const y2 = toPixelY(allPoints[i + 1]);
            const col = colorForPct((allPoints[i] + allPoints[i + 1]) / 2);
            svgLines += `<line x1="${x1.toFixed(1)}" y1="${y1.toFixed(1)}" `
                + `x2="${x2.toFixed(1)}" y2="${y2.toFixed(1)}" `
                + `stroke="${col}" stroke-width="2" stroke-linecap="round"/>`;
        }
    }

    // Dots for each session point
    for (let i = 0; i < nPts; i++) {
        const x = toPixelX(i, nPts);
        const y = toPixelY(allPoints[i]);
        const col = colorForPct(allPoints[i]);
        const isLiveDot = showLive && i === nPts - 1;
        svgDots += `<circle cx="${x.toFixed(1)}" cy="${y.toFixed(1)}" `
            + `r="${isLiveDot ? 4 : 3}" fill="${col}" `
            + `${isLiveDot ? 'opacity="0.7"' : ''}/>`;
    }

    // Reference line at 100%
    const refY = toPixelY(100).toFixed(1);
    const refLine = `<line x1="${PAD_L}" y1="${refY}" x2="${W - PAD_R}" y2="${refY}" `
        + `stroke="#888" stroke-width="1" stroke-dasharray="4,3" opacity="0.5"/>`;

    // Y axis labels
    const yLabels = [70, 85, 100, 115].map(v => {
        const y = toPixelY(v).toFixed(1);
        return `<text x="${PAD_L - 4}" y="${y}" text-anchor="end" `
            + `dominant-baseline="middle" font-size="9" fill="#888">${v}</text>`;
    }).join('');

    // X axis label
    const xLabel = `<text x="${PAD_L + plotW / 2}" y="${H - 3}" `
        + `text-anchor="middle" font-size="9" fill="#888">Sessions (oldest → newest)</text>`;

    // No data message
    const noData = nPts === 0
        ? `<text x="${W / 2}" y="${H / 2}" text-anchor="middle" `
        + `dominant-baseline="middle" font-size="11" fill="#666">No session history yet</text>`
        : '';

    container.innerHTML =
        `<svg width="${W}" height="${H}" xmlns="http://www.w3.org/2000/svg">`
        + refLine + svgLines + svgDots + yLabels + xLabel + noData
        + `</svg>`;
}

// ==================== THERMAL LOG PLOTS ====================
let _thermalStateArrays = {
    flagsArr: [], antiWindupArr: [], stageArr: [], tArr: []
};
let thermalLogAutoRefreshTimer = null;
let thermalLogPlots = [null, null, null];
let thermalLogResizeObservers = [null, null, null];
let thermalWindowMin = 30;

// Default visibility — hide the four requested series
let thermalSeriesVisible = {
    tempFilt: true, tempSP: true, penaltyAmps: false,
    outerP: true, outerI: false, outerD: false,
    impliedPenalty: true, outerDExternal: true,
    measAmps: false, uTarget: true
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
        `Kd: ${getEchoText('TempPIDKd_echo')}   KdExt: ${getEchoText('TempPIDKdExternal_echo')}`,
        `Margin: ${getEchoText('TempPIDMarginF_echo')}°F   Interval: ${getEchoText('TempPIDIntervalMs_echo')}ms`
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
    const statusEl = document.getElementById('thermallog-status');
    if (statusEl) statusEl.textContent = 'Fetching…';
    const scrollY = window.scrollY;
    try {
        const resp = await fetch('/thermallog.bin');
        if (!resp.ok) throw new Error(`HTTP ${resp.status}`);
        const buf = await resp.arrayBuffer();
        parseThermalBin(buf);
        if (statusEl) statusEl.textContent = `Updated ${new Date().toLocaleTimeString()}`;
    } catch (err) {
        if (statusEl) statusEl.textContent = `Fetch error: ${err.message}`;
        console.error('thermallog fetch:', err);
    }
    requestAnimationFrame(() => {
        requestAnimationFrame(() => { window.scrollTo(0, scrollY); });
    });
}

// ---------------------------------------------------------------------------
// Parse binary
// ---------------------------------------------------------------------------
function parseThermalBin(buf) {
    const view = new DataView(buf);
    const ENTRY_SIZE = 48;
    const HEADER_SIZE = 8;

    // No-data path: render plots with empty data so axes draw instead of blank space.
    let count = 0;
    let intervalMs = 1000;
    if (buf.byteLength >= 8) {
        count = view.getUint32(0, true);
        intervalMs = view.getUint32(4, true);
    }

    if (count > 0 && buf.byteLength < HEADER_SIZE + count * ENTRY_SIZE) {
        console.error('thermallog.bin: truncated response', buf.byteLength, 'need', HEADER_SIZE + count * ENTRY_SIZE);
        return;
    }

    const intervalMin = intervalMs / 60000.0;

    const t = new Array(count);
    const tempFilt = new Array(count);
    const tempSP = new Array(count);
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

        tempFilt[i] = view.getInt16(b + 4, true) / 10.0;
        tempSP[i] = view.getInt16(b + 6, true) / 10.0;
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
        outerDExt[i] = view.getInt16(b + 44, true) / 10.0;
    }

    renderThermalPlot1([t, tempFilt, tempSP, penalty, measAmps, uTarget], t[0]);
    renderThermalPlotState([t, new Array(count).fill(null)], t[0], flagsArr, antiWindup, stageArr, t);
    renderThermalPlot2([t, outerP, outerI, outerD, implied, outerDExt], t[0]);
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
                label: 'Temp Setpoint (°F)', stroke: '#f39c12', width: 1.5,
                scale: 'temp', dash: [5, 5],
                show: thermalSeriesVisible.tempSP !== false
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
        { key: 'tempFilt', label: 'Temp Filtered', color: '#e74c3c', idx: 1 },
        { key: 'tempSP', label: 'Temp Setpoint', color: '#f39c12', idx: 2 },
        { key: 'penaltyAmps', label: 'Penalty Amps', color: '#2ecc71', idx: 3 },
        { key: 'measAmps', label: 'Measured Amps', color: '#3498db', idx: 4 },
        { key: 'uTarget', label: 'U Target', color: '#9b59b6', idx: 5 }
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
            { label: 'Implied Penalty', stroke: '#2ecc71', width: 2, scale: 'amps' },
            { label: 'D External', stroke: '#1abc9c', width: 1.5, scale: 'amps', dash: [3, 3] }
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
        { key: 'outerP', label: 'Outer P', color: '#3498db', idx: 1 },
        { key: 'outerI', label: 'Outer I', color: '#e67e22', idx: 2 },
        { key: 'outerD', label: 'Outer D', color: '#9b59b6', idx: 3 },
        { key: 'impliedPenalty', label: 'Implied Penalty', color: '#2ecc71', idx: 4 },
        { key: 'outerDExternal', label: 'D External', color: '#1abc9c', idx: 5 }
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
        thermalLogPlots[3].redraw();
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
                thermalLogAutoRefreshTimer = setInterval(fetchAndRenderThermalLog, 10000);
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

    if (changedAny('FloatVoltage', 'AbsorptionVoltage') &&
        Number.isFinite(state.FloatVoltage) &&
        Number.isFinite(state.AbsorptionVoltage) &&
        state.FloatVoltage > state.AbsorptionVoltage) {
        return { valid: false, error: 'Float Voltage cannot be higher than Absorption Voltage.' };
    }

    if (changedAny('RebulkVoltage', 'FloatVoltage') &&
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
// Binary layout: 24-byte header + N × 32-byte CvLogEntry structs (little-endian).
//
// Header (24 bytes):
//   offset  0  uint32  count
//   offset  4  uint32  entrySize (= 32)
//   offset  8  float32 VoltageKp
//   offset 12  float32 VoltageKi
//   offset 16  uint32  VoltageLoopInterval (ms)
//   offset 20  uint32  reserved
//
// Entry (42 bytes):
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
//                                    b3=soft b4=hard b5=iExcess)
//   offset 29  uint8    pad
//   offset 30  int16    rpm
//   offset 32  int16    iMA2_x10    / 10   → A
//   offset 34  int16    iMA4_x10    / 10   → A
//   offset 36  int16    dIdt2_x10   / 10   → A/s
//   offset 38  int16    dIdt4_x10   / 10   → A/s
//   offset 40  int16    ch1IntervalMs       → ms
// ===========================================================================

const CV_LOG_HEADER_SIZE = 24;
const CV_LOG_ENTRY_SIZE = 40;

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
    const softClamp = new Array(count);
    const hardClamp = new Array(count);
    const rpm = new Array(count);
    const battV_filt = new Array(count);
    const iMeas_filt = new Array(count);
    const ch1Interval = new Array(count);
    const iExcess = new Array(count);

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
        softClamp[i] = (f >> 3) & 1;
        hardClamp[i] = (f >> 4) & 1;
        rpm[i] = view.getInt16(b + 30, true);
        battV_filt[i] = view.getInt16(b + 32, true) / 100.0;
        iMeas_filt[i] = view.getInt16(b + 34, true) / 10.0;
       ch1Interval[i] = view.getInt16(b + 36, true);  // correct offset for ch1IntervalMs
// pad2 at b+38 intentionally skipped
        iExcess[i] = (f >> 5) & 1;                  // move up alongside other flag bits (optional but clean)
    }

    return {
        count, voltKp, voltKi, voltInterval,
        ts, battV, targV, vError, dvdt, vPred,
        fastOvCap, cv_I, Icv, uTarget, spLimited,
        iMeas, duty, flags,
        fastOvActive, voltLoopFired, cvActive, softClamp, hardClamp,
        rpm, battV_filt, iMeas_filt, ch1Interval, iExcess,
    };
}


// ---------------------------------------------------------------------------
// cvBinToCsv(d)
// Converts a parsed cvlog object to a CSV string.
// First row is a settings comment; second row is column headers.
// ---------------------------------------------------------------------------
function cvBinToCsv(d) {
    const lines = [];

    // Settings header row — hardcoded OV constants match AdjustFieldLearnMode()
    lines.push(
        `# VoltageKp=${d.voltKp.toFixed(2)} VoltageKi=${d.voltKi.toFixed(3)}` +
        ` VoltageLoopInterval=${d.voltInterval}ms` +
        ` | FastOV constants not logged — ask firmware for values`
    );

    // Column headers
    lines.push([
        't_s',
        'battV', 'targV', 'vError_V', 'dvdt_Vs', 'vPred',
        'fastOvCap_A', 'cv_I_A', 'Icv_A', 'uTarget_A', 'spLimited_A',
        'iMeas_A', 'duty_pct',
        'fastOvActive', 'voltLoopFired', 'cvActive', 'softClamp', 'hardClamp',
        'rpm',
        'battV_filt_V', 'iMeas_filt_A', 'ch1_interval_ms', 'iExcess',
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
            d.softClamp[i], d.hardClamp[i],
            d.rpm[i],
            d.battV_filt[i].toFixed(2), d.iMeas_filt[i].toFixed(1),
            d.ch1Interval[i], d.iExcess[i],
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

    const csv = cvBinToCsv(d);
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

function startSystemIDTest() {
    if (!currentAdminPassword) {
        showSystemIDStatus('error', 'Settings must be unlocked first.');
        return;
    }

    // Pre-flight validation — catch obvious failures before wasting time
    const rpm = parseFloat(getField("RPMID")) || 0;
    const duty = parseFloat(getField("dutyCycleID")) || 0;
    if (rpm < 50) {
        showSystemIDStatus('error', 'Engine does not appear to be running (RPM = ' + rpm.toFixed(0) + '). Start engine first.');
        return;
    }
    if (duty < 5) {
        showSystemIDStatus('error', 'Field duty too low (' + duty.toFixed(1) + '%). Alternator must be active with a load.');
        return;
    }

    // Compute estimated duration from current TC so user knows what to expect
    const tcMs = parseFloat(getField("InputFilterTC_echo")) || 1000;
    const holdMs = Math.max(15 * tcMs, 5000);
    const estSecs = Math.round(7 * holdMs / 1000);

    dismissSystemIDResults();
    showSystemIDStatus('running', 'Starting… estimated duration: ~' + estSecs + 's');

    const btn = document.getElementById('systemIDStartBtn');
    if (btn) { btn.disabled = true; btn.textContent = 'Running…'; }

    const formData = new URLSearchParams();
    formData.append("password", currentAdminPassword);
    formData.append("startSystemID", "1");

    fetchWithTimeout(buildURL("/get?" + formData.toString()), {}, 8000)
        .then(() => {
            console.log("SystemID test requested");
            waitForSystemIDResults();
        })
        .catch(err => {
            console.error("SystemID request failed:", err);
            showSystemIDStatus('error', 'Failed to send start command: ' + err.message);
            resetSystemIDButton();
        });
}

function waitForSystemIDResults() {
    const pollMs = 500;
    let elapsed = 0;
    let everSawActive = false;

    // Fix: use || not ?? so parseFloat("?") → NaN falls back to default instead of using "?" as-is
    const tcMs = parseFloat(getField("InputFilterTC_echo")) || 1000;
    const holdMs = Math.max(15 * tcMs, 5000);
    const maxWaitMs = 7 * holdMs + 15000;
    const estSecs = Math.round(7 * holdMs / 1000);

    console.log("SystemID: polling | TC=" + tcMs + "ms  holdMs=" + holdMs + "  maxWait=" + (maxWaitMs / 1000).toFixed(0) + "s");

    const poll = setInterval(() => {
        elapsed += pollMs;

        // Read raw numeric value from dataset.raw (set by updateSystemIDTelemetryLabels),
        // or fall back to textContent for the initial "?" state before first CSVData3 update
        const activeEl = document.getElementById('systemIDActive_ID');
        const readyEl  = document.getElementById('systemIDResultsReady_ID');
        const active = parseInt(activeEl?.dataset?.raw ?? activeEl?.textContent ?? '0') || 0;
        const ready  = parseInt(readyEl?.dataset?.raw  ?? readyEl?.textContent  ?? '0') || 0;

        if (active === 1) everSawActive = true;

        // Keep button text and status line updated with elapsed time
        const btn = document.getElementById('systemIDStartBtn');
        if (btn) btn.textContent = 'Running… (' + (elapsed / 1000).toFixed(0) + 's)';
        showSystemIDStatus('running',
            'Test in progress — ' + (elapsed / 1000).toFixed(0) + 's elapsed (est. ~' + estSecs + 's)');

        if (elapsed > maxWaitMs) {
            clearInterval(poll);
            resetSystemIDButton();
            let msg;
            if (!everSawActive) {
                msg = 'Timed out — test never started. Check that system is in AUTO mode and alternator is active.';
            } else if (active === 1) {
                msg = 'Timed out after ' + (elapsed / 1000).toFixed(0) + 's — test still running. Check serial console.';
            } else {
                msg = 'Timed out — test finished but results never posted. Check serial console.';
            }
            showSystemIDStatus('error', msg);
            return;
        }

        if (ready !== 1) return;

        clearInterval(poll);
        resetSystemIDButton();
        showSystemIDStatus('done', 'Test complete — results below.');
        showSystemIDResults();
    }, pollMs);
}

function showSystemIDResults() {
    const r0 = parseFloat(getField("systemIDRiseDelay_0_ID") ?? '-1');
    const r1 = parseFloat(getField("systemIDRiseDelay_1_ID") ?? '-1');
    const r2 = parseFloat(getField("systemIDRiseDelay_2_ID") ?? '-1');
    const ra = parseFloat(getField("systemIDRiseAvg_ID")     ?? '-1');
    const f0 = parseFloat(getField("systemIDFallDelay_0_ID") ?? '-1');
    const f1 = parseFloat(getField("systemIDFallDelay_1_ID") ?? '-1');
    const f2 = parseFloat(getField("systemIDFallDelay_2_ID") ?? '-1');
    const fa = parseFloat(getField("systemIDFallAvg_ID")     ?? '-1');

    // Suggested TC is max of rise/fall averages; only valid if at least one is positive
    const suggestedTC = (ra > 0 || fa > 0) ? Math.max(ra, fa) : -1;

    // Compute last-two average for rise and fall (indices 1 and 2, skip -1 = not detected)
    const last2Rise = [r1, r2].filter(v => v >= 0);
    const last2Fall = [f1, f2].filter(v => v >= 0);
    const avgLast2Rise = last2Rise.length ? last2Rise.reduce((a, b) => a + b, 0) / last2Rise.length : -1;
    const avgLast2Fall = last2Fall.length ? last2Fall.reduce((a, b) => a + b, 0) / last2Fall.length : -1;
    const suggestedLast2TC = (avgLast2Rise > 0 || avgLast2Fall > 0) ? Math.max(avgLast2Rise, avgLast2Fall) : -1;

    const card = document.getElementById('systemIDResultsCard');
    if (!card) return;

    const fmt = v => v < 0 ? '<span style="color:var(--warning,#e65100)">n/d</span>' : v.toFixed(0);

    document.getElementById('sysid_r0').innerHTML = fmt(r0);
    document.getElementById('sysid_r1').innerHTML = fmt(r1);
    document.getElementById('sysid_r2').innerHTML = fmt(r2);
    document.getElementById('sysid_ra').innerHTML = fmt(ra);
    document.getElementById('sysid_f0').innerHTML = fmt(f0);
    document.getElementById('sysid_f1').innerHTML = fmt(f1);
    document.getElementById('sysid_f2').innerHTML = fmt(f2);
    document.getElementById('sysid_fa').innerHTML = fmt(fa);

    const tcEl    = document.getElementById('sysid_suggested_tc');
    const applyBtn = document.getElementById('sysid_apply_btn');
    if (suggestedTC > 0) {
        tcEl.textContent        = suggestedTC.toFixed(0) + ' ms';
        applyBtn.textContent    = 'Apply ' + suggestedTC.toFixed(0) + ' ms as Filter TC';
        applyBtn.disabled       = false;
        applyBtn.dataset.tc     = suggestedTC.toFixed(0);
    } else {
        tcEl.innerHTML          = '<span style="color:var(--warning,#e65100)">Detection failed — check step amplitude and baseline stability</span>';
        applyBtn.disabled       = true;
        applyBtn.textContent    = 'Apply (unavailable)';
    }

    // Show the last-two tip only when first step is a meaningful outlier (>5 ms difference)
    const tipEl = document.getElementById('sysid_last2_tip');
    if (tipEl) {
        if (suggestedLast2TC > 0 && Math.abs(suggestedLast2TC - suggestedTC) > 5) {
            tipEl.style.display = '';
            tipEl.textContent =
                'The first step differs noticeably from the others. If it looks noisy in the Plots tab, ' +
                'you can type in ' + suggestedLast2TC.toFixed(0) + ' ms manually instead ' +
                '(average of the last two readings: Rise ' + avgLast2Rise.toFixed(0) +
                ' ms / Fall ' + avgLast2Fall.toFixed(0) + ' ms).';
        } else {
            tipEl.style.display = 'none';
        }
    }

    card.style.display = '';
}

function applySystemIDTC() {
    const btn = document.getElementById('sysid_apply_btn');
    const tc = parseFloat(btn?.dataset?.tc);
    if (!tc || tc <= 0 || !currentAdminPassword) return;
    fetch(buildURL("/get?InputFilterTC=" + encodeURIComponent(tc) +
        "&password=" + encodeURIComponent(currentAdminPassword)))
        .then(() => showSystemIDStatus('done', 'Filter TC updated to ' + tc + ' ms — saved to flash.'))
        .catch(err => showSystemIDStatus('error', 'Failed to apply TC: ' + err.message));
}

function dismissSystemIDResults() {
    const card = document.getElementById('systemIDResultsCard');
    if (card) card.style.display = 'none';
}

function resetSystemIDButton() {
    const btn = document.getElementById('systemIDStartBtn');
    if (btn) { btn.disabled = false; btn.textContent = 'Start Test'; }
}

function showSystemIDStatus(type, msg) {
    const el = document.getElementById('systemIDStatusDiv');
    if (!el) return;
    el.style.display = '';
    el.style.color   = type === 'error'   ? 'var(--warning, #e65100)' :
                       type === 'done'    ? 'var(--green,   #2e7d32)' :
                       /* running */        'var(--text-dark)';
    el.textContent = msg;
}

function updateSystemIDTelemetryLabels(active, ready) {
    // Store numeric values in dataset.raw so the polling loop can read them
    // even after textContent is changed to human-readable labels
    const activeEl = document.getElementById('systemIDActive_ID');
    const readyEl  = document.getElementById('systemIDResultsReady_ID');
    if (activeEl) {
        activeEl.dataset.raw = active;
        activeEl.textContent = active === 1 ? 'Active' : 'Idle';
        activeEl.style.color = active === 1 ? 'var(--warning, #f57c00)' : '';
    }
    if (readyEl) {
        readyEl.dataset.raw = ready;
        readyEl.textContent = ready === 1 ? 'Yes' : 'No';
        readyEl.style.color = ready === 1 ? 'var(--green, #2e7d32)' : '';
    }
}


/* XREG_END */