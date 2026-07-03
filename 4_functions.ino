
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

/*
 * KNOWN HEAP EFFICIENCY ISSUE (LOW PRIORITY): the OTA / forced-update HTTPS handlers
 * (checkForSpecificOTAUpdate, executeUpdateFirmwareVersion, executeCheckForcedUpdate)
 * build URLs / headers / JSON with String concatenation, which fragments the heap.
 * Accepted as-is: these run rarely and never in a tight loop. If OTA stability ever
 * suffers, convert them to snprintf into char buffers (see executeFetchWeatherData /
 * buildConfigPayload for the pattern).
 */

void updateCpuLoad() {
  if (!taskArray) return;  // allocated in setup()

  UBaseType_t taskCount = uxTaskGetSystemState(taskArray, MAX_TASKS, NULL);
  if (taskCount == 0) return;

  static uint64_t lastIdle0 = 0, lastIdle1 = 0;
  static uint64_t lastTotal = 0;
  static bool initialized = false;

  uint64_t idle0 = 0, idle1 = 0;
  uint64_t total = 0;

  for (UBaseType_t i = 0; i < taskCount; i++) {
    uint64_t t = (uint64_t)taskArray[i].ulRunTimeCounter;
    total += t;
    if (strcmp(taskArray[i].pcTaskName, "IDLE0") == 0) {
      idle0 = t;
    } else if (strcmp(taskArray[i].pcTaskName, "IDLE1") == 0) {
      idle1 = t;
    }
  }

  if (!initialized) {
    lastIdle0 = idle0;
    lastIdle1 = idle1;
    lastTotal = total;
    initialized = true;
    return;
  }

  uint64_t deltaIdle0 = idle0 - lastIdle0;
  uint64_t deltaIdle1 = idle1 - lastIdle1;
  uint64_t deltaTotal = total - lastTotal;

  if (deltaTotal == 0) return;

  uint32_t idlePct0 = (uint32_t)((deltaIdle0 * 200ULL) / deltaTotal);
  uint32_t idlePct1 = (uint32_t)((deltaIdle1 * 200ULL) / deltaTotal);

  if (idlePct0 > 100U) idlePct0 = 100U;
  if (idlePct1 > 100U) idlePct1 = 100U;

  cpuLoadCore0 = 100 - (int)idlePct0;
  cpuLoadCore1 = 100 - (int)idlePct1;

  if (cpuLoadCore0 < 0) cpuLoadCore0 = 0;
  if (cpuLoadCore0 > 100) cpuLoadCore0 = 100;
  if (cpuLoadCore1 < 0) cpuLoadCore1 = 0;
  if (cpuLoadCore1 > 100) cpuLoadCore1 = 100;

  if (cpuLoadCore0 > cpuLoadCore0Max) cpuLoadCore0Max = cpuLoadCore0;
  if (cpuLoadCore1 > cpuLoadCore1Max) cpuLoadCore1Max = cpuLoadCore1;

  lastIdle0 = idle0;
  lastIdle1 = idle1;
  lastTotal = total;
}

// Core-0 sampler for updateCpuLoad() — the uxTaskGetSystemState scan (~4ms) is too heavy for the
// Core-1 control loop. Runs at priority 0, so it only takes Core 0 when no network/upload task
// needs it. Writes cpuLoadCore0/1(+Max), read by the CSV2 sender on Core 1.
void cpuLoadTask(void *param) {
  for (;;) {
    vTaskDelay(pdMS_TO_TICKS(2000));
    updateCpuLoad();
  }
}

void updateSystemHealthStats() {
  if (otaInProgress) return;

  // Only the 4s heap walk; CPU-load sampling runs in cpuLoadTask on Core 0.
  static unsigned long lastHeapSample = 0;

  unsigned long now = millis();

  if (now - lastHeapSample < 4000) return;
  if (gHeavyRanThisPass) return;  // another heavy ran this pass — defer (timer unchanged → still due)
  gHeavyRanThisPass = true;
  lastHeapSample = now;

  {
    FreeHeap = esp_get_free_heap_size() / 1024;
    MinFreeHeap = esp_get_minimum_free_heap_size() / 1024;
    FreeInternalRam = heap_caps_get_free_size(MALLOC_CAP_INTERNAL) / 1024;
    TotalInternalRam = heap_caps_get_total_size(MALLOC_CAP_INTERNAL) / 1024;
    LargestInternalBlock = heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL) / 1024;
    FreePSRAM = heap_caps_get_free_size(MALLOC_CAP_SPIRAM) / 1024;
    TotalPSRAM = heap_caps_get_total_size(MALLOC_CAP_SPIRAM) / 1024;
    Heapfrag = (FreeInternalRam > 0)
                 ? 100 - (LargestInternalBlock * 100 / FreeInternalRam)
                 : 100;

    // Deliberately no low-contiguous-RAM console warning: every HTTPS handshake briefly
    // dips largest-block under 34 KB and recovers, so it would re-fire constantly.
    // The heap figures above surface it live in the ESP32 Stats panel.
  }
}

bool executeFetchWeatherData() {
  // Called by HTTPS task on Core 0
  Serial.println(">>> executeFetchWeatherData() ENTERED");
  if (LatitudeNMEA == 0.0 && LongitudeNMEA == 0.0) {
    snprintf(weatherLastError, sizeof(weatherLastError), "No GPS coordinates available");
    weatherDataValid = 0;
    queueConsoleMessageF("Weather: No GPS coordinates");
    return false;
  }
  if (currentMode != MODE_CLIENT) {
    snprintf(weatherLastError, sizeof(weatherLastError), "Not in client mode");
    weatherDataValid = 0;
    return false;
  }

  if (WiFi.status() != WL_CONNECTED) {
    snprintf(weatherLastError, sizeof(weatherLastError), "WiFi not connected");
    weatherDataValid = 0;
    return false;
  }
  WiFiClientSecure client;
  client.setInsecure();

  HTTPClient http;

  char url[256];
  snprintf(url, sizeof(url),
           "https://api.open-meteo.com/v1/forecast?latitude=%.6f&longitude=%.6f&daily=shortwave_radiation_sum&timezone=auto",
           LatitudeNMEA, LongitudeNMEA);

  Serial.print("Open-Meteo URL: ");
  Serial.println(url);

  http.begin(client, url);
  http.setTimeout(WeatherTimeoutMs);

  int httpResponseCode = http.GET();
  weatherHttpResponseCode = httpResponseCode;

  if (httpResponseCode > 0) {
    String payload = http.getString();
    queueConsoleMessageF("GPS: %.6f,%.6f", LatitudeNMEA, LongitudeNMEA);
    Serial.printf("Response Code: %d\n", httpResponseCode);

    DynamicJsonDocument doc(4096);
    DeserializationError error = deserializeJson(doc, payload);
    if (error) {
      snprintf(weatherLastError, sizeof(weatherLastError), "JSON parse error: %s", error.c_str());
      weatherDataValid = 0;
      Serial.printf("JSON error: %s\n", error.c_str());
      http.end();
      return false;
    }
    JsonArray radiation = doc["daily"]["shortwave_radiation_sum"];
    if (radiation.size() < 3) {
      snprintf(weatherLastError, sizeof(weatherLastError), "Insufficient forecast data");
      weatherDataValid = 0;
      http.end();
      return false;
    }
    float mjToday = radiation[0];
    float mjTomorrow = radiation[1];
    float mjDay2 = radiation[2];
    pKwHrToday = (mjToday * MJ_TO_KWH_CONVERSION / STC_IRRADIANCE) * SolarWatts * performanceRatio;
    pKwHrTomorrow = (mjTomorrow * MJ_TO_KWH_CONVERSION / STC_IRRADIANCE) * SolarWatts * performanceRatio;
    pKwHr2days = (mjDay2 * MJ_TO_KWH_CONVERSION / STC_IRRADIANCE) * SolarWatts * performanceRatio;
    UVToday = mjToday * MJ_TO_KWH_CONVERSION;
    UVTomorrow = mjTomorrow * MJ_TO_KWH_CONVERSION;
    UVDay2 = mjDay2 * MJ_TO_KWH_CONVERSION;
    weatherDataValid = 1;
    weatherLastError[0] = '\0';
    nextWeatherUpdate = millis() + 3600000;
    Serial.printf("Solar forecast: Today=%.1f, Tomorrow=%.1f, Day2=%.1f kWh\n",
                  pKwHrToday, pKwHrTomorrow, pKwHr2days);
    queueConsoleMessageF("Weather updated: %.1f kWh today", pKwHrToday);


    http.end();
    return true;
  } else {
    snprintf(weatherLastError, sizeof(weatherLastError), "HTTP error: %d", httpResponseCode);
    weatherDataValid = 0;
    Serial.printf("HTTP error: %d\n", httpResponseCode);
    http.end();
    return false;
  }
}

void analyzeWeatherMode() {
  if (otaInProgress) {
    return;
  }
  //If 2 or more days have a UV index above the configured threshold, it sets the mode to high UV mode (1), which disables the alternator.
  if (!weatherDataValid || !weatherModeEnabled) {
    currentWeatherMode = 0;
    return;
  }
  int highUVDays = 0;
  if (pKwHrToday >= UVThresholdHigh) highUVDays++;
  if (pKwHrTomorrow >= UVThresholdHigh) highUVDays++;
  if (pKwHr2days >= UVThresholdHigh) highUVDays++;

  if (highUVDays >= 2) {
    currentWeatherMode = 1;  // Disable alternator
  } else {
    currentWeatherMode = 0;  // Normal operation
  }
}
void updateWeatherMode() {
  if (otaInProgress) {
    return;
  }
  if (!weatherModeEnabled) {
    currentWeatherMode = 0;
    return;
  }

  unsigned long now = millis();

  if (weatherDataValid && (now - weatherLastUpdate < WeatherUpdateInterval)) {
    analyzeWeatherMode();
    return;
  }

  // Internet fetch requires field off for 75s. Do not advance nextWeatherUpdate
  // while blocked — it fires promptly once the gate opens.
  if (!fieldOffSettled(15000)) {
    if (weatherDataValid) analyzeWeatherMode();
    return;
  }

  // Signed delta survives the 49.7-day millis() rollover
  if ((int32_t)(now - nextWeatherUpdate) >= 0) {
    HttpsRequest req = { .type = HTTPS_FETCH_WEATHER };
    if (xQueueSend(httpsQueue, &req, 0) == pdTRUE) {
      nextWeatherUpdate = now + WeatherUpdateInterval;
    } else {
      nextWeatherUpdate = now + 2000;
    }
  }
  if (weatherDataValid) {
    analyzeWeatherMode();
  }
}

void initWeatherModeSettings() {
  if (!settingExists(NK_LatitudeNMEA)) {
    settingWrite(NK_LatitudeNMEA, "0.0");
  } else {
    LatitudeNMEA = settingRead(NK_LatitudeNMEA).toDouble();
  }
  if (!settingExists(NK_LongitudeNMEA)) {
    settingWrite(NK_LongitudeNMEA, "0.0");
  } else {
    LongitudeNMEA = settingRead(NK_LongitudeNMEA).toDouble();
  }
  // Sticky manual GPS override (Weather Mode). Restored across reboot.
  if (!settingExists(NK_LatitudeManual)) {
    settingWrite(NK_LatitudeManual, "0.0");
  } else {
    LatitudeManual = settingRead(NK_LatitudeManual).toDouble();
  }
  if (!settingExists(NK_LongitudeManual)) {
    settingWrite(NK_LongitudeManual, "0.0");
  } else {
    LongitudeManual = settingRead(NK_LongitudeManual).toDouble();
  }
  if (!settingExists(NK_gpsManualActive)) {
    settingWrite(NK_gpsManualActive, "0");
  } else {
    gpsManualActive = (settingRead(NK_gpsManualActive).toInt() == 1);
  }
  if (gpsManualActive) {  // apply immediately so weather has coords before first resolveSources()
    LatitudeNMEA  = LatitudeManual;
    LongitudeNMEA = LongitudeManual;
  }
  if (!settingExists(NK_weatherModeEnabled)) {
    settingWrite(NK_weatherModeEnabled, String(weatherModeEnabled).c_str());
  } else {
    weatherModeEnabled = settingRead(NK_weatherModeEnabled).toInt();
  }
  if (!settingExists(NK_UVThresholdHigh)) {
    settingWrite(NK_UVThresholdHigh, String(UVThresholdHigh, 1).c_str());
  } else {
    UVThresholdHigh = settingRead(NK_UVThresholdHigh).toFloat();
  }
  if (!settingExists(NK_performanceRatio)) {
    settingWrite(NK_performanceRatio, String(performanceRatio, 2).c_str());
  } else {
    performanceRatio = settingRead(NK_performanceRatio).toFloat();
  }
  if (!settingExists(NK_SolarWatts)) {
    settingWrite(NK_SolarWatts, String(SolarWatts).c_str());
  } else {
    SolarWatts = settingRead(NK_SolarWatts).toInt();
  }
  if (!settingExists(NK_weatherDataValid)) {
    settingWrite(NK_weatherDataValid, "0");
  } else {
    weatherDataValid = settingRead(NK_weatherDataValid).toInt();
  }
  if (!settingExists(NK_WeatherUpdateInterval)) {
    settingWrite(NK_WeatherUpdateInterval, String(WeatherUpdateInterval).c_str());
  } else {
    WeatherUpdateInterval = settingRead(NK_WeatherUpdateInterval).toInt();
  }
  if (!settingExists(NK_WeatherTimeoutMs)) {
    settingWrite(NK_WeatherTimeoutMs, String(WeatherTimeoutMs).c_str());
  } else {
    WeatherTimeoutMs = settingRead(NK_WeatherTimeoutMs).toInt();
  }
}


void triggerWeatherUpdate() {
  if (LatitudeNMEA == 0.0 && LongitudeNMEA == 0.0) {
    queueConsoleMessageF("Weather: Failed - No GPS lock");
    return;
  }

  if (WiFi.RSSI() < -76) {
    queueConsoleMessageF("Weather: Failed - Weak WiFi signal");
    return;
  }

  if (currentMode != MODE_CLIENT) {
    queueConsoleMessageF("Weather: Failed - Not in client mode");
    return;
  }

  HttpsRequest req = { .type = HTTPS_FETCH_WEATHER };
  if (xQueueSend(httpsQueue, &req, 0) == pdTRUE) {
    queueConsoleMessageF("Weather: Update queued");
    nextWeatherUpdate = millis() + WeatherUpdateInterval;
  } else {
    queueConsoleMessageF("Weather: Failed - Queue full, retry in 1s");
  }
}

void printTempDebugStatus() {
  unsigned long age = millis() - tempLastSuccessMillis;
  Serial.printf(
    "TempDbg: lastGood=%.2fF age=%lums ok=%lu readFail=%lu crcFail=%lu crcRec=%lu allFF=%lu 85C=%lu oor=%lu reqFail=%lu connFail=%lu resFix=%lu rereadFail=%lu resFixCrcFail=%lu enumFail=%lu coreBusySkip=%lu intervalSkip=%lu\n",
    tempLastGoodF, age,
    (unsigned long)tempReadSuccessCount,
    (unsigned long)tempReadFailCount,
    (unsigned long)tempCrcFailCount,
    (unsigned long)tempCrcRecoveredCount,
    (unsigned long)tempAllFFCount,
    (unsigned long)tempPowerOn85Count,
    (unsigned long)tempOutOfRangeCount,
    (unsigned long)tempRequestFailCount,
    (unsigned long)tempConnectedFailCount,
    (unsigned long)tempResolutionFixCount,
    (unsigned long)tempRereadFailCount,
    (unsigned long)tempResolutionFixCrcFailCount,
    (unsigned long)tempEnumerateFailCount,
    (unsigned long)tempCoreBusySkipCount,
    (unsigned long)tempStaleSkipCount);
}


void checkTempTaskHealth() {
  if (otaInProgress) {
    return;
  }
  if (tempTaskSuspended) return;  // Temp task intentionally suspended in 80MHz engine-off idle — not a hang
  static unsigned long lastTempHealthCheck = 0;
  unsigned long now = millis();

  if (now - lastTempHealthCheck < 5000) return;
  lastTempHealthCheck = now;

  if (now - lastTempTaskHeartbeat > TEMP_TASK_TIMEOUT) {
    if (tempTaskHealthy) {  // First time detecting the problem
      tempTaskHealthy = false;
      tempTaskAlarm = true;
      queueConsoleMessageF("CRITICAL: TempTask hung up - task not responding for %lu seconds", (now - lastTempTaskHeartbeat) / 1000);

      // Field cut is handled by the regulation loop: tempDataVeryStale fires at 20s of no
      // MARK_FRESH call, which produces REASON_TEMP_STALE → MODE_CRITICAL_RAMP → GPIO4 cut.
    }
  } else {
    if (!tempTaskHealthy) {  // Was unhealthy, now recovered
      tempTaskHealthy = true;
      tempTaskAlarm = false;
      queueConsoleMessage("TempTask: Recovered and responding normally");
      // Field re-enable is automatic: regulation loop re-enables GPIO4 as soon as
      // tempDataVeryStale clears, which happens on the first MARK_FRESH after a good read.
    }
  }
}
bool writeINA228Register(uint8_t i2cAddress, uint8_t reg, uint16_t value) {
  Wire.beginTransmission(i2cAddress);
  Wire.write(reg);
  Wire.write(value >> 8);
  Wire.write(value & 0xFF);
  return (Wire.endTransmission() == 0);
}


// ============================================================================
// IMU Initialization
// ============================================================================
bool i2cProbe8bit(uint8_t addr8) {
  uint8_t addr7 = addr8 >> 1;
  Wire.beginTransmission(addr7);
  uint8_t err = Wire.endTransmission(true);
  return (err == 0);
}
void imuInit() {
  Serial.println("Initializing LSM6DSOX IMU...");
  Serial.print("Using I2C address: 0x");
  Serial.print(LSM6DSOX_ADDR, HEX);
  Serial.println(" (8-bit ST format)");
  Serial.print("IMUSample size: ");
  Serial.print(sizeof(IMUSample));
  Serial.println(" bytes");

  if (!i2cProbe8bit(LSM6DSOX_ADDR)) {
    Serial.println("ERROR: No I2C ACK from LSM6DSOX address - sensor not present");
    Serial.println("IMU disabled - will retry on next boot");
    imuEnabled = false;
    return;
  }
  Serial.println("I2C ACK detected at LSM6DSOX address");

  uint8_t who = 0;
  Wire.beginTransmission(LSM6DSOX_ADDR >> 1);
  Wire.write(0x0F);  // WHO_AM_I register
  Wire.endTransmission(false);
  Wire.requestFrom((int)(LSM6DSOX_ADDR >> 1), 1);
  if (Wire.available()) {
    who = Wire.read();
  }

  Serial.print("LSM6DSOX WHO_AM_I = 0x");
  Serial.println(who, HEX);

  if (who != 0x6C) {
    Serial.print("ERROR: WHO_AM_I mismatch (expected 0x6C, got 0x");
    Serial.print(who, HEX);
    Serial.println(") - not an LSM6DSOX");
    Serial.println("IMU disabled - check hardware");
    imuEnabled = false;
    return;
  }
  Serial.println("WHO_AM_I verified - LSM6DSOX detected");

  if (imu.begin() != LSM6DSOX_OK) {
    Serial.println("ERROR: LSM6DSOX library init failed");
    queueConsoleMessageF("IMU library init failed");
    imuEnabled = false;
    return;
  }

  if (imu.Enable_X() != LSM6DSOX_OK) {
    Serial.println("ERROR: Failed to enable accelerometer");
    queueConsoleMessageF("IMU: failed to enable accelerometer");
    imuEnabled = false;
    return;
  }

  if (imu.Enable_G() != LSM6DSOX_OK) {
    Serial.println("ERROR: Failed to enable gyroscope");
    queueConsoleMessageF("IMU: failed to enable gyroscope");
    imuEnabled = false;
    return;
  }

  // Set ODRs.
  // Accel at 104 Hz is plenty for slam peak capture (~9.6 ms sample spacing vs 20–50 ms
  // typical slam pulse), pitch/heel max, wave-period decimation (10 Hz output), and MSI
  // frequency-weighted Z (peaks 0.5–5 Hz). Engine-vibration diagnostics are out of scope
  // (board is not engine-mounted). ACCEL_INTERVAL_US in drainIMUFifo() must match: 9615 µs.
  if (imu.Set_X_ODR(104.0f) != LSM6DSOX_OK) {
    Serial.println("ERROR: Failed to set accel ODR");
    queueConsoleMessageF("IMU: failed to set accel ODR");
    imuEnabled = false;
    return;
  }

  if (imu.Set_G_ODR(52.0f) != LSM6DSOX_OK) {
    Serial.println("ERROR: Failed to set gyro ODR");
    queueConsoleMessageF("IMU: failed to set gyro ODR");
    imuEnabled = false;
    return;
  }

  // Set FIFO batching rates (BDR) - must match ODRs for continuous sampling
  if (imu.Set_FIFO_X_BDR(104.0f) != LSM6DSOX_OK) {
    Serial.println("ERROR: Failed to set FIFO accel BDR");
    queueConsoleMessageF("IMU: failed to set FIFO accel BDR");
    imuEnabled = false;
    return;
  }

  if (imu.Set_FIFO_G_BDR(52.0f) != LSM6DSOX_OK) {
    Serial.println("ERROR: Failed to set FIFO gyro BDR");
    queueConsoleMessageF("IMU: failed to set FIFO gyro BDR");
    imuEnabled = false;
    return;
  }


  // COMPRESSION LATER
  // Left OFF: the FIFO parser only decodes uncompressed tags, so enabling compression routes
  // every compressed sample to the unknown-tag path and stalls the drain. Needs delta-decode first.
  // // Enable FIFO compression (bonus headroom, don't rely on it)
  // if (imu.Set_FIFO_Compression_Algo_Init(1) != LSM6DSOX_OK || imu.Set_FIFO_Compression_Algo_Enable(1) != LSM6DSOX_OK || imu.Set_FIFO_Compression_Algo_Real_Time_Set(1) != LSM6DSOX_OK) {
  //   Serial.println("WARNING: Failed to enable FIFO compression (continuing anyway)");
  //   // Non-fatal - continue without compression
  // }

  // Set FIFO to continuous mode (mode = 6)
  if (imu.Set_FIFO_Mode(6) != LSM6DSOX_OK) {
    Serial.println("ERROR: Failed to set FIFO continuous mode");
    queueConsoleMessageF("IMU: failed to set FIFO mode");
    imuEnabled = false;
    return;
  }

  delay(50);  // Let FIFO stabilize

  // Empirical tag verification (one-time, informational only)
  Serial.println("Verifying FIFO tags...");
  delay(100);  // Let some samples accumulate
  uint16_t test_samples = 0;
  if (imu.Get_FIFO_Num_Samples(&test_samples) == LSM6DSOX_OK && test_samples > 0) {
    uint8_t testBuf[21];  // Read 3 samples to see tag variety
    uint16_t samples_to_check = (test_samples > 3) ? 3 : test_samples;
    if (imu.Get_FIFO_Sample(testBuf, samples_to_check) == LSM6DSOX_OK) {
      for (uint16_t i = 0; i < samples_to_check; i++) {
        uint8_t raw_tag = testBuf[i * 7];
        uint8_t tag_sensor = raw_tag >> 3;
        Serial.print("  Sample ");
        Serial.print(i);
        Serial.print(": raw_tag=0x");
        Serial.print(raw_tag, HEX);
        Serial.print(", tag_sensor=");
        Serial.print(tag_sensor);
        Serial.print(" (");
        if (tag_sensor == TAG_SENSOR_GYRO) {
          Serial.print("GYRO");
        } else if (tag_sensor == TAG_SENSOR_ACCEL) {
          Serial.print("ACCEL");
        } else if (tag_sensor == TAG_SENSOR_TEMP) {
          Serial.print("TEMP");
        } else {
          Serial.print("UNKNOWN");
        }
        Serial.println(")");
      }
    }
  }

  Serial.println("LSM6DSOX initialized successfully");
  Serial.println("  Accel ODR: 104 Hz, FIFO BDR: 104 Hz");
  Serial.println("  Gyro ODR: 52 Hz, FIFO BDR: 52 Hz");
  Serial.println("  FIFO mode: Continuous");
  Serial.print("  Poll interval: ");
  Serial.print(IMU_POLL_INTERVAL);
  Serial.print(" ms, Max drain: ");
  Serial.print(MAX_FIFO_DRAIN_PER_POLL);
  Serial.println(" samples");
  Serial.print("  Ring buffers: Accel ");
  Serial.print(ACCEL_RING_SIZE);
  Serial.print(" samples, Gyro ");
  Serial.print(GYRO_RING_SIZE);
  Serial.println(" samples");

  queueConsoleMessageF("IMU initialized: 104Hz accel, 52Hz gyro");

  imuEnabled = true;
  lastIMUPoll = millis();
}
void initIMUStructures() {
  memset(imuWindow, 0, sizeof(ImuWindow));

  // Initialize min/max to sentinel values (will be overwritten by first real data)
  imuWindow->accel_x_min = imuWindow->accel_y_min = imuWindow->accel_z_min = INT32_MAX;
  imuWindow->accel_x_max = imuWindow->accel_y_max = imuWindow->accel_z_max = INT32_MIN;
  imuWindow->total_accel_min = imuWindow->vertical_accel_min = INT32_MAX;
  imuWindow->total_accel_max = imuWindow->vertical_accel_max = INT32_MIN;
  imuWindow->gyro_x_min = imuWindow->gyro_y_min = imuWindow->gyro_z_min = INT32_MAX;
  imuWindow->gyro_x_max = imuWindow->gyro_y_max = imuWindow->gyro_z_max = INT32_MIN;
  imuWindow->heel_min = imuWindow->pitch_min = INT32_MAX;
  imuWindow->heel_max = imuWindow->pitch_max = INT32_MIN;

  imuWindow->lastUpdateTime_us = 0;
  imuWindow->lastGyroUpdateTime_us = 0;

  imuWindow->wave_period = -1000;  // -1.0s scaled = invalid

  Serial.println("IMU structures initialized");
}
void initializeHardware() {

  Serial.println("Starting hardware initialization...");
  Wire.end();
  Wire.begin(9, 10);
  Wire.setClock(400000);  // in-spec Fast-mode. Do not raise to 800 kHz: ADS1115 was declared disconnected there (adsI2CErrorCount tripwire).
  Wire.setTimeOut(15);   // do not lower — AsyncTCP preemption can stall I2C transactions this long
  delay(100);
  Serial.println("I2C initialized on SDA=9, SCL=10");
  delay(100);  // Give I2C time to initialize

//BMP390
if (!bmp388.begin(BMP3_ADDR)) {
  Serial.println("BMP388 not found");
  while (1);
}
bmp388.setPresOversampling(OVERSAMPLING_X32);
bmp388.setTempOversampling(OVERSAMPLING_X2);
bmp388.setIIRFilter(IIR_FILTER_32);   // use highest exposed by this library
Serial.println("BMP388 found");

  // NMEA2K
  OutputStream = &Serial;
  //  NMEA2000.SetN2kCANReceiveFrameBufSize(50); // was commented
  // Do not forward bus messages at all
  NMEA2000.SetForwardType(tNMEA2000::fwdt_Text);
  NMEA2000.SetForwardStream(OutputStream);
  // Set false below, if you do not want to see messages parsed to HEX withing library
  NMEA2000.EnableForward(false);
  NMEA2000.SetMsgHandler(HandleNMEA2000Msg);
  //  NMEA2000.SetN2kCANMsgBufSize(2);
  NMEA2000.Open();
  Serial.println("NMEA2K Running...");


  //Victron VeDirect and NMEA0183
  Serial1.begin(19200, SERIAL_8N1, 7, -1, 1);  // Victron VEDirect
  Serial2.begin(19200, SERIAL_8N1, 6, -1, 0);  // ... note the "0" at end for normal logic.  This is combined NMEA0183 data from YachtDevices
  Serial2.flush();              

  // INA228 Battery Voltage/Current Sensor
  if (!INA.begin()) {
    Serial.println("Could not connect INA228. Fix and Reboot");
    queueConsoleMessage("WARNING: Could not connect INA228 Battery Voltage/Amp measuring chip");
    INADisconnected = 1;
  } else {
    INADisconnected = 0;

    // setAverage() takes raw register value: 0=1, 1=4, 2=16, 3=64, 4=128, 5=256, 6=512, 7=1024 samples
    INA.setMode(11);                       // Continuous shunt and bus voltage measurement
    INA.setAverage(4);                     // 128-sample averaging — 128 × 8.24ms = 1054ms register update
    INA.setBusVoltageConversionTime(7);    // 4120 µs conversion time
    INA.setShuntVoltageConversionTime(7);  // 4120 µs conversion time

    updateINA228OvervoltageThreshold();
    queueConsoleMessage("INA228 initialized: Hardware overvoltage protection enabled");
  }

  unsigned long now = millis();
  for (int i = 0; i < MAX_DATA_INDICES; i++) {
    dataTimestamps[i] = now;
  }

  // if (setupDisplay()) {
  //   Serial.println("Display ready for use");
  // } else {
  //   Serial.println("Continuing without display");
  // }

  //ADS1115
  if (!adc.testConnection()) {
    Serial.println("ADS1115 Connection failed and would have triggered a return if it wasn't commented out");
    queueConsoleMessage("WARNING: ADS1115 Analog Input chip failed");
    ADS1115Disconnected = 1;
    // return;
  } else {
    ADS1115Disconnected = 0;
  }
  adc.setGain(ADS1115_REG_CONFIG_PGA_6_144V);       // ±6.144V range
  adc.setSampleRate(ADS1115_REG_CONFIG_DR_860SPS);  // 860 SPS, 1.16ms per conversion

  delay(100);  // Give chip time to configure
  if (!adc.testConnection()) {
    Serial.println("ADS1115 failed after configuration");
  } else {
    Serial.println("ADS1115 configured successfully");
  }
  Wire.beginTransmission(0x48);  // ADS1115 default address
  Wire.write(0x01);              // Config register
  Wire.endTransmission(false);
  Wire.requestFrom(0x48, 2);
  if (Wire.available() >= 2) {
    uint16_t configReg = (Wire.read() << 8) | Wire.read();
    queueConsoleMessage("ADS1115 Config Register: 0x" + String(configReg, HEX));

    uint8_t mux = (configReg >> 12) & 0x07;
    uint8_t pga = (configReg >> 9) & 0x07;
    uint8_t mode = (configReg >> 8) & 0x01;

    queueConsoleMessage("ADS1115 MUX: " + String(mux) + ", PGA: " + String(pga) + ", Mode: " + String(mode));
    // Expected values:
    // MUX should be 4 (single-ended A0) when you read channel 0
    // PGA should be 0 (±6.144V range)
    // Mode should be 0 (continuous) or 1 (single-shot)
  } else {
    queueConsoleMessage("ADS1115 Config readback failed - I2C error");
  }
  //onewire
  sensors.begin();
  sensors.setWaitForConversion(false);                   // don't block inside requestTemperaturesByAddress()
  sensors.setCheckForConversion(true);                   // enable polling via isConversionComplete()
  sensors.setAutoSaveScratchPad(false);                  // RAM-only config changes (no EEPROM writes)
  sensors.setResolution(tempDeviceAddress, resolution);  // set once at boot
  sensors.getAddress(tempDeviceAddress, 0);
  if (sensors.getDeviceCount() == 0) {
    Serial.println("WARNING: No DS18B20 sensors found on the bus.");
    queueConsoleMessage("WARNING: No DS18B20 sensors found on the bus");
    sensors.setWaitForConversion(false);  // this is critical!
  }
  imuInit();
  initIMUStructures();
  Serial.println("Hardware initialization complete");
}
void InitSystemSettings() {  // load all settings from NVS.  If no keys exist, create them and populate with the hardcoded values
  // (the one-time LittleFS-to-NVS import sweep runs earlier, in setup(), before the alt/perf registry loaders)

  // Load vessel info from LittleFS (stream parse; no malloc)
  if (LittleFS.exists("/vessel_info.json")) {
    File file = LittleFS.open("/vessel_info.json", "r");
    if (file) {
      size_t size = file.size();
      Serial.printf("vessel_info.json size=%u bytes\n", (unsigned)size);

      DynamicJsonDocument doc(4096);
      DeserializationError error = deserializeJson(doc, file);
      file.close();

      if (!error) {
        BOAT_LENGTH_FT = doc["boat_length_ft"] | 0.0f;
        BOAT_DISPLACEMENT_LBS = doc["boat_displacement_lbs"] | 0.0f;

        const char *boat_type = doc["boat_type"] | "monohull";
        BOAT_TYPE = boat_type;

        const char *make_model = doc["boat_make_model"] | "";
        BOAT_MAKE_MODEL = make_model;

        BOAT_YEAR = doc["boat_year"] | 2025;

        if (doc.containsKey("home_port") && !doc["home_port"].isNull()) {
          const char *homePortStr = doc["home_port"] | "";
          strncpy(HOME_PORT, homePortStr, sizeof(HOME_PORT) - 1);
          HOME_PORT[sizeof(HOME_PORT) - 1] = '\0';
        } else {
          HOME_PORT[0] = '\0';
        }

        const char *engine_make = doc["engine_make"] | "";
        ENGINE_MAKE = engine_make;

        ENGINE_HP = doc["engine_hp"] | 0;
        BATTERY_VOLTAGE = doc["battery_voltage"] | 12;
        BatteryCapacity_Ah = doc["battery_capacity_ah"] | 300;

        const char *battery_type = doc["battery_type"] | "lifepo4";
        BATTERY_TYPE = battery_type;

        const char *battery_make_model = doc["battery_make_model"] | "";
        BATTERY_MAKE_MODEL = battery_make_model;

        const char *alt_brand = doc["alternator_brand_model"] | "";
        ALTERNATOR_BRAND_MODEL = alt_brand;

        SolarWatts = doc["solar_watts"] | 0;
        imuMountOrientation = doc["imu_mount_orientation"] | 0;
        IMU_DIST_BOW_FT = doc["imu_dist_bow_ft"] | 0.0f;
        IMU_DIST_CL_FT = doc["imu_dist_cl_ft"] | 0.0f;
        IMU_HEIGHT_WL_FT = doc["imu_height_wl_ft"] | 0.0f;

        Serial.println("Vessel info loaded from LittleFS");
      } else {
        Serial.printf("Vessel info JSON parse failed: %s\n", error.c_str());
      }
    } else {
      Serial.println("Vessel info: open failed");
    }
  } else {
    Serial.println("Vessel info: /vessel_info.json missing");
  }

  // Load IMU zero/level calibration (separate from vessel_info so a profile
  // re-save from the cloud never clobbers physical-mount calibration).
  // Stored as one JSON-string NVS value.
  if (settingExists(NK_imu_zero)) {
    DynamicJsonDocument zdoc(256);
    if (!deserializeJson(zdoc, settingRead(NK_imu_zero))) {
      imuHeelOffsetDeg = zdoc["heel_offset_deg"] | 0.0f;
      imuPitchOffsetDeg = zdoc["pitch_offset_deg"] | 0.0f;
      imuGxBias = zdoc["gx_bias"] | 0.0f;
      imuGyBias = zdoc["gy_bias"] | 0.0f;
      imuGzBias = zdoc["gz_bias"] | 0.0f;
      Serial.println("IMU zero calibration loaded");
    }
  }

  if (!settingExists(NK_InstallId)) {
    settingWrite(NK_InstallId, String((unsigned long)esp_random(), HEX).c_str());
  }
  installId = settingRead(NK_InstallId);

  if (!settingExists(NK_BatteryCapacity_Ah)) {
    settingWrite(NK_BatteryCapacity_Ah, String(BatteryCapacity_Ah).c_str());
  } else {
    BatteryCapacity_Ah = settingRead(NK_BatteryCapacity_Ah).toInt();
  }
  PeukertRatedCurrent_A = BatteryCapacity_Ah / 20.0f;  // derived; recomputed every run regardless of branch
  if (!settingExists(NK_ChargeEfficiency)) {
    // Save user-readable form (e.g. "99.0"), NOT the scaled integer, so the load path always
    // sees a value ≤ 100 and can safely apply × 10 to reconstruct the scaled integer.
    settingWrite(NK_ChargeEfficiency, String(ChargeEfficiency_scaled / 10.0f, 1).c_str());
  } else {
    // Key stores the user-readable percentage string (e.g. "99.0"). Multiply by 10 to get scaled int.
    ChargeEfficiency_scaled = (int)round(settingRead(NK_ChargeEfficiency).toFloat() * 10);
  }
  if (!settingExists(NK_TailCurrent)) {
    settingWrite(NK_TailCurrent, String(TailCurrent).c_str());
  } else {
    TailCurrent = settingRead(NK_TailCurrent).toFloat();
  }
  if (!settingExists(NK_FuelEfficiency)) {
    settingWrite(NK_FuelEfficiency, String(FuelEfficiency_scaled).c_str());
  } else {
    FuelEfficiency_scaled = settingRead(NK_FuelEfficiency).toInt();
  }
  if (!settingExists(NK_TemperatureLimitF)) {
    settingWrite(NK_TemperatureLimitF, String(TemperatureLimitF).c_str());
  } else {
    TemperatureLimitF = settingRead(NK_TemperatureLimitF).toInt();
  }
  if (!settingExists(NK_ManualDutyTarget)) {
    settingWrite(NK_ManualDutyTarget, String(ManualDutyTarget).c_str());
  } else {
    ManualDutyTarget = settingRead(NK_ManualDutyTarget).toInt();
  }
  if (!settingExists(NK_capLimitMode)) {
    settingWrite(NK_capLimitMode, String(capLimitMode).c_str());
  } else {
    capLimitMode = constrain(settingRead(NK_capLimitMode).toInt(), 0, 1);
  }
  // System voltage class (12/24/48) — NVS (NK_BatteryVoltage) is the AUTHORITATIVE source of truth.
  // vessel_info.json (parsed into BATTERY_VOLTAGE above) is only a mirror, used here ONLY to
  // migrate pre-existing devices on the first boot after this key was introduced. Keeping the class in
  // the same store as the rescaled charge profile is what prevents an overvoltage-trip mismatch if
  // LittleFS (formatOnFail) ever loses vessel_info.json. The CV/CC gain normalization
  // (recomputeCv/CcGains, called at the end of this function) divides by BATTERY_VOLTAGE.
  if (!settingExists(NK_BatteryVoltage)) {
    if (BATTERY_VOLTAGE != 12 && BATTERY_VOLTAGE != 24 && BATTERY_VOLTAGE != 48) BATTERY_VOLTAGE = 12;  // validate the mirror before seeding
    settingWrite(NK_BatteryVoltage, String((int)BATTERY_VOLTAGE).c_str());  // migrate: seed NVS from the vessel-JSON mirror
  } else {
    int v = settingRead(NK_BatteryVoltage).toInt();
    if (v != 12 && v != 24 && v != 48) v = 12;  // reject corrupt NVS value (guards vNorm = 12/BATTERY_VOLTAGE div-by-zero/NaN)
    BATTERY_VOLTAGE = (uint8_t)v;  // NVS wins over the vessel-JSON mirror
  }
  if (!settingExists(NK_BulkVoltage)) {
    settingWrite(NK_BulkVoltage, String(BulkVoltage).c_str());
  } else {
    BulkVoltage = settingRead(NK_BulkVoltage).toFloat();
  }
  if (!settingExists(NK_socInfoAvailable)) {
    settingWrite(NK_socInfoAvailable, String(socInfoAvailable).c_str());
  } else {
    socInfoAvailable = settingRead(NK_socInfoAvailable).toInt();
  }
  if (!settingExists(NK_TailCurrent_A)) {
    settingWrite(NK_TailCurrent_A, String(TailCurrent_A).c_str());
  } else {
    TailCurrent_A = settingRead(NK_TailCurrent_A).toFloat();
  }
  if (!settingExists(NK_RebulkVoltage)) {
    settingWrite(NK_RebulkVoltage, String(RebulkVoltage).c_str());
  } else {
    RebulkVoltage = settingRead(NK_RebulkVoltage).toFloat();
  }
  if (!settingExists(NK_rebulkDebounceTime)) {
    settingWrite(NK_rebulkDebounceTime, String(rebulkDebounceTime).c_str());
  } else {
    rebulkDebounceTime = settingRead(NK_rebulkDebounceTime).toInt();
  }
  if (!settingExists(NK_MinFloatTime)) {
    settingWrite(NK_MinFloatTime, String(MinFloatTime).c_str());
  } else {
    MinFloatTime = settingRead(NK_MinFloatTime).toInt();
  }
  if (!settingExists(NK_SOC_BlockRebulk_percent)) {
    settingWrite(NK_SOC_BlockRebulk_percent, String(SOC_BlockRebulk_percent).c_str());
  } else {
    SOC_BlockRebulk_percent = settingRead(NK_SOC_BlockRebulk_percent).toInt();
  }
  if (!settingExists(NK_SOC_AllowRebulk_percent)) {
    settingWrite(NK_SOC_AllowRebulk_percent, String(SOC_AllowRebulk_percent).c_str());
  } else {
    SOC_AllowRebulk_percent = settingRead(NK_SOC_AllowRebulk_percent).toInt();
  }
  if (!settingExists(NK_wavePeriod)) {
    settingWrite(NK_wavePeriod, String(wavePeriod).c_str());
  } else {
    wavePeriod = settingRead(NK_wavePeriod).toInt();
  }

  if (!settingExists(NK_tuningWaveform)) {
    settingWrite(NK_tuningWaveform, String(tuningWaveform).c_str());
  } else {
    tuningWaveform = settingRead(NK_tuningWaveform).toInt();
  }
  if (!settingExists(NK_tuningSineFreq)) {
    settingWrite(NK_tuningSineFreq, String(tuningSineFreq).c_str());
  } else {
    tuningSineFreq = settingRead(NK_tuningSineFreq).toFloat();
  }
  if (!settingExists(NK_tuningSweepStart)) {
    settingWrite(NK_tuningSweepStart, String(tuningSweepStart).c_str());
  } else {
    tuningSweepStart = settingRead(NK_tuningSweepStart).toFloat();
  }
  if (!settingExists(NK_tuningSweepEnd)) {
    settingWrite(NK_tuningSweepEnd, String(tuningSweepEnd).c_str());
  } else {
    tuningSweepEnd = settingRead(NK_tuningSweepEnd).toFloat();
  }
  if (!settingExists(NK_tuningSweepCycles)) {
    settingWrite(NK_tuningSweepCycles, String(tuningSweepCycles).c_str());
  } else {
    tuningSweepCycles = (uint8_t)settingRead(NK_tuningSweepCycles).toInt();
  }

  if (!settingExists(NK_InputFilterTC)) {
      settingWrite(NK_InputFilterTC, String(InputFilterTC).c_str());
    } else {
      InputFilterTC = settingRead(NK_InputFilterTC).toFloat();
    }

    if (!settingExists(NK_SystemIDStepAmplitude)) {
      settingWrite(NK_SystemIDStepAmplitude, String(SystemIDStepAmplitude).c_str());
    } else {
      SystemIDStepAmplitude = settingRead(NK_SystemIDStepAmplitude).toFloat();
    }

    if (!settingExists(NK_systemIDTestType)) {
      settingWrite(NK_systemIDTestType, String(systemIDTestType).c_str());
    } else {
      systemIDTestType = (uint8_t)settingRead(NK_systemIDTestType).toInt();
    }
    if (!settingExists(NK_systemIDSineFreqStart)) {
      settingWrite(NK_systemIDSineFreqStart, String(systemIDSineFreqStart).c_str());
    } else {
      systemIDSineFreqStart = settingRead(NK_systemIDSineFreqStart).toFloat();
    }
    if (!settingExists(NK_systemIDSineFreqEnd)) {
      settingWrite(NK_systemIDSineFreqEnd, String(systemIDSineFreqEnd).c_str());
    } else {
      systemIDSineFreqEnd = settingRead(NK_systemIDSineFreqEnd).toFloat();
    }
    if (!settingExists(NK_sysidPlantTau)) {
      settingWrite(NK_sysidPlantTau, String(systemIDPlantTauMs).c_str());
    } else {
      systemIDPlantTauMs = (uint16_t)settingRead(NK_sysidPlantTau).toInt();
    }
    if (!settingExists(NK_systemIDSineCycles)) {
      settingWrite(NK_systemIDSineCycles, String(systemIDSineCycles).c_str());
    } else {
      systemIDSineCycles = (uint8_t)settingRead(NK_systemIDSineCycles).toInt();
    }
    if (!settingExists(NK_SystemIDStabilizeAmps)) {
      settingWrite(NK_SystemIDStabilizeAmps, String(SystemIDStabilizeAmps).c_str());
    } else {
      SystemIDStabilizeAmps = settingRead(NK_SystemIDStabilizeAmps).toFloat();
    }

  if (!settingExists(NK_SwitchingFrequency)) {
    settingWrite(NK_SwitchingFrequency, String(SwitchingFrequency).c_str());
  } else {
    SwitchingFrequency = settingRead(NK_SwitchingFrequency).toInt();
  }
  if (!settingExists(NK_FloatVoltage)) {
    settingWrite(NK_FloatVoltage, String(FloatVoltage).c_str());
  } else {
    FloatVoltage = settingRead(NK_FloatVoltage).toFloat();
  }
  if (!settingExists(NK_yyMin)) {
    settingWrite(NK_yyMin, String(yyMin).c_str());
  } else {
    yyMin = settingRead(NK_yyMin).toInt();
  }
  if (!settingExists(NK_FieldAdjustmentInterval)) {
    settingWrite(NK_FieldAdjustmentInterval, String(FieldAdjustmentInterval).c_str());
  } else {
    FieldAdjustmentInterval = settingRead(NK_FieldAdjustmentInterval).toFloat();
  }
  if (!settingExists(NK_ManualFieldToggle)) {
    settingWrite(NK_ManualFieldToggle, String(ManualFieldToggle).c_str());
  } else {
    ManualFieldToggle = settingRead(NK_ManualFieldToggle).toInt();
  }
  if (!settingExists(NK_SwitchControlOverride)) {
    settingWrite(NK_SwitchControlOverride, String(SwitchControlOverride).c_str());
  } else {
    SwitchControlOverride = settingRead(NK_SwitchControlOverride).toInt();
  }
  if (!settingExists(NK_IgnitionOverride)) {
    settingWrite(NK_IgnitionOverride, String(IgnitionOverride).c_str());
  } else {
    IgnitionOverride = settingRead(NK_IgnitionOverride).toInt();
  }
  if (!settingExists(NK_hardwarePresent)) {
    settingWrite(NK_hardwarePresent, String(hardwarePresent).c_str());
  } else {
    hardwarePresent = settingRead(NK_hardwarePresent).toInt();
  }
  if (!settingExists(NK_gpsTimeSourceMode)) {
    settingWrite(NK_gpsTimeSourceMode, String(gpsTimeSourceMode).c_str());
  } else {
    gpsTimeSourceMode = (uint8_t)settingRead(NK_gpsTimeSourceMode).toInt();
    if (gpsTimeSourceMode > GTS_NTP) gpsTimeSourceMode = GTS_AUTO;  // sanity
  }
  if (!settingExists(NK_MaintainMode)) {
    settingWrite(NK_MaintainMode, String(MaintainMode).c_str());
  } else {
    MaintainMode = settingRead(NK_MaintainMode).toInt();
  }
  if (!settingExists(NK_TargetVoltageMode)) {
    settingWrite(NK_TargetVoltageMode, String(TargetVoltageMode).c_str());
  } else {
    TargetVoltageMode = settingRead(NK_TargetVoltageMode).toInt();
  }
  if (!settingExists(NK_OnOff)) {
    settingWrite(NK_OnOff, String(OnOff).c_str());
  } else {
    OnOff = settingRead(NK_OnOff).toInt();
  }
  if (!settingExists(NK_HiLow)) {
    settingWrite(NK_HiLow, String(HiLow).c_str());
  } else {
    HiLow = settingRead(NK_HiLow).toInt();
  }
  if (!settingExists(NK_AmpSensorRange)) {
    settingWrite(NK_AmpSensorRange, String(AmpSensorRange).c_str());
  } else {
    AmpSensorRange = settingRead(NK_AmpSensorRange).toInt();
  }
  if (!settingExists(NK_InvertAltAmps)) {
    settingWrite(NK_InvertAltAmps, String(InvertAltAmps).c_str());
  } else {
    InvertAltAmps = settingRead(NK_InvertAltAmps).toInt();
  }
  if (!settingExists(NK_InvertBattAmps)) {
    settingWrite(NK_InvertBattAmps, String(InvertBattAmps).c_str());
  } else {
    InvertBattAmps = settingRead(NK_InvertBattAmps).toInt();
  }
  if (!settingExists(NK_LimpHome)) {
    settingWrite(NK_LimpHome, String(LimpHome).c_str());
  } else {
    LimpHome = settingRead(NK_LimpHome).toInt();
  }
  if (!settingExists(NK_VeData)) {
    settingWrite(NK_VeData, String(VeData).c_str());
  } else {
    VeData = settingRead(NK_VeData).toInt();
  }
  if (!settingExists(NK_NMEA0183Data)) {
    settingWrite(NK_NMEA0183Data, String(NMEA0183Data).c_str());
  } else {
    NMEA0183Data = settingRead(NK_NMEA0183Data).toInt();
  }
  if (!settingExists(NK_NMEA2KData)) {
    settingWrite(NK_NMEA2KData, String(NMEA2KData).c_str());
  } else {
    NMEA2KData = settingRead(NK_NMEA2KData).toInt();
  }
  if (!settingExists(NK_waveAmplitude)) {
    settingWrite(NK_waveAmplitude, String(waveAmplitude).c_str());
  } else {
    waveAmplitude = settingRead(NK_waveAmplitude).toInt();
  }
  if (!settingExists(NK_tuningWaveFloor)) {
    settingWrite(NK_tuningWaveFloor, String(tuningWaveFloor).c_str());
  } else {
    tuningWaveFloor = settingRead(NK_tuningWaveFloor).toInt();
  }
  if (!settingExists(NK_CurrentThreshold)) {
    settingWrite(NK_CurrentThreshold, String(CurrentThreshold).c_str());
  } else {
    CurrentThreshold = settingRead(NK_CurrentThreshold).toFloat();
  }
  if (!settingExists(NK_PeukertExponent)) {
    settingWrite(NK_PeukertExponent, String(PeukertExponent_scaled).c_str());
  } else {
    float pv = settingRead(NK_PeukertExponent).toFloat();
    if (pv <= 2.0f) {
      // old file stored raw user input (e.g. "1.12") — migrate to scaled int
      PeukertExponent_scaled = (int)(pv * 100);
      settingWrite(NK_PeukertExponent, String(PeukertExponent_scaled).c_str());
    } else {
      PeukertExponent_scaled = (int)pv;
    }
  }
  if (!settingExists(NK_ChargedVoltage)) {
    settingWrite(NK_ChargedVoltage, String(ChargedVoltage_Scaled).c_str());
  } else {
    float cv = settingRead(NK_ChargedVoltage).toFloat();
    if (cv <= 70.0f) {
      // old file stored raw user input (e.g. "14.50") — migrate to scaled int
      ChargedVoltage_Scaled = (int)(cv * 100);
      settingWrite(NK_ChargedVoltage, String(ChargedVoltage_Scaled).c_str());
    } else {
      ChargedVoltage_Scaled = (int)cv;
    }
  }
  if (!settingExists(NK_ChargedDetectionTime)) {
    settingWrite(NK_ChargedDetectionTime, String(ChargedDetectionTime).c_str());
  } else {
    ChargedDetectionTime = settingRead(NK_ChargedDetectionTime).toInt();
  }
  if (!settingExists(NK_IgnoreTemperature)) {
    settingWrite(NK_IgnoreTemperature, String(IgnoreTemperature).c_str());
  } else {
    IgnoreTemperature = settingRead(NK_IgnoreTemperature).toInt();
  }
  if (!settingExists(NK_IgnoreRPM)) {
    settingWrite(NK_IgnoreRPM, String(IgnoreRPM).c_str());
  } else {
    IgnoreRPM = settingRead(NK_IgnoreRPM).toInt();
  }
  if (!settingExists(NK_MinRPMForField)) {
    settingWrite(NK_MinRPMForField, String(MinRPMForField).c_str());
  } else {
    MinRPMForField = settingRead(NK_MinRPMForField).toInt();
  }
  if (!settingExists(NK_bmsLogic)) {
    settingWrite(NK_bmsLogic, String(bmsLogic).c_str());
  } else {
    bmsLogic = settingRead(NK_bmsLogic).toInt();
  }
  if (!settingExists(NK_bmsLogicLevelOff)) {
    settingWrite(NK_bmsLogicLevelOff, String(bmsLogicLevelOff).c_str());
  } else {
    bmsLogicLevelOff = settingRead(NK_bmsLogicLevelOff).toInt();
  }
  if (!settingExists(NK_AlarmActivate)) {
    settingWrite(NK_AlarmActivate, String(AlarmActivate).c_str());
  } else {
    AlarmActivate = settingRead(NK_AlarmActivate).toInt();
  }
  if (!settingExists(NK_TempAlarm)) {
    settingWrite(NK_TempAlarm, String(TempAlarm).c_str());
  } else {
    TempAlarm = settingRead(NK_TempAlarm).toInt();
  }
  if (!settingExists(NK_TempAlarmLow)) {
    settingWrite(NK_TempAlarmLow, String(TempAlarmLow).c_str());
  } else {
    TempAlarmLow = settingRead(NK_TempAlarmLow).toInt();
  }
  if (!settingExists(NK_VoltageAlarmHigh)) {
    settingWrite(NK_VoltageAlarmHigh, String(VoltageAlarmHigh, 2).c_str());
  } else {
    VoltageAlarmHigh = settingRead(NK_VoltageAlarmHigh).toFloat();  // pre-float NVS strings ("15") parse fine
  }
  if (!settingExists(NK_VoltageAlarmLow)) {
    settingWrite(NK_VoltageAlarmLow, String(VoltageAlarmLow, 2).c_str());
  } else {
    VoltageAlarmLow = settingRead(NK_VoltageAlarmLow).toFloat();
  }
  if (!settingExists(NK_SocAlarmLow)) {
    settingWrite(NK_SocAlarmLow, String(SocAlarmLow).c_str());
  } else {
    SocAlarmLow = settingRead(NK_SocAlarmLow).toInt();
  }
  if (!settingExists(NK_CurrentAlarmHigh)) {
    settingWrite(NK_CurrentAlarmHigh, String(CurrentAlarmHigh).c_str());
  } else {
    CurrentAlarmHigh = settingRead(NK_CurrentAlarmHigh).toInt();
  }
  if (!settingExists(NK_RPMScalingFactor)) {
    settingWrite(NK_RPMScalingFactor, String(RPMScalingFactor).c_str());
  } else {
    RPMScalingFactor = settingRead(NK_RPMScalingFactor).toInt();
  }
  if (!settingExists(NK_FieldResistance)) {
    settingWrite(NK_FieldResistance, String(FieldResistance).c_str());
  } else {
    FieldResistance = settingRead(NK_FieldResistance).toFloat();
  }
  if (!settingExists(NK_MaximumAllowedBatteryAmps)) {
    settingWrite(NK_MaximumAllowedBatteryAmps, String(MaximumAllowedBatteryAmps).c_str());
  } else {
    MaximumAllowedBatteryAmps = settingRead(NK_MaximumAllowedBatteryAmps).toInt();
  }
  if (!settingExists(NK_LoadDumpDtThresh1)) {
    settingWrite(NK_LoadDumpDtThresh1, String(LoadDumpDtThresh1).c_str());
  } else {
    LoadDumpDtThresh1 = settingRead(NK_LoadDumpDtThresh1).toFloat();
  }
  if (!settingExists(NK_LoadDumpDtThresh)) {
    settingWrite(NK_LoadDumpDtThresh, String(LoadDumpDtThresh).c_str());
  } else {
    LoadDumpDtThresh = settingRead(NK_LoadDumpDtThresh).toFloat();
  }
  if (!settingExists(NK_LoadDumpDtThresh3)) {
    settingWrite(NK_LoadDumpDtThresh3, String(LoadDumpDtThresh3).c_str());
  } else {
    LoadDumpDtThresh3 = settingRead(NK_LoadDumpDtThresh3).toFloat();
  }
  // A live test must NEVER auto-resume after a reboot — force OFF on boot, ignoring any stored value (write back only if it was stuck on).
  CVTuningMode = 0;
  if (settingExists(NK_CVTuningMode) && settingRead(NK_CVTuningMode).toInt() != 0) settingWrite(NK_CVTuningMode, "0");
  if (!settingExists(NK_cvWaveAmplitudeV)) {
    settingWrite(NK_cvWaveAmplitudeV, String(cvWaveAmplitudeV).c_str());
  } else {
    cvWaveAmplitudeV = settingRead(NK_cvWaveAmplitudeV).toFloat();
  }
  if (!settingExists(NK_cvWavePeriodSec)) {
    settingWrite(NK_cvWavePeriodSec, String(cvWavePeriodSec).c_str());
  } else {
    cvWavePeriodSec = settingRead(NK_cvWavePeriodSec).toInt();
  }
  if (!settingExists(NK_cvKOvershoot)) {
    settingWrite(NK_cvKOvershoot, String(cvKOvershoot).c_str());
  } else {
    cvKOvershoot = settingRead(NK_cvKOvershoot).toFloat();
  }
  if (!settingExists(NK_cvConsecutiveReads)) {
    settingWrite(NK_cvConsecutiveReads, String(cvConsecutiveReads).c_str());
  } else {
    cvConsecutiveReads = (uint8_t)settingRead(NK_cvConsecutiveReads).toInt();
  }
  if (!settingExists(NK_ManualSOCPoint)) {
    settingWrite(NK_ManualSOCPoint, String(ManualSOCPoint).c_str());
  } else {
    ManualSOCPoint = settingRead(NK_ManualSOCPoint).toFloat();
  }
  if (!settingExists(NK_ManualLifePercentage)) {  // manual override of alterantor lifetime estimates this is pointless
    settingWrite(NK_ManualLifePercentage, String(ManualLifePercentage).c_str());
  } else {
    ManualLifePercentage = settingRead(NK_ManualLifePercentage).toInt();
  }
  if (!settingExists(NK_ShuntResistanceMicroOhm)) {
    settingWrite(NK_ShuntResistanceMicroOhm, String(ShuntResistanceMicroOhm).c_str());
  } else {
    ShuntResistanceMicroOhm = settingRead(NK_ShuntResistanceMicroOhm).toInt();
  }
  if (!settingExists(NK_maxPoints)) {
    settingWrite(NK_maxPoints, String(maxPoints).c_str());
  } else {
    maxPoints = settingRead(NK_maxPoints).toInt();
  }
  if (!settingExists(NK_Ymin1)) {
    settingWrite(NK_Ymin1, String(Ymin1).c_str());
  } else {
    Ymin1 = settingRead(NK_Ymin1).toInt();
  }
  if (!settingExists(NK_Ymax1)) {
    settingWrite(NK_Ymax1, String(Ymax1).c_str());
  } else {
    Ymax1 = settingRead(NK_Ymax1).toInt();
  }
  if (!settingExists(NK_Ymin2)) {
    settingWrite(NK_Ymin2, String(Ymin2).c_str());
  } else {
    Ymin2 = settingRead(NK_Ymin2).toFloat();
  }
  if (!settingExists(NK_Ymax2)) {
    settingWrite(NK_Ymax2, String(Ymax2).c_str());
  } else {
    Ymax2 = settingRead(NK_Ymax2).toFloat();
  }
  if (!settingExists(NK_Ymin3)) {
    settingWrite(NK_Ymin3, String(Ymin3).c_str());
  } else {
    Ymin3 = settingRead(NK_Ymin3).toInt();
  }
  if (!settingExists(NK_Ymax3)) {
    settingWrite(NK_Ymax3, String(Ymax3).c_str());
  } else {
    Ymax3 = settingRead(NK_Ymax3).toInt();
  }
  if (!settingExists(NK_Ymin4)) {
    settingWrite(NK_Ymin4, String(Ymin4).c_str());
  } else {
    Ymin4 = settingRead(NK_Ymin4).toInt();
  }
  if (!settingExists(NK_Ymax4)) {
    settingWrite(NK_Ymax4, String(Ymax4).c_str());
  } else {
    Ymax4 = settingRead(NK_Ymax4).toInt();
  }
  if (!settingExists(NK_MaxDuty)) {
    // Max Field % is the REAL per-bus field-duty cap. The hardcoded default (99) is a 12V value, so on
    // first creation scale it by ×(12/BATTERY_VOLTAGE) (→ ~50%@24V, ~25%@48V) so worst-case field
    // current never exceeds the 12V case. WYSIWYG: the dashboard box shows the actual cap; user-adjustable.
    MaxDuty = (int)lroundf(MaxDuty * 12.0f / (float)BATTERY_VOLTAGE);
    settingWrite(NK_MaxDuty, String(MaxDuty).c_str());
  } else {
    MaxDuty = settingRead(NK_MaxDuty).toInt();
  }
  if (!settingExists(NK_MinDuty)) {
    settingWrite(NK_MinDuty, String(MinDuty).c_str());
  } else {
    MinDuty = settingRead(NK_MinDuty).toInt();
  }
  if (!settingExists(NK_R_fixed)) {
    settingWrite(NK_R_fixed, String(R_fixed).c_str());
  } else {
    R_fixed = settingRead(NK_R_fixed).toFloat();
  }
  if (!settingExists(NK_Beta)) {
    settingWrite(NK_Beta, String(Beta).c_str());
  } else {
    Beta = settingRead(NK_Beta).toFloat();
  }
  if (!settingExists(NK_T0_C)) {
    settingWrite(NK_T0_C, String(T0_C).c_str());
  } else {
    T0_C = settingRead(NK_T0_C).toFloat();
  }
  if (!settingExists(NK_TempSource)) {
    settingWrite(NK_TempSource, String(TempSource).c_str());
  } else {
    TempSource = settingRead(NK_TempSource).toInt();
  }
  if (!settingExists(NK_AlternatorCOffset)) {
    settingWrite(NK_AlternatorCOffset, String(AlternatorCOffset).c_str());
  } else {
    AlternatorCOffset = settingRead(NK_AlternatorCOffset).toFloat();
  }
  if (!settingExists(NK_BatteryCOffset)) {
    settingWrite(NK_BatteryCOffset, String(BatteryCOffset).c_str());
  } else {
    BatteryCOffset = settingRead(NK_BatteryCOffset).toFloat();
  }
  if (!settingExists(NK_AlarmLatchEnabled)) {
    settingWrite(NK_AlarmLatchEnabled, String(AlarmLatchEnabled).c_str());
  } else {
    AlarmLatchEnabled = settingRead(NK_AlarmLatchEnabled).toInt();
  }
  if (!settingExists(NK_absorptionCompleteTime)) {
    settingWrite(NK_absorptionCompleteTime, String(absorptionCompleteTime).c_str());
  } else {
    absorptionCompleteTime = settingRead(NK_absorptionCompleteTime).toInt();
  }
  if (!settingExists(NK_FLOAT_DURATION)) {
    settingWrite(NK_FLOAT_DURATION, String(FLOAT_DURATION).c_str());
  } else {
    FLOAT_DURATION = settingRead(NK_FLOAT_DURATION).toInt();
  }

  if (!settingExists(NK_RebulkCurrent_A)) {
    settingWrite(NK_RebulkCurrent_A, String(RebulkCurrent_A).c_str());
  } else {
    RebulkCurrent_A = settingRead(NK_RebulkCurrent_A).toFloat();
  }
  if (!settingExists(NK_UseFloat)) {
    settingWrite(NK_UseFloat, String(UseFloat).c_str());
  } else {
    UseFloat = settingRead(NK_UseFloat).toInt();
  }

  if (!settingExists(NK_AutoShuntGainCorrection)) {  // BOOLEAN
    settingWrite(NK_AutoShuntGainCorrection, String(AutoShuntGainCorrection).c_str());
  } else {
    AutoShuntGainCorrection = settingRead(NK_AutoShuntGainCorrection).toInt();
  }
  if (!settingExists(NK_AutoAltCurrentZero)) {  // BOOLEAN
    settingWrite(NK_AutoAltCurrentZero, String(AutoAltCurrentZero).c_str());
  } else {
    AutoAltCurrentZero = settingRead(NK_AutoAltCurrentZero).toInt();
  }
  if (!settingExists(NK_WindingTempOffset)) {
    settingWrite(NK_WindingTempOffset, String(WindingTempOffset, 1).c_str());
  } else {
    WindingTempOffset = settingRead(NK_WindingTempOffset).toFloat();
  }
  if (!settingExists(NK_displayTempUnit)) {
    settingWrite(NK_displayTempUnit, String(displayTempUnit).c_str());
  } else {
    displayTempUnit = (uint8_t)settingRead(NK_displayTempUnit).toInt();
  }
  if (!settingExists(NK_PulleyRatio)) {
    settingWrite(NK_PulleyRatio, String(PulleyRatio, 2).c_str());
  } else {
    PulleyRatio = settingRead(NK_PulleyRatio).toFloat();
  }
  if (!settingExists(NK_BatteryCurrentSource)) {
    settingWrite(NK_BatteryCurrentSource, String(BatteryCurrentSource).c_str());
  } else {
    BatteryCurrentSource = settingRead(NK_BatteryCurrentSource).toInt();
  }

  if (!settingExists(NK_timeAxisModeChanging)) {
    settingWrite(NK_timeAxisModeChanging, String(timeAxisModeChanging).c_str());
  } else {
    timeAxisModeChanging = settingRead(NK_timeAxisModeChanging).toInt();
  }
  if (!settingExists(NK_webgaugesinterval)) {
    settingWrite(NK_webgaugesinterval, String(webgaugesinterval).c_str());
  } else {
    webgaugesinterval = settingRead(NK_webgaugesinterval).toInt();
    webgaugesinterval = constrain(webgaugesinterval, 1, 10000000);
  }
  if (!settingExists(NK_plotTimeWindow)) {
    settingWrite(NK_plotTimeWindow, String(plotTimeWindow).c_str());
  } else {
    plotTimeWindow = settingRead(NK_plotTimeWindow).toInt();
    plotTimeWindow = constrain(plotTimeWindow, 1, 1000000);
  }
  if (!settingExists(NK_IgnoreLearningDuringPenalty)) {
    settingWrite(NK_IgnoreLearningDuringPenalty, String(IgnoreLearningDuringPenalty).c_str());
  } else {
    IgnoreLearningDuringPenalty = settingRead(NK_IgnoreLearningDuringPenalty).toInt();
  }
  if (!settingExists(NK_CloudFeatures)) {
    settingWrite(NK_CloudFeatures, String(CloudFeatures).c_str());
  } else {
    CloudFeatures = settingRead(NK_CloudFeatures).toInt();
  }
  if (!settingExists(NK_EnableAmbientCorrection)) {
    settingWrite(NK_EnableAmbientCorrection, String(EnableAmbientCorrection).c_str());
  } else {
    EnableAmbientCorrection = settingRead(NK_EnableAmbientCorrection).toInt();
  }
  // A live test must NEVER auto-resume after a reboot — force OFF on boot, ignoring any stored value (write back only if it was stuck on).
  TuningMode = 0;
  if (settingExists(NK_TuningMode) && settingRead(NK_TuningMode).toInt() != 0) settingWrite(NK_TuningMode, "0");
  if (!settingExists(NK_LogAllLearningEvents)) {
    settingWrite(NK_LogAllLearningEvents, String(LogAllLearningEvents).c_str());
  } else {
    LogAllLearningEvents = settingRead(NK_LogAllLearningEvents).toInt();
  }
  if (!settingExists(NK_AlternatorNominalAmps)) {
    settingWrite(NK_AlternatorNominalAmps, String(AlternatorNominalAmps).c_str());
  } else {
    AlternatorNominalAmps = settingRead(NK_AlternatorNominalAmps).toInt();
  }
  if (!settingExists(NK_LearningUpStep)) {
    settingWrite(NK_LearningUpStep, String(LearningUpStep, 2).c_str());
  } else {
    LearningUpStep = settingRead(NK_LearningUpStep).toFloat();
  }
  if (!settingExists(NK_LearningDownStep)) {
    settingWrite(NK_LearningDownStep, String(LearningDownStep, 2).c_str());
  } else {
    LearningDownStep = settingRead(NK_LearningDownStep).toFloat();
  }
  if (!settingExists(NK_AmbientTempCorrectionFactor)) {
    settingWrite(NK_AmbientTempCorrectionFactor, String(AmbientTempCorrectionFactor, 2).c_str());
  } else {
    AmbientTempCorrectionFactor = settingRead(NK_AmbientTempCorrectionFactor).toFloat();
  }
  if (!settingExists(NK_xTime)) {
    settingWrite(NK_xTime, String(xTime, 2).c_str());
  } else {
    xTime = settingRead(NK_xTime).toFloat();
  }
  if (!settingExists(NK_MinLearningInterval)) {
    settingWrite(NK_MinLearningInterval, String(MinLearningInterval).c_str());
  } else {
    MinLearningInterval = settingRead(NK_MinLearningInterval).toInt();
  }
  if (!settingExists(NK_SafeOperationThreshold)) {
    settingWrite(NK_SafeOperationThreshold, String(SafeOperationThreshold).c_str());
  } else {
    SafeOperationThreshold = settingRead(NK_SafeOperationThreshold).toInt();
  }
  if (!settingExists(NK_SetpointRiseRate)) {
    settingWrite(NK_SetpointRiseRate, String(SetpointRiseRate, 2).c_str());
  } else {
    SetpointRiseRate = settingRead(NK_SetpointRiseRate).toFloat();
  }
  if (!settingExists(NK_SetpointFallRate)) {
    settingWrite(NK_SetpointFallRate, String(SetpointFallRate, 2).c_str());
  } else {
    SetpointFallRate = settingRead(NK_SetpointFallRate).toFloat();
  }
  if (!settingExists(NK_SetpointBigStepThresh)) {
    settingWrite(NK_SetpointBigStepThresh, String(SetpointBigStepThresh, 2).c_str());
  } else {
    SetpointBigStepThresh = settingRead(NK_SetpointBigStepThresh).toFloat();
  }
  if (!settingExists(NK_SetpointBigStepRiseRate)) {
    settingWrite(NK_SetpointBigStepRiseRate, String(SetpointBigStepRiseRate, 2).c_str());
  } else {
    SetpointBigStepRiseRate = settingRead(NK_SetpointBigStepRiseRate).toFloat();
  }
  if (!settingExists(NK_StartupRiseRate)) {
    settingWrite(NK_StartupRiseRate, String(StartupRiseRate, 2).c_str());
  } else {
    StartupRiseRate = settingRead(NK_StartupRiseRate).toFloat();
  }
  if (!settingExists(NK_PIDTrackingGain)) {
    settingWrite(NK_PIDTrackingGain, String(PIDTrackingGain, 2).c_str());
  } else {
    PIDTrackingGain = settingRead(NK_PIDTrackingGain).toFloat();
  }
  if (!settingExists(NK_AbsorptionVoltage)) {
    settingWrite(NK_AbsorptionVoltage, String(AbsorptionVoltage).c_str());
  } else {
    AbsorptionVoltage = settingRead(NK_AbsorptionVoltage).toFloat();
  }
  if (!settingExists(NK_TargetVoltageSetpoint)) {
    settingWrite(NK_TargetVoltageSetpoint, String(TargetVoltageSetpoint).c_str());
  } else {
    TargetVoltageSetpoint = settingRead(NK_TargetVoltageSetpoint).toFloat();
  }
  if (!settingExists(NK_AbsorptionTimeoutMs)) {
    settingWrite(NK_AbsorptionTimeoutMs, String(AbsorptionTimeoutMs).c_str());
  } else {
    AbsorptionTimeoutMs = settingRead(NK_AbsorptionTimeoutMs).toInt();
  }
  if (!settingExists(NK_bulkVoltageHoldMs)) {
    settingWrite(NK_bulkVoltageHoldMs, String(bulkVoltageHoldMs).c_str());
  } else {
    bulkVoltageHoldMs = settingRead(NK_bulkVoltageHoldMs).toInt();
  }


  if (!settingExists(NK_VoltageKi)) {
    settingWrite(NK_VoltageKi, String(VoltageKi).c_str());
  } else {
    VoltageKi = settingRead(NK_VoltageKi).toFloat();
  }
  if (!settingExists(NK_SlopeBleedThresh)) {
    settingWrite(NK_SlopeBleedThresh, String(SlopeBleedThresh, 3).c_str());
  } else {
    SlopeBleedThresh = settingRead(NK_SlopeBleedThresh).toFloat();
  }
  if (!settingExists(NK_SlopeBleedK)) {
    settingWrite(NK_SlopeBleedK, String(SlopeBleedK, 1).c_str());
  } else {
    SlopeBleedK = settingRead(NK_SlopeBleedK).toFloat();
  }
  // cvHelpersEnabled — master switch for the asymmetric KiDown unwind + slope-aware bleed
  if (!settingExists(NK_cvHelpersEnabled)) {
    settingWrite(NK_cvHelpersEnabled, String((int)cvHelpersEnabled).c_str());
  } else {
    cvHelpersEnabled = settingRead(NK_cvHelpersEnabled).toInt() != 0;
  }
  // ── CV gain-mode system (Auto λ-based vs Manual) + measured plant ──
  if (!settingExists(NK_cvGainMode)) {
    settingWrite(NK_cvGainMode, String((int)cvGainMode).c_str());
  } else {
    cvGainMode = (uint8_t)settingRead(NK_cvGainMode).toInt();
  }
  // CV crossover ω_c — NVS key renamed cvOmega → cvCrossover (§F.3). Mint-new-key + migrate: adopt the old
  // value if present, folding in the misnamed-0.286 → 0.20 default fix (else ~24 % hot under the exact
  // magnitude formula), write the new key, and remove the orphaned old one. Dev-units-only window.
  if (settingExists(NK_cvCrossover)) {
    cvCrossover = settingRead(NK_cvCrossover).toFloat();
  } else if (settingExists("cvOmega")) {
    float old = settingRead("cvOmega").toFloat();
    cvCrossover = (fabsf(old - 0.286f) < 0.003f) ? 0.20f : old;
    settingWrite(NK_cvCrossover, String(cvCrossover, 3).c_str());
    settingRemove("cvOmega");
  } else {
    settingWrite(NK_cvCrossover, String(cvCrossover, 3).c_str());
  }
  // CV PI zero ρ — NVS key renamed cvKiRatio → cvPiZero (§F.3). Same mint-new-key + migrate (value unchanged).
  if (settingExists(NK_cvPiZero)) {
    cvPiZero = settingRead(NK_cvPiZero).toFloat();
  } else if (settingExists("cvKiRatio")) {
    cvPiZero = settingRead("cvKiRatio").toFloat();
    settingWrite(NK_cvPiZero, String(cvPiZero, 3).c_str());
    settingRemove("cvKiRatio");
  } else {
    settingWrite(NK_cvPiZero, String(cvPiZero, 3).c_str());
  }
  if (!settingExists(NK_vTgtRampEnable)) {
    settingWrite(NK_vTgtRampEnable, String((int)vTgtRampEnable).c_str());
  } else {
    vTgtRampEnable = (uint8_t)settingRead(NK_vTgtRampEnable).toInt();
  }
  if (!settingExists(NK_vTgtRampUp)) {
    settingWrite(NK_vTgtRampUp, String(vTgtRampUp, 3).c_str());
  } else {
    vTgtRampUp = settingRead(NK_vTgtRampUp).toFloat();
  }
  if (!settingExists(NK_vTgtRampDn)) {
    settingWrite(NK_vTgtRampDn, String(vTgtRampDn, 3).c_str());
  } else {
    vTgtRampDn = settingRead(NK_vTgtRampDn).toFloat();
  }
  if (!settingExists(NK_setpointSlewEnable)) {
    settingWrite(NK_setpointSlewEnable, String((int)setpointSlewEnable).c_str());
  } else {
    setpointSlewEnable = (uint8_t)settingRead(NK_setpointSlewEnable).toInt();
  }
  if (!settingExists(NK_cvRiseGovEnable)) {
    settingWrite(NK_cvRiseGovEnable, String((int)cvRiseGovEnable).c_str());
  } else {
    cvRiseGovEnable = (uint8_t)settingRead(NK_cvRiseGovEnable).toInt();
  }
  if (!settingExists(NK_dutySlewEnable)) {
    settingWrite(NK_dutySlewEnable, String((int)dutySlewEnable).c_str());
  } else {
    dutySlewEnable = (uint8_t)settingRead(NK_dutySlewEnable).toInt();
  }
  if (!settingExists(NK_cvPlantK)) {
    settingWrite(NK_cvPlantK, String(cvPlantK, 5).c_str());
  } else {
    cvPlantK = settingRead(NK_cvPlantK).toFloat();
  }
  if (settingExists(NK_ripFitAlt))  ripFitDecode(settingRead(NK_ripFitAlt),  ripFitAlt);   // measured ripple projection (§3.3); absent → nPts=0 → plot shows threshold only
  // CommissionTempF — board temp stamped when the CV plant fit was applied; reference for the battery-
  // temp gain derate. No default write: absence = never commissioned = no derate (stays NaN).
  if (settingExists(NK_CommissionTempF)) {
    CommissionTempF = settingRead(NK_CommissionTempF).toFloat();
  }
  if (!settingExists(NK_battTempDerateEn)) {
    settingWrite(NK_battTempDerateEn, String((int)battTempDerateEnable).c_str());
  } else {
    battTempDerateEnable = settingRead(NK_battTempDerateEn).toInt() != 0;
  }
  if (!settingExists(NK_battTempCoeff)) {
    settingWrite(NK_battTempCoeff, String(battTempCoeff, 4).c_str());
  } else {
    battTempCoeff = settingRead(NK_battTempCoeff).toFloat();
  }
  // coldChargeLockoutEnable — master on/off for the board-temp cold-charge lockout (lithium protection)
  if (!settingExists(NK_coldChargeLockoutEnable)) {
    settingWrite(NK_coldChargeLockoutEnable, String((int)coldChargeLockoutEnable).c_str());
  } else {
    coldChargeLockoutEnable = settingRead(NK_coldChargeLockoutEnable).toInt() != 0;
  }
  // MinChargeTempF — board-temp floor below which charging is locked out (°F)
  if (!settingExists(NK_MinChargeTempF)) {
    settingWrite(NK_MinChargeTempF, String(MinChargeTempF).c_str());
  } else {
    MinChargeTempF = settingRead(NK_MinChargeTempF).toFloat();
  }
  if (!settingExists(NK_SlopeBleedProxV)) {
    settingWrite(NK_SlopeBleedProxV, String(SlopeBleedProxV, 2).c_str());
  } else {
    SlopeBleedProxV = settingRead(NK_SlopeBleedProxV).toFloat();
  }
  if (!settingExists(NK_PidKp)) {
    settingWrite(NK_PidKp, String(PidKp, 3).c_str());
  } else {
    PidKp = settingRead(NK_PidKp).toFloat();
  }
  if (!settingExists(NK_TempPIDKp)) {
    settingWrite(NK_TempPIDKp, String(TempPIDKp, 6).c_str());
  } else {
    TempPIDKp = settingRead(NK_TempPIDKp).toFloat();
  }
  if (!settingExists(NK_ThermalLookaheadSec)) {
    settingWrite(NK_ThermalLookaheadSec, String(ThermalLookaheadSec, 1).c_str());
  } else {
    ThermalLookaheadSec = max(0.0f, settingRead(NK_ThermalLookaheadSec).toFloat());
  }

  if (!settingExists(NK_ThermalSlopeWindowSec)) {
    settingWrite(NK_ThermalSlopeWindowSec, String(ThermalSlopeWindowSec, 1).c_str());
  } else {
    ThermalSlopeWindowSec = constrain(settingRead(NK_ThermalSlopeWindowSec).toFloat(), 10.0f, 60.0f);
  }

  if (!settingExists(NK_TempPIDKi)) {
    settingWrite(NK_TempPIDKi, String(TempPIDKi, 6).c_str());
  } else {
    TempPIDKi = settingRead(NK_TempPIDKi).toFloat();
  }

  if (!settingExists(NK_TempPIDKiDownFrac)) {
    settingWrite(NK_TempPIDKiDownFrac, String(TempPIDKiDownFrac, 3).c_str());
  } else {
    TempPIDKiDownFrac = settingRead(NK_TempPIDKiDownFrac).toFloat();
  }


  if (!settingExists(NK_TempPIDIntervalMs)) {
    settingWrite(NK_TempPIDIntervalMs, String(TempPIDIntervalMs).c_str());
  } else {
    TempPIDIntervalMs = settingRead(NK_TempPIDIntervalMs).toInt();
  }

  if (!settingExists(NK_TempPIDFilterAlpha)) {
    settingWrite(NK_TempPIDFilterAlpha, String(TempPIDFilterAlpha, 3).c_str());
  } else {
    TempPIDFilterAlpha = settingRead(NK_TempPIDFilterAlpha).toFloat();
  }


  if (!settingExists(NK_PidKi)) {
    settingWrite(NK_PidKi, String(PidKi, 3).c_str());
  } else {
    PidKi = settingRead(NK_PidKi).toFloat();
  }
  if (!settingExists(NK_PidKd)) {
    settingWrite(NK_PidKd, String(PidKd, 3).c_str());
  } else {
    PidKd = settingRead(NK_PidKd).toFloat();
  }
  if (!settingExists(NK_DutySlowRampRate)) {
    settingWrite(NK_DutySlowRampRate, String(DutySlowRampRate, 2).c_str());
  } else {
    DutySlowRampRate = settingRead(NK_DutySlowRampRate).toFloat();
  }
  if (!settingExists(NK_ShutdownPhase2HoldMs)) {
    settingWrite(NK_ShutdownPhase2HoldMs, String(ShutdownPhase2HoldMs).c_str());
  } else {
    ShutdownPhase2HoldMs = settingRead(NK_ShutdownPhase2HoldMs).toInt();
  }
  if (!settingExists(NK_PidSampleDivisor)) {
    settingWrite(NK_PidSampleDivisor, String(PidSampleDivisor).c_str());
  } else {
    PidSampleDivisor = settingRead(NK_PidSampleDivisor).toInt();
  }
  if (!settingExists(NK_LearningSettlingPeriod)) {
    settingWrite(NK_LearningSettlingPeriod, String(LearningSettlingPeriod).c_str());
  } else {
    LearningSettlingPeriod = settingRead(NK_LearningSettlingPeriod).toInt();
  }
  if (!settingExists(NK_LearningRPMChangeThreshold)) {
    settingWrite(NK_LearningRPMChangeThreshold, String(LearningRPMChangeThreshold).c_str());
  } else {
    LearningRPMChangeThreshold = settingRead(NK_LearningRPMChangeThreshold).toInt();
  }
  if (!settingExists(NK_LearningTempHysteresis)) {
    settingWrite(NK_LearningTempHysteresis, String(LearningTempHysteresis).c_str());
  } else {
    LearningTempHysteresis = settingRead(NK_LearningTempHysteresis).toInt();
  }
  if (!settingExists(NK_MaxTableValue)) {
    settingWrite(NK_MaxTableValue, String(MaxTableValue, 2).c_str());
  } else {
    MaxTableValue = settingRead(NK_MaxTableValue).toFloat();
  }
  HardOCTripAmps = MaxTableValue + 10.0f;  // always derived, not persisted
  if (!settingExists(NK_MaxPenaltyPercent)) {
    settingWrite(NK_MaxPenaltyPercent, String(MaxPenaltyPercent, 2).c_str());
  } else {
    MaxPenaltyPercent = settingRead(NK_MaxPenaltyPercent).toFloat();
  }
  if (!settingExists(NK_MaxPenaltyDuration)) {
    settingWrite(NK_MaxPenaltyDuration, String(MaxPenaltyDuration).c_str());
  } else {
    MaxPenaltyDuration = settingRead(NK_MaxPenaltyDuration).toInt();
  }
  if (!settingExists(NK_NeighborLearningFactor)) {
    settingWrite(NK_NeighborLearningFactor, String(NeighborLearningFactor, 3).c_str());
  } else {
    NeighborLearningFactor = settingRead(NK_NeighborLearningFactor).toFloat();
  }
  if (!settingExists(NK_yyMax)) {
    settingWrite(NK_yyMax, String(yyMax).c_str());
  } else {
    yyMax = settingRead(NK_yyMax).toInt();
  }
  if (!settingExists(NK_LearningMemoryDuration)) {
    settingWrite(NK_LearningMemoryDuration, String(LearningMemoryDuration).c_str());
  } else {
    LearningMemoryDuration = settingRead(NK_LearningMemoryDuration).toInt();
  }
  if (!settingExists(NK_DutyRampRate)) {
    // Duty-domain knob stored in REAL %/s for THIS bus. The hardcoded default is a 12V value, so on
    // first creation scale it by ×(12/BATTERY_VOLTAGE) (40%/s → 10%/s @48V) before persisting — the
    // field-current slew stays constant across banks, and the dashboard box shows what's actually used.
    DutyRampRate = DutyRampRate * 12.0f / (float)BATTERY_VOLTAGE;
    settingWrite(NK_DutyRampRate, String(DutyRampRate, 1).c_str());
  } else {
    DutyRampRate = settingRead(NK_DutyRampRate).toFloat();
  }
  if (!settingExists(NK_SettleTimeBeforeCut)) {
    settingWrite(NK_SettleTimeBeforeCut, String(SettleTimeBeforeCut).c_str());
  } else {
    SettleTimeBeforeCut = settingRead(NK_SettleTimeBeforeCut).toInt();
  }
  if (!settingExists(NK_TempWarnExcess)) {
    settingWrite(NK_TempWarnExcess, String(TempWarnExcess, 1).c_str());
  } else {
    TempWarnExcess = settingRead(NK_TempWarnExcess).toFloat();
  }
  if (!settingExists(NK_TempCritExcess)) {
    settingWrite(NK_TempCritExcess, String(TempCritExcess, 1).c_str());
  } else {
    TempCritExcess = settingRead(NK_TempCritExcess).toFloat();
  }
  if (!settingExists(NK_TempSustainedTimeout)) {
    settingWrite(NK_TempSustainedTimeout, String(TempSustainedTimeout).c_str());
  } else {
    TempSustainedTimeout = settingRead(NK_TempSustainedTimeout).toInt();
  }
  // AlternatorHardShutdownV — absolute hard-shutdown voltage threshold.
  // First-boot default auto-scales as BulkVoltage + 0.3 V so 24V/48V systems get sensible
  // defaults. Once written, the value is treated as user-set and never auto-overwritten —
  // a system-class change later requires manually re-setting it from the UI.
  // Migration: an old VoltageSpikeMargin key (a margin) converts to an absolute value.
  if (!settingExists(NK_AlternatorHardShutdownV)) {
    if (settingExists(NK_VoltageSpikeMargin)) {
      float oldMargin = settingRead(NK_VoltageSpikeMargin).toFloat();
      AlternatorHardShutdownV = BulkVoltage + oldMargin;
    } else {
      AlternatorHardShutdownV = BulkVoltage + 0.3f;  // first-boot auto-scale default
    }
    settingWrite(NK_AlternatorHardShutdownV, String(AlternatorHardShutdownV, 2).c_str());
  } else {
    AlternatorHardShutdownV = settingRead(NK_AlternatorHardShutdownV).toFloat();
  }
  if (!settingExists(NK_HardOCDebounceMs)) {
    settingWrite(NK_HardOCDebounceMs, String(HardOCDebounceMs).c_str());
  } else {
    HardOCDebounceMs = (uint32_t)settingRead(NK_HardOCDebounceMs).toInt();
  }
  if (!settingExists(NK_WarmupRampRate)) {
    settingWrite(NK_WarmupRampRate, String(WarmupRampRate, 2).c_str());
  } else {
    WarmupRampRate = max(0.0f, settingRead(NK_WarmupRampRate).toFloat());
  }
  if (!settingExists(NK_IExcessFrac)) {
    settingWrite(NK_IExcessFrac, String(IExcessFrac, 3).c_str());
  } else {
    IExcessFrac = settingRead(NK_IExcessFrac).toFloat();
  }
  if (!settingExists(NK_IExcessFracBulk)) {
    settingWrite(NK_IExcessFracBulk, String(IExcessFracBulk, 3).c_str());
  } else {
    IExcessFracBulk = settingRead(NK_IExcessFracBulk).toFloat();
  }
  if (!settingExists(NK_IExcessFloorA)) {
    settingWrite(NK_IExcessFloorA, String(IExcessFloorA, 1).c_str());
  } else {
    IExcessFloorA = settingRead(NK_IExcessFloorA).toFloat();
  }
  if (!settingExists(NK_IExcessCeilA)) {
    settingWrite(NK_IExcessCeilA, String(IExcessCeilA, 1).c_str());
  } else {
    IExcessCeilA = settingRead(NK_IExcessCeilA).toFloat();
  }
  if (!settingExists(NK_BattCurrentLimitA)) {
    settingWrite(NK_BattCurrentLimitA, String(BattCurrentLimitA, 1).c_str());
  } else {
    BattCurrentLimitA = settingRead(NK_BattCurrentLimitA).toFloat();
  }
  if (!settingExists(NK_IExcessTau)) {
    settingWrite(NK_IExcessTau, String(IExcessTau, 1).c_str());
  } else {
    IExcessTau = settingRead(NK_IExcessTau).toFloat();
  }
  if (!settingExists(NK_IExcessRelFrac)) {
    settingWrite(NK_IExcessRelFrac, String(IExcessRelFrac, 3).c_str());
  } else {
    IExcessRelFrac = settingRead(NK_IExcessRelFrac).toFloat();
  }
  if (!settingExists(NK_IExcessKBleed)) {
    settingWrite(NK_IExcessKBleed, String(IExcessKBleed, 2).c_str());
  } else {
    IExcessKBleed = settingRead(NK_IExcessKBleed).toFloat();
  }
  // Auto-commissioning state (default 0 = NOT_COMMISSIONED on a virgin device).
  if (settingExists(NK_commissionState)) {
    commissionState = (uint8_t)settingRead(NK_commissionState).toInt();
  }
  // Furthest commissioning phase reached (default 0 = Prep). Drives the tab checklist.
  if (settingExists(NK_commissionPhase)) {
    commissionPhase = (uint8_t)settingRead(NK_commissionPhase).toInt();
  }
  // Per-stage completion bitmask (default 0 = nothing done). Loaded BEFORE the in-progress
  // revert below so that revert can also clear it. This is the source of truth for the
  // per-step ✓ marks; commissionState is derived from it.
  if (settingExists(NK_commissionDoneMask)) {
    commissionDoneMask = (uint16_t)settingRead(NK_commissionDoneMask).toInt();
  }
  // A genuinely INTERRUPTED wizard run must NOT survive a reboot — detect it by the Phase-0
  // snapshot still being present (it is removed on both Finish and Abort). Such a boot is
  // treated exactly like Abort: revert every setting to the snapshot, reset to NOT_COMMISSIONED,
  // and clear the done mask so the user starts over from the reverted baseline. (All
  // snapshot-covered settings are loaded above, so the revert wins.)
  // NOTE: a COMMITTED-but-partial config (e.g. a commissioned device whose Min% floor was
  // invalidated by an RPM-breakpoint change) is also state==1 but has NO snapshot — it must
  // persist across reboot so the badge keeps nagging that a step needs re-running.
  if (commissionState == 1 && settingExists(NK_commissionSnap)) {
    commissionRestore();          // revert to the Phase-0 snapshot (also removes it)
    commissionSetState(0);
    commissionSetPhase(0);
    commissionDoneMask = 0;
    commissionWriteDoneMask();
  }
  if (!settingExists(NK_IExcessArmMarginV)) {
    settingWrite(NK_IExcessArmMarginV, String(IExcessArmMarginV, 3).c_str());
  } else {
    IExcessArmMarginV = settingRead(NK_IExcessArmMarginV).toFloat();
  }
  if (!settingExists(NK_AwBleedRate)) {
    settingWrite(NK_AwBleedRate, String(AwBleedRate, 2).c_str());
  } else {
    AwBleedRate = settingRead(NK_AwBleedRate).toFloat();
  }
  // AwRecoverRate is hardcoded (0.1f) — not persisted
  if (!settingExists(NK_AwSeedProtectMs)) {
    settingWrite(NK_AwSeedProtectMs, String(AwSeedProtectMs).c_str());
  } else {
    AwSeedProtectMs = (uint16_t)settingRead(NK_AwSeedProtectMs).toInt();
  }
  if (!settingExists(NK_FastSetpointRiseRate)) {
    settingWrite(NK_FastSetpointRiseRate, String(FastSetpointRiseRate, 1).c_str());
  } else {
    FastSetpointRiseRate = settingRead(NK_FastSetpointRiseRate).toFloat();
  }
  if (!settingExists(NK_FastSetpointRiseWindowMs)) {
    settingWrite(NK_FastSetpointRiseWindowMs, String(FastSetpointRiseWindowMs).c_str());
  } else {
    FastSetpointRiseWindowMs = (uint32_t)settingRead(NK_FastSetpointRiseWindowMs).toInt();
  }
  if (!settingExists(NK_FastSetpointRiseHeadroomV)) {
    settingWrite(NK_FastSetpointRiseHeadroomV, String(FastSetpointRiseHeadroomV, 2).c_str());
  } else {
    FastSetpointRiseHeadroomV = settingRead(NK_FastSetpointRiseHeadroomV).toFloat();
  }
  if (!settingExists(NK_KHard)) {
    settingWrite(NK_KHard, String(KHard, 1).c_str());
  } else {
    KHard = settingRead(NK_KHard).toFloat();
  }
  // ReseedFrac (was IExcessReseedFrac) — migrates from old filename if present
  if (settingExists(NK_ReseedFrac)) {
    ReseedFrac = settingRead(NK_ReseedFrac).toFloat();
  } else if (settingExists(NK_IExcessReseedFrac)) {
    ReseedFrac = settingRead(NK_IExcessReseedFrac).toFloat();
    settingWrite(NK_ReseedFrac, String(ReseedFrac, 2).c_str());
  } else {
    settingWrite(NK_ReseedFrac, String(ReseedFrac, 2).c_str());
  }
  // OvGroup1Enable — migrates from old /OvLayer2Enable.txt if found
  if (settingExists(NK_OvGroup1Enable)) {
    OvGroup1Enable = settingRead(NK_OvGroup1Enable).toInt() != 0;
  } else if (settingExists(NK_OvLayer2Enable)) {
    OvGroup1Enable = settingRead(NK_OvLayer2Enable).toInt() != 0;
    settingWrite(NK_OvGroup1Enable, String((int)OvGroup1Enable).c_str());
  } else {
    settingWrite(NK_OvGroup1Enable, String((int)OvGroup1Enable).c_str());
  }
  // OvGroup2Enable — migrates from old /OvLayer3Enable.txt if found
  if (settingExists(NK_OvGroup2Enable)) {
    OvGroup2Enable = settingRead(NK_OvGroup2Enable).toInt() != 0;
  } else if (settingExists(NK_OvLayer3Enable)) {
    OvGroup2Enable = settingRead(NK_OvLayer3Enable).toInt() != 0;
    settingWrite(NK_OvGroup2Enable, String((int)OvGroup2Enable).c_str());
  } else {
    settingWrite(NK_OvGroup2Enable, String((int)OvGroup2Enable).c_str());
  }
  if (!settingExists(NK_OutputPIDSigSrc)) {
    settingWrite(NK_OutputPIDSigSrc, String(OutputPIDSigSrc).c_str());
  } else {
    OutputPIDSigSrc = constrain(settingRead(NK_OutputPIDSigSrc).toInt(), 0, 2);
  }
  if (!settingExists(NK_OutputPIDMA_N)) {
    settingWrite(NK_OutputPIDMA_N, String(OutputPIDMA_N).c_str());
  } else {
    OutputPIDMA_N = constrain(settingRead(NK_OutputPIDMA_N).toInt(), 1, I_RING_SIZE);
  }
  if (!settingExists(NK_OutputPIDFilterTC)) {
    settingWrite(NK_OutputPIDFilterTC, String(OutputPIDFilterTC).c_str());
  } else {
    OutputPIDFilterTC = settingRead(NK_OutputPIDFilterTC).toFloat();
  }
  if (!settingExists(NK_VoltageFilterTC)) {
    settingWrite(NK_VoltageFilterTC, String(VoltageFilterTC).c_str());
  } else {
    VoltageFilterTC = settingRead(NK_VoltageFilterTC).toFloat();
  }
  if (!settingExists(NK_TdPred)) {
    settingWrite(NK_TdPred, String(TdPred, 3).c_str());
  } else {
    TdPred = settingRead(NK_TdPred).toFloat();
  }
  if (!settingExists(NK_OvMeasMarginV)) {
    if (settingExists(NK_VSoftMarginV)) { OvMeasMarginV = settingRead(NK_VSoftMarginV).toFloat(); }
    settingWrite(NK_OvMeasMarginV, String(OvMeasMarginV, 3).c_str());
  } else {
    OvMeasMarginV = settingRead(NK_OvMeasMarginV).toFloat();
  }
  if (!settingExists(NK_OvPredMarginV)) {
    if (settingExists(NK_VHardMarginV)) { OvPredMarginV = settingRead(NK_VHardMarginV).toFloat(); }
    settingWrite(NK_OvPredMarginV, String(OvPredMarginV, 3).c_str());
  } else {
    OvPredMarginV = settingRead(NK_OvPredMarginV).toFloat();
  }
  // DvdtTC (was DvdtAlpha) — migrates from old alpha-based file if present.
  // Conversion: TC = 5ms × (1 − α) / α  (preserves behavior at 5ms nominal cadence).
  if (settingExists(NK_DvdtTC)) {
    DvdtTC = constrain(settingRead(NK_DvdtTC).toFloat(), 5.0f, 500.0f);
  } else if (settingExists(NK_DvdtAlpha)) {
    float oldAlpha = constrain(settingRead(NK_DvdtAlpha).toFloat(), 0.01f, 0.50f);
    DvdtTC = constrain(5.0f * (1.0f - oldAlpha) / oldAlpha, 5.0f, 500.0f);
    settingWrite(NK_DvdtTC, String(DvdtTC, 1).c_str());
  } else {
    settingWrite(NK_DvdtTC, String(DvdtTC, 1).c_str());
  }
  if (!settingExists(NK_VoltageDisagreeThreshold)) {
    settingWrite(NK_VoltageDisagreeThreshold, String(VoltageDisagreeThreshold, 2).c_str());
  } else {
    VoltageDisagreeThreshold = settingRead(NK_VoltageDisagreeThreshold).toFloat();
  }
  if (!settingExists(NK_VoltageDisagreeTimeout)) {
    settingWrite(NK_VoltageDisagreeTimeout, String(VoltageDisagreeTimeout).c_str());
  } else {
    VoltageDisagreeTimeout = settingRead(NK_VoltageDisagreeTimeout).toInt();
  }
  // Alternator-health settings load via altSettingsLoad() in setup.

  if (!settingExists(NK_VoltageKp)) {
    settingWrite(NK_VoltageKp, String(VoltageKp, 1).c_str());
  } else {
    VoltageKp = settingRead(NK_VoltageKp).toFloat();
  }
  // Derive the active CV gains now that gain mode, λ, plant, and manual Kp/Ki are all loaded.
  recomputeCvGains();
  recomputeCcGains();  // and the active CC (output-current) gains, normalized to BATTERY_VOLTAGE
  if (!settingExists(NK_VoltageLoopInterval)) {
    settingWrite(NK_VoltageLoopInterval, String(VoltageLoopInterval).c_str());
  } else {
    VoltageLoopInterval = settingRead(NK_VoltageLoopInterval).toInt();
  }
  if (!settingExists(NK_FIELD_COLLAPSE_DELAY)) {
    settingWrite(NK_FIELD_COLLAPSE_DELAY, String(FIELD_COLLAPSE_DELAY).c_str());
  } else {
    FIELD_COLLAPSE_DELAY = settingRead(NK_FIELD_COLLAPSE_DELAY).toInt();
  }
  // IMU safety thresholds — user-set via form, persisted (Pattern B).
  // imuMountOrientation rides on /vessel_info.json (separate path).
  if (!settingExists(NK_CAPSIZE_THRESHOLD_DEG)) {
    settingWrite(NK_CAPSIZE_THRESHOLD_DEG, String(CAPSIZE_THRESHOLD_DEG, 1).c_str());
  } else {
    CAPSIZE_THRESHOLD_DEG = settingRead(NK_CAPSIZE_THRESHOLD_DEG).toFloat();
  }
  if (!settingExists(NK_PITCHPOLE_THRESHOLD_DEG)) {
    settingWrite(NK_PITCHPOLE_THRESHOLD_DEG, String(PITCHPOLE_THRESHOLD_DEG, 1).c_str());
  } else {
    PITCHPOLE_THRESHOLD_DEG = settingRead(NK_PITCHPOLE_THRESHOLD_DEG).toFloat();
  }
  if (!settingExists(NK_SLAM_THRESHOLD_G)) {
    settingWrite(NK_SLAM_THRESHOLD_G, String(SLAM_THRESHOLD_G, 2).c_str());
  } else {
    SLAM_THRESHOLD_G = settingRead(NK_SLAM_THRESHOLD_G).toFloat();
  }

  // Fast alt-current diagnostic knobs (Pattern B). Defaults = the #define values, so a fresh
  // device behaves exactly as before the knobs existed. faInit() also reads NK_faEnabled
  // directly (it runs before this), but writing the default here creates the key on day one.
  if (!settingExists(NK_faEnabled)) {
    settingWrite(NK_faEnabled, faEnabled ? "1" : "0");
  } else {
    faEnabled = (settingRead(NK_faEnabled).toInt() != 0);
  }
  // WiFi Napping standby toggle (Client-mode only; default off)
  if (!settingExists(NK_wifiNapEnabled)) {
    settingWrite(NK_wifiNapEnabled, wifiNapEnabled ? "1" : "0");
  } else {
    wifiNapEnabled = (settingRead(NK_wifiNapEnabled).toInt() != 0);
  }
  if (!settingExists(NK_faAlarmEnable)) {
    settingWrite(NK_faAlarmEnable, faAlarmEnable ? "1" : "0");
  } else {
    faAlarmEnable = (settingRead(NK_faAlarmEnable).toInt() != 0);
  }
  if (!settingExists(NK_faAnomPause)) {
    settingWrite(NK_faAnomPause, faAnomPause ? "1" : "0");
  } else {
    faAnomPause = (settingRead(NK_faAnomPause).toInt() != 0);
  }
  if (!settingExists(NK_faRpmEdgeMargin)) {
    settingWrite(NK_faRpmEdgeMargin, String(faRpmEdgeMargin, 1).c_str());
  } else {
    faRpmEdgeMargin = settingRead(NK_faRpmEdgeMargin).toFloat();
  }
  if (!settingExists(NK_faAmpsDriftFloorA)) {
    settingWrite(NK_faAmpsDriftFloorA, String(faAmpsDriftFloorA, 2).c_str());
  } else {
    faAmpsDriftFloorA = settingRead(NK_faAmpsDriftFloorA).toFloat();
  }
  if (!settingExists(NK_faAmpsDriftPct)) {
    settingWrite(NK_faAmpsDriftPct, String(faAmpsDriftPct, 1).c_str());
  } else {
    faAmpsDriftPct = settingRead(NK_faAmpsDriftPct).toFloat();
  }
  // Measured-ripple capture admission gates (§10.8/§11). NVS key "ripRpmMargin" is abandoned — never reuse.
  if (!settingExists(NK_ripWinMs)) {
    settingWrite(NK_ripWinMs, String(ripWinMs, 0).c_str());
  } else {
    // §11 halves need ≥1 disturbance cycle each — floor pre-§11 stored values (250–500 ms era) up to the new minimum
    ripWinMs = fmaxf(500.0f, settingRead(NK_ripWinMs).toFloat());
  }
  if (!settingExists(NK_ripDriftFloorA)) {
    settingWrite(NK_ripDriftFloorA, String(ripDriftFloorA, 2).c_str());
  } else {
    ripDriftFloorA = settingRead(NK_ripDriftFloorA).toFloat();
  }
  if (!settingExists(NK_ripDriftPct)) {
    settingWrite(NK_ripDriftPct, String(ripDriftPct, 1).c_str());
  } else {
    ripDriftPct = settingRead(NK_ripDriftPct).toFloat();
  }
  if (!settingExists(NK_faAttenUpAmps)) {
    settingWrite(NK_faAttenUpAmps, String(faAttenUpAmps, 1).c_str());
  } else {
    faAttenUpAmps = settingRead(NK_faAttenUpAmps).toFloat();
  }
  if (!settingExists(NK_faAttenDownAmps)) {
    settingWrite(NK_faAttenDownAmps, String(faAttenDownAmps, 1).c_str());
  } else {
    faAttenDownAmps = settingRead(NK_faAttenDownAmps).toFloat();
  }
  if (!settingExists(NK_faPeakMinA)) {
    settingWrite(NK_faPeakMinA, String(faPeakMinA, 2).c_str());
  } else {
    faPeakMinA = settingRead(NK_faPeakMinA).toFloat();
  }
}

// ============================================================================
// IMU Ring Buffer Helper Functions
// ============================================================================
inline void pushAccelSample(int16_t x, int16_t y, int16_t z, uint32_t timestamp_us) {
  uint16_t next_head = (imuRingBuffer->accel_head + 1) % ACCEL_RING_SIZE;

  if (next_head == imuRingBuffer->accel_tail) {
    // Ring buffer full - drop oldest sample (move tail forward)
    imuRingBuffer->accel_tail = (imuRingBuffer->accel_tail + 1) % ACCEL_RING_SIZE;
    imuRingBuffer->accel_dropped++;
  }

  imuRingBuffer->accel[imuRingBuffer->accel_head].x = x;
  imuRingBuffer->accel[imuRingBuffer->accel_head].y = y;
  imuRingBuffer->accel[imuRingBuffer->accel_head].z = z;
  imuRingBuffer->accel[imuRingBuffer->accel_head].timestamp_us = timestamp_us;
  imuRingBuffer->accel_head = next_head;

  imu_total_samples_accel++;
}

inline void pushGyroSample(int16_t x, int16_t y, int16_t z, uint32_t timestamp_us) {
  uint16_t next_head = (imuRingBuffer->gyro_head + 1) % GYRO_RING_SIZE;

  if (next_head == imuRingBuffer->gyro_tail) {
    // Ring buffer full - drop oldest sample (move tail forward)
    imuRingBuffer->gyro_tail = (imuRingBuffer->gyro_tail + 1) % GYRO_RING_SIZE;
    imuRingBuffer->gyro_dropped++;
  }

  imuRingBuffer->gyro[imuRingBuffer->gyro_head].x = x;
  imuRingBuffer->gyro[imuRingBuffer->gyro_head].y = y;
  imuRingBuffer->gyro[imuRingBuffer->gyro_head].z = z;
  imuRingBuffer->gyro[imuRingBuffer->gyro_head].timestamp_us = timestamp_us;
  imuRingBuffer->gyro_head = next_head;

  imu_total_samples_gyro++;
}

// ============================================================================
// IMU CONVERSION CONSTANTS
// ============================================================================
// LSM6DSOX sensitivity values (from datasheet)
// Accel: ±2g range → 0.061 mg/LSB → 0.000061 g/LSB
// Gyro: ±2000 dps range → 70 mdps/LSB → 0.070 dps/LSB
constexpr float ACCEL_SCALE = 0.000061f;  // g per LSB
constexpr float GYRO_SCALE = 0.070f;      // dps per LSB

// Axis remapping: maps sensor axes to vessel axes for each mounting orientation.
// src[i] = which sensor axis (0=X, 1=Y, 2=Z) feeds vessel axis i
// sign[i] = +1 or -1
// Index order: [0]=accel_fwd, [1]=accel_stbd, [2]=accel_up,
//              [3]=gyro_pitch, [4]=gyro_heel,  [5]=gyro_yaw
// All four orientations VERIFIED from physical tilt tests.
struct AxisRemap {
  uint8_t src[6];
  int8_t sign[6];
};

const AxisRemap axisRemap[4] = {
  { { 2, 0, 1, 2, 0, 1 }, { -1, -1, -1, -1, -1, -1 } },  // 0: Fwd bulkhead, facing aft
  { { 2, 0, 1, 2, 0, 1 }, { +1, +1, -1, +1, +1, -1 } },  // 1: Aft bulkhead, facing fwd
  { { 0, 2, 1, 0, 2, 1 }, { -1, +1, -1, -1, +1, -1 } },  // 2: Port wall, facing stbd
  { { 0, 2, 1, 0, 2, 1 }, { +1, -1, -1, +1, -1, -1 } },  // 3: Stbd wall, facing port
};


// Complementary filter parameters
constexpr float CF_ALPHA = 0.90f;  // Gyro weight (0.90 = trust gyro 90%, accel 10%) → time constant ~0.19 s; may show some wave flutter at sea

// 60-second rolling window
constexpr uint16_t ROLLING_WINDOW_SIZE = 60;  // 1 sample per second
struct RollingWindow {
  float heel[ROLLING_WINDOW_SIZE];
  float pitch[ROLLING_WINDOW_SIZE];
  uint16_t index = 0;
  uint16_t count = 0;  // How many valid samples (0-60)
} rolling60s;

// 120-second rolling window for anchor display (roll/pitch deviation + heading swing)
// heading entries: actual degrees [0,360] when fresh, -1.0 when compass stale
constexpr uint16_t ROLLING_WINDOW_120 = 120;
struct RollingWindow120 {
  float heel[ROLLING_WINDOW_120];
  float pitch[ROLLING_WINDOW_120];
  float heading[ROLLING_WINDOW_120];
  uint16_t index = 0;
  uint16_t count = 0;
} rolling120s;

// ============================================================================
// IMU METRIC PROCESSING FUNCTIONS
// ============================================================================
void resetAccelStats() {
  // Reset all period statistics (called after uploading to Supabase)
  imu_accel_x_min = 999.0;
  imu_accel_x_max = -999.0;
  imu_accel_x_sum = 0;
  imu_accel_x_count = 0;

  imu_accel_y_min = 999.0;
  imu_accel_y_max = -999.0;
  imu_accel_y_sum = 0;
  imu_accel_y_count = 0;

  imu_accel_z_min = 999.0;
  imu_accel_z_max = -999.0;
  imu_accel_z_sum = 0;
  imu_accel_z_count = 0;

  imu_gyro_x_min = 999.0;
  imu_gyro_x_max = -999.0;
  imu_gyro_x_sum = 0;
  imu_gyro_x_count = 0;

  imu_gyro_y_min = 999.0;
  imu_gyro_y_max = -999.0;
  imu_gyro_y_sum = 0;
  imu_gyro_y_count = 0;

  imu_gyro_z_min = 999.0;
  imu_gyro_z_max = -999.0;
  imu_gyro_z_sum = 0;
  imu_gyro_z_count = 0;

  imu_heel_min = 999.0;
  imu_heel_max = -999.0;
  imu_heel_sum = 0;
  imu_heel_count = 0;

  imu_pitch_min = 999.0;
  imu_pitch_max = -999.0;
  imu_pitch_sum = 0;
  imu_pitch_count = 0;

  imu_vertical_accel_min = 999.0;
  imu_vertical_accel_max = -999.0;
  imu_vertical_accel_sum = 0;
  imu_vertical_accel_count = 0;

  imu_total_accel_min = 999.0;
  imu_total_accel_max = -999.0;
  imu_total_accel_sum = 0;
  imu_total_accel_count = 0;

  imu_slam_count = 0;
  imu_slam_peak_max = 0;
}
void complementaryFilter(float ax, float ay, float az, float gx, float gy, float dt) {
  // Accel-derived angles (noisy but no drift)
  float accel_heel = atan2(ay, sqrt(ax * ax + az * az)) * 180.0f / PI - imuHeelOffsetDeg;
  float accel_pitch = atan2(-ax, sqrt(ay * ay + az * az)) * 180.0f / PI - imuPitchOffsetDeg;

  // First sample after boot seeds the filter from accel directly. Starting from
  // 0 made the estimate slew to the true attitude over several seconds, and the
  // long-term ring recorded that sweep as a 0-to-actual min/max wedge in the
  // first bucket. (IMU-zero capture still snaps to 0 by design — that's a new
  // level reference, not a restart.)
  static bool cf_seeded = false;
  if (!cf_seeded) {
    cf_seeded = true;
    cf_heel = accel_heel;
    cf_pitch = accel_pitch;
    return;
  }

  // Integrate gyro (smooth but drifts)
  cf_heel += gy * dt;
  cf_pitch += gx * dt;

  cf_heel = CF_ALPHA * cf_heel + (1.0f - CF_ALPHA) * accel_heel;
  cf_pitch = CF_ALPHA * cf_pitch + (1.0f - CF_ALPHA) * accel_pitch;
}
void updateAccelMetrics() {
  // Called from main loop every iteration (~300 Hz at 3ms loop time, NOT 1 Hz).
  // Most calls find 0 new samples (arrival rate is 104 Hz post-ODR-drop) and exit quickly;
  // accel_cap=50 limits per-call work when a backlog exists. Timer reports sub-ms worst,
  // which integer-divides to 0 on the dashboard — function IS running (Ring Usage stays 0%).

  if (!imuEnabled) return;
  if (!imuRingBuffer || !imuWindow) { imuEnabled = false; return; }

  uint32_t samples_processed = 0;
  unsigned long now = millis();

  // Wave period decimation state (10 Hz output, real-time gated)
  static float wave_decim_sum = 0;
  static uint16_t wave_decim_count = 0;
  static unsigned long wave_last_decim_ms = 0;
  static bool wave_dc_initialized = false;
  static float msi_rms2_ewma = 0;    // 60s RMS² EWMA for MSI frequency-weighted vertical accel
  static bool gps_was_moving = false; // hysteresis state: ON above 1.7 kt, OFF below 1.3 kt

  // Process accel ring buffer — cap per call as a safety net against edge cases
  uint16_t accel_cap = 0;
  while (imuRingBuffer->accel_tail != imuRingBuffer->accel_head && accel_cap < 50) {
    accel_cap++;
    IMUSample *s = &imuRingBuffer->accel[imuRingBuffer->accel_tail];

    // Calculate time delta for area accumulation
    uint64_t now_us = s->timestamp_us;
    uint64_t dt_us = (imuWindow->lastUpdateTime_us > 0)
                       ? (now_us - imuWindow->lastUpdateTime_us)
                       : 0;

    // Convert to engineering units with full axis permutation + sign remapping
    const AxisRemap &r = axisRemap[imuMountOrientation];
    float raw_a[3] = { (float)s->x, (float)s->y, (float)s->z };
    float ax = raw_a[r.src[0]] * ACCEL_SCALE * r.sign[0];  // vessel forward
    float ay = raw_a[r.src[1]] * ACCEL_SCALE * r.sign[1];  // vessel starboard
    float az = raw_a[r.src[2]] * ACCEL_SCALE * r.sign[2];  // vessel up

    // IMU zero capture — accumulate at-rest accel for level reference
    if (imuZeroInProgress && imuZeroAccelN < IMU_ZERO_TARGET) {
      imuZeroAxSum += ax; imuZeroAySum += ay; imuZeroAzSum += az;
      imuZeroAccelN++;
    }

    // Scale to integers (1.234g → 1234)
    int32_t ax_scaled = (int32_t)(ax * 1000.0f);
    int32_t ay_scaled = (int32_t)(ay * 1000.0f);
    int32_t az_scaled = (int32_t)(az * 1000.0f);

    // Update window statistics with time-weighted averaging
    if (dt_us > 0 && dt_us < 100000) {  // Sanity check: 0-100ms
      if (ax_scaled < imuWindow->accel_x_min) imuWindow->accel_x_min = ax_scaled;
      if (ax_scaled > imuWindow->accel_x_max) imuWindow->accel_x_max = ax_scaled;
      imuWindow->accel_x_area_v_us += (int64_t)ax_scaled * dt_us;
      imuWindow->accel_x_valid_us += dt_us;

      if (ay_scaled < imuWindow->accel_y_min) imuWindow->accel_y_min = ay_scaled;
      if (ay_scaled > imuWindow->accel_y_max) imuWindow->accel_y_max = ay_scaled;
      imuWindow->accel_y_area_v_us += (int64_t)ay_scaled * dt_us;
      imuWindow->accel_y_valid_us += dt_us;

      if (az_scaled < imuWindow->accel_z_min) imuWindow->accel_z_min = az_scaled;
      if (az_scaled > imuWindow->accel_z_max) imuWindow->accel_z_max = az_scaled;
      imuWindow->accel_z_area_v_us += (int64_t)az_scaled * dt_us;
      imuWindow->accel_z_valid_us += dt_us;

      float total_accel = sqrt(ax * ax + ay * ay + az * az);
      int32_t total_accel_scaled = (int32_t)(total_accel * 1000.0f);
      if (total_accel_scaled < imuWindow->total_accel_min) imuWindow->total_accel_min = total_accel_scaled;
      if (total_accel_scaled > imuWindow->total_accel_max) imuWindow->total_accel_max = total_accel_scaled;
      imuWindow->total_accel_area_v_us += (int64_t)total_accel_scaled * dt_us;
      imuWindow->total_accel_valid_us += dt_us;

      // Vertical acceleration (Z-axis in vessel frame)
      float vert_accel = az;
      int32_t vert_accel_scaled = az_scaled;  // Already scaled above
      if (vert_accel_scaled < imuWindow->vertical_accel_min) imuWindow->vertical_accel_min = vert_accel_scaled;
      if (vert_accel_scaled > imuWindow->vertical_accel_max) imuWindow->vertical_accel_max = vert_accel_scaled;
      imuWindow->vertical_accel_area_v_us += (int64_t)vert_accel_scaled * dt_us;
      imuWindow->vertical_accel_valid_us += dt_us;

      // Slam detection — refractory period prevents multi-counting a single physical event
      static uint32_t lastSlamMs = 0;
      if (vert_accel > SLAM_THRESHOLD_G) {
        if ((uint32_t)now - lastSlamMs >= 300) {
          lastSlamMs = (uint32_t)now;
          imuWindow->slam_count++;
          imu_slam_count_lifetime++;
        }
        // Always update peak — capture the worst reading within the event
        if (vert_accel_scaled > imuWindow->slam_peak_max) imuWindow->slam_peak_max = vert_accel_scaled;
        if (vert_accel > imu_slam_peak_lifetime) imu_slam_peak_lifetime = vert_accel;
      }
    }

    // Accumulate az for wave period decimation (emitted every 100ms after the loop)
    wave_decim_sum += az;
    wave_decim_count++;

    // Store current raw values (last sample processed, for display)
    imu_accel_x_raw = ax;
    imu_accel_y_raw = ay;
    imu_accel_z_raw = az;
    imu_vertical_accel_g = az;
    imu_total_accel_g = sqrt(ax * ax + ay * ay + az * az);
    wmIgnUpdate(wmIgn_vacc, imu_vertical_accel_g);  // ignition-cycle watermark

    // Wave period decimation and processing is handled inline below (10 Hz decimation, zero-crossing)

    imuWindow->lastUpdateTime_us = now_us;
    imuRingBuffer->accel_tail = (imuRingBuffer->accel_tail + 1) % ACCEL_RING_SIZE;
    samples_processed++;
  }

  // GPS speed gate — ON above 1.7 kt, OFF below 1.3 kt (hysteresis prevents threshold chatter)
  bool gps_is_moving = (SOGNMEA > 1.7f) || (gps_was_moving && SOGNMEA >= 1.3f);
  if (gps_is_moving != gps_was_moving) {
    // Both scores reset to zero on any mode switch — no cross-contamination between trips and anchorings
    msi_rms2_ewma = 0.0f;
    if (!gps_is_moving) {
      // Underway → anchored: flush rolling window so passage motion doesn't skew comfort score
      rolling60s.index = 0;
      rolling60s.count = 0;
      rolling120s.index = 0;
      rolling120s.count = 0;
      imu_heading_swing_120s = -1.0f;
    }
    // (Anchored → underway: msi_rms2_ewma already reset above; scores will read 0.0 until EWMA builds)
    gps_was_moving = gps_is_moving;
  }

  // Wave period detection — emit one decimated sample every 100ms (10 Hz)
  if (wave_decim_count > 0 && (now - wave_last_decim_ms) >= 100) {
    float az_dec = wave_decim_sum / wave_decim_count;
    wave_last_decim_ms = now;
    wave_decim_sum = 0;
    wave_decim_count = 0;

    // DC removal: slow EWMA with ~30s time constant at 10 Hz (alpha = 1 - 1/300 samples)
    if (!wave_dc_initialized) {
      wave_last_dc_mean = az_dec;
      wave_dc_initialized = true;
    } else {
      wave_last_dc_mean = 0.9967f * wave_last_dc_mean + 0.0033f * az_dec;
    }
    float detrended = az_dec - wave_last_dc_mean;

    // Motion Sickness Index — L&G 1987 frequency-weighted vertical accel RMS
    // Weighting function peaks at 0.2 Hz (5s wave period); falls off as a Gaussian in log-freq space
    {
      float W_e;
      if (imu_wave_period_sec > 0) {
        float f = 1.0f / imu_wave_period_sec;
        float log_ratio = logf(f / 0.2f);  // log(f/0.2); 0 at 5s, ±1.5 at ~1s/15s
        W_e = 1.0f / (1.0f + log_ratio * log_ratio * 4.43f);
      } else {
        W_e = 0.5f;  // mid-range estimate when wave period is unknown
      }
      float a_w = detrended * W_e;
      if (gps_is_moving) {
        // 60s EWMA at 10 Hz: alpha = 1/(10 × 60) = 0.00167
        msi_rms2_ewma = 0.9983f * msi_rms2_ewma + 0.0017f * (a_w * a_w);
      }
      // Always publish scores so JS can display a grayed value at anchor; EWMA just doesn't accumulate there
      {
        float a_w_rms = sqrtf(msi_rms2_ewma);
        // L&G 1987 calibration: 0.03g weighted RMS → ~10% vomiting in 2hrs
        imu_msi_score = (a_w_rms / 0.03f) * 30.0f;
        // Power-law approx of L&G empirical population curve — labeled as approximate
        imu_vomit_pct = constrain(100.0f * powf(a_w_rms / 0.03f, 1.3f) * 0.10f, 0.0f, 100.0f);
      }
    }

    // Zero-crossing with hysteresis — prevents noise-triggered false crossings
    // wave_last_crossing_positive: true = currently in positive zone (last crossed up through +HYST)
    const float WAVE_HYST = 0.02f;  // ±20 mg band
    bool crossing = false;
    if (!wave_last_crossing_positive && detrended > WAVE_HYST) {
      wave_last_crossing_positive = true;
      crossing = true;
    } else if (wave_last_crossing_positive && detrended < -WAVE_HYST) {
      wave_last_crossing_positive = false;
      crossing = true;
    }

    if (crossing) {
      if (wave_last_crossing_time > 0) {
        // Each crossing is a half-period; full period = 2 × half-period
        float half_period_s = (now - wave_last_crossing_time) / 1000.0f;
        float period_s = half_period_s * 2.0f;
        if (period_s >= 2.0f && period_s <= 20.0f) {
          if (wave_period_ewma < 0) {
            wave_period_ewma = period_s;
          } else {
            wave_period_ewma = 0.8f * wave_period_ewma + 0.2f * period_s;
          }
          imu_wave_period_sec = wave_period_ewma;
        }
      }
      wave_last_crossing_time = now;
    }

    // Invalidate if no crossing detected for >30s (boat stopped, no waves, or signal lost)
    if (wave_last_crossing_time > 0 && (now - wave_last_crossing_time) > 30000) {
      imu_wave_period_sec = -1.0f;
      wave_period_ewma = -1.0f;
      wave_last_crossing_time = 0;
    }
  }

  // Process gyro ring buffer — cap per call as a safety net
  uint16_t gyro_cap = 0;
  while (imuRingBuffer->gyro_tail != imuRingBuffer->gyro_head && gyro_cap < 20) {
    gyro_cap++;
    IMUSample *s = &imuRingBuffer->gyro[imuRingBuffer->gyro_tail];

    // Calculate time delta (uses separate gyro tracker, not shared with accel)
    uint64_t now_us = s->timestamp_us;
    uint64_t dt_us = (imuWindow->lastGyroUpdateTime_us > 0)
                       ? (now_us - imuWindow->lastGyroUpdateTime_us)
                       : 0;

    // Convert to engineering units with full axis permutation + sign remapping
    const AxisRemap &r = axisRemap[imuMountOrientation];
    float raw_g[3] = { (float)s->x, (float)s->y, (float)s->z };
    float gx = raw_g[r.src[3]] * GYRO_SCALE * r.sign[3];  // vessel pitch rate
    float gy = raw_g[r.src[4]] * GYRO_SCALE * r.sign[4];  // vessel heel rate
    float gz = raw_g[r.src[5]] * GYRO_SCALE * r.sign[5];  // vessel yaw rate

    // IMU zero capture — accumulate raw (pre-bias) gyro to measure rest bias
    if (imuZeroInProgress && imuZeroGyroN < IMU_ZERO_TARGET) {
      imuZeroGxSum += gx; imuZeroGySum += gy; imuZeroGzSum += gz;
      imuZeroGyroN++;
    }
    // Apply stored rest bias to live gyro
    gx -= imuGxBias; gy -= imuGyBias; gz -= imuGzBias;

    // Scale to integers (12.34 dps → 1234)
    int32_t gx_scaled = (int32_t)(gx * 100.0f);
    int32_t gy_scaled = (int32_t)(gy * 100.0f);
    int32_t gz_scaled = (int32_t)(gz * 100.0f);

    // Update window statistics
    if (dt_us > 0 && dt_us < 100000) {  // Sanity check
      if (gx_scaled < imuWindow->gyro_x_min) imuWindow->gyro_x_min = gx_scaled;
      if (gx_scaled > imuWindow->gyro_x_max) imuWindow->gyro_x_max = gx_scaled;
      imuWindow->gyro_x_area_v_us += (int64_t)gx_scaled * dt_us;
      imuWindow->gyro_x_valid_us += dt_us;

      if (gy_scaled < imuWindow->gyro_y_min) imuWindow->gyro_y_min = gy_scaled;
      if (gy_scaled > imuWindow->gyro_y_max) imuWindow->gyro_y_max = gy_scaled;
      imuWindow->gyro_y_area_v_us += (int64_t)gy_scaled * dt_us;
      imuWindow->gyro_y_valid_us += dt_us;

      if (gz_scaled < imuWindow->gyro_z_min) imuWindow->gyro_z_min = gz_scaled;
      if (gz_scaled > imuWindow->gyro_z_max) imuWindow->gyro_z_max = gz_scaled;
      imuWindow->gyro_z_area_v_us += (int64_t)gz_scaled * dt_us;
      imuWindow->gyro_z_valid_us += dt_us;
    }

    // Store current raw values
    imu_gyro_x_raw = gx;
    imu_gyro_y_raw = gy;
    imu_gyro_z_raw = gz;
    imu_yaw_rate_dps = gz;

    // Update complementary filter with latest accel + this gyro sample
    float dt_sec = dt_us / 1000000.0f;
    if (dt_sec > 0 && dt_sec < 1.0f) {  // Sanity check
      complementaryFilter(imu_accel_x_raw, imu_accel_y_raw, imu_accel_z_raw,
                          gx, gy, dt_sec);

      // Update heel/pitch window stats (scaled by 100: 12.34° → 1234)
      int32_t heel_scaled = (int32_t)(cf_heel * 100.0f);
      int32_t pitch_scaled = (int32_t)(cf_pitch * 100.0f);

      if (heel_scaled < imuWindow->heel_min) imuWindow->heel_min = heel_scaled;
      if (heel_scaled > imuWindow->heel_max) imuWindow->heel_max = heel_scaled;
      imuWindow->heel_area_v_us += (int64_t)heel_scaled * dt_us;
      imuWindow->heel_valid_us += dt_us;

      if (pitch_scaled < imuWindow->pitch_min) imuWindow->pitch_min = pitch_scaled;
      if (pitch_scaled > imuWindow->pitch_max) imuWindow->pitch_max = pitch_scaled;
      imuWindow->pitch_area_v_us += (int64_t)pitch_scaled * dt_us;
      imuWindow->pitch_valid_us += dt_us;
    }
    cf_lastUpdate = s->timestamp_us;

    imuWindow->lastGyroUpdateTime_us = now_us;  // separate from accel tracker (a shared one would zero the gyro dt)
    imuRingBuffer->gyro_tail = (imuRingBuffer->gyro_tail + 1) % GYRO_RING_SIZE;
  }

  // Finalize IMU zero: normally once both windows hit the target (~2s @104Hz). The timeout
  // margin guarantees the capture always resolves — if samples arrive slowly it finalizes on
  // whatever it gathered (provided the IMU_ZERO_MIN floor), otherwise it aborts cleanly. Either
  // way settingsDirty fires so the CSV3 echo refreshes the dashboard "calculating" label at once.
  if (imuZeroInProgress) {
    bool targetMet = (imuZeroAccelN >= IMU_ZERO_TARGET && imuZeroGyroN >= IMU_ZERO_TARGET);
    bool timedOut  = (millis() - imuZeroStartMs >= IMU_ZERO_TIMEOUT_MS);
    if (targetMet || timedOut) {
      if (imuZeroAccelN >= IMU_ZERO_MIN && imuZeroGyroN >= IMU_ZERO_MIN) {
        float ax0 = imuZeroAxSum / imuZeroAccelN;
        float ay0 = imuZeroAySum / imuZeroAccelN;
        float az0 = imuZeroAzSum / imuZeroAccelN;
        imuHeelOffsetDeg = atan2(ay0, sqrt(ax0 * ax0 + az0 * az0)) * 180.0f / PI;
        imuPitchOffsetDeg = atan2(-ax0, sqrt(ay0 * ay0 + az0 * az0)) * 180.0f / PI;
        imuGxBias = imuZeroGxSum / imuZeroGyroN;
        imuGyBias = imuZeroGySum / imuZeroGyroN;
        imuGzBias = imuZeroGzSum / imuZeroGyroN;

        DynamicJsonDocument zdoc(256);
        zdoc["heel_offset_deg"] = imuHeelOffsetDeg;
        zdoc["pitch_offset_deg"] = imuPitchOffsetDeg;
        zdoc["gx_bias"] = imuGxBias;
        zdoc["gy_bias"] = imuGyBias;
        zdoc["gz_bias"] = imuGzBias;
        String zout;
        serializeJson(zdoc, zout);
        settingWrite(NK_imu_zero, zout.c_str());

        cf_heel = 0; cf_pitch = 0;  // snap filter to new level reference (feels instant)
        queueConsoleMessage("IMU ZERO: captured (heel " + String(imuHeelOffsetDeg, 1) +
                            "°, pitch " + String(imuPitchOffsetDeg, 1) +
                            (targetMet ? "°)" : "°, partial — timed out)"));
      } else {
        // Not enough samples even after the timeout (IMU barely streaming) — abort, keep old offsets.
        queueConsoleMessage("IMU ZERO: aborted — too few samples (is the IMU streaming?)");
      }
      imuZeroInProgress = false;
      settingsDirty = true;   // fire CSV3 now so the echo lands ~immediately, not on the 60s heartbeat
    }
  }

  imu_heel_deg = cf_heel;
  imu_pitch_deg = cf_pitch;
  wmIgnUpdate(wmIgn_heel,  imu_heel_deg);   // ignition-cycle watermarks
  wmIgnUpdate(wmIgn_pitch, imu_pitch_deg);

  // Update 60s rolling window (called at ~1Hz from main loop)
  static unsigned long last60sUpdate = 0;
  if (now - last60sUpdate >= 1000) {
    last60sUpdate = now;

    rolling60s.heel[rolling60s.index] = cf_heel;
    rolling60s.pitch[rolling60s.index] = cf_pitch;
    rolling60s.index = (rolling60s.index + 1) % ROLLING_WINDOW_SIZE;
    if (rolling60s.count < ROLLING_WINDOW_SIZE) rolling60s.count++;

    // Compute 60s change/deviation metrics
    if (rolling60s.count > 10) {  // Need some samples
      float heel_min_60s = 999.0, heel_max_60s = -999.0, heel_sum_60s = 0;
      float pitch_min_60s = 999.0, pitch_max_60s = -999.0, pitch_sum_60s = 0;

      for (uint16_t i = 0; i < rolling60s.count; i++) {
        if (rolling60s.heel[i] < heel_min_60s) heel_min_60s = rolling60s.heel[i];
        if (rolling60s.heel[i] > heel_max_60s) heel_max_60s = rolling60s.heel[i];
        heel_sum_60s += rolling60s.heel[i];

        if (rolling60s.pitch[i] < pitch_min_60s) pitch_min_60s = rolling60s.pitch[i];
        if (rolling60s.pitch[i] > pitch_max_60s) pitch_max_60s = rolling60s.pitch[i];
        pitch_sum_60s += rolling60s.pitch[i];
      }

      float heel_mean_60s = heel_sum_60s / rolling60s.count;
      float pitch_mean_60s = pitch_sum_60s / rolling60s.count;

      // Store as scaled integers (12.34° → 1234)
      imuWindow->heel_change_60s = (int32_t)((heel_max_60s - heel_min_60s) * 100.0f);
      imuWindow->pitch_change_60s = (int32_t)((pitch_max_60s - pitch_min_60s) * 100.0f);

      float heel_dev = max(abs(heel_max_60s - heel_mean_60s), abs(heel_min_60s - heel_mean_60s));
      float pitch_dev = max(abs(pitch_max_60s - pitch_mean_60s), abs(pitch_min_60s - pitch_mean_60s));

      imuWindow->heel_deviation_60s = (int32_t)(heel_dev * 100.0f);
      imuWindow->pitch_deviation_60s = (int32_t)(pitch_dev * 100.0f);

      imu_heel_change_60s = heel_max_60s - heel_min_60s;
      imu_heel_deviation_60s = heel_dev;
      imu_pitch_change_60s = pitch_max_60s - pitch_min_60s;
      imu_pitch_deviation_60s = pitch_dev;
    }

    // --- 120-second ring buffer (same 1s tick) ---
    rolling120s.heel[rolling120s.index]    = cf_heel;
    rolling120s.pitch[rolling120s.index]   = cf_pitch;
    // Store compass heading if fresh; -1.0 sentinel means stale/unavailable
    rolling120s.heading[rolling120s.index] = IS_STALE(IDX_HEADING_NMEA) ? -1.0f : HeadingNMEA;
    rolling120s.index = (rolling120s.index + 1) % ROLLING_WINDOW_120;
    if (rolling120s.count < ROLLING_WINDOW_120) rolling120s.count++;

    if (rolling120s.count > 30) {
      // Roll deviation — peak departure from 2-min mean
      float h_min = 999.0f, h_max = -999.0f, h_sum = 0;
      float p_min = 999.0f, p_max = -999.0f, p_sum = 0;
      for (uint16_t i = 0; i < rolling120s.count; i++) {
        if (rolling120s.heel[i]  < h_min) h_min = rolling120s.heel[i];
        if (rolling120s.heel[i]  > h_max) h_max = rolling120s.heel[i];
        h_sum += rolling120s.heel[i];
        if (rolling120s.pitch[i] < p_min) p_min = rolling120s.pitch[i];
        if (rolling120s.pitch[i] > p_max) p_max = rolling120s.pitch[i];
        p_sum += rolling120s.pitch[i];
      }
      float h_mean = h_sum / rolling120s.count;
      float p_mean = p_sum / rolling120s.count;
      imu_heel_deviation_120s  = max(abs(h_max - h_mean), abs(h_min - h_mean));
      imu_pitch_deviation_120s = max(abs(p_max - p_mean), abs(p_min - p_mean));

      // Heading swing — wrap-aware using mean-vector approach (handles 0°/360° boundary)
      float sin_sum = 0, cos_sum = 0;
      int valid_hd = 0;
      for (uint16_t i = 0; i < rolling120s.count; i++) {
        float hd = rolling120s.heading[i];
        if (hd >= 0) {  // -1 sentinel = stale
          float hd_rad = hd * (float)(M_PI / 180.0);
          sin_sum += sinf(hd_rad);
          cos_sum += cosf(hd_rad);
          valid_hd++;
        }
      }
      if (valid_hd > 10) {
        float mean_rad = atan2f(sin_sum, cos_sum);
        float max_dev = 0;
        for (uint16_t i = 0; i < rolling120s.count; i++) {
          float hd = rolling120s.heading[i];
          if (hd >= 0) {
            float diff = hd * (float)(M_PI / 180.0) - mean_rad;
            while (diff >  (float)M_PI) diff -= 2.0f * (float)M_PI;
            while (diff < -(float)M_PI) diff += 2.0f * (float)M_PI;
            if (fabsf(diff) > max_dev) max_dev = fabsf(diff);
          }
        }
        imu_heading_swing_120s = max_dev * (float)(180.0 / M_PI) * 2.0f;  // peak-to-peak
      } else {
        imu_heading_swing_120s = -1.0f;  // not enough compass data
      }
    }
  }

  // Capsize/pitchpole detection
  // _triggered arms the 1s timer; _reported latches true after the message fires
  // and prevents re-firing while the angle stays above threshold. Both flags reset
  // only when the angle drops back below threshold (one message per event).
  static bool capsize_triggered = false;
  static bool capsize_reported = false;
  static bool pitchpole_triggered = false;
  static bool pitchpole_reported = false;
  static unsigned long capsize_start = 0;
  static unsigned long pitchpole_start = 0;

  if (abs(cf_heel) > CAPSIZE_THRESHOLD_DEG) {
    if (!capsize_triggered) {
      capsize_triggered = true;
      capsize_start = now;
    } else if (!capsize_reported && (now - capsize_start > 1000)) {  // fires once per event
      imu_capsize_count++;
      queueConsoleMessageF("CAPSIZE EVENT: %.1f deg", cf_heel);
      saveNVSDataFull();  // boat may lose power — commit lifetime counter now (sync)
      capsize_reported = true;
    }
  } else {
    capsize_triggered = false;
    capsize_reported = false;
  }

  if (abs(cf_pitch) > PITCHPOLE_THRESHOLD_DEG) {
    if (!pitchpole_triggered) {
      pitchpole_triggered = true;
      pitchpole_start = now;
    } else if (!pitchpole_reported && (now - pitchpole_start > 1000)) {  // fires once per event
      imu_pitchpole_count++;
      queueConsoleMessageF("PITCHPOLE EVENT: %.1f deg", cf_pitch);
      saveNVSDataFull();  // boat may lose power — commit lifetime counter now (sync)
      pitchpole_reported = true;
    }
  } else {
    pitchpole_triggered = false;
    pitchpole_reported = false;
  }

  if (abs(cf_heel) > imu_heel_max_lifetime) {
    imu_heel_max_lifetime = abs(cf_heel);
  }
  if (abs(cf_pitch) > imu_pitch_max_lifetime) {
    imu_pitch_max_lifetime = abs(cf_pitch);
  }

  // Sea state minute bucketing — tick every 60s
  // Bucket by MSI score: gentle<10, moderate<30, rough<70, extreme>=70
  // Moving vs stationary by SOGNMEA (knots): moving if >= 1.5 kts
  static unsigned long seaStateTick_ms = 0;
  if (now - seaStateTick_ms >= 60000) {
    seaStateTick_ms = now;
    bool moving = (SOGNMEA >= 1.5f);
    if (imu_msi_score < 10.0f) {
      if (moving) imu_min_moving_gentle++;  else imu_min_stat_gentle++;
    } else if (imu_msi_score < 30.0f) {
      if (moving) imu_min_moving_moderate++; else imu_min_stat_moderate++;
    } else if (imu_msi_score < 70.0f) {
      if (moving) imu_min_moving_rough++;   else imu_min_stat_rough++;
    } else {
      if (moving) imu_min_moving_extreme++; else imu_min_stat_extreme++;
    }
  }

  // Anchorage comfort — always computed so JS can show a grayed value underway; contextually meaningful only at anchor
  // Roll and pitch only — at anchor there are no underway-style slams; rocking and hobby-horsing are the discomfort drivers
  {
    float roll_penalty  = min(imu_heel_deviation_60s  / 12.0f, 1.0f) * 65.0f;  // 12 deg heel dev -> full penalty
    float pitch_penalty = min(imu_pitch_deviation_60s /  8.0f, 1.0f) * 35.0f;  //  8 deg pitch dev -> full penalty
    imu_anchorage_comfort = constrain(100.0f - roll_penalty - pitch_penalty, 0.0f, 100.0f);
  }
}


// OTA updates + cloud auth-token handling
void initializeDeviceId() {
  // STEP 1: Always read real chip ID first
  chipid = ESP.getEfuseMac();
  deviceIdUpper = (uint32_t)(chipid >> 32);
  deviceIdLower = (uint32_t)(chipid & 0xFFFFFFFF);

  // STEP 2: TESTING OVERRIDE - uncomment to fake device ID
  // Device UID spoofing conflicts with OTA updates if the hardcoded override values have changed since
  // factory partition was programmed. During OTA, the device temporarily boots to factory partition with the old UID,
  // triggering token clearance. Workarounds exist but aren't worth the complexity—just re-register after OTA if you've
  // changed the spoofed UID since initially programming the factory partition.
  //deviceIdUpper = 0xBBBBBBBB;
  //deviceIdLower = 0xAAAAAAAA;

  // STEP 3: Build string representation from the (possibly overridden) values
  snprintf(device_id_hex, sizeof(device_id_hex), "%08X%08X",
           (unsigned int)deviceIdUpper,
           (unsigned int)deviceIdLower);
}
String getDeviceId() {
  return String(device_id_hex);
}
void checkDeviceUIDChange() {
  currentUID = String(device_id_hex);

  nvs_handle_t handle;
  nvs_open("cloud", NVS_READWRITE, &handle);

  size_t len = 0;
  String lastUID = "";
  esp_err_t err = nvs_get_str(handle, "lastUID", NULL, &len);
  if (err == ESP_OK && len > 0) {
    char *buf = (char *)malloc(len);
    if (!buf) {
      Serial.println("checkDeviceUIDChange: malloc failed for lastUID buffer - skipping read");
    } else {
      nvs_get_str(handle, "lastUID", buf, &len);
      lastUID = String(buf);
      free(buf);
    }
  }

  Serial.println("=== checkDeviceUIDChange ===");
  Serial.print("Current UID: ");
  Serial.println(currentUID);
  Serial.print("Last UID: ");
  Serial.println(lastUID);

  if (lastUID != currentUID && lastUID.length() > 0) {
    Serial.println("Device UID changed - clearing token");
    nvs_erase_key(handle, "authToken");
    nvs_commit(handle);
    authToken = "";
    isRegistered = false;
    Serial.println("Auth token cleared");
  } else {
    Serial.println("UID unchanged, keeping token");
  }

  nvs_set_str(handle, "lastUID", currentUID.c_str());
  nvs_commit(handle);
  nvs_close(handle);
}
bool base64Decode(const String &input, uint8_t *output, size_t outputSize, size_t *decodedLength) {
  size_t inputLen = input.length();
  size_t expectedLen = (inputLen * 3) / 4;
  if (expectedLen > outputSize) {
    Serial.printf("Base64 decode: output buffer too small (%d needed, %d available)\n", expectedLen, outputSize);
    return false;
  }

  int ret = mbedtls_base64_decode(output, outputSize, decodedLength,
                                  (const unsigned char *)input.c_str(), inputLen);

  if (ret != 0) {
    Serial.printf("Base64 decode failed: %d\n", ret);
    return false;
  }

  return true;
}
// Verify firmware signature using RSA public key
bool verifyPackageSignature(uint8_t *packageData, size_t packageSize, const String &signatureBase64) {
  Serial.println("🛡️ Starting package signature verification...");

  mbedtls_pk_context pk;
  uint8_t signature[520];  // Buffer for RSA-4096 signatures
  size_t sigLength;

  if (!base64Decode(signatureBase64, signature, sizeof(signature), &sigLength)) {
    Serial.println("SECURITY: Failed to decode signature");
    return false;
  }

  if (sigLength != 512) {
    Serial.printf("SECURITY: Invalid signature length: %d (expected 512)\n", sigLength);
    return false;
  }

  // Hash the complete package using SHA-256
  uint8_t hash[32];
  mbedtls_md_context_t ctx;
  mbedtls_md_init(&ctx);

  const mbedtls_md_info_t *info = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
  if (info == NULL) {
    Serial.println("SECURITY: Failed to get SHA-256 info");
    mbedtls_md_free(&ctx);
    return false;
  }

  int ret = mbedtls_md_setup(&ctx, info, 0);
  if (ret != 0) {
    Serial.printf("SECURITY: Failed to setup MD context: %d\n", ret);
    mbedtls_md_free(&ctx);
    return false;
  }

  mbedtls_md_starts(&ctx);
  mbedtls_md_update(&ctx, packageData, packageSize);
  mbedtls_md_finish(&ctx, hash);
  mbedtls_md_free(&ctx);

  Serial.println("Package hash computed successfully");

  mbedtls_pk_init(&pk);

  ret = mbedtls_pk_parse_public_key(&pk, (const unsigned char *)OTA_PUBLIC_KEY, strlen(OTA_PUBLIC_KEY) + 1);
  if (ret != 0) {
    Serial.printf("SECURITY: Failed to parse public key: -0x%04x\n", -ret);
    mbedtls_pk_free(&pk);
    return false;
  }

  ret = mbedtls_pk_verify(&pk, MBEDTLS_MD_SHA256, hash, 32, signature, sigLength);
  mbedtls_pk_free(&pk);

  if (ret == 0) {
    Serial.println("SECURITY: Package signature verification PASSED ✅");
    return true;
  } else {
    Serial.printf("SECURITY: Package signature verification FAILED ❌ (error -0x%04x)\n", -ret);
    return false;
  }
}
bool initStreamingExtractor(StreamingExtractor *extractor) {
  memset(extractor, 0, sizeof(StreamingExtractor));

  extractor->inTarHeader = true;
  extractor->tarHeaderPos = 0;
  extractor->inPadding = false;
  extractor->paddingRemaining = 0;

  // Initialize hash for signature verification
  mbedtls_md_init(&extractor->hashCtx);
  const mbedtls_md_info_t *info = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
  int mdRet = mbedtls_md_setup(&extractor->hashCtx, info, 0);
  if (mdRet != 0) {
    Serial.printf("OTA: mbedtls_md_setup failed: %d\n", mdRet);
    mbedtls_md_free(&extractor->hashCtx);
    return false;
  }
  mbedtls_md_starts(&extractor->hashCtx);
  extractor->hashStarted = true;

  Serial.println("✅ Streaming tar extractor initialized");
  return true;
}
bool parseTarHeader(StreamingExtractor *extractor) {
  // Validate header first (but skip the all-zeros check since it's handled in processDataChunk)
  if (memcmp(extractor->tarHeader + 257, "ustar", 5) != 0) {
    Serial.println("❌ Invalid tar header: missing ustar magic");
    Serial.print("Header start: ");
    for (int i = 0; i < 20; i++) {
      Serial.printf("%02x ", extractor->tarHeader[i]);
    }
    Serial.println();
    return false;
  }

  Serial.println("✅ Valid tar header with ustar magic");

  // Extract filename (starts at offset 0, max 100 chars)
  char rawFilename[101];
  memcpy(rawFilename, extractor->tarHeader, 100);
  rawFilename[100] = '\0';

  // Check file type (position 156 in tar header)
  char typeFlag = extractor->tarHeader[156];

  String filename = String(rawFilename);
  filename.trim();

  if (filename.startsWith("./")) {
    filename = filename.substring(2);
  }

  // Skip directories
  if (typeFlag == '5' || filename.endsWith("/")) {
    Serial.println("📁 Skipping directory entry");
    extractor->currentFileName = "";
    extractor->currentFileSize = 0;
    extractor->currentFilePos = 0;
    extractor->isCurrentFileFirmware = false;
    return true;
  }

  // Only process regular files
  if (typeFlag != '0' && typeFlag != '\0') {
    Serial.printf("⚠️  Skipping file type '%c' for: %s\n", typeFlag, filename.c_str());
    extractor->currentFileName = "";
    extractor->currentFileSize = 0;
    extractor->currentFilePos = 0;
    extractor->isCurrentFileFirmware = false;
    return true;
  }

  extractor->currentFileName = filename;

  // Extract file size (starts at offset 124, 12 chars, octal)
  char sizeStr[13];
  memcpy(sizeStr, extractor->tarHeader + 124, 12);
  sizeStr[12] = '\0';

  extractor->currentFileSize = 0;
  for (int i = 0; i < 12 && sizeStr[i] != '\0' && sizeStr[i] != ' '; i++) {
    if (sizeStr[i] >= '0' && sizeStr[i] <= '7') {
      extractor->currentFileSize = extractor->currentFileSize * 8 + (sizeStr[i] - '0');
    }
  }

  extractor->currentFilePos = 0;

  extractor->isCurrentFileFirmware = extractor->currentFileName.equals("firmware.bin");

  Serial.printf("📁 Found file: %s (%d bytes) type='%c'\n",
                extractor->currentFileName.c_str(), extractor->currentFileSize, typeFlag);

  // Route file to appropriate destination
  if (extractor->currentFileName.equals("firmware.bin")) {
    extractor->otaPartition = esp_partition_find_first(ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_APP_OTA_0, NULL);
    esp_ota_begin(extractor->otaPartition, extractor->currentFileSize, &extractor->otaHandle);
    extractor->otaStarted = true;
    extractor->isCurrentFileFirmware = true;
  } else if (extractor->currentFileName.indexOf('.') > 0) {
    // Mount prod_fs for web file updates
    if (!extractor->prodFSMounted) {
      webFS.end();
      if (webFS.begin(true, "/web", 10, "prod_fs")) {
        extractor->prodFSMounted = true;
        Serial.println("OTA: Mounted prod_fs for web file updates");
      } else {
        Serial.println("OTA: Failed to mount prod_fs");
        return false;
      }
    }
    String filePath = "/" + extractor->currentFileName;
    extractor->currentWebFile = webFS.open(filePath, "w");
  }
  return true;
}
bool processDataChunk(StreamingExtractor *extractor, uint8_t *data, size_t dataSize) {
  size_t processed = 0;

  while (processed < dataSize) {
    if (extractor->inPadding) {
      size_t toSkip = min(extractor->paddingRemaining, dataSize - processed);
      processed += toSkip;
      extractor->paddingRemaining -= toSkip;

      if (extractor->paddingRemaining == 0) {
        extractor->inPadding = false;
        extractor->inTarHeader = true;
        extractor->tarHeaderPos = 0;
        Serial.println("✅ Padding skipped, ready for next header");
      }

    } else if (extractor->inTarHeader) {
      size_t headerRemaining = 512 - extractor->tarHeaderPos;
      size_t toCopy = min(headerRemaining, dataSize - processed);

      memcpy(extractor->tarHeader + extractor->tarHeaderPos, data + processed, toCopy);
      extractor->tarHeaderPos += toCopy;
      processed += toCopy;

      if (extractor->tarHeaderPos >= 512) {
        // Complete header received - check for end of archive
        bool allZeros = true;
        for (int i = 0; i < 512; i++) {
          if (extractor->tarHeader[i] != 0) {
            allZeros = false;
            break;
          }
        }

        if (allZeros) {
          Serial.println("✅ End of tar archive detected - extraction complete!");
          return true;  // SUCCESS! Archive is complete
        }

        if (!parseTarHeader(extractor)) {
          Serial.println("❌ Failed to parse tar header");
          return false;  // Actual parsing error
        }
        extractor->inTarHeader = false;
      }

    } else {
      // Reading file data
      size_t fileRemaining = extractor->currentFileSize - extractor->currentFilePos;
      size_t dataRemaining = dataSize - processed;
      size_t toWrite = min(fileRemaining, dataRemaining);

      if (toWrite > 0 && extractor->currentFileName.length() > 0) {
        // Only process if we have a valid filename (not a skipped entry)
        if (extractor->isCurrentFileFirmware && extractor->otaStarted) {
          esp_err_t err = esp_ota_write(extractor->otaHandle, data + processed, toWrite);
          if (err != ESP_OK) {
            Serial.printf("❌ OTA write failed: %s\n", esp_err_to_name(err));
            return false;
          }

        } else if (extractor->currentWebFile) {
          size_t written = extractor->currentWebFile.write(data + processed, toWrite);
          if (written != toWrite) {
            Serial.printf("❌ Web file write failed: %d/%d bytes\n", written, toWrite);
          }
        }
      }

      if (toWrite > 0) {
        extractor->currentFilePos += toWrite;
        processed += toWrite;
      }

      if (extractor->currentFilePos >= extractor->currentFileSize) {
        if (extractor->isCurrentFileFirmware && extractor->otaStarted) {
          Serial.println("✅ Firmware extraction completed");
        } else if (extractor->currentWebFile) {
          extractor->currentWebFile.close();
          Serial.printf("✅ Web file completed: %s (%d bytes)\n",
                        extractor->currentFileName.c_str(), extractor->currentFileSize);
        } else if (extractor->currentFileName.length() > 0) {
          Serial.printf("✅ Skipped file completed: %s\n", extractor->currentFileName.c_str());
        }

        // Calculate padding (files are padded to 512-byte boundaries)
        size_t padding = (512 - (extractor->currentFileSize % 512)) % 512;

        if (padding > 0) {
          Serial.printf("📏 File complete, need to skip %d padding bytes\n", padding);
          extractor->inPadding = true;
          extractor->paddingRemaining = padding;
        } else {
          Serial.println("📏 File complete, no padding needed");
          extractor->inTarHeader = true;
          extractor->tarHeaderPos = 0;
        }
      }
    }
  }

  return true;
}
void prepareForOTA() {
  otaInProgress = true;
  // Close EventSource FIRST (before any heap measurements)
  Serial.println("Closing EventSource connections...");
  events.close();
  delay(100);

  Serial.println("🧹 Preparing system for OTA - freeing memory...");
  Serial.printf("Heap BEFORE cleanup: %u bytes\n", ESP.getFreeHeap());

  if (httpsTaskHandle != NULL) {
    esp_task_wdt_delete(httpsTaskHandle);
    vTaskDelete(httpsTaskHandle);
    httpsTaskHandle = NULL;
  }
  if (tempTaskHandle != NULL) {
    esp_task_wdt_delete(tempTaskHandle);
    vTaskDelete(tempTaskHandle);
    tempTaskHandle = NULL;
  }

  delay(1000);
  esp_task_wdt_reset();

  Serial.printf("Heap AFTER cleanup: %u bytes\n", ESP.getFreeHeap());
  Serial.printf("Internal free: %u, largest: %u\n",
                heap_caps_get_free_size(MALLOC_CAP_INTERNAL),
                heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL));
  Serial.println("Ready for OTA download!");
}
bool validateHeapIntegrity() {
  // Snapshot: use heap_caps_* for reliable internal vs PSRAM separation
  size_t freeInternal = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
  size_t largestInternal = heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL);

  size_t freePsram = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
  size_t totalPsram = ESP.getPsramSize();  // Arduino-reported PSRAM size

  // Optional: total internal size (may be 0 on some builds; keep for debug only)
  size_t totalInternal = heap_caps_get_total_size(MALLOC_CAP_INTERNAL);

  // Sanity checks (lightweight)
  if (totalInternal > 0 && freeInternal > totalInternal) {
    Serial.printf("ERROR: Heap corruption? freeInternal=%lu > totalInternal=%lu\n",
                  (unsigned long)freeInternal, (unsigned long)totalInternal);
    return false;
  }
  if (totalPsram > 0 && freePsram > totalPsram) {
    Serial.printf("ERROR: PSRAM corruption? freePsram=%lu > totalPsram=%lu\n",
                  (unsigned long)freePsram, (unsigned long)totalPsram);
    return false;
  }
  if (freeInternal > 0 && largestInternal > freeInternal) {
    Serial.printf("ERROR: Heap corruption? largestInternal=%lu > freeInternal=%lu\n",
                  (unsigned long)largestInternal, (unsigned long)freeInternal);
    return false;
  }

  // Print (single-line where possible to reduce truncation risk)
  Serial.printf("HEAP: internal free=%luB, largest=%luB, total=%luB\n",
                (unsigned long)freeInternal,
                (unsigned long)largestInternal,
                (unsigned long)totalInternal);

  Serial.printf("PSRAM: free=%luB, total=%luB\n",
                (unsigned long)freePsram,
                (unsigned long)totalPsram);

  // Also print Arduino wrappers for correlation (not used for decisions)
  Serial.printf("ARDUINO HEAP: free=%luB, size=%luB, maxAlloc=%luB\n",
                (unsigned long)ESP.getFreeHeap(),
                (unsigned long)ESP.getHeapSize(),
                (unsigned long)ESP.getMaxAllocHeap());

  return true;
}
void performStreamingOTAUpdate(const UpdateInfo &updateInfo, const String &signatureBase64, WiFiClientSecure &client) {
  //Called by: performOTAUpdate() when on factory partition
  //Purpose: Actually downloads and installs the firmware
  //Action:
  //1) Downloads firmware from updateInfo.firmwareUrl
  //2) Streams and extracts TAR file
  //3) Verifies signature
  // 4) Installs to OTA partition

  // DECLARE ALL VARIABLES AT TOP (before any goto statements)
  bool success = true;
  // Post-download rejection (signature/finalize). The reboot-into-current-
  // firmware these paths take is the right recovery — but without this flag
  // the cleanup branch reported it as a successful update.
  bool verifyFailed = false;
  StreamingExtractor extractor = {};
  HTTPClient http;
  unsigned long downloadStartTime = 0;
  unsigned long downloadDuration = 0;
  int contentLength = 0;
  WiFiClient *stream = nullptr;
  const size_t CHUNK_SIZE = 1024;
  uint8_t inputBuffer[CHUNK_SIZE];
  int totalDownloaded = 0;
  unsigned long lastProgress = 0;
  uint8_t hash[32];
  uint8_t signature[520];
  size_t sigLength = 0;
  mbedtls_pk_context pk;
  int ret = 0;
  int rssi = 0;
  size_t freeHeap = 0;
  bool beginSuccess = false;
  int httpCode = 0;

  Serial.println("=== STARTING STREAMING TAR UPDATE ===");
  // Rate limit ALL HTTPS operations system-wide
  esp_task_wdt_reset();
  unsigned long timeSinceLastHttps = millis() - lastHttpsOperationTime;
  if (timeSinceLastHttps < HTTPS_MIN_INTERVAL) {
    unsigned long waitTime = HTTPS_MIN_INTERVAL - timeSinceLastHttps;
    Serial.printf("RATE LIMIT: Waiting %lu ms before HTTPS\n", waitTime);
    delay(waitTime);
  }
  esp_task_wdt_reset();
  Serial.println("=== STREAMING OTA DEBUG ===");
  int wifiStatus = WiFi.status();
  Serial.print("1. WiFi status: ");
  Serial.println(wifiStatus);
  Serial.print("2. Current mode: ");
  Serial.println(currentMode);
  size_t freeHeap2 = ESP.getFreeHeap();
  Serial.print("3. Free heap2: ");
  Serial.print(freeHeap2);
  Serial.println(" bytes");
  Serial.print("4. URL length: ");
  Serial.println(updateInfo.firmwareUrl.length());
  Serial.print("5. Signature length: ");
  Serial.println(signatureBase64.length());

  // CRITICAL: Verify WiFi is connected before attempting SSL/HTTP operations
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("ERROR: WiFi not connected - cannot perform OTA update");
    queueConsoleMessage("OTA FAILED: WiFi disconnected before update started");
    goto cleanup;
  }

  if (currentMode != MODE_CLIENT) {
    Serial.println("ERROR: Not in client mode - cannot perform OTA update");
    queueConsoleMessage("OTA FAILED: Must be in client mode for updates");
    goto cleanup;
  }

  rssi = WiFi.RSSI();
  if (rssi < -76) {
    Serial.printf("OTA: WiFi too weak (%d dBm) - unsafe for firmware download\n", rssi);
    queueConsoleMessage("OTA FAILED: WiFi signal too weak");
    goto cleanup;
  }

  Serial.printf("WiFi status: %d, Signal: %d dBm\n", WiFi.status(), WiFi.RSSI());

  downloadStartTime = millis();

  if (!initStreamingExtractor(&extractor)) {
    goto cleanup;
  }
  otaHeapMark("BEFORE WiFiClientSecure");

  otaHeapMark("AFTER WiFiClientSecure");

  // setInsecure() not setCACert(): downloads go through the ota.xengineering.net
  // proxy, whose cert chain we don't pin (and it may change when re-homed). Transport
  // auth is irrelevant here — the downloaded package is RSA-signature-verified below
  // (mbedtls_pk_verify / OTA_PUBLIC_KEY), which is the real integrity guarantee.
  Serial.println("7. Setting TLS (insecure transport; payload is signature-verified)...");
  client.setInsecure();
  otaHeapMark("AFTER setInsecure");

  // CRITICAL: Set matching timeouts BEFORE connection attempt
  client.setTimeout(30000);
  client.setHandshakeTimeout(30000);  // Must match setTimeout

  Serial.println("8. Pre-flight memory check...");
  freeHeap = ESP.getFreeHeap();
  Serial.print("Free heap: ");
  Serial.print(freeHeap);
  Serial.println(" bytes");

  Serial.println("Less than 50kb is not great, worked at 46kb in the past!");

  Serial.println("9. Creating HTTPClient...");
  otaHeapMark("BEFORE HTTPClient");

  otaHeapMark("AFTER HTTPClient");

  // Add watchdog reset before expensive SSL operation
  esp_task_wdt_reset();

  if (!validateHeapIntegrity()) {
    Serial.println("ABORT: Memory subsystem corrupted");
    goto cleanup;
  }

  Serial.println("10. Attempting http.begin() with SSL...");
  // No M17-style heap pre-check here: tried in 0.0.38, broke OTA. prepareForOTA() above
  // already frees memory for TLS; let http.begin() / mbedTLS surface failures themselves.
  beginSuccess = http.begin(client, updateInfo.firmwareUrl);
  otaHeapMark("AFTER http.begin");

  if (!beginSuccess) {
    Serial.println("ABORT: http.begin() failed - SSL handshake error");
    queueConsoleMessage("OTA FAILED: Cannot establish secure connection");
    goto cleanup;
  }

  Serial.println("11. SSL handshake completed successfully");

  // Now safe to add headers
  http.addHeader("Device-ID", getDeviceId());
  http.addHeader("Current-Version", FIRMWARE_VERSION);
  http.setTimeout(60000);

  httpCode = http.GET();
  otaHeapMark("AFTER http.GET");

  if (httpCode != 200) {
    Serial.printf("Package download failed: %d (%s)\n",
                  httpCode,
                  http.errorToString(httpCode).c_str());
    Serial.printf("WiFi.status=%d RSSI=%d\n", WiFi.status(), WiFi.RSSI());
    Serial.printf("Internal free=%u largest=%u\n",
                  heap_caps_get_free_size(MALLOC_CAP_INTERNAL),
                  heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL));
    http.end();
    goto cleanup;
  }

  contentLength = http.getSize();
  if (contentLength <= 0) {
    // Missing/invalid Content-Length → while-loop never enters, hash computes over zero bytes,
    // signature verify fails with a misleading "verification FAILED" message. Fail fast instead.
    Serial.printf("ABORT: OTA server returned invalid Content-Length: %d\n", contentLength);
    queueConsoleMessage("OTA FAILED: Server returned no Content-Length");
    http.end();
    goto cleanup;
  }
  stream = http.getStreamPtr();

  // Streaming tar extraction
  totalDownloaded = 0;
  lastProgress = 0;

  while (totalDownloaded < contentLength && success) {
    if (!client.connected()) {
      Serial.println("Connection lost during download");
      success = false;
      break;
    }
    esp_task_wdt_reset();
    int available = stream->available();
    if (available > 0) {
      int toRead = min(available, (int)min(CHUNK_SIZE, (size_t)(contentLength - totalDownloaded)));
      int actualRead = stream->readBytes(inputBuffer, toRead);

      if (actualRead > 0) {
        totalDownloaded += actualRead;

        // Update hash for signature verification
        mbedtls_md_update(&extractor.hashCtx, inputBuffer, actualRead);

        // Process tar data directly (no decompression needed)
        if (!processDataChunk(&extractor, inputBuffer, actualRead)) {
          success = false;
          break;
        }

        if (millis() - lastProgress > 2000) {
          Serial.printf("Progress: %d%% (%d/%d bytes)\n",
                        (totalDownloaded * 100) / contentLength, totalDownloaded, contentLength);
          lastProgress = millis();
          esp_task_wdt_reset();
        }
      }
    } else {
      delay(10);
      esp_task_wdt_reset();
    }
  }

  http.end();

  downloadDuration = (millis() - downloadStartTime) / 1000;
  Serial.printf("SPEED: Download completed in %lu seconds\n", downloadDuration);

  if (!success) {
    Serial.println("Streaming extraction failed");
    if (extractor.otaStarted) {
      esp_ota_abort(extractor.otaHandle);
    }
    goto cleanup;
  }

  Serial.println("Streaming download and extraction completed");

  // Verify signature
  mbedtls_md_finish(&extractor.hashCtx, hash);

  if (!base64Decode(signatureBase64, signature, sizeof(signature), &sigLength)) {
    Serial.println("SECURITY: Failed to decode signature");
    if (extractor.otaStarted) esp_ota_abort(extractor.otaHandle);
    verifyFailed = true;
    goto cleanup;
  }

  mbedtls_pk_init(&pk);
  ret = mbedtls_pk_parse_public_key(&pk, (const unsigned char *)OTA_PUBLIC_KEY, strlen(OTA_PUBLIC_KEY) + 1);
  if (ret != 0) {
    Serial.printf("SECURITY: Failed to parse public key: -0x%04x\n", -ret);
    if (extractor.otaStarted) esp_ota_abort(extractor.otaHandle);
    mbedtls_pk_free(&pk);
    verifyFailed = true;
    goto cleanup;
  }

  ret = mbedtls_pk_verify(&pk, MBEDTLS_MD_SHA256, hash, 32, signature, sigLength);
  mbedtls_pk_free(&pk);

  if (ret != 0) {
    Serial.printf("SECURITY: Signature verification FAILED (error -0x%04x)\n", -ret);
    if (extractor.otaStarted) esp_ota_abort(extractor.otaHandle);
    verifyFailed = true;
    goto cleanup;
  }

  Serial.println("SECURITY: Signature verification PASSED");

  if (extractor.otaStarted) {
    esp_err_t err = esp_ota_end(extractor.otaHandle);
    if (err != ESP_OK) {
      Serial.printf("OTA end failed: %s\n", esp_err_to_name(err));
      verifyFailed = true;
      goto cleanup;
    }

    err = esp_ota_set_boot_partition(extractor.otaPartition);
    if (err != ESP_OK) {
      Serial.printf("Set boot partition failed: %s\n", esp_err_to_name(err));
      verifyFailed = true;
      goto cleanup;
    }
  }

  Serial.println("=== STREAMING OTA UPDATE SUCCESSFUL ===");
  Serial.printf("Updated from %s to %s\n", FIRMWARE_VERSION, updateInfo.version.c_str());

  // Clear forced update flags in Supabase after successful update
  if (hasForcedUpdate) {
    executeClearForcedUpdate();
  }
  Serial.println("Restarting in 3 seconds...");

cleanup:
  if (extractor.hashStarted) {
    mbedtls_md_free(&extractor.hashCtx);
  }
  if (extractor.currentWebFile) {
    extractor.currentWebFile.close();
  }
  if (extractor.prodFSMounted) {
    webFS.end();
  }

  if (success && extractor.otaStarted) {
    lastHttpsOperationTime = millis();
    if (verifyFailed) {
      // Same reboot as the success path (clean recovery into current firmware,
      // rejected image already aborted/never set bootable) — but say so honestly.
      Serial.println("=== OTA UPDATE FAILED AFTER DOWNLOAD (see error above) — rebooting on current firmware ===");
    }
    delay(3000);
    ESP.restart();
  } else {
    otaRestoreNormalOperation(false);
  }
}
void performOTAUpdate(const UpdateInfo &updateInfo) {
  //Purpose: Handles the partition switching and signature download
  //Action:
  //1) Downloads signature from server
  //2) Checks if running on ota_0 or factory partition
  //3) If on ota_0: switches to factory and reboots
  //4) If on factory: calls performStreamingOTAUpdate()
  Serial.println("🔒 === STARTING SECURE PACKAGE UPDATE ===");
  // Rate limit ALL HTTPS operations system-wide
  unsigned long timeSinceLastHttps = millis() - lastHttpsOperationTime;
  if (timeSinceLastHttps < HTTPS_MIN_INTERVAL) {
    unsigned long waitTime = HTTPS_MIN_INTERVAL - timeSinceLastHttps;
    Serial.printf("RATE LIMIT: Waiting %lu ms before HTTPS\n", waitTime);
    delay(waitTime);
  }
  prepareForOTA();
  // Download signature first
  WiFiClientSecure client;
  client.setInsecure();  // ota.xengineering.net proxy (unpinned chain); payload integrity comes from RSA verify, not TLS pinning
  HTTPClient http;
  Serial.printf("📥 Downloading signature from: %s\n", updateInfo.signatureUrl.c_str());
  // No 40K heap gate — see notes at firmware-download http.begin above.
  http.begin(client, updateInfo.signatureUrl);
  http.addHeader("Device-ID", getDeviceId());
  int httpCode = http.GET();

  if (httpCode != 200) {
    Serial.printf("❌ Signature download failed: %d (%s)\n",
                  httpCode,
                  http.errorToString(httpCode).c_str());
    http.end();
    otaRestoreNormalOperation(false);
    return;
  }
  String signatureBase64 = http.getString();
  http.end();
  signatureBase64.trim();
  Serial.printf("✅ Signature downloaded (%d chars)\n", signatureBase64.length());

  Serial.println("UPDATE: Starting firmware download, web interface will be unresponsive");
  //events.send("UPDATE: Downloading firmware - interface will freeze 60-90 sec", "console", millis());
  delay(3000);

  // Check if we need to restart to factory first
  const esp_partition_t *ota0_partition = esp_partition_find_first(ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_APP_OTA_0, NULL);
  const esp_partition_t *factory_partition = esp_partition_find_first(ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_APP_FACTORY, NULL);
  const esp_partition_t *running_partition = esp_ota_get_running_partition();

  if (running_partition == ota0_partition) {
    Serial.println("OTA: Currently on ota_0, switching to factory for update...");
    // events.send("OTA: Switching to factory partition for update", "console", millis());

    esp_ota_set_boot_partition(factory_partition);
    switchToFactoryWebFiles();

    Serial.println("OTA: Restarting to factory, then will download update to ota_0");
    events.send("OTA: Restarting to factory - device will reboot now", "console", millis());
    delay(3000);
    ESP.restart();
    // Execution stops here
  }

  // If we reach here, we're running from factory - proceed with update
  lastHttpsOperationTime = millis();  // UPDATE before next HTTPS call
  performStreamingOTAUpdate(updateInfo, signatureBase64, client);
}
// Returns true if update pending, fills versionOut with target version
bool checkForPendingUpdateNonBlocking(char *versionOut) {
  if (currentMode == MODE_CONFIG || currentMode == MODE_AP) {
    return false;
  }

  nvs_handle_t nvs_handle;
  if (nvs_open("update_req", NVS_READONLY, &nvs_handle) != ESP_OK) {
    return false;
  }

  uint8_t updateRequested = 0;
  size_t required_size = 64;

  bool updateFound = false;
  if (nvs_get_u8(nvs_handle, "update_flag", &updateRequested) == ESP_OK && updateRequested == 1) {
    if (nvs_get_str(nvs_handle, "target_ver", versionOut, &required_size) == ESP_OK) {
      updateFound = true;
    }
  }

  nvs_close(nvs_handle);
  return updateFound;
}
// Clear the NVS flags (call this ONLY when ready to proceed)
void clearPendingUpdateNVS() {
  nvs_handle_t clear_handle;
  if (nvs_open("update_req", NVS_READWRITE, &clear_handle) == ESP_OK) {
    nvs_erase_key(clear_handle, "update_flag");
    nvs_erase_key(clear_handle, "target_ver");
    nvs_erase_key(clear_handle, "wake_flag");
    nvs_commit(clear_handle);
    nvs_close(clear_handle);
    Serial.println("UPDATE: NVS flags cleared");
  }
}
void performOTAUpdateToVersion(const char *targetVersion) {
  //This performs an OTA update to specific version
  // Called by: The /get handler when UpdateToVersion parameter is received
  //Purpose: Initiates a targeted version update
  //Action:
  //1) Builds the artifact URLs from OTA_BASE_URL (ota.xengineering.net proxy -> Supabase) for the target version (no check.php)
  //2) Calls performOTAUpdate() which downloads, verifies signature, and installs
  // Rate limit ALL HTTPS operations system-wide
  unsigned long timeSinceLastHttps = millis() - lastHttpsOperationTime;
  if (timeSinceLastHttps < HTTPS_MIN_INTERVAL) {
    unsigned long waitTime = HTTPS_MIN_INTERVAL - timeSinceLastHttps;
    Serial.printf("RATE LIMIT: Waiting %lu ms before HTTPS\n", waitTime);
    delay(waitTime);
  }

  //Delting this- an OTA update takes priority, and we will bulldoze thru
  // if (core0Busy) {
  //   Serial.println("performOTAUpdateToVersion blocked by upload in progress");
  //   return;
  // }
  core0Busy = true;

  Serial.println("HEAP BEFORE version check:");
  Serial.printf("  Internal: %u free, %u largest\n",
                heap_caps_get_free_size(MALLOC_CAP_INTERNAL),
                heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL));

  Serial.println();
  Serial.println();
  Serial.printf("🎯 === PERFORMING TARGETED OTA UPDATE TO %s ===\n", targetVersion);
  Serial.println("OTA: Starting targeted update to version " + String(targetVersion));
  events.send("OTA: Starting update to version " + String(targetVersion), "console", millis());

  if (currentMode != MODE_CLIENT) {
    Serial.println("OTA: Cannot update - not in client mode");
    events.send("OTA: Cannot update - must be in client mode", "console", millis());
    core0Busy = false;
    return;
  }

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("OTA: Cannot update - WiFi not connected");
    events.send("OTA: Cannot update - WiFi not connected", "console", millis());
    core0Busy = false;
    return;
  }

  if (!testInternetSpeed()) {
    Serial.println("OTA: Cannot update - internet too slow or unavailable");
    events.send("OTA: Cannot update - internet connection insufficient", "console", millis());
    core0Busy = false;
    return;
  }

  int rssi = WiFi.RSSI();
  if (rssi < -76) {
    Serial.printf("OTA: WiFi too weak (%d dBm) - canceling update\n", rssi);
    events.send("OTA: WiFi signal too weak for safe download", "console", millis());
    core0Busy = false;
    return;
  }


  esp_task_wdt_reset();

  // No middleman: the target version is already known (forced-update from the
  // cloud, or a user pick in the dashboard), so we build the artifact URLs directly
  // from our stable OTA_BASE_URL (ota.xengineering.net proxy -> Supabase) instead of
  // asking check.php to resolve them. A version that doesn't exist surfaces as an
  // HTTP 404 inside performOTAUpdate(), which aborts safely (alternator stays off).
  String otaBase = String(OTA_BASE_URL);
  UpdateInfo updateInfo;
  updateInfo.hasUpdate = true;
  updateInfo.version = String(targetVersion);
  updateInfo.firmwareUrl = otaBase + "/firmware_" + String(targetVersion) + ".tar";
  updateInfo.signatureUrl = otaBase + "/firmware_" + String(targetVersion) + ".sig";
  updateInfo.changelog = "";
  updateInfo.firmwareSize = 0;  // unknown ahead of time; tar extractor streams to EOF

  Serial.printf("=== STARTING TARGETED UPDATE TO %s ===\n", targetVersion);
  Serial.println("🌐 Firmware URL: " + updateInfo.firmwareUrl);
  events.send("OTA: Beginning download of version " + String(targetVersion), "console", millis());
  performOTAUpdate(updateInfo);

  lastHttpsOperationTime = millis();
  Serial.println("HEAP AFTER OTA attempt:");
  Serial.printf("  Internal: %u free, %u largest\n",
                heap_caps_get_free_size(MALLOC_CAP_INTERNAL),
                heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL));
  core0Busy = false;
}
bool isValidTarHeader(uint8_t *header) {
  // Check for all zeros (end of archive)
  bool allZeros = true;
  for (int i = 0; i < 512; i++) {
    if (header[i] != 0) {
      allZeros = false;
      break;
    }
  }
  if (allZeros) {
    Serial.println("📦 End of tar archive detected");
    return false;  // End of archive
  }

  if (memcmp(header + 257, "ustar", 5) != 0) {
    Serial.println("❌ Invalid tar header: missing ustar magic");
    Serial.print("Header start: ");
    for (int i = 0; i < 20; i++) {
      Serial.printf("%02x ", header[i]);
    }
    Serial.println();
    return false;
  }

  Serial.println("✅ Valid tar header with ustar magic");
  return true;
}
// FORCED OTA STUFF
void executeUpdateFirmwareVersion() {
  // Called by HTTPS task on Core 0

  if (!isRegistered || authToken.length() == 0) {
    Serial.println("FW_UPDATE: Not registered");
    return;
  }

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("FW_UPDATE: WiFi not connected");
    return;
  }

  if (currentMode != MODE_CLIENT) {
    Serial.println("FW_UPDATE: Not in client mode");
    return;
  }

  int rssi = WiFi.RSSI();
  if (rssi < -76) {
    Serial.printf("FW_UPDATE: WiFi too weak (%d dBm)\n", rssi);
    return;
  }

  Serial.printf("FW_UPDATE: Reporting version %d\n", firmwareVersionInt);

  WiFiClientSecure client;
  client.setInsecure();
  client.setTimeout(8);

  HTTPClient http;
  String url = String(SUPABASE_URL) + "/functions/v1/update-firmware-version";

  if (!http.begin(client, url)) {
    Serial.println("FW_UPDATE: HTTP begin failed");
    return;
  }

  http.addHeader("Content-Type", "application/json");
  http.addHeader("Authorization", "Bearer " + String(SUPABASE_ANON_KEY));
  http.setTimeout(8000);

  String payload = "{\"token\":\"" + authToken + "\",\"firmware_version_int\":" + String(firmwareVersionInt) + "}";

  int httpCode = http.POST(payload);

  if (httpCode == 200) {
    Serial.println("FW_UPDATE: Success");
    queueConsoleMessage("FW_UPDATE: Version reported");
  } else if (httpCode < 0) {
    Serial.printf("FW_UPDATE: Failed: %s\n", http.errorToString(httpCode).c_str());
  } else {
    Serial.printf("FW_UPDATE: HTTP %d\n", httpCode);
  }

  http.end();
}
void executeCheckForcedUpdate() {
  // Called by HTTPS task on Core 0

  if (currentMode != MODE_CLIENT) {
    Serial.println("FORCED_UPDATE: Not in client mode");
    hasForcedUpdate = false;
    forcedFwVersionInt = 0;
    forcedUpdateDeadline = 0;
    return;
  }

  if (!isRegistered || authToken.length() == 0) {
    return;
  }

  if (WiFi.status() != WL_CONNECTED) {
    return;
  }

  int rssi = WiFi.RSSI();
  if (rssi < -76) {
    Serial.printf("FORCED_UPDATE: WiFi too weak (%d dBm)\n", rssi);
    return;
  }

  Serial.println("FORCED_UPDATE: Checking...");

  WiFiClientSecure client;
  client.setInsecure();
  client.setTimeout(8);

  HTTPClient http;
  String url = String(SUPABASE_URL) + "/functions/v1/check-forced-update";

  if (!http.begin(client, url)) {
    Serial.println("FORCED_UPDATE: HTTP begin failed");
    return;
  }

  http.addHeader("Content-Type", "application/json");
  http.addHeader("Authorization", "Bearer " + String(SUPABASE_ANON_KEY));
  http.setTimeout(8000);

  String payload = "{\"token\":\"" + authToken + "\"}";
  int httpCode = http.POST(payload);

  if (httpCode == 200) {
    String response = http.getString();
    Serial.println("FORCED_UPDATE: Response: " + response);

    DynamicJsonDocument doc(1024);
    DeserializationError error = deserializeJson(doc, response.c_str(), response.length());
    if (!error) {
      const char *forced_version = doc["forced_fw_version"];
      const char *deadline_str = doc["forced_update_deadline"];

      if (forced_version != nullptr && strlen(forced_version) > 0) {
        String versionStr = String(forced_version);
        int major = 0, minor = 0, patch = 0;

        int firstDot = versionStr.indexOf('.');
        int secondDot = versionStr.indexOf('.', firstDot + 1);

        if (firstDot > 0 && secondDot > firstDot) {
          major = versionStr.substring(0, firstDot).toInt();
          minor = versionStr.substring(firstDot + 1, secondDot).toInt();
          patch = versionStr.substring(secondDot + 1).toInt();
          forcedFwVersionInt = major * 10000 + minor * 100 + patch;
        }

        // Already running the forced version — the post-install clear POST must have
        // failed (network blip before reboot). Drop the flag locally so no stale
        // "Update Now" modal shows, and queue a clear to self-heal the DB row. Safe
        // because versions are monotonic (downgrades refused), so equality is the
        // only "already satisfied" case. Best-effort: a dropped clear retries next boot.
        if (forcedFwVersionInt == firmwareVersionInt) {
          Serial.println("FORCED_UPDATE: Already on forced version — clearing stale flag");
          hasForcedUpdate = false;
          forcedFwVersionInt = 0;
          forcedUpdateDeadline = 0;
          HttpsRequest clearReq = { .type = HTTPS_CLEAR_FORCED_UPDATE };
          xQueueSend(httpsQueue, &clearReq, 0);
          http.end();
          return;
        }

        if (deadline_str != nullptr && strlen(deadline_str) > 0) {
          String timestampStr = String(deadline_str);

          int year = timestampStr.substring(0, 4).toInt();
          int month = timestampStr.substring(5, 7).toInt();
          int day = timestampStr.substring(8, 10).toInt();
          int hour = timestampStr.substring(11, 13).toInt();
          int minute = timestampStr.substring(14, 16).toInt();
          int second = timestampStr.substring(17, 19).toInt();

          struct tm timeinfo;
          timeinfo.tm_year = year - 1900;
          timeinfo.tm_mon = month - 1;
          timeinfo.tm_mday = day;
          timeinfo.tm_hour = hour;
          timeinfo.tm_min = minute;
          timeinfo.tm_sec = second;
          timeinfo.tm_isdst = 0;

          forcedUpdateDeadline = mktime(&timeinfo);
          Serial.printf("FORCED_UPDATE: Deadline: %lu\n", forcedUpdateDeadline);
        }

        hasForcedUpdate = true;
        Serial.printf("FORCED_UPDATE: Version %s required\n", forced_version);
        queueConsoleMessage("⚠️ FORCED UPDATE: Version " + String(forced_version) + " required");

      } else {
        hasForcedUpdate = false;
        forcedFwVersionInt = 0;
        forcedUpdateDeadline = 0;
        Serial.println("FORCED_UPDATE: No mandatory updates");
      }
    }
  } else if (httpCode < 0) {
    Serial.printf("FORCED_UPDATE: Failed: %s\n", http.errorToString(httpCode).c_str());
  } else {
    Serial.printf("FORCED_UPDATE: HTTP %d\n", httpCode);
  }
  http.end();
}
void executeClearForcedUpdate() {
  // Called by HTTPS task on Core 0

  if (!isRegistered || authToken.length() == 0) {
    return;
  }

  if (WiFi.status() != WL_CONNECTED) {
    return;
  }

  if (currentMode != MODE_CLIENT) {
    return;
  }

  Serial.println("CLEAR_FORCED_UPDATE: Clearing flag");

  WiFiClientSecure client;
  client.setInsecure();
  client.setTimeout(8);

  HTTPClient http;
  String url = String(SUPABASE_URL) + "/functions/v1/clear-forced-update";

  if (!http.begin(client, url)) {
    Serial.println("CLEAR_FORCED_UPDATE: HTTP begin failed");
    return;
  }

  http.addHeader("Content-Type", "application/json");
  http.addHeader("Authorization", "Bearer " + String(SUPABASE_ANON_KEY));
  http.setTimeout(8000);

  String payload = "{\"token\":\"" + authToken + "\"}";
  int httpCode = http.POST(payload);

  if (httpCode == 200) {
    Serial.println("CLEAR_FORCED_UPDATE: Success");
    hasForcedUpdate = false;
    forcedFwVersionInt = 0;
    forcedUpdateDeadline = 0;
  } else {
    Serial.printf("CLEAR_FORCED_UPDATE: HTTP %d\n", httpCode);
  }

  http.end();
}

// Extract a flat top-level "field":"value" string from a JSON body (returns "" for
// missing or null). Used for the short id fields in the pending-config response; the
// big config blob itself is handled by applyImportConfig's string scan, not ArduinoJson.
static String jsonStringField(const String &body, const char *field) {
  String needle = "\"";
  needle += field;
  needle += "\"";
  int i = body.indexOf(needle);
  if (i < 0) return "";
  i += needle.length();
  while (i < (int)body.length() && (body[i] == ' ' || body[i] == '\t' || body[i] == ':')) i++;
  if (i >= (int)body.length() || body[i] != '"') return "";   // null or non-string -> none
  i++;
  int j = body.indexOf('"', i);
  if (j < 0) return "";
  return body.substring(i, j);
}

// Admin config push (boot-only): ask the cloud whether a config is queued for this device.
// If a new one is present (id != the last applied id stored in NVS), apply tier-1 settings,
// record the id, clear the server flag, and reboot so InitSystemSettings re-reads cleanly.
// The NVS id guard makes this idempotent — a stale/un-cleared flag never re-applies or loops.
void executeGetPendingConfig() {
  if (!isRegistered || authToken.length() == 0) return;
  if (WiFi.status() != WL_CONNECTED) return;
  if (currentMode != MODE_CLIENT) return;
  if (WiFi.RSSI() < -76) return;

  // Lean raw-TLS path (doCloudPOST) instead of HTTPClient over WiFiClientSecure: the combined
  // http.begin(client,url) pattern uses far more internal RAM (CLAUDE.md) and getString() can hang,
  // which made this call return -1 when contiguous RAM was tight (e.g. right after the registration
  // handshake). doCloudPOST adds the anon-key Bearer + Content-Type itself. Response holds the full
  // tier-1 config blob (exportConfigJson reserves 14 KB), so stage it in a 24 KB PSRAM scratch buffer,
  // copy to the String the parsing below expects, and free the scratch immediately.
  String response;
  int httpCode = -1;
  {
    const size_t CAP = 24576;
    char *scratch = (char *)ps_malloc(CAP);
    if (!scratch) {
      Serial.println("PENDING_CONFIG: ps_malloc failed");
      return;
    }
    String payload = "{\"token\":\"" + authToken + "\"}";
    httpCode = doCloudPOST("/functions/v1/get-pending-config", payload.c_str(), scratch, CAP);
    if (httpCode == 200) response = String(scratch);
    free(scratch);
  }
  if (httpCode != 200) {
    Serial.printf("PENDING_CONFIG: HTTP %d\n", httpCode);
    return;
  }

  String pid = jsonStringField(response, "pending_config_id");
  if (pid.length() == 0) {
    Serial.println("PENDING_CONFIG: none queued");
    return;
  }

  String lastApplied = settingExists(NK_lastAppldCfgId) ? settingRead(NK_lastAppldCfgId) : "";
  if (pid == lastApplied) {
    // Already applied — self-heal a server flag the previous boot's clear may have missed.
    Serial.println("PENDING_CONFIG: already applied; self-healing flag");
    pendingConfigClearId = pid;
    executeClearPendingConfig();
    return;
  }

  // Apply tier-1 only (never push hardware/calibration to a device remotely). applyImportConfig
  // scans the response for the embedded "config" object, so no big JSON parse is needed.
  int n = applyImportConfig(response.c_str(), 0);
  if (n < 0) {
    Serial.println("PENDING_CONFIG: malformed blob, not applied");
    return;
  }
  settingWrite(NK_lastAppldCfgId, pid.c_str());
  Serial.printf("PENDING_CONFIG: applied %d settings from config %s; rebooting\n", n, pid.c_str());
  queueConsoleMessage("Config push: applied " + String(n) + " settings, rebooting to load");

  pendingConfigClearId = pid;
  executeClearPendingConfig();   // synchronous (same task) so it completes before the reboot
  rebootRequested = true;
  rebootRequestedAt = millis();
}

// Clear the queued config server-side after applying it. Mirrors executeClearForcedUpdate;
// config_id is matched so a newer push staged in the meantime is not wiped.
void executeClearPendingConfig() {
  if (!isRegistered || authToken.length() == 0) return;
  if (WiFi.status() != WL_CONNECTED) return;
  if (currentMode != MODE_CLIENT) return;

  // Lean raw-TLS path (doCloudPOST), same reasons as executeGetPendingConfig. Response is a tiny
  // success ack, so a small stack buffer is plenty.
  String payload = "{\"token\":\"" + authToken + "\",\"config_id\":\"" + pendingConfigClearId + "\"}";
  char response[256];
  int httpCode = doCloudPOST("/functions/v1/clear-pending-config", payload.c_str(), response, sizeof(response));
  Serial.printf("CLEAR_PENDING_CONFIG: HTTP %d\n", httpCode);
}
void printPartitionInfo() {
  Serial.println("=== PARTITION SUMMARY ===");

  const esp_partition_t *running = esp_ota_get_running_partition();
  if (running) {
    Serial.printf("🚀 RUNNING APP: %s - %d bytes (%.2f MB) at 0x%X\n",
                  running->label, running->size, running->size / 1024.0 / 1024.0, running->address);
  }

  Serial.println("\n📋 ALL PARTITIONS IN TABLE:");
  esp_partition_iterator_t it = esp_partition_find(ESP_PARTITION_TYPE_ANY, ESP_PARTITION_SUBTYPE_ANY, NULL);
  int partitionCount = 0;

  while (it != NULL) {
    const esp_partition_t *partition = esp_partition_get(it);
    const char *type_str = (partition->type == ESP_PARTITION_TYPE_APP) ? "APP" : (partition->type == ESP_PARTITION_TYPE_DATA) ? "DATA"
                                                                                                                              : "OTHER";

    Serial.printf("  %s (%s) - %s: 0x%X -> 0x%X (%d bytes, %.2f MB)\n",
                  partition->label, type_str,
                  getSubtypeString(partition->type, partition->subtype).c_str(),
                  partition->address, partition->address + partition->size,
                  partition->size, partition->size / 1024.0 / 1024.0);
    partitionCount++;
    it = esp_partition_next(it);
  }
  esp_partition_iterator_release(it);
  Serial.printf("Total partitions found: %d\n", partitionCount);

  Serial.println("\n🔍 EXPECTED PARTITION VERIFICATION:");
  checkExpectedPartition("factory", ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_APP_FACTORY, 0x280000);
  checkExpectedPartition("ota_0", ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_APP_OTA_0, 0x280000);
  checkExpectedPartition("factory_fs", ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_LITTLEFS, 0x100000);
  checkExpectedPartition("prod_fs", ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_LITTLEFS, 0x100000);
  checkExpectedPartition("userdata", ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_LITTLEFS, 0x8E0000);

  Serial.println("\n🔄 OTA PARTITION STATUS:");
  const esp_partition_t *ota_partition = esp_ota_get_next_update_partition(NULL);
  if (ota_partition) {
    Serial.printf("Next OTA target: %s at 0x%X\n", ota_partition->label, ota_partition->address);
  }

  const esp_partition_t *boot_partition = esp_ota_get_boot_partition();
  if (boot_partition) {
    Serial.printf("Boot partition: %s at 0x%X\n", boot_partition->label, boot_partition->address);
  }

  Serial.println("\n💾 FLASH & MEMORY INFO:");
  Serial.printf("Flash size: %d MB\n", ESP.getFlashChipSize() / 1024 / 1024);
  Serial.printf("Current sketch size: %d bytes (%.2f MB)\n",
                ESP.getSketchSize(), ESP.getSketchSize() / 1024.0 / 1024.0);
  Serial.printf("Free sketch space: %d bytes (%.2f MB)\n",
                ESP.getFreeSketchSpace(), ESP.getFreeSketchSpace() / 1024.0 / 1024.0);
  Serial.printf("Free heap: %d bytes\n", ESP.getFreeHeap());
  Serial.println("=== END PARTITION SUMMARY ===\n");
}
bool testInternetSpeed() {
  // Don't interfere with HTTPS operations
  if (core0Busy) {
    Serial.println("Speed test skipped - upload in progress");
    return true;  // Assume connection is OK
  }

  Serial.println("========================================");
  Serial.println(">>> testInternetSpeed() ENTERED");
  Serial.println("========================================");
  Serial.flush();

  Serial.printf(">>> WiFi.status() = %d (3=connected)\n", WiFi.status());
  Serial.flush();

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println(">>> Speed test failed: WiFi not connected");
    Serial.flush();
    queueConsoleMessage("Internet check failed: WiFi not connected");
    return false;
  }

  Serial.println(">>> WiFi connected, starting speed test");
  Serial.flush();

  esp_task_wdt_reset();
  Serial.println(">>> Watchdog reset pre-speed-test");
  Serial.flush();

  WiFiClient client;

  // Test 1: Can we connect at all? (3 second timeout)
  Serial.println(">>> Test 1: Attempting to connect to 1.1.1.1:80 (3s timeout)");
  Serial.flush();
  unsigned long connectStart = millis();
  if (!client.connect("1.1.1.1", 80, 3000)) {
    Serial.printf(">>> Connection FAILED after %lu ms\n", millis() - connectStart);
    Serial.flush();
    queueConsoleMessage("Internet check failed: No internet access on WiFi network");
    esp_task_wdt_reset();
    return false;
  }

  unsigned long connectTime = millis() - connectStart;
  client.stop();
  esp_task_wdt_reset();
  // Test 2: Download small file and measure speed
  Serial.println(">>> Test 2: Setting up HTTP client for speed test");
  Serial.flush();
  HTTPClient http;
  // Use a reliable small file (Cloudflare's trace - about 200-300 bytes)
  http.begin(client, "http://cloudflare.com/cdn-cgi/trace");
  http.setTimeout(5000);
  unsigned long downloadStart = millis();
  int httpCode = http.GET();
  Serial.printf(">>> http.GET() returned code %d after %lu ms\n", httpCode, millis() - downloadStart);
  Serial.flush();
  weatherHttpResponseCode = httpCode;
  esp_task_wdt_reset();
  Serial.println(">>> Watchdog reset #3");
  Serial.flush();

  if (httpCode != 200) {
    http.end();
    Serial.printf(">>> Speed test FAILED: HTTP error %d\n", httpCode);
    Serial.flush();
    queueConsoleMessageF("Internet check failed: Connection error (HTTP %d)", httpCode);
    return false;
  }

  Serial.println(">>> Getting payload string");
  Serial.flush();

  String payload = http.getString();
  esp_task_wdt_reset();  // Feed after potentially slow getString()
  unsigned long downloadTime = millis() - downloadStart;

  int bytesReceived = payload.length();

  Serial.printf(">>> Received %d bytes in %lu ms\n", bytesReceived, downloadTime);
  Serial.flush();

  http.end();
  Serial.println(">>> HTTP connection ended");
  Serial.flush();

  esp_task_wdt_reset();
  Serial.println(">>> Watchdog reset #4");
  Serial.flush();

  float bytesPerSecond = 0;
  if (downloadTime > 0) {
    bytesPerSecond = (bytesReceived * 1000.0) / downloadTime;
  }

  float kbps = (bytesPerSecond * 8.0) / 1000.0;

  Serial.printf(">>> Speed test result: %.2f Kbps (%d bytes in %lu ms)\n", kbps, bytesReceived, downloadTime);
  Serial.flush();

  if (kbps < 5.0) {
    Serial.printf(">>> Speed test FAILED: Connection too slow (%.2f Kbps < 5 Kbps minimum)\n", kbps);
    Serial.flush();
    queueConsoleMessageF("Internet too slow: %.1f Kbps (need 5+ Kbps)", kbps);
    return false;
  }

  Serial.printf(">>> Speed test PASSED: %.2f Kbps\n", kbps);
  Serial.flush();
  queueConsoleMessageF("Internet speed OK: %.1f Kbps", kbps);

  Serial.println("========================================");
  Serial.println(">>> testInternetSpeed() COMPLETE - PASSED");
  Serial.println("========================================");
  Serial.flush();

  return true;
}
void otaRestoreNormalOperation(bool success) {
  Serial.printf("OTA restore: success=%d, restoring system state...\n", success ? 1 : 0);

  core0Busy = false;
  otaInProgress = false;
  lastHttpsOperationTime = millis();

  // Recreate TempTask if deleted — params must match the xTaskCreatePinnedToCore in setup()
  if (tempTaskHandle == NULL) {
    BaseType_t ok = xTaskCreatePinnedToCore(TempTask, "TempTask", 4096, NULL, 1, &tempTaskHandle, 0);
    if (ok == pdPASS) {
      Serial.println("✅ TempTask recreated on Core 0");
    } else {
      Serial.println("❌ TempTask recreation FAILED");
    }
  }

  // Recreate HTTPS task if deleted — params must match setup() (see setup() note)
  if (httpsTaskHandle == NULL) {
    BaseType_t ok = xTaskCreatePinnedToCore(httpsTask, "HTTPS", 12288, NULL, 1, &httpsTaskHandle, 0);
    if (ok == pdPASS) {
      Serial.println("✅ HTTPS task recreated on Core 0");
    } else {
      Serial.println("❌ HTTPS task recreation FAILED");
    }
  }

  vTaskDelay(pdMS_TO_TICKS(100));  // let tasks initialize cleanly

  Serial.println("System restoration complete");
}
static void otaHeapMark(const char *tag) {
  Serial.printf("HEAP %s: internal_free=%u largest=%u total_free=%u\n",
                tag,
                heap_caps_get_free_size(MALLOC_CAP_INTERNAL),
                heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL),
                ESP.getFreeHeap());
}
// TIME MANAGEMENT FUNCTIONS
// Time sync is deliberately RAM-only. NVS persistence (save/loadTimeSyncState)
// was deleted: the loader was never wired into setup(), so the per-message
// commits only wore the tiny 20KB nvs partition without ever being read back.
// After a reboot the device stays unsynced until a live source (NMEA / phone /
// NTP) reports. Don't re-add persistence casually — restoring a prior boot's
// timeBaseMillis breaks getCurrentTimestamp(): millis() restarts at 0, so the
// unsigned subtraction wraps ~49.7 days into the future.

time_t getCurrentTimestamp() {
  if (timeIsSynced && timeBase > 0) {
    unsigned long elapsedSeconds = (millis() - timeBaseMillis) / 1000;
    return timeBase + elapsedSeconds;
  }
  return 0;
}

// ── Soft clock recovery ─────────────────────────────────────────────────────
// Fixes the phantom multi-hour gaps in the Long Term history plots on a device
// with NO live time source (AP mode, no NTP/phone/GPS). Without a clock the
// long-term records are stamped 0, and the dashboard back-projects those
// zero-stamped records onto the wrong part of the time axis — good data renders
// as a giant gap. Re-establish a usable timebase at boot from two free sources,
// lowest-risk first:
//   1. RTC retention — the ESP32 system clock keeps running across a SOFTWARE
//      reset (OTA install, scheduled restart, crash/WDT). If a prior boot ran
//      settimeofday() (every real source does), time(nullptr) is still valid AND
//      already includes the brief reboot downtime → accurate, no estimate.
//   2. NVS-persisted epoch ("SoftClockEp", written at each field-off save) —
//      survives a cold power-up. Downtime is unknown, so this is an ESTIMATE
//      (assumes ~0 off-time); it snaps to truth the moment a real source reports.
// Either way the source is labeled TIME_ESTIMATED (low confidence) until a real
// source overwrites timeBase. CRITICAL: timeBaseMillis is anchored to the CURRENT
// millis() (it restarts at 0 each boot). Restoring a PRIOR boot's timeBaseMillis
// would wrap getCurrentTimestamp() ~49.7 days forward — exactly the hazard the old
// RAM-only design avoided. We never persist millis, only the epoch, so we're safe.
void restoreSoftClock() {
  if (timeIsSynced) return;   // a live source already won during early boot — leave it

  time_t rtc = time(nullptr);
  if (rtc > SOFTCLOCK_SANE_EPOCH) {            // RTC survived a soft reset → adopt as-is
    timeBase = rtc;
    timeBaseMillis = millis();
    timeIsSynced = true;
    currentTimeSource = TIME_ESTIMATED;
    Serial.printf("Soft clock: adopted retained RTC epoch=%ld\n", (long)rtc);
    return;
  }

  // Cold boot — seed from the epoch we persisted at the previous field-off edge.
  nvs_handle_t h;
  if (nvs_open("storage", NVS_READONLY, &h) == ESP_OK) {
    uint32_t saved = 0;
    if (nvs_get_u32(h, "SoftClockEp", &saved) == ESP_OK && (time_t)saved > SOFTCLOCK_SANE_EPOCH) {
      timeBase = (time_t)saved;
      timeBaseMillis = millis();
      timeIsSynced = true;
      currentTimeSource = TIME_ESTIMATED;
      struct timeval tv = { (time_t)saved, 0 };
      settimeofday(&tv, nullptr);              // keep time(nullptr) consistent for AP-mode callers
      Serial.printf("Soft clock: restored NVS epoch=%lu (estimate, downtime unknown)\n", (unsigned long)saved);
    }
    nvs_close(h);
  }
}
void syncTimeFromGPS(uint16_t daysSince1970, double secondsSinceMidnight) {
  // Manual mode gate: only AUTO and NMEA-forced accept NMEA time.
  if (gpsTimeSourceMode == GTS_PHONE || gpsTimeSourceMode == GTS_NTP) return;
  time_t gpsTime = (daysSince1970 * 86400UL) + (time_t)secondsSinceMidnight;

  if (gpsTime > 1577836800) {  // Jan 1, 2020
    timeBase = gpsTime;
    timeBaseMillis = millis();
    timeIsSynced = true;
    currentTimeSource = TIME_GPS;
    lastTimeSyncAttempt = millis();
    // Set the libc clock too, so time(NULL) is valid in NMEA/AP mode — not just after NTP.
    // Without this every time(NULL) timestamp (captures, sync badges) reads ~1970 off-grid.
    struct timeval tv = { (time_t)gpsTime, 0 };
    settimeofday(&tv, nullptr);

    if (NMEA2KVerbose) {
      Serial.println("Time synced from GPS");
    }
  }
}

// Adopt phone-provided time IF NMEA SystemTime isn't fresher AND the user
// hasn't forced a non-phone source. Called from /set_phone_data handler.
void syncTimeFromPhone(time_t phoneEpochSec) {
  if (phoneEpochSec <= 1577836800) return;  // sanity (pre-2020)
  // Skew sanity: if we already have a trusted timebase, reject phone times
  // more than 24h off — guards against phones with manually-set clocks or
  // simulators that boot without time sync. First sync skips this (nothing
  // to compare); NMEA/NTP will eventually overwrite a bad first sync.
  if (timeIsSynced && timeBase > 0) {
    time_t now = getCurrentTimestamp();
    long long diff = (long long)phoneEpochSec - (long long)now;
    if (diff < 0) diff = -diff;
    if (diff > 86400LL) return;  // > 24h off — likely bad phone clock
  }
  // Manual mode gate: only AUTO and PHONE-forced accept phone time.
  if (gpsTimeSourceMode == GTS_NMEA || gpsTimeSourceMode == GTS_NTP) return;
  // AUTO: NMEA still wins if fresh. PHONE-forced: take it regardless.
  if (gpsTimeSourceMode == GTS_AUTO) {
    bool nmeaFresh = (lastNmea2kSystemTimeMs > 0) &&
                     (millis() - lastNmea2kSystemTimeMs < NMEA_TIME_FRESH_MS);
    if (nmeaFresh) return;
  }
  timeBase = phoneEpochSec;
  timeBaseMillis = millis();
  timeIsSynced = true;
  currentTimeSource = TIME_PHONE;
  lastTimeSyncAttempt = millis();
  // Set the libc clock too, so time(NULL) is valid in AP/phone mode — not just after NTP.
  // Without this every time(NULL) timestamp (captures, sync badges) reads ~1970 off-grid.
  struct timeval tv = { (time_t)phoneEpochSec, 0 };
  settimeofday(&tv, nullptr);
}

// Promote phone GPS into the effective Latitude/Longitude globals. Respects
// gpsTimeSourceMode: AUTO defers to fresh NMEA; PHONE-forced always proceeds;
// NMEA/NTP-forced never proceed. MARK_FRESH on each index so IS_STALE-gated
// consumers (distance, smoothing, sensor hist) actually see the phone fix.
void consumePhoneGps() {
  if (gpsManualActive) return;  // sticky manual override beats phone
  if (gpsTimeSourceMode == GTS_NMEA || gpsTimeSourceMode == GTS_NTP) return;
  if (gpsTimeSourceMode == GTS_AUTO) {
    bool nmeaFresh = (lastNmea2kGnssMs > 0) &&
                     (millis() - lastNmea2kGnssMs < NMEA_GPS_FRESH_MS);
    if (nmeaFresh) return;  // NMEA wins in AUTO
  }
  bool phoneFresh = (lastPhoneGpsMs > 0) &&
                    (millis() - lastPhoneGpsMs < PHONE_FRESH_MS);
  if (!phoneFresh) return;
  LatitudeNMEA  = LatitudePhone;
  LongitudeNMEA = LongitudePhone;
  MARK_FRESH(IDX_LATITUDE_NMEA);
  MARK_FRESH(IDX_LONGITUDE_NMEA);
  MARK_FRESH(IDX_SATELLITE_COUNT);
  currentGpsSource = GPS_PHONE;
}

// Resolve current GPS + time source labels every loop tick. Behaviour depends
// on user-set gpsTimeSourceMode:
//   GTS_AUTO  — freshness-arbitrated priority chain (NMEA→Phone→NTP). Promotes
//               phone GPS to effective globals when NMEA stale; demotes both
//               source labels to none/drifting when nothing fresh.
//   GTS_NMEA  — force NMEA. Don't touch GNSS handler's writes. Label NMEA
//               always (even when stale — dashboard's IS_STALE greying alerts
//               the user).
//   GTS_PHONE — force phone. Overwrite LatitudeNMEA/LongitudeNMEA from phone
//               values. MARK_FRESH only if phone is actually fresh (so stale
//               forced data doesn't poison distance/smoothing).
//   GTS_NTP   — NTP-only time; GPS falls through to AUTO chain (NTP doesn't
//               give position).
// Cheap (a few unsigned compares); safe to call every tick.
void resolveSources() {
  unsigned long now = millis();
  bool nmeaGpsFresh   = (lastNmea2kGnssMs > 0)       && (now - lastNmea2kGnssMs       < NMEA_GPS_FRESH_MS);
  bool phoneGpsFresh  = (lastPhoneGpsMs > 0)         && (now - lastPhoneGpsMs         < PHONE_FRESH_MS);
  bool nmeaTimeFresh  = (lastNmea2kSystemTimeMs > 0) && (now - lastNmea2kSystemTimeMs < NMEA_TIME_FRESH_MS);
  bool phoneTimeFresh = (lastPhoneTimeMs > 0)        && (now - lastPhoneTimeMs        < PHONE_FRESH_MS);

  // ── GPS source ─────────────────────────────────────────────────────────
  // Sticky manual override sits ABOVE the source-mode chain: when the user has
  // typed coordinates in Weather Mode, they win over boat NMEA and phone GPS
  // (and even the forced modes) and are reasserted every tick until cleared.
  if (gpsManualActive) {
    LatitudeNMEA  = LatitudeManual;
    LongitudeNMEA = LongitudeManual;
    MARK_FRESH(IDX_LATITUDE_NMEA);   // manual coords are always "fresh" — user vouched for them
    MARK_FRESH(IDX_LONGITUDE_NMEA);
    currentGpsSource = GPS_MANUAL;
  } else
  switch (gpsTimeSourceMode) {
    case GTS_NMEA:
      currentGpsSource = GPS_NMEA;  // honor user choice even when stale
      break;
    case GTS_PHONE:
      LatitudeNMEA  = LatitudePhone;
      LongitudeNMEA = LongitudePhone;
      if (phoneGpsFresh) {
        MARK_FRESH(IDX_LATITUDE_NMEA);
        MARK_FRESH(IDX_LONGITUDE_NMEA);
        MARK_FRESH(IDX_SATELLITE_COUNT);
      }
      currentGpsSource = GPS_PHONE;
      break;
    case GTS_AUTO:
    case GTS_NTP:  // GPS falls through to AUTO (NTP is time-only)
    default:
      if (nmeaGpsFresh) {
        // GNSS handler already set GPS_NMEA + MARK_FRESH on parse.
      } else if (phoneGpsFresh) {
        consumePhoneGps();  // overwrites globals, MARK_FRESH, sets GPS_PHONE
      } else {
        currentGpsSource = GPS_NONE;
      }
      break;
  }

  // ── Time source label ──────────────────────────────────────────────────
  // (Forced modes never need label-flipping here — the syncTime* setters are
  // already gated, so only the authorized source can write the timebase.)
  if (gpsTimeSourceMode == GTS_AUTO) {
    if (nmeaTimeFresh) {
      // SystemTime handler sets TIME_GPS on parse.
    } else if (phoneTimeFresh) {
      if (currentTimeSource != TIME_PHONE && currentTimeSource != TIME_NTP) {
        currentTimeSource = TIME_PHONE;
      }
    } else if (currentTimeSource == TIME_GPS || currentTimeSource == TIME_PHONE) {
      currentTimeSource = TIME_MILLIS;
    }
  }
}

void checkTimeSync() {
  unsigned long currentMillis = millis();

  if (currentMillis - lastTimeSyncAttempt < TIME_SYNC_INTERVAL) {
    return;
  }

  // AUTO: NTP fires only when BOTH NMEA SystemTime AND phone time are stale
  // (phone is fresher and wins). NTP-forced: always fire — syncTimeFromNTP
  // itself early-returns if the mode disallows it. NMEA/Phone-forced: never
  // touch NTP (the syncTimeFromNTP early-return handles this too, but skip
  // the call to avoid the WiFi check + log noise).
  if (gpsTimeSourceMode == GTS_AUTO) {
    bool nmeaTimeFresh  = (lastNmea2kSystemTimeMs > 0) &&
                          (currentMillis - lastNmea2kSystemTimeMs < NMEA_TIME_FRESH_MS);
    bool phoneTimeFresh = (lastPhoneTimeMs > 0) &&
                          (currentMillis - lastPhoneTimeMs < PHONE_FRESH_MS);
    if (!nmeaTimeFresh && !phoneTimeFresh) syncTimeFromNTP();
  } else if (gpsTimeSourceMode == GTS_NTP) {
    syncTimeFromNTP();
  }
  // GTS_NMEA / GTS_PHONE → never NTP.
}
// Format timestamp into static buffer - returns pointer to timestampBuffer
const char *formatTimestamp(time_t timestamp) {
  if (timestamp == 0) {
    timestampBuffer[0] = '\0';
    return timestampBuffer;
  }

  struct tm timeinfo;
  gmtime_r(&timestamp, &timeinfo);
  strftime(timestampBuffer, TIMESTAMP_BUFFER_SIZE, "%Y-%m-%dT%H:%M:%SZ", &timeinfo);

  return timestampBuffer;
}
// Compute collection time for current window
// Returns: valid epoch if time synced, negative millis if not
time_t computeCollectionTime() {
  if (timeIsSynced && timeBase > 0) {
    long long deltaMs = (long long)currentWindow->windowStartTime - (long long)timeBaseMillis;
    long long deltaSec = deltaMs / 1000;
    time_t epoch = timeBase + deltaSec;

    // Sanity check: if result is unreasonable, fall back to millis marker
    if (epoch > 1704067200 && epoch < 2000000000) {
      return epoch;
    }
  }

  // No valid time: return NEGATIVE millis to mark as "needs correction"
  return -(time_t)currentWindow->windowStartTime;
}
// Reconstruct timestamp for buffered record
// Handles negative millis markers and reboot scenarios
time_t reconstructTimestamp(time_t collectionTime) {
  if (collectionTime > 1577836800) {
    // Already a valid epoch
    return collectionTime;
  }

  if (collectionTime < 0) {
    // Was stored as negative millis marker
    unsigned long capturedMillis = (unsigned long)(-collectionTime);

    // Check if reboot happened (millis wrapped or is now less than captured)
    if (millis() < capturedMillis) {
      // Reboot detected: can't reliably reconstruct
      // Use current time as best effort
      Serial.println("Warning: Buffered record from previous boot, using current time");
      return getCurrentTimestamp();
    }

    // Same boot session: reconstruct using time base
    if (timeIsSynced && timeBase > 0) {
      long long deltaMs = (long long)capturedMillis - (long long)timeBaseMillis;
      long long deltaSec = deltaMs / 1000;
      time_t reconstructed = timeBase + deltaSec;

      // Sanity check
      if (reconstructed > 1704067200 && reconstructed < 2000000000) {
        return reconstructed;
      }
    }
  }

  // Fallback: use current time as best effort
  return getCurrentTimestamp();
}

