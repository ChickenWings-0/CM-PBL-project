/*
 * dataset.h - the Dataset structure shared by every module.
 *
 * A Dataset holds one day of smart-meter readings:
 *   t[i]       time of reading i in decimal hours (18.5 = 18:30)
 *   p[i]       power reading in kW (meaningless if missing[i] == 1)
 *   missing[i] 1 if the reading was lost (NA in the CSV), else 0
 *
 * Optional extra information that makes error analysis possible:
 *   has_truth  true power values are known at EVERY time (truth[i])
 *   has_model  data came from the analytic synthetic model (load_model.c)
 *   has_fine   1-minute reference data is available (fine_t / fine_p)
 */
#ifndef DATASET_H
#define DATASET_H

#define MAX_POINTS 2000     /* enough for 1441 one-minute readings */

typedef struct {
    char name[96];          /* description shown in headers                */
    char tag[16];           /* "synthetic", "real" or "custom" (output dir) */

    int n;                  /* number of readings (known + missing)        */
    double t[MAX_POINTS];   /* time (hours)                                */
    double p[MAX_POINTS];   /* power (kW)                                  */
    int missing[MAX_POINTS];/* 1 = reading missing                         */

    int has_truth;          /* 1 if truth[] is valid                       */
    double truth[MAX_POINTS];

    int has_model;          /* 1 if the exact model P(t) is the truth      */

    int has_fine;           /* 1 if 1-minute reference data is loaded      */
    int n_fine;
    double fine_t[MAX_POINTS];
    double fine_p[MAX_POINTS];
} Dataset;

#endif
