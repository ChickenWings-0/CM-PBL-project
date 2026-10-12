/*
 * load_model.h - the synthetic "true" load curve of a Delhi household
 * on a summer day (Tier B data). Because P(t) is a known formula, the exact
 * missing values, exact energy and exact crossing times are all known.
 */
#ifndef LOAD_MODEL_H
#define LOAD_MODEL_H

/* Power demand in kW at time t (hours, 0..24). */
double model_power(double t);

/* Exact energy in kWh between times a and b: integral of model_power. */
double model_energy(double a, double b);

/* Write data/synthetic_day.csv: hourly readings, 4 decimals, hours 4,9,13,14,19 = NA.
 * Returns 1 on success. */
int generate_synthetic_csv(const char *path);

#endif
