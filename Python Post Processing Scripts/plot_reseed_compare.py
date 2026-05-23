"""
Compare CV-loop fastOV recovery across runs with different ReseedFrac values.

Aligns each file by the rising edge of fastOvActive (t=0 = first sample where
fastOvActive goes 0 -> 1) and plots RPM, battery voltage, and alternator
current vs time for each run on shared time axes.

Also prints the observed reseed ratio per file (post-release cv_I / pre-event cv_I)
so the label you assign matches what the data actually shows. Useful when the
shipped firmware default changes and old captures might not be what the filename suggests.

Run:
    cd /Users/joeceo/Documents/Arduino/Xregulator && \
    "$HOME/Documents/Xeng_Python_Venvs/tk/bin/python" \
    "Python Post Processing Scripts/plot_reseed_compare.py"

Edit FILES below to add/remove runs or change labels.
"""

import csv
from pathlib import Path
import matplotlib.pyplot as plt

# ---------------------------------------------------------------------------
# Edit this list. Each entry is (path, label). The script will verify the
# observed reseed ratio matches the label and warn if it does not.
# ---------------------------------------------------------------------------
FILES = [
    ("/Users/joeceo/Downloads/cvlog_20260523_1313_1.5s_6s.csv", "ReseedFrac = 0.1"),
    ("/Users/joeceo/Downloads/FinalsMahbeidle_1s_10s.csv",      "ReseedFrac = 0.7"),
    ("/Users/joeceo/Downloads/Point9_4s_10s.csv",                "ReseedFrac = 0.9"),
]

# Plot window in milliseconds relative to fastOvActive rising edge.
T_PRE_MS  = 200    # how far before the trip to show
T_POST_MS = 1500   # how far after the trip to show


# CSV column indices (0-based). Schema is fixed for cvlog-style output.
COL_T_S       = 0
COL_BATTV     = 1
COL_CV_I      = 7
COL_IMEAS     = 11
COL_FAST_OV   = 13
COL_RPM       = 17


def load_run(path):
    """Read a cvlog CSV and return a dict of parallel lists."""
    t, battV, iMeas, rpm, cv_I, fastOv = [], [], [], [], [], []
    with open(path, newline="") as f:
        for row in csv.reader(f):
            if not row or row[0].startswith("#") or row[0] == "t_s":
                continue
            try:
                t.append(float(row[COL_T_S]))
                battV.append(float(row[COL_BATTV]))
                iMeas.append(float(row[COL_IMEAS]))
                rpm.append(float(row[COL_RPM]))
                cv_I.append(float(row[COL_CV_I]))
                fastOv.append(int(row[COL_FAST_OV]))
            except (ValueError, IndexError):
                continue
    return dict(t=t, battV=battV, iMeas=iMeas, rpm=rpm, cv_I=cv_I, fastOv=fastOv)


def find_event_index(fastOv):
    """First index where fastOvActive goes 0 -> 1. None if no event."""
    for i in range(1, len(fastOv)):
        if fastOv[i] == 1 and fastOv[i - 1] == 0:
            return i
    return None


def find_release_index(fastOv, start):
    """First index >= start where fastOvActive is 0 again."""
    for i in range(start, len(fastOv)):
        if fastOv[i] == 0:
            return i
    return None


def observed_seed_ratio(run, evt_idx):
    """Pre-event cv_I = sample immediately before the trip.
    Post-release cv_I = first sample after fastOv returns to 0.
    Ratio = post / pre.
    """
    pre = run["cv_I"][evt_idx - 1] if evt_idx > 0 else float("nan")
    rel_idx = find_release_index(run["fastOv"], evt_idx)
    post = run["cv_I"][rel_idx] if rel_idx is not None else float("nan")
    ratio = post / pre if pre > 0 else float("nan")
    return pre, post, ratio


def align_and_window(run, evt_idx):
    """Return a sub-run with t in milliseconds relative to event index,
    windowed to [-T_PRE_MS, +T_POST_MS]."""
    t0 = run["t"][evt_idx]
    t_ms = [(t - t0) * 1000.0 for t in run["t"]]
    out = {k: [] for k in run}
    out["t_ms"] = []
    for i, tm in enumerate(t_ms):
        if -T_PRE_MS <= tm <= T_POST_MS:
            out["t_ms"].append(tm)
            for k in ("battV", "iMeas", "rpm", "cv_I", "fastOv"):
                out[k].append(run[k][i])
    return out


def main():
    runs = []
    print("=" * 78)
    print(f"{'file':40s} {'pre cv_I':>10s} {'post cv_I':>10s} {'ratio':>8s}")
    print("-" * 78)
    for path, label in FILES:
        run = load_run(path)
        evt = find_event_index(run["fastOv"])
        if evt is None:
            print(f"{Path(path).name:40s}  (no fastOvActive rising edge found — skipped)")
            continue
        pre, post, ratio = observed_seed_ratio(run, evt)
        windowed = align_and_window(run, evt)
        runs.append((label, windowed, ratio))
        print(f"{Path(path).name:40s} {pre:10.2f} {post:10.2f} {ratio:8.3f}")
    print("=" * 78)
    print("If any 'ratio' does not match the label you assigned in FILES, fix the label.")
    print()

    if not runs:
        print("No usable runs. Exiting.")
        return

    fig, (ax_rpm, ax_v, ax_i) = plt.subplots(3, 1, figsize=(10, 8), sharex=True)

    for label, w, ratio in runs:
        ax_rpm.plot(w["t_ms"], w["rpm"],   label=label)
        ax_v.plot  (w["t_ms"], w["battV"], label=label)
        ax_i.plot  (w["t_ms"], w["iMeas"], label=label)

    for ax in (ax_rpm, ax_v, ax_i):
        ax.axvline(0, color="black", linewidth=0.6, alpha=0.5)
        ax.grid(True, alpha=0.3)

    ax_rpm.set_ylabel("RPM")
    ax_v.set_ylabel("Battery voltage (V)")
    ax_i.set_ylabel("Alternator current (A)")
    ax_i.set_xlabel("Time relative to fastOV rising edge (ms)")

    ax_rpm.set_title("CV-loop fastOV recovery vs ReseedFrac\n(t=0 is the fastOvActive 0→1 edge in each run)")
    ax_rpm.legend(loc="lower right")

    fig.tight_layout()
    out_path = Path(__file__).parent / "plot_reseed_compare.png"
    fig.savefig(out_path, dpi=140)
    print(f"Saved figure to {out_path}")
    plt.show()


if __name__ == "__main__":
    main()
