/*
 * interpolation.h - estimating power at a time where no reading exists.
 *
 * All functions take the KNOWN readings x[0..n-1] (times, increasing)
 * and y[0..n-1] (power), and return the estimate at time t.
 */
#ifndef INTERPOLATION_H
#define INTERPOLATION_H

#include "dataset.h"

typedef enum {
    M_LINEAR = 0,       /* straight line between neighbours                  */
    M_LAGRANGE,         /* local cubic (4 nearest points), Lagrange form      */
    M_NDD,              /* same local cubic, Newton divided-difference form   */
    M_GLOBAL,           /* one polynomial through ALL points (Runge demo)     */
    N_INTERP
} InterpMethod;

extern const char *INTERP_NAMES[N_INTERP];      /* long names  */
extern const char *INTERP_SHORT[N_INTERP];      /* short names */

/* Index i of the interval x[i] <= t <= x[i+1]. */
int find_interval(const double x[], int n, double t);

/* Linear interpolation between the two neighbours of t. */
double interp_linear(const double x[], const double y[], int n, double t);

/* First index s of the 4 points x[s..s+3] used by the local cubic at t. */
int local_stencil(const double x[], int n, double t);

/* Lagrange polynomial through all m points (x[0..m-1], y[0..m-1]). */
double lagrange_poly(const double x[], const double y[], int m, double t);

/* Divided-difference table for m (<= 4) points:
 * table[i][0] = y[i],  table[i][j] = f[x_i, ..., x_{i+j}]. */
void ndd_build_table(const double x[], const double y[], int m, double table[4][4]);

/* Evaluate the Newton form using the top row of the table (nested multiplication). */
double ndd_eval(const double x[], double table[4][4], int m, double t);

double interp_lagrange_local(const double x[], const double y[], int n, double t);
double interp_ndd_local(const double x[], const double y[], int n, double t);
double interp_lagrange_global(const double x[], const double y[], int n, double t);

/* Call the chosen method. */
double interp_eval(InterpMethod m, const double x[], const double y[], int n, double t);

/* out[i] = reading i if known, otherwise the estimate by method m. */
void fill_missing(const Dataset *d, InterpMethod m, double out[]);

#endif
