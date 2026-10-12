/* icons.js - one consistent, hand-drawn icon set (24 x 24, 1.8 px rounded strokes)
   and the mascot: a small smart meter with a face and a plug-cord tail. */
(function () {
  "use strict";

  var P = {
    overview: '<rect x="3.5" y="3.5" width="7" height="7" rx="2.2"/><rect x="13.5" y="3.5" width="7" height="7" rx="2.2"/><rect x="3.5" y="13.5" width="7" height="7" rx="2.2"/><rect x="13.5" y="13.5" width="7" height="7" rx="2.2"/>',
    dataset: '<rect x="3.5" y="4.5" width="17" height="15" rx="3"/><path d="M3.5 9.5h17M9.5 9.5v10"/>',
    fill: '<path d="M3 17c2.2-.2 3.6-6.4 6.3-7.3"/><path d="M10.6 9.4c1.7.4 2.4 2.2 3.6 3.6" stroke-dasharray="1.2 2.4"/><path d="M15.3 14.2c1.8.9 3.3-.4 5.7-4.2"/><circle cx="12.3" cy="10.6" r="1.9"/>',
    energy: '<path d="M13.2 3 5.8 13.4h5.4l-1 7.6 7.5-10.6h-5.4l.9-7.4z"/>',
    alert: '<path d="M6 16.6v-5.4a6 6 0 0 1 12 0v5.4l1.6 1.6H4.4z"/><path d="M10 20.6a2.1 2.1 0 0 0 4 0"/>',
    compare: '<path d="M3.5 20h17"/><rect x="5" y="11" width="3.6" height="9" rx="1.2"/><rect x="10.2" y="5.5" width="3.6" height="14.5" rx="1.2"/><rect x="15.4" y="8.5" width="3.6" height="11.5" rx="1.2"/>',
    tests: '<circle cx="12" cy="12" r="8.6"/><path d="m8.4 12.3 2.5 2.5 4.9-5.2"/>',
    arrow: '<path d="M5 12h13M13 6.5l5.5 5.5-5.5 5.5"/>',
    info: '<circle cx="12" cy="12" r="8.6"/><path d="M12 11v5.2M12 7.8v.1"/>',
    clock: '<circle cx="12" cy="12" r="8.6"/><path d="M12 7.5V12l3 2"/>',
    up: '<path d="M4 17.5 10 11l3.5 3.5L20 8"/><path d="M15 8h5v5"/>',
    down: '<path d="M4 7l6 6.5 3.5-3.5L20 16.5"/><path d="M15 16.5h5v-5"/>',
    battery: '<rect x="3" y="7.5" width="15.5" height="9" rx="2.6"/><path d="M21 10.5v3"/><path d="M6.5 11h6.5M6.5 13.5h4"/>',
    coin: '<circle cx="12" cy="12" r="8.6"/><path d="M9 8.6h6M9 11.4h6M13.6 8.6c0 3.2-2.6 3-4.6 3l4.8 4.4"/>',
    check: '<path d="m5.5 12.5 4.2 4.2 8.8-9.4"/>',
    cross: '<path d="M7 7l10 10M17 7 7 17"/>',
    play: '<path d="M8 5.8v12.4a1 1 0 0 0 1.5.9l9.6-6.2a1 1 0 0 0 0-1.7L9.5 4.9A1 1 0 0 0 8 5.8z"/>',
    target: '<circle cx="12" cy="12" r="8.6"/><circle cx="12" cy="12" r="4.4"/><circle cx="12" cy="12" r=".6"/>',
    chip: '<rect x="6.5" y="6.5" width="11" height="11" rx="2.4"/><path d="M9.5 3.5v3M14.5 3.5v3M9.5 17.5v3M14.5 17.5v3M3.5 9.5h3M3.5 14.5h3M17.5 9.5h3M17.5 14.5h3"/>',
    wave: '<path d="M3 12h3l2.2-5 3.6 10 2.6-6.5 1.6 3.5H21"/>',
    area: '<path d="M3.5 19.5h17"/><path d="M4 18.5c2.4-.2 3.4-9 6.2-9s3.4 5.5 5.6 5.5 2.8-2.6 4.2-4.6v8.1z" fill="currentColor" fill-opacity=".14"/>',
    steps: '<path d="M4 18h4v-4h4v-4h4V6h4"/>',
    spark: '<path d="M12 3.5v4M12 16.5v4M3.5 12h4M16.5 12h4M6.3 6.3l2.6 2.6M15.1 15.1l2.6 2.6M6.3 17.7l2.6-2.6M15.1 8.9l2.6-2.6"/>',
    leaf: '<path d="M5 19c0-8 5.5-13.5 14-14 .5 8.5-5 14-13 14z"/><path d="M5 19 13 11"/>'
  };

  function icon(name, cls) {
    return '<svg class="' + (cls || "") + '" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.8" ' +
      'stroke-linecap="round" stroke-linejoin="round" aria-hidden="true">' + (P[name] || "") + "</svg>";
  }

  /* mood: "happy" (default), "alert" (a bit worried), "proud" (eyes closed, big smile) */
  function mascot(mood) {
    var ink = "#2f2a35";
    var eyes, mouth, brows = "";
    if (mood === "alert") {
      eyes = '<circle cx="47" cy="71" r="4" fill="' + ink + '"/><circle cx="73" cy="71" r="4" fill="' + ink + '"/>' +
             '<circle cx="48.4" cy="69.6" r="1.3" fill="#fff"/><circle cx="74.4" cy="69.6" r="1.3" fill="#fff"/>';
      brows = '<path d="M41 63.5l9-2.5M79 63.5l-9-2.5" stroke="' + ink + '" stroke-width="3" stroke-linecap="round"/>';
      mouth = '<ellipse cx="60" cy="82" rx="4.4" ry="3.6" fill="' + ink + '"/>';
    } else if (mood === "proud") {
      eyes = '<path d="M42.5 72q4.5-5 9 0M68.5 72q4.5-5 9 0" stroke="' + ink + '" stroke-width="3" fill="none" stroke-linecap="round"/>';
      mouth = '<path d="M51 78.5q9 10 18 0z" fill="' + ink + '" stroke="' + ink + '" stroke-width="2.4" stroke-linejoin="round"/>' +
              '<path d="M55.5 82.6q4.5 2.6 9 0" stroke="#ff9a7f" stroke-width="2.4" fill="none" stroke-linecap="round"/>';
    } else {
      eyes = '<circle cx="47" cy="71" r="3.8" fill="' + ink + '"/><circle cx="73" cy="71" r="3.8" fill="' + ink + '"/>' +
             '<circle cx="48.3" cy="69.7" r="1.2" fill="#fff"/><circle cx="74.3" cy="69.7" r="1.2" fill="#fff"/>';
      mouth = '<path d="M53 79q7 6.5 14 0" stroke="' + ink + '" stroke-width="3" fill="none" stroke-linecap="round"/>';
    }
    var screenLine = mood === "alert"
      ? '<path d="M39 49h8l3-4 4 0 3-9 3 17 3-11 3 7h10" stroke="#e2583a" stroke-width="3" fill="none" stroke-linecap="round" stroke-linejoin="round"/>'
      : '<path d="M39 48h7l4-8 6 13 5-9 4 4h8" stroke="#0a8f7c" stroke-width="3" fill="none" stroke-linecap="round" stroke-linejoin="round"/>';
    return '<svg viewBox="0 0 124 124" aria-hidden="true">' +
      /* cord and plug */
      '<path d="M60 97c0 12 20 15 33 9" stroke="' + ink + '" stroke-width="3" fill="none" stroke-linecap="round"/>' +
      '<rect x="90" y="98.5" width="17" height="14" rx="4.5" fill="#e2583a" stroke="' + ink + '" stroke-width="3"/>' +
      '<path d="M107 102.5h5.5M107 108.5h5.5" stroke="' + ink + '" stroke-width="3" stroke-linecap="round"/>' +
      /* little bolt antenna */
      '<path d="M60 19v-6" stroke="' + ink + '" stroke-width="3" stroke-linecap="round"/>' +
      '<circle cx="60" cy="10" r="5" fill="#f6c445" stroke="' + ink + '" stroke-width="3"/>' +
      /* body */
      '<rect x="21" y="19" width="78" height="80" rx="23" fill="#b4e2d1" stroke="' + ink + '" stroke-width="3"/>' +
      '<path d="M30 30q2-4 7-5" stroke="#fff" stroke-width="3" stroke-linecap="round" opacity=".75"/>' +
      /* screen */
      '<rect x="32" y="30" width="56" height="29" rx="9.5" fill="#fffaf3" stroke="' + ink + '" stroke-width="3"/>' +
      screenLine +
      /* face */
      brows + eyes +
      '<ellipse cx="38.5" cy="79.5" rx="5.2" ry="3.2" fill="#ffb59c"/><ellipse cx="81.5" cy="79.5" rx="5.2" ry="3.2" fill="#ffb59c"/>' +
      mouth +
      /* feet */
      '<path d="M41 99v4M79 99v4" stroke="' + ink + '" stroke-width="3" stroke-linecap="round"/>' +
      "</svg>";
  }

  window.UI_ICONS = { icon: icon, mascot: mascot };
})();
