#!/usr/bin/env node
// Determinism-spike runner: boots the (patched) meleelight dist page in
// headless Chrome, sets up a versus match programmatically, steps the sim
// one gameTick per frame with synthetic inputs, and emits a per-frame
// SHA-256 checksum stream.
//
// Usage:
//   node run.js --dist <meleelight-clone-root> --trace trace-p1p2.json \
//     --frames 3600 --seed 42 --out out/run.json \
//     [--native-rng] [--cpu] [--capture-frames 512,513] [--render]
//
// --cpu        : P2 is a CPU (difficulty 5) instead of trace-driven human.
// --native-rng : leave Math.random unseeded (experiment B).
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

const DIST_ROOT = path.resolve(arg("dist", ""));
const TRACE = arg("trace", path.join(__dirname, "trace-p1p2.json"));
const FRAMES = parseInt(arg("frames", "3600"), 10);
const SEED = parseInt(arg("seed", "42"), 10);
const OUT = arg("out", "run.json");
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
    console.error("--dist must point at a built meleelight clone (dist/meleelight.html)");
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

  // match config: P1 fox human; P2 marth, human (trace) or cpu
  const cpu = has("cpu");
  await page.evaluate(({ trace, cpu }) => {
    window.__trace = trace;
    window.__resetMathCalls(); // count sim exposure, not boot noise
    window.__harness.setupMatch({
      players: [
        { type: 0, character: 2 },                                // P1 fox
        cpu ? { type: 1, character: 0, difficulty: 5 }
            : { type: 0, character: 0 },                          // P2 marth
        null, null,
      ],
      stage: 0,                                                   // battlefield
    });
  }, { trace, cpu });

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
      seedRandom: !has("native-rng"), cpu, wallMs: wall,
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
