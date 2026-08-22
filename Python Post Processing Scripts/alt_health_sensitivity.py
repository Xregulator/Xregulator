#!/usr/bin/env python3
"""Per-axis sensitivity study of Alternator Health Dataset exports.

Usage:  python3 alt_health_sensitivity.py <export1.csv> [<export2.csv> ...]

Feed it one or more "Export Health Dataset" files (Charging System Health button). For each
file it parses the MYHIST records and SESSPT dots, then across all files fits:
  - excitation gain g = dlnA/dln(exc) from within-file record pairs at like temp/Vbus
  - apparent temp slope of field-corrected output per file (positive during warm-up =
    case-sensor lag; ~0 settled; physical expectation ~ -0.1%/F)
  - normalized-output offsets between files/phases at like tags (the 2026-08-21 finding:
    10-20% bands, airflow/thermal-state hypothesis)
It prints the numbers and writes figures next to the first input file (_altsens_*.png).

Export vintages (2026-08-21 boundary):
  - NEW exports carry per-dot vbus, tSlopeFmin (case-temp slope F/min), sogKts (motion ground
    truth), fieldPct at 2 dp; MYHIST rows end with dutyPct,tSlopeFmin. Used directly.
  - OLD exports lack vbus (reconstructed here by interpolating the MYHIST V trajectory over
    an idx-based uptime estimate) and have integer fieldPct (expect 5-6% quantization streaks
    in corrected output at g~2 — do not read those streaks as machine behavior).
Background + findings log: Working Markdown Docs/Alt_Health_Dev_Summary.md section 8.
"""
import sys
import numpy as np, pandas as pd, matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
from pathlib import Path

ALPHA, TREF = 0.00393, 25.0
def excitation(dutyPct, vbus, tF):
    tc = (tF - 32.0) / 1.8
    return (dutyPct / 100.0) * vbus / (1.0 + ALPHA * (tc - TREF))

def parse(path):
    recs, dots = [], []
    sess_cols = None
    for line in Path(path).read_text().splitlines():
        p = line.split(',')
        if p[0] == 'MYHIST' and len(p) >= 9:
            r = dict(idx=int(p[1]), rpm=float(p[2]), exc=float(p[3]), V=float(p[4]),
                     F=float(p[5]), A=float(p[6]), n=int(p[7]), t=float(p[8]) / 1000.0)
            if len(p) >= 11:                     # new format: dutyPct, tSlopeFmin appended
                r['duty'] = float(p[9]); r['tSlope'] = float(p[10])
            recs.append(r)
        elif p[0] == 'SESSPT' and not p[1].isdigit():
            sess_cols = p[1:]                    # header row names the columns
        elif p[0] == 'SESSPT' and len(p) >= 13:
            d = dict(idx=int(p[1]), t=float(p[2]), rpm=float(p[3]), duty=float(p[4]),
                     F=float(p[5]), mA=float(p[6]), gA=float(p[7]), eA=float(p[8]),
                     pct=float(p[9]), state=int(p[10]), ring=int(p[12]))
            def opt(name):
                if sess_cols and name in sess_cols:
                    v = p[1 + sess_cols.index(name)]
                    return float(v) if v not in ('', None) else np.nan
                return np.nan
            d['vbus'] = opt('vbus'); d['tSlope'] = opt('tSlopeFmin'); d['sog'] = opt('sogKts')
            dots.append(d)
    return pd.DataFrame(recs), pd.DataFrame(dots)

def prep(path):
    rec, dot = parse(path)
    rec = rec.sort_values('t').reset_index(drop=True)
    if dot.empty: return rec, dot
    if dot.vbus.isna().all():                    # old export: reconstruct vbus from records
        # dot counter ticks ~5.1 s from boot; pin the offset with a cruise stint if both sides
        # have one, else fall back to idx*5.1.
        cr_r, cr_d = rec[rec.rpm > 1000], dot[dot.rpm > 1000]
        off = (cr_r.t.min() - cr_d.t.min()) if (len(cr_r) and len(cr_d)) else dot.idx.iloc[0] * 5.1 - dot.t.iloc[0]
        up = dot.t + off
        this_boot = rec if rec.t.is_monotonic_increasing else rec[rec.t < 900]
        dot = dot.assign(vbus=np.interp(up, this_boot.t.values, this_boot.V.values))
    dot = dot.assign(exc=excitation(dot.duty, dot.vbus, dot.F))
    return rec, dot

def main(paths):
    outdir = Path(paths[0]).parent
    sets = []
    for pth in paths:
        rec, dot = prep(pth)
        label = Path(pth).stem.replace('Alternator Health Dataset ', '')
        sets.append((label, rec, dot))
        print(f'{label}: {len(rec)} records, {len(dot)} dots')

    # excitation gain from within-file idle record pairs at like temp/V
    pairs = []
    for label, rec, _ in sets:
        ri = rec[rec.rpm < 1000].reset_index(drop=True)
        for i in range(len(ri)):
            for j in range(i + 1, len(ri)):
                a, b = ri.iloc[i], ri.iloc[j]
                if abs(a.F - b.F) > 8 or abs(a.V - b.V) > 0.06: continue
                dle = np.log(b.exc / a.exc)
                if abs(dle) < 0.01: continue
                pairs.append(np.log(b.A / a.A) / dle)
    g = float(np.median(pairs)) if pairs else 2.0
    print(f'\nexc gain g = {g:.2f} (median of {len(pairs)} pairs; 2026-08-21 baseline was 2.0)')

    # per-file temp slope + normalized output level
    print(f'\nfield-corrected output eta = ln(gA) - g*ln(exc); temp slope of eta per file:')
    base = None
    for label, _, dot in sets:
        d = dot[(dot.rpm < 1000)].dropna(subset=['gA', 'exc'])
        if len(d) < 6: print(f'  {label}: <6 idle dots, skipped'); continue
        eta = np.log(d.gA) - g * np.log(d.exc)
        X = np.column_stack([np.ones(len(d)), d.F])
        beta, *_ = np.linalg.lstsq(X, eta, rcond=None)
        if base is None: base = eta.mean()
        print(f'  {label}: n={len(d)}  slope {beta[1]*100:+.3f} %/F  level {np.exp(eta.mean()-base)*100:6.1f}%'
              f'  (sd {eta.std()*100:.1f}%)  F {d.F.min():.0f}-{d.F.max():.0f}'
              + (f'  tSlope med {d.tSlope.median():+.1f} F/min' if d.tSlope.notna().any() else '')
              + (f'  SOG>1kt dots: {(d.sog > 1).sum()}' if d.sog.notna().any() else ''))

    # figure: eta vs case temp per file, plus eta vs tSlope when available (the lag picture)
    C = ['#2a78d6', '#eb6834', '#1baf7a', '#4a3aa7', '#eda100']
    fig, ax = plt.subplots(figsize=(9, 5.6))
    for k, (label, _, dot) in enumerate(sets):
        d = dot[dot.rpm < 1000].dropna(subset=['gA', 'exc']).sort_values('t')
        if len(d) < 2: continue
        eta = np.log(d.gA) - g * np.log(d.exc)
        ax.plot(d.F, np.exp(eta - base) * 100, 'o-', ms=4, lw=1.1, color=C[k % 5], label=label, alpha=.85)
    ax.set_xlabel('case temperature (F)'); ax.set_ylabel('A / exc^g (% of first-file mean)')
    ax.set_title(f'Field-corrected output vs case temp (g={g:.1f}), time order')
    ax.grid(alpha=.4); ax.legend(fontsize=8.5, frameon=False)
    fig.tight_layout(); fig.savefig(outdir / '_altsens_eta_vs_temp.png', dpi=110); plt.close(fig)

    has_slope = any(s[2].tSlope.notna().any() for s in sets if len(s[2]))
    if has_slope:
        fig, ax = plt.subplots(figsize=(9, 5.6))
        for k, (label, _, dot) in enumerate(sets):
            d = dot[dot.rpm < 1000].dropna(subset=['gA', 'exc', 'tSlope'])
            if len(d) < 2: continue
            eta = np.log(d.gA) - g * np.log(d.exc)
            ax.scatter(d.tSlope, np.exp(eta - base) * 100, s=18, color=C[k % 5], label=label, alpha=.8)
        ax.set_xlabel('case-temp slope (F/min)'); ax.set_ylabel('A / exc^g (%)')
        ax.set_title('Lag test: corrected output vs thermal-transient tag')
        ax.grid(alpha=.4); ax.legend(fontsize=8.5, frameon=False)
        fig.tight_layout(); fig.savefig(outdir / '_altsens_eta_vs_tslope.png', dpi=110); plt.close(fig)
    print(f'\nfigures -> {outdir}/_altsens_*.png')

if __name__ == '__main__':
    if len(sys.argv) < 2:
        print(__doc__); sys.exit(1)
    main(sys.argv[1:])
