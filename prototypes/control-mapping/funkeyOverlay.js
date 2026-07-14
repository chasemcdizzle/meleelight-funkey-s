/*eslint-disable */
// =============================================================================
// FunKey control-mapping debug overlay (wayfinder ticket #9).
// Shows: active scheme, live synthesized left-stick position on a unit square
// with meleelight's real threshold geometry (per-AXIS bands, not rings:
// deadzone 0.28, tilt floor 0.3, dash 0.79, u-smash/tap-jump 0.66, crouch
// -0.69, shield-drop band (-0.70,-0.65]), C-stick dot, held virtual buttons,
// and which modifier coordinate family produced the value.
// F9 toggles visibility, F8 cycles scheme (handled in funkeyMapping.js).
// =============================================================================

var SIZE = 200;      // plot pixels
var PAD = 10;
var HALF = SIZE / 2;

function toPx (v, flipY) { // melee unit -> canvas px (y up in melee)
  return PAD + HALF + (flipY ? -v : v) * HALF;
}

export function startOverlay (state, isWanted) {
  if (typeof document === "undefined") { return; }

  var box = document.createElement("div");
  box.id = "funkeyOverlay";
  box.style.cssText = "position:fixed;top:8px;right:8px;z-index:99999;" +
    "background:rgba(10,12,18,0.88);border:1px solid #3a4a5a;border-radius:6px;" +
    "padding:6px;font:11px/1.35 monospace;color:#cfe0ee;pointer-events:none;" +
    "width:" + (SIZE + 2 * PAD + 12) + "px";

  var canvas = document.createElement("canvas");
  canvas.width = SIZE + 2 * PAD;
  canvas.height = SIZE + 2 * PAD;
  box.appendChild(canvas);

  var text = document.createElement("pre");
  text.style.cssText = "margin:4px 0 0 0;white-space:pre-wrap;font:inherit;color:inherit";
  box.appendChild(text);

  document.body.appendChild(box);
  var ctx = canvas.getContext("2d");

  function vline (x, color, dash) {
    ctx.strokeStyle = color;
    ctx.setLineDash(dash || []);
    ctx.beginPath();
    ctx.moveTo(toPx(x), PAD);
    ctx.lineTo(toPx(x), PAD + SIZE);
    ctx.stroke();
    ctx.setLineDash([]);
  }
  function hline (y, color, dash) {
    ctx.strokeStyle = color;
    ctx.setLineDash(dash || []);
    ctx.beginPath();
    ctx.moveTo(PAD, toPx(y, true));
    ctx.lineTo(PAD + SIZE, toPx(y, true));
    ctx.stroke();
    ctx.setLineDash([]);
  }

  function draw () {
    ctx.clearRect(0, 0, canvas.width, canvas.height);
    ctx.lineWidth = 1;

    // deadzone bands (|axis| < 0.28 is zeroed per axis)
    ctx.fillStyle = "rgba(120,120,120,0.18)";
    ctx.fillRect(toPx(-0.28), PAD, 0.56 * HALF, SIZE);
    ctx.fillRect(PAD, toPx(0.28, true), SIZE, 0.56 * HALF);

    // shield-drop band lsY in (-0.70, -0.65]
    ctx.fillStyle = "rgba(170,90,255,0.25)";
    ctx.fillRect(PAD, toPx(-0.65, true), SIZE, 0.05 * HALF);

    // unit square + circle
    ctx.strokeStyle = "#55606c";
    ctx.strokeRect(PAD, PAD, SIZE, SIZE);
    ctx.beginPath();
    ctx.arc(PAD + HALF, PAD + HALF, HALF, 0, 2 * Math.PI);
    ctx.stroke();

    // tilt floor 0.3 (teal), dash 0.79 (red), usmash/tapjump 0.66 (orange),
    // crouch -0.69 (dashed)
    vline( 0.3, "#2fbfae"); vline(-0.3, "#2fbfae");
    hline( 0.3, "#2fbfae"); hline(-0.3, "#2fbfae");
    vline( 0.79, "#e05555"); vline(-0.79, "#e05555");
    hline( 0.66, "#e0a030");
    hline(-0.69, "#e05555", [3, 3]);

    var inp = state.input;
    if (inp) {
      // C-stick dot (orange) when deflected
      if (inp.csX !== 0 || inp.csY !== 0) {
        ctx.fillStyle = "#ffb347";
        ctx.beginPath();
        ctx.arc(toPx(inp.csX), toPx(inp.csY, true), 4, 0, 2 * Math.PI);
        ctx.fill();
      }
      // left-stick dot (green)
      ctx.fillStyle = "#5cff7a";
      ctx.beginPath();
      ctx.arc(toPx(inp.lsX), toPx(inp.lsY, true), 5, 0, 2 * Math.PI);
      ctx.fill();
    }

    // ------- text block -------
    var b = state.buttons || {};
    var held = [];
    var order = ["DU", "DD", "DL", "DR", "A", "B", "X", "Y", "L", "R", "START", "FN"];
    for (var i = 0; i < order.length; i++) {
      if (b[order[i]]) { held.push(order[i]); }
    }
    var f = state.flags || {};
    var badges = [];
    if (f.mod)    { badges.push("MOD"); }
    if (f.modx)   { badges.push("MODX"); }
    if (f.mody)   { badges.push("MODY"); }
    if (f.clayer) { badges.push("C-LAYER"); }
    if (f.shield) { badges.push("SHIELD"); }

    var lines = [
      state.schemeName + "  [F8 scheme / F9 hide]",
      "funkey: " + (held.length ? held.join(" ") : "-"),
      "mods:   " + (badges.length ? badges.join(" ") : "-"),
      "coord:  " + (state.family || "-")
    ];
    if (inp) {
      lines.push("ls(" + inp.lsX.toFixed(4) + "," + inp.lsY.toFixed(4) + ")" +
                 " cs(" + inp.csX.toFixed(4) + "," + inp.csY.toFixed(4) + ")");
      var db = [];
      if (inp.a) { db.push("a"); }
      if (inp.b) { db.push("b"); }
      if (inp.x) { db.push("x"); }
      if (inp.y) { db.push("y"); }
      if (inp.z) { db.push("z"); }
      if (inp.l) { db.push("l"); }
      if (inp.r) { db.push("r"); }
      if (inp.s) { db.push("s"); }
      lines.push("gc out: " + (db.length ? db.join(" ") : "-") +
                 (inp.rA ? "  rA=" + inp.rA : "") + (inp.lA ? "  lA=" + inp.lA : ""));
    }
    text.textContent = lines.join("\n");

    box.style.display = isWanted() ? "block" : "none";
    window.requestAnimationFrame(draw);
  }
  window.requestAnimationFrame(draw);
}
