"""
_preview_altmatrix.py  —  synthetic data preview for plot_altmatrix.py
Run this to see what the matrix plot looks like without a real export file.
Delete this file when done; it is not part of the normal workflow.
"""

import numpy as np
import pandas as pd
import os, sys

# ── bring in the real plot function ─────────────────────────────────────────
sys.path.insert(0, os.path.dirname(__file__))
from plot_altmatrix import MatrixViewer

rng = np.random.default_rng(42)

RPM_LABELS   = ["0-500",    "500-1k",  "1k-1.5k", "1.5k-2k",
                 "2k-2.5k", "2.5k-3k", "3k-4k",   "4k+"]
TEMP_LABELS  = ["<80F",    "80-110F", "110-140F", "140-160F",
                 "160-180F","180-200F",">200F"]
FIELD_LABELS = ["0-2.1V",  "2.1-4.3V","4.3-6.4V",  "6.4-8.6V",
                 "8.6-10.7V","10.7-12.9V","12.9-15V"]

N_RPM, N_TEMP, N_FIELD = 8, 7, 7

rows = []
ref_bins_remaining = 3   # how many reference bins to sprinkle in

for r in range(N_RPM):
    for t in range(N_TEMP):
        for f in range(N_FIELD):

            # Realistic operating envelope:
            #   engine mostly runs 1k-3k RPM → buckets 2-5
            #   alternator mostly 110-180F     → temp buckets 2-4
            #   field volts mostly 4-11V       → field buckets 2-4
            #   low RPM and very high temp buckets rarely visited
            rpm_weight  = [0.02, 0.05, 0.20, 0.25, 0.22, 0.15, 0.08, 0.03][r]
            temp_weight = [0.04, 0.08, 0.22, 0.28, 0.22, 0.12, 0.04][t]
            fld_weight  = [0.02, 0.06, 0.20, 0.28, 0.24, 0.14, 0.06][f]
            prob_visited = rpm_weight * temp_weight * fld_weight * 80

            visited = rng.random() < prob_visited
            if not visited:
                ss = 0
                avg_a = min_a = max_a = 0.0
                ref_avg = ref_min = ref_max = 0.0
                is_ref = 0
            else:
                # SS time: more in common operating zones, less on edges
                base_ss = rng.integers(30, 900)
                ss = int(base_ss * (rpm_weight * temp_weight * fld_weight * 200) ** 0.5)
                ss = max(10, ss)

                # avg amps: rises with field voltage, drops slightly at high temp
                field_center = [1.07, 3.21, 5.36, 7.50, 9.64, 11.78, 13.93][f]
                base_amps    = field_center * 3.2 + rng.normal(0, 1.5)
                temp_penalty = max(0, (t - 3) * 1.8)
                avg_a = max(2.0, min(48.0, base_amps - temp_penalty))
                spread  = rng.uniform(1.5, 4.0)
                min_a   = max(0.0, avg_a - spread)
                max_a   = avg_a + spread

                # Reference bins: high SS time in the core operating zone
                is_ref = 0
                if ss > 200 and 2 <= r <= 5 and 2 <= t <= 4 and 2 <= f <= 4 and ref_bins_remaining > 0:
                    if rng.random() < 0.25:
                        is_ref = 1
                        ref_bins_remaining -= 1

                if is_ref:
                    ref_avg = avg_a
                    ref_min = min_a
                    ref_max = max_a
                else:
                    ref_avg = ref_min = ref_max = 0.0

            rows.append({
                "rpm_bucket":      r,
                "rpm_label":       RPM_LABELS[r],
                "temp_bucket":     t,
                "temp_label":      TEMP_LABELS[t],
                "field_bucket":    f,
                "field_label":     FIELD_LABELS[f],
                "ss_seconds":      ss,
                "avg_amps":        round(avg_a, 2),
                "min_amps":        round(min_a, 2),
                "max_amps":        round(max_a, 2),
                "ref_avg_amps":    round(ref_avg, 2),
                "ref_min_amps":    round(ref_min, 2),
                "ref_max_amps":    round(ref_max, 2),
                "is_reference_bin": is_ref,
            })

df = pd.DataFrame(rows)

df["populated"] = df["ss_seconds"] > 0
print(f"Synthetic matrix: {len(df)} cells, {df['populated'].sum()} populated")

MatrixViewer(df, "SYNTHETIC_PREVIEW_AltHealthMatrix_20260515_1430.csv")
