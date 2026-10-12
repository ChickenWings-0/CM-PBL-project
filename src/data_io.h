/*
 * data_io.h - reading and writing CSV files.
 */
#ifndef DATA_IO_H
#define DATA_IO_H

#include "dataset.h"

/* Read "time_hours,power_kW" rows into d. NA or empty power = missing.
 * Returns 1 on success, 0 on error (a message is printed).            */
int load_csv(const char *path, Dataset *d);

/* Read a complete file (no NA allowed) with the same times as d into d->truth. */
int load_reference(const char *path, Dataset *d);

/* Read 1-minute reference data into d->fine_t / d->fine_p. */
int load_fine(const char *path, Dataset *d);

/* Copy only the known (non-missing) readings into x[], y[]. Returns count. */
int known_points(const Dataset *d, double x[], double y[]);

/* Number of missing readings in d. */
int count_missing(const Dataset *d);

/* Create a folder if it does not exist (works on Windows and Linux). */
void make_dir(const char *path);

/* ----- the two built-in datasets (paths relative to the project folder) ----- */
#define SYNTHETIC_CSV  "data/synthetic_day.csv"
#define REAL_CSV       "data/real_day_hourly.csv"
#define REAL_TRUTH_CSV "data/real_day_hourly_complete.csv"
#define REAL_MINUTE_CSV "data/real_day_minute.csv"

/* Load the synthetic day (creates the CSV from the model if it is missing).
 * True values come from the exact model. Returns 1 on success. */
int load_synthetic_dataset(Dataset *d);

/* Load the real UCI day with its hourly ground truth and 1-minute data. */
int load_real_dataset(Dataset *d);

#endif
