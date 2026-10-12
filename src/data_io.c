/*
 * data_io.c - reading CSV files of meter readings.
 *
 * File format (see docs/PLAN.md section 2.3):
 *     # comment lines start with '#'
 *     time_hours,power_kW        <- header line (optional)
 *     0,0.5000
 *     4,NA                       <- missing reading
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
#include "data_io.h"
#include "load_model.h"

#ifdef _WIN32
#include <direct.h>     /* _mkdir on Windows */
#else
#include <sys/stat.h>   /* mkdir on Linux/Mac */
#endif

/* Remove spaces, tabs and newline characters from both ends of s. */
static char *trim(char *s)
{
    char *end;
    while (isspace((unsigned char)*s)) s++;
    end = s + strlen(s);
    while (end > s && isspace((unsigned char)end[-1])) end--;
    *end = '\0';
    return s;
}

/* Returns 1 if the whole string s is a valid number, and stores it in *value. */
static int parse_number(const char *s, double *value)
{
    char *end;
    if (*s == '\0') return 0;
    *value = strtod(s, &end);
    return *end == '\0';
}

/*
 * Read a CSV file into arrays. Used by all the load_* functions below.
 * miss[i] is set to 1 for NA / empty power values.
 * Returns 1 on success, 0 on any error (with a message).
 */
static int parse_file(const char *path, double t[], double p[], int miss[], int *n_out)
{
    FILE *fp = fopen(path, "r");
    char line[256];
    int line_no = 0, n = 0, header_seen = 0;

    if (fp == NULL) {
        printf("  ERROR: cannot open file '%s'\n", path);
        return 0;
    }

    while (fgets(line, sizeof line, fp) != NULL) {
        char *s = trim(line), *comma, *time_str, *power_str;
        double tv, pv;
        line_no++;

        if (*s == '\0' || *s == '#') continue;          /* blank or comment */

        comma = strchr(s, ',');
        if (comma == NULL) {
            printf("  ERROR: line %d of '%s' has no comma: \"%s\"\n", line_no, path, s);
            fclose(fp);
            return 0;
        }
        *comma = '\0';                                    /* split the line */
        time_str = trim(s);
        power_str = trim(comma + 1);

        if (!parse_number(time_str, &tv)) {
            if (!header_seen && n == 0) {                 /* first text line = header */
                header_seen = 1;
                continue;
            }
            printf("  ERROR: line %d: time '%s' is not a number\n", line_no, time_str);
            fclose(fp);
            return 0;
        }
        if (n >= MAX_POINTS) {
            printf("  ERROR: too many rows (maximum %d)\n", MAX_POINTS);
            fclose(fp);
            return 0;
        }

        if (*power_str == '\0' || strcmp(power_str, "NA") == 0 || strcmp(power_str, "na") == 0) {
            miss[n] = 1;                                  /* missing reading */
            pv = 0.0;
        } else if (parse_number(power_str, &pv)) {
            miss[n] = 0;
        } else {
            printf("  ERROR: line %d: power '%s' is not a number (use NA for missing)\n",
                   line_no, power_str);
            fclose(fp);
            return 0;
        }

        if (n > 0 && tv <= t[n - 1]) {
            printf("  ERROR: line %d: times must be strictly increasing\n", line_no);
            fclose(fp);
            return 0;
        }
        t[n] = tv;
        p[n] = pv;
        n++;
    }
    fclose(fp);

    if (n < 2) {
        printf("  ERROR: '%s' must contain at least 2 readings\n", path);
        return 0;
    }
    *n_out = n;
    return 1;
}

int load_csv(const char *path, Dataset *d)
{
    int n;
    if (!parse_file(path, d->t, d->p, d->missing, &n)) return 0;

    /* Filling a missing FIRST or LAST reading would be extrapolation: not allowed. */
    if (d->missing[0] || d->missing[n - 1]) {
        printf("  ERROR: the first and last readings must not be missing\n"
               "         (estimating them would be extrapolation, which is unreliable).\n");
        return 0;
    }
    d->n = n;
    d->has_truth = 0;
    d->has_model = 0;
    d->has_fine = 0;
    d->n_fine = 0;
    return 1;
}

int load_reference(const char *path, Dataset *d)
{
    static double t[MAX_POINTS], p[MAX_POINTS];
    static int miss[MAX_POINTS];
    int n, i;

    if (!parse_file(path, t, p, miss, &n)) return 0;
    if (n != d->n) {
        printf("  ERROR: reference file has %d rows, dataset has %d\n", n, d->n);
        return 0;
    }
    for (i = 0; i < n; i++) {
        if (miss[i] || fabs(t[i] - d->t[i]) > 1e-9) {
            printf("  ERROR: reference file row %d does not match the dataset\n", i + 1);
            return 0;
        }
        d->truth[i] = p[i];
    }
    d->has_truth = 1;
    return 1;
}

int load_fine(const char *path, Dataset *d)
{
    static int miss[MAX_POINTS];
    int n, i;

    if (!parse_file(path, d->fine_t, d->fine_p, miss, &n)) return 0;
    for (i = 0; i < n; i++) {
        if (miss[i]) {
            printf("  ERROR: 1-minute reference data may not contain NA\n");
            return 0;
        }
    }
    d->n_fine = n;
    d->has_fine = 1;
    return 1;
}

int known_points(const Dataset *d, double x[], double y[])
{
    int i, k = 0;
    for (i = 0; i < d->n; i++) {
        if (!d->missing[i]) {
            x[k] = d->t[i];
            y[k] = d->p[i];
            k++;
        }
    }
    return k;
}

int count_missing(const Dataset *d)
{
    int i, c = 0;
    for (i = 0; i < d->n; i++) c += d->missing[i];
    return c;
}

void make_dir(const char *path)
{
#ifdef _WIN32
    _mkdir(path);           /* fails silently if it already exists - that is fine */
#else
    mkdir(path, 0755);
#endif
}

int load_synthetic_dataset(Dataset *d)
{
    FILE *fp = fopen(SYNTHETIC_CSV, "r");
    int i;
    if (fp == NULL) {                       /* first run: create the file */
        printf("  '%s' not found - generating it from the model.\n", SYNTHETIC_CSV);
        if (!generate_synthetic_csv(SYNTHETIC_CSV)) return 0;
    } else {
        fclose(fp);
    }
    if (!load_csv(SYNTHETIC_CSV, d)) return 0;
    strcpy(d->name, "Synthetic Delhi household, summer day (hourly, 5 readings lost)");
    strcpy(d->tag, "synthetic");
    for (i = 0; i < d->n; i++) d->truth[i] = model_power(d->t[i]);   /* exact values */
    d->has_truth = 1;
    d->has_model = 1;
    return 1;
}

int load_real_dataset(Dataset *d)
{
    if (!load_csv(REAL_CSV, d)) return 0;
    if (!load_reference(REAL_TRUTH_CSV, d)) return 0;
    if (!load_fine(REAL_MINUTE_CSV, d)) return 0;
    strcpy(d->name, "Real household, UCI dataset, 14-May-2007 (hourly, 5 readings hidden)");
    strcpy(d->tag, "real");
    return 1;
}
