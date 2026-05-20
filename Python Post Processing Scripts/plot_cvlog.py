"""
plot_cvlog.py
Diagnostic plotter for CVlog data from the ESP32 alternator regulator.

3 plot windows:
  Plot 1 — Voltage: battV, battV_filt_V, targV, vPred | duty% right axis
  Plot 2 — Current command chain (top) + overvoltage protection layers (bottom)
  Plot 3 — Engine RPM + CH1 scheduling jitter | duty% right axis

State strip (below each plot):
  - cvActive bar (green when CV active, grey otherwise)
  - fastOvActive overlay (red when FastOV or iExcess active; load dump has its own track)
  - Tick marks: voltLoopFired (pink), hardClamp (purple),
                iExcess (teal), loadDumpActive (orange)

File picker searches ~/Downloads for *.csv, newest first.
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
from matplotlib.widgets import CheckButtons, TextBox, Button as MplButton
from matplotlib.lines import Line2D
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
    "legend.handlelength": 3.5,
    "legend.handleheight": 1.5,
})

# ---------------------------------------------------------------------------
# 1. File selector
# ---------------------------------------------------------------------------
def pick_file():
    files = sorted(
        glob.glob(os.path.join(DOWNLOADS, "*.csv")),
        key=os.path.getmtime,
        reverse=True
    )
    if not files:
        messagebox.showerror("No files", f"No *.csv found in {DOWNLOADS}")
        return None

    selected = []

    root = tk.Tk()
    try:  # force light appearance on macOS regardless of system dark-mode setting
        root.tk.call("::tk::unsupported::MacWindowStyle", "appearance",
                     root._w, "NSAppearanceNameAqua")
    except Exception:
        pass
    root.title("Select CV Log")
    root.resizable(False, False)
    root.configure(bg="#f5f5f5")

    tk.Label(
        root,
        text="Select a log file:",
        font=("Helvetica", 16, "bold"),
        bg="#f5f5f5",
        fg="#1a1a1a"
    ).pack(padx=20, pady=(16, 8))

    frame = tk.Frame(root, bg="#f5f5f5")
    frame.pack(padx=20, pady=8)

    scrollbar = tk.Scrollbar(frame, orient=tk.VERTICAL)
    listbox = tk.Listbox(
        frame,
        yscrollcommand=scrollbar.set,
        width=70,
        height=min(len(files), 16),
        font=("Courier", 15),
        selectmode=tk.SINGLE,
        bg="#ffffff",
        fg="#1a1a1a",
        selectbackground="#1565c0",
        selectforeground="#ffffff",
        highlightbackground="#cccccc",
        highlightcolor="#1565c0"
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

    btn_frame = tk.Frame(root, bg="#f5f5f5")
    btn_frame.pack(pady=16)

    # tk.Label used instead of tk.Button — macOS ignores bg/fg on native buttons
    _lbl_open = tk.Label(
        btn_frame, text="Open",
        font=("Helvetica", 15, "bold"),
        bg="#1565c0", fg="#ffffff",
        relief=tk.SOLID, bd=1, padx=18, pady=8, width=10, cursor="hand2"
    )
    _lbl_open.bind("<Button-1>", lambda e: on_go())
    _lbl_open.pack(side=tk.LEFT, padx=10)

    _lbl_cancel = tk.Label(
        btn_frame, text="Cancel",
        font=("Helvetica", 14),
        bg="#ffffff", fg="#1a1a1a",
        relief=tk.SOLID, bd=1, padx=18, pady=8, width=10, cursor="hand2"
    )
    _lbl_cancel.bind("<Button-1>", lambda e: on_cancel())
    _lbl_cancel.pack(side=tk.LEFT, padx=10)

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

# Robust header search — accepts both old format (t_s, comma-sep) and new (ts_ms, tab-sep).
# Some firmware builds write the last comment and the header on the same line (missing newline),
# e.g. "# cv_I: ... PI ints_ms,chargeStageDisplay,...". Detect by presence of both "battV"
# and "ts_ms"/"t_s", then repair the garbled first column after splitting.
with open(path, encoding="utf-8", errors="replace") as _f:
    _lines = _f.readlines()

_header_idx  = None
_header_line = None
for _i, _raw in enumerate(_lines):
    if "battV" in _raw and ("ts_ms" in _raw or "t_s" in _raw):
        _header_idx  = _i
        _header_line = _raw.strip()
        break

if _header_idx is None:
    raise SystemExit(
        f"ERROR: No line containing a time column and 'battV' found in {path}.\n"
        "Check that the CVlog header was written correctly by the firmware."
    )

# Auto-detect delimiter
_sep = "\t" if "\t" in _header_line else ","
_col_names = [c.strip() for c in _header_line.split(_sep)]

# Repair garbled first column: firmware sometimes omits the newline between the last
# comment line and the header, so "...PI int" + "s_ms,col1,..." becomes "...PI ints_ms,col1,...".
# The first field ends up as e.g. "# cv_I: CV position-form PI ints_ms" instead of "ts_ms".
if _col_names:
    if _col_names[0] != "ts_ms" and "ts_ms" in _col_names[0]:
        _col_names[0] = "ts_ms"
    elif _col_names[0] != "t_s" and _col_names[0].endswith("t_s"):
        _col_names[0] = "t_s"

print(f"Header found at file line {_header_idx} (sep={'TAB' if _sep == chr(9) else 'comma'}): {_header_line[:80]}...")
print(f"Columns ({len(_col_names)}): {_col_names}")

from io import StringIO
_data_text = "".join(_lines[_header_idx + 1:])
df = pd.read_csv(StringIO(_data_text), names=_col_names, sep=_sep, on_bad_lines="skip")

# ---------------------------------------------------------------------------
# Normalise new log format → names the rest of the script expects.
# New format is tab-separated with different column names and packed bitfields.
# ---------------------------------------------------------------------------
_rename_map = {
    # time
    "ts_ms":                   "ts_ms_raw",
    # voltage
    "ChargingVoltageTarget":   "targV",
    "vError":                  "vError_V",
    # current
    "Icv":                     "Icv_A",
    "cv_I":                    "cv_I_A",
    "tableThermalLimit":       "uTarget_A",
    "setpointCmd":             "spLimited_A",
    "measAmps":                "iMeas_A",
    # duty
    "dutyApplied":             "duty_pct",
    # loop event
    "voltageLoopRanThisTick":  "voltLoopFired",
    # misc
    "dBcur_dt":                "dBcur_dt_Aps",
    "battI":                   "battI_A",
}
df.rename(columns={k: v for k, v in _rename_map.items() if k in df.columns}, inplace=True)

# Derive t_s (seconds) from ts_ms_raw if present; old format already has t_s
if "ts_ms_raw" in df.columns:
    df["t_s"] = pd.to_numeric(df["ts_ms_raw"], errors="coerce") / 1000.0
elif "t_s" not in df.columns:
    raise SystemExit(f"ERROR: no time column found in parsed columns: {list(df.columns)}")

# Unpack ovFlags bitmask → individual flag columns (new format)
if "ovFlags" in df.columns:
    _ov = pd.to_numeric(df["ovFlags"], errors="coerce").fillna(0).astype("int64")
    df["fastOvActive"]   = (_ov & 1)
    # bit 1 reserved (was softClamp — Layer 1 removed)
    df["hardClamp"]      = ((_ov & 4) > 0).astype("int64")
    df["iExcess"]        = ((_ov & 8) > 0).astype("int64")
    df["loadDumpActive"] = ((_ov & 16) > 0).astype("int64")

# Unpack flags bitmask → cvActive (bit1 = voltCtrl = CV loop running)
if "flags" in df.columns and "cvActive" not in df.columns:
    _fl = pd.to_numeric(df["flags"], errors="coerce").fillna(0).astype("int64")
    df["cvActive"] = ((_fl & 2) > 0).astype("int64")

# ---------------------------------------------------------------------------

if "t_s" not in df.columns:
    raise SystemExit(f"ERROR: 't_s' not in parsed columns: {list(df.columns)}")

numeric_cols = [
    "t_s",
    "battV", "targV", "vError_V", "dvdt_Vs", "vPred",
    "fastOvCap_A", "cv_I_A", "Icv_A", "uTarget_A", "spLimited_A",
    "iMeas_A", "duty_pct",
    "fastOvActive", "voltLoopFired", "cvActive",
    "hardClamp",
    "rpm",
    "battV_filt_V", "iMeas_filt_A",
    "ch1_last_ms", "iExcess",
    "battI_A", "dBcur_dt_Aps", "loadDumpActive",
    "cvDSlope_Vps", "awState",
    "voltLoopInterval_ms", "inaInterval_ms",
    "slopeBleedAmps_A",
]

for col in numeric_cols:
    if col in df.columns:
        df[col] = pd.to_numeric(df[col], errors="coerce")

df.dropna(subset=["t_s"], inplace=True)
df.reset_index(drop=True, inplace=True)

# t_s is already in seconds
df["t_s"] = df["t_s"] - df["t_s"].iloc[0]   # zero-reference

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

# Integer / flag columns
def _to_int(col):
    if col not in df.columns:
        return pd.Series(0, index=df.index)
    return pd.to_numeric(df[col], errors="coerce").fillna(0).astype(int)

df["fastOvActive"]   = _to_int("fastOvActive")
df["voltLoopFired"]  = _to_int("voltLoopFired")
df["cvActive"]       = _to_int("cvActive")
df["hardClamp"]      = _to_int("hardClamp")
df["iExcess"]        = _to_int("iExcess")
df["loadDumpActive"] = _to_int("loadDumpActive")

# Extract voltage loop Kp/Ki (D term removed — VoltageKd was always 0, tombstoned in log).
# New log format: per-row voltageKp/Ki columns hold the actual outer voltage loop gains.
# Old log format: only gainKp/Ki/Kd existed — those are INNER current-loop gains (PidKp/Ki/Kd),
#   NOT the voltage loop gains. Do not use them as voltage loop gains.
import re
_kp = float("nan")
_ki = float("nan")
# Primary: new per-row voltage loop columns
if "voltageKp" in df.columns:
    _v = pd.to_numeric(df["voltageKp"], errors="coerce").dropna()
    if len(_v): _kp = float(_v.iloc[0])
if "voltageKi" in df.columns:
    _v = pd.to_numeric(df["voltageKi"], errors="coerce").dropna()
    if len(_v): _ki = float(_v.iloc[0])
# Legacy: header comment with VoltageKp= (never produced by firmware, kept for future use)
if np.isnan(_kp):
    for _raw in _lines[:_header_idx]:
        if "VoltageKp" in _raw:
            m = re.search(r"VoltageKp=([\d.]+)", _raw)
            if m: _kp = float(m.group(1))
            m = re.search(r"VoltageKi=([\d.]+)", _raw)
            if m: _ki = float(m.group(1))
            break
# Old-format log warning: gainKp/Ki/Kd = inner current-loop gains, not voltage loop gains
if np.isnan(_kp) and ("gainKp" in df.columns or "innerKp" in df.columns):
    print("WARNING: old log format — voltage loop gains unavailable. "
          "gainKp/Ki/Kd (if present) are inner current-loop gains, not VoltageKp/Ki/Kd.")

outer_label = f"VoltageKp={_kp:.4g}  VoltageKi={_ki:.4g}"
print(f"Voltage loop gains: {outer_label}")

# Extract SlopeBleed constants from the header comment line emitted by cvBinToCsv.
# Format: "# SlopeBleed: SlopeBleedThresh=0.100V/s SlopeBleedK=50.0A/(V/s) SlopeBleedProxV=0.150V"
_sb_thresh = float("nan")
_sb_k      = float("nan")
_sb_proxv  = float("nan")
for _raw in _lines[:_header_idx]:
    if "SlopeBleed" in _raw and "SlopeBleedThresh" in _raw:
        m = re.search(r"SlopeBleedThresh=([\d.]+)", _raw)
        if m: _sb_thresh = float(m.group(1))
        m = re.search(r"SlopeBleedK=([\d.]+)", _raw)
        if m: _sb_k = float(m.group(1))
        m = re.search(r"SlopeBleedProxV=([\d.]+)", _raw)
        if m: _sb_proxv = float(m.group(1))
        break
_sb_label = (
    f"SlopeBleedThresh={_sb_thresh:.3g}V/s  "
    f"SlopeBleedK={_sb_k:.4g}A/(V/s)  "
    f"SlopeBleedProxV={_sb_proxv:.3g}V"
    if not np.isnan(_sb_thresh) else "SlopeBleed params not in header (older log)"
)
print(f"Slope bleed params: {_sb_label}")

# ---------------------------------------------------------------------------
# 3. Shared drawing helpers
# ---------------------------------------------------------------------------
GRID_KW    = dict(alpha=0.4, linewidth=0.7)
DUTY_COLOR = "#78909c"   # field duty % line — neutral grey-blue on all plots

EV_COLOR_VLOOP = "#aeea00"   # voltLoopFired ticks  (lime)
EV_COLOR_HARD  = "#6a1b9a"   # hardClamp ticks      (purple)
EV_COLOR_FAST  = "#00838f"   # iExcess ticks        (teal)
EV_COLOR_LDUMP = "#f57c00"   # loadDumpActive ticks (orange)

_checkbox_refs = []  # keep CheckButtons alive — GC drops them without this

def _make_checkbox_panel(fig, lines):
    """Add a CheckButtons panel on the right margin to toggle line visibility."""
    lines = [l for l in lines if not l.get_label().startswith("_")]
    if not lines:
        return None
    labels = [l.get_label() for l in lines]
    n = len(labels)
    panel_h = min(0.85, max(0.12, n * 0.10))
    y0 = max(0.05, 0.52 - panel_h / 2)
    ax_cb = fig.add_axes([0.82, y0, 0.16, panel_h])
    ax_cb.set_frame_on(False)
    check = CheckButtons(ax_cb, labels, [True] * n)
    for lbl_obj, line in zip(check.labels, lines):
        lbl_obj.set_color(line.get_color())
        lbl_obj.set_fontsize(8)
    def toggle(label):
        for line in lines:
            if line.get_label() == label:
                line.set_visible(not line.get_visible())
        fig.canvas.draw_idle()
    check.on_clicked(toggle)
    _checkbox_refs.append(check)
    return check


def draw_state_strip(ax, df):
    ax.set_ylim(0, 1)
    ax.set_yticks([])
    ax.set_xlim(df["t_plot"].iloc[0], df["t_plot"].iloc[-1])
    ax.set_xlabel(time_label, fontsize=13)

    span = df["t_plot"].iloc[-1] - df["t_plot"].iloc[0]

    # cvActive background bar — fills almost the full strip height
    prev_t   = df["t_plot"].iloc[0]
    prev_val = df["cvActive"].iloc[0]
    for i in range(1, len(df)):
        val = df["cvActive"].iloc[i]
        if val != prev_val or i == len(df) - 1:
            t     = df["t_plot"].iloc[i]
            color = "#2e7d32" if prev_val else "#cccccc"
            width = t - prev_t
            ax.barh(0.5, width, left=prev_t, height=0.82,
                    color=color, alpha=0.85, align="center")
            if prev_val and width > span * 0.04:
                ax.text(prev_t + width / 2, 0.5, "CV",
                        ha="center", va="center",
                        fontsize=8, color="white", fontweight="bold", clip_on=True)
            prev_t  = t
            prev_val = val

    # voltLoopFired ticks — span full bar height
    for t in df.loc[df["voltLoopFired"] == 1, "t_plot"]:
        ax.axvline(x=t, ymin=0.09, ymax=0.91, color=EV_COLOR_VLOOP, linewidth=1.0, alpha=0.9)

    # Legend
    ax.text(df["t_plot"].iloc[0] + span * 0.005, 0.88, "cvActive", fontsize=7, color="#1b5e20", va="center")
    ax.text(df["t_plot"].iloc[0] + span * 0.005, 0.12, "VLoop▲",   fontsize=7, color=EV_COLOR_VLOOP, va="center")

    ax.spines["top"].set_visible(False)
    ax.spines["right"].set_visible(False)
    ax.spines["left"].set_visible(False)


def add_ov_shading(ax, df):
    """Light red background fill where fastOvActive=1."""
    in_ov    = False
    ov_start = None
    for i, row in df.iterrows():
        if row["fastOvActive"] and not in_ov:
            ov_start = row["t_plot"]
            in_ov    = True
        elif not row["fastOvActive"] and in_ov:
            ax.axvspan(ov_start, row["t_plot"], color="#c62828", alpha=0.08)
            in_ov = False
    if in_ov:
        ax.axvspan(ov_start, df["t_plot"].iloc[-1], color="#c62828", alpha=0.08)


def add_voltloop_vlines(ax, df):
    """Faint pink vlines where voltage loop fired."""
    for t in df.loc[df["voltLoopFired"] == 1, "t_plot"]:
        ax.axvline(x=t, color=EV_COLOR_VLOOP, linewidth=0.6, alpha=0.35)


def add_duty_axis(ax):
    """Add field duty % as a secondary right axis. Returns the twin axis."""
    ax_d = ax.twinx()
    if "duty_pct" in df.columns:
        ax_d.plot(df["t_plot"], df["duty_pct"],
                  color=DUTY_COLOR, lw=1.3, alpha=0.55, linestyle="-.",
                  label="Field duty (%)")
    ax_d.set_ylabel("Field duty (%)", color=DUTY_COLOR, fontsize=12)
    ax_d.set_ylim(0, 115)
    ax_d.tick_params(axis="y", colors=DUTY_COLOR, labelsize=11)
    return ax_d


def add_subtitle(fig, text, y=0.955):
    """Small italic subtitle below suptitle."""
    fig.text(0.5, y, text, ha="center", va="top",
             fontsize=9.5, color="#555555", style="italic")


def save_fig(fig, suffix):
    out = os.path.join(DOWNLOADS, f"{basename}_{suffix}.png")
    fig.savefig(out, dpi=150, bbox_inches="tight")
    print(f"Saved: {out}")


def draw_flag_bars(ax, df):
    """Mode bar + duration bars + VLoop ticks — replaces state strip on all plots."""
    flag_h  = 0.18
    spacing = 0.26
    n_rows  = 6   # cvActive + voltLoopFired + 4 protection flags
    top     = (n_rows - 1) * spacing + flag_h + 0.06

    span = df["t_plot"].iloc[-1] - df["t_plot"].iloc[0]

    # --- Row 5 (top): cvActive mode bar — green/grey segments ---
    cv_offset = 5 * spacing
    prev_t   = df["t_plot"].iloc[0]
    prev_val = df["cvActive"].iloc[0]
    for i in range(1, len(df)):
        val = df["cvActive"].iloc[i]
        if val != prev_val or i == len(df) - 1:
            t     = df["t_plot"].iloc[i]
            color = "#2e7d32" if prev_val else "#aaaaaa"
            width = t - prev_t
            ax.barh(cv_offset + flag_h / 2, width, left=prev_t, height=flag_h,
                    color=color, alpha=0.85, align="center")
            prev_t   = t
            prev_val = val
    ax.text(df["t_plot"].iloc[0], cv_offset + flag_h / 2, "  cvActive (mode)",
            va="center", fontsize=7, color="#ffffff", fontweight="bold")

    # --- Row 4: voltLoopFired — tick marks (fires every loop tick) ---
    vl_offset = 4 * spacing
    for t in df.loc[df["voltLoopFired"] == 1, "t_plot"]:
        ax.axvline(x=t, ymin=vl_offset / top, ymax=(vl_offset + flag_h) / top,
                   color=EV_COLOR_VLOOP, linewidth=0.8, alpha=0.7)
    ax.text(df["t_plot"].iloc[0], vl_offset + flag_h / 2, "  voltLoopFired (voltage loop tick)",
            va="center", fontsize=7, color="#212121", fontweight="bold")

    # --- Rows 3–0: protection duration bars ---
    bar_rows = [
        (3 * spacing, "fastOvActive",   "#c62828", "fastOvActive  (FastOV or iExcess; load dump separate)"),
        (2 * spacing, "hardClamp",      "#6a1b9a", "hardClamp     (layer 2/3 — hard ceiling)"),
        (1 * spacing, "iExcess",        "#00838f", "iExcess       (current excess protection)"),
        (0 * spacing, "loadDumpActive", "#f57c00", "loadDumpActive (sudden load drop detected)"),
    ]
    for offset, col, color, label in bar_rows:
        vals = df[col].values
        t    = df["t_plot"].values
        in_flag    = False
        flag_start = None
        for i in range(len(vals)):
            if vals[i] and not in_flag:
                flag_start = t[i]
                in_flag    = True
            elif not vals[i] and in_flag:
                ax.barh(offset + flag_h / 2, t[i] - flag_start,
                        left=flag_start, height=flag_h,
                        color=color, alpha=0.80, align="center")
                in_flag = False
        if in_flag:
            ax.barh(offset + flag_h / 2, t[-1] - flag_start,
                    left=flag_start, height=flag_h,
                    color=color, alpha=0.80, align="center")
        ax.text(df["t_plot"].iloc[0], offset + flag_h / 2, f"  {label}",
                va="center", fontsize=7, color=color)

    ax.set_ylim(-0.03, top)
    ax.set_yticks([])
    ax.set_xlim(df["t_plot"].iloc[0], df["t_plot"].iloc[-1])
    ax.set_xlabel(time_label, fontsize=13)
    ax.set_ylabel("Events", fontsize=9)
    ax.spines["top"].set_visible(False)
    ax.spines["right"].set_visible(False)
    ax.spines["left"].set_visible(False)
    ax.grid(**GRID_KW)


# ---------------------------------------------------------------------------
# PLOT 1 — Voltage
# ---------------------------------------------------------------------------
fig1 = plt.figure(figsize=(18, 8), num="Plot 1 — Voltage")
gs1  = gridspec.GridSpec(2, 1, height_ratios=[5, 0.6], hspace=0.08)
ax1  = fig1.add_subplot(gs1[0])
ax1s = fig1.add_subplot(gs1[1], sharex=ax1)
plt.setp(ax1.get_xticklabels(), visible=False)
fig1.suptitle(f"Plot 1 — Voltage  |  {outer_label}", fontsize=14, y=0.99)
add_subtitle(fig1,
    "Was the voltage setpoint reached? Did the regulator approach it cleanly, "
    "or did it overshoot? vPred shows how far ahead the loop was looking.")
fig1.subplots_adjust(top=0.90, right=0.80, bottom=0.10)

ax1.plot(df["t_plot"], df["battV"],
         color="#1565c0", lw=2.5, label="battV (measured)")
ax1.plot(df["t_plot"], df["battV_filt_V"],
         color="#90caf9", lw=1.6, linestyle="--", alpha=0.85, label="battV_filt_V (EMA)")
ax1.plot(df["t_plot"], df["targV"],
         color="#e91e63", lw=2.2, linestyle="--", label="targV (setpoint)")
if "vPred" in df.columns:
    ax1.plot(df["t_plot"], df["vPred"],
             color="#00838f", lw=2.0, linestyle="-.", label="vPred (predicted)", alpha=0.85)
ax1.set_ylabel("Voltage (V)")
ax1.grid(**GRID_KW)
ax1_d = add_duty_axis(ax1)

_h1 = [l for l in ax1.get_lines() if not l.get_label().startswith("_")]
_h1_duty = [l for l in ax1_d.get_lines() if not l.get_label().startswith("_")]
_leg1 = ax1_d.legend(_h1 + _h1_duty, [l.get_label() for l in _h1 + _h1_duty], loc="upper left")
_leg1.set_draggable(True)
_cb1 = _make_checkbox_panel(fig1, _h1 + _h1_duty)

add_ov_shading(ax1, df)
add_voltloop_vlines(ax1, df)
draw_flag_bars(ax1s, df)
# save_fig(fig1, "plot1_voltage")


# ---------------------------------------------------------------------------
# PLOT 2 — Current command chain (top) + protection layers (bottom)
# ---------------------------------------------------------------------------
fig2 = plt.figure(figsize=(18, 12), num="Plot 2 — Command Chain & Protections")
gs2  = gridspec.GridSpec(2, 1, height_ratios=[4, 0.6], hspace=0.12)
ax2a = fig2.add_subplot(gs2[0])
ax2b = fig2.add_subplot(gs2[1], sharex=ax2a)
plt.setp(ax2a.get_xticklabels(), visible=False)
fig2.suptitle(f"Plot 2 — Current Command Chain & Protections  |  {outer_label}", fontsize=14, y=0.99)
add_subtitle(fig2,
    "Top: which limit is dominating the current setpoint, and did the alternator deliver it?  "
    "Bottom: when each overvoltage protection layer fired and for how long.")
fig2.subplots_adjust(top=0.92, right=0.80)

# --- 2a: command chain ---
if "uTarget_A" in df.columns:
    ax2a.plot(df["t_plot"], df["uTarget_A"],
              color="#1565c0", lw=2.5, label="Current ceiling after all caps  (uTarget_A)")
if "Icv_A" in df.columns:
    ax2a.plot(df["t_plot"], df["Icv_A"],
              color="#e91e63", lw=2.2, linestyle="--", label="CV setpoint  (Icv_A)")
if "fastOvCap_A" in df.columns:
    ax2a.plot(df["t_plot"], df["fastOvCap_A"],
              color="#455a64", lw=1.6, linestyle=":", label="FastOV voltage ceiling  (fastOvCap_A)", alpha=0.85)
if "spLimited_A" in df.columns:
    ax2a.plot(df["t_plot"], df["spLimited_A"],
              color="#2e7d32", lw=2.0, label="PID command  (spLimited_A)")
if "iMeas_A" in df.columns:
    ax2a.plot(df["t_plot"], df["iMeas_A"],
              color="#c62828", lw=2.0, label="Actual current  (iMeas_A)", alpha=0.90)
if "iMeas_filt_A" in df.columns:
    ax2a.plot(df["t_plot"], df["iMeas_filt_A"],
              color="#ef9a9a", lw=1.4, linestyle="--", label="Actual current EMA  (iMeas_filt_A)", alpha=0.75)
if "battI_A" in df.columns:
    ax2a.plot(df["t_plot"], df["battI_A"],
              color="#f9a825", lw=1.8, alpha=0.80, label="Battery current  (battI_A)")

ax2a.set_ylabel("Current (A)")
ax2a.grid(**GRID_KW)
ax2a.set_title("Command chain — which limit is lowest wins; field duty on right axis",
               fontsize=10, color="#444444", style="italic", pad=4)
ax2a_d = add_duty_axis(ax2a)

_h2a = [l for l in ax2a.get_lines() if not l.get_label().startswith("_")]
_h2a_duty = [l for l in ax2a_d.get_lines() if not l.get_label().startswith("_")]
_leg2a = ax2a_d.legend(_h2a + _h2a_duty, [l.get_label() for l in _h2a + _h2a_duty], loc="upper left", fontsize=11)
_leg2a.set_draggable(True)

add_ov_shading(ax2a, df)
add_voltloop_vlines(ax2a, df)

# Variable key table — maps legend nicknames to internal variable names; drag anywhere to reposition
_p2_key = (
    "  Current ceiling/all caps =  uTarget_A      (A)\n"
    "  CV setpoint              =  Icv_A          (A)\n"
    "  FastOV voltage ceiling   =  fastOvCap_A    (A)\n"
    "  PID command              =  spLimited_A    (A)\n"
    "  Actual current (raw)     =  iMeas_A        (A)\n"
    "  Actual current (EMA)     =  iMeas_filt_A   (A)\n"
    "  Field duty               =  duty_pct       (%)"
)
_key_text = fig2.text(
    0.01, 0.30, _p2_key,
    fontsize=8, family="monospace",
    va="top", ha="left",
    bbox=dict(boxstyle="round,pad=0.35", fc="#f9f9f9", ec="#cccccc", alpha=0.92),
    zorder=10,
)

_key_drag = {"on": False, "x": 0.0, "y": 0.0, "pos": (0.01, 0.30)}

def _kpress(ev):
    if ev.button != 1:
        return
    try:
        bb = _key_text.get_window_extent(fig2.canvas.get_renderer())
    except Exception:
        return
    if bb.contains(ev.x, ev.y):
        _key_drag["on"] = True
        _key_drag["x"] = ev.x
        _key_drag["y"] = ev.y
        _key_drag["pos"] = _key_text.get_position()

def _kmove(ev):
    if not _key_drag["on"]:
        return
    fw = fig2.get_figwidth() * fig2.dpi
    fh = fig2.get_figheight() * fig2.dpi
    nx = _key_drag["pos"][0] + (ev.x - _key_drag["x"]) / fw
    ny = _key_drag["pos"][1] + (ev.y - _key_drag["y"]) / fh
    _key_text.set_position((nx, ny))
    fig2.canvas.draw_idle()

def _krelease(ev):
    _key_drag["on"] = False

fig2.canvas.mpl_connect("button_press_event", _kpress)
fig2.canvas.mpl_connect("motion_notify_event", _kmove)
fig2.canvas.mpl_connect("button_release_event", _krelease)

# --- 2b: mode + protection events panel ---
draw_flag_bars(ax2b, df)

# Checkbox panel for ax2a lines only (battI_A now lives there)
_cb2 = _make_checkbox_panel(fig2, _h2a + _h2a_duty)

# save_fig(fig2, "plot2_command_chain_protections")


# ---------------------------------------------------------------------------
# PLOT 3 — Engine & scheduling context
# ---------------------------------------------------------------------------
fig3 = plt.figure(figsize=(18, 9), num="Plot 3 — RPM & Context")
gs3  = gridspec.GridSpec(3, 1, height_ratios=[2, 1.5, 0.6], hspace=0.10)
ax3  = fig3.add_subplot(gs3[0])
ax3b = fig3.add_subplot(gs3[1], sharex=ax3)
ax3s = fig3.add_subplot(gs3[2], sharex=ax3)
plt.setp(ax3.get_xticklabels(),  visible=False)
plt.setp(ax3b.get_xticklabels(), visible=False)
fig3.suptitle(f"Plot 3 — Engine & Scheduling Context  |  {outer_label}", fontsize=14, y=0.99)
add_subtitle(fig3,
    "RPM, ADC scheduling jitter, and field duty — the mechanical and timing backdrop "
    "for everything in Plots 1 and 2.")
fig3.subplots_adjust(top=0.90, right=0.80)

if "rpm" in df.columns:
    ax3.plot(df["t_plot"], df["rpm"],
             color="#f9a825", lw=2.2, label="RPM")
else:
    ax3.text(0.5, 0.5, "rpm not present in this log",
             ha="center", va="center", transform=ax3.transAxes,
             color="#888888", fontsize=13)
ax3.set_ylabel("RPM")
ax3.grid(**GRID_KW)
ax3_d = add_duty_axis(ax3)
_h3 = [l for l in ax3.get_lines() if not l.get_label().startswith("_")]
_h3_duty = [l for l in ax3_d.get_lines() if not l.get_label().startswith("_")]
_leg3 = ax3_d.legend(_h3 + _h3_duty, [l.get_label() for l in _h3 + _h3_duty], loc="upper left")
_leg3.set_draggable(True)
add_voltloop_vlines(ax3, df)

if "ch1_last_ms" in df.columns:
    ax3b.plot(df["t_plot"], df["ch1_last_ms"],
              color="#00838f", lw=1.8, label="ch1_last_ms (CH1)", zorder=2)
    ax3b.axhline(5,  color="#2e7d32", linewidth=0.8, linestyle=":", alpha=0.70, label="5 ms (CH1 nominal)")
    ax3b.axhline(15, color="#c62828", linewidth=0.8, linestyle=":", alpha=0.70,
                 label="15 ms (CH1 3× — stale reading risk)")
else:
    ax3b.text(0.5, 0.5, "ch1_last_ms not present in this log (older firmware)",
              ha="center", va="center", transform=ax3b.transAxes,
              color="#888888", fontsize=12)

if "voltLoopInterval_ms" in df.columns:
    vl_rows = df[df["voltLoopFired"] == 1].copy()
    if not vl_rows.empty:
        ax3b.scatter(vl_rows["t_plot"], vl_rows["voltLoopInterval_ms"],
                     s=18, color="#ef5350", zorder=4, label="voltLoop actual interval (fired ticks only)")
    ax3b.axhline(100, color="#ef5350", linewidth=0.8, linestyle=":",  alpha=0.70, label="100 ms (vLoop target)")
    ax3b.axhline(200, color="#b71c1c", linewidth=0.8, linestyle="--", alpha=0.70, label="200 ms (vLoop 2×)")

if "inaInterval_ms" in df.columns:
    ax3b.plot(df["t_plot"], df["inaInterval_ms"],
              color="#42a5f5", lw=1.0, alpha=0.8, label="inaInterval_ms (INA228)", zorder=1)
    ax3b.axhline(10, color="#1565c0", linewidth=0.8, linestyle=":", alpha=0.70, label="10 ms (INA228 2×)")

ax3b.set_ylabel("Intervals (ms)")
ax3b.grid(**GRID_KW)
_h3b = [l for l in ax3b.get_lines() if not l.get_label().startswith("_")]
_leg3b = ax3b.legend(loc="upper right", fontsize=10)
_leg3b.set_draggable(True)

# One checkbox panel for both ax3 and ax3b lines
_cb3 = _make_checkbox_panel(fig3, _h3 + _h3_duty + _h3b)

draw_flag_bars(ax3s, df)
# save_fig(fig3, "plot3_rpm_context")


# ---------------------------------------------------------------------------
# PLOT 4 — CV PID term decomposition
#
# Reconstructs P, I, D contributions from logged signals + header gains.
# D term is subtracted in the firmware, so it appears as a negative contribution
# when voltage is rising. P + I − D should equal Icv_A (before clamping).
# ---------------------------------------------------------------------------
fig4 = plt.figure(figsize=(18, 9), num="Plot 4 — PID Term Decomposition")
gs4  = gridspec.GridSpec(3, 1, height_ratios=[4, 1.5, 0.6], hspace=0.10)
ax4  = fig4.add_subplot(gs4[0])
ax4b = fig4.add_subplot(gs4[1], sharex=ax4)
ax4s = fig4.add_subplot(gs4[2], sharex=ax4)
plt.setp(ax4.get_xticklabels(),  visible=False)
plt.setp(ax4b.get_xticklabels(), visible=False)
fig4.suptitle(f"Plot 4 — CV PID Term Decomposition  |  {outer_label}", fontsize=14, y=0.99)
add_subtitle(fig4,
    "What is the voltage loop actually doing? P reacts to filtered voltage error (targV − battV_filt_V), "
    "I holds the running correction, D backs off current as voltage rises. "
    "Sum of P + I − D should closely match Icv_A. "
    "SlopeBleed (orange scatter) shows per-tick cv_I drain on voltage loop ticks where slope exceeded threshold.")
fig4.subplots_adjust(top=0.90, right=0.80)

# Compute P term from header gains + logged signals.
# D term removed — VoltageKd was always 0 and is tombstoned in the log format.
# P term uses (targV - battV_filt_V): the firmware always uses raw IBV for the voltage error.
# voltageTargetSlewed (the slewed target) is not logged; targV is used as the target
# approximation — differs only briefly on CV entry or setpoint change.
if not np.isnan(_kp):
    df["pid_P"] = _kp * (df["targV"] - df["battV_filt_V"])
    _have_gains = True
else:
    _have_gains = False
    print("WARNING: VoltageKp not found in log header — P term cannot be computed")

# I term is cv_I_A directly (already Ki-scaled, in amps)
# Reconstructed total for sanity check
if _have_gains and "cv_I_A" in df.columns:
    df["pid_reconstructed"] = df["pid_P"] + df["cv_I_A"]

# --- Plot P, I, and total ---
if _have_gains:
    ax4.plot(df["t_plot"], df["pid_P"],
             color="#1565c0", lw=2.0, label=f"P term  =  Kp({_kp:.4g}) × (targV − battV_filt_V)  (A)")
if "cv_I_A" in df.columns:
    ax4.plot(df["t_plot"], df["cv_I_A"],
             color="#2e7d32", lw=2.0, label="I term  =  cv_I_A  (running integral, A)")
if "Icv_A" in df.columns:
    ax4.plot(df["t_plot"], df["Icv_A"],
             color="#c62828", lw=2.2, linestyle="-.",
             label="Icv_A  (total CV output — P + I, clamped)", alpha=0.90)
if "pid_reconstructed" in df.columns:
    ax4.plot(df["t_plot"], df["pid_reconstructed"],
             color="#888888", lw=1.2, linestyle=":",
             label="P + I  (reconstructed — should match Icv_A)", alpha=0.70)

# Slope bleed drain: scatter on voltage-loop ticks where bleed was non-zero.
# Units: A drained from cv_I that tick. Only meaningful on voltLoopFired rows.
if "slopeBleedAmps_A" in df.columns:
    _sb = df[(df["voltLoopFired"] == 1) & (df["slopeBleedAmps_A"] > 0)].copy()
    if not _sb.empty:
        ax4.scatter(_sb["t_plot"], _sb["slopeBleedAmps_A"],
                    s=28, color="#f57c00", zorder=5,
                    label="SlopeBleed drain (A per VL tick — actual cv_I drain)")
        ax4.axhline(0, color="#999999", linewidth=0.7, linestyle=":", alpha=0.5)
    else:
        ax4.axhline(0, color="#999999", linewidth=0.7, linestyle=":", alpha=0.5)
else:
    ax4.axhline(0, color="#999999", linewidth=0.7, linestyle=":", alpha=0.5)
ax4.set_ylabel("Current contribution (A)")
ax4.grid(**GRID_KW)
ax4_d = add_duty_axis(ax4)
_h4 = [l for l in ax4.get_lines() if not l.get_label().startswith("_")]
_h4_duty = [l for l in ax4_d.get_lines() if not l.get_label().startswith("_")]
_leg4 = ax4_d.legend(_h4 + _h4_duty, [l.get_label() for l in _h4 + _h4_duty], loc="upper left", fontsize=10)
_leg4.set_draggable(True)
add_ov_shading(ax4, df)
add_voltloop_vlines(ax4, df)

# vError context panel — shows the filtered error the P term actually reacted to.
# Uses (targV - battV_filt_V) to match what the firmware feeds to Kp.
df["filt_error_V"] = df["targV"] - df["battV_filt_V"]
ax4b.plot(df["t_plot"], df["filt_error_V"],
          color="#1565c0", lw=1.8, alpha=0.85, label="targV − battV_filt_V  (filtered error fed to P term)")
ax4b.plot(df["t_plot"], df["vError_V"],
          color="#90caf9", lw=1.2, linestyle="--", alpha=0.65, label="vError_V  (raw IBV, logged reference)")
ax4b.axhline(0, color="#999999", linewidth=0.7, linestyle=":", alpha=0.5)
ax4b.set_ylabel("vError (V)")

# cvDSlope on a right twin axis — V/s scale is very different from V error scale
if "cvDSlope_Vps" in df.columns:
    _ax4b_slope = ax4b.twinx()
    _ax4b_slope.plot(df["t_plot"], df["cvDSlope_Vps"],
                     color="#f57c00", lw=1.4, alpha=0.75, linestyle="-.",
                     label="cvDSlope  (V/s — slope bleed input)")
    if not np.isnan(_sb_thresh):
        _ax4b_slope.axhline(_sb_thresh, color="#f57c00", linewidth=0.8, linestyle=":",
                            alpha=0.65, label=f"SlopeBleedThresh ({_sb_thresh:.3g} V/s)")
    _ax4b_slope.set_ylabel("cvDSlope (V/s)", color="#f57c00", fontsize=11)
    _ax4b_slope.tick_params(axis="y", colors="#f57c00", labelsize=10)
    _h4b_slope = [l for l in _ax4b_slope.get_lines() if not l.get_label().startswith("_")]
else:
    _h4b_slope = []

ax4b.grid(**GRID_KW)
_h4b = [l for l in ax4b.get_lines() if not l.get_label().startswith("_")]
_leg4b = ax4b.legend(_h4b + _h4b_slope,
                     [l.get_label() for l in _h4b + _h4b_slope],
                     loc="upper left", fontsize=10)
_leg4b.set_draggable(True)

# One checkbox panel for ax4, ax4b, and slope twin lines
_cb4 = _make_checkbox_panel(fig4, _h4 + _h4_duty + _h4b + _h4b_slope)

draw_flag_bars(ax4s, df)
# save_fig(fig4, "plot4_pid_decomposition")


# ---------------------------------------------------------------------------
# Linked x-axis zoom — syncs all plot windows when any one is zoomed/panned.
# ---------------------------------------------------------------------------
_all_primary_axes = [ax1, ax2a, ax3, ax4]
_all_figs         = [fig1, fig2, fig3, fig4]
_syncing = [False]

def _on_xlim_changed(changed_ax):
    if _syncing[0]:
        return
    _syncing[0] = True
    try:
        new_xlim = changed_ax.get_xlim()
        for ax in _all_primary_axes:
            if ax is not changed_ax:
                ax.set_xlim(new_xlim)
        for fig in _all_figs:
            fig.canvas.draw_idle()
    finally:
        _syncing[0] = False

for _ax in _all_primary_axes:
    _ax.callbacks.connect("xlim_changed", _on_xlim_changed)

# ---------------------------------------------------------------------------
# File Trimmer
# ---------------------------------------------------------------------------
_trim_total_s = df["t_s"].iloc[-1]

def _fmt_trim_t(t):
    return f"{int(t)}" if t == int(t) else f"{t:.3f}".rstrip("0").rstrip(".")

def _do_trim(event=None):
    try:
        _s = float(_tb_trim_start.text)
        _e = float(_tb_trim_end.text)
    except ValueError:
        _trim_status_lbl.set_text("Bad input — start/end must be numbers")
        fig1.canvas.draw_idle()
        return
    if _s < 0 or _s >= _e or _e > _trim_total_s * 1.001:
        _trim_status_lbl.set_text(f"Range error  (file is 0 – {_trim_total_s:.1f} s)")
        fig1.canvas.draw_idle()
        return
    from io import StringIO as _TrimSI
    _tsep = "\t" if "\t" in _header_line else ","
    _trdf = pd.read_csv(
        _TrimSI("".join(_lines[_header_idx + 1:])),
        sep=_tsep, names=_col_names, on_bad_lines="skip"
    )
    _tcol = next((c for c in ("ts_ms", "t_s") if c in _trdf.columns), None)
    if _tcol is None:
        _trim_status_lbl.set_text("No time column found")
        fig1.canvas.draw_idle()
        return
    _trdf[_tcol] = pd.to_numeric(_trdf[_tcol], errors="coerce")
    _trdf.dropna(subset=[_tcol], inplace=True)
    _t_zero = _trdf[_tcol].iloc[0]
    _t_s_col = (_trdf[_tcol] - _t_zero) / (1000.0 if _tcol == "ts_ms" else 1.0)
    _tmask = (_t_s_col >= _s) & (_t_s_col <= _e)
    _tout  = _trdf[_tmask]
    if _tout.empty:
        _trim_status_lbl.set_text("No rows in that range")
        fig1.canvas.draw_idle()
        return
    _tname = f"{basename}_{_fmt_trim_t(_s)}s_{_fmt_trim_t(_e)}s.csv"
    _tpath = os.path.join(DOWNLOADS, _tname)
    with open(_tpath, "w", encoding="utf-8", newline="") as _tf:
        for _tln in _lines[:_header_idx]:
            _tf.write(_tln)
        _tf.write(_header_line + "\n")
        _tout.to_csv(_tf, index=False, header=False, columns=_col_names)
    _trim_status_lbl.set_text(f"Saved: {_tname}  ({len(_tout)} rows)")
    fig1.canvas.draw_idle()

_ax_trim_s   = fig1.add_axes([0.12, 0.018, 0.10, 0.05])
_ax_trim_e   = fig1.add_axes([0.28, 0.018, 0.10, 0.05])
_ax_trim_go  = fig1.add_axes([0.40, 0.018, 0.04, 0.05])
_tb_trim_start = TextBox(_ax_trim_s, "Start (s) ", initial="0")
_tb_trim_end   = TextBox(_ax_trim_e, "End (s) ",   initial=str(round(_trim_total_s, 1)))
_btn_trim_go   = MplButton(_ax_trim_go, "Go", color="#1565c0", hovercolor="#0d47a1")
_btn_trim_go.label.set_color("white")
_btn_trim_go.label.set_fontsize(10)
_btn_trim_go.on_clicked(_do_trim)
_trim_status_lbl = fig1.text(
    0.46, 0.043,
    f"File: {basename}  ({_trim_total_s:.1f} s)",
    fontsize=9, color="#555555", verticalalignment="center"
)
fig1.text(0.03, 0.043, "Trim file:", fontsize=10, color="#1a1a1a",
          fontweight="bold", verticalalignment="center")

# ---------------------------------------------------------------------------
print("Tip: use the checkboxes on the right of each plot to show/hide series.")
plt.show()
