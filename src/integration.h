/*
 * integration.h - energy = area under the power curve (kW x h = kWh).
 *
 * n is the number of POINTS; the number of intervals is n - 1.
 * Simpson rules need equal spacing h.
 * *warn is set to 1 if leftover intervals had to be done with the
 * trapezoidal rule (interval count not a multiple of 2 or 3), else 0.
 */
#ifndef INTEGRATION_H
#define INTEGRATION_H

typedef enum { Q_TRAP = 0, Q_SIMP13, Q_SIMP38, N_QUAD } QuadMethod;

extern const char *QUAD_NAMES[N_QUAD];

/* Composite trapezoidal rule; works for ANY spacing. */
double trapezoidal(const double x[], const double y[], int n);

/* Composite Simpson 1/3 (needs an even number of intervals). */
double simpson_13(const double y[], int n, double h, int *warn);

/* Composite Simpson 3/8 (needs a multiple of 3 intervals). */
double simpson_38(const double y[], int n, double h, int *warn);

/* 1 if x[] is equally spaced (within 1e-9), else 0. */
int is_equally_spaced(const double x[], int n);

/* Call the chosen rule on points (x[i], y[i]).
 * *status = 0 ok, 1 ok but leftover intervals used trapezoid,
 *           2 not possible (Simpson on unequal spacing) -> returns 0. */
double integrate_grid(QuadMethod m, const double x[], const double y[], int n, int *status);

#endif
