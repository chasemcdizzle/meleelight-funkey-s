#!/usr/bin/env node
// port/gfx/glyph-jitter-probe.js — glyph-rasterization jitter probe
// (M4 task 2, iter 72; PROCESS §8 standing instrument).
//
// Characterizes the cross-session distribution of the browser's
// VFXGLYPHS1 dump: N FULLY FRESH browser sessions (each its own
// chromium.launch -> served pinned dist -> the capture-canvas.js
// init-script stack verbatim -> __gfxDumpGlyphs() -> teardown), one
// dump file per session. Pairwise measurement is glyph-compare.js
// --measure (the same parser/comparator check-render.sh judges with —
// never a second implementation).
//
// Probe simplification (documented, pre-registered iter 72): no
// setupMatch / no trace feed — glyph rasterization consumes only
// (browser build, font, canvas ops); the dump path is otherwise the
// capture-canvas.js path byte-for-byte (same served bytes incl. the
// main.js __wpCache hook, same init scripts, same page URL).
//
// Usage: node glyph-jitter-probe.js --sessions 5 --out-dir build/glyphchar
"use strict";

const crypto = require("crypto");
const fs = require("fs");
const http = require("http");
const path = require("path");

const REPO = path.resolve(__dirname, "..", "..");
const HARNESS = path.join(REPO, "oracle", "harness");
const { chromium } = require(path.join(HARNESS, "node_modules", "playwright"));

function arg(name, dflt) {
  const i = process.argv.indexOf("--" + name);
  if (i === -1) return dflt;
  const v = process.argv[i + 1];
  return v === undefined || v.startsWith("--") ? true : v;
}

const SESSIONS = parseInt(String(arg("sessions", "5")), 10);
const OUT_DIR = String(arg("out-dir", ""));
if (!Number.isInteger(SESSIONS) || SESSIONS < 1 || SESSIONS > 16 ||
    !OUT_DIR || OUT_DIR === "true") {
  console.error("glyph-jitter-probe: --sessions 1..16 and --out-dir required");
  process.exit(1);
}
const DIST_ROOT = path.resolve(String(arg("dist",
  process.env.MELEELIGHT_CLONE ||
  path.join(process.env.HOME, ".cache", "meleelight-funkey-s", "upstream"))));

// Same closure map the capture uses (g01's trace parameterizes it; the
// probe never reads the trace — it only resolves the init-script paths
// through the ONE enumeration, capture-closure.js).
const manifest = JSON.parse(fs.readFileSync(
  path.join(REPO, "oracle", "goldens", "manifest.json"), "utf8"));
const g = manifest.goldens.find((x) => x.id === "g01");
if (!g) { console.error("glyph-jitter-probe: g01 missing from manifest"); process.exit(1); }
const { closureFiles } = require(path.join(__dirname, "capture-closure.js"));
const CLOSURE = closureFiles(g.trace);

const MIME = {
  ".html": "text/html", ".js": "text/javascript", ".css": "text/css",
  ".json": "application/json", ".png": "image/png", ".jpg": "image/jpeg",
  ".svg": "image/svg+xml", ".wav": "audio/wav", ".mp3": "audio/mpeg",
};
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
          res.end("probe: webpack bootstrap line not found exactly once in main.js");
          return;
        }
        res.writeHead(200, { "Content-Type": "text/javascript" });
        res.end(Buffer.from(
          src.slice(0, i) + BOOT_HOOK + src.slice(i + BOOT_LINE.length), "utf8"));
        return;
      }
      res.writeHead(200, { "Content-Type": MIME[path.extname(fp)] || "application/octet-stream" });
      res.end(fs.readFileSync(fp));
    });
    srv.listen(0, "127.0.0.1", () => resolve(srv));
  });
}

async function oneSession(port, outFile) {
  let browser;
  try {
    browser = await chromium.launch({ channel: "chrome", headless: true });
  } catch (e) {
    browser = await chromium.launch({ headless: true });
  }
  try {
    const context = await browser.newContext();
    await context.route(/\/(sfx|music)\//, (r) => r.abort());
    const page = await context.newPage();
    page.on("pageerror", (e) => {
      console.error("[pageerror]", e.message);
      process.exit(1);
    });
    await page.addInitScript({
      content: "window.__harnessConfig = " + JSON.stringify({
        seedRandom: true, seed: g.seed, virtualClock: true, fdlibm: true,
        captureMath: false }) + ";",
    });
    await page.addInitScript({ path: CLOSURE["port/fdlibm/fdlibm.js"] });
    await page.addInitScript({ path: CLOSURE["oracle/harness/init.js"] });
    await page.addInitScript({ path: CLOSURE["oracle/harness/pagelib.js"] });
    await page.addInitScript({ path: CLOSURE["port/gfx/gfx-pagelib.js"] });
    await page.goto(`http://localhost:${port}/dist/meleelight.html`);
    await page.waitForFunction(
      "window.__harness && window.__nextInputBuffers", null, { timeout: 120000 });
    const glyphs = await page.evaluate(() => window.__gfxDumpGlyphs());
    fs.writeFileSync(outFile, glyphs);
    const sha = crypto.createHash("sha256").update(glyphs).digest("hex");
    const browserVer = browser.browserType().name() + " " + browser.version();
    return { sha, browserVer };
  } finally {
    await browser.close();
  }
}

async function main() {
  if (!fs.existsSync(path.join(DIST_ROOT, "dist", "meleelight.html"))) {
    console.error("--dist must point at a built meleelight clone (run oracle/build-upstream.sh)");
    process.exit(1);
  }
  fs.mkdirSync(OUT_DIR, { recursive: true });
  const srv = await serve(DIST_ROOT);
  const port = srv.address().port;
  for (let n = 1; n <= SESSIONS; n++) {
    const out = path.join(OUT_DIR, `s${n}.txt`);
    fs.rmSync(out, { force: true });
    const r = await oneSession(port, out);
    if (!fs.existsSync(out) || fs.statSync(out).size === 0) {
      console.error(`glyph-jitter-probe: session ${n} produced no dump`);
      process.exit(1);
    }
    console.log(`session ${n}: ${out} sha256=${r.sha} (${r.browserVer})`);
  }
  srv.close();
}

main().catch((e) => { console.error(e); process.exit(1); });
