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

void writeFile(fs::FS &fs, const char *path, const char *message) {
  if (!littleFSMounted && !ensureLittleFS()) {
    return;
  }

  if (!fsMutex || xSemaphoreTake(fsMutex, pdMS_TO_TICKS(5000)) != pdTRUE) {
    Serial.printf("writeFile: mutex timeout: %s\n", path);
    return;
  }

  File file = fs.open(path, "w");
  if (!file) {
    xSemaphoreGive(fsMutex);
    return;
  }

  file.print(message);
  file.flush();
  file.close();

  xSemaphoreGive(fsMutex);
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

  // Step 1: Unmount and reformat LittleFS (userdata partition - all settings/buffer files)
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
      if (request.type == HTTPS_UPLOAD_PAYLOAD || request.type == HTTPS_UPLOAD_CONFIG) {
        size_t payloadLen = strlen(request.payload);
        // Serial.printf("DEBUG: Received payload, length=%d bytes\n", payloadLen);

        if (payloadLen >= sizeof(request.payload) || payloadLen == 0) {
          // Serial.printf("ERROR: Invalid payload length %d - skipping\n", payloadLen);
          continue;
        }

        if (request.payload[0] != '{') {
          // Serial.printf("ERROR: Payload doesn't start with '{' - corrupted\n");
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
        } else if (lastHttpResponseCode != 400 && lastHttpResponseCode != 401) {
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

  // Alternator temperature
  if (TempSource == 0) {
    TempToUse = AlternatorTemperatureF;
  } else {
    TempToUse = temperatureThermistor;
  }
  int32_t altTemp;
  if (isnan(TempToUse)) {
    altTemp = -9900;  // Invalid marker (-99.00 when divided by 100)
  } else {
    altTemp = (int32_t)(TempToUse * 100.0);
  }
  if (altTemp < currentWindow->altTemp_min) currentWindow->altTemp_min = altTemp;
  if (altTemp > currentWindow->altTemp_max) currentWindow->altTemp_max = altTemp;
  if (shouldAccumulate && !isnan(TempToUse)) {
    currentWindow->altTemp_area_v_us += (int64_t)altTemp * delta_us;
    currentWindow->altTemp_valid_us += delta_us;
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
//
// All-time globals (AvgVoltage_AllTime, EngineRunTime_AllTime, totalOverheats,
// etc.) are read LIVE — they represent cumulative state as of upload time,
// not snapshot capture time. For delayed uploads this means an older buffered
// snapshot will report current cumulative totals; acceptable simplification.
size_t buildSnapshotJson(const SensorSnapshot &snap) {
#define SAFE_AVG(area, valid) ((valid) > 0 ? ((double)(area) / (double)(valid)) / 100.0 : 0.0)
  time_t finalTs = reconstructTimestamp((time_t)snap.collectionTime);
  const char *timestampStr = formatTimestamp(finalTs);
  unsigned long intendedIntervalSec = SENSOR_UPLOAD_INTERVAL / 1000;
  int written = snprintf(
    payloadBuffer, PAYLOAD_BUFFER_SIZE,
    "{"
    "\"device_uid\":\"%s\","
    "\"token\":\"%s\","
    "\"timestamp\":\"%s\","
    "\"firmware_version\":\"%s\","
    "\"batt_volt_min\":%.2f,"
    "\"batt_volt_max\":%.2f,"
    "\"batt_volt_avg\":%.2f,"
    "\"voltage_avg_lifetime\":%.2f,"
    "\"voltage_sample_time\":%lu,"
    "\"batt_curr_min\":%.2f,"
    "\"batt_curr_max\":%.2f,"
    "\"batt_curr_avg\":%.2f,"
    "\"alt_curr_min\":%.2f,"
    "\"alt_curr_max\":%.2f,"
    "\"alt_curr_avg\":%.2f,"
    "\"victron_curr_min\":%.2f,"
    "\"victron_curr_max\":%.2f,"
    "\"victron_curr_avg\":%.2f,"
    "\"soc_min\":%.2f,"
    "\"soc_max\":%.2f,"
    "\"soc_avg\":%.2f,"
    "\"soc_avg_lifetime\":%.2f,"
    "\"soc_sample_time\":%lu,"
    "\"baro_min\":%.2f,"
    "\"baro_max\":%.2f,"
    "\"baro_avg\":%.2f,"
    "\"alt_temp_min\":%.2f,"
    "\"alt_temp_max\":%.2f,"
    "\"alt_temp_avg\":%.2f,"
    "\"temp_therm_min\":%.2f,"
    "\"temp_therm_max\":%.2f,"
    "\"temp_therm_avg\":%.2f,"
    "\"amb_temp_min\":%.2f,"
    "\"amb_temp_max\":%.2f,"
    "\"amb_temp_avg\":%.2f,"
    "\"rpm_min\":%d,"
    "\"rpm_max\":%d,"
    "\"rpm_avg\":%d,"
    "\"wifi_str_min\":%d,"
    "\"wifi_str_max\":%d,"
    "\"wifi_str_avg\":%d,"
    "\"duty_cycle_min\":%.2f,"
    "\"duty_cycle_max\":%.2f,"
    "\"duty_cycle_avg\":%.2f,"
    "\"alt_zero_min\":%.2f,"
    "\"alt_zero_max\":%.2f,"
    "\"alt_zero_avg\":%.2f,"
    "\"sog_min\":%.2f,"
    "\"sog_max\":%.2f,"
    "\"sog_avg\":%.2f,"
    "\"speed_avg_lifetime\":%.2f,"
    "\"speed_sample_time\":%lu,"
    "\"cog_min\":%.2f,"
    "\"cog_max\":%.2f,"
    "\"cog_avg\":%.2f,"
    "\"heading_min\":%.2f,"
    "\"heading_max\":%.2f,"
    "\"heading_avg\":%.2f,"
    "\"aws_min\":%.2f,"
    "\"aws_max\":%.2f,"
    "\"aws_avg\":%.2f,"
    "\"awa_min\":%.2f,"
    "\"awa_max\":%.2f,"
    "\"awa_avg\":%.2f,"
    "\"tws_min\":%.2f,"
    "\"tws_max\":%.2f,"
    "\"tws_avg\":%.2f,"
    "\"twa_min\":%.2f,"
    "\"twa_max\":%.2f,"
    "\"twa_avg\":%.2f,"
    "\"leeway_min\":%.2f,"
    "\"leeway_max\":%.2f,"
    "\"leeway_avg\":%.2f,"
    "\"vmg_min\":%.2f,"
    "\"vmg_max\":%.2f,"
    "\"vmg_avg\":%.2f,"
    "\"lat_avg\":%.6f,"
    "\"lon_avg\":%.6f,"
    "\"eng_hrs\":%.2f,"
    "\"alt_hrs\":%.2f,"
    "\"eng_cycles\":%d,"
    "\"eng_fuel\":%.2f,"
    "\"alt_fuel\":%.2f,"
    "\"charge_cycles\":%u,"
    "\"total_dist\":%.2f,"
    "\"solar_kwh\":%.2f,"
    "\"solar_kwh_alltime\":%.2f,"
    "\"intended_interval_sec\":%lu,"
    "\"u_target_amps_min\":%.2f,"
    "\"u_target_amps_max\":%.2f,"
    "\"u_target_amps_avg\":%.2f,"
    "\"temp_margin_min\":%.2f,"
    "\"temp_margin_max\":%.2f,"
    "\"temp_margin_avg\":%.2f,"
    "\"total_overheats\":%u,"
    "\"total_safe_hours\":%.2f,"
    "\"charged_energy_alltime\":%.2f,"
    "\"discharged_energy_alltime\":%.2f,"
    "\"sailing_days_alltime\":%.2f,"
    "\"sailing_ratio\":%.2f,"
    "\"distance_this_interval\":%.2f,"
    "\"max_wind_speed_true_alltime\":%.2f,"
    "\"max_wind_speed_apparent_alltime\":%.2f,"
    "\"board_temp_max_alltime\":%.2f,"
    "\"board_temp_min_alltime\":%.2f,"
    "\"baro_pressure_max_alltime\":%.2f,"
    "\"baro_pressure_min_alltime\":%.2f"
    "}",
    device_id_hex,
    authToken.c_str(),
    timestampStr,
    FIRMWARE_VERSION,
    snap.window.battVolt_min / 100.0,
    snap.window.battVolt_max / 100.0,
    SAFE_AVG(snap.window.battVolt_area_v_us, snap.window.battVolt_valid_us),
    AvgVoltage_AllTime,
    (unsigned long)totalVoltageSampleTime_AllTime,
    snap.window.battCurr_min / 100.0,
    snap.window.battCurr_max / 100.0,
    SAFE_AVG(snap.window.battCurr_area_v_us, snap.window.battCurr_valid_us),
    snap.window.altCurr_min / 100.0,
    snap.window.altCurr_max / 100.0,
    SAFE_AVG(snap.window.altCurr_area_v_us, snap.window.altCurr_valid_us),
    snap.window.victronCurr_min / 100.0,
    snap.window.victronCurr_max / 100.0,
    SAFE_AVG(snap.window.victronCurr_area_v_us, snap.window.victronCurr_valid_us),
    snap.window.soc_min / 100.0,
    snap.window.soc_max / 100.0,
    SAFE_AVG(snap.window.soc_area_v_us, snap.window.soc_valid_us),
    AvgSOC_AllTime,
    (unsigned long)totalSocSampleTime_AllTime,
    snap.window.baro_min / 100.0,
    snap.window.baro_max / 100.0,
    SAFE_AVG(snap.window.baro_area_v_us, snap.window.baro_valid_us),
    snap.window.altTemp_min / 100.0,
    snap.window.altTemp_max / 100.0,
    SAFE_AVG(snap.window.altTemp_area_v_us, snap.window.altTemp_valid_us),
    snap.window.tempTherm_min / 100.0,
    snap.window.tempTherm_max / 100.0,
    SAFE_AVG(snap.window.tempTherm_area_v_us, snap.window.tempTherm_valid_us),
    snap.window.ambTemp_min / 100.0,
    snap.window.ambTemp_max / 100.0,
    SAFE_AVG(snap.window.ambTemp_area_v_us, snap.window.ambTemp_valid_us),
    snap.window.rpm_min,
    snap.window.rpm_max,
    (int)SAFE_AVG(snap.window.rpm_area_v_us, snap.window.rpm_valid_us),
    snap.window.wifiStr_min,
    snap.window.wifiStr_max,
    (int)SAFE_AVG(snap.window.wifiStr_area_v_us, snap.window.wifiStr_valid_us),
    snap.window.dutyCycle_min / 100.0,
    snap.window.dutyCycle_max / 100.0,
    SAFE_AVG(snap.window.dutyCycle_area_v_us, snap.window.dutyCycle_valid_us),
    snap.window.altZero_min / 100.0,
    snap.window.altZero_max / 100.0,
    SAFE_AVG(snap.window.altZero_area_v_us, snap.window.altZero_valid_us),
    snap.window.sog_min / 100.0,
    snap.window.sog_max / 100.0,
    SAFE_AVG(snap.window.sog_area_v_us, snap.window.sog_valid_us),
    AvgSpeed_AllTime,
    (unsigned long)totalSpeedSampleTime_AllTime,
    snap.window.cog_min / 100.0,
    snap.window.cog_max / 100.0,
    SAFE_AVG(snap.window.cog_area_v_us, snap.window.cog_valid_us),
    snap.window.heading_min / 100.0,
    snap.window.heading_max / 100.0,
    SAFE_AVG(snap.window.heading_area_v_us, snap.window.heading_valid_us),
    snap.window.aws_min / 100.0,
    snap.window.aws_max / 100.0,
    SAFE_AVG(snap.window.aws_area_v_us, snap.window.aws_valid_us),
    snap.window.awa_min / 100.0,
    snap.window.awa_max / 100.0,
    SAFE_AVG(snap.window.awa_area_v_us, snap.window.awa_valid_us),
    snap.window.tws_min / 100.0,
    snap.window.tws_max / 100.0,
    SAFE_AVG(snap.window.tws_area_v_us, snap.window.tws_valid_us),
    snap.window.twa_min / 100.0,
    snap.window.twa_max / 100.0,
    SAFE_AVG(snap.window.twa_area_v_us, snap.window.twa_valid_us),
    snap.window.leeway_min / 100.0,
    snap.window.leeway_max / 100.0,
    SAFE_AVG(snap.window.leeway_area_v_us, snap.window.leeway_valid_us),
    snap.window.vmg_min / 100.0,
    snap.window.vmg_max / 100.0,
    SAFE_AVG(snap.window.vmg_area_v_us, snap.window.vmg_valid_us),
    snap.window.lat_current,
    snap.window.lon_current,
    EngineRunTime_AllTime,
    AlternatorOnTime_AllTime,
    EngineCycles_AllTime,
    EngineFuelUsed_AllTime,
    AlternatorFuelUsed_AllTime,
    ChargeCycles_AllTime,
    TotalDistance,
    SolarChargedEnergy / 1000.0,
    SolarChargedEnergy_AllTime / 1000.0,
    intendedIntervalSec,
    snap.window.uTargetAmps_min / 100.0,
    snap.window.uTargetAmps_max / 100.0,
    SAFE_AVG(snap.window.uTargetAmps_area_v_us, snap.window.uTargetAmps_valid_us),
    snap.window.tempMargin_min / 100.0,
    snap.window.tempMargin_max / 100.0,
    SAFE_AVG(snap.window.tempMargin_area_v_us, snap.window.tempMargin_valid_us),
    (unsigned int)totalOverheats,
    (float)totalSafeHours,
    ChargedEnergy_AllTime / 1000.0,
    DischargedEnergy_AllTime / 1000.0,
    sailing_days_alltime,
    sailing_ratio,
    distance_this_interval,
    max_wind_speed_true_alltime,
    max_wind_speed_apparent_alltime,
    board_temp_max_alltime,
    board_temp_min_alltime,
    baro_pressure_max_alltime,
    baro_pressure_min_alltime);
#undef SAFE_AVG
  if (written <= 0 || written >= PAYLOAD_BUFFER_SIZE) return 0;
  return (size_t)written;
}

void uploadBufferedRecords() {
  if (otaInProgress) return;
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
  strncpy(req.payload, payloadBuffer, sizeof(req.payload) - 1);
  req.payload[sizeof(req.payload) - 1] = '\0';

  if (xQueueSend(httpsQueue, &req, 0) == pdTRUE) {
    sensorRingInFlightIndex = (int32_t)sensorRingTail;
    lastUploadWasBuffered = true;
  } else {
    Serial.println("uploadBufferedRecords: httpsQueue full, will retry next tick");
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
  if (ringIsFull()) {
    if (sensorRingInFlightIndex == (int32_t)sensorRingTail) {
      // Oldest slot is being uploaded; drop the NEW snapshot instead.
      Serial.println("sensorRing full + tail in-flight; dropping newest snapshot");
      return;
    }
    // Drop oldest to make room.
    sensorRingTail = (sensorRingTail + 1) % SENSOR_RING_SIZE;
    sensorRingCount--;
  }
  sensorRing[sensorRingHead].collectionTime = (int32_t)collectionTime;
  sensorRing[sensorRingHead].window = *currentWindow;  // struct copy from PSRAM to PSRAM
  sensorRingHead = (sensorRingHead + 1) % SENSOR_RING_SIZE;
  sensorRingCount++;
  bufferedRecordCount = sensorRingCount;  // dashboard mirror
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
  if (!sensorRing || ringIsEmpty()) return;
  sensorRingTail = (sensorRingTail + 1) % SENSOR_RING_SIZE;
  sensorRingCount--;
  bufferedRecordCount = sensorRingCount;  // dashboard mirror
}

// Binary-format file used by Phase 3 (shutdown dump + boot restore) so the
// PSRAM ring survives a power-cycle when WiFi/cloud couldn't drain everything
// during the 30-min ignition-off window.
#define SENSOR_RING_BACKUP_PATH  "/sensor_ring_backup.bin"
#define SENSOR_RING_BACKUP_MAGIC 0x53524258u  // 'SRBX'
#define SENSOR_RING_BACKUP_VER   1u

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
void uploadSensorHistory() {
  if (otaInProgress) return;

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
// Dashboard "Clear" button handler. Empties the PSRAM ring and removes the
// LittleFS shutdown-dump file so nothing comes back on next boot.
void clearSensorBuffer() {
  sensorRingHead = 0;
  sensorRingTail = 0;
  sensorRingCount = 0;
  sensorRingInFlightIndex = -1;
  sensorRingAnnouncedEmpty = false;
  bufferedRecordCount = 0;

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
// Build config snapshot JSON payload
bool buildConfigPayload() {
  int offset = snprintf(configPayloadBuffer, CONFIG_PAYLOAD_SIZE,
                        "{\"device_uid\":\"%s\",\"token\":\"%s\",\"unix_time\":%lu",
                        device_id_hex, authToken.c_str(), (unsigned long)time(NULL));

  if (offset < 0 || offset >= CONFIG_PAYLOAD_SIZE) {
    //Serial.println("ERROR: Config payload buffer too small");
    return false;
  }

  // System Health
  offset += snprintf(configPayloadBuffer + offset, CONFIG_PAYLOAD_SIZE - offset,
                     ",\"current_partition_type\":%d,\"firmware_version_int\":%d"
                     ",\"raw_free_heap\":%d,\"min_free_heap\":%d,\"free_internal_ram\":%d"
                     ",\"heapfrag\":%d,\"cpu_load_core0\":%d,\"cpu_load_core1\":%d"
                     ",\"cpu_load_core0_max\":%d,\"cpu_load_core1_max\":%d"
                     ",\"loop_time\":%d,\"max_loop_time\":%d,\"wifi_strength\":%d"
                     ",\"wifi_reconnects_total\":%d,\"wifi_disconnect_count\":%d",
                     currentPartitionType, firmwareVersionInt, rawFreeHeap, MinFreeHeap,
                     FreeInternalRam, Heapfrag, cpuLoadCore0, cpuLoadCore1,
                     cpuLoadCore0Max, cpuLoadCore1Max, LoopTime,
                     MaxLoopTime, WifiStrength, wifiReconnectsTotal, wifiDisconnectCount);

  // Session Stats
  offset += snprintf(configPayloadBuffer + offset, CONFIG_PAYLOAD_SIZE - offset,
                     ",\"last_session_duration\":%lu,\"last_session_max_loop_time\":%d"
                     ",\"last_session_min_heap\":%d,\"last_reset_reason\":%d"
                     ",\"ancient_reset_reason\":%d,\"total_power_cycles\":%d",
                     LastSessionDuration, LastSessionMaxLoopTime, lastSessionMinHeap,
                     LastResetReason, ancientResetReason, totalPowerCycles);

  // Weather/Solar
  offset += snprintf(configPayloadBuffer + offset, CONFIG_PAYLOAD_SIZE - offset,
                     ",\"solar_watts\":%d,\"performance_ratio\":%.2f"
                     ",\"weather_mode_enabled\":%d,\"current_weather_mode\":%d",
                     SolarWatts, performanceRatio, weatherModeEnabled, currentWeatherMode);

  // Charge Settings
  offset += snprintf(configPayloadBuffer + offset, CONFIG_PAYLOAD_SIZE - offset,
                     ",\"float_voltage\":%.2f,\"bulk_voltage\":%.2f"
                     ",\"bulk_complete_time\":%lu,\"float_duration\":%lu"
                     ",\"temperature_limit_f\":%.1f,\"force_float\":%d",
                     FloatVoltage, BulkVoltage, absorptionCompleteTime, FLOAT_DURATION,
                     TemperatureLimitF, MaintainMode);

  // Control Switches
  offset += snprintf(configPayloadBuffer + offset, CONFIG_PAYLOAD_SIZE - offset,
                     ",\"on_off\":%d,\"ignition\":%d,\"ignition_override\":%d"
                     ",\"hi_low\":%d,\"amp_sensor_range\":%d",
                     OnOff, Ignition, IgnitionOverride, HiLow, AmpSensorRange);

  // Field Control
  offset += snprintf(configPayloadBuffer + offset, CONFIG_PAYLOAD_SIZE - offset,
                     ",\"duty_step\":%d,\"switching_frequency\":%.1f"
                     ",\"max_duty\":%.2f,\"min_duty\":%.2f"
                     ",\"manual_duty_target\":%d,\"freq\":%u",
                     yyMin, SwitchingFrequency, MaxDuty, MinDuty,
                     ManualDutyTarget, Freq);

  // Hardware Specs
  offset += snprintf(configPayloadBuffer + offset, CONFIG_PAYLOAD_SIZE - offset,
                     ",\"field_resistance\":%.3f,\"r_fixed\":%.1f"
                     ",\"beta\":%.0f,\"t0_c\":%.1f"
                     ",\"temp_source\":%d,\"current_time_source\":%d",
                     FieldResistance, R_fixed, Beta, T0_C,
                     TempSource, (int)currentTimeSource);

  // Sensor Config
  offset += snprintf(configPayloadBuffer + offset, CONFIG_PAYLOAD_SIZE - offset,
                     ",\"sensor_upload_interval\":%lu,\"buffered_record_count\":%d"
                     ",\"battery_capacity_ah\":%d,\"peukert_rated_current_a\":%.1f"
                     ",\"soc_update_interval\":%d,\"fuel_efficiency_scaled\":%d"
                     ",\"battery_voltage_source\":%d,\"battery_current_source\":%d",
                     SENSOR_UPLOAD_INTERVAL, bufferedRecordCount, BatteryCapacity_Ah,
                     PeukertRatedCurrent_A, SOCUpdateInterval, FuelEfficiency_scaled,
                     BatteryVoltageSource, BatteryCurrentSource);

  // Learning System Settings
  offset += snprintf(configPayloadBuffer + offset, CONFIG_PAYLOAD_SIZE - offset,
                     ",\"learning_mode\":%d,\"learning_paused\":%d"
                     ",\"learning_upward_enabled\":%d,\"learning_downward_enabled\":%d"
                     ",\"alternator_nominal_amps\":%d,\"learning_up_step\":%.2f"
                     ",\"learning_down_step\":%.2f,\"ambient_temp_correction_factor\":%.3f"
                     ",\"ambient_temp_baseline\":%.1f,\"min_learning_interval\":%lu"
                     ",\"safe_operation_threshold\":%lu,\"last_significant_rpm_change\":%lu"
                     ",\"last_stable_rpm\":%d,\"learning_settling_period\":%d"
                     ",\"learning_rpm_change_threshold\":%d,\"learning_temp_hysteresis\":%d",
                     LearningMode, LearningPaused, LearningUpwardEnabled, LearningDownwardEnabled,
                     AlternatorNominalAmps, LearningUpStep, LearningDownStep,
                     AmbientTempCorrectionFactor, xTime, MinLearningInterval,
                     SafeOperationThreshold, lastSignificantRPMChange, lastStableRPM,
                     LearningSettlingPeriod, LearningRPMChangeThreshold, LearningTempHysteresis);

  // PID Tuning
  offset += snprintf(configPayloadBuffer + offset, CONFIG_PAYLOAD_SIZE - offset,
                     ",\"pid_kp\":%.3f,\"pid_ki\":%.3f,\"pid_kd\":%.3f"
                     ",\"pid_sample_divisor\":%lu,\"max_penalty_percent\":%.1f"
                     ",\"max_penalty_duration\":%lu",
                     PidKp, PidKi, PidKd, PidSampleDivisor,
                     MaxPenaltyPercent, MaxPenaltyDuration);

  // Learning Diagnostics
  offset += snprintf(configPayloadBuffer + offset, CONFIG_PAYLOAD_SIZE - offset,
                     ",\"neighbor_learning_factor\":%.2f,\"learning_rpm_spacing\":%d"
                     ",\"learning_memory_duration\":%lu,\"ignore_learning_during_penalty\":%d"
                     ",\"enable_neighbor_learning\":%d,\"enable_ambient_correction\":%d"
                     ",\"learning_failsafe_mode\":%d,\"learning_dry_run_mode\":%d"
                     // auto_save_learning_table and learning_table_save_interval — OBSOLETE REMOVE LATER
                     ",\"clear_overheat_history\":%d,\"overheating_penalty_timer\":%lu"
                     ",\"overheating_penalty_amps\":%.1f,\"total_learning_events\":%lu"
                     ",\"total_overheats\":%lu,\"total_safe_hours\":%lu"
                     ",\"average_table_value\":%.2f",
                     NeighborLearningFactor, yyMax, LearningMemoryDuration,
                     IgnoreLearningDuringPenalty, EnableNeighborLearning, EnableAmbientCorrection,
                     TuningMode, LearningDryRunMode, ClearOverheatHistory, overheatingPenaltyTimer,
                     overheatingPenaltyAmps, totalLearningEvents, totalOverheats,
                     totalSafeHours, averageTableValue);

  // SOC Algorithm
  offset += snprintf(configPayloadBuffer + offset, CONFIG_PAYLOAD_SIZE - offset,
                     ",\"auto_shunt_gain_correction\":%d,\"dynamic_shunt_gain_factor\":%.4f"
                     ",\"auto_alt_current_zero\":%d,\"dynamic_alt_current_zero\":%.2f"
                     ",\"current_threshold\":%d,\"peukert_exponent_scaled\":%d"
                     ",\"charge_efficiency_scaled\":%d,\"charged_voltage_scaled\":%d"
                     ",\"tail_current\":%d,\"shunt_resistance_micro_ohm\":%d"
                     ",\"charged_detection_time\":%d,\"ignore_temperature\":%d",
                     AutoShuntGainCorrection, DynamicShuntGainFactor, AutoAltCurrentZero,
                     DynamicAltCurrentZero, CurrentThreshold, PeukertExponent_scaled,
                     ChargeEfficiency_scaled, ChargedVoltage_Scaled, (int)(TailCurrent * 10),
                     ShuntResistanceMicroOhm, ChargedDetectionTime, IgnoreTemperature);

  // BMS Integration
  offset += snprintf(configPayloadBuffer + offset, CONFIG_PAYLOAD_SIZE - offset,
                     ",\"bms_logic\":%d,\"bms_logic_level_off\":%d",
                     bmsLogic, bmsLogicLevelOff);

  // Alarm Settings
  offset += snprintf(configPayloadBuffer + offset, CONFIG_PAYLOAD_SIZE - offset,
                     ",\"alarm_activate\":%d,\"temp_alarm\":%d"
                     ",\"voltage_alarm_high\":%d,\"voltage_alarm_low\":%d"
                     ",\"current_alarm_high\":%d,\"maximum_allowed_battery_amps\":%d",
                     AlarmActivate, TempAlarm, VoltageAlarmHigh, VoltageAlarmLow,
                     CurrentAlarmHigh, MaximumAllowedBatteryAmps);

  // Calibration
  offset += snprintf(configPayloadBuffer + offset, CONFIG_PAYLOAD_SIZE - offset,
                     ",\"four_way\":%d,\"rpm_scaling_factor\":%d"
                     ",\"alternator_c_offset\":%.2f,\"battery_c_offset\":%.2f"
                     ",\"time_to_full_charge_min\":%d,\"time_to_full_discharge_min\":%d",
                     FourWay, RPMScalingFactor, AlternatorCOffset, BatteryCOffset,
                     timeToFullChargeMin, timeToFullDischargeMin);

  // Engine Tracking
  offset += snprintf(configPayloadBuffer + offset, CONFIG_PAYLOAD_SIZE - offset,
                     ",\"engine_run_accumulator\":%lu,\"alternator_on_accumulator\":%lu",
                     engineRunAccumulator, alternatorOnAccumulator);

  // Temperature Sensor
  offset += snprintf(configPayloadBuffer + offset, CONFIG_PAYLOAD_SIZE - offset,
                     ",\"winding_temp_offset\":%.2f,\"pulley_ratio\":%.2f",
                     WindingTempOffset, PulleyRatio);

  // Thermal Stress
  offset += snprintf(configPayloadBuffer + offset, CONFIG_PAYLOAD_SIZE - offset,
                     ",\"cumulative_insulation_damage\":%.2f,\"cumulative_grease_damage\":%.2f"
                     ",\"cumulative_brush_damage\":%.2f,\"insulation_life_percent\":%.1f"
                     ",\"grease_life_percent\":%.1f,\"brush_life_percent\":%.1f"
                     ",\"predicted_life_hours\":%.1f,\"life_indicator_color\":%d",
                     CumulativeInsulationDamage, CumulativeGreaseDamage, CumulativeBrushDamage,
                     InsulationLifePercent, GreaseLifePercent, BrushLifePercent,
                     PredictedLifeHours, LifeIndicatorColor);

  // Timing Config
  offset += snprintf(configPayloadBuffer + offset, CONFIG_PAYLOAD_SIZE - offset,
                     ",\"maximum_loop_time\":%d,\"rpm_threshold\":%d"
                     ",\"ve_time\":%d,\"send_wifi_time\":%d"
                     ",\"analog_read_time\":%d,\"analog_read_time2\":%d"
                     ",\"web_gauges_interval\":%lu,\"plot_time_window\":%d"
                     ",\"healthystuff_interval\":%lu",
                     MaximumLoopTime, RPMThreshold, VeTime, SendWifiTime,
                     AnalogReadTime, AnalogReadTime2, webgaugesinterval, plotTimeWindow,
                     healthystuffinterval);

  // Authentication (booleans as 1/0)
  offset += snprintf(configPayloadBuffer + offset, CONFIG_PAYLOAD_SIZE - offset,
                     ",\"is_registered\":%d,\"learning_table_updated\":%d"
                     ",\"charging_enabled\":%d,\"bms_signal_active\":%d",
                     isRegistered ? 1 : 0, learningTableUpdated ? 1 : 0,
                     chargingEnabled ? 1 : 0, bmsSignalActive ? 1 : 0);

  // RPM Current Table (10 values)
  offset += snprintf(configPayloadBuffer + offset, CONFIG_PAYLOAD_SIZE - offset,
                     ",\"rpm_current_table_0\":%.2f,\"rpm_current_table_1\":%.2f"
                     ",\"rpm_current_table_2\":%.2f,\"rpm_current_table_3\":%.2f"
                     ",\"rpm_current_table_4\":%.2f,\"rpm_current_table_5\":%.2f"
                     ",\"rpm_current_table_6\":%.2f,\"rpm_current_table_7\":%.2f"
                     ",\"rpm_current_table_8\":%.2f,\"rpm_current_table_9\":%.2f",
                     rpmCurrentTable[0], rpmCurrentTable[1], rpmCurrentTable[2],
                     rpmCurrentTable[3], rpmCurrentTable[4], rpmCurrentTable[5],
                     rpmCurrentTable[6], rpmCurrentTable[7], rpmCurrentTable[8],
                     rpmCurrentTable[9]);

  // RPM Table Points (10 values)
  offset += snprintf(configPayloadBuffer + offset, CONFIG_PAYLOAD_SIZE - offset,
                     ",\"rpm_table_rpm_points_0\":%d,\"rpm_table_rpm_points_1\":%d"
                     ",\"rpm_table_rpm_points_2\":%d,\"rpm_table_rpm_points_3\":%d"
                     ",\"rpm_table_rpm_points_4\":%d,\"rpm_table_rpm_points_5\":%d"
                     ",\"rpm_table_rpm_points_6\":%d,\"rpm_table_rpm_points_7\":%d"
                     ",\"rpm_table_rpm_points_8\":%d,\"rpm_table_rpm_points_9\":%d",
                     rpmTableRPMPoints[0], rpmTableRPMPoints[1], rpmTableRPMPoints[2],
                     rpmTableRPMPoints[3], rpmTableRPMPoints[4], rpmTableRPMPoints[5],
                     rpmTableRPMPoints[6], rpmTableRPMPoints[7], rpmTableRPMPoints[8],
                     rpmTableRPMPoints[9]);

  // Fuel Table RPM (10 values)
  offset += snprintf(configPayloadBuffer + offset, CONFIG_PAYLOAD_SIZE - offset,
                     ",\"fuel_table_rpm_0\":%.1f,\"fuel_table_rpm_1\":%.1f"
                     ",\"fuel_table_rpm_2\":%.1f,\"fuel_table_rpm_3\":%.1f"
                     ",\"fuel_table_rpm_4\":%.1f,\"fuel_table_rpm_5\":%.1f"
                     ",\"fuel_table_rpm_6\":%.1f,\"fuel_table_rpm_7\":%.1f"
                     ",\"fuel_table_rpm_8\":%.1f,\"fuel_table_rpm_9\":%.1f",
                     fuelTableRPM[0], fuelTableRPM[1], fuelTableRPM[2],
                     fuelTableRPM[3], fuelTableRPM[4], fuelTableRPM[5],
                     fuelTableRPM[6], fuelTableRPM[7], fuelTableRPM[8],
                     fuelTableRPM[9]);

  // Fuel Table GPH (10 values)
  offset += snprintf(configPayloadBuffer + offset, CONFIG_PAYLOAD_SIZE - offset,
                     ",\"fuel_table_gph_0\":%.2f,\"fuel_table_gph_1\":%.2f"
                     ",\"fuel_table_gph_2\":%.2f,\"fuel_table_gph_3\":%.2f"
                     ",\"fuel_table_gph_4\":%.2f,\"fuel_table_gph_5\":%.2f"
                     ",\"fuel_table_gph_6\":%.2f,\"fuel_table_gph_7\":%.2f"
                     ",\"fuel_table_gph_8\":%.2f,\"fuel_table_gph_9\":%.2f",
                     fuelTableGPH[0], fuelTableGPH[1], fuelTableGPH[2],
                     fuelTableGPH[3], fuelTableGPH[4], fuelTableGPH[5],
                     fuelTableGPH[6], fuelTableGPH[7], fuelTableGPH[8],
                     fuelTableGPH[9]);

  // Overheat Count (10 values)
  offset += snprintf(configPayloadBuffer + offset, CONFIG_PAYLOAD_SIZE - offset,
                     ",\"overheat_count_0\":%d,\"overheat_count_1\":%d"
                     ",\"overheat_count_2\":%d,\"overheat_count_3\":%d"
                     ",\"overheat_count_4\":%d,\"overheat_count_5\":%d"
                     ",\"overheat_count_6\":%d,\"overheat_count_7\":%d"
                     ",\"overheat_count_8\":%d,\"overheat_count_9\":%d",
                     overheatCount[0], overheatCount[1], overheatCount[2],
                     overheatCount[3], overheatCount[4], overheatCount[5],
                     overheatCount[6], overheatCount[7], overheatCount[8],
                     overheatCount[9]);

  // Last Overheat Time (10 values)
  offset += snprintf(configPayloadBuffer + offset, CONFIG_PAYLOAD_SIZE - offset,
                     ",\"last_overheat_time_0\":%lu,\"last_overheat_time_1\":%lu"
                     ",\"last_overheat_time_2\":%lu,\"last_overheat_time_3\":%lu"
                     ",\"last_overheat_time_4\":%lu,\"last_overheat_time_5\":%lu"
                     ",\"last_overheat_time_6\":%lu,\"last_overheat_time_7\":%lu"
                     ",\"last_overheat_time_8\":%lu,\"last_overheat_time_9\":%lu",
                     lastOverheatTime[0], lastOverheatTime[1], lastOverheatTime[2],
                     lastOverheatTime[3], lastOverheatTime[4], lastOverheatTime[5],
                     lastOverheatTime[6], lastOverheatTime[7], lastOverheatTime[8],
                     lastOverheatTime[9]);

  // Cumulative No Overheat Time (10 values)
  offset += snprintf(configPayloadBuffer + offset, CONFIG_PAYLOAD_SIZE - offset,
                     ",\"cumulative_no_overheat_time_0\":%lu,\"cumulative_no_overheat_time_1\":%lu"
                     ",\"cumulative_no_overheat_time_2\":%lu,\"cumulative_no_overheat_time_3\":%lu"
                     ",\"cumulative_no_overheat_time_4\":%lu,\"cumulative_no_overheat_time_5\":%lu"
                     ",\"cumulative_no_overheat_time_6\":%lu,\"cumulative_no_overheat_time_7\":%lu"
                     ",\"cumulative_no_overheat_time_8\":%lu,\"cumulative_no_overheat_time_9\":%lu",
                     cumulativeNoOverheatTime[0], cumulativeNoOverheatTime[1], cumulativeNoOverheatTime[2],
                     cumulativeNoOverheatTime[3], cumulativeNoOverheatTime[4], cumulativeNoOverheatTime[5],
                     cumulativeNoOverheatTime[6], cumulativeNoOverheatTime[7], cumulativeNoOverheatTime[8],
                     cumulativeNoOverheatTime[9]);

  // Close JSON
  offset += snprintf(configPayloadBuffer + offset, CONFIG_PAYLOAD_SIZE - offset, "}");

  if (offset >= CONFIG_PAYLOAD_SIZE - 1) {
    //Serial.println("ERROR: Config payload truncated");
    return false;
  }
  //Serial.printf("Config payload built: %d bytes\n", offset);
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

void syncTimeFromNTP() {
  if (otaInProgress) {
    return;  // Skip during OTA
  }
  if (currentMode != MODE_CLIENT || WiFi.status() != WL_CONNECTED) return;  // MODE_CLIENT = 1

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

    saveTimeSyncState();  // Persist immediately

    queueConsoleMessage("Time synced from NTP");
    Serial.printf("NTP synced: epoch=%ld\n", timeBase);
    core0Busy = false;
    return;
  }

  Serial.println("NTP sync attempt failed");
  lastTimeSyncAttempt = millis();
  core0Busy = false;
}

bool canUploadNow() {
  if (WiFi.status() != WL_CONNECTED) return false;
  if (WiFi.RSSI() < -76) return false;
  return true;
}