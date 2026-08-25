#!/usr/bin/env node
// port/goldens-m4/run-4p.js — FOUR-PORT golden recorder (A46).
//
// WHY THIS FILE EXISTS (measured, not assumed). The browser harness is
// ALREADY four-port: oracle/meleelight-harness.patch:76-92 is
//   export function harnessSetupMatch (cfg){
//     for (var i = 0; i < 4; i++) { var pc = cfg.players[i]; if (pc) {…} else {…} }
// and oracle/harness/pagelib.js's __serializeState / __runFrames /
// __coverage all loop `i < 4` over `playerType[i] > -1`. The ONLY
// two-port thing in the oracle is a CALLER: oracle/harness/run.js:152-157
// hands setupMatch a literal `[p1, p2, null, null]` and exposes no
// --p3/--p4. run.js lives under oracle/, which HARD RULE 3 makes
// read-only outside M0 — so this is the run-target.js pattern
// (port/goldens-m4/run-target.js, the REGISTERED FALLBACK recorder):
// a port/goldens-m4/-side driver that reuses the oracle harness bytes
// VERBATIM BY PATH (playwright from oracle/harness/node_modules, and
// fdlibm.js / init.js / pagelib.js through the exact same addInitScript
// pipeline run.js uses) and only changes the cfg.players ARRAY it passes.
// Zero oracle/ edits; nothing tracked under oracle/ is written.
//
// Usage:
//   node run-4p.js --dist <built-clone-root> --trace <trace.json> \
//     --frames N --seed N --p1 <0-4> --p2 <0-4> --p3 <0-4> --p4 <0-4> \
//     --stage <0-5> --out out/run.json [--capture-frames a,b]
//
// --p3/--p4 accept "none" for an ABSENT port (the patch's else arm), so
// this driver also reproduces a 3-port match and — with both "none" —
// run.js's own two-port config, which is how its equivalence to run.js
// is proven (check-fourport.sh leg [0]).
//
// Every port is a HUMAN (type 0), trace-driven: the CPU arm belongs to
// run.js/run-target.js's domain and the AIBRIDGE1 replay artifact is one
// stream for one CPU slot (port/sim/ai_bridge.h), so a CPU on port 2/3
// has no C-side replay today and is deliberately not offered here.
//
// Output run JSON: run.js's meta shape TYPE-exactly, so the UNCHANGED
// oracle/harness/verify-stream.js judges it with zero changes (it pins
// meta.{frames,seed,p1,p2,stage,cpu,difficulty,fdlibm,seedRandom} and the
// trace basename), plus meta.p3/meta.p4 which port/goldens-m4/
// freeze-stream-m4.js pins into params for four-port goldens.
"use strict";

const fs = require("fs");
const http = require("http");
const path = require("path");

const REPO = path.join(__dirname, "..", "..");
const HARNESS = path.join(REPO, "oracle", "harness");
const { chromium } = require(path.join(HARNESS, "node_modules", "playwright"));

function arg(name, dflt) {
  const i = process.argv.indexOf("--" + name);
  if (i === -1) return dflt;
  const v = process.argv[i + 1];
  return v === undefined || v.startsWith("--") ? true : v;
}

function die(msg) {
  console.error("run-4p: " + msg);
  process.exit(1);
}

function intArg(name, dflt, lo, hi, what) {
  const raw = arg(name, String(dflt));
  const v = parseInt(raw, 10);
  if (!Number.isInteger(v) || String(v) !== String(raw).trim() || v < lo || v > hi) {
    die(`--${name} must be an integer ${lo}-${hi} (${what}); got: ${raw}`);
  }
  return v;
}

// A port character, or null for an ABSENT port. STRICT: only "none" or a
// bare 0-4 — a typo can never silently become "absent" (which would
// record a 3-port stream under a 4-port golden's name).
function portArg(name) {
  const raw = arg(name, "none");
  if (raw === "none") return null;
  if (!/^[0-4]$/.test(String(raw))) {
    die(`--${name} must be 0-4 or the literal "none"; got: ${raw}`);
  }
  return parseInt(raw, 10);
}

const DIST_ROOT = path.resolve(arg("dist", ""));
const TRACE = arg("trace", "");
const FRAMES = intArg("frames", 3600, 1, 1000000, "frames to simulate");
const SEED = intArg("seed", 42, 0, 4294967295, "Math.random seed");
const OUT = arg("out", "run.json");
const P1 = intArg("p1", 2, 0, 4, "port 0 character");
const P2 = intArg("p2", 0, 0, 4, "port 1 character");
const P3 = portArg("p3");
const P4 = portArg("p4");
const STAGE = intArg("stage", 0, 0, 5,
  "stage: 0 battlefield 1 ystory 2 pstadium 3 dreamland 4 fdest 5 fountain");
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
    die("--dist must point at a built meleelight clone (dist/meleelight.html); " +
      "run oracle/build-upstream.sh first");
  }
  if (!TRACE || TRACE === true || !fs.existsSync(TRACE)) {
    die("--trace <file> is required (input trace JSON)");
  }
  const trace = JSON.parse(fs.readFileSync(TRACE, "utf8"));

  // TRACE/CONFIG AGREEMENT (whitelist rule, PROCESS §3): a present port
  // must have an input column and an absent port must not. Without this
  // a 4-port config fed a 2-column trace would record a stream in which
  // ports 2/3 stand still forever — a silently degenerate golden.
  const wantCol = [true, true, P3 !== null, P4 !== null];
  if (!Array.isArray(trace) || trace.length < 1) die("trace is not a nonempty array");
  trace.forEach((row, f) => {
    if (!Array.isArray(row) || row.length !== 4) {
      die(`trace frame ${f} is not an array of exactly 4 entries`);
    }
    for (let s = 0; s < 4; s++) {
      const present = row[s] !== null;
      if (present !== wantCol[s]) {
        die(`trace frame ${f} slot ${s} is ${present ? "present" : "null"} but ` +
          `the port config wants it ${wantCol[s] ? "present" : "null"}`);
      }
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
  page.on("pageerror", (e) => console.error("[pageerror]", e.message));
  page.on("console", (m) => {
    if (m.type() === "error" && !m.text().includes("Failed to load resource")) {
      console.error("[console.error]", m.text());
    }
  });

  // The run.js init pipeline, byte-for-byte (the recording is only a
  // golden if the shims are the golden shims): seeded mulberry32 +
  // virtual clock + the vendored fdlibm Math shim, all default-on.
  await page.addInitScript({
    content: "window.__harnessConfig = " +
      JSON.stringify({ seedRandom: true, seed: SEED, virtualClock: true,
        fdlibm: true, captureMath: false }) + ";",
  });
  await page.addInitScript({
    path: path.join(REPO, "port", "fdlibm", "fdlibm.js"),
  });
  await page.addInitScript({ path: path.join(HARNESS, "init.js") });
  await page.addInitScript({ path: path.join(HARNESS, "pagelib.js") });

  await page.goto(`http://localhost:${port}/dist/meleelight.html`);
  await page.waitForFunction(
    "window.__harness && window.__nextInputBuffers", null, { timeout: 120000 });

  // ONE setupMatch, exactly as run.js does it (startGame burns the single
  // legitimate off-step seeded draw, CHECKSUM.md §6 — calling setup twice
  // would burn two and break rngCallsOutsideStep == 1).
  await page.evaluate(({ trace, chars, stage }) => {
    window.__trace = trace;
    window.__resetMathCalls(); // count sim exposure, not boot noise
    window.__harness.setupMatch({
      players: chars.map((c) => (c === null ? null : { type: 0, character: c })),
      stage: stage,
    });
  }, { trace, chars: [P1, P2, P3, P4], stage: STAGE });

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

  const coverage = await page.evaluate(() => window.__coverage());
  const captured = await page.evaluate(() => window.__captured);

  fs.mkdirSync(path.dirname(path.resolve(OUT)), { recursive: true });
  fs.writeFileSync(OUT, JSON.stringify({
    meta: {
      dist: DIST_ROOT, trace: TRACE, frames: FRAMES, seed: SEED,
      p1: P1, p2: P2, p3: P3, p4: P4, stage: STAGE,
      seedRandom: true, fdlibm: true, cpu: false,
      difficulty: null, wallMs: wall,
      browser: browser.browserType().name(), version: browser.version(),
    },
    coverage,
    captured,
    frames,
  }));
  console.log(`${OUT}: ${FRAMES} frames in ${wall}ms ` +
    `(${(FRAMES / (wall / 1000)).toFixed(0)} fps), ports=` +
    `[${[P1, P2, P3, P4].map((c) => (c === null ? "-" : c)).join(",")}], ` +
    `rngCalls=${coverage.rngCalls}`);
  console.log("coverage:", Object.keys(coverage.actionStatesSeen).sort().join(","));
  console.log("maxArticles:", coverage.maxArticles,
    "stocks:", coverage.stocks, "percents:", coverage.percents);

  await browser.close();
  srv.close();
}

main().catch((e) => { console.error(e); process.exit(1); });
