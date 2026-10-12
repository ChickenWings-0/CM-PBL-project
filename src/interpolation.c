/*
 * interpolation.c - Linear, Lagrange (local cubic), Newton Divided
 * Difference (local cubic) and global Lagrange interpolation.
 *
 * The local cubic uses the 4 known readings closest to t: two on each side
 * (x[i-1], x[i], x[i+1], x[i+2] for t in [x[i], x[i+1]]), shifted inward at
 * the ends. The 4 points are chosen PER INTERVAL, so the curves on both sides
 * of a reading pass through that reading: the curve is continuous.
 */
#include "interpolation.h"
#include "data_io.h"

const char *INTERP_NAMES[N_INTERP] = {
    "Linear", "Lagrange (local cubic)", "Newton Div. Diff. (local cubic)", "Global Lagrange"
};
const char *INTERP_SHORT[N_INTERP] = { "Linear", "Lagrange", "NDD", "Global" };

int find_interval(const double x[], int n, double t)
{
    int i = 0;
    /* move right while the next point is still to the left of t */
    while (i < n - 2 && x[i + 1] < t) i++;
    return i;
}

/*
 * Linear: P(t) = y0 + (y1 - y0) * (t - x0) / (x1 - x0)
 * Error is about (h^2 / 8) * max|P''|  ->  order h^2.
 */
double interp_linear(const double x[], const double y[], int n, double t)
{
    int i = find_interval(x, n, t);
    return y[i] + (y[i + 1] - y[i]) * (t - x[i]) / (x[i + 1] - x[i]);
}

int local_stencil(const double x[], int n, double t)
{
    int s = find_interval(x, n, t) - 1;     /* one point to the left of the interval */
    if (s < 0) s = 0;                       /* near the start: shift right           */
    if (s > n - 4) s = n - 4;               /* near the end: shift left              */
    return s;
}

/*
 * Lagrange form:  P(t) = sum_i  y_i * L_i(t)
 *                 L_i(t) = product over j != i of (t - x_j) / (x_i - x_j)
 * L_i is 1 at x_i and 0 at every other x_j, so P passes through every point.
 */
double lagrange_poly(const double x[], const double y[], int m, double t)
{
    double sum = 0.0;
    int i, j;
    for (i = 0; i < m; i++) {
        double L = 1.0;
        for (j = 0; j < m; j++)
            if (j != i) L *= (t - x[j]) / (x[i] - x[j]);
        sum += y[i] * L;
    }
    return sum;
}

/*
 * Divided differences:
 *   f[x_i]             = y_i
 *   f[x_i, ..., x_i+j] = ( f[x_i+1, ..., x_i+j] - f[x_i, ..., x_i+j-1] ) / (x_i+j - x_i)
 * Stored as table[i][j]. The Newton coefficients are the top row table[0][j].
 */
void ndd_build_table(const double x[], const double y[], int m, double table[4][4])
{
    int i, j;
    for (i = 0; i < 4; i++)
        for (j = 0; j < 4; j++) table[i][j] = 0.0;
    for (i = 0; i < m; i++) table[i][0] = y[i];
    for (j = 1; j < m; j++)
        for (i = 0; i < m - j; i++)
            table[i][j] = (table[i + 1][j - 1] - table[i][j - 1]) / (x[i + j] - x[i]);
}

/*
 * Newton form:
 *   P(t) = c0 + c1 (t-x0) + c2 (t-x0)(t-x1) + c3 (t-x0)(t-x1)(t-x2)
 * evaluated by nested multiplication (like Horner's rule):
 *   P = ((c3 (t-x2) + c2)(t-x1) + c1)(t-x0) + c0
 */
double ndd_eval(const double x[], double table[4][4], int m, double t)
{
    double result = table[0][m - 1];
    int j;
    for (j = m - 2; j >= 0; j--)
        result = result * (t - x[j]) + table[0][j];
    return result;
}

double interp_lagrange_local(const double x[], const double y[], int n, double t)
{
    int s;
    if (n < 4) return lagrange_poly(x, y, n, t);     /* fewer points: use them all */
    s = local_stencil(x, n, t);
    return lagrange_poly(x + s, y + s, 4, t);        /* x+s = address of x[s]     */
}

double interp_ndd_local(const double x[], const double y[], int n, double t)
{
    double table[4][4];
    int s = 0, m = 4;
    if (n < 4) m = n;
    else s = local_stencil(x, n, t);
    ndd_build_table(x + s, y + s, m, table);
    return ndd_eval(x + s, table, m, t);
}

/* One polynomial of degree n-1 through every known reading. */
double interp_lagrange_global(const double x[], const double y[], int n, double t)
{
    return lagrange_poly(x, y, n, t);
}

double interp_eval(InterpMethod m, const double x[], const double y[], int n, double t)
{
    switch (m) {
    case M_LINEAR:   return interp_linear(x, y, n, t);
    case M_LAGRANGE: return interp_lagrange_local(x, y, n, t);
    case M_NDD:      return interp_ndd_local(x, y, n, t);
    case M_GLOBAL:   return interp_lagrange_global(x, y, n, t);
    default:         return 0.0;
    }
}

void fill_missing(const Dataset *d, InterpMethod m, double out[])
{
    static double kx[MAX_POINTS], ky[MAX_POINTS];
    int nk = known_points(d, kx, ky), i;
    for (i = 0; i < d->n; i++)
        out[i] = d->missing[i] ? interp_eval(m, kx, ky, nk, d->t[i]) : d->p[i];
}
