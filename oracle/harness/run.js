#!/usr/bin/env node
// Oracle harness runner (productionized from spikes/determinism/harness/,
// which stays frozen evidence — THIS copy is the maintained one).
//
// Boots the patched meleelight dist page (built by oracle/build-upstream.sh)
// in headless Chrome, sets up a versus match programmatically, steps the sim
// one gameTick per frame feeding a recorded input trace, and emits a
// per-frame SHA-256 checksum stream.
//
// Usage:
//   node run.js --dist <built-meleelight-clone-root> --trace <trace.json> \
//     --frames 3600 --seed 1337 --out out/run.json \
//     [--p1 <char 0-4>] [--p2 <char 0-4>] [--stage <0-5>] \
//     [--cpu [--difficulty N]] \
//     [--native-rng] [--real-clock] [--capture-frames 512,513] \
//     [--capture-all] [--render]
//
// Character indices (upstream characterSelections):
//   0 marth · 1 puff · 2 fox · 3 falco · 4 falcon
// Stage indices (upstream stageMapping, src/stages/activeStage.js):
//   0 battlefield · 1 ystory · 2 pstadium · 3 dreamland · 4 fdest · 5 fountain
// Defaults (--p1 2 --p2 0 --stage 0) reproduce golden #1: Fox vs Marth on
// Battlefield.
//
// --cpu        : P2 is a CPU (--difficulty N, default 5) instead of
//                trace-driven; the trace's P2 column is ignored (the patch
//                lets AI slots keep reading aiInputBank).
// --native-rng : leave Math.random unseeded (determinism experiments only).
"use strict";

const fs = require("fs");
const http = require("http");
const path = require("path");
const { chromium } = require("playwright");

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

const DIST_ROOT = path.resolve(arg("dist", ""));
const TRACE = arg("trace", "");
const FRAMES = intArg("frames", 3600, 1, 1000000, "frames to simulate");
const SEED = intArg("seed", 42, 0, 4294967295, "Math.random seed");
const OUT = arg("out", "run.json");
const P1 = intArg("p1", 2, 0, 4, "P1 character: 0 marth 1 puff 2 fox 3 falco 4 falcon");
const P2 = intArg("p2", 0, 0, 4, "P2 character: 0 marth 1 puff 2 fox 3 falco 4 falcon");
const STAGE = intArg("stage", 0, 0, 5,
  "stage: 0 battlefield 1 ystory 2 pstadium 3 dreamland 4 fdest 5 fountain");
const CPU = has("cpu");
const DIFFICULTY = intArg("difficulty", 5, 1, 9, "CPU difficulty");
const CHUNK = 120;

const MIME = {
  ".html": "text/html", ".js": "text/javascript", ".css": "text/css",
  ".json": "application/json", ".png": "image/png", ".jpg": "image/jpeg",
  ".svg": "image/svg+xml", ".wav": "audio/wav", ".mp3": "audio/mpeg",
};

function serve(root) {
  return new Promise((resolve) => {
    const srv = http.createServer((req, res) => {
      const urlPath = decodeURIComponent(req.url.split("?")[0]);
      const fp = path.join(root, path.normalize(urlPath));
      if (!fp.startsWith(root) || !fs.existsSync(fp) || fs.statSync(fp).isDirectory()) {
        res.writeHead(404); res.end("nope"); return;
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
    console.error("--trace <file> is required (input trace JSON; see gen-trace.js)");
    process.exit(1);
  }
  const trace = JSON.parse(fs.readFileSync(TRACE, "utf8"));
  const srv = await serve(DIST_ROOT);
  const port = srv.address().port;

  let browser;
  try {
    browser = await chromium.launch({ channel: "chrome", headless: true });
  } catch (e) {
    browser = await chromium.launch({ headless: true });
  }
  const context = await browser.newContext();
  // audio is irrelevant to the sim; don't waste time loading 36MB of it
  await context.route(/\/(sfx|music)\//, (r) => r.abort());

  const page = await context.newPage();
  page.on("pageerror", (e) => console.error("[pageerror]", e.message));
  page.on("console", (m) => {
    // aborted sfx/music requests log "Failed to load resource" — expected
    if (m.type() === "error" && !m.text().includes("Failed to load resource")) {
      console.error("[console.error]", m.text());
    }
  });

  await page.addInitScript({
    content: "window.__harnessConfig = " +
      JSON.stringify({ seedRandom: !has("native-rng"), seed: SEED,
        virtualClock: !has("real-clock") }) + ";",
  });
  await page.addInitScript({ path: path.join(__dirname, "init.js") });
  await page.addInitScript({ path: path.join(__dirname, "pagelib.js") });
  if (has("render")) {
    await page.addInitScript({ content: "window.__harnessNoRender = false;" });
  }

  await page.goto(`http://localhost:${port}/dist/meleelight.html`);
  await page.waitForFunction(
    "window.__harness && window.__nextInputBuffers", null, { timeout: 120000 });

  // match config: P1 human; P2 human (trace-driven) or CPU
  await page.evaluate(({ trace, p1, p2, stage, cpu, difficulty }) => {
    window.__trace = trace;
    window.__resetMathCalls(); // count sim exposure, not boot noise
    window.__harness.setupMatch({
      players: [
        { type: 0, character: p1 },
        cpu ? { type: 1, character: p2, difficulty: difficulty }
            : { type: 0, character: p2 },
        null, null,
      ],
      stage: stage,
    });
  }, { trace, p1: P1, p2: P2, stage: STAGE, cpu: CPU, difficulty: DIFFICULTY });

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
        { capture: opts.captureAll, captureFrames: window.__captureFrames }),
      { n, captureAll: has("capture-all") });
    frames.push(...chunk);
  }
  const wall = Date.now() - t0;

  const coverage = await page.evaluate(() => window.__coverage());
  const captured = await page.evaluate(() => window.__captured);

  fs.mkdirSync(path.dirname(path.resolve(OUT)), { recursive: true });
  fs.writeFileSync(OUT, JSON.stringify({
    meta: {
      dist: DIST_ROOT, trace: TRACE, frames: FRAMES, seed: SEED,
      p1: P1, p2: P2, stage: STAGE,
      seedRandom: !has("native-rng"), cpu: CPU,
      difficulty: CPU ? DIFFICULTY : null, wallMs: wall,
      browser: browser.browserType().name(), version: browser.version(),
    },
    coverage,
    captured,
    frames,
  }));
  console.log(`${OUT}: ${FRAMES} frames in ${wall}ms ` +
    `(${(FRAMES / (wall / 1000)).toFixed(0)} fps), rngCalls=${coverage.rngCalls}`);
  console.log("coverage:", Object.keys(coverage.actionStatesSeen).sort().join(","));
  console.log("maxArticles:", coverage.maxArticles,
    "stocks:", coverage.stocks, "percents:", coverage.percents);

  await browser.close();
  srv.close();
}

main().catch((e) => { console.error(e); process.exit(1); });
