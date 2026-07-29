// port/gfx/gfx-pagelib.js — page-side helpers for the render-reference
// capture (M3 task 3; EXTENDED M4 task 2). Injected AFTER
// oracle/harness/{init,pagelib}.js (both used VERBATIM — oracle/ is never
// modified) by capture-canvas.js.
//
// Provides:
//   __gfxFindModules(): locate the render-plane modules in the webpack
//     module cache (window.__wpCache, exposed by the served-bytes boot
//     hook — the run-capture.js mechanism class). Unique-match hard-fail.
//   __gfxDumpData(): the GFXDATA1 text (palettes / pPal / flashOnLCancel
//     / reverseModel flags) EXECUTED out of the live page — the C
//     renderer's colour/flag data plane, never hand-typed.
//   __gfxDumpVfxData(): the VFXDATA1 text — the POST-ALIAS vfx[name]
//     template table (authored path arrays / frames / colours; vfx.js
//     gives wallBounce/ceilingBounce groundBounce's planes) + the
//     swordSwings table, EXECUTED out of the live page (M4 task 2).
//   __gfxDumpGlyphs(): the VFXGLYPHS1 text — the browser's OWN Arial
//     rasterization of every HUD/banner glyph (fill+stroke alpha masks,
//     5x5 area-averaged to device scale, advances) + the static
//     Ready/Go! composite sprites (M4 task 2).
//   __gfxRunChunk(n, sampled, inject): steps n frames through the
//     UNCHANGED pagelib __runFrames (1 frame at a time, so the checksum
//     stream is the harness's own), runs the render sequence after every
//     step (injecting the synthetic-coverage configs on the pinned frame
//     first), and captures the fg1|fg2|UI ink mask (+ a composite PNG
//     for humans) on sampled frames.
//
// Render sequence = the FULL gameMode-3 renderTick branch
// (main.js:1243-1261), drawBackground INCLUDED (U1):
//   clearScreen -> drawBackground (isShowSFX gate, upstream's own) ->
//   drawStage -> renderPlayer x4 -> renderArticles -> renderVfx() ->
//   renderOverlay(true)
//
// U1 — why drawBackground used to be excluded, and what makes it
// includable now. drawStars consumes Math.random EVERY frame (18 draws
// for the two mountains' control points, +3 per star respawn), so under
// the native-RNG swap below the browser walks a random trajectory the C
// cannot follow: pairing the BG2 plane frame-by-frame was impossible,
// not merely unjudged. The fix is the project's own sweep discipline
// (fix_plan §M2 rules 11/12): give BOTH sides the SAME stream and the
// SAME start state.
//   - stream: a mulberry32 seeded with gfx_bg.c's own render-local
//     constant, installed as Math.random for the duration of
//     drawBackground ONLY (the rest of the render keeps the native swap,
//     so nothing else changes);
//   - start state: bgPos/direction/circleSize/bgSparkle/ang are still at
//     their stagerender.js module literals when the capture begins
//     (renderTick is disabled via __harnessNoRender, so drawBackground
//     has never run) — asserted, not assumed, by __gfxBgInit. bgStars is
//     the one piece that is NOT (its 20 stars are built at bundle-eval
//     time from the seeded BOOT stream), so __gfxBgInit re-seeds it in
//     place by calling window.__bgReseed under the mirrored stream.
// gfx_bg_reset() performs exactly that same 6-draws-per-star sequence
// from the same seed, so the two starfields and both mountains then
// advance draw-for-draw together for the whole run.
//
// NO SELF-REFERENCE: every line the judge compares is upstream's own
// executed code. bgStar(), drawStars(), drawTunnel(), drawBackgroundInit()
// and the bezier fills run from the bundle; the star-initialisation loop
// (stagerender.js:16-19), which is a module-level statement rather than a
// callable, is SOURCE-TRANSFORMED into window.__bgReseed by copying its
// exact served bytes (capture-canvas.js reseedHook), never retyped.
// (review-u1 r1 M1 — an earlier version restated its two position
// expressions, which would have mirrored a matching error in gfx_bg.c.)
//
// Guards that keep the sim stream clean, PROVEN by the run's
// verify-stream STREAM MATCH:
//   - Math.random is swapped to the harness's stashed native RNG for the
//     duration of the render call (render-plane code — incl. the dVfx
//     re-spawns and the injected drawVfx calls — must not consume the
//     seeded gameplay stream);
//   - phys.outOfCameraTimer is snapshot/restored around it (renderPlayer
//     writes it and physics.js:660 FEEDS IT BACK into percent).
// renderVfx/renderOverlay mutate only render-module state (vfxQueue,
// lostStockQueue) that the sim never reads; dVfx.start's ready/go Howl
// plays advance only the howler id counter (not on the checksum surface).
(() => {
  "use strict";
  const mods = {};

  function findModule(what, pred) {
    const cache = window.__wpCache;
    if (!cache) throw new Error("gfx-capture: __wpCache hook missing");
    const hits = [];
    for (const id of Object.keys(cache)) {
      const m = cache[id];
      if (m && m.exports && pred(m.exports)) hits.push(m.exports);
    }
    if (hits.length !== 1) {
      throw new Error(`gfx-capture: module '${what}' matched ${hits.length} times (need exactly 1)`);
    }
    return hits[0];
  }

  window.__gfxFindModules = function () {
    mods.main = findModule("main/main", (e) =>
      typeof e.clearScreen === "function" && typeof e.startGame === "function" &&
      e.layers && e.palettes && e.pPal);
    mods.stage = findModule("stages/stagerender", (e) =>
      typeof e.drawStage === "function" && typeof e.drawStageInit === "function");
    mods.render = findModule("main/render", (e) =>
      typeof e.renderPlayer === "function" &&
      typeof e.renderOverlay === "function" &&
      typeof e.drawArrayPathCompress === "function");
    mods.article = findModule("physics/article", (e) =>
      typeof e.renderArticles === "function" && e.articles && e.aArticles !== undefined);
    mods.asshort = findModule("physics/actionStateShortcuts", (e) =>
      !!e.actionStates && typeof e.turnOffHitboxes === "function");
    mods.settings = findModule("settings", (e) =>
      e.gameSettings && "flashOnLCancel" in e.gameSettings);
    // M4 task 2: the vfx plane
    mods.vfx = findModule("main/vfx (aggregate)", (e) =>
      !!e.vfx && !!e.dVfx && typeof e.isShowSFX === "function");
    mods.renderVfx = findModule("main/vfx/renderVfx", (e) =>
      typeof e.renderVfx === "function" && Object.keys(e).length === 1);
    mods.drawVfx = findModule("main/vfx/drawVfx", (e) =>
      typeof e.drawVfx === "function" && Object.keys(e).length === 1);
    mods.swords = findModule("main/swordSwings", (e) =>
      !!e.swordSwings && !!e.swordSwings.FAIR);
    return Object.keys(mods).length;
  };

  // --- GFXDATA1 (executed render data plane) ------------------------------
  window.__gfxDumpData = function () {
    const lines = ["GFXDATA1"];
    lines.push("FLASHONLCANCEL " + (mods.settings.gameSettings.flashOnLCancel ? "1" : "0"));
    lines.push("PPAL " + mods.main.pPal.slice(0, 4).join(" "));
    const pal = mods.main.palettes;
    if (pal.length !== 7) throw new Error("gfx-capture: palettes length != 7");
    const re = /^rgba?\((\d+),\s*(\d+),\s*(\d+)/;
    for (let i = 0; i < pal.length; i++) {
      if (pal[i].length !== 5) throw new Error("gfx-capture: palette row length != 5");
      for (let j = 0; j < 5; j++) {
        const m = re.exec(pal[i][j]);
        if (!m) throw new Error("gfx-capture: unparseable palette entry " + pal[i][j]);
        lines.push(`PAL ${i} ${j} ${m[1]} ${m[2]} ${m[3]}`);
      }
    }
    const as = mods.asshort.actionStates;
    for (let c = 0; c < 5; c++) {
      if (!as[c]) throw new Error("gfx-capture: actionStates[" + c + "] missing");
      for (const state of Object.keys(as[c]).sort()) {
        if (as[c][state] && as[c][state].reverseModel) lines.push(`REV ${c} ${state}`);
      }
    }
    lines.push("END");
    return lines.join("\n") + "\n";
  };

  // --- VFXDATA1 (executed vfx template plane; M4 task 2) -------------------
  // Bracket-stream over nested numeric arrays (typed arrays included —
  // phantasm.path rows are Int16Arrays), numbers via String(x). Non-array
  // template values the draw code never indexes (string colours,
  // hitCurve's dead svg string) become documented SKIP lines; anything
  // else unexpected hard-throws.
  function bracketStream(v, where) {
    if (typeof v === "number") {
      if (!isFinite(v)) throw new Error("gfx-capture: non-finite template number at " + where);
      return String(v);
    }
    if (Array.isArray(v) || ArrayBuffer.isView(v)) {
      const parts = ["["];
      for (let i = 0; i < v.length; i++) parts.push(bracketStream(v[i], where + "[" + i + "]"));
      parts.push("]");
      return parts.join(" ");
    }
    throw new Error("gfx-capture: unsupported template value at " + where + ": " + typeof v);
  }

  window.__gfxDumpVfxData = function () {
    const vfx = mods.vfx.vfx;
    const lines = ["VFXDATA1"];
    for (const name of Object.keys(vfx).sort()) {
      const tpl = vfx[name];
      lines.push("TPL " + name);
      if (typeof tpl.frames !== "number") {
        throw new Error("gfx-capture: template without numeric frames: " + name);
      }
      lines.push("FRAMES " + String(tpl.frames));
      for (const key of Object.keys(tpl).sort()) {
        if (key === "name" || key === "frames") continue;
        const v = tpl[key];
        if (key === "colour") {
          if (Array.isArray(v)) {
            if (v.length !== 3) throw new Error("gfx-capture: colour arity at " + name);
            lines.push("COLOUR " + v.map(String).join(" "));
          } else if (typeof v === "string") {
            lines.push("SKIP " + name + " colour"); // never indexed by draw code
          } else {
            throw new Error("gfx-capture: unexpected colour type at " + name);
          }
          continue;
        }
        if (typeof v === "string") {
          if (name === "hitCurve" && key === "svg") {
            lines.push("SKIP " + name + " svg"); // dVfx/hitCurve.js is an empty TODO
            continue;
          }
          throw new Error("gfx-capture: unexpected string template key " + name + "." + key);
        }
        lines.push("KEY " + key + " " + bracketStream(v, name + "." + key));
      }
    }
    const swords = mods.swords.swordSwings;
    for (const type of Object.keys(swords).sort()) {
      lines.push("SWORD " + type + " " + bracketStream(swords[type], "swordSwings." + type));
    }
    lines.push("END");
    return lines.join("\n") + "\n";
  };

  // --- VFXGLYPHS1 (browser-rasterized text plane; M4 task 2) ---------------
  // Fonts in the exact upstream draw forms (render.js renderOverlay +
  // dVfx/start.js): id order is the C GfxFontId enum.
  const GLYPH_FONTS = [
    { font: "900 40px Arial", lw: 2, xscale: 1, chars: "0123456789:" },
    { font: "900 25px Arial", lw: 2, xscale: 1, chars: "0123456789" },
    { font: "900 53px Arial", lw: 2, xscale: 0.8, chars: "0123456789%" },
    { font: "italic 700 70px Arial", lw: 10, xscale: 1, chars: "0123456789 " },
  ];
  const GPEN_X = 200, GPEN_Y = 600; // 5-grid-aligned pen for glyph rasters

  function hex2(n) { return (n < 16 ? "0" : "") + n.toString(16); }

  function alphaBBox(data, w, h) {
    let x0 = w, y0 = h, x1 = -1, y1 = -1;
    for (let y = 0; y < h; y++) {
      for (let x = 0; x < w; x++) {
        if (data[(y * w + x) * 4 + 3] > 0) {
          if (x < x0) x0 = x;
          if (x > x1) x1 = x;
          if (y < y0) y0 = y;
          if (y > y1) y1 = y;
        }
      }
    }
    if (x1 < 0) return null;
    // align outward to the 5-grid (device-pixel cells)
    return {
      x0: Math.floor(x0 / 5) * 5, y0: Math.floor(y0 / 5) * 5,
      x1: Math.floor(x1 / 5) * 5 + 5, y1: Math.floor(y1 / 5) * 5 + 5,
    };
  }

  // 5x5 area-average of the alpha channel inside bbox -> hex mask string
  function downAlpha(data, w, bb) {
    const dw = (bb.x1 - bb.x0) / 5, dh = (bb.y1 - bb.y0) / 5;
    let out = "";
    for (let dy = 0; dy < dh; dy++) {
      for (let dx = 0; dx < dw; dx++) {
        let sum = 0;
        for (let sy = 0; sy < 5; sy++) {
          for (let sx = 0; sx < 5; sx++) {
            sum += data[((bb.y0 + dy * 5 + sy) * w + bb.x0 + dx * 5 + sx) * 4 + 3];
          }
        }
        out += hex2(Math.round(sum / 25));
      }
    }
    return out;
  }

  function glyphCanvas() {
    const c = document.createElement("canvas");
    c.width = 600;
    c.height = 800;
    return c;
  }

  function rasterGlyph(spec, ch, mode) {
    const c = glyphCanvas();
    const x = c.getContext("2d");
    x.save();
    x.translate(GPEN_X, GPEN_Y);
    if (spec.xscale !== 1) x.scale(spec.xscale, 1);
    x.font = spec.font;
    x.textAlign = "start";
    if (mode === "fill") {
      x.fillStyle = "white";
      x.fillText(ch, 0, 0);
    } else {
      x.strokeStyle = "white";
      x.lineWidth = spec.lw;
      x.strokeText(ch, 0, 0);
    }
    x.restore();
    return x.getImageData(0, 0, c.width, c.height).data;
  }

  function measureAdvance(spec, ch) {
    const c = glyphCanvas();
    const x = c.getContext("2d");
    x.font = spec.font;
    return x.measureText(ch).width * spec.xscale * 0.2;
  }

  function spriteLines(name, draw, anchorX, anchorY) {
    const c = document.createElement("canvas");
    c.width = 1200;
    c.height = 750;
    const x = c.getContext("2d");
    draw(x);
    const data = x.getImageData(0, 0, c.width, c.height).data;
    const bb = alphaBBox(data, c.width, c.height);
    if (!bb) throw new Error("gfx-capture: empty banner sprite " + name);
    const dw = (bb.x1 - bb.x0) / 5, dh = (bb.y1 - bb.y0) / 5;
    let hexs = "";
    for (let dy = 0; dy < dh; dy++) {
      for (let dx = 0; dx < dw; dx++) {
        let aSum = 0, rSum = 0, gSum = 0, bSum = 0;
        for (let sy = 0; sy < 5; sy++) {
          for (let sx = 0; sx < 5; sx++) {
            const i = ((bb.y0 + dy * 5 + sy) * c.width + bb.x0 + dx * 5 + sx) * 4;
            const a = data[i + 3];
            aSum += a;
            rSum += data[i] * a;
            gSum += data[i + 1] * a;
            bSum += data[i + 2] * a;
          }
        }
        if (aSum === 0) {
          hexs += "00000000";
        } else {
          hexs += hex2(Math.round(rSum / aSum)) + hex2(Math.round(gSum / aSum)) +
                  hex2(Math.round(bSum / aSum)) + hex2(Math.round(aSum / 25));
        }
      }
    }
    const dx = bb.x0 / 5 - anchorX * 0.2;
    const dy = bb.y0 / 5 - anchorY * 0.2;
    return ["SPRITE " + name + " " + dw + " " + dh + " " + String(dx) + " " + String(dy),
            "RGBA " + hexs];
  }

  window.__gfxDumpGlyphs = function () {
    const lines = ["VFXGLYPHS1"];
    for (let fid = 0; fid < GLYPH_FONTS.length; fid++) {
      const spec = GLYPH_FONTS[fid];
      for (const ch of spec.chars) {
        const adv = measureAdvance(spec, ch);
        const fillData = rasterGlyph(spec, ch, "fill");
        const strokeData = rasterGlyph(spec, ch, "stroke");
        // union bbox so both masks share one rect
        const fb = alphaBBox(fillData, 600, 800);
        const sb = alphaBBox(strokeData, 600, 800);
        let bb = null;
        if (fb && sb) {
          bb = { x0: Math.min(fb.x0, sb.x0), y0: Math.min(fb.y0, sb.y0),
                 x1: Math.max(fb.x1, sb.x1), y1: Math.max(fb.y1, sb.y1) };
        } else {
          bb = fb || sb;
        }
        if (!bb) {
          // ink-free glyph (space): advance only
          lines.push("GLYPH " + fid + " " + ch.charCodeAt(0) + " 0 0 0 0 " + String(adv));
          continue;
        }
        const w = (bb.x1 - bb.x0) / 5, h = (bb.y1 - bb.y0) / 5;
        const dx = bb.x0 / 5 - GPEN_X / 5, dy = bb.y0 / 5 - GPEN_Y / 5;
        lines.push("GLYPH " + fid + " " + ch.charCodeAt(0) + " " + w + " " + h +
                   " " + String(dx) + " " + String(dy) + " " + String(adv));
        lines.push("FMASK " + downAlpha(fillData, 600, bb));
        lines.push("SMASK " + downAlpha(strokeData, 600, bb));
      }
    }
    // Ready / Go! composite sprites (dVfx/start.js static text arms,
    // gradients + double strokes verbatim; the animated countdown +
    // progress bar are drawn geometrically by the C side)
    lines.push(...spriteLines("ready", (x) => {
      const textGrad = x.createLinearGradient(0, 200, 0, 500);
      textGrad.addColorStop(0, "rgb(255, 51, 51)");
      textGrad.addColorStop(0.6, "rgb(255, 51, 51)");
      textGrad.addColorStop(1, "rgb(121, 0, 0)");
      x.fillStyle = textGrad;
      x.textAlign = "start";
      x.lineWidth = 20;
      x.strokeStyle = "black";
      x.font = "italic 900 200px Arial";
      x.strokeText("Ready", 240, 420);
      x.lineWidth = 10;
      x.strokeStyle = "white";
      x.strokeText("Ready", 240, 420);
      x.fillText("Ready", 240, 420);
    }, 240, 420));
    lines.push(...spriteLines("go", (x) => {
      const textGrad = x.createLinearGradient(0, 200, 0, 480);
      textGrad.addColorStop(0, "black");
      textGrad.addColorStop(0.6, "black");
      textGrad.addColorStop(1, "rgb(221, 145, 57)");
      x.fillStyle = textGrad;
      x.textAlign = "start";
      x.lineWidth = 40;
      x.strokeStyle = "black";
      x.font = "900 400px Arial";
      x.strokeText("Go!", 240, 470);
      x.lineWidth = 20;
      x.strokeStyle = "white";
      x.strokeText("Go!", 240, 470);
      x.fillText("Go!", 240, 470);
    }, 240, 470));
    lines.push("END");
    return lines.join("\n") + "\n";
  };

  // --- U1 background parity -------------------------------------------------
  // gfx_bg.c's render-local seed, mirrored bit-for-bit (ml_rng.h
  // mulberry32 == oracle/harness/init.js's algorithm).
  const BG_SEED = 0xBADD00D5;
  let bgRandom = null;

  function mulberry32(a) {
    return function () {
      a |= 0; a = (a + 0x6D2B79F5) | 0;
      let t = Math.imul(a ^ (a >>> 15), 1 | a);
      t = (t + Math.imul(t ^ (t >>> 7), 61 | t)) ^ t;
      return ((t ^ (t >>> 14)) >>> 0) / 4294967296;
    };
  }

  function eqArr(a, b) {
    if (!Array.isArray(a) || a.length !== b.length) return false;
    for (let i = 0; i < a.length; i++) {
      if (Array.isArray(b[i])) { if (!eqArr(a[i], b[i])) return false; }
      else if (a[i] !== b[i]) return false;
    }
    return true;
  }

  // Seeds the mirrored stream and puts stagerender's bg module state in
  // the exact state gfx_bg_reset() leaves the C in. Hard-fails if the
  // page is not where we believe it is.
  window.__gfxBgInit = function () {
    if (typeof window.__bgState !== "function") {
      throw new Error("gfx-capture: __bgState hook missing (served-bytes bg injection)");
    }
    const st = window.__bgState();
    const stars = st[0], pos = st[1], dir = st[2], circ = st[3];
    const sparkle = st[4], ang = st[5];
    // Pristine-state assertions: everything except bgStars must still be
    // at its stagerender.js literal, or drawBackground has already run
    // and the two sides cannot be paired.
    if (!eqArr(pos, [[-30, 500, 300, 500, 900, 500, 1230, 450, 358],
                     [-30, 400, 300, 400, 900, 400, 1230, 350, 179]])) {
      throw new Error("gfx-capture: bgPos is not at its module literal (drawBackground already ran?)");
    }
    if (!eqArr(dir, [[1, -1, 1, -1, 1, -1, 1, -1, 1],
                     [-1, 1, -1, 1, -1, 1, -1, 1, -1]])) {
      throw new Error("gfx-capture: direction is not at its module literal");
    }
    if (!eqArr(circ, [0, 40, 80, 120, 160])) {
      throw new Error("gfx-capture: circleSize is not at its module literal");
    }
    if (sparkle !== 3 || ang !== 0) {
      throw new Error("gfx-capture: bgSparkle/ang are not at their module literals");
    }
    if (!Array.isArray(stars) || stars.length !== 20) {
      throw new Error("gfx-capture: bgStars is not the 20-element module array");
    }
    // Re-run stagerender.js:16-19 over the mirrored stream by calling
    // UPSTREAM'S OWN BYTES: __bgReseed is that loop source-transformed
    // into a function by the served-bytes hook (capture-canvas.js
    // reseedHook), so nothing about the star initialisation is restated
    // on our side and a matching error in gfx_bg.c cannot hide.
    if (typeof window.__bgReseed !== "function") {
      throw new Error("gfx-capture: __bgReseed hook missing (served-bytes reseed injection)");
    }
    bgRandom = mulberry32(BG_SEED);
    const saved = Math.random;
    Math.random = bgRandom;
    try {
      window.__bgReseed();
    } finally {
      Math.random = saved;
    }
    if (window.__bgState()[0] !== stars || stars.length !== 20) {
      throw new Error("gfx-capture: __bgReseed replaced the bgStars array instead of re-seeding it");
    }
    return true;
  };

  // --- star-only plane (review-u1 r4) ---------------------------------------
  // The starfield needs judging over the WHOLE letterbox band, not just the
  // rows mountains cannot reach: stars draw below that line too, so a
  // renderer clipping only the lower ones would otherwise pass. drawStars
  // is the only background code that issues bg2.arc()+fill() (the
  // mountains use bezierCurveTo/lineTo, drawTunnel strokes), so mirroring
  // exactly those calls onto a scratch layer isolates the starfield with
  // upstream's own centre, radius, fillStyle and globalAlpha.
  let starLayer = null, starCtx = null;

  function hookStarCapture() {
    const bg2 = mods.main.layers.BG2.getContext("2d");
    if (!starLayer) {
      starLayer = document.createElement("canvas");
      starLayer.width = mods.main.layers.BG2.width;
      starLayer.height = mods.main.layers.BG2.height;
      starCtx = starLayer.getContext("2d");
    }
    starCtx.clearRect(0, 0, starLayer.width, starLayer.height);
    const realArc = bg2.arc, realFill = bg2.fill;
    let pending = null;
    bg2.arc = function (x, y, r, a0, a1, ccw) {
      pending = [x, y, r, a0, a1, ccw];
      return realArc.call(this, x, y, r, a0, a1, ccw);
    };
    bg2.fill = function () {
      if (pending) {
        starCtx.globalAlpha = this.globalAlpha;
        starCtx.fillStyle = this.fillStyle;
        starCtx.beginPath();
        realArc.apply(starCtx, pending);
        realFill.call(starCtx);
        pending = null;
      }
      return realFill.apply(this, arguments);
    };
    return function () {
      delete bg2.arc;
      delete bg2.fill;
      if (bg2.arc !== realArc || bg2.fill !== realFill) {
        throw new Error("gfx-capture: failed to restore BG2 arc/fill after star capture");
      }
    };
  }

  // --- render + mask capture ------------------------------------------------
  // opts.bg === false skips drawBackground. The injection frame renders
  // SEVEN times (canonical + det + 5 leave-one-out) and the bg stream
  // must advance exactly ONCE per game frame to stay in step with the C,
  // so only the canonical render of any frame draws the background. The
  // fg1|fg2|UI mask those extra renders exist to compare is untouched by
  // the bg pass (different canvas layers), and boxFill only changes fg2
  // COLOUR, never its alpha — so the fg judgments are unaffected.
  window.__gfxRender = function (opts) {
    const withBg = !(opts && opts.bg === false);
    const M = mods.main, S = mods.stage, R = mods.render, A = mods.article;
    const H = window.__harness;
    const players = H.getPlayers();
    const ptype = H.getPlayerType();
    const savedRandom = Math.random;
    const oct = [null, null, null, null];
    const shake = [null, null, null, null];
    for (let i = 0; i < 4; i++) {
      if (ptype[i] > -1) {
        oct[i] = players[i].phys.outOfCameraTimer;
        // percentShake guard (M4 task 2 methodology round, refutation
        // shape (a)): percentShake decays via WALL-CLOCK setTimeout
        // callbacks (the CHECKSUM.md section-7 timing-dependent
        // exclusion), so its value at a sampled frame varies capture to
        // capture — measured: f1297 IoU 0.9074 vs 0.8835 across two
        // otherwise-identical captures. The C overlay draws unshaken;
        // zeroing the shake for the reference render makes the pairing
        // honest on both sides (same guard class as the native-RNG swap
        // and the outOfCameraTimer snapshot). Snapshot/restore keeps the
        // page state untouched.
        shake[i] = { x: players[i].percentShake.x, y: players[i].percentShake.y };
        players[i].percentShake.x = 0;
        players[i].percentShake.y = 0;
      }
    }
    Math.random = window.__nativeRandom;
    try {
      M.clearScreen();
      if (withBg && mods.vfx.isShowSFX()) {
        // upstream's own gate (main.js:1249-1251). The bg gets the
        // MIRRORED stream, everything else keeps the native swap.
        if (!bgRandom) throw new Error("gfx-capture: __gfxBgInit was never run");
        Math.random = bgRandom;
        const unhook = hookStarCapture();
        try { S.drawBackground(); } finally {
          unhook();
          Math.random = window.__nativeRandom;
        }
      }
      S.drawStage();
      for (let i = 0; i < 4; i++) {
        if (ptype[i] > -1) R.renderPlayer(i);
      }
      A.renderArticles();
      mods.renderVfx.renderVfx();
      R.renderOverlay(true);
    } finally {
      Math.random = savedRandom;
      for (let i = 0; i < 4; i++) {
        if (oct[i] !== null) players[i].phys.outOfCameraTimer = oct[i];
        if (shake[i] !== null) {
          players[i].percentShake.x = shake[i].x;
          players[i].percentShake.y = shake[i].y;
        }
      }
    }
  };

  // Synthetic-coverage injection (expected-render.json "inject"): the
  // SAME configs spawn on both sides at the same point in the frame
  // (post-tick, pre-render). None of the injected names draw seeded RNG
  // at spawn (only circleDust does, and it is not injectable here);
  // native RNG is swapped in anyway, same guard as the render call.
  window.__gfxInject = function (configs) {
    const savedRandom = Math.random;
    Math.random = window.__nativeRandom;
    try {
      for (const cfg of configs) {
        const c = { name: cfg.name, pos: { x: cfg.pos.x, y: cfg.pos.y } };
        if ("face" in cfg) c.face = cfg.face;
        if ("f" in cfg) c.f = cfg.f;
        mods.drawVfx.drawVfx(c);
      }
    } finally {
      Math.random = savedRandom;
    }
  };

  function b64(bytes) {
    let s = "";
    const CH = 0x8000;
    for (let i = 0; i < bytes.length; i += CH) {
      s += String.fromCharCode.apply(null, bytes.subarray(i, i + CH));
    }
    return btoa(s);
  }

  window.__gfxCaptureMask = function () {
    const l = mods.main.layers;
    const w = l.FG1.width, h = l.FG1.height;
    if (w !== 1200 || h !== 750) throw new Error("gfx-capture: unexpected canvas size");
    if (l.UI.width !== w || l.UI.height !== h) {
      throw new Error("gfx-capture: UI layer size mismatch");
    }
    const d1 = l.FG1.getContext("2d").getImageData(0, 0, w, h).data;
    const d2 = l.FG2.getContext("2d").getImageData(0, 0, w, h).data;
    const d3 = l.UI.getContext("2d").getImageData(0, 0, w, h).data;
    const m = new Uint8Array(w * h);
    for (let i = 0; i < w * h; i++) {
      m[i] = (d1[i * 4 + 3] > 0 || d2[i * 4 + 3] > 0 || d3[i * 4 + 3] > 0) ? 1 : 0;
    }
    // U1: the BG2 plane, judged separately. BG1 is deliberately NOT in
    // it — drawBackgroundInit fills BG1 opaque edge to edge, so its
    // silhouette is all-ones on both sides and would judge nothing; the
    // gradient is judged by colour instead (__gfxBgGradColumn).
    // SILHOUETTE ONLY (alpha > 0), matching the fg mask and the C ink
    // plane: star/mountain COLOURS and opacity magnitudes are not judged
    // here (review-u1 r1 L1). The BG1 gradient is the colour-judged part.
    if (l.BG2.width !== w || l.BG2.height !== h) {
      throw new Error("gfx-capture: BG2 layer size mismatch");
    }
    const db = l.BG2.getContext("2d").getImageData(0, 0, w, h).data;
    const mb = new Uint8Array(w * h);
    for (let i = 0; i < w * h; i++) mb[i] = db[i * 4 + 3] > 0 ? 1 : 0;
    const ms = new Uint8Array(w * h);
    if (starCtx) {
      const ds = starCtx.getImageData(0, 0, w, h).data;
      for (let i = 0; i < w * h; i++) ms[i] = ds[i * 4 + 3] > 0 ? 1 : 0;
    }
    // composite PNG (ui over fg2 over fg1 on black) — human debugging only
    const c = document.createElement("canvas");
    c.width = w; c.height = h;
    const ctx = c.getContext("2d");
    ctx.fillStyle = "black";
    ctx.fillRect(0, 0, w, h);
    ctx.drawImage(l.FG1, 0, 0);
    ctx.drawImage(l.FG2, 0, 0);
    ctx.drawImage(l.UI, 0, 0);
    return { mask: b64(m), bg: b64(mb), star: b64(ms), png: c.toDataURL("image/png") };
  };

  // The type the PAGE selected, from upstream's own seeded startGame
  // draw (main.js:1322). Read through __bgState so it is the live module
  // variable, not a stale export snapshot.
  window.__gfxBgType = function () {
    return window.__bgState()[6];
  };

  // The BG1 gradient as one RGB triplet per canvas row, read out of the
  // live layer at mid-canvas (the gradient is vertical, so any column
  // carries it). Compared against the 8-bit row colours the C's gradient
  // loop actually handed the rasterizer, which gfx_replay cross-checks
  // against the RGB565 framebuffer that same loop wrote.
  window.__gfxBgGradColumn = function () {
    const l = mods.main.layers;
    const d = l.BG1.getContext("2d").getImageData(600, 0, 1, l.BG1.height).data;
    const lines = ["BGGRAD1"];
    for (let y = 0; y < l.BG1.height; y++) {
      if (d[y * 4 + 3] !== 255) {
        throw new Error("gfx-capture: BG1 is not opaque at row " + y +
          " (drawBackgroundInit never ran?)");
      }
      lines.push(`R ${y} ${d[y * 4]} ${d[y * 4 + 1]} ${d[y * 4 + 2]}`);
    }
    lines.push("END");
    return lines.join("\n") + "\n";
  };

  // The standalone tunnel leg (backgroundType 1). The type is a per-SEED
  // coin flip at startGame; MEASURED over the committed goldens,
  // g01/g02/g03/g05/g06/g07/g08/m02 select type 0 while g04 (seed
  // 7344) and m01 (seed 8114) select type 1.
  // This rig replays g01 (type 0) only, so drawTunnel is unreachable
  // from its match — but it is half of gfx_bg.c and consumes no
  // randomness, so it pairs exactly from pristine module state. Runs AFTER the match, driving
  // upstream's own drawBackground through upstream's own type-1 init.
  window.__gfxBgTunnel = function (frames, sampled) {
    const M = mods.main, S = mods.stage;
    const out = {};
    S.setBackgroundType(1);
    S.drawBackgroundInit(); // installs the radial gridGrad strokeStyle
    const saved = Math.random;
    Math.random = window.__nativeRandom;
    try {
      for (let f = 1; f <= frames; f++) {
        M.clearScreen();
        S.drawBackground();
        if (sampled[f]) out[f] = window.__gfxCaptureMask().bg;
      }
    } finally {
      Math.random = saved;
    }
    return out;
  };

  window.__gfxCaptured = {};

  // Step n frames (pagelib __runFrames VERBATIM, one frame per call so
  // the render runs between steps exactly like a live browser frame),
  // inject on the pinned frame, render each, capture sampled ones.
  window.__gfxRunChunk = async function (n, sampled, inject) {
    const out = [];
    for (let k = 0; k < n; k++) {
      const r = await window.__runFrames(1, {});
      out.push(r[0]);
      if (inject && r[0].f === inject.frame) {
        window.__gfxInject(inject.configs);
      }
      window.__gfxRender();
      if (sampled[r[0].f]) {
        window.__gfxCaptured[r[0].f] = window.__gfxCaptureMask();
      }
    }
    return out;
  };

  window.__gfxDrainCaptured = function () {
    const c = window.__gfxCaptured;
    window.__gfxCaptured = {};
    return c;
  };
})();
