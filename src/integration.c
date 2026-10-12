/*
 * integration.c - composite Trapezoidal, Simpson 1/3 and Simpson 3/8 rules.
 *
 *   Trapezoidal : I = h/2 [y0 + 2(y1 + ... + y_{m-1}) + ym]           error O(h^2)
 *   Simpson 1/3 : I = h/3 [y0 + 4(odd) + 2(even) + ym]                error O(h^4)
 *   Simpson 3/8 : I = 3h/8 [y0 + 3(y1 + y2) + 2 y3 + 3(y4 + y5) + ... + ym]   error O(h^4)
 * (m = number of intervals)
 */
#include <math.h>
#include "integration.h"

const char *QUAD_NAMES[N_QUAD] = { "Trapezoidal", "Simpson 1/3", "Simpson 3/8" };

/* Adds up the area of each trapezoid separately, so spacing may vary. */
double trapezoidal(const double x[], const double y[], int n)
{
    double sum = 0.0;
    int i;
    for (i = 0; i < n - 1; i++)
        sum += 0.5 * (y[i] + y[i + 1]) * (x[i + 1] - x[i]);
    return sum;
}

/* Trapezoidal rule on equally spaced y[from..to] (used for leftover intervals). */
static double trap_equal(const double y[], int from, int to, double h)
{
    double sum = 0.0;
    int i;
    for (i = from; i < to; i++)
        sum += 0.5 * h * (y[i] + y[i + 1]);
    return sum;
}

double simpson_13(const double y[], int n, double h, int *warn)
{
    int m = n - 1;                  /* number of intervals                   */
    int ms = (m % 2 == 0) ? m : m - 1;  /* largest even number <= m          */
    double sum;
    int i;

    *warn = (ms != m);
    if (ms == 0) {                  /* only 1 interval: Simpson impossible   */
        *warn = 1;
        return trap_equal(y, 0, m, h);
    }
    sum = y[0] + y[ms];
    for (i = 1; i < ms; i++)
        sum += (i % 2 == 1 ? 4.0 : 2.0) * y[i];   /* odd index x4, even index x2 */
    return h / 3.0 * sum + trap_equal(y, ms, m, h);   /* leftover interval (if any) */
}

double simpson_38(const double y[], int n, double h, int *warn)
{
    int m = n - 1;
    int ms = m - m % 3;             /* largest multiple of 3 <= m            */
    double sum;
    int i;

    *warn = (ms != m);
    if (ms == 0) {
        *warn = 1;
        return trap_equal(y, 0, m, h);
    }
    sum = y[0] + y[ms];
    for (i = 1; i < ms; i++)
        sum += (i % 3 == 0 ? 2.0 : 3.0) * y[i];   /* every 3rd point x2, others x3 */
    return 3.0 * h / 8.0 * sum + trap_equal(y, ms, m, h);
}

/* Times in a CSV are rounded (e.g. 1 minute = 0.016667 h), so spacings are
 * compared with a small tolerance relative to h (0.01 % of h). */
int is_equally_spaced(const double x[], int n)
{
    double h = x[1] - x[0];
    int i;
    for (i = 1; i < n - 1; i++)
        if (fabs((x[i + 1] - x[i]) - h) > 1e-4 * fabs(h)) return 0;
    return 1;
}

double integrate_grid(QuadMethod m, const double x[], const double y[], int n, int *status)
{
    int warn = 0;
    double result;

    if (m == Q_TRAP) {
        *status = 0;
        return trapezoidal(x, y, n);
    }
    if (n < 2 || !is_equally_spaced(x, n)) {
        *status = 2;                /* Simpson needs equal spacing */
        return 0.0;
    }
    if (m == Q_SIMP13) result = simpson_13(y, n, x[1] - x[0], &warn);
    else               result = simpson_38(y, n, x[1] - x[0], &warn);
    *status = warn;
    return result;
}
