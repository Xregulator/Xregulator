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
// - Uses a single cleanup path to guarantee core0Busy is always released.
// - tempTaskHealthy is owned by checkTempTaskHealth(); TempTask only sets it true on a clean read.
// - File writes happen after core0Busy is released; fsMutex serializes filesystem access independently.
void TempTask(void *parameter) {
  // NO watchdog registration - keep it separate from main loop watchdog

  static uint8_t scratchPad[9];
  static unsigned long lastTempRead = 0;
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
        Serial.printf("TempTask: Sensor enumerated at %d-bit resolution\n", resolution);
      } else {
        tempEnumerateFailCount++;
        Serial.printf("TempTask FAIL: sensor not found on bus  enumFail=%lu\n", (unsigned long)tempEnumerateFailCount);
        vTaskDelay(pdMS_TO_TICKS(5000));
        continue;
      }
    }

    if (core0Busy) {
      tempCoreBusySkipCount++;
      vTaskDelay(pdMS_TO_TICKS(1000));
      continue;
    }

    if (now - lastTempRead < 5000) {
      tempStaleSkipCount++;
      vTaskDelay(pdMS_TO_TICKS(1000));
      continue;
    }

    bool writeMaxTemp = false;
    bool writeMaxTempAllTime = false;
    float pendingMaxTemp = 0.0f;
    float pendingMaxTempAllTime = 0.0f;

    core0Busy = true;

    if (!sensors.isConnected(tempDeviceAddress)) {
      tempConnectedFailCount++;
      Serial.printf("TempTask FAIL: isConnected false - will re-enumerate  connFail=%lu\n", (unsigned long)tempConnectedFailCount);
      lastValidTemp = -99;
      sensorEnumerated = false;
      goto cleanup;
    }

    if (!sensors.requestTemperaturesByAddress(tempDeviceAddress)) {
      tempRequestFailCount++;
      Serial.printf("TempTask FAIL: requestTemperatures NACK  reqFail=%lu\n", (unsigned long)tempRequestFailCount);
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
          Serial.printf("TempTask FAIL: conversion timeout after %lums  reqFail=%lu\n", millis() - convStart, (unsigned long)tempRequestFailCount);
          goto cleanup;
        }
      }
    }

    if (sensors.readScratchPad(tempDeviceAddress, scratchPad)) {

      // CHECK 1: CRC validation
      uint8_t crc = OneWire::crc8(scratchPad, 8);
      if (crc != scratchPad[8]) {
        tempCrcFailCount++;
        Serial.printf("TempTask FAIL: CRC bad cfg=0x%02X crcFail=%lu  scratchpad=", (unsigned int)scratchPad[4], (unsigned long)tempCrcFailCount);
        for (int i = 0; i < 9; i++) Serial.printf("%02X ", scratchPad[i]);
        Serial.printf(" calc=%02X exp=%02X\n", crc, scratchPad[8]);

        // Immediate single retry
        vTaskDelay(pdMS_TO_TICKS(2));
        if (sensors.readScratchPad(tempDeviceAddress, scratchPad)) {
          uint8_t crc2 = OneWire::crc8(scratchPad, 8);
          if (crc2 != scratchPad[8]) {
            Serial.printf("TempTask FAIL: CRC bad on retry crcFail=%lu  scratchpad=", (unsigned long)tempCrcFailCount);
            for (int i = 0; i < 9; i++) Serial.printf("%02X ", scratchPad[i]);
            Serial.printf(" calc=%02X exp=%02X\n", crc2, scratchPad[8]);
            goto cleanup;
          } else {
            tempCrcRecoveredCount++;
            Serial.printf("TempTask: CRC recovered on retry  crcRec=%lu\n", (unsigned long)tempCrcRecoveredCount);
          }
        } else {
          tempReadFailCount++;
          Serial.printf("TempTask FAIL: readScratchPad failed on retry  readFail=%lu\n", (unsigned long)tempReadFailCount);
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
        Serial.printf("TempTask FAIL: all-0xFF - will re-enumerate  allFF=%lu\n", (unsigned long)tempAllFFCount);
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
        Serial.printf("TempTask FAIL: power-on 85C value detected  85C=%lu\n", (unsigned long)tempPowerOn85Count);
        goto cleanup;
      }

      // CHECK 4: Verify resolution; auto-correct if EEPROM or reset changed it
      if (scratchPad[4] != DS18B20_CFG_BYTE) {
        tempResolutionFixCount++;
        Serial.printf("TempTask: Resolution mismatch (cfg=0x%02X expected=0x%02X) forcing %d-bit  resFix=%lu\n", (unsigned int)scratchPad[4], DS18B20_CFG_BYTE, resolution, (unsigned long)tempResolutionFixCount);
        sensors.setResolution(tempDeviceAddress, resolution);
        if (!sensors.requestTemperaturesByAddress(tempDeviceAddress)) {
          tempRequestFailCount++;
          Serial.printf("TempTask FAIL: re-request after resolution fix failed  reqFail=%lu\n", (unsigned long)tempRequestFailCount);
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
              Serial.printf("TempTask FAIL: conversion timeout after res fix %lums  reqFail=%lu\n", millis() - convStart2, (unsigned long)tempRequestFailCount);
              goto cleanup;
            }
          }
        }

        if (!sensors.readScratchPad(tempDeviceAddress, scratchPad)) {
          tempRereadFailCount++;
          Serial.printf("TempTask FAIL: re-read after resolution fix failed  rereadFail=%lu\n", (unsigned long)tempRereadFailCount);
          goto cleanup;
        }
        if (OneWire::crc8(scratchPad, 8) != scratchPad[8]) {
          tempResolutionFixCrcFailCount++;
          Serial.printf("TempTask FAIL: CRC fail after resolution fix  resFixCrcFail=%lu\n", (unsigned long)tempResolutionFixCrcFailCount);
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
        MARK_FRESH(IDX_ALTERNATOR_TEMP);

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
        Serial.printf("TempTask FAIL: out of range %.1fF  oor=%lu\n", tempF, (unsigned long)tempOutOfRangeCount);
      }
    } else {
      tempReadFailCount++;
      Serial.printf("TempTask FAIL: readScratchPad failed  readFail=%lu\n", (unsigned long)tempReadFailCount);
    }

cleanup:
    core0Busy = false;
    lastTempRead = millis();
    lastTempTaskHeartbeat = millis();

    if (writeMaxTemp) {
      MaxAlternatorTemperatureF = pendingMaxTemp;
      char tbuf[32];
      snprintf(tbuf, sizeof(tbuf), "%.2f", pendingMaxTemp);
      writeFile(LittleFS, "/MaxAlternatorTemperatureF.txt", tbuf);
    }
    if (writeMaxTempAllTime) {
      MaxAlternatorTemperatureF_AllTime = pendingMaxTempAllTime;
      char tbuf2[32];
      snprintf(tbuf2, sizeof(tbuf2), "%.2f", pendingMaxTempAllTime);
      writeFile(LittleFS, "/MaxAlternatorTemperatureF_AllTime.txt", tbuf2);
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
void initSensorBuffer() {
  if (!fsExists(SENSOR_BUFFER_DIR)) {
    fsMkdir(SENSOR_BUFFER_DIR);
  }

  // Count existing buffered files
  fsTakeLock();
  File root = LittleFS.open(SENSOR_BUFFER_DIR);
  if (root && root.isDirectory()) {
    bufferedRecordCount = 0;
    File file = root.openNextFile();
    while (file) {
      esp_task_wdt_reset();
      if (!file.isDirectory()) {
        bufferedRecordCount++;
      }
      file = root.openNextFile();
    }
  }
  fsReleaseLock();

  if (bufferedRecordCount > 0) {
    snprintf(messageBuffer, MESSAGE_BUFFER_SIZE, "Found %d buffered", bufferedRecordCount);
    queueConsoleMessage(messageBuffer);
  }

  resetSensorWindow();
  resetAccelWindow();
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

// FILE BUFFERING
void saveToLocalBuffer(time_t collectionTime) {
  if (otaInProgress) {
    return;  // Skip during OTA
  }
  // Don't buffer if not registered or token is empty
if (!isRegistered || authToken.isEmpty()) {
    static unsigned long lastNotRegisteredMsgMs = 0;
    if (millis() - lastNotRegisteredMsgMs >= 30000) {
      lastNotRegisteredMsgMs = millis();
      Serial.println("Skipping buffer save - not registered or no token");
      queueConsoleMessage("Skipped buffer save: not registered");
    }
    return;
  }
  esp_task_wdt_reset();
  fsTakeLock();  //- must be before any LittleFS call or fsReleaseLock()
  // Enforce max buffered records (delete oldest to make room for newest)
  if (bufferedRecordCount >= MAX_BUFFERED_RECORDS) {
    File root = LittleFS.open(SENSOR_BUFFER_DIR);
    if (root && root.isDirectory()) {
      char oldestFile[FILENAME_BUFFER_SIZE] = "";
      unsigned long oldestTimestamp = ULONG_MAX;
      File file = root.openNextFile();
      while (file) {
        esp_task_wdt_reset();
        if (!file.isDirectory()) {
          const char *name = file.name();  // e.g. "1739051234.json" or with path
          if (name) {
            // Find last '.' in name
            const char *dot = strrchr(name, '.');
            if (dot && dot > name) {
              // Parse leading unsigned long from start of name up to dot
              unsigned long ts = 0;
              const char *p = name;

              // If name includes directories, advance to basename
              const char *slash = strrchr(name, '/');
              if (slash) p = slash + 1;

              while (p < dot && (*p >= '0' && *p <= '9')) {
                ts = (ts * 10UL) + (unsigned long)(*p - '0');
                p++;
              }

              // Accept only if we parsed at least one digit and consumed all chars up to dot
              if (p > (slash ? slash + 1 : name) && p == dot) {
                if (ts < oldestTimestamp) {
                  oldestTimestamp = ts;
                  strncpy(oldestFile, file.path(), FILENAME_BUFFER_SIZE - 1);
                  oldestFile[FILENAME_BUFFER_SIZE - 1] = '\0';
                }
              }
            }
          }
        }
        file = root.openNextFile();
      }
      root.close();

      if (oldestFile[0] != '\0') {
        LittleFS.remove(oldestFile);
        if (bufferedRecordCount > 0) {
          bufferedRecordCount--;
        }
      }
    }
  }

// Add the SAFE_AVG macro here for the snprintf section below
#define SAFE_AVG(area, valid) ((valid) > 0 ? ((double)(area) / (double)(valid)) / 100.0 : 0.0)
  time_t finalTs = reconstructTimestamp(collectionTime);
  const char *timestampStr = formatTimestamp(finalTs);
  unsigned long intendedIntervalSec = SENSOR_UPLOAD_INTERVAL / 1000;
  // NORMAL JSON structure - but possibly with bad token/UID/timestamp
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
    // VALUES START HERE
    device_id_hex,      // device_uid
    authToken.c_str(),  // token
    timestampStr,       // timestamp
    FIRMWARE_VERSION,
    currentWindow->battVolt_min / 100.0,
    currentWindow->battVolt_max / 100.0,
    SAFE_AVG(currentWindow->battVolt_area_v_us, currentWindow->battVolt_valid_us),
    AvgVoltage_AllTime,
    (unsigned long)totalVoltageSampleTime_AllTime,
    currentWindow->battCurr_min / 100.0,
    currentWindow->battCurr_max / 100.0,
    SAFE_AVG(currentWindow->battCurr_area_v_us, currentWindow->battCurr_valid_us),
    currentWindow->altCurr_min / 100.0,
    currentWindow->altCurr_max / 100.0,
    SAFE_AVG(currentWindow->altCurr_area_v_us, currentWindow->altCurr_valid_us),
    currentWindow->victronCurr_min / 100.0,
    currentWindow->victronCurr_max / 100.0,
    SAFE_AVG(currentWindow->victronCurr_area_v_us, currentWindow->victronCurr_valid_us),
    currentWindow->soc_min / 100.0,
    currentWindow->soc_max / 100.0,
    SAFE_AVG(currentWindow->soc_area_v_us, currentWindow->soc_valid_us),
    AvgSOC_AllTime,
    (unsigned long)totalSocSampleTime_AllTime,
    currentWindow->baro_min / 100.0,
    currentWindow->baro_max / 100.0,
    SAFE_AVG(currentWindow->baro_area_v_us, currentWindow->baro_valid_us),
    currentWindow->altTemp_min / 100.0,
    currentWindow->altTemp_max / 100.0,
    SAFE_AVG(currentWindow->altTemp_area_v_us, currentWindow->altTemp_valid_us),
    currentWindow->tempTherm_min / 100.0,
    currentWindow->tempTherm_max / 100.0,
    SAFE_AVG(currentWindow->tempTherm_area_v_us, currentWindow->tempTherm_valid_us),
    currentWindow->ambTemp_min / 100.0,
    currentWindow->ambTemp_max / 100.0,
    SAFE_AVG(currentWindow->ambTemp_area_v_us, currentWindow->ambTemp_valid_us),
    currentWindow->rpm_min,
    currentWindow->rpm_max,
    (int)SAFE_AVG(currentWindow->rpm_area_v_us, currentWindow->rpm_valid_us),
    currentWindow->wifiStr_min,
    currentWindow->wifiStr_max,
    (int)SAFE_AVG(currentWindow->wifiStr_area_v_us, currentWindow->wifiStr_valid_us),
    currentWindow->dutyCycle_min / 100.0,
    currentWindow->dutyCycle_max / 100.0,
    SAFE_AVG(currentWindow->dutyCycle_area_v_us, currentWindow->dutyCycle_valid_us),
    currentWindow->altZero_min / 100.0,
    currentWindow->altZero_max / 100.0,
    SAFE_AVG(currentWindow->altZero_area_v_us, currentWindow->altZero_valid_us),
    currentWindow->sog_min / 100.0,
    currentWindow->sog_max / 100.0,
    SAFE_AVG(currentWindow->sog_area_v_us, currentWindow->sog_valid_us),
    AvgSpeed_AllTime,
    (unsigned long)totalSpeedSampleTime_AllTime,
    currentWindow->cog_min / 100.0,
    currentWindow->cog_max / 100.0,
    SAFE_AVG(currentWindow->cog_area_v_us, currentWindow->cog_valid_us),
    currentWindow->heading_min / 100.0,
    currentWindow->heading_max / 100.0,
    SAFE_AVG(currentWindow->heading_area_v_us, currentWindow->heading_valid_us),
    currentWindow->aws_min / 100.0,
    currentWindow->aws_max / 100.0,
    SAFE_AVG(currentWindow->aws_area_v_us, currentWindow->aws_valid_us),
    currentWindow->awa_min / 100.0,
    currentWindow->awa_max / 100.0,
    SAFE_AVG(currentWindow->awa_area_v_us, currentWindow->awa_valid_us),
    currentWindow->tws_min / 100.0,
    currentWindow->tws_max / 100.0,
    SAFE_AVG(currentWindow->tws_area_v_us, currentWindow->tws_valid_us),
    currentWindow->twa_min / 100.0,
    currentWindow->twa_max / 100.0,
    SAFE_AVG(currentWindow->twa_area_v_us, currentWindow->twa_valid_us),
    currentWindow->leeway_min / 100.0,
    currentWindow->leeway_max / 100.0,
    SAFE_AVG(currentWindow->leeway_area_v_us, currentWindow->leeway_valid_us),
    currentWindow->vmg_min / 100.0,
    currentWindow->vmg_max / 100.0,
    SAFE_AVG(currentWindow->vmg_area_v_us, currentWindow->vmg_valid_us),
    currentWindow->lat_current,
    currentWindow->lon_current,
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
    currentWindow->uTargetAmps_min / 100.0,
    currentWindow->uTargetAmps_max / 100.0,
    SAFE_AVG(currentWindow->uTargetAmps_area_v_us, currentWindow->uTargetAmps_valid_us),
    currentWindow->tempMargin_min / 100.0,
    currentWindow->tempMargin_max / 100.0,
    SAFE_AVG(currentWindow->tempMargin_area_v_us, currentWindow->tempMargin_valid_us),
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

  if (written <= 0 || written >= PAYLOAD_BUFFER_SIZE) {
    fsReleaseLock();
    Serial.println("Failed to build buffered JSON (overflow)");
    return;
  }

  unsigned long filenameTime = (collectionTime < 0) ? (unsigned long)(-collectionTime) : (unsigned long)collectionTime;
  snprintf(filenameBuffer, FILENAME_BUFFER_SIZE, "%s/%010lu.json", SENSOR_BUFFER_DIR, filenameTime);
  filenameBuffer[FILENAME_BUFFER_SIZE - 1] = '\0';

  File file = LittleFS.open(filenameBuffer, "w");
  if (!file) {
    fsReleaseLock();
    Serial.println("Failed to open file for buffering");
    return;
  }
  size_t bytesWritten = file.write((const uint8_t *)payloadBuffer, written);
  file.close();

  if (bytesWritten != written) {
    queueConsoleMessage("Buffer save failed: incomplete write");
    fsReleaseLock();
    return;
  }

  bufferedRecordCount++;
  fsReleaseLock();
  esp_task_wdt_reset();
}
void uploadBufferedRecords() {
  if (otaInProgress) {
    return;  // Skip during OTA
  }
  if (bufferedRecordCount == 0) return;
  // Don't attempt uploads if not registered - files should be cleared manually
  if (!isRegistered || authToken.isEmpty()) {
    Serial.println("uploadBufferedRecords: Skipping - not registered (buffer has stale files)");
    return;
  }
  // Removed core0Busy check - queue handles busy state
  if (!canUploadNow()) {
    Serial.println("Skipping buffer upload, !canUploadNow");
    return;
  }
  unsigned long funcStart = millis();
  Serial.printf("uploadBufferedRecords: %d files buffered\n", bufferedRecordCount);
  esp_task_wdt_reset();
  fsTakeLock();
  File root = LittleFS.open(SENSOR_BUFFER_DIR);
  if (!root || !root.isDirectory()) {
    Serial.println("ERROR: Cannot open buffer directory");
    fsReleaseLock();
    return;
  }
  char oldestFile[FILENAME_BUFFER_SIZE] = "";
  File file = root.openNextFile();
  while (file) {
    if (!file.isDirectory()) {
      strncpy(oldestFile, file.path(), sizeof(oldestFile) - 1);
      oldestFile[sizeof(oldestFile) - 1] = '\0';
      file.close();
      break;
    }
    file.close();
    file = root.openNextFile();
  }
  root.close();
  if (oldestFile[0] == '\0') {
    Serial.println("ERROR: No valid files found in buffer");
    fsReleaseLock();
    return;
  }
  Serial.printf("Selected file: %s\n", oldestFile);
  File payloadFile = LittleFS.open(oldestFile, "r");
  if (!payloadFile) {
    Serial.printf("ERROR: Cannot open file: %s\n", oldestFile);
    fsReleaseLock();
    return;
  }
  size_t fileSize = payloadFile.size();
  if (fileSize == 0 || fileSize >= PAYLOAD_BUFFER_SIZE) {
    payloadFile.close();
    LittleFS.remove(oldestFile);
    if (bufferedRecordCount > 0) {
      bufferedRecordCount--;
    }
    fsReleaseLock();
    Serial.printf("Deleted invalid file (%d bytes): %s\n", (int)fileSize, oldestFile);
    return;
  }
  size_t bytesRead = payloadFile.readBytes(payloadBuffer, fileSize);
  payloadBuffer[bytesRead] = '\0';
  payloadFile.close();
  // Serial.printf("Read %d bytes from file\n", (int)bytesRead);
  // Serial.println("=== BUFFERED FILE CONTENTS ===");
  // Serial.printf("First 500 chars: %.500s\n", payloadBuffer);
  // Serial.println("==============================");
  fsReleaseLock();
  HttpsRequest req = {};
  req.type = HTTPS_UPLOAD_PAYLOAD;
  strncpy(req.payload, payloadBuffer, sizeof(req.payload) - 1);
  req.payload[sizeof(req.payload) - 1] = '\0';
  if (xQueueSend(httpsQueue, &req, 0) == pdTRUE) {
    strncpy(lastUploadedFilePath, oldestFile, sizeof(lastUploadedFilePath) - 1);
    lastUploadedFilePath[sizeof(lastUploadedFilePath) - 1] = '\0';
    lastUploadWasBuffered = true;
    Serial.println("Queued for upload (Core0 will delete on success)");
  } else {
    Serial.println("ERROR: Queue full, upload not queued");
  }
  unsigned long totalDuration = millis() - funcStart;
  Serial.printf("✓ Queued buffered record for upload (Core0 will process) - took %lums\n", totalDuration);
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
  client.setTimeout(5);
  client.setHandshakeTimeout(HANDSHAKE_TIMEOUT);

  uint32_t start = millis();
  esp_task_wdt_reset();

  Serial.println("Connecting...");
  // Hard-bounded TLS connect
  if (!client.connect(host, port, CONNECT_TIMEOUT)) {
    Serial.println("Connect fail");
    client.stop();
    return false;
  }

  // NEW: Defensive global timeout check after connect/handshake
  if (millis() - start > GLOBAL_TIMEOUT) {
    Serial.println("Connect exceeded global timeout");
    client.stop();
    return false;
  }

  Serial.println("Connected");
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

  Serial.printf("HTTP %d\n", httpCode);
  lastHttpResponseCode = httpCode;

  // ===== Handle file deletion based on response code =====
  if (lastUploadWasBuffered && lastUploadedFilePath[0] != '\0') {

    if (httpCode == 200) {
      // ✅ SUCCESS - Delete uploaded file
      // Serial.printf("HTTP 200: Attempting delete of '%s'\n", lastUploadedFilePath);
      if (fsRemove(lastUploadedFilePath)) {
        fsTakeLock();
        if (bufferedRecordCount > 0) {
          bufferedRecordCount--;
        }
        fsReleaseLock();
        Serial.printf("✓ Uploaded & deleted: %s (remaining: %d)\n", lastUploadedFilePath, bufferedRecordCount);
        if (bufferedRecordCount == 0) {
          queueConsoleMessage("Cloud sync: all data uploaded");
        } else {
          snprintf(messageBuffer, MESSAGE_BUFFER_SIZE, "Cloud sync: %d records queued", bufferedRecordCount);
          queueConsoleMessage(messageBuffer);
        }
      } else {
        Serial.printf("✗ Upload succeeded but delete failed: %s\n", lastUploadedFilePath);
      }
      lastUploadedFilePath[0] = '\0';

    } else if (httpCode == 400 || httpCode == 401) {
      // ❌ VALIDATION ERROR - Delete bad data immediately
      Serial.printf("HTTP %d: Deleting bad data: %s\n", httpCode, lastUploadedFilePath);
      if (fsRemove(lastUploadedFilePath)) {
        fsTakeLock();
        if (bufferedRecordCount > 0) {
          bufferedRecordCount--;
        }
        fsReleaseLock();
        Serial.printf("✓ Deleted bad data (remaining: %d)\n", bufferedRecordCount);
        snprintf(messageBuffer, MESSAGE_BUFFER_SIZE, "Cleared bad data (%d queued)", bufferedRecordCount);
        queueConsoleMessage(messageBuffer);
      } else {
        Serial.printf("✗ Failed to delete bad data: %s\n", lastUploadedFilePath);
      }
      lastUploadedFilePath[0] = '\0';

    } else {
      // ⚠️ NETWORK/SERVER ERROR - Keep file for retry
      Serial.printf("⚠ HTTP %d: Keeping file for retry: %s\n", httpCode, lastUploadedFilePath);
      snprintf(messageBuffer, MESSAGE_BUFFER_SIZE, "Cloud sync failed (HTTP %d)", httpCode);
      queueConsoleMessage(messageBuffer);
      // Don't clear lastUploadedFilePath or set to '\0' - will retry
    }

  } else {
    // No buffered file or path not set
    Serial.printf("No delete: code=%d, buffered=%d, path='%s'\n",
                  httpCode, lastUploadWasBuffered, lastUploadedFilePath);
    if (lastUploadWasBuffered && httpCode != 200) {
      snprintf(messageBuffer, MESSAGE_BUFFER_SIZE, "Cloud sync failed (HTTP %d)", httpCode);
      queueConsoleMessage(messageBuffer);
    }
  }

  // NEW: Only clear buffered flag when we're done with the file (path cleared).
  // This avoids losing delete/retry state if upstream doesn't re-arm the flag.
  if (lastUploadedFilePath[0] == '\0') {
    lastUploadWasBuffered = false;
  }

  return (httpCode == 200);
}

void uploadSensorHistory() {
  if (otaInProgress) {
    return;  // Skip during OTA
  }
  // Serial.printf("=== UPLOAD DEBUG ===\n");
  // Serial.printf("authToken: '%s' (len=%d)\n", authToken.c_str(), authToken.length());
  // Serial.printf("device_id_hex: '%s'\n", device_id_hex);
  // Serial.printf("isRegistered: %d\n", isRegistered);
  // Serial.printf("===================\n");

  if (currentWindow->battVolt_valid_us == 0) {  // check if any data collected
    resetSensorWindow();
    resetAccelWindow();
    return;  // No sensor data at all, skip upload
  }

  esp_task_wdt_reset();

  //Serial.println("=== uploadSensorHistory() ===");
  // Serial.printf("Samples: %u, buffered: %d\n", currentWindow->sampleCount, bufferedRecordCount);  // OLD - commented out

  // Sync time if needed
  if (currentMode == MODE_CLIENT && WiFi.status() == WL_CONNECTED && currentTimeSource != TIME_GPS && !timeIsSynced) {
    syncTimeFromNTP();
  }
  time_t collectionTime = computeCollectionTime();
  saveToLocalBuffer(collectionTime);  // This eventually will include IMU data in JSON
  // CHANGED - Updated debug to show time-weighted stats
  //Serial.printf("UPLOAD DEBUG: baro_min=%d, baro_max=%d, baro_area=%lld, baro_valid_us=%llu\n",
  //  currentWindow->baro_min, currentWindow->baro_max,
  //currentWindow->baro_area_v_us, currentWindow->baro_valid_us);
  // Reset BOTH windows after local save
  resetSensorWindow();
  resetAccelWindow();
  esp_task_wdt_reset();
}
// Below is NOT REALLY NEEDED.  I only need to run it once to clear bad tokens from buffer leftover from old strategies
void clearSensorBuffer() {
  //Serial.println("Clearing sensor buffer...");

  fsTakeLock();

  File root = LittleFS.open(SENSOR_BUFFER_DIR);
  if (!root || !root.isDirectory()) {
    fsReleaseLock();
    Serial.println("No sensor buffer directory found");
    return;
  }

  int removed = 0;
  File file = root.openNextFile();

  while (file) {
    esp_task_wdt_reset();

    if (!file.isDirectory()) {
      String path = file.path();
      file.close();
      if (LittleFS.remove(path.c_str())) {  // Already inside lock
        removed++;
      } else {
        Serial.printf("Failed to remove %s\n", path.c_str());
      }
    }
    file = root.openNextFile();
  }

  bufferedRecordCount = 0;
  fsReleaseLock();
  Serial.printf("Cleared %d buffered records\n", removed);
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
                     ",\"hi_low\":%d,\"amp_src\":%d",
                     OnOff, Ignition, IgnitionOverride, HiLow, AmpSrc);

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
                     ",\"auto_save_learning_table\":%d,\"learning_table_save_interval\":%lu"
                     ",\"clear_overheat_history\":%d,\"overheating_penalty_timer\":%lu"
                     ",\"overheating_penalty_amps\":%.1f,\"total_learning_events\":%lu"
                     ",\"total_overheats\":%lu,\"total_safe_hours\":%lu"
                     ",\"average_table_value\":%.2f",
                     NeighborLearningFactor, yyMax, LearningMemoryDuration,
                     IgnoreLearningDuringPenalty, EnableNeighborLearning, EnableAmbientCorrection,
                     TuningMode, LearningDryRunMode, AutoSaveLearningTable,
                     LearningTableSaveInterval, ClearOverheatHistory, overheatingPenaltyTimer,
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
                     ChargeEfficiency_scaled, ChargedVoltage_Scaled, TailCurrent,
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
  client.setTimeout(5);
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

  Serial.println("Config: Connected");
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
  esp_task_wdt_reset();

  configTime(0, 0, "pool.ntp.org", "time.nist.gov");

  struct tm timeinfo;
  int retries = 0;
  const int MAX_NTP_RETRIES = 3;
  const int NTP_TIMEOUT_MS = 3000;

  while (retries < MAX_NTP_RETRIES) {
    esp_task_wdt_reset();  // Feed watchdog each attempt

    if (getLocalTime(&timeinfo, NTP_TIMEOUT_MS)) {
      timeBase = time(nullptr);
      timeBaseMillis = millis();
      timeIsSynced = true;
      currentTimeSource = TIME_NTP;
      lastTimeSyncAttempt = millis();

      saveTimeSyncState();  // Persist immediately

      queueConsoleMessage("Time synced from NTP");
      Serial.printf("NTP synced: epoch=%ld\n", timeBase);
      return;
    }

    retries++;
    Serial.printf("NTP attempt %d/%d failed\n", retries, MAX_NTP_RETRIES);
    esp_task_wdt_reset();
    delay(500);
  }

  Serial.println("NTP sync failed after all attempts");
  lastTimeSyncAttempt = millis();
}

bool canUploadNow() {
  if (WiFi.status() != WL_CONNECTED) return false;
  if (WiFi.RSSI() < -76) return false;
  return true;
}