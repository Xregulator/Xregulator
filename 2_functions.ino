/**
 * AI_SUMMARY: Important functions for AI to parse to help solve the problem. There are more files called 3_nonImportantFunctions and 4_optional that can be shared if necessary.
 * AI_PURPOSE: 
 * AI_INPUTS: 
 * AI_OUTPUTS: 
 * AI_DEPENDENCIES: 
 * AI_RISKS: 
 * CRITICAL_INSTRUCTION_FOR_AI:: When adding new code, try to first use or modify existing code whenever possible, to avoid bloat. When impossible, always mimick my style and coding patterns. If you have a performance improvement idea, tell me. When giving me new code, I prefer complete copy and paste functions when they are short, or for you to give step by step instructions for me to edit if function is long, to conserve tokens. Always specify which option you chose.
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

String readFile(fs::FS &fs, const char *path) {
  if (!littleFSMounted && !ensureLittleFS()) {
    Serial.printf("Cannot read file - LittleFS not available: %s\n", path);
    return String();
  }

  if (!fsMutex) {
    Serial.printf("readFile: fsMutex NULL: %s\n", path);
    return String();
  }

  if (xSemaphoreTake(fsMutex, pdMS_TO_TICKS(5000)) != pdTRUE) {
    Serial.printf("readFile: mutex timeout: %s\n", path);
    return String();
  }

  File file = fs.open(path, "r");
  if (!file || file.isDirectory()) {
    xSemaphoreGive(fsMutex);
    Serial.printf("readFile: open failed or is directory: %s\n", path);
    return String();
  }

  size_t size = file.size();
  if (size > 4096) {  // pick your limit
    file.close();
    xSemaphoreGive(fsMutex);
    Serial.printf("readFile: file too large (%u): %s\n", (unsigned)size, path);
    return String();  // More importantly, callers can't distinguish "file too large" from "file not found" — both return String(). The config payload buffer is 8192 bytes, so it's possible valid config JSON could exceed 4096 bytes. Consider bumping the limit or returning a distinct error indicator.
  }

  char *buffer = (char *)ps_malloc(size + 1);
  if (!buffer) {
    file.close();
    xSemaphoreGive(fsMutex);
    Serial.println("readFile: allocation failed");
    return String();
  }

  size_t bytesRead = file.readBytes(buffer, size);
  buffer[bytesRead] = '\0';

  file.close();
  xSemaphoreGive(fsMutex);

  String result(buffer);
  free(buffer);
  return result;
}

// Settings-write wrapper: skip the LittleFS erase/rewrite when the file already holds this exact
// content (saves a flash cycle on no-op form re-submits). Fail-safe — any read mismatch just writes,
// so it can never skip a needed write. writeFile uses file.print(message) and readFile reads it back
// byte-for-byte, so an unchanged value round-trips exactly.
bool writeFileIfChanged(fs::FS &fs, const char *path, const char *message) {
  if (fsExists(path) && readFile(fs, path) == message) return true;   // unchanged → no write
  return writeFile(fs, path, message);
}

bool writeFile(fs::FS &fs, const char *path, const char *message) {
  if (!littleFSMounted && !ensureLittleFS()) {
    return false;
  }

  if (!fsMutex || xSemaphoreTake(fsMutex, pdMS_TO_TICKS(5000)) != pdTRUE) {
    Serial.printf("writeFile: mutex timeout: %s\n", path);
    return false;
  }

  File file = fs.open(path, "w");
  if (!file) {
    xSemaphoreGive(fsMutex);
    return false;
  }

  const size_t expected = strlen(message);
  const size_t written  = file.print(message);
  file.flush();
  file.close();

  // Truncated write — LittleFS full or other I/O failure. Delete the partial
  // file so the next boot reads "no setting" (and Pattern B re-initializes the
  // default) instead of "" → toFloat()→0.0 → silent zeroing of a critical setting.
  if (written != expected) {
    Serial.printf("writeFile: truncated write on %s (%u/%u bytes) — deleting\n",
                  path, (unsigned)written, (unsigned)expected);
    fs.remove(path);
    xSemaphoreGive(fsMutex);
    return false;
  }

  xSemaphoreGive(fsMutex);
  return true;
}
bool fsExists(const char *path) {
  if (!fsMutex || xSemaphoreTake(fsMutex, pdMS_TO_TICKS(5000)) != pdTRUE) {
    Serial.println("fsExists: mutex timeout");
    return false;
  }
  bool result = LittleFS.exists(path);
  xSemaphoreGive(fsMutex);
  return result;
}

bool fsRemove(const char *path) {
  if (!fsMutex || xSemaphoreTake(fsMutex, pdMS_TO_TICKS(5000)) != pdTRUE) {
    Serial.println("fsRemove: mutex timeout");
    return false;
  }
  bool result = LittleFS.remove(path);
  xSemaphoreGive(fsMutex);
  return result;
}

// ==== NVS settings layer ====
// User settings live in the NVS "settings" namespace as strings, NOT in
// LittleFS files. The userdata LittleFS partition is high-churn (logs, rings,
// history) and mounts with formatOnFail — a corruption there must not wipe
// user settings. String storage keeps exact parity with the old file contents
// (same parse paths, e.g. ChargeEfficiency's user-readable "99.0" form).
// NVS keys cap at 15 chars, so long setting names map to short keys via the
// NK_* macros below (generated; doc: Working Markdown Docs/NVS_SETTINGS_KEY_MAP.md).
// The alt*/perf* registry tables in 7_functions.ino use their entry names
// truncated to 15 chars as keys directly — keep new registry names unique
// in their first 15 chars.
#define NK_AbsorptionTimeoutMs "AbsorptionTmtMs"
#define NK_AbsorptionVoltage "AbsorptionVoltg"
#define NK_AlarmActivate "AlarmActivate"
#define NK_AlarmLatchEnabled "AlarmLatchEnbld"
#define NK_AlternatorCOffset "AlternatrCOffst"
#define NK_AlternatorHardShutdownV "AltrntrHrdShtdw"
#define NK_AlternatorNominalAmps "AltrntrNmnlAmps"
#define NK_AmbientTempCorrectionFactor "AmbntTmpCrrctnF"
#define NK_AmpSensorRange "AmpSensorRange"
#define NK_AutoAltCurrentZero "AutoAltCurrntZr"
#define NK_AutoShuntGainCorrection "AutShntGnCrrctn"
#define NK_AwBleedRate "AwBleedRate"
#define NK_AwSeedProtectMs "AwSeedProtectMs"
#define NK_BatteryCOffset "BatteryCOffset"
#define NK_BatteryCapacity_Ah "BatteryCapctyAh"
#define NK_BatteryCurrentSource "BatteryCrrntSrc"
#define NK_Beta "Beta"
#define NK_BulkVoltage "BulkVoltage"
#define NK_CAPSIZE_THRESHOLD_DEG "CAPSIZETHRESHOL"
#define NK_CVTuningMode "CVTuningMode"
#define NK_ChargeEfficiency "ChargeEfficincy"
#define NK_ChargedDetectionTime "ChargedDetctnTm"
#define NK_ChargedVoltage "ChargedVoltage"
#define NK_CloudFeatures "CloudFeatures"
#define NK_CurrentAlarmHigh "CurrentAlarmHgh"
#define NK_CurrentThreshold "CurrentThreshld"
#define NK_DutyRampRate "DutyRampRate"
#define NK_DutySlowRampRate "DutySlowRampRat"
#define NK_DvdtAlpha "DvdtAlpha"
#define NK_DvdtTC "DvdtTC"
#define NK_EnableAmbientCorrection "EnblAmbntCrrctn"
#define NK_FIELD_COLLAPSE_DELAY "FIELDCOLLAPSEDE"
#define NK_FLOAT_DURATION "FLOAT_DURATION"
#define NK_FastSetpointRiseHeadroomV "FstStpntRsHdrmV"
#define NK_FastSetpointRiseRate "FastSetpontRsRt"
#define NK_FastSetpointRiseWindowMs "FstStpntRsWndwM"
#define NK_FieldAdjustmentInterval "FldAdjstmntIntr"
#define NK_FieldResistance "FieldResistance"
#define NK_FloatVoltage "FloatVoltage"
#define NK_FuelEfficiency "FuelEfficiency"
#define NK_HardOCDebounceMs "HardOCDebouncMs"
#define NK_HiLow "HiLow"
#define NK_IExcessArmMarginV "IExcessArmMrgnV"
#define NK_IExcessK "IExcessK"
#define NK_IExcessKBleed "IExcessKBleed"
#define NK_IExcessMA_N "IExcessMA_N"
#define NK_IExcessN "IExcessN"
#define NK_IExcessReseedFrac "IExcessResedFrc"
#define NK_IExcessSigSrc "IExcessSigSrc"
#define NK_IgnitionOverride "IgnitionOverrid"
#define NK_IgnoreLearningDuringPenalty "IgnrLrnngDrngPn"
#define NK_IgnoreRPM "IgnoreRPM"
#define NK_IgnoreTemperature "IgnoreTemperatr"
#define NK_InputFilterTC "InputFilterTC"
#define NK_InvertAltAmps "InvertAltAmps"
#define NK_InvertBattAmps "InvertBattAmps"
#define NK_KHard "KHard"
#define NK_LastResetReason "LastResetReason"
#define NK_LatitudeManual "LatitudeManual"
#define NK_LatitudeNMEA "LatitudeNMEA"
#define NK_LearningDownStep "LearningDownStp"
#define NK_LearningMemoryDuration "LearnngMmryDrtn"
#define NK_LearningRPMChangeThreshold "LrnngRPMChngThr"
#define NK_LearningSettlingPeriod "LernngSttlngPrd"
#define NK_LearningTempHysteresis "LrnngTmpHystrss"
#define NK_LearningUpStep "LearningUpStep"
#define NK_LimpHome "LimpHome"
#define NK_LoadDumpDtThresh "LoadDumpDtThrsh"
#define NK_LoadDumpDtThresh1 "LoadDmpDtThrsh1"
#define NK_LoadDumpDtThresh3 "LoadDmpDtThrsh3"
#define NK_LogAllLearningEvents "LgAllLrnngEvnts"
#define NK_LongitudeManual "LongitudeManual"
#define NK_LongitudeNMEA "LongitudeNMEA"
#define NK_MaintainMode "MaintainMode"
#define NK_ManualDutyTarget "ManualDutyTargt"
#define NK_ManualFieldToggle "ManualFieldTggl"
#define NK_ManualLifePercentage "ManualLifPrcntg"
#define NK_ManualSOCPoint "ManualSOCPoint"
#define NK_MaxDuty "MaxDuty"
#define NK_MaxPenaltyDuration "MaxPenaltyDurtn"
#define NK_MaxPenaltyPercent "MaxPenaltyPrcnt"
#define NK_MaxTableValue "MaxTableValue"
#define NK_MaximumAllowedBatteryAmps "MxmmAllwdBttryA"
#define NK_MinDuty "MinDuty"
#define NK_MinFloatTime "MinFloatTime"
#define NK_MinLearningInterval "MinLernngIntrvl"
#define NK_MinRPMForField "MinRPMForField"
#define NK_NMEA0183Data "NMEA0183Data"
#define NK_NMEA2KData "NMEA2KData"
#define NK_NeighborLearningFactor "NeghbrLrnngFctr"
#define NK_OnOff "OnOff"
#define NK_OutputPIDFilterTC "OutputPIDFltrTC"
#define NK_OutputPIDMA_N "OutputPIDMA_N"
#define NK_OutputPIDSigSrc "OutputPIDSigSrc"
#define NK_OvGroup1Enable "OvGroup1Enable"
#define NK_OvGroup2Enable "OvGroup2Enable"
#define NK_OvLayer2Enable "OvLayer2Enable"
#define NK_OvLayer3Enable "OvLayer3Enable"
#define NK_OvMeasMarginV "OvMeasMarginV"
#define NK_OvPredMarginV "OvPredMarginV"
#define NK_PIDTrackingGain "PIDTrackingGain"
#define NK_PITCHPOLE_THRESHOLD_DEG "PITCHPOLETHRESH"
#define NK_PeukertExponent "PeukertExponent"
#define NK_PidKd "PidKd"
#define NK_PidKi "PidKi"
#define NK_PidKp "PidKp"
#define NK_PidSampleDivisor "PidSampleDivisr"
#define NK_PulleyRatio "PulleyRatio"
#define NK_RPMScalingFactor "RPMScalingFactr"
#define NK_R_fixed "R_fixed"
#define NK_RebulkCurrent_A "RebulkCurrent_A"
#define NK_RebulkVoltage "RebulkVoltage"
#define NK_ReseedFrac "ReseedFrac"
#define NK_SLAM_THRESHOLD_G "SLAMTHRESHOLDG"
#define NK_SOC_AllowRebulk_percent "SOCAllwRblkprcn"
#define NK_SOC_BlockRebulk_percent "SOCBlckRblkprcn"
#define NK_SafeOperationThreshold "SafOprtnThrshld"
#define NK_SetpointFallRate "SetpointFallRat"
#define NK_SetpointRiseRate "SetpointRiseRat"
#define NK_SettleTimeBeforeCut "SettleTimeBfrCt"
#define NK_ShuntResistanceMicroOhm "ShntRsstncMcrOh"
#define NK_ShutdownPhase2HoldMs "ShtdwnPhs2HldMs"
#define NK_SlopeBleedK "SlopeBleedK"
#define NK_SlopeBleedProxV "SlopeBleedProxV"
#define NK_SlopeBleedThresh "SlopeBleedThrsh"
#define NK_SolarWatts "SolarWatts"
#define NK_StartupRiseRate "StartupRiseRate"
#define NK_SwitchControlOverride "SwtchCntrlOvrrd"
#define NK_SwitchingFrequency "SwitchingFrqncy"
#define NK_SystemIDStepAmplitude "SystmIDStpAmplt"
#define NK_T0_C "T0_C"
#define NK_TailCurrent "TailCurrent"
#define NK_TailCurrent_A "TailCurrent_A"
#define NK_TargetVoltageMode "TargetVoltageMd"
#define NK_TargetVoltageSetpoint "TargetVltgStpnt"
#define NK_TdPred "TdPred"
#define NK_TempAlarm "TempAlarm"
#define NK_TempAlarmLow "TempAlarmLow"
#define NK_TempCritExcess "TempCritExcess"
#define NK_TempPIDFilterAlpha "TempPIDFltrAlph"
#define NK_TempPIDIntervalMs "TempPIDIntrvlMs"
#define NK_TempPIDKi "TempPIDKi"
#define NK_TempPIDKp "TempPIDKp"
#define NK_TempSource "TempSource"
#define NK_TempSustainedTimeout "TempSustaindTmt"
#define NK_TempWarnExcess "TempWarnExcess"
#define NK_TemperatureLimitF "TemperatureLmtF"
#define NK_ThermalLookaheadSec "ThermalLookhdSc"
#define NK_ThermalTuningMode "ThermalTuningMd"
#define NK_TuningMode "TuningMode"
#define NK_UVThresholdHigh "UVThresholdHigh"
#define NK_UseFloat "UseFloat"
#define NK_VHardMarginV "VHardMarginV"
#define NK_VSoftMarginV "VSoftMarginV"
#define NK_VeData "VeData"
#define NK_VoltageAlarmHigh "VoltageAlarmHgh"
#define NK_VoltageAlarmLow "VoltageAlarmLow"
#define NK_VoltageDisagreeThreshold "VltgDsgrThrshld"
#define NK_VoltageDisagreeTimeout "VoltageDisgrTmt"
#define NK_VoltageFilterTC "VoltageFilterTC"
#define NK_VoltageKi "VoltageKi"
#define NK_VoltageKp "VoltageKp"
#define NK_VoltageLoopInterval "VoltageLpIntrvl"
#define NK_VoltageSpikeMargin "VoltageSpikMrgn"
#define NK_WarmupRampRate "WarmupRampRate"
#define NK_WeatherTimeoutMs "WeatherTimeotMs"
#define NK_WeatherUpdateInterval "WethrUpdtIntrvl"
#define NK_WindingTempOffset "WindingTmpOffst"
#define NK_Ymax1 "Ymax1"
#define NK_Ymax2 "Ymax2"
#define NK_Ymax3 "Ymax3"
#define NK_Ymax4 "Ymax4"
#define NK_Ymin1 "Ymin1"
#define NK_Ymin2 "Ymin2"
#define NK_Ymin3 "Ymin3"
#define NK_Ymin4 "Ymin4"
#define NK_absorptionCompleteTime "absorptnCmpltTm"
#define NK_altPaused "altPaused"
#define NK_altbaseSec "altbaseSec"
#define NK_bmsLogic "bmsLogic"
#define NK_bmsLogicLevelOff "bmsLogicLevlOff"
#define NK_bulkVoltageHoldMs "bulkVoltagHldMs"
#define NK_capLimitMode "capLimitMode"
#define NK_cvConsecutiveReads "cvConsecutivRds"
#define NK_cvKOvershoot "cvKOvershoot"
#define NK_cvWaveAmplitudeV "cvWaveAmplitudV"
#define NK_cvWavePeriodSec "cvWavePeriodSec"
#define NK_displayTempUnit "displayTempUnit"
#define NK_gpsManualActive "gpsManualActive"
#define NK_gpsTimeSourceMode "gpsTimeSourceMd"
#define NK_hardwarePresent "hardwarePresent"
#define NK_maxPoints "maxPoints"
#define NK_perfPaused "perfPaused"
#define NK_performanceRatio "performanceRati"
#define NK_plotTimeWindow "plotTimeWindow"
#define NK_rebulkDebounceTime "rebulkDebouncTm"
#define NK_socInfoAvailable "socInfoAvailabl"
#define NK_thermalConsecutiveReads "thermlCnsctvRds"
#define NK_thermalKOvershoot "thermalKOversht"
#define NK_thermalKUndershoot "thermalKUndrsht"
#define NK_thermalSettleThreshF "thrmlSttlThrshF"
#define NK_thermalWaveHalfPeriodMin "thrmlWvHlfPrdMn"
#define NK_thermalWaveHighF "thermalWaveHghF"
#define NK_thermalWaveLowF "thermalWaveLowF"
#define NK_timeAxisModeChanging "timeAxsMdChngng"
#define NK_totalPowerCycles "totalPowerCycls"
#define NK_waveAmplitude "waveAmplitude"
#define NK_wavePeriod "wavePeriod"
#define NK_weatherDataValid "weatherDataVald"
#define NK_weatherModeEnabled "weatherModEnbld"
#define NK_webgaugesinterval "webgaugesintrvl"
#define NK_xTime "xTime"
#define NK_yyMax "yyMax"
#define NK_yyMin "yyMin"
// WiFi provisioning + interface password + IMU level calibration (migrated 2nd pass)
#define NK_ssid "ssid"
#define NK_pass "pass"
#define NK_apssid "apssid"
#define NK_appass "appass"
#define NK_first_config_done "firstconfigdone"
#define NK_password "password"
#define NK_passwordHash "passwordHash"
#define NK_imu_zero "imu_zero"

#define SETTINGS_NVS_NAMESPACE "settings"

bool settingExists(const char *key) {
  nvs_handle_t h;
  if (nvs_open(SETTINGS_NVS_NAMESPACE, NVS_READONLY, &h) != ESP_OK) return false;
  size_t len = 0;
  bool found = (nvs_get_str(h, key, NULL, &len) == ESP_OK);
  nvs_close(h);
  return found;
}

String settingRead(const char *key) {
  nvs_handle_t h;
  if (nvs_open(SETTINGS_NVS_NAMESPACE, NVS_READONLY, &h) != ESP_OK) return String();
  String out;
  size_t len = 0;
  if (nvs_get_str(h, key, NULL, &len) == ESP_OK && len > 0 && len < 4096) {
    char *buf = (char *)malloc(len);
    if (buf) {
      if (nvs_get_str(h, key, buf, &len) == ESP_OK) out = String(buf);
      free(buf);
    }
  }
  nvs_close(h);
  return out;
}

bool settingWrite(const char *key, const char *value) {
  if (!key || !value) return false;
  nvs_handle_t h;
  if (nvs_open(SETTINGS_NVS_NAMESPACE, NVS_READWRITE, &h) != ESP_OK) return false;
  // compare-first: unchanged value -> no flash write (wear + entry churn)
  char cur[128];
  size_t len = sizeof(cur);
  if (nvs_get_str(h, key, cur, &len) == ESP_OK && strcmp(cur, value) == 0) {
    nvs_close(h);
    return true;
  }
  esp_err_t e = nvs_set_str(h, key, value);
  if (e == ESP_OK) e = nvs_commit(h);
  nvs_close(h);
  return e == ESP_OK;
}

bool settingRemove(const char *key) {
  nvs_handle_t h;
  if (nvs_open(SETTINGS_NVS_NAMESPACE, NVS_READWRITE, &h) != ESP_OK) return false;
  esp_err_t e = nvs_erase_key(h, key);
  if (e == ESP_OK) e = nvs_commit(h);
  nvs_close(h);
  return e == ESP_OK || e == ESP_ERR_NVS_NOT_FOUND;  // already-absent counts as removed
}

// One-time sweep at boot: move any pre-NVS settings file into NVS, then delete
// the file. No-op once the filesystem is clean; a virgin device finds nothing
// and falls through to hardcoded defaults in InitSystemSettings.
struct LegacySettingFile { const char *file; const char *key; };
static const LegacySettingFile LEGACY_SETTINGS[] = {
  { "/AbsorptionTimeoutMs.txt", NK_AbsorptionTimeoutMs },
  { "/AbsorptionVoltage.txt", NK_AbsorptionVoltage },
  { "/AlarmActivate.txt", NK_AlarmActivate },
  { "/AlarmLatchEnabled.txt", NK_AlarmLatchEnabled },
  { "/AlternatorCOffset.txt", NK_AlternatorCOffset },
  { "/AlternatorHardShutdownV.txt", NK_AlternatorHardShutdownV },
  { "/AlternatorNominalAmps.txt", NK_AlternatorNominalAmps },
  { "/AmbientTempCorrectionFactor.txt", NK_AmbientTempCorrectionFactor },
  { "/AmpSensorRange.txt", NK_AmpSensorRange },
  { "/AutoAltCurrentZero.txt", NK_AutoAltCurrentZero },
  { "/AutoShuntGainCorrection.txt", NK_AutoShuntGainCorrection },
  { "/AwBleedRate.txt", NK_AwBleedRate },
  { "/AwSeedProtectMs.txt", NK_AwSeedProtectMs },
  { "/BatteryCOffset.txt", NK_BatteryCOffset },
  { "/BatteryCapacity_Ah.txt", NK_BatteryCapacity_Ah },
  { "/BatteryCurrentSource.txt", NK_BatteryCurrentSource },
  { "/Beta.txt", NK_Beta },
  { "/BulkVoltage.txt", NK_BulkVoltage },
  { "/CAPSIZE_THRESHOLD_DEG.txt", NK_CAPSIZE_THRESHOLD_DEG },
  { "/CVTuningMode.txt", NK_CVTuningMode },
  { "/ChargeEfficiency.txt", NK_ChargeEfficiency },
  { "/ChargedDetectionTime.txt", NK_ChargedDetectionTime },
  { "/ChargedVoltage.txt", NK_ChargedVoltage },
  { "/CloudFeatures.txt", NK_CloudFeatures },
  { "/CurrentAlarmHigh.txt", NK_CurrentAlarmHigh },
  { "/CurrentThreshold.txt", NK_CurrentThreshold },
  { "/DutyRampRate.txt", NK_DutyRampRate },
  { "/DutySlowRampRate.txt", NK_DutySlowRampRate },
  { "/DvdtAlpha.txt", NK_DvdtAlpha },
  { "/DvdtTC.txt", NK_DvdtTC },
  { "/EnableAmbientCorrection.txt", NK_EnableAmbientCorrection },
  { "/FIELD_COLLAPSE_DELAY.txt", NK_FIELD_COLLAPSE_DELAY },
  { "/FLOAT_DURATION.txt", NK_FLOAT_DURATION },
  { "/FastSetpointRiseHeadroomV.txt", NK_FastSetpointRiseHeadroomV },
  { "/FastSetpointRiseRate.txt", NK_FastSetpointRiseRate },
  { "/FastSetpointRiseWindowMs.txt", NK_FastSetpointRiseWindowMs },
  { "/FieldAdjustmentInterval.txt", NK_FieldAdjustmentInterval },
  { "/FieldResistance.txt", NK_FieldResistance },
  { "/FloatVoltage.txt", NK_FloatVoltage },
  { "/FuelEfficiency.txt", NK_FuelEfficiency },
  { "/HardOCDebounceMs.txt", NK_HardOCDebounceMs },
  { "/HiLow.txt", NK_HiLow },
  { "/IExcessArmMarginV.txt", NK_IExcessArmMarginV },
  { "/IExcessK.txt", NK_IExcessK },
  { "/IExcessKBleed.txt", NK_IExcessKBleed },
  { "/IExcessMA_N.txt", NK_IExcessMA_N },
  { "/IExcessN.txt", NK_IExcessN },
  { "/IExcessReseedFrac.txt", NK_IExcessReseedFrac },
  { "/IExcessSigSrc.txt", NK_IExcessSigSrc },
  { "/IgnitionOverride.txt", NK_IgnitionOverride },
  { "/IgnoreLearningDuringPenalty.txt", NK_IgnoreLearningDuringPenalty },
  { "/IgnoreRPM.txt", NK_IgnoreRPM },
  { "/IgnoreTemperature.txt", NK_IgnoreTemperature },
  { "/InputFilterTC.txt", NK_InputFilterTC },
  { "/InvertAltAmps.txt", NK_InvertAltAmps },
  { "/InvertBattAmps.txt", NK_InvertBattAmps },
  { "/KHard.txt", NK_KHard },
  { "/LastResetReason.txt", NK_LastResetReason },
  { "/LatitudeManual.txt", NK_LatitudeManual },
  { "/LatitudeNMEA.txt", NK_LatitudeNMEA },
  { "/LearningDownStep.txt", NK_LearningDownStep },
  { "/LearningMemoryDuration.txt", NK_LearningMemoryDuration },
  { "/LearningRPMChangeThreshold.txt", NK_LearningRPMChangeThreshold },
  { "/LearningSettlingPeriod.txt", NK_LearningSettlingPeriod },
  { "/LearningTempHysteresis.txt", NK_LearningTempHysteresis },
  { "/LearningUpStep.txt", NK_LearningUpStep },
  { "/LimpHome.txt", NK_LimpHome },
  { "/LoadDumpDtThresh.txt", NK_LoadDumpDtThresh },
  { "/LoadDumpDtThresh1.txt", NK_LoadDumpDtThresh1 },
  { "/LoadDumpDtThresh3.txt", NK_LoadDumpDtThresh3 },
  { "/LogAllLearningEvents.txt", NK_LogAllLearningEvents },
  { "/LongitudeManual.txt", NK_LongitudeManual },
  { "/LongitudeNMEA.txt", NK_LongitudeNMEA },
  { "/MaintainMode.txt", NK_MaintainMode },
  { "/ManualDutyTarget.txt", NK_ManualDutyTarget },
  { "/ManualFieldToggle.txt", NK_ManualFieldToggle },
  { "/ManualLifePercentage.txt", NK_ManualLifePercentage },
  { "/ManualSOCPoint.txt", NK_ManualSOCPoint },
  { "/MaxDuty.txt", NK_MaxDuty },
  { "/MaxPenaltyDuration.txt", NK_MaxPenaltyDuration },
  { "/MaxPenaltyPercent.txt", NK_MaxPenaltyPercent },
  { "/MaxTableValue.txt", NK_MaxTableValue },
  { "/MaximumAllowedBatteryAmps.txt", NK_MaximumAllowedBatteryAmps },
  { "/MinDuty.txt", NK_MinDuty },
  { "/MinFloatTime.txt", NK_MinFloatTime },
  { "/MinLearningInterval.txt", NK_MinLearningInterval },
  { "/MinRPMForField.txt", NK_MinRPMForField },
  { "/NMEA0183Data.txt", NK_NMEA0183Data },
  { "/NMEA2KData.txt", NK_NMEA2KData },
  { "/NeighborLearningFactor.txt", NK_NeighborLearningFactor },
  { "/OnOff.txt", NK_OnOff },
  { "/OutputPIDFilterTC.txt", NK_OutputPIDFilterTC },
  { "/OutputPIDMA_N.txt", NK_OutputPIDMA_N },
  { "/OutputPIDSigSrc.txt", NK_OutputPIDSigSrc },
  { "/OvGroup1Enable.txt", NK_OvGroup1Enable },
  { "/OvGroup2Enable.txt", NK_OvGroup2Enable },
  { "/OvLayer2Enable.txt", NK_OvLayer2Enable },
  { "/OvLayer3Enable.txt", NK_OvLayer3Enable },
  { "/OvMeasMarginV.txt", NK_OvMeasMarginV },
  { "/OvPredMarginV.txt", NK_OvPredMarginV },
  { "/PIDTrackingGain.txt", NK_PIDTrackingGain },
  { "/PITCHPOLE_THRESHOLD_DEG.txt", NK_PITCHPOLE_THRESHOLD_DEG },
  { "/PeukertExponent.txt", NK_PeukertExponent },
  { "/PidKd.txt", NK_PidKd },
  { "/PidKi.txt", NK_PidKi },
  { "/PidKp.txt", NK_PidKp },
  { "/PidSampleDivisor.txt", NK_PidSampleDivisor },
  { "/PulleyRatio.txt", NK_PulleyRatio },
  { "/RPMScalingFactor.txt", NK_RPMScalingFactor },
  { "/R_fixed.txt", NK_R_fixed },
  { "/RebulkCurrent_A.txt", NK_RebulkCurrent_A },
  { "/RebulkVoltage.txt", NK_RebulkVoltage },
  { "/ReseedFrac.txt", NK_ReseedFrac },
  { "/SLAM_THRESHOLD_G.txt", NK_SLAM_THRESHOLD_G },
  { "/SOC_AllowRebulk_percent.txt", NK_SOC_AllowRebulk_percent },
  { "/SOC_BlockRebulk_percent.txt", NK_SOC_BlockRebulk_percent },
  { "/SafeOperationThreshold.txt", NK_SafeOperationThreshold },
  { "/SetpointFallRate.txt", NK_SetpointFallRate },
  { "/SetpointRiseRate.txt", NK_SetpointRiseRate },
  { "/SettleTimeBeforeCut.txt", NK_SettleTimeBeforeCut },
  { "/ShuntResistanceMicroOhm.txt", NK_ShuntResistanceMicroOhm },
  { "/ShutdownPhase2HoldMs.txt", NK_ShutdownPhase2HoldMs },
  { "/SlopeBleedK.txt", NK_SlopeBleedK },
  { "/SlopeBleedProxV.txt", NK_SlopeBleedProxV },
  { "/SlopeBleedThresh.txt", NK_SlopeBleedThresh },
  { "/SolarWatts.txt", NK_SolarWatts },
  { "/StartupRiseRate.txt", NK_StartupRiseRate },
  { "/SwitchControlOverride.txt", NK_SwitchControlOverride },
  { "/SwitchingFrequency.txt", NK_SwitchingFrequency },
  { "/SystemIDStepAmplitude.txt", NK_SystemIDStepAmplitude },
  { "/T0_C.txt", NK_T0_C },
  { "/TailCurrent.txt", NK_TailCurrent },
  { "/TailCurrent_A.txt", NK_TailCurrent_A },
  { "/TargetVoltageMode.txt", NK_TargetVoltageMode },
  { "/TargetVoltageSetpoint.txt", NK_TargetVoltageSetpoint },
  { "/TdPred.txt", NK_TdPred },
  { "/TempAlarm.txt", NK_TempAlarm },
  { "/TempAlarmLow.txt", NK_TempAlarmLow },
  { "/TempCritExcess.txt", NK_TempCritExcess },
  { "/TempPIDFilterAlpha.txt", NK_TempPIDFilterAlpha },
  { "/TempPIDIntervalMs.txt", NK_TempPIDIntervalMs },
  { "/TempPIDKi.txt", NK_TempPIDKi },
  { "/TempPIDKp.txt", NK_TempPIDKp },
  { "/TempSource.txt", NK_TempSource },
  { "/TempSustainedTimeout.txt", NK_TempSustainedTimeout },
  { "/TempWarnExcess.txt", NK_TempWarnExcess },
  { "/TemperatureLimitF.txt", NK_TemperatureLimitF },
  { "/ThermalLookaheadSec.txt", NK_ThermalLookaheadSec },
  { "/ThermalTuningMode.txt", NK_ThermalTuningMode },
  { "/TuningMode.txt", NK_TuningMode },
  { "/UVThresholdHigh.txt", NK_UVThresholdHigh },
  { "/UseFloat.txt", NK_UseFloat },
  { "/VHardMarginV.txt", NK_VHardMarginV },
  { "/VSoftMarginV.txt", NK_VSoftMarginV },
  { "/VeData.txt", NK_VeData },
  { "/VoltageAlarmHigh.txt", NK_VoltageAlarmHigh },
  { "/VoltageAlarmLow.txt", NK_VoltageAlarmLow },
  { "/VoltageDisagreeThreshold.txt", NK_VoltageDisagreeThreshold },
  { "/VoltageDisagreeTimeout.txt", NK_VoltageDisagreeTimeout },
  { "/VoltageFilterTC.txt", NK_VoltageFilterTC },
  { "/VoltageKi.txt", NK_VoltageKi },
  { "/VoltageKp.txt", NK_VoltageKp },
  { "/VoltageLoopInterval.txt", NK_VoltageLoopInterval },
  { "/VoltageSpikeMargin.txt", NK_VoltageSpikeMargin },
  { "/WarmupRampRate.txt", NK_WarmupRampRate },
  { "/WeatherTimeoutMs.txt", NK_WeatherTimeoutMs },
  { "/WeatherUpdateInterval.txt", NK_WeatherUpdateInterval },
  { "/WindingTempOffset.txt", NK_WindingTempOffset },
  { "/Ymax1.txt", NK_Ymax1 },
  { "/Ymax2.txt", NK_Ymax2 },
  { "/Ymax3.txt", NK_Ymax3 },
  { "/Ymax4.txt", NK_Ymax4 },
  { "/Ymin1.txt", NK_Ymin1 },
  { "/Ymin2.txt", NK_Ymin2 },
  { "/Ymin3.txt", NK_Ymin3 },
  { "/Ymin4.txt", NK_Ymin4 },
  { "/absorptionCompleteTime.txt", NK_absorptionCompleteTime },
  { "/altPaused.txt", NK_altPaused },
  { "/altbaseSec.txt", NK_altbaseSec },
  { "/bmsLogic.txt", NK_bmsLogic },
  { "/bmsLogicLevelOff.txt", NK_bmsLogicLevelOff },
  { "/bulkVoltageHoldMs.txt", NK_bulkVoltageHoldMs },
  { "/capLimitMode.txt", NK_capLimitMode },
  { "/cvConsecutiveReads.txt", NK_cvConsecutiveReads },
  { "/cvKOvershoot.txt", NK_cvKOvershoot },
  { "/cvWaveAmplitudeV.txt", NK_cvWaveAmplitudeV },
  { "/cvWavePeriodSec.txt", NK_cvWavePeriodSec },
  { "/displayTempUnit.txt", NK_displayTempUnit },
  { "/gpsManualActive.txt", NK_gpsManualActive },
  { "/gpsTimeSourceMode.txt", NK_gpsTimeSourceMode },
  { "/hardwarePresent.txt", NK_hardwarePresent },
  { "/maxPoints.txt", NK_maxPoints },
  { "/perfPaused.txt", NK_perfPaused },
  { "/performanceRatio.txt", NK_performanceRatio },
  { "/plotTimeWindow.txt", NK_plotTimeWindow },
  { "/rebulkDebounceTime.txt", NK_rebulkDebounceTime },
  { "/socInfoAvailable.txt", NK_socInfoAvailable },
  { "/thermalConsecutiveReads.txt", NK_thermalConsecutiveReads },
  { "/thermalKOvershoot.txt", NK_thermalKOvershoot },
  { "/thermalKUndershoot.txt", NK_thermalKUndershoot },
  { "/thermalSettleThreshF.txt", NK_thermalSettleThreshF },
  { "/thermalWaveHalfPeriodMin.txt", NK_thermalWaveHalfPeriodMin },
  { "/thermalWaveHighF.txt", NK_thermalWaveHighF },
  { "/thermalWaveLowF.txt", NK_thermalWaveLowF },
  { "/timeAxisModeChanging.txt", NK_timeAxisModeChanging },
  { "/totalPowerCycles.txt", NK_totalPowerCycles },
  { "/waveAmplitude.txt", NK_waveAmplitude },
  { "/wavePeriod.txt", NK_wavePeriod },
  { "/weatherDataValid.txt", NK_weatherDataValid },
  { "/weatherModeEnabled.txt", NK_weatherModeEnabled },
  { "/webgaugesinterval.txt", NK_webgaugesinterval },
  { "/xTime.txt", NK_xTime },
  { "/yyMax.txt", NK_yyMax },
  { "/yyMin.txt", NK_yyMin },
  { "/altRpmTol.txt", "altRpmTol" },
  { "/altRpmSec.txt", "altRpmSec" },
  { "/altDutyTolPct.txt", "altDutyTolPct" },
  { "/altDutySec.txt", "altDutySec" },
  { "/altVbusTol.txt", "altVbusTol" },
  { "/altVbusSec.txt", "altVbusSec" },
  { "/altThermDegF.txt", "altThermDegF" },
  { "/altThermSec.txt", "altThermSec" },
  { "/altMinAmps.txt", "altMinAmps" },
  { "/altMinDuty.txt", "altMinDuty" },
  { "/altSafetyMargin.txt", "altSafetyMargin" },
  { "/altIdwPower.txt", "altIdwPower" },
  { "/altPruneK.txt", "altPruneK" },
  { "/perfWsTol.txt", "perfWsTol" },
  { "/perfWsSec.txt", "perfWsSec" },
  { "/perfWaTol.txt", "perfWaTol" },
  { "/perfWaSec.txt", "perfWaSec" },
  { "/perfSeaTol.txt", "perfSeaTol" },
  { "/perfSeaSec.txt", "perfSeaSec" },
  { "/perfSeaWinSec.txt", "perfSeaWinSec" },
  { "/perfRpmTol.txt", "perfRpmTol" },
  { "/perfRpmSec.txt", "perfRpmSec" },
  { "/perfHwTol.txt", "perfHwTol" },
  { "/perfHwSec.txt", "perfHwSec" },
  { "/perfMinBoatSpeed.txt", "perfMinBoatSpee" },
  { "/perfMinWindSpeed.txt", "perfMinWindSpee" },
  { "/perfRpmFloor.txt", "perfRpmFloor" },
  { "/perfSafetyMargin.txt", "perfSafetyMargi" },
  { "/perfIdwPower.txt", "perfIdwPower" },
  { "/perfPruneK.txt", "perfPruneK" },
  { "/perfSpeedSrc.txt", "perfSpeedSrc" },
  { "/perfFoldSymmetric.txt", "perfFoldSymmetr" },
  // 2nd pass: WiFi provisioning, interface password, IMU level calibration.
  // Values may carry a trailing newline from the old file writers (println) —
  // the loaders trim. imu_zero is the whole JSON blob as one string value.
  { "/ssid.txt", NK_ssid },
  { "/pass.txt", NK_pass },
  { "/apssid.txt", NK_apssid },
  { "/appass.txt", NK_appass },
  { "/first_config_done.txt", NK_first_config_done },
  { "/password.txt", NK_password },
  { "/password.hash", NK_passwordHash },
  { "/imu_zero.json", NK_imu_zero },
};

void importLegacySettingsFromLittleFS() {
  if (!littleFSMounted) return;
  int moved = 0;
  for (size_t i = 0; i < sizeof(LEGACY_SETTINGS) / sizeof(LEGACY_SETTINGS[0]); i++) {
    if (!fsExists(LEGACY_SETTINGS[i].file)) continue;
    if (!settingExists(LEGACY_SETTINGS[i].key)) {
      String v = readFile(LittleFS, LEGACY_SETTINGS[i].file);
      if (v.length()) settingWrite(LEGACY_SETTINGS[i].key, v.c_str());
      moved++;
    }
    fsRemove(LEGACY_SETTINGS[i].file);
  }
  if (moved) Serial.printf("Settings: imported %d legacy LittleFS settings into NVS\n", moved);
}

bool fsMkdir(const char *path) {
  if (!fsMutex || xSemaphoreTake(fsMutex, pdMS_TO_TICKS(5000)) != pdTRUE) {
    Serial.println("fsMkdir: mutex timeout");
    return false;
  }
  bool result = LittleFS.mkdir(path);
  xSemaphoreGive(fsMutex);
  return result;
}

size_t fsTotalBytes() {
  if (!fsMutex || xSemaphoreTake(fsMutex, pdMS_TO_TICKS(5000)) != pdTRUE) {
    return 0;
  }
  size_t result = LittleFS.totalBytes();
  xSemaphoreGive(fsMutex);
  return result;
}

size_t fsUsedBytes() {
  if (!fsMutex || xSemaphoreTake(fsMutex, pdMS_TO_TICKS(5000)) != pdTRUE) {
    return 0;
  }
  size_t result = LittleFS.usedBytes();
  xSemaphoreGive(fsMutex);
  return result;
}

void fsTakeLock() {
  if (fsMutex) {
    if (xSemaphoreTake(fsMutex, pdMS_TO_TICKS(10000)) != pdTRUE) {
      Serial.println("fsTakeLock: mutex timeout - filesystem unprotected!");
    }
  }
}

void fsReleaseLock() {
  if (fsMutex) {
    xSemaphoreGive(fsMutex);
  }
}

// ───────────────────────────────────────────────────────────────────────────
// Versioned PSRAM-blob persistence — the shared scaffold for "accumulate in
// PSRAM, dump to a versioned LittleFS blob at field-off, restore at boot."
// Built for the long-term-plot ring and the alternator/boat matrices so each
// system stops re-hand-rolling the header, fsLock, chunked I/O, watchdog resets,
// and layout-change guard. The sensor ring + eff matrix predate this and keep
// their own copies; retrofit them onto this later. Field-off only (LittleFS
// writes can stall a core ~300 ms — never on a live control loop).
// ───────────────────────────────────────────────────────────────────────────
struct PsramBlobHeader {
  uint32_t magic;       // caller-chosen dataset id
  uint32_t version;     // caller-chosen layout version (bump on record-layout change)
  uint32_t count;       // records in body
  uint32_t recordSize;  // sizeof(record) — layout-change guard
  uint32_t userWord;    // caller flags (e.g. "reference finalized"); 0 if unused
};

// Dump `count` records to a versioned LittleFS blob. Records live in `base` as a
// ring of `capacity` slots starting at `startIdx`, written in logical order so a
// circular buffer dumps chronologically; a plain array passes startIdx=0,
// count=capacity (no wrap). Body goes out in ≤2 contiguous spans. Removes the
// partial file on any short write. Returns records written (0 on failure).
uint32_t writePsramBlob(const char *path, uint32_t magic, uint32_t version,
                        uint32_t userWord, const void *base, size_t recordSize,
                        uint32_t capacity, uint32_t startIdx, uint32_t count) {
  if (!base || recordSize == 0 || capacity == 0) return 0;
  if (count > capacity) count = capacity;

  fsTakeLock();
  File f = LittleFS.open(path, "w");
  if (!f) {
    fsReleaseLock();
    Serial.printf("writePsramBlob: open failed (%s)\n", path);
    return 0;
  }

  PsramBlobHeader hdr = { magic, version, count, (uint32_t)recordSize, userWord };
  bool ok = (f.write((const uint8_t *)&hdr, sizeof(hdr)) == sizeof(hdr));

  // Body in up to two contiguous spans: [startIdx..capacity) then wrap [0..rem).
  const uint8_t *bytes = (const uint8_t *)base;
  uint32_t remaining = count;
  uint32_t idx = startIdx % capacity;
  while (ok && remaining > 0) {
    uint32_t span = capacity - idx;          // slots to end of buffer
    if (span > remaining) span = remaining;
    size_t spanBytes = (size_t)span * recordSize;
    ok = (f.write(bytes + (size_t)idx * recordSize, spanBytes) == spanBytes);
    remaining -= span;
    idx = 0;                                 // wrapped span starts at buffer top
    esp_task_wdt_reset();
  }

  f.close();
  if (!ok) LittleFS.remove(path);            // drop the partial file
  fsReleaseLock();
  if (!ok) {
    Serial.printf("writePsramBlob: short write (%s) — file removed\n", path);
    return 0;
  }
  return count;
}

// Restore a writePsramBlob() blob into linear buffer `destBase` (`destCapacity`
// slots). Validates magic/version/recordSize; on any mismatch or short read it
// deletes the file and returns 0. Returns records restored (≤ destCapacity), and
// the stored userWord via `userWordOut` if non-null. `deleteAfter` removes the
// file post-restore (rings set true to avoid double-upload; grids set false).
uint32_t readPsramBlob(const char *path, uint32_t magic, uint32_t version,
                       void *destBase, size_t recordSize, uint32_t destCapacity,
                       uint32_t *userWordOut, bool deleteAfter) {
  if (!destBase || recordSize == 0 || destCapacity == 0) return 0;
  if (!fsExists(path)) return 0;

  fsTakeLock();
  File f = LittleFS.open(path, "r");
  if (!f) {
    fsReleaseLock();
    Serial.printf("readPsramBlob: open failed (%s)\n", path);
    return 0;
  }

  PsramBlobHeader hdr;
  bool ok = (f.readBytes((char *)&hdr, sizeof(hdr)) == sizeof(hdr)
             && hdr.magic == magic
             && hdr.version == version
             && hdr.recordSize == (uint32_t)recordSize);
  if (!ok) {
    f.close();
    LittleFS.remove(path);
    fsReleaseLock();
    Serial.printf("readPsramBlob: header mismatch (%s) — discarded\n", path);
    return 0;
  }

  uint32_t toRead = (hdr.count > destCapacity) ? destCapacity : hdr.count;
  size_t bodyBytes = (size_t)toRead * recordSize;
  size_t bodyRead = f.readBytes((char *)destBase, bodyBytes);
  esp_task_wdt_reset();
  f.close();
  if (deleteAfter) LittleFS.remove(path);
  fsReleaseLock();

  if (bodyRead != bodyBytes) {
    Serial.printf("readPsramBlob: short body read (%u/%u) (%s)\n",
                  (unsigned)bodyRead, (unsigned)bodyBytes, path);
    return 0;
  }
  if (userWordOut) *userWordOut = hdr.userWord;
  return toRead;
}

void performDeepFactoryReset() {
  Serial.println("\n=== DEEP FACTORY RESET INITIATED ===");
  queueConsoleMessage("DEEP FACTORY RESET: Scorched-earth reset starting...");

  // Step 0: Preserve cloud identity (authToken) across the wipe so that pressing
  // Erase All Memory does NOT lock the user out of their cloud account. Token is
  // held in a stack buffer for the ~2-3 second wipe; power-loss in that window
  // loses the token and requires support contact for re-registration.
  char savedToken[128] = { 0 };
  {
    nvs_handle_t h;
    if (nvs_open("cloud", NVS_READONLY, &h) == ESP_OK) {
      size_t len = sizeof(savedToken);
      esp_err_t e = nvs_get_str(h, "authToken", savedToken, &len);
      if (e != ESP_OK) savedToken[0] = '\0';
      nvs_close(h);
    }
    if (savedToken[0] != '\0') {
      Serial.println("RESET: Preserving authToken across NVS wipe");
    } else {
      Serial.println("RESET: No authToken to preserve (device unregistered)");
    }
  }

  // Step 1: Unmount and reformat LittleFS (userdata partition - rings, logs, buffer files,
  // vessel_info.json; user settings now live in NVS and are wiped in Step 2)
  Serial.println("RESET: Acquiring FS mutex and unmounting LittleFS...");
  if (fsMutex) {
    xSemaphoreTake(fsMutex, pdMS_TO_TICKS(5000));  // Block other FS ops
  }
  LittleFS.end();
  littleFSMounted = false;

  Serial.println("RESET: Formatting LittleFS...");
  if (LittleFS.format()) {
    Serial.println("RESET: LittleFS formatted successfully");
    queueConsoleMessage("RESET: LittleFS formatted");
  } else {
    Serial.println("RESET: WARNING - LittleFS format failed");
    queueConsoleMessage("WARNING: LittleFS format failed - continuing anyway");
  }

  if (fsMutex) {
    xSemaphoreGive(fsMutex);
  }

  // Remount using ensureLittleFS() so we respect the same partition label and flags as setup()
  if (ensureLittleFS()) {
    Serial.println("RESET: LittleFS remounted successfully");
  } else {
    Serial.println("RESET: WARNING - LittleFS remount failed");
    queueConsoleMessage("WARNING: LittleFS remount failed");
  }

  // Step 2: Erase ALL NVS namespaces (storage, cloud, auth, update_req, timesync, etc.)
  Serial.println("RESET: Erasing entire NVS flash partition...");
  nvs_flash_deinit();  // Deinit before erase to avoid stale handles
  esp_err_t nvs_err = nvs_flash_erase();
  if (nvs_err == ESP_OK) {
    Serial.println("RESET: All NVS namespaces erased successfully");
    queueConsoleMessage("RESET: All NVS data erased");
  } else {
    Serial.printf("RESET: WARNING - NVS erase failed: %s\n", esp_err_to_name(nvs_err));
    queueConsoleMessage("WARNING: NVS erase failed - continuing anyway");
  }
  nvs_err = nvs_flash_init();
  if (nvs_err != ESP_OK) {
    Serial.printf("RESET: WARNING - NVS reinit failed: %s\n", esp_err_to_name(nvs_err));
  } else {
    Serial.println("RESET: NVS flash reinitialized");
  }

  // Step 3: Recreate all defaults from hardcoded values
  Serial.println("RESET: Reinitializing settings with defaults...");
  InitSystemSettings();
  Serial.println("RESET: InitSystemSettings() complete");

  // Step 4: Restore preserved authToken so cloud account survives the wipe
  if (savedToken[0] != '\0') {
    nvs_handle_t h;
    if (nvs_open("cloud", NVS_READWRITE, &h) == ESP_OK) {
      esp_err_t e = nvs_set_str(h, "authToken", savedToken);
      if (e == ESP_OK) e = nvs_commit(h);
      nvs_close(h);
      if (e == ESP_OK) {
        authToken = String(savedToken);
        isRegistered = true;
        Serial.println("RESET: authToken restored - cloud account preserved");
        queueConsoleMessage("RESET: Cloud registration preserved");
      } else {
        Serial.printf("RESET: WARNING - authToken restore failed: %s\n", esp_err_to_name(e));
        queueConsoleMessage("WARNING: Cloud token restore failed - contact support");
      }
    }
  }

  queueConsoleMessage("DEEP FACTORY RESET: Complete! Restarting now...");
  Serial.println("=== DEEP FACTORY RESET COMPLETE - RESTARTING ===\n");
  Serial.flush();
  delay(1500);  // Give SSE message time to reach client
  ESP.restart();
}

// Temperature task with robust fault handling:
// - Reads DS18B20 scratchpad every 5s with CRC validation.
// - Rejects known invalid signatures (CRC fail, all-0xFF, power-on 85°C/185°F).
// - Holds last good value on any read fault; freshness is tracked separately.
// - Applies sanity range (-50..300°F).
// - Auto-corrects resolution if EEPROM or sensor reset changed it from 12-bit.
// - Re-enumerates sensor after disconnect detection.
// - tempTaskHealthy is owned by checkTempTaskHealth(); TempTask only sets it true on a clean read.
// - File writes use fsMutex for filesystem serialization.
// - Reads core0Busy as a courtesy (defers if HTTPS/NTP/OTA is mid-flight) but does NOT
//   set it — TempTask runs during active charging, and core0Busy gates AdjustFieldLearnMode,
//   so setting it would freeze the voltage/current control loops for ~190–750 ms every 5 s.
void TempTask(void *parameter) {
  // NO watchdog registration - keep it separate from main loop watchdog

  static uint8_t scratchPad[9];
  static unsigned long lastTempRead = 0;
  static bool lastReadWasSuccess = true;  // false after any failure; drives 1s retry vs 5s normal poll
  static float lastValidTemp = -99;  // Track last valid reading (-99 = uninitialized)
  static bool sensorEnumerated = false;

// Config byte derived from global resolution (9..12 bit -> 0x1F/0x3F/0x5F/0x7F); target is 12-bit (0x7F)
#define DS18B20_CFG_BYTE (0x1F | ((resolution - 9) << 5))

  for (;;) {
    unsigned long now = millis();

    lastTempTaskHeartbeat = now;

    if (otaInProgress) {
      tempTaskHandle = NULL;
      vTaskDelete(NULL);
    }

    if (hardwarePresent == 0) {
      vTaskDelay(pdMS_TO_TICKS(5000));
      continue;
    }

    if (!sensorEnumerated) {
      sensors.begin();
      sensors.setWaitForConversion(false);  // re-apply after begin() in case it resets state
      sensors.setCheckForConversion(true);
      if (sensors.getAddress(tempDeviceAddress, 0)) {
        sensors.setResolution(tempDeviceAddress, resolution);
        sensorEnumerated = true;
      } else {
        tempEnumerateFailCount++;
        vTaskDelay(pdMS_TO_TICKS(5000));
        continue;
      }
    }

    if (core0Busy) {
      tempCoreBusySkipCount++;
      vTaskDelay(pdMS_TO_TICKS(1000));
      continue;
    }

    uint32_t pollInterval = lastReadWasSuccess ? 5000 : 1000;
    if (now - lastTempRead < pollInterval) {
      tempStaleSkipCount++;
      vTaskDelay(pdMS_TO_TICKS(1000));
      continue;
    }

    bool thisReadSucceeded = false;
    bool writeMaxTemp = false;
    bool writeMaxTempAllTime = false;
    float pendingMaxTemp = 0.0f;
    float pendingMaxTempAllTime = 0.0f;

    if (!sensors.isConnected(tempDeviceAddress)) {
      tempConnectedFailCount++;
      lastValidTemp = -99;
      sensorEnumerated = false;
      goto cleanup;
    }

    if (!sensors.requestTemperaturesByAddress(tempDeviceAddress)) {
      tempRequestFailCount++;
      goto cleanup;
    }

    {
      // Poll for conversion complete instead of a fixed 2000ms block.
      // Timeout is resolution-based per library + small guard band.
      unsigned long convStart = millis();
      unsigned long convTimeout = sensors.millisToWaitForConversion(resolution) + 50;

      while (!sensors.isConversionComplete()) {
        vTaskDelay(pdMS_TO_TICKS(10));
        lastTempTaskHeartbeat = millis();
        if (otaInProgress) goto cleanup;

        if (millis() - convStart > convTimeout) {
          tempRequestFailCount++;
          goto cleanup;
        }
      }
    }

    if (sensors.readScratchPad(tempDeviceAddress, scratchPad)) {

      // CHECK 1: CRC validation
      uint8_t crc = OneWire::crc8(scratchPad, 8);
      if (crc != scratchPad[8]) {
        tempCrcFailCount++;

        // Immediate single retry
        vTaskDelay(pdMS_TO_TICKS(2));
        if (sensors.readScratchPad(tempDeviceAddress, scratchPad)) {
          uint8_t crc2 = OneWire::crc8(scratchPad, 8);
          if (crc2 != scratchPad[8]) {
            goto cleanup;
          } else {
            tempCrcRecoveredCount++;
          }
        } else {
          tempReadFailCount++;
          goto cleanup;
        }
      }

      // CHECK 2: Detect all-0xFF (disconnected sensor)
      bool allFF = true;
      for (int i = 0; i < 9; i++) {
        if (scratchPad[i] != 0xFF) {
          allFF = false;
          break;
        }
      }
      if (allFF) {
        tempAllFFCount++;
        lastValidTemp = -99;
        sensorEnumerated = false;
        goto cleanup;
      }

      int16_t raw = (scratchPad[1] << 8) | scratchPad[0];
      float tempC = raw / 16.0f;
      float tempF = tempC * 1.8f + 32.0f;

      // CHECK 3: Power-on signature (0x0550 = 85°C = 185°F)
      if (raw == 0x0550) {
        tempPowerOn85Count++;
        goto cleanup;
      }

      // CHECK 4: Verify resolution; auto-correct if EEPROM or reset changed it
      if (scratchPad[4] != DS18B20_CFG_BYTE) {
        tempResolutionFixCount++;
        sensors.setResolution(tempDeviceAddress, resolution);
        if (!sensors.requestTemperaturesByAddress(tempDeviceAddress)) {
          tempRequestFailCount++;
          goto cleanup;
        }

        {
          // Poll for conversion complete after resolution fix; same approach as primary wait.
          unsigned long convStart2 = millis();
          unsigned long convTimeout2 = sensors.millisToWaitForConversion(resolution) + 50;

          while (!sensors.isConversionComplete()) {
            vTaskDelay(pdMS_TO_TICKS(10));
            lastTempTaskHeartbeat = millis();
            if (otaInProgress) goto cleanup;

            if (millis() - convStart2 > convTimeout2) {
              tempRequestFailCount++;
              goto cleanup;
            }
          }
        }

        if (!sensors.readScratchPad(tempDeviceAddress, scratchPad)) {
          tempRereadFailCount++;
          goto cleanup;
        }
        if (OneWire::crc8(scratchPad, 8) != scratchPad[8]) {
          tempResolutionFixCrcFailCount++;
          goto cleanup;
        }
        // Recalculate from corrected scratchpad
        raw = (scratchPad[1] << 8) | scratchPad[0];
        tempC = raw / 16.0f;
        tempF = tempC * 1.8f + 32.0f;
      }

      // CHECK 5: Sanity range
      if (tempF > -50 && tempF < 300) {
        unsigned long ageBeforeThisGoodRead = millis() - tempLastSuccessMillis;
        tempReadSuccessCount++;
        tempLastGoodF = tempF;
        tempLastSuccessMillis = millis();

        AlternatorTemperatureF = tempF;
        lastValidTemp = tempF;
        tempTaskHealthy = true;
        thisReadSucceeded = true;
        MARK_FRESH(IDX_ALTERNATOR_TEMP);
        wmIgnUpdate(wmIgn_altTempF, AlternatorTemperatureF);  // ignition-cycle watermark

        if (AlternatorTemperatureF > MaxAlternatorTemperatureF) {
          pendingMaxTemp = AlternatorTemperatureF;
          writeMaxTemp = true;
        }
        if (AlternatorTemperatureF > MaxAlternatorTemperatureF_AllTime) {
          pendingMaxTempAllTime = AlternatorTemperatureF;
          writeMaxTempAllTime = true;
        }
      } else {
        tempOutOfRangeCount++;
      }
    } else {
      tempReadFailCount++;
    }

cleanup:
    lastReadWasSuccess = thisReadSucceeded;
    lastTempRead = millis();
    lastTempTaskHeartbeat = millis();

    // Alert on persistent sensor failure — 5 consecutive non-success reads
    {
      static uint8_t tempConsecFail = 0;
      static unsigned long lastTempFailAlert = 0;
      if (thisReadSucceeded) {
        tempConsecFail = 0;
      } else {
        tempConsecFail++;
        if (tempConsecFail >= 5 && (millis() - lastTempFailAlert > 60000)) {
          queueConsoleMessage("DS18B20 WARNING: 5+ consecutive read failures — check sensor connection");
          lastTempFailAlert = millis();
          tempConsecFail = 0;
        }
      }
    }

    if (writeMaxTemp) {
      MaxAlternatorTemperatureF = pendingMaxTemp;
    }
    if (writeMaxTempAllTime) {
      MaxAlternatorTemperatureF_AllTime = pendingMaxTempAllTime;
    }

    if (otaInProgress) {
      tempTaskHandle = NULL;
      vTaskDelete(NULL);
    }
  }
}

void httpsTask(void *param) {
  if (otaInProgress) {
    return;  // Skip during OTA
  }
  esp_task_wdt_add(NULL);
  // Serial.println("HTTPS Task started on Core 0");
  // Serial.printf("sizeof(HttpsRequest) = %d bytes\n", sizeof(HttpsRequest));
  // Serial.printf("Queue item size = %d bytes\n", sizeof(HttpsRequest));

  static int consecutiveFailures = 0;
  const int MAX_CONSECUTIVE_FAILURES = 5;
  static unsigned long uploadsSuspendedUntil = 0;
  for (;;) {
    // Check if uploads are suspended due to consecutive failures
    if (uploadsSuspendedUntil && millis() < uploadsSuspendedUntil) {
      vTaskDelay(pdMS_TO_TICKS(500));
      esp_task_wdt_reset();
      continue;
    }

    esp_task_wdt_reset();

    HttpsRequest request;
    if (xQueueReceive(httpsQueue, &request, pdMS_TO_TICKS(1000))) {

      // VERIFY PAYLOAD BEFORE PROCESSING
      if (request.type == HTTPS_UPLOAD_PAYLOAD || request.type == HTTPS_UPLOAD_CONFIG || request.type == HTTPS_UPLOAD_BOATPERF || request.type == HTTPS_UPLOAD_ALTHEALTH) {
        size_t payloadLen = request.payload ? strlen(request.payload) : 0;
        // payloadCap is the ps_malloc'd capacity (NOT sizeof a pointer); free + skip on any bad payload.
        if (!request.payload || payloadLen >= request.payloadCap || payloadLen == 0) {
          if (request.payload) free(request.payload);
          continue;
        }
        if (request.payload[0] != '{') {
          free(request.payload);
          continue;
        }
      }

      core0Busy = true;
      unsigned long opStart = millis();
      bool opSuccess = false;

      // Execute operation
      switch (request.type) {
        case HTTPS_UPLOAD_PAYLOAD:
          opSuccess = executeUploadPayload(request.payload);
          break;
        case HTTPS_UPLOAD_CONFIG:
          opSuccess = executeUploadConfig(request.payload);
          break;
        case HTTPS_UPLOAD_BOATPERF:
          opSuccess = executeUploadBoatPerf(request.payload);
          break;
        case HTTPS_UPLOAD_ALTHEALTH:
          opSuccess = executeUploadAltHealth(request.payload);
          break;
        case HTTPS_FETCH_WEATHER:
          executeFetchWeatherData();
          opSuccess = true;
          break;
        case HTTPS_UPDATE_FW_VERSION:
          executeUpdateFirmwareVersion();
          opSuccess = true;
          break;
        case HTTPS_CHECK_FORCED_UPDATE:
          executeCheckForcedUpdate();
          opSuccess = true;
          break;
        case HTTPS_CLEAR_FORCED_UPDATE:
          executeClearForcedUpdate();
          opSuccess = true;
          break;
      }

      unsigned long opDuration = millis() - opStart;

      if (opDuration > 9000) {
        Serial.printf("WARNING: Operation took %lums (>9s limit)\n", opDuration);
      }

      // Track consecutive failures for uploads
      if (request.type == HTTPS_UPLOAD_PAYLOAD) {
        if (opSuccess) {
          consecutiveFailures = 0;
        } else {
          // Transport-level failures (WiFi drop, connect fail, send fail, read
          // timeout) return from executeUploadPayload before its response
          // handling, leaving the buffered in-flight marker set — clear it here
          // or uploadBufferedRecords() refuses to queue forever and the full
          // ring silently drops every new snapshot. The record itself stays in
          // the ring for retry. (Idempotent: parsed-response failure paths
          // already cleared it.)
          if (lastUploadWasBuffered && sensorRingInFlightIndex >= 0) {
            sensorRingInFlightIndex = -1;
            lastUploadWasBuffered = false;
          }
        }
        if (!opSuccess && lastHttpResponseCode != 400 && lastHttpResponseCode != 401) {
          // Don't count 400/401 as failures - they indicate bad data that gets auto-deleted
          consecutiveFailures++;
          //Serial.printf("Consecutive failures: %d/%d\n", consecutiveFailures, MAX_CONSECUTIVE_FAILURES);MAX_CONSECUTIVE_FAILURES);

          // Add delay after failure to prevent rapid retries
          // Serial.println("Upload failed - delaying 3s before next attempt");
          unsigned long delayStart = millis();
          while (millis() - delayStart < 3000) {
            esp_task_wdt_reset();
            delay(100);
          }

          // If too many failures, suspend uploads temporarily
          if (consecutiveFailures >= MAX_CONSECUTIVE_FAILURES) {
            uploadsSuspendedUntil = millis() + 30000;  // 30 second backoff
            Serial.println("ERROR: Too many failures - suspending uploads for 30s");
            queueConsoleMessage("Cloud sync paused, too many failures, retry in 30s");
            consecutiveFailures = 0;
          }
        }
      }

      lastHttpsOperationTime = millis();
      core0Busy = false;  // Clear lock AFTER operation completes
      esp_task_wdt_reset();

      if (request.payload) { free(request.payload); request.payload = nullptr; }  // release the PSRAM payload we own
    }
  }
}
// SENSOR AGGREGATION FUNCTIONS
// Init at boot: reset the in-progress accumulator windows. The PSRAM ring
// itself is restored by restoreSensorRingFromLittleFS() right after this.
// Also one-shot scrub of the legacy /sensor_buffer/ dir from pre-May2026
// firmware — no-op on fresh devices.
void initSensorBuffer() {
  resetSensorWindow();
  resetAccelWindow();

  const char *legacyDir = "/sensor_buffer";
  if (fsExists(legacyDir)) {
    fsTakeLock();
    File root = LittleFS.open(legacyDir);
    if (root && root.isDirectory()) {
      File f = root.openNextFile();
      while (f) {
        esp_task_wdt_reset();
        if (!f.isDirectory()) {
          String p = f.path();
          f.close();
          LittleFS.remove(p.c_str());
        }
        f = root.openNextFile();
      }
      root.close();
      LittleFS.rmdir(legacyDir);
    }
    fsReleaseLock();
    Serial.println("Scrubbed legacy /sensor_buffer/ dir");
  }
}

void resetSensorWindow() {
  currentWindow->battVolt_min = 999900;
  currentWindow->battVolt_max = 0;
  currentWindow->battVolt_area_v_us = 0;
  currentWindow->battVolt_valid_us = 0;

  currentWindow->battCurr_min = 999900;
  currentWindow->battCurr_max = -999900;
  currentWindow->battCurr_area_v_us = 0;
  currentWindow->battCurr_valid_us = 0;

  currentWindow->altCurr_min = 999900;
  currentWindow->altCurr_max = 0;
  currentWindow->altCurr_area_v_us = 0;
  currentWindow->altCurr_valid_us = 0;

  currentWindow->victronCurr_min = 999900;
  currentWindow->victronCurr_max = -999900;
  currentWindow->victronCurr_area_v_us = 0;
  currentWindow->victronCurr_valid_us = 0;

  currentWindow->soc_min = 999900;
  currentWindow->soc_max = 0;
  currentWindow->soc_area_v_us = 0;
  currentWindow->soc_valid_us = 0;

  currentWindow->baro_min = 999900;
  currentWindow->baro_max = 0;
  currentWindow->baro_area_v_us = 0;
  currentWindow->baro_valid_us = 0;

  currentWindow->altTemp_min = 999900;
  currentWindow->altTemp_max = -999900;
  currentWindow->altTemp_area_v_us = 0;
  currentWindow->altTemp_valid_us = 0;

  currentWindow->tempTherm_min = 999900;
  currentWindow->tempTherm_max = -999900;
  currentWindow->tempTherm_area_v_us = 0;
  currentWindow->tempTherm_valid_us = 0;

  currentWindow->ambTemp_min = 999900;
  currentWindow->ambTemp_max = -999900;
  currentWindow->ambTemp_area_v_us = 0;
  currentWindow->ambTemp_valid_us = 0;

  currentWindow->rpm_min = 999900;
  currentWindow->rpm_max = 0;
  currentWindow->rpm_area_v_us = 0;
  currentWindow->rpm_valid_us = 0;

  currentWindow->wifiStr_min = 999900;
  currentWindow->wifiStr_max = -999900;
  currentWindow->wifiStr_area_v_us = 0;
  currentWindow->wifiStr_valid_us = 0;

  currentWindow->dutyCycle_min = 999900;
  currentWindow->dutyCycle_max = 0;
  currentWindow->dutyCycle_area_v_us = 0;
  currentWindow->dutyCycle_valid_us = 0;

  currentWindow->altZero_min = 999900;
  currentWindow->altZero_max = -999900;
  currentWindow->altZero_area_v_us = 0;
  currentWindow->altZero_valid_us = 0;

  currentWindow->sog_min = 999900;
  currentWindow->sog_max = 0;
  currentWindow->sog_area_v_us = 0;
  currentWindow->sog_valid_us = 0;

  currentWindow->cog_min = 999900;
  currentWindow->cog_max = 0;
  currentWindow->cog_area_v_us = 0;
  currentWindow->cog_valid_us = 0;

  currentWindow->heading_min = 999900;
  currentWindow->heading_max = 0;
  currentWindow->heading_area_v_us = 0;
  currentWindow->heading_valid_us = 0;

  currentWindow->aws_min = 999900;
  currentWindow->aws_max = 0;
  currentWindow->aws_area_v_us = 0;
  currentWindow->aws_valid_us = 0;

  currentWindow->awa_min = 999900;
  currentWindow->awa_max = 0;
  currentWindow->awa_area_v_us = 0;
  currentWindow->awa_valid_us = 0;

  currentWindow->tws_min = 999900;
  currentWindow->tws_max = 0;
  currentWindow->tws_area_v_us = 0;
  currentWindow->tws_valid_us = 0;

  currentWindow->twa_min = 999900;
  currentWindow->twa_max = 0;
  currentWindow->twa_area_v_us = 0;
  currentWindow->twa_valid_us = 0;

  currentWindow->leeway_min = 999900;
  currentWindow->leeway_max = -999900;
  currentWindow->leeway_area_v_us = 0;
  currentWindow->leeway_valid_us = 0;

  currentWindow->vmg_min = 999900;
  currentWindow->vmg_max = -999900;
  currentWindow->vmg_area_v_us = 0;
  currentWindow->vmg_valid_us = 0;

  currentWindow->lat_current = 0;
  currentWindow->lon_current = 0;

  currentWindow->uTargetAmps_min = 999900;
  currentWindow->uTargetAmps_max = -999900;
  currentWindow->uTargetAmps_area_v_us = 0;
  currentWindow->uTargetAmps_valid_us = 0;

  currentWindow->tempMargin_min = 999900;
  currentWindow->tempMargin_max = -999900;
  currentWindow->tempMargin_area_v_us = 0;
  currentWindow->tempMargin_valid_us = 0;

  currentWindow->lastUpdateTime_us = micros();  // Initialize timing
  currentWindow->windowStartTime = millis();
}

void resetAccelWindow() {
  // Raw accel (signed - can be negative, scaled by 1000)
  imuWindow->accel_x_min = 999900;
  imuWindow->accel_x_max = -999900;
  imuWindow->accel_x_area_v_us = 0;
  imuWindow->accel_x_valid_us = 0;

  imuWindow->accel_y_min = 999900;
  imuWindow->accel_y_max = -999900;
  imuWindow->accel_y_area_v_us = 0;
  imuWindow->accel_y_valid_us = 0;

  imuWindow->accel_z_min = 999900;
  imuWindow->accel_z_max = -999900;
  imuWindow->accel_z_area_v_us = 0;
  imuWindow->accel_z_valid_us = 0;

  // Raw gyro (signed - can be negative, scaled by 100)
  imuWindow->gyro_x_min = 999900;
  imuWindow->gyro_x_max = -999900;
  imuWindow->gyro_x_area_v_us = 0;
  imuWindow->gyro_x_valid_us = 0;

  imuWindow->gyro_y_min = 999900;
  imuWindow->gyro_y_max = -999900;
  imuWindow->gyro_y_area_v_us = 0;
  imuWindow->gyro_y_valid_us = 0;

  imuWindow->gyro_z_min = 999900;
  imuWindow->gyro_z_max = -999900;
  imuWindow->gyro_z_area_v_us = 0;
  imuWindow->gyro_z_valid_us = 0;

  // Calculated metrics - heel/pitch (signed, scaled by 100)
  imuWindow->heel_min = 999900;
  imuWindow->heel_max = -999900;
  imuWindow->heel_area_v_us = 0;
  imuWindow->heel_valid_us = 0;

  imuWindow->pitch_min = 999900;
  imuWindow->pitch_max = -999900;
  imuWindow->pitch_area_v_us = 0;
  imuWindow->pitch_valid_us = 0;

  // Calculated metrics - accelerations (signed, scaled by 1000)
  imuWindow->vertical_accel_min = 999900;
  imuWindow->vertical_accel_max = -999900;
  imuWindow->vertical_accel_area_v_us = 0;
  imuWindow->vertical_accel_valid_us = 0;

  imuWindow->total_accel_min = 999900;
  imuWindow->total_accel_max = 0;  // Total accel is always positive
  imuWindow->total_accel_area_v_us = 0;
  imuWindow->total_accel_valid_us = 0;

  // Vibration energy (scaled by 1000000)
  imuWindow->hf_vibe_min = 999900000;
  imuWindow->hf_vibe_max = 0;
  imuWindow->hf_vibe_area_v_us = 0;
  imuWindow->hf_vibe_valid_us = 0;

  // 60s rolling metrics
  imuWindow->heel_change_60s = 0;
  imuWindow->heel_deviation_60s = 0;
  imuWindow->pitch_change_60s = 0;
  imuWindow->pitch_deviation_60s = 0;

  // Wave period
  imuWindow->wave_period = -1000;  // -1.0s scaled

  // Period counters
  imuWindow->slam_count = 0;
  imuWindow->slam_peak_max = 0;

  // Timing
  imuWindow->lastUpdateTime_us = micros();
  imuWindow->windowStartTime = millis();
}

void updateSensorWindow() {
  // Calculate time delta since last update
  uint64_t now_us = micros();
  uint64_t delta_us;

  if (currentWindow->lastUpdateTime_us == 0) {
    // First call after reset - no delta yet
    delta_us = 0;
  } else {
    delta_us = now_us - currentWindow->lastUpdateTime_us;
  }

  currentWindow->lastUpdateTime_us = now_us;

  // If this is the first call (delta_us == 0), skip accumulation but still update min/max
  bool shouldAccumulate = (delta_us > 0);

  // Battery voltage
  int32_t battVolt = (int32_t)(getBatteryVoltage() * 100.0);
  if (battVolt < currentWindow->battVolt_min) currentWindow->battVolt_min = battVolt;
  if (battVolt > currentWindow->battVolt_max) currentWindow->battVolt_max = battVolt;
  if (shouldAccumulate) {
    currentWindow->battVolt_area_v_us += (int64_t)battVolt * delta_us;
    currentWindow->battVolt_valid_us += delta_us;
  }

  // Battery current
  int32_t battCurr = (int32_t)(Bcur * 100.0);
  if (battCurr < currentWindow->battCurr_min) currentWindow->battCurr_min = battCurr;
  if (battCurr > currentWindow->battCurr_max) currentWindow->battCurr_max = battCurr;
  if (shouldAccumulate) {
    currentWindow->battCurr_area_v_us += (int64_t)battCurr * delta_us;
    currentWindow->battCurr_valid_us += delta_us;
  }

  // Alternator current
  int32_t altCurr = (int32_t)(MeasuredAmps * 100.0);
  if (altCurr < currentWindow->altCurr_min) currentWindow->altCurr_min = altCurr;
  if (altCurr > currentWindow->altCurr_max) currentWindow->altCurr_max = altCurr;
  if (shouldAccumulate) {
    currentWindow->altCurr_area_v_us += (int64_t)altCurr * delta_us;
    currentWindow->altCurr_valid_us += delta_us;
  }

  // Victron current
  int32_t victronCurr = (int32_t)(VictronCurrent * 100.0);
  if (victronCurr < currentWindow->victronCurr_min) currentWindow->victronCurr_min = victronCurr;
  if (victronCurr > currentWindow->victronCurr_max) currentWindow->victronCurr_max = victronCurr;
  if (shouldAccumulate) {
    currentWindow->victronCurr_area_v_us += (int64_t)victronCurr * delta_us;
    currentWindow->victronCurr_valid_us += delta_us;
  }

  // SOC
  int32_t soc = SOC_percent;
  if (soc < currentWindow->soc_min) currentWindow->soc_min = soc;
  if (soc > currentWindow->soc_max) currentWindow->soc_max = soc;
  if (shouldAccumulate) {
    currentWindow->soc_area_v_us += (int64_t)soc * delta_us;
    currentWindow->soc_valid_us += delta_us;
  }

  // Barometric pressure - CONDITIONAL on freshness
  if (!IS_STALE(IDX_BARO_PRESSURE) && isfinite(baroPressure)) {
    int32_t baro = (int32_t)(baroPressure * 100.0);
    if (baro < currentWindow->baro_min) currentWindow->baro_min = baro;
    if (baro > currentWindow->baro_max) currentWindow->baro_max = baro;
    if (shouldAccumulate) {
      currentWindow->baro_area_v_us += (int64_t)baro * delta_us;
      currentWindow->baro_valid_us += delta_us;
    }
  }

  // Alternator temperature - CONDITIONAL on validity (matches thermistor pattern below).
  // NaN (no OneWire reading yet) and the thermistor's -99 marker must never touch the
  // window, or the long-term plot's min envelope dips to -99 after every boot.
  if (TempSource == 0) {
    TempToUse = AlternatorTemperatureF;
  } else {
    TempToUse = temperatureThermistor;
  }
  if (!isnan(TempToUse) && TempToUse != -99) {
    int32_t altTemp = (int32_t)(TempToUse * 100.0);
    if (altTemp < currentWindow->altTemp_min) currentWindow->altTemp_min = altTemp;
    if (altTemp > currentWindow->altTemp_max) currentWindow->altTemp_max = altTemp;
    if (shouldAccumulate) {
      currentWindow->altTemp_area_v_us += (int64_t)altTemp * delta_us;
      currentWindow->altTemp_valid_us += delta_us;
    }
  }

  // Thermistor temperature - CONDITIONAL on freshness (matches baro/ambient pattern)
  if (!IS_STALE(IDX_THERMISTOR_TEMP) && temperatureThermistor != -99) {
    int32_t tempTherm = (int32_t)(temperatureThermistor * 100.0);
    if (tempTherm < currentWindow->tempTherm_min) currentWindow->tempTherm_min = tempTherm;
    if (tempTherm > currentWindow->tempTherm_max) currentWindow->tempTherm_max = tempTherm;
    if (shouldAccumulate) {
      currentWindow->tempTherm_area_v_us += (int64_t)tempTherm * delta_us;
      currentWindow->tempTherm_valid_us += delta_us;
    }
  }

  // Ambient temperature - CONDITIONAL on freshness
  if (!IS_STALE(IDX_AMBIENT_TEMP) && isfinite(ambientTemp)) {
    int32_t ambTemp = (int32_t)(ambientTemp * 100.0);
    if (ambTemp < currentWindow->ambTemp_min) currentWindow->ambTemp_min = ambTemp;
    if (ambTemp > currentWindow->ambTemp_max) currentWindow->ambTemp_max = ambTemp;
    if (shouldAccumulate) {
      currentWindow->ambTemp_area_v_us += (int64_t)ambTemp * delta_us;
      currentWindow->ambTemp_valid_us += delta_us;
    }
  }

  // RPM
  int32_t rpm = (int32_t)RPM;
  if (rpm < currentWindow->rpm_min) currentWindow->rpm_min = rpm;
  if (rpm > currentWindow->rpm_max) currentWindow->rpm_max = rpm;
  if (shouldAccumulate) {
    currentWindow->rpm_area_v_us += (int64_t)rpm * delta_us;
    currentWindow->rpm_valid_us += delta_us;
  }

  // WiFi strength
  int32_t wifiStr = WifiStrength;
  if (wifiStr < currentWindow->wifiStr_min) currentWindow->wifiStr_min = wifiStr;
  if (wifiStr > currentWindow->wifiStr_max) currentWindow->wifiStr_max = wifiStr;
  if (shouldAccumulate) {
    currentWindow->wifiStr_area_v_us += (int64_t)wifiStr * delta_us;
    currentWindow->wifiStr_valid_us += delta_us;
  }

  // Duty cycle
  int32_t duty = (int32_t)(dutyCycle * 100.0);
  if (duty < currentWindow->dutyCycle_min) currentWindow->dutyCycle_min = duty;
  if (duty > currentWindow->dutyCycle_max) currentWindow->dutyCycle_max = duty;
  if (shouldAccumulate) {
    currentWindow->dutyCycle_area_v_us += (int64_t)duty * delta_us;
    currentWindow->dutyCycle_valid_us += delta_us;
  }

  // Dynamic alternator zero
  int32_t altZero = (int32_t)(DynamicAltCurrentZero * 100.0);
  if (altZero < currentWindow->altZero_min) currentWindow->altZero_min = altZero;
  if (altZero > currentWindow->altZero_max) currentWindow->altZero_max = altZero;
  if (shouldAccumulate) {
    currentWindow->altZero_area_v_us += (int64_t)altZero * delta_us;
    currentWindow->altZero_valid_us += delta_us;
  }

  // Target amps (uTargetAmps)
  int32_t targetAmps = (int32_t)(uTargetAmps * 100.0);
  if (targetAmps < currentWindow->uTargetAmps_min) currentWindow->uTargetAmps_min = targetAmps;
  if (targetAmps > currentWindow->uTargetAmps_max) currentWindow->uTargetAmps_max = targetAmps;
  if (shouldAccumulate) {
    currentWindow->uTargetAmps_area_v_us += (int64_t)targetAmps * delta_us;
    currentWindow->uTargetAmps_valid_us += delta_us;
  }

  // Temperature margin
  float tempMargin = TemperatureLimitF - TempToUse;
  int32_t tempMarginScaled = (int32_t)(tempMargin * 100.0);
  if (tempMarginScaled < currentWindow->tempMargin_min) currentWindow->tempMargin_min = tempMarginScaled;
  if (tempMarginScaled > currentWindow->tempMargin_max) currentWindow->tempMargin_max = tempMarginScaled;
  if (shouldAccumulate) {
    currentWindow->tempMargin_area_v_us += (int64_t)tempMarginScaled * delta_us;
    currentWindow->tempMargin_valid_us += delta_us;
  }

  // Speed over ground - CONDITIONAL on freshness
  if (!IS_STALE(IDX_SOG_NMEA)) {
    int32_t sog = (int32_t)(SOGNMEA * 100.0);
    if (sog < currentWindow->sog_min) currentWindow->sog_min = sog;
    if (sog > currentWindow->sog_max) currentWindow->sog_max = sog;
    if (shouldAccumulate) {
      currentWindow->sog_area_v_us += (int64_t)sog * delta_us;
      currentWindow->sog_valid_us += delta_us;
    }
  }

  // Course over ground - CONDITIONAL on freshness
  if (!IS_STALE(IDX_COG_NMEA)) {
    int32_t cog = (int32_t)(COGNMEA * 100.0);
    if (cog < currentWindow->cog_min) currentWindow->cog_min = cog;
    if (cog > currentWindow->cog_max) currentWindow->cog_max = cog;
    if (shouldAccumulate) {
      currentWindow->cog_area_v_us += (int64_t)cog * delta_us;
      currentWindow->cog_valid_us += delta_us;
    }
  }

  // Heading
  if (!IS_STALE(IDX_HEADING_NMEA)) {
    int32_t heading = (int32_t)(HeadingNMEA * 100.0);
    if (heading < currentWindow->heading_min) currentWindow->heading_min = heading;
    if (heading > currentWindow->heading_max) currentWindow->heading_max = heading;
    if (shouldAccumulate) {
      currentWindow->heading_area_v_us += (int64_t)heading * delta_us;
      currentWindow->heading_valid_us += delta_us;
    }
  }

  // Apparent wind speed
  int32_t aws = (int32_t)(ApparentWindSpeedNMEA * 100.0);
  if (aws < currentWindow->aws_min) currentWindow->aws_min = aws;
  if (aws > currentWindow->aws_max) currentWindow->aws_max = aws;
  if (shouldAccumulate) {
    currentWindow->aws_area_v_us += (int64_t)aws * delta_us;
    currentWindow->aws_valid_us += delta_us;
  }

  // Apparent wind angle
  int32_t awa = (int32_t)(ApparentWindAngleNMEA * 100.0);
  if (awa < currentWindow->awa_min) currentWindow->awa_min = awa;
  if (awa > currentWindow->awa_max) currentWindow->awa_max = awa;
  if (shouldAccumulate) {
    currentWindow->awa_area_v_us += (int64_t)awa * delta_us;
    currentWindow->awa_valid_us += delta_us;
  }

  // True wind speed - CONDITIONAL on valid
  if (!isnan(TrueWindSpeedNMEA)) {
    int32_t tws = (int32_t)(TrueWindSpeedNMEA * 100.0);
    if (tws < currentWindow->tws_min) currentWindow->tws_min = tws;
    if (tws > currentWindow->tws_max) currentWindow->tws_max = tws;
    if (shouldAccumulate) {
      currentWindow->tws_area_v_us += (int64_t)tws * delta_us;
      currentWindow->tws_valid_us += delta_us;
    }
  }

  // True wind angle - CONDITIONAL on valid
  if (!isnan(TrueWindAngleNMEA)) {
    int32_t twa = (int32_t)(TrueWindAngleNMEA * 100.0);
    if (twa < currentWindow->twa_min) currentWindow->twa_min = twa;
    if (twa > currentWindow->twa_max) currentWindow->twa_max = twa;
    if (shouldAccumulate) {
      currentWindow->twa_area_v_us += (int64_t)twa * delta_us;
      currentWindow->twa_valid_us += delta_us;
    }
  }

  // Leeway - CONDITIONAL on valid
  if (!isnan(LeewayNMEA)) {
    int32_t leeway = (int32_t)(LeewayNMEA * 100.0);
    if (leeway < currentWindow->leeway_min) currentWindow->leeway_min = leeway;
    if (leeway > currentWindow->leeway_max) currentWindow->leeway_max = leeway;
    if (shouldAccumulate) {
      currentWindow->leeway_area_v_us += (int64_t)leeway * delta_us;
      currentWindow->leeway_valid_us += delta_us;
    }
  }

  // VMG - CONDITIONAL on valid
  if (!isnan(VMGNMEA)) {
    int32_t vmg = (int32_t)(VMGNMEA * 100.0);
    if (vmg < currentWindow->vmg_min) currentWindow->vmg_min = vmg;
    if (vmg > currentWindow->vmg_max) currentWindow->vmg_max = vmg;
    if (shouldAccumulate) {
      currentWindow->vmg_area_v_us += (int64_t)vmg * delta_us;
      currentWindow->vmg_valid_us += delta_us;
    }
  }

  // Update GPS buffer and store smoothed position
  updateGPSBuffer();
  double smoothLat, smoothLon;
  getSmoothedGPS(smoothLat, smoothLon);
  currentWindow->lat_current = smoothLat;
  currentWindow->lon_current = smoothLon;
}

// Build the cloud upload JSON for one sensor snapshot into the global
// payloadBuffer. Returns bytes written (excluding null terminator), 0 on
// overflow. Used by the PSRAM-ring upload path (uploadBufferedRecords).
// All payload values come from the snapshot itself (snap.window + snap.imu);
// lifetime/session accumulators moved to the 24h config snapshot upload.
size_t buildSnapshotJson(const SensorSnapshot &snap) {
// Per-window avg = time-weighted area / valid microseconds, then un-scale.
// Two scale factors in play: SensorWindow uses ×100 for most fields;
// ImuWindow uses ×100 for heel/pitch but ×1000 for vertical/total accel.
#define SAFE_AVG_100(area, valid)  ((valid) > 0 ? ((double)(area) / (double)(valid)) / 100.0  : 0.0)
#define SAFE_AVG_1000(area, valid) ((valid) > 0 ? ((double)(area) / (double)(valid)) / 1000.0 : 0.0)
  time_t finalTs = reconstructTimestamp((time_t)snap.collectionTime);
  const char *timestampStr = formatTimestamp(finalTs);
  int written = snprintf(
    payloadBuffer, PAYLOAD_BUFFER_SIZE,
    "{"
    // Identity & metadata
    "\"device_uid\":\"%s\","
    "\"token\":\"%s\","
    "\"timestamp\":\"%s\","
    "\"firmware_version_int\":%d,"
    // payload_v = ingest payload schema version; bump when this body's shape changes.
    // Edge fn inserts an explicit column whitelist, so it safely ignores this. See CLOUD_PLATFORM.md §3a.
    "\"payload_v\":1,"
    "\"current_time_source\":%d,"
    // Battery
    "\"batt_volt_min\":%.2f,\"batt_volt_max\":%.2f,\"batt_volt_avg\":%.2f,"
    "\"batt_curr_min\":%.2f,\"batt_curr_max\":%.2f,\"batt_curr_avg\":%.2f,"
    // Alternator
    "\"alt_curr_min\":%.2f,\"alt_curr_max\":%.2f,\"alt_curr_avg\":%.2f,"
    "\"duty_cycle_min\":%.2f,\"duty_cycle_max\":%.2f,\"duty_cycle_avg\":%.2f,"
    // Victron / external
    "\"victron_curr_min\":%.2f,\"victron_curr_max\":%.2f,\"victron_curr_avg\":%.2f,"
    // SOC
    "\"soc_min\":%.2f,\"soc_max\":%.2f,\"soc_avg\":%.2f,"
    // Engine
    "\"rpm_min\":%d,\"rpm_max\":%d,\"rpm_avg\":%d,"
    // Temperatures
    "\"alt_temp_min\":%.2f,\"alt_temp_max\":%.2f,\"alt_temp_avg\":%.2f,"
    "\"temp_therm_min\":%.2f,\"temp_therm_max\":%.2f,\"temp_therm_avg\":%.2f,"
    "\"amb_temp_min\":%.2f,\"amb_temp_max\":%.2f,\"amb_temp_avg\":%.2f,"
    // Pressure
    "\"baro_min\":%.2f,\"baro_max\":%.2f,\"baro_avg\":%.2f,"
    // Control loop diagnostics
    "\"u_target_amps_min\":%.2f,\"u_target_amps_max\":%.2f,\"u_target_amps_avg\":%.2f,"
    // NMEA navigation
    "\"sog_min\":%.2f,\"sog_max\":%.2f,\"sog_avg\":%.2f,"
    "\"lat_current\":%.6f,\"lon_current\":%.6f,"
    // NMEA wind & sailing
    "\"aws_min\":%.2f,\"aws_max\":%.2f,\"aws_avg\":%.2f,"
    "\"tws_min\":%.2f,\"tws_max\":%.2f,\"tws_avg\":%.2f,"
    "\"vmg_min\":%.2f,\"vmg_max\":%.2f,\"vmg_avg\":%.2f,"
    "\"leeway_min\":%.2f,\"leeway_max\":%.2f,\"leeway_avg\":%.2f,"
    // Wind/heading angles + alt-zero + charge stage (long-term-plot stitch fields).
    // awa/twa envelope; cog/heading/alt_zero avg-only; charge_stage categorical (0-7).
    "\"awa_min\":%.2f,\"awa_max\":%.2f,\"awa_avg\":%.2f,"
    "\"twa_min\":%.2f,\"twa_max\":%.2f,\"twa_avg\":%.2f,"
    "\"cog_avg\":%.2f,\"heading_avg\":%.2f,\"alt_zero_avg\":%.2f,"
    "\"charge_stage\":%d,"
    // IMU peak motion (per-window aggregates)
    "\"imu_heel_min\":%.2f,\"imu_heel_max\":%.2f,\"imu_heel_avg\":%.2f,"
    "\"imu_pitch_min\":%.2f,\"imu_pitch_max\":%.2f,\"imu_pitch_avg\":%.2f,"
    "\"imu_vertical_accel_min\":%.3f,\"imu_vertical_accel_max\":%.3f,\"imu_vertical_accel_avg\":%.3f,"
    "\"imu_total_accel_min\":%.3f,\"imu_total_accel_max\":%.3f,\"imu_total_accel_avg\":%.3f,"
    // IMU comfort scores (point values at upload time)
    "\"imu_msi_score\":%.2f,"
    "\"imu_vomit_pct\":%.2f,"
    "\"imu_anchorage_comfort\":%.2f,"
    "\"imu_wave_period_sec\":%.2f,"
    // IMU events (counters for this window)
    "\"imu_slam_count_window\":%u,"
    "\"imu_slam_peak_max_window\":%.3f"
    "}",
    device_id_hex,
    authToken.c_str(),
    timestampStr,
    firmwareVersionInt,
    (int)currentTimeSource,
    snap.window.battVolt_min / 100.0, snap.window.battVolt_max / 100.0,
    SAFE_AVG_100(snap.window.battVolt_area_v_us, snap.window.battVolt_valid_us),
    snap.window.battCurr_min / 100.0, snap.window.battCurr_max / 100.0,
    SAFE_AVG_100(snap.window.battCurr_area_v_us, snap.window.battCurr_valid_us),
    snap.window.altCurr_min / 100.0, snap.window.altCurr_max / 100.0,
    SAFE_AVG_100(snap.window.altCurr_area_v_us, snap.window.altCurr_valid_us),
    snap.window.dutyCycle_min / 100.0, snap.window.dutyCycle_max / 100.0,
    SAFE_AVG_100(snap.window.dutyCycle_area_v_us, snap.window.dutyCycle_valid_us),
    snap.window.victronCurr_min / 100.0, snap.window.victronCurr_max / 100.0,
    SAFE_AVG_100(snap.window.victronCurr_area_v_us, snap.window.victronCurr_valid_us),
    snap.window.soc_min / 100.0, snap.window.soc_max / 100.0,
    SAFE_AVG_100(snap.window.soc_area_v_us, snap.window.soc_valid_us),
    snap.window.rpm_min, snap.window.rpm_max,
    (int)SAFE_AVG_100(snap.window.rpm_area_v_us, snap.window.rpm_valid_us),
    snap.window.altTemp_min / 100.0, snap.window.altTemp_max / 100.0,
    SAFE_AVG_100(snap.window.altTemp_area_v_us, snap.window.altTemp_valid_us),
    snap.window.tempTherm_min / 100.0, snap.window.tempTherm_max / 100.0,
    SAFE_AVG_100(snap.window.tempTherm_area_v_us, snap.window.tempTherm_valid_us),
    snap.window.ambTemp_min / 100.0, snap.window.ambTemp_max / 100.0,
    SAFE_AVG_100(snap.window.ambTemp_area_v_us, snap.window.ambTemp_valid_us),
    snap.window.baro_min / 100.0, snap.window.baro_max / 100.0,
    SAFE_AVG_100(snap.window.baro_area_v_us, snap.window.baro_valid_us),
    snap.window.uTargetAmps_min / 100.0, snap.window.uTargetAmps_max / 100.0,
    SAFE_AVG_100(snap.window.uTargetAmps_area_v_us, snap.window.uTargetAmps_valid_us),
    snap.window.sog_min / 100.0, snap.window.sog_max / 100.0,
    SAFE_AVG_100(snap.window.sog_area_v_us, snap.window.sog_valid_us),
    snap.window.lat_current, snap.window.lon_current,
    snap.window.aws_min / 100.0, snap.window.aws_max / 100.0,
    SAFE_AVG_100(snap.window.aws_area_v_us, snap.window.aws_valid_us),
    snap.window.tws_min / 100.0, snap.window.tws_max / 100.0,
    SAFE_AVG_100(snap.window.tws_area_v_us, snap.window.tws_valid_us),
    snap.window.vmg_min / 100.0, snap.window.vmg_max / 100.0,
    SAFE_AVG_100(snap.window.vmg_area_v_us, snap.window.vmg_valid_us),
    snap.window.leeway_min / 100.0, snap.window.leeway_max / 100.0,
    SAFE_AVG_100(snap.window.leeway_area_v_us, snap.window.leeway_valid_us),
    snap.window.awa_min / 100.0, snap.window.awa_max / 100.0,
    SAFE_AVG_100(snap.window.awa_area_v_us, snap.window.awa_valid_us),
    snap.window.twa_min / 100.0, snap.window.twa_max / 100.0,
    SAFE_AVG_100(snap.window.twa_area_v_us, snap.window.twa_valid_us),
    SAFE_AVG_100(snap.window.cog_area_v_us, snap.window.cog_valid_us),
    SAFE_AVG_100(snap.window.heading_area_v_us, snap.window.heading_valid_us),
    SAFE_AVG_100(snap.window.altZero_area_v_us, snap.window.altZero_valid_us),
    (int)snap.chargeStage,
    // IMU values read from the frozen ImuSnapshot — values matching the moment
    // the window was rolled, not whatever imuWindow currently holds at upload time.
    snap.imu.heel_min / 100.0, snap.imu.heel_max / 100.0,
    SAFE_AVG_100(snap.imu.heel_area_v_us, snap.imu.heel_valid_us),
    snap.imu.pitch_min / 100.0, snap.imu.pitch_max / 100.0,
    SAFE_AVG_100(snap.imu.pitch_area_v_us, snap.imu.pitch_valid_us),
    snap.imu.vertical_accel_min / 1000.0, snap.imu.vertical_accel_max / 1000.0,
    SAFE_AVG_1000(snap.imu.vertical_accel_area_v_us, snap.imu.vertical_accel_valid_us),
    snap.imu.total_accel_min / 1000.0, snap.imu.total_accel_max / 1000.0,
    SAFE_AVG_1000(snap.imu.total_accel_area_v_us, snap.imu.total_accel_valid_us),
    snap.imu.msi_score,
    snap.imu.vomit_pct,
    snap.imu.anchorage_comfort,
    snap.imu.wave_period / 1000.0,
    (unsigned int)snap.imu.slam_count,
    snap.imu.slam_peak_max / 1000.0);
#undef SAFE_AVG_100
#undef SAFE_AVG_1000
  if (written <= 0 || written >= PAYLOAD_BUFFER_SIZE) return 0;
  return (size_t)written;
}

void uploadBufferedRecords() {
  if (otaInProgress) return;
  if (hardwarePresent != 1) return;   // sim mode (HardwarePresent=0): never upload fake data to the cloud
  if (ringIsEmpty()) return;
  if (!isRegistered || authToken.isEmpty()) {
    Serial.println("uploadBufferedRecords: skipping — not registered");
    return;
  }
  if (!canUploadNow()) return;

  // One upload in flight at a time. Core 0 advances the tail when it succeeds
  // (or hits a 400/401). On network failure it leaves the slot alone for retry.
  if (sensorRingInFlightIndex >= 0) return;

  SensorSnapshot snap;
  if (!peekTailSnapshot(&snap)) return;

  size_t written = buildSnapshotJson(snap);
  if (written == 0) {
    // JSON build overflow — drop this slot so we don't loop on it forever.
    Serial.println("uploadBufferedRecords: JSON overflow, dropping tail snapshot");
    popTailSnapshot();
    return;
  }

  HttpsRequest req = {};
  req.type = HTTPS_UPLOAD_PAYLOAD;
  req.payloadCap = PAYLOAD_BUFFER_SIZE;
  req.payload = (char *)ps_malloc(req.payloadCap);
  if (req.payload) {
    strncpy(req.payload, payloadBuffer, req.payloadCap - 1);
    req.payload[req.payloadCap - 1] = '\0';
    if (xQueueSend(httpsQueue, &req, 0) == pdTRUE) {
      sensorRingInFlightIndex = (int32_t)sensorRingTail;
      lastUploadWasBuffered = true;
    } else {
      free(req.payload);
      Serial.println("uploadBufferedRecords: httpsQueue full, will retry next tick");
    }
  }
  esp_task_wdt_reset();
}
bool executeUploadPayload(const char *payload) {
  if (WiFi.status() != WL_CONNECTED) return false;
  if (WiFi.RSSI() < -76) return false;
  if (!isRegistered || authToken.isEmpty()) {
    Serial.println("executeUploadPayload: ABORT - no token (file stays in buffer)");
    return false;
  }
  WiFiClientSecure client;
  client.setInsecure();
  // setTimeout omitted: Stream::setTimeout is ms (not seconds as some docs claim) and
  // the read loops below use available()+read() polling with explicit millis() deadlines,
  // so the Stream-level timeout doesn't gate anything here.
  client.setHandshakeTimeout(HANDSHAKE_TIMEOUT);

  uint32_t start = millis();
  esp_task_wdt_reset();

  // Hard-bounded TLS connect (silent on success; logs only on failure)
  if (!client.connect(host, port, CONNECT_TIMEOUT)) {
    Serial.println("Upload: connect fail");
    client.stop();
    return false;
  }

  // Defensive global timeout check after connect/handshake
  if (millis() - start > GLOBAL_TIMEOUT) {
    Serial.println("Upload: connect exceeded global timeout");
    client.stop();
    return false;
  }

  esp_task_wdt_reset();

  // Serial.println("=== REQUEST DEBUG ===");
  // Serial.printf("Host: %s\n", host);
  // Serial.printf("Port: %d\n", port);
  // Serial.printf("Endpoint: /functions/v1/update-sensor-history\n");
  // Serial.printf("API Key (first 50 chars): %.50s...\n", SUPABASE_ANON_KEY);
  // Serial.println("====================");



  // ===== Send headers (small, no heap) =====
  int headerBytes = client.printf(
    "POST /functions/v1/update-sensor-history HTTP/1.1\r\n"
    "Host: %s\r\n"
    "Content-Type: application/json\r\n"
    "Authorization: Bearer %s\r\n"  // ← Changed from "apikey: %s\r\n"
    "Connection: close\r\n"
    "Content-Length: %u\r\n\r\n",
    host,
    SUPABASE_ANON_KEY,
    (unsigned)strlen(payload));

  // NEW: Check header send success / connection state
  if (headerBytes <= 0 || !client.connected()) {
    Serial.println("Header send fail");
    client.stop();
    return false;
  }

  // ===== Stream payload directly =====
  size_t payloadLen = strlen(payload);
  size_t sent = client.write((const uint8_t *)payload, payloadLen);
  if (sent != payloadLen) {
    Serial.println("Payload send fail");
    client.stop();
    return false;
  }
  esp_task_wdt_reset();

  // ===== Read status line + drain headers (no header parsing/no truncation spam) =====
  int httpCode = 0;
  uint32_t readStart = millis();

  // 1) Read ONLY the first line (status line). Hard deadline.
  char statusBuf[64];
  size_t statusLen = 0;
  bool gotStatusLine = false;

  while (client.connected() && (millis() - readStart < READ_TIMEOUT)) {
    esp_task_wdt_reset();

    while (client.available()) {
      char c = (char)client.read();

      // collect status line until '\n'
      if (statusLen < sizeof(statusBuf) - 1) {
        statusBuf[statusLen++] = c;
      }

      if (c == '\n') {
        statusBuf[statusLen] = '\0';
        gotStatusLine = true;

        // Parse: "HTTP/1.1 200 ..." or "HTTP/2 200 ..."
        if (strncmp(statusBuf, "HTTP/", 5) == 0) {
          const char *sp = strchr(statusBuf, ' ');
          if (sp) httpCode = atoi(sp + 1);
        }
        goto drain_headers;
      }
    }

    if (millis() - start > GLOBAL_TIMEOUT) break;
    delay(1);
  }

drain_headers:
  // 2) Drain headers until CRLFCRLF (end of headers). No logging.
  // If we never got the status line, this will still just time out and we’ll return "No response".
  {
    uint32_t drainStart = millis();
    uint8_t state = 0;  // match sequence: \r \n \r \n

    while (client.connected() && (millis() - drainStart < READ_TIMEOUT)) {
      esp_task_wdt_reset();

      while (client.available()) {
        char c = (char)client.read();

        if (state == 0 && c == '\r') state = 1;
        else if (state == 1 && c == '\n') state = 2;
        else if (state == 2 && c == '\r') state = 3;
        else if (state == 3 && c == '\n') goto done_headers;
        else state = 0;
      }

      if (millis() - start > GLOBAL_TIMEOUT) break;
      delay(1);
    }
  }

done_headers:
  // We ignore body completely.
  client.stop();

  esp_task_wdt_reset();

  if (httpCode == 0) {
    Serial.println("No response received (timeout)");
    return false;
  }

  // Log only non-200 codes — success is the common path and was noisy.
  if (httpCode != 200) Serial.printf("Upload: HTTP %d\n", httpCode);
  lastHttpResponseCode = httpCode;

  // ===== Handle PSRAM-ring slot based on response code =====
  // sensorRingInFlightIndex was set by uploadBufferedRecords() when it queued
  // this request. On 200 → pop tail (slot consumed). On 400/401 → also pop
  // (bad data, no point retrying). On network/server error → leave ring as-is
  // and the next uploadBufferedRecords() tick will re-queue the same slot.
  if (lastUploadWasBuffered && sensorRingInFlightIndex >= 0) {
    if (httpCode == 200) {
      popTailSnapshot();
      if (ringIsEmpty()) {
        // Only announce "all uploaded" once per drain-to-empty transition.
        // pushSensorSnapshot re-arms the flag when count goes back above zero.
        if (!sensorRingAnnouncedEmpty) {
          queueConsoleMessage("Cloud sync: all data uploaded");
          sensorRingAnnouncedEmpty = true;
        }
      } else {
        // Throttle the "N queued" progress chatter to at most one per minute.
        // Full drain visibility is still on the dashboard's bufferedRecordCount.
        static unsigned long lastQueuedMsgMs = 0;
        if (millis() - lastQueuedMsgMs >= 60000) {
          lastQueuedMsgMs = millis();
          snprintf(messageBuffer, MESSAGE_BUFFER_SIZE, "Cloud sync: %u records queued", (unsigned)sensorRingCount);
          queueConsoleMessage(messageBuffer);
        }
      }
      sensorRingInFlightIndex = -1;

    } else if (httpCode == 400 || httpCode == 401) {
      Serial.printf("HTTP %d: dropping bad-data ring slot\n", httpCode);
      popTailSnapshot();
      snprintf(messageBuffer, MESSAGE_BUFFER_SIZE, "Cleared bad data (%u queued)", (unsigned)sensorRingCount);
      queueConsoleMessage(messageBuffer);
      sensorRingInFlightIndex = -1;

    } else {
      // Network/server error — keep slot for retry on next tick.
      Serial.printf("⚠ HTTP %d: keeping ring slot for retry\n", httpCode);
      snprintf(messageBuffer, MESSAGE_BUFFER_SIZE, "Cloud sync failed (HTTP %d)", httpCode);
      queueConsoleMessage(messageBuffer);
      sensorRingInFlightIndex = -1;  // allow re-queue next tick
    }
    lastUploadWasBuffered = false;
  } else if (lastUploadWasBuffered && httpCode != 200) {
    // Buffered flag set but no in-flight ring slot (shouldn't normally happen).
    snprintf(messageBuffer, MESSAGE_BUFFER_SIZE, "Cloud sync failed (HTTP %d)", httpCode);
    queueConsoleMessage(messageBuffer);
    lastUploadWasBuffered = false;
  }

  return (httpCode == 200);
}

// ─────────────────────────────────────────────────────────────────────────────
// PSRAM sensor-snapshot ring (replaces the old LittleFS /buffer/*.json layout).
// One slot per completed SENSOR_UPLOAD_INTERVAL window. Push is microseconds;
// no flash I/O, no Core 1 stall. Drain to Supabase happens from the cloud-
// feature block in loop() under the same field-off gate as uploadBufferedRecords.
// On overflow, the oldest unread slot is dropped.
// ─────────────────────────────────────────────────────────────────────────────
inline bool ringIsFull()  { return sensorRingCount >= SENSOR_RING_SIZE; }
inline bool ringIsEmpty() { return sensorRingCount == 0; }

// Push current window snapshot. Drops oldest if ring is full (unless the
// oldest slot is currently being uploaded — then drop the new sample instead
// so we don't yank the rug out from Core 0 mid-upload).
void pushSensorSnapshot(time_t collectionTime) {
  if (!sensorRing) return;
  // Index RMWs under sensorRingMux (Core 0 pops concurrently). The big struct
  // copy below stays OUTSIDE the lock — the head slot is invisible to Core 0
  // until the count++ publish at the end.
  bool dropNewest = false;
  portENTER_CRITICAL(&sensorRingMux);
  if (ringIsFull()) {
    if (sensorRingInFlightIndex == (int32_t)sensorRingTail) {
      // Oldest slot is being uploaded; drop the NEW snapshot instead.
      dropNewest = true;
    } else {
      // Drop oldest to make room.
      sensorRingTail = (sensorRingTail + 1) % SENSOR_RING_SIZE;
      sensorRingCount--;
    }
  }
  portEXIT_CRITICAL(&sensorRingMux);
  if (dropNewest) {
    Serial.println("sensorRing full + tail in-flight; dropping newest snapshot");
    return;
  }
  sensorRing[sensorRingHead].collectionTime = (int64_t)collectionTime;
  sensorRing[sensorRingHead].window = *currentWindow;  // struct copy from PSRAM to PSRAM
  // Freeze the IMU subset (caller resets imuWindow right after, so we capture
  // the closing window's values here before they get clobbered).
  ImuSnapshot &is = sensorRing[sensorRingHead].imu;
  is.heel_min                  = imuWindow->heel_min;
  is.heel_max                  = imuWindow->heel_max;
  is.heel_area_v_us            = imuWindow->heel_area_v_us;
  is.heel_valid_us             = imuWindow->heel_valid_us;
  is.pitch_min                 = imuWindow->pitch_min;
  is.pitch_max                 = imuWindow->pitch_max;
  is.pitch_area_v_us           = imuWindow->pitch_area_v_us;
  is.pitch_valid_us            = imuWindow->pitch_valid_us;
  is.vertical_accel_min        = imuWindow->vertical_accel_min;
  is.vertical_accel_max        = imuWindow->vertical_accel_max;
  is.vertical_accel_area_v_us  = imuWindow->vertical_accel_area_v_us;
  is.vertical_accel_valid_us   = imuWindow->vertical_accel_valid_us;
  is.total_accel_min           = imuWindow->total_accel_min;
  is.total_accel_max           = imuWindow->total_accel_max;
  is.total_accel_area_v_us     = imuWindow->total_accel_area_v_us;
  is.total_accel_valid_us      = imuWindow->total_accel_valid_us;
  is.msi_score                 = imu_msi_score;
  is.vomit_pct                 = imu_vomit_pct;
  is.anchorage_comfort         = imu_anchorage_comfort;
  is.wave_period               = imuWindow->wave_period;
  is.slam_count                = imuWindow->slam_count;
  is.slam_peak_max             = imuWindow->slam_peak_max;
  // Charge stage at window-roll time (cloud upload is deferred minutes/hours, so
  // we can't read live state at JSON-build time). Matches the LT-ring capture.
  sensorRing[sensorRingHead].chargeStage = getChargeStageDisplayCode();
  portENTER_CRITICAL(&sensorRingMux);
  sensorRingHead = (sensorRingHead + 1) % SENSOR_RING_SIZE;
  sensorRingCount++;
  bufferedRecordCount = sensorRingCount;  // dashboard mirror
  portEXIT_CRITICAL(&sensorRingMux);
  // Re-arm the "all uploaded" console message — count just went non-zero so
  // the next time we drain to empty, we want to announce it once.
  extern volatile bool sensorRingAnnouncedEmpty;
  sensorRingAnnouncedEmpty = false;
}

// Peek at oldest unread snapshot without removing it. Caller passes a pointer
// to a SensorSnapshot it'll copy into. Returns false if ring is empty or busy.
bool peekTailSnapshot(SensorSnapshot *out) {
  if (!sensorRing || ringIsEmpty() || !out) return false;
  *out = sensorRing[sensorRingTail];
  return true;
}

// Pop oldest. Caller must have already successfully consumed it (e.g. queued
// for upload and Core 0 confirmed 200 OK).
void popTailSnapshot() {
  if (!sensorRing) return;
  // Runs on Core 0 (httpsTask) against Core 1's push — empty-check and RMW
  // must be one atomic unit or a lost decrement wraps count to 65535.
  portENTER_CRITICAL(&sensorRingMux);
  if (ringIsEmpty()) {
    portEXIT_CRITICAL(&sensorRingMux);
    return;
  }
  sensorRingTail = (sensorRingTail + 1) % SENSOR_RING_SIZE;
  sensorRingCount--;
  bufferedRecordCount = sensorRingCount;  // dashboard mirror
  portEXIT_CRITICAL(&sensorRingMux);
}

// Binary-format file used by Phase 3 (shutdown dump + boot restore) so the
// PSRAM ring survives a power-cycle when WiFi/cloud couldn't drain everything
// during the 30-min ignition-off window.
#define SENSOR_RING_BACKUP_PATH  "/sensor_ring_backup.bin"
#define SENSOR_RING_BACKUP_MAGIC 0x53524258u  // 'SRBX'
#define SENSOR_RING_BACKUP_VER   3u  // v3: + chargeStage byte (LT-plot cloud-stitch field)

struct SensorRingBackupHeader {
  uint32_t magic;
  uint32_t version;
  uint32_t count;
  uint32_t entrySize;  // sizeof(SensorSnapshot) — sanity guard against struct layout change
};

// One-shot bulk dump of the PSRAM ring to LittleFS. Called from shutdown
// Phase 4 when the 30-min cloud drain window expired with records remaining.
// Returns number of snapshots written, or 0 if ring was empty / on error.
// Blocking flash write is acceptable here — the field is already off and the
// device is about to sleep.
uint16_t dumpSensorRingToLittleFS() {
  if (!sensorRing || ringIsEmpty()) return 0;
  fsTakeLock();
  File f = LittleFS.open(SENSOR_RING_BACKUP_PATH, "w");
  if (!f) {
    fsReleaseLock();
    Serial.println("dumpSensorRingToLittleFS: open failed");
    return 0;
  }
  SensorRingBackupHeader hdr = {
    SENSOR_RING_BACKUP_MAGIC,
    SENSOR_RING_BACKUP_VER,
    (uint32_t)sensorRingCount,
    (uint32_t)sizeof(SensorSnapshot),
  };
  size_t hdrWritten = f.write((const uint8_t *)&hdr, sizeof(hdr));
  if (hdrWritten != sizeof(hdr)) {
    f.close();
    LittleFS.remove(SENSOR_RING_BACKUP_PATH);
    fsReleaseLock();
    Serial.println("dumpSensorRingToLittleFS: header write failed");
    return 0;
  }
  uint16_t written = 0;
  uint16_t idx = sensorRingTail;
  for (uint16_t i = 0; i < sensorRingCount; i++) {
    size_t n = f.write((const uint8_t *)&sensorRing[idx], sizeof(SensorSnapshot));
    if (n != sizeof(SensorSnapshot)) {
      Serial.printf("dumpSensorRingToLittleFS: short write at entry %u\n", i);
      break;
    }
    written++;
    idx = (idx + 1) % SENSOR_RING_SIZE;
    esp_task_wdt_reset();
  }
  f.close();
  fsReleaseLock();
  Serial.printf("dumpSensorRingToLittleFS: wrote %u snapshots\n", written);
  return written;
}

// Boot-time restore: read the LittleFS backup file (if present) and re-push
// each snapshot into the empty PSRAM ring. Delete the file regardless of
// success — a partial restore is acceptable, but leaving a stale backup
// around would cause double-uploads on the next boot.
// Returns count restored. Safe to call when no backup file exists.
uint16_t restoreSensorRingFromLittleFS() {
  if (!sensorRing) return 0;
  if (!fsExists(SENSOR_RING_BACKUP_PATH)) return 0;
  fsTakeLock();
  File f = LittleFS.open(SENSOR_RING_BACKUP_PATH, "r");
  if (!f) {
    fsReleaseLock();
    Serial.println("restoreSensorRingFromLittleFS: open failed");
    return 0;
  }
  SensorRingBackupHeader hdr;
  if (f.readBytes((char *)&hdr, sizeof(hdr)) != sizeof(hdr)) {
    f.close();
    LittleFS.remove(SENSOR_RING_BACKUP_PATH);
    fsReleaseLock();
    Serial.println("restoreSensorRingFromLittleFS: short header read, file removed");
    return 0;
  }
  if (hdr.magic != SENSOR_RING_BACKUP_MAGIC
      || hdr.version != SENSOR_RING_BACKUP_VER
      || hdr.entrySize != sizeof(SensorSnapshot)) {
    f.close();
    LittleFS.remove(SENSOR_RING_BACKUP_PATH);
    fsReleaseLock();
    Serial.printf("restoreSensorRingFromLittleFS: header mismatch (magic=%08x ver=%u sz=%u), discarding\n",
                  hdr.magic, hdr.version, hdr.entrySize);
    return 0;
  }
  uint16_t restored = 0;
  uint32_t toRestore = (hdr.count > SENSOR_RING_SIZE) ? SENSOR_RING_SIZE : hdr.count;
  SensorSnapshot tmp;
  for (uint32_t i = 0; i < toRestore; i++) {
    if (f.readBytes((char *)&tmp, sizeof(tmp)) != sizeof(tmp)) {
      Serial.printf("restoreSensorRingFromLittleFS: short read at entry %u\n", i);
      break;
    }
    // Push directly into the ring (bypass pushSensorSnapshot to avoid the
    // currentWindow copy — we have the full snapshot already).
    sensorRing[sensorRingHead] = tmp;
    sensorRingHead = (sensorRingHead + 1) % SENSOR_RING_SIZE;
    if (sensorRingCount < SENSOR_RING_SIZE) sensorRingCount++;
    restored++;
    esp_task_wdt_reset();
  }
  bufferedRecordCount = sensorRingCount;
  f.close();
  LittleFS.remove(SENSOR_RING_BACKUP_PATH);
  fsReleaseLock();
  Serial.printf("restoreSensorRingFromLittleFS: restored %u snapshots\n", restored);
  return restored;
}

// Snapshot current sensor window into the PSRAM ring. Replaces the old
// LittleFS write path; this is microseconds and does not stall Core 1.
// Actual upload to Supabase happens later from the cloud-feature block
// (gated by field-off settle, or forced by the "Upload Now" button).
// Rescale a SensorWindow ×100 value to a long-term record's target scale (divisor =
// 100/targetScale: 1 for ×100, 10 for ×10, 100 for ×1) and clamp to int16.
static inline int16_t ltScale(int32_t v100, int32_t divisor) {
  int32_t s = v100 / divisor;
  if (s > 32767) s = 32767;
  if (s < -32768) s = -32768;
  return (int16_t)s;
}

// Derive one LongTermRecord from the closing SensorWindow + ImuWindow and push it
// into the month-long PSRAM ring. Called every SENSOR_UPLOAD_INTERVAL from
// uploadSensorHistory() (both paths) so the fixed-cadence timeline never gaps within
// a session. validMask bit per field (set only when that field had valid data):
//   ENVELOPE 0..13: battVolt battCurr altCurr victronCurr rpm duty altTemp tempTherm
//                   sog tws vmg aws heel pitch
//   AVG-ONLY 14..22: soc baro ambTemp cog heading awa twa leeway altZero
//   POSITION 23: lat/lon. (Mirror this order in the dashboard JS.)
// envelope (min,max,avg) from currentWindow; avg = area/valid, then rescaled+clamped.
#define LT_ENV(dst, field, divisor, bit) do { \
  if (currentWindow->field##_valid_us > 0) { \
    int32_t _a = (int32_t)(currentWindow->field##_area_v_us / (int64_t)currentWindow->field##_valid_us); \
    rec.dst[0] = ltScale(currentWindow->field##_min, divisor); \
    rec.dst[1] = ltScale(currentWindow->field##_max, divisor); \
    rec.dst[2] = ltScale(_a, divisor); \
    rec.validMask |= (1u << (bit)); \
  } } while (0)
#define LT_AVG(dst, field, divisor, bit) do { \
  if (currentWindow->field##_valid_us > 0) { \
    int32_t _a = (int32_t)(currentWindow->field##_area_v_us / (int64_t)currentWindow->field##_valid_us); \
    rec.dst = ltScale(_a, divisor); \
    rec.validMask |= (1u << (bit)); \
  } } while (0)
#define LT_IMU(dst, field, divisor, bit) do { \
  if (imuWindow && imuWindow->field##_valid_us > 0) { \
    int32_t _a = (int32_t)(imuWindow->field##_area_v_us / (int64_t)imuWindow->field##_valid_us); \
    rec.dst[0] = ltScale(imuWindow->field##_min, divisor); \
    rec.dst[1] = ltScale(imuWindow->field##_max, divisor); \
    rec.dst[2] = ltScale(_a, divisor); \
    rec.validMask |= (1u << (bit)); \
  } } while (0)
void pushLongTermRecord() {
  if (!longTermRing) return;
  LongTermRecord rec = {};  // zero-init: absent fields stay 0 with validMask bit clear

  // Real epoch of THIS record (0 if unsynced) — drives the legit time axis + gap detection.
  uint32_t nowEpoch = (timeIsSynced && timeBase > 0)
                        ? (uint32_t)(timeBase + (millis() - timeBaseMillis) / 1000) : 0;
  rec.timestamp = nowEpoch;

  LT_ENV(battVolt,    battVolt,    1,  0);
  LT_ENV(battCurr,    battCurr,    10, 1);
  LT_ENV(altCurr,     altCurr,     10, 2);
  LT_ENV(victronCurr, victronCurr, 10, 3);
  LT_ENV(rpm,         rpm,         100,4);
  LT_ENV(duty,        dutyCycle,   1,  5);
  LT_ENV(altTemp,     altTemp,     10, 6);
  LT_ENV(tempTherm,   tempTherm,   10, 7);
  LT_ENV(sog,         sog,         1,  8);
  LT_ENV(tws,         tws,         1,  9);
  LT_ENV(vmg,         vmg,         1,  10);
  LT_ENV(aws,         aws,         1,  11);
  LT_ENV(awa,         awa,         10, 12);   // moved avg-only → envelope (SensorWindow awa ×100 → ×10)
  LT_ENV(twa,         twa,         10, 13);   // moved avg-only → envelope
  LT_IMU(heel,        heel,        1,  14);
  LT_IMU(pitch,       pitch,       1,  15);

  LT_AVG(soc_avg,     soc,     10, 16);
  LT_AVG(baro_avg,    baro,    10, 17);
  LT_AVG(ambTemp_avg, ambTemp, 10, 18);
  LT_AVG(cog_avg,     cog,     10, 19);
  LT_AVG(heading_avg, heading, 10, 20);
  LT_AVG(leeway_avg,  leeway,  10, 21);
  LT_AVG(altZero_avg, altZero, 1,  22);

  // Position: smoothed lat/lon (deg ×1e5), valid only when GPS isn't stale.
  if (!IS_STALE(IDX_LATITUDE_NMEA) && !IS_STALE(IDX_LONGITUDE_NMEA)) {
    rec.lat_avg = (int32_t)(currentWindow->lat_current * 100000.0);
    rec.lon_avg = (int32_t)(currentWindow->lon_current * 100000.0);
    rec.validMask |= (1u << 23);
  }
  rec.chargeStage = getChargeStageDisplayCode();

  longTermRing[longTermHead] = rec;
  longTermHead = (longTermHead + 1) % LONGTERM_RING_SIZE;
  if (longTermCount < LONGTERM_RING_SIZE) longTermCount++;
  // Header anchor for the .bin (newest record's epoch); unchanged when unsynced.
  if (nowEpoch) longTermLastEpoch = nowEpoch;
}
#undef LT_ENV
#undef LT_AVG
#undef LT_IMU

void uploadSensorHistory() {
  if (otaInProgress) return;

  // Sim mode (HardwarePresent=0): live display only — no long-term ring, no cloud
  // buffering. Still roll the window so it doesn't accumulate fake data unbounded.
  if (hardwarePresent != 1) {
    resetSensorWindow();
    resetAccelWindow();
    return;
  }

  // Long-term plot record fires every interval regardless of charging state (so the
  // fixed-cadence timeline stays gap-free); reads the closing window before reset.
  pushLongTermRecord();

  if (currentWindow->battVolt_valid_us == 0) {  // no data collected this window
    resetSensorWindow();
    resetAccelWindow();
    return;
  }

  // Stamp collection time NOW (still reflects when the window closed even if
  // upload is deferred minutes/hours).
  time_t collectionTime = computeCollectionTime();
  pushSensorSnapshot(collectionTime);

  resetSensorWindow();
  resetAccelWindow();
}

// Long-term plot ring persistence via the Phase-0 scaffold. Durable month-long
// cache (NOT drained like the sensor ring), so restore keeps the file
// (deleteAfter=false) and each dump overwrites it. lastEpoch rides in the scaffold
// userWord (uint32 → good to 2106). Field-off only — called on the field-off-settled
// edge + at shutdown.
void dumpLongTermRing() {
  if (!longTermRing || longTermCount == 0) return;
  uint16_t startIdx = (longTermCount < LONGTERM_RING_SIZE) ? 0 : longTermHead;  // oldest record
  uint32_t n = writePsramBlob(LONGTERM_BACKUP_PATH, LONGTERM_BACKUP_MAGIC, LONGTERM_BACKUP_VER,
                              (uint32_t)longTermLastEpoch, longTermRing, sizeof(LongTermRecord),
                              LONGTERM_RING_SIZE, startIdx, longTermCount);
  if (n > 0) prev_longTermHead = longTermHead;  // mark dumped so the edge won't re-write unchanged
  Serial.printf("dumpLongTermRing: wrote %u records\n", (unsigned)n);
}

// Boot restore. Unwraps the stored ring into linear order (tail=0, head=count) and
// keeps the file as the durable copy. No-op if absent / layout-mismatched.
void restoreLongTermRing() {
  if (!longTermRing) return;
  uint32_t epochU32 = 0;
  uint32_t n = readPsramBlob(LONGTERM_BACKUP_PATH, LONGTERM_BACKUP_MAGIC, LONGTERM_BACKUP_VER,
                             longTermRing, sizeof(LongTermRecord), LONGTERM_RING_SIZE,
                             &epochU32, false);
  if (n == 0) return;
  longTermCount = (uint16_t)n;
  longTermHead = (n >= LONGTERM_RING_SIZE) ? 0 : (uint16_t)n;
  longTermLastEpoch = (time_t)epochU32;
  prev_longTermHead = longTermHead;
  Serial.printf("restoreLongTermRing: restored %u records\n", (unsigned)n);
}

// Dashboard "Clear" button handler. Empties the PSRAM ring and removes the
// LittleFS shutdown-dump file so nothing comes back on next boot.
void clearSensorBuffer() {
  // Runs on the async web task — third context touching the ring indices.
  portENTER_CRITICAL(&sensorRingMux);
  sensorRingHead = 0;
  sensorRingTail = 0;
  sensorRingCount = 0;
  sensorRingInFlightIndex = -1;
  sensorRingAnnouncedEmpty = false;
  bufferedRecordCount = 0;
  portEXIT_CRITICAL(&sensorRingMux);

  fsTakeLock();
  if (fsExists(SENSOR_RING_BACKUP_PATH)) {
    LittleFS.remove(SENSOR_RING_BACKUP_PATH);
  }
  fsReleaseLock();

  Serial.println("Cleared sensor ring + shutdown backup");
}

// ============================================
// ESP32 CONFIG SNAPSHOT FUNCTIONS
// ============================================
// Build config snapshot JSON payload.
// Shape: { device_uid, token, snapshot_timestamp, settings: {…161 fields…}, state: {…28 fields…} }
// Settings populate device_settings_snapshots (owner-visible only, never leaderboards).
// State populates device_state_daily + UPSERTs lifetime fields into device_statistics.
// Field names and grouping mirror configsnapshot_picker.html (locked 2026-05-27).
// All checked-box fields included; "Not in HTML — JS-driven" picker notes are noted but
// the firmware variables still exist and are emitted normally.
bool buildConfigPayload() {
  time_t now_ts = time(NULL);
  const char *timestampStr = formatTimestamp(now_ts);

  int offset = snprintf(configPayloadBuffer, CONFIG_PAYLOAD_SIZE,
    "{\"device_uid\":\"%s\",\"token\":\"%s\",\"snapshot_timestamp\":\"%s\","
    // payload_v = ingest payload schema version; bump when this body's shape changes.
    // Edge fn destructures named keys so it ignores this; present for version tracing.
    "\"payload_v\":1,"
    "\"settings\":{",
    device_id_hex, authToken.c_str(), timestampStr);
  if (offset < 0 || offset >= CONFIG_PAYLOAD_SIZE) return false;

  // ─── Settings ───────────────────────────────────────────────────────────────

  // Electrical sizing & sensor topology
  offset += snprintf(configPayloadBuffer + offset, CONFIG_PAYLOAD_SIZE - offset,
    "\"BatteryCapacity_Ah\":%d,\"AlternatorNominalAmps\":%d,\"SolarWatts\":%d,"
    "\"AmpSensorRange\":%d,\"ShuntResistanceMicroOhm\":%d,"
    "\"BatteryCurrentSource\":%d,"
    "\"InvertAltAmps\":%d,\"InvertBattAmps\":%d,\"hardwarePresent\":%d",
    BatteryCapacity_Ah, AlternatorNominalAmps, SolarWatts,
    AmpSensorRange, ShuntResistanceMicroOhm,
    BatteryCurrentSource,
    InvertAltAmps, InvertBattAmps, hardwarePresent);

  // Thermistor / temperature config
  offset += snprintf(configPayloadBuffer + offset, CONFIG_PAYLOAD_SIZE - offset,
    ",\"TempSource\":%d,\"R_fixed\":%.2f,\"Beta\":%.2f,\"T0_C\":%.2f,"
    "\"WindingTempOffset\":%.2f,\"displayTempUnit\":%d",
    TempSource, R_fixed, Beta, T0_C, WindingTempOffset, displayTempUnit);

  // Mechanical
  offset += snprintf(configPayloadBuffer + offset, CONFIG_PAYLOAD_SIZE - offset,
    ",\"FieldResistance\":%.2f,\"PulleyRatio\":%.3f,\"RPMScalingFactor\":%d,"
    "\"SwitchingFrequency\":%.0f",
    FieldResistance, PulleyRatio, RPMScalingFactor, SwitchingFrequency);

  // Charge profile — voltage targets & stage timers
  offset += snprintf(configPayloadBuffer + offset, CONFIG_PAYLOAD_SIZE - offset,
    ",\"BulkVoltage\":%.2f,\"AbsorptionVoltage\":%.2f,\"FloatVoltage\":%.2f,"
    "\"TargetVoltageMode\":%d,\"TargetVoltageSetpoint\":%.2f,"
    "\"MaintainMode\":%d,\"UseFloat\":%d,"
    "\"absorptionCompleteTime\":%lu,\"AbsorptionTimeoutMs\":%lu,"
    "\"bulkVoltageHoldMs\":%lu,\"FLOAT_DURATION\":%lu,\"MinFloatTime\":%lu,"
    "\"ChargedVoltage\":%.2f,\"ChargedDetectionTime\":%d,"
    "\"TailCurrent\":%.2f,\"TailCurrent_A\":%.2f,\"CurrentThreshold\":%.3f,"
    "\"MaximumAllowedBatteryAmps\":%d,\"MinRPMForField\":%d",
    BulkVoltage, AbsorptionVoltage, FloatVoltage,
    TargetVoltageMode, TargetVoltageSetpoint,
    MaintainMode, UseFloat,
    (unsigned long)absorptionCompleteTime, (unsigned long)AbsorptionTimeoutMs,
    (unsigned long)bulkVoltageHoldMs, (unsigned long)FLOAT_DURATION, (unsigned long)MinFloatTime,
    ChargedVoltage_Scaled / 100.0, ChargedDetectionTime,
    TailCurrent, TailCurrent_A, CurrentThreshold,
    MaximumAllowedBatteryAmps, MinRPMForField);

  // Rebulk logic
  offset += snprintf(configPayloadBuffer + offset, CONFIG_PAYLOAD_SIZE - offset,
    ",\"RebulkVoltage\":%.2f,\"RebulkCurrent_A\":%.2f,\"rebulkDebounceTime\":%lu,"
    "\"SOC_BlockRebulk_percent\":%d,\"SOC_AllowRebulk_percent\":%d",
    RebulkVoltage, RebulkCurrent_A, (unsigned long)rebulkDebounceTime,
    SOC_BlockRebulk_percent, SOC_AllowRebulk_percent);

  // Safety / OV / OC / load dump / temp limits
  offset += snprintf(configPayloadBuffer + offset, CONFIG_PAYLOAD_SIZE - offset,
    ",\"AlternatorHardShutdownV\":%.2f,\"OvGroup1Enable\":%d,\"OvGroup2Enable\":%d,"
    "\"OvMeasMarginV\":%.2f,\"OvPredMarginV\":%.2f,\"HardOCDebounceMs\":%lu,"
    "\"LoadDumpDtThresh\":%.2f,\"LoadDumpDtThresh1\":%.2f,\"LoadDumpDtThresh3\":%.2f,"
    "\"VoltageDisagreeThreshold\":%.2f,\"VoltageDisagreeTimeout\":%lu,"
    "\"TemperatureLimitF\":%.2f,\"TempWarnExcess\":%.2f,\"TempCritExcess\":%.2f,"
    "\"TempSustainedTimeout\":%lu,\"TempAlarm\":%d,\"TempAlarmLow\":%d,"
    "\"VoltageAlarmHigh\":%.2f,\"VoltageAlarmLow\":%.2f,\"CurrentAlarmHigh\":%d,"
    "\"AlarmActivate\":%d,\"AlarmLatchEnabled\":%d,"
    "\"MaxDuty\":%.2f,\"MinDuty\":%.2f,\"MaxTableValue\":%.2f",
    AlternatorHardShutdownV, OvGroup1Enable ? 1 : 0, OvGroup2Enable ? 1 : 0,
    OvMeasMarginV, OvPredMarginV, (unsigned long)HardOCDebounceMs,
    LoadDumpDtThresh, LoadDumpDtThresh1, LoadDumpDtThresh3,
    VoltageDisagreeThreshold, (unsigned long)VoltageDisagreeTimeout,
    TemperatureLimitF, TempWarnExcess, TempCritExcess,
    (unsigned long)TempSustainedTimeout, TempAlarm, TempAlarmLow,
    (float)VoltageAlarmHigh, (float)VoltageAlarmLow, CurrentAlarmHigh,
    AlarmActivate, AlarmLatchEnabled,
    MaxDuty, MinDuty, MaxTableValue);

  // IMU motion thresholds
  offset += snprintf(configPayloadBuffer + offset, CONFIG_PAYLOAD_SIZE - offset,
    ",\"CAPSIZE_THRESHOLD_DEG\":%.2f,\"PITCHPOLE_THRESHOLD_DEG\":%.2f,"
    "\"SLAM_THRESHOLD_G\":%.2f",
    CAPSIZE_THRESHOLD_DEG, PITCHPOLE_THRESHOLD_DEG, SLAM_THRESHOLD_G);

  // Current PID (inner loop)
  offset += snprintf(configPayloadBuffer + offset, CONFIG_PAYLOAD_SIZE - offset,
    ",\"PidKp\":%.4f,\"PidKi\":%.4f,\"PidKd\":%.4f,\"PidSampleDivisor\":%d,"
    "\"PIDTrackingGain\":%.4f,\"InputFilterTC\":%.2f,\"OutputPIDFilterTC\":%.2f,"
    "\"OutputPIDMA_N\":%d,\"OutputPIDSigSrc\":%d",
    PidKp, PidKi, PidKd, PidSampleDivisor,
    PIDTrackingGain, InputFilterTC, OutputPIDFilterTC,
    OutputPIDMA_N, OutputPIDSigSrc);

  // Voltage / CV protection loop
  offset += snprintf(configPayloadBuffer + offset, CONFIG_PAYLOAD_SIZE - offset,
    ",\"VoltageKp\":%.4f,\"VoltageKi\":%.4f,\"VoltageFilterTC\":%.2f,"
    "\"VoltageLoopInterval\":%lu,"
    "\"IExcessK\":%.4f,\"IExcessN\":%d,\"IExcessKBleed\":%.4f,"
    "\"IExcessArmMarginV\":%.2f,\"IExcessMA_N\":%d,\"IExcessSigSrc\":%d,"
    "\"AwBleedRate\":%.4f,\"AwSeedProtectMs\":%lu,"
    "\"FastSetpointRiseRate\":%.2f,\"FastSetpointRiseWindowMs\":%lu,"
    "\"FastSetpointRiseHeadroomV\":%.2f,\"KHard\":%.4f,\"TdPred\":%.4f,"
    "\"ReseedFrac\":%.4f,\"SlopeBleedThresh\":%.4f,\"SlopeBleedK\":%.4f,"
    "\"SlopeBleedProxV\":%.2f,\"DvdtTC\":%.2f",
    VoltageKp, VoltageKi, VoltageFilterTC,
    (unsigned long)VoltageLoopInterval,
    IExcessK, IExcessN, IExcessKBleed,
    IExcessArmMarginV, IExcessMA_N, IExcessSigSrc,
    AwBleedRate, (unsigned long)AwSeedProtectMs,
    FastSetpointRiseRate, (unsigned long)FastSetpointRiseWindowMs,
    FastSetpointRiseHeadroomV, KHard, TdPred,
    ReseedFrac, SlopeBleedThresh, SlopeBleedK,
    SlopeBleedProxV, DvdtTC);

  // Thermal PID
  offset += snprintf(configPayloadBuffer + offset, CONFIG_PAYLOAD_SIZE - offset,
    ",\"TempPIDKp\":%.4f,\"TempPIDKi\":%.4f,\"TempPIDIntervalMs\":%lu,"
    "\"TempPIDFilterAlpha\":%.4f,\"ThermalLookaheadSec\":%.2f",
    TempPIDKp, TempPIDKi, (unsigned long)TempPIDIntervalMs,
    TempPIDFilterAlpha, ThermalLookaheadSec);

  // Setpoint slew / ramp rates
  offset += snprintf(configPayloadBuffer + offset, CONFIG_PAYLOAD_SIZE - offset,
    ",\"SetpointRiseRate\":%.2f,\"SetpointFallRate\":%.2f,"
    "\"StartupRiseRate\":%.2f,\"WarmupRampRate\":%.2f,"
    "\"DutyRampRate\":%.2f,\"DutySlowRampRate\":%.2f,"
    "\"SettleTimeBeforeCut\":%lu,\"ShutdownPhase2HoldMs\":%lu,"
    "\"FIELD_COLLAPSE_DELAY\":%lu,\"FieldAdjustmentInterval\":%.2f",
    SetpointRiseRate, SetpointFallRate,
    StartupRiseRate, WarmupRampRate,
    DutyRampRate, DutySlowRampRate,
    (unsigned long)SettleTimeBeforeCut, (unsigned long)ShutdownPhase2HoldMs,
    (unsigned long)FIELD_COLLAPSE_DELAY, FieldAdjustmentInterval);

  // RPM / fuel / duty lookup tables (6 × 10-entry arrays expanded as JSON arrays)
  offset += snprintf(configPayloadBuffer + offset, CONFIG_PAYLOAD_SIZE - offset,
    ",\"rpmTableRPMPoints\":[%d,%d,%d,%d,%d,%d,%d,%d,%d,%d]",
    rpmTableRPMPoints[0], rpmTableRPMPoints[1], rpmTableRPMPoints[2], rpmTableRPMPoints[3], rpmTableRPMPoints[4],
    rpmTableRPMPoints[5], rpmTableRPMPoints[6], rpmTableRPMPoints[7], rpmTableRPMPoints[8], rpmTableRPMPoints[9]);
  offset += snprintf(configPayloadBuffer + offset, CONFIG_PAYLOAD_SIZE - offset,
    ",\"rpmCapCurrentTable\":[%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f]",
    rpmCapCurrentTable[0], rpmCapCurrentTable[1], rpmCapCurrentTable[2], rpmCapCurrentTable[3], rpmCapCurrentTable[4],
    rpmCapCurrentTable[5], rpmCapCurrentTable[6], rpmCapCurrentTable[7], rpmCapCurrentTable[8], rpmCapCurrentTable[9]);
  offset += snprintf(configPayloadBuffer + offset, CONFIG_PAYLOAD_SIZE - offset,
    ",\"rpmCapKW\":[%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f]",
    rpmCapPowerTable[0], rpmCapPowerTable[1], rpmCapPowerTable[2], rpmCapPowerTable[3], rpmCapPowerTable[4],
    rpmCapPowerTable[5], rpmCapPowerTable[6], rpmCapPowerTable[7], rpmCapPowerTable[8], rpmCapPowerTable[9]);
  offset += snprintf(configPayloadBuffer + offset, CONFIG_PAYLOAD_SIZE - offset,
    ",\"rpmMinDutyTable\":[%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f]",
    rpmMinDutyTable[0], rpmMinDutyTable[1], rpmMinDutyTable[2], rpmMinDutyTable[3], rpmMinDutyTable[4],
    rpmMinDutyTable[5], rpmMinDutyTable[6], rpmMinDutyTable[7], rpmMinDutyTable[8], rpmMinDutyTable[9]);
  offset += snprintf(configPayloadBuffer + offset, CONFIG_PAYLOAD_SIZE - offset,
    ",\"fuelTableRPM\":[%d,%d,%d,%d,%d,%d,%d,%d,%d,%d]",
    fuelTableRPM[0], fuelTableRPM[1], fuelTableRPM[2], fuelTableRPM[3], fuelTableRPM[4],
    fuelTableRPM[5], fuelTableRPM[6], fuelTableRPM[7], fuelTableRPM[8], fuelTableRPM[9]);
  offset += snprintf(configPayloadBuffer + offset, CONFIG_PAYLOAD_SIZE - offset,
    ",\"fuelTableGPH\":[%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f]",
    fuelTableGPH[0], fuelTableGPH[1], fuelTableGPH[2], fuelTableGPH[3], fuelTableGPH[4],
    fuelTableGPH[5], fuelTableGPH[6], fuelTableGPH[7], fuelTableGPH[8], fuelTableGPH[9]);

  // Calibration / auto-zero
  offset += snprintf(configPayloadBuffer + offset, CONFIG_PAYLOAD_SIZE - offset,
    ",\"AlternatorCOffset\":%.2f,\"BatteryCOffset\":%.2f,"
    "\"AutoAltCurrentZero\":%d,\"AutoShuntGainCorrection\":%d",
    AlternatorCOffset, BatteryCOffset, AutoAltCurrentZero, AutoShuntGainCorrection);

  // Battery model — firmware stores scaled ints; descale to real values for the snapshot.
  // PeukertExponent_scaled × 100 (e.g. 105 = 1.05).  ChargeEfficiency_scaled × 10 (e.g. 990 = 99.0%).
  offset += snprintf(configPayloadBuffer + offset, CONFIG_PAYLOAD_SIZE - offset,
    ",\"PeukertExponent\":%.3f,\"ChargeEfficiency\":%.3f",
    PeukertExponent_scaled / 100.0, ChargeEfficiency_scaled / 10.0);

  // Integrations & feature flags
  offset += snprintf(configPayloadBuffer + offset, CONFIG_PAYLOAD_SIZE - offset,
    ",\"VeData\":%d,\"NMEA0183Data\":%d,\"NMEA2KData\":%d,"
    "\"gpsTimeSourceMode\":%d,\"socInfoAvailable\":%d,"
    "\"bmsLogic\":%d,\"bmsLogicLevelOff\":%d,"
    "\"CloudFeatures\":%d,"
    "\"weatherModeEnabled\":%d,\"capLimitMode\":%d",
    VeData, NMEA0183Data, NMEA2KData,
    (int)gpsTimeSourceMode, socInfoAvailable ? 1 : 0,
    bmsLogic, bmsLogicLevelOff,
    CloudFeatures,
    weatherModeEnabled, (int)capLimitMode);

  // (Anomaly-detection config-snapshot fields removed with the old eff matrix.)

  // Weather / location
  offset += snprintf(configPayloadBuffer + offset, CONFIG_PAYLOAD_SIZE - offset,
    ",\"LatitudeNMEA\":%.6f,\"LongitudeNMEA\":%.6f,"
    "\"performanceRatio\":%.3f,\"UVThresholdHigh\":%.2f",
    LatitudeNMEA, LongitudeNMEA, performanceRatio, UVThresholdHigh);

  // ─── State ─────────────────────────────────────────────────────────────────
  offset += snprintf(configPayloadBuffer + offset, CONFIG_PAYLOAD_SIZE - offset, "},\"state\":{");

  // Lifetime accumulators — every field also UPSERTs into device_statistics.
  // eng_hrs / alt_hrs sent as RAW SECONDS (firmware-canonical).
  offset += snprintf(configPayloadBuffer + offset, CONFIG_PAYLOAD_SIZE - offset,
    "\"voltage_avg_lifetime\":%.4f,\"voltage_sample_time\":%lu,"
    "\"soc_avg_lifetime\":%.4f,\"soc_sample_time\":%lu,"
    "\"speed_avg_lifetime\":%.4f,\"speed_sample_time\":%lu,"
    "\"eng_hrs\":%lu,\"alt_hrs\":%lu,\"eng_cycles\":%lu,"
    "\"eng_fuel\":%.3f,\"alt_fuel\":%.3f,\"charge_cycles\":%u,"
    "\"solar_kwh_alltime\":%.3f,\"charged_energy_alltime\":%.3f,"
    "\"discharged_energy_alltime\":%.3f,\"alt_charged_energy_alltime\":%.3f,"
    "\"total_dist_alltime\":%.3f,\"sailing_days_alltime\":%.3f,\"sailing_ratio\":%.3f,"
    "\"total_overheats\":%lu,\"total_safe_hours\":%.3f,"
    "\"longest_single_trip_nm_alltime\":%.3f,\"max_24hr_distance\":%.3f,"
    "\"deepest_anchorage_ft\":%.2f,"
    "\"best_upwind_vmg_alltime\":%.2f,\"longest_gale_duration_hours_alltime\":%.3f,"
    "\"sailing_dist_alltime\":%.3f,\"alt_power_max_alltime_w\":%.3f,\"solar_power_max_alltime_w\":%.3f",
    AvgVoltage_AllTime, (unsigned long)totalVoltageSampleTime_AllTime,
    AvgSOC_AllTime, (unsigned long)totalSocSampleTime_AllTime,
    AvgSpeed_AllTime, (unsigned long)totalSpeedSampleTime_AllTime,
    (unsigned long)EngineRunTime_AllTime, (unsigned long)AlternatorOnTime_AllTime, (unsigned long)EngineCycles_AllTime,
    EngineFuelUsed_AllTime, AlternatorFuelUsed_AllTime, ChargeCycles_AllTime,
    SolarChargedEnergy_AllTime / 1000.0, ChargedEnergy_AllTime / 1000.0,
    DischargedEnergy_AllTime / 1000.0, AlternatorChargedEnergy_AllTime / 1000.0,
    TotalDistance_AllTime, sailing_days_alltime, sailing_ratio,
    (unsigned long)totalOverheats, (double)totalSafeHours,
    LongestSingleTrip_Nm_AllTime, Max24hrDistance_AllTime,
    DeepestAnchorage_Ft_AllTime,
    best_upwind_vmg_alltime, longest_gale_duration_hours_alltime,
    sailing_dist_alltime, alt_power_max_alltime_w, solar_power_max_alltime_w);

  // Session totals (point values at upload time; owner-visible only, no leaderboard)
  offset += snprintf(configPayloadBuffer + offset, CONFIG_PAYLOAD_SIZE - offset,
    ",\"total_dist_session\":%.3f,\"solar_kwh_session\":%.3f,"
    "\"charged_energy_session\":%.3f,\"discharged_energy_session\":%.3f,"
    "\"alt_charged_energy_session\":%.3f",
    TotalDistance, SolarChargedEnergy / 1000.0,
    ChargedEnergy / 1000.0, DischargedEnergy / 1000.0,
    AlternatorChargedEnergy / 1000.0);

  // Slow-changing runtime scalars
  offset += snprintf(configPayloadBuffer + offset, CONFIG_PAYLOAD_SIZE - offset,
    ",\"current_weather_mode\":%d,\"uv_today\":%.2f",
    currentWeatherMode, UVToday);

  // Close state, close root object
  offset += snprintf(configPayloadBuffer + offset, CONFIG_PAYLOAD_SIZE - offset, "}}");

  if (offset >= CONFIG_PAYLOAD_SIZE - 1) {
    Serial.println("ERROR: Config payload truncated");
    return false;
  }
  return true;
}
bool executeUploadConfig(const char *payload) {
  if (WiFi.status() != WL_CONNECTED) return false;
  if (WiFi.RSSI() < -76) return false;
  if (!isRegistered || authToken.isEmpty()) return false;

  WiFiClientSecure client;
  client.setInsecure();
  // setTimeout omitted: Stream::setTimeout is ms (not seconds as some docs claim) and
  // the read loops below use available()+read() polling with explicit millis() deadlines,
  // so the Stream-level timeout doesn't gate anything here.
  client.setHandshakeTimeout(HANDSHAKE_TIMEOUT);

  uint32_t start = millis();
  esp_task_wdt_reset();

  // Serial.println("Config: Connecting...");
  // Hard-bounded TLS connect
  if (!client.connect(host, port, CONNECT_TIMEOUT)) {
    Serial.println("Config: Connect fail");
    client.stop();
    return false;
  }

  // Defensive global timeout check after connect/handshake
  if (millis() - start > GLOBAL_TIMEOUT) {
    Serial.println("Config: Connect exceeded global timeout");
    client.stop();
    return false;
  }

  esp_task_wdt_reset();

  int headerBytes = client.printf(
    "POST /functions/v1/update-config-snapshot HTTP/1.1\r\n"
    "Host: %s\r\n"
    "Content-Type: application/json\r\n"
    "Authorization: Bearer %s\r\n"  // ← Changed from "apikey: %s\r\n"
    "Connection: close\r\n"
    "Content-Length: %u\r\n\r\n",
    host,
    SUPABASE_ANON_KEY,
    (unsigned)strlen(payload));

  // ===== Stream payload directly =====
  size_t payloadLen = strlen(payload);
  size_t sent = client.write((const uint8_t *)payload, payloadLen);
  if (sent != payloadLen) {
    Serial.println("Config: Payload send fail");
    client.stop();
    return false;
  }
  esp_task_wdt_reset();

  // ===== Read status line + drain headers (no String, no header parsing) =====
  int httpCode = 0;
  uint32_t readStart = millis();

  // 1) Read ONLY the first line (status line). Hard deadline.
  char statusBuf[64];
  size_t statusLen = 0;

  while (client.connected() && (millis() - readStart < READ_TIMEOUT)) {
    esp_task_wdt_reset();

    while (client.available()) {
      char c = (char)client.read();

      if (statusLen < sizeof(statusBuf) - 1) {
        statusBuf[statusLen++] = c;
      }

      if (c == '\n') {
        statusBuf[statusLen] = '\0';

        if (strncmp(statusBuf, "HTTP/", 5) == 0) {
          const char *sp = strchr(statusBuf, ' ');
          if (sp) httpCode = atoi(sp + 1);
        }
        goto drain_headers_cfg;
      }
    }

    if (millis() - start > GLOBAL_TIMEOUT) break;
    delay(1);
  }

drain_headers_cfg:
  // 2) Drain headers until CRLFCRLF (end of headers). No logging.
  {
    uint32_t drainStart = millis();
    uint8_t state = 0;  // \r \n \r \n

    while (client.connected() && (millis() - drainStart < READ_TIMEOUT)) {
      esp_task_wdt_reset();

      while (client.available()) {
        char c = (char)client.read();

        if (state == 0 && c == '\r') state = 1;
        else if (state == 1 && c == '\n') state = 2;
        else if (state == 2 && c == '\r') state = 3;
        else if (state == 3 && c == '\n') goto done_headers_cfg;
        else state = 0;
      }

      if (millis() - start > GLOBAL_TIMEOUT) break;
      delay(1);
    }
  }

done_headers_cfg:
  client.stop();

  esp_task_wdt_reset();

  if (httpCode == 0) {
    Serial.println("Config: No response received (timeout)");
    queueConsoleMessage("Config upload failed (timeout)");
    return false;
  }

  //Serial.printf("Config: HTTP %d\n", httpCode); //this was useful debugging

  // ===== Handle response =====
  bool success = (httpCode == 200);
  if (success) {
    queueConsoleMessage("Config snapshot uploaded");
  } else if (httpCode > 0) {
    snprintf(messageBuffer, MESSAGE_BUFFER_SIZE, "Config upload failed HTTP %d", httpCode);
    queueConsoleMessage(messageBuffer);
  }

  return success;
}

// Shared response-body buffer for the two front sync-backs (boat + alt). PSRAM, allocated once and
// kept (both sync paths run sequentially in the same httpsTask worker). Must hold the boat pair at
// full cap (2 × 4096 pts). A read that fills it completely is reported and NOT parsed — a truncated
// front must never wholesale-replace a good local one.
static char *syncBody = nullptr;
#define SYNC_BODY_CAP 524288u
static char *syncBodyGet() {
  if (!syncBody) syncBody = (char *)ps_malloc(SYNC_BODY_CAP);
  return syncBody;
}

// Mirrors executeUploadConfig() exactly (proven HTTPS pattern) — only the endpoint + log labels
// differ. Uploads the boat-performance aggregates to the update-boat-performance edge function.
bool executeUploadBoatPerf(const char *payload) {
  if (WiFi.status() != WL_CONNECTED) return false;
  if (WiFi.RSSI() < -76) return false;
  if (!isRegistered || authToken.isEmpty()) return false;

  WiFiClientSecure client;
  client.setInsecure();
  client.setHandshakeTimeout(HANDSHAKE_TIMEOUT);

  uint32_t start = millis();
  esp_task_wdt_reset();

  if (!client.connect(host, port, CONNECT_TIMEOUT)) {
    Serial.println("BoatPerf: Connect fail");
    client.stop();
    return false;
  }
  if (millis() - start > GLOBAL_TIMEOUT) {
    client.stop();
    return false;
  }
  esp_task_wdt_reset();

  client.printf(
    "POST /functions/v1/update-boat-performance HTTP/1.1\r\n"
    "Host: %s\r\n"
    "Content-Type: application/json\r\n"
    "Authorization: Bearer %s\r\n"
    "Connection: close\r\n"
    "Content-Length: %u\r\n\r\n",
    host, SUPABASE_ANON_KEY, (unsigned)strlen(payload));

  size_t payloadLen = strlen(payload);
  if (client.write((const uint8_t *)payload, payloadLen) != payloadLen) {
    Serial.println("BoatPerf: Payload send fail");
    client.stop();
    return false;
  }
  esp_task_wdt_reset();

  int httpCode = 0;
  uint32_t readStart = millis();
  char statusBuf[64];
  size_t statusLen = 0;
  while (client.connected() && (millis() - readStart < READ_TIMEOUT)) {
    esp_task_wdt_reset();
    while (client.available()) {
      char c = (char)client.read();
      if (statusLen < sizeof(statusBuf) - 1) statusBuf[statusLen++] = c;
      if (c == '\n') {
        statusBuf[statusLen] = '\0';
        if (strncmp(statusBuf, "HTTP/", 5) == 0) {
          const char *sp = strchr(statusBuf, ' ');
          if (sp) httpCode = atoi(sp + 1);
        }
        goto drain_headers_bp;
      }
    }
    if (millis() - start > GLOBAL_TIMEOUT) break;
    delay(1);
  }
drain_headers_bp:
  {
    uint32_t drainStart = millis();
    uint8_t state = 0;
    while (client.connected() && (millis() - drainStart < READ_TIMEOUT)) {
      esp_task_wdt_reset();
      while (client.available()) {
        char c = (char)client.read();
        if (state == 0 && c == '\r') state = 1;
        else if (state == 1 && c == '\n') state = 2;
        else if (state == 2 && c == '\r') state = 3;
        else if (state == 3 && c == '\n') goto done_headers_bp;
        else state = 0;
      }
      if (millis() - start > GLOBAL_TIMEOUT) break;
      delay(1);
    }
  }
done_headers_bp:
  // Read the response BODY (both pruned BEFRONT1 blocks) into the shared PSRAM sync buffer.
  {
    char *bpBody = syncBodyGet();
    if (!bpBody) { client.stop(); return false; }   // PSRAM alloc failed — skip, retry next cycle
    size_t bl = 0;
    uint32_t bodyStart = millis();
    while (client.connected() && bl < SYNC_BODY_CAP - 1 && (millis() - bodyStart < READ_TIMEOUT)) {
      while (client.available() && bl < SYNC_BODY_CAP - 1) bpBody[bl++] = (char)client.read();
      if (millis() - start > GLOBAL_TIMEOUT) break;
      delay(1);
    }
    bpBody[bl] = '\0';
    client.stop();
    esp_task_wdt_reset();

    bool success = (httpCode == 200);
    if (success && bl >= SYNC_BODY_CAP - 1) {
      // Buffer filled completely → the front CSV may be cut mid-stream. Never parse a possibly-
      // truncated front (the ingest replaces the local front wholesale).
      queueConsoleMessage("WARN: boat-perf sync response overran the sync buffer — front NOT updated");
      if (timeIsSynced) lastBoatPerfSyncEpoch = (int64_t)time(NULL);
      perfClearPending();   // upload itself succeeded; only the sync-back is unusable
      return success;
    }
    if (success) {
      if (timeIsSynced) lastBoatPerfSyncEpoch = (int64_t)time(NULL);   // for the "synced N ago" badge
      perfClearPending();   // cloud accepted the batch (raw history) → drop the pending points
      if (perfIngestFrontCsv(bpBody)) {
        // Field-off-gate the flash write: a LittleFS write stalls the flash cache (both cores).
        // If the field re-engaged while this upload was in flight, skip — the in-memory front is
        // already updated, and the field-off Gate-2 edge persists it later (no data loss).
        if (fieldActiveStatus <= 0) boatPerfSave();   // persist the cloud's pruned fronts
        queueConsoleMessage("Boat performance: fronts updated from cloud");
      } else {
        queueConsoleMessage("Boat performance uploaded (no front in response)");
      }
    } else if (httpCode > 0) {
      snprintf(messageBuffer, MESSAGE_BUFFER_SIZE, "BoatPerf upload failed HTTP %d", httpCode);
      queueConsoleMessage(messageBuffer);
    }
    return success;
  }
}

// Alternator (charging-system) health v2 upload — clone of executeUploadBoatPerf. POST the
// best-ever record batch, read the fitted ALTCURVE1 curve from the HTTP response BODY, and
// persist it. Same WiFiClientSecure raw-write + manual-read pattern (low internal RAM).
bool executeUploadAltHealth(const char *payload) {
  if (WiFi.status() != WL_CONNECTED) return false;
  if (WiFi.RSSI() < -76) return false;
  if (!isRegistered || authToken.isEmpty()) return false;

  WiFiClientSecure client;
  client.setInsecure();
  client.setHandshakeTimeout(HANDSHAKE_TIMEOUT);

  uint32_t start = millis();
  esp_task_wdt_reset();

  if (!client.connect(host, port, CONNECT_TIMEOUT)) {
    Serial.println("AltHealth: Connect fail");
    client.stop();
    return false;
  }
  if (millis() - start > GLOBAL_TIMEOUT) {
    client.stop();
    return false;
  }
  esp_task_wdt_reset();

  client.printf(
    "POST /functions/v1/update-alt-health HTTP/1.1\r\n"
    "Host: %s\r\n"
    "Content-Type: application/json\r\n"
    "Authorization: Bearer %s\r\n"
    "Connection: close\r\n"
    "Content-Length: %u\r\n\r\n",
    host, SUPABASE_ANON_KEY, (unsigned)strlen(payload));

  size_t payloadLen = strlen(payload);
  if (client.write((const uint8_t *)payload, payloadLen) != payloadLen) {
    Serial.println("AltHealth: Payload send fail");
    client.stop();
    return false;
  }
  esp_task_wdt_reset();

  int httpCode = 0;
  uint32_t readStart = millis();
  char statusBuf[64];
  size_t statusLen = 0;
  while (client.connected() && (millis() - readStart < READ_TIMEOUT)) {
    esp_task_wdt_reset();
    while (client.available()) {
      char c = (char)client.read();
      if (statusLen < sizeof(statusBuf) - 1) statusBuf[statusLen++] = c;
      if (c == '\n') {
        statusBuf[statusLen] = '\0';
        if (strncmp(statusBuf, "HTTP/", 5) == 0) {
          const char *sp = strchr(statusBuf, ' ');
          if (sp) httpCode = atoi(sp + 1);
        }
        goto drain_headers_ah;
      }
    }
    if (millis() - start > GLOBAL_TIMEOUT) break;
    delay(1);
  }
drain_headers_ah:
  {
    uint32_t drainStart = millis();
    uint8_t state = 0;
    while (client.connected() && (millis() - drainStart < READ_TIMEOUT)) {
      esp_task_wdt_reset();
      while (client.available()) {
        char c = (char)client.read();
        if (state == 0 && c == '\r') state = 1;
        else if (state == 1 && c == '\n') state = 2;
        else if (state == 2 && c == '\r') state = 3;
        else if (state == 3 && c == '\n') goto done_headers_ah;
        else state = 0;
      }
      if (millis() - start > GLOBAL_TIMEOUT) break;
      delay(1);
    }
  }
done_headers_ah:
  // Read the response BODY (the pruned BEFRONT1 front CSV) into the shared PSRAM sync buffer.
  {
    char *ahBody = syncBodyGet();
    if (!ahBody) { client.stop(); return false; }   // PSRAM alloc failed — skip, retry next cycle
    size_t bl = 0;
    uint32_t bodyStart = millis();
    while (client.connected() && bl < SYNC_BODY_CAP - 1 && (millis() - bodyStart < READ_TIMEOUT)) {
      while (client.available() && bl < SYNC_BODY_CAP - 1) ahBody[bl++] = (char)client.read();
      if (millis() - start > GLOBAL_TIMEOUT) break;
      delay(1);
    }
    ahBody[bl] = '\0';
    client.stop();
    esp_task_wdt_reset();

    bool success = (httpCode == 200);
    if (success && bl >= SYNC_BODY_CAP - 1) {
      // Buffer filled completely → the front CSV may be cut mid-stream. Never parse a possibly-
      // truncated front (the ingest replaces the local front wholesale).
      queueConsoleMessage("WARN: alt-health sync response overran the sync buffer — front NOT updated");
      if (timeIsSynced) lastAltHealthSyncEpoch = (int64_t)time(NULL);
      altClearPending();   // upload itself succeeded; only the sync-back is unusable
      return success;
    }
    if (success) {
      if (timeIsSynced) lastAltHealthSyncEpoch = (int64_t)time(NULL);   // for the "synced N ago" badge
      altClearPending();   // cloud accepted the batch (raw history) → drop the pending points
      if (altIngestFrontCsv(ahBody)) {
        // Field-off-gate the flash write: a LittleFS write stalls the flash cache (both cores).
        // If the field re-engaged while this upload was in flight, skip — the in-memory front is
        // already updated, and the field-off Gate-2 edge persists it later (no data loss).
        if (fieldActiveStatus <= 0) altHealthSave();   // persist the cloud's pruned front
        queueConsoleMessage("Alternator health: front updated from cloud");
      } else {
        queueConsoleMessage("Alternator health uploaded (no front in response)");
      }
    } else if (httpCode > 0) {
      snprintf(messageBuffer, MESSAGE_BUFFER_SIZE, "AltHealth upload failed HTTP %d", httpCode);
      queueConsoleMessage(messageBuffer);
    }
    return success;
  }
}

void syncTimeFromNTP() {
  if (otaInProgress) {
    return;  // Skip during OTA — don't arm fast retry; OTA path is brief.
  }
  // Manual mode gate: only AUTO and NTP-forced ever fall through to NTP.
  if (gpsTimeSourceMode == GTS_NMEA || gpsTimeSourceMode == GTS_PHONE) return;
  if (currentMode != MODE_CLIENT || WiFi.status() != WL_CONNECTED) {
    // WiFi not ready yet (typical on cold boot before association). Arm a
    // fast retry so the next checkTimeSync() tick fires ~60s from now
    // instead of waiting the full 12h. Underflow on early-boot millis is
    // intentional and handled by unsigned wraparound in the gate.
    lastTimeSyncAttempt = millis() - (TIME_SYNC_INTERVAL - 60000UL);
    return;
  }

  Serial.println("Starting NTP sync...");
  // Hold core0Busy so RunAlternator will not enable the field while we block
  // on getLocalTime(). Cleared on all exit paths below.
  core0Busy = true;
  esp_task_wdt_reset();

  configTime(0, 0, "pool.ntp.org", "time.nist.gov");

  // Single-shot — no retry loop, no delay(). checkTimeSync() retries on its
  // normal TIME_SYNC_INTERVAL cadence. A retry loop with delay(500) would
  // block Core 1 for up to 10.5s (3×3000ms timeout + 3×500ms) if NTP is down.
  struct tm timeinfo;
  esp_task_wdt_reset();

  if (getLocalTime(&timeinfo, 3000)) {
    timeBase = time(nullptr);
    timeBaseMillis = millis();
    timeIsSynced = true;
    currentTimeSource = TIME_NTP;
    lastTimeSyncAttempt = millis();

    queueConsoleMessage("Time synced from NTP");
    Serial.printf("NTP synced: epoch=%ld\n", timeBase);
    core0Busy = false;
    return;
  }

  Serial.println("NTP sync attempt failed");
  // Arm fast retry — only successful syncs get the full 12h throttle.
  lastTimeSyncAttempt = millis() - (TIME_SYNC_INTERVAL - 60000UL);
  core0Busy = false;
}

bool canUploadNow() {
  if (WiFi.status() != WL_CONNECTED) return false;
  if (WiFi.RSSI() < -76) return false;
  return true;
}