/*
 * main.c - Smart Energy Consumption Prediction & Analysis System
 *
 * Computational Methods Lab (ES-251), Delhi Technical Campus (GGSIPU)
 * Team: Vaibhav (Team Leader, 01118015625), Aksh Rastogi (02518015625),
 *       Akshay Kumar (00618015625), Apurva Kr. Yadav (02418015625),
 *       Vaibhav Sharma (00518015625)
 *
 * Menu-driven program. Run from the project folder (it reads data/ and
 * writes output/).
 *
 *   energy.exe               interactive menu
 *   energy.exe --test        run the test cases only
 *   energy.exe --export-all  write all result files for both datasets + tests
 *   energy.exe --export-web  write web/data/results.js (snapshot for the web page)
 *   energy.exe --json ...    results as JSON for web/serve.py (see json_command)
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "dataset.h"
#include "data_io.h"
#include "load_model.h"
#include "interpolation.h"
#include "analysis.h"
#include "report.h"
#include "tests.h"
#include "web_export.h"

static Dataset g_data;          /* the dataset currently loaded           */
static Dataset g_temp;          /* used while loading, so a failed load   */
                                /* does not destroy the current dataset   */

static void print_menu(const Dataset *d, const Settings *s)
{
    printf("\n=================== MAIN MENU ===================\n");
    printf(" Dataset: %s\n", d->name);
    printf(" Gap filling for energy/bill: %s\n", INTERP_NAMES[s->fill]);
    printf("-------------------------------------------------\n");
    printf(" 1. Load dataset\n");
    printf(" 2. View readings and load-curve chart\n");
    printf(" 3. Fill missing values (interpolation)\n");
    printf(" 4. Energy calculation (integration)\n");
    printf(" 5. Threshold and energy-budget analysis (root finding)\n");
    printf(" 6. Bill estimate (flat Rs/kWh rate)\n");
    printf(" 7. Error analysis and method comparison\n");
    printf(" 8. Run test cases\n");
    printf(" 9. Export all results to output/\n");
    printf(" 0. Exit\n");
    printf("=================================================\n");
}

static void menu_load(void)
{
    char path[200];
    int c, ok = 0;

    printf("\n  1) Synthetic Delhi household day (%s)\n", SYNTHETIC_CSV);
    printf("  2) Real household day, UCI 14-May-2007 (%s)\n", REAL_CSV);
    printf("  3) Custom CSV file (format: time_hours,power_kW  - NA for missing)\n");
    printf("  4) Regenerate synthetic CSV from the model\n");
    c = read_int("  Choice: ", 1, 4);
    if (c == 1) {
        ok = load_synthetic_dataset(&g_temp);
    } else if (c == 2) {
        ok = load_real_dataset(&g_temp);
    } else if (c == 3) {
        printf("  File path (e.g. data/custom_template.csv): ");
        if (!read_line(path, sizeof path) || path[0] == '\0') return;
        ok = load_csv(path, &g_temp);
        if (ok) {
            sprintf(g_temp.name, "Custom: %.80s", path);
            strcpy(g_temp.tag, "custom");
        }
    } else if (c == 4) {
        if (generate_synthetic_csv(SYNTHETIC_CSV)) {
            printf("  %s regenerated.\n", SYNTHETIC_CSV);
            ok = load_synthetic_dataset(&g_temp);
        }
    }
    if (ok) {
        g_data = g_temp;                    /* copy the whole structure */
        printf("  Loaded: %s\n  %d readings, %d missing.\n", g_data.name, g_data.n, count_missing(&g_data));
    } else if (c > 0) {
        printf("  Dataset NOT changed.\n");
    }
}

static void menu_fill(Settings *s)
{
    int c;
    printf("\n  1) Linear\n  2) Lagrange (local cubic)\n  3) Newton Divided Difference (local cubic)\n");
    printf("  4) Global Lagrange (Runge demonstration)\n  5) Compare all methods\n");
    c = read_int("  Choice: ", 1, 5);
    if (c < 1) return;
    show_fill_missing(&g_data, c - 1);
    if (c <= 3) {
        s->fill = (InterpMethod)(c - 1);
        printf("\n  '%s' will now be used to fill gaps for energy and bill (options 4, 6).\n",
               INTERP_NAMES[s->fill]);
    }
}

static void ask_peak_window(Settings *s)
{
    double t0 = g_data.t[0], tn = g_data.t[g_data.n - 1];
    /* if the old window does not fit this dataset, offer the whole day instead */
    if (s->peak_start < t0 || s->peak_start > tn) s->peak_start = t0;
    if (s->peak_end < s->peak_start || s->peak_end > tn) s->peak_end = tn;
    s->peak_start = read_double_default("  Peak window start (hours)", s->peak_start, t0, tn);
    s->peak_end = read_double_default("  Peak window end (hours)", s->peak_end, s->peak_start, tn);
}

static void menu_analysis(const Settings *s)
{
    int c;
    printf("\n  1) Interpolation errors (gaps, divided differences, Runge, leave-one-out)\n");
    printf("  2) Integration errors (vs reference, error through the chain, convergence)\n");
    printf("  3) Root finding comparison (solver error vs model error, iterations)\n");
    printf("  4) All of the above\n");
    c = read_int("  Choice: ", 1, 4);
    if (c == 1 || c == 4) analysis_interpolation(&g_data, NULL);
    if (c == 2 || c == 4) analysis_integration(&g_data, s, NULL);
    if (c == 3 || c == 4) analysis_roots(&g_data, s, NULL);
}

/* Export both built-in datasets and the test results (used by --export-all). */
static int export_everything(void)
{
    Settings s = default_settings();
    FILE *fp;
    int failed;

    if (load_synthetic_dataset(&g_data)) export_all(&g_data, &s);
    if (load_real_dataset(&g_data)) export_all(&g_data, &s);

    make_dir("output");
    fp = fopen("output/test_results.txt", "w");
    report_set_files(fp, NULL);
    print_header();
    failed = run_all_tests();
    report_set_files(NULL, NULL);
    if (fp) fclose(fp);
    printf("  Test results written to output/test_results.txt\n");
    export_web();                           /* snapshot for the web page */
    return failed;
}

/* Write {"error": "..."} so the web page can show what went wrong. */
static int json_error(FILE *fp, const char *msg)
{
    fprintf(fp, "{\"error\":\"%s\"}", msg);
    fclose(fp);
    return 2;
}

/*
 * Used by the web page (through web/serve.py):
 *   energy.exe --json <synthetic|real> <outfile> [threshold budget peak_start peak_end rate fill probe]
 * Missing values keep their defaults. probe = time for an extra estimate (-1 = none).
 */
static int json_command(int argc, char *argv[])
{
    Settings s = default_settings();
    double v[7], probe = -1.0, t0, tn;
    int i, nv = 0, ok;
    FILE *fp;

    if (argc < 4) {
        printf("usage: energy.exe --json <synthetic|real> <outfile> [threshold budget "
               "peak_start peak_end rate fill probe]\n");
        return 2;
    }
    fp = fopen(argv[3], "w");
    if (fp == NULL) return 2;

    if (strcmp(argv[2], "synthetic") == 0) ok = load_synthetic_dataset(&g_data);
    else if (strcmp(argv[2], "real") == 0) ok = load_real_dataset(&g_data);
    else ok = 0;
    if (!ok) return json_error(fp, "unknown dataset or data files missing");

    for (i = 4; i < argc && nv < 7; i++) {
        char *end;
        v[nv] = strtod(argv[i], &end);
        if (end == argv[i] || *end != '\0' || !isfinite(v[nv]))
            return json_error(fp, "a parameter is not a number");
        nv++;
    }
    t0 = g_data.t[0];
    tn = g_data.t[g_data.n - 1];
    if (nv > 0) s.threshold = v[0];
    if (nv > 1) s.budget = v[1];
    if (nv > 2) s.peak_start = v[2];
    if (nv > 3) s.peak_end = v[3];
    if (nv > 4) s.rate = v[4];
    if (nv > 5) s.fill = (InterpMethod)(int)v[5];
    if (nv > 6) probe = v[6];

    if (s.threshold <= 0.0) return json_error(fp, "threshold must be above 0 kW");
    if (s.budget <= 0.0) return json_error(fp, "budget must be above 0 kWh");
    if (s.peak_start < t0 || s.peak_end > tn || s.peak_start >= s.peak_end)
        return json_error(fp, "peak window must lie inside the day and start before it ends");
    if (s.rate < 0.0) return json_error(fp, "rate cannot be negative");
    if ((int)s.fill < 0 || (int)s.fill > M_NDD)
        return json_error(fp, "fill method must be 0 (Linear), 1 (Lagrange) or 2 (NDD)");

    write_json(fp, &g_data, &s, probe);
    fclose(fp);
    return 0;
}

int main(int argc, char *argv[])
{
    Settings s = default_settings();
    int choice;

    if (argc > 1 && strcmp(argv[1], "--test") == 0) {
        print_header();
        return run_all_tests() == 0 ? 0 : 1;
    }
    if (argc > 1 && strcmp(argv[1], "--export-all") == 0)
        return export_everything() == 0 ? 0 : 1;
    if (argc > 1 && strcmp(argv[1], "--export-web") == 0)
        return export_web() ? 0 : 1;
    if (argc > 1 && strcmp(argv[1], "--json") == 0)
        return json_command(argc, argv);
    if (argc > 2 && strcmp(argv[1], "--json-tests") == 0) {
        FILE *fp = fopen(argv[2], "w");
        if (fp == NULL) return 2;
        write_tests_json(fp);
        fclose(fp);
        return 0;
    }

    print_header();
    if (!load_synthetic_dataset(&g_data)) {
        printf("Could not load the synthetic dataset. Run the program from the project folder.\n");
        return 1;
    }
    printf("\n  Loaded: %s\n", g_data.name);

    for (;;) {
        print_menu(&g_data, &s);
        choice = read_int(" Enter choice: ", 0, 9);
        if (choice <= 0) break;                 /* 0 = exit, -1 = input ended */

        switch (choice) {
        case 1: menu_load(); break;
        case 2: show_readings(&g_data, &s); break;
        case 3: menu_fill(&s); break;
        case 4:
            ask_peak_window(&s);
            show_energy(&g_data, &s);
            break;
        case 5:
            s.threshold = read_double_default("  Power threshold (kW)", s.threshold, 0.0, 1000.0);
            s.budget = read_double_default("  Daily energy budget (kWh)", s.budget, 0.0, 100000.0);
            show_threshold(&g_data, &s);
            break;
        case 6:
            s.rate = read_double_default("  Electricity rate (Rs per kWh)", s.rate, 0.0, 1000.0);
            show_bill(&g_data, &s);
            break;
        case 7: menu_analysis(&s); break;
        case 8: run_all_tests(); break;
        case 9: export_all(&g_data, &s); break;
        }
        if (input_ended()) break;
    }
    printf("\n  Thank you for using the Smart Energy Analysis System. Goodbye!\n");
    return 0;
}
