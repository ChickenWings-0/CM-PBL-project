# PPT Content: Smart Energy Consumption Prediction & Analysis System

> **How to use this file.** There is one block per slide: the title, short bullets (at most 6), the visual to place
> (with its file path), and an optional speaker note. Every number comes from the program's output files in `output/`.
> Items in `[SQUARE BRACKETS]` still need to be filled in. There are 19 slides. To reach 18, merge slides 9 and 10
> (Hardware + Software Requirements).
>
> **Framing used throughout:** the core project is the **C program**. All numerical methods, the error analysis, the
> tests and the menu are in C. The web dashboard is a showcase layer that runs the same C program and only displays
> its results. Python is used only for optional helpers (the local server, graphs, one-time data extraction).

Header or footer on every slide: *Delhi Technical Campus, Greater Noida · Computational Methods Lab (ES-251) ·
Session 2026-27 (Odd Sem)*

---

## Slide 1: Title

**Smart Energy Consumption Prediction & Analysis System**
*Interpolation · Numerical Integration · Root Finding, in C*

- Computational Methods Lab (ES-251), Project-Based Learning
- Delhi Technical Campus, Greater Noida (GGSIPU), Session 2026-27 (Odd Sem)
- [BRANCH / SECTION / SEMESTER]
- **Team:** Vaibhav (Team Leader, 01118015625) · Aksh Rastogi (02518015625) · Akshay Kumar (00618015625) ·
  Apurva Kr. Yadav (02418015625) · Vaibhav Sharma (00518015625)
- **Guide:** [GUIDE NAME]

**Visual:** the mascot or overview image, cropped from `output/web_screenshots/screen_1920_synthetic_overview.png`
(the hero card with the meter mascot), or the college logo.

---

## Slide 2: Introduction / Problem Definition

- Smart meters record household power (kW) every hour: 25 readings a day.
- Some readings are **lost** (network drops, power cuts, meter faults): 5 of 25 here.
- **Q1:** What were the missing readings? → **Interpolation**
- **Q2:** How much energy was used, and what is the bill? → **Integration**, E = ∫P dt
- **Q3:** When did the load cross **3 kW**, and when was the **15 kWh** budget used up? → **Root finding**

**Visual:** `output/figures/fig_load_curve_synthetic.png` (load curve with lost readings and the 3 kW line).

*Speaker note:* one chain: the curve from step 1 is integrated in step 2 and solved in step 3.

---

## Slide 3: Feasibility Study

- **Technical:** standard C99 with GCC; no external libraries (only `math.h`); builds with zero warnings under
  `-Wall -Wextra -pedantic`.
- **Technical:** small inputs (25 readings, or 1441 for the 1-minute data) fit in fixed arrays; results are instant.
- **Economic:** zero cost. Free compiler (GCC/MinGW), open data (UCI, CC BY 4.0), open-source chart library (MIT).
- **Operational:** a menu-driven terminal program; defaults on every prompt (press Enter); bad input never crashes it.
- **Operational:** one command (`energy.exe --export-all`) regenerates every result file for the report.

**Visual:** none, or a small three-column icon layout (Technical / Economic / Operational).

---

## Slide 4: Need and Significance

- **Billing** needs the full day's energy even when readings are missing.
- **Load alerts:** going above the sanctioned load (3 kW) can trip supply or add penalties.
- **Budgeting:** knowing *when* a daily energy budget runs out helps consumers shift their usage.
- **Method choice matters less than expected.** This project measures *how much* accuracy each method gives, and
  where the error really comes from.
- Applies the ES-251 syllabus methods to a real Indian problem: smart-meter roll-outs.

**Visual:** `output/web_screenshots/screen_1920_synthetic_alerts.png`, cropped to the alert cards.

---

## Slide 5: Objectives

- Implement **11 methods in C**: 4 interpolation, 3 integration and 4 root finding.
- **Chain** them: fill gaps → integrate → find alert times on the same curve.
- **Verify** each method against known answers (test functions, an exact synthetic model).
- **Test on real data:** a UCI household day with 1-minute reference data.
- **Compare** the methods by error, iterations and observed order, separating *method* error from *data* error.
- Produce **21 automated tests**, ready-to-use tables, graphs and a visual dashboard.

**Visual:** none (text slide), or the three-step pipeline from the overview screenshot.

---

## Slide 6: Literature Survey

- **Interpolation:** Lagrange and Newton divided-difference forms, and their error terms (Sastry; Burden & Faires).
- **Runge (1901):** high-degree polynomials on equally spaced points oscillate near the ends, so use a local
  low-degree polynomial.
- **Integration:** composite Trapezoidal O(h²), Simpson 1/3 and 3/8 O(h⁴) (Chapra & Canale; Sastry).
- **Root finding:** Bisection and Regula Falsi (bracketed, order 1); Newton-Raphson (order 2); Secant (order 1.618)
  (Burden & Faires).
- **Data:** UCI *Individual Household Electric Power Consumption* (Hebrail & Berard, 2006): 1-minute household data,
  2006–2010, CC BY 4.0.
- **Indian data:** iAWE, a Delhi home from IIIT-Delhi (Batra et al., 2013), noted for future work.

**Visual:** none (text slide), or textbook cover thumbnails.

---

## Slide 7: Design Analysis: System Flowchart

- Input: a CSV of `time_hours, power_kW`, with `NA` for each lost reading.
- Interpolation builds a continuous curve P(t), using the local cubic through the 4 nearest readings.
- Integration gives the day and peak energy and the bill. Root finding gives the crossing and budget times.
- Every result is compared with the true values; the output is tables plus `.txt` and `.csv` files.

**Visual:** the "Overall system" Mermaid flowchart (`docs/algorithms.md` §1, also in `REPORT_CONTENT.md` §5.1),
exported as PNG from https://mermaid.live.

---

## Slide 8: Methodology: the Three Method Families

| Family | Methods | Order / note |
|---|---|---|
| **Interpolation** | Linear · Lagrange (local cubic) · Newton DD (local cubic) · Global Lagrange (Runge demo) | O(h²) · O(h⁴) · O(h⁴) · unstable |
| **Integration** | Trapezoidal · Simpson 1/3 · Simpson 3/8 | 2 · 4 · 4 |
| **Root finding** | Bisection · Regula Falsi · Newton-Raphson · Secant | 1 · 1 · 2 · 1.618 |

- Local cubic: 4 nearest known readings, so the curve is continuous.
- Newton's slope: central difference, f'(x) ≈ [f(x+δ) − f(x−δ)]/2δ with δ = 10⁻⁵ h.
- Stopping rule: step < 10⁻⁶ h. Brackets come from a scan every 15 min.

**Visual:** the table above. Optionally the NDD flowchart (`REPORT_CONTENT.md` §5.5) as a small inset.

---

## Slide 9: Hardware Requirements

- Any desktop or laptop PC that can run GCC (Windows, Linux or macOS).
- Development and testing machine: Windows 11 PC.
- Disk: the program is about 199 KB (`energy.exe`, 203,518 bytes); the whole project folder is about 12 MB, mostly
  screenshots.
- Display: a terminal at least 100 characters wide shows the tables without wrapping.
- No internet connection is needed. The data, fonts and chart library are all stored locally.

**Visual:** none (text slide).

---

## Slide 10: Software Requirements

- **Core (required):** C99 · GCC (MinGW GCC 6.3.0 on Windows 11 was used) · `build.bat` or a single gcc command.
- **Web layer (optional showcase):** HTML · CSS · plain JavaScript · Chart.js 4.4.1 (bundled) · any modern browser.
- **Optional Python 3 helpers:**
  - `web/serve.py`: local server (standard library only);
  - `scripts/plot_results.py`: graphs (matplotlib);
  - `scripts/extract_uci_day.py`: one-time data extraction.
- **Docs:** Markdown, with Mermaid for the flowcharts.

**Visual:** none, or small logos (C, GCC, HTML/CSS/JS, Chart.js, Python), each marked "core" or "optional".

---

## Slide 11: Scope

- **In scope:** one day of readings (hourly, or any equally spaced data); filling gaps; day and peak energy; a
  flat-rate bill; threshold and budget alerts; full error analysis; a custom CSV option.
- **Out of scope:** extrapolating a missing first or last reading; tariff slabs and fixed charges; multi-day
  forecasting; live meter feeds.
- **Future scope:** cubic splines, PCHIP, Romberg or adaptive Simpson, Brent's method, Indian data (iAWE), DERC tariff
  slabs.

**Visual:** a three-column layout (In / Out / Future).

---

## Slide 12: Implementation: the C Program

- **10 modules**, 3,836 lines of C: `interpolation`, `integration`, `root_finding`, `analysis`, `data_io`,
  `load_model`, `report`, `tests`, `web_export`, `main`.
- **Menu:** 1 Load · 2 View · 3 Fill gaps · 4 Energy · 5 Thresholds · 6 Bill · 7 Error analysis · 8 Tests · 9 Export.
- **Checks itself:** Lagrange and NDD must agree, and they differ by 4.44 × 10⁻¹⁶ kW.
- **Exports:** `.txt` tables and `.csv` data for every section; `--test` runs all 21 tests.
- **Compiles** with zero warnings (`-std=c99 -Wall -Wextra -pedantic`).

**Visual:** your own terminal screenshots (see checklist items P-1 and P-2): the header with the MAIN MENU, and the
Menu 3 → 5 comparison table with the CORRECTNESS CHECK line.

---

## Slide 13: Implementation: Web Dashboard (Showcase Layer)

- A **visual layer only:** every number is computed by `energy.exe`, and the page computes nothing itself.
- Flow: browser → `serve.py` (local) → `energy.exe --json` → JSON → tables and Chart.js charts.
- **Live:** change the threshold, budget, peak window, rate or gap-fill method, and the C program re-runs instantly.
- **Offline:** open `web/index.html` to see a saved snapshot of the same results.
- **Seven sections:** Overview, Dataset, Fill Missing Values, Energy & Bill, Threshold Alerts, Method Comparison, Tests.

**Visual:** `output/web_screenshots/screen_1920_synthetic_overview.png`, plus a smaller inset of
`output/web_screenshots/screen_1920_synthetic_alerts.png`.

---

## Slide 14: Results: Interpolation (Filling the 5 Lost Readings)

- **Synthetic:** the local cubic's largest error is **0.0702 kW** (at 19:00); RMSE 0.0399 kW against 0.0417 kW for
  linear.
- **Leave-one-out (synthetic):** local cubic RMSE **0.1569 kW** against **0.2681 kW** for linear.
- **Real day:** linear wins (leave-one-out RMSE **1.1520** against **1.4025 kW**). The real load is spiky, and the
  cubic overshoots.
- **Global degree-19 polynomial:** curve error **24.25 kW** (synthetic), from Runge's phenomenon.
- **Lagrange = NDD** to 4.44 × 10⁻¹⁶ kW: the same cubic in two forms.

**Visual:** `output/figures/fig_loocv.png`. Optional inset: `output/figures/fig_load_curve_real.png`.

---

## Slide 15: Results: Integration (Energy and Bill)

- **Synthetic exact energy 30.6882 kWh:** Trapezoidal **+0.145 %**, Simpson 1/3 **+0.330 %**, Simpson 3/8
  **+0.262 %**.
- **Observed orders:** **2.00 · 4.01 · 4.03** (theory 2 · 4 · 4).
- **Real metered energy 24.4630 kWh:** from hourly readings, Simpson 1/3 **−7.678 %**, Trapezoidal −9.322 %.
- **Sampling study (real):** reading every 60 min gives 2.9–11.9 % error; every 30 min, 0.26–1.53 %; every 1 min,
  ≤ 0.14 %.
- **Bill at Rs 8/kWh:** synthetic Rs 245.51/day (exact) against Rs 246.32 (Simpson 1/3); real Rs 195.70 (metered).

**Visual:** `output/figures/fig_integration_convergence.png`, with `output/figures/fig_sampling_real.png` beside it.

---

## Slide 16: Results: Root Finding (Alert Times)

- **Synthetic:** the load goes above 3 kW at **19:23:40** (true 19:26:15) and falls below at **21:25:40**
  (true 21:26:04).
- **Budget 15 kWh:** reached at **16:57:37**, true 16:56:47, so **+0.845 min**.
- **Iterations to 10⁻⁶ h:** Bisection **18** · Regula Falsi **3–5** · Newton-Raphson **3–4** · Secant **3–4**.
- **Observed orders:** Newton **1.97–2.02** · Secant **1.45–1.87** (theory 1.618) · Regula Falsi **1.00**.
- **All 4 methods agree:** the largest solver error is 9.29 × 10⁻⁷ h (**3.3 ms**).

**Visual:** `output/figures/fig_root_iterations_synthetic.png`. Optional: the "Four methods, one equation" table
cropped from the full-page `output/web_screenshots/1920_synthetic_alerts.png` (the one-screen `screen_` version cuts
the table off after two rows).

---

## Slide 17: Key Findings

- **Data resolution matters more than method choice.**
- Root-finder error **≈ 3 ms**, but alert-time error is **2.59 min** (synthetic) and **76.42 min** (real).
- Real day: the hidden 19:00 reading was a **4.634 kW** peak. The cubic estimates **1.450 kW**, so the alert is
  missed.
- The 1-minute data crosses 3 kW **50 times**; hourly readings see only **2**.
- On smooth data, higher order wins. On spiky real data, **simple methods are as good or better**.

**Visual:** `output/figures/fig_load_curve_real.png` (hourly curve against the 1-minute data). Alternative: the
"Solver error vs model error" chart cropped from the full-page `output/web_screenshots/1920_real_compare.png`.

---

## Slide 18: Conclusion

- Built and verified a **modular, menu-driven C program** with 11 numerical methods chained into one workflow.
- Results match theory: **orders 2.00 / 4.01 / 4.03**, Newton ≈ 2, and **Lagrange = NDD** to 10⁻¹⁶.
- **21 / 21 tests pass**, on test functions, an exact synthetic model and real UCI data.
- Main lesson: **to improve accuracy, read the meter more often**; changing the method helps far less.
- Future: splines, PCHIP, Romberg, Brent's method, Indian data (iAWE), real tariff slabs.

**Visual:** `output/web_screenshots/1920_synthetic_tests.png`, cropped to the "21 / 21" ring. Alternatively, your own
`energy.exe --test` screenshot (checklist item P-3).

---

## Slide 19: References

1. S. S. Sastry, *Introductory Methods of Numerical Analysis*, 5th ed., PHI Learning, 2012.
2. S. C. Chapra & R. P. Canale, *Numerical Methods for Engineers*, 7th ed., McGraw-Hill, 2015.
3. R. L. Burden & J. D. Faires, *Numerical Analysis*, 9th ed., Brooks/Cole, 2011.
4. C. Runge, "Über empirische Funktionen und die Interpolation zwischen äquidistanten Ordinaten," *Z. Math. Phys.*
   46, 1901.
5. G. Hebrail & A. Berard, *Individual Household Electric Power Consumption*, UCI ML Repository, 2006,
   https://doi.org/10.24432/C58K54
6. N. Batra et al., "It's Different: Insights into home energy consumption in India," BuildSys 2013 (iAWE).
7. Chart.js 4.4.1, https://www.chartjs.org (MIT License).

**Visual:** none.

---

## Checklist

### A. Terminal screenshots to take yourself

Build first (`build.bat`), then run from the project folder in a terminal at least 100 characters wide.

| # | Command / menu path | What to capture | Slide |
|---|---|---|---|
| P-1 | `energy.exe` (start) | The header with the team box and the MAIN MENU | 12 |
| P-2 | Menu **3** → **5** (Compare all methods) | The "Estimated power" table, the "Absolute error" table and the CORRECTNESS CHECK line | 12 (or 14) |
| P-3 | `energy.exe --test` | The bottom of the test table with "TOTAL: 21 passed, 0 failed (out of 21)" | 18 |
| P-4 *(optional)* | Menu **5** (press Enter for 3 kW and 15 kWh) | The Crossing 1 table (4 methods, iterations, order p) and the ">> ALERT" lines | 16 |
| P-5 *(optional)* | Menu **7** → **2** | Section 2C, the convergence table with the p column | 15 |

### B. Files from `output/`

| File | Slide |
|---|---|
| `output/synthetic/summary_report.txt` | Numbers on slides 14–17 (check before presenting) |
| `output/real/summary_report.txt` | Numbers on slides 14, 15 and 17 |
| `output/test_results.txt` | Slide 18 |

### C. Figures from `output/figures/`

| PNG | Slide |
|---|---|
| `fig_load_curve_synthetic.png` | 2 |
| `fig_loocv.png` | 14 |
| `fig_load_curve_real.png` | 14 (inset) or 17 |
| `fig_integration_convergence.png` | 15 |
| `fig_sampling_real.png` | 15 |
| `fig_root_iterations_synthetic.png` | 16 |

### D. Dashboard screenshots from `output/web_screenshots/`

| PNG | Slide |
|---|---|
| `screen_1920_synthetic_overview.png` | 1 (mascot crop) and 13 |
| `screen_1920_synthetic_alerts.png` | 4 (alert-card crop), 13 (inset) |
| `1920_synthetic_alerts.png` | 16 (optional; crop to the "Four methods, one equation" table) |
| `1920_real_compare.png` | 17 (alternative; crop to the "Solver error vs model error" chart) |
| `1920_synthetic_tests.png` | 18 (crop to the pass ring) |

### E. Flowchart images to export

| Flowchart | Source | Slide |
|---|---|---|
| Overall system | `REPORT_CONTENT.md` §5.1 → https://mermaid.live → export PNG | 7 |
| NDD local cubic (optional) | `REPORT_CONTENT.md` §5.5 | 8 |
