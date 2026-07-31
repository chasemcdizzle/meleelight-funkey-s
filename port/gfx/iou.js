#!/usr/bin/env node
// port/gfx/iou.js — silhouette IoU judge (M3 task 3).
//
// Compares, per sampled frame, the browser reference ink mask
// (capture-canvas.js: 1200x750 bytes, 1 = fg1|fg2 alpha>0) against the C
// renderer's ink plane (gfx_replay fNNNN.pgm, 240x240) under the frozen
// methodology (iter-44 pre-registration):
//   - browser mask -> 240x150 by 5x5 box: cell inked iff ANY of its 25
//     source pixels is inked;
//   - C mask -> the 240x150 letterbox band rows [45,195); ANY C ink
//     outside the band is a hard failure (the retarget must not leak);
//   - IoU = |A&B| / |A|B| per frame, compared against the frozen
//     threshold in expected-render.json (never loosened).
//
// Per-effect injection assertions (review-65 M2, iter 67; browser
// attribution added review-65 r2 M4, iter 70): at the pinned injection
// frame, every inkNames effect must contribute ink on BOTH sides —
// (i) browser mask ink > 0 in the effect's derived region, (i') browser
// LEAVE-ONE-OUT differential ink > 0 (f<tag>.det.mask.bin vs
// f<tag>.loo-<name>.mask.bin, both rendered by the capture under a
// deterministic page-local render RNG — the browser twin of (iii); the
// canonical injection-frame render shares that RNG and det is asserted
// byte-identical to it, review-70 r3 trajectory continuity),
// (ii) C with-inject ink > 0 in the region, (iii) C LEAVE-ONE-OUT
// DIFFERENTIAL ink > 0 in the region: the full render diffed against a
// baseline whose INJECT1 dropped exactly that effect. (iii) is the
// sharp per-effect tripwire — full and leave-out-X are identical
// renders except for X itself, so a stubbed X draw arm makes the diff
// empty by construction and cannot hide inside the aggregate IoU (which
// stays the frame-level judge). Attribution argument: the injected
// effects draw in queue spawn order and only firefoxtail/shine consume
// the render-local RNG, so removing X can only ripple into LATER
// RNG-consuming effects' samples — measured to land inside the mover's
// own region (shine's region is disjoint from firefoxtail's), never in
// X's. (A shared no-inject baseline was the round-1 design; the T-M2
// tooth REFUTED it — overlapping regions let another effect's ink flip
// pixels inside a stubbed effect's box. See .loop/m4-task2r67-teeth.log.)
// A region-soundness guard additionally requires EVERY full-vs-noinject
// differential pixel in the whole frame to fall inside the union of
// derived regions (an effect drawing outside its derived bounds, or the
// gated no-draw pair drawing anything, dies loudly). Region derivation
// is documented at injectRegions() below.
//
// Usage: node iou.js --canvas DIR --render DIR --render-noinject DIR
//        --render-loo PREFIX (PREFIX-<name>/ per inkNames effect)
//        --vfxdata vfxdata-frozen.txt --stages stages.json --stage N
//        --expected expected-render.json [--report out.json]
// Prints one line per frame + INJ lines + a MIN line; exit 0 iff every
// frame passes and every injection assertion holds.
"use strict";

const fs = require("fs");
const path = require("path");

function arg(name, dflt) {
  const i = process.argv.indexOf("--" + name);
  if (i === -1) return dflt;
  const v = process.argv[i + 1];
  return v === undefined || v.startsWith("--") ? true : v;
}

const CANVAS = arg("canvas", "");
const RENDER = arg("render", "");
const RENDER_NOINJ = arg("render-noinject", "");
const RENDER_LOO = arg("render-loo", "");
const VFXDATA = arg("vfxdata", "");
const STAGES = arg("stages", "");
const STAGE_IDX = arg("stage", "");
const EXPECTED = arg("expected", "");
const REPORT = arg("report", "");
const BG_TUNNEL = arg("bg-tunnel", "");     // C tunnel-leg pgm dir (U1)
const BG_GRAD_C = arg("bg-grad-c", "");     // C BGGRAD1 dump (U1)
if (!CANVAS || !RENDER || !EXPECTED || CANVAS === true || RENDER === true || EXPECTED === true ||
    !RENDER_NOINJ || RENDER_NOINJ === true || !RENDER_LOO || RENDER_LOO === true ||
    !VFXDATA || VFXDATA === true ||
    !BG_TUNNEL || BG_TUNNEL === true || !BG_GRAD_C || BG_GRAD_C === true ||
    !STAGES || STAGES === true || STAGE_IDX === "" || STAGE_IDX === true ||
    !/^[0-9]+$/.test(String(STAGE_IDX))) {
  console.error("iou: --canvas, --render, --render-noinject, --render-loo, --vfxdata, --bg-tunnel, --bg-grad-c, --stages, --stage, --expected are required");
  process.exit(1);
}

const exp = JSON.parse(fs.readFileSync(EXPECTED, "utf8"));
// Corpus pin (review-44 fix 1; EXTENDED 16 -> 24 by M4 task 2's reviewed
// corpus change — vfx/overlay-bearing frames + the injection frame): the
// frozen 24-frame corpus, exactly — count pinned both here and in
// expected-render.json (sampledFrameCount), every frame a unique positive
// integer. Changing the corpus is a reviewed repo change to BOTH files,
// never a runtime degradation.
if (!Array.isArray(exp.sampledFrames) || !Number.isInteger(exp.sampledFrameCount) ||
    exp.sampledFrameCount !== 24 || exp.sampledFrames.length !== exp.sampledFrameCount) {
  console.error("iou: corpus pin violated — expected-render.json must pin exactly 24 sampled frames (sampledFrames + sampledFrameCount)");
  process.exit(1);
}
{
  const seen = new Set();
  for (const v of exp.sampledFrames) {
    if (!Number.isInteger(v) || v < 1 || seen.has(v)) {
      console.error(`iou: corpus pin violated — bad or duplicate sampled frame ${v}`);
      process.exit(1);
    }
    seen.add(v);
  }
}
// EXACT pin, not a shape check (C28 re-freeze, iter 134). The old shape
// check accepted any number in (0,1], so the fg bound could be widened
// silently — which is exactly the failure mode the re-freeze exists to
// close. 0.92 is measured-then-frozen: min 0.9208 over 6 fresh captures
// with ZERO spread, and 0.0003 max movement on the 19 near-bound frames
// under a controlled same-version rasterizer-backend change (see
// expected-render.json measuredAtFreeze). Twin-pin class: check-render.sh
// and capture-canvas.js carry their own copies.
if (exp.iouThreshold !== 0.92) {
  console.error("iou: iouThreshold pin violated — got " + String(exp.iouThreshold) +
    ", pinned 0.92 (measured-then-frozen floor; never loosened)");
  process.exit(1);
}

// U1 background pins, JUDGE SIDE (twin-pin class, third independent
// copy alongside check-render.sh and capture-canvas.js). The bg
// thresholds are measured-then-frozen and are floors, never dials: both
// sides run the same mirrored stream from the same start state, so a
// real translation bug shows up as a large drop, not a nudge.
// EXACT pins, not shape checks (review-u1 r1 M3): a shape check would
// accept a tunnel corpus shrunk to [1] or thresholds loosened to
// 0.01/0.01/8 and still print RENDER OK. These are the reviewed frozen
// values, hard-coded here and independently in check-render.sh and
// capture-canvas.js; changing any of them means changing all the copies,
// which is a reviewed repo change, never a runtime degradation.
if (!Array.isArray(exp.bgTunnelFrames) ||
    exp.bgTunnelFrames.join(",") !== "1,2,3,41,81,121,161,200,201,202,240") {
  console.error("iou: bgTunnelFrames pin violated — [" +
    String(exp.bgTunnelFrames) + "] != the hard-pinned reviewed 11-frame list");
  process.exit(1);
}
if (exp.bgIouThreshold !== 0.99 || exp.bgStarIouThreshold !== 0.78 ||
    exp.bgTunnelIouThreshold !== 0.81 || exp.bgGradMaxChannelDelta !== 1) {
  console.error("iou: background threshold pin violated — got " +
    [exp.bgIouThreshold, exp.bgStarIouThreshold, exp.bgTunnelIouThreshold,
     exp.bgGradMaxChannelDelta].join("/") +
    ", pinned 0.99/0.78/0.81/1 (measured-then-frozen floors; never loosened)");
  process.exit(1);
}
if (!Number.isInteger(exp.bgStarFrameCount) || exp.bgStarFrameCount !== 6) {
  console.error("iou: bgStarFrameCount pin violated — want exactly 6 star-bearing sampled frames");
  process.exit(1);
}
// U3 article-plane pins, same twin-pin discipline (check-render.sh and
// capture-canvas.js carry their own copies). Exact values, not shape
// checks: a shape check would accept a 0.01 floor or artFrameCount 0 and
// still print RENDER OK.
// PER-FRAME FLOORS (r6 M1). A single global floor can only be calibrated on
// one frame, so calibrated on the MIN frame it leaves progressively more
// slack on every higher-scoring frame (measured at the old global 0.645:
// f0184 still passed with THREE cells missing). Each article frame now
// carries its own floor, each admitting exactly ONE device cell of movement
// and rejecting TWO. Exact map pin — keys, order and values.
{
  const WANT = { f0184: 0.6707, f1234: 0.6708, f1237: 0.6455, f1238: 0.6455 };
  const got = exp.artFrameFloors;
  if (!got || typeof got !== "object" || Array.isArray(got) ||
      Object.keys(got).join(",") !== Object.keys(WANT).join(",") ||
      Object.keys(WANT).some((k) => got[k] !== WANT[k])) {
    console.error("iou: artFrameFloors pin violated — got " + JSON.stringify(got) +
      ", pinned " + JSON.stringify(WANT) +
      " (measured-then-frozen per-frame floors; never loosened)");
    process.exit(1);
  }
  // the floors must cover exactly the pinned article frames
  const fromFrames = exp.artFrames.map((f) => "f" + String(f).padStart(4, "0")).join(",");
  if (Object.keys(got).join(",") !== fromFrames) {
    console.error("iou: artFrameFloors keys [" + Object.keys(got).join(",") +
      "] != the pinned artFrames [" + fromFrames + "]");
    process.exit(1);
  }
}
if (!Number.isInteger(exp.artFrameCount) || exp.artFrameCount !== 4) {
  console.error("iou: artFrameCount pin violated — want exactly 4 article-bearing sampled frames");
  process.exit(1);
}
// IDENTITIES, not just the count (review-134 r3 M2): a coverage set that
// drifts four-for-four — different frames carrying articles, or a labelling
// slip — would satisfy a bare count. The ordered list is the pin; the count
// is kept as its redundant twin so a half-edit dies.
if (!Array.isArray(exp.artFrames) || exp.artFrames.join(",") !== "184,1234,1237,1238") {
  console.error("iou: artFrames pin violated — [" + String(exp.artFrames) +
    "] != the measured article-bearing set 184,1234,1237,1238");
  process.exit(1);
}
if (exp.artFrames.length !== exp.artFrameCount) {
  console.error("iou: artFrames/artFrameCount disagree");
  process.exit(1);
}
for (const v of exp.artFrames) {
  if (!exp.sampledFrames.includes(v)) {
    console.error("iou: artFrames pin violated — frame " + v + " is not a sampled frame");
    process.exit(1);
  }
}

// Injection-set pin, JUDGE SIDE (review-65 M1, iter 67): iou.js is the
// third independent consumer (check-render.sh + capture-canvas.js carry
// their own copies). The runtime inject table must equal the frozen
// reviewed injectPin; the injection frame must be a sampled frame (the
// per-effect assertions below judge exactly that frame's masks).
const PIN = exp.injectPin;
const INJ = exp.inject;
{
  if (!PIN || !Array.isArray(PIN.names) || !Array.isArray(PIN.inkNames) ||
      !Number.isInteger(PIN.frame) || !Number.isInteger(PIN.count)) {
    console.error("iou: injectPin missing/malformed in expected-render.json");
    process.exit(1);
  }
  if (!INJ || !Number.isInteger(INJ.frame) || !Array.isArray(INJ.configs)) {
    console.error("iou: inject table missing/malformed in expected-render.json");
    process.exit(1);
  }
  const names = INJ.configs.map((c) => c && c.name);
  if (names.some((n) => typeof n !== "string" || n.length === 0) ||
      new Set(names).size !== names.length) {
    console.error("iou: inject pin violated (missing/duplicate config name)");
    process.exit(1);
  }
  if (PIN.count !== PIN.names.length || names.length !== PIN.count ||
      names.join(" ") !== PIN.names.join(" ")) {
    console.error("iou: inject pin violated — runtime configs [" + names.join(", ") +
      "] != pinned reviewed set [" + PIN.names.join(", ") + "]");
    process.exit(1);
  }
  if (INJ.frame !== PIN.frame || !exp.sampledFrames.includes(INJ.frame)) {
    console.error("iou: inject pin violated (inject.frame must equal the pinned frame and be a sampled frame)");
    process.exit(1);
  }
  if (PIN.inkNames.length === 0 || new Set(PIN.inkNames).size !== PIN.inkNames.length ||
      !PIN.inkNames.every((n) => PIN.names.includes(n))) {
    console.error("iou: inject pin violated (inkNames must be a nonempty unique subset of names)");
    process.exit(1);
  }
  // Exact ordered inkNames pin (review-65 r2 M5): the 5 live-drawing
  // effects, HARD-CODED here and in check-render.sh + capture-canvas.js
  // (the sampledFrameCount twin-pin class) — a name silently dropped
  // from expected-render.json's inkNames would otherwise pass every
  // subset check while its leave-one-out and regional assertions
  // vanish. Changing the set is a reviewed change to all four files.
  if (PIN.inkNames.length !== 5 ||
      PIN.inkNames.join(" ") !== "firefoxcharge firefoxtail shine dashDust groundBounce") {
    console.error("iou: inject pin violated — inkNames [" + PIN.inkNames.join(", ") +
      "] != the hard-pinned reviewed 5-name set");
    process.exit(1);
  }
  if (PIN.names.some((n) => !/^[A-Za-z][A-Za-z0-9]*$/.test(n))) {
    console.error("iou: inject pin violated (names must match the identifier grammar — they double as artifact path tokens)");
    process.exit(1);
  }
}

const W = 240, BAND_Y0 = 45, BAND_H = 150;
const SRC_W = 1200, SRC_H = 750;

// RETARGET PIN — MADE OPERATIVE (review-134 r5 Low). expected-render.json
// carried a top-level `"retarget": { "k": 0.2, "dy": 45 }` that LOOKED like
// the frozen methodology but had NO runtime reader: this judge and the
// renderer each hard-coded /5 and +45 independently, so the file could be
// edited to claim a different methodology and the check would still print
// RENDER OK — decision-looking configuration that decides nothing is exactly
// the class this lane exists to remove. It is now BOUND to the constants
// actually used: k is the downscale factor (1/k == the 5x5 box below) and dy
// is the letterbox band origin. Editing either side alone is a loud death.
if (!exp.retarget || typeof exp.retarget !== "object" ||
    exp.retarget.k !== 0.2 || exp.retarget.dy !== 45) {
  console.error("iou: retarget pin violated — expected-render.json must pin " +
    "retarget k=0.2 dy=45, got " + JSON.stringify(exp.retarget));
  process.exit(1);
}
{
  // 1/k is the integer downscale factor; derive it once so the float
  // reciprocal is never compared directly.
  const F = Math.round(1 / exp.retarget.k);
  if (Math.abs(1 / exp.retarget.k - F) > 1e-12 || F !== 5 ||
      SRC_W !== W * F || SRC_H !== BAND_H * F || exp.retarget.dy !== BAND_Y0) {
    console.error("iou: retarget pin does not match the geometry this judge uses — " +
      `pinned k=${exp.retarget.k} (1/k=${F}) dy=${exp.retarget.dy}; judge uses ` +
      `${SRC_W}x${SRC_H} -> ${W}x${BAND_H} (factor ${SRC_W / W}) and band origin ${BAND_Y0}`);
    process.exit(1);
  }
}

// ---------------------------------------------------------------------------
// Injection-region derivation (review-65 M2, iter 67).
//
// Center: the injected config's pos through the EXECUTED stage transform
// (scale/offset read from the pipeline stages.json artifact — bit-pattern
// doubles, never hand-retyped engine values): canvas x = pos.x*S + OX,
// canvas y = -pos.y*S + OY (the ubiquitous newPos map, render.js).
// Extents: the frozen VFXDATA1 template bounds (nested-array walk over
// the COMMITTED vfxdata-frozen.txt; bezier control points bound their
// curves — convex hull) times the upstream dVfx CODE-LITERAL draw scales
// (cited per arm below; these are code constants like the stage rail
// literals, not data-plane values). Margin: +-3 canvas px, +-1 device px
// on top, clipped to the mask/letterbox band. The region-soundness guard
// (every differential pixel inside the union of regions) mechanically
// validates these bounds every run.
//   - dashDust: dVfx/general.js via dashDust.js — path frame idx 0 at
//     the injection draw (timer 1), scale 0.2*(S/4.5); x symmetric
//     (face flip), y true template range.
//   - groundBounce: general.js path frame 0, scale 0.2*(S/4.5), rotated
//     pi/2 - facing -> rotation-safe circle of maxAbs(template).
//   - firefoxcharge: fireFox.js charge — draws path[facing] and
//     path[(facing+4)%10] (config facing 3 -> frames 3 and 7), scales
//     0.35*(S/4.5) x / 0.5*(S/4.5) y about the mapped center.
//   - firefoxtail: fireFox.js tail — two arcs about (pos.x, pos.y+4)
//     mapped; centers jittered within +-2S, radius 4S -> radius 6S.
//   - shine: shine.js t=1 — hexagon circumradius up to 6S + star spawns
//     (stars.js) radial <= 5.5S with drift ~0.94 and star extent 1.1S
//     -> radius 7S covers both.
// firefoxlaunch/shineloop are PLAYER-STATE-GATED (draw nothing on a
// plain frame): no region; the soundness guard proves they contributed
// zero differential ink.
// ---------------------------------------------------------------------------

// Full-corpus VFXDATA1 validation (review-65 r2 M7; PROCESS section-3
// grammar rule). Producer = gfx-pagelib.js __gfxDumpVfxData; grammar
// MEASURED from the committed vfxdata-frozen.txt (the entire real
// corpus validates with zero false rejections): magic first line, then
// TPL blocks (TPL immediately followed by FRAMES, then any of
// COLOUR/SKIP/KEY inside the block), then SWORD lines, then one END as
// the last line, file \n-terminated, nothing after. Tokens: identifiers
// [A-Za-z][A-Za-z0-9]* (SWORD types are a subset), exact-decimal
// numbers -?(0|[1-9][0-9]*)(\.[0-9]+)?, balanced nonempty single
// bracket streams on KEY/SWORD. A file that RESEMBLES the grammar but
// deviates anywhere is corruption -> fail closed (the old scanner
// extracted plausible values from truncated/damaged corpora).
const NUM_RE = /^-?(0|[1-9][0-9]*)(\.[0-9]+)?$/;
const IDENT_RE = /^[A-Za-z][A-Za-z0-9]*$/;

function vfxCorpusDie(lineNo, why) {
  console.error(`iou: vfxdata corpus INVALID at line ${lineNo}: ${why} (fail closed — corrupt/truncated artifact)`);
  process.exit(1);
}

function validateBracketStream(toks, lineNo) {
  // one balanced nonempty [ ... ] group spanning the whole token list
  if (toks.length < 2 || toks[0] !== "[") vfxCorpusDie(lineNo, "stream must open with [");
  let depth = 0;
  for (let i = 0; i < toks.length; i++) {
    const t = toks[i];
    if (t === "[") depth++;
    else if (t === "]") {
      depth--;
      if (depth < 0) vfxCorpusDie(lineNo, "unbalanced ]");
      if (depth === 0 && i !== toks.length - 1) vfxCorpusDie(lineNo, "content after the closing ]");
    } else if (!NUM_RE.test(t)) {
      vfxCorpusDie(lineNo, `bad token '${t}'`);
    } else if (depth === 0) {
      vfxCorpusDie(lineNo, "number outside brackets");
    }
  }
  if (depth !== 0) vfxCorpusDie(lineNo, "unbalanced [");
}

function validateVfxCorpus(txt) {
  if (!txt.endsWith("\n")) vfxCorpusDie(0, "missing final newline");
  const lines = txt.slice(0, -1).split("\n");
  if (lines[0] !== "VFXDATA1") vfxCorpusDie(1, "bad magic");
  if (lines[lines.length - 1] !== "END") vfxCorpusDie(lines.length, "last line must be END");
  // state: 0 = before first TPL, 1 = expect FRAMES (just saw TPL),
  // 2 = inside a TPL block, 3 = in the SWORD tail
  let state = 0, tplCount = 0;
  for (let i = 1; i < lines.length - 1; i++) {
    const ln = i + 1;
    const line = lines[i];
    const sp = line.split(" ");
    const kind = sp[0];
    if (kind === "END") vfxCorpusDie(ln, "END before the last line");
    if (state === 1) {
      if (kind !== "FRAMES" || sp.length !== 2 || !/^(0|[1-9][0-9]*)$/.test(sp[1])) {
        vfxCorpusDie(ln, "TPL must be immediately followed by a FRAMES line");
      }
      state = 2;
      continue;
    }
    if (kind === "TPL") {
      if (state === 3) vfxCorpusDie(ln, "TPL after the SWORD tail");
      if (sp.length !== 2 || !IDENT_RE.test(sp[1])) vfxCorpusDie(ln, "bad TPL line");
      state = 1;
      tplCount++;
      continue;
    }
    if (kind === "SWORD") {
      if (state !== 2 && state !== 3) vfxCorpusDie(ln, "SWORD outside the tail");
      if (sp.length < 3 || !IDENT_RE.test(sp[1])) vfxCorpusDie(ln, "bad SWORD line");
      validateBracketStream(sp.slice(2), ln);
      state = 3;
      continue;
    }
    if (state !== 2) vfxCorpusDie(ln, `${kind} line outside a TPL block`);
    if (kind === "COLOUR") {
      if (sp.length !== 4 || !sp.slice(1).every((t) => /^(0|[1-9][0-9]*)$/.test(t))) {
        vfxCorpusDie(ln, "bad COLOUR line");
      }
    } else if (kind === "SKIP") {
      if (sp.length !== 3 || !IDENT_RE.test(sp[1]) || !IDENT_RE.test(sp[2])) {
        vfxCorpusDie(ln, "bad SKIP line");
      }
    } else if (kind === "KEY") {
      if (sp.length < 3 || !IDENT_RE.test(sp[1])) vfxCorpusDie(ln, "bad KEY line");
      validateBracketStream(sp.slice(2), ln);
    } else {
      vfxCorpusDie(ln, `unknown line kind '${kind}'`);
    }
  }
  if (state === 1) vfxCorpusDie(lines.length, "TPL without FRAMES at corpus end");
  if (tplCount === 0) vfxCorpusDie(lines.length, "no TPL blocks");
}

let vfxCorpusValidated = null;

function parseVfxTplKey(txt, tpl, key) {
  // VFXDATA1 lookup — only ever runs over a corpus that passed the full
  // validation above (cached per content; iou.js reads one file).
  if (vfxCorpusValidated !== txt) {
    validateVfxCorpus(txt);
    vfxCorpusValidated = txt;
  }
  const lines = txt.split("\n");
  let inTpl = false;
  for (const line of lines) {
    if (line.startsWith("TPL ")) { inTpl = (line === "TPL " + tpl); continue; }
    if (!inTpl || !line.startsWith("KEY " + key + " ")) continue;
    const toks = line.slice(("KEY " + key + " ").length).split(" ");
    const stack = [[]];
    for (const t of toks) {
      if (t === "[") { const a = []; stack[stack.length - 1].push(a); stack.push(a); }
      else if (t === "]") {
        if (stack.length < 2) { console.error(`iou: vfxdata ${tpl}.${key}: unbalanced ]`); process.exit(1); }
        stack.pop();
      } else {
        const v = Number(t);
        if (!Number.isFinite(v)) { console.error(`iou: vfxdata ${tpl}.${key}: bad token '${t}'`); process.exit(1); }
        stack[stack.length - 1].push(v);
      }
    }
    if (stack.length !== 1 || stack[0].length !== 1) {
      console.error(`iou: vfxdata ${tpl}.${key}: malformed nesting`);
      process.exit(1);
    }
    return stack[0][0];
  }
  console.error(`iou: vfxdata: TPL ${tpl} KEY ${key} not found`);
  process.exit(1);
}

function tplBounds(node, what) {
  // Walk nested arrays; innermost numeric arrays are flat even-length
  // (x, y) coordinate lists (rows of arity 2 or 6 — beziers included).
  const b = { minX: Infinity, maxX: -Infinity, minY: Infinity, maxY: -Infinity };
  (function walk(n) {
    if (!Array.isArray(n)) {
      console.error(`iou: ${what}: non-array node in template walk`);
      process.exit(1);
    }
    if (n.length && typeof n[0] === "number") {
      if (n.some((v) => typeof v !== "number") || n.length % 2 !== 0) {
        console.error(`iou: ${what}: innermost row is not an even numeric list`);
        process.exit(1);
      }
      for (let i = 0; i < n.length; i += 2) {
        if (n[i] < b.minX) b.minX = n[i];
        if (n[i] > b.maxX) b.maxX = n[i];
        if (n[i + 1] < b.minY) b.minY = n[i + 1];
        if (n[i + 1] > b.maxY) b.maxY = n[i + 1];
      }
      return;
    }
    n.forEach(walk);
  })(node);
  if (!Number.isFinite(b.minX)) {
    console.error(`iou: ${what}: empty template bounds`);
    process.exit(1);
  }
  return b;
}

function injectRegions() {
  const stages = JSON.parse(fs.readFileSync(STAGES, "utf8"));
  const st = stages.stages && stages.stages[Number(STAGE_IDX)];
  if (!st || !st.scale || typeof st.scale.bits !== "string" ||
      !Array.isArray(st.offset) || st.offset.length !== 2 ||
      typeof st.offset[0] !== "number" || typeof st.offset[1] !== "number") {
    console.error("iou: stages.json: stage " + STAGE_IDX + " transform missing/malformed");
    process.exit(1);
  }
  // Exact hex grammar (review-65 r2 M7): Buffer.from(_, "hex") silently
  // truncates at the first invalid character and ignores trailing
  // bytes — a corrupt/suffixed bits string would parse to a plausible
  // double. Measured producer form (pipeline stages.json): exactly 16
  // lowercase hex chars. Resembles-but-doesn't-match = corruption.
  if (!/^[0-9a-f]{16}$/.test(st.scale.bits)) {
    console.error("iou: stages.json: scale.bits fails the exact 16-lowercase-hex grammar (fail closed — corrupt artifact)");
    process.exit(1);
  }
  const S = Buffer.from(st.scale.bits, "hex").readDoubleBE(0);
  if (!Number.isFinite(S) || S <= 0) {
    console.error("iou: stages.json: bad stage scale");
    process.exit(1);
  }
  const OX = st.offset[0], OY = st.offset[1];
  const s45 = S / 4.5;
  const vfxTxt = fs.readFileSync(VFXDATA, "utf8");
  const M = 3; // canvas-px margin (raster/AA boundary)

  const regions = [];
  for (const cfg of INJ.configs) {
    if (!PIN.inkNames.includes(cfg.name)) continue; // gated no-draw pair
    const cX = cfg.pos.x * S + OX;
    const cY = -cfg.pos.y * S + OY;
    let x0, x1, y0, y1;
    if (cfg.name === "dashDust") {
      const b = tplBounds(parseVfxTplKey(vfxTxt, "dashDust", "path")[0], "dashDust path[0]");
      const k = 0.2 * s45;
      const ax = Math.max(Math.abs(b.minX), Math.abs(b.maxX)) * k;
      x0 = cX - ax; x1 = cX + ax;
      y0 = cY + b.minY * k; y1 = cY + b.maxY * k;
    } else if (cfg.name === "groundBounce") {
      const b = tplBounds(parseVfxTplKey(vfxTxt, "groundBounce", "path")[0], "groundBounce path[0]");
      const r = Math.max(Math.abs(b.minX), Math.abs(b.maxX),
                         Math.abs(b.minY), Math.abs(b.maxY)) * 0.2 * s45;
      x0 = cX - r; x1 = cX + r; y0 = cY - r; y1 = cY + r;
    } else if (cfg.name === "firefoxcharge") {
      const paths = parseVfxTplKey(vfxTxt, "firefoxcharge", "path");
      // review-65 r2 M3: the frame index is cfg.f — upstream drawVfx
      // maps f -> vfxQueue[..].facing and firefoxcharge.js draws
      // path[facing] + path[(facing+4)%10] (cfg.face is the MIRROR flag
      // fed to drawArrayPathNew, not a frame). The old cfg.face read
      // measured frames 1/5 instead of the drawn 3/7 (narrower bounds).
      const facing = cfg.f;
      if (!Number.isInteger(facing) || facing < 0 || facing >= paths.length) {
        console.error("iou: firefoxcharge inject config f (frame index) out of template range");
        process.exit(1);
      }
      const second = (facing + 4) % 10;
      if (second >= paths.length) {
        console.error("iou: firefoxcharge second frame out of template range");
        process.exit(1);
      }
      const b1 = tplBounds(paths[facing], "firefoxcharge path[facing]");
      const b2 = tplBounds(paths[second], "firefoxcharge path[second]");
      const b = { minX: Math.min(b1.minX, b2.minX), maxX: Math.max(b1.maxX, b2.maxX),
                  minY: Math.min(b1.minY, b2.minY), maxY: Math.max(b1.maxY, b2.maxY) };
      const ax = Math.max(Math.abs(b.minX), Math.abs(b.maxX)) * 0.35 * s45;
      x0 = cX - ax; x1 = cX + ax;
      y0 = cY + b.minY * 0.5 * s45; y1 = cY + b.maxY * 0.5 * s45;
    } else if (cfg.name === "firefoxtail") {
      const tY = -(cfg.pos.y + 4) * S + OY;
      const r = 6 * S;
      x0 = cX - r; x1 = cX + r; y0 = tY - r; y1 = tY + r;
    } else if (cfg.name === "shine") {
      const r = 7 * S;
      x0 = cX - r; x1 = cX + r; y0 = cY - r; y1 = cY + r;
    } else {
      console.error("iou: no region derivation for inkNames effect '" + cfg.name +
        "' — extend injectRegions() with a documented derivation");
      process.exit(1);
    }
    regions.push({ name: cfg.name, x0: x0 - M, x1: x1 + M, y0: y0 - M, y1: y1 + M });
  }
  if (regions.length !== PIN.inkNames.length) {
    console.error("iou: region derivation did not cover every inkNames effect");
    process.exit(1);
  }
  return regions;
}

// device-space (240x240 C pgm) box for a canvas-space region: the C
// retarget is k = 0.2 (= /5, the same factor as the browser 5x5
// downscale) plus the dy = 45 letterbox shift; +-1 device px margin.
function deviceBox(r) {
  return {
    x0: Math.max(0, Math.floor(r.x0 / 5) - 1),
    x1: Math.min(W - 1, Math.ceil(r.x1 / 5) + 1),
    y0: Math.max(BAND_Y0, Math.floor(r.y0 / 5) + BAND_Y0 - 1),
    y1: Math.min(BAND_Y0 + BAND_H - 1, Math.ceil(r.y1 / 5) + BAND_Y0 + 1),
  };
}

function loadPgm(fp) {
  const b = fs.readFileSync(fp);
  const head = `P5\n240 240\n255\n`;
  const hb = Buffer.from(head, "ascii");
  if (b.length !== hb.length + 240 * 240 || !b.subarray(0, hb.length).equals(hb)) {
    console.error(`iou: ${fp}: not the expected 240x240 P5 PGM`);
    process.exit(1);
  }
  return b.subarray(hb.length);
}

const results = [];
let minIou = Infinity;
let fail = 0;
for (const f of exp.sampledFrames) {
  const tag = String(f).padStart(4, "0");
  const maskFp = path.join(CANVAS, `f${tag}.mask.bin`);
  const pgmFp = path.join(RENDER, `f${tag}.pgm`);
  if (!fs.existsSync(maskFp) || fs.statSync(maskFp).size === 0) {
    console.error(`iou: ${maskFp}: missing or empty mask (corpus pin: every pinned frame needs a non-empty mask)`);
    process.exit(1);
  }
  const src = fs.readFileSync(maskFp);
  if (src.length !== SRC_W * SRC_H) {
    console.error(`iou: ${maskFp}: ${src.length} bytes (want ${SRC_W * SRC_H})`);
    process.exit(1);
  }
  const cink = loadPgm(pgmFp);

  // C ink must stay inside the letterbox band
  for (let y = 0; y < 240; y++) {
    if (y >= BAND_Y0 && y < BAND_Y0 + BAND_H) continue;
    for (let x = 0; x < W; x++) {
      if (cink[y * W + x]) {
        console.error(`iou: frame ${f}: C ink outside the letterbox band at (${x},${y})`);
        process.exit(1);
      }
    }
  }

  // browser 5x5 any-ink downscale + IoU over the band
  let inter = 0, union = 0;
  for (let Y = 0; Y < BAND_H; Y++) {
    for (let X = 0; X < W; X++) {
      let a = 0;
      for (let dy = 0; dy < 5 && !a; dy++) {
        const row = (Y * 5 + dy) * SRC_W + X * 5;
        for (let dx = 0; dx < 5; dx++) {
          if (src[row + dx]) { a = 1; break; }
        }
      }
      const b = cink[(Y + BAND_Y0) * W + X] ? 1 : 0;
      if (a & b) inter++;
      if (a | b) union++;
    }
  }
  if (union === 0) {
    console.error(`iou: frame ${f}: both masks empty — degenerate sample`);
    process.exit(1);
  }
  const iou = inter / union;
  if (iou < minIou) minIou = iou;
  const pass = iou >= exp.iouThreshold;
  if (!pass) fail++;
  results.push({ frame: f, iou: iou, intersection: inter, union: union, pass: pass });
  console.log(`IOU f${tag} ${iou.toFixed(4)} (${inter}/${union}) ${pass ? "PASS" : "FAIL"}`);
}

// --- U1: the BACKGROUND planes -------------------------------------------
//
// Same downscale/band methodology as the fg above, applied to the BG2
// plane (browser f<tag>.bg.bin = layers.BG2 alpha; C f<tag>.bg.pgm = the
// ink laid down by drawStars/drawTunnel alone). Both sides walk the same
// mirrored mulberry32 from the same start state (gfx-pagelib.js
// __gfxBgInit / gfx_bg.c gfx_bg_reset), so this is a real geometry
// comparison, not a statistical one.
function bandIou(src, cink, what) {
  let inter = 0, union = 0;
  for (let Y = 0; Y < BAND_H; Y++) {
    for (let X = 0; X < W; X++) {
      let a = 0;
      for (let dy = 0; dy < 5 && !a; dy++) {
        const row = (Y * 5 + dy) * SRC_W + X * 5;
        for (let dx = 0; dx < 5; dx++) {
          if (src[row + dx]) { a = 1; break; }
        }
      }
      const b = cink[(Y + BAND_Y0) * W + X] ? 1 : 0;
      if (a & b) inter++;
      if (a | b) union++;
    }
  }
  if (union === 0) {
    console.error(`iou: ${what}: both background masks empty — degenerate sample`);
    process.exit(1);
  }
  return { iou: inter / union, inter: inter, union: union };
}

function bgLeg(label, pairs, threshold) {
  let min = Infinity, bad = 0;
  for (const [what, maskFp, pgmFp] of pairs) {
    if (!fs.existsSync(maskFp) || fs.statSync(maskFp).size === 0) {
      console.error(`iou: ${maskFp}: missing or empty background mask`);
      process.exit(1);
    }
    const src = fs.readFileSync(maskFp);
    if (src.length !== SRC_W * SRC_H) {
      console.error(`iou: ${maskFp}: ${src.length} bytes (want ${SRC_W * SRC_H})`);
      process.exit(1);
    }
    const cink = loadPgm(pgmFp);
    for (let y = 0; y < 240; y++) {
      if (y >= BAND_Y0 && y < BAND_Y0 + BAND_H) continue;
      for (let x = 0; x < W; x++) {
        if (cink[y * W + x]) {
          console.error(`iou: ${what}: C background ink outside the letterbox band at (${x},${y})`);
          process.exit(1);
        }
      }
    }
    const r = bandIou(src, cink, what);
    if (r.iou < min) min = r.iou;
    const pass = r.iou >= threshold;
    if (!pass) bad++;
    bgResults.push({ leg: label, frame: what, iou: r.iou, pass: pass });
    console.log(`BG ${label} ${what} ${r.iou.toFixed(4)} (${r.inter}/${r.union}) ${pass ? "PASS" : "FAIL"}`);
  }
  console.log(`BG ${label} MIN ${min.toFixed(4)} threshold ${threshold} frames ${pairs.length}`);
  return bad;
}

const bgResults = [];
let bgFail = 0;

bgFail += bgLeg("stars", exp.sampledFrames.map((f) => {
  const tag = String(f).padStart(4, "0");
  return [`f${tag}`, path.join(CANVAS, `f${tag}.bg.bin`), path.join(RENDER, `f${tag}.bg.pgm`)];
}), exp.bgIouThreshold);

// H1 (review-u1 r1): the aggregate stars-leg IoU above is DOMINATED by
// the two mountain fills — measured, a mountains-only render with all 20
// star circles deleted still scores >= 0.9927 on every sampled frame,
// because stars draw only on bgSparkle==0 frames (6 of the 24) and
// contribute 27-67 device cells against a ~16,000-cell union. An
// aggregate threshold over a whole plane cannot see a feature that is
// entirely missing from it. So the starfield gets its OWN judge.
//
// It compares a STAR-ONLY PLANE captured on both sides over the whole
// letterbox band: gfx_bg.c fires its star sink between the circles and
// the mountains, and the browser mirrors upstream's own bg2.arc()+fill()
// calls onto a scratch layer. The first version instead judged the rows
// no mountain can reach ([45,114) after the overshoot correction), which
// review-u1 r4 showed still left a hole — stars draw below that line
// too, so clipping only the lower ones passed. Measuring the plane
// directly needs no row argument at all.
{
  let starFrames = 0, min = Infinity;
  for (const f of exp.sampledFrames) {
    const tag = String(f).padStart(4, "0");
    const src = fs.readFileSync(path.join(CANVAS, `f${tag}.star.bin`));
    if (src.length !== SRC_W * SRC_H) {
      console.error(`iou: f${tag}.star.bin: ${src.length} bytes (want ${SRC_W * SRC_H})`);
      process.exit(1);
    }
    const cink = loadPgm(path.join(RENDER, `f${tag}.star.pgm`));
    // same band-leak guard the fg loop and bgLeg carry: the star plane is
    // the one place a retarget bug would land outside the compared window
    for (let y = 0; y < 240; y++) {
      if (y >= BAND_Y0 && y < BAND_Y0 + BAND_H) continue;
      for (let x = 0; x < W; x++) {
        if (cink[y * W + x]) {
          console.error(`iou: BG starfield f${tag}: C star ink outside the letterbox band at (${x},${y})`);
          process.exit(1);
        }
      }
    }
    let inter = 0, union = 0, bi = 0, ci = 0;
    for (let Y = 0; Y < BAND_H; Y++) {
      for (let X = 0; X < W; X++) {
        let a = 0;
        for (let dy = 0; dy < 5 && !a; dy++) {
          const row = (Y * 5 + dy) * SRC_W + X * 5;
          for (let dx = 0; dx < 5; dx++) {
            if (src[row + dx]) { a = 1; break; }
          }
        }
        const b = cink[(Y + BAND_Y0) * W + X] ? 1 : 0;
        if (a) bi++;
        if (b) ci++;
        if (a & b) inter++;
        if (a | b) union++;
      }
    }
    if (bi === 0) {
      // no stars visible this frame; the C must not invent any either
      if (ci !== 0) {
        console.error(`iou: BG starfield f${tag}: C drew ${ci} star cells where the browser drew none`);
        process.exit(1);
      }
      continue;
    }
    starFrames++;
    const iou = inter / union;
    if (iou < min) min = iou;
    const pass = iou >= exp.bgStarIouThreshold;
    if (!pass) bgFail++;
    bgResults.push({ leg: "starfield", frame: `f${tag}`, iou: iou, pass: pass });
    console.log(`BG starfield f${tag} ${iou.toFixed(4)} (${inter}/${union}) ` +
      `browser=${bi} C=${ci} ${pass ? "PASS" : "FAIL"}`);
  }
  // A starless renderer must not pass by simply never producing a
  // star-bearing frame: the COUNT is pinned.
  if (starFrames !== exp.bgStarFrameCount) {
    console.error(`iou: BG starfield pin violated — ${starFrames} star-bearing frames, pinned ${exp.bgStarFrameCount}`);
    process.exit(1);
  }
  console.log(`BG starfield MIN ${min.toFixed(4)} threshold ${exp.bgStarIouThreshold} ` +
    `frames ${starFrames} (of ${exp.sampledFrames.length} sampled; star-only plane, full band)`);
}

// --- U3: the ARTICLE plane -------------------------------------------------
// Same argument as the starfield leg above, on the FOREGROUND side. The fg
// IoU is an aggregate over players + articles + vfx + overlay, and an
// aggregate cannot see a feature that is entirely missing from it:
// MEASURED (iter 134) a C build with renderArticles stubbed out entirely
// still scores IOU MIN 0.9143 against the frozen 0.88 fg bound and exits
// 0 — articles move only 22-52 device cells on the 4 sampled frames that
// carry a laser, against ~3,700-cell unions. Third instance of the class
// (stars 0.9927; blend565's wrong blend survived because the fg leg
// judges binary ink masks).
//
// So articles get their own plane, produced on both sides by drawing the
// article pass ALONE: gfx_render.c clears the ink plane around
// render_articles (fNNNN.art.pgm), and the browser replays
// renderArticles onto a cleared FG2 after the frame is captured
// (gfx-pagelib.js __gfxArticlePlane -> fNNNN.art.bin). Measuring the
// plane directly needs no argument about where a laser may appear —
// exactly the U1 lesson.
//
// Two pins keep an empty judgment from passing: every frame the BROWSER
// draws articles on must be article-bearing on the C side too (an empty
// C plane against a non-empty browser plane is IoU 0 = FAIL, never a
// skip), and the number of article-bearing frames is FROZEN, so a
// renderer that simply never emits an article cannot pass by producing
// no comparable frames.
// Own counter and own result list (review-134 r1 L1): folding article
// failures into bgFail/bgResults made them print as "background
// judgment(s)" and serialize under `background.samples`, so an archived
// report could not tell the U3 judgment apart from the U1 ones.
const artResults = [];
let artFail = 0;
let artMin = Infinity;
{
  let artFrames = 0, min = Infinity;
  let artContained = 0;
  let artRestored = 0;
  const seenArtFrames = [];
  for (const f of exp.sampledFrames) {
    const tag = String(f).padStart(4, "0");
    const maskFp = path.join(CANVAS, `f${tag}.art.bin`);
    if (!fs.existsSync(maskFp) || fs.statSync(maskFp).size === 0) {
      console.error(`iou: ${maskFp}: missing or empty article mask ` +
        `(corpus pin: every sampled frame needs one)`);
      process.exit(1);
    }
    const src = fs.readFileSync(maskFp);
    if (src.length !== SRC_W * SRC_H) {
      console.error(`iou: ${maskFp}: ${src.length} bytes (want ${SRC_W * SRC_H})`);
      process.exit(1);
    }
    const cink = loadPgm(path.join(RENDER, `f${tag}.art.pgm`));
    // same band-leak guard the fg loop, bgLeg and the starfield leg carry
    for (let y = 0; y < 240; y++) {
      if (y >= BAND_Y0 && y < BAND_Y0 + BAND_H) continue;
      for (let x = 0; x < W; x++) {
        if (cink[y * W + x]) {
          console.error(`iou: article f${tag}: C article ink outside the letterbox band at (${x},${y})`);
          process.exit(1);
        }
      }
    }
    // ART-PLANE CONTAINMENT (independent-reviewer Low, review-134). The ART
    // leg's whole value is that it judges the article pass OF THE FRAME THE
    // FG LEG JUDGED. Nothing else established that link: on the browser side
    // `__gfxArticlePlane` replays renderArticles on a cleared fg2 AFTER the
    // frame's own render, so the replay inherits end-of-frame canvas state
    // rather than the state the real mid-render pass ran under (upstream
    // renderVfx has many fg2 translate/rotate/scale sites); on the C side the
    // sink saves/clears/ORs-back the ink plane. If either replay were
    // transformed, clipped or mis-restored relative to the real pass, a C
    // renderer that matched the REPLAY could pass while disagreeing with the
    // frame the fg leg judged — a false GREEN, the worst direction a judge
    // can fail in. MEASURED at 0 violations on all four article frames on
    // BOTH sides (f0184 carries live vfx, so the inherited-CTM case is
    // exercised), so this FREEZES a measured invariant: it adds no tolerance
    // and can mask nothing.
    {
      const fgMaskFp = path.join(CANVAS, `f${tag}.mask.bin`);
      const fgSrc = fs.readFileSync(fgMaskFp);
      if (fgSrc.length !== SRC_W * SRC_H) {
        console.error(`iou: ${fgMaskFp}: ${fgSrc.length} bytes (want ${SRC_W * SRC_H})`);
        process.exit(1);
      }
      let bviol = 0, bviolAt = -1;
      for (let i = 0; i < src.length; i++) {
        if (src[i] && !fgSrc[i]) { bviol++; if (bviolAt < 0) bviolAt = i; }
      }
      if (bviol !== 0) {
        console.error(`iou: article f${tag}: ${bviol} browser article pixel(s) are NOT set in that ` +
          `frame's judged fg mask (first at ${bviolAt % SRC_W},${Math.floor(bviolAt / SRC_W)}) — the ` +
          `replayed article plane is not part of the plane the fg leg judged`);
        process.exit(1);
      }
      const fgInk = loadPgm(path.join(RENDER, `f${tag}.pgm`));
      // The C-side FRAME-TAG arm. This one is deliberately stated for what
      // it can actually catch and NOTHING MORE (review-134 indep-2 M1): it
      // is NOT the twin of the browser arm above. On the C side there is no
      // replay — the article pass IS the real mid-render pass — and this
      // raster's ink is set-only (raster.c writes it exclusively as
      // 1/memset-1) with its only clears at PROF stages 0-1, strictly
      // before the article pass. So `article subset of final fg` is true
      // for ANY renderer and ANY sink, and it was measured green with the
      // OR-back deleted. What it still detects is a frame-tag misalignment
      // between art_sink's dump and the fg dump (they are tagged from
      // g_bgFrame and f+1 independently), which would pair an article plane
      // with the wrong frame's fg plane. Kept for that, named for that.
      let cviol = 0, cviolAt = -1;
      for (let i = 0; i < cink.length; i++) {
        if (cink[i] && !fgInk[i]) { cviol++; if (cviolAt < 0) cviolAt = i; }
      }
      if (cviol !== 0) {
        console.error(`iou: article f${tag}: ${cviol} C article cell(s) are NOT set in that frame's ` +
          `C fg ink plane (first at ${cviolAt % W},${Math.floor(cviolAt / W)}) — the article plane and ` +
          `the fg plane are not from the same frame`);
        process.exit(1);
      }
      // THE C-SIDE RESTORATION ARM — the one that is contingent on the
      // isolation working (review-134 indep-2 M1, which found the arm above
      // tautological with respect to the failure it used to name). The sink
      // publishes the plane it SAVED before clearing: everything drawn
      // before the article pass. That ink is removed from the live plane by
      // the memset and returns ONLY because the OR-back puts it back, so
      // `saved subset of final fg` is FALSE the moment the OR-back is
      // removed, truncated, or given the wrong length — and it is toothed by
      // editing gfx_render.c, not by editing a .pgm (the self-referential
      // negative-test class CLAUDE.md already registers). Runs on all 24
      // sampled frames, not just the four article-bearing ones, because the
      // isolation runs on every frame the sink is armed for.
      const artPre = loadPgm(path.join(RENDER, `f${tag}.artpre.pgm`));
      let pviol = 0, pviolAt = -1, pre = 0;
      for (let i = 0; i < artPre.length; i++) {
        if (artPre[i]) {
          pre++;
          if (!fgInk[i]) { pviol++; if (pviolAt < 0) pviolAt = i; }
        }
      }
      // A saved plane with no ink at all would make the arm vacuous, so the
      // premise is asserted rather than assumed: stage + players are drawn
      // before the article pass on every sampled frame.
      if (pre === 0) {
        console.error(`iou: article f${tag}: the saved pre-article plane is EMPTY — the restoration ` +
          `check would be vacuous (stage/player ink is drawn before the article pass)`);
        process.exit(1);
      }
      if (pviol !== 0) {
        console.error(`iou: article f${tag}: ${pviol} of ${pre} pre-article cell(s) are MISSING from ` +
          `that frame's C fg ink plane (first at ${pviolAt % W},${Math.floor(pviolAt / W)}) — the ` +
          `article sink's save/clear/OR-back did not restore the plane it isolated`);
        process.exit(1);
      }
      artRestored++;
      artContained++;
    }
    let inter = 0, union = 0, bi = 0, ci = 0;
    for (let Y = 0; Y < BAND_H; Y++) {
      for (let X = 0; X < W; X++) {
        let a = 0;
        for (let dy = 0; dy < 5 && !a; dy++) {
          const row = (Y * 5 + dy) * SRC_W + X * 5;
          for (let dx = 0; dx < 5; dx++) {
            if (src[row + dx]) { a = 1; break; }
          }
        }
        const b = cink[(Y + BAND_Y0) * W + X] ? 1 : 0;
        if (a) bi++;
        if (b) ci++;
        if (a & b) inter++;
        if (a | b) union++;
      }
    }
    if (bi === 0) {
      // no articles on screen this frame; the C must not invent any
      if (ci !== 0) {
        console.error(`iou: article f${tag}: C drew ${ci} article cells where the browser drew none`);
        process.exit(1);
      }
      continue;
    }
    artFrames++;
    seenArtFrames.push(f);
    const iou = inter / union;
    if (iou < min) min = iou;
    const floor = exp.artFrameFloors[`f${tag}`];
    if (typeof floor !== "number") {
      console.error(`iou: article f${tag} is article-bearing but has no pinned floor in artFrameFloors`);
      process.exit(1);
    }
    const pass = iou >= floor;
    if (!pass) artFail++;
    artResults.push({ frame: `f${tag}`, iou: iou, intersection: inter,
                      union: union, browserCells: bi, cCells: ci, pass: pass });
    console.log(`ART f${tag} ${iou.toFixed(4)} (${inter}/${union}) ` +
      `browser=${bi} C=${ci} floor ${floor} ${pass ? "PASS" : "FAIL"}`);
  }
  if (artFrames !== exp.artFrameCount) {
    console.error(`iou: article pin violated — ${artFrames} article-bearing frames, pinned ${exp.artFrameCount}`);
    process.exit(1);
  }
  // r3 M2: WHICH frames, not just how many. sampledFrames is iterated in
  // order, so this is an ordered comparison against the frozen list.
  if (seenArtFrames.join(",") !== exp.artFrames.join(",")) {
    console.error(`iou: article pin violated — article-bearing frames [${seenArtFrames}] ` +
      `!= the frozen set [${exp.artFrames}]`);
    process.exit(1);
  }
  // The containment invariant is checked on EVERY sampled frame, not only
  // the article-bearing ones (it runs before the no-articles `continue`), so
  // this cardinality is an exact-coverage assertion.
  if (artContained !== exp.sampledFrames.length) {
    console.error(`iou: article containment ran on ${artContained} frames, want ${exp.sampledFrames.length}`);
    process.exit(1);
  }
  // Separate counter for the restoration arm so the published line cannot
  // claim coverage the contingent check did not actually get (review-134
  // indep-2 M1).
  if (artRestored !== exp.sampledFrames.length) {
    console.error(`iou: article pre-plane restoration ran on ${artRestored} frames, want ${exp.sampledFrames.length}`);
    process.exit(1);
  }
  console.log(`ART CONTAIN OK ${artContained}/${exp.sampledFrames.length} frames ` +
    `(browser article plane subset of judged fg mask; C article plane same-frame as C fg ink; ` +
    `C pre-article plane restored into C fg ink ${artRestored}/${exp.sampledFrames.length})`);
  artMin = min;
  console.log(`ART MIN ${min.toFixed(4)} per-frame floors ` +
    `frames ${artFrames} (of ${exp.sampledFrames.length} sampled; article-only plane, full band)`);
}

bgFail += bgLeg("tunnel", exp.bgTunnelFrames.map((f) => {
  const tag = String(f).padStart(4, "0");
  return [`t${tag}`, path.join(CANVAS, `t${tag}.bg.bin`), path.join(BG_TUNNEL, `f${tag}.bg.pgm`)];
}), exp.bgTunnelIouThreshold);

// The BG1 gradient, judged by COLOUR (its silhouette is all-ones on both
// sides — drawBackgroundInit fills the layer opaque edge to edge — so a
// mask would judge nothing).
//
// Browser: layers.BG1 read back per canvas row. C: the REAL rendered
// framebuffer, observed by gfx_bg.c right after the gradient rows land
// (review-u1 r1 H2 — an earlier version called the colour function
// directly, so deleting the row loop kept this green). The C side is
// therefore reported per device row in TWO forms: the RGB565 framebuffer
// value (proving the row loop wrote it) and the 8-bit colour the loop
// handed the rasterizer. gfx_replay hard-fails if those two disagree, so
// the 8-bit plane is an observation of the real path, not a second
// opinion about it. GFX_K is exactly 0.2, so device row y is canvas row
// (y - 45) * 5 with no resampling slack.
//
// WHY 8-BIT AND NOT THE FRAMEBUFFER (review-u1 r2 M1 / r3 L1, measured).
// Every MAGNITUDE statistic over the RGB565 rows fails to separate a
// correct gradient from a wrong one, because red has only ~4 distinct
// levels across the band. Measured, true renderer vs perturbations:
// an existential +-1 band passes endpoint 24->25, 17->18 and denominator
// 500->510 alike (delta 0 for all); a single-row or 5-row-mean reference
// gives delta 8 for ALL of them including the true one; least-squares
// fitting the quantized rows gives the TRUE renderer maxdA 3.59 vs
// 3.18-3.64 for the perturbations — no separation.
// SCOPE OF THAT CLAIM (r3 L1): it is about magnitude statistics, NOT a
// proof that RGB565 carries no signal. A COUNTING metric does separate —
// mismatch-count against a browser-fit reference measures TRUE 10 vs
// 16-26 for the perturbations. It was not adopted because its margin is
// a chosen cut between two nonzero counts that would have to be frozen
// against Chrome's dithering across cold captures, whereas the 8-bit
// comparison below is exact-by-construction against a bound (+-1) that
// is a measured property of the reference. Registered as the better
// upgrade path if the sub-2% slope blind spot ever needs closing.
//
// RESIDUAL, REGISTERED: Chrome DITHERS canvas gradients by +-1 (BG1 row 0
// reads darker than row 1 — non-monotonic) and its rendered ramp is not
// exactly the ideal line (least-squares over 500 rows: intercept 23.87,
// zero crossing at y~496 rather than 24 and 500). So this comparison
// cannot resolve differences below ~1 in 8-bit: a slope error under about
// 2% would pass. It DOES catch endpoint errors and any structural break.
// That bound is a property of the reference, not a dial.
function q565(c) {
  return [(c[0] >> 3) << 3, (c[1] >> 2) << 2, (c[2] >> 3) << 3];
}

// Strict BGGRAD1 parser (review-u1 r1 M4). Browser rows carry 3 channels,
// C rows carry 6 (framebuffer triple + observed 8-bit triple).
function loadGrad(fp, what, first, count, chans) {
  const txt = fs.readFileSync(fp, "utf8");
  if (!txt.endsWith("\n")) {
    console.error(`iou: ${fp}: ${what} dump does not end with a newline`);
    process.exit(1);
  }
  const lines = txt.slice(0, -1).split("\n");
  if (lines.length !== count + 2 || lines[0] !== "BGGRAD1" ||
      lines[lines.length - 1] !== "END") {
    console.error(`iou: ${fp}: ${what} dump is not BGGRAD1 + ${count} rows + END ` +
      `(got ${lines.length} lines)`);
    process.exit(1);
  }
  // canonical decimals only (review-u1 r6): `00` is not a form the
  // producer can emit, so accepting it would accept impossible evidence
  const D = "(?:0|[1-9][0-9]*)";
  const re = new RegExp("^R (" + D + ")" + (" (" + D + ")").repeat(chans) + "$");
  const rows = [];
  for (let i = 1; i < lines.length - 1; i++) {
    const m = re.exec(lines[i]);
    if (!m) {
      console.error(`iou: ${fp}: unparseable ${what} row '${lines[i]}'`);
      process.exit(1);
    }
    if (Number(m[1]) !== first + rows.length) {
      console.error(`iou: ${fp}: ${what} rows out of order at '${lines[i]}' ` +
        `(want index ${first + rows.length})`);
      process.exit(1);
    }
    const ch = m.slice(2).map(Number);
    if (ch.some((v) => v < 0 || v > 255)) {
      console.error(`iou: ${fp}: ${what} channel out of range at '${lines[i]}'`);
      process.exit(1);
    }
    rows.push(ch);
  }
  return rows;
}

const gradB = loadGrad(path.join(CANVAS, "bg1-grad.txt"), "browser gradient", 0, SRC_H, 3);
const gradC = loadGrad(BG_GRAD_C, "C gradient", BAND_Y0, BAND_H, 6);
let gradMax = 0, gradMaxRow = -1;
for (let y = BAND_Y0; y < BAND_Y0 + BAND_H; y++) {
  const canvasRow = (y - BAND_Y0) * 5; // GFX_K == 0.2 exactly
  const want = gradB[canvasRow];
  const row = gradC[y - BAND_Y0];
  const fb = row.slice(0, 3), obs = row.slice(3, 6);
  // re-check the C's own framebuffer/observation agreement judge-side too
  const qo = q565(obs);
  for (let c = 0; c < 3; c++) {
    if (qo[c] !== fb[c]) {
      console.error(`iou: C gradient row ${y}: observed 8-bit ${obs} does not quantize to framebuffer ${fb}`);
      process.exit(1);
    }
    const d = Math.abs(want[c] - obs[c]);
    if (d > gradMax) { gradMax = d; gradMaxRow = y; }
  }
}
const gradPass = gradMax <= exp.bgGradMaxChannelDelta;
console.log(`BG GRAD maxdelta ${gradMax} (device row ${gradMaxRow}) ` +
  `limit ${exp.bgGradMaxChannelDelta} ${gradPass ? "PASS" : "FAIL"}`);
if (!gradPass) bgFail++;

// --- per-effect injection assertions (review-65 M2, iter 67) ---------------
// Runs AFTER the frame loop so the injection frame's aggregate IoU line
// is already on record: a missing effect small enough to pass the
// aggregate is exactly what these tripwires exist to catch.
const injResults = [];
{
  const tag = String(INJ.frame).padStart(4, "0");
  const src = fs.readFileSync(path.join(CANVAS, `f${tag}.mask.bin`));
  if (src.length !== SRC_W * SRC_H) {
    console.error(`iou: injection-frame browser mask has ${src.length} bytes (want ${SRC_W * SRC_H})`);
    process.exit(1);
  }
  const withInk = loadPgm(path.join(RENDER, `f${tag}.pgm`));
  const noInk = loadPgm(path.join(RENDER_NOINJ, `f${tag}.pgm`));
  const regions = injectRegions();

  // Browser-side attributable differential (review-65 r2 M4): the
  // capture additionally rendered the injection frame under a
  // DETERMINISTIC page-local render RNG — one full render
  // (f<tag>.det.mask.bin) and one leave-one-out render per inkNames
  // effect (f<tag>.loo-<name>.mask.bin), each starting from the SAME
  // REWOUND render-RNG state (setState, not a reseed — C28), so det
  // and loo-X differ only by X's draws (plus the queue-order-bounded
  // RNG ripple into later movers' own regions — the same argument as
  // the C leave-one-out, now deterministic on the browser side too).
  // bdiff > 0 in X's region is browser ATTRIBUTION: a browser injection
  // that silently skipped X makes loo-X identical to det inside X's
  // region by construction — the presence-in-box check alone could not
  // see that (another layer's pixels keep presence nonzero).
  const loadBrowserMask = (fp) => {
    const b = fs.readFileSync(fp);
    if (b.length !== SRC_W * SRC_H) {
      console.error(`iou: ${fp}: ${b.length} bytes (want ${SRC_W * SRC_H})`);
      process.exit(1);
    }
    return b;
  };
  const detMask = loadBrowserMask(path.join(CANVAS, `f${tag}.det.mask.bin`));

  // Trajectory-continuity pin, JUDGE SIDE (review-70 r3, iter 71): the
  // capture's injection-frame CANONICAL render runs on the same
  // deterministic page-local render RNG and the det mask is a strict
  // REPLAY of it (pre-render snapshot restored + the render RNG rewound
  // to the same state, not reseeded) — byte-
  // identical by construction, asserted capture-side too (twin-pin
  // class). A divergence means the canonical render left the det
  // trajectory (e.g., a native-RNG canonical or a finally re-render
  // regression) and the loo attribution baselines no longer share the
  // trajectory frames 151+ continued from. Fail closed.
  if (Buffer.compare(src, detMask) !== 0) {
    console.error("iou: injection-frame canonical mask != det replay mask — " +
      "trajectory continuity broken (capture-canvas.js review-70 r3 contract; fail closed)");
    process.exit(1);
  }
  console.log(`INJ DET==CANONICAL f${tag} (trajectory continuity)`);

  for (const r of regions) {
    // (i) browser mask ink in the canvas-space region + (i') browser
    // LEAVE-ONE-OUT differential (det vs loo-X) in the same region
    let browser = 0, bdiff = 0;
    const looMask = loadBrowserMask(path.join(CANVAS, `f${tag}.loo-${r.name}.mask.bin`));
    const bx0 = Math.max(0, Math.floor(r.x0)), bx1 = Math.min(SRC_W - 1, Math.ceil(r.x1));
    const by0 = Math.max(0, Math.floor(r.y0)), by1 = Math.min(SRC_H - 1, Math.ceil(r.y1));
    for (let y = by0; y <= by1; y++) {
      for (let x = bx0; x <= bx1; x++) {
        const i = y * SRC_W + x;
        if (src[i]) browser++;
        if (detMask[i] !== looMask[i]) bdiff++;
      }
    }
    // (ii) C with-inject ink in the device-space box, plus the bg
    // honesty report (no-inject baseline ink sharing the region — the
    // amount by which (i)/(ii) are weakened; (iii) is unaffected).
    // (iii) LEAVE-ONE-OUT differential: full render vs the baseline that
    // dropped exactly this effect — attributing by construction.
    const looInk = loadPgm(path.join(`${RENDER_LOO}-${r.name}`, `f${tag}.pgm`));
    const d = deviceBox(r);
    let cink = 0, diff = 0, bg = 0;
    for (let y = d.y0; y <= d.y1; y++) {
      for (let x = d.x0; x <= d.x1; x++) {
        const a = withInk[y * W + x] ? 1 : 0;
        const n = noInk[y * W + x] ? 1 : 0;
        const l = looInk[y * W + x] ? 1 : 0;
        if (a) cink++;
        if (n) bg++;
        if (a !== l) diff++;
      }
    }
    const ok = browser > 0 && bdiff > 0 && cink > 0 && diff > 0;
    console.log(`INJ ${r.name} browser=${browser} bdiff=${bdiff} c=${cink} diff=${diff} bg=${bg} ${ok ? "OK" : "FAIL"}`);
    injResults.push({ name: r.name, browser, bdiff, c: cink, diff, bg, pass: ok });
    if (!ok) {
      console.error(`iou: injected effect '${r.name}' failed its per-effect ink assertion at frame ${INJ.frame} ` +
        "(browser presence, browser leave-one-out bdiff, C ink, and C leave-one-out diff must all be nonzero " +
        "in the derived region — a side is not drawing this effect)");
      process.exit(1);
    }
  }

  // region-soundness guard: every differential pixel anywhere in the C
  // frame must fall inside the union of derived (device-space) regions —
  // an effect drawing outside its documented bounds, or the gated
  // firefoxlaunch/shineloop pair drawing anything, is a loud death.
  const boxes = regions.map(deviceBox);
  for (let y = 0; y < 240; y++) {
    for (let x = 0; x < W; x++) {
      const a = withInk[y * W + x] ? 1 : 0;
      const n = noInk[y * W + x] ? 1 : 0;
      if (a === n) continue;
      let inside = false;
      for (const d of boxes) {
        if (x >= d.x0 && x <= d.x1 && y >= d.y0 && y <= d.y1) { inside = true; break; }
      }
      if (!inside) {
        console.error(`iou: differential ink OUTSIDE every derived injection region at device (${x},${y}) ` +
          "— region derivation unsound or an unexpected effect drew (fail closed)");
        process.exit(1);
      }
    }
  }
  console.log(`INJ REGIONS SOUND (all differential ink inside ${boxes.length} derived regions)`);
}

console.log(`IOU MIN ${minIou.toFixed(4)} threshold ${exp.iouThreshold} frames ${exp.sampledFrames.length}`);
if (REPORT && REPORT !== true) {
  fs.writeFileSync(REPORT, JSON.stringify({
    threshold: exp.iouThreshold,
    minIou: minIou,
    inject: { frame: INJ.frame, effects: injResults },
    frames: results,
    background: {
      bgIouThreshold: exp.bgIouThreshold,
      bgTunnelIouThreshold: exp.bgTunnelIouThreshold,
      bgGradMaxChannelDelta: exp.bgGradMaxChannelDelta,
      gradMaxDelta: gradMax,
      gradMaxRow: gradMaxRow,
      samples: bgResults,
    },
    // U3, its own section (review-134 r1 L1)
    article: {
      artFrameFloors: exp.artFrameFloors,
      artFrameCount: exp.artFrameCount,
      minIou: artMin === Infinity ? null : artMin,
      samples: artResults,
    },
  }, null, 2) + "\n");
}
if (fail > 0) {
  console.error(`iou: ${fail} frame(s) below threshold`);
  process.exit(1);
}
if (bgFail > 0) {
  console.error(`iou: ${bgFail} background judgment(s) below threshold`);
  process.exit(1);
}
if (artFail > 0) {
  console.error(`iou: ${artFail} article judgment(s) below threshold`);
  process.exit(1);
}
console.log("IOU OK");
