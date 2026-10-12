/*
 * report.h - everything about printing results and reading user input.
 *
 * rprintf() works like printf(), but can ALSO copy the text into up to two
 * files (used by "Export all results"). This way the same code produces
 * the screen output and the .txt report files.
 */
#ifndef REPORT_H
#define REPORT_H

#include <stdio.h>
#include "dataset.h"

/* printf to the screen (if enabled) and to the attached files (if any). */
void rprintf(const char *fmt, ...);

/* Attach up to two files that receive a copy of rprintf output (NULL = none). */
void report_set_files(FILE *f1, FILE *f2);

/* 1 = rprintf also prints to the screen (default), 0 = files only. */
void report_set_screen(int on);

/* Title box with project name, course and team members. */
void print_header(void);

/* A line of 'width' copies of character c. */
void print_rule(char c, int width);

/* Section title with '=' underline. */
void print_title(const char *title);

/* Hours -> "HH:MM" and "HH:MM:SS" (buf needs 12 chars). */
const char *fmt_hhmm(double hours, char *buf);
const char *fmt_hhmmss(double hours, char *buf);

/* Horizontal bar chart of the readings with the threshold line.
 * filled[] holds the values to draw (estimates for missing readings). */
void ascii_chart(const Dataset *d, const double filled[], double threshold);

/* ----- input helpers (they print prompts with printf, never to files) ----- */

/* Read one line into buf. Returns 0 at end of input. */
int read_line(char *buf, int size);

/* Ask for an integer in [min, max] until valid. Returns -1 at end of input. */
int read_int(const char *prompt, int min, int max);

/* Ask for a number in [min, max]; pressing Enter keeps the default. */
double read_double_default(const char *prompt, double def, double min, double max);

/* 1 once the input has ended (e.g. piped input ran out). */
int input_ended(void);

#endif
