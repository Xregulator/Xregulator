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
  // LiveStream: fast-changing sensor readings sent at webgaugesinterval
  CSV1_AlternatorTemperatureF,
  CSV1_dutyCycle,
  CSV1_BatteryV,
  CSV1_MeasuredAmps,
  CSV1_RPM,
  CSV1_Channel3V,
  CSV1_IBV,
  CSV1_Bcur,
  CSV1_VictronVoltage,
  CSV1_LoopTime,
  CSV1_WifiHeartBeat,
  CSV1_vvout,
  CSV1_iiout,
  CSV1_FreeHeap,
  CSV1_Alarm_Status,
  CSV1_fieldActiveStatus,
  CSV1_currentMode,
  CSV1_stateRevision,
  CSV1_setpointLimited,
  CSV1_uTargetAmps,
  CSV1_pidInput,
  CSV1_pidOutput,
  CSV1_pidError,
  CSV1_imu_heel_deg,
  CSV1_imu_pitch_deg,
  CSV1_imu_vertical_accel_g,
  CSV1_imu_yaw_rate_dps,
  CSV1_imu_total_accel_g,
  CSV1_perfCountersResetElapsedS,  // seconds since "Reset Peak Values" press (or boot if never pressed); UI formats as "last X min" / "last X.X hr"
  CSV1_shutdownPhase,
  CSV1_BatteryV_raw,
  CSV1_MeasuredAmps_filtered,
  CSV1_voltageTarget,
  CSV1_Icv,

  CSV1_FIELD_COUNT  // = 34
};

enum Csv2Index {
  // DiagStream: slower-changing telemetry, diagnostics, computed values — sent every 5s
  // Fields 0-229: from original CSV2 (settings removed, duplicates removed)
  CSV2_IBVMax,
  CSV2_MeasuredAmpsMax,
  CSV2_RPMMax,
  CSV2_SOC_percent,
  CSV2_EngineRunTime,
  CSV2_AlternatorOnTime,
  CSV2_AlternatorFuelUsed,
  CSV2_ChargedEnergy,
  CSV2_DischargedEnergy,
  CSV2_AlternatorChargedEnergy,
  CSV2_MaxAlternatorTemperatureF,
  CSV2_temperatureThermistor,
  CSV2_MaxTemperatureThermistor,
  CSV2_VictronCurrent,
  CSV2_timeToFullChargeMin,
  CSV2_timeToFullDischargeMin,
  CSV2_LatitudeNMEA,
  CSV2_LongitudeNMEA,
  CSV2_SatelliteCountNMEA,
  CSV2_LastSessionDuration,
  CSV2_LastSessionMaxLoopTime,
  CSV2_lastSessionMinHeap,
  CSV2_wifiReconnectsTotal,
  CSV2_LastResetReason,
  CSV2_ancientResetReason,
  CSV2_totalPowerCycles,
  CSV2_MinFreeHeap,
  CSV2_currentWeatherMode,
  CSV2_UVToday,
  CSV2_UVTomorrow,
  CSV2_UVDay2,
  CSV2_weatherDataValid,
  CSV2_SolarWatts,
  CSV2_performanceRatio,
  CSV2_VeData,
  CSV2_NMEA0183Data,
  CSV2_NMEA2KData,
  CSV2_AlarmLatchState,
  CSV2_ResetAlarmLatch,
  CSV2_ResetLearningTable,
  CSV2_ClearOverheatHistory,
  CSV2_DynamicShuntGainFactor,
  CSV2_DynamicAltCurrentZero,
  CSV2_InsulationLifePercent,
  CSV2_GreaseLifePercent,
  CSV2_BrushLifePercent,
  CSV2_PredictedLifeHours,
  CSV2_LifeIndicatorColor,
  CSV2_pKwHrToday,
  CSV2_pKwHrTomorrow,
  CSV2_pKwHr2days,
  CSV2_ambientTemp,
  CSV2_baroPressure,
  CSV2_firmwareVersionInt,
  CSV2_deviceIdUpper,
  CSV2_deviceIdLower,
  CSV2_ChargedEnergy_AllTime,
  CSV2_AlternatorFuelUsed_AllTime,
  CSV2_PeakVoltage_AllTime,
  CSV2_EngineRunTime_AllTime,
  CSV2_MinVoltage,
  CSV2_MinVoltage_AllTime,
  CSV2_ChargeCycles,
  CSV2_ChargeCycles_AllTime,
  CSV2_EngineFuelUsed,
  CSV2_EngineFuelUsed_AllTime,
  CSV2_TotalDistance,
  CSV2_TotalDistance_AllTime,
  CSV2_MaxSpeed,
  CSV2_MaxSpeed_AllTime,
  CSV2_SolarChargedEnergy,
  CSV2_SolarChargedEnergy_AllTime,
  CSV2_AlternatorChargedEnergy_AllTime,
  CSV2_DischargedEnergy_AllTime,
  CSV2_AvgSOC_AllTime,
  CSV2_AvgSpeed_AllTime,
  CSV2_AvgSpeed,
  CSV2_AlternatorOnTime_AllTime,
  CSV2_EngineCycles_AllTime,
  CSV2_MaxAlternatorTemperatureF_AllTime,
  CSV2_MaxTemperatureThermistor_AllTime,
  CSV2_MeasuredAmpsMax_AllTime,
  CSV2_RPMMax_AllTime,
  CSV2_Ignition,
  CSV2_BulkStage,
  CSV2_WifiWakeSecondsRemaining,
  CSV2_BufferedRecordCount,
  CSV2_BufferedRecordPercent,
  CSV2_MAX_BUFFERED_RECORDS,
  CSV2_COGNMEA,
  CSV2_SOGNMEA,
  CSV2_ApparentWindSpeedNMEA,
  CSV2_ApparentWindAngleNMEA,
  CSV2_TrueWindSpeedNMEA,
  CSV2_TrueWindAngleNMEA,
  CSV2_LeewayNMEA,
  CSV2_VMGNMEA,
  CSV2_VMGTargetBearing,
  CSV2_VMGUseTrueWind,
  CSV2_cpuLoadCore0,
  CSV2_cpuLoadCore0Max,
  CSV2_cpuLoadCore1,
  CSV2_cpuLoadCore1Max,
  CSV2_hasForcedUpdate,
  CSV2_forcedFwVersionInt,
  CSV2_forcedUpdateDeadline,
  CSV2_stateRevision,
  CSV2_hardwarePresent,
  CSV2_imu_accel_x_raw,
  CSV2_imu_accel_y_raw,
  CSV2_imu_accel_z_raw,
  CSV2_imu_gyro_x_raw,
  CSV2_imu_gyro_y_raw,
  CSV2_imu_gyro_z_raw,
  CSV2_accel_x_min,
  CSV2_accel_x_max,
  CSV2_accel_x_avg,
  CSV2_accel_y_min,
  CSV2_accel_y_max,
  CSV2_accel_y_avg,
  CSV2_accel_z_min,
  CSV2_accel_z_max,
  CSV2_accel_z_avg,
  CSV2_gyro_x_min,
  CSV2_gyro_x_max,
  CSV2_gyro_x_avg,
  CSV2_gyro_y_min,
  CSV2_gyro_y_max,
  CSV2_gyro_y_avg,
  CSV2_gyro_z_min,
  CSV2_gyro_z_max,
  CSV2_gyro_z_avg,
  CSV2_heel_min,
  CSV2_heel_max,
  CSV2_heel_avg,
  CSV2_pitch_min,
  CSV2_pitch_max,
  CSV2_pitch_avg,
  CSV2_vertical_accel_min,
  CSV2_vertical_accel_max,
  CSV2_vertical_accel_avg,
  CSV2_total_accel_min,
  CSV2_total_accel_max,
  CSV2_total_accel_avg,
  CSV2_imu_slam_count,
  CSV2_imu_slam_peak_max,
  CSV2_imu_slam_count_lifetime,
  CSV2_imu_capsize_count,
  CSV2_imu_pitchpole_count,
  CSV2_imu_heel_change_60s,
  CSV2_imu_heel_deviation_60s,
  CSV2_imu_pitch_change_60s,
  CSV2_imu_pitch_deviation_60s,
  CSV2_imu_wave_period_sec,
  CSV2_imu_heel_max_lifetime,
  CSV2_imu_pitch_max_lifetime,
  CSV2_imu_slam_peak_lifetime,
  CSV2_imu_fifo_overrun_count,
  CSV2_imu_i2c_error_count,
  CSV2_imu_unknown_tag_count,
  CSV2_imu_accel_dropped,
  CSV2_imu_gyro_dropped,
  CSV2_imu_total_samples_accel,
  CSV2_imu_total_samples_gyro,
  CSV2_IMUReadTime2,
  CSV2_IMUReadTime,
  CSV2_adsI2CErrorCount,
  CSV2_tempPIDActive,
  CSV2_tempPIDInput_d,
  CSV2_tempPIDSetpoint_d,
  CSV2_thermalPenaltyAmps,
  CSV2_innerTermP,
  CSV2_innerTermI,
  CSV2_innerTermD,
  CSV2_outerTermP,
  CSV2_outerTermI,
  CSV2_outerTermD,
  CSV2_thermalSlopeFPerSec,
  CSV2_chargeStageDisplay,
  CSV2_voltageControlActive,
  CSV2_voltageError,
  CSV2_cv_I,
  CSV2_inIdleStage,
  CSV2_referenceFinalized,
  CSV2_ft_rai_total_win,
  CSV2_ft_rai_total_ses,
  CSV2_ft_rai_ina228_win,
  CSV2_ft_rai_ina228_ses,
  CSV2_ft_rai_ads_state_win,
  CSV2_ft_rai_ads_state_ses,
  CSV2_ft_rai_bmp_state_win,
  CSV2_ft_rai_bmp_state_ses,
  CSV2_ft_rai_imu_win,
  CSV2_ft_rai_imu_ses,
  CSV2_reserved_cv_D,  // 194 reserved — was cv_D (D term removed)
  CSV2_tempReadFailCount,
  CSV2_tempCrcFailCount,
  CSV2_tempCrcRecoveredCount,
  CSV2_tempAllFFCount,
  CSV2_tempPowerOn85Count,
  CSV2_tempOutOfRangeCount,
  CSV2_tempRequestFailCount,
  CSV2_tempConnectedFailCount,
  CSV2_tempResolutionFixCount,
  CSV2_tempRereadFailCount,
  CSV2_tempResolutionFixCrcFailCount,
  CSV2_tempEnumerateFailCount,
  CSV2_warmupCeiling,
  CSV2_imu_min_moving_gentle,
  CSV2_imu_min_moving_moderate,
  CSV2_imu_min_moving_rough,
  CSV2_imu_min_moving_extreme,
  CSV2_imu_min_stat_gentle,
  CSV2_imu_min_stat_moderate,
  CSV2_imu_min_stat_rough,
  CSV2_imu_min_stat_extreme,
  CSV2_imu_heel_deviation_120s,
  CSV2_imu_pitch_deviation_120s,
  CSV2_imu_heading_swing_120s,
  CSV2_dBcur_dt,
  CSV2_loadDumpActive,
  CSV2_thermalLiveScore0,
  CSV2_thermalLiveScore1,
  CSV2_thermalLiveScore2,
  CSV2_thermalLiveScore3,
  CSV2_thermalTuningTestPhase,
  CSV2_ft_updateAccelMetrics_win,
  CSV2_ft_updateAccelMetrics_ses,
  // Fields 230-322: diagnostics moved from CSV1
  CSV2_WifiStrength,
  CSV2_SendWifiTime,
  CSV2_AnalogReadTime,
  CSV2_VeTime,
  CSV2_MaximumLoopTime,
  CSV2_HeadingNMEA,
  CSV2_EngineCycles,
  CSV2_CurrentSessionDuration,
  CSV2_timeAxisModeChanging,
  CSV2_currentPartitionType,
  CSV2_fastOvCurrentCap,
  CSV2_fastOvClampCount,
  CSV2_fastOvHardCount,
  CSV2_ch1_last_ms,
  CSV2_ch1_avg_10s,
  CSV2_ch1_worst_10s,
  CSV2_ch1_over2x_10s,
  CSV2_ch1_n_10s,
  CSV2_ch1_avg_2m,
  CSV2_ch1_worst_2m,
  CSV2_ch1_over2x_2m,
  CSV2_ch1_n_2m,
  CSV2_ch1_avg_at,
  CSV2_ch1_worst_at,
  CSV2_ch1_over2x_at,
  CSV2_ch1_n_at,
  CSV2_iExcessCount,
  CSV2_inaOVCount,
  CSV2_hardOCCount,
  CSV2_voltSpikeCount,
  CSV2_voltDisagreeCritCount,
  CSV2_voltDisagreeWarnCount,
  CSV2_voltImplausibleCount,
  CSV2_tempCritCount,
  CSV2_tempSustainedCount,
  CSV2_tempStaleCount,
  CSV2_currentStaleCount,
  CSV2_imu_msi_score,
  CSV2_imu_vomit_pct,
  CSV2_imu_anchorage_comfort,
  CSV2_ina_last_ms,
  CSV2_ina_avg_10s,
  CSV2_ina_worst_10s,
  CSV2_ina_over2x_10s,
  CSV2_ina_avg_2m,
  CSV2_ina_worst_2m,
  CSV2_ina_over2x_2m,
  CSV2_ina_avg_at,
  CSV2_ina_worst_at,
  CSV2_ina_over2x_at,
  CSV2_loopTime5sWindow_ms,
  CSV2_MaximumLoopTime_ms,
  CSV2_ft_SendWifiData_win,
  CSV2_ft_SendWifiData_ses,
  CSV2_ft_CheckAlarms_win,
  CSV2_ft_CheckAlarms_ses,
  CSV2_ft_calculateDerivedMetrics_win,
  CSV2_ft_calculateDerivedMetrics_ses,
  CSV2_ft_logDashboardValues_win,
  CSV2_ft_logDashboardValues_ses,
  CSV2_ft_updateSystemHealthStats_win,
  CSV2_ft_updateSystemHealthStats_ses,
  CSV2_ft_checkWiFiConnection_win,
  CSV2_ft_checkWiFiConnection_ses,
  CSV2_ft_ch1_compute_stats_win,
  CSV2_ft_ch1_compute_stats_ses,
  CSV2_ft_UpdateEngineRuntime_win,
  CSV2_ft_UpdateEngineRuntime_ses,
  CSV2_ft_UpdateEngineFuel_win,
  CSV2_ft_UpdateEngineFuel_ses,
  CSV2_ft_UpdateBatterySOC_win,
  CSV2_ft_UpdateBatterySOC_ses,
  CSV2_ft_UpdateTravelStatistics_win,
  CSV2_ft_UpdateTravelStatistics_ses,
  CSV2_ft_UpdateDistanceThisInterval_win,
  CSV2_ft_UpdateDistanceThisInterval_ses,
  CSV2_ft_UpdateBoardTempPressureMaximums_win,
  CSV2_ft_UpdateBoardTempPressureMaximums_ses,
  CSV2_ft_handleSocGainReset_win,
  CSV2_ft_handleSocGainReset_ses,
  CSV2_ft_handleAltZeroReset_win,
  CSV2_ft_handleAltZeroReset_ses,
  CSV2_ft_calculateChargeTimes_win,
  CSV2_ft_calculateChargeTimes_ses,
  CSV2_ft_UpdateSailingMetrics_win,
  CSV2_ft_UpdateSailingMetrics_ses,
  CSV2_ft_updateWeatherMode_win,
  CSV2_ft_updateWeatherMode_ses,
  CSV2_ft_updateSensorWindow_win,
  CSV2_ft_updateSensorWindow_ses,
  CSV2_ft_checkTimeSync_win,
  CSV2_ft_checkTimeSync_ses,
  // Fields 323-401: firmware-computed values moved from CSV3
  CSV2_currentRPMTableIndex,
  CSV2_pidInitialized,
  CSV2_pidSetpoint,
  CSV2_TempToUse,
  CSV2_learningTargetFromRPM,
  CSV2_ambientTempCorrection,
  CSV2_finalLearningTarget,
  CSV2_overheatingPenaltyTimer,
  CSV2_overheatingPenaltyAmps,
  CSV2_averageTableValue,
  CSV2_timeSinceLastOverheat,
  CSV2_socInfoAvailable,
  CSV2_overheatCount0,
  CSV2_overheatCount1,
  CSV2_overheatCount2,
  CSV2_overheatCount3,
  CSV2_overheatCount4,
  CSV2_overheatCount5,
  CSV2_overheatCount6,
  CSV2_overheatCount7,
  CSV2_overheatCount8,
  CSV2_overheatCount9,
  CSV2_cumulativeNoOverheatTime0,
  CSV2_cumulativeNoOverheatTime1,
  CSV2_cumulativeNoOverheatTime2,
  CSV2_cumulativeNoOverheatTime3,
  CSV2_cumulativeNoOverheatTime4,
  CSV2_cumulativeNoOverheatTime5,
  CSV2_cumulativeNoOverheatTime6,
  CSV2_cumulativeNoOverheatTime7,
  CSV2_cumulativeNoOverheatTime8,
  CSV2_cumulativeNoOverheatTime9,
  CSV2_learningUpCount0,
  CSV2_learningUpCount1,
  CSV2_learningUpCount2,
  CSV2_learningUpCount3,
  CSV2_learningUpCount4,
  CSV2_learningUpCount5,
  CSV2_learningUpCount6,
  CSV2_learningUpCount7,
  CSV2_learningUpCount8,
  CSV2_learningUpCount9,
  CSV2_totalLearningEvents,
  CSV2_totalOverheats,
  CSV2_totalSafeHours,
  CSV2_FreeInternalRam,
  CSV2_TotalInternalRam,
  CSV2_LargestInternalBlock,
  CSV2_FreePSRAM,
  CSV2_TotalPSRAM,
  CSV2_Heapfrag,
  CSV2_ft_ReadAnalogInputs_win,
  CSV2_ft_ReadAnalogInputs_ses,
  CSV2_ft_AdjustFieldLearnMode_win,
  CSV2_ft_AdjustFieldLearnMode_ses,
  CSV2_ft_uploadSensorHistory_win,
  CSV2_ft_uploadSensorHistory_ses,
  CSV2_ft_uploadBufferedRecords_win,
  CSV2_ft_uploadBufferedRecords_ses,
  CSV2_ft_buildConfigPayload_win,
  CSV2_ft_buildConfigPayload_ses,
  CSV2_VeTime2,
  CSV2_systemIDRiseDelay_0,
  CSV2_systemIDRiseDelay_1,
  CSV2_systemIDRiseDelay_2,
  CSV2_systemIDFallDelay_0,
  CSV2_systemIDFallDelay_1,
  CSV2_systemIDFallDelay_2,
  CSV2_systemIDRiseAvg,
  CSV2_systemIDFallAvg,
  CSV2_nvsPhase,
  CSV2_ft_saveNVSData_win,
  CSV2_ft_saveNVSData_ses,
  CSV2_ft_efficiencyTracker_win,
  CSV2_ft_efficiencyTracker_ses,
  CSV2_systemIDActive,
  CSV2_systemIDResultsReady,
  CSV2_systemIDStepAmp_0,         // 397
  CSV2_systemIDStepAmp_1,         // 398
  CSV2_systemIDStepAmp_2,         // 399
  CSV2_systemIDQuietPP_0,         // 400
  CSV2_systemIDQuietPP_1,         // 401
  CSV2_systemIDQuietPP_2,         // 402
  CSV2_nvsCycleMs,                // 403 — ms elapsed for last complete NVS drain cycle
  CSV2_voltLoopWorstInterval_5s,  // 404 — worst voltage loop actual interval in 5s window (ms)
  CSV2_voltLoopWorstInterval_ses, // 405 — worst voltage loop actual interval since boot (ms)
  // NVS commit timing — separates commit cost from the 9-phase cycle cost.
  CSV2_nvsCommitCount,            // 406 — total nvs_commit() calls since boot
  CSV2_nvsCommitLongCount,        // 407 — commits that took >= 100 ms
  CSV2_nvsCommitWorstMs,          // 408 — worst single commit duration since boot (ms)
  CSV2_nvsCommitLastMs,           // 409 — most recent commit duration (ms)

  CSV2_FIELD_COUNT  // = 410
};

enum Csv3Index {
  // SettingsStream: user-configurable settings — sent on change (settingsDirty) or every 60s fallback
  // Fields 0-208: settings kept from original CSV3
  CSV3_TemperatureLimitF,
  CSV3_BulkVoltage,
  CSV3_wavePeriod,
  CSV3_FloatVoltage,
  CSV3_SwitchingFrequency,
  CSV3_yyMin,
  CSV3_FieldAdjustmentInterval,
  CSV3_ManualDutyTarget,
  CSV3_SwitchControlOverride,
  CSV3_waveAmplitude,
  CSV3_CurrentThreshold,
  CSV3_PeukertExponent_scaled,
  CSV3_ChargeEfficiency_scaled,
  CSV3_ChargedVoltage_Scaled,
  CSV3_TailCurrent,
  CSV3_ChargedDetectionTime,
  CSV3_IgnoreTemperature,
  CSV3_bmsLogic,
  CSV3_bmsLogicLevelOff,
  CSV3_RPMScalingFactor,
  CSV3_MaximumAllowedBatteryAmps,
  CSV3_BatteryVoltageSource,
  CSV3_LearningUpwardEnabled,
  CSV3_LearningDownwardEnabled,
  CSV3_AlternatorNominalAmps,
  CSV3_LearningUpStep,
  CSV3_LearningDownStep,
  CSV3_AmbientTempCorrectionFactor,
  CSV3_xTime,
  CSV3_MinLearningInterval,
  CSV3_SafeOperationThreshold,
  CSV3_PidKp,
  CSV3_PidKi,
  CSV3_PidKd,
  CSV3_PidSampleDivisor,
  CSV3_MaxTableValue,
  CSV3_MaxPenaltyPercent,
  CSV3_MaxPenaltyDuration,
  CSV3_NeighborLearningFactor,
  CSV3_yyMax,
  CSV3_LearningMemoryDuration,
  CSV3_EnableNeighborLearning,
  CSV3_EnableAmbientCorrection,
  CSV3_TuningMode,
  CSV3_rpmCurrentTable_0,
  CSV3_rpmCurrentTable_1,
  CSV3_rpmCurrentTable_2,
  CSV3_rpmCurrentTable_3,
  CSV3_rpmCurrentTable_4,
  CSV3_rpmCurrentTable_5,
  CSV3_rpmCurrentTable_6,
  CSV3_rpmCurrentTable_7,
  CSV3_rpmCurrentTable_8,
  CSV3_rpmCurrentTable_9,
  CSV3_ShuntResistanceMicroOhm,
  CSV3_InvertAltAmps,
  CSV3_InvertBattAmps,
  CSV3_MaxDuty,
  CSV3_MinDuty,
  CSV3_FieldResistance,
  CSV3_maxPoints,
  CSV3_AlternatorCOffset,
  CSV3_BatteryCOffset,
  CSV3_BatteryCapacity_Ah,
  CSV3_AmpSensorRange,
  CSV3_R_fixed,
  CSV3_Beta,
  CSV3_T0_C,
  CSV3_TempSource,
  CSV3_IgnitionOverride,
  CSV3_FLOAT_DURATION,
  CSV3_PulleyRatio,
  CSV3_BatteryCurrentSource,
  CSV3_rpmTableRPMPoints_0,
  CSV3_rpmTableRPMPoints_1,
  CSV3_rpmTableRPMPoints_2,
  CSV3_rpmTableRPMPoints_3,
  CSV3_rpmTableRPMPoints_4,
  CSV3_rpmTableRPMPoints_5,
  CSV3_rpmTableRPMPoints_6,
  CSV3_rpmTableRPMPoints_7,
  CSV3_rpmTableRPMPoints_8,
  CSV3_rpmTableRPMPoints_9,
  CSV3_LearningSettlingPeriod,
  CSV3_LearningRPMChangeThreshold,
  CSV3_LearningTempHysteresis,
  CSV3_fuelTableRPM_0,
  CSV3_fuelTableRPM_1,
  CSV3_fuelTableRPM_2,
  CSV3_fuelTableRPM_3,
  CSV3_fuelTableRPM_4,
  CSV3_fuelTableRPM_5,
  CSV3_fuelTableRPM_6,
  CSV3_fuelTableRPM_7,
  CSV3_fuelTableRPM_8,
  CSV3_fuelTableRPM_9,
  CSV3_fuelTableGPH_0,
  CSV3_fuelTableGPH_1,
  CSV3_fuelTableGPH_2,
  CSV3_fuelTableGPH_3,
  CSV3_fuelTableGPH_4,
  CSV3_fuelTableGPH_5,
  CSV3_fuelTableGPH_6,
  CSV3_fuelTableGPH_7,
  CSV3_fuelTableGPH_8,
  CSV3_fuelTableGPH_9,
  CSV3_stateRevision,
  CSV3_SetpointRampRate,
  CSV3_DutyRampRate,
  CSV3_SettleTimeBeforeCut,
  CSV3_TempWarnExcess,
  CSV3_TempCritExcess,
  CSV3_TempSustainedTimeout,
  CSV3_AlternatorHardShutdownV,
  CSV3_VoltageDisagreeThreshold,
  CSV3_VoltageDisagreeTimeout,
  CSV3_rpmMinDutyTable_0,
  CSV3_rpmMinDutyTable_1,
  CSV3_rpmMinDutyTable_2,
  CSV3_rpmMinDutyTable_3,
  CSV3_rpmMinDutyTable_4,
  CSV3_rpmMinDutyTable_5,
  CSV3_rpmMinDutyTable_6,
  CSV3_rpmMinDutyTable_7,
  CSV3_rpmMinDutyTable_8,
  CSV3_rpmMinDutyTable_9,
  CSV3_rpmCapCurrentTable_0,
  CSV3_rpmCapCurrentTable_1,
  CSV3_rpmCapCurrentTable_2,
  CSV3_rpmCapCurrentTable_3,
  CSV3_rpmCapCurrentTable_4,
  CSV3_rpmCapCurrentTable_5,
  CSV3_rpmCapCurrentTable_6,
  CSV3_rpmCapCurrentTable_7,
  CSV3_rpmCapCurrentTable_8,
  CSV3_rpmCapCurrentTable_9,
  CSV3_VoltageKp,
  CSV3_VoltageLoopInterval,
  CSV3_FIELD_COLLAPSE_DELAY,
  CSV3_SetpointRiseRate,
  CSV3_SetpointFallRate,
  CSV3_PIDTrackingGain,
  CSV3_CAPSIZE_THRESHOLD_DEG,
  CSV3_PITCHPOLE_THRESHOLD_DEG,
  CSV3_SLAM_THRESHOLD_G,
  CSV3_imuMountOrientation,
  CSV3_TailCurrent_A,
  CSV3_RebulkVoltage,
  CSV3_rebulkDebounceTime,
  CSV3_MinFloatTime,
  CSV3_SOC_BlockRebulk_percent,
  CSV3_SOC_AllowRebulk_percent,
  CSV3_accelEnabled,
  CSV3_DutySlowRampRate,
  CSV3_ShutdownPhase2HoldMs,
  CSV3_TempPIDKp,
  CSV3_TempPIDKi,
  CSV3_ThermalLookaheadSec,          // (was TempPIDMarginF — same conceptual slot)
  CSV3_TempPIDIntervalMs,
  CSV3_TempPIDFilterAlpha,
  CSV3_VoltageKi,
  CSV3_rpmCapPowerTable_0,
  CSV3_rpmCapPowerTable_1,
  CSV3_rpmCapPowerTable_2,
  CSV3_rpmCapPowerTable_3,
  CSV3_rpmCapPowerTable_4,
  CSV3_rpmCapPowerTable_5,
  CSV3_rpmCapPowerTable_6,
  CSV3_rpmCapPowerTable_7,
  CSV3_rpmCapPowerTable_8,
  CSV3_rpmCapPowerTable_9,
  CSV3_VoltageTrimLimit,
  CSV3_InputFilterTC,
  CSV3_SystemIDStepAmplitude,
  CSV3_HardOCTripAmps,
  CSV3_HardOCDebounceMs,
  CSV3_IExcessK,
  CSV3_IExcessN,
  CSV3_IExcessKBleed,
  CSV3_IgnoreRPM,
  CSV3_MinRPMForField,
  CSV3_AwBleedRate,
  CSV3_AwRecoverRate,
  CSV3_KHard,
  CSV3_IExcessReseedFrac,
  CSV3_AwSeedProtectMs,
  CSV3_reserved_VoltageKd,  // 187 reserved — was VoltageKd; D term removed
  CSV3_displayTempUnit,
  CSV3_WarmupRampRate,
  CSV3_OvGroup1Enable,
  CSV3_OvGroup2Enable,
  CSV3_IExcessSigSrc,
  CSV3_IExcessMA_N,
  CSV3_OutputPIDSigSrc,
  CSV3_TdPred,          // %.3f
  CSV3_OvMeasMarginV,   // %.3f
  CSV3_OvPredMarginV,   // %.3f
  CSV3_OutputPIDMA_N,
  CSV3_OutputPIDFilterTC,
  CSV3_VoltageFilterTC,
  CSV3_ProtectionProxGateV,
  CSV3_SlopeBleedThresh,
  CSV3_SlopeBleedK,
  CSV3_DvdtAlpha,
  CSV3_SlopeBleedProxV,
  CSV3_StartupRiseRate,
  // Fields 209-265: settings moved from CSV2
  CSV3_absorptionCompleteTime,
  CSV3_OnOff,
  CSV3_ManualFieldToggle,
  CSV3_HiLow,
  CSV3_LimpHome,
  CSV3_AlarmActivate,
  CSV3_TempAlarm,
  CSV3_VoltageAlarmHigh,
  CSV3_VoltageAlarmLow,
  CSV3_CurrentAlarmHigh,
  CSV3_AlarmTest,
  CSV3_AlarmLatchEnabled,
  CSV3_MaintainMode,
  CSV3_ManualSOCPoint,
  CSV3_LearningMode,
  CSV3_LearningPaused,
  CSV3_IgnoreLearningDuringPenalty,
  CSV3_ShowLearningDebugMessages,
  CSV3_LogAllLearningEvents,
  CSV3_CloudFeatures,
  CSV3_LearningDryRunMode,
  CSV3_AutoShuntGainCorrection,
  CSV3_AutoAltCurrentZero,
  CSV3_WindingTempOffset,
  CSV3_ManualLifePercentage,
  CSV3_UVThresholdHigh,
  CSV3_weatherModeEnabled,
  CSV3_SENSOR_UPLOAD_INTERVAL,
  CSV3_imuEnabled,
  CSV3_AbsorptionVoltage,
  CSV3_AbsorptionTimeoutMs,
  CSV3_bulkVoltageHoldMs,
  CSV3_capLimitMode,
  CSV3_TargetVoltageMode,
  CSV3_TargetVoltageSetpoint,
  CSV3_RebulkCurrent_A,
  CSV3_UseFloat,
  CSV3_anomalyMarginAmps,
  CSV3_anomalyAlarmThreshold,
  CSV3_anomalyAlarmEnable,
  CSV3_degradationThreshold,
  CSV3_TempAlarmLow,
  CSV3_LoadDumpDtThresh,
  CSV3_LoadDumpDtThresh1,
  CSV3_CVTuningMode,
  CSV3_cvWaveAmplitudeV,
  CSV3_cvWavePeriodSec,
  CSV3_cvKOvershoot,
  CSV3_cvConsecutiveReads,
  CSV3_ThermalTuningMode,
  CSV3_thermalWaveLowF,
  CSV3_thermalWaveHighF,
  CSV3_thermalWaveHalfPeriodMin,
  CSV3_thermalKOvershoot,
  CSV3_thermalKUndershoot,
  CSV3_thermalSettleThreshF,
  CSV3_thermalConsecutiveReads,
  // Fields 266-275: settings moved from CSV1
  CSV3_webgaugesinterval,
  CSV3_plotTimeWindow,
  CSV3_Ymin1,
  CSV3_Ymax1,
  CSV3_Ymin2,
  CSV3_Ymax2,
  CSV3_Ymin3,
  CSV3_Ymax3,
  CSV3_Ymin4,
  CSV3_Ymax4,
  CSV3_LoadDumpDtThresh3,

  CSV3_FIELD_COUNT  // = 274
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
                "# ovFlags: bit0=fastOvActive bit1=reserved(wasSoftClamp) bit2=hardClamp bit3=iExcess bit4=loadDumpActive\n"
                "# voltageLoopRanThisTick=1 means Icv/cv_I updated this row\n"
                "# vError: always fresh every tick regardless of loop interval\n"
                "# Icv: CV position-form PI output — the direct current setpoint in CV modes\n"
                "# cv_I: CV position-form PI integrator state\n"
                "# tableThermalLimit: RPM cap minus thermal penalty, before CV\n"
                "# setpointCmd: Icv in CV modes, tableThermalLimit in bulk\n"
                "# dBcur_dt: battery current derivative (A/s) — positive = load dump event\n"
                "# battI: battery current from INA228 or Victron (A)\n");
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
                "innerKp,"
                "innerKi,"
                "innerKd,"
                "voltageKp,"
                "voltageKi,"
                "voltageKd,"
                "battV_filt_V,"
                "iMeas_filt_A,"
                "flags,"
                "ovFlags,"
                "dBcur_dt,"
                "battI,"
                "ch1IntervalMs,"
                "voltLoopIntervalMs,"
                "inaIntervalMs\n");

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
                "%.4f,%.4f,%.4f,"   // innerKp, innerKi, innerKd
                "%.4f,%.4f,%.4f,"   // voltageKp, voltageKi, voltageKd
                "%.3f,%.3f,"  // battV_filt, iMeas_filt
                "%u,%u,"          // flags, ovFlags
                "%.2f,%.3f,"      // dBcur_dt, battI
                "%d,%d,%d\n",     // ch1IntervalMs, voltLoopIntervalMs, inaIntervalMs
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
                e.innerKp,
                e.innerKi,
                e.innerKd,
                e.voltageKp,
                e.voltageKi,
                e.voltageKd,
                e.battV_filt,
                e.iMeas_filt,
                (unsigned)e.flags,
                (unsigned)e.ovFlags,
                e.dBcur_dt,
                e.battI,
                (int)e.ch1IntervalMs,
                (int)e.voltLoopIntervalMs,
                (int)e.inaIntervalMs);
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

  // ── Efficiency matrix export ──────────────────────────────────────────────
  server.on("/effmatrix.csv", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (!effMatrix) {
      request->send(200, "text/plain", "Efficiency matrix not initialized.");
      return;
    }

    struct EffMatExportState {
      int r, t, f;
      bool header;
      bool done;
      char line[256];
      int lineLen;
      int linePos;
    };

    EffMatExportState state;
    state.r = 0; state.t = 0; state.f = 0;
    state.header = true;
    state.done = false;
    state.lineLen = 0; state.linePos = 0;

    AsyncWebServerResponse *response = request->beginChunkedResponse(
      "text/csv",
      [state](uint8_t *buf, size_t maxLen, size_t) mutable -> size_t {
        if (state.done) return 0;
        size_t written = 0;
        while (written < maxLen) {
          if (state.linePos >= state.lineLen) {
            if (state.header) {
              state.lineLen = snprintf(state.line, sizeof(state.line),
                "rpm_bucket,rpm_label,temp_bucket,temp_label,field_bucket,field_label,"
                "ss_seconds,avg_amps,min_amps,max_amps,"
                "ref_avg_amps,ref_min_amps,ref_max_amps,is_reference_bin\n");
              state.header = false;
            } else {
              if (state.r >= NUM_RPM_BUCKETS) {
                state.done = true;
                return written;
              }
              MatrixCell &cell = MATRIX_CELL(state.r, state.t, state.f);
              state.lineLen = snprintf(state.line, sizeof(state.line),
                "%d,%s,%d,%s,%d,%s,%lu,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%d\n",
                state.r, RPM_LABELS[state.r],
                state.t, TEMP_LABELS[state.t],
                state.f, FIELD_LABELS[state.f],
                (unsigned long)cell.ss_seconds,
                cell.avg_amps, cell.min_amps, cell.max_amps,
                cell.ref_avg_amps, cell.ref_min_amps, cell.ref_max_amps,
                (int)cell.is_reference_bin);
              // Advance indices: field → temp → rpm
              state.f++;
              if (state.f >= NUM_FIELD_BUCKETS) { state.f = 0; state.t++; }
              if (state.t >= NUM_TEMP_BUCKETS)  { state.t = 0; state.r++; }
            }
            state.linePos = 0;
          }
          size_t toWrite = min((size_t)(state.lineLen - state.linePos), maxLen - written);
          memcpy(buf + written, state.line + state.linePos, toWrite);
          written += toWrite;
          state.linePos += (int)toWrite;
        }
        return written;
      });

    char effTs[20] = "export";
    if (timeIsSynced) {
      time_t effNow = time(nullptr);
      struct tm effTm;
      localtime_r(&effNow, &effTm);
      strftime(effTs, sizeof(effTs), "%Y%m%d_%H%M%S", &effTm);
    }
    char effDisp[80];
    snprintf(effDisp, sizeof(effDisp), "attachment; filename=\"AltHealthMatrix_%s.csv\"", effTs);
    response->addHeader("Content-Disposition", effDisp);
    response->addHeader("Cache-Control", "no-cache");
    request->send(response);
  });

  // ── Efficiency matrix bucket summary (JSON) ───────────────────────────────
  // Aggregates SS time per RPM / temp / field bucket. Small payload (~600 B).
  // Used by the Live Data → Alternator card summary bars.
  server.on("/effmatrixstats", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (!effMatrix) {
      request->send(200, "application/json", "{\"error\":\"not_ready\"}");
      return;
    }

    uint32_t rpm_ss[NUM_RPM_BUCKETS]    = {0};
    uint32_t temp_ss[NUM_TEMP_BUCKETS]  = {0};
    uint32_t field_ss[NUM_FIELD_BUCKETS]= {0};
    float field_min_amps[NUM_FIELD_BUCKETS];
    float field_max_amps[NUM_FIELD_BUCKETS];
    for (int f = 0; f < NUM_FIELD_BUCKETS; f++) { field_min_amps[f] = 9999.0f; field_max_amps[f] = -1.0f; }
    uint32_t total_ss = 0;
    int pop_cells = 0, ref_bins = 0;

    for (int r = 0; r < NUM_RPM_BUCKETS; r++)
      for (int t = 0; t < NUM_TEMP_BUCKETS; t++)
        for (int f = 0; f < NUM_FIELD_BUCKETS; f++) {
          MatrixCell &c = MATRIX_CELL(r, t, f);
          rpm_ss[r]   += c.ss_seconds;
          temp_ss[t]  += c.ss_seconds;
          field_ss[f] += c.ss_seconds;
          total_ss    += c.ss_seconds;
          if (c.ss_seconds > 0) {
            pop_cells++;
            if (c.min_amps < field_min_amps[f]) field_min_amps[f] = c.min_amps;
            if (c.max_amps > field_max_amps[f]) field_max_amps[f] = c.max_amps;
          }
          if (c.is_reference_bin)  ref_bins++;
        }

    char *buf = (char *)ps_malloc(2048);
    if (!buf) { request->send(500, "application/json", "{\"error\":\"oom\"}"); return; }

    int off = 0;
    off += snprintf(buf + off, 2048 - off,
      "{\"total_cells\":%d,\"pop_cells\":%d,\"ref_bins\":%d,\"total_ss\":%lu",
      NUM_MATRIX_CELLS, pop_cells, ref_bins, (unsigned long)total_ss);

    off += snprintf(buf + off, 2048 - off, ",\"rpm_ss\":[");
    for (int r = 0; r < NUM_RPM_BUCKETS; r++)
      off += snprintf(buf + off, 2048 - off, "%s%lu", r ? "," : "", (unsigned long)rpm_ss[r]);
    off += snprintf(buf + off, 2048 - off, "],\"rpm_labels\":[");
    for (int r = 0; r < NUM_RPM_BUCKETS; r++)
      off += snprintf(buf + off, 2048 - off, "%s\"%s\"", r ? "," : "", RPM_LABELS[r]);

    off += snprintf(buf + off, 2048 - off, "],\"temp_ss\":[");
    for (int t = 0; t < NUM_TEMP_BUCKETS; t++)
      off += snprintf(buf + off, 2048 - off, "%s%lu", t ? "," : "", (unsigned long)temp_ss[t]);
    off += snprintf(buf + off, 2048 - off, "],\"temp_labels\":[");
    for (int t = 0; t < NUM_TEMP_BUCKETS; t++)
      off += snprintf(buf + off, 2048 - off, "%s\"%s\"", t ? "," : "", TEMP_LABELS[t]);

    off += snprintf(buf + off, 2048 - off, "],\"field_ss\":[");
    for (int f = 0; f < NUM_FIELD_BUCKETS; f++)
      off += snprintf(buf + off, 2048 - off, "%s%lu", f ? "," : "", (unsigned long)field_ss[f]);
    off += snprintf(buf + off, 2048 - off, "],\"field_labels\":[");
    for (int f = 0; f < NUM_FIELD_BUCKETS; f++)
      off += snprintf(buf + off, 2048 - off, "%s\"%s\"", f ? "," : "", FIELD_LABELS[f]);

    off += snprintf(buf + off, 2048 - off, "],\"field_min_amps\":[");
    for (int f = 0; f < NUM_FIELD_BUCKETS; f++)
      off += snprintf(buf + off, 2048 - off, "%s%.1f", f ? "," : "", field_max_amps[f] < 0 ? -1.0f : field_min_amps[f]);
    off += snprintf(buf + off, 2048 - off, "],\"field_max_amps\":[");
    for (int f = 0; f < NUM_FIELD_BUCKETS; f++)
      off += snprintf(buf + off, 2048 - off, "%s%.1f", f ? "," : "", field_max_amps[f]);

    off += snprintf(buf + off, 2048 - off, "]}");

    request->send(200, "application/json", buf);
    free(buf);
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
    float kd = 0.0f;  // reserved — was VoltageKd; D term removed; 0 preserves binary header layout
    uint32_t interval = (uint32_t)VoltageLoopInterval;

    float sbThresh = SlopeBleedThresh;
    float sbK      = SlopeBleedK;
    float sbProxV  = SlopeBleedProxV;

    memcpy(state.header + 0,  &cnt,      4);
    memcpy(state.header + 4,  &entrySize, 4);
    memcpy(state.header + 8,  &kp,       4);
    memcpy(state.header + 12, &ki,       4);
    memcpy(state.header + 16, &interval, 4);
    memcpy(state.header + 20, &kd,       4);
    memcpy(state.header + 24, &sbThresh, 4);  // SlopeBleedThresh (V/s)
    memcpy(state.header + 28, &sbK,      4);  // SlopeBleedK (A/(V/s))
    memcpy(state.header + 32, &sbProxV,  4);  // SlopeBleedProxV (V)

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
    // Defer file write to Core 1 — LittleFS open/write on Core 0 stalls SSE delivery
    pendingSaveVesselInfo = true;
    request->send(200, "application/json", "{\"success\":true}");
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

    // Defer file delete to Core 1 — LittleFS.remove on Core 0 stalls SSE delivery
    pendingClearVesselInfo = true;
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
      bool sysidModeOK = (sysMode == SYS_MODE_AUTO);
      if (sysMode == SYS_MODE_MANUAL) {
        queueConsoleMessage("SystemID: start blocked — not allowed in manual mode (duty is fixed; test cannot drive the field)");
      } else if (!sysidModeOK) {
        queueConsoleMessage("SystemID: start blocked — only allowed in AUTO mode (bulk, absorption, float, or target voltage)");
      } else if (systemIDActive == 0 && (millis() - systemIDLastEndMs) > 2000UL) {
        systemIDRequested = true;
        systemIDResultsReady = false;
        systemIDAbortRequested = false;   // clear any stale abort from a prior run
        queueConsoleMessage("SystemID: test requested via web UI");
      } else {
        queueConsoleMessage("SystemID: start ignored (cooldown or already active)");
      }
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

      pendingClearToken = true;  // nvs_commit deferred to Core 1 to avoid SSE gap

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
      pendingResetEfficiencyMatrix = true;  // deferred to Core 1 to avoid SSE gap
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
      writeFile(LittleFS, "/ManualFieldToggle.txt", inputMessage.c_str());
      ManualFieldToggle = inputMessage.toInt();
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
      MaintainMode = inputMessage.toInt();
      writeFile(LittleFS, "/MaintainMode.txt", inputMessage.c_str());
      if (MaintainMode) {
        // MaintainMode and TargetVoltageMode are mutually exclusive — clear the other.
        TargetVoltageMode = 0;
        writeFile(LittleFS, "/TargetVoltageMode.txt", "0");
      }
      queueConsoleMessageF("MaintainMode mode %s", MaintainMode ? "enabled" : "disabled");
    }
    if (request->hasParam("TargetVoltageMode")) {
      foundParameter = true;
      inputMessage = request->getParam("TargetVoltageMode")->value();
      TargetVoltageMode = inputMessage.toInt();
      writeFile(LittleFS, "/TargetVoltageMode.txt", inputMessage.c_str());
      if (TargetVoltageMode) {
        // MaintainMode and TargetVoltageMode are mutually exclusive — clear the other.
        MaintainMode = 0;
        writeFile(LittleFS, "/MaintainMode.txt", "0");
      }
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
      queueConsoleMessage("Max Thermistor Temp: Reset requested from web interface");
    }
    if (request->hasParam("ResetTemp")) {
      foundParameter = true;
      MaxAlternatorTemperatureF = 0;
      queueConsoleMessage("Max Alterantor Temp: Reset requested from web interface");
    }
    if (request->hasParam("ResetVoltage")) {
      foundParameter = true;
      IBVMax = 0;
      queueConsoleMessage("Max Voltage: Reset requested from web interface");
    }
    if (request->hasParam("ResetCurrent")) {
      foundParameter = true;
      MeasuredAmpsMax = 0;
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
    if (request->hasParam("LoadDumpDtThresh1")) {
      foundParameter = true;
      inputMessage = request->getParam("LoadDumpDtThresh1")->value();
      LoadDumpDtThresh1 = inputMessage.toFloat();
      writeFile(LittleFS, "/LoadDumpDtThresh1.txt", String(LoadDumpDtThresh1).c_str());
    }
    if (request->hasParam("LoadDumpDtThresh3")) {
      foundParameter = true;
      inputMessage = request->getParam("LoadDumpDtThresh3")->value();
      LoadDumpDtThresh3 = inputMessage.toFloat();
      writeFile(LittleFS, "/LoadDumpDtThresh3.txt", String(LoadDumpDtThresh3).c_str());
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
      // Persistence handled by saveNVSData() phase 7 (InsulDamage/GreaseDamage/BrushDamage).
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
      if (fieldActiveStatus > 0) {
        queueConsoleMessage("Weather update refused: disable the field first");
        inputMessage = "field_on";
      } else if (WiFi.RSSI() >= -76 && LatitudeNMEA != 0.0 && LongitudeNMEA != 0.0) {
        queueConsoleMessage("Weather: Manual update triggered");
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
      pendingClearOverheatHistory = true;  // deferred to Core 1 to avoid SSE gap
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
    if (request->hasParam("commitTuningScore")) {
      foundParameter = true;
      manualCommitTuningRequested = true;
      queueConsoleMessage("TuningScore: manual commit requested via UI");
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

      pendingSaveUserTableEdits = true;  // deferred to Core 1 to avoid SSE gap
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
        fuelTableRPM[i] = value;
        fuelTableUpdated = true;
      }

      snprintf(paramName, sizeof(paramName), "fuelTableGPH%d", i);
      if (request->hasParam(paramName)) {
        foundParameter = true;
        float value = request->getParam(paramName)->value().toFloat();
        fuelTableGPH[i] = value;
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
    if (request->hasParam("StartupRiseRate")) {
      foundParameter = true;
      inputMessage = request->getParam("StartupRiseRate")->value();
      writeFile(LittleFS, "/StartupRiseRate.txt", inputMessage.c_str());
      StartupRiseRate = inputMessage.toFloat();
      queueConsoleMessageF("Startup rise rate set to: %.2f A/sec", StartupRiseRate);
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
    // VoltageKd server handler removed — D term removed.
    if (request->hasParam("ProtectionProxGateV")) {
      foundParameter = true;
      inputMessage = request->getParam("ProtectionProxGateV")->value();
      ProtectionProxGateV = inputMessage.toFloat();
      writeFile(LittleFS, "/ProtectionProxGateV.txt", String(ProtectionProxGateV, 2).c_str());
      queueConsoleMessageF("Protection proximity gate: %.2f V below BulkVoltage", ProtectionProxGateV);
    }
    if (request->hasParam("SlopeBleedThresh")) {
      foundParameter = true;
      inputMessage = request->getParam("SlopeBleedThresh")->value();
      SlopeBleedThresh = inputMessage.toFloat();
      writeFile(LittleFS, "/SlopeBleedThresh.txt", String(SlopeBleedThresh, 3).c_str());
      queueConsoleMessageF("Slope bleed threshold: %.3f V/s", SlopeBleedThresh);
    }
    if (request->hasParam("SlopeBleedK")) {
      foundParameter = true;
      inputMessage = request->getParam("SlopeBleedK")->value();
      SlopeBleedK = inputMessage.toFloat();
      writeFile(LittleFS, "/SlopeBleedK.txt", String(SlopeBleedK, 1).c_str());
      queueConsoleMessageF("Slope bleed gain: %.1f A/(V/s)", SlopeBleedK);
    }
    if (request->hasParam("SlopeBleedProxV")) {
      foundParameter = true;
      inputMessage = request->getParam("SlopeBleedProxV")->value();
      SlopeBleedProxV = inputMessage.toFloat();
      writeFile(LittleFS, "/SlopeBleedProxV.txt", String(SlopeBleedProxV, 2).c_str());
      queueConsoleMessageF("Slope bleed proximity gate: %.2f V", SlopeBleedProxV);
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
    if (request->hasParam("AlternatorHardShutdownV")) {
      foundParameter = true;
      inputMessage = request->getParam("AlternatorHardShutdownV")->value();
      writeFile(LittleFS, "/AlternatorHardShutdownV.txt", inputMessage.c_str());
      AlternatorHardShutdownV = inputMessage.toFloat();
      queueConsoleMessageF("Alternator hard-shutdown voltage set to: %.2fV (absolute)", AlternatorHardShutdownV);
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
    if (request->hasParam("KHard")) {
      foundParameter = true;
      inputMessage = request->getParam("KHard")->value();
      KHard = inputMessage.toFloat();
      writeFile(LittleFS, "/KHard.txt", String(KHard, 1).c_str());
      queueConsoleMessageF("KHard set to: %.1f A/V", KHard);
      if (CVTuningMode) cvTuningParamChanged = true;
    }
    if (request->hasParam("OvGroup1Enable")) {
      foundParameter = true;
      inputMessage = request->getParam("OvGroup1Enable")->value();
      OvGroup1Enable = inputMessage.toInt() != 0;
      writeFile(LittleFS, "/OvGroup1Enable.txt", String((int)OvGroup1Enable).c_str());
      queueConsoleMessageF("OV Group 1 (prediction-based cap): %s", OvGroup1Enable ? "ENABLED" : "DISABLED");
    }
    if (request->hasParam("OvGroup2Enable")) {
      foundParameter = true;
      inputMessage = request->getParam("OvGroup2Enable")->value();
      OvGroup2Enable = inputMessage.toInt() != 0;
      writeFile(LittleFS, "/OvGroup2Enable.txt", String((int)OvGroup2Enable).c_str());
      queueConsoleMessageF("OV Group 2 (measured-voltage threshold): %s", OvGroup2Enable ? "ENABLED" : "DISABLED");
    }
    if (request->hasParam("IExcessSigSrc")) {
      foundParameter = true;
      inputMessage = request->getParam("IExcessSigSrc")->value();
      IExcessSigSrc = constrain(inputMessage.toInt(), 0, 2);
      writeFile(LittleFS, "/IExcessSigSrc.txt", String(IExcessSigSrc).c_str());
      const char* sigNames[] = { "MA(N)", "EMA(TC)", "Raw" };
      queueConsoleMessageF("iExcess signal source: %s", sigNames[IExcessSigSrc]);
    }
    if (request->hasParam("IExcessMA_N")) {
      foundParameter = true;
      inputMessage = request->getParam("IExcessMA_N")->value();
      IExcessMA_N = constrain(inputMessage.toInt(), 1, I_RING_SIZE);
      writeFile(LittleFS, "/IExcessMA_N.txt", String(IExcessMA_N).c_str());
      queueConsoleMessageF("iExcess MA window: N=%d", IExcessMA_N);
    }
    if (request->hasParam("OutputPIDSigSrc")) {
      foundParameter = true;
      inputMessage = request->getParam("OutputPIDSigSrc")->value();
      OutputPIDSigSrc = constrain(inputMessage.toInt(), 0, 2);
      writeFile(LittleFS, "/OutputPIDSigSrc.txt", String(OutputPIDSigSrc).c_str());
      const char* sigNames[] = { "EMA(TC)", "MA(N)", "Raw" };
      queueConsoleMessageF("Output PID signal source: %s", sigNames[OutputPIDSigSrc]);
    }
    if (request->hasParam("OutputPIDMA_N")) {
      foundParameter = true;
      inputMessage = request->getParam("OutputPIDMA_N")->value();
      OutputPIDMA_N = constrain(inputMessage.toInt(), 1, I_RING_SIZE);
      writeFile(LittleFS, "/OutputPIDMA_N.txt", String(OutputPIDMA_N).c_str());
      queueConsoleMessageF("Output PID MA window: N=%d", OutputPIDMA_N);
    }
    if (request->hasParam("OutputPIDFilterTC")) {
      foundParameter = true;
      inputMessage = request->getParam("OutputPIDFilterTC")->value();
      OutputPIDFilterTC = inputMessage.toFloat();
      writeFile(LittleFS, "/OutputPIDFilterTC.txt", String(OutputPIDFilterTC).c_str());
      queueConsoleMessageF("Output PID EMA TC: %.1f ms", OutputPIDFilterTC);
    }
    if (request->hasParam("VoltageFilterTC")) {
      foundParameter = true;
      inputMessage = request->getParam("VoltageFilterTC")->value();
      VoltageFilterTC = inputMessage.toFloat();
      writeFile(LittleFS, "/VoltageFilterTC.txt", String(VoltageFilterTC).c_str());
      queueConsoleMessageF("Voltage EMA TC: %.1f ms", VoltageFilterTC);
    }
    if (request->hasParam("TdPred")) {
      foundParameter = true;
      inputMessage = request->getParam("TdPred")->value();
      TdPred = constrain(inputMessage.toFloat(), 0.01f, 0.30f);
      writeFile(LittleFS, "/TdPred.txt", String(TdPred, 3).c_str());
      queueConsoleMessageF("OV prediction horizon set to: %.3f s", TdPred);
    }
    if (request->hasParam("OvMeasMarginV")) {
      foundParameter = true;
      inputMessage = request->getParam("OvMeasMarginV")->value();
      OvMeasMarginV = constrain(inputMessage.toFloat(), 0.020f, 0.500f);
      writeFile(LittleFS, "/OvMeasMarginV.txt", String(OvMeasMarginV, 3).c_str());
      queueConsoleMessageF("Group 2 measured-voltage trigger margin set to: %.0f mV", OvMeasMarginV * 1000.0f);
    }
    if (request->hasParam("OvPredMarginV")) {
      foundParameter = true;
      inputMessage = request->getParam("OvPredMarginV")->value();
      OvPredMarginV = constrain(inputMessage.toFloat(), 0.050f, 1.000f);
      writeFile(LittleFS, "/OvPredMarginV.txt", String(OvPredMarginV, 3).c_str());
      queueConsoleMessageF("Group 1 prediction trigger margin set to: %.0f mV", OvPredMarginV * 1000.0f);
    }
    if (request->hasParam("DvdtAlpha")) {
      foundParameter = true;
      inputMessage = request->getParam("DvdtAlpha")->value();
      DvdtAlpha = constrain(inputMessage.toFloat(), 0.01f, 0.50f);
      writeFile(LittleFS, "/DvdtAlpha.txt", String(DvdtAlpha, 3).c_str());
      queueConsoleMessageF("dvdt EMA alpha set to: %.3f (~%.0f ms TC at 5ms cadence)", DvdtAlpha, 5.0f * (1.0f - DvdtAlpha) / DvdtAlpha);
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
    if (request->hasParam("commitCVTuningScore")) {
      foundParameter = true;
      manualCommitCVTuningRequested = true;
      queueConsoleMessage("CVTuningScore: manual commit requested via UI");
    }
    if (request->hasParam("restartCVTest")) {
      foundParameter = true;
      cvTuningScore = {};
      cvTuningParamChanged = false;
      queueConsoleMessage("CVTuningScore: test restarted via UI");
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
      // Function timing — session worsts (full list mirrors the periodic
      // worstWindow reset block in loop() so every timer on the dashboard
      // actually clears on button press).
      ft_ReadAnalogInputs.worstSession = 0;
      ft_saveNVSData.worstSession = 0;
      ft_AdjustFieldLearnMode.worstSession = 0;
      ft_logDashboardValues.worstSession = 0;
      ft_updateSystemHealthStats.worstSession = 0;
      ft_checkWiFiConnection.worstSession = 0;
      ft_SendWifiData.worstSession = 0;
      ft_CheckAlarms.worstSession = 0;
      ft_calculateDerivedMetrics.worstSession = 0;
      ft_ch1_compute_stats.worstSession = 0;
      ft_uploadSensorHistory.worstSession = 0;
      ft_uploadBufferedRecords.worstSession = 0;
      ft_buildConfigPayload.worstSession = 0;
      ft_UpdateEngineRuntime.worstSession = 0;
      ft_UpdateEngineFuel.worstSession = 0;
      ft_UpdateBatterySOC.worstSession = 0;
      ft_UpdateTravelStatistics.worstSession = 0;
      ft_UpdateDistanceThisInterval.worstSession = 0;
      ft_UpdateBoardTempPressureMaximums.worstSession = 0;
      ft_handleSocGainReset.worstSession = 0;
      ft_handleAltZeroReset.worstSession = 0;
      ft_calculateChargeTimes.worstSession = 0;
      ft_UpdateSailingMetrics.worstSession = 0;
      ft_updateWeatherMode.worstSession = 0;
      ft_updateSensorWindow.worstSession = 0;
      ft_checkTimeSync.worstSession = 0;
      ft_rai_total.worstSession = 0;
      ft_rai_ina228.worstSession = 0;
      ft_rai_ads_state.worstSession = 0;
      ft_rai_bmp_state.worstSession = 0;
      ft_rai_imu.worstSession = 0;
      ft_updateAccelMetrics.worstSession = 0;
      ft_ReadVEData.worstSession = 0;
      ft_efficiencyTracker.worstSession = 0;
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
      // INA228 interval stats — clears all windows AND all-time accumulators
      // (without this, ina_worst_at/ina_worst_2m survived the reset and the
      // dashboard kept showing stale 149 ms NVS-stall spikes forever).
      resetINA228AllStats();
      // Voltage loop worst (labeled "Worst Session" in UI) — wasn't reset before;
      // adding here so the label-renamed "Worst — last X min" tracks reality.
      voltLoopWorstInterval_ses = 0;
      // Stamp the reset moment. Dashboard reads CSV1 slot 28 = (millis()-this)/1000
      // and formats it as "last 12 min" / "last 1.4 hr" on all .session-window-label spans.
      perfCountersResetMs = millis();
      queueConsoleMessage("Peak counters reset from web interface");
    }

    if (request->hasParam("forceCloudFlush")) {
      foundParameter = true;
      // "Upload Cloud Now" button — bypasses fieldOffSettled and the 13-sec
      // interval throttle. The cloud-feature block in loop() reads this flag,
      // drains the PSRAM ring through the HTTPS queue, and clears the flag
      // once the ring is empty.
      forceCloudFlushPending = true;
      queueConsoleMessage("Cloud sync: forced flush requested");
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
      stateRevision++;      // Increment whenever any setting changed
      settingsDirty = true; // trigger immediate CSV3 settings echo
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
    settingsDirty = true;  // send CSV3 immediately so new client gets current settings
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
        "\"ls\":%.2f,\"lst\":%.1f,\"lwo\":%.3f,\"lio\":%.4f,\"lus\":%.3f,"
        "\"fov\":%d,\"iex\":%d,\"ld\":%d,\"hoc\":%d,"
        "\"vkp\":%.3f,\"vki\":%.3f,\"vkd\":%.2f,"
        "\"srr\":%.1f,\"sfr\":%.1f,"
        "\"abl\":%.2f,\"arl\":%.3f,\"asp\":%d,\"irf\":%.2f,"
        "\"kh\":%.1f,"
        "\"iek\":%.1f,\"ien\":%d,\"iekb\":%.2f,"
        "\"lddt\":%.0f,\"ldt1\":%.0f,\"ldt3\":%.0f,"
        "\"tc\":%.0f,\"wa\":%.2f,\"wp\":%d,\"ko\":%.1f,\"cr\":%d,"
        "\"rpm\":%.0f,\"tmp\":%.1f,\"bv\":%.2f,\"soc\":%.1f,\"cvt\":%.2f}",
        i > 0 ? "," : "",
        r.runNumber, r.score, r.avgSettlingTimeSec, r.worstOvershootV,
        r.avgIntegratedOvershootVs, r.activeTimeSec,
        r.lowScore, r.avgLowSettlingTimeSec, r.worstLowOvV, r.avgLowIntOvVs, r.worstLowUndershootV,
        (int)r.fastOvFires, (int)r.iExcessFires, (int)r.loadDumpFires, (int)r.hardOcFires,
        r.voltageKp, r.voltageKi, r.voltageKd,
        r.setpointRiseRate, r.setpointFallRate,
        r.awBleedRate, r.awRecoverRate, (int)r.awSeedProtectMs, r.iExcessReseedFrac,
        r.kHard,
        r.iExcessK, (int)r.iExcessN, r.iExcessKBleed,
        r.loadDumpDtThresh, r.loadDumpDtThresh1, r.loadDumpDtThresh3,
        r.inputFilterTC, r.waveAmplitudeV, (int)r.wavePeriodSec, r.kOvershoot, (int)r.consecutiveReads,
        r.avgRPM, r.avgAltTempF, r.battVAtStart, r.socAtStart * 100.0f, r.chargingVoltageTarget);
    }
    // Active test state
    bool cvTestActive = (CVTuningMode && cvTuningScore.testStarted);
    float cvts = 0.0f;
    if (cvTestActive && cvTuningScore.activeTimeSec > 0.0f) {
      cvts = 1000.0f * (cvTuningScore.totalIntegratedOvershootVs
                        + cvTuningScore.totalLowIntOvVs
                        + cvTuningScore.totalLowUndershootVs)
             / cvTuningScore.activeTimeSec;
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
    pendingSaveCVTuningLog = true;  // deferred to Core 1 — avoids blocking Core 0 SSE
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
    bool testActive = (ThermalTuningMode && thermalTuningScore.testStarted && thermalTuningScore.waveHigh);
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
    pendingSaveThermalTuningLog = true;  // deferred to Core 1 — avoids blocking Core 0 SSE
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
    pendingSaveTuningLog = true;  // deferred to Core 1 — avoids blocking Core 0 SSE
    request->send(200, "text/plain", "OK");
  });

  server.on("/resetVoltageLoop", HTTP_POST, [](AsyncWebServerRequest *request) {
    cvLoopResetRequested = true;
    request->send(200, "text/plain", "OK");
  });

  server.on("/resetVoltageProtectionCounters", HTTP_POST, [](AsyncWebServerRequest *request) {
    g_fastOvClampCount = 0;
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
  const unsigned long EVENTSOURCE_COOLDOWN = 10;
  const unsigned long CONSOLE_MESSAGE_INTERVAL = 1000;

  bool canSendNow = (now - lastEventSourceSend >= EVENTSOURCE_COOLDOWN);
  if (!canSendNow) return;

  bool sentSomething = false;

  // PRIORITY 1: CSVData
  if (!sentSomething && now - prev_millis5 >= webgaugesinterval && events.count() > 0) {
    WifiHeartBeat = WifiHeartBeat + 1;

    static char *payload1 = nullptr;
    static const size_t PAYLOAD1_SIZE = 1400;
    if (!payload1) {
      payload1 = (char *)ps_malloc(PAYLOAD1_SIZE);  // bumped from 500, allocated to PSRAM
      if (!payload1) {
        Serial.println("FATAL: payload1 ps_malloc failed");
        return;
      }
    }
    int payload1Len = snprintf(payload1, PAYLOAD1_SIZE,
                               "%d,"  // CSV1_FIELD_COUNT
                               "%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,"
                               "%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,"
                               "%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,"
                               "%d,%d,%d,%d",

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
                               SafeInt(WifiHeartBeat),                  // 10
                               SafeInt(vvout, 100),                     // 11
                               SafeInt(iiout, 100),                     // 12
                               SafeInt(FreeHeap),                       // 13
                               SafeInt(Alarm_Status),                   // 14
                               SafeInt(fieldActiveStatus),              // 15
                               SafeInt((int)currentMode),               // 16
                               SafeInt(stateRevision),                  // 17
                               SafeInt(setpointLimited, 100),           // 18
                               SafeInt(uTargetAmps, 100),               // 19
                               SafeInt(pidInput, 100),                  // 20
                               SafeInt(pidOutput, 100),                 // 21
                               SafeInt(pidError, 100),                  // 22
                               SafeInt(imu_heel_deg, 100),              // 23
                               SafeInt(imu_pitch_deg, 100),             // 24
                               SafeInt(imu_vertical_accel_g, 1000),     // 25
                               SafeInt(imu_yaw_rate_dps, 100),          // 26
                               SafeInt(imu_total_accel_g, 1000),        // 27
                               SafeInt((int32_t)((millis() - perfCountersResetMs) / 1000UL)),  // 28 — seconds since perf-counters reset (0 = boot)
                               SafeInt(shutdownPhase),                  // 29
                               SafeInt(BatteryV, 100),                  // 30 — raw ADS1115
                               SafeInt(MeasuredAmps_filtered, 100),     // 31
                               SafeInt(ChargingVoltageTarget * 100),    // 32
                               SafeInt(Icv * 100)                       // 33
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
  // PRIORITY 3: CSVData2 (status/settings data).
  // During a plant delay test, bypass the sentSomething gate and send every 500 ms so the
  // UI phase-progress display stays within one poll tick of the actual firmware state.
  // Normal operation: every 5 s, gated behind CSV1 to avoid double-sending per tick.
  const bool sysIDRunning = (systemIDActive != 0);
  if ((sysIDRunning || !sentSomething) && now - lastpayload2send >= (sysIDRunning ? 500UL : 5000UL) && events.count() > 0) {
    WifiStrength = cachedWiFiRSSI;
    ch1_compute_stats();
    static char *payload2 = nullptr;
    static const size_t PAYLOAD2_SIZE = 3400;  // (410 fields + 1) × 7 = 2877, rounded up to 3400
    if (!payload2) {
      payload2 = (char *)ps_malloc(PAYLOAD2_SIZE);  // allocated to PSRAM
      if (!payload2) {
        Serial.println("FATAL: payload2 ps_malloc failed");
        return;
      }
    }  // Format string:
    int payload2Len = snprintf(payload2, PAYLOAD2_SIZE,
                               "%d,"  // CSV2_FIELD_COUNT
                               "%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,"
                               "%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,"
                               "%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,"
                               "%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,"
                               "%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,"
                               "%d,%d,%d,%u,%u,%d,%d,%d,%d,%d,"
                               "%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,"
                               "%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,"
                               "%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,"
                               "%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,"
                               "%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,"
                               "%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,"
                               "%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,"
                               "%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,"
                               "%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,"
                               "%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,"
                               "%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,"
                               "%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,"
                               "%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,"
                               "%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,"
                               "%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,"
                               "%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,"
                               "%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,"
                               "%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,"
                               "%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,"
                               "%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,"
                               "%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,"
                               "%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,"
                               "%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,"
                               "%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,"
                               "%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,"
                               "%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,"
                               "%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,"
                               "%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,"
                               "%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,"
                               "%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,"
                               "%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,"
                               "%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,"
                               "%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,"
                               "%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,"
                               "%d,%d,%d,%d,%d,%d,%d,%d,%d,%d",

                               CSV2_FIELD_COUNT,
                               SafeInt(IBVMax, 100),                             // 0
                               SafeInt(MeasuredAmpsMax, 100),                    // 1
                               SafeInt(RPMMax),                                  // 2
                               SafeInt(SOC_percent),                             // 3
                               SafeInt(EngineRunTime * 100 / 3600, 1),           // 4
                               SafeInt(AlternatorOnTime * 100 / 3600, 1),        // 5
                               SafeInt(AlternatorFuelUsed, 100),                 // 6
                               SafeInt(ChargedEnergy),                           // 7
                               SafeInt(DischargedEnergy),                        // 8
                               SafeInt(AlternatorChargedEnergy),                 // 9
                               SafeInt(MaxAlternatorTemperatureF),               // 10
                               SafeInt(temperatureThermistor),                   // 11
                               SafeInt(MaxTemperatureThermistor),                // 12
                               SafeInt(VictronCurrent, 100),                     // 13
                               SafeInt(timeToFullChargeMin),                     // 14
                               SafeInt(timeToFullDischargeMin),                  // 15
                               SafeInt(LatitudeNMEA * 1000000),                  // 16
                               SafeInt(LongitudeNMEA * 1000000),                 // 17
                               SafeInt(SatelliteCountNMEA),                      // 18
                               SafeInt(LastSessionDuration),                     // 19
                               SafeInt(LastSessionMaxLoopTime),                  // 20
                               SafeInt(lastSessionMinHeap),                      // 21
                               SafeInt(wifiReconnectsTotal),                     // 22
                               SafeInt(LastResetReason),                         // 23
                               SafeInt(ancientResetReason),                      // 24
                               SafeInt(totalPowerCycles),                        // 25
                               SafeInt(MinFreeHeap),                             // 26
                               SafeInt(currentWeatherMode),                      // 27
                               SafeInt(UVToday, 100),                            // 28
                               SafeInt(UVTomorrow, 100),                         // 29
                               SafeInt(UVDay2, 100),                             // 30
                               SafeInt(weatherDataValid),                        // 31
                               SafeInt(SolarWatts),                              // 32
                               SafeInt(performanceRatio, 100),                   // 33
                               SafeInt(VeData),                                  // 34
                               SafeInt(NMEA0183Data),                            // 35
                               SafeInt(NMEA2KData),                              // 36
                               SafeInt(alarmLatch ? 1 : 0),                      // 37
                               SafeInt(ResetAlarmLatch),                         // 38
                               SafeInt(ResetLearningTable),                      // 39
                               SafeInt(ClearOverheatHistory),                    // 40
                               SafeInt(DynamicShuntGainFactor, 1000),            // 41
                               SafeInt(DynamicAltCurrentZero, 1000),             // 42
                               SafeInt(InsulationLifePercent, 100),              // 43
                               SafeInt(GreaseLifePercent, 100),                  // 44
                               SafeInt(BrushLifePercent, 100),                   // 45
                               SafeInt(PredictedLifeHours),                      // 46
                               SafeInt(LifeIndicatorColor),                      // 47
                               SafeInt(pKwHrToday, 100),                         // 48
                               SafeInt(pKwHrTomorrow, 100),                      // 49
                               SafeInt(pKwHr2days, 100),                         // 50
                               SafeInt(ambientTemp),                             // 51
                               SafeInt(baroPressure),                            // 52
                               SafeInt(firmwareVersionInt),                      // 53
                               deviceIdUpper,                                    // 54 (%u)
                               deviceIdLower,                                    // 55 (%u)
                               SafeInt(ChargedEnergy_AllTime),                   // 56
                               SafeInt(AlternatorFuelUsed_AllTime, 100),         // 57
                               SafeInt(PeakVoltage_AllTime, 100),                // 58
                               SafeInt(EngineRunTime_AllTime * 100 / 3600, 1),   // 59
                               SafeInt(MinVoltage, 100),                         // 60
                               SafeInt(MinVoltage_AllTime, 100),                 // 61
                               SafeInt(ChargeCycles, 100),                       // 62
                               SafeInt(ChargeCycles_AllTime, 100),               // 63
                               SafeInt(EngineFuelUsed, 100),                     // 64
                               SafeInt(EngineFuelUsed_AllTime, 100),             // 65
                               SafeInt(TotalDistance, 10),                       // 66
                               SafeInt(TotalDistance_AllTime, 10),               // 67
                               SafeInt(MaxSpeed, 100),                           // 68
                               SafeInt(MaxSpeed_AllTime, 100),                   // 69
                               SafeInt(SolarChargedEnergy),                      // 70
                               SafeInt(SolarChargedEnergy_AllTime),              // 71
                               SafeInt(AlternatorChargedEnergy_AllTime),         // 72
                               SafeInt(DischargedEnergy_AllTime),                // 73
                               SafeInt(AvgSOC_AllTime, 100),                     // 74
                               SafeInt(AvgSpeed_AllTime, 100),                   // 75
                               SafeInt(AvgSpeed, 100),                           // 76
                               SafeInt(AlternatorOnTime_AllTime * 100 / 3600, 1),// 77
                               SafeInt(EngineCycles_AllTime),                    // 78
                               SafeInt(MaxAlternatorTemperatureF_AllTime),       // 79
                               SafeInt(MaxTemperatureThermistor_AllTime),        // 80
                               SafeInt(MeasuredAmpsMax_AllTime, 100),            // 81
                               SafeInt(RPMMax_AllTime),                          // 82
                               SafeInt(Ignition),                                // 83
                               SafeInt(inBulkStage ? 1 : 0),                    // 84
                               // 85: seconds of WiFi remaining — WiFi wake OR shutdown drain window (shown as countdown banner)
                               SafeInt(
                                 (wifiWakeStart > 0 && (millis() - wifiWakeStart) < WIFI_WAKE_DURATION)
                                   ? (WIFI_WAKE_DURATION - (millis() - wifiWakeStart)) / 1000
                                   : (pendingShutdownFlush && shutdownCloudDeadlineMs > millis())
                                       ? (shutdownCloudDeadlineMs - millis()) / 1000
                                       : 0),                                     // 85
                               SafeInt(bufferedRecordCount),                     // 86
                               SafeInt((bufferedRecordCount * 100) / MAX_BUFFERED_RECORDS), // 87
                               SafeInt(MAX_BUFFERED_RECORDS),                    // 88
                               SafeInt(COGNMEA),                                 // 89
                               SafeInt(SOGNMEA, 100),                            // 90
                               SafeInt(ApparentWindSpeedNMEA, 100),              // 91
                               SafeInt(ApparentWindAngleNMEA),                   // 92
                               SafeInt(TrueWindSpeedNMEA, 100),                  // 93
                               SafeInt(TrueWindAngleNMEA),                       // 94
                               SafeInt(LeewayNMEA),                              // 95
                               SafeInt(VMGNMEA, 100),                            // 96
                               SafeInt(VMGTargetBearing),                        // 97
                               SafeInt(VMGUseTrueWind),                          // 98
                               SafeInt(cpuLoadCore0),                            // 99
                               SafeInt(cpuLoadCore0Max),                         // 100
                               SafeInt(cpuLoadCore1),                            // 101
                               SafeInt(cpuLoadCore1Max),                         // 102
                               SafeInt(hasForcedUpdate ? 1 : 0),                 // 103
                               SafeInt(forcedFwVersionInt),                      // 104
                               (forcedUpdateDeadline),                           // 105
                               SafeInt(stateRevision),                           // 106
                               SafeInt(hardwarePresent),                         // 107
                               SafeInt(imu_accel_x_raw, 1000),                   // 108
                               SafeInt(imu_accel_y_raw, 1000),                   // 109
                               SafeInt(imu_accel_z_raw, 1000),                   // 110
                               SafeInt(imu_gyro_x_raw, 100),                     // 111
                               SafeInt(imu_gyro_y_raw, 100),                     // 112
                               SafeInt(imu_gyro_z_raw, 100),                     // 113
                               SafeInt(imuWindow->accel_x_min),                  // 114
                               SafeInt(imuWindow->accel_x_max),                  // 115
                               SafeInt(imuWindow->accel_x_valid_us > 0 ? (int)((int64_t)imuWindow->accel_x_area_v_us / (int64_t)imuWindow->accel_x_valid_us) : 0), // 116
                               SafeInt(imuWindow->accel_y_min),                  // 117
                               SafeInt(imuWindow->accel_y_max),                  // 118
                               SafeInt(imuWindow->accel_y_valid_us > 0 ? (int)((int64_t)imuWindow->accel_y_area_v_us / (int64_t)imuWindow->accel_y_valid_us) : 0), // 119
                               SafeInt(imuWindow->accel_z_min),                  // 120
                               SafeInt(imuWindow->accel_z_max),                  // 121
                               SafeInt(imuWindow->accel_z_valid_us > 0 ? (int)((int64_t)imuWindow->accel_z_area_v_us / (int64_t)imuWindow->accel_z_valid_us) : 0), // 122
                               SafeInt(imuWindow->gyro_x_min),                   // 123
                               SafeInt(imuWindow->gyro_x_max),                   // 124
                               SafeInt(imuWindow->gyro_x_valid_us > 0 ? (int)((int64_t)imuWindow->gyro_x_area_v_us / (int64_t)imuWindow->gyro_x_valid_us) : 0),    // 125
                               SafeInt(imuWindow->gyro_y_min),                   // 126
                               SafeInt(imuWindow->gyro_y_max),                   // 127
                               SafeInt(imuWindow->gyro_y_valid_us > 0 ? (int)((int64_t)imuWindow->gyro_y_area_v_us / (int64_t)imuWindow->gyro_y_valid_us) : 0),    // 128
                               SafeInt(imuWindow->gyro_z_min),                   // 129
                               SafeInt(imuWindow->gyro_z_max),                   // 130
                               SafeInt(imuWindow->gyro_z_valid_us > 0 ? (int)((int64_t)imuWindow->gyro_z_area_v_us / (int64_t)imuWindow->gyro_z_valid_us) : 0),    // 131
                               SafeInt(imuWindow->heel_min),                     // 132
                               SafeInt(imuWindow->heel_max),                     // 133
                               SafeInt(imuWindow->heel_valid_us > 0 ? (int)((int64_t)imuWindow->heel_area_v_us / (int64_t)imuWindow->heel_valid_us) : 0),           // 134
                               SafeInt(imuWindow->pitch_min),                    // 135
                               SafeInt(imuWindow->pitch_max),                    // 136
                               SafeInt(imuWindow->pitch_valid_us > 0 ? (int)((int64_t)imuWindow->pitch_area_v_us / (int64_t)imuWindow->pitch_valid_us) : 0),        // 137
                               SafeInt(imuWindow->vertical_accel_min),           // 138
                               SafeInt(imuWindow->vertical_accel_max),           // 139
                               SafeInt(imuWindow->vertical_accel_valid_us > 0 ? (int)((int64_t)imuWindow->vertical_accel_area_v_us / (int64_t)imuWindow->vertical_accel_valid_us) : 0), // 140
                               SafeInt(imuWindow->total_accel_min),              // 141
                               SafeInt(imuWindow->total_accel_max),              // 142
                               SafeInt(imuWindow->total_accel_valid_us > 0 ? (int)((int64_t)imuWindow->total_accel_area_v_us / (int64_t)imuWindow->total_accel_valid_us) : 0),         // 143
                               SafeInt(imuWindow->slam_count),                   // 144
                               SafeInt(imuWindow->slam_peak_max),                // 145
                               SafeInt(imu_slam_count_lifetime),                 // 146
                               SafeInt(imu_capsize_count),                       // 147
                               SafeInt(imu_pitchpole_count),                     // 148
                               SafeInt(imuWindow->heel_change_60s),              // 149
                               SafeInt(imuWindow->heel_deviation_60s),           // 150
                               SafeInt(imuWindow->pitch_change_60s),             // 151
                               SafeInt(imuWindow->pitch_deviation_60s),          // 152
                               SafeInt(imuWindow->wave_period),                  // 153
                               SafeInt(imu_heel_max_lifetime, 100),              // 154
                               SafeInt(imu_pitch_max_lifetime, 100),             // 155
                               SafeInt(imu_slam_peak_lifetime, 1000),            // 156
                               SafeInt(imu_fifo_overrun_count),                  // 157
                               SafeInt(imu_i2c_error_count),                     // 158
                               SafeInt(imu_unknown_tag_count),                   // 159
                               SafeInt(imuRingBuffer->accel_dropped),            // 160
                               SafeInt(imuRingBuffer->gyro_dropped),             // 161
                               SafeInt(imu_total_samples_accel),                 // 162
                               SafeInt(imu_total_samples_gyro),                  // 163
                               SafeInt(IMUReadTime2),                            // 164
                               SafeInt(IMUReadTime),                             // 165
                               SafeInt(adsI2CErrorCount),                        // 166
                               SafeInt(tempPIDActive ? 1 : 0),                   // 167
                               SafeInt(tempPIDInput_d, 100),                     // 168
                               SafeInt(tempPIDSetpoint_d, 100),                  // 169
                               SafeInt(thermalPenaltyAmps, 100),                 // 170
                               SafeInt(innerTermP, 100),                         // 171
                               SafeInt(innerTermI, 100),                         // 172
                               SafeInt(innerTermD, 100),                         // 173
                               SafeInt(outerTermP, 100),                         // 174
                               SafeInt(outerTermI, 100),                         // 175
                               SafeInt(outerTermD, 100),                         // 176
                               SafeInt(thermalSlopeFPerSec, 1000),               // 177
                               SafeInt(chargeStageDisplay),                      // 178
                               SafeInt(voltageControlActive),                    // 179
                               SafeInt((ChargingVoltageTarget - getBatteryVoltage()) * 100), // 180
                               SafeInt(cv_I * 100),                              // 181
                               SafeInt(inIdleStage),                             // 182
                               SafeInt(referenceFinalized),                      // 183
                               SafeInt(ft_rai_total.worstWindow),                // 184
                               SafeInt(ft_rai_total.worstSession),               // 185
                               SafeInt(ft_rai_ina228.worstWindow),               // 186
                               SafeInt(ft_rai_ina228.worstSession),              // 187
                               SafeInt(ft_rai_ads_state.worstWindow),            // 188
                               SafeInt(ft_rai_ads_state.worstSession),           // 189
                               SafeInt(ft_rai_bmp_state.worstWindow),            // 190
                               SafeInt(ft_rai_bmp_state.worstSession),           // 191
                               SafeInt(ft_rai_imu.worstWindow),                  // 192
                               SafeInt(ft_rai_imu.worstSession),                 // 193
                               0,                                                 // 194 reserved — was cv_D (D term removed)
                               SafeInt(tempReadFailCount),                       // 195
                               SafeInt(tempCrcFailCount),                        // 196
                               SafeInt(tempCrcRecoveredCount),                   // 197
                               SafeInt(tempAllFFCount),                          // 198
                               SafeInt(tempPowerOn85Count),                      // 199
                               SafeInt(tempOutOfRangeCount),                     // 200
                               SafeInt(tempRequestFailCount),                    // 201
                               SafeInt(tempConnectedFailCount),                  // 202
                               SafeInt(tempResolutionFixCount),                  // 203
                               SafeInt(tempRereadFailCount),                     // 204
                               SafeInt(tempResolutionFixCrcFailCount),           // 205
                               SafeInt(tempEnumerateFailCount),                  // 206
                               SafeInt(warmupCeiling),                           // 207
                               SafeInt(imu_min_moving_gentle),                   // 208
                               SafeInt(imu_min_moving_moderate),                 // 209
                               SafeInt(imu_min_moving_rough),                    // 210
                               SafeInt(imu_min_moving_extreme),                  // 211
                               SafeInt(imu_min_stat_gentle),                     // 212
                               SafeInt(imu_min_stat_moderate),                   // 213
                               SafeInt(imu_min_stat_rough),                      // 214
                               SafeInt(imu_min_stat_extreme),                    // 215
                               SafeInt(imu_heel_deviation_120s, 100),            // 216 — ×100, 2dp degrees
                               SafeInt(imu_pitch_deviation_120s, 100),           // 217 — ×100, 2dp degrees
                               SafeInt(imu_heading_swing_120s, 10),              // 218 — ×10, 1dp degrees; -10 = no compass data
                               SafeInt(g_dBcur_dt, 10),                          // 219 — ×10, 1dp A/s battery current rate of change
                               (int)g_loadDumpActive,                            // 220 — 1 if load dump feedforward is active
                               SafeInt(thermalLiveScoreVal[0], 10000),           // 221 — ×10000
                               SafeInt(thermalLiveScoreVal[1], 10000),           // 222 — ×10000
                               SafeInt(thermalLiveScoreVal[2], 10000),           // 223 — ×10000
                               SafeInt(thermalLiveScoreVal[3], 10000),           // 224 — ×10000
                               (ThermalTuningMode && thermalTuningScore.testStarted && thermalTuningScore.waveHigh) ? 1 : 0, // 225
                               SafeInt(ft_updateAccelMetrics.worstWindow),// 226
                               SafeInt(ft_updateAccelMetrics.worstSession),// 227
                               // from CSV1
                               SafeInt(WifiStrength),                            // 228
                               SafeInt(SendWifiTime),                            // 229
                               SafeInt(AnalogReadTime),                          // 230
                               SafeInt(VeTime),                                  // 231
                               SafeInt(MaximumLoopTime),                         // 232
                               SafeInt(HeadingNMEA),                             // 233
                               SafeInt(EngineCycles),                            // 234
                               SafeInt(CurrentSessionDuration),                  // 235
                               SafeInt(timeAxisModeChanging),                    // 236
                               SafeInt(currentPartitionType),                    // 237
                               SafeInt(g_fastOvCurrentCap, 100),                 // 238
                               SafeInt(g_fastOvClampCount),                      // 239
                               SafeInt(g_fastOvHardCount),                       // 240
                               SafeInt(ch1_last_ms),                             // 241
                               SafeInt(ch1_avg_10s, 100),                        // 242
                               SafeInt(ch1_worst_10s),                           // 243
                               SafeInt(ch1_over2x_10s),                          // 244
                               SafeInt(ch1_n_10s),                               // 245
                               SafeInt(ch1_avg_2m, 100),                         // 246
                               SafeInt(ch1_worst_2m),                            // 247
                               SafeInt(ch1_over2x_2m),                           // 248
                               SafeInt(ch1_n_2m),                                // 249
                               SafeInt(ch1_avg_at, 100),                         // 250
                               SafeInt(ch1_worst_at),                            // 251
                               SafeInt(ch1_over2x_at),                           // 252
                               SafeInt(ch1_n_at),                                // 253
                               SafeInt(g_iExcessCount),                          // 254
                               SafeInt(g_inaOVCount),                            // 255
                               SafeInt(g_hardOCCount),                           // 256
                               SafeInt(g_voltSpikeCount),                        // 257
                               SafeInt(g_voltDisagreeCritCount),                 // 258
                               SafeInt(g_voltDisagreeWarnCount),                 // 259
                               SafeInt(g_voltImplausibleCount),                  // 260
                               SafeInt(g_tempCritCount),                         // 261
                               SafeInt(g_tempSustainedCount),                    // 262
                               SafeInt(g_tempStaleCount),                        // 263
                               SafeInt(g_currentStaleCount),                     // 264
                               SafeInt(imu_msi_score, 100),                      // 265
                               SafeInt(imu_vomit_pct, 100),                      // 266
                               SafeInt(imu_anchorage_comfort, 100),              // 267
                               SafeInt(ina_last_ms),                             // 268
                               SafeInt(ina_avg_10s, 100),                        // 269
                               SafeInt(ina_worst_10s),                           // 270
                               SafeInt(ina_over2x_10s),                          // 271
                               SafeInt(ina_avg_2m, 100),                         // 272
                               SafeInt(ina_worst_2m),                            // 273
                               SafeInt(ina_over2x_2m),                           // 274
                               SafeInt(ina_avg_at, 100),                         // 275
                               SafeInt(ina_worst_at),                            // 276
                               SafeInt(ina_over2x_at),                           // 277
                               SafeInt(loopTime5sWindow / 1000),                 // 278
                               SafeInt(MaximumLoopTime / 1000),                  // 279
                               SafeInt(ft_SendWifiData.worstWindow),      // 280
                               SafeInt(ft_SendWifiData.worstSession),     // 281
                               SafeInt(ft_CheckAlarms.worstWindow),       // 282
                               SafeInt(ft_CheckAlarms.worstSession),      // 283
                               SafeInt(ft_calculateDerivedMetrics.worstWindow),  // 284
                               SafeInt(ft_calculateDerivedMetrics.worstSession), // 285
                               SafeInt(ft_logDashboardValues.worstWindow),       // 286
                               SafeInt(ft_logDashboardValues.worstSession),      // 287
                               SafeInt(ft_updateSystemHealthStats.worstWindow),  // 288
                               SafeInt(ft_updateSystemHealthStats.worstSession), // 289
                               SafeInt(ft_checkWiFiConnection.worstWindow),      // 290
                               SafeInt(ft_checkWiFiConnection.worstSession),     // 291
                               SafeInt(ft_ch1_compute_stats.worstWindow),        // 292
                               SafeInt(ft_ch1_compute_stats.worstSession),       // 293
                               SafeInt(ft_UpdateEngineRuntime.worstWindow),      // 294
                               SafeInt(ft_UpdateEngineRuntime.worstSession),     // 295
                               SafeInt(ft_UpdateEngineFuel.worstWindow),         // 296
                               SafeInt(ft_UpdateEngineFuel.worstSession),        // 297
                               SafeInt(ft_UpdateBatterySOC.worstWindow),         // 298
                               SafeInt(ft_UpdateBatterySOC.worstSession),        // 299
                               SafeInt(ft_UpdateTravelStatistics.worstWindow),   // 300
                               SafeInt(ft_UpdateTravelStatistics.worstSession),  // 301
                               SafeInt(ft_UpdateDistanceThisInterval.worstWindow),     // 302
                               SafeInt(ft_UpdateDistanceThisInterval.worstSession),    // 303
                               SafeInt(ft_UpdateBoardTempPressureMaximums.worstWindow),// 304
                               SafeInt(ft_UpdateBoardTempPressureMaximums.worstSession),// 305
                               SafeInt(ft_handleSocGainReset.worstWindow),       // 306
                               SafeInt(ft_handleSocGainReset.worstSession),      // 307
                               SafeInt(ft_handleAltZeroReset.worstWindow),       // 308
                               SafeInt(ft_handleAltZeroReset.worstSession),      // 309
                               SafeInt(ft_calculateChargeTimes.worstWindow),     // 310
                               SafeInt(ft_calculateChargeTimes.worstSession),    // 311
                               SafeInt(ft_UpdateSailingMetrics.worstWindow),     // 312
                               SafeInt(ft_UpdateSailingMetrics.worstSession),    // 313
                               SafeInt(ft_updateWeatherMode.worstWindow),        // 314
                               SafeInt(ft_updateWeatherMode.worstSession),       // 315
                               SafeInt(ft_updateSensorWindow.worstWindow),       // 316
                               SafeInt(ft_updateSensorWindow.worstSession),      // 317
                               SafeInt(ft_checkTimeSync.worstWindow),            // 318
                               SafeInt(ft_checkTimeSync.worstSession),           // 319
                               // from CSV3 (firmware-computed)
                               SafeInt(currentRPMTableIndex),                    // 320
                               SafeInt(pidInitialized ? 1 : 0),                  // 321
                               SafeInt(pidSetpoint, 100),                        // 322
                               SafeInt(TempToUse),                               // 323
                               SafeInt(learningTargetFromRPM, 100),              // 324
                               SafeInt(ambientTempCorrection, 100),              // 325
                               SafeInt(finalLearningTarget, 100),                // 326
                               SafeInt(overheatingPenaltyTimer / 1000),          // 327
                               SafeInt(overheatingPenaltyAmps, 100),             // 328
                               SafeInt(averageTableValue, 100),                  // 329
                               SafeInt(timeSinceLastOverheat / 1000),            // 330
                               SafeInt(socInfoAvailable),                        // 331
                               SafeInt(overheatCount[0]),                        // 332
                               SafeInt(overheatCount[1]),                        // 333
                               SafeInt(overheatCount[2]),                        // 334
                               SafeInt(overheatCount[3]),                        // 335
                               SafeInt(overheatCount[4]),                        // 336
                               SafeInt(overheatCount[5]),                        // 337
                               SafeInt(overheatCount[6]),                        // 338
                               SafeInt(overheatCount[7]),                        // 339
                               SafeInt(overheatCount[8]),                        // 340
                               SafeInt(overheatCount[9]),                        // 341
                               SafeInt(cumulativeNoOverheatTime[0] / 1000),      // 342
                               SafeInt(cumulativeNoOverheatTime[1] / 1000),      // 343
                               SafeInt(cumulativeNoOverheatTime[2] / 1000),      // 344
                               SafeInt(cumulativeNoOverheatTime[3] / 1000),      // 345
                               SafeInt(cumulativeNoOverheatTime[4] / 1000),      // 346
                               SafeInt(cumulativeNoOverheatTime[5] / 1000),      // 347
                               SafeInt(cumulativeNoOverheatTime[6] / 1000),      // 348
                               SafeInt(cumulativeNoOverheatTime[7] / 1000),      // 349
                               SafeInt(cumulativeNoOverheatTime[8] / 1000),      // 350
                               SafeInt(cumulativeNoOverheatTime[9] / 1000),      // 351
                               SafeInt(learningUpCount[0]),                      // 352
                               SafeInt(learningUpCount[1]),                      // 353
                               SafeInt(learningUpCount[2]),                      // 354
                               SafeInt(learningUpCount[3]),                      // 355
                               SafeInt(learningUpCount[4]),                      // 356
                               SafeInt(learningUpCount[5]),                      // 357
                               SafeInt(learningUpCount[6]),                      // 358
                               SafeInt(learningUpCount[7]),                      // 359
                               SafeInt(learningUpCount[8]),                      // 360
                               SafeInt(learningUpCount[9]),                      // 361
                               SafeInt(totalLearningEvents),                     // 362
                               SafeInt(totalOverheats),                          // 363
                               SafeInt(totalSafeHours),                          // 364
                               SafeInt(FreeInternalRam),                         // 365
                               SafeInt(TotalInternalRam),                        // 366
                               SafeInt(LargestInternalBlock),                    // 367
                               SafeInt(FreePSRAM),                               // 368
                               SafeInt(TotalPSRAM),                              // 369
                               SafeInt(Heapfrag),                                // 370
                               SafeInt(ft_ReadAnalogInputs.worstWindow),         // 371
                               SafeInt(ft_ReadAnalogInputs.worstSession),        // 372
                               SafeInt(ft_AdjustFieldLearnMode.worstWindow),     // 373
                               SafeInt(ft_AdjustFieldLearnMode.worstSession),    // 374
                               SafeInt(ft_uploadSensorHistory.worstWindow),      // 375
                               SafeInt(ft_uploadSensorHistory.worstSession),     // 376
                               SafeInt(ft_uploadBufferedRecords.worstWindow),    // 377
                               SafeInt(ft_uploadBufferedRecords.worstSession),   // 378
                               SafeInt(ft_buildConfigPayload.worstWindow),       // 379
                               SafeInt(ft_buildConfigPayload.worstSession),      // 380
                               SafeInt(VeTime2),                                 // 381
                               (int)systemIDRiseDelay_ms[0],                     // 382
                               (int)systemIDRiseDelay_ms[1],                     // 383
                               (int)systemIDRiseDelay_ms[2],                     // 384
                               (int)systemIDFallDelay_ms[0],                     // 385
                               (int)systemIDFallDelay_ms[1],                     // 386
                               (int)systemIDFallDelay_ms[2],                     // 387
                               (int)systemIDRiseAvg_ms,                          // 388
                               (int)systemIDFallAvg_ms,                          // 389
                               (int)nvsPhase,                                    // 390
                               SafeInt(ft_saveNVSData.worstWindow),              // 391
                               SafeInt(ft_saveNVSData.worstSession),             // 392
                               SafeInt(ft_efficiencyTracker.worstWindow),        // 393
                               SafeInt(ft_efficiencyTracker.worstSession),       // 394
                               (int)systemIDActive,                              // 395
                               (int)systemIDResultsReady,                        // 396
                               (int)(systemIDStepAmp_A[0] * 10),                // 397
                               (int)(systemIDStepAmp_A[1] * 10),                // 398
                               (int)(systemIDStepAmp_A[2] * 10),                // 399
                               (int)(systemIDQuietPP_A[0] * 10),                // 400
                               (int)(systemIDQuietPP_A[1] * 10),                // 401
                               (int)(systemIDQuietPP_A[2] * 10),                // 402
                               SafeInt(nvsCycleMs),                              // 403 — ms elapsed for last complete NVS drain cycle
                               SafeInt(voltLoopWorstInterval_5s),                // 404
                               SafeInt(voltLoopWorstInterval_ses),               // 405
                               SafeInt(nvsCommitCount),                          // 406
                               SafeInt(nvsCommitLongCount),                      // 407
                               SafeInt(nvsCommitWorstMs),                        // 408
                               SafeInt(nvsCommitLastMs)                          // 409
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

  // PRIORITY 4: CSVData3 — sent immediately when settingsDirty (event-driven), or every 60s fallback
  if (!sentSomething && (settingsDirty || now - lastpayload3send >= 60000) && events.count() > 0) {
    static char *payload3 = nullptr;
    static const size_t PAYLOAD3_SIZE = 2400;  // (276 fields + 1) × 7 = 1939, rounded up to 2400
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
                               "%d,"  // CSV3_FIELD_COUNT
                               "%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,"
                               "%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,"
                               "%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,"
                               "%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,"
                               "%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,"
                               "%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,"
                               "%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,"
                               "%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,"
                               "%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,"
                               "%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,"
                               "%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,"
                               "%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,"
                               "%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,"
                               "%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,"
                               "%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,"
                               "%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,"
                               "%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,"
                               "%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,"
                               "%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,"
                               "%d,%d,%d,%d,%.3f,%.3f,%.3f,%d,%d,%d,"
                               "%d,%d,%d,%d,%d,%d,%d,%d,"
                               "%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,"
                               "%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,"
                               "%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,"
                               "%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,"
                               "%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,"
                               "%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,"
                               "%d,%d,%d,%d,%d,%d",

                               CSV3_FIELD_COUNT,
                               SafeInt(TemperatureLimitF),                       // 0
                               SafeInt(BulkVoltage, 100),                        // 1
                               SafeInt(wavePeriod),                              // 2
                               SafeInt(FloatVoltage, 100),                       // 3
                               SafeInt(SwitchingFrequency),                      // 4
                               SafeInt(yyMin),                                   // 5
                               SafeInt(FieldAdjustmentInterval),                 // 6
                               SafeInt(ManualDutyTarget),                        // 7
                               SafeInt(SwitchControlOverride),                   // 8
                               SafeInt(waveAmplitude),                           // 9
                               SafeInt(CurrentThreshold, 100),                   // 10
                               SafeInt(PeukertExponent_scaled),                  // 11
                               SafeInt(ChargeEfficiency_scaled),                 // 12
                               SafeInt(ChargedVoltage_Scaled),                   // 13
                               SafeInt(TailCurrent, 10),                         // 14  (× 10 so JS can show 1 decimal)
                               SafeInt(ChargedDetectionTime),                    // 15
                               SafeInt(IgnoreTemperature),                       // 16
                               SafeInt(bmsLogic),                                // 17
                               SafeInt(bmsLogicLevelOff),                        // 18
                               SafeInt(RPMScalingFactor),                        // 19
                               SafeInt(MaximumAllowedBatteryAmps),               // 20
                               SafeInt(BatteryVoltageSource),                    // 21
                               SafeInt(LearningUpwardEnabled),                   // 22
                               SafeInt(LearningDownwardEnabled),                 // 23
                               SafeInt(AlternatorNominalAmps),                   // 24
                               SafeInt(LearningUpStep, 100),                     // 25
                               SafeInt(LearningDownStep, 100),                   // 26
                               SafeInt(AmbientTempCorrectionFactor, 100),        // 27
                               SafeInt(xTime),                                   // 28
                               SafeInt(MinLearningInterval),                     // 29
                               SafeInt(SafeOperationThreshold),                  // 30
                               SafeInt(PidKp, 1000),                             // 31
                               SafeInt(PidKi, 1000),                             // 32
                               SafeInt(PidKd, 1000),                             // 33
                               SafeInt(PidSampleDivisor),                        // 34
                               SafeInt(MaxTableValue, 100),                      // 35
                               SafeInt(MaxPenaltyPercent, 100),                  // 36
                               SafeInt(MaxPenaltyDuration / 1000),               // 37
                               SafeInt(NeighborLearningFactor, 1000),            // 38
                               SafeInt(yyMax),                                   // 39
                               SafeInt(LearningMemoryDuration / 86400000),       // 40
                               SafeInt(EnableNeighborLearning),                  // 41
                               SafeInt(EnableAmbientCorrection),                 // 42
                               SafeInt(TuningMode),                              // 43
                               SafeInt(rpmCurrentTable[0]),                      // 44
                               SafeInt(rpmCurrentTable[1]),                      // 45
                               SafeInt(rpmCurrentTable[2]),                      // 46
                               SafeInt(rpmCurrentTable[3]),                      // 47
                               SafeInt(rpmCurrentTable[4]),                      // 48
                               SafeInt(rpmCurrentTable[5]),                      // 49
                               SafeInt(rpmCurrentTable[6]),                      // 50
                               SafeInt(rpmCurrentTable[7]),                      // 51
                               SafeInt(rpmCurrentTable[8]),                      // 52
                               SafeInt(rpmCurrentTable[9]),                      // 53
                               SafeInt(ShuntResistanceMicroOhm),                 // 54
                               SafeInt(InvertAltAmps),                           // 55
                               SafeInt(InvertBattAmps),                          // 56
                               SafeInt(MaxDuty),                                 // 57
                               SafeInt(MinDuty),                                 // 58
                               SafeInt(FieldResistance, 100),                    // 59
                               SafeInt(maxPoints),                               // 60
                               SafeInt(AlternatorCOffset, 100),                  // 61
                               SafeInt(BatteryCOffset, 100),                     // 62
                               SafeInt(BatteryCapacity_Ah),                      // 63
                               SafeInt(AmpSensorRange),                          // 64
                               SafeInt(R_fixed, 100),                            // 65
                               SafeInt(Beta, 100),                               // 66
                               SafeInt(T0_C, 100),                               // 67
                               SafeInt(TempSource),                              // 68
                               SafeInt(IgnitionOverride),                        // 69
                               SafeInt(FLOAT_DURATION),                          // 70
                               SafeInt(PulleyRatio, 100),                        // 71
                               SafeInt(BatteryCurrentSource),                    // 72
                               SafeInt(rpmTableRPMPoints[0]),                    // 73
                               SafeInt(rpmTableRPMPoints[1]),                    // 74
                               SafeInt(rpmTableRPMPoints[2]),                    // 75
                               SafeInt(rpmTableRPMPoints[3]),                    // 76
                               SafeInt(rpmTableRPMPoints[4]),                    // 77
                               SafeInt(rpmTableRPMPoints[5]),                    // 78
                               SafeInt(rpmTableRPMPoints[6]),                    // 79
                               SafeInt(rpmTableRPMPoints[7]),                    // 80
                               SafeInt(rpmTableRPMPoints[8]),                    // 81
                               SafeInt(rpmTableRPMPoints[9]),                    // 82
                               SafeInt(LearningSettlingPeriod),                  // 83
                               SafeInt(LearningRPMChangeThreshold),              // 84
                               SafeInt(LearningTempHysteresis),                  // 85
                               SafeInt(fuelTableRPM[0]),                         // 86
                               SafeInt(fuelTableRPM[1]),                         // 87
                               SafeInt(fuelTableRPM[2]),                         // 88
                               SafeInt(fuelTableRPM[3]),                         // 89
                               SafeInt(fuelTableRPM[4]),                         // 90
                               SafeInt(fuelTableRPM[5]),                         // 91
                               SafeInt(fuelTableRPM[6]),                         // 92
                               SafeInt(fuelTableRPM[7]),                         // 93
                               SafeInt(fuelTableRPM[8]),                         // 94
                               SafeInt(fuelTableRPM[9]),                         // 95
                               SafeInt(fuelTableGPH[0], 100),                    // 96
                               SafeInt(fuelTableGPH[1], 100),                    // 97
                               SafeInt(fuelTableGPH[2], 100),                    // 98
                               SafeInt(fuelTableGPH[3], 100),                    // 99
                               SafeInt(fuelTableGPH[4], 100),                    // 100
                               SafeInt(fuelTableGPH[5], 100),                    // 101
                               SafeInt(fuelTableGPH[6], 100),                    // 102
                               SafeInt(fuelTableGPH[7], 100),                    // 103
                               SafeInt(fuelTableGPH[8], 100),                    // 104
                               SafeInt(fuelTableGPH[9], 100),                    // 105
                               SafeInt(stateRevision),                           // 106
                               SafeInt(SetpointRampRate, 100),                   // 107
                               SafeInt(DutyRampRate, 100),                       // 108
                               SafeInt(SettleTimeBeforeCut),                     // 109
                               SafeInt(TempWarnExcess, 100),                     // 110
                               SafeInt(TempCritExcess, 100),                     // 111
                               SafeInt(TempSustainedTimeout / 1000),             // 112
                               SafeInt(AlternatorHardShutdownV, 100),            // 113
                               SafeInt(VoltageDisagreeThreshold, 100),           // 114
                               SafeInt(VoltageDisagreeTimeout / 1000),           // 115
                               SafeInt(rpmMinDutyTable[0], 100),                 // 116
                               SafeInt(rpmMinDutyTable[1], 100),                 // 117
                               SafeInt(rpmMinDutyTable[2], 100),                 // 118
                               SafeInt(rpmMinDutyTable[3], 100),                 // 119
                               SafeInt(rpmMinDutyTable[4], 100),                 // 120
                               SafeInt(rpmMinDutyTable[5], 100),                 // 121
                               SafeInt(rpmMinDutyTable[6], 100),                 // 122
                               SafeInt(rpmMinDutyTable[7], 100),                 // 123
                               SafeInt(rpmMinDutyTable[8], 100),                 // 124
                               SafeInt(rpmMinDutyTable[9], 100),                 // 125
                               SafeInt(rpmCapCurrentTable[0], 100),              // 126
                               SafeInt(rpmCapCurrentTable[1], 100),              // 127
                               SafeInt(rpmCapCurrentTable[2], 100),              // 128
                               SafeInt(rpmCapCurrentTable[3], 100),              // 129
                               SafeInt(rpmCapCurrentTable[4], 100),              // 130
                               SafeInt(rpmCapCurrentTable[5], 100),              // 131
                               SafeInt(rpmCapCurrentTable[6], 100),              // 132
                               SafeInt(rpmCapCurrentTable[7], 100),              // 133
                               SafeInt(rpmCapCurrentTable[8], 100),              // 134
                               SafeInt(rpmCapCurrentTable[9], 100),              // 135
                               SafeInt(VoltageKp, 100),                          // 136
                               SafeInt(VoltageLoopInterval),                     // 137
                               SafeInt(FIELD_COLLAPSE_DELAY),                    // 138
                               SafeInt(SetpointRiseRate, 100),                   // 139
                               SafeInt(SetpointFallRate, 100),                   // 140
                               SafeInt(PIDTrackingGain, 100),                    // 141
                               SafeInt(CAPSIZE_THRESHOLD_DEG),                   // 142
                               SafeInt(PITCHPOLE_THRESHOLD_DEG),                 // 143
                               SafeInt(SLAM_THRESHOLD_G, 10),                    // 144
                               SafeInt(imuMountOrientation),                     // 145
                               SafeInt(TailCurrent_A, 100),                      // 146
                               SafeInt(RebulkVoltage, 100),                      // 147
                               SafeInt(rebulkDebounceTime),                      // 148
                               SafeInt(MinFloatTime),                            // 149
                               SafeInt(SOC_BlockRebulk_percent),                 // 150
                               SafeInt(SOC_AllowRebulk_percent),                 // 151
                               SafeInt(accelEnabled),                            // 152
                               SafeInt(DutySlowRampRate, 100),                   // 153
                               SafeInt(ShutdownPhase2HoldMs),                    // 154
                               SafeInt(TempPIDKp, 1000),                         // 155
                               SafeInt(TempPIDKi, 1000),                         // 156
                               SafeInt(ThermalLookaheadSec),                     // 157
                               SafeInt(TempPIDIntervalMs),                       // 158
                               SafeInt(TempPIDFilterAlpha, 1000),                // 159
                               SafeInt(VoltageKi, 100),                          // 160
                               (int)rpmCapPowerTable[0],                         // 161
                               (int)rpmCapPowerTable[1],                         // 162
                               (int)rpmCapPowerTable[2],                         // 163
                               (int)rpmCapPowerTable[3],                         // 164
                               (int)rpmCapPowerTable[4],                         // 165
                               (int)rpmCapPowerTable[5],                         // 166
                               (int)rpmCapPowerTable[6],                         // 167
                               (int)rpmCapPowerTable[7],                         // 168
                               (int)rpmCapPowerTable[8],                         // 169
                               (int)rpmCapPowerTable[9],                         // 170
                               SafeInt(VoltageTrimLimit, 100),                   // 171
                               (int)InputFilterTC,                               // 172
                               (int)SystemIDStepAmplitude,                       // 173
                               SafeInt(HardOCTripAmps, 10),                      // 174 — ×10, 1 decimal
                               SafeInt(HardOCDebounceMs),                        // 175 — raw ms
                               SafeInt(IExcessK, 10),                            // 176 — ×10, 1 decimal
                               SafeInt(IExcessN),                                // 177 — raw int
                               SafeInt(IExcessKBleed, 100),                      // 178 — ×100, 2 decimals
                               SafeInt(IgnoreRPM),                               // 179
                               SafeInt(MinRPMForField),                          // 180
                               SafeInt(AwBleedRate, 10),                         // 181 — ×10, 1 decimal
                               SafeInt(AwRecoverRate, 10),                       // 182 — ×10, 1 decimal
                               SafeInt(KHard, 10),                               // 183 — ×10, 1 decimal
                               SafeInt(IExcessReseedFrac, 100),                  // 185 — ×100, 2 decimal
                               (int)AwSeedProtectMs,                             // 186
                               0,                                                 // 187 reserved — was VoltageKd (D term removed)
                               SafeInt(displayTempUnit),                         // 188
                               SafeInt(WarmupRampRate, 10),                      // 189 — ×10, 1 decimal
                               (int)OvGroup1Enable,                              // 188
                               (int)OvGroup2Enable,                              // 192
                               IExcessSigSrc,                                    // 193
                               IExcessMA_N,                                      // 194
                               OutputPIDSigSrc,                                  // 195
                               TdPred,                                           // 196 (%.3f)
                               OvMeasMarginV,                                    // 197 (%.3f)
                               OvPredMarginV,                                    // 198 (%.3f)
                               OutputPIDMA_N,                                    // 199
                               (int)OutputPIDFilterTC,                           // 200
                               (int)VoltageFilterTC,                             // 201
                               SafeInt(ProtectionProxGateV, 100),                // 202
                               SafeInt(SlopeBleedThresh, 100),                   // 203
                               (int)SlopeBleedK,                                 // 204
                               SafeInt(DvdtAlpha, 1000),                         // 205 — ×1000, 3 decimals
                               SafeInt(SlopeBleedProxV, 100),                    // 206 — ×100, 2 decimals
                               SafeInt(StartupRiseRate, 100),                    // 207 — ×100, 2 decimals
                               // from CSV2 (settings)
                               SafeInt(absorptionCompleteTime),                  // 208
                               SafeInt(OnOff),                                   // 209
                               SafeInt(ManualFieldToggle),                       // 210
                               SafeInt(HiLow),                                   // 211
                               SafeInt(LimpHome),                                // 212
                               SafeInt(AlarmActivate),                           // 213
                               SafeInt(TempAlarm),                               // 214
                               SafeInt(VoltageAlarmHigh),                        // 215
                               SafeInt(VoltageAlarmLow),                         // 216
                               SafeInt(CurrentAlarmHigh),                        // 217
                               SafeInt(AlarmTest),                               // 218
                               SafeInt(AlarmLatchEnabled),                       // 219
                               SafeInt(MaintainMode),                            // 220
                               SafeInt(ManualSOCPoint),                          // 221
                               SafeInt(LearningMode),                            // 222
                               SafeInt(LearningPaused),                          // 223
                               SafeInt(IgnoreLearningDuringPenalty),             // 224
                               SafeInt(ShowLearningDebugMessages),               // 225
                               SafeInt(LogAllLearningEvents),                    // 226
                               SafeInt(CloudFeatures),                           // 227
                               SafeInt(LearningDryRunMode),                      // 228
                               SafeInt(AutoShuntGainCorrection),                 // 229
                               SafeInt(AutoAltCurrentZero),                      // 230
                               SafeInt(WindingTempOffset),                       // 231
                               SafeInt(ManualLifePercentage),                    // 232
                               SafeInt(UVThresholdHigh, 100),                    // 233
                               SafeInt(weatherModeEnabled),                      // 234
                               SafeInt(SENSOR_UPLOAD_INTERVAL),                  // 235
                               SafeInt(imuEnabled ? 1 : 0),                      // 236
                               SafeInt(AbsorptionVoltage * 100),                 // 237
                               SafeInt(AbsorptionTimeoutMs),                     // 238
                               SafeInt(bulkVoltageHoldMs),                       // 239
                               SafeInt(capLimitMode),                            // 240
                               SafeInt(TargetVoltageMode),                       // 241
                               SafeInt(TargetVoltageSetpoint, 100),              // 242
                               SafeInt(RebulkCurrent_A, 100),                    // 243
                               SafeInt(UseFloat),                                // 244
                               SafeInt(anomalyMarginAmps, 10),                   // 245 — 1 decimal, divide by 10 in JS
                               SafeInt(anomalyAlarmThreshold),                   // 246
                               SafeInt(anomalyAlarmEnable),                      // 247
                               SafeInt(degradationThreshold, 100),               // 248
                               SafeInt(TempAlarmLow),                            // 249
                               SafeInt(LoadDumpDtThresh),                        // 250 — A/s tier-2 threshold (2 consecutive)
                               SafeInt(LoadDumpDtThresh1),                       // 251 — A/s tier-1 threshold (1 sample)
                               (int)CVTuningMode,                                // 252
                               SafeInt(cvWaveAmplitudeV, 100),                   // 253 — ×100, 2dp V
                               (int)cvWavePeriodSec,                             // 254
                               SafeInt(cvKOvershoot, 10),                        // 255 — ×10, 1dp
                               (int)cvConsecutiveReads,                          // 256
                               (int)ThermalTuningMode,                           // 257
                               SafeInt(thermalWaveLowF, 10),                     // 258 — ×10, 1dp °F
                               SafeInt(thermalWaveHighF, 10),                    // 259 — ×10, 1dp °F
                               SafeInt(thermalWaveHalfPeriodMin, 10),            // 260 — ×10, 1dp min
                               SafeInt(thermalKOvershoot, 100),                  // 261 — ×100, 2dp
                               SafeInt(thermalKUndershoot, 100),                 // 262 — ×100, 2dp
                               SafeInt(thermalSettleThreshF, 10),                // 263 — ×10, 1dp °F
                               (int)thermalConsecutiveReads,                     // 264
                               // from CSV1 (settings)
                               SafeInt(webgaugesinterval),                       // 265
                               SafeInt(plotTimeWindow),                          // 266
                               SafeInt(Ymin1),                                   // 267
                               SafeInt(Ymax1),                                   // 268
                               SafeInt(Ymin2, 100),                              // 269
                               SafeInt(Ymax2, 100),                              // 270
                               SafeInt(Ymin3),                                   // 271
                               SafeInt(Ymax3),                                   // 272
                               SafeInt(Ymin4),                                   // 273
                               SafeInt(Ymax4),                                   // 274
                               SafeInt(LoadDumpDtThresh3)                        // 275 — A/s tier-3 threshold (3 consecutive)
    );
    if (payload3Len < 0 || payload3Len >= PAYLOAD3_SIZE) {
      Serial.printf("payload3 truncated or format error: %d\n", payload3Len);
      return;
    }

    events.send(payload3, "CSVData3");
    settingsDirty = false;
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

// Called from Core 1 main loop via pendingSaveVesselInfo flag — not safe to call on Core 0
void saveVesselInfoToFile() {
  if (!fsMutex || xSemaphoreTake(fsMutex, pdMS_TO_TICKS(5000)) != pdTRUE) {
    Serial.println("saveVesselInfoToFile: mutex timeout");
    return;
  }
  DynamicJsonDocument doc(1024);
  doc["boat_length_ft"]         = BOAT_LENGTH_FT;
  doc["boat_type"]              = BOAT_TYPE;
  doc["boat_make_model"]        = BOAT_MAKE_MODEL;
  doc["boat_year"]              = BOAT_YEAR;
  doc["home_port"]              = HOME_PORT;
  doc["engine_make"]            = ENGINE_MAKE;
  doc["engine_hp"]              = ENGINE_HP;
  doc["battery_voltage"]        = BATTERY_VOLTAGE;
  doc["battery_capacity_ah"]    = BatteryCapacity_Ah;
  doc["battery_type"]           = BATTERY_TYPE;
  doc["alternator_brand_model"] = ALTERNATOR_BRAND_MODEL;
  doc["solar_watts"]            = SolarWatts;
  doc["imu_mount_orientation"]  = imuMountOrientation;
  doc["imu_dist_bow_ft"]        = IMU_DIST_BOW_FT;
  doc["imu_dist_cl_ft"]         = IMU_DIST_CL_FT;
  doc["imu_height_wl_ft"]       = IMU_HEIGHT_WL_FT;
  File file = LittleFS.open("/vessel_info.json", "w");
  if (file) {
    serializeJson(doc, file);
    file.close();
  }
  xSemaphoreGive(fsMutex);
}

// Called from Core 1 main loop via pendingClearVesselInfo flag — not safe to call on Core 0
void executeClearVesselInfo() {
  if (!fsMutex || xSemaphoreTake(fsMutex, pdMS_TO_TICKS(5000)) != pdTRUE) {
    Serial.println("executeClearVesselInfo: mutex timeout");
    return;
  }
  if (LittleFS.exists("/vessel_info.json")) {
    LittleFS.remove("/vessel_info.json");
  }
  xSemaphoreGive(fsMutex);
}
