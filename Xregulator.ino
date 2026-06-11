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
//#include <U8g2lib.h>            // display
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


// ============================================================
// BEST-EVER FRONT — shared engine (Phase A). Generic over an axis count NAXIS so one C++
// instance serves each system: alternator (4-D), sail (3-D), motor (3-D). Lives here in the
// main sketch (this project keeps no .h files) so the templates are defined before any use in
// the _functions.ino files. Design contract: BEST_EVER_FRONT_SPEC.md §2/§5 + IMPLEMENTATION_PLAN.md §2.
//   1. Episode<NAXIS>    — backward look-back / reseed steady-run detector (replaces fixed windows).
//   2. FrontStore<NAXIS> — sparse best-ever support points (never a grid) + IDW eval + device keep-gate.
// ============================================================

// Per-axis steadiness knobs (live-tunable): deviation bound + how long it must hold.
struct EpAxisCfg { float tol; float steadySec; };

// One raw sample fed to the detector. x[] are the steadiness axes (band-checked AND averaged);
// ex[] are extra raw "passenger" inputs that are AVERAGED over the run but NOT band-checked — kept
// so the cloud retains the spec's diagnostic/forensic inputs (alt raw duty; motor raw AWS/AWA) for
// recomputing a derived axis or diagnosing a bad point. out is the measured output, averaged.
template <int NAXIS>
struct RawSample { float x[NAXIS]; float ex[2]; float out; uint32_t tMs; };

// One emitted episode point == one front support point. x[] are the SURFACE coordinates (which may
// be DERIVED from the steadiness axes, e.g. excitation from duty); ex[] are the retained raw extras
// (run-averaged, uploaded to the cloud's raw history, not used by the front eval); y is the output.
template <int NAXIS>
struct FrontPoint { float x[NAXIS]; float ex[2]; float y; uint32_t nSamp; uint32_t tEmit; };

// A run is a contiguous tail of recent samples all mutually within band on every axis (max-min
// ≤ tol). It grows while each new sample stays in band; on a break it emits the run's average
// (if every axis held for its steady time) and reseeds the next run from the longest in-band
// tail ending at the breaking sample, so compatible points are reused, never discarded.
template <int NAXIS>
struct Episode {
  EpAxisCfg cfg[NAXIS];
  // current run (running sums → cheap average, no per-sample storage):
  double    sumX[NAXIS], sumEx[2], sumOut;
  float     runMin[NAXIS], runMax[NAXIS];
  uint32_t  count, runStartMs;
  // reseed look-back ring (PSRAM, allocated by the caller):
  RawSample<NAXIS> *ring; int ringCap, ringHead, ringCount;
  // Per-axis INDEPENDENT steadiness trackers. Each axis keeps its own in-band window + dwell so a
  // long steady-time on a slow axis (e.g. temperature, 30 s) does NOT force the fast axes
  // (RPM/duty/Vbus, ~3 s) to also hold that long — the whole point of per-axis criteria. These
  // PERSIST across reseeds (a fast-axis break must not wipe the slow axis's accumulated dwell);
  // they re-center only on a full reset (ringCount==0 → next sample is "fresh").
  float     axMin[NAXIS], axMax[NAXIS];
  uint32_t  axSinceMs[NAXIS];   // when each axis last (re)entered its band
  bool      runQualified;       // current run has met EVERY axis's own steady time
  // OPTIONAL output-steadiness band (outCfg.tol <= 0 → disabled, the default): the emitted
  // quantity itself must also hold steady. Directly guards what gets recorded, which is what
  // allows the input bands to be sized purely for transient rejection. Same independent-dwell
  // pattern as the input axes.
  EpAxisCfg outCfg;
  float     outAxMin, outAxMax;     // independent dwell tracker (persists across reseeds, like axMin/axMax)
  uint32_t  outAxSinceMs;
  float     runOutMin, runOutMax;   // current run's output band
  uint32_t  minRunMs;               // minimum run duration to emit (0 = no floor); closes the
                                    // short-tail-after-reseed emit edge case
  float     lastRunSpan[NAXIS], lastRunOutSpan;   // per-axis spread of the last EMITTED run (soak diagnostics)

  void init(RawSample<NAXIS> *ringBuf, int cap) {
    ring = ringBuf; ringCap = cap; ringHead = 0; ringCount = 0;
    outCfg = { 0, 0 }; minRunMs = 0;
    outAxMin = outAxMax = 0; outAxSinceMs = 0; lastRunOutSpan = 0;
    for (int a = 0; a < NAXIS; a++) lastRunSpan[a] = 0;
    clearRun();
  }
  void clearRun() {
    sumOut = 0; count = 0; runStartMs = 0; sumEx[0] = sumEx[1] = 0;
    runQualified = false;   // NB: does NOT reset the per-axis trackers (reseed() calls clearRun);
    for (int a = 0; a < NAXIS; a++) { sumX[a] = 0; runMin[a] = 0; runMax[a] = 0; }   // they re-center on a "fresh" sample
    runOutMin = 0; runOutMax = 0;
  }
  // Per-axis independent steadiness. `fresh` (first sample after a full reset) re-centers every
  // axis on the sample; otherwise each axis extends its own band, and restarts ONLY its own clock
  // when IT leaves its band — independent of the other axes.
  void axisTrack(const RawSample<NAXIS> &s, bool fresh) {
    for (int a = 0; a < NAXIS; a++) {
      if (fresh) { axMin[a] = axMax[a] = s.x[a]; axSinceMs[a] = s.tMs; continue; }
      float lo = axMin[a] < s.x[a] ? axMin[a] : s.x[a];
      float hi = axMax[a] > s.x[a] ? axMax[a] : s.x[a];
      if (hi - lo > cfg[a].tol) { axMin[a] = axMax[a] = s.x[a]; axSinceMs[a] = s.tMs; }   // this axis left its band → restart its own clock
      else { axMin[a] = lo; axMax[a] = hi; }
    }
    if (outCfg.tol > 0) {   // output band tracks the same way, with its own independent clock
      if (fresh) { outAxMin = outAxMax = s.out; outAxSinceMs = s.tMs; }
      else {
        float lo = outAxMin < s.out ? outAxMin : s.out;
        float hi = outAxMax > s.out ? outAxMax : s.out;
        if (hi - lo > outCfg.tol) { outAxMin = outAxMax = s.out; outAxSinceMs = s.tMs; }
        else { outAxMin = lo; outAxMax = hi; }
      }
    }
  }
  // True once EVERY axis (and the output band, if enabled) has independently held within its
  // band for at least its OWN steady time.
  bool axesQualified(uint32_t nowMs) const {
    for (int a = 0; a < NAXIS; a++)
      if ((uint32_t)(nowMs - axSinceMs[a]) < (uint32_t)(cfg[a].steadySec * 1000.0f)) return false;
    if (outCfg.tol > 0 && (uint32_t)(nowMs - outAxSinceMs) < (uint32_t)(outCfg.steadySec * 1000.0f)) return false;
    return true;
  }
  void ringPush(const RawSample<NAXIS> &s) {
    ring[ringHead] = s;
    ringHead = (ringHead + 1) % ringCap;
    if (ringCount < ringCap) ringCount++;
  }
  // ring index of the k-th newest sample (k=0 = newest just pushed)
  int ringIdx(int k) const { return ((ringHead - 1 - k) % ringCap + ringCap) % ringCap; }

  void startRunWith(const RawSample<NAXIS> &s) {
    sumOut = s.out; count = 1; runStartMs = s.tMs;
    sumEx[0] = s.ex[0]; sumEx[1] = s.ex[1];
    for (int a = 0; a < NAXIS; a++) { sumX[a] = s.x[a]; runMin[a] = runMax[a] = s.x[a]; }
    runOutMin = runOutMax = s.out;
  }
  bool inBandWith(const RawSample<NAXIS> &s) const {
    for (int a = 0; a < NAXIS; a++) {
      float lo = runMin[a] < s.x[a] ? runMin[a] : s.x[a];
      float hi = runMax[a] > s.x[a] ? runMax[a] : s.x[a];
      if (hi - lo > cfg[a].tol) return false;
    }
    if (outCfg.tol > 0) {
      float lo = runOutMin < s.out ? runOutMin : s.out;
      float hi = runOutMax > s.out ? runOutMax : s.out;
      if (hi - lo > outCfg.tol) return false;
    }
    return true;
  }
  void commit(const RawSample<NAXIS> &s) {
    for (int a = 0; a < NAXIS; a++) {
      sumX[a] += s.x[a];
      if (s.x[a] < runMin[a]) runMin[a] = s.x[a];
      if (s.x[a] > runMax[a]) runMax[a] = s.x[a];
    }
    if (s.out < runOutMin) runOutMin = s.out;
    if (s.out > runOutMax) runOutMax = s.out;
    sumEx[0] += s.ex[0]; sumEx[1] += s.ex[1];
    sumOut += s.out; count++;
  }
  // Complete the current run; emit its average to `out` if it qualified — i.e. every axis
  // independently held within its band for its OWN steady time (runQualified, latched during
  // feed) AND the run itself lasted at least minRunMs (a reseed can start a run with latched
  // qualification, so without the floor a short tail could emit). `nowMs` is the breaking
  // sample's time. Returns true if a point was emitted.
  bool complete(uint32_t nowMs, FrontPoint<NAXIS> *out) {
    bool emit = (count >= 1) && runQualified
                && (minRunMs == 0 || (uint32_t)(nowMs - runStartMs) >= minRunMs);
    if (emit && out) {
      for (int a = 0; a < NAXIS; a++) out->x[a] = (float)(sumX[a] / (double)count);
      out->ex[0] = (float)(sumEx[0] / (double)count);
      out->ex[1] = (float)(sumEx[1] / (double)count);
      out->y = (float)(sumOut / (double)count);
      out->nSamp = count;
      out->tEmit = nowMs;
      for (int a = 0; a < NAXIS; a++) lastRunSpan[a] = runMax[a] - runMin[a];
      lastRunOutSpan = runOutMax - runOutMin;
    }
    return emit;
  }
  // Reseed a fresh run from the longest in-band tail of the ring ending at the newest sample
  // (in-band on every axis AND the output band, when enabled).
  void reseed() {
    clearRun();
    if (ringCount <= 0) return;
    float mn[NAXIS], mx[NAXIS], mnO, mxO;
    { const RawSample<NAXIS> &s0 = ring[ringIdx(0)];
      for (int a = 0; a < NAXIS; a++) mn[a] = mx[a] = s0.x[a];
      mnO = mxO = s0.out; }
    int oldestK = 0;
    for (int k = 1; k < ringCount; k++) {
      const RawSample<NAXIS> &s = ring[ringIdx(k)];
      bool ok = true;
      for (int a = 0; a < NAXIS; a++) {
        float lo = mn[a] < s.x[a] ? mn[a] : s.x[a];
        float hi = mx[a] > s.x[a] ? mx[a] : s.x[a];
        if (hi - lo > cfg[a].tol) { ok = false; break; }
      }
      if (ok && outCfg.tol > 0) {
        float lo = mnO < s.out ? mnO : s.out;
        float hi = mxO > s.out ? mxO : s.out;
        if (hi - lo > outCfg.tol) ok = false;
      }
      if (!ok) break;
      for (int a = 0; a < NAXIS; a++) { if (s.x[a] < mn[a]) mn[a] = s.x[a]; if (s.x[a] > mx[a]) mx[a] = s.x[a]; }
      if (s.out < mnO) mnO = s.out;
      if (s.out > mxO) mxO = s.out;
      oldestK = k;
    }
    for (int a = 0; a < NAXIS; a++) { runMin[a] = mn[a]; runMax[a] = mx[a]; }
    runOutMin = mnO; runOutMax = mxO;
    for (int k = oldestK; k >= 0; k--) {           // oldest → newest
      const RawSample<NAXIS> &s = ring[ringIdx(k)];
      for (int a = 0; a < NAXIS; a++) sumX[a] += s.x[a];
      sumEx[0] += s.ex[0]; sumEx[1] += s.ex[1];
      sumOut += s.out; count++;
    }
    runStartMs = ring[ringIdx(oldestK)].tMs;        // preserves accumulated dwell
  }
  // Feed one sample. eligible=false → below an admission floor: hard break, no reseed across it
  // (the ring is dropped so a later run can't look back over the gap). Returns true + fills `out`
  // when a completed run emits a point.
  bool feed(bool eligible, const RawSample<NAXIS> &s, FrontPoint<NAXIS> *out) {
    if (!eligible) {
      bool emitted = complete(s.tMs, out);
      clearRun();
      ringHead = 0; ringCount = 0;                  // barrier: no reseed across the ineligible gap
      return emitted;
    }
    bool fresh = (ringCount == 0);                   // first eligible sample after init/barrier → re-center axis trackers
    axisTrack(s, fresh);                             // per-axis INDEPENDENT steadiness (decoupled steady times)
    if (!runQualified && axesQualified(s.tMs)) runQualified = true;   // latch once every axis has met its own time
    ringPush(s);
    if (count == 0) { startRunWith(s); return false; }   // empty run → s founds it
    if (inBandWith(s)) { commit(s); return false; }      // grow
    bool emitted = complete(s.tMs, out);                 // break: complete then reseed (includes s)
    reseed();
    return emitted;
  }
};

// Sparse support points (never a grid). Memory scales with the data, not the input volume —
// what makes the 4-D alternator affordable. axisScale[] normalizes each dimension's distance
// for IDW (≈ that axis's tol or characteristic span).
template <int NAXIS>
struct FrontStore {
  FrontPoint<NAXIS> *pts; int count, cap;
  uint8_t source;                                   // 0 = LEARNED, 1 = FIXED (loaded curve)
  float   axisScale[NAXIS];

  void init(FrontPoint<NAXIS> *buf, int c) {
    pts = buf; cap = c; count = 0; source = 0;
    for (int a = 0; a < NAXIS; a++) axisScale[a] = 1.0f;
  }
  // Max-per-cell insert: keep the store a true upper ENVELOPE, not a point cloud. If an incoming run
  // lands in the same operating cell as an existing point (within half an axisScale on EVERY axis),
  // keep only the higher-y one — so eval() interpolates between bests instead of averaging a best with
  // also-rans. Returns true ONLY on a genuine improvement (new cell, or a higher y in an existing cell);
  // false when an existing run already beat it — callers use that to gate cloud upload + console logs.
  bool add(const FrontPoint<NAXIS> &p) {
    for (int i = 0; i < count; i++) {
      bool sameCell = true;
      for (int a = 0; a < NAXIS; a++) {
        float sc = (axisScale[a] > 1e-9f) ? axisScale[a] : 1.0f;
        if (fabsf(p.x[a] - pts[i].x[a]) > 0.5f * sc) { sameCell = false; break; }
      }
      if (sameCell) {
        if (p.y > pts[i].y) { pts[i] = p; return true; }   // new best at this cell
        return false;                                       // an existing run here was already better
      }
    }
    if (count >= cap) return false;
    pts[count++] = p; return true;
  }
  // IDW surface evaluation, O(count) — the spec's front_eval(). Float + precomputed reciprocal axis
  // scales (4 mults/point, not 4 divides) keep this cheap on the 200 Hz fold even at the raised cap.
  // A convex blend of the support points → the result is ALWAYS within their y-range: never extrapolates.
  // d_i = sqrt(Σ_a ((x[a]-pts.x[a])*invSc[a])^2); exact hit → that point's y; else Σ w_i y_i / Σ w_i.
  float eval(const float x[NAXIS], float idwPower) const {
    if (count <= 0) return 0.0f;                     // bootstrap: no surface yet
    float invSc[NAXIS];
    for (int a = 0; a < NAXIS; a++) invSc[a] = (axisScale[a] > 1e-9f) ? (1.0f / axisScale[a]) : 1.0f;
    float wsum = 0, num = 0;
    for (int i = 0; i < count; i++) {
      float d2 = 0;
      for (int a = 0; a < NAXIS; a++) {
        float dx = (x[a] - pts[i].x[a]) * invSc[a];
        d2 += dx * dx;
      }
      if (d2 < 1e-12f) return pts[i].y;              // exact hit
      // dᵢ^power. Fast-path power 2 (the default) — d2 already is dᵢ²; skip sqrt+pow (200 Hz hot path).
      float dp = (idwPower == 2.0f) ? d2 : powf(sqrtf(d2), idwPower);
      float w = 1.0f / (dp + 1e-9f);
      wsum += w; num += w * pts[i].y;
    }
    return (wsum > 0) ? (num / wsum) : 0.0f;
  }
  // Device keep-gate (LEARNED) — the spec's front_pushes(). Keep only runs that beat the current
  // surface; safetyMargin now defaults 0 (was a keep-bias for cloud sampling, but the cloud only
  // prunes / gets raw episodes regardless, so a margin just polluted the local eval). count==0 bootstraps.
  // Callers should only apply this gate when hasLocalSupport() is true — far from all support the
  // IDW blend is not a fair bar (it once locked low-RPM regions out forever).
  bool pushes(const float x[NAXIS], float y, float safetyMargin, float idwPower) const {
    if (count <= 0) return true;
    return y > eval(x, idwPower) - safetyMargin;
  }
  // Any support point within the same cell (the half-axisScale box add() dedupes in)? A run landing
  // in an unvisited cell is admitted unconditionally — it opens that region at its true value, and
  // add()'s max-per-cell keeps later, better runs.
  bool hasLocalSupport(const float x[NAXIS]) const {
    for (int i = 0; i < count; i++) {
      bool sameCell = true;
      for (int a = 0; a < NAXIS; a++) {
        float sc = (axisScale[a] > 1e-9f) ? axisScale[a] : 1.0f;
        if (fabsf(x[a] - pts[i].x[a]) > 0.5f * sc) { sameCell = false; break; }
      }
      if (sameCell) return true;
    }
    return false;
  }
  // Normalized (axis-scaled) distance to the nearest support point — the "is the IDW reference
  // trustworthy here" test. Beyond the caller's radius the live % and trend report no reference
  // instead of a ratio against a blend of faraway points. Empty front → huge distance.
  float nearestNormDist(const float x[NAXIS]) const {
    if (count <= 0) return 1e9f;
    float invSc[NAXIS];
    for (int a = 0; a < NAXIS; a++) invSc[a] = (axisScale[a] > 1e-9f) ? (1.0f / axisScale[a]) : 1.0f;
    float best = 1e30f;
    for (int i = 0; i < count; i++) {
      float d2 = 0;
      for (int a = 0; a < NAXIS; a++) {
        float dx = (x[a] - pts[i].x[a]) * invSc[a];
        d2 += dx * dx;
      }
      if (d2 < best) best = d2;
    }
    return sqrtf(best);
  }
};



// Make the types visible to auto-generated prototypes, hack
struct UpdateInfo;
struct StreamingExtractor;
struct HttpsRequest;
struct SensorSnapshot;  // full definition near line 1334 (PSRAM sensor ring)
struct IgnWatermark;    // full definition further down; needed here so the
                        // auto-prototype of wmIgnUpdate(IgnWatermark&, float)
                        // doesn't precede the struct declaration
// Auto-prototype generator fails on default-argument functions defined in later .ino files.
bool fieldOffSettled(uint32_t extraMs = 0);

SET_LOOP_TASK_STACK_SIZE(20 * 1024);  // Increase stack from 8KB to 20KB, necessary for SSL/TLS operations, backtraced at 12 on 4/18/26
int hardwarePresent = 1;              // usage varies
// Parse JSON update response - this struct definition cannot move down in file, leave it here.
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
  // PSRAM only — no internal-heap fallback: ~300KB of web bundle on the internal
  // heap would destroy the contiguous block TLS handshakes need. On failure
  // serveCachedGz() returns false and the file is served from flash instead.
  result.data = (uint8_t *)ps_malloc(result.size);
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

// ============= HTTPS TASK SYSTEM =============
int lastHttpResponseCode = 0;  // Track last HTTP response for failure handling
QueueHandle_t httpsQueue;
TaskHandle_t httpsTaskHandle;

SemaphoreHandle_t fsMutex = NULL;  // mutex protection for littlefs

enum HttpsRequestType {
  HTTPS_UPLOAD_PAYLOAD,
  HTTPS_UPLOAD_CONFIG,
  HTTPS_UPLOAD_BOATPERF,
  HTTPS_UPLOAD_ALTHEALTH,
  HTTPS_FETCH_WEATHER,
  HTTPS_UPDATE_FW_VERSION,
  HTTPS_CHECK_FORCED_UPDATE,
  HTTPS_CLEAR_FORCED_UPDATE
};

struct HttpsRequest {
  HttpsRequestType type;
  char    *payload;     // PSRAM, OWNED: producer ps_malloc's, the httpsTask worker free's after use.
  uint32_t payloadCap;  // allocated capacity (0 / NULL payload for the no-body request types)
};

// OTA artifacts are served from a stable URL we control: ota.xengineering.net, a thin
// proxy on our own web host that forwards to the Supabase Storage "ota" bucket. The
// firmware only ever knows this domain — to re-home OTA hosting later, we repoint the
// proxy, no firmware/factory re-flash. URLs are built from OTA_BASE_URL in
// performOTAUpdateToVersion(). (setInsecure on the download path; integrity is the
// on-device RSA-4096 signature check, not TLS — see performOTAUpdate.)
const char *OTA_BASE_URL = "https://ota.xengineering.net";
// IMPORTANT: Firmware Version number MUST follow semantic versioning (x.y.z format)
// - Only numeric digits and dots allowed (regex: ^\d+\.\d+\.\d+$)
// - Examples: "1.0.0", "2.1.3", "10.5.22" ✅
// - Invalid: "1.1.1Retry", "v2.0.0", "2.1.0-beta" ❌
//Maximum supported version = "999.99.99" → 999*10000 + 99*100 + 99 = 9,999,999
//         Safe Version Ranges:
//major: 0-999   (4 digits max)
//minor: 0-99    (2 digits max)
//patch: 0-99    (2 digits max)
const char *FIRMWARE_VERSION = "0.0.36";

String currentUID;

#define FUNC_TIMING_WINDOW_MS 10000  // rolling window for per-function worst-case timing (ms)


// ============================================================
// ALTERNATOR HEALTH v2 — cloud-fitted curve model + best-ever record book.
//   A_pred = base(RPM, excitation) × tempCorr(T) × busCorr(Vbus).
//   "excitation proxy" = temp-normalized field drive, NOT measured field current.
//   The CLOUD fits these surfaces from uploaded best-ever record points (field-off +
//   interval); the device only EVALUATES them to rate live output, BUFFERS new
//   envelope-pushing records for the next upload, and tracks a performance%-vs-engine-
//   hours TREND (the headline display). Best-ever works here because an alternator's
//   output is near-deterministic given its inputs, so best-ever ≈ expected and the
//   reference auto-ratchets to the healthy peak (no freeze trigger needed).
//   Mirrors boat-performance v2. Full design: ALT_HEALTH_V2_REBUILD_PLAN.md.
// ============================================================

// Excitation-proxy temp normalization (copper seed; cloud refits α later).
#define ALT_ALPHA_PER_C 0.00393f
#define ALT_TREF_C      25.0f
#define ALT_MIN_BATT_V  8.0f
#define ALT_RPM_MAX     6000.0f   // axis full-scale (RPM)
#define ALT_FI_MAX      15.0f     // axis full-scale (excitation proxy)

// Curve grid — anchors/samples. MUST match the cloud edge fn (update-alt-health) exactly.
// ============================================================
// ALTERNATOR HEALTH — Best-Ever Front. Reference = a sparse best-ever output-amps surface over
//   {RPM, excitation, Vbus, temp}. The device learns + evaluates it (Episode + FrontStore engine
//   at the top of this file); the cloud prunes + retains raw history. Replaces the old cloud-fitted
//   anchor-grid curve + record book + sliding window. Engine instance + functions: 7_functions.ino.
// ============================================================

// Performance-vs-engine-hours TREND ring (the headline). One point per engine-hour bucket:
// average + worst output-% vs the best-ever front. Survives reboot; uploads for cloud history.
#define ALT_TREND_CAP 20000    // engine-hour buckets retained (~20k hrs; 120 KB PSRAM)
struct AltTrendPt {
  uint16_t engHour;            // engine-hours-since-baseline bucket index
  int16_t  worstPct;          // worst (min) output-% in this bucket (×10)
  int16_t  overallPct;        // average output-% in this bucket (×10)
};
static AltTrendPt *altTrend = nullptr;
static int         altTrendCount = 0;
static uint32_t    altTrendFlushed = 0;     // trend records already in the /alttrend.bin append log
static bool        altTrendRewrite = true;  // force a full log rewrite (load-miss or ring eviction)
// Current-bucket accumulators (committed when the engine-hour bucket advances).
static double altBucket_sum = 0, altBucket_n = 0;   // running average over the bucket
static float  altBucket_worst = 0;                  // min output-% seen this bucket
static int    altCurEngHour = -1;
double altTrendBaselineSec = 0.0;   // EngineRunTime_AllTime at last "Start Over" (trend X-axis origin)

// Live point for the dashboard gauge / trend dot, via AltLive SSE.
static float   altLive_rpm = 0, altLive_exc = 0, altLive_amps = 0, altLive_pred = 0;
static float   altLive_pct = 0;            // live output-% = amps ÷ front_eval (NO clamp; may exceed 100)
static bool    altRefOk = false;           // nearest support point within altRefRadius → the % is trustworthy
static float   altRefDist = 999.0f;        // normalized distance to nearest support point (diagnostics)
static bool    altLiveValid = false;
static bool    altSteady = false;          // currently inside a steady episode run (live indicator)
static float   altWorstPctLive = 0;        // worst-bucket output-% (headline)
static float   altOverallPctLive = 0;      // current-bucket average output-%
static uint8_t altStatusCode = 0;          // 0 learning/insufficient, 1 healthy, 2 drifting, 3 disabled

// GUI-adjustable settings (Pattern B; build-then-tune). Per-axis deviation bounds (steady times +
// front/eval knobs altRpmSec/altDutySec/altVbusSec/altThermDegF/altThermSec/altSafetyMargin/
// altIdwPower/altPruneK live with the engine instance in 7_functions.ino):
// Bands apply to the EMA-FILTERED detector inputs (altEmaSec), so they're sized for real
// operating-point drift, not raw loop dither / sensor jitter (which the filter strips):
float altDutyTolPct    = 1.5f;   // field-duty stability band (% points, filtered; raw CV dither is ~3 p-p)
float altRpmTol        = 60.0f;  // RPM deviation band (filtered; raw idle jitter is ~50 p-p)
float altVbusTol       = 0.05f;  // bus-voltage deviation band (V, filtered)
// Admission floors:
float altMinAmps = 2.0f;
float altMinDuty = 5.0f;
// Mode:
float altPaused       = 0.0f;    // 1 = halt learning/persist (sticky); live display still updates
float altSimMode      = 0.0f;    // 1 = inject synthetic operating points for bench testing (not persisted)

// ============================================================
// BOAT PERFORMANCE — sailing polar (Phase 3). Pitch-corrected best-ever speed map.
//   V_best(windspeed, windangle) × pitchDerate(pitch_std).  Predict STW (fallback SOG).
//   Learning unit = sliding ~60 s windows; per-cell best-ever = robust top-K of window means.
//   Fouling/current = one-sided CUSUM on recent ÷ best-ever.  Manual Pause (sticky) + Reset.
//   Motoring map + web dashboard = follow-up slices. Full design: BOAT_PERFORMANCE_MATRIX_PLAN.md.
// ============================================================
#define PERF_WS_BINS    16         // wind-speed bands 0..40 kt
#define PERF_WS_MAX     40.0f
#define PERF_WA_BINS    36         // wind-angle bins 0..360° (symmetric fold uses 0..180 → first 18)
#define PERF_WA_MAX     360.0f
#define PERF_PITCH_BINS 6          // sea-state (pitch-std) bands for the 1-D derate
#define PERF_PITCH_MAX  12.0f      // pitch std (deg) full scale
#define PERF_SAIL_CELLS (PERF_WS_BINS * PERF_WA_BINS)   // 576
#define PERF_TOPK       4          // best-ever = K best window-means; reference = min(topK) (robust high pct)

// ============================================================
// BOAT PERFORMANCE — Best-Ever Front (sailing polar + motoring curve). Two sparse best-ever
//   boat-speed surfaces: SAIL over {AWS, AWA, sea-state(pitch-std)}, MOTOR over {RPM, headwind,
//   sea-state}. Wind axes are APPARENT (raw both-sided stored); perfFoldSymmetric folds |AWA| at
//   eval+display only. Speed = user-selected STW or SOG. Device learns + evaluates; cloud prunes +
//   retains raw history. Engine instances + functions: 7_functions.ino. Generic engine: top of file.
// ============================================================

// Live points for the dashboard (red dot + % gauge), via PerfLive / MotorLive SSE.
static float perfLive_ws = 0, perfLive_wa = 0, perfLive_spd = 0, perfLive_best = 0;
static float perfLive_pitch = 0, perfLive_pct = 0;
static bool  perfLiveValid = false;
static bool  perfSteady = false;    // currently inside a steady-run (sail OR motor episode accumulating) — live indicator
static uint8_t perfLiveSrc = 0;     // 1 STW, 2 SOG
static float motorLive_rpm = 0, motorLive_hw = 0, motorLive_spd = 0, motorLive_best = 0, motorLive_pct = 0, motorLive_pitch = 0;
static bool  motorLiveValid = false;
static uint8_t motorLiveSrc = 0;

// GUI-adjustable settings (registry-driven; build-then-tune). Per-axis deviation bounds here;
// steady times + sea-state window + headwind/front/eval/prune knobs live with the engine instance
// in 7_functions.ino.
float perfWsTol        = 2.0f;      // AWS deviation band (kt)
float perfWaTol        = 12.0f;     // AWA deviation band (deg)
float perfRpmTol       = 100.0f;    // motoring RPM deviation band
float perfMinBoatSpeed = 0.5f;      // admission floor (kt)
float perfMinWindSpeed = 2.0f;      // admission floor (kt, sailing)
float perfRpmFloor     = 50.0f;     // RPM ≤ this = engine off = sailing; above = motoring (motorsailing counts as motoring)
float perfSpeedSrc     = 1.0f;      // 1 = STW, 2 = SOG ("both" removed; switching source = Clear-All)
float perfFoldSymmetric = 1.0f;     // 1 = fold |AWA| at eval/display (default); 0 = keep both tacks
float perfPaused       = 0.0f;      // 1 = halt learning/persist (sticky across reboot); live display still updates
float perfSimMode      = 0.0f;      // 1 = inject synthetic wind/speed/RPM/pitch for bench testing; NOT persisted

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
volatile bool settingsDirty = false;  // set by /get handler; triggers immediate CSV3 send
unsigned long lastEventSourceSend = 0;


// Ring buffers 
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
uint64_t imu_total_samples_accel = 0;
uint64_t imu_total_samples_gyro = 0;
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
float imu_heel_deg = 0;              // Current heel angle
float imu_pitch_deg = 0;             // Current pitch angle
float imu_yaw_rate_dps = 0;          // Current yaw rate (direct from gyro_z)
float imu_vertical_accel_g = 0;      // Vertical acceleration
float imu_total_accel_g = 0;         // Total acceleration magnitude
float imu_msi_score = 0;             // Motion Sickness Index (L&G 1987, freq-weighted vertical accel RMS; 100 = severe)
float imu_vomit_pct = 0;             // Estimated % of population vomiting after 2hrs (L&G 1987, power-law approx)
float imu_anchorage_comfort = 0;     // Heuristic comfort score 0-100 (100=calm); roll+MSI+slam weighted
float imu_heel_deviation_120s = 0;   // Peak roll deviation from 2-min mean (°)
float imu_pitch_deviation_120s = 0;  // Peak pitch deviation from 2-min mean (°)
float imu_heading_swing_120s = -1;   // Peak-to-peak heading swing over 2 min (°); -1 = no compass data

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

// --- SEA STATE HOURS (stored as minutes, NVS-persisted) ---
// Moving = SOGNMEA >= 1.5 kts; buckets by MSI: gentle<10, moderate<30, rough<70, extreme>=70
uint32_t imu_min_moving_gentle = 0;
uint32_t imu_min_moving_moderate = 0;
uint32_t imu_min_moving_rough = 0;
uint32_t imu_min_moving_extreme = 0;
uint32_t imu_min_stat_gentle = 0;
uint32_t imu_min_stat_moderate = 0;
uint32_t imu_min_stat_rough = 0;
uint32_t imu_min_stat_extreme = 0;

// --- WAVE PERIOD ---
float imu_wave_period_sec = -1.0;  // -1 = no valid detection

// --- LIFETIME MAXIMUMS (Persistent to NVS) ---
float imu_heel_max_lifetime = 0;
float imu_pitch_max_lifetime = 0;
float imu_slam_peak_lifetime = 0;

// --- MOUNTING ORIENTATION ---
uint8_t imuMountOrientation = 0;  // 0-3, persisted via /vessel_info.json on LittleFS

// --- COMPLEMENTARY FILTER STATE ---
float cf_heel = 0;   // Filtered heel angle
float cf_pitch = 0;  // Filtered pitch angle
unsigned long cf_lastUpdate = 0;

// --- IMU ZERO / LEVEL CALIBRATION (captured at rest, persisted as JSON string in NVS key NK_imu_zero) ---
float imuHeelOffsetDeg = 0;   // accel-derived heel at rest; subtracted from filter
float imuPitchOffsetDeg = 0;  // accel-derived pitch at rest; subtracted from filter
float imuGxBias = 0;          // gyro pitch-rate bias at rest (dps, vessel frame)
float imuGyBias = 0;          // gyro heel-rate bias at rest (dps)
float imuGzBias = 0;          // gyro yaw-rate bias at rest (dps)

// Zero-capture state machine (averages ~2s of samples before storing)
volatile bool imuZeroInProgress = false;
double imuZeroAxSum = 0, imuZeroAySum = 0, imuZeroAzSum = 0;  // accel sums
double imuZeroGxSum = 0, imuZeroGySum = 0, imuZeroGzSum = 0;  // gyro sums
uint16_t imuZeroAccelN = 0, imuZeroGyroN = 0;
const uint16_t IMU_ZERO_TARGET = 200;  // ~2s of accel samples at 104 Hz

// --- WAVE PERIOD DETECTION STATE ---
float wave_decimated_buffer[100];  // Circular buffer for decimated samples
uint16_t wave_decim_head = 0;
float wave_last_dc_mean = 0;  // For DC removal
bool wave_last_crossing_positive = false;
unsigned long wave_last_crossing_time = 0;
float wave_period_ewma = -1.0;

// IMU NVS cache variables (may belong with other prev_* variables, but...)
uint32_t prev_imu_capsize_count = 0;
uint32_t prev_imu_pitchpole_count = 0;
uint32_t prev_imu_slam_count_lifetime = 0;
float prev_imu_heel_max_lifetime = 0;
float prev_imu_pitch_max_lifetime = 0;
float prev_imu_slam_peak_lifetime = 0;
uint32_t prev_imu_min_moving_gentle = 0;
uint32_t prev_imu_min_moving_moderate = 0;
uint32_t prev_imu_min_moving_rough = 0;
uint32_t prev_imu_min_moving_extreme = 0;
uint32_t prev_imu_min_stat_gentle = 0;
uint32_t prev_imu_min_stat_moderate = 0;
uint32_t prev_imu_min_stat_rough = 0;
uint32_t prev_imu_min_stat_extreme = 0;



// Forced update tracking
unsigned long forcedUpdateDeadline = 0;
bool hasForcedUpdate = false;
int forcedFwVersionInt = 0;

// WiFiClientSecure secureClient;  // Reusable SSL client to prevent stack overflow when we had individual ones for each upload   ABANDONED, THIS WAS NOT GOOD IN FLAKY WIFI
unsigned long lastHttpsOperationTime = 0;
const unsigned long HTTPS_MIN_INTERVAL = 500;            // was 2 seconds between any HTTPS calls becasue core0 tiny system stacks (ipc0 = 1024 bytes) , not sure it was necessary
const unsigned long CONFIG_SNAPSHOT_INTERVAL = 300000;    // 5 min — TEST (production: 86400000 = 24 hr)
// SENSOR_UPLOAD_INTERVAL is firmware-only (no UI, no LittleFS) — edit + reflash to change.
const unsigned long SENSOR_UPLOAD_INTERVAL = 120000;          // 2 min — TEST (production: 600000 = 10 min)
const unsigned long BUFFER_UPLOAD_INTERVAL = 13000;  // 13 seconds
const unsigned long BOATPERF_UPLOAD_INTERVAL = 900000;  // 15 min — field-off-gated upload + cloud re-fit cadence
unsigned long lastBoatPerfUploadTime = 0;
const unsigned long ALTHEALTH_UPLOAD_INTERVAL = 900000;  // 15 min — field-off-gated alt-health upload + cloud re-fit cadence
unsigned long lastAltHealthUploadTime = 0;
int64_t lastAltHealthSyncEpoch = 0;   // unix sec of last SUCCESSFUL alt-health cloud sync this boot (0 = none) — for "synced N ago"
int64_t lastBoatPerfSyncEpoch  = 0;   // unix sec of last SUCCESSFUL boat-perf cloud sync this boot
// Set to PRODUCTION 2026-06-02. (Other commented options: 14400000 = 4 hr, 1800000 = 30 min TEST.)
const unsigned long RESTART_INTERVAL = 43200000;   // 12 hours (PRODUCTION)
//const unsigned long RESTART_INTERVAL= 1800000;     // 30 mins(TEST)

//Configuration Snapshot Stuff
char *configPayloadBuffer;

unsigned long lastConfigSnapshotTime = 0;
unsigned long lastConfigSnapshotAttempt = 0;
int configSnapshotRetryCount = 0;
unsigned long queueDrainHoldStart = 0;  // millis() when we started waiting for queue to drain before low power; 0 = not waiting
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
//these will be the custom network created by the user in AP mode (custom values persist in NVS as NK_apssid / NK_appass)
String esp32_ap_ssid = "ALTERNATOR_WIFI";  // Default SSID
// WiFi connection timeout when trying to avoid Access Point Mode (and connect to ship's wifi on reboot)
const unsigned long WIFI_TIMEOUT = 20000;  // 20 seconds
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
// Duration of last WiFi session (seconds, persistent)
unsigned long LastSessionDuration = 0;     // seconds, persistent
unsigned long CurrentSessionDuration = 0;  // seconds
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
bool tempTaskSuspended = false;                 // True while temp task is intentionally suspended (80MHz engine-off idle); gates checkTempTaskHealth()
bool tempTaskAlarm = false;                     // Set by checkTempTaskHealth() — raises alarm condition; physical buzzer (GPIO21) is still gated by AlarmActivate
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
// Protections continue to read originals. Control loops will migrate to
// _filtered in a subsequent pass via getBatteryVoltage() / getTargetAmps().
// Thermistor (CH3) is left on its own filter inside tempPID_tick().
float InputFilterTC = 12.0f;       // ms — iExcess EMA TC, NVS-backed (default 100→12, 2026-06-10: plant/3 at measured 35ms plant delay)
float OutputPIDFilterTC = 12.0f;   // ms — Output Current PID EMA TC, NVS-backed (default 100→12, 2026-06-10: plant/3 at measured 35ms plant delay)
float VoltageFilterTC = 35.0f;     // ms — IBV EMA TC for CV voltage loop, NVS-backed (default 100→35, 2026-06-10: full plant delay at measured 35ms)
float MeasuredAmps_filtered = 0.0f;  // iExcess EMA signal
float g_pidI_filtered = 0.0f;        // Output Current PID EMA signal
float IBV_filtered = 0.0f;           // EMA of INA228 bus voltage — used by getFiltV()

// ── SystemID — plant delay measurement ───────────────────────────────────
// Step test: baseline → 3× (duty up / duty down) → post-process.
// Samples stored in PSRAM. Buffer allocated on first test run, never freed.
// Results populate the JS popup and optionally update InputFilterTC in flash.
// Legal in any SYS_MODE_AUTO state (bulk, absorption, float, target voltage).
// Manual mode is banned: duty is locked to ManualDutyTarget so the test's duty commands are silently ignored.
// Note: voltageControlActive = !inIdleStage, so it is true even in bulk — do NOT gate on !voltageControlActive.
// Enforced in the /get startSystemID handler and in the JS preflight check.
float SystemIDStepAmplitude = 6.0f;  // % duty step — web-configurable; 6% is a good default

volatile bool systemIDRequested = false;       // set true by UI handler to trigger a test run
volatile bool systemIDAbortRequested = false;  // set true by UI handler to abort in-progress test
uint8_t systemIDActive = 0;                    // 0=idle, 1-9=current phase (sent to UI for progress)
bool systemIDResultsReady = false;             // set true when post-processing is complete
uint32_t systemIDLastEndMs = 0;                // millis() when last test ended (cooldown guard)
uint8_t systemIDAbortReason = 0;               // FieldEventReason code if protection aborted the test; 0 = no abort / clean exit
uint8_t systemIDAbortPhase = 0;                // phase (1-9) at moment of protection abort; 0 = no abort

// ── Stabilize-phase constants ────────────────────────────────────────────────
#define SYSID_STABILIZE_AMPS 10.0f        // target alternator output before baseline begins
#define SYSID_STABILIZE_SAMPLES 5         // ring buffer size: 5 samples × 1Hz = 5-second window
#define SYSID_STABILIZE_BAND_A 3.0f       // 5-s rolling average must be within ±3A of target
#define SYSID_STABILIZE_TIMEOUT_MS 30000  // abort if can't stabilize within this window

float systemIDRiseDelay_ms[3] = { 0.0f, 0.0f, 0.0f };  // rising-step delays, ms
float systemIDFallDelay_ms[3] = { 0.0f, 0.0f, 0.0f };  // falling-step delays, ms
float systemIDRiseAvg_ms = 0.0f;
float systemIDFallAvg_ms = 0.0f;
float systemIDStepAmp_A[3] = { 0.0f, 0.0f, 0.0f };    // rise step amplitude per trial (upMean - quietMean), A
float systemIDQuietPP_A[3] = { 0.0f, 0.0f, 0.0f };    // quiet-phase peak-to-peak noise per trial (quietMax - quietMin), A

// ── SystemID ring buffer log (50 records, persisted to /systemidlog.bin) ──
// Mirrors tuningLog / cvTuningLog / thermalTuningLog pattern. Captured on
// every test completion (success or abort) so fleet snapshots can ship a
// short history of plant-delay results.
struct SystemIDRecord {
  uint16_t runNumber;
  float    score;              // = riseAvg_ms (lower = faster plant). -1 if aborted / no rises.
  float    riseDelays[3];      // ms per trial; -1 if not measured
  float    fallDelays[3];      // ms per trial; -1 if not measured
  float    riseAvg_ms;
  float    fallAvg_ms;
  float    stepAmps[3];        // A per trial (upMean - quietMean)
  float    quietPP[3];         // A peak-to-peak quiet-phase noise per trial
  uint8_t  abortReason;        // 0 = clean exit; non-zero = FieldEventReason
  uint8_t  abortPhase;         // phase number at abort; 0 = clean exit
  float    setupStepAmplitude; // SystemIDStepAmplitude at test time (% duty)
  float    avgRPM;             // RPM snapshot at commit
  float    avgAltTempF;        // AlternatorTemperatureF snapshot at commit
};

SystemIDRecord *systemIDLog = nullptr;  // ps_malloc(50 × sizeof(SystemIDRecord))
uint8_t  systemIDLogCount = 0;          // records currently in ring buffer (0–50)
uint8_t  systemIDLogHead  = 0;          // next write index
uint16_t systemIDRunCounter = 0;        // increments each commit; persists via loadSystemIDLog

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
uint32_t rebulkDebounceTime = 10UL * 1000UL;  // ms
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
int UseFloat = 0;              // 1 = enter float after absorption, 0 = idle until rebulk criteria met


// Absorption stage
volatile bool inAbsorptionStage = false;
uint32_t absorptionStartTime = 0;
float AbsorptionVoltage = 14.0f;
uint32_t AbsorptionTimeoutMs = 1200000UL;   //
uint32_t absorptionCompleteTime = 30000UL;  // tail current hold before float
uint32_t absorptionTailTimer = 0;
uint32_t bulkVoltageHoldMs = 250;  // time at bulk voltage before entering absorption

float FieldAdjustmentInterval = 50;  // The regulator field output is updated once every this many milliseconds
float TemperatureLimitF = 212;       // measured at the case probe; internal/metal temps run roughly +40 to +50 °F above this depending on sensor installation. Strategy rationale: docs Charging Strategy page
int ManualFieldToggle = 0;           // 0 = Auto (PID) — fresh-flash default. Set to 1 for manual field control (debugging).
int SwitchControlOverride = 1;       // set to 1 for web interface switches to override physical switch panel
int MaintainMode = 0;                // Set to 1 to target 0 amps at battery
int TargetVoltageMode = 0;
float TargetVoltageSetpoint = 12.6f;
int OnOff = 0;             // 0 is charger off, 1 is charger On (corresponds to Alternator Enable in Basic Settings)
int Ignition = 0;          // Digital Input      NEED THIS TO HAVE WIFI ON , FOR NOW
int IgnitionOverride = 1;  // to fake the ignition signal w/ software
int HiLow = 1;             // 0 will be a low setting, 1 a high setting
int AmpSensorRange = 1;    // 0=±200A, 1=±300A (default), 2=±500A — hall effect sensor range
int LimpHome = 0;          // 1 will set to limp home mode, whatever that gets set up to be
int resolution = 12;       // for OneWire temp sensor measurement
int VeData = 0;            // Set to 1 if VE serial data exists
int NMEA0183Data = 0;      // Set to 1 if NMEA serial data exists doesn't do anything yet
// ── HARD OVER-CURRENT PROTECTION ─────────────────────────────
// "Group 0" in UI = hardware overcurrent trip (no protection-group integration yet)
float HardOCTripAmps = 160.0f;   // derived: MaxTableValue + 10A — recomputed at boot and on MaxTableValue change, not persisted
uint32_t HardOCDebounceMs = 20;  // user-adjustable, persisted in LittleFS
uint32_t hardOCStartMs = 0;
//Field PWM stuff
const int pwmPin = 14;  // field PWM pin # (was 32)
//const int pwmChannel = 0;      //0–7 available for high-speed PWM  (ESP32)
const int pwmResolution = 12;          // Error = +0.010%    PWM Resolution = ±0.024% (1/4096)
float SwitchingFrequency = 400;        // Field switching frequency (doesn't much matter, avoid human hearing range is nice depending on install location)
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
float Beta = 3380.0;                   // Thermistor Beta constant — default matches Murata NXFT15XH103FA2B050 (β25/50 = 3380K)
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
float STWNMEA = NAN;                   // Speed Through Water (SOW) from NMEA2K PGN 128259 (knots); NAN = no log
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
float VMGNMEA = NAN;            // VMG toward the manual target bearing (knots)
float VMGUpwind = NAN;          // VMG to windward = SOG·cos(TWA), knots — always computed (no mode toggle)
float VMGTargetBearing = -1;    // User-set target bearing for VMG (-1 = not set)
float sustainedTWS = NAN;       // 2-min running avg of TrueWindSpeedNMEA (knots); basis for Beaufort + gale (not instantaneous)
float currentGaleMinutes = 0;   // live: minutes continuously in a gale (sustainedTWS >= 34 kt), 0 when not
// Duty cycle tracking
//float IgnitionDutyCycle = 0;       // % of time ignition is on
//float EngineRunningDutyCycle = 0;  // % of time engine RPM > 100

// ADS1115
int16_t Raw = 0;
float Channel0V, Channel1V, Channel2V, Channel3V;
float BatteryV, MeasuredAmps, RPM;  //Readings from ADS1115
float MeasuredAmpsMax;              // used to track maximum alternator output
float RPMMax;                       // used to track maximum RPM
int ADS1115Disconnected = 0;
volatile bool battVFreshFlag = false;
bool ibvFreshFlag = false;  // set each INA228 read; cleared by fastOV dvdt block

// === Cloud Features Variables (and some others later added to LiveData) ===
// Energy (Wh) — _AllTime totals are double so they survive years of accumulation
// without losing precision (float mantissa runs out around 16M Wh)
double ChargedEnergy_AllTime = 0.0;            // Wh (lifetime total produced from all sources)
double DischargedEnergy_AllTime = 0.0;         // Wh (lifetime)
float SolarChargedEnergy = 0.0f;               // Solar Wh (session)
double SolarChargedEnergy_AllTime = 0.0;       // Solar Wh (lifetime)
double AlternatorChargedEnergy_AllTime = 0.0;  // Alternator Wh (lifetime)
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
float currentFuelGPH = 0.0f;  // live fuel flow, gallons/hr (interpolated from table by RPM; 0 = engine off)
float currentNMPG = 0.0f;     // live fuel economy, nautical miles per gallon (SOG / GPH; 0 = unavailable)
// Session fuel-economy curve: observed mpg binned by RPM, this session only (zeroed at boot + Reset).
// A reading is binned only once RPM and boat speed have BOTH held within tolerance for the hold time
// (steady-state gate, mirrors the boat-perf Episode banding) so accel/decel transients don't pollute it.
// UNIVERSAL SCALE: bins span [0, top configured fuel-table RPM], so binWidth auto-fits the engine
// (default table top 4500 -> 250-RPM bins; an 8000-RPM engine -> 444-RPM bins). No hardcoded ceiling.
#define FUELCURVE_BINS 18           // number of RPM bins (fixed); width = currentFuelTopRPM / FUELCURVE_BINS
float fuelCurveNMPG[FUELCURVE_BINS] = { 0 };  // per-bin naut mi/gal; 0 = no qualified reading yet
float currentFuelTopRPM = 0.0f;     // top configured fuel-table RPM; sent to the chart for x-axis scaling
// Settle-then-measure gate: RPM+speed must hold within band for the settle time (boat speed lags throttle
// by tens of seconds), THEN mpg is averaged over the sample window and that average freezes the bin.
float fuelCurveRpmTol = 500.0f;     // RPM spread (max-min) still counted as steady
float fuelCurveSpdTol = 2.0f;       // boat-speed spread (max-min) still counted as steady (= +/-1 kt)
float fuelCurveSettleSec = 40.0f;   // hold steady this long before sampling starts (settling, discarded)
float fuelCurveSampleSec = 20.0f;   // then average mpg over this window; the average freezes the bin
// steady-gate running state + sample accumulators (engine-off / stopped / Reset all break the run):
float fcRpmMin = 0, fcRpmMax = 0, fcSpdMin = 0, fcSpdMax = 0;
uint32_t fcRunStartMs = 0;
bool fcRun = false;
double fcSampleMpgSum = 0, fcSampleRpmSum = 0;
uint32_t fcSampleCount = 0;
// Voltage (V)
float PeakVoltage_AllTime = 0.0f;  // V (lifetime peak)
float MinVoltage = 99.0f;          // V (session min)
float MinVoltage_AllTime = 99.0f;  // V (lifetime min)
// Runtime (hours)
double EngineRunTime_AllTime = 0.0;  // seconds (lifetime) — divide by 3600 for hours; double for many-year precision
// Boat-speed data maturity — lifetime seconds spent moving in each mode (sailing = engine off, motoring = engine on).
// Shown next to "front points" in the Boat Speed plot. Reset by Clear All (resetBoatPerformance). Persisted in NVS.
double perfSailSeconds = 0.0;
double perfMotorSeconds = 0.0;
// Charge cycles (count)
float ChargeCycles = 0;          // count (session)
float ChargeCycles_AllTime = 0;  // count (lifetime)
// Travel (nm, kts)
float TotalDistance = 0.0f;                      // nm (session)
float TotalDistance_AllTime = 0.0f;              // nm (lifetime)
float MaxSpeed = 0.0f;                           // kts (session)
float MaxSpeed_AllTime = 0.0f;                   // kts (lifetime)
// Longest single trip (nm). Trip ends after 60 min continuous GPS-invalid OR continuous SOG < 1.5 kn.
float LongestSingleTrip_Nm_AllTime = 0.0f;       // nm (lifetime — winning trip)
float currentTripDistanceNm = 0.0f;              // nm (in-progress trip; NVS-persisted with epoch)
uint32_t currentTripLastUpdateEpoch = 0;         // Unix seconds of last advance (0 = unknown; only stamped when timeIsSynced)
bool tripActive = false;                         // true between motion start and 60-min stall finalize
bool tripPendingRecovery = false;                // set true on boot if NVS had a saved in-progress trip; resolved once time syncs
uint32_t timeSinceLastMotion = 0;                // ms accumulated below 1.5 kn while trip active
uint32_t timeSinceLastValidGps = 0;              // ms accumulated with invalid GPS while trip active
// Rolling 24-hour max distance (lb-max-24hr leaderboard). 24 hourly buckets; bucket[head] = in-progress hour.
// Sum of all 24 ≈ last 24 hours of motion (~1 hr undercount worst case). Ring is session-only; watermark is NVS-persisted.
float Max24hrDistance_AllTime = 0.0f;            // nm (lifetime max over any rolling 24-hr window)
float distHourBuckets[24] = {0};                 // per-hour distance accumulators (nm)
uint8_t distHourHead = 0;                        // index of current in-progress hour bucket
uint32_t distHourStartMs = 0;                    // millis() at start of current hour bucket (0 = uninitialized)
// Water depth (NMEA2k PGN 128267). Stored in meters (NMEA native); displayed in feet (×3.28084).
float WaterDepth_m = 0.0f;                       // depth below transducer in meters
// Deepest anchorage leaderboard — sliding 5-hr window. Qualifying anchorage = boat stays within 100yd
// AND depth changes by 2–100 ft. Ring is allocated in PSRAM at boot (see setup()); watermark NVS-persisted.
float DeepestAnchorage_Ft_AllTime = 0.0f;        // ft (lifetime deepest qualifying anchorage avg depth)
// Sample tuple held in PSRAM ring; ~24 B × 300 slots = ~7.2 KB.
struct AnchorageSample {
  uint32_t sampleMs;   // millis() at sample
  double lat;
  double lon;
  float depth_ft;      // converted to feet for direct comparison with thresholds
};
AnchorageSample *anchorageRing = nullptr;        // PSRAM-allocated 300-slot ring
const uint16_t ANCHORAGE_RING_SIZE = 300;        // 300 minutes × 60s = 5 hr
uint16_t anchorageRingHead = 0;                  // next write slot
uint16_t anchorageRingCount = 0;                 // valid samples (up to ANCHORAGE_RING_SIZE)
uint32_t lastAnchorageSampleMs = 0;              // last sample push time
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
double voltageAccumulator_AllTime = 0.0;  // V·s (lifetime) — double; float overflowed in ~14 days
double speedAccumulator_AllTime = 0.0;    // kt·s (lifetime) — double
float AvgVoltage_AllTime = 0.0f;


//supabase authentications stuff
String authToken = "";      // Stored auth token
bool isRegistered = false;  // Registration status


// CLOUD FEATURES - STATIC BUFFERS// For ESP32-S3 with 16GB and OPI PSRAM
const int PAYLOAD_BUFFER_SIZE = 4096;
const int CONFIG_PAYLOAD_SIZE = 32768;  // bumped 8KB → 32KB for step 4 picker spec (~190 fields)
const int ALT_UPLOAD_BUF_SIZE  = 64 * 1024;   // PSRAM upload buffer — holds a full alt front batch (≤1024 pts)
const int PERF_UPLOAD_BUF_SIZE = 128 * 1024;  // PSRAM upload buffer — holds sail+motor batches (≤2048 pts)

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
const unsigned long TIME_SYNC_INTERVAL = 43200000;  // 12 hour
enum TimeSource { TIME_NONE,
                  TIME_GPS,
                  TIME_PHONE,
                  TIME_NTP,
                  TIME_MILLIS };
TimeSource currentTimeSource = TIME_NONE;

// Freshness trackers for the priority chain (NMEA → Phone → NTP).
// Without these, once a source sets currentTimeSource, the device never
// re-considers other sources even if the chosen source goes silent (the
// "once TIME_GPS, never NTP again" bug).
unsigned long lastNmea2kSystemTimeMs = 0;  // millis() of last successful PGN 126992 SystemTime parse
unsigned long lastNmea2kGnssMs       = 0;  // millis() of last successful PGN 129029 GNSS position parse
unsigned long lastPhoneTimeMs        = 0;  // millis() of last phone-sourced time POST
unsigned long lastPhoneGpsMs         = 0;  // millis() of last phone-sourced GPS POST
const unsigned long NMEA_TIME_FRESH_MS = 300000UL;   // 5 min — fall back to other sources if no SystemTime
const unsigned long NMEA_GPS_FRESH_MS  = 60000UL;    //  1 min — GPS PGN cadence is ~1-2s, anything >60s is dead
const unsigned long PHONE_FRESH_MS     = 120000UL;   //  2 min — app posts every ~30-60s

// Phone-sourced values (held separately from NMEA so the priority chain can
// arbitrate cleanly without one source overwriting the other's globals).
double LatitudePhone  = 0.0;
double LongitudePhone = 0.0;
time_t PhoneTimeEpoch = 0;  // Unix seconds the phone reported, anchored at lastPhoneTimeMs

// Manually typed coordinates (Weather Mode "Override manually"). When
// gpsManualActive is set, these win over BOTH boat NMEA and phone GPS and stick
// until the user clears the override. Held separately so the resolver can
// reassert them every tick without the auto-sources clobbering them. Persisted.
double LatitudeManual  = 0.0;
double LongitudeManual = 0.0;
bool   gpsManualActive = false;

// Which source the *effective* GPS position currently comes from. Resolved
// every tick by resolveSources() in 4_functions.ino. Published in CSV2 slot
// `currentGpsSource` for the dashboard's live source indicator.
enum GpsSource { GPS_NONE, GPS_NMEA, GPS_PHONE, GPS_MANUAL };
GpsSource currentGpsSource = GPS_NONE;

// User override for the GPS+time priority chain. Default GTS_AUTO runs the
// freshness-arbitrated chain. The forced modes pin behaviour even if the
// chosen source is stale (resolver still shows the value but skips MARK_FRESH,
// so distance/smoothing know not to use it). Persisted in LittleFS Pattern B.
enum GpsTimeSourceMode { GTS_AUTO, GTS_NMEA, GTS_PHONE, GTS_NTP };
uint8_t gpsTimeSourceMode = GTS_AUTO;

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

// Subset of ImuWindow + the three comfort score globals, captured at
// window-roll time and frozen into the SensorSnapshot. Without this, buffered
// records would all read the live (post-reset) imuWindow at upload time,
// because uploads are field-off-gated and run minutes-to-hours after the
// snapshot was rolled. Only fields actually uploaded to the cloud are kept
// (raw per-axis accel/gyro, hf_vibe, 60s rolling metrics intentionally excluded).
struct ImuSnapshot {
  // Heel (×100)
  int32_t  heel_min;
  int32_t  heel_max;
  int64_t  heel_area_v_us;
  uint64_t heel_valid_us;
  // Pitch (×100)
  int32_t  pitch_min;
  int32_t  pitch_max;
  int64_t  pitch_area_v_us;
  uint64_t pitch_valid_us;
  // Vertical accel (×1000)
  int32_t  vertical_accel_min;
  int32_t  vertical_accel_max;
  int64_t  vertical_accel_area_v_us;
  uint64_t vertical_accel_valid_us;
  // Total accel magnitude (×1000, always positive)
  int32_t  total_accel_min;
  int32_t  total_accel_max;
  int64_t  total_accel_area_v_us;
  uint64_t total_accel_valid_us;
  // Comfort score point values at roll time (raw firmware 0-100 scales)
  float    msi_score;
  float    vomit_pct;
  float    anchorage_comfort;
  // Wave period (×1000; -1000 if invalid)
  int32_t  wave_period;
  // Events this window
  uint32_t slam_count;
  int32_t  slam_peak_max;  // ×1000 (millig's)
};

// PSRAM ring of completed sensor snapshots, waiting to upload to Supabase.
// Replaces the old per-snapshot LittleFS write that stalled Core 1 for ~400 ms
// during sector erase. Pushes are microseconds. Uploads drain to httpsQueue
// from the cloud-feature block (field-off gate) or via the "Upload Now" button.
struct SensorSnapshot {
  int64_t collectionTime;       // time_t (64-bit, Y2038-safe) — positive epoch if synced, negative-millis marker if not
  SensorWindow window;          // accumulated min/max/area for this window
  ImuSnapshot imu;              // frozen IMU subset (resists imuWindow reset between roll and upload)
  uint8_t chargeStage;          // display code (0-7) captured at window roll — uploads are deferred so this can't read live state later
};
const uint16_t SENSOR_RING_SIZE = 1000;  // 1000 × ~820 B ≈ 820 KB PSRAM; ~83 hr at 5 min/sample
SensorSnapshot *sensorRing = nullptr;    // ps_malloc'd in setup()
volatile uint16_t sensorRingHead = 0;    // next write slot
volatile uint16_t sensorRingTail = 0;    // oldest unread slot
volatile uint16_t sensorRingCount = 0;   // 0..SENSOR_RING_SIZE
volatile int32_t sensorRingInFlightIndex = -1;  // ring index currently being uploaded by Core 0 (-1 = none)
// Guards the count/tail/head read-modify-writes: Core 1 pushes, Core 0 pops,
// the async web task clears — volatile alone doesn't make uint16 RMW atomic
// on Xtensa, and a lost update near empty wraps count to 65535 (= full forever).
portMUX_TYPE sensorRingMux = portMUX_INITIALIZER_UNLOCKED;
volatile bool forceCloudFlushPending = false;   // set by /get?forceCloudFlush=1, cleared after drain attempt
volatile bool sensorRingAnnouncedEmpty = false; // throttle for "all data uploaded" console — fires once per drain-to-empty

// Barometric pressure history — 10-min cadence, 7 days. Feeds the Other-tab barometer
// (3hr tendency for Zambretti + plot of pressure trend). Beyond 1 week → cloud.
// Sized small (2 KB blob) to keep NVS wear negligible; saved only at field-off edge.
const uint16_t  BARO_HISTORY_SIZE       = 1008;                  // 7 d × 24 h × 6 samp/h
const uint32_t  BARO_SAMPLE_INTERVAL_MS = 10UL * 60UL * 1000UL;  // 10 minutes
uint16_t       *baroPressureHistory     = nullptr;               // ps_malloc'd in setup(); mbar × 10 (0 = no sample)
volatile uint16_t baroHistoryHead       = 0;                     // next write slot (circular)
uint16_t        prev_baroHistoryHead    = 0xFFFF;                // NVS shadow — write only when head moved
unsigned long   lastBaroSampleMs        = 0;                     // millis() of last sample
time_t          baroHistoryLastEpoch    = 0;                     // GPS/NTP epoch of newest sample (0 = unsynced — JS falls back to fixed sample spacing)

// Long Term Plots — local fast tier for the history tab (Phase 1). ~1-month PSRAM
// ring of min/max/avg envelopes at the SENSOR_UPLOAD_INTERVAL cadence (10 min in
// prod), derived from the closing SensorWindow in uploadSensorHistory(). Served
// from PSRAM via /longTermPlots.bin; persisted via the Phase-0 scaffold
// (writePsramBlob/readPsramBlob) at field-off edge + shutdown; restored at boot.
// No per-record timestamp: record N's time = lastEpoch − (newest − N) × interval
// (the baro scheme); cadence is served in the .bin header. Brush past ~1 month →
// cloud aggregate (1b stitch). recordSize is the scaffold layout guard — pinned below.
struct LongTermRecord {                 // naturally aligned; see static_assert for true size
  // 4-byte block
  uint32_t timestamp;                   // epoch seconds of THIS record (0 = unsynced) — legit time axis + gap detection
  int32_t  lat_avg, lon_avg;            // deg ×1e5
  uint32_t validMask;                   // 1 bit/field: had valid data this window

  // ENVELOPE (min,max,avg) — 16 fields × 3 × int16
  int16_t battVolt[3];                  // V   ×100
  int16_t battCurr[3];                  // A   ×10
  int16_t altCurr[3];                   // A   ×10
  int16_t victronCurr[3];               // A   ×10
  int16_t rpm[3];                       //     ×1   (×100 overflows int16 >327 RPM)
  int16_t duty[3];                      // %   ×100  (this IS field drive; battVolt is stored too)
  int16_t altTemp[3];                   // °F  ×10
  int16_t tempTherm[3];                 // °F  ×10
  int16_t sog[3];                       // kt  ×100
  int16_t tws[3];                       // kt  ×100
  int16_t vmg[3];                       // kt  ×100
  int16_t aws[3];                       // kt  ×100
  int16_t awa[3];                       // deg ×10 (signed)
  int16_t twa[3];                       // deg ×10 (signed)
  int16_t heel[3];                      // deg ×100 (signed, IMU)
  int16_t pitch[3];                     // deg ×100 (signed, IMU)

  // AVG-ONLY — 7 fields × int16
  int16_t soc_avg;                      // %   ×10
  int16_t baro_avg;                     // mbar×10
  int16_t ambTemp_avg;                  // °F  ×10
  int16_t cog_avg, heading_avg;         // deg ×10
  int16_t leeway_avg;                   // deg ×10 (signed)
  int16_t altZero_avg;                  //     ×100

  // byte block
  uint8_t chargeStage;                  // 0 off,1 bulk,2 absorption,3 float,4 manual,5 maintain,6 targetV,7 idle
  uint8_t _pad;
};
// 16 (4-byte block) + 96 (16 envelope) + 14 (7 avg) + 2 (byte) = 128, a multiple of
// 4 (no tail padding). Use sizeof() as recordSize everywhere; this pins it so a
// layout edit can't silently desync the scaffold guard.
static_assert(sizeof(LongTermRecord) == 128, "LongTermRecord layout changed — update the scaffold recordSize guard");

const uint16_t LONGTERM_RING_SIZE = 4320;    // 30 d × 24 h × 6/h at 10-min cadence (~501 KB @ 116 B)
LongTermRecord *longTermRing      = nullptr; // ps_malloc'd in setup()
volatile uint16_t longTermHead    = 0;       // next write slot (circular)
volatile uint16_t longTermCount   = 0;       // 0..LONGTERM_RING_SIZE
uint16_t   prev_longTermHead      = 0xFFFF;  // shadow — persist only when head moved
time_t     longTermLastEpoch      = 0;       // epoch of newest record (0 = unsynced)
#define LONGTERM_BACKUP_PATH  "/longterm_ring.bin"
#define LONGTERM_BACKUP_MAGIC 0x4C54504Cu    // 'LTPL'
#define LONGTERM_BACKUP_VER   2u    // v2: +timestamp, awa/twa → envelope (116→128 B)
// Periodic field-off dump interval. The rising-edge dump alone captures a nearly
// empty ring on a bench (field never cycles), so accumulated records were lost on
// reboot/power-loss. This re-dumps every interval WHILE field-off (only when the
// ring actually changed). writePsramBlob writes just `count` records, so it's cheap
// while the ring is small and bounded (~501 KB) when full.
const unsigned long LONGTERM_DUMP_INTERVAL_MS = 15UL * 60UL * 1000UL;  // 15 min

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

// core0Busy — "hold the field off while a long Core 0 op is in flight."
//
// What it actually does: AdjustFieldLearnMode() early-returns when this is set,
// which prevents digitalWrite(4, HIGH) from re-enabling the field AND freezes the
// voltage PI + inner current PID. The early-return is the whole control loop, not
// just the field-enable line.
//
// What should set it:
//   - Long-running Core 0 operations that are themselves only allowed to start
//     while the field has been off for ≥ 60 s (gated by fieldOffSettled()).
//     Currently: httpsTask (HTTPS uploads/fetches), syncTimeFromNTP, and the OTA
//     update path. Field is already off when they start, and the gate keeps it off
//     until they finish so the WiFi/HTTPS subsystem isn't fighting a field
//     re-enable mid-transaction.
//
// What must NOT set it:
//   - Anything that can fire during active charging. Setting this flag while RPM
//     could change blinds the protection logic and PID for the full duration of
//     the op.
//
// Readers: TempTask (defer own work), testInternetSpeed (skip), scheduled restart
// (wait up to 30 s), OTA orchestration, and the AFLM gate at 6_functions.ino:1388.
volatile bool core0Busy = false;

// Dashboard mirror of sensorRingCount. Real cap is SENSOR_RING_SIZE (1000).
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
unsigned long FullChargeTimer = 0;    // Accumulates elapsed seconds while full-charge conditions hold; compared against ChargedDetectionTime
// Timing variables
unsigned long currentTime = 0;
unsigned long elapsedMillis = 0;
unsigned long lastSOCUpdateTime = 0;      // Last time SOC was updated
unsigned long lastEngineMonitorTime = 0;  // Last time engine metrics were updated
unsigned long lastDataSaveTime = 0;       // Last time data was saved to LittleFS
int SOCUpdateInterval = 2000;             // Update SOC every 2 seconds.   Don't make this smaller than 1 without study

//NVS Stuff
unsigned long lastNVSSaveTime = 0;  // millis() at end of last successful saveNVSDataFull() — 0 = never saved
uint32_t nvsFullSaveCount = 0;      // total saveNVSDataFull() calls since boot
uint16_t nvsFullSaveLastMs = 0;     // wall-clock duration of most recent saveNVSDataFull() (ms)
uint16_t nvsFullSaveWorstMs = 0;    // worst saveNVSDataFull() duration since boot (ms)

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
  
  Periodic/threshold writes (committed by saveNVSDataFull at field-off edge):
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
int32_t prev_perfSailSeconds = 0;
int32_t prev_perfMotorSeconds = 0;
int32_t prev_EngineCycles_AllTime = 0;
int32_t prev_AltOnTime_AllTime = 0;
float prev_ChargeCycles_AllTime = 0;
int32_t prev_TotalDist_AllTime = 0;
int32_t prev_AvgSpeed_AllTime = 0;
// Longest-trip persistence shadows. Distance scaled ×100 for 0.01-nm resolution.
int32_t prev_LongestTrip_AT = 0;
int32_t prev_CurrTripDist = 0;
uint32_t prev_CurrTripEpoch = 0;
// 24-hour max distance — watermark only (ring is session-only). Scaled ×100.
int32_t prev_Max24hrDist_AT = 0;
// Deepest anchorage — watermark only (ring is session-only). Scaled ×10 for 0.1-ft resolution.
int32_t prev_DeepAnchor_AT = 0;
// Best upwind VMG (×100, 0.01 kt) + longest gale duration (×100, 0.01 hr) — leaderboard lifetime watermarks.
int32_t prev_BestUpVMG_AT = 0;
int32_t prev_GaleHrs_AT = 0;
double prev_spdAccum_AllTime = -1.0;
uint32_t prev_spdTime_AllTime = 0;
double prev_vltAccum_AllTime = -1.0;
uint32_t prev_vltTime_AllTime = 0;
int32_t prev_AvgSOC = 0;
int32_t prev_AvgSOC_AllTime = 0;
static float prev_sailing_days_alltime = -1.0f;
static float prev_sailing_dist_alltime = -1.0f;
static float prev_alt_power_max_alltime_w = -1.0f;
static float prev_solar_power_max_alltime_w = -1.0f;
uint64_t prev_socAccum_AllTime = 0;
uint32_t prev_socTime_AllTime = 0;

// NVS shadow caches — used by saveNVSDataFull() change-detection to skip unchanged keys.
float prev_MaxSpeed = 0.0f;
float prev_MaxSpeed_AllTime = 0.0f;
float prev_MeasAmpsMax = 0.0f;
float prev_MeasAmpsMax_AllTime = 0.0f;
float prev_RPMMax = 0.0f;
float prev_RPMMax_AllTime = 0.0f;
float prev_IBVMax = 0.0f;
float prev_PeakV_AllTime = 0.0f;
float prev_MinVoltage = 0.0f;
float prev_MinVoltage_AllTime = 0.0f;
float prev_board_temp_max = 0.0f;
float prev_board_temp_min = 0.0f;
float prev_baro_max = 0.0f;
float prev_baro_min = 0.0f;
float prev_MaxTempTherm = 0.0f;
float prev_MaxTempTherm_AllTime = 0.0f;
float prev_MaxAltTempF = 0.0f;
float prev_MaxAltTempF_AllTime = 0.0f;
float prev_MaxWindApp = 0.0f;
float prev_MaxWindTrue = 0.0f;
float prev_UVToday = 0.0f;
float prev_UVTomorrow = 0.0f;
float prev_UVDay2 = 0.0f;

// Accumulators for runtime tracking
unsigned long engineRunAccumulator = 0;     // Milliseconds accumulator for engine runtime
unsigned long alternatorOnAccumulator = 0;  // Milliseconds accumulator for alternator runtime

//ALternator Lifetime Prediction
// Thermal Stress Calculation - Settings
const unsigned long THERMAL_UPDATE_INTERVAL = 10000;  // 10 seconds   (this is just used for how often to update thermal stresses)

float WindingTempOffset = 50.0;  // User configurable winding temp offset (°F)
uint8_t displayTempUnit = 0;     // 0 = °F, 1 = °C — display preference, no firmware math changes
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
unsigned long lastThermalUpdateTime = 0;     // Last thermal calculation time
const uint32_t INA_SLOW_INTERVAL_MS = 1100;  // field off: AVG=128, CT=4120µs → 1054ms update
const uint32_t INA_FAST_INTERVAL_MS = 5;     // field on:  AVG=4,   CT=540µs  → 4.3ms update
uint32_t inaReadInterval = INA_SLOW_INTERVAL_MS;
bool inaFastModeActive = false;

// INA228 fast-mode interval stats — updated only when inaFastModeActive, frozen when field off
uint16_t ina_last_ms = 0;
float ina_avg_10s = 0.0f;
uint16_t ina_worst_10s = 0;
uint16_t ina_over2x_10s = 0;
float ina_avg_2m = 0.0f;
uint16_t ina_worst_2m = 0;
uint16_t ina_over2x_2m = 0;
float ina_avg_at = 0.0f;
uint16_t ina_worst_at = 0;
uint32_t ina_over2x_at = 0;

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
bool alarmLatch = false;        // Current latched alarm state
int AlarmLatchEnabled = 0;      // Whether latching is enabled (0/1 for consistency)
int AlarmTest = 0;              // Momentary alarm test (1 = test active)
bool alarmOutputState = false;  // Single source of truth for GPIO 21 state — mirrors every digitalWrite(21,...)
int ResetAlarmLatch = 0;        // Momentary reset command
unsigned long alarmTestStartTime = 0;
const unsigned long ALARM_TEST_DURATION = 2000;  // 2 seconds test duration
int Alarm_Status;                                // for alarm mirror light on Client

//More Settings
// SOC Parameters
float CurrentThreshold = 0.01f;       // Ignore currents below this (amps)
int PeukertExponent_scaled = 105;     // Peukert exponent × 100 (112 = 1.12)
int ChargeEfficiency_scaled = 990;    // Charging efficiency % × 10 (990 = 99.0%)
int ChargedVoltage_Scaled = 1400;     // Voltage threshold for "charged" (V × 100) (a Battery Monitor setup parameter, nothing to do with alternator)
float TailCurrent = 2.0f;             // % of battery Ah capacity (1 decimal place)
int ShuntResistanceMicroOhm = 100;    // Shunt resistance in microohms
int ChargedDetectionTime = 180;       // Time at charged state to consider 100% (seconds) (3 mins, industry standard for lithium)
int IgnoreTemperature = 0;            // If no temp sensor, set to 1
int IgnoreRPM = 0;                    // If RPM sensor absent or malfunctioning, set to 1 to bypass RPM gate
int MinRPMForField = 200;             // Field is cut when RPM is below this threshold (RPM)
int bmsLogic = 0;                     // if BMS is asked to turn the alternator on and off
int bmsLogicLevelOff = 0;             // set to 0 if the BMS gives a low signal (<3V?) when no charging is desired
bool chargingEnabled;                 // defined from other variables
bool bmsSignalActive;                 // Read from GPIO34
int AlarmActivate = 0;                // set to 1 to enable alarm conditions
int TempAlarm = 190;                  // above this value, sound alarm
int TempAlarmLow = 32;                // below this value, sound alarm (0 = disabled)
int VoltageAlarmHigh = 15;            // above this value, sound alarm
int VoltageAlarmLow = 11;             // below this value, sound alarm
int CurrentAlarmHigh = 100;           // above this value, sound alarm
int MaximumAllowedBatteryAmps = 150;  // safety for battery, optional
int RPMScalingFactor = 1330;          // self explanatory, adjust until it matches your trusted tachometer
float AlternatorCOffset = 0;          // tare for alt current
float BatteryCOffset = 0;             // tare or batt current
int timeToFullChargeMin = NAN;        // self explained
int timeToFullDischargeMin = NAN;     // self explained


// fields 52-57 in CSVData2 are reserved zeros (Reset* flags were always 0; buttons use hasParam, not these vars)
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

// ─────────────────────────────────────────────────────────────────────────────
// Inner Current PID Firing Interval — field-on-gated clone of the CH1 tracker.
// Records the gap between successive inner-current-PID firings, but ONLY on ticks
// where the field is actually driven. pidFire_record() is called once per normal
// control tick (past the digitalWrite(4,HIGH) gate); field-off ticks never reach
// it, and loop() clears pfHasPrev whenever the field is down so the first firing
// after a field-off stretch re-baselines instead of logging the whole off-gap.
// Reuses Ch1Bucket. No PSRAM ring — CH1's 5000-entry ring only ever yielded "last",
// which we track directly in pf_last_ms.
// ─────────────────────────────────────────────────────────────────────────────
uint16_t pf_last_ms = 0;
float    pf_avg_10s = 0;
uint16_t pf_worst_10s = 0;
uint16_t pf_over2x_10s = 0;  // mirrors CH1: not tracked at 1s granularity (always 0)
float    pf_avg_2m = 0;
uint16_t pf_worst_2m = 0;
uint32_t pf_over2x_2m = 0;
float    pf_avg_at = 0;
uint16_t pf_worst_at = 0;
uint32_t pf_over2x_at = 0;

#define PF_BUCKETS 12      // 12 closed buckets × 10 s = 2-minute window
#define PF_1S_BUCKETS 10   // 1s mini-buckets for O(1) 10s window stats
static Ch1Bucket pfBuckets[PF_BUCKETS];
static uint8_t pfBktHead = 0;
static uint8_t pfBktCount = 0;
static uint32_t pfBktStart = 0;
Ch1Bucket pfBkt1s[PF_1S_BUCKETS];
Ch1Bucket pfBkt1sCurrent = { 0, 0, 0, 0 };
uint8_t pfBkt1sHead = 0;
uint8_t pfBkt1sCount = 0;
uint32_t pfBkt1sStart = 0;
static uint64_t pfAtSum = 0;
static uint32_t pfAtCount = 0;
static uint16_t pfAtWorst = 0;
static uint32_t pfAtOver2x = 0;
static uint32_t pfPrevTs = 0;
bool pfHasPrev = false;

// ─────────────────────────────────────────────────────────────────────────────
// CV Voltage Loop Firing Interval — CV-mode-gated clone of the pf tracker above.
// Records the gap between successive CV (constant-voltage) loop firings, but ONLY
// while voltageControlActive (the regulator is actively holding a voltage target).
// voltLoop_record() is called once per CV fire; the control loop clears vlHasPrev
// whenever CV is inactive so the first firing after a CV-off stretch re-baselines
// instead of logging the whole off-gap (this is why the old single-watermark card
// warned about inflated values — the ladder fixes it). Reuses Ch1Bucket.
// ─────────────────────────────────────────────────────────────────────────────
uint16_t vl_last_ms = 0;
float    vl_avg_10s = 0;
uint16_t vl_worst_10s = 0;
uint16_t vl_over2x_10s = 0;
float    vl_avg_2m = 0;
uint16_t vl_worst_2m = 0;
uint32_t vl_over2x_2m = 0;
float    vl_avg_at = 0;
uint16_t vl_worst_at = 0;
uint32_t vl_over2x_at = 0;

#define VL_BUCKETS 12      // 12 closed buckets × 10 s = 2-minute window
#define VL_1S_BUCKETS 10   // 1s mini-buckets for O(1) 10s window stats
static Ch1Bucket vlBuckets[VL_BUCKETS];
static uint8_t vlBktHead = 0;
static uint8_t vlBktCount = 0;
static uint32_t vlBktStart = 0;
Ch1Bucket vlBkt1s[VL_1S_BUCKETS];
Ch1Bucket vlBkt1sCurrent = { 0, 0, 0, 0 };
uint8_t vlBkt1sHead = 0;
uint8_t vlBkt1sCount = 0;
uint32_t vlBkt1sStart = 0;
static uint64_t vlAtSum = 0;
static uint32_t vlAtCount = 0;
static uint16_t vlAtWorst = 0;
static uint32_t vlAtOver2x = 0;
static uint32_t vlPrevTs = 0;
bool vlHasPrev = false;

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
FuncTiming ft_AdjustFieldLearnMode;
FuncTiming ft_logDashboardValues;
FuncTiming ft_updateSystemHealthStats;
FuncTiming ft_checkWiFiConnection;
FuncTiming ft_SendWifiData;
FuncTiming ft_CheckAlarms;
FuncTiming ft_calculateDerivedMetrics;
FuncTiming ft_ch1_compute_stats;
FuncTiming ft_uploadSensorHistory;
FuncTiming ft_dumpLongTermRing;  // 15-min long-term-ring flash flush (field-off only) — was untimed, caused invisible ~250ms loop spikes
FuncTiming ft_uploadBufferedRecords;
FuncTiming ft_buildConfigPayload;
FuncTiming ft_UpdateEngineRuntime;
FuncTiming ft_UpdateEngineFuel;
FuncTiming ft_UpdateBatterySOC;
FuncTiming ft_UpdateTravelStatistics;
FuncTiming ft_UpdateBoardTempPressureMaximums;
FuncTiming ft_handleSocGainReset;
FuncTiming ft_handleAltZeroReset;
FuncTiming ft_calculateChargeTimes;
FuncTiming ft_UpdateSailingMetrics;
FuncTiming ft_updateWeatherMode;
FuncTiming ft_updateSensorWindow;
FuncTiming ft_checkTimeSync;
FuncTiming ft_rai_total;           // ReadAnalogInputs() — full function including flash writes
FuncTiming ft_rai_ina228;          // INA228 read block only
FuncTiming ft_rai_ads_state;       // ADS1115 state machine — cost per state step
FuncTiming ft_rai_bmp_state;       // BMP388 state machine — cost per state step
FuncTiming ft_rai_imu;             // IMU FIFO drain block
FuncTiming ft_updateAccelMetrics;  // accel ring-buffer processing (updateAccelMetrics)
FuncTiming ft_ReadVEData;
FuncTiming ft_altHealth;
FuncTiming ft_altFold;     // 200 Hz alt fold (IDW eval cost) — distinct from ft_altHealth (SSE wrapper)
FuncTiming ft_boatPerf;

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

// ── 80MHz low-power-mode loop instrumentation ─────────────────────────────────
// Engine-off drops the CPU to 80MHz; these track loop health in that state only
// (gated on getCpuFrequencyMhz()<81 at the read site — one cheap register read).
// The accel FIFO net-fills if a single loop pass exceeds ~38ms (6-sample drain
// budget / 156 sample-per-sec fill), so loopOver80ImuLimitCount is a "near-miss to
// IMU overrun" counter — pair it with imu_fifo_overrun_count (actual losses).
const uint32_t LOOP80_IMU_LIMIT_US = 38000;  // ~38ms: accel FIFO drain budget per poll
uint32_t loopWorst80Win = 0;           // worst 80MHz loop pass, rolling 5s window (µs)
uint32_t loopWorst80Ses = 0;           // worst 80MHz loop pass since Reset Peak Values (µs)
uint32_t loopFieldOnWin = 0;           // worst field-ON loop pass, rolling 5s window (µs)
uint32_t loopFieldOnSes = 0;           // worst field-ON loop pass since Reset Peak Values (µs)
uint32_t loopOver80ImuLimitCount = 0;  // # of 80MHz passes over LOOP80_IMU_LIMIT_US since reset
uint32_t loop80IterCount = 0;          // total 80MHz passes since reset (denominator for the above)

// ── End per-function timing ───────────────────────────────────────────────────



// Global variable to track ESP32 restart time
unsigned long lastRestartTime = 0;
bool systemShuttingDown = false;
// Deferred reboot via /get?RebootRegulator — flag set in handler, restart done in loop()
// so the HTTP response flushes first and we don't restart inside the async TCP callback
volatile bool rebootRequested = false;
unsigned long rebootRequestedAt = 0;
// Scheduled-restart user warning. 0 = outside the 10-min warning window (dashboard hides banner).
// Updated each loop tick by checkAndRestart(); published via CSV2.
uint32_t restartRemainingSec = 0;
const unsigned long RESTART_WARNING_WINDOW_MS = 600000UL;  // 10 minutes

int BatteryCurrentSource = 0;  // 0=INA228, 1=NMEA2K Batt, 2=NMEA0183 Batt, 3=Victron Batt
int timeAxisModeChanging = 0;  // toggle the time axis on and off in Plots.  Off = less janky but less info

int maxPoints;                 //number of points plotted per plot (X axis length)
int Ymin1 = -10, Ymax1 = 50;    // Current plot
float Ymin2 = 12.0, Ymax2 = 16.0;  // Voltage plot
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
// LearningPaused / LearningUpwardEnabled / LearningDownwardEnabled removed —
// were write-only (settable via /get, persisted, transmitted in CSV3 + config snapshot)
// but never consumed in any control logic. UI never built. Cleaner to delete than leave dormant.

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

// === PID Tuning Score System ===
struct TuningRecord {
  uint16_t runNumber;
  float score;
  float activeTimeSec;
  float kp, ki, kd;
  uint8_t sampleDivisor;
  float trackingGain;
  float dutyRampRate;
  int16_t waveAmplitude;
  int16_t wavePeriod;
  float avgRPM;
  float avgAltTempF;
  float worstErrorA;
};

struct ScoreBucket {
  float errorAccum;
  float activeTimeSec;
};

struct TuningScoreState {
  uint8_t toggleCount;        // half-period toggles seen since test start
  uint8_t scoredToggleCount;  // toggles that occurred in scoring phase (post ring-in)
  bool ringInDone;            // true after 4 toggles (2 full cycles discarded)
  bool inScoringWindow;       // true while scoring is active (after slew settles, times out 5s after opening)
  bool pendingWindowOpen;     // toggle seen post ring-in; waiting for slew to settle before opening window
  float errorAccum;           // ISE accumulator (e² × dt)
  float activeTimeSec;        // total time spent inside scoring windows
  uint32_t lastToggleMs;      // millis() when scoring window last opened (for 5s timeout timer)
  float rpmSum;               // for computing avg RPM over test
  float tempSum;              // for computing avg alt temp over test
  uint16_t avgSampleCount;
  float worstErrorA;  // largest single |error| seen during test
  float score;        // current normalized score = errorAccum / activeTimeSec
};

const uint32_t LIVE_BUCKET_MS[4] = { 1000UL, 10000UL, 100000UL, 1000000UL };  // bucket widths: 1s, 10s, 100s, 1000s
const uint8_t LIVE_BUCKET_N = 60;                                             // buckets per window (60 × bucket_width = window size)
const float CV_LIVE_GATE_APS = 15.0f;                                         // A/s battery current rate-of-change to open CV scoring window

TuningRecord *tuningLog = nullptr;  // ps_malloc(50 × sizeof(TuningRecord))
uint8_t tuningLogCount = 0;         // records currently in ring buffer (0–50)
uint8_t tuningLogHead = 0;          // next write index
uint16_t tuningRunCounter = 0;      // increments each commit, persists via loadTuningLog
TuningScoreState tuningScore = {};  // active test accumulator
bool tuningParamChanged = false;             // set by server handlers when a tuning param is updated
volatile bool manualCommitTuningRequested = false;   // set by UI commit button
volatile bool manualCommitCVTuningRequested = false; // set by UI commit button

ScoreBucket *liveScoreBuckets[4] = {};  // ps_malloc'd — 4 windows × 60 buckets × 8 bytes = 1920 bytes
uint8_t liveScoreHead[4] = {};
uint32_t liveBucketStartMs[4] = {};
float liveScoreVal[4] = {};      // cached computed scores, updated each accumulation tick
float liveScore_lastCmd = 0.0f;  // setpointCommand from previous tick (pre-slew)
float liveScore_thisCmd = 0.0f;  // setpointCommand from current tick (pre-slew)
uint32_t liveScore_lastStepMs = 0;
bool liveScore_inWindow = false;

// CV loop always-running live score — same bucket structure as inner loop
ScoreBucket *cvLiveScoreBuckets[4] = {};  // ps_malloc'd — 4 windows × 60 buckets × 8 bytes = 1920 bytes
uint8_t cvLiveScoreHead[4] = {};
uint32_t cvLiveBucketStartMs[4] = {};
float cvLiveScoreVal[4] = {};
uint32_t cvLiveScore_lastDtMs = 0;  // last time |g_dBcur_dt| crossed the gate threshold
bool cvLiveScore_inWindow = false;

// === CV Loop Tuning Score System ===
float cvWaveAmplitudeV = 0.30f;   // V — target rises by this during HIGH phase (LOW phase sits at the real target)
int cvWavePeriodSec = 30;         // s — full period of CV test wave (one LOW + one HIGH); each half-period = this / 2. Default matches the UI input minimum.
float cvKOvershoot = 10.0f;       // penalty weight on integrated overshoot (user-exposed)
uint8_t cvConsecutiveReads = 10;  // consecutive filtered reads within ±0.1V to declare settled (~1s at 100ms rate)
int CVTuningMode = 0;             // 0=off, 1=on
float cvBaseTarget = 0.0f;        // real ChargingVoltageTarget captured at test start; global so wave gen + scorer share it

const float CV_SETTLE_V_THRESH    = 0.10f;   // V — settling threshold
const float CV_HIGH_DEADBAND_V   = 0.025f;  // V — HIGH phase overshoot dead-band; below this is free
const float CV_LOW_GRACE_SEC     = 1.0f;    // s — grace period from LOW phase start before undershoot scoring begins
const float CV_LOW_RAMP_SEC      = 10.0f;   // s — undershoot weight ramps 0→1 over this window after grace
const float CV_UNDERSHOOT_SCALE  = 0.15f;   // undershoot ISE weight relative to overshoot ISE

struct CVTuningRecord {
  uint16_t runNumber;
  // Results
  float score;
  float avgSettlingTimeSec;
  float worstOvershootV;
  float avgIntegratedOvershootVs;
  float activeTimeSec;
  uint16_t fastOvFires, iExcessFires, loadDumpFires, hardOcFires;
  // CV PI (voltageKd reserved — was D term, now always 0.0)
  float voltageKp, voltageKi, voltageKd;
  // Setpoint shaping
  float setpointRiseRate, setpointFallRate;
  // Integrator management
  float awBleedRate, awRecoverRate;
  uint16_t awSeedProtectMs;
  float reseedFrac;
  float slopeBleedK;  // A/(V/s) — slope-aware integrator bleed gain (column "KS")
  // FastOV supervisor
  float kHard;
  // iExcess
  float iExcessK;
  int iExcessN;
  float iExcessKBleed;
  // Load dump
  float loadDumpDtThresh, loadDumpDtThresh1, loadDumpDtThresh3;
  // Filter
  float inputFilterTC;
  // Test setup
  float waveAmplitudeV;
  uint16_t wavePeriodSec;
  float kOvershoot;
  uint8_t consecutiveReads;
  // Operating conditions
  float avgRPM, avgAltTempF;
  float battVAtStart, socAtStart;
  float chargingVoltageTarget;
  // Low phase results (step-down response)
  float lowScore;
  float avgLowSettlingTimeSec;
  float avgLowIntOvVs;  // avg integrated overvoltage above lowTarget (V·s)
  float worstLowOvV;          // peak above lowTarget during any scored LOW phase (after zero crossing)
  float worstLowUndershootV;  // peak below lowTarget during any scored LOW phase
};

struct CVTuningScoreState {
  bool waveHigh;
  uint32_t lastToggleMs;
  uint8_t halfPeriodCount;
  bool ringInDone;
  // Per-HIGH-phase state (reset each toggle-to-high)
  uint32_t phaseStartMs;
  bool phaseSettled;
  uint8_t consecutiveInBand;
  // Accumulators across scored HIGH phases
  uint8_t scoredHighCount;
  float totalSettlingTimeSec;
  float worstOvershootV;
  float totalIntegratedOvershootVs;
  float activeTimeSec;
  // Protection deltas (snapshotted at start of each HIGH phase)
  uint32_t fastOvSnap, iExcessSnap, loadDumpSnap, hardOcSnap;
  uint16_t fastOvFires, iExcessFires, loadDumpFires, hardOcFires;
  // Per-LOW-phase state (reset each toggle-to-low after ring-in)
  uint32_t lowPhaseStartMs;
  bool lowPhaseSettled;
  uint8_t lowConsecInBand;
  // Accumulators across scored LOW phases
  uint8_t scoredLowCount;
  float totalLowSettlingTimeSec;
  float totalLowIntOvVs;       // ISE of re-overshoot above lowTarget (only after zero crossing) × dt
  float worstLowOvV;           // peak above lowTarget during any scored LOW phase (after zero crossing)
  float totalLowUndershootVs;  // weighted ISE of voltage below lowTarget (time-ramped, ×CV_UNDERSHOOT_SCALE)
  float worstLowUndershootV;   // peak below lowTarget (V, absolute) during any scored LOW phase
  bool lowCrossedBelow;        // true once IBV has crossed below lowTarget in current LOW phase
  // Protection snaps for LOW phases
  uint32_t lowFastOvSnap, lowIExSnap, lowLdSnap, lowHocSnap;
  // Operating conditions
  float battVAtStart, socAtStart;
  bool testStarted;
  // Averages
  float rpmSum, tempSum;
  uint16_t avgSampleCount;
};

CVTuningRecord *cvTuningLog = nullptr;  // ps_malloc(50 × sizeof(CVTuningRecord))
uint8_t cvTuningLogCount = 0;
uint8_t cvTuningLogHead = 0;
uint16_t cvTuningRunCounter = 0;
CVTuningScoreState cvTuningScore = {};
bool cvTuningParamChanged = false;

// ===== THERMAL STEP TEST TUNING =====

struct ThermalTuningRecord {
  uint16_t runNumber;
  float score;  // avgSettlingTimeSec + Ko×avgIntOverFs + Ku×avgIntUnderFs
  float avgSettlingTimeSec;
  float worstOvershootF;  // peak above HIGH setpoint across all scored steps
  float avgIntOverFs;     // avg integral of temp above HIGH setpoint per step (°F·s)
  float avgIntUnderFs;    // avg integral of temp below HIGH setpoint after settle per step
  uint16_t scoredStepCount;
  float activeTimeSec;
  // Tuning parameters at test time
  float kp, ki;
  float lookaheadSec;
  float filterAlpha;
  uint16_t intervalMs;
  float waveLowF;
  float waveHighF;
  float waveHalfPeriodMin;
  // Slew rates at test time
  float riseRate;  // ThermalPenaltyRiseRate (A/s)
  float fallRate;  // ThermalPenaltyFallRate (A/s)
  // Operating conditions
  float avgRPM;
  float avgAmbientF;
};

struct ThermalTuningScoreState {
  bool testStarted;
  bool waveHigh;          // true = currently in HIGH phase
  uint32_t lastToggleMs;  // ms when HIGH phase started (for half-period timer)
  // LOW phase stability tracking (must stabilize at thermalWaveLowF before stepping up)
  bool lowPhaseStable;      // true once temp has been within ±thermalSettleThreshF for thermalConsecutiveReads
  uint8_t lowConsecInBand;  // consecutive in-band reads while in LOW phase
  // Per-HIGH-phase state (reset on each step-up)
  uint32_t phaseStartMs;
  bool phaseSettled;        // temperature entered settle band this phase (informational only)
  uint32_t phaseSettledMs;  // ms when first settled (informational only)
  uint8_t consecutiveInBand;
  float intOverFs;        // integral of max(0, temp - thermalWaveHighF) × dt this phase
  float intUnderFs;       // integral of max(0, thermalWaveHighF - temp) × dt from step-up (entire phase)
  float worstOvershootF;  // peak above thermalWaveHighF this phase
  // Accumulators across scored HIGH phases
  float totalSettlingTimeSec;  // informational — not included in score formula
  float totalIntOverFs;
  float totalIntUnderFs;
  float worstOvAll;
  uint16_t scoredStepCount;
  float activeTimeSec;
  // Conditions snapshot
  float rpmSum;
  float ambientSum;
  uint16_t avgSampleCount;
};

// Thermal tuning mode settings (persisted to LittleFS)
int ThermalTuningMode = 0;               // 0=off, 1=on
float thermalWaveLowF = 120.0f;          // LOW phase setpoint (°F)
float thermalWaveHighF = 150.0f;         // HIGH phase setpoint (°F)
float thermalWaveHalfPeriodMin = 10.0f;  // minutes per half-period
float thermalKOvershoot = 10.0f;         // ISE penalty weight for above-setpoint (10× harder than undershoot)
float thermalKUndershoot = 1.0f;         // ISE penalty weight for below-setpoint
float thermalSettleThreshF = 2.0f;       // ±°F band for settled check
uint8_t thermalConsecutiveReads = 3;     // consecutive in-band reads to declare settled
bool thermalTuningParamChanged = false;

float thermalWaveCurrentSetpointF = 0.0f;  // active wave setpoint; 0 = not started

ThermalTuningRecord *thermalTuningLog = nullptr;  // ps_malloc(50 × sizeof(ThermalTuningRecord))
uint8_t thermalTuningLogCount = 0;
uint8_t thermalTuningLogHead = 0;
uint16_t thermalTuningRunCounter = 0;
ThermalTuningScoreState thermalTuningScore = {};

// Thermal always-on live score buckets (30m, 3h, 24h, 7d — thermal system has long time constants)
const uint32_t THERMAL_LIVE_BUCKET_MS[4] = { 1800000UL, 10800000UL, 86400000UL, 604800000UL };
ScoreBucket *thermalLiveScoreBuckets[4] = {};  // ps_malloc'd
uint8_t thermalLiveScoreHead[4] = {};
uint32_t thermalLiveBucketStartMs[4] = {};
float thermalLiveScoreVal[4] = {};

float xTime = 60.0;     // seconds    PID Chart
int yyMax = 105;        // PID Chart     Amps
int yyMin = -25;        //  PID Chart Amps
float pidError = 0.0f;  // PID error for display (A)


// (accelEnabled global removed 2026-05-26 — accelerometer is always on; UI toggle/LittleFS persistence purged)

// =====================================================================================
// === CV LOOP PARAMETERS — all tunable values consolidated here
// === Note: fastOV thresholds (V_SOFT=+0.08V, V_HARD=+0.15V, PRED_GUARD=0.06V,
// ===       TD_PRED=0.08s, HARD_CLAMP_HYST=0.08V) are local const float inside
// ===       AdjustFieldLearnMode() in 6_functions.ino — not globals.
// =====================================================================================
// --- Output current PID ---
float PidKp = 0.5f;   // A/% duty — proportional gain
float PidKi = 2.0f;   // integral gain
float PidKd = 0.01f;  // derivative gain
// --- Voltage (CV) PID ---
volatile float VoltageKp = 30.0f;    // A/V — proportional gain (volatile: written from Core 0 web handler, read from Core 1 PID)
volatile float VoltageKi = 15.0f;    // A/(V·s) — integral gain; above-target unwind uses KiDown = 7×VoltageKi
// VoltageKd removed — D term was always 0 and is redundant with slope-aware integrator bleed (SlopeBleedK).
float SlopeBleedThresh = 0.50f;      // V/s — integrator bleed activates when cvDSlope exceeds this
float SlopeBleedK = 50.0f;          // A/(V/s) — bleed rate: per V/s of excess slope, drain this many A/s from cv_I
float SlopeBleedProxV = 0.20f;      // V — proximity gate: bleed scales linearly from 0 (e >= ProxV) to full (e <= 0); default 0.50→0.20, 2026-06-10
uint32_t VoltageLoopInterval = 100;  // ms — PI fires at this interval
float VoltageTargetRiseRate = 0.3f;  // V/s — governor slew rate for voltage target rises only
// --- FastOV supervisor ---
float KHard = 35.0f;  // A/V — OV cap slope (Group 1: Vpred > target+OvPredMarginV; Group 2: IBV > target+OvMeasMarginV)
bool  OvGroup1Enable  = true;   // Group 1 — prediction-based cap enable (Vpred > target + OvPredMarginV)
bool  OvGroup2Enable  = true;   // Group 2 — measured-voltage threshold enable (IBV > target + OvMeasMarginV)
int   IExcessSigSrc   = 0;      // Group 3 — 0=MA(N), 1=EMA(TC), 2=Raw
int   IExcessMA_N     = 2;      // Group 3 — MA window size (1–10 samples)
int   OutputPIDSigSrc = 0;      // Output current PID — 0=EMA(TC), 1=MA(N), 2=Raw
int   OutputPIDMA_N   = 2;      // Output current PID — MA window size (1–10 samples)
float TdPred         = 0.045f;  // Group 1 lookahead horizon (s)
float OvMeasMarginV  = 0.100f;  // Group 2 measured-voltage trigger margin above target (V)
float OvPredMarginV  = 0.150f;  // Group 1 prediction trigger margin above target (V)
float DvdtTC         = 58.0f;   // ms — TC for dvdt (rate-of-rise) EMA fed into Vpred. dt-aware: alpha = dt/(TC+dt). Was DvdtAlpha (constant alpha); renamed 2026-05-22.
// --- iExcess current supervisor ---
float IExcessK = 5.0f;           // A above setpoint to arm supervisor
int IExcessN = 3;                // consecutive ticks required (3 ≈ 15ms, tuned for 28Hz belt resonance on this install)
float IExcessKBleed = 0.0f;      // 0=snap-to-zero; >0=proportional bleed rate (A/s per A of excess)
float IExcessArmMarginV = 0.200f; // V below target at which iExcess voltage gate opens (decoupled from OvMeasMarginV 2026-05-23)
float ReseedFrac = 0.5f;  // shared: fraction of pre-event cv_I to seed on any protection recovery (was IExcessReseedFrac)
// --- Anti-windup ---
float AwBleedRate = 2.0f;        // fraction of MaxTableValue/s — cv_I bleed rate while fastOV active (2.0×50A=100A/s)
float AwRecoverRate = 0.1f;      // HARDCODED — no longer user-adjustable. cv_I_aw_cap recovery rate (fraction of MaxTableValue/s) after fastOV clears. Only exercised on cold CV re-entry (MANUAL→AUTO, idle→bulk, post-shutdown). CSV3 slot CSV3_reserved_AwRecoverRate held for future use.
uint16_t AwSeedProtectMs = 150;  // ms to suppress AwBleed + CC-tracker after any bumpless seed fires; 0=disabled
float FastSetpointRiseRate = 8.0f;       // multiplier on normal setpoint rise slew during post-protection recovery window
uint32_t FastSetpointRiseWindowMs = 5000; // hard upper bound (ms) on how long the fast-rise window stays open after any protection releases
float FastSetpointRiseHeadroomV = 0.2f;  // V below ChargingVoltageTarget at which fast-rise is allowed; gate closes once IBV climbs into target - this margin
// --- Test-mode protection override ---
// User-controlled flag (per test page). When TRUE (default) G1, G2, G3, and
// AlternatorHardShutdownV all fire normally. When FALSE the user has disabled them
// so step-tests can characterise the plant without protection layers fighting the
// test. NOT persisted — resets to TRUE (enabled) on every boot. G4 (Load Dump),
// INA228 hardware OV, and the hardware OC trip (MaxTableValue+10) stay active
// regardless of this flag.
bool testProtectionsEnabled = true;
// --- CV loop runtime state ---
uint32_t lastVoltageLoopMs = 0;           // timestamp of last voltage loop update
uint16_t g_voltLoopActualIntervalMs = 0;  // actual interval of last voltage loop fire (ms); 0 until second fire
// (voltLoopWorstInterval_5s/_ses removed — replaced by the vl_* interval ladder above)
float g_slopeBleedAmpsThisTick = 0.0f;   // slope bleed drain applied this voltage loop tick (A); cleared by cvLog_tick after logging
float Icv = 0.0f;                         // CV PID output — direct current setpoint (A)
float cv_I = 0.0f;                        // CV integrator state (A)
bool voltageControlActive = false;        // true when voltage PID is active (non-idle stages)
uint32_t thermalScoreLastExternalMs = 0;  // last ms when voltageControlActive was true; gates 3-min blanking
// =====================================================================================
// Table Bounds & Safety
// "Group 0" in UI = hardware overcurrent trip (no protection-group integration yet)
float MaxTableValue = 150.0;               // Maximum table entry (A)
float MaxPenaltyPercent = 15.0;            // Max penalty as % of nominal
unsigned long MaxPenaltyDuration = 60000;  // Max penalty time (ms)

// Advanced Learning
float NeighborLearningFactor = 0.25;                // Neighbor reduction factor
unsigned long LearningMemoryDuration = 2592000000;  // How long to remember events (30 days in ms) SEEMS UNUSED, DELETE LATER

// Safety Overrides
int IgnoreLearningDuringPenalty = 1;  // Block learning during penalty
// EnableNeighborLearning removed — write-only with no consumer (see note above on LearningPaused).
int EnableAmbientCorrection = 0;      // Apply temperature correction

// Diagnostics & Debugging
// ShowLearningDebugMessages / LearningDryRunMode removed — write-only with no consumer.
int LogAllLearningEvents = 0;       // Log every learning decision
int CloudFeatures = 1;

// Data Management
// Deferred saves — set by Core 0 (AsyncWebServer handlers), executed on Core 1 in main loop
// to avoid blocking SSE delivery on Core 0
volatile bool pendingSaveCVTuningLog = false;
volatile bool pendingSaveTuningLog = false;
volatile bool pendingSaveThermalTuningLog = false;
volatile bool pendingSaveSystemIDLog = false;
volatile bool pendingResetAlternatorHealth = false;
volatile bool pendingResetBoatPerformance = false;
volatile bool pendingClearOverheatHistory = false;
volatile bool pendingSaveUserTableEdits = false;
volatile bool pendingSaveVesselInfo = false;
bool pendingShutdownFlush = false;     // set on ignition-off edge; cleared after full flush
bool shutdownNVSFlushDone = false;     // true once NVS+sensor window saved this shutdown
uint32_t shutdownCloudDeadlineMs = 0;  // millis() deadline for cloud drain window

// Field-off flush triggers — two staggered gates off the same field-off edge.
// +5s:  saveNVSDataFull()      drains the storage namespace to NVS (heavy commit).
// +13s: altHealthSave() writes the alt-health blobs (/altbase.bin + 3 more) to LittleFS (~67 KB).
// Staggered so the LittleFS write doesn't pile onto the NVS commit's flash
// relocation tail. Both re-arm on the next field-on edge. Independent of
// fieldOffSettled() (which has a 60s baseline used by cloud/network callers).
bool fieldOffFlushDone = false;         // NVS drain done this field-off window
bool fieldOffMatrixFlushDone = false;   // matrix write done this field-off window
int8_t lastFieldStateForFlush = -1;    // -1 = uninit; 0 = field off; 1 = field on
uint32_t fieldOffEdgeMs = 0;           // millis() captured at most recent field-on -> field-off edge


uint32_t perfCountersResetMs = 0;

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
  REASON_HARD_OVERCURRENT,
  REASON_RPM_TOO_LOW,
  REASON_CURRENT_STALE
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
  bool ignoreRPM;
  bool rpmBelowMinimum;

  bool voltagePlausible;
  bool voltageDisagreementCritical;
  bool voltageDisagreementWarning;

  bool inLockout;

  float bulkVoltage;
  float alternatorHardShutdownV;
  bool testProtectionsEnabled;  // snapshot of the per-tick value so the decision logic sees a stable flag

  float tempToUseF;
  float tempLimitF;
  float tempWarnExcessF;
  float tempCritExcessF;
  bool tempSourceIsAlt;

  bool inaOvervoltageLatched;

  bool currentDataStale;

  bool inAbsorptionStage;
};

// ==================== CONFIGURABLE PARAMETERS ====================
// Expose these in web UI for tuning

// --- Rate Limiting (LM2907 coupling cap protection) ---
float DutyRampRate = 50.0f;  // %/sec - max rate of duty cycle change (protects coupling cap from harsh transitions, includes OnOff toggle!)
// Asymmetric setpoint slew
float SetpointRiseRate = 30.0f;  // A/sec
float SetpointFallRate = 50.0f;  // A/sec
float StartupRiseRate  = 3.0f;   // A/sec — setpoint slew rate applied only on field turn-on (OFF/FAULT→AUTO); user-adjustable
bool  inStartupRamp    = false;  // true from field turn-on until setpointLimited catches up to command

// --- Settle Time Before GPIO4 Cut ---
uint32_t SettleTimeBeforeCut = 1000;  // ms - how long duty must be at 0% before GPIO4 goes LOW

// --- Temperature Thresholds (°F above TemperatureLimitF) ---
float TempWarnExcess = 2.0f;             // °F above limit triggers WARNING ramp, starts lockout
float TempCritExcess = 10.0f;            // °F above limit triggers IMMEDIATE GPIO4 cut (skips Phase 1 ramp)
uint32_t TempSustainedTimeout = 120000;  // ms - WARNING temp sustained this long triggers GPIO4 cut (2 min default)

// --- Voltage Thresholds ---
// AlternatorHardShutdownV: absolute battery voltage above which the alternator field is cut
// and a cooldown lockout starts. Should be set just below the battery BMS shutdown voltage.
// This is the only software-layer hard OV shutdown; below it sit the Group 1/2/3 throttling
// protections, alongside it sits the INA228 hardware ALERT pin (same default threshold of
// BulkVoltage + 0.3 V but using the chip's slow-averaged value, so it acts as a hardware
// backup that fires after the software on slow ramps and slightly after on fast transients).
float AlternatorHardShutdownV = 14.8f;    // V — absolute hard-shutdown threshold; this 14.8 is only the in-RAM seed for first boot on a 12V system. First-boot init in 4_functions.ino overwrites it with BulkVoltage + 0.3 V so 24V/48V systems get sensible defaults (29.1 V / 57.9 V).
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

// Fast OV supervisor instrumentation — unified across all protection paths
// (Group 1/2 OV, iExcess, LoadDump). Exported from the bumpless tracker block
// in AdjustFieldLearnMode after every supervisor has voted, so all three flags
// below reflect the true combined state, not just Group 1/2.
float g_fastOvCurrentCap = 0.0f;   // live unified cap ceiling this tick (amps)
volatile bool g_fastOvClampActive = false;  // true if ANY protection capped current this tick
uint32_t g_fastOvClampCount = 0;   // rising-edge counter across all protections

float g_I_cap = 0.0f;  // RPM table current ceiling this tick (A); set each AUTO tick



// ===== RPM TABLE BREAKPOINTS =====
// 10 evenly-spaced RPM points covering the alternator's full operating range.
// Linear interpolation is used between points. Values at or below the first
// breakpoint (100 RPM) use the first entry directly — no extrapolation below it.
#define RPM_TABLE_SIZE 10
int rpmTableRPMPoints[RPM_TABLE_SIZE] = { 100, 600, 1100, 1600, 2100, 2600, 3100, 3600, 4100, 4600 };
// Factory defaults for RPM breakpoints
int defaultRPMValues[RPM_TABLE_SIZE] = { 100, 600, 1100, 1600, 2100, 2600, 3100, 3600, 4100, 4600 };

// ===== TARGET CURRENT TABLE =====
// Maximum amps the regulator will command at each RPM breakpoint in Normal mode (HiLow=1).
// In Low mode (HiLow=0) the regulator quarters these values before sending to the PID,
// so Normal=50A → Low=12A (closest integer) automatically — no separate Low-mode table is needed.
// The first entry (≤100 RPM = effectively stopped) is 0 to guarantee no field
// current when the alternator is not spinning. The factory reset button restores
// defaultCurrentValues below.
float rpmCurrentTable[RPM_TABLE_SIZE] = { 0, 50, 50, 50, 50, 50, 50, 50, 50, 50 };
float defaultCurrentValues[RPM_TABLE_SIZE] = { 0, 50, 50, 50, 50, 50, 50, 50, 50, 50 };

// ===== CAP CURRENT TABLE =====
// Hard ceiling on commanded current at each RPM, always enforced regardless of
// mode or PID output. Exists to protect the belt, shaft, and mounting hardware
// from mechanical overload at any RPM. The target table above cannot push current
// above this ceiling. Factory reset restores defaultCapCurrentValues.
float rpmCapCurrentTable[RPM_TABLE_SIZE] = { 0, 50, 50, 50, 50, 50, 50, 50, 50, 50 };
float defaultCapCurrentValues[RPM_TABLE_SIZE] = { 0, 50, 50, 50, 50, 50, 50, 50, 50, 50 };

// ===== CAP POWER TABLE =====
// Alternative cap expressed in kW instead of amps (active only when capLimitMode=1).
// The firmware converts to an equivalent amp limit using live battery voltage.
// All zeros = disabled (no power cap). capLimitMode selects which cap is active.
float rpmCapPowerTable[RPM_TABLE_SIZE] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
float defaultCapPowerValues[RPM_TABLE_SIZE] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
uint8_t capLimitMode = 0;  // 0 = use amp cap (rpmCapCurrentTable), 1 = use kW cap (rpmCapPowerTable)

// ===== MINIMUM FIELD DUTY TABLE =====
// Minimum PWM duty cycle (%) applied to the field at each RPM breakpoint.
// Prevents the RPM signal from dropping out due to rapid stator signal changes. Higher values at low RPM because the alternator needs more field
// excitation to produce useful output when spinning slowly.
float rpmMinDutyTable[RPM_TABLE_SIZE] = { 5.0, 5.0, 4.0, 4.0, 3.0, 3.0, 2.0, 2.0, 1.0, 1.0 };
float defaultMinDutyValues[RPM_TABLE_SIZE] = { 5.0, 5.0, 4.0, 4.0, 3.0, 3.0, 2.0, 2.0, 1.0, 1.0 };

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
// Output current loop (currentPID) — units: duty cycle %
// P is exact. D is from error derivative. I is residual (output - P - D).
float innerTermP = 0.0f;
float innerTermI = 0.0f;
float innerTermD = 0.0f;

// Temperature loop (tempPID) — units: amps
// Only updated when tempPID.Compute() returns true (~every TempPIDIntervalMs).
// Values hold between computes, which is correct — the integrator state is stable.
float outerTermP = 0.0f;
float outerTermI = 0.0f;
float outerTermLookahead = 0.0f;   // look-ahead share of outerTermP: Kp × max(0, projected − present temp), penalty-signed. Replaced always-zero outerTermD (Kd is hardwired 0; derivative action lives in the projected input)
float thermalSlopeFPerSec = 0.0f;  // long-window slope estimate (°F/sec); replaces outerTermDExternal

volatile bool tempPIDResetRequested = false;
volatile bool innerPIDResetRequested = false;
volatile bool cvLoopResetRequested = false;

float tempFiltered = NAN;
float thermalPenaltyLastValid = 0.0f;
// Slope ring buffer: THERMAL_SLOPE_BUF readings × 5s = 60s window (12 intervals)
#define THERMAL_SLOPE_BUF 13
float thermalSlopeBuffer[THERMAL_SLOPE_BUF];
uint8_t thermalSlopeBufIdx = 0;
bool thermalSlopeBufFull = false;
float projectedTempF = NAN;           // tempNow + slopeF_per_sec × ThermalLookaheadSec — PID process variable
uint32_t thermalSlopeLastPushMs = 0;  // gates slope buffer push to TempPIDIntervalMs cadence

float cvDSlope = 0.0f;              // V/s — mirrors g_fastOvDvdt; used by slope-aware integrator bleed (SlopeBleedK)

float outerImpliedPenalty = 0.0f;
bool outerAntiWindupFired = false;
// Sticky version of outerAntiWindupFired: set on any CV-bleed event, cleared when CSV2 captures it.
// outerAntiWindupFired resets every tempPID_tick (~16Hz) so the 5s CSV2 frame would otherwise miss it.
volatile bool thermalAntiWindupLatch = false;

// ===== CALCULATED VALUES FOR DISPLAY =====
float learningTargetFromRPM = -1.0;  // Table lookup result before corrections
float ambientTempCorrection = 0.0;   // Calculated temp correction (A)
float finalLearningTarget = 0.0;     // After all corrections applied
float ambientTemp = NAN;             // Current ambient temperature (°F)
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

// ===== TEMPERATURE LOOP PID (replaces thermal model) =====
// Tuning — all web UI configurable
float TempPIDKp = 3.0f;             // A/°F proportional gain
float TempPIDKi = 0.1f;             // A/(°F·s) integral gain — must wind the full steady-state penalty alone (P contributes nothing at zero error)
float ThermalLookaheadSec = 60.0f;  // prediction horizon: project this many seconds ahead; size ~= plant dead time (~20s measured) + slope-estimator latency (~30s), NOT the settling time constant

float ThermalPenaltyRiseRate = 60.0f;  // A/s — how fast penalty can increase (restrict current)
float ThermalPenaltyFallRate = 20.0f;  // A/s — how fast penalty can decrease (allow more current)

float WarmupRampRate = 0.0f;      // A/s — rate at which output ceiling rises from 0 on field enable; 0 = disabled
float warmupCeiling = 0.0f;       // runtime warmup ceiling (not persisted)
float prevThermalPenalty = 0.0f;  // file-scope, tracks previous slew-limited value

uint32_t TempPIDIntervalMs = 5000;  // Temperature loop update period (ms) — independent of output current loop and sensor rate
float TempPIDFilterAlpha = 0.2f;    // IIR smoothing for DS18B20 (0=frozen, 1=raw); feeds slowly at 16Hz on a frozen 5s sample
// ThermistorFilterAlpha removed — hardcoded 0.02f in tempPID_tick IIR filter, not user-configurable
// Runtime state — expose via telemetry
double tempPIDInput_d = 77.0;        // PID process variable (°F) = max(projected, present) — projected = filtered + slope × lookahead
double tempPIDSetpoint_d = 0.0;      // Setpoint = TemperatureLimitF (real damage limit)
bool tempPIDActive = false;          // true when temperature PID is in AUTO
bool tempFilterNeedsReseed = false;  // Set true to force IIR cold-start on next tempPID_tick()

float thermalPenaltyAmps = 0.0f;    // temperature PID output: amps subtracted from target table
double thermalPenaltyAmps_d = 0.0;  // double version for PID library

//REVERSE because rising temperature should increase the penalty
PID tempPID(&tempPIDInput_d, &thermalPenaltyAmps_d, &tempPIDSetpoint_d, TempPIDKp, TempPIDKi, 0.0, REVERSE);




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
  int16_t tempFiltered;   // actual IIR-filtered sensor reading
  int16_t tempProjected;  // projectedTempF = filtered + slope × lookahead (what PID sees)
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
  int16_t outerTermLookahead;  // repurposed from always-zero outerTermD; CSV column renamed to "lookahead"
  int16_t impliedPenalty;
  int16_t thermalSlope;  // thermalSlopeFPerSec × 1000 (0.001 °F/sec per count)
  // gainKp/Ki/Lookahead written once in pidlog CONST row
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
#define CV_LOG_HEADER_SIZE 36
#define CV_LOG_ENTRY_SIZE 50

volatile bool pidLogPaused = false;
volatile uint32_t pidLogPausedAtMs = 0;

struct PidLogEntry {
  // ── Timestamp ────────────────────────────────────────────────────
  uint32_t ts;  // millis() at log point

  // ── Mode / Stage ─────────────────────────────────────────────────
  uint8_t chargeStageDisplay;  // getChargeStageDisplayCode() enum value
  uint8_t TargetVoltageMode;   // runtime TargetVoltageMode flag (0 or 1)
  uint8_t flags;               // bit0=AUTO bit1=voltCtrl bit4=govBypass
  uint8_t ovFlags;             // bit0=fastOvActive bit1=reserved(was softClamp) bit2=hardClamp bit3=iExcess bit4=loadDumpActive

  // ── CV loop ──────────────────────────────────────────────────────
  float battV;                  // tick.currentBatteryVoltage
  float ChargingVoltageTarget;  // target voltage this tick
  float vError;                 // ChargingVoltageTarget - battV (always fresh)
  float Icv;                    // CV position-form PI output — direct current setpoint (A)
  float cv_I;                   // CV position-form PI integrator state
  float tableThermalLimit;      // uTargetAmps before CV — RPM cap minus thermal penalty
  float setpointCmd;            // value fed to setpointCommand (Icv in CV, tableThermalLimit in bulk)

  // ── Voltage loop event flags ─────────────────────────────────────
  uint8_t voltageLoopRanThisTick;     // 1 only when interval/enteringCV fired
  uint8_t enteringCV;                 // 1 on first CV tick (transition only)
  uint8_t enteringTargetVoltageMode;  // 1 on mode entry tick only
  uint8_t pad1;

  // ── Output current PID ───────────────────────────────────────────
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
  float innerKp;    // PidKp  — inner output-current PID gain (NOT voltage loop Kp)
  float innerKi;    // PidKi  — inner output-current PID gain (NOT voltage loop Ki)
  float innerKd;    // PidKd  — inner output-current PID gain (NOT voltage loop Kd)
  float voltageKp;  // VoltageKp — outer voltage loop proportional gain (A/V)
  float voltageKi;  // VoltageKi — outer voltage loop integral gain (A/(V·s))
  float voltageKd;  // reserved — was VoltageKd (D term removed); always 0.0
  // ── Filtered signals ─────────────────────────────────────────────
  float battV_filt;  // IBV
  float iMeas_filt;  // MeasuredAmps_filtered
  // ── Protection flags & signals ───────────────────────────────────────────
  float dBcur_dt;    // g_dBcur_dt (A/s) — battery current derivative for load dump
  float battI;       // getBatteryCurrent() (A) — INA228 or Victron
  // ── Timing diagnostics ───────────────────────────────────────────────────
  int16_t ch1IntervalMs;       // g_ch1LastIntervalMs — CH1 inter-sample gap this tick (ms)
  int16_t voltLoopIntervalMs;  // actual voltage loop interval when fired this tick (ms); 0 if not fired
  int16_t inaIntervalMs;       // ina_last_ms — INA228 read freshness (ms)
  int16_t pad2;                // alignment pad
};                   // 132 bytes — naturally aligned, no implicit holes

struct PidDLState {
  int count;
  int oldest;
  int row;
  bool done;
  char line[440];  // header row = 402 chars; comment block = ~711 chars (truncated fine); was 320, too small
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
// Logs every CH1 sample (~213 Hz / ~4.7ms interval) — no internal rate limiter.
// Binary download via /cvlog.bin, decoded to CSV by JS.
// 42 bytes/entry × 6000 entries = 252 KB PSRAM → ~28 sec at full rate.
// ===========================================================================

// ---------------------------------------------------------------------------
// STRUCT  (42 bytes, offsets below match JS parser exactly)
// ---------------------------------------------------------------------------
//
//  offset  field            scale      notes
//  ──────  ─────            ─────      ─────
//   0      ts               raw ms     millis()
//   4      battV            ×100       IBV (raw bus voltage)
//   6      targV            ×100       ChargingVoltageTarget
//   8      vErrorMv         ×1000      (target − batt), millivolts resolution
//  10      dvdt_x1000       ×1000      filtered dV/dt, signed
//  12      vPred            ×100       IBV + TdPred × max(0,dvdt)
//  14      fastOvCap        ×10        fastOvCurrentCap ceiling this tick
//  16      cv_I_x10         ×10        cv_I integrator state
//  18      Icv_x10          ×10        Icv PI output (current setpoint to output current loop)
//  20      uTarget          ×10        uTargetAmps (table+thermal+user ceiling)
//  22      spLimited        ×10        setpointLimited (slewed command to PID)
//  24      iMeas            ×10        MeasuredAmps (raw alternator current)
//  26      duty             ×10        dutyCycle
//  28      flags            —          see bit definitions below
//  29      pad              —          zero
//  30      rpm              raw        RPM, clamped to int16 range
//  32      battV_filt_x100  ×100       IBV (raw battery voltage)
//  34      iMeas_filt_x10   ×10        MeasuredAmps_filtered (EMA)
//  36      ch1IntervalMs    raw ms     last CH1 inter-sample gap
//  38      cvDSlope_x10000  ×10000     cvDSlope (500ms backward diff on filtered V)
//  40      battI_x10        ×10        getBatteryCurrent() — INA228 or Victron
//  42      dBcur_dt_Aps     raw A/s    g_dBcur_dt clamped to int16
//  44      voltLoopIntervalMs  raw ms  actual voltage loop interval when fired this tick; 0 if not fired
//  46      inaIntervalMs       raw ms  ina_last_ms at log time — INA228 read freshness
//  48      slopeBleedAmps_x1000 ×1000  cv_I drain applied this voltage loop tick (A×1000); 0 on non-VL ticks
//
//  flags bits:
//    b0  fastOvActive    any OV clamp fired this tick
//    b1  voltLoopFired   voltage PI ran this tick (100ms cadence)
//    b2  cvActive        voltageControlActive
//    b3  reserved        (was softClamp — old soft-cap removed)
//    b4  hardClamp       Group 1 (prediction cap) or Group 2 (voltage threshold) applied
//    b5  iExcess         Group 3 (iExcess supervisor) fired this tick
//    b6  loadDumpActive  load dump feedforward active this tick




struct __attribute__((packed)) CvLogEntry {
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
  uint8_t flags;   // b0=fastOvActive b1=voltLoopFired b2=cvActive b3=reserved(was softClamp) b4=hardClamp b5=iExcess b6=loadDumpActive
  uint8_t awState; // 0=normal 1=frozen(supervisor) 2=saturated 3=bleeding 4=bumpless
  int16_t rpm;
  int16_t battV_filt_x100;  // IBV × 100    (V)
  int16_t iMeas_filt_x10;   // MeasuredAmps_filtered × 10 (A)
  int16_t ch1IntervalMs;    // last CH1 inter-sample gap   (ms)
  int16_t cvDSlope_x10000;  // cvDSlope × 10000 (V/s × 10000 → ~0.0001 V/s per count)
  int16_t battI_x10;        // getBatteryCurrent() × 10 (A) — INA228 or Victron
  int16_t dBcur_dt_Aps;       // g_dBcur_dt clamped to int16 (A/s) — load dump derivative
  int16_t voltLoopIntervalMs;      // actual voltage loop interval when fired this tick (ms); 0 if not fired
  int16_t inaIntervalMs;           // ina_last_ms at log time — INA228 read freshness (ms)
  int16_t slopeBleedAmps_x1000;    // cv_I drain applied this voltage loop tick (A × 1000); 0 on non-VL ticks
  uint8_t capReason;               // which layer set fastOvCap this tick: 0=none 1=KHard_G1 2=KHard_G2 3=iExcess 4=loadDump
};
static_assert(sizeof(CvLogEntry) == 51, "CvLogEntry must be 51 bytes");


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
// BINARY HEADER  (36 bytes)
// offset  field           type      notes
//   0     count           uint32    number of valid entries
//   4     entrySize       uint32    = 51
//   8     voltageKp       float     VoltageKp at download time
//  12     voltageKi       float     VoltageKi at download time
//  16     voltageInterval uint32    VoltageLoopInterval ms
//  20     reserved        float     (was VoltageKd — D term removed; always 0.0)
//  24     sbThresh        float     SlopeBleedThresh (V/s)
//  28     sbK             float     SlopeBleedK (A/(V/s))
//  32     sbProxV         float     SlopeBleedProxV (V)
// ---------------------------------------------------------------------------

static CvLogEntry *cvLog = nullptr;
bool loggingActive = true;  // Stop/Start Logs: false freezes thermal/PID/CV ring buffers
static int cvLogHead = 0;
static int cvLogCount = 0;
static bool cvLogReady = false;
static bool cvLogPaused = false;
static uint32_t cvLogPausedAtMs = 0;


float g_fastOvDvdt = 0.0f;        // filtered dV/dt (V/s), updated every IBV fresh tick
float g_dBcur_dt = 0.0f;          // dBcur/dt (A/s), updated every INA228 read; positive = load dump
float g_fastOvVpred = 0.0f;       // predicted voltage, updated when voltageControlActive
bool g_fastOvHardActive = false;  // K_HARD or hysteresis block fired this tick
uint32_t g_fastOvHardCount = 0;

// Which protection layer actually set the final fastOvCurrentCap this tick (the BINDING
// constraint — lowest cap wins). Lets the CV log distinguish "KHard fired" from "KHard
// fired but iExcess/loadDump was the real limit". 0 = cap left at base (unclamped).
enum FastOvCapReason : uint8_t {
  CAP_REASON_NONE     = 0,  // cap at fastOvBaseCap — no protection bound
  CAP_REASON_KHARD_G1 = 1,  // Group 1 predictive KHard (Vpred)
  CAP_REASON_KHARD_G2 = 2,  // Group 2 measured KHard / hysteresis hold (IBV)
  CAP_REASON_IEXCESS  = 3,  // iExcess supervisor
  CAP_REASON_LOADDUMP = 4,  // load-dump cutoff
};
uint8_t g_fastOvCapReason = CAP_REASON_NONE;  // exported once per full control tick alongside g_fastOvCurrentCap (see capReasonTick in AdjustFieldLearnMode)

// ── Current ring / MA / dI/dt ─────────────────────────────────────────────
// Written in ADS case 1, read by AdjustFieldLearnMode and cvLog_tick
#define I_RING_SIZE 10
struct IAmpEntry {
  uint32_t ts;
  float val;
};
static IAmpEntry iAmpRing[I_RING_SIZE];
static uint8_t iAmpHead = 0;
static uint8_t iAmpCount = 0;

float g_iMA_N   = 0.0f;   // MA(N) where N = IExcessMA_N (iExcess signal)
float g_pidMA_N = 0.0f;   // MA(N) where N = OutputPIDMA_N (Output Current PID signal)
uint16_t g_ch1LastIntervalMs = 0;  // last CH1 inter-sample gap, for cvLog

bool g_iExcessActive = false;
float g_iExcessDutyCap = 100.0f;

// Load dump detection via dBcur/dt — three-tier cascade
float LoadDumpDtThresh1 = 4000.0f;  // A/s — tier 1: fires on a SINGLE sample above this (hard-switched FET disconnects)
float LoadDumpDtThresh  = 1500.0f;  // A/s — tier 2: fires when TWO consecutive samples both exceed this; noise ceiling ~354 A/s consecutive
float LoadDumpDtThresh3 = 1000.0f;  // A/s — tier 3: fires when THREE consecutive samples all exceed this (slow relay-contact disconnects)
volatile bool g_loadDumpActive = false;
uint8_t g_awState = 0;  // integrator AW state: 0=normal 1=frozen(supervisor) 2=saturated 3=bleeding 4=bumpless
uint32_t g_loadDumpCount = 0;

// Protection event counters — rising-edge, cleared by web UI reset buttons
uint32_t g_iExcessCount = 0;
uint32_t g_inaOVCount = 0;
uint32_t g_hardOCCount = 0;
uint32_t g_voltSpikeCount = 0;
uint32_t g_voltDisagreeCritCount = 0;
uint32_t g_voltDisagreeWarnCount = 0;
uint32_t g_voltImplausibleCount = 0;
uint32_t g_tempCritCount = 0;
uint32_t g_tempSustainedCount = 0;
uint32_t g_tempStaleCount = 0;
uint32_t g_currentStaleCount = 0;

// CV loop tunable parameters moved to the CV LOOP PARAMETERS block above (~line 1860)

//additional leaderboard stuff
float sailing_days_alltime = 0.0;             // Total sailing days (lifetime)
float sailing_ratio = 0.0;                    // % of time spent sailing (calculated)
float sailing_dist_alltime = 0.0f;            // nm under sail / engine-off (lifetime)
float alt_power_max_alltime_w = 0.0f;         // peak alternator power, W (lifetime)
float solar_power_max_alltime_w = 0.0f;       // peak solar power, W (lifetime)
float VictronSolarPower_W = 0.0f;             // live solar panel power, W (VE.Direct PPV)
float VictronSolarVoltage_V = 0.0f;           // live solar panel voltage, V (VE.Direct VPV)
float VictronSolarCurrent_A = 0.0f;           // live solar panel current, A (PPV / VPV)
int VictronChargeState = -1;                  // VE.Direct CS code (0=Off,3=Bulk,4=Abs,5=Float,7=Equalize); -1 = unknown
int VictronMPPTMode = -1;                     // VE.Direct MPPT code (0=Off,1=V/I limited,2=active MPPT); -1 = unknown
int VictronError = -1;                        // VE.Direct ERR code (0=no error); -1 = unknown
float VictronYieldToday_kWh = 0.0f;           // VE.Direct H20 (0.01 kWh units -> kWh)
float VictronMaxPowerToday_W = 0.0f;          // VE.Direct H21 (W)
float VictronYieldYesterday_kWh = 0.0f;       // VE.Direct H22 (0.01 kWh units -> kWh)
float VictronMaxPowerYesterday_W = 0.0f;      // VE.Direct H23 (W)
float max_wind_speed_true_alltime = 0.0;      // Maximum true wind speed (knots)
float max_wind_speed_apparent_alltime = 0.0;  // Maximum apparent wind speed (knots)
float board_temp_max_alltime = -999.0;        // Maximum board temperature (°F)
float board_temp_min_alltime = 999.0;         // Minimum board temperature (°F)
float baro_pressure_max_alltime = 0.0;        // Maximum barometric pressure (mbar)
float baro_pressure_min_alltime = 9999.0;     // Minimum barometric pressure (mbar)
float best_upwind_vmg_alltime = 0.0;          // leaderboard: lifetime best VMG to windward (kt), gated RPM<50
float longest_gale_duration_hours_alltime = 0.0;  // leaderboard: lifetime longest continuous gale (sustained TWS>=34kt), hours
// Gale detector runtime state (not persisted): the current continuous gale run
bool galeActive = false;
uint32_t galeStartMs = 0;

// ─────────────────────────────────────────────────────────────────────
// IGNITION-CYCLE WATERMARKS
// Min/max since boot (= since ignition cycled). SEPARATE from the
// *_AllTime / *Max / *_min systems above — those persist across boots
// via NVS; these reset every boot. Initialized to NAN so the first valid
// sample seeds both ends (see wmIgnUpdate).
// 16 pairs × 8 bytes = 128 bytes, internal RAM, plain globals.
// ─────────────────────────────────────────────────────────────────────
struct IgnWatermark { float lo; float hi; };

IgnWatermark wmIgn_amps     = { NAN, NAN };   // MeasuredAmps (A)
IgnWatermark wmIgn_altTempF = { NAN, NAN };   // AlternatorTemperatureF (°F)
IgnWatermark wmIgn_IBV      = { NAN, NAN };   // IBV — INA228 battery V
IgnWatermark wmIgn_Bcur     = { NAN, NAN };   // Bcur — INA228 battery A
IgnWatermark wmIgn_SOC      = { NAN, NAN };   // SOC percent (0..100, float)
IgnWatermark wmIgn_RPM      = { NAN, NAN };   // RPM
IgnWatermark wmIgn_SOG      = { NAN, NAN };   // SOGNMEA (knots)
IgnWatermark wmIgn_AWS      = { NAN, NAN };   // ApparentWindSpeedNMEA (knots)
IgnWatermark wmIgn_TWS      = { NAN, NAN };   // TrueWindSpeedNMEA (knots)
IgnWatermark wmIgn_heel     = { NAN, NAN };   // imu_heel_deg
IgnWatermark wmIgn_pitch    = { NAN, NAN };   // imu_pitch_deg
IgnWatermark wmIgn_vacc     = { NAN, NAN };   // imu_vertical_accel_g
IgnWatermark wmIgn_baro     = { NAN, NAN };   // baroPressure (mbar)
IgnWatermark wmIgn_ambient  = { NAN, NAN };   // ambientTemp (°F)
IgnWatermark wmIgn_VMGman   = { NAN, NAN };   // VMGNMEA — VMG to manual bearing (knots)
IgnWatermark wmIgn_VMGup    = { NAN, NAN };   // VMGUpwind — VMG to windward (knots)

inline void wmIgnUpdate(IgnWatermark &w, float v) {
  if (!isfinite(v)) return;
  if (isnan(w.lo) || v < w.lo) w.lo = v;
  if (isnan(w.hi) || v > w.hi) w.hi = v;
}
inline float wmIgnSafe(float v) { return isnan(v) ? 0.0f : v; }

// Universal data freshness tracking

// timing requirements are defined in the javascript file
// search for STALENESS DISPLAY THRESHOLDS at top of file

// Complete DataIndex enum for all variables displayed in Live Data
// Streamlined DataIndex enum - only tracks real-time sensor data that might go stale if a sensor is disconnected
// Excludes peak/cumulative values that should persist even when source fails
enum DataIndex {
  IDX_HEADING_NMEA = 0,
  IDX_LATITUDE_NMEA,
  IDX_LONGITUDE_NMEA,
  IDX_SATELLITE_COUNT,
  IDX_VICTRON_VOLTAGE,
  IDX_VICTRON_CURRENT,
  IDX_ALTERNATOR_TEMP,
  IDX_THERMISTOR_TEMP,
  IDX_RPM,
  IDX_MEASURED_AMPS,
  IDX_BATTERY_V,                 // 10 - ADS1115 battery voltage
  IDX_IBV,                       // 11 - INA228 battery voltage
  IDX_BCUR,                      // 12 - Battery current from INA228
  IDX_CHANNEL3V,                 // 13 - ADS Ch3 Voltage
  IDX_DUTY_CYCLE,                // 14 - Field duty cycle percentage
  IDX_FIELD_VOLTS,               // 15 - vvout (calculated field voltage)
  IDX_FIELD_AMPS,                // 16 - iiout (calculated field current)
  IDX_COG_NMEA,
  IDX_SOG_NMEA,
  IDX_APPARENT_WIND_SPEED,
  IDX_APPARENT_WIND_ANGLE,
  IDX_TRUE_WIND_SPEED,
  IDX_TRUE_WIND_ANGLE,
  IDX_LEEWAY,
  IDX_VMG,
  IDX_BARO_PRESSURE,
  IDX_AMBIENT_TEMP,
  IDX_SOC_PERCENT,
  IDX_WIFI_STRENGTH,
  IDX_DYNAMIC_ALT_CURRENT_ZERO,
  IDX_CHARGING_MODE,
  IDX_TIME_TO_FULL_CHARGE,
  IDX_TIME_TO_FULL_DISCHARGE,
  IDX_DYNAMIC_SHUNT_GAIN,
  IDX_IMU,                       // 34 - IMU (accel/gyro/derived angles)
  IDX_WATER_DEPTH,               // 35 - NMEA2k WaterDepth (PGN 128267)
  IDX_STW_NMEA,                  // 36 - Speed Through Water / SOW (PGN 128259) for the boat-performance polar
  IDX_VICTRON_SOLAR,             // 37 - Victron VE.Direct solar (PPV/VPV) staleness
  // Keep this last and increment when new added
  MAX_DATA_INDICES = 38
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
unsigned long wifiWakeStart = 0;                  // millis() when wake was triggered; 0 = inactive. Never store millis()+constant — use elapsed-time comparisons to survive 49.7-day wraparound.

AsyncWebServer server(80);                  // Create AsyncWebServer object on port 80
AsyncEventSource events("/events");         // Create an Event Source on /events
unsigned long webgaugesinterval = 100;      // delay in ms between sensor updates on webpage
int plotTimeWindow = 60;                    // Plot time window in seconds
unsigned long healthystuffinterval = 5000;  // check hardware health parameters only every 5 seconds, not that they consume much   THIS IS DEAD CODE, REMOVE LATER

// WiFi provisioning settings persist in NVS as NK_ssid / NK_pass
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

Stream *OutputStream = &Serial;  

//ADS1115 more pre-setup crap
uint32_t adsI2CErrorCount = 0;
uint32_t adsSlowReadCount = 0;      // times ADS_READ_RESULT took >5ms (I2C stall events)
// I2C bus-health instrumentation — separates a true bus stall from loop preemption.
// inaBusReadWorstUs / imuFifoFetchWorstUs time ONLY the Wire transactions; compare them
// to the whole-block ft_rai_ina228 / IMU-drain timers: equal => the bus, much smaller =>
// the loop was preempted mid-read by Core-1 load. All four reset with "Reset Peak Values".
uint32_t inaBusReadWorstUs = 0;    // worst µs spent in the two INA228 Wire reads since reset
uint32_t inaBusSlowCount = 0;      // INA228 bus reads > 15 ms (one Wire-timeout's worth) since reset
uint32_t ina228ErrorCount = 0;     // INA228 reads dropped (sanity fail / exception) — was silent before
uint32_t imuFifoFetchWorstUs = 0;  // worst µs spent in Get_FIFO_Sample since reset
uint16_t imuFifoWorstSamples = 0;  // sample count of THAT worst fetch — 42 bytes is ~1ms at 400kHz, so a
                                   // worst at the 6-sample cap proves the stall is bus/preemption, not transfer size
uint32_t adsLastSlowEndTxUs = 0;    // endTransmission duration on most recent slow read (µs)
uint32_t adsLastSlowReqFromUs = 0;  // requestFrom duration on most recent slow read (µs)
// AdjustFieldLearnMode worst-pass section profiler — post-AsyncTCP-pin residual stalls
// (~16ms) keep landing in this one function; this latches a per-section breakdown of the
// worst FULL pass (early-return passes are not recorded — if the Adjust Field row in
// Function Timing spikes but aflWorstTotalUs doesn't, the spike was in an early-return
// pass, i.e. the pre-gate region). Read via /debug; resets with "Reset Peak Values".
#define AFL_SECTIONS 7
uint32_t aflWorstTotalUs = 0;
uint32_t aflWorstSecUs[AFL_SECTIONS] = { 0 };

enum ADS1115_State {
  ADS_IDLE,
  ADS_WAIT,
  ADS_READ_RESULT
};

ADS1115_State adsState = ADS_IDLE;
uint8_t adsCurrentChannel = 0;  // Driven by adsSeq[] = {1,0,1,2,1,3}; CH1 fires 3× per cycle (~213 Hz / ~4.7ms)
int adsTriggeredChannel = 0;
unsigned long adsStateEntered = 0;
const unsigned long ADS_CONVERSION_MS = 3;  // 1.16ms at 860SPS + millis() granularity margin
const unsigned long ADS_TIMEOUT_MS = 10;    // hardware fault catcher

volatile bool ch1FreshFlag = false;  // Set when CH1 result is ready, consumed by AdjustFieldLearnMode()

uint8_t PidSampleDivisor = 1;  // 1=PID runs every CH1 sample (~213 Hz / ~4.7ms), 2=every other (~107 Hz), etc.
                               // CH1 fires at positions 0, 2, 4 in adsSeq[] {1,0,1,2,1,3};
                               // 6 steps × ~2.35ms/step = ~14ms full cycle → CH1 every ~4.7ms

const uint16_t adsMuxCodes[4] = {
  ADS1115_REG_CONFIG_MUX_SINGLE_0,
  ADS1115_REG_CONFIG_MUX_SINGLE_1,
  ADS1115_REG_CONFIG_MUX_SINGLE_2,
  ADS1115_REG_CONFIG_MUX_SINGLE_3
};

// Forward declarations (for WiFi functions)
String readFile(fs::FS &fs, const char *path);
bool writeFile(fs::FS &fs, const char *path, const char *message);
bool writeFileIfChanged(fs::FS &fs, const char *path, const char *message);
// Versioned PSRAM-blob persistence (shared scaffold — see 2_functions.ino)
uint32_t writePsramBlob(const char *path, uint32_t magic, uint32_t version,
                        uint32_t userWord, const void *base, size_t recordSize,
                        uint32_t capacity, uint32_t startIdx, uint32_t count);
uint32_t readPsramBlob(const char *path, uint32_t magic, uint32_t version,
                       void *destBase, size_t recordSize, uint32_t destCapacity,
                       uint32_t *userWordOut, bool deleteAfter);
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
  "label{display:block;margin-top:8px;color:#333}"
  // Option-grouping boxes: each WiFi mode (its description + its inputs) lives in one bordered box
  ".option{border-radius:8px;padding:14px;margin:16px 0;border:2px solid}"
  ".option-preferred{border-color:#00a19a;background:#f1faf9}"
  ".option-secondary{border-color:#ccc;background:#fafafa}"
  ".opt-badge{display:inline-block;font-size:12px;font-weight:bold;letter-spacing:.5px;text-transform:uppercase;padding:3px 10px;border-radius:12px;margin-bottom:10px}"
  ".badge-preferred{background:#00a19a;color:white}"
  ".badge-secondary{background:#888;color:white}"
  ".opt-title{font-weight:bold;color:#333;display:block;margin-bottom:4px}"
  ".opt-desc{font-size:14px;color:#444;line-height:1.45}"
  ".option .info-box{margin-top:12px}"
  // box-sizing so full-width inputs sit flush inside the padded option boxes
  "input,select{box-sizing:border-box}"
  "</style>"
  "</head><body>"

  "<div class=\"card\">"
  "<h1>WiFi Configuration</h1>"

  "<form action=\"/wifi\" method=\"POST\">"

  // Preferred option: client mode credentials live inside the teal-accented box
  "<div class=\"option option-preferred\">"
  "<span class=\"opt-badge badge-preferred\">Preferred</span>"
  "<span class=\"opt-title\">Connect to your ship's WiFi (Client Mode)</span>"
  "<span class=\"opt-desc\">Enter your ship's network WiFi credentials below. The regulator will run in Client Mode and the user interface will be accessible via your local wifi.  Navigate to alternator.local in any browser, just like you'd go to google.com.</span>"
  "<label>Ship's WiFi Name (SSID):</label>"
  "<input type=\"text\" name=\"ssid\" placeholder=\"Required for client mode\">"
  "<label>Ship's WiFi Password:</label>"
  "<input type=\"password\" name=\"password\" placeholder=\"Required for client mode\">"
  "</div>"

  // Non-preferred option: hotspot/AP credentials live inside the de-emphasized grey box
  "<div class=\"option option-secondary\">"
  "<span class=\"opt-badge badge-secondary\">Non-Preferred</span>"
  "<span class=\"opt-title\">Use the regulator as a Hotspot (Access Point)</span>"
  "<span class=\"opt-desc\">As backup, or for ships without existing WiFi networks, you may use the regualtor as a Hotspot (aka Access Point). The regulator controller will broadcast its own WiFi network which you can connect to from any device (phone, ipad, laptop, etc.).  Mostly the same functionality will exist at alternator.local, but with no internet, you won't be able to use weather mode, get software updates, see Community features, etc.  This mode is less supported.  To enter this mode on a reboot, you must connect pin 12 in RJ3 (the rightmost ethernet connector, Blue wire) to Ground.  Leave it connected to GND forever if you prefer this mode.</span>"
  "<label>New Alt. Reg. Hotspot Name (SSID):</label>"
  "<input type=\"text\" name=\"hotspot_ssid\" placeholder=\"Leave blank for default: ALTERNATOR_WIFI\">"
  "<label>New Alt. Reg. Hotspot Password:</label>"
  "<input type=\"password\" name=\"ap_password\" placeholder=\"Leave blank for default: alternator123\">"
  "<div class=\"info-box\">"
  "***To boot into Hotspot mode, the Hotspot Wire (pin 12 in RJ3, the rightmost ethernet connector, Blue wire) must be connected to Ground during a restart.***"
  "</div>"
  "</div>"

  "<div class=\"info-box\">"
  "To return to this Wifi Configuration page at any time, connect the WifiReset wire (pin 11, RJ3, Orange/White) to Ground during a restart."
  "</div>"

  "<button type=\"submit\">Save Configuration</button>"

  "<div class=\"info-box\">"
  "After saving, this page may become unresponsive or disappear. In any case, wait 20 seconds, then reconnect to your chosen network to access the full alternator interface at this same url (alternator.local).  Or, just use the iOS app."
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
  delay(100);
  Serial.println("\n\n=== SYSTEM STARTUP ===");
  // Allocate buffers from PSRAM
  configPayloadBuffer = (char *)ps_malloc(CONFIG_PAYLOAD_SIZE);
  if (!configPayloadBuffer) Serial.println("FATAL: configPayloadBuffer ps_malloc failed");
  payloadBuffer = (char *)ps_malloc(PAYLOAD_BUFFER_SIZE);
  if (!payloadBuffer) Serial.println("FATAL: payloadBuffer ps_malloc failed");
  tempBuffer = (char *)ps_malloc(PAYLOAD_BUFFER_SIZE);
  if (!tempBuffer) Serial.println("FATAL: tempBuffer ps_malloc failed");
  filenameBuffer = (char *)ps_malloc(FILENAME_BUFFER_SIZE);
  if (!filenameBuffer) Serial.println("FATAL: filenameBuffer ps_malloc failed");
  timestampBuffer = (char *)ps_malloc(TIMESTAMP_BUFFER_SIZE);
  if (!timestampBuffer) Serial.println("FATAL: timestampBuffer ps_malloc failed");
  messageBuffer = (char *)ps_malloc(MESSAGE_BUFFER_SIZE);
  if (!messageBuffer) Serial.println("FATAL: messageBuffer ps_malloc failed");
  consoleQueue = (ConsoleMessage *)ps_malloc(CONSOLE_QUEUE_SIZE * sizeof(ConsoleMessage));
  if (!consoleQueue) Serial.println("FATAL: consoleQueue ps_malloc failed");
  sensorRing = (SensorSnapshot *)ps_malloc(SENSOR_RING_SIZE * sizeof(SensorSnapshot));
  if (!sensorRing) Serial.println("FATAL: sensorRing ps_malloc failed");
  else memset(sensorRing, 0, SENSOR_RING_SIZE * sizeof(SensorSnapshot));
  baroPressureHistory = (uint16_t *)ps_malloc(BARO_HISTORY_SIZE * sizeof(uint16_t));
  if (!baroPressureHistory) Serial.println("FATAL: baroPressureHistory ps_malloc failed");
  else memset(baroPressureHistory, 0, BARO_HISTORY_SIZE * sizeof(uint16_t));
  longTermRing = (LongTermRecord *)ps_malloc(LONGTERM_RING_SIZE * sizeof(LongTermRecord));
  if (!longTermRing) Serial.println("FATAL: longTermRing ps_malloc failed");
  else memset(longTermRing, 0, LONGTERM_RING_SIZE * sizeof(LongTermRecord));
  // Anchorage detection sliding-window ring (5 hr × 60 s = 300 slots × 24 B = ~7.2 KB).
  anchorageRing = (AnchorageSample *)ps_malloc(ANCHORAGE_RING_SIZE * sizeof(AnchorageSample));
  if (!anchorageRing) Serial.println("FATAL: anchorageRing ps_malloc failed");
  else memset(anchorageRing, 0, ANCHORAGE_RING_SIZE * sizeof(AnchorageSample));
  if (consoleQueue) memset(consoleQueue, 0, CONSOLE_QUEUE_SIZE * sizeof(ConsoleMessage));
  taskArray = (TaskStatus_t *)ps_malloc(MAX_TASKS * sizeof(TaskStatus_t));
  if (!taskArray) Serial.println("FATAL: taskArray ps_malloc failed");
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
  // Tuning score log — 50 records × ~48 bytes = ~2.4 KB PSRAM
  tuningLog = (TuningRecord *)ps_malloc(50 * sizeof(TuningRecord));
  if (!tuningLog) Serial.println("FATAL: tuningLog ps_malloc failed");
  else memset(tuningLog, 0, 50 * sizeof(TuningRecord));
  // Live score buckets — 4 windows × 60 × 8 bytes = 1920 bytes PSRAM
  for (int i = 0; i < 4; i++) {
    liveScoreBuckets[i] = (ScoreBucket *)ps_malloc(LIVE_BUCKET_N * sizeof(ScoreBucket));
    if (!liveScoreBuckets[i]) Serial.printf("FATAL: liveScoreBuckets[%d] ps_malloc failed\n", i);
    else memset(liveScoreBuckets[i], 0, LIVE_BUCKET_N * sizeof(ScoreBucket));
  }
  // CV live score buckets — 4 windows × 60 × 8 bytes = 1920 bytes PSRAM
  for (int i = 0; i < 4; i++) {
    cvLiveScoreBuckets[i] = (ScoreBucket *)ps_malloc(LIVE_BUCKET_N * sizeof(ScoreBucket));
    if (!cvLiveScoreBuckets[i]) Serial.printf("FATAL: cvLiveScoreBuckets[%d] ps_malloc failed\n", i);
    else memset(cvLiveScoreBuckets[i], 0, LIVE_BUCKET_N * sizeof(ScoreBucket));
  }
  // CV tuning score log — 50 records × ~120 bytes = ~6 KB PSRAM
  cvTuningLog = (CVTuningRecord *)ps_malloc(50 * sizeof(CVTuningRecord));
  if (!cvTuningLog) Serial.println("FATAL: cvTuningLog ps_malloc failed");
  else memset(cvTuningLog, 0, 50 * sizeof(CVTuningRecord));
  // Thermal tuning score log — 50 records × ~80 bytes = ~4 KB PSRAM
  thermalTuningLog = (ThermalTuningRecord *)ps_malloc(50 * sizeof(ThermalTuningRecord));
  if (!thermalTuningLog) Serial.println("FATAL: thermalTuningLog ps_malloc failed");
  else memset(thermalTuningLog, 0, 50 * sizeof(ThermalTuningRecord));
  // SystemID log — 50 records × ~76 bytes = ~3.8 KB PSRAM
  systemIDLog = (SystemIDRecord *)ps_malloc(50 * sizeof(SystemIDRecord));
  if (!systemIDLog) Serial.println("FATAL: systemIDLog ps_malloc failed");
  else memset(systemIDLog, 0, 50 * sizeof(SystemIDRecord));
  // Thermal live score buckets — 4 windows × 60 × 8 bytes = 1920 bytes PSRAM
  for (int i = 0; i < 4; i++) {
    thermalLiveScoreBuckets[i] = (ScoreBucket *)ps_malloc(LIVE_BUCKET_N * sizeof(ScoreBucket));
    if (!thermalLiveScoreBuckets[i]) Serial.printf("FATAL: thermalLiveScoreBuckets[%d] ps_malloc failed\n", i);
    else memset(thermalLiveScoreBuckets[i], 0, LIVE_BUCKET_N * sizeof(ScoreBucket));
  }
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
  loadAuthToken();         // Loads token (will be empty if just cleared)  // for supabase
  Serial.flush();          // Ensure it's sent before continuing
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
  printPartitionInfo();  
  initializeNVS();
  loadFuelTableFromNVS();

  if (!ensureLittleFS()) {
    Serial.println("CRITICAL: Cannot continue without filesystem");
    Serial.println();
    while (true)
      ;  // halt
  } else {
    initSensorBuffer();
    // Restore PSRAM ring from LittleFS backup if Phase 4 dumped one before
    // last power-down. No-op if no backup file exists.
    restoreSensorRingFromLittleFS();
    restoreLongTermRing();  // durable month-long plot cache → PSRAM
  }
  delay(50);
  checkWebFilesExist();
  sessionStartTime = millis();
  // Reset all per-function timing structs and session loop metrics at boot
  MaximumLoopTime = 0;
  loopTime5sWindow = 0;
  memset(&ft_ReadAnalogInputs, 0, sizeof(FuncTiming));
  memset(&ft_AdjustFieldLearnMode, 0, sizeof(FuncTiming));
  memset(&ft_logDashboardValues, 0, sizeof(FuncTiming));
  memset(&ft_updateSystemHealthStats, 0, sizeof(FuncTiming));
  memset(&ft_checkWiFiConnection, 0, sizeof(FuncTiming));
  memset(&ft_SendWifiData, 0, sizeof(FuncTiming));
  memset(&ft_CheckAlarms, 0, sizeof(FuncTiming));
  memset(&ft_calculateDerivedMetrics, 0, sizeof(FuncTiming));
  memset(&ft_ch1_compute_stats, 0, sizeof(FuncTiming));
  memset(&ft_uploadSensorHistory, 0, sizeof(FuncTiming));
  memset(&ft_dumpLongTermRing, 0, sizeof(FuncTiming));
  memset(&ft_uploadBufferedRecords, 0, sizeof(FuncTiming));
  memset(&ft_buildConfigPayload, 0, sizeof(FuncTiming));
  memset(&ft_UpdateEngineRuntime, 0, sizeof(FuncTiming));
  memset(&ft_UpdateEngineFuel, 0, sizeof(FuncTiming));
  memset(&ft_UpdateBatterySOC, 0, sizeof(FuncTiming));
  memset(&ft_UpdateTravelStatistics, 0, sizeof(FuncTiming));
  memset(&ft_UpdateBoardTempPressureMaximums, 0, sizeof(FuncTiming));
  memset(&ft_handleSocGainReset, 0, sizeof(FuncTiming));
  memset(&ft_handleAltZeroReset, 0, sizeof(FuncTiming));
  memset(&ft_calculateChargeTimes, 0, sizeof(FuncTiming));
  memset(&ft_UpdateSailingMetrics, 0, sizeof(FuncTiming));
  memset(&ft_updateWeatherMode, 0, sizeof(FuncTiming));
  memset(&ft_updateSensorWindow, 0, sizeof(FuncTiming));
  memset(&ft_checkTimeSync, 0, sizeof(FuncTiming));
  memset(&ft_updateAccelMetrics, 0, sizeof(FuncTiming));
  memset(&ft_ReadVEData, 0, sizeof(FuncTiming));
  memset(&ft_altHealth, 0, sizeof(FuncTiming));
  memset(&ft_altFold, 0, sizeof(FuncTiming));
  memset(&ft_boatPerf, 0, sizeof(FuncTiming));
  memset(&ft_rai_total, 0, sizeof(FuncTiming));
  memset(&ft_rai_ina228, 0, sizeof(FuncTiming));
  memset(&ft_rai_ads_state, 0, sizeof(FuncTiming));
  memset(&ft_rai_bmp_state, 0, sizeof(FuncTiming));
  memset(&ft_rai_imu, 0, sizeof(FuncTiming));


  captureResetReason();            // immediately capture the reason for last ESP32 shutdown and store in LittleFS and variable that won't be overwritten until next boot
  ensurePreferredBootPartition();  // Ensure we boot from preferred partition
  loadNVSData();                   // Load persistent variables from NVS- everything from last session is restored
  initNVSCache();                  // Sync change-detection cache with loaded NVS values to prevent false writes
  //Reset some parameters to zero since we are re-starting on a re-boot
  CurrentSessionDuration = 0;
  prevSessionMaxLoopTime = MaxLoopTime;  // snapshot last session's worst before zeroing
  MaxLoopTime = 0;                       // reset for this session (persists to NVS on next save)
  totalPowerCycles++;
  saveNVSDataFull();  // Synchronous write — persists boot-time adjustments before loop() starts
  importLegacySettingsFromLittleFS();  // one-time pre-NVS sweep — MUST run before ANY settings reader (alt/perf loaders below, InitSystemSettings, WiFi creds, password)
  initAlternatorHealth();
  altSettingsLoad();
  initBoatPerformance();
  perfSettingsLoad();
  setCpuFrequencyMhz(240);
  pinMode(4, OUTPUT);     // This pin is used to provide a high signal to Field Enable pin
  digitalWrite(4, LOW);   // Start with field off
  gpio4IsLow = true;      // keep the shadow in sync — field starts off
  pinMode(5, INPUT);      // WiFi wake button
  pinMode(2, OUTPUT);     // This pin is used to provide a field PWM indicator (pin 2 of ESP32 is the LED)
  pinMode(1, INPUT);      // Ignition
  pinMode(21, OUTPUT);    // Alarm/Buzzer output (was 33)
  digitalWrite(21, LOW);  // Start with alarm off
  alarmOutputState = false;
  pinMode(42, INPUT);  // bmsLogic
  // PWM setup (needed for basic operation)
  //ledcAttach(pwmPin, SwitchingFrequency, pwmResolution);

  DefaultHeaders::Instance().addHeader("Access-Control-Allow-Origin", "*");
  // If you have custom headers, also:
  DefaultHeaders::Instance().addHeader("Access-Control-Allow-Headers", "*");

  InitSystemSettings();       // load all settings from NVS (one-time LittleFS import sweep first).  If no keys exist, create them.
  initWeatherModeSettings();  // Add weather mode settings--- otherwise similar to line above (InitSystemSettings)
  loadTuningLog();            // restore last session's tuning records from LittleFS
  loadCVTuningLog();          // restore CV tuning records from LittleFS
  loadThermalTuningLog();     // restore thermal step test records from LittleFS
  loadSystemIDLog();          // restore plant-delay (SystemID) records from LittleFS
  loadPasswordHash();
  // Check if we should wake WiFi for a pending OTA update
  nvs_handle_t wake_handle;
  if (nvs_open("update_req", NVS_READONLY, &wake_handle) == ESP_OK) {
    uint8_t wakeFlag = 0;
    if (nvs_get_u8(wake_handle, "wake_flag", &wakeFlag) == ESP_OK && wakeFlag == 1) {
      wifiWakeStart = millis();
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
  httpsQueue = xQueueCreate(2, sizeof(HttpsRequest));  // tiny messages now (payload is a PSRAM pointer, not an inline 6 KB buffer)
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
  delay(50);  // Brief settling time
  if (hardwarePresent == 1) {
    ReadAnalogInputs();
    delay(50);           // Give it a moment to process
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
  tuningScore_init();
  thermalScore_init();
  Serial.println("=== SETUP COMPLETE ===");
}

void loop() {

  esp_task_wdt_reset();

  // Deferred reboot from /get?RebootRegulator — wait 2s so the HTTP 200 reaches the caller
  if (rebootRequested && millis() - rebootRequestedAt > 2000) {
    Serial.println("=== REMOTE REBOOT (/get?RebootRegulator) ===");
    ESP.restart();
  }
  Ignition = !digitalRead(1);  // ! is for optocoupler
  if (IgnitionOverride == 1) {
    Ignition = 1;  // force ON  (bench testing, normal default)
  } else if (IgnitionOverride == 0) {
    Ignition = 0;  // force OFF (test shutdown sequence — change from 1 to 0 to trigger)
  }
 
  WiFiWakeButton = !digitalRead(5);  // Read WiFi wake button state. Active LOW (button pulls to ground)

  // Button press extends/starts timeout
  if (WiFiWakeButton == 1) {
    wifiWakeStart = millis();  // Reset 5-min window from now
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

        // Both sends must succeed before we mark otaCheckDone — otherwise a queue-full
        // at boot would skip both retries permanently. httpsQueue is only depth-2.
        bool bothSent = true;

        debugStackBeforeHTTPS("updateFirmwareVersionInSupabase");
        if (WiFi.RSSI() >= -76) {
          HttpsRequest req = { .type = HTTPS_UPDATE_FW_VERSION };
          if (xQueueSend(httpsQueue, &req, 0) != pdTRUE) {
            Serial.println("OTA: HTTPS queue full — will retry FW version push next loop");
            bothSent = false;
          }
        }

        debugStackBeforeHTTPS("checkForForcedUpdate");
        if (WiFi.RSSI() >= -76) {
          HttpsRequest req = { .type = HTTPS_CHECK_FORCED_UPDATE };
          if (xQueueSend(httpsQueue, &req, 0) != pdTRUE) {
            Serial.println("OTA: HTTPS queue full — will retry forced-update check next loop");
            bothSent = false;
          }
        }

        if (bothSent) otaCheckDone = true;
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
  // Latched at the top of the pass so a pass that drops the field mid-way still counts as
  // field-on (it ran control code). Consumed by the field-ON loop worst at the bottom of loop().
  bool passFieldOn = !gpio4IsLow;
  currentTime = millis();

  // SOC and runtime update every 2 seconds (runs regardless of hardwarePresent)
  if (currentTime - lastSOCUpdateTime >= SOCUpdateInterval) {
    CurrentSessionDuration = (millis() - sessionStartTime) / 1000;  // seconds
    elapsedMillis = currentTime - lastSOCUpdateTime;
    lastSOCUpdateTime = currentTime;
    TIMED_CALL(ft_UpdateEngineRuntime, UpdateEngineRuntime(elapsedMillis));
    TIMED_CALL(ft_UpdateEngineFuel, UpdateEngineFuel(elapsedMillis));  //
    TIMED_CALL(ft_UpdateBatterySOC, UpdateBatterySOC(elapsedMillis));
    TIMED_CALL(ft_UpdateTravelStatistics, UpdateTravelStatistics(elapsedMillis));       //
    TIMED_CALL(ft_UpdateBoardTempPressureMaximums, UpdateBoardTempPressureMaximums());  // NEW
    TIMED_CALL(ft_handleSocGainReset, handleSocGainReset());                            // do the dynamic updates
    TIMED_CALL(ft_handleAltZeroReset, handleAltZeroReset());                            // do the dynamic udpates

    // Barometric pressure history sampler — 5-min cadence into baroPressureHistory ring.
    // Skipped if BMP388 hasn't reported (NAN). Wall-clock epoch stamped only if timeIsSynced
    // (NTP or GPS); on cold boot before sync, last-sample epoch stays at its loaded NVS value
    // so the JS can detect a power-off gap and grey out the affected slots.
    if (hardwarePresent == 1 && baroPressureHistory && (currentTime - lastBaroSampleMs >= BARO_SAMPLE_INTERVAL_MS)) {   // sim mode: no fake baro history
      lastBaroSampleMs = currentTime;
      if (!isnan(baroPressure)) {
        baroPressureHistory[baroHistoryHead] = (uint16_t)(baroPressure * 10.0f + 0.5f);
        baroHistoryHead = (baroHistoryHead + 1) % BARO_HISTORY_SIZE;
        if (timeIsSynced) baroHistoryLastEpoch = timeBase + (currentTime - timeBaseMillis) / 1000;
      }
    }

    // Long-term plot ring → flash while field-off (the doc's "field-off edge"; shutdown
    // does the final dump). Field-off only — never write while the control loop is live.
    // Dumps on the field-off-settled rising edge AND periodically thereafter (so a long
    // engine-off stretch keeps persisting new records, not just the first one). The
    // prev_longTermHead guard skips a dump when nothing changed since the last one.
    static bool prevLongTermFieldOff = false;
    static unsigned long lastLongTermDumpMs = 0;
    bool ltFieldOff = fieldOffSettled(0);
    bool ltRisingEdge = (ltFieldOff && !prevLongTermFieldOff);
    bool ltPeriodic   = (ltFieldOff && (millis() - lastLongTermDumpMs >= LONGTERM_DUMP_INTERVAL_MS));
    if ((ltRisingEdge || ltPeriodic) && prev_longTermHead != longTermHead) {
      TIMED_CALL(ft_dumpLongTermRing, dumpLongTermRing());  // untimed before — this LittleFS flush is the periodic ~250ms loop spike
      lastLongTermDumpMs = millis();
    }
    prevLongTermFieldOff = ltFieldOff;
  }
  TIMED_CALL(ft_calculateChargeTimes, calculateChargeTimes());  // might want to put this in the above if statement and unthrottle at some point update later
  // Periodic NVS save removed: nvs_commit() can block Core 1 for hundreds of ms during
  // flash sector erase, which collides with the voltage loop and risks OV on transients.
  // NVS now persists only at the field-off edge (saveNVSDataFull() further down in loop)
  // and at the shutdown sequence — both run with field off, so any commit duration is safe.
  // Deferred saves from Core 0 button handlers — executed here on Core 1 so Core 0 SSE is not blocked.
  // Fired immediately, no electrical-zone gate: every flag here is set only by a manual UI action
  // (reset/clear/save buttons, or a user-requested SystemID run), so the brief flash write can only
  // happen as a direct result of a user click — never autonomously. All flags execute in the same tick.
  if (pendingSaveCVTuningLog) {
    pendingSaveCVTuningLog = false;
    saveCVTuningLog();
  }
  if (pendingSaveTuningLog) {
    pendingSaveTuningLog = false;
    saveTuningLog();
  }
  if (pendingSaveThermalTuningLog) {
    pendingSaveThermalTuningLog = false;
    saveThermalTuningLog();
  }
  if (pendingSaveSystemIDLog) {
    pendingSaveSystemIDLog = false;
    saveSystemIDLog();
  }
  if (pendingResetAlternatorHealth) {
    pendingResetAlternatorHealth = false;
    resetAlternatorHealth();
  }
  if (pendingResetBoatPerformance) {
    pendingResetBoatPerformance = false;
    resetBoatPerformance();
  }
  if (pendingClearOverheatHistory) {
    pendingClearOverheatHistory = false;
    clearOverheatHistoryAction();
  }
  if (pendingSaveUserTableEdits) {
    pendingSaveUserTableEdits = false;
    saveUserTableEdits();
  }
  if (pendingSaveVesselInfo) {
    pendingSaveVesselInfo = false;
    saveVesselInfoToFile();
  }
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
      // Edge detection FIRST — must set pendingShutdownFlush before the state machine checks it
      if (lastIgnitionState == 1) {
        pendingShutdownFlush = true;
        Serial.println("Ignition OFF");
        lastIgnitionState = 0;
      }
      wifiWakeActive = (wifiWakeStart > 0 && (millis() - wifiWakeStart) < WIFI_WAKE_DURATION);
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
        if ((WIFI_WAKE_DURATION - (millis() - wifiWakeStart)) < 20000 && !wakeExpiryWarningShown) {
          queueConsoleMessage("WiFi wake mode expiring in 20 seconds - press button to extend");
          wakeExpiryWarningShown = true;
        }
      } else {
        if (pendingShutdownFlush) {
          if (!gpio4IsLow) {
            // Phase 1: field still ramping — hold 240MHz and WiFi until it cuts
            setCpuFrequencyMhz(240);
          } else if (!shutdownNVSFlushDone) {
            // Phase 2: field just cut — flush NVS and save partial sensor window immediately
            setCpuFrequencyMhz(240);
            saveNVSDataFull();  // absolute latest values, no critical-zone gate
            altHealthSave();
            boatPerfSave();
            uploadSensorHistory();  // save whatever is in the current window to local buffer
            if (pendingSaveCVTuningLog) {
              pendingSaveCVTuningLog = false;
              saveCVTuningLog();
            }
            if (pendingSaveTuningLog) {
              pendingSaveTuningLog = false;
              saveTuningLog();
            }
            if (pendingSaveThermalTuningLog) {
              pendingSaveThermalTuningLog = false;
              saveThermalTuningLog();
            }
            if (pendingSaveSystemIDLog) {
              pendingSaveSystemIDLog = false;
              saveSystemIDLog();
            }
            if (pendingResetAlternatorHealth) {
              pendingResetAlternatorHealth = false;
              resetAlternatorHealth();
            }
            if (pendingResetBoatPerformance) {
              pendingResetBoatPerformance = false;
              resetBoatPerformance();
            }
            if (pendingClearOverheatHistory) {
              pendingClearOverheatHistory = false;
              clearOverheatHistoryAction();
            }
            if (pendingSaveUserTableEdits) {
              pendingSaveUserTableEdits = false;
              saveUserTableEdits();
            }
            if (pendingSaveVesselInfo) {
              pendingSaveVesselInfo = false;
              saveVesselInfoToFile();
            }
            shutdownNVSFlushDone = true;
            shutdownCloudDeadlineMs = millis() + 1800000;  // 30-min window: fieldOffSettled() gates fire at 60-75s after field off; 30 min gives full time for NTP, uploads, weather, and buffer drain
          } else if (millis() < shutdownCloudDeadlineMs) {
            // Phase 3: hold 240MHz and WiFi for the full 30-min window unconditionally.
            // Lets the CloudFeatures block continue uploading, and keeps WiFi open to verify the drain.
            setCpuFrequencyMhz(240);
          } else {
            // Phase 4: 30-min cloud drain window expired. If the PSRAM ring
            // still has unsent records, bulk-dump them to LittleFS so they
            // survive the imminent power-down. setup() will restore them on
            // next boot. Acceptable to block on flash here — device is about
            // to sleep anyway.
            if (!ringIsEmpty()) {
              Serial.printf("Shutdown Phase 4: ring still has %u records — dumping to LittleFS\n",
                            (unsigned)sensorRingCount);
              dumpSensorRingToLittleFS();
            }
            TIMED_CALL(ft_dumpLongTermRing, dumpLongTermRing());  // durable month-long plot cache → flash (final)
            pendingShutdownFlush = false;
            shutdownNVSFlushDone = false;
            shutdownCloudDeadlineMs = 0;
          }
        } else if (!core0Busy && uxQueueMessagesWaiting(httpsQueue) == 0) {
          // Queue drained — go low power
          queueDrainHoldStart = 0;
          WiFi.mode(WIFI_OFF);  // THIS MUST BE DONE FIRST
          if (tempTaskHandle != NULL) {
            vTaskSuspend(tempTaskHandle);  // SUSPEND BACKGROUND TASKS BEFORE SLOWING CPU
            tempTaskSuspended = true;      // intentional suspend — health monitor must not read this as a hang
          }
          setCpuFrequencyMhz(80);  // THIS MUST BE DONE SECOND
        } else {
          // Upload in flight — hold at 240MHz/WiFi-on, but cap the wait to 5 minutes
          // so a stuck upload (bad WiFi, server error, malformed payload) can't hold
          // the device at high power indefinitely
          if (queueDrainHoldStart == 0) queueDrainHoldStart = millis();
          if (millis() - queueDrainHoldStart > 300000UL) {
            queueDrainHoldStart = 0;
            queueConsoleMessage("Upload drain timeout - forcing low power");
            WiFi.mode(WIFI_OFF);  // THIS MUST BE DONE FIRST
            if (tempTaskHandle != NULL) {
              vTaskSuspend(tempTaskHandle);  // SUSPEND BACKGROUND TASKS BEFORE SLOWING CPU
              tempTaskSuspended = true;      // intentional suspend — health monitor must not read this as a hang
            }
            setCpuFrequencyMhz(80);  // THIS MUST BE DONE SECOND
          }
          // else: within 5-min window — retry next tick
        }
      }

    } else {
      // ===== IGNITION ON - Normal operation =====
      // Detect ignition state change to ON
      if (lastIgnitionState != 1) {
        pendingShutdownFlush = false;  // ignition back on — cancel any pending flush
        shutdownNVSFlushDone = false;
        shutdownCloudDeadlineMs = 0;
        queueDrainHoldStart = 0;
        Serial.println("Ignition ON - Normal operation mode");
        lastIgnitionState = 1;
      }
      wifiWakeStart = 0;               // Clear wake mode
      lastWifiWakeActive = false;      // Reset wake mode tracking
      wakeExpiryWarningShown = false;  // Reset warning
      setCpuFrequencyMhz(240);         // Ensure full speed
      if (tempTaskHandle != NULL) {
        vTaskResume(tempTaskHandle);  // RESUME BACKGROUND TASKS AFTER SPEEDING UP CPU
        tempTaskSuspended = false;
        lastTempTaskHeartbeat = millis();  // restart the 20s window so the health check doesn't trip before the task posts its first beat
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
        delay(1);                 // yield to asyncTCP callbacks — I2C wait states do this naturally in real mode
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
      // Inner-PID firing-interval re-baseline: while the field is down, drop the previous-
      // firing timestamp so the first firing after re-enable measures a fresh interval,
      // not the whole field-off gap. gpio4IsLow reflects field state set inside the call above.
      if (gpio4IsLow) pfHasPrev = false;
      TIMED_CALL(ft_altHealth, altHealth_tick(millis()));
      TIMED_CALL(ft_boatPerf, boatPerf_tick(millis()));   // Phase 3 boat performance (timing exposed via PerfLive registry)
      if (hardwarePresent == 1) drainIMUFifo();  // skip in fake mode — no hardware means 15ms I2C timeout per call floods the loop

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
      TIMED_CALL(ft_logDashboardValues, logDashboardValues());            //  nice to have some history in the Console
      TIMED_CALL(ft_updateSystemHealthStats, updateSystemHealthStats());  // samples CPU load + heap stats into globals for CSVData2
      resolveSources();                                                   // GPS/time source arbitration: promote phone fallback, demote stale labels
      TIMED_CALL(ft_updateSensorWindow, updateSensorWindow());            // Update sensor aggregation (after sensor reads)
      TIMED_CALL(ft_updateAccelMetrics, updateAccelMetrics());            // accel always on; toggle/transition flush removed

      // ===== FIELD-OFF NVS DRAIN (5s settled, once per field-off window) =====
      // Independent of fieldOffSettled() — that helper has a 60s baseline intended for
      // cloud/network callers. This drain wants to lock telemetry to flash quickly after
      // the field cuts (engine pause, idle, ignition cut) without waiting for the 2-min
      // schedule. Skips when pendingShutdownFlush owns the ignition-off sequence.
      {
        bool fieldNowActive = (fieldActiveStatus > 0);
        if (lastFieldStateForFlush == 1 && !fieldNowActive) {
          fieldOffEdgeMs = millis();  // capture the field-on -> field-off edge
        } else if (lastFieldStateForFlush == 0 && fieldNowActive) {
          fieldOffFlushDone = false;        // re-arm on field-on edge
          fieldOffMatrixFlushDone = false;
        } else if (lastFieldStateForFlush == -1 && !fieldNowActive) {
          fieldOffEdgeMs = millis();  // first observation; field already off
        }
        lastFieldStateForFlush = fieldNowActive ? 1 : 0;

        // Gate 1 (+5 s): drain storage namespace to NVS
        if (!fieldOffFlushDone && !pendingShutdownFlush && !fieldNowActive
            && fieldOffEdgeMs > 0 && (millis() - fieldOffEdgeMs) >= 5000UL) {
          saveNVSDataFull();
          fieldOffFlushDone = true;
        }
        // Gate 2 (+13 s): write performance matrix to LittleFS — staggered so
        // the ~150 ms file write doesn't land on the NVS commit's relocation tail
        if (!fieldOffMatrixFlushDone && !pendingShutdownFlush && !fieldNowActive
            && fieldOffEdgeMs > 0 && (millis() - fieldOffEdgeMs) >= 13000UL) {
          altHealthSave();
          boatPerfSave();
          fieldOffMatrixFlushDone = true;
        }
      }

      // Local long-term accumulation — runs regardless of CloudFeatures so the 30-day
      // local ring (and its Long Term plots) fill even in AP mode / no-WiFi. Only the
      // cloud uploads below are gated. uploadSensorHistory() self-skips during OTA; the
      // window accumulates while the field is on and is saved at any time, no internet.
      if (!otaInProgress) {
        unsigned long nowLocalAccum = millis();
        if (nowLocalAccum - lastSensorUploadTime >= SENSOR_UPLOAD_INTERVAL) {
          esp_task_wdt_reset();
          TIMED_CALL(ft_UpdateSailingMetrics, UpdateSailingMetrics(SENSOR_UPLOAD_INTERVAL));
          lastSensorUploadTime = nowLocalAccum;
          TIMED_CALL(ft_uploadSensorHistory, uploadSensorHistory());
          esp_task_wdt_reset();
        }
      }

      if (CloudFeatures == 1) {
        if (otaInProgress) {
          return;  // Skip during OTA
        }
        unsigned long currentMillisz = millis();
        // Check time sync every 12 hours — requires field off for 60s (fieldOffSettled)
        if (fieldOffSettled(0)) TIMED_CALL(ft_checkTimeSync, checkTimeSync());

        // Upload buffered records every BUFFER_UPLOAD_INTERVAL — requires field off for 70s.
        // Bumped from 65s to 70s so the buffered upload's TLS handshake doesn't land on top
        // of the field-off NVS drain (fires at 5s). 60s baseline + 10s extra = 70s total.
        // The "Upload Cloud Now" dashboard button sets forceCloudFlushPending = true which
        // bypasses BOTH the field-off settle AND the 13s interval throttle so records drain
        // back-to-back (still rate-limited by the HTTPS queue depth on Core 0).
        bool flushBypass = forceCloudFlushPending;
        if ((fieldOffSettled(10000) || flushBypass)
            && (flushBypass || currentMillisz - lastBufferUploadAttempt >= BUFFER_UPLOAD_INTERVAL - 7)) {
          lastBufferUploadAttempt = currentMillisz;
          if (bufferedRecordCount > 0) {
            esp_task_wdt_reset();                                           // Feed before upload
            TIMED_CALL(ft_uploadBufferedRecords, uploadBufferedRecords());  // times JSON build + queue send; HTTP transfer is on core 0 and not captured here
            esp_task_wdt_reset();                                           // Feed after upload completes
          } else if (flushBypass) {
            // Ring empty — force-flush goal met, clear the flag.
            forceCloudFlushPending = false;
            queueConsoleMessage("Cloud sync: forced flush complete (queue empty)");
          }
        }
        
        // Configuration Snapshot — requires field off for 70s. Sim mode (HardwarePresent=0):
        // skip — never push fake stats/state/leaderboard data to the cloud.
        if (hardwarePresent == 1 && fieldOffSettled(10000) && millis() - lastConfigSnapshotTime >= CONFIG_SNAPSHOT_INTERVAL) {
          lastConfigSnapshotTime = millis();
          if (currentMode == MODE_CLIENT && WiFi.status() == WL_CONNECTED && isRegistered) {
            if (WiFi.RSSI() >= -76) {
              bool _configBuilt;
              TIMED_CALL(ft_buildConfigPayload, _configBuilt = buildConfigPayload());  // time the payload build separately from the queue send
              if (_configBuilt) {
                HttpsRequest req = {};
                req.type = HTTPS_UPLOAD_CONFIG;
                req.payloadCap = CONFIG_PAYLOAD_SIZE;            // full config (was silently truncated to 6 KB)
                req.payload = (char *)ps_malloc(req.payloadCap);
                if (req.payload) {
                  strncpy(req.payload, configPayloadBuffer, req.payloadCap - 1);
                  req.payload[req.payloadCap - 1] = '\0';
                  if (xQueueSend(httpsQueue, &req, 0) != pdTRUE) {
                    free(req.payload);
                    queueConsoleMessage("Config snapshot: HTTPS queue full, will retry next interval");
                  }
                }
              }
            }
          }
        }

        // Boat-performance aggregates → cloud (field-off gated; sim never uploads).
        if (hardwarePresent == 1 && fieldOffSettled(10000) && millis() - lastBoatPerfUploadTime >= BOATPERF_UPLOAD_INTERVAL) {
          lastBoatPerfUploadTime = millis();
          if (currentMode == MODE_CLIENT && WiFi.status() == WL_CONNECTED && isRegistered && WiFi.RSSI() >= -76) {
            HttpsRequest req = {};
            req.type = HTTPS_UPLOAD_BOATPERF;
            req.payloadCap = PERF_UPLOAD_BUF_SIZE;
            req.payload = (char *)ps_malloc(req.payloadCap);
            if (req.payload && buildBoatPerfPayload(req.payload, req.payloadCap)) {
              if (xQueueSend(httpsQueue, &req, 0) != pdTRUE) {
                free(req.payload);
                queueConsoleMessage("BoatPerf upload: HTTPS queue full, will retry next interval");
              }
            } else if (req.payload) {
              free(req.payload);   // nothing to send (build returned false) → release the buffer
            }
          }
        }

        // Alternator-health best-ever records → cloud (field-off gated; sim never uploads).
        // Alt-health LEARNS with the field ON, but uploads (like all flash/HTTPS) are field-OFF:
        // records bank during charging and flush at the next field-off. buildAltHealthPayload
        // is dirty-gated, so this is a no-op when nothing new has been banked.
        if (hardwarePresent == 1 && fieldOffSettled(10000) && millis() - lastAltHealthUploadTime >= ALTHEALTH_UPLOAD_INTERVAL) {
          lastAltHealthUploadTime = millis();
          if (currentMode == MODE_CLIENT && WiFi.status() == WL_CONNECTED && isRegistered && WiFi.RSSI() >= -76) {
            HttpsRequest req = {};
            req.type = HTTPS_UPLOAD_ALTHEALTH;
            req.payloadCap = ALT_UPLOAD_BUF_SIZE;
            req.payload = (char *)ps_malloc(req.payloadCap);
            if (req.payload && buildAltHealthPayload(req.payload, req.payloadCap)) {
              if (xQueueSend(httpsQueue, &req, 0) != pdTRUE) {
                free(req.payload);
                queueConsoleMessage("AltHealth upload: HTTPS queue full, will retry next interval");
              }
            } else if (req.payload) {
              free(req.payload);   // nothing to send (build returned false) → release the buffer
            }
          }
        }
      }
      TIMED_CALL(ft_ch1_compute_stats, ch1_compute_stats());
      TIMED_CALL(ft_SendWifiData, SendWifiData());  // Safely sends data (has internal guard to check if WiFi is actually on)

      // Client-specific connection monitoring
      // if (currentMode == MODE_CLIENT) {  // moved the gating check into the checkwificonnection function WAS NOT SUFFICIENT FOR WHATEVER REASON
      if (currentMode == MODE_CLIENT && (Ignition == 1 || (wifiWakeStart > 0 && (millis() - wifiWakeStart) < WIFI_WAKE_DURATION))) {
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
    ft_AdjustFieldLearnMode.worstWindow = 0;
    ft_logDashboardValues.worstWindow = 0;
    ft_updateSystemHealthStats.worstWindow = 0;
    ft_checkWiFiConnection.worstWindow = 0;
    ft_SendWifiData.worstWindow = 0;
    ft_CheckAlarms.worstWindow = 0;
    ft_calculateDerivedMetrics.worstWindow = 0;
    ft_ch1_compute_stats.worstWindow = 0;
    ft_uploadSensorHistory.worstWindow = 0;
    ft_dumpLongTermRing.worstWindow = 0;
    ft_uploadBufferedRecords.worstWindow = 0;
    ft_buildConfigPayload.worstWindow = 0;
    ft_UpdateEngineRuntime.worstWindow = 0;
    ft_UpdateEngineFuel.worstWindow = 0;
    ft_UpdateBatterySOC.worstWindow = 0;
    ft_UpdateTravelStatistics.worstWindow = 0;
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
    ft_updateAccelMetrics.worstWindow = 0;
    ft_ReadVEData.worstWindow = 0;
    ft_altHealth.worstWindow = 0;
    ft_altFold.worstWindow = 0;
    ft_boatPerf.worstWindow = 0;
    loopWorst80Win = 0;  // 80MHz low-power loop worst — rolling 5s window
    loopFieldOnWin = 0;  // field-ON loop worst — rolling 5s window

    prev_millis7888 = millis();
  }
  endtime = esp_timer_get_time();  //Record end of Loop
  LoopTime = (endtime - starttime);
  // 80MHz low-power loop health (engine-off only). One cheap freq read gates it;
  // tracks worst pass + counts passes over the accel FIFO drain limit. See LOOP80_* globals.
  if (getCpuFrequencyMhz() < 81) {
    loop80IterCount++;
    if ((uint32_t)LoopTime > loopWorst80Win) loopWorst80Win = (uint32_t)LoopTime;
    if ((uint32_t)LoopTime > loopWorst80Ses) loopWorst80Ses = (uint32_t)LoopTime;
    if (LoopTime > LOOP80_IMU_LIMIT_US) loopOver80ImuLimitCount++;
  }
  // Field-ON loop health: same pass timer, gated to passes that started with the field gate
  // open. Splits real control-path stalls (CV-tick / overvoltage risk) from intentional
  // field-off background work (flash flushes, uploads) that dominates the ungated worst.
  if (passFieldOn) {
    if ((uint32_t)LoopTime > loopFieldOnWin) loopFieldOnWin = (uint32_t)LoopTime;
    if ((uint32_t)LoopTime > loopFieldOnSes) loopFieldOnSes = (uint32_t)LoopTime;
  }
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
  static unsigned long lastTempDebugPrint = 0;
  if (millis() - lastTempDebugPrint >= 300000) {  //5 minutes
    lastTempDebugPrint = millis();
    printTempDebugStatus();
  }

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

  taskYIELD();  //consider removing
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