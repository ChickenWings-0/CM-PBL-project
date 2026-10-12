# Algorithms and Flowcharts

Each section gives the idea, the step-by-step algorithm and a Mermaid flowchart. Mermaid renders on GitHub, in VS Code
(Markdown Preview Mermaid extension) and at https://mermaid.live, where flowcharts can be exported as PNG/SVG for the report.
The function names match the C code in `src/`.

Notation:
- t = time in hours, P = power in kW, E = energy in kWh
- x[i], y[i] = the known readings, n = number of known readings
- h = spacing between readings

---

## 1. Overall system

The three methods form one chain. Each step uses the result of the step before it.

```mermaid
flowchart TD
    A(["Start"]) --> B["Load CSV readings<br/>time_hours, power_kW"]
    B --> C{"Any reading NA?"}
    C -- Yes --> D["INTERPOLATION<br/>estimate each missing reading<br/>Linear / Lagrange / NDD"]
    C -- No --> E
    D --> E["Continuous curve P(t)<br/>local cubic through known readings"]
    E --> F["INTEGRATION<br/>E = integral of P dt<br/>Trapezoidal / Simpson 1/3 / Simpson 3/8"]
    F --> G["Daily energy, peak-window energy, bill"]
    E --> H["ROOT FINDING<br/>P(t) - 3 kW = 0<br/>Bisection / Regula Falsi / Newton / Secant"]
    F --> I["ROOT FINDING<br/>E(t) - 15 kWh = 0"]
    G --> J["ERROR ANALYSIS<br/>compare with exact model or 1-minute data"]
    H --> J
    I --> J
    J --> K["Tables on screen + .txt and .csv files"]
    K --> L(["End"])
```

## 2. Main program (menu)

```mermaid
flowchart TD
    A(["Start"]) --> B["Print header, load synthetic dataset"]
    B --> C["Show menu"]
    C --> D["Read choice 0-9<br/>re-ask if invalid"]
    D --> E{"Choice"}
    E -- 1 --> F1["Load dataset"]
    E -- 2 --> F2["View readings + ASCII chart"]
    E -- 3 --> F3["Fill missing values"]
    E -- 4 --> F4["Energy calculation"]
    E -- 5 --> F5["Threshold + budget times"]
    E -- 6 --> F6["Bill estimate"]
    E -- 7 --> F7["Error analysis"]
    E -- 8 --> F8["Run test cases"]
    E -- 9 --> F9["Export all results"]
    E -- "0 or end of input" --> Z(["Exit"])
    F1 --> C
    F2 --> C
    F3 --> C
    F4 --> C
    F5 --> C
    F6 --> C
    F7 --> C
    F8 --> C
    F9 --> C
```

---

## 3. Interpolation

### 3.1 Finding the interval: `find_interval()`
Start at i = 0 and move right while x[i+1] < t. The result is i such that x[i] ≤ t ≤ x[i+1].

### 3.2 Linear interpolation: `interp_linear()`

**Formula:** P(t) = y[i] + (y[i+1] − y[i]) · (t − x[i]) / (x[i+1] − x[i])

**Error:** at most (h²/8)·max|P''|, so the method is order h².

**Algorithm:**
1. i ← find_interval(x, n, t)
2. Return y[i] + (y[i+1] − y[i])(t − x[i]) / (x[i+1] − x[i])

```mermaid
flowchart TD
    A(["Start: t"]) --> B["i = interval with x_i <= t <= x_i+1"]
    B --> C["slope = (y_i+1 - y_i) / (x_i+1 - x_i)"]
    C --> D["P = y_i + slope * (t - x_i)"]
    D --> E(["Return P"])
```

### 3.3 Choosing the 4 points: `local_stencil()`

**Idea:** use two known readings on each side of t. Near the ends the 4 points are shifted inward.

1. s ← find_interval(x, n, t) − 1
2. If s < 0, set s ← 0
3. If s > n − 4, set s ← n − 4
4. The cubic uses x[s], x[s+1], x[s+2], x[s+3]

The 4 points are chosen per interval, so the curves on both sides of a reading pass through that reading. This makes
the whole curve continuous.

### 3.4 Lagrange local cubic: `interp_lagrange_local()` → `lagrange_poly()`

**Formula:** P(t) = Σᵢ yᵢ·Lᵢ(t), where Lᵢ(t) = Π over j ≠ i of (t − xⱼ)/(xᵢ − xⱼ)

Lᵢ equals 1 at xᵢ and 0 at every other xⱼ, so P passes through every point.

**Algorithm:**
1. s ← local_stencil(x, n, t)
2. sum ← 0
3. For i = 0 to 3:
   1. L ← 1
   2. For j = 0 to 3 with j ≠ i: L ← L·(t − x[s+j])/(x[s+i] − x[s+j])
   3. sum ← sum + y[s+i]·L
4. Return sum

```mermaid
flowchart TD
    A(["Start: t"]) --> B["s = local_stencil: 4 nearest known readings"]
    B --> C["sum = 0, i = 0"]
    C --> D{"i < 4 ?"}
    D -- No --> K(["Return sum"])
    D -- Yes --> E["L = 1, j = 0"]
    E --> F{"j < 4 ?"}
    F -- No --> I["sum = sum + y_i * L<br/>i = i + 1"]
    I --> D
    F -- Yes --> G{"j == i ?"}
    G -- Yes --> H["j = j + 1"]
    G -- No --> J["L = L * (t - x_j) / (x_i - x_j)"]
    J --> H
    H --> F
```

### 3.5 Newton Divided Difference local cubic: `interp_ndd_local()`

**Divided-difference table** (`ndd_build_table()`):
- f[xᵢ] = yᵢ
- f[xᵢ, …, xᵢ₊ⱼ] = (f[xᵢ₊₁, …, xᵢ₊ⱼ] − f[xᵢ, …, xᵢ₊ⱼ₋₁]) / (xᵢ₊ⱼ − xᵢ)

The table is stored as `table[i][j]`. The top row `table[0][0..3]` holds the Newton coefficients c₀ … c₃.

**Newton form** (`ndd_eval()`), evaluated by nested multiplication (Horner's rule):

P(t) = c₀ + c₁(t−x₀) + c₂(t−x₀)(t−x₁) + c₃(t−x₀)(t−x₁)(t−x₂)
     = ((c₃(t−x₂) + c₂)(t−x₁) + c₁)(t−x₀) + c₀

**Algorithm:**
1. s ← local_stencil(x, n, t)
2. For i = 0 to 3: table[i][0] ← y[s+i]
3. For j = 1 to 3, and for i = 0 to 3 − j:
   table[i][j] ← (table[i+1][j−1] − table[i][j−1]) / (x[s+i+j] − x[s+i])
4. result ← table[0][3]
5. For j = 2 down to 0: result ← result·(t − x[s+j]) + table[0][j]
6. Return result

**Correctness check:** Lagrange and NDD build the *same* unique cubic through the same 4 points, so their values must
agree to rounding error (about 10⁻¹⁶). The program prints this check every time.

```mermaid
flowchart TD
    A(["Start: t"]) --> B["s = local_stencil: 4 nearest known readings"]
    B --> C["Column 0: table_i0 = y_i"]
    C --> D["For j = 1..3, i = 0..3-j:<br/>table_ij = (table_i+1,j-1 - table_i,j-1) / (x_i+j - x_i)"]
    D --> E["Coefficients c_j = table_0j"]
    E --> F["result = c3"]
    F --> G["For j = 2 down to 0:<br/>result = result * (t - x_j) + c_j"]
    G --> H(["Return result"])
```

### 3.6 Global Lagrange (demonstration): `interp_lagrange_global()`
This is the same Lagrange formula applied to **all** n known readings at once, giving a polynomial of degree n − 1 = 19.
Between equally spaced points, high-degree polynomials oscillate strongly near the ends of the range
(**Runge's phenomenon**). The program shows a curve error of 24 kW on a load that never exceeds 3.6 kW.

### 3.7 Filling the gaps: `fill_missing()`
For each reading i: if it is missing, out[i] ← interpolate(t[i]) using only the known readings. Otherwise
out[i] ← p[i].

### 3.8 Leave-one-out test (error analysis)
For each inner known reading j:
1. Remove reading j.
2. Predict its value from the other readings.
3. Record the error (prediction − actual).

Then report MAE = mean|e|, RMSE = √(mean e²) and the largest |e|.

```mermaid
flowchart TD
    A(["Start"]) --> B["j = 1"]
    B --> C{"j <= n-2 ?"}
    C -- No --> G["MAE, RMSE, max error per method"]
    G --> H(["End"])
    C -- Yes --> D["Copy all known readings except j"]
    D --> E["Predict value at x_j with each method"]
    E --> F["error = predicted - actual y_j<br/>j = j + 1"]
    F --> C
```

---

## 4. Integration (energy = area under the power curve)

All three rules take readings y₀ … yₘ at equal spacing h, so there are m intervals.

### 4.1 Composite trapezoidal rule: `trapezoidal()`

**Formula:** I = Σ ½(yᵢ + yᵢ₊₁)(xᵢ₊₁ − xᵢ). With equal spacing this becomes h/2·[y₀ + 2(y₁ + … + yₘ₋₁) + yₘ].

**Error:** −(b−a)h²/12·P''(ξ), so the method is **order 2**. The program adds up each trapezoid separately, so it also
works when the spacing varies.

```mermaid
flowchart TD
    A(["Start"]) --> B["sum = 0, i = 0"]
    B --> C{"i < m ?"}
    C -- Yes --> D["sum = sum + 0.5 * (y_i + y_i+1) * (x_i+1 - x_i)<br/>i = i + 1"]
    D --> C
    C -- No --> E(["Return sum"])
```

### 4.2 Composite Simpson's 1/3 rule: `simpson_13()`

**Formula:** I = h/3·[y₀ + 4(y₁ + y₃ + …) + 2(y₂ + y₄ + …) + yₘ]. This needs **m even**.

**Error:** −(b−a)h⁴/180·P⁽⁴⁾(ξ), so the method is **order 4**. It is exact for cubics.

**Fallback:** if m is odd, Simpson is applied to the first m − 1 intervals and the trapezoidal rule to the last one,
and a warning flag is set.

```mermaid
flowchart TD
    A(["Start"]) --> B["m = number of intervals"]
    B --> C{"m even ?"}
    C -- Yes --> D["ms = m, warn = 0"]
    C -- No --> E["ms = m - 1, warn = 1"]
    D --> F["sum = y_0 + y_ms"]
    E --> F
    F --> G["For i = 1..ms-1:<br/>odd i: sum += 4 y_i<br/>even i: sum += 2 y_i"]
    G --> H["I = h/3 * sum"]
    H --> J{"ms < m ?"}
    J -- Yes --> K["I = I + trapezoid on last interval"]
    J -- No --> L(["Return I"])
    K --> L
```

### 4.3 Composite Simpson's 3/8 rule: `simpson_38()`

**Formula:** I = 3h/8·[y₀ + 3y₁ + 3y₂ + 2y₃ + 3y₄ + 3y₅ + 2y₆ + … + yₘ]. This needs **m a multiple of 3**.

**Error:** −(b−a)h⁴/80·P⁽⁴⁾(ξ), so the method is **order 4**.

**Fallback:** 3/8 is applied to the largest multiple of 3 intervals, and the trapezoidal rule to the remaining 1 or 2.

```mermaid
flowchart TD
    A(["Start"]) --> B["m = number of intervals<br/>ms = m - (m mod 3)"]
    B --> C["warn = 1 if ms < m"]
    C --> D["sum = y_0 + y_ms"]
    D --> E["For i = 1..ms-1:<br/>i multiple of 3: sum += 2 y_i<br/>otherwise: sum += 3 y_i"]
    E --> F["I = 3h/8 * sum"]
    F --> G{"ms < m ?"}
    G -- Yes --> H["I = I + trapezoid on leftover intervals"]
    G -- No --> K(["Return I"])
    H --> K
```

### 4.4 Energy up to time t on the curve: `curve_energy_upto()`
This is needed for the energy-budget root. The local cubic is a single cubic on each interval [xᵢ, xᵢ₊₁], and
Simpson's 1/3 rule is exact for cubics. So E(t) is the sum of (hi − lo)/6·[P(lo) + 4P(mid) + P(hi)] over each interval
up to t, where the last interval is cut at t. This gives the **exact** integral of the model curve.

### 4.5 Observed order of convergence
Integrate with h and with h/2. Then p = log₂(|E(h)| / |E(h/2)|), where E is the error. Theory predicts p = 2 for the
trapezoidal rule and p = 4 for both Simpson rules.

---

## 5. Root finding

We solve f(t) = 0 with:
- f(t) = P(t) − 3 kW (when the load crosses the limit), or
- f(t) = E(t) − 15 kWh (when the energy budget is used up).

**Stopping rule** for all four methods: stop when the step |xₖ − xₖ₋₁| (the half-width for bisection) is below 10⁻⁶ h,
or when f is exactly 0, or after 100 iterations.

### 5.1 Finding brackets: `scan_brackets()`
1. Evaluate f every 0.25 h (15 min) from the first to the last reading.
2. Every sign change between two neighbouring points gives a bracket [a, b] that contains a root.
3. If f is exactly 0 at a check point, record [x, x]: that point is itself a root.

```mermaid
flowchart TD
    A(["Start"]) --> B["x_prev = t0, f_prev = f t0"]
    B --> C["x = x_prev + step"]
    C --> D{"x > t_end ?"}
    D -- Yes --> Z(["Return list of brackets"])
    D -- No --> E["fx = f x"]
    E --> F{"fx == 0 ?"}
    F -- Yes --> G["Add bracket x, x"]
    F -- No --> H{"f_prev * fx < 0 ?"}
    H -- Yes --> I["Add bracket x_prev, x"]
    H -- No --> J["no root here"]
    G --> K["x_prev = x, f_prev = fx"]
    I --> K
    J --> K
    K --> C
```

### 5.2 Bisection: `bisection()`

**Idea:** halve the bracket each time and keep the half where the sign changes.

**Convergence:** always converges. The error bound halves every step: order 1, error ratio ½.
Iterations needed ≈ log₂((b − a)/tol). For a 0.25 h bracket that is 18.

```mermaid
flowchart TD
    A(["Start: a, b with f a * f b < 0"]) --> B["c = (a + b) / 2"]
    B --> C{"f c == 0 or (b - a)/2 < tol ?"}
    C -- Yes --> D(["Root = c"])
    C -- No --> E{"f a * f c < 0 ?"}
    E -- Yes --> F["b = c"]
    E -- No --> G["a = c"]
    F --> B
    G --> B
```

### 5.3 Regula Falsi (false position): `regula_falsi()`

**Idea:** join (a, f(a)) and (b, f(b)) with a straight line and take the point where it crosses zero.

**Formula:** c = b − f(b)(b − a) / (f(b) − f(a))

**Convergence:** always converges because the root stays bracketed. The order is 1, but it is usually much faster than
bisection. One endpoint often stays fixed, which keeps it linear.

```mermaid
flowchart TD
    A(["Start: a, b with f a * f b < 0"]) --> B["c = b - f b * (b - a) / (f b - f a)"]
    B --> C{"f c == 0 or abs(c - c_old) < tol ?"}
    C -- Yes --> D(["Root = c"])
    C -- No --> E{"f a * f c < 0 ?"}
    E -- Yes --> F["b = c"]
    E -- No --> G["a = c"]
    F --> H["c_old = c"]
    G --> H
    H --> B
```

### 5.4 Newton-Raphson: `newton_raphson()`

**Idea:** follow the tangent line down to zero.

**Formula:** xₖ₊₁ = xₖ − f(xₖ)/f'(xₖ)

**Slope:** central difference, f'(x) ≈ [f(x+δ) − f(x−δ)]/(2δ) with δ = 10⁻⁵ h (`central_diff()`). Its error is about
δ², which is far below the tolerance.

**Start:** x₀ = middle of the bracket.

**Convergence:** order 2 near the root, so the number of correct digits roughly doubles each step. It is not guaranteed:
if an iterate leaves the bracket, the program reports "FAILED (left bracket)".

```mermaid
flowchart TD
    A(["Start: x0 = middle of bracket"]) --> B["slope = (f(x+d) - f(x-d)) / 2d"]
    B --> C{"slope == 0 ?"}
    C -- Yes --> X(["Fail: flat curve"])
    C -- No --> D["x_new = x - f x / slope"]
    D --> E{"x_new outside a, b ?"}
    E -- Yes --> Y(["Fail: left bracket"])
    E -- No --> F{"abs(x_new - x) < tol ?"}
    F -- Yes --> G(["Root = x_new"])
    F -- No --> H["x = x_new"]
    H --> B
```

### 5.5 Secant: `secant()`

**Idea:** like Newton, but the slope comes from the last two iterates, so no derivative is needed.

**Formula:** xₖ₊₁ = xₖ − f(xₖ)(xₖ − xₖ₋₁) / (f(xₖ) − f(xₖ₋₁))

**Start:** x₀ = a, x₁ = b.

**Convergence:** order (1 + √5)/2 ≈ 1.618. Like Newton, it is not guaranteed.

```mermaid
flowchart TD
    A(["Start: x0 = a, x1 = b"]) --> B{"f x1 == f x0 ?"}
    B -- Yes --> X(["Fail: horizontal secant"])
    B -- No --> C["x2 = x1 - f x1 * (x1 - x0) / (f x1 - f x0)"]
    C --> D{"x2 outside a, b ?"}
    D -- Yes --> Y(["Fail: left bracket"])
    D -- No --> E{"abs(x2 - x1) < tol ?"}
    E -- Yes --> F(["Root = x2"])
    E -- No --> G["x0 = x1, x1 = x2"]
    G --> B
```

### 5.6 Error measures for root finding
- **Solver error** = |root − x*|, where x* is bisection run to 10⁻¹³ on the **same** curve. This measures how well the method solved the equation it was given.
- **Model error** = root − true crossing time. This measures how well the curve, built from hourly readings, matches reality.
- **Observed order** p = ln(eₖ₊₁/eₖ) / ln(eₖ/eₖ₋₁), using the last three errors above 10⁻¹² (the starting value counts as e₀).

### 5.7 Threshold analysis: `threshold_core()`

```mermaid
flowchart TD
    A(["Start"]) --> B["Build curve P(t): local cubic through known readings"]
    B --> C["Scan f = P - threshold every 15 min"]
    C --> D{"Any brackets?"}
    D -- No --> E["Report: limit never crossed"]
    D -- Yes --> F["For each bracket:<br/>solve with all 4 methods<br/>direction = up or down"]
    F --> G["Compare with true crossing time"]
    G --> H["Total time above limit"]
    E --> I["Compute E(24) by Simpson on the curve"]
    H --> I
    I --> J{"E(24) >= budget ?"}
    J -- No --> K["Report: budget never reached"]
    J -- Yes --> L["Scan E - budget, solve with all 4 methods"]
    L --> M(["Print alerts"])
    K --> M
```

---

## 6. Web front-end data flow (optional part)

The web page contains no numerical methods. The numbers always come from the C program, through one of two paths.

**Live mode** (`python web/serve.py`): each input change runs the C program once.

```mermaid
sequenceDiagram
    participant U as User
    participant P as Web page (app.js)
    participant S as serve.py
    participant C as energy.exe
    U->>P: change an input (e.g. threshold 2.5 kW)
    P->>P: wait 350 ms for typing to stop
    P->>S: GET /api/run?dataset=synthetic&threshold=2.5&...
    S->>S: check every value is a finite number
    S->>C: energy.exe --json synthetic out.json 2.5 15.0 18.0 22.0 8.0 1 -1.0
    C->>C: check ranges, then interpolate, integrate, find roots
    C-->>S: out.json (results, or an error message)
    S-->>P: JSON
    P->>U: redraw tables and charts
```

**Offline mode** (opening `web/index.html` directly from disk):

```mermaid
flowchart LR
    A["energy.exe --export-web<br/>(also run by --export-all)"] --> B["web/data/results.js<br/>both datasets at default settings + tests"]
    B --> C["index.html loads it as a script"]
    C --> D["Page shows the saved results<br/>inputs locked, banner explains why"]
```

At start-up the page asks `/api/ping`. If the server answers, the page uses live mode; otherwise it uses offline mode.
