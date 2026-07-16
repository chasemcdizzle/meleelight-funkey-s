#!/usr/bin/env node
// dump-sim-data.js — M2 task 17: one-shot static-data dump for the
// integrated C sim. Boots the built upstream clone through the EXACT
// run-capture.js page recipe (same served-bytes hooks, same init-script
// pipeline, capturelib.js for ctx.canon/findModule) and dumps the FIVE
// data planes the C sim loads as tables — no golden, no trace, no frames:
// every plane is page-boot static (the capture specs' finalCheck drift
// guards prove the flags/mvData planes never mutate over a match, and
// the per-frame planes are the replay clusters' business, not this
// script's).
//
// Usage: node dump-sim-data.js [--dist <clone-root>] [--out <file>]
//
// Sections (canon v1 strings, byte-comparable against the capture specs'
// frame-0 records):
//   asFlags    — spec-physics.js's asFlags dump, verbatim (the
//                actionStates data plane physics branches on, 5 chars)
//   hdFlags    — spec-hitdet.js's hdFlags dump, verbatim
//   mvChars    — {0..4: cd} where cd is the per-char mvData `cd` object
//                the moves specs build (actionSounds/name/posOffset*/
//                setPositionsCaptureDamage/setVelocities/shared);
//                sharedOrigin measured by FUNCTION IDENTITY against the
//                shared moves-index module (puff's FURAFURA/JUMPAERIALB/
//                JUMPAERIALF overrides come out false on table 1
//                automatically)
//   mvCharData — {0..4: {state: {key: array}}}: every own enumerable
//                ARRAY-valued prop of every NON-shared actionStates entry
//                (the generalized per-char "data plane" — the exact rule
//                all five spec-moves-*.js dumps use; the only known
//                non-array datum, falcon SIDESPECIALGROUND.canEdgeCancel,
//                is a runtime-written scalar deliberately OUTSIDE the
//                dumps — C module state, spec-moves-falcon.js note)
//   palettes0  — the 4-element [palettes[pPal[k]][0]] list (mvData's
//                palettes0)
//
// Artifact (default port/sim/calib/build/simdata.txt): line 1 "SIMDATA1",
// then 5 "<section>\t<canon>" lines. DETERMINISTIC — no timestamps, no
// paths (the check script runs it twice and cmps byte-identical).
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

const DIST_ROOT = path.resolve(arg("dist",
  process.env.MELEELIGHT_CLONE ||
  path.join(process.env.HOME, ".cache", "meleelight-funkey-s", "upstream")));
const OUT = path.resolve(arg("out",
  path.join(__dirname, "build", "simdata.txt")));
// Fixed seed: the data planes are static, but the boot pipeline is kept
// byte-identical to a real capture run (seeded RNG, virtual clock, fdlibm).
const SEED = 1337;
const SECTIONS = ["asFlags", "hdFlags", "mvChars", "mvCharData", "palettes0"];

const MIME = {
  ".html": "text/html", ".js": "text/javascript", ".css": "text/css",
  ".json": "application/json", ".png": "image/png", ".jpg": "image/jpeg",
  ".svg": "image/svg+xml", ".wav": "audio/wav", ".mp3": "audio/mpeg",
};

// Served-bytes hooks, verbatim from run-capture.js: (1) the webpack module
// cache exposed as window.__wpCache (capturelib requires it); (2) the
// fountain platformStates getter (unused here, kept so the served main.js
// bytes are IDENTICAL to every capture run's).
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
          res.end("simdata: webpack bootstrap line not found exactly once in main.js");
          return;
        }
        let hooked = src.slice(0, i) + BOOT_HOOK + src.slice(i + BOOT_LINE.length);
        const j = hooked.indexOf(PS_LINE);
        if (j === -1 || hooked.indexOf(PS_LINE, j + 1) !== -1) {
          res.writeHead(500);
          res.end("simdata: fountain platformStates declaration not found exactly once in main.js");
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

// The simdata spec, injected AFTER capturelib.js so window.__capSpecs
// exists. Wraps NOTHING (expectWrapped 0); install() builds the five
// canon strings onto this._out and hard-throws on any domain violation.
const SPEC_SRC = `(() => {
  // spec-physics.js FLAG_KEYS, verbatim:
  const AS_FLAG_KEYS = ["airborneState", "canEdgeCancel", "canGrabLedge",
                        "canPassThrough", "dead", "disableTeeter", "headBonk",
                        "ignoreCollision", "inGrab", "landType", "missfoot",
                        "name", "specialWallCollide", "wallJumpAble"];
  // spec-hitdet.js FLAG_KEYS, verbatim:
  const HD_FLAG_KEYS = ["canBeGrabbed", "crouch", "downed", "name",
                        "specialClank", "specialOnHit", "vCancel"];
  // the moves specs' mvData key lists, verbatim:
  const SND_KEYS = ["CLIFFCATCH", "DEAD", "ESCAPEAIR", "ESCAPEB", "ESCAPEF",
                    "ESCAPEN", "FURAFURA", "GUARDOFF", "GUARDON", "JUMP",
                    "JUMPAERIAL", "OTTOTTOWAIT", "TECH"];
  const SETVEL_KEYS = ["DOWNSTANDB", "DOWNSTANDF", "ESCAPEB", "ESCAPEF",
                       "TECHB", "TECHF"];

  window.__capSpecs.simdata = {
    expectWrapped: 0,

    install(ctx) {
      const cache = ctx.cache;
      const moduleIds = {};
      const find = (pred, what) => {
        const m = ctx.findModule(cache, pred, what);
        moduleIds[what] = m.id;
        return m.exports;
      };

      const AS = find((ex) =>
        typeof ex.turboGroundedInterrupt === "function" &&
        typeof ex.checkForIASA === "function" &&
        typeof ex.setupActionStates === "function" &&
        Array.isArray(ex.actionStates), "actionStateShortcuts");
      const M = find((ex) =>
        Array.isArray(ex.player) && Array.isArray(ex.playerType) &&
        Array.isArray(ex.characterSelections) &&
        typeof ex.gameTick === "function", "main");
      const CHARS = find((ex) =>
        Array.isArray(ex.intangibility) && Array.isArray(ex.actionSounds) &&
        ex.actionSounds.length >= 5, "characters");
      const SHARED = find((ex) =>
        ex.default && typeof ex.default === "object" &&
        ex.default.WAIT && typeof ex.default.WAIT === "object" &&
        typeof ex.default.WAIT.init === "function" &&
        ex.default.THROWNFALCONDIVE && !ex.default.ATTACKAIRF,
        "sharedMovesIndex").default;

      // --- domain assertions (hard-throw) --------------------------------
      if (AS.actionStates.length !== 5) {
        throw new Error("simdata: expected 5 action-state tables, got " +
                        AS.actionStates.length);
      }
      for (let c = 0; c < 5; c++) {
        const tbl = AS.actionStates[c];
        if (!tbl || typeof tbl !== "object" || Object.keys(tbl).length === 0) {
          throw new Error("simdata: action-state table " + c +
                          " missing or empty");
        }
      }
      const sharedKeys = Object.keys(SHARED);
      if (sharedKeys.length !== 79) {
        throw new Error("simdata: expected 79 shared index keys, got " +
                        sharedKeys.length);
      }

      // --- name/sharedOrigin maps (fn identity vs the SHARED module) -----
      const sharedOrigin = [{}, {}, {}, {}, {}];
      const nameMap = [{}, {}, {}, {}, {}];
      for (let c = 0; c < 5; c++) {
        const tbl = AS.actionStates[c];
        for (const st of Object.keys(tbl)) {
          const entry = tbl[st];
          if (!entry || typeof entry !== "object" ||
              typeof entry.name !== "string") {
            throw new Error("simdata: table " + c + " state " + st +
                            " has no string .name");
          }
          nameMap[c][st] = entry.name;
          sharedOrigin[c][st] = SHARED[st] !== undefined &&
                                typeof SHARED[st].init === "function" &&
                                entry.init === SHARED[st].init;
        }
        for (const st of sharedKeys) {
          if (!(st in sharedOrigin[c])) {
            throw new Error("simdata: table " + c +
                            " missing shared state " + st);
          }
        }
      }

      // --- asFlags / hdFlags (the spec-physics/spec-hitdet dumps) --------
      const flagsCanon = (FLAG_KEYS) => {
        const dump = {};
        for (let c = 0; c < AS.actionStates.length; c++) {
          const tbl = AS.actionStates[c];
          if (!tbl) continue;
          const chr = {};
          for (const st of Object.keys(tbl)) {
            const e = tbl[st];
            const f = {};
            for (const k of FLAG_KEYS) f[k] = e[k];
            chr[st] = f;
          }
          dump[c] = chr;
        }
        return ctx.canon(dump);
      };

      // --- mvChars (the moves specs' per-char cd object) ------------------
      const chars = {};
      for (let c = 0; c < 5; c++) {
        const tbl = AS.actionStates[c];
        const cd = {
          actionSounds: {},
          name: nameMap[c],
          posOffsetCliffCatch: tbl.CLIFFCATCH.posOffset,
          posOffsetCliffWait: tbl.CLIFFWAIT.posOffset,
          setPositionsCaptureDamage: tbl.CAPTUREDAMAGE.setPositions,
          setVelocities: {},
          shared: sharedOrigin[c],
        };
        for (const k of SETVEL_KEYS) cd.setVelocities[k] = tbl[k].setVelocities;
        for (const k of SND_KEYS) {
          if (CHARS.actionSounds[c][k] !== undefined) {
            cd.actionSounds[k] = CHARS.actionSounds[c][k];
          }
        }
        chars[c] = cd;
      }

      // --- mvCharData (the generalized per-char data plane) ----------------
      // every own enumerable ARRAY-valued prop of every NON-shared entry
      // (the exact rule every spec-moves-*.js dump uses; a state with no
      // array props is omitted, matching the specs' \`if (any)\`).
      const charData = {};
      for (let c = 0; c < 5; c++) {
        const tbl = AS.actionStates[c];
        const cdata = {};
        for (const st of Object.keys(tbl)) {
          if (sharedOrigin[c][st]) continue;
          const entry = tbl[st];
          const d = {};
          let any = false;
          for (const k of Object.keys(entry)) {
            if (Array.isArray(entry[k])) { d[k] = entry[k]; any = true; }
          }
          if (any) cdata[st] = d;
        }
        charData[c] = cdata;
      }

      // --- palettes0 (mvData's 4-element list) -----------------------------
      const palettes0 = [];
      for (let k = 0; k < 4; k++) {
        palettes0.push(M.palettes[M.pPal[k]][0]);
      }

      this._out = {
        asFlags: flagsCanon(AS_FLAG_KEYS),
        hdFlags: flagsCanon(HD_FLAG_KEYS),
        mvChars: ctx.canon(chars),
        mvCharData: ctx.canon(charData),
        palettes0: ctx.canon(palettes0),
      };
      return { moduleIds: moduleIds };
    },
  };
})();`;

async function main() {
  if (!fs.existsSync(path.join(DIST_ROOT, "dist", "meleelight.html"))) {
    console.error("--dist must point at a built meleelight clone (run oracle/build-upstream.sh)");
    process.exit(1);
  }
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

  // Identical init pipeline to run-capture.js (fixed seed — no golden).
  await page.addInitScript({
    content: "window.__harnessConfig = " + JSON.stringify({
      seedRandom: true, seed: SEED, virtualClock: true, fdlibm: true,
      captureMath: false }) + ";",
  });
  await page.addInitScript({ path: path.join(REPO, "port", "fdlibm", "fdlibm.js") });
  await page.addInitScript({ path: path.join(HARNESS, "init.js") });
  await page.addInitScript({ path: path.join(HARNESS, "pagelib.js") });
  await page.addInitScript({ path: path.join(__dirname, "capturelib.js") });
  await page.addInitScript({ content: SPEC_SRC });

  await page.goto(`http://localhost:${port}/dist/meleelight.html`);
  await page.waitForFunction(
    "window.__harness && window.__nextInputBuffers", null, { timeout: 120000 });

  const install = await page.evaluate((s) => window.__capInstallSpec(s), "simdata");
  if (install.wrapped !== 0) {
    throw new Error(`simdata: expected 0 wrapped exports, got ${install.wrapped}`);
  }
  const out = await page.evaluate(() => window.__capSpecs.simdata._out);
  for (const s of SECTIONS) {
    if (typeof out[s] !== "string" || out[s].length === 0) {
      throw new Error(`simdata: section ${s} missing/empty in page output`);
    }
  }

  fs.mkdirSync(path.dirname(OUT), { recursive: true });
  fs.writeFileSync(OUT,
    "SIMDATA1\n" + SECTIONS.map((s) => s + "\t" + out[s] + "\n").join(""));

  console.log("simdata: " +
    SECTIONS.map((s) => `${s}=${Buffer.byteLength(out[s])}B`).join(" ") +
    ` -> ${path.relative(process.cwd(), OUT)}`);

  await browser.close();
  srv.close();
}

main().catch((e) => { console.error(e); process.exit(1); });
