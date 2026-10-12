/*
 * analysis.h - the energy chain (interpolate -> integrate -> root-find),
 * the menu features and all error-analysis experiments.
 */
#ifndef ANALYSIS_H
#define ANALYSIS_H

#include "dataset.h"
#include "interpolation.h"
#include "integration.h"
#include "root_finding.h"

/* User settings (defaults in main.c: 3 kW, 15 kWh, 18-22 h, Rs 8/kWh, Lagrange). */
typedef struct {
    double threshold;       /* load limit, kW                       */
    double budget;          /* daily energy budget, kWh             */
    double peak_start;      /* peak window start, hours             */
    double peak_end;        /* peak window end, hours               */
    double rate;            /* electricity rate, Rs per kWh         */
    InterpMethod fill;      /* method used to fill gaps before integration */
} Settings;

/* 3 kW, 15 kWh, 18:00-22:00, Rs 8/kWh, Lagrange local cubic. */
Settings default_settings(void);

/* ---------- the continuous curve used for root finding ---------- */

/* Build the curve P~(t) from the KNOWN readings of d using method m. */
void curve_set(const Dataset *d, InterpMethod m);

/* P~(t): interpolated power at any time t. */
double curve_power(double t);

/* E~(t): energy from the first reading up to t, integrating P~ exactly
 * (Simpson 1/3 on each interval is exact for cubics). */
double curve_energy_upto(double t);

/* Set the threshold (kW) and budget (kWh) used by f_power / f_energy. */
void set_targets(double threshold, double budget);

double f_power(double t);    /* P~(t) - threshold */
double f_energy(double t);   /* E~(t) - budget    */

/* ---------- reference ("true") answers ---------- */

/* 1 if d has an exact reference (model or 1-minute data), else 0. */
int has_reference(const Dataset *d);

/* Exact / metered energy between a and b. */
double reference_energy(const Dataset *d, double a, double b);

/* True crossing times of 'threshold' (dir[k] = +1 upward, -1 downward).
 * Returns the count, or -1 if no reference is available. */
int true_power_crossings(const Dataset *d, double threshold, double t_out[], int dir[], int max);

/* True time when the cumulative energy reaches 'budget' (-1 if never / unknown). */
double true_budget_time(const Dataset *d, double budget);

/* Fill gaps with 'fill', then integrate between a and b (must be reading times)
 * with rule q. *status as in integrate_grid, or 3 if a/b are not reading times. */
double energy_with_fill(const Dataset *d, InterpMethod fill, QuadMethod q,
                        double a, double b, int *status);

/* Integrate values y[] (one per reading of d) between reading times a and b. */
double energy_of_array(const Dataset *d, const double y[], QuadMethod q,
                       double a, double b, int *status);

/* True power at time t: the exact model, or the nearest 1-minute reading. */
double reference_power(const Dataset *d, double t);

/* ---------- menu features ---------- */
void show_readings(const Dataset *d, const Settings *s);
void show_fill_missing(const Dataset *d, int choice);   /* choice 0..3 one method, 4 all */
void show_energy(const Dataset *d, const Settings *s);
void show_threshold(const Dataset *d, const Settings *s);
void show_bill(const Dataset *d, const Settings *s);

/* ---------- error analysis (csv_dir = folder for CSV files, or NULL) ---------- */
void analysis_interpolation(const Dataset *d, const char *csv_dir);
void analysis_integration(const Dataset *d, const Settings *s, const char *csv_dir);
void analysis_roots(const Dataset *d, const Settings *s, const char *csv_dir);

/* Write every result file for dataset d into output/<tag>/ (see PLAN.md section 8). */
void export_all(const Dataset *d, const Settings *s);

#endif
