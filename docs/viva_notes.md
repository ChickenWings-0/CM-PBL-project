# Viva Notes

Short answers to the questions an examiner is likely to ask, with the project's own numbers. Every number here comes
from `energy.exe --export-all` (files in `output/`). If the code changes, re-check the numbers.

---

## 1. The 60-second explanation

> Smart meters record a home's power demand at regular times, but some readings get lost. We built a C program that:
> 1. fills in lost readings by **interpolation**,
> 2. works out the energy used, which is the area under the power curve, by **numerical integration**, for the bill, and
> 3. finds the exact time the load goes above the 3 kW limit, and the time the 15 kWh daily budget runs out,
>    by **root finding**.
>
> We test every method against data where we **know the true answer**: an exact mathematical model of a Delhi summer
> day, and a real household's 1-minute meter data. So we measure real errors, not just compare methods with each other.

## 2. Key results to quote

### Synthetic day (exact answers known)

| What | Result |
|---|---|
| Lost readings, local cubic (Lagrange = NDD) | Largest error 0.070 kW (at 19:00). Leave-one-out RMSE 0.157 kW, against 0.268 kW for linear |
| Lagrange vs NDD | Differ by about 10⁻¹⁶ kW. They are the same cubic written in two forms (correctness check) |
| Global Lagrange (degree 19) | Curve error up to **24 kW** near midnight. This is Runge's phenomenon |
| Exact daily energy | 30.6882 kWh. At h = 1 h: trapezoidal +0.15 %, Simpson 1/3 +0.33 %, Simpson 3/8 +0.26 % (after gap filling) |
| Convergence orders (observed) | Trapezoidal **2.00**; Simpson 1/3 **4.01**; Simpson 3/8 **4.03** (theory: 2, 4, 4) |
| 3 kW crossings | 19:23:40 up and 21:25:40 down. True times 19:26:15 and 21:26:04, so the model error is −2.6 and −0.4 min |
| Iterations, tolerance 10⁻⁶ h | Bisection 18, Regula Falsi 5, Newton-Raphson 3, Secant 4 |
| Observed order | Regula Falsi 1.00, Newton ≈ 2.0, Secant ≈ 1.5–1.8 (theory 1.618) |
| 15 kWh budget | Used up at 16:58 (true 16:57). This is before the 18:00 peak window even starts |
| Solver error vs model error | Solver ≤ 9 × 10⁻⁷ h (3 ms). Model up to 2.6 min. **The data limits accuracy, not the root finder** |

### Real day (UCI, 14 May 2007)

| What | Result |
|---|---|
| Metered energy | 24.463 kWh. From hourly readings with gaps filled: Simpson 1/3 −7.7 %, trapezoidal −9.3 % |
| Leave-one-out | Linear RMSE 1.15 kW **beats** local cubic 1.40 kW. Real load is spiky, and the cubic overshoots |
| Hidden 19:00 reading | Was a 4.63 kW peak. Every method estimates only about 1.5 kW, so a 3 kW crossing is **missed** |
| Crossings | The 1-minute data crosses 3 kW 50 times (first at 07:00). The hourly curve sees only 2 |
| Budget time | Found at 20:41, but the truth is 19:25: **76 min late**, again because of the lost 19:00 peak |
| Sampling study | Reading every 60 min: error 3–12 %. Every 30 min: 0.3–1.5 %. Every minute: < 0.2 % |

## 3. Questions and answers

### About the problem and data

**Q: Why these three methods together?**
They form one chain. Interpolation fills the gaps and gives a continuous curve P(t). Integration of P(t) gives energy.
Root finding on P(t) and E(t) gives the alert times. Errors in one step carry into the next, and section 2B of the output
measures that.

**Q: How do you know your answers are correct?**
We use three tiers:
1. **Hand-solvable test functions** (tests T1–T9). For example, Simpson must give exactly 4 for ∫₀² x³ dx.
2. **A synthetic day built from a formula**, so the exact energy (using `erf`) and the exact crossing times are known.
3. **Real data where we hide readings we actually have**, compared against the 1-minute data.

**Q: Why bell curves (Gaussians) and not sine waves for the synthetic day?**
For a sum of sine waves over one full period, the trapezoidal rule is almost exact. It would beat Simpson and make
the comparison misleading. The bell-curve day has different slopes at 0:00 and 24:00, so the normal h² error appears.

**Q: How is the synthetic data made?**
`generate_synthetic_csv()` evaluates P(t) every hour from 0 to 24, rounds to 4 decimals (like a meter display) and writes
NA at hours 4, 9, 13, 14 and 19. The parameters are illustrative, not fitted to measured Delhi data.

**Q: Where does the real data come from?**
The UCI Machine Learning Repository: Hebrail & Berard (2006), a house near Paris, with one reading per minute for nearly
4 years. Licence CC BY 4.0, DOI 10.24432/C58K54. We extracted 14 May 2007 using `scripts/extract_uci_day.py`.
Full steps are in `docs/data_source.md`.

**Q: Why hours 4, 9, 13, 14 and 19 for the missing readings?**
Each tests a different case:
- 04:00: flat night load (easy)
- 09:00: steep slope after the morning peak
- 13:00 and 14:00: two lost readings in a row
- 19:00: on the evening ramp, right where the 3 kW alert matters

### Interpolation

**Q: Why not Newton's forward difference formula?**
It needs equally spaced points. Once readings are missing, the known points are no longer equally spaced (11, 12, 15, 16).
Lagrange and divided differences work with any spacing.

**Q: Lagrange and NDD give the same numbers. Why implement both?**
Through 4 points there is only **one** cubic, so both must agree. That makes them a correctness check on each other:
the program prints max |Lagrange − NDD| ≈ 10⁻¹⁶. They also teach different things:
- NDD shows the divided-difference table.
- NDD can be extended by adding one point without restarting.
- NDD evaluates efficiently by nested multiplication.

**Q: Why a local cubic and not one polynomial through all points?**
Runge's phenomenon. A degree-19 polynomial through our 20 points swings up to 24 kW near midnight on a curve whose true
maximum is 3.6 kW. The leave-one-out RMSE is 47 kW, against 0.16 kW for the local cubic.

**Q: How are the 4 points chosen?**
`local_stencil()` finds the interval [xᵢ, xᵢ₊₁] containing t and uses xᵢ₋₁, xᵢ, xᵢ₊₁, xᵢ₊₂, shifted inward at the ends.
Choosing per interval keeps the curve continuous: both neighbouring cubics pass through each reading.

**Q: On real data, linear beat the cubic. Is the cubic wrong?**
No. A cubic assumes the curve is smooth. Real load jumps when an appliance switches on, and the cubic overshoots around
jumps. Higher order only helps on smooth data, which is a key conclusion of the project.

**Q: Why don't you extrapolate a missing first or last reading?**
Interpolation is reliable *between* known points. Outside them the error is not controlled, so the program rejects such
files (test T12).

### Integration

**Q: Why does Simpson not beat the trapezoidal rule at h = 1 hour?**
The O(h⁴) error bound assumes the 4th derivative is small compared with 1/h⁴. The morning peak has a width of σ = 1 h,
the same as the reading interval, so it is under-resolved. The convergence table shows Simpson wins clearly once
h ≤ 0.5 h, and its error then drops 16× per halving (p = 4.01).

**Q: What happens if the number of intervals is odd for Simpson 1/3?**
Simpson is applied to the even part and the trapezoidal rule to the last interval, and a `*` warning is printed (test T14).
The 18:00–22:00 window has 4 intervals, which does not suit Simpson 3/8, so 3/8 does 3 intervals and the trapezoidal rule does 1.

**Q: How is E(t) computed for the budget root?**
`curve_energy_upto()` applies Simpson 1/3 to each interval of the local-cubic curve. Simpson is exact for cubics, so E(t)
is the exact integral of our model curve. Note that E'(t) = P(t).

**Q: What is the reference energy for the real day?**
Each 1-minute value is that minute's average power, so energy = Σ P × (1/60 h) = 24.463 kWh.

### Root finding

**Q: Why are the brackets found first?**
Power can cross 3 kW several times. Checking f every 15 minutes finds each sign change. Each bracket [a, b] then contains
one root, which bisection and Regula Falsi need, and it gives Newton and Secant good starting points.

**Q: Newton needs f'(t). Where does it come from?**
From a central difference, f'(t) ≈ [f(t+δ) − f(t−δ)]/(2δ) with δ = 10⁻⁵ h. Its error is about δ² ≈ 10⁻¹⁰, which is far
below our tolerance. It works for any curve, so the same Newton code solves both P(t) = 3 and E(t) = 15.

**Q: Why does Newton use fewer iterations than bisection?**
Bisection only halves the bracket each step, so 0.25 h → 10⁻⁶ h takes log₂(250000) ≈ 18 steps. Newton converges
quadratically: the errors were 9 × 10⁻⁵, then 2 × 10⁻⁹, then 4 × 10⁻¹⁴. The number of correct digits roughly doubles each step.

**Q: Why is bisection's order shown as "1*"?**
Its error |cₖ − x*| jumps up and down (see the iteration history), so the order formula gives nonsense values. What *does*
halve every step is the bracket width, so its order is 1 with ratio ½.

**Q: Then why use bisection at all?**
It **always** works if the bracket is valid. Newton and Secant can jump out of the bracket; the code checks for this and
reports "FAILED (left bracket)". In practice, a safe method sits behind a fast one.

**Q: What is the difference between solver error and model error?**
- **Solver error:** how well the method solved the equation we gave it (≤ 3 ms here).
- **Model error:** how well that equation, a curve built from hourly readings, matches reality (minutes on the
  synthetic day, up to 76 min on the real day).

The second is about 46,000 times bigger on the synthetic day (2.59 min against 9.29 × 10⁻⁷ h) and about 1.5 million
times bigger on the real day (76.42 min against 8.54 × 10⁻⁷ h). So choosing a better root finder cannot improve the
alert time. More frequent readings can.

### Code questions

**Q: Why are there global (static) variables in analysis.c?**
The root finders take a function `double f(double t)`. C has no closures, so f cannot carry the dataset with it.
`curve_set()` stores the known readings in file-level variables, and `f_power()` / `f_energy()` read them.

**Q: What is `rprintf`?**
Our printf that also copies the text into report files during export. The same code produces the screen output and the
`.txt` reports. It also fixes Windows' 3-digit exponents (1.2e-005 → 1.2e-05).

**Q: What does `Func` mean in root_finding.h?**
`typedef double (*Func)(double);` is a pointer to a function. It lets one bisection routine solve any equation.

**Q: Why fixed-size arrays (`MAX_POINTS 2000`)?**
Simplicity: no `malloc`/`free` and no memory leaks. One day of 1-minute data (1441 values) fits.

**Q: How do you handle bad input?**
- Menu numbers are read as text and checked with `strtol`, and the prompt repeats if the input is invalid.
- CSV rows are checked: numbers parsed, times increasing, NA allowed only inside the range.
- A failed load never destroys the current dataset, because loading uses a temporary copy (`g_temp`).
- End of input exits cleanly.

### The web front-end

**Q: Is the web page part of the C implementation?**
No. It is a presentation layer. The numerical methods exist only in C. The page shows numbers that `energy.exe` writes
as JSON:
- **Live:** `web/serve.py` runs `energy.exe --json ...` every time an input changes.
- **Offline:** it reads `web/data/results.js`, which `energy.exe --export-web` produced.

The JavaScript only formats numbers and draws charts.

**Q: Why Python for the server?**
It is about 100 lines and uses only the standard library. Writing an HTTP server in C (sockets) would add a lot of code
with no numerical content.

**Q: How do you know the page shows the same numbers as the terminal?**
Both come from the same C functions. For example:
- Crossing time 19:23:40, Newton in 3 iterations.
- Exact energy 30.6882 kWh.
- 21 of 21 tests passed.

These match `output/synthetic/root_results.txt`, `output/synthetic/integration_results.txt` and
`output/test_results.txt`.

**Q: What if someone types nonsense into the page?**
The input is checked twice. `serve.py` accepts only finite numbers, and the C program checks them again: it rejects text,
`nan` and `inf`, and checks that the threshold and budget are above 0, the peak window lies inside the day and the rate is
not negative. A rejected value comes back as `{"error": ...}` and the page shows that message. The NaN check matters
because every comparison with NaN is false, so `threshold <= 0` alone would let a NaN through.

## 4. Limitations (say these before you are asked)

1. Readings are treated as instantaneous samples. Real meters record 15- or 30-minute averages.
2. The 15-minute bracket scan can miss two crossings that are closer together than 15 minutes.
3. The local cubic is continuous, but its slope can jump at readings.
4. The real data is French, not Indian. iAWE (a Delhi house, IIIT-Delhi) is the natural next dataset.
5. The bill uses a flat rate. Real tariffs have slabs, fixed charges and time-of-day rates.
6. Hourly readings fundamentally cannot capture minute-level spikes. No numerical method can recover information that was never measured.

## 5. Possible future enhancements
- Cubic spline interpolation (smooth slope, needs a tridiagonal solve).
- Shape-preserving interpolation (e.g. PCHIP) to avoid overshoot on spiky data.
- Romberg integration or adaptive Simpson.
- Brent's method (bisection safety + secant speed).
- Indian datasets (iAWE), real DERC tariff slabs, time-of-day pricing.
- Live alerts from a real smart-meter feed, and multi-day forecasting.
