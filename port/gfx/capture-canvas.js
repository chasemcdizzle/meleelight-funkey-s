#!/usr/bin/env node
// port/gfx/capture-canvas.js — browser render-reference capture (M3 task 3).
//
// Replays a golden through the browser oracle harness
// (oracle/harness/{init,pagelib}.js VERBATIM — oracle/ is never modified;
// the run-capture.js served-bytes __wpCache hook reaches the render
// modules) with the reduced gameMode-3 render sequence executed after
// EVERY step, and dumps per-frame fg1|fg2 ink masks (+ composite PNGs)
// for the sampled frames. Also dumps the GFXDATA1 render data plane
// (palettes/pPal/flashOnLCancel/reverseModel) EXECUTED out of the page.
//
// The emitted run JSON is verify-stream.js-compatible; the capture is
// only trustworthy if that run passes STREAM MATCH against the frozen
// golden (non-perturbation guard) — check-render.sh enforces it.
//
// Usage:
//   node capture-canvas.js --golden g01 --frames-list 30,100,...
//     --out-dir build/canvas --out-run build/g01.render-run.json
//     --gfxdata build/gfxdata.txt [--dist <clone-root>]
//
// Canvas dumps are NON-FROZEN reference material (browser canvas
// rasterization is not bit-deterministic): regenerated per check run,
// never committed, judged only through the structural IoU.
"use strict";

const crypto = require("crypto");
const fs = require("fs");
const http = require("http");
const path = require("path");

function sha256Hex(buf) {
  return crypto.createHash("sha256").update(buf).digest("hex");
}

const REPO = path.resolve(__dirname, "..", "..");
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
// Capture-INPUT CLOSURE (review-46 fix 2): every static repo file this
// capture loads/reads, enumerated ONCE in capture-closure.js. Load paths
// below resolve FROM this map (a new input cannot be loaded without
// joining the closure) and the reuse sidecar binds sha256 of every
// member. manifest.json above is the documented bootstrap exception
// (it parameterizes the map); it is still a member and sidecar-bound.
const { closureFiles } = require(path.join(__dirname, "capture-closure.js"));
const CLOSURE = closureFiles(g.trace);
// Pre-consumption closure SNAPSHOT (review-48 round-3 fix, iter 49):
// sha256 every member NOW, before ANY member is consumed through the
// map (expected-render.json parse below, the trace read, the page init
// scripts), so the sidecar binds the exact bytes this capture consumed
// — never whatever is on disk after the multi-second replay (an editor
// save mid-capture would otherwise bind NEW bytes to masks produced
// from OLD bytes, letting a later reuse pass falsely). Re-verified at
// sidecar-write time; any drift → loud death, NO sidecar written.
// (manifest.json + the two driver files are consumed milliseconds
// earlier at process boot — the same bootstrap window as the
// documented manifest exception; they are snapshot here all the same.)
const CLOSURE_SNAP = {};
for (const k of Object.keys(CLOSURE)) {
  CLOSURE_SNAP[k] = sha256Hex(fs.readFileSync(CLOSURE[k]));
}
const TRACE = CLOSURE["oracle/goldens/" + g.trace];
const OUT_DIR = arg("out-dir", "");
const OUT_RUN = arg("out-run", "");
const GFXDATA = arg("gfxdata", "");
const VFXDATA = arg("vfxdata", "");
const GLYPHS = arg("glyphs", "");
const FRAMES_LIST = String(arg("frames-list", ""));
if (!OUT_DIR || OUT_DIR === true || !OUT_RUN || OUT_RUN === true ||
    !GFXDATA || GFXDATA === true || !VFXDATA || VFXDATA === true ||
    !GLYPHS || GLYPHS === true || !FRAMES_LIST) {
  console.error("capture-canvas: --out-dir, --out-run, --gfxdata, --vfxdata, --glyphs, --frames-list are required");
  process.exit(1);
}
// Corpus pin (review-44 fix 1): duplicates are REJECTED — every sampled
// frame must appear exactly once, so masksWritten == list length is an
// exact-coverage assertion, never a dedup illusion.
const sampled = {};
const sampledList = [];
for (const s of FRAMES_LIST.split(",")) {
  const v = parseInt(s, 10);
  if (!Number.isInteger(v) || v < 1 || v > g.frames || String(v) !== s.trim()) {
    console.error(`capture-canvas: bad sampled frame '${s}'`);
    process.exit(1);
  }
  if (sampled[v]) {
    console.error(`capture-canvas: duplicate sampled frame ${v} (corpus pin: each frame exactly once)`);
    process.exit(1);
  }
  sampled[v] = true;
  sampledList.push(v);
}
const CHUNK = 120;

// Console fail-closed allowlist (review-44 fix 4): frozen in
// expected-render.json; ANY page console.error outside it kills the
// capture immediately.
const EXPECTED_RENDER = JSON.parse(
  fs.readFileSync(CLOSURE["port/gfx/expected-render.json"], "utf8"));
// U1 tunnel-leg corpus (frozen in expected-render.json; iou.js and
// check-render.sh carry their own independent copies of this assert —
// the twin-pin class, so a silently shrunk list dies on every side).
const TUNNEL_FRAMES = EXPECTED_RENDER.bgTunnelFrames;
if (!Array.isArray(TUNNEL_FRAMES) || TUNNEL_FRAMES.join(",") !== "1,2,3,41,81,121,161,200,201,202,240") {
  console.error("capture-canvas: bgTunnelFrames pin violated — [" +
    String(TUNNEL_FRAMES) + "] != the hard-pinned reviewed 11-frame list");
  process.exit(1);
}

// U3 article-plane pin (same twin-pin class; iou.js and check-render.sh
// carry their own copies). This side produces the browser plane, so it
// asserts the frozen values BEFORE the browser is launched.
if (EXPECTED_RENDER.artFrameCount !== 4 ||
    !Array.isArray(EXPECTED_RENDER.artFrames) ||
    EXPECTED_RENDER.artFrames.join(",") !== "184,1234,1237,1238" ||
    !EXPECTED_RENDER.artFrameFloors ||
    typeof EXPECTED_RENDER.artFrameFloors !== "object" ||
    Array.isArray(EXPECTED_RENDER.artFrameFloors) ||
    JSON.stringify(EXPECTED_RENDER.artFrameFloors) !== '{"f0184":0.6707,"f1234":0.6708,"f1237":0.6455,"f1238":0.6455}') {
  console.error("capture-canvas: article plane pin violated — got " +
    [JSON.stringify(EXPECTED_RENDER.artFrameFloors), EXPECTED_RENDER.artFrameCount,
     String(EXPECTED_RENDER.artFrames)].join("/") + ", pinned f0184=0.6707,f1234=0.6708,f1237=0.6455,f1238=0.6455/4/184,1234,1237,1238");
  process.exit(1);
}
if (EXPECTED_RENDER.iouThreshold !== 0.92) {
  console.error("capture-canvas: fg iouThreshold pin violated — got " +
    String(EXPECTED_RENDER.iouThreshold) + ", pinned 0.92 (C28 re-freeze)");
  process.exit(1);
}
// Browser identity pin (review-134 r1 M1). Twin-pin class: check-render.sh
// carries its own copy and applies it to the SIDECAR, so the reuse hatch is
// covered too.
const BROWSER_PIN = EXPECTED_RENDER.browserPin;
if (!BROWSER_PIN || BROWSER_PIN.name !== "chromium" || BROWSER_PIN.channel !== "chrome" ||
    BROWSER_PIN.version !== "151.0.7922.71") {
  console.error("capture-canvas: browserPin malformed or not the frozen value " +
    "(chromium / chrome / 151.0.7922.71)");
  process.exit(1);
}

const CONSOLE_ALLOW = EXPECTED_RENDER.consoleErrorAllowlist;
if (!Array.isArray(CONSOLE_ALLOW) || CONSOLE_ALLOW.length === 0) {
  console.error("capture-canvas: consoleErrorAllowlist missing/malformed in expected-render.json");
  process.exit(1);
}
// Match-pattern floor (review-46 fix 1): an empty / whitespace-only /
// too-short substring is an over-broad matcher (textIncludes:"" would
// allowlist EVERY console error; urlIncludes:"" removes URL scoping) —
// a config error, die loud at load, BEFORE any browser launch. Floor:
// trimmed length >= 8 for textIncludes AND (when present) urlIncludes.
// The pinned url patterns are the MEASURED served-URL forms
// ("/dist/sfx/", "/dist/music/" — .loop/m3-task3r46-capture-measure.log).
const MIN_PATTERN_LEN = 8;
const badPattern = (v) => typeof v !== "string" || v.trim().length < MIN_PATTERN_LEN;
for (const a of CONSOLE_ALLOW) {
  if (!a || badPattern(a.textIncludes) ||
      (a.urlIncludes !== undefined && badPattern(a.urlIncludes))) {
    console.error("capture-canvas: consoleErrorAllowlist entry REJECTED " +
      `(textIncludes and any urlIncludes must be strings of trimmed length >= ${MIN_PATTERN_LEN}; ` +
      "an over-broad pattern is a config error): " + JSON.stringify(a));
    process.exit(1);
  }
}

// Injection-set pin, BROWSER SIDE (review-65 M1, iter 67): asserted here
// PRE-browser (no server/page work yet), independently of
// check-render.sh's copy — the runtime inject table this capture will
// spawn must equal the frozen reviewed injectPin: exact ordered name
// list, uniqueness, count, frame identity, and the injection frame must
// be one of the frames this capture actually samples (else the injected
// coverage would never be judged). A dropped/renamed effect is a loud
// pin death on THIS side even if the other side's copy is broken.
{
  const pin = EXPECTED_RENDER.injectPin;
  const inj = EXPECTED_RENDER.inject;
  if (!pin || !Array.isArray(pin.names) || !Array.isArray(pin.inkNames) ||
      !Number.isInteger(pin.frame) || !Number.isInteger(pin.count)) {
    console.error("capture-canvas: injectPin missing/malformed in expected-render.json");
    process.exit(1);
  }
  if (!inj || !Number.isInteger(inj.frame) || !Array.isArray(inj.configs)) {
    console.error("capture-canvas: inject table missing/malformed in expected-render.json");
    process.exit(1);
  }
  const names = inj.configs.map((c) => c && c.name);
  if (names.some((n) => typeof n !== "string" || n.length === 0) ||
      new Set(names).size !== names.length) {
    console.error("capture-canvas: inject pin violated (missing/duplicate config name)");
    process.exit(1);
  }
  if (pin.count !== pin.names.length || names.length !== pin.count ||
      names.join(" ") !== pin.names.join(" ")) {
    console.error("capture-canvas: inject pin violated — runtime configs [" +
      names.join(", ") + "] != pinned reviewed set [" + pin.names.join(", ") + "]");
    process.exit(1);
  }
  if (inj.frame !== pin.frame) {
    console.error(`capture-canvas: inject pin violated (inject.frame ${inj.frame} != pinned ${pin.frame})`);
    process.exit(1);
  }
  if (!sampledList.includes(inj.frame)) {
    console.error(`capture-canvas: inject pin violated (inject.frame ${inj.frame} not in the sampled frame list — the injection would never be captured)`);
    process.exit(1);
  }
  if (pin.inkNames.length === 0 || new Set(pin.inkNames).size !== pin.inkNames.length ||
      !pin.inkNames.every((n) => pin.names.includes(n))) {
    console.error("capture-canvas: inject pin violated (inkNames must be a nonempty unique subset of names)");
    process.exit(1);
  }
  // Exact ordered inkNames pin (review-65 r2 M5): hard-coded here and in
  // check-render.sh + iou.js (twin-pin class) — a name silently dropped
  // from expected-render.json's inkNames must die on every side.
  if (pin.inkNames.length !== 5 ||
      pin.inkNames.join(" ") !== "firefoxcharge firefoxtail shine dashDust groundBounce") {
    console.error("capture-canvas: inject pin violated — inkNames [" + pin.inkNames.join(", ") +
      "] != the hard-pinned reviewed 5-name set");
    process.exit(1);
  }
}

const MIME = {
  ".html": "text/html", ".js": "text/javascript", ".css": "text/css",
  ".json": "application/json", ".png": "image/png", ".jpg": "image/jpeg",
  ".svg": "image/svg+xml", ".wav": "audio/wav", ".mp3": "audio/mpeg",
};

// Serve the dist untouched EXCEPT dist/js/main.js, whose served bytes get
// the run-capture.js webpack-bootstrap hook (window.__wpCache). Disk is
// never written.
//
// Oracle build digest (review-44 fix 2): serve() records the sha256 of
// the EXACT bytes sent for every 200 response (the hooked main.js is
// hashed AS SERVED — the hook is committed code, so the digest stays
// deterministic). The run JSON meta carries a digest over the sorted
// (path, hash) list; check-render.sh asserts it against the frozen
// servedDistSha256 pin in expected-render.json BEFORE judging.
const BOOT_LINE = "var installedModules = {};";
const BOOT_HOOK = BOOT_LINE + " window.__wpCache = installedModules;";

// U1: a SECOND served-bytes hook, same mechanism and same
// unique-match-or-die discipline. stagerender.js keeps its background
// state module-private (bgStars / bgPos / direction / circleSize /
// bgSparkle / ang are bare `var`s, none exported), and pairing our
// renderer against it needs both a read of that state and an in-place
// re-seed of bgStars. The accessor is appended after the LAST of those
// declarations so every one of them is in scope; it is quote-free and
// backslash-free because the module body is carried inside a webpack
// eval() string literal in the served bytes.
const BG_LINE = "var backgroundType = exports.backgroundType = 0;";
const BG_HOOK = BG_LINE +
  " window.__bgState = function(){return [bgStars, bgPos, direction, circleSize, bgSparkle, ang, backgroundType];};";

// U1 / review-u1 r1 M1: re-seeding bgStars needs stagerender.js:16-19's
// star-initialisation loop as a CALLABLE. Restating those two position
// expressions in our own code would make the judge blind to a matching
// error in gfx_bg.c's copy of them, so instead we SOURCE-TRANSFORM: copy
// the loop's exact served bytes into a function body. Zero transcription
// — the same "extract the reference from the oracle's own bytes, never
// retype it" rule the ml_fmt/ml_ser differential follows. The `var
// bgStars = [];` declaration is deliberately NOT included: re-declaring
// it inside the function would shadow the module array instead of
// re-seeding it.
const RESEED_FROM = "for (var p = 0; p < 20; p++) {";
const RESEED_TO = "var bgSparkle = 3;";

function reseedHook(src) {
  const a = src.indexOf(RESEED_FROM);
  const b = src.indexOf(RESEED_TO);
  if (a === -1 || src.indexOf(RESEED_FROM, a + 1) !== -1 ||
      b === -1 || src.indexOf(RESEED_TO, b + 1) !== -1 || b <= a) {
    return null;
  }
  const block = src.slice(a, b); // upstream's own bytes, verbatim
  return { at: b, line: "", hook: " window.__bgReseed = function(){ " + block + " };" };
}

const servedSha = {}; // urlPath -> sha256 hex of the exact bytes sent

function servedDigest() {
  const files = Object.keys(servedSha).sort();
  const h = crypto.createHash("sha256");
  for (const k of files) h.update(k + "\n" + servedSha[k] + "\n");
  return { digest: h.digest("hex"), files: files };
}

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
        const j = src.indexOf(BG_LINE);
        if (j === -1 || src.indexOf(BG_LINE, j + 1) !== -1) {
          res.writeHead(500);
          res.end("capture: stagerender bg-state line not found exactly once in main.js");
          return;
        }
        const rs = reseedHook(src);
        if (!rs) {
          res.writeHead(500);
          res.end("capture: stagerender star-init block not found exactly once in main.js");
          return;
        }
        // Apply every hook by DESCENDING offset so an earlier edit cannot
        // shift a later one's index.
        let patched = src;
        for (const [at, line, hook] of [[i, BOOT_LINE, BOOT_HOOK], [j, BG_LINE, BG_HOOK],
                                        [rs.at, rs.line, rs.hook]]
               .sort((a, b) => b[0] - a[0])) {
          patched = patched.slice(0, at) + hook + patched.slice(at + line.length);
        }
        const hooked = Buffer.from(patched, "utf8");
        servedSha[urlPath] = sha256Hex(hooked);
        res.writeHead(200, { "Content-Type": "text/javascript" });
        res.end(hooked);
        return;
      }
      const body = fs.readFileSync(fp);
      servedSha[urlPath] = sha256Hex(body);
      res.writeHead(200, { "Content-Type": MIME[path.extname(fp)] || "application/octet-stream" });
      res.end(body);
    });
    srv.listen(0, "127.0.0.1", () => resolve(srv));
  });
}

let pageErrored = false;

async function main() {
  if (!fs.existsSync(path.join(DIST_ROOT, "dist", "meleelight.html"))) {
    console.error("--dist must point at a built meleelight clone (run oracle/build-upstream.sh)");
    process.exit(1);
  }
  const trace = JSON.parse(fs.readFileSync(TRACE, "utf8"));
  const srv = await serve(DIST_ROOT);
  const port = srv.address().port;

  // BROWSER IDENTITY IS FROZEN (review-134 r1 M1). This used to try channel
  // "chrome" and SILENTLY fall back to Playwright's bundled Chromium — two
  // different rasterizers behind one browserType().name() of "chromium", so
  // a capture could be produced by an engine nobody measured. Every bound in
  // expected-render.json (fg 0.92, the PER-FRAME article floors
  // artFrameFloors, the three background legs) is a measured property of ONE
  // rasterizer, so: one launch, no fallback,
  // and the engine's name AND EXACT version must equal the frozen pin (see
  // the r2 M1 note below the launch — a major-version prefix is NOT enough).
  //
  // When Chrome updates this FIRES. That is the intended behaviour, not a
  // nuisance: the margins are properties of the engine that produced them.
  // The response is to re-measure (>=6 fresh captures, record the spread)
  // and re-freeze browserPin plus any bound that actually moved — never to
  // widen a bound to silence it.
  const browser = await chromium.launch({ channel: "chrome", headless: true });
  const browserId = { name: browser.browserType().name(), version: browser.version() };
  // EXACT version equality (review-134 r2 M1): Skia rasterization changes at
  // PATCH level, and the fg margin is only 0.0008 IoU — about 3 device cells
  // on the MIN frame, where one cell is ~0.00026 IoU (r3 M4 measurement, with
  // the bound now at 0.92) — so a major-version prefix would have let an
  // unmeasured rasterizer through.
  if (browserId.name !== BROWSER_PIN.name || browserId.version !== BROWSER_PIN.version) {
    console.error("capture-canvas: browser identity pin violated — launched " +
      browserId.name + " " + browserId.version + ", pinned " + BROWSER_PIN.name +
      " " + BROWSER_PIN.version + ". The render bounds were measured on the " +
      "pinned engine; re-measure and re-freeze rather than widening them.");
    await browser.close();
    process.exit(1);
  }
  const context = await browser.newContext();
  await context.route(/\/(sfx|music)\//, (r) => r.abort());
  const page = await context.newPage();
  page.on("pageerror", (e) => {
    console.error("[pageerror]", e.message);
    process.exitCode = 1;
    pageErrored = true; // and the sidecar is withheld (review-u1 r6)
  });
  // Fail-closed console policy (review-44 fix 4): a caught render error or
  // missing render asset surfaces as console.error while masks stay
  // structurally valid — so ONLY the frozen allowlist (CLAUDE.md's
  // expected upstream noise + our own deliberate sfx/music route aborts)
  // may pass; anything else kills the capture immediately. The old
  // blanket "Failed to load resource" suppression is gone.
  page.on("console", (m) => {
    if (m.type() !== "error") return;
    const text = m.text();
    const loc = m.location();
    const url = (loc && loc.url) || "";
    for (const a of CONSOLE_ALLOW) {
      if (text.includes(a.textIncludes) &&
          (a.urlIncludes === undefined || url.includes(a.urlIncludes))) {
        console.error("[console.error allowlisted]", text, url);
        return;
      }
    }
    console.error("[console.error UNLISTED -> fail-closed]", text, url);
    process.exit(1);
  });

  // Identical init pipeline to oracle/harness/run.js golden recording:
  // seeded RNG, virtual clock, fdlibm shim ON. __harnessNoRender stays
  // TRUE — the rAF renderTick draws nothing; OUR explicit sequence is
  // the only renderer, so frame pairing is exact.
  await page.addInitScript({
    content: "window.__harnessConfig = " + JSON.stringify({
      seedRandom: true, seed: g.seed, virtualClock: true, fdlibm: true,
      captureMath: false }) + ";",
  });
  // (paths resolved FROM the capture-input closure map — see fix-2 note)
  await page.addInitScript({ path: CLOSURE["port/fdlibm/fdlibm.js"] });
  await page.addInitScript({ path: CLOSURE["oracle/harness/init.js"] });
  await page.addInitScript({ path: CLOSURE["oracle/harness/pagelib.js"] });
  await page.addInitScript({ path: CLOSURE["port/gfx/gfx-pagelib.js"] });

  await page.goto(`http://localhost:${port}/dist/meleelight.html`);
  await page.waitForFunction(
    "window.__harness && window.__nextInputBuffers", null, { timeout: 120000 });

  await page.evaluate(() => window.__gfxFindModules());

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

  // U1: seed the mirrored background stream and put stagerender's bg
  // module state where gfx_bg_reset() leaves the C. Post-setupMatch (so
  // startGame's setBackgroundType/drawBackgroundInit have run) and
  // BEFORE the first render.
  await page.evaluate(() => window.__gfxBgInit());
  const bgGrad = await page.evaluate(() => window.__gfxBgGradColumn());
  fs.mkdirSync(OUT_DIR, { recursive: true });
  fs.writeFileSync(path.join(OUT_DIR, "bg1-grad.txt"), bgGrad);
  // M2: the type the PAGE selected, through upstream's own seeded
  // startGame draw. check-render.sh asserts the C selected the same.
  const bgType = await page.evaluate(() => window.__gfxBgType());
  if (bgType !== 0 && bgType !== 1) {
    console.error(`capture-canvas: page backgroundType is ${bgType} (want 0 or 1)`);
    process.exit(1);
  }
  fs.writeFileSync(path.join(OUT_DIR, "bg-type.txt"), String(bgType) + "\n");

  // GFXDATA1 (post-setup: actionStates registries are live)
  const gfxdata = await page.evaluate(() => window.__gfxDumpData());
  fs.mkdirSync(path.dirname(path.resolve(GFXDATA)), { recursive: true });
  fs.writeFileSync(GFXDATA, gfxdata);

  // VFXDATA1 + VFXGLYPHS1 (M4 task 2: executed template plane + the
  // browser-rasterized glyph atlas)
  const vfxdata = await page.evaluate(() => window.__gfxDumpVfxData());
  fs.mkdirSync(path.dirname(path.resolve(VFXDATA)), { recursive: true });
  fs.writeFileSync(VFXDATA, vfxdata);
  const glyphs = await page.evaluate(() => window.__gfxDumpGlyphs());
  fs.mkdirSync(path.dirname(path.resolve(GLYPHS)), { recursive: true });
  fs.writeFileSync(GLYPHS, glyphs);

  // Synthetic-coverage injection table (M4 task 2): frozen in
  // expected-render.json; the SAME configs spawn C-side (INJECT1,
  // generated by check-render.sh) at the same post-tick point.
  const INJECT = EXPECTED_RENDER.inject;
  if (!INJECT || !Number.isInteger(INJECT.frame) || INJECT.frame < 1 ||
      !Array.isArray(INJECT.configs) || INJECT.configs.length === 0) {
    console.error("capture-canvas: expected-render.json inject table missing/malformed");
    process.exit(1);
  }

  fs.mkdirSync(OUT_DIR, { recursive: true });

  const frames = [];
  let masksWritten = 0;
  const t0 = Date.now();

  const decodeMask = (b64s, what) => {
    const m = Buffer.from(b64s, "base64");
    if (m.length !== 1200 * 750) {
      console.error(`capture-canvas: ${what} is ${m.length} bytes (want 900000)`);
      process.exit(1);
    }
    return m;
  };
  const drain = async () => {
    const captured = await page.evaluate(() => window.__gfxDrainCaptured());
    for (const f of Object.keys(captured)) {
      const m = decodeMask(captured[f].mask, `frame ${f} mask`);
      fs.writeFileSync(path.join(OUT_DIR, `f${String(f).padStart(4, "0")}.mask.bin`), m);
      fs.writeFileSync(path.join(OUT_DIR, `f${String(f).padStart(4, "0")}.bg.bin`),
        decodeMask(captured[f].bg, `frame ${f} bg mask`));
      fs.writeFileSync(path.join(OUT_DIR, `f${String(f).padStart(4, "0")}.star.bin`),
        decodeMask(captured[f].star, `frame ${f} star mask`));
      // U3 article-only plane. Required on EVERY sampled frame — a null
      // here means the render was taken without the isolation, and a
      // missing plane must never read as "no articles this frame".
      if (typeof captured[f].art !== "string") {
        console.error(`capture-canvas: frame ${f} has no article plane — __gfxArticlePlane() is called unconditionally for every sampled frame, so a missing plane means the capture path changed, not that the frame has no articles`);
        process.exit(1);
      }
      fs.writeFileSync(path.join(OUT_DIR, `f${String(f).padStart(4, "0")}.art.bin`),
        decodeMask(captured[f].art, `frame ${f} article mask`));
      const png = captured[f].png;
      const pfx = "data:image/png;base64,";
      if (!png.startsWith(pfx)) {
        console.error("capture-canvas: composite PNG dataURL malformed");
        process.exit(1);
      }
      fs.writeFileSync(path.join(OUT_DIR, `f${String(f).padStart(4, "0")}.png`),
        Buffer.from(png.slice(pfx.length), "base64"));
      masksWritten++;
    }
  };
  const runPlain = async (count) => {
    for (let done = 0; done < count; done += CHUNK) {
      const n = Math.min(CHUNK, count - done);
      const chunk = await page.evaluate(
        (o) => window.__gfxRunChunk(o.n, o.sampled, o.inject),
        { n, sampled, inject: null });
      frames.push(...chunk);
      await drain();
    }
  };

  // Browser-side attribution masks (review-65 r2 M4; trajectory
  // continuity review-70 r3, iter 71): the replay is segmented around
  // the pinned injection frame; that ONE frame runs through a dedicated
  // evaluate (step -> inject -> canonical render -> mask, the
  // __gfxRunChunk shape) which ADDITIONALLY records, without stepping
  // the sim again: one full render and one leave-one-out render per
  // inkNames effect, all on the DETERMINISTIC render-plane mulberry32
  // (gfx-pagelib.js __gfxRenderRng — the browser twin of the C
  // leave-one-out baselines; the gameplay chain is untouched, STREAM
  // MATCH still gates the whole capture). The CANONICAL render itself
  // runs on that same RNG (review-70 r3 fix): a native-RNG canonical
  // render produced post-render queue state (firefoxtail.randomTail,
  // shine star scatter) that the old finally-path re-render could not
  // reproduce — native RNG cannot be rewound — leaving frame 150's saved
  // mask on trajectory A while frames 151+ continued on trajectory B
  // (measured: randomTail 0/4 components equal,
  // .loop/m4-task2r71-probe-old.log).
  // Now: canonical render (live queue) -> mask; snapPost = the
  // post-canonical queue instances BY REFERENCE; det render = a strict
  // REPLAY (pre-render JSON snapshot restored + the render RNG REWOUND
  // to its pre-canonical state), ASSERTED bitwise-equal to the canonical
  // mask (iou.js re-asserts it judge-side); loo renders operate on
  // clones only; finally restores snapPost AND the post-canonical RNG
  // state, and NEVER re-renders. So the continuing simulation sees
  // exactly the state the single canonical render produced: one
  // trajectory, on one unbroken render stream. (Canvas pixel state needs
  // no restore — every render begins with clearScreen.)
  // C28: the rewind replaced a per-render RESEED. The render plane is
  // now ONE chained stream for the whole replay, mirroring the C's
  // gfx_vfx.c chain, so reseeding at frame 150 would fork the browser
  // off the C's stream for the remaining 3450 frames.
  if (!Number.isInteger(INJECT.frame) || INJECT.frame < 1 || INJECT.frame > g.frames) {
    console.error("capture-canvas: inject.frame outside the replay range");
    process.exit(1);
  }
  await runPlain(INJECT.frame - 1);
  const special = await page.evaluate(async (o) => {
    const r = await window.__runFrames(1, {});
    if (r[0].f !== o.inject.frame) {
      throw new Error("gfx-capture: injection-frame misalignment: stepped " + r[0].f +
        " expected " + o.inject.frame);
    }
    window.__gfxInject(o.inject.configs);
    // upstream vfxQueue module (unique-match hard-fail, findModule class)
    const cache = window.__wpCache;
    if (!cache) throw new Error("gfx-capture: __wpCache hook missing");
    const hits = [];
    for (const id of Object.keys(cache)) {
      const m = cache[id];
      if (m && m.exports && Array.isArray(m.exports.vfxQueue) &&
          typeof m.exports.addToVfxQueue === "function" &&
          typeof m.exports.dropFromVfxQueue === "function" &&
          typeof m.exports.resetVfxQueue === "function") hits.push(m.exports);
    }
    if (hits.length !== 1) {
      throw new Error("gfx-capture: vfxQueue module matched " + hits.length + " times (need exactly 1)");
    }
    const qmod = hits[0];
    for (const c of o.inject.configs) {
      if (!qmod.vfxQueue.some((v) => v && v.name === c.name)) {
        throw new Error("gfx-capture: injected config '" + c.name + "' missing from vfxQueue post-inject");
      }
    }
    const snap = JSON.parse(JSON.stringify(qmod.vfxQueue));
    const restore = (skip) => {
      const q = qmod.vfxQueue;
      q.length = 0;
      for (const inst of snap) {
        if (skip !== null && inst.name === skip) continue;
        q.push(JSON.parse(JSON.stringify(inst)));
      }
    };
    // C28: the render plane is one CHAINED mulberry32 for the whole
    // replay now (gfx-pagelib.js __gfxRenderRng), mirroring the C's
    // gfx_vfx.c stream, so the det/loo replays REWIND that chain instead
    // of reseeding it. Reseeding here would fork the browser off the C's
    // stream from frame 150 onward. mulberry32 state is one int32, so the
    // rewind is exact and the contract below is unchanged.
    const rng = window.__gfxRenderRng;
    if (!rng || typeof rng.state !== "function" || typeof rng.setState !== "function") {
      throw new Error("gfx-capture: __gfxRenderRng hook missing (render-plane RNG)");
    }
    const rngPre = rng.state();
    let rngPost = null;
    let det = null;
    const loo = {};
    let snapPost = null;
    try {
      // canonical render + mask — on the deterministic render-plane RNG
      // (review-70 r3): its queue side effects ARE the trajectory that
      // frames 151+ continue from, and det below replays it exactly.
      window.__gfxRender();
      const canonical = window.__gfxCaptureMask();
      rngPost = rng.state(); // the trajectory frames 151+ continue from
      // U3: the injection frame is a SAMPLED frame and owes the article
      // plane like every other one. Taken here, AFTER __gfxCaptureMask,
      // and it consumes no RNG (LASER.draw draws no randomness), so the
      // rngPost snapshot above still describes the trajectory. The det
      // replay below re-clears FG2 through clearScreen anyway.
      canonical.art = window.__gfxArticlePlane();
      window.__gfxCaptured[r[0].f] = canonical;
      // post-canonical-render queue state, by REFERENCE (the live
      // instances this render mutated/spawned — restored in finally;
      // the det/loo renders below touch JSON clones only)
      snapPost = qmod.vfxQueue.slice();
      // det = strict replay of the canonical render: same pre-render
      // snapshot, and the page-local render RNG REWOUND to the same state
      // (rng.setState(rngPre) — a rewind of the one chained stream, NOT a
      // reseed; reseeding would fork the browser off the C's chain, see the
      // C28 note above) -> must reproduce the mask bit-for-bit
      // (free tooth: proves the observation machinery is side-effect-
      // free and every loo baseline shares the canonical trajectory)
      restore(null);
      rng.setState(rngPre);
      // bg:false — U1: only the canonical render of a frame may advance
      // the mirrored background stream (see gfx-pagelib.js __gfxRender).
      window.__gfxRender({ bg: false });
      det = window.__gfxCaptureMask().mask;
      if (det !== canonical.mask) {
        throw new Error("gfx-capture: det replay mask != canonical mask at the injection frame " +
          "(snapshot/restore + RNG rewind failed to reproduce the canonical render — " +
          "trajectory continuity broken)");
      }
      for (const nm of o.inkNames) {
        restore(nm);
        rng.setState(rngPre);
        window.__gfxRender({ bg: false });
        loo[nm] = window.__gfxCaptureMask().mask;
      }
    } finally {
      if (rngPost !== null) rng.setState(rngPost);
      // restore the post-canonical-render trajectory; NEVER re-render
      // (a re-render here was the review-70 r3 discontinuity)
      if (snapPost !== null) {
        const q = qmod.vfxQueue;
        q.length = 0;
        for (const inst of snapPost) q.push(inst);
      }
    }
    return { rec: r[0], det: det, loo: loo };
  }, { inject: INJECT, inkNames: EXPECTED_RENDER.injectPin.inkNames });
  frames.push(special.rec);
  {
    const tag = String(INJECT.frame).padStart(4, "0");
    fs.writeFileSync(path.join(OUT_DIR, `f${tag}.det.mask.bin`),
      decodeMask(special.det, "det mask"));
    for (const nm of EXPECTED_RENDER.injectPin.inkNames) {
      if (typeof special.loo[nm] !== "string") {
        console.error(`capture-canvas: leave-one-out mask for '${nm}' missing`);
        process.exit(1);
      }
      fs.writeFileSync(path.join(OUT_DIR, `f${tag}.loo-${nm}.mask.bin`),
        decodeMask(special.loo[nm], `loo-${nm} mask`));
    }
  }
  await drain();
  await runPlain(g.frames - INJECT.frame);
  const wall = Date.now() - t0;
  const wanted = sampledList.length;
  if (masksWritten !== wanted) {
    console.error(`capture-canvas: wrote ${masksWritten} masks, wanted ${wanted}`);
    process.exit(1);
  }

  // U1 tunnel leg (backgroundType 1). The type is one seeded draw at
  // startGame, i.e. a per-SEED coin flip, not a stage property. MEASURED
  // over the committed goldens: g01/g02/g03/g05/g06/g07/g08/m02 select type 0 while g04 (seed
  // 7344) and m01 (seed 8114) select type 1.
  // This rig replays g01 only, so drawTunnel is unreachable from the
  // match above. It consumes no randomness, so it pairs with the C exactly
  // from pristine module state. Runs LAST: the checksum stream, every
  // sampled mask and the run JSON inputs are already collected, and
  // setBackgroundType/drawBackgroundInit only touch render state.
  {
    const tSampled = {};
    for (const v of TUNNEL_FRAMES) tSampled[v] = true;
    const tOut = await page.evaluate(
      (o) => window.__gfxBgTunnel(o.frames, o.sampled),
      { frames: TUNNEL_FRAMES[TUNNEL_FRAMES.length - 1], sampled: tSampled });
    let n = 0;
    for (const f of Object.keys(tOut)) {
      fs.writeFileSync(path.join(OUT_DIR, `t${String(f).padStart(4, "0")}.bg.bin`),
        decodeMask(tOut[f], `tunnel frame ${f} bg mask`));
      n++;
    }
    if (n !== TUNNEL_FRAMES.length) {
      console.error(`capture-canvas: tunnel leg wrote ${n} masks, wanted ${TUNNEL_FRAMES.length}`);
      process.exit(1);
    }
  }

  // Reuse-hatch INPUT-CLOSURE binding (review-44 fix 3, widened by
  // review-46 fix 2; snapshot-verified per review-48 round 3, iter 49):
  // record, alongside the cached masks, the PRE-CONSUMPTION sha256 of
  // EVERY capture-input-closure member (CLOSURE_SNAP — drivers, page
  // init scripts, expected-render.json as the allowlist/pin source,
  // manifest, trace) + the GFXDATA this capture wrote. Before writing,
  // RE-hash every member and verify it still equals its snapshot: a
  // member changed mid-capture (a normal editor save is enough) means
  // the bytes on disk are no longer the bytes this capture consumed —
  // binding them would let a later reuse pass against masks produced
  // from OTHER bytes. Loud death, NO sidecar. check-render.sh's
  // MLFK_GFX_REUSE_CANVAS path re-derives the closure and refuses
  // (loud) on any member-set or digest drift.
  for (const k of Object.keys(CLOSURE)) {
    const now = sha256Hex(fs.readFileSync(CLOSURE[k]));
    if (now !== CLOSURE_SNAP[k]) {
      console.error("capture-canvas: closure member changed MID-CAPTURE: " + k +
        ` (snapshot ${CLOSURE_SNAP[k]} != current ${now}); ` +
        "the capture consumed bytes that are no longer on disk — sidecar NOT written");
      process.exit(1);
    }
  }
  // The sidecar is published at the very END of this function — after the
  // last page call, the run JSON, and the browser/server shutdown — and
  // only if no page error has occurred by then (review-u1 r7). Publishing
  // it earlier let a LATE page error leave a sidecar behind that a later
  // MLFK_GFX_REUSE_CANVAS run would accept, laundering a failed capture
  // into RENDER OK. Anything that fails before that point leaves NO
  // sidecar, so reuse cannot proceed at all.
  const sidecarPath = path.join(OUT_DIR, "capture.digests.json");
  fs.rmSync(sidecarPath, { force: true });

  const coverage = await page.evaluate(() => window.__coverage());
  fs.writeFileSync(OUT_RUN, JSON.stringify({
    meta: {
      dist: DIST_ROOT, trace: TRACE, frames: g.frames, seed: g.seed,
      p1: g.p1, p2: g.p2, stage: g.stage,
      seedRandom: true, fdlibm: true, cpu: g.cpu,
      difficulty: g.cpu ? g.difficulty : null, wallMs: wall,
      browser: browserId.name, version: browserId.version,
      gfxCapture: {
        sampled: sampledList.slice().sort((a, b) => a - b),
        served: servedDigest(),
      },
    },
    coverage,
    frames,
  }));

  await browser.close();
  srv.close();

  // PUBLICATION, last of all (review-u1 r7). Every page call is done and
  // the browser is closed, so `pageErrored` is now final. Only a clean
  // run gets a sidecar, and it is written after the artifacts it hashes.
  if (pageErrored) {
    console.error("capture-canvas: page error(s) occurred — no sidecar published, " +
      "so this capture cannot be reused");
    process.exit(1);
  }
  const artifacts = {};
  for (const name of fs.readdirSync(OUT_DIR).sort()) {
    if (name === "capture.digests.json") continue;
    artifacts[name] = sha256Hex(fs.readFileSync(path.join(OUT_DIR, name)));
  }
  fs.writeFileSync(sidecarPath, JSON.stringify({
    artifacts: artifacts,
    // The ENGINE that rasterized these artifacts (review-134 r1 M1). Bound
    // here so MLFK_GFX_REUSE_CANVAS and check-render.sh's closure step can
    // both refuse a capture taken on an unmeasured rasterizer — reuse never
    // launches a browser, so without this the identity would be unknowable
    // in exactly the mode that skips the check.
    browser: browserId,
    closure: CLOSURE_SNAP,
    gfxdata: sha256Hex(fs.readFileSync(path.resolve(GFXDATA))),
    vfxdata: sha256Hex(fs.readFileSync(path.resolve(VFXDATA))),
    glyphs: sha256Hex(fs.readFileSync(path.resolve(GLYPHS))),
  }, null, 2) + "\n");

  console.log(`${g.id}: ${g.frames} frames rendered+stepped in ${wall}ms; ` +
    `${masksWritten} sampled masks -> ${OUT_DIR}`);
}

main().catch((e) => {
  // never leave a sidecar from a failed run (review-u1 r7)
  try { fs.rmSync(path.join(OUT_DIR, "capture.digests.json"), { force: true }); } catch (_) {}
  console.error(e);
  process.exit(1);
});
