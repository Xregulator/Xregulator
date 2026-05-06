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


enum Csv1Index {
  CSV1_AlternatorTemperatureF,   // 0
  CSV1_dutyCycle,                // 1
  CSV1_BatteryV,                 // 2
  CSV1_MeasuredAmps,             // 3
  CSV1_RPM,                      // 4
  CSV1_Channel3V,                // 5
  CSV1_IBV,                      // 6
  CSV1_Bcur,                     // 7
  CSV1_VictronVoltage,           // 8
  CSV1_LoopTime,                 // 9
  CSV1_WifiStrength,             // 10
  CSV1_WifiHeartBeat,            // 11
  CSV1_SendWifiTime,             // 12
  CSV1_AnalogReadTime,           // 13
  CSV1_VeTime,                   // 14
  CSV1_MaximumLoopTime,          // 15
  CSV1_HeadingNMEA,              // 16
  CSV1_vvout,                    // 17
  CSV1_iiout,                    // 18
  CSV1_FreeHeap,                 // 19
  CSV1_EngineCycles,             // 20
  CSV1_Alarm_Status,             // 21
  CSV1_fieldActiveStatus,        // 22
  CSV1_CurrentSessionDuration,   // 23
  CSV1_timeAxisModeChanging,     // 24
  CSV1_webgaugesinterval,        // 25
  CSV1_plotTimeWindow,           // 26
  CSV1_Ymin1,                    // 27
  CSV1_Ymax1,                    // 28
  CSV1_Ymin2,                    // 29
  CSV1_Ymax2,                    // 30
  CSV1_Ymin3,                    // 31
  CSV1_Ymax3,                    // 32
  CSV1_Ymin4,                    // 33
  CSV1_Ymax4,                    // 34
  CSV1_currentMode,              // 35
  CSV1_currentPartitionType,     // 36
  CSV1_stateRevision,            // 37
  CSV1_setpointLimited,          // 38
  CSV1_uTargetAmps,              // 39
  CSV1_pidInput,                 // 40
  CSV1_pidOutput,                // 41
  CSV1_pidError,                 // 42
  CSV1_imu_heel_deg,             // 43
  CSV1_imu_pitch_deg,            // 44
  CSV1_imu_vertical_accel_g,     // 45
  CSV1_imu_yaw_rate_dps,         // 46
  CSV1_imu_total_accel_g,        // 47
  CSV1_imu_hf_vibration_energy,  // 48
  CSV1_shutdownPhase,            // 49
  CSV1_g_fastOvCurrentCap,       // 50
  CSV1_g_fastOvClampCount,       // 51
  CSV1_g_fastOvSoftCount,        // 52
  CSV1_g_fastOvHardCount,        // 53
  CSV1_ch1_last_ms,              // 54
  CSV1_ch1_avg_10s,              // 55
  CSV1_ch1_worst_10s,            // 56
  CSV1_ch1_over2x_10s,           // 57
  CSV1_ch1_n_10s,                // 58
  CSV1_ch1_avg_2m,               // 59
  CSV1_ch1_worst_2m,             // 60
  CSV1_ch1_over2x_2m,            // 61
  CSV1_ch1_n_2m,                 // 62
  CSV1_ch1_avg_at,               // 63
  CSV1_ch1_worst_at,             // 64
  CSV1_ch1_over2x_at,            // 65
  CSV1_ch1_n_at,                 // 66
  CSV1_battV_filtered,           // 67
  CSV1_iMeas_filtered,           // 68
  CSV1_g_iExcessCount,           // 69
  CSV1_g_inaOVCount,             // 70
  CSV1_g_hardOCCount,            // 71
  CSV1_g_voltSpikeCount,         // 72
  CSV1_g_voltDisagreeCritCount,  // 73
  CSV1_g_voltDisagreeWarnCount,  // 74
  CSV1_g_voltImplausibleCount,   // 75
  CSV1_g_tempCritCount,          // 76
  CSV1_g_tempSustainedCount,     // 77
  CSV1_g_tempStaleCount,         // 78
  CSV1_g_currentStaleCount,      // 79
  CSV1_imu_msi_score,            // 80
  CSV1_imu_vomit_pct,            // 81
  CSV1_imu_anchorage_comfort,    // 82

  CSV1_FIELD_COUNT  // = 83

};

enum Csv2Index {
  CSV2_IBVMax,                             // 0
  CSV2_MeasuredAmpsMax,                    // 1
  CSV2_RPMMax,                             // 2
  CSV2_SOC_percent,                        // 3
  CSV2_EngineRunTime,                      // 4
  CSV2_AlternatorOnTime,                   // 5
  CSV2_AlternatorFuelUsed,                 // 6
  CSV2_ChargedEnergy,                      // 7
  CSV2_DischargedEnergy,                   // 8
  CSV2_AlternatorChargedEnergy,            // 9
  CSV2_MaxAlternatorTemperatureF,          // 10
  CSV2_temperatureThermistor,              // 11
  CSV2_MaxTemperatureThermistor,           // 12
  CSV2_VictronCurrent,                     // 13
  CSV2_timeToFullChargeMin,                // 14
  CSV2_timeToFullDischargeMin,             // 15
  CSV2_LatitudeNMEA,                       // 16
  CSV2_LongitudeNMEA,                      // 17
  CSV2_SatelliteCountNMEA,                 // 18
  CSV2_absorptionCompleteTime,             // 19
  CSV2_LastSessionDuration,                // 20
  CSV2_LastSessionMaxLoopTime,             // 21
  CSV2_lastSessionMinHeap,                 // 22
  CSV2_wifiReconnectsTotal,                // 23
  CSV2_LastResetReason,                    // 24
  CSV2_ancientResetReason,                 // 25
  CSV2_totalPowerCycles,                   // 26
  CSV2_MinFreeHeap,                        // 27
  CSV2_currentWeatherMode,                 // 28
  CSV2_UVToday,                            // 29
  CSV2_UVTomorrow,                         // 30
  CSV2_UVDay2,                             // 31
  CSV2_weatherDataValid,                   // 32
  CSV2_SolarWatts,                         // 33
  CSV2_performanceRatio,                   // 34
  CSV2_OnOff,                              // 35
  CSV2_ManualFieldToggle,                  // 36
  CSV2_HiLow,                              // 37
  CSV2_LimpHome,                           // 38
  CSV2_VeData,                             // 39
  CSV2_NMEA0183Data,                       // 40
  CSV2_NMEA2KData,                         // 41
  CSV2_AlarmActivate,                      // 42
  CSV2_TempAlarm,                          // 43
  CSV2_VoltageAlarmHigh,                   // 44
  CSV2_VoltageAlarmLow,                    // 45
  CSV2_CurrentAlarmHigh,                   // 46
  CSV2_AlarmTest,                          // 47
  CSV2_AlarmLatchEnabled,                  // 48
  CSV2_AlarmLatchState,                    // 49
  CSV2_ResetAlarmLatch,                    // 50
  CSV2_MaintainMode,                       // 51
  CSV2_ResetTemp,                          // 52
  CSV2_ResetVoltage,                       // 53
  CSV2_ResetCurrent,                       // 54
  CSV2_ResetEngineRunTime,                 // 55
  CSV2_ResetAlternatorOnTime,              // 56
  CSV2_ResetEnergy,                        // 57
  CSV2_ManualSOCPoint,                     // 58
  CSV2_LearningMode,                       // 59
  CSV2_LearningPaused,                     // 60
  CSV2_IgnoreLearningDuringPenalty,        // 61
  CSV2_ShowLearningDebugMessages,          // 62
  CSV2_LogAllLearningEvents,               // 63
  CSV2_CloudFeatures,                      // 64
  CSV2_LearningDryRunMode,                 // 65
  CSV2_AutoSaveLearningTable,              // 66
  CSV2_ResetLearningTable,                 // 67
  CSV2_ClearOverheatHistory,               // 68
  CSV2_AutoShuntGainCorrection,            // 69
  CSV2_DynamicShuntGainFactor,             // 70
  CSV2_AutoAltCurrentZero,                 // 71
  CSV2_DynamicAltCurrentZero,              // 72
  CSV2_InsulationLifePercent,              // 73
  CSV2_GreaseLifePercent,                  // 74
  CSV2_BrushLifePercent,                   // 75
  CSV2_PredictedLifeHours,                 // 76
  CSV2_LifeIndicatorColor,                 // 77
  CSV2_WindingTempOffset,                  // 78
  CSV2_ManualLifePercentage,               // 79
  CSV2_UVThresholdHigh,                    // 80
  CSV2_weatherModeEnabled,                 // 81
  CSV2_pKwHrToday,                         // 82
  CSV2_pKwHrTomorrow,                      // 83
  CSV2_pKwHr2days,                         // 84
  CSV2_ambientTemp,                        // 85
  CSV2_baroPressure,                       // 86
  CSV2_firmwareVersionInt,                 // 87
  CSV2_deviceIdUpper,                      // 88
  CSV2_deviceIdLower,                      // 89
  CSV2_ChargedEnergy_AllTime,              // 90
  CSV2_AlternatorFuelUsed_AllTime,         // 91
  CSV2_PeakVoltage_AllTime,                // 92
  CSV2_EngineRunTime_AllTime,              // 93
  CSV2_MinVoltage,                         // 94
  CSV2_MinVoltage_AllTime,                 // 95
  CSV2_ChargeCycles,                       // 96
  CSV2_ChargeCycles_AllTime,               // 97
  CSV2_EngineFuelUsed,                     // 98
  CSV2_EngineFuelUsed_AllTime,             // 99
  CSV2_TotalDistance,                      // 100
  CSV2_TotalDistance_AllTime,              // 101
  CSV2_MaxSpeed,                           // 102
  CSV2_MaxSpeed_AllTime,                   // 103
  CSV2_SolarChargedEnergy,                 // 104
  CSV2_SolarChargedEnergy_AllTime,         // 105
  CSV2_AlternatorChargedEnergy_AllTime,    // 106
  CSV2_DischargedEnergy_AllTime,           // 107
  CSV2_AvgSOC_AllTime,                     // 108
  CSV2_AvgSpeed_AllTime,                   // 109
  CSV2_AvgSpeed,                           // 110
  CSV2_AlternatorOnTime_AllTime,           // 111
  CSV2_EngineCycles_AllTime,               // 112
  CSV2_MaxAlternatorTemperatureF_AllTime,  // 113
  CSV2_MaxTemperatureThermistor_AllTime,   // 114
  CSV2_MeasuredAmpsMax_AllTime,            // 115
  CSV2_RPMMax_AllTime,                     // 116
  CSV2_Ignition,                           // 117
  CSV2_BulkStage,                          // 118
  CSV2_WifiWakeSecondsRemaining,           // 119
  CSV2_BufferedRecordCount,                // 120
  CSV2_BufferedRecordPercent,              // 121
  CSV2_MAX_BUFFERED_RECORDS,               // 122
  CSV2_COGNMEA,                            // 123
  CSV2_SOGNMEA,                            // 124
  CSV2_ApparentWindSpeedNMEA,              // 125
  CSV2_ApparentWindAngleNMEA,              // 126
  CSV2_TrueWindSpeedNMEA,                  // 127
  CSV2_TrueWindAngleNMEA,                  // 128
  CSV2_LeewayNMEA,                         // 129
  CSV2_VMGNMEA,                            // 130
  CSV2_VMGTargetBearing,                   // 131
  CSV2_VMGUseTrueWind,                     // 132
  CSV2_SENSOR_UPLOAD_INTERVAL,             // 133
  CSV2_cpuLoadCore0,                       // 134
  CSV2_cpuLoadCore0Max,                    // 135
  CSV2_cpuLoadCore1,                       // 136
  CSV2_cpuLoadCore1Max,                    // 137
  CSV2_hasForcedUpdate,                    // 138
  CSV2_forcedFwVersionInt,                 // 139
  CSV2_forcedUpdateDeadline,               // 140
  CSV2_stateRevision,                      // 141
  CSV2_hardwarePresent,                    // 142
  CSV2_imu_accel_x_raw,                    // 143
  CSV2_imu_accel_y_raw,                    // 144
  CSV2_imu_accel_z_raw,                    // 145
  CSV2_imu_gyro_x_raw,                     // 146
  CSV2_imu_gyro_y_raw,                     // 147
  CSV2_imu_gyro_z_raw,                     // 148
  CSV2_accel_x_min,                        // 149
  CSV2_accel_x_max,                        // 150
  CSV2_accel_x_avg,                        // 151
  CSV2_accel_y_min,                        // 152
  CSV2_accel_y_max,                        // 153
  CSV2_accel_y_avg,                        // 154
  CSV2_accel_z_min,                        // 155
  CSV2_accel_z_max,                        // 156
  CSV2_accel_z_avg,                        // 157
  CSV2_gyro_x_min,                         // 158
  CSV2_gyro_x_max,                         // 159
  CSV2_gyro_x_avg,                         // 160
  CSV2_gyro_y_min,                         // 161
  CSV2_gyro_y_max,                         // 162
  CSV2_gyro_y_avg,                         // 163
  CSV2_gyro_z_min,                         // 164
  CSV2_gyro_z_max,                         // 165
  CSV2_gyro_z_avg,                         // 166
  CSV2_heel_min,                           // 167
  CSV2_heel_max,                           // 168
  CSV2_heel_avg,                           // 169
  CSV2_pitch_min,                          // 170
  CSV2_pitch_max,                          // 171
  CSV2_pitch_avg,                          // 172
  CSV2_vertical_accel_min,                 // 173
  CSV2_vertical_accel_max,                 // 174
  CSV2_vertical_accel_avg,                 // 175
  CSV2_total_accel_min,                    // 176
  CSV2_total_accel_max,                    // 177
  CSV2_total_accel_avg,                    // 178
  CSV2_imu_slam_count,                     // 179
  CSV2_imu_slam_peak_max,                  // 180
  CSV2_imu_slam_count_lifetime,            // 181
  CSV2_imu_capsize_count,                  // 182
  CSV2_imu_pitchpole_count,                // 183
  CSV2_imu_heel_change_60s,                // 184
  CSV2_imu_heel_deviation_60s,             // 185
  CSV2_imu_pitch_change_60s,               // 186
  CSV2_imu_pitch_deviation_60s,            // 187
  CSV2_imu_wave_period_sec,                // 188
  CSV2_imu_heel_max_lifetime,              // 189
  CSV2_imu_pitch_max_lifetime,             // 190
  CSV2_imu_slam_peak_lifetime,             // 191
  CSV2_imuEnabled,                         // 192
  CSV2_imuMountOrientation,                // 193
  CSV2_imu_fifo_overrun_count,             // 194
  CSV2_imu_i2c_error_count,                // 195
  CSV2_imu_unknown_tag_count,              // 196
  CSV2_imu_accel_dropped,                  // 197
  CSV2_imu_gyro_dropped,                   // 198
  CSV2_imu_total_samples_accel,            // 199
  CSV2_imu_total_samples_gyro,             // 200
  CSV2_IMUReadTime2,                       // 201
  CSV2_IMUReadTime,                        // 202
  CSV2_adsI2CErrorCount,                   // 203
  CSV2_tempPIDActive,                      // 204
  CSV2_tempPIDInput_d,                     // 205
  CSV2_tempPIDSetpoint_d,                  // 206
  CSV2_thermalPenaltyAmps,                 // 207
  CSV2_innerTermP,                         // 208
  CSV2_innerTermI,                         // 209
  CSV2_innerTermD,                         // 210
  CSV2_outerTermP,                         // 211
  CSV2_outerTermI,                         // 212
  CSV2_outerTermD,                         // 213
  CSV2_thermalSlopeFPerSec,                // 214
  CSV2_AbsorptionVoltage,                  // 215
  CSV2_AbsorptionTimeoutMs,                // 216
  CSV2_bulkVoltageHoldMs,                  // 217
  CSV2_chargeStageDisplay,                 // 218
  CSV2_voltageControlActive,               // 219
  CSV2_voltageTarget,                      // 220
  CSV2_voltageError,                       // 221
  CSV2_Icv,                                // 222
  CSV2_cv_I,                               // 223
  CSV2_capLimitMode,                       // 224
  CSV2_TargetVoltageMode,                  // 225
  CSV2_TargetVoltageSetpoint,              // 226
  CSV2_RebulkCurrent_A,                    // 227
  CSV2_UseFloat,                           // 228
  CSV2_inIdleStage,                        // 229
  CSV2_referenceFinalized,                 // 230
  CSV2_sessionErrorCount,                  // 231
  CSV2_anomalyMarginAmps,                  // 232
  CSV2_anomalyAlarmThreshold,              // 233
  CSV2_anomalyAlarmEnable,                 // 234
  CSV2_degradationThreshold,               // 235
  CSV2_ft_rai_total_win,                   // 236
  CSV2_ft_rai_total_ses,                   // 237
  CSV2_ft_rai_ina228_win,                  // 238
  CSV2_ft_rai_ina228_ses,                  // 239
  CSV2_ft_rai_ads_state_win,               // 240
  CSV2_ft_rai_ads_state_ses,               // 241
  CSV2_ft_rai_bmp_state_win,               // 242
  CSV2_ft_rai_bmp_state_ses,               // 243
  CSV2_ft_rai_imu_win,                     // 244
  CSV2_ft_rai_imu_ses,                     // 245
  CSV2_fsWriteQueueDrops,                  // 246
  CSV2_TempAlarmLow,                       // 247
  CSV2_cv_D,                               // 248 — D term contribution: VoltageKd × dV/dt (amps, ×100)
  CSV2_tempReadFailCount,                  // 249
  CSV2_tempCrcFailCount,                   // 250
  CSV2_tempCrcRecoveredCount,              // 251
  CSV2_tempAllFFCount,                     // 252
  CSV2_tempPowerOn85Count,                 // 253
  CSV2_tempOutOfRangeCount,               // 254
  CSV2_tempRequestFailCount,               // 255
  CSV2_tempConnectedFailCount,             // 256
  CSV2_tempResolutionFixCount,             // 257
  CSV2_tempRereadFailCount,                // 258
  CSV2_tempResolutionFixCrcFailCount,      // 259
  CSV2_tempEnumerateFailCount,             // 260
  CSV2_warmupCeiling,                      // 261
  CSV2_imu_min_moving_gentle,              // 262
  CSV2_imu_min_moving_moderate,            // 263
  CSV2_imu_min_moving_rough,               // 264
  CSV2_imu_min_moving_extreme,             // 265
  CSV2_imu_min_stat_gentle,                // 266
  CSV2_imu_min_stat_moderate,              // 267
  CSV2_imu_min_stat_rough,                 // 268
  CSV2_imu_min_stat_extreme,               // 269
  CSV2_imu_heel_deviation_120s,            // 270
  CSV2_imu_pitch_deviation_120s,           // 271
  CSV2_imu_heading_swing_120s,             // 272

  CSV2_loadDumpDtThresh,                   // 273
  CSV2_loadDumpCurrentDrop,                // 274
  CSV2_dBcur_dt,                           // 275
  CSV2_loadDumpActive,                     // 276
  CSV2_CVTuningMode,                       // 277
  CSV2_cvWaveAmplitudeV,                   // 278 — ×100
  CSV2_cvWavePeriodSec,                    // 279
  CSV2_cvKOvershoot,                       // 280 — ×10
  CSV2_cvConsecutiveReads,                 // 281

  CSV2_ThermalTuningMode,                  // 282
  CSV2_thermalWaveLowF,                    // 283 — ×10
  CSV2_thermalWaveHighF,                   // 284 — ×10
  CSV2_thermalWaveHalfPeriodMin,           // 285 — ×10
  CSV2_thermalKOvershoot,                  // 286 — ×100
  CSV2_thermalKUndershoot,                 // 287 — ×100
  CSV2_thermalSettleThreshF,               // 288 — ×10
  CSV2_thermalConsecutiveReads,            // 289
  CSV2_thermalLiveScore0,                  // 290 — ×10000, 10-min window
  CSV2_thermalLiveScore1,                  // 291 — ×10000, 1-hr window
  CSV2_thermalLiveScore2,                  // 292 — ×10000, 10-hr window
  CSV2_thermalLiveScore3,                  // 293 — ×10000, 100-hr window
  CSV2_thermalTuningTestPhase,             // 294 — 0=off/ring-in 1=scored active

  CSV2_FIELD_COUNT  // ← always last, = 295
};

enum Csv3Index {
  CSV3_TemperatureLimitF,             // 0
  CSV3_BulkVoltage,                   // 1
  CSV3_wavePeriod,                    // 2
  CSV3_FloatVoltage,                  // 3
  CSV3_SwitchingFrequency,            // 4
  CSV3_yyMin,                         // 5
  CSV3_FieldAdjustmentInterval,       // 6
  CSV3_ManualDutyTarget,              // 7
  CSV3_SwitchControlOverride,         // 8
  CSV3_waveAmplitude,                 // 9
  CSV3_CurrentThreshold,              // 10
  CSV3_PeukertExponent_scaled,        // 11
  CSV3_ChargeEfficiency_scaled,       // 12
  CSV3_ChargedVoltage_Scaled,         // 13
  CSV3_TailCurrent,                   // 14
  CSV3_ChargedDetectionTime,          // 15
  CSV3_IgnoreTemperature,             // 16
  CSV3_bmsLogic,                      // 17
  CSV3_bmsLogicLevelOff,              // 18
  CSV3_FourWay,                       // 19
  CSV3_RPMScalingFactor,              // 20
  CSV3_MaximumAllowedBatteryAmps,     // 21
  CSV3_BatteryVoltageSource,          // 22
  CSV3_LearningUpwardEnabled,         // 23
  CSV3_LearningDownwardEnabled,       // 24
  CSV3_AlternatorNominalAmps,         // 25
  CSV3_LearningUpStep,                // 26
  CSV3_LearningDownStep,              // 27
  CSV3_AmbientTempCorrectionFactor,   // 28
  CSV3_xTime,                         // 29
  CSV3_MinLearningInterval,           // 30
  CSV3_SafeOperationThreshold,        // 31
  CSV3_PidKp,                         // 32
  CSV3_PidKi,                         // 33
  CSV3_PidKd,                         // 34
  CSV3_PidSampleDivisor,              // 35
  CSV3_MaxTableValue,                 // 36
  CSV3_MinTableValue,                 // 37
  CSV3_MaxPenaltyPercent,             // 38
  CSV3_MaxPenaltyDuration,            // 39
  CSV3_NeighborLearningFactor,        // 40
  CSV3_yyMax,                         // 41
  CSV3_LearningMemoryDuration,        // 42
  CSV3_EnableNeighborLearning,        // 43
  CSV3_EnableAmbientCorrection,       // 44
  CSV3_TuningMode,                    // 45
  CSV3_LearningTableSaveInterval,     // 46
  CSV3_rpmCurrentTable_0,             // 47
  CSV3_rpmCurrentTable_1,             // 48
  CSV3_rpmCurrentTable_2,             // 49
  CSV3_rpmCurrentTable_3,             // 50
  CSV3_rpmCurrentTable_4,             // 51
  CSV3_rpmCurrentTable_5,             // 52
  CSV3_rpmCurrentTable_6,             // 53
  CSV3_rpmCurrentTable_7,             // 54
  CSV3_rpmCurrentTable_8,             // 55
  CSV3_rpmCurrentTable_9,             // 56
  CSV3_currentRPMTableIndex,          // 57
  CSV3_pidInitialized,                // 58
  CSV3_ShuntResistanceMicroOhm,       // 59
  CSV3_InvertAltAmps,                 // 60
  CSV3_InvertBattAmps,                // 61
  CSV3_MaxDuty,                       // 62
  CSV3_MinDuty,                       // 63
  CSV3_FieldResistance,               // 64
  CSV3_maxPoints,                     // 65
  CSV3_AlternatorCOffset,             // 66
  CSV3_BatteryCOffset,                // 67
  CSV3_BatteryCapacity_Ah,            // 68
  CSV3_AmpSensorRange,                // 69
  CSV3_R_fixed,                       // 70
  CSV3_Beta,                          // 71
  CSV3_T0_C,                          // 72
  CSV3_TempSource,                    // 73
  CSV3_IgnitionOverride,              // 74
  CSV3_FLOAT_DURATION,                // 75
  CSV3_PulleyRatio,                   // 76
  CSV3_BatteryCurrentSource,          // 77
  CSV3_overheatCount_0,               // 78
  CSV3_overheatCount_1,               // 79
  CSV3_overheatCount_2,               // 80
  CSV3_overheatCount_3,               // 81
  CSV3_overheatCount_4,               // 82
  CSV3_overheatCount_5,               // 83
  CSV3_overheatCount_6,               // 84
  CSV3_overheatCount_7,               // 85
  CSV3_overheatCount_8,               // 86
  CSV3_overheatCount_9,               // 87
  CSV3_cumulativeNoOverheatTime_0,    // 88
  CSV3_cumulativeNoOverheatTime_1,    // 89
  CSV3_cumulativeNoOverheatTime_2,    // 90
  CSV3_cumulativeNoOverheatTime_3,    // 91
  CSV3_cumulativeNoOverheatTime_4,    // 92
  CSV3_cumulativeNoOverheatTime_5,    // 93
  CSV3_cumulativeNoOverheatTime_6,    // 94
  CSV3_cumulativeNoOverheatTime_7,    // 95
  CSV3_cumulativeNoOverheatTime_8,    // 96
  CSV3_cumulativeNoOverheatTime_9,    // 97
  CSV3_totalLearningEvents,           // 98
  CSV3_totalOverheats,                // 99
  CSV3_totalSafeHours,                // 100
  CSV3_averageTableValue,             // 101
  CSV3_timeSinceLastOverheat,         // 102
  CSV3_learningTargetFromRPM,         // 103
  CSV3_ambientTempCorrection,         // 104
  CSV3_finalLearningTarget,           // 105
  CSV3_overheatingPenaltyTimer,       // 106
  CSV3_overheatingPenaltyAmps,        // 107
  CSV3_pidSetpoint,                   // 108
  CSV3_TempToUse,                     // 109
  CSV3_rpmTableRPMPoints_0,           // 110
  CSV3_rpmTableRPMPoints_1,           // 111
  CSV3_rpmTableRPMPoints_2,           // 112
  CSV3_rpmTableRPMPoints_3,           // 113
  CSV3_rpmTableRPMPoints_4,           // 114
  CSV3_rpmTableRPMPoints_5,           // 115
  CSV3_rpmTableRPMPoints_6,           // 116
  CSV3_rpmTableRPMPoints_7,           // 117
  CSV3_rpmTableRPMPoints_8,           // 118
  CSV3_rpmTableRPMPoints_9,           // 119
  CSV3_LearningSettlingPeriod,        // 120
  CSV3_LearningRPMChangeThreshold,    // 121
  CSV3_LearningTempHysteresis,        // 122
  CSV3_fuelTableRPM_0,                // 123
  CSV3_fuelTableRPM_1,                // 124
  CSV3_fuelTableRPM_2,                // 125
  CSV3_fuelTableRPM_3,                // 126
  CSV3_fuelTableRPM_4,                // 127
  CSV3_fuelTableRPM_5,                // 128
  CSV3_fuelTableRPM_6,                // 129
  CSV3_fuelTableRPM_7,                // 130
  CSV3_fuelTableRPM_8,                // 131
  CSV3_fuelTableRPM_9,                // 132
  CSV3_fuelTableGPH_0,                // 133
  CSV3_fuelTableGPH_1,                // 134
  CSV3_fuelTableGPH_2,                // 135
  CSV3_fuelTableGPH_3,                // 136
  CSV3_fuelTableGPH_4,                // 137
  CSV3_fuelTableGPH_5,                // 138
  CSV3_fuelTableGPH_6,                // 139
  CSV3_fuelTableGPH_7,                // 140
  CSV3_fuelTableGPH_8,                // 141
  CSV3_fuelTableGPH_9,                // 142
  CSV3_stateRevision,                 // 143
  CSV3_SetpointRampRate,              // 144
  CSV3_DutyRampRate,                  // 145
  CSV3_SettleTimeBeforeCut,           // 146
  CSV3_TempWarnExcess,                // 147
  CSV3_TempCritExcess,                // 148
  CSV3_TempSustainedTimeout,          // 149
  CSV3_VoltageSpikeMargin,            // 150
  CSV3_VoltageDisagreeThreshold,      // 151
  CSV3_VoltageDisagreeTimeout,        // 152
  CSV3_rpmMinDutyTable_0,             // 153
  CSV3_rpmMinDutyTable_1,             // 154
  CSV3_rpmMinDutyTable_2,             // 155
  CSV3_rpmMinDutyTable_3,             // 156
  CSV3_rpmMinDutyTable_4,             // 157
  CSV3_rpmMinDutyTable_5,             // 158
  CSV3_rpmMinDutyTable_6,             // 159
  CSV3_rpmMinDutyTable_7,             // 160
  CSV3_rpmMinDutyTable_8,             // 161
  CSV3_rpmMinDutyTable_9,             // 162
  CSV3_rpmCapCurrentTable_0,          // 163
  CSV3_rpmCapCurrentTable_1,          // 164
  CSV3_rpmCapCurrentTable_2,          // 165
  CSV3_rpmCapCurrentTable_3,          // 166
  CSV3_rpmCapCurrentTable_4,          // 167
  CSV3_rpmCapCurrentTable_5,          // 168
  CSV3_rpmCapCurrentTable_6,          // 169
  CSV3_rpmCapCurrentTable_7,          // 170
  CSV3_rpmCapCurrentTable_8,          // 171
  CSV3_rpmCapCurrentTable_9,          // 172
  CSV3_VoltageKp,                     // 173
  CSV3_VoltageLoopInterval,           // 174
  CSV3_FIELD_COLLAPSE_DELAY,          // 175
  CSV3_SetpointRiseRate,              // 176
  CSV3_SetpointFallRate,              // 177
  CSV3_PIDTrackingGain,               // 178
  CSV3_CAPSIZE_THRESHOLD_DEG,         // 179
  CSV3_PITCHPOLE_THRESHOLD_DEG,       // 180
  CSV3_SLAM_THRESHOLD_G,              // 181
  CSV3_imuMountOrientation,           // 182
  CSV3_socInfoAvailable,              // 183
  CSV3_TailCurrent_A,                 // 184
  CSV3_RebulkVoltage,                 // 185
  CSV3_rebulkDebounceTime,            // 186
  CSV3_MinFloatTime,                  // 187
  CSV3_SOC_BlockRebulk_percent,       // 188
  CSV3_SOC_AllowRebulk_percent,       // 189
  CSV3_accelEnabled,                  // 190
  CSV3_DutySlowRampRate,              // 191
  CSV3_ShutdownPhase2HoldMs,          // 192
  CSV3_learningUpCount_0,             // 193
  CSV3_learningUpCount_1,             // 194
  CSV3_learningUpCount_2,             // 195
  CSV3_learningUpCount_3,             // 196
  CSV3_learningUpCount_4,             // 197
  CSV3_learningUpCount_5,             // 198
  CSV3_learningUpCount_6,             // 199
  CSV3_learningUpCount_7,             // 200
  CSV3_learningUpCount_8,             // 201
  CSV3_learningUpCount_9,             // 202
  CSV3_TempPIDKp,                     // 203
  CSV3_TempPIDKi,                     // 204
  CSV3_UNUSED_205,                    // 205 (was TempPIDKd, removed)
  CSV3_ThermalLookaheadSec,          // 206 (was TempPIDMarginF — same conceptual slot)
  CSV3_TempPIDIntervalMs,             // 207
  CSV3_TempPIDFilterAlpha,            // 208
  CSV3_UNUSED_209,                    // 209 (was TempPIDStaleMs, removed)
  CSV3_UNUSED_210,                    // 210 (was TempPIDAntiWindupMarginA, removed)
  CSV3_FreeInternalRam,               // 211
  CSV3_TotalInternalRam,              // 212
  CSV3_LargestInternalBlock,          // 213
  CSV3_FreePSRAM,                     // 214
  CSV3_TotalPSRAM,                    // 215
  CSV3_Heapfrag,                      // 216
  CSV3_UNUSED_217,                    // 217 (was TempPIDKdExternal, removed)
  CSV3_VoltageKi,                     // 218
  CSV3_rpmCapPowerTable_0,            // 219
  CSV3_rpmCapPowerTable_1,            // 220
  CSV3_rpmCapPowerTable_2,            // 221
  CSV3_rpmCapPowerTable_3,            // 222
  CSV3_rpmCapPowerTable_4,            // 223
  CSV3_rpmCapPowerTable_5,            // 224
  CSV3_rpmCapPowerTable_6,            // 225
  CSV3_rpmCapPowerTable_7,            // 226
  CSV3_rpmCapPowerTable_8,            // 227
  CSV3_rpmCapPowerTable_9,            // 228
  CSV3_VoltageTrimLimit,              // 229
  CSV3_ft_ReadAnalogInputs_win,       // 230
  CSV3_ft_ReadAnalogInputs_ses,       // 231
  CSV3_ft_AdjustFieldLearnMode_win,   // 232
  CSV3_ft_AdjustFieldLearnMode_ses,   // 233
  CSV3_ft_uploadSensorHistory_win,    // 234
  CSV3_ft_uploadSensorHistory_ses,    // 235
  CSV3_ft_uploadBufferedRecords_win,  // 236
  CSV3_ft_uploadBufferedRecords_ses,  // 237
  CSV3_ft_buildConfigPayload_win,     // 238
  CSV3_ft_buildConfigPayload_ses,     // 239
  CSV3_VeTime2,                       // 240
  CSV3_systemIDActive,                // 241 OBSOLETE — HTML removed, firmware sends 0
  CSV3_systemIDResultsReady,          // 242 OBSOLETE — HTML removed, firmware sends 0
  CSV3_systemIDRiseDelay_0,           // 243
  CSV3_systemIDRiseDelay_1,           // 244
  CSV3_systemIDRiseDelay_2,           // 245
  CSV3_systemIDFallDelay_0,           // 246
  CSV3_systemIDFallDelay_1,           // 247
  CSV3_systemIDFallDelay_2,           // 248
  CSV3_systemIDRiseAvg,               // 249
  CSV3_systemIDFallAvg,               // 250
  CSV3_InputFilterTC,                 // 251
  CSV3_SystemIDStepAmplitude,         // 252
  CSV3_HardOCTripAmps,                // 253
  CSV3_HardOCDebounceMs,              // 254
  CSV3_IExcessK,                      // 255
  CSV3_IExcessN,                      // 256
  CSV3_IExcessKBleed,                 // 257
  CSV3_IgnoreRPM,                     // 258
  CSV3_MinRPMForField,                // 259
  CSV3_UNUSED_260,                    // 260 (was ThermalTimeConstantSec, removed)
  CSV3_AwBleedRate,                   // 261
  CSV3_AwRecoverRate,                 // 262
  CSV3_KSoft,                         // 263
  CSV3_KHard,                         // 264
  CSV3_IExcessReseedFrac,             // 265
  CSV3_AwSeedProtectMs,               // 266
  CSV3_VoltageKd,                     // 267
  CSV3_UNUSED_268,                    // 268 (was ThermistorFilterAlpha, removed)
  CSV3_displayTempUnit,               // 269
  CSV3_WarmupRampRate,                // 270
  CSV3_nvsPhase,                      // 271 — NVS drain phase (0=idle, 1-8=writing, 9=commit)
  CSV3_ft_saveNVSData_win,               // 272 — saveNVSData worst 5s window (µs)
  CSV3_ft_saveNVSData_ses,               // 273 — saveNVSData worst session (µs)
  CSV3_nvsCycleMs,                       // 274 — last full NVS drain duration (ms)
  CSV3_ft_FlushFileWriteQueue_win,       // 275 — FlushFileWriteQueue worst 5s window (µs)
  CSV3_ft_FlushFileWriteQueue_ses,       // 276 — FlushFileWriteQueue worst session (µs)

  CSV3_FIELD_COUNT  // = 277
};


enum TsIndex {
  TS_HeadingNMEA,     // 0
  TS_LatitudeNMEA,    // 1
  TS_LongitudeNMEA,   // 2
  TS_SatelliteCount,  // 3
  TS_VictronVoltage,  // 4
  TS_VictronCurrent,  // 5
  TS_AlternatorTemp,  // 6
  TS_ThermistorTemp,  // 7
  TS_RPM,             // 8
  TS_MeasuredAmps,    // 9
  TS_BatteryV,        // 10
  TS_IBV,             // 11
  TS_Bcur,            // 12
  TS_Channel3V,       // 13
  TS_DutyCycle,       // 14
  TS_FieldVolts,      // 15
  TS_FieldAmps,       // 16
  TS_CogNMEA,         // 17
  TS_SogNMEA,         // 18
  TS_AppWindSpeed,    // 19
  TS_AppWindAngle,    // 20
  TS_TrueWindSpeed,   // 21
  TS_TrueWindAngle,   // 22
  TS_Leeway,          // 23
  TS_VMG,             // 24
  TS_BaroPressure,    // 25
  TS_AmbientTemp,     // 26
  TS_IMU,             // 27

  TS_FIELD_COUNT  // = 28
};


// Cap current table functions
float getCapCurrentForRPM(float rpm);
void saveCapCurrentTableToNVS();
void loadCapCurrentTableFromNVS();
void loadCapTablesForMode(int mode);

int SafeInt(float f, int scale = 1) {
  // where this is matters!!   Put utility functions like SafeInt() above setup() and loop() , according to ChatGPT.  And I proved it matters.
  return isnan(f) || isinf(f) ? -1 : (int)roundf(f * scale);
}
void loadAPCredentials(bool forceDefaults = false) {
  if (forceDefaults) {
    esp32_ap_ssid = "ALTERNATOR_WIFI";
    esp32_ap_password = "alternator123";
    Serial.println("Using default AP credentials for password recovery or first boot");
    return;
  }

  // Load custom credentials from files
  if (fsExists(AP_SSID_FILE)) {
    esp32_ap_ssid = readFile(LittleFS, AP_SSID_FILE);
    esp32_ap_ssid.trim();
    if (esp32_ap_ssid.length() == 0) {
      esp32_ap_ssid = "ALTERNATOR_WIFI";
    }
  } else {
    esp32_ap_ssid = "ALTERNATOR_WIFI";
  }

  if (fsExists(AP_PASSWORD_FILE)) {
    esp32_ap_password = readFile(LittleFS, AP_PASSWORD_FILE);
    esp32_ap_password.trim();
    if (esp32_ap_password.length() == 0) {
      esp32_ap_password = "alternator123";
    }
  } else {
    esp32_ap_password = "alternator123";
  }

  Serial.println("Loaded AP credentials: " + esp32_ap_ssid + " / [" + String(esp32_ap_password.length()) + " chars]");
}

void setupWiFi() {  // Function to set up WiFi with new GPIO-based mode selection
  Serial.println("\n=== WiFi Setup Starting ===");

  // Configuration Options Summary - GPIO Boot Mode Selection
  // GPIO41 - Boot from Factory Partition (emergency recovery from bad OTA)
  // GPIO45 - Force Configuration Mode (WiFi setup/password recovery - alternator disabled for safety)
  // GPIO46 - Select AP vs Client Mode:
  //   - LOW  = AP Mode (creates own WiFi network, full alternator operation, emergency access without credentials)
  //   - HIGH = Client Mode (connects to ship's WiFi network, full alternator operation)
  //
  // EMERGENCY ACCESS WITHOUT CREDENTIALS:
  //   Hold GPIO46 LOW during boot → AP mode → Full alternator functionality
  //   Connect to "ALTERNATOR_WIFI" (or custom SSID) with password "alternator123" (or custom)
  //   Access full interface at http://192.168.4.1
  //
  // WHY CONFIG MODE DISABLES ALTERNATOR:
  //   CONFIG mode (GPIO45 LOW or no credentials) intentionally prevents alternator operation
  //   This is a safety feature - prevents running with unconfigured/unknown settings
  //   Use GPIO46 LOW (AP mode) for emergency operation without reconfiguring

  // GPIO45 = Configuration Mode Override (always checked first)
  pinMode(45, INPUT_PULLUP);
  bool forceConfigMode = (digitalRead(45) == LOW);

  if (forceConfigMode) {
    Serial.println("=== GPIO45 LOW: FORCED CONFIGURATION MODE ===");
    Serial.println("=== ALTERNATOR DISABLED FOR SAFETY - Use GPIO46 LOW for emergency operation ===");
    loadAPCredentials(true);  // Force defaults for password recovery
    setupAccessPoint();
    setupWiFiConfigServer();  // Always serve config interface when GPIO45 is low.  This can be used to reset lost passwords
    currentMode = MODE_CONFIG;
    return;  // Exit setupWiFi() - alternator will not run
  }

  // GPIO46 = Operational Mode Selection (checked early to allow emergency AP access on first boot)
  // This check happens before isFirstBoot to ensure AP mode works even on first boot
  // Rationale: If user has no WiFi or WiFi router is down, they can still run the regulator in AP mode
  // This allows emergency operation: GPIO41 low = factory firmware, GPIO41 high = OTA firmware (if valid OTA exists, else factory)
  // GPIO46 low = AP mode regardless of any credentials of any kind
  // Settings persist in userdata partition; AP credentials default to ALTERNATOR_WIFI/alternator123

  pinMode(46, INPUT_PULLUP);
  bool requestAPMode = (digitalRead(46) == LOW);

  if (requestAPMode) {
    Serial.println("=== GPIO46 LOW: OPERATIONAL AP MODE ===");
    loadAPCredentials(false);  // Load custom credentials for normal AP operation
    setupAccessPoint();
    currentMode = MODE_AP;
    CloudFeatures = 0;
    // Serve full alternator interface (operational AP mode)
    if (webFS.exists("/index.html.gz")) {
      Serial.println("Serving full alternator interface in AP mode");
      setupServer();
    } else {
      Serial.println("No index.html.gz found - serving basic landing page");
      setupCaptivePortalLanding();
    }
    return;
  }
  // Check if this is truly first boot (no configuration has ever been done)
  bool isFirstBoot = !fsExists("/first_config_done.txt");

  if (isFirstBoot) {
    Serial.println("=== FIRST BOOT DETECTED: FORCING CONFIGURATION MODE ===");
    Serial.println("User must configure credentials before accessing interface");
    loadAPCredentials(true);  // Use defaults so user can learn them
    setupAccessPoint();
    setupWiFiConfigServer();
    currentMode = MODE_CONFIG;
    return;
  }

  // Check if the device has ever been configured with WiFi credentials before
  bool hasClientConfig = fsExists(WIFI_SSID_FILE);

  // Load and cache WiFi client credentials once
  if (hasClientConfig) {
    String ssid = readFile(LittleFS, WIFI_SSID_FILE);
    String pass = readFile(LittleFS, WIFI_PASS_FILE);
    ssid.trim();
    pass.trim();

    strncpy(cached_wifi_ssid, ssid.c_str(), sizeof(cached_wifi_ssid) - 1);
    cached_wifi_ssid[sizeof(cached_wifi_ssid) - 1] = '\0';

    strncpy(cached_wifi_pass, pass.c_str(), sizeof(cached_wifi_pass) - 1);
    cached_wifi_pass[sizeof(cached_wifi_pass) - 1] = '\0';

    cached_wifi_creds_valid = (strlen(cached_wifi_ssid) > 0);
  } else {
    cached_wifi_ssid[0] = '\0';
    cached_wifi_pass[0] = '\0';
    cached_wifi_creds_valid = false;
  }

  // GPIO46 HIGH = Client Mode Requested
  Serial.println("=== GPIO46 HIGH: CLIENT MODE REQUESTED ===");

  // If no saved credentials, enter config mode
  if (!hasClientConfig || !cached_wifi_creds_valid || strlen(cached_wifi_ssid) == 0) {
    Serial.println("=== NO CLIENT CREDENTIALS: ENTERING CONFIGURATION MODE ===");
    Serial.println(!hasClientConfig ? "Reason: No saved credentials file" : "Reason: Empty SSID");
    loadAPCredentials(false);  // Use custom AP credentials for config mode
    setupAccessPoint();
    setupWiFiConfigServer();  // Serve WiFi configuration interface
    currentMode = MODE_CONFIG;
    return;
  }

  // Normal operation - load custom AP credentials in case we need them later
  loadAPCredentials(false);

  // Attempt client connection with timeout (safe for 15s watchdog)
  if (connectToWiFi(cached_wifi_ssid, cached_wifi_pass, 10000)) {
    currentMode = MODE_CLIENT;
    Serial.println("=== CLIENT MODE SUCCESS ===");
    Serial.println("WiFi connected successfully!");
    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());
    setupServer();
  } else {
    Serial.println("=== CLIENT MODE FAILED ===");
    Serial.println("WiFi connection failed - will retry periodically (no AP fallback)");
    currentMode = MODE_CLIENT;  // Keep trying client mode
    // No automatic AP fallback - let intelligent reconnection logic handle it
  }


  Serial.println("=== WiFi Setup Complete ===");
}

bool connectToWiFi(const char *ssid, const char *password, unsigned long timeout) {

  if (!ssid || strlen(ssid) == 0) {
    Serial.println("ERROR: No SSID provided for WiFi connection");  // PRESERVES: Your error message style
    return false;
  }

  Serial.printf("Connecting to WiFi: %s\n", ssid);
  Serial.printf("Password length: %d\n", strlen(password));

  WiFi.disconnect();
  delay(100);
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);          // Disable WiFi power save
  WiFi.setAutoReconnect(false);  // checkWiFiConnection() handles reconnection

  if (strlen(password) > 0) {
    WiFi.begin(ssid, password);
  } else {
    WiFi.begin(ssid);  // Open network
  }

  unsigned long startTime = millis();
  int attempts = 0;
  const int maxAttempts = timeout / 500;  // Check every 500ms

  while (WiFi.status() != WL_CONNECTED && attempts < maxAttempts) {
    delay(500);
    esp_task_wdt_reset();
    attempts++;

    // Print progress every 2 seconds
    if (attempts % 4 == 0) {
      Serial.printf("WiFi Status: %d, attempt %d/%d\n", WiFi.status(), attempts, maxAttempts);
    }
  }

  if (WiFi.status() == WL_CONNECTED) {

    Serial.println("WiFi connection successful!");                         // PRESERVES: Your success message
    Serial.printf("IP address: %s\n", WiFi.localIP().toString().c_str());  // PRESERVES: Your IP logging
    Serial.printf("Signal strength: %d dBm\n", WiFi.RSSI());               // PRESERVES: Your signal logging

    // mDNS setup
    MDNS.end();
    if (MDNS.begin("alternator")) {
      Serial.println("mDNS responder started");
      MDNS.addService("http", "tcp", 80);
    }

    return true;

  } else {

    Serial.printf("WiFi connection failed after %lu ms\n", timeout);  // PRESERVES: Your failure message style
    Serial.printf("Final status: %d\n", WiFi.status());               // PRESERVES: Your debug info
    return false;
  }
}
void setupCaptivePortalLanding() {
  static bool captiveInitialized = false;
  if (captiveInitialized) {
    Serial.println("setupCaptivePortalLanding() already initialized - skipping");
    return;
  }
  captiveInitialized = true;
  Serial.println("Setting up captive portal landing...");
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    static const char PAGE[] PROGMEM = R"HTML(<!DOCTYPE html><html><head><title>Alternator Regulator Connected</title>
<meta name='viewport' content='width=device-width,initial-scale=1'>
<style>
body{font-family:Arial;text-align:center;padding:20px;background:#f5f5f5;line-height:1.6}
.card{background:white;padding:24px;border-left:4px solid #ff6600;border-radius:8px;max-width:500px;margin:0 auto;box-shadow:0 2px 4px rgba(0,0,0,0.1)}
h1{color:#333;margin-bottom:1rem;font-size:24px}
.success-box{background:#d4edda;border:1px solid #c3e6cb;color:#155724;padding:16px;border-radius:8px;margin:20px 0}
.big-button{display:inline-block;background:#ff6600;color:white;padding:16px 32px;text-decoration:none;border-radius:8px;font-weight:bold;font-size:18px;margin:20px 0}
.big-button:hover{background:#e65c00}
.bookmark-info{background:#f8f9fa;border:1px solid #dee2e6;padding:12px;border-radius:8px;margin:16px 0;font-size:14px}
.ip-address{font-family:monospace;font-size:20px;font-weight:bold;color:#ff6600;background:#f8f9fa;padding:8px 12px;border-radius:8px;display:inline-block;margin:8px 0}
</style></head><body>
<div class='card'>
<h1>Alternator Regulator</h1>
<div class='success-box'><strong>Successfully Connected!</strong><br>You are now connected to the alternator regulator's WiFi network.</div>
<p>Access the full alternator control interface:</p>
<a href='http://192.168.4.1' class='big-button'>Open Alternator Interface</a>
<div class='bookmark-info'><strong>For easy future access:</strong><br>Bookmark this address: <span class='ip-address'>192.168.4.1</span></div>
<p style='margin-top:24px;font-size:14px;color:#666'><strong>Network:</strong> ALTERNATOR_WIFI<br><strong>Device IP:</strong> 192.168.4.1</p>
</div></body></html>)HTML";
    request->send_P(200, "text/html", PAGE);
  });
  server.begin();
  Serial.println("Landing page ready");
}

void setupAccessPoint() {
  static bool apInitialized = false;
  if (apInitialized) {
    Serial.println("setupAccessPoint() already initialized - skipping");
    return;
  }
  apInitialized = true;
  Serial.println("=== SETTING UP ACCESS POINT ===");
  Serial.println("Using SSID: '" + esp32_ap_ssid + "'");
  Serial.println("Using password: [" + String(esp32_ap_password.length()) + " chars]");
  Serial.println("Password length: " + String(esp32_ap_password.length()));
  Serial.println("SSID length: " + String(esp32_ap_ssid.length()));

  WiFi.mode(WIFI_AP);
  WiFi.setSleep(false);  // Disable WiFi power save

  bool apStarted = WiFi.softAP(esp32_ap_ssid.c_str(), esp32_ap_password.c_str());
  delay(500);
  if (apStarted) {
    Serial.println("=== ACCESS POINT STARTED SUCCESSFULLY ===");
    Serial.print("AP SSID: ");
    Serial.println(esp32_ap_ssid);
    Serial.print("AP Password length: ");
    Serial.println(esp32_ap_password.length());
    Serial.print("AP IP address: ");
    Serial.println(WiFi.softAPIP());

    // Start DNS server for captive portal
    IPAddress apIP = WiFi.softAPIP();
    Serial.println("AP IP before DNS start: " + apIP.toString());
    bool dnsStarted = dnsServer.start(DNS_PORT, "*", apIP);
    Serial.println("DNS server start result: " + String(dnsStarted));
    Serial.println("DNS server started for captive portal");

    // **FIX: Start mDNS in AP mode too (best-effort for alternator.local)**
    MDNS.end();  // Ensure clean state
    if (MDNS.begin("alternator")) {
      MDNS.addService("http", "tcp", 80);
      Serial.println("✅ mDNS started - alternator.local available (may not work on all devices in AP mode)");
    } else {
      Serial.println("⚠️ mDNS failed to start in AP mode");
    }

    Serial.println("=== AP SETUP COMPLETE ===");
  } else {
    Serial.println("=== ACCESS POINT FAILED TO START ===");
    Serial.println("This is a critical error!");
    // Try with default settings as fallback
    Serial.println("Trying with default settings as fallback...");
    WiFi.softAP("ALTERNATOR_WIFI", "alternator123");
  }
}

static void sendWifiConfigPortal(AsyncWebServerRequest *request) {
  AsyncWebServerResponse *response = request->beginResponse_P(200, "text/html", WIFI_CONFIG_HTML);
  response->addHeader("Cache-Control", "no-cache, no-store, must-revalidate");
  response->addHeader("Pragma", "no-cache");
  response->addHeader("Expires", "0");
  request->send(response);
}
void setupWiFiConfigServer() {
  static bool configServerInitialized = false;
  if (configServerInitialized) {
    Serial.println("setupWiFiConfigServer() already initialized - skipping");
    return;
  }
  configServerInitialized = true;
  Serial.println("\n=== SETTING UP WIFI CONFIG SERVER ===");
  Serial.println("=== IMPORTANT: Alternator is DISABLED in CONFIG mode ===");
  Serial.println("=== For emergency operation: Power down, hold GPIO46 LOW, power up ===");
  Serial.println("=== This enters AP mode with full alternator functionality ===\n");

  server.on("/generate_204", HTTP_GET, [](AsyncWebServerRequest *request) {
    Serial.println("Android captive portal detection");
    request->redirect("http://" + WiFi.softAPIP().toString() + "/");
  });
  server.on("/gen_204", HTTP_GET, [](AsyncWebServerRequest *request) {
    Serial.println("Android captive portal detection (alt)");
    request->redirect("http://" + WiFi.softAPIP().toString() + "/");
  });

  server.on("/hotspot-detect.html", HTTP_ANY, [](AsyncWebServerRequest *request) {
    Serial.println("Apple captive portal detection");
    sendWifiConfigPortal(request);
  });

  server.on("/ncsi.txt", HTTP_GET, [](AsyncWebServerRequest *request) {
    Serial.println("Windows NCSI detection");
    request->redirect("http://" + WiFi.softAPIP().toString() + "/");
  });
  server.on("/connecttest.txt", HTTP_GET, [](AsyncWebServerRequest *request) {
    Serial.println("Windows captive portal detection");
    request->redirect("http://" + WiFi.softAPIP().toString() + "/");
  });

  server.on("/", HTTP_ANY, [](AsyncWebServerRequest *request) {
    Serial.println("=== CONFIG PAGE REQUEST ===");
    Serial.println("Client IP: " + request->client()->remoteIP().toString());
    Serial.println("User-Agent: " + request->header("User-Agent"));
    Serial.println("Serving WiFi configuration page");
    sendWifiConfigPortal(request);
  });

  server.on("/wifi", HTTP_POST, [](AsyncWebServerRequest *request) {
    Serial.println("\n=== WIFI CONFIG POST RECEIVED ===");

    char ssid[33] = "";
    char password[65] = "";
    char ap_password[65] = "";
    char hotspot_ssid[33] = "";

    auto copyParamTrim = [](AsyncWebServerRequest *req, const char *name, char *dst, size_t dstSize) -> bool {
      if (!req->hasParam(name, true)) return false;
      const String &v = req->getParam(name, true)->value();
      size_t n = v.length();
      if (n >= dstSize) n = dstSize - 1;
      memcpy(dst, v.c_str(), n);
      dst[n] = '\0';

      size_t start = 0;
      while (dst[start] == ' ' || dst[start] == '\t' || dst[start] == '\r' || dst[start] == '\n') start++;
      if (start) memmove(dst, dst + start, strlen(dst + start) + 1);

      size_t end = strlen(dst);
      while (end > 0 && (dst[end - 1] == ' ' || dst[end - 1] == '\t' || dst[end - 1] == '\r' || dst[end - 1] == '\n')) {
        dst[end - 1] = '\0';
        end--;
      }
      return true;
    };

    copyParamTrim(request, "ap_password", ap_password, sizeof(ap_password));
    copyParamTrim(request, "hotspot_ssid", hotspot_ssid, sizeof(hotspot_ssid));
    copyParamTrim(request, "ssid", ssid, sizeof(ssid));
    copyParamTrim(request, "password", password, sizeof(password));

    if (ap_password[0] != '\0' && strlen(ap_password) < 8) {
      static const char ERR[] PROGMEM =
        "<!DOCTYPE html><html><head><title>Configuration Error</title></head><body>"
        "<h1>Configuration Error</h1>"
        "<p><strong>AP password must be at least 8 characters or left blank for default.</strong></p>"
        "<p><a href='javascript:history.back()'>Go Back and Try Again</a></p>"
        "</body></html>";
      request->send_P(400, "text/html", ERR);
      return;
    }

    if (ap_password[0] == '\0') {
      strncpy(ap_password, "alternator123", sizeof(ap_password) - 1);
      ap_password[sizeof(ap_password) - 1] = '\0';
    }

    Serial.println("=== SAVING CONFIGURATION ===");
    Serial.printf("SSID: '%s' (length: %u)\n", ssid, (unsigned)strlen(ssid));
    Serial.printf("Password: '%s' (length: %u)\n", password, (unsigned)strlen(password));

    Serial.printf("LittleFS mounted: %s\n", littleFSMounted ? "YES" : "NO");
    if (!littleFSMounted) {
      Serial.println("Attempting to mount LittleFS...");
      if (!ensureLittleFS()) {
        Serial.println("CRITICAL: LittleFS mount failed");
        request->send(500, "text/plain", "Filesystem error - cannot save configuration");
        return;
      }
    }

    size_t totalB = fsTotalBytes();
    size_t usedB = fsUsedBytes();
    Serial.printf("LittleFS total bytes: %u\n", (unsigned)totalB);
    Serial.printf("LittleFS used bytes: %u\n", (unsigned)usedB);
    Serial.printf("LittleFS free bytes: %u\n", (unsigned)(totalB - usedB));

    writeFile(LittleFS, AP_PASSWORD_FILE, ap_password);
    esp32_ap_password = ap_password;

    if (hotspot_ssid[0] != '\0') {
      writeFile(LittleFS, AP_SSID_FILE, hotspot_ssid);
      esp32_ap_ssid = hotspot_ssid;
    } else {
      if (fsExists(AP_SSID_FILE)) {
        fsRemove(AP_SSID_FILE);
      }
      esp32_ap_ssid = "ALTERNATOR_WIFI";
    }

    writeFile(LittleFS, "/ssid.txt", ssid);
    writeFile(LittleFS, "/pass.txt", password);

    strncpy(cached_wifi_ssid, ssid, sizeof(cached_wifi_ssid) - 1);
    cached_wifi_ssid[sizeof(cached_wifi_ssid) - 1] = '\0';

    strncpy(cached_wifi_pass, password, sizeof(cached_wifi_pass) - 1);
    cached_wifi_pass[sizeof(cached_wifi_pass) - 1] = '\0';

    cached_wifi_creds_valid = (cached_wifi_ssid[0] != '\0');

    Serial.printf("Verification - SSID: '%s'\n", cached_wifi_ssid);
    Serial.printf("Verification - Password: '%s'\n", cached_wifi_pass);
    delay(1000);

    request->send(200, "text/plain", "Configuration saved! Device will restart in 3 seconds.");

    Serial.println("=== CONFIGURATION SAVED - RESTARTING ===");
    writeFile(LittleFS, "/first_config_done.txt", "1");
    delay(3000);
    ESP.restart();
  });

  // Enhanced 404 handler
  server.onNotFound([](AsyncWebServerRequest *request) {
    if (WiFi.getMode() == WIFI_AP || WiFi.getMode() == WIFI_AP_STA) {
      request->redirect("http://" + WiFi.softAPIP().toString() + "/");
    } else {
      request->send(404, "text/plain", "Not Found");
    }
  });

  server.begin();
  Serial.println("=== WIFI CONFIG SERVER STARTED ===");
  Serial.println("=== WIFI CONFIG SERVER SETUP COMPLETE ===");
}

void setupServer() {

  static bool serverInitialized = false;
  if (serverInitialized) {
    Serial.println("setupServer() already initialized - skipping");
    return;
  }
  serverInitialized = true;
  Serial.println("=== SETTING UP MAIN SERVER ===");

  ensureWebFS();  // Mount appropriate web partition (prod_fs with factory_fs fallback)

  server.on("/favicon.ico", HTTP_GET, [](AsyncWebServerRequest *request) {  // just get rid of annoying chrome Console error messages
    static const uint8_t blank[] = { 0 };
    request->send(200, "image/x-icon", blank, sizeof(blank));
  });
  //Factory Reset Logic
  server.on("/factoryReset", HTTP_POST, [](AsyncWebServerRequest *request) {
    if (!request->hasParam("password", true)) {
      request->send(400, "text/plain", "Missing password");
      return;
    }
    String password = request->getParam("password", true)->value();
    if (!validatePassword(password.c_str())) {
      request->send(403, "text/plain", "FAIL");
      return;
    }

    Serial.println("\n=== FACTORY RESET INITIATED FROM WEB ===");
    queueConsoleMessage("FACTORY RESET: Initiated from web interface");

    request->send(200, "text/plain", "OK");  // Respond before blocking restart

    performDeepFactoryReset();  // Unmounts+reformats LittleFS, erases all NVS, reinits, restarts
  });


  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    AsyncWebServerResponse *response = request->beginResponse(webFS, "/index.html.gz", "text/html");
    response->addHeader("Content-Encoding", "gzip");
    request->send(response);
  });
  server.on("/thermallog.csv", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (!thermalLogReady || !thermalLog || thermalLogCount == 0) {
      request->send(200, "text/plain", "No thermal log data yet.");
      return;
    }

    ThermalDLState state;
    state.count = thermalLogCount;
    state.oldest = (thermalLogHead - thermalLogCount + THERMAL_LOG_SIZE) % THERMAL_LOG_SIZE;
    state.row = 0;
    state.done = false;
    state.lineLen = 0;
    state.linePos = 0;

    thermalLogPaused = true;
    thermalLogPausedAtMs = millis();

    AsyncWebServerResponse *response = request->beginChunkedResponse(
      "text/csv",
      [state](uint8_t *buf, size_t maxLen, size_t) mutable -> size_t {
        thermalLogPausedAtMs = millis();

        if (state.done) return 0;

        size_t written = 0;

        while (written < maxLen) {

          if (state.linePos >= state.lineLen) {

            if (state.row > state.count) {
              thermalLogPaused = false;
              state.done = true;
              return written;
            }

            if (state.row == 0) {
              // Header row — tempRaw and gains removed
              state.lineLen = snprintf(
                state.line, sizeof(state.line),
                "ts_ms,tempFilt_F,tempProj_F,nominalTarget_A,"
                "rpmCap_A,voltCap_A,uTarget_A,spLimited_A,"
                "pidErr_A,pidOut_pct,duty_pct,RPM,battV,measAmps_A,"
                "penaltyAmps_A,flags,chargeStageDisplay,"
                "outerP,outerI,outerD,impliedPenalty,antiWindupFired,thermalSlope_F_sec\n");

            } else if (state.row == 1) {
              // Constants row — written once, Python detects via "CONST" in ts_ms field
              state.lineLen = snprintf(
                state.line, sizeof(state.line),
                "CONST,kp=%.6g,ki=%.6g,lookahead=%.1f\n",
                TempPIDKp,
                TempPIDKi,
                ThermalLookaheadSec);

            } else {
              // Data rows — index offset by 2 (header + constants row)
              int idx = (state.oldest + state.row - 2) % THERMAL_LOG_SIZE;
              ThermalLogEntry e;
              memcpy(&e, &thermalLog[idx], 48);

              state.lineLen = snprintf(
                state.line, sizeof(state.line),
                "%lu,%.1f,%.1f,%.1f,%.1f,%.1f,%.1f,%.1f,"
                "%.1f,%.1f,%.1f,%d,%.1f,%.1f,%.1f,%u,%u,"
                "%.1f,%.1f,%.1f,%.1f,%u,%.1f\n",
                (unsigned long)e.ts,
                e.tempFiltered / 10.0f,
                e.tempProjected / 10.0f,
                e.nominalTarget / 10.0f,
                e.rpmCap / 10.0f,
                e.voltCap / 10.0f,
                e.uTarget / 10.0f,
                e.spLimited / 10.0f,
                e.pidErr / 10.0f,
                e.pidOut / 10.0f,
                e.duty / 10.0f,
                (int)e.rpm,
                e.battV / 10.0f,
                e.measAmps / 10.0f,
                e.penaltyAmps / 10.0f,
                (unsigned)e.flags,
                (unsigned)e.chargeStageDisplay,
                e.outerTermP / 10.0f,
                e.outerTermI / 10.0f,
                e.outerTermD / 10.0f,
                e.impliedPenalty / 10.0f,
                (unsigned)e.antiWindupFired,
                e.thermalSlope / 1000.0f);
            }

            state.linePos = 0;
            state.row++;
          }

          size_t canSend = min(maxLen - written,
                               (size_t)(state.lineLen - state.linePos));
          memcpy(buf + written, state.line + state.linePos, canSend);
          written += canSend;
          state.linePos += (int)canSend;
        }

        return written;
      });

    response->addHeader("Content-Disposition", "attachment");
    response->addHeader("Cache-Control", "no-cache");
    request->send(response);
  });


  server.on("/thermallog.bin", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (!thermalLogReady || !thermalLog || thermalLogCount == 0) {
      request->send(200, "application/octet-stream", "");
      return;
    }

    ThermalBinDLState state;
    uint32_t cnt = (uint32_t)thermalLogCount;
    uint32_t intMs = THERMAL_LOG_INTERVAL_MS;
    memcpy(state.header, &cnt, 4);
    memcpy(state.header + 4, &intMs, 4);
    state.headerPos = 0;
    state.count = thermalLogCount;
    state.oldest = (thermalLogHead - thermalLogCount + THERMAL_LOG_SIZE) % THERMAL_LOG_SIZE;
    state.row = 0;
    state.done = false;
    state.entryLen = 0;
    state.entryPos = 0;

    thermalLogPaused = true;
    thermalLogPausedAtMs = millis();

    AsyncWebServerResponse *response = request->beginChunkedResponse(
      "application/octet-stream",
      [state](uint8_t *buf, size_t maxLen, size_t) mutable -> size_t {
        thermalLogPausedAtMs = millis();
        if (state.done) return 0;

        size_t written = 0;

        while (written < maxLen) {
          // Phase 1: drain 8-byte header
          if (state.headerPos < 8) {
            size_t canSend = min(maxLen - written, (size_t)(8 - state.headerPos));
            memcpy(buf + written, state.header + state.headerPos, canSend);
            written += canSend;
            state.headerPos += (int)canSend;
            continue;
          }

          // Phase 2: stream entries as raw struct bytes
          if (state.entryPos >= state.entryLen) {
            if (state.row >= state.count) {
              thermalLogPaused = false;
              state.done = true;
              return written;
            }
            int idx = (state.oldest + state.row) % THERMAL_LOG_SIZE;
            memcpy(state.entryBuf, &thermalLog[idx], 48);
            state.entryLen = 48;
            state.entryPos = 0;
            state.row++;
          }

          size_t canSend = min(maxLen - written, (size_t)(state.entryLen - state.entryPos));
          memcpy(buf + written, state.entryBuf + state.entryPos, canSend);
          written += canSend;
          state.entryPos += (int)canSend;
        }

        return written;
      });

    response->addHeader("Cache-Control", "no-cache");
    request->send(response);
  });

  server.on("/pidlog.csv", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (!pidLogReady || !pidLog || pidLogCount == 0) {
      request->send(200, "text/plain", "No PID log data yet.");
      return;
    }

    PidDLState state;
    state.count = pidLogCount;
    state.oldest = (pidLogHead - pidLogCount + PID_LOG_SIZE) % PID_LOG_SIZE;
    state.row = 0;
    state.done = false;
    state.lineLen = 0;
    state.linePos = 0;

    pidLogPaused = true;
    pidLogPausedAtMs = millis();

    AsyncWebServerResponse *response = request->beginChunkedResponse(
      "text/csv",
      [state](uint8_t *buf, size_t maxLen, size_t) mutable -> size_t {
        // Refresh pause watchdog on every chunk callback
        pidLogPausedAtMs = millis();

        if (state.done) return 0;

        size_t written = 0;

        while (written < maxLen) {

          // Refill line buffer when previous line is exhausted
          if (state.linePos >= state.lineLen) {

            // ── Row 0: comment block ──────────────────────────────────────
            if (state.row == 0) {
              state.lineLen = snprintf(
                state.line, sizeof(state.line),
                "# PID diagnostic log — CV loop / output current PID / duty pipeline\n"
                "# flags: bit0=AUTO bit1=voltCtrl bit4=govBypass\n"
                "# voltageLoopRanThisTick=1 means Icv/cv_I updated this row\n"
                "# vError: always fresh every tick regardless of loop interval\n"
                "# Icv: CV velocity-form PI output — the direct current setpoint in CV modes\n"
                "# cv_I: CV position-form PI integrator state\n"
                "# tableThermalLimit: RPM cap minus thermal penalty, before CV\n"
                "# setpointCmd: Icv in CV modes, tableThermalLimit in bulk\n");
              state.lineLen = min((int)state.lineLen, (int)sizeof(state.line) - 1);

            } else if (state.row == 1) {
              state.lineLen = snprintf(
                state.line, sizeof(state.line),
                "ts_ms,"
                "chargeStageDisplay,"
                "TargetVoltageMode,"
                "battV,"
                "ChargingVoltageTarget,"
                "vError,"
                "Icv,"
                "cv_I,"
                "tableThermalLimit,"
                "setpointCmd,"
                "voltageLoopRanThisTick,"
                "pidSetpoint,"
                "pidInput,"
                "pidUnsatOutput,"
                "pidOutput,"
                "innerTermP,"
                "innerTermI,"
                "innerTermD,"
                "dutyRequest,"
                "dutyApplied,"
                "enteringCV,"
                "enteringTargetVoltageMode,"
                "rpm,"
                "measAmps,"
                "gainKp,"
                "gainKi,"
                "gainKd,"
                "battV_filt_V,"
                "iMeas_filt_A,"
                "flags\n");

              state.lineLen = min((int)state.lineLen, (int)sizeof(state.line) - 1);

              // ── Row 2+: data rows ─────────────────────────────────────────
            } else {
              int dataRow = state.row - 2;
              if (dataRow >= state.count) {
                // Signal end-of-stream
                pidLogPaused = false;
                state.done = true;
                return written;
              }

              int idx = (state.oldest + dataRow) % PID_LOG_SIZE;
              PidLogEntry e;
              memcpy(&e, &pidLog[idx], sizeof(PidLogEntry));

              state.lineLen = snprintf(
                state.line, sizeof(state.line),
                "%lu,"
                "%u,%u,"
                "%.3f,%.3f,%.4f,"
                "%.3f,%.4f,"     // Icv, cv_I
                "%.3f,%.3f,%u,"  // tableThermalLimit, setpointCmd, voltageLoopRanThisTick
                "%.3f,%.3f,%.3f,%.3f,"
                "%.4f,%.4f,%.4f,"
                "%.2f,%.2f,"
                "%u,%u,"
                "%.0f,%.2f,"
                "%.4f,%.4f,%.4f,"
                "%.3f,%.3f,"  // battV_filt, iMeas_filt
                "%u\n",
                (unsigned long)e.ts,
                (unsigned)e.chargeStageDisplay,
                (unsigned)e.TargetVoltageMode,
                e.battV,
                e.ChargingVoltageTarget,
                e.vError,
                e.Icv,
                e.cv_I,
                e.tableThermalLimit,
                e.setpointCmd,
                (unsigned)e.voltageLoopRanThisTick,
                e.pidSetpoint,
                e.pidInput,
                e.pidUnsatOutput,
                e.pidOutput,
                e.innerTermP,
                e.innerTermI,
                e.innerTermD,
                e.dutyRequest,
                e.dutyApplied,
                (unsigned)e.enteringCV,
                (unsigned)e.enteringTargetVoltageMode,
                e.rpm,
                e.measAmps,
                e.gainKp,
                e.gainKi,
                e.gainKd,
                e.battV_filt,
                e.iMeas_filt,
                (unsigned)e.flags);
              state.lineLen = min((int)state.lineLen, (int)sizeof(state.line) - 1);
            }

            state.linePos = 0;
            state.row++;
          }

          // Copy as much of the current line into the output buffer as fits
          size_t canSend = min(maxLen - written,
                               (size_t)(state.lineLen - state.linePos));
          memcpy(buf + written, state.line + state.linePos, canSend);
          written += canSend;
          state.linePos += (int)canSend;
        }

        return written;
      });

    response->addHeader("Content-Disposition", "attachment; filename=\"pidlog.csv\"");
    response->addHeader("Cache-Control", "no-cache");
    request->send(response);
  });


  server.on("/cvlog.bin", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (!cvLogReady || !cvLog || cvLogCount == 0) {
      request->send(200, "application/octet-stream", "");
      return;
    }

    CvBinDLState state;
    memset(&state, 0, sizeof(state));

    uint32_t cnt = (uint32_t)cvLogCount;
    uint32_t entrySize = (uint32_t)sizeof(CvLogEntry);
    float kp = (float)VoltageKp;
    float ki = (float)VoltageKi;
    float kd = (float)VoltageKd;
    uint32_t interval = (uint32_t)VoltageLoopInterval;

    memcpy(state.header + 0, &cnt, 4);
    memcpy(state.header + 4, &entrySize, 4);
    memcpy(state.header + 8, &kp, 4);
    memcpy(state.header + 12, &ki, 4);
    memcpy(state.header + 16, &interval, 4);
    memcpy(state.header + 20, &kd, 4);

    state.count = cvLogCount;
    state.oldest = (cvLogHead - cvLogCount + CV_LOG_SIZE) % CV_LOG_SIZE;

    cvLogPaused = true;
    cvLogPausedAtMs = millis();

    AsyncWebServerResponse *response = request->beginChunkedResponse(
      "application/octet-stream",
      [state](uint8_t *buf, size_t maxLen, size_t) mutable -> size_t {
        cvLogPausedAtMs = millis();
        if (state.done) return 0;

        size_t written = 0;
        while (written < maxLen) {
          // Phase 1: drain 24-byte header
          if (state.headerPos < CV_LOG_HEADER_SIZE) {
            size_t canSend = min(maxLen - written,
                                 (size_t)(CV_LOG_HEADER_SIZE - state.headerPos));
            memcpy(buf + written, state.header + state.headerPos, canSend);
            written += canSend;
            state.headerPos += (int)canSend;
            continue;
          }
          // Phase 2: stream entries
          if (state.entryPos >= state.entryLen) {
            if (state.row >= state.count) {
              cvLogPaused = false;
              state.done = true;
              return written;
            }
            int idx = (state.oldest + state.row) % CV_LOG_SIZE;
            memcpy(state.entryBuf, &cvLog[idx], sizeof(CvLogEntry));
            state.entryLen = (int)sizeof(CvLogEntry);
            state.entryPos = 0;
            state.row++;
          }
          size_t canSend = min(maxLen - written,
                               (size_t)(state.entryLen - state.entryPos));
          memcpy(buf + written, state.entryBuf + state.entryPos, canSend);
          written += canSend;
          state.entryPos += (int)canSend;
        }
        return written;
      });

    response->addHeader("Cache-Control", "no-cache");
    request->send(response);
  });


  server.on("/resetlogs", HTTP_POST, [](AsyncWebServerRequest *request) {
    thermalLogHead = 0;
    thermalLogCount = 0;
    thermalLogPaused = false;
    pidLogHead = 0;
    pidLogCount = 0;
    pidLogPaused = false;
    cvLogHead = 0;
    cvLogCount = 0;
    cvLogPaused = false;
    request->send(200, "text/plain", "OK");
  });


  server.on("/vessel_info.json", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (LittleFS.exists("/vessel_info.json")) {
      request->send(LittleFS, "/vessel_info.json", "application/json");
    } else {
      request->send(404, "application/json", "{\"error\":\"Vessel info not found\"}");
    }
  });
  server.on("/saveVesselInfo", HTTP_POST, [](AsyncWebServerRequest *request) {
    // Password check
    if (!request->hasParam("password", true)) {
      request->send(401, "application/json", "{\"success\":false,\"error\":\"No password\"}");
      return;
    }
    String password = request->getParam("password", true)->value();
    password.trim();
    if (!validatePassword(password.c_str())) {
      request->send(401, "application/json", "{\"success\":false,\"error\":\"Invalid password\"}");
      return;
    }
    // Get JSON data
    if (!request->hasParam("vesselData", true)) {
      request->send(400, "application/json", "{\"success\":false,\"error\":\"No vessel data\"}");
      return;
    }
    String jsonStr = request->getParam("vesselData", true)->value();
    DynamicJsonDocument doc(1024);
    DeserializationError error = deserializeJson(doc, jsonStr);
    if (error) {
      request->send(400, "application/json", "{\"success\":false,\"error\":\"Invalid JSON\"}");
      return;
    }
    // Update RAM variables
    BOAT_LENGTH_FT = doc["boat_length_ft"];
    BOAT_TYPE = doc["boat_type"].as<String>();
    BOAT_MAKE_MODEL = doc["boat_make_model"].as<String>();
    BOAT_YEAR = doc["boat_year"];
    if (doc.containsKey("home_port") && !doc["home_port"].isNull()) {
      const char *homePortStr = doc["home_port"];
      strncpy(HOME_PORT, homePortStr, 50);
      HOME_PORT[50] = '\0';
    } else {
      HOME_PORT[0] = '\0';  // Empty string if not provided
    }
    ENGINE_MAKE = doc["engine_make"].as<String>();
    ENGINE_HP = doc["engine_hp"];
    BATTERY_VOLTAGE = doc["battery_voltage"];
    BatteryCapacity_Ah = doc["battery_capacity_ah"];
    BATTERY_TYPE = doc["battery_type"].as<String>();
    ALTERNATOR_BRAND_MODEL = doc["alternator_brand_model"].as<String>();
    SolarWatts = doc["solar_watts"];
    imuMountOrientation = doc["imu_mount_orientation"];
    IMU_DIST_BOW_FT = doc["imu_dist_bow_ft"];
    IMU_DIST_CL_FT = doc["imu_dist_cl_ft"];
    IMU_HEIGHT_WL_FT = doc["imu_height_wl_ft"];
    // Write to LittleFS
    File file = LittleFS.open("/vessel_info.json", "w");
    if (file) {
      serializeJson(doc, file);
      file.close();
      Serial.println("Vessel info saved successfully");
      request->send(200, "application/json", "{\"success\":true}");
    } else {
      Serial.println("Failed to write vessel_info.json");
      request->send(500, "application/json", "{\"success\":false,\"error\":\"File write failed\"}");
    }
  });
  server.on("/clearVesselInfo", HTTP_POST, [](AsyncWebServerRequest *request) {
    // Password check
    if (!request->hasParam("password", true)) {
      request->send(401, "application/json", "{\"success\":false,\"error\":\"No password\"}");
      return;
    }
    String password = request->getParam("password", true)->value();
    password.trim();
    if (!validatePassword(password.c_str())) {
      request->send(401, "application/json", "{\"success\":false,\"error\":\"Invalid password\"}");
      return;
    }

    // Reset all vessel info to defaults
    BOAT_LENGTH_FT = 0;
    BOAT_TYPE = "monohull";
    BOAT_MAKE_MODEL = "";
    BOAT_YEAR = 2025;
    ENGINE_MAKE = "";
    ENGINE_HP = 0;
    HOME_PORT[0] = '\0';
    BATTERY_VOLTAGE = 12;
    BatteryCapacity_Ah = 0;
    BATTERY_TYPE = "lifepo4";
    ALTERNATOR_BRAND_MODEL = "";
    SolarWatts = 0;
    imuMountOrientation = 0;
    IMU_DIST_BOW_FT = 0;
    IMU_DIST_CL_FT = 0;
    IMU_HEIGHT_WL_FT = 0;

    // Delete the file (or write defaults)
    if (LittleFS.exists("/vessel_info.json")) {
      LittleFS.remove("/vessel_info.json");
    }

    Serial.println("Vessel info cleared");
    request->send(200, "application/json", "{\"success\":true}");
  });
  // ============================================================
  // REFACTORED /get HANDLER
  // Replace the existing server.on("/get", ...) block in
  // 2_aimportantfunctions.ino with this entire block.
  //
  // Change summary:
  //   - Every `else if (request->hasParam(...))` in the parameter
  //     processing section is now a standalone `if (...)`, so ALL
  //     matching parameters in a single request are processed.
  //   - The early-exit special cases (UpdateToVersion, OnOff=0
  //     safety, password gate) are structurally unchanged.
  //   - The RPM/fuel table loops are unchanged.
  //   - Parameters that were previously unreachable when
  //     fuelTableUpdated==true (AlternatorNominalAmps onward) are
  //     now independent if-blocks, fixing that latent bug.
  //   - All file writes, variable assignments, and side-effect
  //     calls (SetTunings, etc.) are identical to the original.
  // ============================================================

  server.on("/get", HTTP_GET, [](AsyncWebServerRequest *request) {
    bool foundParameter = false;
    String inputMessage;

    if (request->hasParam("UpdateToVersion")) {
      foundParameter = true;
      String selectedVersionStr = request->getParam("UpdateToVersion")->value();

      char selectedVersion[32];
      strncpy(selectedVersion, selectedVersionStr.c_str(), 31);
      selectedVersion[31] = '\0';

      OnOff = 0;
      Serial.println("OTA UPDATE: Alternator disabled for safety");
      queueConsoleMessage("OTA UPDATE: Alternator disabled for safety");

      unsigned long timeSinceLastNVSSave = millis() - lastNVSSaveTime;
      Serial.printf("UPDATE DEBUG: Time since last NVS save: %lu ms\n", timeSinceLastNVSSave);
      Serial.printf("UPDATE DEBUG: Free heap: %u bytes\n", ESP.getFreeHeap());

      size_t freeInternal = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
      size_t largestBlock = heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL);
      Serial.printf("UPDATE DEBUG: Free internal: %u, Largest block: %u\n", freeInternal, largestBlock);

      const esp_partition_t *running_partition = esp_ota_get_running_partition();
      const esp_partition_t *factory_partition = esp_partition_find_first(
        ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_APP_FACTORY, NULL);

      if (running_partition == factory_partition) {
        Serial.printf("Direct update to version %s from factory partition\n", selectedVersion);
        performOTAUpdateToVersion(selectedVersion);
        inputMessage = "Update initiated from factory partition";

      } else {
        Serial.println("UPDATE DEBUG: Storing update request in NVS");

        nvs_handle_t nvs_handle;
        esp_err_t err = nvs_open("update_req", NVS_READWRITE, &nvs_handle);
        if (err != ESP_OK) {
          Serial.printf("UPDATE DEBUG: Failed to open NVS namespace 'update_req': %s\n", esp_err_to_name(err));
          request->send(500, "text/plain", "NVS open failed");
          return;
        }

        nvs_erase_key(nvs_handle, "target_ver");
        nvs_erase_key(nvs_handle, "update_flag");

        err = nvs_set_str(nvs_handle, "target_ver", selectedVersion);
        if (err != ESP_OK) {
          Serial.printf("UPDATE DEBUG: Failed to set target_ver: %s\n", esp_err_to_name(err));
          nvs_close(nvs_handle);
          request->send(500, "text/plain", "NVS write failed");
          return;
        }

        uint8_t updateFlag = 1;
        err = nvs_set_u8(nvs_handle, "update_flag", updateFlag);
        if (err != ESP_OK) {
          Serial.printf("UPDATE DEBUG: Failed to set update_flag: %s\n", esp_err_to_name(err));
          nvs_close(nvs_handle);
          request->send(500, "text/plain", "NVS write failed");
          return;
        }

        uint8_t wakeFlag = 1;
        err = nvs_set_u8(nvs_handle, "wake_flag", wakeFlag);
        if (err != ESP_OK) {
          Serial.printf("UPDATE DEBUG: Failed to set wake_flag: %s\n", esp_err_to_name(err));
        }

        err = nvs_commit(nvs_handle);

        nvs_handle_t verify_handle;
        if (nvs_open("update_req", NVS_READONLY, &verify_handle) == ESP_OK) {
          uint8_t verifyFlag = 0;
          char verifyVersion[32] = { 0 };
          size_t verifySize = 32;

          if (nvs_get_u8(verify_handle, "update_flag", &verifyFlag) == ESP_OK && nvs_get_str(verify_handle, "target_ver", verifyVersion, &verifySize) == ESP_OK) {
            Serial.printf("UPDATE DEBUG: NVS write verified - flag: %u, version: %s\n", verifyFlag, verifyVersion);
          } else {
            Serial.println("UPDATE DEBUG: NVS write verification FAILED");
          }
          nvs_close(verify_handle);
        }

        Serial.printf("Update to version %s requested - rebooting to factory\n", selectedVersion);
        esp_ota_set_boot_partition(factory_partition);
        inputMessage = "Rebooting to factory for update";

        request->send(200, "text/plain", inputMessage);
        Serial.println("=== RESTART SEQUENCE ===");
        Serial.printf("Free heap: %u bytes\n", ESP.getFreeHeap());
        Serial.printf("Active tasks: %u\n", uxTaskGetNumberOfTasks());
        Serial.println("Closing event connections...");
        events.close();
        delay(1000);
        Serial.println("Restarting now...");
        Serial.println("========================================");
        Serial.println("NOTE: Task termination backtrace next is expected and harmless");
        Serial.println("========================================");
        Serial.flush();
        delay(5000);
        ESP.restart();
        return;
      }
    }

    // === SAFETY: Allow field OFF without password ===
    if (request->hasParam("OnOff")) {
      int requestedState = request->getParam("OnOff")->value().toInt();
      if (requestedState == 0) {
        // Turning OFF is ALWAYS allowed — safety critical
        OnOff = 0;
        writeFile(LittleFS, "/OnOff.txt", "0");
        stateRevision++;
        queueConsoleMessage("FIELD OFF: Safety override (no password required)");
        request->send(200, "text/plain", "0");
        return;
      }
      // If turning ON, fall through to password check below
    }

    if (!request->hasParam("password") || strcmp(request->getParam("password")->value().c_str(), requiredPassword) != 0) {
      request->send(403, "text/plain", "Forbidden");
      return;
    }

    // ---------------------------------------------------------------
    // All parameters below are processed independently — every
    // matching param in the request is handled, not just the first.
    // ---------------------------------------------------------------
    else if (request->hasParam("InputFilterTC")) {
      foundParameter = true;
      inputMessage = request->getParam("InputFilterTC")->value();
      writeFile(LittleFS, "/InputFilterTC.txt", inputMessage.c_str());
      InputFilterTC = inputMessage.toFloat();
      if (CVTuningMode) cvTuningParamChanged = true;
    }

    else if (request->hasParam("SystemIDStepAmplitude")) {
      foundParameter = true;
      inputMessage = request->getParam("SystemIDStepAmplitude")->value();
      writeFile(LittleFS, "/SystemIDStepAmplitude.txt", inputMessage.c_str());
      SystemIDStepAmplitude = inputMessage.toFloat();
    }

    else if (request->hasParam("startSystemID")) {
      foundParameter = true;
      systemIDRequested = true;
      systemIDResultsReady = false;
      queueConsoleMessage("SystemID: test requested via web UI");
    }

    else if (request->hasParam("cancelSystemID")) {
      foundParameter = true;
      systemIDAbortRequested = true;
      queueConsoleMessage("SystemID: abort requested via web UI");
    }

    if (request->hasParam("TemperatureLimitF")) {
      foundParameter = true;
      inputMessage = request->getParam("TemperatureLimitF")->value();
      writeFile(LittleFS, "/TemperatureLimitF.txt", inputMessage.c_str());
      TemperatureLimitF = inputMessage.toInt();
    }
    if (request->hasParam("ClearBuffer")) {
      foundParameter = true;
      clearSensorBuffer();
      queueConsoleMessage("Upload buffer manually cleared from web");
      inputMessage = "1";
    }
    if (request->hasParam("ClearToken")) {
      foundParameter = true;

      authToken = "";
      isRegistered = false;

      nvs_handle_t nvs_handle;
      if (nvs_open("auth", NVS_READWRITE, &nvs_handle) == ESP_OK) {
        nvs_erase_key(nvs_handle, "token");
        nvs_erase_key(nvs_handle, "registered");
        nvs_commit(nvs_handle);
        nvs_close(nvs_handle);
      }

      queueConsoleMessage("Auth token cleared - device requires re-registration");
      inputMessage = "1";
    }

    else if (request->hasParam("degradationThreshold")) {
      foundParameter = true;
      inputMessage = request->getParam("degradationThreshold")->value();
      // UI sends integer percent (e.g. 15), store as float fraction (0.15)
      degradationThreshold = inputMessage.toFloat() / 100.0f;
      writeFile(LittleFS, "/degradationThresh.txt",
                String(degradationThreshold).c_str());
    } else if (request->hasParam("anomalyMarginAmps")) {
      foundParameter = true;
      inputMessage = request->getParam("anomalyMarginAmps")->value();
      anomalyMarginAmps = inputMessage.toFloat();
      writeFile(LittleFS, "/anomalyMarginAmps.txt", String(anomalyMarginAmps).c_str());
    } else if (request->hasParam("anomalyAlarmThreshold")) {
      foundParameter = true;
      inputMessage = request->getParam("anomalyAlarmThreshold")->value();
      anomalyAlarmThreshold = (int)inputMessage.toInt();
      writeFile(LittleFS, "/anomalyAlarmThresh.txt", String(anomalyAlarmThreshold).c_str());
    } else if (request->hasParam("anomalyAlarmEnable")) {
      foundParameter = true;
      inputMessage = request->getParam("anomalyAlarmEnable")->value();
      anomalyAlarmEnable = (inputMessage.toInt() != 0);
      writeFile(LittleFS, "/anomalyAlarmEnable.txt", String((int)anomalyAlarmEnable).c_str());
    } else if (request->hasParam("ResetEfficiencyMatrix")) {
      foundParameter = true;
      resetEfficiencyMatrix();
    }

    if (request->hasParam("ManualDutyTarget")) {
      foundParameter = true;
      inputMessage = request->getParam("ManualDutyTarget")->value();
      writeFile(LittleFS, "/ManualDutyTarget.txt", inputMessage.c_str());
      ManualDutyTarget = inputMessage.toInt();
    }
    if (request->hasParam("socInfoAvailable")) {
      foundParameter = true;
      inputMessage = request->getParam("socInfoAvailable")->value();
      writeFile(LittleFS, "/socInfoAvailable.txt", inputMessage.c_str());
      socInfoAvailable = inputMessage.toInt();
    }
    if (request->hasParam("TailCurrent_A")) {
      foundParameter = true;
      inputMessage = request->getParam("TailCurrent_A")->value();
      writeFile(LittleFS, "/TailCurrent_A.txt", inputMessage.c_str());
      TailCurrent_A = inputMessage.toFloat();
    }
    if (request->hasParam("RebulkVoltage")) {
      foundParameter = true;
      inputMessage = request->getParam("RebulkVoltage")->value();
      writeFile(LittleFS, "/RebulkVoltage.txt", inputMessage.c_str());
      RebulkVoltage = inputMessage.toFloat();
    }
    if (request->hasParam("rebulkDebounceTime")) {
      foundParameter = true;
      inputMessage = request->getParam("rebulkDebounceTime")->value();
      rebulkDebounceTime = (uint32_t)inputMessage.toInt() * 1000UL;  // sec → ms
      // Save the ms value so boot load is consistent
      writeFile(LittleFS, "/rebulkDebounceTime.txt", String(rebulkDebounceTime).c_str());
    }

    if (request->hasParam("MinFloatTime")) {
      foundParameter = true;
      inputMessage = request->getParam("MinFloatTime")->value();
      MinFloatTime = (uint32_t)inputMessage.toInt() * 60000UL;  // min → ms
      writeFile(LittleFS, "/MinFloatTime.txt", String(MinFloatTime).c_str());
    }
    if (request->hasParam("SOC_BlockRebulk_percent")) {
      foundParameter = true;
      inputMessage = request->getParam("SOC_BlockRebulk_percent")->value();
      writeFile(LittleFS, "/SOC_BlockRebulk_percent.txt", inputMessage.c_str());
      SOC_BlockRebulk_percent = inputMessage.toInt();
    }
    if (request->hasParam("SOC_AllowRebulk_percent")) {
      foundParameter = true;
      inputMessage = request->getParam("SOC_AllowRebulk_percent")->value();
      writeFile(LittleFS, "/SOC_AllowRebulk_percent.txt", inputMessage.c_str());
      SOC_AllowRebulk_percent = inputMessage.toInt();
    }
    if (request->hasParam("BulkVoltage")) {
      foundParameter = true;
      inputMessage = request->getParam("BulkVoltage")->value();
      writeFile(LittleFS, "/BulkVoltage.txt", inputMessage.c_str());
      BulkVoltage = inputMessage.toFloat();
      updateINA228OvervoltageThreshold();  // important!  update the hardware overvoltage limit provided by INA228
    }
    if (request->hasParam("wavePeriod")) {
      foundParameter = true;
      inputMessage = request->getParam("wavePeriod")->value();
      writeFile(LittleFS, "/wavePeriod.txt", inputMessage.c_str());
      wavePeriod = inputMessage.toInt();
      if (TuningMode) tuningParamChanged = true;
    }
    if (request->hasParam("SwitchingFrequency")) {
      foundParameter = true;
      inputMessage = request->getParam("SwitchingFrequency")->value();
      int requestedFreq = inputMessage.toInt();
      writeFile(LittleFS, "/SwitchingFrequency.txt", String(requestedFreq).c_str());
      SwitchingFrequency = requestedFreq;
      queueConsoleMessageF("Frequency target set to %dHz", SwitchingFrequency);
    }
    if (request->hasParam("FloatVoltage")) {
      foundParameter = true;
      inputMessage = request->getParam("FloatVoltage")->value();
      writeFile(LittleFS, "/FloatVoltage.txt", inputMessage.c_str());
      FloatVoltage = inputMessage.toFloat();
    }
    if (request->hasParam("yyMin")) {
      foundParameter = true;
      inputMessage = request->getParam("yyMin")->value();
      writeFile(LittleFS, "/yyMin.txt", inputMessage.c_str());
      yyMin = inputMessage.toInt();
    }
    if (request->hasParam("FieldAdjustmentInterval")) {
      foundParameter = true;
      inputMessage = request->getParam("FieldAdjustmentInterval")->value();
      writeFile(LittleFS, "/FieldAdjustmentInterval.txt", inputMessage.c_str());
      FieldAdjustmentInterval = inputMessage.toFloat();
    }
    if (request->hasParam("ManualFieldToggle")) {
      foundParameter = true;
      inputMessage = request->getParam("ManualFieldToggle")->value();
      Serial.printf("[MFT] ESP32 received ManualFieldToggle=%s\n", inputMessage.c_str());
      writeFile(LittleFS, "/ManualFieldToggle.txt", inputMessage.c_str());
      ManualFieldToggle = inputMessage.toInt();
      Serial.printf("[MFT] ManualFieldToggle now=%d\n", ManualFieldToggle);
    }
    if (request->hasParam("capLimitMode")) {
      foundParameter = true;
      inputMessage = request->getParam("capLimitMode")->value();
      writeFile(LittleFS, "/capLimitMode.txt", inputMessage.c_str());
      capLimitMode = constrain(inputMessage.toInt(), 0, 1);
    }
    if (request->hasParam("SwitchControlOverride")) {
      foundParameter = true;
      inputMessage = request->getParam("SwitchControlOverride")->value();
      writeFile(LittleFS, "/SwitchControlOverride.txt", inputMessage.c_str());
      SwitchControlOverride = inputMessage.toInt();
    }
    if (request->hasParam("MaintainMode")) {
      foundParameter = true;
      inputMessage = request->getParam("MaintainMode")->value();
      writeFile(LittleFS, "/MaintainMode.txt", inputMessage.c_str());
      MaintainMode = inputMessage.toInt();
      queueConsoleMessageF("MaintainMode mode %s", MaintainMode ? "enabled" : "disabled");
    }
    if (request->hasParam("TargetVoltageMode")) {
      foundParameter = true;
      inputMessage = request->getParam("TargetVoltageMode")->value();
      writeFile(LittleFS, "/TargetVoltageMode.txt", inputMessage.c_str());
      TargetVoltageMode = inputMessage.toInt();
      queueConsoleMessageF("TargetVoltageMode %s", TargetVoltageMode ? "enabled" : "disabled");
    }
    if (request->hasParam("OnOff")) {
      // NOTE: OnOff==0 already caused an early return above, so this
      // branch only runs for OnOff==1 (or any non-zero value) after
      // password validation has passed.
      foundParameter = true;
      inputMessage = request->getParam("OnOff")->value();
      writeFile(LittleFS, "/OnOff.txt", inputMessage.c_str());
      OnOff = inputMessage.toInt();
    }
    if (request->hasParam("HiLow")) {
      foundParameter = true;
      inputMessage = request->getParam("HiLow")->value();
      int newMode = inputMessage.toInt();
      if (newMode != HiLow) {
        HiLow = newMode;
        writeFile(LittleFS, "/HiLow.txt", inputMessage.c_str());
        loadCapTablesForMode(HiLow);  // swap active cap tables to match new mode
        tempPIDActive = false;        // re-seeds thermal integrator for new cap on next tick
        stateRevision++;              // force immediate CSVData echo of new table values
        queueConsoleMessageF("Charge rate mode: switched to %s", HiLow == 1 ? "Normal" : "Low");
      }
    }
    if (request->hasParam("InvertAltAmps")) {
      foundParameter = true;
      inputMessage = request->getParam("InvertAltAmps")->value();
      writeFile(LittleFS, "/InvertAltAmps.txt", inputMessage.c_str());
      InvertAltAmps = inputMessage.toInt();
    }
    if (request->hasParam("InvertBattAmps")) {
      foundParameter = true;
      inputMessage = request->getParam("InvertBattAmps")->value();
      writeFile(LittleFS, "/InvertBattAmps.txt", inputMessage.c_str());
      InvertBattAmps = inputMessage.toInt();
    }
    if (request->hasParam("MaxDuty")) {
      foundParameter = true;
      inputMessage = request->getParam("MaxDuty")->value();
      writeFile(LittleFS, "/MaxDuty.txt", inputMessage.c_str());
      MaxDuty = inputMessage.toInt();
      if (pidInitialized) {
        currentPID.SetOutputLimits(MinDuty, MaxDuty);
      }
      queueConsoleMessageF("Max Duty updated to: %d%%", MaxDuty);
    }
    if (request->hasParam("MinDuty")) {
      foundParameter = true;
      inputMessage = request->getParam("MinDuty")->value();
      writeFile(LittleFS, "/MinDuty.txt", inputMessage.c_str());
      MinDuty = inputMessage.toInt();
      if (pidInitialized) {
        currentPID.SetOutputLimits(MinDuty, MaxDuty);
      }
      queueConsoleMessageF("Min Duty updated to: %d%%", MinDuty);
    }
    if (request->hasParam("LimpHome")) {
      foundParameter = true;
      inputMessage = request->getParam("LimpHome")->value();
      writeFile(LittleFS, "/LimpHome.txt", inputMessage.c_str());
      LimpHome = inputMessage.toInt();
    }
    if (request->hasParam("VeData")) {
      foundParameter = true;
      inputMessage = request->getParam("VeData")->value();
      writeFile(LittleFS, "/VeData.txt", inputMessage.c_str());
      VeData = inputMessage.toInt();
    }
    if (request->hasParam("NMEA0183Data")) {
      foundParameter = true;
      inputMessage = request->getParam("NMEA0183Data")->value();
      writeFile(LittleFS, "/NMEA0183Data.txt", inputMessage.c_str());
      NMEA0183Data = inputMessage.toInt();
    }
    if (request->hasParam("NMEA2KData")) {
      foundParameter = true;
      inputMessage = request->getParam("NMEA2KData")->value();
      writeFile(LittleFS, "/NMEA2KData.txt", inputMessage.c_str());
      NMEA2KData = inputMessage.toInt();
    }
    if (request->hasParam("waveAmplitude")) {
      foundParameter = true;
      inputMessage = request->getParam("waveAmplitude")->value();
      writeFile(LittleFS, "/waveAmplitude.txt", inputMessage.c_str());
      waveAmplitude = inputMessage.toInt();
      if (TuningMode) tuningParamChanged = true;
    }
    if (request->hasParam("CurrentThreshold")) {
      foundParameter = true;
      inputMessage = request->getParam("CurrentThreshold")->value();
      writeFile(LittleFS, "/CurrentThreshold.txt", inputMessage.c_str());
      CurrentThreshold = inputMessage.toFloat();
    }
    if (request->hasParam("PeukertExponent")) {
      foundParameter = true;
      inputMessage = request->getParam("PeukertExponent")->value();
      PeukertExponent_scaled = (int)(inputMessage.toFloat() * 100);
      writeFile(LittleFS, "/PeukertExponent.txt", String(PeukertExponent_scaled).c_str());
    }
    if (request->hasParam("ChargeEfficiency")) {
      foundParameter = true;
      inputMessage = request->getParam("ChargeEfficiency")->value();
      writeFile(LittleFS, "/ChargeEfficiency.txt", inputMessage.c_str());
      ChargeEfficiency_scaled = (int)round(inputMessage.toFloat() * 10);  // store as % × 10
    }
    if (request->hasParam("ChargedVoltage")) {
      foundParameter = true;
      inputMessage = request->getParam("ChargedVoltage")->value();
      ChargedVoltage_Scaled = (int)(inputMessage.toFloat() * 100);
      writeFile(LittleFS, "/ChargedVoltage.txt", String(ChargedVoltage_Scaled).c_str());
    }
    if (request->hasParam("TailCurrent")) {
      foundParameter = true;
      inputMessage = request->getParam("TailCurrent")->value();
      writeFile(LittleFS, "/TailCurrent.txt", inputMessage.c_str());
      TailCurrent = inputMessage.toFloat();
    }
    if (request->hasParam("ChargedDetectionTime")) {
      foundParameter = true;
      inputMessage = request->getParam("ChargedDetectionTime")->value();
      writeFile(LittleFS, "/ChargedDetectionTime.txt", inputMessage.c_str());
      ChargedDetectionTime = inputMessage.toInt();
    }
    if (request->hasParam("IgnoreTemperature")) {
      foundParameter = true;
      inputMessage = request->getParam("IgnoreTemperature")->value();
      writeFile(LittleFS, "/IgnoreTemperature.txt", inputMessage.c_str());
      IgnoreTemperature = inputMessage.toInt();
    }
    if (request->hasParam("IgnoreRPM")) {
      foundParameter = true;
      inputMessage = request->getParam("IgnoreRPM")->value();
      writeFile(LittleFS, "/IgnoreRPM.txt", inputMessage.c_str());
      IgnoreRPM = inputMessage.toInt();
    }
    if (request->hasParam("MinRPMForField")) {
      foundParameter = true;
      inputMessage = request->getParam("MinRPMForField")->value();
      writeFile(LittleFS, "/MinRPMForField.txt", inputMessage.c_str());
      MinRPMForField = inputMessage.toInt();
    }
    if (request->hasParam("bmsLogic")) {
      foundParameter = true;
      inputMessage = request->getParam("bmsLogic")->value();
      writeFile(LittleFS, "/bmsLogic.txt", inputMessage.c_str());
      bmsLogic = inputMessage.toInt();
    }
    if (request->hasParam("bmsLogicLevelOff")) {
      foundParameter = true;
      inputMessage = request->getParam("bmsLogicLevelOff")->value();
      writeFile(LittleFS, "/bmsLogicLevelOff.txt", inputMessage.c_str());
      bmsLogicLevelOff = inputMessage.toInt();
    }
    if (request->hasParam("AlarmActivate")) {
      foundParameter = true;
      inputMessage = request->getParam("AlarmActivate")->value();
      writeFile(LittleFS, "/AlarmActivate.txt", inputMessage.c_str());
      AlarmActivate = inputMessage.toInt();
    }
    if (request->hasParam("TempAlarm")) {
      foundParameter = true;
      inputMessage = request->getParam("TempAlarm")->value();
      writeFile(LittleFS, "/TempAlarm.txt", inputMessage.c_str());
      TempAlarm = inputMessage.toInt();
    }
    if (request->hasParam("TempAlarmLow")) {
      foundParameter = true;
      inputMessage = request->getParam("TempAlarmLow")->value();
      writeFile(LittleFS, "/TempAlarmLow.txt", inputMessage.c_str());
      TempAlarmLow = inputMessage.toInt();
    }
    if (request->hasParam("VoltageAlarmHigh")) {
      foundParameter = true;
      inputMessage = request->getParam("VoltageAlarmHigh")->value();
      writeFile(LittleFS, "/VoltageAlarmHigh.txt", inputMessage.c_str());
      VoltageAlarmHigh = inputMessage.toInt();
    }
    if (request->hasParam("VoltageAlarmLow")) {
      foundParameter = true;
      inputMessage = request->getParam("VoltageAlarmLow")->value();
      writeFile(LittleFS, "/VoltageAlarmLow.txt", inputMessage.c_str());
      VoltageAlarmLow = inputMessage.toInt();
    }
    if (request->hasParam("CurrentAlarmHigh")) {
      foundParameter = true;
      inputMessage = request->getParam("CurrentAlarmHigh")->value();
      writeFile(LittleFS, "/CurrentAlarmHigh.txt", inputMessage.c_str());
      CurrentAlarmHigh = inputMessage.toInt();
    }
    if (request->hasParam("FourWay")) {
      foundParameter = true;
      inputMessage = request->getParam("FourWay")->value();
      writeFile(LittleFS, "/FourWay.txt", inputMessage.c_str());
      FourWay = inputMessage.toInt();
    }
    if (request->hasParam("RPMScalingFactor")) {
      foundParameter = true;
      inputMessage = request->getParam("RPMScalingFactor")->value();
      writeFile(LittleFS, "/RPMScalingFactor.txt", inputMessage.c_str());
      RPMScalingFactor = inputMessage.toInt();
    }
    if (request->hasParam("FieldResistance")) {
      foundParameter = true;
      inputMessage = request->getParam("FieldResistance")->value();
      writeFile(LittleFS, "/FieldResistance.txt", inputMessage.c_str());
      FieldResistance = inputMessage.toFloat();
    }
    if (request->hasParam("AlternatorCOffset")) {
      foundParameter = true;
      inputMessage = request->getParam("AlternatorCOffset")->value();
      writeFile(LittleFS, "/AlternatorCOffset.txt", inputMessage.c_str());
      AlternatorCOffset = inputMessage.toFloat();
    }
    if (request->hasParam("BatteryCOffset")) {
      foundParameter = true;
      inputMessage = request->getParam("BatteryCOffset")->value();
      writeFile(LittleFS, "/BatteryCOffset.txt", inputMessage.c_str());
      BatteryCOffset = inputMessage.toFloat();
    }
    if (request->hasParam("AmpSensorRange")) {
      foundParameter = true;
      inputMessage = request->getParam("AmpSensorRange")->value();
      writeFile(LittleFS, "/AmpSensorRange.txt", inputMessage.c_str());
      AmpSensorRange = inputMessage.toInt();
      queueConsoleMessageF("AmpSensorRange changed to: %d", AmpSensorRange);
    }
    if (request->hasParam("BatteryVoltageSource")) {
      foundParameter = true;
      inputMessage = request->getParam("BatteryVoltageSource")->value();
      writeFile(LittleFS, "/BatteryVoltageSource.txt", inputMessage.c_str());
      BatteryVoltageSource = inputMessage.toInt();
      queueConsoleMessageF("Battery voltage source changed to: %d", BatteryVoltageSource);
    }
    if (request->hasParam("R_fixed")) {
      foundParameter = true;
      inputMessage = request->getParam("R_fixed")->value();
      writeFile(LittleFS, "/R_fixed.txt", inputMessage.c_str());
      R_fixed = inputMessage.toFloat();
    }
    if (request->hasParam("Beta")) {
      foundParameter = true;
      inputMessage = request->getParam("Beta")->value();
      writeFile(LittleFS, "/Beta.txt", inputMessage.c_str());
      Beta = inputMessage.toFloat();
    }
    if (request->hasParam("T0_C")) {
      foundParameter = true;
      inputMessage = request->getParam("T0_C")->value();
      writeFile(LittleFS, "/T0_C.txt", inputMessage.c_str());
      T0_C = inputMessage.toFloat();
    }
    if (request->hasParam("TempSource")) {
      foundParameter = true;
      inputMessage = request->getParam("TempSource")->value();
      writeFile(LittleFS, "/TempSource.txt", inputMessage.c_str());
      TempSource = inputMessage.toInt();
    }
    if (request->hasParam("IgnitionOverride")) {
      foundParameter = true;
      inputMessage = request->getParam("IgnitionOverride")->value();
      writeFile(LittleFS, "/IgnitionOverride.txt", inputMessage.c_str());
      IgnitionOverride = inputMessage.toInt();
    }
    if (request->hasParam("AlarmLatchEnabled")) {
      foundParameter = true;
      inputMessage = request->getParam("AlarmLatchEnabled")->value();
      writeFile(LittleFS, "/AlarmLatchEnabled.txt", inputMessage.c_str());
      AlarmLatchEnabled = inputMessage.toInt();
    }
    if (request->hasParam("AlarmTest")) {
      foundParameter = true;
      AlarmTest = 1;  // Set the flag - don't save to file as it's momentary
      queueConsoleMessage("ALARM TEST: Initiated from web interface");
      inputMessage = "1";
    }
    if (request->hasParam("ResetAlarmLatch")) {
      foundParameter = true;
      ResetAlarmLatch = 1;  // Set the flag - don't save to file as it's momentary
      queueConsoleMessage("ALARM LATCH: Reset requested from web interface NO FUNCTION!");
      inputMessage = "1";
    }
    if (request->hasParam("absorptionCompleteTime")) {
      foundParameter = true;
      inputMessage = request->getParam("absorptionCompleteTime")->value();
      uint32_t seconds = (uint32_t)inputMessage.toInt();
      absorptionCompleteTime = seconds * 1000UL;
      writeFile(LittleFS, "/absorptionCompleteTime.txt", String(absorptionCompleteTime).c_str());
    }
    if (request->hasParam("FLOAT_DURATION")) {
      foundParameter = true;
      inputMessage = request->getParam("FLOAT_DURATION")->value();
      float hours = inputMessage.toFloat();
      int seconds = (int)(hours * 3600.0f);  // FIXED: fractional hours preserved
      FLOAT_DURATION = seconds;
      writeFile(LittleFS, "/FLOAT_DURATION.txt", String(seconds).c_str());
    }
    if (request->hasParam("UseFloat")) {
      foundParameter = true;
      inputMessage = request->getParam("UseFloat")->value();
      UseFloat = inputMessage.toInt();
      writeFile(LittleFS, "/UseFloat.txt", String(UseFloat).c_str());
    }
    if (request->hasParam("RebulkCurrent_A")) {
      foundParameter = true;
      inputMessage = request->getParam("RebulkCurrent_A")->value();
      RebulkCurrent_A = inputMessage.toFloat();
      writeFile(LittleFS, "/RebulkCurrent_A.txt", String(RebulkCurrent_A).c_str());
    }
    if (request->hasParam("VMGUseTrueWind")) {
      foundParameter = true;
      inputMessage = request->getParam("VMGUseTrueWind")->value();
      writeFile(LittleFS, "/VMGUseTrueWind.txt", inputMessage.c_str());
      VMGUseTrueWind = inputMessage.toInt();
      queueConsoleMessageF("VMG uses true wind %s", VMGUseTrueWind ? "enabled" : "disabled");
    }
    if (request->hasParam("SENSOR_UPLOAD_INTERVAL")) {
      foundParameter = true;
      inputMessage = request->getParam("SENSOR_UPLOAD_INTERVAL")->value();
      float minutes = inputMessage.toFloat();
      SENSOR_UPLOAD_INTERVAL = minutes * 60000;  // Convert minutes to milliseconds
      writeFile(LittleFS, "/SENSOR_UPLOAD_INTERVAL.txt", String(SENSOR_UPLOAD_INTERVAL).c_str());
      queueConsoleMessageF("Cloud upload interval set to %.1f minutes", minutes);
    }
    if (request->hasParam("ResetThermTemp")) {
      foundParameter = true;
      MaxTemperatureThermistor = 0;
      writeFile(LittleFS, "/MaxTemperatureThermistor.txt", "0");
      queueConsoleMessage("Max Thermistor Temp: Reset requested from web interface");
    }
    if (request->hasParam("ResetTemp")) {
      foundParameter = true;
      MaxAlternatorTemperatureF = 0;
      writeFile(LittleFS, "/MaxAlternatorTemperatureF.txt", "0");
      queueConsoleMessage("Max Alterantor Temp: Reset requested from web interface");
    }
    if (request->hasParam("ResetVoltage")) {
      foundParameter = true;
      IBVMax = 0;
      writeFile(LittleFS, "/IBVMax.txt", "0");
      queueConsoleMessage("Max Voltage: Reset requested from web interface");
    }
    if (request->hasParam("ResetCurrent")) {
      foundParameter = true;
      MeasuredAmpsMax = 0;
      writeFile(LittleFS, "/MeasuredAmpsMax.txt", "0");
      queueConsoleMessage("Max Battery Current: Reset requested from web interface");
    }
    if (request->hasParam("ResetEngineRunTime")) {
      foundParameter = true;
      EngineRunTime = 0;
      queueConsoleMessage("Engine Run Time: Reset requested from web interface");
    }
    if (request->hasParam("ResetAlternatorOnTime")) {
      foundParameter = true;
      AlternatorOnTime = 0;
      queueConsoleMessage("Alternator On Time: Reset requested from web interface");
    }
    if (request->hasParam("ResetEnergy")) {
      foundParameter = true;
      ChargedEnergy = 0;
      queueConsoleMessage("Battery Charged Energy: Reset requested from web interface");
    }
    if (request->hasParam("ResetDischargedEnergy")) {
      foundParameter = true;
      DischargedEnergy = 0;
      queueConsoleMessage("Battery Discharged Energy: Reset requested from web interface");
    }
    if (request->hasParam("ResetFuelUsed")) {
      foundParameter = true;
      AlternatorFuelUsed = 0;
      queueConsoleMessage("Fuel Used: Reset requested from web interface");
    }
    if (request->hasParam("ResetAlternatorChargedEnergy")) {
      foundParameter = true;
      AlternatorChargedEnergy = 0;
      queueConsoleMessage("Alternator Charged Energy: Reset requested from web interface");
    }
    if (request->hasParam("ResetEngineCycles")) {
      foundParameter = true;
      EngineCycles = 0;
      queueConsoleMessage("Engine Cycles: Reset requested from web interface");
    }
    if (request->hasParam("ResetRPMMax")) {
      foundParameter = true;
      RPMMax = 0;
      writeFile(LittleFS, "/RPMMax.txt", "0");
      queueConsoleMessage("Max Engine Speed: Reset requested from web interface");
    }
    if (request->hasParam("ResetSolarEnergy")) {
      foundParameter = true;
      SolarChargedEnergy = 0;
      queueConsoleMessage("Solar Energy: Reset requested from web interface");
    }
    if (request->hasParam("ResetChargeCycles")) {
      foundParameter = true;
      ChargeCycles = 0;
      queueConsoleMessage("Charge Cycles: Reset requested from web interface");
    }
    if (request->hasParam("ResetMinVoltage")) {
      foundParameter = true;
      MinVoltage = 999.0;
      writeFile(LittleFS, "/MinVoltage.txt", "999.0");
      queueConsoleMessage("Min Voltage: Reset requested from web interface");
    }
    if (request->hasParam("ResetTotalDistance")) {
      foundParameter = true;
      TotalDistance = 0;
      queueConsoleMessage("Total Distance: Reset requested from web interface");
    }
    if (request->hasParam("ResetAvgSpeed")) {
      foundParameter = true;
      AvgSpeed = 0;
      queueConsoleMessage("Average Speed: Reset requested from web interface");
    }
    if (request->hasParam("ResetMaxSpeed")) {
      foundParameter = true;
      MaxSpeed = 0;
      writeFile(LittleFS, "/MaxSpeed.txt", "0");
      queueConsoleMessage("Max Speed: Reset requested from web interface");
    }
    if (request->hasParam("ResetEngineFuelUsed")) {
      foundParameter = true;
      EngineFuelUsed = 0;
      queueConsoleMessage("Engine Fuel Used: Reset requested from web interface");
    }
    if (request->hasParam("MaximumAllowedBatteryAmps")) {
      foundParameter = true;
      inputMessage = request->getParam("MaximumAllowedBatteryAmps")->value();
      writeFile(LittleFS, "/MaximumAllowedBatteryAmps.txt", inputMessage.c_str());
      MaximumAllowedBatteryAmps = inputMessage.toInt();
    }
    if (request->hasParam("LoadDumpDtThresh")) {
      foundParameter = true;
      inputMessage = request->getParam("LoadDumpDtThresh")->value();
      LoadDumpDtThresh = inputMessage.toFloat();
      writeFile(LittleFS, "/LoadDumpDtThresh.txt", String(LoadDumpDtThresh).c_str());
    }
    if (request->hasParam("LoadDumpCurrentDrop")) {
      foundParameter = true;
      inputMessage = request->getParam("LoadDumpCurrentDrop")->value();
      LoadDumpCurrentDrop = inputMessage.toFloat();
      writeFile(LittleFS, "/LoadDumpCurrentDrop.txt", String(LoadDumpCurrentDrop).c_str());
    }
    if (request->hasParam("ManualSOCPoint")) {
      foundParameter = true;
      inputMessage = request->getParam("ManualSOCPoint")->value();
      writeFile(LittleFS, "/ManualSOCPoint.txt", inputMessage.c_str());
      ManualSOCPoint = inputMessage.toInt();
      SOC_percent = ManualSOCPoint * 100;
      CoulombCount_Ah_scaled = (ManualSOCPoint * BatteryCapacity_Ah);
      queueConsoleMessageF("SoC manually set to: %d%%", ManualSOCPoint);
    }
    if (request->hasParam("BatteryCapacity_Ah")) {
      foundParameter = true;
      inputMessage = request->getParam("BatteryCapacity_Ah")->value();
      writeFile(LittleFS, "/BatteryCapacity_Ah.txt", inputMessage.c_str());
      BatteryCapacity_Ah = inputMessage.toInt();
      PeukertRatedCurrent_A = BatteryCapacity_Ah / 20.0f;
      updateVesselInfoField("battery_capacity_ah", BatteryCapacity_Ah);
      queueConsoleMessageF("Battery capacity set to: %d Ah", BatteryCapacity_Ah);
    }
    if (request->hasParam("ShuntResistanceMicroOhm")) {
      foundParameter = true;
      inputMessage = request->getParam("ShuntResistanceMicroOhm")->value();
      writeFile(LittleFS, "/ShuntResistanceMicroOhm.txt", inputMessage.c_str());
      ShuntResistanceMicroOhm = inputMessage.toInt();
    }

    if (request->hasParam("VoltageKp")) {
      foundParameter = true;
      inputMessage = request->getParam("VoltageKp")->value();
      writeFile(LittleFS, "/VoltageKp.txt", inputMessage.c_str());
      VoltageKp = inputMessage.toFloat();
      if (CVTuningMode) cvTuningParamChanged = true;
    }

    if (request->hasParam("maxPoints")) {
      foundParameter = true;
      inputMessage = request->getParam("maxPoints")->value();
      writeFile(LittleFS, "/maxPoints.txt", inputMessage.c_str());
      maxPoints = inputMessage.toInt();
    }
    if (request->hasParam("Ymin1")) {
      foundParameter = true;
      inputMessage = request->getParam("Ymin1")->value();
      writeFile(LittleFS, "/Ymin1.txt", inputMessage.c_str());
      Ymin1 = inputMessage.toInt();
    }
    if (request->hasParam("Ymax1")) {
      foundParameter = true;
      inputMessage = request->getParam("Ymax1")->value();
      writeFile(LittleFS, "/Ymax1.txt", inputMessage.c_str());
      Ymax1 = inputMessage.toInt();
    }
    if (request->hasParam("Ymin2")) {
      foundParameter = true;
      inputMessage = request->getParam("Ymin2")->value();
      writeFile(LittleFS, "/Ymin2.txt", inputMessage.c_str());
      Ymin2 = inputMessage.toFloat();
    }
    if (request->hasParam("Ymax2")) {
      foundParameter = true;
      inputMessage = request->getParam("Ymax2")->value();
      writeFile(LittleFS, "/Ymax2.txt", inputMessage.c_str());
      Ymax2 = inputMessage.toFloat();
    }
    if (request->hasParam("Ymin3")) {
      foundParameter = true;
      inputMessage = request->getParam("Ymin3")->value();
      writeFile(LittleFS, "/Ymin3.txt", inputMessage.c_str());
      Ymin3 = inputMessage.toInt();
    }
    if (request->hasParam("Ymax3")) {
      foundParameter = true;
      inputMessage = request->getParam("Ymax3")->value();
      writeFile(LittleFS, "/Ymax3.txt", inputMessage.c_str());
      Ymax3 = inputMessage.toInt();
    }
    if (request->hasParam("Ymin4")) {
      foundParameter = true;
      inputMessage = request->getParam("Ymin4")->value();
      writeFile(LittleFS, "/Ymin4.txt", inputMessage.c_str());
      Ymin4 = inputMessage.toInt();
    }
    if (request->hasParam("Ymax4")) {
      foundParameter = true;
      inputMessage = request->getParam("Ymax4")->value();
      writeFile(LittleFS, "/Ymax4.txt", inputMessage.c_str());
      Ymax4 = inputMessage.toInt();
    }
    if (request->hasParam("AutoShuntGainCorrection")) {
      foundParameter = true;
      inputMessage = request->getParam("AutoShuntGainCorrection")->value();
      writeFile(LittleFS, "/AutoShuntGainCorrection.txt", inputMessage.c_str());
      AutoShuntGainCorrection = inputMessage.toInt();
    }
    if (request->hasParam("AutoAltCurrentZero")) {
      foundParameter = true;
      inputMessage = request->getParam("AutoAltCurrentZero")->value();
      writeFile(LittleFS, "/AutoAltCurrentZero.txt", inputMessage.c_str());
      AutoAltCurrentZero = inputMessage.toInt();
    }
    if (request->hasParam("ResetDynamicShuntGain")) {
      foundParameter = true;
      ResetDynamicShuntGain = 1;  // Set the flag - don't save to file as it's momentary
      queueConsoleMessage("SOC Gain: Reset requested from web interface");
      inputMessage = "1";
    }
    if (request->hasParam("ResetDynamicAltZero")) {
      foundParameter = true;
      ResetDynamicAltZero = 1;  // Set the flag - don't save to file as it's momentary
      queueConsoleMessage("Alt Zero: Reset requested from web interface");
      inputMessage = "1";
    }
    if (request->hasParam("WindingTempOffset")) {
      foundParameter = true;
      inputMessage = request->getParam("WindingTempOffset")->value();
      writeFile(LittleFS, "/WindingTempOffset.txt", inputMessage.c_str());
      WindingTempOffset = inputMessage.toFloat();
    }
    if (request->hasParam("displayTempUnit")) {
      foundParameter = true;
      inputMessage = request->getParam("displayTempUnit")->value();
      writeFile(LittleFS, "/displayTempUnit.txt", inputMessage.c_str());
      displayTempUnit = (uint8_t)inputMessage.toInt();
    }
    if (request->hasParam("PulleyRatio")) {
      foundParameter = true;
      inputMessage = request->getParam("PulleyRatio")->value();
      writeFile(LittleFS, "/PulleyRatio.txt", inputMessage.c_str());
      PulleyRatio = inputMessage.toFloat();
    }
    if (request->hasParam("ManualLifePercentage")) {
      foundParameter = true;
      inputMessage = request->getParam("ManualLifePercentage")->value();
      writeFile(LittleFS, "/ManualLifePercentage.txt", inputMessage.c_str());
      ManualLifePercentage = inputMessage.toInt();
      float life_fraction = ManualLifePercentage / 100.00;
      CumulativeInsulationDamage = 1.0 - life_fraction;
      CumulativeGreaseDamage = 1.0 - life_fraction;
      CumulativeBrushDamage = 1.0 - life_fraction;
      writeFile(LittleFS, "/CumulativeInsulationDamage.txt", String(CumulativeInsulationDamage, 6).c_str());
      writeFile(LittleFS, "/CumulativeGreaseDamage.txt", String(CumulativeGreaseDamage, 6).c_str());
      writeFile(LittleFS, "/CumulativeBrushDamage.txt", String(CumulativeBrushDamage, 6).c_str());
      queueConsoleMessageF("Alternator life manually set to %d%%", ManualLifePercentage);
    }
    if (request->hasParam("webgaugesinterval")) {
      foundParameter = true;
      inputMessage = request->getParam("webgaugesinterval")->value();
      int newInterval = inputMessage.toInt();
      newInterval = constrain(newInterval, 1, 10000000);
      writeFile(LittleFS, "/webgaugesinterval.txt", String(newInterval).c_str());
      webgaugesinterval = newInterval;
      queueConsoleMessageF("Update interval set to: %dms", newInterval);
    }
    if (request->hasParam("BatteryCurrentSource")) {
      foundParameter = true;
      inputMessage = request->getParam("BatteryCurrentSource")->value();
      writeFile(LittleFS, "/BatteryCurrentSource.txt", inputMessage.c_str());
      BatteryCurrentSource = inputMessage.toInt();
      queueConsoleMessageF("Battery current source changed to: %d", BatteryCurrentSource);
    }
    if (request->hasParam("totalPowerCycles")) {
      foundParameter = true;
      inputMessage = request->getParam("totalPowerCycles")->value();
      writeFile(LittleFS, "/totalPowerCycles.txt", inputMessage.c_str());
      totalPowerCycles = inputMessage.toInt();
    }
    if (request->hasParam("timeAxisModeChanging")) {
      foundParameter = true;
      inputMessage = request->getParam("timeAxisModeChanging")->value();
      writeFile(LittleFS, "/timeAxisModeChanging.txt", inputMessage.c_str());
      timeAxisModeChanging = inputMessage.toInt();
      queueConsoleMessageF("Time axis mode changed to: %s", timeAxisModeChanging ? "UNIX timestamps" : "relative time");
    }
    if (request->hasParam("plotTimeWindow")) {
      foundParameter = true;
      inputMessage = request->getParam("plotTimeWindow")->value();
      writeFile(LittleFS, "/plotTimeWindow.txt", inputMessage.c_str());
      plotTimeWindow = inputMessage.toInt();
    }
    if (request->hasParam("LatitudeNMEA") && request->hasParam("LongitudeNMEA")) {
      foundParameter = true;
      LatitudeNMEA = request->getParam("LatitudeNMEA")->value().toDouble();
      LongitudeNMEA = request->getParam("LongitudeNMEA")->value().toDouble();
      writeFile(LittleFS, "/LatitudeNMEA.txt", String(LatitudeNMEA, 6).c_str());
      writeFile(LittleFS, "/LongitudeNMEA.txt", String(LongitudeNMEA, 6).c_str());
      queueConsoleMessageF("GPS: Manual coords set to %.6f, %.6f", LatitudeNMEA, LongitudeNMEA);
      nextWeatherUpdate = 0;
    }
    if (request->hasParam("weatherModeEnabled")) {
      foundParameter = true;
      inputMessage = request->getParam("weatherModeEnabled")->value();
      writeFile(LittleFS, "/weatherModeEnabled.txt", inputMessage.c_str());
      weatherModeEnabled = inputMessage.toInt();
      queueConsoleMessageF("Weather Mode %s", weatherModeEnabled ? "enabled" : "disabled");
    }
    if (request->hasParam("UVThresholdHigh")) {
      foundParameter = true;
      inputMessage = request->getParam("UVThresholdHigh")->value();
      writeFile(LittleFS, "/UVThresholdHigh.txt", inputMessage.c_str());
      UVThresholdHigh = inputMessage.toFloat();
    }
    if (request->hasParam("SolarWatts")) {
      foundParameter = true;
      inputMessage = request->getParam("SolarWatts")->value();
      writeFile(LittleFS, "/SolarWatts.txt", inputMessage.c_str());
      SolarWatts = inputMessage.toInt();
      updateVesselInfoField("solar_watts", SolarWatts);
    }
    if (request->hasParam("performanceRatio")) {
      foundParameter = true;
      inputMessage = request->getParam("performanceRatio")->value();
      writeFile(LittleFS, "/performanceRatio.txt", inputMessage.c_str());
      performanceRatio = inputMessage.toFloat();
    }
    if (request->hasParam("TriggerWeatherUpdate")) {
      foundParameter = true;
      queueConsoleMessage("Weather: Manual update triggered");
      if (WiFi.RSSI() >= -76 && LatitudeNMEA != 0.0 && LongitudeNMEA != 0.0) {
        HttpsRequest req = { .type = HTTPS_FETCH_WEATHER };
        xQueueSend(httpsQueue, &req, 0);
      }
    }
    if (request->hasParam("ResetLearningTable")) {
      foundParameter = true;
      resetLearningTableToDefaults();
      queueConsoleMessage("Learning table reset from web - refresh to see defaults");
      inputMessage = "1";
    }
    if (request->hasParam("ClearOverheatHistory")) {
      foundParameter = true;
      clearOverheatHistoryAction();
      inputMessage = "1";
    }
    if (request->hasParam("LearningMode")) {
      foundParameter = true;
      inputMessage = request->getParam("LearningMode")->value();
      writeFile(LittleFS, "/LearningMode.txt", inputMessage.c_str());
      LearningMode = inputMessage.toInt();
    }
    if (request->hasParam("accelEnabled")) {
      foundParameter = true;
      inputMessage = request->getParam("accelEnabled")->value();
      writeFile(LittleFS, "/accelEnabled.txt", inputMessage.c_str());
      accelEnabled = inputMessage.toInt();
    }
    if (request->hasParam("CAPSIZE_THRESHOLD_DEG")) {
      foundParameter = true;
      inputMessage = request->getParam("CAPSIZE_THRESHOLD_DEG")->value();
      CAPSIZE_THRESHOLD_DEG = inputMessage.toFloat();  // NVS persistence handled by periodic save in 5_functions
    }
    if (request->hasParam("PITCHPOLE_THRESHOLD_DEG")) {
      foundParameter = true;
      inputMessage = request->getParam("PITCHPOLE_THRESHOLD_DEG")->value();
      PITCHPOLE_THRESHOLD_DEG = inputMessage.toFloat();  // NVS persistence handled by periodic save in 5_functions
    }
    if (request->hasParam("SLAM_THRESHOLD_G")) {
      foundParameter = true;
      inputMessage = request->getParam("SLAM_THRESHOLD_G")->value();
      SLAM_THRESHOLD_G = inputMessage.toFloat();  // NVS persistence handled by periodic save in 5_functions
    }
    if (request->hasParam("LearningPaused")) {
      foundParameter = true;
      inputMessage = request->getParam("LearningPaused")->value();
      writeFile(LittleFS, "/LearningPaused.txt", inputMessage.c_str());
      LearningPaused = inputMessage.toInt();
    }
    if (request->hasParam("IgnoreLearningDuringPenalty")) {
      foundParameter = true;
      inputMessage = request->getParam("IgnoreLearningDuringPenalty")->value();
      writeFile(LittleFS, "/IgnoreLearningDuringPenalty.txt", inputMessage.c_str());
      IgnoreLearningDuringPenalty = inputMessage.toInt();
    }
    if (request->hasParam("CloudFeatures")) {
      foundParameter = true;
      inputMessage = request->getParam("CloudFeatures")->value();
      writeFile(LittleFS, "/CloudFeatures.txt", inputMessage.c_str());
      CloudFeatures = inputMessage.toInt();
    }
    if (request->hasParam("LearningDryRunMode")) {
      foundParameter = true;
      inputMessage = request->getParam("LearningDryRunMode")->value();
      writeFile(LittleFS, "/LearningDryRunMode.txt", inputMessage.c_str());
      LearningDryRunMode = inputMessage.toInt();
    }
    // AutoSaveLearningTable handler — OBSOLETE REMOVE LATER
    if (request->hasParam("LearningUpwardEnabled")) {
      foundParameter = true;
      inputMessage = request->getParam("LearningUpwardEnabled")->value();
      writeFile(LittleFS, "/LearningUpwardEnabled.txt", inputMessage.c_str());
      LearningUpwardEnabled = inputMessage.toInt();
    }
    if (request->hasParam("LearningDownwardEnabled")) {
      foundParameter = true;
      inputMessage = request->getParam("LearningDownwardEnabled")->value();
      writeFile(LittleFS, "/LearningDownwardEnabled.txt", inputMessage.c_str());
      LearningDownwardEnabled = inputMessage.toInt();
    }
    if (request->hasParam("EnableNeighborLearning")) {
      foundParameter = true;
      inputMessage = request->getParam("EnableNeighborLearning")->value();
      writeFile(LittleFS, "/EnableNeighborLearning.txt", inputMessage.c_str());
      EnableNeighborLearning = inputMessage.toInt();
    }
    if (request->hasParam("EnableAmbientCorrection")) {
      foundParameter = true;
      inputMessage = request->getParam("EnableAmbientCorrection")->value();
      writeFile(LittleFS, "/EnableAmbientCorrection.txt", inputMessage.c_str());
      EnableAmbientCorrection = inputMessage.toInt();
    }
    if (request->hasParam("TuningMode")) {
      foundParameter = true;
      inputMessage = request->getParam("TuningMode")->value();
      writeFile(LittleFS, "/TuningMode.txt", inputMessage.c_str());
      TuningMode = inputMessage.toInt();
    }
    if (request->hasParam("ShowLearningDebugMessages")) {
      foundParameter = true;
      inputMessage = request->getParam("ShowLearningDebugMessages")->value();
      writeFile(LittleFS, "/ShowLearningDebugMessages.txt", inputMessage.c_str());
      ShowLearningDebugMessages = inputMessage.toInt();
    }
    if (request->hasParam("LogAllLearningEvents")) {
      foundParameter = true;
      inputMessage = request->getParam("LogAllLearningEvents")->value();
      writeFile(LittleFS, "/LogAllLearningEvents.txt", inputMessage.c_str());
      LogAllLearningEvents = inputMessage.toInt();
    }
    if (request->hasParam("hardwarePresent")) {
      foundParameter = true;
      inputMessage = request->getParam("hardwarePresent")->value();
      writeFile(LittleFS, "/hardwarePresent.txt", inputMessage.c_str());
      hardwarePresent = inputMessage.toInt();
      queueConsoleMessageF("hardwarePresent mode %s", hardwarePresent ? "enabled" : "disabled");
    }

    // RPM breakpoint and current table handlers
    bool tableChangedThisRequest = false;
    for (int i = 0; i < 10; i++) {
      char paramName[32];

      snprintf(paramName, sizeof(paramName), "rpmTableRPMPoints%d", i);
      if (request->hasParam(paramName)) {
        foundParameter = true;
        rpmTableRPMPoints[i] = request->getParam(paramName)->value().toInt();
        tableChangedThisRequest = true;
      }

      snprintf(paramName, sizeof(paramName), "rpmCapCurrentTable%d", i);
      if (request->hasParam(paramName)) {
        foundParameter = true;
        rpmCapCurrentTable[i] = request->getParam(paramName)->value().toFloat();
        tableChangedThisRequest = true;
      }

      snprintf(paramName, sizeof(paramName), "rpmCapKW%d", i);
      if (request->hasParam(paramName)) {
        foundParameter = true;
        rpmCapPowerTable[i] = request->getParam(paramName)->value().toFloat() * 1000.0f;
        tableChangedThisRequest = true;
      }

      snprintf(paramName, sizeof(paramName), "rpmMinDutyTable%d", i);
      if (request->hasParam(paramName)) {
        foundParameter = true;
        rpmMinDutyTable[i] = request->getParam(paramName)->value().toFloat();
        tableChangedThisRequest = true;
      }
    }

    if (tableChangedThisRequest) {
      // Track which RPM breakpoints changed
      learningTableUpdated = true;  // ← update the global so buildConfigPayload() sees it

      static int previousRPMPoints[RPM_TABLE_SIZE] = { 0 };
      static bool firstRun = true;

      if (firstRun) {
        for (int i = 0; i < RPM_TABLE_SIZE; i++) {
          previousRPMPoints[i] = rpmTableRPMPoints[i];
        }
        firstRun = false;
      }

      for (int i = 0; i < RPM_TABLE_SIZE; i++) {
        if (rpmTableRPMPoints[i] != previousRPMPoints[i]) {
          overheatCount[i] = 0;
          lastOverheatTime[i] = 0;
          cumulativeNoOverheatTime[i] = 0;
          learningUpCount[i] = 0;

          if (i > 0) {
            overheatCount[i - 1] = 0;
            lastOverheatTime[i - 1] = 0;
            cumulativeNoOverheatTime[i - 1] = 0;
            learningUpCount[i - 1] = 0;
          }

          queueConsoleMessageF("Learning: RPM bin %d changed - cleared affected history", i);
          previousRPMPoints[i] = rpmTableRPMPoints[i];
        }
      }

      saveUserTableEdits();
      queueConsoleMessage("Learning: Table saved to NVS");
    }

    // Fuel table handlers
    bool fuelTableUpdated = false;
    for (int i = 0; i < 10; i++) {
      char paramName[32];

      snprintf(paramName, sizeof(paramName), "fuelTableRPM%d", i);
      if (request->hasParam(paramName)) {
        foundParameter = true;
        float value = request->getParam(paramName)->value().toFloat();
        Serial.printf("DEBUG: Received %s = %.2f\n", paramName, value);
        fuelTableRPM[i] = value;
        Serial.printf("DEBUG: fuelTableRPM[%d] now = %.2f\n", i, fuelTableRPM[i]);
        fuelTableUpdated = true;
      }

      snprintf(paramName, sizeof(paramName), "fuelTableGPH%d", i);
      if (request->hasParam(paramName)) {
        foundParameter = true;
        float value = request->getParam(paramName)->value().toFloat();
        Serial.printf("DEBUG: Received %s = %.2f\n", paramName, value);
        fuelTableGPH[i] = value;
        Serial.printf("DEBUG: fuelTableGPH[%d] now = %.2f\n", i, fuelTableGPH[i]);
        fuelTableUpdated = true;
      }
    }

    if (fuelTableUpdated) {
      saveFuelTableToNVS();
      queueConsoleMessage("Fuel table updated from web interface");
    }


    if (request->hasParam("AlternatorNominalAmps")) {
      foundParameter = true;
      inputMessage = request->getParam("AlternatorNominalAmps")->value();
      writeFile(LittleFS, "/AlternatorNominalAmps.txt", inputMessage.c_str());
      AlternatorNominalAmps = inputMessage.toInt();
    }
    if (request->hasParam("LearningUpStep")) {
      foundParameter = true;
      inputMessage = request->getParam("LearningUpStep")->value();
      writeFile(LittleFS, "/LearningUpStep.txt", inputMessage.c_str());
      LearningUpStep = inputMessage.toFloat();
    }
    if (request->hasParam("LearningDownStep")) {
      foundParameter = true;
      inputMessage = request->getParam("LearningDownStep")->value();
      writeFile(LittleFS, "/LearningDownStep.txt", inputMessage.c_str());
      LearningDownStep = inputMessage.toFloat();
    }
    if (request->hasParam("AmbientTempCorrectionFactor")) {
      foundParameter = true;
      inputMessage = request->getParam("AmbientTempCorrectionFactor")->value();
      writeFile(LittleFS, "/AmbientTempCorrectionFactor.txt", inputMessage.c_str());
      AmbientTempCorrectionFactor = inputMessage.toFloat();
    }
    if (request->hasParam("xTime")) {
      foundParameter = true;
      inputMessage = request->getParam("xTime")->value();
      writeFile(LittleFS, "/xTime.txt", inputMessage.c_str());
      xTime = inputMessage.toFloat();
    }
    if (request->hasParam("MinLearningInterval")) {
      foundParameter = true;
      inputMessage = request->getParam("MinLearningInterval")->value();
      int temp = inputMessage.toInt() * 1000;  // from seconds (entry into html) to ms
      writeFile(LittleFS, "/MinLearningInterval.txt", String(temp).c_str());
      MinLearningInterval = temp;
    }
    if (request->hasParam("SetpointRiseRate")) {
      foundParameter = true;
      inputMessage = request->getParam("SetpointRiseRate")->value();
      writeFile(LittleFS, "/SetpointRiseRate.txt", inputMessage.c_str());
      SetpointRiseRate = inputMessage.toFloat();
      if (TuningMode) tuningParamChanged = true;
      if (CVTuningMode) cvTuningParamChanged = true;
    }
    if (request->hasParam("SetpointFallRate")) {
      foundParameter = true;
      inputMessage = request->getParam("SetpointFallRate")->value();
      writeFile(LittleFS, "/SetpointFallRate.txt", inputMessage.c_str());
      SetpointFallRate = inputMessage.toFloat();
      if (TuningMode) tuningParamChanged = true;
      if (CVTuningMode) cvTuningParamChanged = true;
    }
    if (request->hasParam("PIDTrackingGain")) {
      foundParameter = true;
      inputMessage = request->getParam("PIDTrackingGain")->value();
      float temp = inputMessage.toFloat();
      writeFile(LittleFS, "/PIDTrackingGain.txt", String(temp).c_str());
      PIDTrackingGain = temp;
      if (TuningMode) tuningParamChanged = true;
    }
    if (request->hasParam("SafeOperationThreshold")) {
      foundParameter = true;
      inputMessage = request->getParam("SafeOperationThreshold")->value();
      int temp = inputMessage.toInt() * 1000;  // from seconds (entry into html) to ms
      writeFile(LittleFS, "/SafeOperationThreshold.txt", String(temp).c_str());
      SafeOperationThreshold = temp;
    }
    if (request->hasParam("PidKp")) {
      foundParameter = true;
      inputMessage = request->getParam("PidKp")->value();
      writeFile(LittleFS, "/PidKp.txt", inputMessage.c_str());
      PidKp = inputMessage.toFloat();
      if (pidInitialized) {
        currentPID.SetTunings(PidKp, PidKi, PidKd);
      }
      if (TuningMode) tuningParamChanged = true;
      queueConsoleMessageF("PID Kp updated to: %.6f", PidKp);
    }
    if (request->hasParam("AbsorptionVoltage")) {
      foundParameter = true;
      inputMessage = request->getParam("AbsorptionVoltage")->value();
      AbsorptionVoltage = inputMessage.toFloat();
      writeFile(LittleFS, "/AbsorptionVoltage.txt", inputMessage.c_str());
    }
    if (request->hasParam("TargetVoltageSetpoint")) {
      foundParameter = true;
      inputMessage = request->getParam("TargetVoltageSetpoint")->value();
      TargetVoltageSetpoint = inputMessage.toFloat();
      writeFile(LittleFS, "/TargetVoltageSetpoint.txt", inputMessage.c_str());
    }

    if (request->hasParam("AbsorptionTimeoutMs")) {
      foundParameter = true;
      inputMessage = request->getParam("AbsorptionTimeoutMs")->value();
      uint32_t minutes = (uint32_t)inputMessage.toInt();
      AbsorptionTimeoutMs = minutes * 60000UL;
      writeFile(LittleFS, "/AbsorptionTimeoutMs.txt", String(AbsorptionTimeoutMs).c_str());
    }
    if (request->hasParam("bulkVoltageHoldMs")) {
      foundParameter = true;
      inputMessage = request->getParam("bulkVoltageHoldMs")->value();
      float seconds = inputMessage.toFloat();
      bulkVoltageHoldMs = (uint32_t)(seconds * 1000.0f);
      writeFile(LittleFS, "/bulkVoltageHoldMs.txt", String(bulkVoltageHoldMs).c_str());
    }
    if (request->hasParam("VoltageKi")) {
      foundParameter = true;
      inputMessage = request->getParam("VoltageKi")->value();
      VoltageKi = inputMessage.toFloat();
      writeFile(LittleFS, "/VoltageKi.txt", String(VoltageKi).c_str());
      if (CVTuningMode) cvTuningParamChanged = true;
    }
    if (request->hasParam("VoltageKd")) {
      foundParameter = true;
      inputMessage = request->getParam("VoltageKd")->value();
      VoltageKd = inputMessage.toFloat();
      writeFile(LittleFS, "/VoltageKd.txt", String(VoltageKd).c_str());
      if (CVTuningMode) cvTuningParamChanged = true;
    }
    if (request->hasParam("TempPIDKp")) {
      foundParameter = true;
      inputMessage = request->getParam("TempPIDKp")->value();
      writeFile(LittleFS, "/TempPIDKp.txt", inputMessage.c_str());
      TempPIDKp = inputMessage.toFloat();
      tempPID.SetTunings(TempPIDKp, TempPIDKi, 0.0);
      if (ThermalTuningMode) thermalTuningParamChanged = true;
      queueConsoleMessageF("Temp PID Kp updated to: %.6f", TempPIDKp);
    }
    if (request->hasParam("TempPIDKi")) {
      foundParameter = true;
      inputMessage = request->getParam("TempPIDKi")->value();
      writeFile(LittleFS, "/TempPIDKi.txt", inputMessage.c_str());
      TempPIDKi = inputMessage.toFloat();
      tempPID.SetTunings(TempPIDKp, TempPIDKi, 0.0);
      if (ThermalTuningMode) thermalTuningParamChanged = true;
      queueConsoleMessageF("Temp PID Ki updated to: %.6f", TempPIDKi);
    }
    if (request->hasParam("ThermalLookaheadSec")) {
      foundParameter = true;
      inputMessage = request->getParam("ThermalLookaheadSec")->value();
      ThermalLookaheadSec = clamp_f(inputMessage.toFloat(), 0.0f, 300.0f);
      writeFile(LittleFS, "/ThermalLookaheadSec.txt", String(ThermalLookaheadSec, 1).c_str());
      if (ThermalTuningMode) thermalTuningParamChanged = true;
      queueConsoleMessageF("ThermalLookaheadSec set to: %.1f s", ThermalLookaheadSec);
    }
    if (request->hasParam("TempPIDIntervalMs")) {
      foundParameter = true;
      inputMessage = request->getParam("TempPIDIntervalMs")->value();
      writeFile(LittleFS, "/TempPIDIntervalMs.txt", inputMessage.c_str());
      TempPIDIntervalMs = inputMessage.toInt();
      if (ThermalTuningMode) thermalTuningParamChanged = true;
      queueConsoleMessageF("Temp PID interval updated to: %d ms", TempPIDIntervalMs);
    }
    if (request->hasParam("TempPIDFilterAlpha")) {
      foundParameter = true;
      inputMessage = request->getParam("TempPIDFilterAlpha")->value();
      writeFile(LittleFS, "/TempPIDFilterAlpha.txt", inputMessage.c_str());
      TempPIDFilterAlpha = inputMessage.toFloat();
      if (ThermalTuningMode) thermalTuningParamChanged = true;
      queueConsoleMessageF("Temp PID filter alpha updated to: %.3f", TempPIDFilterAlpha);
    }
    if (request->hasParam("PidKi")) {
      foundParameter = true;
      inputMessage = request->getParam("PidKi")->value();
      writeFile(LittleFS, "/PidKi.txt", inputMessage.c_str());
      PidKi = inputMessage.toFloat();
      if (pidInitialized) {
        currentPID.SetTunings(PidKp, PidKi, PidKd);
      }
      if (TuningMode) tuningParamChanged = true;
      queueConsoleMessageF("PID Ki updated to: %.6f", PidKi);
    }
    if (request->hasParam("PidKd")) {
      foundParameter = true;
      inputMessage = request->getParam("PidKd")->value();
      writeFile(LittleFS, "/PidKd.txt", inputMessage.c_str());
      PidKd = inputMessage.toFloat();
      if (pidInitialized) {
        currentPID.SetTunings(PidKp, PidKi, PidKd);
      }
      if (TuningMode) tuningParamChanged = true;
      queueConsoleMessageF("PID Kd updated to: %.6f", PidKd);
    }
    if (request->hasParam("PidSampleDivisor")) {
      foundParameter = true;
      inputMessage = request->getParam("PidSampleDivisor")->value();
      writeFile(LittleFS, "/PidSampleDivisor.txt", inputMessage.c_str());
      PidSampleDivisor = inputMessage.toInt();
      if (TuningMode) tuningParamChanged = true;
    }
    if (request->hasParam("LearningSettlingPeriod")) {
      foundParameter = true;
      inputMessage = request->getParam("LearningSettlingPeriod")->value();
      int temp = inputMessage.toInt() * 1000;  // Convert seconds to ms
      writeFile(LittleFS, "/LearningSettlingPeriod.txt", String(temp).c_str());
      LearningSettlingPeriod = temp;
    }
    if (request->hasParam("LearningRPMChangeThreshold")) {
      foundParameter = true;
      inputMessage = request->getParam("LearningRPMChangeThreshold")->value();
      writeFile(LittleFS, "/LearningRPMChangeThreshold.txt", inputMessage.c_str());
      LearningRPMChangeThreshold = inputMessage.toInt();
    }
    if (request->hasParam("LearningTempHysteresis")) {
      foundParameter = true;
      inputMessage = request->getParam("LearningTempHysteresis")->value();
      writeFile(LittleFS, "/LearningTempHysteresis.txt", inputMessage.c_str());
      LearningTempHysteresis = inputMessage.toInt();
    }
    if (request->hasParam("MaxTableValue")) {
      foundParameter = true;
      inputMessage = request->getParam("MaxTableValue")->value();
      writeFile(LittleFS, "/MaxTableValue.txt", inputMessage.c_str());
      MaxTableValue = inputMessage.toFloat();
      HardOCTripAmps = MaxTableValue + 10.0f;  // always 10A above current limit
      queueConsoleMessageF("Alternator current limit set to %.1fA — OC trip threshold: %.1fA", MaxTableValue, HardOCTripAmps);
    }
    // MinTableValue handler — OBSOLETE REMOVE LATER
    if (request->hasParam("MaxPenaltyPercent")) {
      foundParameter = true;
      inputMessage = request->getParam("MaxPenaltyPercent")->value();
      writeFile(LittleFS, "/MaxPenaltyPercent.txt", inputMessage.c_str());
      MaxPenaltyPercent = inputMessage.toFloat();
    }
    if (request->hasParam("MaxPenaltyDuration")) {
      foundParameter = true;
      inputMessage = request->getParam("MaxPenaltyDuration")->value();
      int temp = inputMessage.toInt() * 1000;
      writeFile(LittleFS, "/MaxPenaltyDuration.txt", String(temp).c_str());
      MaxPenaltyDuration = temp;
    }
    if (request->hasParam("NeighborLearningFactor")) {
      foundParameter = true;
      inputMessage = request->getParam("NeighborLearningFactor")->value();
      writeFile(LittleFS, "/NeighborLearningFactor.txt", inputMessage.c_str());
      NeighborLearningFactor = inputMessage.toFloat();
    }
    if (request->hasParam("yyMax")) {
      foundParameter = true;
      inputMessage = request->getParam("yyMax")->value();
      writeFile(LittleFS, "/yyMax.txt", inputMessage.c_str());
      yyMax = inputMessage.toInt();
    }
    if (request->hasParam("LearningMemoryDuration")) {
      foundParameter = true;
      inputMessage = request->getParam("LearningMemoryDuration")->value();
      writeFile(LittleFS, "/LearningMemoryDuration.txt", inputMessage.c_str());
      LearningMemoryDuration = inputMessage.toInt();
    }
    // LearningTableSaveInterval handler — OBSOLETE REMOVE LATER
    if (request->hasParam("SetpointRampRate")) {
      foundParameter = true;
      inputMessage = request->getParam("SetpointRampRate")->value();
      writeFile(LittleFS, "/SetpointRampRate.txt", inputMessage.c_str());
      SetpointRampRate = inputMessage.toFloat();
      queueConsoleMessageF("Setpoint ramp rate set to: %.1f A/sec", SetpointRampRate);
    }
    if (request->hasParam("DutyRampRate")) {
      foundParameter = true;
      inputMessage = request->getParam("DutyRampRate")->value();
      writeFile(LittleFS, "/DutyRampRate.txt", inputMessage.c_str());
      DutyRampRate = inputMessage.toFloat();
      if (TuningMode) tuningParamChanged = true;
      queueConsoleMessageF("Duty ramp rate set to: %.1f %%/sec", DutyRampRate);
    }
    if (request->hasParam("DutySlowRampRate")) {
      foundParameter = true;
      inputMessage = request->getParam("DutySlowRampRate")->value();
      writeFile(LittleFS, "/DutySlowRampRate.txt", inputMessage.c_str());
      DutySlowRampRate = inputMessage.toFloat();
      queueConsoleMessageF("Shutdown slow ramp rate set to: %.2f %%/s", DutySlowRampRate);
    }
    if (request->hasParam("ShutdownPhase2HoldMs")) {
      foundParameter = true;
      inputMessage = request->getParam("ShutdownPhase2HoldMs")->value();
      uint32_t ms = (uint32_t)inputMessage.toInt();
      writeFile(LittleFS, "/ShutdownPhase2HoldMs.txt", String(ms).c_str());
      ShutdownPhase2HoldMs = ms;
      queueConsoleMessageF("Shutdown phase 2 hold set to: %u ms", ShutdownPhase2HoldMs);
    }
    if (request->hasParam("SettleTimeBeforeCut")) {
      foundParameter = true;
      inputMessage = request->getParam("SettleTimeBeforeCut")->value();
      writeFile(LittleFS, "/SettleTimeBeforeCut.txt", inputMessage.c_str());
      SettleTimeBeforeCut = inputMessage.toInt();
      queueConsoleMessageF("Settle time before cut set to: %d ms", SettleTimeBeforeCut);
    }
    if (request->hasParam("TempWarnExcess")) {
      foundParameter = true;
      inputMessage = request->getParam("TempWarnExcess")->value();
      writeFile(LittleFS, "/TempWarnExcess.txt", inputMessage.c_str());
      TempWarnExcess = inputMessage.toFloat();
      queueConsoleMessageF("Temp warning threshold set to: +%.1f°F above limit", TempWarnExcess);
    }
    if (request->hasParam("TempCritExcess")) {
      foundParameter = true;
      inputMessage = request->getParam("TempCritExcess")->value();
      writeFile(LittleFS, "/TempCritExcess.txt", inputMessage.c_str());
      TempCritExcess = inputMessage.toFloat();
      queueConsoleMessageF("Temp critical threshold set to: +%.1f°F above limit", TempCritExcess);
    }
    if (request->hasParam("TempSustainedTimeout")) {
      foundParameter = true;
      inputMessage = request->getParam("TempSustainedTimeout")->value();
      int temp = inputMessage.toInt() * 1000;  // user enters seconds
      writeFile(LittleFS, "/TempSustainedTimeout.txt", String(temp).c_str());
      TempSustainedTimeout = temp;
      queueConsoleMessageF("Temp sustained timeout set to: %d seconds", inputMessage.toInt());
    }
    if (request->hasParam("VoltageSpikeMargin")) {
      foundParameter = true;
      inputMessage = request->getParam("VoltageSpikeMargin")->value();
      writeFile(LittleFS, "/VoltageSpikeMargin.txt", inputMessage.c_str());
      VoltageSpikeMargin = inputMessage.toFloat();
      queueConsoleMessageF("Voltage spike margin set to: %.2fV above bulk", VoltageSpikeMargin);
    }
    if (request->hasParam("HardOCDebounceMs")) {
      foundParameter = true;
      inputMessage = request->getParam("HardOCDebounceMs")->value();
      writeFile(LittleFS, "/HardOCDebounceMs.txt", inputMessage.c_str());
      HardOCDebounceMs = (uint32_t)inputMessage.toInt();
      queueConsoleMessageF("Overcurrent trip debounce set to: %ums", HardOCDebounceMs);
    }
    if (request->hasParam("WarmupRampRate")) {
      foundParameter = true;
      inputMessage = request->getParam("WarmupRampRate")->value();
      WarmupRampRate = max(0.0f, inputMessage.toFloat());
      writeFile(LittleFS, "/WarmupRampRate.txt", String(WarmupRampRate, 2).c_str());
      queueConsoleMessageF("Warmup ramp rate set to: %.2f A/s", WarmupRampRate);
    }
    if (request->hasParam("IExcessK")) {
      foundParameter = true;
      inputMessage = request->getParam("IExcessK")->value();
      IExcessK = inputMessage.toFloat();
      writeFile(LittleFS, "/IExcessK.txt", String(IExcessK, 1).c_str());
      queueConsoleMessageF("IExcess threshold set to: %.1fA above setpoint", IExcessK);
      if (CVTuningMode) cvTuningParamChanged = true;
    }
    if (request->hasParam("IExcessN")) {
      foundParameter = true;
      inputMessage = request->getParam("IExcessN")->value();
      IExcessN = (int)inputMessage.toInt();
      writeFile(LittleFS, "/IExcessN.txt", String(IExcessN).c_str());
      queueConsoleMessageF("IExcess persistence set to: %d ticks", IExcessN);
      if (CVTuningMode) cvTuningParamChanged = true;
    }
    if (request->hasParam("IExcessKBleed")) {
      foundParameter = true;
      inputMessage = request->getParam("IExcessKBleed")->value();
      IExcessKBleed = inputMessage.toFloat();
      writeFile(LittleFS, "/IExcessKBleed.txt", String(IExcessKBleed, 2).c_str());
      queueConsoleMessageF("K_bleed set to: %.2f A/s per A", IExcessKBleed);
      if (CVTuningMode) cvTuningParamChanged = true;
    }
    if (request->hasParam("AwBleedRate")) {
      foundParameter = true;
      inputMessage = request->getParam("AwBleedRate")->value();
      AwBleedRate = inputMessage.toFloat();
      writeFile(LittleFS, "/AwBleedRate.txt", String(AwBleedRate, 1).c_str());
      queueConsoleMessageF("AW bleed rate set to: %.1f A/s", AwBleedRate);
      if (CVTuningMode) cvTuningParamChanged = true;
    }
    if (request->hasParam("AwRecoverRate")) {
      foundParameter = true;
      inputMessage = request->getParam("AwRecoverRate")->value();
      AwRecoverRate = inputMessage.toFloat();
      writeFile(LittleFS, "/AwRecoverRate.txt", String(AwRecoverRate, 1).c_str());
      queueConsoleMessageF("AW recovery rate set to: %.1f A/s", AwRecoverRate);
      if (CVTuningMode) cvTuningParamChanged = true;
    }
    if (request->hasParam("AwSeedProtectMs")) {
      foundParameter = true;
      inputMessage = request->getParam("AwSeedProtectMs")->value();
      AwSeedProtectMs = (uint16_t)constrain(inputMessage.toInt(), 0, 2000);
      writeFile(LittleFS, "/AwSeedProtectMs.txt", String(AwSeedProtectMs).c_str());
      queueConsoleMessageF("AW seed protect window set to: %u ms", (unsigned)AwSeedProtectMs);
      if (CVTuningMode) cvTuningParamChanged = true;
    }
    if (request->hasParam("KSoft")) {
      foundParameter = true;
      inputMessage = request->getParam("KSoft")->value();
      KSoft = inputMessage.toFloat();
      writeFile(LittleFS, "/KSoft.txt", String(KSoft, 1).c_str());
      queueConsoleMessageF("KSoft set to: %.1f A/V", KSoft);
      if (CVTuningMode) cvTuningParamChanged = true;
    }
    if (request->hasParam("KHard")) {
      foundParameter = true;
      inputMessage = request->getParam("KHard")->value();
      KHard = inputMessage.toFloat();
      writeFile(LittleFS, "/KHard.txt", String(KHard, 1).c_str());
      queueConsoleMessageF("KHard set to: %.1f A/V", KHard);
      if (CVTuningMode) cvTuningParamChanged = true;
    }
    if (request->hasParam("IExcessReseedFrac")) {
      foundParameter = true;
      inputMessage = request->getParam("IExcessReseedFrac")->value();
      IExcessReseedFrac = inputMessage.toFloat();
      writeFile(LittleFS, "/IExcessReseedFrac.txt", String(IExcessReseedFrac, 2).c_str());
      queueConsoleMessageF("IExcess reseed fraction set to: %.2f", IExcessReseedFrac);
      if (CVTuningMode) cvTuningParamChanged = true;
    }
    if (request->hasParam("CVTuningMode")) {
      foundParameter = true;
      inputMessage = request->getParam("CVTuningMode")->value();
      writeFile(LittleFS, "/CVTuningMode.txt", inputMessage.c_str());
      CVTuningMode = inputMessage.toInt();
    }
    if (request->hasParam("cvWaveAmplitudeV")) {
      foundParameter = true;
      inputMessage = request->getParam("cvWaveAmplitudeV")->value();
      cvWaveAmplitudeV = inputMessage.toFloat();
      writeFile(LittleFS, "/cvWaveAmplitudeV.txt", String(cvWaveAmplitudeV, 2).c_str());
      if (CVTuningMode) cvTuningParamChanged = true;
    }
    if (request->hasParam("cvWavePeriodSec")) {
      foundParameter = true;
      inputMessage = request->getParam("cvWavePeriodSec")->value();
      cvWavePeriodSec = inputMessage.toInt();
      writeFile(LittleFS, "/cvWavePeriodSec.txt", String(cvWavePeriodSec).c_str());
      if (CVTuningMode) cvTuningParamChanged = true;
    }
    if (request->hasParam("cvKOvershoot")) {
      foundParameter = true;
      inputMessage = request->getParam("cvKOvershoot")->value();
      cvKOvershoot = inputMessage.toFloat();
      writeFile(LittleFS, "/cvKOvershoot.txt", String(cvKOvershoot, 1).c_str());
    }
    if (request->hasParam("cvConsecutiveReads")) {
      foundParameter = true;
      inputMessage = request->getParam("cvConsecutiveReads")->value();
      cvConsecutiveReads = (uint8_t)constrain(inputMessage.toInt(), 1, 20);
      writeFile(LittleFS, "/cvConsecutiveReads.txt", String(cvConsecutiveReads).c_str());
    }
    if (request->hasParam("ThermalTuningMode")) {
      foundParameter = true;
      inputMessage = request->getParam("ThermalTuningMode")->value();
      writeFile(LittleFS, "/ThermalTuningMode.txt", inputMessage.c_str());
      ThermalTuningMode = inputMessage.toInt();
    }
    if (request->hasParam("thermalWaveLowF")) {
      foundParameter = true;
      inputMessage = request->getParam("thermalWaveLowF")->value();
      thermalWaveLowF = inputMessage.toFloat();
      writeFile(LittleFS, "/thermalWaveLowF.txt", String(thermalWaveLowF, 1).c_str());
      if (ThermalTuningMode) thermalTuningParamChanged = true;
    }
    if (request->hasParam("thermalWaveHighF")) {
      foundParameter = true;
      inputMessage = request->getParam("thermalWaveHighF")->value();
      thermalWaveHighF = inputMessage.toFloat();
      writeFile(LittleFS, "/thermalWaveHighF.txt", String(thermalWaveHighF, 1).c_str());
      if (ThermalTuningMode) thermalTuningParamChanged = true;
    }
    if (request->hasParam("thermalWaveHalfPeriodMin")) {
      foundParameter = true;
      inputMessage = request->getParam("thermalWaveHalfPeriodMin")->value();
      thermalWaveHalfPeriodMin = inputMessage.toFloat();
      writeFile(LittleFS, "/thermalWaveHalfPeriodMin.txt", String(thermalWaveHalfPeriodMin, 1).c_str());
      if (ThermalTuningMode) thermalTuningParamChanged = true;
    }
    if (request->hasParam("thermalKOvershoot")) {
      foundParameter = true;
      inputMessage = request->getParam("thermalKOvershoot")->value();
      thermalKOvershoot = inputMessage.toFloat();
      writeFile(LittleFS, "/thermalKOvershoot.txt", String(thermalKOvershoot, 1).c_str());
    }
    if (request->hasParam("thermalKUndershoot")) {
      foundParameter = true;
      inputMessage = request->getParam("thermalKUndershoot")->value();
      thermalKUndershoot = inputMessage.toFloat();
      writeFile(LittleFS, "/thermalKUndershoot.txt", String(thermalKUndershoot, 1).c_str());
    }
    if (request->hasParam("thermalSettleThreshF")) {
      foundParameter = true;
      inputMessage = request->getParam("thermalSettleThreshF")->value();
      thermalSettleThreshF = inputMessage.toFloat();
      writeFile(LittleFS, "/thermalSettleThreshF.txt", String(thermalSettleThreshF, 1).c_str());
    }
    if (request->hasParam("thermalConsecutiveReads")) {
      foundParameter = true;
      inputMessage = request->getParam("thermalConsecutiveReads")->value();
      thermalConsecutiveReads = (uint8_t)constrain(inputMessage.toInt(), 1, 20);
      writeFile(LittleFS, "/thermalConsecutiveReads.txt", String(thermalConsecutiveReads).c_str());
    }
    if (request->hasParam("VoltageDisagreeThreshold")) {
      foundParameter = true;
      inputMessage = request->getParam("VoltageDisagreeThreshold")->value();
      writeFile(LittleFS, "/VoltageDisagreeThreshold.txt", inputMessage.c_str());
      VoltageDisagreeThreshold = inputMessage.toFloat();
      queueConsoleMessageF("Voltage disagreement threshold set to: %.2fV", VoltageDisagreeThreshold);
    }
    if (request->hasParam("VoltageDisagreeTimeout")) {
      foundParameter = true;
      inputMessage = request->getParam("VoltageDisagreeTimeout")->value();
      int temp = inputMessage.toInt() * 1000;  // user enters seconds
      writeFile(LittleFS, "/VoltageDisagreeTimeout.txt", String(temp).c_str());
      VoltageDisagreeTimeout = temp;
      queueConsoleMessageF("Voltage disagreement timeout set to: %d seconds", inputMessage.toInt());
    }
    if (request->hasParam("VoltageTrimLimit")) {
      foundParameter = true;
      inputMessage = request->getParam("VoltageTrimLimit")->value();
      writeFile(LittleFS, "/VoltageTrimLimit.txt", inputMessage.c_str());
      VoltageTrimLimit = inputMessage.toFloat();
    }
    if (request->hasParam("VoltageLoopInterval")) {
      foundParameter = true;
      inputMessage = request->getParam("VoltageLoopInterval")->value();
      writeFile(LittleFS, "/VoltageLoopInterval.txt", inputMessage.c_str());
      VoltageLoopInterval = inputMessage.toInt();
    }
    if (request->hasParam("FIELD_COLLAPSE_DELAY")) {
      foundParameter = true;
      inputMessage = request->getParam("FIELD_COLLAPSE_DELAY")->value();
      int temp = inputMessage.toInt() * 1000;
      writeFile(LittleFS, "/FIELD_COLLAPSE_DELAY.txt", String(temp).c_str());
      FIELD_COLLAPSE_DELAY = temp;
    }
    if (request->hasParam("EffXMin")) {
      foundParameter = true;
      inputMessage = request->getParam("EffXMin")->value();
      writeFile(LittleFS, "/EffXMin.txt", inputMessage.c_str());
      EffXMin = inputMessage.toFloat();
    }
    if (request->hasParam("EffXMax")) {
      foundParameter = true;
      inputMessage = request->getParam("EffXMax")->value();
      writeFile(LittleFS, "/EffXMax.txt", inputMessage.c_str());
      EffXMax = inputMessage.toFloat();
    }
    if (request->hasParam("EffYMin")) {
      foundParameter = true;
      inputMessage = request->getParam("EffYMin")->value();
      writeFile(LittleFS, "/EffYMin.txt", inputMessage.c_str());
      EffYMin = inputMessage.toFloat();
    }
    if (request->hasParam("EffYMax")) {
      foundParameter = true;
      inputMessage = request->getParam("EffYMax")->value();
      writeFile(LittleFS, "/EffYMax.txt", inputMessage.c_str());
      EffYMax = inputMessage.toFloat();
    }
    if (request->hasParam("ResetPerfCounters")) {
      foundParameter = true;
      // Function timing — session worsts
      ft_ReadAnalogInputs.worstSession = 0;
      ft_rai_total.worstSession = 0;
      ft_rai_ina228.worstSession = 0;
      ft_rai_ads_state.worstSession = 0;
      ft_rai_bmp_state.worstSession = 0;
      ft_rai_imu.worstSession = 0;
      ft_AdjustFieldLearnMode.worstSession = 0;
      ft_uploadSensorHistory.worstSession = 0;
      ft_uploadBufferedRecords.worstSession = 0;
      ft_buildConfigPayload.worstSession = 0;
      ft_ReadVEData.worstSession = 0;
      ft_saveNVSData.worstSession = 0;
      ft_FlushFileWriteQueue.worstSession = 0;
      VeTime2 = 0;
      // CPU load maxes
      cpuLoadCore0Max = 0;
      cpuLoadCore1Max = 0;
      // Session max loop time (this session only — last session is preserved)
      MaximumLoopTime = 0;
      // CH1 all-time accumulators
      ch1AtWorst = 0;
      ch1AtOver2x = 0;
      ch1AtSum = 0;
      ch1AtCount = 0;
      ch1_worst_at = 0;
      ch1_over2x_at = 0;
      ch1_avg_at = 0.0f;
      ch1_n_at = 0;
      // CH1 10s ring buffer
      ch1Head = 0;
      ch1Count = 0;
      ch1_avg_10s = 0.0f;
      ch1_worst_10s = 0;
      ch1_over2x_10s = 0;
      ch1_n_10s = 0;
      // CH1 2m bucket ring
      ch1BktHead = 0;
      ch1BktCount = 0;
      ch1BktStart = millis();
      ch1_avg_2m = 0.0f;
      ch1_worst_2m = 0;
      ch1_over2x_2m = 0;
      ch1_n_2m = 0;
      // CH1 1s mini-buckets
      ch1Bkt1sCount = 0;
      ch1Bkt1sHead = 0;
      ch1Bkt1sCurrent = { 0, 0, 0, 0 };
      ch1Bkt1sStart = millis();
      // Reset interval baseline so first post-reset sample doesn't carry stale timestamp gap
      ch1HasPrev = false;
      queueConsoleMessage("Peak counters reset from web interface");
    }

    if (request->hasParam("ResetAccelSession")) {
      foundParameter = true;
      imu_total_samples_accel      = 0;
      imu_total_samples_gyro       = 0;
      imuRingBuffer->accel_dropped = 0;
      imuRingBuffer->gyro_dropped  = 0;
      imu_fifo_overrun_count       = 0;
      imu_i2c_error_count          = 0;
      imu_unknown_tag_count        = 0;
      imu_slam_count               = 0;
      imu_slam_peak_max            = 0;
      imuWindow->slam_count        = 0;
      imuWindow->slam_peak_max     = 0;
      queueConsoleMessage("Accel session stats reset from web interface");
    }

    if (request->hasParam("ResetAccelLifetime")) {
      foundParameter = true;
      imu_heel_max_lifetime        = 0;
      imu_pitch_max_lifetime       = 0;
      imu_slam_peak_lifetime       = 0;
      imu_slam_count_lifetime      = 0;
      imu_capsize_count            = 0;
      imu_pitchpole_count          = 0;
      // Reset shadow vars so saveNVSData() sees a change and writes 0s to NVS
      prev_imu_heel_max_lifetime   = -1;
      prev_imu_pitch_max_lifetime  = -1;
      prev_imu_slam_peak_lifetime  = -1;
      prev_imu_slam_count_lifetime = UINT32_MAX;
      prev_imu_capsize_count       = UINT32_MAX;
      prev_imu_pitchpole_count     = UINT32_MAX;
      queueConsoleMessage("Accel lifetime stats reset from web interface");
    }

    if (foundParameter) {
      stateRevision++;  // Increment whenever any setting changed
    }
    if (!foundParameter) {
      inputMessage = "No message sent, the request_hasParam found no match";
    }
    request->send(200, "text/plain", inputMessage);
  });

  server.on("/setPassword", HTTP_POST, [](AsyncWebServerRequest *request) {
    if (!request->hasParam("password", true) || !request->hasParam("newpassword", true)) {
      request->send(400, "text/plain", "Missing fields");
      return;
    }
    String password = request->getParam("password", true)->value();
    String newPassword = request->getParam("newpassword", true)->value();
    password.trim();
    newPassword.trim();
    if (newPassword.length() == 0) {
      request->send(400, "text/plain", "Empty new password");
      return;
    }
    // Validate the existing admin password first
    if (!validatePassword(password.c_str())) {
      request->send(403, "text/plain", "FAIL");  // Wrong password
      return;
    }
    // Create the hash first (before taking lock)
    char hash[65] = { 0 };
    sha256(newPassword.c_str(), hash);
    // Save the plaintext password and hash
    fsTakeLock();
    File plainFile = LittleFS.open("/password.txt", "w");
    if (plainFile) {
      plainFile.println(newPassword);
      plainFile.close();
    }
    File file = LittleFS.open("/password.hash", "w");
    if (!file) {
      fsReleaseLock();
      request->send(500, "text/plain", "Failed to open password file");
      return;
    }
    file.println(hash);
    file.close();
    fsReleaseLock();
    // Update RAM copy
    strncpy(requiredPassword, newPassword.c_str(), sizeof(requiredPassword) - 1);
    strncpy(storedPasswordHash, hash, sizeof(storedPasswordHash) - 1);
    request->send(200, "text/plain", "OK");
  });
  server.on("/checkPassword", HTTP_POST, [](AsyncWebServerRequest *request) {
    if (!request->hasParam("password", true)) {
      request->send(400, "text/plain", "Missing password");
      return;
    }
    String password = request->getParam("password", true)->value();
    password.trim();
    if (validatePassword(password.c_str())) {
      request->send(200, "text/plain", "OK");
    } else {
      request->send(403, "text/plain", "FAIL");
    }
  });

  // Explicit routes for all static web assets — served directly, never hit onNotFound
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (!serveCachedGz(request, "/index.html", "text/html"))
      request->send(webFS, "/index.html", "text/html");
  });
  server.on("/index.html", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (!serveCachedGz(request, "/index.html", "text/html"))
      request->send(webFS, "/index.html", "text/html");
  });
  server.on("/styles.css", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (!serveCachedGz(request, "/styles.css", "text/css"))
      request->send(webFS, "/styles.css", "text/css");
  });
  server.on("/uPlot.min.css", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (!serveCachedGz(request, "/uPlot.min.css", "text/css"))
      request->send(webFS, "/uPlot.min.css", "text/css");
  });
  server.on("/uPlot.iife.min.js", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (!serveCachedGz(request, "/uPlot.iife.min.js", "application/javascript"))
      request->send(webFS, "/uPlot.iife.min.js", "application/javascript");
  });
  server.on("/script.js", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (!serveCachedGz(request, "/script.js", "application/javascript"))
      request->send(webFS, "/script.js", "application/javascript");
  });
  server.onNotFound([](AsyncWebServerRequest *request) {
    if (WiFi.getMode() == WIFI_AP || WiFi.getMode() == WIFI_AP_STA) {
      sendWifiConfigPortal(request);
    } else {
      request->send(404, "text/plain", "Not Found");
    }
  });
  // Setup event source for real-time updates
  events.onConnect([](AsyncEventSourceClient *client) {
    if (client->lastId()) {
      Serial.printf("Client reconnected! Last message ID that it got is: %u\n", client->lastId());
    }
    client->send("hello!", NULL, millis(), 10000);
  });
  // Diagnostic endpoint to check partition and version
  server.on("/debug", HTTP_GET, [](AsyncWebServerRequest *request) {
    const esp_partition_t *running = esp_ota_get_running_partition();
    char out[192];
    snprintf(out, sizeof(out),
             "Partition: %s\nVersion: %s\nFree heap: %lu\n",
             (running && running->label) ? running->label : "unknown",
             FIRMWARE_VERSION,
             (unsigned long)ESP.getFreeHeap());
    request->send(200, "text/plain", out);
  });

  // Cloud Features
  server.on("/checkRegistration", HTTP_POST, [](AsyncWebServerRequest *request) {
    // Check password
    if (!request->hasParam("password", true) || strcmp(request->getParam("password", true)->value().c_str(), requiredPassword) != 0) {
      request->send(403, "text/plain", "Forbidden");
      return;
    }

    String deviceUID = String(device_id_hex);
    Serial.println("=== checkRegistration called ===");
    Serial.println("isRegistered: " + String(isRegistered));
    Serial.println("authToken: " + authToken);
    Serial.println("Current deviceUID: " + deviceUID);

    if (!isRegistered) {
      request->send(200, "application/json", "{\"registered\":false}");
      return;
    }

    // Validate token with Supabase (send only token, not device_uid)
    HTTPClient http;
    char url[256];
    snprintf(url, sizeof(url), "%s/functions/v1/validate-token", SUPABASE_URL);
    http.begin(url);
    http.addHeader("Content-Type", "application/json");
    char authHeader[512];
    snprintf(authHeader, sizeof(authHeader), "Bearer %s", SUPABASE_ANON_KEY);
    http.addHeader("Authorization", authHeader);

    DynamicJsonDocument payloadDoc(256);
    payloadDoc["token"] = authToken;
    String payload;
    serializeJson(payloadDoc, payload);
    int httpCode = http.POST(payload);
    String response = http.getString();
    http.end();

    Serial.println("Validate-token HTTP code: " + String(httpCode));
    Serial.println("Validate-token response: " + response);
    if (httpCode == 200) {
      queueConsoleMessage("Cloud: profile verified");
    } else if (httpCode == 401) {
      queueConsoleMessage("Cloud: ready to register");
    }
    if (httpCode == 200) {
      // Parse response to check device_uid match
      DynamicJsonDocument doc(2048);
      DeserializationError error = deserializeJson(doc, response.c_str(), response.length());
      if (error) {
        Serial.println("JSON parse error");
        request->send(200, "application/json", "{\"registered\":false,\"error\":\"parse_failed\"}");
        return;
      }

      String tokenDeviceUID = doc["device_uid"].as<String>();
      Serial.println("Token belongs to device: " + tokenDeviceUID);

      // Check if token's device matches current device
      if (tokenDeviceUID != deviceUID) {
        Serial.println("ERROR: Token device mismatch!");
        Serial.println("  Token device: " + tokenDeviceUID);
        Serial.println("  Current device: " + deviceUID);
        request->send(200, "application/json", "{\"registered\":false,\"error\":\"device_mismatch\"}");
        return;
      }

      // Token valid and matches this device - return profile data
      request->send(200, "application/json", response);

    } else if (httpCode == 401) {
      // Parse error response to check WHY validation failed
      DynamicJsonDocument doc(1024);
      DeserializationError error = deserializeJson(doc, response.c_str(), response.length());

      String errorMsg = "";
      if (!error && doc.containsKey("error")) {
        errorMsg = doc["error"].as<String>();
      }

      Serial.println("Validation failed: " + errorMsg);

      // Only clear credentials if token genuinely doesn't exist in database
      if (errorMsg == "Invalid token" || errorMsg == "Token not found") {
        Serial.println("━━━━━━━━━━━━━━━━━━━━━━━━━━━━");
        Serial.println("⚠️ CLEARING INVALID CREDENTIALS");
        Serial.println("━━━━━━━━━━━━━━━━━━━━━━━━━━━━");
        Serial.println("Token no longer exists in database");
        Serial.println("Device ready for re-registration");
        Serial.println("━━━━━━━━━━━━━━━━━━━━━━━━━━━━");

        clearAuthToken();
      } else {
        Serial.println("⚠️ Auth error but keeping credentials (may be temporary)");
      }

      request->send(200, "application/json", "{\"registered\":false,\"error\":\"validation_failed\"}");

    } else {
      // Network error or server error - keep credentials
      Serial.println("⚠️ Network/server issue (HTTP " + String(httpCode) + ") - keeping credentials");
      request->send(200, "application/json", "{\"registered\":false,\"error\":\"network_error\"}");
    }
  });

  server.on("/getAuthToken", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (!isRegistered) {
      request->send(200, "application/json", "{\"registered\":false}");
      return;
    }

    DynamicJsonDocument doc(256);
    doc["registered"] = true;
    doc["token"] = authToken;

    String response;
    serializeJson(doc, response);  // Keep as-is, String works fine
    request->send(200, "application/json", response);
  });
  // Cloud Features
  server.on("/registerProfile", HTTP_POST, [](AsyncWebServerRequest *request) {
    // Check password
    if (!request->hasParam("password", true) || strcmp(request->getParam("password", true)->value().c_str(), requiredPassword) != 0) {
      request->send(403, "text/plain", "Forbidden");
      return;
    }
    String deviceUID = String(device_id_hex);
    DynamicJsonDocument doc(2048);
    doc["device_uid"] = deviceUID;
    doc["password"] = String(requiredPassword);
    // User account info
    doc["username"] = request->getParam("username", true)->value();
    doc["email"] = request->getParam("email", true)->value();
    // Vessel info (15 fields)
    doc["boat_length_ft"] = request->getParam("boat_length_ft", true)->value().toFloat();
    doc["boat_type"] = request->getParam("boat_type", true)->value();
    doc["boat_make_model"] = request->getParam("boat_make_model", true)->value();
    doc["boat_year"] = request->getParam("boat_year", true)->value().toInt();
    doc["home_port"] = request->getParam("home_port", true)->value();
    doc["engine_make"] = request->getParam("engine_make", true)->value();
    doc["engine_hp"] = request->getParam("engine_hp", true)->value().toInt();
    doc["battery_voltage"] = request->getParam("battery_voltage", true)->value().toInt();
    doc["battery_capacity_ah"] = request->getParam("battery_capacity_ah", true)->value().toInt();
    doc["battery_type"] = request->getParam("battery_type", true)->value();
    doc["alternator_brand_model"] = request->getParam("alternator_brand_model", true)->value();
    doc["solar_watts"] = request->getParam("solar_watts", true)->value().toInt();
    doc["imu_mount_orientation"] = request->getParam("imu_mount_orientation", true)->value().toInt();
    doc["imu_dist_bow_ft"] = request->getParam("imu_dist_bow_ft", true)->value().toFloat();
    doc["imu_dist_cl_ft"] = request->getParam("imu_dist_cl_ft", true)->value().toFloat();
    doc["imu_height_wl_ft"] = request->getParam("imu_height_wl_ft", true)->value().toFloat();
    String payload;
    serializeJson(doc, payload);
    Serial.println("=== REGISTRATION REQUEST ===");
    Serial.println("Payload: " + payload);
    HTTPClient http;
    char url[256];
    snprintf(url, sizeof(url), "%s/functions/v1/register-device", SUPABASE_URL);
    Serial.print("Connecting to: ");
    Serial.println(url);
    Serial.printf("=== PRE-REGISTRATION HEAP ===\n");
    Serial.printf("Free internal: %u\n", ESP.getFreeHeap());
    Serial.printf("Max alloc internal: %u\n", ESP.getMaxAllocHeap());
    Serial.printf("Free PSRAM: %u\n", ESP.getFreePsram());
    http.begin(url);
    http.addHeader("Content-Type", "application/json");
    char authHeader[512];
    snprintf(authHeader, sizeof(authHeader), "Bearer %s", SUPABASE_ANON_KEY);
    http.addHeader("Authorization", authHeader);

    int httpCode = http.POST(payload);
    String response = http.getString();

    Serial.println("Response code: " + String(httpCode));
    Serial.println("Response: " + response);

    http.end();

    // Connection-level failure (DNS, timeout, refused, etc.)
    if (httpCode <= 0) {
      Serial.println("HTTP connection failed: " + String(httpCode));
      request->send(503, "application/json",
                    "{\"error\":\"Connection to cloud failed\",\"code\":" + String(httpCode) + "}");
      return;
    }

    // Empty body guard
    if (response.length() == 0) {
      Serial.println("Empty response from server (HTTP " + String(httpCode) + ")");
      request->send(502, "application/json",
                    "{\"error\":\"Empty response from cloud\",\"code\":" + String(httpCode) + "}");
      return;
    }

    // Token extraction for 200
    if (httpCode == 200) {
      DynamicJsonDocument responseDoc(1024);
      DeserializationError error = deserializeJson(responseDoc, response.c_str(), response.length());
      if (!error && responseDoc.containsKey("token")) {
        String newToken = responseDoc["token"].as<String>();
        if (newToken.length() > 0) {
          saveAuthToken(newToken);
          Serial.println("Token saved to NVS");
        }
      }
    }

    request->send(httpCode, "application/json", response);
  });

  server.on("/updateProfile", HTTP_POST, [](AsyncWebServerRequest *request) {
    if (!request->hasParam("password", true) || strcmp(request->getParam("password", true)->value().c_str(), requiredPassword) != 0) {
      request->send(403, "text/plain", "Forbidden");
      return;
    }

    String token = loadAuthToken();
    if (token.length() == 0) {
      request->send(400, "application/json", "{\"success\":false,\"error\":\"Not registered\"}");
      return;
    }

    String deviceUID = String(device_id_hex);

    DynamicJsonDocument doc(2048);
    doc["device_uid"] = deviceUID;
    doc["token"] = token;

    // User account info
    doc["username"] = request->getParam("username", true)->value();
    doc["email"] = request->getParam("email", true)->value();

    // Vessel info (15 fields)
    doc["boat_length_ft"] = request->getParam("boat_length_ft", true)->value().toFloat();
    doc["boat_type"] = request->getParam("boat_type", true)->value();
    doc["boat_make_model"] = request->getParam("boat_make_model", true)->value();
    doc["boat_year"] = request->getParam("boat_year", true)->value().toInt();
    doc["home_port"] = request->getParam("home_port", true)->value();
    doc["engine_make"] = request->getParam("engine_make", true)->value();
    doc["engine_hp"] = request->getParam("engine_hp", true)->value().toInt();
    doc["battery_voltage"] = request->getParam("battery_voltage", true)->value().toInt();
    doc["battery_capacity_ah"] = request->getParam("battery_capacity_ah", true)->value().toInt();
    doc["battery_type"] = request->getParam("battery_type", true)->value();
    doc["alternator_brand_model"] = request->getParam("alternator_brand_model", true)->value();
    doc["solar_watts"] = request->getParam("solar_watts", true)->value().toInt();
    doc["imu_mount_orientation"] = request->getParam("imu_mount_orientation", true)->value().toInt();
    doc["imu_dist_bow_ft"] = request->getParam("imu_dist_bow_ft", true)->value().toFloat();
    doc["imu_dist_cl_ft"] = request->getParam("imu_dist_cl_ft", true)->value().toFloat();
    doc["imu_height_wl_ft"] = request->getParam("imu_height_wl_ft", true)->value().toFloat();

    String payload;
    serializeJson(doc, payload);

    Serial.println("=== UPDATE PROFILE REQUEST ===");
    Serial.println("Token from NVS: " + token);
    Serial.println("Device UID: " + deviceUID);
    Serial.println("Payload: " + payload);

    HTTPClient http;
    char url[256];
    snprintf(url, sizeof(url), "%s/functions/v1/update-profile", SUPABASE_URL);
    http.begin(url);
    http.addHeader("Content-Type", "application/json");
    char authHeader[512];
    snprintf(authHeader, sizeof(authHeader), "Bearer %s", SUPABASE_ANON_KEY);
    http.addHeader("Authorization", authHeader);

    int httpCode = http.POST(payload);
    String response = http.getString();

    Serial.println("Response code: " + String(httpCode));
    Serial.println("Response: " + response);

    http.end();

    request->send(httpCode, "application/json", response);
  });

  // Delete All Data endpoint
  server.on("/deleteAllData", HTTP_POST, [](AsyncWebServerRequest *request) {
    if (!request->hasParam("password", true) || strcmp(request->getParam("password", true)->value().c_str(), requiredPassword) != 0) {
      request->send(403, "text/plain", "Forbidden");
      return;
    }

    String deviceUID = String(device_id_hex);
    // Use ArduinoJson for consistency
    DynamicJsonDocument doc(256);
    doc["device_uid"] = deviceUID;

    String payload;
    serializeJson(doc, payload);

    Serial.println("=== DELETE ALL DATA REQUEST ===");
    Serial.println("Payload: " + payload);

    HTTPClient http;
    char url[256];
    snprintf(url, sizeof(url), "%s/functions/v1/delete-user-data", SUPABASE_URL);
    http.begin(url);
    http.addHeader("Content-Type", "application/json");
    char authHeader[512];
    snprintf(authHeader, sizeof(authHeader), "Bearer %s", SUPABASE_ANON_KEY);
    http.addHeader("Authorization", authHeader);

    int httpCode = http.POST(payload);
    String response = http.getString();
    http.end();

    Serial.println("Response code: " + String(httpCode));
    Serial.println("Response: " + response);

    if (httpCode == 200) {
      clearAuthToken();
      Serial.println("Auth token cleared from NVS");
    }

    request->send(httpCode, "application/json", response);
  });

  server.on("/resetThermalPID", HTTP_POST, [](AsyncWebServerRequest *request) {
    tempPIDResetRequested = true;
    request->send(200, "text/plain", "OK");
  });

  server.on("/resetInnerPID", HTTP_POST, [](AsyncWebServerRequest *request) {
    innerPIDResetRequested = true;
    request->send(200, "text/plain", "OK");
  });

  server.on("/tuninglog", HTTP_GET, [](AsyncWebServerRequest *request) {
    char *buf = (char *)ps_malloc(10240);
    if (!buf) { request->send(500, "text/plain", "OOM"); return; }

    // Build sorted index (insertion sort — 50 entries max)
    uint8_t sortIdx[50];
    for (int i = 0; i < tuningLogCount; i++) sortIdx[i] = i;
    for (int i = 1; i < tuningLogCount; i++) {
      uint8_t key = sortIdx[i];
      float keyScore = tuningLog[key].score;
      int j = i - 1;
      while (j >= 0 && tuningLog[sortIdx[j]].score > keyScore) {
        sortIdx[j + 1] = sortIdx[j];
        j--;
      }
      sortIdx[j + 1] = key;
    }

    int pos = 0;
    pos += snprintf(buf + pos, 10240 - pos, "{\"rec\":[");
    for (int i = 0; i < tuningLogCount && pos < 9800; i++) {
      TuningRecord &r = tuningLog[sortIdx[i]];
      pos += snprintf(buf + pos, 10240 - pos,
        "%s{\"n\":%d,\"s\":%.2f,\"t\":%.1f,"
        "\"kp\":%.4f,\"ki\":%.4f,\"kd\":%.5f,"
        "\"sd\":%d,\"tg\":%.2f,\"dr\":%.1f,"
        "\"wa\":%d,\"wp\":%d,"
        "\"rpm\":%.0f,\"temp\":%.1f,\"worst\":%.1f}",
        i > 0 ? "," : "",
        r.runNumber, r.score, r.activeTimeSec,
        r.kp, r.ki, r.kd,
        r.sampleDivisor, r.trackingGain, r.dutyRampRate,
        (int)r.waveAmplitude, (int)r.wavePeriod,
        r.avgRPM, r.avgAltTempF, r.worstErrorA);
    }
    bool testActive = (TuningMode && tuningScore.toggleCount > 0);
    float ts = (tuningScore.activeTimeSec > 0.0f)
                 ? (tuningScore.errorAccum / tuningScore.activeTimeSec) : 0.0f;
    pos += snprintf(buf + pos, 10240 - pos,
      "],\"live\":[%.2f,%.2f,%.2f,%.2f],"
      "\"ts\":%.2f,\"tt\":%d,\"ta\":%d}",
      liveScoreVal[0], liveScoreVal[1], liveScoreVal[2], liveScoreVal[3],
      ts, (int)tuningScore.toggleCount, testActive ? 1 : 0);

    request->send(200, "application/json", String(buf));
    free(buf);
  });

  server.on("/cvtuninglog", HTTP_GET, [](AsyncWebServerRequest *request) {
    char *buf = (char *)ps_malloc(16384);
    if (!buf) { request->send(500, "text/plain", "OOM"); return; }

    // Build sorted index (insertion sort, best score first)
    uint8_t sortIdx[50];
    for (int i = 0; i < cvTuningLogCount; i++) sortIdx[i] = i;
    for (int i = 1; i < cvTuningLogCount; i++) {
      uint8_t key = sortIdx[i];
      float keyScore = cvTuningLog[key].score;
      int j = i - 1;
      while (j >= 0 && cvTuningLog[sortIdx[j]].score > keyScore) {
        sortIdx[j + 1] = sortIdx[j];
        j--;
      }
      sortIdx[j + 1] = key;
    }

    int pos = 0;
    pos += snprintf(buf + pos, 16384 - pos, "{\"rec\":[");
    for (int i = 0; i < cvTuningLogCount && pos < 15800; i++) {
      CVTuningRecord &r = cvTuningLog[sortIdx[i]];
      pos += snprintf(buf + pos, 16384 - pos,
        "%s{\"n\":%d,\"s\":%.2f,\"st\":%.1f,\"wo\":%.3f,\"io\":%.4f,\"t\":%.1f,"
        "\"ls\":%.2f,\"lst\":%.1f,\"lwo\":%.3f,\"lio\":%.4f,"
        "\"fov\":%d,\"iex\":%d,\"ld\":%d,\"hoc\":%d,"
        "\"vkp\":%.3f,\"vki\":%.3f,\"vkd\":%.2f,"
        "\"srr\":%.1f,\"sfr\":%.1f,"
        "\"abl\":%.2f,\"arl\":%.3f,\"asp\":%d,\"irf\":%.2f,"
        "\"ks\":%.1f,\"kh\":%.1f,"
        "\"iek\":%.1f,\"ien\":%d,\"iekb\":%.2f,"
        "\"lddt\":%.0f,\"ldcd\":%.0f,"
        "\"tc\":%.0f,\"wa\":%.2f,\"wp\":%d,\"ko\":%.1f,\"cr\":%d,"
        "\"rpm\":%.0f,\"tmp\":%.1f,\"bv\":%.2f,\"soc\":%.1f,\"cvt\":%.2f}",
        i > 0 ? "," : "",
        r.runNumber, r.score, r.avgSettlingTimeSec, r.worstOvershootV,
        r.avgIntegratedOvershootVs, r.activeTimeSec,
        r.lowScore, r.avgLowSettlingTimeSec, r.worstLowOvV, r.avgLowIntOvVs,
        (int)r.fastOvFires, (int)r.iExcessFires, (int)r.loadDumpFires, (int)r.hardOcFires,
        r.voltageKp, r.voltageKi, r.voltageKd,
        r.setpointRiseRate, r.setpointFallRate,
        r.awBleedRate, r.awRecoverRate, (int)r.awSeedProtectMs, r.iExcessReseedFrac,
        r.kSoft, r.kHard,
        r.iExcessK, (int)r.iExcessN, r.iExcessKBleed,
        r.loadDumpDtThresh, r.loadDumpCurrentDrop,
        r.inputFilterTC, r.waveAmplitudeV, (int)r.wavePeriodSec, r.kOvershoot, (int)r.consecutiveReads,
        r.avgRPM, r.avgAltTempF, r.battVAtStart, r.socAtStart * 100.0f, r.chargingVoltageTarget);
    }
    // Active test state
    bool cvTestActive = (CVTuningMode && cvTuningScore.testStarted);
    float cvts = 0.0f;
    if (cvTestActive && cvTuningScore.scoredHighCount > 0) {
      float n = (float)cvTuningScore.scoredHighCount;
      cvts = (cvTuningScore.totalSettlingTimeSec / n)
           + cvKOvershoot * (cvTuningScore.totalIntegratedOvershootVs / n);
    }
    pos += snprintf(buf + pos, 16384 - pos,
      "],\"live\":[%.2f,%.2f,%.2f,%.2f],"
      "\"ts\":%.2f,\"tc\":%d,\"ta\":%d}",
      cvLiveScoreVal[0], cvLiveScoreVal[1], cvLiveScoreVal[2], cvLiveScoreVal[3],
      cvts, (int)cvTuningScore.scoredHighCount, cvTestActive ? 1 : 0);

    request->send(200, "application/json", String(buf));
    free(buf);
  });

  server.on("/resetcvtuninglog", HTTP_POST, [](AsyncWebServerRequest *request) {
    cvTuningLogCount     = 0;
    cvTuningLogHead      = 0;
    cvTuningRunCounter   = 0;
    cvTuningScore        = {};
    cvTuningParamChanged = false;
    if (cvTuningLog) memset(cvTuningLog, 0, 50 * sizeof(CVTuningRecord));
    for (int i = 0; i < 4; i++) {
      if (cvLiveScoreBuckets[i]) memset(cvLiveScoreBuckets[i], 0, LIVE_BUCKET_N * sizeof(ScoreBucket));
      cvLiveScoreHead[i]      = 0;
      cvLiveBucketStartMs[i]  = 0;
      cvLiveScoreVal[i]       = 0.0f;
    }
    cvLiveScore_lastDtMs = 0;
    cvLiveScore_inWindow = false;
    saveCVTuningLog();
    request->send(200, "text/plain", "OK");
  });

  server.on("/thermaltuninglog", HTTP_GET, [](AsyncWebServerRequest *request) {
    char *buf = (char *)ps_malloc(8192);
    if (!buf) { request->send(500, "text/plain", "OOM"); return; }

    // Sort by score ascending (best first)
    uint8_t sortIdx[50];
    for (int i = 0; i < thermalTuningLogCount; i++) sortIdx[i] = i;
    for (int i = 1; i < thermalTuningLogCount; i++) {
      uint8_t key = sortIdx[i];
      float keyScore = thermalTuningLog[key].score;
      int j = i - 1;
      while (j >= 0 && thermalTuningLog[sortIdx[j]].score > keyScore) {
        sortIdx[j + 1] = sortIdx[j];
        j--;
      }
      sortIdx[j + 1] = key;
    }

    int pos = 0;
    pos += snprintf(buf + pos, 8192 - pos, "{\"rec\":[");
    for (int i = 0; i < thermalTuningLogCount && pos < 7800; i++) {
      ThermalTuningRecord &r = thermalTuningLog[sortIdx[i]];
      pos += snprintf(buf + pos, 8192 - pos,
        "%s{\"n\":%d,\"s\":%.2f,\"st\":%.0f,\"wo\":%.1f,\"io\":%.2f,\"iu\":%.2f,"
        "\"ns\":%d,\"t\":%.0f,"
        "\"kp\":%.4f,\"ki\":%.5f,\"la\":%.0f,\"fa\":%.3f,\"im\":%d,"
        "\"wl\":%.0f,\"wh\":%.0f,\"wp\":%.1f,"
        "\"rr\":%.1f,\"fr\":%.1f,"
        "\"rpm\":%.0f,\"amb\":%.1f}",
        i > 0 ? "," : "",
        r.runNumber, r.score, r.avgSettlingTimeSec, r.worstOvershootF,
        r.avgIntOverFs, r.avgIntUnderFs,
        (int)r.scoredStepCount, r.activeTimeSec,
        r.kp, r.ki, r.lookaheadSec, r.filterAlpha, (int)r.intervalMs,
        r.waveLowF, r.waveHighF, r.waveHalfPeriodMin,
        r.riseRate, r.fallRate,
        r.avgRPM, r.avgAmbientF);
    }
    // Active test state
    bool testActive = (ThermalTuningMode && thermalTuningScore.testStarted && thermalTuningScore.ringInDone);
    float ts = 0.0f;
    if (testActive && thermalTuningScore.scoredStepCount > 0) {
      float n = (float)thermalTuningScore.scoredStepCount;
      float avgSettle = thermalTuningScore.totalSettlingTimeSec / n;
      float avgOver   = thermalTuningScore.totalIntOverFs / n;
      float avgUnder  = thermalTuningScore.totalIntUnderFs / n;
      ts = avgSettle + thermalKOvershoot * avgOver + thermalKUndershoot * avgUnder;
    }
    pos += snprintf(buf + pos, 8192 - pos,
      "],\"live\":[%.4f,%.4f,%.4f,%.4f],"
      "\"ts\":%.2f,\"tc\":%d,\"ta\":%d}",
      thermalLiveScoreVal[0], thermalLiveScoreVal[1],
      thermalLiveScoreVal[2], thermalLiveScoreVal[3],
      ts, (int)thermalTuningScore.scoredStepCount, testActive ? 1 : 0);

    request->send(200, "application/json", String(buf));
    free(buf);
  });

  server.on("/resetthermaltuninglog", HTTP_POST, [](AsyncWebServerRequest *request) {
    thermalTuningLogCount     = 0;
    thermalTuningLogHead      = 0;
    thermalTuningRunCounter   = 0;
    thermalTuningScore        = {};
    thermalTuningParamChanged = false;
    thermalWaveCurrentSetpointF = 0.0f;
    if (thermalTuningLog) memset(thermalTuningLog, 0, 50 * sizeof(ThermalTuningRecord));
    for (int i = 0; i < 4; i++) {
      if (thermalLiveScoreBuckets[i]) memset(thermalLiveScoreBuckets[i], 0, LIVE_BUCKET_N * sizeof(ScoreBucket));
      thermalLiveScoreHead[i]       = 0;
      thermalLiveBucketStartMs[i]   = 0;
      thermalLiveScoreVal[i]        = 0.0f;
    }
    saveThermalTuningLog();
    request->send(200, "text/plain", "OK");
  });

  server.on("/resettuninglog", HTTP_POST, [](AsyncWebServerRequest *request) {
    tuningLogCount    = 0;
    tuningLogHead     = 0;
    tuningRunCounter  = 0;
    tuningScore       = {};
    tuningParamChanged = false;
    if (tuningLog) memset(tuningLog, 0, 50 * sizeof(TuningRecord));
    for (int i = 0; i < 4; i++) {
      if (liveScoreBuckets[i]) memset(liveScoreBuckets[i], 0, LIVE_BUCKET_N * sizeof(ScoreBucket));
      liveScoreHead[i]     = 0;
      liveBucketStartMs[i] = 0;
      liveScoreVal[i]      = 0.0f;
    }
    liveScore_lastCmd    = 0.0f;
    liveScore_thisCmd    = 0.0f;
    liveScore_lastStepMs = 0;
    liveScore_inWindow   = false;
    saveTuningLog();
    request->send(200, "text/plain", "OK");
  });

  server.on("/resetVoltageLoop", HTTP_POST, [](AsyncWebServerRequest *request) {
    cvLoopResetRequested = true;
    request->send(200, "text/plain", "OK");
  });

  server.on("/resetVoltageProtectionCounters", HTTP_POST, [](AsyncWebServerRequest *request) {
    g_fastOvClampCount = 0;
    g_fastOvSoftCount = 0;
    g_fastOvHardCount = 0;
    g_iExcessCount = 0;
    g_inaOVCount = 0;
    g_hardOCCount = 0;
    g_voltSpikeCount = 0;
    g_voltDisagreeCritCount = 0;
    g_voltDisagreeWarnCount = 0;
    g_voltImplausibleCount = 0;
    g_currentStaleCount = 0;
    request->send(200, "text/plain", "OK");
  });

  server.on("/resetThermalProtectionCounters", HTTP_POST, [](AsyncWebServerRequest *request) {
    g_tempCritCount = 0;
    g_tempSustainedCount = 0;
    g_tempStaleCount = 0;
    request->send(200, "text/plain", "OK");
  });

  server.on("/resetTempTaskCounters", HTTP_POST, [](AsyncWebServerRequest *request) {
    tempReadFailCount = 0;
    tempCrcFailCount = 0;
    tempCrcRecoveredCount = 0;
    tempAllFFCount = 0;
    tempPowerOn85Count = 0;
    tempOutOfRangeCount = 0;
    tempRequestFailCount = 0;
    tempConnectedFailCount = 0;
    tempResolutionFixCount = 0;
    tempRereadFailCount = 0;
    tempResolutionFixCrcFailCount = 0;
    tempEnumerateFailCount = 0;
    request->send(200, "text/plain", "OK");
  });

  // Cloud Features debug (raw NVS, no Preferences, still untested)
  server.on("/debugToken", HTTP_GET, [](AsyncWebServerRequest *request) {
    char storedToken[256] = "";
    bool haveStored = false;

    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open("cloud", NVS_READONLY, &nvs_handle);
    if (err == ESP_OK) {
      size_t len = 0;
      err = nvs_get_str(nvs_handle, "authToken", nullptr, &len);

      if (err == ESP_OK && len > 0) {
        if (len >= sizeof(storedToken)) len = sizeof(storedToken) - 1;
        size_t tmpLen = len + 1;  // nvs expects space for null
        err = nvs_get_str(nvs_handle, "authToken", storedToken, &tmpLen);
        if (err == ESP_OK) {
          storedToken[sizeof(storedToken) - 1] = '\0';
          haveStored = true;
        } else {
          Serial.printf("ERROR: nvs_get_str(authToken) failed on second read (err=%d)\n", (int)err);
        }
      } else if (err == ESP_ERR_NVS_NOT_FOUND) {
        Serial.println("NVS: authToken key not found in 'cloud' namespace");
      } else if (err != ESP_OK) {
        Serial.printf("ERROR: nvs_get_str(authToken) len query failed (err=%d)\n", (int)err);
      }

      nvs_close(nvs_handle);
    } else {
      Serial.printf("ERROR: Failed to open NVS namespace 'cloud' (err=%d)\n", (int)err);
    }

    char out[768];
    const char *globalTok = authToken.c_str();  // authToken is your global String
    snprintf(out, sizeof(out),
             "isRegistered: %d\nauthToken global: %s\nNVS stored token: %s\n",
             (int)isRegistered,
             globalTok ? globalTok : "",
             haveStored ? storedToken : "");
    request->send(200, "text/plain", out);
  });

  server.addHandler(&events);
  server.begin();
  Serial.println("=== MAIN SERVER STARTED ===");
  Serial.println("=== MAIN SERVER SETUP COMPLETE ===");
}
void dnsHandleRequest() {  // process dns request for captive portals
  if (currentMode == MODE_AP || currentMode == MODE_CONFIG) {
    // Only process if there might actually be a request waiting
    // This is a workaround for ESP32-S3 DNS library blocking issues
    static unsigned long lastDNSProcess = 0;
    if (millis() - lastDNSProcess > 10) {  // Throttle to every 10ms for fast captive portal response
      dnsServer.processNextRequest();
      lastDNSProcess = millis();
    }
  }
}
void checkWiFiConnection() {
  // Only attempt reconnection in client mode
  if (currentMode != MODE_CLIENT) return;
  // === VOLTAGE-PROXIMITY RECONNECT THROTTLE ===
  // When battery voltage is close to charging target, alternator is working hard
  // and a 5-second reconnect block could disrupt regulation at a critical moment.
  // Use 5-minute minimum interval when within 0.3V of target, normal backoff otherwise.
  if (OnOff == 1 && ChargingVoltageTarget > 0) {
    float voltageError = ChargingVoltageTarget - getBatteryVoltage();
    // voltageError > 0 means battery is below target (still charging hard)
    // voltageError < 0 means battery is above target (shouldn't happen normally)
    if (voltageError >= -0.3f && voltageError <= 0.3f) {
      // Within 0.3V of target - near regulation point, don't risk a 5s block
      if (millis() - wifiRecon.lastAttempt < wifiRecon.maxInterval) return;
    }
  }
  // === END VOLTAGE-PROXIMITY RECONNECT THROTTLE ===
  // === THROTTLE WIFI CHECKS (every 2 seconds) ===
  static int cachedWiFiMode = WIFI_OFF;
  static int cachedWiFiStatus = WL_DISCONNECTED;
  static int cachedWiFiRSSI = -100;
  static int cachedCpuFreq = 240;
  static unsigned long lastWiFiPoll = 0;

  unsigned long now = millis();

  // Poll actual WiFi state only every 2 seconds
  if (now - lastWiFiPoll > 2000) {
    cachedWiFiMode = WiFi.getMode();
    cachedWiFiStatus = WiFi.status();
    cachedWiFiRSSI = WiFi.RSSI();
    cachedCpuFreq = getCpuFrequencyMhz();
    lastWiFiPoll = now;
  }

  // Use cached values for early returns (no ipc0!)
  if (cachedWiFiMode == WIFI_OFF) return;
  if (cachedCpuFreq < 81) return;

  // === END THROTTLE ===

  if (cachedWiFiStatus == WL_CONNECTED) {  // ← Use cached
    // Update signal strength while connected
    wifiRecon.lastSignalStrength = cachedWiFiRSSI;  // ← Use cached

    // Reset reconnection state on successful connection
    if (wifiRecon.attemptCount > 0) {
      Serial.println("WiFi reconnected successfully!");
      queueConsoleMessageF("WiFi reconnected after %d attempts", wifiRecon.attemptCount);
    }
    wifiRecon.attemptCount = 0;
    wifiRecon.currentInterval = wifiRecon.minInterval;
    wifiRecon.giveUpMode = false;
    return;
  }

  // WiFi is disconnected - check if we should give up temporarily
  if (wifiRecon.giveUpMode) {
    if (now - wifiRecon.lastAttempt < 300000) return;  // ← Use now instead of millis()
    Serial.println("WiFi: Exiting give-up mode, attempting fresh reconnection burst");
    wifiRecon.giveUpMode = false;
    wifiRecon.attemptCount = 0;
    wifiRecon.currentInterval = wifiRecon.minInterval;
  }

  // Signal strength awareness
  if (wifiRecon.lastSignalStrength != -999 && wifiRecon.lastSignalStrength < wifiRecon.minSignalThreshold) {
    if (now - wifiRecon.lastAttempt < 60000) return;  // ← Use now
    Serial.printf("WiFi: Poor signal (%d dBm), using extended retry interval\n", wifiRecon.lastSignalStrength);
  } else {
    if (now - wifiRecon.lastAttempt < wifiRecon.currentInterval) return;  // ← Use now
  }

  wifiRecon.lastAttempt = now;  // ← Use now
  wifiRecon.attemptCount++;

  Serial.printf("WiFi reconnection attempt #%d (interval: %lums, last signal: %d dBm)\n",
                wifiRecon.attemptCount, wifiRecon.currentInterval, wifiRecon.lastSignalStrength);

  // Use cached credentials only (no filesystem reads here)
  if (!cached_wifi_creds_valid || strlen(cached_wifi_ssid) == 0) {
    Serial.println("WiFi: No cached SSID found for reconnection");
    return;
  }

  bool connected = connectToWiFi(cached_wifi_ssid, cached_wifi_pass, 5000);
  if (connected) {
    Serial.println("WiFi reconnection successful!");
    return;
  }

  // Failed - check if we should give up temporarily
  if (wifiRecon.attemptCount >= wifiRecon.maxAttempts) {
    Serial.println("WiFi: Max attempts reached, entering give-up mode for 5 minutes");
    queueConsoleMessageF("WiFi: Max reconnection attempts (%d) reached, will retry in 5 minutes", wifiRecon.maxAttempts);
    wifiRecon.giveUpMode = true;
    return;
  }

  // Intelligent exponential backoff
  if (wifiRecon.currentInterval < 32000) {
    wifiRecon.currentInterval *= 2;
  } else if (wifiRecon.currentInterval < 60000) {
    wifiRecon.currentInterval = 60000;
  } else if (wifiRecon.currentInterval < 120000) {
    wifiRecon.currentInterval = 120000;
  } else {
    wifiRecon.currentInterval = wifiRecon.maxInterval;
  }

  Serial.printf("WiFi: Next attempt in %lu seconds\n", wifiRecon.currentInterval / 1000);

  if (wifiRecon.lastSignalStrength != -999 && wifiRecon.lastSignalStrength < -76) {
    queueConsoleMessageF("WiFi: Weak signal (%d dBm) may be causing disconnections", wifiRecon.lastSignalStrength);
  }
}
void SendWifiData() {
  // Don't send WiFi data during HTTPS operations
  //if (core0Busy) return; // from Claude: My guess: The check is overly defensive. EventSource sends are typically async and won't block HTTPS. But worth testing since WiFi radio is shared.
  unsigned long start66 = micros();
  // === THROTTLED WIFI CHECKS (every 2 seconds) ===
  static int cachedWiFiMode = WIFI_OFF;
  static int cachedWiFiConnected = 0;
  static int cachedWiFiRSSI = -100;
  static unsigned long lastWiFiCheck = 0;

  unsigned long now = millis();

  if (now - lastWiFiCheck > 2000) {  // Check WiFi every 2 seconds
    cachedWiFiMode = WiFi.getMode();
    cachedWiFiConnected = (WiFi.status() == WL_CONNECTED) ? 1 : 0;
    cachedWiFiRSSI = WiFi.RSSI();
    lastWiFiCheck = now;
  }

  // Use cached values (no ipc0 calls!)
  if (cachedWiFiMode == WIFI_OFF) return;
  if (currentMode == MODE_CLIENT && !cachedWiFiConnected) return;

  // === END THROTTLED CHECKS ===

  // Static variables for timing control
  static unsigned long prev_millis5 = 0;
  static unsigned long lastConsoleMessageTime = 0;
  static unsigned long lastpayload2send = 0;
  static unsigned long lastpayload3send = 0;
  static unsigned long lastTimestampSend = 0;
  const unsigned long EVENTSOURCE_COOLDOWN = 50;
  const unsigned long CONSOLE_MESSAGE_INTERVAL = 1000;

  bool canSendNow = (now - lastEventSourceSend >= EVENTSOURCE_COOLDOWN);
  if (!canSendNow) return;

  bool sentSomething = false;

  // PRIORITY 1: CSVData
  if (!sentSomething && now - prev_millis5 >= webgaugesinterval && events.count() > 0) {
    WifiStrength = cachedWiFiRSSI;
    WifiHeartBeat = WifiHeartBeat + 1;
    ch1_compute_stats();  // ← add this line immediately before the snprintf

    static char *payload1 = nullptr;
    static const size_t PAYLOAD1_SIZE = 700;
    if (!payload1) {
      payload1 = (char *)ps_malloc(PAYLOAD1_SIZE);  // bumped from 500, allocated to PSRAM
      if (!payload1) {
        Serial.println("FATAL: payload1 ps_malloc failed");
        return;
      }
    }
    int payload1Len = snprintf(payload1, PAYLOAD1_SIZE,
                               "%d,"  // CSV1_FIELD_COUNT
                               "%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,"
                               "%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,"
                               "%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,"
                               "%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,"
                               "%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,"
                               "%d,%d,%d",

                               CSV1_FIELD_COUNT,
                               SafeInt(AlternatorTemperatureF, 100),    // 0
                               SafeInt(dutyCycle, 100),                 // 1
                               SafeInt(BatteryV, 100),                  // 2
                               SafeInt(MeasuredAmps, 100),              // 3
                               SafeInt(RPM),                            // 4
                               SafeInt(Channel3V, 100),                 // 5
                               SafeInt(IBV, 100),                       // 6
                               SafeInt(Bcur, 100),                      // 7
                               SafeInt(VictronVoltage, 100),            // 8
                               SafeInt(LoopTime),                       // 9
                               SafeInt(WifiStrength),                   // 10
                               SafeInt(WifiHeartBeat),                  // 11
                               SafeInt(SendWifiTime),                   // 12
                               SafeInt(AnalogReadTime),                 // 13
                               SafeInt(VeTime),                         // 14
                               SafeInt(MaximumLoopTime),                // 15
                               SafeInt(HeadingNMEA),                    // 16
                               SafeInt(vvout, 100),                     // 17
                               SafeInt(iiout, 100),                     // 18
                               SafeInt(FreeHeap),                       // 19
                               SafeInt(EngineCycles),                   // 20
                               SafeInt(Alarm_Status),                   // 21
                               SafeInt(fieldActiveStatus),              // 22
                               SafeInt(CurrentSessionDuration),         // 23
                               SafeInt(timeAxisModeChanging),           // 24
                               SafeInt(webgaugesinterval),              // 25
                               SafeInt(plotTimeWindow),                 // 26
                               SafeInt(Ymin1),                          // 27
                               SafeInt(Ymax1),                          // 28
                               SafeInt(Ymin2, 100),                     // 29
                               SafeInt(Ymax2, 100),                     // 30
                               SafeInt(Ymin3),                          // 31
                               SafeInt(Ymax3),                          // 32
                               SafeInt(Ymin4),                          // 33
                               SafeInt(Ymax4),                          // 34
                               SafeInt((int)currentMode),               // 35
                               SafeInt(currentPartitionType),           // 36
                               SafeInt(stateRevision),                  // 37
                               SafeInt(setpointLimited, 100),           // 38
                               SafeInt(uTargetAmps, 100),               // 39
                               SafeInt(pidInput, 100),                  // 40
                               SafeInt(pidOutput, 100),                 // 41
                               SafeInt(pidError, 100),                  // 42
                               SafeInt(imu_heel_deg, 100),              // 43
                               SafeInt(imu_pitch_deg, 100),             // 44
                               SafeInt(imu_vertical_accel_g, 1000),     // 45
                               SafeInt(imu_yaw_rate_dps, 100),          // 46
                               SafeInt(imu_total_accel_g, 1000),        // 47
                               SafeInt(imu_hf_vibration_energy, 1000),  // 48
                               SafeInt(shutdownPhase),                  // 49
                               SafeInt(g_fastOvCurrentCap, 100),        // 50
                               SafeInt(g_fastOvClampCount),             // 51
                               SafeInt(g_fastOvSoftCount),              // 52
                               SafeInt(g_fastOvHardCount),              // 53
                               // CH1 interval diagnostics
                               SafeInt(ch1_last_ms),                // 54
                               SafeInt(ch1_avg_10s, 100),           // 55  — 2 decimal places
                               SafeInt(ch1_worst_10s),              // 56
                               SafeInt(ch1_over2x_10s),             // 57
                               SafeInt(ch1_n_10s),                  // 58
                               SafeInt(ch1_avg_2m, 100),            // 59  — 2 decimal places
                               SafeInt(ch1_worst_2m),               // 60
                               SafeInt(ch1_over2x_2m),              // 61
                               SafeInt(ch1_n_2m),                   // 62
                               SafeInt(ch1_avg_at, 100),            // 63  — 2 decimal places
                               SafeInt(ch1_worst_at),               // 64
                               SafeInt(ch1_over2x_at),              // 65
                               SafeInt(ch1_n_at),                   // 66
                               SafeInt(BatteryV_filtered, 100),     // 67 — 2 decimal places
                               SafeInt(MeasuredAmps_filtered, 100), // 68 — 2 decimal places
                               SafeInt(g_iExcessCount),             // 69
                               SafeInt(g_inaOVCount),               // 70
                               SafeInt(g_hardOCCount),              // 71
                               SafeInt(g_voltSpikeCount),           // 72
                               SafeInt(g_voltDisagreeCritCount),    // 73
                               SafeInt(g_voltDisagreeWarnCount),    // 74
                               SafeInt(g_voltImplausibleCount),     // 75
                               SafeInt(g_tempCritCount),            // 76
                               SafeInt(g_tempSustainedCount),       // 77
                               SafeInt(g_tempStaleCount),           // 78
                               SafeInt(g_currentStaleCount),        // 79
                               SafeInt(imu_msi_score, 100),         // 80
                               SafeInt(imu_vomit_pct, 100),         // 81
                               SafeInt(imu_anchorage_comfort, 100)  // 82
    );
    if (payload1Len < 0 || payload1Len >= PAYLOAD1_SIZE) {
      Serial.printf("payload1 truncated or format error: %d\n", payload1Len);
      return;
    }

    events.send(payload1, "CSVData");
    SendWifiTime = micros() - start66;
    prev_millis5 = now;
    lastEventSourceSend = now;
    sentSomething = true;
  }
  // PRIORITY 2: Console
  trySendConsoleSSE(sentSomething, now);
  // PRIORITY 3: CSVData2 (status data - every 2 seconds)
  if (!sentSomething && now - lastpayload2send >= 2000 && events.count() > 0) {
    static char *payload2 = nullptr;
    static const size_t PAYLOAD2_SIZE = 2550;
    if (!payload2) {
      payload2 = (char *)ps_malloc(PAYLOAD2_SIZE);  // allocated to PSRAM
      if (!payload2) {
        Serial.println("FATAL: payload2 ps_malloc failed");
        return;
      }
    }  // Format string:
    int payload2Len = snprintf(payload2, PAYLOAD2_SIZE,

                               "%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,"
                               "%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,"
                               "%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,"
                               "%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,"
                               "%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,"
                               "%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,"
                               "%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,"
                               "%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,"
                               "%d,%d,%d,%d,%d,%d,%d,%u,%u,%d,"
                               "%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,"
                               "%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,"
                               "%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,"
                               "%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,"
                               "%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,"
                               "%d,%d,%d,%d,%d,%d,%d,%d,"
                               "%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,"
                               "%d,%d,%d,%d,%d,%d,%d,%d,%d,"
                               "%d,%d,%d,%d,%d,%d,%d,%d,%d,"
                               "%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,"
                               "%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,"
                               "%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,"
                               "%d,%d,%d,%d,%d,%d,%d,%d,"
                               "%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,"
                               "%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,"
                               "%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,"
                               "%d,%d,%d,%d,%d,%d,%d,%d,"
                               "%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,"
                               "%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d",

                               CSV2_FIELD_COUNT,
                               SafeInt(IBVMax, 100),                                                                                                                                     //0
                               SafeInt(MeasuredAmpsMax, 100),                                                                                                                            //1
                               SafeInt(RPMMax),                                                                                                                                          //2
                               SafeInt(SOC_percent),                                                                                                                                     //3
                               SafeInt(EngineRunTime * 100 / 3600, 1),                                                                                                                   //4
                               SafeInt(AlternatorOnTime * 100 / 3600, 1),                                                                                                                //5
                               SafeInt(AlternatorFuelUsed, 100),                                                                                                                         //6
                               SafeInt(ChargedEnergy),                                                                                                                                   //7
                               SafeInt(DischargedEnergy),                                                                                                                                //8
                               SafeInt(AlternatorChargedEnergy),                                                                                                                         //9
                               SafeInt(MaxAlternatorTemperatureF),                                                                                                                       //10
                               SafeInt(temperatureThermistor),                                                                                                                           //11
                               SafeInt(MaxTemperatureThermistor),                                                                                                                        //12
                               SafeInt(VictronCurrent, 100),                                                                                                                             //13
                               SafeInt(timeToFullChargeMin),                                                                                                                             //14
                               SafeInt(timeToFullDischargeMin),                                                                                                                          //15
                               SafeInt(LatitudeNMEA * 1000000),                                                                                                                          //16
                               SafeInt(LongitudeNMEA * 1000000),                                                                                                                         //17
                               SafeInt(SatelliteCountNMEA),                                                                                                                              //18
                               SafeInt(absorptionCompleteTime),                                                                                                                          //19
                               SafeInt(LastSessionDuration),                                                                                                                             //20
                               SafeInt(LastSessionMaxLoopTime),                                                                                                                          //21
                               SafeInt(lastSessionMinHeap),                                                                                                                              //22
                               SafeInt(wifiReconnectsTotal),                                                                                                                             //23
                               SafeInt(LastResetReason),                                                                                                                                 //24
                               SafeInt(ancientResetReason),                                                                                                                              //25
                               SafeInt(totalPowerCycles),                                                                                                                                //26
                               SafeInt(MinFreeHeap),                                                                                                                                     //27
                               SafeInt(currentWeatherMode),                                                                                                                              //28
                               SafeInt(UVToday, 100),                                                                                                                                    //29
                               SafeInt(UVTomorrow, 100),                                                                                                                                 //30
                               SafeInt(UVDay2, 100),                                                                                                                                     //31
                               SafeInt(weatherDataValid),                                                                                                                                //32
                               SafeInt(SolarWatts),                                                                                                                                      //33
                               SafeInt(performanceRatio, 100),                                                                                                                           //34
                               SafeInt(OnOff),                                                                                                                                           //35
                               SafeInt(ManualFieldToggle),                                                                                                                               //36
                               SafeInt(HiLow),                                                                                                                                           //37
                               SafeInt(LimpHome),                                                                                                                                        //38
                               SafeInt(VeData),                                                                                                                                          //39
                               SafeInt(NMEA0183Data),                                                                                                                                    //40
                               SafeInt(NMEA2KData),                                                                                                                                      //41
                               SafeInt(AlarmActivate),                                                                                                                                   //42
                               SafeInt(TempAlarm),                                                                                                                                       //43
                               SafeInt(VoltageAlarmHigh),                                                                                                                                //44
                               SafeInt(VoltageAlarmLow),                                                                                                                                 //45
                               SafeInt(CurrentAlarmHigh),                                                                                                                                //46
                               SafeInt(AlarmTest),                                                                                                                                       //47
                               SafeInt(AlarmLatchEnabled),                                                                                                                               //48
                               SafeInt(alarmLatch ? 1 : 0),                                                                                                                              //49
                               SafeInt(ResetAlarmLatch),                                                                                                                                 //50
                               SafeInt(MaintainMode),                                                                                                                                    //51
                               SafeInt(ResetTemp),                                                                                                                                       //52
                               SafeInt(ResetVoltage),                                                                                                                                    //53
                               SafeInt(ResetCurrent),                                                                                                                                    //54
                               SafeInt(ResetEngineRunTime),                                                                                                                              //55
                               SafeInt(ResetAlternatorOnTime),                                                                                                                           //56
                               SafeInt(ResetEnergy),                                                                                                                                     //57
                               SafeInt(ManualSOCPoint),                                                                                                                                  //58
                               SafeInt(LearningMode),                                                                                                                                    //59
                               SafeInt(LearningPaused),                                                                                                                                  //60
                               SafeInt(IgnoreLearningDuringPenalty),                                                                                                                     //61
                               SafeInt(ShowLearningDebugMessages),                                                                                                                       //62
                               SafeInt(LogAllLearningEvents),                                                                                                                            //63
                               SafeInt(CloudFeatures),                                                                                                                                   //64
                               SafeInt(LearningDryRunMode),                                                                                                                              //65
                               0,                                                                                                                                                 //66 OBSOLETE AutoSaveLearningTable
                               SafeInt(ResetLearningTable),                                                                                                                              //67
                               SafeInt(ClearOverheatHistory),                                                                                                                            //68
                               SafeInt(AutoShuntGainCorrection),                                                                                                                         //69
                               SafeInt(DynamicShuntGainFactor, 1000),                                                                                                                    //70
                               SafeInt(AutoAltCurrentZero),                                                                                                                              //71
                               SafeInt(DynamicAltCurrentZero, 1000),                                                                                                                     //72
                               SafeInt(InsulationLifePercent, 100),                                                                                                                      //73
                               SafeInt(GreaseLifePercent, 100),                                                                                                                          //74
                               SafeInt(BrushLifePercent, 100),                                                                                                                           //75
                               SafeInt(PredictedLifeHours),                                                                                                                              //76
                               SafeInt(LifeIndicatorColor),                                                                                                                              //77
                               SafeInt(WindingTempOffset),                                                                                                                               //78
                               SafeInt(ManualLifePercentage),                                                                                                                            //79
                               SafeInt(UVThresholdHigh, 100),                                                                                                                            //80
                               SafeInt(weatherModeEnabled),                                                                                                                              //81
                               SafeInt(pKwHrToday, 100),                                                                                                                                 //82
                               SafeInt(pKwHrTomorrow, 100),                                                                                                                              //83
                               SafeInt(pKwHr2days, 100),                                                                                                                                 //84
                               SafeInt(ambientTemp),                                                                                                                                     //85
                               SafeInt(baroPressure),                                                                                                                                    //86
                               SafeInt(firmwareVersionInt),                                                                                                                              //87
                               deviceIdUpper,                                                                                                                                            //88
                               deviceIdLower,                                                                                                                                            //89
                               SafeInt(ChargedEnergy_AllTime),                                                                                                                           //90
                               SafeInt(AlternatorFuelUsed_AllTime, 100),                                                                                                                 //91
                               SafeInt(PeakVoltage_AllTime, 100),                                                                                                                        //92
                               SafeInt(EngineRunTime_AllTime * 100 / 3600, 1),                                                                                                           //93
                               SafeInt(MinVoltage, 100),                                                                                                                                 //94
                               SafeInt(MinVoltage_AllTime, 100),                                                                                                                         //95
                               SafeInt(ChargeCycles, 100),                                                                                                                               //96
                               SafeInt(ChargeCycles_AllTime, 100),                                                                                                                       //97
                               SafeInt(EngineFuelUsed, 100),                                                                                                                             //98
                               SafeInt(EngineFuelUsed_AllTime, 100),                                                                                                                     //99
                               SafeInt(TotalDistance, 10),                                                                                                                               //100
                               SafeInt(TotalDistance_AllTime, 10),                                                                                                                       //101
                               SafeInt(MaxSpeed, 100),                                                                                                                                   //102
                               SafeInt(MaxSpeed_AllTime, 100),                                                                                                                           //103
                               SafeInt(SolarChargedEnergy),                                                                                                                              //104
                               SafeInt(SolarChargedEnergy_AllTime),                                                                                                                      //105
                               SafeInt(AlternatorChargedEnergy_AllTime),                                                                                                                 //106
                               SafeInt(DischargedEnergy_AllTime),                                                                                                                        //107
                               SafeInt(AvgSOC_AllTime, 100),                                                                                                                             //108
                               SafeInt(AvgSpeed_AllTime, 100),                                                                                                                           //109
                               SafeInt(AvgSpeed, 100),                                                                                                                                   //110
                               SafeInt(AlternatorOnTime_AllTime * 100 / 3600, 1),                                                                                                        //111
                               SafeInt(EngineCycles_AllTime),                                                                                                                            //112
                               SafeInt(MaxAlternatorTemperatureF_AllTime),                                                                                                               //113
                               SafeInt(MaxTemperatureThermistor_AllTime),                                                                                                                //114
                               SafeInt(MeasuredAmpsMax_AllTime, 100),                                                                                                                    //115
                               SafeInt(RPMMax_AllTime),                                                                                                                                  //116
                               SafeInt(Ignition),                                                                                                                                        //117
                               SafeInt(inBulkStage ? 1 : 0),                                                                                                                             //118
                               SafeInt((wifiWakeStart > 0 && (millis() - wifiWakeStart) < WIFI_WAKE_DURATION) ? (WIFI_WAKE_DURATION - (millis() - wifiWakeStart)) / 1000 : 0),         //119
                               SafeInt(bufferedRecordCount),                                                                                                                             //120
                               SafeInt((bufferedRecordCount * 100) / MAX_BUFFERED_RECORDS),                                                                                              //121
                               SafeInt(MAX_BUFFERED_RECORDS),                                                                                                                            //122
                               SafeInt(COGNMEA),                                                                                                                                         //123
                               SafeInt(SOGNMEA, 100),                                                                                                                                    //124
                               SafeInt(ApparentWindSpeedNMEA, 100),                                                                                                                      //125
                               SafeInt(ApparentWindAngleNMEA),                                                                                                                           //126
                               SafeInt(TrueWindSpeedNMEA, 100),                                                                                                                          //127
                               SafeInt(TrueWindAngleNMEA),                                                                                                                               //128
                               SafeInt(LeewayNMEA),                                                                                                                                      //129
                               SafeInt(VMGNMEA, 100),                                                                                                                                    //130
                               SafeInt(VMGTargetBearing),                                                                                                                                //131
                               SafeInt(VMGUseTrueWind),                                                                                                                                  //132
                               SafeInt(SENSOR_UPLOAD_INTERVAL),                                                                                                                          //133
                               SafeInt(cpuLoadCore0),                                                                                                                                    //134
                               SafeInt(cpuLoadCore0Max),                                                                                                                                 //135
                               SafeInt(cpuLoadCore1),                                                                                                                                    //136
                               SafeInt(cpuLoadCore1Max),                                                                                                                                 //137
                               SafeInt(hasForcedUpdate ? 1 : 0),                                                                                                                         //138
                               SafeInt(forcedFwVersionInt),                                                                                                                              //139
                               (forcedUpdateDeadline),                                                                                                                                   //140
                               SafeInt(stateRevision),                                                                                                                                   //141
                               SafeInt(hardwarePresent),                                                                                                                                 //142
                               SafeInt(imu_accel_x_raw, 1000),                                                                                                                           //143
                               SafeInt(imu_accel_y_raw, 1000),                                                                                                                           //144
                               SafeInt(imu_accel_z_raw, 1000),                                                                                                                           //145
                               SafeInt(imu_gyro_x_raw, 100),                                                                                                                             //146
                               SafeInt(imu_gyro_y_raw, 100),                                                                                                                             //147
                               SafeInt(imu_gyro_z_raw, 100),                                                                                                                             //148
                               SafeInt(imuWindow->accel_x_min),                                                                                                                          //149
                               SafeInt(imuWindow->accel_x_max),                                                                                                                          //150
                               SafeInt(imuWindow->accel_x_valid_us > 0 ? (int)((int64_t)imuWindow->accel_x_area_v_us / (int64_t)imuWindow->accel_x_valid_us) : 0),                       //151
                               SafeInt(imuWindow->accel_y_min),                                                                                                                          //152
                               SafeInt(imuWindow->accel_y_max),                                                                                                                          //153
                               SafeInt(imuWindow->accel_y_valid_us > 0 ? (int)((int64_t)imuWindow->accel_y_area_v_us / (int64_t)imuWindow->accel_y_valid_us) : 0),                       //154
                               SafeInt(imuWindow->accel_z_min),                                                                                                                          //155
                               SafeInt(imuWindow->accel_z_max),                                                                                                                          //156
                               SafeInt(imuWindow->accel_z_valid_us > 0 ? (int)((int64_t)imuWindow->accel_z_area_v_us / (int64_t)imuWindow->accel_z_valid_us) : 0),                       //157
                               SafeInt(imuWindow->gyro_x_min),                                                                                                                           //158
                               SafeInt(imuWindow->gyro_x_max),                                                                                                                           //159
                               SafeInt(imuWindow->gyro_x_valid_us > 0 ? (int)((int64_t)imuWindow->gyro_x_area_v_us / (int64_t)imuWindow->gyro_x_valid_us) : 0),                          //160
                               SafeInt(imuWindow->gyro_y_min),                                                                                                                           //161
                               SafeInt(imuWindow->gyro_y_max),                                                                                                                           //162
                               SafeInt(imuWindow->gyro_y_valid_us > 0 ? (int)((int64_t)imuWindow->gyro_y_area_v_us / (int64_t)imuWindow->gyro_y_valid_us) : 0),                          //163
                               SafeInt(imuWindow->gyro_z_min),                                                                                                                           //164
                               SafeInt(imuWindow->gyro_z_max),                                                                                                                           //165
                               SafeInt(imuWindow->gyro_z_valid_us > 0 ? (int)((int64_t)imuWindow->gyro_z_area_v_us / (int64_t)imuWindow->gyro_z_valid_us) : 0),                          //166
                               SafeInt(imuWindow->heel_min),                                                                                                                             //167
                               SafeInt(imuWindow->heel_max),                                                                                                                             //168
                               SafeInt(imuWindow->heel_valid_us > 0 ? (int)((int64_t)imuWindow->heel_area_v_us / (int64_t)imuWindow->heel_valid_us) : 0),                                //169
                               SafeInt(imuWindow->pitch_min),                                                                                                                            //170
                               SafeInt(imuWindow->pitch_max),                                                                                                                            //171
                               SafeInt(imuWindow->pitch_valid_us > 0 ? (int)((int64_t)imuWindow->pitch_area_v_us / (int64_t)imuWindow->pitch_valid_us) : 0),                             //172
                               SafeInt(imuWindow->vertical_accel_min),                                                                                                                   //173
                               SafeInt(imuWindow->vertical_accel_max),                                                                                                                   //174
                               SafeInt(imuWindow->vertical_accel_valid_us > 0 ? (int)((int64_t)imuWindow->vertical_accel_area_v_us / (int64_t)imuWindow->vertical_accel_valid_us) : 0),  //175
                               SafeInt(imuWindow->total_accel_min),                                                                                                                      //176
                               SafeInt(imuWindow->total_accel_max),                                                                                                                      //177
                               SafeInt(imuWindow->total_accel_valid_us > 0 ? (int)((int64_t)imuWindow->total_accel_area_v_us / (int64_t)imuWindow->total_accel_valid_us) : 0),           //178
                               SafeInt(imuWindow->slam_count),                                                                                                                           //179
                               SafeInt(imuWindow->slam_peak_max),                                                                                                                        //180
                               SafeInt(imu_slam_count_lifetime),                                                                                                                         //181
                               SafeInt(imu_capsize_count),                                                                                                                               //182
                               SafeInt(imu_pitchpole_count),                                                                                                                             //183
                               SafeInt(imuWindow->heel_change_60s),                                                                                                                      //184
                               SafeInt(imuWindow->heel_deviation_60s),                                                                                                                   //185
                               SafeInt(imuWindow->pitch_change_60s),                                                                                                                     //186
                               SafeInt(imuWindow->pitch_deviation_60s),                                                                                                                  //187
                               SafeInt(imuWindow->wave_period),                                                                                                                          //188
                               SafeInt(imu_heel_max_lifetime, 100),                                                                                                                      //189
                               SafeInt(imu_pitch_max_lifetime, 100),                                                                                                                     //190
                               SafeInt(imu_slam_peak_lifetime, 1000),                                                                                                                    //191
                               SafeInt(imuEnabled ? 1 : 0),                                                                                                                              //192
                               SafeInt(imuMountOrientation),                                                                                                                             //193
                               SafeInt(imu_fifo_overrun_count),                                                                                                                          //194
                               SafeInt(imu_i2c_error_count),                                                                                                                             //195
                               SafeInt(imu_unknown_tag_count),                                                                                                                           //196
                               SafeInt(imuRingBuffer->accel_dropped),                                                                                                                    //197
                               SafeInt(imuRingBuffer->gyro_dropped),                                                                                                                     //198
                               SafeInt(imu_total_samples_accel),                                                                                                                         //199
                               SafeInt(imu_total_samples_gyro),                                                                                                                          //200
                               SafeInt(IMUReadTime2),                                                                                                                                    //201
                               SafeInt(IMUReadTime),                                                                                                                                     //202
                               SafeInt(adsI2CErrorCount),                                                                                                                                //203
                               SafeInt(tempPIDActive ? 1 : 0),                                                                                                                           //204
                               SafeInt(tempPIDInput_d, 100),                                                                                                                             //205
                               SafeInt(tempPIDSetpoint_d, 100),                                                                                                                          //206
                               SafeInt(thermalPenaltyAmps, 100),                                                                                                                         //207
                               SafeInt(innerTermP, 100),                                                                                                                                 //208
                               SafeInt(innerTermI, 100),                                                                                                                                 //209
                               SafeInt(innerTermD, 100),                                                                                                                                 //210
                               SafeInt(outerTermP, 100),                                                                                                                                 //211
                               SafeInt(outerTermI, 100),                                                                                                                                 //212
                               SafeInt(outerTermD, 100),                                                                                                                                 //213
                               SafeInt(thermalSlopeFPerSec, 1000),                                                                                                                       //214
                               SafeInt(AbsorptionVoltage * 100),                                                                                                                         // 215
                               SafeInt(AbsorptionTimeoutMs),                                                                                                                             // 216
                               SafeInt(bulkVoltageHoldMs),                                                                                                                               // 217
                               SafeInt(chargeStageDisplay),                                                                                                                              // 218
                               SafeInt(voltageControlActive),                                                                                                                            // 219
                               SafeInt(ChargingVoltageTarget * 100),                                                                                                                     // 220
                               SafeInt((ChargingVoltageTarget - getBatteryVoltage()) * 100),                                                                                             // 221
                               SafeInt(Icv * 100),                                                                                                                                       // 222
                               SafeInt(cv_I * 100),                                                                                                                                      // 223
                               SafeInt(capLimitMode),
                               SafeInt(TargetVoltageMode),              //225
                               SafeInt(TargetVoltageSetpoint, 100),     // 226
                               SafeInt(RebulkCurrent_A, 100),           // 227
                               SafeInt(UseFloat),                       // 228
                               SafeInt(inIdleStage),                    // 229
                               SafeInt(referenceFinalized),             // 230
                               SafeInt(sessionErrorCount),              // 231
                               SafeInt(anomalyMarginAmps, 10),          // 232  — 1 decimal, divide by 10 in JS
                               SafeInt(anomalyAlarmThreshold),          // 233
                               SafeInt(anomalyAlarmEnable),             // 234
                               SafeInt(degradationThreshold, 100),      // 235
                               SafeInt(ft_rai_total.worstWindow),       // 236
                               SafeInt(ft_rai_total.worstSession),      // 237
                               SafeInt(ft_rai_ina228.worstWindow),      // 238
                               SafeInt(ft_rai_ina228.worstSession),     // 239
                               SafeInt(ft_rai_ads_state.worstWindow),   // 240
                               SafeInt(ft_rai_ads_state.worstSession),  // 241
                               SafeInt(ft_rai_bmp_state.worstWindow),   // 242
                               SafeInt(ft_rai_bmp_state.worstSession),  // 243
                               SafeInt(ft_rai_imu.worstWindow),         // 244
                               SafeInt(ft_rai_imu.worstSession),        // 245
                               SafeInt(fsWriteQueueDrops),              // 246
                               SafeInt(TempAlarmLow),                   // 247
                               SafeInt(VoltageKd * g_fastOvDvdt, 100), // 248
                               SafeInt(tempReadFailCount),              // 249
                               SafeInt(tempCrcFailCount),               // 250
                               SafeInt(tempCrcRecoveredCount),          // 251
                               SafeInt(tempAllFFCount),                 // 252
                               SafeInt(tempPowerOn85Count),             // 253
                               SafeInt(tempOutOfRangeCount),            // 254
                               SafeInt(tempRequestFailCount),           // 255
                               SafeInt(tempConnectedFailCount),         // 256
                               SafeInt(tempResolutionFixCount),         // 257
                               SafeInt(tempRereadFailCount),            // 258
                               SafeInt(tempResolutionFixCrcFailCount),  // 259
                               SafeInt(tempEnumerateFailCount),         // 260
                               SafeInt(warmupCeiling),                  // 261
                               SafeInt(imu_min_moving_gentle),          // 262
                               SafeInt(imu_min_moving_moderate),        // 263
                               SafeInt(imu_min_moving_rough),           // 264
                               SafeInt(imu_min_moving_extreme),         // 265
                               SafeInt(imu_min_stat_gentle),            // 266
                               SafeInt(imu_min_stat_moderate),          // 267
                               SafeInt(imu_min_stat_rough),             // 268
                               SafeInt(imu_min_stat_extreme),           // 269
                               SafeInt(imu_heel_deviation_120s, 100),   // 270 — ×100, 2dp degrees
                               SafeInt(imu_pitch_deviation_120s, 100),  // 271 — ×100, 2dp degrees
                               SafeInt(imu_heading_swing_120s, 10),     // 272 — ×10, 1dp degrees; -10 = no compass data
                               SafeInt(LoadDumpDtThresh),               // 273 — A/s threshold for load dump detection
                               SafeInt(LoadDumpCurrentDrop),            // 274 — A current drop cap on load dump
                               SafeInt(g_dBcur_dt, 10),                 // 275 — ×10, 1dp A/s battery current rate of change
                               (int)g_loadDumpActive,                   // 276 — 1 if load dump feedforward is active
                               (int)CVTuningMode,                       // 277
                               SafeInt(cvWaveAmplitudeV, 100),          // 278 — ×100, 2dp V
                               (int)cvWavePeriodSec,                    // 279
                               SafeInt(cvKOvershoot, 10),               // 280 — ×10, 1dp
                               (int)cvConsecutiveReads,                 // 281
                               (int)ThermalTuningMode,                  // 282
                               SafeInt(thermalWaveLowF, 10),            // 283 — ×10, 1dp °F
                               SafeInt(thermalWaveHighF, 10),           // 284 — ×10, 1dp °F
                               SafeInt(thermalWaveHalfPeriodMin, 10),   // 285 — ×10, 1dp min
                               SafeInt(thermalKOvershoot, 100),         // 286 — ×100, 2dp
                               SafeInt(thermalKUndershoot, 100),        // 287 — ×100, 2dp
                               SafeInt(thermalSettleThreshF, 10),       // 288 — ×10, 1dp °F
                               (int)thermalConsecutiveReads,            // 289
                               SafeInt(thermalLiveScoreVal[0], 10000),  // 290 — ×10000
                               SafeInt(thermalLiveScoreVal[1], 10000),  // 291 — ×10000
                               SafeInt(thermalLiveScoreVal[2], 10000),  // 292 — ×10000
                               SafeInt(thermalLiveScoreVal[3], 10000),  // 293 — ×10000
                               (ThermalTuningMode && thermalTuningScore.testStarted && thermalTuningScore.ringInDone) ? 1 : 0  // 294
    );
    if (payload2Len < 0 || payload2Len >= PAYLOAD2_SIZE) {
      Serial.printf("payload2 truncated or format error: %d\n", payload2Len);
      return;
    }

    events.send(payload2, "CSVData2");
    lastpayload2send = now;
    lastEventSourceSend = now;
    sentSomething = true;
  }

  // PRIORITY 4: CSVData3 (settings data - every 2 seconds)
  if (!sentSomething && now - lastpayload3send >= 2000 && events.count() > 0) {
    static char *payload3 = nullptr;
    static const size_t PAYLOAD3_SIZE = 1400;
    if (!payload3) {
      payload3 = (char *)ps_malloc(PAYLOAD3_SIZE);  // allocated to PSRAM
      if (!payload3) {
        Serial.println("FATAL: payload3 ps_malloc failed");
        return;
      }
    }
    /// ALL THIS SAFEINT STUFF WAS A HUGE WASTE OF TIME, BAD ADVICE, COULD HAVE JUST SENT ROUNDED FLOATS FOR 1 Byte (or bit?) xtra
    //WifiSendTime was 834uS before increasing csv3 payload size from 1100 to 1400     No change after.  Again, this separation into groups and worry about wifi packet size seems like AI nonsense.

    int payload3Len = snprintf(payload3, PAYLOAD3_SIZE,
                               "%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,"
                               "%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,"
                               "%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,"
                               "%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,"
                               "%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,"
                               "%d,%d,%d,%d,%d,%d",

                               CSV3_FIELD_COUNT,                                       // prepended count
                               SafeInt(TemperatureLimitF),                             // 0
                               SafeInt(BulkVoltage, 100),                              // 1
                               SafeInt(wavePeriod),                                    // 2
                               SafeInt(FloatVoltage, 100),                             // 3
                               SafeInt(SwitchingFrequency),                            // 4
                               SafeInt(yyMin),                                         // 5
                               SafeInt(FieldAdjustmentInterval),                       // 6
                               SafeInt(ManualDutyTarget),                              // 7
                               SafeInt(SwitchControlOverride),                         // 8
                               SafeInt(waveAmplitude),                                 // 9
                               SafeInt(CurrentThreshold, 100),                         // 10
                               SafeInt(PeukertExponent_scaled),                        // 11
                               SafeInt(ChargeEfficiency_scaled),                       // 12
                               SafeInt(ChargedVoltage_Scaled),                         // 13
                               SafeInt(TailCurrent, 10),                               // 14  (× 10 so JS can show 1 decimal)
                               SafeInt(ChargedDetectionTime),                          // 15
                               SafeInt(IgnoreTemperature),                             // 16
                               SafeInt(bmsLogic),                                      // 17
                               SafeInt(bmsLogicLevelOff),                              // 18
                               SafeInt(FourWay),                                       // 19
                               SafeInt(RPMScalingFactor),                              // 20
                               SafeInt(MaximumAllowedBatteryAmps),                     // 21
                               SafeInt(BatteryVoltageSource),                          // 22
                               SafeInt(LearningUpwardEnabled),                         // 23
                               SafeInt(LearningDownwardEnabled),                       // 24
                               SafeInt(AlternatorNominalAmps),                         // 25
                               SafeInt(LearningUpStep, 100),                           // 26
                               SafeInt(LearningDownStep, 100),                         // 27
                               SafeInt(AmbientTempCorrectionFactor, 100),              // 28
                               SafeInt(xTime),                                         // 29
                               SafeInt(MinLearningInterval),                           // 30
                               SafeInt(SafeOperationThreshold),                        // 31
                               SafeInt(PidKp, 1000),                                   // 32
                               SafeInt(PidKi, 1000),                                   // 33
                               SafeInt(PidKd, 1000),                                   // 34
                               SafeInt(PidSampleDivisor),                              // 35
                               SafeInt(MaxTableValue, 100),                            // 36
                               0,                                                      // 37 OBSOLETE MinTableValue
                               SafeInt(MaxPenaltyPercent, 100),                        // 38
                               SafeInt(MaxPenaltyDuration / 1000),                     // 39
                               SafeInt(NeighborLearningFactor, 1000),                  // 40
                               SafeInt(yyMax),                                         // 41
                               SafeInt(LearningMemoryDuration / 86400000),             // 42
                               SafeInt(EnableNeighborLearning),                        // 43
                               SafeInt(EnableAmbientCorrection),                       // 44
                               SafeInt(TuningMode),                                    // 45
                               0,                                                      // 46 OBSOLETE LearningTableSaveInterval
                               SafeInt(rpmCurrentTable[0]),                            // 47
                               SafeInt(rpmCurrentTable[1]),                            // 48
                               SafeInt(rpmCurrentTable[2]),                            // 49
                               SafeInt(rpmCurrentTable[3]),                            // 50
                               SafeInt(rpmCurrentTable[4]),                            // 51
                               SafeInt(rpmCurrentTable[5]),                            // 52
                               SafeInt(rpmCurrentTable[6]),                            // 53
                               SafeInt(rpmCurrentTable[7]),                            // 54
                               SafeInt(rpmCurrentTable[8]),                            // 55
                               SafeInt(rpmCurrentTable[9]),                            // 56
                               SafeInt(currentRPMTableIndex),                          // 57
                               SafeInt(pidInitialized ? 1 : 0),                        // 58
                               SafeInt(ShuntResistanceMicroOhm),                       // 59
                               SafeInt(InvertAltAmps),                                 // 60
                               SafeInt(InvertBattAmps),                                // 61
                               SafeInt(MaxDuty),                                       // 62
                               SafeInt(MinDuty),                                       // 63
                               SafeInt(FieldResistance, 100),                          // 64
                               SafeInt(maxPoints),                                     // 65
                               SafeInt(AlternatorCOffset, 100),                        // 66
                               SafeInt(BatteryCOffset, 100),                           // 67
                               SafeInt(BatteryCapacity_Ah),                            // 68
                               SafeInt(AmpSensorRange),                                // 69
                               SafeInt(R_fixed, 100),                                  // 70
                               SafeInt(Beta, 100),                                     // 71
                               SafeInt(T0_C, 100),                                     // 72
                               SafeInt(TempSource),                                    // 73
                               SafeInt(IgnitionOverride),                              // 74
                               SafeInt(FLOAT_DURATION),                                // 75
                               SafeInt(PulleyRatio, 100),                              // 76
                               SafeInt(BatteryCurrentSource),                          // 77
                               SafeInt(overheatCount[0]),                              // 78
                               SafeInt(overheatCount[1]),                              // 79
                               SafeInt(overheatCount[2]),                              // 80
                               SafeInt(overheatCount[3]),                              // 81
                               SafeInt(overheatCount[4]),                              // 82
                               SafeInt(overheatCount[5]),                              // 83
                               SafeInt(overheatCount[6]),                              // 84
                               SafeInt(overheatCount[7]),                              // 85
                               SafeInt(overheatCount[8]),                              // 86
                               SafeInt(overheatCount[9]),                              // 87
                               SafeInt(cumulativeNoOverheatTime[0] / 1000),            // 88
                               SafeInt(cumulativeNoOverheatTime[1] / 1000),            // 89
                               SafeInt(cumulativeNoOverheatTime[2] / 1000),            // 90
                               SafeInt(cumulativeNoOverheatTime[3] / 1000),            // 91
                               SafeInt(cumulativeNoOverheatTime[4] / 1000),            // 92
                               SafeInt(cumulativeNoOverheatTime[5] / 1000),            // 93
                               SafeInt(cumulativeNoOverheatTime[6] / 1000),            // 94
                               SafeInt(cumulativeNoOverheatTime[7] / 1000),            // 95
                               SafeInt(cumulativeNoOverheatTime[8] / 1000),            // 96
                               SafeInt(cumulativeNoOverheatTime[9] / 1000),            // 97
                               SafeInt(totalLearningEvents),                           // 98
                               SafeInt(totalOverheats),                                // 99
                               SafeInt(totalSafeHours),                                // 100
                               SafeInt(averageTableValue, 100),                        // 101
                               SafeInt(timeSinceLastOverheat / 1000),                  // 102
                               SafeInt(learningTargetFromRPM, 100),                    // 103
                               SafeInt(ambientTempCorrection, 100),                    // 104
                               SafeInt(finalLearningTarget, 100),                      // 105
                               SafeInt(overheatingPenaltyTimer / 1000),                // 106
                               SafeInt(overheatingPenaltyAmps, 100),                   // 107
                               SafeInt(pidSetpoint, 100),                              // 108
                               SafeInt(TempToUse),                                     // 109
                               SafeInt(rpmTableRPMPoints[0]),                          // 110
                               SafeInt(rpmTableRPMPoints[1]),                          // 111
                               SafeInt(rpmTableRPMPoints[2]),                          // 112
                               SafeInt(rpmTableRPMPoints[3]),                          // 113
                               SafeInt(rpmTableRPMPoints[4]),                          // 114
                               SafeInt(rpmTableRPMPoints[5]),                          // 115
                               SafeInt(rpmTableRPMPoints[6]),                          // 116
                               SafeInt(rpmTableRPMPoints[7]),                          // 117
                               SafeInt(rpmTableRPMPoints[8]),                          // 118
                               SafeInt(rpmTableRPMPoints[9]),                          // 119
                               SafeInt(LearningSettlingPeriod),                        // 120
                               SafeInt(LearningRPMChangeThreshold),                    // 121
                               SafeInt(LearningTempHysteresis),                        // 122
                               SafeInt(fuelTableRPM[0]),                               // 123
                               SafeInt(fuelTableRPM[1]),                               // 124
                               SafeInt(fuelTableRPM[2]),                               // 125
                               SafeInt(fuelTableRPM[3]),                               // 126
                               SafeInt(fuelTableRPM[4]),                               // 127
                               SafeInt(fuelTableRPM[5]),                               // 128
                               SafeInt(fuelTableRPM[6]),                               // 129
                               SafeInt(fuelTableRPM[7]),                               // 130
                               SafeInt(fuelTableRPM[8]),                               // 131
                               SafeInt(fuelTableRPM[9]),                               // 132
                               SafeInt(fuelTableGPH[0], 100),                          // 133
                               SafeInt(fuelTableGPH[1], 100),                          // 134
                               SafeInt(fuelTableGPH[2], 100),                          // 135
                               SafeInt(fuelTableGPH[3], 100),                          // 136
                               SafeInt(fuelTableGPH[4], 100),                          // 137
                               SafeInt(fuelTableGPH[5], 100),                          // 138
                               SafeInt(fuelTableGPH[6], 100),                          // 139
                               SafeInt(fuelTableGPH[7], 100),                          // 140
                               SafeInt(fuelTableGPH[8], 100),                          // 141
                               SafeInt(fuelTableGPH[9], 100),                          // 142
                               SafeInt(stateRevision),                                 // 143
                               SafeInt(SetpointRampRate, 100),                         // 144
                               SafeInt(DutyRampRate, 100),                             // 145
                               SafeInt(SettleTimeBeforeCut),                           // 146
                               SafeInt(TempWarnExcess, 100),                           // 147
                               SafeInt(TempCritExcess, 100),                           // 148
                               SafeInt(TempSustainedTimeout / 1000),                   // 149
                               SafeInt(VoltageSpikeMargin, 100),                       // 150
                               SafeInt(VoltageDisagreeThreshold, 100),                 // 151
                               SafeInt(VoltageDisagreeTimeout / 1000),                 // 152
                               SafeInt(rpmMinDutyTable[0], 100),                       // 153
                               SafeInt(rpmMinDutyTable[1], 100),                       // 154
                               SafeInt(rpmMinDutyTable[2], 100),                       // 155
                               SafeInt(rpmMinDutyTable[3], 100),                       // 156
                               SafeInt(rpmMinDutyTable[4], 100),                       // 157
                               SafeInt(rpmMinDutyTable[5], 100),                       // 158
                               SafeInt(rpmMinDutyTable[6], 100),                       // 159
                               SafeInt(rpmMinDutyTable[7], 100),                       // 160
                               SafeInt(rpmMinDutyTable[8], 100),                       // 161
                               SafeInt(rpmMinDutyTable[9], 100),                       // 162
                               SafeInt(rpmCapCurrentTable[0], 100),                    // 163
                               SafeInt(rpmCapCurrentTable[1], 100),                    // 164
                               SafeInt(rpmCapCurrentTable[2], 100),                    // 165
                               SafeInt(rpmCapCurrentTable[3], 100),                    // 166
                               SafeInt(rpmCapCurrentTable[4], 100),                    // 167
                               SafeInt(rpmCapCurrentTable[5], 100),                    // 168
                               SafeInt(rpmCapCurrentTable[6], 100),                    // 169
                               SafeInt(rpmCapCurrentTable[7], 100),                    // 170
                               SafeInt(rpmCapCurrentTable[8], 100),                    // 171
                               SafeInt(rpmCapCurrentTable[9], 100),                    // 172
                               SafeInt(VoltageKp, 100),                                // 173
                               SafeInt(VoltageLoopInterval),                           // 174
                               SafeInt(FIELD_COLLAPSE_DELAY),                          // 175
                               SafeInt(SetpointRiseRate, 100),                         // 176
                               SafeInt(SetpointFallRate, 100),                         // 177
                               SafeInt(PIDTrackingGain, 100),                          // 178
                               SafeInt(CAPSIZE_THRESHOLD_DEG),                         // 179
                               SafeInt(PITCHPOLE_THRESHOLD_DEG),                       // 180
                               SafeInt(SLAM_THRESHOLD_G, 10),                          // 181
                               SafeInt(imuMountOrientation),                           // 182
                               SafeInt(socInfoAvailable),                              // 183
                               SafeInt(TailCurrent_A, 100),                            // 184
                               SafeInt(RebulkVoltage, 100),                            // 185
                               SafeInt(rebulkDebounceTime),                            // 186
                               SafeInt(MinFloatTime),                                  // 187
                               SafeInt(SOC_BlockRebulk_percent),                       // 188
                               SafeInt(SOC_AllowRebulk_percent),                       // 189
                               SafeInt(accelEnabled),                                  //190
                               SafeInt(DutySlowRampRate, 100),                         // 191
                               SafeInt(ShutdownPhase2HoldMs),                          // 192
                               SafeInt(learningUpCount[0]),                            // 193
                               SafeInt(learningUpCount[1]),                            // 194
                               SafeInt(learningUpCount[2]),                            // 195
                               SafeInt(learningUpCount[3]),                            // 196
                               SafeInt(learningUpCount[4]),                            // 197
                               SafeInt(learningUpCount[5]),                            // 198
                               SafeInt(learningUpCount[6]),                            // 199
                               SafeInt(learningUpCount[7]),                            // 200
                               SafeInt(learningUpCount[8]),                            // 201
                               SafeInt(learningUpCount[9]),                            // 202
                               SafeInt(TempPIDKp, 1000),                               // 203
                               SafeInt(TempPIDKi, 1000),                               // 204
                               0,                                                      // 205 (was TempPIDKd, removed)
                               SafeInt(ThermalLookaheadSec),                           // 206
                               SafeInt(TempPIDIntervalMs),                             // 207
                               SafeInt(TempPIDFilterAlpha, 1000),                      // 208
                               0,                                                      // 209 (was TempPIDStaleMs, removed)
                               0,                                                      // 210 (was TempPIDAntiWindupMarginA, removed)
                               SafeInt(FreeInternalRam),                               // 211
                               SafeInt(TotalInternalRam),                              // 212
                               SafeInt(LargestInternalBlock),                          // 213
                               SafeInt(FreePSRAM),                                     // 214
                               SafeInt(TotalPSRAM),                                    // 215
                               SafeInt(Heapfrag),                                      // 216
                               0,                                                      // 217 (was TempPIDKdExternal, removed)
                               SafeInt(VoltageKi, 100),                                // 218
                               (int)rpmCapPowerTable[0],                               // 219
                               (int)rpmCapPowerTable[1],                               // 220
                               (int)rpmCapPowerTable[2],                               // 221
                               (int)rpmCapPowerTable[3],                               // 222
                               (int)rpmCapPowerTable[4],                               // 223
                               (int)rpmCapPowerTable[5],                               // 224
                               (int)rpmCapPowerTable[6],                               // 225
                               (int)rpmCapPowerTable[7],                               // 226
                               (int)rpmCapPowerTable[8],                               // 227
                               (int)rpmCapPowerTable[9],                               // 228
                               SafeInt(VoltageTrimLimit, 100),                         // 229
                               SafeInt(ft_ReadAnalogInputs.worstWindow),        // 230 — Read Analog Inputs worst 5s window (µs)
                               SafeInt(ft_ReadAnalogInputs.worstSession),       // 231 — Read Analog Inputs worst session (µs)
                               SafeInt(ft_AdjustFieldLearnMode.worstWindow),    // 232 — Alternator Control Logic worst 5s window (µs)
                               SafeInt(ft_AdjustFieldLearnMode.worstSession),   // 233 — Alternator Control Logic worst session (µs)
                               SafeInt(ft_uploadSensorHistory.worstWindow),     // 234 — Upload Sensor History worst 5s window (µs)
                               SafeInt(ft_uploadSensorHistory.worstSession),    // 235 — Upload Sensor History worst session (µs)
                               SafeInt(ft_uploadBufferedRecords.worstWindow),   // 236 — Upload Buffered Records worst 5s window (µs)
                               SafeInt(ft_uploadBufferedRecords.worstSession),  // 237 — Upload Buffered Records worst session (µs)
                               SafeInt(ft_buildConfigPayload.worstWindow),      // 238 — Build Config Payload worst 5s window (µs)
                               SafeInt(ft_buildConfigPayload.worstSession),     // 239 — Build Config Payload worst session (µs)
                               SafeInt(VeTime2),                                       //240
                               0,                                                      // 241 OBSOLETE systemIDActive
                               0,                                                      // 242 OBSOLETE systemIDResultsReady
                               (int)systemIDRiseDelay_ms[0],                           // 243
                               (int)systemIDRiseDelay_ms[1],                           // 244
                               (int)systemIDRiseDelay_ms[2],                           // 245
                               (int)systemIDFallDelay_ms[0],                           // 246
                               (int)systemIDFallDelay_ms[1],                           // 247
                               (int)systemIDFallDelay_ms[2],                           // 248
                               (int)systemIDRiseAvg_ms,                                // 249
                               (int)systemIDFallAvg_ms,                                // 250
                               (int)InputFilterTC,                                     // 251
                               (int)SystemIDStepAmplitude,                             // 252
                               SafeInt(HardOCTripAmps, 10),                            // 253 — ×10, 1 decimal
                               SafeInt(HardOCDebounceMs),                              // 254 — raw ms
                               SafeInt(IExcessK, 10),                                  // 255 — ×10, 1 decimal
                               SafeInt(IExcessN),                                       // 256 — raw int
                               SafeInt(IExcessKBleed, 100),                             // 257 — ×100, 2 decimals
                               SafeInt(IgnoreRPM),                                      // 258
                               SafeInt(MinRPMForField),                                 // 259
                               0,                                                       // 260 (was ThermalTimeConstantSec, removed)
                               SafeInt(AwBleedRate, 10),                                // 261 — ×10, 1 decimal
                               SafeInt(AwRecoverRate, 10),                              // 262 — ×10, 1 decimal
                               SafeInt(KSoft, 10),                                      // 263 — ×10, 1 decimal
                               SafeInt(KHard, 10),                                      // 264 — ×10, 1 decimal
                               SafeInt(IExcessReseedFrac, 100),                         // 265 — ×100, 2 decimal
                               (int)AwSeedProtectMs,                                    // 266
                               SafeInt(VoltageKd, 100),                                 // 267
                               0,                                                       // 268 (was ThermistorFilterAlpha, removed)
                               SafeInt(displayTempUnit),                                // 269
                               SafeInt(WarmupRampRate, 10),                             // 270 — ×10, 1 decimal
                               (int)nvsPhase,                                           // 271 — NVS drain phase (0=idle, 1-8=writing, 9=commit)
                               SafeInt(ft_saveNVSData.worstWindow),                     // 272 — saveNVSData worst 5s window (µs)
                               SafeInt(ft_saveNVSData.worstSession),                    // 273 — saveNVSData worst session (µs)
                               SafeInt(nvsCycleMs),                                     // 274 — last full NVS drain duration (ms)
                               SafeInt(ft_FlushFileWriteQueue.worstWindow),             // 275 — FlushFileWriteQueue worst 5s window (µs)
                               SafeInt(ft_FlushFileWriteQueue.worstSession)             // 276 — FlushFileWriteQueue worst session (µs)
    );
    if (payload3Len < 0 || payload3Len >= PAYLOAD3_SIZE) {
      Serial.printf("payload3 truncated or format error: %d\n", payload3Len);
      return;
    }

    events.send(payload3, "CSVData3");
    lastpayload3send = now;
    lastEventSourceSend = now;
    sentSomething = true;
  }

  // PRIORITY 5: TimestampData (staleness data - every 3 seconds)
  if (!sentSomething && now - lastTimestampSend >= 3000 && events.count() > 0) {

    static char *timestampPayload = nullptr;
    static const size_t TIMESTAMP_PAYLOAD_SIZE = 400;
    if (!timestampPayload) {
      timestampPayload = (char *)ps_malloc(TIMESTAMP_PAYLOAD_SIZE);  // allocated to PSRAM
      if (!timestampPayload) {
        Serial.println("FATAL: timestampPayload ps_malloc failed");
        return;
      }
    }
    int timestampPayloadLen = snprintf(timestampPayload, TIMESTAMP_PAYLOAD_SIZE,
                                       "%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu",
                                       (unsigned long)TS_FIELD_COUNT,
                                       (dataTimestamps[IDX_HEADING_NMEA] == 0) ? 999999 : (now - dataTimestamps[IDX_HEADING_NMEA]),              // 0
                                       (dataTimestamps[IDX_LATITUDE_NMEA] == 0) ? 999999 : (now - dataTimestamps[IDX_LATITUDE_NMEA]),            // 1
                                       (dataTimestamps[IDX_LONGITUDE_NMEA] == 0) ? 999999 : (now - dataTimestamps[IDX_LONGITUDE_NMEA]),          // 2
                                       (dataTimestamps[IDX_SATELLITE_COUNT] == 0) ? 999999 : (now - dataTimestamps[IDX_SATELLITE_COUNT]),        // 3
                                       (dataTimestamps[IDX_VICTRON_VOLTAGE] == 0) ? 999999 : (now - dataTimestamps[IDX_VICTRON_VOLTAGE]),        // 4
                                       (dataTimestamps[IDX_VICTRON_CURRENT] == 0) ? 999999 : (now - dataTimestamps[IDX_VICTRON_CURRENT]),        // 5
                                       (dataTimestamps[IDX_ALTERNATOR_TEMP] == 0) ? 999999 : (now - dataTimestamps[IDX_ALTERNATOR_TEMP]),        // 6
                                       (dataTimestamps[IDX_THERMISTOR_TEMP] == 0) ? 999999 : (now - dataTimestamps[IDX_THERMISTOR_TEMP]),        // 7
                                       (dataTimestamps[IDX_RPM] == 0) ? 999999 : (now - dataTimestamps[IDX_RPM]),                               // 8
                                       (dataTimestamps[IDX_MEASURED_AMPS] == 0) ? 999999 : (now - dataTimestamps[IDX_MEASURED_AMPS]),            // 9
                                       (dataTimestamps[IDX_BATTERY_V] == 0) ? 999999 : (now - dataTimestamps[IDX_BATTERY_V]),                   // 10
                                       (dataTimestamps[IDX_IBV] == 0) ? 999999 : (now - dataTimestamps[IDX_IBV]),                               // 11
                                       (dataTimestamps[IDX_BCUR] == 0) ? 999999 : (now - dataTimestamps[IDX_BCUR]),                             // 12
                                       (dataTimestamps[IDX_CHANNEL3V] == 0) ? 999999 : (now - dataTimestamps[IDX_CHANNEL3V]),                   // 13
                                       (dataTimestamps[IDX_DUTY_CYCLE] == 0) ? 999999 : (now - dataTimestamps[IDX_DUTY_CYCLE]),                 // 14
                                       (dataTimestamps[IDX_FIELD_VOLTS] == 0) ? 999999 : (now - dataTimestamps[IDX_FIELD_VOLTS]),               // 15
                                       (dataTimestamps[IDX_FIELD_AMPS] == 0) ? 999999 : (now - dataTimestamps[IDX_FIELD_AMPS]),                 // 16
                                       (dataTimestamps[IDX_COG_NMEA] == 0) ? 999999 : (now - dataTimestamps[IDX_COG_NMEA]),                     // 17
                                       (dataTimestamps[IDX_SOG_NMEA] == 0) ? 999999 : (now - dataTimestamps[IDX_SOG_NMEA]),                     // 18
                                       (dataTimestamps[IDX_APPARENT_WIND_SPEED] == 0) ? 999999 : (now - dataTimestamps[IDX_APPARENT_WIND_SPEED]), // 19
                                       (dataTimestamps[IDX_APPARENT_WIND_ANGLE] == 0) ? 999999 : (now - dataTimestamps[IDX_APPARENT_WIND_ANGLE]), // 20
                                       (dataTimestamps[IDX_TRUE_WIND_SPEED] == 0) ? 999999 : (now - dataTimestamps[IDX_TRUE_WIND_SPEED]),        // 21
                                       (dataTimestamps[IDX_TRUE_WIND_ANGLE] == 0) ? 999999 : (now - dataTimestamps[IDX_TRUE_WIND_ANGLE]),        // 22
                                       (dataTimestamps[IDX_LEEWAY] == 0) ? 999999 : (now - dataTimestamps[IDX_LEEWAY]),                         // 23
                                       (dataTimestamps[IDX_VMG] == 0) ? 999999 : (now - dataTimestamps[IDX_VMG]),                               // 24
                                       (dataTimestamps[IDX_BARO_PRESSURE] == 0) ? 999999 : (now - dataTimestamps[IDX_BARO_PRESSURE]),           // 25
                                       (dataTimestamps[IDX_AMBIENT_TEMP] == 0) ? 999999 : (now - dataTimestamps[IDX_AMBIENT_TEMP]),             // 26
                                       (dataTimestamps[IDX_IMU] == 0) ? 999999 : (now - dataTimestamps[IDX_IMU])                               // 27
    );
    if (timestampPayloadLen < 0 || timestampPayloadLen >= TIMESTAMP_PAYLOAD_SIZE) {
      Serial.printf("timestampPayload truncated or format error: %d\n", timestampPayloadLen);
      return;
    }

    events.send(timestampPayload, "TimestampData");
    lastTimestampSend = now;
    lastEventSourceSend = now;
    sentSomething = true;
  }
}
// CLOUD FEATURES STUFF BELOW
// AUTH TOKEN MANAGEMENT
String loadAuthToken() {
  nvs_handle_t handle;
  esp_err_t err = nvs_open("cloud", NVS_READONLY, &handle);

  authToken = "";

  if (err == ESP_OK) {
    size_t len = 0;
    err = nvs_get_str(handle, "authToken", NULL, &len);

    if (err == ESP_OK && len > 0) {
      char *buf = (char *)malloc(len);
      if (!buf) {
        nvs_close(handle);
        return authToken;
      }
      nvs_get_str(handle, "authToken", buf, &len);
      authToken = String(buf);
      free(buf);
    }

    nvs_close(handle);
  }

  isRegistered = (authToken.length() > 0);

  Serial.println("=== loadAuthToken ===");
  Serial.println("Token from NVS: " + authToken);

  if (isRegistered) {
    Serial.println("Auth token loaded: " + authToken);
  } else {
    Serial.println("No auth token found - device not registered");
  }

  return authToken;
}
void saveAuthToken(String token) {
  nvs_handle_t handle;
  esp_err_t err = nvs_open("cloud", NVS_READWRITE, &handle);
  if (err != ESP_OK) {
    Serial.printf("ERROR opening NVS for write: %d\n", err);
    return;
  }

  err = nvs_set_str(handle, "authToken", token.c_str());
  if (err != ESP_OK) {
    Serial.printf("ERROR writing token: %d\n", err);
  }
  nvs_commit(handle);
  nvs_close(handle);

  authToken = token;
  isRegistered = true;

  // REPLACE "Auth token saved: " + token WITH THIS:
  Serial.println("\n========================================");
  Serial.println("REGISTRATION SUCCESSFUL");
  Serial.println("========================================");
  Serial.printf("Auth Token: %s\n", token.c_str());
  Serial.println("========================================");
  Serial.println("Copy token above and add to Vercel URL:");
  Serial.printf("?token=%s\n", token.c_str());
  Serial.println("========================================\n");
}
void clearAuthToken() {
  nvs_handle_t handle;
  esp_err_t err = nvs_open("cloud", NVS_READWRITE, &handle);
  if (err != ESP_OK) {
    Serial.printf("ERROR opening NVS for clear: %d\n", err);
    return;
  }
  nvs_erase_key(handle, "authToken");
  nvs_commit(handle);
  nvs_close(handle);
  authToken = "";
  isRegistered = false;
  Serial.println("Auth token cleared");
}

void debugStackBeforeHTTPS(const char *functionName) {
  UBaseType_t loopStackBytes = uxTaskGetStackHighWaterMark(NULL);  // Already in bytes

  Serial.printf("=== BEFORE %s ===\n", functionName);
  Serial.printf("Loop stack free: %d bytes\n", loopStackBytes);
  Serial.printf("Free heap: %u KB\n", ESP.getFreeHeap() / 1024);
  Serial.printf("Largest heap block: %u bytes\n", heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));
#ifdef BOARD_HAS_PSRAM
  Serial.printf("Free PSRAM: %u KB\n", ESP.getFreePsram() / 1024);
#endif
  Serial.println("==================");
}


void updateVesselInfoField(const char *fieldName, int value) {
  if (!littleFSMounted && !ensureLittleFS()) return;
  if (!fsMutex || xSemaphoreTake(fsMutex, pdMS_TO_TICKS(5000)) != pdTRUE) {
    Serial.println("updateVesselInfoField: mutex timeout");
    return;
  }

  if (!LittleFS.exists("/vessel_info.json")) {
    xSemaphoreGive(fsMutex);
    return;
  }

  File file = LittleFS.open("/vessel_info.json", "r");
  if (!file) {
    xSemaphoreGive(fsMutex);
    return;
  }

  String jsonStr = file.readString();
  file.close();

  DynamicJsonDocument doc(1024);
  deserializeJson(doc, jsonStr);
  doc[fieldName] = value;

  file = LittleFS.open("/vessel_info.json", "w");
  if (file) {
    serializeJson(doc, file);
    file.close();
  }
  xSemaphoreGive(fsMutex);
}
