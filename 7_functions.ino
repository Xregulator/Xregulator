
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

// ============================================================
// EFFICIENCY MATRIX TRACKER
//
//   3D matrix: RPM bucket × Temp bucket × Field-drive bucket
//   Each cell stores time-weighted avg + min/max (everlasting)
//   and a frozen reference snapshot (once criteria are met).
//   Red dot = live operating point.
//   Anomaly detection: new SS points checked against reference.
// ============================================================


// ============================================================
// BUCKET LOOKUP
// ============================================================

static int getRPMBucket(float rpm) {
  if (isnan(rpm) || rpm < 0.0f) return -1;
  for (int i = 0; i < NUM_RPM_BUCKETS; i++) {
    if (rpm >= RPM_BOUNDS[i] && rpm < RPM_BOUNDS[i + 1]) return i;
  }
  return -1;
}

static int getTempBucket(float tempF) {
  if (IgnoreTemperature) return 1;
  if (isnan(tempF)) return -1;
  for (int i = 0; i < NUM_TEMP_BUCKETS; i++) {
    if (tempF >= TEMP_BOUNDS[i] && tempF < TEMP_BOUNDS[i + 1]) return i;
  }
  return -1;
}

static int getFieldBucket(float fieldVolts) {
  if (isnan(fieldVolts) || fieldVolts < 0.0f) return -1;
  for (int i = 0; i < NUM_FIELD_BUCKETS; i++) {
    if (fieldVolts >= FIELD_BOUNDS[i] && fieldVolts < FIELD_BOUNDS[i + 1]) return i;
  }
  // Clamp: at or above max goes in last bucket
  if (fieldVolts >= FIELD_BOUNDS[NUM_FIELD_BUCKETS - 1]) return NUM_FIELD_BUCKETS - 1;
  return -1;
}


// ============================================================
// STEADY STATE DETECTION     POSSIBLY DELETE THIS LATER , IS IT USED??
// Call at 1Hz. Returns true after REQUIRED consecutive stable passes.
// PLACEHOLDER: all tolerances and REQUIRED count — tune for your alternator.
// ============================================================

// static bool checkSteadyState() {
//   static float prevRPM = 0.0f;
//   static float prevDuty = 0.0f;
//   static float prevBattV = 0.0f;
//   static float prevAmps = 0.0f;
//   static float prevTempF = 0.0f;
//   static int passes = 0;

//   const float RPM_TOL = 75.0f;   // PLACEHOLDER
//   const float DUTY_TOL = 2.0f;   // PLACEHOLDER
//   const float BATTV_TOL = 0.1f;  // PLACEHOLDER
//   const float AMPS_TOL = 2.0f;   // PLACEHOLDER
//   const float TEMP_TOL = 2.0f;   // PLACEHOLDER
//   const int REQUIRED = 3;        // PLACEHOLDER

//   float battV = getBatteryVoltage();
//   if (isnan(battV)) {
//     passes = 0;
//     return false;
//   }

//   float amps = isnan(MeasuredAmps) ? 0.0f : MeasuredAmps;
//   float tempF = (IgnoreTemperature || isnan(TempToUse)) ? prevTempF : TempToUse;

//   bool stable = (fabsf(RPM - prevRPM) < RPM_TOL && fabsf(dutyCycle - prevDuty) < DUTY_TOL && fabsf(battV - prevBattV) < BATTV_TOL && fabsf(amps - prevAmps) < AMPS_TOL && (IgnoreTemperature || fabsf(tempF - prevTempF) < TEMP_TOL));

//   prevRPM = RPM;
//   prevDuty = dutyCycle;
//   prevBattV = battV;
//   prevAmps = amps;
//   prevTempF = tempF;

//   if (stable) passes++;
//   else passes = 0;

//   return (passes >= REQUIRED);
// }
// ============================================================
// UNIFIED POINT VALIDITY CHECK
//
// Single gate used for BOTH matrix accumulation AND anomaly
// scoring. Deliberately loose — a degraded alternator must
// still qualify, otherwise failures are invisible.
// No consecutive-pass requirement. Single-sample RPM delta only.
// ============================================================
static bool checkPointValid(float amps, float battV, float &fieldVolts,
                            int &rBucket, int &tBucket, int &fBucket) {
  static float prevRPM = 0.0f;
  float rpmDelta = fabsf(RPM - prevRPM);
  prevRPM = RPM;

  if (dutyCycle < 5.0f) return false;
  if (amps < 2.0f) return false;
  if (isnan(battV)) return false;
  if (battV < EFF_MIN_BATT_V) return false;
  if (rpmDelta > 200.0f) return false;  // PLACEHOLDER: tune if too restrictive

  rBucket = getRPMBucket(RPM);
  tBucket = getTempBucket(TempToUse);
  fieldVolts = dutyCycle * battV / 100.0f;
  fBucket = getFieldBucket(fieldVolts);

  if (rBucket < 0 || tBucket < 0 || fBucket < 0) return false;

  return true;
}


// ============================================================
// NVS — SAVE
// Namespace "effmat" (distinct from "storage" and old "effgrid").
// One blob per RPM row: "mat_r0".."mat_r7".
// Each blob = NUM_TEMP_BUCKETS * NUM_FIELD_BUCKETS * sizeof(MatrixCell).
// referenceFinalized flag stored as "mat_final".
// ============================================================

static void saveEfficiencyMatrix() {
  if (!effMatrix) return;

  nvs_handle_t handle;
  if (nvs_open("effmat", NVS_READWRITE, &handle) != ESP_OK) {
    queueConsoleMessage("EffMatrix: NVS open failed (save)");
    return;
  }

  const size_t rowSize = (size_t)NUM_TEMP_BUCKETS * NUM_FIELD_BUCKETS * sizeof(MatrixCell);

  for (int r = 0; r < NUM_RPM_BUCKETS; r++) {
    char key[12];
    snprintf(key, sizeof(key), "mat_r%d", r);
    nvs_set_blob(handle, key, &MATRIX_CELL(r, 0, 0), rowSize);
  }

  nvs_set_u8(handle, "mat_final", (uint8_t)referenceFinalized);

  nvs_commit(handle);
  nvs_close(handle);
}


// ============================================================
// NVS — LOAD
// Size mismatch on any row resets that row only (same strategy as old system).
// ============================================================

static void loadEfficiencyMatrix() {
  if (!effMatrix) return;

  nvs_handle_t handle;
  if (nvs_open("effmat", NVS_READONLY, &handle) != ESP_OK) {
    return;  // No persisted data yet — matrix stays zeroed
  }

  const size_t rowSize = (size_t)NUM_TEMP_BUCKETS * NUM_FIELD_BUCKETS * sizeof(MatrixCell);

  for (int r = 0; r < NUM_RPM_BUCKETS; r++) {
    char key[12];
    snprintf(key, sizeof(key), "mat_r%d", r);

    size_t sz = rowSize;
    esp_err_t err = nvs_get_blob(handle, key, &MATRIX_CELL(r, 0, 0), &sz);

    if (err != ESP_OK || sz != rowSize) {
      memset(&MATRIX_CELL(r, 0, 0), 0, rowSize);
      queueConsoleMessageF(
        "EffMatrix: RPM row %d size mismatch (got %u, want %u) — row reset",
        r, (unsigned)sz, (unsigned)rowSize);
    }
  }

  uint8_t finalized = 0;
  if (nvs_get_u8(handle, "mat_final", &finalized) == ESP_OK) {
    referenceFinalized = (finalized != 0);
  }

  nvs_close(handle);
  loadEffHistory();  // Commit previous session and load history buffer
}


// ============================================================
// REFERENCE BIN SELECTION — greedy spread-first
//
// Selects NUM_REFERENCE_BINS from eligible candidates to
// maximize coverage across temp/field/RPM space.
// Seeds with highest SS-time bin, then greedily adds the
// candidate that most expands spread, weighted by its SS time.
// Logs which spread axis is blocking finalization if criteria
// not yet met, to aid tuning.
// Does nothing if referenceFinalized is already true.
// ============================================================

static void selectReferenceBins() {
  if (!effMatrix || referenceFinalized) return;

  // Build candidate list — all bins meeting minimum SS time
  struct Candidate {
    int idx, r, t, f;
    uint32_t ss_seconds;
  };

  static Candidate candidates[NUM_MATRIX_CELLS];
  int nCandidates = 0;

  for (int r = 0; r < NUM_RPM_BUCKETS; r++) {
    for (int t = 0; t < NUM_TEMP_BUCKETS; t++) {
      for (int f = 0; f < NUM_FIELD_BUCKETS; f++) {
        MatrixCell &cell = MATRIX_CELL(r, t, f);
        if (cell.ss_seconds >= REF_MIN_SS_SECONDS) {
          candidates[nCandidates++] = {
            MATRIX_IDX(r, t, f), r, t, f, cell.ss_seconds
          };
        }
      }
    }
  }

  if (nCandidates < NUM_REFERENCE_BINS) return;

  // Sort descending by SS time
  for (int i = 1; i < nCandidates; i++) {
    Candidate key = candidates[i];
    int j = i - 1;
    while (j >= 0 && candidates[j].ss_seconds < key.ss_seconds) {
      candidates[j + 1] = candidates[j];
      j--;
    }
    candidates[j + 1] = key;
  }

  // Greedy spread-first selection
  // selected[] tracks indices into candidates[]
  static int selected[NUM_REFERENCE_BINS];
  bool used[NUM_MATRIX_CELLS] = {};
  int nSelected = 0;

  // Seed: highest SS-time bin
  selected[nSelected++] = 0;
  used[0] = true;

  // Track current span of selected set
  float minTempLow = TEMP_BOUNDS[candidates[0].t];
  float maxTempHigh = TEMP_BOUNDS[candidates[0].t + 1];
  float minFLow = FIELD_BOUNDS[candidates[0].f];
  float maxFHigh = FIELD_BOUNDS[candidates[0].f + 1];
  float minRPMLow = RPM_BOUNDS[candidates[0].r];
  float maxRPMHigh = RPM_BOUNDS[candidates[0].r + 1];

  while (nSelected < NUM_REFERENCE_BINS) {
    int bestCand = -1;
    float bestScore = -1.0f;

    for (int i = 0; i < nCandidates; i++) {
      if (used[i]) continue;

      const Candidate &c = candidates[i];

      // How much does adding this bin expand the bounding box?
      float newTempLow = fminf(minTempLow, TEMP_BOUNDS[c.t]);
      float newTempHigh = fmaxf(maxTempHigh, TEMP_BOUNDS[c.t + 1]);
      float newFLow = fminf(minFLow, FIELD_BOUNDS[c.f]);
      float newFHigh = fmaxf(maxFHigh, FIELD_BOUNDS[c.f + 1]);
      float newRPMLow = fminf(minRPMLow, RPM_BOUNDS[c.r]);
      float newRPMHigh = fmaxf(maxRPMHigh, RPM_BOUNDS[c.r + 1]);

      float spreadGain =
        (newTempHigh - newTempLow - (maxTempHigh - minTempLow)) / 200.0f + (newFHigh - newFLow - (maxFHigh - minFLow)) / 15.0f + (newRPMHigh - newRPMLow - (maxRPMHigh - minRPMLow)) / 5000.0f;

      // Weight by SS time so low-data bins don't win purely on spread
      float score = spreadGain * logf((float)c.ss_seconds + 1.0f);

      if (score > bestScore) {
        bestScore = score;
        bestCand = i;
      }
    }

    if (bestCand < 0) break;  // Ran out of candidates

    const Candidate &c = candidates[bestCand];
    selected[nSelected++] = bestCand;
    used[bestCand] = true;

    // Update tracked span
    minTempLow = fminf(minTempLow, TEMP_BOUNDS[c.t]);
    maxTempHigh = fmaxf(maxTempHigh, TEMP_BOUNDS[c.t + 1]);
    minFLow = fminf(minFLow, FIELD_BOUNDS[c.f]);
    maxFHigh = fmaxf(maxFHigh, FIELD_BOUNDS[c.f + 1]);
    minRPMLow = fminf(minRPMLow, RPM_BOUNDS[c.r]);
    maxRPMHigh = fmaxf(maxRPMHigh, RPM_BOUNDS[c.r + 1]);
  }

  // Measure final spread
  float tempSpread = maxTempHigh - minTempLow;
  float fieldSpread = maxFHigh - minFLow;
  float rpmSpread = maxRPMHigh - minRPMLow;

  // Total SS across selected set
  uint32_t totalSS = 0;
  for (int i = 0; i < nSelected; i++) {
    totalSS += candidates[selected[i]].ss_seconds;
  }

  bool spreadOK = (tempSpread >= REF_SPREAD_TEMP_DEG && fieldSpread >= REF_SPREAD_FIELD_VOLTS && rpmSpread >= REF_SPREAD_RPM);
  bool ssOK = (totalSS >= REF_FREEZE_TOTAL_SS);

  if (!spreadOK || !ssOK) {
    // Log which axis is blocking — aids tuning
    queueConsoleMessageF(
      "EffMatrix: ref pending — "
      "temp %.0fF/%.0F%s field %.1fV/%.1f%s RPM %.0f/%.0f%s SS %us/%us%s",
      tempSpread, REF_SPREAD_TEMP_DEG, spreadOK || tempSpread >= REF_SPREAD_TEMP_DEG ? "" : "!",
      fieldSpread, REF_SPREAD_FIELD_VOLTS, spreadOK || fieldSpread >= REF_SPREAD_FIELD_VOLTS ? "" : "!",
      rpmSpread, REF_SPREAD_RPM, spreadOK || rpmSpread >= REF_SPREAD_RPM ? "" : "!",
      totalSS, REF_FREEZE_TOTAL_SS, ssOK ? "" : "!");
    return;
  }

  // Clear all existing reference flags
  for (int i = 0; i < NUM_MATRIX_CELLS; i++) {
    effMatrix[i].is_reference_bin = 0;
  }

  // Freeze reference layer for selected bins
  for (int i = 0; i < nSelected; i++) {
    MatrixCell &cell = effMatrix[candidates[selected[i]].idx];
    cell.ref_avg_amps = cell.avg_amps;
    cell.ref_min_amps = cell.min_amps;
    cell.ref_max_amps = cell.max_amps;
    cell.is_reference_bin = 1;

    queueConsoleMessageF(
      "EffMatrix: RefBin[%d] RPM=%s Temp=%s Field=%s avg=%.1fA [%.1f-%.1f] SS=%us",
      i,
      RPM_LABELS[candidates[selected[i]].r],
      TEMP_LABELS[candidates[selected[i]].t],
      FIELD_LABELS[candidates[selected[i]].f],
      cell.ref_avg_amps, cell.ref_min_amps, cell.ref_max_amps,
      candidates[selected[i]].ss_seconds);
  }

  referenceFinalized = true;
  saveEfficiencyMatrix();

  queueConsoleMessageF(
    "EffMatrix: Reference FROZEN — %d bins "
    "temp=%.0fF field=%.1fV RPM=%.0f totalSS=%us",
    nSelected, tempSpread, fieldSpread, rpmSpread, totalSS);
}


// ============================================================
// 2-MINUTE WINDOW MERGE
//
// Winner-takes-all: the bin with the most SS seconds this window
// is merged into the persistent matrix. All others discarded.
// Window accumulators cleared after merge.
// NVS save and reference re-evaluation happen here.
// ============================================================

static void mergeWindowIntoMatrix() {
  if (!effMatrix || !effWindow) return;

  int winnerIdx = -1;
  uint32_t maxSS = 0;

  for (int i = 0; i < MAX_ACTIVE_BINS_PER_WINDOW; i++) {
    if (effWindow[i].active && effWindow[i].ss_seconds > maxSS) {
      maxSS = effWindow[i].ss_seconds;
      winnerIdx = i;
    }
  }

  if (winnerIdx >= 0) {
    WindowSlot &win = effWindow[winnerIdx];
    MatrixCell &cell = MATRIX_CELL(win.r, win.t, win.f);

    uint32_t oldSS = cell.ss_seconds;
    uint32_t newSS = oldSS + win.ss_seconds;

    if (newSS > 0) {
      cell.avg_amps = (cell.avg_amps * (float)oldSS + win.wt_avg_amps * (float)win.ss_seconds) / (float)newSS;
    }
    cell.ss_seconds = newSS;

    // min_amps: treat 0.0 in cell as "not yet set"
    if (cell.min_amps == 0.0f || win.min_amps < cell.min_amps)
      cell.min_amps = win.min_amps;
    if (win.max_amps > cell.max_amps)
      cell.max_amps = win.max_amps;

    saveEfficiencyMatrix();
    selectReferenceBins();
  }
  // Save current session health snapshot so it survives unexpected power loss
  saveCurrentSessionHealth();
  // Clear all window accumulators regardless of whether there was a winner
  memset(effWindow, 0, (size_t)MAX_ACTIVE_BINS_PER_WINDOW * sizeof(WindowSlot));
}

// ============================================================
// ANOMALY DETECTION — TWO LAYERS
//
// Layer 1: Instantaneous — fires if current amps is outside
//   (ref_min - margin) to (ref_max + margin).
//   Catches sudden failures in both directions.
//   Fires every qualifying tick.
//
// Layer 2: Thermal average — fires once per bin per session
//   after count >= EFF_THERMAL_MIN_SAMPLES samples accumulated.
//   Compares session average to reference average.
//   Catches gradual degradation masked by thermal lag.
//   Only fires once per bin per session to avoid flooding.
//
// Both layers require referenceFinalized and is_reference_bin.
// sessionErrorCount = layer1 + layer2 combined.
// PLACEHOLDER: EFF_THERMAL_MIN_SAMPLES = 90 — tune if thermal
//   time constant differs from expected 1-2 minutes.
// ============================================================

#define EFF_THERMAL_MIN_SAMPLES 90  // PLACEHOLDER: ~90 seconds at 1Hz

static void checkAnomaly(int r, int t, int f, float amps) {
  if (!referenceFinalized) return;
  if (!sessionStats) return;

  MatrixCell &cell = MATRIX_CELL(r, t, f);
  SessionBinStats &ss = sessionStats[MATRIX_IDX(r, t, f)];

  if (!cell.is_reference_bin) return;
  if (cell.ref_avg_amps <= 0) return;  // Reference not yet valid

  // ── Accumulate session stats for this bin (both layers use this) ──
  ss.sum_amps += amps;
  ss.count++;
  // Accumulate session health ratio for sparkline history
  sessionHealthSum += amps / cell.ref_avg_amps;
  sessionHealthCount++;

  // ── Layer 1: Instantaneous min/max check (both directions) ──
  float effectiveMin = cell.ref_min_amps - anomalyMarginAmps;
  float effectiveMax = cell.ref_max_amps + anomalyMarginAmps;

  if (amps < effectiveMin || amps > effectiveMax) {
    sessionLayer1Errors++;
    float delta = (amps < effectiveMin)
                    ? (effectiveMin - amps)
                    : (amps - effectiveMax);
    const char *dir = (amps < effectiveMin) ? "LOW" : "HIGH";

    queueConsoleMessageF(
      "EffAnomaly L1: bin[%d,%d,%d] %s amps=%.1fA ref=[%.1f,%.1f] "
      "margin=%.1f delta=%.1fA L1=%d L2=%d",
      r, t, f, dir, amps,
      cell.ref_min_amps, cell.ref_max_amps,
      anomalyMarginAmps, delta,
      sessionLayer1Errors, sessionLayer2Errors);
  }

  // ── Layer 2: Thermal average check — only after enough samples ──
  if (!ss.trend_fired && ss.count >= EFF_THERMAL_MIN_SAMPLES) {
    float sessionAvg = ss.sum_amps / (float)ss.count;
    float refAvg = cell.ref_avg_amps;

    float upperLimit = refAvg * (1.0f + degradationThreshold);
    float lowerLimit = refAvg * (1.0f - degradationThreshold);

    if (sessionAvg < lowerLimit || sessionAvg > upperLimit) {
      ss.trend_fired = true;
      sessionLayer2Errors++;

      float pctDelta = ((sessionAvg - refAvg) / refAvg) * 100.0f;
      const char *dir = (sessionAvg < lowerLimit) ? "LOW" : "HIGH";

      queueConsoleMessageF(
        "EffAnomaly L2: bin[%d,%d,%d] thermal avg %s "
        "session=%.1fA ref=%.1fA delta=%.1f%% threshold=%.0f%% "
        "L1=%d L2=%d",
        r, t, f, dir,
        sessionAvg, refAvg, pctDelta,
        degradationThreshold * 100.0f,
        sessionLayer1Errors, sessionLayer2Errors);
    }
  }

  // ── Combined alarm check ──
  int totalErrors = sessionLayer1Errors + sessionLayer2Errors;
  if (anomalyAlarmEnable && totalErrors >= anomalyAlarmThreshold) {
    effAnomalyAlarmActive = true;
    queueConsoleMessageF(
      "EffAnomaly: ALARM — %d total errors (L1=%d L2=%d) threshold=%d",
      totalErrors, sessionLayer1Errors, sessionLayer2Errors,
      anomalyAlarmThreshold);
  }
}

void initEfficiencyTracker() {
  const size_t matrixSize = (size_t)NUM_MATRIX_CELLS * sizeof(MatrixCell);
  const size_t windowSize = (size_t)MAX_ACTIVE_BINS_PER_WINDOW * sizeof(WindowSlot);
  const size_t sessionSize = (size_t)NUM_MATRIX_CELLS * sizeof(SessionBinStats);

  // Persistent matrix — PSRAM, loaded from NVS
  effMatrix = (MatrixCell *)ps_malloc(matrixSize);
  if (!effMatrix) {
    queueConsoleMessage("ERROR: EffMatrix ps_malloc failed");
    return;
  }
  memset(effMatrix, 0, matrixSize);

  // Window accumulators — PSRAM, session only, never persisted
  effWindow = (WindowSlot *)ps_malloc(windowSize);
  if (!effWindow) {
    queueConsoleMessage("ERROR: EffWindow ps_malloc failed");
    return;
  }
  memset(effWindow, 0, windowSize);

  // Session bin stats — PSRAM, session only, never persisted
  sessionStats = (SessionBinStats *)ps_malloc(sessionSize);
  if (!sessionStats) {
    queueConsoleMessage("ERROR: EffSessionStats ps_malloc failed");
    return;
  }
  memset(sessionStats, 0, sessionSize);

  loadEfficiencyMatrix();

  int populated = 0;
  int refBins = 0;
  uint32_t totalSS = 0;
  for (int i = 0; i < NUM_MATRIX_CELLS; i++) {
    if (effMatrix[i].ss_seconds > 0) populated++;
    totalSS += effMatrix[i].ss_seconds;
    if (effMatrix[i].is_reference_bin) refBins++;
  }

  queueConsoleMessageF(
    "EffMatrix: %d/%d bins populated, %lu total SS sec, "
    "%d ref bins, finalized=%d, matrix=%.1fKB session=%.1fKB",
    populated, NUM_MATRIX_CELLS, (unsigned long)totalSS,
    refBins, (int)referenceFinalized,
    (float)matrixSize / 1024.0f,
    (float)sessionSize / 1024.0f);

  // Re-evaluate reference bins in case matrix grew but finalized flag was lost
  if (!referenceFinalized) selectReferenceBins();
}


// ============================================================
// RED DOT UPDATE — 1Hz
// ============================================================

void updateEfficiencyRedDot() {
  if (!effMatrix) return;

  float battV = getBatteryVoltage();
  float amps = isnan(MeasuredAmps) ? 0.0f : MeasuredAmps;
  float fieldVolts = dutyCycle * battV / 100.0f;

  activeRPMBucket = getRPMBucket(RPM);
  activeTempBucket = getTempBucket(TempToUse);
  activeFieldBucket = getFieldBucket(fieldVolts);

  redDotValid = (dutyCycle > 5.0f && amps > 2.0f && activeRPMBucket >= 0 && activeTempBucket >= 0 && activeFieldBucket >= 0 && !isnan(battV) && battV >= EFF_MIN_BATT_V);

  if (redDotValid) {
    redDot_fieldVolts = fieldVolts;
    redDot_amps = amps;
  }
}


// ============================================================
// MATRIX UPDATE + ANOMALY SCORING — 1Hz
//
// Uses unified checkPointValid() gate for both accumulation
// and anomaly scoring. Same criteria, same data, always.
// Window accumulator updated here; merge happens on 2-min tick.
// ============================================================

void updateEfficiencyMatrix() {
  if (!effMatrix || !effWindow) return;

  float battV = getBatteryVoltage();
  float amps = isnan(MeasuredAmps) ? 0.0f : MeasuredAmps;
  float fieldVolts = 0.0f;
  int r = -1, t = -1, f = -1;

  if (!checkPointValid(amps, battV, fieldVolts, r, t, f)) return;

  // ── Window accumulation (feeds permanent matrix on 2-min merge) ──
  // Find existing slot for this bin or claim an empty one
  int slotIdx = -1;
  for (int i = 0; i < MAX_ACTIVE_BINS_PER_WINDOW; i++) {
    if (effWindow[i].active && effWindow[i].r == r && effWindow[i].t == t && effWindow[i].f == f) {
      slotIdx = i;
      break;
    }
  }
  if (slotIdx < 0) {
    // Find empty slot
    for (int i = 0; i < MAX_ACTIVE_BINS_PER_WINDOW; i++) {
      if (!effWindow[i].active) {
        slotIdx = i;
        effWindow[i].r = (int8_t)r;
        effWindow[i].t = (int8_t)t;
        effWindow[i].f = (int8_t)f;
        effWindow[i].min_amps = amps;
        effWindow[i].max_amps = amps;
        break;
      }
    }
  }

  if (slotIdx >= 0) {
    WindowSlot &slot = effWindow[slotIdx];
    uint32_t oldSS = slot.ss_seconds;
    uint32_t newSS = oldSS + 1;
    slot.wt_avg_amps = (slot.wt_avg_amps * (float)oldSS + amps) / (float)newSS;
    slot.ss_seconds = newSS;
    if (amps < slot.min_amps) slot.min_amps = amps;
    if (amps > slot.max_amps) slot.max_amps = amps;
    slot.active = true;
  } else {
    // All slots full — this window has unusually many active bins
    // Point is not accumulated this window but anomaly check still runs
    queueConsoleMessage("EffMatrix: window slots full — increase MAX_ACTIVE_BINS_PER_WINDOW");
  }

  // ── Anomaly scoring — runs regardless of slot availability ──
  checkAnomaly(r, t, f, amps);
}


// ============================================================
// RESET — wipes matrix, window, and reference state entirely
// Called from "Start Over" / ResetEfficiencyMatrix HTTP handler
// ============================================================

void resetEfficiencyMatrix() {
  if (effMatrix) {
    memset(effMatrix, 0, (size_t)NUM_MATRIX_CELLS * sizeof(MatrixCell));
    saveEfficiencyMatrix();
  }
  if (effWindow) {
    memset(effWindow, 0, (size_t)MAX_ACTIVE_BINS_PER_WINDOW * sizeof(WindowSlot));
  }
  if (sessionStats) {
    memset(sessionStats, 0, (size_t)NUM_MATRIX_CELLS * sizeof(SessionBinStats));
  }

  // Clear session history and trend data
  memset(&effHistory, 0, sizeof(EffHistoryData));
  sessionHealthSum = 0.0f;
  sessionHealthCount = 0;
  saveEffHistory();
  effHistoryDirty = true;

  referenceFinalized = false;
  sessionLayer1Errors = 0;
  sessionLayer2Errors = 0;
  effAnomalyAlarmActive = false;

  // Remove legacy sessionErrorCount if still present — replaced by L1+L2 counters
  queueConsoleMessage("EffMatrix: Full reset — all matrix, reference, and history data cleared");
}


// ============================================================
// SEND MATRIX STATE TO CLIENT — 5s cadence, change-triggered
//
// SSE event: "EffMatrix"
// Payload:
//   state, rBucket, tBucket, fBucket,
//   rLabel, tLabel, fLabel,
//   ss_seconds, avg_amps, min_amps, max_amps,
//   is_reference_bin, sessionErrorCount
//
// state: 0 = weak/empty (no reference data)
//        1 = populated but not a reference bin (low confidence)
//        2 = finalized reference bin (full anomaly scoring)
// ============================================================

void sendEfficiencyData() {
  if (!effMatrix) return;
  if (activeRPMBucket < 0 || activeTempBucket < 0 || activeFieldBucket < 0) return;

  static int lastR = -1;
  static int lastT = -1;
  static int lastF = -1;
  static uint32_t lastSS = 0xFFFFFFFF;
  static int lastErr = -1;

  MatrixCell &cell = MATRIX_CELL(activeRPMBucket, activeTempBucket, activeFieldBucket);

  int currentErrors = sessionLayer1Errors + sessionLayer2Errors;
  bool changed = (activeRPMBucket != lastR || activeTempBucket != lastT || activeFieldBucket != lastF || cell.ss_seconds != lastSS || currentErrors != lastErr);
  if (!changed) return;

  int state;
  if (cell.is_reference_bin) {
    state = 2;
  } else if (cell.ss_seconds >= REF_MIN_SS_SECONDS) {
    state = 1;
  } else {
    state = 0;
  }

  char buf[220];
  snprintf(buf, sizeof(buf),
           "%d,%d,%d,%d,%s,%s,%s,%lu,%.2f,%.2f,%.2f,%d,%d",
           state,
           activeRPMBucket, activeTempBucket, activeFieldBucket,
           RPM_LABELS[activeRPMBucket],
           TEMP_LABELS[activeTempBucket],
           FIELD_LABELS[activeFieldBucket],
           (unsigned long)cell.ss_seconds,
           cell.avg_amps, cell.min_amps, cell.max_amps,
           (int)cell.is_reference_bin,
           sessionLayer1Errors + sessionLayer2Errors);

  events.send(buf, "EffMatrix");

  lastR = activeRPMBucket;
  lastT = activeTempBucket;
  lastF = activeFieldBucket;
  lastSS = cell.ss_seconds;
  lastErr = sessionErrorCount;
}

void sendEfficiencyRedDot() {
  char buf[80];
  snprintf(buf, sizeof(buf), "%d,%.2f,%.2f,%d,%d,%d",
           (int)redDotValid,
           redDot_fieldVolts,
           redDot_amps,
           activeRPMBucket,
           activeTempBucket,
           activeFieldBucket);
  events.send(buf, "EffRed");
}


// ============================================================
// TICK — call from loop() every iteration, self-timed
// ============================================================

void efficiencyTracker_tick() {
  static uint32_t last1Hz = 0;
  static uint32_t last5s = 0;
  static uint32_t last2min = 0;
  uint32_t now = millis();

  if (now - last1Hz >= 1000) {
    last1Hz = now;
    updateEfficiencyRedDot();
    updateEfficiencyMatrix();
  }

  if (now - last5s >= 5000) {
    last5s = now;
    sendEfficiencyData();
    sendEfficiencyRedDot();
    sendEfficiencyHistory();
  }

  if (now - last2min >= 120000) {
    last2min = now;
    mergeWindowIntoMatrix();
  }
}



// ============================================================
// SESSION HISTORY NVS — save/load
//
// "eff_hist" blob  = EffHistoryData struct (committed sessions)
// "eff_cs"   blob  = single float (current session health,
//                    updated every 2 min, committed on next boot)
// ============================================================

static void saveEffHistory() {
  nvs_handle_t handle;
  if (nvs_open("effmat", NVS_READWRITE, &handle) != ESP_OK) {
    queueConsoleMessage("EffHistory: NVS open failed (save)");
    return;
  }
  nvs_set_blob(handle, "eff_hist", &effHistory, sizeof(EffHistoryData));
  nvs_commit(handle);
  nvs_close(handle);
}

static void saveCurrentSessionHealth() {
  if (sessionHealthCount == 0) return;
  float ratio = sessionHealthSum / (float)sessionHealthCount;

  nvs_handle_t handle;
  if (nvs_open("effmat", NVS_READWRITE, &handle) != ESP_OK) return;
  nvs_set_blob(handle, "eff_cs", &ratio, sizeof(float));
  nvs_commit(handle);
  nvs_close(handle);
}

static void loadEffHistory() {
  nvs_handle_t handle;
  if (nvs_open("effmat", NVS_READONLY, &handle) != ESP_OK) return;

  // Load committed history
  size_t sz = sizeof(EffHistoryData);
  nvs_get_blob(handle, "eff_hist", &effHistory, &sz);

  // Commit previous session if one was saved
  float prevSessionRatio = 0.0f;
  size_t fsz = sizeof(float);
  esp_err_t err = nvs_get_blob(handle, "eff_cs", &prevSessionRatio, &fsz);
  nvs_close(handle);

  if (err == ESP_OK && prevSessionRatio > 0.1f && prevSessionRatio < 3.0f) {
    // Valid previous session — commit it to history
    effHistory.values[effHistory.head] = prevSessionRatio;
    effHistory.head = (effHistory.head + 1) % EFF_HISTORY_SESSIONS;
    if (effHistory.count < EFF_HISTORY_SESSIONS) effHistory.count++;
    saveEffHistory();

    // Clear committed session marker
    nvs_handle_t h2;
    if (nvs_open("effmat", NVS_READWRITE, &h2) == ESP_OK) {
      float zero = 0.0f;
      nvs_set_blob(h2, "eff_cs", &zero, sizeof(float));
      nvs_commit(h2);
      nvs_close(h2);
    }

    queueConsoleMessageF(
      "EffHistory: committed previous session health=%.2f (%d sessions stored)",
      prevSessionRatio, effHistory.count);

    effHistoryDirty = true;
  }
}


// ============================================================
// SEND SESSION HISTORY TO CLIENT
//
// SSE event: "EffHistory"
// Payload: count,head,v0,v1,v2,...,v29
//   count = number of valid entries
//   head  = index of oldest entry (for correct ordering in JS)
//   v0..v29 = all 30 slots in raw array order
//             (JS reconstructs chronological order using head/count)
// Sent when effHistoryDirty is set, then flag cleared.
// Also sent on first call after boot via boot_sent flag.
// ============================================================

void sendEfficiencyHistory() {
  static bool bootSent = false;
  if (!effHistoryDirty && bootSent) return;

  char buf[300];
  int offset = snprintf(buf, sizeof(buf), "%d,%d",
                        (int)effHistory.count,
                        (int)effHistory.head);

  for (int i = 0; i < EFF_HISTORY_SESSIONS; i++) {
    int remaining = (int)sizeof(buf) - offset;
    if (remaining < 12) break;
    offset += snprintf(buf + offset, remaining, ",%.3f",
                       effHistory.values[i]);
  }

  events.send(buf, "EffHistory");
  effHistoryDirty = false;
  bootSent = true;
}




// ===========================================================================
// VOLTAGE MODE SUPPORT FUNCTIONS
// ===========================================================================
void cvLog_init() {
  if (!psramFound()) {
    Serial.println("cvLog: PSRAM not found, disabled");
    cvLogReady = false;
    cvLog = nullptr;
    return;
  }

  cvLog = (CvLogEntry *)heap_caps_malloc(
    CV_LOG_SIZE * sizeof(CvLogEntry),
    MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);

  if (!cvLog) {
    Serial.println("cvLog: PSRAM malloc failed");
    cvLogReady = false;
    return;
  }

  cvLogHead = 0;
  cvLogCount = 0;
  cvLogReady = true;

  Serial.printf("cvLog: %d entries × %u bytes = %u KB PSRAM\n",
                CV_LOG_SIZE,
                (unsigned)sizeof(CvLogEntry),
                (unsigned)(CV_LOG_SIZE * sizeof(CvLogEntry) / 1024));
}

void cvLog_tick(uint32_t nowMs) {
  if (!cvLogReady || !cvLog) return;
  if (sysMode != SYS_MODE_AUTO) return;

  // Pause watchdog — same pattern as thermalLog
  if (cvLogPaused) {
    if ((uint32_t)(nowMs - cvLogPausedAtMs) > THERMAL_LOG_PAUSE_TIMEOUT_MS) {
      Serial.println("cvLog: pause watchdog triggered - resuming");
      cvLogPaused = false;
    } else {
      return;
    }
  }

  CvLogEntry &e = cvLog[cvLogHead];

  e.ts = nowMs;
  e.battV = (int16_t)(BatteryV * 100.0f);
  e.targV = (int16_t)(ChargingVoltageTarget * 100.0f);
  e.vErrorMv = (int16_t)((ChargingVoltageTarget - BatteryV) * 1000.0f);
  e.dvdt_x1000 = (int16_t)clamp_f(g_fastOvDvdt * 1000.0f, -32767.0f, 32767.0f);
  e.vPred = (int16_t)(g_fastOvVpred * 100.0f);
  e.fastOvCap = (int16_t)(g_fastOvCurrentCap * 10.0f);
  e.cv_I_x10 = (int16_t)clamp_f(cv_I * 10.0f, -32767.0f, 32767.0f);
  e.Icv_x10 = (int16_t)clamp_f(Icv * 10.0f, -32767.0f, 32767.0f);
  e.uTarget = (int16_t)clamp_f((float)uTargetAmps * 10.0f, -32767.0f, 32767.0f);
  e.spLimited = (int16_t)clamp_f(setpointLimited * 10.0f, -32767.0f, 32767.0f);
  e.iMeas = (int16_t)clamp_f(MeasuredAmps * 10.0f, -32767.0f, 32767.0f);
  e.duty = (int16_t)(dutyCycle * 10.0f);

  e.flags = 0;
  if (g_fastOvClampActive) e.flags |= (1 << 0);
  if (pidLog_voltageLoopRanThisTick) e.flags |= (1 << 1);
  if (voltageControlActive) e.flags |= (1 << 2);
  if (g_fastOvSoftActive) e.flags |= (1 << 3);
  if (g_fastOvHardActive) e.flags |= (1 << 4);

  e.pad = 0;
  e.rpm = (int16_t)constrain((int)RPM, -32768, 32767);
  e.iMA2_x10 = (int16_t)clamp_f(g_iMA2 * 10.0f, -32767.0f, 32767.0f);
  e.iMA4_x10 = (int16_t)clamp_f(g_iMA4 * 10.0f, -32767.0f, 32767.0f);
  e.dIdt2_x10 = (int16_t)clamp_f(g_dIdt2 * 10.0f, -32767.0f, 32767.0f);
  e.dIdt4_x10 = (int16_t)clamp_f(g_dIdt4 * 10.0f, -32767.0f, 32767.0f);
  e.ch1IntervalMs = (int16_t)g_ch1LastIntervalMs;

  if (g_fastIRisingActive) e.flags |= (1 << 5);

  cvLogHead = (cvLogHead + 1) % CV_LOG_SIZE;
  if (cvLogCount < CV_LOG_SIZE) cvLogCount++;
}

// Call once per confirmed good CH1 read, at the very top of case 1.
// Runs in the ADC hot path — no Serial, no allocation.
void ch1_record(uint32_t now) {
  if (!ch1HasPrev) {
    ch1PrevTs = now;
    ch1BktStart = now;
    ch1HasPrev = true;
    return;
  }

  uint32_t diff = now - ch1PrevTs;
  ch1PrevTs = now;
  uint16_t iv = (diff > 65535u) ? 65535u : (uint16_t)diff;
  g_ch1LastIntervalMs = iv;  // export to cvLog


  // ── 10s ring ──────────────────────────────────────────────────────────
  ch1Ring[ch1Head] = { now, iv };
  ch1Head = (ch1Head + 1) % CH1_RING;
  if (ch1Count < CH1_RING) ch1Count++;

  // ── All-time accumulators ─────────────────────────────────────────────
  ch1AtCount++;
  ch1AtSum += iv;
  if (iv > ch1AtWorst) ch1AtWorst = iv;
  // over2x uses running mean at this instant as threshold — known approximation,
  // acceptable for a session-level diagnostic counter.
  if (ch1AtCount > 1) {
    float runMean = (float)((double)ch1AtSum / ch1AtCount);
    if ((float)iv > runMean * 2.0f) ch1AtOver2x++;
  }

// ── 1s mini-bucket: incremental update, O(1), no ring scan ────────────
  ch1Bkt1sCurrent.sum += iv;
  ch1Bkt1sCurrent.count++;
  if (iv > ch1Bkt1sCurrent.worst) ch1Bkt1sCurrent.worst = iv;

  // 1s rollover: close current mini-bucket, open a new one
  if (now - ch1Bkt1sStart >= 1000UL) {
    ch1Bkt1s[ch1Bkt1sHead] = ch1Bkt1sCurrent;
    ch1Bkt1sHead  = (ch1Bkt1sHead + 1) % CH1_1S_BUCKETS;
    if (ch1Bkt1sCount < CH1_1S_BUCKETS) ch1Bkt1sCount++;
    ch1Bkt1sCurrent = { 0, 0, 0, 0 };
    ch1Bkt1sStart   = now;
  }

  // ── 10s→2m bucket rollover: O(10) mini-bucket sum, no ring scan ────────
  // over2x approximated using bucket mean * 2 threshold (acceptable for 2m diagnostic)
  if (now - ch1BktStart >= 10000UL) {
    Ch1Bucket bkt = { 0, 0, 0, 0 };

    // Sum all closed 1s mini-buckets
    for (uint8_t i = 0; i < ch1Bkt1sCount; i++) {
      uint8_t idx = (ch1Bkt1sHead + CH1_1S_BUCKETS - 1 - i) % CH1_1S_BUCKETS;
      bkt.sum   += ch1Bkt1s[idx].sum;
      bkt.count += ch1Bkt1s[idx].count;
      if (ch1Bkt1s[idx].worst > bkt.worst) bkt.worst = ch1Bkt1s[idx].worst;
    }
    // Include currently open mini-bucket
    bkt.sum   += ch1Bkt1sCurrent.sum;
    bkt.count += ch1Bkt1sCurrent.count;
    if (ch1Bkt1sCurrent.worst > bkt.worst) bkt.worst = ch1Bkt1sCurrent.worst;

    // over2x: approximate — count mini-buckets whose worst exceeds 2× overall mean
    // (per-sample accuracy not possible without ring scan; this is a diagnostic counter)
    if (bkt.count > 0) {
      float thresh = ((float)bkt.sum / (float)bkt.count) * 2.0f;
      for (uint8_t i = 0; i < ch1Bkt1sCount; i++) {
        uint8_t idx = (ch1Bkt1sHead + CH1_1S_BUCKETS - 1 - i) % CH1_1S_BUCKETS;
        bkt.over2x += ch1Bkt1s[idx].over2x;  // carry forward from mini-buckets if tracked
        if ((float)ch1Bkt1s[idx].worst > thresh) bkt.over2x++;
      }
    }

    ch1Buckets[ch1BktHead] = bkt;
    ch1BktHead = (ch1BktHead + 1) % CH1_BUCKETS;
    if (ch1BktCount < CH1_BUCKETS) ch1BktCount++;
    ch1BktStart = now;
  }
}

void ch1_compute_stats() {
  if (ch1Count == 0) return;

  uint32_t now = millis();
// ── 10s: O(10) mini-bucket scan — no ring access, no PSRAM thrash ────
  ch1_last_ms    = ch1Ring[(ch1Head + CH1_RING - 1) % CH1_RING].iv;  // O(1) single element
  ch1_n_10s      = ch1Bkt1sCurrent.count;   // start with open bucket
  ch1_worst_10s  = ch1Bkt1sCurrent.worst;
  ch1_over2x_10s = 0;                        // not tracked at 1s granularity
  uint32_t sum10 = ch1Bkt1sCurrent.sum;

  for (uint8_t i = 0; i < ch1Bkt1sCount; i++) {
    uint8_t idx = (ch1Bkt1sHead + CH1_1S_BUCKETS - 1 - i) % CH1_1S_BUCKETS;
    sum10        += ch1Bkt1s[idx].sum;
    ch1_n_10s    += ch1Bkt1s[idx].count;
    if (ch1Bkt1s[idx].worst > ch1_worst_10s) ch1_worst_10s = ch1Bkt1s[idx].worst;
  }
  if (ch1_n_10s > 0) ch1_avg_10s = (float)sum10 / (float)ch1_n_10s;
  else               ch1_avg_10s = 0.0f;

  // ── 2m ───────────────────────────────────────────────────────────────
  ch1_n_2m = 0;
  ch1_worst_2m = 0;
  ch1_over2x_2m = 0;
  ch1_avg_2m = 0;

  uint64_t sum2m = 0;
  for (uint8_t i = 0; i < ch1BktCount; i++) {
    uint8_t idx = (ch1BktHead + CH1_BUCKETS - 1 - i) % CH1_BUCKETS;
    ch1_n_2m += ch1Buckets[idx].count;
    sum2m += ch1Buckets[idx].sum;
    ch1_over2x_2m += ch1Buckets[idx].over2x;
    if (ch1Buckets[idx].worst > ch1_worst_2m) ch1_worst_2m = ch1Buckets[idx].worst;
  }
  if (ch1_n_2m > 0) ch1_avg_2m = (float)sum2m / (float)ch1_n_2m;

  // ── All-time ──────────────────────────────────────────────────────────
  ch1_n_at = ch1AtCount;
  ch1_worst_at = ch1AtWorst;
  ch1_avg_at = ch1AtCount > 0 ? (float)((double)ch1AtSum / ch1AtCount) : 0.0f;
  ch1_over2x_at = ch1AtOver2x;
}

void cacheGzFiles() {
  cachedIndex    = loadFileToRAM("/index.html.gz");
  cachedCss      = loadFileToRAM("/styles.css.gz");
  cachedJs       = loadFileToRAM("/script.js.gz");
  cachedUplotCss = loadFileToRAM("/uPlot.min.css.gz");
  cachedUplotJs  = loadFileToRAM("/uPlot.iife.min.js.gz");
}


bool serveCachedGz(AsyncWebServerRequest* request, const String& path, const String& contentType) {
  CachedGzFile* cf = nullptr;
  if      (path == "/index.html")        cf = &cachedIndex;
  else if (path == "/styles.css")        cf = &cachedCss;
  else if (path == "/script.js")         cf = &cachedJs;
  else if (path == "/uPlot.min.css")     cf = &cachedUplotCss;
  else if (path == "/uPlot.iife.min.js") cf = &cachedUplotJs;

  if (cf && cf->data && cf->size > 0) {
    uint8_t* data = cf->data;
    size_t   len  = cf->size;
    AsyncWebServerResponse* resp = request->beginResponse(
      contentType, len,
      [data, len](uint8_t* buffer, size_t maxLen, size_t index) -> size_t {
        size_t remaining = len - index;
        size_t toSend    = min(maxLen, remaining);
        memcpy(buffer, data + index, toSend);
        return toSend;
      });
    resp->addHeader("Content-Encoding", "gzip");
    resp->addHeader("Cache-Control", "public, max-age=3600");
    request->send(resp);
    return true;
  }
  return false;
}