// port/gfx/gfx-pagelib.js — page-side helpers for the render-reference
// capture (M3 task 3). Injected AFTER oracle/harness/{init,pagelib}.js
// (both used VERBATIM — oracle/ is never modified) by capture-canvas.js.
//
// Provides:
//   __gfxFindModules(): locate the render-plane modules in the webpack
//     module cache (window.__wpCache, exposed by the served-bytes boot
//     hook — the run-capture.js mechanism class). Unique-match hard-fail.
//   __gfxDumpData(): the GFXDATA1 text (palettes / pPal / flashOnLCancel
//     / reverseModel flags) EXECUTED out of the live page — the C
//     renderer's colour/flag data plane, never hand-typed.
//   __gfxRunChunk(n, sampled): steps n frames through the UNCHANGED
//     pagelib __runFrames (1 frame at a time, so the checksum stream is
//     the harness's own), runs the reduced render sequence after every
//     step, and captures the fg1|fg2 ink mask (+ a composite PNG for
//     humans) on sampled frames.
//
// Render sequence = the gameMode-3 renderTick branch (main.js:1243-1261)
// MINUS drawBackground/renderVfx/renderOverlay (out of scope both sides;
// see the iter-44 pre-registration). Two guards keep the sim stream
// clean, PROVEN by the run's verify-stream STREAM MATCH:
//   - Math.random is swapped to the harness's stashed native RNG for the
//     duration of the render call (render-plane code must not consume
//     the seeded gameplay stream);
//   - phys.outOfCameraTimer is snapshot/restored around it (renderPlayer
//     writes it and physics.js:660 FEEDS IT BACK into percent).
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
      typeof e.drawArrayPathCompress === "function");
    mods.article = findModule("physics/article", (e) =>
      typeof e.renderArticles === "function" && e.articles && e.aArticles !== undefined);
    mods.asshort = findModule("physics/actionStateShortcuts", (e) =>
      !!e.actionStates && typeof e.turnOffHitboxes === "function");
    mods.settings = findModule("settings", (e) =>
      e.gameSettings && "flashOnLCancel" in e.gameSettings);
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

  // --- render + mask capture ------------------------------------------------
  window.__gfxRender = function () {
    const M = mods.main, S = mods.stage, R = mods.render, A = mods.article;
    const H = window.__harness;
    const players = H.getPlayers();
    const ptype = H.getPlayerType();
    const savedRandom = Math.random;
    const oct = [null, null, null, null];
    for (let i = 0; i < 4; i++) {
      if (ptype[i] > -1) oct[i] = players[i].phys.outOfCameraTimer;
    }
    Math.random = window.__nativeRandom;
    try {
      M.clearScreen();
      S.drawStage();
      for (let i = 0; i < 4; i++) {
        if (ptype[i] > -1) R.renderPlayer(i);
      }
      A.renderArticles();
    } finally {
      Math.random = savedRandom;
      for (let i = 0; i < 4; i++) {
        if (oct[i] !== null) players[i].phys.outOfCameraTimer = oct[i];
      }
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
    const d1 = l.FG1.getContext("2d").getImageData(0, 0, w, h).data;
    const d2 = l.FG2.getContext("2d").getImageData(0, 0, w, h).data;
    const m = new Uint8Array(w * h);
    for (let i = 0; i < w * h; i++) {
      m[i] = (d1[i * 4 + 3] > 0 || d2[i * 4 + 3] > 0) ? 1 : 0;
    }
    // composite PNG (fg2 over fg1 on black) — human debugging only
    const c = document.createElement("canvas");
    c.width = w; c.height = h;
    const ctx = c.getContext("2d");
    ctx.fillStyle = "black";
    ctx.fillRect(0, 0, w, h);
    ctx.drawImage(l.FG1, 0, 0);
    ctx.drawImage(l.FG2, 0, 0);
    return { mask: b64(m), png: c.toDataURL("image/png") };
  };

  window.__gfxCaptured = {};

  // Step n frames (pagelib __runFrames VERBATIM, one frame per call so
  // the render runs between steps exactly like a live browser frame),
  // render each, capture sampled ones.
  window.__gfxRunChunk = async function (n, sampled) {
    const out = [];
    for (let k = 0; k < n; k++) {
      const r = await window.__runFrames(1, {});
      out.push(r[0]);
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
