/*
 * web_export.h - results as JSON for the web front-end (web/index.html).
 *
 * The web page never computes anything itself: every number it shows is
 * produced here by the same C functions the menu uses.
 */
#ifndef WEB_EXPORT_H
#define WEB_EXPORT_H

#include <stdio.h>
#include "dataset.h"
#include "analysis.h"

/* All results for dataset d with settings s, as one JSON object.
 * probe_t >= 0 also adds the estimates (and divided-difference table) at time probe_t. */
void write_json(FILE *fp, const Dataset *d, const Settings *s, double probe_t);

/* Run every test case and write the results as a JSON object. */
void write_tests_json(FILE *fp);

/* Write web/data/results.js (both datasets + tests, default settings) so the
 * page also works when opened directly from disk. Returns 1 on success. */
int export_web(void);

#endif
