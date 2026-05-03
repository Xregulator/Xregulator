"""
plot_cvlog.py
Diagnostic plotter for CVlog data from the ESP32 alternator regulator.

4 plot windows, each with a state strip below:
  Plot 1 — Voltage: battV, targV, vError_V, vPred, dvdt_Vs
  Plot 2 — CV command chain: cv_I_A, Icv_A, uTarget_A, spLimited_A vs iMeas_A
  Plot 3 — Current signal quality: iMeas / iMA2 / iMA4 + dIdt2 / dIdt4 + CH1 interval
  Plot 4 — Duty pipeline + OV supervisor flags + fastOvCap_A

State strip shows:
  - cvActive bar (green when CV active, grey otherwise)
  - fastOvActive overlay (red when any OV clamp active)
  - Orange tick marks where voltLoopFired=1
  - softClamp / hardClamp / iExcess tick marks

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

# Robust header search — same approach as plot_pidlog.py.
# Handles truncated/concatenated comment+header lines.
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

df["fastOvActive"]  = _to_int("fastOvActive")
df["voltLoopFired"] = _to_int("voltLoopFired")
df["cvActive"]      = _to_int("cvActive")
df["softClamp"]     = _to_int("softClamp")
df["hardClamp"]     = _to_int("hardClamp")
df["iExcess"]   = _to_int("iExcess")

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
GRID_KW = dict(alpha=0.4, linewidth=0.7)

EV_COLOR_VLOOP = "#e91e63"   # voltLoopFired ticks  (hot pink)
EV_COLOR_SOFT  = "#ffeb3b"   # softClamp ticks      (bright yellow — distinct from red fastOV overlay and blue palette)
EV_COLOR_HARD  = "#6a1b9a"   # hardClamp ticks      (purple)
EV_COLOR_FAST  = "#00838f"   # iExcess ticks        (teal)


def draw_state_strip(ax, df):
    """
    State strip:
      Top band   — cvActive bar (green=active, grey=inactive)
      OV overlay — red fill where fastOvActive=1
      Tick lanes:
        row ~0.9  hot pink  = voltLoopFired
        row ~0.5  yellow    = softClamp
        row ~0.3  purple    = hardClamp
        row ~0.1  teal      = iExcess
    """
    ax.set_ylim(0, 3)
    ax.set_yticks([])
    ax.set_xlim(df["t_plot"].iloc[0], df["t_plot"].iloc[-1])
    ax.set_xlabel(time_label, fontsize=13)

    span = df["t_plot"].iloc[-1] - df["t_plot"].iloc[0]

    # --- cvActive background bar ---
    prev_t   = df["t_plot"].iloc[0]
    prev_val = df["cvActive"].iloc[0]
    for i in range(1, len(df)):
        val = df["cvActive"].iloc[i]
        if val != prev_val or i == len(df) - 1:
            t = df["t_plot"].iloc[i]
            color  = "#2e7d32" if prev_val else "#cccccc"
            label  = "CV" if prev_val else ""
            width  = t - prev_t
            ax.barh(2, width, left=prev_t, height=0.75,
                    color=color, alpha=0.85, align="center")
            if prev_val and width > span * 0.04:
                ax.text(prev_t + width / 2, 2, label,
                        ha="center", va="center",
                        fontsize=8, color="white", fontweight="bold", clip_on=True)
            prev_t  = t
            prev_val = val

    # --- fastOvActive red overlay ---
    ov_rows = df[df["fastOvActive"] == 1]
    for t in ov_rows["t_plot"]:
        ax.axvspan(t - 0.002 * span, t + 0.002 * span,
                   ymin=0.5, ymax=1.0, color="#c62828", alpha=0.4)

    # --- Tick lanes ---
    for t in df.loc[df["voltLoopFired"] == 1, "t_plot"]:
        ax.axvline(x=t, ymin=0.62, ymax=0.78,
                   color=EV_COLOR_VLOOP, linewidth=1.2, alpha=0.9)

    for t in df.loc[df["softClamp"] == 1, "t_plot"]:
        ax.axvline(x=t, ymin=0.40, ymax=0.55,
                   color=EV_COLOR_SOFT, linewidth=1.2, alpha=0.85)

    for t in df.loc[df["hardClamp"] == 1, "t_plot"]:
        ax.axvline(x=t, ymin=0.22, ymax=0.37,
                   color=EV_COLOR_HARD, linewidth=1.2, alpha=0.85)

    for t in df.loc[df["iExcess"] == 1, "t_plot"]:
        ax.axvline(x=t, ymin=0.05, ymax=0.18,
                   color=EV_COLOR_FAST, linewidth=1.2, alpha=0.85)

    # Legend text
    ax.text(df["t_plot"].iloc[0] + span * 0.01, 2.55,
            "cvActive", fontsize=7, color="#1b5e20", va="center")
    ax.text(df["t_plot"].iloc[0] + span * 0.01, 1.68,
            "VLoop▲", fontsize=7, color=EV_COLOR_VLOOP, va="center")
    ax.text(df["t_plot"].iloc[0] + span * 0.01, 1.35,
            "softClamp▲", fontsize=7, color=EV_COLOR_SOFT, va="center")
    ax.text(df["t_plot"].iloc[0] + span * 0.01, 1.02,
            "hardClamp▲", fontsize=7, color=EV_COLOR_HARD, va="center")
    ax.text(df["t_plot"].iloc[0] + span * 0.01, 0.72,
            "iExcess▲", fontsize=7, color=EV_COLOR_FAST, va="center")

    ax.spines["top"].set_visible(False)
    ax.spines["right"].set_visible(False)
    ax.spines["left"].set_visible(False)


def add_ov_shading(ax, df):
    """Light red background fill where fastOvActive=1."""
    in_ov   = False
    ov_start = None
    for i, row in df.iterrows():
        if row["fastOvActive"] and not in_ov:
            ov_start = row["t_plot"]
            in_ov    = True
        elif not row["fastOvActive"] and in_ov:
            ax.axvspan(ov_start, row["t_plot"],
                       color="#c62828", alpha=0.08)
            in_ov = False
    if in_ov:
        ax.axvspan(ov_start, df["t_plot"].iloc[-1],
                   color="#c62828", alpha=0.08)


def add_voltloop_vlines(ax, df):
    """Faint orange vlines where voltage loop fired."""
    for t in df.loc[df["voltLoopFired"] == 1, "t_plot"]:
        ax.axvline(x=t, color=EV_COLOR_VLOOP, linewidth=0.6, alpha=0.35)


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
    fig.suptitle(f"{title_suffix}  |  {outer_label}", fontsize=14)
    return fig, ax, axs


# ---------------------------------------------------------------------------
# PLOT 1 — Voltage: what the voltage loop sees
#
# Diagnoses: battV vs target, vError sign/magnitude, vPred vs battV,
#            dvdt_Vs sign (rising/falling), OV shading.
# ---------------------------------------------------------------------------
fig1, ax1, ax1s = make_fig("Plot 1 — Voltage Loop Input", "Plot 1 — Voltage")
ax1b = ax1.twinx()

ax1.plot(df["t_plot"], df["battV"],
         color="#1565c0", lw=2.5, label="battV")
ax1.plot(df["t_plot"], df["targV"],
         color="#e91e63", lw=2.2, linestyle="--", label="targV")
ax1.plot(df["t_plot"], df["vPred"],
         color="#00838f", lw=2.0, linestyle="-.", label="vPred (battV + TD·dvdt⁺)", alpha=0.85)

ax1b.plot(df["t_plot"], df["vError_V"],
          color="#c62828", lw=2.0, label="vError_V", alpha=0.9)
ax1b.plot(df["t_plot"], df["dvdt_Vs"],
          color="#37474f", lw=2.2, linestyle="--", label="dvdt_Vs (EMA)", alpha=0.95)
ax1b.axhline(0, color="#c62828", linewidth=0.6, linestyle=":", alpha=0.4)

ax1.set_ylabel("Voltage (V)", color="#1565c0")
ax1b.set_ylabel("vError (V) / dvdt (V/s)", color="#c62828")
ax1.grid(**GRID_KW)

# Collect handles before decorative vlines are added to ax1
_h1 = [l for l in ax1.get_lines() + ax1b.get_lines() if not l.get_label().startswith("_")]
ax1.legend(_h1, [l.get_label() for l in _h1], loc="upper left")

add_ov_shading(ax1, df)
add_voltloop_vlines(ax1, df)

draw_state_strip(ax1s, df)
# save_fig(fig1, "plot1_voltage")


# ---------------------------------------------------------------------------
# PLOT 2 — CV command chain
#
# Diagnoses: is cv_I settled or drifting? Is Icv tracking setpoint or
#            being clamped by uTarget? Is spLimited lagging Icv?
#            How does iMeas_A compare to the commanded setpoint?
#            OV clamp squeezes fastOvCap_A below uTarget — visible here.
# ---------------------------------------------------------------------------
fig2, ax2, ax2s = make_fig("Plot 2 — CV Command Chain (cv_I, Icv, limits, iMeas)", "Plot 2 — Command Chain")
ax2b = ax2.twinx()

ax2.plot(df["t_plot"], df["uTarget_A"],
         color="#1565c0", lw=2.5, label="uTarget_A (RPM/thermal ceiling, post-OV cap)")
ax2.plot(df["t_plot"], df["Icv_A"],
         color="#e91e63", lw=2.2, linestyle="--", label="Icv_A (CV setpoint)")
ax2.plot(df["t_plot"], df["spLimited_A"],
         color="#2e7d32", lw=2.0, label="spLimited_A (slew-limited → output current PID)")
ax2.plot(df["t_plot"], df["fastOvCap_A"],
         color="#455a64", lw=1.6, linestyle=":", label="fastOvCap_A (OV ceiling)", alpha=0.80)

ax2b.plot(df["t_plot"], df["cv_I_A"],
          color="#6a1b9a", lw=2.0, label="cv_I_A (integrator state)", alpha=0.85)
ax2b.set_ylabel("cv_I (A) — integrator", color="#6a1b9a")
ax2.set_ylabel("Current (A)")
ax2.grid(**GRID_KW)

# iMeas on right axis too — overlay with cv_I axis
ax2b.plot(df["t_plot"], df["iMeas_A"],
          color="#c62828", lw=1.8, linestyle="--", label="iMeas_A", alpha=0.75)

# Collect handles before decorative vlines are added to ax2
_h2 = [l for l in ax2.get_lines() + ax2b.get_lines() if not l.get_label().startswith("_")]
ax2.legend(_h2, [l.get_label() for l in _h2], loc="upper left")

add_ov_shading(ax2, df)
add_voltloop_vlines(ax2, df)

draw_state_strip(ax2s, df)
# save_fig(fig2, "plot2_command_chain")

# ---------------------------------------------------------------------------
# PLOT 3 — Filtered signal quality
#
# Three sub-panels:
#   3a: battV vs battV_filt_V — voltage filter lag / noise rejection
#   3b: iMeas_A vs iMeas_filt_A — current filter lag / noise rejection
#   3c: ch1_interval_ms — ADC scheduling jitter (gaps → stale readings)
# ---------------------------------------------------------------------------
fig3 = plt.figure(figsize=(18, 11), num="Plot 3 — Filtered Signal Quality")
gs3  = gridspec.GridSpec(4, 1, height_ratios=[3, 3, 1.5, 1], hspace=0.10)
ax3a = fig3.add_subplot(gs3[0])
ax3b = fig3.add_subplot(gs3[1], sharex=ax3a)
ax3c = fig3.add_subplot(gs3[2], sharex=ax3a)
ax3s = fig3.add_subplot(gs3[3], sharex=ax3a)

plt.setp(ax3a.get_xticklabels(), visible=False)
plt.setp(ax3b.get_xticklabels(), visible=False)
plt.setp(ax3c.get_xticklabels(), visible=False)
fig3.suptitle(f"Plot 3 — Filtered Signal Quality  |  {outer_label}", fontsize=14)

# 3a: battery voltage raw vs filtered, plus target reference
ax3a.plot(df["t_plot"], df["battV"],
          color="#64b5f6", lw=1.6, alpha=0.70, label="battV (raw)")
ax3a.plot(df["t_plot"], df["battV_filt_V"],
          color="#1565c0", lw=2.2, label="battV_filt_V (filtered)")
ax3a.plot(df["t_plot"], df["targV"],
          color="#e91e63", lw=1.8, linestyle="--", alpha=0.85, label="targV")
ax3a.set_ylabel("Voltage (V)")
ax3a.grid(**GRID_KW)
ax3a.legend(loc="upper left")
add_ov_shading(ax3a, df)

# 3b: measured current raw vs filtered
ax3b.plot(df["t_plot"], df["iMeas_A"],
          color="#43a047", lw=1.6, alpha=0.70, label="iMeas_A (raw)")
ax3b.plot(df["t_plot"], df["iMeas_filt_A"],
          color="#1b5e20", lw=2.2, label="iMeas_filt_A (filtered)")
ax3b.set_ylabel("Current (A)")
ax3b.grid(**GRID_KW)
ax3b.legend(loc="upper left")
add_ov_shading(ax3b, df)

# 3c: CH1 interval
ax3c.plot(df["t_plot"], df["ch1_interval_ms"],
          color="#00838f", lw=1.8, label="ch1_interval_ms")
ax3c.axhline(5, color="#2e7d32", linewidth=0.8, linestyle=":",
             alpha=0.70, label="5 ms nominal")
ax3c.axhline(15, color="#c62828", linewidth=0.8, linestyle=":",
             alpha=0.70, label="15 ms (3× — stale reading risk)")
ax3c.set_ylabel("CH1 interval (ms)")
ax3c.grid(**GRID_KW)
ax3c.legend(loc="upper right", fontsize=10)

draw_state_strip(ax3s, df)
# save_fig(fig3, "plot3_filtered_signals")
# ---------------------------------------------------------------------------
# PLOT 4 — Duty pipeline + OV supervisor
#
# Diagnoses: how aggressively is the OV supervisor cutting fastOvCap_A?
#            Does duty follow Icv promptly or is the governor slewing it?
#            Where do softClamp / hardClamp / iExcess actually fire?
# ---------------------------------------------------------------------------
fig4 = plt.figure(figsize=(18, 9), num="Plot 4 — Duty + OV Supervisor")
gs4  = gridspec.GridSpec(3, 1, height_ratios=[3.5, 1.5, 1], hspace=0.10)
ax4a = fig4.add_subplot(gs4[0])
ax4b = fig4.add_subplot(gs4[1], sharex=ax4a)
ax4s = fig4.add_subplot(gs4[2], sharex=ax4a)

plt.setp(ax4a.get_xticklabels(), visible=False)
plt.setp(ax4b.get_xticklabels(), visible=False)
fig4.suptitle(f"Plot 4 — Duty Pipeline & OV Supervisor  |  {outer_label}", fontsize=14)

# 4a: duty + OV cap + iMeas
ax4a_r = ax4a.twinx()
ax4a.plot(df["t_plot"], df["duty_pct"],
          color="#2e7d32", lw=2.5, label="duty_pct")
ax4a.plot(df["t_plot"], df["spLimited_A"],
          color="#e91e63", lw=1.8, linestyle="--", label="spLimited_A (output current PID setpoint)", alpha=0.80)
ax4a_r.plot(df["t_plot"], df["iMeas_A"],
            color="#c62828", lw=2.0, label="iMeas_A", alpha=0.85)
ax4a_r.plot(df["t_plot"], df["fastOvCap_A"],
            color="#455a64", lw=1.6, linestyle=":", label="fastOvCap_A", alpha=0.75)
ax4a.set_ylabel("Duty (%) / Setpoint (A)")
ax4a_r.set_ylabel("iMeas / OV cap (A)", color="#c62828")
ax4a.grid(**GRID_KW)
add_ov_shading(ax4a, df)

lines4  = ax4a.get_lines() + ax4a_r.get_lines()
labels4 = [l.get_label() for l in lines4 if not l.get_label().startswith("_")]
ax4a.legend(lines4[:len(labels4)], labels4, loc="upper left")

# 4b: binary flag lanes stacked as bar-style fills
flag_h = 0.8
for offset, col, color, label in [
    (3.0, "fastOvActive", "#c62828",  "fastOvActive"),
    (2.0, "softClamp",    "#0277bd",  "softClamp"),
    (1.0, "hardClamp",    "#6a1b9a",  "hardClamp"),
    (0.0, "iExcess",      "#00838f",  "iExcess"),
]:
    vals = df[col].values
    t    = df["t_plot"].values
    # Fill spans where flag=1
    in_flag   = False
    flag_start = None
    for i in range(len(vals)):
        if vals[i] and not in_flag:
            flag_start = t[i]
            in_flag    = True
        elif not vals[i] and in_flag:
            ax4b.barh(offset + flag_h / 2, t[i] - flag_start,
                      left=flag_start, height=flag_h,
                      color=color, alpha=0.80, align="center")
            in_flag = False
    if in_flag:
        ax4b.barh(offset + flag_h / 2, t[-1] - flag_start,
                  left=flag_start, height=flag_h,
                  color=color, alpha=0.80, align="center")

    ax4b.text(df["t_plot"].iloc[0], offset + flag_h / 2, f"  {label}",
              va="center", fontsize=9, color=color)

ax4b.set_ylim(-0.2, 4.2)
ax4b.set_yticks([])
ax4b.set_ylabel("Flags")
ax4b.grid(**GRID_KW)

draw_state_strip(ax4s, df)
# save_fig(fig4, "plot4_duty_ov")

# ---------------------------------------------------------------------------
# PLOT 5 — RPM
#
# Shows engine/alternator RPM over time so you can correlate speed changes
# with voltage/current behaviour in the other plots.
# ---------------------------------------------------------------------------
fig5, ax5, ax5s = make_fig("Plot 5 — Engine RPM", "Plot 5 — RPM")

if "rpm" in df.columns:
    ax5.plot(df["t_plot"], df["rpm"],
             color="#f9a825", lw=2.2, label="rpm")
else:
    ax5.text(0.5, 0.5, "rpm column not present in this log",
             ha="center", va="center", transform=ax5.transAxes,
             color="#888888", fontsize=13)

ax5.set_ylabel("RPM")
ax5.grid(**GRID_KW)
ax5.legend(loc="upper left")
add_voltloop_vlines(ax5, df)

draw_state_strip(ax5s, df)
# save_fig(fig5, "plot5_rpm")

# ---------------------------------------------------------------------------
# Linked x-axis zoom — syncs all plot windows when any one is zoomed/panned.
# Registers xlim_changed on the primary (top) axes of each figure; because
# sub-axes within a figure already share x via sharex, only one representative
# per figure is needed.
# ---------------------------------------------------------------------------
_all_primary_axes = [ax1, ax2, ax3a, ax4a, ax5]
_all_figs         = [fig1, fig2, fig3, fig4, fig5]
_syncing = [False]   # mutable container so the closure can write to it

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
