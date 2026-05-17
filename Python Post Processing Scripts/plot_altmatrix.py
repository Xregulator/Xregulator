"""
plot_altmatrix.py
Alternator Health Matrix viewer for Xregulator.

Finds AltHealthMatrix_*.csv files in ~/Downloads (newest first),
lets you pick one, then opens a two-tab window:

  Tab 1 — Overview
    Summary stats, SS-time bar charts by RPM/temp/field bucket,
    heatmap grid of SS seconds (one panel per RPM bucket),
    heatmap grid of average output amps.

  Tab 2 — Field Curve
    Line plot: output amps (Y) vs field voltage (X).
    One line per RPM × temperature bucket combination.
    Color = RPM bucket (blue → red).  Marker = temperature bucket.
    Shaded band = min/max amps range.
    Sliders filter which lines appear:
      RPM min / max bucket
      Temp min / max bucket
      Min SS seconds (hide low-confidence cells)

Run:
  cd /Users/joeceo/Documents/Arduino/Xregulator && \\
  "$HOME/Documents/Xeng_Python_Venvs/tk/bin/python" \\
  "Python Post Processing Scripts/plot_altmatrix.py"
"""

import glob
import os
import re
import datetime
import tkinter as tk
from tkinter import messagebox, ttk

import matplotlib
matplotlib.use("TkAgg")
from matplotlib.backends.backend_tkagg import FigureCanvasTkAgg, NavigationToolbar2Tk
import matplotlib.pyplot as plt
import matplotlib.gridspec as gridspec
from matplotlib.patches import Rectangle
import numpy as np
import pandas as pd

plt.rcParams.update({
    "font.size":       9,
    "axes.titlesize":  9,
    "axes.labelsize":  8,
    "xtick.labelsize": 7,
    "ytick.labelsize": 7,
})

DOWNLOADS    = os.path.expanduser("~/Downloads")
FILE_KEYWORD = "AltHealthMatrix"

SS_CMAP  = matplotlib.cm.YlOrRd.copy();  SS_CMAP.set_bad(color="#cccccc")
AMP_CMAP = matplotlib.cm.plasma.copy();  AMP_CMAP.set_bad(color="#cccccc")
RPM_LINE_CMAP = matplotlib.cm.get_cmap("RdYlBu_r")
TEMP_MARKERS  = ['o', 's', '^', 'D', 'v', 'P', '*']


# ─────────────────────────────────────────────────────────────────────────────
# File picker
# ─────────────────────────────────────────────────────────────────────────────
def pick_file():
    pattern = os.path.join(DOWNLOADS, f"{FILE_KEYWORD}_*.csv")
    files   = sorted(glob.glob(pattern), key=os.path.getmtime, reverse=True)
    if not files:
        _r = tk.Tk(); _r.withdraw()
        messagebox.showerror(
            "No files",
            f"No {FILE_KEYWORD}_*.csv found in {DOWNLOADS}\n\n"
            "Export one from the Live Data tab on the regulator UI.")
        _r.destroy()
        return None

    selected = []
    root = tk.Tk()
    try:
        root.tk.call("::tk::unsupported::MacWindowStyle", "appearance",
                     root._w, "NSAppearanceNameAqua")
    except Exception:
        pass
    root.title("Select Matrix Export")
    root.resizable(False, False)
    root.configure(bg="#f5f5f5")

    tk.Label(root, text="Select matrix export to view:",
             bg="#f5f5f5", font=("Helvetica", 13, "bold")).pack(padx=16, pady=(16, 8))

    frame = tk.Frame(root, bg="#f5f5f5")
    frame.pack(padx=16, pady=4, fill="both", expand=True)
    sb = tk.Scrollbar(frame);  sb.pack(side="right", fill="y")
    lb = tk.Listbox(frame, yscrollcommand=sb.set, width=64,
                    height=min(len(files), 12),
                    font=("Helvetica", 11), selectmode="single", bg="white")
    for f in files:
        ts = datetime.datetime.fromtimestamp(os.path.getmtime(f)).strftime("%Y-%m-%d %H:%M")
        kb = os.path.getsize(f) // 1024
        lb.insert(tk.END, f"  {ts}   {os.path.basename(f)}  ({kb} KB)")
    lb.pack(side="left", fill="both", expand=True)
    lb.selection_set(0)
    sb.config(command=lb.yview)

    def on_ok():
        idx = lb.curselection()
        if idx:
            selected.append(files[idx[0]])
        root.destroy()

    bf = tk.Frame(root, bg="#f5f5f5");  bf.pack(pady=12)
    tk.Button(bf, text="Open",   command=on_ok,        width=12,
              font=("Helvetica", 12)).pack(side="left", padx=8)
    tk.Button(bf, text="Cancel", command=root.destroy, width=10,
              font=("Helvetica", 12)).pack(side="left", padx=4)
    root.mainloop()
    return selected[0] if selected else None


# ─────────────────────────────────────────────────────────────────────────────
# Data helpers
# ─────────────────────────────────────────────────────────────────────────────
def load_matrix(path):
    df = pd.read_csv(path)
    df["populated"] = df["ss_seconds"] > 0
    return df


def sorted_labels(df, bucket_col, label_col):
    return [df[df[bucket_col] == i][label_col].iloc[0]
            for i in sorted(df[bucket_col].unique())]


def field_center(label):
    """'4.3-6.4V' → 5.35  (midpoint of the two boundary numbers)."""
    nums = re.findall(r"[\d.]+", label)
    return (float(nums[0]) + float(nums[1])) / 2 if len(nums) >= 2 else float(nums[0])


def pivot(df, rpm_bucket, value_col, mask_empty=True):
    sub = df[df["rpm_bucket"] == rpm_bucket]
    nt  = df["temp_bucket"].nunique()
    nf  = df["field_bucket"].nunique()
    arr = np.full((nt, nf), np.nan)
    for _, row in sub.iterrows():
        t, f = int(row["temp_bucket"]), int(row["field_bucket"])
        if mask_empty and row["ss_seconds"] == 0:
            arr[t, f] = np.nan
        else:
            arr[t, f] = row[value_col]
    return np.ma.masked_invalid(arr)


def ref_mask(df, rpm_bucket):
    sub = df[df["rpm_bucket"] == rpm_bucket]
    nt  = df["temp_bucket"].nunique()
    nf  = df["field_bucket"].nunique()
    out = np.zeros((nt, nf), dtype=bool)
    for _, row in sub.iterrows():
        if row["is_reference_bin"] == 1:
            out[int(row["temp_bucket"]), int(row["field_bucket"])] = True
    return out


# ─────────────────────────────────────────────────────────────────────────────
# Overview figure builder  (returns Figure, does NOT call plt.show)
# ─────────────────────────────────────────────────────────────────────────────
def _draw_heatmap_row(axes, df, n_rpm, rpm_labels, temp_labels, field_labels,
                      value_col, cmap, vmin, vmax, unit, mask_empty=True):
    last_im = None
    for i, ax in enumerate(axes):
        if i >= n_rpm:
            ax.axis("off")
            continue
        arr  = pivot(df, i, value_col, mask_empty=mask_empty)
        refs = ref_mask(df, i)
        im   = ax.imshow(arr, aspect="auto", origin="lower",
                         cmap=cmap, vmin=vmin, vmax=vmax, interpolation="nearest")
        last_im = im
        for t in range(arr.shape[0]):
            for f in range(arr.shape[1]):
                if refs[t, f]:
                    ax.add_patch(Rectangle((f-0.5, t-0.5), 1, 1,
                                           linewidth=1.6, edgecolor="#00cc44", facecolor="none"))
        ax.set_title(rpm_labels[i], fontsize=8, pad=3)
        ax.set_xticks(range(len(field_labels)))
        ax.set_xticklabels(field_labels, rotation=50, ha="right", fontsize=6)
        ax.set_yticks(range(len(temp_labels)))
        ax.set_yticklabels(temp_labels, fontsize=6)
        if i == 0:
            ax.set_ylabel("Temp bucket", fontsize=7)
    if last_im is not None:
        plt.colorbar(last_im, ax=axes[n_rpm-1], label=unit, fraction=0.05, pad=0.06)


def build_overview_figure(df, fname):
    rpm_labels   = sorted_labels(df, "rpm_bucket",   "rpm_label")
    temp_labels  = sorted_labels(df, "temp_bucket",  "temp_label")
    field_labels = sorted_labels(df, "field_bucket", "field_label")
    n_rpm, n_temp, n_fld = len(rpm_labels), len(temp_labels), len(field_labels)

    total_cells = len(df)
    pop_cells   = int(df["populated"].sum())
    ref_bins    = int((df["is_reference_bin"] == 1).sum())
    total_ss_s  = int(df["ss_seconds"].sum())
    pop_pct     = 100.0 * pop_cells / total_cells if total_cells else 0

    pop = df[df["populated"]]
    summary_lines = [
        f"File:          {os.path.basename(fname)}",
        f"Matrix size:   {n_rpm} RPM × {n_temp} temp × {n_fld} field = {total_cells} cells",
        f"Populated:     {pop_cells} / {total_cells}  ({pop_pct:.1f}%)",
        f"Reference bins: {ref_bins}",
        f"Total SS time:  {total_ss_s//3600}h {(total_ss_s%3600)//60}m {total_ss_s%60}s",
        "",
        f"RPM visited:   {', '.join(sorted(pop['rpm_label'].unique())) or 'none'}",
        f"Temp visited:  {', '.join(sorted(pop['temp_label'].unique())) or 'none'}",
        f"Field visited: {', '.join(sorted(pop['field_label'].unique())) or 'none'}",
        "",
        "  ▪ green border = reference bin",
        "  ▪ gray fill    = never visited",
    ]

    fig_w = max(18, n_rpm * 2.4)
    fig   = plt.figure(figsize=(fig_w, 22))
    fig.suptitle(f"Alternator Health Matrix — {os.path.basename(fname)}", fontsize=11, y=0.998)

    outer = gridspec.GridSpec(4, 1, figure=fig,
                              height_ratios=[3.0, 0.25, 4.5, 4.5], hspace=0.50)

    # ── summary + bar charts ─────────────────────────────────────────────────
    top = gridspec.GridSpecFromSubplotSpec(1, 4, subplot_spec=outer[0], wspace=0.45)

    ax_txt = fig.add_subplot(top[0])
    ax_txt.axis("off")
    ax_txt.text(0.03, 0.97, "\n".join(summary_lines), transform=ax_txt.transAxes,
                fontsize=7.5, va="top", family="monospace",
                bbox=dict(boxstyle="round", facecolor="#f0f0f0", alpha=0.8))

    def bar(ax, values, labels, title, color_hit, color_miss, xlabel):
        colors = [color_hit if v > 0 else color_miss for v in values]
        ax.barh(range(len(values)), values, color=colors)
        ax.set_yticks(range(len(labels)));  ax.set_yticklabels(labels, fontsize=7)
        ax.set_xlabel(xlabel, fontsize=7);  ax.set_title(title, fontsize=8)

    rpm_ss  = [df[df["rpm_bucket"]  == i]["ss_seconds"].sum()/3600 for i in range(n_rpm)]
    temp_ss = [df[df["temp_bucket"] == i]["ss_seconds"].sum()/3600 for i in range(n_temp)]
    fld_ss  = [df[df["field_bucket"]== i]["ss_seconds"].sum()/3600 for i in range(n_fld)]

    bar(fig.add_subplot(top[1]), rpm_ss,  rpm_labels,   "SS Time by RPM Bucket",  "#4a90d9", "#cccccc", "SS hours")
    bar(fig.add_subplot(top[2]), temp_ss, temp_labels,  "SS Time by Temp Bucket", "#e07b39", "#cccccc", "SS hours")
    bar(fig.add_subplot(top[3]), fld_ss,  field_labels, "SS Time by Field Bucket","#5aab61", "#cccccc", "SS hours")

    # ── section divider ──────────────────────────────────────────────────────
    ax_div = fig.add_subplot(outer[1]);  ax_div.axis("off")
    ax_div.text(0.5, 0.5,
                "X = Field Voltage Bucket  ·  Y = Temperature Bucket  "
                "·  one panel per RPM bucket  ·  gray = unvisited  ·  green border = reference bin",
                ha="center", va="center", fontsize=8, color="#555555")

    # ── ss_seconds heatmaps ──────────────────────────────────────────────────
    ss_pop  = df[df["populated"]]["ss_seconds"]
    ss_vmax = float(ss_pop.max()) if len(ss_pop) else 1.0
    ss_gs   = gridspec.GridSpecFromSubplotSpec(1, n_rpm, subplot_spec=outer[2], wspace=0.35)
    ss_axes = [fig.add_subplot(ss_gs[i]) for i in range(n_rpm)]
    fig.text(0.005, 0.50, "SS Seconds Accumulated", va="center", rotation=90,
             fontsize=8.5, fontweight="bold", color="#333")
    _draw_heatmap_row(ss_axes, df, n_rpm, rpm_labels, temp_labels, field_labels,
                      "ss_seconds", SS_CMAP, 0, ss_vmax, "seconds", mask_empty=False)

    # ── avg_amps heatmaps ────────────────────────────────────────────────────
    amp_pop  = df[df["populated"]]["avg_amps"]
    amp_vmin = float(amp_pop.min()) if len(amp_pop) else 0.0
    amp_vmax = float(amp_pop.max()) if len(amp_pop) else 1.0
    amp_gs   = gridspec.GridSpecFromSubplotSpec(1, n_rpm, subplot_spec=outer[3], wspace=0.35)
    amp_axes = [fig.add_subplot(amp_gs[i]) for i in range(n_rpm)]
    fig.text(0.005, 0.25, "Average Output Amps", va="center", rotation=90,
             fontsize=8.5, fontweight="bold", color="#333")
    _draw_heatmap_row(amp_axes, df, n_rpm, rpm_labels, temp_labels, field_labels,
                      "avg_amps", AMP_CMAP, amp_vmin, amp_vmax, "amps", mask_empty=True)

    fig.tight_layout(rect=[0.015, 0, 1, 0.997])
    return fig


# ─────────────────────────────────────────────────────────────────────────────
# Main application window
# ─────────────────────────────────────────────────────────────────────────────
class MatrixViewer:
    def __init__(self, df, fname):
        self.df    = df
        self.fname = fname

        self.rpm_labels   = sorted_labels(df, "rpm_bucket",   "rpm_label")
        self.temp_labels  = sorted_labels(df, "temp_bucket",  "temp_label")
        self.field_labels = sorted_labels(df, "field_bucket", "field_label")
        self.n_rpm  = len(self.rpm_labels)
        self.n_temp = len(self.temp_labels)
        self.n_fld  = len(self.field_labels)
        self.field_centers = [field_center(l) for l in self.field_labels]

        # Color per RPM bucket: blue (slow) → red (fast)
        self.rpm_colors = [RPM_LINE_CMAP(i / max(self.n_rpm - 1, 1))
                           for i in range(self.n_rpm)]

        # ── main window ──────────────────────────────────────────────────────
        self.root = tk.Tk()
        try:
            self.root.tk.call("::tk::unsupported::MacWindowStyle", "appearance",
                              self.root._w, "NSAppearanceNameAqua")
        except Exception:
            pass
        self.root.title(f"Alternator Health Matrix — {os.path.basename(fname)}")
        self.root.geometry("1400x900")
        self.root.protocol("WM_DELETE_WINDOW", self._on_close)

        nb = ttk.Notebook(self.root)
        nb.pack(fill="both", expand=True)

        # ── Tab 1: Overview ──────────────────────────────────────────────────
        tab1 = ttk.Frame(nb)
        nb.add(tab1, text="   Overview   ")
        self._build_overview_tab(tab1)

        # ── Tab 2: Field Curve ───────────────────────────────────────────────
        tab2 = ttk.Frame(nb)
        nb.add(tab2, text="   Field Curve   ")
        self._build_field_curve_tab(tab2)

        self.root.mainloop()

    def _on_close(self):
        plt.close("all")
        self.root.destroy()

    # ── Tab 1 ────────────────────────────────────────────────────────────────
    def _build_overview_tab(self, parent):
        fig    = build_overview_figure(self.df, self.fname)
        canvas = FigureCanvasTkAgg(fig, master=parent)
        canvas.draw()
        toolbar = NavigationToolbar2Tk(canvas, parent)
        toolbar.update()
        toolbar.pack(side="top", fill="x")
        canvas.get_tk_widget().pack(fill="both", expand=True)

    # ── Tab 2 ────────────────────────────────────────────────────────────────
    def _build_field_curve_tab(self, parent):
        # Figure area
        fig_frame = tk.Frame(parent)
        fig_frame.pack(fill="both", expand=True)

        self.fc_fig, self.fc_ax = plt.subplots(figsize=(13, 6), tight_layout=True)
        self.fc_canvas = FigureCanvasTkAgg(self.fc_fig, master=fig_frame)
        self.fc_canvas.draw()
        fc_toolbar = NavigationToolbar2Tk(self.fc_canvas, fig_frame)
        fc_toolbar.update()
        fc_toolbar.pack(side="top", fill="x")
        self.fc_canvas.get_tk_widget().pack(fill="both", expand=True)

        # Controls strip
        ctrl = tk.Frame(parent, bg="#ebebeb", relief="ridge", bd=1)
        ctrl.pack(fill="x", padx=6, pady=(2, 6))

        tk.Label(ctrl, text="Filters", bg="#ebebeb",
                 font=("Helvetica", 9, "bold")).grid(
            row=0, column=0, columnspan=10, sticky="w", padx=8, pady=(5, 2))

        self.rpm_lo  = tk.IntVar(value=0)
        self.rpm_hi  = tk.IntVar(value=self.n_rpm - 1)
        self.temp_lo = tk.IntVar(value=0)
        self.temp_hi = tk.IntVar(value=self.n_temp - 1)
        self.min_ss  = tk.IntVar(value=0)
        self.show_bands = tk.BooleanVar(value=True)

        def redraw(*_):
            self._redraw_field_curve()

        def slider(row, col, label, var, lo, hi):
            tk.Label(ctrl, text=label, bg="#ebebeb", font=("Helvetica", 8),
                     anchor="e", width=11).grid(row=row, column=col, sticky="e", padx=(8, 2))
            tk.Scale(ctrl, variable=var, from_=lo, to=hi, orient="horizontal",
                     length=170, showvalue=True, command=redraw,
                     bg="#ebebeb", highlightthickness=0,
                     font=("Helvetica", 7)).grid(row=row, column=col+1, padx=2, pady=2)

        slider(1, 0, "RPM min bucket:",  self.rpm_lo,  0, self.n_rpm  - 1)
        slider(1, 2, "RPM max bucket:",  self.rpm_hi,  0, self.n_rpm  - 1)
        slider(2, 0, "Temp min bucket:", self.temp_lo, 0, self.n_temp - 1)
        slider(2, 2, "Temp max bucket:", self.temp_hi, 0, self.n_temp - 1)
        slider(3, 0, "Min SS seconds:",  self.min_ss,  0, 600)

        tk.Checkbutton(ctrl, text="Show min/max band",
                       variable=self.show_bands, command=redraw,
                       bg="#ebebeb", font=("Helvetica", 8)).grid(
            row=3, column=2, columnspan=2, padx=12, sticky="w")

        # Live label showing current bucket names
        self.rpm_sel_lbl  = tk.Label(ctrl, text="", bg="#ebebeb",
                                     font=("Courier", 8), fg="#0055aa")
        self.rpm_sel_lbl.grid(row=1, column=4, columnspan=4, sticky="w", padx=8)
        self.temp_sel_lbl = tk.Label(ctrl, text="", bg="#ebebeb",
                                     font=("Courier", 8), fg="#aa5500")
        self.temp_sel_lbl.grid(row=2, column=4, columnspan=4, sticky="w", padx=8)

        self._redraw_field_curve()

    def _redraw_field_curve(self):
        rpm_lo  = self.rpm_lo.get()
        rpm_hi  = max(self.rpm_lo.get(), self.rpm_hi.get())
        temp_lo = self.temp_lo.get()
        temp_hi = max(self.temp_lo.get(), self.temp_hi.get())
        min_ss  = self.min_ss.get()
        bands   = self.show_bands.get()

        self.rpm_sel_lbl.config(
            text=f"→ {self.rpm_labels[rpm_lo]}  …  {self.rpm_labels[rpm_hi]}")
        self.temp_sel_lbl.config(
            text=f"→ {self.temp_labels[temp_lo]}  …  {self.temp_labels[temp_hi]}")

        ax = self.fc_ax
        ax.cla()

        df       = self.df
        x_all    = self.field_centers
        n_lines  = 0
        handles  = []

        for r in range(rpm_lo, rpm_hi + 1):
            for t in range(temp_lo, temp_hi + 1):
                sub = df[
                    (df["rpm_bucket"] == r) &
                    (df["temp_bucket"] == t) &
                    df["populated"] &
                    (df["ss_seconds"] >= max(1, min_ss))
                ].sort_values("field_bucket")

                if sub.empty:
                    continue

                x_pts = [x_all[int(f)] for f in sub["field_bucket"]]
                y_avg = sub["avg_amps"].values
                y_min = sub["min_amps"].values
                y_max = sub["max_amps"].values

                color  = self.rpm_colors[r]
                marker = TEMP_MARKERS[t % len(TEMP_MARKERS)]
                lbl    = f"{self.rpm_labels[r]} RPM · {self.temp_labels[t]}"

                ln, = ax.plot(x_pts, y_avg, color=color, marker=marker,
                              linewidth=1.8, markersize=7, label=lbl, zorder=3)
                handles.append(ln)

                if bands:
                    ax.fill_between(x_pts, y_min, y_max, color=color, alpha=0.13, zorder=2)

                n_lines += 1

        ax.set_xlabel("Field Voltage (V)", fontsize=11)
        ax.set_ylabel("Alternator Output (A)", fontsize=11)

        title = (f"Output Amps vs Field Voltage  ·  "
                 f"RPM {self.rpm_labels[rpm_lo]}–{self.rpm_labels[rpm_hi]}  ·  "
                 f"Temp {self.temp_labels[temp_lo]}–{self.temp_labels[temp_hi]}")
        if min_ss > 0:
            title += f"  ·  SS ≥ {min_ss}s"
        ax.set_title(title, fontsize=9)

        if n_lines > 0:
            ax.legend(handles=handles, loc="upper left", fontsize=7,
                      ncol=max(1, n_lines // 8),
                      framealpha=0.9, borderpad=0.5)
        else:
            ax.text(0.5, 0.5,
                    "No data in selected range.\nTry widening bucket sliders or reducing Min SS seconds.",
                    ha="center", va="center", transform=ax.transAxes,
                    fontsize=12, color="#888888")

        ax.grid(True, linestyle="--", alpha=0.45)
        ax.set_xlim(left=0)
        ax.set_ylim(bottom=0)
        self.fc_canvas.draw_idle()


# ─────────────────────────────────────────────────────────────────────────────
# Entry point
# ─────────────────────────────────────────────────────────────────────────────
if __name__ == "__main__":
    path = pick_file()
    if not path:
        raise SystemExit

    df = load_matrix(path)
    if df.empty:
        print("File loaded but contains no rows.")
        raise SystemExit

    MatrixViewer(df, path)
