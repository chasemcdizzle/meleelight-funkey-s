#!/usr/bin/env node
// Module-boundary capture runner (M2-CAL rig, generalized for M2):
// replays a golden through the browser oracle harness
// (oracle/harness/{init,pagelib}.js VERBATIM — oracle/ is never modified)
// while recording every call crossing the exported boundary of the chosen
// capture SPEC's modules. See port/sim/calib/FORMAT.md.
//
// Usage:
//   node run-capture.js --golden g01 [--spec envcoll|util] [--dist <clone-root>] \
//     [--out-jsonl build/g01.<spec>.jsonl] [--out-run <run.json>]
//
// --spec envcoll (default, M2-CAL boundary — CLI/output byte-compatible
//   with the original runner; run JSON keeps meta.envcollCapture);
// --spec <other> loads port/sim/calib/spec-<name>.js and writes
//   meta.capture = {spec, moduleIds, counts}.
//
// Params (trace/frames/seed/chars/stage/cpu) come ONLY from
// oracle/goldens/manifest.json (single param source, M0 convention).
// The emitted run JSON is verify-stream.js-compatible; the capture is
// only trustworthy if that run passes STREAM MATCH against the frozen
// golden (non-perturbation guard) — the check scripts enforce it.
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
const SPEC = arg("spec", "envcoll");
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
const SPEC_FILE = SPEC === "envcoll" ? null : path.join(__dirname, `spec-${SPEC}.js`);
if (SPEC_FILE && !fs.existsSync(SPEC_FILE)) {
  console.error(`--spec ${SPEC}: no such spec file ${SPEC_FILE}`);
  process.exit(1);
}
const TRACE = path.join(REPO, "oracle", "goldens", g.trace);
const OUT_JSONL = path.resolve(arg("out-jsonl",
  path.join(__dirname, "build", `${g.id}.${SPEC}.jsonl`)));
const OUT_RUN = path.resolve(arg("out-run",
  path.join(__dirname, "build",
    SPEC === "envcoll" ? `${g.id}.capture-run.json` : `${g.id}.${SPEC}-run.json`)));
const CHUNK = 120;

const MIME = {
  ".html": "text/html", ".js": "text/javascript", ".css": "text/css",
  ".json": "application/json", ".png": "image/png", ".jpg": "image/jpeg",
  ".svg": "image/svg+xml", ".wav": "audio/wav", ".mp3": "audio/mpeg",
};

// Serve the dist untouched EXCEPT dist/js/main.js, whose served bytes get
// exactly two injections. Disk is never written; nothing else changes.
// (1) the webpack bootstrap exposes the module cache as window.__wpCache;
// (2) fountain's module-PRIVATE `platformStates` (a closure `let` no
//     export reaches — src/stages/vs-stages/fountain.js:22) gains a
//     read-only window getter right after its declaration (M2 task 14:
//     the platforms spec records/chain-verifies it per rule 18). The
//     injected statement carries no quotes or newlines, so it is safe
//     inside the webpack eval-wrapped module string; behavior-neutral
//     (pure exposure — only the capture spec ever CALLS the getter).
const BOOT_LINE = "var installedModules = {};";
const BOOT_HOOK = BOOT_LINE + " window.__wpCache = installedModules;";
const PS_LINE =
  "var platformStates = [{ state: \\\"moving\\\", timer: 0, destination: " +
  "22.125 }, { state: \\\"moving\\\", timer: 0, destination: 16.125 }];";
const PS_HOOK = PS_LINE +
  " window.__mpFountainPS = function () { return platformStates; };";

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
        let hooked = src.slice(0, i) + BOOT_HOOK + src.slice(i + BOOT_LINE.length);
        const j = hooked.indexOf(PS_LINE);
        if (j === -1 || hooked.indexOf(PS_LINE, j + 1) !== -1) {
          res.writeHead(500);
          res.end("capture: fountain platformStates declaration not found exactly once in main.js");
          return;
        }
        hooked = hooked.slice(0, j) + PS_HOOK + hooked.slice(j + PS_LINE.length);
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
  if (SPEC_FILE) await page.addInitScript({ path: SPEC_FILE });

  await page.goto(`http://localhost:${port}/dist/meleelight.html`);
  await page.waitForFunction(
    "window.__harness && window.__nextInputBuffers", null, { timeout: 120000 });

  // Install the spec; the spec itself asserts its expected wrapped count
  // (envcoll: 13 — the original M2-CAL assertion, now spec-declared).
  const install = await page.evaluate((s) => window.__capInstallSpec(s), SPEC);
  const expectWrapped = await page.evaluate((s) => window.__capSpecs[s].expectWrapped, SPEC);
  if (install.wrapped !== expectWrapped) {
    throw new Error(`capture: expected ${expectWrapped} wrapped exports, got ${install.wrapped}`);
  }

  // Optional spec-defined synthetic-domain sweep (M2 task 3, FORMAT.md
  // "synthetic-domain sweep"): a DETERMINISTIC set of extra calls through
  // the WRAPPED exports, executed before setupMatch so the records land at
  // frame 0 ahead of every sim-step record. Pure functions only — the
  // verify-stream non-perturbation guard still judges the whole run.
  const swept = await page.evaluate((s) => {
    const spec = window.__capSpecs[s];
    return spec.sweep ? spec.sweep() : 0;
  }, SPEC);
  if (swept > 0) console.log(`sweep: ${swept} synthetic-domain calls recorded`);

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
  const setupRecords = await page.evaluate(() => window.__capDrain());
  if (setupRecords.length > 0) fs.appendFileSync(OUT_JSONL, setupRecords + "\n");

  const frames = [];
  const t0 = Date.now();
  for (let done = 0; done < g.frames; done += CHUNK) {
    const n = Math.min(CHUNK, g.frames - done);
    const chunk = await page.evaluate((nn) => window.__runFrames(nn, {}), n);
    frames.push(...chunk);
    const records = await page.evaluate(() => window.__capDrain());
    if (records.length > 0) fs.appendFileSync(OUT_JSONL, records + "\n");
  }
  const wall = Date.now() - t0;

  // Optional spec-defined post-run assertion (M2 task 5): e.g. the physics
  // spec re-dumps the actionStates flag table and hard-fails on drift
  // (soundness of its frame-0 asFlags record). Throws = capture fails.
  await page.evaluate((s) => {
    const spec = window.__capSpecs[s];
    return spec.finalCheck ? spec.finalCheck() : 0;
  }, SPEC);

  const coverage = await page.evaluate(() => window.__coverage());
  const counts = await page.evaluate(() => window.__capCounts);

  // meta: envcoll keeps the original M2-CAL shape (check-capture-pins.js
  // reads meta.envcollCapture); other specs use the generic meta.capture.
  const capMeta = SPEC === "envcoll"
    ? { envcollCapture: { moduleId: install.moduleIds.environmentalCollision, counts: counts } }
    : { capture: { spec: SPEC, moduleIds: install.moduleIds, counts: counts } };

  fs.writeFileSync(OUT_RUN, JSON.stringify({
    meta: Object.assign({
      dist: DIST_ROOT, trace: TRACE, frames: g.frames, seed: g.seed,
      p1: g.p1, p2: g.p2, stage: g.stage,
      seedRandom: true, fdlibm: true, cpu: g.cpu,
      difficulty: g.cpu ? g.difficulty : null, wallMs: wall,
      browser: browser.browserType().name(), version: browser.version(),
    }, capMeta),
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
