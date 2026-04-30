/**  
* AI_SUMMARY: Alternator Regulator Project - Hardware: ESP32-S3-WROOM-1U-N16R8 with PSRAM enabled- this file contains libraries, hardcoded values, setup(), and loop(). Supporting functions are in thee additional files: 2_importantFunctions should always be parsed, but 3_nonImportantFunctions and 4_optional are usually not necessary, containing functions less relevant to this debugging session.
* AI_PURPOSE: Read data from various sensors (ADS1115, INA228, NMEA2K CAN Network, VictronVEDirect), control alternator field output, provide real-time user interface with persistent settings
* AI_INPUTS: Sensor data from hardware, hardcoded values, and settings from user interface
* AI_OUTPUTS: user interface display, control of GPIO
* AI_DEPENDENCIES:
* AI_RISKS: Variable naming is inconsistent, need to be careful not to assume consistent patterns. Unit conversion can be very confusing and propagate to many places, have to trace dependencies in variables ALL THE WAY to every end point.  Everything works, but the battery monitor units are challenging to follow thru the codebase.
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

#include <OneWire.h>            // temp sensors
#include <DallasTemperature.h>  // temp sensors
//#include <SPI.h>                // display- removed to save connectors space
//#include <U8g2lib.h>            // display- removed to save connectors space
#include <ADS1115_lite.h>  // measuring 4 analog inputs
ADS1115_lite adc(ADS1115_DEFAULT_ADDRESS);
#include "VeDirectFrameHandler.h"  // for victron communication
#include "INA228.h"
INA228 INA(0x40);
//DONT MOVE THE NEXT 6 LINES AROUND, MUST STAY IN THIS ORDER
#define ESP32_CAN_RX_PIN GPIO_NUM_16                           //
#define ESP32_CAN_TX_PIN GPIO_NUM_17                           //
#include <NMEA2000_esp32.h>                                    // (This one is for ESP32-S3, thank you Svante Karlsson and Timo Lappalainen )
tNMEA2000_esp32 NMEA2000(ESP32_CAN_TX_PIN, ESP32_CAN_RX_PIN);  // necessary for ESP32-S3 library
#include <N2kMessages.h>
#include <N2kMessagesEnumToStr.h>  // questionably needed
#include <WiFi.h>
#include <AsyncTCP.h>                    // for wifi stuff, IMPORTANT, don't ever update, use mathieucarbou github repository
#include <LittleFS.h>                    //
fs::LittleFSFS webFS;                    // Separate filesystem for web files
bool usingFactoryWebFiles = false;       // Track which web partition is mounted
#include <ESPAsyncWebServer.h>           // for wifi stuff, important, don't ever update, use mathieucarbou github repository
#include <DNSServer.h>                   // For captive portal (Wifi Network Provisioning) functionality
#include <ESPmDNS.h>                     // helps with wifi provisioning (to save users trouble of looking up ESP32's IP address)
#include "esp_heap_caps.h"               // needed for tracking heap usage
#include "freertos/FreeRTOS.h"           // for stack usage
#include "freertos/task.h"               // for stack usage
#define configGENERATE_RUN_TIME_STATS 1  // for CPU use tracking
#include <mbedtls/md.h>                  // security
#include <vector>                        // Console message queue system
#include <String>                        // Console message queue system
#include "esp_task_wdt.h"                //Watch dog to prevent hung up code from wreaking havoc
#include "esp_log.h"                     // get rid of spam in serial monitor
#include <TinyGPSPlus.h>                 // used for NMEA0183, was working great but not currently implemented
#include <math.h>                        // For pow() function needed by Peukert calculation
#include "nvs_flash.h"                   //for persistent storage that's better than Flash
#include "nvs.h"                         //for persistent storage that's better than Flash
#include <HTTPClient.h>                  // for weather reports
#include <ArduinoJson.h>                 // for weather reports (and secure OTA?)
#include <WiFiClientSecure.h>            // for weather reports
#include <PID_v1_xeng.h>                 // My fork to include tracking anti-windup methods       Thanks Brett!
#include <esp_ota_ops.h>                 // secure OTA
#include <mbedtls/pk.h>                  // secure OTA
#include <mbedtls/base64.h>              // secure OTA
#include "esp_system.h"                  // secure OTA
#include <BMP388_DEV.h>                  //non blocking capability - don't upgrade, this was customized for better error handling by xengineering
#include "esp_partition.h"               // for esp_partition_find_first
#include "esp_heap_caps.h"
#include <time.h>            // Supabase
#include "esp_psram.h"       // for ESP32 health calculations
#include "LSM6DSOXSensor.h"  //accelerometer


// Make the types visible to auto-generated prototypes, hack
struct UpdateInfo;
struct StreamingExtractor;
struct HttpsRequest;

SET_LOOP_TASK_STACK_SIZE(20 * 1024);  // Increase stack from 8KB to 20KB, necessary for SSL/TLS operations, backtraced at 12 on 4/18/26
int hardwarePresent = 1;              // usage varies
// Parse JSON update response - this struct definition cannot move down in file, I don't know why, but leave it here.
struct UpdateInfo {
  bool hasUpdate;
  String version;
  String firmwareUrl;
  String signatureUrl;
  String changelog;
  size_t firmwareSize;
};

struct CachedGzFile {
  uint8_t *data = nullptr;
  size_t size = 0;
};

CachedGzFile loadFileToRAM(const char *path);  // add this line

CachedGzFile cachedIndex, cachedCss, cachedJs, cachedUplotCss, cachedUplotJs;

CachedGzFile loadFileToRAM(const char *path) {
  CachedGzFile result;
  File f = webFS.open(path, "r");
  if (!f) {
    Serial.printf("preload FAILED: %s\n", path);
    return result;
  }
  result.size = f.size();
  result.data = (uint8_t *)ps_malloc(result.size);                 // PSRAM-aware malloc
  if (!result.data) result.data = (uint8_t *)malloc(result.size);  // heap fallback
  if (result.data) {
    f.read(result.data, result.size);
    Serial.printf("Preloaded %s into RAM (%d bytes)\n", path, result.size);
  } else {
    Serial.printf("preload malloc FAILED: %s\n", path);
    result.size = 0;
  }
  f.close();
  return result;
}

// ── Async LittleFS write queue ────────────────────────────────
// Decouples writeFileThrottled() from the actual flash write.
// FlushFileWriteQueue() drains one entry per loop() call.
// If queue fills (drops > 0), increase FS_WRITE_QUEUE_DEPTH.
#define FS_WRITE_QUEUE_DEPTH 16
#define FS_WRITE_PATH_MAX 48
#define FS_WRITE_DATA_MAX 48  // bump if any setting value exceeds 47 chars

struct PendingFSWrite {
  char path[FS_WRITE_PATH_MAX];
  char data[FS_WRITE_DATA_MAX];
};

static PendingFSWrite fsWriteQueue[FS_WRITE_QUEUE_DEPTH];
static volatile uint8_t fsWriteHead = 0;  // consumer (FlushFileWriteQueue)
static volatile uint8_t fsWriteTail = 0;  // producer (writeFileThrottled)
uint32_t fsWriteQueueDrops = 0;           // wire into telemetry

// ============= HTTPS TASK SYSTEM =============
int lastHttpResponseCode = 0;  // Track last HTTP response for failure handling
QueueHandle_t httpsQueue;
TaskHandle_t httpsTaskHandle;

SemaphoreHandle_t fsMutex = NULL;  // mutex protection for littlefs

enum HttpsRequestType {
  HTTPS_UPLOAD_PAYLOAD,
  HTTPS_UPLOAD_CONFIG,
  HTTPS_FETCH_WEATHER,
  HTTPS_UPDATE_FW_VERSION,
  HTTPS_CHECK_FORCED_UPDATE,
  HTTPS_CLEAR_FORCED_UPDATE
};

struct HttpsRequest {
  HttpsRequestType type;
  char payload[6144];
};

// OTA server configuration
const char *OTA_SERVER_URL = "https://ota.xengineering.net";
// IMPORTANT: Firmware Version number MUST follow semantic versioning (x.y.z format)
// - Only numeric digits and dots allowed (regex: ^\d+\.\d+\.\d+$)
// - Examples: "1.0.0", "2.1.3", "10.5.22" ✅
// - Invalid: "1.1.1Retry", "v2.0.0", "2.1.0-beta" ❌
//Maximum supported version = "999.99.99" → 999*10000 + 99*100 + 99 = 9,999,999
//         Safe Version Ranges:
//major: 0-999   (4 digits max)
//minor: 0-99    (2 digits max)
//patch: 0-99    (2 digits max)
const char *FIRMWARE_VERSION = "0.0.32";

String currentUID;

// ============================================================
// ============================================================
// EFFICIENCY MATRIX — CONFIG
// To change bucket counts or ranges, edit ONLY this section.
// If you change anything here, wipe NVS (Start Over button).
// ============================================================

// ── Session health history — persisted circular buffer ──────────
// Each entry is the average (actual_amps / ref_avg_amps) ratio
// for one power session. 1.0 = perfect, 0.85 = 15% degraded.
// Committed to NVS at start of next session so power loss
// during a session doesn't corrupt prior history.

#define EFF_HISTORY_SESSIONS 30

struct EffHistoryData {
  float values[EFF_HISTORY_SESSIONS];
  uint8_t head;
  uint8_t count;
} __attribute__((packed));

static EffHistoryData effHistory = {};

// Current session accumulator — saved to NVS every 2 min
// so it survives unexpected power loss
static float sessionHealthSum = 0.0f;
static uint32_t sessionHealthCount = 0;

// Set when history changes — triggers SSE resend
static bool effHistoryDirty = false;

#define FUNC_TIMING_WINDOW_MS 10000  // rolling window for per-function worst-case timing (ms)


// --- Field-drive axis ---
#define NUM_FIELD_BUCKETS 7
#define EFF_FIELD_MIN 0.0f   // PLACEHOLDER: adjust if your alternator never goes below X
#define EFF_FIELD_MAX 15.0f  // PLACEHOLDER: adjust if your field ever exceeds 15V

static const float FIELD_BOUNDS[NUM_FIELD_BUCKETS + 1] = {
  0.0f, 2.14f, 4.29f, 6.43f, 8.57f, 10.71f, 12.86f, 15.0f
};
static const char *FIELD_LABELS[NUM_FIELD_BUCKETS] = {
  "0-2.1V", "2.1-4.3V", "4.3-6.4V", "6.4-8.6V",
  "8.6-10.7V", "10.7-12.9V", "12.9-15V"
};

// --- RPM axis (unchanged from prior system) ---
#define NUM_RPM_BUCKETS 8
static const float RPM_BOUNDS[NUM_RPM_BUCKETS + 1] = {
  0.0f, 500.0f, 1000.0f, 1500.0f, 2000.0f,
  2500.0f, 3000.0f, 4000.0f, 99999.0f
};
static const char *RPM_LABELS[NUM_RPM_BUCKETS] = {
  "0-500", "500-1k", "1k-1.5k", "1.5k-2k",
  "2k-2.5k", "2.5k-3k", "3k-4k", "4k+"
};

// --- Temperature axis ---
#define NUM_TEMP_BUCKETS 7
static const float TEMP_BOUNDS[NUM_TEMP_BUCKETS + 1] = {
  0.0f, 80.0f, 110.0f, 140.0f, 160.0f, 180.0f, 200.0f, 9999.0f
};
static const char *TEMP_LABELS[NUM_TEMP_BUCKETS] = {
  "<80F", "80-110F", "110-140F", "140-160F", "160-180F", "180-200F", ">200F"
};

// --- Total matrix size ---
#define NUM_MATRIX_CELLS (NUM_RPM_BUCKETS * NUM_TEMP_BUCKETS * NUM_FIELD_BUCKETS)
// 8 × 3 × 7 = 168 cells

// --- Reference bin selection criteria (all PLACEHOLDER — tune after first data) ---
#define NUM_REFERENCE_BINS 10         // Top N bins by accumulated SS time
#define REF_MIN_SS_SECONDS 300        // PLACEHOLDER: min SS seconds for a bin to be eligible (~5 min)
#define REF_FREEZE_TOTAL_SS 6000      // PLACEHOLDER: total SS seconds across selected bins to trigger freeze (~100 min)
#define REF_SPREAD_TEMP_DEG 50.0f     // PLACEHOLDER: min temp span across selected bins (°F)
#define REF_SPREAD_FIELD_VOLTS 3.75f  // PLACEHOLDER: min field-drive span (25% of 15V range)
#define REF_SPREAD_RPM 1000.0f        // PLACEHOLDER: min RPM span across selected bins

// --- Anomaly detection — runtime, user-configurable via LittleFS ---
float anomalyMarginAmps = 5.0f;   // Extra amps of tolerance beyond ref min/max (default: none)
int anomalyAlarmThreshold = 5;    // Session error count before alarm fires
bool anomalyAlarmEnable = false;  // Whether to trigger actual alarm output

// --- Misc ---
#define EFF_MIN_BATT_V 8.0f  // Minimum plausible battery voltage for field-volts computation

// ============================================================
// DATA STRUCTURES
// ============================================================

struct MatrixCell {
  // Everlasting layer — accumulates forever, never resets except manual wipe
  uint32_t ss_seconds;  // Total steady-state seconds accumulated in this bin
  float avg_amps;       // Time-weighted running average output amps
  float min_amps;       // Observed minimum output amps
  float max_amps;       // Observed maximum output amps

  // Reference layer — written once when freeze criteria are met; never modified after
  float ref_avg_amps;
  float ref_min_amps;
  float ref_max_amps;
  uint8_t is_reference_bin;  // 1 = finalized reference bin, 0 = not
};

// Window accumulator — small fixed array, PSRAM, session only.
// Tracks only bins active in the current 2-minute window.
// MAX_ACTIVE_BINS_PER_WINDOW = 12 gives headroom for warmup transients.
#define MAX_ACTIVE_BINS_PER_WINDOW 12

struct WindowSlot {
  int8_t r, t, f;       // Which bin (RPM, temp, field bucket indices)
  uint32_t ss_seconds;  // SS seconds accumulated this window
  float wt_avg_amps;    // Running weighted average amps this window
  float min_amps;       // Min amps seen this window
  float max_amps;       // Max amps seen this window
  bool active;          // True if this slot is in use
};

static WindowSlot *effWindow = nullptr;

// Per-bin session stats — PSRAM, resets each power cycle.
// Accumulates current session's readings for Layer 2 thermal-average comparison.
// count >= 90 required before Layer 2 fires (matches ~90s thermal time constant).
struct SessionBinStats {
  float sum_amps;    // Running sum of qualifying amps this session
  uint16_t count;    // Number of qualifying samples this session
  bool trend_fired;  // True once Layer 2 has already fired for this bin this session
};
static SessionBinStats *sessionStats = nullptr;  // Allocated in PSRAM at init

// Layer 2 thermal average comparison threshold — user adjustable via LittleFS
// 0.15 = alert if session average is more than 15% above or below reference average
float degradationThreshold = 0.15f;

// Separate error counters for each layer — summed for alarm threshold comparison
static int sessionLayer1Errors = 0;  // Instantaneous min/max violations
static int sessionLayer2Errors = 0;  // Thermal average drift violations

// Matrix in PSRAM — allocated in initEfficiencyTracker()
static MatrixCell *effMatrix = nullptr;

bool effAnomalyAlarmActive = false;  // Set by checkAnomaly(), cleared on reset

// Row-major index: [rpmBucket][tempBucket][fieldBucket]
#define MATRIX_IDX(r, t, f) ((r)*NUM_TEMP_BUCKETS * NUM_FIELD_BUCKETS + (t)*NUM_FIELD_BUCKETS + (f))
#define MATRIX_CELL(r, t, f) effMatrix[MATRIX_IDX(r, t, f)]
#define WINDOW_ACCUM(r, t, f) effWindow[MATRIX_IDX(r, t, f)]

// Reference finalization state (persisted in NVS alongside matrix)
static bool referenceFinalized = false;

// Active 3D bucket (updated every 1Hz tick)
static int activeRPMBucket = -1;
static int activeTempBucket = -1;
static int activeFieldBucket = -1;

// Red dot live state
static float redDot_fieldVolts = 0.0f;
static float redDot_amps = 0.0f;
static bool redDotValid = false;

// Session anomaly counter — resets on power cycle
static int sessionErrorCount = 0;

// Plot axis limits sent to JS
float EffXMin = EFF_FIELD_MIN;
float EffXMax = EFF_FIELD_MAX;
float EffYMin = 0.0f;
float EffYMax = 150.0f;


// Supabase configuration......similar stuff is locally defined in a few functions that upload sensor data, config snapshots to cloud
const char *SUPABASE_URL = "https://qnbekuaoweuteylitzvo.supabase.co";
// PUBLIC anon key - safe to commit, protected by RLS policies
const char *SUPABASE_ANON_KEY = "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJpc3MiOiJzdXBhYmFzZSIsInJlZiI6InFuYmVrdWFvd2V1dGV5bGl0enZvIiwicm9sZSI6ImFub24iLCJpYXQiOjE3NTE5NzY1MzUsImV4cCI6MjA2NzU1MjUzNX0.k2S_kzkdAyN1Azs_7enxLun9LouB1bA_q7Sw8x1Cp0o";


// Vessel Info (14 new variables)
float BOAT_LENGTH_FT = 0;
String BOAT_TYPE = "monohull";
String BOAT_MAKE_MODEL = "";
uint16_t BOAT_YEAR = 2025;
String ENGINE_MAKE = "";
uint16_t ENGINE_HP = 0;
uint8_t BATTERY_VOLTAGE = 12;
// BatteryCapacity_Ah already exists
String BATTERY_TYPE = "lifepo4";
String ALTERNATOR_BRAND_MODEL = "";
// SolarWatts already exists
// imuMountOrientation already exists
float IMU_DIST_BOW_FT = 0;
float IMU_DIST_CL_FT = 0;
float IMU_HEIGHT_WL_FT = 0;
char HOME_PORT[51] = "";  // 50 chars + null terminator


// Accelerometer Stuff
// I²C address - ST-style 8-bit format (library shifts internally)
// LSM6DSOX_I2C_ADD_L = 0xD5 (SA0=0), LSM6DSOX_I2C_ADD_H = 0xD7 (SA0=1)
constexpr uint8_t LSM6DSOX_ADDR = LSM6DSOX_I2C_ADD_L;

// LSM6DSOX sensor object (shares existing Wire instance)
LSM6DSOXSensor imu(&Wire, LSM6DSOX_ADDR);

// FIFO tag_sensor values (after decoding: tag_sensor = raw_tag >> 3)
constexpr uint8_t TAG_SENSOR_GYRO = 1;
constexpr uint8_t TAG_SENSOR_ACCEL = 2;
constexpr uint8_t TAG_SENSOR_TEMP = 3;

// Axis remapping (compile-time constants, will move to settings later)
// Adjust these based on PCB mounting orientation relative to boat axes
constexpr int8_t ACCEL_X_SIGN = 1;  // +1 or -1
constexpr int8_t ACCEL_Y_SIGN = 1;
constexpr int8_t ACCEL_Z_SIGN = 1;
constexpr int8_t GYRO_X_SIGN = 1;
constexpr int8_t GYRO_Y_SIGN = 1;
constexpr int8_t GYRO_Z_SIGN = 1;

// Ring buffer sample structure
struct IMUSample {
  int16_t x, y, z;
  uint32_t timestamp_us;
};

// Verify struct size at compile time (expect 10 bytes logical, possibly 12 with padding)
static_assert(sizeof(IMUSample) == 10 || sizeof(IMUSample) == 12,
              "IMUSample unexpected size - check padding");

// Ring buffer sizes (adjust based on actual sizeof(IMUSample))
// Target ~32 KB total. If sizeof=12, use smaller counts.
constexpr uint16_t ACCEL_RING_SIZE = (sizeof(IMUSample) == 10) ? 2000 : 1666;  // ~24 KB
constexpr uint16_t GYRO_RING_SIZE = (sizeof(IMUSample) == 10) ? 666 : 555;     // ~8 KB

volatile bool otaInProgress = false;
unsigned long lastEventSourceSend = 0;


// Ring buffers (static allocation, no heap). // OLD COMMENT, WAS THIS BS?
struct ImuRingBuffer {
  IMUSample accel[ACCEL_RING_SIZE];
  IMUSample gyro[GYRO_RING_SIZE];
  uint16_t accel_head = 0;
  uint16_t accel_tail = 0;
  uint16_t gyro_head = 0;
  uint16_t gyro_tail = 0;
  uint32_t accel_dropped = 0;
  uint32_t gyro_dropped = 0;
};
static ImuRingBuffer *imuRingBuffer = nullptr;

// IMU polling state
bool imuEnabled = false;
unsigned long lastIMUPoll = 0;
constexpr unsigned long IMU_POLL_INTERVAL = 10;  // ms

// FIFO drain budget
constexpr uint16_t MAX_FIFO_DRAIN_PER_POLL = 6;   // Conservative start, tune up if needed
uint8_t fifoBuffer[MAX_FIFO_DRAIN_PER_POLL * 7];  // 7 bytes per FIFO entry

// Error/diagnostic counters
uint32_t imu_fifo_overrun_count = 0;
uint32_t imu_i2c_error_count = 0;
uint32_t imu_total_samples_accel = 0;
uint32_t imu_total_samples_gyro = 0;
uint32_t imu_unknown_tag_count = 0;

// Collision avoidance threshold
constexpr int ANALOG_READ_COLLISION_THRESHOLD = 1200;  // µs - skip IMU if analog read was long


// ============================================================================
// IMU DERIVED METRICS - GLOBAL VARIABLES
// ============================================================================

// --- RAW SIGNALS (Current values, updated at reporting rate) ---
// Note: Stored as int16 in ring buffers, converted to float only when needed
float imu_accel_x_raw = 0;  // g
float imu_accel_y_raw = 0;  // g
float imu_accel_z_raw = 0;  // g
float imu_gyro_x_raw = 0;   // dps
float imu_gyro_y_raw = 0;   // dps
float imu_gyro_z_raw = 0;   // dps

// --- CALCULATED METRICS (Real-time) ---
float imu_heel_deg = 0;             // Current heel angle
float imu_pitch_deg = 0;            // Current pitch angle
float imu_yaw_rate_dps = 0;         // Current yaw rate (direct from gyro_z)
float imu_vertical_accel_g = 0;     // Vertical acceleration
float imu_total_accel_g = 0;        // Total acceleration magnitude
float imu_hf_vibration_energy = 0;  // High-freq vibration energy (RMS²)

// --- RAW SIGNAL STATISTICS (Reset every reporting period) ---
// Accel X
float imu_accel_x_min = 999.0;
float imu_accel_x_max = -999.0;
float imu_accel_x_sum = 0;
uint32_t imu_accel_x_count = 0;

// Accel Y
float imu_accel_y_min = 999.0;
float imu_accel_y_max = -999.0;
float imu_accel_y_sum = 0;
uint32_t imu_accel_y_count = 0;

// Accel Z
float imu_accel_z_min = 999.0;
float imu_accel_z_max = -999.0;
float imu_accel_z_sum = 0;
uint32_t imu_accel_z_count = 0;

// Gyro X
float imu_gyro_x_min = 999.0;
float imu_gyro_x_max = -999.0;
float imu_gyro_x_sum = 0;
uint32_t imu_gyro_x_count = 0;

// Gyro Y
float imu_gyro_y_min = 999.0;
float imu_gyro_y_max = -999.0;
float imu_gyro_y_sum = 0;
uint32_t imu_gyro_y_count = 0;

// Gyro Z
float imu_gyro_z_min = 999.0;
float imu_gyro_z_max = -999.0;
float imu_gyro_z_sum = 0;
uint32_t imu_gyro_z_count = 0;

// --- CALCULATED METRIC STATISTICS (Reset every reporting period) ---

// Event detection thresholds
float CAPSIZE_THRESHOLD_DEG = 120.0f;
float PITCHPOLE_THRESHOLD_DEG = 70.0f;
float SLAM_THRESHOLD_G = 2.5f;  // Vertical accel threshold for slam

// Heel
float imu_heel_min = 999.0;
float imu_heel_max = -999.0;
float imu_heel_sum = 0;
uint32_t imu_heel_count = 0;

// Pitch
float imu_pitch_min = 999.0;
float imu_pitch_max = -999.0;
float imu_pitch_sum = 0;
uint32_t imu_pitch_count = 0;

// Vertical accel
float imu_vertical_accel_min = 999.0;
float imu_vertical_accel_max = -999.0;
float imu_vertical_accel_sum = 0;
uint32_t imu_vertical_accel_count = 0;

// Total accel
float imu_total_accel_min = 999.0;
float imu_total_accel_max = -999.0;
float imu_total_accel_sum = 0;
uint32_t imu_total_accel_count = 0;

// --- SUMMARY METRICS (Time-windowed, 60s rolling) ---
float imu_heel_change_60s = 0;     // max - min over 60s
float imu_heel_deviation_60s = 0;  // deviation from mean over 60s
float imu_pitch_change_60s = 0;
float imu_pitch_deviation_60s = 0;

// --- EVENT COUNTERS ---
uint32_t imu_capsize_count = 0;        // Lifetime, persistent to NVS
uint32_t imu_pitchpole_count = 0;      // Lifetime, persistent to NVS
uint32_t imu_slam_count = 0;           // Reset every reporting period
uint32_t imu_slam_count_lifetime = 0;  // Lifetime, persistent to NVS
float imu_slam_peak_max = 0;           // Reset every reporting period

// --- WAVE PERIOD ---
float imu_wave_period_sec = -1.0;  // -1 = no valid detection

// --- LIFETIME MAXIMUMS (Persistent to NVS) ---
float imu_heel_max_lifetime = 0;
float imu_pitch_max_lifetime = 0;
float imu_slam_peak_lifetime = 0;

// --- MOUNTING ORIENTATION ---
uint8_t imuMountOrientation = 0;  // 0-3, saved to NVS key "IMU_Orient"

// --- COMPLEMENTARY FILTER STATE ---
float cf_heel = 0;   // Filtered heel angle
float cf_pitch = 0;  // Filtered pitch angle
unsigned long cf_lastUpdate = 0;

// --- WAVE PERIOD DETECTION STATE ---
float wave_decimated_buffer[100];  // Circular buffer for decimated samples
uint16_t wave_decim_head = 0;
float wave_last_dc_mean = 0;  // For DC removal
bool wave_last_crossing_positive = false;
unsigned long wave_last_crossing_time = 0;
float wave_period_ewma = -1.0;

// IMU NVS cache variables (may belong with other prev_* variables, but...)
static uint32_t prev_imu_capsize_count = 0;
static uint32_t prev_imu_pitchpole_count = 0;
static uint32_t prev_imu_slam_count_lifetime = 0;
static float prev_imu_heel_max_lifetime = 0;
static float prev_imu_pitch_max_lifetime = 0;
static float prev_imu_slam_peak_lifetime = 0;
static uint8_t prev_imuMountOrientation = 0;
static float prev_CAPSIZE_THRESHOLD_DEG = 0;
static float prev_PITCHPOLE_THRESHOLD_DEG = 0;
static float prev_SLAM_THRESHOLD_G = 0;



// Forced update tracking
unsigned long forcedUpdateDeadline = 0;
bool hasForcedUpdate = false;
int forcedFwVersionInt = 0;

// WiFiClientSecure secureClient;  // Reusable SSL client to prevent stack overflow when we had individual ones for each upload   ABANDONED, THIS WAS NOT GOOD IN FLAKY WIFI
unsigned long lastHttpsOperationTime = 0;
const unsigned long HTTPS_MIN_INTERVAL = 500;            // was 2 seconds between any HTTPS calls becasue core0 tiny system stacks (ipc0 = 1024 bytes) , not sure it was necessary
const unsigned long CONFIG_SNAPSHOT_INTERVAL = 2400000;  //86400000;  // 24 hours  //2400000;  //40 minutes     300000 is 5 mins
//unsigned long SENSOR_UPLOAD_INTERVAL = 300000;            // 300000 is 5 mins
unsigned long SENSOR_UPLOAD_INTERVAL = 30000;        // 30 seconds for testing
const unsigned long BUFFER_UPLOAD_INTERVAL = 13000;  // 13 seconds
const unsigned long RESTART_INTERVAL = 43200000;     //12 hours    // 4 hour MAINTENANCE INTERVAL 14400000
//const unsigned long RESTART_INTERVAL= 1800000; // 30 mins

//Configuration Snapshot Stuff
char *configPayloadBuffer;

unsigned long lastConfigSnapshotTime = 0;
unsigned long lastConfigSnapshotAttempt = 0;
int configSnapshotRetryCount = 0;
const unsigned long CONFIG_RETRY_DELAYS[] = {
  5000,     // 5 sec
  30000,    // 30 sec
  60000,    // 1 min
  70000,    // 70 sec
  80000,    // 80 sec
  90000,    // 90 sec
  300000,   // 5 min
  600000,   // 10 min
  900000,   // 15 min
  990000,   // 16.5 min
  1800000,  // 30 min
  3600000,  // 1 hour
  7200000,  // 2 hours
  18000000  // 5 hours
};
const int CONFIG_MAX_RETRIES = sizeof(CONFIG_RETRY_DELAYS) / sizeof(CONFIG_RETRY_DELAYS[0]);

uint64_t chipid;
int firmwareVersionInt = 0;
uint32_t deviceIdUpper = 0;  // Change from int to uint32_t (matches the cast)
uint32_t deviceIdLower = 0;  // Change from int to uint32_t (matches the cast)
char device_id_hex[17];

int currentPartitionType = 0;  // 0=factory, 1=ota_0

// Streaming tar extraction state (no gzip decompression)
struct StreamingExtractor {
  // Tar parsing state
  bool inTarHeader;
  uint8_t tarHeader[512];
  size_t tarHeaderPos;
  String currentFileName;
  size_t currentFileSize;
  size_t currentFilePos;
  bool isCurrentFileFirmware;

  //Padding state for proper streaming
  bool inPadding;
  size_t paddingRemaining;

  // OTA state
  esp_ota_handle_t otaHandle;
  const esp_partition_t *otaPartition;
  bool otaStarted;

  // LittleFS state
  File currentWebFile;
  bool prodFSMounted;

  // Hash verification
  mbedtls_md_context_t hashCtx;
  bool hashStarted;
};

#define BMP3_ADDR 0x76
BMP388_DEV bmp388;

//WIFI STUFF
//these will be the custom network created by the user in AP mode
String esp32_ap_ssid = "ALTERNATOR_WIFI";  // Default SSID
const char *AP_SSID_FILE = "/apssid.txt";  // File to store custom SSID
// WiFi connection timeout when trying to avoid Access Point Mode (and connect to ship's wifi on reboot)
const unsigned long WIFI_TIMEOUT = 20000;  // 20 seconds
const char *AP_PASSWORD_FILE = "/appass.txt";
String esp32_ap_password = "alternator123";  // Default ESP32 AP password
//WiFi Reconnection Management with Signal Strength Awareness
struct WiFiReconnection {
  unsigned long lastAttempt = 0;
  int attemptCount = 0;
  unsigned long currentInterval = 2000;      // Start at 2 seconds
  const unsigned long minInterval = 2000;    // 2 seconds minimum
  const unsigned long maxInterval = 300000;  // 5 minutes maximum
  const int maxAttempts = 20;                // Give up after 20 attempts
  bool giveUpMode = false;
  int lastSignalStrength = -999;       // Track signal strength when connected
  const int minSignalThreshold = -80;  // Don't retry aggressively if signal was weaker than -80 dBm
} wifiRecon;

//Security
char requiredPassword[32] = "admin";  // Max password length = 31 chars     Password for access to change settings from browser
char storedPasswordHash[65] = { 0 };

// ===== HEAP MONITORING =====
int rawFreeHeap = 0;      // in bytes
int FreeHeap = 0;         // in KB
int MinFreeHeap = 0;      // in KB
int FreeInternalRam = 0;  // in KB
int Heapfrag = 0;         // 0–100 %, integer only
uint32_t FreePSRAM = 0;   // KB of free PSRAM
size_t TotalInternalRam, LargestInternalBlock, TotalPSRAM;


// ===== TASK STACK MONITORING =====
const int MAX_TASKS = 50;  // Grossly big enough!
TaskStatus_t *taskArray;
// Optional: reuse these across calls if needed
int numTasks = 0;
int tasksCaptured = 0;
int stackBytes = 0;
int core = 0;

//CPU
// ===== CPU LOAD TRACKING =====
unsigned long lastIdle0Time = 0;  // Previous IDLE0 task runtime counter
unsigned long lastIdle1Time = 0;  // Previous IDLE1 task runtime counter
unsigned long lastCheckTime = 0;  // Last time CPU load was measured
int cpuLoadCore0 = 0;             // CPU load percentage for Core 0
int cpuLoadCore1 = 0;             // CPU load percentage for Core 1
int cpuLoadCore0Max = 0;
int cpuLoadCore1Max = 0;

//More health monitoring
// Session and health tracking
unsigned long sessionStartTime = 0;
// Duration of last WiFi session (minutes, persistent)
unsigned long LastSessionDuration = 0;     // minutes, persistent
unsigned long CurrentSessionDuration = 0;  //minutes
int LastSessionMaxLoopTime = 0;            // milliseconds, persistent
int MaxLoopTime = 0;                       // not displayed on client, but available here
int lastSessionMinHeap = 999999;           // KB, persistent
int wifiReconnectsTotal = 0;               // persistent, the one i actually use
// Simple WiFi disconnect tracking
int wifiDisconnectCount = 0;
int LastResetReason;         // why the ESP32 restarted most recently
int ancientResetReason = 0;  // why the ESP32 restarted 2 sessions ago
int totalPowerCycles = 0;    // Total number of power cycles (persistent)

// These are health variables for the TempTask (digital temperature measurement)
unsigned long lastTempTaskHeartbeat = 0;
bool tempTaskHealthy = true;
bool tempTaskAlarm = false;   // Set by checkTempTaskHealth() — triggers buzzer regardless of AlarmActivate
const unsigned long TEMP_TASK_TIMEOUT = 20000;  // 20 seconds

//Cloud Upload Stuff
static const char *UPLOAD_HOST = "qnbekuaoweuteylitzvo.supabase.co";
static const uint16_t UPLOAD_PORT = 443;
static const uint32_t UPLOAD_CONNECT_TIMEOUT_MS = 5000;
static const uint32_t UPLOAD_HANDSHAKE_TIMEOUT_MS = 5000;
static const uint32_t UPLOAD_READ_TIMEOUT_MS = 8000;
static const uint32_t UPLOAD_GLOBAL_TIMEOUT_MS = 14000;
//snapshot
const char *host = "qnbekuaoweuteylitzvo.supabase.co";
const int port = 443;
const uint32_t CONNECT_TIMEOUT = 5000;    // ms
const uint32_t HANDSHAKE_TIMEOUT = 5000;  // ms
const uint32_t READ_TIMEOUT = 8000;       // ms
const uint32_t GLOBAL_TIMEOUT = 14000;    // ms (must stay < WDT)


//Console
unsigned long lastConsoleMessageTime = 0;
const unsigned long CONSOLE_MESSAGE_INTERVAL = 700;  //
// Fixed-size circular buffer:
struct ConsoleMessage {
  char message[128];  // Fixed size instead of dynamic String
  unsigned long timestamp;
};
#define CONSOLE_QUEUE_SIZE 10  // Reduced from potentially unlimited vector
ConsoleMessage *consoleQueue;
volatile int consoleHead = 0;
volatile int consoleTail = 0;
volatile int consoleCount = 0;
static portMUX_TYPE consoleMux = portMUX_INITIALIZER_UNLOCKED;


// DNS Server for captive portal
DNSServer dnsServer;
const byte DNS_PORT = 53;

enum DeviceMode {
  MODE_CONFIG,  // WiFi configuration (minimal - just DNS + config page)
  MODE_AP,      // Access Point with full functionality
  MODE_CLIENT   // Client mode connected to ship's WiFi
};
DeviceMode currentMode = MODE_CLIENT;  // gets overwritten immediately, exact mode here doesn't matter

//little fs monitor
bool littleFSMounted = false;

int INADisconnected = 0;
int WifiHeartBeat = 0;
static uint32_t stateRevision = 0;  // used for UX in client interface improvement
int VeTime = 0;                     // rolling
int VeTime2 = 0;                    //session

int SendWifiTime = 0;
//Needed for ALERT pin on INA228 to shut off Field and delay 10 seconds before retrying if fault condition is gone
bool inaOvervoltageLatched = false;
unsigned long inaOvervoltageTime = 0;
uint32_t inaOvervoltageClearedMs = 0;  // timestamp when INA OV latch last cleared; used to suppress disagreement check
// How long after INA OV latch clears to keep suppressing the ADS/INA
// voltage disagreement check. Long enough to cover field collapse + both
// sensors resettling. Must be longer than INA228 averaged update rate.
const uint32_t INA_OV_DISAGREE_SUPPRESS_MS = 10000;  // 10 seconds


// ── Input filter (disturbance rejection) ─────────────────────────────────
// IIR EMA applied to CH0 (BatteryV), CH1 (MeasuredAmps), CH2 (RPM).
// α = dt / (TC + dt), computed per-sample from actual elapsed time so
// variable loop cadence is handled correctly without a fixed assumption.
// _filtered variables are the smoothed outputs; originals are unchanged.
// Safeties continue to read originals. Control loops will migrate to
// _filtered in a subsequent pass via getBatteryVoltage() / getTargetAmps().
// Thermistor (CH3) is left on its own filter inside tempPID_tick().
float InputFilterTC = 100.0f;  // ms — web-configurable, LittleFS-backed
float BatteryV_filtered = 0.0f;
float MeasuredAmps_filtered = 0.0f;
float RPM_filtered = 0.0f;

// ── SystemID — plant delay measurement ───────────────────────────────────
// Step test: baseline → 3× (duty up / duty down) → post-process.
// Samples stored in PSRAM. Buffer allocated on first test run, never freed.
// Results populate the JS popup and optionally update InputFilterTC in flash.
// Only legal in SYS_MODE_AUTO; systemID_tick() enforces this.
float SystemIDStepAmplitude = 15.0f;  // % duty step — will be web-configurable later

volatile bool systemIDRequested = false;  // set true by UI handler to trigger a test run
bool systemIDActive = false;              // true while test is in progress
bool systemIDResultsReady = false;        // set true when post-processing is complete

float systemIDRiseDelay_ms[3] = { 0.0f, 0.0f, 0.0f };  // rising-step delays, ms
float systemIDFallDelay_ms[3] = { 0.0f, 0.0f, 0.0f };  // falling-step delays, ms
float systemIDRiseAvg_ms = 0.0f;
float systemIDFallAvg_ms = 0.0f;

struct SystemIDSample {
  uint32_t ts;     // millis() at sample time
  float duty_cmd;  // duty commanded this tick (%)
  float amps;      // MeasuredAmps at sample time (raw, unfiltered)
};
// 15000 × 12 bytes = 180 KB in PSRAM. Covers TC up to ~714 ms at 5 ms CH1 cadence.
static const int SYSID_BUF_SIZE = 15000;
SystemIDSample *sysIDBuffer = nullptr;  // ps_malloc'd on first test run
int sysIDSampleCount = 0;

// ── SystemID pre-test state snapshot ─────────────────────────────────────────
// Captured at test start; restored on clean exit so the user returns to
// whatever mode they were in (AUTO, MANUAL, CV, bulk, etc.) without a jump.
struct SystemIDResumeState {
  bool valid = false;
int sysMode = 0;
  float setpointLimited_snap = 0.0f;
  float lastAppliedDuty_snap = 0.0f;  // informational only; NOT replayed as a command
  float cv_I_snap = 0.0f;
  bool voltageControlActive_snap = false;
};
static SystemIDResumeState g_sysIDResume;

// Weather Mode Global Variables (add with your other globals)
float UVToday = 0.0;
float UVTomorrow = 0.0;
float UVDay2 = 0.0;
int SolarWatts = 660;                      // nominal solar array power in watts
float pKwHrToday;                          // predicted solar output in kw*hr for the rest of today, as of the last weather forecast download
float pKwHrTomorrow;                       // predicted solar output in kw*hr for all of tomorrow
float pKwHr2days;                          // predicted solar output in kw*hr for 2 days from now
const float MJ_TO_KWH_CONVERSION = 0.278;  // MJ/m²/day to kWh/m²/day
const float STC_IRRADIANCE = 1000.0f;      // Standard Test Conditions W/m²
float performanceRatio = .75;              // Real-world performance losses (typically 0.75)


unsigned long weatherLastUpdate = 0;
int weatherDataValid = 0;  // 0=false, 1=true
char weatherLastError[64];
int weatherHttpResponseCode = 0;

// Weather Mode Settings (add to your settings section)
int weatherModeEnabled = 0;            // 0=disabled, 1=enabled
float UVThresholdHigh = 2.1;           // UV index above this = high solar expected (kWh)
int WeatherUpdateInterval = 21600000;  // Update every 6 hours (in milliseconds)
int WeatherTimeoutMs = 10000;          // HTTP timeout in milliseconds

// Weather Mode Status Variables
int currentWeatherMode = 0;           // 0=normal, 1=high solar, 2=low solar
unsigned long nextWeatherUpdate = 0;  // When next update is due

//Input Settings
float uTargetAmps = 3;                           // the one that gets used as the real target
float FloatVoltage = 13.4;                       // self-explanatory
float BulkVoltage = 14.5;                        // this could have been called Target Bulk Voltage to be more clear
float ChargingVoltageTarget = 0;                 // This becomes active target
float VoltageHardwareLimit = BulkVoltage + 0.1;  // could make this a setting later, but this should be decently safe
bool inBulkStage = true;

enum ChargeStageDisplayCode : uint8_t {
  CHARGE_STAGE_NONE = 0,
  CHARGE_STAGE_BULK = 1,
  CHARGE_STAGE_ABSORPTION = 2,
  CHARGE_STAGE_FLOAT = 3,
  CHARGE_STAGE_MANUAL = 4,
  CHARGE_STAGE_MAINTAIN = 5,
  CHARGE_STAGE_TARGET_V = 6,
  CHARGE_STAGE_IDLE = 7
};
uint8_t chargeStageDisplay = 0;

// ====== Charger algorithm add-ons (no safeties here) ======

// SoC availability (you'll decide when to assert this)
bool socInfoAvailable = false;

// Tail-current based bulk completion
float TailCurrent_A = 5.0f;
uint32_t bulkVoltageHoldTimer = 0;  // millis() timestamp, 0 = inactive

// Float->Bulk (rebulk) based on sag + debounce
float RebulkVoltage = 13.2f;
uint32_t rebulkDebounceTime = 60UL * 1000UL;  // ms
uint32_t rebulkTimer = 0;                     // millis() timestamp, 0 = inactive

// Optional: don't allow immediate rebulk right after entering float
uint32_t MinFloatTime = 5UL * 60UL * 1000UL;  // ms

// SoC-based refinements (same scaling as SOC_percent: percent*100)
int SOC_BlockRebulk_percent = 95.00;
int SOC_AllowRebulk_percent = 94.00;

uint32_t FLOAT_DURATION = 12 * 3600;  // 12 hours in seconds
uint32_t floatStartTime = 0;

float RebulkCurrent_A = 5.0f;  // net discharge current threshold to trigger rebulk
bool inIdleStage = false;      // true when UseFloat=0 and absorption complete, waiting for rebulk
int UseFloat = 1;              // 1 = enter float after absorption, 0 = idle until rebulk criteria met


// Absorption stage
bool inAbsorptionStage = false;
uint32_t absorptionStartTime = 0;
float AbsorptionVoltage = 14.0f;
uint32_t AbsorptionTimeoutMs = 1200000UL;   //
uint32_t absorptionCompleteTime = 30000UL;  // tail current hold before float
uint32_t absorptionTailTimer = 0;
uint32_t bulkVoltageHoldMs = 250;  // time at bulk voltage before entering absorption

float FieldAdjustmentInterval = 50;  // The regulator field output is updated once every this many milliseconds
float TemperatureLimitF = 150;       // the offset appears to be +40 to +50 to get true max alternator external metal temp, depending on temp sensor installation, so 150 here will produce a metal temp ~200F
int ManualFieldToggle = 1;           // set to 1 to enable manual control of regulator field output, helpful for debugging
int SwitchControlOverride = 1;       // set to 1 for web interface switches to override physical switch panel
int MaintainMode = 0;                // Set to 1 to target 0 amps at battery
int TargetVoltageMode = 0;
float TargetVoltageSetpoint = 12.6f;
int OnOff = 0;             // 0 is charger off, 1 is charger On (corresponds to Alternator Enable in Basic Settings)
int Ignition = 0;          // Digital Input      NEED THIS TO HAVE WIFI ON , FOR NOW
int IgnitionOverride = 1;  // to fake the ignition signal w/ software
int HiLow = 1;             // 0 will be a low setting, 1 a high setting
int AmpSrc = 0;            // OBSOLETE, NEEDS REMOVING
int LimpHome = 0;          // 1 will set to limp home mode, whatever that gets set up to be
int resolution = 12;       // for OneWire temp sensor measurement
int VeData = 0;            // Set to 1 if VE serial data exists
int NMEA0183Data = 0;      // Set to 1 if NMEA serial data exists doesn't do anything yet
// ── HARD OVER-CURRENT PROTECTION ─────────────────────────────
float HardOCTripAmps = 180.0f;       // user-adjustable, persisted in LittleFS
uint32_t HardOCDebounceMs = 20;      // user-adjustable, persisted in LittleFS
uint32_t hardOCStartMs = 0;
//Field PWM stuff
const int pwmPin = 14;  // field PWM pin # (was 32)
//const int pwmChannel = 0;      //0–7 available for high-speed PWM  (ESP32)
const int pwmResolution = 12;          // Error = +0.010%    PWM Resolution = ±0.024% (1/4096)
float SwitchingFrequency = 100;        // Field switching frequency (doesn't much matter, avoid human hearing range is nice depending on install location)
const int MIN_SAFE_FREQUENCY = 50;     // Above most audible issues // set to 2000 later
const int MAX_SAFE_FREQUENCY = 25000;  // Below core loss and EMI concerns
float MaxDuty = 99.0;
float MinDuty = 1.0;       //33 works on my boat to make sure RPM is always present, may need to make funciton of RPM?? Later
int ManualDutyTarget = 4;  // example manual override value
int InvertAltAmps = 0;     // change sign of alternator amp reading
int InvertBattAmps = 0;    // change sign of battery amp reading
uint32_t Freq = 0;         // ESP32 switching Frequency in case we want to report it for debugging

//Variables to store measurements
float ShuntVoltage_mV;                        // Battery shunt voltage from INA228
float Bcur;                                   // battery shunt current from INA228
float targetCurrent;                          // This is used in the field adjustment loop, gets set to the desired source of current info (ie battery shunt, alt hall sensor, victron, etc.)
float IBV;                                    // Ina 228 battery voltage
float IBVMax = 6;                             // used to track maximum battery voltage    TEMPORARY TROUBLESHOOTING
float dutyCycle;                              // Field outout %--- this is just what's transmitted over Wifi (case sensitive)
float FieldResistance = 2;                    // Field resistance in Ohms usually between 2 and 6 Ω, changes 10-20% with temp
float vvout;                                  // Calculated field volts (approximate)
float iiout;                                  // Calculated field amps (approximate)
volatile float AlternatorTemperatureF = NAN;  // alternator temperature
float MaxAlternatorTemperatureF = 0;          // maximum alternator temperature
// === Thermistor Stuff
float R_fixed = 10000.0;               // Series resistor in ohms
float Beta = 3950.0;                   // Thermistor Beta constant (e.g. 3950K)
float R0 = 10000.0;                    // Thermistor resistance at T0
float T0_C = 25.0;                     // Reference temp in Celsius
int TempSource = 0;                    // 0 for OneWire default, 1 for Thermistor
int temperatureThermistor = -99;       // thermistor reading
float MaxTemperatureThermistor = -99;  // maximum thermistor temperature (on alternator)
float TempToUse = NAN;                 // gets set to temperatureThermistor or AlternatorTemperatureF
TaskHandle_t tempTaskHandle = NULL;    // make a separate cpu task for temp reading (because it was so slow before making it non-blocking.  Now it's non-blocking so this is just moot leftover)
float VictronVoltage = 0;              // battery V reading from VeDirect
float VictronCurrent = 0;              // battery Current (careful, can also be solar current if hooked up to solar charge controller not BMV712)
float HeadingNMEA = 0;                 // Just here to test NMEA functionality
float COGNMEA = 0;                     // Course Over Ground from NMEA2K (degrees)
float SOGNMEA = 0;                     // Speed Over Ground from NMEA2K (knots)
double LatitudeNMEA = 0;               // from own ship AIS FIX LATER
double LongitudeNMEA = 0;              // from own ship AIS FIX LATER
float ApparentWindSpeedNMEA = 0;       // Apparent wind speed from NMEA2K (knots)
float ApparentWindAngleNMEA = 0;       // Apparent wind angle from NMEA2K (degrees)
int NMEA2KData = 0;                    //
int NMEA2KVerbose = 0;                 // print stuff to serial monitor or not
int SatelliteCountNMEA = 0;            // Number of satellites
#define GPS_SMOOTHING_SAMPLES 5
double latBuffer[GPS_SMOOTHING_SAMPLES] = { 0 };
double lonBuffer[GPS_SMOOTHING_SAMPLES] = { 0 };
int gpsBufferIndex = 0;
int gpsBufferCount = 0;

// Calculated navigation metrics
float TrueWindSpeedNMEA = NAN;  // True wind speed (knots)
float TrueWindAngleNMEA = NAN;  // True wind angle (degrees)
float LeewayNMEA = NAN;         // Leeway angle (degrees, + = drift to starboard)
float VMGNMEA = NAN;            // Velocity made good toward target (knots)
float VMGTargetBearing = -1;    // User-set target bearing for VMG (-1 = not set)
// Duty cycle tracking
//float IgnitionDutyCycle = 0;       // % of time ignition is on
//float EngineRunningDutyCycle = 0;  // % of time engine RPM > 100
//a setting
int VMGUseTrueWind = 0;  // 0=use manual bearing, 1=track true wind angle

// ADS1115
int16_t Raw = 0;
float Channel0V, Channel1V, Channel2V, Channel3V;
float BatteryV, MeasuredAmps, RPM;  //Readings from ADS1115
float MeasuredAmpsMax;              // used to track maximum alternator output
float RPMMax;                       // used to track maximum RPM
int ADS1115Disconnected = 0;
volatile bool battVFreshFlag = false;

// === Cloud Features Variables (and some others later added to LiveData) ===
// Energy (Wh)
float ChargedEnergy_AllTime = 0.0f;            // Wh (lifetime total produced from all sources)
float DischargedEnergy_AllTime = 0.0f;         // Wh (lifetime)
float SolarChargedEnergy = 0.0f;               // Solar Wh (session)
float SolarChargedEnergy_AllTime = 0.0f;       // Solar Wh (lifetime)
float AlternatorChargedEnergy_AllTime = 0.0f;  // Alternator Wh (lifetime)
// Fuel (L)
float EngineFuelUsed = 0.0f;              // L (session)
float EngineFuelUsed_AllTime = 0.0f;      // L (lifetime)
float AlternatorFuelUsed_AllTime = 0.0f;  // L (lifetime)
// Engine fuel consumption table - RPM vs. gallons per hour
// Fuel table defaults
#define FUEL_TABLE_SIZE 10
float fuelTableRPM[FUEL_TABLE_SIZE] = { 0, 500, 1000, 1500, 2000, 2500, 3000, 3500, 4000, 4500 };
float fuelTableGPH[FUEL_TABLE_SIZE] = { 0.0, 0.3, 0.5, 0.8, 1.2, 1.8, 2.5, 3.5, 5.0, 7.0 };
float defaultFuelRPMValues[FUEL_TABLE_SIZE] = { 0, 500, 1000, 1500, 2000, 2500, 3000, 3500, 4000, 4500 };
float defaultFuelGPHValues[FUEL_TABLE_SIZE] = { 0.0, 0.3, 0.5, 0.8, 1.2, 1.8, 2.5, 3.5, 5.0, 7.0 };
// Voltage (V)
float PeakVoltage_AllTime = 0.0f;  // V (lifetime peak)
float MinVoltage = 99.0f;          // V (session min)
float MinVoltage_AllTime = 99.0f;  // V (lifetime min)
// Runtime (hours)
float EngineRunTime_AllTime = 0.0f;  // hours (lifetime)
// Charge cycles (count)
float ChargeCycles = 0;          // count (session)
float ChargeCycles_AllTime = 0;  // count (lifetime)
// Travel (nm, kts)
float TotalDistance = 0.0f;                      // nm (session)
float TotalDistance_AllTime = 0.0f;              // nm (lifetime)
float MaxSpeed = 0.0f;                           // kts (session)
float MaxSpeed_AllTime = 0.0f;                   // kts (lifetime)
float AvgSOC = 0.0f;                             // % (session average state of charge, not currently used for anything)
float AvgSOC_AllTime = 0.0f;                     // % (lifetime average state of charge)
float AvgSpeed_AllTime = 0.0f;                   // kts (lifetime average speed)
float AvgSpeed = 0.0f;                           // Session average speed (kts) - random test value
float AlternatorOnTime_AllTime = 0.0f;           // Lifetime alternator on time (minutes) - random test
int EngineCycles_AllTime = 0;                    // Lifetime engine cycles - random test
float MaxAlternatorTemperatureF_AllTime = 0.0f;  // Lifetime max alternator temp (°F) - random test
float MaxTemperatureThermistor_AllTime = -99;    // Lifetime max thermistor temp (°C) - random test
float MeasuredAmpsMax_AllTime = 0.0f;            // Lifetime max alternator output (A) - random test
float RPMMax_AllTime = 0.0f;                     // Lifetime max RPM - random test
// SOC tracking accumulators (must be global for NVS save/load)
float socAccumulator_AllTime = 0.0f;
unsigned long totalSocSampleTime_AllTime = 0;
unsigned long totalVoltageSampleTime_AllTime = 0;
unsigned long totalSpeedSampleTime_AllTime = 0;
float voltageAccumulator_AllTime = 0.0f;
float speedAccumulator_AllTime = 0.0f;  //
float AvgVoltage_AllTime = 0.0f;


//supabase authentications stuff
String authToken = "";      // Stored auth token
bool isRegistered = false;  // Registration status


// CLOUD FEATURES - STATIC BUFFERS// For ESP32-S3 with 16GB and OPI PSRAM
const int PAYLOAD_BUFFER_SIZE = 4096;
const int CONFIG_PAYLOAD_SIZE = 8192;

char lastUploadedFilePath[128] = "";  // Track which file to delete after successful upload


char *tempBuffer;

const int FILENAME_BUFFER_SIZE = 64;
const int TIMESTAMP_BUFFER_SIZE = 32;
const int MESSAGE_BUFFER_SIZE = 128;
char *payloadBuffer;
char *filenameBuffer;
char *timestampBuffer;
char *messageBuffer;
// Time management
bool timeIsSynced = false;
time_t timeBase = 0;               // Unix timestamp at last sync
unsigned long timeBaseMillis = 0;  // millis() at last sync
unsigned long lastTimeSyncAttempt = 0;
const unsigned long TIME_SYNC_INTERVAL = 43200000;  // 12 hour , was previously 1 hour during testing and worked
enum TimeSource { TIME_NONE,
                  TIME_GPS,
                  TIME_NTP,
                  TIME_MILLIS };
TimeSource currentTimeSource = TIME_NONE;

// Sensor aggregation window
struct SensorWindow {
  // Battery voltage
  int32_t battVolt_min = 999900;
  int32_t battVolt_max = 0;
  int64_t battVolt_area_v_us = 0;  // Changed from _sum
  uint64_t battVolt_valid_us = 0;  // New: microseconds when valid

  // Battery current
  int32_t battCurr_min = 999900;
  int32_t battCurr_max = -999900;
  int64_t battCurr_area_v_us = 0;
  uint64_t battCurr_valid_us = 0;

  // Alternator current
  int32_t altCurr_min = 999900;
  int32_t altCurr_max = 0;
  int64_t altCurr_area_v_us = 0;
  uint64_t altCurr_valid_us = 0;

  // Victron current
  int32_t victronCurr_min = 999900;
  int32_t victronCurr_max = -999900;
  int64_t victronCurr_area_v_us = 0;
  uint64_t victronCurr_valid_us = 0;

  // SOC
  int32_t soc_min = 999900;
  int32_t soc_max = 0;
  int64_t soc_area_v_us = 0;
  uint64_t soc_valid_us = 0;

  // Barometric pressure
  int32_t baro_min = 999900;
  int32_t baro_max = 0;
  int64_t baro_area_v_us = 0;
  uint64_t baro_valid_us = 0;

  // Alternator temperature
  int32_t altTemp_min = 999900;
  int32_t altTemp_max = -999900;
  int64_t altTemp_area_v_us = 0;
  uint64_t altTemp_valid_us = 0;

  // Thermistor temperature
  int32_t tempTherm_min = 999900;
  int32_t tempTherm_max = -999900;
  int64_t tempTherm_area_v_us = 0;
  uint64_t tempTherm_valid_us = 0;

  // Ambient temperature
  int32_t ambTemp_min = 999900;
  int32_t ambTemp_max = -999900;
  int64_t ambTemp_area_v_us = 0;
  uint64_t ambTemp_valid_us = 0;

  // RPM
  int32_t rpm_min = 999900;
  int32_t rpm_max = 0;
  int64_t rpm_area_v_us = 0;
  uint64_t rpm_valid_us = 0;

  // WiFi strength
  int32_t wifiStr_min = 999900;
  int32_t wifiStr_max = -999900;
  int64_t wifiStr_area_v_us = 0;
  uint64_t wifiStr_valid_us = 0;

  // Duty cycle
  int32_t dutyCycle_min = 999900;
  int32_t dutyCycle_max = 0;
  int64_t dutyCycle_area_v_us = 0;
  uint64_t dutyCycle_valid_us = 0;

  // Dynamic alternator zero
  int32_t altZero_min = 999900;
  int32_t altZero_max = -999900;
  int64_t altZero_area_v_us = 0;
  uint64_t altZero_valid_us = 0;

  // Speed over ground
  int32_t sog_min = 999900;
  int32_t sog_max = 0;
  int64_t sog_area_v_us = 0;
  uint64_t sog_valid_us = 0;

  // Course over ground
  int32_t cog_min = 999900;
  int32_t cog_max = 0;
  int64_t cog_area_v_us = 0;
  uint64_t cog_valid_us = 0;

  // Heading
  int32_t heading_min = 999900;
  int32_t heading_max = 0;
  int64_t heading_area_v_us = 0;
  uint64_t heading_valid_us = 0;

  // Apparent wind speed
  int32_t aws_min = 999900;
  int32_t aws_max = 0;
  int64_t aws_area_v_us = 0;
  uint64_t aws_valid_us = 0;

  // Apparent wind angle
  int32_t awa_min = 999900;
  int32_t awa_max = 0;
  int64_t awa_area_v_us = 0;
  uint64_t awa_valid_us = 0;

  // True wind speed
  int32_t tws_min = 999900;
  int32_t tws_max = 0;
  int64_t tws_area_v_us = 0;
  uint64_t tws_valid_us = 0;

  // True wind angle
  int32_t twa_min = 999900;
  int32_t twa_max = 0;
  int64_t twa_area_v_us = 0;
  uint64_t twa_valid_us = 0;

  // Leeway
  int32_t leeway_min = 999900;
  int32_t leeway_max = -999900;
  int64_t leeway_area_v_us = 0;
  uint64_t leeway_valid_us = 0;

  // VMG
  int32_t vmg_min = 999900;
  int32_t vmg_max = -999900;
  int64_t vmg_area_v_us = 0;
  uint64_t vmg_valid_us = 0;

  int32_t uTargetAmps_min = 999900;
  int32_t uTargetAmps_max = -999900;
  int64_t uTargetAmps_area_v_us = 0;
  uint64_t uTargetAmps_valid_us = 0;

  int32_t tempMargin_min = 999900;
  int32_t tempMargin_max = -999900;
  int64_t tempMargin_area_v_us = 0;
  uint64_t tempMargin_valid_us = 0;

  // GPS
  double lat_current;  // Most recent smoothed latitude
  double lon_current;  // Most recent smoothed longitude

  // Timing - removed global sampleCount
  uint64_t lastUpdateTime_us = 0;  // Track last update for time-weighted averaging
  unsigned long windowStartTime = 0;
};
SensorWindow *currentWindow = nullptr;  // allocated to PSRAM in init

struct ImuWindow {  // moved to PSRAM to save internal SRAM
  // Raw accel signals (scaled by 1000: 1.234g → 1234)
  int32_t accel_x_min, accel_x_max;
  int64_t accel_x_area_v_us;
  uint64_t accel_x_valid_us;

  int32_t accel_y_min, accel_y_max;
  int64_t accel_y_area_v_us;
  uint64_t accel_y_valid_us;

  int32_t accel_z_min, accel_z_max;
  int64_t accel_z_area_v_us;
  uint64_t accel_z_valid_us;

  // Raw gyro signals (scaled by 100: 12.34 dps → 1234)
  int32_t gyro_x_min, gyro_x_max;
  int64_t gyro_x_area_v_us;
  uint64_t gyro_x_valid_us;

  int32_t gyro_y_min, gyro_y_max;
  int64_t gyro_y_area_v_us;
  uint64_t gyro_y_valid_us;

  int32_t gyro_z_min, gyro_z_max;
  int64_t gyro_z_area_v_us;
  uint64_t gyro_z_valid_us;

  // Calculated metrics - heel/pitch (scaled by 100: 12.34° → 1234)
  int32_t heel_min, heel_max;
  int64_t heel_area_v_us;
  uint64_t heel_valid_us;

  int32_t pitch_min, pitch_max;
  int64_t pitch_area_v_us;
  uint64_t pitch_valid_us;

  // Calculated metrics - accelerations (scaled by 1000)
  int32_t vertical_accel_min, vertical_accel_max;
  int64_t vertical_accel_area_v_us;
  uint64_t vertical_accel_valid_us;

  int32_t total_accel_min, total_accel_max;
  int64_t total_accel_area_v_us;
  uint64_t total_accel_valid_us;

  // High-frequency vibration energy (scaled by 1000000: μg² RMS)
  int32_t hf_vibe_min, hf_vibe_max;
  int64_t hf_vibe_area_v_us;
  uint64_t hf_vibe_valid_us;

  // 60s rolling window metrics (updated at ~1Hz, stored as current value)
  int32_t heel_change_60s;      // Scaled by 100
  int32_t heel_deviation_60s;   // Scaled by 100
  int32_t pitch_change_60s;     // Scaled by 100
  int32_t pitch_deviation_60s;  // Scaled by 100

  // Wave period (scaled by 1000: 6.5s → 6500, or -1000 if invalid)
  int32_t wave_period;

  // Period counters (reset each window)
  uint32_t slam_count;
  int32_t slam_peak_max;  // Scaled by 1000 (millig's)

  // Timing
  uint64_t lastUpdateTime_us;
  uint64_t lastGyroUpdateTime_us;
  unsigned long windowStartTime;
};
ImuWindow *imuWindow = nullptr;

// Upload timing
unsigned long lastSensorUploadTime = 0;
bool sensorUploadInProgress = false;

volatile bool core0Busy = false;  // Guards Core 0 CPU intensive ops

// Local buffer config
const char *SENSOR_BUFFER_DIR = "/sensor_buffer";
const int MAX_BUFFERED_RECORDS = 900;  // ~1.75 MB in 3.8 MB userdata partition that also has at least 0.5mb of web files (as of Nov 2025)
//The above translates to
int bufferedRecordCount = 0;
unsigned long lastBufferUploadAttempt = 0;

//temp debug of cloud uploads
int lastHttpStatus = -1;
bool lastUploadWasBuffered = false;
unsigned long lastBufferStatusPrint = 0;
const unsigned long BUFFER_STATUS_INTERVAL = 60000;  // 60 seconds


// Battery SOC Monitoring Variables
int BatteryCapacity_Ah = 200;         // Battery capacity in Amp-hours
int SOC_percent = 5000;               // State of Charge percentage (0-100) but have to multiply by 100 for annoying reasons, but go with it
int ManualSOCPoint = 25;              // Used to set it manually
int CoulombCount_Ah_scaled = 7500;    // Current energy in battery (Ah × 100 for precision)
float PeukertRatedCurrent_A = 15.0f;  // Standard discharge rate for Peukert (C/20), will be calculated from capacity
bool FullChargeDetected = false;      // Flag for full charge detection
unsigned long FullChargeTimer = 600;  // Timer for full charge detection, 10 minutes
// Timing variables
unsigned long currentTime = 0;
unsigned long elapsedMillis = 0;
unsigned long lastSOCUpdateTime = 0;      // Last time SOC was updated
unsigned long lastEngineMonitorTime = 0;  // Last time engine metrics were updated
unsigned long lastDataSaveTime = 0;       // Last time data was saved to LittleFS
int SOCUpdateInterval = 2000;             // Update SOC every 2 seconds.   Don't make this smaller than 1 without study

//NVS Stuff
unsigned long lastNVSSaveTime = 0;
const unsigned long NVS_SAVE_INTERVAL = 120000;  // 2 minutes

/*
NVS lifetime analysis (ESP32-S3-WROOM-1U-N16R8, 16 MB flash)
Partition: nvs @ 0x9000, size = 0x5000 (20 KB = 5 pages × 4 KB)
Effective pages for wear leveling: ~4 (1 reserved for GC)
Entries/page ≈ 126 (32 B each) → total ~504 entries capacity before GC
Flash endurance: ≥100,000 erase cycles per page

Save cadence = 2 minutes → 720 saves/day
Case A (5 keys change/save): 3,600 entries/day → 3,600/504 ≈ 7.14 GC cycles/day
 → 7.14 erases/page/day → lifetime ≈ 100,000/7.14 ≈ 14,000 days ≈ 38 years
Case B (21 keys change/save): 15,120 entries/day → 30 GC cycles/day
 → 30 erases/page/day → lifetime ≈ 3,333 days ≈ 9 years

Conclusion: At 2-minute saves with change-detection, NVS wear is well within
multi-year (≈10–40 yr) service life even in worst case.
*/


/*
UPDATE: IMU NVS additions (estimate +3-8 keys changing per save):
  Always changing (if IMU enabled):
    None - IMU lifetime counters only write on events or threshold changes
  
  Event-driven writes (outside normal save cycle):
    imu_capsize_count, imu_pitchpole_count - immediate write on event (rare)
  
  Periodic/threshold writes (within saveNVSData):
    imu_slam_count_lifetime - only if slams occurred (variable)
    imu_hours_gentle/mod/rough/extr - once per hour when bucket transitions
    VibEnvelope blob - only when envelope changes >threshold (~1/hour)
  
  Updated Case B estimate with IMU: 21 + 5 = 26 keys → 18,720 entries/day
    → 37 GC cycles/day → lifetime ≈ 2,700 days ≈ 7.4 years
  
  Still acceptable for marine service life.
*/



// Previous values for change detection (simple globals)
uint32_t prev_ChargedEnergy = 0;
uint32_t prev_DischrgdEnergy = 0;
uint32_t prev_AltChrgdEnergy = 0;
int32_t prev_AltFuelUsed = 0;
int32_t prev_EngineRunTime = 0;
int32_t prev_EngineCycles = 0;
int32_t prev_AltOnTime = 0;
int32_t prev_SOC_percent = 0;
int32_t prev_CoulombCount = 0;
uint32_t prev_SessionDur = 0;
int32_t prev_MaxLoop = 0;
int32_t prev_MinHeap = 0;
int32_t prev_PowerCycles = 0;
float prev_InsulDamage = 0.0f;
float prev_GreaseDamage = 0.0f;
float prev_BrushDamage = 0.0f;
float prev_ShuntGain = 0.0f;
float prev_AltZero = 0.0f;
uint32_t prev_LastGainTime = 0;
uint32_t prev_LastZeroTime = 0;
float prev_LastZeroTemp = 0.0f;
uint32_t prev_SolarEnergy = 0;
int32_t prev_EngineFuel = 0;
float prev_ChargeCycles = 0;
int32_t prev_TotalDist = 0;
int32_t prev_AvgSpeed = 0;
// NVS cache variables for _AllTime
uint32_t prev_ChargedEnergy_AllTime = 0;
uint32_t prev_DischrgdEnergy_AllTime = 0;
uint32_t prev_AltChrgdEnergy_AllTime = 0;
uint32_t prev_SolarEnergy_AllTime = 0;
int32_t prev_AltFuelUsed_AllTime = 0;
int32_t prev_EngineFuel_AllTime = 0;
int32_t prev_EngineRunTime_AllTime = 0;
int32_t prev_EngineCycles_AllTime = 0;
int32_t prev_AltOnTime_AllTime = 0;
float prev_ChargeCycles_AllTime = 0;
int32_t prev_TotalDist_AllTime = 0;
int32_t prev_AvgSpeed_AllTime = 0;
int32_t prev_AvgSOC = 0;
int32_t prev_AvgSOC_AllTime = 0;
static float prev_sailing_days_alltime = -1.0f;

// Accumulators for runtime tracking
unsigned long engineRunAccumulator = 0;     // Milliseconds accumulator for engine runtime
unsigned long alternatorOnAccumulator = 0;  // Milliseconds accumulator for alternator runtime

//ALternator Lifetime Prediction
// Thermal Stress Calculation - Settings
const unsigned long THERMAL_UPDATE_INTERVAL = 10000;  // 10 seconds   (this is just used for how often to update thermal stresses)

float WindingTempOffset = 50.0;  // User configurable winding temp offset (°F)
float PulleyRatio = 2.0;         // User configurable pulley ratio
int ManualLifePercentage = 100;  // Manual override for life remaining %

// Thermal Stress Calculation - Accumulated Damage
float CumulativeInsulationDamage = 0.0;  // 0.0 to 1.0 (1.0 = end of life)
float CumulativeGreaseDamage = 0.0;      // 0.0 to 1.0 (1.0 = end of life)
float CumulativeBrushDamage = 0.0;       // 0.0 to 1.0 (1.0 = end of life)

// Thermal Stress Calculation - Current Status
float InsulationLifePercent = 100.0;  // Current insulation life remaining %
float GreaseLifePercent = 100.0;      // Current grease life remaining %
float BrushLifePercent = 100.0;       // Current brush life remaining %
float PredictedLifeHours = 10000.0;   // Minimum predicted life remaining
int LifeIndicatorColor = 0;           // 0=green, 1=yellow, 2=red


// Timing
unsigned long lastThermalUpdateTime = 0;  // Last thermal calculation time
const int AnalogInputReadInterval = 900;

// Physical Constants
const float EA_INSULATION = 1.0f;     // eV, activation energy
const float BOLTZMANN_K = 8.617e-5f;  // eV/K
const float T_REF_K = 373.15f;        // 100°C reference temperature in Kelvin
const float L_REF_INSUL = 50000.0f;   // Reference insulation life at 100°C (hours)
const float L_REF_GREASE = 40000.0f;  // Reference grease life at 158°F (hours)
const float L_REF_BRUSH = 5000.0f;    // Reference brush life at 6000 RPM and 150°F (hours)

//Cool dynamic factors
// SOC Correction Dynamic Learning
int AutoShuntGainCorrection = 0;           // 0=off, 1=on - enable/disable auto-correction
float DynamicShuntGainFactor = 1.0;        // Learned gain correction factor (starts at 1.0)
int ResetDynamicShuntGain = 0;             // Momentary reset button (0=normal, 1=reset)
unsigned long lastGainCorrectionTime = 0;  // Last time gain correction was applied

// SOC Correction Protection Constants
const float MAX_GAIN_ADJUSTMENT_PER_CYCLE = 0.05;            // Max 5% change per full charge detection
const float MIN_DYNAMIC_GAIN_FACTOR = 0.8;                   // Don't go below 80%
const float MAX_DYNAMIC_GAIN_FACTOR = 1.2;                   // Don't go above 120%
const float MAX_REASONABLE_ERROR = 0.2;                      // Don't correct if error > 20%
const unsigned long MIN_GAIN_CORRECTION_INTERVAL = 3600000;  // 1 hour minimum between corrections

// Alternator Current Auto-Zeroing
int AutoAltCurrentZero = 0;         // 0=off, 1=on - enable/disable auto-zeroing
float DynamicAltCurrentZero = 0.0;  // Learned zero offset (starts at 0.0)
int ResetDynamicAltZero = 0;        // Momentary reset button (0=normal, 1=reset)

// Auto-zeroing timing and state tracking
unsigned long lastAutoZeroTime = 0;                // Last time auto-zero was performed
float lastAutoZeroTemp = -999.0;                   // Temperature when last auto-zero was done
unsigned long autoZeroStartTime = 0;               // When current auto-zero cycle started (0 = not active)
const unsigned long AUTO_ZERO_DURATION = 10000;    // 10 seconds at zero field
const unsigned long AUTO_ZERO_INTERVAL = 3600000;  // 1 hour in milliseconds
const float AUTO_ZERO_TEMP_DELTA = 20.0;           // 20°F temperature change triggers auto-zero
// Auto-zero averaging variables
float autoZeroAccumulator = 0.0;
int autoZeroSampleCount = 0;

//Momentary Buttons and alarm logic
int FactorySettings = 0;  // Reset Button
// Add these alarm variables with your other globals
bool alarmLatch = false;    // Current latched alarm state
int AlarmLatchEnabled = 0;  // Whether latching is enabled (0/1 for consistency)
int AlarmTest = 0;          // Momentary alarm test (1 = test active)
bool alarmOutputState = false;  // Single source of truth for GPIO 21 state — mirrors every digitalWrite(21,...)
int ResetAlarmLatch = 0;    // Momentary reset command
unsigned long alarmTestStartTime = 0;
const unsigned long ALARM_TEST_DURATION = 2000;  // 2 seconds test duration
int Alarm_Status;                                // for alarm mirror light on Client

//More Settings
// SOC Parameters
float CurrentThreshold = 0.01f;       // Ignore currents below this (amps)
int PeukertExponent_scaled = 105;     // Peukert exponent × 100 (112 = 1.12)
int ChargeEfficiency_scaled = 990;    // Charging efficiency % × 10 (990 = 99.0%)
int ChargedVoltage_Scaled = 1450;     // Voltage threshold for "charged" (V × 100) (a Battery Monitor setup parameter, nothing to do with alternator)
float TailCurrent = 2.0f;             // % of battery Ah capacity (1 decimal place)
int ShuntResistanceMicroOhm = 100;    // Shunt resistance in microohms
int ChargedDetectionTime = 600;       // Time at charged state to consider 100% (seconds) (10 mins)
int IgnoreTemperature = 0;            // If no temp sensor, set to 1
int bmsLogic = 0;                     // if BMS is asked to turn the alternator on and off
int bmsLogicLevelOff = 0;             // set to 0 if the BMS gives a low signal (<3V?) when no charging is desired
bool chargingEnabled;                 // defined from other variables
bool bmsSignalActive;                 // Read from GPIO34
int AlarmActivate = 0;                // set to 1 to enable alarm conditions
int TempAlarm = 190;                  // above this value, sound alarm
int VoltageAlarmHigh = 15;            // above this value, sound alarm
int VoltageAlarmLow = 11;             // below this value, sound alarm
int CurrentAlarmHigh = 100;           // above this value, sound alarm
int MaximumAllowedBatteryAmps = 150;  // safety for battery, optional
int FourWay = 0;                      // OBSOLETE REMOVE
int RPMScalingFactor = 1330;          // self explanatory, adjust until it matches your trusted tachometer
float AlternatorCOffset = 0;          // tare for alt current
float BatteryCOffset = 0;             // tare or batt current
int timeToFullChargeMin = NAN;        // self explained
int timeToFullDischargeMin = NAN;     // self explained


int ResetTemp;              // reset the maximum alternator temperature tracker
int ResetVoltage;           // reset the maximum battery voltage measured
int ResetCurrent;           // reset the maximmum alternator output current
int ResetEngineRunTime;     // reset engine run time tracker
int ResetAlternatorOnTime;  //reset AlternatorOnTime
int ResetEnergy;            // ???
int ResetDischargedEnergy;  // total discharged from battery
int ResetFuelUsed;          // fuel used by alternator
int ResetAlternatorChargedEnergy;
int ResetEngineCycles;
int ResetRPMMax;
int ResetThermTemp = 0;  // Max thermistor temp reset

unsigned long fieldCollapseTime = 0;
unsigned long FIELD_COLLAPSE_DELAY = 30000;  // ms
// Lockout period after temperature or voltage spike faults.
// Charging resumes automatically once this expires and the fault has cleared.
// NOTE: INA228 hardware overvoltage is NOT governed by this timer — it uses
// its own independent 10s latch in CheckAlarms() and bypasses the lockout
// entirely via shouldImmediatelyCutGPIO4().
int fieldActiveStatus = 0;  // direct read of ESP32 hardare to control the field active light in Banner


int Voltage_scaled = 0;         // Battery voltage scaled (V × 100)
int BatteryCurrent_scaled = 0;  // A × 100
int BatteryPower_scaled = 0;    // Battery power (W × 100)
int EnergyDelta_scaled = 0;     // Energy change (Wh × 100)
float AlternatorFuelUsed = 0;   // Total fuel used by alternator (mL) - INTEGER (note: mL not L)
bool alternatorIsOn = false;    // Current alternator state
// Energy Tracking Variables
unsigned long ChargedEnergy = 0;            // Total charged energy from battery (Wh)
unsigned long DischargedEnergy = 0;         // Total discharged energy from battery (Wh)
unsigned long AlternatorChargedEnergy = 0;  // Total energy from alternator (Wh)
int FuelEfficiency_scaled = 250;            // Engine efficiency: Wh per mL of fuel (× 100)
// Engine & Alternator Runtime Tracking
int EngineRunTime = 0;          // Time engine has been spinning (minutes)
int EngineCycles = 0;           // Average RPM * Minutes of run time
int AlternatorOnTime = 0;       // Time alternator has been producing current (minutes)
bool engineWasRunning = false;  // Engine state in previous check
bool alternatorWasOn = false;   // Alternator state in previous check


// ─────────────────────────────────────────────────────────────────────────────
// CH1 Interval Instrumentation — zero heap, all storage is static
//
// Three tiers:
//   10s  : ring of timestamped interval entries; stats computed on demand
//   2m   : ring of 12 pre-aggregated 10-second closed buckets
//   at   : running scalar accumulators (all-time this session)
//
// 2m stats reflect only closed buckets, so the most recent ≤10s of data is
// not yet included. Acceptable for a diagnostic tool.
//
// CH1_RING sizing: must hold at least 10000 / (minimum expected CH1 interval ms).
// Default 2000 × 6 bytes = 12 KB static on ESP32.
// After timing test, set CH1_RING = ceil(10000 / actual_min_interval_ms) × 1.25.
// ─────────────────────────────────────────────────────────────────────────────
// CH1 interval telemetry globals
uint16_t ch1_last_ms = 0;
float ch1_avg_10s = 0;
uint16_t ch1_worst_10s = 0;
uint16_t ch1_over2x_10s = 0;
uint16_t ch1_n_10s = 0;
float ch1_avg_2m = 0;
uint16_t ch1_worst_2m = 0;
uint32_t ch1_over2x_2m = 0;
uint32_t ch1_n_2m = 0;
float ch1_avg_at = 0;
uint16_t ch1_worst_at = 0;
uint32_t ch1_over2x_at = 0;
uint32_t ch1_n_at = 0;

#define CH1_RING 5000
#define CH1_BUCKETS 12  // 12 closed buckets × 10 s = 2-minute window

struct Ch1Entry {
  uint32_t ts;  // millis() at read completion
  uint16_t iv;  // interval since previous CH1 read, ms (saturates at 65535)
};

struct Ch1Bucket {
  uint32_t sum;     // sum of all intervals in this bucket, ms
  uint16_t count;   // number of CH1 reads in this bucket
  uint16_t worst;   // maximum interval seen, ms
  uint16_t over2x;  // count of intervals > 2× this bucket's own mean
};

// 10-second ring
static Ch1Entry *ch1Ring = nullptr;
static uint16_t ch1Head = 0;
static uint16_t ch1Count = 0;  // valid entries; saturates at CH1_RING

// 2-minute bucket ring
static Ch1Bucket ch1Buckets[CH1_BUCKETS];
static uint8_t ch1BktHead = 0;
static uint8_t ch1BktCount = 0;
static uint32_t ch1BktStart = 0;  // millis() when current bucket opened
// ── 1s mini-buckets for O(1) 10s window stats (eliminates ring scan) ──────
#define CH1_1S_BUCKETS 10
Ch1Bucket ch1Bkt1s[CH1_1S_BUCKETS];          // closed 1s buckets ring (internal RAM, ~120 bytes)
Ch1Bucket ch1Bkt1sCurrent = { 0, 0, 0, 0 };  // currently open/accumulating 1s bucket
uint8_t ch1Bkt1sHead = 0;                    // next write slot in ch1Bkt1s[]
uint8_t ch1Bkt1sCount = 0;                   // how many closed 1s buckets are valid
uint32_t ch1Bkt1sStart = 0;                  // millis() when current 1s bucket opened
// ── End 1s mini-bucket globals ─────────────────────────────────────────────

// All-time accumulators
static uint64_t ch1AtSum = 0;  // 64-bit: survives multi-hour sessions
static uint32_t ch1AtCount = 0;
static uint16_t ch1AtWorst = 0;
static uint32_t ch1AtOver2x = 0;  // threshold = running mean at sample time (approximation)

// State
static uint32_t ch1PrevTs = 0;
static bool ch1HasPrev = false;

// variables used to show how long each loop takes
uint64_t starttime;
uint64_t endtime;
uint64_t LoopTime;                  // must not use unsigned long becasue cant run String() on an unsigned long and that's done by the wifi code
int WifiStrength = -999;            // must not use unsigned long becasue cant run String() on an unsigned long and that's done by the wifi code
int MaximumLoopTime;                // must not use unsigned long becasue cant run String() on an unsigned long and that's done by the wifi code
unsigned long prev_millis7888 = 0;  // used to reset the meximum loop time
// ── Per-function worst-case timing ───────────────────────────────────────────
// worstWindow resets every FUNC_TIMING_WINDOW_MS — shows current spike behavior
// worstSession resets only at boot — never lies about session worst
struct FuncTiming {
  uint32_t worstWindow;
  uint32_t worstSession;
  uint32_t lastCall;  // Most recent individual call duration (µs)
};

// One instance per timed function
FuncTiming ft_ReadAnalogInputs;
FuncTiming ft_saveNVSData;
FuncTiming ft_AdjustFieldLearnMode;
FuncTiming ft_logDashboardValues;
FuncTiming ft_printSystemHealth;
FuncTiming ft_checkWiFiConnection;
FuncTiming ft_SendWifiData;
FuncTiming ft_CheckAlarms;
FuncTiming ft_calculateDerivedMetrics;
FuncTiming ft_ch1_compute_stats;
FuncTiming ft_uploadSensorHistory;
FuncTiming ft_uploadBufferedRecords;
FuncTiming ft_buildConfigPayload;
FuncTiming ft_UpdateEngineRuntime;
FuncTiming ft_UpdateEngineFuel;
FuncTiming ft_UpdateBatterySOC;
FuncTiming ft_UpdateTravelStatistics;
FuncTiming ft_UpdateDistanceThisInterval;
FuncTiming ft_UpdateBoardTempPressureMaximums;
FuncTiming ft_handleSocGainReset;
FuncTiming ft_handleAltZeroReset;
FuncTiming ft_calculateChargeTimes;
FuncTiming ft_UpdateSailingMetrics;
FuncTiming ft_updateWeatherMode;
FuncTiming ft_updateSensorWindow;
FuncTiming ft_checkTimeSync;
FuncTiming ft_rai_total;      // ReadAnalogInputs() — full function including flash writes
FuncTiming ft_rai_ina228;     // INA228 read block only
FuncTiming ft_rai_ads_state;  // ADS1115 state machine — cost per state step
FuncTiming ft_rai_bmp_state;  // BMP388 state machine — cost per state step
FuncTiming ft_rai_imu;        // IMU FIFO drain block
FuncTiming ft_ReadVEData;
FuncTiming ft_FlushFileWriteQueue;

// Aliases so existing telemetry plumbing (CSVData2, dashboard) needs no changes.
// Time2 = last call duration; Time = worst-in-window. Both now backed by FuncTiming.
#define AnalogReadTime ft_rai_ina228.worstWindow
#define AnalogReadTime2 ft_rai_ina228.lastCall
#define IMUReadTime ft_rai_imu.worstWindow
#define IMUReadTime2 ft_rai_imu.lastCall
// Wrap any void call — records last, worst-window, and worst-session into FuncTiming.
// Nested calls (e.g. a flash write inside ReadAnalogInputs) are fully included in
// the outer timer, allowing triangulation without needing to instrument sub-calls.
#define TIMED_CALL(ft, call) \
  do { \
    uint32_t _t0 = (uint32_t)esp_timer_get_time(); \
    call; \
    uint32_t _dt = (uint32_t)esp_timer_get_time() - _t0; \
    (ft).lastCall = _dt; \
    if (_dt > (ft).worstWindow) (ft).worstWindow = _dt; \
    if (_dt > (ft).worstSession) (ft).worstSession = _dt; \
  } while (0)

// Session-scoped rolling 5s loop worst (replaces MaximumLoopTime rolling reset)
uint32_t loopTime5sWindow = 0;                     // worst loop time in last AinputTrackerTime window (µs)
constexpr unsigned long AinputTrackerTime = 5000;  // rolling window reset interval (ms)
// Previous session max loop time — snapshot of MaxLoopTime taken at boot before reset
uint32_t prevSessionMaxLoopTime = 0;  // worst loop time from the session before this one (µs)

// ── End per-function timing ───────────────────────────────────────────────────



// Global variable to track ESP32 restart time
unsigned long lastRestartTime = 0;
bool systemShuttingDown = false;

int BatteryVoltageSource = 0;  // OBSOLETE REMOVE LATER
int BatteryCurrentSource = 0;  // 0=INA228, 1=NMEA2K Batt, 2=NMEA0183 Batt, 3=Victron Batt
int timeAxisModeChanging = 0;  // toggle the time axis on and off in Plots.  Off = less janky but less info

int RPMThreshold = -20000;  //below this, there will be no field output in auto mode (Update this if we have RPM at low speeds and no field, otherwise, depend on Ignition) Check this later

int maxPoints;                 //number of points plotted per plot (X axis length)
int Ymin1 = -10, Ymax1 = 90;   // Current plot
float Ymin2 = 10, Ymax2 = 20;  // Voltage plot
int Ymin3 = 0, Ymax3 = 4000;   // RPM plot
int Ymin4 = 50, Ymax4 = 250;   // Temperature plot


//Advanced temperature control variables:
//=============================================================================
//                        LEARNING MODE GLOBAL VARIABLES
//=============================================================================

float PIDTrackingGain = 4.0f;  // Tracking gain in 1/sec (increase for faster tracking)

// Governor state
enum GovernorMode { GOV_NORMAL_SLEW,
                    GOV_BYPASS_SLEW,
                    GOV_HOLD };
enum SystemMode { SYS_MODE_OFF,
                  SYS_MODE_MANUAL,
                  SYS_MODE_AUTO,
                  SYS_MODE_FAULT };

SystemMode sysMode = SYS_MODE_OFF;
GovernorMode govMode = GOV_NORMAL_SLEW;

// Setpoint tracking
float setpointLimited = 0.0f;
bool setpointInitialized = false;

// ===== LEARNING MODE CONTROL PARAMETERS =====
// Primary Controls
//int loggingEnabled = 0;             // 0=disabled, 1=enabled    was a dead variable
int LearningPaused = 0;           // Temporarily pause learning
int LearningUpwardEnabled = 1;    // Allow upward table adjustments
int LearningDownwardEnabled = 1;  // Allow downward table adjustments

bool learningTableUpdated = false;

// Learning Parameters
int AlternatorNominalAmps = 100;               // Alternator rating for penalty calculation
float LearningUpStep = 1;                      // Table increase amount (A)
float LearningDownStep = 2;                    // Table decrease amount (A)
float AmbientTempCorrectionFactor = -0.5;      // A per °C correction
unsigned long MinLearningInterval = 30000;     // Min time between updates (ms)
unsigned long SafeOperationThreshold = 30000;  // Time for upward learning (ms)

// RPM Transition Filtering (add with other learning variables)
unsigned long lastSignificantRPMChange = 0;  // Timestamp of last major RPM change
int lastStableRPM = 0;                       // RPM before transition
int LearningSettlingPeriod = 30000;          // Milliseconds to wait after RPM change (default 30s)
int LearningRPMChangeThreshold = 500;        // RPM change that triggers settling period
int LearningTempHysteresis = 10;

// PID Tuning
int wavePeriod = 10;     //For PID tuning
int waveAmplitude = 10;  //For PID tuning
int TuningMode = 0;      //
float xTime = 60.0;      // seconds    PID Chart
int yyMax = 105;         // PID Chart     Amps
int yyMin = -25;         //  PID Chart Amps
float pidError = 0.0f;   // PID error for display (A)
int LearningMode = 0;    // 0=disabled, 1=enabled


int accelEnabled = 0;  // Set to false to block accelerometer related calcs

float PidKp = 0.5;   // Proportional gain
float PidKi = 2.0;   // Integral gain
float PidKd = 0.01;  // Derivative gain
// ===== VOLTAGE CONTROL (Float Stage) =====
float VoltageTrimLimit = 5.0f;       // max trim authority in amps.   OBSOLETE DELETE LATER
float VoltageKp = 25.0f;             // Voltage loop gain (A per V error), user adjustable
uint32_t VoltageLoopInterval = 100;  // Voltage loop update interval (ms), user adjustable
uint32_t lastVoltageLoopMs = 0;      // Last voltage loop update timestamp
float Icv = 0.0f;                    // CV velocity-form PI output (A) — direct current setpoint in CV modes
float cv_I = 0.0f;                   // CV position-form PI integrator state
bool voltageControlActive = false;   // True when in float stage (suppresses learning)
float VoltageKi = 2.5f;              // Voltage loop integral gain (A per V per second)
float VoltageTargetRiseRate = 0.3f;  // V/s — slew rate for voltage target rise only ADD TO WEB INTERFACE LATER
// Table Bounds & Safety
float MaxTableValue = 150.0;               // Maximum table entry (A)
float MinTableValue = 0.0;                 // Minimum table entry (A)
float MaxPenaltyPercent = 15.0;            // Max penalty as % of nominal
unsigned long MaxPenaltyDuration = 60000;  // Max penalty time (ms)

// Advanced Learning
float NeighborLearningFactor = 0.25;                // Neighbor reduction factor
unsigned long LearningMemoryDuration = 2592000000;  // How long to remember events (30 days in ms) SEEMS UNUSED, DELETE LATER

// Safety Overrides
int IgnoreLearningDuringPenalty = 1;  // Block learning during penalty
int EnableNeighborLearning = 1;       // Update adjacent RPM points
int EnableAmbientCorrection = 0;      // Apply temperature correction

// Diagnostics & Debugging
int ShowLearningDebugMessages = 1;  // Verbose console output
int LogAllLearningEvents = 0;       // Log every learning decision
int CloudFeatures = 1;
int LearningDryRunMode = 0;  // Calculate but don't apply changes

// Data Management
int AutoSaveLearningTable = 1;                     // Auto-save to NVS
unsigned long LearningTableSaveInterval = 300000;  // How often to save (5 min in ms)

// Momentary Actions (Reset to 0 after execution)
int ResetLearningTable = 0;    // OBSOLETE LEGACY EXTRA
int ClearOverheatHistory = 0;  // OBSOLETE LEGACY EXTRA
// ==================== ENUMS ====================
enum FieldControlMode {
  MODE_CRITICAL_RAMP,             // Critical fault: ramp to 0, cut if fault persists
  MODE_WARNING_RAMP_AND_LOCKOUT,  // Warning fault: ramp to 0, cut if fault persists, start lockout
  MODE_LOCKOUT_RAMP,              // Lockout/auto-zero: ramp to 0, cut when settled
  MODE_DISABLED_RAMP,             // Normal shutdown: ramp to 0, cut when settled
  MODE_NORMAL_MANUAL,             // Manual control with rate limiting
  MODE_NORMAL_AUTO_PID            // PID control with learning
};

enum FieldEventReason : uint8_t {
  REASON_NONE = 0,
  REASON_AUTOZERO_ACTIVE,
  REASON_TEMP_STALE,
  REASON_TEMP_CRITICAL,
  REASON_TEMP_WARNING,
  REASON_TEMP_SUSTAINED,
  REASON_VOLTAGE_IMPLAUSIBLE,
  REASON_VOLTAGE_DISAGREE_CRITICAL,
  REASON_VOLTAGE_SPIKE,
  REASON_VOLTAGE_DISAGREE_WARNING,
  REASON_LOCKOUT_ACTIVE,
  REASON_CHARGING_DISABLED,
  REASON_MANUAL_MODE,
  REASON_INA_OVERVOLTAGE,
  REASON_HARD_OVERCURRENT
};

// ==================== TICK SNAPSHOT STRUCT ====================

struct TickSnapshot {
  uint32_t nowMs;
  uint32_t dt_ms;
  float currentBatteryVoltage;
  float batteryV;
  float ibv;
  float batteryCurrentA;
  float rpmMinDuty;

  bool chargingEnabled;
  bool manualMode;
  bool autoZeroActive;

  bool tempDataVeryStale;
  bool ignoreTemperature;

  bool voltagePlausible;
  bool voltageDisagreementCritical;
  bool voltageDisagreementWarning;

  bool inLockout;

  float bulkVoltage;
  float voltageSpikeMargin;

  float tempToUseF;
  float tempLimitF;
  float tempWarnExcessF;
  float tempCritExcessF;
  bool tempSourceIsAlt;

  bool inaOvervoltageLatched;

  bool inAbsorptionStage;
};

// ==================== CONFIGURABLE PARAMETERS ====================
// Expose these in web UI for tuning

// --- Rate Limiting (LM2907 coupling cap protection) ---
float DutyRampRate = 50.0f;  // %/sec - max rate of duty cycle change (protects coupling cap from harsh transitions, includes OnOff toggle!)
// Asymmetric setpoint slew
float SetpointRiseRate = 30.0f;  // A/sec
float SetpointFallRate = 50.0f;  // A/sec
float SetpointRampRate = 0.0f;   // THIS IS OBSOLETE AND NEEDS DELETING SOMEDAY LATER

// --- Settle Time Before GPIO4 Cut ---
uint32_t SettleTimeBeforeCut = 1000;  // ms - how long duty must be at 0% before GPIO4 goes LOW

// --- Temperature Thresholds (°F above TemperatureLimitF) ---
float TempWarnExcess = 10.0f;            // °F above limit triggers WARNING ramp, starts lockout
float TempCritExcess = 30.0f;            // °F above limit triggers IMMEDIATE GPIO4 cut (skips Phase 1 ramp)
uint32_t TempSustainedTimeout = 120000;  // ms - WARNING temp sustained this long triggers GPIO4 cut (2 min default)

// --- Voltage Thresholds ---
float VoltageSpikeMargin = 0.3f;          // V above BulkVoltage triggers voltage spike warning
float VoltageDisagreeThreshold = 0.15f;   // V difference between BatteryV and IBV for disagreement detection
uint32_t VoltageDisagreeTimeout = 10000;  // ms - sustained disagreement this long triggers warning
uint32_t VoltageDisagreeCriticalTimeoutMs = 3000;
// ==================== STATE VARIABLES (global for telemetry) ====================

// --- Hardware State Tracking ---
float lastAppliedDuty = -1.0f;  // Actual  duty last sent to hardware (-1 = never set) NO LONGER AN INT!
                                // Used to initialize previousDutyCycle accurately

// --- Two-Phase Shutdown State ---
uint32_t settledAtZeroDutyMs = 0;  // When duty first reached 0% (0 = not settled yet)
bool gpio4IsLow = false;           // Track actual GPIO4 state for logging/telemetry

// --- Temperature Sustained Warning Timer ---
uint32_t tempWarningStartMs = 0;  // When temp first exceeded limit+TempWarnExcess (0 = not active)

// --- Voltage Disagreement Timer ---
uint32_t voltageDisagreementStart = 0;

// --- Mode Transition Tracking ---
FieldControlMode prevMode = (FieldControlMode)255;  // Previous mode for transition detection

// --- Reporting State ---
FieldControlMode lastReportedMode = (FieldControlMode)255;
FieldEventReason lastReportedReason = (FieldEventReason)255;
uint32_t lastReportMs = 0;

// --- Lockout Transition Tracking ---
bool lockoutWasActive = false;

// Fast OV supervisor instrumentation
float g_fastOvCurrentCap = 0.0f;   // live cap ceiling this tick (amps)
bool g_fastOvClampActive = false;  // true if cap is below MaxTableValue this tick
uint32_t g_fastOvClampCount = 0;   // rising-edge counter — watch for increments



// ===== "TARGET" TABLE DATA =====
#define RPM_TABLE_SIZE 10
float rpmCurrentTable[RPM_TABLE_SIZE] = { 0, 70, 70, 80, 80, 90, 90, 90, 90, 90 };
int rpmTableRPMPoints[RPM_TABLE_SIZE] = { 100, 600, 1100, 1600, 2100, 2600, 3100, 3600, 4100, 4600 };
// Reset for conservative factory defaults for current limits
float defaultCurrentValues[RPM_TABLE_SIZE] = { 0, 70, 70, 80, 80, 90, 90, 90, 90, 90 };
// Reset for factory defaults for RPM breakpoints
int defaultRPMValues[RPM_TABLE_SIZE] = { 100, 600, 1100, 1600, 2100, 2600, 3100, 3600, 4100, 4600 };


// ===== CAP CURRENT TABLE (always enabled) =====
// RPM-dependent ceiling for belt/shaft/mounting limits
float rpmCapCurrentTable[RPM_TABLE_SIZE] = { 120, 120, 120, 120, 120, 120, 120, 120, 120, 120 };
// Factory defaults for cap current table
float defaultCapCurrentValues[RPM_TABLE_SIZE] = { 120, 120, 120, 120, 120, 120, 120, 120, 120, 120 };

float rpmCapPowerTable[RPM_TABLE_SIZE] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
float defaultCapPowerValues[RPM_TABLE_SIZE] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
uint8_t capLimitMode = 0;  // 0 = amps, 1 = kW

// ===== MINIMUM FIELD TABLE  =====
float rpmMinDutyTable[RPM_TABLE_SIZE] = { 18.0, 18.0, 18.0, 10.0, 10.0, 5.0, 5.0, 5.0, 5.0, 5.0 };
float defaultMinDutyValues[RPM_TABLE_SIZE] = { 18.0, 18.0, 18.0, 10.0, 10.0, 5.0, 5.0, 5.0, 5.0, 5.0 };

unsigned long lastOverheatTime[RPM_TABLE_SIZE] = { 0 };          // Timestamp of last overheat per RPM
int overheatCount[RPM_TABLE_SIZE] = { 0 };                       // Total overheat events per RPM
unsigned long cumulativeNoOverheatTime[RPM_TABLE_SIZE] = { 0 };  // Safe operation time per RPM
uint32_t learningUpCount[RPM_TABLE_SIZE] = { 0 };

// ===== LEARNING STATE VARIABLES =====
uint32_t overheatPenaltyEndMs = 0;           // as used by AdjustFieldLearning Mode
unsigned long overheatingPenaltyTimer = 0;   //Same as above, but used in interfaces (legacy)
float overheatingPenaltyAmps = 0.0;          // Current penalty amount (A)
int currentRPMTableIndex = -1;               // Which table entry we're in (-1 = invalid)
unsigned long learningSessionStartTime = 0;  // Session start time for current RPM
unsigned long lastTableUpdateTime = 0;       // Last learning update timestamp

// ===== PID CONTROLLER VARIABLES =====
double pidInput = 0.0;                                                             // Current measured amps
double pidOutput = 0.0;                                                            // PID duty cycle output
double pidSetpoint = 0.0;                                                          // Current target amps
PID currentPID(&pidInput, &pidOutput, &pidSetpoint, PidKp, PidKi, PidKd, DIRECT);  // PID object
bool pidInitialized = false;                                                       // PID initialization status

// ===== PID Term Contributions (telemetry/visualization) =====
// Inner current loop (currentPID) — units: duty cycle %
// P is exact. D is from error derivative. I is residual (output - P - D).
float innerTermP = 0.0f;
float innerTermI = 0.0f;
float innerTermD = 0.0f;

// Outer temperature loop (tempPID) — units: amps
// Only updated when tempPID.Compute() returns true (~every TempPIDIntervalMs).
// Values hold between computes, which is correct — the integrator state is stable.
float outerTermP = 0.0f;
float outerTermI = 0.0f;
float outerTermD = 0.0f;          // probably always 0
float outerTermDExternal = 0.0f;  //the more interesting one

volatile bool tempPIDResetRequested = false;
volatile bool innerPIDResetRequested = false;

float tempFiltered = NAN;
float thermalPenaltyLastValid = 0.0f;
float dBuf[6] = { NAN, NAN, NAN, NAN, NAN, NAN };
uint8_t dHead = 0;

float outerImpliedPenalty = 0.0f;
bool outerAntiWindupFired = false;

// ===== CALCULATED VALUES FOR DISPLAY =====
float learningTargetFromRPM = -1.0;  // Table lookup result before corrections
float ambientTempCorrection = 0.0;   // Calculated temp correction (A)
float finalLearningTarget = 0.0;     // After all corrections applied
float ambientTemp = NAN;             // Current ambient temperature (°C)
float baroPressure = NAN;            // bmp380

// ===== LEARNING DIAGNOSTICS =====
unsigned long totalLearningEvents = 0;    // Total learning updates performed
unsigned long totalOverheats = 0;         // Sum of all overheatCount[]
uint64_t totalSafeMs = 0;                 //   used in AdjustFieldLearningMode
float totalSafeHours;                     //  same as above, but for displays (legacy)
float averageTableValue = 0.0;            // Mean of rpmCurrentTable[]
unsigned long timeSinceLastOverheat = 0;  // Global time since any overheat


// ===== FIELD SHUTDOWN STATE MACHINE =====
enum ShutdownPhase {
  SHUTDOWN_PHASE_NONE = 0,
  SHUTDOWN_PHASE_1 = 1,
  SHUTDOWN_PHASE_3 = 3,
  SHUTDOWN_PHASE_4 = 4
};
ShutdownPhase shutdownPhase = SHUTDOWN_PHASE_NONE;
uint32_t shutdownPhaseEntryMs = 0;
uint32_t shutdownPhase2EntryMs = 0;  // add with other shutdown globals
// Tunable parameters (expose in web UI)
float DutySlowRampRate = 1.0f;      // %/sec - Phase 3 slow ramp to 0
uint32_t ShutdownPhase2HoldMs = 0;  // ms - hold at rpmMinDuty before slow ramp (0 = skip)

// ===== TEMPERATURE PID (Outer Loop - replaces thermal model) =====
// Tuning — all web UI configurable
float TempPIDKp = 3.0f;            // A/°F proportional gain
float TempPIDKi = 0.025f;          // Integral gain
float TempPIDKd = 0.0f;            // Derivative gain (provides predictive feel via dT/dt)
float TempPIDKdExternal = 200.0f;  // external D gain, 20s window

float ThermalPenaltyRiseRate = 60.0f;  // A/s — how fast penalty can increase (restrict current)
float ThermalPenaltyFallRate = 20.0f;  // A/s — how fast penalty can decrease (allow more current)
float prevThermalPenalty = 0.0f;       // file-scope, tracks previous slew-limited value

float TempPIDMarginF = 15;              //15.0f;           // °F below TemperatureLimitF — PID targets this, not the limit itself
uint32_t TempPIDIntervalMs = 5000;      // Outer loop update period (ms) — independent of inner loop and sensor rate
float TempPIDFilterAlpha = 0.2f;        // IIR smoothing on temp input (0=frozen, 1=raw)
uint32_t TempPIDStaleMs = 15000;        // Hold-last if temp older than this (ms)
float TempPIDAntiWindupMarginA = 5.0f;  // OBSOLETE REMOVE
// Runtime state — expose via telemetry
double tempPIDInput_d = 77.0;        // Filtered temp (°F), PID input
double tempCapAmps_d = 9999.0;       // OBSOLETE
double tempPIDSetpoint_d = 0.0;      // Setpoint = TemperatureLimitF - TempPIDMarginF
float tempCapAmps = 9999.0f;         // OBSOLETE
bool tempPIDActive = false;          // true when outer PID is in AUTO
bool tempFilterNeedsReseed = false;  // Set true to force IIR cold-start on next tempPID_tick()

float thermalPenaltyAmps = 0.0f;    // outer PID output: amps subtracted from target table
double thermalPenaltyAmps_d = 0.0;  // double version for PID library

//REVERSE because rising temperature should increase the penalty
PID tempPID(&tempPIDInput_d, &thermalPenaltyAmps_d, &tempPIDSetpoint_d, TempPIDKp, TempPIDKi, TempPIDKd, REVERSE);




//=============================================================================
//                    END LEARNING MODE GLOBAL VARIABLES
//=============================================================================
// ===== THERMAL LOG (PSRAM circular buffer) =====
#define THERMAL_LOG_SIZE 7200  // was 2000 — now covers 2 hours at 1 Hz
#define THERMAL_LOG_INTERVAL_MS 1000

volatile bool thermalLogPaused = false;
volatile uint32_t thermalLogPausedAtMs = 0;
#define THERMAL_LOG_PAUSE_TIMEOUT_MS 30000



static int16_t thermalLogScale10(float v) {
  if (isnan(v) || isinf(v)) return 0;
  float scaled = v * 10.0f;
  if (scaled > 32767.0f) return 32767;
  if (scaled < -32768.0f) return -32768;
  return (int16_t)lroundf(scaled);
}

static int16_t thermalLogScaleRPM(float v) {
  if (isnan(v) || isinf(v)) return 0;
  if (v > 32767.0f) return 32767;
  if (v < -32768.0f) return -32768;
  return (int16_t)lroundf(v);
}

static uint8_t thermalLogGetStageCode() {
  return getChargeStageDisplayCode();
}

struct ThermalBinDLState {
  uint8_t header[8];  // count(uint32) + intervalMs(uint32), little-endian
  int headerPos;
  int count;
  int oldest;
  int row;
  bool done;
  uint8_t entryBuf[48];
  int entryLen;
  int entryPos;
};


struct ThermalLogEntry {
  uint32_t ts;

  // tempRaw removed — filtered is sufficient for PID tuning
  int16_t tempFiltered;
  int16_t tempSetpoint;
  int16_t nominalTarget;
  int16_t rpmCap;
  int16_t voltCap;
  int16_t uTarget;
  int16_t spLimited;
  int16_t pidErr;
  int16_t pidOut;
  int16_t duty;
  int16_t rpm;
  int16_t battV;
  int16_t measAmps;
  int16_t penaltyAmps;

  uint8_t flags;
  uint8_t antiWindupFired;
  uint8_t chargeStageDisplay;
  uint8_t pad;

  int16_t outerTermP;
  int16_t outerTermI;
  int16_t outerTermD;
  int16_t impliedPenalty;
  int16_t outerTermDExternal;
  // gainKp/Ki/Kd/KdExternal removed — written once in CSV CONST row
};

// At file scope, outside setupServer():
struct ThermalDLState {
  int count;
  int oldest;
  int row;
  bool done;
  char line[500];
  int lineLen;
  int linePos;
};


ThermalLogEntry *thermalLog = nullptr;
int thermalLogHead = 0;
int thermalLogCount = 0;
bool thermalLogReady = false;
uint32_t thermalLogBurstUntilMs = 0;

#define PID_LOG_SIZE 2400
#define CV_LOG_SIZE 6000
#define CV_LOG_HEADER_SIZE 24
#define CV_LOG_ENTRY_SIZE 44

volatile bool pidLogPaused = false;
volatile uint32_t pidLogPausedAtMs = 0;

struct PidLogEntry {
  // ── Timestamp ────────────────────────────────────────────────────
  uint32_t ts;  // millis() at log point

  // ── Mode / Stage ─────────────────────────────────────────────────
  uint8_t chargeStageDisplay;  // getChargeStageDisplayCode() enum value
  uint8_t TargetVoltageMode;   // runtime TargetVoltageMode flag (0 or 1)
  uint8_t flags;               // bit0=AUTO bit1=voltCtrl bit4=govBypass
  uint8_t pad0;

  // ── CV outer loop ────────────────────────────────────────────────
  float battV;                  // tick.currentBatteryVoltage
  float ChargingVoltageTarget;  // target voltage this tick
  float vError;                 // ChargingVoltageTarget - battV (always fresh)
  float Icv;                    // CV velocity-form PI output — direct current setpoint (A)
  float cv_I;                   // CV position-form PI integrator state
  float tableThermalLimit;      // uTargetAmps before CV — RPM cap minus thermal penalty
  float setpointCmd;            // value fed to setpointCommand (Icv in CV, tableThermalLimit in bulk)

  // ── Voltage loop event flags ─────────────────────────────────────
  uint8_t voltageLoopRanThisTick;     // 1 only when interval/enteringCV fired
  uint8_t enteringCV;                 // 1 on first CV tick (transition only)
  uint8_t enteringTargetVoltageMode;  // 1 on mode entry tick only
  uint8_t pad1;

  // ── Inner current PID ────────────────────────────────────────────
  float pidSetpoint;     // setpointLimited — slew-filtered command to PID
  float pidInput;        // measured current fed to PID
  float pidUnsatOutput;  // pre-clamp PID output (saturation detector)
  float pidOutput;       // clamped PID output → duty request
  float innerTermP;      // P contribution this tick
  float innerTermI;      // I contribution this tick
  float innerTermD;      // D contribution this tick

  // ── Duty pipeline ────────────────────────────────────────────────
  float dutyRequest;  // value sent into governor_apply()
  float dutyApplied;  // value returned by governor_apply()

  // ── Context ──────────────────────────────────────────────────────
  float rpm;
  float measAmps;
  float gainKp;
  float gainKi;
  float gainKd;
  // ── Filtered signals ─────────────────────────────────────────────
  float battV_filt;  // BatteryV_filtered
  float iMeas_filt;  // MeasuredAmps_filtered
};                   // 104 bytes — naturally aligned, no implicit holes

struct PidDLState {
  int count;
  int oldest;
  int row;
  bool done;
  char line[440];  // header row = 364 chars; comment block = 348 chars; was 320, too small
  int lineLen;
  int linePos;
};


PidLogEntry *pidLog = nullptr;
int pidLogHead = 0;
int pidLogCount = 0;
bool pidLogReady = false;


// ── pidLog capture globals ──────────────────────────────────────────────────
// Written during AdjustFieldLearnMode(), read by pidLog_tick().
// Reset to zero at the top of each gated control tick.
float pidLog_vError = 0.0f;
float pidLog_uTargetBeforeVoltCap = 0.0f;
float pidLog_uTargetAfterVoltCap = 0.0f;
float pidLog_dutyRequest = 0.0f;
float pidLog_dutyApplied = 0.0f;
uint8_t pidLog_voltageLoopRanThisTick = 0;
uint8_t pidLog_enteringCV = 0;
uint8_t pidLog_enteringTargetVoltageMode = 0;



// ===========================================================================
// CV / Voltage Tuner Log
// Logs every AUTO-mode inner-loop tick (~16.7 Hz) while voltageControlActive.
// Binary download via /cvlog.bin, decoded to CSV by JS.
// 32 bytes/entry × 6000 entries = 192 KB PSRAM → ~6 min at full rate.
// ===========================================================================

// ---------------------------------------------------------------------------
// STRUCT  (32 bytes, offsets below match JS parser exactly)
// ---------------------------------------------------------------------------
//
//  offset  field          scale      notes
//  ──────  ─────          ─────      ─────
//   0      ts             raw ms     millis()
//   4      battV          ×100       BatteryV
//   6      targV          ×100       ChargingVoltageTarget
//   8      vErrorMv       ×1000      (target − batt), millivolts resolution
//  10      dvdt_x1000     ×1000      filtered dV/dt, signed
//  12      vPred          ×100       BatteryV + TD_PRED*max(0,dvdt)
//  14      fastOvCap      ×10        fastOvCurrentCap ceiling this tick
//  16      cv_I_x10       ×10        cv_I integrator state
//  18      Icv_x10        ×10        Icv PI output (current setpoint to inner loop)
//  20      uTarget        ×10        uTargetAmps (table+thermal+user ceiling)
//  22      spLimited      ×10        setpointLimited (slewed command to PID)
//  24      iMeas          ×10        MeasuredAmps
//  26      duty           ×10        dutyCycle
//  28      flags          —          see bit definitions below
//  29      pad            —          zero
//  30      rpm            raw        RPM, clamped to int16 range
//
//  flags bits:
//    b0  fastOvActive    any OV clamp fired this tick
//    b1  voltLoopFired   voltage PI ran this tick (100ms cadence)
//    b2  cvActive        voltageControlActive
//    b3  softClamp       K_SOFT correction applied
//    b4  hardClamp       K_HARD or HARD_CLAMP_HYST block applied




struct CvLogEntry {
  uint32_t ts;
  int16_t battV;
  int16_t targV;
  int16_t vErrorMv;
  int16_t dvdt_x1000;
  int16_t vPred;
  int16_t fastOvCap;
  int16_t cv_I_x10;
  int16_t Icv_x10;
  int16_t uTarget;
  int16_t spLimited;
  int16_t iMeas;
  int16_t duty;
  uint8_t flags;
  uint8_t pad;
  int16_t rpm;
  int16_t battV_filt_x100;  // BatteryV_filtered × 100    (V)
  int16_t iMeas_filt_x10;   // MeasuredAmps_filtered × 10 (A)
  int16_t ch1IntervalMs;    // last CH1 inter-sample gap   (ms)
  int16_t pad2;             // explicit alignment pad — keeps sizeof == 40
  // pack into existing flags: bit 5 = iExcess
};
static_assert(sizeof(CvLogEntry) == 40, "CvLogEntry must be 40 bytes");


struct CvBinDLState {
  uint8_t header[CV_LOG_HEADER_SIZE];
  int headerPos;
  uint32_t count;
  uint32_t oldest;
  uint32_t row;
  bool done;
  uint8_t entryBuf[sizeof(CvLogEntry)];
  int entryLen;
  int entryPos;
};

// ---------------------------------------------------------------------------
// BINARY HEADER  (24 bytes)
// offset  field           type      notes
//   0     count           uint32    number of valid entries
//   4     entrySize       uint32    = 32 (future-proof)
//   8     voltageKp       float     VoltageKp at download time
//  12     voltageKi       float     VoltageKi at download time
//  16     voltageInterval uint32    VoltageLoopInterval ms
//  20     reserved        uint32    0
// ---------------------------------------------------------------------------

static CvLogEntry *cvLog = nullptr;
static int cvLogHead = 0;
static int cvLogCount = 0;
static bool cvLogReady = false;
static bool cvLogPaused = false;
static uint32_t cvLogPausedAtMs = 0;


float g_fastOvDvdt = 0.0f;        // filtered dV/dt (V/s), updated every battV fresh tick
float g_fastOvVpred = 0.0f;       // predicted voltage, updated when voltageControlActive
bool g_fastOvSoftActive = false;  // K_SOFT correction fired this tick
bool g_fastOvHardActive = false;  // K_HARD or hysteresis block fired this tick
uint32_t g_fastOvSoftCount = 0;
uint32_t g_fastOvHardCount = 0;

// ── Current ring / MA / dI/dt ─────────────────────────────────────────────
// Written in ADS case 1, read by AdjustFieldLearnMode and cvLog_tick
#define I_RING_SIZE 4
struct IAmpEntry {
  uint32_t ts;
  float val;
};
static IAmpEntry iAmpRing[I_RING_SIZE];
static uint8_t iAmpHead = 0;
static uint8_t iAmpCount = 0;

float g_iMA2 = 0.0f;
float g_iMA4 = 0.0f;
float g_dIdt2 = 0.0f;              // A/s, newest-to-prev
float g_dIdt4 = 0.0f;              // A/s, newest-to-oldest in ring
uint16_t g_ch1LastIntervalMs = 0;  // last CH1 inter-sample gap, for cvLog

bool g_iExcessActive = false;
float g_iExcessDutyCap = 100.0f;

//additional leaderboard stuff
float sailing_days_alltime = 0.0;             // Total sailing days (lifetime)
float sailing_ratio = 0.0;                    // % of time spent sailing (calculated)
float distance_this_interval = 0.0;           // Nautical miles traveled this interval
float max_wind_speed_true_alltime = 0.0;      // Maximum true wind speed (knots)
float max_wind_speed_apparent_alltime = 0.0;  // Maximum apparent wind speed (knots)
float board_temp_max_alltime = -999.0;        // Maximum board temperature (°F)
float board_temp_min_alltime = 999.0;         // Minimum board temperature (°F)
float baro_pressure_max_alltime = 0.0;        // Maximum barometric pressure (mbar)
float baro_pressure_min_alltime = 9999.0;     // Minimum barometric pressure (mbar)
// Helper variables for distance tracking
double last_position_lat = 0.0;
double last_position_lon = 0.0;
bool first_distance_calc = true;

// Universal data freshness tracking

// timing requirements are defined in the javascript file
// search for STALENESS DISPLAY THRESHOLDS at top of file

// Complete DataIndex enum for all variables displayed in Live Data
// Streamlined DataIndex enum - only tracks real-time sensor data that might go stale if a sensor is disconnected
// Excludes peak/cumulative values that should persist even when source fails
enum DataIndex {
  IDX_HEADING_NMEA = 0,          // 0
  IDX_LATITUDE_NMEA,             // 1
  IDX_LONGITUDE_NMEA,            // 2
  IDX_SATELLITE_COUNT,           // 3
  IDX_VICTRON_VOLTAGE,           // 4
  IDX_VICTRON_CURRENT,           // 5
  IDX_ALTERNATOR_TEMP,           // 6
  IDX_THERMISTOR_TEMP,           // 7
  IDX_RPM,                       // 8
  IDX_MEASURED_AMPS,             // 9
  IDX_BATTERY_V,                 // 10 - ADS1115 battery voltage
  IDX_IBV,                       // 11 - INA228 battery voltage
  IDX_BCUR,                      // 12 - Battery current from INA228
  IDX_CHANNEL3V,                 // 13 - ADS Ch3 Voltage
  IDX_DUTY_CYCLE,                // 14 - Field duty cycle percentage
  IDX_FIELD_VOLTS,               // 15 - vvout (calculated field voltage)
  IDX_FIELD_AMPS,                // 16 - iiout (calculated field current)
  IDX_COG_NMEA,                  // 17
  IDX_SOG_NMEA,                  // 18
  IDX_APPARENT_WIND_SPEED,       // 19
  IDX_APPARENT_WIND_ANGLE,       // 20
  IDX_TRUE_WIND_SPEED,           // 21
  IDX_TRUE_WIND_ANGLE,           // 22
  IDX_LEEWAY,                    // 23
  IDX_VMG,                       // 24
  IDX_BARO_PRESSURE,             // 25
  IDX_AMBIENT_TEMP,              // 26
  IDX_SOC_PERCENT,               // 27
  IDX_WIFI_STRENGTH,             // 28
  IDX_DYNAMIC_ALT_CURRENT_ZERO,  // 29
  IDX_CHARGING_MODE,             // 30
  IDX_TIME_TO_FULL_CHARGE,       // 31
  IDX_TIME_TO_FULL_DISCHARGE,    // 32
  IDX_DYNAMIC_SHUNT_GAIN,        // 33
  // Keep this last and increment when new added
  MAX_DATA_INDICES = 34
};

unsigned long dataTimestamps[MAX_DATA_INDICES];  // Uses the enum size automatically

const unsigned long DATA_TIMEOUT = 10000;  // 10 seconds default timeout
// Universal macros for clean syntax
#define MARK_FRESH(index) dataTimestamps[index] = millis()
#define IS_STALE(index) (millis() - dataTimestamps[index] > DATA_TIMEOUT)
#define SET_IF_STALE(index, variable, staleValue) \
  if (IS_STALE(index)) { variable = staleValue; }

// ISRG Root X1 Certificate (Let's Encrypt Root CA)
// Valid until: June 4, 2035 (expires in 2035, stable for 10+ years)
// This root certificate doesn't change like intermediate certificates (R10->R12 rotation)
// Using root instead of intermediate prevents certificate validation failures during Let's Encrypt updates
const char *server_root_ca =
  "-----BEGIN CERTIFICATE-----\n"
  "MIIFazCCA1OgAwIBAgIRAIIQz7DSQONZRGPgu2OCiwAwDQYJKoZIhvcNAQELBQAw\n"
  "TzELMAkGA1UEBhMCVVMxKTAnBgNVBAoTIEludGVybmV0IFNlY3VyaXR5IFJlc2Vh\n"
  "cmNoIEdyb3VwMRUwEwYDVQQDEwxJU1JHIFJvb3QgWDEwHhcNMTUwNjA0MTEwNDM4\n"
  "WhcNMzUwNjA0MTEwNDM4WjBPMQswCQYDVQQGEwJVUzEpMCcGA1UEChMgSW50ZXJu\n"
  "ZXQgU2VjdXJpdHkgUmVzZWFyY2ggR3JvdXAxFTATBgNVBAMTDElTUkcgUm9vdCBY\n"
  "MTCCAiIwDQYJKoZIhvcNAQEBBQADggIPADCCAgoCggIBAK3oJHP0FDfzm54rVygc\n"
  "h77ct984kIxuPOZXoHj3dcKi/vVqbvYATyjb3miGbESTtrFj/RQSa78f0uoxmyF+\n"
  "0TM8ukj13Xnfs7j/EvEhmkvBioZxaUpmZmyPfjxwv60pIgbz5MDmgK7iS4+3mX6U\n"
  "A5/TR5d8mUgjU+g4rk8Kb4Mu0UlXjIB0ttov0DiNewNwIRt18jA8+o+u3dpjq+sW\n"
  "T8KOEUt+zwvo/7V3LvSye0rgTBIlDHCNAymg4VMk7BPZ7hm/ELNKjD+Jo2FR3qyH\n"
  "B5T0Y3HsLuJvW5iB4YlcNHlsdu87kGJ55tukmi8mxdAQ4Q7e2RCOFvu396j3x+UC\n"
  "B5iPNgiV5+I3lg02dZ77DnKxHZu8A/lJBdiB3QW0KtZB6awBdpUKD9jf1b0SHzUv\n"
  "KBds0pjBqAlkd25HN7rOrFleaJ1/ctaJxQZBKT5ZPt0m9STJEadao0xAH0ahmbWn\n"
  "OlFuhjuefXKnEgV4We0+UXgVCwOPjdAvBbI+e0ocS3MFEvzG6uBQE3xDk3SzynTn\n"
  "jh8BCNAw1FtxNrQHusEwMFxIt4I7mKZ9YIqioymCzLq9gwQbooMDQaHWBfEbwrbw\n"
  "qHyGO0aoSCqI3Haadr8faqU9GY/rOPNk3sgrDQoo//fb4hVC1CLQJ13hef4Y53CI\n"
  "rU7m2Ys6xt0nUW7/vGT1M0NPAgMBAAGjQjBAMA4GA1UdDwEB/wQEAwIBBjAPBgNV\n"
  "HRMBAf8EBTADAQH/MB0GA1UdDgQWBBR5tFnme7bl5AFzgAiIyBpY9umbbjANBgkq\n"
  "hkiG9w0BAQsFAAOCAgEAVR9YqbyyqFDQDLHYGmkgJykIrGF1XIpu+ILlaS/V9lZL\n"
  "ubhzEFnTIZd+50xx+7LSYK05qAvqFyFWhfFQDlnrzuBZ6brJFe+GnY+EgPbk6ZGQ\n"
  "3BebYhtF8GaV0nxvwuo77x/Py9auJ/GpsMiu/X1+mvoiBOv/2X/qkSsisRcOj/KK\n"
  "NFtY2PwByVS5uCbMiogziUwthDyC3+6WVwW6LLv3xLfHTjuCvjHIInNzktHCgKQ5\n"
  "ORAzI4JMPJ+GslWYHb4phowim57iaztXOoJwTdwJx4nLCgdNbOhdjsnvzqvHu7Ur\n"
  "TkXWStAmzOVyyghqpZXjFaH3pO3JLF+l+/+sKAIuvtd7u+Nxe5AW0wdeRlN8NwdC\n"
  "jNPElpzVmbUq4JUagEiuTDkHzsxHpFKVK7q4+63SM1N95R1NbdWhscdCb+ZAJzVc\n"
  "oyi3B43njTOQ5yOf+1CceWxG1bQVs5ZufpsMljq4Ui0/1lvh+wjChP4kqKOJ2qxq\n"
  "4RgqsahDYVvTH9w7jXbyLeiNdd8XM2w9U/t7y0Ff/9yi0GE44Za4rF2LN9d11TPA\n"
  "mRGunUHBcnWEvgJBQl9nJEiU0Zsnvgc/ubhPgXRR4Xq37Z0j4r7g1SgEEzwxA57d\n"
  "emyPxgcYxn/eR44/KJ4EBs+lVDR3veyJm+kXQ99b21/+jh5Xos1AnX5iItreGCc=\n"
  "-----END CERTIFICATE-----\n";


// RSA Public Key -
const char *OTA_PUBLIC_KEY =
  "-----BEGIN PUBLIC KEY-----\n"
  "MIICIjANBgkqhkiG9w0BAQEFAAOCAg8AMIICCgKCAgEAp2sRgMjD4wazKHo6Rk3g\n"
  "7APj7xsYGimOUKhWGWvd8wxFOEL+bHatAeKyCZXBjrZI9rdiUGtyHDdXS246YvV6\n"
  "dr56hJFT05n9iMHfx5T99NOyBl8Nm18dijqLGDSgVD9rC8v4aMrx5lSMrgzfz+bh\n"
  "HaDny6BFBHvaCQZW2KxXdnZElgnRidsoSR+5twdwdv9biHEwm4Miss1QQjmVZPVg\n"
  "SPIj4j9pf6wf8+kr57nEtjQORu+lLVj3OkM+A94N4k78+tR3gc3UMr1GvrqXm9D6\n"
  "VBH4OZDEgnx+QzraU9O60c6Jp/YXJkeLjRGFS7AaTd5+8sEFHoyx4BhvvxyUC7tX\n"
  "SpIZqziIlRjdJFE+8PjJ76u2BD1JmGeboUU+sxLJFiMpNeBskJG0kVwkFobkUtpN\n"
  "7O57CaGuzA8Eih+x3jTO85CiqgsK7rRckFyvMin1AgIvvxN9CUY+jTYvfcRdUO5Y\n"
  "7xQ8bFCvNUnpmqlWRveq3cxNZpNjOf99NinLJ6ewuaBqYXTtYbNCeAO+l0IiEl74\n"
  "c0HALRefCJO9mHmSoHsYJ7RRTLKhNO7jvoo7B+qCb2JDUKhwaLEetIDF6nw0dH22\n"
  "hONJuBOYktuwhrrsupJyBWYHHFmnFhqBcXCuTiPDhpcW05/3oAwApS7SpN8bN7dp\n"
  "SXQcqfsev1ke5IRosYpCv6cCAwEAAQ==\n"
  "-----END PUBLIC KEY-----\n";


// pre-setup stuff
// onewire    Data wire is connetec to the Arduino digital pin 13
#define ONE_WIRE_BUS 13
// Setup a oneWire instance to communicate with any OneWire devices
OneWire oneWire(ONE_WIRE_BUS);
// Pass our oneWire reference to Dallas Temperature sensor
DallasTemperature sensors(&oneWire);
// near your other statics:
DeviceAddress tempDeviceAddress;
//TEMPTASK DEBUG
volatile uint32_t tempReadSuccessCount = 0;
volatile uint32_t tempReadFailCount = 0;
volatile uint32_t tempCrcFailCount = 0;
volatile uint32_t tempCrcRecoveredCount = 0;
volatile uint32_t tempAllFFCount = 0;
volatile uint32_t tempPowerOn85Count = 0;
volatile uint32_t tempOutOfRangeCount = 0;
volatile uint32_t tempRequestFailCount = 0;
volatile uint32_t tempConnectedFailCount = 0;
volatile uint32_t tempResolutionFixCount = 0;
volatile uint32_t tempRereadFailCount = 0;
volatile uint32_t tempResolutionFixCrcFailCount = 0;
volatile uint32_t tempEnumerateFailCount = 0;
volatile uint32_t tempCoreBusySkipCount = 0;
volatile uint32_t tempStaleSkipCount = 0;
volatile float tempLastGoodF = -99.0f;
volatile unsigned long tempLastSuccessMillis = 0;


//VictronEnergy
VeDirectFrameHandler myve;
// WIFI STUFF
// WiFi wake button functionality (GPIO5)
int WiFiWakeButton = 0;                           // Current button state (1=pressed)
unsigned long wifiWakeButtonPressTime = 0;        // Timestamp when button pressed
const unsigned long WIFI_WAKE_DURATION = 300000;  // 5 minutes
bool wifiWakeActive = false;                      // Tracking wake mode state
unsigned long wifiWakeTimeout = 0;                // When WiFi should turn off (0 = inactive)

AsyncWebServer server(80);                  // Create AsyncWebServer object on port 80
AsyncEventSource events("/events");         // Create an Event Source on /events
unsigned long webgaugesinterval = 200;      // delay in ms between sensor updates on webpage
int plotTimeWindow = 120;                   // Plot time window in seconds
unsigned long healthystuffinterval = 5000;  // check hardware health parameters only every 5 seconds, not that they consume much   THIS IS DEAD CODE, REMOVE LATER

// WiFi provisioning settings
const char *WIFI_SSID_FILE = "/ssid.txt";
const char *WIFI_PASS_FILE = "/pass.txt";
// Cached WiFi client credentials (loaded once, reused for reconnects)
char cached_wifi_ssid[33] = "";  // 32 + null
char cached_wifi_pass[65] = "";  // 64 + null
bool cached_wifi_creds_valid = false;


typedef struct {
  unsigned long PGN;
  void (*Handler)(const tN2kMsg &N2kMsg);
} tNMEA2000Handler;

void SystemTime(const tN2kMsg &N2kMsg);
void Rudder(const tN2kMsg &N2kMsg);
void Speed(const tN2kMsg &N2kMsg);
void WaterDepth(const tN2kMsg &N2kMsg);
void DCStatus(const tN2kMsg &N2kMsg);
void BatteryConfigurationStatus(const tN2kMsg &N2kMsg);
void COGSOG(const tN2kMsg &N2kMsg);
void GNSS(const tN2kMsg &N2kMsg);
void Attitude(const tN2kMsg &N2kMsg);
void Heading(const tN2kMsg &N2kMsg);
void GNSSSatsInView(const tN2kMsg &N2kMsg);
void WindSpeed(const tN2kMsg &N2kMsg);

//PGN Handler Table
tNMEA2000Handler NMEA2000Handlers[] = {
  { 126992L, &SystemTime },
  { 127245L, &Rudder },
  { 127250L, &Heading },
  { 127257L, &Attitude },
  { 127506L, &DCStatus },
  { 127513L, &BatteryConfigurationStatus },
  { 128259L, &Speed },
  { 128267L, &WaterDepth },
  { 129026L, &COGSOG },
  { 129029L, &GNSS },
  { 129540L, &GNSSSatsInView },
  { 130306L, &WindSpeed },
  { 0, 0 }
};
// Stream *OutputStream; // old, i think it worked at one point..
Stream *OutputStream = &Serial;  // safe, correct

//ADS1115 more pre-setup crap
uint32_t adsI2CErrorCount = 0;

enum ADS1115_State {
  ADS_IDLE,
  ADS_WAIT,
  ADS_READ_RESULT
};

ADS1115_State adsState = ADS_IDLE;
uint8_t adsCurrentChannel = 0;  // Driven by adsSeq[] = {0,1,0,1,2,3}; CH1 fires 2× per cycle (~16.7 Hz)
int adsTriggeredChannel = 0;
unsigned long adsStateEntered = 0;
const unsigned long ADS_TIMEOUT_MS = 50;

volatile bool ch1FreshFlag = false;  // Set when CH1 result is ready, consumed by AdjustFieldLearnMode()

uint8_t PidSampleDivisor = 1;  // 1=PID runs every CH1 sample (~16.7 Hz / 60ms), 2=every other (~8.3 Hz), etc.
                               // CH1 fires at positions 1 and 3 in adsSeq[] {0,1,0,1,2,3};
                               // 6 steps × ~20ms/step = 120ms cycle → CH1 every ~60ms

const uint16_t adsMuxCodes[4] = {
  ADS1115_REG_CONFIG_MUX_SINGLE_0,
  ADS1115_REG_CONFIG_MUX_SINGLE_1,
  ADS1115_REG_CONFIG_MUX_SINGLE_2,
  ADS1115_REG_CONFIG_MUX_SINGLE_3
};

// Forward declarations (for WiFi functions)
String readFile(fs::FS &fs, const char *path);
void writeFile(fs::FS &fs, const char *path, const char *message);
void setupWiFi();
bool connectToWiFi(const char *ssid, const char *password, unsigned long timeout);
void setupAccessPoint();
void setupWiFiConfigServer();
void dnsHandleRequest();
void HandleNMEA2000Msg(const tN2kMsg &N2kMsg);
// (and other functions)
void performOTAUpdateToVersion(const char *targetVersion);
void performOTAUpdate(const UpdateInfo &updateInfo);

// HTML for the WiFi configuration page with traditional concatenation
const char WIFI_CONFIG_HTML[] PROGMEM =
  "<!DOCTYPE html>"
  "<html><head>"
  "<title>WiFi Configuration</title>"
  "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">"
  "<style>"
  "body{font-family:Arial;padding:20px;background:#f5f5f5}"
  ".card{background:white;padding:20px;border-radius:8px;max-width:400px;margin:0 auto}"
  "h1{color:#333;margin-bottom:20px;text-align:center}"
  "input,select{width:100%;padding:8px;margin:5px 0;border:1px solid #ddd;border-radius:4px}"
  "button{background:#00a19a;color:white;padding:10px 20px;border:none;border-radius:4px;cursor:pointer;width:100%}"
  "button:hover{background:#008c86}"
  ".info-box{background:#e8f4f8;border:1px solid #bee5eb;color:#0c5460;padding:12px;border-radius:4px;margin:10px 0;font-size:14px}"
  "</style>"
  "</head><body>"

  "<div class=\"card\">"
  "<h1>WiFi Configuration</h1>"

  "<div class=\"info-box\">"
  "<strong>Preferred Option:</strong><br>"
  "Enter your ship's network WiFi credentials below. The regulator will henceforth run in Client Mode and the user interface will be accessible via your local wifi.  Navigate to alternator.local in any browser, just like you'd go to google.com."
  "</div>"

  "<form action=\"/wifi\" method=\"POST\">"
  "<label>Ship's WiFi Name (SSID):</label>"
  "<input type=\"text\" name=\"ssid\" placeholder=\"Required for client mode\">"
  "<label>Ship's WiFi Password:</label>"
  "<input type=\"password\" name=\"password\" placeholder=\"Required for client mode\">"

  "<div class=\"info-box\">"
  "<strong>Non-Preferred Option:</strong><br>"
  "As backup, or for ships without existing WiFi networks, you may use the regualtor as a Hotspot (aka Access Point). The regulator controller will broadcast a new WiFi network which you can connect to from any device.  The same interface and functionality will exist at alternator.local, but due to lack of internet connection, you won't be able to use Solar mode, get software updates, see Community features, etc.  This mode is less supported.  To enter this mode on a reboot, you must connect pin 12 in RJ3 (the rightmost ethernet connector, blue wire) to Ground.  I recommend to just leave it connected forever if you prefer this mode."
  "</div>"

  "<label>New Alt. Reg. Hotspot Name (SSID):</label>"
  "<input type=\"text\" name=\"hotspot_ssid\" placeholder=\"Leave blank for default: ALTERNATOR_WIFI\">"
  "<label>New Alt. Reg. Hotspot Password:</label>"
  "<input type=\"password\" name=\"ap_password\" placeholder=\"Leave blank for default: alternator123\">"

  "<div class=\"info-box\">"
  "***To boot into Hotspot mode, the Hotspot Wire must be connected to Ground during a restart.***"
  "</div>"

  "<div class=\"info-box\">"
  "To return to this Wifi Configuration page at any time, connect the Wifi Configuration Wire to Ground during a restart."
  "</div>"

  "<button type=\"submit\">Save Configuration</button>"

  "<div class=\"info-box\">"
  "After saving, this page may become unresponsive or disappear. In any case, wait 20 seconds, then reconnect to your chosen network to access the full alternator interface."
  "</div>"
  "</form>"
  "</div>"
  "</body></html>";

void setup() {

  Serial.end();  // Force complete serial restart (might not need this delete later)
  delay(100);
  Serial.begin(230400);  //note, the below settings only apply to Serial, not Serial1 or Serial2
  // Serial.setTxTimeoutMs(500);    // avoid long stalls on any possible big serial monitor bursts     DOESN'T WORK ON NEW ESP32 BOARD DEFINITION IN ARDUINO IDE

  // ========== ENABLE DETAILED CRASH DUMPS ==========
  // esp_log_level_set("*", ESP_LOG_VERBOSE);  // Verbose logging.  May want this later, but adds massive noise to SSL/HTP stuff, may change timing

  Serial.setTxBufferSize(4096);  // 4 KB TX in internal RAM
  Serial.setRxBufferSize(2048);  // optional, we don't currently Rx anyway...
  delay(200);
  Serial.println("\n\n=== SYSTEM STARTUP ===");
  delay(200);
  // Allocate buffers from PSRAM
  configPayloadBuffer = (char *)ps_malloc(CONFIG_PAYLOAD_SIZE);
  payloadBuffer = (char *)ps_malloc(PAYLOAD_BUFFER_SIZE);
  tempBuffer = (char *)ps_malloc(PAYLOAD_BUFFER_SIZE);
  filenameBuffer = (char *)ps_malloc(FILENAME_BUFFER_SIZE);
  timestampBuffer = (char *)ps_malloc(TIMESTAMP_BUFFER_SIZE);
  messageBuffer = (char *)ps_malloc(MESSAGE_BUFFER_SIZE);
  consoleQueue = (ConsoleMessage *)ps_malloc(CONSOLE_QUEUE_SIZE * sizeof(ConsoleMessage));
  if (consoleQueue) memset(consoleQueue, 0, CONSOLE_QUEUE_SIZE * sizeof(ConsoleMessage));
  taskArray = (TaskStatus_t *)ps_malloc(MAX_TASKS * sizeof(TaskStatus_t));
  // CH1 interval ring — 30 KB to PSRAM
  ch1Ring = (Ch1Entry *)ps_malloc(sizeof(Ch1Entry) * CH1_RING);
  if (!ch1Ring) Serial.println("FATAL: ch1Ring ps_malloc failed");
  else memset(ch1Ring, 0, sizeof(Ch1Entry) * CH1_RING);
  // Allocate currentWindow to PSRAM — SensorWindow has non-zero defaults (999900 sentinels)
  Serial.printf("Free PSRAM before window allocs: %u bytes\n", ESP.getFreePsram());
  currentWindow = (SensorWindow *)ps_malloc(sizeof(SensorWindow));
  if (!currentWindow) {
    Serial.println("FATAL: currentWindow ps_malloc failed");
    while (1) delay(100);
  } else new (currentWindow) SensorWindow();
  // Allocate imuWindow to PSRAM — ImuWindow has no non-zero defaults
  imuWindow = (ImuWindow *)ps_malloc(sizeof(ImuWindow));
  if (!imuWindow) {
    Serial.println("FATAL: imuWindow ps_malloc failed");
    while (1) delay(100);
  } else memset(imuWindow, 0, sizeof(ImuWindow));

  // IMU ring buffer — ~30 KB to PSRAM
  imuRingBuffer = (ImuRingBuffer *)ps_malloc(sizeof(ImuRingBuffer));
  if (!imuRingBuffer) Serial.println("FATAL: imuRingBuffer ps_malloc failed");
  else memset(imuRingBuffer, 0, sizeof(ImuRingBuffer));
  size_t loopStackBytes = getArduinoLoopTaskStackSize();
  UBaseType_t loopHighWaterBytes = uxTaskGetStackHighWaterMark(NULL);  // bytes on ESP32-S3

  Serial.printf("Configured Arduino loop stack: %u bytes\n", (unsigned)loopStackBytes);
  Serial.printf("Loop min free so far (high-water): %u bytes\n", (unsigned)loopHighWaterBytes);

  Serial.println("\n=== LOOP TASK STACK INFO ===");
  Serial.printf("High-water at setup(): %u bytes\n", (unsigned)loopHighWaterBytes);
  Serial.println();
  // === STACK SIZE VERIFICATION ===
  // Check immediately - this should show nearly full stack
  UBaseType_t stackRemainingNow = uxTaskGetStackHighWaterMark(NULL);
  Serial.println("\n=== LOOP TASK STACK INFO ===");
  Serial.printf("Stack remaining at setup(): %u bytes\n", stackRemainingNow);
  Serial.println();
  initializeDeviceId();    // Sets the UID (real or spoofed)
  checkDeviceUIDChange();  // Compares to last boot, COMMENT THIS OUT DUE TO AVOID NEED TO REREGISTER DURING TESTING.  NEEDS UNCOMMENTING WHEN DEVICE ID CHANGES!!
  delay(200);
  loadAuthToken();  // Loads token (will be empty if just cleared)  // for supabase
  delay(200);
  Serial.flush();  // Ensure it's sent before continuing
  int major = 0, minor = 0, patch = 0;
  // Parse FIRMWARE_VERSION directly (e.g. "0.1.65")
  const char *version = FIRMWARE_VERSION;
  int len = strlen(version);

  // Find first dot
  int firstDot = -1;
  for (int i = 0; i < len; i++) {
    if (version[i] == '.') {
      firstDot = i;
      break;
    }
  }
  // Find second dot
  int secondDot = -1;
  for (int i = firstDot + 1; i < len; i++) {
    if (version[i] == '.') {
      secondDot = i;
      break;
    }
  }
  // Parse each part
  if (firstDot > 0 && secondDot > firstDot) {
    // Parse major (0 to firstDot)
    for (int i = 0; i < firstDot; i++) {
      major = major * 10 + (version[i] - '0');
    }
    // Parse minor (firstDot+1 to secondDot)
    for (int i = firstDot + 1; i < secondDot; i++) {
      minor = minor * 10 + (version[i] - '0');
    }
    // Parse patch (secondDot+1 to end)
    for (int i = secondDot + 1; i < len; i++) {
      patch = patch * 10 + (version[i] - '0');
    }
  }

  firmwareVersionInt = major * 10000 + minor * 100 + patch;
  Serial.print("Firmware Version Int: ");
  Serial.println(firmwareVersionInt);
  Serial.println();
  printPartitionInfo();  // DELETE LATER, just used to prove the Custom scheme works
  initializeNVS();
  //RestoreLastSessionValues();  // just used for ESP32 stats from last session  REMOVED claimed to be redundant.
  loadFuelTableFromNVS();

  if (!ensureLittleFS()) {
    Serial.println("CRITICAL: Cannot continue without filesystem");
    Serial.println();
    while (true)
      ;  // halt
  } else {
    initSensorBuffer();
  }
  delay(500);
  checkWebFilesExist();
  sessionStartTime = millis();
  // Reset all per-function timing structs and session loop metrics at boot
  MaximumLoopTime = 0;
  loopTime5sWindow = 0;
  memset(&ft_ReadAnalogInputs, 0, sizeof(FuncTiming));
  memset(&ft_saveNVSData, 0, sizeof(FuncTiming));
  memset(&ft_AdjustFieldLearnMode, 0, sizeof(FuncTiming));
  memset(&ft_logDashboardValues, 0, sizeof(FuncTiming));
  memset(&ft_printSystemHealth, 0, sizeof(FuncTiming));
  memset(&ft_checkWiFiConnection, 0, sizeof(FuncTiming));
  memset(&ft_SendWifiData, 0, sizeof(FuncTiming));
  memset(&ft_CheckAlarms, 0, sizeof(FuncTiming));
  memset(&ft_calculateDerivedMetrics, 0, sizeof(FuncTiming));
  memset(&ft_ch1_compute_stats, 0, sizeof(FuncTiming));
  memset(&ft_uploadSensorHistory, 0, sizeof(FuncTiming));
  memset(&ft_uploadBufferedRecords, 0, sizeof(FuncTiming));
  memset(&ft_buildConfigPayload, 0, sizeof(FuncTiming));
  memset(&ft_UpdateEngineRuntime, 0, sizeof(FuncTiming));
  memset(&ft_UpdateEngineFuel, 0, sizeof(FuncTiming));
  memset(&ft_UpdateBatterySOC, 0, sizeof(FuncTiming));
  memset(&ft_UpdateTravelStatistics, 0, sizeof(FuncTiming));
  memset(&ft_UpdateDistanceThisInterval, 0, sizeof(FuncTiming));
  memset(&ft_UpdateBoardTempPressureMaximums, 0, sizeof(FuncTiming));
  memset(&ft_handleSocGainReset, 0, sizeof(FuncTiming));
  memset(&ft_handleAltZeroReset, 0, sizeof(FuncTiming));
  memset(&ft_calculateChargeTimes, 0, sizeof(FuncTiming));
  memset(&ft_UpdateSailingMetrics, 0, sizeof(FuncTiming));
  memset(&ft_updateWeatherMode, 0, sizeof(FuncTiming));
  memset(&ft_updateSensorWindow, 0, sizeof(FuncTiming));
  memset(&ft_checkTimeSync, 0, sizeof(FuncTiming));
  memset(&ft_ReadVEData, 0, sizeof(FuncTiming));
  memset(&ft_FlushFileWriteQueue, 0, sizeof(FuncTiming));


  captureResetReason();            // immediately capture the reason for last ESP32 shutdown and store in LittleFS and variable that won't be overwritten until next boot
  ensurePreferredBootPartition();  // Ensure we boot from preferred partition
  loadNVSData();                   // Load persistent variables from NVS- everything from last session is restored
  initNVSCache();                  // Sync change-detection cache with loaded NVS values to prevent false writes
  //Reset some parameters to zero since we are re-starting on a re-boot
  CurrentSessionDuration = 0;
  prevSessionMaxLoopTime = MaxLoopTime;  // snapshot last session's worst before zeroing
  MaxLoopTime = 0;                       // reset for this session (persists to NVS on next save)
  totalPowerCycles++;
  saveNVSData();  // Save immediately to persist the adjustments done above in setup so far
  initEfficiencyTracker();
  setCpuFrequencyMhz(240);
  pinMode(4, OUTPUT);     // This pin is used to provide a high signal to Field Enable pin
  digitalWrite(4, LOW);   // Start with field off
  pinMode(5, INPUT);      // WiFi wake button
  pinMode(2, OUTPUT);     // This pin is used to provide a field PWM indicator (pin 2 of ESP32 is the LED)
  pinMode(1, INPUT);      // Ignition
  pinMode(21, OUTPUT);    // Alarm/Buzzer output (was 33)
  digitalWrite(21, LOW);  // Start with alarm off
  alarmOutputState = false;
  pinMode(42, INPUT);     // bmsLogic
  // PWM setup (needed for basic operation)
  //ledcAttach(pwmPin, SwitchingFrequency, pwmResolution);

  DefaultHeaders::Instance().addHeader("Access-Control-Allow-Origin", "*");
  // If you have custom headers, also:
  DefaultHeaders::Instance().addHeader("Access-Control-Allow-Headers", "*");

  InitSystemSettings();       // load all settings from LittleFS.  If no files exist, create them.
  initWeatherModeSettings();  // Add weather mode settings--- otherwise similar to line above (InitSystemSettings)
  loadPasswordHash();
  // Check if we should wake WiFi for a pending OTA update
  nvs_handle_t wake_handle;
  if (nvs_open("update_req", NVS_READONLY, &wake_handle) == ESP_OK) {
    uint8_t wakeFlag = 0;
    if (nvs_get_u8(wake_handle, "wake_flag", &wakeFlag) == ESP_OK && wakeFlag == 1) {
      wifiWakeTimeout = millis() + WIFI_WAKE_DURATION;
      queueConsoleMessage("UPDATE: WiFi wake enabled for pending update (5 min)");

      // Clear the wake flag immediately
      nvs_close(wake_handle);
      nvs_handle_t clear_wake;
      if (nvs_open("update_req", NVS_READWRITE, &clear_wake) == ESP_OK) {
        nvs_erase_key(clear_wake, "wake_flag");
        nvs_commit(clear_wake);
        nvs_close(clear_wake);
      }
    } else {
      nvs_close(wake_handle);
    }
  }

  resetSensorWindow();  // Initialize sensor window (Cloud Features My History, starts hi/low water marks all fresh)
  resetAccelWindow();
  setupWiFi();  // NOW setup WiFi with all settings properly loaded

  // ===== CREATE HTTPS TASK ON CORE 0 =====
  httpsQueue = xQueueCreate(2, sizeof(HttpsRequest));  // Smaller queue, bigger messages
  Serial.println(httpsQueue ? "HTTPS queue created" : "ERROR: Queue creation failed");

  // Both stay on Core 0, but priority > 0
  xTaskCreatePinnedToCore(TempTask, "TempTask", 4096, NULL, 1, &tempTaskHandle, 0);
  Serial.println("Temp task created on Core 0");

  xTaskCreatePinnedToCore(httpsTask, "HTTPS", 20480, NULL, 1, &httpsTaskHandle, 0);
  Serial.println("HTTPS task created on Core 0");

  if (currentMode == MODE_CLIENT && WiFi.status() == WL_CONNECTED) {
    Serial.println("Starting NTP sync...");
    syncTimeFromNTP();
    Serial.printf("Time synced. Timestamp: %lu\n", getCurrentTimestamp());
  }


  esp_log_level_set("esp32-hal-i2c-ng", ESP_LOG_WARN);
  queueConsoleMessage("System starting up...");

  if (hardwarePresent == 1) {
    initializeHardware();  // Initialize hardware systems
  };

  // Enable watchdog - only after setup is complete
  // What Happens During Watchdog Reset:
  //Watchdog triggers (after 16 seconds of hang)
  //ESP32 immediately reboots (hardware reset)
  //ALL GPIO pins reset to 0 (including pin 4 field enable)
  //Field control pin 4 goes LOW → Field turns OFF
  //Alternator output stops → Batteries safe
  //ESP32 restarts and runs setup()
  //Normal operation resumes

  // Enable watchdog - only after setup is complete
  Serial.println();
  Serial.println("Enabling watchdog protection...");

  // Configure 16s watchdog
  esp_task_wdt_config_t wdt_config = {
    .timeout_ms = 16000,
    .idle_core_mask = 0,
    .trigger_panic = true
  };

  // Try reconfigure first (TWDT likely already exists on core 3.x)
  esp_err_t result = esp_task_wdt_reconfigure(&wdt_config);

  if (result == ESP_OK) {
    Serial.println("✅ Watchdog reconfigured: 16s timeout");
  } else if (result == ESP_ERR_INVALID_STATE) {
    // Not initialized, init it
    result = esp_task_wdt_init(&wdt_config);
    if (result == ESP_OK) {
      Serial.println("✅ Watchdog initialized: 16s timeout");
    } else {
      Serial.printf("❌ Watchdog init failed: %s\n", esp_err_to_name(result));
    }
  } else {
    Serial.printf("❌ Watchdog reconfigure failed: %s\n", esp_err_to_name(result));
  }

  // Add main task
  if (result == ESP_OK) {
    esp_err_t add_result = esp_task_wdt_add(NULL);
    if (add_result == ESP_OK) {
      Serial.println("✅ Main task added to watchdog");
      queueConsoleMessage("Watchdog: 16s timeout active");
    } else {
      Serial.printf("⚠️ Failed to add main task: %s\n", esp_err_to_name(add_result));
    }
  } else {
    queueConsoleMessage("Watchdog: Failed to configure");
  }
  loadLearningTableFromNVS();  // Load all table data at boot
  Serial.println();
  // Force initial sensor readings before main loop starts
  delay(100);  // Brief settling time
  if (hardwarePresent == 1) {
    ReadAnalogInputs();
    delay(100);          // Give it a moment to process
    ReadAnalogInputs();  // Second reading to be sure
  }

  const esp_partition_t *running_partition = esp_ota_get_running_partition();
  const esp_partition_t *factory_partition = esp_partition_find_first(ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_APP_FACTORY, NULL);
  if (running_partition == factory_partition) {
    currentPartitionType = 0;  // Factory
    Serial.println("=== We are in factory partition! ===");
  } else {
    currentPartitionType = 1;  // OTA_0 (production)
    Serial.println("=== We are in OTA partition! ===");
  }
  tempPID_init();
  thermalLog_init();
  pidLog_init();
  cvLog_init();
  Serial.println("=== SETUP COMPLETE ===");
}

void loop() {

  esp_task_wdt_reset();
  Ignition = !digitalRead(1);  // ! is for optocoupler
  if (IgnitionOverride == 1) {
    Ignition = 1;
  }
  // static DeviceMode lastMode = MODE_CONFIG;  //DEBUG REMOVE LATER
  // if (currentMode != lastMode) {
  //   Serial.printf("MODE CHANGED (0=CONFIG, 1= AP, 2 = CLIENT): %d -> %d\n", lastMode, currentMode);
  //   lastMode = currentMode;
  // }                                  //END DEBUG
  WiFiWakeButton = !digitalRead(5);  // Read WiFi wake button state. Active LOW (button pulls to ground)

  // Button press extends/starts timeout
  if (WiFiWakeButton == 1) {
    wifiWakeTimeout = millis() + WIFI_WAKE_DURATION;  // Reset to 5min from now
  }

  // === OTA UPDATE CHECK - USER INITIATED UPDATE FROM NVS ===
  static bool manualUpdateCheckDone = false;  // ← Changed name
  static char pendingVersion[64] = { 0 };

  if (!manualUpdateCheckDone && millis() > 5000 && currentMode == MODE_CLIENT) {
    if (checkForPendingUpdateNonBlocking(pendingVersion)) {
      Serial.printf("UPDATE: Found pending update for %s\n", pendingVersion);

      // Don't wait for core0 - we're shutting it down anyway!
      clearPendingUpdateNVS();  // Clear NVS flags to prevent boot loop

      // Force core0 to not be busy (we're taking over)
      core0Busy = true;

      performOTAUpdateToVersion(pendingVersion);  // This calls prepareForOTA() which kills tasks
    }
    manualUpdateCheckDone = true;  // ← Changed here too
  }
  // === END OTA MANUAL UPDATE CHECK ===

  // === OTA UPDATE: simple one-shot FORCED UPDATE check after 3s ===
  static bool otaCheckDone = false;

  if (!otaCheckDone && millis() > 3000) {  // 3 seconds after boot
    if (currentMode == MODE_CLIENT && WiFi.status() == WL_CONNECTED && isRegistered) {
      // Only proceed if no uploads in progress
      if (!core0Busy) {
        Serial.println("OTA: Running one-time firmware/version checks after 3s...");

        debugStackBeforeHTTPS("updateFirmwareVersionInSupabase");
        if (WiFi.RSSI() >= -76) {
          HttpsRequest req = { .type = HTTPS_UPDATE_FW_VERSION };
          xQueueSend(httpsQueue, &req, 0);
        }

        debugStackBeforeHTTPS("checkForForcedUpdate");
        if (WiFi.RSSI() >= -76) {
          HttpsRequest req = { .type = HTTPS_CHECK_FORCED_UPDATE };
          xQueueSend(httpsQueue, &req, 0);
        }

        otaCheckDone = true;
      }
      // If blocked, will retry next loop iteration
    } else {
      // Still not ready (no WiFi or not client mode) – log once and give up
      Serial.println("OTA: Skipping one-time checks - not in CLIENT mode or no WiFi/registration");
      otaCheckDone = true;
    }
  }
  // // === END OTA UPDATE ===
  esp_task_wdt_reset();              // Feed the watchdog to prevent timeout
  starttime = esp_timer_get_time();  // Record start time for Loop
  currentTime = millis();

  // SOC and runtime update every 2 seconds (runs regardless of hardwarePresent)
  if (currentTime - lastSOCUpdateTime >= SOCUpdateInterval) {
    CurrentSessionDuration = (millis() - sessionStartTime) / 1000 / 60;  //minutes
    elapsedMillis = currentTime - lastSOCUpdateTime;
    lastSOCUpdateTime = currentTime;
    TIMED_CALL(ft_UpdateEngineRuntime, UpdateEngineRuntime(elapsedMillis));
    TIMED_CALL(ft_UpdateEngineFuel, UpdateEngineFuel(elapsedMillis));  //
    TIMED_CALL(ft_UpdateBatterySOC, UpdateBatterySOC(elapsedMillis));
    TIMED_CALL(ft_UpdateTravelStatistics, UpdateTravelStatistics(elapsedMillis));       //
    TIMED_CALL(ft_UpdateDistanceThisInterval, UpdateDistanceThisInterval());            // NEW
    TIMED_CALL(ft_UpdateBoardTempPressureMaximums, UpdateBoardTempPressureMaximums());  // NEW
    TIMED_CALL(ft_handleSocGainReset, handleSocGainReset());                            // do the dynamic updates
    TIMED_CALL(ft_handleAltZeroReset, handleAltZeroReset());                            // do the dynamic udpates
  }
  TIMED_CALL(ft_calculateChargeTimes, calculateChargeTimes());  // might want to put this in the above if statement and unthrottle at some point update later
  TIMED_CALL(ft_saveNVSData, saveNVSData());                    // Only save current operational data, not session stats
  TIMED_CALL(ft_saveNVSData, saveNVSData());
  TIMED_CALL(ft_FlushFileWriteQueue, FlushFileWriteQueue());
  // ========== POWER MANAGEMENT: Handle ignition state and WiFi wake mode ==========
  // This runs BEFORE the mode switch to ensure WiFi is in correct state before attempting transmission
  // Power management affects AP and CLIENT modes, but NOT CONFIG mode (CONFIG mode exits early below)
  if (currentMode != MODE_CONFIG) {
    // Ignition state tracking variables
    static int lastIgnitionState = 1;            // Track previous ignition state (-1 = uninitialized)
    static bool lastWifiWakeActive = false;      // Track previous WiFi wake mode state
    static bool wakeExpiryWarningShown = false;  // Track if we've warned about wake mode expiring

    if (Ignition == 0) {
      // ===== IGNITION OFF =====
      wifiWakeActive = (millis() < wifiWakeTimeout);
      // Detect state change for WiFi wake mode
      if (wifiWakeActive != lastWifiWakeActive) {
        if (wifiWakeActive) {
          Serial.println("WiFi Wake Mode ACTIVATED (GPIO5 triggered)");
        } else {
          Serial.println("WiFi Wake Mode DEACTIVATED - Entering low power mode");
          wakeExpiryWarningShown = false;  // Reset warning flag for next wake cycle
        }
        lastWifiWakeActive = wifiWakeActive;
      }
      if (wifiWakeActive) {
        // Wake mode active - full power for user monitoring
        setCpuFrequencyMhz(240);
        if (WiFi.getMode() == WIFI_OFF) {  // WiFi was turned off, need to reconnect
          setupWiFi();
        }
        // Warn user before timeout expires
        if ((wifiWakeTimeout - millis()) < 20000 && !wakeExpiryWarningShown) {
          queueConsoleMessage("WiFi wake mode expiring in 20 seconds - press button to extend");
          wakeExpiryWarningShown = true;
        }
      } else {
        // Low power mode - save battery
        WiFi.mode(WIFI_OFF);  // THIS MUST BE DONE FIRST
        if (tempTaskHandle != NULL) {
          vTaskSuspend(tempTaskHandle);  // SUSPEND BACKGROUND TASKS BEFORE SLOWING CPU
        }
        setCpuFrequencyMhz(80);  // THIS MUST BE DONE SECOND
      }

      // Detect ignition turning OFF
      if (lastIgnitionState == 1) {
        Serial.println("Ignition OFF");
        lastIgnitionState = 0;
      }

    } else {
      // ===== IGNITION ON - Normal operation =====
      // Detect ignition state change to ON
      if (lastIgnitionState != 1) {
        Serial.println("Ignition ON - Normal operation mode");
        lastIgnitionState = 1;
      }
      wifiWakeTimeout = 0;             // Clear wake mode
      lastWifiWakeActive = false;      // Reset wake mode tracking
      wakeExpiryWarningShown = false;  // Reset warning
      setCpuFrequencyMhz(240);         // Ensure full speed
      if (tempTaskHandle != NULL) {
        vTaskResume(tempTaskHandle);  // RESUME BACKGROUND TASKS AFTER SPEEDING UP CPU
      }
      // Reconnect WiFi if needed (works for both AP and CLIENT modes)
      if (WiFi.getMode() == WIFI_OFF) {
        setupWiFi();  // Reconnects in current mode
      }
    }
  }
  // ========== END POWER MANAGEMENT ==========

  // ========== MODE HANDLING: Switch statement runs for all modes ==========
  switch (currentMode) {
    case MODE_CONFIG:
      // Configuration mode - WiFi setup ONLY, no alternator operation
      // This is intentional for safety: user must configure WiFi before alternator use
      // EMERGENCY ACCESS: Hold GPIO46 LOW during boot to enter AP mode (full functionality without credentials)
      dnsHandleRequest();  // Process captive portal DNS requests
        // Throttle this later , it's not so trivial    Serial.println("You're in Config Mode, connect to the alternator's hotspot and enter credentials");
      return;  // Exit loop() entirely - prevents alternator operation without proper configuration
    case MODE_AP:
      // Full functionality - same as client mode, plus DNS for captive portal
      dnsHandleRequest();  // Need DNS for captive portal in AP mode
      // Fall through to full functionality (no break statement - intentional)

    case MODE_CLIENT:
      // Full sensor/data functionality (also runs for MODE_AP due to fall-through above)

      // ========== HARDWARE-DEPENDENT SECTION ==========
      // Read sensors: Real hardware or fake data depending on hardwarePresent flag
      if (hardwarePresent == 1) {
        checkTempTaskHealth();  // digital temperature measurement monitor (only with real hardware)

        TIMED_CALL(ft_ReadAnalogInputs, ReadAnalogInputs());
      } else {
        imuEnabled = true;        //hack but it works for now
        ReadAnalogInputs_Fake();  // Fake sensor readings for development
        // Notify user every 60 seconds that we're in fake mode
        static unsigned long lastFakeWarning = 0;
        if (millis() - lastFakeWarning > 60000) {
          Serial.println("hardwarePresent=0: Using fake sensor data");
          if (WiFi.getMode() != WIFI_OFF) {
            events.send("hardwarePresent flag is 0 - using fake sensor data", "console", millis());
          }
          lastFakeWarning = millis();
        }
      }

      // ========== END HARDWARE-DEPENDENT SECTION ==========
      // All remaining code runs regardless of hardwarePresent flag
      // This allows full development/testing even with broken hardware
      TIMED_CALL(ft_calculateDerivedMetrics, calculateDerivedMetrics());  // Calculate true wind, leeway, VMG, duty cycles
      if (VeData == 1 && hardwarePresent == 1) {
        TIMED_CALL(ft_ReadVEData, ReadVEData());
        VeTime = ft_ReadVEData.worstWindow;
        VeTime2 = ft_ReadVEData.worstSession;
      }
      if (NMEA2KData == 1 && hardwarePresent == 1) {
        NMEA2000.ParseMessages();  // CAN bus (only with real hardware)
      }
      TIMED_CALL(ft_CheckAlarms, CheckAlarms());  // Process alarms (runs with fake or real data)
      calculateThermalStress();                   // alternator lifetime modeling (runs with fake or real data)
      //UpdateDisplay();
      checkAutoZeroTriggers();  //Auto-zero processing (must be before AdjustField)
      processAutoZero();        //Auto-zero processing (must be before AdjustField)

      if (currentMode == MODE_CLIENT && WiFi.status() == WL_CONNECTED) {
        TIMED_CALL(ft_updateWeatherMode, updateWeatherMode());
      }
      TIMED_CALL(ft_AdjustFieldLearnMode, AdjustFieldLearnMode());
      efficiencyTracker_tick();

      // Sync legacy display variables
      totalSafeHours = (float)totalSafeMs / 3600000.0f;  // Fractional hours for UI
      // Wrap-safe remaining time calculation
      if (overheatPenaltyEndMs > 0) {
        uint32_t now = millis();
        int32_t remaining = (int32_t)(overheatPenaltyEndMs - now);
        if (remaining > 0) {
          overheatingPenaltyTimer = (uint32_t)remaining;
        } else {
          overheatingPenaltyTimer = 0;
        }
      } else {
        overheatingPenaltyTimer = 0;
      }
      TIMED_CALL(ft_logDashboardValues, logDashboardValues());  //  nice to have some history in the Console
      TIMED_CALL(ft_printSystemHealth, printSystemHealth());
      TIMED_CALL(ft_updateSensorWindow, updateSensorWindow());  // Update sensor aggregation (after sensor reads)
      if (accelEnabled == 1) {
        updateAccelMetrics();  // accelerometer
      };

      if (CloudFeatures == 1) {
        if (otaInProgress) {
          return;  // Skip during OTA
        }
        unsigned long currentMillisz = millis();
        // Check time sync every 12 hours
        TIMED_CALL(ft_checkTimeSync, checkTimeSync());
        // Save current sensor window to buffer every SENSOR_UPLOAD_INTERVAL. All data goes to buffer first
        if (currentMillisz - lastSensorUploadTime >= SENSOR_UPLOAD_INTERVAL) {
          esp_task_wdt_reset();
          TIMED_CALL(ft_UpdateSailingMetrics, UpdateSailingMetrics(SENSOR_UPLOAD_INTERVAL));
          lastSensorUploadTime = currentMillisz;
          TIMED_CALL(ft_uploadSensorHistory, uploadSensorHistory());
          resetDistanceThisInterval();
          esp_task_wdt_reset();
        }

        // Upload buffered records every BUFFER_UPLOAD_INTERVAL minutes.  Reads file, uploads it, deletes on success
        if (currentMillisz - lastBufferUploadAttempt >= BUFFER_UPLOAD_INTERVAL - 7) {  // added 7 to give a tiny offset, not sure if useful or not
          lastBufferUploadAttempt = currentMillisz;
          if (bufferedRecordCount > 0) {
            esp_task_wdt_reset();                                           // Feed before upload
            TIMED_CALL(ft_uploadBufferedRecords, uploadBufferedRecords());  // times LittleFS read + queue send; HTTP transfer is on core 0 and not captured here
            esp_task_wdt_reset();                                           // Feed after upload completes
          }
        }
        //delay(3);  // removed 4/18/26, don't think it was ever necessary

        // Configuration Snapshot
        if (millis() - lastConfigSnapshotTime >= CONFIG_SNAPSHOT_INTERVAL) {
          lastConfigSnapshotTime = millis();
          if (currentMode == MODE_CLIENT && WiFi.status() == WL_CONNECTED && isRegistered) {
            if (WiFi.RSSI() >= -76) {
              bool _configBuilt;
              TIMED_CALL(ft_buildConfigPayload, _configBuilt = buildConfigPayload());  // time the payload build separately from the queue send
              if (_configBuilt) {
                HttpsRequest req = {};
                req.type = HTTPS_UPLOAD_CONFIG;
                strncpy(req.payload, configPayloadBuffer, sizeof(req.payload) - 1);
                req.payload[sizeof(req.payload) - 1] = '\0';
                xQueueSend(httpsQueue, &req, 0);
              }
            }
          }
        }
      }
      TIMED_CALL(ft_ch1_compute_stats, ch1_compute_stats());
      TIMED_CALL(ft_SendWifiData, SendWifiData());  // Safely sends data (has internal guard to check if WiFi is actually on)

      // Client-specific connection monitoring
      // if (currentMode == MODE_CLIENT) {  // moved the gating check into the checkwificonnection function WAS NOT SUFFICIENT FOR WHATEVER REASON
      if (currentMode == MODE_CLIENT && (Ignition == 1 || millis() < wifiWakeTimeout)) {
        // if (currentMode == MODE_CLIENT && (Ignition == 1 || wifiWakeActive)) { // can't try to do anything wifi related unless ignition is on and clock speed is fast enough to not crash
        TIMED_CALL(ft_checkWiFiConnection, checkWiFiConnection());

        // Track actual WiFi disconnects
        static unsigned long lastWiFiStatusCheck = 0;
        static bool lastWiFiConnected = true;

        if (millis() - lastWiFiStatusCheck > 5000) {
          bool currentlyConnected = (WiFi.status() == WL_CONNECTED);

          if (lastWiFiConnected && !currentlyConnected) {
            wifiDisconnectCount++;
            queueConsoleMessageF("WiFi disconnected #%u", wifiDisconnectCount);
          } else if (!lastWiFiConnected && currentlyConnected) {
            wifiReconnectsTotal++;
            queueConsoleMessageF("WiFi reconnected #%u", wifiReconnectsTotal);
          }

          lastWiFiConnected = currentlyConnected;
          lastWiFiStatusCheck = millis();
        }
      }
      break;  // Exit switch statement, continue with rest of loop()
  }
  // ========== LOOP TIMING METRICS ==========
  if (millis() - prev_millis7888 > AinputTrackerTime) {  // every 5 seconds reset the rolling window metrics
    loopTime5sWindow = 0;                                // rolling 5s loop worst resets here; MaximumLoopTime is now session-persistent
    // Reset all per-function rolling windows
    ft_ReadAnalogInputs.worstWindow = 0;
    ft_saveNVSData.worstWindow = 0;
    ft_AdjustFieldLearnMode.worstWindow = 0;
    ft_logDashboardValues.worstWindow = 0;
    ft_printSystemHealth.worstWindow = 0;
    ft_checkWiFiConnection.worstWindow = 0;
    ft_SendWifiData.worstWindow = 0;
    ft_CheckAlarms.worstWindow = 0;
    ft_calculateDerivedMetrics.worstWindow = 0;
    ft_ch1_compute_stats.worstWindow = 0;
    ft_uploadSensorHistory.worstWindow = 0;
    ft_uploadBufferedRecords.worstWindow = 0;
    ft_buildConfigPayload.worstWindow = 0;
    ft_UpdateEngineRuntime.worstWindow = 0;
    ft_UpdateEngineFuel.worstWindow = 0;
    ft_UpdateBatterySOC.worstWindow = 0;
    ft_UpdateTravelStatistics.worstWindow = 0;
    ft_UpdateDistanceThisInterval.worstWindow = 0;
    ft_UpdateBoardTempPressureMaximums.worstWindow = 0;
    ft_handleSocGainReset.worstWindow = 0;
    ft_handleAltZeroReset.worstWindow = 0;
    ft_calculateChargeTimes.worstWindow = 0;
    ft_UpdateSailingMetrics.worstWindow = 0;
    ft_updateWeatherMode.worstWindow = 0;
    ft_updateSensorWindow.worstWindow = 0;
    ft_checkTimeSync.worstWindow = 0;
    ft_rai_total.worstWindow = 0;
    ft_rai_ina228.worstWindow = 0;
    ft_rai_ads_state.worstWindow = 0;
    ft_rai_bmp_state.worstWindow = 0;
    ft_rai_imu.worstWindow = 0;
    ft_ReadVEData.worstWindow = 0;
    ft_FlushFileWriteQueue.worstWindow = 0;



    prev_millis7888 = millis();
  }
  endtime = esp_timer_get_time();  //Record end of Loop
  LoopTime = (endtime - starttime);
  if (LoopTime > 5000000) {  // 5 seconds in microseconds
    Serial.printf("WARNING: Loop took %lld - potential watchdog risk\n", LoopTime / 1000);
    if (WiFi.getMode() != WIFI_OFF) {  //  GATE
      char warnMsg[96];
      snprintf(warnMsg, sizeof(warnMsg), "WARNING: Loop took %llums - potential watchdog risk", LoopTime / 1000);
      events.send(warnMsg, "console", millis());
    }
  }
  //This one resets every 5 seconds (AinputTrackerTime) — rolling short-window worst for UI sparkle
  if (LoopTime > loopTime5sWindow) {
    loopTime5sWindow = LoopTime;
  }
  //This one is the true session worst — resets only at boot, never mid-session
  if (LoopTime > MaximumLoopTime) {
    MaximumLoopTime = LoopTime;
  }
  //This one persists to NVS — becomes prevSessionMaxLoopTime on next boot
  if (LoopTime > MaxLoopTime) {
    MaxLoopTime = LoopTime;
  }

  //TempTask Debug
  // Temp debug status - print every 30 seconds
  static unsigned long lastTempDebugPrint = 0;
  if (millis() - lastTempDebugPrint >= 300000) {  //5 minutes
    lastTempDebugPrint = millis();
    printTempDebugStatus();
  }
  // === FUNCTION TIMING DIGEST — Serial only, key spike candidates ===
  static unsigned long lastTimingPrint = 0;
  if (millis() - lastTimingPrint >= 300000) {  //5 minutes
    lastTimingPrint = millis();
    Serial.println("--- Function Timing Worst-Case (ms) | 5s-win / session ---");
    Serial.printf("  Loop overall:           %4lu / %4lu\n",
                  (uint32_t)(loopTime5sWindow / 1000), (uint32_t)(MaximumLoopTime / 1000));
    Serial.printf("  ReadAnalogInputs:       %4lu / %4lu\n",
                  ft_ReadAnalogInputs.worstWindow / 1000, ft_ReadAnalogInputs.worstSession / 1000);
    Serial.printf("  RAI total:              %4lu / %4lu\n",
                  ft_rai_total.worstWindow / 1000, ft_rai_total.worstSession / 1000);
    Serial.printf("  RAI INA228:             %4lu / %4lu\n",
                  ft_rai_ina228.worstWindow / 1000, ft_rai_ina228.worstSession / 1000);
    Serial.printf("  RAI ADS state:          %4lu / %4lu\n",
                  ft_rai_ads_state.worstWindow / 1000, ft_rai_ads_state.worstSession / 1000);
    Serial.printf("  RAI BMP state:          %4lu / %4lu\n",
                  ft_rai_bmp_state.worstWindow / 1000, ft_rai_bmp_state.worstSession / 1000);
    Serial.printf("  RAI IMU:                %4lu / %4lu\n",
                  ft_rai_imu.worstWindow / 1000, ft_rai_imu.worstSession / 1000);
    Serial.printf("  saveNVSData:            %4lu / %4lu\n",
                  ft_saveNVSData.worstWindow / 1000, ft_saveNVSData.worstSession / 1000);
    Serial.printf("  Alternator Control:     %4lu / %4lu\n",
                  ft_AdjustFieldLearnMode.worstWindow / 1000, ft_AdjustFieldLearnMode.worstSession / 1000);
    Serial.printf("  CheckAlarms:            %4lu / %4lu\n",
                  ft_CheckAlarms.worstWindow / 1000, ft_CheckAlarms.worstSession / 1000);
    Serial.printf("  calcDerivedMetrics:     %4lu / %4lu\n",
                  ft_calculateDerivedMetrics.worstWindow / 1000, ft_calculateDerivedMetrics.worstSession / 1000);
    Serial.printf("  logDashboardValues:     %4lu / %4lu\n",
                  ft_logDashboardValues.worstWindow / 1000, ft_logDashboardValues.worstSession / 1000);
    Serial.printf("  printSystemHealth:      %4lu / %4lu\n",
                  ft_printSystemHealth.worstWindow / 1000, ft_printSystemHealth.worstSession / 1000);
    Serial.printf("  checkWiFiConnection:    %4lu / %4lu\n",
                  ft_checkWiFiConnection.worstWindow / 1000, ft_checkWiFiConnection.worstSession / 1000);
    Serial.printf("  SendWifiData:           %4lu / %4lu\n",
                  ft_SendWifiData.worstWindow / 1000, ft_SendWifiData.worstSession / 1000);
    Serial.printf("  ch1_compute_stats:      %4lu / %4lu\n",
                  ft_ch1_compute_stats.worstWindow / 1000, ft_ch1_compute_stats.worstSession / 1000);
    Serial.printf("  uploadSensorHistory:    %4lu / %4lu\n",
                  ft_uploadSensorHistory.worstWindow / 1000, ft_uploadSensorHistory.worstSession / 1000);
    Serial.printf("  uploadBufferedRecords:  %4lu / %4lu\n",
                  ft_uploadBufferedRecords.worstWindow / 1000, ft_uploadBufferedRecords.worstSession / 1000);
    Serial.printf("  buildConfigPayload:     %4lu / %4lu\n",
                  ft_buildConfigPayload.worstWindow / 1000, ft_buildConfigPayload.worstSession / 1000);
    Serial.println("--- SOC / update block ---");
    Serial.printf("  UpdateEngineRuntime:    %4lu / %4lu\n",
                  ft_UpdateEngineRuntime.worstWindow / 1000, ft_UpdateEngineRuntime.worstSession / 1000);
    Serial.printf("  UpdateEngineFuel:       %4lu / %4lu\n",
                  ft_UpdateEngineFuel.worstWindow / 1000, ft_UpdateEngineFuel.worstSession / 1000);
    Serial.printf("  UpdateBatterySOC:       %4lu / %4lu\n",
                  ft_UpdateBatterySOC.worstWindow / 1000, ft_UpdateBatterySOC.worstSession / 1000);
    Serial.printf("  UpdateTravelStats:      %4lu / %4lu\n",
                  ft_UpdateTravelStatistics.worstWindow / 1000, ft_UpdateTravelStatistics.worstSession / 1000);
    Serial.printf("  UpdateDistanceInterval: %4lu / %4lu\n",
                  ft_UpdateDistanceThisInterval.worstWindow / 1000, ft_UpdateDistanceThisInterval.worstSession / 1000);
    Serial.printf("  UpdateBoardTempPress:   %4lu / %4lu\n",
                  ft_UpdateBoardTempPressureMaximums.worstWindow / 1000, ft_UpdateBoardTempPressureMaximums.worstSession / 1000);
    Serial.printf("  handleSocGainReset:     %4lu / %4lu\n",
                  ft_handleSocGainReset.worstWindow / 1000, ft_handleSocGainReset.worstSession / 1000);
    Serial.printf("  handleAltZeroReset:     %4lu / %4lu\n",
                  ft_handleAltZeroReset.worstWindow / 1000, ft_handleAltZeroReset.worstSession / 1000);
    Serial.printf("  calculateChargeTimes:   %4lu / %4lu\n",
                  ft_calculateChargeTimes.worstWindow / 1000, ft_calculateChargeTimes.worstSession / 1000);
    Serial.printf("  UpdateSailingMetrics:   %4lu / %4lu\n",
                  ft_UpdateSailingMetrics.worstWindow / 1000, ft_UpdateSailingMetrics.worstSession / 1000);
    Serial.printf("  updateWeatherMode:      %4lu / %4lu\n",
                  ft_updateWeatherMode.worstWindow / 1000, ft_updateWeatherMode.worstSession / 1000);
    Serial.printf("  updateSensorWindow:     %4lu / %4lu\n",
                  ft_updateSensorWindow.worstWindow / 1000, ft_updateSensorWindow.worstSession / 1000);
    Serial.printf("  checkTimeSync:          %4lu / %4lu\n",
                  ft_checkTimeSync.worstWindow / 1000, ft_checkTimeSync.worstSession / 1000);
    Serial.println("---");
    Serial.printf("  FlushFileWriteQueue:    %4lu / %4lu\n",
                  ft_FlushFileWriteQueue.worstWindow / 1000, ft_FlushFileWriteQueue.worstSession / 1000);
  }
  // === END FUNCTION TIMING DIGEST ===

  // === LED FIELD INDICATOR: 10-second cycle, ON-time proportional to duty MOVE THIS OUT OF LOOP SOMEDAY ===
  {
    static unsigned long ledCycleStart = 0;
    const unsigned long LED_CYCLE_MS = 10000;
    unsigned long elapsed = millis() - ledCycleStart;
    if (elapsed >= LED_CYCLE_MS) {
      ledCycleStart = millis();
      elapsed = 0;
    }
    unsigned long onTime = (unsigned long)(dutyCycle * (LED_CYCLE_MS / 100.0f));
    if (onTime < 150) onTime = 0;
    digitalWrite(2, (elapsed < onTime) ? HIGH : LOW);
  }
  checkAndRestart();     // scheduled maintenance restart
  esp_task_wdt_reset();  // Always reset watchdog at end of loop
                         // delay(1);              // Removed 4/18/2026
}

//This has to stay here, something about lambda functions (?)
template<typename T> void PrintLabelValWithConversionCheckUnDef(const char *label, T val, double (*ConvFunc)(double val) = 0, bool AddLf = false, int8_t Desim = -1) {
  OutputStream->print(label);
  if (!N2kIsNA(val)) {
    if (Desim < 0) {
      if (ConvFunc) {
        OutputStream->print(ConvFunc(val));
      } else {
        OutputStream->print(val);
      }
    } else {
      if (ConvFunc) {
        OutputStream->print(ConvFunc(val), Desim);
      } else {
        OutputStream->print(val, Desim);
      }
    }
  } else OutputStream->print("not available");
  if (AddLf) OutputStream->println();
}
//*****************************************************************************