/*
 * tests.c - automated test cases.
 *
 *   T1-T9   Tier A: each method on a function whose answer is known by hand
 *   T10-T15 edge cases and bad input
 *   S1-S6   full scenarios on the synthetic and real datasets
 *
 * Each test prints one row: ID, what was tested, the measured value, PASS/FAIL.
 */
#include <stdio.h>
#include <math.h>
#include <string.h>
#include "tests.h"
#include "dataset.h"
#include "data_io.h"
#include "load_model.h"
#include "interpolation.h"
#include "integration.h"
#include "root_finding.h"
#include "analysis.h"
#include "report.h"

static const double PI = 3.14159265358979323846;
static int g_pass, g_fail;
static TestRow g_rows[MAX_TESTS];       /* every result, kept for the web page */
static int g_nrows;

/* Print one result row, count it and remember it. */
static void check(const char *id, const char *what, const char *detail, int ok)
{
    rprintf("  | %-4s | %-42s | %-32s | %s |\n", id, what, detail, ok ? "PASS" : "FAIL");
    if (ok) g_pass++; else g_fail++;
    if (g_nrows < MAX_TESTS) {
        TestRow *r = &g_rows[g_nrows++];
        strncpy(r->id, id, sizeof r->id - 1);
        r->id[sizeof r->id - 1] = '\0';
        strncpy(r->what, what, sizeof r->what - 1);
        r->what[sizeof r->what - 1] = '\0';
        strncpy(r->detail, detail, sizeof r->detail - 1);
        r->detail[sizeof r->detail - 1] = '\0';
        r->passed = ok;
    }
}

int test_count(void)
{
    return g_nrows;
}

const TestRow *test_row(int i)
{
    return &g_rows[i];
}

/* Test functions with known answers */
static double cubic(double x)    { return x * x * x - 2 * x * x + x - 5; }
static double sqrt2_f(double x)  { return x * x - 2.0; }

/* Write a small text file (used for the bad-input tests). */
static void write_file(const char *path, const char *text)
{
    FILE *fp = fopen(path, "w");
    if (fp) {
        fputs(text, fp);
        fclose(fp);
    }
}

int run_all_tests(void)
{
    static Dataset syn, real, tmp;
    static double br[64][2], est[MAX_POINTS], kx[MAX_POINTS], ky[MAX_POINTS];
    double x[8], y[8], v, e6, e12, e24, r2 = sqrt(2.0);
    char detail[64];
    int i, st, nb, ok;
    RootResult rb, rr, rn, rs;

    g_pass = g_fail = 0;
    g_nrows = 0;
    make_dir("output");                     /* temporary test files go here */
    print_title("TEST CASES");
    rprintf("  +------+--------------------------------------------+----------------------------------+------+\n");
    rprintf("  | ID   | Test                                       | Result                           |      |\n");
    rprintf("  +------+--------------------------------------------+----------------------------------+------+\n");

    /* ---------------- Tier A: method verification ---------------- */
    for (i = 0; i < 5; i++) { x[i] = i; y[i] = 2.0 * i + 1.0; }
    v = interp_linear(x, y, 5, 2.5);
    sprintf(detail, "P(2.5) = %.10f", v);
    check("T1", "Linear interp of y=2x+1 at 2.5 (exact 6)", detail, fabs(v - 6.0) < 1e-12);

    for (i = 0; i < 5; i++) { x[i] = i; y[i] = cubic(i); }
    v = interp_lagrange_local(x, y, 5, 2.7);
    sprintf(detail, "err = %.1e", fabs(v - cubic(2.7)));
    check("T2", "Local Lagrange on a cubic at 2.7 (exact)", detail, fabs(v - cubic(2.7)) < 1e-12);

    v = interp_ndd_local(x, y, 5, 2.7);
    sprintf(detail, "err %.1e, |L-N| %.1e", fabs(v - cubic(2.7)),
            fabs(v - interp_lagrange_local(x, y, 5, 2.7)));
    check("T3", "Local NDD on the cubic, NDD == Lagrange", detail,
          fabs(v - cubic(2.7)) < 1e-12 && fabs(v - interp_lagrange_local(x, y, 5, 2.7)) < 1e-12);

    {   /* trapezoidal on sin x from 0 to pi with n = 6, 12, 24 intervals */
        static double sx[32], sy[32];
        int n, k;
        double err[3];
        for (k = 0; k < 3; k++) {
            n = 6 << k;                             /* 6, 12, 24 */
            for (i = 0; i <= n; i++) { sx[i] = PI * i / n; sy[i] = sin(sx[i]); }
            err[k] = fabs(trapezoidal(sx, sy, n + 1) - 2.0);
        }
        e6 = err[0]; e12 = err[1]; e24 = err[2];
        sprintf(detail, "ratios %.3f, %.3f", e6 / e12, e12 / e24);
        check("T4", "Trapezoid sin 0..pi: error /4 when h/2", detail,
              fabs(e6 / e12 - 4.0) < 0.1 && fabs(e12 / e24 - 4.0) < 0.1);
    }

    for (i = 0; i <= 4; i++) y[i] = pow(0.5 * i, 3);           /* x^3 on 0..2, h = 0.5 */
    v = simpson_13(y, 5, 0.5, &st);
    sprintf(detail, "I = %.12f", v);
    check("T5", "Simpson 1/3: integral x^3 on [0,2] = 4", detail, fabs(v - 4.0) < 1e-12 && st == 0);

    for (i = 0; i <= 6; i++) y[i] = pow(i / 3.0, 3);           /* x^3 on 0..2, h = 1/3 */
    v = simpson_38(y, 7, 1.0 / 3.0, &st);
    sprintf(detail, "I = %.12f", v);
    check("T6", "Simpson 3/8: integral x^3 on [0,2] = 4", detail, fabs(v - 4.0) < 1e-12 && st == 0);

    rb = bisection(sqrt2_f, 1.0, 2.0, 1e-10, 100);
    sprintf(detail, "%.10f, %d iter", rb.root, rb.iterations);
    check("T7", "Bisection x^2-2=0 on [1,2] -> sqrt 2", detail, fabs(rb.root - r2) < 1e-9);

    rr = regula_falsi(sqrt2_f, 1.0, 2.0, 1e-10, 100);
    sprintf(detail, "%.10f, %d iter", rr.root, rr.iterations);
    check("T8", "Regula Falsi x^2-2=0 on [1,2]", detail, fabs(rr.root - r2) < 1e-9);

    rn = newton_raphson(sqrt2_f, 1.5, 1.0, 2.0, 1e-10, 100);
    rs = secant(sqrt2_f, 1.0, 2.0, 1.0, 2.0, 1e-10, 100);
    sprintf(detail, "NR %d it, Sec %d it, Bis %d", rn.iterations, rs.iterations, rb.iterations);
    check("T9", "Newton + Secant -> sqrt 2, NR faster", detail,
          fabs(rn.root - r2) < 1e-9 && fabs(rs.root - r2) < 1e-9 && rn.iterations < rb.iterations);

    /* ---------------- edge cases ---------------- */
    if (!load_synthetic_dataset(&syn)) {
        rprintf("  | ---- | synthetic dataset could not be loaded: remaining tests skipped |\n");
        g_fail++;
        return g_fail;
    }
    curve_set(&syn, M_LAGRANGE);

    set_targets(10.0, 15.0);
    nb = scan_brackets(f_power, 0.0, 24.0, SCAN_STEP, br, 64);
    sprintf(detail, "%d crossings found", nb);
    check("T10", "Threshold 10 kW (above daily max)", detail, nb == 0);

    set_targets(syn.p[18], 15.0);                    /* threshold = the 18:00 reading */
    nb = scan_brackets(f_power, 0.0, 24.0, SCAN_STEP, br, 64);
    ok = 0;
    for (i = 0; i < nb; i++)
        if (br[i][0] == 18.0 && br[i][1] == 18.0 &&
            solve_root(R_NEWTON, f_power, 18.0, 18.0, ROOT_TOL).root == 18.0 &&
            solve_root(R_SECANT, f_power, 18.0, 18.0, ROOT_TOL).root == 18.0) ok = 1;
    sprintf(detail, "root at 18.0 found: %s", ok ? "yes" : "no");
    check("T11", "Threshold equal to the 18:00 reading", detail, ok);

    write_file("output/test_tmp.csv", "time_hours,power_kW\n0,NA\n1,0.5\n2,0.7\n");
    printf("  (T12 expects an error message:)\n");
    ok = !load_csv("output/test_tmp.csv", &tmp);
    check("T12", "CSV whose first reading is NA", ok ? "rejected" : "accepted!", ok);

    {
        int nk = known_points(&syn, kx, ky), s = local_stencil(kx, nk, 13.5);
        fill_missing(&syn, M_LAGRANGE, est);
        sprintf(detail, "pts %.0f,%.0f,%.0f,%.0f err %.3f,%.3f", kx[s], kx[s + 1], kx[s + 2], kx[s + 3],
                fabs(est[13] - syn.truth[13]), fabs(est[14] - syn.truth[14]));
        check("T13", "Two missing in a row (13:00, 14:00)", detail,
              kx[s] == 11 && kx[s + 1] == 12 && kx[s + 2] == 15 && kx[s + 3] == 16 &&
              fabs(est[13] - syn.truth[13]) < 0.1 && fabs(est[14] - syn.truth[14]) < 0.1);
    }

    for (i = 0; i <= 5; i++) y[i] = (double)i * i;             /* x^2, 5 intervals */
    v = simpson_13(y, 6, 1.0, &st);
    sprintf(detail, "I = %.6f, warning = %d", v, st);
    check("T14", "Simpson 1/3, odd intervals (5): fallback", detail,
          st == 1 && fabs(v - (64.0 / 3.0 + 20.5)) < 1e-12);

    write_file("output/test_tmp.csv", "time_hours,power_kW\n0,0.5\n1,abc\n2,0.7\n");
    printf("  (T15 expects two error messages:)\n");
    ok = !load_csv("output/does_not_exist.csv", &tmp) && !load_csv("output/test_tmp.csv", &tmp);
    remove("output/test_tmp.csv");
    check("T15", "Missing file / non-numeric value", ok ? "both rejected" : "accepted!", ok);

    /* ---------------- scenarios ---------------- */
    {
        double max_local = 0.0, max_global = 0.0, t;
        int nk = known_points(&syn, kx, ky);
        fill_missing(&syn, M_LAGRANGE, est);
        for (i = 0; i < syn.n; i++)
            if (syn.missing[i] && fabs(est[i] - syn.truth[i]) > max_local)
                max_local = fabs(est[i] - syn.truth[i]);
        for (t = 0.0; t <= 24.0; t += 1.0 / 12.0)
            if (fabs(interp_lagrange_global(kx, ky, nk, t) - model_power(t)) > max_global)
                max_global = fabs(interp_lagrange_global(kx, ky, nk, t) - model_power(t));
        sprintf(detail, "local %.3f, global %.1f kW", max_local, max_global);
        check("S1", "Synthetic gaps: local < 0.1, global > 1 kW", detail,
              max_local < 0.1 && max_global > 1.0);
    }

    {
        double worst = 0.0, ed = model_energy(0, 24), ep = model_energy(18, 22);
        int q;
        for (q = 0; q < N_QUAD; q++) {
            double a = fabs(energy_with_fill(&syn, M_LAGRANGE, (QuadMethod)q, 0, 24, &st) - ed) / ed;
            double b = fabs(energy_with_fill(&syn, M_LAGRANGE, (QuadMethod)q, 18, 22, &st) - ep) / ep;
            if (a > worst) worst = a;
            if (b > worst) worst = b;
        }
        sprintf(detail, "worst error %.3f %%", 100 * worst);
        check("S2", "Synthetic energy, day + peak: all < 1%", detail, worst < 0.01);
    }

    {
        double tt[8], roots[N_ROOT];
        int dirs[8], ntrue, m, agree = 1, close = 1, c;
        curve_set(&syn, M_LAGRANGE);
        set_targets(3.0, 15.0);
        nb = scan_brackets(f_power, 0.0, 24.0, SCAN_STEP, br, 64);
        ntrue = true_power_crossings(&syn, 3.0, tt, dirs, 8);
        for (c = 0; c < nb && c < ntrue; c++) {
            for (m = 0; m < N_ROOT; m++) roots[m] = solve_root((RootMethod)m, f_power, br[c][0], br[c][1], ROOT_TOL).root;
            for (m = 1; m < N_ROOT; m++) if (fabs(roots[m] - roots[0]) > 1e-5) agree = 0;
            if (fabs(roots[0] - tt[c]) * 60.0 > 5.0) close = 0;
        }
        sprintf(detail, "%d crossings, agree %s, <5min %s", nb, agree ? "y" : "n", close ? "y" : "n");
        check("S3", "Synthetic 3 kW crossings (up, down)", detail,
              nb == 2 && ntrue == 2 && dirs[0] == 1 && dirs[1] == -1 && agree && close);
    }

    {
        double t_true = true_budget_time(&syn, 15.0), t_est;
        nb = scan_brackets(f_energy, 0.0, 24.0, SCAN_STEP, br, 64);
        t_est = (nb == 1) ? solve_root(R_NEWTON, f_energy, br[0][0], br[0][1], ROOT_TOL).root : -1;
        sprintf(detail, "%d root, error %.2f min", nb, (t_est - t_true) * 60.0);
        check("S4", "Synthetic 15 kWh budget time", detail, nb == 1 && fabs(t_est - t_true) * 60.0 < 5.0);
    }

    printf("  (loading real dataset for S5:)\n");
    if (load_real_dataset(&real)) {
        double e = energy_with_fill(&real, M_LAGRANGE, Q_SIMP13, 0, 24, &st);
        double em = reference_energy(&real, 0, 24), tb_true = true_budget_time(&real, 15.0), tb = -1;
        int nk = known_points(&real, kx, ky);
        curve_set(&real, M_LAGRANGE);
        set_targets(3.0, 15.0);
        nb = scan_brackets(f_energy, 0.0, 24.0, SCAN_STEP, br, 64);
        if (nb == 1) tb = solve_root(R_BISECTION, f_energy, br[0][0], br[0][1], ROOT_TOL).root;
        sprintf(detail, "E err %.1f%%, budget %+.0f min", 100 * (e - em) / em, (tb - tb_true) * 60);
        check("S5", "Real day: energy within 10%, budget found", detail,
              nk >= 6 && fabs(e - em) / em < 0.10 && tb > 0 && tb_true > 0);
    } else {
        check("S5", "Real day: energy within 10%, budget found", "real data files missing", 0);
    }

    {   /* observed order between h = 0.25 and h = 0.125 */
        static double sx[200], sy[200];
        double ex = model_energy(0, 24), err[2][N_QUAD], p[N_QUAD];
        int k, q, n;
        for (k = 0; k < 2; k++) {
            n = 96 << k;                                /* 96 and 192 intervals */
            for (i = 0; i <= n; i++) { sx[i] = 24.0 * i / n; sy[i] = model_power(sx[i]); }
            for (q = 0; q < N_QUAD; q++) err[k][q] = integrate_grid((QuadMethod)q, sx, sy, n + 1, &st) - ex;
        }
        for (q = 0; q < N_QUAD; q++) p[q] = log(fabs(err[0][q] / err[1][q])) / log(2.0);
        sprintf(detail, "p = %.2f, %.2f, %.2f", p[0], p[1], p[2]);
        check("S6", "Convergence order: Trap 2, Simpson 4", detail,
              fabs(p[0] - 2) < 0.1 && fabs(p[1] - 4) < 0.2 && fabs(p[2] - 4) < 0.2);
    }

    rprintf("  +------+--------------------------------------------+----------------------------------+------+\n");
    rprintf("  TOTAL: %d passed, %d failed (out of %d)\n", g_pass, g_fail, g_pass + g_fail);
    return g_fail;
}
