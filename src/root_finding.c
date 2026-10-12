/*
 * root_finding.c - Bisection, Regula Falsi, Newton-Raphson and Secant.
 *
 *   Bisection     c = (a + b) / 2                         order 1   (always converges)
 *   Regula Falsi  c = b - f(b)(b - a) / (f(b) - f(a))     order 1   (always converges)
 *   Newton        x1 = x0 - f(x0) / f'(x0)                order 2   (needs good start)
 *   Secant        x2 = x1 - f(x1)(x1 - x0) / (f(x1) - f(x0))   order ~1.618
 *
 * All methods stop when the step |x_k - x_(k-1)| (half-width for bisection)
 * is below tol, or when f is exactly 0, or after max_iter iterations.
 */
#include <math.h>
#include "root_finding.h"

const char *ROOT_NAMES[N_ROOT] = { "Bisection", "Regula Falsi", "Newton-Raphson", "Secant" };

/* Start an empty result. */
static RootResult new_result(void)
{
    RootResult r;
    r.root = 0.0;
    r.iterations = 0;
    r.converged = 0;
    r.left_bracket = 0;
    r.n_hist = 0;
    return r;
}

/* Remember the estimate of this iteration (for the convergence table). */
static void record(RootResult *r, double x)
{
    if (r->n_hist <= ROOT_MAX_ITER) r->hist[r->n_hist++] = x;
    r->root = x;
    r->iterations++;
}

RootResult bisection(Func f, double a, double b, double tol, int max_iter)
{
    RootResult r = new_result();
    double fa = f(a);
    int k;

    if (a == b) {                       /* f was exactly zero at a scan point */
        r.root = a;
        r.converged = 1;
        return r;
    }
    for (k = 0; k < max_iter; k++) {
        double c = 0.5 * (a + b), fc = f(c);
        record(&r, c);
        if (fc == 0.0 || 0.5 * (b - a) < tol) {
            r.converged = 1;
            break;
        }
        if (fa * fc < 0.0) {            /* root is in the left half  */
            b = c;
        } else {                        /* root is in the right half */
            a = c;
            fa = fc;
        }
    }
    return r;
}

RootResult regula_falsi(Func f, double a, double b, double tol, int max_iter)
{
    RootResult r = new_result();
    double fa = f(a), fb = f(b), c_old = a;
    int k;

    if (a == b) {
        r.root = a;
        r.converged = 1;
        return r;
    }
    for (k = 0; k < max_iter; k++) {
        /* where the straight line through (a,fa) and (b,fb) cuts the t-axis */
        double c = b - fb * (b - a) / (fb - fa), fc = f(c);
        record(&r, c);
        if (fc == 0.0 || (k > 0 && fabs(c - c_old) < tol)) {
            r.converged = 1;
            break;
        }
        if (fa * fc < 0.0) {
            b = c;
            fb = fc;
        } else {
            a = c;
            fa = fc;
        }
        c_old = c;
    }
    return r;
}

double central_diff(Func f, double x)
{
    const double d = 1e-5;
    return (f(x + d) - f(x - d)) / (2.0 * d);
}

RootResult newton_raphson(Func f, double x0, double a, double b, double tol, int max_iter)
{
    RootResult r = new_result();
    double x = x0;
    int k;

    for (k = 0; k < max_iter; k++) {
        double fx = f(x), slope = central_diff(f, x), x_new;
        if (fx == 0.0) {                /* already exactly on the root */
            r.root = x;
            r.converged = 1;
            break;
        }
        if (slope == 0.0) break;        /* flat curve: Newton cannot continue */
        x_new = x - fx / slope;
        record(&r, x_new);
        if (x_new < a || x_new > b) {   /* jumped out of the bracket: unsafe */
            r.left_bracket = 1;
            break;
        }
        if (fabs(x_new - x) < tol) {
            r.converged = 1;
            break;
        }
        x = x_new;
    }
    return r;
}

RootResult secant(Func f, double x0, double x1, double a, double b, double tol, int max_iter)
{
    RootResult r = new_result();
    double f0 = f(x0), f1 = f(x1);
    int k;

    if (x0 == x1) {
        r.root = x0;
        r.converged = 1;
        return r;
    }
    for (k = 0; k < max_iter; k++) {
        double x2;
        if (f1 == f0) break;            /* horizontal secant line: cannot continue */
        x2 = x1 - f1 * (x1 - x0) / (f1 - f0);
        record(&r, x2);
        if (x2 < a || x2 > b) {
            r.left_bracket = 1;
            break;
        }
        if (fabs(x2 - x1) < tol) {
            r.converged = 1;
            break;
        }
        x0 = x1;                        /* shift: keep the two newest points */
        f0 = f1;
        x1 = x2;
        f1 = f(x2);
        if (f1 == 0.0) {
            r.converged = 1;
            break;
        }
    }
    return r;
}

RootResult solve_root(RootMethod m, Func f, double a, double b, double tol)
{
    switch (m) {
    case R_BISECTION: return bisection(f, a, b, tol, ROOT_MAX_ITER);
    case R_REGULA:    return regula_falsi(f, a, b, tol, ROOT_MAX_ITER);
    case R_NEWTON:
        if (a == b) return bisection(f, a, b, tol, ROOT_MAX_ITER);   /* exact zero */
        return newton_raphson(f, 0.5 * (a + b), a, b, tol, ROOT_MAX_ITER);
    default:          return secant(f, a, b, a, b, tol, ROOT_MAX_ITER);
    }
}

int scan_brackets(Func f, double from, double to, double step, double br[][2], int max_br)
{
    int count = 0, k, n_steps = (int)floor((to - from) / step + 0.5);
    double x_prev = from, f_prev = f(from);

    if (f_prev == 0.0 && count < max_br) {          /* root exactly at the start */
        br[count][0] = br[count][1] = from;
        count++;
    }
    for (k = 1; k <= n_steps && count < max_br; k++) {
        double x = from + k * step, fx;
        if (x > to) x = to;
        fx = f(x);
        if (fx == 0.0) {                            /* root exactly at a check point */
            br[count][0] = br[count][1] = x;
            count++;
        } else if (f_prev * fx < 0.0) {             /* sign change: root in between */
            br[count][0] = x_prev;
            br[count][1] = x;
            count++;
        }
        x_prev = x;
        f_prev = fx;
    }
    return count;
}

double estimate_order(const RootResult *r, double x_start, double ref)
{
    double e[ROOT_MAX_ITER + 2], err = fabs(x_start - ref);
    int m = 0, k;
    /* keep errors that are still above rounding noise */
    if (err > 1e-12) e[m++] = err;
    for (k = 0; k < r->n_hist; k++) {
        err = fabs(r->hist[k] - ref);
        if (err > 1e-12) e[m++] = err;
    }
    if (m < 3) return 0.0;
    /* p = ln(e_k+1 / e_k) / ln(e_k / e_k-1) using the last three errors */
    return log(e[m - 1] / e[m - 2]) / log(e[m - 2] / e[m - 3]);
}
