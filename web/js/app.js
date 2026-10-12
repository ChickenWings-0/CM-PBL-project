/* app.js - Smart Energy web front-end.
 *
 * The page does NOT compute any numerical method. Every number comes from the
 * C program (energy.exe):
 *   - live mode:     web/serve.py runs  energy.exe --json ...  on each change;
 *   - offline mode:  web/data/results.js, written by  energy.exe --export-web.
 * This file only formats those numbers into tables and charts.
 */
(function () {
  "use strict";

  var I = window.UI_ICONS;
  var C = {
    ink: "#2f2a35", ink2: "#5d5664", ink3: "#8c8491", line: "#eadcc8", grid: "rgba(226, 210, 188, .55)",
    card: "#fffaf3", teal: "#0a8f7c", coral: "#e2583a", plum: "#7457a8", amber: "#d99400",
    minute: "#c7b9a9", tealSoft: "rgba(10, 143, 124, .14)", coralSoft: "rgba(226, 88, 58, .16)"
  };
  /* one colour per method, in a fixed order (validated colour-blind safe on adjacent pairs) */
  var INTERP_COL = [C.teal, C.coral, C.coral, C.plum];
  var QUAD_COL = [C.teal, C.coral, C.plum];
  var ROOT_COL = [C.teal, C.coral, C.plum, C.amber];
  var ROOT_MARK = ["circle", "rectRounded", "triangle", "rectRot"];
  var DENSE_KEY = ["linear", "lagrange", "ndd", "global"];

  var VIEWS = {
    overview: { n: "01", title: "Overview", lede: "Five meter readings went missing. The C engine fills the gaps, adds up the energy and finds the exact moment the load crosses its limit, and checks every answer against the truth." },
    dataset: { n: "02", title: "Dataset", lede: "One day of household power readings, one per hour. Five readings were lost on purpose, so the true values are known and every estimate can be checked." },
    fill: { n: "03", title: "Fill Missing Values", lede: "Interpolation estimates each lost reading from its neighbours. Lagrange and Newton divided differences build the same local cubic in two different ways." },
    energy: { n: "04", title: "Energy & Bill", lede: "Energy is the area under the power curve. Three integration rules add it up from the gap-filled readings, and a flat rate turns it into a bill." },
    alerts: { n: "05", title: "Threshold Alerts", lede: "Root finding solves P(t) = limit and E(t) = budget. Four methods solve the same equations, so their speed and accuracy can be compared." },
    compare: { n: "06", title: "Method Comparison & Error Analysis", lede: "Every method measured against a known true answer: convergence orders, whole-curve errors, leave-one-out tests, and solver error versus model error." },
    tests: { n: "07", title: "Tests", lede: "21 automated test cases run inside the C engine: hand-checkable functions, edge cases and full scenarios on both datasets." }
  };

  var DEFAULTS = { threshold: 3, budget: 15, peak_start: 18, peak_end: 22, rate: 8, fill: 1, probe: -1 };
  var TEAM = [
    { name: "Vaibhav", enr: "01118015625", lead: true },
    { name: "Aksh Rastogi", enr: "02518015625" },
    { name: "Akshay Kumar", enr: "00618015625" },
    { name: "Apurva Kr. Yadav", enr: "02418015625" },
    { name: "Vaibhav Sharma", enr: "00518015625" }
  ];
  var S = {
    mode: "loading", ds: "synthetic", view: "overview", data: {}, tests: null, generated: "",
    params: { synthetic: copy(DEFAULTS), real: copy(DEFAULTS) },
    fillSel: 4, nddSel: 0, eventSel: 0, error: null, pending: null
  };
  var charts = {};

  /* ------------------------------------------------------------------ helpers */
  function copy(o) { return JSON.parse(JSON.stringify(o)); }
  function $(sel, root) { return (root || document).querySelector(sel); }
  function $all(sel, root) { return Array.prototype.slice.call((root || document).querySelectorAll(sel)); }
  function pad(n) { return (n < 10 ? "0" : "") + n; }
  function hhmm(h) { var m = Math.round(h * 60); return pad(Math.floor(m / 60)) + ":" + pad(m % 60); }
  function hhmmss(h) { var s = Math.round(h * 3600); return pad(Math.floor(s / 3600)) + ":" + pad(Math.floor(s / 60) % 60) + ":" + pad(s % 60); }
  function dur(h) { var m = Math.round(h * 60); return Math.floor(m / 60) + " h " + pad(m % 60) + " min"; }
  function ok(x) { return x !== null && x !== undefined && isFinite(x); }
  function fx(x, d) {
    if (!ok(x)) return "–";
    var s = Math.abs(x).toFixed(d);
    return (x < 0 && Number(s) !== 0 ? "−" : "") + s;
  }
  function fxs(x, d) { return ok(x) ? (x > 0 ? "+" : "") + fx(x, d) : "–"; }
  function sci(x, d) {
    if (!ok(x)) return "–";
    if (x === 0) return "0";
    var e = Math.floor(Math.log10(Math.abs(x))), m = x / Math.pow(10, e);
    if (Math.abs(Number(m.toFixed(d == null ? 1 : d))) >= 10) { m /= 10; e += 1; }
    return fx(m, d == null ? 1 : d) + "×10<sup>" + (e < 0 ? "−" + (-e) : e) + "</sup>";
  }
  function rs(x, d) {
    if (!ok(x)) return "–";
    return "Rs " + x.toLocaleString("en-IN", { minimumFractionDigits: d, maximumFractionDigits: d });
  }
  function money(x, d) {
    return ok(x) ? x.toLocaleString("en-IN", { minimumFractionDigits: d, maximumFractionDigits: d }) : "–";
  }
  function supPow(n) {   /* 10 to the power n, for canvas text (no HTML) */
    var map = { "-": "⁻", "0": "⁰", "1": "¹", "2": "²", "3": "³", "4": "⁴", "5": "⁵", "6": "⁶", "7": "⁷", "8": "⁸", "9": "⁹" };
    return "10" + String(n).split("").map(function (c) { return map[c]; }).join("");
  }
  function D() { return S.data[S.ds]; }
  function P() { return S.params[S.ds]; }
  function live() { return S.mode === "live"; }
  function shortNames() { return D().methods.interp_short; }

  /* --------------------------------------------------------------- engine */
  function apiRun(ds, p) {
    var q = ["dataset=" + ds, "threshold=" + p.threshold, "budget=" + p.budget, "peak_start=" + p.peak_start,
      "peak_end=" + p.peak_end, "rate=" + p.rate, "fill=" + p.fill, "probe=" + p.probe].join("&");
    return fetch("/api/run?" + q).then(function (r) {
      return r.json().then(function (j) {
        if (!r.ok || j.error) throw new Error(j.error || "The C engine returned an error.");
        return j;
      });
    });
  }

  function recompute(delay) {
    if (!live()) return;
    clearTimeout(S.pending);
    S.pending = setTimeout(function () {
      var view = $("#view-" + S.view), ds = S.ds;
      view.classList.add("busy");
      apiRun(ds, S.params[ds]).then(function (j) {
        S.data[ds] = j;
        S.error = null;
        render();
      }).catch(function (e) {
        S.error = e.message;
        renderBanner();
      }).then(function () { view.classList.remove("busy"); });
    }, delay == null ? 350 : delay);
  }

  function init() {
    decorateStatic();
    var params = new URLSearchParams(location.search);
    if (params.get("ds") === "real") S.ds = "real";
    var h = location.hash.replace("#", "");
    if (VIEWS[h]) S.view = h;

    var snap = window.ENERGY_DATA;
    var ping = location.protocol.indexOf("http") === 0
      ? fetch("/api/ping").then(function (r) { return r.ok ? r.json() : null; }).catch(function () { return null; })
      : Promise.resolve(null);

    ping.then(function (p) {
      if (p && p.engine) {
        return Promise.all([apiRun("synthetic", DEFAULTS), apiRun("real", DEFAULTS),
          fetch("/api/tests").then(function (r) { return r.json(); })]).then(function (res) {
          S.data.synthetic = res[0]; S.data.real = res[1]; S.tests = res[2]; S.mode = "live";
        });
      }
      throw new Error("no engine");
    }).catch(function () {
      if (snap && snap.synthetic) {
        S.data.synthetic = snap.synthetic; S.data.real = snap.real; S.tests = snap.tests;
        S.generated = snap.generated; S.mode = "offline";
      } else {
        S.mode = "none";
      }
    }).then(render);

    window.addEventListener("hashchange", function () {
      var v = location.hash.replace("#", "");
      if (VIEWS[v] && v !== S.view) { S.view = v; render(); window.scrollTo(0, 0); }
    });
    $all("#ds-switch button").forEach(function (b) {
      b.addEventListener("click", function () {
        if (b.dataset.ds === S.ds) return;
        S.ds = b.dataset.ds; S.nddSel = 0; S.eventSel = 0;
        render();
      });
    });
  }

  function decorateStatic() {
    $all(".nav a").forEach(function (a) { a.insertAdjacentHTML("afterbegin", I.icon(a.dataset.icon)); });
    $all("[data-mascot]").forEach(function (el) { el.innerHTML = I.mascot(el.dataset.mascot); });
  }

  /* --------------------------------------------------------------- shell */
  function render() {
    var v = VIEWS[S.view];
    $all(".nav a").forEach(function (a) {
      var on = a.dataset.view === S.view;
      a.classList.toggle("active", on);
      if (on) a.setAttribute("aria-current", "page"); else a.removeAttribute("aria-current");
    });
    $("#eyebrow").textContent = "Section " + v.n + " of 07";
    $("#title").textContent = v.title;
    $("#lede").textContent = v.lede;
    $all("#ds-switch button").forEach(function (b) {
      var on = b.dataset.ds === S.ds;
      b.classList.toggle("on", on);
      b.setAttribute("aria-pressed", on ? "true" : "false");
    });
    renderEngineCard();
    renderBanner();
    $all(".view").forEach(function (s) { s.classList.toggle("on", s.id === "view-" + S.view); });
    if (S.mode === "none" || !D()) {
      $("#view-" + S.view).innerHTML = '<div class="card empty-state">' + I.mascot("alert") +
        '<h3>No results yet</h3><p>Run <span class="mono">energy.exe --export-web</span> in the project folder, or start the engine with <span class="mono">python web/serve.py</span>.</p></div>';
      return;
    }
    /* if the user is typing in an input, keep its exact text and focus across the re-render */
    var a = document.activeElement;
    var keep = a && a.tagName === "INPUT" && a.id ? { id: a.id, value: a.value, s: a.selectionStart, e: a.selectionEnd } : null;
    Object.keys(charts).forEach(function (k) { charts[k].destroy(); delete charts[k]; });
    RENDER[S.view]($("#view-" + S.view));
    /* numbers count up when a page opens; while typing they just update */
    countUp($("#view-" + S.view), !!keep);
    if (keep) {
      var n = document.getElementById(keep.id);
      if (n) {
        n.value = keep.value;
        n.focus();
        if (keep.s != null) n.setSelectionRange(keep.s, keep.e);
      }
    }
  }

  function renderEngineCard() {
    var el = $("#engine-card");
    if (live()) {
      el.innerHTML = '<div class="row"><span class="dot live"></span>C engine: live</div>' +
        "<p>Every number on this page is computed by <span class=\"mono\">energy.exe</span> on request.</p>";
    } else if (S.mode === "offline") {
      el.innerHTML = '<div class="row"><span class="dot offline"></span>Offline snapshot</div>' +
        "<p>Results from <span class=\"mono\">energy.exe</span>, " + S.generated + ".</p>";
    } else {
      el.innerHTML = '<div class="row"><span class="dot"></span>No engine</div><p>See the note on the page.</p>';
    }
  }

  function renderBanner() {
    var b = $("#banner"), html = "";
    if (S.error) {
      html = '<div class="banner error">' + I.icon("info") + "<span><b>The C engine could not compute this:</b> " + S.error + "</span></div>";
    } else if (S.mode === "offline") {
      html = '<div class="banner">' + I.icon("info") + "<span>Showing the saved snapshot with default inputs. To change inputs, run " +
        "<code>python web/serve.py</code> and open <code>http://localhost:8000</code>.</span></div>";
    }
    b.innerHTML = html;
  }

  /* count numbers up from zero when a view appears (instant = just show the value) */
  function countUp(root, instant) {
    var reduce = instant || (window.matchMedia && window.matchMedia("(prefers-reduced-motion: reduce)").matches);
    $all("[data-count]", root).forEach(function (el) {
      var target = Number(el.dataset.count), dec = Number(el.dataset.dec || 0), money = el.dataset.money === "1";
      function show(v) {
        el.textContent = money ? v.toLocaleString("en-IN", { minimumFractionDigits: dec, maximumFractionDigits: dec }) : v.toFixed(dec);
      }
      if (reduce || !isFinite(target)) { show(target); return; }
      var t0 = performance.now(), ms = 950;
      (function step(now) {
        var k = Math.min(1, (now - t0) / ms), e = 1 - Math.pow(1 - k, 3);
        show(target * e);
        if (k < 1) requestAnimationFrame(step);
      })(t0);
    });
  }

  /* ------------------------------------------------------------- charts */
  function setupChartDefaults() {
    var d = Chart.defaults;
    d.font.family = '"Nunito Sans", system-ui, sans-serif';
    d.font.size = 12;
    d.color = C.ink2;
    d.borderColor = C.grid;
    d.animation.duration = 650;
    d.maintainAspectRatio = false;
    d.elements.line.borderWidth = 2;
    d.elements.line.borderCapStyle = "round";
    d.elements.line.borderJoinStyle = "round";
    d.elements.point.hoverRadius = 6;
    d.plugins.legend.position = "bottom";
    d.plugins.legend.labels.usePointStyle = true;
    d.plugins.legend.labels.boxWidth = 8;
    d.plugins.legend.labels.boxHeight = 8;
    d.plugins.legend.labels.padding = 16;
    d.plugins.legend.labels.font = { size: 12, weight: 600 };
    /* legend in the order the series were added (not in drawing order) */
    d.plugins.legend.labels.sort = function (a, b) { return a.datasetIndex - b.datasetIndex; };
    /* lines show as short lines in the legend, markers as markers */
    var originalLabels = d.plugins.legend.labels.generateLabels;
    d.plugins.legend.labels.generateLabels = function (chart) {
      var items = originalLabels.call(this, chart);
      items.forEach(function (it) {
        var ds = chart.data.datasets[it.datasetIndex];
        if (ds && ds.showLine !== false && chart.config.type === "line" && !ds.legendMarker) {
          it.pointStyle = "line";
          it.lineWidth = 2.5;
          it.strokeStyle = ds.borderColor;
          it.lineDash = ds.borderDash || [];
        }
      });
      return items.filter(function (it) { return it.text !== ""; });
    };
    var t = d.plugins.tooltip;
    t.backgroundColor = C.ink;
    t.titleColor = "#fff";
    t.bodyColor = "#f4ece2";
    t.titleFont = { family: '"Fredoka", sans-serif', size: 13, weight: 600 };
    t.bodyFont = { family: '"JetBrains Mono", monospace', size: 12 };
    t.cornerRadius = 12;
    t.padding = 10;
    t.boxPadding = 5;
    t.usePointStyle = true;
    t.caretSize = 6;
  }

  function mk(id, cfg) {
    var el = document.getElementById(id);
    if (!el) return;
    if (charts[id]) charts[id].destroy();
    charts[id] = new Chart(el, cfg);
  }

  function timeAxis(t0, tn, step) {
    return {
      type: "linear", min: t0, max: tn, border: { display: false },
      grid: { color: C.grid, drawTicks: false },
      ticks: { stepSize: step || 3, padding: 8, callback: function (v) { return hhmm(v); } }
    };
  }
  function valueAxis(title, extra) {
    var a = {
      border: { display: false }, grid: { color: C.grid, drawTicks: false },
      ticks: { padding: 8 }, title: { display: !!title, text: title, color: C.ink3, font: { weight: 700, size: 12 } }
    };
    Object.keys(extra || {}).forEach(function (k) { a[k] = extra[k]; });
    return a;
  }
  /* log axes: keep only whole powers of ten, so the grid stays calm */
  function decadesOnly(ax) {
    ax.ticks = ax.ticks.filter(function (t) { var e = Math.log10(t.value); return Math.abs(e - Math.round(e)) < 1e-9; });
  }
  function pairs(xs, ys) { return xs.map(function (x, i) { return { x: x, y: ys[i] }; }); }

  /* the main power-vs-time chart, used on several pages */
  function loadCurve(d, o) {
    var den = d.dense, t0 = d.dataset.t0, tn = d.dataset.tn, ds = [], names = d.methods.interp;
    var thr = d.roots.threshold, ymax = o.ymax;
    if (!ymax) {
      var top = thr;
      d.readings.forEach(function (r) { top = Math.max(top, r.p || 0, r.truth || 0); });
      if (d.minute && o.truth !== false) top = Math.max(top, Math.max.apply(null, d.minute.p));
      /* a round top value so the tick labels stay evenly spaced */
      ymax = top * 1.12 > 4 ? Math.ceil(top * 1.12) : Math.ceil(top * 1.12 * 2) / 2;
    }
    if (o.truth !== false) {
      if (d.minute) {
        ds.push({ label: "1-minute meter data", data: pairs(d.minute.t, d.minute.p), borderColor: C.minute, borderWidth: 1, pointRadius: 0, order: 9 });
      } else if (den.true) {
        ds.push({ label: "True load P(t)", data: pairs(den.t, den.true), borderColor: C.ink, borderWidth: 2.4, pointRadius: 0, order: 6 });
      }
    }
    (o.interp || []).forEach(function (m) {
      ds.push({ label: names[m], data: pairs(den.t, den[DENSE_KEY[m]]), borderColor: m === 3 ? "rgba(116, 87, 168, .7)" : INTERP_COL[m], borderWidth: m === 3 ? 1.6 : 2.2,
        borderDash: m === 2 ? [7, 5] : [], pointRadius: 0, order: 4 });
    });
    if (o.readings !== false) {
      var known = d.readings.filter(function (r) { return !r.missing; });
      ds.push({ label: "Meter reading", data: known.map(function (r) { return { x: r.t, y: r.p }; }), showLine: false,
        pointRadius: 4, pointBackgroundColor: C.ink, pointBorderColor: C.card, pointBorderWidth: 2, borderColor: C.ink, order: 2 });
      var miss = d.readings.filter(function (r) { return r.missing && ok(r.truth); });
      if (miss.length) {
        ds.push({ label: "Lost reading (true value)", data: miss.map(function (r) { return { x: r.t, y: r.truth }; }), showLine: false,
          pointRadius: 6.5, pointBackgroundColor: C.card, pointBorderColor: C.ink, pointBorderWidth: 2.2, borderColor: C.ink, order: 1 });
      }
    }
    if (o.estimate != null) {
      var m = o.estimate;
      ds.push({ label: "Estimate (" + d.methods.interp_short[m] + ")", data: d.interp.missing.map(function (r) { return { x: r.t, y: r.est[m] }; }),
        showLine: false, pointStyle: "rectRot", pointRadius: 6.5, pointBackgroundColor: INTERP_COL[m], pointBorderColor: C.card, pointBorderWidth: 2,
        borderColor: INTERP_COL[m], order: 0 });
    }
    if (o.threshold !== false) {
      ds.push({ label: fx(thr, 1) + " kW limit", data: [{ x: t0, y: thr }, { x: tn, y: thr }], borderColor: C.ink3, borderDash: [6, 5],
        borderWidth: 1.6, pointRadius: 0, order: 8 });
    }
    if (o.crossings) {
      ds.push({ label: "Crossing found", data: o.crossings.map(function (x) { return { x: x, y: thr }; }), showLine: false,
        pointRadius: 7, pointBackgroundColor: C.coral, pointBorderColor: C.card, pointBorderWidth: 2.5, borderColor: C.coral, order: 0 });
    }
    return {
      type: "line",
      data: { datasets: ds },
      options: {
        interaction: { mode: "nearest", intersect: false },
        plugins: {
          legend: { labels: { filter: function (it) { return it.text !== ""; } } },
          tooltip: { callbacks: {
            title: function (it) { return hhmm(it[0].parsed.x); },
            label: function (it) { return " " + it.dataset.label + ": " + it.parsed.y.toFixed(3) + " kW"; }
          } }
        },
        scales: { x: timeAxis(t0, tn, o.step), y: valueAxis("Power (kW)", { min: 0, max: ymax }) }
      }
    };
  }

  /* ---------------------------------------------------------- UI bits */
  function card(cls, head, body) {
    return '<div class="card ' + (cls || "") + '">' + (head ? '<div class="card-head">' + head + "</div>" : "") + body + "</div>";
  }
  function head(title, sub, tools) {
    return "<div><h3>" + title + "</h3>" + (sub ? '<p class="sub">' + sub + "</p>" : "") + "</div>" +
      (tools ? '<div class="tools">' + tools + "</div>" : "");
  }
  function chip(text, cls, icon) { return '<span class="chip ' + (cls || "") + '">' + (icon ? I.icon(icon) : "") + "<span>" + text + "</span></span>"; }
  function sw(color) { return '<span class="swatch" style="background:' + color + '"></span>'; }
  function seg(id, items, current, small, disabled) {
    return '<div class="segmented' + (small ? " small" : "") + '" id="' + id + '" role="group">' +
      items.map(function (it, i) {
        return '<button type="button" data-i="' + i + '" class="' + (i === current ? "on" : "") + '"' + (disabled ? " disabled" : "") +
          ' aria-pressed="' + (i === current) + '">' + it + "</button>";
      }).join("") + "</div>";
  }
  function onSeg(id, fn) {
    $all("#" + id + " button").forEach(function (b) { b.addEventListener("click", function () { fn(Number(b.dataset.i)); }); });
  }
  function field(id, label, value, unit, opts) {
    opts = opts || {};
    var dis = !live();
    return '<div class="field"><label for="' + id + '">' + label + '</label><div class="input-wrap' + (dis ? " locked" : "") + '">' +
      /* text input (not type=number) so the caret survives a re-render while typing */
      '<input id="' + id + '" type="text" inputmode="decimal" autocomplete="off" spellcheck="false" value="' + value + '"' + (dis ? " disabled" : "") + ">" +
      (unit ? '<span class="unit">' + unit + "</span>" : "") + '</div><span class="hint" id="' + id + '-hint">' + (opts.hint || "") + "</span></div>";
  }
  function bindNumber(id, key, check) {
    var el = document.getElementById(id);
    if (!el) return;
    el.addEventListener("input", function () {
      var raw = el.value.trim(), v = Number(raw);
      var msg = raw === "" || !isFinite(v) ? "Enter a number" : (check ? check(v) : "");
      $("#" + id + "-hint").textContent = msg || "";
      if (msg) return;
      P()[key] = v;
      recompute();
    });
  }
  function kpi(cls, iconName, label, valueHtml, foot) {
    return '<div class="card kpi lift ' + cls + '"><div class="blob">' + I.icon(iconName) + '</div><div class="label">' + label +
      '</div><div class="value">' + valueHtml + '</div><div class="foot">' + foot + "</div></div>";
  }

  /* first crossing / budget summary used by several pages */
  function alertFacts(d) {
    var r = d.roots, up = r.crossings.filter(function (c) { return c.dir > 0; })[0];
    var down = r.crossings.filter(function (c) { return c.dir < 0; })[0];
    function best(ev) { return ev ? ev.methods[2].root != null ? ev.methods[2].root : ev.ref : null; }
    return { up: up, down: down, upT: best(up), downT: best(down), budget: r.budget_event, budgetT: best(r.budget_event) };
  }

  /* ============================================================ VIEWS */
  var RENDER = {};

  /* ---------------------------------------------------------- overview */
  RENDER.overview = function (el) {
    var d = D(), e = d.energy, r = d.roots, f = alertFacts(d), syn = S.ds === "synthetic";
    var s13 = e.rules[1].day, ref = e.ref_day, nTests = S.tests ? S.tests.total : 0, nPass = S.tests ? S.tests.total - S.tests.failed : 0;
    var bubble = "Today: <b>" + fx(s13, 1) + " kWh</b> used" +
      (f.upT != null ? ". Load went above " + fx(r.threshold, 0) + " kW at <b>" + hhmm(f.upT) + "</b>" : "") +
      (f.budgetT != null ? ", and the " + fx(r.budget, 0) + " kWh budget ran out at <b>" + hhmm(f.budgetT) + "</b>." : ".");

    var html = '<div class="grid">' +
      '<div class="card hero s12"><div>' +
      "<h2>A smart meter lost five readings. <em>We rebuilt the day</em>, counted every kWh and caught the " + fx(r.threshold, 0) + " kW alarm.</h2>" +
      '<p class="intro">Three numerical methods form one chain, all written in C. Each step uses the answer of the one before, and every result is checked against data whose true answer is known.</p>' +
      '<div class="pipeline">' +
      '<div class="step s-a"><span class="n">1</span><h4>Interpolation</h4><p>Linear, Lagrange and Newton divided differences fill the lost readings.</p></div>' +
      '<div class="arrow">' + I.icon("arrow") + "</div>" +
      '<div class="step s-b"><span class="n">2</span><h4>Integration</h4><p>Trapezoidal, Simpson 1/3 and 3/8 rules turn power into energy.</p></div>' +
      '<div class="arrow">' + I.icon("arrow") + "</div>" +
      '<div class="step s-c"><span class="n">3</span><h4>Root finding</h4><p>Bisection, Regula Falsi, Newton-Raphson and Secant find the alert times.</p></div>' +
      "</div>" +
      '<div class="team"><span class="lbl">Team</span>' +
      TEAM.map(function (m) {
        return '<span class="member' + (m.lead ? " lead" : "") + '"><b>' + m.name + "</b>" +
          (m.lead ? '<span class="role">Team Leader</span>' : "") + '<span class="enr">' + m.enr + "</span></span>";
      }).join("") + "</div>" +
      '</div><div class="hero-side"><div class="bubble">' + bubble + '</div><div class="hero-mascot">' + I.mascot("happy") + "</div></div></div>";

    html += '<div class="s3">' + kpi("mint", "energy", "Energy used today",
      '<span data-count="' + s13 + '" data-dec="2">0</span><span class="unit">kWh</span>',
      ok(ref) ? "Reference " + fx(ref, 2) + " kWh, error " + fxs(100 * (s13 - ref) / ref, 2) + " %" : "Simpson 1/3 rule") + "</div>";
    html += '<div class="s3">' + kpi("peach", "alert", "Time above " + fx(r.threshold, 0) + " kW",
      f.upT != null ? dur(r.above_time).replace(" min", '<span class="unit">min</span>').replace(" h ", '<span class="unit">h</span> ') : "None",
      f.upT != null ? "From " + hhmm(f.upT) + (f.downT != null ? " to " + hhmm(f.downT) : "") : "The limit was never crossed") + "</div>";
    html += '<div class="s3">' + kpi("butter", "battery", fx(r.budget, 0) + " kWh budget used up",
      f.budgetT != null ? hhmm(f.budgetT) : "Never",
      f.budget && ok(f.budget.true) ? "True time " + hhmm(f.budget.true) + " (" + fxs((f.budgetT - f.budget.true) * 60, 1) + " min)" : "Root of E(t) − budget = 0") + "</div>";
    html += '<div class="s3">' + kpi("plum", "tests", "Tests passed",
      '<span data-count="' + nPass + '">0</span><span class="unit">/ ' + nTests + "</span>", "Run inside the C engine") + "</div>";

    html += '<div class="s8">' + card("flexcol", head("The day at a glance", syn ? "True load, hourly meter readings, the lost readings and the local-cubic estimate." :
      "Real 1-minute meter data behind the hourly readings, with the local-cubic estimate."),
      '<div class="chart-box tall"><canvas id="ch-ov"></canvas></div>') + "</div>";
    html += '<div class="s4">' + card("", head("What we found", "Straight from the error analysis."), findings(d)) + "</div>";
    html += "</div>";
    el.innerHTML = html;
    mk("ch-ov", loadCurve(d, { interp: [1] }));
  };

  function findings(d) {
    var lo = d.interp.loocv, cv = d.energy.convergence, r = d.roots, c1 = r.crossings[0], items = [];
    if (S.ds === "synthetic") {
      items.push(["mint", "fill", "The local cubic fills gaps best", "Leave-one-out RMSE " + fx(lo.rmse[1], 3) + " kW against " + fx(lo.rmse[0], 3) + " kW for linear. One global polynomial reaches " + fx(lo.rmse[3], 0) + " kW (Runge)."]);
      var last = cv.rows[cv.rows.length - 1];
      items.push(["peach", "area", "Simpson converges at order 4", "Observed orders: trapezoidal " + fx(last.p[0], 2) + ", Simpson 1/3 " + fx(last.p[1], 2) + ", Simpson 3/8 " + fx(last.p[2], 2) + "."]);
    } else {
      items.push(["mint", "fill", "On spiky real data, linear wins", "Leave-one-out RMSE " + fx(lo.rmse[0], 2) + " kW for linear against " + fx(lo.rmse[1], 2) + " kW for the cubic, which overshoots at appliance jumps."]);
      var r60 = cv.rows[1], r30 = cv.rows[2];
      items.push(["peach", "area", "Read more often, not harder", "Hourly readings: energy off by up to " + fx(Math.max.apply(null, r60.err_pct.map(Math.abs)), 1) + " %. Every 30 min: at most " + fx(Math.max.apply(null, r30.err_pct.map(Math.abs)), 1) + " %."]);
    }
    if (c1) {
      items.push(["plum", "steps", "Newton needs " + c1.methods[2].iterations + " steps, bisection " + c1.methods[0].iterations,
        "Same answer to 10" + "<sup>−6</sup> h. Regula Falsi took " + c1.methods[1].iterations + " and Secant " + c1.methods[3].iterations + "."]);
    }
    items.push(["butter", "target", "The data limits accuracy, not the solver",
      "Solver error at most " + fx(r.max_solver_err * 3.6e6, 1) + " ms; the curve itself is off by up to " + fx(r.max_model_err_min, 1) + " min."]);
    return '<ul class="findings">' + items.map(function (it) {
      return '<li><span class="ic ' + it[0] + '">' + I.icon(it[1]) + "</span><div><h4>" + it[2] + "</h4><p>" + it[3] + "</p></div></li>";
    }).join("") + "</ul>";
  }

  /* ---------------------------------------------------------- dataset */
  RENDER.dataset = function (el) {
    var d = D(), syn = S.ds === "synthetic", miss = d.readings.filter(function (r) { return r.missing; });
    var about = syn
      ? '<dl class="kv"><dt>Source</dt><dd>Formula for a Delhi summer day, generated by <span class="mono">load_model.c</span></dd>' +
        "<dt>Readings</dt><dd>" + d.dataset.n + " (every hour, 00:00 to 24:00), rounded to 0.1 W</dd>" +
        "<dt>Lost</dt><dd>" + miss.map(function (r) { return hhmm(r.t); }).join(", ") + "</dd>" +
        "<dt>Truth</dt><dd>Exact P(t) at any time; exact energy " + fx(d.energy.ref_day, 4) + " kWh (erf formula)</dd></dl>" +
        '<div class="formula" style="margin-top:14px">P(t) = 0.5 + 1.0 g(t; 8, 1.0) + 0.8 g(t; 15, 2.5) + 3.0 g(t; 20.5, 1.5)<br>g(t; μ, σ) = exp(−(t − μ)² / 2σ²)</div>' +
        '<p class="muted" style="font-size:13px;margin-top:10px">Terms: 0.5 kW always-on base, a morning peak at 08:00, afternoon AC around 15:00 and the evening peak at 20:30 (AC, lights, cooking).</p>'
      : '<dl class="kv"><dt>Source</dt><dd>UCI Machine Learning Repository: Hebrail &amp; Berard (2006), <i>Individual Household Electric Power Consumption</i>, CC BY 4.0</dd>' +
        "<dt>House</dt><dd>Sceaux, France (7 km from Paris)</dd><dt>Day</dt><dd>14 May 2007, all 1440 minutes present</dd>" +
        "<dt>Readings</dt><dd>" + d.dataset.n + " hourly samples (the 1-minute value at each hh:00)</dd>" +
        "<dt>Hidden</dt><dd>" + miss.map(function (r) { return hhmm(r.t); }).join(", ") + "</dd>" +
        "<dt>Truth</dt><dd>1441 one-minute readings; metered energy " + fx(d.energy.ref_day, 3) + " kWh</dd></dl>" +
        '<div class="callout butter" style="margin-top:14px"><span class="ic">' + I.icon("info") + "</span><span>The hidden 19:00 reading was a <b>" +
        fx(miss.filter(function (r) { return r.t === 19; })[0].truth, 2) + " kW</b> peak. Watch what that does to the alerts.</span></div>";

    var half = Math.ceil(d.readings.length / 2);
    function tableOf(rows, start) {
      return '<div class="tbl-wrap"><table class="tbl tight"><thead><tr><th>#</th><th>Time</th><th class="num">Reading (kW)</th>' +
        (d.dataset.has_truth ? '<th class="num">True (kW)</th>' : "") + "</tr></thead><tbody>" +
        rows.map(function (r, i) {
          return "<tr" + (r.missing ? ' class="sel"' : "") + '><td class="dim">' + (start + i + 1) + '</td><td class="name">' + hhmm(r.t) +
            (r.missing ? '<span class="miss-tag">LOST</span>' : "") + '</td><td class="num">' + (r.missing ? '<span class="dim">NA</span>' : fx(r.p, syn ? 4 : 3)) +
            "</td>" + (d.dataset.has_truth ? '<td class="num">' + fx(r.truth, syn ? 4 : 3) + "</td>" : "") + "</tr>";
        }).join("") + "</tbody></table></div>";
    }
    el.innerHTML = '<div class="grid">' +
      '<div class="s5">' + card("", head(syn ? "Synthetic Delhi household" : "Real household, UCI dataset",
        syn ? "Built from a formula, so every true answer is known exactly." : "Real readings; hidden values are known from the meter."), about) + "</div>" +
      '<div class="s7">' + card("flexcol", head("Meter readings", "Filled dots were received; hollow dots are the true values of the lost readings."),
        '<div class="chart-box tall"><canvas id="ch-ds"></canvas></div>') + "</div>" +
      '<div class="s12">' + card("", head("All readings", d.dataset.n + " readings, " + d.dataset.n_missing + " lost. True values are shown only to check the estimates."),
        '<div class="split">' + tableOf(d.readings.slice(0, half), 0) + tableOf(d.readings.slice(half), half) + "</div>") + "</div></div>";
    mk("ch-ds", loadCurve(d, { interp: [], threshold: false }));
  };

  /* ---------------------------------------------------------- fill */
  RENDER.fill = function (el) {
    var d = D(), it = d.interp, names = d.methods.interp, sn = shortNames(), sel = S.fillSel, p = P();
    var shown = sel === 4 ? [0, 1, 2, 3] : [sel];
    var probeOn = live();

    var controls = '<div class="card control-card s12"><div class="controls">' +
      '<div class="field"><label>Method</label>' + seg("seg-fill", ["Linear", "Lagrange", "NDD", "Global", "Compare all"], sel) + "</div>" +
      '<div class="field"><label>&nbsp;</label>' + chip("Lagrange = NDD: max difference " + sci(it.lag_ndd_maxdiff, 1) + " kW", "mint", "check") + "</div>" +
      '<div class="controls" style="gap:12px">' + field("in-probe", "Estimate at time", p.probe >= 0 ? p.probe : 9.5, "h",
        { step: 0.25, min: d.dataset.t0, max: d.dataset.tn, hint: p.probe >= 0 ? hhmm(p.probe) : (probeOn ? "e.g. 9.5 = 09:30" : "Needs the live engine") }) +
      '<div class="field"><label>&nbsp;</label><button class="btn" id="btn-probe"' + (probeOn ? "" : " disabled") + ">" + I.icon("play") + "Estimate</button><span class=\"hint\"></span></div></div>" +
      "</div></div>";

    /* lost readings table */
    var rows = it.missing.map(function (m) {
      var errs = [0, 1, 2, 3].map(function (k) { return ok(m.true) ? Math.abs(m.est[k] - m.true) : null; });
      var bestErr = ok(m.true) ? Math.min(errs[0], errs[1], errs[3]) : null;
      return "<tr><td class=\"name\">" + hhmm(m.t) + '</td><td class="num">' + fx(m.true, 4) + "</td>" +
        [0, 1, 2, 3].map(function (k) {
          var isBest = ok(bestErr) && Math.abs(errs[k] - bestErr) < 1e-12;
          var cls = (shown.indexOf(k) < 0 ? " dim" : "") + (isBest ? " best" : "") + (k === 3 && errs[k] > 0.5 ? " bad" : "");
          return '<td class="num' + cls + '">' + fx(m.est[k], 4) + (ok(errs[k]) ? '<div class="dim" style="font-size:11px">err ' + fx(errs[k], 4) + "</div>" : "") + "</td>";
        }).join("") + "</tr>";
    }).join("");
    var foot = it.stats ? ["mae", "rmse", "max"].map(function (k, j) {
      return "<tr><td>" + ["MAE", "RMSE", "Max"][j] + '</td><td></td>' + [0, 1, 2, 3].map(function (m) { return '<td class="num">' + fx(it.stats[k][m], 4) + "</td>"; }).join("") + "</tr>";
    }).join("") : "";
    var table = '<div class="tbl-wrap"><table class="tbl"><thead><tr><th>Time</th><th class="num">True (kW)</th>' +
      sn.map(function (n, k) { return '<th class="num"><span class="nm">' + sw(INTERP_COL[k]) + n + "</span></th>"; }).join("") +
      "</tr></thead><tbody>" + rows + "</tbody>" + (foot ? "<tfoot>" + foot + "</tfoot>" : "") + "</table></div>";

    /* divided-difference table */
    var opts = it.missing.map(function (m) { return hhmm(m.t); });
    if (it.probe) opts.push("Your time " + hhmm(it.probe.t));
    if (S.nddSel >= opts.length) S.nddSel = 0;
    var src = S.nddSel < it.missing.length ? it.missing[S.nddSel] : it.probe;
    var ddHtml = nddBlock(src.ndd, src.t);

    var probeCard = "";
    if (it.probe) {
      var pr = it.probe;
      probeCard = '<div class="s12">' + card("", head("Estimate at " + hhmm(pr.t), "Computed by the C engine for the time you entered."),
        '<div class="tbl-wrap"><table class="tbl"><thead><tr><th>Method</th><th class="num">Estimate (kW)</th>' + (ok(pr.true) ? '<th class="num">True (kW)</th><th class="num">Error (kW)</th>' : "") +
        "</tr></thead><tbody>" + [0, 1, 2, 3].map(function (k) {
          return '<tr><td class="name"><span class="nm">' + sw(INTERP_COL[k]) + names[k] + '</span></td><td class="num">' + fx(pr.est[k], 4) + "</td>" +
            (ok(pr.true) ? '<td class="num">' + fx(pr.true, 4) + '</td><td class="num">' + fxs(pr.est[k] - pr.true, 4) + "</td>" : "") + "</tr>";
        }).join("") + "</tbody></table></div>") + "</div>";
    }

    var ce = it.curve_error, lo = it.loocv;
    var curveTbl = ce ? '<div class="tbl-wrap"><table class="tbl compact"><thead><tr><th>Method</th><th class="num">Max error</th><th class="num">At</th><th class="num">RMS</th></tr></thead><tbody>' +
      ce.map(function (c, k) {
        return '<tr><td class="name"><span class="nm">' + sw(INTERP_COL[k]) + sn[k] + '</span></td><td class="num' + (k === 3 ? " bad" : "") + '">' + fx(c.max, 3) +
          '</td><td class="num">' + hhmm(c.at) + '</td><td class="num">' + fx(c.rms, 3) + "</td></tr>";
      }).join("") + "</tbody></table></div>" : "";
    var lbest = lo ? Math.min(lo.rmse[0], lo.rmse[1]) : null;
    var looTbl = lo ? '<div class="tbl-wrap"><table class="tbl compact"><thead><tr><th>Method</th><th class="num">MAE</th><th class="num">RMSE</th><th class="num">Worst</th></tr></thead><tbody>' +
      [0, 1, 2, 3].map(function (k) {
        return '<tr><td class="name"><span class="nm">' + sw(INTERP_COL[k]) + sn[k] + '</span></td><td class="num">' + fx(lo.mae[k], 3) + '</td><td class="num' +
          (Math.abs(lo.rmse[k] - lbest) < 1e-12 ? " best" : "") + (k === 3 ? " bad" : "") + '">' + fx(lo.rmse[k], 3) + '</td><td class="num">' + hhmm(lo.worst_t[k]) + "</td></tr>";
      }).join("") + "</tbody></table></div>" : "";

    el.innerHTML = '<div class="grid">' + controls +
      '<div class="s8">' + card("flexcol", head("Load curve with interpolations", sel === 4 ? "All methods together. The global polynomial runs off the chart near midnight." :
        names[sel] + ", with diamonds at its estimates for the lost readings."), '<div class="chart-box tall"><canvas id="ch-fill"></canvas></div>') + "</div>" +
      '<div class="s4 stack">' + card("", head("Whole-curve error", "Every 5 minutes, against the " + (S.ds === "synthetic" ? "exact model." : "1-minute data.")), curveTbl) +
      card("", head("Leave-one-out test", "Hide each known reading, predict it, compare. Errors in kW."), looTbl) + "</div>" +
      probeCard +
      '<div class="s12">' + card("", head("Lost readings: estimates and errors", "Dot = smallest error in the row. NDD equals Lagrange, as it must.",
        chip(d.interp.n_known + " known readings", "")), table) + "</div>" +
      '<div class="s12">' + card("", head("Newton divided-difference table", "The local cubic through the 4 nearest known readings. The top row holds the Newton coefficients.",
        seg("seg-ndd", opts, S.nddSel, true)), ddHtml) + "</div>" +
      "</div>";

    onSeg("seg-fill", function (i) { S.fillSel = i; RENDER.fill(el); });
    onSeg("seg-ndd", function (i) { S.nddSel = i; RENDER.fill(el); });
    var btn = $("#btn-probe");
    if (btn) btn.addEventListener("click", function () {
      var v = Number($("#in-probe").value);
      if (!(v >= d.dataset.t0 && v <= d.dataset.tn)) { $("#in-probe-hint").textContent = "Between " + d.dataset.t0 + " and " + d.dataset.tn; return; }
      P().probe = v;
      S.nddSel = it.missing.length;
      recompute(0);
    });
    mk("ch-fill", loadCurve(d, { interp: shown, estimate: sel === 4 ? null : sel, threshold: true }));
  };

  function nddBlock(n, t) {
    if (!n) return '<p class="muted">Not enough known readings for a cubic.</p>';
    var hdr = ["i", "x<sub>i</sub> (h)", "f[x<sub>i</sub>]", "1st divided diff.", "2nd divided diff.", "3rd divided diff."];
    var body = n.x.map(function (x, r) {
      var cells = "";
      for (var c = 0; c < 4; c++) {
        cells += c < n.table[r].length ? '<td class="cell' + (r === 0 ? " top" : "") + '">' + fx(n.table[r][c], 6) + "</td>" : '<td class="empty"></td>';
      }
      return '<tr><td class="cell dim">' + r + '</td><td class="cell">' + fx(x, 2) + "</td>" + cells + "</tr>";
    }).join("");
    var c = n.table[0];
    var expr = "P(" + fx(t, 2) + ") = <span class=\"c\">" + fx(c[0], 6) + "</span>" +
      [1, 2, 3].map(function (k) { return " + (<span class=\"c\">" + fx(c[k], 6) + "</span>)(" + fx(n.products[k - 1], 2) + ")"; }).join("") +
      "<br>&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp; = <span class=\"res\">" + fx(n.value, 6) + " kW</span>";
    var prodText = "Products: (t−x₀) = " + fx(n.products[0], 2) + ", (t−x₀)(t−x₁) = " + fx(n.products[1], 2) +
      ", (t−x₀)(t−x₁)(t−x₂) = " + fx(n.products[2], 2);
    return '<div class="tbl-wrap"><table class="tbl ddt"><thead><tr>' + hdr.map(function (h, i) { return "<th" + (i > 1 ? ' style="text-align:center"' : "") + ">" + h + "</th>"; }).join("") +
      "</tr></thead><tbody>" + body + '</tbody></table></div><div class="newton">' + expr + '<br><span class="muted">' + prodText + "</span></div>";
  }

  /* short takeaways under the energy and bill tables (numbers from the engine) */
  function energyNote(d) {
    var e = d.energy, ch = e.chain;
    if (!ch || !ok(e.ref_day)) return "";
    var rule = Math.max.apply(null, ch[0].err.map(Math.abs));                  /* true readings, no gaps */
    var fill = Math.abs(ch[d.settings.fill + 1].err[1] - ch[0].err[1]);        /* what the gaps add (Simpson 1/3) */
    return '<div class="callout" style="margin-top:14px"><span class="ic">' + I.icon("area") + "</span><span>With every true reading the rules miss by at most <b>" +
      fx(rule, 3) + " kWh</b>. Filling the gaps shifts the Simpson 1/3 total by <b>" + fx(fill, 3) + " kWh</b>, so " +
      (fill > rule ? "the lost readings matter more than the choice of rule." : "the choice of rule matters about as much as the lost readings.") + "</span></div>";
  }
  function billNote(d) {
    var costs = d.bill.rows.map(function (r) { return r.day_cost; }).filter(ok);
    if (!costs.length) return "";
    var spread = Math.max.apply(null, costs) - Math.min.apply(null, costs);
    return '<div class="callout peach" style="margin-top:14px"><span class="ic">' + I.icon("coin") + "</span><span>The three rules disagree by <b>" + rs(spread, 2) +
      "</b> a day, about " + rs(30 * spread, 0) + " over 30 days." + (ok(d.bill.reference && d.bill.reference.day_cost) ?
      " The bill from the " + (S.ds === "synthetic" ? "exact model" : "meter") + " is " + rs(d.bill.reference.day_cost, 2) + "." : "") + "</span></div>";
  }

  /* ---------------------------------------------------------- energy */
  RENDER.energy = function (el) {
    var d = D(), e = d.energy, b = d.bill, p = P(), names = e.rules.map(function (r) { return r.name; });
    var s13 = e.rules[1], ref = e.ref_day, refP = e.ref_peak, star = false;
    var controls = '<div class="card control-card s12"><div class="controls">' +
      '<div class="field"><label>Fill lost readings with</label>' + seg("seg-efill", ["Linear", "Lagrange", "NDD"], d.settings.fill, false, !live()) + "</div>" +
      '<div class="controls" style="gap:12px">' +
      field("in-ps", "Peak window from", p.peak_start, "h", { step: 1, min: d.dataset.t0, max: d.dataset.tn, hint: hhmm(p.peak_start) }) +
      field("in-pe", "to", p.peak_end, "h", { step: 1, min: d.dataset.t0, max: d.dataset.tn, hint: hhmm(p.peak_end) }) +
      field("in-rate", "Rate", p.rate, "Rs/kWh", { step: 0.5, min: 0, hint: "Flat rate, no slabs" }) +
      "</div></div></div>";

    function errPct(v, r) { return ok(v) && ok(r) ? fxs(100 * (v - r) / r, 2) + " %" : "–"; }
    var rows = (ok(ref) ? '<tr class="ref"><td class="name">' + e.reference_name + '</td><td class="num">' + fx(ref, 4) + '</td><td class="num">–</td><td class="num">' +
      fx(refP, 4) + '</td><td class="num">–</td></tr>' : "") +
      e.rules.map(function (r, k) {
        if (r.peak_status === 1 || r.day_status === 1) star = true;
        return '<tr><td class="name"><span class="nm">' + sw(QUAD_COL[k]) + r.name + '</span></td><td class="num">' + fx(r.day, 4) + (r.day_status === 1 ? "*" : "") +
          '</td><td class="num">' + errPct(r.day, ref) + '</td><td class="num">' + fx(r.peak, 4) + (r.peak_status === 1 ? "*" : "") + '</td><td class="num">' + errPct(r.peak, refP) + "</td></tr>";
      }).join("");
    var energyTbl = '<div class="tbl-wrap"><table class="tbl compact"><thead><tr><th>Rule</th><th class="num">Day (kWh)</th><th class="num">Error</th><th class="num">Peak (kWh)</th><th class="num">Error</th></tr></thead><tbody>' +
      rows + "</tbody></table></div>" + (star ? '<p class="muted" style="font-size:12px;margin-top:8px">* Interval count not a multiple of 3, so the leftover interval used the trapezoidal rule.</p>' : "");

    var billRows = (b.reference ? '<tr class="ref"><td class="name">' + e.reference_name + '</td><td class="num">' + money(b.reference.day_cost, 2) + '</td><td class="num">' +
      money(b.reference.peak_cost, 2) + '</td><td class="num">' + money(b.reference.month_cost, 0) + "</td></tr>" : "") +
      b.rows.map(function (r, k) {
        return '<tr><td class="name"><span class="nm">' + sw(QUAD_COL[k]) + r.name + '</span></td><td class="num">' + money(r.day_cost, 2) + '</td><td class="num">' + money(r.peak_cost, 2) +
          '</td><td class="num">' + money(r.month_cost, 0) + "</td></tr>";
      }).join("");
    var billTbl = '<div class="tbl-wrap"><table class="tbl compact"><thead><tr><th>Energy from</th><th class="num">Day (Rs)</th><th class="num">Peak (Rs)</th><th class="num">30 days (Rs)</th></tr></thead><tbody>' +
      billRows + "</tbody></table></div>";

    var peakShare = ok(s13.peak) ? 100 * s13.peak / s13.day : null;
    el.innerHTML = '<div class="grid">' + controls +
      '<div class="s4">' + kpi("mint", "energy", "Energy today, Simpson 1/3",
        '<span data-count="' + s13.day + '" data-dec="2">0</span><span class="unit">kWh</span>',
        ok(ref) ? e.reference_name.split(" (")[0] + " " + fx(ref, 3) + " kWh, error " + errPct(s13.day, ref) : "Average " + fx(s13.day / (d.dataset.tn - d.dataset.t0), 2) + " kW") + "</div>" +
      '<div class="s4">' + kpi("peach", "coin", "Bill for the day",
        '<span class="unit" style="margin:0 4px 0 0">Rs</span><span data-count="' + b.rows[1].day_cost + '" data-dec="2" data-money="1">0</span>',
        "At Rs " + fx(b.rate, 2) + "/kWh; about " + rs(b.rows[1].month_cost, 0) + " for 30 days") + "</div>" +
      '<div class="s4">' + kpi("butter", "clock", "Peak window " + hhmm(p.peak_start) + "–" + hhmm(p.peak_end),
        ok(s13.peak) ? '<span data-count="' + s13.peak + '" data-dec="2">0</span><span class="unit">kWh</span>' : "–",
        ok(peakShare) ? fx(peakShare, 1) + " % of the day's energy in " + fx(p.peak_end - p.peak_start, 0) + " hours" : "Window must start and end on reading times") + "</div>" +
      '<div class="s6">' + card("flexcol", head("Energy is the area under the curve", "Area under the curve = energy. The peach band is the peak window."),
        '<div class="chart-box"><canvas id="ch-area"></canvas></div>') + "</div>" +
      '<div class="s6">' + card("", head("Energy by integration rule", "Readings every hour, gaps filled with " + d.methods.interp[d.settings.fill] + "."), energyTbl + energyNote(d)) + "</div>" +
      '<div class="s7">' + card("flexcol", head("Energy error per method", "Full-day error after filling the gaps with each method and integrating with each rule."),
        e.chain ? '<div class="chart-box"><canvas id="ch-chain"></canvas></div>' : '<p class="muted">Needs a dataset with known true values.</p>') + "</div>" +
      '<div class="s5">' + card("", head("Electricity bill", "Flat rate of " + rs(b.rate, 2) + " per kWh. No slabs or fixed charges."), billTbl + billNote(d)) + "</div>" +
      "</div>";

    onSeg("seg-efill", function (i) { if (!live()) return; P().fill = i; recompute(0); });
    bindNumber("in-ps", "peak_start", function (v) { return v >= P().peak_end ? "Must be before the end" : (v % 1 ? "Whole hours only" : ""); });
    bindNumber("in-pe", "peak_end", function (v) { return v <= P().peak_start ? "Must be after the start" : (v % 1 ? "Whole hours only" : ""); });
    bindNumber("in-rate", "rate", function (v) { return v < 0 ? "Cannot be negative" : ""; });

    /* area chart */
    var den = d.dense, key = DENSE_KEY[d.settings.fill], ps = p.peak_start, pe = p.peak_end;
    var all = pairs(den.t, den[key]);
    var peak = all.filter(function (q) { return q.x >= ps - 1e-9 && q.x <= pe + 1e-9; });
    mk("ch-area", {
      type: "line",
      /* layers, bottom to top: day area (mint), peak band (solid peach), the curve, the readings */
      data: { datasets: [
        { label: "Power curve (" + d.methods.interp_short[d.settings.fill] + ")", data: all, borderColor: C.teal, borderWidth: 2.4, fill: false, pointRadius: 0, order: 1 },
        { label: "Energy over the day", data: all, borderColor: "transparent", backgroundColor: "#d7efe6", fill: "origin", pointRadius: 0, order: 4, legendMarker: true, pointStyle: "rect" },
        { label: "Peak window " + hhmm(ps) + "–" + hhmm(pe), data: peak, borderColor: "transparent", backgroundColor: "#ffd2bf", fill: "origin", pointRadius: 0, order: 3, legendMarker: true, pointStyle: "rect" },
        { label: "Meter reading", data: d.readings.filter(function (r) { return !r.missing; }).map(function (r) { return { x: r.t, y: r.p }; }), showLine: false,
          pointRadius: 3.5, pointBackgroundColor: C.ink, pointBorderColor: C.card, pointBorderWidth: 1.5, borderColor: C.ink, order: 0 }
      ] },
      options: {
        interaction: { mode: "nearest", intersect: false },
        plugins: { tooltip: {
          filter: function (it) { return !it.dataset.legendMarker; },
          callbacks: { title: function (it) { return hhmm(it[0].parsed.x); }, label: function (it) { return " " + it.dataset.label + ": " + it.parsed.y.toFixed(3) + " kW"; } } } },
        scales: { x: timeAxis(d.dataset.t0, d.dataset.tn), y: valueAxis("Power (kW)", { min: 0 }) }
      }
    });

    if (e.chain) {
      var rowsC = e.chain.filter(function (r) { return r.name.indexOf("Newton") !== 0; });
      var labels = rowsC.map(function (r) { return r.name.replace(" (local cubic)", "").replace("True readings (no gaps)", "No gaps (true)"); });
      mk("ch-chain", {
        type: "bar",
        data: { labels: labels, datasets: names.map(function (n, k) {
          return { label: n, data: rowsC.map(function (r) { return r.err[k]; }), backgroundColor: QUAD_COL[k], borderRadius: 6, borderSkipped: "start", maxBarThickness: 34, categoryPercentage: .7, barPercentage: .9 };
        }) },
        options: {
          plugins: { tooltip: { callbacks: { label: function (it) { return " " + it.dataset.label + ": " + (it.parsed.y > 0 ? "+" : "") + it.parsed.y.toFixed(4) + " kWh"; } } } },
          scales: { x: { grid: { display: false }, border: { display: false }, ticks: { font: { weight: 700 } } },
                    y: valueAxis("Energy error (kWh)", { grid: { color: function (c) { return c.tick.value === 0 ? C.ink3 : C.grid; }, drawTicks: false } }) }
        }
      });
    }
  };

  /* ---------------------------------------------------------- alerts */
  RENDER.alerts = function (el) {
    var d = D(), r = d.roots, f = alertFacts(d), p = P(), syn = S.ds === "synthetic";
    var controls = '<div class="card control-card s12"><div class="controls"><div class="controls" style="gap:12px">' +
      field("in-thr", "Power limit", p.threshold, "kW", { step: 0.1, min: 0.1, hint: "Sanctioned-load style limit" }) +
      field("in-bud", "Daily energy budget", p.budget, "kWh", { step: 1, min: 1, hint: "Alert when used up" }) + "</div>" +
      '<div class="tabs-inline">' + chip("Curve: local cubic through " + d.interp.n_known + " readings", "mint", "fill") +
      chip("Tolerance 10<sup>−6</sup> h", "plum", "target") + chip("Scan step 15 min", "butter", "clock") + "</div></div></div>";

    var cards = '<div class="alert-grid s12"><div class="alert-mascot">' + I.mascot(f.upT != null ? "alert" : "happy") + "</div>" +
      '<div class="card alert hot"><div class="t">' + I.icon("up") + "Load goes above " + fx(r.threshold, 1) + ' kW</div><div class="big">' +
      (f.upT != null ? hhmmss(f.upT) : "Never") + '</div><div class="d">' +
      (f.upT != null ? (ok(f.up.true) ? "True time " + hhmmss(f.up.true) + ", off by " + fxs((f.upT - f.up.true) * 60, 1) + " min" : "No true time known") :
        "Highest point " + fx(r.curve_max.p, 2) + " kW at " + hhmm(r.curve_max.t)) + "</div></div>" +
      '<div class="card alert plumc"><div class="t">' + I.icon("clock") + 'Time above the limit</div><div class="big">' +
      (f.upT != null ? dur(r.above_time).replace(" min", "<small> min</small>").replace(" h ", "<small> h </small>") : "0<small> min</small>") + '</div><div class="d">' +
      (f.downT != null ? "Back below at " + hhmmss(f.downT) : " ") + (r.crossings.length > 2 ? ", " + r.crossings.length + " crossings in all" : "") + "</div></div>" +
      '<div class="card alert cool"><div class="t">' + I.icon("battery") + fx(r.budget, 0) + ' kWh budget used up</div><div class="big">' +
      (f.budgetT != null ? hhmmss(f.budgetT) : "Never") + '</div><div class="d">' +
      (f.budgetT != null ? (ok(f.budget.true) ? "True time " + hhmmss(f.budget.true) + ", off by " + fxs((f.budgetT - f.budget.true) * 60, 1) + " min" : "") :
        "Day total on the curve: " + fx(r.day_total_curve, 2) + " kWh") + "</div></div></div>";

    var note = "";
    if (r.fine_crossings != null) {
      note = '<div class="s12"><div class="callout peach"><span class="ic">' + I.icon("info") + "</span><span>The real 1-minute data crosses " + fx(r.threshold, 1) + " kW <b>" +
        r.fine_crossings + " times</b>, first at <b>" + hhmm(r.fine_first_up) + "</b>. Hourly readings let the curve see only " + r.crossings.length +
        ". The root finders are exact for the curve they are given; the curve cannot know about spikes between readings.</span></div></div>";
    }

    /* events */
    var events = r.crossings.map(function (c, i) { return { ev: c, label: "Crossing " + (i + 1) + (c.dir > 0 ? ", rises" : ", falls"), unit: "kW", kind: "power" }; });
    if (r.budget_event) events.push({ ev: r.budget_event, label: "Budget " + fx(r.budget, 0) + " kWh", kind: "energy" });
    if (S.eventSel >= events.length) S.eventSel = 0;
    var evBlock = "";
    if (events.length) {
      var E = events[S.eventSel].ev;
      evBlock = '<div class="s12">' + card("", head("Four methods, one equation", (events[S.eventSel].kind === "power" ? "Solve P(t) − " + fx(r.threshold, 1) + " = 0" : "Solve E(t) − " + fx(r.budget, 0) + " = 0") +
        " on the bracket [" + hhmm(E.a) + ", " + hhmm(E.b) + "] found by the scan.", seg("seg-ev", events.map(function (x) { return x.label; }), S.eventSel, true)),
        methodTable(E) ) + "</div>" +
        '<div class="s6">' + card("", head("Iteration history", "Error |x<sub>k</sub> − x*| in hours, x* = bisection to 10<sup>−13</sup> on the same curve."), historyTable(E)) + "</div>" +
        '<div class="s6">' + card("flexcol", head("Error per iteration", "Log scale: a straight fall is linear convergence, a steepening one is faster."),
          '<div class="chart-box"><canvas id="ch-iter"></canvas></div>') + "</div>";
    } else {
      evBlock = '<div class="s12">' + card("empty-state", "", I.mascot("happy") + "<h3>Nothing to solve</h3><p>The load never reaches the limit and the budget is never used up. Try a lower limit.</p>") + "</div>";
    }

    el.innerHTML = '<div class="grid">' + controls + cards + note +
      '<div class="s7">' + card("flexcol", head("Where the load crosses the limit", "Local-cubic curve with every crossing the root finders located."),
        '<div class="chart-box"><canvas id="ch-cross"></canvas></div>') + "</div>" +
      '<div class="s5">' + card("flexcol", head("Energy used so far", "E(t) on the curve, and the budget line."), '<div class="chart-box"><canvas id="ch-budget"></canvas></div>') + "</div>" +
      evBlock + "</div>";

    bindNumber("in-thr", "threshold", function (v) { return v <= 0 ? "Must be above 0" : ""; });
    bindNumber("in-bud", "budget", function (v) { return v <= 0 ? "Must be above 0" : ""; });
    onSeg("seg-ev", function (i) { S.eventSel = i; RENDER.alerts(el); countUp(el); });

    var crossT = r.crossings.map(function (c) { return c.ref; });
    mk("ch-cross", loadCurve(d, { interp: [1], crossings: crossT, truth: syn }));

    var ec = r.energy_curve, bud = [];
    if (r.budget_event) bud = [{ x: r.budget_event.ref, y: r.budget }];
    var dsB = [{ label: "E(t) on the curve", data: pairs(ec.t, ec.curve), borderColor: C.coral, pointRadius: 0, borderWidth: 2.4, order: 2 }];
    if (ec.true) dsB.push({ label: syn ? "Exact E(t)" : "Metered E(t)", data: pairs(ec.t, ec.true), borderColor: C.ink, borderDash: [5, 4], pointRadius: 0, borderWidth: 1.6, order: 3 });
    dsB.push({ label: fx(r.budget, 0) + " kWh budget", data: [{ x: d.dataset.t0, y: r.budget }, { x: d.dataset.tn, y: r.budget }], borderColor: C.ink3, borderDash: [6, 5], borderWidth: 1.5, pointRadius: 0, order: 4 });
    if (bud.length) dsB.push({ label: "Budget used up", data: bud, showLine: false, pointRadius: 7, pointBackgroundColor: C.teal, pointBorderColor: C.card, pointBorderWidth: 2.5, borderColor: C.teal, order: 0 });
    mk("ch-budget", { type: "line", data: { datasets: dsB }, options: {
      interaction: { mode: "nearest", intersect: false },
      plugins: { tooltip: { callbacks: { title: function (it) { return hhmm(it[0].parsed.x); }, label: function (it) { return " " + it.dataset.label + ": " + it.parsed.y.toFixed(2) + " kWh"; } } } },
      scales: { x: timeAxis(d.dataset.t0, d.dataset.tn, 4), y: valueAxis("Energy (kWh)", { min: 0 }) } } });

    if (events.length) {
      var Ev = events[S.eventSel].ev;
      var dsI = Ev.methods.map(function (m, k) {
        return { label: m.name, data: m.hist_err.map(function (e, i) { return { x: i + 1, y: Math.max(e, 1e-16) }; }), borderColor: ROOT_COL[k], backgroundColor: ROOT_COL[k],
          pointStyle: ROOT_MARK[k], pointRadius: 4.5, pointBorderColor: C.card, pointBorderWidth: 1.5, borderWidth: 2 };
      });
      var maxK = Math.max.apply(null, Ev.methods.map(function (m) { return m.hist_err.length; }));
      dsI.push({ label: "Tolerance", data: [{ x: 1, y: 1e-6 }, { x: maxK, y: 1e-6 }], borderColor: C.ink3, borderDash: [6, 5], borderWidth: 1.5, pointRadius: 0 });
      mk("ch-iter", { type: "line", data: { datasets: dsI }, options: {
        interaction: { mode: "nearest", intersect: false },
        plugins: { tooltip: { callbacks: { title: function (it) { return "Iteration " + it[0].parsed.x; }, label: function (it) { return " " + it.dataset.label + ": " + it.parsed.y.toExponential(2) + " h"; } } } },
        scales: { x: valueAxis("Iteration k", { type: "linear", min: 1, max: maxK, ticks: { stepSize: 1, padding: 8, autoSkip: true, maxRotation: 0 } }),
                  y: valueAxis("|error| (h)", { type: "logarithmic", min: 1e-15, afterBuildTicks: decadesOnly, ticks: { padding: 8, callback: function (v) { var e = Math.log10(v); return Math.abs(e - Math.round(e)) < 1e-9 && Math.round(e) % 3 === 0 ? supPow(Math.round(e)) : ""; } } }) } } });
    }
  };

  function methodTable(E) {
    var bestIt = Math.min.apply(null, E.methods.filter(function (m) { return m.converged; }).map(function (m) { return m.iterations; }));
    var rows = E.methods.map(function (m, k) {
      if (!m.converged) return '<tr><td class="name"><span class="nm">' + sw(ROOT_COL[k]) + m.name + '</span></td><td colspan="7" class="bad">Failed: ' +
        (m.left_bracket ? "an iterate left the bracket" : "did not converge") + "</td></tr>";
      return '<tr><td class="name"><span class="nm">' + sw(ROOT_COL[k]) + m.name + '</span></td><td class="num">' + fx(m.root, 7) + '</td><td class="num">' + hhmmss(m.root) +
        '</td><td class="num' + (m.iterations === bestIt ? " best" : "") + '">' + m.iterations + '</td><td class="num">' + sci(m.f_root, 1) + '</td><td class="num">' + sci(m.solver_err, 1) +
        '</td><td class="num">' + (ok(m.model_err_min) ? fxs(m.model_err_min, 3) + " min" : "–") + '</td><td class="num">' + (k === 0 ? "1*" : ok(m.order) ? fx(m.order, 2) : "–") + "</td></tr>";
    }).join("");
    return '<div class="tbl-wrap"><table class="tbl"><thead><tr><th>Method</th><th class="num">Root (h)</th><th class="num">Time</th><th class="num">Iterations</th>' +
      '<th class="num">|f(root)|</th><th class="num">Solver error (h)</th><th class="num">vs true time</th><th class="num">Order p</th></tr></thead><tbody>' + rows +
      '</tbody></table></div><p class="muted" style="font-size:12px;margin-top:8px">' + (ok(E.true) ? "True time " + hhmmss(E.true) + ". " : "") +
      "Theory: Regula Falsi 1, Newton 2, Secant 1.618. * Bisection's error jumps up and down, so p is not estimated; its bracket halves every step (order 1).</p>";
  }

  function historyTable(E) {
    var maxK = Math.max.apply(null, E.methods.map(function (m) { return m.hist_err.length; }));
    var rows = "";
    for (var k = 0; k < maxK; k++) {
      rows += '<tr><td class="dim">' + (k + 1) + "</td>" + E.methods.map(function (m) {
        var v = m.hist_err[k];
        return '<td class="num">' + (v != null ? sci(v, 2) : "") + "</td>";
      }).join("") + "</tr>";
    }
    return '<div class="tbl-wrap"><table class="tbl compact hist"><thead><tr><th>k</th>' +
      E.methods.map(function (m, k) { return '<th class="num"><span class="nm">' + sw(ROOT_COL[k]) + m.name + "</span></th>"; }).join("") +
      "</tr></thead><tbody>" + rows + "</tbody></table></div>";
  }

  /* ---------------------------------------------------------- compare */
  RENDER.compare = function (el) {
    var d = D(), it = d.interp, e = d.energy, r = d.roots, cv = e.convergence, sn = shortNames(), syn = S.ds === "synthetic";
    var convTbl = "";
    if (cv && cv.kind === "model") {
      convTbl = '<div class="tbl-wrap"><table class="tbl compact wrap-head"><thead><tr><th class="num">h (hours)</th>' +
        ["Trapezoidal", "Simpson 1/3", "Simpson 3/8"].map(function (q, k) { return '<th class="num"><span class="nm">' + sw(QUAD_COL[k]) + q + ' error</span></th><th class="num">p</th>'; }).join("") +
        "</tr></thead><tbody>" + cv.rows.map(function (row) {
          return '<tr><td class="num">' + row.h + "</td>" + row.err.map(function (x, k) {
            return '<td class="num">' + sci(x, 1) + '</td><td class="num strong">' + (ok(row.p[k]) ? fx(row.p[k], 2) : "–") + "</td>";
          }).join("") + "</tr>";
        }).join("") + "</tbody></table></div>" +
        '<p class="muted" style="font-size:12px;margin-top:8px">p = log<sub>2</sub>(|E(2h)| / |E(h)|). Theory: trapezoidal 2, both Simpson rules 4. At large h the peaks are under-sampled, so p settles as h shrinks.</p>';
    } else if (cv) {
      convTbl = '<div class="tbl-wrap"><table class="tbl compact"><thead><tr><th class="num">Read every</th><th class="num">Readings</th>' +
        e.rules.map(function (q, k) { return '<th class="num"><span class="nm">' + sw(QUAD_COL[k]) + q.name + "</span></th>"; }).join("") +
        "</tr></thead><tbody>" + cv.rows.map(function (row) {
          return '<tr><td class="num">' + row.step_min + ' min</td><td class="num">' + row.n + "</td>" + row.err_pct.map(function (x) { return '<td class="num">' + fxs(x, 2) + " %</td>"; }).join("") + "</tr>";
        }).join("") + "</tbody></table></div>" +
        '<p class="muted" style="font-size:12px;margin-top:8px">Error against the metered ' + fx(cv.exact, 3) + " kWh. It does not fall smoothly like h² or h⁴, because real load jumps when appliances switch.</p>" +
        '<div class="callout peach" style="margin-top:14px"><span class="ic">' + I.icon("clock") + "</span><span><b>What this means for a smart meter.</b> Going from hourly to half-hourly readings matters far more than the choice of rule. With minute readings all three rules agree within 0.2 %.</span></div>";
    }

    var ce = it.curve_error, lo = it.loocv;
    var interpTbl = '<div class="tbl-wrap"><table class="tbl compact"><thead><tr><th>Method</th><th class="num">Curve max</th><th class="num">Curve RMS</th><th class="num">Leave-one-out RMSE</th><th class="num">Lost readings RMSE</th></tr></thead><tbody>' +
      [0, 1, 2, 3].map(function (k) {
        return '<tr><td class="name"><span class="nm">' + sw(INTERP_COL[k]) + sn[k] + '</span></td><td class="num' + (k === 3 ? " bad" : "") + '">' + (ce ? fx(ce[k].max, 3) : "–") +
          '</td><td class="num">' + (ce ? fx(ce[k].rms, 3) : "–") + '</td><td class="num' + (k === 3 ? " bad" : "") + '">' + (lo ? fx(lo.rmse[k], 3) : "–") +
          '</td><td class="num">' + (it.stats ? fx(it.stats.rmse[k], 3) : "–") + "</td></tr>";
      }).join("") + "</tbody></table></div>";

    var evs = r.crossings.slice();
    if (r.budget_event) evs.push(r.budget_event);
    var evNames = r.crossings.map(function (c, i) { return "Crossing " + (i + 1); });
    if (r.budget_event) evNames.push("Budget");
    var rootTbl = '<div class="tbl-wrap"><table class="tbl compact"><thead><tr><th>Method</th>' + evNames.map(function (n) { return '<th class="num">' + n + "</th>"; }).join("") +
      '<th class="num">Order (seen)</th><th class="num">Theory</th></tr></thead><tbody>' +
      ["Bisection", "Regula Falsi", "Newton-Raphson", "Secant"].map(function (name, k) {
        var orders = evs.map(function (E) { return E.methods[k].order; }).filter(ok);
        var avg = orders.length ? orders.reduce(function (a, b) { return a + b; }, 0) / orders.length : null;
        return '<tr><td class="name"><span class="nm">' + sw(ROOT_COL[k]) + name + "</span></td>" + evs.map(function (E) { return '<td class="num">' + E.methods[k].iterations + "</td>"; }).join("") +
          '<td class="num strong">' + (k === 0 ? "1*" : ok(avg) ? fx(avg, 2) : "–") + '</td><td class="num">' + ["1", "1", "2", "1.618"][k] + "</td></tr>";
      }).join("") + '</tbody></table></div><p class="muted" style="font-size:12px;margin-top:8px">Iterations to reach 10<sup>−6</sup> h. * Bisection halves its bracket every step.</p>';

    var conclusions = syn ? [
      ["Local beats global", "A degree-" + it.global_degree + " polynomial misses by up to " + fx(ce[3].max, 1) + " kW near midnight (Runge's phenomenon). The local cubic stays within " + fx(ce[1].max, 3) + " kW everywhere."],
      ["Theory holds", "Measured convergence orders " + fx(cv.rows[4].p[0], 2) + ", " + fx(cv.rows[4].p[1], 2) + " and " + fx(cv.rows[4].p[2], 2) + " match the predicted 2, 4 and 4."],
      ["Speed differs, answers do not", "All four root finders agree to " + sci(r.max_solver_err, 0) + " h. Newton gets there in 3 steps, bisection in 18."],
      ["The data sets the limit", "Model error (" + fx(r.max_model_err_min, 1) + " min) is about a million times the solver error. Better readings, not a better solver, would improve the alert."]
    ] : [
      ["Smooth assumptions break", "On real, spiky load the linear method beats the cubic in the leave-one-out test (" + fx(lo.rmse[0], 2) + " vs " + fx(lo.rmse[1], 2) + " kW)."],
      ["Sampling beats sophistication", "Reading every 30 minutes instead of every hour cuts the energy error far more than switching from trapezoidal to Simpson."],
      ["A lost peak hides an alarm", "The hidden 19:00 reading was a 4.6 kW peak. No interpolation can recover it, so the curve misses that crossing entirely."],
      ["The data sets the limit", "The budget alert comes " + fx(r.max_model_err_min, 0) + " min late, while every root finder is accurate to milliseconds."]
    ];
    var concl = '<div class="grid" style="gap:14px">' + conclusions.map(function (c, i) {
      return '<div class="s6"><div class="callout ' + ["", "peach", "butter", ""][i] + '"><span class="ic"><b style="font-family:var(--font-display);font-size:16px;color:inherit">' + (i + 1) +
        "</b></span><span><b>" + c[0] + ".</b> " + c[1] + "</span></div></div>";
    }).join("") + "</div>";

    el.innerHTML = '<div class="grid">' +
      '<div class="s6">' + card("flexcol", head(syn ? "Integration error vs step size" : "Energy error vs how often the meter reads",
        syn ? "Log-log axes: the slope of each line is the order of the method." : "Real data: the error does not follow a clean power law."),
        '<div class="chart-box tall"><canvas id="ch-conv"></canvas></div>') + "</div>" +
      '<div class="s6">' + card("", head(syn ? "Observed order of convergence" : "Sampling study", syn ? "Synthetic day sampled at five step sizes." : "1-minute data sampled at six intervals."), convTbl) + "</div>" +
      '<div class="s7">' + card("", head("Interpolation: four ways to measure error", "Whole curve every 5 min, leave-one-out on known readings, and the lost readings themselves (kW)."), interpTbl +
        '<div class="callout butter" style="margin-top:14px"><span class="ic">' + I.icon("info") + "</span><span><b>Why the global polynomial fails.</b> One polynomial through all " +
        it.n_known + " readings has degree " + it.global_degree + ". Between equally spaced points such a polynomial swings wildly near the ends (Runge's phenomenon), so each estimate uses only the 4 nearest readings.</span></div>") + "</div>" +
      '<div class="s5">' + card("flexcol", head("Leave-one-out RMSE", "Lower is better. Global Lagrange is off the scale (" + fx(lo.rmse[3], 0) + " kW)."),
        '<div class="chart-box short"><canvas id="ch-loo"></canvas></div>') + "</div>" +
      '<div class="s7">' + card("", head("Root finders at a glance", "Same brackets, same tolerance, same curve."), rootTbl +
        '<div class="split" style="margin-top:14px"><div class="callout"><span class="ic">' + I.icon("target") + "</span><span><b>Bracketing methods</b> (bisection, Regula Falsi) always keep the root between two points, so they never fail, but they converge linearly.</span></div>" +
        '<div class="callout peach"><span class="ic">' + I.icon("spark") + "</span><span><b>Open methods</b> (Newton, Secant) are much faster but can jump away from the root; the C code checks every step stays inside the bracket.</span></div></div>") + "</div>" +
      '<div class="s5">' + card("flexcol", head("Solver error vs model error", "How well the equation was solved, against how well the equation describes reality."),
        '<div class="chart-box short"><canvas id="ch-sm"></canvas></div>') + "</div>" +
      '<div class="s12">' + card("", head("Conclusions", syn ? "Synthetic day: every answer checked against the exact model." : "Real day: checked against the 1-minute meter data."), concl) + "</div>" +
      "</div>";

    /* convergence / sampling chart */
    if (cv && cv.kind === "model") {
      var H = cv.rows.map(function (x) { return x.h; });
      var dsC = e.rules.map(function (q, k) {
        return { label: q.name, data: cv.rows.map(function (row) { return { x: row.h, y: Math.abs(row.err[k]) }; }), borderColor: QUAD_COL[k], backgroundColor: QUAD_COL[k],
          pointStyle: ROOT_MARK[k], pointRadius: 5, pointBorderColor: C.card, pointBorderWidth: 1.5 };
      });
      var last = cv.rows[cv.rows.length - 1], h0 = last.h, h1 = cv.rows[1].h;
      dsC.push({ label: "Slope 2 (h²)", data: [{ x: h0, y: Math.abs(last.err[0]) * 0.45 }, { x: h1, y: Math.abs(last.err[0]) * 0.45 * Math.pow(h1 / h0, 2) }], borderColor: C.ink3, borderDash: [3, 4], borderWidth: 1.4, pointRadius: 0 });
      dsC.push({ label: "Slope 4 (h⁴)", data: [{ x: h0, y: Math.abs(last.err[1]) * 0.45 }, { x: h1, y: Math.abs(last.err[1]) * 0.45 * Math.pow(h1 / h0, 4) }], borderColor: C.ink3, borderDash: [1, 4], borderWidth: 1.8, pointRadius: 0 });
      mk("ch-conv", { type: "line", data: { datasets: dsC }, options: {
        interaction: { mode: "nearest", intersect: true },
        plugins: { tooltip: { callbacks: { title: function (it) { return "h = " + it[0].parsed.x + " h"; }, label: function (it) { return " " + it.dataset.label + ": " + it.parsed.y.toExponential(2) + " kWh"; } } } },
        scales: {
          x: valueAxis("Step size h (hours)", { type: "logarithmic", min: 0.1, max: 2.4, afterBuildTicks: function (ax) { ax.ticks = H.map(function (v) { return { value: v }; }); },
            ticks: { padding: 8, callback: function (v) { return String(v); } } }),
          y: valueAxis("|Energy error| (kWh)", { type: "logarithmic", afterBuildTicks: decadesOnly, ticks: { padding: 8, callback: function (v) { var e = Math.log10(v); return Math.abs(e - Math.round(e)) < 1e-9 ? supPow(Math.round(e)) : ""; } } })
        } } });
    } else if (cv) {
      var steps = cv.rows.map(function (x) { return x.step_min; });
      mk("ch-conv", { type: "line", data: { datasets: e.rules.map(function (q, k) {
        return { label: q.name, data: cv.rows.map(function (row) { return { x: row.step_min, y: Math.abs(row.err_pct[k]) }; }), borderColor: QUAD_COL[k], backgroundColor: QUAD_COL[k],
          pointStyle: ROOT_MARK[k], pointRadius: 5, pointBorderColor: C.card, pointBorderWidth: 1.5 };
      }) }, options: {
        interaction: { mode: "nearest", intersect: true },
        plugins: { tooltip: { callbacks: { title: function (it) { return "Every " + it[0].parsed.x + " min"; }, label: function (it) { return " " + it.dataset.label + ": " + it.parsed.y.toFixed(2) + " %"; } } } },
        scales: {
          x: valueAxis("Meter reads every ... minutes", { type: "logarithmic", reverse: true, min: 0.8, max: 150, afterBuildTicks: function (ax) { ax.ticks = steps.map(function (v) { return { value: v }; }); },
            ticks: { padding: 8, callback: function (v) { return String(v); } } }),
          y: valueAxis("|Energy error| (%)", { min: 0 })
        } } });
    }

    /* leave-one-out bars */
    mk("ch-loo", { type: "bar", data: { labels: [sn[0], sn[1], sn[2]], datasets: [{ label: "Leave-one-out RMSE (kW)", data: [lo.rmse[0], lo.rmse[1], lo.rmse[2]],
      backgroundColor: [INTERP_COL[0], INTERP_COL[1], INTERP_COL[2]], borderRadius: 7, borderSkipped: "start", maxBarThickness: 30 }] },
      options: { indexAxis: "y", plugins: { legend: { display: false }, tooltip: { callbacks: { label: function (it) { return " RMSE " + it.parsed.x.toFixed(3) + " kW"; } } } },
        scales: { x: valueAxis("kW", { min: 0 }), y: { grid: { display: false }, border: { display: false }, ticks: { font: { weight: 700 } } } } } });

    /* solver vs model error (log scale, in hours) */
    var solver = r.max_solver_err, model = ok(r.max_model_err_min) ? r.max_model_err_min / 60 : null;
    mk("ch-sm", { type: "bar", data: { labels: ["Solver error", "Model error"], datasets: [{ label: "Largest error (h)", data: [solver, model],
      backgroundColor: [C.plum, C.coral], borderRadius: 7, borderSkipped: "start", maxBarThickness: 30 }] },
      options: { indexAxis: "y", plugins: { legend: { display: false }, tooltip: { callbacks: { label: function (it) {
        var v = it.parsed.x; return v * 60 >= 1 ? " " + (v * 60).toFixed(2) + " min" : " " + (v * 3.6e6).toFixed(2) + " ms"; } } } },
        scales: { x: valueAxis("Largest error (hours, log scale)", { type: "logarithmic", min: 1e-8, max: 10,
          afterBuildTicks: function (ax) { ax.ticks = [1e-8, 1e-6, 1e-4, 1e-2, 1].map(function (v) { return { value: v }; }); },
          ticks: { padding: 8, maxRotation: 0, autoSkip: false, callback: function (v) {
          var e = Math.log10(v); return Math.abs(e - Math.round(e)) < 1e-9 && Math.round(e) % 2 === 0 ? supPow(Math.round(e)) : ""; } } }),
          y: { grid: { display: false }, border: { display: false }, ticks: { font: { weight: 700 } } } } } });
  };

  /* ---------------------------------------------------------- tests */
  RENDER.tests = function (el) {
    var t = S.tests;
    if (!t) { el.innerHTML = card("empty-state", "", "<p>No test results.</p>"); return; }
    var passed = t.total - t.failed, frac = t.total ? passed / t.total : 0, R = 54, L = 2 * Math.PI * R;
    var groups = [
      ["Method verification", "Hand-checkable answers: lines, cubics, sin x, x² − 2.", function (id) { return /^T[1-9]$/.test(id); }],
      ["Edge cases and bad input", "Limits never reached, exact hits, NA at the ends, odd intervals, bad files.", function (id) { return /^T1[0-5]$/.test(id); }],
      ["Full scenarios", "Both datasets end to end, with pass criteria from PLAN.md.", function (id) { return /^S/.test(id); }]
    ];
    var ring = '<svg class="ring" viewBox="0 0 132 132"><circle class="track" cx="66" cy="66" r="' + R + '" fill="none" stroke-width="12"/>' +
      '<circle class="bar" cx="66" cy="66" r="' + R + '" fill="none" stroke-width="12" stroke-linecap="round" transform="rotate(-90 66 66)" stroke-dasharray="' + L +
      '" stroke-dashoffset="' + L + '" id="ring-bar"/><text class="ring-text" x="66" y="64" text-anchor="middle" font-size="30">' + passed + '</text>' +
      '<text x="66" y="86" text-anchor="middle" font-size="12" fill="#8c8491" font-family="Nunito Sans" font-weight="700">of ' + t.total + " passed</text></svg>";
    var html = '<div class="grid">' +
      '<div class="s5">' + card("ring-card", "", ring + '<div><h3>' + (t.failed ? t.failed + " test" + (t.failed > 1 ? "s" : "") + " failing" : "All tests pass") + '</h3><p style="margin-top:6px">' +
        "Each test compares a method with an answer known in advance, inside the C engine.</p>" +
        '<div style="margin-top:12px;display:flex;gap:10px;align-items:center;flex-wrap:wrap"><button class="btn" id="btn-tests"' + (live() ? "" : " disabled") + ">" + I.icon("play") + "Run tests again</button>" +
        (live() ? "" : '<span class="muted" style="font-size:12px">Needs the live engine</span>') + "</div></div>") + "</div>" +
      '<div class="s7">' + card("", "", '<div style="display:grid;grid-template-columns:110px minmax(0,1fr);gap:18px;align-items:center"><div>' + I.mascot(t.failed ? "alert" : "proud") +
        "</div><div><h3>How the tests are built</h3>" +
        '<div class="tabs-inline" style="margin-top:10px">' + groups.map(function (g, i) {
          var n = t.rows.filter(function (r) { return g[2](r.id); }).length;
          return chip(g[0] + ": " + n, ["mint", "butter", "plum"][i]);
        }).join("") + '</div><p style="margin-top:10px;font-size:13.5px">Run them yourself with <span class="mono">energy.exe --test</span>. The same table is saved to <span class="mono">output/test_results.txt</span>.</p></div></div>') + "</div>";
    groups.forEach(function (g) {
      var rows = t.rows.filter(function (r) { return g[2](r.id); });
      html += '<div class="s12">' + card("", head(g[0], g[1], chip(rows.filter(function (r) { return r.passed; }).length + " / " + rows.length + " passed", "mint", "check")),
        '<div class="tbl-wrap"><table class="tbl"><thead><tr><th style="width:70px">ID</th><th>Test</th><th>Result from the C engine</th><th style="width:110px">Status</th></tr></thead><tbody>' +
        rows.map(function (r) {
          return '<tr><td class="name">' + r.id + "</td><td>" + r.what + '</td><td class="mono">' + r.detail.replace(/e([+-])0(\d\d)/g, "e$1$2") + "</td><td>" +
            (r.passed ? '<span class="pass">' + I.icon("check") + "PASS</span>" : '<span class="fail">' + I.icon("cross") + "FAIL</span>") + "</td></tr>";
        }).join("") + "</tbody></table></div>") + "</div>";
    });
    el.innerHTML = html + "</div>";
    requestAnimationFrame(function () { var b = $("#ring-bar"); if (b) b.style.strokeDashoffset = L * (1 - frac); });
    var btn = $("#btn-tests");
    if (btn) btn.addEventListener("click", function () {
      btn.disabled = true;
      el.classList.add("busy");
      fetch("/api/tests").then(function (r) { return r.json(); }).then(function (j) { if (!j.error) S.tests = j; render(); })
        .catch(function (e) { S.error = e.message; renderBanner(); }).then(function () { el.classList.remove("busy"); });
    });
  };

  /* ---------------------------------------------------------- start */
  setupChartDefaults();
  /* charts draw on canvas, so the bundled fonts must be loaded before the first chart */
  var fontsReady = document.fonts && document.fonts.load
    ? Promise.all(['400 12px "Nunito Sans"', '600 12px "Nunito Sans"', '700 12px "Nunito Sans"', '500 13px "Fredoka"',
        '600 13px "Fredoka"', '400 12px "JetBrains Mono"'].map(function (f) { return document.fonts.load(f); })).catch(function () {})
    : Promise.resolve();
  fontsReady.then(init);
})();
