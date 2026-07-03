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
  CSV1_MeasuredAmps_filtered,  // CC current-PID process variable — the OutputPIDSigSrc-selected signal (EMA/MA/raw), NOT the InputFilterTC display EMA (name kept for JS compatibility)
  CSV1_voltageTarget,
  CSV1_Icv,
  CSV1_WaterDepth_ft,  // NMEA2k depth ×3.28084 (meters → feet), scaled ×10 (0.1 ft); 0 if stale
  CSV1_Ignition,       // effective ignition state (real GPIO1 wire OR force-on override) — on CSV1 so the banner tracks it at ~10 Hz, not the 5 s CSV2 cadence
  CSV1_mExcessEma,       // iExcess detector: averaged signed current excess over command (A ×10) — tuning trace
  CSV1_iExcessThreshold, // iExcess detector: computed fire threshold E (A ×10) — tuning trace
  CSV1_mExcessEmaPeak,   // iExcess: per-CSV1-frame peak averaged excess (A ×10) — live sparkline
  CSV1_iExcessThreshMin, // iExcess: per-CSV1-frame min fire threshold E (A ×10) — live sparkline
  CSV1_protEventMask,    // protection-event bitmask this frame (1=OV 2=iExcess 4=LoadDump) — Plots-tab vertical markers

  CSV1_FIELD_COUNT  // = 41
};

enum Csv2Index {
  // DiagStream: slower-changing telemetry, diagnostics, computed values — sent every 5s
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
  CSV2_reserved_SolarWatts,        // moved to CSV3
  CSV2_reserved_performanceRatio,  // moved to CSV3
  CSV2_reserved_VeData,            // moved to CSV3
  CSV2_reserved_NMEA0183Data,      // moved to CSV3
  CSV2_reserved_NMEA2KData,        // moved to CSV3
  CSV2_AlarmLatchState,
  CSV2_ResetAlarmLatch,
  CSV2_reserved_ResetLearningTable,    // dead slot — command is action-only, no echo
  CSV2_reserved_ClearOverheatHistory,  // dead slot — command is action-only, no echo
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
  CSV2_reserved_Ignition,  // moved to CSV1 (banner needs ~10 Hz, not 5 s)
  CSV2_BulkStage,
  CSV2_WifiWakeSecondsRemaining,
  CSV2_BufferedRecordCount,
  CSV2_BufferedRecordPercent,
  CSV2_BufferedRecordCap,
  CSV2_VMGTargetBearing,
  CSV2_reserved_VMGUseTrueWind,  // 98 reserved — moved to CSV3
  CSV2_cpuLoadCore0,
  CSV2_cpuLoadCore0Max,
  CSV2_cpuLoadCore1,
  CSV2_cpuLoadCore1Max,
  CSV2_hasForcedUpdate,
  CSV2_forcedFwVersionInt,
  CSV2_forcedUpdateDeadline,
  CSV2_stateRevision,
  CSV2_reserved_hardwarePresent,  // 107 reserved — moved to CSV3
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
  CSV2_outerTermLookahead,  // look-ahead share of outerTermP (A ×100)
  CSV2_thermalSlopeFPerSec,
  CSV2_chargeStageDisplay,
  CSV2_voltageControlActive,
  CSV2_voltageError,
  CSV2_cv_I,
  CSV2_inIdleStage,
  CSV2_altBaselineFrozen,    // 1 = a cloud-fitted curve is held (else awaiting first fit)
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
  CSV2_reserved_cv_D,  // dead slot — CV loop has no D term
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
  CSV2_reserved_thermalTestPhase,  // dead slot, sends 0
  CSV2_ft_updateAccelMetrics_win,
  CSV2_ft_updateAccelMetrics_ses,
  // Diagnostics block
  CSV2_WifiStrength,
  CSV2_SendWifiTime,
  CSV2_AnalogReadTime,
  CSV2_VeTime,
  CSV2_MaximumLoopTime,
  CSV2_EngineCycles,
  CSV2_CurrentSessionDuration,
  CSV2_reserved_timeAxisModeChanging,  // moved to CSV3
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
  CSV2_ft_UpdateBatterySOC_win,
  CSV2_ft_UpdateBatterySOC_ses,
  CSV2_ft_updateSensorWindow_win,
  CSV2_ft_updateSensorWindow_ses,
  CSV2_ft_checkTimeSync_win,
  CSV2_ft_checkTimeSync_ses,
  // Field-off flash/NVS-flush timers (loop-direct; see ft_dumpLongTermRing rationale)
  CSV2_ft_zeroLogService_win,
  CSV2_ft_zeroLogService_ses,
  CSV2_ft_bhFlushCapNVS_win,
  CSV2_ft_bhFlushCapNVS_ses,
  CSV2_ft_kneeLearnService_win,
  CSV2_ft_kneeLearnService_ses,
  // Firmware-computed values block
  CSV2_currentRPMTableIndex,
  CSV2_pidInitialized,
  CSV2_pidSetpoint,
  CSV2_TempToUse,
  CSV2_learningTargetFromRPM,
  CSV2_ambientTempCorrection,
  CSV2_finalLearningTarget,
  CSV2_overheatingPenaltyTimer,
  CSV2_overheatingPenaltyAmps,
  CSV2_reserved_averageTableValue,  // dead slot, sends 0
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
  CSV2_ft_altHealth_win,
  CSV2_ft_altHealth_ses,
  CSV2_ft_altFold_win,
  CSV2_ft_altFold_ses,
  CSV2_ft_boatPerf_win,
  CSV2_ft_boatPerf_ses,
  CSV2_systemIDActive,
  CSV2_systemIDResultsReady,
  CSV2_systemIDStepAmp_0,
  CSV2_systemIDStepAmp_1,
  CSV2_systemIDStepAmp_2,
  CSV2_systemIDQuietPP_0,
  CSV2_systemIDQuietPP_1,
  CSV2_systemIDQuietPP_2,
  CSV2_systemIDAbortReason,       // FieldEventReason code if protection aborted last test; 0=no abort
  CSV2_systemIDAbortPhase,        // phase 1-9 at moment of protection abort; 0=no abort
  // CV voltage-loop firing-interval ladder (vl_*), CH1/pf-style stats
  CSV2_vl_last_ms,
  CSV2_vl_avg_10s,
  CSV2_vl_worst_10s,
  CSV2_vl_over2x_10s,
  CSV2_vl_avg_2m,
  CSV2_vl_worst_2m,
  CSV2_vl_over2x_2m,
  CSV2_vl_avg_at,
  CSV2_vl_worst_at,
  CSV2_vl_over2x_at,
  // NVS full-save diagnostics — saveNVSDataFull() fires only at field-off edge / shutdown / capsize.
  CSV2_nvsSecsSinceLastSave,      // seconds since last successful saveNVSDataFull() (0 = never saved this boot)
  CSV2_nvsFullSaveLastMs,         // wall-clock duration of most recent saveNVSDataFull() (ms)
  CSV2_nvsFullSaveWorstMs,        // worst saveNVSDataFull() duration since boot (ms)
  CSV2_nvsFullSaveCount,          // total saveNVSDataFull() calls since boot

  // Ignition-cycle watermarks (lo + hi pairs, reset every boot). See wmIgn_* globals in Xregulator.ino.
  CSV2_wmIgn_amps_lo,     CSV2_wmIgn_amps_hi,      // MeasuredAmps (A, int)
  CSV2_wmIgn_altTempF_lo, CSV2_wmIgn_altTempF_hi,  // AlternatorTemperatureF (°F, int)
  CSV2_wmIgn_IBV_lo,      CSV2_wmIgn_IBV_hi,       // INA228 battery V (×10, 1 decimal)
  CSV2_wmIgn_Bcur_lo,     CSV2_wmIgn_Bcur_hi,      // INA228 battery A (int)
  CSV2_wmIgn_SOC_lo,      CSV2_wmIgn_SOC_hi,       // SOC percent (0..100, int)
  CSV2_wmIgn_RPM_lo,      CSV2_wmIgn_RPM_hi,       // Engine RPM (int)
  CSV2_wmIgn_SOG_lo,      CSV2_wmIgn_SOG_hi,       // SOGNMEA knots (int)
  CSV2_wmIgn_AWS_lo,      CSV2_wmIgn_AWS_hi,       // ApparentWindSpeedNMEA knots (int)
  CSV2_wmIgn_TWS_lo,      CSV2_wmIgn_TWS_hi,       // TrueWindSpeedNMEA knots (int)
  CSV2_wmIgn_heel_lo,     CSV2_wmIgn_heel_hi,      // imu_heel_deg (int)
  CSV2_wmIgn_pitch_lo,    CSV2_wmIgn_pitch_hi,     // imu_pitch_deg (int)
  CSV2_wmIgn_vacc_lo,     CSV2_wmIgn_vacc_hi,      // imu_vertical_accel_g (×10, 1 decimal)
  CSV2_wmIgn_baro_lo,     CSV2_wmIgn_baro_hi,      // baroPressure mbar (int)
  CSV2_wmIgn_ambient_lo,  CSV2_wmIgn_ambient_hi,   // ambientTemp °F (int)
  CSV2_restartRemainingSec,                        // seconds until scheduled reboot (0 = outside 10-min warning window)
  CSV2_currentGpsSource,                           // 0=none, 1=NMEA, 2=Phone, 3=Manual (GpsSource enum)
  CSV2_currentTimeSource,                          // 0=none, 1=GPS, 2=Phone, 3=NTP, 4=drifting (TimeSource enum)
  CSV2_loggingActive,   // 1 if logging active, 0 if stopped (Stop/Start Logs)
  CSV2_sustainedTWS,                               // 2-min sustained true wind, knots ×10 (Beaufort + gale basis)
  CSV2_currentGaleMinutes,                         // live minutes continuously in a gale (sustained ≥34kt), int
  CSV2_wmIgn_VMGman_lo,   CSV2_wmIgn_VMGman_hi,    // VMG manual session min/max (knots ×10)
  CSV2_wmIgn_VMGup_lo,    CSV2_wmIgn_VMGup_hi,     // VMG upwind session min/max (knots ×10)

  // Alternator (charging-system) health summary
  CSV2_altHealthPct,        // worst-region performance % ×10
  CSV2_altHealthStatus,     // 0 insufficient/awaiting fit, 1 healthy, 2 drifting
  CSV2_altCoveragePct,      // record-book fill % ×10
  CSV2_altObsCount,         // banked best-ever record count

  // IMU zero/level calibration echo (Phase 2 IMU zero button)
  CSV2_reserved_imuHeelOffset,   // moved to CSV3 (settings echo) so the Level Zero label updates fast
  CSV2_reserved_imuPitchOffset,  // moved to CSV3

  // Victron VE.Direct solar/MPPT live block (7 fields — PPV/VPV/derived-current moved to CSV4/NavStream)
  CSV2_VictronChargeState,       // CS code (×1)
  CSV2_VictronMPPTMode,          // MPPT tracker code (×1)
  CSV2_VictronError,             // ERR code (×1)
  CSV2_VictronYieldToday,        // H20 yield today (kWh ×100)
  CSV2_VictronMaxPowerToday,     // H21 max power today (W ×1)
  CSV2_VictronYieldYesterday,    // H22 yield yesterday (kWh ×100)
  CSV2_VictronMaxPowerYesterday, // H23 max power yesterday (W ×1)

  // (live engine fuel flow + economy — currentFuelGPH/currentNMPG — moved to CSV4/NavStream)

  // Session fuel-economy curve: mpg per 250-RPM bin (18 bins, 0..4500), naut mi/gal ×100, 0 = empty
  CSV2_fuelCurveNMPG_0,  CSV2_fuelCurveNMPG_1,  CSV2_fuelCurveNMPG_2,
  CSV2_fuelCurveNMPG_3,  CSV2_fuelCurveNMPG_4,  CSV2_fuelCurveNMPG_5,
  CSV2_fuelCurveNMPG_6,  CSV2_fuelCurveNMPG_7,  CSV2_fuelCurveNMPG_8,
  CSV2_fuelCurveNMPG_9,  CSV2_fuelCurveNMPG_10, CSV2_fuelCurveNMPG_11,
  CSV2_fuelCurveNMPG_12, CSV2_fuelCurveNMPG_13, CSV2_fuelCurveNMPG_14,
  CSV2_fuelCurveNMPG_15, CSV2_fuelCurveNMPG_16, CSV2_fuelCurveNMPG_17,
  CSV2_fuelCurveTopRPM,          // top configured fuel-table RPM -> chart x-axis scale (×1)

  // 80MHz low-power loop instrumentation (4 fields) — health while engine off / CPU throttled
  CSV2_loopWorst80Win_ms,        // worst 80MHz loop pass, rolling 5s (ms ×1)
  CSV2_loopWorst80Ses_ms,        // worst 80MHz loop pass since Reset Peak Values (ms ×1)
  CSV2_loopOver80ImuLimitCount,  // # 80MHz passes over accel FIFO drain limit (~38ms) since reset
  CSV2_loop80IterCount,          // total 80MHz passes since reset (denominator)
  // Field-ON loop instrumentation (2 fields) — worst pass while actually regulating (gate latched
  // at top of pass); splits control-path stalls from intentional field-off background work
  CSV2_loopFieldOnWin_ms,        // worst field-ON loop pass, rolling 5s (ms ×1)
  CSV2_loopFieldOnSes_ms,        // worst field-ON loop pass since Reset Peak Values (ms ×1)
  // Thermal tuning plot live-stream fields
  CSV2_tempFiltered,             // IIR-filtered alt temp (°F ×100); distinct from raw AlternatorTemperatureF, used as PID base
  CSV2_outerImpliedPenalty,      // voltage cap expressed as a downstream amps penalty (A ×100); Plot 2 "Implied Penalty"
  CSV2_thermalFlags,             // state-strip bitfield: bit0 tempPIDActive, bit4 AUTO, bit5 shutdown
  CSV2_thermalAntiWindupLatch,   // 1 = CV-bleed anti-windup fired since last CSV2 send (latched; JS draws red ticks)

  // Inner Current PID firing interval (field-on-gated), CH1-style stats (avg ×100)
  CSV2_pf_last_ms,
  CSV2_pf_avg_10s,
  CSV2_pf_worst_10s,
  CSV2_pf_over2x_10s,
  CSV2_pf_avg_2m,
  CSV2_pf_worst_2m,
  CSV2_pf_over2x_2m,
  CSV2_pf_avg_at,
  CSV2_pf_worst_at,
  CSV2_pf_over2x_at,

  // I2C bus-health — bus-only timing isolates a true bus stall from loop preemption
  CSV2_inaBusReadWorstUs,    // worst µs in the two INA228 Wire reads (vs whole-block ft_rai_ina228)
  CSV2_inaBusSlowCount,      // INA228 bus reads > 15 ms since reset
  CSV2_ina228ErrorCount,     // INA228 reads dropped (sanity fail / exception)
  CSV2_imuFifoFetchWorstUs,  // worst µs in Get_FIFO_Sample
  CSV2_imuFifoWorstSamples,  // sample count of that worst fetch — small count + big µs = stall/preemption, not transfer size

  // Long-term-ring flash-flush timer (field-off 15-min dump)
  CSV2_dumpLongTermRing_win,  // worst µs of the flush, rolling 5s window
  CSV2_dumpLongTermRing_ses,  // worst µs of the flush since last Reset Peak Values

  // Fast alternator-current channel (GPIO3) — timers, status, detector, session worsts
  CSV2_fastAltDrain_win,    // worst µs of the bounded DMA drain, rolling 5s window
  CSV2_fastAltDrain_ses,    // ...since last Reset Peak Values
  CSV2_faMatrixFlush_win,   // worst µs of the disturbance-matrix/flipbook flash flush, rolling 5s window
  CSV2_faMatrixFlush_ses,   // ...since last Reset Peak Values
  CSV2_faDetector_win,      // detector whole-analysis compute on the Core-0 worker — LAST run (µs); JS /1000 -> ms
  CSV2_faDetector_ses,      // detector whole-analysis compute — WORST since last Reset Peak Values (µs)
  CSV2_faWindowFinalize_win, // worst µs of one per-2s-window finalize (Goertzel/matrix-fold/detector-arm), rolling 5s
  CSV2_faWindowFinalize_ses, // ...since last Reset Peak Values
  CSV2_faChanState,         // 0 = off, 1 = sampling, 2 = railed/dormant (jumper open)
  CSV2_faCellsUsed,         // disturbance-matrix cells with ≥1 qualified window
  CSV2_faDetectK,           // failure detector: fault class of the last FAULT verdict, 0 = quiet
  CSV2_faSesPkpkWorst,      // session-worst broadband pk-pk, A ×100
  CSV2_faSesPeakWorst,      // session-worst spectral peak, A ×100
  CSV2_faSesPeakWorstHz,    // ...its frequency, Hz ×10
  CSV2_faAnomalyCount,      // lifetime detector FAULT-verdict count (persisted fleet scalar)
  CSV2_faDomFreqHz,         // Highest Tone in Map: frequency, Hz ×10
  CSV2_faDomAmp,            // ...amplitude, A ×100
  CSV2_faDomRpm,            // ...RPM (bin center) where it occurs
  CSV2_faDomAmps,           // ...window-mean output current there, A ×10
  CSV2_faDomTempF,          // ...alternator temperature there, °F ×10 (-1 = no probe)
  CSV2_faDomEpoch,          // ...wall-clock epoch when set (0 = clock not synced)
  CSV2_faSesPkpkRpm,        // Session Worst Pk-Pk: RPM (bin center) where it occurred
  CSV2_faSesPkpkAmps,       // ...window-mean output current there, A ×10
  CSV2_faSesPkpkTempF,      // ...alternator temperature there, °F ×10 (-1 = no probe)
  CSV2_faSesPkpkEpoch,      // ...wall-clock epoch when set (0 = clock not synced)

  // gate-tuning 10s live readouts (firmware Roll10s extreme; ROLL_EMPTY sentinel when no sample in window)
  CSV2_faRpmEdge10sMin,     // RPM edge margin, 10s trough (RPM ×10)
  CSV2_faAmpsDrift10sMax,   // amps-drift EMA spread, 10s peak (A ×100)
  CSV2_faAmpsDriftExc10sMax,// drift spread minus its effective gate limit, 10s peak (A ×100); <=0 = drift gate passing
  CSV2_faTonePk10sMax,      // largest spectral peak, 10s peak (A ×100)
  CSV2_ldSlew10sMax,        // current slew g_dBcur_dt, 10s peak (A/s ×10)
  CSV2_cvSlope10sMax,       // voltage rise cvDSlope, 10s peak (V/s ×10000)
  // ripple-capture (faFiltRippleUpdate §11 stationarity) admission gates — window quantity minus its limit, 10s peak; <=0 = that gate passing
  CSV2_ripCmdExc10sMax,     // setpointLimited travel − amplitude limit (A ×100)
  CSV2_ripAltExc10sMax,     // alternator half-window mean-shift − stationarity limit (A ×100)
  CSV2_ripBattExc10sMax,    // battery half-window mean-shift − stationarity limit (A ×100)
  CSV2_ripRpmShift10sMax,   // RPM half-window mean-shift − stationarity limit, 10s peak (RPM ×10); <=0 = passing
  CSV2_ripAltAdmitCount,    // windows that passed ALL alt-fold gates (lifetime; UI diffs frames for rate)
  CSV2_ripBattAdmitCount,   // same for the battery detector

  // Lifetime nav/sailing records (so the Lifetime Statistics panel can show + individually reset
  // them). Persisted in NVS + uploaded to the leaderboards.
  CSV2_LongestTripAT,       // longest single trip, nm ×10
  CSV2_Max24hrDistAT,       // max 24-hour distance, nm ×10
  CSV2_DeepestAnchorAT,     // deepest anchorage, ft ×10
  CSV2_BestUpwindVmgAT,     // best upwind VMG, kts ×100
  CSV2_LongestGaleAT,       // longest gale duration, hours ×100

  // Control Accuracy Scores (accumulate-since-reset): RMS error + worst overshoot per loop.
  // Current in A ×100, voltage in mV (V ×1000), thermal in °F ×100.
  CSV2_accCurRms,    // inner current loop RMS error (A ×100)
  CSV2_accCurPeak,   // inner current loop worst over-current (A ×100)
  CSV2_accVoltRms,   // CV loop RMS error (mV)
  CSV2_accVoltPeak,  // CV loop worst over-voltage (mV)
  CSV2_accThermRms,  // thermal loop RMS error (°F ×100)
  CSV2_accThermPeak, // thermal loop worst over-temp (°F ×100)

  CSV2_cpuFreqMhz,   // live CPU clock (80 or 240 MHz) — 80 = engine-off low-power throttle, 240 = full speed

  // ADS slow-channel inter-sample gap meters (ms). last = most recent gap between valid
  // readings, worst = max since Reset Peak Values. Validate the continuous-mode excursion
  // scheme keeps CH0/CH2 under their <30ms targets. Dashboard: Live Data → ESP32.
  CSV2_ch0GapLast,   // CH0 battery-voltage read gap, most recent (ms)
  CSV2_ch0GapWorst,  // CH0 battery-voltage read gap, worst since reset (ms)
  CSV2_ch2GapLast,   // CH2 RPM read gap, most recent (ms)
  CSV2_ch2GapWorst,  // CH2 RPM read gap, worst since reset (ms)

  // CSV2 send-cost breakdown (µs): build (snprintf) vs send (events.send). For deciding
  // whether to chunk the build or the send to shrink the WiFi-send spike.
  CSV2_csv2BuildLast,   // CSV2 build (snprintf) time, previous cycle (µs)
  CSV2_csv2BuildWorst,  // CSV2 build time, worst since reset (µs)
  CSV2_csv2SendLast,    // CSV2 send (events.send) time, previous cycle (µs)
  CSV2_csv2SendWorst,   // CSV2 send time, worst since reset (µs)

  // Core-0 HTTPS task — cloud op wall-clock time (ms). LAST op + WORST since Reset Peak Values.
  CSV2_httpsUpload_win, // Core-0 cloud op time, LAST (ms)  [_win = LAST, matching the faDetector row]
  CSV2_httpsUpload_ses, // Core-0 cloud op time, WORST since last Reset Peak Values (ms)

  CSV2_cvTempDerateScale, // live battery-temp gain derate multiplier on the active CV gains; ×1000

  CSV2_FIELD_COUNT // enum position is authoritative — never hand-count; CSV payload specifier count must equal this +1
};

enum Csv4Index {
  // NavStream: live nav / wind / solar / fuel readouts — sent every 500 ms (2 Hz).
  // Sits between CSV1 (10 Hz control-loop) and CSV2 (5 s status); the dial/compass/speed/solar/fuel gauges need ~2 Hz.
  CSV4_HeadingNMEA,             // heading (deg, int)
  CSV4_SOGNMEA,                 // speed over ground (knots ×100)
  CSV4_COGNMEA,                 // course over ground (deg, int)
  CSV4_STWNMEA,                 // speed through water (SOW, knots ×100; NAN/no-log -> 0)
  CSV4_ApparentWindSpeedNMEA,   // AWS (knots ×100)
  CSV4_ApparentWindAngleNMEA,   // AWA (deg, int)
  CSV4_TrueWindSpeedNMEA,       // TWS (knots ×100)
  CSV4_TrueWindAngleNMEA,       // TWA (deg, int)
  CSV4_LeewayNMEA,              // leeway (deg, int)
  CSV4_VMGNMEA,                 // VMG (knots ×100)
  CSV4_VMGUpwind,               // VMG to windward = SOG·cos(TWA), knots ×100
  CSV4_VictronSolarPower,       // PPV panel power (W ×1)
  CSV4_VictronSolarVoltage,     // VPV panel voltage (V ×100)
  CSV4_VictronSolarCurrent,     // derived panel current (A ×100)
  CSV4_VictronCurrent,          // Victron battery current (A ×100)
  CSV4_currentFuelGPH,          // live fuel flow (gal/hr ×100)
  CSV4_currentNMPG,             // live fuel economy (naut mi/gal ×100)
  CSV4_FIELD_COUNT  // = 17
};

enum Csv3Index {
  // SettingsStream: user-configurable settings — sent on change (settingsDirty) or every 60s fallback
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
  CSV3_reserved_BatteryVoltageSource,  // dead slot, sends 0
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
  CSV3_EnableAmbientCorrection,
  CSV3_TuningMode,
  CSV3_reserved_rpmCurrentTable_0,  // dead slots (0-9), send 0
  CSV3_reserved_rpmCurrentTable_1,
  CSV3_reserved_rpmCurrentTable_2,
  CSV3_reserved_rpmCurrentTable_3,
  CSV3_reserved_rpmCurrentTable_4,
  CSV3_reserved_rpmCurrentTable_5,
  CSV3_reserved_rpmCurrentTable_6,
  CSV3_reserved_rpmCurrentTable_7,
  CSV3_reserved_rpmCurrentTable_8,
  CSV3_reserved_rpmCurrentTable_9,
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
  CSV3_reserved_SetpointRampRate,  // dead slot, sends 0
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
  CSV3_SetpointBigStepThresh,
  CSV3_SetpointBigStepRiseRate,
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
  CSV3_reserved_accelEnabled,  // dead slot — accelerometer is always-on, no UI toggle
  CSV3_DutySlowRampRate,
  CSV3_ShutdownPhase2HoldMs,
  CSV3_TempPIDKp,
  CSV3_TempPIDKi,
  CSV3_ThermalLookaheadSec,
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
  CSV3_reserved_VoltageTrimLimit,  // dead slot, sends 0
  CSV3_InputFilterTC,
  CSV3_SystemIDStepAmplitude,
  CSV3_HardOCTripAmps,
  CSV3_HardOCDebounceMs,
  CSV3_IExcessFrac,    // CV threshold fraction (×1000)
  CSV3_IExcessFloorA,  // threshold floor (A ×10)
  CSV3_IExcessKBleed,
  CSV3_IgnoreRPM,
  CSV3_MinRPMForField,
  CSV3_AwBleedRate,
  CSV3_reserved_AwRecoverRate,  // dead slot — rate is hardcoded to 0.1 in firmware; free for future use
  CSV3_KHard,
  CSV3_ReseedFrac,
  CSV3_AwSeedProtectMs,
  CSV3_reserved_VoltageKd,  // dead slot — voltage loop has no D term
  CSV3_displayTempUnit,
  CSV3_WarmupRampRate,
  CSV3_OvGroup1Enable,
  CSV3_OvGroup2Enable,
  CSV3_IExcessCeilA,   // threshold ceiling (A ×10)
  CSV3_IExcessTau,     // EMA time constant (ms, raw int)
  CSV3_OutputPIDSigSrc,
  CSV3_TdPred,          // %.3f
  CSV3_OvMeasMarginV,   // %.3f
  CSV3_OvPredMarginV,   // %.3f
  CSV3_OutputPIDMA_N,
  CSV3_OutputPIDFilterTC,
  CSV3_VoltageFilterTC,
  CSV3_reserved_ProtectionProxGateV,  // dead slot
  CSV3_SlopeBleedThresh,
  CSV3_SlopeBleedK,
  CSV3_DvdtTC,
  CSV3_SlopeBleedProxV,
  CSV3_StartupRiseRate,
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
  CSV3_IgnoreLearningDuringPenalty,
  CSV3_LogAllLearningEvents,
  CSV3_CloudFeatures,
  CSV3_AutoShuntGainCorrection,
  CSV3_AutoAltCurrentZero,
  CSV3_WindingTempOffset,
  CSV3_ManualLifePercentage,
  CSV3_UVThresholdHigh,
  CSV3_weatherModeEnabled,
  CSV3_reserved_SENSOR_UPLOAD_INTERVAL,  // dead slot — SENSOR_UPLOAD_INTERVAL is a firmware-only constant (edit + reflash)
  CSV3_imuEnabled,
  CSV3_AbsorptionVoltage,
  CSV3_AbsorptionTimeoutMs,
  CSV3_bulkVoltageHoldMs,
  CSV3_capLimitMode,
  CSV3_TargetVoltageMode,
  CSV3_TargetVoltageSetpoint,
  CSV3_RebulkCurrent_A,
  CSV3_UseFloat,
  CSV3_IExcessFracBulk,  // BULK threshold fraction (×1000)
  CSV3_IExcessRelFrac,   // release hysteresis fraction (×1000)
  CSV3_systemIDPlantTauMs,   // fitted plant time constant (ms), persisted
  CSV3_altSpare3,   // reserved
  CSV3_TempAlarmLow,
  CSV3_LoadDumpDtThresh,
  CSV3_LoadDumpDtThresh1,
  CSV3_CVTuningMode,
  CSV3_cvWaveAmplitudeV,
  CSV3_cvWavePeriodSec,
  CSV3_cvKOvershoot,
  CSV3_cvConsecutiveReads,
  // 8 dead slots — kept to preserve CSV3 indices, all send 0
  CSV3_reserved_thermal0,
  CSV3_reserved_thermal1,
  CSV3_reserved_thermal2,
  CSV3_reserved_thermal3,
  CSV3_reserved_thermal4,
  CSV3_reserved_thermal5,
  CSV3_reserved_thermal6,
  CSV3_reserved_thermal7,
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
  CSV3_reserved_VMGUseTrueWind,   // dead slot, sends 0
  CSV3_hardwarePresent,
  CSV3_testProtectionsEnabled,  // runtime flag — not persisted, resets true (enabled) on boot
  CSV3_IExcessArmMarginV,       // %.3f — iExcess voltage gate, independent of OvMeasMarginV
  CSV3_FastSetpointRiseRate,    // ×100, 1 decimal — multiplier on setpoint rise slew during post-protection recovery
  CSV3_FastSetpointRiseWindowMs, // raw ms — hard upper bound on fast-rise window
  CSV3_FastSetpointRiseHeadroomV, // ×100, 2 decimal — V below target at which fast-rise gate stays open
  CSV3_SolarWatts,
  CSV3_performanceRatio,        // ×100, 2 decimal
  CSV3_VeData,                  // 0/1
  CSV3_NMEA0183Data,            // 0/1
  CSV3_NMEA2KData,              // 0/1
  CSV3_timeAxisModeChanging,    // 0/1
  CSV3_gpsTimeSourceMode,       // 0=auto, 1=NMEA-forced, 2=Phone-forced, 3=NTP-time-forced
  // Fast alt-current diagnostic knobs (Pattern B echo)
  CSV3_faEnabled,               // 0/1 — global ON/OFF
  CSV3_faAlarmEnable,           // 0/1 — FAULT drives audible alarm
  CSV3_faAnomPause,             // 0/1 — freeze anomaly flipbook slots
  CSV3_faRpmEdgeMargin,         // RPM ×10
  CSV3_faAmpsDriftFloorA,       // A ×100
  CSV3_faAmpsDriftPct,          // percent ×10
  CSV3_faAttenUpAmps,           // A ×10
  CSV3_faAttenDownAmps,         // A ×10
  CSV3_faPeakMinA,              // A ×100
  CSV3_wifiNapEnabled,          // 0/1 — WiFi Napping standby toggle (Client only)
  CSV3_imuHeelOffset,           // captured rest heel offset (deg ×100); on CSV3 so the Level Zero echo is fast
  CSV3_imuPitchOffset,          // captured rest pitch offset (deg ×100)
  CSV3_systemIDTestType,        // 0=step, 1=sine sweep (Plant Delay test type)
  CSV3_systemIDSineFreqStart,   // Hz ×10
  CSV3_systemIDSineFreqEnd,     // Hz ×10
  CSV3_systemIDSineCycles,      // analysed cycles per sweep frequency
  CSV3_tuningWaveform,          // 0=square, 1=sine manual, 2=sine auto-sweep
  CSV3_tuningSineFreq,          // Hz ×10 (manual sine frequency)
  CSV3_tuningSweepStart,        // Hz ×10
  CSV3_tuningSweepEnd,          // Hz ×10
  CSV3_tuningSweepCycles,       // analysed cycles per sweep frequency
  CSV3_SystemIDStabilizeAmps,   // A ×10 — plant-delay baseline/trough current
  CSV3_tuningWaveFloor,         // A — Current Target Generator wave floor (trough), shared square + sine
  CSV3_commissionState,         // auto-commissioning state: 0=not, 1=in-progress, 2=commissioned
  CSV3_commissionPhase,         // furthest wizard phase reached: 0=Prep…6=CV plant fit, 7=Min% floor, 8=finished
  CSV3_commissionDoneMask,      // per-stage completion bitmask (bit i = stage i done)
  CSV3_cvHelpersEnabled,        // master switch: asymmetric KiDown unwind + slope-aware integrator bleed (1=on)
  CSV3_MinChargeTempF,          // cold-charge lockout board-temp floor (°F)
  CSV3_coldChargeLockoutEnable, // cold-charge lockout master on/off (1=on)
  CSV3_cvGainMode,              // CV gain mode: 0=Manual, 1=Auto (lambda-based)
  CSV3_EXTRA1,                  // reserved placeholder — sends 0
  CSV3_cvPlantK,                // measured plant gain K (V/A); ×10000
  CSV3_cvPlantTau,              // measured rise time tau (s); ×100
  CSV3_cvPlantL,                // measured dead time L (s); ×100
  CSV3_cvComputedKp,            // Auto-computed Kp (12V-equiv); ×100
  CSV3_cvComputedKi,            // Auto-computed Ki (12V-equiv); ×100
  CSV3_cvCrossover,             // CV crossover ω_c (rad/s); ×100
  CSV3_cvPiZero,                // CV PI integral zero ρ (rad/s); ×100
  CSV3_vTgtRampUp,              // CV voltage-target ramp UP rate (V/s); ×1000
  CSV3_vTgtRampDn,              // CV voltage-target ramp DOWN rate (V/s); ×1000
  CSV3_vTgtRampEnable,          // CV voltage-target slew master switch (0/1)
  CSV3_setpointSlewEnable,      // inner-loop current setpoint slew master switch (0/1)
  CSV3_cvRiseGovEnable,         // CV rise governor / anti-windup master switch (0/1)
  CSV3_dutySlewEnable,          // field duty slew master switch (0/1)
  CSV3_CommissionTempF,         // board temp when CV fit applied — derate reference (°F ×10; -32768 = unset/NaN)
  CSV3_battTempDerateEnable,    // battery-temp gain derate master on/off (0/1)
  CSV3_battTempCoeff,           // battery fractional resistance change per °C; ×10000
  CSV3_TempPIDKiDownFrac,       // thermal velocity-form below-setpoint integral bleed ratio (×Ki); ×1000
  CSV3_ThermalSlopeWindowSec,   // thermal slope backward-difference window (s); integer
  CSV3_BattCurrentLimitA,       // max battery charge current (A ×10, G4); 0 = disabled — ceiling on the alternator command = limit + house-load offset
  // measured-ripple capture admission gates (§10.8/§11) — own knobs, decoupled from the fa* detector gates
  CSV3_ripWinMs,                // pk-pk capture window (ms, integer)
  CSV3_ripDriftFloorA,          // shared floor: command-travel gate + stationarity mean-shift tolerance (A ×100)
  CSV3_ripDriftPct,             // command-travel gate slope (% of mean, ×10) — command gate only since §11
  CSV3_SocAlarmLow,             // low-SoC alarm threshold (%, integer); 0 = disabled

  CSV3_FIELD_COUNT  // enum position is authoritative — never hand-count; CSV payload specifier count must equal this +1
};


enum TsIndex {
  TS_HeadingNMEA,
  TS_LatitudeNMEA,
  TS_LongitudeNMEA,
  TS_SatelliteCount,
  TS_VictronVoltage,
  TS_VictronCurrent,
  TS_AlternatorTemp,
  TS_ThermistorTemp,
  TS_RPM,
  TS_MeasuredAmps,
  TS_BatteryV,
  TS_IBV,
  TS_Bcur,
  TS_Channel3V,
  TS_DutyCycle,
  TS_FieldVolts,
  TS_FieldAmps,
  TS_CogNMEA,
  TS_SogNMEA,
  TS_AppWindSpeed,
  TS_AppWindAngle,
  TS_TrueWindSpeed,
  TS_TrueWindAngle,
  TS_Leeway,
  TS_VMG,
  TS_BaroPressure,
  TS_AmbientTemp,
  TS_IMU,
  TS_VictronSolar,  // VE.Direct solar (PPV/VPV) staleness
  TS_StwNMEA,       // Speed Through Water (SOW, PGN 128259) staleness

  TS_FIELD_COUNT  // = 30
};


// Cap current table functions
float getCapCurrentForRPM(float rpm);
void saveCapCurrentTableToNVS();
void loadCapCurrentTableFromNVS();
void loadCapTablesForMode(int mode);

int SafeInt(double f, int scale = 1) {
  // where this is matters, don't move!!
  // Param widened to double so AllTime accumulators don't narrow at call sites; float callers promote implicitly.
  return isnan(f) || isinf(f) ? -1 : (int)round(f * scale);
}
void loadAPCredentials(bool forceDefaults = false) {
  if (forceDefaults) {
    esp32_ap_ssid = "ALTERNATOR_WIFI";
    esp32_ap_password = "alternator123";
    Serial.println("Using default AP credentials for password recovery or first boot");
    return;
  }

  // Load custom credentials from NVS
  if (settingExists(NK_apssid)) {
    esp32_ap_ssid = settingRead(NK_apssid);
    esp32_ap_ssid.trim();
    if (esp32_ap_ssid.length() == 0) {
      esp32_ap_ssid = "ALTERNATOR_WIFI";
    }
  } else {
    esp32_ap_ssid = "ALTERNATOR_WIFI";
  }

  if (settingExists(NK_appass)) {
    esp32_ap_password = settingRead(NK_appass);
    esp32_ap_password.trim();
    if (esp32_ap_password.length() == 0) {
      esp32_ap_password = "alternator123";
    }
  } else {
    esp32_ap_password = "alternator123";
  }

  Serial.println("Loaded AP credentials: " + esp32_ap_ssid + " / [" + String(esp32_ap_password.length()) + " chars]");
}

void setupWiFi() {
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
  bool isFirstBoot = !settingExists(NK_first_config_done);

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
  bool hasClientConfig = settingExists(NK_ssid);

  // Load and cache WiFi client credentials once
  if (hasClientConfig) {
    String ssid = settingRead(NK_ssid);
    String pass = settingRead(NK_pass);
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
    Serial.println("ERROR: No SSID provided for WiFi connection");  
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
  const int maxAttempts = timeout / 100;  // Check every 100ms — detects connect up to ~400ms sooner on wake

  while (WiFi.status() != WL_CONNECTED && attempts < maxAttempts) {
    delay(100);
    esp_task_wdt_reset();
    attempts++;

    // Print progress every 5 seconds — a normal ~2s connect stays silent; only slow/stuck connects log
    if (attempts % 50 == 0) {
      Serial.printf("WiFi Status: %d, attempt %d/%d\n", WiFi.status(), attempts, maxAttempts);
    }
  }

  if (WiFi.status() == WL_CONNECTED) {

    Serial.println("WiFi connection successful!");
    Serial.printf("IP address: %s\n", WiFi.localIP().toString().c_str());
    Serial.printf("Signal strength: %d dBm\n", WiFi.RSSI());

    // mDNS setup — start once and never MDNS.end() on reconnect: ESP32 core 3.3.8's mdns
    // teardown null-derefs the netif (LoadProhibited crash on wake/reconnect). mDNS stays bound
    // to the persistent STA netif across reconnects, so it keeps working without a restart.
    static bool mdnsStarted = false;
    if (!mdnsStarted && MDNS.begin("alternator")) {
      Serial.println("mDNS responder started");
      MDNS.addService("http", "tcp", 80);
      mdnsStarted = true;
    }

    return true;

  } else {

    Serial.printf("WiFi connection failed after %lu ms\n", timeout);
    Serial.printf("Final status: %d\n", WiFi.status());
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

    // Start mDNS in AP mode too (best-effort for alternator.local).
    // No MDNS.end() — core 3.3.8 mdns teardown null-derefs the netif (see client path); start once.
    static bool apMdnsStarted = false;
    if (!apMdnsStarted && MDNS.begin("alternator")) {
      MDNS.addService("http", "tcp", 80);
      apMdnsStarted = true;
      Serial.println("mDNS started - alternator.local available (may not work on all devices in AP mode)");
    } else if (!apMdnsStarted) {
      Serial.println("mDNS failed to start in AP mode");
    }

    Serial.println("=== AP SETUP COMPLETE ===");
  } else {
    Serial.println("=== ACCESS POINT FAILED TO START ===");
    Serial.println("This is a critical error!");
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

    // Credentials persist in NVS (settings namespace) — no LittleFS dependency here
    if (!settingWrite(NK_appass, ap_password)) {
      Serial.println("CRITICAL: NVS write failed");
      request->send(500, "text/plain", "Storage error - cannot save configuration");
      return;
    }
    esp32_ap_password = ap_password;

    if (hotspot_ssid[0] != '\0') {
      settingWrite(NK_apssid, hotspot_ssid);
      esp32_ap_ssid = hotspot_ssid;
    } else {
      settingRemove(NK_apssid);  // cleared custom AP SSID -> back to default
      esp32_ap_ssid = "ALTERNATOR_WIFI";
    }

    settingWrite(NK_ssid, ssid);
    settingWrite(NK_pass, password);

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
    settingWrite(NK_first_config_done, "1");
    delay(3000);
    ESP.restart();
  });

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

// Raw WiFiClientSecure POST for the 4 cloud-POST handlers (/checkRegistration /registerProfile
// /updateProfile /deleteAllData). Deliberately NOT HTTPClient: HTTPClient::getString() hangs on
// TLS, and http.begin(url) with no WiFiClientSecure has undefined behavior on https URLs. Pattern
// mirrors executeUploadPayload() in 2_functions.ino. Returns HTTP status code (e.g. 200,
// 401) on success, or a negative sentinel on transport failure:
//   -1 = low heap, -2 = connect fail, -3 = handshake/global timeout, -4 = send fail,
//   -5 = read timeout / no status line.
// responseBuf is null-terminated on return; pass nullptr if body not needed.
int doCloudPOST(const char *endpointPath, const char *payload,
                char *responseBuf, size_t responseBufSize) {
  if (responseBuf && responseBufSize > 0) responseBuf[0] = '\0';

  // Hard floor only — setInsecure() TLS footprint is ~5-10 KB. Do not raise it:
  // a 40 KB threshold (sized for setCACert) tripped right after sensor uploads
  // while heap wasn't reclaimed yet. A real OOM makes client.connect() return
  // false → -2 with diagnostic.
  if (ESP.getMaxAllocHeap() < 15000) {
    Serial.printf("doCloudPOST(%s): heap critically low (max alloc %u), aborting\n",
                  endpointPath, ESP.getMaxAllocHeap());
    return -1;
  }

  WiFiClientSecure client;
  // Match executeUploadPayload (2_functions.ino) which uses setInsecure(). setCACert(server_root_ca)
  // does NOT work here: that cert is our Let's Encrypt ISRG Root X1, but Supabase fronts behind
  // Cloudflare and presents a different chain, so verification fails (mbedTLS -0x2700) and
  // connect() returns false. Deliberate — marine threat model is trusted owner-WiFi.
  client.setInsecure();
  // setTimeout omitted: Stream::setTimeout is ms (not seconds as some docs claim) and
  // our read loops use available()+read() polling with explicit millis() deadlines,
  // so the Stream-level timeout doesn't gate anything in this path.
  client.setHandshakeTimeout(HANDSHAKE_TIMEOUT);

  uint32_t start = millis();
  esp_task_wdt_reset();

  if (!client.connect(host, port, CONNECT_TIMEOUT)) {
    uint32_t connElapsed = millis() - start;  // how long the failed TLS connect took (before extra probes below)
    char errBuf[128] = {0};
    int lastErr = client.lastError(errBuf, sizeof(errBuf));
    // Disambiguate the three distinct failure modes for the dashboard Console — a generic
    // mbedTLS -1 can be any of them and they need very different fixes:
    //   (a) DNS lookup fails        -> router/WiFi DNS problem
    //   (b) DNS ok, raw TCP blocked -> firewall/router blocking outbound 443
    //   (c) DNS ok, TCP ok, TLS bad -> secure handshake fails (TLS version/cipher, or MTU dropping
    //                                  the large Cloudflare cert) even though a browser works
    IPAddress resolved;
    bool dnsOk = WiFi.hostByName(host, resolved);
    bool tcpOk = false;
    if (dnsOk) {
      WiFiClient probe;  // plain TCP, no TLS — tests whether port 443 is reachable at all
      tcpOk = probe.connect(resolved, port, CONNECT_TIMEOUT);
      probe.stop();
    }
    if (!dnsOk) {
      queueConsoleMessageF("Cloud unreachable: DNS lookup for %s failed (check WiFi/router DNS)", host);
    } else if (!tcpOk) {
      queueConsoleMessageF("Cloud unreachable: %s resolves to %s but port 443 is blocked (firewall/router/client isolation)",
                           host, resolved.toString().c_str());
    } else {
      // TCP works but the TLS handshake failed. The usual cause on this device is NOT the network
      // but scarce CONTIGUOUS internal RAM — mbedTLS needs ~32-40KB in one block, and field-on
      // workload (control loop, SSE, logging) fragments it. Report the largest free block so a
      // low number (and a fast fail, not a ~5s timeout) points at RAM, not the network.
      queueConsoleMessageF("Cloud unreachable: TCP reached %s:443 but TLS handshake failed in %u ms; largest free internal block %u B (mbedTLS needs ~32-40KB contiguous — if low, it's RAM not network; try with field OFF)",
                           resolved.toString().c_str(), (unsigned)connElapsed, (unsigned)ESP.getMaxAllocHeap());
    }
    Serial.printf("doCloudPOST(%s): connect FAIL in %u ms, mbedTLS err=%d (0x%X) '%s', dnsOk=%d tcpOk=%d\n",
                  endpointPath, (unsigned)(millis() - start), lastErr, lastErr, errBuf, (int)dnsOk, (int)tcpOk);
    client.stop();
    return -2;
  }
  esp_task_wdt_reset();
  if (millis() - start > GLOBAL_TIMEOUT) {
    Serial.printf("doCloudPOST(%s): GLOBAL_TIMEOUT after connect at %u ms\n",
                  endpointPath, (unsigned)(millis() - start));
    client.stop();
    return -3;
  }

  size_t payloadLen = strlen(payload);
  // HTTP/1.0 deliberately: Cloudflare (which fronts Supabase) sends
  // `Transfer-Encoding: chunked` to HTTP/1.1 clients, and our simple read-until-EOF
  // body loop below has no chunked-decoder. HTTP/1.0 forces Cloudflare to send a
  // plain body + `Connection: close`, which the loop handles correctly. We don't
  // need keep-alive here (each handler does at most one POST).
  int headerBytes = client.printf(
    "POST %s HTTP/1.0\r\n"
    "Host: %s\r\n"
    "Content-Type: application/json\r\n"
    "Authorization: Bearer %s\r\n"
    "Connection: close\r\n"
    "Content-Length: %u\r\n\r\n",
    endpointPath, host, SUPABASE_ANON_KEY, (unsigned)payloadLen);

  if (headerBytes <= 0 || !client.connected()) {
    Serial.printf("doCloudPOST(%s): header write FAIL headerBytes=%d connected=%d\n",
                  endpointPath, headerBytes, (int)client.connected());
    client.stop();
    return -4;
  }

  size_t sent = client.write((const uint8_t *)payload, payloadLen);
  if (sent != payloadLen) {
    Serial.printf("doCloudPOST(%s): payload write SHORT sent=%u expected=%u\n",
                  endpointPath, (unsigned)sent, (unsigned)payloadLen);
    client.stop();
    return -4;
  }

  // Read status line into local buffer
  int httpCode = 0;
  uint32_t readStart = millis();
  char statusBuf[64];
  size_t statusLen = 0;
  bool gotStatusLine = false;

  while (client.connected() && (millis() - readStart < READ_TIMEOUT)) {
    esp_task_wdt_reset();
    while (client.available()) {
      char c = (char)client.read();
      if (statusLen < sizeof(statusBuf) - 1) statusBuf[statusLen++] = c;
      if (c == '\n') { gotStatusLine = true; break; }
    }
    if (gotStatusLine) break;
    if (millis() - start > GLOBAL_TIMEOUT) break;
    delay(1);
  }
  if (!gotStatusLine) {
    Serial.printf("doCloudPOST(%s): no status line within READ_TIMEOUT (got %u bytes, connected=%d)\n",
                  endpointPath, (unsigned)statusLen, (int)client.connected());
    client.stop();
    return -5;
  }
  statusBuf[statusLen] = '\0';
  char *sp = strchr(statusBuf, ' ');
  if (sp) httpCode = atoi(sp + 1);
  if (httpCode <= 0) {
    Serial.printf("doCloudPOST(%s): unparseable status line '%s'\n", endpointPath, statusBuf);
    client.stop();
    return -5;
  }

  // Drain headers until \r\n\r\n, then read body into responseBuf
  enum { ST_HDR, ST_BODY } state = ST_HDR;
  int crlfRun = 0;
  size_t bodyLen = 0;

  while (client.connected() && (millis() - readStart < READ_TIMEOUT)) {
    esp_task_wdt_reset();
    while (client.available()) {
      char c = (char)client.read();
      if (state == ST_HDR) {
        if (c == '\r') continue;
        if (c == '\n') {
          crlfRun++;
          if (crlfRun >= 2) state = ST_BODY;
        } else {
          crlfRun = 0;
        }
      } else {
        if (responseBuf && bodyLen < responseBufSize - 1) responseBuf[bodyLen++] = c;
      }
    }
    if (millis() - start > GLOBAL_TIMEOUT) break;
    delay(1);
  }
  if (responseBuf && responseBufSize > 0) responseBuf[bodyLen] = '\0';

  client.stop();
  return httpCode;
}

// Plain-English reason for a doCloudPOST() negative return code, for the dashboard Console
// (saves a trip to the Serial Monitor). Mirrors the sentinels returned above.
const char *cloudFailReason(int code) {
  switch (code) {
    case -1: return "internal memory too low for secure connection";
    case -2: return "could not reach cloud server (check this WiFi has internet)";
    case -3: return "timed out after connecting";
    case -4: return "lost connection while sending";
    case -5: return "no response from cloud server";
    default: return "unknown error";
  }
}

// Accumulator for the /perfUploadFront POST body (Load CSV) — filled across body chunks, then
// ingested once the request completes. LAN-only single-client dashboard, so one global is fine.
static String perfUploadBuf;
static String altUploadBuf;   // same, for /altUploadFront (alternator-health Load CSV)
static String importConfigBuf;   // body accumulator for POST /importConfig (config sharing)

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
              // Header row
              state.lineLen = snprintf(
                state.line, sizeof(state.line),
                "ts_ms,tempFilt_F,tempProj_F,nominalTarget_A,"
                "rpmCap_A,voltCap_A,uTarget_A,spLimited_A,"
                "pidErr_A,pidOut_pct,duty_pct,RPM,battV,measAmps_A,"
                "penaltyAmps_A,flags,chargeStageDisplay,"
                "outerP,outerI,lookahead,impliedPenalty,antiWindupFired,thermalSlope_F_sec,"
                "freezeWhy,penaltyRaw_A,iCeil_A\n");
            } else if (state.row == 1) {
              // Constants row — written once, Python detects via "CONST" in ts_ms field.
              // limit/warn/crit let the plotter draw the limit + warning-trip (limit+warn) +
              // critical-trip (limit+crit) reference lines. nominalTarget column carries the
              // live regulation setpoint in °F.
              state.lineLen = snprintf(
                state.line, sizeof(state.line),
                "CONST,kp=%.6g,ki=%.6g,lookahead=%.1f,limit=%.1f,warn=%.1f,crit=%.1f,kidownfrac=%.3g,slopewin=%.1f\n",
                TempPIDKp,
                TempPIDKi,
                ThermalLookaheadSec,
                TemperatureLimitF,
                TempWarnExcess,
                TempCritExcess,
                TempPIDKiDownFrac,
                ThermalSlopeWindowSec);
            } else {
              // Data rows — index offset by 2 (header + constants row)
              int idx = (state.oldest + state.row - 2) % THERMAL_LOG_SIZE;
              ThermalLogEntry e;
              memcpy(&e, &thermalLog[idx], sizeof(ThermalLogEntry));
              state.lineLen = snprintf(
                state.line, sizeof(state.line),
                "%lu,%.1f,%.1f,%.1f,%.1f,%.1f,%.1f,%.1f,"
                "%.1f,%.1f,%.1f,%d,%.1f,%.1f,%.1f,%u,%u,"
                "%.1f,%.1f,%.1f,%.1f,%u,%.1f,"
                "%u,%.1f,%.1f\n",
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
                e.outerTermLookahead / 10.0f,
                e.impliedPenalty / 10.0f,
                (unsigned)e.antiWindupFired,
                e.thermalSlope / 1000.0f,
                (unsigned)e.freezeWhy,
                e.penaltyRaw / 10.0f,
                e.holdEstimate / 10.0f);
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
            memcpy(state.entryBuf, &thermalLog[idx], sizeof(ThermalLogEntry));
            state.entryLen = sizeof(ThermalLogEntry);
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
                "# ovFlags: bit0=fastOvActive bit1=iExcessBulk(current-control phase) bit2=hardClamp bit3=iExcess bit4=loadDumpActive\n"
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
                "inaIntervalMs,"
                "mExcessEma,"
                "iExcessThreshold\n");

              state.lineLen = min((int)state.lineLen, (int)sizeof(state.line) - 1);

              // ── Row 2+: data rows ─────────────────────────────────────────
            } else {
              int dataRow = state.row - 2;
              if (dataRow >= state.count) {
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
                "%d,%d,%d,"       // ch1IntervalMs, voltLoopIntervalMs, inaIntervalMs
                "%.3f,%.3f\n",    // mExcessEma, iExcessThreshold (A)
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
                (int)e.inaIntervalMs,
                e.mExcessEma,
                e.iExcessThreshold);
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

  // ── Fast alternator-current channel: live scope dump ──
  // 500 ms raw ring (20 kSPS, calibrated mV) + 16 B header — see faScopeSnapshot for layout.
  // Snapshot is copied into a PSRAM buffer up front (spinlock vs the loop() drain), then
  // streamed; the shared_ptr deleter frees it on any completion path including client abort.
  server.on("/fastscope.bin", HTTP_GET, [](AsyncWebServerRequest *request) {
    const size_t cap = 24 + (size_t)FA_RAW_RING_N * 2;  // header (24 B) + full scope ring
    uint8_t *buf = (uint8_t *)ps_malloc(cap);
    if (!buf) {
      request->send(503, "text/plain", "no mem");
      return;
    }
    size_t n = faScopeSnapshot(buf, cap);
    if (n == 0) {
      free(buf);
      request->send(503, "text/plain", "snapshot failed");
      return;
    }
    std::shared_ptr<uint8_t> sp(buf, free);
    AsyncWebServerResponse *response = request->beginResponse("application/octet-stream", n,
      [sp, n](uint8_t *out, size_t maxLen, size_t index) -> size_t {
        if (index >= n) return 0;
        size_t tw = n - index;
        if (tw > maxLen) tw = maxLen;
        memcpy(out, sp.get() + index, tw);
        return tw;
      });
    response->addHeader("Cache-Control", "no-cache");
    request->send(response);
  });

  // ── Fast alternator-current channel: reference flipbook dump ──
  // Header (8 B LE): u32 magic 'FFLP', u8 refSlots, u8 anomSlots, u16 sampleRate/10.
  // Then (refSlots+anomSlots) × FaFlipPage verbatim (2020 B each, layout pinned by
  // static_assert in 2_functions.ino). Pages freeze once captured, so no lock is needed;
  // `used` is set last on capture so a mid-capture read just sees an empty slot.
  server.on("/faflip.bin", HTTP_GET, [](AsyncWebServerRequest *request) {
    const size_t pgSize = sizeof(FaFlipPage);
    const size_t total = 8 + pgSize * FA_FLIP_SLOTS;
    uint8_t *buf = (uint8_t *)ps_malloc(total);
    if (!buf || !faFlip) {
      if (buf) free(buf);
      request->send(503, "text/plain", "no data");
      return;
    }
    uint32_t magic = 0x46464C50UL;  // 'FFLP'
    uint16_t rateDiv10 = FA_SAMPLE_RATE_HZ / 10;  // 20 kSPS (full rate, matches the live scope)
    memcpy(buf + 0, &magic, 4);
    buf[4] = FA_FLIP_BANDS;
    buf[5] = FA_FLIP_ANOM;
    memcpy(buf + 6, &rateDiv10, 2);
    memcpy(buf + 8, faFlip, pgSize * FA_FLIP_SLOTS);
    std::shared_ptr<uint8_t> sp(buf, free);
    AsyncWebServerResponse *response = request->beginResponse("application/octet-stream", total,
      [sp, total](uint8_t *out, size_t maxLen, size_t index) -> size_t {
        if (index >= total) return 0;
        size_t tw = total - index;
        if (tw > maxLen) tw = maxLen;
        memcpy(out, sp.get() + index, tw);
        return tw;
      });
    response->addHeader("Cache-Control", "no-cache");
    request->send(response);
  });

  // ── Fast alternator-current channel: disturbance matrix CSV export ──
  // One row per populated cell (RPM bin × amps bin): top-6 mean-accumulated peaks +
  // broadband pk-pk + window count. Chunked like /alttrend.csv. Reads race the Core-1
  // merge harmlessly (diagnostic export — a torn cell is one stale row).
  server.on("/famatrix.csv", HTTP_GET, [](AsyncWebServerRequest *request) {
    struct FmExp { int idx; bool header, done; char line[280]; int len, pos; };
    FmExp st;
    st.idx = 0; st.header = true; st.done = false; st.len = 0; st.pos = 0;
    AsyncWebServerResponse *response = request->beginChunkedResponse("text/csv",
      [st](uint8_t *buf, size_t maxLen, size_t) mutable -> size_t {
        if (st.done) return 0;
        size_t written = 0;
        const int NCELLS = FA_RPM_BINS * FA_AMP_BINS;
        while (written < maxLen) {
          if (st.pos >= st.len) {
            if (st.header) {
              st.len = snprintf(st.line, sizeof(st.line),
                                "rpmLo,ampLo,windows,pkpkA,f1Hz,a1A,n1,f2Hz,a2A,n2,f3Hz,a3A,n3,f4Hz,a4A,n4,f5Hz,a5A,n5,f6Hz,a6A,n6\n");
              st.header = false;
            } else {
              while (st.idx < NCELLS && faMatrix && faMatrix[st.idx].windows == 0) st.idx++;
              if (st.idx >= NCELLS || !faMatrix) { st.done = true; return written; }
              FaCell *c = &faMatrix[st.idx];
              int rpmLo = (st.idx / FA_AMP_BINS) * FA_RPM_BIN_W;
              int ampLo = FA_AMP_BIN_LO + (st.idx % FA_AMP_BINS) * FA_AMP_BIN_W;
              int l = snprintf(st.line, sizeof(st.line), "%d,%d,%u,%.2f",
                               rpmLo, ampLo, (unsigned)c->windows, c->pkpkAX100 / 100.0);
              for (int s = 0; s < FA_CELL_PEAKS; s++)
                l += snprintf(st.line + l, sizeof(st.line) - l, ",%.1f,%.2f,%u",
                              c->pk[s].freqHzX10 / 10.0, c->pk[s].ampAX100 / 100.0, (unsigned)c->pk[s].nAcc);
              l += snprintf(st.line + l, sizeof(st.line) - l, "\n");
              st.len = l;
              st.idx++;
            }
            st.pos = 0;
          }
          size_t tw = min((size_t)(st.len - st.pos), maxLen - written);
          memcpy(buf + written, st.line + st.pos, tw);
          written += tw;
          st.pos += (int)tw;
        }
        return written;
      });
    response->addHeader("Cache-Control", "no-cache");
    request->send(response);
  });

  // ── Measured filtered ripple per RPM bin (RPM_RIPPLE_TABLE_SPEC) ──
  // Reads the game-scoped 1-D ripTab: one row per 50-RPM bin with any state, IExcessTau-filtered pk-pk
  // (== detector mExcessEma pk-pk) captured at the game's fixed commanded current. First three columns
  // keep the legacy layout (parsers read [0..2] after a length ≥ 3 check); altSt/battSt appended:
  // 0 = none, 1 = pending (value = the unconfirmed candidate), 2 = committed. The wizard pick and the
  // game's kill/coverage logic use committed only; the game strip may draw pending dimmed.
  server.on("/filtripple.csv", HTTP_GET, [](AsyncWebServerRequest *request) {
    String out = "rpmLo,altFiltPkA,battFiltPkA,altSt,battSt\n";
    for (int r = 0; r < RIPTAB_BINS; r++) {
      const RipTabCell *c = &ripTab.cell[r];
      if (c->state == 0) continue;
      int altSt  = (c->state & RIPTAB_ALT_DONE)  ? 2 : (c->state & RIPTAB_ALT_PEND)  ? 1 : 0;
      int battSt = (c->state & RIPTAB_BATT_DONE) ? 2 : (c->state & RIPTAB_BATT_PEND) ? 1 : 0;
      uint16_t altV  = (altSt == 2)  ? c->altPkX100  : c->altPendX100;
      uint16_t battV = (battSt == 2) ? c->battPkX100 : c->battPendX100;
      out += String(r * FA_RPM_BIN_W);
      out += ',';
      out += String(altV / 100.0f, 2);
      out += ',';
      out += String(battV / 100.0f, 2);
      out += ',';
      out += String(altSt);
      out += ',';
      out += String(battSt);
      out += '\n';
    }
    AsyncWebServerResponse *response = request->beginResponse(200, "text/csv", out);
    response->addHeader("Cache-Control", "no-cache");
    request->send(response);
  });

  // ── RPM ripple table session stamps (RPM_RIPPLE_TABLE_SPEC §4) ──
  // "captured at N A, 13.1–13.6 V" for the wizard + Protections overlay annotation. levelA 0 = no
  // sweep recorded yet; ibv fields 0 when no window folded.
  server.on("/riptabmeta", HTTP_GET, [](AsyncWebServerRequest *request) {
    bool ibvOk = (ripTab.sess.ibvMaxV >= ripTab.sess.ibvMinV);
    String out = "{\"level\":" + String(ripTab.sess.levelA, 1)
               + ",\"ibvMin\":" + String(ibvOk ? ripTab.sess.ibvMinV : 0.0f, 2)
               + ",\"ibvMax\":" + String(ibvOk ? ripTab.sess.ibvMaxV : 0.0f, 2)
               + ",\"idleRpm\":" + String(ripTab.sess.idleRpm)
               + ",\"epoch\":" + String(ripTab.sess.epoch)
               + ",\"gameActive\":" + String((ripGameFill || ripTabPendingWipe) ? 1 : 0) + "}";
    AsyncWebServerResponse *response = request->beginResponse(200, "application/json", out);
    response->addHeader("Cache-Control", "no-cache");
    request->send(response);
  });

  // ── Filtered-ripple fold forensics (diagnostic) ──
  // One row per ripTab fold event, oldest→newest — the "why won't this bin settle" trace. event:
  // P = pending set, D = disagree (otherA = the pending it replaced), C = commit (otherA = the pair
  // partner; the committed value is their average). pkpkA is always the window's own value.
  // crossings ~2 = one-shot transient; many = sustained oscillation.
  server.on("/ripforensic.csv", HTTP_GET, [](AsyncWebServerRequest *request) {
    String out = "detector,rpm,ampLo,meanA,pkpkA,meanShiftA,cmdTravelA,crossings,event,otherA\n";
    uint16_t n = ripForensicCount < RIP_FORENSIC_CAP ? ripForensicCount : RIP_FORENSIC_CAP;
    for (uint16_t i = 0; i < n; i++) {
      uint16_t idx = (ripForensicHead + RIP_FORENSIC_CAP - n + i) % RIP_FORENSIC_CAP;
      const RipForensicPt &p = ripForensic[idx];
      out += (p.detector == 0 ? "alt" : "batt");
      out += ',' + String(p.rpm) + ',' + String(p.ampLo) + ',';
      out += String(p.meanX100 / 100.0f, 2) + ',' + String(p.pkpkX100 / 100.0f, 2) + ',';
      out += String(p.shiftX100 / 100.0f, 2) + ',' + String(p.cmdTravelX100 / 100.0f, 2) + ',';
      out += String(p.crossings) + ',' + String(p.event) + ',' + String(p.otherX100 / 100.0f, 2) + '\n';
    }
    AsyncWebServerResponse *response = request->beginResponse(200, "text/csv", out);
    response->addHeader("Cache-Control", "no-cache");
    request->send(response);
  });

  // Active 3-current resonance test points: (rpm, operating current, pk-pk) per window since arm. Small
  // (≤BCUR_RTEST_CAP rows) → plain non-chunked response. Browser fits ripple = a0 + a1·I from these.
  server.on("/bcurrtest.csv", HTTP_GET, [](AsyncWebServerRequest *request) {
    String out = "rpm,iA,pkpkA,iAltA,pkpkAltA\n";  // iA/pkpkA = battery (INA); iAltA/pkpkAltA = alternator (ADS)
    uint16_t n = bcurRtestCount; if (n > BCUR_RTEST_CAP) n = BCUR_RTEST_CAP;
    for (uint16_t i = 0; i < n; i++) {
      out += String(bcurRtest[i].rpm);
      out += ',';
      out += String(bcurRtest[i].iX100 / 100.0f, 2);
      out += ',';
      out += String(bcurRtest[i].pkpkX100 / 100.0f, 2);
      out += ',';
      out += String(bcurRtest[i].iAltX100 / 100.0f, 2);
      out += ',';
      out += String(bcurRtest[i].altPkpkX100 / 100.0f, 2);
      out += '\n';
    }
    AsyncWebServerResponse *response = request->beginResponse(200, "text/csv", out);
    response->addHeader("Cache-Control", "no-cache");
    request->send(response);
  });

  // ── Measured ripple projection (§3.3/§4) — the stored per-detector fit for the Protections plot ──
  // ripple(I)=a0+a1·I plus the 3 measured (I, pk-pk) points and the RPM the test ran at. n=0 → no test
  // yet (plot shows the threshold line only). Read on the Protections tab and by the Step-6 review.
  server.on("/ripfit", HTTP_GET, [](AsyncWebServerRequest *request) {
    auto j = [](const RipFit &r) {
      String s = "{\"a0\":" + String(r.a0, 4) + ",\"a1\":" + String(r.a1, 5)
               + ",\"rpm\":" + String(r.rpm, 0) + ",\"n\":" + String((int)r.nPts) + ",\"i\":[";
      for (int k = 0; k < 3; k++) { if (k) s += ','; s += String(r.iPt[k], 2); }
      s += "],\"pk\":[";
      for (int k = 0; k < 3; k++) { if (k) s += ','; s += String(r.pkPt[k], 3); }
      s += "]}";
      return s;
    };
    request->send(200, "application/json", "{\"alt\":" + j(ripFitAlt) + "}");
  });

  // ── Alternator (charging-system) health v2 — schema + curve + records + trend exports ──
  // Self-describing schema; the dashboard zips these names against AltLive/AltSettings SSE values.
  server.on("/altschema", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(200, "application/json", altSchemaJson());
  });
  server.on("/installid", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(200, "text/plain", installId);
  });
  // Auto Min% learning ("knee tracker") state: knobs + live status + per-bin learned floors.
  server.on("/kneeLearnState", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(200, "application/json", kneeLearnStateJson());
  });
  // First-boot SoC seed record for the commissioning popup; ack=1 once the user pressed Finish.
  server.on("/socseed", HTTP_GET, [](AsyncWebServerRequest *request) {
    String snap = settingExists(NK_SocSeedSnap) ? settingRead(NK_SocSeedSnap) : String();
    String ack = settingExists(NK_SocSeedAck) ? settingRead(NK_SocSeedAck) : String();
    if (snap.length() == 0) snap = "null";
    if (ack.length() == 0) ack = "1";
    request->send(200, "application/json", String("{\"snap\":") + snap + ",\"ack\":" + ack + "}");
  });
  // Battery Health: DCIR test status + result table + capacity-vs-cycles trend.
  server.on("/batteryHealth", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(200, "application/json", bhBuildStatusJson());
  });
  // Zero-drift characterization log: small status JSON for the dashboard panel (enable, fill, span).
  server.on("/zerologstate", HTTP_GET, [](AsyncWebServerRequest *request) {
    uint32_t oldest = 0, newest = 0;
    if (zeroLogRing && zeroLogCount > 0) {
      uint16_t oi = (zeroLogCount < ZEROLOG_RING_SIZE) ? 0 : zeroLogHead;
      uint16_t ni = (uint16_t)((zeroLogHead + ZEROLOG_RING_SIZE - 1) % ZEROLOG_RING_SIZE);
      oldest = zeroLogRing[oi].epoch;
      newest = zeroLogRing[ni].epoch;
    }
    String j = "{\"enable\":";  j += (ZeroLogEnable ? 1 : 0);
    j += ",\"count\":";  j += String((unsigned)zeroLogCount);
    j += ",\"cap\":";    j += String((unsigned)ZEROLOG_RING_SIZE);
    j += ",\"oldest\":"; j += String(oldest);
    j += ",\"newest\":"; j += String(newest);
    j += "}";
    request->send(200, "application/json", j);
  });
  // Temp-comp zero correction: live learned-equation state for the dashboard panel.
  server.on("/zerofitstate", HTTP_GET, [](AsyncWebServerRequest *request) {
    String j = "{\"valid\":";   j += zfValid;
    j += ",\"sensor\":\"";      j += (zfSensor == ZF_ALT ? "alt" : "board"); j += "\"";
    j += ",\"c\":";             j += String(zfC, 4);
    j += ",\"b\":";             j += String(zfB, 5);
    j += ",\"r2\":";            j += String(zfR2, 3);
    j += ",\"corrNow\":";       j += String(DynamicAltCurrentZero, 3);
    j += ",\"applied\":";       j += (AutoAltCurrentZero ? 1 : 0);
    j += ",\"epoch\":";         j += String((unsigned)zfLastEpoch);
    j += ",\"histCount\":";     j += String((unsigned)zeroFitHistCount);
    j += "}";
    request->send(200, "application/json", j);
  });
  // Temp-comp zero correction: daily-fit history (≤90 rows) for the trend view. Small enough to build inline.
  server.on("/zerofit.csv", HTTP_GET, [](AsyncWebServerRequest *request) {
    String out = "epoch,sensor,c,b,r2,n\n";
    if (zeroFitHist && zeroFitHistCount > 0) {
      for (uint16_t i = 0; i < zeroFitHistCount; i++) {
        uint16_t idx = (zeroFitHistCount < ZFIT_HIST_SIZE) ? i
                       : (uint16_t)((zeroFitHistHead + i) % ZFIT_HIST_SIZE);
        ZeroFitRecord &r = zeroFitHist[idx];
        char line[80];
        snprintf(line, sizeof(line), "%u,%s,%.4f,%.5f,%.3f,%u\n",
                 (unsigned)r.epoch, (r.sensor == ZF_ALT ? "alt" : "board"),
                 r.c, r.b, r.r2, (unsigned)r.n);
        out += line;
      }
    }
    request->send(200, "text/csv", out);
  });
  // Zero-drift log → CSV, streamed oldest-first (constant RAM). Reads the live PSRAM ring, so the
  // download is always complete regardless of the last flash flush.
  server.on("/zerolog.csv", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (!zeroLogRing || zeroLogCount == 0) {
      request->send(200, "text/csv", "epoch,rpm,battV,altTempF,boardTempF,amps,p2pAmps\n");
      return;
    }
    struct ZExp { uint16_t head, count, idx; bool header, done; char line[96]; int len, pos; };
    ZExp st;
    st.head = zeroLogHead; st.count = zeroLogCount; st.idx = 0;
    st.header = true; st.done = false; st.len = 0; st.pos = 0;
    AsyncWebServerResponse *response = request->beginChunkedResponse("text/csv",
      [st](uint8_t *buf, size_t maxLen, size_t) mutable -> size_t {
        if (st.done) return 0;
        size_t written = 0;
        while (written < maxLen) {
          if (st.pos >= st.len) {
            if (st.header) {
              st.len = snprintf(st.line, sizeof(st.line), "epoch,rpm,battV,altTempF,boardTempF,amps,p2pAmps\n");
              st.header = false;
            } else {
              if (st.idx >= st.count) { st.done = true; return written; }
              uint16_t ai = (st.count < ZEROLOG_RING_SIZE) ? st.idx
                            : (uint16_t)((st.head + st.idx) % ZEROLOG_RING_SIZE);
              ZeroLogRecord &r = zeroLogRing[ai];
              char altF[12];   // empty cell when the alt-temp sensor was absent (blank sentinel)
              if (r.altTempFx10 == ZEROLOG_TEMP_BLANK) altF[0] = '\0';
              else snprintf(altF, sizeof(altF), "%.1f", r.altTempFx10 / 10.0f);
              char boardF[12]; // empty cell when the board-temp sensor was absent (blank sentinel)
              if (r.boardTempFx10 == ZEROLOG_TEMP_BLANK) boardF[0] = '\0';
              else snprintf(boardF, sizeof(boardF), "%.1f", r.boardTempFx10 / 10.0f);
              st.len = snprintf(st.line, sizeof(st.line), "%u,%d,%.2f,%s,%s,%.3f,%.3f\n",
                                (unsigned)r.epoch, (int)r.rpm, r.battVx100 / 100.0f,
                                altF, boardF, r.amps, r.p2pAmps);
              st.idx++;
            }
            st.pos = 0;
          }
          size_t tw = min((size_t)(st.len - st.pos), maxLen - written);
          memcpy(buf + written, st.line + st.pos, tw);
          written += tw; st.pos += (int)tw;
        }
        return written;
      });
    response->addHeader("Cache-Control", "no-cache");
    request->send(response);
  });
  // The held best-ever front (BEFRONT1 CSV artifact) — streamed (constant RAM at any front size).
  server.on("/altcurve.csv", HTTP_GET, [](AsyncWebServerRequest *request) {
    altCurveCsvSend(request);
  });
  // Front support points as a plain scatter table — streamed.
  server.on("/altrecords.csv", HTTP_GET, [](AsyncWebServerRequest *request) {
    altFrontRecordsCsvSend(request);
  });
  // Full alt-health state dump for offline / AI debugging (Download Debug CSV) — streamed.
  server.on("/altdebug.csv", HTTP_GET, [](AsyncWebServerRequest *request) {
    altDebugCsvSend(request);
  });
  // Performance-vs-engine-hours trend (header + points, chunked). This is the headline. Decimated to
  // <= TR_MAXOUT output points for readability + payload (full hourly history stays on the device):
  // each output point is one bucket of source hours — worst = min (preserve the early-warning
  // envelope), overall = mean. stride==1 (<= TR_MAXOUT total) streams every hour, as before.
  extern float altTrendBucketSec;   // defined in 7_functions.ino (concatenated later) — needed to convert bucket index → engine-hours
  server.on("/alttrend.csv", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (!altTrend) { request->send(200, "text/plain", "engHours,worstPct,overallPct\n"); return; }
    const int TR_MAXOUT = 200;
    // Optional ?hours=N window: decimate WITHIN the last N engine-hours so the dashboard's
    // 10h/100h shortcuts aren't starved. A single global 200-pt stride over a multi-thousand-hour
    // history lands ~0 points inside a recent 10h slice; windowing the base index first fixes that.
    // hours<=0 or absent = full history (the "All" shortcut).
    int base = 0;
    if (request->hasParam("hours")) {
      int winHours = request->getParam("hours")->value().toInt();
      if (winHours > 0 && altTrendCount > 0) {
        float hrPerBucket = altTrendBucketSec / 3600.0f;
        float cutoff = altTrend[altTrendCount - 1].engHour * hrPerBucket - (float)winHours;
        for (int i = 0; i < altTrendCount; i++) {
          if (altTrend[i].engHour * hrPerBucket >= cutoff) { base = i; break; }
        }
      }
    }
    struct TrExp { int base, total, stride, outIdx, numOut; bool header, done; char line[64]; int len, pos; };
    TrExp st;
    st.base = base;
    st.total = altTrendCount - base;
    st.stride = (st.total > TR_MAXOUT) ? ((st.total + TR_MAXOUT - 1) / TR_MAXOUT) : 1;
    st.numOut = (st.stride > 0) ? ((st.total + st.stride - 1) / st.stride) : 0;
    st.outIdx = 0; st.header = true; st.done = false; st.len = 0; st.pos = 0;
    AsyncWebServerResponse *response = request->beginChunkedResponse("text/csv",
      [st](uint8_t *buf, size_t maxLen, size_t) mutable -> size_t {
        if (st.done) return 0;
        size_t written = 0;
        while (written < maxLen) {
          if (st.pos >= st.len) {
            if (st.header) {
              st.len = snprintf(st.line, sizeof(st.line), "engHours,worstPct,overallPct\n");
              st.header = false;
            } else {
              if (st.outIdx >= st.numOut) { st.done = true; return written; }
              int start = st.base + st.outIdx * st.stride, end = start + st.stride;
              if (end > st.base + st.total) end = st.base + st.total;
              float worst = 1e9f, sum = 0; int n = 0;
              for (int i = start; i < end; i++) {
                float w = altTrend[i].worstPct / 10.0f;
                if (w < worst) worst = w;
                sum += altTrend[i].overallPct / 10.0f; n++;
              }
              // Stored engHour is a BUCKET INDEX. Convert to true engine-hours so the X axis is correct at
              // any bucket size (index × bucketSec/3600): at 3600 s index==hours; at 600 s six buckets = 1 h.
              float ehHours = (float)altTrend[end - 1].engHour * (altTrendBucketSec / 3600.0f);
              st.len = snprintf(st.line, sizeof(st.line), "%.3f,%.1f,%.1f\n", ehHours, worst, n ? sum / n : 0.0f);
              st.outIdx++;
            }
            st.pos = 0;
          }
          size_t tw = min((size_t)(st.len - st.pos), maxLen - written);
          memcpy(buf + written, st.line + st.pos, tw);
          written += tw; st.pos += (int)tw;
        }
        return written;
      });
    response->addHeader("Cache-Control", "no-cache");
    request->send(response);
  });

  // Boat-performance telemetry schema — the dashboard fetches this once and zips the names
  // against the PerfLive/PerfSettings SSE values, so no field array is hand-kept in JS.
  server.on("/perfschema", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(200, "application/json", perfSchemaJson());
  });

  // The held best-ever fronts (BEFRONT1 CSV pair: SAIL + MOTOR blocks) — dashboard polar/curve source. Streamed.
  server.on("/perfcurve.csv", HTTP_GET, [](AsyncWebServerRequest *request) {
    perfCurveCsvSend(request);
  });

  // Front support points as a plain scatter table (sail + motor, mode-tagged) — streamed.
  server.on("/perfrecords.csv", HTTP_GET, [](AsyncWebServerRequest *request) {
    perfRecordsCsvSend(request);
  });

  // Load CSV (import a shared/saved polar): POST the BEFRONT1 sail+motor pair as the raw body to
  // /perfUploadFront?password=XXX. The body handler accumulates chunks into perfUploadBuf; the
  // request handler (runs once the body is complete) password-gates, then perfUploadFrontCsv()
  // replaces both fronts and applies the user's chosen mode (?fixed=1 freeze / 0 learn) + persists.
  // Same ingest path as cloud-sync/Load-saved.
  server.on("/perfUploadFront", HTTP_POST,
    [](AsyncWebServerRequest *request) {
      if (!request->hasParam("password") || strcmp(request->getParam("password")->value().c_str(), requiredPassword) != 0) {
        perfUploadBuf = ""; request->send(403, "text/plain", "Forbidden"); return;
      }
      if (perfUploadBuf.length() < 8 || perfUploadBuf.indexOf("BEFRONT1") < 0) {
        perfUploadBuf = ""; request->send(400, "text/plain", "No BEFRONT1 data in upload"); return;
      }
      char *bodyc = strdup(perfUploadBuf.c_str());
      perfUploadBuf = "";
      if (!bodyc) { request->send(500, "text/plain", "Out of memory"); return; }
      // fixed=1 → FIXED+paused (freeze), fixed=0 → LEARNED+resumed (learn). Default freeze if absent.
      bool fixed = (!request->hasParam("fixed")) || (request->getParam("fixed")->value().toInt() != 0);
      bool ok = perfUploadFrontCsv(bodyc, fixed);
      free(bodyc);
      request->send(ok ? 200 : 400, "text/plain", ok ? "OK" : "Parse failed");
    },
    NULL,
    [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
      if (index == 0) { perfUploadBuf = ""; perfUploadBuf.reserve(total + 1); }
      perfUploadBuf.concat((const char *)data, len);
    });

  // Load CSV (alternator health): POST the BEFRONT1 front as the raw body to /altUploadFront?password=XXX.
  // Mirrors /perfUploadFront. ?fixed=1 freeze (local only) / 0 learn (adopt to cloud, tagged). The
  // request handler password-gates once the body is complete, then altUploadFrontCsv() applies it.
  server.on("/altUploadFront", HTTP_POST,
    [](AsyncWebServerRequest *request) {
      if (!request->hasParam("password") || strcmp(request->getParam("password")->value().c_str(), requiredPassword) != 0) {
        altUploadBuf = ""; request->send(403, "text/plain", "Forbidden"); return;
      }
      if (altUploadBuf.length() < 8 || altUploadBuf.indexOf("BEFRONT1") < 0) {
        altUploadBuf = ""; request->send(400, "text/plain", "No BEFRONT1 data in upload"); return;
      }
      char *bodyc = strdup(altUploadBuf.c_str());
      altUploadBuf = "";
      if (!bodyc) { request->send(500, "text/plain", "Out of memory"); return; }
      // fixed=1 → FIXED+paused (freeze), fixed=0 → LEARNED+resumed+adopt (learn). Default freeze if absent.
      bool fixed = (!request->hasParam("fixed")) || (request->getParam("fixed")->value().toInt() != 0);
      bool ok = altUploadFrontCsv(bodyc, fixed);
      free(bodyc);
      request->send(ok ? 200 : 400, "text/plain", ok ? "OK" : "Parse failed");
    },
    NULL,
    [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
      if (index == 0) { altUploadBuf = ""; altUploadBuf.reserve(total + 1); }
      altUploadBuf.concat((const char *)data, len);
    });

  // Config Sharing — export the cloneable settings set as one JSON blob (for download
  // or cloud submission). ?includeHardware=1 adds the tier-2 install/topology keys.
  // Password-gated like the upload endpoints.
  server.on("/exportConfig", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (!request->hasParam("password") || strcmp(request->getParam("password")->value().c_str(), requiredPassword) != 0) {
      request->send(403, "text/plain", "Forbidden"); return;
    }
    bool hw = request->hasParam("includeHardware") && request->getParam("includeHardware")->value().toInt() != 0;
    request->send(200, "application/json", exportConfigJson(hw));
  });

  // Config Sharing — apply an imported config blob (POST the /exportConfig JSON as the body).
  // Only allowlisted manifest keys are written; everything else in the body is ignored.
  // ?includeHardware=1 also applies the tier-2 install/topology keys (default OFF — different
  // hardware). Reboots after applying so InitSystemSettings re-reads the whole set consistently
  // (suppress with ?noReboot=1). Mirrors /perfUploadFront's body-accumulator pattern.
  server.on("/importConfig", HTTP_POST,
    [](AsyncWebServerRequest *request) {
      if (!request->hasParam("password") || strcmp(request->getParam("password")->value().c_str(), requiredPassword) != 0) {
        importConfigBuf = ""; request->send(403, "text/plain", "Forbidden"); return;
      }
      if (importConfigBuf.length() < 2) {
        importConfigBuf = ""; request->send(400, "text/plain", "Empty body"); return;
      }
      char *bodyc = strdup(importConfigBuf.c_str());
      importConfigBuf = "";
      if (!bodyc) { request->send(500, "text/plain", "Out of memory"); return; }
      bool hw = request->hasParam("includeHardware") && request->getParam("includeHardware")->value().toInt() != 0;
      int n = applyImportConfig(bodyc, hw);
      free(bodyc);
      if (n < 0) { request->send(400, "text/plain", "No config object in body"); return; }
      bool reboot = !request->hasParam("noReboot") || request->getParam("noReboot")->value().toInt() == 0;
      if (reboot) { rebootRequested = true; rebootRequestedAt = millis(); }
      String resp = "{\"applied\":";
      resp += n; resp += ",\"reboot\":"; resp += (reboot ? 1 : 0); resp += "}";
      request->send(200, "application/json", resp);
    },
    NULL,
    [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
      if (total > 65536) return;   // bound: a real config blob is ~10 KB; reject anything absurd
      if (index == 0) { importConfigBuf = ""; importConfigBuf.reserve(total + 1); }
      importConfigBuf.concat((const char *)data, len);
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
    float kp = (float)VoltageKp_active;  // gains ACTUALLY in effect (Manual or Auto-λ, 12V-block normalized) — not the raw manual VoltageKp, which the loop ignores in Auto mode / on 24-48V
    float ki = (float)VoltageKi_active;
    float kd = 0.0f;  // voltage loop has no D term; 0 preserves binary header layout
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


  // Barometric pressure 14-day history dump. 8-byte header + 4032 little-endian uint16
  // samples (mbar × 10; 0 = no sample). Dashboard fetches once on Other-tab activation
  // and every 5 min thereafter.
  // Header layout (little-endian):
  //   [0..1]  uint16 headIdx  — next write slot (samples[headIdx-1] is newest)
  //   [2..5]  uint32 epoch    — wall-clock seconds of newest sample (0 if never synced)
  //   [6..7]  uint16 count    — BARO_HISTORY_SIZE, for client-side sanity check
  server.on("/baroHistory.bin", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (!baroPressureHistory) {
      request->send(503, "application/octet-stream", "");
      return;
    }
    struct BaroDLState {
      uint8_t  header[8];
      int      headerPos;
      uint16_t samplePos;
      bool     done;
    };
    BaroDLState state;
    memset(&state, 0, sizeof(state));
    uint16_t headLE  = baroHistoryHead;
    uint32_t epochLE = (uint32_t)baroHistoryLastEpoch;
    uint16_t countLE = BARO_HISTORY_SIZE;
    memcpy(state.header + 0, &headLE,  2);
    memcpy(state.header + 2, &epochLE, 4);
    memcpy(state.header + 6, &countLE, 2);

    AsyncWebServerResponse *response = request->beginChunkedResponse(
      "application/octet-stream",
      [state](uint8_t *buf, size_t maxLen, size_t) mutable -> size_t {
        if (state.done) return 0;
        size_t written = 0;
        while (written < maxLen) {
          if (state.headerPos < 8) {
            size_t canSend = min(maxLen - written, (size_t)(8 - state.headerPos));
            memcpy(buf + written, state.header + state.headerPos, canSend);
            written += canSend;
            state.headerPos += (int)canSend;
            continue;
          }
          if (state.samplePos >= BARO_HISTORY_SIZE) {
            state.done = true;
            return written;
          }
          size_t bytesLeft = (BARO_HISTORY_SIZE - state.samplePos) * sizeof(uint16_t);
          size_t canSend = min(maxLen - written, bytesLeft);
          memcpy(buf + written,
                 ((uint8_t *)baroPressureHistory) + state.samplePos * sizeof(uint16_t),
                 canSend);
          written += canSend;
          state.samplePos += canSend / sizeof(uint16_t);
        }
        return written;
      });
    response->addHeader("Cache-Control", "no-cache");
    request->send(response);
  });

  // Long Term Plots history download — header (16 B LE) + raw ring array. Lazy:
  // dashboard fetches once on tab open. JS walks records in chrono order via head +
  // count; record time = lastEpoch − (count−1−i) × intervalSec (no per-record stamp).
  //   [0..1] u16 head  [2..3] u16 count  [4..5] u16 capacity  [6..7] u16 recordSize
  //   [8..11] u32 lastEpoch  [12..15] u32 intervalSec
  server.on("/longTermPlots.bin", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (!longTermRing) {
      request->send(503, "application/octet-stream", "");
      return;
    }
    struct LtDLState {
      uint8_t  header[16];
      int      headerPos;
      uint32_t recIdx;      // which chronological record (0..count-1)
      uint32_t recBytePos;  // byte offset within the current record
      bool     done;
    };
    LtDLState state;
    memset(&state, 0, sizeof(state));
    uint16_t headLE = longTermHead, countLE = longTermCount;
    uint16_t capLE = LONGTERM_RING_SIZE, recLE = (uint16_t)sizeof(LongTermRecord);
    uint32_t epochLE = (uint32_t)longTermLastEpoch;
    uint32_t intervalLE = SENSOR_UPLOAD_INTERVAL / 1000UL;
    uint16_t tail = (countLE < capLE) ? 0 : headLE;   // oldest record (chronological start)
    memcpy(state.header + 0,  &headLE,     2);
    memcpy(state.header + 2,  &countLE,    2);
    memcpy(state.header + 4,  &capLE,      2);
    memcpy(state.header + 6,  &recLE,      2);
    memcpy(state.header + 8,  &epochLE,    4);
    memcpy(state.header + 12, &intervalLE, 4);

    // Body = `count` records in CHRONOLOGICAL order (oldest first), NOT the full ring.
    // Sending the whole 4320-slot ring (~540 KB) made the async server truncate the
    // chunked transfer (ERR_INCOMPLETE_CHUNKED_ENCODING); count×recSize stays small until
    // the ring is genuinely full. JS reads records linearly (base = 16 + i*recSize).
    AsyncWebServerResponse *response = request->beginChunkedResponse(
      "application/octet-stream",
      [state, countLE, tail, capLE, recLE](uint8_t *buf, size_t maxLen, size_t) mutable -> size_t {
        if (state.done) return 0;
        size_t written = 0;
        while (written < maxLen) {
          if (state.headerPos < 16) {
            size_t canSend = min(maxLen - written, (size_t)(16 - state.headerPos));
            memcpy(buf + written, state.header + state.headerPos, canSend);
            written += canSend;
            state.headerPos += (int)canSend;
            continue;
          }
          if (state.recIdx >= countLE) { state.done = true; return written; }
          uint32_t srcIdx = ((uint32_t)tail + state.recIdx) % capLE;   // ring → chronological
          const uint8_t *src = (const uint8_t *)longTermRing + (size_t)srcIdx * recLE;
          size_t canSend = min(maxLen - written, (size_t)(recLE - state.recBytePos));
          memcpy(buf + written, src + state.recBytePos, canSend);
          written += canSend;
          state.recBytePos += (uint32_t)canSend;
          if (state.recBytePos >= recLE) { state.recBytePos = 0; state.recIdx++; }
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

  server.on("/stoplogs", HTTP_POST, [](AsyncWebServerRequest *request) {
    loggingActive = false;
    request->send(200, "text/plain", "Logging stopped");
  });

  server.on("/startlogs", HTTP_POST, [](AsyncWebServerRequest *request) {
    loggingActive = true;
    request->send(200, "text/plain", "Logging started");
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
    BOAT_DISPLACEMENT_LBS = doc["boat_displacement_lbs"];
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
    // System voltage is the sole source of truth for the 12/24/48V class. On a change, rescale the
    // whole charge-voltage profile + re-derive the hard-shutdown trip + the INA228 OV limit + both
    // control loops' normalized gains (CV and CC). The dashboard warns the user before submitting.
    int oldBatteryVoltage = BATTERY_VOLTAGE;
    int newBatteryVoltage = doc["battery_voltage"] | (int)BATTERY_VOLTAGE;
    if (newBatteryVoltage != 12 && newBatteryVoltage != 24 && newBatteryVoltage != 48) newBatteryVoltage = oldBatteryVoltage;
    BATTERY_VOLTAGE = (uint8_t)newBatteryVoltage;
    applyNominalVoltageChange(oldBatteryVoltage, newBatteryVoltage);
    BatteryCapacity_Ah = doc["battery_capacity_ah"];
    BATTERY_TYPE = doc["battery_type"].as<String>();
    BATTERY_MAKE_MODEL = doc["battery_make_model"].as<String>();
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
  server.on("/get", HTTP_GET, [](AsyncWebServerRequest *request) {
    bool foundParameter = false;
    bool nvsPersistNow = false;   // set by discrete reset/set handlers that write saveNVSDataFull()-owned vars; forces ONE immediate persist at the end so a reboot before the next field-off edge can't revert the action
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
        settingWrite(NK_OnOff, "0");
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

    else if (request->hasParam("InputFilterTC")) {
      foundParameter = true;
      inputMessage = request->getParam("InputFilterTC")->value();
      settingWrite(NK_InputFilterTC, inputMessage.c_str());
      InputFilterTC = inputMessage.toFloat();
      if (CVTuningMode) cvTuningParamChanged = true;
    }

    else if (request->hasParam("SystemIDStepAmplitude")) {
      foundParameter = true;
      inputMessage = request->getParam("SystemIDStepAmplitude")->value();
      settingWrite(NK_SystemIDStepAmplitude, inputMessage.c_str());
      SystemIDStepAmplitude = inputMessage.toFloat();
    }
    else if (request->hasParam("systemIDTestType")) {
      foundParameter = true;
      inputMessage = request->getParam("systemIDTestType")->value();
      settingWrite(NK_systemIDTestType, inputMessage.c_str());
      systemIDTestType = (uint8_t)inputMessage.toInt();
    }
    else if (request->hasParam("systemIDSineFreqStart")) {
      foundParameter = true;
      inputMessage = request->getParam("systemIDSineFreqStart")->value();
      settingWrite(NK_systemIDSineFreqStart, inputMessage.c_str());
      systemIDSineFreqStart = inputMessage.toFloat();
    }
    else if (request->hasParam("systemIDSineFreqEnd")) {
      foundParameter = true;
      inputMessage = request->getParam("systemIDSineFreqEnd")->value();
      settingWrite(NK_systemIDSineFreqEnd, inputMessage.c_str());
      systemIDSineFreqEnd = inputMessage.toFloat();
    }
    else if (request->hasParam("systemIDPlantTauMs")) {
      // Fitted plant tau from the dashboard Plant Delay sweep. Persisted with no field-off gate
      // (cheap compare-first scalar) so the actionable-disturbance readout survives reboots.
      foundParameter = true;
      inputMessage = request->getParam("systemIDPlantTauMs")->value();
      systemIDPlantTauMs = (uint16_t)inputMessage.toInt();
      settingWrite(NK_sysidPlantTau, String(systemIDPlantTauMs).c_str());
    }
    else if (request->hasParam("systemIDSineCycles")) {
      foundParameter = true;
      inputMessage = request->getParam("systemIDSineCycles")->value();
      settingWrite(NK_systemIDSineCycles, inputMessage.c_str());
      systemIDSineCycles = (uint8_t)inputMessage.toInt();
    }
    else if (request->hasParam("SystemIDStabilizeAmps")) {
      foundParameter = true;
      inputMessage = request->getParam("SystemIDStabilizeAmps")->value();
      settingWrite(NK_SystemIDStabilizeAmps, inputMessage.c_str());
      SystemIDStabilizeAmps = inputMessage.toFloat();
    }

    else if (request->hasParam("startSystemID")) {
      foundParameter = true;
      bool sysidModeOK = (sysMode == SYS_MODE_AUTO);
      // Mutex: refuse if any square-wave tuning test is already on. All four tests must run independently.
      const char *activeTuning = TuningMode ? "Current tuning"
                                            : (CVTuningMode ? "Voltage tuning"
                                                            : (fieldCurveActive != 0 ? "Field curve" : nullptr));
      if (sysMode == SYS_MODE_MANUAL) {
        queueConsoleMessage("SystemID: start blocked — not allowed in manual mode (duty is fixed; test cannot drive the field)");
      } else if (!sysidModeOK) {
        queueConsoleMessage("SystemID: start blocked — only allowed in AUTO mode (bulk, absorption, float, or target voltage)");
      } else if (activeTuning != nullptr) {
        queueConsoleMessageF("SystemID: start blocked — %s is active. Turn it off before running the Plant Delay Test.", activeTuning);
      } else if (systemIDActive == 0 && (millis() - systemIDLastEndMs) > 2000UL) {
        systemIDRequested = true;
        systemIDResultsReady = false;
        systemIDAbortRequested = false;   // clear any stale abort from a prior run
        systemIDAbortReason = 0;          // clear prior abort reason so UI doesn't show stale value
        systemIDAbortPhase = 0;
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

    // ── Auto-commissioning: field-% curve (Phase 1a) ────────────────────────
    else if (request->hasParam("startFieldCurve")) {
      foundParameter = true;
      const char *busy = (systemIDActive != 0) ? "Plant Delay test"
                         : TuningMode ? "Current tuning"
                         : CVTuningMode ? "Voltage tuning"
                         : (fieldCurveActive != 0) ? "Field curve" : nullptr;
      if (sysMode != SYS_MODE_AUTO) {
        queueConsoleMessage("Field curve: start blocked — only allowed in AUTO mode");
      } else if (busy != nullptr) {
        queueConsoleMessageF("Field curve: start blocked — %s is active", busy);
      } else if ((millis() - fieldCurveLastEndMs) > 2000UL) {
        fieldCurveOnsetMode = false;  // saturation sweep (SystemID amplitudes)
        fieldCurveRequested = true;
        fieldCurveResultsReady = false;
        fieldCurveAbortRequested = false;
        fieldCurveAbortReason = 0; fieldCurveAbortMsg[0] = '\0';  // clear prior abort latch
        queueConsoleMessage("Field curve: requested via web UI");
      } else {
        queueConsoleMessage("Field curve: start ignored (cooldown)");
      }
    }
    else if (request->hasParam("cancelFieldCurve")) {
      foundParameter = true;
      fieldCurveAbortRequested = true;
      queueConsoleMessage("Field curve: abort requested via web UI");
    }

    // ── Min% onset-knee sweep (commissioning) ───────────────────────────────
    // Reuses the field-curve ramp in onset-stop mode (stops at first current). Each completed
    // sweep is committed as an anchor; applyKneeCurve fits the Min% column across the anchors.
    else if (request->hasParam("startKneeSweep")) {
      foundParameter = true;
      const char *busy = (systemIDActive != 0) ? "Plant Delay test"
                         : TuningMode ? "Current tuning"
                         : CVTuningMode ? "Voltage tuning"
                         : (fieldCurveActive != 0) ? "Field curve" : nullptr;
      if (sysMode != SYS_MODE_AUTO) {
        queueConsoleMessage("Min% knee: start blocked — only allowed in AUTO mode");
      } else if (busy != nullptr) {
        queueConsoleMessageF("Min%% knee: start blocked — %s is active", busy);
      } else if ((millis() - fieldCurveLastEndMs) > 2000UL) {
        fieldCurveOnsetMode = true;  // onset-stop sweep (Min% floor)
        fieldCurveRequested = true;
        fieldCurveResultsReady = false;
        fieldCurveAbortRequested = false;
        fieldCurveAbortReason = 0; fieldCurveAbortMsg[0] = '\0';  // clear prior abort latch
        queueConsoleMessage("Min% knee: sweep requested via web UI");
      } else {
        queueConsoleMessage("Min% knee: start ignored (cooldown)");
      }
    }
    else if (request->hasParam("cancelKneeSweep")) {
      foundParameter = true;
      fieldCurveAbortRequested = true;
      queueConsoleMessage("Min% knee: abort requested via web UI");
    }
    else if (request->hasParam("addKneeAnchor")) {
      foundParameter = true;
      if (kneeSweepKneeDuty <= 0.0f) {
        queueConsoleMessage("Min% knee: no sweep result to commit");
      } else if (kneeAnchorN >= KNEE_ANCHOR_MAX) {
        queueConsoleMessage("Min% knee: anchor list full");
      } else {
        kneeAnchorRPM[kneeAnchorN] = kneeSweepRPM;
        kneeAnchorDuty[kneeAnchorN] = kneeSweepKneeDuty;
        kneeAnchorTempF[kneeAnchorN] = kneeSweepTempF;
        kneeAnchorN++;
        // Refresh the live onset = a + C/RPM fit so /kneesweep.json (the review screen) can flag a bad
        // point before Apply. Needs >= 2 anchors; leaves residual = -1 until then.
        {
          float a, C, resid; int wi;
          if (kneeFitModel(a, C, resid, wi)) { kneeFitA = a; kneeFitC = C; kneeFitResidPct = resid; kneeFitWorstIdx = wi; }
          else { kneeFitResidPct = -1.0f; kneeFitWorstIdx = -1; }
        }
        queueConsoleMessageF("Min%% knee: anchor %d committed (%.1f%% @ %.0f RPM, %.0fF)",
                             kneeAnchorN, kneeSweepKneeDuty, kneeSweepRPM, kneeSweepTempF);
      }
    }
    else if (request->hasParam("clearKneeAnchors")) {
      foundParameter = true;
      kneeAnchorN = 0;
      kneeFitResidPct = -1.0f; kneeFitWorstIdx = -1; kneeFitA = 0.0f; kneeFitC = 0.0f;
      queueConsoleMessage("Min% knee: anchors cleared");
    }
    else if (request->hasParam("applyKneeCurve")) {
      foundParameter = true;
      if (!kneeCurveApply())
        queueConsoleMessage("Min% knee: need at least 3 anchors to fit a curve");
    }

    // ── Auto-commissioning: Phase-2 relaxed matrix gate ─────────────────────
    else if (request->hasParam("faCommissionGate")) {
      foundParameter = true;
      faCommissionGate = (request->getParam("faCommissionGate")->value().toInt() != 0);
      queueConsoleMessageF("Commissioning matrix gate %s", faCommissionGate ? "RELAXED (Phase 2)" : "strict");
    }

    // ── RPM ripple table game-fill window (RPM_RIPPLE_TABLE_SPEC §2.2) ──
    // Arm = wipe + stamp + open the fold (deferred to Core 1); disarm = freeze + persist. The browser
    // sends ripGameIdleRpm and the resTest level BEFORE arming — the wipe stamps both into the session.
    else if (request->hasParam("ripGameFill")) {
      foundParameter = true;
      bool on = (request->getParam("ripGameFill")->value().toInt() != 0);
      if (on) {
        ripTabPendingWipe = true;
      } else {
        ripGameFill = false;
        ripTabPendingWipe = false;  // cancel an arm that never executed
        ripTabPendingSave = true;
      }
      queueConsoleMessageF("RPM ripple sweep %s", on ? "started (table wiped)" : "ended (table frozen + saved)");
    }
    else if (request->hasParam("ripGameIdleRpm")) {
      foundParameter = true;
      long v = request->getParam("ripGameIdleRpm")->value().toInt();
      ripIdleRpmStage = (uint16_t)((v < 0) ? 0 : (v > 4000) ? 4000 : v);
    }

    // ── Active 3-current resonance test (COMMISSIONING_SPEC §3.2): arm = clear the ring + collect ──
    else if (request->hasParam("bcurRtest")) {
      foundParameter = true;
      bool on = (request->getParam("bcurRtest")->value().toInt() != 0);
      if (on) bcurRtestCount = 0;  // arm clears; windows append only when the §10 steadiness gates pass (both sensors)
      bcurRtestActive = on;
      queueConsoleMessageF("Resonance current test %s", on ? "ARMED (vary charge current)" : "stopped");
    }

    // ── Auto-commissioning: state machine ───────────────────────────────────
    else if (request->hasParam("commissionStart")) {
      foundParameter = true;
      // Take ownership of the protection flag for the wizard run: save the user's manual-tuning value
      // (only on a fresh start, not a resume — don't clobber the original) and force protections ON so the
      // tuning-tab Protections sliders can't leak a stray "off" into commissioning. Restored on done/abort.
      if (commissionState != 1) commissionProtBackup = testProtectionsEnabled;
      testProtectionsEnabled = true;
      commissionSnapshot();         // capture pre-commissioning tune for the abort path
      commissionSetState(1);        // IN_PROGRESS (held explicitly for the whole wizard run)
      commissionSetPhase(0);        // reset furthest-phase (the wizard bumps it forward per phase)
      commissionMarkStage(0);       // Prep complete: snapshot taken, preconditions checked
      // Wipe the live onset-knee FIT SCRATCH so the Min% floor step (step 8) starts the sweeps clean if it is run.
      // (This is only the in-session anchor/fit state — NOT the applied rpmMinDutyTable floors,
      // which are left intact so a partial re-run that skips Min% keeps its learned floors.)
      kneeAnchorN = 0;
      kneeFitResidPct = -1.0f; kneeFitWorstIdx = -1; kneeFitA = 0.0f; kneeFitC = 0.0f;
      settingsDirty = true;         // push the CSV3 state echo promptly
      queueConsoleMessage("Commissioning: started — settings snapshotted");
    }
    // Mark one wizard stage complete (i = 0=Prep…7=CV plant fit). Sets its done bit and clears
    // any downstream stage it feeds (coupling: see commissionDependentsMask). Drives the ✓ marks.
    else if (request->hasParam("commissionStageDone")) {
      foundParameter = true;
      int s = request->getParam("commissionStageDone")->value().toInt();
      commissionMarkStage(s);
      settingsDirty = true;
      queueConsoleMessageF("Commissioning: step %d complete", s + 1);
    }
    else if (request->hasParam("commissionAbort")) {
      foundParameter = true;
      faCommissionGate = false;
      // Abort is a teardown path too: committed cells from an aborted sweep are honest data captured
      // at level — freeze + persist, never discard (RPM_RIPPLE_TABLE_SPEC §2.2).
      if (ripGameFill || ripTabPendingWipe) { ripGameFill = false; ripTabPendingWipe = false; ripTabPendingSave = true; }
      testProtectionsEnabled = commissionProtBackup;  // restore the user's manual-tuning protection setting
      bool reverted = commissionRestore();  // revert every setting to the Phase-0 snapshot
      commissionSetState(0);                // NOT_COMMISSIONED
      commissionSetPhase(0);                // clear checklist progress
      commissionDoneMask = 0;               // revert also drops all per-stage completion
      commissionWriteDoneMask();
      settingsDirty = true;
      queueConsoleMessageF("Commissioning: aborted — %s",
                           reverted ? "settings reverted to the pre-commissioning snapshot"
                                    : "nothing to revert (no commissioning was in progress, so no pre-state was saved)");
    }
    else if (request->hasParam("commissionDone")) {
      foundParameter = true;
      faCommissionGate = false;
      if (ripGameFill || ripTabPendingWipe) { ripGameFill = false; ripTabPendingWipe = false; ripTabPendingSave = true; }
      testProtectionsEnabled = commissionProtBackup;  // restore the user's manual-tuning protection setting
      settingRemove(NK_commissionSnap);     // commit the new tune (no snapshot ⇒ reboot won't revert)
      commissionClearMinPctBackup();        // discard the Min% backup too — the new floors stay
      commissionRecomputeState();           // COMMISSIONED if every stage done, else IN_PROGRESS (partial)
      commissionSetPhase(COMMISSION_STAGE_COUNT);  // wizard pass finished (= one past the last step)
      settingsDirty = true;
      queueConsoleMessageF("Commissioning: pass finished — %s",
                           commissionDoneMask >= COMMISSION_ALL_DONE ? "all steps complete, device COMMISSIONED"
                                                                     : "partial — some steps still pending");
    }
    // Wizard heartbeat: persist the furthest phase reached so the tab checklist survives
    // a page reload / a different client. Clamped 0..COMMISSION_STAGE_COUNT (last value = finished).
    // Does not change commissionState.
    else if (request->hasParam("commissionPhase")) {
      foundParameter = true;
      int p = request->getParam("commissionPhase")->value().toInt();
      if (p < 0) p = 0; if (p > COMMISSION_STAGE_COUNT) p = COMMISSION_STAGE_COUNT;
      commissionSetPhase((uint8_t)p);
      settingsDirty = true;
    }
    // Commissioning idle-rest heartbeat. The open wizard pings =1 every ~2 s to keep the field "rested"
    // at a low duty between steps; on a clean close it pings =0 to drop the hold and resume charging
    // immediately. A missing ping (crash / Wi-Fi drop) goes stale on its own after the firmware timeout.
    else if (request->hasParam("commissionHeartbeat")) {
      foundParameter = true;
      lastCommissionHeartbeatMs = (request->getParam("commissionHeartbeat")->value().toInt() != 0)
                                  ? millis() : 0;   // 0 = explicit exit → stale now → charging resumes
    }

    if (request->hasParam("TemperatureLimitF")) {
      foundParameter = true;
      inputMessage = request->getParam("TemperatureLimitF")->value();
      settingWrite(NK_TemperatureLimitF, inputMessage.c_str());
      TemperatureLimitF = inputMessage.toInt();
    }
    // Cold-charge lockout (lithium protection) — master on/off. Turning it OFF is the user "override".
    if (request->hasParam("coldChargeLockoutEnable")) {
      foundParameter = true;
      inputMessage = request->getParam("coldChargeLockoutEnable")->value();
      coldChargeLockoutEnable = inputMessage.toInt() != 0;
      settingWrite(NK_coldChargeLockoutEnable, String((int)coldChargeLockoutEnable).c_str());
      queueConsoleMessageF("Cold-charge lockout (board-temp battery proxy): %s", coldChargeLockoutEnable ? "ENABLED" : "DISABLED");
    }
    // Cold-charge lockout temperature floor (board temp °F, stored raw)
    if (request->hasParam("MinChargeTempF")) {
      foundParameter = true;
      inputMessage = request->getParam("MinChargeTempF")->value();
      settingWrite(NK_MinChargeTempF, inputMessage.c_str());
      MinChargeTempF = inputMessage.toFloat();
    }
    if (request->hasParam("ClearBuffer")) {
      foundParameter = true;
      clearSensorBuffer();
      queueConsoleMessage("Upload buffer manually cleared from web");
      inputMessage = "1";
    }
    else if (request->hasParam("ResetAlternatorHealth")) {
      foundParameter = true;
      pendingResetAlternatorHealth = true;  // deferred to Core 1 to avoid SSE gap (Start Over)
    }
    else if (request->hasParam("FastAltClearMatrix")) {
      foundParameter = true;
      faPendingMatrixClear = true;  // deferred to Core 1 (flash remove) — fast alt-current disturbance matrix wipe
    }
    else if (request->hasParam("ResetRipplePeaks")) {
      // Ripple analyzer's own worst-value reset (Session Worst Pk-Pk / Worst Peak / Worst Hz).
      // These persist across reboot, so clear + commit now; prev_* shadows left alone so the
      // cleared values actually write (prev != 0 → saveNVSDataFull writes).
      foundParameter = true;
      faSesPkpkWorstA = 0.0f;
      faSesPkpkAmpsA = 0.0f;     // clear the pk-pk operating-point context too
      faSesPkpkTempF = NAN;
      faSesPkpkRpm = 0;
      faSesPkpkEpoch = 0;
      faSesPeakWorstA = 0.0f;
      faSesPeakWorstHz = 0.0f;
      faDomReset();  // Highest Tone in Map headline clears with the other worsts
      nvsPersistNow = true;
      queueConsoleMessage("Ripple analyzer worst values: Reset requested from web interface");
    }
    else if (request->hasParam("FastAltRebaseline")) {
      foundParameter = true;
      faPendingRebaseline = true;  // deferred to Core 1 — clears the reference flipbook (freeze-once pages re-capture)
    }
    // Fast alt-current diagnostic knobs (Pattern B). Globals are updated live so faDrain()
    // and the steady-state gate react immediately; no reboot needed (faEnabled toggles the driver).
    else if (request->hasParam("faEnabled")) {
      foundParameter = true;
      faEnabled = (request->getParam("faEnabled")->value().toInt() != 0);
      settingWrite(NK_faEnabled, faEnabled ? "1" : "0");
    }
    else if (request->hasParam("faAlarmEnable")) {
      foundParameter = true;
      faAlarmEnable = (request->getParam("faAlarmEnable")->value().toInt() != 0);
      settingWrite(NK_faAlarmEnable, faAlarmEnable ? "1" : "0");
    }
    else if (request->hasParam("faAnomPause")) {
      foundParameter = true;
      faAnomPause = (request->getParam("faAnomPause")->value().toInt() != 0);
      settingWrite(NK_faAnomPause, faAnomPause ? "1" : "0");
    }
    else if (request->hasParam("faRpmEdgeMargin")) {
      foundParameter = true;
      faRpmEdgeMargin = request->getParam("faRpmEdgeMargin")->value().toFloat();
      settingWrite(NK_faRpmEdgeMargin, String(faRpmEdgeMargin, 1).c_str());
    }
    else if (request->hasParam("faAmpsDriftFloorA")) {
      foundParameter = true;
      faAmpsDriftFloorA = request->getParam("faAmpsDriftFloorA")->value().toFloat();
      settingWrite(NK_faAmpsDriftFloorA, String(faAmpsDriftFloorA, 2).c_str());
    }
    else if (request->hasParam("faAmpsDriftPct")) {
      foundParameter = true;
      faAmpsDriftPct = request->getParam("faAmpsDriftPct")->value().toFloat();
      settingWrite(NK_faAmpsDriftPct, String(faAmpsDriftPct, 1).c_str());
    }
    // Measured-ripple capture admission gates (§10.8/§11) — own knobs, decoupled from the fa* detector gates
    else if (request->hasParam("ripWinMs")) {
      foundParameter = true;
      // Floor 500 ms: each HALF-window (§11 stationarity/min-of-halves) must hold ≥1 cycle of the lowest
      // resolvable disturbance; ceiling 4 s bounds capture latency. Also sizes the min-sample gate.
      ripWinMs = clamp_f(request->getParam("ripWinMs")->value().toFloat(), 500.0f, 4000.0f);
      settingWrite(NK_ripWinMs, String(ripWinMs, 0).c_str());
      queueConsoleMessage("Ripple capture window changed — stored map/fit values from the old window are not comparable (clear map + re-run current check)");
    }
    else if (request->hasParam("ripDriftFloorA")) {
      foundParameter = true;
      ripDriftFloorA = clamp_f(request->getParam("ripDriftFloorA")->value().toFloat(), 0.0f, 50.0f);
      settingWrite(NK_ripDriftFloorA, String(ripDriftFloorA, 2).c_str());
    }
    else if (request->hasParam("ripDriftPct")) {
      foundParameter = true;
      ripDriftPct = clamp_f(request->getParam("ripDriftPct")->value().toFloat(), 0.0f, 50.0f);
      settingWrite(NK_ripDriftPct, String(ripDriftPct, 1).c_str());
    }
    else if (request->hasParam("faAttenUpAmps")) {
      foundParameter = true;
      faAttenUpAmps = request->getParam("faAttenUpAmps")->value().toFloat();
      settingWrite(NK_faAttenUpAmps, String(faAttenUpAmps, 1).c_str());
    }
    else if (request->hasParam("faAttenDownAmps")) {
      foundParameter = true;
      faAttenDownAmps = request->getParam("faAttenDownAmps")->value().toFloat();
      settingWrite(NK_faAttenDownAmps, String(faAttenDownAmps, 1).c_str());
    }
    else if (request->hasParam("faPeakMinA")) {
      foundParameter = true;
      faPeakMinA = request->getParam("faPeakMinA")->value().toFloat();
      settingWrite(NK_faPeakMinA, String(faPeakMinA, 2).c_str());
    }
    // ---- Auto Min% learning ("knee tracker") knobs ----
    else if (request->hasParam("kneeLearnEnable")) {
      foundParameter = true;
      kneeLearnEnable = (request->getParam("kneeLearnEnable")->value().toInt() != 0);
      settingWrite(NK_kneeLearnEnable, kneeLearnEnable ? "1" : "0");
      if (kneeLearnEnable) {
        // Take ownership of the Min% column immediately from the learned floors (bin 0 always 0%).
        for (int i = 0; i < RPM_TABLE_SIZE; i++) {
          float f = (i == 0) ? 0.0f : kneeFloor[i];
          if (f < 0) f = 0; if (f > kneeMaxFloorPct) f = kneeMaxFloorPct;
          rpmMinDutyTable[i] = f;
        }
      }
    }
    else if (request->hasParam("kneeMarginPct")) {
      foundParameter = true;
      kneeMarginPct = request->getParam("kneeMarginPct")->value().toFloat();
      settingWrite(NK_kneeMarginPct, String(kneeMarginPct, 2).c_str());
    }
    else if (request->hasParam("kneeOnsetA")) {
      foundParameter = true;
      kneeOnsetA = request->getParam("kneeOnsetA")->value().toFloat();
      settingWrite(NK_kneeOnsetA, String(kneeOnsetA, 2).c_str());
    }
    else if (request->hasParam("kneeReArmA")) {
      foundParameter = true;
      kneeReArmA = request->getParam("kneeReArmA")->value().toFloat();
      settingWrite(NK_kneeReArmA, String(kneeReArmA, 2).c_str());
    }
    else if (request->hasParam("kneeDwellSec")) {
      foundParameter = true;
      kneeDwellSec = request->getParam("kneeDwellSec")->value().toFloat();
      settingWrite(NK_kneeDwellSec, String(kneeDwellSec, 1).c_str());
    }
    else if (request->hasParam("kneeStepPct")) {
      foundParameter = true;
      kneeStepPct = request->getParam("kneeStepPct")->value().toFloat();
      settingWrite(NK_kneeStepPct, String(kneeStepPct, 2).c_str());
    }
    else if (request->hasParam("kneeTempComp")) {
      foundParameter = true;
      kneeTempComp = (request->getParam("kneeTempComp")->value().toInt() != 0);
      settingWrite(NK_kneeTempComp, kneeTempComp ? "1" : "0");
    }
    else if (request->hasParam("kneeTempRefF")) {
      foundParameter = true;
      kneeTempRefF = request->getParam("kneeTempRefF")->value().toFloat();
      settingWrite(NK_kneeTempRefF, String(kneeTempRefF, 1).c_str());
    }
    else if (request->hasParam("kneeMaxFloorPct")) {
      foundParameter = true;
      kneeMaxFloorPct = request->getParam("kneeMaxFloorPct")->value().toFloat();
      settingWrite(NK_kneeMaxFloorPct, String(kneeMaxFloorPct, 2).c_str());
    }
    else if (request->hasParam("kneeRpmTolPct")) {
      foundParameter = true;
      kneeRpmTolPct = request->getParam("kneeRpmTolPct")->value().toFloat();
      settingWrite(NK_kneeRpmTolPct, String(kneeRpmTolPct, 1).c_str());
    }
    else if (request->hasParam("kneeTempTolF")) {
      foundParameter = true;
      kneeTempTolF = request->getParam("kneeTempTolF")->value().toFloat();
      settingWrite(NK_kneeTempTolF, String(kneeTempTolF, 1).c_str());
    }
    else if (request->hasParam("kneeDutyTolPct")) {
      foundParameter = true;
      kneeDutyTolPct = request->getParam("kneeDutyTolPct")->value().toFloat();
      settingWrite(NK_kneeDutyTolPct, String(kneeDutyTolPct, 2).c_str());
    }
    else if (request->hasParam("ResetKneeLearn")) {
      foundParameter = true;
      kneeLearnResetDefaults();
    }
    // ---- Zero-drift characterization log (diagnostic) ----
    else if (request->hasParam("ZeroLogEnable")) {
      foundParameter = true;
      ZeroLogEnable = (request->getParam("ZeroLogEnable")->value().toInt() != 0);
      settingWrite(NK_ZeroLogEnable, ZeroLogEnable ? "1" : "0");
    }
    else if (request->hasParam("ResetZeroLog")) {
      foundParameter = true;
      zeroLogResetAll();
    }
    else if (request->hasParam("wifiNapEnabled")) {
      foundParameter = true;
      wifiNapEnabled = (request->getParam("wifiNapEnabled")->value().toInt() != 0);
      settingWrite(NK_wifiNapEnabled, wifiNapEnabled ? "1" : "0");
    }
    else if (request->hasParam("altSimMode")) {
      foundParameter = true;
      altSimMode = request->getParam("altSimMode")->value().toFloat();  // bench simulator (not persisted)
    }
    // Alternator-health steady-state / window / record settings (registry-driven)
    if (altSettingsHandle(request)) {
      foundParameter = true;
      sendAltSettings();
    }
    else if (request->hasParam("ResetBoatPerformance")) {
      foundParameter = true;
      pendingResetBoatPerformance = true;  // deferred to Core 1 to avoid SSE gap (Clear all data)
    }
    else if (request->hasParam("perfSimMode")) {
      foundParameter = true;
      perfSimMode = request->getParam("perfSimMode")->value().toFloat();  // bench NMEA simulator (not persisted)
    }
    // Boat-performance polar settings (registry-driven, incl. sticky Pause + speed-source selector)
    if (perfSettingsHandle(request)) {
      foundParameter = true;
      sendPerfSettings();
    }

    if (request->hasParam("ManualDutyTarget")) {
      foundParameter = true;
      inputMessage = request->getParam("ManualDutyTarget")->value();
      settingWrite(NK_ManualDutyTarget, inputMessage.c_str());
      ManualDutyTarget = inputMessage.toInt();
    }
    if (request->hasParam("socInfoAvailable")) {
      foundParameter = true;
      inputMessage = request->getParam("socInfoAvailable")->value();
      settingWrite(NK_socInfoAvailable, inputMessage.c_str());
      socInfoAvailable = inputMessage.toInt();
    }
    if (request->hasParam("TailCurrent_A")) {
      foundParameter = true;
      inputMessage = request->getParam("TailCurrent_A")->value();
      settingWrite(NK_TailCurrent_A, inputMessage.c_str());
      TailCurrent_A = inputMessage.toFloat();
    }
    if (request->hasParam("RebulkVoltage")) {
      foundParameter = true;
      inputMessage = request->getParam("RebulkVoltage")->value();
      settingWrite(NK_RebulkVoltage, inputMessage.c_str());
      RebulkVoltage = inputMessage.toFloat();
    }
    if (request->hasParam("rebulkDebounceTime")) {
      foundParameter = true;
      inputMessage = request->getParam("rebulkDebounceTime")->value();
      rebulkDebounceTime = (uint32_t)inputMessage.toInt() * 1000UL;  // sec → ms
      // Save the ms value so boot load is consistent
      settingWrite(NK_rebulkDebounceTime, String(rebulkDebounceTime).c_str());
    }

    if (request->hasParam("MinFloatTime")) {
      foundParameter = true;
      inputMessage = request->getParam("MinFloatTime")->value();
      MinFloatTime = (uint32_t)inputMessage.toInt() * 60000UL;  // min → ms
      settingWrite(NK_MinFloatTime, String(MinFloatTime).c_str());
    }
    if (request->hasParam("SOC_BlockRebulk_percent")) {
      foundParameter = true;
      inputMessage = request->getParam("SOC_BlockRebulk_percent")->value();
      settingWrite(NK_SOC_BlockRebulk_percent, inputMessage.c_str());
      SOC_BlockRebulk_percent = inputMessage.toInt();
    }
    if (request->hasParam("SOC_AllowRebulk_percent")) {
      foundParameter = true;
      inputMessage = request->getParam("SOC_AllowRebulk_percent")->value();
      settingWrite(NK_SOC_AllowRebulk_percent, inputMessage.c_str());
      SOC_AllowRebulk_percent = inputMessage.toInt();
    }
    if (request->hasParam("BulkVoltage")) {
      foundParameter = true;
      inputMessage = request->getParam("BulkVoltage")->value();
      settingWrite(NK_BulkVoltage, inputMessage.c_str());
      BulkVoltage = inputMessage.toFloat();
      updateINA228OvervoltageThreshold();  // important!  update the hardware overvoltage limit provided by INA228
    }
    // NOTE: system voltage (12/24/48V) is NOT a /get setting — it lives in Vessel Info and the whole
    // class-change rescale (charge profile, hard-shutdown, INA228 OV, normalized CV/CC gains) runs in
    // the /saveVesselInfo handler via applyNominalVoltageChange(). BATTERY_VOLTAGE is the sole source.
    if (request->hasParam("wavePeriod")) {
      foundParameter = true;
      inputMessage = request->getParam("wavePeriod")->value();
      settingWrite(NK_wavePeriod, inputMessage.c_str());
      wavePeriod = inputMessage.toInt();
      if (TuningMode) tuningParamChanged = true;
    }
    if (request->hasParam("tuningWaveform")) {
      foundParameter = true;
      inputMessage = request->getParam("tuningWaveform")->value();
      settingWrite(NK_tuningWaveform, inputMessage.c_str());
      tuningWaveform = inputMessage.toInt();
      if (TuningMode) tuningParamChanged = true;
    }
    if (request->hasParam("tuningSineFreq")) {
      foundParameter = true;
      inputMessage = request->getParam("tuningSineFreq")->value();
      settingWrite(NK_tuningSineFreq, inputMessage.c_str());
      tuningSineFreq = inputMessage.toFloat();
    }
    if (request->hasParam("tuningSweepStart")) {
      foundParameter = true;
      inputMessage = request->getParam("tuningSweepStart")->value();
      settingWrite(NK_tuningSweepStart, inputMessage.c_str());
      tuningSweepStart = inputMessage.toFloat();
    }
    if (request->hasParam("tuningSweepEnd")) {
      foundParameter = true;
      inputMessage = request->getParam("tuningSweepEnd")->value();
      settingWrite(NK_tuningSweepEnd, inputMessage.c_str());
      tuningSweepEnd = inputMessage.toFloat();
    }
    if (request->hasParam("tuningSweepCycles")) {
      foundParameter = true;
      inputMessage = request->getParam("tuningSweepCycles")->value();
      settingWrite(NK_tuningSweepCycles, inputMessage.c_str());
      tuningSweepCycles = (uint8_t)inputMessage.toInt();
    }
    if (request->hasParam("startTuningSweep")) {
      foundParameter = true;
      if (systemIDActive != 0 || fieldCurveActive != 0) {
        queueConsoleMessage("Tuning sweep: start blocked — a SystemID/Field-curve test is active");
      } else {
        tuningSweepRequested = true;   // momentary — the TuningMode sine block consumes it
      }
    }
    if (request->hasParam("SwitchingFrequency")) {
      foundParameter = true;
      inputMessage = request->getParam("SwitchingFrequency")->value();
      int requestedFreq = inputMessage.toInt();
      settingWrite(NK_SwitchingFrequency, String(requestedFreq).c_str());
      SwitchingFrequency = requestedFreq;
      queueConsoleMessageF("Frequency target set to %dHz", SwitchingFrequency);
    }
    if (request->hasParam("FloatVoltage")) {
      foundParameter = true;
      inputMessage = request->getParam("FloatVoltage")->value();
      settingWrite(NK_FloatVoltage, inputMessage.c_str());
      FloatVoltage = inputMessage.toFloat();
    }
    if (request->hasParam("yyMin")) {
      foundParameter = true;
      inputMessage = request->getParam("yyMin")->value();
      settingWrite(NK_yyMin, inputMessage.c_str());
      yyMin = inputMessage.toInt();
    }
    if (request->hasParam("FieldAdjustmentInterval")) {
      foundParameter = true;
      inputMessage = request->getParam("FieldAdjustmentInterval")->value();
      settingWrite(NK_FieldAdjustmentInterval, inputMessage.c_str());
      FieldAdjustmentInterval = inputMessage.toFloat();
    }
    if (request->hasParam("ManualFieldToggle")) {
      foundParameter = true;
      inputMessage = request->getParam("ManualFieldToggle")->value();
      settingWrite(NK_ManualFieldToggle, inputMessage.c_str());
      ManualFieldToggle = inputMessage.toInt();
    }
    if (request->hasParam("capLimitMode")) {
      foundParameter = true;
      inputMessage = request->getParam("capLimitMode")->value();
      settingWrite(NK_capLimitMode, inputMessage.c_str());
      capLimitMode = constrain(inputMessage.toInt(), 0, 1);
    }
    if (request->hasParam("SwitchControlOverride")) {
      foundParameter = true;
      inputMessage = request->getParam("SwitchControlOverride")->value();
      settingWrite(NK_SwitchControlOverride, inputMessage.c_str());
      SwitchControlOverride = inputMessage.toInt();
    }
    if (request->hasParam("MaintainMode")) {
      foundParameter = true;
      inputMessage = request->getParam("MaintainMode")->value();
      MaintainMode = inputMessage.toInt();
      settingWrite(NK_MaintainMode, inputMessage.c_str());
      if (MaintainMode) {
        // MaintainMode and TargetVoltageMode are mutually exclusive — clear the other.
        TargetVoltageMode = 0;
        settingWrite(NK_TargetVoltageMode, "0");
      }
      queueConsoleMessageF("MaintainMode mode %s", MaintainMode ? "enabled" : "disabled");
    }
    if (request->hasParam("TargetVoltageMode")) {
      foundParameter = true;
      inputMessage = request->getParam("TargetVoltageMode")->value();
      TargetVoltageMode = inputMessage.toInt();
      settingWrite(NK_TargetVoltageMode, inputMessage.c_str());
      if (TargetVoltageMode) {
        // MaintainMode and TargetVoltageMode are mutually exclusive — clear the other.
        MaintainMode = 0;
        settingWrite(NK_MaintainMode, "0");
      }
      queueConsoleMessageF("TargetVoltageMode %s", TargetVoltageMode ? "enabled" : "disabled");
    }
    if (request->hasParam("OnOff")) {
      // NOTE: OnOff==0 already caused an early return above, so this
      // branch only runs for OnOff==1 (or any non-zero value) after
      // password validation has passed.
      foundParameter = true;
      inputMessage = request->getParam("OnOff")->value();
      settingWrite(NK_OnOff, inputMessage.c_str());
      OnOff = inputMessage.toInt();
    }
    if (request->hasParam("HiLow")) {
      foundParameter = true;
      inputMessage = request->getParam("HiLow")->value();
      int newMode = inputMessage.toInt();
      if (newMode != HiLow) {
        HiLow = newMode;
        settingWrite(NK_HiLow, inputMessage.c_str());
        loadCapTablesForMode(HiLow);  // swap active cap tables to match new mode
        // Do NOT deactivate the thermal loop here. The new cap is already honored: the
        // velocity-form penalty update clamps to the LIVE capCurrent (which tracks the new
        // table) every tick, so the penalty re-bounds to the new cap immediately. Setting
        // tempPIDActive=false would instead force the re-enable path, which CLEARS the slope
        // buffer — that restarts the 60s warmup window and drops the setpoint to limit-20 (the
        // spurious 20°F reduction seen on a Lo<->Hi switch), and also re-seeds the penalty
        // accumulator from P-only, dumping the learned holding level. A mode switch must be
        // bumpless for the thermal loop.
        stateRevision++;              // force immediate CSVData echo of new table values
        if (HiLow == 0) {
          // Switching to Low drops the ceiling. Capture the present ceiling and arm the glide so the
          // control loop ramps it down to the new Low cap instead of stepping (prevents the iExcess
          // false-trip on the deliberate command drop). Up-switches don't glide — the loop lets them rise.
          modeCapSlew = uTargetAmps;
          modeCapSlewActive = true;
        }
        queueConsoleMessageF("Charge rate mode: switched to %s", HiLow == 1 ? "Normal" : "Low");
      }
    }
    if (request->hasParam("InvertAltAmps")) {
      foundParameter = true;
      inputMessage = request->getParam("InvertAltAmps")->value();
      settingWrite(NK_InvertAltAmps, inputMessage.c_str());
      InvertAltAmps = inputMessage.toInt();
    }
    if (request->hasParam("InvertBattAmps")) {
      foundParameter = true;
      inputMessage = request->getParam("InvertBattAmps")->value();
      settingWrite(NK_InvertBattAmps, inputMessage.c_str());
      InvertBattAmps = inputMessage.toInt();
    }
    if (request->hasParam("MaxDuty")) {
      foundParameter = true;
      inputMessage = request->getParam("MaxDuty")->value();
      settingWrite(NK_MaxDuty, inputMessage.c_str());
      MaxDuty = inputMessage.toInt();
      if (pidInitialized) {
        currentPID.SetOutputLimits(MinDuty, MaxDuty);
      }
      queueConsoleMessageF("Max Duty updated to: %d%%", MaxDuty);
    }
    if (request->hasParam("MinDuty")) {
      foundParameter = true;
      inputMessage = request->getParam("MinDuty")->value();
      settingWrite(NK_MinDuty, inputMessage.c_str());
      MinDuty = inputMessage.toInt();
      if (pidInitialized) {
        currentPID.SetOutputLimits(MinDuty, MaxDuty);
      }
      queueConsoleMessageF("Min Duty updated to: %d%%", MinDuty);
    }
    if (request->hasParam("LimpHome")) {
      foundParameter = true;
      inputMessage = request->getParam("LimpHome")->value();
      settingWrite(NK_LimpHome, inputMessage.c_str());
      LimpHome = inputMessage.toInt();
    }
    if (request->hasParam("VeData")) {
      foundParameter = true;
      inputMessage = request->getParam("VeData")->value();
      settingWrite(NK_VeData, inputMessage.c_str());
      VeData = inputMessage.toInt();
    }
    if (request->hasParam("NMEA0183Data")) {
      foundParameter = true;
      inputMessage = request->getParam("NMEA0183Data")->value();
      settingWrite(NK_NMEA0183Data, inputMessage.c_str());
      NMEA0183Data = inputMessage.toInt();
    }
    if (request->hasParam("NMEA2KData")) {
      foundParameter = true;
      inputMessage = request->getParam("NMEA2KData")->value();
      settingWrite(NK_NMEA2KData, inputMessage.c_str());
      NMEA2KData = inputMessage.toInt();
    }
    if (request->hasParam("waveAmplitude")) {
      foundParameter = true;
      inputMessage = request->getParam("waveAmplitude")->value();
      settingWrite(NK_waveAmplitude, inputMessage.c_str());
      waveAmplitude = inputMessage.toInt();
      if (TuningMode) tuningParamChanged = true;
    }
    if (request->hasParam("tuningWaveFloor")) {
      foundParameter = true;
      inputMessage = request->getParam("tuningWaveFloor")->value();
      settingWrite(NK_tuningWaveFloor, inputMessage.c_str());
      tuningWaveFloor = inputMessage.toInt();
      if (TuningMode) tuningParamChanged = true;
    }
    if (request->hasParam("CurrentThreshold")) {
      foundParameter = true;
      inputMessage = request->getParam("CurrentThreshold")->value();
      settingWrite(NK_CurrentThreshold, inputMessage.c_str());
      CurrentThreshold = inputMessage.toFloat();
    }
    if (request->hasParam("PeukertExponent")) {
      foundParameter = true;
      inputMessage = request->getParam("PeukertExponent")->value();
      PeukertExponent_scaled = (int)(inputMessage.toFloat() * 100);
      settingWrite(NK_PeukertExponent, String(PeukertExponent_scaled).c_str());
    }
    if (request->hasParam("ChargeEfficiency")) {
      foundParameter = true;
      inputMessage = request->getParam("ChargeEfficiency")->value();
      settingWrite(NK_ChargeEfficiency, inputMessage.c_str());
      ChargeEfficiency_scaled = (int)round(inputMessage.toFloat() * 10);  // store as % × 10
    }
    if (request->hasParam("ChargedVoltage")) {
      foundParameter = true;
      inputMessage = request->getParam("ChargedVoltage")->value();
      ChargedVoltage_Scaled = (int)(inputMessage.toFloat() * 100);
      settingWrite(NK_ChargedVoltage, String(ChargedVoltage_Scaled).c_str());
    }
    if (request->hasParam("TailCurrent")) {
      foundParameter = true;
      inputMessage = request->getParam("TailCurrent")->value();
      settingWrite(NK_TailCurrent, inputMessage.c_str());
      TailCurrent = inputMessage.toFloat();
    }
    if (request->hasParam("ChargedDetectionTime")) {
      foundParameter = true;
      inputMessage = request->getParam("ChargedDetectionTime")->value();
      settingWrite(NK_ChargedDetectionTime, inputMessage.c_str());
      ChargedDetectionTime = inputMessage.toInt();
    }
    if (request->hasParam("IgnoreTemperature")) {
      foundParameter = true;
      inputMessage = request->getParam("IgnoreTemperature")->value();
      settingWrite(NK_IgnoreTemperature, inputMessage.c_str());
      IgnoreTemperature = inputMessage.toInt();
    }
    if (request->hasParam("IgnoreRPM")) {
      foundParameter = true;
      inputMessage = request->getParam("IgnoreRPM")->value();
      settingWrite(NK_IgnoreRPM, inputMessage.c_str());
      IgnoreRPM = inputMessage.toInt();
    }
    if (request->hasParam("MinRPMForField")) {
      foundParameter = true;
      inputMessage = request->getParam("MinRPMForField")->value();
      settingWrite(NK_MinRPMForField, inputMessage.c_str());
      MinRPMForField = inputMessage.toInt();
    }
    if (request->hasParam("bmsLogic")) {
      foundParameter = true;
      inputMessage = request->getParam("bmsLogic")->value();
      settingWrite(NK_bmsLogic, inputMessage.c_str());
      bmsLogic = inputMessage.toInt();
    }
    if (request->hasParam("bmsLogicLevelOff")) {
      foundParameter = true;
      inputMessage = request->getParam("bmsLogicLevelOff")->value();
      settingWrite(NK_bmsLogicLevelOff, inputMessage.c_str());
      bmsLogicLevelOff = inputMessage.toInt();
    }
    if (request->hasParam("AlarmActivate")) {
      foundParameter = true;
      inputMessage = request->getParam("AlarmActivate")->value();
      settingWrite(NK_AlarmActivate, inputMessage.c_str());
      AlarmActivate = inputMessage.toInt();
    }
    if (request->hasParam("TempAlarm")) {
      foundParameter = true;
      inputMessage = request->getParam("TempAlarm")->value();
      settingWrite(NK_TempAlarm, inputMessage.c_str());
      TempAlarm = inputMessage.toInt();
    }
    if (request->hasParam("TempAlarmLow")) {
      foundParameter = true;
      inputMessage = request->getParam("TempAlarmLow")->value();
      settingWrite(NK_TempAlarmLow, inputMessage.c_str());
      TempAlarmLow = inputMessage.toInt();
    }
    if (request->hasParam("VoltageAlarmHigh")) {
      foundParameter = true;
      inputMessage = request->getParam("VoltageAlarmHigh")->value();
      settingWrite(NK_VoltageAlarmHigh, inputMessage.c_str());
      VoltageAlarmHigh = inputMessage.toFloat();
    }
    if (request->hasParam("VoltageAlarmLow")) {
      foundParameter = true;
      inputMessage = request->getParam("VoltageAlarmLow")->value();
      settingWrite(NK_VoltageAlarmLow, inputMessage.c_str());
      VoltageAlarmLow = inputMessage.toFloat();
    }
    if (request->hasParam("SocAlarmLow")) {
      foundParameter = true;
      inputMessage = request->getParam("SocAlarmLow")->value();
      SocAlarmLow = constrain(inputMessage.toInt(), 0, 100);
      settingWrite(NK_SocAlarmLow, String(SocAlarmLow).c_str());
    }
    if (request->hasParam("CurrentAlarmHigh")) {
      foundParameter = true;
      inputMessage = request->getParam("CurrentAlarmHigh")->value();
      settingWrite(NK_CurrentAlarmHigh, inputMessage.c_str());
      CurrentAlarmHigh = inputMessage.toInt();
    }
    if (request->hasParam("RPMScalingFactor")) {
      foundParameter = true;
      inputMessage = request->getParam("RPMScalingFactor")->value();
      settingWrite(NK_RPMScalingFactor, inputMessage.c_str());
      RPMScalingFactor = inputMessage.toInt();
    }
    if (request->hasParam("FieldResistance")) {
      foundParameter = true;
      inputMessage = request->getParam("FieldResistance")->value();
      settingWrite(NK_FieldResistance, inputMessage.c_str());
      FieldResistance = inputMessage.toFloat();
    }
    if (request->hasParam("AlternatorCOffset")) {
      foundParameter = true;
      inputMessage = request->getParam("AlternatorCOffset")->value();
      settingWrite(NK_AlternatorCOffset, inputMessage.c_str());
      AlternatorCOffset = inputMessage.toFloat();
    }
    if (request->hasParam("BatteryCOffset")) {
      foundParameter = true;
      inputMessage = request->getParam("BatteryCOffset")->value();
      settingWrite(NK_BatteryCOffset, inputMessage.c_str());
      BatteryCOffset = inputMessage.toFloat();
    }
    if (request->hasParam("AmpSensorRange")) {
      foundParameter = true;
      inputMessage = request->getParam("AmpSensorRange")->value();
      settingWrite(NK_AmpSensorRange, inputMessage.c_str());
      AmpSensorRange = inputMessage.toInt();
      queueConsoleMessageF("AmpSensorRange changed to: %d", AmpSensorRange);
    }
    if (request->hasParam("R_fixed")) {
      foundParameter = true;
      inputMessage = request->getParam("R_fixed")->value();
      settingWrite(NK_R_fixed, inputMessage.c_str());
      R_fixed = inputMessage.toFloat();
    }
    if (request->hasParam("Beta")) {
      foundParameter = true;
      inputMessage = request->getParam("Beta")->value();
      settingWrite(NK_Beta, inputMessage.c_str());
      Beta = inputMessage.toFloat();
    }
    if (request->hasParam("T0_C")) {
      foundParameter = true;
      inputMessage = request->getParam("T0_C")->value();
      settingWrite(NK_T0_C, inputMessage.c_str());
      T0_C = inputMessage.toFloat();
    }
    if (request->hasParam("TempSource")) {
      foundParameter = true;
      inputMessage = request->getParam("TempSource")->value();
      settingWrite(NK_TempSource, inputMessage.c_str());
      TempSource = inputMessage.toInt();
    }
    if (request->hasParam("IgnitionOverride")) {
      foundParameter = true;
      inputMessage = request->getParam("IgnitionOverride")->value();
      settingWrite(NK_IgnitionOverride, inputMessage.c_str());
      IgnitionOverride = inputMessage.toInt();
    }
    if (request->hasParam("AlarmLatchEnabled")) {
      foundParameter = true;
      inputMessage = request->getParam("AlarmLatchEnabled")->value();
      settingWrite(NK_AlarmLatchEnabled, inputMessage.c_str());
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
    if (request->hasParam("ZeroIMU")) {
      foundParameter = true;
      if (!imuEnabled) {
        queueConsoleMessage("IMU ZERO: rejected — IMU not enabled");
      } else if (imuZeroInProgress) {
        queueConsoleMessage("IMU ZERO: already in progress");
      } else {
        // Start a fresh capture window; offsets are written when N samples collected
        imuZeroAxSum = imuZeroAySum = imuZeroAzSum = 0;
        imuZeroGxSum = imuZeroGySum = imuZeroGzSum = 0;
        imuZeroAccelN = imuZeroGyroN = 0;
        imuZeroInProgress = true;
        imuZeroStartMs = millis();   // timeout reference so the capture always resolves
        queueConsoleMessage("IMU ZERO: hold still ~2s, capturing level reference");
      }
      inputMessage = "1";
    }
    if (request->hasParam("ClearIMUZero")) {
      foundParameter = true;
      imuHeelOffsetDeg = imuPitchOffsetDeg = 0;
      imuGxBias = imuGyBias = imuGzBias = 0;
      settingRemove(NK_imu_zero);
      queueConsoleMessage("IMU ZERO: calibration cleared");
      inputMessage = "1";
    }

    if (request->hasParam("RebootRegulator")) {
      foundParameter = true;
      rebootRequested = true;          // deferred — loop() does the actual restart after response flushes
      rebootRequestedAt = millis();
      queueConsoleMessage("REBOOT: requested from /get endpoint");
      inputMessage = "1";
    }
    if (request->hasParam("absorptionCompleteTime")) {
      foundParameter = true;
      inputMessage = request->getParam("absorptionCompleteTime")->value();
      uint32_t seconds = (uint32_t)inputMessage.toInt();
      absorptionCompleteTime = seconds * 1000UL;
      settingWrite(NK_absorptionCompleteTime, String(absorptionCompleteTime).c_str());
    }
    if (request->hasParam("FLOAT_DURATION")) {
      foundParameter = true;
      inputMessage = request->getParam("FLOAT_DURATION")->value();
      float hours = inputMessage.toFloat();
      int seconds = (int)(hours * 3600.0f);  // fractional hours preserved
      FLOAT_DURATION = seconds;
      settingWrite(NK_FLOAT_DURATION, String(seconds).c_str());
    }
    if (request->hasParam("UseFloat")) {
      foundParameter = true;
      inputMessage = request->getParam("UseFloat")->value();
      UseFloat = constrain(inputMessage.toInt(), 0, 2);  // 0=idle, 1=voltage float, 2=zero-current float
      settingWrite(NK_UseFloat, String(UseFloat).c_str());
    }
    if (request->hasParam("RebulkCurrent_A")) {
      foundParameter = true;
      inputMessage = request->getParam("RebulkCurrent_A")->value();
      RebulkCurrent_A = inputMessage.toFloat();
      settingWrite(NK_RebulkCurrent_A, String(RebulkCurrent_A).c_str());
    }
    // No VMGUseTrueWind param — both VMGs (manual + upwind) are always computed.
    if (request->hasParam("gpsTimeSourceMode")) {
      foundParameter = true;
      inputMessage = request->getParam("gpsTimeSourceMode")->value();
      uint8_t m = (uint8_t)inputMessage.toInt();
      if (m > GTS_NTP) m = GTS_AUTO;  // sanity
      gpsTimeSourceMode = m;
      settingWrite(NK_gpsTimeSourceMode, String(gpsTimeSourceMode).c_str());
      const char *lbl = (m == GTS_AUTO)  ? "auto"
                      : (m == GTS_NMEA)  ? "NMEA only"
                      : (m == GTS_PHONE) ? "phone only"
                                         : "NTP time only";
      queueConsoleMessageF("GPS/time source mode set to %s", lbl);
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
      nvsPersistNow = true;
      queueConsoleMessage("Engine Run Time: Reset requested from web interface");
    }
    if (request->hasParam("ResetAlternatorOnTime")) {
      foundParameter = true;
      AlternatorOnTime = 0;
      nvsPersistNow = true;
      queueConsoleMessage("Alternator On Time: Reset requested from web interface");
    }
    if (request->hasParam("ResetEnergy")) {
      foundParameter = true;
      ChargedEnergy = 0;
      nvsPersistNow = true;
      queueConsoleMessage("Battery Charged Energy: Reset requested from web interface");
    }
    if (request->hasParam("ResetDischargedEnergy")) {
      foundParameter = true;
      DischargedEnergy = 0;
      nvsPersistNow = true;
      queueConsoleMessage("Battery Discharged Energy: Reset requested from web interface");
    }
    if (request->hasParam("ResetFuelUsed")) {
      foundParameter = true;
      AlternatorFuelUsed = 0;
      nvsPersistNow = true;
      queueConsoleMessage("Fuel Used: Reset requested from web interface");
    }
    if (request->hasParam("ResetAlternatorChargedEnergy")) {
      foundParameter = true;
      AlternatorChargedEnergy = 0;
      nvsPersistNow = true;
      queueConsoleMessage("Alternator Charged Energy: Reset requested from web interface");
    }
    if (request->hasParam("ResetEngineCycles")) {
      foundParameter = true;
      EngineCycles = 0;
      nvsPersistNow = true;
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
      nvsPersistNow = true;
      queueConsoleMessage("Solar Energy: Reset requested from web interface");
    }
    if (request->hasParam("ResetChargeCycles")) {
      foundParameter = true;
      ChargeCycles = 0;
      nvsPersistNow = true;
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
      nvsPersistNow = true;
      queueConsoleMessage("Total Distance: Reset requested from web interface");
    }
    if (request->hasParam("ResetAvgSpeed")) {
      foundParameter = true;
      AvgSpeed = 0;
      nvsPersistNow = true;
      queueConsoleMessage("Average Speed: Reset requested from web interface");
    }
    if (request->hasParam("ResetMaxSpeed")) {
      foundParameter = true;
      MaxSpeed = 0;
      nvsPersistNow = true;
      queueConsoleMessage("Max Speed: Reset requested from web interface");
    }
    // Lifetime nav/sailing records — individual resets (moved off the diagnostics "Reset Peak
    // Values" button). prev_* shadows are intentionally NOT touched so saveNVSDataFull() sees the
    // change and actually persists the cleared value.
    if (request->hasParam("ResetLongestTrip")) {
      foundParameter = true;
      LongestSingleTrip_Nm_AllTime = 0.0f;   // in-progress trip (currentTripDistanceNm) intentionally preserved
      nvsPersistNow = true;
      queueConsoleMessage("Longest Single Trip: Reset requested from web interface");
    }
    if (request->hasParam("ResetMax24hrDist")) {
      foundParameter = true;
      // clear watermark + the 24-bucket ring, else the pre-reset window keeps the old peak alive
      Max24hrDistance_AllTime = 0.0f;
      for (uint8_t i = 0; i < 24; i++) distHourBuckets[i] = 0.0f;
      distHourHead = 0;
      distHourStartMs = millis();
      nvsPersistNow = true;
      queueConsoleMessage("Max 24-Hour Distance: Reset requested from web interface");
    }
    if (request->hasParam("ResetDeepestAnchor")) {
      foundParameter = true;
      // clear watermark + anchorage ring (same rationale as the 24h window)
      DeepestAnchorage_Ft_AllTime = 0.0f;
      if (anchorageRing) memset(anchorageRing, 0, ANCHORAGE_RING_SIZE * sizeof(AnchorageSample));
      anchorageRingHead = 0;
      anchorageRingCount = 0;
      lastAnchorageSampleMs = 0;
      nvsPersistNow = true;
      queueConsoleMessage("Deepest Anchorage: Reset requested from web interface");
    }
    if (request->hasParam("ResetBestUpwindVMG")) {
      foundParameter = true;
      best_upwind_vmg_alltime = 0.0f;
      nvsPersistNow = true;
      queueConsoleMessage("Best Upwind VMG: Reset requested from web interface");
    }
    if (request->hasParam("ResetLongestGale")) {
      foundParameter = true;
      // clear the watermark AND the in-progress gale run so it can't keep extending post-reset
      longest_gale_duration_hours_alltime = 0.0f;
      galeActive = false;
      currentGaleMinutes = 0;
      nvsPersistNow = true;
      queueConsoleMessage("Longest Gale Duration: Reset requested from web interface");
    }
    if (request->hasParam("ResetEngineFuelUsed")) {
      foundParameter = true;
      EngineFuelUsed = 0;
      nvsPersistNow = true;
      queueConsoleMessage("Engine Fuel Used: Reset requested from web interface");
    }
    if (request->hasParam("ResetFuelCurve")) {
      foundParameter = true;
      memset(fuelCurveNMPG, 0, sizeof(fuelCurveNMPG));  // clear all bins
      fcRun = false;                                    // break any in-progress steady run
      queueConsoleMessage("Fuel Economy Curve: Reset requested from web interface");
    }
    if (request->hasParam("MaximumAllowedBatteryAmps")) {
      foundParameter = true;
      inputMessage = request->getParam("MaximumAllowedBatteryAmps")->value();
      settingWrite(NK_MaximumAllowedBatteryAmps, inputMessage.c_str());
      MaximumAllowedBatteryAmps = inputMessage.toInt();
    }
    if (request->hasParam("LoadDumpDtThresh")) {
      foundParameter = true;
      inputMessage = request->getParam("LoadDumpDtThresh")->value();
      LoadDumpDtThresh = inputMessage.toFloat();
      settingWrite(NK_LoadDumpDtThresh, String(LoadDumpDtThresh).c_str());
    }
    if (request->hasParam("LoadDumpDtThresh1")) {
      foundParameter = true;
      inputMessage = request->getParam("LoadDumpDtThresh1")->value();
      LoadDumpDtThresh1 = inputMessage.toFloat();
      settingWrite(NK_LoadDumpDtThresh1, String(LoadDumpDtThresh1).c_str());
    }
    if (request->hasParam("LoadDumpDtThresh3")) {
      foundParameter = true;
      inputMessage = request->getParam("LoadDumpDtThresh3")->value();
      LoadDumpDtThresh3 = inputMessage.toFloat();
      settingWrite(NK_LoadDumpDtThresh3, String(LoadDumpDtThresh3).c_str());
    }
    if (request->hasParam("SocSeedAck")) {
      foundParameter = true;
      inputMessage = request->getParam("SocSeedAck")->value();
      settingWrite(NK_SocSeedAck, inputMessage.c_str());
    }
    if (request->hasParam("ManualSOCPoint")) {
      foundParameter = true;
      inputMessage = request->getParam("ManualSOCPoint")->value();
      settingWrite(NK_ManualSOCPoint, inputMessage.c_str());
      ManualSOCPoint = inputMessage.toFloat();
      SOC_percent = (int)roundf(ManualSOCPoint * 100.0f);   // SOC_percent is percent x100; round so decimals seed exactly
      CoulombCount_Ah_scaled = (ManualSOCPoint * BatteryCapacity_Ah);
      nvsPersistNow = true;  // persist SoC + coulomb count NOW (single save at end of handler). NVS otherwise only saves at the field-off edge, so a reboot before then (e.g. a forced OTA) would revert the manual seed and the loop would re-derive SoC from the stale/zero coulomb count.
      queueConsoleMessageF("SoC manually set to: %.2f%%", ManualSOCPoint);
    }
    if (request->hasParam("BatteryCapacity_Ah")) {
      foundParameter = true;
      inputMessage = request->getParam("BatteryCapacity_Ah")->value();
      settingWrite(NK_BatteryCapacity_Ah, inputMessage.c_str());
      BatteryCapacity_Ah = inputMessage.toInt();
      PeukertRatedCurrent_A = BatteryCapacity_Ah / 20.0f;
      updateVesselInfoField("battery_capacity_ah", BatteryCapacity_Ah);
      queueConsoleMessageF("Battery capacity set to: %d Ah", BatteryCapacity_Ah);
    }
    if (request->hasParam("ShuntResistanceMicroOhm")) {
      foundParameter = true;
      inputMessage = request->getParam("ShuntResistanceMicroOhm")->value();
      settingWrite(NK_ShuntResistanceMicroOhm, inputMessage.c_str());
      ShuntResistanceMicroOhm = inputMessage.toInt();
    }

    if (request->hasParam("VoltageKp")) {
      foundParameter = true;
      inputMessage = request->getParam("VoltageKp")->value();
      settingWrite(NK_VoltageKp, inputMessage.c_str());
      VoltageKp = inputMessage.toFloat();
      recomputeCvGains();  // manual gain changed → refresh the active gain
      if (CVTuningMode) cvTuningParamChanged = true;
    }

    if (request->hasParam("maxPoints")) {
      foundParameter = true;
      inputMessage = request->getParam("maxPoints")->value();
      settingWrite(NK_maxPoints, inputMessage.c_str());
      maxPoints = inputMessage.toInt();
    }
    if (request->hasParam("Ymin1")) {
      foundParameter = true;
      inputMessage = request->getParam("Ymin1")->value();
      settingWrite(NK_Ymin1, inputMessage.c_str());
      Ymin1 = inputMessage.toInt();
    }
    if (request->hasParam("Ymax1")) {
      foundParameter = true;
      inputMessage = request->getParam("Ymax1")->value();
      settingWrite(NK_Ymax1, inputMessage.c_str());
      Ymax1 = inputMessage.toInt();
    }
    if (request->hasParam("Ymin2")) {
      foundParameter = true;
      inputMessage = request->getParam("Ymin2")->value();
      settingWrite(NK_Ymin2, inputMessage.c_str());
      Ymin2 = inputMessage.toFloat();
    }
    if (request->hasParam("Ymax2")) {
      foundParameter = true;
      inputMessage = request->getParam("Ymax2")->value();
      settingWrite(NK_Ymax2, inputMessage.c_str());
      Ymax2 = inputMessage.toFloat();
    }
    if (request->hasParam("Ymin3")) {
      foundParameter = true;
      inputMessage = request->getParam("Ymin3")->value();
      settingWrite(NK_Ymin3, inputMessage.c_str());
      Ymin3 = inputMessage.toInt();
    }
    if (request->hasParam("Ymax3")) {
      foundParameter = true;
      inputMessage = request->getParam("Ymax3")->value();
      settingWrite(NK_Ymax3, inputMessage.c_str());
      Ymax3 = inputMessage.toInt();
    }
    if (request->hasParam("Ymin4")) {
      foundParameter = true;
      inputMessage = request->getParam("Ymin4")->value();
      settingWrite(NK_Ymin4, inputMessage.c_str());
      Ymin4 = inputMessage.toInt();
    }
    if (request->hasParam("Ymax4")) {
      foundParameter = true;
      inputMessage = request->getParam("Ymax4")->value();
      settingWrite(NK_Ymax4, inputMessage.c_str());
      Ymax4 = inputMessage.toInt();
    }
    if (request->hasParam("AutoShuntGainCorrection")) {
      foundParameter = true;
      inputMessage = request->getParam("AutoShuntGainCorrection")->value();
      settingWrite(NK_AutoShuntGainCorrection, inputMessage.c_str());
      AutoShuntGainCorrection = inputMessage.toInt();
    }
    if (request->hasParam("AutoAltCurrentZero")) {
      foundParameter = true;
      inputMessage = request->getParam("AutoAltCurrentZero")->value();
      settingWrite(NK_AutoAltCurrentZero, inputMessage.c_str());
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
      settingWrite(NK_WindingTempOffset, inputMessage.c_str());
      WindingTempOffset = inputMessage.toFloat();
    }
    if (request->hasParam("displayTempUnit")) {
      foundParameter = true;
      inputMessage = request->getParam("displayTempUnit")->value();
      settingWrite(NK_displayTempUnit, inputMessage.c_str());
      displayTempUnit = (uint8_t)inputMessage.toInt();
    }
    if (request->hasParam("PulleyRatio")) {
      foundParameter = true;
      inputMessage = request->getParam("PulleyRatio")->value();
      settingWrite(NK_PulleyRatio, inputMessage.c_str());
      PulleyRatio = inputMessage.toFloat();
    }
    if (request->hasParam("ManualLifePercentage")) {
      foundParameter = true;
      inputMessage = request->getParam("ManualLifePercentage")->value();
      settingWrite(NK_ManualLifePercentage, inputMessage.c_str());
      ManualLifePercentage = inputMessage.toInt();
      float life_fraction = ManualLifePercentage / 100.00;
      CumulativeInsulationDamage = 1.0 - life_fraction;
      CumulativeGreaseDamage = 1.0 - life_fraction;
      CumulativeBrushDamage = 1.0 - life_fraction;
      nvsPersistNow = true;   // commit InsulDamage/GreaseDamage/BrushDamage now (single save at end of handler) instead of waiting for the next field-off edge.
      queueConsoleMessageF("Alternator life manually set to %d%%", ManualLifePercentage);
    }
    if (request->hasParam("webgaugesinterval")) {
      foundParameter = true;
      inputMessage = request->getParam("webgaugesinterval")->value();
      int newInterval = inputMessage.toInt();
      newInterval = constrain(newInterval, 1, 10000000);
      settingWrite(NK_webgaugesinterval, String(newInterval).c_str());
      webgaugesinterval = newInterval;
      queueConsoleMessageF("Update interval set to: %dms", newInterval);
    }
    if (request->hasParam("BatteryCurrentSource")) {
      foundParameter = true;
      inputMessage = request->getParam("BatteryCurrentSource")->value();
      settingWrite(NK_BatteryCurrentSource, inputMessage.c_str());
      BatteryCurrentSource = inputMessage.toInt();
      queueConsoleMessageF("Battery current source changed to: %d", BatteryCurrentSource);
    }
    if (request->hasParam("totalPowerCycles")) {
      foundParameter = true;
      inputMessage = request->getParam("totalPowerCycles")->value();
      settingWrite(NK_totalPowerCycles, inputMessage.c_str());
      totalPowerCycles = inputMessage.toInt();
      nvsPersistNow = true;   // also commit the storage-namespace "PowerCycles" (what loadNVSData reads at boot) now, not at the field-off edge.
    }
    if (request->hasParam("timeAxisModeChanging")) {
      foundParameter = true;
      inputMessage = request->getParam("timeAxisModeChanging")->value();
      settingWrite(NK_timeAxisModeChanging, inputMessage.c_str());
      timeAxisModeChanging = inputMessage.toInt();
      queueConsoleMessageF("Time axis mode changed to: %s", timeAxisModeChanging ? "UNIX timestamps" : "relative time");
    }
    if (request->hasParam("plotTimeWindow")) {
      foundParameter = true;
      inputMessage = request->getParam("plotTimeWindow")->value();
      settingWrite(NK_plotTimeWindow, inputMessage.c_str());
      plotTimeWindow = inputMessage.toInt();
    }
    if (request->hasParam("LatitudeNMEA") && request->hasParam("LongitudeNMEA")) {
      foundParameter = true;
      // Engage the sticky manual override: these coords now beat boat NMEA and
      // phone GPS until the user clears it (clearGpsManual). resolveSources()
      // reasserts them every tick, so the auto-sources can't clobber them.
      LatitudeManual  = request->getParam("LatitudeNMEA")->value().toDouble();
      LongitudeManual = request->getParam("LongitudeNMEA")->value().toDouble();
      gpsManualActive = true;
      LatitudeNMEA  = LatitudeManual;
      LongitudeNMEA = LongitudeManual;
      settingWrite(NK_LatitudeManual, String(LatitudeManual, 6).c_str());
      settingWrite(NK_LongitudeManual, String(LongitudeManual, 6).c_str());
      settingWrite(NK_gpsManualActive, "1");
      queueConsoleMessageF("GPS: Manual override set to %.6f, %.6f (sticky — beats NMEA & phone)", LatitudeManual, LongitudeManual);
      nextWeatherUpdate = millis();  // fire next tick (signed-delta safe; '= 0' would miss past 24.8d uptime)
    }
    if (request->hasParam("clearGpsManual")) {
      foundParameter = true;
      gpsManualActive = false;
      settingWrite(NK_gpsManualActive, "0");
      queueConsoleMessage("GPS: Manual override cleared — back to automatic (NMEA/phone)");
      nextWeatherUpdate = millis();  // refresh weather with whatever the auto chain now resolves
    }
    if (request->hasParam("weatherModeEnabled")) {
      foundParameter = true;
      inputMessage = request->getParam("weatherModeEnabled")->value();
      settingWrite(NK_weatherModeEnabled, inputMessage.c_str());
      weatherModeEnabled = inputMessage.toInt();
      queueConsoleMessageF("Weather Mode %s", weatherModeEnabled ? "enabled" : "disabled");
    }
    if (request->hasParam("UVThresholdHigh")) {
      foundParameter = true;
      inputMessage = request->getParam("UVThresholdHigh")->value();
      settingWrite(NK_UVThresholdHigh, inputMessage.c_str());
      UVThresholdHigh = inputMessage.toFloat();
    }
    if (request->hasParam("SolarWatts")) {
      foundParameter = true;
      inputMessage = request->getParam("SolarWatts")->value();
      settingWrite(NK_SolarWatts, inputMessage.c_str());
      SolarWatts = inputMessage.toInt();
      updateVesselInfoField("solar_watts", SolarWatts);
    }
    if (request->hasParam("performanceRatio")) {
      foundParameter = true;
      inputMessage = request->getParam("performanceRatio")->value();
      settingWrite(NK_performanceRatio, inputMessage.c_str());
      performanceRatio = inputMessage.toFloat();
    }
    if (request->hasParam("TriggerWeatherUpdate")) {
      foundParameter = true;
      // Every rejection path must give user feedback — a silent fall-through (e.g. no GPS) reads as
      // "nothing happened". Check each precondition separately so the message names the actual reason.
      if (fieldActiveStatus > 0) {
        queueConsoleMessage("Weather update refused: disable the field first");
        inputMessage = "field_on";
      } else if (LatitudeNMEA == 0.0 && LongitudeNMEA == 0.0) {
        queueConsoleMessage("Weather update failed: no GPS position yet — wait for a fix or set a manual lat/lon");
        inputMessage = "no_gps";
      } else if (WiFi.RSSI() < -76) {
        queueConsoleMessage("Weather update failed: WiFi signal too weak");
        inputMessage = "weak_wifi";
      } else {
        HttpsRequest req = { .type = HTTPS_FETCH_WEATHER };
        if (xQueueSend(httpsQueue, &req, 0) == pdTRUE) {
          queueConsoleMessage("Weather: Manual update triggered");
        } else {
          queueConsoleMessage("Weather: HTTPS queue full, try again in a moment");
        }
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
    if (request->hasParam("CAPSIZE_THRESHOLD_DEG")) {
      foundParameter = true;
      inputMessage = request->getParam("CAPSIZE_THRESHOLD_DEG")->value();
      settingWrite(NK_CAPSIZE_THRESHOLD_DEG, inputMessage.c_str());
      CAPSIZE_THRESHOLD_DEG = inputMessage.toFloat();
    }
    if (request->hasParam("PITCHPOLE_THRESHOLD_DEG")) {
      foundParameter = true;
      inputMessage = request->getParam("PITCHPOLE_THRESHOLD_DEG")->value();
      settingWrite(NK_PITCHPOLE_THRESHOLD_DEG, inputMessage.c_str());
      PITCHPOLE_THRESHOLD_DEG = inputMessage.toFloat();
    }
    if (request->hasParam("SLAM_THRESHOLD_G")) {
      foundParameter = true;
      inputMessage = request->getParam("SLAM_THRESHOLD_G")->value();
      settingWrite(NK_SLAM_THRESHOLD_G, inputMessage.c_str());
      SLAM_THRESHOLD_G = inputMessage.toFloat();
    }
    if (request->hasParam("IgnoreLearningDuringPenalty")) {
      foundParameter = true;
      inputMessage = request->getParam("IgnoreLearningDuringPenalty")->value();
      settingWrite(NK_IgnoreLearningDuringPenalty, inputMessage.c_str());
      IgnoreLearningDuringPenalty = inputMessage.toInt();
    }
    if (request->hasParam("CloudFeatures")) {
      foundParameter = true;
      inputMessage = request->getParam("CloudFeatures")->value();
      settingWrite(NK_CloudFeatures, inputMessage.c_str());
      CloudFeatures = inputMessage.toInt();
    }
    if (request->hasParam("EnableAmbientCorrection")) {
      foundParameter = true;
      inputMessage = request->getParam("EnableAmbientCorrection")->value();
      settingWrite(NK_EnableAmbientCorrection, inputMessage.c_str());
      EnableAmbientCorrection = inputMessage.toInt();
    }
    if (request->hasParam("testProtectionsEnabled")) {
      foundParameter = true;
      inputMessage = request->getParam("testProtectionsEnabled")->value();
      testProtectionsEnabled = (inputMessage.toInt() != 0);
      // NOT persisted to LittleFS — resets TRUE (enabled) on every boot by design.
      // foundParameter=true above bumps settingsDirty so CSV3 echoes the change immediately,
      // which fires the dashboard banner show/hide and syncs the per-page toggle states.
      if (testProtectionsEnabled) {
        queueConsoleMessage("PROTECTIONS ENABLED — all protection layers restored");
      } else {
        queueConsoleMessage("PROTECTIONS DISABLED for tuning — G1/G2/G3 bypassed; fast OV (AlternatorHardShutdownV), G4, INA228, and hardware OC remain active");
      }
    }
    if (request->hasParam("TuningMode")) {
      foundParameter = true;
      inputMessage = request->getParam("TuningMode")->value();
      int requested = inputMessage.toInt();
      // Mutex: refuse turn-on if another test is already running. All four tests must run independently.
      if (requested == 1 && TuningMode == 0) {
        const char *blocker = (systemIDActive != 0) ? "Plant Delay Test"
                                                    : (CVTuningMode ? "Voltage tuning" : nullptr);
        if (blocker != nullptr) {
          queueConsoleMessageF("Current tuning: turn-on blocked — %s is active. Turn it off first.", blocker);
        } else {
          settingWrite(NK_TuningMode, inputMessage.c_str());
          TuningMode = requested;
        }
      } else {
        settingWrite(NK_TuningMode, inputMessage.c_str());
        TuningMode = requested;
      }
    }
    if (request->hasParam("tuningSquareAbrupt")) {
      foundParameter = true;   // transient (NOT persisted): CV plant fit asks for abrupt square edges
      tuningSquareAbrupt = (request->getParam("tuningSquareAbrupt")->value().toInt() != 0);
    }
    if (request->hasParam("commitTuningScore")) {
      foundParameter = true;
      manualCommitTuningRequested = true;
      queueConsoleMessage("TuningScore: manual commit requested via UI");
    }
    // ── Battery Health (DCIR active test) ──
    if (request->hasParam("BatteryHealthTest")) {
      foundParameter = true;
      if (!bhStartTest()) queueConsoleMessageF("BATT HEALTH: cannot start — %s", bhAbortReason);
    }
    if (request->hasParam("bhAbort")) {
      foundParameter = true;   // wizard Abort — only meaningful mid-run; the loop resumes normal slew-limited AUTO control
      if (bhTestState == 1) bhAbort("cancelled by user");
    }
    if (request->hasParam("bhStepLowA")) {
      foundParameter = true;
      bhStepLowA = request->getParam("bhStepLowA")->value().toFloat();
      settingWrite(NK_bhStepLowA, String(bhStepLowA, 1).c_str());
    }
    if (request->hasParam("bhStepDeltaA")) {
      foundParameter = true;
      bhStepDeltaA = request->getParam("bhStepDeltaA")->value().toFloat();
      if (bhStepDeltaA < 5.0f) bhStepDeltaA = 5.0f;
      settingWrite(NK_bhStepDeltaA, String(bhStepDeltaA, 1).c_str());
      if (bhDwellMs < bhMinDwellMs()) {   // bigger step lengthens the slew traverse — stretch dwell to match
        bhDwellMs = bhMinDwellMs();
        settingWrite(NK_bhDwellMs, String(bhDwellMs).c_str());
      }
    }
    if (request->hasParam("bhDwellSec")) {
      foundParameter = true;   // UI is seconds, internal is ms
      bhDwellMs = (uint32_t)(request->getParam("bhDwellSec")->value().toFloat() * 1000.0f);
      if (bhDwellMs < bhMinDwellMs()) bhDwellMs = bhMinDwellMs();
      settingWrite(NK_bhDwellMs, String(bhDwellMs).c_str());
    }
    if (request->hasParam("bhNumEdges")) {
      foundParameter = true;
      int e = request->getParam("bhNumEdges")->value().toInt();
      if (e < 3) e = 3;
      if (e > BH_MAX_TOGGLES - 3) e = BH_MAX_TOGGLES - 3;
      bhNumEdges = (uint8_t)e;
      settingWrite(NK_bhNumEdges, String(bhNumEdges).c_str());
    }
    if (request->hasParam("bhStartOver")) {
      foundParameter = true;   // battery replaced: re-baseline + wipe history
      bhBaselineCapacityAh = 0.0f;
      bhResultCount = 0; bhResultHead = 0;
      bhCapCount = 0; bhCapHead = 0; bhCapDirty = false;
      bhTestState = 0;
      capLowAnchorValid = false; capLastPct = NAN; capLastUpdateEpoch = 0;  // reset the capacity anchor/measurement state
      settingWrite(NK_bhResults, "");
      settingWrite(NK_bhCapBlob, "");
      settingWrite(NK_bhBaseline, "0");
      queueConsoleMessage("BATT HEALTH: history cleared, baseline reset (battery replaced)");
    }
    if (request->hasParam("bhClearHistory")) {
      foundParameter = true;   // wipe ONLY the resistance-test table; capacity baseline + trend untouched
      bhResultCount = 0; bhResultHead = 0;
      bhLastResultDcir = 0.0f;
      settingWrite(NK_bhResults, "");
      queueConsoleMessage("BATT HEALTH: resistance-test history cleared");
    }
    // ── Capacity tracker config + OCV table ──
    if (request->hasParam("capRestFrac"))   { foundParameter = true; capRestCurrentFrac = request->getParam("capRestFrac")->value().toFloat();   settingWrite(NK_capRestFrac, String(capRestCurrentFrac, 4).c_str()); }
    if (request->hasParam("capRestFloor"))  { foundParameter = true; capRestFloorMin    = (uint16_t)request->getParam("capRestFloor")->value().toInt(); settingWrite(NK_capRestFloor, String(capRestFloorMin).c_str()); }
    if (request->hasParam("capSettleRate")) { foundParameter = true; capSettleRateMv10  = request->getParam("capSettleRate")->value().toFloat(); settingWrite(NK_capSettleRate, String(capSettleRateMv10, 2).c_str()); }
    if (request->hasParam("capSocLowMax"))  { foundParameter = true; capSocLowMax       = request->getParam("capSocLowMax")->value().toFloat();  settingWrite(NK_capSocLowMax, String(capSocLowMax, 1).c_str()); }
    if (request->hasParam("capMinSpan"))    { foundParameter = true; capMinSpan         = request->getParam("capMinSpan")->value().toFloat();    settingWrite(NK_capMinSpan, String(capMinSpan, 1).c_str()); }
    if (request->hasParam("capFullSoc"))    { foundParameter = true; capFullSoc         = request->getParam("capFullSoc")->value().toFloat();    settingWrite(NK_capFullSoc, String(capFullSoc, 1).c_str()); }
    if (request->hasParam("capRefMode"))    { foundParameter = true; capRefMode         = (uint8_t)request->getParam("capRefMode")->value().toInt(); settingWrite(NK_capRefMode, String(capRefMode).c_str()); }
    if (request->hasParam("capTempNorm"))   { foundParameter = true; capTempNormEnable  = (uint8_t)request->getParam("capTempNorm")->value().toInt(); settingWrite(NK_capTempNorm, String(capTempNormEnable).c_str()); }
    if (request->hasParam("capTempCoeff"))  { foundParameter = true; capTempCoeffPctC   = request->getParam("capTempCoeff")->value().toFloat();  settingWrite(NK_capTempCoeff, String(capTempCoeffPctC, 3).c_str()); }
    if (request->hasParam("capTempRef"))    { foundParameter = true; capTempRefC        = request->getParam("capTempRef")->value().toFloat();   settingWrite(NK_capTempRef, String(capTempRefC, 1).c_str()); }
    if (request->hasParam("capOcv")) {
      foundParameter = true;   // full OCV table as a comma-joined string of CAP_OCV_ROWS rested voltages
      String body = request->getParam("capOcv")->value();
      capDeserializeOcv(body);
      settingWrite(NK_capOcvBlob, body.c_str());
    }
    if (request->hasParam("capClearPoints")) {
      foundParameter = true;   // wipe ONLY the capacity trend + baseline; resistance history untouched
      bhBaselineCapacityAh = 0.0f;
      bhCapCount = 0; bhCapHead = 0; bhCapDirty = false;
      capLowAnchorValid = false; capLastPct = NAN; capLastUpdateEpoch = 0;
      settingWrite(NK_bhCapBlob, "");
      settingWrite(NK_bhBaseline, "0");
      queueConsoleMessage("BATT CAP: capacity trend + baseline cleared");
    }
    if (request->hasParam("LogAllLearningEvents")) {
      foundParameter = true;
      inputMessage = request->getParam("LogAllLearningEvents")->value();
      settingWrite(NK_LogAllLearningEvents, inputMessage.c_str());
      LogAllLearningEvents = inputMessage.toInt();
    }
    if (request->hasParam("hardwarePresent")) {
      foundParameter = true;
      inputMessage = request->getParam("hardwarePresent")->value();
      settingWrite(NK_hardwarePresent, inputMessage.c_str());
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

      bool rpmPointChanged = false;
      for (int i = 0; i < RPM_TABLE_SIZE; i++) {
        if (rpmTableRPMPoints[i] != previousRPMPoints[i]) {
          rpmPointChanged = true;
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

      // An RPM breakpoint moved → every learned Min% floor is now keyed to the wrong RPM
      // (the per-RPM onset-knee fit spans all bins). Wipe the knee tracker entirely and flag the
      // Min% floor commissioning step (index 2) for re-run, so the floors are re-measured at the
      // new breakpoints instead of silently re-applied at shifted RPMs.
      if (rpmPointChanged) {
        kneeLearnResetDefaults();   // zero floors/knees, unfreeze, drop rpmMinDutyTable to 0
        commissionClearStage(7);    // Min% floor stage now stale (demotes a commissioned device to in-progress)
        queueConsoleMessage("Learning: RPM breakpoints changed — Min% floors cleared, re-run the Min% floor step");
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
      memset(fuelCurveNMPG, 0, sizeof(fuelCurveNMPG));  // GPH map + bin scale changed -> old mpg stale
      fcRun = false;
      queueConsoleMessage("Fuel table updated from web interface");
    }


    if (request->hasParam("AlternatorNominalAmps")) {
      foundParameter = true;
      inputMessage = request->getParam("AlternatorNominalAmps")->value();
      settingWrite(NK_AlternatorNominalAmps, inputMessage.c_str());
      AlternatorNominalAmps = inputMessage.toInt();
    }
    if (request->hasParam("LearningUpStep")) {
      foundParameter = true;
      inputMessage = request->getParam("LearningUpStep")->value();
      settingWrite(NK_LearningUpStep, inputMessage.c_str());
      LearningUpStep = inputMessage.toFloat();
    }
    if (request->hasParam("LearningDownStep")) {
      foundParameter = true;
      inputMessage = request->getParam("LearningDownStep")->value();
      settingWrite(NK_LearningDownStep, inputMessage.c_str());
      LearningDownStep = inputMessage.toFloat();
    }
    if (request->hasParam("AmbientTempCorrectionFactor")) {
      foundParameter = true;
      inputMessage = request->getParam("AmbientTempCorrectionFactor")->value();
      settingWrite(NK_AmbientTempCorrectionFactor, inputMessage.c_str());
      AmbientTempCorrectionFactor = inputMessage.toFloat();
    }
    if (request->hasParam("xTime")) {
      foundParameter = true;
      inputMessage = request->getParam("xTime")->value();
      settingWrite(NK_xTime, inputMessage.c_str());
      xTime = inputMessage.toFloat();
    }
    if (request->hasParam("MinLearningInterval")) {
      foundParameter = true;
      inputMessage = request->getParam("MinLearningInterval")->value();
      int temp = inputMessage.toInt() * 1000;  // from seconds (entry into html) to ms
      settingWrite(NK_MinLearningInterval, String(temp).c_str());
      MinLearningInterval = temp;
    }
    if (request->hasParam("SetpointRiseRate")) {
      foundParameter = true;
      inputMessage = request->getParam("SetpointRiseRate")->value();
      settingWrite(NK_SetpointRiseRate, inputMessage.c_str());
      SetpointRiseRate = inputMessage.toFloat();
      if (TuningMode) tuningParamChanged = true;
      if (CVTuningMode) cvTuningParamChanged = true;
    }
    if (request->hasParam("SetpointBigStepThresh")) {
      foundParameter = true;
      inputMessage = request->getParam("SetpointBigStepThresh")->value();
      settingWrite(NK_SetpointBigStepThresh, inputMessage.c_str());
      SetpointBigStepThresh = inputMessage.toFloat();
      if (TuningMode) tuningParamChanged = true;
      if (CVTuningMode) cvTuningParamChanged = true;
    }
    if (request->hasParam("SetpointBigStepRiseRate")) {
      foundParameter = true;
      inputMessage = request->getParam("SetpointBigStepRiseRate")->value();
      settingWrite(NK_SetpointBigStepRiseRate, inputMessage.c_str());
      SetpointBigStepRiseRate = inputMessage.toFloat();
      if (TuningMode) tuningParamChanged = true;
      if (CVTuningMode) cvTuningParamChanged = true;
    }
    if (request->hasParam("SetpointFallRate")) {
      foundParameter = true;
      inputMessage = request->getParam("SetpointFallRate")->value();
      settingWrite(NK_SetpointFallRate, inputMessage.c_str());
      SetpointFallRate = inputMessage.toFloat();
      if (TuningMode) tuningParamChanged = true;
      if (CVTuningMode) cvTuningParamChanged = true;
    }
    if (request->hasParam("StartupRiseRate")) {
      foundParameter = true;
      inputMessage = request->getParam("StartupRiseRate")->value();
      settingWrite(NK_StartupRiseRate, inputMessage.c_str());
      StartupRiseRate = inputMessage.toFloat();
      queueConsoleMessageF("Startup rise rate set to: %.2f A/sec", StartupRiseRate);
    }
    if (request->hasParam("PIDTrackingGain")) {
      foundParameter = true;
      inputMessage = request->getParam("PIDTrackingGain")->value();
      float temp = inputMessage.toFloat();
      settingWrite(NK_PIDTrackingGain, String(temp).c_str());
      PIDTrackingGain = temp;
      if (TuningMode) tuningParamChanged = true;
    }
    if (request->hasParam("SafeOperationThreshold")) {
      foundParameter = true;
      inputMessage = request->getParam("SafeOperationThreshold")->value();
      int temp = inputMessage.toInt() * 1000;  // from seconds (entry into html) to ms
      settingWrite(NK_SafeOperationThreshold, String(temp).c_str());
      SafeOperationThreshold = temp;
    }
    if (request->hasParam("PidKp")) {
      foundParameter = true;
      inputMessage = request->getParam("PidKp")->value();
      settingWrite(NK_PidKp, inputMessage.c_str());
      PidKp = inputMessage.toFloat();
      if (pidInitialized) {
        recomputeCcGains();  // apply voltage-normalized PidK*_active
      }
      if (TuningMode) tuningParamChanged = true;
      queueConsoleMessageF("PID Kp updated to: %.6f", PidKp);
    }
    if (request->hasParam("AbsorptionVoltage")) {
      foundParameter = true;
      inputMessage = request->getParam("AbsorptionVoltage")->value();
      AbsorptionVoltage = inputMessage.toFloat();
      settingWrite(NK_AbsorptionVoltage, inputMessage.c_str());
    }
    if (request->hasParam("TargetVoltageSetpoint")) {
      foundParameter = true;
      inputMessage = request->getParam("TargetVoltageSetpoint")->value();
      TargetVoltageSetpoint = inputMessage.toFloat();
      settingWrite(NK_TargetVoltageSetpoint, inputMessage.c_str());
    }

    if (request->hasParam("AbsorptionTimeoutMs")) {
      foundParameter = true;
      inputMessage = request->getParam("AbsorptionTimeoutMs")->value();
      uint32_t minutes = (uint32_t)inputMessage.toInt();
      AbsorptionTimeoutMs = minutes * 60000UL;
      settingWrite(NK_AbsorptionTimeoutMs, String(AbsorptionTimeoutMs).c_str());
    }
    if (request->hasParam("bulkVoltageHoldMs")) {
      foundParameter = true;
      inputMessage = request->getParam("bulkVoltageHoldMs")->value();
      float seconds = inputMessage.toFloat();
      bulkVoltageHoldMs = (uint32_t)(seconds * 1000.0f);
      settingWrite(NK_bulkVoltageHoldMs, String(bulkVoltageHoldMs).c_str());
    }
    if (request->hasParam("VoltageKi")) {
      foundParameter = true;
      inputMessage = request->getParam("VoltageKi")->value();
      VoltageKi = inputMessage.toFloat();
      settingWrite(NK_VoltageKi, String(VoltageKi).c_str());
      recomputeCvGains();  // manual gain changed → refresh the active gain
      if (CVTuningMode) cvTuningParamChanged = true;
    }
    // ── CV gain-mode system: mode toggle, λ multiplier, and (bench) hand-entered plant ──
    if (request->hasParam("cvGainMode")) {
      foundParameter = true;
      cvGainMode = (uint8_t)(request->getParam("cvGainMode")->value().toInt() != 0 ? 1 : 0);
      settingWrite(NK_cvGainMode, String((int)cvGainMode).c_str());
      recomputeCvGains();
      queueConsoleMessageF("CV gain mode: %s", cvGainMode ? "AUTO (lambda-based)" : "MANUAL");
    }
    // No cvLambdaMult handler — the Response Speed slider drives cvCrossover.
    if (request->hasParam("cvCrossover")) {  // CV crossover ω_c (rad/s) — exact magnitude formula, §F.3
      foundParameter = true;
      cvCrossover = clamp_f(request->getParam("cvCrossover")->value().toFloat(), 0.05f, 0.80f);  // 0.80 ≈ 5 s, fast end of the seconds control
      settingWrite(NK_cvCrossover, String(cvCrossover, 3).c_str());
      recomputeCvGains();
      queueConsoleMessageF("CV crossover omega_c: %.2f rad/s", cvCrossover);
    }
    // User-facing CV Response Time in seconds → ω_c = 4/sec (settle ≈ 4 time constants). Stored as cvCrossover;
    // the dashboard relabels ω_c as seconds, but ω_c stays the internal/NVS unit so recomputeCvGains is unchanged.
    if (request->hasParam("cvRespS")) {
      foundParameter = true;
      float respS = request->getParam("cvRespS")->value().toFloat();
      cvCrossover = clamp_f((respS > 0.0f) ? (4.0f / respS) : 0.20f, 0.05f, 0.80f);
      settingWrite(NK_cvCrossover, String(cvCrossover, 3).c_str());
      recomputeCvGains();
      queueConsoleMessageF("CV response time: %.0f s (crossover %.2f rad/s)", 4.0f / cvCrossover, cvCrossover);
    }
    if (request->hasParam("cvPiZero")) {  // CV PI integral zero ρ (rad/s) — Ki = ρ·Kp (§F.3)
      foundParameter = true;
      cvPiZero = clamp_f(request->getParam("cvPiZero")->value().toFloat(), 0.2f, 1.5f);
      settingWrite(NK_cvPiZero, String(cvPiZero, 3).c_str());
      recomputeCvGains();
      queueConsoleMessageF("CV PI zero (Ki/Kp ratio): %.2f", cvPiZero);
    }
    if (request->hasParam("battTempDerateEnable")) {  // master on/off for the battery-temp gain derate
      foundParameter = true;
      battTempDerateEnable = (request->getParam("battTempDerateEnable")->value().toInt() != 0);
      settingWrite(NK_battTempDerateEn, String((int)battTempDerateEnable).c_str());
      recomputeCvGains();
      queueConsoleMessageF("Battery-temp gain derate: %s", battTempDerateEnable ? "ON" : "OFF");
    }
    if (request->hasParam("battTempCoeff")) {  // battery fractional resistance change per °C (R rises as T falls)
      foundParameter = true;
      battTempCoeff = clamp_f(request->getParam("battTempCoeff")->value().toFloat(), 0.0f, 0.10f);
      settingWrite(NK_battTempCoeff, String(battTempCoeff, 4).c_str());
      recomputeCvGains();
      queueConsoleMessageF("Battery-temp resistance coeff: %.3f /°C", battTempCoeff);
    }
    if (request->hasParam("vTgtRampEnable")) {  // master switch for the voltage-target slew (1=on, 0=instant)
      foundParameter = true;
      vTgtRampEnable = (uint8_t)(request->getParam("vTgtRampEnable")->value().toInt() ? 1 : 0);
      settingWrite(NK_vTgtRampEnable, String((int)vTgtRampEnable).c_str());
      queueConsoleMessageF("CV target ramp limiter: %s", vTgtRampEnable ? "ON" : "OFF (instant)");
    }
    if (request->hasParam("setpointSlewEnable")) {  // master switch for inner-loop current setpoint slew (1=on, 0=instant)
      foundParameter = true;
      setpointSlewEnable = (uint8_t)(request->getParam("setpointSlewEnable")->value().toInt() ? 1 : 0);
      settingWrite(NK_setpointSlewEnable, String((int)setpointSlewEnable).c_str());
      queueConsoleMessageF("Current setpoint slew limiter: %s", setpointSlewEnable ? "ON" : "OFF (instant)");
    }
    if (request->hasParam("cvRiseGovEnable")) {  // master switch for CV rise governor / anti-windup clamp (1=on, 0=off)
      foundParameter = true;
      cvRiseGovEnable = (uint8_t)(request->getParam("cvRiseGovEnable")->value().toInt() ? 1 : 0);
      settingWrite(NK_cvRiseGovEnable, String((int)cvRiseGovEnable).c_str());
      queueConsoleMessageF("CV rise governor (anti-windup): %s", cvRiseGovEnable ? "ON" : "OFF — OV-trip risk on up-steps");
    }
    if (request->hasParam("dutySlewEnable")) {  // master switch for field duty slew (1=on, 0=instant)
      foundParameter = true;
      dutySlewEnable = (uint8_t)(request->getParam("dutySlewEnable")->value().toInt() ? 1 : 0);
      settingWrite(NK_dutySlewEnable, String((int)dutySlewEnable).c_str());
      queueConsoleMessageF("Field duty slew limiter: %s", dutySlewEnable ? "ON" : "OFF (instant)");
    }
    if (request->hasParam("vTgtRampUp")) {  // CV voltage-target ramp UP rate (V/s); 0 = instant
      foundParameter = true;
      vTgtRampUp = clamp_f(request->getParam("vTgtRampUp")->value().toFloat(), 0.0f, 5.0f);
      settingWrite(NK_vTgtRampUp, String(vTgtRampUp, 3).c_str());
      queueConsoleMessageF("CV target ramp up: %.3f V/s", vTgtRampUp);
    }
    if (request->hasParam("vTgtRampDn")) {  // CV voltage-target ramp DOWN rate (V/s); 0 = instant
      foundParameter = true;
      vTgtRampDn = clamp_f(request->getParam("vTgtRampDn")->value().toFloat(), 0.0f, 5.0f);
      settingWrite(NK_vTgtRampDn, String(vTgtRampDn, 3).c_str());
      queueConsoleMessageF("CV target ramp down: %.3f V/s", vTgtRampDn);
    }
    if (request->hasParam("cvPlantK")) {  // hand-entered plant gain (bench) — the fit step writes these directly
      foundParameter = true;
      cvPlantK = request->getParam("cvPlantK")->value().toFloat();
      settingWrite(NK_cvPlantK, String(cvPlantK, 5).c_str());
      // Stamp the board temp at the moment K_dc was measured — the reference for the battery-temp gain
      // derate. Only when the proxy reading is live; otherwise leave the prior stamp (no false reference).
      if (!IS_STALE(IDX_AMBIENT_TEMP) && isfinite(ambientTemp)) {
        CommissionTempF = ambientTemp;
        settingWrite(NK_CommissionTempF, String(CommissionTempF, 2).c_str());
      }
      recomputeCvGains();
    }
    // Measured ripple projection (§3.3): the browser fits ripple(I)=a0+a1·I from the 3-level current-check
    // points and POSTs the whole record as a CSV string. Reference data for the Protections
    // ripple-vs-threshold plot ONLY — never touches the over-current floor.
    if (request->hasParam("ripFitAlt")) {
      foundParameter = true;
      ripFitDecode(request->getParam("ripFitAlt")->value(), ripFitAlt);
      settingWrite(NK_ripFitAlt, ripFitEncode(ripFitAlt).c_str());
    }
    // Resonance current-check (§3.2): arm/disarm field-commanding, and set the commanded level. Disarm
    // also zeroes the target so the loop slews back toward the normal AUTO setpoint gently.
    else if (request->hasParam("resTest")) {
      foundParameter = true;
      bool arm = (request->getParam("resTest")->value().toInt() != 0);
      if (arm) { resTestActive = true; resTestReleasing = false; }
      // Release = DEFERRED: keep the field under test-control (OV still suppressed) and let the loop wind the
      // current down to ~0 first; it drops resTestActive itself once the field is down (6_functions.ino), so
      // CV re-enters at low voltage and slews up from below rather than slamming G2 on the transition edge.
      else if (resTestActive) { resTestReleasing = true; resTestTargetA = 0.0f; }
      resTestLastCmdMs = millis();
      queueConsoleMessageF("Resonance current-check %s", arm ? "ARMED (wizard commands current)" : "winding down");
    }
    else if (request->hasParam("resTestTargetA")) {
      foundParameter = true;
      resTestTargetA = request->getParam("resTestTargetA")->value().toFloat();
      resTestLastCmdMs = millis();  // keepalive refresh (deadman)
    }
    if (request->hasParam("cvPlantTau")) {
      foundParameter = true;
      cvPlantTau = request->getParam("cvPlantTau")->value().toFloat();
      settingWrite(NK_cvPlantTau, String(cvPlantTau, 3).c_str());
      recomputeCvGains();
    }
    if (request->hasParam("cvPlantL")) {
      foundParameter = true;
      cvPlantL = request->getParam("cvPlantL")->value().toFloat();
      settingWrite(NK_cvPlantL, String(cvPlantL, 3).c_str());
      recomputeCvGains();
    }
    // No VoltageKd handler — voltage loop has no D term.
    if (request->hasParam("SlopeBleedThresh")) {
      foundParameter = true;
      inputMessage = request->getParam("SlopeBleedThresh")->value();
      SlopeBleedThresh = inputMessage.toFloat();
      settingWrite(NK_SlopeBleedThresh, String(SlopeBleedThresh, 3).c_str());
      queueConsoleMessageF("Slope bleed threshold: %.3f V/s", SlopeBleedThresh);
    }
    if (request->hasParam("SlopeBleedK")) {
      foundParameter = true;
      inputMessage = request->getParam("SlopeBleedK")->value();
      SlopeBleedK = inputMessage.toFloat();
      settingWrite(NK_SlopeBleedK, String(SlopeBleedK, 1).c_str());
      queueConsoleMessageF("Slope bleed gain: %.1f A/(V/s)", SlopeBleedK);
    }
    if (request->hasParam("cvHelpersEnabled")) {
      foundParameter = true;
      inputMessage = request->getParam("cvHelpersEnabled")->value();
      cvHelpersEnabled = inputMessage.toInt() != 0;
      settingWrite(NK_cvHelpersEnabled, String((int)cvHelpersEnabled).c_str());
      queueConsoleMessageF("CV tuning helpers (asymmetric unwind + slope bleed): %s", cvHelpersEnabled ? "ENABLED" : "DISABLED");
    }
    if (request->hasParam("SlopeBleedProxV")) {
      foundParameter = true;
      inputMessage = request->getParam("SlopeBleedProxV")->value();
      SlopeBleedProxV = inputMessage.toFloat();
      settingWrite(NK_SlopeBleedProxV, String(SlopeBleedProxV, 2).c_str());
      queueConsoleMessageF("Slope bleed proximity gate: %.2f V", SlopeBleedProxV);
    }
    if (request->hasParam("TempPIDKp")) {
      foundParameter = true;
      inputMessage = request->getParam("TempPIDKp")->value();
      settingWrite(NK_TempPIDKp, inputMessage.c_str());
      TempPIDKp = inputMessage.toFloat();
      // Velocity form reads TempPIDKp directly each tick — no library object to retune.
      queueConsoleMessageF("Temp PID Kp updated to: %.6f", TempPIDKp);
    }
    if (request->hasParam("TempPIDKi")) {
      foundParameter = true;
      inputMessage = request->getParam("TempPIDKi")->value();
      settingWrite(NK_TempPIDKi, inputMessage.c_str());
      TempPIDKi = inputMessage.toFloat();
      // Velocity form reads TempPIDKi directly each tick — no library object to retune.
      queueConsoleMessageF("Temp PID Ki updated to: %.6f", TempPIDKi);
    }
    if (request->hasParam("TempPIDKiDownFrac")) {
      foundParameter = true;
      inputMessage = request->getParam("TempPIDKiDownFrac")->value();
      TempPIDKiDownFrac = constrain(inputMessage.toFloat(), 0.0f, 1.0f);  // ratio of Ki used to bleed below setpoint
      settingWrite(NK_TempPIDKiDownFrac, String(TempPIDKiDownFrac, 3).c_str());
      queueConsoleMessageF("Temp PID below-setpoint bleed ratio set to: %.2f x Ki", TempPIDKiDownFrac);
    }
    if (request->hasParam("ThermalLookaheadSec")) {
      foundParameter = true;
      inputMessage = request->getParam("ThermalLookaheadSec")->value();
      ThermalLookaheadSec = clamp_f(inputMessage.toFloat(), 0.0f, 300.0f);
      settingWrite(NK_ThermalLookaheadSec, String(ThermalLookaheadSec, 1).c_str());      queueConsoleMessageF("ThermalLookaheadSec set to: %.1f s", ThermalLookaheadSec);
    }
    if (request->hasParam("ThermalSlopeWindowSec")) {
      foundParameter = true;
      inputMessage = request->getParam("ThermalSlopeWindowSec")->value();
      ThermalSlopeWindowSec = clamp_f(inputMessage.toFloat(), 10.0f, 60.0f);  // slope backward-difference window
      settingWrite(NK_ThermalSlopeWindowSec, String(ThermalSlopeWindowSec, 1).c_str());
      queueConsoleMessageF("ThermalSlopeWindowSec set to: %.1f s", ThermalSlopeWindowSec);
    }
    if (request->hasParam("TempPIDIntervalMs")) {
      foundParameter = true;
      inputMessage = request->getParam("TempPIDIntervalMs")->value();
      settingWrite(NK_TempPIDIntervalMs, inputMessage.c_str());
      TempPIDIntervalMs = inputMessage.toInt();      queueConsoleMessageF("Temp PID interval updated to: %d ms", TempPIDIntervalMs);
    }
    if (request->hasParam("TempPIDFilterAlpha")) {
      foundParameter = true;
      inputMessage = request->getParam("TempPIDFilterAlpha")->value();
      settingWrite(NK_TempPIDFilterAlpha, inputMessage.c_str());
      TempPIDFilterAlpha = inputMessage.toFloat();      queueConsoleMessageF("Temp PID filter alpha updated to: %.3f", TempPIDFilterAlpha);
    }
    if (request->hasParam("PidKi")) {
      foundParameter = true;
      inputMessage = request->getParam("PidKi")->value();
      settingWrite(NK_PidKi, inputMessage.c_str());
      PidKi = inputMessage.toFloat();
      if (pidInitialized) {
        recomputeCcGains();  // apply voltage-normalized PidK*_active
      }
      if (TuningMode) tuningParamChanged = true;
      queueConsoleMessageF("PID Ki updated to: %.6f", PidKi);
    }
    if (request->hasParam("PidKd")) {
      foundParameter = true;
      inputMessage = request->getParam("PidKd")->value();
      settingWrite(NK_PidKd, inputMessage.c_str());
      PidKd = inputMessage.toFloat();
      if (pidInitialized) {
        recomputeCcGains();  // apply voltage-normalized PidK*_active
      }
      if (TuningMode) tuningParamChanged = true;
      queueConsoleMessageF("PID Kd updated to: %.6f", PidKd);
    }
    if (request->hasParam("PidSampleDivisor")) {
      foundParameter = true;
      inputMessage = request->getParam("PidSampleDivisor")->value();
      settingWrite(NK_PidSampleDivisor, inputMessage.c_str());
      PidSampleDivisor = inputMessage.toInt();
      if (TuningMode) tuningParamChanged = true;
    }
    if (request->hasParam("LearningSettlingPeriod")) {
      foundParameter = true;
      inputMessage = request->getParam("LearningSettlingPeriod")->value();
      int temp = inputMessage.toInt() * 1000;  // Convert seconds to ms
      settingWrite(NK_LearningSettlingPeriod, String(temp).c_str());
      LearningSettlingPeriod = temp;
    }
    if (request->hasParam("LearningRPMChangeThreshold")) {
      foundParameter = true;
      inputMessage = request->getParam("LearningRPMChangeThreshold")->value();
      settingWrite(NK_LearningRPMChangeThreshold, inputMessage.c_str());
      LearningRPMChangeThreshold = inputMessage.toInt();
    }
    if (request->hasParam("LearningTempHysteresis")) {
      foundParameter = true;
      inputMessage = request->getParam("LearningTempHysteresis")->value();
      settingWrite(NK_LearningTempHysteresis, inputMessage.c_str());
      LearningTempHysteresis = inputMessage.toInt();
    }
    // "Group 0" in UI = hardware overcurrent trip (no protection-group integration yet)
    if (request->hasParam("MaxTableValue")) {
      foundParameter = true;
      inputMessage = request->getParam("MaxTableValue")->value();
      settingWrite(NK_MaxTableValue, inputMessage.c_str());
      MaxTableValue = inputMessage.toFloat();
      HardOCTripAmps = MaxTableValue + 10.0f;  // always 10A above current limit
      queueConsoleMessageF("Alternator current limit set to %.1fA — OC trip threshold: %.1fA", MaxTableValue, HardOCTripAmps);
    }
    if (request->hasParam("MaxPenaltyPercent")) {
      foundParameter = true;
      inputMessage = request->getParam("MaxPenaltyPercent")->value();
      settingWrite(NK_MaxPenaltyPercent, inputMessage.c_str());
      MaxPenaltyPercent = inputMessage.toFloat();
    }
    if (request->hasParam("MaxPenaltyDuration")) {
      foundParameter = true;
      inputMessage = request->getParam("MaxPenaltyDuration")->value();
      int temp = inputMessage.toInt() * 1000;
      settingWrite(NK_MaxPenaltyDuration, String(temp).c_str());
      MaxPenaltyDuration = temp;
    }
    if (request->hasParam("NeighborLearningFactor")) {
      foundParameter = true;
      inputMessage = request->getParam("NeighborLearningFactor")->value();
      settingWrite(NK_NeighborLearningFactor, inputMessage.c_str());
      NeighborLearningFactor = inputMessage.toFloat();
    }
    if (request->hasParam("yyMax")) {
      foundParameter = true;
      inputMessage = request->getParam("yyMax")->value();
      settingWrite(NK_yyMax, inputMessage.c_str());
      yyMax = inputMessage.toInt();
    }
    if (request->hasParam("LearningMemoryDuration")) {
      foundParameter = true;
      inputMessage = request->getParam("LearningMemoryDuration")->value();
      settingWrite(NK_LearningMemoryDuration, inputMessage.c_str());
      LearningMemoryDuration = inputMessage.toInt();
    }
    if (request->hasParam("DutyRampRate")) {
      foundParameter = true;
      inputMessage = request->getParam("DutyRampRate")->value();
      settingWrite(NK_DutyRampRate, inputMessage.c_str());
      DutyRampRate = inputMessage.toFloat();
      if (TuningMode) tuningParamChanged = true;
      queueConsoleMessageF("Duty ramp rate set to: %.1f %%/sec", DutyRampRate);
    }
    if (request->hasParam("DutySlowRampRate")) {
      foundParameter = true;
      inputMessage = request->getParam("DutySlowRampRate")->value();
      settingWrite(NK_DutySlowRampRate, inputMessage.c_str());
      DutySlowRampRate = inputMessage.toFloat();
      queueConsoleMessageF("Shutdown slow ramp rate set to: %.2f %%/s", DutySlowRampRate);
    }
    if (request->hasParam("ShutdownPhase2HoldMs")) {
      foundParameter = true;
      inputMessage = request->getParam("ShutdownPhase2HoldMs")->value();
      uint32_t ms = (uint32_t)inputMessage.toInt();
      settingWrite(NK_ShutdownPhase2HoldMs, String(ms).c_str());
      ShutdownPhase2HoldMs = ms;
      queueConsoleMessageF("Shutdown phase 2 hold set to: %u ms", ShutdownPhase2HoldMs);
    }
    if (request->hasParam("SettleTimeBeforeCut")) {
      foundParameter = true;
      inputMessage = request->getParam("SettleTimeBeforeCut")->value();
      settingWrite(NK_SettleTimeBeforeCut, inputMessage.c_str());
      SettleTimeBeforeCut = inputMessage.toInt();
      queueConsoleMessageF("Settle time before cut set to: %d ms", SettleTimeBeforeCut);
    }
    if (request->hasParam("TempWarnExcess")) {
      foundParameter = true;
      inputMessage = request->getParam("TempWarnExcess")->value();
      settingWrite(NK_TempWarnExcess, inputMessage.c_str());
      TempWarnExcess = inputMessage.toFloat();
      queueConsoleMessageF("Temp warning threshold set to: +%.1f°F above limit", TempWarnExcess);
    }
    if (request->hasParam("TempCritExcess")) {
      foundParameter = true;
      inputMessage = request->getParam("TempCritExcess")->value();
      settingWrite(NK_TempCritExcess, inputMessage.c_str());
      TempCritExcess = inputMessage.toFloat();
      queueConsoleMessageF("Temp critical threshold set to: +%.1f°F above limit", TempCritExcess);
    }
    if (request->hasParam("TempSustainedTimeout")) {
      foundParameter = true;
      inputMessage = request->getParam("TempSustainedTimeout")->value();
      int temp = inputMessage.toInt() * 1000;  // user enters seconds
      settingWrite(NK_TempSustainedTimeout, String(temp).c_str());
      TempSustainedTimeout = temp;
      queueConsoleMessageF("Temp sustained timeout set to: %d seconds", inputMessage.toInt());
    }
    if (request->hasParam("AlternatorHardShutdownV")) {
      foundParameter = true;
      inputMessage = request->getParam("AlternatorHardShutdownV")->value();
      settingWrite(NK_AlternatorHardShutdownV, inputMessage.c_str());
      AlternatorHardShutdownV = inputMessage.toFloat();
      queueConsoleMessageF("Alternator hard-shutdown voltage set to: %.2fV (absolute)", AlternatorHardShutdownV);
    }
    // "Group 0" in UI = hardware overcurrent trip (no protection-group integration yet)
    if (request->hasParam("HardOCDebounceMs")) {
      foundParameter = true;
      inputMessage = request->getParam("HardOCDebounceMs")->value();
      settingWrite(NK_HardOCDebounceMs, inputMessage.c_str());
      HardOCDebounceMs = (uint32_t)inputMessage.toInt();
      queueConsoleMessageF("Overcurrent trip debounce set to: %ums", HardOCDebounceMs);
    }
    if (request->hasParam("WarmupRampRate")) {
      foundParameter = true;
      inputMessage = request->getParam("WarmupRampRate")->value();
      WarmupRampRate = max(0.0f, inputMessage.toFloat());
      settingWrite(NK_WarmupRampRate, String(WarmupRampRate, 2).c_str());
      queueConsoleMessageF("Warmup ramp rate set to: %.2f A/s", WarmupRampRate);
    }
    if (request->hasParam("IExcessFrac")) {
      foundParameter = true;
      inputMessage = request->getParam("IExcessFrac")->value();  // UI sends percent; stored as fraction
      IExcessFrac = constrain(inputMessage.toFloat() / 100.0f, 0.02f, 0.50f);
      settingWrite(NK_IExcessFrac, String(IExcessFrac, 3).c_str());
      queueConsoleMessageF("IExcess CV threshold set to: %.0f%% of command", IExcessFrac * 100.0f);
      if (CVTuningMode) cvTuningParamChanged = true;
    }
    if (request->hasParam("IExcessFracBulk")) {
      foundParameter = true;
      inputMessage = request->getParam("IExcessFracBulk")->value();  // UI sends percent; stored as fraction
      IExcessFracBulk = constrain(inputMessage.toFloat() / 100.0f, 0.02f, 0.60f);
      settingWrite(NK_IExcessFracBulk, String(IExcessFracBulk, 3).c_str());
      queueConsoleMessageF("IExcess bulk threshold set to: %.0f%% of ceiling", IExcessFracBulk * 100.0f);
    }
    if (request->hasParam("IExcessFloorA")) {
      foundParameter = true;
      inputMessage = request->getParam("IExcessFloorA")->value();
      IExcessFloorA = constrain(inputMessage.toFloat(), 1.0f, 20.0f);
      settingWrite(NK_IExcessFloorA, String(IExcessFloorA, 1).c_str());
      queueConsoleMessageF("IExcess threshold floor set to: %.1fA", IExcessFloorA);
      if (CVTuningMode) cvTuningParamChanged = true;
    }
    if (request->hasParam("IExcessCeilA")) {
      foundParameter = true;
      inputMessage = request->getParam("IExcessCeilA")->value();
      IExcessCeilA = constrain(inputMessage.toFloat(), 5.0f, 80.0f);
      settingWrite(NK_IExcessCeilA, String(IExcessCeilA, 1).c_str());
      queueConsoleMessageF("IExcess threshold ceiling set to: %.1fA", IExcessCeilA);
      if (CVTuningMode) cvTuningParamChanged = true;
    }
    // Max battery charge current (G4). Ceiling on the alternator command = limit + measured
    // house-load offset; requires the INA228 battery shunt. 0 disables the feature.
    if (request->hasParam("BattCurrentLimitA")) {
      foundParameter = true;
      inputMessage = request->getParam("BattCurrentLimitA")->value();
      BattCurrentLimitA = constrain(inputMessage.toFloat(), 0.0f, 500.0f);
      settingWrite(NK_BattCurrentLimitA, String(BattCurrentLimitA, 1).c_str());
      if (BattCurrentLimitA > 0.0f) queueConsoleMessageF("Battery charge current limit set to: %.1fA", BattCurrentLimitA);
      else queueConsoleMessage("Battery charge current limit disabled");
    }
    if (request->hasParam("IExcessTau")) {
      foundParameter = true;
      inputMessage = request->getParam("IExcessTau")->value();
      IExcessTau = constrain(inputMessage.toFloat(), 20.0f, 300.0f);
      settingWrite(NK_IExcessTau, String(IExcessTau, 1).c_str());
      queueConsoleMessageF("IExcess averaging TC set to: %.0f ms", IExcessTau);
      if (CVTuningMode) cvTuningParamChanged = true;
    }
    if (request->hasParam("IExcessRelFrac")) {
      foundParameter = true;
      inputMessage = request->getParam("IExcessRelFrac")->value();  // UI sends percent; stored as fraction
      IExcessRelFrac = constrain(inputMessage.toFloat() / 100.0f, 0.1f, 0.9f);
      settingWrite(NK_IExcessRelFrac, String(IExcessRelFrac, 3).c_str());
      queueConsoleMessageF("IExcess release hysteresis set to: %.0f%% of threshold", IExcessRelFrac * 100.0f);
      if (CVTuningMode) cvTuningParamChanged = true;
    }
    if (request->hasParam("IExcessKBleed")) {
      foundParameter = true;
      inputMessage = request->getParam("IExcessKBleed")->value();
      IExcessKBleed = inputMessage.toFloat();
      settingWrite(NK_IExcessKBleed, String(IExcessKBleed, 2).c_str());
      queueConsoleMessageF("K_bleed set to: %.2f A/s per A", IExcessKBleed);
      if (CVTuningMode) cvTuningParamChanged = true;
    }
    if (request->hasParam("IExcessArmMarginV")) {
      foundParameter = true;
      inputMessage = request->getParam("IExcessArmMarginV")->value();
      IExcessArmMarginV = constrain(inputMessage.toFloat(), 0.020f, 5.000f);
      settingWrite(NK_IExcessArmMarginV, String(IExcessArmMarginV, 3).c_str());
      queueConsoleMessageF("iExcess arming margin set to: %.0f mV below target", IExcessArmMarginV * 1000.0f);
    }
    if (request->hasParam("AwBleedRate")) {
      foundParameter = true;
      inputMessage = request->getParam("AwBleedRate")->value();
      AwBleedRate = inputMessage.toFloat();
      settingWrite(NK_AwBleedRate, String(AwBleedRate, 1).c_str());
      queueConsoleMessageF("AW bleed rate set to: %.1f A/s", AwBleedRate);
      if (CVTuningMode) cvTuningParamChanged = true;
    }
    // No AwRecoverRate handler — hardcoded in firmware (0.1f)
    if (request->hasParam("FastSetpointRiseRate")) {
      foundParameter = true;
      inputMessage = request->getParam("FastSetpointRiseRate")->value();
      FastSetpointRiseRate = constrain(inputMessage.toFloat(), 1.0f, 50.0f);
      settingWrite(NK_FastSetpointRiseRate, String(FastSetpointRiseRate, 1).c_str());
      queueConsoleMessageF("Fast setpoint rise rate set to: %.1fx", FastSetpointRiseRate);
    }
    if (request->hasParam("FastSetpointRiseWindowMs")) {
      foundParameter = true;
      inputMessage = request->getParam("FastSetpointRiseWindowMs")->value();
      FastSetpointRiseWindowMs = (uint32_t)constrain(inputMessage.toInt(), 500, 30000);
      settingWrite(NK_FastSetpointRiseWindowMs, String(FastSetpointRiseWindowMs).c_str());
      queueConsoleMessageF("Fast rise window set to: %ums", FastSetpointRiseWindowMs);
    }
    if (request->hasParam("FastSetpointRiseHeadroomV")) {
      foundParameter = true;
      inputMessage = request->getParam("FastSetpointRiseHeadroomV")->value();
      FastSetpointRiseHeadroomV = constrain(inputMessage.toFloat(), 0.05f, 2.0f);
      settingWrite(NK_FastSetpointRiseHeadroomV, String(FastSetpointRiseHeadroomV, 2).c_str());
      queueConsoleMessageF("Fast rise headroom set to: %.2fV", FastSetpointRiseHeadroomV);
    }
    if (request->hasParam("AwSeedProtectMs")) {
      foundParameter = true;
      inputMessage = request->getParam("AwSeedProtectMs")->value();
      AwSeedProtectMs = (uint16_t)constrain(inputMessage.toInt(), 0, 2000);
      settingWrite(NK_AwSeedProtectMs, String(AwSeedProtectMs).c_str());
      queueConsoleMessageF("AW seed protect window set to: %u ms", (unsigned)AwSeedProtectMs);
      if (CVTuningMode) cvTuningParamChanged = true;
    }
    if (request->hasParam("KHard")) {
      foundParameter = true;
      inputMessage = request->getParam("KHard")->value();
      KHard = inputMessage.toFloat();
      settingWrite(NK_KHard, String(KHard, 1).c_str());
      queueConsoleMessageF("KHard set to: %.1f A/V", KHard);
      if (CVTuningMode) cvTuningParamChanged = true;
    }
    if (request->hasParam("OvGroup1Enable")) {
      foundParameter = true;
      inputMessage = request->getParam("OvGroup1Enable")->value();
      OvGroup1Enable = inputMessage.toInt() != 0;
      settingWrite(NK_OvGroup1Enable, String((int)OvGroup1Enable).c_str());
      queueConsoleMessageF("OV Group 1 (prediction-based cap): %s", OvGroup1Enable ? "ENABLED" : "DISABLED");
    }
    if (request->hasParam("OvGroup2Enable")) {
      foundParameter = true;
      inputMessage = request->getParam("OvGroup2Enable")->value();
      OvGroup2Enable = inputMessage.toInt() != 0;
      settingWrite(NK_OvGroup2Enable, String((int)OvGroup2Enable).c_str());
      queueConsoleMessageF("OV Group 2 (measured-voltage threshold): %s", OvGroup2Enable ? "ENABLED" : "DISABLED");
    }
    if (request->hasParam("OutputPIDSigSrc")) {
      foundParameter = true;
      inputMessage = request->getParam("OutputPIDSigSrc")->value();
      OutputPIDSigSrc = constrain(inputMessage.toInt(), 0, 2);
      settingWrite(NK_OutputPIDSigSrc, String(OutputPIDSigSrc).c_str());
      const char* sigNames[] = { "EMA(TC)", "MA(N)", "Raw" };
      queueConsoleMessageF("Output PID signal source: %s", sigNames[OutputPIDSigSrc]);
    }
    if (request->hasParam("OutputPIDMA_N")) {
      foundParameter = true;
      inputMessage = request->getParam("OutputPIDMA_N")->value();
      OutputPIDMA_N = constrain(inputMessage.toInt(), 1, I_RING_SIZE);
      settingWrite(NK_OutputPIDMA_N, String(OutputPIDMA_N).c_str());
      queueConsoleMessageF("Output PID MA window: N=%d", OutputPIDMA_N);
    }
    if (request->hasParam("OutputPIDFilterTC")) {
      foundParameter = true;
      inputMessage = request->getParam("OutputPIDFilterTC")->value();
      OutputPIDFilterTC = inputMessage.toFloat();
      settingWrite(NK_OutputPIDFilterTC, String(OutputPIDFilterTC).c_str());
      queueConsoleMessageF("Output PID EMA TC: %.1f ms", OutputPIDFilterTC);
    }
    if (request->hasParam("VoltageFilterTC")) {
      foundParameter = true;
      inputMessage = request->getParam("VoltageFilterTC")->value();
      VoltageFilterTC = inputMessage.toFloat();
      settingWrite(NK_VoltageFilterTC, String(VoltageFilterTC).c_str());
      queueConsoleMessageF("Voltage EMA TC: %.1f ms", VoltageFilterTC);
    }
    if (request->hasParam("TdPred")) {
      foundParameter = true;
      inputMessage = request->getParam("TdPred")->value();
      TdPred = constrain(inputMessage.toFloat(), 0.01f, 0.30f);
      settingWrite(NK_TdPred, String(TdPred, 3).c_str());
      queueConsoleMessageF("OV prediction horizon set to: %.3f s", TdPred);
    }
    if (request->hasParam("OvMeasMarginV")) {
      foundParameter = true;
      inputMessage = request->getParam("OvMeasMarginV")->value();
      OvMeasMarginV = constrain(inputMessage.toFloat(), 0.020f, 0.500f);
      settingWrite(NK_OvMeasMarginV, String(OvMeasMarginV, 3).c_str());
      queueConsoleMessageF("Group 2 measured-voltage trigger margin set to: %.0f mV", OvMeasMarginV * 1000.0f);
    }
    if (request->hasParam("OvPredMarginV")) {
      foundParameter = true;
      inputMessage = request->getParam("OvPredMarginV")->value();
      OvPredMarginV = constrain(inputMessage.toFloat(), 0.050f, 1.000f);
      settingWrite(NK_OvPredMarginV, String(OvPredMarginV, 3).c_str());
      queueConsoleMessageF("Group 1 prediction trigger margin set to: %.0f mV", OvPredMarginV * 1000.0f);
    }
    if (request->hasParam("DvdtTC")) {
      foundParameter = true;
      inputMessage = request->getParam("DvdtTC")->value();
      DvdtTC = constrain(inputMessage.toFloat(), 5.0f, 500.0f);
      settingWrite(NK_DvdtTC, String(DvdtTC, 1).c_str());
      queueConsoleMessageF("dvdt EMA TC set to: %.1f ms (dt-aware; alpha computed per-sample)", DvdtTC);
    }
    if (request->hasParam("ReseedFrac")) {
      foundParameter = true;
      inputMessage = request->getParam("ReseedFrac")->value();
      ReseedFrac = inputMessage.toFloat();
      settingWrite(NK_ReseedFrac, String(ReseedFrac, 2).c_str());
      queueConsoleMessageF("Recovery seed fraction set to: %.2f", ReseedFrac);
      if (CVTuningMode) cvTuningParamChanged = true;
    }
    if (request->hasParam("CVTuningMode")) {
      foundParameter = true;
      inputMessage = request->getParam("CVTuningMode")->value();
      int requested = inputMessage.toInt();
      // Mutex: refuse turn-on if another test is already running. All four tests must run independently.
      if (requested == 1 && CVTuningMode == 0) {
        const char *blocker = (systemIDActive != 0) ? "Plant Delay Test"
                                                    : (TuningMode ? "Current tuning" : nullptr);
        if (blocker != nullptr) {
          queueConsoleMessageF("Voltage tuning: turn-on blocked — %s is active. Turn it off first.", blocker);
        } else {
          settingWrite(NK_CVTuningMode, inputMessage.c_str());
          CVTuningMode = requested;
        }
      } else {
        settingWrite(NK_CVTuningMode, inputMessage.c_str());
        CVTuningMode = requested;
      }
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
      settingWrite(NK_cvWaveAmplitudeV, String(cvWaveAmplitudeV, 2).c_str());
      if (CVTuningMode) cvTuningParamChanged = true;
    }
    if (request->hasParam("cvWavePeriodSec")) {
      foundParameter = true;
      inputMessage = request->getParam("cvWavePeriodSec")->value();
      cvWavePeriodSec = inputMessage.toInt();
      settingWrite(NK_cvWavePeriodSec, String(cvWavePeriodSec).c_str());
      if (CVTuningMode) cvTuningParamChanged = true;
    }
    if (request->hasParam("cvKOvershoot")) {
      foundParameter = true;
      inputMessage = request->getParam("cvKOvershoot")->value();
      cvKOvershoot = inputMessage.toFloat();
      settingWrite(NK_cvKOvershoot, String(cvKOvershoot, 1).c_str());
    }
    if (request->hasParam("cvConsecutiveReads")) {
      foundParameter = true;
      inputMessage = request->getParam("cvConsecutiveReads")->value();
      cvConsecutiveReads = (uint8_t)constrain(inputMessage.toInt(), 1, 20);
      settingWrite(NK_cvConsecutiveReads, String(cvConsecutiveReads).c_str());
    }
    if (request->hasParam("VoltageDisagreeThreshold")) {
      foundParameter = true;
      inputMessage = request->getParam("VoltageDisagreeThreshold")->value();
      settingWrite(NK_VoltageDisagreeThreshold, inputMessage.c_str());
      VoltageDisagreeThreshold = inputMessage.toFloat();
      queueConsoleMessageF("Voltage disagreement threshold set to: %.2fV", VoltageDisagreeThreshold);
    }
    if (request->hasParam("VoltageDisagreeTimeout")) {
      foundParameter = true;
      inputMessage = request->getParam("VoltageDisagreeTimeout")->value();
      int temp = inputMessage.toInt() * 1000;  // user enters seconds
      settingWrite(NK_VoltageDisagreeTimeout, String(temp).c_str());
      VoltageDisagreeTimeout = temp;
      queueConsoleMessageF("Voltage disagreement timeout set to: %d seconds", inputMessage.toInt());
    }
    if (request->hasParam("VoltageLoopInterval")) {
      foundParameter = true;
      inputMessage = request->getParam("VoltageLoopInterval")->value();
      settingWrite(NK_VoltageLoopInterval, inputMessage.c_str());
      VoltageLoopInterval = inputMessage.toInt();
    }
    if (request->hasParam("FIELD_COLLAPSE_DELAY")) {
      foundParameter = true;
      inputMessage = request->getParam("FIELD_COLLAPSE_DELAY")->value();
      int temp = inputMessage.toInt() * 1000;
      settingWrite(NK_FIELD_COLLAPSE_DELAY, String(temp).c_str());
      FIELD_COLLAPSE_DELAY = temp;
    }
    if (request->hasParam("ResetPerfCounters")) {
      foundParameter = true;
      // Function timing — session worsts (full list mirrors the periodic
      // worstWindow reset block in loop() so every timer on the dashboard
      // actually clears on button press).
      ft_ReadAnalogInputs.worstSession = 0;
      ft_AdjustFieldLearnMode.worstSession = 0;
      ft_logDashboardValues.worstSession = 0;
      ft_updateSystemHealthStats.worstSession = 0;
      ft_checkWiFiConnection.worstSession = 0;
      ft_SendWifiData.worstSession = 0;
      ft_CheckAlarms.worstSession = 0;
      ft_calculateDerivedMetrics.worstSession = 0;
      ft_ch1_compute_stats.worstSession = 0;
      ft_uploadSensorHistory.worstSession = 0;
      ft_dumpLongTermRing.worstSession = 0;
      ft_fastAltDrain.worstSession = 0;
      ft_faMatrixFlush.worstSession = 0;
      ft_faDetector.worstSession = 0;
      ft_faWindowFinalize.worstSession = 0;
      ft_zeroLogService.worstSession = 0;
      ft_bhFlushCapNVS.worstSession = 0;
      ft_kneeLearnService.worstSession = 0;
      // Deliberately NOT cleared here: the ripple analyzer's per-session worsts (faSesPkpkWorstA /
      // faSesPeakWorstA / faSesPeakWorstHz) persist across reboot and are cleared only by the
      // ripple panel's own reset (ResetRipplePeaks handler).
      ft_uploadBufferedRecords.worstSession = 0;
      ft_buildConfigPayload.worstSession = 0;
      ft_UpdateBatterySOC.worstSession = 0;
      ft_updateSensorWindow.worstSession = 0;
      ft_checkTimeSync.worstSession = 0;
      ft_rai_total.worstSession = 0;
      ft_rai_ina228.worstSession = 0;
      ft_rai_ads_state.worstSession = 0;
      ft_rai_bmp_state.worstSession = 0;
      ft_rai_imu.worstSession = 0;
      ft_updateAccelMetrics.worstSession = 0;
      ft_ReadVEData.worstSession = 0;
      ft_altHealth.worstSession = 0;
      ft_altFold.worstSession = 0;
      ft_boatPerf.worstSession = 0;
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
      // Inner Current PID firing-interval stats — clears all windows AND all-time accumulators
      pfAtWorst = 0;
      pfAtOver2x = 0;
      pfAtSum = 0;
      pfAtCount = 0;
      pf_worst_at = 0;
      pf_over2x_at = 0;
      pf_avg_at = 0.0f;
      pf_avg_10s = 0.0f;
      pf_worst_10s = 0;
      pf_over2x_10s = 0;
      pf_avg_2m = 0.0f;
      pf_worst_2m = 0;
      pf_over2x_2m = 0;
      pfBktHead = 0;
      pfBktCount = 0;
      pfBktStart = millis();
      pfBkt1sCount = 0;
      pfBkt1sHead = 0;
      pfBkt1sCurrent = { 0, 0, 0, 0 };
      pfBkt1sStart = millis();
      pfHasPrev = false;
      // INA228 interval stats — must clear all windows AND all-time accumulators,
      // else stale ina_worst_at/ina_worst_2m spikes survive the reset.
      resetINA228AllStats();
      // CV voltage-loop firing-interval ladder — clears all windows AND all-time accumulators
      vlAtWorst = 0;
      vlAtOver2x = 0;
      vlAtSum = 0;
      vlAtCount = 0;
      vl_worst_at = 0;
      vl_over2x_at = 0;
      vl_avg_at = 0.0f;
      vl_avg_10s = 0.0f;
      vl_worst_10s = 0;
      vl_over2x_10s = 0;
      vl_avg_2m = 0.0f;
      vl_worst_2m = 0;
      vl_over2x_2m = 0;
      vlBktHead = 0;
      vlBktCount = 0;
      vlBktStart = millis();
      vlBkt1sCount = 0;
      vlBkt1sHead = 0;
      vlBkt1sCurrent = { 0, 0, 0, 0 };
      vlBkt1sStart = millis();
      vlHasPrev = false;
      // Deliberately NOT cleared here: the lifetime nav/sailing records reset individually from
      // the Lifetime Statistics panel (ResetLongestTrip / ResetMax24hrDist / ResetDeepestAnchor /
      // ResetBestUpwindVMG / ResetLongestGale) — "Reset Peak Values" must never nuke leaderboard records.
      // 80MHz low-power loop instrumentation — clear session worst + near-miss counters
      loopWorst80Ses = 0;
      loopOver80ImuLimitCount = 0;
      loop80IterCount = 0;
      // field-ON loop instrumentation — clear session worst
      loopFieldOnSes = 0;
      // I2C bus-health — clear bus-only worst-timers and stall/error counts for a fresh window
      inaBusReadWorstUs = 0;
      inaBusSlowCount = 0;
      ina228ErrorCount = 0;
      adsI2CErrorCount = 0;
      imuFifoFetchWorstUs = 0;
      imuFifoWorstSamples = 0;
      // ADS slow-channel gap meters — clear worst (keep last; prev re-seeds on next read)
      ch0GapWorstMs = 0;
      ch2GapWorstMs = 0;
      // CSV2 build/send cost — clear worsts
      csv2BuildWorstUs = 0;
      csv2SendWorstUs = 0;
      // AdjustField section profiler — clear the worst-full-pass latch + breakdown (/debug)
      aflWorstTotalUs = 0;
      memset(aflWorstSecUs, 0, sizeof(aflWorstSecUs));
      // Worst-loop attribution diagnostic — re-arm so the next worst pass prints to Console
      loopDiagWorstUs = 0;
      // Fault-detector overall compute time — clear so /debug tracks worst since-reset.
      faDetWorstComputeUs = 0;
      faDetLastComputeUs = 0;
      // Core-0 HTTPS task cloud-op time — clear last + worst-since-reset
      httpsUploadWorstMs = 0;
      httpsUploadLastMs = 0;
      // NVS full-save diagnostics — clear the worst-duration watermark and the call
      // counter so "Worst Save Duration" and "Save Count" track since-reset, not since-boot.
      // "Last Save Duration" is zeroed too (it just shows 0 until the next field-off save).
      nvsFullSaveWorstMs = 0;
      nvsFullSaveCount = 0;
      nvsFullSaveLastMs = 0;
      // Reset to the "never saved this boot" sentinel so "Time Since Last Save" reads 0
      // and STAYS 0 until the next field-off save fires — turns the field into a live
      // "is the save actually firing?" indicator after a reset.
      lastNVSSaveTime = 0;
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
      // Reset shadow vars so the next saveNVSDataFull() sees a change and writes 0s to NVS
      prev_imu_heel_max_lifetime   = -1;
      prev_imu_pitch_max_lifetime  = -1;
      prev_imu_slam_peak_lifetime  = -1;
      prev_imu_slam_count_lifetime = UINT32_MAX;
      prev_imu_capsize_count       = UINT32_MAX;
      prev_imu_pitchpole_count     = UINT32_MAX;
      nvsPersistNow = true;   // commit the cleared IMU lifetime stats now, not at the next field-off edge.
      queueConsoleMessage("Accel lifetime stats reset from web interface");
    }

    if (nvsPersistNow) saveNVSDataFull();   // commit storage-namespace resets/sets now (at most one save per request); otherwise they'd wait for the field-off edge and a reboot/power-cut before then would revert them
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
    // Save the plaintext password and hash (NVS settings namespace)
    settingWrite(NK_password, newPassword.c_str());
    if (!settingWrite(NK_passwordHash, hash)) {
      request->send(500, "text/plain", "Failed to save password");
      return;
    }
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
    if (currentMode == MODE_CONFIG) {
      sendWifiConfigPortal(request);  // provisioning: captive probe → WiFi-setup page (correct for new users)
    } else if (WiFi.getMode() == WIFI_AP || WiFi.getMode() == WIFI_AP_STA) {
      // Operational AP: answer the OS connectivity probe as "success" so NO captive
      // browser sheet auto-pops — app-first. Browser only appears if user navigates there.
      request->send(200, "text/html", "<HTML><HEAD><TITLE>Success</TITLE></HEAD><BODY>Success</BODY></HTML>");
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
    const char *timeSrcName = (currentTimeSource == TIME_GPS)   ? "NMEA-GPS"
                            : (currentTimeSource == TIME_PHONE) ? "Phone"
                            : (currentTimeSource == TIME_NTP)   ? "NTP"
                            : (currentTimeSource == TIME_MILLIS) ? "drifting"
                            : (currentTimeSource == TIME_ESTIMATED) ? "estimated" : "none";
    const char *gpsSrcName  = (currentGpsSource == GPS_NMEA)   ? "NMEA"
                            : (currentGpsSource == GPS_PHONE)  ? "Phone"
                            : (currentGpsSource == GPS_MANUAL) ? "Manual" : "none";
    unsigned long now = millis();
    // Ground truth for the Core-1 preemption investigation: where the network tasks
    // actually landed. async_tcp must read 0 if the Core-0 pin made it into THIS image
    // (the pin is a compile-time default — a stale factory/IDE flash won't have it).
    // 2147483647 = tskNO_AFFINITY (floating), -99 = task not found by name.
    TaskHandle_t hAsyncTcp = xTaskGetHandle("async_tcp");
    TaskHandle_t hLwip = xTaskGetHandle("tiT");
    int asyncTcpCore = hAsyncTcp ? (int)xTaskGetCoreID(hAsyncTcp) : -99;
    int lwipCore = hLwip ? (int)xTaskGetCoreID(hLwip) : -99;
    const char *faStateName = (faChanState == 1) ? "live" : (faChanState == 2) ? "RAILED-dormant (jumper open?)" : "off";
    // mbedTLS allocator self-test: do a TLS-sized alloc through mbedtls_calloc and see which heap it
    // lands in. Proves the setup() mbedtls_platform_set_calloc_free(->PSRAM) override is live, without
    // relying on catching the early boot serial line. "PSRAM" = TLS no longer needs contiguous internal.
    size_t tlsPsBefore = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
    void *tlsTest = mbedtls_calloc(1, 16384);
    size_t tlsPsAfter = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
    const char *tlsMem = !tlsTest ? "ALLOC-FAILED" : ((tlsPsBefore - tlsPsAfter) >= 16000 ? "PSRAM" : "INTERNAL");
    if (tlsTest) mbedtls_free(tlsTest);
    char out[2816];
    int dpos = snprintf(out, sizeof(out),
             "Partition: %s\nVersion: %s\nFree heap: %lu\n"
             "TLS buffers -> %s (largest internal block %u B, free PSRAM %u B)\n"
             "Net task cores (0/1=pinned, 2147483647=floating, -99=not found): async_tcp=%d lwIP=%d\n"
             "AdjustField worst full pass (ms): total=%.1f | thermal=%.1f snapshot=%.1f fastov=%.1f modes=%.1f control=%.1f duty=%.1f tail=%.1f\n"
             "Time source: %s (NMEA last sync: %lus ago, Phone last: %lus ago)\n"
             "GPS source:  %s (NMEA last fix: %lus ago, Phone last: %lus ago)\n"
             "Lat/Lon: %.6f, %.6f\n"
             "FastAlt: %s range=%sdB windows ok=%lu disc=%lu matrixCells=%u sesPkpk=%.1fA sesPeak=%.1fA@%.0fHz\n",
             (running && running->label) ? running->label : "unknown",
             FIRMWARE_VERSION,
             (unsigned long)ESP.getFreeHeap(),
             tlsMem,
             (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL),
             (unsigned)heap_caps_get_free_size(MALLOC_CAP_SPIRAM),
             asyncTcpCore, lwipCore,
             aflWorstTotalUs / 1000.0f,
             aflWorstSecUs[0] / 1000.0f, aflWorstSecUs[1] / 1000.0f, aflWorstSecUs[2] / 1000.0f,
             aflWorstSecUs[3] / 1000.0f, aflWorstSecUs[4] / 1000.0f, aflWorstSecUs[5] / 1000.0f,
             aflWorstSecUs[6] / 1000.0f,
             timeSrcName,
             lastNmea2kSystemTimeMs ? (now - lastNmea2kSystemTimeMs) / 1000UL : 0UL,
             lastPhoneTimeMs        ? (now - lastPhoneTimeMs)        / 1000UL : 0UL,
             gpsSrcName,
             lastNmea2kGnssMs       ? (now - lastNmea2kGnssMs)       / 1000UL : 0UL,
             lastPhoneGpsMs         ? (now - lastPhoneGpsMs)         / 1000UL : 0UL,
             LatitudeNMEA, LongitudeNMEA,
             faStateName, faAttenIs12 ? "12" : "6",
             (unsigned long)faWindowsAccepted, (unsigned long)faWindowsDiscarded,
             (unsigned)faCellsUsed, faSesPkpkWorstA, faSesPeakWorstA, faSesPeakWorstHz);
    // Floating-task census for the Core-1 preemption hunt: any task with core=FLOAT
    // (tskNO_AFFINITY) can land on Core 1 and stall the priority-1 loop task. The loop runs
    // at priority 1 — read the core column, then suspect any FLOAT task above it.
    if (dpos > 0 && dpos < (int)sizeof(out)) {
      const UBaseType_t maxTasks = 32;   // uxTaskGetSystemState returns 0 if this is too small
      TaskStatus_t st[maxTasks];
      UBaseType_t nTasks = uxTaskGetSystemState(st, maxTasks, nullptr);
      dpos += snprintf(out + dpos, sizeof(out) - dpos, "Tasks (name prio core stackHWM):");
      for (UBaseType_t i = 0; i < nTasks && dpos < (int)sizeof(out) - 64; i++) {
        int core = (int)xTaskGetCoreID(st[i].xHandle);
        dpos += snprintf(out + dpos, sizeof(out) - dpos, " [%s p%u c%s hwm%u]",
                         st[i].pcTaskName,
                         (unsigned)st[i].uxCurrentPriority,
                         core == 2147483647 ? "FLOAT" : (core == 0 ? "0" : "1"),
                         (unsigned)st[i].usStackHighWaterMark);
      }
      snprintf(out + dpos, sizeof(out) - dpos, "\n");
    }
    request->send(200, "text/plain", out);
  });

  // Bench-only data-growth ceiling test: fill every ring to cap + measure the worst-case
  // scans (handlers defined at the tail of 8_functions.ino where the ring symbols are in scope).
  { void debugFillMax(AsyncWebServerRequest *); void debugClearMax(AsyncWebServerRequest *);
    server.on("/debug/fillmax",  HTTP_GET, debugFillMax);
    server.on("/debug/clearmax", HTTP_GET, debugClearMax); }

  // Phone-sourced GPS + time backup. Browser + Capacitor app both POST here
  // periodically (every ~30-60s) when they have a location fix. The priority
  // chain (NMEA → Phone → NTP) consumes these via the *Phone globals and the
  // lastPhone*Ms freshness timestamps. Partial submissions are OK — send only
  // the fields you have.
  server.on("/set_phone_data", HTTP_GET, [](AsyncWebServerRequest *request) {
    // Time sync (epochMs) needs NO password: adopting the client's clock is benign and is the
    // PRIMARY time source on NMEA-less boats in AP mode (browser or app), where the device has
    // no other clock. GPS injection (lat/lon) overrides navigation data, so it still requires auth.
    bool authed = request->hasParam("password") &&
                  strcmp(request->getParam("password")->value().c_str(), requiredPassword) == 0;
    bool acceptedGps = false, acceptedTime = false;
    if (authed && request->hasParam("lat") && request->hasParam("lon")) {
      // Use strtod + endptr (not String::toDouble — that silently returns 0.0
      // for unparseable input, so "lat=NaN&lon=42" would pass as (0.0, 42)).
      const char *latStr = request->getParam("lat")->value().c_str();
      const char *lonStr = request->getParam("lon")->value().c_str();
      char *latEnd = nullptr, *lonEnd = nullptr;
      double lat = strtod(latStr, &latEnd);
      double lon = strtod(lonStr, &lonEnd);
      bool latOk = (latEnd != latStr) && (*latEnd == '\0' || *latEnd == ' ');
      bool lonOk = (lonEnd != lonStr) && (*lonEnd == '\0' || *lonEnd == ' ');
      if (latOk && lonOk &&
          !isnan(lat) && !isnan(lon) &&
          fabs(lat) <= 90.0 && fabs(lon) <= 180.0 &&
          !(lat == 0.0 && lon == 0.0)) {
        LatitudePhone  = lat;
        LongitudePhone = lon;
        lastPhoneGpsMs = millis();
        acceptedGps = true;
      }
    }
    if (request->hasParam("epochMs")) {
      // JS Date.now() is millis since 1970. We store seconds for parity with
      // the GPS/NTP path. Reject anything before Jan 1, 2020 (sanity).
      uint64_t ms = (uint64_t)strtoull(request->getParam("epochMs")->value().c_str(), nullptr, 10);
      time_t sec = (time_t)(ms / 1000ULL);
      if (sec > 1577836800) {
        PhoneTimeEpoch  = sec;
        lastPhoneTimeMs = millis();
        acceptedTime = true;
      }
    }
    // Promote phone data into the effective globals if NMEA is stale. Both of
    // these are no-ops when NMEA is fresh (NMEA wins), so it's safe to call
    // unconditionally.
    if (acceptedGps)  consumePhoneGps();
    if (acceptedTime) syncTimeFromPhone(PhoneTimeEpoch);
    char out[64];
    snprintf(out, sizeof(out), "gps=%d time=%d", acceptedGps ? 1 : 0, acceptedTime ? 1 : 0);
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
    Serial.println("Current deviceUID: " + deviceUID);

    if (!isRegistered) {
      request->send(200, "application/json", "{\"registered\":false}");
      return;
    }

    // Validate token with Supabase (send only token, not device_uid)
    DynamicJsonDocument payloadDoc(256);
    payloadDoc["token"] = authToken;
    String payload;
    serializeJson(payloadDoc, payload);

    char responseBuf[2048];
    int httpCode = doCloudPOST("/functions/v1/validate-token", payload.c_str(),
                               responseBuf, sizeof(responseBuf));
    String response = String(responseBuf);

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
    serializeJson(doc, response);
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
    // User account info
    doc["username"] = request->getParam("username", true)->value();
    doc["email"] = request->getParam("email", true)->value();
    // Vessel info (17 fields)
    doc["boat_length_ft"] = request->getParam("boat_length_ft", true)->value().toFloat();
    // Guarded: old vessel_info.json predating these fields won't carry them, so a profile
    // update from a stale RAM cache would null-deref an unguarded getParam.
    doc["boat_displacement_lbs"] = request->hasParam("boat_displacement_lbs", true) ? request->getParam("boat_displacement_lbs", true)->value().toFloat() : 0.0f;
    doc["boat_type"] = request->getParam("boat_type", true)->value();
    doc["boat_make_model"] = request->getParam("boat_make_model", true)->value();
    doc["boat_year"] = request->getParam("boat_year", true)->value().toInt();
    doc["home_port"] = request->getParam("home_port", true)->value();
    doc["engine_make"] = request->getParam("engine_make", true)->value();
    doc["engine_hp"] = request->getParam("engine_hp", true)->value().toInt();
    doc["battery_voltage"] = request->getParam("battery_voltage", true)->value().toInt();
    doc["battery_capacity_ah"] = request->getParam("battery_capacity_ah", true)->value().toInt();
    doc["battery_type"] = request->getParam("battery_type", true)->value();
    doc["battery_make_model"] = request->hasParam("battery_make_model", true) ? request->getParam("battery_make_model", true)->value() : String("");
    doc["alternator_brand_model"] = request->getParam("alternator_brand_model", true)->value();
    doc["solar_watts"] = request->getParam("solar_watts", true)->value().toInt();
    doc["imu_mount_orientation"] = request->getParam("imu_mount_orientation", true)->value().toInt();
    doc["imu_dist_bow_ft"] = request->getParam("imu_dist_bow_ft", true)->value().toFloat();
    doc["imu_dist_cl_ft"] = request->getParam("imu_dist_cl_ft", true)->value().toFloat();
    doc["imu_height_wl_ft"] = request->getParam("imu_height_wl_ft", true)->value().toFloat();
    String payload;
    serializeJson(doc, payload);
    Serial.println("=== REGISTRATION REQUEST ===");
    Serial.printf("=== PRE-REGISTRATION HEAP ===\n");
    Serial.printf("Free internal: %u\n", ESP.getFreeHeap());
    Serial.printf("Max alloc internal: %u\n", ESP.getMaxAllocHeap());
    Serial.printf("Free PSRAM: %u\n", ESP.getFreePsram());

    char responseBuf[2048];
    int httpCode = doCloudPOST("/functions/v1/register-device", payload.c_str(),
                               responseBuf, sizeof(responseBuf));
    String response = String(responseBuf);

    Serial.println("Response code: " + String(httpCode));
    Serial.println("Response: " + response);

    // Connection-level failure (negative sentinels from doCloudPOST)
    if (httpCode <= 0) {
      Serial.println("HTTP connection failed: " + String(httpCode));
      queueConsoleMessageF("Registration failed: %s (code %d)", cloudFailReason(httpCode), httpCode);
      request->send(503, "application/json",
                    "{\"error\":\"Connection to cloud failed\",\"code\":" + String(httpCode) + "}");
      return;
    }

    // Empty body guard
    if (response.length() == 0) {
      Serial.println("Empty response from server (HTTP " + String(httpCode) + ")");
      queueConsoleMessageF("Registration failed: empty response from cloud (HTTP %d)", httpCode);
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

    // Vessel info (17 fields)
    doc["boat_length_ft"] = request->getParam("boat_length_ft", true)->value().toFloat();
    // Guarded: old vessel_info.json predating these fields won't carry them, so a profile
    // update from a stale RAM cache would null-deref an unguarded getParam.
    doc["boat_displacement_lbs"] = request->hasParam("boat_displacement_lbs", true) ? request->getParam("boat_displacement_lbs", true)->value().toFloat() : 0.0f;
    doc["boat_type"] = request->getParam("boat_type", true)->value();
    doc["boat_make_model"] = request->getParam("boat_make_model", true)->value();
    doc["boat_year"] = request->getParam("boat_year", true)->value().toInt();
    doc["home_port"] = request->getParam("home_port", true)->value();
    doc["engine_make"] = request->getParam("engine_make", true)->value();
    doc["engine_hp"] = request->getParam("engine_hp", true)->value().toInt();
    doc["battery_voltage"] = request->getParam("battery_voltage", true)->value().toInt();
    doc["battery_capacity_ah"] = request->getParam("battery_capacity_ah", true)->value().toInt();
    doc["battery_type"] = request->getParam("battery_type", true)->value();
    doc["battery_make_model"] = request->hasParam("battery_make_model", true) ? request->getParam("battery_make_model", true)->value() : String("");
    doc["alternator_brand_model"] = request->getParam("alternator_brand_model", true)->value();
    doc["solar_watts"] = request->getParam("solar_watts", true)->value().toInt();
    doc["imu_mount_orientation"] = request->getParam("imu_mount_orientation", true)->value().toInt();
    doc["imu_dist_bow_ft"] = request->getParam("imu_dist_bow_ft", true)->value().toFloat();
    doc["imu_dist_cl_ft"] = request->getParam("imu_dist_cl_ft", true)->value().toFloat();
    doc["imu_height_wl_ft"] = request->getParam("imu_height_wl_ft", true)->value().toFloat();

    String payload;
    serializeJson(doc, payload);

    Serial.println("=== UPDATE PROFILE REQUEST ===");
    Serial.println("Device UID: " + deviceUID);

    char responseBuf[2048];
    int httpCode = doCloudPOST("/functions/v1/update-profile", payload.c_str(),
                               responseBuf, sizeof(responseBuf));
    String response = String(responseBuf);

    Serial.println("Response code: " + String(httpCode));
    Serial.println("Response: " + response);

    if (httpCode <= 0) {
      queueConsoleMessageF("Profile update failed: %s (code %d)", cloudFailReason(httpCode), httpCode);
      request->send(503, "application/json",
                    "{\"success\":false,\"error\":\"Connection to cloud failed\",\"code\":" + String(httpCode) + "}");
      return;
    }
    request->send(httpCode, "application/json", response);
  });

  // Delete All Data endpoint
  server.on("/deleteAllData", HTTP_POST, [](AsyncWebServerRequest *request) {
    if (!request->hasParam("password", true) || strcmp(request->getParam("password", true)->value().c_str(), requiredPassword) != 0) {
      request->send(403, "text/plain", "Forbidden");
      return;
    }

    // Cloud requires auth_token to authorize the cascade delete
    String token = loadAuthToken();
    if (token.length() == 0) {
      request->send(400, "application/json", "{\"success\":false,\"error\":\"Not registered\"}");
      return;
    }

    DynamicJsonDocument doc(256);
    doc["token"] = token;

    String payload;
    serializeJson(doc, payload);

    Serial.println("=== DELETE ALL DATA REQUEST ===");

    char responseBuf[1024];
    int httpCode = doCloudPOST("/functions/v1/delete-user-data", payload.c_str(),
                               responseBuf, sizeof(responseBuf));
    String response = String(responseBuf);

    Serial.println("Response code: " + String(httpCode));
    Serial.println("Response: " + response);

    if (httpCode <= 0) {
      queueConsoleMessageF("Account delete failed: %s (code %d)", cloudFailReason(httpCode), httpCode);
      request->send(503, "application/json",
                    "{\"success\":false,\"error\":\"Connection to cloud failed\",\"code\":" + String(httpCode) + "}");
      return;
    }
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
    // PSRAM-backed buffer; shared_ptr deleter frees on response destruction
    // (normal completion OR abort). Avoids 10 KB transient on the internal heap.
    std::shared_ptr<char> bufPtr((char *)ps_malloc(10240), [](char *p) { if (p) free(p); });
    if (!bufPtr) { request->send(500, "text/plain", "OOM"); return; }
    char *buf = bufPtr.get();

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
        "\"wa\":%d,\"wp\":%d,\"wf\":%d,"
        "\"rpm\":%.0f,\"temp\":%.1f,\"worst\":%.1f,\"bv\":%.2f,\"cs\":%d,\"ts\":%u}",
        i > 0 ? "," : "",
        r.runNumber, r.score, r.activeTimeSec,
        r.kp, r.ki, r.kd,
        r.sampleDivisor, r.trackingGain, r.dutyRampRate,
        (int)r.waveAmplitude, (int)r.wavePeriod, (int)r.waveFloor,
        r.avgRPM, r.avgAltTempF, r.worstErrorA, r.battV, (int)r.chargeStage, (unsigned)r.epoch);
    }
    bool testActive = (TuningMode && tuningScore.toggleCount > 0);
    float ts = (tuningScore.activeTimeSec > 0.0f)
                 ? (tuningScore.errorAccum / tuningScore.activeTimeSec) : 0.0f;
    // "live" now carries the since-reset Control Accuracy score for this loop: [RMS error (A),
    // worst over-current (A), 0, 0]. (The old 4 ISE windows are gone — 4-slot shape kept for the
    // tuning UI parser.)
    pos += snprintf(buf + pos, 10240 - pos,
      "],\"live\":[%.2f,%.2f,%.2f,%.2f],"
      "\"ts\":%.2f,\"tt\":%d,\"ta\":%d}",
      accScoreRms(accCurrent.errAccum, accCurrent.timeAccum), accCurrent.worstOver, 0.0f, 0.0f,
      ts, (int)tuningScore.toggleCount, testActive ? 1 : 0);

    // Chunked send — only one TCP-MSS chunk (~1.5 KB) lands on the internal heap.
    size_t total = (size_t)pos;
    AsyncWebServerResponse *response = request->beginChunkedResponse(
      "application/json",
      [bufPtr, total](uint8_t *out, size_t maxLen, size_t index) -> size_t {
        if (index >= total) return 0;
        size_t toSend = (maxLen < (total - index)) ? maxLen : (total - index);
        memcpy(out, bufPtr.get() + index, toSend);
        return toSend;
      });
    request->send(response);
  });

  // Closed-loop Bode from the Tuning→Current sine auto-sweep. Small (≤10 points) → plain send.
  server.on("/tuningbode", HTTP_GET, [](AsyncWebServerRequest *request) {
    std::shared_ptr<char> bufPtr((char *)ps_malloc(2048), [](char *p) { if (p) free(p); });
    if (!bufPtr) { request->send(500, "text/plain", "OOM"); return; }
    char *buf = bufPtr.get();
    int pos = 0;
    pos += snprintf(buf + pos, 2048 - pos, "{\"pts\":[");
    for (int i = 0; i < tuningBodeCount && pos < 1900; i++) {
      pos += snprintf(buf + pos, 2048 - pos, "%s{\"f\":%.2f,\"g\":%.4f,\"ph\":%.1f}",
                      i > 0 ? "," : "",
                      tuningBode[i].freqHz, tuningBode[i].gain, tuningBode[i].phaseDeg);
    }
    pos += snprintf(buf + pos, 2048 - pos, "],\"active\":%d,\"done\":%d}",
                    tuningSweepActive ? 1 : 0, tuningSweepDone ? 1 : 0);
    request->send(200, "application/json", buf);
  });

  // Open-loop plant Bode from the SystemID sine sweep. Small (≤10 points) → plain send.
  server.on("/sysidbode", HTTP_GET, [](AsyncWebServerRequest *request) {
    std::shared_ptr<char> bufPtr((char *)ps_malloc(2048), [](char *p) { if (p) free(p); });
    if (!bufPtr) { request->send(500, "text/plain", "OOM"); return; }
    char *buf = bufPtr.get();
    int pos = 0;
    pos += snprintf(buf + pos, 2048 - pos, "{\"pts\":[");
    for (int i = 0; i < systemIDBodeCount && pos < 1900; i++) {
      pos += snprintf(buf + pos, 2048 - pos, "%s{\"f\":%.2f,\"g\":%.4f,\"ph\":%.1f}",
                      i > 0 ? "," : "",
                      systemIDBode[i].freqHz, systemIDBode[i].gainApPct, systemIDBode[i].phaseDeg);
    }
    bool active = (systemIDActive != 0 && systemIDTestType == 1);
    // "aborted" = protection-abort latch (systemIDAbortRequested), reported independently of "active":
    // a protection cut leaves systemIDActive set until systemID_tick clears it, and that tick is gated
    // out during the fault/lockout, so the plant-fit poller would otherwise wait the full 240s timeout.
    pos += snprintf(buf + pos, 2048 - pos, "],\"active\":%d,\"ready\":%d,\"aborted\":%d,\"amp\":%.1f}",
                    active ? 1 : 0,
                    (systemIDResultsReady && systemIDTestType == 1) ? 1 : 0,
                    systemIDAbortRequested ? 1 : 0,
                    SystemIDStepAmplitude);
    request->send(200, "application/json", buf);
  });

  // Auto-commissioning field-% curve: the duty→amps points + the saturation knee + the
  // proposed open-loop-sine settings. Dashboard shows these and lets the user Apply.
  server.on("/fieldcurve.json", HTTP_GET, [](AsyncWebServerRequest *request) {
    std::shared_ptr<char> bufPtr((char *)ps_malloc(2048), [](char *p) { if (p) free(p); });
    if (!bufPtr) { request->send(500, "text/plain", "OOM"); return; }
    char *buf = bufPtr.get();
    int pos = 0;
    pos += snprintf(buf + pos, 2048 - pos, "{\"pts\":[");
    for (int i = 0; i < fieldCurveCount && pos < 1700; i++) {
      pos += snprintf(buf + pos, 2048 - pos, "%s{\"d\":%.1f,\"a\":%.2f}",
                      i > 0 ? "," : "", fieldCurveBuf[i].duty, fieldCurveBuf[i].amps);
    }
    // "aborted" is the protection-abort latch (fieldCurveAbortRequested). It is reported INDEPENDENTLY
    // of "active" because a protection cut latches the abort but leaves fieldCurveActive set until
    // fieldCurve_tick next runs — and that tick is gated out during the fault/30s lockout, so "active"
    // can stay 1 indefinitely. The poller checks "aborted" so it never hangs waiting for !active.
    pos += snprintf(buf + pos, 2048 - pos,
                    "],\"active\":%d,\"ready\":%d,\"ok\":%d,\"kneeDuty\":%.1f,\"kneeAmps\":%.2f,"
                    "\"targetA\":%.1f,\"propStabA\":%.1f,\"propStepPct\":%.2f,\"ceilLimited\":%d,\"aborted\":%d,\"abort\":\"%s\"}",
                    fieldCurveActive != 0 ? 1 : 0, fieldCurveResultsReady ? 1 : 0, fieldCurveOk ? 1 : 0,
                    fieldCurveKneeDuty, fieldCurveKneeAmps, fieldCurveTargetLimitA,
                    fieldCurvePropStabA, fieldCurvePropStepPct, fieldCurveCeilingLimited ? 1 : 0,
                    fieldCurveAbortRequested ? 1 : 0, fieldCurveAbortMsg);
    request->send(200, "application/json", buf);
  });

  // Min% onset-knee sweep status + committed anchors (commissioning step polls this).
  server.on("/kneesweep.json", HTTP_GET, [](AsyncWebServerRequest *request) {
    std::shared_ptr<char> bufPtr((char *)ps_malloc(1024), [](char *p) { if (p) free(p); });
    if (!bufPtr) { request->send(500, "text/plain", "OOM"); return; }
    char *buf = bufPtr.get();
    int pos = 0;
    pos += snprintf(buf + pos, 1024 - pos,
                    "{\"active\":%d,\"ready\":%d,\"ok\":%d,\"kneeDuty\":%.1f,\"rpm\":%.0f,\"tempF\":%.0f,"
                    "\"fitResid\":%.2f,\"fitWorstIdx\":%d,\"anchors\":[",
                    (fieldCurveActive != 0 && fieldCurveOnsetMode) ? 1 : 0,
                    (fieldCurveResultsReady && fieldCurveOnsetMode) ? 1 : 0, kneeSweepOk ? 1 : 0,
                    kneeSweepKneeDuty, kneeSweepRPM, kneeSweepTempF, kneeFitResidPct, kneeFitWorstIdx);
    for (int i = 0; i < kneeAnchorN && pos < 900; i++) {
      pos += snprintf(buf + pos, 1024 - pos, "%s{\"rpm\":%.0f,\"duty\":%.1f,\"tempF\":%.0f}",
                      i > 0 ? "," : "", kneeAnchorRPM[i], kneeAnchorDuty[i], kneeAnchorTempF[i]);
    }
    // "aborted" = protection-abort latch, reported independently of "active" (see /fieldcurve.json note).
    // "ceilLimited" = the sweep stopped at the 24/48V field-duty ceiling before finding onset.
    pos += snprintf(buf + pos, 1024 - pos, "],\"ceilLimited\":%d,\"aborted\":%d,\"abort\":\"%s\"}",
                    fieldCurveCeilingLimited ? 1 : 0, fieldCurveAbortRequested ? 1 : 0, fieldCurveAbortMsg);
    request->send(200, "application/json", buf);
  });

  server.on("/tuningsweeplog", HTTP_GET, [](AsyncWebServerRequest *request) {
    const int CAP = 32768;
    std::shared_ptr<char> bufPtr((char *)ps_malloc(CAP), [](char *p) { if (p) free(p); });
    if (!bufPtr) { request->send(500, "text/plain", "OOM"); return; }
    char *buf = bufPtr.get();
    // Sort by bandwidth; never-tracks (<0) sinks to end.
    uint8_t sortIdx[50];
    for (int i = 0; i < tuningSweepLogCount; i++) sortIdx[i] = i;
    for (int i = 1; i < tuningSweepLogCount; i++) {
      uint8_t key = sortIdx[i];
      float kb = tuningSweepLog[key].bandwidthHz; if (kb < 0.0f) kb = -1e9f;
      int j = i - 1;
      while (j >= 0) {
        float pv = tuningSweepLog[sortIdx[j]].bandwidthHz; if (pv < 0.0f) pv = -1e9f;
        if (pv >= kb) break;
        sortIdx[j + 1] = sortIdx[j]; j--;
      }
      sortIdx[j + 1] = key;
    }
    int pos = 0;
    pos += snprintf(buf + pos, CAP - pos, "{\"rec\":[");
    for (int i = 0; i < tuningSweepLogCount && pos < CAP - 600; i++) {
      TuningSweepRecord &r = tuningSweepLog[sortIdx[i]];
      pos += snprintf(buf + pos, CAP - pos,
        "%s{\"n\":%d,\"bw\":%.2f,\"pg\":%.3f,\"pgf\":%.2f,\"wp\":%.0f,\"wpf\":%.2f,"
        "\"kp\":%.4f,\"ki\":%.4f,\"kd\":%.5f,\"f0\":%.2f,\"f1\":%.2f,\"cy\":%d,"
        "\"rpm\":%.0f,\"temp\":%.1f,"
        "\"amp\":%.2f,\"base\":%.2f,\"bv\":%.2f,\"rmin\":%.0f,\"rmax\":%.0f,"
        "\"coh\":%.3f,\"clip\":%d,\"cs\":%d,\"ts\":%u,\"pts\":[",
        i > 0 ? "," : "",
        r.runNumber, r.bandwidthHz, r.peakGain, r.peakGainFreqHz, r.worstPhaseDeg, r.worstPhaseFreqHz,
        r.kp, r.ki, r.kd, r.sweepStartHz, r.sweepEndHz, (int)r.cycles,
        r.avgRPM, r.avgAltTempF,
        r.sineAmpA, r.baseA, r.battV, r.rpmMin, r.rpmMax,
        r.worstCoherence, (int)r.dutyRailed, (int)r.chargeStage, (unsigned)r.epoch);
      for (int k = 0; k < r.nPoints && k < TUNING_SWEEP_NPOINTS && pos < CAP - 60; k++) {
        pos += snprintf(buf + pos, CAP - pos, "%s{\"f\":%.2f,\"g\":%.4f,\"ph\":%.1f}",
                        k > 0 ? "," : "", r.curve[k].freqHz, r.curve[k].gain, r.curve[k].phaseDeg);
      }
      pos += snprintf(buf + pos, CAP - pos, "]}");
    }
    pos += snprintf(buf + pos, CAP - pos, "],\"active\":%d,\"done\":%d}",
                    tuningSweepActive ? 1 : 0, tuningSweepDone ? 1 : 0);
    size_t total = (size_t)pos;
    AsyncWebServerResponse *response = request->beginChunkedResponse(
      "application/json",
      [bufPtr, total](uint8_t *out, size_t maxLen, size_t index) -> size_t {
        if (index >= total) return 0;
        size_t toSend = (maxLen < (total - index)) ? maxLen : (total - index);
        memcpy(out, bufPtr.get() + index, toSend);
        return toSend;
      });
    request->send(response);
  });

  server.on("/sysidsweeplog", HTTP_GET, [](AsyncWebServerRequest *request) {
    const int CAP = 32768;
    std::shared_ptr<char> bufPtr((char *)ps_malloc(CAP), [](char *p) { if (p) free(p); });
    if (!bufPtr) { request->send(500, "text/plain", "OOM"); return; }
    char *buf = bufPtr.get();
    // Sort by roll-off; <0 sinks to end.
    uint8_t sortIdx[50];
    for (int i = 0; i < sysidSweepLogCount; i++) sortIdx[i] = i;
    for (int i = 1; i < sysidSweepLogCount; i++) {
      uint8_t key = sortIdx[i];
      float kb = sysidSweepLog[key].rolloffHz; if (kb < 0.0f) kb = -1e9f;
      int j = i - 1;
      while (j >= 0) {
        float pv = sysidSweepLog[sortIdx[j]].rolloffHz; if (pv < 0.0f) pv = -1e9f;
        if (pv >= kb) break;
        sortIdx[j + 1] = sortIdx[j]; j--;
      }
      sortIdx[j + 1] = key;
    }
    int pos = 0;
    pos += snprintf(buf + pos, CAP - pos, "{\"rec\":[");
    for (int i = 0; i < sysidSweepLogCount && pos < CAP - 600; i++) {
      SysIDSweepRecord &r = sysidSweepLog[sortIdx[i]];
      pos += snprintf(buf + pos, CAP - pos,
        "%s{\"n\":%d,\"ro\":%.2f,\"dc\":%.4f,\"wp\":%.0f,\"wpf\":%.2f,"
        "\"amp\":%.1f,\"floor\":%.1f,\"f0\":%.2f,\"f1\":%.2f,\"cy\":%d,"
        "\"rpm\":%.0f,\"temp\":%.1f,\"bv\":%.2f,\"cs\":%d,\"ts\":%u,\"pts\":[",
        i > 0 ? "," : "",
        r.runNumber, r.rolloffHz, r.dcGainApPct, r.worstPhaseDeg, r.worstPhaseFreqHz,
        r.setupAmplitude, r.stabilizeAmps, r.sweepStartHz, r.sweepEndHz, (int)r.cycles,
        r.avgRPM, r.avgAltTempF, r.battV, (int)r.chargeStage, (unsigned)r.epoch);
      for (int k = 0; k < r.nPoints && k < SYSID_SINE_NPOINTS && pos < CAP - 60; k++) {
        pos += snprintf(buf + pos, CAP - pos, "%s{\"f\":%.2f,\"g\":%.4f,\"ph\":%.1f}",
                        k > 0 ? "," : "", r.curve[k].freqHz, r.curve[k].gainApPct, r.curve[k].phaseDeg);
      }
      pos += snprintf(buf + pos, CAP - pos, "]}");
    }
    bool active = (systemIDActive != 0 && systemIDTestType == 1);
    pos += snprintf(buf + pos, CAP - pos, "],\"active\":%d}", active ? 1 : 0);
    size_t total = (size_t)pos;
    AsyncWebServerResponse *response = request->beginChunkedResponse(
      "application/json",
      [bufPtr, total](uint8_t *out, size_t maxLen, size_t index) -> size_t {
        if (index >= total) return 0;
        size_t toSend = (maxLen < (total - index)) ? maxLen : (total - index);
        memcpy(out, bufPtr.get() + index, toSend);
        return toSend;
      });
    request->send(response);
  });

  server.on("/systemidlog", HTTP_GET, [](AsyncWebServerRequest *request) {
    // PSRAM-backed buffer (50 records × ~200 bytes ≈ 10 KB).
    std::shared_ptr<char> bufPtr((char *)ps_malloc(10240), [](char *p) { if (p) free(p); });
    if (!bufPtr) { request->send(500, "text/plain", "OOM"); return; }
    char *buf = bufPtr.get();

    // Sort by score (lower = faster plant = better). Aborted runs (-1) sink to end.
    uint8_t sortIdx[50];
    for (int i = 0; i < systemIDLogCount; i++) sortIdx[i] = i;
    for (int i = 1; i < systemIDLogCount; i++) {
      uint8_t key = sortIdx[i];
      float keyScore = systemIDLog[key].score;
      if (keyScore < 0.0f) keyScore = 1e9f;  // aborted → sort last
      int j = i - 1;
      while (j >= 0) {
        float prev = systemIDLog[sortIdx[j]].score;
        if (prev < 0.0f) prev = 1e9f;
        if (prev <= keyScore) break;
        sortIdx[j + 1] = sortIdx[j];
        j--;
      }
      sortIdx[j + 1] = key;
    }

    int pos = 0;
    pos += snprintf(buf + pos, 10240 - pos, "{\"rec\":[");
    for (int i = 0; i < systemIDLogCount && pos < 9800; i++) {
      SystemIDRecord &r = systemIDLog[sortIdx[i]];
      pos += snprintf(buf + pos, 10240 - pos,
        "%s{\"n\":%u,\"s\":%.1f,"
        "\"rd\":[%.0f,%.0f,%.0f],\"fd\":[%.0f,%.0f,%.0f],"
        "\"ra\":%.1f,\"fa\":%.1f,"
        "\"sa\":[%.2f,%.2f,%.2f],\"qp\":[%.3f,%.3f,%.3f],"
        "\"ar\":%u,\"ap\":%u,\"amp\":%.2f,"
        "\"rpm\":%.0f,\"temp\":%.1f,\"bv\":%.2f,\"cs\":%d,\"ts\":%u}",
        i > 0 ? "," : "",
        (unsigned)r.runNumber, r.score,
        r.riseDelays[0], r.riseDelays[1], r.riseDelays[2],
        r.fallDelays[0], r.fallDelays[1], r.fallDelays[2],
        r.riseAvg_ms, r.fallAvg_ms,
        r.stepAmps[0], r.stepAmps[1], r.stepAmps[2],
        r.quietPP[0], r.quietPP[1], r.quietPP[2],
        (unsigned)r.abortReason, (unsigned)r.abortPhase, r.setupStepAmplitude,
        r.avgRPM, r.avgAltTempF, r.battV, (int)r.chargeStage, (unsigned)r.epoch);
    }
    pos += snprintf(buf + pos, 10240 - pos,
      "],\"active\":%d,\"ready\":%d}",
      (int)systemIDActive, systemIDResultsReady ? 1 : 0);

    size_t total = (size_t)pos;
    AsyncWebServerResponse *response = request->beginChunkedResponse(
      "application/json",
      [bufPtr, total](uint8_t *out, size_t maxLen, size_t index) -> size_t {
        if (index >= total) return 0;
        size_t toSend = (maxLen < (total - index)) ? maxLen : (total - index);
        memcpy(out, bufPtr.get() + index, toSend);
        return toSend;
      });
    request->send(response);
  });

  server.on("/cvtuninglog", HTTP_GET, [](AsyncWebServerRequest *request) {
    // PSRAM-backed buffer (50 records × ~420 bytes ≈ 21 KB). shared_ptr deleter runs
    // when the chunked-response lambda is destroyed (normal completion OR abort), so
    // no manual free is needed and aborted requests can't leak.
    std::shared_ptr<char> bufPtr((char *)ps_malloc(32768), [](char *p) { if (p) free(p); });
    if (!bufPtr) { request->send(500, "text/plain", "OOM"); return; }
    char *buf = bufPtr.get();

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
    pos += snprintf(buf + pos, (pos >= 32768 ? 0 : 32768 - pos), "{\"rec\":[");
    for (int i = 0; i < cvTuningLogCount && pos < 32000; i++) {
      CVTuningRecord &r = cvTuningLog[sortIdx[i]];
      pos += snprintf(buf + pos, (pos >= 32768 ? 0 : 32768 - pos),
        "%s{\"n\":%d,\"s\":%.2f,\"st\":%.1f,\"wo\":%.3f,\"io\":%.4f,\"t\":%.1f,"
        "\"ls\":%.2f,\"lst\":%.1f,\"lwo\":%.3f,\"lio\":%.4f,\"lus\":%.3f,"
        "\"fov\":%d,\"iex\":%d,\"ld\":%d,\"hoc\":%d,"
        "\"vkp\":%.3f,\"vki\":%.3f,\"vkd\":%.2f,"
        "\"srr\":%.1f,\"sfr\":%.1f,"
        "\"abl\":%.2f,\"arl\":%.3f,\"asp\":%d,\"irf\":%.2f,"
        "\"ks\":%.1f,\"kh\":%.1f,"
        "\"iefr\":%.3f,\"ietau\":%.0f,\"iekb\":%.2f,"
        "\"lddt\":%.0f,\"ldt1\":%.0f,\"ldt3\":%.0f,"
        "\"tc\":%.0f,\"wa\":%.2f,\"wp\":%d,\"ko\":%.1f,\"cr\":%d,"
        "\"rpm\":%.0f,\"tmp\":%.1f,\"bv\":%.2f,\"soc\":%.1f,\"cvt\":%.2f,\"cs\":%d,\"ts\":%u}",
        i > 0 ? "," : "",
        r.runNumber, r.score, r.avgSettlingTimeSec, r.worstOvershootV,
        r.avgIntegratedOvershootVs, r.activeTimeSec,
        r.lowScore, r.avgLowSettlingTimeSec, r.worstLowOvV, r.avgLowIntOvVs, r.worstLowUndershootV,
        (int)r.fastOvFires, (int)r.iExcessFires, (int)r.loadDumpFires, (int)r.hardOcFires,
        r.voltageKp, r.voltageKi, r.voltageKd,
        r.setpointRiseRate, r.setpointFallRate,
        r.awBleedRate, r.awRecoverRate, (int)r.awSeedProtectMs, r.reseedFrac,
        r.slopeBleedK, r.kHard,
        r.iExcessFrac, r.iExcessTau, r.iExcessKBleed,
        r.loadDumpDtThresh, r.loadDumpDtThresh1, r.loadDumpDtThresh3,
        r.inputFilterTC, r.waveAmplitudeV, (int)r.wavePeriodSec, r.kOvershoot, (int)r.consecutiveReads,
        r.avgRPM, r.avgAltTempF, r.battVAtStart, r.socAtStart * 100.0f, r.chargingVoltageTarget,
        (int)r.chargeStage, (unsigned)r.epoch);
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
    // "live" now carries the since-reset Control Accuracy score for the CV loop: [RMS error (mV),
    // worst over-voltage (mV), 0, 0]. (4-slot shape kept for the tuning UI parser.)
    pos += snprintf(buf + pos, (pos >= 32768 ? 0 : 32768 - pos),
      "],\"live\":[%.0f,%.0f,%.0f,%.0f],"
      "\"ts\":%.2f,\"tc\":%d,\"ta\":%d}",
      accScoreRms(accVoltage.errAccum, accVoltage.timeAccum) * 1000.0f, accVoltage.worstOver * 1000.0f, 0.0f, 0.0f,  // mV
      cvts, (int)cvTuningScore.scoredHighCount, cvTestActive ? 1 : 0);

    // Chunked send: only ~1.5 KB per chunk lands on the internal heap. bufPtr is
    // captured by value (ref-count) so the PSRAM buffer lives for the duration of
    // the response and is freed automatically when the lambda is destroyed.
    size_t total = (size_t)pos;
    AsyncWebServerResponse *response = request->beginChunkedResponse(
      "application/json",
      [bufPtr, total](uint8_t *out, size_t maxLen, size_t index) -> size_t {
        if (index >= total) return 0;
        size_t toSend = (maxLen < (total - index)) ? maxLen : (total - index);
        memcpy(out, bufPtr.get() + index, toSend);
        return toSend;
      });
    request->send(response);
  });

  server.on("/resetcvtuninglog", HTTP_POST, [](AsyncWebServerRequest *request) {
    cvTuningLogCount     = 0;
    cvTuningLogHead      = 0;
    cvTuningRunCounter   = 0;
    cvTuningScore        = {};
    cvTuningParamChanged = false;
    if (cvTuningLog) memset(cvTuningLog, 0, 50 * sizeof(CVTuningRecord));
    accVoltage = {};  // clear the CV loop's Control Accuracy score too
    pendingSaveCVTuningLog = true;  // deferred to Core 1 — avoids blocking Core 0 SSE
    request->send(200, "text/plain", "OK");
  });

  server.on("/resettuninglog", HTTP_POST, [](AsyncWebServerRequest *request) {
    tuningLogCount    = 0;
    tuningLogHead     = 0;
    tuningRunCounter  = 0;
    tuningScore       = {};
    tuningParamChanged = false;
    if (tuningLog) memset(tuningLog, 0, 50 * sizeof(TuningRecord));
    accCurrent = {};  // clear the inner current loop's Control Accuracy score too
    pendingSaveTuningLog = true;  // deferred to Core 1 — avoids blocking Core 0 SSE
    request->send(200, "text/plain", "OK");
  });

  // Manual "Reset" button under the Control Accuracy Scores panel. Zeros all three loops'
  // since-reset accumulators. (They also auto-reset after each config-snapshot upload.)
  server.on("/resetAccuracyScores", HTTP_POST, [](AsyncWebServerRequest *request) {
    resetAccuracyScores();
    request->send(200, "text/plain", "OK");
  });

  server.on("/resetsystemidlog", HTTP_POST, [](AsyncWebServerRequest *request) {
    systemIDLogCount    = 0;
    systemIDLogHead     = 0;
    systemIDRunCounter  = 0;
    if (systemIDLog) memset(systemIDLog, 0, 50 * sizeof(SystemIDRecord));
    pendingSaveSystemIDLog = true;  // deferred to Core 1 — avoids blocking Core 0 SSE
    request->send(200, "text/plain", "OK");
  });

  server.on("/resettuningsweeplog", HTTP_POST, [](AsyncWebServerRequest *request) {
    tuningSweepLogCount   = 0;
    tuningSweepLogHead    = 0;
    tuningSweepRunCounter = 0;
    if (tuningSweepLog) memset(tuningSweepLog, 0, 50 * sizeof(TuningSweepRecord));
    pendingSaveTuningSweepLog = true;  // deferred to Core 1
    request->send(200, "text/plain", "OK");
  });

  server.on("/resetsysidsweeplog", HTTP_POST, [](AsyncWebServerRequest *request) {
    sysidSweepLogCount   = 0;
    sysidSweepLogHead    = 0;
    sysidSweepRunCounter = 0;
    if (sysidSweepLog) memset(sysidSweepLog, 0, 50 * sizeof(SysIDSweepRecord));
    pendingSaveSysidSweepLog = true;  // deferred to Core 1
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

  // Cloud Features debug (raw NVS, no Preferences)
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

    // Redact token to last 4 chars - this endpoint is unauthenticated; the full
    // token would let anyone on the LAN impersonate the device against the cloud.
    auto tail4 = [](const char *s) -> const char * {
      size_t n = strlen(s);
      return (n > 4) ? (s + n - 4) : s;
    };
    char out[512];
    const char *globalTok = authToken.c_str();
    snprintf(out, sizeof(out),
             "isRegistered: %d\nauthToken global: ...%s (len=%u)\nNVS stored token: ...%s (len=%u)\n",
             (int)isRegistered,
             globalTok && *globalTok ? tail4(globalTok) : "(empty)",
             (unsigned)strlen(globalTok),
             haveStored ? tail4(storedToken) : "(empty)",
             haveStored ? (unsigned)strlen(storedToken) : 0u);
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
// Engine-off standby power drop. Normally powers WiFi fully off and slows the CPU to 80MHz. With
// WiFi Napping enabled in Client mode it instead keeps WiFi associated in modem-sleep so the
// dashboard stays reachable with no button press — indefinitely while the regulator stays on the
// router. Nap costs only ~1mA over full-off (measured), so there is no idle timeout. While napping
// the CPU bumps to 240MHz whenever a dashboard is connected (snappy UI) and drops to 80MHz when
// idle. The temp task is NO LONGER suspended here — the DS18B20/OneWire path reads fine at 80MHz
// (all its timing is wall-clock, not CPU-cycle, based), and the task self-throttles to a 10-min
// cadence while the engine is off (see TempTask), which keeps alt temperature fresh for the
// zero-drift log at negligible standby power. Modem-sleep stays on, so there's still ~100-300ms
// beacon latency on the first packet, which is fine. Ordering matters: do the WiFi op, then set the
// clock.
void enterLowPowerStandby() {
  if (wifiNapEnabled && currentMode == MODE_CLIENT) {
    if (!wifiNapActive) {
      WiFi.setSleep(true);           // modem sleep — WiFi naps between DTIM beacons
      wifiNapActive = true;
      queueConsoleMessage("WiFi napping: dashboard stays reachable at low power (no button needed)");
    }
  } else {
    WiFi.mode(WIFI_OFF);             // THIS MUST BE DONE FIRST
    wifiNapActive = false;           // napping disabled (or AP mode) — clear stale nap state
  }
  // Temp task intentionally NOT suspended (reads fine at 80MHz; self-throttles to 10 min engine-off).
  // While napping, run 240MHz when a dashboard is actually connected (snappy UI), else 80MHz.
  // WiFi-off standby always stays 80MHz. Guarded so the PLL isn't reconfigured every loop pass.
  uint32_t targetMhz = (wifiNapActive && events.count() > 0) ? 240 : 80;
  if (getCpuFrequencyMhz() != targetMhz) {
    setCpuFrequencyMhz(targetMhz);   // THIS MUST BE DONE AFTER THE WIFI OP
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
  if (cachedCpuFreq < 81 && !wifiNapActive) return;  // napping reconnects at 80MHz so a router drop can't strand it

  // === END THROTTLE ===

  if (cachedWiFiStatus == WL_CONNECTED) {
    // Update signal strength while connected
    wifiRecon.lastSignalStrength = cachedWiFiRSSI;

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
    if (now - wifiRecon.lastAttempt < 300000) return;
    Serial.println("WiFi: Exiting give-up mode, attempting fresh reconnection burst");
    wifiRecon.giveUpMode = false;
    wifiRecon.attemptCount = 0;
    wifiRecon.currentInterval = wifiRecon.minInterval;
  }

  // Signal strength awareness
  if (wifiRecon.lastSignalStrength != -999 && wifiRecon.lastSignalStrength < wifiRecon.minSignalThreshold) {
    if (now - wifiRecon.lastAttempt < 60000) return;
    Serial.printf("WiFi: Poor signal (%d dBm), using extended retry interval\n", wifiRecon.lastSignalStrength);
  } else {
    if (now - wifiRecon.lastAttempt < wifiRecon.currentInterval) return;
  }

  wifiRecon.lastAttempt = now;
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
  static unsigned long lastpayload4send = 0;
  static unsigned long lastTimestampSend = 0;
  const unsigned long EVENTSOURCE_COOLDOWN = 10;
  const unsigned long CONSOLE_MESSAGE_INTERVAL = 1000;

  bool canSendNow = (now - lastEventSourceSend >= EVENTSOURCE_COOLDOWN);
  if (!canSendNow) return;
  if (gHeavyRanThisPass) return;   // one-heavy-per-pass gate; defer the send (cooldown unchanged → retries next pass)
  gHeavyRanThisPass = true;

  bool sentSomething = false;

  // One-shot boot announcement. Emitted only once a browser is connected, so it
  // survives the 10-slot console queue churning during boot and lands right after
  // the SSE auto-reconnect that follows a reboot — exactly the remote-debug case.
  static bool bootLineSent = false;
  if (!bootLineSent && events.count() > 0) {
    bootLineSent = true;
    queueConsoleMessageF("BOOTED firmware v%s (reset: %s)", FIRMWARE_VERSION, resetReasonName());
  }

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
    // Consume the protection-event latch for this frame — clear only the bits we read so
    // an event firing on the control-loop core mid-build isn't lost.
    uint8_t protMask = g_protEventLatch;
    g_protEventLatch &= ~protMask;

    // The "Alt Current filtered" plot trace must show the SAME signal the CC current PID
    // acts on — the OutputPIDSigSrc-selected PV (mirrors the ternary in AdjustFieldLearnMode),
    // not MeasuredAmps_filtered's InputFilterTC display EMA.
    float pidAltPV = (OutputPIDSigSrc == 2) ? MeasuredAmps : (OutputPIDSigSrc == 1) ? g_pidMA_N
                                                                                    : g_pidI_filtered;

    int payload1Len = snprintf(payload1, PAYLOAD1_SIZE,
                               "%d,"  // CSV1_FIELD_COUNT
                               "%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,"
                               "%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,"
                               "%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,"
                               "%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d",  // +2: mExcessEmaPeak, iExcessThreshMin

                               CSV1_FIELD_COUNT,
                               SafeInt(AlternatorTemperatureF, 100),
                               SafeInt(dutyCycle, 100),
                               SafeInt(BatteryV, 100),
                               SafeInt(MeasuredAmps, 100),
                               SafeInt(RPM),
                               SafeInt(Channel3V, 100),
                               SafeInt(IBV, 100),
                               SafeInt(Bcur, 100),
                               SafeInt(VictronVoltage, 100),
                               SafeInt(LoopTime),
                               SafeInt(WifiHeartBeat),
                               SafeInt(vvout, 100),
                               SafeInt(iiout, 100),
                               SafeInt(FreeHeap),
                               SafeInt(Alarm_Status),
                               SafeInt(fieldActiveStatus),
                               SafeInt((int)currentMode),
                               SafeInt(stateRevision),
                               SafeInt(setpointLimited, 100),
                               SafeInt(uTargetAmps, 100),
                               SafeInt(pidInput, 100),
                               SafeInt(pidOutput, 100),
                               SafeInt(pidError, 100),
                               SafeInt(imu_heel_deg, 100),
                               SafeInt(imu_pitch_deg, 100),
                               SafeInt(imu_vertical_accel_g, 1000),
                               SafeInt(imu_yaw_rate_dps, 100),
                               SafeInt(imu_total_accel_g, 1000),
                               SafeInt((int32_t)((millis() - perfCountersResetMs) / 1000UL)),  // seconds since perf-counters reset (0 = boot)
                               SafeInt(shutdownPhase),
                               SafeInt(BatteryV, 100),                  // raw ADS1115
                               SafeInt(pidAltPV, 100),  // CSV1_MeasuredAmps_filtered — CC PID PV (OutputPIDSigSrc-selected)
                               SafeInt(ChargingVoltageTarget * 100),
                               SafeInt(Icv * 100),
                               // Water depth in feet ×10 (0.1 ft resolution). 0 if NMEA depth stale or unavailable.
                               SafeInt(IS_STALE(IDX_WATER_DEPTH) ? 0 : (WaterDepth_m * 3.28084f), 10),
                               SafeInt(Ignition),  // effective ignition (override applied at top of loop)
                               SafeInt(g_mExcessEma, 10),       // CSV1_mExcessEma — averaged current excess (A ×10)
                               SafeInt(g_iExcessThreshold, 10), // CSV1_iExcessThreshold — fire threshold E (A ×10)
                               SafeInt(g_iExcessArmedWin ? g_mExcessEmaPeak : g_mExcessEma, 10),       // CSV1_mExcessEmaPeak (A ×10)
                               SafeInt(g_iExcessArmedWin ? g_iExcessThreshWinMin : g_iExcessThreshold, 10), // CSV1_iExcessThreshMin (A ×10)
                               SafeInt(protMask)                // CSV1_protEventMask — protection-event bits this frame
    );
    // Reset the per-frame iExcess sparkline aggregates now that they've been captured.
    g_mExcessEmaPeak = 0.0f;
    g_iExcessThreshWinMin = 0.0f;
    g_iExcessArmedWin = false;
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
  // PRIORITY 2: CSVData4 / NavStream — live nav/wind/solar/fuel at 2 Hz (500 ms).
  // High priority (right after CSV1) so these gauges aren't starved by the slower CSV2/CSV3/TS
  // channels. These fields used to ride the 5 s CSV2 cadence and looked frozen on the dial/helm.
  if (!sentSomething && now - lastpayload4send >= 500UL && events.count() > 0) {
    static char *payload4 = nullptr;
    static const size_t PAYLOAD4_SIZE = 256;  // (17 fields + 1) × 7 = 126, rounded up with headroom
    if (!payload4) {
      payload4 = (char *)ps_malloc(PAYLOAD4_SIZE);  // allocated to PSRAM
      if (!payload4) {
        Serial.println("FATAL: payload4 ps_malloc failed");
        return;
      }
    }
    int payload4Len = snprintf(payload4, PAYLOAD4_SIZE,
                               "%d,"  // CSV4_FIELD_COUNT
                               "%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,"
                               "%d,%d,%d,%d,%d,%d,%d",

                               CSV4_FIELD_COUNT,
                               SafeInt(HeadingNMEA),                     // CSV4_HeadingNMEA
                               SafeInt(SOGNMEA, 100),                    // CSV4_SOGNMEA
                               SafeInt(COGNMEA),                         // CSV4_COGNMEA
                               SafeInt(STWNMEA, 100),                    // CSV4_STWNMEA (knots ×100; NAN/no-log -> 0)
                               SafeInt(ApparentWindSpeedNMEA, 100),      // CSV4_ApparentWindSpeedNMEA
                               SafeInt(ApparentWindAngleNMEA),           // CSV4_ApparentWindAngleNMEA
                               SafeInt(TrueWindSpeedNMEA, 100),          // CSV4_TrueWindSpeedNMEA
                               SafeInt(TrueWindAngleNMEA),               // CSV4_TrueWindAngleNMEA
                               SafeInt(LeewayNMEA),                      // CSV4_LeewayNMEA
                               SafeInt(VMGNMEA, 100),                    // CSV4_VMGNMEA
                               SafeInt(VMGUpwind, 100),                  // CSV4_VMGUpwind (VMG to windward, knots ×100)
                               SafeInt(VictronSolarPower_W),             // CSV4_VictronSolarPower
                               SafeInt(VictronSolarVoltage_V, 100),      // CSV4_VictronSolarVoltage
                               SafeInt(VictronSolarCurrent_A, 100),      // CSV4_VictronSolarCurrent
                               SafeInt(VictronCurrent, 100),             // CSV4_VictronCurrent
                               SafeInt(currentFuelGPH, 100),             // CSV4_currentFuelGPH
                               SafeInt(currentNMPG, 100)                 // CSV4_currentNMPG
    );
    if (payload4Len < 0 || payload4Len >= PAYLOAD4_SIZE) {
      Serial.printf("payload4 truncated or format error: %d\n", payload4Len);
      return;
    }
    events.send(payload4, "CSVData4");
    lastpayload4send = now;
    lastEventSourceSend = now;
    sentSomething = true;
  }
  // PRIORITY 3: Console
  trySendConsoleSSE(sentSomething, now);
  // PRIORITY 4: CSVData2 (status/settings data).
  // During a plant delay test, bypass the sentSomething gate and send every 500 ms so the
  // UI phase-progress display stays within one poll tick of the actual firmware state.
  // Normal operation: every 5 s, gated behind CSV1 to avoid double-sending per tick.
  const bool sysIDRunning = (systemIDActive != 0);
  if ((sysIDRunning || !sentSomething) && now - lastpayload2send >= (sysIDRunning ? 500UL : 5000UL) && events.count() > 0) {
    static char *payload2 = nullptr;
    static const size_t PAYLOAD2_SIZE = 3800;  // (505 fields + 1) × 7 = 3542, rounded up to 3800
    static int payload2Len = 0;       // persists between the build pass and the send pass
    static uint8_t csv2Phase = 0;     // 0 = build this due pass, 1 = send the built payload next pass
    if (!payload2) {
      payload2 = (char *)ps_malloc(PAYLOAD2_SIZE);  // allocated to PSRAM
      if (!payload2) {
        Serial.println("FATAL: payload2 ps_malloc failed");
        return;
      }
    }
    if (csv2Phase == 0) {
    // PASS 1 — build only. The snprintf is the ~4.3 ms cost; the events.send is deferred to PASS 2
    // (next pass) so build + send never stack in one loop tick (CH1/Vbus loop-stall work).
    WifiStrength = cachedWiFiRSSI;
    ch1_compute_stats();
    pidFire_compute_stats();
    voltLoop_compute_stats();
    // Format string:
    uint32_t _csv2b0 = micros();   // CSV2 build-cost timer (snprintf)
    payload2Len = snprintf(payload2, PAYLOAD2_SIZE,
                               "%d,"  // CSV2_FIELD_COUNT
                               "%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,"
                               "%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,"
                               "%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,"
                               "%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,"
                               "%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,"
                               // -1 %d: VictronCurrent moved to CSV4 (it sat before the %u,%u device-id pair)
                               "%d,%d,%u,%u,%d,%d,%d,%d,%d,"
                               // -9 %d: COG/SOG/AWS/AWA/TWS/TWA/Leeway/VMGNMEA + HeadingNMEA moved to CSV4
                               "%d,"
                               // -10 %d: one 10-specifier row dropped here (Control Accuracy Scores v2 net change)
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
                               "%d,%d,%d,%d,"
                               // net -12 specifiers parked here (count-only; field identity is by CSV2_FIELDS order)
                               // 28 ignition-cycle watermark fields (14 lo + 14 hi)
                               "%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,"
                               "%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,"
                               "%d,%d,%d,%d,%d,%d,%d,%d,"
                               // restartRemainingSec + GPS/time source labels + loggingActive
                               "%d,%d,%d,%d,"
                               // sustainedTWS + currentGaleMinutes + 2 VMG watermark pairs (lo/hi) (VMGUpwind moved to CSV4)
                               // ...4 alt-health + 2 imu-zero offsets
                               "%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,"
                               // Victron VE.Direct solar/MPPT live block (7 fields — PPV/VPV/derived-current moved to CSV4)
                               "%d,%d,%d,%d,%d,%d,%d,"
                               // (live engine fuel flow + economy — currentFuelGPH/currentNMPG — moved to CSV4)
                               // session fuel-economy curve (18 RPM bins + top-RPM scale)
                               "%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,"
                               // +4 specifiers to balance the count after inserting ft_altFold + ft_boatPerf
                               // win/ses mid-list (args emit positionally; field identity is via CSV2_FIELDS order)
                               "%d,%d,%d,%d,"
                               // +4: 80MHz low-power loop instrumentation (worst_win, worst_ses, over-limit count, total iters)
                               "%d,%d,%d,%d,"
                               // +2: field-ON loop instrumentation (worst_win, worst_ses)
                               "%d,%d,"
                               // (Speed Through Water STW/SOW moved to CSV4)
                               // +4: thermal tuning live-stream fields (tempFiltered, impliedPenalty, flags, antiWindup latch)
                               "%d,%d,%d,%d,"
                               // +10: inner-current-PID firing interval (field-on), CH1-style stats
                               "%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,"
                               // +4: I2C bus-health (inaBusReadWorstUs, inaBusSlowCount, ina228ErrorCount, imuFifoFetchWorstUs)
                               "%d,%d,%d,%d,"
                               // +8 net: CV voltage-loop ladder is 10 vl_* fields, but reuses the 2 specifiers freed by the
                               // removed 2-row voltLoop watermarks (still sitting in the generic blocks above), so only +8 here.
                               "%d,%d,%d,%d,%d,%d,%d,%d,"
                               // +1: imuFifoWorstSamples (sample count at the worst IMU fetch — bus-stall vs transfer-size diag)
                               "%d,"
                               // +2: long-term-ring flash-flush timer (worst window / session, µs)
                               "%d,%d,"
                               // +18: fast alt-current channel (drain timer ×2, flush timer ×2, detector timer ×2, window-finalize timer ×2, state, cells, detectK, session worsts ×3, anomaly count, Highest Tone in Map ×3 [freq, amp, rpm])
                               "%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,"
                               // +7: operating-point context for the two headline worsts (Highest Tone amps/temp/epoch, Session Pk-Pk rpm/amps/temp/epoch)
                               "%d,%d,%d,%d,%d,%d,%d,"
                               // +12: gate-tuning 10s live readouts (RPM edge margin, amps-drift spread, amps-drift gate excess, tone peak, current slew, voltage slope, ripple-capture cmd/alt/batt gate excess + rpm margin + 2 admit counters)
                               "%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,"
                               // +5: lifetime nav/sailing records (longest trip, max 24h dist, deepest anchorage, best upwind VMG, longest gale)
                               "%d,%d,%d,%d,%d,"
                               // +12: inner-current-loop live accuracy (×4, amps²×10000) + CV-voltage-loop RMS error (×4, mV) + CV peak overshoot (×4, mV), 1m/10m/100m/1000m windows each
                               "%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,"
                               // +1: live CPU clock (80 or 240 MHz)
                               "%d,"
                               // +4: ADS slow-channel gap meters (ch0/ch2 last+worst, ms)
                               "%d,%d,%d,%d,"
                               // +4: CSV2 send-cost (build last/worst, send last/worst, µs)
                               "%d,%d,%d,%d,"
                               // +2: Core-0 HTTPS task cloud op time (last/worst, ms)
                               "%d,%d,"
                               // +1: live battery-temp CV gain derate multiplier (×1000)
                               "%d",

                               CSV2_FIELD_COUNT,
                               SafeInt(IBVMax, 100),
                               SafeInt(MeasuredAmpsMax, 100),
                               SafeInt(RPMMax),
                               SafeInt(SOC_percent),
                               SafeInt(EngineRunTime * 100 / 3600, 1),
                               SafeInt(AlternatorOnTime * 100 / 3600, 1),
                               SafeInt(AlternatorFuelUsed, 100),
                               SafeInt(ChargedEnergy),
                               SafeInt(DischargedEnergy),
                               SafeInt(AlternatorChargedEnergy),
                               SafeInt(MaxAlternatorTemperatureF),
                               SafeInt(temperatureThermistor),
                               SafeInt(MaxTemperatureThermistor),
                               SafeInt(timeToFullChargeMin),
                               SafeInt(timeToFullDischargeMin),
                               SafeInt(LatitudeNMEA * 1000000),
                               SafeInt(LongitudeNMEA * 1000000),
                               SafeInt(SatelliteCountNMEA),
                               SafeInt(LastSessionDuration),
                               SafeInt(LastSessionMaxLoopTime),
                               SafeInt(lastSessionMinHeap),
                               SafeInt(wifiReconnectsTotal),
                               SafeInt(LastResetReason),
                               SafeInt(ancientResetReason),
                               SafeInt(totalPowerCycles),
                               SafeInt(MinFreeHeap),
                               SafeInt(currentWeatherMode),
                               SafeInt(UVToday, 100),
                               SafeInt(UVTomorrow, 100),
                               SafeInt(UVDay2, 100),
                               SafeInt(weatherDataValid),
                               0,                                                // reserved — moved to CSV3 (SolarWatts)
                               0,                                                // reserved — moved to CSV3 (performanceRatio)
                               0,                                                // reserved — moved to CSV3 (VeData)
                               0,                                                // reserved — moved to CSV3 (NMEA0183Data)
                               0,                                                // reserved — moved to CSV3 (NMEA2KData)
                               SafeInt(alarmLatch ? 1 : 0),
                               SafeInt(ResetAlarmLatch),
                               0,  // CSV2_reserved_ResetLearningTable — action-only, no echo
                               0,  // CSV2_reserved_ClearOverheatHistory — action-only, no echo
                               SafeInt(DynamicShuntGainFactor, 1000),
                               SafeInt(DynamicAltCurrentZero, 1000),
                               SafeInt(InsulationLifePercent, 100),
                               SafeInt(GreaseLifePercent, 100),
                               SafeInt(BrushLifePercent, 100),
                               SafeInt(PredictedLifeHours),
                               SafeInt(LifeIndicatorColor),
                               SafeInt(pKwHrToday, 100),
                               SafeInt(pKwHrTomorrow, 100),
                               SafeInt(pKwHr2days, 100),
                               SafeInt(ambientTemp),
                               SafeInt(baroPressure, 10),  // mbar ×10 — whole-mbar rounding froze the display (~1 mbar/hr drift); JS divides by 10
                               SafeInt(firmwareVersionInt),
                               deviceIdUpper,                                    // 54 (%u)
                               deviceIdLower,                                    // 55 (%u)
                               SafeInt(ChargedEnergy_AllTime),
                               SafeInt(AlternatorFuelUsed_AllTime, 100),
                               SafeInt(PeakVoltage_AllTime, 100),
                               SafeInt(EngineRunTime_AllTime * 100 / 3600, 1),
                               SafeInt(MinVoltage, 100),
                               SafeInt(MinVoltage_AllTime, 100),
                               SafeInt(ChargeCycles, 100),
                               SafeInt(ChargeCycles_AllTime, 100),
                               SafeInt(EngineFuelUsed, 100),
                               SafeInt(EngineFuelUsed_AllTime, 100),
                               SafeInt(TotalDistance, 10),
                               SafeInt(TotalDistance_AllTime, 10),
                               SafeInt(MaxSpeed, 100),
                               SafeInt(MaxSpeed_AllTime, 100),
                               SafeInt(SolarChargedEnergy),
                               SafeInt(SolarChargedEnergy_AllTime),
                               SafeInt(AlternatorChargedEnergy_AllTime),
                               SafeInt(DischargedEnergy_AllTime),
                               SafeInt(AvgSOC_AllTime, 100),
                               SafeInt(AvgSpeed_AllTime, 100),
                               SafeInt(AvgSpeed, 100),
                               SafeInt(AlternatorOnTime_AllTime * 100 / 3600, 1),
                               SafeInt(EngineCycles_AllTime),
                               SafeInt(MaxAlternatorTemperatureF_AllTime),
                               SafeInt(MaxTemperatureThermistor_AllTime),
                               SafeInt(MeasuredAmpsMax_AllTime, 100),
                               SafeInt(RPMMax_AllTime),
                               0,  // reserved — Ignition moved to CSV1
                               SafeInt(inBulkStage ? 1 : 0),
                               // 85: seconds of WiFi remaining — WiFi wake OR shutdown drain window (shown as countdown banner)
                               SafeInt(
                                 (wifiWakeStart > 0 && (millis() - wifiWakeStart) < WIFI_WAKE_DURATION)
                                   ? (WIFI_WAKE_DURATION - (millis() - wifiWakeStart)) / 1000
                                   : (pendingShutdownFlush && shutdownCloudDeadlineMs > millis())
                                       ? (shutdownCloudDeadlineMs - millis()) / 1000
                                       : 0),
                               SafeInt(bufferedRecordCount),
                               SafeInt((bufferedRecordCount * 100) / SENSOR_RING_SIZE),
                               SafeInt(SENSOR_RING_SIZE),
                               SafeInt(VMGTargetBearing),
                               0,                                                // 98 reserved — moved to CSV3
                               SafeInt(cpuLoadCore0),
                               SafeInt(cpuLoadCore0Max),
                               SafeInt(cpuLoadCore1),
                               SafeInt(cpuLoadCore1Max),
                               SafeInt(hasForcedUpdate ? 1 : 0),
                               SafeInt(forcedFwVersionInt),
                               (forcedUpdateDeadline),
                               SafeInt(stateRevision),
                               0,                                                // 107 reserved — moved to CSV3
                               SafeInt(imu_accel_x_raw, 1000),
                               SafeInt(imu_accel_y_raw, 1000),
                               SafeInt(imu_accel_z_raw, 1000),
                               SafeInt(imu_gyro_x_raw, 100),
                               SafeInt(imu_gyro_y_raw, 100),
                               SafeInt(imu_gyro_z_raw, 100),
                               // Time-weighted averages below cast valid_us to int64 before dividing — else the unsigned valid_us promotes the signed area and corrupts negative values.
                               SafeInt(imuWindow->accel_x_min),
                               SafeInt(imuWindow->accel_x_max),
                               SafeInt(imuWindow->accel_x_valid_us > 0 ? (int)((int64_t)imuWindow->accel_x_area_v_us / (int64_t)imuWindow->accel_x_valid_us) : 0),
                               SafeInt(imuWindow->accel_y_min),
                               SafeInt(imuWindow->accel_y_max),
                               SafeInt(imuWindow->accel_y_valid_us > 0 ? (int)((int64_t)imuWindow->accel_y_area_v_us / (int64_t)imuWindow->accel_y_valid_us) : 0),
                               SafeInt(imuWindow->accel_z_min),
                               SafeInt(imuWindow->accel_z_max),
                               SafeInt(imuWindow->accel_z_valid_us > 0 ? (int)((int64_t)imuWindow->accel_z_area_v_us / (int64_t)imuWindow->accel_z_valid_us) : 0),
                               SafeInt(imuWindow->gyro_x_min),
                               SafeInt(imuWindow->gyro_x_max),
                               SafeInt(imuWindow->gyro_x_valid_us > 0 ? (int)((int64_t)imuWindow->gyro_x_area_v_us / (int64_t)imuWindow->gyro_x_valid_us) : 0),
                               SafeInt(imuWindow->gyro_y_min),
                               SafeInt(imuWindow->gyro_y_max),
                               SafeInt(imuWindow->gyro_y_valid_us > 0 ? (int)((int64_t)imuWindow->gyro_y_area_v_us / (int64_t)imuWindow->gyro_y_valid_us) : 0),
                               SafeInt(imuWindow->gyro_z_min),
                               SafeInt(imuWindow->gyro_z_max),
                               SafeInt(imuWindow->gyro_z_valid_us > 0 ? (int)((int64_t)imuWindow->gyro_z_area_v_us / (int64_t)imuWindow->gyro_z_valid_us) : 0),
                               SafeInt(imuWindow->heel_min),
                               SafeInt(imuWindow->heel_max),
                               SafeInt(imuWindow->heel_valid_us > 0 ? (int)((int64_t)imuWindow->heel_area_v_us / (int64_t)imuWindow->heel_valid_us) : 0),
                               SafeInt(imuWindow->pitch_min),
                               SafeInt(imuWindow->pitch_max),
                               SafeInt(imuWindow->pitch_valid_us > 0 ? (int)((int64_t)imuWindow->pitch_area_v_us / (int64_t)imuWindow->pitch_valid_us) : 0),
                               SafeInt(imuWindow->vertical_accel_min),
                               SafeInt(imuWindow->vertical_accel_max),
                               SafeInt(imuWindow->vertical_accel_valid_us > 0 ? (int)((int64_t)imuWindow->vertical_accel_area_v_us / (int64_t)imuWindow->vertical_accel_valid_us) : 0),
                               SafeInt(imuWindow->total_accel_min),
                               SafeInt(imuWindow->total_accel_max),
                               SafeInt(imuWindow->total_accel_valid_us > 0 ? (int)((int64_t)imuWindow->total_accel_area_v_us / (int64_t)imuWindow->total_accel_valid_us) : 0),
                               SafeInt(imuWindow->slam_count),
                               SafeInt(imuWindow->slam_peak_max),
                               SafeInt(imu_slam_count_lifetime),
                               SafeInt(imu_capsize_count),
                               SafeInt(imu_pitchpole_count),
                               SafeInt(imuWindow->heel_change_60s),
                               SafeInt(imuWindow->heel_deviation_60s),
                               SafeInt(imuWindow->pitch_change_60s),
                               SafeInt(imuWindow->pitch_deviation_60s),
                               SafeInt(imuWindow->wave_period),
                               SafeInt(imu_heel_max_lifetime, 100),
                               SafeInt(imu_pitch_max_lifetime, 100),
                               SafeInt(imu_slam_peak_lifetime, 1000),
                               SafeInt(imu_fifo_overrun_count),
                               SafeInt(imu_i2c_error_count),
                               SafeInt(imu_unknown_tag_count),
                               SafeInt(imuRingBuffer->accel_dropped),
                               SafeInt(imuRingBuffer->gyro_dropped),
                               SafeInt(imu_total_samples_accel),
                               SafeInt(imu_total_samples_gyro),
                               SafeInt(IMUReadTime2),
                               SafeInt(IMUReadTime),
                               SafeInt(adsI2CErrorCount),
                               SafeInt(tempPIDActive ? 1 : 0),
                               SafeInt(tempPIDInput_d, 100),
                               SafeInt(tempPIDSetpoint_d, 100),
                               SafeInt(thermalPenaltyAmps, 100),
                               SafeInt(innerTermP, 100),
                               SafeInt(innerTermI, 100),
                               SafeInt(innerTermD, 100),
                               SafeInt(outerTermP, 100),
                               SafeInt(outerTermI, 100),
                               SafeInt(outerTermLookahead, 100),
                               SafeInt(thermalSlopeFPerSec, 1000),
                               SafeInt(chargeStageDisplay),
                               SafeInt(voltageControlActive),
                               SafeInt((ChargingVoltageTarget - getBatteryVoltage()) * 100),
                               SafeInt(cv_I * 100),
                               SafeInt(inIdleStage),
                               altHaveFront(),                          // CSV2_altBaselineFrozen → have a usable best-ever front
                               SafeInt(ft_rai_total.worstWindow),
                               SafeInt(ft_rai_total.worstSession),
                               SafeInt(ft_rai_ina228.worstWindow),
                               SafeInt(ft_rai_ina228.worstSession),
                               SafeInt(ft_rai_ads_state.worstWindow),
                               SafeInt(ft_rai_ads_state.worstSession),
                               SafeInt(ft_rai_bmp_state.worstWindow),
                               SafeInt(ft_rai_bmp_state.worstSession),
                               SafeInt(ft_rai_imu.worstWindow),
                               SafeInt(ft_rai_imu.worstSession),
                               0,                                                 // CSV2_reserved_cv_D — dead slot (no D term)
                               SafeInt(tempReadFailCount),
                               SafeInt(tempCrcFailCount),
                               SafeInt(tempCrcRecoveredCount),
                               SafeInt(tempAllFFCount),
                               SafeInt(tempPowerOn85Count),
                               SafeInt(tempOutOfRangeCount),
                               SafeInt(tempRequestFailCount),
                               SafeInt(tempConnectedFailCount),
                               SafeInt(tempResolutionFixCount),
                               SafeInt(tempRereadFailCount),
                               SafeInt(tempResolutionFixCrcFailCount),
                               SafeInt(tempEnumerateFailCount),
                               SafeInt(warmupCeiling),
                               SafeInt(imu_min_moving_gentle),
                               SafeInt(imu_min_moving_moderate),
                               SafeInt(imu_min_moving_rough),
                               SafeInt(imu_min_moving_extreme),
                               SafeInt(imu_min_stat_gentle),
                               SafeInt(imu_min_stat_moderate),
                               SafeInt(imu_min_stat_rough),
                               SafeInt(imu_min_stat_extreme),
                               SafeInt(imu_heel_deviation_120s, 100),            // ×100, 2dp degrees
                               SafeInt(imu_pitch_deviation_120s, 100),           // ×100, 2dp degrees
                               SafeInt(imu_heading_swing_120s, 10),              // ×10, 1dp degrees; -10 = no compass data
                               SafeInt(g_dBcur_dt, 10),                          // ×10, 1dp A/s battery current rate of change
                               (int)g_loadDumpActive,                            // 1 if load dump feedforward is active
                               0,                                                // CSV2_reserved_thermalTestPhase (dead slot)
                               SafeInt(ft_updateAccelMetrics.worstWindow),
                               SafeInt(ft_updateAccelMetrics.worstSession),
                               // from CSV1
                               SafeInt(WifiStrength),
                               SafeInt(SendWifiTime),
                               SafeInt(AnalogReadTime),
                               SafeInt(VeTime),
                               SafeInt(MaximumLoopTime),
                               SafeInt(EngineCycles),
                               SafeInt(CurrentSessionDuration),
                               0,                                                // reserved — moved to CSV3 (timeAxisModeChanging)
                               SafeInt(currentPartitionType),
                               SafeInt(g_fastOvCurrentCap, 100),
                               SafeInt(g_fastOvClampCount),
                               SafeInt(g_fastOvHardCount),
                               SafeInt(ch1_last_ms),
                               SafeInt(ch1_avg_10s, 100),
                               SafeInt(ch1_worst_10s),
                               SafeInt(ch1_over2x_10s),
                               SafeInt(ch1_n_10s),
                               SafeInt(ch1_avg_2m, 100),
                               SafeInt(ch1_worst_2m),
                               SafeInt(ch1_over2x_2m),
                               SafeInt(ch1_n_2m),
                               SafeInt(ch1_avg_at, 100),
                               SafeInt(ch1_worst_at),
                               SafeInt(ch1_over2x_at),
                               SafeInt(ch1_n_at),
                               SafeInt(g_iExcessCount),
                               SafeInt(g_inaOVCount),
                               SafeInt(g_hardOCCount),
                               SafeInt(g_voltSpikeCount),
                               SafeInt(g_voltDisagreeCritCount),
                               SafeInt(g_voltDisagreeWarnCount),
                               SafeInt(g_voltImplausibleCount),
                               SafeInt(g_tempCritCount),
                               SafeInt(g_tempSustainedCount),
                               SafeInt(g_tempStaleCount),
                               SafeInt(g_currentStaleCount),
                               SafeInt(imu_msi_score, 100),
                               SafeInt(imu_vomit_pct, 100),
                               SafeInt(imu_anchorage_comfort, 100),
                               SafeInt(ina_last_ms),
                               SafeInt(ina_avg_10s, 100),
                               SafeInt(ina_worst_10s),
                               SafeInt(ina_over2x_10s),
                               SafeInt(ina_avg_2m, 100),
                               SafeInt(ina_worst_2m),
                               SafeInt(ina_over2x_2m),
                               SafeInt(ina_avg_at, 100),
                               SafeInt(ina_worst_at),
                               SafeInt(ina_over2x_at),
                               SafeInt(loopTime5sWindow / 1000),
                               SafeInt(MaximumLoopTime / 1000),
                               SafeInt(ft_SendWifiData.worstWindow),
                               SafeInt(ft_SendWifiData.worstSession),
                               SafeInt(ft_CheckAlarms.worstWindow),
                               SafeInt(ft_CheckAlarms.worstSession),
                               SafeInt(ft_calculateDerivedMetrics.worstWindow),
                               SafeInt(ft_calculateDerivedMetrics.worstSession),
                               SafeInt(ft_logDashboardValues.worstWindow),
                               SafeInt(ft_logDashboardValues.worstSession),
                               SafeInt(ft_updateSystemHealthStats.worstWindow),
                               SafeInt(ft_updateSystemHealthStats.worstSession),
                               SafeInt(ft_checkWiFiConnection.worstWindow),
                               SafeInt(ft_checkWiFiConnection.worstSession),
                               SafeInt(ft_ch1_compute_stats.worstWindow),
                               SafeInt(ft_ch1_compute_stats.worstSession),
                               SafeInt(ft_UpdateBatterySOC.worstWindow),
                               SafeInt(ft_UpdateBatterySOC.worstSession),
                               SafeInt(ft_updateSensorWindow.worstWindow),
                               SafeInt(ft_updateSensorWindow.worstSession),
                               SafeInt(ft_checkTimeSync.worstWindow),
                               SafeInt(ft_checkTimeSync.worstSession),
                               SafeInt(ft_zeroLogService.worstWindow),
                               SafeInt(ft_zeroLogService.worstSession),
                               SafeInt(ft_bhFlushCapNVS.worstWindow),
                               SafeInt(ft_bhFlushCapNVS.worstSession),
                               SafeInt(ft_kneeLearnService.worstWindow),
                               SafeInt(ft_kneeLearnService.worstSession),
                               // from CSV3 (firmware-computed)
                               SafeInt(currentRPMTableIndex),
                               SafeInt(pidInitialized ? 1 : 0),
                               SafeInt(pidSetpoint, 100),
                               SafeInt(TempToUse),
                               SafeInt(learningTargetFromRPM, 100),
                               SafeInt(ambientTempCorrection, 100),
                               SafeInt(finalLearningTarget, 100),
                               SafeInt(overheatingPenaltyTimer / 1000),
                               SafeInt(overheatingPenaltyAmps, 100),
                               0,  // CSV2_reserved_averageTableValue — dead slot, sends 0
                               SafeInt(timeSinceLastOverheat / 1000),
                               SafeInt(socInfoAvailable),
                               SafeInt(overheatCount[0]),
                               SafeInt(overheatCount[1]),
                               SafeInt(overheatCount[2]),
                               SafeInt(overheatCount[3]),
                               SafeInt(overheatCount[4]),
                               SafeInt(overheatCount[5]),
                               SafeInt(overheatCount[6]),
                               SafeInt(overheatCount[7]),
                               SafeInt(overheatCount[8]),
                               SafeInt(overheatCount[9]),
                               SafeInt(cumulativeNoOverheatTime[0] / 1000),
                               SafeInt(cumulativeNoOverheatTime[1] / 1000),
                               SafeInt(cumulativeNoOverheatTime[2] / 1000),
                               SafeInt(cumulativeNoOverheatTime[3] / 1000),
                               SafeInt(cumulativeNoOverheatTime[4] / 1000),
                               SafeInt(cumulativeNoOverheatTime[5] / 1000),
                               SafeInt(cumulativeNoOverheatTime[6] / 1000),
                               SafeInt(cumulativeNoOverheatTime[7] / 1000),
                               SafeInt(cumulativeNoOverheatTime[8] / 1000),
                               SafeInt(cumulativeNoOverheatTime[9] / 1000),
                               SafeInt(learningUpCount[0]),
                               SafeInt(learningUpCount[1]),
                               SafeInt(learningUpCount[2]),
                               SafeInt(learningUpCount[3]),
                               SafeInt(learningUpCount[4]),
                               SafeInt(learningUpCount[5]),
                               SafeInt(learningUpCount[6]),
                               SafeInt(learningUpCount[7]),
                               SafeInt(learningUpCount[8]),
                               SafeInt(learningUpCount[9]),
                               SafeInt(totalLearningEvents),
                               SafeInt(totalOverheats),
                               SafeInt(totalSafeHours),
                               SafeInt(FreeInternalRam),
                               SafeInt(TotalInternalRam),
                               SafeInt(LargestInternalBlock),
                               SafeInt(FreePSRAM),
                               SafeInt(TotalPSRAM),
                               SafeInt(Heapfrag),
                               SafeInt(ft_ReadAnalogInputs.worstWindow),
                               SafeInt(ft_ReadAnalogInputs.worstSession),
                               SafeInt(ft_AdjustFieldLearnMode.worstWindow),
                               SafeInt(ft_AdjustFieldLearnMode.worstSession),
                               SafeInt(ft_uploadSensorHistory.worstWindow),
                               SafeInt(ft_uploadSensorHistory.worstSession),
                               SafeInt(ft_uploadBufferedRecords.worstWindow),
                               SafeInt(ft_uploadBufferedRecords.worstSession),
                               SafeInt(ft_buildConfigPayload.worstWindow),
                               SafeInt(ft_buildConfigPayload.worstSession),
                               SafeInt(VeTime2),
                               (int)systemIDRiseDelay_ms[0],
                               (int)systemIDRiseDelay_ms[1],
                               (int)systemIDRiseDelay_ms[2],
                               (int)systemIDFallDelay_ms[0],
                               (int)systemIDFallDelay_ms[1],
                               (int)systemIDFallDelay_ms[2],
                               (int)systemIDRiseAvg_ms,
                               (int)systemIDFallAvg_ms,
                               SafeInt(ft_altHealth.worstWindow),
                               SafeInt(ft_altHealth.worstSession),
                               SafeInt(ft_altFold.worstWindow),
                               SafeInt(ft_altFold.worstSession),
                               SafeInt(ft_boatPerf.worstWindow),
                               SafeInt(ft_boatPerf.worstSession),
                               (int)systemIDActive,
                               (int)systemIDResultsReady,
                               (int)(systemIDStepAmp_A[0] * 10),
                               (int)(systemIDStepAmp_A[1] * 10),
                               (int)(systemIDStepAmp_A[2] * 10),
                               (int)(systemIDQuietPP_A[0] * 10),
                               (int)(systemIDQuietPP_A[1] * 10),
                               (int)(systemIDQuietPP_A[2] * 10),
                               (int)systemIDAbortReason,
                               (int)systemIDAbortPhase,
                               SafeInt(vl_last_ms),
                               SafeInt(vl_avg_10s, 100),
                               SafeInt(vl_worst_10s),
                               SafeInt(vl_over2x_10s),
                               SafeInt(vl_avg_2m, 100),
                               SafeInt(vl_worst_2m),
                               SafeInt(vl_over2x_2m),
                               SafeInt(vl_avg_at, 100),
                               SafeInt(vl_worst_at),
                               SafeInt(vl_over2x_at),
                               (int)((lastNVSSaveTime == 0) ? 0 : ((millis() - lastNVSSaveTime) / 1000UL)),
                               SafeInt(nvsFullSaveLastMs),
                               SafeInt(nvsFullSaveWorstMs),
                               SafeInt(nvsFullSaveCount),
                               // 28 ignition-cycle watermarks (lo, hi for 14 params). Scale must match enum comments.
                               SafeInt(wmIgnSafe(wmIgn_amps.lo), 1),     SafeInt(wmIgnSafe(wmIgn_amps.hi), 1),
                               SafeInt(wmIgnSafe(wmIgn_altTempF.lo), 1), SafeInt(wmIgnSafe(wmIgn_altTempF.hi), 1),
                               SafeInt(wmIgnSafe(wmIgn_IBV.lo), 10),     SafeInt(wmIgnSafe(wmIgn_IBV.hi), 10),
                               SafeInt(wmIgnSafe(wmIgn_Bcur.lo), 1),     SafeInt(wmIgnSafe(wmIgn_Bcur.hi), 1),
                               SafeInt(wmIgnSafe(wmIgn_SOC.lo), 1),      SafeInt(wmIgnSafe(wmIgn_SOC.hi), 1),
                               SafeInt(wmIgnSafe(wmIgn_RPM.lo), 1),      SafeInt(wmIgnSafe(wmIgn_RPM.hi), 1),
                               SafeInt(wmIgnSafe(wmIgn_SOG.lo), 1),      SafeInt(wmIgnSafe(wmIgn_SOG.hi), 1),
                               SafeInt(wmIgnSafe(wmIgn_AWS.lo), 1),      SafeInt(wmIgnSafe(wmIgn_AWS.hi), 1),
                               SafeInt(wmIgnSafe(wmIgn_TWS.lo), 1),      SafeInt(wmIgnSafe(wmIgn_TWS.hi), 1),
                               SafeInt(wmIgnSafe(wmIgn_heel.lo), 1),     SafeInt(wmIgnSafe(wmIgn_heel.hi), 1),
                               SafeInt(wmIgnSafe(wmIgn_pitch.lo), 1),    SafeInt(wmIgnSafe(wmIgn_pitch.hi), 1),
                               SafeInt(wmIgnSafe(wmIgn_vacc.lo), 10),    SafeInt(wmIgnSafe(wmIgn_vacc.hi), 10),
                               SafeInt(wmIgnSafe(wmIgn_baro.lo), 1),     SafeInt(wmIgnSafe(wmIgn_baro.hi), 1),
                               SafeInt(wmIgnSafe(wmIgn_ambient.lo), 1),  SafeInt(wmIgnSafe(wmIgn_ambient.hi), 1),
                               (int)restartRemainingSec,
                               (int)currentGpsSource,                    // 0=none 1=NMEA 2=Phone 3=Manual
                               (int)currentTimeSource,                   // 0=none 1=GPS 2=Phone 3=NTP 4=drifting
                               (int)loggingActive,                       // 1=logging, 0=stopped
                               SafeInt(sustainedTWS, 10),                // 2-min sustained TWS, knots ×10
                               SafeInt(currentGaleMinutes, 1),           // live gale minutes (int)
                               SafeInt(wmIgnSafe(wmIgn_VMGman.lo), 10),  SafeInt(wmIgnSafe(wmIgn_VMGman.hi), 10),
                               SafeInt(wmIgnSafe(wmIgn_VMGup.lo), 10),   SafeInt(wmIgnSafe(wmIgn_VMGup.hi), 10),
                               SafeInt(altWorstPct(), 10),              // CSV2_altHealthPct → worst-region perf% (v2)
                               altStatus(),                              // CSV2_altHealthStatus → 0 insufficient,1 healthy,2 drifting (v2)
                               SafeInt(altCoveragePct(), 10),            // CSV2_altCoveragePct → record-book fill% (v2)
                               altFrontCount(),                          // CSV2_altObsCount → front support-point count
                               0,  // reserved — imuHeelOffset moved to CSV3
                               0,  // reserved — imuPitchOffset moved to CSV3
                               // Victron VE.Direct solar/MPPT live block (7 fields — PPV/VPV/derived-current moved to CSV4)
                               SafeInt(VictronChargeState),              // CSV2_VictronChargeState
                               SafeInt(VictronMPPTMode),                 // CSV2_VictronMPPTMode
                               SafeInt(VictronError),                    // CSV2_VictronError
                               SafeInt(VictronYieldToday_kWh, 100),      // CSV2_VictronYieldToday
                               SafeInt(VictronMaxPowerToday_W),          // CSV2_VictronMaxPowerToday
                               SafeInt(VictronYieldYesterday_kWh, 100),  // CSV2_VictronYieldYesterday
                               SafeInt(VictronMaxPowerYesterday_W),      // CSV2_VictronMaxPowerYesterday
                               // (currentFuelGPH/currentNMPG moved to CSV4/NavStream)
                               // session fuel-economy curve (18 RPM bins, naut mi/gal ×100)
                               SafeInt(fuelCurveNMPG[0], 100),  SafeInt(fuelCurveNMPG[1], 100),  SafeInt(fuelCurveNMPG[2], 100),
                               SafeInt(fuelCurveNMPG[3], 100),  SafeInt(fuelCurveNMPG[4], 100),  SafeInt(fuelCurveNMPG[5], 100),
                               SafeInt(fuelCurveNMPG[6], 100),  SafeInt(fuelCurveNMPG[7], 100),  SafeInt(fuelCurveNMPG[8], 100),
                               SafeInt(fuelCurveNMPG[9], 100),  SafeInt(fuelCurveNMPG[10], 100), SafeInt(fuelCurveNMPG[11], 100),
                               SafeInt(fuelCurveNMPG[12], 100), SafeInt(fuelCurveNMPG[13], 100), SafeInt(fuelCurveNMPG[14], 100),
                               SafeInt(fuelCurveNMPG[15], 100), SafeInt(fuelCurveNMPG[16], 100), SafeInt(fuelCurveNMPG[17], 100),
                               SafeInt(currentFuelTopRPM),               // CSV2_fuelCurveTopRPM (top fuel-table RPM)
                               // 80MHz low-power loop instrumentation (4 fields) — µs→ms for the two worsts
                               SafeInt(loopWorst80Win / 1000),           // CSV2_loopWorst80Win_ms
                               SafeInt(loopWorst80Ses / 1000),           // CSV2_loopWorst80Ses_ms
                               SafeInt(loopOver80ImuLimitCount),         // CSV2_loopOver80ImuLimitCount
                               SafeInt(loop80IterCount),                 // CSV2_loop80IterCount
                               // field-ON loop instrumentation (2 fields) — µs→ms
                               SafeInt(loopFieldOnWin / 1000),           // CSV2_loopFieldOnWin_ms
                               SafeInt(loopFieldOnSes / 1000),           // CSV2_loopFieldOnSes_ms
                               // thermal tuning live-stream fields (see CSV2 enum) — flags byte mirrors the thermal log writer
                               SafeInt(tempFiltered, 100),               // CSV2_tempFiltered
                               SafeInt(outerImpliedPenalty, 100),        // CSV2_outerImpliedPenalty
                               SafeInt((tempPIDActive ? (1 << 0) : 0) | (sysMode == SYS_MODE_AUTO ? (1 << 4) : 0) | (shutdownPhase != SHUTDOWN_PHASE_NONE ? (1 << 5) : 0)),  // CSV2_thermalFlags
                               SafeInt(thermalAntiWindupLatch ? 1 : 0),  // CSV2_thermalAntiWindupLatch
                               // +10: inner-current-PID firing interval (field-on)
                               SafeInt(pf_last_ms),                      // CSV2_pf_last_ms
                               SafeInt(pf_avg_10s, 100),                 // CSV2_pf_avg_10s
                               SafeInt(pf_worst_10s),                    // CSV2_pf_worst_10s
                               SafeInt(pf_over2x_10s),                   // CSV2_pf_over2x_10s
                               SafeInt(pf_avg_2m, 100),                  // CSV2_pf_avg_2m
                               SafeInt(pf_worst_2m),                     // CSV2_pf_worst_2m
                               SafeInt(pf_over2x_2m),                    // CSV2_pf_over2x_2m
                               SafeInt(pf_avg_at, 100),                  // CSV2_pf_avg_at
                               SafeInt(pf_worst_at),                     // CSV2_pf_worst_at
                               SafeInt(pf_over2x_at),                    // CSV2_pf_over2x_at
                               SafeInt(inaBusReadWorstUs),               // CSV2_inaBusReadWorstUs
                               SafeInt(inaBusSlowCount),                 // CSV2_inaBusSlowCount
                               SafeInt(ina228ErrorCount),                // CSV2_ina228ErrorCount
                               SafeInt(imuFifoFetchWorstUs),             // CSV2_imuFifoFetchWorstUs
                               SafeInt(imuFifoWorstSamples),             // CSV2_imuFifoWorstSamples
                               SafeInt(ft_dumpLongTermRing.worstWindow),   // CSV2_dumpLongTermRing_win
                               SafeInt(ft_dumpLongTermRing.worstSession),  // CSV2_dumpLongTermRing_ses
                               SafeInt(ft_fastAltDrain.worstWindow),       // CSV2_fastAltDrain_win
                               SafeInt(ft_fastAltDrain.worstSession),      // CSV2_fastAltDrain_ses
                               SafeInt(ft_faMatrixFlush.worstWindow),      // CSV2_faMatrixFlush_win
                               SafeInt(ft_faMatrixFlush.worstSession),     // CSV2_faMatrixFlush_ses
                               SafeInt(faDetLastComputeUs),                // CSV2_faDetector_win  -> detector compute, LAST run (us)
                               SafeInt(faDetWorstComputeUs),               // CSV2_faDetector_ses  -> detector compute, WORST since reset (us)
                               SafeInt(ft_faWindowFinalize.worstWindow),   // CSV2_faWindowFinalize_win
                               SafeInt(ft_faWindowFinalize.worstSession),  // CSV2_faWindowFinalize_ses
                               SafeInt(faChanState),                       // CSV2_faChanState
                               SafeInt(faCellsUsed),                       // CSV2_faCellsUsed
                               SafeInt(faDetectLastK),                     // CSV2_faDetectK (fault class of last FAULT verdict)
                               SafeInt(faSesPkpkWorstA, 100),              // CSV2_faSesPkpkWorst
                               SafeInt(faSesPeakWorstA, 100),              // CSV2_faSesPeakWorst
                               SafeInt(faSesPeakWorstHz, 10),              // CSV2_faSesPeakWorstHz
                               SafeInt(faAnomalyCount),                    // CSV2_faAnomalyCount
                               SafeInt(faDomFreqHzX10),                    // CSV2_faDomFreqHz (already Hz×10; JS divides by 10)
                               SafeInt(faDomAmpAX100),                     // CSV2_faDomAmp (already A×100; JS divides by 100)
                               SafeInt(faDomRpm),                          // CSV2_faDomRpm (raw RPM)
                               SafeInt(faDomAmpsA, 10),                    // CSV2_faDomAmps (A ×10)
                               SafeInt(faDomTempF, 10),                    // CSV2_faDomTempF (°F ×10; NAN -> -1)
                               SafeInt((double)faDomEpoch),                // CSV2_faDomEpoch (epoch seconds; 0 = clock not synced)
                               SafeInt(faSesPkpkRpm),                      // CSV2_faSesPkpkRpm (raw RPM)
                               SafeInt(faSesPkpkAmpsA, 10),                // CSV2_faSesPkpkAmps (A ×10)
                               SafeInt(faSesPkpkTempF, 10),                // CSV2_faSesPkpkTempF (°F ×10; NAN -> -1)
                               SafeInt((double)faSesPkpkEpoch),            // CSV2_faSesPkpkEpoch (epoch seconds; 0 = clock not synced)
                               // +5: gate-tuning 10s live readouts (ROLL_EMPTY when no sample in window)
                               rollCsv(ROLL_RPMEDGE, 10),                  // CSV2_faRpmEdge10sMin  (RPM ×10, trough)
                               rollCsv(ROLL_AMPSDRIFT, 100),               // CSV2_faAmpsDrift10sMax (A ×100, peak)
                               rollCsv(ROLL_AMPSDRIFTEXC, 100),            // CSV2_faAmpsDriftExc10sMax (A ×100, peak; <=0 = passing)
                               rollCsv(ROLL_TONEPK, 100),                  // CSV2_faTonePk10sMax    (A ×100, peak)
                               rollCsv(ROLL_LDSLEW, 10),                   // CSV2_ldSlew10sMax      (A/s ×10, peak)
                               rollCsv(ROLL_CVSLOPE, 10000),               // CSV2_cvSlope10sMax     (V/s ×10000, peak)
                               rollCsv(ROLL_RIPCMDEXC, 100),               // CSV2_ripCmdExc10sMax   (A ×100, peak; <=0 = passing)
                               rollCsv(ROLL_RIPALTEXC, 100),               // CSV2_ripAltExc10sMax   (A ×100, peak; <=0 = passing)
                               rollCsv(ROLL_RIPBATTEXC, 100),              // CSV2_ripBattExc10sMax  (A ×100, peak; <=0 = passing)
                               rollCsv(ROLL_RIPRPMSHIFT, 10),              // CSV2_ripRpmShift10sMax (RPM ×10, peak; <=0 = passing)
                               (int)(g_ripAltAdmitCount & 0x7FFFFFFF),     // CSV2_ripAltAdmitCount  (lifetime admit counter)
                               (int)(g_ripBattAdmitCount & 0x7FFFFFFF),    // CSV2_ripBattAdmitCount (lifetime admit counter)
                               // Lifetime nav/sailing records (NVS-persisted; shown read-only in Lifetime Statistics)
                               SafeInt(LongestSingleTrip_Nm_AllTime, 10),  // CSV2_LongestTripAT     (nm ×10)
                               SafeInt(Max24hrDistance_AllTime, 10),       // CSV2_Max24hrDistAT     (nm ×10)
                               SafeInt(DeepestAnchorage_Ft_AllTime, 10),   // CSV2_DeepestAnchorAT   (ft ×10)
                               SafeInt(best_upwind_vmg_alltime, 100),      // CSV2_BestUpwindVmgAT   (kts ×100)
                               SafeInt(longest_gale_duration_hours_alltime, 100), // CSV2_LongestGaleAT (hr ×100)
                               // Control Accuracy Scores (since last reset). RMS error + worst overshoot per loop.
                               SafeInt(accScoreRms(accCurrent.errAccum, accCurrent.timeAccum), 100),     // CSV2_accCurRms   (A ×100)
                               SafeInt(accCurrent.worstOver, 100),        // CSV2_accCurPeak  (A ×100)
                               SafeInt(accScoreRms(accVoltage.errAccum, accVoltage.timeAccum), 1000),    // CSV2_accVoltRms  (V→mV)
                               SafeInt(accVoltage.worstOver, 1000),       // CSV2_accVoltPeak (V→mV)
                               SafeInt(accScoreRms(accThermal.errAccum, accThermal.timeAccum), 100),     // CSV2_accThermRms (°F ×100)
                               SafeInt(accThermal.worstOver, 100),        // CSV2_accThermPeak (°F ×100)
                               SafeInt(getCpuFrequencyMhz()),             // CSV2_cpuFreqMhz   (MHz ×1)
                               SafeInt(ch0GapLastMs),                     // CSV2_ch0GapLast   (ms)
                               SafeInt(ch0GapWorstMs),                    // CSV2_ch0GapWorst  (ms)
                               SafeInt(ch2GapLastMs),                     // CSV2_ch2GapLast   (ms)
                               SafeInt(ch2GapWorstMs),                    // CSV2_ch2GapWorst  (ms)
                               SafeInt(csv2BuildLastUs),                  // CSV2_csv2BuildLast  (µs)
                               SafeInt(csv2BuildWorstUs),                 // CSV2_csv2BuildWorst (µs)
                               SafeInt(csv2SendLastUs),                   // CSV2_csv2SendLast   (µs)
                               SafeInt(csv2SendWorstUs),                  // CSV2_csv2SendWorst  (µs)
                               SafeInt(httpsUploadLastMs),                // CSV2_httpsUpload_win -> Core-0 cloud op LAST (ms)
                               SafeInt(httpsUploadWorstMs),               // CSV2_httpsUpload_ses -> Core-0 cloud op WORST since reset (ms)
                               SafeInt(cvTempDerateScale, 1000)           // CSV2_cvTempDerateScale -> battery-temp gain derate multiplier (×1000)
    );
    csv2BuildLastUs = micros() - _csv2b0;   // CSV2 build (snprintf) cost
    if (csv2BuildLastUs > csv2BuildWorstUs) csv2BuildWorstUs = csv2BuildLastUs;
    // Clear the anti-windup latch now that this CSV2 frame has captured it (set in tempPID_tick on each CV-bleed event)
    thermalAntiWindupLatch = false;
    if (payload2Len < 0 || payload2Len >= (int)PAYLOAD2_SIZE) {
      Serial.printf("payload2 truncated or format error: %d\n", payload2Len);
      csv2Phase = 0;
      return;
    }
    csv2Phase = 1;          // built OK — send it on the next pass
    sentSomething = true;   // claim this pass (the heavy build ran)
    } else {
      // PASS 2 — send the payload built last pass (cheap ~1.3 ms; kept off the build pass).
      uint32_t _csv2s0 = micros();   // CSV2 send-cost timer (events.send → AsyncTCP)
      events.send(payload2, "CSVData2");
      csv2SendLastUs = micros() - _csv2s0;
      if (csv2SendLastUs > csv2SendWorstUs) csv2SendWorstUs = csv2SendLastUs;
      lastpayload2send = now;
      lastEventSourceSend = now;
      csv2Phase = 0;
      sentSomething = true;
    }
  }

  // PRIORITY 5: CSVData3 — sent immediately when settingsDirty (event-driven), or every 60s fallback
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
    /// ALL THIS SAFEINT STUFF MAY BE UNNECESSAREY BAD ADVICE, COULD HAVE JUST SENT ROUNDED FLOATS FOR 1 Byte (or bit?) xtra
    //WifiSendTime was 834uS before increasing csv3 payload size from 1100 to 1400     No change after.  Again, this separation into groups and worry about wifi packet size seems like AI nonsense.

    int payload3Len = snprintf(payload3, PAYLOAD3_SIZE,
                               "%d,"  // CSV3_FIELD_COUNT
                               "%d,%d,"  // 2 added: SetpointBigStepThresh, SetpointBigStepRiseRate
                               "%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,"
                               "%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,"
                               "%d,%d,%d,%d,%d,%d,%d,%d,"
                               "%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,"
                               "%d,%d,%d,%d,%d,%d,%d,%d,%d,"
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
                               "%d,%d,%d,%d,%d,%d,"
                               "%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,"
                               "%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,"
                               "%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,"
                               "%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,"
                               "%d,%d,%d,%d,%d,%d,%d,%d,%d,%.3f,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,"
                               "%d,%d,%d,%d,%d,%d,%d,%d,%d,"  // 9 fast-alt diagnostic knobs
                               "%d,%d,%d,"  // wifiNapEnabled + 2 imu zero offsets (moved from CSV2)
                               "%d,%d,%d,%d,"  // 4 systemID sine-sweep params
                               "%d,%d,%d,%d,%d,"  // 5 tuning sine params
                               "%d,"  // SystemIDStabilizeAmps
                               "%d,"  // tuningWaveFloor (Current Target Generator floor)
                               "%d,"  // commissionState
                               "%d,"  // commissionPhase
                               "%d,"  // commissionDoneMask
                               "%d,"  // cvHelpersEnabled
                               "%d,"  // MinChargeTempF
                               "%d,"  // coldChargeLockoutEnable
                               "%d,"  // cvGainMode
                               "%d,"  // CSV3_EXTRA1 (reserved placeholder)
                               "%d,"  // cvPlantK
                               "%d,"  // cvPlantTau
                               "%d,"  // cvPlantL
                               "%d,"  // cvComputedKp
                               "%d,"  // cvComputedKi
                               "%d,"  // cvCrossover
                               "%d,"  // cvPiZero
                               "%d,"  // vTgtRampUp
                               "%d,"  // vTgtRampDn
                               "%d,"  // vTgtRampEnable
                               "%d,"  // setpointSlewEnable
                               "%d,"  // cvRiseGovEnable
                               "%d,"  // dutySlewEnable
                               "%d,"  // CommissionTempF
                               "%d,"  // battTempDerateEnable
                               "%d,"  // battTempCoeff
                               "%d,"  // TempPIDKiDownFrac
                               "%d,"  // ThermalSlopeWindowSec
                               "%d,"  // BattCurrentLimitA
                               "%d,"  // ripWinMs
                               "%d,"  // ripDriftFloorA
                               "%d,"  // ripDriftPct
                               "%d",  // SocAlarmLow

                               CSV3_FIELD_COUNT,
                               SafeInt(TemperatureLimitF),
                               SafeInt(BulkVoltage, 100),
                               SafeInt(wavePeriod),
                               SafeInt(FloatVoltage, 100),
                               SafeInt(SwitchingFrequency),
                               SafeInt(yyMin),
                               SafeInt(FieldAdjustmentInterval),
                               SafeInt(ManualDutyTarget),
                               SafeInt(SwitchControlOverride),
                               SafeInt(waveAmplitude),
                               SafeInt(CurrentThreshold, 100),
                               SafeInt(PeukertExponent_scaled),
                               SafeInt(ChargeEfficiency_scaled),
                               SafeInt(ChargedVoltage_Scaled),
                               SafeInt(TailCurrent, 10),                         // 14  (× 10 so JS can show 1 decimal)
                               SafeInt(ChargedDetectionTime),
                               SafeInt(IgnoreTemperature),
                               SafeInt(bmsLogic),
                               SafeInt(bmsLogicLevelOff),
                               SafeInt(RPMScalingFactor),
                               SafeInt(MaximumAllowedBatteryAmps),
                               0,  // CSV3_reserved_BatteryVoltageSource — dead slot
                               SafeInt(AlternatorNominalAmps),
                               SafeInt(LearningUpStep, 100),
                               SafeInt(LearningDownStep, 100),
                               SafeInt(AmbientTempCorrectionFactor, 100),
                               SafeInt(xTime),
                               SafeInt(MinLearningInterval),
                               SafeInt(SafeOperationThreshold),
                               SafeInt(PidKp, 1000),
                               SafeInt(PidKi, 1000),
                               SafeInt(PidKd, 1000),
                               SafeInt(PidSampleDivisor),
                               SafeInt(MaxTableValue, 100),
                               SafeInt(MaxPenaltyPercent, 100),
                               SafeInt(MaxPenaltyDuration / 1000),
                               SafeInt(NeighborLearningFactor, 1000),
                               SafeInt(yyMax),
                               SafeInt(LearningMemoryDuration / 86400000),
                               SafeInt(EnableAmbientCorrection),
                               SafeInt(TuningMode),
                               0,  // CSV3_reserved_rpmCurrentTable_0..9 — dead slots, send 0
                               0,  // CSV3_reserved_rpmCurrentTable_1
                               0,  // CSV3_reserved_rpmCurrentTable_2
                               0,  // CSV3_reserved_rpmCurrentTable_3
                               0,  // CSV3_reserved_rpmCurrentTable_4
                               0,  // CSV3_reserved_rpmCurrentTable_5
                               0,  // CSV3_reserved_rpmCurrentTable_6
                               0,  // CSV3_reserved_rpmCurrentTable_7
                               0,  // CSV3_reserved_rpmCurrentTable_8
                               0,  // CSV3_reserved_rpmCurrentTable_9
                               SafeInt(ShuntResistanceMicroOhm),
                               SafeInt(InvertAltAmps),
                               SafeInt(InvertBattAmps),
                               SafeInt(MaxDuty),
                               SafeInt(MinDuty),
                               SafeInt(FieldResistance, 100),
                               SafeInt(maxPoints),
                               SafeInt(AlternatorCOffset, 100),
                               SafeInt(BatteryCOffset, 100),
                               SafeInt(BatteryCapacity_Ah),
                               SafeInt(AmpSensorRange),
                               SafeInt(R_fixed, 100),
                               SafeInt(Beta, 100),
                               SafeInt(T0_C, 100),
                               SafeInt(TempSource),
                               SafeInt(IgnitionOverride),
                               SafeInt(FLOAT_DURATION),
                               SafeInt(PulleyRatio, 100),
                               SafeInt(BatteryCurrentSource),
                               SafeInt(rpmTableRPMPoints[0]),
                               SafeInt(rpmTableRPMPoints[1]),
                               SafeInt(rpmTableRPMPoints[2]),
                               SafeInt(rpmTableRPMPoints[3]),
                               SafeInt(rpmTableRPMPoints[4]),
                               SafeInt(rpmTableRPMPoints[5]),
                               SafeInt(rpmTableRPMPoints[6]),
                               SafeInt(rpmTableRPMPoints[7]),
                               SafeInt(rpmTableRPMPoints[8]),
                               SafeInt(rpmTableRPMPoints[9]),
                               SafeInt(LearningSettlingPeriod),
                               SafeInt(LearningRPMChangeThreshold),
                               SafeInt(LearningTempHysteresis),
                               SafeInt(fuelTableRPM[0]),
                               SafeInt(fuelTableRPM[1]),
                               SafeInt(fuelTableRPM[2]),
                               SafeInt(fuelTableRPM[3]),
                               SafeInt(fuelTableRPM[4]),
                               SafeInt(fuelTableRPM[5]),
                               SafeInt(fuelTableRPM[6]),
                               SafeInt(fuelTableRPM[7]),
                               SafeInt(fuelTableRPM[8]),
                               SafeInt(fuelTableRPM[9]),
                               SafeInt(fuelTableGPH[0], 100),
                               SafeInt(fuelTableGPH[1], 100),
                               SafeInt(fuelTableGPH[2], 100),
                               SafeInt(fuelTableGPH[3], 100),
                               SafeInt(fuelTableGPH[4], 100),
                               SafeInt(fuelTableGPH[5], 100),
                               SafeInt(fuelTableGPH[6], 100),
                               SafeInt(fuelTableGPH[7], 100),
                               SafeInt(fuelTableGPH[8], 100),
                               SafeInt(fuelTableGPH[9], 100),
                               SafeInt(stateRevision),
                               0,  // CSV3_reserved_SetpointRampRate — dead slot
                               SafeInt(DutyRampRate, 100),
                               SafeInt(SettleTimeBeforeCut),
                               SafeInt(TempWarnExcess, 100),
                               SafeInt(TempCritExcess, 100),
                               SafeInt(TempSustainedTimeout / 1000),
                               SafeInt(AlternatorHardShutdownV, 100),
                               SafeInt(VoltageDisagreeThreshold, 100),
                               SafeInt(VoltageDisagreeTimeout / 1000),
                               SafeInt(rpmMinDutyTable[0], 100),
                               SafeInt(rpmMinDutyTable[1], 100),
                               SafeInt(rpmMinDutyTable[2], 100),
                               SafeInt(rpmMinDutyTable[3], 100),
                               SafeInt(rpmMinDutyTable[4], 100),
                               SafeInt(rpmMinDutyTable[5], 100),
                               SafeInt(rpmMinDutyTable[6], 100),
                               SafeInt(rpmMinDutyTable[7], 100),
                               SafeInt(rpmMinDutyTable[8], 100),
                               SafeInt(rpmMinDutyTable[9], 100),
                               SafeInt(rpmCapCurrentTable[0], 100),
                               SafeInt(rpmCapCurrentTable[1], 100),
                               SafeInt(rpmCapCurrentTable[2], 100),
                               SafeInt(rpmCapCurrentTable[3], 100),
                               SafeInt(rpmCapCurrentTable[4], 100),
                               SafeInt(rpmCapCurrentTable[5], 100),
                               SafeInt(rpmCapCurrentTable[6], 100),
                               SafeInt(rpmCapCurrentTable[7], 100),
                               SafeInt(rpmCapCurrentTable[8], 100),
                               SafeInt(rpmCapCurrentTable[9], 100),
                               SafeInt(VoltageKp, 100),
                               SafeInt(VoltageLoopInterval),
                               SafeInt(FIELD_COLLAPSE_DELAY),
                               SafeInt(SetpointRiseRate, 100),
                               SafeInt(SetpointFallRate, 100),
                               SafeInt(SetpointBigStepThresh, 100),
                               SafeInt(SetpointBigStepRiseRate, 100),
                               SafeInt(PIDTrackingGain, 100),
                               SafeInt(CAPSIZE_THRESHOLD_DEG),
                               SafeInt(PITCHPOLE_THRESHOLD_DEG),
                               SafeInt(SLAM_THRESHOLD_G, 10),
                               SafeInt(imuMountOrientation),
                               SafeInt(TailCurrent_A, 100),
                               SafeInt(RebulkVoltage, 100),
                               SafeInt(rebulkDebounceTime),
                               SafeInt(MinFloatTime),
                               SafeInt(SOC_BlockRebulk_percent),
                               SafeInt(SOC_AllowRebulk_percent),
                               0,                                                 // CSV3_reserved_accelEnabled — dead slot (accelerometer always-on)
                               SafeInt(DutySlowRampRate, 100),
                               SafeInt(ShutdownPhase2HoldMs),
                               SafeInt(TempPIDKp, 1000),
                               SafeInt(TempPIDKi, 1000),
                               SafeInt(ThermalLookaheadSec),
                               SafeInt(TempPIDIntervalMs),
                               SafeInt(TempPIDFilterAlpha, 1000),
                               SafeInt(VoltageKi, 100),
                               (int)rpmCapPowerTable[0],
                               (int)rpmCapPowerTable[1],
                               (int)rpmCapPowerTable[2],
                               (int)rpmCapPowerTable[3],
                               (int)rpmCapPowerTable[4],
                               (int)rpmCapPowerTable[5],
                               (int)rpmCapPowerTable[6],
                               (int)rpmCapPowerTable[7],
                               (int)rpmCapPowerTable[8],
                               (int)rpmCapPowerTable[9],
                               0,  // CSV3_reserved_VoltageTrimLimit — dead slot
                               (int)InputFilterTC,
                               SafeInt(SystemIDStepAmplitude, 10),               // ×10, 1 decimal
                               SafeInt(HardOCTripAmps, 10),                      // ×10, 1 decimal
                               SafeInt(HardOCDebounceMs),                        // raw ms
                               SafeInt(IExcessFrac, 1000),                       // CSV3_IExcessFrac — ×1000, 3 decimals
                               SafeInt(IExcessFloorA, 10),                       // CSV3_IExcessFloorA — ×10, 1 decimal
                               SafeInt(IExcessKBleed, 100),                      // ×100, 2 decimals
                               SafeInt(IgnoreRPM),
                               SafeInt(MinRPMForField),
                               SafeInt(AwBleedRate, 10),                         // ×10, 1 decimal
                               0,                                                 // CSV3_reserved_AwRecoverRate — rate hardcoded to 0.1 in firmware
                               SafeInt(KHard, 10),                               // ×10, 1 decimal
                               SafeInt(ReseedFrac, 100),                         // ×100, 2 decimal (shared recovery seed fraction)
                               (int)AwSeedProtectMs,
                               0,                                                 // CSV3_reserved_VoltageKd — dead slot (no D term)
                               SafeInt(displayTempUnit),
                               SafeInt(WarmupRampRate, 10),                      // ×10, 1 decimal
                               (int)OvGroup1Enable,
                               (int)OvGroup2Enable,
                               SafeInt(IExcessCeilA, 10),                        // CSV3_IExcessCeilA — ×10, 1 decimal
                               SafeInt(IExcessTau),                              // CSV3_IExcessTau — raw int ms
                               OutputPIDSigSrc,
                               TdPred,                                           // 196 (%.3f)
                               OvMeasMarginV,                                    // 197 (%.3f)
                               OvPredMarginV,                                    // 198 (%.3f)
                               OutputPIDMA_N,
                               (int)OutputPIDFilterTC,
                               (int)VoltageFilterTC,
                               0,                                                // CSV3_reserved_ProtectionProxGateV — dead slot
                               SafeInt(SlopeBleedThresh, 100),
                               (int)SlopeBleedK,
                               SafeInt(DvdtTC, 10),                              // ×10, 1 decimal (ms)
                               SafeInt(SlopeBleedProxV, 100),                    // ×100, 2 decimals
                               SafeInt(StartupRiseRate, 100),                    // ×100, 2 decimals
                               // from CSV2 (settings)
                               SafeInt(absorptionCompleteTime),
                               SafeInt(OnOff),
                               SafeInt(ManualFieldToggle),
                               SafeInt(HiLow),
                               SafeInt(LimpHome),
                               SafeInt(AlarmActivate),
                               SafeInt(TempAlarm),
                               SafeInt(VoltageAlarmHigh, 100),  // V ×100, 2 decimals
                               SafeInt(VoltageAlarmLow, 100),   // V ×100, 2 decimals
                               SafeInt(CurrentAlarmHigh),
                               SafeInt(AlarmTest),
                               SafeInt(AlarmLatchEnabled),
                               SafeInt(MaintainMode),
                               SafeInt(ManualSOCPoint, 100),
                               SafeInt(IgnoreLearningDuringPenalty),
                               SafeInt(LogAllLearningEvents),
                               SafeInt(CloudFeatures),
                               SafeInt(AutoShuntGainCorrection),
                               SafeInt(AutoAltCurrentZero),
                               SafeInt(WindingTempOffset),
                               SafeInt(ManualLifePercentage),
                               SafeInt(UVThresholdHigh, 100),
                               SafeInt(weatherModeEnabled),
                               0,                                                 // CSV3_reserved_SENSOR_UPLOAD_INTERVAL — firmware-only constant
                               SafeInt(imuEnabled ? 1 : 0),
                               SafeInt(AbsorptionVoltage * 100),
                               SafeInt(AbsorptionTimeoutMs),
                               SafeInt(bulkVoltageHoldMs),
                               SafeInt(capLimitMode),
                               SafeInt(TargetVoltageMode),
                               SafeInt(TargetVoltageSetpoint, 100),
                               SafeInt(RebulkCurrent_A, 100),
                               SafeInt(UseFloat),
                               SafeInt(IExcessFracBulk, 1000),  // CSV3_IExcessFracBulk (×1000, 3 decimals)
                               SafeInt(IExcessRelFrac, 1000),   // CSV3_IExcessRelFrac (×1000, 3 decimals)
                               SafeInt(systemIDPlantTauMs),   // CSV3_systemIDPlantTauMs — fitted plant tau (ms), persisted
                               SafeInt(0),   // CSV3_altSpare3 (reserved)
                               SafeInt(TempAlarmLow),
                               SafeInt(LoadDumpDtThresh),                        // A/s tier-2 threshold (2 consecutive)
                               SafeInt(LoadDumpDtThresh1),                       // A/s tier-1 threshold (1 sample)
                               (int)CVTuningMode,
                               SafeInt(cvWaveAmplitudeV, 100),                   // ×100, 2dp V
                               (int)cvWavePeriodSec,
                               SafeInt(cvKOvershoot, 10),                        // ×10, 1dp
                               (int)cvConsecutiveReads,
                               // 8 dead slots (CSV3_reserved_thermal0..7), all send 0
                               0, 0, 0, 0, 0, 0, 0, 0,
                               // from CSV1 (settings)
                               SafeInt(webgaugesinterval),
                               SafeInt(plotTimeWindow),
                               SafeInt(Ymin1),
                               SafeInt(Ymax1),
                               SafeInt(Ymin2, 100),
                               SafeInt(Ymax2, 100),
                               SafeInt(Ymin3),
                               SafeInt(Ymax3),
                               SafeInt(Ymin4),
                               SafeInt(Ymax4),
                               SafeInt(LoadDumpDtThresh3),                       // A/s tier-3 threshold (3 consecutive)
                               SafeInt(0),                                      // CSV3_reserved_VMGUseTrueWind — dead slot
                               SafeInt(hardwarePresent),                         // moved from CSV2
                               (int)testProtectionsEnabled,                     // 0/1 — runtime flag, not persisted
                               IExcessArmMarginV,                               // %.3f — iExcess voltage gate margin
                               SafeInt(FastSetpointRiseRate, 100),              // ×100, 1 decimal — post-protection rise-slew multiplier
                               (int)FastSetpointRiseWindowMs,                   // raw ms
                               SafeInt(FastSetpointRiseHeadroomV, 100),         // ×100, 2 decimal — V headroom gate
                               SafeInt(SolarWatts),                             // moved from CSV2
                               SafeInt(performanceRatio, 100),                  // moved from CSV2 (×100, 2 decimal)
                               SafeInt(VeData),                                 // moved from CSV2
                               SafeInt(NMEA0183Data),                           // moved from CSV2
                               SafeInt(NMEA2KData),                             // moved from CSV2
                               SafeInt(timeAxisModeChanging),                   // moved from CSV2
                               (int)gpsTimeSourceMode,                          // 0=auto,1=NMEA,2=Phone,3=NTP
                               // Fast alt-current diagnostic knobs (Pattern B echo)
                               (int)faEnabled,                                  // 0/1
                               (int)faAlarmEnable,                              // 0/1
                               (int)faAnomPause,                                // 0/1
                               SafeInt(faRpmEdgeMargin, 10),                    // RPM ×10
                               SafeInt(faAmpsDriftFloorA, 100),                 // A ×100
                               SafeInt(faAmpsDriftPct, 10),                     // percent ×10
                               SafeInt(faAttenUpAmps, 10),                      // A ×10
                               SafeInt(faAttenDownAmps, 10),                    // A ×10
                               SafeInt(faPeakMinA, 100),                        // A ×100
                               (int)wifiNapEnabled,                             // 0/1
                               SafeInt(imuHeelOffsetDeg, 100),                  // moved from CSV2
                               SafeInt(imuPitchOffsetDeg, 100),
                               SafeInt(systemIDTestType),                       // 0=step, 1=sine
                               SafeInt(systemIDSineFreqStart, 10),              // Hz ×10
                               SafeInt(systemIDSineFreqEnd, 10),                // Hz ×10
                               SafeInt(systemIDSineCycles),
                               SafeInt(tuningWaveform),                         // 0=square,1=sine,2=sweep
                               SafeInt(tuningSineFreq, 10),                     // Hz ×10
                               SafeInt(tuningSweepStart, 10),                   // Hz ×10
                               SafeInt(tuningSweepEnd, 10),                     // Hz ×10
                               SafeInt(tuningSweepCycles),
                               SafeInt(SystemIDStabilizeAmps, 10),              // A ×10
                               SafeInt(tuningWaveFloor),                        // A (raw)
                               (int)commissionState,                            // CSV3_commissionState (0/1/2)
                               (int)commissionPhase,                            // CSV3_commissionPhase (0..8)
                               (int)commissionDoneMask,                         // CSV3_commissionDoneMask (bit i = stage i done)
                               (int)cvHelpersEnabled,                           // CSV3_cvHelpersEnabled (asymmetric unwind + slope bleed master switch)
                               SafeInt(MinChargeTempF),                         // CSV3_MinChargeTempF (cold-charge lockout board-temp floor, °F)
                               (int)coldChargeLockoutEnable,                    // CSV3_coldChargeLockoutEnable (cold-charge lockout master on/off)
                               (int)cvGainMode,                                 // CSV3_cvGainMode (0=Manual, 1=Auto λ-based)
                               0,                                              // CSV3_EXTRA1 (reserved placeholder)
                               SafeInt(cvPlantK, 10000),                        // CSV3_cvPlantK (measured plant gain V/A, ×10000)
                               SafeInt(cvPlantTau, 100),                        // CSV3_cvPlantTau (measured rise time s, ×100)
                               SafeInt(cvPlantL, 100),                          // CSV3_cvPlantL (measured dead time s, ×100)
                               SafeInt(cvComputedKp, 100),                      // CSV3_cvComputedKp (Auto-computed Kp 12V-equiv, ×100)
                               SafeInt(cvComputedKi, 100),                      // CSV3_cvComputedKi (Auto-computed Ki 12V-equiv, ×100)
                               SafeInt(cvCrossover, 100),                       // CSV3_cvCrossover (crossover ω_c rad/s, ×100)
                               SafeInt(cvPiZero, 100),                          // CSV3_cvPiZero (PI integral zero ρ rad/s, ×100)
                               SafeInt(vTgtRampUp, 1000),                       // CSV3_vTgtRampUp (target ramp up rate V/s, ×1000)
                               SafeInt(vTgtRampDn, 1000),                       // CSV3_vTgtRampDn (target ramp down rate V/s, ×1000)
                               (int)vTgtRampEnable,                             // CSV3_vTgtRampEnable (slew master switch 0/1)
                               (int)setpointSlewEnable,                         // CSV3_setpointSlewEnable (current setpoint slew master switch 0/1)
                               (int)cvRiseGovEnable,                            // CSV3_cvRiseGovEnable (CV rise governor master switch 0/1)
                               (int)dutySlewEnable,                             // CSV3_dutySlewEnable (field duty slew master switch 0/1)
                               isnan(CommissionTempF) ? -32768 : (int)lroundf(CommissionTempF * 10.0f),  // CSV3_CommissionTempF (°F ×10; -32768 = unset)
                               (int)battTempDerateEnable,                       // CSV3_battTempDerateEnable (0/1)
                               SafeInt(battTempCoeff, 10000),                   // CSV3_battTempCoeff (fractional R change per °C, ×10000)
                               SafeInt(TempPIDKiDownFrac, 1000),                // CSV3_TempPIDKiDownFrac (below-setpoint bleed ratio ×Ki, ×1000)
                               SafeInt(ThermalSlopeWindowSec),                  // CSV3_ThermalSlopeWindowSec (slope difference window, s)
                               SafeInt(BattCurrentLimitA, 10),                  // CSV3_BattCurrentLimitA — ×10, 1 decimal; 0 = disabled
                               SafeInt(ripWinMs),                               // CSV3_ripWinMs (ms, integer)
                               SafeInt(ripDriftFloorA, 100),                    // CSV3_ripDriftFloorA (A ×100)
                               SafeInt(ripDriftPct, 10),                        // CSV3_ripDriftPct (% ×10)
                               SafeInt(SocAlarmLow)                             // CSV3_SocAlarmLow (%, integer; 0 = disabled)
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

  // PRIORITY 6: TimestampData (staleness data - every 3 seconds)
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
                                       "%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu",
                                       (unsigned long)TS_FIELD_COUNT,
                                       (dataTimestamps[IDX_HEADING_NMEA] == 0) ? 999999 : (now - dataTimestamps[IDX_HEADING_NMEA]),
                                       (dataTimestamps[IDX_LATITUDE_NMEA] == 0) ? 999999 : (now - dataTimestamps[IDX_LATITUDE_NMEA]),
                                       (dataTimestamps[IDX_LONGITUDE_NMEA] == 0) ? 999999 : (now - dataTimestamps[IDX_LONGITUDE_NMEA]),
                                       (dataTimestamps[IDX_SATELLITE_COUNT] == 0) ? 999999 : (now - dataTimestamps[IDX_SATELLITE_COUNT]),
                                       (dataTimestamps[IDX_VICTRON_VOLTAGE] == 0) ? 999999 : (now - dataTimestamps[IDX_VICTRON_VOLTAGE]),
                                       (dataTimestamps[IDX_VICTRON_CURRENT] == 0) ? 999999 : (now - dataTimestamps[IDX_VICTRON_CURRENT]),
                                       (dataTimestamps[IDX_ALTERNATOR_TEMP] == 0) ? 999999 : (now - dataTimestamps[IDX_ALTERNATOR_TEMP]),
                                       (dataTimestamps[IDX_THERMISTOR_TEMP] == 0) ? 999999 : (now - dataTimestamps[IDX_THERMISTOR_TEMP]),
                                       (dataTimestamps[IDX_RPM] == 0) ? 999999 : (now - dataTimestamps[IDX_RPM]),
                                       (dataTimestamps[IDX_MEASURED_AMPS] == 0) ? 999999 : (now - dataTimestamps[IDX_MEASURED_AMPS]),
                                       (dataTimestamps[IDX_BATTERY_V] == 0) ? 999999 : (now - dataTimestamps[IDX_BATTERY_V]),
                                       (dataTimestamps[IDX_IBV] == 0) ? 999999 : (now - dataTimestamps[IDX_IBV]),
                                       (dataTimestamps[IDX_BCUR] == 0) ? 999999 : (now - dataTimestamps[IDX_BCUR]),
                                       (dataTimestamps[IDX_CHANNEL3V] == 0) ? 999999 : (now - dataTimestamps[IDX_CHANNEL3V]),
                                       (dataTimestamps[IDX_DUTY_CYCLE] == 0) ? 999999 : (now - dataTimestamps[IDX_DUTY_CYCLE]),
                                       (dataTimestamps[IDX_FIELD_VOLTS] == 0) ? 999999 : (now - dataTimestamps[IDX_FIELD_VOLTS]),
                                       (dataTimestamps[IDX_FIELD_AMPS] == 0) ? 999999 : (now - dataTimestamps[IDX_FIELD_AMPS]),
                                       (dataTimestamps[IDX_COG_NMEA] == 0) ? 999999 : (now - dataTimestamps[IDX_COG_NMEA]),
                                       (dataTimestamps[IDX_SOG_NMEA] == 0) ? 999999 : (now - dataTimestamps[IDX_SOG_NMEA]),
                                       (dataTimestamps[IDX_APPARENT_WIND_SPEED] == 0) ? 999999 : (now - dataTimestamps[IDX_APPARENT_WIND_SPEED]),
                                       (dataTimestamps[IDX_APPARENT_WIND_ANGLE] == 0) ? 999999 : (now - dataTimestamps[IDX_APPARENT_WIND_ANGLE]),
                                       (dataTimestamps[IDX_TRUE_WIND_SPEED] == 0) ? 999999 : (now - dataTimestamps[IDX_TRUE_WIND_SPEED]),
                                       (dataTimestamps[IDX_TRUE_WIND_ANGLE] == 0) ? 999999 : (now - dataTimestamps[IDX_TRUE_WIND_ANGLE]),
                                       (dataTimestamps[IDX_LEEWAY] == 0) ? 999999 : (now - dataTimestamps[IDX_LEEWAY]),
                                       (dataTimestamps[IDX_VMG] == 0) ? 999999 : (now - dataTimestamps[IDX_VMG]),
                                       (dataTimestamps[IDX_BARO_PRESSURE] == 0) ? 999999 : (now - dataTimestamps[IDX_BARO_PRESSURE]),
                                       (dataTimestamps[IDX_AMBIENT_TEMP] == 0) ? 999999 : (now - dataTimestamps[IDX_AMBIENT_TEMP]),
                                       (dataTimestamps[IDX_IMU] == 0) ? 999999 : (now - dataTimestamps[IDX_IMU]),
                                       (dataTimestamps[IDX_VICTRON_SOLAR] == 0) ? 999999 : (now - dataTimestamps[IDX_VICTRON_SOLAR]),
                                       (dataTimestamps[IDX_STW_NMEA] == 0) ? 999999 : (now - dataTimestamps[IDX_STW_NMEA])
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

  if (isRegistered) {
    Serial.println("Auth token loaded (registered)");
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
  doc["boat_displacement_lbs"]  = BOAT_DISPLACEMENT_LBS;
  doc["boat_type"]              = BOAT_TYPE;
  doc["boat_make_model"]        = BOAT_MAKE_MODEL;
  doc["boat_year"]              = BOAT_YEAR;
  doc["home_port"]              = HOME_PORT;
  doc["engine_make"]            = ENGINE_MAKE;
  doc["engine_hp"]              = ENGINE_HP;
  doc["battery_voltage"]        = BATTERY_VOLTAGE;
  doc["battery_capacity_ah"]    = BatteryCapacity_Ah;
  doc["battery_type"]           = BATTERY_TYPE;
  doc["battery_make_model"]     = BATTERY_MAKE_MODEL;
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

  // Mirror duplicated fields into their standalone settings keys so the next
  // boot's standalone-load doesn't overwrite the vesselData values in RAM.
  // The reverse direction (standalone /get handlers → vessel_info.json) is
  // handled by updateVesselInfoField().
  settingWrite(NK_BatteryCapacity_Ah, String(BatteryCapacity_Ah).c_str());
  settingWrite(NK_SolarWatts,         String(SolarWatts).c_str());
}

