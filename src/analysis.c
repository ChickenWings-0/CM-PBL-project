/*
 * analysis.c - the energy chain and all error-analysis experiments.
 *
 *   readings --(interpolation)--> curve P~(t) --(integration)--> energy
 *                                      |
 *                              (root finding)  P~(t) = 3 kW,  E~(t) = 15 kWh
 *
 * Every printed table goes through rprintf(), so the same code produces the
 * screen output AND the .txt files written by "Export all results".
 */
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "analysis.h"
#include "data_io.h"
#include "load_model.h"
#include "report.h"

#define MAX_CROSS 256           /* max crossings stored (1-min data has many) */

/* ======================================================================
 *  1. The continuous curve P~(t) and energy E~(t)
 *
 *  C has no "closures", so the root-finding functions f_power / f_energy
 *  read the curve data from these file-level (static) variables.
 *  curve_set() must be called before using them.
 * ====================================================================== */
static double g_kx[MAX_POINTS], g_ky[MAX_POINTS];   /* known readings      */
static int g_nk = 0;                                /* how many            */
static InterpMethod g_method = M_LAGRANGE;          /* curve type          */
static double g_threshold = 3.0, g_budget = 15.0;   /* targets             */

/* Default settings (PLAN.md section 5). */
Settings default_settings(void)
{
    Settings s;
    s.threshold = 3.0;          /* kW  - sanctioned-load style limit */
    s.budget = 15.0;            /* kWh - daily energy budget         */
    s.peak_start = 18.0;        /* 18:00                             */
    s.peak_end = 22.0;          /* 22:00                             */
    s.rate = 8.0;               /* Rs per kWh (flat)                 */
    s.fill = M_LAGRANGE;        /* local cubic                       */
    return s;
}

void curve_set(const Dataset *d, InterpMethod m)
{
    g_nk = known_points(d, g_kx, g_ky);
    g_method = m;
}

double curve_power(double t)
{
    return interp_eval(g_method, g_kx, g_ky, g_nk, t);
}

/*
 * Energy from the first reading up to time t.
 * On each interval [x_i, x_i+1] the local cubic is ONE cubic polynomial, and
 * Simpson's 1/3 rule (h/6)[f(lo) + 4 f(mid) + f(hi)] is exact for cubics.
 */
double curve_energy_upto(double t)
{
    double e = 0.0;
    int i;
    for (i = 0; i < g_nk - 1; i++) {
        double lo = g_kx[i], hi = g_kx[i + 1], mid;
        if (lo >= t) break;             /* interval starts after t: done   */
        if (hi > t) hi = t;             /* last, partial interval          */
        mid = 0.5 * (lo + hi);
        e += (hi - lo) / 6.0 * (curve_power(lo) + 4.0 * curve_power(mid) + curve_power(hi));
    }
    return e;
}

void set_targets(double threshold, double budget)
{
    g_threshold = threshold;
    g_budget = budget;
}

double f_power(double t)  { return curve_power(t) - g_threshold; }
double f_energy(double t) { return curve_energy_upto(t) - g_budget; }

/* ======================================================================
 *  2. Reference ("true") answers
 * ====================================================================== */
static double g_true_thr = 3.0, g_true_budget = 15.0, g_true_t0 = 0.0;

static double f_model_power(double t)  { return model_power(t) - g_true_thr; }
static double f_model_energy(double t) { return model_energy(g_true_t0, t) - g_true_budget; }

int has_reference(const Dataset *d)
{
    return d->has_model || d->has_fine;
}

double reference_energy(const Dataset *d, double a, double b)
{
    if (d->has_model) return model_energy(a, b);
    if (d->has_fine) {
        /* each 1-minute value is the AVERAGE power over that minute,
         * so the metered energy is sum of P_k * (duration of minute k) */
        double e = 0.0;
        int k;
        for (k = 0; k < d->n_fine - 1; k++)
            if (d->fine_t[k] >= a - 1e-9 && d->fine_t[k] < b - 1e-9)
                e += d->fine_p[k] * (d->fine_t[k + 1] - d->fine_t[k]);
        return e;
    }
    return 0.0;
}

int true_power_crossings(const Dataset *d, double threshold, double t_out[], int dir[], int max)
{
    int count = 0, k;
    if (d->has_model) {
        /* scan the exact model finely (0.01 h) and solve each bracket to 1e-12 */
        static double br[MAX_CROSS][2];
        int nb;
        g_true_thr = threshold;
        nb = scan_brackets(f_model_power, d->t[0], d->t[d->n - 1], 0.01, br, MAX_CROSS);
        for (k = 0; k < nb && count < max; k++) {
            RootResult r = bisection(f_model_power, br[k][0], br[k][1], 1e-12, 200);
            t_out[count] = r.root;
            dir[count] = (f_model_power(r.root + 0.01) > 0.0) ? +1 : -1;
            count++;
        }
        return count;
    }
    if (d->has_fine) {
        /* sign change between consecutive minutes -> straight-line crossing time */
        for (k = 0; k < d->n_fine - 1 && count < max; k++) {
            double p0 = d->fine_p[k] - threshold, p1 = d->fine_p[k + 1] - threshold;
            if (p0 * p1 < 0.0) {
                t_out[count] = d->fine_t[k] + (-p0) / (p1 - p0) * (d->fine_t[k + 1] - d->fine_t[k]);
                dir[count] = (p1 > p0) ? +1 : -1;
                count++;
            }
        }
        return count;
    }
    return -1;
}

double true_budget_time(const Dataset *d, double budget)
{
    double t0 = d->t[0], tn = d->t[d->n - 1];
    if (d->has_model) {
        g_true_budget = budget;
        g_true_t0 = t0;
        if (f_model_energy(tn) < 0.0) return -1.0;          /* never reached */
        return bisection(f_model_energy, t0, tn, 1e-12, 200).root;
    }
    if (d->has_fine) {
        /* energy grows linearly during each minute at rate P_k (kW) */
        double cum = 0.0;
        int k;
        for (k = 0; k < d->n_fine - 1; k++) {
            double dt = d->fine_t[k + 1] - d->fine_t[k];
            double next = cum + d->fine_p[k] * dt;
            if (next >= budget && d->fine_p[k] > 0.0)
                return d->fine_t[k] + (budget - cum) / d->fine_p[k];
            cum = next;
        }
        return -1.0;
    }
    return -1.0;
}

/* Index of reading time t in d, or -1 if t is not a reading time. */
static int index_of_time(const Dataset *d, double t)
{
    int i;
    for (i = 0; i < d->n; i++)
        if (fabs(d->t[i] - t) < 1e-9) return i;
    return -1;
}

/* Integrate values y[] (one per reading) between reading times a and b. */
double energy_of_array(const Dataset *d, const double y[], QuadMethod q,
                              double a, double b, int *status)
{
    int ia = index_of_time(d, a), ib = index_of_time(d, b);
    if (ia < 0 || ib < 0 || ib <= ia) {
        *status = 3;
        return 0.0;
    }
    return integrate_grid(q, d->t + ia, y + ia, ib - ia + 1, status);
}

double energy_with_fill(const Dataset *d, InterpMethod fill, QuadMethod q,
                        double a, double b, int *status)
{
    static double y[MAX_POINTS];
    fill_missing(d, fill, y);
    return energy_of_array(d, y, q, a, b, status);
}

/* ======================================================================
 *  3. Small helpers
 * ====================================================================== */
static FILE *open_csv(const char *dir, const char *name)
{
    char path[256];
    FILE *fp;
    if (dir == NULL) return NULL;
    sprintf(path, "%s/%s", dir, name);
    fp = fopen(path, "w");
    if (fp == NULL) printf("  WARNING: cannot write %s\n", path);
    return fp;
}

/* Mean absolute error, root-mean-square error and maximum error of err[0..n-1]. */
static void error_stats(const double err[], int n, double *mae, double *rmse, double *mx, int *imax)
{
    double sa = 0.0, sq = 0.0;
    int i;
    *mx = 0.0;
    *imax = 0;
    for (i = 0; i < n; i++) {
        double e = fabs(err[i]);
        sa += e;
        sq += e * e;
        if (e > *mx) {
            *mx = e;
            *imax = i;
        }
    }
    *mae = (n > 0) ? sa / n : 0.0;
    *rmse = (n > 0) ? sqrt(sq / n) : 0.0;
}

static const char *ref_name(const Dataset *d)
{
    if (d->has_model) return "Exact (erf formula)";
    if (d->has_fine) return "Metered (1-min data)";
    return "none";
}

/* Reference power at time t (model, or the 1-minute reading closest to t). */
double reference_power(const Dataset *d, double t)
{
    if (d->has_model) return model_power(t);
    {
        int k = (int)floor((t - d->fine_t[0]) * 60.0 + 0.5);
        if (k < 0) k = 0;
        if (k > d->n_fine - 1) k = d->n_fine - 1;
        return d->fine_p[k];
    }
}

/* ======================================================================
 *  4. Menu features
 * ====================================================================== */
void show_readings(const Dataset *d, const Settings *s)
{
    static double filled[MAX_POINTS];
    char buf[12];
    int i;

    print_title("Meter readings");
    rprintf("  Dataset : %s\n", d->name);
    rprintf("  Readings: %d (%d missing)   From %s to %s\n\n", d->n, count_missing(d),
            fmt_hhmm(d->t[0], buf), fmt_hhmm(d->t[d->n - 1], buf + 6));
    rprintf("  +-----+-------+------------+%s\n", d->has_truth ? "------------+" : "");
    rprintf("  | No. | Time  | Reading kW |%s\n", d->has_truth ? "  True kW   |" : "");
    rprintf("  +-----+-------+------------+%s\n", d->has_truth ? "------------+" : "");
    for (i = 0; i < d->n; i++) {
        if (d->missing[i])
            rprintf("  | %3d | %s |  -- NA --  |", i + 1, fmt_hhmm(d->t[i], buf));
        else
            rprintf("  | %3d | %s | %10.4f |", i + 1, fmt_hhmm(d->t[i], buf), d->p[i]);
        if (d->has_truth) rprintf(" %10.4f |", d->truth[i]);
        rprintf("\n");
    }
    rprintf("  +-----+-------+------------+%s\n", d->has_truth ? "------------+" : "");
    if (d->has_truth)
        rprintf("  'True kW' is the real value (known for this dataset, used only for error analysis).\n");

    fill_missing(d, s->fill, filled);
    ascii_chart(d, filled, s->threshold);
    if (count_missing(d) > 0)
        rprintf("  Missing readings drawn with estimates from: %s\n", INTERP_NAMES[s->fill]);
}

/* Prints the divided-difference table used at every missing reading. */
static void print_ndd_tables(const Dataset *d)
{
    static double kx[MAX_POINTS], ky[MAX_POINTS];
    double table[4][4];
    int nk = known_points(d, kx, ky), i, r, c;
    char buf[12];

    if (nk < 4) {
        rprintf("  (fewer than 4 known readings: no cubic tables)\n");
        return;
    }
    for (i = 0; i < d->n; i++) {
        double t, value, product = 1.0;
        const double *x;
        int s;
        if (!d->missing[i]) continue;
        t = d->t[i];
        s = local_stencil(kx, nk, t);
        x = kx + s;
        ndd_build_table(x, ky + s, 4, table);

        rprintf("\n  Missing reading at %s (t = %.2f h). Nearest known readings: t = %.2f, %.2f, %.2f, %.2f\n",
                fmt_hhmm(t, buf), t, x[0], x[1], x[2], x[3]);
        rprintf("  +---+--------+------------+------------+------------+------------+\n");
        rprintf("  | i |  x_i   |   f[x_i]   |  1st div.  |  2nd div.  |  3rd div.  |\n");
        rprintf("  +---+--------+------------+------------+------------+------------+\n");
        for (r = 0; r < 4; r++) {
            rprintf("  | %d | %6.2f |", r, x[r]);
            for (c = 0; c < 4; c++) {
                if (c < 4 - r) rprintf(" %10.6f |", table[r][c]);
                else rprintf("            |");
            }
            rprintf("\n");
        }
        rprintf("  +---+--------+------------+------------+------------+------------+\n");
        /* show the Newton form term by term, as done by hand */
        rprintf("  P(%.2f) = %.6f", t, table[0][0]);
        value = table[0][0];
        for (c = 1; c < 4; c++) {
            product *= (t - x[c - 1]);
            rprintf(" + (%.6f)(%.2f)", table[0][c], product);
            value += table[0][c] * product;
        }
        rprintf("\n          = %.6f kW\n", value);
    }
    rprintf("  (The Newton coefficients are the top row; products are (t-x0), (t-x0)(t-x1), ...)\n");
}

void show_fill_missing(const Dataset *d, int choice)
{
    static double kx[MAX_POINTS], ky[MAX_POINTS], est[MAX_POINTS];
    int nk = known_points(d, kx, ky), i;
    InterpMethod m;
    char buf[12];

    if (count_missing(d) == 0) {
        rprintf("\n  This dataset has no missing readings - nothing to fill.\n");
        return;
    }
    if (choice == N_INTERP) {           /* compare all methods */
        analysis_interpolation(d, NULL);
        return;
    }
    m = (InterpMethod)choice;
    fill_missing(d, m, est);

    print_title("Filling missing readings");
    rprintf("  Method: %s", INTERP_NAMES[m]);
    if (m == M_GLOBAL) rprintf(" (one polynomial of degree %d)", nk - 1);
    rprintf("\n\n");
    rprintf("  +-------+--------------+%s\n", d->has_truth ? "------------+------------+---------+" : "");
    rprintf("  | Time  | Estimate kW  |%s\n", d->has_truth ? "  True kW   | Abs. error | Error % |" : "");
    rprintf("  +-------+--------------+%s\n", d->has_truth ? "------------+------------+---------+" : "");
    for (i = 0; i < d->n; i++) {
        if (!d->missing[i]) continue;
        rprintf("  | %s | %12.4f |", fmt_hhmm(d->t[i], buf), est[i]);
        if (d->has_truth)
            rprintf(" %10.4f | %10.4f | %7.2f |", d->truth[i], fabs(est[i] - d->truth[i]),
                    100.0 * fabs(est[i] - d->truth[i]) / fabs(d->truth[i]));
        rprintf("\n");
    }
    rprintf("  +-------+--------------+%s\n", d->has_truth ? "------------+------------+---------+" : "");

    if (m == M_NDD) {
        print_title("Divided-difference tables");
        print_ndd_tables(d);
    }
    if (m == M_GLOBAL)
        rprintf("  NOTE: global Lagrange is shown only to demonstrate Runge's phenomenon.\n"
                "        It is NOT used to fill data for the other menu options.\n");
}

/* Energy table for the full day and the peak window, printed under 'title'. */
static void energy_table(const Dataset *d, const Settings *s, const char *title)
{
    double t0 = d->t[0], tn = d->t[d->n - 1];
    double ref_day = 0, ref_peak = 0, day[N_QUAD], peak[N_QUAD];
    int st_day[N_QUAD], st_peak[N_QUAD], q, ref = has_reference(d), any_warn = 0;
    char b1[12], b2[12];

    print_title(title);
    rprintf("  Dataset: %s\n", d->name);
    if (count_missing(d) > 0) rprintf("  Missing readings filled with: %s\n", INTERP_NAMES[s->fill]);
    rprintf("  Peak window: %s to %s\n\n", fmt_hhmm(s->peak_start, b1), fmt_hhmm(s->peak_end, b2));

    for (q = 0; q < N_QUAD; q++) {
        day[q] = energy_with_fill(d, s->fill, (QuadMethod)q, t0, tn, &st_day[q]);
        peak[q] = energy_with_fill(d, s->fill, (QuadMethod)q, s->peak_start, s->peak_end, &st_peak[q]);
    }
    if (ref) {
        ref_day = reference_energy(d, t0, tn);
        ref_peak = reference_energy(d, s->peak_start, s->peak_end);
    }

    rprintf("  +----------------------+------------+----------+------------+----------+\n");
    rprintf("  | Rule                 |  Day (kWh) | Error %%  | Peak (kWh) | Error %%  |\n");
    rprintf("  +----------------------+------------+----------+------------+----------+\n");
    if (ref)
        rprintf("  | %-20s | %10.4f |     --   | %10.4f |     --   |\n", ref_name(d), ref_day, ref_peak);
    for (q = 0; q < N_QUAD; q++) {
        rprintf("  | %-20s |", QUAD_NAMES[q]);
        if (st_day[q] >= 2) rprintf("     n/a    |     --   |");
        else if (ref) rprintf(" %10.4f%s| %+8.3f |", day[q], st_day[q] ? "*" : " ", 100 * (day[q] - ref_day) / ref_day);
        else rprintf(" %10.4f%s|     --   |", day[q], st_day[q] ? "*" : " ");
        if (st_peak[q] >= 2) rprintf("     n/a    |     --   |\n");
        else if (ref) rprintf(" %10.4f%s| %+8.3f |\n", peak[q], st_peak[q] ? "*" : " ", 100 * (peak[q] - ref_peak) / ref_peak);
        else rprintf(" %10.4f%s|     --   |\n", peak[q], st_peak[q] ? "*" : " ");
        if (st_day[q] == 1 || st_peak[q] == 1) any_warn = 1;
    }
    rprintf("  +----------------------+------------+----------+------------+----------+\n");
    if (any_warn)
        rprintf("  * interval count not a multiple of 2 (Simpson 1/3) or 3 (Simpson 3/8):\n"
                "    leftover interval(s) were integrated with the trapezoidal rule.\n");
    for (q = 0; q < N_QUAD; q++) {
        if (st_day[q] == 2 || st_peak[q] == 2) {
            rprintf("  n/a: Simpson rules need equally spaced readings.\n");
            break;
        }
    }
    for (q = 0; q < N_QUAD; q++) {
        if (st_peak[q] == 3) {
            rprintf("  n/a: the peak window must start and end at reading times.\n");
            break;
        }
    }
    if (st_day[Q_SIMP13] < 2) {
        rprintf("\n  Average power over the day : %.3f kW (Simpson 1/3)\n", day[Q_SIMP13] / (tn - t0));
        if (st_peak[Q_SIMP13] < 2)
            rprintf("  Share of energy in peak window: %.1f %%\n", 100.0 * peak[Q_SIMP13] / day[Q_SIMP13]);
    }
}

void show_energy(const Dataset *d, const Settings *s)
{
    energy_table(d, s, "Energy consumption (integration)");
}

static const char *dir_text(int dir)
{
    return dir > 0 ? "RISES above" : "FALLS below";
}

/*
 * Solve one event (a crossing or the budget) with all four methods and print
 * one table. Returns the reference root (bisection to 1e-13 on the same curve).
 *   has_true / t_true : true crossing time (for the model-error column)
 *   detailed          : add solver-error / order columns and iteration history
 */
static double g_max_solver_err, g_max_model_err;    /* for the final summary */

static double solve_event(const char *event, Func f, double a, double b,
                          int has_true, double t_true, int detailed,
                          FILE *csv_res, FILE *csv_it)
{
    RootResult res[N_ROOT];
    double ref = bisection(f, a, b, 1e-13, 200).root;
    int m, k, max_hist = 0;
    char buf[16];

    rprintf("  Bracket from scan: [%.2f, %.2f] h\n", a, b);
    if (detailed) {
        rprintf("  +----------------+-------------+----------+------+----------+------------+-----------+-------+\n");
        rprintf("  | Method         |   Root (h)  | HH:MM:SS | Iter | |f(root)| | Solver err |Model err  |Order p|\n");
        rprintf("  |                |             |          |      |          |    (h)     |  (min)    |       |\n");
        rprintf("  +----------------+-------------+----------+------+----------+------------+-----------+-------+\n");
    } else {
        rprintf("  +----------------+-------------+----------+------+----------+-------------------+\n");
        rprintf("  | Method         |   Root (h)  | HH:MM:SS | Iter | |f(root)| | Error vs true     |\n");
        rprintf("  +----------------+-------------+----------+------+----------+-------------------+\n");
    }
    for (m = 0; m < N_ROOT; m++) {
        RootResult *r = &res[m];
        double solver_err, model_err = 0.0;
        *r = solve_root((RootMethod)m, f, a, b, ROOT_TOL);
        if (!r->converged) {
            rprintf("  | %-14s | FAILED: %-60s|\n", ROOT_NAMES[m],
                    r->left_bracket ? "iterate left the bracket" : "did not converge");
            if (csv_res) fprintf(csv_res, "%s,%s,,,%d,0,,,\n", event, ROOT_NAMES[m], r->iterations);
            continue;
        }
        solver_err = fabs(r->root - ref);
        if (has_true) model_err = (r->root - t_true) * 60.0;
        if (solver_err > g_max_solver_err) g_max_solver_err = solver_err;
        if (has_true && fabs(model_err) > g_max_model_err) g_max_model_err = fabs(model_err);
        if (r->n_hist > max_hist) max_hist = r->n_hist;

        rprintf("  | %-14s | %11.7f | %s | %4d | %8.1e |", ROOT_NAMES[m], r->root,
                fmt_hhmmss(r->root, buf), r->iterations, fabs(f(r->root)));
        if (detailed) {
            rprintf(" %10.2e |", solver_err);
            if (has_true) rprintf(" %+9.3f |", model_err); else rprintf("     --    |");
            if (m == R_BISECTION) {
                rprintf("   1*  |\n");           /* see footnote below the table */
            } else {
                /* starting value: Newton starts at the middle, the others at b */
                double p = estimate_order(r, m == R_NEWTON ? 0.5 * (a + b) : b, ref);
                if (p > 0.0) rprintf(" %5.2f |\n", p);
                else rprintf("   --  |\n");
            }
        } else {
            if (has_true) rprintf(" %+8.3f min      |\n", model_err);
            else rprintf("        --         |\n");
        }
        if (csv_res) {
            fprintf(csv_res, "%s,%s,%.10f,%s,%d,1,%.3e,%.3e,", event, ROOT_NAMES[m], r->root,
                    buf, r->iterations, fabs(f(r->root)), solver_err);
            if (has_true) fprintf(csv_res, "%.4f", model_err);   /* empty if no true time */
            fprintf(csv_res, "\n");
        }
        if (csv_it)
            for (k = 0; k < r->n_hist; k++)
                fprintf(csv_it, "%s,%s,%d,%.12f,%.3e\n", event, ROOT_NAMES[m], k + 1,
                        r->hist[k], fabs(r->hist[k] - ref));
    }
    if (detailed) {
        rprintf("  +----------------+-------------+----------+------+----------+------------+-----------+-------+\n");
        rprintf("  Order p = ln(e_k+1/e_k) / ln(e_k/e_k-1). Theory: Regula Falsi 1, Newton 2, Secant 1.618.\n");
        rprintf("  * Bisection's error jumps up and down (see history), so p is not estimated from it;\n"
                "    its bracket halves every step, so it converges with order 1 (error ratio 1/2).\n");
    } else
        rprintf("  +----------------+-------------+----------+------+----------+-------------------+\n");
    if (has_true) {
        rprintf("  True time: %.7f h = %s\n", t_true, fmt_hhmmss(t_true, buf));
    }

    /* iteration history: error |x_k - x*| of every method at every step */
    if (detailed && max_hist > 0) {
        rprintf("\n  Iteration history: error |x_k - x*| in hours  (x* = %.10f h)\n", ref);
        rprintf("  +----+------------+------------+------------+------------+\n");
        rprintf("  |  k | Bisection  |Regula Falsi|  Newton-R  |   Secant   |\n");
        rprintf("  +----+------------+------------+------------+------------+\n");
        for (k = 0; k < max_hist; k++) {
            rprintf("  | %2d |", k + 1);
            for (m = 0; m < N_ROOT; m++) {
                if (res[m].converged && k < res[m].n_hist)
                    rprintf(" %10.3e |", fabs(res[m].hist[k] - ref));
                else
                    rprintf("            |");
            }
            rprintf("\n");
        }
        rprintf("  +----+------------+------------+------------+------------+\n");
    }
    return ref;
}

/* Power crossings + energy budget. detailed = 0 for the menu, 1 for error analysis. */
static void threshold_core(const Dataset *d, const Settings *s, int detailed,
                           FILE *csv_res, FILE *csv_it)
{
    static double br[MAX_CROSS][2], true_t[MAX_CROSS];
    static int true_dir[MAX_CROSS];
    double t0 = d->t[0], tn = d->t[d->n - 1], total, cross_t[MAX_CROSS], above_time = 0.0;
    double start_above;
    int nb, n_true, k, j, cross_dir[MAX_CROSS], above;
    char buf[16], buf2[16], event[32];

    curve_set(d, M_LAGRANGE);                   /* root finding always on the local cubic */
    set_targets(s->threshold, s->budget);
    g_max_solver_err = 0.0;
    g_max_model_err = 0.0;
    n_true = true_power_crossings(d, s->threshold, true_t, true_dir, MAX_CROSS);

    rprintf("  Curve: local cubic (Lagrange) through the %d known readings\n", g_nk);
    rprintf("  Tolerance %.0e h, scan step %.2f h (%d min), Newton slope by central difference\n",
            ROOT_TOL, SCAN_STEP, (int)(SCAN_STEP * 60 + 0.5));

    /* ---------------- (a) load crossing the power threshold ---------------- */
    rprintf("\n  (a) When does the load cross %.2f kW?   Solve  P(t) - %.2f = 0\n", s->threshold, s->threshold);
    nb = scan_brackets(f_power, t0, tn, SCAN_STEP, br, MAX_CROSS);
    if (nb == 0) {
        double best = t0, pmax = curve_power(t0), t;
        for (t = t0; t <= tn; t += 0.01)
            if (curve_power(t) > pmax) { pmax = curve_power(t); best = t; }
        rprintf("  The load NEVER crosses %.2f kW (maximum of the curve: %.3f kW at %s).\n",
                s->threshold, pmax, fmt_hhmm(best, buf));
    }
    for (k = 0; k < nb; k++) {
        double a = br[k][0], b = br[k][1], t_match = 0.0, best_gap = 1e9;
        int dir, has_match = 0;
        if (a == b) dir = (f_power(a + 0.01) > 0.0) ? +1 : -1;
        else dir = (f_power(a) < 0.0) ? +1 : -1;

        /* matching true crossing: the nearest one in the same direction */
        for (j = 0; j < n_true; j++) {
            double gap = fabs(true_t[j] - 0.5 * (a + b));
            if (true_dir[j] == dir && gap < best_gap) {
                best_gap = gap;
                t_match = true_t[j];
                has_match = 1;
            }
        }
        rprintf("\n  Crossing %d: load %s %.2f kW\n", k + 1, dir_text(dir), s->threshold);
        sprintf(event, "crossing_%d_%s", k + 1, dir > 0 ? "up" : "down");
        cross_t[k] = solve_event(event, f_power, a, b, has_match, t_match, detailed, csv_res, csv_it);
        cross_dir[k] = dir;
        if (d->has_fine && has_match)
            rprintf("  (true time = nearest crossing in the same direction in the 1-minute data)\n");
    }

    /* total time above the limit, walking through the crossings in order */
    if (nb > 0) {
        above = f_power(t0) > 0.0;
        start_above = t0;
        for (k = 0; k < nb; k++) {
            if (cross_dir[k] > 0) {
                start_above = cross_t[k];
                above = 1;
            } else if (above) {
                above_time += cross_t[k] - start_above;
                above = 0;
            }
        }
        if (above) above_time += tn - start_above;
        rprintf("\n  >> ALERT: load first goes above %.2f kW at %s;", s->threshold,
                fmt_hhmm(cross_dir[0] > 0 ? cross_t[0] : t0, buf));
        rprintf(" total time above limit = %s (h:min)\n", fmt_hhmm(above_time, buf2));
    }
    if (d->has_fine && n_true >= 0) {
        rprintf("  Note: the real 1-minute data crosses %.2f kW %d times; hourly readings see only %d.\n",
                s->threshold, n_true, nb);
        for (j = 0; j < n_true; j++) {
            if (true_dir[j] > 0) {
                rprintf("        In the 1-minute data the load FIRST goes above %.2f kW at %s.\n",
                        s->threshold, fmt_hhmm(true_t[j], buf));
                break;
            }
        }
    }

    /* ---------------- (b) cumulative energy reaching the budget ---------------- */
    rprintf("\n  (b) When does the day's energy reach %.2f kWh?   Solve  E(t) - %.2f = 0\n",
            s->budget, s->budget);
    total = curve_energy_upto(tn);
    rprintf("  E(t) = integral of the curve from %s to t (Simpson 1/3, exact for cubics). Day total = %.3f kWh\n",
            fmt_hhmm(t0, buf), total);
    if (total < s->budget) {
        rprintf("  The budget is NEVER reached (day total %.3f kWh < %.2f kWh).\n", total, s->budget);
    } else {
        double t_true = true_budget_time(d, s->budget);
        nb = scan_brackets(f_energy, t0, tn, SCAN_STEP, br, MAX_CROSS);
        if (nb > 0) {
            double r = solve_event("budget", f_energy, br[0][0], br[0][1], t_true >= 0, t_true,
                                   detailed, csv_res, csv_it);
            rprintf("\n  >> ALERT: %.2f kWh budget used up at %s", s->budget, fmt_hhmm(r, buf));
            if (r < s->peak_start)
                rprintf(" - BEFORE the %s peak window even starts.\n", fmt_hhmm(s->peak_start, buf2));
            else
                rprintf(".\n");
        }
    }
}

void show_threshold(const Dataset *d, const Settings *s)
{
    print_title("Threshold and energy-budget analysis (root finding)");
    rprintf("  Dataset: %s\n", d->name);
    threshold_core(d, s, 0, NULL, NULL);
}

void show_bill(const Dataset *d, const Settings *s)
{
    double t0 = d->t[0], tn = d->t[d->n - 1];
    int q, st1, st2;
    char b1[12], b2[12];

    print_title("Electricity bill estimate (flat rate)");
    rprintf("  Dataset: %s\n", d->name);
    rprintf("  Rate: Rs %.2f per kWh (flat, no slabs or fixed charges)   Peak window: %s-%s\n\n",
            s->rate, fmt_hhmm(s->peak_start, b1), fmt_hhmm(s->peak_end, b2));
    rprintf("  +----------------------+-----------+-------------+-----------+------------+--------------+\n");
    rprintf("  | Energy from          | Day (kWh) | Day bill Rs | Peak kWh  | Peak cost  | 30-day est.  |\n");
    rprintf("  +----------------------+-----------+-------------+-----------+------------+--------------+\n");
    if (has_reference(d)) {
        double e = reference_energy(d, t0, tn), ep = reference_energy(d, s->peak_start, s->peak_end);
        rprintf("  | %-20s | %9.3f | %11.2f | %9.3f | %10.2f | %12.2f |\n", ref_name(d),
                e, e * s->rate, ep, ep * s->rate, 30.0 * e * s->rate);
    }
    for (q = 0; q < N_QUAD; q++) {
        double e = energy_with_fill(d, s->fill, (QuadMethod)q, t0, tn, &st1);
        double ep = energy_with_fill(d, s->fill, (QuadMethod)q, s->peak_start, s->peak_end, &st2);
        if (st1 >= 2) {
            rprintf("  | %-20s |   n/a     |     n/a     |   n/a     |    n/a     |     n/a      |\n", QUAD_NAMES[q]);
            continue;
        }
        rprintf("  | %-20s | %9.3f | %11.2f |", QUAD_NAMES[q], e, e * s->rate);
        if (st2 >= 2) rprintf("   n/a     |    n/a     |");
        else rprintf(" %9.3f | %10.2f |", ep, ep * s->rate);
        rprintf(" %12.2f |\n", 30.0 * e * s->rate);
    }
    rprintf("  +----------------------+-----------+-------------+-----------+------------+--------------+\n");
    rprintf("  30-day estimate assumes every day is like this one.\n");
    if (count_missing(d) > 0) rprintf("  Missing readings filled with: %s\n", INTERP_NAMES[s->fill]);
}

/* ======================================================================
 *  5. Error analysis sections
 * ====================================================================== */

/* 1A: estimates at the missing readings + errors + Lagrange/NDD check */
static void sec_interp_missing(const Dataset *d, const char *dir)
{
    static double kx[MAX_POINTS], ky[MAX_POINTS], est[N_INTERP][MAX_POINTS], err[N_INTERP][MAX_POINTS];
    int nk = known_points(d, kx, ky), i, m, nm = 0, best = 0;
    double max_diff = 0.0, mae[N_INTERP], rmse[N_INTERP], mx[N_INTERP];
    int imax[N_INTERP], idx[MAX_POINTS];
    char buf[12];
    FILE *csv = open_csv(dir, "interpolation_results.csv");

    print_title("1A. Missing readings estimated by each interpolation method");
    rprintf("  Dataset: %s\n", d->name);
    if (count_missing(d) == 0) {
        rprintf("  This dataset has no missing readings.\n");
        if (csv) fclose(csv);
        return;
    }
    for (m = 0; m < N_INTERP; m++) fill_missing(d, (InterpMethod)m, est[m]);
    for (i = 0; i < d->n; i++) if (d->missing[i]) idx[nm++] = i;

    rprintf("  Local cubic = 4 nearest known readings. Global = one polynomial of degree %d.\n\n", nk - 1);
    rprintf("  Estimated power (kW)\n");
    rprintf("  +-------+------------+------------+------------+------------+------------+\n");
    rprintf("  | Time  |  True kW   |   Linear   |  Lagrange  |    NDD     |   Global   |\n");
    rprintf("  +-------+------------+------------+------------+------------+------------+\n");
    for (i = 0; i < nm; i++) {
        int r = idx[i];
        if (d->has_truth) rprintf("  | %s | %10.4f |", fmt_hhmm(d->t[r], buf), d->truth[r]);
        else rprintf("  | %s |     --     |", fmt_hhmm(d->t[r], buf));
        for (m = 0; m < N_INTERP; m++) rprintf(" %10.4f |", est[m][r]);
        rprintf("\n");
        if (fabs(est[M_LAGRANGE][r] - est[M_NDD][r]) > max_diff)
            max_diff = fabs(est[M_LAGRANGE][r] - est[M_NDD][r]);
    }
    rprintf("  +-------+------------+------------+------------+------------+------------+\n");

    if (csv) {
        fprintf(csv, "time_h,true_kW,linear,lagrange,ndd,global\n");
        for (i = 0; i < nm; i++) {
            int r = idx[i];
            fprintf(csv, "%.4f,", d->t[r]);
            if (d->has_truth) fprintf(csv, "%.6f", d->truth[r]);
            for (m = 0; m < N_INTERP; m++) fprintf(csv, ",%.6f", est[m][r]);
            fprintf(csv, "\n");
        }
        fclose(csv);
    }

    if (d->has_truth) {
        rprintf("\n  Absolute error |estimate - true| (kW)\n");
        rprintf("  +-------+------------+------------+------------+------------+\n");
        rprintf("  | Time  |   Linear   |  Lagrange  |    NDD     |   Global   |\n");
        rprintf("  +-------+------------+------------+------------+------------+\n");
        for (i = 0; i < nm; i++) {
            int r = idx[i];
            rprintf("  | %s |", fmt_hhmm(d->t[r], buf));
            for (m = 0; m < N_INTERP; m++) {
                err[m][i] = est[m][r] - d->truth[r];
                rprintf(" %10.4f |", fabs(err[m][i]));
            }
            rprintf("\n");
        }
        rprintf("  +-------+------------+------------+------------+------------+\n");
        for (m = 0; m < N_INTERP; m++) error_stats(err[m], nm, &mae[m], &rmse[m], &mx[m], &imax[m]);
        rprintf("  | MAE   |"); for (m = 0; m < N_INTERP; m++) rprintf(" %10.4f |", mae[m]); rprintf("\n");
        rprintf("  | RMSE  |"); for (m = 0; m < N_INTERP; m++) rprintf(" %10.4f |", rmse[m]); rprintf("\n");
        rprintf("  | Max   |"); for (m = 0; m < N_INTERP; m++) rprintf(" %10.4f |", mx[m]); rprintf("\n");
        rprintf("  +-------+------------+------------+------------+------------+\n");
        for (m = 1; m < N_INTERP; m++) if (rmse[m] < rmse[best]) best = m;
        rprintf("  Lowest RMSE at these %d readings: %s.\n", nm, INTERP_NAMES[best]);
        rprintf("  (Only %d test points - one lucky guess can decide this. Sections 1C and 1D\n"
                "   test every method on many more points and are more reliable.)\n", nm);
        rprintf("  Largest error of the local cubic: %.4f kW at %s.\n", mx[M_LAGRANGE],
                fmt_hhmm(d->t[idx[imax[M_LAGRANGE]]], buf));
    }
    rprintf("\n  CORRECTNESS CHECK: max |Lagrange - NDD| = %.2e kW  -> %s\n", max_diff,
            max_diff < 1e-10 ? "identical (same cubic, two different formulas)" : "MISMATCH - check code!");

    print_title("1B. Newton divided-difference tables at each missing reading");
    print_ndd_tables(d);
}

/* 1C: error of the whole interpolated curve against the true curve */
static void sec_interp_curve(const Dataset *d)
{
    static double kx[MAX_POINTS], ky[MAX_POINTS];
    int nk = known_points(d, kx, ky), m, k, npts;
    double t0 = d->t[0], tn = d->t[d->n - 1];
    char buf[12];

    print_title("1C. Error of the whole curve over the day (checked every 5 minutes)");
    if (!has_reference(d)) {
        rprintf("  No true curve available for this dataset.\n");
        return;
    }
    rprintf("  Reference: %s\n", d->has_model ? "exact model P(t)" : "1-minute meter data");
    rprintf("  +----------------------------------+--------------+----------+--------------+\n");
    rprintf("  | Method                           | Max |error|  | at time  | RMS error    |\n");
    rprintf("  +----------------------------------+--------------+----------+--------------+\n");
    npts = (int)floor((tn - t0) * 12.0 + 0.5) + 1;
    for (m = 0; m < N_INTERP; m++) {
        double mx = 0.0, sq = 0.0, t_mx = t0;
        for (k = 0; k < npts; k++) {
            double t = t0 + k / 12.0;
            double e = fabs(interp_eval((InterpMethod)m, kx, ky, nk, t) - reference_power(d, t));
            sq += e * e;
            if (e > mx) { mx = e; t_mx = t; }
        }
        rprintf("  | %-32s | %12.4f | %s    | %12.4f |\n", INTERP_NAMES[m], mx, fmt_hhmm(t_mx, buf),
                sqrt(sq / npts));
    }
    rprintf("  +----------------------------------+--------------+----------+--------------+\n");
    rprintf("  Global Lagrange swings wildly near the ends of the day (Runge's phenomenon),\n"
            "  which is why the local cubic (4 nearest points) is used instead.\n");
    if (d->has_fine)
        rprintf("  On real data every method has large curve errors: short appliance spikes between\n"
                "  two hourly readings cannot be recovered by any interpolation method.\n");
}

/* 1D: leave-one-out test on the known readings */
static void sec_interp_loocv(const Dataset *d, const char *dir)
{
    static double kx[MAX_POINTS], ky[MAX_POINTS], tx[MAX_POINTS], ty[MAX_POINTS];
    static double err[N_INTERP][MAX_POINTS];
    double mae, rmse, mx;
    int nk = known_points(d, kx, ky), j, i, m, nt = 0, imax;
    char buf[12];
    FILE *csv = open_csv(dir, "interpolation_loocv.csv");

    print_title("1D. Leave-one-out test (hide each known reading, predict it, compare)");
    if (nk < 6) {
        rprintf("  Too few known readings for this test.\n");
        if (csv) fclose(csv);
        return;
    }
    rprintf("  Each of the %d inner known readings is hidden in turn and predicted from the other %d.\n\n",
            nk - 2, nk - 1);
    rprintf("  +-------+-----------+-----------+-----------+-----------+-------------+\n");
    rprintf("  | Time  | Actual kW |  Linear   | Lagrange  |    NDD    | Global(%2d)  |\n", nk - 2);
    rprintf("  +-------+-----------+-----------+-----------+-----------+-------------+\n");
    if (csv) fprintf(csv, "time_h,actual_kW,linear,lagrange,ndd,global\n");
    for (j = 1; j < nk - 1; j++) {
        int c = 0;
        for (i = 0; i < nk; i++)                 /* copy all points except j */
            if (i != j) { tx[c] = kx[i]; ty[c] = ky[i]; c++; }
        rprintf("  | %s | %9.3f |", fmt_hhmm(kx[j], buf), ky[j]);
        if (csv) fprintf(csv, "%.4f,%.6f", kx[j], ky[j]);
        for (m = 0; m < N_INTERP; m++) {
            double p = interp_eval((InterpMethod)m, tx, ty, c, kx[j]);
            err[m][nt] = p - ky[j];
            if (m < M_GLOBAL) rprintf(" %9.3f |", p);
            else rprintf(" %11.3f |", p);
            if (csv) fprintf(csv, ",%.6f", p);
        }
        rprintf("\n");
        if (csv) fprintf(csv, "\n");
        nt++;
    }
    rprintf("  +-------+-----------+-----------+-----------+-----------+-------------+\n");
    if (csv) fclose(csv);

    rprintf("\n  Leave-one-out summary (kW)\n");
    rprintf("  +----------------------------------+----------+----------+------------+----------+\n");
    rprintf("  | Method                           |   MAE    |   RMSE   |  Max error | Worst at |\n");
    rprintf("  +----------------------------------+----------+----------+------------+----------+\n");
    for (m = 0; m < N_INTERP; m++) {
        error_stats(err[m], nt, &mae, &rmse, &mx, &imax);
        rprintf("  | %-32s | %8.4f | %8.4f | %10.4f |  %s   |\n", INTERP_NAMES[m], mae, rmse, mx,
                fmt_hhmm(kx[imax + 1], buf));
    }
    rprintf("  +----------------------------------+----------+----------+------------+----------+\n");
}

/* 2A + 2B: energy by each rule, and the 4 x 3 "error through the chain" table */
static void sec_integ_energy(const Dataset *d, const Settings *s, const char *dir)
{
    double t0 = d->t[0], tn = d->t[d->n - 1], ref = 0.0, ref_peak = 0.0;
    int q, m, st, st2;
    FILE *csv = open_csv(dir, "integration_results.csv");

    energy_table(d, s, "2A. Energy by each integration rule");
    if (csv) {
        fprintf(csv, "rule,day_kWh,day_ref_kWh,day_error_kWh,peak_kWh,peak_ref_kWh,peak_error_kWh\n");
        if (has_reference(d)) {
            ref = reference_energy(d, t0, tn);
            ref_peak = reference_energy(d, s->peak_start, s->peak_end);
        }
        for (q = 0; q < N_QUAD; q++) {
            double e = energy_with_fill(d, s->fill, (QuadMethod)q, t0, tn, &st);
            double ep = energy_with_fill(d, s->fill, (QuadMethod)q, s->peak_start, s->peak_end, &st2);
            fprintf(csv, "%s,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f\n", QUAD_NAMES[q], e, ref, e - ref,
                    ep, ref_peak, ep - ref_peak);
        }
        fclose(csv);
    }

    print_title("2B. Error carried through the chain: gap-filling method x integration rule");
    if (!(count_missing(d) > 0 && d->has_truth && has_reference(d))) {
        rprintf("  Needs missing readings, true values and a reference energy - not available.\n");
        return;
    }
    ref = reference_energy(d, t0, tn);
    csv = open_csv(dir, "integration_chain.csv");
    if (csv) fprintf(csv, "filled_with,trap_error_kWh,simp13_error_kWh,simp38_error_kWh\n");
    rprintf("  Full-day energy error (kWh) = computed - %s (%.4f kWh)\n", ref_name(d), ref);
    rprintf("  +----------------------------------+--------------+--------------+--------------+\n");
    rprintf("  | Missing readings filled with     |  Trapezoidal |  Simpson 1/3 |  Simpson 3/8 |\n");
    rprintf("  +----------------------------------+--------------+--------------+--------------+\n");
    rprintf("  | %-32s |", "(none: true readings, no gaps)");
    if (csv) fprintf(csv, "True readings (no gaps)");
    for (q = 0; q < N_QUAD; q++) {
        double e = energy_of_array(d, d->truth, (QuadMethod)q, t0, tn, &st);
        rprintf(" %+12.4f |", e - ref);
        if (csv) fprintf(csv, ",%.6f", e - ref);
    }
    rprintf("\n");
    if (csv) fprintf(csv, "\n");
    for (m = 0; m < N_INTERP; m++) {
        rprintf("  | %-32s |", INTERP_NAMES[m]);
        if (csv) fprintf(csv, "%s", INTERP_NAMES[m]);
        for (q = 0; q < N_QUAD; q++) {
            double e = energy_with_fill(d, (InterpMethod)m, (QuadMethod)q, t0, tn, &st);
            rprintf(" %+12.4f |", e - ref);
            if (csv) fprintf(csv, ",%.6f", e - ref);
        }
        rprintf("\n");
        if (csv) fprintf(csv, "\n");
    }
    if (csv) fclose(csv);
    rprintf("  +----------------------------------+--------------+--------------+--------------+\n");
    rprintf("  First row = error of the integration rule alone (hourly sampling).\n"
            "  Other rows = integration error + error from the estimated readings.\n");
}

/* 2C: convergence study (model) or sampling study (1-minute data) */
static void sec_integ_convergence(const Dataset *d, const char *dir)
{
    static double x[MAX_POINTS], y[MAX_POINTS];
    double t0 = d->t[0], tn = d->t[d->n - 1];
    int q, k, i, st;
    FILE *csv = open_csv(dir, "integration_convergence.csv");

    if (d->has_model) {
        static const double H[] = {2.0, 1.0, 0.5, 0.25, 0.125};
        double exact = model_energy(t0, tn), prev[N_QUAD] = {0, 0, 0};

        print_title("2C. Convergence study: error vs step size h (exact model)");
        rprintf("  Exact energy (erf formula) = %.10f kWh\n", exact);
        rprintf("  Observed order p = log2( |E(2h)| / |E(h)| ). Theory: Trapezoidal 2, Simpson 4.\n\n");
        rprintf("  +---------+-----+-------------+------+-------------+------+-------------+------+\n");
        rprintf("  |  h (h)  |  n  | Trap. error |  p   | Simp1/3 err |  p   | Simp3/8 err |  p   |\n");
        rprintf("  +---------+-----+-------------+------+-------------+------+-------------+------+\n");
        if (csv) fprintf(csv, "h_hours,intervals,trap_error,simp13_error,simp38_error\n");
        for (k = 0; k < 5; k++) {
            int n = (int)floor((tn - t0) / H[k] + 0.5);
            for (i = 0; i <= n; i++) {
                x[i] = t0 + i * H[k];
                y[i] = model_power(x[i]);
            }
            rprintf("  | %7.3f | %3d |", H[k], n);
            if (csv) fprintf(csv, "%.4f,%d", H[k], n);
            for (q = 0; q < N_QUAD; q++) {
                double e = integrate_grid((QuadMethod)q, x, y, n + 1, &st) - exact;
                rprintf(" %+11.3e |", e);
                if (k > 0) rprintf(" %4.2f |", log(fabs(prev[q] / e)) / log(2.0));
                else rprintf("  --  |");
                prev[q] = e;
                if (csv) fprintf(csv, ",%.6e", e);
            }
            rprintf("\n");
            if (csv) fprintf(csv, "\n");
        }
        rprintf("  +---------+-----+-------------+------+-------------+------+-------------+------+\n");
        rprintf("  At large h the peaks are under-sampled, so p only settles as h gets small.\n");
    } else if (d->has_fine) {
        static const int STEP[] = {120, 60, 30, 15, 5, 1};      /* minutes */
        double exact = reference_energy(d, d->fine_t[0], d->fine_t[d->n_fine - 1]);

        print_title("2C. Sampling study: how often must the meter read? (real 1-minute data)");
        rprintf("  Metered energy (sum of 1-minute averages) = %.4f kWh\n\n", exact);
        rprintf("  +-------------+----------+---------------+---------------+---------------+\n");
        rprintf("  | Read every  | Readings | Trap. error %% | Simp1/3 err %% | Simp3/8 err %% |\n");
        rprintf("  +-------------+----------+---------------+---------------+---------------+\n");
        if (csv) fprintf(csv, "step_minutes,readings,trap_error_pct,simp13_error_pct,simp38_error_pct\n");
        for (k = 0; k < 6; k++) {
            int n = 0;
            for (i = 0; i < d->n_fine; i += STEP[k]) {
                x[n] = d->fine_t[i];
                y[n] = d->fine_p[i];
                n++;
            }
            rprintf("  | %4d min    | %8d |", STEP[k], n);
            if (csv) fprintf(csv, "%d,%d", STEP[k], n);
            for (q = 0; q < N_QUAD; q++) {
                double e = 100.0 * (integrate_grid((QuadMethod)q, x, y, n, &st) - exact) / exact;
                if (st == 2) {                  /* Simpson impossible: unequal spacing */
                    rprintf("           n/a |");
                    if (csv) fprintf(csv, ",");
                    continue;
                }
                rprintf(" %+13.3f |", e);
                if (csv) fprintf(csv, ",%.6f", e);
            }
            rprintf("\n");
            if (csv) fprintf(csv, "\n");
        }
        rprintf("  +-------------+----------+---------------+---------------+---------------+\n");
        rprintf("  The error does not fall smoothly like h^2 or h^4: real load jumps when appliances\n"
                "  switch on/off, so how OFTEN the meter reads matters more than WHICH rule is used.\n");
    } else {
        print_title("2C. Convergence study");
        rprintf("  Needs an exact model or 1-minute data - not available for this dataset.\n");
    }
    if (csv) fclose(csv);
}

/* 3: root finding with solver error, model error and iteration history */
static void sec_roots(const Dataset *d, const Settings *s, const char *dir)
{
    FILE *csv_res = open_csv(dir, "root_results.csv");
    FILE *csv_it = open_csv(dir, "root_iterations.csv");

    print_title("3. Root finding: comparison of Bisection, Regula Falsi, Newton-Raphson, Secant");
    rprintf("  Dataset: %s\n", d->name);
    if (csv_res) fprintf(csv_res, "event,method,root_h,hh_mm_ss,iterations,converged,abs_f_root,solver_error_h,model_error_min\n");
    if (csv_it) fprintf(csv_it, "event,method,k,x_k,abs_error_h\n");
    rprintf("  Solver error = |root - x*|, x* = bisection to 1e-13 on the SAME curve (method accuracy).\n");
    rprintf("  Model error  = root - TRUE time, in minutes (accuracy of the data + interpolation).\n");
    threshold_core(d, s, 1, csv_res, csv_it);

    rprintf("\n  SUMMARY: largest solver error = %.2e h (%.3f ms)", g_max_solver_err, g_max_solver_err * 3.6e6);
    if (has_reference(d))
        rprintf("; largest model error = %.2f min.\n"
                "  -> Accuracy is limited by the readings and interpolation, not by the root finder.\n",
                g_max_model_err);
    else
        rprintf(".\n");
    if (csv_res) fclose(csv_res);
    if (csv_it) fclose(csv_it);
}

void analysis_interpolation(const Dataset *d, const char *csv_dir)
{
    sec_interp_missing(d, csv_dir);
    sec_interp_curve(d);
    sec_interp_loocv(d, csv_dir);
}

void analysis_integration(const Dataset *d, const Settings *s, const char *csv_dir)
{
    sec_integ_energy(d, s, csv_dir);
    sec_integ_convergence(d, csv_dir);
}

void analysis_roots(const Dataset *d, const Settings *s, const char *csv_dir)
{
    sec_roots(d, s, csv_dir);
}

/* ======================================================================
 *  6. Export
 * ====================================================================== */

/* Dense curve every 5 minutes, for plotting. */
static void write_dense_csv(const Dataset *d, const char *dir)
{
    static double kx[MAX_POINTS], ky[MAX_POINTS];
    int nk = known_points(d, kx, ky), k, m, npts;
    double t0 = d->t[0], tn = d->t[d->n - 1];
    FILE *csv = open_csv(dir, "load_curve_dense.csv");
    if (csv == NULL) return;
    fprintf(csv, "time_h,true_kW,linear,lagrange,ndd,global\n");
    npts = (int)floor((tn - t0) * 12.0 + 0.5) + 1;
    for (k = 0; k < npts; k++) {
        double t = t0 + k / 12.0;
        fprintf(csv, "%.5f,", t);
        if (has_reference(d)) fprintf(csv, "%.6f", reference_power(d, t));
        for (m = 0; m < N_INTERP; m++)
            fprintf(csv, ",%.6f", interp_eval((InterpMethod)m, kx, ky, nk, t));
        fprintf(csv, "\n");
    }
    fclose(csv);

    /* the readings themselves (known + missing + truth), also for plotting */
    csv = open_csv(dir, "readings.csv");
    if (csv == NULL) return;
    fprintf(csv, "time_h,reading_kW,missing,true_kW\n");
    for (k = 0; k < d->n; k++) {
        fprintf(csv, "%.5f,", d->t[k]);
        if (!d->missing[k]) fprintf(csv, "%.6f", d->p[k]);
        fprintf(csv, ",%d,", d->missing[k]);
        if (d->has_truth) fprintf(csv, "%.6f", d->truth[k]);
        fprintf(csv, "\n");
    }
    fclose(csv);
}

/* Open dir/name for a .txt section. The file starts with the title box;
 * after that, rprintf output goes to this file AND to the summary file. */
static FILE *open_section(const char *dir, const char *name, const Dataset *d, FILE *summary)
{
    char path[256];
    FILE *fp;
    sprintf(path, "%s/%s", dir, name);
    fp = fopen(path, "w");
    if (fp == NULL) {
        printf("  WARNING: cannot write %s\n", path);
        report_set_files(summary, NULL);
        return NULL;
    }
    report_set_files(fp, NULL);         /* header only in the section file */
    print_header();
    rprintf("Dataset: %s\n", d->name);
    report_set_files(fp, summary);      /* content goes to both files      */
    return fp;
}

static void close_section(FILE *fp)
{
    report_set_files(NULL, NULL);
    if (fp) fclose(fp);
}

void export_all(const Dataset *d, const Settings *s)
{
    char dir[64], path[256];
    FILE *summary, *fp;

    sprintf(dir, "output/%s", d->tag);
    make_dir("output");
    make_dir(dir);
    sprintf(path, "%s/summary_report.txt", dir);
    summary = fopen(path, "w");
    if (summary == NULL) {
        printf("  ERROR: cannot write %s (is the output folder writable?)\n", path);
        return;
    }

    report_set_screen(0);               /* write to files only */
    report_set_files(summary, NULL);
    print_header();
    rprintf("SUMMARY REPORT\nDataset: %s\n", d->name);
    rprintf("Settings: threshold %.2f kW, budget %.2f kWh, peak window %.2f-%.2f h, rate Rs %.2f/kWh\n",
            s->threshold, s->budget, s->peak_start, s->peak_end, s->rate);
    show_readings(d, s);

    fp = open_section(dir, "interpolation_results.txt", d, summary);
    sec_interp_missing(d, dir);
    sec_interp_curve(d);
    close_section(fp);

    fp = open_section(dir, "interpolation_loocv.txt", d, summary);
    sec_interp_loocv(d, dir);
    close_section(fp);

    fp = open_section(dir, "integration_results.txt", d, summary);
    sec_integ_energy(d, s, dir);
    close_section(fp);

    fp = open_section(dir, "integration_convergence.txt", d, summary);
    sec_integ_convergence(d, dir);
    close_section(fp);

    fp = open_section(dir, "root_results.txt", d, summary);
    sec_roots(d, s, dir);
    close_section(fp);

    fp = open_section(dir, "bill_estimate.txt", d, summary);
    show_bill(d, s);
    close_section(fp);

    write_dense_csv(d, dir);
    fclose(summary);
    report_set_screen(1);

    printf("\n  Results for '%s' written to %s/ :\n", d->tag, dir);
    printf("    summary_report.txt\n");
    printf("    interpolation_results.txt/.csv   interpolation_loocv.txt/.csv\n");
    printf("    integration_results.txt/.csv     integration_convergence.txt/.csv\n");
    printf("    root_results.txt/.csv            root_iterations.csv\n");
    printf("    bill_estimate.txt                load_curve_dense.csv, readings.csv\n");
}
