#!/usr/bin/env node
// port/goldens-m4/run-target.js — TARGET-TEST recorder (fix_plan §M4
// task 11; the REGISTERED FALLBACK recorder — measured iter 94: the
// harness patch has NO target-test entry (harnessSetupMatch hardcodes
// startGame() -> changeGamemode(3); run.js has no gameMode/target
// param), and HARD RULE 3 forbids touching oracle/. This driver
// therefore reuses the oracle harness bytes VERBATIM BY PATH
// (fdlibm.js / init.js / pagelib.js addInitScript — the exact run.js
// init pipeline) and reaches the upstream target-test entry through the
// webpack module cache (the run-capture.js served-bytes BOOT_HOOK
// class: window.__wpCache; disk never written), mirroring the REAL
// entry path stages/targetselect.js:143-146:
//   setActiveStageTarget(n); setTargetStagePlaying(n);
//   startTargetGame(0, false)
// over the harnessSetupMatch state writes (patch:76-92) projected to
// the 1-player target domain. Zero oracle/ edits; oracle/goldens/*
// untouched.
//
// Usage:
//   node run-target.js --dist <built-clone-root> --trace <trace.json> \
//     --frames N --seed N --char <0-4> --tstage <0-9> --out out/run.json \
//     [--capture-frames a,b]
//
// Output run JSON:
//   meta     — run.js-compatible pins for the UNCHANGED verify-stream.js
//              (p2/stage/cpu/difficulty are null/false in target mode)
//              + mode:"target" + tstage (pinned by verify-target-stream)
//   coverage — pagelib __coverage() + the target-plane finals
//   frames   — the spec-v1 player/article checksum stream (pagelib's OWN
//              __runFrames/__serializeState/__sha256, bytes by path)
//   target.frames — the SEPARATE target-plane stream (iter-63
//              convention): per frame, the fixed-literal envelope
//   {"endTargetGame":<T/F>,"matchTimer":<num>,"targetDestroyed":[<T/F>
//    x len],"targetsDestroyed":<num>}
//              hashed with the page's own __sha256. Value tokens follow
//              CHECKSUM.md §3 over this domain's {boolean, number}
//              values: T/F, and numStr(v) = Object.is(v,-0) ? "-0" :
//              String(v) — the pagelib.js:10-13 rule restated over a
//              string-free domain (the C twin ml_sb_bool/ml_sb_num is
//              the task-15 differentially-proven implementation; the
//              x2-identity + C-conformance triangle covers the rest).
//              targetDestroyed serializes at its VERBATIM length (the
//              10-slot let — targetplay.js:37/:189).
//
// Snapshot timing: __serializeState is wrapped (called EXACTLY once per
// frame inside pagelib's __runFrames, immediately post-step) to push the
// target envelope STRING synchronously; hashing happens after the run
// (crypto.subtle — no seeded-RNG interaction either way). Each snap
// carries the page's OWN frame counter (window.__frameCount, set to f+1
// at pagelib.js:101 right before the call) captured AT CALL TIME with a
// strict +1 monotonicity death — frame numbers are never invented
// post-hoc (review-94 M3, iter 96).
"use strict";

const fs = require("fs");
const http = require("http");
const path = require("path");
const { chromium } = require(path.join(__dirname, "..", "..", "oracle",
  "harness", "node_modules", "playwright"));

function arg(name, dflt) {
  const i = process.argv.indexOf("--" + name);
  if (i === -1) return dflt;
  const v = process.argv[i + 1];
  return v === undefined || v.startsWith("--") ? true : v;
}
const has = (name) => process.argv.includes("--" + name);

function intArg(name, dflt, lo, hi, what) {
  const raw = arg(name, String(dflt));
  const v = parseInt(raw, 10);
  if (!Number.isInteger(v) || String(v) !== String(raw).trim() || v < lo || v > hi) {
    console.error(`--${name} must be an integer ${lo}-${hi} (${what}); got: ${raw}`);
    process.exit(1);
  }
  return v;
}

const REPO = path.join(__dirname, "..", "..");
const HARNESS = path.join(REPO, "oracle", "harness");

const DIST_ROOT = path.resolve(arg("dist", ""));
const TRACE = arg("trace", "");
const FRAMES = intArg("frames", 3600, 1, 1000000, "frames to simulate");
const SEED = intArg("seed", 42, 0, 4294967295, "Math.random seed");
const OUT = arg("out", "run-target.json");
const CHAR = intArg("char", 2, 0, 4,
  "character: 0 marth 1 puff 2 fox 3 falco 4 falcon");
const TSTAGE = intArg("tstage", 0, 0, 9,
  "target stage: 0-9 == targetstage1..10 (activeStage.js targetStageMapping)");
const CHUNK = 120;

const MIME = {
  ".html": "text/html", ".js": "text/javascript", ".css": "text/css",
  ".json": "application/json", ".png": "image/png", ".jpg": "image/jpeg",
  ".svg": "image/svg+xml", ".wav": "audio/wav", ".mp3": "audio/mpeg",
};

// The run-capture.js served-bytes class (port/sim/calib/run-capture.js
// :71-83): expose the webpack module cache; the ONE injection, quote-
// and newline-free, unique-match hard-fail, disk untouched.
const BOOT_LINE = "var installedModules = {};";
const BOOT_HOOK = BOOT_LINE + " window.__wpCache = installedModules;";

function serve(root) {
  return new Promise((resolve) => {
    const srv = http.createServer((req, res) => {
      const urlPath = decodeURIComponent(req.url.split("?")[0]);
      const fp = path.join(root, path.normalize(urlPath));
      if (!fp.startsWith(root) || !fs.existsSync(fp) || fs.statSync(fp).isDirectory()) {
        res.writeHead(404); res.end("nope"); return;
      }
      if (urlPath.endsWith("/dist/js/main.js")) {
        const src = fs.readFileSync(fp, "utf8");
        const i = src.indexOf(BOOT_LINE);
        if (i === -1 || src.indexOf(BOOT_LINE, i + 1) !== -1) {
          res.writeHead(500);
          res.end("run-target: webpack bootstrap line not found exactly once in main.js");
          return;
        }
        const hooked = src.slice(0, i) + BOOT_HOOK + src.slice(i + BOOT_LINE.length);
        res.writeHead(200, { "Content-Type": "text/javascript" });
        res.end(hooked);
        return;
      }
      res.writeHead(200, { "Content-Type": MIME[path.extname(fp)] || "application/octet-stream" });
      fs.createReadStream(fp).pipe(res);
    });
    srv.listen(0, "127.0.0.1", () => resolve(srv));
  });
}

async function main() {
  if (!DIST_ROOT || !fs.existsSync(path.join(DIST_ROOT, "dist/meleelight.html"))) {
    console.error("--dist must point at a built meleelight clone (dist/meleelight.html); " +
      "run oracle/build-upstream.sh first");
    process.exit(1);
  }
  if (!TRACE || TRACE === true || !fs.existsSync(TRACE)) {
    console.error("--trace <file> is required (target trace JSON; slot 0 only)");
    process.exit(1);
  }
  const trace = JSON.parse(fs.readFileSync(TRACE, "utf8"));
  // Target traces are single-player: slots 1-3 must be null on every row
  // (the C twin refuses the same shape — target_main.c).
  trace.forEach((row, f) => {
    if (!Array.isArray(row) || row.length !== 4 || row[0] === null ||
        row[1] !== null || row[2] !== null || row[3] !== null) {
      console.error(`trace row ${f}: target traces need [input, null, null, null]`);
      process.exit(1);
    }
  });

  const srv = await serve(DIST_ROOT);
  const port = srv.address().port;

  let browser;
  try {
    browser = await chromium.launch({ channel: "chrome", headless: true });
  } catch (e) {
    browser = await chromium.launch({ headless: true });
  }
  const context = await browser.newContext();
  await context.route(/\/(sfx|music)\//, (r) => r.abort());

  const page = await context.newPage();
  let pageErr = false;
  page.on("pageerror", (e) => { console.error("[pageerror]", e.message); pageErr = true; });
  page.on("console", (m) => {
    if (m.type() === "error" && !m.text().includes("Failed to load resource")) {
      console.error("[console.error]", m.text());
    }
  });

  // The EXACT run.js init pipeline (golden-recording defaults: seeded
  // RNG, virtual clock, fdlibm shim ON) — bytes consumed BY PATH.
  await page.addInitScript({
    content: "window.__harnessConfig = " +
      JSON.stringify({ seedRandom: true, seed: SEED, virtualClock: true,
        fdlibm: true, captureMath: false }) + ";",
  });
  await page.addInitScript({ path: path.join(REPO, "port", "fdlibm", "fdlibm.js") });
  await page.addInitScript({ path: path.join(HARNESS, "init.js") });
  await page.addInitScript({ path: path.join(HARNESS, "pagelib.js") });

  await page.goto(`http://localhost:${port}/dist/meleelight.html`);
  await page.waitForFunction(
    "window.__harness && window.__nextInputBuffers && window.__wpCache",
    null, { timeout: 120000 });

  // Locate the three upstream modules by EXPORT SHAPE (unique-match
  // hard-fail), mirror the harnessSetupMatch state writes for the
  // 1-player target domain, then take the MEASURED targetselect entry.
  await page.evaluate(({ trace, charId, tstage }) => {
    function findModule(what, pred) {
      const c = window.__wpCache;
      const hits = [];
      for (const k in c) {
        const ex = c[k] && c[k].exports;
        if (ex && pred(ex)) hits.push(ex);
      }
      if (hits.length !== 1) {
        throw new Error("run-target: module '" + what + "' matched " +
          hits.length + " cache entries (want exactly 1)");
      }
      return hits[0];
    }
    const mainM = findModule("main/main", (ex) =>
      Array.isArray(ex.playerType) && Array.isArray(ex.characterSelections) &&
      Array.isArray(ex.mType) && Array.isArray(ex.currentPlayers) &&
      Array.isArray(ex.cpuDifficulty) && typeof ex.gameTick === "function");
    const tpM = findModule("target/targetplay", (ex) =>
      typeof ex.startTargetGame === "function" &&
      typeof ex.setTargetStagePlaying === "function" &&
      typeof ex.targetHitDetection === "function" &&
      Array.isArray(ex.targetDestroyed));
    const asM = findModule("stages/activeStage", (ex) =>
      typeof ex.setActiveStageTarget === "function" && ex.activeStage);

    window.__trace = trace;
    window.__resetMathCalls(); // count sim exposure, not boot noise (run.js:150)

    // harnessSetupMatch's state writes (patch:76-92), 1-player form:
    // slot 0 human keyboard, slots 1-3 off.
    mainM.playerType[0] = 0;
    mainM.mType[0] = "keyboard";
    mainM.currentPlayers[0] = 0;
    mainM.characterSelections[0] = charId;
    mainM.cpuDifficulty[0] = 3;
    for (let i = 1; i < 4; i++) {
      mainM.playerType[i] = -1;
      mainM.currentPlayers[i] = -1;
    }
    // stages/targetselect.js:143-146 (sounds.menuForward.play() is a
    // menu-plane Howl with no seeded draw — not part of the sim entry):
    asM.setActiveStageTarget(tstage);
    tpM.setTargetStagePlaying(tstage);
    tpM.startTargetGame(0, false); // the ONE off-step seeded draw inside

    // Target-plane capture: wrap __serializeState (called EXACTLY once
    // per frame inside __runFrames, post-step) to push the envelope
    // string synchronously; the hash pass runs after the frame loop.
    // FRAME NUMBERS ARE CAPTURED, NEVER INVENTED (review-94 M3, iter
    // 96): pagelib.js:101 sets window.__frameCount = f+1 immediately
    // BEFORE this post-step call, so the captured value IS the page's
    // OWN index of the frame just stepped; strict +1 monotonicity is
    // asserted AT CALL TIME — a duplicate or dropped callback dies at
    // the exact frame, even inside the constant starting window where
    // hash sequences could mask it.
    const numStr = (v) => (Object.is(v, -0) ? "-0" : String(v)); // pagelib.js:10-13
    window.__targetSnaps = [];
    const origSer = window.__serializeState;
    window.__serializeState = function () {
      const fNow = window.__frameCount; // the page's OWN frame counter
      const prev = window.__targetSnaps.length
        ? window.__targetSnaps[window.__targetSnaps.length - 1].f : 0;
      if (!Number.isInteger(fNow) || fNow !== prev + 1) {
        throw new Error("target-plane snap counter broke +1 monotonicity: " +
          prev + " -> " + fNow +
          " (duplicate/dropped __serializeState callback)");
      }
      const td = tpM.targetDestroyed; // re-read: startTargetGame REBINDS it
      let tdStr = "[";
      for (let k = 0; k < td.length; k++) {
        tdStr += (k ? "," : "") + (td[k] ? "T" : "F");
      }
      tdStr += "]";
      window.__targetSnaps.push({ f: fNow, s:
        '{"endTargetGame":' + (mainM.endTargetGame ? "T" : "F") +
        ',"matchTimer":' + numStr(mainM.matchTimer) +
        ',"targetDestroyed":' + tdStr +
        ',"targetsDestroyed":' + numStr(tpM.targetsDestroyed) + "}" });
      return origSer();
    };
    window.__targetFinal = function () {
      return {
        endTargetGame: mainM.endTargetGame,
        targetsDestroyed: tpM.targetsDestroyed,
        targetDestroyed: tpM.targetDestroyed.slice(),
        targetStagePlaying: tpM.targetStagePlaying,
        gameMode: window.__harness.getGameMode(),
      };
    };
  }, { trace, charId: CHAR, tstage: TSTAGE });

  const captureFrames = {};
  if (arg("capture-frames", null)) {
    for (const s of String(arg("capture-frames")).split(",")) captureFrames[+s] = true;
    await page.evaluate((cf) => { window.__captureFrames = cf; }, captureFrames);
  }

  const frames = [];
  const t0 = Date.now();
  for (let done = 0; done < FRAMES; done += CHUNK) {
    const n = Math.min(CHUNK, FRAMES - done);
    const chunk = await page.evaluate(
      (opts) => window.__runFrames(opts.n,
        { capture: false, captureFrames: window.__captureFrames }),
      { n });
    frames.push(...chunk);
  }
  const wall = Date.now() - t0;

  // Target-plane hash pass (page's own __sha256; crypto.subtle — no
  // seeded-RNG interaction). Frame numbers come from the CAPTURED page
  // counter (review-94 M3) — never invented here.
  const targetFrames = await page.evaluate(async () => {
    const out = [];
    for (let k = 0; k < window.__targetSnaps.length; k++) {
      const snap = window.__targetSnaps[k];
      out.push({ f: snap.f, h: await window.__sha256(snap.s) });
    }
    return out;
  });
  if (targetFrames.length !== FRAMES) {
    console.error(`target-plane snap count ${targetFrames.length} != frames ${FRAMES} ` +
      "(the __serializeState wrapper must fire exactly once per frame)");
    process.exit(1);
  }
  // Captured-counter first/last + numbering (monotonicity was already
  // asserted at call time inside the wrapper; this is the belt).
  for (let k = 0; k < targetFrames.length; k++) {
    if (targetFrames[k].f !== k + 1) {
      console.error(`captured frame counter row ${k} carries f=` +
        `${targetFrames[k].f}, want ${k + 1} (first must be 1, last ${FRAMES})`);
      process.exit(1);
    }
  }

  const coverage = await page.evaluate(() => window.__coverage());
  const targetFinal = await page.evaluate(() => window.__targetFinal());
  const captured = await page.evaluate(() => window.__captured);
  coverage.target = targetFinal;

  if (pageErr) {
    console.error("run-target: page errors occurred — refusing to write a run JSON");
    process.exit(1);
  }

  fs.mkdirSync(path.dirname(path.resolve(OUT)), { recursive: true });
  fs.writeFileSync(OUT, JSON.stringify({
    meta: {
      dist: DIST_ROOT, trace: TRACE, frames: FRAMES, seed: SEED,
      p1: CHAR, p2: null, stage: null,
      seedRandom: true, fdlibm: true, cpu: false, difficulty: null,
      mode: "target", tstage: TSTAGE, wallMs: wall,
      browser: browser.browserType().name(), version: browser.version(),
    },
    coverage,
    captured,
    frames,
    target: { frames: targetFrames },
  }));
  console.log(`${OUT}: ${FRAMES} target-mode frames in ${wall}ms ` +
    `(${(FRAMES / (wall / 1000)).toFixed(0)} fps), rngCalls=${coverage.rngCalls}`);
  console.log("coverage:", Object.keys(coverage.actionStatesSeen).sort().join(","));
  console.log("targetsDestroyed:", targetFinal.targetsDestroyed,
    "endTargetGame:", targetFinal.endTargetGame,
    "gameMode:", targetFinal.gameMode,
    "maxArticles:", coverage.maxArticles);

  await browser.close();
  srv.close();
}

main().catch((e) => { console.error(e); process.exit(1); });
