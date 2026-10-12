# Poster Content: Smart Energy Consumption Prediction & Analysis System

> **How to use this file.** This is the content for a one-page academic poster, in the order of its sections. Keep it
> short: the text below is already at poster length. Every number comes from `output/`. Items in `[SQUARE BRACKETS]`
> still need to be filled in.

---

**Header band**

**DELHI TECHNICAL CAMPUS, GREATER NOIDA**
Computational Methods Lab (ES-251) · Session 2026-27 (Odd Sem) · [BRANCH / SECTION / SEMESTER]

# Smart Energy Consumption Prediction & Analysis System
*Filling lost smart-meter readings, computing energy and finding alert times with numerical methods in C*

---

## Introduction

Smart meters record household power every hour, but some readings are lost. From one day of 25 readings with
5 missing, our C program answers three questions:
- What were the missing readings? (**interpolation**)
- How much energy was used, and what is the bill? (**integration**)
- When did the load cross **3 kW**, and when was the **15 kWh** budget used up? (**root finding**)

---

## Objectives

- Implement 11 numerical methods in C and chain them: fill gaps → integrate → find roots.
- Verify every method against known true answers.
- Compare methods by error, iterations and convergence order.
- Find out where the error really comes from: the method, or the data.

---

## Feasibility Study and Tools Used

| Feasibility | |
|---|---|
| Technical | Standard C99 + GCC, only `math.h`, zero compiler warnings |
| Economic | Zero cost: free compiler, open data (CC BY 4.0), open-source libraries |
| Operational | Menu-driven, defaults on every prompt, runs fully offline |

**Tools:**
- **Core:** C99 · GCC (MinGW) · Windows 11
- **Showcase dashboard:** HTML/CSS/JS · Chart.js
- **Optional helpers:** Python (local server, graphs, data extraction)

---

## System Architecture

```
 CSV readings ──► INTERPOLATION ──► curve P(t) ──► INTEGRATION ──► energy, bill
 (NA = lost)      Linear, Lagrange,       │         Trapezoidal,
                  Newton DD               │         Simpson 1/3, 3/8
                                          ▼
                                    ROOT FINDING ──► alert times
                                    Bisection, Regula Falsi,
                                    Newton-Raphson, Secant
                                          │
                                          ▼
                     ERROR ANALYSIS vs exact model / 1-minute real data
                                          │
                                          ▼
            Terminal tables + .txt/.csv files   ──►  Web dashboard (displays C results)
```

All computation is in the **C program**. The web dashboard runs it (`energy.exe --json`) and only draws the results.

---

## Design and Implementation

**Implementation flow:** CSV → `data_io` (load and check) → `interpolation` (local cubic) → `integration`
(Simpson/Trapezoid) → `root_finding` (4 solvers) → `analysis` (errors) → `report` (tables and files).

- 10 C modules, 3,836 lines; menu with 9 options; 21 automated tests.
- **Local cubic:** the 4 nearest known readings give a continuous curve and avoid Runge's phenomenon.
- **Newton-Raphson:** the slope comes from a central difference. Iteration stops when the step is below 10⁻⁶ h.
- **Data:**
  - Synthetic Delhi summer day, with exact energy from erf: 30.6882 kWh.
  - Real UCI household day (14 May 2007), with 1-minute reference data: 24.4630 kWh.

---

## Result

| Headline | Value |
|---|---|
| Convergence orders, Trapezoidal / Simpson 1/3 / Simpson 3/8 | **2.00 / 4.01 / 4.03** (theory 2 / 4 / 4) |
| Iterations to 10⁻⁶ h, Newton-Raphson vs Bisection | **3** vs **18** |
| Root-finder error vs alert-time error | **3 ms** vs **2.59 min** (synthetic) and **76.42 min** (real) |
| Real-day energy error, meter read every 60 min vs every 30 min (Simpson 1/3) | **+11.902 %** vs **+0.260 %** |

Lagrange and NDD agree to 4.44 × 10⁻¹⁶ kW. All **21 / 21 tests pass**.

**Images to show:**
1. **Chart:** `output/figures/fig_load_curve_real.png` (hourly readings vs 1-minute data, which shows the missed peak).
   Alternative: `output/figures/fig_integration_convergence.png`.
2. **Dashboard:** `output/web_screenshots/screen_1920_synthetic_overview.png`.

---

## Conclusion

- All 11 methods are implemented in C and verified; the observed convergence orders match theory.
- On smooth data, higher-order methods win. On spiky real data, simple methods are as good or better.
- **Accuracy is limited by how often the meter reads, not by the numerical method.**
- Future work: splines, Romberg integration, Brent's method, Indian (iAWE) data.

---

## Submission by

| Name | Enrollment No. |
|---|---|
| Vaibhav (Team Leader) | 01118015625 |
| Aksh Rastogi | 02518015625 |
| Akshay Kumar | 00618015625 |
| Apurva Kr. Yadav | 02418015625 |
| Vaibhav Sharma | 00518015625 |

## Under the guidance of

[GUIDE NAME], [DESIGNATION], [DEPARTMENT]

*Data: Hebrail & Berard (2006), UCI Machine Learning Repository, doi:10.24432/C58K54 (CC BY 4.0).*

---

## Checklist

### A. Terminal screenshots to take yourself (optional for the poster)

A poster usually has room for at most one terminal image. If you add one:

| # | Command / menu path | What to capture | Where |
|---|---|---|---|
| Q-1 | `energy.exe --test` | The last few rows and "TOTAL: 21 passed, 0 failed (out of 21)" | Result section, small |
| Q-2 *(alternative)* | Menu **5** (press Enter for 3 kW and 15 kWh) | The two ">> ALERT" lines and the SUMMARY line (solver error vs model error) | Result section, small |

### B. Files from `output/` (to check the numbers)

| File | Numbers it confirms |
|---|---|
| `output/synthetic/summary_report.txt` | Orders 2.00/4.01/4.03; 3 vs 18 iterations; 2.59 min; 4.44 × 10⁻¹⁶; 30.6882 kWh |
| `output/real/summary_report.txt` | 76.42 min; +11.902 % vs +0.260 %; 24.4630 kWh |
| `output/test_results.txt` | 21 / 21 tests |

### C. Images

| File | Poster section |
|---|---|
| `output/figures/fig_load_curve_real.png` (or `fig_integration_convergence.png`) | Result: the one chart |
| `output/web_screenshots/screen_1920_synthetic_overview.png` | Result: the one dashboard screenshot |
| Overall-system flowchart (`REPORT_CONTENT.md` §5.1 → https://mermaid.live → PNG) *(optional)* | System Architecture, instead of the text diagram |
