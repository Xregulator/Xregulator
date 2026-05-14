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
  - Tick marks: voltLoopFired (pink), softClamp (yellow), hardClamp (purple),
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

# Robust header search
with open(path, encoding="utf-8", errors="replace") as _f:
    _lines = _f.readlines()

_header_idx  = None
_header_line = None
for _i, _raw in enumerate(_lines):
    if "t_s" in _raw and "battV" in _raw:
        _header_idx  = _i
        _header_line = _raw[_raw.find("t_s"):].strip()
        break

if _header_idx is None:
    raise SystemExit(
        f"ERROR: No line containing 't_s' and 'battV' found in {path}.\n"
        "Check that the CVlog header was written correctly by the firmware."
    )

print(f"Header found at file line {_header_idx}: {_header_line[:80]}...")

_col_names = [c.strip() for c in _header_line.split(",")]
print(f"Columns ({len(_col_names)}): {_col_names}")

from io import StringIO
_data_text = "".join(_lines[_header_idx + 1:])
df = pd.read_csv(StringIO(_data_text), names=_col_names, on_bad_lines="skip")

if "t_s" not in df.columns:
    raise SystemExit(f"ERROR: 't_s' not in parsed columns: {list(df.columns)}")

numeric_cols = [
    "t_s",
    "battV", "targV", "vError_V", "dvdt_Vs", "vPred",
    "fastOvCap_A", "cv_I_A", "Icv_A", "uTarget_A", "spLimited_A",
    "iMeas_A", "duty_pct",
    "fastOvActive", "voltLoopFired", "cvActive",
    "softClamp", "hardClamp",
    "rpm",
    "battV_filt_V", "iMeas_filt_A",
    "ch1_interval_ms", "iExcess",
    "battI_A", "dBcur_dt_Aps", "loadDumpActive",
    "cvDSlope_Vps", "awState",
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
df["softClamp"]      = _to_int("softClamp")
df["hardClamp"]      = _to_int("hardClamp")
df["iExcess"]        = _to_int("iExcess")
df["loadDumpActive"] = _to_int("loadDumpActive")

# Extract Kp/Ki/Kd from comment line in header if present
_kp = float("nan")
_ki = float("nan")
_kd = float("nan")
for _raw in _lines[:_header_idx]:
    if "VoltageKp" in _raw:
        import re
        m = re.search(r"VoltageKp=([\d.]+)", _raw)
        if m: _kp = float(m.group(1))
        m = re.search(r"VoltageKi=([\d.]+)", _raw)
        if m: _ki = float(m.group(1))
        m = re.search(r"VoltageKd=([\d.]+)", _raw)
        if m: _kd = float(m.group(1))
        break

outer_label = f"VoltageKp={_kp:.4g}  VoltageKi={_ki:.4g}  VoltageKd={_kd:.4g}"
print(f"Voltage loop gains: {outer_label}")

# ---------------------------------------------------------------------------
# 3. Shared drawing helpers
# ---------------------------------------------------------------------------
GRID_KW    = dict(alpha=0.4, linewidth=0.7)
DUTY_COLOR = "#78909c"   # field duty % line — neutral grey-blue on all plots

EV_COLOR_VLOOP = "#e91e63"   # voltLoopFired ticks  (hot pink)
EV_COLOR_SOFT  = "#ffeb3b"   # softClamp ticks      (bright yellow)
EV_COLOR_HARD  = "#6a1b9a"   # hardClamp ticks      (purple)
EV_COLOR_FAST  = "#00838f"   # iExcess ticks        (teal)
EV_COLOR_LDUMP = "#f57c00"   # loadDumpActive ticks (orange)


def draw_state_strip(ax, df):
    ax.set_ylim(0, 3)
    ax.set_yticks([])
    ax.set_xlim(df["t_plot"].iloc[0], df["t_plot"].iloc[-1])
    ax.set_xlabel(time_label, fontsize=13)

    span = df["t_plot"].iloc[-1] - df["t_plot"].iloc[0]

    # cvActive background bar
    prev_t   = df["t_plot"].iloc[0]
    prev_val = df["cvActive"].iloc[0]
    for i in range(1, len(df)):
        val = df["cvActive"].iloc[i]
        if val != prev_val or i == len(df) - 1:
            t     = df["t_plot"].iloc[i]
            color = "#2e7d32" if prev_val else "#cccccc"
            width = t - prev_t
            ax.barh(2, width, left=prev_t, height=0.75,
                    color=color, alpha=0.85, align="center")
            if prev_val and width > span * 0.04:
                ax.text(prev_t + width / 2, 2, "CV",
                        ha="center", va="center",
                        fontsize=8, color="white", fontweight="bold", clip_on=True)
            prev_t  = t
            prev_val = val

    # fastOvActive red overlay
    ov_rows = df[df["fastOvActive"] == 1]
    for t in ov_rows["t_plot"]:
        ax.axvspan(t - 0.002 * span, t + 0.002 * span,
                   ymin=0.5, ymax=1.0, color="#c62828", alpha=0.4)

    # Tick lanes
    for t in df.loc[df["voltLoopFired"] == 1, "t_plot"]:
        ax.axvline(x=t, ymin=0.65, ymax=0.78, color=EV_COLOR_VLOOP, linewidth=1.2, alpha=0.9)
    for t in df.loc[df["softClamp"] == 1, "t_plot"]:
        ax.axvline(x=t, ymin=0.49, ymax=0.62, color=EV_COLOR_SOFT,  linewidth=1.2, alpha=0.85)
    for t in df.loc[df["hardClamp"] == 1, "t_plot"]:
        ax.axvline(x=t, ymin=0.34, ymax=0.47, color=EV_COLOR_HARD,  linewidth=1.2, alpha=0.85)
    for t in df.loc[df["iExcess"] == 1, "t_plot"]:
        ax.axvline(x=t, ymin=0.19, ymax=0.32, color=EV_COLOR_FAST,  linewidth=1.2, alpha=0.85)
    for t in df.loc[df["loadDumpActive"] == 1, "t_plot"]:
        ax.axvline(x=t, ymin=0.05, ymax=0.17, color=EV_COLOR_LDUMP, linewidth=1.2, alpha=0.85)

    # Legend text
    ax.text(df["t_plot"].iloc[0] + span * 0.01, 2.55, "cvActive",    fontsize=7, color="#1b5e20", va="center")
    ax.text(df["t_plot"].iloc[0] + span * 0.01, 1.75, "VLoop▲",      fontsize=7, color=EV_COLOR_VLOOP, va="center")
    ax.text(df["t_plot"].iloc[0] + span * 0.01, 1.45, "softClamp▲",  fontsize=7, color=EV_COLOR_SOFT,  va="center")
    ax.text(df["t_plot"].iloc[0] + span * 0.01, 1.15, "hardClamp▲",  fontsize=7, color=EV_COLOR_HARD,  va="center")
    ax.text(df["t_plot"].iloc[0] + span * 0.01, 0.85, "iExcess▲",    fontsize=7, color=EV_COLOR_FAST,  va="center")
    ax.text(df["t_plot"].iloc[0] + span * 0.01, 0.55, "loadDump▲",   fontsize=7, color=EV_COLOR_LDUMP, va="center")

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
    """Filled horizontal duration bars for each binary protection flag."""
    flag_h = 0.75
    for offset, col, color, label in [
        (4.0, "fastOvActive",   "#c62828", "fastOvActive  (FastOV or iExcess; load dump separate)"),
        (3.0, "softClamp",      "#0277bd", "softClamp     (layer 1 — gentle ceiling)"),
        (2.0, "hardClamp",      "#6a1b9a", "hardClamp     (layer 2 — hard ceiling)"),
        (1.0, "iExcess",        "#00838f", "iExcess       (current excess protection)"),
        (0.0, "loadDumpActive", "#f57c00", "loadDumpActive (sudden load drop detected)"),
    ]:
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
                va="center", fontsize=9, color=color)

    ax.set_ylim(-0.2, 5.2)
    ax.set_yticks([])
    ax.set_ylabel("Protection layers")
    ax.grid(**GRID_KW)


# ---------------------------------------------------------------------------
# PLOT 1 — Voltage
# ---------------------------------------------------------------------------
fig1 = plt.figure(figsize=(18, 8), num="Plot 1 — Voltage")
gs1  = gridspec.GridSpec(2, 1, height_ratios=[5, 1], hspace=0.08)
ax1  = fig1.add_subplot(gs1[0])
ax1s = fig1.add_subplot(gs1[1], sharex=ax1)
plt.setp(ax1.get_xticklabels(), visible=False)
fig1.suptitle(f"Plot 1 — Voltage  |  {outer_label}", fontsize=14, y=0.99)
add_subtitle(fig1,
    "Was the voltage setpoint reached? Did the regulator approach it cleanly, "
    "or did it overshoot? vPred shows how far ahead the loop was looking.")
fig1.subplots_adjust(top=0.90)

ax1.plot(df["t_plot"], df["battV"],
         color="#1565c0", lw=2.5, label="battV (measured)")
ax1.plot(df["t_plot"], df["battV_filt_V"],
         color="#90caf9", lw=1.6, linestyle="--", alpha=0.85, label="battV_filt_V (EMA)")
ax1.plot(df["t_plot"], df["targV"],
         color="#e91e63", lw=2.2, linestyle="--", label="targV (setpoint)")
ax1.plot(df["t_plot"], df["vPred"],
         color="#00838f", lw=2.0, linestyle="-.", label="vPred (predicted)", alpha=0.85)
ax1.set_ylabel("Voltage (V)")
ax1.grid(**GRID_KW)
add_duty_axis(ax1)

_h1 = [l for l in ax1.get_lines() if not l.get_label().startswith("_")]
ax1.legend(_h1, [l.get_label() for l in _h1], loc="upper left")

add_ov_shading(ax1, df)
add_voltloop_vlines(ax1, df)
draw_state_strip(ax1s, df)
# save_fig(fig1, "plot1_voltage")


# ---------------------------------------------------------------------------
# PLOT 2 — Current command chain (top) + protection layers (bottom)
# ---------------------------------------------------------------------------
fig2 = plt.figure(figsize=(18, 12), num="Plot 2 — Command Chain & Protections")
gs2  = gridspec.GridSpec(3, 1, height_ratios=[4, 2.5, 1], hspace=0.12)
ax2a = fig2.add_subplot(gs2[0])
ax2b = fig2.add_subplot(gs2[1], sharex=ax2a)
ax2s = fig2.add_subplot(gs2[2], sharex=ax2a)
plt.setp(ax2a.get_xticklabels(), visible=False)
plt.setp(ax2b.get_xticklabels(), visible=False)
fig2.suptitle(f"Plot 2 — Current Command Chain & Protections  |  {outer_label}", fontsize=14, y=0.99)
add_subtitle(fig2,
    "Top: which limit is dominating the current setpoint, and did the alternator deliver it?  "
    "Bottom: when each overvoltage protection layer fired and for how long.")
fig2.subplots_adjust(top=0.92)

# --- 2a: command chain ---
ax2a.plot(df["t_plot"], df["uTarget_A"],
          color="#1565c0", lw=2.5, label="Current ceiling after all caps  (uTarget_A)")
ax2a.plot(df["t_plot"], df["Icv_A"],
          color="#e91e63", lw=2.2, linestyle="--", label="CV setpoint  (Icv_A)")
ax2a.plot(df["t_plot"], df["fastOvCap_A"],
          color="#455a64", lw=1.6, linestyle=":", label="FastOV voltage ceiling  (fastOvCap_A)", alpha=0.85)
ax2a.plot(df["t_plot"], df["spLimited_A"],
          color="#2e7d32", lw=2.0, label="PID command  (spLimited_A)")
ax2a.plot(df["t_plot"], df["iMeas_A"],
          color="#c62828", lw=2.0, label="Actual current  (iMeas_A)", alpha=0.90)
if "iMeas_filt_A" in df.columns:
    ax2a.plot(df["t_plot"], df["iMeas_filt_A"],
              color="#ef9a9a", lw=1.4, linestyle="--", label="Actual current EMA  (iMeas_filt_A)", alpha=0.75)

ax2a.set_ylabel("Current (A)")
ax2a.grid(**GRID_KW)
ax2a.set_title("Command chain — which limit is lowest wins; field duty on right axis",
               fontsize=10, color="#444444", style="italic", pad=4)
add_duty_axis(ax2a)

_h2a = [l for l in ax2a.get_lines() if not l.get_label().startswith("_")]
ax2a.legend(_h2a, [l.get_label() for l in _h2a], loc="upper left", fontsize=11)

add_ov_shading(ax2a, df)
add_voltloop_vlines(ax2a, df)

# Variable key table — maps legend nicknames to internal variable names
_p2_key = (
    "  Current ceiling/all caps =  uTarget_A      (A)\n"
    "  CV setpoint              =  Icv_A          (A)\n"
    "  FastOV voltage ceiling   =  fastOvCap_A    (A)\n"
    "  PID command              =  spLimited_A    (A)\n"
    "  Actual current (raw)     =  iMeas_A        (A)\n"
    "  Actual current (EMA)     =  iMeas_filt_A   (A)\n"
    "  Field duty               =  duty_pct       (%)"
)
fig2.text(
    0.01, 0.385, _p2_key,
    fontsize=8, family="monospace",
    va="top", ha="left",
    bbox=dict(boxstyle="round,pad=0.35", fc="#f9f9f9", ec="#cccccc", alpha=0.92)
)

# --- 2b: protection flag duration bars + battery current ---
ax2b.set_title(
    "Protection layers — bar length = time active; battery current (right axis) gives load-dump context",
    fontsize=10, color="#444444", style="italic", pad=4)

draw_flag_bars(ax2b, df)

ax2b_r = ax2b.twinx()
if "battI_A" in df.columns:
    ax2b_r.plot(df["t_plot"], df["battI_A"],
                color="#f9a825", lw=1.8, alpha=0.80, label="battI_A (battery current)")
    ax2b_r.axhline(0, color="#888888", linewidth=0.6, linestyle=":", alpha=0.5)
    ax2b_r.set_ylabel("Battery current (A)", color="#f9a825", fontsize=12)
    ax2b_r.tick_params(axis="y", colors="#f9a825", labelsize=11)
    ax2b_r.legend(loc="upper right", fontsize=10)
else:
    ax2b_r.set_yticks([])

draw_state_strip(ax2s, df)
# save_fig(fig2, "plot2_command_chain_protections")


# ---------------------------------------------------------------------------
# PLOT 3 — Engine & scheduling context
# ---------------------------------------------------------------------------
fig3 = plt.figure(figsize=(18, 9), num="Plot 3 — RPM & Context")
gs3  = gridspec.GridSpec(3, 1, height_ratios=[3, 2, 1], hspace=0.10)
ax3  = fig3.add_subplot(gs3[0])
ax3b = fig3.add_subplot(gs3[1], sharex=ax3)
ax3s = fig3.add_subplot(gs3[2], sharex=ax3)
plt.setp(ax3.get_xticklabels(),  visible=False)
plt.setp(ax3b.get_xticklabels(), visible=False)
fig3.suptitle(f"Plot 3 — Engine & Scheduling Context  |  {outer_label}", fontsize=14, y=0.99)
add_subtitle(fig3,
    "RPM, ADC scheduling jitter, and field duty — the mechanical and timing backdrop "
    "for everything in Plots 1 and 2.")
fig3.subplots_adjust(top=0.90)

if "rpm" in df.columns:
    ax3.plot(df["t_plot"], df["rpm"],
             color="#f9a825", lw=2.2, label="RPM")
else:
    ax3.text(0.5, 0.5, "rpm not present in this log",
             ha="center", va="center", transform=ax3.transAxes,
             color="#888888", fontsize=13)
ax3.set_ylabel("RPM")
ax3.grid(**GRID_KW)
ax3.legend(loc="upper left")
add_duty_axis(ax3)
add_voltloop_vlines(ax3, df)

if "ch1_interval_ms" in df.columns:
    ax3b.plot(df["t_plot"], df["ch1_interval_ms"],
              color="#00838f", lw=1.8, label="ch1_interval_ms")
    ax3b.axhline(5,  color="#2e7d32", linewidth=0.8, linestyle=":", alpha=0.70, label="5 ms nominal")
    ax3b.axhline(15, color="#c62828", linewidth=0.8, linestyle=":", alpha=0.70,
                 label="15 ms (3× — stale reading risk)")
else:
    ax3b.text(0.5, 0.5, "ch1_interval_ms not present in this log (older firmware)",
              ha="center", va="center", transform=ax3b.transAxes,
              color="#888888", fontsize=12)
ax3b.set_ylabel("CH1 interval (ms)")
ax3b.grid(**GRID_KW)
ax3b.legend(loc="upper right", fontsize=10)

draw_state_strip(ax3s, df)
# save_fig(fig3, "plot3_rpm_context")


# ---------------------------------------------------------------------------
# PLOT 4 — CV PID term decomposition
#
# Reconstructs P, I, D contributions from logged signals + header gains.
# D term is subtracted in the firmware, so it appears as a negative contribution
# when voltage is rising. P + I − D should equal Icv_A (before clamping).
# ---------------------------------------------------------------------------
fig4 = plt.figure(figsize=(18, 9), num="Plot 4 — PID Term Decomposition")
gs4  = gridspec.GridSpec(3, 1, height_ratios=[4, 1.5, 1], hspace=0.10)
ax4  = fig4.add_subplot(gs4[0])
ax4b = fig4.add_subplot(gs4[1], sharex=ax4)
ax4s = fig4.add_subplot(gs4[2], sharex=ax4)
plt.setp(ax4.get_xticklabels(),  visible=False)
plt.setp(ax4b.get_xticklabels(), visible=False)
fig4.suptitle(f"Plot 4 — CV PID Term Decomposition  |  {outer_label}", fontsize=14, y=0.99)
add_subtitle(fig4,
    "What is the voltage loop actually doing? P reacts to filtered voltage error (targV − battV_filt_V), "
    "I holds the running correction, D backs off current as voltage rises. "
    "Sum of P + I − D should closely match Icv_A (slew target not logged — minor discrepancy on CV entry).")
fig4.subplots_adjust(top=0.90)

# Compute P and D terms from header gains + logged signals.
# P term uses (targV - battV_filt_V): the firmware always uses IBV_filtered (getFiltV())
# for the voltage error, so this matches the actual signal fed to Kp.
# voltageTargetSlewed (the slewed target) is not logged; targV is used as the target
# approximation — differs only briefly on CV entry or setpoint change.
if not (np.isnan(_kp) or np.isnan(_kd)):
    df["pid_P"] = _kp * (df["targV"] - df["battV_filt_V"])
    _have_gains = True
else:
    _have_gains = False
    print("WARNING: VoltageKp/Kd not found in log header — P and D terms cannot be computed")

if "cvDSlope_Vps" in df.columns and not np.isnan(_kd):
    df["pid_D"] = _kd * df["cvDSlope_Vps"]   # D contribution (positive = suppressing)
else:
    df["pid_D"] = pd.Series(0.0, index=df.index)
    print("WARNING: cvDSlope_Vps not in log — D term set to zero (re-download log from updated firmware)")

# I term is cv_I_A directly (already Ki-scaled, in amps)
# Reconstructed total for sanity check
if _have_gains and "cv_I_A" in df.columns:
    df["pid_reconstructed"] = df["pid_P"] + df["cv_I_A"] - df["pid_D"]

# --- Plot P, I, D, and total ---
if _have_gains:
    ax4.plot(df["t_plot"], df["pid_P"],
             color="#1565c0", lw=2.0, label=f"P term  =  Kp({_kp:.4g}) × (targV − battV_filt_V)  (A)")
if "cv_I_A" in df.columns:
    ax4.plot(df["t_plot"], df["cv_I_A"],
             color="#2e7d32", lw=2.0, label="I term  =  cv_I_A  (running integral, A)")
if "pid_D" in df.columns and df["pid_D"].abs().max() > 0:
    ax4.plot(df["t_plot"], -df["pid_D"],
             color="#e65100", lw=2.0, linestyle="--",
             label=f"−D term  =  −Kd({_kd:.4g}) × cvDSlope  (A, negative = suppressing)", alpha=0.85)
if "Icv_A" in df.columns:
    ax4.plot(df["t_plot"], df["Icv_A"],
             color="#c62828", lw=2.2, linestyle="-.",
             label="Icv_A  (total CV output — P + I − D, clamped)", alpha=0.90)
if "pid_reconstructed" in df.columns:
    ax4.plot(df["t_plot"], df["pid_reconstructed"],
             color="#888888", lw=1.2, linestyle=":",
             label="P + I − D  (reconstructed — should match Icv_A)", alpha=0.70)

ax4.axhline(0, color="#999999", linewidth=0.7, linestyle=":", alpha=0.5)
ax4.set_ylabel("Current contribution (A)")
ax4.grid(**GRID_KW)
add_duty_axis(ax4)
_h4 = [l for l in ax4.get_lines() if not l.get_label().startswith("_")]
ax4.legend(_h4, [l.get_label() for l in _h4], loc="upper left", fontsize=10)
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
ax4b.grid(**GRID_KW)
ax4b.legend(loc="upper left", fontsize=10)

draw_state_strip(ax4s, df)
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
plt.show()
