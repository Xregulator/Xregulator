"""
plotlayout.py — tile all open matplotlib (TkAgg) figure windows into a grid so
the multi-plot analyzers stop opening full-screen stacked on top of each other.

Call tile_figures() once, right before plt.show(). The grid auto-sizes to the
number of open figures so none overlap:
    1-4 -> 2x2 (true quadrants),  5-6 -> 3x2,  7-9 -> 3x3,  10-12 -> 4x3.
Pass cols/rows to force a specific layout (e.g. tile_figures(cols=2, rows=2)).

No-op on a non-Tk backend or when no windows exist, so it is always safe to call.
"""

import matplotlib.pyplot as plt


def _grid_for(n):
    if n <= 1:
        return 1, 1
    if n <= 2:
        return 2, 1
    if n <= 4:
        return 2, 2
    if n <= 6:
        return 3, 2
    if n <= 9:
        return 3, 3
    if n <= 12:
        return 4, 3
    return 4, 4


def tile_figures(cols=None, rows=None, top_margin=28, bottom_margin=0, title_bar=30):
    nums = plt.get_fignums()
    if not nums:
        return
    figs = [plt.figure(n) for n in nums]
    try:
        w0 = figs[0].canvas.manager.window
        w0.update_idletasks()
        sw = w0.winfo_screenwidth()
        sh = w0.winfo_screenheight()
    except Exception:
        return  # not a Tk window manager (headless / different backend)

    if not cols or not rows:
        gc, gr = _grid_for(len(figs))
        cols = cols or gc
        rows = rows or gr

    cell_w = sw // cols
    cell_h = (sh - top_margin - bottom_margin) // rows

    for i, fig in enumerate(figs):
        c = i % cols
        r = (i // cols) % rows
        x = c * cell_w
        y = top_margin + r * cell_h
        try:
            fig.canvas.manager.window.wm_geometry(
                "%dx%d+%d+%d" % (cell_w - 12, cell_h - title_bar, x, y))
        except Exception:
            pass
