"""
plot_thermallog.py
- File selector GUI (tkinter) — pick any *.csv from Downloads, newest first
- 3 plot windows, each with a state strip below for binary/low-integer signals
- PNGs saved to Downloads alongside the source CSV
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
    root.title("Select Thermal Log")
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

# Parse CONST row
constants = {}
with open(path) as f:
    for line in f:
        if line.startswith("CONST"):
            for token in line.strip().split(",")[1:]:
                if "=" in token:
                    k, v = token.split("=")
                    constants[k.strip()] = float(v.strip())
            break

kp       = constants.get("kp",       float("nan"))
ki       = constants.get("ki",       float("nan"))
lookahead = constants.get("lookahead", float("nan"))
const_label = f"Kp={kp}  Ki={ki}  Lookahead={lookahead}s"
print(f"Constants: {const_label}")

# Load data rows
df = pd.read_csv(path, on_bad_lines="skip")
df = df[df["ts_ms"] != "CONST"].copy()

numeric_cols = [
    "ts_ms", "tempFilt_F", "tempProj_F", "nominalTarget_A", "rpmCap_A",
    "voltCap_A", "uTarget_A", "spLimited_A", "pidErr_A", "pidOut_pct",
    "duty_pct", "RPM", "battV", "measAmps_A", "penaltyAmps_A", "flags",
    "chargeStageDisplay", "outerP", "outerI", "outerD", "impliedPenalty",
    "antiWindupFired", "thermalSlope_F_sec"
]

for col in numeric_cols:
    if col in df.columns:
        df[col] = pd.to_numeric(df[col], errors="coerce")

df.dropna(subset=["ts_ms"], inplace=True)
df.reset_index(drop=True, inplace=True)
df["t_s"] = (df["ts_ms"] - df["ts_ms"].iloc[0]) / 1000.0

total_time_s = df["t_s"].iloc[-1]

if total_time_s > 7200:  # >2 hours
    df["t_plot"] = df["t_s"] / 3600.0
    time_label = "Time (hours)"
elif total_time_s > 120:  # >2 minutes
    df["t_plot"] = df["t_s"] / 60.0
    time_label = "Time (minutes)"
else:
    df["t_plot"] = df["t_s"]
    time_label = "Time (seconds)"

# Decode flags and stage
flags = pd.to_numeric(df["flags"], errors="coerce").fillna(0).astype("int64")
df["chargeStageDisplay"] = pd.to_numeric(df["chargeStageDisplay"], errors="coerce").fillna(0).astype("int64")

df["f_tempPIDActive"] = ((flags // (2**0)) % 2).astype(int)
df["f_autoMode"]      = ((flags // (2**4)) % 2).astype(int)
df["f_shutdown"]      = ((flags // (2**5)) % 2).astype(int)

# ---------------------------------------------------------------------------
# 3. Mode-change events
# ---------------------------------------------------------------------------
THERMAL_MODE_COLORS = {
    "shutdown":   "#c62828",
    "bulk":       "#2e7d32",
    "absorption": "#6a1b9a",
    "float":      "#e91e63",
    "manual":     "#c62828",
    "maintain":   "#388e3c",
    "targetV":    "#1565c0",
    "idle":       "#777777",
    "antiWindup": "#c62828",
}

state_changes = df.index[
    (df["flags"] != df["flags"].shift()) |
    (df["chargeStageDisplay"] != df["chargeStageDisplay"].shift())
].tolist()
if state_changes and state_changes[0] == 0:
    state_changes = state_changes[1:]

def mode_label_color(row):
    if row["f_shutdown"]:
        return "SHUTDOWN", THERMAL_MODE_COLORS["shutdown"]

    stage = int(row["chargeStageDisplay"])

    if stage == 1:
        return "BULK", THERMAL_MODE_COLORS["bulk"]
    if stage == 2:
        return "ABSORPTION", THERMAL_MODE_COLORS["absorption"]
    if stage == 3:
        return "FLOAT", THERMAL_MODE_COLORS["float"]
    if stage == 4:
        return "MANUAL", THERMAL_MODE_COLORS["manual"]
    if stage == 5:
        return "MAINTAIN", THERMAL_MODE_COLORS["maintain"]
    if stage == 6:
        return "TARGET V", THERMAL_MODE_COLORS["targetV"]

    return "IDLE", THERMAL_MODE_COLORS["idle"]

# ---------------------------------------------------------------------------
# 4. State strip
#    - Color-coded mode timeline bar
#    - antiWindupFired as vertical red ticks
# ---------------------------------------------------------------------------
def draw_state_strip(ax, df, state_changes):
    ax.set_ylim(0, 3)
    ax.set_yticks([])
    ax.set_xlim(df["t_plot"].iloc[0], df["t_plot"].iloc[-1])
    ax.set_xlabel(time_label, fontsize=13)

    # Build mode segments
    boundaries = state_changes + [len(df) - 1]
    prev_t = df["t_plot"].iloc[0]
    prev_row = df.iloc[0]

    for idx in boundaries:
        t = df.loc[idx, "t_plot"] if idx < len(df) else df["t_plot"].iloc[-1]
        label, color = mode_label_color(prev_row)
        width = t - prev_t
        ax.barh(2, width, left=prev_t, height=0.7, color=color, alpha=0.85, align="center")
        if width > (df["t_plot"].iloc[-1] - df["t_plot"].iloc[0]) * 0.03:
            ax.text(prev_t + width / 2, 2, label,
                    ha="center", va="center",
                    fontsize=8, color="white", fontweight="bold", clip_on=True)
        if idx < len(df):
            prev_row = df.loc[idx]
        prev_t = t

    aw = df[df["antiWindupFired"] == 1]
    for t in aw["t_plot"]:
        ax.axvline(x=t, ymin=0, ymax=0.38, color=THERMAL_MODE_COLORS["antiWindup"], linewidth=1.5, alpha=0.9)

    if not aw.empty:
        span = df["t_plot"].iloc[-1] - df["t_plot"].iloc[0]
        ax.text(df["t_plot"].iloc[0] + span * 0.01, 0.5, "antiWindup ▲",
                fontsize=8, color=THERMAL_MODE_COLORS["antiWindup"], va="center")

    ax.spines["top"].set_visible(False)
    ax.spines["right"].set_visible(False)
    ax.spines["left"].set_visible(False)


def add_mode_vlines(ax, df, state_changes):
    for idx in state_changes:
        t = df.loc[idx, "t_plot"]
        _, color = mode_label_color(df.loc[idx])
        ax.axvline(x=t, color=color, linewidth=1.0, linestyle="--", alpha=0.5)


def save_fig(fig, suffix):
    out = os.path.join(DOWNLOADS, f"{basename}_{suffix}.png")
    fig.savefig(out, dpi=150, bbox_inches="tight")
    print(f"Saved: {out}")


# ---------------------------------------------------------------------------
# Shared style
# ---------------------------------------------------------------------------
GRID_KW = dict(alpha=0.4, linewidth=0.7)

# ---------------------------------------------------------------------------
# PLOT 1 — Temperature control & penalty output
# ---------------------------------------------------------------------------
fig1 = plt.figure(figsize=(16, 8), num="Plot 1 — Temp Control & Penalty")
gs1  = gridspec.GridSpec(2, 1, height_ratios=[5, 1], hspace=0.08)
ax1a = fig1.add_subplot(gs1[0])
ax1b = ax1a.twinx()
ax1s = fig1.add_subplot(gs1[1], sharex=ax1a)

ax1a.plot(df["t_plot"], df["tempFilt_F"],  color="#c62828", lw=2.5, label="tempFilt_F (measured)")
ax1a.plot(df["t_plot"], df["tempProj_F"], color="#e91e63", lw=2.2, linestyle="--", label="tempProj_F (PID input)")
ax1b.plot(df["t_plot"], df["penaltyAmps_A"], color="#2e7d32", lw=2.2, label="penaltyAmps_A")

ax1a.set_ylabel("Temperature (°F)", color="#c62828")
ax1b.set_ylabel("Penalty Amps (A)", color="#2e7d32")
ax1a.grid(**GRID_KW)
plt.setp(ax1a.get_xticklabels(), visible=False)

lines1  = ax1a.get_lines() + ax1b.get_lines()
labels1 = [l.get_label() for l in lines1]
ax1a.legend(lines1, labels1, loc="upper left")

add_mode_vlines(ax1a, df, state_changes)
draw_state_strip(ax1s, df, state_changes)
fig1.suptitle(f"Temp Control & Penalty  |  {const_label}")
# save_fig(fig1, "plot1_temp")

# ---------------------------------------------------------------------------
# PLOT 2 — PID term decomposition
# ---------------------------------------------------------------------------
fig2 = plt.figure(figsize=(16, 8), num="Plot 2 — PID Term Decomposition")
gs2  = gridspec.GridSpec(2, 1, height_ratios=[5, 1], hspace=0.08)
ax2  = fig2.add_subplot(gs2[0])
ax2s = fig2.add_subplot(gs2[1], sharex=ax2)

ax2.plot(df["t_plot"], df["outerP"],          color="#1565c0", lw=2.2, label="outerP")
ax2.plot(df["t_plot"], df["outerI"],          color="#f9a825", lw=2.2, label="outerI")
ax2.plot(df["t_plot"], df["outerD"],          color="#6a1b9a", lw=2.2, label="outerD")
ax2.plot(df["t_plot"], df["impliedPenalty"],  color="#2e7d32", lw=2.5, label="impliedPenalty")
ax2.plot(df["t_plot"], df["thermalSlope_F_sec"], color="#00838f", lw=2.0, linestyle=":", label="thermalSlope_F_sec")

ax2.set_ylabel("PID Terms")
ax2.grid(**GRID_KW)
ax2.legend(loc="upper left")
plt.setp(ax2.get_xticklabels(), visible=False)

add_mode_vlines(ax2, df, state_changes)
draw_state_strip(ax2s, df, state_changes)
fig2.suptitle(f"PID Term Decomposition  |  {const_label}")
# save_fig(fig2, "plot2_pid")

# ---------------------------------------------------------------------------
# PLOT 3 — Constraint context
# ---------------------------------------------------------------------------
fig3 = plt.figure(figsize=(16, 8), num="Plot 3 — Constraint Context")
gs3  = gridspec.GridSpec(2, 1, height_ratios=[5, 1], hspace=0.08)
ax3  = fig3.add_subplot(gs3[0])
ax3s = fig3.add_subplot(gs3[1], sharex=ax3)

ax3.plot(df["t_plot"], df["measAmps_A"],  color="#c62828", lw=2.5, label="measAmps_A")
ax3.plot(df["t_plot"], df["uTarget_A"],   color="#1565c0", lw=2.2, label="uTarget_A")
ax3.plot(df["t_plot"], df["spLimited_A"], color="#e91e63", lw=2.2, linestyle="--", label="spLimited_A")
ax3.plot(df["t_plot"], df["voltCap_A"],   color="#6a1b9a", lw=2.0, linestyle=":", label="voltCap_A")
ax3.plot(df["t_plot"], df["duty_pct"],
         color="#455a64", lw=1.8, linestyle="-.", label="duty_pct")

ax3.set_ylabel("Amps (A)")
ax3.grid(**GRID_KW)
ax3.legend(loc="upper left")
plt.setp(ax3.get_xticklabels(), visible=False)

add_mode_vlines(ax3, df, state_changes)
draw_state_strip(ax3s, df, state_changes)
fig3.suptitle(f"Constraint Context  |  {const_label}")
# save_fig(fig3, "plot3_constraints")

# ---------------------------------------------------------------------------

# ---------------------------------------------------------------------------
# PLOT 4 — Battery Voltage & RPM
# ---------------------------------------------------------------------------
fig4 = plt.figure(figsize=(16, 8), num="Plot 4 — Battery Voltage & RPM")
gs4  = gridspec.GridSpec(2, 1, height_ratios=[5, 1], hspace=0.08)
ax4a = fig4.add_subplot(gs4[0])
ax4b = ax4a.twinx()
ax4s = fig4.add_subplot(gs4[1], sharex=ax4a)

# Left axis: battery voltage
ax4a.plot(df["t_plot"], df["battV"],
          color="#1565c0", lw=2.5, label="battV")

# Right axis: RPM / 100
ax4b.plot(df["t_plot"], df["RPM"] / 100.0,
          color="#f9a825", lw=2.2, label="RPM/100")

ax4a.set_ylabel("Battery Voltage (V)", color="#1565c0")
ax4b.set_ylabel("RPM / 100", color="#f9a825")

ax4a.grid(**GRID_KW)
plt.setp(ax4a.get_xticklabels(), visible=False)

# Combined legend (same pattern as Plot 1)
lines4  = ax4a.get_lines() + ax4b.get_lines()
labels4 = [l.get_label() for l in lines4]
ax4a.legend(lines4, labels4, loc="upper left")

add_mode_vlines(ax4a, df, state_changes)
draw_state_strip(ax4s, df, state_changes)

fig4.suptitle(f"Battery Voltage & RPM  |  {const_label}")
# save_fig(fig4, "plot4_batt_rpm")

# ---------------------------------------------------------------------------
# Linked x-axis zoom — syncs all plot windows when any one is zoomed/panned.
# Registers xlim_changed on the primary (top) axes of each figure; sub-axes
# within a figure already share x via sharex so only one per figure is needed.
# ---------------------------------------------------------------------------
_all_primary_axes = [ax1a, ax2, ax3, ax4a]
_all_figs         = [fig1, fig2, fig3, fig4]
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
