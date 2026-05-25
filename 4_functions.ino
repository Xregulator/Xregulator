
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
 * KNOWN HEAP EFFICIENCY ISSUE - OTA/HTTPS String Concatenation (LOW PRIORITY)
 * 
 * Several OTA and HTTPS functions use String concatenation (+) instead of snprintf:
 *   - Line ~3126: checkForSpecificOTAUpdate() - URL building
 *   - Line ~3242: executeUpdateFirmwareVersion() - URL building  
 *   - Line ~3250: executeUpdateFirmwareVersion() - Authorization header
 *   - Line ~3253: executeUpdateFirmwareVersion() - JSON payload
 *   - Line ~3300: executeCheckForcedUpdate() - URL building
 *   - Line ~3308: executeCheckForcedUpdate() - Authorization header
 *   - Line ~3311: executeCheckForcedUpdate() - JSON payload
 *   - Lines ~3147-3169: Various Serial/events.send() messages
 * 
 * This creates temporary String objects and heap fragmentation. HOWEVER:
 *   - These functions run infrequently (user-initiated OTA or periodic checks)
 *   - Not called in tight loops or critical paths
 *   - Heap impact is minimal compared to HTTPS/WiFi operations already happening
 * 
 * DECISION: Acceptable as-is. OTA operations are rare enough that the heap churn
 * doesn't matter. If OTA stability issues arise, fix by replacing String concat
 * with snprintf into char buffers (see executeFetchWeatherData() lines 241-244
 * or buildConfigPayload() for correct pattern).
 * 
 * Core telemetry, sensor, and control loops use proper snprintf - no issues there.
 */

void updateCpuLoad() {
  if (!taskArray) return;  // Safety check - allocated in setup()

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

void updateSystemHealthStats() {
  if (otaInProgress) return;

  static unsigned long lastCpuSample = 0;
  static unsigned long lastHeapSample = 0;

  unsigned long now = millis();

  if (now - lastCpuSample >= 2000) {
    lastCpuSample = now;
    updateCpuLoad();
  }

  if (now - lastHeapSample >= 4000) {
    lastHeapSample = now;

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

    // TLS handshake needs ~32-40 KB contiguous internal RAM. Warn early so there is
    // time to investigate before HTTPS starts silently failing.
    // Every HTTPS handshake briefly fragments largest-block under 34 KB and recovers
    // above 38 KB, so without throttling this would fire on every upload. Cap at
    // one console message per 5 minutes — the underlying dip/recover cycle is still
    // tracked by heapWarnSent so we re-fire promptly on the next dip after the throttle expires.
    static bool heapWarnSent = false;
    static unsigned long lastHeapWarnMs = 0;
    const unsigned long HEAP_WARN_THROTTLE_MS = 300000UL;
    if (LargestInternalBlock < 34 && !heapWarnSent) {
      heapWarnSent = true;
      if (millis() - lastHeapWarnMs >= HEAP_WARN_THROTTLE_MS) {
        lastHeapWarnMs = millis();
        queueConsoleMessage("WARNING: Internal RAM fragmented — HTTPS may fail. Check ESP32 Stats panel.");
      }
    } else if (LargestInternalBlock >= 38) {
      heapWarnSent = false;  // Re-arm dip detector; throttle still applies on next fire
    }
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
    UVToday = mjToday * MJ_TO_KWH_CONVERSION;  // Convert MJ/m² to kWh/m²
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
    return;  // Skip during OTA
  }
  //Analyze weather data and decide alternator mode
  // If weather mode is disabled or the weather data is invalid, it sets the mode to normal (0) and exits.
  // Otherwise, it checks the UV index for today, tomorrow, and the next day.
  //If 2 or more days have a UV index above the configured threshold, it sets the mode to high UV mode (1), which disables the alternator.
  if (!weatherDataValid || !weatherModeEnabled) {  // if weather mode is not enabled, or weather data is invalid
    currentWeatherMode = 0;
    return;
  }
  // Count days above threshold
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
    return;  // Skip during OTA
  }
  if (!weatherModeEnabled) {
    currentWeatherMode = 0;
    return;
  }

  unsigned long now = millis();

  // Use existing valid data if fresh
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

  // Check if time to auto-refresh — signed delta survives the 49.7-day millis() rollover
  if ((int32_t)(now - nextWeatherUpdate) >= 0) {
    HttpsRequest req = { .type = HTTPS_FETCH_WEATHER };
    if (xQueueSend(httpsQueue, &req, 0) == pdTRUE) {
      nextWeatherUpdate = now + WeatherUpdateInterval;
    } else {
      nextWeatherUpdate = now + 2000;  // Retry soon if queue full
    }
  }
  // Analyze existing data if available
  if (weatherDataValid) {
    analyzeWeatherMode();
  }
}

void initWeatherModeSettings() {
  if (!fsExists("/LatitudeNMEA.txt")) {
    writeFile(LittleFS, "/LatitudeNMEA.txt", "0.0");
  } else {
    LatitudeNMEA = readFile(LittleFS, "/LatitudeNMEA.txt").toDouble();
  }
  if (!fsExists("/LongitudeNMEA.txt")) {
    writeFile(LittleFS, "/LongitudeNMEA.txt", "0.0");
  } else {
    LongitudeNMEA = readFile(LittleFS, "/LongitudeNMEA.txt").toDouble();
  }
  if (!fsExists("/weatherModeEnabled.txt")) {
    writeFile(LittleFS, "/weatherModeEnabled.txt", String(weatherModeEnabled).c_str());
  } else {
    weatherModeEnabled = readFile(LittleFS, "/weatherModeEnabled.txt").toInt();
  }
  if (!fsExists("/UVThresholdHigh.txt")) {
    writeFile(LittleFS, "/UVThresholdHigh.txt", String(UVThresholdHigh, 1).c_str());
  } else {
    UVThresholdHigh = readFile(LittleFS, "/UVThresholdHigh.txt").toFloat();
  }
  if (!fsExists("/performanceRatio.txt")) {
    writeFile(LittleFS, "/performanceRatio.txt", String(performanceRatio, 2).c_str());
  } else {
    performanceRatio = readFile(LittleFS, "/performanceRatio.txt").toFloat();
  }
  if (!fsExists("/SolarWatts.txt")) {
    writeFile(LittleFS, "/SolarWatts.txt", String(SolarWatts).c_str());
  } else {
    SolarWatts = readFile(LittleFS, "/SolarWatts.txt").toInt();
  }
  if (!fsExists("/weatherDataValid.txt")) {
    writeFile(LittleFS, "/weatherDataValid.txt", "0");
  } else {
    weatherDataValid = readFile(LittleFS, "/weatherDataValid.txt").toInt();
  }
  if (!fsExists("/WeatherUpdateInterval.txt")) {
    writeFile(LittleFS, "/WeatherUpdateInterval.txt", String(WeatherUpdateInterval).c_str());
  } else {
    WeatherUpdateInterval = readFile(LittleFS, "/WeatherUpdateInterval.txt").toInt();
  }
  if (!fsExists("/WeatherTimeoutMs.txt")) {
    writeFile(LittleFS, "/WeatherTimeoutMs.txt", String(WeatherTimeoutMs).c_str());
  } else {
    WeatherTimeoutMs = readFile(LittleFS, "/WeatherTimeoutMs.txt").toInt();
  }
}


void triggerWeatherUpdate() {
  // Validate prerequisites
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

  // Queue the fetch
  HttpsRequest req = { .type = HTTPS_FETCH_WEATHER };
  if (xQueueSend(httpsQueue, &req, 0) == pdTRUE) {
    queueConsoleMessageF("Weather: Update queued");
    nextWeatherUpdate = millis() + WeatherUpdateInterval;  // Reset auto timer
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
    return;  // Skip during OTA
  }
  static unsigned long lastTempHealthCheck = 0;
  unsigned long now = millis();

  // Check every 5 seconds
  if (now - lastTempHealthCheck < 5000) return;
  lastTempHealthCheck = now;

  // Check if TempTask is alive
  if (now - lastTempTaskHeartbeat > TEMP_TASK_TIMEOUT) {
    if (tempTaskHealthy) {  // First time detecting the problem
      tempTaskHealthy = false;
      tempTaskAlarm = true;
      queueConsoleMessageF("CRITICAL: TempTask hung up - task not responding for %lu seconds", (now - lastTempTaskHeartbeat) / 1000);

      // Field cut is handled by the regulation loop: tempDataVeryStale fires at 20s of no
      // MARK_FRESH call, which produces REASON_TEMP_STALE → MODE_CRITICAL_RAMP → GPIO4 cut.
    }
  } else {
    // TempTask is responding
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

  // CRITICAL: Verify device is actually present on I2C bus
  if (!i2cProbe8bit(LSM6DSOX_ADDR)) {
    Serial.println("ERROR: No I2C ACK from LSM6DSOX address - sensor not present");
    Serial.println("IMU disabled - will retry on next boot");
    imuEnabled = false;
    return;
  }
  Serial.println("I2C ACK detected at LSM6DSOX address");

  // CRITICAL: Read WHO_AM_I and verify it's actually an LSM6DSOX
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

  // Now proceed with library initialization
  if (imu.begin() != LSM6DSOX_OK) {
    Serial.println("ERROR: LSM6DSOX library init failed");
    queueConsoleMessageF("IMU library init failed");
    imuEnabled = false;
    return;
  }

  // Enable accelerometer and gyroscope
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

  // Only print success if we actually got here with hardware verified
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
  // Zero out the entire window structure to avoid garbage values
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

  // Initialize timing
  imuWindow->lastUpdateTime_us = 0;
  imuWindow->lastGyroUpdateTime_us = 0;

  // Initialize wave period to invalid
  imuWindow->wave_period = -1000;  // -1.0s scaled

  Serial.println("IMU structures initialized");
}
void initializeHardware() {  // Helper function to organize hardware initialization

  Serial.println("Starting hardware initialization...");
  // Force I2C initialization with correct pins
  Wire.end();  // End any existing I2C
  Wire.begin(9, 10);
  Wire.setClock(800000);  // 800 kHz — current default. Out of spec for LSM6DSOX IMU (400 kHz max) and ADS1115 in Fast-mode, but verified stable: zero I²C errors / FIFO overruns / Unknown Tags across multiple sessions, ~15% faster IMU reads vs 400 kHz. Revert to 400000 if any of imu_i2c_error_count / imu_unknown_tag_count / adsI2CErrorCount ever go non-zero.
  Wire.setTimeOut(15);   // Added as safety April 2026
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
  NMEA2000.EnableForward(false);  // was false
  NMEA2000.SetMsgHandler(HandleNMEA2000Msg);
  //  NMEA2000.SetN2kCANMsgBufSize(2);
  NMEA2000.Open();
  Serial.println("NMEA2K Running...");


  //Victron VeDirect and NMEA0183
  Serial1.begin(19200, SERIAL_8N1, 7, -1, 1);  // This is the reading of Victron VEDirect
  Serial2.begin(19200, SERIAL_8N1, 6, -1, 0);  // ... note the "0" at end for normal logic.  This is the reading of the combined NMEA0183 data from YachtDevices
  Serial2.flush();                             // why don't i do this for Serial 1??

  // INA228 Battery Voltage/Current Sensor
  if (!INA.begin()) {
    Serial.println("Could not connect INA228. Fix and Reboot");
    queueConsoleMessage("WARNING: Could not connect INA228 Battery Voltage/Amp measuring chip");
    INADisconnected = 1;
  } else {
    INADisconnected = 0;

    // Configure ADC settings (527ms update time with these settings)
    // setAverage() takes raw register value: 0=1, 1=4, 2=16, 3=64, 4=128, 5=256, 6=512, 7=1024 samples
    // Previous code used setAverage(4)=128 samples (1054ms) — register update exceeded 900ms poll interval.
    INA.setMode(11);                       // Continuous shunt and bus voltage measurement
    INA.setAverage(4);                     // 128-sample averaging — 128 × 8.24ms = 1054ms register update
    INA.setBusVoltageConversionTime(7);    // 4120 µs conversion time
    INA.setShuntVoltageConversionTime(7);  // 4120 µs conversion time

    // Set overvoltage threshold for hardware protection
    updateINA228OvervoltageThreshold();
    queueConsoleMessage("INA228 initialized: Hardware overvoltage protection enabled");
  }

  // Initialize data timestamps
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
  //Connection check
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
  // Read back the config register to verify settings
  Wire.beginTransmission(0x48);  // ADS1115 default address
  Wire.write(0x01);              // Config register
  Wire.endTransmission(false);
  Wire.requestFrom(0x48, 2);
  if (Wire.available() >= 2) {
    uint16_t configReg = (Wire.read() << 8) | Wire.read();
    queueConsoleMessage("ADS1115 Config Register: 0x" + String(configReg, HEX));

    // Decode key bits
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
  imuInit();            // Accelerometer
  initIMUStructures();  // Accelerometer
  Serial.println("Hardware initialization complete");
}
void InitSystemSettings() {  // load all settings from LittleFS.  If no files exist, create them and populate with the hardcoded values

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

  if (!fsExists("/BatteryCapacity_Ah.txt")) {
    writeFile(LittleFS, "/BatteryCapacity_Ah.txt", String(BatteryCapacity_Ah).c_str());
  } else {
    BatteryCapacity_Ah = readFile(LittleFS, "/BatteryCapacity_Ah.txt").toInt();
  }
  PeukertRatedCurrent_A = BatteryCapacity_Ah / 20.0f;  // executes ever time InitSystemSettings runs, regardless of which branches are taken.  Correct.
  if (!fsExists("/ChargeEfficiency.txt")) {
    // Save user-readable form (e.g. "99.0"), NOT the scaled integer, so the load path always
    // sees a value ≤ 100 and can safely apply × 10 to reconstruct the scaled integer.
    writeFile(LittleFS, "/ChargeEfficiency.txt", String(ChargeEfficiency_scaled / 10.0f, 1).c_str());
  } else {
    // File stores the user-readable percentage string (e.g. "99.0"). Multiply by 10 to get scaled int.
    ChargeEfficiency_scaled = (int)round(readFile(LittleFS, "/ChargeEfficiency.txt").toFloat() * 10);
  }
  if (!fsExists("/TailCurrent.txt")) {
    writeFile(LittleFS, "/TailCurrent.txt", String(TailCurrent).c_str());
  } else {
    TailCurrent = readFile(LittleFS, "/TailCurrent.txt").toFloat();
  }
  if (!fsExists("/FuelEfficiency.txt")) {
    writeFile(LittleFS, "/FuelEfficiency.txt", String(FuelEfficiency_scaled).c_str());
  } else {
    FuelEfficiency_scaled = readFile(LittleFS, "/FuelEfficiency.txt").toInt();
  }
  if (!fsExists("/TemperatureLimitF.txt")) {
    writeFile(LittleFS, "/TemperatureLimitF.txt", String(TemperatureLimitF).c_str());
  } else {
    TemperatureLimitF = readFile(LittleFS, "/TemperatureLimitF.txt").toInt();
  }
  if (!fsExists("/ManualDutyTarget.txt")) {
    writeFile(LittleFS, "/ManualDutyTarget.txt", String(ManualDutyTarget).c_str());
  } else {
    ManualDutyTarget = readFile(LittleFS, "/ManualDutyTarget.txt").toInt();  //
  }
  if (!fsExists("/capLimitMode.txt")) {
    writeFile(LittleFS, "/capLimitMode.txt", String(capLimitMode).c_str());
  } else {
    capLimitMode = constrain(readFile(LittleFS, "/capLimitMode.txt").toInt(), 0, 1);
  }
  if (!fsExists("/BulkVoltage.txt")) {
    writeFile(LittleFS, "/BulkVoltage.txt", String(BulkVoltage).c_str());
  } else {
    BulkVoltage = readFile(LittleFS, "/BulkVoltage.txt").toFloat();
  }
  if (!fsExists("/socInfoAvailable.txt")) {
    writeFile(LittleFS, "/socInfoAvailable.txt", String(socInfoAvailable).c_str());
  } else {
    socInfoAvailable = readFile(LittleFS, "/socInfoAvailable.txt").toInt();
  }
  if (!fsExists("/TailCurrent_A.txt")) {
    writeFile(LittleFS, "/TailCurrent_A.txt", String(TailCurrent_A).c_str());
  } else {
    TailCurrent_A = readFile(LittleFS, "/TailCurrent_A.txt").toFloat();
  }
  if (!fsExists("/RebulkVoltage.txt")) {
    writeFile(LittleFS, "/RebulkVoltage.txt", String(RebulkVoltage).c_str());
  } else {
    RebulkVoltage = readFile(LittleFS, "/RebulkVoltage.txt").toFloat();
  }
  if (!fsExists("/rebulkDebounceTime.txt")) {
    writeFile(LittleFS, "/rebulkDebounceTime.txt", String(rebulkDebounceTime).c_str());
  } else {
    rebulkDebounceTime = readFile(LittleFS, "/rebulkDebounceTime.txt").toInt();
  }
  if (!fsExists("/MinFloatTime.txt")) {
    writeFile(LittleFS, "/MinFloatTime.txt", String(MinFloatTime).c_str());
  } else {
    MinFloatTime = readFile(LittleFS, "/MinFloatTime.txt").toInt();
  }
  if (!fsExists("/SOC_BlockRebulk_percent.txt")) {
    writeFile(LittleFS, "/SOC_BlockRebulk_percent.txt", String(SOC_BlockRebulk_percent).c_str());
  } else {
    SOC_BlockRebulk_percent = readFile(LittleFS, "/SOC_BlockRebulk_percent.txt").toInt();
  }
  if (!fsExists("/SOC_AllowRebulk_percent.txt")) {
    writeFile(LittleFS, "/SOC_AllowRebulk_percent.txt", String(SOC_AllowRebulk_percent).c_str());
  } else {
    SOC_AllowRebulk_percent = readFile(LittleFS, "/SOC_AllowRebulk_percent.txt").toInt();
  }
  if (!fsExists("/wavePeriod.txt")) {
    writeFile(LittleFS, "/wavePeriod.txt", String(wavePeriod).c_str());
  } else {
    wavePeriod = readFile(LittleFS, "/wavePeriod.txt").toInt();
  }

  if (!fsExists("/InputFilterTC.txt")) {
      writeFile(LittleFS, "/InputFilterTC.txt", String(InputFilterTC).c_str());
    } else {
      InputFilterTC = readFile(LittleFS, "/InputFilterTC.txt").toFloat();
    }

    if (!fsExists("/SystemIDStepAmplitude.txt")) {
      writeFile(LittleFS, "/SystemIDStepAmplitude.txt", String(SystemIDStepAmplitude).c_str());
    } else {
      SystemIDStepAmplitude = readFile(LittleFS, "/SystemIDStepAmplitude.txt").toFloat();
    }
    
  if (!fsExists("/SwitchingFrequency.txt")) {
    writeFile(LittleFS, "/SwitchingFrequency.txt", String(SwitchingFrequency).c_str());
  } else {
    SwitchingFrequency = readFile(LittleFS, "/SwitchingFrequency.txt").toInt();
  }
  if (!fsExists("/FloatVoltage.txt")) {
    writeFile(LittleFS, "/FloatVoltage.txt", String(FloatVoltage).c_str());
  } else {
    FloatVoltage = readFile(LittleFS, "/FloatVoltage.txt").toFloat();
  }
  if (!fsExists("/yyMin.txt")) {
    writeFile(LittleFS, "/yyMin.txt", String(yyMin).c_str());
  } else {
    yyMin = readFile(LittleFS, "/yyMin.txt").toInt();
  }
  if (!fsExists("/FieldAdjustmentInterval.txt")) {
    writeFile(LittleFS, "/FieldAdjustmentInterval.txt", String(FieldAdjustmentInterval).c_str());
  } else {
    FieldAdjustmentInterval = readFile(LittleFS, "/FieldAdjustmentInterval.txt").toFloat();
  }
  if (!fsExists("/ManualFieldToggle.txt")) {
    writeFile(LittleFS, "/ManualFieldToggle.txt", String(ManualFieldToggle).c_str());
  } else {
    ManualFieldToggle = readFile(LittleFS, "/ManualFieldToggle.txt").toInt();
  }
  if (!fsExists("/SwitchControlOverride.txt")) {
    writeFile(LittleFS, "/SwitchControlOverride.txt", String(SwitchControlOverride).c_str());
  } else {
    SwitchControlOverride = readFile(LittleFS, "/SwitchControlOverride.txt").toInt();
  }
  if (!fsExists("/IgnitionOverride.txt")) {
    writeFile(LittleFS, "/IgnitionOverride.txt", String(IgnitionOverride).c_str());
  } else {
    IgnitionOverride = readFile(LittleFS, "/IgnitionOverride.txt").toInt();
  }
  if (!fsExists("/hardwarePresent.txt")) {
    writeFile(LittleFS, "/hardwarePresent.txt", String(hardwarePresent).c_str());
  } else {
    hardwarePresent = readFile(LittleFS, "/hardwarePresent.txt").toInt();
  }
  if (!fsExists("/SENSOR_UPLOAD_INTERVAL.txt")) {
    writeFile(LittleFS, "/SENSOR_UPLOAD_INTERVAL.txt", String(SENSOR_UPLOAD_INTERVAL).c_str());
  } else {
    SENSOR_UPLOAD_INTERVAL = (unsigned long)readFile(LittleFS, "/SENSOR_UPLOAD_INTERVAL.txt").toInt();
  }
  if (!fsExists("/VMGUseTrueWind.txt")) {
    writeFile(LittleFS, "/VMGUseTrueWind.txt", String(VMGUseTrueWind).c_str());
  } else {
    VMGUseTrueWind = readFile(LittleFS, "/VMGUseTrueWind.txt").toInt();
  }
  if (!fsExists("/MaintainMode.txt")) {
    writeFile(LittleFS, "/MaintainMode.txt", String(MaintainMode).c_str());
  } else {
    MaintainMode = readFile(LittleFS, "/MaintainMode.txt").toInt();
  }
  if (!fsExists("/TargetVoltageMode.txt")) {
    writeFile(LittleFS, "/TargetVoltageMode.txt", String(TargetVoltageMode).c_str());
  } else {
    TargetVoltageMode = readFile(LittleFS, "/TargetVoltageMode.txt").toInt();
  }
  if (!fsExists("/OnOff.txt")) {
    writeFile(LittleFS, "/OnOff.txt", String(OnOff).c_str());
  } else {
    OnOff = readFile(LittleFS, "/OnOff.txt").toInt();
  }
  if (!fsExists("/HiLow.txt")) {
    writeFile(LittleFS, "/HiLow.txt", String(HiLow).c_str());
  } else {
    HiLow = readFile(LittleFS, "/HiLow.txt").toInt();
  }
  if (!fsExists("/AmpSensorRange.txt")) {
    writeFile(LittleFS, "/AmpSensorRange.txt", String(AmpSensorRange).c_str());
  } else {
    AmpSensorRange = readFile(LittleFS, "/AmpSensorRange.txt").toInt();
  }
  if (!fsExists("/InvertAltAmps.txt")) {
    writeFile(LittleFS, "/InvertAltAmps.txt", String(InvertAltAmps).c_str());
  } else {
    InvertAltAmps = readFile(LittleFS, "/InvertAltAmps.txt").toInt();
  }
  if (!fsExists("/InvertBattAmps.txt")) {
    writeFile(LittleFS, "/InvertBattAmps.txt", String(InvertBattAmps).c_str());
  } else {
    InvertBattAmps = readFile(LittleFS, "/InvertBattAmps.txt").toInt();
  }
  if (!fsExists("/LimpHome.txt")) {
    writeFile(LittleFS, "/LimpHome.txt", String(LimpHome).c_str());
  } else {
    LimpHome = readFile(LittleFS, "/LimpHome.txt").toInt();
  }
  if (!fsExists("/VeData.txt")) {
    writeFile(LittleFS, "/VeData.txt", String(VeData).c_str());
  } else {
    VeData = readFile(LittleFS, "/VeData.txt").toInt();
  }
  if (!fsExists("/NMEA0183Data.txt")) {
    writeFile(LittleFS, "/NMEA0183Data.txt", String(NMEA0183Data).c_str());
  } else {
    NMEA0183Data = readFile(LittleFS, "/NMEA0183Data.txt").toInt();
  }
  if (!fsExists("/NMEA2KData.txt")) {
    writeFile(LittleFS, "/NMEA2KData.txt", String(NMEA2KData).c_str());
  } else {
    NMEA2KData = readFile(LittleFS, "/NMEA2KData.txt").toInt();
  }
  if (!fsExists("/waveAmplitude.txt")) {
    writeFile(LittleFS, "/waveAmplitude.txt", String(waveAmplitude).c_str());
  } else {
    waveAmplitude = readFile(LittleFS, "/waveAmplitude.txt").toInt();
  }
  if (!fsExists("/CurrentThreshold.txt")) {
    writeFile(LittleFS, "/CurrentThreshold.txt", String(CurrentThreshold).c_str());
  } else {
    CurrentThreshold = readFile(LittleFS, "/CurrentThreshold.txt").toFloat();
  }
  if (!fsExists("/PeukertExponent.txt")) {
    writeFile(LittleFS, "/PeukertExponent.txt", String(PeukertExponent_scaled).c_str());
  } else {
    float pv = readFile(LittleFS, "/PeukertExponent.txt").toFloat();
    if (pv <= 2.0f) {
      // old file stored raw user input (e.g. "1.12") — migrate to scaled int
      PeukertExponent_scaled = (int)(pv * 100);
      writeFile(LittleFS, "/PeukertExponent.txt", String(PeukertExponent_scaled).c_str());
    } else {
      PeukertExponent_scaled = (int)pv;
    }
  }
  if (!fsExists("/ChargedVoltage.txt")) {
    writeFile(LittleFS, "/ChargedVoltage.txt", String(ChargedVoltage_Scaled).c_str());
  } else {
    float cv = readFile(LittleFS, "/ChargedVoltage.txt").toFloat();
    if (cv <= 70.0f) {
      // old file stored raw user input (e.g. "14.50") — migrate to scaled int
      ChargedVoltage_Scaled = (int)(cv * 100);
      writeFile(LittleFS, "/ChargedVoltage.txt", String(ChargedVoltage_Scaled).c_str());
    } else {
      ChargedVoltage_Scaled = (int)cv;
    }
  }
  if (!fsExists("/ChargedDetectionTime.txt")) {
    writeFile(LittleFS, "/ChargedDetectionTime.txt", String(ChargedDetectionTime).c_str());
  } else {
    ChargedDetectionTime = readFile(LittleFS, "/ChargedDetectionTime.txt").toInt();
  }
  if (!fsExists("/IgnoreTemperature.txt")) {
    writeFile(LittleFS, "/IgnoreTemperature.txt", String(IgnoreTemperature).c_str());
  } else {
    IgnoreTemperature = readFile(LittleFS, "/IgnoreTemperature.txt").toInt();
  }
  if (!fsExists("/IgnoreRPM.txt")) {
    writeFile(LittleFS, "/IgnoreRPM.txt", String(IgnoreRPM).c_str());
  } else {
    IgnoreRPM = readFile(LittleFS, "/IgnoreRPM.txt").toInt();
  }
  if (!fsExists("/MinRPMForField.txt")) {
    writeFile(LittleFS, "/MinRPMForField.txt", String(MinRPMForField).c_str());
  } else {
    MinRPMForField = readFile(LittleFS, "/MinRPMForField.txt").toInt();
  }
  if (!fsExists("/bmsLogic.txt")) {
    writeFile(LittleFS, "/bmsLogic.txt", String(bmsLogic).c_str());
  } else {
    bmsLogic = readFile(LittleFS, "/bmsLogic.txt").toInt();
  }
  if (!fsExists("/bmsLogicLevelOff.txt")) {
    writeFile(LittleFS, "/bmsLogicLevelOff.txt", String(bmsLogicLevelOff).c_str());
  } else {
    bmsLogicLevelOff = readFile(LittleFS, "/bmsLogicLevelOff.txt").toInt();
  }
  if (!fsExists("/AlarmActivate.txt")) {
    writeFile(LittleFS, "/AlarmActivate.txt", String(AlarmActivate).c_str());
  } else {
    AlarmActivate = readFile(LittleFS, "/AlarmActivate.txt").toInt();
  }
  if (!fsExists("/TempAlarm.txt")) {
    writeFile(LittleFS, "/TempAlarm.txt", String(TempAlarm).c_str());
  } else {
    TempAlarm = readFile(LittleFS, "/TempAlarm.txt").toInt();
  }
  if (!fsExists("/TempAlarmLow.txt")) {
    writeFile(LittleFS, "/TempAlarmLow.txt", String(TempAlarmLow).c_str());
  } else {
    TempAlarmLow = readFile(LittleFS, "/TempAlarmLow.txt").toInt();
  }
  if (!fsExists("/VoltageAlarmHigh.txt")) {
    writeFile(LittleFS, "/VoltageAlarmHigh.txt", String(VoltageAlarmHigh).c_str());
  } else {
    VoltageAlarmHigh = readFile(LittleFS, "/VoltageAlarmHigh.txt").toInt();
  }
  if (!fsExists("/VoltageAlarmLow.txt")) {
    writeFile(LittleFS, "/VoltageAlarmLow.txt", String(VoltageAlarmLow).c_str());
  } else {
    VoltageAlarmLow = readFile(LittleFS, "/VoltageAlarmLow.txt").toInt();
  }
  if (!fsExists("/CurrentAlarmHigh.txt")) {
    writeFile(LittleFS, "/CurrentAlarmHigh.txt", String(CurrentAlarmHigh).c_str());
  } else {
    CurrentAlarmHigh = readFile(LittleFS, "/CurrentAlarmHigh.txt").toInt();
  }
  if (!fsExists("/RPMScalingFactor.txt")) {
    writeFile(LittleFS, "/RPMScalingFactor.txt", String(RPMScalingFactor).c_str());
  } else {
    RPMScalingFactor = readFile(LittleFS, "/RPMScalingFactor.txt").toInt();
  }
  if (!fsExists("/FieldResistance.txt")) {
    writeFile(LittleFS, "/FieldResistance.txt", String(FieldResistance).c_str());
  } else {
    FieldResistance = readFile(LittleFS, "/FieldResistance.txt").toFloat();
  }
  if (!fsExists("/MaximumAllowedBatteryAmps.txt")) {
    writeFile(LittleFS, "/MaximumAllowedBatteryAmps.txt", String(MaximumAllowedBatteryAmps).c_str());
  } else {
    MaximumAllowedBatteryAmps = readFile(LittleFS, "/MaximumAllowedBatteryAmps.txt").toInt();
  }
  if (!fsExists("/LoadDumpDtThresh1.txt")) {
    writeFile(LittleFS, "/LoadDumpDtThresh1.txt", String(LoadDumpDtThresh1).c_str());
  } else {
    LoadDumpDtThresh1 = readFile(LittleFS, "/LoadDumpDtThresh1.txt").toFloat();
  }
  if (!fsExists("/LoadDumpDtThresh.txt")) {
    writeFile(LittleFS, "/LoadDumpDtThresh.txt", String(LoadDumpDtThresh).c_str());
  } else {
    LoadDumpDtThresh = readFile(LittleFS, "/LoadDumpDtThresh.txt").toFloat();
  }
  if (!fsExists("/LoadDumpDtThresh3.txt")) {
    writeFile(LittleFS, "/LoadDumpDtThresh3.txt", String(LoadDumpDtThresh3).c_str());
  } else {
    LoadDumpDtThresh3 = readFile(LittleFS, "/LoadDumpDtThresh3.txt").toFloat();
  }
  if (!fsExists("/CVTuningMode.txt")) {
    writeFile(LittleFS, "/CVTuningMode.txt", String(CVTuningMode).c_str());
  } else {
    CVTuningMode = readFile(LittleFS, "/CVTuningMode.txt").toInt();
  }
  if (!fsExists("/cvWaveAmplitudeV.txt")) {
    writeFile(LittleFS, "/cvWaveAmplitudeV.txt", String(cvWaveAmplitudeV).c_str());
  } else {
    cvWaveAmplitudeV = readFile(LittleFS, "/cvWaveAmplitudeV.txt").toFloat();
  }
  if (!fsExists("/cvWavePeriodSec.txt")) {
    writeFile(LittleFS, "/cvWavePeriodSec.txt", String(cvWavePeriodSec).c_str());
  } else {
    cvWavePeriodSec = readFile(LittleFS, "/cvWavePeriodSec.txt").toInt();
  }
  if (!fsExists("/cvKOvershoot.txt")) {
    writeFile(LittleFS, "/cvKOvershoot.txt", String(cvKOvershoot).c_str());
  } else {
    cvKOvershoot = readFile(LittleFS, "/cvKOvershoot.txt").toFloat();
  }
  if (!fsExists("/cvConsecutiveReads.txt")) {
    writeFile(LittleFS, "/cvConsecutiveReads.txt", String(cvConsecutiveReads).c_str());
  } else {
    cvConsecutiveReads = (uint8_t)readFile(LittleFS, "/cvConsecutiveReads.txt").toInt();
  }
  if (!fsExists("/ThermalTuningMode.txt")) {
    writeFile(LittleFS, "/ThermalTuningMode.txt", String(ThermalTuningMode).c_str());
  } else {
    ThermalTuningMode = readFile(LittleFS, "/ThermalTuningMode.txt").toInt();
  }
  if (!fsExists("/thermalWaveLowF.txt")) {
    writeFile(LittleFS, "/thermalWaveLowF.txt", String(thermalWaveLowF, 1).c_str());
  } else {
    thermalWaveLowF = readFile(LittleFS, "/thermalWaveLowF.txt").toFloat();
  }
  if (!fsExists("/thermalWaveHighF.txt")) {
    writeFile(LittleFS, "/thermalWaveHighF.txt", String(thermalWaveHighF, 1).c_str());
  } else {
    thermalWaveHighF = readFile(LittleFS, "/thermalWaveHighF.txt").toFloat();
  }
  if (!fsExists("/thermalWaveHalfPeriodMin.txt")) {
    writeFile(LittleFS, "/thermalWaveHalfPeriodMin.txt", String(thermalWaveHalfPeriodMin, 1).c_str());
  } else {
    thermalWaveHalfPeriodMin = readFile(LittleFS, "/thermalWaveHalfPeriodMin.txt").toFloat();
  }
  if (!fsExists("/thermalKOvershoot.txt")) {
    writeFile(LittleFS, "/thermalKOvershoot.txt", String(thermalKOvershoot, 1).c_str());
  } else {
    thermalKOvershoot = readFile(LittleFS, "/thermalKOvershoot.txt").toFloat();
  }
  if (!fsExists("/thermalKUndershoot.txt")) {
    writeFile(LittleFS, "/thermalKUndershoot.txt", String(thermalKUndershoot, 1).c_str());
  } else {
    thermalKUndershoot = readFile(LittleFS, "/thermalKUndershoot.txt").toFloat();
  }
  if (!fsExists("/thermalSettleThreshF.txt")) {
    writeFile(LittleFS, "/thermalSettleThreshF.txt", String(thermalSettleThreshF, 1).c_str());
  } else {
    thermalSettleThreshF = readFile(LittleFS, "/thermalSettleThreshF.txt").toFloat();
  }
  if (!fsExists("/thermalConsecutiveReads.txt")) {
    writeFile(LittleFS, "/thermalConsecutiveReads.txt", String(thermalConsecutiveReads).c_str());
  } else {
    thermalConsecutiveReads = (uint8_t)readFile(LittleFS, "/thermalConsecutiveReads.txt").toInt();
  }
  if (!fsExists("/ManualSOCPoint.txt")) {
    writeFile(LittleFS, "/ManualSOCPoint.txt", String(ManualSOCPoint).c_str());
  } else {
    ManualSOCPoint = readFile(LittleFS, "/ManualSOCPoint.txt").toInt();
  }
  if (!fsExists("/ManualLifePercentage.txt")) {  // manual override of alterantor lifetime estimates this is pointless
    writeFile(LittleFS, "/ManualLifePercentage.txt", String(ManualLifePercentage).c_str());
  } else {
    ManualLifePercentage = readFile(LittleFS, "/ManualLifePercentage.txt").toInt();
  }
  if (!fsExists("/BatteryVoltageSource.txt")) {
    writeFile(LittleFS, "/BatteryVoltageSource.txt", String(BatteryVoltageSource).c_str());
  } else {
    BatteryVoltageSource = readFile(LittleFS, "/BatteryVoltageSource.txt").toInt();
  }
  if (!fsExists("/ShuntResistanceMicroOhm.txt")) {
    writeFile(LittleFS, "/ShuntResistanceMicroOhm.txt", String(ShuntResistanceMicroOhm).c_str());
  } else {
    ShuntResistanceMicroOhm = readFile(LittleFS, "/ShuntResistanceMicroOhm.txt").toInt();
  }
  if (!fsExists("/maxPoints.txt")) {
    writeFile(LittleFS, "/maxPoints.txt", String(maxPoints).c_str());
  } else {
    maxPoints = readFile(LittleFS, "/maxPoints.txt").toInt();
  }
  if (!fsExists("/Ymin1.txt")) {
    writeFile(LittleFS, "/Ymin1.txt", String(Ymin1).c_str());
  } else {
    Ymin1 = readFile(LittleFS, "/Ymin1.txt").toInt();
  }
  if (!fsExists("/Ymax1.txt")) {
    writeFile(LittleFS, "/Ymax1.txt", String(Ymax1).c_str());
  } else {
    Ymax1 = readFile(LittleFS, "/Ymax1.txt").toInt();
  }
  if (!fsExists("/Ymin2.txt")) {
    writeFile(LittleFS, "/Ymin2.txt", String(Ymin2).c_str());
  } else {
    Ymin2 = readFile(LittleFS, "/Ymin2.txt").toFloat();
  }
  if (!fsExists("/Ymax2.txt")) {
    writeFile(LittleFS, "/Ymax2.txt", String(Ymax2).c_str());
  } else {
    Ymax2 = readFile(LittleFS, "/Ymax2.txt").toFloat();
  }
  if (!fsExists("/Ymin3.txt")) {
    writeFile(LittleFS, "/Ymin3.txt", String(Ymin3).c_str());
  } else {
    Ymin3 = readFile(LittleFS, "/Ymin3.txt").toInt();
  }
  if (!fsExists("/Ymax3.txt")) {
    writeFile(LittleFS, "/Ymax3.txt", String(Ymax3).c_str());
  } else {
    Ymax3 = readFile(LittleFS, "/Ymax3.txt").toInt();
  }
  if (!fsExists("/Ymin4.txt")) {
    writeFile(LittleFS, "/Ymin4.txt", String(Ymin4).c_str());
  } else {
    Ymin4 = readFile(LittleFS, "/Ymin4.txt").toInt();
  }
  if (!fsExists("/Ymax4.txt")) {
    writeFile(LittleFS, "/Ymax4.txt", String(Ymax4).c_str());
  } else {
    Ymax4 = readFile(LittleFS, "/Ymax4.txt").toInt();
  }
  if (!fsExists("/MaxDuty.txt")) {
    writeFile(LittleFS, "/MaxDuty.txt", String(MaxDuty).c_str());
  } else {
    MaxDuty = readFile(LittleFS, "/MaxDuty.txt").toInt();
  }
  if (!fsExists("/MinDuty.txt")) {
    writeFile(LittleFS, "/MinDuty.txt", String(MinDuty).c_str());
  } else {
    MinDuty = readFile(LittleFS, "/MinDuty.txt").toInt();
  }
  if (!fsExists("/R_fixed.txt")) {
    writeFile(LittleFS, "/R_fixed.txt", String(R_fixed).c_str());
  } else {
    R_fixed = readFile(LittleFS, "/R_fixed.txt").toFloat();
  }
  if (!fsExists("/Beta.txt")) {
    writeFile(LittleFS, "/Beta.txt", String(Beta).c_str());
  } else {
    Beta = readFile(LittleFS, "/Beta.txt").toFloat();
  }
  if (!fsExists("/T0_C.txt")) {
    writeFile(LittleFS, "/T0_C.txt", String(T0_C).c_str());
  } else {
    T0_C = readFile(LittleFS, "/T0_C.txt").toFloat();
  }
  if (!fsExists("/TempSource.txt")) {
    writeFile(LittleFS, "/TempSource.txt", String(TempSource).c_str());
  } else {
    TempSource = readFile(LittleFS, "/TempSource.txt").toInt();
  }
  if (!fsExists("/AlternatorCOffset.txt")) {
    writeFile(LittleFS, "/AlternatorCOffset.txt", String(AlternatorCOffset).c_str());
  } else {
    AlternatorCOffset = readFile(LittleFS, "/AlternatorCOffset.txt").toFloat();
  }
  if (!fsExists("/BatteryCOffset.txt")) {
    writeFile(LittleFS, "/BatteryCOffset.txt", String(BatteryCOffset).c_str());
  } else {
    BatteryCOffset = readFile(LittleFS, "/BatteryCOffset.txt").toFloat();
  }
  if (!fsExists("/AlarmLatchEnabled.txt")) {
    writeFile(LittleFS, "/AlarmLatchEnabled.txt", String(AlarmLatchEnabled).c_str());
  } else {
    AlarmLatchEnabled = readFile(LittleFS, "/AlarmLatchEnabled.txt").toInt();
  }
  if (!fsExists("/absorptionCompleteTime.txt")) {
    writeFile(LittleFS, "/absorptionCompleteTime.txt", String(absorptionCompleteTime).c_str());
  } else {
    absorptionCompleteTime = readFile(LittleFS, "/absorptionCompleteTime.txt").toInt();
  }
  if (!fsExists("/FLOAT_DURATION.txt")) {
    writeFile(LittleFS, "/FLOAT_DURATION.txt", String(FLOAT_DURATION).c_str());
  } else {
    FLOAT_DURATION = readFile(LittleFS, "/FLOAT_DURATION.txt").toInt();
  }

  if (!fsExists("/RebulkCurrent_A.txt")) {
    writeFile(LittleFS, "/RebulkCurrent_A.txt", String(RebulkCurrent_A).c_str());
  } else {
    RebulkCurrent_A = readFile(LittleFS, "/RebulkCurrent_A.txt").toFloat();
  }
  if (!fsExists("/UseFloat.txt")) {
    writeFile(LittleFS, "/UseFloat.txt", String(UseFloat).c_str());
  } else {
    UseFloat = readFile(LittleFS, "/UseFloat.txt").toInt();
  }

  // ADD: Dynamic correction settings (add with other settings)
  if (!fsExists("/AutoShuntGainCorrection.txt")) {  // BOOLEAN
    writeFile(LittleFS, "/AutoShuntGainCorrection.txt", String(AutoShuntGainCorrection).c_str());
  } else {
    AutoShuntGainCorrection = readFile(LittleFS, "/AutoShuntGainCorrection.txt").toInt();
  }
  if (!fsExists("/AutoAltCurrentZero.txt")) {  // BOOLEAN
    writeFile(LittleFS, "/AutoAltCurrentZero.txt", String(AutoAltCurrentZero).c_str());
  } else {
    AutoAltCurrentZero = readFile(LittleFS, "/AutoAltCurrentZero.txt").toInt();
  }
  if (!fsExists("/WindingTempOffset.txt")) {
    writeFile(LittleFS, "/WindingTempOffset.txt", String(WindingTempOffset, 1).c_str());
  } else {
    WindingTempOffset = readFile(LittleFS, "/WindingTempOffset.txt").toFloat();
  }
  if (!fsExists("/displayTempUnit.txt")) {
    writeFile(LittleFS, "/displayTempUnit.txt", String(displayTempUnit).c_str());
  } else {
    displayTempUnit = (uint8_t)readFile(LittleFS, "/displayTempUnit.txt").toInt();
  }
  if (!fsExists("/PulleyRatio.txt")) {
    writeFile(LittleFS, "/PulleyRatio.txt", String(PulleyRatio, 2).c_str());
  } else {
    PulleyRatio = readFile(LittleFS, "/PulleyRatio.txt").toFloat();
  }
  if (!fsExists("/BatteryCurrentSource.txt")) {
    writeFile(LittleFS, "/BatteryCurrentSource.txt", String(BatteryCurrentSource).c_str());
  } else {
    BatteryCurrentSource = readFile(LittleFS, "/BatteryCurrentSource.txt").toInt();
  }

  if (!fsExists("/timeAxisModeChanging.txt")) {
    writeFile(LittleFS, "/timeAxisModeChanging.txt", String(timeAxisModeChanging).c_str());
  } else {
    timeAxisModeChanging = readFile(LittleFS, "/timeAxisModeChanging.txt").toInt();
  }
  if (!fsExists("/webgaugesinterval.txt")) {
    writeFile(LittleFS, "/webgaugesinterval.txt", String(webgaugesinterval).c_str());
  } else {
    webgaugesinterval = readFile(LittleFS, "/webgaugesinterval.txt").toInt();
    webgaugesinterval = constrain(webgaugesinterval, 1, 10000000);
  }
  if (!fsExists("/plotTimeWindow.txt")) {
    writeFile(LittleFS, "/plotTimeWindow.txt", String(plotTimeWindow).c_str());
  } else {
    plotTimeWindow = readFile(LittleFS, "/plotTimeWindow.txt").toInt();
    plotTimeWindow = constrain(plotTimeWindow, 1, 1000000);  // 10s to 10min
  }
  if (!fsExists("/LearningMode.txt")) {
    writeFile(LittleFS, "/LearningMode.txt", String(LearningMode).c_str());
  } else {
    LearningMode = readFile(LittleFS, "/LearningMode.txt").toInt();
  }
  if (!fsExists("/accelEnabled.txt")) {
    writeFile(LittleFS, "/accelEnabled.txt", String(accelEnabled).c_str());
  } else {
    accelEnabled = readFile(LittleFS, "/accelEnabled.txt").toInt();
  }

  if (!fsExists("/IgnoreLearningDuringPenalty.txt")) {
    writeFile(LittleFS, "/IgnoreLearningDuringPenalty.txt", String(IgnoreLearningDuringPenalty).c_str());
  } else {
    IgnoreLearningDuringPenalty = readFile(LittleFS, "/IgnoreLearningDuringPenalty.txt").toInt();
  }
  if (!fsExists("/CloudFeatures.txt")) {
    writeFile(LittleFS, "/CloudFeatures.txt", String(CloudFeatures).c_str());
  } else {
    CloudFeatures = readFile(LittleFS, "/CloudFeatures.txt").toInt();
  }
  // AutoSaveLearningTable — OBSOLETE REMOVE LATER (LittleFS init removed)
  // LearningPaused / LearningUpwardEnabled / LearningDownwardEnabled / EnableNeighborLearning /
  // ShowLearningDebugMessages / LearningDryRunMode boot-init removed — vars deleted (write-only no-consumers).
  // Existing on-disk /LearningX.txt files left as harmless orphans; no migration code (memory: no-unshipped-migration).
  if (!fsExists("/EnableAmbientCorrection.txt")) {
    writeFile(LittleFS, "/EnableAmbientCorrection.txt", String(EnableAmbientCorrection).c_str());
  } else {
    EnableAmbientCorrection = readFile(LittleFS, "/EnableAmbientCorrection.txt").toInt();
  }
  if (!fsExists("/TuningMode.txt")) {
    writeFile(LittleFS, "/TuningMode.txt", String(TuningMode).c_str());
  } else {
    TuningMode = readFile(LittleFS, "/TuningMode.txt").toInt();
  }
  if (!fsExists("/LogAllLearningEvents.txt")) {
    writeFile(LittleFS, "/LogAllLearningEvents.txt", String(LogAllLearningEvents).c_str());
  } else {
    LogAllLearningEvents = readFile(LittleFS, "/LogAllLearningEvents.txt").toInt();
  }
  if (!fsExists("/AlternatorNominalAmps.txt")) {
    writeFile(LittleFS, "/AlternatorNominalAmps.txt", String(AlternatorNominalAmps).c_str());
  } else {
    AlternatorNominalAmps = readFile(LittleFS, "/AlternatorNominalAmps.txt").toInt();
  }
  if (!fsExists("/LearningUpStep.txt")) {
    writeFile(LittleFS, "/LearningUpStep.txt", String(LearningUpStep, 2).c_str());
  } else {
    LearningUpStep = readFile(LittleFS, "/LearningUpStep.txt").toFloat();
  }
  if (!fsExists("/LearningDownStep.txt")) {
    writeFile(LittleFS, "/LearningDownStep.txt", String(LearningDownStep, 2).c_str());
  } else {
    LearningDownStep = readFile(LittleFS, "/LearningDownStep.txt").toFloat();
  }
  if (!fsExists("/AmbientTempCorrectionFactor.txt")) {
    writeFile(LittleFS, "/AmbientTempCorrectionFactor.txt", String(AmbientTempCorrectionFactor, 2).c_str());
  } else {
    AmbientTempCorrectionFactor = readFile(LittleFS, "/AmbientTempCorrectionFactor.txt").toFloat();
  }
  if (!fsExists("/xTime.txt")) {
    writeFile(LittleFS, "/xTime.txt", String(xTime, 2).c_str());
  } else {
    xTime = readFile(LittleFS, "/xTime.txt").toFloat();
  }
  if (!fsExists("/MinLearningInterval.txt")) {
    writeFile(LittleFS, "/MinLearningInterval.txt", String(MinLearningInterval).c_str());
  } else {
    MinLearningInterval = readFile(LittleFS, "/MinLearningInterval.txt").toInt();
  }
  if (!fsExists("/SafeOperationThreshold.txt")) {
    writeFile(LittleFS, "/SafeOperationThreshold.txt", String(SafeOperationThreshold).c_str());
  } else {
    SafeOperationThreshold = readFile(LittleFS, "/SafeOperationThreshold.txt").toInt();
  }
  if (!fsExists("/SetpointRiseRate.txt")) {
    writeFile(LittleFS, "/SetpointRiseRate.txt", String(SetpointRiseRate, 2).c_str());
  } else {
    SetpointRiseRate = readFile(LittleFS, "/SetpointRiseRate.txt").toFloat();
  }
  if (!fsExists("/SetpointFallRate.txt")) {
    writeFile(LittleFS, "/SetpointFallRate.txt", String(SetpointFallRate, 2).c_str());
  } else {
    SetpointFallRate = readFile(LittleFS, "/SetpointFallRate.txt").toFloat();
  }
  if (!fsExists("/StartupRiseRate.txt")) {
    writeFile(LittleFS, "/StartupRiseRate.txt", String(StartupRiseRate, 2).c_str());
  } else {
    StartupRiseRate = readFile(LittleFS, "/StartupRiseRate.txt").toFloat();
  }
  if (!fsExists("/PIDTrackingGain.txt")) {
    writeFile(LittleFS, "/PIDTrackingGain.txt", String(PIDTrackingGain, 2).c_str());
  } else {
    PIDTrackingGain = readFile(LittleFS, "/PIDTrackingGain.txt").toFloat();
  }
  if (!fsExists("/AbsorptionVoltage.txt")) {
    writeFile(LittleFS, "/AbsorptionVoltage.txt", String(AbsorptionVoltage).c_str());
  } else {
    AbsorptionVoltage = readFile(LittleFS, "/AbsorptionVoltage.txt").toFloat();
  }
  if (!fsExists("/TargetVoltageSetpoint.txt")) {
    writeFile(LittleFS, "/TargetVoltageSetpoint.txt", String(TargetVoltageSetpoint).c_str());
  } else {
    TargetVoltageSetpoint = readFile(LittleFS, "/TargetVoltageSetpoint.txt").toFloat();
  }
  if (!fsExists("/AbsorptionTimeoutMs.txt")) {
    writeFile(LittleFS, "/AbsorptionTimeoutMs.txt", String(AbsorptionTimeoutMs).c_str());
  } else {
    AbsorptionTimeoutMs = readFile(LittleFS, "/AbsorptionTimeoutMs.txt").toInt();
  }
  if (!fsExists("/bulkVoltageHoldMs.txt")) {
    writeFile(LittleFS, "/bulkVoltageHoldMs.txt", String(bulkVoltageHoldMs).c_str());
  } else {
    bulkVoltageHoldMs = readFile(LittleFS, "/bulkVoltageHoldMs.txt").toInt();
  }


  if (!fsExists("/VoltageKi.txt")) {
    writeFile(LittleFS, "/VoltageKi.txt", String(VoltageKi).c_str());
  } else {
    VoltageKi = readFile(LittleFS, "/VoltageKi.txt").toFloat();
  }
  // VoltageKd (D term) removed — LittleFS file /VoltageKd.txt no longer loaded.
  // ProtectionProxGateV removed 2026-05-22 — no longer used by any protection. See CV_Loop_Dev_Summary.md.
  if (!fsExists("/SlopeBleedThresh.txt")) {
    writeFile(LittleFS, "/SlopeBleedThresh.txt", String(SlopeBleedThresh, 3).c_str());
  } else {
    SlopeBleedThresh = readFile(LittleFS, "/SlopeBleedThresh.txt").toFloat();
  }
  if (!fsExists("/SlopeBleedK.txt")) {
    writeFile(LittleFS, "/SlopeBleedK.txt", String(SlopeBleedK, 1).c_str());
  } else {
    SlopeBleedK = readFile(LittleFS, "/SlopeBleedK.txt").toFloat();
  }
  if (!fsExists("/SlopeBleedProxV.txt")) {
    writeFile(LittleFS, "/SlopeBleedProxV.txt", String(SlopeBleedProxV, 2).c_str());
  } else {
    SlopeBleedProxV = readFile(LittleFS, "/SlopeBleedProxV.txt").toFloat();
  }
  if (!fsExists("/PidKp.txt")) {
    writeFile(LittleFS, "/PidKp.txt", String(PidKp, 3).c_str());
  } else {
    PidKp = readFile(LittleFS, "/PidKp.txt").toFloat();
  }
  if (!fsExists("/TempPIDKp.txt")) {
    writeFile(LittleFS, "/TempPIDKp.txt", String(TempPIDKp, 6).c_str());
  } else {
    TempPIDKp = readFile(LittleFS, "/TempPIDKp.txt").toFloat();
  }
  if (!fsExists("/ThermalLookaheadSec.txt")) {
    writeFile(LittleFS, "/ThermalLookaheadSec.txt", String(ThermalLookaheadSec, 1).c_str());
  } else {
    ThermalLookaheadSec = max(0.0f, readFile(LittleFS, "/ThermalLookaheadSec.txt").toFloat());
  }

  if (!fsExists("/TempPIDKi.txt")) {
    writeFile(LittleFS, "/TempPIDKi.txt", String(TempPIDKi, 6).c_str());
  } else {
    TempPIDKi = readFile(LittleFS, "/TempPIDKi.txt").toFloat();
  }


  if (!fsExists("/TempPIDIntervalMs.txt")) {
    writeFile(LittleFS, "/TempPIDIntervalMs.txt", String(TempPIDIntervalMs).c_str());
  } else {
    TempPIDIntervalMs = readFile(LittleFS, "/TempPIDIntervalMs.txt").toInt();
  }

  if (!fsExists("/TempPIDFilterAlpha.txt")) {
    writeFile(LittleFS, "/TempPIDFilterAlpha.txt", String(TempPIDFilterAlpha, 3).c_str());
  } else {
    TempPIDFilterAlpha = readFile(LittleFS, "/TempPIDFilterAlpha.txt").toFloat();
  }


  if (!fsExists("/PidKi.txt")) {
    writeFile(LittleFS, "/PidKi.txt", String(PidKi, 3).c_str());
  } else {
    PidKi = readFile(LittleFS, "/PidKi.txt").toFloat();
  }
  if (!fsExists("/PidKd.txt")) {
    writeFile(LittleFS, "/PidKd.txt", String(PidKd, 3).c_str());
  } else {
    PidKd = readFile(LittleFS, "/PidKd.txt").toFloat();
  }
  if (!fsExists("/DutySlowRampRate.txt")) {
    writeFile(LittleFS, "/DutySlowRampRate.txt", String(DutySlowRampRate, 2).c_str());
  } else {
    DutySlowRampRate = readFile(LittleFS, "/DutySlowRampRate.txt").toFloat();
  }
  if (!fsExists("/ShutdownPhase2HoldMs.txt")) {
    writeFile(LittleFS, "/ShutdownPhase2HoldMs.txt", String(ShutdownPhase2HoldMs).c_str());
  } else {
    ShutdownPhase2HoldMs = readFile(LittleFS, "/ShutdownPhase2HoldMs.txt").toInt();
  }
  if (!fsExists("/PidSampleDivisor.txt")) {
    writeFile(LittleFS, "/PidSampleDivisor.txt", String(PidSampleDivisor).c_str());
  } else {
    PidSampleDivisor = readFile(LittleFS, "/PidSampleDivisor.txt").toInt();
  }
  // Add after other learning settings:
  if (!fsExists("/LearningSettlingPeriod.txt")) {
    writeFile(LittleFS, "/LearningSettlingPeriod.txt", String(LearningSettlingPeriod).c_str());
  } else {
    LearningSettlingPeriod = readFile(LittleFS, "/LearningSettlingPeriod.txt").toInt();
  }
  if (!fsExists("/LearningRPMChangeThreshold.txt")) {
    writeFile(LittleFS, "/LearningRPMChangeThreshold.txt", String(LearningRPMChangeThreshold).c_str());
  } else {
    LearningRPMChangeThreshold = readFile(LittleFS, "/LearningRPMChangeThreshold.txt").toInt();
  }
  if (!fsExists("/LearningTempHysteresis.txt")) {
    writeFile(LittleFS, "/LearningTempHysteresis.txt", String(LearningTempHysteresis).c_str());
  } else {
    LearningTempHysteresis = readFile(LittleFS, "/LearningTempHysteresis.txt").toInt();
  }
  if (!fsExists("/MaxTableValue.txt")) {
    writeFile(LittleFS, "/MaxTableValue.txt", String(MaxTableValue, 2).c_str());
  } else {
    MaxTableValue = readFile(LittleFS, "/MaxTableValue.txt").toFloat();
  }
  HardOCTripAmps = MaxTableValue + 10.0f;  // always derived, not persisted
  // MinTableValue — OBSOLETE REMOVE LATER (LittleFS init removed)
  if (!fsExists("/MaxPenaltyPercent.txt")) {
    writeFile(LittleFS, "/MaxPenaltyPercent.txt", String(MaxPenaltyPercent, 2).c_str());
  } else {
    MaxPenaltyPercent = readFile(LittleFS, "/MaxPenaltyPercent.txt").toFloat();
  }
  if (!fsExists("/MaxPenaltyDuration.txt")) {
    writeFile(LittleFS, "/MaxPenaltyDuration.txt", String(MaxPenaltyDuration).c_str());
  } else {
    MaxPenaltyDuration = readFile(LittleFS, "/MaxPenaltyDuration.txt").toInt();
  }
  if (!fsExists("/NeighborLearningFactor.txt")) {
    writeFile(LittleFS, "/NeighborLearningFactor.txt", String(NeighborLearningFactor, 3).c_str());
  } else {
    NeighborLearningFactor = readFile(LittleFS, "/NeighborLearningFactor.txt").toFloat();
  }
  if (!fsExists("/yyMax.txt")) {
    writeFile(LittleFS, "/yyMax.txt", String(yyMax).c_str());
  } else {
    yyMax = readFile(LittleFS, "/yyMax.txt").toInt();
  }
  if (!fsExists("/LearningMemoryDuration.txt")) {
    writeFile(LittleFS, "/LearningMemoryDuration.txt", String(LearningMemoryDuration).c_str());
  } else {
    LearningMemoryDuration = readFile(LittleFS, "/LearningMemoryDuration.txt").toInt();
  }
  // LearningTableSaveInterval — OBSOLETE REMOVE LATER (LittleFS init removed)
  if (!fsExists("/SetpointRampRate.txt")) {
    writeFile(LittleFS, "/SetpointRampRate.txt", String(SetpointRampRate, 1).c_str());
  } else {
    SetpointRampRate = readFile(LittleFS, "/SetpointRampRate.txt").toFloat();
  }
  if (!fsExists("/DutyRampRate.txt")) {
    writeFile(LittleFS, "/DutyRampRate.txt", String(DutyRampRate, 1).c_str());
  } else {
    DutyRampRate = readFile(LittleFS, "/DutyRampRate.txt").toFloat();
  }
  if (!fsExists("/SettleTimeBeforeCut.txt")) {
    writeFile(LittleFS, "/SettleTimeBeforeCut.txt", String(SettleTimeBeforeCut).c_str());
  } else {
    SettleTimeBeforeCut = readFile(LittleFS, "/SettleTimeBeforeCut.txt").toInt();
  }
  if (!fsExists("/TempWarnExcess.txt")) {
    writeFile(LittleFS, "/TempWarnExcess.txt", String(TempWarnExcess, 1).c_str());
  } else {
    TempWarnExcess = readFile(LittleFS, "/TempWarnExcess.txt").toFloat();
  }
  if (!fsExists("/TempCritExcess.txt")) {
    writeFile(LittleFS, "/TempCritExcess.txt", String(TempCritExcess, 1).c_str());
  } else {
    TempCritExcess = readFile(LittleFS, "/TempCritExcess.txt").toFloat();
  }
  if (!fsExists("/TempSustainedTimeout.txt")) {
    writeFile(LittleFS, "/TempSustainedTimeout.txt", String(TempSustainedTimeout).c_str());
  } else {
    TempSustainedTimeout = readFile(LittleFS, "/TempSustainedTimeout.txt").toInt();
  }
  // AlternatorHardShutdownV — absolute hard-shutdown voltage threshold.
  // First-boot default auto-scales as BulkVoltage + 0.3 V so 24V and 48V systems get
  // sensible defaults (29.1 V / 57.9 V) instead of the 12V-only static 14.8 V seed.
  // Once written to LittleFS the value is treated as user-set and never auto-overwritten —
  // a system-class change later requires manually re-setting it from the UI.
  // Migration: if the old /VoltageSpikeMargin.txt exists and the new file does not, convert
  // the stored margin to an absolute value (BulkVoltage was loaded earlier in this function).
  if (!fsExists("/AlternatorHardShutdownV.txt")) {
    if (fsExists("/VoltageSpikeMargin.txt")) {
      float oldMargin = readFile(LittleFS, "/VoltageSpikeMargin.txt").toFloat();
      AlternatorHardShutdownV = BulkVoltage + oldMargin;
    } else {
      AlternatorHardShutdownV = BulkVoltage + 0.3f;  // first-boot auto-scale default
    }
    writeFile(LittleFS, "/AlternatorHardShutdownV.txt", String(AlternatorHardShutdownV, 2).c_str());
  } else {
    AlternatorHardShutdownV = readFile(LittleFS, "/AlternatorHardShutdownV.txt").toFloat();
  }
  if (!fsExists("/HardOCDebounceMs.txt")) {
    writeFile(LittleFS, "/HardOCDebounceMs.txt", String(HardOCDebounceMs).c_str());
  } else {
    HardOCDebounceMs = (uint32_t)readFile(LittleFS, "/HardOCDebounceMs.txt").toInt();
  }
  if (!fsExists("/WarmupRampRate.txt")) {
    writeFile(LittleFS, "/WarmupRampRate.txt", String(WarmupRampRate, 2).c_str());
  } else {
    WarmupRampRate = max(0.0f, readFile(LittleFS, "/WarmupRampRate.txt").toFloat());
  }
  if (!fsExists("/IExcessK.txt")) {
    writeFile(LittleFS, "/IExcessK.txt", String(IExcessK, 1).c_str());
  } else {
    IExcessK = readFile(LittleFS, "/IExcessK.txt").toFloat();
  }
  if (!fsExists("/IExcessN.txt")) {
    writeFile(LittleFS, "/IExcessN.txt", String(IExcessN).c_str());
  } else {
    IExcessN = (int)readFile(LittleFS, "/IExcessN.txt").toInt();
  }
  if (!fsExists("/IExcessKBleed.txt")) {
    writeFile(LittleFS, "/IExcessKBleed.txt", String(IExcessKBleed, 2).c_str());
  } else {
    IExcessKBleed = readFile(LittleFS, "/IExcessKBleed.txt").toFloat();
  }
  if (!fsExists("/IExcessArmMarginV.txt")) {
    writeFile(LittleFS, "/IExcessArmMarginV.txt", String(IExcessArmMarginV, 3).c_str());
  } else {
    IExcessArmMarginV = readFile(LittleFS, "/IExcessArmMarginV.txt").toFloat();
  }
  if (!fsExists("/AwBleedRate.txt")) {
    writeFile(LittleFS, "/AwBleedRate.txt", String(AwBleedRate, 2).c_str());
  } else {
    AwBleedRate = readFile(LittleFS, "/AwBleedRate.txt").toFloat();
  }
  // AwRecoverRate is hardcoded (0.1f) — no LittleFS persistence
  if (!fsExists("/AwSeedProtectMs.txt")) {
    writeFile(LittleFS, "/AwSeedProtectMs.txt", String(AwSeedProtectMs).c_str());
  } else {
    AwSeedProtectMs = (uint16_t)readFile(LittleFS, "/AwSeedProtectMs.txt").toInt();
  }
  if (!fsExists("/FastSetpointRiseRate.txt")) {
    writeFile(LittleFS, "/FastSetpointRiseRate.txt", String(FastSetpointRiseRate, 1).c_str());
  } else {
    FastSetpointRiseRate = readFile(LittleFS, "/FastSetpointRiseRate.txt").toFloat();
  }
  if (!fsExists("/FastSetpointRiseWindowMs.txt")) {
    writeFile(LittleFS, "/FastSetpointRiseWindowMs.txt", String(FastSetpointRiseWindowMs).c_str());
  } else {
    FastSetpointRiseWindowMs = (uint32_t)readFile(LittleFS, "/FastSetpointRiseWindowMs.txt").toInt();
  }
  if (!fsExists("/FastSetpointRiseHeadroomV.txt")) {
    writeFile(LittleFS, "/FastSetpointRiseHeadroomV.txt", String(FastSetpointRiseHeadroomV, 2).c_str());
  } else {
    FastSetpointRiseHeadroomV = readFile(LittleFS, "/FastSetpointRiseHeadroomV.txt").toFloat();
  }
  if (!fsExists("/KHard.txt")) {
    writeFile(LittleFS, "/KHard.txt", String(KHard, 1).c_str());
  } else {
    KHard = readFile(LittleFS, "/KHard.txt").toFloat();
  }
  // ReseedFrac (was IExcessReseedFrac) — migrates from old filename if present
  if (fsExists("/ReseedFrac.txt")) {
    ReseedFrac = readFile(LittleFS, "/ReseedFrac.txt").toFloat();
  } else if (fsExists("/IExcessReseedFrac.txt")) {
    ReseedFrac = readFile(LittleFS, "/IExcessReseedFrac.txt").toFloat();
    writeFile(LittleFS, "/ReseedFrac.txt", String(ReseedFrac, 2).c_str());
  } else {
    writeFile(LittleFS, "/ReseedFrac.txt", String(ReseedFrac, 2).c_str());
  }
  // OvGroup1Enable — migrates from old /OvLayer2Enable.txt if found
  if (fsExists("/OvGroup1Enable.txt")) {
    OvGroup1Enable = readFile(LittleFS, "/OvGroup1Enable.txt").toInt() != 0;
  } else if (fsExists("/OvLayer2Enable.txt")) {
    OvGroup1Enable = readFile(LittleFS, "/OvLayer2Enable.txt").toInt() != 0;
    writeFile(LittleFS, "/OvGroup1Enable.txt", String((int)OvGroup1Enable).c_str());
  } else {
    writeFile(LittleFS, "/OvGroup1Enable.txt", String((int)OvGroup1Enable).c_str());
  }
  // OvGroup2Enable — migrates from old /OvLayer3Enable.txt if found
  if (fsExists("/OvGroup2Enable.txt")) {
    OvGroup2Enable = readFile(LittleFS, "/OvGroup2Enable.txt").toInt() != 0;
  } else if (fsExists("/OvLayer3Enable.txt")) {
    OvGroup2Enable = readFile(LittleFS, "/OvLayer3Enable.txt").toInt() != 0;
    writeFile(LittleFS, "/OvGroup2Enable.txt", String((int)OvGroup2Enable).c_str());
  } else {
    writeFile(LittleFS, "/OvGroup2Enable.txt", String((int)OvGroup2Enable).c_str());
  }
  if (!fsExists("/IExcessSigSrc.txt")) {
    writeFile(LittleFS, "/IExcessSigSrc.txt", String(IExcessSigSrc).c_str());
  } else {
    IExcessSigSrc = constrain(readFile(LittleFS, "/IExcessSigSrc.txt").toInt(), 0, 2);
  }
  if (!fsExists("/IExcessMA_N.txt")) {
    writeFile(LittleFS, "/IExcessMA_N.txt", String(IExcessMA_N).c_str());
  } else {
    IExcessMA_N = constrain(readFile(LittleFS, "/IExcessMA_N.txt").toInt(), 1, I_RING_SIZE);
  }
  if (!fsExists("/OutputPIDSigSrc.txt")) {
    writeFile(LittleFS, "/OutputPIDSigSrc.txt", String(OutputPIDSigSrc).c_str());
  } else {
    OutputPIDSigSrc = constrain(readFile(LittleFS, "/OutputPIDSigSrc.txt").toInt(), 0, 2);
  }
  if (!fsExists("/OutputPIDMA_N.txt")) {
    writeFile(LittleFS, "/OutputPIDMA_N.txt", String(OutputPIDMA_N).c_str());
  } else {
    OutputPIDMA_N = constrain(readFile(LittleFS, "/OutputPIDMA_N.txt").toInt(), 1, I_RING_SIZE);
  }
  if (!fsExists("/OutputPIDFilterTC.txt")) {
    writeFile(LittleFS, "/OutputPIDFilterTC.txt", String(OutputPIDFilterTC).c_str());
  } else {
    OutputPIDFilterTC = readFile(LittleFS, "/OutputPIDFilterTC.txt").toFloat();
  }
  if (!fsExists("/VoltageFilterTC.txt")) {
    writeFile(LittleFS, "/VoltageFilterTC.txt", String(VoltageFilterTC).c_str());
  } else {
    VoltageFilterTC = readFile(LittleFS, "/VoltageFilterTC.txt").toFloat();
  }
  if (!fsExists("/TdPred.txt")) {
    writeFile(LittleFS, "/TdPred.txt", String(TdPred, 3).c_str());
  } else {
    TdPred = readFile(LittleFS, "/TdPred.txt").toFloat();
  }
  if (!fsExists("/OvMeasMarginV.txt")) {
    if (fsExists("/VSoftMarginV.txt")) { OvMeasMarginV = readFile(LittleFS, "/VSoftMarginV.txt").toFloat(); }
    writeFile(LittleFS, "/OvMeasMarginV.txt", String(OvMeasMarginV, 3).c_str());
  } else {
    OvMeasMarginV = readFile(LittleFS, "/OvMeasMarginV.txt").toFloat();
  }
  if (!fsExists("/OvPredMarginV.txt")) {
    if (fsExists("/VHardMarginV.txt")) { OvPredMarginV = readFile(LittleFS, "/VHardMarginV.txt").toFloat(); }
    writeFile(LittleFS, "/OvPredMarginV.txt", String(OvPredMarginV, 3).c_str());
  } else {
    OvPredMarginV = readFile(LittleFS, "/OvPredMarginV.txt").toFloat();
  }
  // DvdtTC (was DvdtAlpha) — migrates from old alpha-based file if present.
  // Conversion: TC = 5ms × (1 − α) / α  (preserves behavior at 5ms nominal cadence).
  if (fsExists("/DvdtTC.txt")) {
    DvdtTC = constrain(readFile(LittleFS, "/DvdtTC.txt").toFloat(), 5.0f, 500.0f);
  } else if (fsExists("/DvdtAlpha.txt")) {
    float oldAlpha = constrain(readFile(LittleFS, "/DvdtAlpha.txt").toFloat(), 0.01f, 0.50f);
    DvdtTC = constrain(5.0f * (1.0f - oldAlpha) / oldAlpha, 5.0f, 500.0f);
    writeFile(LittleFS, "/DvdtTC.txt", String(DvdtTC, 1).c_str());
  } else {
    writeFile(LittleFS, "/DvdtTC.txt", String(DvdtTC, 1).c_str());
  }
  if (!fsExists("/VoltageDisagreeThreshold.txt")) {
    writeFile(LittleFS, "/VoltageDisagreeThreshold.txt", String(VoltageDisagreeThreshold, 2).c_str());
  } else {
    VoltageDisagreeThreshold = readFile(LittleFS, "/VoltageDisagreeThreshold.txt").toFloat();
  }
  if (!fsExists("/VoltageDisagreeTimeout.txt")) {
    writeFile(LittleFS, "/VoltageDisagreeTimeout.txt", String(VoltageDisagreeTimeout).c_str());
  } else {
    VoltageDisagreeTimeout = readFile(LittleFS, "/VoltageDisagreeTimeout.txt").toInt();
  }
  // Anomaly margin amps
  if (!fsExists("/anomalyMarginAmps.txt")) {
    writeFile(LittleFS, "/anomalyMarginAmps.txt", String(anomalyMarginAmps, 4).c_str());
  } else {
    anomalyMarginAmps = readFile(LittleFS, "/anomalyMarginAmps.txt").toFloat();
  }

  if (!fsExists("/degradationThresh.txt")) {
    writeFile(LittleFS, "/degradationThresh.txt",
              String(degradationThreshold, 4).c_str());
  } else {
    degradationThreshold =
      readFile(LittleFS, "/degradationThresh.txt").toFloat();
  }

  // Anomaly alarm threshold (session error count)
  if (!fsExists("/anomalyAlarmThresh.txt")) {
    writeFile(LittleFS, "/anomalyAlarmThresh.txt", String(anomalyAlarmThreshold).c_str());
  } else {
    anomalyAlarmThreshold = (int)readFile(LittleFS, "/anomalyAlarmThresh.txt").toInt();
  }

  // Anomaly alarm enable
  if (!fsExists("/anomalyAlarmEnable.txt")) {
    writeFile(LittleFS, "/anomalyAlarmEnable.txt", String((int)anomalyAlarmEnable).c_str());
  } else {
    anomalyAlarmEnable = (readFile(LittleFS, "/anomalyAlarmEnable.txt").toInt() != 0);
  }

  if (!fsExists("/VoltageKp.txt")) {
    writeFile(LittleFS, "/VoltageKp.txt", String(VoltageKp, 1).c_str());
  } else {
    VoltageKp = readFile(LittleFS, "/VoltageKp.txt").toFloat();
  }
  if (!fsExists("/VoltageTrimLimit.txt")) {
    writeFile(LittleFS, "/VoltageTrimLimit.txt", String(VoltageTrimLimit, 1).c_str());
  } else {
    VoltageTrimLimit = readFile(LittleFS, "/VoltageTrimLimit.txt").toFloat();
  }
  if (!fsExists("/VoltageLoopInterval.txt")) {
    writeFile(LittleFS, "/VoltageLoopInterval.txt", String(VoltageLoopInterval).c_str());
  } else {
    VoltageLoopInterval = readFile(LittleFS, "/VoltageLoopInterval.txt").toInt();
  }
  if (!fsExists("/FIELD_COLLAPSE_DELAY.txt")) {
    writeFile(LittleFS, "/FIELD_COLLAPSE_DELAY.txt", String(FIELD_COLLAPSE_DELAY).c_str());
  } else {
    FIELD_COLLAPSE_DELAY = readFile(LittleFS, "/FIELD_COLLAPSE_DELAY.txt").toInt();
  }
  if (!fsExists("/EffXMin.txt")) {
    writeFile(LittleFS, "/EffXMin.txt", String(EffXMin, 2).c_str());
  } else {
    EffXMin = readFile(LittleFS, "/EffXMin.txt").toFloat();
  }
  if (!fsExists("/EffXMax.txt")) {
    writeFile(LittleFS, "/EffXMax.txt", String(EffXMax, 2).c_str());
  } else {
    EffXMax = readFile(LittleFS, "/EffXMax.txt").toFloat();
  }
  if (!fsExists("/EffYMin.txt")) {
    writeFile(LittleFS, "/EffYMin.txt", String(EffYMin, 2).c_str());
  } else {
    EffYMin = readFile(LittleFS, "/EffYMin.txt").toFloat();
  }
  if (!fsExists("/EffYMax.txt")) {
    writeFile(LittleFS, "/EffYMax.txt", String(EffYMax, 2).c_str());
  } else {
    EffYMax = readFile(LittleFS, "/EffYMax.txt").toFloat();
  }
  // IMU safety thresholds — user-set via form, persisted to LittleFS (Pattern B).
  // imuMountOrientation rides on /vessel_info.json (separate path).
  if (!fsExists("/CAPSIZE_THRESHOLD_DEG.txt")) {
    writeFile(LittleFS, "/CAPSIZE_THRESHOLD_DEG.txt", String(CAPSIZE_THRESHOLD_DEG, 1).c_str());
  } else {
    CAPSIZE_THRESHOLD_DEG = readFile(LittleFS, "/CAPSIZE_THRESHOLD_DEG.txt").toFloat();
  }
  if (!fsExists("/PITCHPOLE_THRESHOLD_DEG.txt")) {
    writeFile(LittleFS, "/PITCHPOLE_THRESHOLD_DEG.txt", String(PITCHPOLE_THRESHOLD_DEG, 1).c_str());
  } else {
    PITCHPOLE_THRESHOLD_DEG = readFile(LittleFS, "/PITCHPOLE_THRESHOLD_DEG.txt").toFloat();
  }
  if (!fsExists("/SLAM_THRESHOLD_G.txt")) {
    writeFile(LittleFS, "/SLAM_THRESHOLD_G.txt", String(SLAM_THRESHOLD_G, 2).c_str());
  } else {
    SLAM_THRESHOLD_G = readFile(LittleFS, "/SLAM_THRESHOLD_G.txt").toFloat();
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
// Verification status:
//   0: VERIFIED from physical tilt tests
//   1: DERIVED (geometric 180° flip of orientation 0) - retest when possible
//   2: VERIFIED from physical tilt tests
//   3: VERIFIED from physical tilt tests
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
constexpr float CF_ALPHA = 0.90f;  // Gyro weight (0.90 = trust gyro 90%, accel 10%)  time constant ~0.19 seconds, Feels instant on the bench. Will show some wave flutter at sea but still usable.

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
  // Complementary filter for heel and pitch estimation
  // Accel-derived angles (noisy but no drift)
  float accel_heel = atan2(ay, sqrt(ax * ax + az * az)) * 180.0f / PI;
  float accel_pitch = atan2(-ax, sqrt(ay * ay + az * az)) * 180.0f / PI;

  // Integrate gyro (smooth but drifts)
  cf_heel += gy * dt;
  cf_pitch += gx * dt;

  // Fuse: trust gyro 98%, accel 2%
  cf_heel = CF_ALPHA * cf_heel + (1.0f - CF_ALPHA) * accel_heel;
  cf_pitch = CF_ALPHA * cf_pitch + (1.0f - CF_ALPHA) * accel_pitch;
}
void updateAccelMetrics() {
  // Process all available ring buffer samples
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

    // Scale to integers (1.234g → 1234)
    int32_t ax_scaled = (int32_t)(ax * 1000.0f);
    int32_t ay_scaled = (int32_t)(ay * 1000.0f);
    int32_t az_scaled = (int32_t)(az * 1000.0f);

    // Update window statistics with time-weighted averaging
    if (dt_us > 0 && dt_us < 100000) {  // Sanity check: 0-100ms
      // Accel X
      if (ax_scaled < imuWindow->accel_x_min) imuWindow->accel_x_min = ax_scaled;
      if (ax_scaled > imuWindow->accel_x_max) imuWindow->accel_x_max = ax_scaled;
      imuWindow->accel_x_area_v_us += (int64_t)ax_scaled * dt_us;
      imuWindow->accel_x_valid_us += dt_us;

      // Accel Y
      if (ay_scaled < imuWindow->accel_y_min) imuWindow->accel_y_min = ay_scaled;
      if (ay_scaled > imuWindow->accel_y_max) imuWindow->accel_y_max = ay_scaled;
      imuWindow->accel_y_area_v_us += (int64_t)ay_scaled * dt_us;
      imuWindow->accel_y_valid_us += dt_us;

      // Accel Z
      if (az_scaled < imuWindow->accel_z_min) imuWindow->accel_z_min = az_scaled;
      if (az_scaled > imuWindow->accel_z_max) imuWindow->accel_z_max = az_scaled;
      imuWindow->accel_z_area_v_us += (int64_t)az_scaled * dt_us;
      imuWindow->accel_z_valid_us += dt_us;

      // Total acceleration magnitude
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

    // TODO: Wave period decimation and processing

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

    // Scale to integers (12.34 dps → 1234)
    int32_t gx_scaled = (int32_t)(gx * 100.0f);
    int32_t gy_scaled = (int32_t)(gy * 100.0f);
    int32_t gz_scaled = (int32_t)(gz * 100.0f);

    // Update window statistics
    if (dt_us > 0 && dt_us < 100000) {  // Sanity check
      // Gyro X
      if (gx_scaled < imuWindow->gyro_x_min) imuWindow->gyro_x_min = gx_scaled;
      if (gx_scaled > imuWindow->gyro_x_max) imuWindow->gyro_x_max = gx_scaled;
      imuWindow->gyro_x_area_v_us += (int64_t)gx_scaled * dt_us;
      imuWindow->gyro_x_valid_us += dt_us;

      // Gyro Y
      if (gy_scaled < imuWindow->gyro_y_min) imuWindow->gyro_y_min = gy_scaled;
      if (gy_scaled > imuWindow->gyro_y_max) imuWindow->gyro_y_max = gy_scaled;
      imuWindow->gyro_y_area_v_us += (int64_t)gy_scaled * dt_us;
      imuWindow->gyro_y_valid_us += dt_us;

      // Gyro Z
      if (gz_scaled < imuWindow->gyro_z_min) imuWindow->gyro_z_min = gz_scaled;
      if (gz_scaled > imuWindow->gyro_z_max) imuWindow->gyro_z_max = gz_scaled;
      imuWindow->gyro_z_area_v_us += (int64_t)gz_scaled * dt_us;
      imuWindow->gyro_z_valid_us += dt_us;
    }

    // Store current raw values
    imu_gyro_x_raw = gx;
    imu_gyro_y_raw = gy;
    imu_gyro_z_raw = gz;
    imu_yaw_rate_dps = gz;  // Direct from gyro Z

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

    imuWindow->lastGyroUpdateTime_us = now_us;  // separate from accel tracker
    imuRingBuffer->gyro_tail = (imuRingBuffer->gyro_tail + 1) % GYRO_RING_SIZE;
  }

  // Update current display values
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

      // Update display values
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

  // Update lifetime maximums
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
// ============================================================================
// STUB FUNCTIONS - IMPLEMENT LATER
// ============================================================================
void updateWavePeriod() {
  // Implemented inline in updateAccelMetrics() — 10 Hz decimation, DC EWMA, zero-crossing with hysteresis
}
void updateVibrationEnergy() {
  // Implemented inline in updateAccelMetrics() — HP filter + RMS² + EWMA per accel sample batch
}


//OTA UPDATES--  well, some other stuff first related to authenticating tokens for cloud
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
// Base64 decode function
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
  // Decode the signature from base64
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

  // Parse and verify with RSA public key
  mbedtls_pk_init(&pk);

  ret = mbedtls_pk_parse_public_key(&pk, (const unsigned char *)OTA_PUBLIC_KEY, strlen(OTA_PUBLIC_KEY) + 1);
  if (ret != 0) {
    Serial.printf("SECURITY: Failed to parse public key: -0x%04x\n", -ret);
    mbedtls_pk_free(&pk);
    return false;
  }

  // Verify the signature
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
UpdateInfo parseUpdateResponse(const String &jsonResponse) {
  UpdateInfo info = { false, "", "", "", "", 0 };

  DynamicJsonDocument doc(2048);
  DeserializationError error = deserializeJson(doc, jsonResponse.c_str(), jsonResponse.length());

  if (error) {
    Serial.printf("JSON parse error: %s\n", error.c_str());
    return info;
  }

  if (doc["hasUpdate"].as<bool>()) {
    info.hasUpdate = true;
    info.version = doc["version"].as<String>();
    info.firmwareUrl = doc["firmwareUrl"].as<String>();
    info.signatureUrl = doc["signatureUrl"].as<String>();
    info.changelog = doc["changelog"].as<String>();
    info.firmwareSize = doc["firmwareSize"].as<size_t>();

    Serial.printf("🔄 UPDATE AVAILABLE: %s -> %s\n", FIRMWARE_VERSION, info.version.c_str());
    Serial.printf("📦 Firmware size: %d bytes\n", info.firmwareSize);
    Serial.printf("📝 Changelog: %s\n", info.changelog.c_str());
  }

  return info;
}

// Perform actual firmware download and install
// Initialize streaming extractor
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
// Parse tar header to get file info
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

  // Clean filename
  String filename = String(rawFilename);
  filename.trim();

  // Remove leading "./" if present
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

  // Parse octal size manually
  extractor->currentFileSize = 0;
  for (int i = 0; i < 12 && sizeStr[i] != '\0' && sizeStr[i] != ' '; i++) {
    if (sizeStr[i] >= '0' && sizeStr[i] <= '7') {
      extractor->currentFileSize = extractor->currentFileSize * 8 + (sizeStr[i] - '0');
    }
  }

  extractor->currentFilePos = 0;

  // Check if this is the firmware file
  extractor->isCurrentFileFirmware = extractor->currentFileName.equals("firmware.bin");

  Serial.printf("📁 Found file: %s (%d bytes) type='%c'\n",
                extractor->currentFileName.c_str(), extractor->currentFileSize, typeFlag);

  // Route file to appropriate destination
  if (extractor->currentFileName.equals("firmware.bin")) {
    // Initialize OTA partition write
    extractor->otaPartition = esp_partition_find_first(ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_APP_OTA_0, NULL);
    esp_ota_begin(extractor->otaPartition, extractor->currentFileSize, &extractor->otaHandle);
    extractor->otaStarted = true;
    extractor->isCurrentFileFirmware = true;
  } else if (extractor->currentFileName.indexOf('.') > 0) {
    // Mount prod_fs for web file updates
    if (!extractor->prodFSMounted) {
      webFS.end();  // End any existing mount
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
// Process tar data chunk
bool processDataChunk(StreamingExtractor *extractor, uint8_t *data, size_t dataSize) {
  size_t processed = 0;

  while (processed < dataSize) {
    if (extractor->inPadding) {
      // Skip padding bytes
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
      // Still reading tar header
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

        // Not end of archive, parse the header
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
          // Write to firmware partition
          esp_err_t err = esp_ota_write(extractor->otaHandle, data + processed, toWrite);
          if (err != ESP_OK) {
            Serial.printf("❌ OTA write failed: %s\n", esp_err_to_name(err));
            return false;
          }

        } else if (extractor->currentWebFile) {
          // Write to web file
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

      // Check if current file is complete
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
  otaInProgress = true;  // ADD THIS at top
  // NEW: Close EventSource FIRST (before any heap measurements)
  Serial.println("Closing EventSource connections...");
  events.close();
  delay(100);

  Serial.println("🧹 Preparing system for OTA - freeing memory...");
  Serial.printf("Heap BEFORE cleanup: %u bytes\n", ESP.getFreeHeap());

  // Delete tasks
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

  // Additional safety check for current mode
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

  Serial.println("7. Setting CA cert...");
  client.setCACert(server_root_ca);
  otaHeapMark("AFTER setCACert");

  // CRITICAL: Set matching timeouts BEFORE connection attempt
  client.setTimeout(30000);           // 30 seconds in milliseconds
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

  // Continue with your existing download logic...
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
    // Read chunk from network
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

        // Progress indication
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

  // Decode and verify signature (reuse existing verification function)
  if (!base64Decode(signatureBase64, signature, sizeof(signature), &sigLength)) {
    Serial.println("SECURITY: Failed to decode signature");
    if (extractor.otaStarted) esp_ota_abort(extractor.otaHandle);
    goto cleanup;
  }

  mbedtls_pk_init(&pk);
  ret = mbedtls_pk_parse_public_key(&pk, (const unsigned char *)OTA_PUBLIC_KEY, strlen(OTA_PUBLIC_KEY) + 1);
  if (ret != 0) {
    Serial.printf("SECURITY: Failed to parse public key: -0x%04x\n", -ret);
    if (extractor.otaStarted) esp_ota_abort(extractor.otaHandle);
    mbedtls_pk_free(&pk);
    goto cleanup;
  }

  ret = mbedtls_pk_verify(&pk, MBEDTLS_MD_SHA256, hash, 32, signature, sigLength);
  mbedtls_pk_free(&pk);

  if (ret != 0) {
    Serial.printf("SECURITY: Signature verification FAILED (error -0x%04x)\n", -ret);
    if (extractor.otaStarted) esp_ota_abort(extractor.otaHandle);
    goto cleanup;
  }

  Serial.println("SECURITY: Signature verification PASSED");

  // Finalize OTA
  if (extractor.otaStarted) {
    esp_err_t err = esp_ota_end(extractor.otaHandle);
    if (err != ESP_OK) {
      Serial.printf("OTA end failed: %s\n", esp_err_to_name(err));
      goto cleanup;
    }

    err = esp_ota_set_boot_partition(extractor.otaPartition);
    if (err != ESP_OK) {
      Serial.printf("Set boot partition failed: %s\n", esp_err_to_name(err));
      goto cleanup;
    }
  }

  Serial.println("=== STREAMING OTA UPDATE SUCCESSFUL ===");
  Serial.printf("Updated from %s to %s\n", FIRMWARE_VERSION, updateInfo.version.c_str());

  // NEW: Clear forced update flags in Supabase after successful update
  if (hasForcedUpdate) {
    executeClearForcedUpdate();
  }
  Serial.println("Restarting in 3 seconds...");

cleanup:
  // Cleanup
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
  client.setCACert(server_root_ca);
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

  // Warn user before blocking operation
  Serial.println("UPDATE: Starting firmware download, web interface will be unresponsive");
  //events.send("UPDATE: Downloading firmware - interface will freeze 60-90 sec", "console", millis());
  delay(3000);

  // Perform streaming update
  // Check if we need to restart to factory first
  const esp_partition_t *ota0_partition = esp_partition_find_first(ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_APP_OTA_0, NULL);
  const esp_partition_t *factory_partition = esp_partition_find_first(ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_APP_FACTORY, NULL);
  const esp_partition_t *running_partition = esp_ota_get_running_partition();

  if (running_partition == ota0_partition) {
    Serial.println("OTA: Currently on ota_0, switching to factory for update...");
    // events.send("OTA: Switching to factory partition for update", "console", millis());

    // Switch to factory partition and web files
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
  //1) Makes HTTP request to check.php
  //2) Sends Target-Version as a URL parameter
  //3) Calls performOTAUpdate() if version matches
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

  // Skip if not connected to internet
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

  WiFiClientSecure client;  // LOCAL - fresh SSL state
  client.setCACert(server_root_ca);

  HTTPClient http;

  // Request specific version using Target-Version header (per your documentation)
  String url = String(OTA_SERVER_URL) + "/api/firmware/check.php?requestedVersion=" + String(targetVersion);
  Serial.println("🌐 Requesting specific version from: " + url);

  // No 40K heap gate — see notes at firmware-download http.begin above.
  http.begin(client, url);
  http.addHeader("Content-Type", "application/json");
  http.addHeader("User-Agent", "XRegulator/2.0");
  http.addHeader("Device-ID", getDeviceId());
  http.addHeader("Current-Version", FIRMWARE_VERSION);
  http.addHeader("Hardware-Version", "ESP32-WROOM-32");
  http.setTimeout(30000);

  int httpCode = http.GET();
  Serial.printf("📡 HTTP Response Code: %d\n", httpCode);

  if (httpCode == 200) {
    String response = http.getString();
    Serial.println("📨 Server response: " + response);
    UpdateInfo updateInfo = parseUpdateResponse(response);

    if (updateInfo.hasUpdate && updateInfo.version == String(targetVersion)) {
      Serial.printf("=== STARTING TARGETED UPDATE TO %s ===\n", targetVersion);
      events.send("OTA: Beginning download of version " + String(targetVersion), "console", millis());
      performOTAUpdate(updateInfo);  // ← THIS WAS MISSING
    } else if (!updateInfo.hasUpdate) {
      Serial.println("OTA: Server says no update available for version " + String(targetVersion));
      events.send("OTA: Server says no update available for version " + String(targetVersion), "console", millis());
    } else {
      Serial.println("OTA: Version mismatch - requested " + String(targetVersion) + ", got " + updateInfo.version);
      events.send("OTA: Version mismatch - requested " + String(targetVersion) + ", got " + updateInfo.version, "console", millis());
    }
  } else if (httpCode == 404) {
    Serial.println("OTA: Version " + String(targetVersion) + " not found on server");
    events.send("OTA: Version " + String(targetVersion) + " not found on server", "console", millis());
  } else if (httpCode == 429) {
    Serial.println("OTA: Rate limited - try again later");
    events.send("OTA: Rate limited - try again later", "console", millis());
  } else if (httpCode < 0) {
    Serial.printf("OTA: Connection failed - %s\n", http.errorToString(httpCode).c_str());  // ← ADD ERROR STRING
    events.send("OTA: Connection failed - check internet connection", "console", millis());
  } else {
    Serial.printf("OTA: Update request failed with HTTP code: %d (%s)\n",
                  httpCode,
                  http.errorToString(httpCode).c_str());  // ← ADD ERROR STRING
    events.send("OTA: Update request failed - HTTP error " + String(httpCode), "console", millis());
  }

  http.end();
  lastHttpsOperationTime = millis();  // Update timestamp
  Serial.println("HEAP AFTER http.end():");
  Serial.printf("  Internal: %u free, %u largest\n",
                heap_caps_get_free_size(MALLOC_CAP_INTERNAL),
                heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL));
  core0Busy = false;
}
// Validate tar header checksum and magic
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

  // Check for ustar magic at offset 257
  if (memcmp(header + 257, "ustar", 5) != 0) {
    Serial.println("❌ Invalid tar header: missing ustar magic");
    // Print some header bytes for debugging
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
void printPartitionInfo() {
  Serial.println("=== PARTITION SUMMARY ===");

  // Get current running partition
  const esp_partition_t *running = esp_ota_get_running_partition();
  if (running) {
    Serial.printf("🚀 RUNNING APP: %s - %d bytes (%.2f MB) at 0x%X\n",
                  running->label, running->size, running->size / 1024.0 / 1024.0, running->address);
  }

  // List ALL partitions in the table
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

  // Check specific expected partitions
  Serial.println("\n🔍 EXPECTED PARTITION VERIFICATION:");
  checkExpectedPartition("factory", ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_APP_FACTORY, 0x400000);
  checkExpectedPartition("ota_0", ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_APP_OTA_0, 0x400000);
  checkExpectedPartition("factory_fs", ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_LITTLEFS, 0x200000);
  checkExpectedPartition("prod_fs", ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_LITTLEFS, 0x200000);
  checkExpectedPartition("userdata", ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_LITTLEFS, 0x3D0000);

  // OTA partition info
  Serial.println("\n🔄 OTA PARTITION STATUS:");
  const esp_partition_t *ota_partition = esp_ota_get_next_update_partition(NULL);
  if (ota_partition) {
    Serial.printf("Next OTA target: %s at 0x%X\n", ota_partition->label, ota_partition->address);
  }

  // Boot partition info
  const esp_partition_t *boot_partition = esp_ota_get_boot_partition();
  if (boot_partition) {
    Serial.printf("Boot partition: %s at 0x%X\n", boot_partition->label, boot_partition->address);
  }

  // Flash and memory info
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

  // First check if WiFi is even connected
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

  esp_task_wdt_reset();  // Feed watchdog before test
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
  esp_task_wdt_reset();  // Feed watchdog after connectivity test
  // Test 2: Download small file and measure speed
  Serial.println(">>> Test 2: Setting up HTTP client for speed test");
  Serial.flush();
  HTTPClient http;
  // Use a reliable small file (Cloudflare's trace - about 200-300 bytes)
  http.begin(client, "http://cloudflare.com/cdn-cgi/trace");
  http.setTimeout(5000);  // 5 second timeout for speed test
  unsigned long downloadStart = millis();
  int httpCode = http.GET();
  Serial.printf(">>> http.GET() returned code %d after %lu ms\n", httpCode, millis() - downloadStart);
  Serial.flush();
  weatherHttpResponseCode = httpCode;
  esp_task_wdt_reset();  // Feed watchdog after GET request
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

  esp_task_wdt_reset();  // Feed watchdog after completing test
  Serial.println(">>> Watchdog reset #4");
  Serial.flush();

  // Calculate speed in bytes per second
  float bytesPerSecond = 0;
  if (downloadTime > 0) {
    bytesPerSecond = (bytesReceived * 1000.0) / downloadTime;  // bytes/sec
  }

  float kbps = (bytesPerSecond * 8.0) / 1000.0;  // Convert to kilobits per second

  Serial.printf(">>> Speed test result: %.2f Kbps (%d bytes in %lu ms)\n", kbps, bytesReceived, downloadTime);
  Serial.flush();

  // Require minimum 5 Kbps
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

  // 1) Clear blocking flags
  core0Busy = false;
  otaInProgress = false;
  lastHttpsOperationTime = millis();

  // 2) Recreate TempTask if deleted (matches your setup: 4096 stack, priority 1, core 0)
  if (tempTaskHandle == NULL) {
    BaseType_t ok = xTaskCreatePinnedToCore(TempTask, "TempTask", 4096, NULL, 1, &tempTaskHandle, 0);
    if (ok == pdPASS) {
      Serial.println("✅ TempTask recreated on Core 0");
    } else {
      Serial.println("❌ TempTask recreation FAILED");
    }
  }

  // 3) Recreate HTTPS task if deleted (matches your setup: 20480 stack, priority 1, core 0)
  if (httpsTaskHandle == NULL) {
    BaseType_t ok = xTaskCreatePinnedToCore(httpsTask, "HTTPS", 20480, NULL, 1, &httpsTaskHandle, 0);
    if (ok == pdPASS) {
      Serial.println("✅ HTTPS task recreated on Core 0");
    } else {
      Serial.println("❌ HTTPS task recreation FAILED");
    }
  }

  // 4) Let tasks initialize cleanly
  vTaskDelay(pdMS_TO_TICKS(100));

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
// Load time sync state from NVS (call in setup after loadAuthToken)
void loadTimeSyncState() {
  nvs_handle_t nvs_handle;
  esp_err_t err = nvs_open("timesync", NVS_READONLY, &nvs_handle);
  if (err != ESP_OK) {
    Serial.println("No time sync state in NVS (open failed)");
    timeIsSynced = false;
    timeBase = 0;
    return;
  }

  uint64_t tb_u64 = 0;
  uint32_t tbm_u32 = 0;
  int32_t ts_i32 = TIME_NONE;

  esp_err_t e1 = nvs_get_u64(nvs_handle, "timeBase", &tb_u64);
  esp_err_t e2 = nvs_get_u32(nvs_handle, "timeBaseMillis", &tbm_u32);
  esp_err_t e3 = nvs_get_i32(nvs_handle, "timeSource", &ts_i32);

  nvs_close(nvs_handle);

  // Require all three keys; otherwise treat as “no valid state”
  if (e1 != ESP_OK || e2 != ESP_OK || e3 != ESP_OK) {
    Serial.println("No complete time sync state in NVS (missing key)");
    timeIsSynced = false;
    timeBase = 0;
    return;
  }

  // Restore globals
  timeBase = (time_t)tb_u64;
  timeBaseMillis = (unsigned long)tbm_u32;
  currentTimeSource = (TimeSource)ts_i32;

  // Sanity check: if timeBase looks recent and reasonable, trust it
  // Allow reconstruction for ~30 days after reboot (well under 49-day millis wrap)
  time_t now_approx = timeBase + ((millis() - timeBaseMillis) / 1000);

  if (now_approx > 1704067200 && now_approx < 2000000000) {  // Jan 1 2024 to ~2033
    timeIsSynced = true;
    Serial.printf("Time sync restored from NVS: epoch=%ld, millis=%lu\n",
                  (long)timeBase, (unsigned long)timeBaseMillis);
  } else {
    timeIsSynced = false;
    timeBase = 0;
    Serial.println("NVS time sync invalid - will re-sync");
  }
}
// Persist time sync state (call after successful NTP/GPS sync)
void saveTimeSyncState() {
  if (!timeIsSynced || timeBase == 0) return;

  nvs_handle_t nvs_handle;
  esp_err_t err = nvs_open("timesync", NVS_READWRITE, &nvs_handle);
  if (err != ESP_OK) {
    Serial.println("ERROR: Failed to open NVS for time sync");
    return;
  }

  esp_err_t e1 = nvs_set_u64(nvs_handle, "timeBase", (uint64_t)timeBase);
  esp_err_t e2 = nvs_set_u32(nvs_handle, "timeBaseMillis", (uint32_t)timeBaseMillis);
  esp_err_t e3 = nvs_set_i32(nvs_handle, "timeSource", (int32_t)currentTimeSource);

  if (e1 != ESP_OK || e2 != ESP_OK || e3 != ESP_OK) {
    Serial.printf("ERROR: Failed writing time sync state: e1=%d e2=%d e3=%d\n",
                  (int)e1, (int)e2, (int)e3);
    nvs_close(nvs_handle);
    return;
  }

  err = nvs_commit(nvs_handle);
  nvs_close(nvs_handle);

  if (err != ESP_OK) {
    Serial.printf("ERROR: Failed to commit time sync state to NVS (err=%d)\n", (int)err);
  } else {
    Serial.println("Time sync state saved to NVS");
  }
}

time_t getCurrentTimestamp() {
  if (timeIsSynced && timeBase > 0) {
    unsigned long elapsedSeconds = (millis() - timeBaseMillis) / 1000;
    return timeBase + elapsedSeconds;
  }
  return 0;
}
void syncTimeFromGPS(uint16_t daysSince1970, double secondsSinceMidnight) {
  time_t gpsTime = (daysSince1970 * 86400UL) + (time_t)secondsSinceMidnight;

  if (gpsTime > 1577836800) {  // Jan 1, 2020
    timeBase = gpsTime;
    timeBaseMillis = millis();
    timeIsSynced = true;
    currentTimeSource = TIME_GPS;
    lastTimeSyncAttempt = millis();

    saveTimeSyncState();  // Persist immediately

    if (NMEA2KVerbose) {
      Serial.println("Time synced from GPS");
    }
  }
}

void checkTimeSync() {
  unsigned long currentMillis = millis();

  if (currentMillis - lastTimeSyncAttempt < TIME_SYNC_INTERVAL) {
    return;
  }

  // Only use NTP if GPS isn't providing time
  if (currentTimeSource != TIME_GPS) {
    syncTimeFromNTP();
  }
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
    // Compute epoch for window start
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

