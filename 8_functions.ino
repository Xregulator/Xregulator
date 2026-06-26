// ── 8_functions.ino ── Fast alternator-current failure detector (consumer 2): algorithm bodies.

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
// float-partial->double accumulation (FAD_SEG_SAMPLES) so results stay bit-identical to the old
// sliced version; do not flatten them. Rationale + validation: Fault_Detector_Dev_Summary.md.

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
  if (J->src) for (int i = 0; i < n; i++) J->xf[i] = (float)J->src[i];  // int16 source -> float
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
// "settings" keys are shareable. tier 1 = free clone; tier 2 = install/hardware
// topology (sensor/shunt/polarity) — exported but applied only with includeHardware.
// Per-device calibration, identity/secrets, UI prefs, and momentary actions are
// NEVER in the manifest (see config_drift_check.py — it fails the build if any
// settingWrite key is in neither the manifest nor its EXCLUDE list).
//
// Values are carried as the RAW NVS strings (settingRead output), so import is a
// straight settingWrite of byte-identical internal state followed by a reboot —
// InitSystemSettings then re-reads the whole set consistently. No per-key unit
// conversion, no /get replay. Cross-rev safe: a key absent on the destination is
// skipped and that firmware's own default applies (see CLAUDE.md "NVS Cross-Rev Risk").
// ============================================================================
// AUTO-CLASSIFIED config manifest (allowlist). tier 1 = free clone, 2 = install/hardware.
// Generated from NK_ macros + tier classification; see config_drift_check.py guard.
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
  { "RPMScalingFactor", NK_RPMScalingFactor, 1 },
  { "SwitchingFrequency", NK_SwitchingFrequency, 1 },
  { "PidKp", NK_PidKp, 1 },
  { "PidKi", NK_PidKi, 1 },
  { "PidKd", NK_PidKd, 1 },
  { "PidSampleDivisor", NK_PidSampleDivisor, 1 },
  { "PIDTrackingGain", NK_PIDTrackingGain, 1 },
  { "OutputPIDFilterTC", NK_OutputPIDFilterTC, 1 },
  { "OutputPIDMA_N", NK_OutputPIDMA_N, 1 },
  { "OutputPIDSigSrc", NK_OutputPIDSigSrc, 1 },
  { "InputFilterTC", NK_InputFilterTC, 1 },
  { "VoltageFilterTC", NK_VoltageFilterTC, 1 },
  { "DvdtTC", NK_DvdtTC, 1 },
  { "VoltageKp", NK_VoltageKp, 1 },
  { "VoltageKi", NK_VoltageKi, 1 },
  { "VoltageLoopInterval", NK_VoltageLoopInterval, 1 },
  { "AwBleedRate", NK_AwBleedRate, 1 },
  { "AwSeedProtectMs", NK_AwSeedProtectMs, 1 },
  { "SetpointRiseRate", NK_SetpointRiseRate, 1 },
  { "SetpointFallRate", NK_SetpointFallRate, 1 },
  { "SetpointBigStepThresh", NK_SetpointBigStepThresh, 1 },
  { "SetpointBigStepRiseRate", NK_SetpointBigStepRiseRate, 1 },
  { "FastSetpointRiseRate", NK_FastSetpointRiseRate, 1 },
  { "FastSetpointRiseHeadroomV", NK_FastSetpointRiseHeadroomV, 1 },
  { "FastSetpointRiseWindowMs", NK_FastSetpointRiseWindowMs, 1 },
  { "cvHelpersEnabled", NK_cvHelpersEnabled, 1 },
  { "cvGainMode", NK_cvGainMode, 1 },
  { "cvOmega", NK_cvOmega, 1 },
  { "cvKiRatio", NK_cvKiRatio, 1 },
  { "vTgtRampEnable", NK_vTgtRampEnable, 1 },
  { "vTgtRampUp", NK_vTgtRampUp, 1 },
  { "vTgtRampDn", NK_vTgtRampDn, 1 },
  { "coldChargeLockoutEnable", NK_coldChargeLockoutEnable, 1 },
  { "MinChargeTempF", NK_MinChargeTempF, 1 },
  { "battTempDerateEnable", NK_battTempDerateEn, 1 },
  { "battTempCoeff", NK_battTempCoeff, 1 },
  { "SlopeBleedK", NK_SlopeBleedK, 1 },
  { "SlopeBleedProxV", NK_SlopeBleedProxV, 1 },
  { "SlopeBleedThresh", NK_SlopeBleedThresh, 1 },
  { "TdPred", NK_TdPred, 1 },
  { "KHard", NK_KHard, 1 },
  { "OvGroup1Enable", NK_OvGroup1Enable, 1 },
  { "OvGroup2Enable", NK_OvGroup2Enable, 1 },
  { "OvMeasMarginV", NK_OvMeasMarginV, 1 },
  { "OvPredMarginV", NK_OvPredMarginV, 1 },
  { "HardOCDebounceMs", NK_HardOCDebounceMs, 1 },
  { "SettleTimeBeforeCut", NK_SettleTimeBeforeCut, 1 },
  { "ShutdownPhase2HoldMs", NK_ShutdownPhase2HoldMs, 1 },
  { "ReseedFrac", NK_ReseedFrac, 1 },
  { "IExcessArmMarginV", NK_IExcessArmMarginV, 1 },
  { "IExcessCeilA", NK_IExcessCeilA, 1 },
  { "IExcessFloorA", NK_IExcessFloorA, 1 },
  { "IExcessFrac", NK_IExcessFrac, 1 },
  { "IExcessFracBulk", NK_IExcessFracBulk, 1 },
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
  { "TempSource", NK_TempSource, 1 },
  { "R_fixed", NK_R_fixed, 1 },
  { "Beta", NK_Beta, 1 },
  { "T0_C", NK_T0_C, 1 },
  { "WindingTempOffset", NK_WindingTempOffset, 1 },
  { "displayTempUnit", NK_displayTempUnit, 1 },
  { "AmbientTempCorrectionFactor", NK_AmbientTempCorrectionFactor, 1 },
  { "EnableAmbientCorrection", NK_EnableAmbientCorrection, 1 },
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
  { "BatteryCurrentSource", NK_BatteryCurrentSource, 2 },
  { "ShuntResistanceMicroOhm", NK_ShuntResistanceMicroOhm, 2 },
  { "AmpSensorRange", NK_AmpSensorRange, 2 },
  { "InvertAltAmps", NK_InvertAltAmps, 2 },
  { "InvertBattAmps", NK_InvertBattAmps, 2 },
};
static const size_t CONFIG_MANIFEST_COUNT = sizeof(CONFIG_MANIFEST)/sizeof(CONFIG_MANIFEST[0]);

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

// Emit the manifest settings as a complete JSON object {"param":"rawNvsStr",...}.
// Single source of truth for BOTH /exportConfig (sharing) and the daily fleet config
// snapshot (buildConfigPayload's "settings") — so neither can drift as settings are added.
// includeHardware adds the tier-2 install/topology keys. Keys never set are omitted.
String manifestConfigObject(bool includeHardware) {
  String j;
  j.reserve(6144);
  j = "{";
  bool first = true;
  for (size_t i = 0; i < CONFIG_MANIFEST_COUNT; i++) {
    if (CONFIG_MANIFEST[i].tier == 2 && !includeHardware) continue;
    if (!settingExists(CONFIG_MANIFEST[i].nvsKey)) continue;   // key never set -> omit, destination keeps its default
    String v = settingRead(CONFIG_MANIFEST[i].nvsKey);
    if (!first) j += ',';
    first = false;
    j += '"'; j += CONFIG_MANIFEST[i].param; j += "\":";
    cfgAppendJsonStr(j, v);
  }
  j += "}";
  return j;
}

// Build the shareable config blob: fw_version + vessel metadata (for the cloud
// table-of-contents) + the manifest "config" object. includeHardware adds tier-2 keys.
String exportConfigJson(bool includeHardware) {
  String j;
  j.reserve(8192);
  j = "{\"fw_version\":\"";
  j += FIRMWARE_VERSION;
  j += "\",\"payload_v\":1,\"vessel\":{";
  j += "\"boat_make_model\":";        cfgAppendJsonStr(j, BOAT_MAKE_MODEL);
  j += ",\"engine_make\":";           cfgAppendJsonStr(j, ENGINE_MAKE);
  j += ",\"engine_hp\":";             j += String((unsigned)ENGINE_HP);
  j += ",\"battery_voltage\":";       j += String((unsigned)BATTERY_VOLTAGE);
  j += ",\"battery_capacity_ah\":";   j += String(BatteryCapacity_Ah);
  j += ",\"battery_type\":";          cfgAppendJsonStr(j, BATTERY_TYPE);
  j += ",\"alternator_brand_model\":";cfgAppendJsonStr(j, ALTERNATOR_BRAND_MODEL);
  j += "},\"config\":";
  j += manifestConfigObject(includeHardware);
  j += "}";
  return j;
}

// Extract a flat top-level value for "key" from a JSON object starting at 'from'.
// Quote-delimited needle prevents prefix collisions (e.g. TailCurrent vs TailCurrent_A).
// Values are simple numbers / short identifiers (raw NVS strings) — no nested escaping.
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
    const char *e = p;
    while (*e && *e != '"') e++;
    val = ""; val.concat(p, e - p);
    return true;
  }
  const char *e = p;
  while (*e && *e != ',' && *e != '}' && *e != ' ' && *e != '\n' && *e != '\r' && *e != '\t') e++;
  val = ""; val.concat(p, e - p);
  return true;
}

// Apply an imported config blob. Only manifest (allowlisted) keys are written —
// anything else in the body is ignored by construction. Returns count applied,
// or -1 if the body has no "config" object. settingWrite is compare-first so
// unchanged values cost no flash. Caller reboots so the new set loads cleanly.
int applyImportConfig(const char *body, bool includeHardware) {
  if (!body) return -1;
  const char *cfg = strstr(body, "\"config\"");
  if (!cfg) return -1;   // malformed / wrong shape — reject, don't half-apply
  int applied = 0;
  for (size_t i = 0; i < CONFIG_MANIFEST_COUNT; i++) {
    if (CONFIG_MANIFEST[i].tier == 2 && !includeHardware) continue;
    String val;
    if (cfgJsonExtract(cfg, CONFIG_MANIFEST[i].param, val)) {
      if (settingWrite(CONFIG_MANIFEST[i].nvsKey, val.c_str())) applied++;
    }
  }
  return applied;
}
