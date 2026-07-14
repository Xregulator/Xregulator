"""
filepicker.py — shared log-file picker for the post-processing scripts.

pick_file(prefix="cvlog_", title="Select CV Log", ...) returns a full path or None.

Behavior the individual plot_*.py scripts rely on:
  - Opens raised above every other window (macOS tk windows otherwise open buried).
  - A mini folder browser: starts in ~/Downloads, [..] ascends, double-click / Return
    descends into a subfolder, "Browse…" jumps anywhere via the native dir dialog.
  - Only files whose name starts with `prefix` are listed (case-insensitive). The
    "Show all CSVs" box drops the prefix filter for the current folder.
  - Files are sorted newest-first by CREATION time (st_birthtime on macOS), and each
    row shows that timestamp so the sort order is visible.
"""

import os
import datetime
import tkinter as tk
from tkinter import messagebox, filedialog

DOWNLOADS = os.path.expanduser("~/Downloads")
_LASTDIR_FILE = os.path.expanduser("~/.xreg_picker_lastdir")


def _load_lastdir():
    try:
        with open(_LASTDIR_FILE, encoding="utf-8") as f:
            d = f.read().strip()
        return d if os.path.isdir(d) else None
    except OSError:
        return None


def _save_lastdir(d):
    try:
        with open(_LASTDIR_FILE, "w", encoding="utf-8") as f:
            f.write(d)
    except OSError:
        pass


def _created(path):
    st = os.stat(path)
    return getattr(st, "st_birthtime", st.st_mtime)


def _bring_to_front(root):
    root.lift()
    root.attributes("-topmost", True)
    root.focus_force()
    root.after(500, lambda: root.attributes("-topmost", False))
    # tk on macOS launches behind the active app; nudge the whole process forward too.
    try:
        os.system(
            "osascript -e 'tell application \"System Events\" to set frontmost of "
            "first process whose unix id is %d to true' >/dev/null 2>&1" % os.getpid()
        )
    except Exception:
        pass


def _scan(folder, prefix, show_all, view):
    # view: "both" | "csv" (files only) | "dirs" (folders only)
    dirs, files = [], []
    try:
        for name in os.listdir(folder):
            if name.startswith("."):
                continue
            full = os.path.join(folder, name)
            if os.path.isdir(full):
                dirs.append(full)
            elif name.lower().endswith(".csv"):
                if show_all or not prefix or name.lower().startswith(prefix.lower()):
                    files.append(full)
    except (PermissionError, FileNotFoundError, OSError):
        pass
    dirs.sort(key=_created, reverse=True)
    files.sort(key=_created, reverse=True)
    if view == "csv":
        dirs = []
    elif view == "dirs":
        files = []
    return dirs, files


def pick_file(prefix="", title="Select Log", reminder=None, subreminder=None,
              optional=False, start_dir=None):
    state = {"dir": start_dir or _load_lastdir() or DOWNLOADS,
             "show_all": False, "view": "both", "entries": [], "result": None}
    if not os.path.isdir(state["dir"]):
        state["dir"] = DOWNLOADS

    root = tk.Tk()
    try:  # force light appearance on macOS regardless of system dark-mode setting
        root.tk.call("::tk::unsupported::MacWindowStyle", "appearance",
                     root._w, "NSAppearanceNameAqua")
    except Exception:
        pass
    root.title(title)
    root.resizable(False, False)
    root.configure(bg="#f5f5f5")

    tk.Label(root, text=reminder or "Select a log file:",
             font=("Helvetica", 16, "bold"),
             bg="#f5f5f5", fg="#1a1a1a").pack(padx=20, pady=(16, 2))
    if subreminder:
        tk.Label(root, text=subreminder, font=("Helvetica", 12),
                 bg="#f5f5f5", fg="#555555").pack(padx=20, pady=(0, 4))

    path_var = tk.StringVar()
    tk.Label(root, textvariable=path_var, font=("Courier", 12),
             bg="#f5f5f5", fg="#1565c0", anchor="w").pack(padx=20, pady=(0, 8), fill="x")

    frame = tk.Frame(root, bg="#f5f5f5")
    frame.pack(padx=20, pady=4)

    scrollbar = tk.Scrollbar(frame, orient=tk.VERTICAL)
    listbox = tk.Listbox(
        frame, yscrollcommand=scrollbar.set, width=78, height=18,
        font=("Courier", 14), selectmode=tk.SINGLE,
        bg="#ffffff", fg="#1a1a1a",
        selectbackground="#1565c0", selectforeground="#ffffff",
        highlightbackground="#cccccc", highlightcolor="#1565c0",
    )
    scrollbar.config(command=listbox.yview)
    scrollbar.pack(side=tk.RIGHT, fill=tk.Y)
    listbox.pack(side=tk.LEFT)

    def refresh():
        folder = state["dir"]
        _save_lastdir(folder)
        path_var.set(folder.replace(os.path.expanduser("~"), "~"))
        dirs, files = _scan(folder, prefix, state["show_all"], state["view"])
        entries = []
        listbox.delete(0, tk.END)

        parent = os.path.dirname(folder.rstrip(os.sep))
        if parent and parent != folder:
            entries.append(("up", parent))
            listbox.insert(tk.END, "[..]   parent folder")

        for d in dirs:
            stamp = datetime.datetime.fromtimestamp(_created(d)).strftime("%Y-%m-%d %H:%M")
            entries.append(("dir", d))
            listbox.insert(tk.END, "%s   [dir]  %s/" % (stamp, os.path.basename(d)))

        for f in files:
            stamp = datetime.datetime.fromtimestamp(_created(f)).strftime("%Y-%m-%d %H:%M")
            entries.append(("file", f))
            listbox.insert(tk.END, "%s   %s" % (stamp, os.path.basename(f)))

        state["entries"] = entries
        listbox.yview_moveto(0.0)   # newest is at the top; always show from the top
        # Land the selection on the first file (skip [..] and folders) so Enter opens it.
        first_file = next((i for i, e in enumerate(entries) if e[0] == "file"), None)
        if first_file is not None:
            listbox.selection_clear(0, tk.END)
            listbox.selection_set(first_file)

    def activate(idx):
        kind, target = state["entries"][idx]
        if kind in ("up", "dir"):
            state["dir"] = target
            state["view"] = "both"   # entering a folder: show its folders and CSVs
            refresh()
        else:
            state["result"] = target
            root.destroy()

    def on_open():
        idxs = listbox.curselection()
        if not idxs:
            messagebox.showwarning("No selection", "Please select a file or folder.")
            return
        activate(idxs[0])

    def on_double(_event):
        idxs = listbox.curselection()
        if idxs:
            activate(idxs[0])

    def on_browse():
        chosen = filedialog.askdirectory(initialdir=state["dir"], title="Go to folder")
        if chosen:
            state["dir"] = chosen
            state["view"] = "both"
            refresh()
        _bring_to_front(root)

    def on_csvs():
        state["dir"] = DOWNLOADS
        state["view"] = "csv"
        refresh()

    def on_folders():
        state["dir"] = DOWNLOADS
        state["view"] = "dirs"
        refresh()

    def on_toggle_all():
        state["show_all"] = bool(show_all_var.get())
        refresh()

    def on_cancel():
        root.destroy()

    listbox.bind("<Double-Button-1>", on_double)
    listbox.bind("<Return>", on_double)

    opt_frame = tk.Frame(root, bg="#f5f5f5")
    opt_frame.pack(padx=20, pady=(8, 0), fill="x")
    show_all_var = tk.IntVar(value=0)
    tk.Checkbutton(
        opt_frame,
        text=("Show all CSVs (ignore \"%s\" filter)" % prefix) if prefix else "Show all CSVs",
        variable=show_all_var, command=on_toggle_all,
        font=("Helvetica", 12), bg="#f5f5f5", fg="#1a1a1a",
        activebackground="#f5f5f5", selectcolor="#ffffff",
    ).pack(side=tk.LEFT)
    _lbl_browse = tk.Label(opt_frame, text="Browse…", font=("Helvetica", 12),
                           bg="#ffffff", fg="#1a1a1a", relief=tk.SOLID, bd=1,
                           padx=12, pady=4, cursor="hand2")
    _lbl_browse.bind("<Button-1>", lambda e: on_browse())
    _lbl_browse.pack(side=tk.RIGHT)
    _lbl_folders = tk.Label(opt_frame, text="Folders", font=("Helvetica", 12),
                            bg="#ffffff", fg="#1a1a1a", relief=tk.SOLID, bd=1,
                            padx=12, pady=4, cursor="hand2")
    _lbl_folders.bind("<Button-1>", lambda e: on_folders())
    _lbl_folders.pack(side=tk.RIGHT, padx=(0, 8))
    _lbl_csvs = tk.Label(opt_frame, text="CSV's", font=("Helvetica", 12),
                         bg="#ffffff", fg="#1a1a1a", relief=tk.SOLID, bd=1,
                         padx=12, pady=4, cursor="hand2")
    _lbl_csvs.bind("<Button-1>", lambda e: on_csvs())
    _lbl_csvs.pack(side=tk.RIGHT, padx=(0, 8))

    btn_frame = tk.Frame(root, bg="#f5f5f5")
    btn_frame.pack(pady=16)
    # tk.Label used instead of tk.Button — macOS ignores bg/fg on native buttons
    _lbl_open = tk.Label(btn_frame, text="Open", font=("Helvetica", 15, "bold"),
                         bg="#1565c0", fg="#ffffff", relief=tk.SOLID, bd=1,
                         padx=18, pady=8, width=10, cursor="hand2")
    _lbl_open.bind("<Button-1>", lambda e: on_open())
    _lbl_open.pack(side=tk.LEFT, padx=10)
    _lbl_cancel = tk.Label(btn_frame, text="Cancel", font=("Helvetica", 14),
                           bg="#ffffff", fg="#1a1a1a", relief=tk.SOLID, bd=1,
                           padx=18, pady=8, width=10, cursor="hand2")
    _lbl_cancel.bind("<Button-1>", lambda e: on_cancel())
    _lbl_cancel.pack(side=tk.LEFT, padx=10)

    refresh()
    _bring_to_front(root)
    root.mainloop()

    if state["result"] is None and not optional:
        pass  # caller decides what a None means; optional just suppresses nothing here
    return state["result"]
