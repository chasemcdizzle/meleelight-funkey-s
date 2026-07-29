"use strict";
// Stage "assets" — upstream's own menu artwork, decoded and pre-scaled for
// the 240x240 FunKey-S frontend (fix_plan §M4 task A9; format IMG1,
// pipeline/FORMATS.md §7).
//
//   dist/assets/css/*.png          -> 5 character-select portraits
//   dist/assets/stage-icons/*.png  -> 6 VS-stage previews + the RANDOM icon
//   dist/assets/hand/*.png         -> 3 CSS hand cursors
//                                  -> assets/menu.img1  (one packed file)
//
// PROVENANCE: this artwork is Nintendo-derived (Melee character/stage art
// ripped upstream), exactly like the §5 audio blobs. PRIVATE USE ONLY —
// never distributed. It lives ONLY in gitignored build output (build*/);
// the repo commits hashes, never bytes. check-assets.sh enforces the
// no-commit guard.
//
// DETERMINISM: no external tool is involved. PNG decode is stdlib zlib +
// the spec's filter reconstruction (lib/png.js), resampling (in linear
// light) and 565 quantization are exact integer arithmetic (lib/img1.js —
// the sRGB transfer tables are BigInt exact-rational, never Math.pow), so
// two fresh runs are byte-identical by construction rather than by a
// version pin.
// Output bytes are frozen as expected-assets.json assets.artifactsSha256
// (a sibling of expected.json — see lib/check-assets-expected.js for why).

const fs = require("fs");
const path = require("path");
const { sha256, sha256File } = require("../lib/manifest");
const { decodePng } = require("../lib/png");
const { resizeRgba, encodeImg1, gammaTable, mapRgb } = require("../lib/img1");

const REPO_ROOT = path.join(__dirname, "..", "..");

// ---------------------------------------------------------------------------
// TARGET SIZES — the ONE design decision in this stage.
//
// A literal scale of upstream's draw rects is useless: upstream's canvas is
// 1200x750 (dist/meleelight.html:218-222), so its 81x58 portrait maps to
// 16x12 device pixels. The FOH is not a shrunken browser layout, it is a
// native 240x240 UI whose elements are proportionally far larger. So the
// only hand-chosen number per class is a target WIDTH, sized to the FOH
// element the image lands in; the height is DERIVED from the measured
// source dimensions at the source's own aspect ratio (never typed in), and
// lib/img1.js hard-throws if that ever implies an upscale.
//
//   portraits  58 px wide — the sources' OWN width: NO GEOMETRIC SCALING,
//              every destination pixel covers exactly one source pixel.
//              (Not "untouched": alpha-0 pixels are normalized to
//              (0,0,0,0) and colour is quantized to 565 like every other
//              image.) 56 was tried first and rejected on
//              measurement (review-a9 [L]): a 3.4% downscale in which NO
//              destination column is a pure source copy, which converted
//              crisp binary alpha into 6.04% partial alpha (marth) for no
//              layout gain. 4 across = 232 px still fits 240. There is no
//              portrait slot in the FOH CSS yet (foh_render.c render_css is
//              a text list), so native size gives the restyle lane the most
//              to work with — it can letterbox, it cannot un-blur.
//   stagePrev  65 px wide — the EXISTING FOH SSS cell is 65x44 with its
//              cursor frame drawn outside it (port/foh/foh_render.c:139),
//              so 65 wide fills the cell exactly and the 800x300 source
//              aspect leaves 24 px of height, 20 px free for the label
//   cursor     24 px wide — CHOSEN, not derived: upstream's 101x133 on a
//              1200x750 canvas is 8.4% of width / 17.7% of height, which
//              at 240 would be 20x27 — too small to read as a hand. 24x32
//              is ~10% of screen width at the source's aspect ratio.
//
// Re-sizing is a one-line edit here plus an expected-assets.json re-freeze.
//
// STAGE_PREVIEW_GAMMA — a DELIBERATE DEVIATION from upstream's pixels
// (owner ruling 2026-07-28, relayed by the driver; HARD RULE 5 territory,
// so it is documented as a deviation in FORMATS.md §7.2, not as a fix).
// Upstream's stage art is genuinely near-black (bf mean Y 9.18/255, and the
// SOURCE PNG PIXELS are byte-identical to upstream's own browser render —
// .loop/c4-dim/REPORT.md hop 1; the emitted IMG1 bytes are of course resized,
// tone-mapped and 565-quantized); it reads in a browser only because it is
// drawn 800x300 on a 1200x750 canvas. At 130x48 on a 240x240 panel it does
// not. The lift is scoped to the stagePreview CLASS only — never portraits,
// never cursors — and lives here, on the class, so no image is special-cased
// by name. It is an exact rational so the table stays integer: [3,4] = 0.75;
// the alternative the owner asked to see is [13,20] = 0.65.
const STAGE_PREVIEW_GAMMA = [3, 4];
const CLASSES = [
  { dir: "css", width: 58, kind: "portrait",
    files: [["marth", "marth"], ["puff", "puff"], ["fox", "fox"],
            ["falco", "falco"], ["falcon", "falcon"]] },
  // Stage order is the oracle --stage id order (0 battlefield .. 5 fountain,
  // CLAUDE.md §Commands), then the RANDOM icon.
  { dir: "stage-icons", width: 65, kind: "stagePreview", gamma: STAGE_PREVIEW_GAMMA,
    files: [["bf", "stage_bf"], ["ys", "stage_ys"], ["ps", "stage_ps"],
            ["dl", "stage_dl"], ["fd", "stage_fd"], ["fod", "stage_fod"],
            ["Icon_Transparent_Question", "stage_random"]] },
  { dir: "hand", width: 24, kind: "cursor",
    files: [["handpoint", "hand_point"], ["handopen", "hand_open"],
            ["handgrab", "hand_grab"]] },
];

const README = `# assets/ — Nintendo-derived artwork — PRIVATE USE ONLY

menu.img1 holds the upstream meleelight menu artwork (ripped Super Smash
Bros. Melee character and stage art), decoded and pre-scaled for the
240x240 device UI.

- PRIVATE USE ONLY. NEVER distribute this file, the binaries that embed
  it, or any package containing it (CLAUDE.md licensing rule;
  docs/LICENSING.md).
- Generated build output (gitignored via build*/): only manifests and
  hashes are committed, never these bytes.
- Format: IMG1 — RGB565 (little-endian, quantized to the nearest
  bit-replicable code, lib/img1.js quant565) plus an 8-bit alpha plane
  per image (pipeline/FORMATS.md section 7).
  Loader: port/gfx/img1.c.
`;

// Alpha domain of an RGBA buffer, measured (never assumed):
//   "opaque" — every pixel a==255
//   "binary" — only 0 and 255 occur (a 1-bit colour key would suffice)
//   "aa"     — partial alpha occurs (anti-aliased edges; needs 8-bit alpha)
function alphaClass(rgba) {
  let sawZero = false, sawPartial = false;
  for (let i = 3; i < rgba.length; i += 4) {
    const a = rgba[i];
    if (a === 0) sawZero = true;
    else if (a !== 255) { sawPartial = true; break; }
  }
  return sawPartial ? "aa" : sawZero ? "binary" : "opaque";
}

function run(ctx) {
  // Manifest paths are POSIX-separated LITERALS, never path.join() output
  // (review-a9-1 [L]): path.join is platform-dependent, so joining into
  // recorded metadata would emit backslashes on Windows and break the
  // byte-stability claim. path.join is used ONLY to touch the filesystem.
  const sources = [];
  for (const rel of ["pipeline/lib/png.js", "pipeline/lib/img1.js",
                     "pipeline/stages/assets.js"]) {
    sources.push({ path: rel, sha256: sha256File(path.join(REPO_ROOT, ...rel.split("/"))) });
  }

  const images = [];
  const perImage = {};
  const counts = { portrait: 0, stagePreview: 0, cursor: 0 };
  let pixels = 0;
  for (const cls of CLASSES) {
    // one table per CLASS, built once: the hook is the class, never a name.
    // The owner's ruling scoped the lift to stagePreview ONLY, so that scope
    // is ENFORCED here rather than left to convention (review-c4-3 [L]):
    // hanging `gamma` on the portrait or cursor class fails the run loudly.
    if (cls.gamma && cls.kind !== "stagePreview") {
      throw new Error(`assets: the gamma deviation is scoped to stagePreview ` +
        `by owner ruling (FORMATS.md §7.2.1); class ${cls.kind} may not carry it`);
    }
    const tone = cls.gamma ? gammaTable(cls.gamma) : null;
    for (const [stem, name] of cls.files) {
      const rel = `assets/${cls.dir}/${stem}.png`; // POSIX literal, see above
      const abs = path.join(ctx.distRoot, "dist", "assets", cls.dir, stem + ".png");
      const raw = fs.readFileSync(abs);
      sources.push({ path: "dist/" + rel, sha256: sha256(raw) });

      const src = decodePng(raw, rel);
      // Height derives from the MEASURED source aspect, never typed in.
      const w = cls.width;
      const h = Math.round((w * src.h) / src.w);
      const scaled = resizeRgba(src, w, h);
      // tone map AFTER the (light-correct) resample and BEFORE quantization,
      // RGB only — alpha, and therefore the pinned alpha class, is untouched
      const rgba = tone ? mapRgb(scaled, tone) : scaled;

      images.push({ name, w, h, rgba });
      counts[cls.kind]++;
      pixels += w * h;
      perImage[name] = {
        kind: cls.kind, source: rel,
        srcW: src.w, srcH: src.h, srcColorType: src.colorType,
        srcAlpha: alphaClass(src.rgba),
        w, h, alpha: alphaClass(rgba),
      };
    }
  }

  const blob = encodeImg1(images);
  const artifacts = [];
  const write = (name, content) => {
    const buf = Buffer.from(content);
    const abs = path.join(ctx.outDir, name);
    fs.mkdirSync(path.dirname(abs), { recursive: true });
    fs.writeFileSync(abs, buf);
    artifacts.push({ path: name, sha256: sha256(buf), bytes: buf.length });
    ctx.log(`  ${name}: ${buf.length} bytes`);
  };
  write("assets/menu.img1", blob);
  write("assets/README.md", README);
  ctx.log(`  assets: ${images.length} images (${counts.portrait} portraits, ` +
    `${counts.stagePreview} stage previews, ${counts.cursor} cursors), ` +
    `${pixels} pixels`);

  artifacts.sort((a, b) => (a.path < b.path ? -1 : 1));
  sources.sort((a, b) => (a.path < b.path ? -1 : 1));
  return {
    format: "IMG1",
    provenance: {
      contentOrigin: "Nintendo-derived (Super Smash Bros. Melee menu artwork, ripped upstream)",
      use: "PRIVATE USE ONLY - never distributed",
    },
    artifactsSha256: sha256(artifacts.map((a) => `${a.path} ${a.sha256}\n`).join("")),
    // The IMG1 directory ORDER is the consumer's index space (FORMATS.md
    // §7.1: character ids 0-4, stage ids 5-10). perImage is keyed by name
    // and therefore says nothing about order, so the order is recorded
    // here as an ordered array and pinned — otherwise two names could be
    // swapped against their pixels and every other check would still agree
    // with itself (review-a9-2 [M]).
    directory: images.map((im) => im.name),
    sources,
    coverage: {
      images: images.length,
      portraits: counts.portrait,
      stagePreviews: counts.stagePreview,
      cursors: counts.cursor,
      pixels,
      img1Bytes: blob.length,
    },
    perImage,
    artifacts,
  };
}

module.exports = { name: "assets", run };
