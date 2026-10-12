/*
 * tests.h - automated test cases (see docs/PLAN.md section 7).
 */
#ifndef TESTS_H
#define TESTS_H

#define MAX_TESTS 40

/* One test result (kept so the web page can show the table). */
typedef struct {
    char id[8];
    char what[64];
    char detail[64];
    int passed;
} TestRow;

/* Run T1-T15 and S1-S6, print a PASS/FAIL table. Returns the number of failures. */
int run_all_tests(void);

/* Results of the last run_all_tests(): count, and row i (0 .. count-1). */
int test_count(void);
const TestRow *test_row(int i);

#endif
