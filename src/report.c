/*
 * report.c - printing (screen + file copies), tables, ASCII chart and
 * safe input functions.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <math.h>
#include <ctype.h>
#include "report.h"

static FILE *g_file1 = NULL;     /* files that receive a copy of the output */
static FILE *g_file2 = NULL;
static int g_screen = 1;         /* print to the screen?                    */
static int g_input_ended = 0;

void report_set_files(FILE *f1, FILE *f2)
{
    g_file1 = f1;
    g_file2 = f2;
}

void report_set_screen(int on)
{
    g_screen = on;
}

/*
 * The old Windows C library (used by MinGW) prints exponents with 3 digits,
 * e.g. 1.23e-005. Standard C prints 1.23e-05. This changes the 3-digit form
 * to the 2-digit form and shifts the number one place right, so table
 * columns stay aligned. On Linux nothing needs changing.
 */
static void fix_exponents(char *s)
{
    char *p = s;
    while ((p = strchr(p, 'e')) != NULL) {
        if (p > s && isdigit((unsigned char)p[-1]) && (p[1] == '+' || p[1] == '-') &&
            p[2] == '0' && isdigit((unsigned char)p[3]) && isdigit((unsigned char)p[4]) &&
            !isdigit((unsigned char)p[5])) {
            char *start = p;                 /* find where the number begins */
            while (start > s && (isdigit((unsigned char)start[-1]) || start[-1] == '.' ||
                                 start[-1] == '-' || start[-1] == '+'))
                start--;
            memmove(start + 1, start, (size_t)(p + 2 - start));  /* overwrite the extra '0' */
            *start = ' ';
        }
        p++;
    }
}

/* Format the text once, then send it to the screen and the attached files. */
void rprintf(const char *fmt, ...)
{
    static char text[4096];
    va_list args;
    va_start(args, fmt);
    vsnprintf(text, sizeof text, fmt, args);
    va_end(args);
    fix_exponents(text);

    if (g_screen) fputs(text, stdout);
    if (g_file1 != NULL) fputs(text, g_file1);
    if (g_file2 != NULL) fputs(text, g_file2);
}

void print_rule(char c, int width)
{
    int i;
    for (i = 0; i < width; i++) rprintf("%c", c);
    rprintf("\n");
}

void print_title(const char *title)
{
    int len = (int)strlen(title);
    rprintf("\n%s\n", title);
    print_rule('=', len);
}

void print_header(void)
{
    rprintf("+==============================================================================+\n");
    rprintf("|          SMART ENERGY CONSUMPTION PREDICTION & ANALYSIS SYSTEM               |\n");
    rprintf("|     Computational Methods Lab (ES-251) - Project Based Learning              |\n");
    rprintf("|     Delhi Technical Campus (GGSIPU)                                          |\n");
    rprintf("|     Methods: Interpolation | Integration | Root Finding                      |\n");
    rprintf("+------------------------------------------------------------------------------+\n");
    rprintf("|  Team: %-70s|\n", "1. Vaibhav (Team Leader)   Enrollment No. 01118015625");
    rprintf("|        %-70s|\n", "2. Aksh Rastogi            Enrollment No. 02518015625");
    rprintf("|        %-70s|\n", "3. Akshay Kumar            Enrollment No. 00618015625");
    rprintf("|        %-70s|\n", "4. Apurva Kr. Yadav        Enrollment No. 02418015625");
    rprintf("|        %-70s|\n", "5. Vaibhav Sharma          Enrollment No. 00518015625");
    rprintf("+==============================================================================+\n");
}

const char *fmt_hhmm(double hours, char *buf)
{
    long minutes = (long)floor(hours * 60.0 + 0.5);       /* round to nearest minute */
    sprintf(buf, "%02ld:%02ld", minutes / 60, minutes % 60);
    return buf;
}

const char *fmt_hhmmss(double hours, char *buf)
{
    long seconds = (long)floor(hours * 3600.0 + 0.5);     /* round to nearest second */
    sprintf(buf, "%02ld:%02ld:%02ld", seconds / 3600, (seconds / 60) % 60, seconds % 60);
    return buf;
}

/*
 * Example row:   19:00 |##########################|##########     4.558  ABOVE LIMIT
 * '#' = measured reading, '+' = estimated (missing) reading, '|' = threshold.
 */
void ascii_chart(const Dataset *d, const double filled[], double threshold)
{
    const int WIDTH = 50;
    double max_p = threshold, scale;
    int i, col, step = 1, th_col;
    char buf[12];

    for (i = 0; i < d->n; i++)
        if (filled[i] > max_p) max_p = filled[i];
    scale = WIDTH / (max_p * 1.05);                      /* characters per kW */
    th_col = (int)floor(threshold * scale + 0.5);
    if (d->n > 50) step = (d->n + 49) / 50;              /* at most ~50 rows  */

    rprintf("\n  Load curve  ('#' measured, '+' estimated, '|' = %.2f kW limit, 1 char = %.3f kW)\n",
            threshold, 1.0 / scale);
    rprintf("  ------+%s\n", "--------------------------------------------------");
    for (i = 0; i < d->n; i += step) {
        int len = (int)floor(filled[i] * scale + 0.5);
        rprintf("  %s |", fmt_hhmm(d->t[i], buf));
        for (col = 1; col <= WIDTH; col++) {
            if (col == th_col) rprintf("|");
            else if (col <= len) rprintf("%c", d->missing[i] ? '+' : '#');
            else rprintf(" ");
        }
        rprintf(" %6.3f%s%s\n", filled[i], d->missing[i] ? " est." : "     ",
                filled[i] > threshold ? "  ABOVE LIMIT" : "");
    }
    rprintf("  ------+%s\n", "--------------------------------------------------");
}

/* ------------------------------ input ------------------------------ */

int input_ended(void)
{
    return g_input_ended;
}

int read_line(char *buf, int size)
{
    if (fgets(buf, size, stdin) == NULL) {
        g_input_ended = 1;
        buf[0] = '\0';
        return 0;
    }
    buf[strcspn(buf, "\r\n")] = '\0';                   /* remove the newline */
    return 1;
}

int read_int(const char *prompt, int min, int max)
{
    char buf[64], *end;
    long v;
    for (;;) {
        printf("%s", prompt);
        if (!read_line(buf, sizeof buf)) {
            printf("\n");
            return -1;                                   /* input ended */
        }
        v = strtol(buf, &end, 10);
        while (*end == ' ') end++;
        if (end != buf && *end == '\0' && v >= min && v <= max) return (int)v;
        printf("  Invalid choice. Please enter a whole number from %d to %d.\n", min, max);
    }
}

double read_double_default(const char *prompt, double def, double min, double max)
{
    char buf[64], *end;
    double v;
    for (;;) {
        printf("%s [default %.2f]: ", prompt, def);
        if (!read_line(buf, sizeof buf) || buf[0] == '\0') return def;   /* Enter = default */
        v = strtod(buf, &end);
        while (*end == ' ') end++;
        if (end != buf && *end == '\0' && v >= min && v <= max) return v;
        printf("  Invalid value. Enter a number from %.2f to %.2f, or press Enter.\n", min, max);
    }
}
