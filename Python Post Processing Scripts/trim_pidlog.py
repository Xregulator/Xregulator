"""
trim_pidlog.py
Time-range trimmer for any ESP32 regulator log CSV (pidlog, thermallog, cvlog, etc.).

1. File picker searches ~/Downloads for *.csv
2. Time-range dialog lets the user enter Start and End in seconds
   (relative to the first row of the file, same as t_s in the plotter)
3. All rows outside [start, end] are dropped.
4. Trimmed CSV is saved to ~/Downloads as:
       <originalbasename>_<start_s>s_<end_s>s.csv
   with the original header row preserved exactly.

Supports both time column formats:
  ts_ms  — pidlog / thermallog  (milliseconds, converted to seconds internally)
  t_s    — cvlog                (already in seconds)
"""

import glob
import os
import tkinter as tk
from tkinter import messagebox
from io import StringIO

import pandas as pd

DOWNLOADS = os.path.expanduser("~/Downloads")


# ---------------------------------------------------------------------------
# 1. File selector
# ---------------------------------------------------------------------------
def pick_file():
    files = sorted(
        glob.glob(os.path.join(DOWNLOADS, "*.csv")),
        reverse=True,
    )
    if not files:
        messagebox.showerror("No files", f"No *.csv files found in {DOWNLOADS}")
        return None

    selected = []

    root = tk.Tk()
    root.title("Select Log to Trim")
    root.resizable(False, False)
    root.configure(bg="#1e1e1e")

    tk.Label(
        root,
        text="Select a log file:",
        font=("Helvetica", 16, "bold"),
        bg="#1e1e1e",
        fg="#f0f0f0",
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
        highlightcolor="#42a5f5",
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
        relief=tk.FLAT, bd=0, padx=18, pady=8, width=10,
    ).pack(side=tk.LEFT, padx=10)

    tk.Button(
        btn_frame, text="Cancel",
        font=("Helvetica", 14), command=on_cancel,
        bg="#3a3a3a", fg="#f0f0f0",
        activebackground="#555555", activeforeground="#ffffff",
        relief=tk.FLAT, bd=0, padx=18, pady=8, width=10,
    ).pack(side=tk.LEFT, padx=10)

    root.mainloop()
    return selected[0] if selected else None


# ---------------------------------------------------------------------------
# 2. Time-range dialog
# ---------------------------------------------------------------------------
def pick_time_range(total_s: float):
    """
    Show a dialog with Start / End fields pre-filled to 0 / total_s.
    Returns (start_s, end_s) as floats, or (None, None) on cancel.
    """
    result = [None, None]

    win = tk.Tk()
    win.title("Select Time Range")
    win.resizable(False, False)
    win.configure(bg="#1e1e1e")

    tk.Label(
        win,
        text="Trim Time Range",
        font=("Helvetica", 17, "bold"),
        bg="#1e1e1e",
        fg="#f0f0f0",
    ).pack(padx=30, pady=(18, 4))

    tk.Label(
        win,
        text=f"File duration: {total_s:.2f} s",
        font=("Helvetica", 13),
        bg="#1e1e1e",
        fg="#aaaaaa",
    ).pack(pady=(0, 14))

    field_frame = tk.Frame(win, bg="#1e1e1e")
    field_frame.pack(padx=30, pady=4)

    def _label(text, row):
        tk.Label(
            field_frame, text=text,
            font=("Helvetica", 14),
            bg="#1e1e1e", fg="#f0f0f0",
            anchor="e", width=14,
        ).grid(row=row, column=0, padx=(0, 10), pady=8, sticky="e")

    def _entry(default, row):
        var = tk.StringVar(value=str(default))
        e = tk.Entry(
            field_frame,
            textvariable=var,
            font=("Courier", 14),
            width=12,
            bg="#111111", fg="#f0f0f0",
            insertbackground="#f0f0f0",
            relief=tk.FLAT,
            highlightbackground="#555555",
            highlightthickness=1,
        )
        e.grid(row=row, column=1, pady=8, sticky="w")
        return var

    _label("Start (seconds):", 0)
    _label("End (seconds):", 1)

    var_start = _entry(0.0, 0)
    var_end   = _entry(round(total_s, 3), 1)

    tk.Label(
        field_frame, text="seconds",
        font=("Helvetica", 13), bg="#1e1e1e", fg="#888888",
    ).grid(row=0, column=2, padx=6)
    tk.Label(
        field_frame, text="seconds",
        font=("Helvetica", 13), bg="#1e1e1e", fg="#888888",
    ).grid(row=1, column=2, padx=6)

    def on_trim():
        try:
            s = float(var_start.get())
            e = float(var_end.get())
        except ValueError:
            messagebox.showerror("Bad input", "Start and End must be numbers.")
            return
        if s >= e:
            messagebox.showerror("Bad range", "Start must be less than End.")
            return
        if s < 0:
            messagebox.showerror("Bad range", "Start cannot be negative.")
            return
        result[0] = s
        result[1] = e
        win.destroy()

    def on_cancel():
        win.destroy()

    btn_frame = tk.Frame(win, bg="#1e1e1e")
    btn_frame.pack(pady=18)

    tk.Button(
        btn_frame, text="Trim & Save",
        font=("Helvetica", 15, "bold"), command=on_trim,
        bg="#42a5f5", fg="#ffffff",
        activebackground="#1e88e5", activeforeground="#ffffff",
        relief=tk.FLAT, bd=0, padx=18, pady=8, width=12,
    ).pack(side=tk.LEFT, padx=10)

    tk.Button(
        btn_frame, text="Cancel",
        font=("Helvetica", 14), command=on_cancel,
        bg="#3a3a3a", fg="#f0f0f0",
        activebackground="#555555", activeforeground="#ffffff",
        relief=tk.FLAT, bd=0, padx=18, pady=8, width=10,
    ).pack(side=tk.LEFT, padx=10)

    win.mainloop()
    return tuple(result)


# ---------------------------------------------------------------------------
# 3. Robust CSV loader — handles pidlog/thermallog (ts_ms) and cvlog (t_s)
# ---------------------------------------------------------------------------
def load_csv(path):
    with open(path, encoding="utf-8", errors="replace") as f:
        lines = f.readlines()

    header_idx  = None
    header_line = None
    time_col    = None

    for i, raw in enumerate(lines):
        if "ts_ms" in raw:
            header_idx  = i
            header_line = raw[raw.find("ts_ms"):].strip()
            time_col    = "ts_ms"
            break
        if "t_s" in raw and "battV" in raw:
            header_idx  = i
            header_line = raw[raw.find("t_s"):].strip()
            time_col    = "t_s"
            break

    if header_idx is None:
        raise SystemExit(
            f"ERROR: No recognised header found in {path}.\n"
            "Expected a line containing 'ts_ms' (pidlog/thermallog) "
            "or 't_s' + 'battV' (cvlog)."
        )

    _sep = "\t" if "\t" in header_line else ","
    col_names = [c.strip() for c in header_line.split(_sep)]
    data_text = "".join(lines[header_idx + 1:])
    df = pd.read_csv(StringIO(data_text), sep=_sep, names=col_names, on_bad_lines="skip")

    df[time_col] = pd.to_numeric(df[time_col], errors="coerce")
    df.dropna(subset=[time_col], inplace=True)
    df.reset_index(drop=True, inplace=True)

    # t_s: zero-referenced seconds — synthesized from ts_ms or zero-referenced from t_s
    if time_col == "ts_ms":
        df["t_s"] = (df["ts_ms"] - df["ts_ms"].iloc[0]) / 1000.0
    else:
        df["t_s"] = df["t_s"] - df["t_s"].iloc[0]

    return df, col_names, header_line, lines[:header_idx]   # also return preamble lines


# ---------------------------------------------------------------------------
# 4. Main
# ---------------------------------------------------------------------------
path = pick_file()
if not path:
    raise SystemExit("No file selected.")

basename_full = os.path.basename(path)
basename      = os.path.splitext(basename_full)[0]
print(f"Loading: {path}")

df, col_names, header_line, preamble_lines = load_csv(path)

total_s = df["t_s"].iloc[-1]
print(f"File duration: {total_s:.3f} s  ({len(df)} rows)")

start_s, end_s = pick_time_range(total_s)
if start_s is None:
    raise SystemExit("Cancelled.")

print(f"Trimming to [{start_s:.3f} s  →  {end_s:.3f} s]")

# ---------------------------------------------------------------------------
# 5. Filter rows
# ---------------------------------------------------------------------------
mask    = (df["t_s"] >= start_s) & (df["t_s"] <= end_s)
df_trim = df[mask].copy()

if df_trim.empty:
    messagebox.showerror(
        "No data",
        f"No rows found between {start_s} s and {end_s} s.\n"
        f"File duration is {total_s:.2f} s.",
    )
    raise SystemExit("Empty result — nothing saved.")

print(f"Kept {len(df_trim)} of {len(df)} rows.")

# ---------------------------------------------------------------------------
# 6. Build output filename and save
#    Format: originalname_<start>s_<end>s.csv
#    Times are written without the decimal point if they are whole numbers.
# ---------------------------------------------------------------------------
def fmt_t(t):
    return f"{int(t)}" if t == int(t) else f"{t:.3f}".rstrip("0").rstrip(".")

out_name = f"{basename}_{fmt_t(start_s)}s_{fmt_t(end_s)}s.csv"
out_path = os.path.join(DOWNLOADS, out_name)

# Write: preserve any preamble comment lines, then the header, then data.
# to_csv(columns=col_names) ensures only original columns are written —
# the synthesized t_s helper is automatically excluded for ts_ms-based logs.
with open(out_path, "w", encoding="utf-8", newline="") as f:
    # Original comment/preamble lines (if any)
    for line in preamble_lines:
        f.write(line)
    # Header row exactly as found in the source file
    f.write(header_line + "\n")
    # Data rows
    df_trim.to_csv(f, index=False, header=False, columns=col_names)

print(f"Saved: {out_path}")

# ---------------------------------------------------------------------------
# 7. Confirm dialog
# ---------------------------------------------------------------------------
root = tk.Tk()
root.withdraw()
messagebox.showinfo(
    "Done",
    f"Trimmed file saved:\n\n{out_name}\n\n"
    f"{len(df_trim)} rows  |  {fmt_t(start_s)} s → {fmt_t(end_s)} s",
)
root.destroy()
