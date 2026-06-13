// ── 8_functions.ino ── Fast alternator-current failure detector (consumer 2): algorithm bodies.
// Moved here 2026-06-12 from the former fad_core.h (the project keeps no .h files). The shared
// type definitions (FadJob / FadResult / FADV_* / FADS_*) live at the top of the detector
// section in 2_functions.ino, which precedes this file in the Arduino build; this file holds
// only the tuning constants and the function bodies. FAD_NOW_US() is defined below.
//
// Rectifier/stator pulse-pattern fault detector (consumer 2 of the fast alternator-current
// channel) — faithful C++ port of rect_fault_detector.py (offline prototype, 18/18 on the
// synthetic gate; that module's docstring is the architecture reference). Pipeline: regime
// split from the ACF cycle period → boxcar smooth + rough high-pass → crest picking by
// valley-depth merging (idle: one rung; cruise: 3-rung ladder, best SANE rung wins by
// trigger headroom) → cycle-exact re-detrend + 2-cycle envelope → per-crest height/interval
// features → modulo-k ANOVA effect sizes (k=2..8) per 48-crest block → gap-anchored sync
// mode → ACF period-ratio confirmation → FAULT/TREND/healthy + winning k.
// Constants mirror the prototype's TUNING dict — do NOT retune against the synthetic set;
// real boat captures are the validation set (DETECTOR_DEV_NOTES.md).
// Execution: resumable state machine — fadStep() advances until FAD_NOW_US() passes the
// caller's deadline (the ~1 ms loop-budget contract), so a full analysis spreads over many
// loop() passes. Heavy items (direct-form ACFs, ≤400 lags) run in 8 K-sample segments.
// float32 buffers, double accumulators (prefix sums / dot products) for numpy parity.

#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include "esp_timer.h"
#define FAD_NOW_US() ((int64_t)esp_timer_get_time())

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
#define FAD_SEG_SAMPLES 8192 // inner-loop chunk between deadline checks

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

static int fadCmpFloat(const void *a, const void *b) {
  float x = *(const float *)a, y = *(const float *)b;
  return (x > y) - (x < y);
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
  J->stage = src ? FADS_CONVERT : FADS_S3;
  J->pickStage = FADP_MAXIMA;
  J->pos = 0;
  J->lag = 0;
  J->seg = 0;
  J->acc = 0.0;
  J->rung = 0;
  J->haveBest = 0;
  J->bestSane = 0;
  J->bestHeadroom = 0.0f;
  memset(&J->best, 0, sizeof(J->best));
  J->idxOverflow = 0;
}

// ---- crest picking sub-machine (maxima → local contrast → valley-depth merge) ----
// Operates on the current contents of J->rr. Returns 1 = done, 0 = out of time,
// -1 = pick failed (too few maxima/intervals — the prototype's None). On success:
// J->kept/nKept = merged crest train, J->idx/nIdx = raw maxima, J->pickSane set.
static int fadPickStep(FadJob *J,
                       int64_t deadline, float wMs, float frac) {
  const float *r = J->rr;
  int n = J->n;
  switch (J->pickStage) {
    case FADP_MAXIMA: {
      if (J->pos == 0) { J->nIdx = 0; J->pos = 1; }
      while (J->pos <= n - 2) {
        int endx = J->pos + FAD_SEG_SAMPLES;
        if (endx > n - 1) endx = n - 1;  // exclusive; last valid maximum index is n-2
        for (int i = J->pos; i < endx; i++) {
          if (r[i] > r[i - 1] && r[i] >= r[i + 1]) {
            if (J->nIdx >= J->crestCap) {
              J->idxOverflow = 1;
              J->pos = 0;
              return -1;
            }
            J->idx[J->nIdx++] = i;
          }
        }
        J->pos = endx;
        if (J->pos <= n - 2 && FAD_NOW_US() >= deadline) return 0;
      }
      J->pos = 0;
      if (J->nIdx < 32) return -1;
      J->pickStage = FADP_CONTRAST;
    } /* fall through */
    case FADP_CONTRAST: {
      // per-window pk-pk into scratch; window count n/w (numpy reshape semantics)
      int w = (int)llrintf(wMs * 1e-3f * J->fs);
      if (w < 8) w = 8;
      int nw = n / w;
      while (J->pos < nw) {
        int end = J->pos + 512;
        if (end > nw) end = nw;
        for (int b = J->pos; b < end; b++) {
          const float *seg = r + b * w;
          float mn = seg[0], mx = seg[0];
          for (int q = 1; q < w; q++) {
            if (seg[q] < mn) mn = seg[q];
            if (seg[q] > mx) mx = seg[q];
          }
          J->scratch[b] = mx - mn;
        }
        J->pos = end;
        if (J->pos < nw && FAD_NOW_US() >= deadline) return 0;
      }
      J->pickStage = FADP_CONTRAST_SEL;  // J->pos carries nw
    } /* fall through */
    case FADP_CONTRAST_SEL: {
      float c = fadPercentile(J->scratch, J->pos, 25.0f);
      J->mergeDelta = frac * c;
      J->pos = 1;  // next maxima position (kept[0] = idx[0])
      J->nKept = 1;
      J->kept[0] = J->idx[0];
      J->vmin = r[J->idx[0]];
      J->scanFrom = J->idx[0] + 1;
      J->pickStage = FADP_MERGE;
    } /* fall through */
    case FADP_MERGE: {
      // valley-depth merge; vmin/scanFrom maintain an incremental min over [kept[last], i]
      while (J->pos < J->nIdx) {
        int end = J->pos + 512;
        if (end > J->nIdx) end = J->nIdx;
        for (int p = J->pos; p < end; p++) {
          int i = J->idx[p];
          int j = J->kept[J->nKept - 1];
          for (int q = J->scanFrom; q <= i; q++)
            if (r[q] < J->vmin) J->vmin = r[q];
          J->scanFrom = i + 1;
          float lower = (r[j] < r[i]) ? r[j] : r[i];
          if (J->vmin > lower - J->mergeDelta) {
            if (r[i] > r[j]) {
              J->kept[J->nKept - 1] = i;  // merged: keep the taller
              J->vmin = r[i];
            }
          } else {
            J->kept[J->nKept++] = i;  // real valley: new crest
            J->vmin = r[i];
          }
        }
        J->pos = end;
        if (J->pos < J->nIdx && FAD_NOW_US() >= deadline) return 0;
      }
      J->pickStage = FADP_FINISH;
    } /* fall through */
    case FADP_FINISH: {
      int niv = J->nKept - 1;
      J->pos = 0;
      J->pickStage = FADP_MAXIMA;
      if (niv < 16) return -1;
      for (int q = 0; q < niv; q++)
        J->iv[q] = (float)(J->kept[q + 1] - J->kept[q]);
      memcpy(J->scratch, J->iv, sizeof(float) * (size_t)niv);
      float p90 = fadPercentile(J->scratch, niv, 90.0f);
      int dup = 0;
      for (int q = 0; q < niv; q++)
        if (J->iv[q] < FAD_DUPE_IV_FRAC * p90) dup++;
      J->pickSane = ((float)dup / (float)niv <= FAD_DUPE_FRAC_MAX) ? 1 : 0;
      return 1;
    }
  }
  return -1;
}

// per-rung teardown → next rung or done (macro: the .ino prototype generator must not
// see a single-line function signature naming FadJob, or it emits a bad early prototype)
#define fadNextRung(J) \
  do { \
    (J)->rung++; \
    (J)->pos = 0; \
    (J)->lag = 0; \
    (J)->seg = 0; \
    (J)->acc = 0.0; \
    (J)->pickStage = FADP_MAXIMA; \
    (J)->stage = ((J)->rung < (J)->nRungs) ? FADS_RUNG_PICK : FADS_DONE; \
  } while (0)

// Advance the analysis until done or out of budget. Returns 1 when DONE (result in *out
// if non-NULL), else 0. A finished job must be re-armed with fadStart before reuse.
static int fadStep(FadJob *J,
                   int64_t deadline, FadResult *out) {
  int n = J->n;
  for (;;) {
    switch (J->stage) {
      case FADS_IDLE:
        return 0;

      case FADS_CONVERT: {
        while (J->pos < n) {
          int end = J->pos + FAD_SEG_SAMPLES;
          if (end > n) end = n;
          for (int i = J->pos; i < end; i++) J->xf[i] = (float)J->src[i];
          J->pos = end;
          if (J->pos < n && FAD_NOW_US() >= deadline) return 0;
        }
        J->pos = 0;
        J->stage = FADS_S3;
        break;
      }

      case FADS_S3: {  // boxcar-3, numpy clipped-window semantics
        const float *x = J->xf;
        while (J->pos < n) {
          int end = J->pos + FAD_SEG_SAMPLES;
          if (end > n) end = n;
          for (int i = J->pos; i < end; i++) {
            int lo = i - 1;
            if (lo < 0) lo = 0;
            int hi = i + 2;
            if (hi > n) hi = n;
            float acc = 0.0f;
            for (int q = lo; q < hi; q++) acc += x[q];
            J->s[i] = acc / (float)(hi - lo);
          }
          J->pos = end;
          if (J->pos < n && FAD_NOW_US() >= deadline) return 0;
        }
        J->pos = 0;
        J->nRough = (int)llrintf(FAD_ROUGH_MS * 1e-3f * J->fs);
        if (J->nRough < 3) J->nRough = 3;
        J->stage = FADS_PRE_S;
        break;
      }

      case FADS_PRE_S: {
        if (J->pos == 0) J->pre[0] = 0.0;
        while (J->pos < n) {
          int end = J->pos + FAD_SEG_SAMPLES;
          if (end > n) end = n;
          for (int i = J->pos; i < end; i++) J->pre[i + 1] = J->pre[i] + (double)J->s[i];
          J->pos = end;
          if (J->pos < n && FAD_NOW_US() >= deadline) return 0;
        }
        J->pos = 0;
        J->stage = FADS_R1;
        break;
      }

      case FADS_R1: {
        while (J->pos < n) {
          int end = J->pos + FAD_SEG_SAMPLES;
          if (end > n) end = n;
          for (int i = J->pos; i < end; i++)
            J->rr[i] = J->s[i] - fadMov(J->pre, n, J->nRough, i);
          J->pos = end;
          if (J->pos < n && FAD_NOW_US() >= deadline) return 0;
        }
        J->pos = 0;
        J->acc = 0.0;
        J->stage = FADS_R1_MEAN;
        break;
      }

      case FADS_R1_MEAN: {
        while (J->pos < n) {
          int end = J->pos + FAD_SEG_SAMPLES;
          if (end > n) end = n;
          float part = 0.0f;
          for (int i = J->pos; i < end; i++) part += J->rr[i];
          J->acc += (double)part;
          J->pos = end;
          if (J->pos < n && FAD_NOW_US() >= deadline) return 0;
        }
        J->r1Mean = J->acc / (double)n;
        J->pos = 0;
        J->acc = 0.0;
        J->stage = FADS_R1_VAR;
        break;
      }

      case FADS_R1_VAR: {
        float m = (float)J->r1Mean;
        while (J->pos < n) {
          int end = J->pos + FAD_SEG_SAMPLES;
          if (end > n) end = n;
          float part = 0.0f;
          for (int i = J->pos; i < end; i++) {
            float d = J->rr[i] - m;
            part += d * d;
          }
          J->acc += (double)part;
          J->pos = end;
          if (J->pos < n && FAD_NOW_US() >= deadline) return 0;
        }
        J->r1Var = J->acc / (double)n;
        J->lagMaxCls = n / 4;
        if (J->lagMaxCls > FAD_ACF_LAG_CAP) J->lagMaxCls = FAD_ACF_LAG_CAP;
        J->lag = 4;
        J->seg = 0;
        J->acc = 0.0;
        J->stage = FADS_REGIME_ACF;
        break;
      }

      case FADS_REGIME_ACF: {
        if (J->r1Var <= 0.0) {  // flat signal → every value 0 → no peaks → no-signal
          for (int l = 4; l <= J->lagMaxCls; l++) J->acf[l - 4] = 0.0f;
          J->stage = FADS_REGIME_PEAK;
          break;
        }
        float m = (float)J->r1Mean;
        while (J->lag <= J->lagMaxCls) {
          int len = n - J->lag;
          while (J->seg < len) {
            int end = J->seg + FAD_SEG_SAMPLES;
            if (end > len) end = len;
            float part = 0.0f;
            const float *a = J->rr + J->seg, *b = J->rr + J->seg + J->lag;
            int cnt = end - J->seg;
            for (int i = 0; i < cnt; i++) part += (a[i] - m) * (b[i] - m);
            J->acc += (double)part;
            J->seg = end;
            if (J->seg < len && FAD_NOW_US() >= deadline) return 0;
          }
          J->acf[J->lag - 4] = (float)(J->acc / (double)len / J->r1Var);
          J->lag++;
          J->seg = 0;
          J->acc = 0.0;
          if (J->lag <= J->lagMaxCls && FAD_NOW_US() >= deadline) return 0;
        }
        J->stage = FADS_REGIME_PEAK;
        break;
      }

      case FADS_REGIME_PEAK: {
        int m = J->lagMaxCls - 4 + 1;
        int bestP = -1;
        for (int p = 1; p < m - 1; p++)
          if (J->acf[p] > J->acf[p - 1] && J->acf[p] >= J->acf[p + 1])
            if (bestP < 0 || J->acf[p] > J->acf[bestP]) bestP = p;
        if (bestP < 0) {  // no periodicity at all → no-signal
          J->stage = FADS_DONE;
          break;
        }
        J->Tcycle = (float)(4 + bestP);
        J->cruise = (J->Tcycle < FAD_REGIME_CYCLE_MS * 1e-3f * J->fs) ? 1 : 0;
        if (J->cruise) {
          J->nRungs = 3;
          J->rungW[0] = 0.4f;  J->rungF[0] = 0.3f;
          J->rungW[1] = 0.4f;  J->rungF[1] = 0.5f;
          J->rungW[2] = 0.75f; J->rungF[2] = 0.65f;
        } else {
          J->nRungs = 1;
          J->rungW[0] = 0.75f;
          J->rungF[0] = 0.65f;
        }
        J->pos = 0;
        J->pickStage = FADP_MAXIMA;
        J->stage = FADS_P1_PICK;
        break;
      }

      case FADS_P1_PICK: {  // bootstrap pitch pick — always the conservative rung
        int rc = fadPickStep(J, deadline, 0.75f, 0.65f);
        if (rc == 0) return 0;
        if (rc < 0 || J->nKept < 32) {  // every rung would fail identically → quiet
          J->stage = FADS_DONE;
          break;
        }
        J->stage = FADS_PITCH;
        break;
      }

      case FADS_PITCH: {
        int niv = J->nKept - 1;
        for (int q = 0; q < niv; q++)
          J->scratch[q] = (float)(J->kept[q + 1] - J->kept[q]);
        J->P = fadMedian(J->scratch, niv);
        if (!(J->P >= 4.0f && J->P <= 400.0f)) {
          J->stage = FADS_DONE;
          break;
        }
        J->nCyc = (int)llrintf(6.0f * J->P);
        J->envN = (int)llrintf(FAD_ENV_CYCLES * (float)J->nCyc);
        J->pos = 0;
        J->stage = FADS_R2;
        break;
      }

      case FADS_R2: {  // pre[] still holds prefix(s) from FADS_PRE_S
        while (J->pos < n) {
          int end = J->pos + FAD_SEG_SAMPLES;
          if (end > n) end = n;
          for (int i = J->pos; i < end; i++)
            J->rr[i] = J->s[i] - fadMov(J->pre, n, J->nCyc, i);
          J->pos = end;
          if (J->pos < n && FAD_NOW_US() >= deadline) return 0;
        }
        J->pos = 0;
        J->acc = 0.0;
        J->stage = FADS_R_RMS;
        break;
      }

      case FADS_R_RMS: {
        while (J->pos < n) {
          int end = J->pos + FAD_SEG_SAMPLES;
          if (end > n) end = n;
          float part = 0.0f;
          for (int i = J->pos; i < end; i++) part += J->rr[i] * J->rr[i];
          J->acc += (double)part;
          J->pos = end;
          if (J->pos < n && FAD_NOW_US() >= deadline) return 0;
        }
        J->rms = (float)sqrt(J->acc / (double)n);
        J->pos = 0;
        J->stage = FADS_PRE_ABSR;
        break;
      }

      case FADS_PRE_ABSR: {
        if (J->pos == 0) J->pre[0] = 0.0;
        while (J->pos < n) {
          int end = J->pos + FAD_SEG_SAMPLES;
          if (end > n) end = n;
          for (int i = J->pos; i < end; i++)
            J->pre[i + 1] = J->pre[i] + (double)fabsf(J->rr[i]);
          J->pos = end;
          if (J->pos < n && FAD_NOW_US() >= deadline) return 0;
        }
        J->pos = 0;
        J->stage = FADS_ENV;
        break;
      }

      case FADS_ENV: {
        float floorv = 0.2f * J->rms;
        while (J->pos < n) {
          int end = J->pos + FAD_SEG_SAMPLES;
          if (end > n) end = n;
          for (int i = J->pos; i < end; i++) {
            float e = fadMov(J->pre, n, J->envN, i);
            J->env[i] = (e > floorv) ? e : floorv;
          }
          J->pos = end;
          if (J->pos < n && FAD_NOW_US() >= deadline) return 0;
        }
        J->pos = 0;
        J->acc = 0.0;
        J->stage = FADS_RN;
        break;
      }

      case FADS_RN: {  // rn into s (s is dead after R2); accumulate mean for the ACFs
        while (J->pos < n) {
          int end = J->pos + FAD_SEG_SAMPLES;
          if (end > n) end = n;
          float part = 0.0f;
          for (int i = J->pos; i < end; i++) {
            J->s[i] = J->rr[i] / J->env[i];
            part += J->s[i];
          }
          J->acc += (double)part;
          J->pos = end;
          if (J->pos < n && FAD_NOW_US() >= deadline) return 0;
        }
        J->rnMean = J->acc / (double)n;
        J->pos = 0;
        J->acc = 0.0;
        J->stage = FADS_RN_VAR;
        break;
      }

      case FADS_RN_VAR: {
        float m = (float)J->rnMean;
        while (J->pos < n) {
          int end = J->pos + FAD_SEG_SAMPLES;
          if (end > n) end = n;
          float part = 0.0f;
          for (int i = J->pos; i < end; i++) {
            float d = J->s[i] - m;
            part += d * d;
          }
          J->acc += (double)part;
          J->pos = end;
          if (J->pos < n && FAD_NOW_US() >= deadline) return 0;
        }
        J->rnVar = J->acc / (double)n;
        J->rung = 0;
        J->pos = 0;
        J->pickStage = FADP_MAXIMA;
        J->stage = FADS_RUNG_PICK;
        break;
      }

      case FADS_RUNG_PICK: {
        int rc = fadPickStep(J, deadline, J->rungW[J->rung], J->rungF[J->rung]);
        if (rc == 0) return 0;
        if (rc < 0) { fadNextRung(J); break; }
        J->sane2 = J->pickSane;
        J->stage = FADS_RUNG_WEAK;
        break;
      }

      case FADS_RUNG_WEAK: {
        const float *r = J->rr;
        // tall-height reference uses ALL raw maxima; weak floor filters merged crests
        for (int q = 0; q < J->nIdx; q++) J->scratch[q] = r[J->idx[q]];
        J->tallThr = FAD_HEIGHT_REF_FRAC * fadPercentile(J->scratch, J->nIdx, 90.0f);
        int nt = 0;
        for (int q = 0; q < J->nKept; q++) {
          float kh = r[J->kept[q]];
          if (kh >= J->tallThr) J->scratch[nt++] = kh;
        }
        if (nt > 0) {
          J->medTall = fadMedian(J->scratch, nt);
        } else {
          for (int q = 0; q < J->nKept; q++) J->scratch[q] = r[J->kept[q]];
          J->medTall = fadMedian(J->scratch, J->nKept);
        }
        float floorv = FAD_WEAK_KEEP_FRAC * J->medTall;
        int nk = 0;
        for (int q = 0; q < J->nKept; q++)
          if (r[J->kept[q]] >= floorv) J->kept[nk++] = J->kept[q];
        J->nKept = nk;
        if (J->nKept < 2 * FAD_BLOCK_CRESTS) { fadNextRung(J); break; }
        J->stage = FADS_RUNG_SUBTRAIN;
        break;
      }

      case FADS_RUNG_SUBTRAIN: {
        // sup = raw maxima not kept; nearest-kept distance via two sorted pointers.
        // Guard mirrors the prototype's len(sup)*len(kept) < 4e7 cutoff.
        const float *r = J->rr;
        int nSup = J->nIdx - J->nKept;
        J->subtrain = 0.0f;
        if (nSup > 0 && (double)nSup * (double)J->nKept < 4e7) {
          int cnt = 0, kp = 0, q = 0;
          float hFloor = 0.3f * J->medTall, dFloor = 0.3f * J->P;
          for (int p = 0; p < J->nIdx; p++) {
            int e = J->idx[p];
            if (q < J->nKept && J->kept[q] == e) { q++; continue; }  // member of kept
            while (kp + 1 < J->nKept && J->kept[kp + 1] <= e) kp++;
            int d1 = e - J->kept[kp];
            if (d1 < 0) d1 = -d1;
            int d2 = (kp + 1 < J->nKept) ? J->kept[kp + 1] - e : d1;
            if (d2 < 0) d2 = -d2;
            int dNear = (d1 < d2) ? d1 : d2;
            if (r[e] >= hFloor && (float)dNear >= dFloor) cnt++;
          }
          J->subtrain = (float)cnt / (float)J->nKept;
        }
        J->pos = 0;
        J->stage = FADS_RUNG_FEATURES;
        break;
      }

      case FADS_RUNG_FEATURES: {
        const float *r = J->rr;
        while (J->pos < J->nKept) {
          int end = J->pos + 2048;
          if (end > J->nKept) end = J->nKept;
          for (int q = J->pos; q < end; q++) {
            int i = J->kept[q];
            if (i >= 1 && i <= n - 2) {
              fadParabolic(r, i, &J->tt[q], &J->hh[q]);
            } else {
              J->tt[q] = (float)i;
              J->hh[q] = r[i];
            }
            J->hn[q] = J->hh[q] / J->env[i];
          }
          J->pos = end;
          if (J->pos < J->nKept && FAD_NOW_US() >= deadline) return 0;
        }
        memcpy(J->scratch, J->hn, sizeof(float) * (size_t)J->nKept);
        J->scaleH = fadMedian(J->scratch, J->nKept);
        int niv = J->nKept - 1;
        for (int q = 0; q < niv; q++) J->iv[q] = J->tt[q + 1] - J->tt[q];
        memcpy(J->scratch, J->iv, sizeof(float) * (size_t)niv);
        J->medIv = fadMedian(J->scratch, niv);
        for (int q = 0; q < niv; q++) J->dn[q] = J->iv[q] / J->medIv;
        J->pos = 0;
        J->stage = FADS_RUNG_BLOCKS;
        break;
      }

      case FADS_RUNG_BLOCKS: {
        int nBlkH = J->nKept / FAD_BLOCK_CRESTS;
        int nBlkI = (J->nKept - 1) / FAD_BLOCK_CRESTS;
        if (nBlkH > J->blocksCap) nBlkH = J->blocksCap;
        if (nBlkI > J->blocksCap) nBlkI = J->blocksCap;
        int nBlkMax = (nBlkH > nBlkI) ? nBlkH : nBlkI;
        float sh = (J->scaleH > 1e-9f) ? J->scaleH : 1e-9f;
        int plane = J->blocksCap;
        while (J->pos < nBlkMax) {
          int b = J->pos;
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
          J->pos++;
          if (J->pos < nBlkMax && FAD_NOW_US() >= deadline) return 0;
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
        J->stage = FADS_RUNG_SYNC_PREP;
        break;
      }

      case FADS_RUNG_SYNC_PREP: {
        int niv = J->nKept - 1;
        memcpy(J->scratch, J->iv, sizeof(float) * (size_t)niv);
        J->refIv = fadPercentile(J->scratch, niv, FAD_GAP_REF_PCT);
        J->nGap = 0;
        float thr = FAD_GAP_THR * J->refIv;
        for (int q = 0; q < niv; q++)
          if (J->iv[q] > thr) J->gpos[J->nGap++] = q;
        J->lagLo = (int)(1.6f * J->refIv);
        if (J->lagLo < 4) J->lagLo = 4;
        J->lagHi = n / 4;
        if (J->lagHi > FAD_ACF_LAG_CAP) J->lagHi = FAD_ACF_LAG_CAP;
        if (J->nGap < FAD_MIN_GAPS || J->lagLo > J->lagHi) {
          // too few gaps (or empty lag range) — block path only
          J->confirmPhase = 0;
          J->lag = -1;
          J->stage = FADS_RUNG_CONFIRM;
          break;
        }
        J->lag = J->lagLo;
        J->seg = 0;
        J->acc = 0.0;
        J->stage = FADS_RUNG_SYNC_ACF;
        break;
      }

      case FADS_RUNG_SYNC_ACF: {
        const float *rn = J->s;
        float m = (float)J->rnMean;
        if (J->rnVar <= 0.0) {
          for (int l = J->lagLo; l <= J->lagHi; l++) J->acf[l - J->lagLo] = 0.0f;
          J->stage = FADS_RUNG_SYNC_MATCH;
          break;
        }
        while (J->lag <= J->lagHi) {
          int len = n - J->lag;
          while (J->seg < len) {
            int end = J->seg + FAD_SEG_SAMPLES;
            if (end > len) end = len;
            float part = 0.0f;
            const float *a = rn + J->seg, *b = rn + J->seg + J->lag;
            int cnt = end - J->seg;
            for (int i = 0; i < cnt; i++) part += (a[i] - m) * (b[i] - m);
            J->acc += (double)part;
            J->seg = end;
            if (J->seg < len && FAD_NOW_US() >= deadline) return 0;
          }
          J->acf[J->lag - J->lagLo] = (float)(J->acc / (double)len / J->rnVar);
          J->lag++;
          J->seg = 0;
          J->acc = 0.0;
          if (J->lag <= J->lagHi && FAD_NOW_US() >= deadline) return 0;
        }
        J->stage = FADS_RUNG_SYNC_MATCH;
        break;
      }

      case FADS_RUNG_SYNC_MATCH: {
        int m = J->lagHi - J->lagLo + 1;
        int bestP = -1;
        for (int p = 1; p < m - 1; p++)
          if (J->acf[p] > J->acf[p - 1] && J->acf[p] >= J->acf[p + 1])
            if (bestP < 0 || J->acf[p] > J->acf[bestP]) bestP = p;
        if (bestP < 0 || J->acf[bestP] < 0.5f) {
          J->confirmPhase = 0;
          J->lag = -1;
          J->stage = FADS_RUNG_CONFIRM;
          break;
        }
        float T0 = (float)(J->lagLo + bestP);
        // each gap matched independently to its partner one period later (±10%)
        int okc = 0;
        J->nInst = 0;
        for (int j = 0; j < J->nGap; j++) {
          float target = J->tt[J->gpos[j]] + T0;
          int lo = 0, hi = J->nGap;  // lower_bound over gap anchor times
          while (lo < hi) {
            int mid = (lo + hi) / 2;
            if (J->tt[J->gpos[mid]] < target) lo = mid + 1;
            else hi = mid;
          }
          int bestC = -1;
          float bd = T0;
          for (int t = 0; t < 2; t++) {
            int c = lo - 1 + t;
            if (c >= 0 && c < J->nGap && c > j) {
              float d = fabsf(J->tt[J->gpos[c]] - target);
              if (d < bd) { bestC = c; bd = d; }
            }
          }
          if (bestC >= 0 && bd <= 0.10f * T0) {
            okc++;
            J->instA[J->nInst] = j;
            J->instB[J->nInst] = bestC;
            J->nInst++;
          }
        }
        if ((float)okc / (float)J->nGap >= FAD_SYNC_PARTNER_MIN && J->nInst >= FAD_MIN_MODAL_SEGS) {
          for (int i = 0; i < J->nInst; i++)
            J->cnts[i] = J->gpos[J->instB[i]] - J->gpos[J->instA[i]];
          // modal count: sorted-unique scan, first max wins (np.unique + argmax semantics)
          for (int i = 0; i < J->nInst; i++) J->rowsH[i] = (float)J->cnts[i];
          qsort(J->rowsH, (size_t)J->nInst, sizeof(float), fadCmpFloat);
          int mMod = 0, cMax = 0, i0 = 0;
          while (i0 < J->nInst) {
            int i1 = i0;
            while (i1 < J->nInst && J->rowsH[i1] == J->rowsH[i0]) i1++;
            if (i1 - i0 > cMax) {
              cMax = i1 - i0;
              mMod = (int)J->rowsH[i0];
            }
            i0 = i1;
          }
          float frac = (float)cMax / (float)J->nInst;
          if (mMod >= 2 && mMod <= 8 && frac >= FAD_SEG_FRAC_MIN) {
            int mm = mMod, niv = J->nKept - 1;
            J->nRows = 0;
            for (int i = 0; i < J->nInst; i++) {
              if (J->cnts[i] != mm) continue;
              int g = J->gpos[J->instA[i]];
              if (g + mm + 1 > niv) continue;  // prototype would index out of range here
              if ((J->nRows + 1) * mm > J->rowsCap) break;
              float hrow[8], irow[8], key[8];
              for (int q = 0; q < mm; q++) {
                hrow[q] = J->hn[g + 1 + q];
                irow[q] = J->iv[g + 1 + q] / J->refIv;
                key[q] = rintf(irow[q] * 2.0f) / 2.0f;
              }
              int rot = 0;  // lexicographically maximal rotation, first wins ties
              for (int s0 = 1; s0 < mm; s0++) {
                for (int q = 0; q < mm; q++) {
                  float a = key[(s0 + q) % mm], b = key[(rot + q) % mm];
                  if (a > b) { rot = s0; break; }
                  if (a < b) break;
                }
              }
              for (int q = 0; q < mm; q++) {
                J->rowsH[J->nRows * mm + q] = hrow[(q + rot) % mm];
                J->rowsI[J->nRows * mm + q] = irow[(q + rot) % mm];
              }
              J->nRows++;
            }
            if (J->nRows >= FAD_MIN_MODAL_SEGS) {
              float fsh, esh, fsi, esi;
              fadAnova(J->rowsH, J->nRows * mm, mm, &fsh, &esh);
              fadAnova(J->rowsI, J->nRows * mm, mm, &fsi, &esi);
              float sh = (J->scaleH > 1e-9f) ? J->scaleH : 1e-9f;
              float dSyncH = esh / sh, dSyncI = esi;
              int kSync = (int)llrint((double)T0 / (double)J->refIv);
              if (kSync < 2) kSync = 2;
              if (kSync > 8) kSync = 8;
              if (dSyncI > J->dI) {
                J->runI = (J->dI > dSyncI * 1e-3f) ? J->dI : dSyncI * 1e-3f;
                J->kI = kSync;
                J->dI = dSyncI;
                J->Fi[kSync] = fsi;
                J->syncUsed = 1;
              }
              if (dSyncH > J->dH) {
                J->runH = (J->dH > dSyncH * 1e-3f) ? J->dH : dSyncH * 1e-3f;
                J->kH = kSync;
                J->dH = dSyncH;
                J->Fh[kSync] = fsh;
                J->syncUsed = 1;
              }
              if (J->syncUsed) {
                J->T0 = T0;
                J->Tsync = T0;
              }
            }
          }
        }
        J->confirmPhase = 0;
        J->lag = -1;
        J->stage = FADS_RUNG_CONFIRM;
        break;
      }

      case FADS_RUNG_CONFIRM: {
        if (J->lag < 0) {  // one-time setup: T_win from the primary feature pick
          uint8_t trigH = (J->dH >= FAD_TH_H && J->Fh[J->kH] >= FAD_F_SIG) ? 1 : 0;
          uint8_t trigI = (J->dI >= FAD_TH_INT && J->Fi[J->kI] >= FAD_F_SIG) ? 1 : 0;
          if (J->syncUsed) {
            J->Twin = J->Tsync;
          } else {
            uint8_t featI = (trigI || (!trigH && J->dI / FAD_TH_INT >= J->dH / FAD_TH_H)) ? 1 : 0;
            int kWin = featI ? J->kI : J->kH;
            int niv = J->nKept - 1;
            double sum = 0.0;
            for (int q = 0; q < niv; q++) sum += (double)J->iv[q];
            J->Twin = (float)kWin * (float)(sum / (double)niv);
          }
          J->aP = -1e30f;
          J->aT = -1e30f;
          J->confirmPhase = 0;
          int pLo0 = (int)(0.75f * J->P);
          if (pLo0 < 2) pLo0 = 2;
          J->lag = pLo0;
          J->seg = 0;
          J->acc = 0.0;
        }
        const float *rn = J->s;
        float m = (float)J->rnMean;
        int pLo = (int)(0.75f * J->P);
        if (pLo < 2) pLo = 2;
        int pHi = (int)(1.3f * J->P);
        int tLo = (int)(0.85f * J->Twin);
        if (tLo < 2) tLo = 2;
        int tHi = (int)(1.15f * J->Twin) + 1;
        if (pHi > n - 2) pHi = n - 2;
        if (tHi > n - 2) tHi = n - 2;
        if (J->rnVar > 0.0) {
          for (;;) {
            int hiNow = (J->confirmPhase == 0) ? pHi : tHi;
            if (J->lag > hiNow) {
              if (J->confirmPhase == 0) {
                J->confirmPhase = 1;
                J->lag = tLo;
                J->seg = 0;
                J->acc = 0.0;
                continue;
              }
              break;
            }
            // phase 1 skips lags already covered by phase 0 (they updated aT there)
            if (J->confirmPhase == 1 && J->lag >= pLo && J->lag <= pHi) {
              J->lag = pHi + 1;
              J->seg = 0;
              J->acc = 0.0;
              continue;
            }
            int len = n - J->lag;
            int finished = 1;
            while (J->seg < len) {
              int end = J->seg + FAD_SEG_SAMPLES;
              if (end > len) end = len;
              float part = 0.0f;
              const float *a = rn + J->seg, *b = rn + J->seg + J->lag;
              int cnt = end - J->seg;
              for (int i = 0; i < cnt; i++) part += (a[i] - m) * (b[i] - m);
              J->acc += (double)part;
              J->seg = end;
              if (J->seg < len && FAD_NOW_US() >= deadline) { finished = 0; break; }
            }
            if (!finished) return 0;
            float v = (float)(J->acc / (double)len / J->rnVar);
            if (J->lag >= pLo && J->lag <= pHi && v > J->aP) J->aP = v;
            if (J->lag >= tLo && J->lag <= tHi && v > J->aT) J->aT = v;
            J->lag++;
            J->seg = 0;
            J->acc = 0.0;
            if (FAD_NOW_US() >= deadline) return 0;
          }
        }
        if (J->aP < -1e29f) J->aP = 0.0f;
        if (J->aT < -1e29f) J->aT = 0.0f;
        J->stage = FADS_RUNG_VERDICT;
        break;
      }

      case FADS_RUNG_VERDICT: {
        uint8_t trigH = (J->dH >= FAD_TH_H && J->Fh[J->kH] >= FAD_F_SIG) ? 1 : 0;
        uint8_t trigI = (J->dI >= FAD_TH_INT && J->Fi[J->kI] >= FAD_F_SIG) ? 1 : 0;
        uint8_t featI = (trigI || (!trigH && J->dI / FAD_TH_INT >= J->dH / FAD_TH_H)) ? 1 : 0;
        int kWin = featI ? J->kI : J->kH;
        float dWin = featI ? J->dI : J->dH;
        float runner = featI ? J->runI : J->runH;
        float thr = featI ? FAD_TH_INT : FAD_TH_H;
        float aPd = (J->aP > 0.05f) ? J->aP : 0.05f;
        float acfRatio = J->aT / aPd;
        uint8_t confirmed = (J->syncUsed || acfRatio >= FAD_ACF_CONFIRM) ? 1 : 0;
        uint8_t verdict;
        if ((trigH || trigI) && confirmed) verdict = FADV_FAULT;
        else if (J->dI >= FAD_TREND_INT || J->dH >= FAD_TREND_H) verdict = FADV_TREND;
        else verdict = FADV_HEALTHY;

        float hr1 = J->dI / FAD_TH_INT, hr2 = J->dH / FAD_TH_H;
        float headroom = (hr1 > hr2) ? hr1 : hr2;
        uint8_t better = !J->haveBest
                         || (J->sane2 > J->bestSane)
                         || (J->sane2 == J->bestSane && headroom > J->bestHeadroom);
        if (better) {
          J->haveBest = 1;
          J->bestSane = J->sane2;
          J->bestHeadroom = headroom;
          FadResult *R = &J->best;
          R->verdict = verdict;
          R->winningK = (uint8_t)kWin;
          R->featInterval = featI;
          R->sync = J->syncUsed;
          R->score = dWin;
          R->marginThr = dWin / thr;
          R->marginRunner = dWin / ((runner > 1e-12f) ? runner : 1e-12f);
          R->Dheight = J->dH;
          R->Dinterval = J->dI;
          R->Fwin = featI ? J->Fi[J->kI] : J->Fh[J->kH];
          R->acfRatio = acfRatio;
          R->periodRatio = J->Twin / J->P;
          R->pitch = J->P;
          R->subtrain = J->subtrain;
          R->nCrests = J->nKept;
        }
        fadNextRung(J);
        break;
      }

      case FADS_DONE: {
        if (out) {
          if (J->haveBest) *out = J->best;
          else memset(out, 0, sizeof(*out));  // no-signal → quiet
        }
        J->stage = FADS_IDLE;
        return 1;
      }

      default:
        J->stage = FADS_IDLE;
        return 1;
    }
  }
}
