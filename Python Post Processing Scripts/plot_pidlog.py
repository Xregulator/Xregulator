"""
plot_pidlog.py
Diagnostic plotter for TargetVoltageMode instability.

4 plot windows, each with a state strip below:
  Plot 1 — Voltage: what the outer loop sees (battV, target, vError)
  Plot 2 — Outer loop command chain (rawVoltageCap → voltageCapAmps → uTarget)
  Plot 3 — Inner PID internals (setpoint, input, terms, saturation)
  Plot 4 — Duty pipeline + actual amps (dutyRequest vs dutyApplied vs measAmps)

State strip shows:
  - Color-coded charge stage bar
  - Orange tick marks where voltageLoopRanThisTick=1 (outer loop fired)
  - Cyan dashed vlines for enteringCV
  - Blue dashed vlines for enteringTargetVoltageMode

File picker searches ~/Downloads for pidlog*.csv
PNGs saved to Downloads alongside source CSV.
"""

import glob
import os
import tkinter as tk
from tkinter import messagebox

import matplotlib
matplotlib.use("TkAgg")
import matplotlib.pyplot as plt
import matplotlib.gridspec as gridspec
import pandas as pd
import numpy as np

DOWNLOADS = os.path.expanduser("~/Downloads")

plt.rcParams.update({
    "font.size": 14,
    "axes.titlesize": 18,
    "axes.labelsize": 15,
    "xtick.labelsize": 13,
    "ytick.labelsize": 13,
    "legend.fontsize": 12,
    "figure.titlesize": 16,
})

# ---------------------------------------------------------------------------
# 1. File selector
# ---------------------------------------------------------------------------
def pick_file():
    files = sorted(
        glob.glob(os.path.join(DOWNLOADS, "pidlog*.csv")),
        reverse=True
    )
    if not files:
        messagebox.showerror("No files", f"No pidlog*.csv found in {DOWNLOADS}")
        return None

    selected = []

    root = tk.Tk()
    root.title("Select PID Log")
    root.resizable(False, False)
    root.configure(bg="#1e1e1e")

    tk.Label(
        root,
        text="Select a log file:",
        font=("Helvetica", 16, "bold"),
        bg="#1e1e1e",
        fg="#f0f0f0"
    ).pack(padx=20, pady=(16, 8))

    frame = tk.Frame(root, bg="#1e1e1e")
    frame.pack(padx=20, pady=8)

    scrollbar = tk.Scrollbar(frame, orient=tk.VERTICAL)
    listbox = tk.Listbox(
        frame,
        yscrollcommand=scrollbar.set,
        width=70,
        height=min(len(files), 16),
        font=("Courier", 15),
        selectmode=tk.SINGLE,
        bg="#111111",
        fg="#f0f0f0",
        selectbackground="#42a5f5",
        selectforeground="#ffffff",
        highlightbackground="#555555",
        highlightcolor="#42a5f5"
    )
    scrollbar.config(command=listbox.yview)
    scrollbar.pack(side=tk.RIGHT, fill=tk.Y)
    listbox.pack(side=tk.LEFT)

    for f in files:
        listbox.insert(tk.END, os.path.basename(f))
    listbox.selection_set(0)

    def on_go():
        idxs = listbox.curselection()
        if not idxs:
            messagebox.showwarning("No selection", "Please select a file.")
            return
        selected.append(files[idxs[0]])
        root.destroy()

    def on_cancel():
        root.destroy()

    btn_frame = tk.Frame(root, bg="#1e1e1e")
    btn_frame.pack(pady=16)

    tk.Button(
        btn_frame, text="Open",
        font=("Helvetica", 15, "bold"), command=on_go,
        bg="#42a5f5", fg="#ffffff",
        activebackground="#1e88e5", activeforeground="#ffffff",
        relief=tk.FLAT, bd=0, padx=18, pady=8, width=10
    ).pack(side=tk.LEFT, padx=10)

    tk.Button(
        btn_frame, text="Cancel",
        font=("Helvetica", 14), command=on_cancel,
        bg="#3a3a3a", fg="#f0f0f0",
        activebackground="#555555", activeforeground="#ffffff",
        relief=tk.FLAT, bd=0, padx=18, pady=8, width=10
    ).pack(side=tk.LEFT, padx=10)

    root.mainloop()
    return selected[0] if selected else None


# ---------------------------------------------------------------------------
# 2. Load and parse
# ---------------------------------------------------------------------------
path = pick_file()
if not path:
    raise SystemExit("No file selected.")

basename = os.path.splitext(os.path.basename(path))[0]
print(f"Loading: {path}")

# Robust header search.
#
# Problem: the ESP32 chunked response uses a fixed line buffer. If that buffer
# is too small, the comment block and header row get truncated and may be
# concatenated on the same physical line (no \n between them). Simple
# comment-line counting then skips the header along with the comment.
#
# Solution: read all lines, find the one containing "ts_ms", extract the
# substring from "ts_ms" onward as the column name list, then feed the
# remaining lines to pandas directly. Robust against:
#   - Normal files (header on its own line)
#   - Truncated files (header glued to end of last comment line)
#   - Any leading garbage before "ts_ms"

with open(path, encoding="utf-8", errors="replace") as _f:
    _lines = _f.readlines()

_header_idx  = None
_header_line = None
for _i, _raw in enumerate(_lines):
    if "ts_ms" in _raw:
        _header_idx  = _i
        # Slice from "ts_ms" onward — drops any prepended comment fragment
        _header_line = _raw[_raw.find("ts_ms"):].strip()
        break

if _header_idx is None:
    raise SystemExit(
        f"ERROR: No line containing 'ts_ms' found in {path}.\n"
        f"The CSV may be empty, or the header row was never written.\n"
        f"Check that PidDLState.line[] is >= 420 bytes in the ESP32 handler."
    )

print(f"Header found at file line {_header_idx}: {_header_line[:60]}...")

_col_names = [c.strip() for c in _header_line.split(",")]
print(f"Columns ({len(_col_names)}): {_col_names}")

from io import StringIO
_data_text = "".join(_lines[_header_idx + 1:])
df = pd.read_csv(StringIO(_data_text), names=_col_names, on_bad_lines="skip")

if "ts_ms" not in df.columns:
    raise SystemExit(
        f"ERROR: 'ts_ms' not in parsed columns: {list(df.columns)}"
    )

numeric_cols = [
    "ts_ms",
    "chargeStageDisplay", "TargetVoltageMode",
    "battV", "ChargingVoltageTarget", "vError",
    "Icv", "cv_I",
    "tableThermalLimit", "setpointCmd", "voltageLoopRanThisTick",
    "pidSetpoint", "pidInput", "pidUnsatOutput", "pidOutput",
    "innerTermP", "innerTermI", "innerTermD",
    "dutyRequest", "dutyApplied",
    "enteringCV", "enteringTargetVoltageMode",
    "rpm", "measAmps",
    "gainKp", "gainKi", "gainKd", "flags",
]

for col in numeric_cols:
    if col in df.columns:
        df[col] = pd.to_numeric(df[col], errors="coerce")

df.dropna(subset=["ts_ms"], inplace=True)
df.reset_index(drop=True, inplace=True)
df["t_s"] = (df["ts_ms"] - df["ts_ms"].iloc[0]) / 1000.0

total_time_s = df["t_s"].iloc[-1]

if total_time_s > 7200:
    df["t_plot"] = df["t_s"] / 3600.0
    time_label = "Time (hours)"
elif total_time_s > 120:
    df["t_plot"] = df["t_s"] / 60.0
    time_label = "Time (minutes)"
else:
    df["t_plot"] = df["t_s"]
    time_label = "Time (seconds)"

# Derived signals
df["pid_saturation"] = (df["pidUnsatOutput"] - df["pidOutput"]).abs()
df["duty_clamp"]     = (df["dutyRequest"] - df["dutyApplied"]).abs()

# Gain label from first non-null row
kp_inner = df["gainKp"].dropna().iloc[0] if "gainKp" in df.columns and not df["gainKp"].dropna().empty else float("nan")
ki_inner = df["gainKi"].dropna().iloc[0] if "gainKi" in df.columns and not df["gainKi"].dropna().empty else float("nan")
kd_inner = df["gainKd"].dropna().iloc[0] if "gainKd" in df.columns and not df["gainKd"].dropna().empty else float("nan")
inner_label = f"Inner PID  Kp={kp_inner:.4g}  Ki={ki_inner:.4g}  Kd={kd_inner:.4g}"
print(f"Gains: {inner_label}")

# Convert all integer/flag columns from object dtype (result of names= read)
# to numeric. Use floor division instead of >> to stay safe on float64 Series.
def _to_int(col):
    return pd.to_numeric(df[col], errors="coerce").fillna(0).astype(int)

df["chargeStageDisplay"]        = _to_int("chargeStageDisplay")
df["voltageLoopRanThisTick"]    = _to_int("voltageLoopRanThisTick")
df["enteringCV"]                = _to_int("enteringCV")
df["enteringTargetVoltageMode"] = _to_int("enteringTargetVoltageMode")
df["TargetVoltageMode"]         = _to_int("TargetVoltageMode")

_flags_num = pd.to_numeric(df["flags"], errors="coerce").fillna(0)
df["f_govBypass"] = (_flags_num // 16 % 2).astype(int)  # bit 4: no >> needed

# ---------------------------------------------------------------------------
# 3. Stage colors and mode helpers
# ---------------------------------------------------------------------------
STAGE_COLORS = {
    0: ("#666666", "NONE"),
    1: ("#00c853", "BULK"),
    2: ("#7e57c2", "ABSORPTION"),
    3: ("#ffb300", "FLOAT"),
    4: ("#ef5350", "MANUAL"),
    5: ("#66bb6a", "MAINTAIN"),
    6: ("#42a5f5", "TARGET-V"),
    7: ("#888888", "IDLE"),
}

EV_COLOR_CV     = "#00e5ff"   # enteringCV markers
EV_COLOR_TVM    = "#1565c0"   # enteringTargetVoltageMode markers
EV_COLOR_VLOOP  = "#ff9800"   # voltageLoopRanThisTick strip ticks

def stage_color_label(stage_int):
    return STAGE_COLORS.get(int(stage_int), ("#666666", "?"))

# State-change rows (stage or flags changed)
state_changes = df.index[
    (df["chargeStageDisplay"] != df["chargeStageDisplay"].shift()) |
    (df["flags"] != df["flags"].shift())
].tolist()
if state_changes and state_changes[0] == 0:
    state_changes = state_changes[1:]

# ---------------------------------------------------------------------------
# 4. Reusable drawing helpers
# ---------------------------------------------------------------------------
GRID_KW = dict(alpha=0.2, linewidth=0.5)

plt.style.use("dark_background")


def draw_state_strip(ax, df, state_changes):
    """Color-coded stage bar + voltage loop tick marks + event vlines."""
    ax.set_ylim(0, 3)
    ax.set_yticks([])
    ax.set_xlim(df["t_plot"].iloc[0], df["t_plot"].iloc[-1])
    ax.set_xlabel(time_label, fontsize=13)

    span = df["t_plot"].iloc[-1] - df["t_plot"].iloc[0]

    # Stage color bar
    boundaries = state_changes + [len(df) - 1]
    prev_t   = df["t_plot"].iloc[0]
    prev_row = df.iloc[0]

    for idx in boundaries:
        t = df.loc[idx, "t_plot"] if idx < len(df) else df["t_plot"].iloc[-1]
        color, label = stage_color_label(prev_row["chargeStageDisplay"])
        width = t - prev_t
        ax.barh(2, width, left=prev_t, height=0.75,
                color=color, alpha=0.85, align="center")
        if width > span * 0.03:
            ax.text(prev_t + width / 2, 2, label,
                    ha="center", va="center",
                    fontsize=8, color="white", fontweight="bold", clip_on=True)
        if idx < len(df):
            prev_row = df.loc[idx]
        prev_t = t

    # Voltage loop fired ticks (orange, bottom of strip)
    vloop_rows = df[df["voltageLoopRanThisTick"] == 1]
    for t in vloop_rows["t_plot"]:
        ax.axvline(x=t, ymin=0, ymax=0.33,
                   color=EV_COLOR_VLOOP, linewidth=1.2, alpha=0.85)

    if not vloop_rows.empty:
        ax.text(df["t_plot"].iloc[0] + span * 0.01, 0.45,
                "VLoop ▲",
                fontsize=8, color=EV_COLOR_VLOOP, va="center")

    ax.spines["top"].set_visible(False)
    ax.spines["right"].set_visible(False)
    ax.spines["left"].set_visible(False)


def add_event_vlines(ax, df):
    """
    Vertical event lines on a data axes:
      cyan  = enteringCV
      blue  = enteringTargetVoltageMode
    """
    for t in df.loc[df["enteringCV"] == 1, "t_plot"]:
        ax.axvline(x=t, color=EV_COLOR_CV, linewidth=1.2,
                   linestyle="--", alpha=0.8, label="_nolegend_")

    for t in df.loc[df["enteringTargetVoltageMode"] == 1, "t_plot"]:
        ax.axvline(x=t, color=EV_COLOR_TVM, linewidth=1.4,
                   linestyle="--", alpha=0.9, label="_nolegend_")


def add_mode_vlines(ax, df, state_changes):
    """Dashed vlines at stage transitions."""
    for idx in state_changes:
        t = df.loc[idx, "t_plot"]
        color, _ = stage_color_label(df.loc[idx, "chargeStageDisplay"])
        ax.axvline(x=t, color=color, linewidth=1.0,
                   linestyle="--", alpha=0.5)


def save_fig(fig, suffix):
    out = os.path.join(DOWNLOADS, f"{basename}_{suffix}.png")
    fig.savefig(out, dpi=150, bbox_inches="tight")
    print(f"Saved: {out}")


def make_fig(title_suffix, num):
    fig = plt.figure(figsize=(18, 8), num=num)
    gs  = gridspec.GridSpec(2, 1, height_ratios=[5, 1], hspace=0.08)
    ax  = fig.add_subplot(gs[0])
    axs = fig.add_subplot(gs[1], sharex=ax)
    plt.setp(ax.get_xticklabels(), visible=False)
    fig.suptitle(f"{title_suffix}  |  {inner_label}", fontsize=14)
    return fig, ax, axs


# ---------------------------------------------------------------------------
# PLOT 1 — Voltage: what the outer loop sees
#
# Diagnoses: is battV oscillating around ChargingVoltageTarget?
#            What is the amplitude and sign of vError?
#            Does vError lead or lag the amps/duty changes in Plot 4?
# ---------------------------------------------------------------------------
fig1, ax1, ax1s = make_fig("Plot 1 — Voltage Loop Input", "Plot 1 — Voltage")
ax1b = ax1.twinx()

ax1.plot(df["t_plot"], df["battV"],
         color="#00bcd4", lw=2.0, label="battV")
ax1.plot(df["t_plot"], df["ChargingVoltageTarget"],
         color="#ffb300", lw=1.8, linestyle="--", label="ChargingVoltageTarget")

ax1b.plot(df["t_plot"], df["vError"],
          color="#ef5350", lw=1.6, label="vError", alpha=0.9)
ax1b.axhline(0, color="#ef5350", linewidth=0.6, linestyle=":", alpha=0.4)

ax1.set_ylabel("Voltage (V)", color="#00bcd4")
ax1b.set_ylabel("vError (V)", color="#ef5350")
ax1.grid(**GRID_KW)

lines1  = ax1.get_lines() + ax1b.get_lines()
labels1 = [l.get_label() for l in lines1 if not l.get_label().startswith("_")]
ax1.legend(lines1[:len(labels1)], labels1, loc="upper left")

add_event_vlines(ax1, df)
add_mode_vlines(ax1, df, state_changes)
draw_state_strip(ax1s, df, state_changes)

# Annotate enteringCV and enteringTargetVoltageMode at top of strip
for t in df.loc[df["enteringCV"] == 1, "t_plot"]:
    ax1s.axvline(x=t, color=EV_COLOR_CV, linewidth=1.2,
                 linestyle="--", alpha=0.8)
for t in df.loc[df["enteringTargetVoltageMode"] == 1, "t_plot"]:
    ax1s.axvline(x=t, color=EV_COLOR_TVM, linewidth=1.4,
                 linestyle="--", alpha=0.9)

# save_fig(fig1, "plot1_voltage")
# ---------------------------------------------------------------------------
# PLOT 2 — CV outer loop
#
# Diagnoses: is Icv tracking the voltage target smoothly?
#            Is cv_I (integrator) stable or drifting at steady state?
#            Is tableThermalLimit (RPM/thermal ceiling, pre-OV cap) constraining Icv?
#            Does uTargetAmps (post-OV cap) diverge from tableThermalLimit?
#            Is setpointCmd following Icv in CV or switching to
#            tableThermalLimit in bulk?
#            Orange vlines show when the CV loop interval actually fired.
# ---------------------------------------------------------------------------
fig2, ax2, ax2s = make_fig("Plot 2 — CV Outer Loop (Icv, cv_I, limits)  [NOTE: post-OV-cap uTargetAmps not logged]", "Plot 2 — Outer Loop")
ax2b = ax2.twinx()

ax2.plot(df["t_plot"], df["tableThermalLimit"],
         color="#42a5f5", lw=2.0, label="tableThermalLimit (RPM/thermal ceiling — pre-OV cap)")
ax2.plot(df["t_plot"], df["setpointCmd"],
         color="#00c853", lw=2.0, label="setpointCmd (=Icv in CV, =tableThermalLimit in bulk)")
ax2.plot(df["t_plot"], df["Icv"],
         color="#ffb300", lw=1.8, linestyle="--", label="Icv (CV setpoint)")

ax2b.plot(df["t_plot"], df["cv_I"],
          color="#ce93d8", lw=1.6, label="cv_I (integrator state, A)", alpha=0.85)
ax2b.set_ylabel("cv_I (A) — integrator state", color="#ce93d8")
ax2.grid(**GRID_KW)

lines2  = ax2.get_lines() + ax2b.get_lines()
labels2 = [l.get_label() for l in lines2 if not l.get_label().startswith("_")]
ax2.legend(lines2[:len(labels2)], labels2, loc="upper left")

add_event_vlines(ax2, df)
add_mode_vlines(ax2, df, state_changes)
draw_state_strip(ax2s, df, state_changes)

# Mark ticks where outer loop ran — dashed line to data area
for t in df.loc[df["voltageLoopRanThisTick"] == 1, "t_plot"]:
    ax2.axvline(x=t, color=EV_COLOR_VLOOP, linewidth=0.7, alpha=0.4)

# save_fig(fig2, "plot2_outer_loop")

# ---------------------------------------------------------------------------
# PLOT 3 — Inner PID internals
#
# Diagnoses: is the inner loop being given a stable setpoint?
#            Is it saturating (pidUnsatOutput >> pidOutput)?
#            Which term (P/I/D) is dominating during oscillation?
#            Does integrator windup survive the saturation?
# ---------------------------------------------------------------------------
fig3 = plt.figure(figsize=(18, 10), num="Plot 3 — Inner PID")
gs3  = gridspec.GridSpec(3, 1, height_ratios=[3, 2, 1], hspace=0.10)
ax3a = fig3.add_subplot(gs3[0])          # setpoint tracking
ax3b = fig3.add_subplot(gs3[1], sharex=ax3a)  # PID term decomp
ax3s = fig3.add_subplot(gs3[2], sharex=ax3a)  # state strip

plt.setp(ax3a.get_xticklabels(), visible=False)
plt.setp(ax3b.get_xticklabels(), visible=False)
fig3.suptitle(f"Plot 3 — Inner PID Internals  |  {inner_label}", fontsize=14)

# 3a: setpoint vs input, output vs unsat output
ax3a.plot(df["t_plot"], df["pidSetpoint"],
          color="#ffb300", lw=2.0, linestyle="--", label="pidSetpoint")
ax3a.plot(df["t_plot"], df["pidInput"],
          color="#42a5f5", lw=1.8, label="pidInput (measAmps)")
ax3a.plot(df["t_plot"], df["pidOutput"],
          color="#00c853", lw=1.8, label="pidOutput (→ dutyReq)")
ax3a.plot(df["t_plot"], df["pidUnsatOutput"],
          color="#ef5350", lw=1.4, linestyle=":", label="pidUnsatOutput", alpha=0.85)

# Shade saturation region — gap between pidUnsatOutput and pidOutput
ax3a.fill_between(df["t_plot"], df["pidOutput"], df["pidUnsatOutput"],
                  where=(df["pid_saturation"] > 0.1),
                  color="#ef5350", alpha=0.18, label="saturation zone")

ax3a.set_ylabel("Amps / Duty %")
ax3a.grid(**GRID_KW)
ax3a.legend(loc="upper left")

# 3b: term decomposition
ax3b.plot(df["t_plot"], df["innerTermP"],
          color="#42a5f5", lw=1.8, label="innerTermP")
ax3b.plot(df["t_plot"], df["innerTermI"],
          color="#ffb300", lw=1.8, label="innerTermI")
ax3b.plot(df["t_plot"], df["innerTermD"],
          color="#7e57c2", lw=1.6, label="innerTermD")
ax3b.axhline(0, color="#ffffff", linewidth=0.5, alpha=0.3)
ax3b.set_ylabel("PID Term Value")
ax3b.grid(**GRID_KW)
ax3b.legend(loc="upper left")

add_event_vlines(ax3a, df)
add_event_vlines(ax3b, df)
add_mode_vlines(ax3a, df, state_changes)
add_mode_vlines(ax3b, df, state_changes)
draw_state_strip(ax3s, df, state_changes)

# save_fig(fig3, "plot3_inner_pid")

# ---------------------------------------------------------------------------
# PLOT 4 — Duty pipeline + actual amps
#
# Diagnoses: is the governor absorbing/masking PID output via slew limiting?
#            (dutyRequest != dutyApplied means governor clipped it)
#            Does measured amps track duty proportionally?
#            Is the system oscillating in amps despite stable duty?
#            (if yes → suspect load or voltage measurement issue upstream)
# ---------------------------------------------------------------------------
fig4, ax4, ax4s = make_fig("Plot 4 — Duty Pipeline & Measured Amps", "Plot 4 — Duty")
ax4b = ax4.twinx()

ax4.plot(df["t_plot"], df["dutyRequest"],
         color="#ffb300", lw=1.8, linestyle="--", label="dutyRequest")
ax4.plot(df["t_plot"], df["dutyApplied"],
         color="#00c853", lw=2.0, label="dutyApplied")

# Shade where governor is clipping (dutyRequest != dutyApplied)
ax4.fill_between(df["t_plot"], df["dutyRequest"], df["dutyApplied"],
                 where=(df["duty_clamp"] > 0.5),
                 color="#ff9800", alpha=0.22, label="governor clip zone")

ax4b.plot(df["t_plot"], df["measAmps"],
          color="#ef5350", lw=1.8, label="measAmps", alpha=0.9)

ax4.set_ylabel("Duty (%)")
ax4b.set_ylabel("Measured Amps (A)", color="#ef5350")
ax4.grid(**GRID_KW)

lines4  = ax4.get_lines() + ax4b.get_lines()
labels4 = [l.get_label() for l in lines4 if not l.get_label().startswith("_")]
ax4.legend(lines4[:len(labels4)], labels4, loc="upper left")

add_event_vlines(ax4, df)
add_mode_vlines(ax4, df, state_changes)
draw_state_strip(ax4s, df, state_changes)

# save_fig(fig4, "plot4_duty")

# ---------------------------------------------------------------------------
plt.show()