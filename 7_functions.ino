
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
// ALTERNATOR HEALTH — Best-Ever Front (device side). Folds one sample per control tick (~200 Hz,
//   via the pidLog hook) into the Episode detector; an emitted steady-run average pushes the sparse
//   best-ever output-amps front (FrontStore). The cloud prunes dominated points + retains raw
//   history. Surface axes {RPM, excitation, Vbus, temp}; excitation derived from run-average
//   duty/Vbus/temp. Generic engine + globals: Xregulator.ino. Design:
//   BEST_EVER_FRONT_SPEC.md / BEST_EVER_IMPLEMENTATION_PLAN.md.
// ============================================================

#define ALT_VER          4u
#define ALT_FRONT_MAGIC  0x414C4652u  // 'ALFR' — best-ever front point blob
#define ALT_TREND_MAGIC  0x414C5452u  // 'ALTR' (legacy whole-blob trend; superseded by the append log)
#define ALT_TRENDLOG_MAGIC 0x414C544Cu // 'ALTL' — append-only engine-hour trend log

// Temp-normalized field drive ("excitation proxy"): (dutyFrac × Vbus) / (1 + α(Tc − Tref)).
static inline float altExcitation(float duty, float vbus, float tF) {
  float tc = (tF - 32.0f) / 1.8f;
  float denom = 1.0f + ALT_ALPHA_PER_C * (tc - ALT_TREF_C);
  if (denom < 0.5f) denom = 0.5f;
  return (duty / 100.0f) * vbus / denom;
}

// ---- Best-Ever Front engine instance (alternator 4-D) ----
// Steadiness/averaging axes: {RPM, field-duty %, Vbus, tempF}. Defined here (before every function
// that references them) so the rest of the module can use the front. Generic engine: Xregulator.ino.
#define ALT_NAXIS        4
#define ALT_FRONT_CAP    1024     // sparse support points; cap is headroom (float IDW eval is cheap to ~1k)
#define ALT_EP_RING_CAP  8192     // reseed look-back (~40 s of 200 Hz folds); PSRAM
#define ALT_PENDING_CAP  1024     // = front cap: holds every unsynced point through weeks offline (PSRAM)

static Episode<ALT_NAXIS>     altEpisode;
static FrontStore<ALT_NAXIS>  altFront2;
static RawSample<ALT_NAXIS>  *altEpRing   = nullptr;
static FrontPoint<ALT_NAXIS> *altFrontBuf = nullptr;
static FrontPoint<ALT_NAXIS> *altPending  = nullptr;   // accepted-since-last-upload (raw points out)
static int altPendingCount   = 0;
static String altPendingSeededFrom = "";   // non-empty → this pending batch is an adopted import (provenance tag)
static int altFrontEmitCount = 0;        // episode points emitted (whether or not they pushed the front)

// Per-axis steady-time knobs + front/eval knobs (registry-wired below; per-axis tol + floors are in
// Xregulator.ino). altPruneK is echoed to the cloud but applied cloud-side.
float altRpmSec       = 3.0f;    // RPM steady time (s)
float altDutySec      = 3.0f;    // field-duty % steady time (s)
float altVbusSec      = 3.0f;    // bus-voltage steady time (s)
float altThermDegF    = 5.0f;    // temperature deviation bound (°F)
float altThermSec     = 30.0f;   // temperature steady time (s)
// Output-steadiness band (5th criterion: the measured amps themselves must hold steady — directly
// guards what gets recorded, letting the input bands stay tight) + detector signal conditioning:
float altAmpsTolPct   = 5.0f;    // output-amps band, % of the filtered reading
float altAmpsFloorA   = 1.5f;    // output-amps band floor (A) — governs at low output where ripple dominates
float altAmpsSec      = 3.0f;    // output-amps steady time (s)
float altEmaSec       = 0.5f;    // EMA time constant (s) on detector inputs RPM/duty/Vbus/amps (0 = off)
float altMinRunSec    = 2.0f;    // minimum steady-run length to emit a point (s)
float altRefRadius    = 2.0f;    // normalized nearest-support distance beyond which live % + trend report no reference
float altSafetyMargin = 0.0f;    // amps — gate keeps only runs that strictly beat the front (no keep-bias: the cloud only prunes, so sub-front samples were pure pollution of the local eval surface)
float altIdwPower     = 2.0f;    // IDW power for front_eval
float altPruneK       = 6.0f;    // cloud prune neighbor count (echoed; applied cloud-side)

// ---- performance-vs-engine-hours trend (engine-hour buckets: average + worst output-%) ----
// Each emitted steady episode adds its output-% (amps ÷ best-ever-front) to the current engine-hour
// bucket; the bucket's average + worst (min) are committed to the ring when the hour index advances.
// Engine-hours measured since the last "Start Over". NO clamp — % may exceed 100 vs a stale front.
static float altEngineHoursSinceBaseline() {
  double s = EngineRunTime_AllTime - altTrendBaselineSec;
  if (s < 0) s = 0;
  return (float)(s / 3600.0);
}
static void altCommitTrendBucket() {
  if (altCurEngHour < 0 || altBucket_n < 1 || !altTrend) return;
  float overall = (float)(altBucket_sum / altBucket_n) * 100.0f;
  float worst   = altBucket_worst * 100.0f;
  if (altTrendCount >= ALT_TREND_CAP) {  // ring full → drop oldest
    memmove(altTrend, altTrend + 1, (ALT_TREND_CAP - 1) * sizeof(AltTrendPt));
    altTrendCount = ALT_TREND_CAP - 1;
    altTrendRewrite = true;              // indices shifted → next save rewrites the whole log
  }
  AltTrendPt &p = altTrend[altTrendCount++];
  p.engHour = (uint16_t)altCurEngHour;
  p.worstPct = (int16_t)lroundf(worst * 10.0f);
  p.overallPct = (int16_t)lroundf(overall * 10.0f);
}
static void altTrendAdd(float perfFrac) {
  int eh = (int)altEngineHoursSinceBaseline();
  if (altCurEngHour < 0) altCurEngHour = eh;
  if (eh != altCurEngHour) {             // engine-hour bucket advanced → commit + reset
    altCommitTrendBucket();
    altBucket_sum = 0; altBucket_n = 0; altBucket_worst = perfFrac;
    altCurEngHour = eh;
  }
  if (altBucket_n < 1 || perfFrac < altBucket_worst) altBucket_worst = perfFrac;
  altBucket_sum += perfFrac; altBucket_n += 1;
  altOverallPctLive = (float)(altBucket_sum / altBucket_n) * 100.0f;
  altWorstPctLive   = altBucket_worst * 100.0f;
  // status: healthy if worst within ~8% of best-ever; drifting if it has fallen further.
  altStatusCode = (altFront2.count < 4) ? 0 : (altWorstPctLive >= 92.0f ? 1 : 2);
}

// ---- NMEA-style bench simulator (no engine) ----
// Sweeps RPM × excitation points, synthesizes amps from a near-deterministic model × a slow
// degradation ramp + noise so the curve fills and the trend declines. Writes only alt sim vars.
static float altSimRpm = 0, altSimExc = 0, altSimT = 80, altSimV = 13.6f, altSimAmps = 0;
static float altSimDuty = 50.0f;   // synthetic field duty % consistent with altSimExc (spreads the front's excitation axis)
static int   altSimIdx = 0, altSimLoops = 0;
static uint32_t altSimPtStartMs = 0;
static float altSimDeg = 1.0f;
#define ALT_SIM_NRPM 8
#define ALT_SIM_NEXC 5
#define ALT_SIM_NPTS (ALT_SIM_NRPM * ALT_SIM_NEXC)
#define ALT_SIM_HOLD_MS 40000u    // hold each point > max steady time (altThermSec=30 s) so a run forms + emits
static void altSimTick(uint32_t nowMs) {
  uint32_t hold = ALT_SIM_HOLD_MS;
  if (altSimPtStartMs == 0) altSimPtStartMs = nowMs;
  if (nowMs - altSimPtStartMs >= hold) {
    altSimPtStartMs = nowMs;
    if (++altSimIdx >= ALT_SIM_NPTS) { altSimIdx = 0; altSimLoops++; }
  }
  int ri = altSimIdx / ALT_SIM_NEXC, ei = altSimIdx % ALT_SIM_NEXC;
  float rpm = 800.0f + ri * 400.0f;             // 800..3600
  float exc = 3.0f + ei * 2.5f;                 // 3..13
  float tF = 80.0f + 0.02f * rpm;               // hotter at higher RPM
  float vbus = 13.6f;
  float a = 1.4f * exc * (1.0f - expf(-rpm / 1200.0f));   // rises with rpm knee + excitation
  a *= 1.0f - 0.0008f * (tF - 77.0f);                     // derate with temp
  if (altSimLoops >= 2) altSimDeg -= 0.0006f;             // after 2 full sweeps, degrade
  if (altSimDeg < 0.80f) altSimDeg = 0.80f;
  a *= altSimDeg;
  a *= 1.0f + (float)random(-20, 21) / 1000.0f;          // ±2% noise
  if (a < 0.5f) a = 0.5f;
  altSimRpm = rpm; altSimExc = exc; altSimT = tF; altSimV = vbus; altSimAmps = a;
  altSimDuty = exc * 100.0f / vbus;             // invert excitation → duty so the duty axis tracks exc
}

// Per-axis tol from the deviation-bound knobs, steady time from the *Sec knobs. Resynced every
// fold so live knob edits take effect immediately. ampsFilt sizes the relative output band
// (percent-of-reading with an absolute floor — one knob pair works at 5 A float and 150 A bulk).
static void altEpisodeSyncCfg(float ampsFilt) {
  altEpisode.cfg[0] = { altRpmTol,     altRpmSec  };   // RPM (filtered)
  altEpisode.cfg[1] = { altDutyTolPct, altDutySec };   // field duty % (filtered, absolute % points)
  altEpisode.cfg[2] = { altVbusTol,    altVbusSec };   // Vbus (V, filtered)
  altEpisode.cfg[3] = { altThermDegF,  altThermSec };  // temp (°F)
  float aTol = altAmpsTolPct * 0.01f * ampsFilt;
  altEpisode.outCfg = { (aTol > altAmpsFloorA) ? aTol : altAmpsFloorA, altAmpsSec };
  altEpisode.minRunMs = (altMinRunSec > 0) ? (uint32_t)(altMinRunSec * 1000.0f) : 0;
}

// ---- per-control-tick fold (THE canonical cadence) ----
// Live: called from the pidLog hook (~200 Hz). Bench-sim: called at 1 Hz from altHealth_tick.
// Reads the final control state, updates the live output-%, feeds the Episode detector, and on a
// steady-run emit derives the excitation surface coord, gates against the front, pushes if it
// beats best-ever, and feeds the engine-hour trend. The off/fault/shutdown paths early-return
// before the live pidLog hook, so field-off cases exclude themselves with no mode check.
void altFold_tick(uint32_t nowMs) {
  if (!altFrontBuf || !altEpRing) return;

  // IgnoreTemperature → the ENTIRE alt-health system is disabled: no live, no points, no trend.
  if (IgnoreTemperature) { altLiveValid = false; altSteady = false; altStatusCode = 3; return; }

  float rpm, tF, vbus, amps, duty;
  if (altSimMode >= 0.5f) {
    rpm = altSimRpm; tF = altSimT; vbus = altSimV; amps = altSimAmps; duty = altSimDuty;
  } else {
    vbus = getBatteryVoltage();
    amps = isnan(MeasuredAmps) ? 0.0f : MeasuredAmps;
    tF = TempToUse; duty = dutyCycle; rpm = RPM;
  }

  // Detector-input EMA (altEmaSec): strips inner-loop duty dither, RPM jitter, and current ripple
  // so the steadiness bands can be sized purely for real operating-point movement (= transient
  // rejection). The control loops never see these. A long gap between folds (field off, boot)
  // reseeds the filters from raw so stale state can't bridge it.
  static float fRpm = 0, fDuty = 0, fVbus = 0, fAmps = 0;
  static uint32_t lastFoldMs = 0;
  static bool fInit = false;
  float tauMs = altEmaSec * 1000.0f;
  uint32_t dtMs = nowMs - lastFoldMs;
  lastFoldMs = nowMs;
  if (!fInit || tauMs < 1.0f || dtMs > (uint32_t)(5.0f * tauMs) + 1000u) {
    fRpm = rpm; fDuty = duty; fVbus = vbus; fAmps = amps; fInit = true;
  } else {
    float a = (float)dtMs / (tauMs + (float)dtMs);   // irregular-interval EMA
    fRpm  += a * (rpm  - fRpm);  fDuty += a * (duty - fDuty);
    fVbus += a * (vbus - fVbus); fAmps += a * (amps - fAmps);
  }
  // Bench-sim injects its own consistent excitation; live derives it from the filtered drive.
  float exc = (altSimMode >= 0.5f) ? altSimExc : altExcitation(fDuty, fVbus, tF);

  // Live point + best-ever reference (dashboard gauge/dot). % is NOT clamped (spec §1/§6) — but it
  // IS reference-gated: beyond altRefRadius of all support the IDW blend is fiction (a 3-point
  // high-RPM front once "predicted" 21 A at idle → bogus 28% health), so report no reference.
  float surf[ALT_NAXIS] = { fRpm, exc, fVbus, tF };
  altLive_rpm = fRpm; altLive_exc = exc; altLive_amps = fAmps;
  altLive_pred = altFront2.eval(surf, altIdwPower);
  altRefDist   = altFront2.nearestNormDist(surf);
  if (altRefDist > 999.0f) altRefDist = 999.0f;
  altRefOk     = (altFront2.count > 0 && altRefDist <= altRefRadius);
  altLive_pct  = (altRefOk && altLive_pred > 0.1f) ? (fAmps / altLive_pred * 100.0f) : 0.0f;
  altLiveValid = (!isnan(fRpm) && !isnan(exc) && !isnan(fVbus) && fVbus >= ALT_MIN_BATT_V);

  if (hardwarePresent != 1 && altSimMode < 0.5f) { altSteady = false; return; }    // display only
  if (altPaused >= 0.5f || altFront2.source == 1) { altSteady = false; return; }   // FIXED/paused: no learning

  // Feed the Episode detector (steadiness/averaging axes {RPM, duty %, Vbus, tempF} + the
  // output-amps band) — all filtered, including the admission floors, so a single noise dip
  // can't act as a barrier that wipes the look-back ring mid-run.
  altEpisodeSyncCfg(fAmps);
  bool eligible = (!isnan(fVbus) && fVbus >= ALT_MIN_BATT_V && fAmps >= altMinAmps && fDuty >= altMinDuty && fRpm >= 0);
  RawSample<ALT_NAXIS> s;
  s.x[0] = fRpm; s.x[1] = fDuty; s.x[2] = fVbus; s.x[3] = tF; s.out = fAmps; s.tMs = nowMs;
  s.ex[0] = fDuty; s.ex[1] = 0;   // retain run duty (excitation is derived from it) for cloud diagnosis
  FrontPoint<ALT_NAXIS> ep;
  bool emitted = altEpisode.feed(eligible, s, &ep);
  altSteady = (altEpisode.count > 0);
  if (!emitted) return;

  // Steady run completed → build the surface point (excitation derived from run averages).
  altFrontEmitCount++;
  FrontPoint<ALT_NAXIS> sp;
  sp.x[0] = ep.x[0];                                    // RPM
  sp.x[1] = altExcitation(ep.x[1], ep.x[2], ep.x[3]);  // excitation
  sp.x[2] = ep.x[2];                                    // Vbus
  sp.x[3] = ep.x[3];                                    // tempF
  sp.ex[0] = ep.x[1];                                   // raw run-avg duty (retained for cloud diagnosis)
  sp.ex[1] = 0;
  sp.y = ep.y; sp.nSamp = ep.nSamp; sp.tEmit = ep.tEmit;

  float yref = altFront2.eval(sp.x, altIdwPower);                       // pre-add reference (trend %)
  bool refOk = (altFront2.count > 0 && altFront2.nearestNormDist(sp.x) <= altRefRadius);
  if (refOk && yref > 0.1f) altTrendAdd(sp.y / yref);                   // trend only vs a locally-supported reference
  // Cell-local admit gate: a run in an unvisited cell is admitted unconditionally (opens the region
  // at its true value); only a run with same-cell support must beat the IDW surface. The old global
  // gate locked low-output regions out forever once high-output points existed.
  if (altFront2.hasLocalSupport(sp.x)
      && !altFront2.pushes(sp.x, sp.y, altSafetyMargin, altIdwPower)) return;
  if (altFront2.add(sp)) {                                              // optimistic local front (cloud re-prunes)
    if (altPending && altPendingCount < ALT_PENDING_CAP) altPending[altPendingCount++] = sp;  // queue for upload
    // span r/d/v/t/a = the accepted run's per-axis spread (RPM/duty/Vbus/temp/amps) — soak evidence
    // for whether each band is doing real work or just slack.
    queueConsoleMessageF("AltFront +pt #%d rpm=%.0f exc=%.2f V=%.2f T=%.0f amps=%.1f (ref=%.1f n=%u span r=%.0f d=%.2f v=%.3f t=%.1f a=%.2f)",
      altFront2.count, sp.x[0], sp.x[1], sp.x[2], sp.x[3], sp.y, yref, sp.nSamp,
      altEpisode.lastRunSpan[0], altEpisode.lastRunSpan[1], altEpisode.lastRunSpan[2],
      altEpisode.lastRunSpan[3], altEpisode.lastRunOutSpan);
  }
}

// ---- live telemetry registry (schema-driven: one source for the AltLive payload + /altschema) ----
// One {name, getter} table builds BOTH the AltLive SSE payload AND the /altschema field list, so
// the firmware↔dashboard contract can't desync. Adding a field = add ONE row. All values floats.
struct AltLiveField { const char *name; float (*get)(); };
static float alf_valid()     { return (float)altLiveValid; }
static float alf_rpm()       { return altLive_rpm; }
static float alf_exc()       { return altLive_exc; }
static float alf_amps()      { return altLive_amps; }
static float alf_pred()      { return altLive_pred; }
static float alf_pct()       { return altLive_pct; }
static float alf_worst()     { return altWorstPctLive; }
static float alf_overall()   { return altOverallPctLive; }
static float alf_status()    { return (float)altStatusCode; }
static float alf_steady()    { return (float)altSteady; }
static float alf_engHours()  { return altEngineHoursSinceBaseline(); }
static float alf_coverage()  { return altFrontBuf ? (100.0f * (float)altFront2.count / (float)ALT_FRONT_CAP) : 0.0f; }
static float alf_haveCurve() { return (float)(altFront2.count > 0 ? 1 : 0); }   // have a usable front
static float alf_ptCount()   { return (float)altFront2.count; }                 // front support points
static float alf_source()    { return (float)altFront2.source; }                // 0 LEARNED, 1 FIXED
static float alf_paused()    { return (altPaused >= 0.5f) ? 1.0f : 0.0f; }
static float alf_refOk()     { return altRefOk ? 1.0f : 0.0f; }          // live % has nearby support → trustworthy
static float alf_refDist()   { return altRefDist; }                       // normalized distance to nearest support
static float alf_sim()       { return (altSimMode >= 0.5f) ? 1.0f : 0.0f; }
static float alf_syncAgo()   { if (lastAltHealthSyncEpoch <= 0 || !timeIsSynced) return -1.0f;
                               time_t n = time(NULL); return (n > (time_t)lastAltHealthSyncEpoch) ? (float)(n - (time_t)lastAltHealthSyncEpoch) : 0.0f; }
// fold timing moved to the Function Timing table (ft_altHealth / ft_altFold rows) — not in this live stream
static AltLiveField ALT_LIVE[] = {
  {"valid", alf_valid}, {"rpm", alf_rpm}, {"exc", alf_exc}, {"amps", alf_amps},
  {"pred", alf_pred}, {"pct", alf_pct}, {"worstPct", alf_worst}, {"overallPct", alf_overall},
  {"status", alf_status}, {"steady", alf_steady}, {"engHours", alf_engHours},
  {"coverage", alf_coverage}, {"haveCurve", alf_haveCurve}, {"ptCount", alf_ptCount},
  {"source", alf_source}, {"paused", alf_paused},
  {"refOk", alf_refOk}, {"refDist", alf_refDist},
  {"sim", alf_sim}, {"syncAgoS", alf_syncAgo},
};
static const size_t ALT_LIVE_COUNT = sizeof(ALT_LIVE) / sizeof(ALT_LIVE[0]);
static void altSendLive() {
  char buf[256];
  int off = 0;
  for (size_t i = 0; i < ALT_LIVE_COUNT; i++)
    off += snprintf(buf + off, sizeof(buf) - off, (i ? ",%.3f" : "%.3f"), ALT_LIVE[i].get());
  events.send(buf, "AltLive");
}

// ---- coverage / status helpers (CSV2 + dashboard) ----
float altCoveragePct() { return altFrontBuf ? (100.0f * (float)altFront2.count / (float)ALT_FRONT_CAP) : 0.0f; }
float altWorstPct()    { return altWorstPctLive; }
int   altStatus()      { return (int)altStatusCode; }
int   altHaveFront()   { return (altFront2.count > 0) ? 1 : 0; }   // CSV2: have a usable best-ever front
int   altFrontCount()  { return altFront2.count; }                 // CSV2: front support-point count
void  altClearPending() { altPendingCount = 0; altPendingSeededFrom = ""; }   // cloud accepted the batch → drop pending + tag

// ---- front CSV (the artifact): BEFRONT1,<sys>,<naxis>,<source>,<units…> then x0..xN,y,nSamp,tEmit ----
// Serves /altcurve.csv (dashboard), the cloud sync-back, and Save/Load to file (spec §2.2/§8).
String altCurveCsv() {
  String s = "BEFRONT1,ALT,"; s += String(ALT_NAXIS); s += ","; s += String(altFront2.source);
  s += ",rpm,exc,V,F,amps\n";
  for (int i = 0; i < altFront2.count; i++) {
    FrontPoint<ALT_NAXIS> &p = altFrontBuf[i];
    s += String(p.x[0], 0); s += ","; s += String(p.x[1], 3); s += ","; s += String(p.x[2], 2); s += ",";
    s += String(p.x[3], 1); s += ","; s += String(p.y, 2); s += ","; s += String(p.nSamp); s += ",";
    s += String(p.tEmit); s += "\n";
  }
  return s;
}
// Plain front-points table for the dashboard scatter view (/altrecords.csv). Distinct from the
// BEFRONT1 artifact above; small (≤ ALT_FRONT_CAP), so returned whole.
String altFrontRecordsCsv() {
  String s = "rpm,exc,vbus,tF,amps,nSamp\n";
  if (!altFrontBuf) return s;
  for (int i = 0; i < altFront2.count; i++) {
    FrontPoint<ALT_NAXIS> &p = altFrontBuf[i];
    s += String(p.x[0], 0); s += ","; s += String(p.x[1], 3); s += ","; s += String(p.x[2], 2); s += ",";
    s += String(p.x[3], 1); s += ","; s += String(p.y, 2); s += ","; s += String(p.nSamp); s += "\n";
  }
  return s;
}
// Parse a BEFRONT1 CSV (the cloud's pruned front, or a saved file) into altFront2, replacing it.
bool altIngestFrontCsv(char *body) {
  char *p = strstr(body, "BEFRONT1");
  if (!p) return false;
  char *nl = strchr(p, '\n');
  if (!nl) return false;
  uint8_t newSource = 0;
  { char saved = *nl; *nl = '\0';                       // header: BEFRONT1,sys,naxis,source,units…
    char *t = strtok(p, ",");                           // BEFRONT1
    t = strtok(NULL, ",");                              // sys
    t = strtok(NULL, ",");                              // naxis
    t = strtok(NULL, ",");                              // source
    if (t) newSource = (uint8_t)atoi(t);
    *nl = saved; }
  int newCount = 0;
  char *line = nl + 1;
  while (line && *line && newCount < ALT_FRONT_CAP) {
    char *eol = strchr(line, '\n');
    if (eol) *eol = '\0';
    float x0, x1, x2, x3, y; unsigned ns = 0, te = 0;
    if (*line && sscanf(line, "%f,%f,%f,%f,%f,%u,%u", &x0, &x1, &x2, &x3, &y, &ns, &te) >= 5) {
      FrontPoint<ALT_NAXIS> q;
      q.x[0] = x0; q.x[1] = x1; q.x[2] = x2; q.x[3] = x3; q.y = y;
      q.ex[0] = 0; q.ex[1] = 0;   // raw extras live in the cloud table, not the front CSV
      q.nSamp = (uint32_t)ns; q.tEmit = (uint32_t)te;
      altFrontBuf[newCount++] = q;
    }
    if (!eol) break;
    line = eol + 1;
  }
  if (line && *line && newCount >= ALT_FRONT_CAP)   // cloud sent more points than we can hold
    queueConsoleMessageF("WARN: alt front truncated at %d pts — raise ALT_FRONT_CAP", ALT_FRONT_CAP);
  altFront2.count = newCount;
  altFront2.source = newSource;
  return true;
}

// Append-only engine-hour trend log: 8-byte {magic,ver} header + AltTrendPt records. Each field-off
// appends only the newly committed buckets (~6 B/hour) instead of rewriting the whole 120 KB ring;
// a full rewrite happens only on a load-miss or ring eviction (altTrendRewrite).
static void altTrendPersist() {
  if (!altTrend) return;
  bool full = altTrendRewrite || altTrendFlushed > (uint32_t)altTrendCount || !fsExists("/alttrend.bin");
  fsTakeLock();
  if (full) {
    File f = LittleFS.open("/alttrend.bin", "w");
    if (f) {
      uint32_t hdr[2] = { ALT_TRENDLOG_MAGIC, ALT_VER };
      f.write((const uint8_t *)hdr, sizeof(hdr));
      if (altTrendCount > 0) f.write((const uint8_t *)altTrend, (size_t)altTrendCount * sizeof(AltTrendPt));
      f.close();
      altTrendFlushed = altTrendCount; altTrendRewrite = false;
    }
  } else if ((uint32_t)altTrendCount > altTrendFlushed) {   // append only the new buckets
    File f = LittleFS.open("/alttrend.bin", "a");
    if (f) {
      f.write((const uint8_t *)(altTrend + altTrendFlushed),
              (size_t)((uint32_t)altTrendCount - altTrendFlushed) * sizeof(AltTrendPt));
      f.close();
      altTrendFlushed = altTrendCount;
    }
  }
  fsReleaseLock();
}
static void altTrendLoad() {
  altTrendCount = 0; altTrendFlushed = 0; altTrendRewrite = true;   // default: log missing/invalid → rewrite next save
  if (!altTrend || !fsExists("/alttrend.bin")) return;
  fsTakeLock();
  File f = LittleFS.open("/alttrend.bin", "r");
  if (f) {
    uint32_t hdr[2] = { 0, 0 };
    size_t sz = f.size();
    if (sz >= sizeof(hdr) && f.read((uint8_t *)hdr, sizeof(hdr)) == sizeof(hdr)
        && hdr[0] == ALT_TRENDLOG_MAGIC && hdr[1] == ALT_VER) {
      uint32_t n = (sz - sizeof(hdr)) / sizeof(AltTrendPt);
      if (n > (uint32_t)ALT_TREND_CAP) {                  // keep most recent CAP (far-future overflow)
        f.seek(sizeof(hdr) + (size_t)(n - ALT_TREND_CAP) * sizeof(AltTrendPt));
        n = ALT_TREND_CAP;
      }
      size_t got = f.read((uint8_t *)altTrend, (size_t)n * sizeof(AltTrendPt));
      altTrendCount = (int)(got / sizeof(AltTrendPt));
      altTrendFlushed = altTrendCount; altTrendRewrite = false;
    }
    f.close();
  }
  fsReleaseLock();
}

// ---- persistence (field-off-gated by caller) — front + trend survive reboot (cloud authoritative) ----
void altHealthSave() {
  if (!altFrontBuf || hardwarePresent != 1) return;
  uint32_t uw = ((uint32_t)altFront2.source << 8) | (uint32_t)ALT_NAXIS;   // stash source + naxis
  writePsramBlob("/altfront.bin", ALT_FRONT_MAGIC, ALT_VER, uw, altFrontBuf, sizeof(FrontPoint<ALT_NAXIS>), ALT_FRONT_CAP, 0, altFront2.count);
  altTrendPersist();                                                       // append-only trend log
  settingWrite(NK_altbaseSec, String(altTrendBaselineSec, 1).c_str());   // trend X-axis origin
}
static void altLoad() {
  uint32_t uw = 0;
  if (altFrontBuf) {
    altFront2.count = (int)readPsramBlob("/altfront.bin", ALT_FRONT_MAGIC, ALT_VER, altFrontBuf, sizeof(FrontPoint<ALT_NAXIS>), ALT_FRONT_CAP, &uw, false);
    altFront2.source = (uint8_t)((uw >> 8) & 0xFF);
  }
  altTrendLoad();                                                          // append-only trend log
  if (settingExists(NK_altbaseSec)) altTrendBaselineSec = settingRead(NK_altbaseSec).toFloat();
}

// Ingest a BEFRONT1 front UPLOADED from the browser (Load CSV) — replace the front, then apply the
// mode the user chose at import: fixed=true → FIXED + paused (hold the borrowed curve exactly as-is,
// local only, never uploaded); fixed=false → LEARNED + resumed AND adopt to cloud (stage the imported
// points into pending, tagged "import", so the next field-off upload inserts them under THIS device —
// the cloud then treats them as native history). Persists immediately so the import survives reboot.
// Non-static so the /altUploadFront handler in 3_functions.ino can call it. Mutates the body buffer.
bool altUploadFrontCsv(char *body, bool fixed) {
  if (!altFrontBuf || !body) return false;
  bool ok = altIngestFrontCsv(body);
  if (!ok) return false;
  if (fixed) {                                  // FREEZE — local only
    altFront2.source = 1; altPaused = 1.0f;
    settingWrite(NK_altPaused, "1.0000");
    altPendingCount = 0; altPendingSeededFrom = "";   // freeze never uploads
    queueConsoleMessageF("AltFront: UPLOADED %d pts (FIXED, paused)", altFront2.count);
  } else {                                      // LEARN — adopt to cloud, then keep refining
    altPendingCount = 0;                        // replace pending with the imported set (pure seeded batch)
    for (int i = 0; i < altFront2.count && altPendingCount < ALT_PENDING_CAP; i++)
      altPending[altPendingCount++] = altFrontBuf[i];
    altPendingSeededFrom = "import";
    altFront2.source = 0; altPaused = 0.0f;
    settingWrite(NK_altPaused, "0.0000");
    if (altPendingCount < altFront2.count)
      queueConsoleMessageF("AltFront: UPLOADED %d pts (LEARNED); only %d queued to cloud (cap)", altFront2.count, altPendingCount);
    else
      queueConsoleMessageF("AltFront: UPLOADED %d pts (LEARNED, adopting to cloud)", altFront2.count);
  }
  altHealthSave();   // persist the front now (field-off-safe)
  return true;
}

// ---- cloud upload: batch of accepted episode points since the last upload (raw out; pruned front in) ----
// Schema: {device_uid,token,ts,sys,pruneK,idwPower, pts:[[rpm,exc,vbus,tF,amps,nSamp], ...]}.
// Pending cleared on a successful response (executeUploadAltHealth → altIngestFrontCsv).
bool buildAltHealthPayload(char *buf, size_t size) {
  if (!altPending || altPendingCount == 0 || authToken.isEmpty()) return false;
  time_t now_ts = time(NULL);
  int off = snprintf(buf, size,
    "{\"device_uid\":\"%s\",\"token\":\"%s\",\"ts\":\"%s\",\"sys\":\"ALT\",\"pruneK\":%d,\"idwPower\":%.2f,",
    device_id_hex, authToken.c_str(), formatTimestamp(now_ts), (int)altPruneK, altIdwPower);
  if (off < 0 || (size_t)off >= size) return false;
  if (altPendingSeededFrom.length()) {   // adopted import: tag the whole batch as borrowed provenance
    off += snprintf(buf + off, size - off, "\"seededFrom\":\"%s\",", altPendingSeededFrom.c_str());
    if (off < 0 || (size_t)off >= size) return false;
  }
  off += snprintf(buf + off, size - off, "\"pts\":[");
  bool first = true;
  for (int k = 0; k < altPendingCount; k++) {
    if (size - (size_t)off < 80) break;   // margin for the closer
    FrontPoint<ALT_NAXIS> &p = altPending[k];
    off += snprintf(buf + off, size - off, "%s[%.0f,%.3f,%.2f,%.1f,%.2f,%u,%.1f]",
                    first ? "" : ",", p.x[0], p.x[1], p.x[2], p.x[3], p.y, p.nSamp, p.ex[0]);  // ex[0]=raw duty
    first = false;
  }
  off += snprintf(buf + off, size - off, "]}");
  if (off < 0 || (size_t)off >= size - 1) return false;
  return true;
}

// ---- lifecycle ----
void initAlternatorHealth() {
  altTrend = (AltTrendPt *)ps_malloc((size_t)ALT_TREND_CAP * sizeof(AltTrendPt));
  if (!altTrend) { queueConsoleMessage("ERROR: AltHealth trend ps_malloc failed"); return; }
  memset(altTrend, 0, (size_t)ALT_TREND_CAP * sizeof(AltTrendPt));
  altTrendCount = 0; altCurEngHour = -1;
  altFrontInit();          // ps_malloc front + episode ring + pending; init the engine instance
  altLoad();               // restore front + trend + baseline (after the buffers exist)
  queueConsoleMessageF("AltHealth init: %d front pts (%s), %d trend pts",
                       altFront2.count, altFront2.source ? "FIXED" : "LEARNED", altTrendCount);
}
void resetAlternatorHealth() {
  if (!altFrontBuf) return;
  altFront2.count = 0; altFront2.source = 0;
  altPendingCount = 0; altFrontEmitCount = 0;
  altEpisode.clearRun(); altEpisode.ringHead = 0; altEpisode.ringCount = 0;
  altTrendCount = 0; altTrendFlushed = 0; altTrendRewrite = true;   // /alttrend.bin removed below → fresh log
  altBucket_sum = 0; altBucket_n = 0; altBucket_worst = 0; altCurEngHour = -1;
  altWorstPctLive = 0; altOverallPctLive = 0; altStatusCode = 0; altLive_pct = 0;
  altTrendBaselineSec = EngineRunTime_AllTime;   // new baseline → trend X-axis restarts at 0
  fsTakeLock();
  LittleFS.remove("/altfront.bin");
  LittleFS.remove("/alttrend.bin");
  fsReleaseLock();
  settingWrite(NK_altbaseSec, String(altTrendBaselineSec, 1).c_str());
  queueConsoleMessage("AltHealth: full reset (Start Over)");
}

// ---- engine allocation (PSRAM): front points + episode reseed ring + pending-upload buffer ----
void altFrontInit() {
  altEpRing   = (RawSample<ALT_NAXIS>  *)ps_malloc((size_t)ALT_EP_RING_CAP * sizeof(RawSample<ALT_NAXIS>));
  altFrontBuf = (FrontPoint<ALT_NAXIS> *)ps_malloc((size_t)ALT_FRONT_CAP   * sizeof(FrontPoint<ALT_NAXIS>));
  altPending  = (FrontPoint<ALT_NAXIS> *)ps_malloc((size_t)ALT_PENDING_CAP * sizeof(FrontPoint<ALT_NAXIS>));
  if (!altEpRing || !altFrontBuf || !altPending) { queueConsoleMessage("ERROR: AltFront ps_malloc failed"); return; }
  memset(altEpRing,   0, (size_t)ALT_EP_RING_CAP * sizeof(RawSample<ALT_NAXIS>));
  memset(altFrontBuf, 0, (size_t)ALT_FRONT_CAP   * sizeof(FrontPoint<ALT_NAXIS>));
  memset(altPending,  0, (size_t)ALT_PENDING_CAP * sizeof(FrontPoint<ALT_NAXIS>));
  altPendingCount = 0;
  altEpisode.init(altEpRing, ALT_EP_RING_CAP);
  altEpisodeSyncCfg(0.0f);   // amps band starts at the floor; resized from filtered amps every fold
  altFront2.init(altFrontBuf, ALT_FRONT_CAP);
  // axisScale ≈ each surface axis's characteristic span (plan §8: start at the axis tol).
  altFront2.axisScale[0] = 25.0f;   // RPM
  altFront2.axisScale[1] = 0.5f;    // excitation (temp-normalized field volts)
  altFront2.axisScale[2] = 0.1f;    // Vbus
  altFront2.axisScale[3] = 5.0f;    // tempF
  queueConsoleMessageF("AltFront init: cap %d pts, ring %d, %.1fKB PSRAM",
    ALT_FRONT_CAP, ALT_EP_RING_CAP,
    (float)((size_t)ALT_EP_RING_CAP * sizeof(RawSample<ALT_NAXIS>) +
            (size_t)(ALT_FRONT_CAP + ALT_PENDING_CAP) * sizeof(FrontPoint<ALT_NAXIS>)) / 1024.0f);
}

// ---- 1 Hz housekeeping tick (NOT the fold) — sends live telemetry + settings echo. In bench-sim
//      it also advances the simulator and folds at 1 Hz; live, the fold runs in the ~200 Hz pidLog
//      hook (altFold_tick is called from there). ----
void altHealth_tick(uint32_t nowMs) {
  static uint32_t lastMs = 0;
  if (!altFrontBuf) return;
  if (nowMs - lastMs < 1000) return;
  lastMs = nowMs;
  if (altSimMode >= 0.5f) {           // bench simulator: advance synthetic point + fold at 1 Hz
    altSimTick(nowMs);
    altFold_tick(nowMs);
  }
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
  {"altRpmTol", &altRpmTol},   {"altRpmSec", &altRpmSec},
  {"altDutyTolPct", &altDutyTolPct}, {"altDutySec", &altDutySec},
  {"altVbusTol", &altVbusTol}, {"altVbusSec", &altVbusSec},
  {"altThermDegF", &altThermDegF}, {"altThermSec", &altThermSec},
  {"altAmpsTolPct", &altAmpsTolPct}, {"altAmpsFloorA", &altAmpsFloorA}, {"altAmpsSec", &altAmpsSec},
  {"altEmaSec", &altEmaSec}, {"altMinRunSec", &altMinRunSec}, {"altRefRadius", &altRefRadius},
  {"altMinAmps", &altMinAmps}, {"altMinDuty", &altMinDuty},
  {"altSafetyMargin", &altSafetyMargin}, {"altIdwPower", &altIdwPower}, {"altPruneK", &altPruneK},
  {"altPaused", &altPaused},
};
static const size_t ALT_SETTING_COUNT = sizeof(ALT_SETTINGS) / sizeof(ALT_SETTINGS[0]);

void altSettingsLoad() {
  for (size_t i = 0; i < ALT_SETTING_COUNT; i++) {
    char key[16];
    snprintf(key, sizeof(key), "%s", ALT_SETTINGS[i].name);  // NVS key = registry name (15-char cap)
    if (!settingExists(key)) settingWrite(key, String(*ALT_SETTINGS[i].ptr, 4).c_str());
    else *ALT_SETTINGS[i].ptr = settingRead(key).toFloat();
  }
}
bool altSettingsHandle(AsyncWebServerRequest *request) {
  bool handled = false;
  for (size_t i = 0; i < ALT_SETTING_COUNT; i++) {
    if (request->hasParam(ALT_SETTINGS[i].name)) {
      *ALT_SETTINGS[i].ptr = request->getParam(ALT_SETTINGS[i].name)->value().toFloat();
      char key[16];
      snprintf(key, sizeof(key), "%s", ALT_SETTINGS[i].name);  // NVS key = registry name (15-char cap)
      settingWrite(key, String(*ALT_SETTINGS[i].ptr, 4).c_str());
      handled = true;
    }
  }
  // Action (not a float knob): LEARNED↔FIXED source toggle.
  if (request->hasParam("altSource")) {   // 1 = FIXED (freeze + pause), 0 = LEARNED (resume)
    int src = request->getParam("altSource")->value().toInt();
    altFront2.source = (uint8_t)(src ? 1 : 0);
    altPaused = src ? 1.0f : 0.0f;
    settingWrite(NK_altPaused, String(altPaused, 4).c_str());
    handled = true;
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

// Self-describing schema (served at /altschema). The dashboard fetches this ONCE and zips these
// names against the AltLive / AltSettings payload values — so it never keeps its own field array.
String altSchemaJson() {
  String s = "{\"live\":[";
  for (size_t i = 0; i < ALT_LIVE_COUNT; i++) { if (i) s += ","; s += "\""; s += ALT_LIVE[i].name; s += "\""; }
  s += "],\"settings\":[";
  for (size_t i = 0; i < ALT_SETTING_COUNT; i++) { if (i) s += ","; s += "\""; s += ALT_SETTINGS[i].name; s += "\""; }
  s += "]}";
  return s;
}



// ============================================================
// BOAT PERFORMANCE — Best-Ever Front engine instances (sail 3-D + motor 3-D). Mirrors the
//   alternator module. Folds on a fast internal tick (~10 Hz, from boatPerf_tick). Sail axes
//   {AWS, AWA, sea-state}; motor axes {RPM, headwind = AWS·cosAWA, sea-state}. Best-ever output =
//   the user-selected STW or SOG. Generic engine (Episode/FrontStore): top of Xregulator.ino.
// ============================================================
#define PERF_VER         3u
#define PERF_SAILF_MAGIC 0x50534652u  // 'PSFR' sail front blob
#define PERF_MOTF_MAGIC  0x504D4652u  // 'PMFR' motor front blob
#define PERF_NAXIS       3
#define PERF_FRONT_CAP   1024     // sparse support points; cap is headroom (10 Hz fold → eval cost trivial)
#define PERF_EP_RING_CAP 2048    // reseed look-back (~200 s at 10 Hz); PSRAM
#define PERF_PENDING_CAP 1024     // = front cap: holds every unsynced point through weeks offline (PSRAM)

static Episode<PERF_NAXIS>    sailEpisode,  motorEpisode;
static FrontStore<PERF_NAXIS> sailFront,    motorFront;
static RawSample<PERF_NAXIS>  *sailRing = nullptr,     *motorRing = nullptr;
static FrontPoint<PERF_NAXIS> *sailFrontBuf = nullptr, *motorFrontBuf = nullptr;
static FrontPoint<PERF_NAXIS> *sailPending = nullptr,  *motorPending = nullptr;
static int sailPendingCount = 0, motorPendingCount = 0;
static String perfPendingSeededFrom = "";   // non-empty → this pending batch is an adopted import (provenance tag)

// Steady-time + sea-state-window + headwind + front/eval + cloud-prune knobs (registry-wired below;
// per-axis deviation bounds + floors + mode flags are in Xregulator.ino).
float perfWsSec  = 3.0f;     // AWS steady time (s)
float perfWaSec  = 3.0f;     // AWA steady time (s)
float perfSeaTol = 1.0f;     // sea-state (pitch-std) deviation band (deg)
float perfSeaSec = 5.0f;     // sea-state steady time (s)
float perfSeaWinSec = 20.0f; // rolling window the pitch-std NUMBER is computed over (≠ perfSeaSec)
float perfRpmSec = 3.0f;     // motoring RPM steady time (s)
float perfHwTol  = 2.0f;     // headwind deviation band (kt)
float perfHwSec  = 3.0f;     // headwind steady time (s)
float perfSafetyMargin = 0.0f;  // kt — gate keeps only runs that strictly beat the front (no keep-bias: cloud gets raw episodes regardless, so sub-front samples only dragged down the local eval surface)
float perfIdwPower     = 2.0f;  // IDW power for front eval
float perfPruneK       = 6.0f;  // cloud prune neighbor count (echoed; applied cloud-side)

// Coverage / count accessors (CSV2 + dashboard).
float perfCoveragePct()      { return sailFrontBuf  ? (100.0f * (float)sailFront.count  / (float)PERF_FRONT_CAP) : 0.0f; }
float perfMotorCoveragePct() { return motorFrontBuf ? (100.0f * (float)motorFront.count / (float)PERF_FRONT_CAP) : 0.0f; }
int   perfSailCount()  { return sailFront.count; }
int   perfMotorCount() { return motorFront.count; }
void  perfClearPending() { sailPendingCount = 0; motorPendingCount = 0; perfPendingSeededFrom = ""; }   // cloud accepted the batch + tag

// Fold |AWA| to [0,180] when symmetric (eval/display only — raw AWA is stored to pending/cloud).
static inline float perfFoldAwa(float a) {
  if (perfFoldSymmetric < 0.5f) return a;
  float pa = a; while (pa < 0) pa += 360.0f; while (pa >= 360.0f) pa -= 360.0f;
  if (pa > 180.0f) pa = 360.0f - pa;
  return pa;
}

// ---- rolling sea-state: pitch standard deviation over the last perfSeaWinSec (default 20 s) ----
#define PERF_PITCH_RING 256
static float    perfPitchVal[PERF_PITCH_RING];
static uint32_t perfPitchMs[PERF_PITCH_RING];
static int      perfPitchHead = 0, perfPitchCount = 0;
static void perfPitchPush(float pitch, uint32_t nowMs) {
  perfPitchVal[perfPitchHead] = pitch; perfPitchMs[perfPitchHead] = nowMs;
  perfPitchHead = (perfPitchHead + 1) % PERF_PITCH_RING;
  if (perfPitchCount < PERF_PITCH_RING) perfPitchCount++;
}
static float perfSeaState(uint32_t nowMs) {
  uint32_t win = (uint32_t)(perfSeaWinSec * 1000.0f);
  double s = 0, s2 = 0; int n = 0;
  for (int k = 0; k < perfPitchCount; k++) {
    int idx = ((perfPitchHead - 1 - k) % PERF_PITCH_RING + PERF_PITCH_RING) % PERF_PITCH_RING;
    if ((uint32_t)(nowMs - perfPitchMs[idx]) > win) break;       // older than the window → stop
    s += perfPitchVal[idx]; s2 += (double)perfPitchVal[idx] * perfPitchVal[idx]; n++;
  }
  if (n < 2) return 0.0f;
  double var = s2 / n - (s / n) * (s / n);
  return (var > 0) ? (float)sqrt(var) : 0.0f;
}

// Per-axis tol from the deviation-bound knobs, steady time from the *Sec knobs. Resynced each fold.
static void perfEpisodeSyncCfg() {
  sailEpisode.cfg[0]  = { perfWsTol,  perfWsSec  };   // AWS
  sailEpisode.cfg[1]  = { perfWaTol,  perfWaSec  };   // AWA (raw, band-checked)
  sailEpisode.cfg[2]  = { perfSeaTol, perfSeaSec };   // sea-state
  motorEpisode.cfg[0] = { perfRpmTol, perfRpmSec };   // RPM
  motorEpisode.cfg[1] = { perfHwTol,  perfHwSec  };   // headwind
  motorEpisode.cfg[2] = { perfSeaTol, perfSeaSec };   // sea-state
}

// ---- per-tick fold (fast internal cadence, ~10 Hz from boatPerf_tick) ----
// Snapshots the latest axis values, updates the live %, feeds the SAIL or MOTOR Episode (the other
// is fed an ineligible break so a run can't span a sail↔motor transition), and on a steady-run emit
// derives the surface point, gates against the front, pushes if best-ever, and queues it for upload.
// Wind axes are APPARENT; AWA is stored RAW (both-sided) to the front/pending — folded to |AWA| only
// for eval + the device front when perfFoldSymmetric. Speed = STW or SOG per perfSpeedSrc.
// Bench simulator state (perfSim* — written ONLY by perfSimTick, never the real NMEA/IMU globals).
static float perfSimTws = 0, perfSimTwa = 0, perfSimStw = 0, perfSimSog = 0;
static float perfSimHdg = 90, perfSimPitch = 0, perfSimRpm = 0;
static int   perfSimLoops = 0;
static uint32_t perfSimPtStartMs = 0;
static float perfSimFoul = 1.0f;

void perfFold_tick(uint32_t nowMs) {
  if (!sailFrontBuf || !motorFrontBuf) return;

  // elapsed since last tick — for sailing/motoring hour accumulators (added once gated, below)
  static uint32_t perfHoursLastMs = 0;
  uint32_t perfDtMs = (perfHoursLastMs == 0) ? 0 : (nowMs - perfHoursLastMs);
  perfHoursLastMs = nowMs;
  if (perfDtMs > 2000) perfDtMs = 0;   // skip gaps (backgrounded tab, reboot, stalls)

  float aws, awa, pitch, rpm, spd; bool haveSpd;
  if (perfSimMode >= 0.5f) {
    aws = perfSimTws; awa = perfSimTwa; pitch = perfSimPitch; rpm = perfSimRpm;
    spd = (perfSpeedSrc >= 1.5f) ? perfSimSog : perfSimStw; haveSpd = true;
  } else {
    if (IS_STALE(IDX_APPARENT_WIND_SPEED) || isnan(ApparentWindSpeedNMEA) || isnan(ApparentWindAngleNMEA)) { perfSteady = false; return; }
    aws = ApparentWindSpeedNMEA; awa = ApparentWindAngleNMEA;
    pitch = imu_pitch_deg; rpm = RPM;
    if (perfSpeedSrc >= 1.5f) { haveSpd = !IS_STALE(IDX_SOG_NMEA); spd = SOGNMEA; }
    else                     { haveSpd = (!IS_STALE(IDX_STW_NMEA) && !isnan(STWNMEA)); spd = STWNMEA; }
    if (!haveSpd || isnan(spd)) spd = 0;
  }
  perfPitchPush(pitch, nowMs);
  float sea = perfSeaState(nowMs);
  perfEpisodeSyncCfg();
  uint8_t src = (perfSpeedSrc >= 1.5f) ? 2 : 1;
  float headwind = aws * cosf(awa * (float)PI / 180.0f);   // fore-aft apparent component (+ = headwind)
  bool motoring = (rpm > perfRpmFloor);

  // ── live display (NO clamp; % may exceed 100 vs a stale front) ──
  if (motoring) {
    float surf[PERF_NAXIS] = { rpm, headwind, sea };
    float best = motorFront.eval(surf, perfIdwPower);
    motorLive_rpm = rpm; motorLive_hw = headwind; motorLive_spd = spd; motorLive_pitch = sea;
    motorLive_best = best; motorLive_pct = (best > 0.1f) ? (spd / best * 100.0f) : 0.0f;
    motorLiveSrc = src; motorLiveValid = (best > 0.1f && haveSpd); perfLiveValid = false;
  } else {
    float surf[PERF_NAXIS] = { aws, perfFoldAwa(awa), sea };
    float best = sailFront.eval(surf, perfIdwPower);
    perfLive_ws = aws; perfLive_wa = awa; perfLive_spd = spd; perfLive_pitch = sea;
    perfLive_best = best; perfLive_pct = (best > 0.1f) ? (spd / best * 100.0f) : 0.0f;
    perfLiveSrc = src; perfLiveValid = (best > 0.1f && haveSpd && aws >= perfMinWindSpeed); motorLiveValid = false;
  }

  if (hardwarePresent != 1 && perfSimMode < 0.5f) { perfSteady = false; return; }   // display only
  if (perfPaused >= 0.5f) { perfSteady = false; return; }                           // paused: no learning

  // ── data-maturity hours: time spent actually moving in each mode (only while learning) ──
  if (perfDtMs > 0 && spd >= perfMinBoatSpeed) {
    if (motoring) perfMotorSeconds += perfDtMs / 1000.0;
    else          perfSailSeconds  += perfDtMs / 1000.0;
  }

  // ── feed both Episodes; the inactive one gets an ineligible break so runs don't span a mode change ──
  FrontPoint<PERF_NAXIS> ep;

  // SAIL
  bool sailLearn = (sailFront.source != 1);
  bool sailElig = sailLearn && !motoring && haveSpd && spd >= perfMinBoatSpeed && aws >= perfMinWindSpeed;
  RawSample<PERF_NAXIS> ss; ss.x[0] = aws; ss.x[1] = awa; ss.x[2] = sea; ss.out = spd; ss.tMs = nowMs;
  ss.ex[0] = 0; ss.ex[1] = 0;   // sail keeps raw AWS/AWA as surface axes x0/x1 already — no extras needed
  if (sailEpisode.feed(sailElig, ss, &ep)) {
    FrontPoint<PERF_NAXIS> raw = ep;                                  // raw both-sided AWA → cloud
    if (sailPending && sailPendingCount < PERF_PENDING_CAP) sailPending[sailPendingCount++] = raw;
    FrontPoint<PERF_NAXIS> sp = ep; sp.x[1] = perfFoldAwa(ep.x[1]);   // device front: folded AWA
    if (sailFront.pushes(sp.x, sp.y, perfSafetyMargin, perfIdwPower)) {
      if (sailFront.add(sp))
        queueConsoleMessageF("SailFront +pt #%d aws=%.1f awa=%.0f sea=%.2f spd=%.2f n=%u",
          sailFront.count, sp.x[0], sp.x[1], sp.x[2], sp.y, sp.nSamp);
    }
  }

  // MOTOR
  bool motorLearn = (motorFront.source != 1);
  bool motorElig = motorLearn && motoring && haveSpd && spd >= perfMinBoatSpeed;
  RawSample<PERF_NAXIS> ms; ms.x[0] = rpm; ms.x[1] = headwind; ms.x[2] = sea; ms.out = spd; ms.tMs = nowMs;
  ms.ex[0] = aws; ms.ex[1] = awa;   // retain raw AWS/AWA (headwind is derived from them) for cloud diagnosis
  if (motorEpisode.feed(motorElig, ms, &ep)) {
    if (motorPending && motorPendingCount < PERF_PENDING_CAP) motorPending[motorPendingCount++] = ep;
    if (motorFront.pushes(ep.x, ep.y, perfSafetyMargin, perfIdwPower)) {
      if (motorFront.add(ep))
        queueConsoleMessageF("MotorFront +pt #%d rpm=%.0f hw=%.1f sea=%.2f spd=%.2f n=%u",
          motorFront.count, ep.x[0], ep.x[1], ep.x[2], ep.y, ep.nSamp);
    }
  }

  // steady-run indicator: the active mode's Episode is currently accumulating eligible samples
  perfSteady = motoring ? (motorEpisode.count > 0) : (sailEpisode.count > 0);
}
// ---- NMEA SIMULATOR (bench testing, no boat) ----
// Writes ONLY to dedicated perfSim* vars (never the real NMEA/RPM/IMU globals) so it can't
// touch the control loop or other subsystems. Models a realistic sailboat: each "leg" picks a
// random operating point (weighted toward how a boat actually sails), holds it steady within the
// episode bands long enough to bank ONE record, then jumps to a new one — so the best-ever front
// fills ORGANICALLY over time (sparse → dense → converged) instead of re-tracing a fixed grid.
// Speed = hull × wind-response(TWS) × polar-shape(TWA) × sea derate × slow fouling × noise.
// (perfSim* vars declared above the fold.)
#define PERF_SIM_HOLD_MS 14000u    // hold each leg > max steady time so one run forms + emits

// Normalized best-speed fraction vs TWA (0..180, 10° steps): zero in the no-go zone, peak on the
// reach (~100-110°), still ~0.72 of peak dead downwind — matches the dashboard's render shape.
static float perfSimShape(float pa) {
  static const float S[19] = {0,0,0,0.42f,0.60f,0.74f,0.84f,0.90f,0.95f,0.98f,1.00f,1.00f,0.98f,0.94f,0.89f,0.85f,0.81f,0.78f,0.72f};
  if (pa < 0) pa = -pa; if (pa > 180) pa = 360 - pa; if (pa < 0) pa = 0;
  float f = pa / 10.0f; int i = (int)f; if (i > 17) i = 17; float t = f - i;
  return S[i] * (1.0f - t) + S[i + 1] * t;
}
static void perfSimTick(uint32_t nowMs) {
  const float HULL = 7.6f;                  // typical ~35-40 ft sailboat hull speed (kt)
  // Current leg's target — persists across ticks until the hold expires, then a new one is drawn.
  static float legTws = 12, legTwa = 95, legRpm = 0, legSea = 1.0f;
  static uint8_t legMotor = 0; static bool legInit = false;
  if (perfSimPtStartMs == 0) perfSimPtStartMs = nowMs;
  if (!legInit || nowMs - perfSimPtStartMs >= PERF_SIM_HOLD_MS) {
    legInit = true; perfSimPtStartMs = nowMs; perfSimLoops++;          // count legs (drives fouling)
    legMotor = (random(0, 100) < 20) ? 1 : 0;                          // ~20% of legs under engine
    if (legMotor) {
      legRpm = 1000.0f + (float)random(0, 2401);                       // 1000..3400 RPM
      legTws = (float)random(0, 121) / 10.0f;                          // 0..12 kt ambient wind
      legTwa = (float)random(0, 181);
      legSea = 0.3f + (float)random(0, 151) / 100.0f;                  // 0.3..1.8
    } else {
      // wind speed: triangular (avg of two uniforms) → peaks ~13 kt, spans ~4..23
      legTws = 4.0f + (float)(random(0, 191) + random(0, 191)) / 20.0f;
      // wind angle: triangular → peaks ~105°, spans ~30..176 (full range incl. downwind)
      legTwa = 30.0f + (float)(random(0, 1461) + random(0, 1461)) / 20.0f;
      legSea = 0.3f + legTws * 0.12f + (float)random(-30, 31) / 100.0f;// chop grows with wind
      if (legSea < 0) legSea = 0; if (legSea > 5) legSea = 5;
      legRpm = 0;
    }
  }
  // Small in-band jitter so the run looks "live" but stays steady within the episode tolerances.
  uint8_t motor = legMotor;
  float tws = legTws + (float)random(-20, 21) / 100.0f;               // ±0.2 kt
  float twa = legTwa + (float)random(-10, 11) / 10.0f;               // ±1.0°
  float rpm = legMotor ? (legRpm + (float)random(-15, 16)) : 0.0f;    // ±15 RPM
  float sea = legSea;
  float v;
  if (motor) {
    v = HULL * (1.0f - expf(-(rpm - 600.0f) / 1300.0f));              // idle ~2kt, cruise ~6.5, → hull
    if (v < 0) v = 0;
    float hw = tws * cosf(twa * (float)PI / 180.0f);                  // ambient headwind while motoring
    v -= 0.015f * (hw > 0 ? hw : 0);
  } else {
    float resp = 1.0f - expf(-tws / 8.0f);                           // wind response: ~.63@8 .86@16 .94@22
    v = HULL * resp * perfSimShape(twa);                              // × normalized polar shape
  }
  float seaStd = sea / 1.414f;
  v *= (1.0f - 0.025f * seaStd);                                      // rougher seas → slower
  if (perfSimLoops >= 40) perfSimFoul -= 0.0002f;                     // gentle fouling after ~40 legs (~9 min)
  if (perfSimFoul < 0.85f) perfSimFoul = 0.85f;
  v *= perfSimFoul;
  v *= 1.0f + (float)random(-25, 26) / 1000.0f;                      // ±2.5% measurement noise
  if (v < 0.2f) v = 0.2f;
  perfSimPitch = sea * sinf(2.0f * (float)PI * (float)(nowMs % 4000) / 4000.0f);  // wave-like pitch
  perfSimTws = tws; perfSimTwa = twa; perfSimHdg = 90.0f;
  perfSimRpm = motor ? rpm : 0.0f;
  perfSimStw = v; perfSimSog = v;                            // no current; SOG == STW
}

// (perfProcessSample removed — the fold (perfFold_tick) reads the latest axis values directly.)

// ---- live telemetry registry (PILOT: single source of truth for payload + schema) ----
// One {name, getter} table builds BOTH the PerfLive payload AND the /perfschema field list,
// so the firmware↔dashboard field contract cannot desync (no hand-kept parallel JS array,
// no format-string/count drift). Adding a field = add ONE row. All values sent as floats.
struct PerfLiveField { const char *name; float (*get)(); };
static float plf_valid()    { return (float)perfLiveValid; }
static float plf_ws()       { return perfLive_ws; }
static float plf_wa()       { return perfLive_wa; }
static float plf_spd()      { return perfLive_spd; }
static float plf_best()     { return perfLive_best; }
static float plf_pct()      { return perfLive_pct; }
static float plf_pitchStd() { return perfLive_pitch; }
static float plf_src()      { return (float)perfLiveSrc; }
static float plf_coverage() { return perfCoveragePct(); }
static float plf_ptCount()  { return (float)sailFront.count; }
static float plf_source()   { return (float)sailFront.source; }    // 0 LEARNED, 1 FIXED
static float plf_paused()   { return (perfPaused >= 0.5f) ? 1.0f : 0.0f; }
// fold timing moved to the Function Timing table (ft_boatPerf row) — not in this live stream
static float plf_sim()      { return (perfSimMode >= 0.5f) ? 1.0f : 0.0f; }
static float plf_syncAgo()  { if (lastBoatPerfSyncEpoch <= 0 || !timeIsSynced) return -1.0f;
                              time_t n = time(NULL); return (n > (time_t)lastBoatPerfSyncEpoch) ? (float)(n - (time_t)lastBoatPerfSyncEpoch) : 0.0f; }
static float plf_sailHours(){ return (float)(perfSailSeconds / 3600.0); }   // data-maturity hours
static float plf_steady()   { return (float)perfSteady; }                   // in a steady-run right now
static PerfLiveField PERF_LIVE[] = {
  {"valid", plf_valid}, {"ws", plf_ws}, {"wa", plf_wa}, {"spd", plf_spd},
  {"best", plf_best}, {"pct", plf_pct}, {"pitchStd", plf_pitchStd}, {"src", plf_src},
  {"coverage", plf_coverage}, {"ptCount", plf_ptCount}, {"source", plf_source},
  {"paused", plf_paused},
  {"sim", plf_sim}, {"syncAgoS", plf_syncAgo},
  {"sailHours", plf_sailHours}, {"steady", plf_steady},
};
static const size_t PERF_LIVE_COUNT = sizeof(PERF_LIVE) / sizeof(PERF_LIVE[0]);

static void perfSendLive() {
  char buf[224];
  int off = 0;
  for (size_t i = 0; i < PERF_LIVE_COUNT; i++)
    off += snprintf(buf + off, sizeof(buf) - off, (i ? ",%.3f" : "%.3f"), PERF_LIVE[i].get());
  events.send(buf, "PerfLive");
}

// Motoring live registry (same {name,getter} pattern; schema served alongside under "motorLive").
static float pmlf_valid()    { return (float)motorLiveValid; }
static float pmlf_rpm()      { return motorLive_rpm; }
static float pmlf_headwind() { return motorLive_hw; }
static float pmlf_spd()      { return motorLive_spd; }
static float pmlf_best()     { return motorLive_best; }
static float pmlf_pct()      { return motorLive_pct; }
static float pmlf_src()      { return (float)motorLiveSrc; }
static float pmlf_coverage() { return perfMotorCoveragePct(); }
static float pmlf_ptCount()  { return (float)motorFront.count; }
static float pmlf_source()   { return (float)motorFront.source; }
static float pmlf_paused()   { return (perfPaused >= 0.5f) ? 1.0f : 0.0f; }
static float pmlf_pitchStd() { return motorLive_pitch; }
static float pmlf_motorHours() { return (float)(perfMotorSeconds / 3600.0); }   // data-maturity hours
static float pmlf_steady()     { return (float)perfSteady; }                    // in a steady-run right now
static PerfLiveField PERF_MOTOR_LIVE[] = {
  {"valid", pmlf_valid}, {"rpm", pmlf_rpm}, {"headwind", pmlf_headwind}, {"spd", pmlf_spd},
  {"best", pmlf_best}, {"pct", pmlf_pct}, {"src", pmlf_src},
  {"coverage", pmlf_coverage}, {"ptCount", pmlf_ptCount}, {"source", pmlf_source}, {"paused", pmlf_paused},
  {"pitchStd", pmlf_pitchStd},
  {"motorHours", pmlf_motorHours}, {"steady", pmlf_steady},
};
static const size_t PERF_MOTOR_LIVE_COUNT = sizeof(PERF_MOTOR_LIVE) / sizeof(PERF_MOTOR_LIVE[0]);
static void perfSendMotorLive() {
  char buf[224];
  int off = 0;
  for (size_t i = 0; i < PERF_MOTOR_LIVE_COUNT; i++)
    off += snprintf(buf + off, sizeof(buf) - off, (i ? ",%.3f" : "%.3f"), PERF_MOTOR_LIVE[i].get());
  events.send(buf, "MotorLive");
}

// ---- persistence (Phase-0 scaffold; field-off-gated by caller) ----
void boatPerfSave() {
  if (!sailFrontBuf || hardwarePresent != 1) return;
  uint32_t suw = ((uint32_t)sailFront.source  << 8) | (uint32_t)PERF_NAXIS;
  uint32_t muw = ((uint32_t)motorFront.source << 8) | (uint32_t)PERF_NAXIS;
  writePsramBlob("/sailfront.bin",  PERF_SAILF_MAGIC, PERF_VER, suw, sailFrontBuf,  sizeof(FrontPoint<PERF_NAXIS>), PERF_FRONT_CAP, 0, sailFront.count);
  writePsramBlob("/motorfront.bin", PERF_MOTF_MAGIC,  PERF_VER, muw, motorFrontBuf, sizeof(FrontPoint<PERF_NAXIS>), PERF_FRONT_CAP, 0, motorFront.count);
}
static void boatPerfLoad() {
  uint32_t uw = 0;
  if (sailFrontBuf)  { sailFront.count  = (int)readPsramBlob("/sailfront.bin",  PERF_SAILF_MAGIC, PERF_VER, sailFrontBuf,  sizeof(FrontPoint<PERF_NAXIS>), PERF_FRONT_CAP, &uw, false); sailFront.source  = (uint8_t)((uw >> 8) & 0xFF); }
  if (motorFrontBuf) { motorFront.count = (int)readPsramBlob("/motorfront.bin", PERF_MOTF_MAGIC,  PERF_VER, motorFrontBuf, sizeof(FrontPoint<PERF_NAXIS>), PERF_FRONT_CAP, &uw, false); motorFront.source = (uint8_t)((uw >> 8) & 0xFF); }
}

// ---- front CSV (the artifact). A boat "curve" = the PAIR of sail + motor BEFRONT1 blocks, mode-
//      tagged. target: 0 = sail, 1 = motor. (int target, not a template-typed param — keeps
//      Arduino's auto-prototype pass from referencing FrontStore<3> before PERF_NAXIS is defined.) ----
static String perfFrontBlock(int target) {
  FrontStore<PERF_NAXIS> &f = target ? motorFront : sailFront;
  FrontPoint<PERF_NAXIS> *buf = target ? motorFrontBuf : sailFrontBuf;
  String s = "BEFRONT1,"; s += target ? "MOTOR" : "SAIL"; s += ","; s += String(PERF_NAXIS); s += ",";
  s += String(f.source); s += ","; s += target ? "rpm,hw,sea,spd" : "aws,awa,sea,spd"; s += "\n";
  for (int i = 0; i < f.count; i++) {
    FrontPoint<PERF_NAXIS> &p = buf[i];
    s += String(p.x[0], target ? 0 : 2); s += ","; s += String(p.x[1], 1); s += ","; s += String(p.x[2], 3); s += ",";
    s += String(p.y, 2); s += ","; s += String(p.nSamp); s += ","; s += String(p.tEmit); s += "\n";
  }
  return s;
}
String perfCurveCsv()  { return perfFrontBlock(0) + perfFrontBlock(1); }   // /perfcurve.csv
String perfRecordsCsv() {                                                  // /perfrecords.csv — scatter table
  String s = "mode,a0,a1,sea,spd,nSamp\n";
  if (sailFrontBuf)  for (int i = 0; i < sailFront.count;  i++) { FrontPoint<PERF_NAXIS> &p = sailFrontBuf[i];  s += "sail,"  + String(p.x[0],2)+","+String(p.x[1],1)+","+String(p.x[2],3)+","+String(p.y,2)+","+String(p.nSamp)+"\n"; }
  if (motorFrontBuf) for (int i = 0; i < motorFront.count; i++) { FrontPoint<PERF_NAXIS> &p = motorFrontBuf[i]; s += "motor," + String(p.x[0],0)+","+String(p.x[1],1)+","+String(p.x[2],3)+","+String(p.y,2)+","+String(p.nSamp)+"\n"; }
  return s;
}
// Parse one BEFRONT1 block (starting at p) into the sail (target 0) or motor (target 1) front.
static int perfIngestBlock(char *p, int target) {
  FrontStore<PERF_NAXIS> &f = target ? motorFront : sailFront;
  FrontPoint<PERF_NAXIS> *buf = target ? motorFrontBuf : sailFrontBuf;
  char *nl = strchr(p, '\n'); if (!nl) return 0;
  uint8_t src = 0;
  { char saved = *nl; *nl = '\0'; char *t = strtok(p, ",");   // BEFRONT1
    t = strtok(NULL, ","); t = strtok(NULL, ","); t = strtok(NULL, ",");   // sys, naxis, source
    if (t) src = (uint8_t)atoi(t); *nl = saved; }
  int n = 0; char *line = nl + 1;
  while (line && *line && n < PERF_FRONT_CAP) {
    if (strncmp(line, "BEFRONT1", 8) == 0) break;             // next block begins
    char *eol = strchr(line, '\n'); if (eol) *eol = '\0';
    float x0, x1, x2, y; unsigned ns = 0, te = 0;
    if (*line && sscanf(line, "%f,%f,%f,%f,%u,%u", &x0, &x1, &x2, &y, &ns, &te) >= 4) {
      FrontPoint<PERF_NAXIS> q; q.x[0] = x0; q.x[1] = x1; q.x[2] = x2; q.y = y;
      q.ex[0] = 0; q.ex[1] = 0;   // raw extras live in the cloud table, not the front CSV
      q.nSamp = (uint32_t)ns; q.tEmit = (uint32_t)te;
      buf[n++] = q;
    }
    if (!eol) break;
    line = eol + 1;
  }
  if (line && *line && strncmp(line, "BEFRONT1", 8) != 0 && n >= PERF_FRONT_CAP)   // more than we can hold
    queueConsoleMessageF("WARN: %s front truncated at %d pts — raise PERF_FRONT_CAP", target ? "motor" : "sail", PERF_FRONT_CAP);
  f.count = n; f.source = src;
  return n;
}
// The cloud reply (or a saved file) carries both blocks (SAIL then MOTOR). Replace both held fronts.
bool perfIngestFrontCsv(char *body) {
  char *sp = strstr(body, "BEFRONT1,SAIL");
  char *mp = strstr(body, "BEFRONT1,MOTOR");
  if (sp) perfIngestBlock(sp, 0);
  if (mp) perfIngestBlock(mp, 1);
  return (sp || mp);
}

// ---- cloud upload: batch of accepted points since last upload (sail + motor); pruned fronts back ----
// Schema: {device_uid,token,ts,sys:"BOAT",speedSrc,foldSym,pruneK,idwPower,
//          sail:[[aws,awa,sea,spd,nSamp]...], motor:[[rpm,hw,sea,spd,nSamp,aws,awa]...]}. AWA raw both-sided;
//          motor also carries raw AWS/AWA (headwind is derived from them) for cloud diagnosis.
// Pending cleared on a successful response (executeUploadBoatPerf → perfIngestFrontCsv → perfClearPending).
bool buildBoatPerfPayload(char *buf, size_t size) {
  if ((sailPendingCount == 0 && motorPendingCount == 0) || authToken.isEmpty()) return false;
  time_t now_ts = time(NULL);
  int off = snprintf(buf, size,
    "{\"device_uid\":\"%s\",\"token\":\"%s\",\"ts\":\"%s\",\"sys\":\"BOAT\",\"speedSrc\":%d,\"foldSym\":%d,\"pruneK\":%d,\"idwPower\":%.2f,",
    device_id_hex, authToken.c_str(), formatTimestamp(now_ts),
    (int)perfSpeedSrc, (perfFoldSymmetric >= 0.5f) ? 1 : 0, (int)perfPruneK, perfIdwPower);
  if (off < 0 || (size_t)off >= size) return false;
  if (perfPendingSeededFrom.length()) {   // adopted import: tag the whole batch as borrowed provenance
    off += snprintf(buf + off, size - off, "\"seededFrom\":\"%s\",", perfPendingSeededFrom.c_str());
    if (off < 0 || (size_t)off >= size) return false;
  }
  off += snprintf(buf + off, size - off, "\"sail\":[");
  bool first = true;
  for (int k = 0; k < sailPendingCount; k++) {
    if (size - (size_t)off < 64) break;
    FrontPoint<PERF_NAXIS> &p = sailPending[k];
    off += snprintf(buf + off, size - off, "%s[%.2f,%.1f,%.3f,%.2f,%u]", first ? "" : ",", p.x[0], p.x[1], p.x[2], p.y, p.nSamp);
    first = false;
  }
  off += snprintf(buf + off, size - off, "],\"motor\":[");
  first = true;
  for (int k = 0; k < motorPendingCount; k++) {
    if (size - (size_t)off < 64) break;
    FrontPoint<PERF_NAXIS> &p = motorPending[k];
    off += snprintf(buf + off, size - off, "%s[%.0f,%.1f,%.3f,%.2f,%u,%.2f,%.1f]",
                    first ? "" : ",", p.x[0], p.x[1], p.x[2], p.y, p.nSamp, p.ex[0], p.ex[1]);  // ex=raw AWS,AWA
    first = false;
  }
  off += snprintf(buf + off, size - off, "]}");
  if (off < 0 || (size_t)off >= size - 1) return false;
  return true;
}

// Ingest a BEFRONT1 pair UPLOADED from the browser (Load CSV) — replace both fronts, then set the
// mode the user chose at import: fixed=true → FIXED + paused (hold the borrowed polar exactly as-is);
// fixed=false → LEARNED + resumed (use it as a starting point, keep refining from your own sailing).
// Persists immediately so the import survives reboot (the file isn't on the device to re-load).
// Non-static so the /perfUploadFront handler in 3_functions.ino can call it (sailFront/motorFront are
// file-static here). Mutates the body buffer.
bool perfUploadFrontCsv(char *body, bool fixed) {
  if (!sailFrontBuf || !body) return false;
  bool ok = perfIngestFrontCsv(body);
  if (!ok) return false;
  uint8_t src = fixed ? 1 : 0;
  sailFront.source = src; motorFront.source = src;
  perfPaused = fixed ? 1.0f : 0.0f;
  settingWrite(NK_perfPaused, fixed ? "1.0000" : "0.0000");
  if (fixed) {                                  // FREEZE — local only, never uploaded
    sailPendingCount = motorPendingCount = 0; perfPendingSeededFrom = "";
  } else {                                      // LEARN — adopt to cloud: stage both fronts (tagged), keep refining
    sailPendingCount = motorPendingCount = 0;   // replace pending with the imported set (pure seeded batch)
    for (int i = 0; i < sailFront.count  && sailPendingCount  < PERF_PENDING_CAP; i++) sailPending[sailPendingCount++]   = sailFrontBuf[i];
    for (int i = 0; i < motorFront.count && motorPendingCount < PERF_PENDING_CAP; i++) motorPending[motorPendingCount++] = motorFrontBuf[i];
    perfPendingSeededFrom = "import";
  }
  boatPerfSave();   // persist /sailfront.bin + /motorfront.bin now (field-off-safe)
  queueConsoleMessageF("PerfFront: UPLOADED sail %d + motor %d (%s)",
                       sailFront.count, motorFront.count, fixed ? "FIXED, paused" : "LEARNED, adopting to cloud");
  return true;
}

// ---- lifecycle ----
void initBoatPerformance() {
  sailRing      = (RawSample<PERF_NAXIS>  *)ps_malloc((size_t)PERF_EP_RING_CAP * sizeof(RawSample<PERF_NAXIS>));
  motorRing     = (RawSample<PERF_NAXIS>  *)ps_malloc((size_t)PERF_EP_RING_CAP * sizeof(RawSample<PERF_NAXIS>));
  sailFrontBuf  = (FrontPoint<PERF_NAXIS> *)ps_malloc((size_t)PERF_FRONT_CAP   * sizeof(FrontPoint<PERF_NAXIS>));
  motorFrontBuf = (FrontPoint<PERF_NAXIS> *)ps_malloc((size_t)PERF_FRONT_CAP   * sizeof(FrontPoint<PERF_NAXIS>));
  sailPending   = (FrontPoint<PERF_NAXIS> *)ps_malloc((size_t)PERF_PENDING_CAP * sizeof(FrontPoint<PERF_NAXIS>));
  motorPending  = (FrontPoint<PERF_NAXIS> *)ps_malloc((size_t)PERF_PENDING_CAP * sizeof(FrontPoint<PERF_NAXIS>));
  if (!sailRing || !motorRing || !sailFrontBuf || !motorFrontBuf || !sailPending || !motorPending) {
    queueConsoleMessage("ERROR: BoatPerf ps_malloc failed"); return;
  }
  memset(sailFrontBuf,  0, (size_t)PERF_FRONT_CAP * sizeof(FrontPoint<PERF_NAXIS>));
  memset(motorFrontBuf, 0, (size_t)PERF_FRONT_CAP * sizeof(FrontPoint<PERF_NAXIS>));
  sailEpisode.init(sailRing, PERF_EP_RING_CAP);
  motorEpisode.init(motorRing, PERF_EP_RING_CAP);
  perfEpisodeSyncCfg();
  sailFront.init(sailFrontBuf, PERF_FRONT_CAP);
  motorFront.init(motorFrontBuf, PERF_FRONT_CAP);
  sailFront.axisScale[0]  = 2.0f;   sailFront.axisScale[1]  = 12.0f; sailFront.axisScale[2]  = 1.0f;  // AWS, AWA, sea
  motorFront.axisScale[0] = 100.0f; motorFront.axisScale[1] = 2.0f;  motorFront.axisScale[2] = 1.0f;  // RPM, headwind, sea
  sailPendingCount = motorPendingCount = 0;
  boatPerfLoad();
  queueConsoleMessageF("BoatPerf init: sail %d pts (%s), motor %d pts (%s)",
                       sailFront.count, sailFront.source ? "FIXED" : "LEARNED",
                       motorFront.count, motorFront.source ? "FIXED" : "LEARNED");
}
void resetBoatPerformance() {
  if (!sailFrontBuf) return;
  sailFront.count = 0; sailFront.source = 0; motorFront.count = 0; motorFront.source = 0;
  sailPendingCount = motorPendingCount = 0;
  sailEpisode.clearRun();  sailEpisode.ringHead = 0;  sailEpisode.ringCount = 0;
  motorEpisode.clearRun(); motorEpisode.ringHead = 0; motorEpisode.ringCount = 0;
  perfPitchHead = 0; perfPitchCount = 0;
  perfSailSeconds = 0.0; perfMotorSeconds = 0.0;   // data-maturity hours reset with the learned data
  perfLiveValid = false; motorLiveValid = false; perfLive_pct = 0; motorLive_pct = 0;
  fsTakeLock();
  LittleFS.remove("/sailfront.bin");
  LittleFS.remove("/motorfront.bin");
  fsReleaseLock();
  queueConsoleMessage("BoatPerf: full reset (Clear All)");
}

// ---- tick (call from loop()): fold at ~10 Hz, send live telemetry + settings echo at ~1 Hz ----
void boatPerf_tick(uint32_t nowMs) {
  static uint32_t lastFold = 0, lastSse = 0;
  if (!sailFrontBuf) return;
  if (perfSimMode >= 0.5f) perfSimTick(nowMs);          // bench simulator feeds the sim vars first
  if ((uint32_t)(nowMs - lastFold) >= 100) { lastFold = nowMs; perfFold_tick(nowMs); }   // ~10 Hz fold
  if ((uint32_t)(nowMs - lastSse) >= 1000) {                                              // ~1 Hz SSE + echo
    lastSse = nowMs;
    perfSendLive();
    perfSendMotorLive();
    static uint8_t sc = 0;
    if (++sc >= 5) { sc = 0; sendPerfSettings(); }
  }
}

// ============================================================
// BOAT PERFORMANCE — GUI-adjustable settings (registry-driven, like AltSettings).
// ============================================================
struct PerfSetting { const char *name; float *ptr; };
static PerfSetting PERF_SETTINGS[] = {
  {"perfWsTol", &perfWsTol}, {"perfWsSec", &perfWsSec},
  {"perfWaTol", &perfWaTol}, {"perfWaSec", &perfWaSec},
  {"perfSeaTol", &perfSeaTol}, {"perfSeaSec", &perfSeaSec}, {"perfSeaWinSec", &perfSeaWinSec},
  {"perfRpmTol", &perfRpmTol}, {"perfRpmSec", &perfRpmSec},
  {"perfHwTol", &perfHwTol}, {"perfHwSec", &perfHwSec},
  {"perfMinBoatSpeed", &perfMinBoatSpeed}, {"perfMinWindSpeed", &perfMinWindSpeed},
  {"perfRpmFloor", &perfRpmFloor},
  {"perfSafetyMargin", &perfSafetyMargin}, {"perfIdwPower", &perfIdwPower}, {"perfPruneK", &perfPruneK},
  {"perfSpeedSrc", &perfSpeedSrc}, {"perfFoldSymmetric", &perfFoldSymmetric}, {"perfPaused", &perfPaused},
};
static const size_t PERF_SETTING_COUNT = sizeof(PERF_SETTINGS) / sizeof(PERF_SETTINGS[0]);

void perfSettingsLoad() {
  for (size_t i = 0; i < PERF_SETTING_COUNT; i++) {
    char key[16];
    snprintf(key, sizeof(key), "%s", PERF_SETTINGS[i].name);  // NVS key = registry name truncated to the 15-char cap
    if (!settingExists(key)) settingWrite(key, String(*PERF_SETTINGS[i].ptr, 4).c_str());
    else *PERF_SETTINGS[i].ptr = settingRead(key).toFloat();
  }
}
bool perfSettingsHandle(AsyncWebServerRequest *request) {
  bool handled = false;
  float oldSpeedSrc = perfSpeedSrc;
  for (size_t i = 0; i < PERF_SETTING_COUNT; i++) {
    if (request->hasParam(PERF_SETTINGS[i].name)) {
      *PERF_SETTINGS[i].ptr = request->getParam(PERF_SETTINGS[i].name)->value().toFloat();
      char key[16];
      snprintf(key, sizeof(key), "%s", PERF_SETTINGS[i].name);  // NVS key = registry name truncated to the 15-char cap
      settingWrite(key, String(*PERF_SETTINGS[i].ptr, 4).c_str());
      handled = true;
    }
  }
  // Changing the speed source (STW↔SOG) invalidates every learned point → Clear-All (spec §1/§4;
  // the dashboard warns first). Both fronts + raw history reset.
  if (request->hasParam("perfSpeedSrc") && perfSpeedSrc != oldSpeedSrc) {
    resetBoatPerformance();
    queueConsoleMessage("BoatPerf: speed source changed → Clear All (front reset)");
  }
  // Action (not a float knob): LEARNED↔FIXED source toggle.
  if (request->hasParam("perfSource")) {   // 1 = FIXED (freeze + pause), 0 = LEARNED (resume)
    int src = request->getParam("perfSource")->value().toInt();
    sailFront.source = motorFront.source = (uint8_t)(src ? 1 : 0);
    perfPaused = src ? 1.0f : 0.0f;
    settingWrite(NK_perfPaused, String(perfPaused, 4).c_str());
    handled = true;
  }
  return handled;
}
void sendPerfSettings() {
  char buf[256];
  int off = 0;
  for (size_t i = 0; i < PERF_SETTING_COUNT; i++)
    off += snprintf(buf + off, sizeof(buf) - off, (i ? ",%.4f" : "%.4f"), *PERF_SETTINGS[i].ptr);
  events.send(buf, "PerfSettings");
}

// Self-describing schema (served at /perfschema). The dashboard fetches this ONCE on load
// and zips these names against the PerfLive / PerfSettings payload values — so it never keeps
// its own field array and can't fall out of sync with the firmware tables above.
String perfSchemaJson() {
  String s = "{\"live\":[";
  for (size_t i = 0; i < PERF_LIVE_COUNT; i++) {
    if (i) s += ",";
    s += "\""; s += PERF_LIVE[i].name; s += "\"";
  }
  s += "],\"motorLive\":[";
  for (size_t i = 0; i < PERF_MOTOR_LIVE_COUNT; i++) {
    if (i) s += ",";
    s += "\""; s += PERF_MOTOR_LIVE[i].name; s += "\"";
  }
  s += "],\"settings\":[";
  for (size_t i = 0; i < PERF_SETTING_COUNT; i++) {
    if (i) s += ",";
    s += "\""; s += PERF_SETTINGS[i].name; s += "\"";
  }
  s += "]}";
  return s;
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

  e.capReason = g_fastOvCapReason;  // which layer set the binding fastOvCap this tick

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
  // over2x is per-sample vs the running mean — the SAME test feeds the 1s mini-bucket
  // below, so the 10s / 2m / all-time over-counts are all on the same footing.
  bool isOver2x = false;
  if (ch1AtCount > 1) {
    float runMean = (float)((double)ch1AtSum / ch1AtCount);
    if ((float)iv > runMean * 2.0f) { ch1AtOver2x++; isOver2x = true; }
  }

  // ── 1s mini-bucket: incremental update, O(1), no ring scan ────────────
  ch1Bkt1sCurrent.sum += iv;
  ch1Bkt1sCurrent.count++;
  if (iv > ch1Bkt1sCurrent.worst) ch1Bkt1sCurrent.worst = iv;
  if (isOver2x) ch1Bkt1sCurrent.over2x++;

  // 1s rollover: close current mini-bucket, open a new one
  if (now - ch1Bkt1sStart >= 1000UL) {
    ch1Bkt1s[ch1Bkt1sHead] = ch1Bkt1sCurrent;
    ch1Bkt1sHead = (ch1Bkt1sHead + 1) % CH1_1S_BUCKETS;
    if (ch1Bkt1sCount < CH1_1S_BUCKETS) ch1Bkt1sCount++;
    ch1Bkt1sCurrent = { 0, 0, 0, 0 };
    ch1Bkt1sStart = now;
  }

  // ── 10s→2m bucket rollover: O(10) mini-bucket sum, no ring scan ────────
  // over2x carries forward the per-sample counts accumulated in the 1s mini-buckets
  if (now - ch1BktStart >= 10000UL) {
    Ch1Bucket bkt = { 0, 0, 0, 0 };

    // Sum all closed 1s mini-buckets
    for (uint8_t i = 0; i < ch1Bkt1sCount; i++) {
      uint8_t idx = (ch1Bkt1sHead + CH1_1S_BUCKETS - 1 - i) % CH1_1S_BUCKETS;
      bkt.sum += ch1Bkt1s[idx].sum;
      bkt.count += ch1Bkt1s[idx].count;
      if (ch1Bkt1s[idx].worst > bkt.worst) bkt.worst = ch1Bkt1s[idx].worst;
      bkt.over2x += ch1Bkt1s[idx].over2x;  // per-sample over-2× counts
    }
    // Include currently open mini-bucket
    bkt.sum += ch1Bkt1sCurrent.sum;
    bkt.count += ch1Bkt1sCurrent.count;
    if (ch1Bkt1sCurrent.worst > bkt.worst) bkt.worst = ch1Bkt1sCurrent.worst;
    bkt.over2x += ch1Bkt1sCurrent.over2x;

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
  uint32_t ch1_o10 = ch1Bkt1sCurrent.over2x;
  uint32_t sum10 = ch1Bkt1sCurrent.sum;

  for (uint8_t i = 0; i < ch1Bkt1sCount; i++) {
    uint8_t idx = (ch1Bkt1sHead + CH1_1S_BUCKETS - 1 - i) % CH1_1S_BUCKETS;
    sum10 += ch1Bkt1s[idx].sum;
    ch1_n_10s += ch1Bkt1s[idx].count;
    ch1_o10 += ch1Bkt1s[idx].over2x;
    if (ch1Bkt1s[idx].worst > ch1_worst_10s) ch1_worst_10s = ch1Bkt1s[idx].worst;
  }
  ch1_over2x_10s = (ch1_o10 > 65535u) ? 65535u : (uint16_t)ch1_o10;  // per-sample count over the 10s window
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
// Inner Current PID Firing Interval — field-on-gated clone of ch1_record/_compute_stats.
// Called once per normal control tick (field driven). Globals live in Xregulator.ino.
// ─────────────────────────────────────────────────────────────────────────────
void pidFire_record(uint32_t now) {
  if (!pfHasPrev) {
    pfPrevTs = now;
    pfBktStart = now;
    pfBkt1sStart = now;
    pfHasPrev = true;
    return;
  }

  uint32_t diff = now - pfPrevTs;
  pfPrevTs = now;
  uint16_t iv = (diff > 65535u) ? 65535u : (uint16_t)diff;
  pf_last_ms = iv;

  // ── All-time accumulators ─────────────────────────────────────────────
  pfAtCount++;
  pfAtSum += iv;
  if (iv > pfAtWorst) pfAtWorst = iv;
  // over-2× is per-sample vs the running mean — the SAME test feeds the 1s mini-bucket
  // below, so the 10s / 2m / all-time over-counts are all on the same footing.
  bool isOver2x = false;
  if (pfAtCount > 1) {
    float runMean = (float)((double)pfAtSum / pfAtCount);
    if ((float)iv > runMean * 2.0f) { pfAtOver2x++; isOver2x = true; }
  }

  // ── 1s mini-bucket ────────────────────────────────────────────────────
  pfBkt1sCurrent.sum += iv;
  pfBkt1sCurrent.count++;
  if (iv > pfBkt1sCurrent.worst) pfBkt1sCurrent.worst = iv;
  if (isOver2x) pfBkt1sCurrent.over2x++;
  if (now - pfBkt1sStart >= 1000UL) {
    pfBkt1s[pfBkt1sHead] = pfBkt1sCurrent;
    pfBkt1sHead = (pfBkt1sHead + 1) % PF_1S_BUCKETS;
    if (pfBkt1sCount < PF_1S_BUCKETS) pfBkt1sCount++;
    pfBkt1sCurrent = { 0, 0, 0, 0 };
    pfBkt1sStart = now;
  }

  // ── 10s→2m bucket rollover ────────────────────────────────────────────
  if (now - pfBktStart >= 10000UL) {
    Ch1Bucket bkt = { 0, 0, 0, 0 };
    for (uint8_t i = 0; i < pfBkt1sCount; i++) {
      uint8_t idx = (pfBkt1sHead + PF_1S_BUCKETS - 1 - i) % PF_1S_BUCKETS;
      bkt.sum += pfBkt1s[idx].sum;
      bkt.count += pfBkt1s[idx].count;
      if (pfBkt1s[idx].worst > bkt.worst) bkt.worst = pfBkt1s[idx].worst;
      bkt.over2x += pfBkt1s[idx].over2x;
    }
    bkt.sum += pfBkt1sCurrent.sum;
    bkt.count += pfBkt1sCurrent.count;
    if (pfBkt1sCurrent.worst > bkt.worst) bkt.worst = pfBkt1sCurrent.worst;
    bkt.over2x += pfBkt1sCurrent.over2x;
    pfBuckets[pfBktHead] = bkt;
    pfBktHead = (pfBktHead + 1) % PF_BUCKETS;
    if (pfBktCount < PF_BUCKETS) pfBktCount++;
    pfBktStart = now;
  }
}

void pidFire_compute_stats() {
  if (pfAtCount == 0 && pfBkt1sCount == 0 && pfBkt1sCurrent.count == 0) return;

  // ── 10s window: open 1s bucket + closed 1s buckets ────────────────────
  pf_worst_10s = pfBkt1sCurrent.worst;
  uint32_t n10 = pfBkt1sCurrent.count;
  uint32_t sum10 = pfBkt1sCurrent.sum;
  uint32_t o10 = pfBkt1sCurrent.over2x;
  for (uint8_t i = 0; i < pfBkt1sCount; i++) {
    uint8_t idx = (pfBkt1sHead + PF_1S_BUCKETS - 1 - i) % PF_1S_BUCKETS;
    sum10 += pfBkt1s[idx].sum;
    n10 += pfBkt1s[idx].count;
    o10 += pfBkt1s[idx].over2x;
    if (pfBkt1s[idx].worst > pf_worst_10s) pf_worst_10s = pfBkt1s[idx].worst;
  }
  pf_avg_10s = (n10 > 0) ? (float)sum10 / (float)n10 : 0.0f;
  pf_over2x_10s = (o10 > 65535u) ? 65535u : (uint16_t)o10;  // per-sample count over the 10s window

  // ── 2m window: closed 10s buckets ─────────────────────────────────────
  pf_worst_2m = 0;
  pf_over2x_2m = 0;
  uint32_t n2m = 0;
  uint64_t sum2m = 0;
  for (uint8_t i = 0; i < pfBktCount; i++) {
    uint8_t idx = (pfBktHead + PF_BUCKETS - 1 - i) % PF_BUCKETS;
    n2m += pfBuckets[idx].count;
    sum2m += pfBuckets[idx].sum;
    pf_over2x_2m += pfBuckets[idx].over2x;
    if (pfBuckets[idx].worst > pf_worst_2m) pf_worst_2m = pfBuckets[idx].worst;
  }
  pf_avg_2m = (n2m > 0) ? (float)sum2m / (float)n2m : 0.0f;

  // ── All-time ──────────────────────────────────────────────────────────
  pf_worst_at = pfAtWorst;
  pf_avg_at = pfAtCount > 0 ? (float)((double)pfAtSum / pfAtCount) : 0.0f;
  pf_over2x_at = pfAtOver2x;
}

// ─────────────────────────────────────────────────────────────────────────────
// CV Voltage Loop Firing Interval — CV-mode-gated clone of pidFire_record/_compute_stats.
// Called once per CV fire. Globals live in Xregulator.ino. The control loop clears
// vlHasPrev when CV is inactive, so re-entering CV re-baselines instead of logging the gap.
// ─────────────────────────────────────────────────────────────────────────────
void voltLoop_record(uint32_t now) {
  if (!vlHasPrev) {
    vlPrevTs = now;
    vlBktStart = now;
    vlBkt1sStart = now;
    vlHasPrev = true;
    return;
  }

  uint32_t diff = now - vlPrevTs;
  vlPrevTs = now;
  uint16_t iv = (diff > 65535u) ? 65535u : (uint16_t)diff;
  vl_last_ms = iv;

  // ── All-time accumulators ─────────────────────────────────────────────
  vlAtCount++;
  vlAtSum += iv;
  if (iv > vlAtWorst) vlAtWorst = iv;
  bool isOver2x = false;
  if (vlAtCount > 1) {
    float runMean = (float)((double)vlAtSum / vlAtCount);
    if ((float)iv > runMean * 2.0f) { vlAtOver2x++; isOver2x = true; }
  }

  // ── 1s mini-bucket ────────────────────────────────────────────────────
  vlBkt1sCurrent.sum += iv;
  vlBkt1sCurrent.count++;
  if (iv > vlBkt1sCurrent.worst) vlBkt1sCurrent.worst = iv;
  if (isOver2x) vlBkt1sCurrent.over2x++;
  if (now - vlBkt1sStart >= 1000UL) {
    vlBkt1s[vlBkt1sHead] = vlBkt1sCurrent;
    vlBkt1sHead = (vlBkt1sHead + 1) % VL_1S_BUCKETS;
    if (vlBkt1sCount < VL_1S_BUCKETS) vlBkt1sCount++;
    vlBkt1sCurrent = { 0, 0, 0, 0 };
    vlBkt1sStart = now;
  }

  // ── 10s→2m bucket rollover ────────────────────────────────────────────
  if (now - vlBktStart >= 10000UL) {
    Ch1Bucket bkt = { 0, 0, 0, 0 };
    for (uint8_t i = 0; i < vlBkt1sCount; i++) {
      uint8_t idx = (vlBkt1sHead + VL_1S_BUCKETS - 1 - i) % VL_1S_BUCKETS;
      bkt.sum += vlBkt1s[idx].sum;
      bkt.count += vlBkt1s[idx].count;
      if (vlBkt1s[idx].worst > bkt.worst) bkt.worst = vlBkt1s[idx].worst;
      bkt.over2x += vlBkt1s[idx].over2x;
    }
    bkt.sum += vlBkt1sCurrent.sum;
    bkt.count += vlBkt1sCurrent.count;
    if (vlBkt1sCurrent.worst > bkt.worst) bkt.worst = vlBkt1sCurrent.worst;
    bkt.over2x += vlBkt1sCurrent.over2x;
    vlBuckets[vlBktHead] = bkt;
    vlBktHead = (vlBktHead + 1) % VL_BUCKETS;
    if (vlBktCount < VL_BUCKETS) vlBktCount++;
    vlBktStart = now;
  }
}

void voltLoop_compute_stats() {
  if (vlAtCount == 0 && vlBkt1sCount == 0 && vlBkt1sCurrent.count == 0) return;

  // ── 10s window: open 1s bucket + closed 1s buckets ────────────────────
  vl_worst_10s = vlBkt1sCurrent.worst;
  uint32_t n10 = vlBkt1sCurrent.count;
  uint32_t sum10 = vlBkt1sCurrent.sum;
  uint32_t o10 = vlBkt1sCurrent.over2x;
  for (uint8_t i = 0; i < vlBkt1sCount; i++) {
    uint8_t idx = (vlBkt1sHead + VL_1S_BUCKETS - 1 - i) % VL_1S_BUCKETS;
    sum10 += vlBkt1s[idx].sum;
    n10 += vlBkt1s[idx].count;
    o10 += vlBkt1s[idx].over2x;
    if (vlBkt1s[idx].worst > vl_worst_10s) vl_worst_10s = vlBkt1s[idx].worst;
  }
  vl_avg_10s = (n10 > 0) ? (float)sum10 / (float)n10 : 0.0f;
  vl_over2x_10s = (o10 > 65535u) ? 65535u : (uint16_t)o10;

  // ── 2m window: closed 10s buckets ─────────────────────────────────────
  vl_worst_2m = 0;
  vl_over2x_2m = 0;
  uint32_t n2m = 0;
  uint64_t sum2m = 0;
  for (uint8_t i = 0; i < vlBktCount; i++) {
    uint8_t idx = (vlBktHead + VL_BUCKETS - 1 - i) % VL_BUCKETS;
    n2m += vlBuckets[idx].count;
    sum2m += vlBuckets[idx].sum;
    vl_over2x_2m += vlBuckets[idx].over2x;
    if (vlBuckets[idx].worst > vl_worst_2m) vl_worst_2m = vlBuckets[idx].worst;
  }
  vl_avg_2m = (n2m > 0) ? (float)sum2m / (float)n2m : 0.0f;

  // ── All-time ──────────────────────────────────────────────────────────
  vl_worst_at = vlAtWorst;
  vl_avg_at = vlAtCount > 0 ? (float)((double)vlAtSum / vlAtCount) : 0.0f;
  vl_over2x_at = vlAtOver2x;
}

// ─────────────────────────────────────────────────────────────────────────────
// INA228 fast-mode interval tracking
// Mirrors CH1 interval stats. Only updated when inaFastModeActive.
// resetINA228IntervalWindows() clears 10s/2m windows; all-time persists.
// ─────────────────────────────────────────────────────────────────────────────
struct InaMiniB  { uint32_t sum; uint32_t count; uint16_t worst; uint16_t over2x; };
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
  bool isOver2x = false;
  if (inaAtCount > 1) {
    // Bias correction: compute mean of prior samples only, otherwise a huge
    // outlier inflates its own mean and fails the > 2× test against itself.
    float runMean = (float)((double)(inaAtSum - iv) / (inaAtCount - 1));
    if ((float)iv > runMean * 2.0f) { inaAtOver2x++; isOver2x = true; }
  }

  // 1s mini-bucket (over2x counted per-sample so 10s / 2m / all-time agree)
  ina1sCur.sum += iv;
  ina1sCur.count++;
  if (iv > ina1sCur.worst) ina1sCur.worst = iv;
  if (isOver2x) ina1sCur.over2x++;

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
      bkt.over2x += ina1sB[idx].over2x;  // per-sample over-2× counts
    }
    bkt.sum   += ina1sCur.sum;
    bkt.count += ina1sCur.count;
    if (ina1sCur.worst > bkt.worst) bkt.worst = ina1sCur.worst;
    bkt.over2x += ina1sCur.over2x;
    ina2mB[ina2mHead] = bkt;
    ina2mHead = (ina2mHead + 1) % INA_BUCKETS;
    if (ina2mCount < INA_BUCKETS) ina2mCount++;
    ina2mStart = now;
  }

  // Publish 10s stats
  uint32_t sum10 = ina1sCur.sum, n10 = ina1sCur.count, o10 = ina1sCur.over2x;
  ina_worst_10s = ina1sCur.worst;
  for (uint8_t i = 0; i < ina1sCount; i++) {
    uint8_t idx = (ina1sHead + INA_1S_BUCKETS - 1 - i) % INA_1S_BUCKETS;
    sum10 += ina1sB[idx].sum;
    n10   += ina1sB[idx].count;
    o10   += ina1sB[idx].over2x;
    if (ina1sB[idx].worst > ina_worst_10s) ina_worst_10s = ina1sB[idx].worst;
  }
  ina_avg_10s    = (n10 > 0) ? (float)sum10 / (float)n10 : 0.0f;
  ina_over2x_10s = (o10 > 65535u) ? 65535u : (uint16_t)o10;  // per-sample count over the 10s window

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