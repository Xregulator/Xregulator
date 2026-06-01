
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
// ALTERNATOR HEALTH MODEL (Phase 2) — replaces the old 3-D eff matrix.
//   A_pred = base(rpm, excitation) × tempCorr(T) × busCorr(Vbus)
//   excitation proxy = temp-normalized field drive (NOT measured field current).
//   Data unit = ONE observation per bin VISIT (exit + re-enter required for another).
//   Per-cell freeze after X separate visits; advisory health % + drift status, no alarm.
//   Globals/structs are declared in Xregulator.ino. Full design: LOCAL_DATA_SYSTEMS_PLAN.md Phase 2.
// ============================================================

#define ALT_VER         1u
#define ALT_BASE_MAGIC  0x414C5442u  // 'ALTB'
#define ALT_TEMPC_MAGIC 0x414C5443u  // 'ALTC'
#define ALT_BUSC_MAGIC  0x414C5455u  // 'ALTU'
#define ALT_HLTH_MAGIC  0x414C5448u  // 'ALTH'

#define ALT_CORR_FREEZE_N 40         // each tempCorr/busCorr bin freezes after N samples → can't mask later degradation
static float altTempSlow = NAN;      // slow temp EMA for the model-free thermal-rate gate

// ---- bin + axis helpers ----
static inline int altRpmBin(float rpm) {
  if (isnan(rpm) || rpm < 0.0f) return -1;
  int b = (int)(rpm * ALT_RPM_BINS / ALT_RPM_MAX);
  if (b < 0) return -1;
  if (b >= ALT_RPM_BINS) b = ALT_RPM_BINS - 1;
  return b;
}
static inline int altFiBin(float fi) {
  if (isnan(fi) || fi < 0.0f) return -1;
  int b = (int)(fi * ALT_FI_BINS / ALT_FI_MAX);
  if (b < 0) return -1;
  if (b >= ALT_FI_BINS) b = ALT_FI_BINS - 1;
  return b;
}
static inline int altTempBin(float tF) {
  if (isnan(tF)) return -1;
  int b = (int)((tF - ALT_TEMP_MIN) * ALT_TEMP_BINS / (ALT_TEMP_MAX - ALT_TEMP_MIN));
  if (b < 0) b = 0;
  if (b >= ALT_TEMP_BINS) b = ALT_TEMP_BINS - 1;
  return b;
}
static inline int altVbusBin(float v) {
  if (isnan(v)) return -1;
  int b = (int)((v - ALT_VBUS_MIN) * ALT_VBUS_BINS / (ALT_VBUS_MAX - ALT_VBUS_MIN));
  if (b < 0) b = 0;
  if (b >= ALT_VBUS_BINS) b = ALT_VBUS_BINS - 1;
  return b;
}
// Temp-normalized field drive ("excitation proxy"): (dutyFrac × Vbus) / (1 + α(Tc − Tref)).
static inline float altExcitation(float duty, float vbus, float tF) {
  float tc = (tF - 32.0f) / 1.8f;
  float denom = 1.0f + ALT_ALPHA_PER_C * (tc - ALT_TREF_C);
  if (denom < 0.5f) denom = 0.5f;
  return (duty / 100.0f) * vbus / denom;
}
static inline float altTempCorrAt(int bin) {
  if (bin < 0 || !altTempCorr[bin].valid) return 1.0f;
  float v = altTempCorr[bin].value;
  if (v < 0.3f) v = 0.3f;
  if (v > 3.0f) v = 3.0f;
  return v;
}
static inline float altBusCorrAt(int bin) {
  if (bin < 0 || !altBusCorr[bin].valid) return 1.0f;
  float v = altBusCorr[bin].value;
  if (v < 0.3f) v = 0.3f;
  if (v > 3.0f) v = 3.0f;
  return v;
}

// ---- prediction (inverse-distance over graduated cells' centroids) ----
static bool altIdwBase(float rpm, float fi, float *outA, float *outSig, int *nUsed, float *nearDist) {
  int ri0 = altRpmBin(rpm), fb0 = altFiBin(fi);
  if (ri0 < 0 || fb0 < 0) return false;
  const float rw = ALT_RPM_MAX / ALT_RPM_BINS;
  const float fw = ALT_FI_MAX / ALT_FI_BINS;
  const int R = 3;
  double sumW = 0, sumWA = 0, sumWS = 0;
  int n = 0;
  float nearest = 1e9f;
  for (int dr = -R; dr <= R; dr++) {
    int ri = ri0 + dr;
    if (ri < 0 || ri >= ALT_RPM_BINS) continue;
    for (int dfb = -R; dfb <= R; dfb++) {
      int fb = fb0 + dfb;
      if (fb < 0 || fb >= ALT_FI_BINS) continue;
      AltCell &c = ALT_CELL(ri, fb);
      if (!c.ref_valid) continue;
      float ddr = (rpm - c.ref_rpm) / rw;
      float ddf = (fi - c.ref_fi) / fw;
      float d2 = ddr * ddr + ddf * ddf;
      if (d2 < nearest) nearest = d2;
      double w = 1.0 / (d2 + 0.05);
      sumW += w;
      sumWA += w * c.ref_amps;
      sumWS += w * c.ref_sigma;
      n++;
    }
  }
  if (n == 0 || sumW <= 0) return false;
  *outA = (float)(sumWA / sumW);
  *outSig = (float)(sumWS / sumW);
  *nUsed = n;
  *nearDist = sqrtf(nearest);
  return true;
}
// Base prediction + local slopes (finite-difference on the IDW surface) for the error bar.
static bool altPredict(float rpm, float fi, float *Apred, float *dAdr, float *dAdf,
                       float *refSig, int *nUsed, float *nearDist) {
  float a0, s0, nd;
  int n;
  if (!altIdwBase(rpm, fi, &a0, &s0, &n, &nd)) return false;
  *Apred = a0;
  *refSig = s0;
  *nUsed = n;
  *nearDist = nd;
  const float hr = ALT_RPM_MAX / ALT_RPM_BINS;
  const float hf = ALT_FI_MAX / ALT_FI_BINS;
  float ap, am, sd, dnd;
  int dn;
  *dAdr = (altIdwBase(rpm + hr, fi, &ap, &sd, &dn, &dnd) &&
           altIdwBase(rpm - hr, fi, &am, &sd, &dn, &dnd)) ? (ap - am) / (2 * hr) : 0.0f;
  *dAdf = (altIdwBase(rpm, fi + hf, &ap, &sd, &dn, &dnd) &&
           altIdwBase(rpm, fi - hf, &am, &sd, &dn, &dnd)) ? (ap - am) / (2 * hf) : 0.0f;
  return true;
}
// Sensitivity-aware error bar: knee (big slope) → wide; flat/well-sampled → tight.
static float altSigma(float ap, float dAdr, float dAdf, float fi, float refSig, float nearDist) {
  float sr = dAdr * altRpmTol;
  float sf = dAdf * fmaxf((altDutyTolPct / 100.0f) * fi, 0.05f);
  float sT = 0.03f * ap;                  // case-vs-winding temp uncertainty (one lump term)
  float sc = 0.05f * ap * nearDist;       // coverage: wider far from frozen data
  float s2 = sr * sr + sf * sf + sT * sT + refSig * refSig + sc * sc;
  return fmaxf(sqrtf(s2), 0.5f);
}

// ---- steady-state detector (electrical anchor band + model-free thermal rate band) ----
static bool altSteadyUpdate(float rpm, float fi, float vbus, float tF, float dt, uint32_t nowMs) {
  bool elecIn = fabsf(rpm - altSS_rpmRef) <= altRpmTol &&
                fabsf(vbus - altSS_vbusRef) <= altVbusTol &&
                fabsf(fi - altSS_fiRef) <= fmaxf((altDutyTolPct / 100.0f) * fmaxf(fi, 1.0f), 0.05f);
  if (!elecIn) {
    altSS_rpmRef = rpm;
    altSS_fiRef = fi;
    altSS_vbusRef = vbus;
    altSteadyStartMs = nowMs;
  }
  bool elecSteady = (nowMs - altSteadyStartMs) >= (uint32_t)(altElecSettleSec * 1000.0f);

  bool thermSteady;
  if (IgnoreTemperature || isnan(tF)) {
    thermSteady = true;
    altThermInBandMs = (uint32_t)(altThermDwellSec * 1000.0f);
  } else {
    float tau = fmaxf(altThermRateMin * 60.0f, 1.0f);
    if (isnan(altTempSlow)) altTempSlow = tF;
    else altTempSlow += (tF - altTempSlow) * (dt / tau);
    if (fabsf(tF - altTempSlow) <= altThermRateDegF) altThermInBandMs += (uint32_t)(dt * 1000.0f);
    else altThermInBandMs = 0;
    thermSteady = altThermInBandMs >= (uint32_t)(altThermDwellSec * 1000.0f);
  }
  return elecSteady && thermSteady;
}

// ---- visit accumulation / freeze / score ----
static void altResetVisit() {
  altVisit_w = 0;
  altVisit_A = 0;
  altVisit_A2 = 0;
  altVisit_rpm = 0;
  altVisit_fi = 0;
  altVisit_vbus = 0;
  altVisit_t = 0;
  altVisitSteadyMs = 0;
}
static void altTryFreeze(int idx) {
  AltCell &c = altBase[idx];
  if (c.nObs < (uint16_t)altFreezeMinVisits) return;
  float mean = (c.sumW > 0) ? c.sumW_A / c.sumW : 0.0f;
  float var = (c.sumW > 0) ? (c.sumW_A2 / c.sumW - mean * mean) : 0.0f;
  if (var < 0) var = 0;
  float sem = (c.nObs > 1) ? sqrtf(var / (float)(c.nObs - 1)) : 1e6f;  // sample SEM (÷ n−1); n=1 → only the cap can freeze
  if (sem > altFreezeSEM && c.nObs < (uint16_t)altFreezeMaxVisits) return;
  c.ref_amps = mean;
  c.ref_rpm = c.sumW_rpm / c.sumW;
  c.ref_fi = c.sumW_fi / c.sumW;
  c.ref_sigma = fmaxf(sem, 0.1f);
  c.ref_valid = 1;
  altHealth.baselineFrozen = 1;
  queueConsoleMessageF("AltHealth froze cell rpm=%.0f fi=%.2f ref=%.1fA sem=%.2f n=%u",
                       c.ref_rpm, c.ref_fi, c.ref_amps, sem, (unsigned)c.nObs);
}
// Learn the 1-D corrections from this visit's residual (only once base cells exist).
static void altLearnCorr(float meanA, float rpm, float fi, float vbus, float tF) {
  float ap, dr, df, rs, nd;
  int nu;
  if (!altPredict(rpm, fi, &ap, &dr, &df, &rs, &nu, &nd)) return;
  if (ap < 0.5f) return;
  if (nu < 3 || nd > 1.5f) return;       // only learn where the base map is well-supported (no 1-cell IDW garbage)
  int tb = altTempBin(tF);
  float tc = altTempCorrAt(tb);
  // Each correction bin FREEZES after N samples so it can't later absorb (mask) real degradation.
  if (tb >= 0 && altTempCorr[tb].nObs < ALT_CORR_FREEZE_N) {
    AltCorr &c = altTempCorr[tb];
    c.nObs++;
    c.sumW += 1.0f;
    c.sumW_ratio += meanA / ap;            // base-only residual → temp deviation
    c.value = c.sumW_ratio / c.sumW;
    c.valid = 1;
  }
  int vb = altVbusBin(vbus);
  if (vb >= 0 && altBusCorr[vb].nObs < ALT_CORR_FREEZE_N) {
    AltCorr &c = altBusCorr[vb];
    c.nObs++;
    c.sumW += 1.0f;
    c.sumW_ratio += meanA / (ap * (tc > 0.1f ? tc : 1.0f));  // temp-corrected residual → Vbus deviation
    c.value = c.sumW_ratio / c.sumW;
    c.valid = 1;
  }
}
static void altScore(float meanA, float rpm, float fi, float vbus, float tF) {
  float ap, dr, df, rs, nd;
  int nu;
  if (!altPredict(rpm, fi, &ap, &dr, &df, &rs, &nu, &nd)) return;
  float tc = altTempCorrAt(altTempBin(tF));
  float bc = altBusCorrAt(altVbusBin(vbus));
  float Apred = ap * tc * bc;
  if (Apred < 0.5f) return;
  float sig = altSigma(ap, dr, df, fi, rs, nd);
  float z = (meanA - Apred) / sig;
  float r = meanA / Apred;
  altHealth.ewmaRatio += (r - altHealth.ewmaRatio) * altEwmaLambda;
  altHealth.cusumPos = fmaxf(0.0f, altHealth.cusumPos + (z - altCusumK));
  altHealth.cusumNeg = fmaxf(0.0f, altHealth.cusumNeg + (-z - altCusumK));
  altHealth.obsCount++;
  if (altHealth.cusumPos > altCusumH) altHealth.status = 2;
  else if (altHealth.cusumNeg > altCusumH) altHealth.status = 3;
  else altHealth.status = 1;
}
static void altFinalizeVisit() {
  if (altCurBin < 0 || altVisit_w <= 0) return;
  if (altVisitSteadyMs < (uint32_t)(altMinDwellSec * 1000.0f)) return;
  float meanA = (float)(altVisit_A / altVisit_w);
  float meanRpm = (float)(altVisit_rpm / altVisit_w);
  float meanFi = (float)(altVisit_fi / altVisit_w);
  float meanVbus = (float)(altVisit_vbus / altVisit_w);
  float meanT = (float)(altVisit_t / altVisit_w);
  AltCell &c = altBase[altCurBin];
  if (!c.ref_valid) {
    if (c.nObs < (uint16_t)altFreezeMaxVisits) {
      c.nObs++;
      c.sumW += 1.0f;
      c.sumW_A += meanA;
      c.sumW_A2 += meanA * meanA;
      c.sumW_rpm += meanRpm;
      c.sumW_fi += meanFi;
      c.sumW_vbus += meanVbus;
      c.sumW_t += meanT;
    }
    altTryFreeze(altCurBin);
  } else {
    altScore(meanA, meanRpm, meanFi, meanVbus, meanT);
  }
  altLearnCorr(meanA, meanRpm, meanFi, meanVbus, meanT);
}

// ---- per-1Hz sample ----
static void altProcessSample(float dt, uint32_t nowMs) {
  float battV = getBatteryVoltage();
  float amps = isnan(MeasuredAmps) ? 0.0f : MeasuredAmps;
  float tF = TempToUse;
  float duty = dutyCycle;
  float rpm = RPM;
  float fi = altExcitation(duty, battV, tF);
  int ri = altRpmBin(rpm), fb = altFiBin(fi);

  // live point + prediction (drives the dashboard red dot / SSE) — computed in sim too
  altLive_rpm = rpm;
  altLive_fi = fi;
  altLive_amps = amps;
  altLiveValid = (ri >= 0 && fb >= 0 && !isnan(battV) && battV >= ALT_MIN_BATT_V);
  {
    float ap, drr, dff, rs, nd;
    int nu;
    if (altPredict(rpm, fi, &ap, &drr, &dff, &rs, &nu, &nd)) {
      float tc = altTempCorrAt(altTempBin(tF));
      float bc = altBusCorrAt(altVbusBin(battV));
      altLive_pred = ap * tc * bc;
      altLive_z = (altLive_pred > 0.5f) ? (amps - altLive_pred) / altSigma(ap, drr, dff, fi, rs, nd) : 0.0f;
    } else {
      altLive_pred = 0.0f;
      altLive_z = 0.0f;
    }
  }

  if (hardwarePresent != 1) {   // sim: display live only, never learn/score/persist
    altSteady = false;
    return;
  }

  bool admit = (!isnan(battV) && battV >= ALT_MIN_BATT_V &&
                amps >= altMinAmps && duty >= altMinDuty && ri >= 0 && fb >= 0);
  altSteady = admit && altSteadyUpdate(rpm, fi, battV, tF, dt, nowMs);

  int bin = (ri >= 0 && fb >= 0) ? ALT_IDX(ri, fb) : -1;
  if (bin != altCurBin) {       // left a bin → finalize its one visit-observation
    altFinalizeVisit();
    altCurBin = bin;
    altResetVisit();
  }
  if (altSteady && bin >= 0) {
    altVisit_w += 1.0;
    altVisit_A += amps;
    altVisit_A2 += (double)amps * amps;
    altVisit_rpm += rpm;
    altVisit_fi += fi;
    altVisit_vbus += battV;
    altVisit_t += tF;
    altVisitSteadyMs += (uint32_t)(dt * 1000.0f);
  }
}

// ---- live SSE: valid,rpm,fi,amps,pred,z,status,steady ----
static void altSendLive() {
  char buf[160];
  // valid,rpm,fi,amps,pred,z,status,steady,healthPct,coveragePct,baselineFrozen,obsCount
  snprintf(buf, sizeof(buf), "%d,%.0f,%.2f,%.1f,%.1f,%.2f,%d,%d,%.1f,%.0f,%d,%u",
           (int)altLiveValid, altLive_rpm, altLive_fi, altLive_amps, altLive_pred,
           altLive_z, (int)altHealth.status, (int)altSteady,
           altHealthPct(), altCoveragePct(), (int)altHealth.baselineFrozen,
           (unsigned)altHealth.obsCount);
  events.send(buf, "AltLive");
}

// ---- persistence (Phase-0 scaffold; field-off-gated by caller) ----
void altHealthSave() {
  if (!altBase || hardwarePresent != 1) return;
  writePsramBlob("/altbase.bin", ALT_BASE_MAGIC, ALT_VER, 0,
                 altBase, sizeof(AltCell), ALT_NUM_CELLS, 0, ALT_NUM_CELLS);
  writePsramBlob("/alttemp.bin", ALT_TEMPC_MAGIC, ALT_VER, 0,
                 altTempCorr, sizeof(AltCorr), ALT_TEMP_BINS, 0, ALT_TEMP_BINS);
  writePsramBlob("/altbus.bin", ALT_BUSC_MAGIC, ALT_VER, 0,
                 altBusCorr, sizeof(AltCorr), ALT_VBUS_BINS, 0, ALT_VBUS_BINS);
  writePsramBlob("/althealth.bin", ALT_HLTH_MAGIC, ALT_VER, 0,
                 &altHealth, sizeof(AltHealthMon), 1, 0, 1);
}
static void altRecomputeFrozen() {
  altHealth.baselineFrozen = 0;
  for (int i = 0; i < ALT_NUM_CELLS; i++) {
    if (altBase[i].ref_valid) { altHealth.baselineFrozen = 1; break; }
  }
}
static void altLoad() {
  uint32_t uw;
  readPsramBlob("/altbase.bin", ALT_BASE_MAGIC, ALT_VER, altBase, sizeof(AltCell), ALT_NUM_CELLS, &uw, false);
  readPsramBlob("/alttemp.bin", ALT_TEMPC_MAGIC, ALT_VER, altTempCorr, sizeof(AltCorr), ALT_TEMP_BINS, &uw, false);
  readPsramBlob("/altbus.bin", ALT_BUSC_MAGIC, ALT_VER, altBusCorr, sizeof(AltCorr), ALT_VBUS_BINS, &uw, false);
  uint32_t got = readPsramBlob("/althealth.bin", ALT_HLTH_MAGIC, ALT_VER, &altHealth, sizeof(AltHealthMon), 1, &uw, false);
  if (got == 0) {
    altHealth = AltHealthMon{};
    altHealth.ewmaRatio = 1.0f;
  }
  altRecomputeFrozen();
}

// ---- lifecycle ----
void initAlternatorHealth() {
  altBase = (AltCell *)ps_malloc((size_t)ALT_NUM_CELLS * sizeof(AltCell));
  altTempCorr = (AltCorr *)ps_malloc((size_t)ALT_TEMP_BINS * sizeof(AltCorr));
  altBusCorr = (AltCorr *)ps_malloc((size_t)ALT_VBUS_BINS * sizeof(AltCorr));
  if (!altBase || !altTempCorr || !altBusCorr) {
    queueConsoleMessage("ERROR: AltHealth ps_malloc failed");
    return;
  }
  memset(altBase, 0, (size_t)ALT_NUM_CELLS * sizeof(AltCell));
  memset(altTempCorr, 0, (size_t)ALT_TEMP_BINS * sizeof(AltCorr));
  memset(altBusCorr, 0, (size_t)ALT_VBUS_BINS * sizeof(AltCorr));
  altHealth = AltHealthMon{};
  altHealth.ewmaRatio = 1.0f;
  altCurBin = -1;
  altResetVisit();
  altLoad();
  int frozen = 0, withData = 0;
  for (int i = 0; i < ALT_NUM_CELLS; i++) {
    if (altBase[i].ref_valid) frozen++;
    if (altBase[i].nObs) withData++;
  }
  queueConsoleMessageF("AltHealth init: %d cells, %d frozen, %d w/data, %.1fKB PSRAM",
                       ALT_NUM_CELLS, frozen, withData,
                       (float)((size_t)ALT_NUM_CELLS * sizeof(AltCell)) / 1024.0f);
}
void resetAlternatorHealth() {
  if (!altBase) return;
  memset(altBase, 0, (size_t)ALT_NUM_CELLS * sizeof(AltCell));
  memset(altTempCorr, 0, (size_t)ALT_TEMP_BINS * sizeof(AltCorr));
  memset(altBusCorr, 0, (size_t)ALT_VBUS_BINS * sizeof(AltCorr));
  altHealth = AltHealthMon{};
  altHealth.ewmaRatio = 1.0f;
  altCurBin = -1;
  altResetVisit();
  altTempSlow = NAN;
  altSS_rpmRef = 0; altSS_fiRef = 0; altSS_vbusRef = 0; altSS_tempRef = 0;  // clear steady-state anchors
  altSteadyStartMs = 0; altThermInBandMs = 0;
  fsTakeLock();
  LittleFS.remove("/altbase.bin");
  LittleFS.remove("/alttemp.bin");
  LittleFS.remove("/altbus.bin");
  LittleFS.remove("/althealth.bin");
  fsReleaseLock();
  queueConsoleMessage("AltHealth: full reset (Start Over)");
}
float altCoveragePct() {
  if (!altBase) return 0.0f;
  int frozen = 0, withData = 0;
  for (int i = 0; i < ALT_NUM_CELLS; i++) {
    if (altBase[i].nObs) withData++;
    if (altBase[i].ref_valid) frozen++;
  }
  if (withData == 0) return 0.0f;
  return 100.0f * (float)frozen / (float)withData;
}
float altHealthPct() {
  float p = altHealth.ewmaRatio * 100.0f;
  if (p < 0) p = 0;
  if (p > 200) p = 200;
  return p;
}

// ---- tick: ~1 Hz, call from loop() ----
void altHealth_tick(uint32_t nowMs) {
  static uint32_t lastMs = 0;
  if (!altBase) return;
  if (nowMs - lastMs < 1000) return;
  float dt = (lastMs == 0) ? 1.0f : (nowMs - lastMs) / 1000.0f;
  if (dt > 5.0f) dt = 5.0f;
  lastMs = nowMs;
  altProcessSample(dt, nowMs);
  altSendLive();
  static uint8_t settCtr = 0;          // resend settings ~every 5s so reconnects get echoes
  if (++settCtr >= 5) { settCtr = 0; sendAltSettings(); }
}


// ============================================================
// ALTERNATOR HEALTH — GUI-adjustable settings (registry-driven)
//   One float registry → one /get handler loop + one boot-load loop +
//   one "AltSettings" SSE echo. Avoids 16× fragile CSV3 plumbing.
// ============================================================
struct AltSetting { const char *name; float *ptr; };
static AltSetting ALT_SETTINGS[] = {
  {"altElecSettleSec", &altElecSettleSec}, {"altDutyTolPct", &altDutyTolPct},
  {"altRpmTol", &altRpmTol}, {"altVbusTol", &altVbusTol},
  {"altThermRateDegF", &altThermRateDegF}, {"altThermRateMin", &altThermRateMin},
  {"altThermDwellSec", &altThermDwellSec}, {"altMinDwellSec", &altMinDwellSec},
  {"altMinAmps", &altMinAmps}, {"altMinDuty", &altMinDuty},
  {"altFreezeMinVisits", &altFreezeMinVisits}, {"altFreezeMaxVisits", &altFreezeMaxVisits},
  {"altFreezeSEM", &altFreezeSEM},
  {"altEwmaLambda", &altEwmaLambda}, {"altCusumK", &altCusumK}, {"altCusumH", &altCusumH},
};
static const size_t ALT_SETTING_COUNT = sizeof(ALT_SETTINGS) / sizeof(ALT_SETTINGS[0]);

void altSettingsLoad() {
  for (size_t i = 0; i < ALT_SETTING_COUNT; i++) {
    char path[48];
    snprintf(path, sizeof(path), "/%s.txt", ALT_SETTINGS[i].name);
    if (!fsExists(path)) writeFile(LittleFS, path, String(*ALT_SETTINGS[i].ptr, 4).c_str());
    else *ALT_SETTINGS[i].ptr = readFile(LittleFS, path).toFloat();
  }
}
bool altSettingsHandle(AsyncWebServerRequest *request) {
  bool handled = false;
  for (size_t i = 0; i < ALT_SETTING_COUNT; i++) {
    if (request->hasParam(ALT_SETTINGS[i].name)) {
      *ALT_SETTINGS[i].ptr = request->getParam(ALT_SETTINGS[i].name)->value().toFloat();
      char path[48];
      snprintf(path, sizeof(path), "/%s.txt", ALT_SETTINGS[i].name);
      writeFile(LittleFS, path, String(*ALT_SETTINGS[i].ptr, 4).c_str());
      handled = true;
    }
  }
  return handled;
}
void sendAltSettings() {
  char buf[320];
  int off = 0;
  for (size_t i = 0; i < ALT_SETTING_COUNT; i++)
    off += snprintf(buf + off, sizeof(buf) - off, (i ? ",%.4f" : "%.4f"), *ALT_SETTINGS[i].ptr);
  events.send(buf, "AltSettings");
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
  if (!loggingActive) return;  // Stop Logs: skip append, freeze buffer
  if (!cvLogReady || !cvLog) return;
  // if (sysMode != SYS_MODE_AUTO) return;  // this was dumb, probably remove later

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
  e.battV = (int16_t)(IBV * 100.0f);
  e.targV = (int16_t)(ChargingVoltageTarget * 100.0f);
  e.vErrorMv = (int16_t)((ChargingVoltageTarget - IBV) * 1000.0f);
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
  // bit 3 reserved (was softClamp — old soft-cap removed)
  if (g_fastOvHardActive) e.flags |= (1 << 4);

  e.awState = g_awState;
  e.rpm = (int16_t)constrain((int)RPM, -32768, 32767);
  e.battV_filt_x100 = (int16_t)clamp_f(IBV_filtered * 100.0f, -32767.0f, 32767.0f);
  e.iMeas_filt_x10 = (int16_t)clamp_f(MeasuredAmps_filtered * 10.0f, -32767.0f, 32767.0f);
  e.cvDSlope_x10000 = (int16_t)clamp_f(cvDSlope * 10000.0f, -32767.0f, 32767.0f);
  e.ch1IntervalMs = (int16_t)g_ch1LastIntervalMs;
  e.battI_x10 = (int16_t)clamp_f(getBatteryCurrent() * 10.0f, -32767.0f, 32767.0f);
  e.dBcur_dt_Aps = (int16_t)clamp_f(g_dBcur_dt, -32767.0f, 32767.0f);
  e.voltLoopIntervalMs = pidLog_voltageLoopRanThisTick ? (int16_t)g_voltLoopActualIntervalMs : 0;
  e.inaIntervalMs = (int16_t)ina_last_ms;
  e.slopeBleedAmps_x1000 = (int16_t)clamp_f(g_slopeBleedAmpsThisTick * 1000.0f, 0.0f, 32767.0f);
  g_slopeBleedAmpsThisTick = 0.0f;  // clear after logging so non-VL ticks show 0

  if (g_iExcessActive)   e.flags |= (1 << 5);
  if (g_loadDumpActive)  e.flags |= (1 << 6);

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
    ch1Bkt1sHead = (ch1Bkt1sHead + 1) % CH1_1S_BUCKETS;
    if (ch1Bkt1sCount < CH1_1S_BUCKETS) ch1Bkt1sCount++;
    ch1Bkt1sCurrent = { 0, 0, 0, 0 };
    ch1Bkt1sStart = now;
  }

  // ── 10s→2m bucket rollover: O(10) mini-bucket sum, no ring scan ────────
  // over2x approximated using bucket mean * 2 threshold (acceptable for 2m diagnostic)
  if (now - ch1BktStart >= 10000UL) {
    Ch1Bucket bkt = { 0, 0, 0, 0 };

    // Sum all closed 1s mini-buckets
    for (uint8_t i = 0; i < ch1Bkt1sCount; i++) {
      uint8_t idx = (ch1Bkt1sHead + CH1_1S_BUCKETS - 1 - i) % CH1_1S_BUCKETS;
      bkt.sum += ch1Bkt1s[idx].sum;
      bkt.count += ch1Bkt1s[idx].count;
      if (ch1Bkt1s[idx].worst > bkt.worst) bkt.worst = ch1Bkt1s[idx].worst;
    }
    // Include currently open mini-bucket
    bkt.sum += ch1Bkt1sCurrent.sum;
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
  ch1_last_ms = ch1Ring[(ch1Head + CH1_RING - 1) % CH1_RING].iv;  // O(1) single element
  ch1_n_10s = ch1Bkt1sCurrent.count;                              // start with open bucket
  ch1_worst_10s = ch1Bkt1sCurrent.worst;
  ch1_over2x_10s = 0;  // not tracked at 1s granularity
  uint32_t sum10 = ch1Bkt1sCurrent.sum;

  for (uint8_t i = 0; i < ch1Bkt1sCount; i++) {
    uint8_t idx = (ch1Bkt1sHead + CH1_1S_BUCKETS - 1 - i) % CH1_1S_BUCKETS;
    sum10 += ch1Bkt1s[idx].sum;
    ch1_n_10s += ch1Bkt1s[idx].count;
    if (ch1Bkt1s[idx].worst > ch1_worst_10s) ch1_worst_10s = ch1Bkt1s[idx].worst;
  }
  if (ch1_n_10s > 0) ch1_avg_10s = (float)sum10 / (float)ch1_n_10s;
  else ch1_avg_10s = 0.0f;

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

// ─────────────────────────────────────────────────────────────────────────────
// INA228 fast-mode interval tracking
// Mirrors CH1 interval stats. Only updated when inaFastModeActive.
// resetINA228IntervalWindows() clears 10s/2m windows; all-time persists.
// ─────────────────────────────────────────────────────────────────────────────
struct InaMiniB  { uint32_t sum; uint32_t count; uint16_t worst; };
struct InaBucket { uint32_t sum; uint32_t count; uint16_t worst; uint16_t over2x; };

#define INA_1S_BUCKETS 11
#define INA_BUCKETS    12

static InaMiniB  ina1sB[INA_1S_BUCKETS];
static uint8_t   ina1sHead  = 0;
static uint8_t   ina1sCount = 0;
static InaMiniB  ina1sCur   = {0, 0, 0};
static uint32_t  ina1sStart = 0;

static InaBucket ina2mB[INA_BUCKETS];
static uint8_t   ina2mHead  = 0;
static uint8_t   ina2mCount = 0;
static uint32_t  ina2mStart = 0;

static uint64_t  inaAtSum   = 0;
static uint32_t  inaAtCount = 0;
// inaAtWorst removed — write directly to the public ina_worst_at instead.
// Live dashboard was showing ina_worst_at=0 while ina_over2x_at=420 and
// ina_avg_at=5.19 — logically impossible if both updates run through the
// same code path. Cold reading of the function shows no obvious cause,
// so the intermediate is eliminated and the published variable becomes
// the single source of truth. Same treatment applied to ina_worst_2m.
static uint32_t  inaAtOver2x = 0;
static uint32_t  inaPrevRead = 0;

void resetINA228IntervalWindows() {
  memset(ina1sB, 0, sizeof(ina1sB));
  ina1sHead = 0; ina1sCount = 0;
  ina1sCur  = {0, 0, 0};
  ina1sStart = millis();
  memset(ina2mB, 0, sizeof(ina2mB));
  ina2mHead = 0; ina2mCount = 0;
  ina2mStart = millis();
  inaPrevRead = 0;
  ina_last_ms = 0;
  ina_avg_10s = 0.0f; ina_worst_10s = 0; ina_over2x_10s = 0;
  ina_avg_2m  = 0.0f; ina_worst_2m  = 0; ina_over2x_2m  = 0;
}

// Full reset including all-time accumulators — called by the web-side
// Reset Peaks button. resetINA228IntervalWindows() above only clears the
// 10s/2m windows; this also wipes ina_worst_at / ina_avg_at / ina_over2x_at
// and the static counters that feed them.
void resetINA228AllStats() {
  resetINA228IntervalWindows();
  inaAtSum     = 0;
  inaAtCount   = 0;
  inaAtOver2x  = 0;
  ina_avg_at   = 0.0f;
  ina_worst_at = 0;
  ina_over2x_at = 0;
}

void recordINA228Interval(uint32_t now) {
  if (inaPrevRead == 0) { inaPrevRead = now; return; }

  uint32_t diff = now - inaPrevRead;
  inaPrevRead = now;
  uint16_t iv = (diff > 65535u) ? 65535u : (uint16_t)diff;
  ina_last_ms = iv;

  // All-time accumulators (avg + over2x via running mean as before).
  // Worst is written DIRECTLY to the published variable — no intermediate.
  inaAtCount++;
  inaAtSum += iv;
  if (iv > ina_worst_at) ina_worst_at = iv;
  // Also write the "2m" worst directly. With this in place ina_worst_2m
  // becomes "max iv since last fast-mode rising edge" rather than a strict
  // 2m rolling window. The bucket-based avg + over2x for 2m still work
  // and remain rolling. Tooltips should say "since fast-mode start" for these.
  if (iv > ina_worst_2m) ina_worst_2m = iv;
  if (inaAtCount > 1) {
    // Bias correction: compute mean of prior samples only, otherwise a huge
    // outlier inflates its own mean and fails the > 2× test against itself.
    float runMean = (float)((double)(inaAtSum - iv) / (inaAtCount - 1));
    if ((float)iv > runMean * 2.0f) inaAtOver2x++;
  }

  // 1s mini-bucket
  ina1sCur.sum += iv;
  ina1sCur.count++;
  if (iv > ina1sCur.worst) ina1sCur.worst = iv;

  if (now - ina1sStart >= 1000UL) {
    ina1sB[ina1sHead] = ina1sCur;
    ina1sHead = (ina1sHead + 1) % INA_1S_BUCKETS;
    if (ina1sCount < INA_1S_BUCKETS) ina1sCount++;
    ina1sCur  = {0, 0, 0};
    ina1sStart = now;
  }

  // 10s→2m bucket rollover
  if (now - ina2mStart >= 10000UL) {
    InaBucket bkt = {0, 0, 0, 0};
    for (uint8_t i = 0; i < ina1sCount; i++) {
      uint8_t idx = (ina1sHead + INA_1S_BUCKETS - 1 - i) % INA_1S_BUCKETS;
      bkt.sum   += ina1sB[idx].sum;
      bkt.count += ina1sB[idx].count;
      if (ina1sB[idx].worst > bkt.worst) bkt.worst = ina1sB[idx].worst;
    }
    bkt.sum   += ina1sCur.sum;
    bkt.count += ina1sCur.count;
    if (ina1sCur.worst > bkt.worst) bkt.worst = ina1sCur.worst;
    if (bkt.count > 0) {
      float thresh = ((float)bkt.sum / (float)bkt.count) * 2.0f;
      for (uint8_t i = 0; i < ina1sCount; i++) {
        uint8_t idx = (ina1sHead + INA_1S_BUCKETS - 1 - i) % INA_1S_BUCKETS;
        if ((float)ina1sB[idx].worst > thresh) bkt.over2x++;
      }
    }
    ina2mB[ina2mHead] = bkt;
    ina2mHead = (ina2mHead + 1) % INA_BUCKETS;
    if (ina2mCount < INA_BUCKETS) ina2mCount++;
    ina2mStart = now;
  }

  // Publish 10s stats
  uint32_t sum10 = ina1sCur.sum, n10 = ina1sCur.count;
  ina_worst_10s = ina1sCur.worst;
  for (uint8_t i = 0; i < ina1sCount; i++) {
    uint8_t idx = (ina1sHead + INA_1S_BUCKETS - 1 - i) % INA_1S_BUCKETS;
    sum10 += ina1sB[idx].sum;
    n10   += ina1sB[idx].count;
    if (ina1sB[idx].worst > ina_worst_10s) ina_worst_10s = ina1sB[idx].worst;
  }
  ina_avg_10s    = (n10 > 0) ? (float)sum10 / (float)n10 : 0.0f;
  ina_over2x_10s = 0;  // not tracked at 1s granularity

  // Publish 2m stats. ina_worst_2m is now updated DIRECTLY on every sample
  // (above), so this block does NOT touch it — only avg + over2x come from
  // the bucket ring.
  uint32_t n2m = 0;
  uint64_t sum2m = 0;
  ina_over2x_2m  = 0;
  for (uint8_t i = 0; i < ina2mCount; i++) {
    uint8_t idx = (ina2mHead + INA_BUCKETS - 1 - i) % INA_BUCKETS;
    n2m          += ina2mB[idx].count;
    sum2m        += ina2mB[idx].sum;
    ina_over2x_2m += ina2mB[idx].over2x;
  }
  ina_avg_2m = (n2m > 0) ? (float)sum2m / (float)n2m : 0.0f;

  // Publish all-time stats. ina_worst_at is also updated directly above,
  // so the publish only handles avg + over2x.
  ina_avg_at    = (inaAtCount > 0) ? (float)((double)inaAtSum / inaAtCount) : 0.0f;
  ina_over2x_at = inaAtOver2x;
}

void cacheGzFiles() {
  cachedIndex = loadFileToRAM("/index.html.gz");
  cachedCss = loadFileToRAM("/styles.css.gz");
  cachedJs = loadFileToRAM("/script.js.gz");
  cachedUplotCss = loadFileToRAM("/uPlot.min.css.gz");
  cachedUplotJs = loadFileToRAM("/uPlot.iife.min.js.gz");
}
bool serveCachedGz(AsyncWebServerRequest *request, const String &path, const String &contentType) {
  CachedGzFile *cf = nullptr;
  if (path == "/index.html") cf = &cachedIndex;
  else if (path == "/styles.css") cf = &cachedCss;
  else if (path == "/script.js") cf = &cachedJs;
  else if (path == "/uPlot.min.css") cf = &cachedUplotCss;
  else if (path == "/uPlot.iife.min.js") cf = &cachedUplotJs;

  if (cf && cf->data && cf->size > 0) {
    uint8_t *data = cf->data;
    size_t len = cf->size;
    AsyncWebServerResponse *resp = request->beginResponse(
      contentType, len,
      [data, len](uint8_t *buffer, size_t maxLen, size_t index) -> size_t {
        size_t remaining = len - index;
        size_t toSend = min(maxLen, remaining);
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
// ============================================================
// systemID_tick() — plant delay measurement step test
//
// Architecture:
//  • 8-phase state machine: BASELINE + 3× (UP / DOWN).
//  • Each phase holds for 15 × InputFilterTC ms (floor 5000 ms).
//  • Called every CH1 fresh hit from AdjustFieldLearnMode.
//  • Returns true while active; caller must:
//      – force govMode = GOV_BYPASS_SLEW
//      – set PID to MANUAL and reset integrator to dutyOut
//      – use dutyOut as the duty command
//  • On completion, post-processes buffer in-place (O(N) scan)
//    and populates systemIDRise/FallDelay_ms[] and averages.
//  • systemIDResultsReady → true signals UI to show popup.
//  • Buffer (PSRAM) allocated once on first run, never freed.
//
// Detection thresholds:
//  Rising : current must exceed quietMax × 1.2 (20% above prior-phase max)
//  Falling: current must drop below upMin − 0.3 × stepAmp (30% of step below high-phase min)
//           stepAmp = upMin − quietMax, making fall threshold step-relative not absolute-percentage.
//           Scan bounded by t_down_end so it cannot bleed into later phases.
//  A delay of -1 ms means the threshold crossing was not found in the buffer.
// ============================================================

bool systemID_tick(float &dutyOut, float ampsRaw, uint32_t nowMs) {
  // Phase enum. Values 1–9 map to phaseStartMs indices 0–8 via (phase – 1).
  // STABILIZE(1) runs first and is handled with its own early-return block.
  enum SysIDPhase : uint8_t {
    SYSID_IDLE        = 0,
    SYSID_STABILIZE   = 1,
    SYSID_BASELINE    = 2,
    SYSID_UP_1        = 3,
    SYSID_DOWN_1      = 4,
    SYSID_UP_2        = 5,
    SYSID_DOWN_2      = 6,
    SYSID_UP_3        = 7,
    SYSID_DOWN_3      = 8,
    SYSID_PROCESSING  = 9
  };

  static SysIDPhase phase = SYSID_IDLE;
  static float baseDuty = 0.0f;
  static uint32_t holdMs = 0;
  static bool bufFullWarned = false;
  static uint32_t stabilizeLastAdjMs = 0;   // last 1Hz duty adjustment in STABILIZE
  // Rolling 5-sample ring buffer for average-based settle check (1Hz sampling)
  static float stabRing[SYSID_STABILIZE_SAMPLES];
  static uint8_t stabRingIdx = 0;
  static uint8_t stabRingCount = 0;

  // phaseStartMs[0..8]: STABILIZE[0] BASELINE[1] UP_1[2] DOWN_1[3]
  //                     UP_2[4] DOWN_2[5] UP_3[6] DOWN_3[7] test-end[8]
  static uint32_t phaseStartMs[9] = { 0 };

  // One-shot debug on request arrival
  static bool lastReqState = false;
  if (systemIDRequested && !lastReqState) {
    Serial.printf("SystemID: REQUEST SEEN | phase=%d sysMode=%d lastAppliedDuty=%.1f\n",
                  phase, sysMode, lastAppliedDuty);
  }
  lastReqState = systemIDRequested;

  // ── Ignore re-triggers while a test is already running ──────────────────
  if (phase != SYSID_IDLE && systemIDRequested) {
    systemIDRequested = false;
    queueConsoleMessage("SystemID: re-trigger ignored — test already in progress");
  }

  // ── Abort check ─────────────────────────────────────────────────────────
  if (phase != SYSID_IDLE && systemIDAbortRequested) {
    systemIDAbortRequested = false;
    queueConsoleMessage("SystemID: test aborted");
    commitSystemIDRecord(true);  // log aborted run to ring buffer for fleet visibility
    systemIDActive = 0;
    phase = SYSID_IDLE;
    stabilizeLastAdjMs = 0;
    stabRingIdx = 0;
    stabRingCount = 0;
    dutyOut = lastAppliedDuty;
    return false;
  }

  // ── IDLE: wait for trigger ───────────────────────────────────────────────
  if (phase == SYSID_IDLE) {
    if (!systemIDRequested) {
      dutyOut = lastAppliedDuty;
      return false;
    }
    systemIDRequested = false;
    systemIDAbortRequested = false;   // clear any stale abort that arrived after previous test ended

    Serial.printf("SystemID: starting | sysMode=%d lastAppliedDuty=%.1f\n",
                  sysMode, lastAppliedDuty);

    // Allocate PSRAM buffer on first use
    if (sysIDBuffer == nullptr) {
      sysIDBuffer = (SystemIDSample *)ps_malloc(SYSID_BUF_SIZE * sizeof(SystemIDSample));
      if (sysIDBuffer == nullptr) {
        Serial.println("SystemID: ABORTED — PSRAM alloc failed");
        queueConsoleMessage("SystemID: ABORTED — PSRAM alloc failed");
        dutyOut = lastAppliedDuty;
        return false;
      }
      Serial.printf("SystemID: PSRAM alloc OK — %d bytes\n",
                    SYSID_BUF_SIZE * (int)sizeof(SystemIDSample));
    }

    // Initialise test state
    memset(phaseStartMs, 0, sizeof(phaseStartMs));
    sysIDSampleCount = 0;
    bufFullWarned = false;
    stabilizeLastAdjMs = 0;
    stabRingIdx = 0;
    stabRingCount = 0;
    systemIDResultsReady = false;
    baseDuty = lastAppliedDuty;
    holdMs = (uint32_t)(15.0f * InputFilterTC);
    if (holdMs < 5000) holdMs = 5000;  // minimum 5 seconds per phase regardless of TC

    queueConsoleMessageF(
      "SystemID: stabilizing to %.0fA | step=+%.1f%% holdMs=%u TC=%.0fms",
      SYSID_STABILIZE_AMPS, SystemIDStepAmplitude, holdMs, InputFilterTC);

    phaseStartMs[0] = nowMs;  // STABILIZE start
    phase = SYSID_STABILIZE;
    systemIDActive = (uint8_t)SYSID_STABILIZE;
  }

  // ── STABILIZE phase: P-control to SYSID_STABILIZE_AMPS before baseline ──
  // Adjust duty once per second. Once the 5-second rolling average is within
  // ±3A of the target, advance. Abort if timeout exceeded.
  if (phase == SYSID_STABILIZE) {
    if (nowMs - stabilizeLastAdjMs >= 1000) {
      float err = SYSID_STABILIZE_AMPS - ampsRaw;
      baseDuty = constrain(baseDuty + err * 0.5f, 5.0f, 80.0f);
      // Push ampsRaw into the ring buffer on each 1Hz duty update
      stabRing[stabRingIdx] = ampsRaw;
      stabRingIdx = (stabRingIdx + 1) % SYSID_STABILIZE_SAMPLES;
      if (stabRingCount < SYSID_STABILIZE_SAMPLES) stabRingCount++;
      stabilizeLastAdjMs = nowMs;

      // Once we have a full 5-second window, check if the average is within band
      if (stabRingCount >= SYSID_STABILIZE_SAMPLES) {
        float sum = 0;
        for (uint8_t i = 0; i < SYSID_STABILIZE_SAMPLES; i++) sum += stabRing[i];
        float avg = sum / SYSID_STABILIZE_SAMPLES;
        if (fabsf(avg - SYSID_STABILIZE_AMPS) < SYSID_STABILIZE_BAND_A) {
          stabRingIdx = 0;
          stabRingCount = 0;
          stabilizeLastAdjMs = 0;
          phaseStartMs[1] = nowMs;  // BASELINE start
          phase = SYSID_BASELINE;
          systemIDActive = (uint8_t)SYSID_BASELINE;
          queueConsoleMessageF(
            "SystemID: 5s avg=%.1fA (duty=%.1f%%) within %.0fA of target — starting baseline | holdMs=%u",
            avg, baseDuty, SYSID_STABILIZE_BAND_A, holdMs);
          Serial.printf("SystemID: BASELINE\n");
        }
      }
    }
    dutyOut = baseDuty;

    if ((nowMs - phaseStartMs[0]) >= SYSID_STABILIZE_TIMEOUT_MS) {
      stabilizeLastAdjMs = 0;
      stabRingIdx = 0;
      stabRingCount = 0;
      queueConsoleMessageF(
        "SystemID: ABORTED — could not stabilize at %.0fA within %us "
        "(last reading: %.1fA duty=%.1f%%)",
        SYSID_STABILIZE_AMPS, SYSID_STABILIZE_TIMEOUT_MS / 1000,
        ampsRaw, baseDuty);
      systemIDAbortReason = 254;             // sentinel: stabilize-phase timeout (outside FieldEventReason enum)
      systemIDAbortPhase  = systemIDActive;  // current phase before we clear it
      commitSystemIDRecord(true);            // log the timeout-abort to ring buffer
      systemIDActive = 0;
      phase = SYSID_IDLE;
      dutyOut = baseDuty;
      return false;
    }

    return true;
  }

  // ── Determine commanded duty for current phase ───────────────────────────
  // UP phases command baseDuty + amplitude; all others hold baseDuty.
  bool isUpPhase = (phase == SYSID_UP_1 || phase == SYSID_UP_2 || phase == SYSID_UP_3);
  float phaseDuty = isUpPhase
                      ? constrain(baseDuty + SystemIDStepAmplitude, 0.0f, 100.0f)
                      : baseDuty;
  dutyOut = phaseDuty;

  // ── Record sample ────────────────────────────────────────────────────────
  if (sysIDSampleCount < SYSID_BUF_SIZE) {
    sysIDBuffer[sysIDSampleCount++] = { nowMs, phaseDuty, ampsRaw };
  } else {
    if (!bufFullWarned) {
      bufFullWarned = true;
      queueConsoleMessageF("SystemID: WARNING — sample buffer full at %d samples. "
                           "Post-processing will use truncated data. "
                           "Increase SYSID_BUF_SIZE or reduce call rate.",
                           SYSID_BUF_SIZE);
      Serial.printf("SystemID: buffer full at %d samples\n", SYSID_BUF_SIZE);
    }
  }

  // ── Phase advance ────────────────────────────────────────────────────────
  // phase enum starts at 1, phaseStartMs index = phase - 1.
  uint32_t elapsed = nowMs - phaseStartMs[phase - 1];
  if (elapsed >= holdMs) {
    switch (phase) {
      case SYSID_BASELINE:
        phaseStartMs[2] = nowMs;
        phase = SYSID_UP_1;
        systemIDActive = (uint8_t)SYSID_UP_1;
        queueConsoleMessageF("SystemID: BASELINE complete (%ums) — entering UP 1 | duty=%.1f%% amps=%.1fA",
                             elapsed, phaseDuty + SystemIDStepAmplitude, ampsRaw);
        Serial.printf("SystemID: UP 1\n");
        break;
      case SYSID_UP_1:
        phaseStartMs[3] = nowMs;
        phase = SYSID_DOWN_1;
        systemIDActive = (uint8_t)SYSID_DOWN_1;
        queueConsoleMessageF("SystemID: UP 1 complete (%ums) — entering DOWN 1 | amps=%.1fA",
                             elapsed, ampsRaw);
        Serial.printf("SystemID: DOWN 1\n");
        break;
      case SYSID_DOWN_1:
        phaseStartMs[4] = nowMs;
        phase = SYSID_UP_2;
        systemIDActive = (uint8_t)SYSID_UP_2;
        queueConsoleMessageF("SystemID: DOWN 1 complete (%ums) — entering UP 2 | amps=%.1fA",
                             elapsed, ampsRaw);
        Serial.printf("SystemID: UP 2\n");
        break;
      case SYSID_UP_2:
        phaseStartMs[5] = nowMs;
        phase = SYSID_DOWN_2;
        systemIDActive = (uint8_t)SYSID_DOWN_2;
        queueConsoleMessageF("SystemID: UP 2 complete (%ums) — entering DOWN 2 | amps=%.1fA",
                             elapsed, ampsRaw);
        Serial.printf("SystemID: DOWN 2\n");
        break;
      case SYSID_DOWN_2:
        phaseStartMs[6] = nowMs;
        phase = SYSID_UP_3;
        systemIDActive = (uint8_t)SYSID_UP_3;
        queueConsoleMessageF("SystemID: DOWN 2 complete (%ums) — entering UP 3 | amps=%.1fA",
                             elapsed, ampsRaw);
        Serial.printf("SystemID: UP 3\n");
        break;
      case SYSID_UP_3:
        phaseStartMs[7] = nowMs;
        phase = SYSID_DOWN_3;
        systemIDActive = (uint8_t)SYSID_DOWN_3;
        queueConsoleMessageF("SystemID: UP 3 complete (%ums) — entering DOWN 3 | amps=%.1fA",
                             elapsed, ampsRaw);
        Serial.printf("SystemID: DOWN 3\n");
        break;
      case SYSID_DOWN_3:
        phaseStartMs[8] = nowMs;  // test end timestamp
        phase = SYSID_PROCESSING;
        systemIDActive = (uint8_t)SYSID_PROCESSING;
        queueConsoleMessageF("SystemID: DOWN 3 complete (%ums) — %d samples collected, post-processing",
                             elapsed, sysIDSampleCount);
        Serial.println("SystemID: data collection complete — post-processing");
        break;
      default:
        break;
    }
  }

  // ── Post-processing (runs immediately when PROCESSING is entered) ────────
  // phaseStartMs layout (with STABILIZE prefix):
  //   [0] STABILIZE start  [1] BASELINE start  [2] UP_1 start   [3] DOWN_1 start
  //   [4] UP_2 start       [5] DOWN_2 start    [6] UP_3 start   [7] DOWN_3 start
  //   [8] test end (DOWN_3 end)
  //
  // Preceding quiet phase for each rise: BASELINE[1], DOWN_1[3], DOWN_2[5]
  // UP phase starts:                     UP_1[2],     UP_2[4],   UP_3[6]
  // UP phase ends (= next phase start):  DOWN_1[3],   DOWN_2[5], DOWN_3[7]
  //
  // Preceding UP phase for each fall:    UP_1[2],  UP_2[4],  UP_3[6]
  // DOWN phase starts:                   DOWN_1[3],DOWN_2[5],DOWN_3[7]

  if (phase == SYSID_PROCESSING) {

    const uint8_t quietIdx[3] = { 1, 3, 5 };  // phaseStartMs index of pre-rise quiet phase
    const uint8_t upIdx[3] = { 2, 4, 6 };     // phaseStartMs index of UP phase start
    const uint8_t upEndIdx[3] = { 3, 5, 7 };  // phaseStartMs index of UP phase end
    const uint8_t downIdx[3] = { 3, 5, 7 };   // phaseStartMs index of DOWN phase start

    const uint32_t REF_WINDOW_MS = 2000;  // last 2 seconds of each phase used as reference

    // Reference statistics saved between rise and fall loops.
    // Mean anchors the threshold to a stable baseline; max/min measures the local noise
    // half-amplitude. Threshold = mean ± 2× noise_half_amplitude, so the detection point
    // stays just above the noise floor — close to the true transport delay — while a single
    // spike can no longer move the threshold the way raw max/min could.
    float quietMeanArr[3] = { 0.0f, 0.0f, 0.0f };
    float quietMaxArr[3]  = { 0.0f, 0.0f, 0.0f };
    float upMeanArr[3]    = { 0.0f, 0.0f, 0.0f };
    float upMinArr[3]     = { 0.0f, 0.0f, 0.0f };

    // ── Rise delays ─────────────────────────────────────────────────────
    for (int i = 0; i < 3; i++) {
      uint32_t t_quiet_start = phaseStartMs[quietIdx[i]];
      uint32_t t_up_start    = phaseStartMs[upIdx[i]];
      uint32_t t_up_end      = phaseStartMs[upEndIdx[i]];

      // Mean + max of the last 2 seconds of the quiet phase.
      // Mean = stable baseline; max - mean = noise half-amplitude.
      uint32_t quietRefStart = (t_up_start > REF_WINDOW_MS)
                                 ? (t_up_start - REF_WINDOW_MS)
                                 : t_quiet_start;
      float quietSum = 0.0f;
      float quietMax = -1.0e9f;
      float quietMin =  1.0e9f;
      int quietSamples = 0;
      for (int s = 0; s < sysIDSampleCount; s++) {
        if (sysIDBuffer[s].ts < quietRefStart) continue;
        if (sysIDBuffer[s].ts >= t_up_start) break;
        quietSum += sysIDBuffer[s].amps;
        if (sysIDBuffer[s].amps > quietMax) quietMax = sysIDBuffer[s].amps;
        if (sysIDBuffer[s].amps < quietMin) quietMin = sysIDBuffer[s].amps;
        quietSamples++;
      }

      if (quietSamples == 0) {
        queueConsoleMessageF("SystemID rise %d: WARNING — no samples in 2s quiet ref window "
                             "(quietRefStart=%u t_up_start=%u). threshold unreliable.",
                             i + 1, quietRefStart, t_up_start);
        Serial.printf("SystemID rise %d: no samples in quiet ref window\n", i + 1);
      }
      float quietMean = (quietSamples > 0) ? (quietSum / quietSamples) : 0.0f;
      quietMeanArr[i] = quietMean;
      quietMaxArr[i]  = quietMax;

      // Mean + min of the last 2 seconds of the UP phase.
      // Mean = stable settled high level; mean - min = noise half-amplitude (downward).
      uint32_t upRefStart = (t_up_end > REF_WINDOW_MS)
                              ? (t_up_end - REF_WINDOW_MS)
                              : t_up_start;
      float upSum = 0.0f;
      float upMin = 1.0e9f;
      int upSamples = 0;
      for (int s = 0; s < sysIDSampleCount; s++) {
        if (sysIDBuffer[s].ts < upRefStart) continue;
        if (sysIDBuffer[s].ts >= t_up_end) break;
        upSum += sysIDBuffer[s].amps;
        if (sysIDBuffer[s].amps < upMin) upMin = sysIDBuffer[s].amps;
        upSamples++;
      }

      float upMean = (upSamples > 0) ? (upSum / upSamples) : quietMean;
      upMeanArr[i] = upMean;
      upMinArr[i]  = upMin;

      // Signal quality metrics sent to UI for noise / amplitude quality check.
      systemIDStepAmp_A[i] = fmaxf(0.0f, upMean - quietMean);
      systemIDQuietPP_A[i] = (quietSamples > 0) ? fmaxf(0.0f, quietMax - quietMin) : 0.0f;

      // Rise threshold: quiet mean + 2× noise half-amplitude (floored at 0.5A).
      // This sits just above the noise ceiling while the mean anchor keeps it stable
      // across trials — matching the earliest reliable detection of field response.
      float quietNoise = fmaxf(quietMax - quietMean, 0.5f);
      float riseThresh = quietMean + 2.0f * quietNoise;

      // First sample after UP command that crosses above threshold.
      systemIDRiseDelay_ms[i] = -1.0f;
      for (int s = 0; s < sysIDSampleCount; s++) {
        if (sysIDBuffer[s].ts < t_up_start) continue;
        if (sysIDBuffer[s].ts >= t_up_end) break;
        if (sysIDBuffer[s].amps > riseThresh) {
          systemIDRiseDelay_ms[i] = (float)(sysIDBuffer[s].ts - t_up_start);
          break;
        }
      }

      if (systemIDRiseDelay_ms[i] < 0.0f) {
        queueConsoleMessageF("SystemID rise %d: NOT FOUND | quietMean=%.1fA quietMax=%.1fA noise=%.2fA thresh=%.1fA — "
                             "current never crossed threshold during UP phase. "
                             "Step amplitude may be too small or sensor not responding.",
                             i + 1, quietMean, quietMax, quietNoise, riseThresh);
      } else {
        queueConsoleMessageF("SystemID rise %d | quietMean=%.1fA quietMax=%.1fA noise=%.2fA thresh=%.1fA delay=%.0f ms",
                             i + 1, quietMean, quietMax, quietNoise, riseThresh, systemIDRiseDelay_ms[i]);
      }
      Serial.printf("SystemID rise %d | quietMean=%.1fA quietMax=%.1fA noise=%.2fA thresh=%.1fA delay=%.0f ms\n",
                    i + 1, quietMean, quietMax, quietNoise, riseThresh, systemIDRiseDelay_ms[i]);
    }

    // ── Fall delays ─────────────────────────────────────────────────────
    for (int i = 0; i < 3; i++) {
      uint32_t t_down_start = phaseStartMs[downIdx[i]];
      uint32_t t_down_end   = phaseStartMs[downIdx[i] + 1];  // bounded: [4],[6],[8]=test-end

      // Reuse statistics computed in the rise loop — same reference windows.
      float upMean = upMeanArr[i];
      float upMin  = upMinArr[i];

      // Fall threshold: UP mean − 2× noise half-amplitude (floored at 0.5A).
      // Symmetric to the rise: sits just below the noise floor of the settled high level.
      float upNoise  = fmaxf(upMean - upMin, 0.5f);
      float fallThresh = upMean - 2.0f * upNoise;

      // First sample after DOWN command that crosses below threshold.
      // Scan bounded by t_down_end so it cannot bleed into the next UP phase or beyond.
      systemIDFallDelay_ms[i] = -1.0f;
      for (int s = 0; s < sysIDSampleCount; s++) {
        if (sysIDBuffer[s].ts < t_down_start) continue;
        if (sysIDBuffer[s].ts >= t_down_end) break;
        if (sysIDBuffer[s].amps < fallThresh) {
          systemIDFallDelay_ms[i] = (float)(sysIDBuffer[s].ts - t_down_start);
          break;
        }
      }

      if (systemIDFallDelay_ms[i] < 0.0f) {
        queueConsoleMessageF("SystemID fall %d: NOT FOUND | upMean=%.1fA upMin=%.1fA noise=%.2fA thresh=%.1fA — "
                             "current never dropped below threshold in DOWN window. "
                             "Step amplitude may be too small or sensor not responding.",
                             i + 1, upMean, upMin, upNoise, fallThresh);
      } else {
        queueConsoleMessageF("SystemID fall %d | upMean=%.1fA upMin=%.1fA noise=%.2fA thresh=%.1fA delay=%.0f ms",
                             i + 1, upMean, upMin, upNoise, fallThresh, systemIDFallDelay_ms[i]);
      }
      Serial.printf("SystemID fall %d | upMean=%.1fA upMin=%.1fA noise=%.2fA thresh=%.1fA delay=%.0f ms\n",
                    i + 1, upMean, upMin, upNoise, fallThresh, systemIDFallDelay_ms[i]);
    }

    // ── Averages (skip -1 not-found entries) ────────────────────────────
    float riseSum = 0.0f;
    int riseN = 0;
    float fallSum = 0.0f;
    int fallN = 0;
    for (int i = 0; i < 3; i++) {
      if (systemIDRiseDelay_ms[i] >= 0.0f) {
        riseSum += systemIDRiseDelay_ms[i];
        riseN++;
      }
      if (systemIDFallDelay_ms[i] >= 0.0f) {
        fallSum += systemIDFallDelay_ms[i];
        fallN++;
      }
    }
    systemIDRiseAvg_ms = (riseN > 0) ? riseSum / (float)riseN : -1.0f;
    systemIDFallAvg_ms = (fallN > 0) ? fallSum / (float)fallN : -1.0f;

    // ── Summarise outcome ────────────────────────────────────────────────
    if (riseN == 0 && fallN == 0) {
      queueConsoleMessage("SystemID: FAILED — no threshold crossings detected for rise or fall. "
                          "Check ampsRaw signal, SystemIDStepAmplitude, and sensor wiring.");
    } else if (riseN == 0) {
      queueConsoleMessageF("SystemID: WARNING — rise crossings not found (%d/3). "
                           "Fall avg=%.0f ms. Results partial.",
                           3 - riseN, systemIDFallAvg_ms);
    } else if (fallN == 0) {
      queueConsoleMessageF("SystemID: WARNING — fall crossings not found (%d/3). "
                           "Rise avg=%.0f ms. Results partial.",
                           3 - fallN, systemIDRiseAvg_ms);
    }

    queueConsoleMessageF(
      "SystemID results | Rise: %.0f %.0f %.0f avg=%.0f ms | Fall: %.0f %.0f %.0f avg=%.0f ms | samples=%d",
      systemIDRiseDelay_ms[0], systemIDRiseDelay_ms[1], systemIDRiseDelay_ms[2], systemIDRiseAvg_ms,
      systemIDFallDelay_ms[0], systemIDFallDelay_ms[1], systemIDFallDelay_ms[2], systemIDFallAvg_ms,
      sysIDSampleCount);
    Serial.printf(
      "SystemID results | Rise: %.0f %.0f %.0f avg=%.0f ms | Fall: %.0f %.0f %.0f avg=%.0f ms | samples=%d\n",
      systemIDRiseDelay_ms[0], systemIDRiseDelay_ms[1], systemIDRiseDelay_ms[2], systemIDRiseAvg_ms,
      systemIDFallDelay_ms[0], systemIDFallDelay_ms[1], systemIDFallDelay_ms[2], systemIDFallAvg_ms,
      sysIDSampleCount);

    commitSystemIDRecord(false);  // log successful run to ring buffer
    systemIDResultsReady = true;
    systemIDActive = 0;
    systemIDLastEndMs = millis();
    phase = SYSID_IDLE;  // reset for next run
    dutyOut = baseDuty;  // restore base duty on exit tick
    return false;
  }

  return true;  // test still in progress
}