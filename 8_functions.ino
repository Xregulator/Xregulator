// ── 8_functions.ino ── Fast alternator-current failure detector (consumer 2) algorithm bodies,
// Config Sharing manifest, Battery Health monitor (DCIR + capacity), /debug/fillmax endpoints.

#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>

// Tuning constants (rect_fault_detector.py TUNING — rationale comments live there).
// hump_merge_frac is not listed: the picker rungs below carry their own (window, frac).
#define FAD_SMOOTH_N 3
#define FAD_ROUGH_MS 4.0f
#define FAD_REGIME_CYCLE_MS 2.5f
#define FAD_DUPE_IV_FRAC 0.35f
#define FAD_DUPE_FRAC_MAX 0.08f
#define FAD_HEIGHT_REF_FRAC 0.55f
#define FAD_WEAK_KEEP_FRAC 0.4f
#define FAD_ENV_CYCLES 2.0f
#define FAD_BLOCK_CRESTS 48
#define FAD_F_SIG 2.5f
#define FAD_TH_INT 0.15f
#define FAD_TH_H 0.70f
#define FAD_TREND_INT 0.08f
#define FAD_TREND_H 0.45f
#define FAD_ACF_CONFIRM 1.25f
#define FAD_WINNER_PREF 0.7f
#define FAD_GAP_THR 1.45f
#define FAD_GAP_REF_PCT 25.0f
#define FAD_MIN_GAPS 12
#define FAD_SEG_FRAC_MIN 0.35f
#define FAD_MIN_MODAL_SEGS 20
#define FAD_SYNC_PARTNER_MIN 0.5f

#define FAD_ACF_LAG_CAP 400  // regime/sync ACF lag ceiling (min(400, n/4) in the prototype)
#define FAD_SEG_SAMPLES 8192 // reduction chunk: each block sums into a float partial, then folds into a double — preserves the accumulation order the verdict thresholds were tuned against

// ---- small numeric helpers ----

// k-th smallest, Hoare quickselect with median-of-3 pivot. On return a[k] is the k-th
// order statistic and a[] is partitioned ≤/≥ around it (the percentile interpolation
// below relies on that to fetch the next order statistic with a linear scan).
static float fadSelect(float *a, int n, int k) {
  int lo = 0, hi = n - 1;
  while (lo < hi) {
    int mid = lo + (hi - lo) / 2;
    float p0 = a[lo], p1 = a[mid], p2 = a[hi];
    float piv = (p0 < p1) ? ((p1 < p2) ? p1 : (p0 < p2 ? p2 : p0))
                          : ((p0 < p2) ? p0 : ((p1 < p2) ? p2 : p1));
    int i = lo, j = hi;
    while (i <= j) {
      while (a[i] < piv) i++;
      while (a[j] > piv) j--;
      if (i <= j) {
        float t = a[i]; a[i] = a[j]; a[j] = t;
        i++; j--;
      }
    }
    if (k <= j) hi = j;
    else if (k >= i) lo = i;
    else return a[k];
  }
  return a[lo];
}

// numpy 'linear' percentile; MUTATES a
static float fadPercentile(float *a, int n, float q) {
  if (n <= 0) return 0.0f;
  if (n == 1) return a[0];
  double pos = (double)(n - 1) * (double)q / 100.0;
  int i = (int)pos;
  if (i >= n - 1) return fadSelect(a, n, n - 1);
  double frac = pos - (double)i;
  float v1 = fadSelect(a, n, i);
  float v2 = a[i + 1];
  for (int t = i + 2; t < n; t++)
    if (a[t] < v2) v2 = a[t];
  if (frac <= 0.0) return v1;
  return (float)((double)v1 + ((double)v2 - (double)v1) * frac);
}
static inline float fadMedian(float *a, int n) { return fadPercentile(a, n, 50.0f); }

// One-way ANOVA by index mod k → (F, bias-corrected RMS effect size). n is small
// (48-crest blocks / modal-segment concatenations), so double accumulation is cheap.
static void fadAnova(const float *vals, int n, int k, float *Fout, float *effOut) {
  if (n < 2 * k) { *Fout = 0.0f; *effOut = 0.0f; return; }
  double sum[8] = { 0 }, cnt[8] = { 0 };
  for (int i = 0; i < n; i++) {
    sum[i % k] += vals[i];
    cnt[i % k] += 1.0;
  }
  double means[8], grand = 0.0;
  for (int g = 0; g < k; g++) {
    means[g] = sum[g] / cnt[g];
    grand += sum[g];
  }
  grand /= (double)n;
  double ssw = 0.0;
  for (int i = 0; i < n; i++) {
    double d = (double)vals[i] - means[i % k];
    ssw += d * d;
  }
  double msw = ssw / (double)(n - k);
  double ssb = 0.0;
  for (int g = 0; g < k; g++) {
    double d = means[g] - grand;
    ssb += cnt[g] * d * d;
  }
  double msb = ssb / (double)(k - 1);
  double e = (msb - msw) / ((double)n / (double)k);
  *effOut = (float)((e > 0.0) ? sqrt(e) : 0.0);
  *Fout = (float)(msb / ((msw > 1e-12) ? msw : 1e-12));
}

static void fadParabolic(const float *y, int i, float *t, float *h) {
  float den = y[i - 1] - 2.0f * y[i] + y[i + 1];
  if (den < 0.0f) {
    float dt = 0.5f * (y[i - 1] - y[i + 1]) / den;
    if (dt > 0.5f) dt = 0.5f;
    if (dt < -0.5f) dt = -0.5f;
    *t = (float)i + dt;
    *h = y[i] - 0.25f * (y[i - 1] - y[i + 1]) * dt;
  } else {
    *t = (float)i;
    *h = y[i];
  }
}

// centered moving average from the prefix array, numpy edge-shrink semantics
static inline float fadMov(const double *pre, int n, int win, int i) {
  int h = win / 2;
  int lo = i - h;
  if (lo < 0) lo = 0;
  int hi = i + (win - h);
  if (hi > n) hi = n;
  return (float)((pre[hi] - pre[lo]) / (double)(hi - lo));
}

// smallest k with D[k] ≥ 0.7·max, runner = best of the others (floored)
static void fadWinner(const float *D, int *kOut, float *dOut, float *runOut) {
  float dmax = D[2];
  for (int k = 3; k <= 8; k++)
    if (D[k] > dmax) dmax = D[k];
  int kw = 2;
  for (int k = 2; k <= 8; k++)
    if (D[k] >= FAD_WINNER_PREF * dmax) { kw = k; break; }
  float runner = dmax * 1e-3f;
  if (runner < 1e-9f) runner = 1e-9f;
  for (int k = 2; k <= 8; k++)
    if (k != kw && D[k] > runner) runner = D[k];
  *kOut = kw;
  *dOut = D[kw];
  *runOut = runner;
}

// Lay the workspace out inside one externally allocated block (PSRAM on target, malloc on
// the desktop harness). mem == NULL → just return the byte count needed for window n.
static size_t fadCarve(FadJob *J,
                       uint8_t *mem, int n) {
  int crestCap = n / 3 + 16;
  int blocksCap = crestCap / FAD_BLOCK_CRESTS + 2;
  int rowsCap = 32768;
  size_t off = 0;
#define FAD_TAKE(ptr, type, count) \
  do { \
    off = (off + 7u) & ~(size_t)7u; \
    if (mem) J->ptr = (type *)(mem + off); \
    off += sizeof(type) * (size_t)(count); \
  } while (0)
  FAD_TAKE(pre, double, n + 1);
  FAD_TAKE(xf, float, n);
  FAD_TAKE(s, float, n);
  FAD_TAKE(rr, float, n);
  FAD_TAKE(env, float, n);
  FAD_TAKE(acf, float, FAD_ACF_LAG_CAP);
  FAD_TAKE(idx, int32_t, crestCap);
  FAD_TAKE(kept, int32_t, crestCap);
  FAD_TAKE(gpos, int32_t, crestCap);
  FAD_TAKE(instA, int32_t, crestCap);
  FAD_TAKE(instB, int32_t, crestCap);
  FAD_TAKE(cnts, int32_t, crestCap);
  FAD_TAKE(tt, float, crestCap);
  FAD_TAKE(hh, float, crestCap);
  FAD_TAKE(hn, float, crestCap);
  FAD_TAKE(iv, float, crestCap);
  FAD_TAKE(dn, float, crestCap);
  FAD_TAKE(scratch, float, crestCap);
  FAD_TAKE(rowsH, float, rowsCap);
  FAD_TAKE(rowsI, float, rowsCap);
  FAD_TAKE(blkStats, float, 4 * 7 * blocksCap);
#undef FAD_TAKE
  if (mem) {
    J->n = n;
    J->crestCap = crestCap;
    J->blocksCap = blocksCap;
    J->rowsCap = rowsCap;
  }
  return off;
}

// Arm a job. src == NULL means the caller pre-filled J->xf (desktop harness path).
static void fadStart(FadJob *J,
                     const int16_t *src, int n, float fs) {
  J->src = src;
  J->n = n;
  J->fs = fs;
  J->haveBest = 0;
  J->bestSane = 0;
  J->bestHeadroom = 0.0f;
  memset(&J->best, 0, sizeof(J->best));
  J->idxOverflow = 0;
}

// Straight-line detector — runs to completion on the Core-0 worker. Reductions keep the chunked
// float-partial->double accumulation (FAD_SEG_SAMPLES) so results stay bit-identical to what the
// verdict thresholds were tuned against; do not flatten them. Rationale + validation:
// Fault_Detector_Dev_Summary.md.

// Crest pick: maxima -> local-contrast threshold -> valley-depth merge. -1 = too few maxima.
static int fadPick(FadJob *J, float wMs, float frac) {
  const float *r = J->rr;
  int n = J->n;
  J->nIdx = 0;
  for (int i = 1; i <= n - 2; i++)
    if (r[i] > r[i - 1] && r[i] >= r[i + 1]) {
      if (J->nIdx >= J->crestCap) { J->idxOverflow = 1; return -1; }
      J->idx[J->nIdx++] = i;
    }
  if (J->nIdx < 32) return -1;
  // per-window pk-pk into scratch (numpy reshape semantics)
  int w = (int)llrintf(wMs * 1e-3f * J->fs);
  if (w < 8) w = 8;
  int nw = n / w;
  for (int b = 0; b < nw; b++) {
    const float *seg = r + b * w;
    float mn = seg[0], mx = seg[0];
    for (int q = 1; q < w; q++) { if (seg[q] < mn) mn = seg[q]; if (seg[q] > mx) mx = seg[q]; }
    J->scratch[b] = mx - mn;
  }
  float mergeDelta = frac * fadPercentile(J->scratch, nw, 25.0f);
  // valley-depth merge; vmin/scanFrom maintain an incremental min over [kept[last], i]
  J->nKept = 1;
  J->kept[0] = J->idx[0];
  float vmin = r[J->idx[0]];
  int scanFrom = J->idx[0] + 1;
  for (int p = 1; p < J->nIdx; p++) {
    int i = J->idx[p];
    int j = J->kept[J->nKept - 1];
    for (int q = scanFrom; q <= i; q++) if (r[q] < vmin) vmin = r[q];
    scanFrom = i + 1;
    float lower = (r[j] < r[i]) ? r[j] : r[i];
    if (vmin > lower - mergeDelta) {
      if (r[i] > r[j]) { J->kept[J->nKept - 1] = i; vmin = r[i]; }  // merged: keep the taller
    } else {
      J->kept[J->nKept++] = i; vmin = r[i];  // real valley: new crest
    }
  }
  int niv = J->nKept - 1;
  if (niv < 16) return -1;
  for (int q = 0; q < niv; q++) J->iv[q] = (float)(J->kept[q + 1] - J->kept[q]);
  memcpy(J->scratch, J->iv, sizeof(float) * (size_t)niv);
  float p90 = fadPercentile(J->scratch, niv, 90.0f);
  int dup = 0;
  for (int q = 0; q < niv; q++) if (J->iv[q] < FAD_DUPE_IV_FRAC * p90) dup++;
  J->pickSane = ((float)dup / (float)niv <= FAD_DUPE_FRAC_MAX) ? 1 : 0;
  return 1;
}

// One rung: pick -> weak-filter -> subtrain -> features -> block ANOVA -> sync -> confirm ->
// verdict. Keeps the best rung; a pick fail / too-short train skips it. Updates J->best.
static void fadRung(FadJob *J, float wMs, float frac) {
  const float *r = J->rr;
  int n = J->n;
  if (fadPick(J, wMs, frac) < 0) return;
  J->sane2 = J->pickSane;

  // tall-height reference over ALL raw maxima
  for (int q = 0; q < J->nIdx; q++) J->scratch[q] = r[J->idx[q]];
  float tallThr = FAD_HEIGHT_REF_FRAC * fadPercentile(J->scratch, J->nIdx, 90.0f);
  // weak floor: median of kept crests at/above the tall threshold (fallback: all kept)
  int wkNt = 0;
  for (int q = 0; q < J->nKept; q++) { float kh = r[J->kept[q]]; if (kh >= tallThr) J->scratch[wkNt++] = kh; }
  if (wkNt > 0) {
    J->medTall = fadMedian(J->scratch, wkNt);
  } else {
    for (int q = 0; q < J->nKept; q++) J->scratch[q] = r[J->kept[q]];
    J->medTall = fadMedian(J->scratch, J->nKept);
  }
  // drop crests below the weak-keep floor (in-place forward compaction)
  float floorv = FAD_WEAK_KEEP_FRAC * J->medTall;
  int wkNk = 0;
  for (int q = 0; q < J->nKept; q++) if (r[J->kept[q]] >= floorv) J->kept[wkNk++] = J->kept[q];
  J->nKept = wkNk;
  if (J->nKept < 2 * FAD_BLOCK_CRESTS) return;

  // subtrain: raw maxima not kept, nearest-kept distance via two sorted pointers
  J->subtrain = 0.0f;
  {
    int nSup = J->nIdx - J->nKept;
    if (nSup > 0 && (double)nSup * (double)J->nKept < 4e7) {
      int subCnt = 0, subKp = 0, subQ = 0;
      float hFloor = 0.3f * J->medTall, dFloor = 0.3f * J->P;
      for (int p = 0; p < J->nIdx; p++) {
        int e = J->idx[p];
        if (subQ < J->nKept && J->kept[subQ] == e) { subQ++; continue; }  // member of kept
        while (subKp + 1 < J->nKept && J->kept[subKp + 1] <= e) subKp++;
        int d1 = e - J->kept[subKp]; if (d1 < 0) d1 = -d1;
        int d2 = (subKp + 1 < J->nKept) ? J->kept[subKp + 1] - e : d1; if (d2 < 0) d2 = -d2;
        int dNear = (d1 < d2) ? d1 : d2;
        if (r[e] >= hFloor && (float)dNear >= dFloor) subCnt++;
      }
      J->subtrain = (float)subCnt / (float)J->nKept;
    }
  }

  // parabolic features (tt, hh) -> hn, iv, dn
  for (int q = 0; q < J->nKept; q++) {
    int i = J->kept[q];
    if (i >= 1 && i <= n - 2) fadParabolic(r, i, &J->tt[q], &J->hh[q]);
    else { J->tt[q] = (float)i; J->hh[q] = r[i]; }
    J->hn[q] = J->hh[q] / J->env[i];
  }
  memcpy(J->scratch, J->hn, sizeof(float) * (size_t)J->nKept);
  J->scaleH = fadMedian(J->scratch, J->nKept);
  int niv = J->nKept - 1;
  for (int q = 0; q < niv; q++) J->iv[q] = J->tt[q + 1] - J->tt[q];
  memcpy(J->scratch, J->iv, sizeof(float) * (size_t)niv);
  J->medIv = fadMedian(J->scratch, niv);
  for (int q = 0; q < niv; q++) J->dn[q] = J->iv[q] / J->medIv;

  // modulo-k ANOVA per 48-crest block, medians over blocks
  int nBlkH = J->nKept / FAD_BLOCK_CRESTS;
  int nBlkI = (J->nKept - 1) / FAD_BLOCK_CRESTS;
  if (nBlkH > J->blocksCap) nBlkH = J->blocksCap;
  if (nBlkI > J->blocksCap) nBlkI = J->blocksCap;
  int nBlkMax = (nBlkH > nBlkI) ? nBlkH : nBlkI;
  float sh = (J->scaleH > 1e-9f) ? J->scaleH : 1e-9f;
  int plane = J->blocksCap;
  for (int b = 0; b < nBlkMax; b++)
    for (int k = 2; k <= 8; k++) {
      if (b < nBlkH) {
        float F, e;
        fadAnova(J->hn + b * FAD_BLOCK_CRESTS, FAD_BLOCK_CRESTS, k, &F, &e);
        J->blkStats[((0 * 7) + (k - 2)) * plane + b] = e / sh;
        J->blkStats[((1 * 7) + (k - 2)) * plane + b] = F;
      }
      if (b < nBlkI) {
        float F, e;
        fadAnova(J->dn + b * FAD_BLOCK_CRESTS, FAD_BLOCK_CRESTS, k, &F, &e);
        J->blkStats[((2 * 7) + (k - 2)) * plane + b] = e;
        J->blkStats[((3 * 7) + (k - 2)) * plane + b] = F;
      }
    }
  for (int k = 2; k <= 8; k++) {
    memcpy(J->scratch, J->blkStats + ((0 * 7) + (k - 2)) * plane, sizeof(float) * (size_t)nBlkH);
    J->Dh[k] = fadMedian(J->scratch, nBlkH);
    memcpy(J->scratch, J->blkStats + ((1 * 7) + (k - 2)) * plane, sizeof(float) * (size_t)nBlkH);
    J->Fh[k] = fadMedian(J->scratch, nBlkH);
    memcpy(J->scratch, J->blkStats + ((2 * 7) + (k - 2)) * plane, sizeof(float) * (size_t)nBlkI);
    J->Di[k] = fadMedian(J->scratch, nBlkI);
    memcpy(J->scratch, J->blkStats + ((3 * 7) + (k - 2)) * plane, sizeof(float) * (size_t)nBlkI);
    J->Fi[k] = fadMedian(J->scratch, nBlkI);
  }
  fadWinner(J->Dh, &J->kH, &J->dH, &J->runH);
  fadWinner(J->Di, &J->kI, &J->dI, &J->runI);
  J->syncUsed = 0;

  // gap-anchored sync: any precondition fail -> skip to CONFIRM with syncUsed=0
  float T0 = 0.0f;
  do {
    int nivg = J->nKept - 1;
    memcpy(J->scratch, J->iv, sizeof(float) * (size_t)nivg);
    J->refIv = fadPercentile(J->scratch, nivg, FAD_GAP_REF_PCT);
    J->nGap = 0;
    float thr = FAD_GAP_THR * J->refIv;
    for (int q = 0; q < nivg; q++) if (J->iv[q] > thr) J->gpos[J->nGap++] = q;
    J->lagLo = (int)(1.6f * J->refIv); if (J->lagLo < 4) J->lagLo = 4;
    J->lagHi = n / 4; if (J->lagHi > FAD_ACF_LAG_CAP) J->lagHi = FAD_ACF_LAG_CAP;
    if (J->nGap < FAD_MIN_GAPS || J->lagLo > J->lagHi) break;
    // sync ACF over lags lagLo..lagHi of rn (= J->s)
    const float *rn = J->s;
    float m = (float)J->rnMean;
    if (J->rnVar <= 0.0) {
      for (int l = J->lagLo; l <= J->lagHi; l++) J->acf[l - J->lagLo] = 0.0f;
    } else {
      for (int lag = J->lagLo; lag <= J->lagHi; lag++) {
        int len = n - lag; double acc = 0.0;
        for (int seg = 0; seg < len; seg += FAD_SEG_SAMPLES) {
          int end = seg + FAD_SEG_SAMPLES; if (end > len) end = len;
          float part = 0.0f; const float *a = rn + seg, *b = rn + seg + lag; int cnt = end - seg;
          for (int i = 0; i < cnt; i++) part += (a[i] - m) * (b[i] - m);
          acc += (double)part;
        }
        J->acf[lag - J->lagLo] = (float)(acc / (double)len / J->rnVar);
      }
    }
    // sync ACF peak -> matched period T0
    int mm0 = J->lagHi - J->lagLo + 1;
    int bestP = -1;
    for (int p = 1; p < mm0 - 1; p++)
      if (J->acf[p] > J->acf[p - 1] && J->acf[p] >= J->acf[p + 1])
        if (bestP < 0 || J->acf[p] > J->acf[bestP]) bestP = p;
    if (bestP < 0 || J->acf[bestP] < 0.5f) break;
    T0 = (float)(J->lagLo + bestP);
    // match each gap to its one-period-later partner (+/-10%)
    int nInst = 0, mtOkc = 0;
    for (int j = 0; j < J->nGap; j++) {
      float target = J->tt[J->gpos[j]] + T0;
      int lo = 0, hi = J->nGap;
      while (lo < hi) { int mid = (lo + hi) / 2; if (J->tt[J->gpos[mid]] < target) lo = mid + 1; else hi = mid; }
      int bestC = -1; float bd = T0;
      for (int t = 0; t < 2; t++) {
        int c = lo - 1 + t;
        if (c >= 0 && c < J->nGap && c > j) { float d = fabsf(J->tt[J->gpos[c]] - target); if (d < bd) { bestC = c; bd = d; } }
      }
      if (bestC >= 0 && bd <= 0.10f * T0) { mtOkc++; J->instA[nInst] = j; J->instB[nInst] = bestC; nInst++; }
    }
    if (!((float)mtOkc / (float)J->nGap >= FAD_SYNC_PARTNER_MIN && nInst >= FAD_MIN_MODAL_SEGS)) break;
    // modal segment count via counting sort (reuses J->idx as histogram)
    for (int i = 0; i < nInst; i++) J->cnts[i] = J->gpos[J->instB[i]] - J->gpos[J->instA[i]];
    int vmn = J->cnts[0], vmx = J->cnts[0];
    for (int i = 1; i < nInst; i++) { if (J->cnts[i] < vmn) vmn = J->cnts[i]; if (J->cnts[i] > vmx) vmx = J->cnts[i]; }
    int R = vmx - vmn + 1;
    int32_t *hist = J->idx;
    for (int k = 0; k < R; k++) hist[k] = 0;
    for (int i = 0; i < nInst; i++) hist[J->cnts[i] - vmn]++;
    int mMod = 0, cMax = 0;
    for (int k = 0; k < R; k++) if (hist[k] > cMax) { cMax = hist[k]; mMod = vmn + k; }
    float frc = (float)cMax / (float)nInst;
    if (!(mMod >= 2 && mMod <= 8 && frc >= FAD_SEG_FRAC_MIN)) break;
    int mm = mMod;
    // phase-aligned modal-segment rows -> position-in-period ANOVA
    int nRows = 0;
    for (int i = 0; i < nInst; i++) {
      if (J->cnts[i] != mm) continue;
      int g = J->gpos[J->instA[i]];
      if (g + mm + 1 > nivg) continue;
      if ((nRows + 1) * mm > J->rowsCap) break;
      float hrow[8], irow[8], key[8];
      for (int q = 0; q < mm; q++) {
        hrow[q] = J->hn[g + 1 + q];
        irow[q] = J->iv[g + 1 + q] / J->refIv;
        key[q] = rintf(irow[q] * 2.0f) / 2.0f;
      }
      int rot = 0;  // lexicographically maximal rotation, first wins ties
      for (int s0 = 1; s0 < mm; s0++)
        for (int q = 0; q < mm; q++) {
          float a = key[(s0 + q) % mm], b = key[(rot + q) % mm];
          if (a > b) { rot = s0; break; } if (a < b) break;
        }
      for (int q = 0; q < mm; q++) { J->rowsH[nRows * mm + q] = hrow[(q + rot) % mm]; J->rowsI[nRows * mm + q] = irow[(q + rot) % mm]; }
      nRows++;
    }
    if (nRows >= FAD_MIN_MODAL_SEGS) {
      float fsh, esh, fsi, esi;
      fadAnova(J->rowsH, nRows * mm, mm, &fsh, &esh);
      fadAnova(J->rowsI, nRows * mm, mm, &fsi, &esi);
      float shs = (J->scaleH > 1e-9f) ? J->scaleH : 1e-9f;
      float dSyncH = esh / shs, dSyncI = esi;
      int kSync = (int)llrint((double)T0 / (double)J->refIv);
      if (kSync < 2) kSync = 2; if (kSync > 8) kSync = 8;
      if (dSyncI > J->dI) { J->runI = (J->dI > dSyncI * 1e-3f) ? J->dI : dSyncI * 1e-3f; J->kI = kSync; J->dI = dSyncI; J->Fi[kSync] = fsi; J->syncUsed = 1; }
      if (dSyncH > J->dH) { J->runH = (J->dH > dSyncH * 1e-3f) ? J->dH : dSyncH * 1e-3f; J->kH = kSync; J->dH = dSyncH; J->Fh[kSync] = fsh; J->syncUsed = 1; }
      if (J->syncUsed) J->Tsync = T0;
    }
  } while (0);

  // confirm: T_win from the primary feature pick, then ACF spans aP (pitch) and aT (period)
  float Twin;
  {
    uint8_t trigH = (J->dH >= FAD_TH_H && J->Fh[J->kH] >= FAD_F_SIG) ? 1 : 0;
    uint8_t trigI = (J->dI >= FAD_TH_INT && J->Fi[J->kI] >= FAD_F_SIG) ? 1 : 0;
    if (J->syncUsed) {
      Twin = J->Tsync;
    } else {
      uint8_t featI = (trigI || (!trigH && J->dI / FAD_TH_INT >= J->dH / FAD_TH_H)) ? 1 : 0;
      int kWin = featI ? J->kI : J->kH;
      double sum = 0.0;
      for (int q = 0; q < niv; q++) sum += (double)J->iv[q];
      Twin = (float)kWin * (float)(sum / (double)niv);
    }
  }
  float aP = -1e30f, aT = -1e30f;
  {
    const float *rn = J->s;
    float m = (float)J->rnMean;
    int pLo = (int)(0.75f * J->P); if (pLo < 2) pLo = 2;
    int pHi = (int)(1.3f * J->P);
    int tLo = (int)(0.85f * Twin); if (tLo < 2) tLo = 2;
    int tHi = (int)(1.15f * Twin) + 1;
    if (pHi > n - 2) pHi = n - 2;
    if (tHi > n - 2) tHi = n - 2;
    if (J->rnVar > 0.0) {
      for (int lag = pLo; lag <= pHi; lag++) {
        int len = n - lag; double acc = 0.0;
        for (int seg = 0; seg < len; seg += FAD_SEG_SAMPLES) {
          int end = seg + FAD_SEG_SAMPLES; if (end > len) end = len;
          float part = 0.0f; const float *a = rn + seg, *b = rn + seg + lag; int cnt = end - seg;
          for (int i = 0; i < cnt; i++) part += (a[i] - m) * (b[i] - m);
          acc += (double)part;
        }
        float v = (float)(acc / (double)len / J->rnVar);
        if (v > aP) aP = v;
      }
      for (int lag = tLo; lag <= tHi; lag++) {
        int len = n - lag; double acc = 0.0;
        for (int seg = 0; seg < len; seg += FAD_SEG_SAMPLES) {
          int end = seg + FAD_SEG_SAMPLES; if (end > len) end = len;
          float part = 0.0f; const float *a = rn + seg, *b = rn + seg + lag; int cnt = end - seg;
          for (int i = 0; i < cnt; i++) part += (a[i] - m) * (b[i] - m);
          acc += (double)part;
        }
        float v = (float)(acc / (double)len / J->rnVar);
        if (v > aT) aT = v;
      }
    }
    if (aP < -1e29f) aP = 0.0f;
    if (aT < -1e29f) aT = 0.0f;
  }

  // verdict + keep best
  uint8_t trigH = (J->dH >= FAD_TH_H && J->Fh[J->kH] >= FAD_F_SIG) ? 1 : 0;
  uint8_t trigI = (J->dI >= FAD_TH_INT && J->Fi[J->kI] >= FAD_F_SIG) ? 1 : 0;
  uint8_t featI = (trigI || (!trigH && J->dI / FAD_TH_INT >= J->dH / FAD_TH_H)) ? 1 : 0;
  int kWin = featI ? J->kI : J->kH;
  float dWin = featI ? J->dI : J->dH;
  float runner = featI ? J->runI : J->runH;
  float thr = featI ? FAD_TH_INT : FAD_TH_H;
  float aPd = (aP > 0.05f) ? aP : 0.05f;
  float acfRatio = aT / aPd;
  uint8_t confirmed = (J->syncUsed || acfRatio >= FAD_ACF_CONFIRM) ? 1 : 0;
  uint8_t verdict;
  if ((trigH || trigI) && confirmed) verdict = FADV_FAULT;
  else if (J->dI >= FAD_TREND_INT || J->dH >= FAD_TREND_H) verdict = FADV_TREND;
  else verdict = FADV_HEALTHY;
  float hr1 = J->dI / FAD_TH_INT, hr2 = J->dH / FAD_TH_H;
  float headroom = (hr1 > hr2) ? hr1 : hr2;
  uint8_t better = !J->haveBest || (J->sane2 > J->bestSane) || (J->sane2 == J->bestSane && headroom > J->bestHeadroom);
  if (better) {
    J->haveBest = 1; J->bestSane = J->sane2; J->bestHeadroom = headroom;
    FadResult *Rr = &J->best;
    Rr->verdict = verdict; Rr->winningK = (uint8_t)kWin; Rr->featInterval = featI; Rr->sync = J->syncUsed;
    Rr->score = dWin; Rr->marginThr = dWin / thr; Rr->marginRunner = dWin / ((runner > 1e-12f) ? runner : 1e-12f);
    Rr->Dheight = J->dH; Rr->Dinterval = J->dI; Rr->Fwin = featI ? J->Fi[J->kI] : J->Fh[J->kH];
    Rr->acfRatio = acfRatio; Rr->periodRatio = Twin / J->P; Rr->pitch = J->P; Rr->subtrain = J->subtrain; Rr->nCrests = J->nKept;
  }
}

// Whole-analysis driver. deadline is ignored (runs to completion). Returns 1; fills *out.
static int fadStep(FadJob *J, int64_t deadline, FadResult *out) {
  (void)deadline;
  int n = J->n;
  if (J->src) for (int i = 0; i < n; i++) J->xf[i] = (float)J->src[i];
  // s = boxcar-3 of xf (numpy clipped-window semantics)
  {
    const float *x = J->xf;
    for (int i = 0; i < n; i++) {
      int lo = i - 1; if (lo < 0) lo = 0;
      int hi = i + 2; if (hi > n) hi = n;
      float acc = 0.0f; for (int q = lo; q < hi; q++) acc += x[q];
      J->s[i] = acc / (float)(hi - lo);
    }
  }
  int nRough = (int)llrintf(FAD_ROUGH_MS * 1e-3f * J->fs); if (nRough < 3) nRough = 3;
  J->pre[0] = 0.0; for (int i = 0; i < n; i++) J->pre[i + 1] = J->pre[i] + (double)J->s[i];  // prefix(s)
  for (int i = 0; i < n; i++) J->rr[i] = J->s[i] - fadMov(J->pre, n, nRough, i);  // r1 = s - movavg(s,nRough)
  // r1 mean / var (chunked float-partial -> double, order preserved)
  { double acc = 0.0; for (int p = 0; p < n; p += FAD_SEG_SAMPLES) { int end = p + FAD_SEG_SAMPLES; if (end > n) end = n; float part = 0.0f; for (int i = p; i < end; i++) part += J->rr[i]; acc += (double)part; } J->r1Mean = acc / (double)n; }
  { float m = (float)J->r1Mean; double acc = 0.0; for (int p = 0; p < n; p += FAD_SEG_SAMPLES) { int end = p + FAD_SEG_SAMPLES; if (end > n) end = n; float part = 0.0f; for (int i = p; i < end; i++) { float d = J->rr[i] - m; part += d * d; } acc += (double)part; } J->r1Var = acc / (double)n; }
  int lagMaxCls = n / 4; if (lagMaxCls > FAD_ACF_LAG_CAP) lagMaxCls = FAD_ACF_LAG_CAP;
  // regime ACF over lags 4..lagMaxCls of r1
  if (J->r1Var <= 0.0) {
    for (int l = 4; l <= lagMaxCls; l++) J->acf[l - 4] = 0.0f;
  } else {
    float m = (float)J->r1Mean;
    for (int lag = 4; lag <= lagMaxCls; lag++) {
      int len = n - lag; double acc = 0.0;
      for (int seg = 0; seg < len; seg += FAD_SEG_SAMPLES) {
        int end = seg + FAD_SEG_SAMPLES; if (end > len) end = len;
        float part = 0.0f; const float *a = J->rr + seg, *b = J->rr + seg + lag; int cnt = end - seg;
        for (int i = 0; i < cnt; i++) part += (a[i] - m) * (b[i] - m);
        acc += (double)part;
      }
      J->acf[lag - 4] = (float)(acc / (double)len / J->r1Var);
    }
  }
  // regime peak -> cycle period -> idle/cruise ladder
  {
    int m = lagMaxCls - 4 + 1;
    int bestP = -1;
    for (int p = 1; p < m - 1; p++)
      if (J->acf[p] > J->acf[p - 1] && J->acf[p] >= J->acf[p + 1])
        if (bestP < 0 || J->acf[p] > J->acf[bestP]) bestP = p;
    if (bestP < 0) goto done;  // no periodicity -> quiet
    J->Tcycle = (float)(4 + bestP);
    J->cruise = (J->Tcycle < FAD_REGIME_CYCLE_MS * 1e-3f * J->fs) ? 1 : 0;
    if (J->cruise) {
      J->nRungs = 3;
      J->rungW[0] = 0.4f;  J->rungF[0] = 0.3f;
      J->rungW[1] = 0.4f;  J->rungF[1] = 0.5f;
      J->rungW[2] = 0.75f; J->rungF[2] = 0.65f;
    } else {
      J->nRungs = 1; J->rungW[0] = 0.75f; J->rungF[0] = 0.65f;
    }
  }
  // bootstrap pitch pick (conservative rung)
  if (fadPick(J, 0.75f, 0.65f) < 0 || J->nKept < 32) goto done;
  {
    int niv = J->nKept - 1;
    for (int q = 0; q < niv; q++) J->scratch[q] = (float)(J->kept[q + 1] - J->kept[q]);
    J->P = fadMedian(J->scratch, niv);
    if (!(J->P >= 4.0f && J->P <= 400.0f)) goto done;
    J->nCyc = (int)llrintf(6.0f * J->P);
    J->envN = (int)llrintf(FAD_ENV_CYCLES * (float)J->nCyc);
  }
  for (int i = 0; i < n; i++) J->rr[i] = J->s[i] - fadMov(J->pre, n, J->nCyc, i);  // r = s - movavg(s,nCyc)
  { double acc = 0.0; for (int p = 0; p < n; p += FAD_SEG_SAMPLES) { int end = p + FAD_SEG_SAMPLES; if (end > n) end = n; float part = 0.0f; for (int i = p; i < end; i++) part += J->rr[i] * J->rr[i]; acc += (double)part; } J->rms = (float)sqrt(acc / (double)n); }
  J->pre[0] = 0.0; for (int i = 0; i < n; i++) J->pre[i + 1] = J->pre[i] + (double)fabsf(J->rr[i]);  // prefix(|r|)
  { float floorv = 0.2f * J->rms; for (int i = 0; i < n; i++) { float e = fadMov(J->pre, n, J->envN, i); J->env[i] = (e > floorv) ? e : floorv; } }
  // rn = r/env (into s) + mean / var
  { double acc = 0.0; for (int p = 0; p < n; p += FAD_SEG_SAMPLES) { int end = p + FAD_SEG_SAMPLES; if (end > n) end = n; float part = 0.0f; for (int i = p; i < end; i++) { J->s[i] = J->rr[i] / J->env[i]; part += J->s[i]; } acc += (double)part; } J->rnMean = acc / (double)n; }
  { float m = (float)J->rnMean; double acc = 0.0; for (int p = 0; p < n; p += FAD_SEG_SAMPLES) { int end = p + FAD_SEG_SAMPLES; if (end > n) end = n; float part = 0.0f; for (int i = p; i < end; i++) { float d = J->s[i] - m; part += d * d; } acc += (double)part; } J->rnVar = acc / (double)n; }
  for (int rung = 0; rung < J->nRungs; rung++) fadRung(J, J->rungW[rung], J->rungF[rung]);
done:
  if (out) { if (J->haveBest) *out = J->best; else memset(out, 0, sizeof(*out)); }
  return 1;
}

// ============================================================================
// Config Sharing — export/import of the cloneable settings set (Phase 1)
// ----------------------------------------------------------------------------
// One ALLOWLIST (CONFIG_MANIFEST) is the single source of truth for which NVS
// "settings" keys are shareable. tier 1 = imported like any other setting, including
// the install/hardware topology (sensor/shunt/polarity) and per-install calibration;
// tier 3 = per-device history, always exported (fleet snapshot / support) but NEVER
// imported, since adopting another boat's value would corrupt this device's own record.
// The alt-health / boat-perf registry knobs (ALT_SETTINGS / PERF_SETTINGS in
// 7_functions.ino) are ALSO shareable but save through generic loops the literal
// manifest can't reference — they are emitted/imported programmatically (tier-1)
// minus CFG_REGISTRY_SKIP, so a knob added to either registry is covered automatically.
// Philosophy: the export captures essentially EVERY persisted setting — including UI
// prefs and per-install calibration/measured fits. Paring down to what's appropriate to
// apply happens at import time via the diff checkboxes, never by omitting from the
// export. Only identity/secrets (WiFi creds, passwords, InstallId), device
// lifecycle/history state, learned per-device state, and momentary actions stay out
// (see config_drift_check.py — it fails the build if any settingWrite key is in neither
// the manifest nor its EXCLUDE list).
// The user-editable RPM tables are raw blobs in the "learning" namespace, carried by
// a separate "tables" export section (exportTablesObject), not this manifest.
//
// Values are carried as the RAW NVS strings (settingRead output), so import is a
// straight settingWrite of byte-identical internal state followed by a reboot —
// InitSystemSettings then re-reads the whole set consistently. No per-key unit
// conversion, no /get replay. Cross-rev safe: a key absent on the destination is
// skipped and that firmware's own default applies (see CLAUDE.md "NVS Cross-Rev Risk").
// ============================================================================
struct ConfigManifestEntry { const char *param; const char *nvsKey; uint8_t tier; };
static const ConfigManifestEntry CONFIG_MANIFEST[] = {
  { "BulkVoltage", NK_BulkVoltage, 1 },
  { "AbsorptionVoltage", NK_AbsorptionVoltage, 1 },
  { "FloatVoltage", NK_FloatVoltage, 1 },
  { "ChargedVoltage", NK_ChargedVoltage, 1 },
  { "AbsorptionTimeoutMs", NK_AbsorptionTimeoutMs, 1 },
  { "absorptionCompleteTime", NK_absorptionCompleteTime, 1 },
  { "FLOAT_DURATION", NK_FLOAT_DURATION, 1 },
  { "MinFloatTime", NK_MinFloatTime, 1 },
  { "ChargedDetectionTime", NK_ChargedDetectionTime, 1 },
  { "bulkVoltageHoldMs", NK_bulkVoltageHoldMs, 1 },
  { "TailCurrent", NK_TailCurrent, 1 },
  { "TailCurrent_A", NK_TailCurrent_A, 1 },
  { "CurrentThreshold", NK_CurrentThreshold, 1 },
  { "RebulkCurrent_A", NK_RebulkCurrent_A, 1 },
  { "RebulkVoltage", NK_RebulkVoltage, 1 },
  { "rebulkDebounceTime", NK_rebulkDebounceTime, 1 },
  { "SOC_AllowRebulk_percent", NK_SOC_AllowRebulk_percent, 1 },
  { "SOC_BlockRebulk_percent", NK_SOC_BlockRebulk_percent, 1 },
  { "MaintainMode", NK_MaintainMode, 1 },
  { "UseFloat", NK_UseFloat, 1 },
  { "TargetVoltageMode", NK_TargetVoltageMode, 1 },
  { "TargetVoltageSetpoint", NK_TargetVoltageSetpoint, 1 },
  { "ChargeEfficiency", NK_ChargeEfficiency, 1 },
  { "PeukertExponent", NK_PeukertExponent, 1 },
  { "BatteryCapacity_Ah", NK_BatteryCapacity_Ah, 1 },
  { "AlternatorNominalAmps", NK_AlternatorNominalAmps, 1 },
  { "SolarWatts", NK_SolarWatts, 1 },
  { "MaximumAllowedBatteryAmps", NK_MaximumAllowedBatteryAmps, 1 },
  { "MaxTableValue", NK_MaxTableValue, 1 },
  { "MaxDuty", NK_MaxDuty, 1 },
  { "MinDuty", NK_MinDuty, 1 },
  { "AlternatorHardShutdownV", NK_AlternatorHardShutdownV, 1 },
  { "MinRPMForField", NK_MinRPMForField, 1 },
  { "FIELD_COLLAPSE_DELAY", NK_FIELD_COLLAPSE_DELAY, 1 },
  { "FieldAdjustmentInterval", NK_FieldAdjustmentInterval, 1 },
  { "DutyRampRate", NK_DutyRampRate, 1 },
  { "DutySlowRampRate", NK_DutySlowRampRate, 1 },
  { "WarmupRampRate", NK_WarmupRampRate, 1 },
  { "StartupRiseRate", NK_StartupRiseRate, 1 },
  { "FieldResistance", NK_FieldResistance, 1 },
  { "PulleyRatio", NK_PulleyRatio, 1 },
  // Tier 3, not 1: adopting another boat's tach calibration would silently rescale this device's
  // engine-RPM axis via a direct settingWrite, bypassing the /get guard that arms the wipe.
  { "RPMScalingFactor", NK_RPMScalingFactor, 3 },
  { "SwitchingFrequency", NK_SwitchingFrequency, 1 },
  { "PidKp", NK_PidKp, 1 },
  { "PidKi", NK_PidKi, 1 },
  { "PidKd", NK_PidKd, 1 },
  { "PidSampleDivisor", NK_PidSampleDivisor, 1 },
  { "PIDTrackingGain", NK_PIDTrackingGain, 1 },
  { "OutputPIDFilterTC", NK_OutputPIDFilterTC, 1 },
  { "OutputPIDMA_N", NK_OutputPIDMA_N, 1 },
  { "OutputPIDSigSrc", NK_OutputPIDSigSrc, 1 },
  { "VoltageFilterTC", NK_VoltageFilterTC, 1 },
  { "DvdtTC", NK_DvdtTC, 1 },
  { "VoltageKp", NK_VoltageKp, 1 },
  { "VoltageKi", NK_VoltageKi, 1 },
  { "VoltageLoopInterval", NK_VoltageLoopInterval, 1 },
  { "AwBleedRate", NK_AwBleedRate, 1 },
  { "AwSeedProtectMs", NK_AwSeedProtectMs, 1 },
  { "SetpointRiseRate", NK_SetpointRiseRate, 1 },
  { "SetpointFallRate", NK_SetpointFallRate, 1 },
  { "CvBrakeFallRate", NK_CvBrakeFallRate, 1 },
  { "SetpointBigStepThresh", NK_SetpointBigStepThresh, 1 },
  { "SetpointBigStepRiseRate", NK_SetpointBigStepRiseRate, 1 },
  { "FastSetpointRiseRate", NK_FastSetpointRiseRate, 1 },
  { "FastSetpointRiseHeadroomV", NK_FastSetpointRiseHeadroomV, 1 },
  { "FastSetpointRiseWindowMs", NK_FastSetpointRiseWindowMs, 1 },
  { "cvHelpersEnabled", NK_cvHelpersEnabled, 1 },
  { "cvGainMode", NK_cvGainMode, 1 },
  { "cvCrossover", NK_cvCrossover, 1 },
  { "cvPiZero", NK_cvPiZero, 1 },
  { "cvAlpha", NK_cvAlpha, 1 },
  { "vTgtRampEnable", NK_vTgtRampEnable, 1 },
  { "vTgtRampUp", NK_vTgtRampUp, 1 },
  { "vTgtRampDn", NK_vTgtRampDn, 1 },
  { "setpointSlewEnable", NK_setpointSlewEnable, 1 },
  { "cvRiseGovEnable", NK_cvRiseGovEnable, 1 },
  { "cvRecovEnable", NK_cvRecovEnable, 1 },
  { "loadServeBoostEnable", NK_loadServeBoostEnable, 1 },
  { "reseedCorrEnable", NK_reseedCorrEnable, 1 },
  { "HuntGovEnable", NK_HuntGovEnable, 1 },
  { "cvRecovSec", NK_cvRecovSec, 1 },      // retired, inert — kept so shared configs stay complete
  { "cvRecovEmaxV", NK_cvRecovEmaxV, 1 },  // retired, inert — kept so shared configs stay complete
  { "cvRecovKiMax", NK_cvRecovKiMax, 1 },
  { "cvWindDownEnable", NK_cvWindDownEn, 1 },
  { "cvWindDownRate", NK_cvWindDownRate, 1 },
  { "cvWindDownStopV", NK_cvWindDownStopV, 1 },
  { "cvRecovBoostEnable", NK_cvRecovBoostEnable, 1 },
  { "cvRecovBoostMax", NK_cvRecovBoostMax, 1 },
  { "cvRecovBoostErrV", NK_cvRecovBoostErrV, 1 },
  { "cvRecovBoostFloorV", NK_cvRecovBoostFloorV, 1 },
  { "dutySlewEnable", NK_dutySlewEnable, 1 },
  { "testSlewMode", NK_testSlewMode, 1 },
  { "cvTestSlewMode", NK_cvTestSlewMode, 1 },
  { "coldChargeLockoutEnable", NK_coldChargeLockoutEnable, 1 },
  { "MinChargeTempF", NK_MinChargeTempF, 1 },
  { "battTempDerateEnable", NK_battTempDerateEn, 1 },
  { "battTempCoeff", NK_battTempCoeff, 1 },
  { "VoltageKd", NK_VoltageKd, 1 },
  { "CvKdDeadbandVps", NK_CvKdDeadbandVps, 1 },
  { "CvKdDbSlope", NK_CvKdDbSlope, 1 },
  { "CvKdDbFloor", NK_CvKdDbFloor, 1 },
  { "CvKdDbCeil", NK_CvKdDbCeil, 1 },
  { "CvKdExcessMode", NK_CvKdExcessMode, 1 },
  { "CvKdOneSided", NK_CvKdOneSided, 1 },
  { "CvKdArmV", NK_CvKdArmV, 1 },
  { "CvKdMaxTrimA", NK_CvKdMaxTrimA, 1 },
  { "CvKdVoltFiltTC", NK_CvKdVoltFiltTC, 1 },
  { "CvKdSlopeCeil", NK_CvKdSlopeCeil, 1 },
  { "CvStressDropV", NK_CvStressDropV, 1 },
  { "CvStressFailBandV", NK_CvStressFailBandV, 1 },
  { "CvKdTd", NK_CvKdTd, 1 },
  { "TdPred", NK_TdPred, 1 },
  { "KHard", NK_KHard, 1 },
  { "OvGroup1Enable", NK_OvGroup1Enable, 1 },
  { "OvGroup2Enable", NK_OvGroup2Enable, 1 },
  { "HardOCEnable", NK_HardOCEnable, 1 },
  { "IExcessEnable", NK_IExcessEnable, 1 },
  { "BattLimitEnable", NK_BattLimitEnable, 1 },
  { "LoadDumpEnable", NK_LoadDumpEnable, 1 },
  { "OvMeasMarginV", NK_OvMeasMarginV, 1 },
  { "OvPredMarginV", NK_OvPredMarginV, 1 },
  { "HardOCDebounceMs", NK_HardOCDebounceMs, 1 },
  { "SettleTimeBeforeCut", NK_SettleTimeBeforeCut, 1 },
  { "ShutdownPhase2HoldMs", NK_ShutdownPhase2HoldMs, 1 },
  { "ReseedFrac", NK_ReseedFrac, 1 },
  { "ReseedFracNoShunt", NK_ReseedFracNS, 1 },
  { "CvRecovClimbRate", NK_CvRecovClimb, 1 },
  { "IExcessArmMarginV", NK_IExcessArmMarginV, 1 },
  { "IExcessCeilA", NK_IExcessCeilA, 1 },
  { "IExcessFloorA", NK_IExcessFloorA, 1 },
  { "BattCurrentLimitA", NK_BattCurrentLimitA, 1 },
  { "IExcessFrac", NK_IExcessFrac, 1 },
  { "IExcessFracBulk", NK_IExcessFracBulk, 1 },
  { "IExcessBaseA", NK_IExcessBaseA, 1 },
  { "IExcessCcOffsetA", NK_IExcessCcOffsetA, 1 },
  { "IExcessKBleed", NK_IExcessKBleed, 1 },
  { "IExcessRelFrac", NK_IExcessRelFrac, 1 },
  { "IExcessTau", NK_IExcessTau, 1 },
  { "TemperatureLimitF", NK_TemperatureLimitF, 1 },
  { "TempAlarm", NK_TempAlarm, 1 },
  { "TempAlarmLow", NK_TempAlarmLow, 1 },
  { "TempWarnExcess", NK_TempWarnExcess, 1 },
  { "TempCritExcess", NK_TempCritExcess, 1 },
  { "TempSustainedTimeout", NK_TempSustainedTimeout, 1 },
  { "TempPIDKp", NK_TempPIDKp, 1 },
  { "TempPIDKi", NK_TempPIDKi, 1 },
  { "TempPIDKiDownFrac", NK_TempPIDKiDownFrac, 1 },
  { "TempPIDIntervalMs", NK_TempPIDIntervalMs, 1 },
  { "TempPIDFilterAlpha", NK_TempPIDFilterAlpha, 1 },
  { "ThermalLookaheadSec", NK_ThermalLookaheadSec, 1 },
  { "ThermalSlopeWindowSec", NK_ThermalSlopeWindowSec, 1 },
  { "TempSource", NK_TempSource, 1 },
  { "R_fixed", NK_R_fixed, 1 },
  { "Beta", NK_Beta, 1 },
  { "T0_C", NK_T0_C, 1 },
  { "WindingTempOffset", NK_WindingTempOffset, 1 },
  { "displayTempUnit", NK_displayTempUnit, 1 },
  { "LearningUpStep", NK_LearningUpStep, 1 },
  { "LearningDownStep", NK_LearningDownStep, 1 },
  { "LearningSettlingPeriod", NK_LearningSettlingPeriod, 1 },
  { "LearningMemoryDuration", NK_LearningMemoryDuration, 1 },
  { "LearningRPMChangeThreshold", NK_LearningRPMChangeThreshold, 1 },
  { "LearningTempHysteresis", NK_LearningTempHysteresis, 1 },
  { "MinLearningInterval", NK_MinLearningInterval, 1 },
  { "NeighborLearningFactor", NK_NeighborLearningFactor, 1 },
  { "LogAllLearningEvents", NK_LogAllLearningEvents, 1 },
  { "IgnoreLearningDuringPenalty", NK_IgnoreLearningDuringPenalty, 1 },
  { "IgnoreRPM", NK_IgnoreRPM, 1 },
  { "IgnoreTemperature", NK_IgnoreTemperature, 1 },
  { "MaxPenaltyDuration", NK_MaxPenaltyDuration, 1 },
  { "MaxPenaltyPercent", NK_MaxPenaltyPercent, 1 },
  { "SafeOperationThreshold", NK_SafeOperationThreshold, 1 },
  { "kneeLearnEnable", NK_kneeLearnEnable, 1 },
  { "kneeMarginPct", NK_kneeMarginPct, 1 },
  { "kneeMaxFloorPct", NK_kneeMaxFloorPct, 1 },
  { "kneeOnsetA", NK_kneeOnsetA, 1 },
  { "kneeReArmA", NK_kneeReArmA, 1 },
  { "kneeStepPct", NK_kneeStepPct, 1 },
  { "kneeDwellSec", NK_kneeDwellSec, 1 },
  { "kneeTempRefF", NK_kneeTempRefF, 1 },
  { "kneeTempComp", NK_kneeTempComp, 1 },
  { "kneeRpmTolPct", NK_kneeRpmTolPct, 1 },
  { "kneeTempTolF", NK_kneeTempTolF, 1 },
  { "kneeDutyTolPct", NK_kneeDutyTolPct, 1 },
  { "faEnabled", NK_faEnabled, 1 },
  { "faAlarmEnable", NK_faAlarmEnable, 1 },
  { "faPeakMinA", NK_faPeakMinA, 1 },
  { "faAmpsDriftPct", NK_faAmpsDriftPct, 1 },
  { "faAmpsDriftFloorA", NK_faAmpsDriftFloorA, 1 },
  { "faAttenUpAmps", NK_faAttenUpAmps, 1 },
  { "faAttenDownAmps", NK_faAttenDownAmps, 1 },
  { "faRpmEdgeMargin", NK_faRpmEdgeMargin, 1 },
  { "faAnomPause", NK_faAnomPause, 1 },
  { "ripWinMs", NK_ripWinMs, 1 },
  { "ripDriftFloorA", NK_ripDriftFloorA, 1 },
  { "ripDriftPct", NK_ripDriftPct, 1 },
  { "SystemIDStabilizeAmps", NK_SystemIDStabilizeAmps, 1 },
  { "SystemIDStepAmplitude", NK_SystemIDStepAmplitude, 1 },
  { "systemIDTestType", NK_systemIDTestType, 1 },
  { "systemIDSineCycles", NK_systemIDSineCycles, 1 },
  { "systemIDSineFreqStart", NK_systemIDSineFreqStart, 1 },
  { "systemIDSineFreqEnd", NK_systemIDSineFreqEnd, 1 },
  { "tuningWaveform", NK_tuningWaveform, 1 },
  { "tuningSineFreq", NK_tuningSineFreq, 1 },
  { "tuningSweepStart", NK_tuningSweepStart, 1 },
  { "tuningSweepEnd", NK_tuningSweepEnd, 1 },
  { "tuningSweepCycles", NK_tuningSweepCycles, 1 },
  { "tuningWaveFloor", NK_tuningWaveFloor, 1 },
  { "waveAmplitude", NK_waveAmplitude, 1 },
  { "wavePeriod", NK_wavePeriod, 1 },
  { "cvWaveAmplitudeV", NK_cvWaveAmplitudeV, 1 },
  { "cvWavePeriodSec", NK_cvWavePeriodSec, 1 },
  { "cvKOvershoot", NK_cvKOvershoot, 1 },
  { "cvConsecutiveReads", NK_cvConsecutiveReads, 1 },
  { "LoadDumpDtThresh", NK_LoadDumpDtThresh, 1 },
  { "LoadDumpDtThresh1", NK_LoadDumpDtThresh1, 1 },
  { "LoadDumpDtThresh3", NK_LoadDumpDtThresh3, 1 },
  { "VoltageDisagreeThreshold", NK_VoltageDisagreeThreshold, 1 },
  { "VoltageDisagreeTimeout", NK_VoltageDisagreeTimeout, 1 },
  { "AlarmActivate", NK_AlarmActivate, 1 },
  { "AlarmLatchEnabled", NK_AlarmLatchEnabled, 1 },
  { "CurrentAlarmHigh", NK_CurrentAlarmHigh, 1 },
  { "VoltageAlarmHigh", NK_VoltageAlarmHigh, 1 },
  { "VoltageAlarmLow", NK_VoltageAlarmLow, 1 },
  { "SocAlarmLow", NK_SocAlarmLow, 1 },
  { "UVThresholdHigh", NK_UVThresholdHigh, 1 },
  { "CAPSIZE_THRESHOLD_DEG", NK_CAPSIZE_THRESHOLD_DEG, 1 },
  { "PITCHPOLE_THRESHOLD_DEG", NK_PITCHPOLE_THRESHOLD_DEG, 1 },
  { "SLAM_THRESHOLD_G", NK_SLAM_THRESHOLD_G, 1 },
  { "bmsLogic", NK_bmsLogic, 1 },
  { "bmsLogicLevelOff", NK_bmsLogicLevelOff, 1 },
  { "capLimitMode", NK_capLimitMode, 1 },
  { "NMEA0183Data", NK_NMEA0183Data, 1 },
  { "NMEA2KData", NK_NMEA2KData, 1 },
  { "VeData", NK_VeData, 1 },
  { "weatherModeEnabled", NK_weatherModeEnabled, 1 },
  { "gpsTimeSourceMode", NK_gpsTimeSourceMode, 1 },
  { "wifiNapEnabled", NK_wifiNapEnabled, 1 },
  { "ZeroLogEnable", NK_ZeroLogEnable, 1 },
  { "performanceRatio", NK_performanceRatio, 1 },
  { "HiLow", NK_HiLow, 1 },
  { "ManualSOCPoint", NK_ManualSOCPoint, 1 },
  { "ManualLifePercentage", NK_ManualLifePercentage, 1 },
  { "CloudFeatures", NK_CloudFeatures, 1 },
  { "FuelEfficiency", NK_FuelEfficiency, 1 },
  { "WeatherUpdateInterval", NK_WeatherUpdateInterval, 1 },
  { "WeatherTimeoutMs", NK_WeatherTimeoutMs, 1 },
  { "webgaugesinterval", NK_webgaugesinterval, 1 },
  { "plotTimeWindow", NK_plotTimeWindow, 1 },
  { "maxPoints", NK_maxPoints, 1 },
  { "xTime", NK_xTime, 1 },
  { "timeAxisModeChanging", NK_timeAxisModeChanging, 1 },
  { "Ymin1", NK_Ymin1, 1 },
  { "Ymax1", NK_Ymax1, 1 },
  { "Ymin2", NK_Ymin2, 1 },
  { "Ymax2", NK_Ymax2, 1 },
  { "Ymin3", NK_Ymin3, 1 },
  { "Ymax3", NK_Ymax3, 1 },
  { "Ymin4", NK_Ymin4, 1 },
  { "Ymax4", NK_Ymax4, 1 },
  { "yyMin", NK_yyMin, 1 },
  { "yyMax", NK_yyMax, 1 },
  { "bhStepLowA", NK_bhStepLowA, 1 },
  { "bhStepDeltaA", NK_bhStepDeltaA, 1 },
  { "bhDwellMs", NK_bhDwellMs, 1 },   // export key names the STORED unit (ms); the UI param bhDwellSec is seconds
  { "bhNumEdges", NK_bhNumEdges, 1 },
  { "capRestFrac", NK_capRestFrac, 1 },
  { "capRestFloor", NK_capRestFloor, 1 },
  { "capSettleRate", NK_capSettleRate, 1 },
  { "capSocLowMax", NK_capSocLowMax, 1 },
  { "capMinSpan", NK_capMinSpan, 1 },
  { "capFullSoc", NK_capFullSoc, 1 },
  { "capRefMode", NK_capRefMode, 1 },
  { "capTempNorm", NK_capTempNorm, 1 },
  { "capTempCoeff", NK_capTempCoeff, 1 },
  { "capTempRef", NK_capTempRef, 1 },
  { "capOcv", NK_capOcvBlob, 1 },
  { "BatteryCurrentSource", NK_BatteryCurrentSource, 1 },
  { "ShuntResistanceMicroOhm", NK_ShuntResistanceMicroOhm, 1 },
  { "AmpSensorRange", NK_AmpSensorRange, 1 },
  { "InvertAltAmps", NK_InvertAltAmps, 1 },
  { "InvertBattAmps", NK_InvertBattAmps, 1 },
  { "BatteryShuntPresent", NK_BatteryShuntPresent, 1 },
  { "AlternatorCOffset", NK_AlternatorCOffset, 1 },
  { "BatteryCOffset", NK_BatteryCOffset, 1 },
  { "AutoAltCurrentZero", NK_AutoAltCurrentZero, 1 },
  { "AutoShuntGainCorrection", NK_AutoShuntGainCorrection, 1 },
  { "BatteryVoltage", NK_BatteryVoltage, 1 },
  { "cvPlantKa", NK_cvPlantKa, 1 },
  { "cvPlantKb", NK_cvPlantKb, 1 },
  { "CommissionTempF", NK_CommissionTempF, 1 },
  { "CommissionEpoch", NK_CommissionEpoch, 3 },
  { "systemIDPlantTauMs", NK_sysidPlantTau, 1 },
  { "fieldDecayTauMs", NK_fieldDecayTau, 1 },
  { "fdDrainLoMs", NK_fdDrainLoMs, 1 },
  { "fdDrainHiMs", NK_fdDrainHiMs, 1 },
  { "fdDrainRpmLo", NK_fdDrainRpmLo, 1 },
  { "fdDrainRpmHi", NK_fdDrainRpmHi, 1 },
  { "ripFitAlt", NK_ripFitAlt, 1 },
  { "slpFitAlt", NK_slpFitAlt, 1 },
  { "imu_zero", NK_imu_zero, 1 },
};
static const size_t CONFIG_MANIFEST_COUNT = sizeof(CONFIG_MANIFEST)/sizeof(CONFIG_MANIFEST[0]);

// Registry knobs NEVER exported/imported (everything else in ALT_SETTINGS/PERF_SETTINGS is):
//   altPaused / perfPaused — learning pause state, runtime intent not a charge profile
//   perfSpeedSrc — per-boat speed-sensor topology (STW vs SOG); importing it would bypass
//     the Clear-All reset perfSettingsHandle fires on change, leaving the learned
//     surfaces silently mismatched to the new source
// config_drift_check.py parses this array — keep it a plain literal.
static const char *CFG_REGISTRY_SKIP[] = { "altPaused", "perfPaused", "perfSpeedSrc" };
static bool cfgRegistrySkipped(const char *name) {
  for (size_t i = 0; i < sizeof(CFG_REGISTRY_SKIP)/sizeof(CFG_REGISTRY_SKIP[0]); i++)
    if (strcmp(name, CFG_REGISTRY_SKIP[i]) == 0) return true;
  return false;
}

// Append val as a JSON string literal (quotes + backslash escaped).
static void cfgAppendJsonStr(String &out, const String &val) {
  out += '"';
  for (size_t i = 0; i < val.length(); i++) {
    char c = val[i];
    if (c == '"' || c == '\\') out += '\\';
    out += c;
  }
  out += '"';
}

// Append a float as a bare JSON number, or "null" for NaN/Inf (JSON has no NaN literal — a raw
// nan would make the whole settings blob invalid and 500 the daily snapshot). For the structured
// commissioning-result arrays/objects below, which want real numbers, not quoted strings.
static void cfgAppendNum(String &out, float v, int dec) {
  if (isnan(v) || isinf(v)) { out += "null"; return; }
  out += String(v, dec);
}

// Append a float array as a JSON array of bare numbers (NaN/Inf → null, via cfgAppendNum).
static void cfgAppendFloatArr(String &out, const float *a, int n, int dec) {
  out += '[';
  for (int i = 0; i < n; i++) { if (i) out += ','; cfgAppendNum(out, a[i], dec); }
  out += ']';
}

// Emit the manifest settings as a complete JSON object {"param":"rawNvsStr",...}.
// Single source of truth for BOTH /exportConfig (sharing) and the daily fleet config
// snapshot (buildConfigPayload's "settings") — so neither can drift as settings are added.
// Keys never set are omitted.
// The alt-health/boat-perf registries are appended generically (minus CFG_REGISTRY_SKIP),
// so knobs added to those registries are covered without touching the manifest.
String manifestConfigObject() {
  String j;
  j.reserve(18432);   // manifest + registry + commissioning-result blobs (DCIR up to ~5.6KB)
  j = "{";
  bool first = true;
  for (size_t i = 0; i < CONFIG_MANIFEST_COUNT; i++) {
    if (!settingExists(CONFIG_MANIFEST[i].nvsKey)) continue;   // key never set -> omit, destination keeps its default
    String v = settingRead(CONFIG_MANIFEST[i].nvsKey);
    if (!first) j += ',';
    first = false;
    j += '"'; j += CONFIG_MANIFEST[i].param; j += "\":";
    cfgAppendJsonStr(j, v);
  }
  // A macro, not a helper: ALT_SETTINGS and PERF_SETTINGS are different struct types,
  // and a free function taking either by pointer would make Arduino's auto-prototype
  // generator hoist a prototype above the struct definitions in 7_functions.ino
  // ("does not name a type"). JSON param = FULL registry name (what the dashboard
  // sends); NVS key = name truncated to the 15-char cap, mirroring altSettingsLoad/
  // perfSettingsLoad exactly.
  #define CFG_EMIT_REGISTRY(REG, COUNT) \
    for (size_t i = 0; i < COUNT; i++) { \
      if (cfgRegistrySkipped(REG[i].name)) continue; \
      char key[16]; \
      snprintf(key, sizeof(key), "%s", REG[i].name); \
      if (!settingExists(key)) continue; \
      String v = settingRead(key); \
      if (!first) j += ','; \
      first = false; \
      j += '"'; j += REG[i].name; j += "\":"; \
      cfgAppendJsonStr(j, v); \
    }
  CFG_EMIT_REGISTRY(ALT_SETTINGS, ALT_SETTING_COUNT)
  CFG_EMIT_REGISTRY(PERF_SETTINGS, PERF_SETTING_COUNT)
  #undef CFG_EMIT_REGISTRY
  // Derived read-only: the base 12V-equiv CV gains the voltage loop is ACTUALLY running
  // (recomputeCvGains sets these from the plant fit in Auto, or = the manual VoltageKp/Ki in
  // Manual). Emitted so the blob is self-describing — the manual VoltageKp/Ki above are the
  // dormant fallback whenever cvGainMode=Auto. NOT in CONFIG_MANIFEST, so applyImportConfig
  // ignores them on the destination (it recomputes its own from its own plant fit).
  if (!first) j += ',';
  first = false;
  j += "\"cvComputedKp\":"; cfgAppendJsonStr(j, String(cvComputedKp, 2));
  j += ",\"cvComputedKi\":"; cfgAppendJsonStr(j, String(cvComputedKi, 2));
  j += ",\"cvComputedKd\":"; cfgAppendJsonStr(j, String(cvComputedKd, 2));
  // Commissioning RESULT records (measured outcomes, not tunable settings) — captured so the
  // owner can monitor them across the fleet and so a local export archives them. Read from the
  // persisted NVS blobs, which are always the last graded result (the parsed live globals are
  // NOT repopulated at boot). Not in CONFIG_MANIFEST → applyImportConfig ignores them: a boat
  // must never adopt another's measured results — same contract as cvComputed* above.
  j += ",\"commissioning_results\":{";
  {
    bool cfirst = true;
    // CV stress test — ver-2 positional CSV; layout documented at cvStressPersistResult().
    if (settingExists(NK_cvStressLast)) {
      j += "\"stress_test\":"; cfgAppendJsonStr(j, settingRead(NK_cvStressLast)); cfirst = false;
    }
    // Battery-health DCIR: baseline capacity (Ah) + full per-edge result blob (bhSerializeResults()).
    if (settingExists(NK_bhBaseline)) { if (!cfirst) j += ','; j += "\"bh_baseline_ah\":"; cfgAppendJsonStr(j, settingRead(NK_bhBaseline)); cfirst = false; }
    if (settingExists(NK_bhResults))  { if (!cfirst) j += ','; j += "\"bh_dcir\":";        cfgAppendJsonStr(j, settingRead(NK_bhResults));  cfirst = false; }
    // Capacity-fade: SUMMARY only (latest capacity %, point count). The full bhCapCap-point ring
    // (~21KB at 512) would overflow the 32KB daily upload buffer; the monitoring signal is the
    // latest measured capacity, and the full ring is never adopted on import anyway.
    if (bhCapRing && bhCapCount > 0) {
      if (!cfirst) j += ',';
      float capPct = bhCapRing[(bhCapHead - 1 + bhCapCap) % bhCapCap].capPct;
      j += "\"bh_capfade_pct\":";     cfgAppendJsonStr(j, String(capPct, 1));
      j += ",\"bh_capfade_points\":"; cfgAppendJsonStr(j, String(bhCapCount));
      cfirst = false;
    }
    // ── Learned per-device state (adaptive tables + sensor calibration). Monitored, NEVER imported:
    //    export-only, so another boat's hardware calibration can't land on this one. Read from the
    //    live globals, all restored from NVS at boot. Emitted as real JSON numbers/arrays (not the
    //    quoted strings the manifest uses) so the fleet DB can query them directly. rpm_axis is
    //    emitted once — every per-RPM array below shares it. ──
    if (!cfirst) j += ',';
    cfirst = false;
    j += "\"rpm_axis\":[";
    for (int i = 0; i < RPM_TABLE_SIZE; i++) { if (i) j += ','; j += String(rpmTableRPMPoints[i]); }
    j += ']';
    // Min% duty floor actually in effect (kneeFloor owns it when knee-learning is on, else the
    // scalar/manual floor), the detected knee, and per-point frozen flags (1 = learning converged).
    j += ",\"min_duty_floor\":"; cfgAppendFloatArr(j, rpmMinDutyTable, RPM_TABLE_SIZE, 2);
    j += ",\"knee_detected\":";  cfgAppendFloatArr(j, kneeKnee, RPM_TABLE_SIZE, 2);
    j += ",\"knee_frozen\":[";
    for (int i = 0; i < RPM_TABLE_SIZE; i++) { if (i) j += ','; j += (kneeFrozen[i] ? '1' : '0'); }
    j += "],\"knee_fit_a\":"; cfgAppendNum(j, kneeFitA, 3);
    // Per-RPM current (A) + power (W) ceiling for BOTH Hi and Lo modes, read straight from the
    // "learning" NVS namespace (read-only) so neither the active nor the inactive live table is
    // disturbed. A mode with no saved blob emits null (the device runs computed defaults there).
    // cap_mode = the mode in effect now (1 = Normal/High, 0 = Low).
    j += ",\"cap_mode\":"; j += String((int)HiLow);
    {
      nvs_handle_t ch; float tmp[RPM_TABLE_SIZE];
      bool haveNvs = (nvs_open("learning", NVS_READONLY, &ch) == ESP_OK);
      const char *capKeys[4] = { "capTable", "capPowerTable", "capTableLo", "capPowerTableLo" };
      const char *capJson[4] = { "cap_current_hi", "cap_power_hi", "cap_current_lo", "cap_power_lo" };
      const int   capDec[4]  = { 1, 0, 1, 0 };
      for (int m = 0; m < 4; m++) {
        j += ",\""; j += capJson[m]; j += "\":";
        size_t sz = sizeof(tmp);
        if (haveNvs && nvs_get_blob(ch, capKeys[m], tmp, &sz) == ESP_OK && sz == sizeof(tmp))
          cfgAppendFloatArr(j, tmp, RPM_TABLE_SIZE, capDec[m]);
        else
          j += "null";
      }
      if (haveNvs) nvs_close(ch);
    }
    // Current-sensor zero-drift fit (zero-offset = offset + slope·(T − T_ref); slope in A/°F) and the
    // learned shunt-gain correction (1.0 = none). valid=0 until the first accepted fit.
    j += ",\"zero_fit\":{\"valid\":"; j += String((int)zfValid);
    j += ",\"sensor\":"; j += String((int)zfSensor);
    j += ",\"offset\":"; cfgAppendNum(j, zfC, 6);
    j += ",\"slope\":";  cfgAppendNum(j, zfB, 6);
    j += ",\"r2\":";     cfgAppendNum(j, zfR2, 4);
    j += ",\"epoch\":";  j += String((unsigned long)zfLastEpoch); j += "}";
    j += ",\"shunt_gain\":"; cfgAppendNum(j, DynamicShuntGainFactor, 4);
  }
  j += "}";
  j += "}";
  return j;
}

// User-editable RPM tables, read straight from the "learning" NVS namespace (raw
// float/int blobs the string-settings manifest can't carry). Both the Normal and Low
// cap tables are emitted regardless of the active HiLow mode. Values are comma-joined
// raw numbers (amps / watts / duty-%). A blob missing from NVS is omitted.
String exportTablesObject() {
  String j;
  j.reserve(1024);
  j = "{";
  nvs_handle_t h;
  if (nvs_open("learning", NVS_READONLY, &h) != ESP_OK) { j += "}"; return j; }
  bool first = true;
  {
    int pts[RPM_TABLE_SIZE];
    size_t sz = sizeof(pts);
    if (nvs_get_blob(h, "rpmPoints", pts, &sz) == ESP_OK && sz == sizeof(pts)) {
      j += "\"rpmPoints\":\"";
      for (int i = 0; i < RPM_TABLE_SIZE; i++) { if (i) j += ','; j += String(pts[i]); }
      j += '"';
      first = false;
    }
  }
  static const char *CFG_TABLE_FKEYS[] = { "capTable", "capPowerTable", "capTableLo", "capPowerTableLo", "minDutyTable" };
  for (size_t k = 0; k < sizeof(CFG_TABLE_FKEYS)/sizeof(CFG_TABLE_FKEYS[0]); k++) {
    float f[RPM_TABLE_SIZE];
    size_t sz = sizeof(f);
    if (nvs_get_blob(h, CFG_TABLE_FKEYS[k], f, &sz) != ESP_OK || sz != sizeof(f)) continue;
    if (!first) j += ',';
    first = false;
    j += '"'; j += CFG_TABLE_FKEYS[k]; j += "\":\"";
    for (int i = 0; i < RPM_TABLE_SIZE; i++) { if (i) j += ','; j += String(f[i], 2); }
    j += '"';
  }
  nvs_close(h);
  // Fuel-flow curve lives in its own "fuel" namespace (float[FUEL_TABLE_SIZE], not the
  // learning-namespace RPM tables) — per-engine calibration, cloned like the cap tables.
  if (nvs_open("fuel", NVS_READONLY, &h) == ESP_OK) {
    float fr[FUEL_TABLE_SIZE], fg[FUEL_TABLE_SIZE];
    size_t sr = sizeof(fr), sg = sizeof(fg);
    if (nvs_get_blob(h, "fuelRPM", fr, &sr) == ESP_OK && sr == sizeof(fr) &&
        nvs_get_blob(h, "fuelGPH", fg, &sg) == ESP_OK && sg == sizeof(fg)) {
      if (!first) j += ',';
      first = false;
      j += "\"fuelRPM\":\"";
      for (int i = 0; i < FUEL_TABLE_SIZE; i++) { if (i) j += ','; j += String(fr[i], 1); }
      j += "\",\"fuelGPH\":\"";
      for (int i = 0; i < FUEL_TABLE_SIZE; i++) { if (i) j += ','; j += String(fg[i], 3); }
      j += '"';
    }
    nvs_close(h);
  }
  j += "}";
  return j;
}

// Build the shareable config blob: fw_version + vessel metadata (for the cloud
// table-of-contents) + the manifest "config" object + the RPM "tables" object.
// payload_v 2 = tables section added; older firmware ignores unknown sections, so
// blobs remain cross-rev compatible both ways.
String exportConfigJson() {
  String j;
  j.reserve(20480);   // vessel + config (manifest + commissioning-result blobs) + tables
  j = "{\"fw_version\":\"";
  j += FIRMWARE_VERSION;
  j += "\",\"payload_v\":2,\"vessel\":{";
  j += "\"boat_make_model\":";        cfgAppendJsonStr(j, BOAT_MAKE_MODEL);
  j += ",\"engine_make\":";           cfgAppendJsonStr(j, ENGINE_MAKE);
  j += ",\"engine_hp\":";             j += String((unsigned)ENGINE_HP);
  j += ",\"battery_voltage\":";       j += String((unsigned)BATTERY_VOLTAGE);
  j += ",\"battery_capacity_ah\":";   j += String(BatteryCapacity_Ah);
  j += ",\"battery_type\":";          cfgAppendJsonStr(j, BATTERY_TYPE);
  j += ",\"battery_make_model\":";    cfgAppendJsonStr(j, BATTERY_MAKE_MODEL);
  j += ",\"alternator_brand_model\":";cfgAppendJsonStr(j, ALTERNATOR_BRAND_MODEL);
  j += "},\"config\":";
  j += manifestConfigObject();
  j += ",\"tables\":";
  j += exportTablesObject();
  j += "}";
  return j;
}

// Extract a flat top-level value for "key" from a JSON object starting at 'from'.
// Quote-delimited needle prevents prefix collisions (e.g. TailCurrent vs TailCurrent_A).
// Quoted values fold \" and \\ escapes back (cfgAppendJsonStr produces them — e.g. the
// imu_zero value is itself a JSON string with quotes).
static bool cfgJsonExtract(const char *from, const char *key, String &val) {
  String needle = "\"";
  needle += key;
  needle += "\"";
  const char *p = strstr(from, needle.c_str());
  if (!p) return false;
  p += needle.length();
  while (*p == ' ' || *p == '\t' || *p == ':' ) p++;
  if (*p == '"') {
    p++;
    val = "";
    while (*p && *p != '"') {
      if (*p == '\\' && p[1]) p++;
      val += *p++;
    }
    return true;
  }
  const char *e = p;
  while (*e && *e != ',' && *e != '}' && *e != ' ' && *e != '\n' && *e != '\r' && *e != '\t') e++;
  val = ""; val.concat(p, e - p);
  return true;
}

// Strict comma-joined parsers for the "tables" section — exactly n values or reject.
static bool cfgCsvToFloat(const String &s, float *out, int n) {
  int pos = 0;
  for (int i = 0; i < n; i++) {
    if (pos >= (int)s.length()) return false;
    out[i] = atof(s.c_str() + pos);
    int c = s.indexOf(',', pos);
    if (c < 0) return i == n - 1;
    if (i == n - 1) return false;
    pos = c + 1;
  }
  return true;
}
static bool cfgCsvToInt(const String &s, int *out, int n) {
  int pos = 0;
  for (int i = 0; i < n; i++) {
    if (pos >= (int)s.length()) return false;
    out[i] = atoi(s.c_str() + pos);
    int c = s.indexOf(',', pos);
    if (c < 0) return i == n - 1;
    if (i == n - 1) return false;
    pos = c + 1;
  }
  return true;
}

// Apply the "tables" section: user-editable RPM tables into the "learning" namespace.
// A changed rpmPoints set invalidates the knee tracker the same way a manual breakpoint
// edit does (3_functions.ino handler) — reset BEFORE the imported minDutyTable blob lands
// so the two stay consistent with the source file.
// Live arrays reload at the end (import normally reboots; this covers noReboot=1).
static int applyImportTables(const char *body) {
  const char *tbl = strstr(body, "\"tables\"");
  if (!tbl) return 0;
  String val;
  int pts[RPM_TABLE_SIZE];
  bool havePts = cfgJsonExtract(tbl, "rpmPoints", val) && cfgCsvToInt(val, pts, RPM_TABLE_SIZE);
  if (havePts) {
    bool moved = false;
    for (int i = 0; i < RPM_TABLE_SIZE; i++) if (pts[i] != rpmTableRPMPoints[i]) moved = true;
    if (moved) { kneeLearnResetDefaults(); commissionClearStage(7); }   // imported breakpoints moved → Min% floor + Field decay stage stale
  }
  int applied = 0;
  nvs_handle_t h;
  if (nvs_open("learning", NVS_READWRITE, &h) != ESP_OK) return 0;
  if (havePts) { nvs_set_blob(h, "rpmPoints", pts, sizeof(pts)); applied++; }
  static const char *CFG_TABLE_FKEYS[] = { "capTable", "capPowerTable", "capTableLo", "capPowerTableLo", "minDutyTable" };
  for (size_t k = 0; k < sizeof(CFG_TABLE_FKEYS)/sizeof(CFG_TABLE_FKEYS[0]); k++) {
    float f[RPM_TABLE_SIZE];
    if (!cfgJsonExtract(tbl, CFG_TABLE_FKEYS[k], val) || !cfgCsvToFloat(val, f, RPM_TABLE_SIZE)) continue;
    nvs_set_blob(h, CFG_TABLE_FKEYS[k], f, sizeof(f));
    applied++;
  }
  if (applied) nvs_commit(h);
  nvs_close(h);
  // Fuel curve — separate "fuel" namespace + FUEL_TABLE_SIZE arrays. Both halves must
  // parse or neither is written (a half-updated curve would interpolate wrong).
  float fr[FUEL_TABLE_SIZE], fg[FUEL_TABLE_SIZE];
  if (cfgJsonExtract(tbl, "fuelRPM", val) && cfgCsvToFloat(val, fr, FUEL_TABLE_SIZE) &&
      cfgJsonExtract(tbl, "fuelGPH", val) && cfgCsvToFloat(val, fg, FUEL_TABLE_SIZE)) {
    nvs_handle_t fh;
    if (nvs_open("fuel", NVS_READWRITE, &fh) == ESP_OK) {
      nvs_set_blob(fh, "fuelRPM", fr, sizeof(fr));
      nvs_set_blob(fh, "fuelGPH", fg, sizeof(fg));
      nvs_commit(fh);
      nvs_close(fh);
      memcpy(fuelTableRPM, fr, sizeof(fr));   // live arrays for the noReboot=1 path
      memcpy(fuelTableGPH, fg, sizeof(fg));
      applied += 2;
    }
  }
  if (applied) loadLearningTableFromNVS();
  return applied;
}

// Apply an imported config blob. Only manifest + registry (allowlisted) keys plus the
// "tables" section are written — anything else in the body is ignored by construction.
// Returns count applied, or -1 if the body has no "config" object. settingWrite is
// compare-first so unchanged values cost no flash. Caller reboots so the new set loads cleanly.
int applyImportConfig(const char *body) {
  if (!body) return -1;
  const char *cfg = strstr(body, "\"config\"");
  if (!cfg) return -1;   // malformed / wrong shape — reject, don't half-apply
  int applied = 0;
  for (size_t i = 0; i < CONFIG_MANIFEST_COUNT; i++) {
    if (CONFIG_MANIFEST[i].tier == 3) continue;   // export-only: this device's own history, never adopted
    String val;
    if (cfgJsonExtract(cfg, CONFIG_MANIFEST[i].param, val)) {
      if (settingWrite(CONFIG_MANIFEST[i].nvsKey, val.c_str())) applied++;
    }
  }
  // Registry knobs — generic import mirroring the export side.
  // Macro for the same auto-prototype reason as the emit.
  #define CFG_IMPORT_REGISTRY(REG, COUNT) \
    for (size_t i = 0; i < COUNT; i++) { \
      if (cfgRegistrySkipped(REG[i].name)) continue; \
      String val; \
      if (cfgJsonExtract(cfg, REG[i].name, val)) { \
        char key[16]; \
        snprintf(key, sizeof(key), "%s", REG[i].name); \
        if (settingWrite(key, val.c_str())) applied++; \
      } \
    }
  CFG_IMPORT_REGISTRY(ALT_SETTINGS, ALT_SETTING_COUNT)
  CFG_IMPORT_REGISTRY(PERF_SETTINGS, PERF_SETTING_COUNT)
  #undef CFG_IMPORT_REGISTRY
  applied += applyImportTables(body);
  return applied;
}

// ══════════════════════════════════════════════════════════════════════════════
//  BATTERY HEALTH MONITOR — active DCIR test + capacity-vs-cycles trend.
//  Step generator: 6_functions.ino.  Globals/structs: Xregulator.ino.
// ══════════════════════════════════════════════════════════════════════════════

static uint32_t      bhSampleLastMs   = 0;
static const uint32_t BH_SAMPLE_INTERVAL_MS = 15;   // ~INA228 cadence; bounds buffer fill

// Per-tick sampler (control loop). INA228 IBV/Bcur directly — Victron's ~1 Hz is useless
// for a step edge, so bhStartTest() requires the INA source.
void bhSample(uint32_t nowMs) {
  if (!bhSamples || bhSampleCount >= bhSampleCap) return;
  if (nowMs - bhSampleLastMs < BH_SAMPLE_INTERVAL_MS) return;
  bhSampleLastMs = nowMs;
  bhSamples[bhSampleCount].tMs = nowMs;
  bhSamples[bhSampleCount].v   = IBV;
  bhSamples[bhSampleCount].i   = Bcur;
  bhSampleCount++;
}

// Floor for bhDwellMs, derived so the knobs can never produce an unfittable waveform:
// the fit averages the last THIRD of each dwell, so the first two thirds must cover the
// slewed traverse (bhStepDeltaA / TEST_ENTRY_RATE_A) plus a 1s settle margin. Below this
// the wave is a triangle that never reaches either level and every edge fails the ΔI gate.
uint32_t bhMinDwellMs() {
  float traverseMs = (bhStepDeltaA / TEST_ENTRY_RATE_A) * 1000.0f;
  return (uint32_t)(1.5f * (traverseMs + 1000.0f));
}

void bhAbort(const char *reason) {
  batteryHealthTestActive = false;
  bhTestState = 3;
  bhAbortReason = reason;
  queueConsoleMessageF("BATT HEALTH: test aborted — %s", reason);
}

bool bhStartTest() {
  if (bhTestState == 1)        { bhAbortReason = "already running";          return false; }
  if (!bhSamples || !bhResults){ bhAbortReason = "buffers unallocated";      return false; }
  if (RPM < 100)               { bhAbortReason = "engine not running";       return false; }
  if (sysMode != SYS_MODE_AUTO){ bhAbortReason = "must be in AUTO mode";     return false; }   // generator only runs in the AUTO control path
  if (TuningMode || CVTuningMode || systemIDActive || fieldCurveActive || fieldCutActive || cvPlantFitActive || resTestActive || cvStressActive || protTestActive) { bhAbortReason = "another test active"; return false; }
  if (BatteryCurrentSource != 0 || !HAS_BATT_SHUNT){ bhAbortReason = "needs INA228 battery shunt"; return false; }
  if (bhNumEdges < 3) bhNumEdges = 3;
  if (bhNumEdges > BH_MAX_TOGGLES - 3) bhNumEdges = BH_MAX_TOGGLES - 3;
  if (bhDwellMs < bhMinDwellMs()) bhDwellMs = bhMinDwellMs();   // belt-and-braces; set paths already clamp
  bhSampleCount  = 0;
  bhSampleLastMs = 0;
  bhWaveHigh     = false;
  bhEdgeCount    = 0;
  bhToggleCount  = 0;
  bhTestStartMs  = millis();
  bhLastToggleMs = bhTestStartMs;
  bhAbortReason  = "";
  bhTestState    = 1;
  batteryHealthTestActive = true;
  queueConsoleMessageF("BATT HEALTH: DCIR test started (%.1f<->%.1fA, %lus dwell, %u edges)",
                       bhStepLowA, bhStepLowA + bhStepDeltaA, (unsigned long)(bhDwellMs / 1000), bhNumEdges);
  return true;
}

// A macro, not a helper: a free function taking BattHealthResult by reference makes
// Arduino's auto-prototype generator hoist a prototype above the struct definition
// ("does not name a type"). Keep ALL struct types out of free-function signatures.
#define BH_APPEND_RESULT(r) do {                         \
    bhResults[bhResultHead] = (r);                       \
    bhResultHead = (bhResultHead + 1) % bhResultCap;     \
    if (bhResultCount < bhResultCap) bhResultCount++;    \
  } while (0)

// Resistance at the dwell timescale (ohmic + polarization within one dwell). The absolute
// value is timescale-dependent but consistent run-to-run, so the TREND is what matters.
// SEAM for a 3-level sweep (linearity check): the per-edge math already differences whatever
// ΔI occurred, so only the generator + reporting need to fan out to multiple magnitudes.
void bhComputeDcir() {
  uint32_t bhT0 = micros();   // this fit runs in a field-on loop pass — report its cost so a spike is attributable
  if (bhSampleCount < 8 || bhToggleCount < (int)bhNumEdges + 3) { bhAbort("not enough samples"); return; }

  // Average the SETTLED tail of each level (last `win` ms before its end toggle). End-of-dwell
  // windows are robust to the transition slew time — both levels are fully settled regardless
  // of how fast the setpoint got there, so ΔV/ΔI is the same as an abrupt step.
  uint32_t win = bhDwellMs / 3; if (win < 200) win = 200;

  float Rsum = 0.0f, Rsq = 0.0f; int Rn = 0;   // Rsq → per-edge std-dev (fit consistency)
  // Score toggle numbers k = 3 .. bhNumEdges+2 (1-indexed). Array index = k-1.
  for (int k = 3; k <= (int)bhNumEdges + 2; k++) {
    if (k >= bhToggleCount) break;
    uint32_t tEdge = bhToggleMs[k - 1];   // end of prior level
    uint32_t tNext = bhToggleMs[k];       // end of new level
    float vb = 0, ib = 0, va = 0, ia = 0;
    int   nb = 0, na = 0;
    for (int s = 0; s < bhSampleCount; s++) {
      uint32_t t = bhSamples[s].tMs;
      if (t + win >= tEdge && t < tEdge)        { vb += bhSamples[s].v; ib += bhSamples[s].i; nb++; }   // settled tail, prior level
      else if (t + win >= tNext && t < tNext)   { va += bhSamples[s].v; ia += bhSamples[s].i; na++; }   // settled tail, new level
    }
    if (nb < 2 || na < 2) continue;
    vb /= nb; ib /= nb; va /= na; ia /= na;
    float dV = va - vb, dI = ia - ib;
    if (fabsf(dI) < 0.5f * bhStepDeltaA) continue;     // step never manifested (protection clamp / field cut)
    float R = (dV / dI) * 1000.0f;                     // mΩ
    if (R < 0.1f || R > 500.0f) continue;              // sanity bound
    Rsum += R; Rsq += R * R; Rn++;
  }
  if (Rn < 2) { bhAbort("insufficient valid steps (protections firing or step too small?)"); return; }

  BattHealthResult r = {};
  r.epoch      = timeIsSynced ? (timeBase + (millis() - timeBaseMillis) / 1000) : 0;
  r.dcir_mOhm  = Rsum / Rn;
  r.soh_pct    = (bhCapCount > 0) ? bhCapRing[(bhCapHead - 1 + bhCapCap) % bhCapCap].capPct : NAN;
  r.boardTempF = ambientTemp;                          // board-temp proxy (°F)
  r.soc_pct    = SOC_percent / 100.0f;
  r.battV      = IBV;
  r.stepLowA   = bhStepLowA;
  r.stepDeltaA = bhStepDeltaA;
  r.edgesUsed  = (uint8_t)Rn;
  r.dwellMsUsed = (uint16_t)min(bhDwellMs, (uint32_t)65535);
  // population std-dev of the per-edge DCIR (r.dcir_mOhm is already the mean = Rsum/Rn)
  float bhVar = Rsq / Rn - r.dcir_mOhm * r.dcir_mOhm; if (bhVar < 0.0f) bhVar = 0.0f;
  r.fitSpread_mOhm = sqrtf(bhVar);
  BH_APPEND_RESULT(r);
  bhLastResultDcir = r.dcir_mOhm;
  bhTestState = 2;
  bhResultsDirty = true;   // NVS persist deferred to field-off — a test always ends field-on
  queueConsoleMessageF("BATT HEALTH: DCIR = %.2f mOhm (%d/%u edges, SoC %.0f%%, %.1fF, fit %luus)",
                       r.dcir_mOhm, Rn, bhNumEdges, r.soc_pct, r.boardTempF, (unsigned long)(micros() - bhT0));
}

// Called every loop() iteration. Cheap: a couple of compares unless a test is live.
void bhServiceCompletion() {
  if (bhTestState != 1) return;
  if (!batteryHealthTestActive) {           // control loop finished the last edge
    bhComputeDcir();
    return;
  }
  // Watchdog: if the engine stops mid-test the health branch stops toggling and the
  // test would hang RUNNING forever.
  uint32_t budget = (uint32_t)(bhNumEdges + 4) * bhDwellMs + 5000;
  if (millis() - bhTestStartMs > budget) bhAbort("timed out (engine stopped or left AUTO?)");
}

// ════════════════════════ CV PLANT FIT (firmware voltage-loop identification) ════════════════════════
// Ohmic-anchor pulse train (spec: CV_Gain_Ohmic_Anchor_Redesign_Spec.md): SETTLE (CC, capture baseDuty) →
// PILOT (probe + size the step under the OV/gassing/cap-table guards) → HOLD (CC practice run, capture
// stepDuty) → PULSES (4× abrupt base/step duty squares; the 2 s holds give settled baselines either side
// of every edge) → RELEASE. Fit = per-edge settled ΔV(150-250 ms)/ΔI, median of 8 edges → cvpfKa.
static const uint32_t CVPF_SAMPLE_INTERVAL_MS = 40;    // CC phases — ample for settled means; bounds buffer fill
static const uint32_t CVPF_SAMPLE_FAST_MS = 10;        // PULSES/RELEASE — tick-cadence sampling across the edges
static const uint32_t CVPF_SETTLE_MIN_MS = 2000;
static const uint32_t CVPF_SETTLE_TIMEOUT_MS = 15000;  // proceed with what settled — the fit divides by MEASURED ΔI
static const uint32_t CVPF_T_PILOT_MS  = 3000;
static const uint32_t CVPF_INBAND_MS   = 1000;         // measured-current dwell inside the band that counts as settled
static const uint32_t CVPF_HOLD_TIMEOUT_MS = 10000;
static const uint32_t CVPF_PULSE_SEG_MS = 2000;
static const uint32_t CVPF_COND_SKIP_MS = 900;         // wait out the post-edge reaction before averaging result-card conditions in a segment
static const uint32_t CVPF_EDGE_V0_MS = 550;           // ΔV readout ~600 ms after the edge — the timescale the CV loop
static const uint32_t CVPF_EDGE_V1_MS = 650;           //   reacts on; matched pair with cvAlpha. Inside the 2 s pulse hold
static const float    CVPF_DI_MAX_DEFAULT = 40.0f;     // baseline current-step ceiling; cvpfDiMaxA overrides per-run on a weak-signal re-run
static const float    CVPF_DI_MAX_CEIL    = 100.0f;    // absolute backstop for the operator-boosted step (cap-table clamp usually binds first)
static const float    CVPF_CAP_MARGIN_A   = 5.0f;      // keep the commanded step this far under the live cap-table ceiling (g_I_cap)
static const float    CVPF_V_RESERVE = 0.15f;          // keep the projected step peak this far below the hard-OV cut
static const float    CVPF_GASSING_V_12V = 14.2f;      // ~2.37 V/cell — oxygen-evolution onset on lead-acid

// Lithium does not evolve gas at any charge voltage we permit; every other chemistry does. "other" is treated
// as gassing: a false refusal costs the user a retry, a false pass ships an over-gained loop.
static bool cvpfChemGasses() {
  return !BATTERY_TYPE.equalsIgnoreCase("lifepo4");
}

void cvpfSample(uint32_t nowMs) {
  if (!cvpfBuf || cvpfBufCount >= cvpfBufCap) return;
  uint32_t interval = (cvpfPhase >= 3) ? CVPF_SAMPLE_FAST_MS : CVPF_SAMPLE_INTERVAL_MS;
  if (nowMs - cvpfSampleLastMs < interval) return;
  cvpfSampleLastMs = nowMs;
  cvpfBuf[cvpfBufCount].tMs  = nowMs;
  cvpfBuf[cvpfBufCount].v    = IBV;
  cvpfBuf[cvpfBufCount].iAlt = MeasuredAmps;
  cvpfBuf[cvpfBufCount].iBat = Bcur;
  cvpfBufCount++;
}

// Run-mean of ambient conditions for the result card. Called ONLY from the settled tail of the two middle
// pulse segments (one high step, one low base), so every sample sits on a stable current level — a
// steady-state average centered on the operating point, never an instant or a transient.
static void cvpfAccumConditions() {
  cvpfRpmSum += RPM; cvpfBattVSum += IBV; cvpfSocSum += SOC_percent / 100.0f; cvpfCondN++;
}

// One-pass stats over the sample buffer for [t0,t1): mean, linear slope (units per ms), RMS residual about
// the fitted line (ripple proxy). field: 0=v 1=iAlt 2=iBat. Returns sample count (0 → mean=NAN).
static int cvpfWinStats(uint32_t t0, uint32_t t1, int field, float &mean, float &slope, float &resid) {
  double sx = 0, sy = 0, sxx = 0, sxy = 0; int n = 0;
  for (int i = 0; i < cvpfBufCount; i++) {
    uint32_t t = cvpfBuf[i].tMs;
    if (t < t0 || t >= t1) continue;
    double x = (double)(t - t0);
    double y = (field == 0) ? cvpfBuf[i].v : (field == 1) ? cvpfBuf[i].iAlt : cvpfBuf[i].iBat;
    sx += x; sy += y; sxx += x * x; sxy += x * y; n++;
  }
  if (n == 0) { mean = NAN; slope = 0; resid = 0; return 0; }
  mean = (float)(sy / n);
  if (n >= 2) {
    double d = n * sxx - sx * sx;
    double b = (d > 1e-9) ? (n * sxy - sx * sy) / d : 0.0;
    double a = (sy - b * sx) / n;
    slope = (float)b;
    double ss = 0;
    for (int i = 0; i < cvpfBufCount; i++) {
      uint32_t t = cvpfBuf[i].tMs; if (t < t0 || t >= t1) continue;
      double x = (double)(t - t0);
      double y = (field == 0) ? cvpfBuf[i].v : (field == 1) ? cvpfBuf[i].iAlt : cvpfBuf[i].iBat;
      double e = y - (a + b * x); ss += e * e;
    }
    resid = (float)sqrt(ss / n);
  } else { slope = 0; resid = 0; }
  return n;
}

// Size the main step for cvpfTargetDV, capped by OV headroom (never below the pilot — need signal).
static void cvpfSizeStep() {
  // The pilot chord (~2-3 s) matches the 2 s pulse holds, so it sizes the step peak directly.
  float Klong    = cvpfPilotK;
  float Kupper   = Klong * 1.5f;                                             // pessimistic (bigger K → smaller ΔI)
  // CVPF cushions are fixed ABSOLUTE volts, not class-scaled: the probe ΔV is sensor-resolution-limited
  // (excellent noise floor), so a fixed step gives full SNR at any bank voltage and the step peak it bounds
  // is ~absolute. Scaling these up would only shrink the achievable step on a stiff 24/48V bank for no gain.
  float headroom = fmaxf(0.05f, AlternatorHardShutdownV - cvpfPilotVbase - CVPF_V_RESERVE);
  float di = cvpfDiMaxA;
  di = fminf(di, headroom / fmaxf(1e-3f, Kupper));
  di = fminf(di, cvpfTargetDV / fmaxf(1e-3f, Klong));
  // Never command past what the alternator cap table allows at this RPM (the step rides on top of cvpfBaseA).
  // Skip when g_I_cap is unset/tiny (low RPM) — the delivery-short warn catches under-delivery instead.
  if (g_I_cap > cvpfBaseA + cvpfPilotA + CVPF_CAP_MARGIN_A)
    di = fminf(di, g_I_cap - cvpfBaseA - CVPF_CAP_MARGIN_A);
  di = roundf(di);
  if (di < cvpfPilotA) di = cvpfPilotA;
  if (di > cvpfDiMaxA) di = cvpfDiMaxA;
  cvpfStepA = di;
}

// Pulse-train settle/segment trackers (reset in cvpfStartTest)
static float    cvpfAmpsEma = 0.0f;
static uint32_t cvpfInBandMs = 0;
static int8_t   cvpfSeg = -1;      // current pulse segment; even = base duty, odd = step duty

// Called every control tick from the duty-override block in AdjustField (like fieldCut_tick). Returns
// true only while PULSES/RELEASE own duty; CC phases return false. Ends by clearing cvPlantFitActive.
bool cvpf_tick(float &dutyOut, float measA, uint32_t nowMs) {
  if (!cvPlantFitActive) return false;
  cvpfSample(nowMs);
  uint32_t elapsed = nowMs - cvpfPhaseStartMs;
  switch (cvpfPhase) {
    case 0: {  // SETTLE — current-PID at the base level; capture the settled duty as the pulse floor
      cvpfCmdA = cvpfBaseA;
      cvpfAmpsEma += 0.05f * (measA - cvpfAmpsEma);
      bool arrived = fabsf(setpointLimited - cvpfBaseA) < 0.5f;
      bool inBand = arrived && fabsf(cvpfAmpsEma - cvpfBaseA) < fmaxf(2.0f, 0.15f * cvpfBaseA);
      if (inBand) { if (cvpfInBandMs == 0) cvpfInBandMs = nowMs; } else cvpfInBandMs = 0;
      bool settled = (elapsed >= CVPF_SETTLE_MIN_MS) && cvpfInBandMs && (nowMs - cvpfInBandMs >= CVPF_INBAND_MS);
      if (settled || elapsed >= CVPF_SETTLE_TIMEOUT_MS) {
        cvpfBaseDuty = lastAppliedDuty;
        cvpfPhase = 1; cvpfPhaseStartMs = nowMs;
      }
      break; }
    case 1: {  // PILOT — probe the fast stiffness, then gassing-gate and size the main step
      cvpfCmdA = cvpfBaseA + cvpfPilotA;
      if (elapsed >= CVPF_T_PILOT_MS) {
        float vB, vP, iBn, iPn, sl, rs;
        cvpfWinStats(cvpfPhaseStartMs - 2500, cvpfPhaseStartMs - 200, 0, vB,  sl, rs);   // last 2.3s of SETTLE
        cvpfWinStats(cvpfPhaseStartMs - 2500, cvpfPhaseStartMs - 200, 1, iBn, sl, rs);
        cvpfWinStats(nowMs - 1500, nowMs - 200, 0, vP,  sl, rs);                          // last ~1.3s of PILOT
        cvpfWinStats(nowMs - 1500, nowMs - 200, 1, iPn, sl, rs);
        float dIp = iPn - iBn, dVp = vP - vB;
        cvpfPilotK     = (dIp > 0.5f && dVp > 0.0f) ? (dVp / dIp) : (cvpfTargetDV / fmaxf(1.0f, cvpfPilotA));
        cvpfPilotVbase = isnan(vB) ? IBV : vB;
        // Gassing gate: past ~2.37 V/cell some charge current goes to oxygen evolution (Tafel resistance
        // ∝ 1/I_gas), dragging measured dV/dI DOWN → over-gained loop. Refuse rather than measure; judged
        // on baseline + the step about to be commanded (the step's peak gasses, not the rest voltage).
        if (cvpfChemGasses() && isfinite(cvpfPilotVbase)
            && cvpfPilotVbase + cvpfTargetDV > CVPF_GASSING_V_12V * (float)BATTERY_VOLTAGE / 12.0f) {
          cvpfAbort("bank too full to measure — the step would drive it into gassing, which reads the resistance low and over-tunes the loop. Draw the bank down with some loads, then re-run.");
          break;
        }
        cvpfSizeStep();
        cvpfPhase = 2; cvpfPhaseStartMs = nowMs; cvpfInBandMs = 0;
        queueConsoleMessageF("CV plant-fit: pilotK=%.1f mV/A -> pulse step %.0f A; practice run through the current loop",
                             cvpfPilotK * 1000.0f, cvpfStepA);
      }
      break; }
    case 2: {  // HOLD — practice run at the sized step; capture the settled duty as the pulse crest
      float target = cvpfBaseA + cvpfStepA;
      cvpfCmdA = target;
      // Runtime OV guard: first trip → shrink to a 180 mV step and re-hold; still over-volting → give up.
      // 0.10 V cushion is fixed absolute (the fixed-ΔV probe's overshoot is ~absolute, not per-cell).
      if (IBV > AlternatorHardShutdownV - 0.10f) {
        if (!cvpfFellBack) {
          cvpfFellBack = true; cvpfTargetDV = 0.18f; cvpfWarn |= 0x10;
          cvpfSizeStep();
          cvpfPhaseStartMs = nowMs; cvpfInBandMs = 0;
          queueConsoleMessageF("CV plant-fit: over-voltage headroom tight — step reduced to %.0f A, re-testing", cvpfStepA);
        } else {
          cvpfAbort("over-voltage during fit even at the reduced step — lower the charge target or raise OV headroom, then re-run");
        }
        break;
      }
      cvpfAmpsEma += 0.05f * (measA - cvpfAmpsEma);
      bool arrived = fabsf(setpointLimited - target) < 0.5f;
      bool inBand = arrived && fabsf(cvpfAmpsEma - target) < fmaxf(3.0f, 0.08f * target);
      if (inBand) { if (cvpfInBandMs == 0) cvpfInBandMs = nowMs; } else cvpfInBandMs = 0;
      bool settled = (elapsed >= CVPF_SETTLE_MIN_MS) && cvpfInBandMs && (nowMs - cvpfInBandMs >= CVPF_INBAND_MS);
      // Timeout = step unreachable here (railed PID): proceed — the fit divides by MEASURED ΔI.
      if (settled || elapsed >= CVPF_HOLD_TIMEOUT_MS) {
        cvpfStepDuty = lastAppliedDuty;
        // Restore the pre-test setpoint BEFORE first returning true — the SystemID resume snapshot fires
        // on this tick's rising edge and must not capture the test current (same trick as fieldCut_tick).
        setpointLimited = cvpfPreSetpoint;
        cvpfCcActive = false;
        cvpfSeg = 0;
        cvpfPhase = 3; cvpfPhaseStartMs = nowMs;
        queueConsoleMessageF("CV plant-fit: duties captured (base %.1f%% / step %.1f%%) — pulsing", cvpfBaseDuty, cvpfStepDuty);
        dutyOut = cvpfBaseDuty;
        return true;
      }
      break; }
    case 3: {  // PULSES — abrupt duty squares; every segment boundary is a measured edge
      if (IBV > AlternatorHardShutdownV - 0.10f) {
        cvpfAbort("over-voltage during the pulse train — re-run once charging is steady");
        break;
      }
      int seg = (int)(elapsed / CVPF_PULSE_SEG_MS);   // 0,2,4,6 = base; 1,3,5,7 = step
      if (seg > 7) {
        if (cvpfEdgeCount < 8) cvpfEdgeMs[cvpfEdgeCount++] = nowMs;   // final step→base edge opens RELEASE
        cvpfPhase = 4; cvpfPhaseStartMs = nowMs;
        dutyOut = cvpfBaseDuty;
        return true;
      }
      if (seg != cvpfSeg) {
        if (seg > 0 && cvpfEdgeCount < 8) cvpfEdgeMs[cvpfEdgeCount++] = nowMs;   // this tick writes the new duty
        cvpfSeg = seg;
      }
      // Result-card conditions: the two middle segments (3 = high step, 4 = low base), settled tail only —
      // one high + one low centers the reported voltage on the operating point being characterized.
      if ((seg == 3 || seg == 4) && (elapsed - (uint32_t)seg * CVPF_PULSE_SEG_MS) >= CVPF_COND_SKIP_MS)
        cvpfAccumConditions();
      dutyOut = (seg & 1) ? cvpfStepDuty : cvpfBaseDuty;
      return true; }
    case 4: {  // RELEASE — settled base hold that closes the final edge's ΔI window, then hand back
      dutyOut = cvpfBaseDuty;
      if (elapsed >= CVPF_PULSE_SEG_MS) {
        cvPlantFitActive = false;   // resume block reseeds the PID bumplessly at the applied (base) duty
        return false;
      }
      return true; }
  }
  return false;   // current-PID phases — the cvpfCcActive branch drives
}

// Median over n floats (n ≤ 8) — insertion sort on a copy.
static float cvpfMedian(const float *v, int n) {
  float s[8];
  for (int i = 0; i < n; i++) s[i] = v[i];
  for (int i = 1; i < n; i++) {
    float x = s[i]; int j = i - 1;
    while (j >= 0 && s[j] > x) { s[j + 1] = s[j]; j--; }
    s[j + 1] = x;
  }
  return (n & 1) ? s[n / 2] : 0.5f * (s[n / 2 - 1] + s[n / 2]);
}

// Per edge: ΔV = mean[tE+150,250 ms] − pre-edge baseline; ΔI = settled means either side (so current-
// sensor SPEED is irrelevant — the fast transient is all on the INA228 voltage). K = ΔV/ΔI, median over
// edges: a stretched tick or a load switch under one edge drops that edge instead of biasing the fit.
void cvpfProcess() {
  uint32_t tP = micros();
  cvpfReady = true;
  if (cvpfEdgeCount < 4 || cvpfBufCount < 50) { cvpfOk = false; cvpfState = 3; cvpfAbortMsg = "too few usable samples — re-run"; return; }
  bool noShunt = !HAS_BATT_SHUNT;
  int iField = noShunt ? 1 : 2;   // settled ΔI source: calibrated ADS1115 alt current, or the battery shunt
  float Ks[8], dVs[8], dIs[8], dIalts[8];
  float ripSum = 0.0f; int nK = 0, nAlt = 0, ripN = 0;
  for (int i = 0; i < 8; i++) { cvpfEdgeK[i] = 0.0f; cvpfEdgeStat[i] = 4; }   // 4 = not fired (< 8 edges recorded)
  for (int e = 0; e < cvpfEdgeCount; e++) {
    uint32_t tE = cvpfEdgeMs[e];
    float vPre, vPost, iPre, iPost, iaPre, iaPost, sl, rip, rs;
    int n1 = cvpfWinStats(tE - 600, tE - 100, 0, vPre, sl, rip);
    int n2 = cvpfWinStats(tE + CVPF_EDGE_V0_MS, tE + CVPF_EDGE_V1_MS, 0, vPost, sl, rs);
    int n3 = cvpfWinStats(tE - 1100, tE - 100, iField, iPre, sl, rs);
    int n4 = cvpfWinStats(tE + 500, tE + 1900, iField, iPost, sl, rs);
    int n5 = cvpfWinStats(tE - 1100, tE - 100, 1, iaPre, sl, rs);
    int n6 = cvpfWinStats(tE + 500, tE + 1900, 1, iaPost, sl, rs);
    if (n5 >= 3 && n6 >= 5) { dIalts[nAlt++] = fabsf(iaPost - iaPre); }   // delivery/gap-drift reference
    if (n1 < 3 || n2 < 2 || n3 < 3 || n4 < 5) { cvpfEdgeStat[e] = 3; continue; }   // a stretched tick starved a window — drop the edge
    float dV = vPost - vPre, dI = iPost - iPre;
    if (fabsf(dI) < 1.0f) { cvpfEdgeStat[e] = 1; continue; }               // no real current edge under this one
    float K = dV / dI;
    cvpfEdgeK[e] = K * 1000.0f;                                            // report the signed reading even when rejected
    if (!(K > 1e-5f)) { cvpfEdgeStat[e] = 2; continue; }                   // wrong-way edge — a load switched under it
    cvpfEdgeStat[e] = 0;
    Ks[nK] = K; dVs[nK] = fabsf(dV); dIs[nK] = fabsf(dI);
    ripSum += rip; ripN++;
    nK++;
  }
  if (nK < 3) {
    cvpfOk = false; cvpfState = 3;
    cvpfAbortMsg = noShunt ? "pulse edges unusable — no clean current step (weak alternator? raise RPM and re-run)"
                           : "pulse edges unusable — no clean battery-current step; re-run with loads steady";
    return;
  }
  cvpfKa = cvpfMedian(Ks, nK);
  cvpfKb = 0.0f;
  cvpfK  = cvpfKa;
  cvpfDV = cvpfMedian(dVs, nK);
  cvpfDI = cvpfMedian(dIs, nK);
  float ripple = (ripN > 0) ? (ripSum / ripN) : 0.0f;
  cvpfSNR = cvpfDV / fmaxf(1e-4f, ripple);
  // advisory gates (never block — the wizard shows them and the user still chooses Apply)
  if (nK < 5) cvpfWarn |= 0x04;                                          // edges dropped (stretched ticks / disturbances)
  float dIaltMed = (nAlt > 0) ? cvpfMedian(dIalts, nAlt) : 0.0f;
  if (dIaltMed / fmaxf(1e-3f, cvpfStepA) < 0.6f) cvpfWarn |= 0x01;       // delivery came up short
  if (cvpfSNR < 12.0f) cvpfWarn |= 0x02;                                 // weak signal vs ripple
  if (!noShunt && dIaltMed > 0.5f
      && fabsf(dIaltMed - cvpfDI) / dIaltMed > 0.15f) cvpfWarn |= 0x08;  // alt vs battery step disagree ⇒ a load moved
  // display Kp/Ki (mirrors recomputeCvGains; recomputeCvGains is authoritative when the user Applies Ka)
  float vNorm = 12.0f / (float)BATTERY_VOLTAGE;
  float kp = cvAlpha / fmaxf(1e-6f, cvpfKa * vNorm);
  float ki = cvPiZero * kp;                   // from the UN-clamped Kp, then clamp both — matches recomputeCvGains
  cvpfKp = clamp_f(kp, 2.0f, 120.0f);
  cvpfKi = clamp_f(ki, 1.0f, 80.0f);
  // Result-card conditions: mean over the settled tail of the two middle pulse segments (~1 s each). Fall back
  // to the instantaneous read only if the pulse train never reached those segments (aborted mid-run).
  cvpfRpmAtFit   = cvpfCondN ? (float)(cvpfRpmSum   / cvpfCondN) : RPM;
  cvpfBattVAtFit = cvpfCondN ? (float)(cvpfBattVSum / cvpfCondN) : IBV;
  cvpfSocAtFit   = (socInfoAvailable && cvpfCondN) ? (float)(cvpfSocSum / cvpfCondN) : -1.0f;  // −1 → SOC untrustworthy, UI blanks it
  cvpfCapHeadroomA = fmaxf(0.0f, g_I_cap - cvpfBaseA);   // how much bigger a step the alternator table allows at this RPM
  cvpfOk = true; cvpfState = 2;
  queueConsoleMessageF("CV plant-fit: K=%.1f mV/A (median of %d/%d edges) dV=%.0f mV dI=%.2f A SNR=%.0f -> Kp %.1f Ki %.1f warn=%d (%luus)",
                       cvpfKa * 1000.0f, nK, (int)cvpfEdgeCount, cvpfDV * 1000.0f, cvpfDI, cvpfSNR, cvpfKp, cvpfKi, (int)cvpfWarn,
                       (unsigned long)(micros() - tP));
}

bool cvpfStartTest(float diMaxReq) {
  if (cvpfState == 1)           { cvpfAbortMsg = "already running";        return false; }
  if (!cvpfBuf)                 { cvpfAbortMsg = "buffer unallocated";     return false; }
  if (RPM < 100)                { cvpfAbortMsg = "engine not running";     return false; }
  if (sysMode != SYS_MODE_AUTO) { cvpfAbortMsg = "must be in AUTO mode";   return false; }
  if (TuningMode || CVTuningMode || systemIDActive || batteryHealthTestActive || resTestActive || fieldCurveActive || fieldCutActive || cvStressActive || protTestActive) {
    cvpfAbortMsg = "another test active"; return false;
  }
  cvpfBufCount = 0; cvpfSampleLastMs = 0;
  cvpfPhase = 0; cvpfState = 1;
  cvpfPhaseStartMs = millis(); cvpfTestStartMs = cvpfPhaseStartMs;
  cvpfBaseA = 10.0f; cvpfPilotA = 6.0f; cvpfStepA = 6.0f;
  cvpfTargetDV = 0.30f; cvpfFellBack = false;  // fixed absolute ΔV probe (sensor-resolution-limited, not per-cell)
  cvpfCmdA = cvpfBaseA;
  cvpfDiMaxA = (diMaxReq > CVPF_DI_MAX_DEFAULT) ? fminf(diMaxReq, CVPF_DI_MAX_CEIL) : CVPF_DI_MAX_DEFAULT;
  if (cvpfDiMaxA > CVPF_DI_MAX_DEFAULT)
    queueConsoleMessageF("CV plant-fit: step ceiling raised to %.0f A for a stronger signal (weak-signal re-run)", cvpfDiMaxA);
  cvpfBaseDuty = 0.0f; cvpfStepDuty = 0.0f;
  cvpfEdgeCount = 0;
  for (int i = 0; i < 8; i++) { cvpfEdgeK[i] = 0.0f; cvpfEdgeStat[i] = 4; }   // clear the per-edge table; an early abort shows none
  cvpfPreSetpoint = setpointLimited;
  cvpfAmpsEma = MeasuredAmps; cvpfInBandMs = 0; cvpfSeg = -1;
  cvpfHorizonS = (CVPF_EDGE_V0_MS + CVPF_EDGE_V1_MS) / 2000.0f;   // the ~0.6 s edge readout (display only)
  cvpfReady = false; cvpfOk = false; cvpfWarn = 0; cvpfAbortMsg = "";
  cvpfK = cvpfDV = cvpfDI = cvpfSNR = 0.0f; cvpfKp = cvpfKi = 0.0f;
  cvpfRpmSum = cvpfBattVSum = cvpfSocSum = 0.0; cvpfCondN = 0;
  cvpfCcActive = true;
  cvPlantFitActive = true;
  queueConsoleMessage("CV plant-fit: started (settle -> size -> practice run -> 4 abrupt duty pulses, ~0.6s stiffness read)");
  return true;
}

void cvpfAbort(const char *reason) {
  cvPlantFitActive = false; cvpfCcActive = false;
  cvpfState = 3; cvpfReady = true; cvpfOk = false;
  cvpfAbortMsg = reason;
  queueConsoleMessageF("CV plant-fit: aborted — %s", reason);
}

// Called every loop(): runs the fit once the control-loop branch finishes, plus a hang watchdog.
void cvpfServiceCompletion() {
  if (cvpfState != 1) return;
  if (!cvPlantFitActive) { cvpfProcess(); return; }   // tick machine finished the RELEASE phase
  // Worst case: settle timeout + pilot + two hold timeouts (OV re-size) + 9 segments + slack.
  uint32_t budget = CVPF_SETTLE_TIMEOUT_MS + CVPF_T_PILOT_MS + 2 * CVPF_HOLD_TIMEOUT_MS
                    + 9 * CVPF_PULSE_SEG_MS + 8000;
  if (millis() - cvpfTestStartMs > budget) cvpfAbort("timed out (engine stopped or left AUTO?)");
}

// ═══════════════ CV STRESS TEST (commissioning stage 8 / standalone Tuning ▸ Stress Test) ═══════════════
// Provokes the over-voltage protection on a battery of ANY state of charge by first parking the bus at an
// ACHIEVABLE constant-voltage target, then having the operator snap the throttle. Flow:
//   1 STAB_IDLE : charge normally at idle ≥10 s, wait for the bus to hold steady (≤0.10 V over 3 s) AND
//                 stop settling up (≤0.10 V drift over 10 s, waited at most 30 s — the CVS_SETTLEUP_* gate).
//                 Then target := mean(last 2 s bus V) − CvStressDropV — guaranteed reachable, since idle
//                 already held above it. At the deadline, a swing ≤ CvStressFailBandV proceeds anyway (swing
//                 reported, pre-stab graded marginal); beyond it = basic-stability FAIL (test can't run).
//   2 STAB_CV   : force CV at that target, wait for the bus to re-settle onto it (same tiered rule) to within
//                 half the headroom — so the approach transient is over BEFORE the watch starts — then arm.
//   3 ARMED     : operator snaps the throttle; alternator output multiplies and pushes the (now reachable)
//                 bus over target → the OV protection fires. The watcher counts EVERY protection edge (no
//                 clustering — rapid re-clamping IS the defect), times the recovery, measures overshoot/valley.
//   4 DONE      : grade. The firmware ends the run itself, 5 s after the bus has re-settled at the target —
//                 the operator never decides when it is over.
// The test DRIVES the target (no longer a pure observer): it sets ChargingVoltageTargetReq through the
// cvStressForceCV hook in AdjustField, exactly like TargetVoltageMode. Every protection stays fully armed.
static const uint32_t CVS_IDLE_MIN_MS       = 10000;   // min idle charge before the stability gate opens
static const uint32_t CVS_STAB_WIN_MS       = 3000;    // stationarity window (idle and CV)
static const float    CVS_STAB_BAND_V       = 0.10f;   // max−min of the filtered bus over the window (×V/12) — setup gate, not the graded subject; keep loose enough that normal idle ripple passes
// The fail band (CvStressFailBandV) and target headroom (CvStressDropV) are user settings — Xregulator.ino globals.
static const uint32_t CVS_TARGET_AVG_MS     = 2000;    // averaging window for the achievable target
// Settle-up gate: charging keeps nudging the bus upward long after the 3 s window looks flat; a target
// fixed against a still-rising average reads low and the parked current dies away. Require the filtered
// bus to hold within the drift band for the full window (reference re-anchors on any excursion) — with
// the 0.10 V headroom this is what keeps CV, not the current limit, in command at the parked target.
static const float    CVS_SETTLEUP_BAND_V   = 0.10f;   // max drift (×V/12) of IBV_filtered around the settle reference
static const uint32_t CVS_SETTLEUP_WIN_MS   = 10000;   // drift-free hold required before the target is fixed
static const uint32_t CVS_SETTLEUP_MAX_MS   = 30000;   // settle-up wait cap — proceed anyway so a slow bank can't stall the test
static const float    CVS_RESETTLE_BAND_V   = 0.15f;   // |bus − target| that counts as "at setpoint" (×V/12) — post-snap recovery band; the ARM gate uses min(this, ½ CvStressDropV), which is what a headroom smaller than this band demands
static const uint32_t CVS_SETTLE_MS         = 5000;    // sustained in-band + stable = recovered / end
static const uint32_t CVS_IDLESTAB_TO_MS    = 45000;   // idle stability deadline — tight band missed: proceed wobbly (≤ fail band) or basic-stability fail
static const uint32_t CVS_CVSTAB_TO_MS      = 90000;   // CV settle deadline — the approach onto the target refills current at the Ki rate (closed-loop τ ~25 s on the bench; a 40 s deadline missed arming by 9 mV), so allow several time constants; tiered rule at the deadline like idle
static const uint32_t CVS_WATCH_MAX_MS      = 45000;   // armed watch backstop (never re-settles → no-recovery)
static const uint32_t CVS_POLL_DEADMAN_MS   = 20000;   // no /cvstress.json poll while active = browser gone
static const float    CVS_SNAP_RPM_DELTA    = 300.0f;  // rpmMax over arm RPM that counts as "a snap happened"
static const float    CVS_SLEW_SOFT_RPMPS   = 1000.0f; // no trip + slew below this = soft stimulus (snap harder)

// Filtered-bus sample ring for the CVS_STAB_BAND_V / CVS_STAB_WIN_MS stationarity gate + the 2 s target average.
static const int CVS_RING_N = 64;
static float    cvsRingV[CVS_RING_N];
static uint32_t cvsRingMs[CVS_RING_N];
static int      cvsRingHead = 0, cvsRingCount = 0;
static uint32_t cvsRingStartMs = 0, cvsRingLastMs = 0;
static void cvsStabReset(uint32_t nowMs) { cvsRingHead = cvsRingCount = 0; cvsRingStartMs = nowMs; cvsRingLastMs = 0; }
static void cvsStabPush(uint32_t nowMs, float v) {
  if (cvsRingLastMs != 0 && (uint32_t)(nowMs - cvsRingLastMs) < 100UL) return;   // ~10 Hz
  cvsRingLastMs = nowMs;
  cvsRingV[cvsRingHead] = v; cvsRingMs[cvsRingHead] = nowMs;
  cvsRingHead = (cvsRingHead + 1) % CVS_RING_N;
  if (cvsRingCount < CVS_RING_N) cvsRingCount++;
}
static bool cvsStabReady(uint32_t nowMs, uint32_t winMs) { return (uint32_t)(nowMs - cvsRingStartMs) >= winMs && cvsRingCount >= 3; }
static float cvsStabRange(uint32_t nowMs, uint32_t winMs) {   // max−min over the last winMs, order-independent
  float lo = 1e9f, hi = -1e9f;
  for (int i = 0; i < cvsRingCount; i++)
    if ((uint32_t)(nowMs - cvsRingMs[i]) <= winMs) { if (cvsRingV[i] < lo) lo = cvsRingV[i]; if (cvsRingV[i] > hi) hi = cvsRingV[i]; }
  return (hi >= lo) ? (hi - lo) : 0.0f;
}
static float cvsStabMean(uint32_t nowMs, uint32_t winMs) {
  float sum = 0.0f; int n = 0;
  for (int i = 0; i < cvsRingCount; i++)
    if ((uint32_t)(nowMs - cvsRingMs[i]) <= winMs) { sum += cvsRingV[i]; n++; }
  return n ? sum / n : IBV_filtered;
}

// Module state (all consumed within one core; result fields read by the JSON builder)
static uint32_t cvsPhaseStartMs = 0, cvsTestStartMs = 0, cvsLastEndMs = 0;
static volatile uint32_t cvsLastPollMs = 0;
static uint32_t cvsRpmLowSinceMs = 0;
static float    cvsIdleAvgV = 0.0f, cvsBattStartA = 0.0f, cvsArmRpm = 0.0f;
// settle-up gate state (phase 1): reference re-anchors whenever the filtered bus drifts beyond the band
static float    cvsSettleRefV = 0.0f;
static uint32_t cvsSettleRefMs = 0;
// arm baselines (raw protection counters)
static uint32_t cvsClamp0 = 0, cvsClampPrev = 0, cvsHoc0 = 0, cvsLd0 = 0, cvsIx0 = 0;
// watch-window trackers
static float    cvsPeakV = 0.0f, cvsMinV = 0.0f, cvsOvershootV = 0.0f, cvsValleyV = 0.0f;
static float    cvsRpmMax = 0.0f, cvsRpmSlewMax = 0.0f, cvsRpmPrev = 0.0f;
static uint32_t cvsRpmPrevMs = 0;
static uint16_t cvsEvents = 0, cvsHardCuts = 0, cvsIxTrips = 0;
static float    cvsWobbleIdleV = 0.0f, cvsWobbleCvV = 0.0f, cvsWobblePostV = 0.0f;  // measured 3 s swing (max−min) at each phase's settle, clean or loose; 0 = that phase never completed
static uint32_t cvsMaxLockoutMs = 0, cvsFirstEventMs = 0, cvsResettleSinceMs = 0, cvsRecoveryMs = 0;
static bool     cvsSnapDetected = false, cvsRecovered = false, cvsSettled = false, cvsSoftStimulus = false;
static bool     cvsLoosePost = false;   // post-snap settle was outside the tight band — drives the marginal stability grade (the wobble V is recorded even on a clean settle)
static uint8_t  cvsFailPhase = 0;       // phase at a basic-stability fail (1 idle / 2 CV) — 0 = the run armed; lets the web card name the criterion that actually failed
// grade: 0 pass, 1 marginal, 2 fail, 3 n/a. outcome: 1 rode-it-out, 2 event-graded, 3 stability-fail
static uint8_t  cvsOutcome = 0;
static uint8_t  cvsGPreStab = 0, cvsGEvents = 3, cvsGHardCut = 3, cvsGRecovery = 3, cvsGStab = 3, cvsGOverall = 3;
static bool     cvsReady = false, cvsOk = false, cvsAborted = false;

bool cvStressStartTest() {
  if (cvStressActive)           { cvStressAbortMsg = "already running";      return false; }
  if (RPM < 100)                { cvStressAbortMsg = "engine not running";   return false; }
  if (sysMode != SYS_MODE_AUTO) { cvStressAbortMsg = "must be in AUTO mode"; return false; }
  if (TuningMode || CVTuningMode || systemIDActive || batteryHealthTestActive || resTestActive || fieldCurveActive || fieldCutActive || cvPlantFitActive || protTestActive) {
    cvStressAbortMsg = "another test active"; return false;
  }
  if (millis() - cvsLastEndMs < 2000UL) { cvStressAbortMsg = "cooling down — retry in a moment"; return false; }
  uint32_t now = millis();
  cvStressPhase = 1;
  cvsPhaseStartMs = cvsTestStartMs = now;
  cvsLastPollMs = 0; cvsRpmLowSinceMs = 0;
  cvStressForceCV = false;
  cvStressTargetV = 0.0f; cvsIdleAvgV = 0.0f; cvsArmRpm = 0.0f;
  cvsEvents = cvsHardCuts = cvsIxTrips = 0;
  cvsMaxLockoutMs = 0; cvsFirstEventMs = 0; cvsResettleSinceMs = 0; cvsRecoveryMs = 0;
  cvsSnapDetected = cvsRecovered = cvsSettled = cvsSoftStimulus = cvsLoosePost = false;
  cvsOvershootV = cvsValleyV = 0.0f;
  cvsWobbleIdleV = cvsWobbleCvV = cvsWobblePostV = 0.0f;
  cvsPeakV = cvsMinV = IBV; cvsRpmMax = RPM; cvsRpmSlewMax = 0.0f;
  cvsOutcome = 0; cvsFailPhase = 0;
  cvsGPreStab = 0; cvsGEvents = cvsGHardCut = cvsGRecovery = cvsGStab = cvsGOverall = 3;
  cvsReady = cvsOk = cvsAborted = false;
  cvStressAbortMsg = "";
  cvStressAbortRequested = false;
  cvsStabReset(now);
  cvsSettleRefV = IBV_filtered; cvsSettleRefMs = now;
  cvStressActive = true;
  queueConsoleMessage("CV stress test: started — charging at idle while the battery settles (typically 10-45 s)");
  return true;
}

void cvStressAbort(const char *reason) {
  cvStressForceCV = false;
  cvStressActive = false;
  cvsReady = true; cvsOk = false; cvsAborted = true;
  cvStressAbortMsg = reason;
  cvsLastEndMs = millis();
  queueConsoleMessageF("CV stress test: aborted — %s", reason);
}

// Serialize + persist the graded result so the wizard card and Diag survive a reload/reboot.
// ver-2 positional blob (ver-1 layout is incompatible; the web parser reads the leading version).
// The three wobble fields, the two graded stability bands (all V×100; bands = constant × V/12
// at grade time, so failBand reflects the live CvStressFailBandV), then the fail phase are
// appended — the parser reads missing trailing fields as 0, so older blobs degrade gracefully.
static void cvStressPersistResult() {
  const float k = (float)BATTERY_VOLTAGE / 12.0f;
  snprintf(cvsLastBlob, sizeof(cvsLastBlob),
           "2,%d,%d,%d,%d,%d,%d,%d,%lu,%lu,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d",
           (int)cvsOutcome, (int)cvsGOverall, (int)cvsGPreStab, (int)cvsGEvents, (int)cvsGHardCut,
           (int)cvsGRecovery, (int)cvsGStab, (unsigned long)cvsRecoveryMs, (unsigned long)cvsMaxLockoutMs,
           (int)cvsEvents, (int)cvsHardCuts, (int)cvsIxTrips,
           (int)lroundf(cvsOvershootV * 100.0f), (int)lroundf(cvsValleyV * 100.0f),
           (int)lroundf(cvsPeakV * 100.0f), (int)lroundf(cvsMinV * 100.0f),
           (int)lroundf(cvsRpmSlewMax), (int)lroundf(cvsRpmMax),
           (int)lroundf(cvsBattStartA * 10.0f), (int)lroundf(cvStressTargetV * 100.0f),
           (int)(cvsSoftStimulus ? 1 : 0),
           (int)lroundf(cvsWobbleIdleV * 100.0f), (int)lroundf(cvsWobbleCvV * 100.0f),
           (int)lroundf(cvsWobblePostV * 100.0f),
           (int)lroundf(CVS_STAB_BAND_V * k * 100.0f), (int)lroundf(CvStressFailBandV * k * 100.0f),
           (int)cvsFailPhase);
  settingWrite(NK_cvStressLast, cvsLastBlob);
}

// Grade a run that reached ARMED (armed → snapped). Overshoot/valley are reported, never gated.
static void cvStressGrade() {
  cvsGEvents  = (cvsEvents <= 1) ? 0 : (cvsEvents == 2) ? 1 : 2;   // 1 ideal, 2 marginal, 3+ fail
  cvsGHardCut = (cvsHardCuts > 0) ? 2 : 0;                          // any INA228 current cut = fail
  if (cvsFirstEventMs == 0) {
    cvsOutcome = 1;                                                 // rode it out — no protection event
    cvsGRecovery = 3;                                               // n/a
    cvsSoftStimulus = (cvsRpmSlewMax < CVS_SLEW_SOFT_RPMPS);        // weak snap → recommend a harder one
  } else {
    cvsOutcome = 2;
    cvsGRecovery = !cvsRecovered ? 2 : (cvsRecoveryMs <= 12000) ? 0 : (cvsRecoveryMs <= 20000) ? 1 : 2;
    cvsSoftStimulus = false;
  }
  cvsGStab = !cvsSettled ? 2 : cvsLoosePost ? 1 : 0;                // clean stationarity / settled-but-wobbling / never re-settled
  uint8_t worst = cvsGPreStab;                                      // 0 for any run that armed
  uint8_t gs[4] = { cvsGEvents, cvsGHardCut, cvsGRecovery, cvsGStab };
  for (int i = 0; i < 4; i++) if (gs[i] != 3 && gs[i] > worst) worst = gs[i];
  cvsGOverall = worst;
  cvsReady = true; cvsOk = true;
  queueConsoleMessageF("CV stress test: %s — events=%d hardCut=%d recov=%lums overshoot=%.2fV valley=%.2fV rpmSlew=%.0f/s%s",
                       cvsGOverall == 0 ? "PASS" : cvsGOverall == 1 ? "MARGINAL" : "FAIL",
                       (int)cvsEvents, (int)cvsHardCuts, (unsigned long)cvsRecoveryMs,
                       cvsOvershootV, cvsValleyV, cvsRpmSlewMax, cvsSoftStimulus ? " (soft stimulus — snap harder)" : "");
}

// Basic-stability failure: the bus was swinging beyond CvStressFailBandV at a stabilization
// deadline (a swing at or under it proceeds with a marginal grade instead — see the phase logic).
// Recorded as a graded FAIL result — not a silent abort — so the card shows why the test couldn't run.
static void cvStressStabilityFail(const char *where) {
  cvStressForceCV = false;
  cvsOutcome = 3; cvsGPreStab = 2;
  cvsFailPhase = cvStressPhase;
  cvsGEvents = cvsGHardCut = cvsGRecovery = cvsGStab = 3;
  cvsGOverall = 2;
  cvStressPersistResult();
  cvsReady = true; cvsOk = true; cvsAborted = false;
  cvStressActive = false; cvStressPhase = 4; cvsLastEndMs = millis();
  queueConsoleMessageF("CV stress test: FAIL — %s", where);
}

// Called every AdjustField pass (before the mode branches, alongside the other test tick machines).
// Phases 1-2 wait for stationarity; phase 2-3 drive CV at cvStressTargetV via the cvStressForceCV hook.
void cvStress_tick(uint32_t nowMs) {
  if (!cvStressActive) return;
  float    k       = (float)BATTERY_VOLTAGE / 12.0f;
  uint32_t elapsed = nowMs - cvsPhaseStartMs;

  if (cvStressAbortRequested) { cvStressAbortRequested = false; cvStressAbort("cancelled by user"); return; }
  if (sysMode != SYS_MODE_AUTO) { cvStressAbort("left AUTO mode"); return; }
  // Signed compare: a poll can land between this pass's nowMs capture and this check, stamping
  // cvsLastPollMs a hair AHEAD of nowMs — unsigned subtraction would wrap to ~4.3e9 and abort.
  if (cvsLastPollMs != 0 && (int32_t)(nowMs - cvsLastPollMs) > (int32_t)CVS_POLL_DEADMAN_MS) {
    cvStressAbort("browser connection lost"); return;
  }
  // Engine-stall guard, suppressed inside the post-cut tach false-zero window (a hard cut collapses
  // the LM2907 pickup to a ~4.6 s false zero, exactly when the ARMED watcher is busiest).
  bool protGrace = (g_lastProtClampMs != 0) && ((uint32_t)(nowMs - g_lastProtClampMs) < PROT_RPM_GRACE_MS + 2000UL);
  if (RPM < 80.0f && !protGrace) { if (cvsRpmLowSinceMs == 0) cvsRpmLowSinceMs = nowMs; } else cvsRpmLowSinceMs = 0;
  if (cvsRpmLowSinceMs != 0 && (uint32_t)(nowMs - cvsRpmLowSinceMs) > 1500UL) { cvStressAbort("engine stopped"); return; }
  if (nowMs - cvsTestStartMs > CVS_IDLESTAB_TO_MS + CVS_CVSTAB_TO_MS + CVS_WATCH_MAX_MS + 20000UL) {
    cvStressAbort("timed out"); return;
  }

  cvsStabPush(nowMs, IBV_filtered);
  bool ready  = cvsStabReady(nowMs, CVS_STAB_WIN_MS);
  bool stable = ready && (cvsStabRange(nowMs, CVS_STAB_WIN_MS) <= CVS_STAB_BAND_V * k);

  switch (cvStressPhase) {
    case 1: {  // STAB_IDLE — charge normally ≥10 s, wait for the bus to hold steady AND stop settling up, then fix the target
      if (fabsf(IBV_filtered - cvsSettleRefV) > CVS_SETTLEUP_BAND_V * k) { cvsSettleRefV = IBV_filtered; cvsSettleRefMs = nowMs; }
      bool  settledUp = (uint32_t)(nowMs - cvsSettleRefMs) >= CVS_SETTLEUP_WIN_MS || elapsed >= CVS_SETTLEUP_MAX_MS;
      bool  timedOut = elapsed > CVS_IDLESTAB_TO_MS;
      float range    = ready ? cvsStabRange(nowMs, CVS_STAB_WIN_MS) : 1e9f;
      bool  wobblyOk = timedOut && range <= CvStressFailBandV * k;  // tight band missed by the deadline but the swing is workable — proceed and report it
      if (elapsed >= CVS_IDLE_MIN_MS && settledUp && (stable || wobblyOk)) {
        if ((uint32_t)(nowMs - cvsSettleRefMs) < CVS_SETTLEUP_WIN_MS)
          queueConsoleMessage("CV stress test: bus still creeping at the settle-up cap — target may read slightly low");
        cvsWobbleIdleV = range;
        if (!stable) cvsGPreStab = 1;
        cvsIdleAvgV = cvsStabMean(nowMs, CVS_TARGET_AVG_MS);
        cvStressTargetV = cvsIdleAvgV - CvStressDropV * k;   // achievable: idle already held above it
        cvStressForceCV = true;                                   // AdjustField now parks CV at cvStressTargetV
        if (!stable)
          queueConsoleMessageF("CV stress test: idle voltage still swinging %.2f V (within the %.2f V limit) — proceeding, target %.2f V; stability graded marginal", range, CvStressFailBandV * k, cvStressTargetV);
        else
          queueConsoleMessageF("CV stress test: battery stabilized at %.2f V — target set to %.2f V, entering CV", cvsIdleAvgV, cvStressTargetV);
        cvStressPhase = 2; cvsPhaseStartMs = nowMs;
        cvsStabReset(nowMs);
      } else if (timedOut) {
        cvStressStabilityFail("battery voltage swinging beyond the fail limit at idle");
      }
      break; }
    case 2: {  // STAB_CV — CV forced at the new target; wait for the bus to re-settle onto it, then arm
      // Arm band is a FRACTION of the commanded headroom and references cvStressTargetV, never the slewed
      // ChargingVoltageTarget: a fixed 0.15 V band wider than a 0.10 V CvStressDropV armed the instant the
      // slew landed, bus still at idle level, so the whole approach ran inside the armed watch. Window mean
      // (not the instantaneous sample) so ripple inside the stability band can't defeat the tighter band.
      float armBand  = fminf(CVS_RESETTLE_BAND_V, 0.5f * CvStressDropV) * k;
      float busAvg   = ready ? cvsStabMean(nowMs, CVS_STAB_WIN_MS) : IBV_filtered;
      bool  atTarget = fabsf(busAvg - cvStressTargetV) <= armBand;
      bool  timedOut = elapsed > CVS_CVSTAB_TO_MS;
      float range    = ready ? cvsStabRange(nowMs, CVS_STAB_WIN_MS) : 1e9f;
      // Deadline tier: the Ki-paced approach can park steady a hair outside the tight arm band
      // (bench: 9 mV short at the old 40 s deadline), so anything within the full re-settle band
      // arms with a marginal grade instead of failing — CV stays forced through ARMED, so the bus
      // keeps closing the last millivolts while waiting for the snap.
      bool  nearTarget = fabsf(busAvg - cvStressTargetV) <= CVS_RESETTLE_BAND_V * k;
      bool  wobblyOk = timedOut && nearTarget && range <= CvStressFailBandV * k;
      if ((stable && atTarget) || wobblyOk) {
        cvsWobbleCvV = range;
        if (!stable) {
          if (cvsGPreStab < 1) cvsGPreStab = 1;
          queueConsoleMessageF("CV stress test: CV holding target but swinging %.2f V — arming anyway; stability graded marginal", range);
        } else if (!atTarget) {
          if (cvsGPreStab < 1) cvsGPreStab = 1;
          queueConsoleMessageF("CV stress test: bus steady but %.2f V from the %.2f V target at the deadline — arming anyway; stability graded marginal", fabsf(busAvg - cvStressTargetV), cvStressTargetV);
        }
        cvsClamp0 = cvsClampPrev = g_fastOvClampCount;
        cvsHoc0 = g_hardOCCount; cvsLd0 = g_loadDumpCount; cvsIx0 = g_iExcessCount;
        cvsBattStartA = Bcur;
        cvsArmRpm = RPM;
        cvsPeakV = cvsMinV = IBV;
        cvsRpmMax = cvsRpmPrev = RPM; cvsRpmPrevMs = nowMs;
        cvStressPhase = 3; cvsPhaseStartMs = nowMs;
        cvsStabReset(nowMs);
        queueConsoleMessageF("CV stress test: ARMED at %.2f V target, bus settled to %.2f V — snap the throttle to ~half max RPM, then back to idle", cvStressTargetV, busAvg);
      } else if (timedOut) {
        cvStressStabilityFail(nearTarget ? "voltage swinging beyond the fail limit in CV mode"
                                         : "voltage would not settle onto the CV target");
      }
      break; }
    case 3: {  // ARMED — CV still forced; the watcher observes. Firmware ends the run itself.
      if (IBV > cvsPeakV) cvsPeakV = IBV;
      if (RPM > cvsRpmMax) cvsRpmMax = RPM;
      if (cvsRpmMax >= cvsArmRpm + CVS_SNAP_RPM_DELTA) cvsSnapDetected = true;
      // Stimulus slew — skipped through the tach false-zero grace so the 0→real re-bias jump after a
      // cut is not read as a huge fake positive slew. The real ramp happens BEFORE the trip.
      if ((uint32_t)(nowMs - cvsRpmPrevMs) >= 100UL) {
        if (!protGrace && RPM > 100.0f && cvsRpmPrev > 100.0f) {
          float slew = (RPM - cvsRpmPrev) / ((nowMs - cvsRpmPrevMs) * 0.001f);
          if (slew > cvsRpmSlewMax) cvsRpmSlewMax = slew;
        }
        cvsRpmPrev = RPM; cvsRpmPrevMs = nowMs;
      }
      // OV events = every fast-OV clamp rising edge (NO clustering — chatter is the defect).
      uint32_t clampNow = g_fastOvClampCount;
      if (clampNow != cvsClampPrev) {
        cvsEvents = (uint16_t)fminf(65535.0f, (float)(clampNow - cvsClamp0));
        cvsClampPrev = clampNow;
        if (cvsFirstEventMs == 0) cvsFirstEventMs = nowMs;
        cvsResettleSinceMs = 0;   // a fresh event restarts the re-settle clock
      }
      // Hard current cut = INA228-driven field cut (hard over-current OR load dump), NOT the OV clamp.
      cvsHardCuts = (uint16_t)((g_hardOCCount - cvsHoc0) + (g_loadDumpCount - cvsLd0));
      cvsIxTrips  = (uint16_t)(g_iExcessCount - cvsIx0);
      if (fieldCollapseTime != 0 && activeCollapseDelay > cvsMaxLockoutMs) cvsMaxLockoutMs = activeCollapseDelay;
      // All three graded distances reference the COMMANDED cvStressTargetV, not the slewed
      // ChargingVoltageTarget — a target that moves mid-watch would silently rescale them.
      if (IBV - cvStressTargetV > cvsOvershootV) cvsOvershootV = IBV - cvStressTargetV;   // peak overshoot (even a no-trip near-miss)
      if (cvsFirstEventMs != 0) {
        if (IBV < cvsMinV) cvsMinV = IBV;
        cvsValleyV = fmaxf(0.0f, cvStressTargetV - cvsMinV);                 // valley depth below target (report)
      }
      // Re-settle detector = provoked (an event fired, or a snap happened) then held in-band + stable
      // for 5 s. Works from BELOW the target (the field-dump valley), which is the real recovery path.
      bool provoked = (cvsFirstEventMs != 0) || cvsSnapDetected;
      bool inBand    = fabsf(IBV_filtered - cvStressTargetV) <= CVS_RESETTLE_BAND_V * k;
      if (provoked && inBand) { if (cvsResettleSinceMs == 0) cvsResettleSinceMs = nowMs; }
      else cvsResettleSinceMs = 0;
      if (provoked && !cvsSettled && cvsResettleSinceMs != 0
          && (uint32_t)(nowMs - cvsResettleSinceMs) >= CVS_SETTLE_MS) {
        // Tight band settles clean at 5 s in-band (grade 0, unchanged). A wobble that never
        // tightens gets a second CVS_SETTLE_MS of grace, then settles "loose" if within the fail
        // band — swing reported, graded marginal, never a FAIL. Beyond the fail band it keeps
        // waiting → WATCH_MAX graded fail as before.
        float range = ready ? cvsStabRange(nowMs, CVS_STAB_WIN_MS) : 1e9f;
        bool  loose = ((uint32_t)(nowMs - cvsResettleSinceMs) >= 2UL * CVS_SETTLE_MS)
                      && range <= CvStressFailBandV * k;
        if (stable || loose) {
          cvsWobblePostV = range;
          if (!stable) cvsLoosePost = true;
          cvsSettled = true;
          if (cvsFirstEventMs != 0) { cvsRecovered = true; cvsRecoveryMs = cvsResettleSinceMs - cvsFirstEventMs; }
        }
      }
      if (!cvsSettled && elapsed >= CVS_WATCH_MAX_MS && !provoked) {
        cvStressAbort("no throttle snap detected — re-run and snap to ~half max RPM"); return;
      }
      if (cvsSettled || elapsed >= CVS_WATCH_MAX_MS) {   // provoked but never re-settled = graded fail
        cvStressForceCV = false;
        cvStressGrade();
        cvStressPersistResult();
        cvStressActive = false;
        cvStressPhase = 4;
        cvsLastEndMs = nowMs;
      }
      break; }
    default: break;
  }
}

// /cvstress.json builder — also stamps the poll deadman.
int cvStressJsonBuild(char *buf, int cap) {
  cvsLastPollMs = millis();
  const float k = (float)BATTERY_VOLTAGE / 12.0f;   // graded bands are 12V-equiv constants × V/12
  return snprintf(buf, cap,
                  "{\"active\":%d,\"phase\":%d,\"ready\":%d,\"ok\":%d,\"aborted\":%d,\"abort\":\"%s\","
                  "\"targetV\":%.2f,\"idleAvgV\":%.2f,"
                  "\"events\":%d,\"ix\":%d,\"hardCuts\":%d,\"lockMs\":%lu,"
                  "\"overV\":%.2f,\"valleyV\":%.2f,\"peakV\":%.2f,\"minV\":%.2f,\"rpmMax\":%.0f,\"rpmSlew\":%.0f,\"battA\":%.1f,"
                  "\"recovMs\":%lu,\"recovered\":%d,\"settled\":%d,\"softStim\":%d,"
                  "\"outcome\":%d,\"overall\":%d,\"gPreStab\":%d,\"gEvents\":%d,\"gHardCut\":%d,\"gRecov\":%d,\"gStab\":%d,\"failPhase\":%d,"
                  "\"wobIdleV\":%.2f,\"wobCvV\":%.2f,\"wobPostV\":%.2f,"
                  "\"stabBandV\":%.2f,\"failBandV\":%.2f,"
                  "\"tune\":{\"gainMode\":%d,\"alpha\":%.3f,\"td\":%.2f,\"kp\":%.2f,\"ki\":%.2f,\"kd\":%.1f,\"recovEn\":%d},"
                  "\"last\":\"%s\"}",
                  cvStressActive ? 1 : 0, (int)cvStressPhase, cvsReady ? 1 : 0, cvsOk ? 1 : 0, cvsAborted ? 1 : 0, cvStressAbortMsg,
                  cvStressTargetV, cvsIdleAvgV,
                  (int)cvsEvents, (int)cvsIxTrips, (int)cvsHardCuts, (unsigned long)cvsMaxLockoutMs,
                  cvsOvershootV, cvsValleyV, cvsPeakV, cvsMinV, cvsRpmMax, cvsRpmSlewMax, cvsBattStartA,
                  (unsigned long)cvsRecoveryMs, cvsRecovered ? 1 : 0, cvsSettled ? 1 : 0, cvsSoftStimulus ? 1 : 0,
                  (int)cvsOutcome, (int)cvsGOverall, (int)cvsGPreStab, (int)cvsGEvents, (int)cvsGHardCut, (int)cvsGRecovery, (int)cvsGStab, (int)cvsFailPhase,
                  cvsWobbleIdleV, cvsWobbleCvV, cvsWobblePostV,
                  CVS_STAB_BAND_V * k, CvStressFailBandV * k,
                  (int)cvGainMode, cvAlpha, CvKdTd, VoltageKp, VoltageKi, VoltageKd, (int)cvRecovEnable,
                  cvsLastBlob);
}

// ── Capacity tracker (OCV-anchored) — see Xregulator.ino cap globals + dev doc ──
// Rested battery voltage → SoC% via the editable capOcvVolt[] table (12V-referenced, scaled
// to the bank). Table descends in both SoC and voltage. Returns 100 above the top row, 0 below
// the bottom. This is the INDEPENDENT low anchor that makes fade measurable (no coulomb count).
float ocvToSoC(float restedV) {
  float vScale = (float)BATTERY_VOLTAGE / 12.0f;
  if (restedV >= capOcvVolt[0] * vScale) return 100.0f;
  for (int i = 0; i < CAP_OCV_ROWS - 1; i++) {
    float vHi = capOcvVolt[i]     * vScale;   // higher SoC, higher V
    float vLo = capOcvVolt[i + 1] * vScale;   // lower SoC, lower V
    if (restedV <= vHi && restedV >= vLo) {
      float frac = (vHi > vLo) ? (restedV - vLo) / (vHi - vLo) : 0.0f;
      return capOcvSocPct[i + 1] + frac * (capOcvSocPct[i] - capOcvSocPct[i + 1]);
    }
  }
  return 0.0f;
}

static void capAppendPoint(uint32_t epoch, float capAh, float socLow, float tempC, uint8_t conf) {
  if (!bhCapRing) return;
  if (capRefMode == 1 && bhBaselineCapacityAh <= 0.0f) bhBaselineCapacityAh = capAh;  // first = 100%
  float refAh = (capRefMode == 1) ? bhBaselineCapacityAh : (float)BatteryCapacity_Ah;
  BattCapPoint p = {};
  p.epoch = epoch; p.capacityAh = capAh;
  p.capPct = (refAh > 0.0f) ? (capAh / refAh * 100.0f) : 100.0f;
  p.socLow = socLow; p.tempC = tempC; p.conf = conf;
  bhCapRing[bhCapHead] = p;
  bhCapHead = (bhCapHead + 1) % bhCapCap;
  if (bhCapCount < bhCapCap) bhCapCount++;
  capLastPct = p.capPct;
  capLastUpdateEpoch = epoch;
  bhCapDirty = true;   // field-off persist
  queueConsoleMessageF("BATT CAP: %.1f Ah = %.0f%% (from rested SoC %.0f%%, %s)",
                       capAh, p.capPct, socLow, conf ? "high conf" : "LOW conf");
}

// Per battery-update tick. Rest detection (dV/dt-settled + time floor) on the FILTERED voltage,
// low-OCV anchor capture, and an independent unclamped Ah bridge from the anchor.
void capTrackTick(float I_batt, float V_filt, float tempC, float dtSec) {
  if (!bhCapRing || dtSec <= 0.0f) return;
  uint32_t nowMs = millis();
  float restThresh = capRestCurrentFrac * (float)BatteryCapacity_Ah;
  if (restThresh < 0.2f) restThresh = 0.2f;

  // dV/dt of the FILTERED voltage over a ≥2-min window → mV per 10 min (short windows would be all noise)
  if (isnan(capPrevVForRate)) { capPrevVForRate = V_filt; capPrevVRateMs = nowMs; }
  else {
    float dtMin = (nowMs - capPrevVRateMs) / 60000.0f;
    if (dtMin >= 2.0f) {
      capLastDvdtMv10 = fabsf(V_filt - capPrevVForRate) * 1000.0f / dtMin * 10.0f;
      capPrevVForRate = V_filt; capPrevVRateMs = nowMs;
    }
  }

  if (fabsf(I_batt) < restThresh) capRestTimerMs += (uint32_t)(dtSec * 1000.0f);
  else                            capRestTimerMs = 0;

  bool rested = (capRestTimerMs >= (uint32_t)capRestFloorMin * 60000UL) &&
                (capLastDvdtMv10 < capSettleRateMv10);
  if (rested) {
    float soc = ocvToSoC(V_filt);
    if (soc <= capSocLowMax && (!capLowAnchorValid || soc < capLowAnchorSoC)) {  // bottom knee, keep the lowest
      capLowAnchorValid = true;
      capLowAnchorSoC   = soc;
      capLowAnchorTempC = tempC;
      capBridgeAh = 0.0f; capBridgeMinAh = 0.0f;
    }
  }

  if (capLowAnchorValid) {
    float dAh = I_batt * dtSec / 3600.0f;
    if (dAh > 0.0f) dAh *= (ChargeEfficiency_scaled / 1000.0f);   // single source of truth = Battery Monitor's ChargeEfficiency_scaled (%×10)
    capBridgeAh += dAh;
    if (capBridgeAh < capBridgeMinAh) capBridgeMinAh = capBridgeAh;
    if (capBridgeMinAh < -0.05f * (float)BatteryCapacity_Ah) capLowAnchorValid = false;  // re-discharged → stale
  }
}

// At the full-charge anchor. Emits a dated capacity point if the span is deep enough.
void capTrackOnFull(uint32_t epoch, float tempC) {
  if (!capLowAnchorValid) return;                 // no independent low anchor → can't measure
  float span = capFullSoc - capLowAnchorSoC;
  if (span < capMinSpan || capBridgeAh <= 0.0f) { capLowAnchorValid = false; return; }

  float measuredAh = capBridgeAh / (span / 100.0f);   // no coulomb-derived SoC → actually shows fade
  if (capTempNormEnable && !isnan(tempC) && !isnan(capLowAnchorTempC)) {
    float tAvg = 0.5f * (tempC + capLowAnchorTempC);
    measuredAh *= (1.0f + (capTempCoeffPctC / 100.0f) * (capTempRefC - tAvg));
  }
  if (measuredAh < 0.3f * BatteryCapacity_Ah || measuredAh > 1.5f * BatteryCapacity_Ah) {
    capLowAnchorValid = false; return;            // implausible vs rated → discard
  }
  uint8_t conf = (capLastDvdtMv10 < 0.5f * capSettleRateMv10 && span >= capMinSpan + 10.0f) ? 1 : 0;
  capAppendPoint(epoch, measuredAh, capLowAnchorSoC, isnan(tempC) ? 0.0f : tempC, conf);
  capLowAnchorValid = false;                       // consume the anchor
}

// Persist DCIR results + capacity ring to NVS. Caller gates on field-off.
void bhFlushCapNVS() {
  if (dbgRingsSynthetic) return;   // fillmax/clearmax: RAM ring is synthetic/empty — keep the real NVS blob
  if (bhCapDirty) {
    settingWrite(NK_bhCapBlob, bhSerializeCap().c_str());
    settingWrite(NK_bhBaseline, String(bhBaselineCapacityAh, 3).c_str());
    bhCapDirty = false;
  }
  if (bhResultsDirty) {
    settingWrite(NK_bhResults, bhSerializeResults().c_str());
    bhResultsDirty = false;
  }
}

// ── Serialize / deserialize (compact CSV; records joined by ';') ────────────────
String bhSerializeResults() {
  String out; out.reserve(bhResultCount * 56);
  int start = (bhResultCount < bhResultCap) ? 0 : bhResultHead;
  for (int n = 0; n < bhResultCount; n++) {
    BattHealthResult &r = bhResults[(start + n) % bhResultCap];
    if (n) out += ';';
    out += String(r.epoch);            out += ',';
    out += String(r.dcir_mOhm, 2);     out += ',';
    out += (isnan(r.soh_pct) ? String("nan") : String(r.soh_pct, 1)); out += ',';
    out += String(r.boardTempF, 1);    out += ',';
    out += String(r.soc_pct, 1);       out += ',';
    out += String(r.battV, 2);         out += ',';
    out += String(r.stepLowA, 1);      out += ',';
    out += String(r.stepDeltaA, 1);    out += ',';
    out += String((int)r.edgesUsed);   out += ',';
    out += String((unsigned)r.dwellMsUsed); out += ',';
    out += String(r.fitSpread_mOhm, 3);
  }
  return out;
}

static float bhTok(const String &s, int &pos) {   // next comma/semicolon-delimited float
  int e = pos;
  while (e < (int)s.length() && s[e] != ',' && s[e] != ';') e++;
  String t = s.substring(pos, e);
  pos = e + 1;
  if (t == "nan") return NAN;
  return t.toFloat();
}

void bhDeserializeResults(const String &blob) {
  bhResultCount = 0; bhResultHead = 0;
  if (blob.length() == 0) return;
  int pos = 0;
  while (pos < (int)blob.length()) {
    BattHealthResult r = {};
    r.epoch      = (uint32_t)bhTok(blob, pos);
    r.dcir_mOhm  = bhTok(blob, pos);
    r.soh_pct    = bhTok(blob, pos);
    r.boardTempF = bhTok(blob, pos);
    r.soc_pct    = bhTok(blob, pos);
    r.battV      = bhTok(blob, pos);
    r.stepLowA   = bhTok(blob, pos);
    r.stepDeltaA = bhTok(blob, pos);
    r.edgesUsed  = (uint8_t)bhTok(blob, pos);
    r.dwellMsUsed    = (uint16_t)bhTok(blob, pos);   // 0 for pre-upgrade rows (clear history once after flashing)
    r.fitSpread_mOhm = bhTok(blob, pos);
    BH_APPEND_RESULT(r);
  }
}

String bhSerializeCap() {
  String out; out.reserve(bhCapCount * 32);
  int start = (bhCapCount < bhCapCap) ? 0 : bhCapHead;
  for (int n = 0; n < bhCapCount; n++) {
    BattCapPoint &p = bhCapRing[(start + n) % bhCapCap];
    if (n) out += ';';
    out += String(p.epoch);         out += ',';
    out += String(p.capacityAh, 1); out += ',';
    out += String(p.capPct, 1);     out += ',';
    out += String(p.socLow, 1);     out += ',';
    out += String(p.tempC, 1);      out += ',';
    out += String((int)p.conf);
  }
  return out;
}

void bhDeserializeCap(const String &blob) {
  bhCapCount = 0; bhCapHead = 0;
  if (blob.length() == 0) return;
  int pos = 0;
  while (pos < (int)blob.length()) {
    BattCapPoint p = {};
    p.epoch      = (uint32_t)bhTok(blob, pos);
    p.capacityAh = bhTok(blob, pos);
    p.capPct     = bhTok(blob, pos);
    p.socLow     = bhTok(blob, pos);
    p.tempC      = bhTok(blob, pos);
    p.conf       = (uint8_t)bhTok(blob, pos);
    bhCapRing[bhCapHead] = p;
    bhCapHead = (bhCapHead + 1) % bhCapCap;
    if (bhCapCount < bhCapCap) bhCapCount++;
  }
}

// OCV table blob — CAP_OCV_ROWS rested-voltage values, comma-joined.
String capSerializeOcv() {
  String out;
  for (int i = 0; i < CAP_OCV_ROWS; i++) { if (i) out += ','; out += String(capOcvVolt[i], 3); }
  return out;
}
void capDeserializeOcv(const String &blob) {
  if (blob.length() == 0) return;
  int pos = 0;
  for (int i = 0; i < CAP_OCV_ROWS && pos < (int)blob.length(); i++) capOcvVolt[i] = bhTok(blob, pos);
}

static bool ocvMatchesPreset(const float *p) {
  for (int i = 0; i < CAP_OCV_ROWS; i++) if (fabsf(capOcvVolt[i] - p[i]) > 0.005f) return false;
  return true;
}
// True while capOcvVolt[] still equals one of the chemistry presets — i.e. the user has NOT hand-tuned it.
bool ocvIsAnyPreset() {
  return ocvMatchesPreset(capOcvPresetLifepo4) || ocvMatchesPreset(capOcvPresetAgm) || ocvMatchesPreset(capOcvPresetLead);
}
// Bind the rested-voltage curve to the selected chemistry unless the user hand-tuned it. Runs at
// commissioning (vessel-info save) and at boot, before the SoC seed reads the curve, so voltage→SoC
// matches the bank. Idempotent: no NVS write when the curve is already the right preset.
void applyChemistryOcvPreset() {
  if (!ocvIsAnyPreset()) return;  // custom curve — never clobber
  String bt = BATTERY_TYPE; bt.toLowerCase();
  const float *p = nullptr;
  if (bt.indexOf("lifepo") >= 0 || bt.indexOf("lithium") >= 0 || bt.indexOf("li-ion") >= 0 ||
      bt.indexOf("liion") >= 0 || bt.indexOf("lfp") >= 0)  p = capOcvPresetLifepo4;
  else if (bt.indexOf("agm") >= 0)                         p = capOcvPresetAgm;
  else if (bt.indexOf("lead") >= 0)                        p = capOcvPresetLead;
  if (!p || ocvMatchesPreset(p)) return;  // "other"/unknown chemistry, or already the right curve
  for (int i = 0; i < CAP_OCV_ROWS; i++) capOcvVolt[i] = p[i];
  settingWrite(NK_capOcvBlob, capSerializeOcv().c_str());
}

// Call after InitSystemSettings() — needs the NVS settings layer up.
void bhInitSettings() {
  if (!settingExists(NK_bhStepLowA))   settingWrite(NK_bhStepLowA, String(bhStepLowA, 1).c_str());   else bhStepLowA   = settingRead(NK_bhStepLowA).toFloat();
  if (!settingExists(NK_bhStepDeltaA)) settingWrite(NK_bhStepDeltaA, String(bhStepDeltaA, 1).c_str()); else bhStepDeltaA = settingRead(NK_bhStepDeltaA).toFloat();
  if (!settingExists(NK_bhDwellMs))    settingWrite(NK_bhDwellMs, String(bhDwellMs).c_str());         else bhDwellMs    = (uint32_t)settingRead(NK_bhDwellMs).toInt();
  if (!settingExists(NK_bhNumEdges))   settingWrite(NK_bhNumEdges, String(bhNumEdges).c_str());       else bhNumEdges   = (uint8_t)settingRead(NK_bhNumEdges).toInt();
  if (settingExists(NK_bhBaseline))    bhBaselineCapacityAh = settingRead(NK_bhBaseline).toFloat();
  if (bhNumEdges < 3) bhNumEdges = 3;
  if (bhNumEdges > BH_MAX_TOGGLES - 3) bhNumEdges = BH_MAX_TOGGLES - 3;
  if (bhStepDeltaA < 5.0f) bhStepDeltaA = 5.0f;   // ΔV must clear ripple/noise; also guards the ΔI validity gate
  if (bhDwellMs < bhMinDwellMs()) bhDwellMs = bhMinDwellMs();
  if (settingExists(NK_bhResults)) bhDeserializeResults(settingRead(NK_bhResults));
  if (settingExists(NK_bhCapBlob)) bhDeserializeCap(settingRead(NK_bhCapBlob));

  // Capacity tracker config (Pattern B) + the editable OCV table
  if (!settingExists(NK_capRestFrac))   settingWrite(NK_capRestFrac, String(capRestCurrentFrac, 4).c_str());   else capRestCurrentFrac = settingRead(NK_capRestFrac).toFloat();
  if (!settingExists(NK_capRestFloor))  settingWrite(NK_capRestFloor, String(capRestFloorMin).c_str());        else capRestFloorMin   = (uint16_t)settingRead(NK_capRestFloor).toInt();
  // capSettleRate is volt-domain (mV/10min): first creation scales the 12V default ×(V/12), same rule as InitSystemSettings seeds
  if (!settingExists(NK_capSettleRate)) { capSettleRateMv10 *= (float)BATTERY_VOLTAGE / 12.0f; settingWrite(NK_capSettleRate, String(capSettleRateMv10, 2).c_str()); }  else capSettleRateMv10 = settingRead(NK_capSettleRate).toFloat();
  if (!settingExists(NK_capSocLowMax))  settingWrite(NK_capSocLowMax, String(capSocLowMax, 1).c_str());        else capSocLowMax      = settingRead(NK_capSocLowMax).toFloat();
  if (!settingExists(NK_capMinSpan))    settingWrite(NK_capMinSpan, String(capMinSpan, 1).c_str());            else capMinSpan        = settingRead(NK_capMinSpan).toFloat();
  if (!settingExists(NK_capFullSoc))    settingWrite(NK_capFullSoc, String(capFullSoc, 1).c_str());            else capFullSoc        = settingRead(NK_capFullSoc).toFloat();
  if (!settingExists(NK_capRefMode))    settingWrite(NK_capRefMode, String(capRefMode).c_str());               else capRefMode        = (uint8_t)settingRead(NK_capRefMode).toInt();
  if (!settingExists(NK_capTempNorm))   settingWrite(NK_capTempNorm, String(capTempNormEnable).c_str());       else capTempNormEnable = (uint8_t)settingRead(NK_capTempNorm).toInt();
  if (!settingExists(NK_capTempCoeff))  settingWrite(NK_capTempCoeff, String(capTempCoeffPctC, 3).c_str());    else capTempCoeffPctC  = settingRead(NK_capTempCoeff).toFloat();
  if (!settingExists(NK_capTempRef))    settingWrite(NK_capTempRef, String(capTempRefC, 1).c_str());           else capTempRefC       = settingRead(NK_capTempRef).toFloat();
  if (settingExists(NK_capOcvBlob))     capDeserializeOcv(settingRead(NK_capOcvBlob));
}

String bhBuildStatusJson() {
  String j = "{";
  j += "\"state\":" + String(bhTestState);
  j += ",\"reason\":\"" + String(bhAbortReason) + "\"";
  j += ",\"lastDcir\":" + String(bhLastResultDcir, 2);
  j += ",\"baselineAh\":" + String(bhBaselineCapacityAh, 1);
  j += ",\"low\":" + String(bhStepLowA, 1);
  j += ",\"delta\":" + String(bhStepDeltaA, 1);
  j += ",\"dwellMs\":" + String(bhDwellMs);
  j += ",\"edges\":" + String(bhNumEdges);
  // Wizard countdown: total run = (2 ring-in + scored + 1 trailing) toggles × dwell; elapsed is
  // live only while running (0 otherwise) so a reopened modal recomputes remaining without a
  // client-side start-time it would lose on close. Mirrors the (bhNumEdges + 3) bound in bhStep.
  j += ",\"totalMs\":" + String((uint32_t)(bhNumEdges + 3) * bhDwellMs);
  j += ",\"elapsedMs\":" + String(bhTestState == 1 ? (millis() - bhTestStartMs) : 0);
  j += ",\"results\":[";
  int start = (bhResultCount < bhResultCap) ? 0 : bhResultHead;
  for (int n = 0; n < bhResultCount; n++) {
    BattHealthResult &r = bhResults[(start + n) % bhResultCap];
    if (n) j += ',';
    j += "{\"epoch\":" + String(r.epoch);
    j += ",\"dcir\":" + String(r.dcir_mOhm, 2);
    j += ",\"soh\":" + (isnan(r.soh_pct) ? String("null") : String(r.soh_pct, 1));
    j += ",\"tF\":" + String(r.boardTempF, 1);
    j += ",\"soc\":" + String(r.soc_pct, 1);
    j += ",\"v\":" + String(r.battV, 2);
    j += ",\"low\":" + String(r.stepLowA, 1);
    j += ",\"delta\":" + String(r.stepDeltaA, 1);
    j += ",\"edges\":" + String((int)r.edgesUsed);
    j += ",\"dwell\":" + String((unsigned)r.dwellMsUsed);
    j += ",\"spread\":" + String(r.fitSpread_mOhm, 3) + "}";
  }
  j += "]";
  // Capacity: live status + config echoes + editable OCV table + the dated points
  j += ",\"capLastPct\":" + (isnan(capLastPct) ? String("null") : String(capLastPct, 1));
  j += ",\"capLastUpd\":" + String(capLastUpdateEpoch);
  j += ",\"capAnchored\":" + String(capLowAnchorValid ? 1 : 0);
  j += ",\"capAnchorSoC\":" + String(capLowAnchorSoC, 1);
  j += ",\"capRestMin\":" + String(capRestTimerMs / 60000);   // current rest accumulation (min)
  j += ",\"capDvdt\":" + String(capLastDvdtMv10, 1);
  j += ",\"capCfg\":{";
  j += "\"restFrac\":" + String(capRestCurrentFrac, 4);
  j += ",\"restFloor\":" + String(capRestFloorMin);
  j += ",\"settleRate\":" + String(capSettleRateMv10, 2);
  j += ",\"socLowMax\":" + String(capSocLowMax, 1);
  j += ",\"minSpan\":" + String(capMinSpan, 1);
  j += ",\"chgEff\":" + String(ChargeEfficiency_scaled / 1000.0f, 4);   // read-only mirror of Battery Monitor tab
  j += ",\"capacityAh\":" + String(BatteryCapacity_Ah);                 // read-only mirror of Battery Monitor tab (drives % vs rated)
  j += ",\"fullSoc\":" + String(capFullSoc, 1);
  j += ",\"refMode\":" + String(capRefMode);
  j += ",\"tempNorm\":" + String(capTempNormEnable);
  j += ",\"tempCoeff\":" + String(capTempCoeffPctC, 3);
  j += ",\"tempRef\":" + String(capTempRefC, 1) + "}";
  j += ",\"ocv\":[";
  for (int i = 0; i < CAP_OCV_ROWS; i++) { if (i) j += ','; j += String(capOcvVolt[i], 2); }
  j += "],\"ocvSoc\":[";
  for (int i = 0; i < CAP_OCV_ROWS; i++) { if (i) j += ','; j += String((int)capOcvSocPct[i]); }
  j += "],\"cap\":[";
  int cstart = (bhCapCount < bhCapCap) ? 0 : bhCapHead;
  for (int n = 0; n < bhCapCount; n++) {
    BattCapPoint &p = bhCapRing[(cstart + n) % bhCapCap];
    if (n) j += ',';
    j += "{\"epoch\":" + String(p.epoch);
    j += ",\"ah\":" + String(p.capacityAh, 1);
    j += ",\"pct\":" + String(p.capPct, 1);
    j += ",\"socLow\":" + String(p.socLow, 1);
    j += ",\"conf\":" + String((int)p.conf) + "}";
  }
  j += "]}";
  return j;
}

// ───────────────────────────────────────────────────────────────────────────
// /debug/fillmax + /debug/clearmax — bench-only "age the unit to its ceiling": every accumulating
// store is a fixed-cap ring, so a decade-old unit == rings at cap; this forces that state now and
// times the O(count) worst cases on real hardware. Points are DISPERSED (not clones) so the IDW/LWLR
// front scans do real work; the zero-drift log's temp span is kept < ZFIT_MIN_SPAN_F ON PURPOSE so
// zeroFitRegress can't early-exit (worst case — and a synthetic fit can never be accepted). Default is
// RAM-only: dbgRingsSynthetic freezes every field-off persister until reboot, so flash keeps the real
// learned blobs and a plain reboot restores them. &persist=yes skips the freeze and runs+times the five
// real savers inline (end-to-end flash/NVS test) — synthetic data then survives reboot AND reflash
// (data partitions are not touched by a reflash); recover with Erase All Flash / factory reset. Lives
// at the sketch tail so every static ring/front symbol (2/3/7_functions) is in scope; registered in
// setupServer().
// ───────────────────────────────────────────────────────────────────────────
static uint32_t dbgLcg = 0x1234567u;               // deterministic LCG → identical fill every run
static inline float dbgRand(float lo, float hi) {
  dbgLcg = dbgLcg * 1664525u + 1013904223u;
  return lo + (hi - lo) * ((float)(dbgLcg >> 8) / 16777216.0f);
}

void debugFillMax(AsyncWebServerRequest *request) {
  if (!request->hasParam("confirm") || request->getParam("confirm")->value() != "yes") {
    request->send(200, "text/plain",
      "REFUSED. /debug/fillmax fills EVERY ring to cap with synthetic data and\n"
      "OVERWRITES the learned alt-health + boat-perf surfaces in RAM (not flash — a\n"
      "reboot restores them; persistence is frozen until then). Bench units only.\n"
      "Re-send as /debug/fillmax?confirm=yes to proceed.\n"
      "Add &persist=yes to ALSO run+time the real field-off savers (writes the synthetic\n"
      "data to LittleFS/NVS; survives reboot AND reflash — recover with Erase All Flash).\n");
    return;
  }
  bool persist = request->hasParam("persist") && request->getParam("persist")->value() == "yes";
  dbgRingsSynthetic = !persist;                     // default: freeze ALL field-off persistence until reboot (flash stays real)
  dbgLcg = 0x1234567u;                              // reseed
  String out;

  // 4-D alternator best-ever front → cap, dispersed across the real operating box (axisScale set at init)
  if (altFrontBuf) {
    for (int i = 0; i < ALT_FRONT_CAP; i++) {
      FrontPoint<ALT_NAXIS> &p = altFrontBuf[i];
      p.x[0] = dbgRand(800.0f, 6000.0f);            // RPM
      p.x[1] = dbgRand(0.0f, 3.0f);                 // excitation
      p.x[2] = dbgRand(12.0f, 15.0f);               // Vbus
      p.x[3] = dbgRand(40.0f, 250.0f);              // tempF
      p.ex[0] = 0; p.ex[1] = 0; p.y = dbgRand(20.0f, 150.0f); p.nSamp = 120; p.tEmit = (uint32_t)i;
    }
    altFront2.count = ALT_FRONT_CAP; altFront2.source = 0;
  }

  // 3-D boat-perf fronts (sail + motor) → cap (blob + scan cost; not timed here)
  if (sailFrontBuf) {
    for (int i = 0; i < PERF_FRONT_CAP; i++) {
      FrontPoint<PERF_NAXIS> &p = sailFrontBuf[i];
      for (int a = 0; a < PERF_NAXIS; a++) p.x[a] = dbgRand(0.0f, 60.0f);
      p.ex[0] = 0; p.ex[1] = 0; p.y = dbgRand(2.0f, 12.0f); p.nSamp = 60; p.tEmit = (uint32_t)i;
    }
    sailFront.count = PERF_FRONT_CAP; sailFront.source = 0;
  }
  if (motorFrontBuf) {
    for (int i = 0; i < PERF_FRONT_CAP; i++) {
      FrontPoint<PERF_NAXIS> &p = motorFrontBuf[i];
      for (int a = 0; a < PERF_NAXIS; a++) p.x[a] = dbgRand(0.0f, 60.0f);
      p.ex[0] = 0; p.ex[1] = 0; p.y = dbgRand(2.0f, 12.0f); p.nSamp = 60; p.tEmit = (uint32_t)i;
    }
    motorFront.count = PERF_FRONT_CAP; motorFront.source = 0;
  }

  // Alt-health engine-hour trend → cap (drives the /alttrend.csv scan + ~52 KB /alttrend.bin)
  if (altTrend) {
    for (int i = 0; i < ALT_TREND_CAP; i++) {
      altTrend[i].engHour    = (uint16_t)i;
      altTrend[i].worstPct   = (int16_t)lroundf(dbgRand(600.0f, 950.0f));    // %×10
      altTrend[i].overallPct = (int16_t)lroundf(dbgRand(800.0f, 1000.0f));
    }
    altTrendCount = ALT_TREND_CAP; altTrendFlushed = 0; altTrendRewrite = true;  // next field-off save rewrites whole log
  }

  // 30-day long-term ring → cap
  if (longTermRing) {
    uint32_t base = (uint32_t)time(NULL); if (base < 1700000000u) base = 1700000000u;
    for (int i = 0; i < LONGTERM_RING_SIZE; i++) {
      LongTermRecord &r = longTermRing[i];
      r.timestamp = base - (uint32_t)(LONGTERM_RING_SIZE - i) * 600u;         // 10-min cadence
      r.validMask = 0xFFFFFFFFu; r.chargeStage = 1;
    }
    longTermCount = LONGTERM_RING_SIZE; longTermHead = 0;
    // Without this, dumpLongTermRing early-outs (delta == 0 && FileRecords > 0) and &persist=yes silently
    // writes nothing — the timing report then shows ~0 ms for a saver that never ran.
    longTermPushSeq = longTermCount; longTermFlushedSeq = 0; longTermFileRecords = 0;
  }

  // Zero-drift log → cap, temp span < ZFIT_MIN_SPAN_F so zeroFitRegress can't early-exit (true worst case)
  if (zeroLogRing) {
    for (int i = 0; i < ZEROLOG_RING_SIZE; i++) {
      ZeroLogRecord &r = zeroLogRing[i];
      r.epoch = (uint32_t)i; r.amps = dbgRand(-0.5f, 0.5f); r.p2pAmps = 0.1f; r.rpm = 0; r.battVx100 = 1280;
      r.altTempFx10   = (int16_t)lroundf(dbgRand(700.0f, 720.0f));            // ~2 °F span → never satisfies the 30 °F gate
      r.boardTempFx10 = (int16_t)lroundf(dbgRand(700.0f, 720.0f));
    }
    zeroLogCount = ZEROLOG_RING_SIZE; zeroLogHead = 0;
    zeroLogPushSeq = zeroLogCount; zeroLogFlushedSeq = 0; zeroLogFileRecords = 0;  // same early-out as above
  }

  // Battery-capacity ring → cap. (The sensor upload ring is deliberately NOT faked: bumping its
  // count would queue SENSOR_RING_SIZE junk rows to the cloud, and it feeds the Core-0 uploader, not
  // any O(count) control-path scan — so it tests nothing the growth audit cares about.)
  if (bhCapRing) {
    for (int i = 0; i < bhCapCap; i++) {
      BattCapPoint &p = bhCapRing[i];
      p.epoch = 1700000000u + (uint32_t)i * 86400u; p.capacityAh = dbgRand(180.0f, 210.0f);
      p.capPct = dbgRand(85.0f, 100.0f); p.socLow = dbgRand(8.0f, 15.0f); p.tempC = 25.0f; p.conf = 1;
    }
    bhCapCount = bhCapCap; bhCapHead = 0; bhCapDirty = true;   // dirty so a persist-mode bhFlushCapNVS actually writes
  }

  // Measure the two worst-case data-scaling scans on THIS board (not estimates):
  // 1) front classify() at cap = the real 1 Hz alt-health cost (IDW eval + LWLR select + 48-pt solve)
  float pred = 0; volatile long sink = 0;
  float surf[ALT_NAXIS] = { 3000.0f, 1.5f, 13.5f, 150.0f };
  uint32_t t0 = micros();
  for (int r = 0; r < 20; r++) sink += altFront2.classify(surf, altRefRadius, altIdwPower, altRidgeFrac, altRiskThresh, &pred);
  uint32_t classifyUs = (micros() - t0) / 20;
  // 2) trend drop-oldest memmove at cap = the "once per decade" event on real PSRAM
  uint32_t t1 = micros();
  memmove(altTrend, altTrend + 1, (size_t)(ALT_TREND_CAP - 1) * sizeof(AltTrendPt));
  uint32_t memmoveUs = micros() - t1;
  altTrendCount = ALT_TREND_CAP;                    // memmove left a dup in the last slot; keep count at cap

  out  = "FILLMAX done — every ring at cap (bench worst-case)\n";
  out += "altFront2="   + String(altFront2.count) + "/" + String(ALT_FRONT_CAP);
  out += "  sail/motor=" + String(sailFront.count) + "/" + String(motorFront.count) + "\n";
  out += "altTrend="    + String(altTrendCount) + "/" + String(ALT_TREND_CAP);
  out += "  longTerm="  + String((unsigned)longTermCount) + "/" + String((unsigned)LONGTERM_RING_SIZE) + "\n";
  out += "zeroLog="     + String((unsigned)zeroLogCount) + "/" + String((unsigned)ZEROLOG_RING_SIZE);
  out += "  bhCap="     + String(bhCapCount) + "/" + String(bhCapCap) + "\n";
  out += "---- measured ceilings ----\n";
  out += "classify() @ " + String(ALT_FRONT_CAP) + " pts (1 Hz alt-health tick): " + String(classifyUs) + " us\n";
  out += "trend memmove @ cap (once-per-decade drop-oldest): " + String(memmoveUs) + " us\n";
  out += "free heap=" + String(ESP.getFreeHeap()) + " maxBlock=" + String(ESP.getMaxAllocHeap());
  out += " freePSRAM=" + String(ESP.getFreePsram()) + "\n";
  out += "sink=" + String(sink) + "\n\n";
  if (persist) {
    struct { const char *name; void (*fn)(); } savers[] = {
      { "altHealthSave (fronts + trend)", altHealthSave },
      { "boatPerfSave (sail + motor fronts)", boatPerfSave },
      { "dumpLongTermRing", dumpLongTermRing },
      { "dumpZeroLog", dumpZeroLog },
      { "bhFlushCapNVS (capacity blob, NVS)", bhFlushCapNVS },
    };
    out += "---- persist=yes: real field-off savers run inline, timed ----\n";
    for (auto &s : savers) {
      uint32_t w0 = millis();
      s.fn();
      out += String(s.name) + ": " + String(millis() - w0) + " ms\n";
    }
    out += "Flash/NVS now hold SYNTHETIC data (survives reboot AND reflash).\n";
    out += "Reboot to test the load-at-cap boot path; recover with Erase All Flash / factory reset.\n";
  } else {
    out += "Persistence is FROZEN until reboot (flash untouched) — reboot restores real data.\n";
  }
  out += "LittleFS used/total=" + String((unsigned)LittleFS.usedBytes()) + "/" + String((unsigned)LittleFS.totalBytes()) + "\n";
  out += "Next: watch ft_altFold / ft_altHealth / ft_dumpLongTermRing in /debug for worst-pass loop time.\n";
  request->send(200, "text/plain", out);
}

// Zero the in-RAM rings after a fillmax. Persistence stays FROZEN — empty is as un-real as synthetic;
// reboot restores the real data. sensorRing untouched (never faked; zeroing it would drop real pending uploads).
void debugClearMax(AsyncWebServerRequest *request) {
  dbgRingsSynthetic = true;                         // RAM empty ≠ real → keep every field-off persister frozen until reboot
  if (altFront2.source == 0) altFront2.count = 0;   // only clear a LEARNED surface, never an uploaded one
  sailFront.count = 0; motorFront.count = 0;
  altTrendCount = 0; altTrendFlushed = 0; altTrendRewrite = true;
  longTermCount = 0; longTermHead = 0;
  zeroLogCount = 0; zeroLogHead = 0;
  bhCapCount = 0; bhCapHead = 0;
  request->send(200, "text/plain",
    "CLEARMAX done — in-RAM rings zeroed; persistence frozen. REBOOT to reload from flash\n"
    "(real data — unless a fillmax?persist=yes wrote synthetic there; then Erase All Flash).\n");
}
