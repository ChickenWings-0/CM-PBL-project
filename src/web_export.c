/*
 * web_export.c - writes every result as JSON for the web front-end.
 *
 * Nothing new is computed here: the same functions used by the menu
 * (interpolation, integration, root finding, reference answers) are called,
 * and their results are printed as JSON instead of ASCII tables.
 *
 * JSON in brief: { "key": value, ... } is an object, [a, b, c] is a list,
 * and null means "no value" (for example, no true value is known).
 */
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include "web_export.h"
#include "data_io.h"
#include "load_model.h"
#include "interpolation.h"
#include "integration.h"
#include "root_finding.h"
#include "report.h"
#include "tests.h"

#define MAX_CROSS 256

/* ------------------------------------------------------------------ */
/*  Small JSON writing helpers                                        */
/* ------------------------------------------------------------------ */

/* A number, or null if it is not a finite value (NaN / infinity). */
static void jnum(FILE *f, double x)
{
    if (isfinite(x)) fprintf(f, "%.10g", x);
    else fputs("null", f);
}

/* A text value in quotes; " and \ are escaped. */
static void jstr(FILE *f, const char *s)
{
    fputc('"', f);
    for (; *s; s++) {
        if (*s == '"' || *s == '\\') fputc('\\', f);
        fputc(*s, f);
    }
    fputc('"', f);
}

/* "key": */
static void jkey(FILE *f, const char *k)
{
    jstr(f, k);
    fputc(':', f);
}

/* [a0, a1, ...] */
static void jarr(FILE *f, const double *a, int n)
{
    int i;
    fputc('[', f);
    for (i = 0; i < n; i++) {
        if (i) fputc(',', f);
        jnum(f, a[i]);
    }
    fputc(']', f);
}

/* ["name0", "name1", ...] */
static void jnames(FILE *f, const char **names, int n)
{
    int i;
    fputc('[', f);
    for (i = 0; i < n; i++) {
        if (i) fputc(',', f);
        jstr(f, names[i]);
    }
    fputc(']', f);
}

/* MAE, RMSE and max |error| of err[0..n-1]; *imax = index of the max. */
static void stats(const double err[], int n, double *mae, double *rmse, double *mx, int *imax)
{
    double sa = 0.0, sq = 0.0;
    int i;
    *mx = 0.0;
    *imax = 0;
    for (i = 0; i < n; i++) {
        double e = fabs(err[i]);
        sa += e;
        sq += e * e;
        if (e > *mx) { *mx = e; *imax = i; }
    }
    *mae = n ? sa / n : 0.0;
    *rmse = n ? sqrt(sq / n) : 0.0;
}

/* ------------------------------------------------------------------ */
/*  Dataset, readings and curves                                      */
/* ------------------------------------------------------------------ */

static void write_dataset(FILE *f, const Dataset *d, const Settings *s)
{
    int i;
    jkey(f, "dataset");
    fprintf(f, "{");
    jkey(f, "tag"); jstr(f, d->tag); fputc(',', f);
    jkey(f, "name"); jstr(f, d->name); fputc(',', f);
    fprintf(f, "\"n\":%d,\"n_missing\":%d,\"has_truth\":%d,\"has_model\":%d,\"has_fine\":%d,",
            d->n, count_missing(d), d->has_truth, d->has_model, d->has_fine);
    jkey(f, "t0"); jnum(f, d->t[0]); fputc(',', f);
    jkey(f, "tn"); jnum(f, d->t[d->n - 1]);
    fprintf(f, "},");

    jkey(f, "settings");
    fprintf(f, "{");
    jkey(f, "threshold"); jnum(f, s->threshold); fputc(',', f);
    jkey(f, "budget"); jnum(f, s->budget); fputc(',', f);
    jkey(f, "peak_start"); jnum(f, s->peak_start); fputc(',', f);
    jkey(f, "peak_end"); jnum(f, s->peak_end); fputc(',', f);
    jkey(f, "rate"); jnum(f, s->rate); fputc(',', f);
    fprintf(f, "\"fill\":%d", (int)s->fill);
    fprintf(f, "},");

    jkey(f, "methods");
    fprintf(f, "{");
    jkey(f, "interp"); jnames(f, INTERP_NAMES, N_INTERP); fputc(',', f);
    jkey(f, "interp_short"); jnames(f, INTERP_SHORT, N_INTERP); fputc(',', f);
    jkey(f, "quad"); jnames(f, QUAD_NAMES, N_QUAD); fputc(',', f);
    jkey(f, "root"); jnames(f, ROOT_NAMES, N_ROOT);
    fprintf(f, "},");

    jkey(f, "readings");
    fprintf(f, "[");
    for (i = 0; i < d->n; i++) {
        if (i) fputc(',', f);
        fprintf(f, "{\"t\":");
        jnum(f, d->t[i]);
        fprintf(f, ",\"p\":");
        if (d->missing[i]) fputs("null", f); else jnum(f, d->p[i]);
        fprintf(f, ",\"missing\":%d,\"truth\":", d->missing[i]);
        if (d->has_truth) jnum(f, d->truth[i]); else fputs("null", f);
        fputc('}', f);
    }
    fprintf(f, "],");

    jkey(f, "minute");
    if (d->has_fine) {
        fprintf(f, "{\"t\":");
        jarr(f, d->fine_t, d->n_fine);
        fprintf(f, ",\"p\":");
        jarr(f, d->fine_p, d->n_fine);
        fprintf(f, "},");
    } else {
        fprintf(f, "null,");
    }
}

/* Every curve on a 5-minute grid (for the charts). */
static void write_dense(FILE *f, const Dataset *d)
{
    static double kx[MAX_POINTS], ky[MAX_POINTS], tt[MAX_POINTS], v[MAX_POINTS];
    int nk = known_points(d, kx, ky), k, m;
    double t0 = d->t[0], tn = d->t[d->n - 1];
    int npts = (int)floor((tn - t0) * 12.0 + 0.5) + 1;
    static const char *keys[N_INTERP] = {"linear", "lagrange", "ndd", "global"};

    if (npts > MAX_POINTS) npts = MAX_POINTS;
    for (k = 0; k < npts; k++) tt[k] = t0 + k / 12.0;

    jkey(f, "dense");
    fprintf(f, "{\"t\":");
    jarr(f, tt, npts);
    fprintf(f, ",\"true\":");
    if (has_reference(d)) {
        for (k = 0; k < npts; k++) v[k] = reference_power(d, tt[k]);
        jarr(f, v, npts);
    } else {
        fputs("null", f);
    }
    for (m = 0; m < N_INTERP; m++) {
        for (k = 0; k < npts; k++) v[k] = interp_eval((InterpMethod)m, kx, ky, nk, tt[k]);
        fprintf(f, ",\"%s\":", keys[m]);
        jarr(f, v, npts);
    }
    fprintf(f, "},");
}

/* ------------------------------------------------------------------ */
/*  Interpolation                                                     */
/* ------------------------------------------------------------------ */

/* Divided-difference table of the local cubic used at time t. */
static void write_ndd(FILE *f, const double kx[], const double ky[], int nk, double t)
{
    double table[4][4], products[3], value, product = 1.0;
    const double *x;
    int s, r, c;

    if (nk < 4) {
        fputs("null", f);
        return;
    }
    s = local_stencil(kx, nk, t);
    x = kx + s;
    ndd_build_table(x, ky + s, 4, table);
    value = table[0][0];
    for (c = 1; c < 4; c++) {
        product *= (t - x[c - 1]);
        products[c - 1] = product;
        value += table[0][c] * product;
    }
    fprintf(f, "{\"x\":");
    jarr(f, x, 4);
    fprintf(f, ",\"table\":[");
    for (r = 0; r < 4; r++) {
        if (r) fputc(',', f);
        jarr(f, table[r], 4 - r);       /* row r has 4 - r entries */
    }
    fprintf(f, "],\"products\":");
    jarr(f, products, 3);
    fprintf(f, ",\"value\":");
    jnum(f, value);
    fputc('}', f);
}

static void write_interp(FILE *f, const Dataset *d, double probe_t)
{
    static double kx[MAX_POINTS], ky[MAX_POINTS], tx[MAX_POINTS], ty[MAX_POINTS];
    static double est[N_INTERP][MAX_POINTS], err[N_INTERP][MAX_POINTS];
    int nk = known_points(d, kx, ky), i, m, nm = 0, first = 1, k, imax;
    double max_diff = 0.0, mae[N_INTERP], rmse[N_INTERP], mx[N_INTERP], worst[N_INTERP], e4[N_INTERP];
    double t0 = d->t[0], tn = d->t[d->n - 1];

    for (m = 0; m < N_INTERP; m++) fill_missing(d, (InterpMethod)m, est[m]);

    jkey(f, "interp");
    fprintf(f, "{\"n_known\":%d,\"global_degree\":%d,\"missing\":[", nk, nk - 1);
    for (i = 0; i < d->n; i++) {
        if (!d->missing[i]) continue;
        if (!first) fputc(',', f);
        first = 0;
        fprintf(f, "{\"t\":");
        jnum(f, d->t[i]);
        fprintf(f, ",\"true\":");
        if (d->has_truth) jnum(f, d->truth[i]); else fputs("null", f);
        for (m = 0; m < N_INTERP; m++) {
            e4[m] = est[m][i];
            if (d->has_truth) err[m][nm] = est[m][i] - d->truth[i];
        }
        fprintf(f, ",\"est\":");
        jarr(f, e4, N_INTERP);
        fprintf(f, ",\"ndd\":");
        write_ndd(f, kx, ky, nk, d->t[i]);
        fputc('}', f);
        if (fabs(est[M_LAGRANGE][i] - est[M_NDD][i]) > max_diff)
            max_diff = fabs(est[M_LAGRANGE][i] - est[M_NDD][i]);
        nm++;
    }
    fprintf(f, "],\"stats\":");
    if (d->has_truth && nm > 0) {
        for (m = 0; m < N_INTERP; m++) stats(err[m], nm, &mae[m], &rmse[m], &mx[m], &imax);
        fprintf(f, "{\"mae\":"); jarr(f, mae, N_INTERP);
        fprintf(f, ",\"rmse\":"); jarr(f, rmse, N_INTERP);
        fprintf(f, ",\"max\":"); jarr(f, mx, N_INTERP);
        fputc('}', f);
    } else {
        fputs("null", f);
    }
    fprintf(f, ",\"lag_ndd_maxdiff\":");
    jnum(f, max_diff);

    /* error of the whole curve, every 5 minutes */
    fprintf(f, ",\"curve_error\":");
    if (has_reference(d)) {
        int npts = (int)floor((tn - t0) * 12.0 + 0.5) + 1;
        fputc('[', f);
        for (m = 0; m < N_INTERP; m++) {
            double cmax = 0.0, sq = 0.0, at = t0;
            for (k = 0; k < npts; k++) {
                double t = t0 + k / 12.0;
                double e = fabs(interp_eval((InterpMethod)m, kx, ky, nk, t) - reference_power(d, t));
                sq += e * e;
                if (e > cmax) { cmax = e; at = t; }
            }
            if (m) fputc(',', f);
            fprintf(f, "{\"max\":"); jnum(f, cmax);
            fprintf(f, ",\"at\":"); jnum(f, at);
            fprintf(f, ",\"rms\":"); jnum(f, sqrt(sq / npts));
            fputc('}', f);
        }
        fputc(']', f);
    } else {
        fputs("null", f);
    }

    /* leave-one-out test on the inner known readings */
    fprintf(f, ",\"loocv\":");
    if (nk >= 6) {
        int nt = 0, j;
        fprintf(f, "{\"points\":[");
        for (j = 1; j < nk - 1; j++) {
            int c = 0;
            for (i = 0; i < nk; i++)
                if (i != j) { tx[c] = kx[i]; ty[c] = ky[i]; c++; }
            for (m = 0; m < N_INTERP; m++) {
                e4[m] = interp_eval((InterpMethod)m, tx, ty, c, kx[j]);
                err[m][nt] = e4[m] - ky[j];
            }
            if (nt) fputc(',', f);
            fprintf(f, "{\"t\":"); jnum(f, kx[j]);
            fprintf(f, ",\"actual\":"); jnum(f, ky[j]);
            fprintf(f, ",\"pred\":"); jarr(f, e4, N_INTERP);
            fputc('}', f);
            nt++;
        }
        for (m = 0; m < N_INTERP; m++) {
            stats(err[m], nt, &mae[m], &rmse[m], &mx[m], &imax);
            worst[m] = kx[imax + 1];
        }
        fprintf(f, "],\"mae\":"); jarr(f, mae, N_INTERP);
        fprintf(f, ",\"rmse\":"); jarr(f, rmse, N_INTERP);
        fprintf(f, ",\"max\":"); jarr(f, mx, N_INTERP);
        fprintf(f, ",\"worst_t\":"); jarr(f, worst, N_INTERP);
        fputc('}', f);
    } else {
        fputs("null", f);
    }

    /* estimate at a time chosen by the user */
    fprintf(f, ",\"probe\":");
    if (probe_t >= t0 && probe_t <= tn) {
        for (m = 0; m < N_INTERP; m++) e4[m] = interp_eval((InterpMethod)m, kx, ky, nk, probe_t);
        fprintf(f, "{\"t\":"); jnum(f, probe_t);
        fprintf(f, ",\"true\":");
        if (has_reference(d)) jnum(f, reference_power(d, probe_t)); else fputs("null", f);
        fprintf(f, ",\"est\":"); jarr(f, e4, N_INTERP);
        fprintf(f, ",\"ndd\":"); write_ndd(f, kx, ky, nk, probe_t);
        fputc('}', f);
    } else {
        fputs("null", f);
    }
    fprintf(f, "},");
}

/* ------------------------------------------------------------------ */
/*  Integration and bill                                              */
/* ------------------------------------------------------------------ */

static void write_energy(FILE *f, const Dataset *d, const Settings *s)
{
    static double x[MAX_POINTS], y[MAX_POINTS];
    double t0 = d->t[0], tn = d->t[d->n - 1], ref_day = NAN, ref_peak = NAN, day[N_QUAD], peak[N_QUAD];
    int q, m, st, st_day[N_QUAD], st_peak[N_QUAD], k, i;

    if (has_reference(d)) {
        ref_day = reference_energy(d, t0, tn);
        ref_peak = reference_energy(d, s->peak_start, s->peak_end);
    }
    for (q = 0; q < N_QUAD; q++) {
        day[q] = energy_with_fill(d, s->fill, (QuadMethod)q, t0, tn, &st_day[q]);
        peak[q] = energy_with_fill(d, s->fill, (QuadMethod)q, s->peak_start, s->peak_end, &st_peak[q]);
        if (st_day[q] >= 2) day[q] = NAN;          /* not possible -> null */
        if (st_peak[q] >= 2) peak[q] = NAN;
    }

    jkey(f, "energy");
    fprintf(f, "{\"reference_name\":");
    if (d->has_model) jstr(f, "Exact (erf formula)");
    else if (d->has_fine) jstr(f, "Metered (1-min data)");
    else fputs("null", f);
    fprintf(f, ",\"ref_day\":"); jnum(f, ref_day);
    fprintf(f, ",\"ref_peak\":"); jnum(f, ref_peak);
    fprintf(f, ",\"rules\":[");
    for (q = 0; q < N_QUAD; q++) {
        if (q) fputc(',', f);
        fprintf(f, "{\"name\":"); jstr(f, QUAD_NAMES[q]);
        fprintf(f, ",\"day\":"); jnum(f, day[q]);
        fprintf(f, ",\"day_status\":%d,\"peak\":", st_day[q]);
        jnum(f, peak[q]);
        fprintf(f, ",\"peak_status\":%d}", st_peak[q]);
    }
    fprintf(f, "]");

    /* error through the chain: gap filling x integration rule */
    fprintf(f, ",\"chain\":");
    if (count_missing(d) > 0 && d->has_truth && has_reference(d)) {
        double e3[N_QUAD];
        fprintf(f, "[{\"name\":\"True readings (no gaps)\",\"err\":");
        for (q = 0; q < N_QUAD; q++) e3[q] = energy_of_array(d, d->truth, (QuadMethod)q, t0, tn, &st) - ref_day;
        jarr(f, e3, N_QUAD);
        fputc('}', f);
        for (m = 0; m < N_INTERP; m++) {
            for (q = 0; q < N_QUAD; q++)
                e3[q] = energy_with_fill(d, (InterpMethod)m, (QuadMethod)q, t0, tn, &st) - ref_day;
            fprintf(f, ",{\"name\":"); jstr(f, INTERP_NAMES[m]);
            fprintf(f, ",\"err\":"); jarr(f, e3, N_QUAD);
            fputc('}', f);
        }
        fputc(']', f);
    } else {
        fputs("null", f);
    }

    /* convergence study (model) or sampling study (1-minute data) */
    fprintf(f, ",\"convergence\":");
    if (d->has_model) {
        static const double H[] = {2.0, 1.0, 0.5, 0.25, 0.125};
        double exact = model_energy(t0, tn), prev[N_QUAD] = {0, 0, 0}, e3[N_QUAD], p3[N_QUAD];
        fprintf(f, "{\"kind\":\"model\",\"exact\":");
        jnum(f, exact);
        fprintf(f, ",\"rows\":[");
        for (k = 0; k < 5; k++) {
            int n = (int)floor((tn - t0) / H[k] + 0.5);
            for (i = 0; i <= n; i++) { x[i] = t0 + i * H[k]; y[i] = model_power(x[i]); }
            for (q = 0; q < N_QUAD; q++) {
                e3[q] = integrate_grid((QuadMethod)q, x, y, n + 1, &st) - exact;
                p3[q] = (k > 0) ? log(fabs(prev[q] / e3[q])) / log(2.0) : NAN;
                prev[q] = e3[q];
            }
            if (k) fputc(',', f);
            fprintf(f, "{\"h\":"); jnum(f, H[k]);
            fprintf(f, ",\"n\":%d,\"err\":", n); jarr(f, e3, N_QUAD);
            fprintf(f, ",\"p\":"); jarr(f, p3, N_QUAD);
            fputc('}', f);
        }
        fprintf(f, "]}");
    } else if (d->has_fine) {
        static const int STEP[] = {120, 60, 30, 15, 5, 1};
        double exact = reference_energy(d, d->fine_t[0], d->fine_t[d->n_fine - 1]), e3[N_QUAD];
        fprintf(f, "{\"kind\":\"sampling\",\"exact\":");
        jnum(f, exact);
        fprintf(f, ",\"rows\":[");
        for (k = 0; k < 6; k++) {
            int n = 0;
            for (i = 0; i < d->n_fine; i += STEP[k]) { x[n] = d->fine_t[i]; y[n] = d->fine_p[i]; n++; }
            for (q = 0; q < N_QUAD; q++) {
                double e = integrate_grid((QuadMethod)q, x, y, n, &st);
                e3[q] = (st == 2) ? NAN : 100.0 * (e - exact) / exact;
            }
            if (k) fputc(',', f);
            fprintf(f, "{\"step_min\":%d,\"n\":%d,\"err_pct\":", STEP[k], n);
            jarr(f, e3, N_QUAD);
            fputc('}', f);
        }
        fprintf(f, "]}");
    } else {
        fputs("null", f);
    }
    fprintf(f, "},");

    /* bill = energy x flat rate (computed here so the page only displays it) */
    jkey(f, "bill");
    fprintf(f, "{\"rate\":");
    jnum(f, s->rate);
    fprintf(f, ",\"reference\":");
    if (has_reference(d)) {
        fprintf(f, "{\"day_kwh\":"); jnum(f, ref_day);
        fprintf(f, ",\"day_cost\":"); jnum(f, ref_day * s->rate);
        fprintf(f, ",\"peak_kwh\":"); jnum(f, ref_peak);
        fprintf(f, ",\"peak_cost\":"); jnum(f, ref_peak * s->rate);
        fprintf(f, ",\"month_cost\":"); jnum(f, 30.0 * ref_day * s->rate);
        fputc('}', f);
    } else {
        fputs("null", f);
    }
    fprintf(f, ",\"rows\":[");
    for (q = 0; q < N_QUAD; q++) {
        if (q) fputc(',', f);
        fprintf(f, "{\"name\":"); jstr(f, QUAD_NAMES[q]);
        fprintf(f, ",\"day_kwh\":"); jnum(f, day[q]);
        fprintf(f, ",\"day_cost\":"); jnum(f, day[q] * s->rate);
        fprintf(f, ",\"peak_kwh\":"); jnum(f, peak[q]);
        fprintf(f, ",\"peak_cost\":"); jnum(f, peak[q] * s->rate);
        fprintf(f, ",\"month_cost\":"); jnum(f, 30.0 * day[q] * s->rate);
        fputc('}', f);
    }
    fprintf(f, "]},");
}

/* ------------------------------------------------------------------ */
/*  Root finding                                                      */
/* ------------------------------------------------------------------ */

static double g_max_solver, g_max_model;

/* One event (crossing or budget) solved by all four methods. Returns the reference root. */
static double write_event(FILE *f, Func fn, double a, double b, int dir, int has_true, double t_true)
{
    double ref = bisection(fn, a, b, 1e-13, 200).root, herr[ROOT_MAX_ITER + 1];
    int m, k;

    fprintf(f, "{\"dir\":%d,\"a\":", dir);
    jnum(f, a);
    fprintf(f, ",\"b\":"); jnum(f, b);
    fprintf(f, ",\"true\":");
    if (has_true) jnum(f, t_true); else fputs("null", f);
    fprintf(f, ",\"ref\":"); jnum(f, ref);
    fprintf(f, ",\"methods\":[");
    for (m = 0; m < N_ROOT; m++) {
        RootResult r = solve_root((RootMethod)m, fn, a, b, ROOT_TOL);
        double solver = fabs(r.root - ref), order = NAN;
        if (m) fputc(',', f);
        fprintf(f, "{\"name\":"); jstr(f, ROOT_NAMES[m]);
        fprintf(f, ",\"converged\":%d,\"left_bracket\":%d,\"iterations\":%d", r.converged, r.left_bracket,
                r.iterations);
        if (!r.converged) {
            fprintf(f, ",\"root\":null,\"f_root\":null,\"solver_err\":null,\"model_err_min\":null,"
                       "\"order\":null,\"hist\":[],\"hist_err\":[]}");
            continue;
        }
        if (m != R_BISECTION && r.n_hist >= 2) {
            order = estimate_order(&r, m == R_NEWTON ? 0.5 * (a + b) : b, ref);
            if (order <= 0.0) order = NAN;
        }
        if (solver > g_max_solver) g_max_solver = solver;
        if (has_true && fabs(r.root - t_true) * 60.0 > g_max_model) g_max_model = fabs(r.root - t_true) * 60.0;
        for (k = 0; k < r.n_hist; k++) herr[k] = fabs(r.hist[k] - ref);

        fprintf(f, ",\"root\":"); jnum(f, r.root);
        fprintf(f, ",\"f_root\":"); jnum(f, fabs(fn(r.root)));
        fprintf(f, ",\"solver_err\":"); jnum(f, solver);
        fprintf(f, ",\"model_err_min\":");
        if (has_true) jnum(f, (r.root - t_true) * 60.0); else fputs("null", f);
        fprintf(f, ",\"order\":"); jnum(f, order);
        fprintf(f, ",\"hist\":"); jarr(f, r.hist, r.n_hist);
        fprintf(f, ",\"hist_err\":"); jarr(f, herr, r.n_hist);
        fputc('}', f);
    }
    fprintf(f, "]}");
    return ref;
}

static void write_roots(FILE *f, const Dataset *d, const Settings *s)
{
    static double br[MAX_CROSS][2], true_t[MAX_CROSS], cross_t[MAX_CROSS];
    static int true_dir[MAX_CROSS], cross_dir[MAX_CROSS];
    double t0 = d->t[0], tn = d->t[d->n - 1], total, above_time = 0.0, start_above, pmax, tmax, t;
    int nb, n_true, k, j, above;

    curve_set(d, M_LAGRANGE);                /* root finding runs on the local cubic */
    set_targets(s->threshold, s->budget);
    g_max_solver = 0.0;
    g_max_model = 0.0;
    n_true = true_power_crossings(d, s->threshold, true_t, true_dir, MAX_CROSS);

    jkey(f, "roots");
    fprintf(f, "{\"threshold\":");
    jnum(f, s->threshold);
    fprintf(f, ",\"budget\":"); jnum(f, s->budget);
    fprintf(f, ",\"tol\":"); jnum(f, ROOT_TOL);
    fprintf(f, ",\"scan_step\":"); jnum(f, SCAN_STEP);

    /* highest point of the curve (reported when the limit is never crossed) */
    pmax = curve_power(t0);
    tmax = t0;
    for (t = t0; t <= tn; t += 0.01)
        if (curve_power(t) > pmax) { pmax = curve_power(t); tmax = t; }
    fprintf(f, ",\"curve_max\":{\"p\":"); jnum(f, pmax);
    fprintf(f, ",\"t\":"); jnum(f, tmax);
    fputc('}', f);

    fprintf(f, ",\"crossings\":[");
    nb = scan_brackets(f_power, t0, tn, SCAN_STEP, br, MAX_CROSS);
    for (k = 0; k < nb; k++) {
        double a = br[k][0], b = br[k][1], t_match = 0.0, best = 1e9;
        int dir, has_match = 0;
        if (a == b) dir = (f_power(a + 0.01) > 0.0) ? 1 : -1;
        else dir = (f_power(a) < 0.0) ? 1 : -1;
        for (j = 0; j < n_true; j++) {
            double gap = fabs(true_t[j] - 0.5 * (a + b));
            if (true_dir[j] == dir && gap < best) { best = gap; t_match = true_t[j]; has_match = 1; }
        }
        if (k) fputc(',', f);
        cross_t[k] = write_event(f, f_power, a, b, dir, has_match, t_match);
        cross_dir[k] = dir;
    }
    fprintf(f, "]");

    /* time above the limit */
    fprintf(f, ",\"first_above\":");
    if (nb > 0) {
        above = f_power(t0) > 0.0;
        start_above = t0;
        for (k = 0; k < nb; k++) {
            if (cross_dir[k] > 0) { start_above = cross_t[k]; above = 1; }
            else if (above) { above_time += cross_t[k] - start_above; above = 0; }
        }
        if (above) above_time += tn - start_above;
        jnum(f, cross_dir[0] > 0 ? cross_t[0] : t0);
    } else {
        fputs("null", f);
    }
    fprintf(f, ",\"above_time\":"); jnum(f, above_time);

    /* real data: what the 1-minute meter actually saw */
    fprintf(f, ",\"fine_crossings\":");
    if (d->has_fine && n_true >= 0) fprintf(f, "%d", n_true); else fputs("null", f);
    fprintf(f, ",\"fine_first_up\":");
    {
        double first_up = NAN;
        if (d->has_fine)
            for (j = 0; j < n_true; j++)
                if (true_dir[j] > 0) { first_up = true_t[j]; break; }
        jnum(f, first_up);
    }

    /* cumulative energy E(t) every 15 minutes: on the curve, and the true one */
    {
        static double tt[MAX_POINTS], ec[MAX_POINTS], et[MAX_POINTS];
        int np = (int)floor((tn - t0) * 4.0 + 0.5) + 1, ok_true = has_reference(d);
        for (k = 0; k < np; k++) {
            tt[k] = t0 + k * 0.25;
            ec[k] = curve_energy_upto(tt[k]);
            et[k] = ok_true ? reference_energy(d, t0, tt[k]) : NAN;
        }
        fprintf(f, ",\"energy_curve\":{\"t\":"); jarr(f, tt, np);
        fprintf(f, ",\"curve\":"); jarr(f, ec, np);
        fprintf(f, ",\"true\":");
        if (ok_true) jarr(f, et, np); else fputs("null", f);
        fputc('}', f);
    }

    /* energy budget */
    total = curve_energy_upto(tn);
    fprintf(f, ",\"day_total_curve\":"); jnum(f, total);
    fprintf(f, ",\"budget_event\":");
    if (total >= s->budget) {
        double t_true = true_budget_time(d, s->budget);
        nb = scan_brackets(f_energy, t0, tn, SCAN_STEP, br, MAX_CROSS);
        if (nb > 0) write_event(f, f_energy, br[0][0], br[0][1], 0, t_true >= 0, t_true);
        else fputs("null", f);
    } else {
        fputs("null", f);
    }
    fprintf(f, ",\"max_solver_err\":"); jnum(f, g_max_solver);
    fprintf(f, ",\"max_model_err_min\":");
    if (has_reference(d)) jnum(f, g_max_model); else fputs("null", f);
    fprintf(f, "}");
}

/* ------------------------------------------------------------------ */
/*  Public functions                                                  */
/* ------------------------------------------------------------------ */

void write_json(FILE *fp, const Dataset *d, const Settings *s, double probe_t)
{
    fprintf(fp, "{");
    write_dataset(fp, d, s);
    write_dense(fp, d);
    write_interp(fp, d, probe_t);
    write_energy(fp, d, s);
    write_roots(fp, d, s);
    fprintf(fp, "}");
}

void write_tests_json(FILE *fp)
{
    int failed, i, n;
    report_set_screen(0);                   /* tests print tables: keep them off screen */
    failed = run_all_tests();
    report_set_screen(1);
    n = test_count();
    fprintf(fp, "{\"total\":%d,\"failed\":%d,\"rows\":[", n, failed);
    for (i = 0; i < n; i++) {
        const TestRow *r = test_row(i);
        if (i) fputc(',', fp);
        fprintf(fp, "{\"id\":"); jstr(fp, r->id);
        fprintf(fp, ",\"what\":"); jstr(fp, r->what);
        fprintf(fp, ",\"detail\":"); jstr(fp, r->detail);
        fprintf(fp, ",\"passed\":%d}", r->passed);
    }
    fprintf(fp, "]}");
}

int export_web(void)
{
    static Dataset d;
    Settings s = default_settings();
    char stamp[32];
    time_t now = time(NULL);
    FILE *fp;

    make_dir("web");
    make_dir("web/data");
    fp = fopen("web/data/results.js", "w");
    if (fp == NULL) {
        printf("  ERROR: cannot write web/data/results.js\n");
        return 0;
    }
    strftime(stamp, sizeof stamp, "%Y-%m-%d %H:%M", localtime(&now));
    fprintf(fp, "/* Generated by energy.exe --export-web. Do not edit by hand. */\n");
    fprintf(fp, "window.ENERGY_DATA = {\"generated\":");
    jstr(fp, stamp);
    if (load_synthetic_dataset(&d)) {
        fprintf(fp, ",\"synthetic\":");
        write_json(fp, &d, &s, -1.0);
    }
    if (load_real_dataset(&d)) {
        fprintf(fp, ",\"real\":");
        write_json(fp, &d, &s, -1.0);
    }
    fprintf(fp, ",\"tests\":");
    write_tests_json(fp);
    fprintf(fp, "};\n");
    fclose(fp);
    printf("  Web snapshot written to web/data/results.js\n");
    return 1;
}
