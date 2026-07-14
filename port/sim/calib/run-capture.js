#!/usr/bin/env node
// M2-CAL capture runner: replays a golden through the browser oracle
// harness (oracle/harness/{init,pagelib}.js VERBATIM — oracle/ is never
// modified) while recording every call crossing the exported boundary of
// src/physics/environmentalCollision.js. See port/sim/calib/FORMAT.md.
//
// Usage:
//   node run-capture.js --golden g01 [--dist <clone-root>] \
//     [--out-jsonl build/g01.envcoll.jsonl] [--out-run build/g01.capture-run.json]
//
// Params (trace/frames/seed/chars/stage/cpu) come ONLY from
// oracle/goldens/manifest.json (single param source, M0 convention).
// The emitted run JSON is verify-stream.js-compatible; the capture is
// only trustworthy if that run passes STREAM MATCH against the frozen
// golden (non-perturbation guard) — check-capture.sh enforces it.
"use strict";

const fs = require("fs");
const http = require("http");
const path = require("path");

const REPO = path.resolve(__dirname, "..", "..", "..");
const HARNESS = path.join(REPO, "oracle", "harness");
const { chromium } = require(path.join(HARNESS, "node_modules", "playwright"));

function arg(name, dflt) {
  const i = process.argv.indexOf("--" + name);
  if (i === -1) return dflt;
  const v = process.argv[i + 1];
  return v === undefined || v.startsWith("--") ? true : v;
}

const GOLDEN_ID = arg("golden", "");
const DIST_ROOT = path.resolve(arg("dist",
  process.env.MELEELIGHT_CLONE ||
  path.join(process.env.HOME, ".cache", "meleelight-funkey-s", "upstream")));
const manifest = JSON.parse(fs.readFileSync(
  path.join(REPO, "oracle", "goldens", "manifest.json"), "utf8"));
const g = manifest.goldens.find((x) => x.id === GOLDEN_ID);
if (!g) {
  console.error(`--golden must be one of: ${manifest.goldens.map((x) => x.id).join(", ")}`);
  process.exit(1);
}
const TRACE = path.join(REPO, "oracle", "goldens", g.trace);
const OUT_JSONL = path.resolve(arg("out-jsonl",
  path.join(__dirname, "build", `${g.id}.envcoll.jsonl`)));
const OUT_RUN = path.resolve(arg("out-run",
  path.join(__dirname, "build", `${g.id}.capture-run.json`)));
const CHUNK = 120;

const MIME = {
  ".html": "text/html", ".js": "text/javascript", ".css": "text/css",
  ".json": "application/json", ".png": "image/png", ".jpg": "image/jpeg",
  ".svg": "image/svg+xml", ".wav": "audio/wav", ".mp3": "audio/mpeg",
};

// Serve the dist untouched EXCEPT dist/js/main.js, whose served bytes get
// exactly one injection into the webpack bootstrap: expose the module
// cache as window.__wpCache. Disk is never written; nothing else changes.
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
          res.end("capture: webpack bootstrap line not found exactly once in main.js");
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
  if (!fs.existsSync(path.join(DIST_ROOT, "dist", "meleelight.html"))) {
    console.error("--dist must point at a built meleelight clone (run oracle/build-upstream.sh)");
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
  await context.route(/\/(sfx|music)\//, (r) => r.abort());
  const page = await context.newPage();
  page.on("pageerror", (e) => { console.error("[pageerror]", e.message); process.exitCode = 1; });
  page.on("console", (m) => {
    if (m.type() === "error" && !m.text().includes("Failed to load resource")) {
      console.error("[console.error]", m.text());
    }
  });

  // Identical init pipeline to oracle/harness/run.js golden recording:
  // seeded RNG, virtual clock, fdlibm shim ON.
  await page.addInitScript({
    content: "window.__harnessConfig = " + JSON.stringify({
      seedRandom: true, seed: g.seed, virtualClock: true, fdlibm: true,
      captureMath: false }) + ";",
  });
  await page.addInitScript({ path: path.join(REPO, "port", "fdlibm", "fdlibm.js") });
  await page.addInitScript({ path: path.join(HARNESS, "init.js") });
  await page.addInitScript({ path: path.join(HARNESS, "pagelib.js") });
  await page.addInitScript({ path: path.join(__dirname, "capturelib.js") });

  await page.goto(`http://localhost:${port}/dist/meleelight.html`);
  await page.waitForFunction(
    "window.__harness && window.__nextInputBuffers", null, { timeout: 120000 });

  const install = await page.evaluate(() => window.__envcollInstall());
  if (install.wrapped !== 13) {
    throw new Error("capture: expected 13 wrapped exports, got " + install.wrapped);
  }

  fs.mkdirSync(path.dirname(OUT_JSONL), { recursive: true });
  fs.writeFileSync(OUT_JSONL, "");

  await page.evaluate(({ trace, p1, p2, stage, cpu, difficulty }) => {
    window.__trace = trace;
    window.__resetMathCalls();
    window.__harness.setupMatch({
      players: [
        { type: 0, character: p1 },
        cpu ? { type: 1, character: p2, difficulty: difficulty }
            : { type: 0, character: p2 },
        null, null,
      ],
      stage: stage,
    });
  }, { trace, p1: g.p1, p2: g.p2, stage: g.stage, cpu: g.cpu, difficulty: g.difficulty || 5 });

  // Drain the setup-time (frame 0) records before the first tick.
  const setupRecords = await page.evaluate(() => window.__envcollDrain());
  if (setupRecords.length > 0) fs.appendFileSync(OUT_JSONL, setupRecords + "\n");

  const frames = [];
  const t0 = Date.now();
  for (let done = 0; done < g.frames; done += CHUNK) {
    const n = Math.min(CHUNK, g.frames - done);
    const chunk = await page.evaluate((nn) => window.__runFrames(nn, {}), n);
    frames.push(...chunk);
    const records = await page.evaluate(() => window.__envcollDrain());
    if (records.length > 0) fs.appendFileSync(OUT_JSONL, records + "\n");
  }
  const wall = Date.now() - t0;

  const coverage = await page.evaluate(() => window.__coverage());
  const counts = await page.evaluate(() => window.__envcollCounts);

  fs.writeFileSync(OUT_RUN, JSON.stringify({
    meta: {
      dist: DIST_ROOT, trace: TRACE, frames: g.frames, seed: g.seed,
      p1: g.p1, p2: g.p2, stage: g.stage,
      seedRandom: true, fdlibm: true, cpu: g.cpu,
      difficulty: g.cpu ? g.difficulty : null, wallMs: wall,
      browser: browser.browserType().name(), version: browser.version(),
      envcollCapture: { moduleId: install.moduleId, counts: counts },
    },
    coverage,
    frames,
  }));

  const total = Object.keys(counts).reduce((a, k) => a + counts[k], 0);
  console.log(`${g.id}: ${g.frames} frames in ${wall}ms; ` +
    `${total} boundary records -> ${path.relative(process.cwd(), OUT_JSONL)}`);
  console.log("counts: " + Object.keys(counts).sort()
    .map((k) => `${k}=${counts[k]}`).join(" "));

  await browser.close();
  srv.close();
}

main().catch((e) => { console.error(e); process.exit(1); });
