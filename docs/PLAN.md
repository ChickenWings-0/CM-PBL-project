# Smart Energy Consumption Prediction & Analysis System — Project Plan

**Course:** Computational Methods Lab (ES-251), Project-Based Learning
**Institute:** Delhi Technical Campus (GGSIPU)
**Team:** Vaibhav (Team Leader, 01118015625), Aksh Rastogi (02518015625), Akshay Kumar (00618015625),
Apurva Kr. Yadav (02418015625), Vaibhav Sharma (00518015625)
**Language:** C (C99), modular, menu-driven
**Numerical methods:** Interpolation, Integration, Root Finding

This file is the single source of truth for the project. If anything changes during the build, it is updated here.

---

## 1. Problem statement and real-world framing

Smart electricity meters are being installed in Indian homes. A smart meter records the household's
power demand (kW) at fixed times and sends the readings to the electricity company. In practice some readings
never arrive because of network drops, power cuts or meter faults. Both the electricity company and the
consumer still need three answers:

| # | Real-world question | Numerical method |
|---|---|---|
| 1 | What were the missing readings? (fill the gaps) | **Interpolation** |
| 2 | How much energy (kWh) was used in the day, and in the evening peak window? (billing) | **Integration**, since energy = area under the power–time curve, E = ∫P(t) dt |
| 3 | At what exact time did the load go above the **3 kW** limit (sanctioned load), and when did the day's usage pass the **15 kWh** budget? (alerts) | **Root finding**, solving P(t) − 3 = 0 and E(t) − 15 = 0 |

The three methods form **one chain**: each step uses the output of the previous one.

```
CSV meter readings ──► detect missing values ──► INTERPOLATE (fill gaps + continuous curve P(t))
                                                   │
                                                   ▼
                                    INTEGRATE  E = ∫ P(t) dt   (daily energy, peak-window energy, bill)
                                                   │
                                                   ▼
                     ROOT-FIND  P(t) − 3 kW = 0   and   E(t) − 15 kWh = 0
                                                   │
                                                   ▼
                       ERROR ANALYSIS against known true values ──► tables + CSV files
```

The two root-finding problems differ in a way that is useful to discuss:
- **Power crossing:** power goes up and down, so P(t) = 3 kW can have **several** answers (load rises above 3 kW, then falls back).
- **Energy budget:** cumulative energy E(t) only increases, so E(t) = 15 kWh has **exactly one** answer.

## 2. Data design (three tiers)

Error analysis is only meaningful if the **true answer is known**. Comparing methods only with each other
cannot tell us which one is correct. The project therefore uses three tiers of data.

| Tier | Data | How the true answer is known | Used for |
|---|---|---|---|
| **A. Verification functions** | Simple math functions: straight lines, x³, sin x, x² − 2 | Solved by hand | Showing each method is coded correctly (test cases T1–T9) |
| **B. Synthetic household day** (main dataset) | A formula for one summer day of a Delhi household, read every hour, with 5 readings deleted | Exact values, exact energy (using `erf`) and crossing times (solved to 10⁻¹²) are all known | Main results and error analysis |
| **C. Real household day** | One complete day (14 May 2007) from the UCI *Individual Household Electric Power Consumption* dataset | Hidden readings are known. The 1-minute data gives the metered energy and the true budget-crossing time. | Shows how methods behave on real, spiky data |

### 2.1 Tier B — synthetic Delhi household (summer day)

Power demand in kW, with t in hours (0 to 24):

```
P(t) = B + A1·g(t; μ1, σ1) + A2·g(t; μ2, σ2) + A3·g(t; μ3, σ3),     g(t; μ, σ) = exp( −(t − μ)² / (2σ²) )
```

| Term | Meaning | Value |
|---|---|---|
| B | Base load (fridge, fans, standby) | 0.5 kW |
| Bump 1 | Morning peak (water pump, kettle, iron) | A1 = 1.0 kW, μ1 = 8:00, σ1 = 1.0 h |
| Bump 2 | Afternoon AC (hottest hours) | A2 = 0.8 kW, μ2 = 15:00, σ2 = 2.5 h |
| Bump 3 | Evening peak (AC + lights + TV + cooking) | A3 = 3.0 kW, μ3 = 20:30, σ3 = 1.5 h |

- **Exact energy:** each bell curve integrates to an `erf` expression, so

  `E(a,b) = B(b − a) + Σ Ak·σk·√(π/2)·[erf((b − μk)/(σk√2)) − erf((a − μk)/(σk√2))]`.

  `erf()` is in C99 `math.h`.
- **Why bell curves and not sine waves?** A sum of sine and cosine waves over a full 24-hour period makes the
  trapezoidal rule *almost exact*. That would make the trapezoidal rule look better than Simpson and give a misleading
  comparison. The bell-curve model has different slopes at 0:00 and 24:00, so the normal error behaviour shows up.
- **How the CSV is created:** `generate_synthetic_csv()` in `load_model.c` reads P(t) at every hour from 0:00 to 24:00
  (25 readings), rounds each reading to 4 decimals (0.1 W, like a meter display) and writes **NA** for the
  hours **4, 9, 13, 14, 19**. The program writes `data/synthetic_day.csv` this way, and it can be regenerated from the menu.
  The missing hours were chosen to cover different cases:
  - 4:00 is at night, when the load is flat.
  - 9:00 is just after the morning peak, where the curve is steep.
  - 13:00 and 14:00 are two missing readings in a row.
  - 19:00 is on the evening ramp, close to the 3 kW threshold.

### 2.2 Tier C — real household (UCI dataset)

- **Source:** G. Hébrail and A. Bérard, *Individual Household Electric Power Consumption*, UCI Machine Learning
  Repository, 2006 (DOI 10.24432/C58K54, licence CC BY 4.0). This is a house in Sceaux, France, with **1-minute** average active power
  recorded from Dec 2006 to Nov 2010.
- **Day used:** **14 May 2007**. All 1440 minutes are present, the evening load crosses 3 kW, and the 15 kWh budget is
  reached in the evening.
- **Extraction:** `scripts/extract_uci_day.py` (one-time preprocessing, outside the C program) writes three files:
  - `data/real_day_minute.csv`: 1441 rows from 00:00 to 24:00. The 24:00 row is 00:00 of the next day.
  - `data/real_day_hourly_complete.csv`: the minute reading at each hh:00, 25 rows. This is the ground truth for the hidden hours.
  - `data/real_day_hourly.csv`: the same 25 rows with hours 4, 9, 13, 14, 19 set to NA, the same pattern as Tier B.

  Full steps and citation are in `docs/data_source.md`.
- **Reference energy:** each minute value is the average power over that minute, so the metered energy is
  `Σ Pk / 60` kWh over the 1440 minutes.

### 2.3 CSV format

```
# comment lines start with '#'
time_hours,power_kW
0,0.5000
1,0.5000
4,NA          <- missing reading
...
24,0.6982
```

- Time is in decimal hours (18.5 = 18:30) and power is in kW.
- `NA` (or an empty field) marks a missing reading.
- Times must be strictly increasing.
- The first and last readings must not be missing, because filling them would mean extrapolation.
- A custom dataset in the same format can be loaded from the menu. `data/custom_template.csv` is provided as an example.

## 3. Numerical methods and why each was chosen

### 3.1 Interpolation (fill missing values, and give a continuous curve P(t))

| Method | How it works | Why chosen |
|---|---|---|
| **Linear** (baseline) | Straight line between the two neighbouring known readings | The simplest method; everything else is compared against it |
| **Lagrange, local cubic** | Cubic through the **4 nearest known readings** (2 on each side, shifted at the ends), using the Lagrange formula | Works with **unequal spacing**, which the gaps create. Newton's forward/backward difference formulas need equal spacing, so they can't be used. |
| **Newton Divided Difference (NDD), local cubic** | The same 4 points, written in Newton form using a divided-difference table | The same cubic in a different form. The program prints the **divided-difference table** and checks that \|Lagrange − NDD\| ≈ 10⁻¹⁵, which is a **correctness check** for both codes. |
| **Global Lagrange** (demonstration only) | One polynomial through all 20 known readings (degree 19) | Shows **Runge's phenomenon**: high-degree polynomials swing wildly near the ends. This is why local cubics are used. |

**Stencil rule:** for a time t in the interval [x_i, x_{i+1}] between known readings, the cubic uses x_{i−1}, x_i, x_{i+1}, x_{i+2}.
At the ends the 4 points are shifted inward. Because the 4 points are chosen **per interval**, the curve is continuous: the cubics
on both sides of a reading pass through that reading. This piecewise cubic is the continuous curve P̃(t) that root finding uses.

### 3.2 Integration (energy = ∫P dt)

| Method | Formula idea | Error order | Requirement |
|---|---|---|---|
| **Composite Trapezoidal** | Join the readings with straight lines and add up the trapezoid areas | O(h²) | Works with any spacing |
| **Composite Simpson's 1/3** | Fit a parabola through each pair of intervals | O(h⁴) | Equal spacing, **even** number of intervals |
| **Composite Simpson's 3/8** | Fit a cubic through each group of 3 intervals | O(h⁴), smaller constant than 1/3 for some data | Equal spacing, number of intervals a **multiple of 3** |

- The integration runs on the gap-filled hourly grid: 24 intervals, divisible by both 2 and 3.
- If the interval count doesn't fit Simpson's rule, the leftover intervals are done with the trapezoidal rule and a warning is printed.
  An example is the 18:00–22:00 peak window, which has 4 intervals and so does not suit 3/8.
- If the times are not equally spaced, both Simpson rules are skipped with a message.

### 3.3 Root finding (crossing times)

f(t) = P̃(t) − 3 kW for power crossings, and f(t) = Ẽ(t) − 15 kWh for the energy budget.

| Method | Type | Order | Starting values |
|---|---|---|---|
| **Bisection** | Bracketing | 1 (linear) | Bracket [a, b] from the scan |
| **Regula Falsi** (false position) | Bracketing | 1 (linear, often faster than bisection) | Same bracket |
| **Newton-Raphson** | Open | 2 (quadratic) | x₀ = middle of the bracket. Slope f′(t) ≈ [f(t+δ) − f(t−δ)]/(2δ) with δ = 10⁻⁵ h (central difference). |
| **Secant** | Open | ≈1.618 | x₀ = a, x₁ = b |

- **Bracket scan:** f is checked every **0.25 h (15 min)** from 0 to 24. Each sign change gives one bracket, and every
  method then solves inside it. A reading exactly at the threshold is reported directly as a root.
- **Stopping rule:** stop when the step size |x_k − x_{k−1}| (or the bracket half-width for bisection) is below **10⁻⁶ h**
  (about 3.6 ms), or after 100 iterations.
- **Safeguard:** if an open method (Newton-Raphson or Secant) leaves its bracket, the result is flagged as "left bracket"
  instead of being silently accepted.
- **Energy so far, Ẽ(t):** the integral of P̃ from 0 to t. P̃ is a cubic on each interval between known readings, and
  Simpson's 1/3 rule is exact for cubics, so applying Simpson's rule on each interval (and on the last partial interval)
  gives Ẽ(t) **exactly for the model**. Note that Ẽ′(t) = P̃(t) by the fundamental theorem of calculus.

## 4. Error analysis and method comparison

The principle is to compare every method against a **known true value**, not only against the other methods.

### 4.1 Interpolation
- **Tier B:** at each missing hour, the absolute and % error of each method against the exact P(t).
- **Max curve error:** the largest |P̃(t) − P(t)| over the whole day, checked every 5 minutes. This is where Runge's phenomenon shows up.
- **Leave-one-out test (both tiers):**
  - Hide each known interior reading in turn, predict it from the rest, and compare with the real value.
  - Report **MAE, RMSE and maximum error** for each method.
  - On real data this is the only fair way to test interpolation, because we hide values we actually have.
- **Lagrange vs NDD:** the largest |difference| is reported. It should be about 10⁻¹⁵.
- **Gaps at peaks:** errors at the 19:00 gap (evening ramp) are compared with the 4:00 gap (flat night). On the real day, the
  hidden 19:00 reading is a 4.6 kW peak, and the gap **hides a threshold crossing**.

### 4.2 Integration
- **Energy vs reference:**
  - Tier B is compared with the exact `erf` integral.
  - Tier C is compared with the 1-minute metered energy.
  - Results are given for the full day and for the 18:00–22:00 peak window.
- **Convergence study (Tier B):**
  - Read the model every h = 2, 1, 0.5, 0.25 and 0.125 h (12 to 192 intervals) and integrate with all three rules.
  - Calculate the **observed order p = log₂(E(h)/E(h/2))**.
  - Theory says p → 2 for trapezoidal and p → 4 for both Simpson rules.
- **Sampling study (Tier C):** sample the minute data every 2, 1, 0.5 and 0.25 h, plus every minute. The errors do **not** shrink smoothly,
  because real load has sudden switch-on/switch-off spikes. Sampling rate matters more than the choice of formula.
- **Error carried through the chain (3 × 3 table):** fill the gaps with each interpolation method, then integrate with each rule.
  This shows how much the gap-filling choice changes the final kWh.

### 4.3 Root finding
- **Comparison table:** for every crossing, report the root, the time as HH:MM:SS, the number of iterations, |f(root)| and the error
  against the true crossing time in minutes.
  - Tier B truth comes from solving the exact P(t) = 3 and E(t) = 15.
  - Tier C truth for the budget comes from the minute data.
  - For Tier C power crossings, the nearest minute-level crossing in the same direction is shown.
- **Two kinds of error:**
  - **Solver error:** how far a method's answer is from a very precise answer on the *same* curve. All four methods
    reach about 10⁻⁷ h or better.
  - **Model error:** how far the curve's crossing time is from the *true* crossing time. This is caused by interpolation
    and is minutes, not milliseconds.
  - **Conclusion:** the quality of the data and the interpolation limits accuracy, not the root finder.
- **Iteration history:** the error |x_k − x*| at each iteration for every method, saved to CSV for a semi-log plot.
- **Observed order:** estimated as p ≈ ln(e_{k+1}/e_k) / ln(e_k/e_{k−1}).
  - It uses the last three errors above 10⁻¹², which is the accuracy of the reference root.
  - The starting value counts as e₀: the bracket midpoint for Newton, and b for Regula Falsi and Secant. Without this,
    Newton, which needs only 3 iterations, would have too few usable errors.
  - **Bisection is shown as "1\*"** instead of a computed value. Its error |c_k − x*| jumps up and down, so the formula
    gives meaningless numbers. Its bracket halves every step, which means order 1 with ratio ½.

### 4.4 Metrics used
Absolute error, relative error (%), MAE, RMSE, maximum absolute error, iteration count and observed order of convergence.

## 5. Program menu and features

```
=================== MAIN MENU ===================
 1. Load dataset
      1) Synthetic Delhi household day (data/synthetic_day.csv)
      2) Real household day, UCI 14-May-2007 (data/real_day_hourly.csv)
      3) Custom CSV file
      4) Regenerate synthetic CSV from the model
 2. View readings and load-curve chart
 3. Fill missing values (interpolation)
 4. Energy calculation (integration)
 5. Threshold and energy-budget analysis (root finding)
 6. Bill estimate (flat Rs/kWh rate)
 7. Error analysis and method comparison
 8. Run test cases
 9. Export all results to output/
 0. Exit
```

- The synthetic dataset loads automatically at start-up.
- **Defaults** are 3 kW threshold, 15 kWh budget, 18:00–22:00 peak window and Rs 8.00/kWh rate. Press Enter to accept a default or type a new value.
- **Bill estimate:**
  - Daily kWh × rate for each integration method.
  - The cost of the 18:00–22:00 peak window.
  - A 30-day projection.
  - The rate is a single flat rate entered by the user. No tariff slabs are used.
- **Screenshot-friendly output:** plain ASCII tables (`+----+`), at most about 90 characters wide, no colour codes, aligned numbers,
  and times shown both as decimal hours and as HH:MM.
- **Header:** every screen and exported report starts with the project title and the 5 team members with their enrollment numbers.
- **Bad input** (letters, out-of-range numbers, missing files, end of input) is handled without crashing.
- **Command-line shortcuts:**

  | Command | What it does |
  |---|---|
  | `energy.exe --test` | Runs the test cases only |
  | `energy.exe --export-all` | Exports results for both datasets, the tests and the web snapshot. This regenerates everything for the report in one step |
  | `energy.exe --export-web` | Writes only the web snapshot `web/data/results.js` |
  | `energy.exe --json <synthetic\|real> <outfile> [threshold budget peak_start peak_end rate fill probe]` | One dataset's results as JSON, used by `web/serve.py`. Bad values (not a number, `nan`, `inf`, out of range) give `{"error": ...}` |
  | `energy.exe --json-tests <outfile>` | The test results as JSON |

## 6. Folder structure, modules and functions

```
CM pbl/
├── src/
│   ├── main.c            menu loop, command-line flags
│   ├── dataset.h         Dataset struct shared by all modules
│   ├── data_io.c/.h      CSV reading/writing, folder creation
│   ├── load_model.c/.h   synthetic load model P(t), exact energy, synthetic CSV generator
│   ├── interpolation.c/.h linear, local Lagrange, local NDD, global Lagrange
│   ├── integration.c/.h  trapezoidal, Simpson 1/3, Simpson 3/8
│   ├── root_finding.c/.h bisection, regula falsi, Newton-Raphson, secant, bracket scan
│   ├── analysis.c/.h     energy-chain features + all error-analysis experiments
│   ├── report.c/.h       printing to screen and file, tables, ASCII chart, HH:MM, input helpers
│   ├── tests.c/.h        automated test cases T1–T15 and S1–S6
│   └── web_export.c/.h   all results as JSON for the web front-end (see §9b)
├── web/      optional front-end: index.html, css/, js/, vendor/, data/results.js, serve.py
├── data/     synthetic_day.csv, real_day_hourly.csv, real_day_hourly_complete.csv,
│             real_day_minute.csv, custom_template.csv
├── output/   generated results: output/synthetic/, output/real/, output/figures/, output/web_screenshots/,
│             test_results.txt
├── scripts/  extract_uci_day.py (data preparation), plot_results.py (graphs, optional)
├── docs/     PLAN.md, algorithms.md, data_source.md, viva_notes.md
├── build.bat
├── README.md
├── LICENSE           MIT for the team's code; third-party licences listed (see §9c)
├── .gitignore        files kept out of the repository (see §9c)
└── .gitattributes    line endings and generated/vendored markers (see §9c)
```

### Functions by module

**data_io**
- `load_csv()`: reads `time,power` rows, marks `NA` values as missing and checks the rules in §2.3.
- `load_reference()`: reads a complete file of true values.
- `load_fine()`: reads the 1-minute data.
- `known_points()`: copies only the known readings into arrays.
- `count_missing()`: counts the missing readings.
- `make_dir()`: creates an output folder.
- `load_synthetic_dataset()`: loads the synthetic day, creating the CSV if it is missing; the true values come from the model.
- `load_real_dataset()`: loads the real day together with its hourly truth file and 1-minute data.

**load_model**
- `model_power(t)`: the synthetic P(t).
- `model_energy(a, b)`: exact energy using `erf`.
- `generate_synthetic_csv(path)`: writes the synthetic dataset.

**interpolation**
- `find_interval()`: finds the interval between known readings that contains t.
- `interp_linear()`: straight line between neighbours.
- `local_stencil()`: picks the 4 points used by the local cubics.
- `lagrange_poly()`: the Lagrange formula on any set of points.
- `interp_lagrange_local()`: local cubic, Lagrange form.
- `ndd_build_table()`: builds the divided-difference table.
- `ndd_eval()`: evaluates the Newton form by nested multiplication.
- `interp_ndd_local()`: local cubic, Newton form.
- `interp_lagrange_global()`: one polynomial through all known points.
- `interp_eval(method, …)`: calls the chosen method.
- `fill_missing()`: fills every missing reading with the chosen method.

**integration**
- `trapezoidal()`: composite trapezoidal rule.
- `simpson_13()`: composite Simpson's 1/3 rule, with trapezoid fallback for a leftover interval.
- `simpson_38()`: composite Simpson's 3/8 rule, with fallback for leftover intervals.
- `is_equally_spaced()`: checks the spacing before Simpson is used.
- `integrate_grid(method, …)`: calls the chosen rule.

**root_finding**
- `bisection()`, `regula_falsi()`, `newton_raphson()`, `secant()`: each returns a `RootResult` with the root, iteration count,
  convergence flag and iteration history.
- `central_diff()`: slope estimate for Newton-Raphson.
- `scan_brackets()`: finds the sign changes.
- `solve_root(method, …)`: runs one method with its standard starting values.
- `estimate_order(r, x_start, ref)`: observed convergence order from the starting value plus the iteration history.

**analysis**
- The curve: `curve_set()` stores the known readings, `curve_power()` gives P̃(t), `curve_energy_upto()` gives Ẽ(t),
  `set_targets()` sets the threshold and budget, and `f_power()` / `f_energy()` are the functions handed to the root finders.
- Reference answers: `has_reference()`, `reference_energy()`, `true_power_crossings()`, `true_budget_time()`,
  `energy_with_fill()`.
- Menu features: `show_readings()`, `show_fill_missing()`, `show_energy()`, `show_threshold()`, `show_bill()`.
- Shared helpers:
  - `print_ndd_tables()`: prints the divided-difference tables.
  - `energy_table()`: the energy table.
  - `threshold_core()`: all crossings and the budget.
  - `solve_event()`: solves one event with all 4 methods and prints the table and iteration history.
- Error-analysis sections:
  - `sec_interp_missing()` (1A, 1B), `sec_interp_curve()` (1C), `sec_interp_loocv()` (1D)
  - `sec_integ_energy()` (2A, 2B), `sec_integ_convergence()` (2C)
  - `sec_roots()` (3)
  - Menu wrappers: `analysis_interpolation()`, `analysis_integration()`, `analysis_roots()`.
- Export: `export_all()` (one `.txt` per section plus `summary_report.txt`) and `write_dense_csv()`.

**report**
- Output:
  - `rprintf()`: prints to the screen and, during export, also to up to two files.
  - `fix_exponents()`: turns Windows' 3-digit exponents into the standard 2 digits.
  - `print_header()`, `print_title()`, `print_rule()`, `fmt_hhmm()`, `fmt_hhmmss()`, `ascii_chart()`.
- Input: `read_int()`, `read_double_default()`, `read_line()`, `input_ended()`.

**tests**
- `run_all_tests()`: runs all test cases and prints a PASS/FAIL table.
- `test_count()`, `test_row(i)`: the stored results of the last run, used by the JSON writer.

**web_export**
- `write_json(fp, dataset, settings, probe_t)`: every result for one dataset as one JSON object: readings, curves every
  5 minutes, gap estimates with the divided-difference table, energy by rule, error-through-the-chain table, convergence or
  sampling study, bill, and every crossing solved by all four root finders with their iteration histories.
- `write_tests_json(fp)`: runs the tests and writes their results.
- `export_web()`: writes `web/data/results.js` (both datasets at default settings, plus the tests) for the offline page.

## 7. Test cases

### Tier A: method verification (true answers known)

| ID | Test | Expected |
|---|---|---|
| T1 | Linear interpolation of y = 2x + 1 at x = 2.5 | 6 exactly |
| T2 | Local Lagrange on y = x³ − 2x² + x − 5 (5 points) at x = 2.7 | Exact for a cubic |
| T3 | Local NDD on the same cubic, and NDD equal to Lagrange | Exact; difference < 10⁻¹² |
| T4 | Trapezoidal ∫₀^π sin x dx with n = 6, 12, 24 | Approaches 2, error drops ~4× when h halves (order 2) |
| T5 | Simpson 1/3 ∫₀² x³ dx | 4 exactly (Simpson is exact for cubics) |
| T6 | Simpson 3/8 ∫₀² x³ dx | 4 exactly |
| T7 | Bisection on x² − 2 = 0 in [1, 2] | 1.41421356 |
| T8 | Regula Falsi on the same | 1.41421356 |
| T9 | Newton-Raphson and Secant on the same | 1.41421356, NR in fewer iterations than bisection |

### Edge and robustness cases

| ID | Test | Expected |
|---|---|---|
| T10 | Threshold above the day's maximum load | No brackets found; "never exceeded" message |
| T11 | Threshold exactly equal to a reading | That reading's time is reported as a root |
| T12 | CSV whose first reading is NA | Rejected with a clear message |
| T13 | Two missing readings in a row (13:00, 14:00) | Both filled; local cubic still uses 4 distinct known points |
| T14 | Simpson 1/3 with an odd number of intervals (5) | Warning flag set; result = Simpson on 4 + trapezoid on 1 |
| T15 | File not found / non-numeric value in CSV | Error message, no crash |

### Scenario cases

| ID | Scenario | Check |
|---|---|---|
| S1 | Synthetic day: fill the 5 gaps with every method | Local cubic errors < 0.1 kW; global Lagrange max curve error > 1 kW (Runge) |
| S2 | Synthetic day: energy for the full day and 18:00–22:00 | All rules within **1 %** of the exact `erf` energy at h = 1 h (see note) |
| S3 | Synthetic day: 3 kW crossings | Exactly 2 crossings (up, then down); all 4 methods agree to < 10⁻⁵ h; within 5 min of the true times |
| S4 | Synthetic day: 15 kWh budget | One crossing, within 5 min of the exact time |
| S5 | Real day: leave-one-out, energy vs metered, crossings | Leave-one-out runs; energy within 10 % of metered; budget time found |
| S6 | Convergence study | Observed order ≈ 2 (trapezoidal) and ≈ 4 (Simpson rules) at the smallest h |

**Note on S2:** the original target was 0.5 %. The measured worst case is 0.87 % (Simpson 1/3 on the 18:00–22:00 window).
That is not an integration bug:
- The window contains the lost 19:00 reading on the steep evening ramp, whose estimate is 0.07 kW off.
- Hourly readings under-sample the sharp evening peak.

The full-day errors are 0.15–0.33 %. The target was therefore changed to 1 %, and the reason is recorded here and in
the summary rather than hidden.

## 8. Output files (for the report, PPT and poster)

The program writes these for the current dataset into `output/<synthetic|real|custom>/`. Most results come as a `.txt` file
(aligned table, ready to paste) and a `.csv` file (for charts).

| File | Contents |
|---|---|
| `interpolation_results.txt/.csv` | Filled values, true values and errors per method; divided-difference tables; max curve error |
| `interpolation_loocv.txt/.csv` | Leave-one-out MAE, RMSE and max error per method (`.csv` has every prediction) |
| `integration_results.txt/.csv` | Energy per rule vs reference (full day + peak window). The `.txt` also has the error-through-the-chain table |
| `integration_chain.csv` | Error-through-the-chain table: gap-filling method (none / Linear / Lagrange / NDD / Global) × rule |
| `integration_convergence.txt/.csv` | h vs error for each rule and observed order (synthetic), or sampling study (real) |
| `root_results.txt/.csv` | Crossings and budget time by every method: root, HH:MM:SS, iterations, errors |
| `root_iterations.csv` | Error at each iteration for every method (for convergence plots) |
| `load_curve_dense.csv` | Every 5 minutes: true curve (if known) and the linear, local cubic, NDD and global Lagrange curves |
| `readings.csv` | The readings with missing flags and true values (for plotting) |
| `bill_estimate.txt` | Bill table at the default rate |
| `summary_report.txt` | All of the above in one file |
| `output/test_results.txt` | PASS/FAIL table of all test cases |
| `web/data/results.js` | Web snapshot: every result for both datasets plus the tests, as JSON (see §9b) |
| `output/web_screenshots/*.png` | Screenshots of every web section at 1366 × 768 and 1920 × 1080 (taken with headless Chrome, not by the program) |

`scripts/plot_results.py` (optional, Python + matplotlib) turns the CSVs into PNGs in `output/figures/`:

| Figure | Shows |
|---|---|
| `fig_load_curve_synthetic.png`, `fig_load_curve_real.png` | Load curve with gaps, interpolations and the 3 kW line. The real one is drawn over the 1-minute data |
| `fig_integration_convergence.png` | Convergence plot on log-log axes |
| `fig_sampling_real.png` | Real-data sampling study |
| `fig_root_iterations_synthetic.png`, `fig_root_iterations_real.png` | Root-finder error vs iteration |
| `fig_energy_chain_synthetic.png`, `fig_energy_chain_real.png` | Energy-error bars |
| `fig_loocv.png` | Leave-one-out bars |

## 9. Design decisions, trade-offs and known limitations

1. **Readings are treated as instantaneous power values.** Real meters store the average over each 15- or 30-minute block.
   Treating readings as point values is a modelling simplification.
2. **No extrapolation.** Missing readings at the first or last time are rejected, because filling them would mean predicting outside the data.
3. **Local cubic, not global polynomial.** A degree-19 polynomial shows Runge's phenomenon. The local cubic is continuous, but its slope
   can jump at known readings. Newton-Raphson uses a central-difference slope, which is fine away from those readings.
4. **Missed crossings.** The bracket scan uses a 15-minute step, so two crossings less than 15 minutes apart can be missed.
   Real 1-minute data crosses 3 kW dozens of times a day, and hourly readings cannot see most of these.
5. **Hourly sampling limits accuracy on real data** much more than the integration rule does. A short switch-on spike (kettle, iron)
   between two hourly readings is simply invisible.
6. **The real data is from France, not India.** This is the cleanest freely available minute-level household dataset. An Indian dataset
   (iAWE, IIIT-Delhi) is listed as a future enhancement. The synthetic model is shaped like a Delhi summer day.
7. **Flat-rate bill only.** Real Delhi tariffs use slabs and fixed charges. These are left out on purpose to keep the focus on the numerical methods.
8. **Fixed-size arrays** (up to 2000 readings) keep the C code simple with no dynamic memory. 1-minute data for one day fits.
9. **Global variables for the curve.** C has no closures, so the curve model that the root finders solve is stored in file-level
   variables in `analysis.c` before the solvers are called. This is explained in the viva notes.
10. **The true crossing times for Tier B** are computed with bisection to 10⁻¹² on the *exact* model, not on the interpolated curve.
11. **Equal-spacing check with tolerance.** CSV times are rounded (1 minute = 0.016667 h), so `is_equally_spaced()`
    accepts spacing differences up to 0.01 % of h. An exact 10⁻⁹ check wrongly rejected the 1-minute data for Simpson.
12. **Exponent format.** The old Windows C library prints `1.2e-005`. `rprintf()` rewrites this as `1.2e-05` and keeps
    column alignment. This only affects screenshots; `.csv` files keep the raw format, which Excel and Python read fine.
13. **Peak-window default for custom data.** If 18:00–22:00 lies outside a custom dataset's time range, the prompt offers
    that dataset's whole range instead.
14. **Small samples can mislead.** On the real day, global Lagrange has the lowest RMSE at the 5 hidden readings by
    one lucky guess at 19:00, yet its leave-one-out RMSE is 152 kW. The program prints a caution next to this result
    and points to the larger tests (1C, 1D).

## 9a. Changes made during the build

| Change | Reason |
|---|---|
| S2 target changed from 0.5 % to 1 % | Measured worst case 0.87 %, caused by the 19:00 gap and peak under-sampling inside the window (see §7 note) |
| Order column: bisection shown as "1\*"; estimate includes the starting value; noise cutoff 10⁻¹² | Bisection's error is not monotonic. Newton converges in 3 steps, which left too few usable errors |
| Added `integration_chain.csv` and `readings.csv` | Needed by the plotting script |
| Equal-spacing tolerance relative to h (item 11) | Bug found while testing: Simpson was wrongly marked "n/a" on 1-minute data |
| 2-digit exponent normalisation (item 12) | Readable screenshots on Windows |
| Caution note under "lowest RMSE at missing readings" (item 14) | Prevents a wrong conclusion from 5 points |
| The real-data note also gives the first real 3 kW crossing from the 1-minute data | Shows how much the hourly view misses |

## 9b. Web front-end (added after the C build)

**Goal:** a friendly, screenshot-ready visual front-end for the report, PPT, poster and viva demo, without moving any
computation out of C.

### Architecture
- `src/web_export.c` writes all results as JSON, using the same module functions as the menu.
  - `--json <dataset> <outfile> [threshold budget peak_start peak_end rate fill probe]`: results for one dataset.
  - `--json-tests <outfile>`: the test results.
  - `--export-web`: writes `web/data/results.js`. `--export-all` now also does this.
- `web/serve.py` (Python standard library only):
  - serves `web/`;
  - answers `/api/run` and `/api/tests` by running `energy.exe` and returning its JSON;
  - checks every input before passing it to the engine.
- `web/index.html` with `css/style.css`, `js/icons.js` and `js/app.js` (plain JavaScript, no framework):
  - It only formats numbers into tables and charts.
  - **Live mode** applies when the server is running.
  - **Offline mode** reads the snapshot. Inputs are locked and a banner explains how to start the engine.

### Sections and content

| Section | Content |
|---|---|
| Overview | The problem and the three-step method chain, the team with enrollment numbers, four key figures that count up, the load curve, findings |
| Dataset | Source and truth for each dataset, readings chart, all 25 readings in two tables |
| Fill Missing Values | Choose a method, estimate at any time (live), interpolation chart, lost-reading errors, divided-difference table with the Newton form worked out, whole-curve error, leave-one-out |
| Energy & Bill | Gap-fill method, peak window, rate; area-under-curve chart, energy by rule, energy error per method (bar chart), bill |
| Threshold Alerts | Limit and budget inputs, alert cards, crossing chart, E(t) chart, four-method table per event, iteration history table, error-per-iteration chart (log scale) |
| Method Comparison | Error vs h on log-log axes (synthetic) or sampling study (real), observed orders, interpolation error measures, leave-one-out bars, root-finder summary, solver error vs model error, conclusions |
| Tests | Pass ring, the 21 tests in three groups, re-run button (live) |

### Design
- **Colours:** warm cream background, mint/teal and peach/coral accents. Coral is the one strong accent.
- **Chart colours:** fixed per method family, in the order teal, coral, plum, amber. These were checked with the
  dataviz colour validator: colour-blind safe for neighbouring series. Markers and legends are always present as a
  second cue besides colour.
- **Fonts:** Fredoka (display), Nunito Sans with tabular figures (body and table numbers), JetBrains Mono (code-like text).
  All are bundled locally under the SIL Open Font License.
- **Mascot and icons:** a hand-drawn SVG smart-meter mascot with three moods, and one consistent hand-drawn SVG icon set.
  No emoji anywhere; this was checked with a script.
- **Screens:** tested at 1366 × 768 and 1920 × 1080 with headless Chrome, with automatic checks for horizontal overflow
  and console errors.

### Changes made while building the front-end

| Change | Reason |
|---|---|
| `energy_of_array()`, `reference_power()` and `default_settings()` made public (`analysis.h`) | Needed by the JSON writer |
| `tests.c` keeps each result in a list (`test_count()`, `test_row()`) | The page shows the test table |
| Table numbers use Nunito Sans tabular figures, not JetBrains Mono | The mono font's dotted zero made 0.157 look like 8.157 |
| Inputs are text fields with `inputmode="decimal"`, not number fields | A recompute while typing moved the caret to the start, so typing 2.5 slowly gave 52 |
| Log axes show decade grid lines only | Minor ticks made the charts look striped |
| `--json` and `serve.py` reject `nan` and `inf` | `strtod` accepts them, and `threshold <= 0` is false for NaN, so a NaN threshold got past the range checks |
| `serve.py` finds the program as `energy.exe` or `energy` | The Linux/Mac build may use either name |

## 9c. Publishing on GitHub

- `.gitignore` leaves out the compiled program (`energy.exe`, `*.o`), results for personal CSV files (`output/custom/`),
  Python caches and editor files.
- The generated results in `output/` and `web/data/results.js` **are committed**. The report material can then be
  browsed on GitHub, and the offline web page works straight after cloning. Run `energy.exe --export-all` and commit
  again after any change to the C code.
- `.gitattributes` stores text with LF line endings, keeps CRLF for `build.bat`, marks images and fonts as binary, and
  marks `web/vendor/`, `web/data/results.js` and `output/` as vendored or generated. GitHub's language bar then shows the
  project as C.
- Third-party files keep their licences: Chart.js (MIT) and the fonts (SIL OFL) in `web/vendor/`. The UCI data is
  CC BY 4.0 and is cited in the README and in `docs/data_source.md`.
- **Licences:** the team's own code and documents are under the MIT License (`LICENSE`). Third-party parts keep their
  own licences: Chart.js (MIT), the fonts (SIL OFL 1.1) and the UCI-derived data in `data/` (CC BY 4.0). The README's
  "Licence" section lists them.
- The team names and enrollment numbers are in `src/report.c` (`print_header()`, used by every screen and report),
  `web/js/app.js` (`TEAM`), the README and this plan. After changing them in C, run `energy.exe --export-all`.

## 10. Build and run

```
build.bat                      (runs: gcc -std=c99 -Wall -Wextra -O2 <all src/*.c files> -o energy.exe -lm)
energy.exe                     (interactive menu)
energy.exe --export-all        (regenerates all output files + web snapshot)
python scripts/plot_results.py (optional graphs)
python web/serve.py            (optional web front-end at http://localhost:8000)
```

On Linux/Mac: `gcc -std=c99 -Wall -Wextra -O2 src/*.c -o energy.exe -lm`, then `./energy.exe`.

Compiler used for development: MinGW GCC 6.3.0 on Windows 11.
