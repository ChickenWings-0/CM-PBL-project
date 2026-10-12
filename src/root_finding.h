/*
 * root_finding.h - solving f(t) = 0, e.g. P(t) - 3 kW = 0.
 */
#ifndef ROOT_FINDING_H
#define ROOT_FINDING_H

#define ROOT_MAX_ITER 100
#define ROOT_TOL 1e-6           /* hours (= 3.6 milliseconds) */
#define SCAN_STEP 0.25          /* hours (= 15 minutes)       */

typedef double (*Func)(double); /* pointer to a function f(t) */

typedef enum { R_BISECTION = 0, R_REGULA, R_NEWTON, R_SECANT, N_ROOT } RootMethod;

extern const char *ROOT_NAMES[N_ROOT];

typedef struct {
    double root;                    /* final answer                              */
    int iterations;                 /* number of iterations used                 */
    int converged;                  /* 1 if the tolerance was met                */
    int left_bracket;               /* 1 if an open method jumped outside [a,b]  */
    int n_hist;                     /* number of entries in hist[]               */
    double hist[ROOT_MAX_ITER + 1]; /* estimate after each iteration             */
} RootResult;

RootResult bisection(Func f, double a, double b, double tol, int max_iter);
RootResult regula_falsi(Func f, double a, double b, double tol, int max_iter);
RootResult newton_raphson(Func f, double x0, double a, double b, double tol, int max_iter);
RootResult secant(Func f, double x0, double x1, double a, double b, double tol, int max_iter);

/* Run method m on bracket [a,b] with the standard starting values:
 * bisection/regula falsi use [a,b], Newton starts at (a+b)/2, secant at a and b. */
RootResult solve_root(RootMethod m, Func f, double a, double b, double tol);

/* Slope estimate f'(x) = [f(x+d) - f(x-d)] / (2d), d = 1e-5. */
double central_diff(Func f, double x);

/* Check f every 'step' from 'from' to 'to'. Every sign change gives one bracket
 * br[k][0..1]. If f is exactly 0 at a check point, the bracket is [x, x].
 * Returns the number of brackets found (at most max_br). */
int scan_brackets(Func f, double from, double to, double step, double br[][2], int max_br);

/* Observed order of convergence p from the last 3 usable errors |x_k - ref|.
 * The error sequence is: starting value x_start, then every iterate.
 * Errors below 1e-12 (rounding noise) are ignored.
 * Returns 0 if fewer than 3 usable errors exist. */
double estimate_order(const RootResult *r, double x_start, double ref);

#endif
