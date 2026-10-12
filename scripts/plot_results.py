"""
plot_results.py  --  OPTIONAL graphs for the report / PPT / poster.

This is NOT part of the C program. It only reads the CSV files that
"energy.exe --export-all" writes into output/ and saves PNG images into
output/figures/.

Usage (from the project folder):
    energy.exe --export-all
    python scripts/plot_results.py

Needs: Python 3 + matplotlib.
"""
import csv
import math
import os
import sys

try:
    import matplotlib
    matplotlib.use("Agg")                  # draw to files, no window
    import matplotlib.pyplot as plt
    import matplotlib.ticker
except ImportError:
    sys.exit("matplotlib is not installed:  python -m pip install matplotlib")

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.normpath(os.path.join(HERE, "..", "output"))
DATA = os.path.normpath(os.path.join(HERE, "..", "data"))
FIG = os.path.join(OUT, "figures")

# Colours: validated categorical palette (fixed order, colour-blind safe on
# adjacent pairs), plus neutral inks for text, the true curve and reference lines.
SERIES = ["#2a78d6", "#eb6834", "#1baf7a", "#eda100"]   # blue, orange, aqua, yellow
INK = "#0b0b0b"          # primary text / true curve
INK2 = "#52514e"         # secondary text
MUTED = "#8a8985"        # reference lines, 1-minute data
GRID = "#e6e5e1"
SURFACE = "#fcfcfb"
MARKERS = ["o", "s", "^", "D"]            # second encoding besides colour

plt.rcParams.update({
    "figure.facecolor": SURFACE, "axes.facecolor": SURFACE, "savefig.facecolor": SURFACE,
    "axes.edgecolor": MUTED, "axes.labelcolor": INK2, "axes.titlecolor": INK,
    "xtick.color": INK2, "ytick.color": INK2, "text.color": INK,
    "axes.grid": True, "grid.color": GRID, "grid.linewidth": 0.8, "axes.axisbelow": True,
    "axes.spines.top": False, "axes.spines.right": False,
    "font.size": 10, "axes.titlesize": 12, "axes.titleweight": "bold",
    "legend.frameon": False, "lines.linewidth": 2, "lines.markersize": 6,
})


def read_csv(name):
    """Return the rows of output/<name> as a list of dicts (None if missing)."""
    path = os.path.join(OUT, name)
    if not os.path.exists(path):
        print("  skipped (not found):", path)
        return None
    with open(path, newline="") as f:
        return list(csv.DictReader(f))


def num(s):
    return float(s) if s not in ("", None) else math.nan


def hour_ticks(ax, start=0, end=24, step=3):
    ticks = list(range(int(start), int(end) + 1, step))
    ax.set_xticks(ticks)
    ax.set_xticklabels(["%02d:00" % t for t in ticks])
    ax.set_xlim(start, end)


def save(fig, name):
    path = os.path.join(FIG, name)
    fig.savefig(path, dpi=200, bbox_inches="tight")
    plt.close(fig)
    print("  wrote", os.path.relpath(path, os.path.join(OUT, "..")))


# ---------------------------------------------------------------------------
def fig_load_curve(tag, title, show_global, true_label):
    dense, rd = read_csv(tag + "/load_curve_dense.csv"), read_csv(tag + "/readings.csv")
    if not dense or not rd:
        return
    t = [num(r["time_h"]) for r in dense]
    fig, ax = plt.subplots(figsize=(10, 4.8))
    if tag == "real":
        # every 1-minute reading (the file has '#' comment lines before the header)
        with open(os.path.join(DATA, "real_day_minute.csv")) as f:
            lines = [ln for ln in f if not ln.startswith("#")][1:]
        tm = [float(ln.split(",")[0]) for ln in lines]
        pm = [float(ln.split(",")[1]) for ln in lines]
        ax.plot(tm, pm, color=MUTED, lw=0.8, label=true_label, zorder=1)
        top = max(pm) * 1.05
    else:
        ax.plot(t, [num(r["true_kW"]) for r in dense], color=INK, lw=2, label=true_label, zorder=1)
        top = 4.2
    ax.plot(t, [num(r["linear"]) for r in dense], color=SERIES[0], label="Linear", zorder=2)
    ax.plot(t, [num(r["lagrange"]) for r in dense], color=SERIES[1],
            label="Local cubic (Lagrange = NDD)", zorder=3)
    if show_global:
        ax.plot(t, [num(r["global"]) for r in dense], color=SERIES[2], lw=1.5,
                label="Global Lagrange (degree 19)", zorder=2)

    known = [r for r in rd if r["missing"] == "0"]
    miss = [r for r in rd if r["missing"] == "1"]
    ax.scatter([num(r["time_h"]) for r in known], [num(r["reading_kW"]) for r in known],
               s=36, color=INK, edgecolor=SURFACE, linewidth=1.5, zorder=5, label="Meter reading")
    ax.scatter([num(r["time_h"]) for r in miss], [num(r["true_kW"]) for r in miss],
               s=60, facecolor=SURFACE, edgecolor=INK, linewidth=1.5, zorder=5,
               label="Lost reading (true value)")
    ax.axhline(3.0, color=INK2, lw=1.2, ls="--", zorder=1)
    ax.text(0.3, 3.08, "3 kW limit", color=INK2, fontsize=9, va="bottom")

    ax.set_ylim(0, top)
    if show_global:
        ax.text(12, top * 0.93, "Global Lagrange leaves the chart near 00:00 and 24:00 (Runge's phenomenon)",
                ha="center", color=INK2, fontsize=9)
    hour_ticks(ax)
    ax.set_ylabel("Power (kW)")
    ax.set_title(title, loc="left")
    ax.legend(loc="upper left", bbox_to_anchor=(0, -0.12), ncol=3, fontsize=9)
    save(fig, "fig_load_curve_%s.png" % tag)


def fig_convergence():
    rows = read_csv("synthetic/integration_convergence.csv")
    if not rows:
        return
    h = [num(r["h_hours"]) for r in rows]
    fig, ax = plt.subplots(figsize=(7, 4.8))
    for i, (key, name) in enumerate([("trap_error", "Trapezoidal"), ("simp13_error", "Simpson 1/3"),
                                     ("simp38_error", "Simpson 3/8")]):
        e = [abs(num(r[key])) for r in rows]
        ax.loglog(h, e, color=SERIES[i], marker=MARKERS[i], label=name)
    # reference slopes drawn through the smallest-h points
    e_t = abs(num(rows[-1]["trap_error"]))
    e_s = abs(num(rows[-1]["simp13_error"]))
    hs = [h[-1], h[1]]
    ax.loglog(hs, [e_t * (x / h[-1]) ** 2 for x in hs], color=MUTED, ls=":", lw=1.2)
    ax.loglog(hs, [e_s * (x / h[-1]) ** 4 for x in hs], color=MUTED, ls=":", lw=1.2)
    ax.text(hs[1], e_t * (hs[1] / h[-1]) ** 2 * 0.45, "slope 2 (h²)", color=INK2, fontsize=9, va="top")
    ax.text(hs[1], e_s * (hs[1] / h[-1]) ** 4 * 1.6, "slope 4 (h⁴)", color=INK2, fontsize=9)
    ax.set_xticks(h)
    ax.set_xticklabels(["%g" % x for x in h])
    ax.minorticks_off()
    ax.set_xlabel("Step size h (hours)")
    ax.set_ylabel("|Energy error| (kWh)")
    ax.set_title("Integration error vs step size (synthetic day)", loc="left")
    ax.legend(loc="lower right", fontsize=9)
    save(fig, "fig_integration_convergence.png")


def fig_sampling_real():
    rows = read_csv("real/integration_convergence.csv")
    if not rows:
        return
    step = [num(r["step_minutes"]) for r in rows]
    fig, ax = plt.subplots(figsize=(7, 4.5))
    for i, (key, name) in enumerate([("trap_error_pct", "Trapezoidal"), ("simp13_error_pct", "Simpson 1/3"),
                                     ("simp38_error_pct", "Simpson 3/8")]):
        ax.plot(step, [abs(num(r[key])) for r in rows], color=SERIES[i], marker=MARKERS[i], label=name)
    ax.set_xscale("log")
    ax.set_xticks(step)
    ax.set_xticklabels(["%d" % s for s in step])
    ax.minorticks_off()
    ax.invert_xaxis()
    ax.set_xlabel("Meter reads every ... minutes")
    ax.set_ylabel("|Energy error| (% of metered)")
    ax.set_title("Real household: error vs how often the meter reads", loc="left")
    ax.legend(fontsize=9)
    save(fig, "fig_sampling_real.png")


def fig_root_iterations(tag):
    rows = read_csv(tag + "/root_iterations.csv")
    if not rows:
        return
    event = rows[0]["event"]               # first crossing
    fig, ax = plt.subplots(figsize=(7, 4.5))
    for i, name in enumerate(["Bisection", "Regula Falsi", "Newton-Raphson", "Secant"]):
        pts = [(int(r["k"]), max(num(r["abs_error_h"]), 1e-16)) for r in rows
               if r["event"] == event and r["method"] == name]
        if not pts:
            continue
        ax.semilogy([p[0] for p in pts], [p[1] for p in pts], color=SERIES[i], marker=MARKERS[i],
                    label="%s (%d iterations)" % (name, len(pts)))
    ax.axhline(1e-6, color=INK2, ls="--", lw=1.2)
    ax.text(7, 1.5e-6, "tolerance 10⁻⁶ h", color=INK2, fontsize=9)
    ax.xaxis.set_major_locator(matplotlib.ticker.MaxNLocator(integer=True))
    ax.set_xlabel("Iteration k")
    ax.set_ylabel("|x_k − x*| (hours)")
    ax.set_title("Root finding: error per iteration (%s, %s)" % (tag, event.replace("_", " ")), loc="left")
    ax.legend(fontsize=9)
    save(fig, "fig_root_iterations_%s.png" % tag)


def fig_energy_chain(tag):
    rows = read_csv(tag + "/integration_chain.csv")
    if not rows:
        return
    rows = [r for r in rows if not r["filled_with"].startswith("Newton")]   # NDD == Lagrange
    labels = [r["filled_with"].replace(" (local cubic)", "\n(local cubic)").replace(" (no gaps)", "\n(no gaps)")
              for r in rows]
    fig, ax = plt.subplots(figsize=(8.5, 4.5))
    width = 0.26
    for i, (key, name) in enumerate([("trap_error_kWh", "Trapezoidal"), ("simp13_error_kWh", "Simpson 1/3"),
                                     ("simp38_error_kWh", "Simpson 3/8")]):
        xs = [k + (i - 1) * (width + 0.02) for k in range(len(rows))]
        ax.bar(xs, [num(r[key]) for r in rows], width, color=SERIES[i], label=name,
               edgecolor=SURFACE, linewidth=1)
    ax.axhline(0, color=INK2, lw=1)
    ax.set_xticks(range(len(rows)))
    ax.set_xticklabels(labels, fontsize=9)
    ax.grid(axis="x", visible=False)
    ax.set_ylabel("Full-day energy error (kWh)")
    ax.set_title("Error through the chain: gap filling × integration rule (%s)" % tag, loc="left")
    ax.legend(fontsize=9, ncol=3, loc="upper left", bbox_to_anchor=(0, -0.18))
    save(fig, "fig_energy_chain_%s.png" % tag)


def fig_loocv():
    data = {}
    for tag in ("synthetic", "real"):
        rows = read_csv(tag + "/interpolation_loocv.csv")
        if not rows:
            return
        out = {}
        for key in ("linear", "lagrange", "global"):
            err = [num(r[key]) - num(r["actual_kW"]) for r in rows]
            out[key] = math.sqrt(sum(e * e for e in err) / len(err))
        data[tag] = out
    fig, ax = plt.subplots(figsize=(6.5, 4.2))
    width = 0.34
    for i, (key, name) in enumerate([("linear", "Linear"), ("lagrange", "Local cubic (Lagrange = NDD)")]):
        xs = [k + (i - 0.5) * (width + 0.02) for k in range(2)]
        vals = [data["synthetic"][key], data["real"][key]]
        ax.bar(xs, vals, width, color=SERIES[i], label=name, edgecolor=SURFACE)
        for x, v in zip(xs, vals):
            ax.text(x, v, "%.3f" % v, ha="center", va="bottom", fontsize=9, color=INK2)
    ax.set_xticks([0, 1])
    ax.set_xticklabels(["Synthetic day\n(smooth)", "Real day\n(spiky)"])
    ax.grid(axis="x", visible=False)
    ax.set_ylabel("Leave-one-out RMSE (kW)")
    ax.set_title("Interpolation accuracy: leave-one-out test", loc="left")
    ax.legend(fontsize=9)
    ax.text(0.5, -0.25, "Global Lagrange not shown (off-scale): RMSE %.0f kW (synthetic), %.0f kW (real)"
            % (data["synthetic"]["global"], data["real"]["global"]),
            transform=ax.transAxes, ha="center", fontsize=8.5, color=INK2)
    save(fig, "fig_loocv.png")


def main():
    if not os.path.isdir(os.path.join(OUT, "synthetic")):
        sys.exit("Run 'energy.exe --export-all' first (output/synthetic not found).")
    os.makedirs(FIG, exist_ok=True)
    fig_load_curve("synthetic", "Synthetic Delhi household: lost readings and interpolation",
                   True, "True load P(t)")
    fig_load_curve("real", "Real household (UCI, 14-May-2007): hourly readings vs 1-minute data",
                   False, "1-minute meter data")
    fig_convergence()
    fig_sampling_real()
    fig_root_iterations("synthetic")
    fig_root_iterations("real")
    fig_energy_chain("synthetic")
    fig_energy_chain("real")
    fig_loocv()


if __name__ == "__main__":
    main()
