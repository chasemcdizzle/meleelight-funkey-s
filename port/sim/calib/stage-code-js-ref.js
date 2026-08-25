#!/usr/bin/env node
// stage-code-js-ref.js — the JS half of A45 T1's differential rig.
//
// It NEVER restates encode.js's logic. It transpiles upstream's OWN source
// bytes out of the READ-ONLY pinned clone with the clone's OWN babel
// (presets es2015 + stage-0, exactly package.json's `babel` block, plus
// flow-strip-types) into the gitignored build directory, requires the
// result, and calls createStageCode / parseStageCode. That is the
// fmt_diff discipline: a transcription bug would otherwise mirror itself
// on both sides of the differential and prove nothing.
//
//   transpile <clone> <outdir>
//   gen       <tpdir> <outdir>            corpora + meta.json
//   ref       <tpdir> <codes> <out>       "NULL" | "OK <re-encoded code>"
//   tofixed   <hex> <out>                 V8's own Number#toFixed(2)
//   judge     <builddir> <expected.json>  the measured-then-frozen pins
"use strict";

const fs = require("fs");
const path = require("path");

// The transitive source set of src/stages/encode.js. `./stage` is a
// type-only import and is erased by flow-strip-types.
const SOURCES = [
  "src/stages/encode.js",
  "src/main/util/Box2D.js",
  "src/main/util/Vec2D.js",
  "src/main/util/deepValue.js",
  "src/main/linAlg.js",
  "src/stages/util/extremePoint.js",
  "src/target/util/getConnected.js",
];

function transpile(clone, outdir) {
  const babel = require(path.join(clone, "node_modules", "babel-core"));
  for (const rel of SOURCES) {
    const src = fs.readFileSync(path.join(clone, rel), "utf8");
    const res = babel.transform(src, {
      presets: ["es2015", "stage-0"],
      plugins: ["transform-flow-strip-types"],
      babelrc: false,
      // Absolute, so babel resolves the presets/plugin from the CLONE's own
      // node_modules (upstream's toolchain), not from this repo.
      filename: path.join(clone, rel),
    });
    const dst = path.join(outdir, rel);
    fs.mkdirSync(path.dirname(dst), { recursive: true });
    fs.writeFileSync(dst, res.code);
  }
  process.stdout.write(
    "  transpiled " + SOURCES.length + " upstream sources (clone untouched)\n"
  );
}

function loadEncode(tpdir) {
  return require(path.resolve(tpdir, "src/stages/encode.js"));
}

// --- corpus ----------------------------------------------------------------

// mulberry32, the project's standard deterministic generator.
function rng(seed) {
  let a = seed >>> 0;
  return function () {
    a = (a + 0x6d2b79f5) >>> 0;
    let t = a;
    t = Math.imul(t ^ (t >>> 15), t | 1);
    t ^= t + Math.imul(t ^ (t >>> 7), t | 61);
    return ((t ^ (t >>> 14)) >>> 0) / 4294967296;
  };
}

const DAMAGE = ["fire", "electric", "slash", "darkness"];

function makeGen(r) {
  const ri = (n) => Math.floor(r() * n);
  // Coordinates deliberately span four regimes: the exact hundredth grid
  // (already canonical), raw doubles (the lossy first emission), the
  // thousandths that sit on toFixed's rounding boundary, and magnitudes
  // large enough to leave the 2^53 hundredths band comfortably behind but
  // still inside it.
  function coord() {
    const k = ri(4);
    if (k === 0) return (ri(80001) - 40000) / 100;
    if (k === 1) return r() * 1000 - 500;
    if (k === 2) return (ri(400001) - 200000) / 1000;
    return (ri(2000001) - 1000000) * (r() < 0.5 ? 1 : 1e4) / 100;
  }
  const vec = () => ({ x: coord(), y: coord() });
  function surface(forceDamage) {
    const s = [vec(), vec()];
    const k = forceDamage ? 0 : ri(5);
    if (k === 0) s.push({ damageType: DAMAGE[ri(4)] });
    else if (k === 1) s.push({ damageType: null });
    else if (k === 2) s.push({}); // props with no damageType key at all
    // k === 3 | 4: a bare two-element surface
    return s;
  }
  function surfaces() {
    // A third of the lists are seven-deep and fully damaged, so upstream
    // BUG 1 (the lost 6th damage digit) is exercised, not merely hoped for.
    if (r() < 0.34) {
      const out = [];
      for (let i = 0; i < 6 + ri(4); i++) out.push(surface(true));
      return out;
    }
    const n = ri(9);
    const out = [];
    for (let i = 0; i < n; i++) out.push(surface(false));
    return out;
  }
  function polygons() {
    const n = ri(4);
    const out = [];
    for (let i = 0; i < n; i++) {
      const m = ri(8); // 0 is legal — parsePolygon returns [] on odd input
      const pts = [];
      for (let j = 0; j < m; j++) pts.push(vec());
      out.push(pts);
    }
    return out;
  }
  return function stage() {
    const st = {
      startingPoint: [],
      ledge: [],
      polygon: polygons(),
      ground: surfaces(),
      ceiling: surfaces(),
      wallL: surfaces(),
      wallR: surfaces(),
      platform: surfaces(),
      background: { polygon: polygons(), line: surfaces() },
      target: [],
      blastzone: { min: vec(), max: vec() },
      // Zero scale is excluded here on purpose: it takes upstream's
      // `|| 3` branch and is therefore NOT idempotent. It lives in the
      // hostile/edge table instead, where both verdicts are pinned.
      scale: 2 + r() * 4,
    };
    // A coordinate in (-0.005, 0) emits the token "-0.00", which parses
    // back to -0 and RE-ENCODES as "0.00": the one place a code this codec
    // emitted is not its own fixed point. Injected into a FEW stages, at
    // stage level rather than per coordinate (at per-coordinate odds a
    // stage's hundreds of numbers make it a certainty and the well-formed
    // bucket empties out — measured). These stages land in the `edge`
    // bucket, where C and JS must still agree byte for byte.
    if (r() < 0.03) st.blastzone.min.x = -r() * 0.005;
    const nsp = 1 + ri(8);
    for (let i = 0; i < nsp; i++) st.startingPoint.push(vec());
    if (r() < 0.5) {
      st.startingFace = [];
      const nf = ri(5);
      for (let i = 0; i < nf; i++) st.startingFace.push(r() < 0.5 ? 1 : -1);
    } // else: the key is absent, which emits "1,1,1,1"
    const nt = ri(21);
    for (let i = 0; i < nt; i++) st.target.push(vec());
    const nl = ri(17);
    for (let i = 0; i < nl; i++) {
      const usePlatform = r() < 0.5 && st.platform.length > 0;
      const list = usePlatform ? "platform" : "ground";
      if (st[list].length === 0) continue; // an invalid index throws upstream
      st.ledge.push([list, ri(st[list].length), ri(2)]);
    }
    return st;
  };
}

// Codes whose parse upstream and the port answer DIFFERENTLY, or whose
// answer is interesting enough to freeze. Each is labelled; the check
// records both sides' verdicts and compares the table with the committed
// expected-stage-code.json.
function hostileCases() {
  const BZ = "-250.00,-250.00,250.00,250.00";
  const base = (f) => {
    const a = ["0.00,0.00", "1", "", "", "", "", "", "", "", "", "", "", BZ, "3.00"];
    for (const k of Object.keys(f)) a[k] = f[k];
    return a.join("&");
  };
  const many = (n, rec) => new Array(n).fill(rec).join("~");
  return [
    ["empty", ""],
    ["shorter-than-14", "0.00&1&&&&"],
    ["exactly-14-junk", "aaaaaaaaaaaaaa"],
    ["only-separators", "&&&&&&&&&&&&&"],
    ["no-starting-point", base({ 0: "" })],
    ["starting-point-one-number", base({ 0: "1.00" })],
    ["three-decimals", base({ 0: "1.234,0.00" })],
    ["exponent-form", base({ 0: "1e+21,0.00" })],
    ["nan-token", base({ 0: "NaN,NaN" })],
    ["no-fraction-digits", base({ 0: "1,2" })],
    ["trailing-dot", base({ 0: "1.,2.00" })],
    ["plus-sign", base({ 0: "+1.00,2.00" })],
    ["leading-space", base({ 0: " 1.00,2.00" })],
    ["negative-zero-coord", base({ 0: "-0.00,-0.00" })],
    ["tiny-negative-coord", base({ 0: "-0.001,0.00" })],
    ["scale-zero", base({ 13: "0.00" })],
    ["scale-absent", base({}).replace(/&3\.00$/, "&")],
    ["fields-truncated-to-12", base({}).split("&").slice(0, 12).join("&")],
    ["extra-fields", base({}) + "&9.99&8.88"],
    ["surface-four-tokens", base({ 2: "0.00,0.00,1.00,1.00" })],
    ["surface-six-tokens", base({ 2: "0.00,0.00,1.00,1.00,1,7" })],
    ["surface-damage-out-of-range", base({ 2: "0.00,0.00,1.00,1.00,9" })],
    ["surface-three-tokens", base({ 2: "0.00,0.00,1.00" })],
    ["polygon-odd-token-count", base({ 8: "1.00,2.00,3.00" })],
    ["polygon-empty-record", base({ 8: "~" })],
    ["ledge-index-out-of-range", base({ 2: "0.00,0.00,1.00,1.00,0", 10: "g,5,0" })],
    ["ledge-index-negative", base({ 2: "0.00,0.00,1.00,1.00,0", 10: "g,-1,0" })],
    ["ledge-index-nan", base({ 2: "0.00,0.00,1.00,1.00,0", 10: "g,x,0" })],
    ["ledge-no-ground", base({ 10: "g,0,0" })],
    ["ledge-side-out-of-range", base({ 2: "0.00,0.00,1.00,1.00,0", 10: "g,0,7" })],
    ["ledge-index-with-decimals", base({ 2: "0.00,0.00,1.00,1.00,0", 10: "g,0.00,0" })],
    ["ledge-two-tokens", base({ 2: "0.00,0.00,1.00,1.00,0", 10: "g,0" })],
    ["blastzone-three-numbers", base({ 12: "-1.00,-1.00,1.00" })],
    ["blastzone-absent", base({ 12: "" })],
    ["over-cap-surfaces", base({ 2: many(65, "0.00,0.00,1.00,1.00,0") })],
    ["over-cap-targets", base({ 11: many(21, "1.00,1.00") })],
    ["over-cap-polygons", base({ 8: many(17, "1.00,2.00") })],
    ["over-cap-polygon-points", base({ 8: many(33, "1.00,2.00").replace(/~/g, ",") })],
    ["over-cap-ledges", base({ 2: "0.00,0.00,1.00,1.00,0", 10: many(17, "g,0,0") })],
    ["over-cap-starting-points", base({ 0: many(9, "1.00,1.00") })],
    ["huge-coordinate", base({ 0: "99999999999999.00,0.00" })],
    ["coordinate-past-2p53-hundredths", base({ 0: "900719925474099.99,0.00" })],
  ];
}

function gen(tpdir, outdir) {
  const E = loadEncode(tpdir);
  const r = rng(0xa45c0de);
  const nextStage = makeGen(r);

  const well = [];
  const edge = [];
  let lossy = 0;
  let bug1 = 0; // surface lists long enough for encode.js:39 to bite
  for (let i = 0; i < 4000; i++) {
    const st = nextStage();
    const c1 = E.createStageCode(st);
    const p = E.parseStageCode(c1);
    if (p === null) continue; // not in the emitted language; hostile's job
    const c2 = E.createStageCode(p);
    (c1 === c2 ? well : edge).push(c1);
    // The FIRST emission is lossy: prove it bites rather than assuming it.
    for (const t of ["ground", "ceiling", "wallL", "wallR", "platform"]) {
      if (st[t].length > 5) bug1++;
      for (let k = 0; k < st[t].length; k++) {
        if (st[t][k][0].x !== p[t][k][0].x) { lossy++; break; }
      }
    }
  }
  const hostile = hostileCases();
  fs.writeFileSync(path.join(outdir, "codes-wellformed.txt"), well.join("\n") + "\n");
  fs.writeFileSync(path.join(outdir, "codes-edge.txt"), edge.join("\n") + "\n");
  fs.writeFileSync(
    path.join(outdir, "codes-hostile.txt"),
    hostile.map((h) => h[1]).join("\n") + "\n"
  );
  fs.writeFileSync(
    path.join(outdir, "hostile-labels.json"),
    JSON.stringify(hostile.map((h) => h[0]))
  );
  fs.writeFileSync(
    path.join(outdir, "meta.json"),
    JSON.stringify(
      {
        wellformed: well.length,
        edge: edge.length,
        hostile: hostile.length,
        lossyFirstEmission: lossy,
        sixthSurfaceLists: bug1,
      },
      null,
      2
    ) + "\n"
  );
  // Asserted AFTER the artifacts are on disk, so a degenerate corpus can
  // be inspected instead of merely reported.
  if (well.length === 0 || edge.length === 0 || lossy === 0 || bug1 === 0) {
    throw new Error(
      "corpus generator produced a degenerate corpus: well=" + well.length +
        " edge=" + edge.length + " lossy=" + lossy + " bug1=" + bug1
    );
  }
  process.stdout.write(
    "  corpus: " + well.length + " well-formed, " + edge.length + " edge, " +
      hostile.length + " hostile; " + lossy + " lossy first emissions, " +
      bug1 + " lists past the encode.js:39 boundary\n"
  );
}

function ref(tpdir, codesPath, outPath) {
  const E = loadEncode(tpdir);
  const lines = fs.readFileSync(codesPath, "utf8").split("\n");
  if (lines[lines.length - 1] === "") lines.pop();
  const out = lines.map((c) => {
    const st = E.parseStageCode(c);
    return st === null ? "NULL" : "OK " + E.createStageCode(st);
  });
  fs.writeFileSync(outPath, out.join("\n") + "\n");
}

function tofixed(hexPath, outPath) {
  const inp = fs.readFileSync(hexPath, "utf8");
  const out = [];
  let i = 0;
  for (;;) {
    const j = inp.indexOf("\n", i);
    if (j < 0) break;
    const b = Buffer.from(inp.slice(i, j), "hex");
    i = j + 1;
    out.push(b.readDoubleBE(0).toFixed(2));
  }
  fs.writeFileSync(outPath, out.join("\n") + "\n");
}


// --- judge -----------------------------------------------------------------
//
// Everything that is NOT a byte comparison (those are `cmp`'s job in
// check-stage-code.sh): the idempotence properties, the positive
// assertions that upstream BUG 1 actually occurred in the corpus, and the
// measured-then-frozen pin table — including the hostile verdicts, which
// are where deviation D39 is visible and therefore reviewable.
function sha256(buf) {
  return require("crypto").createHash("sha256").update(buf).digest("hex");
}

function readLines(p) {
  const l = fs.readFileSync(p, "utf8").split("\n");
  if (l[l.length - 1] === "") l.pop();
  return l;
}

function judge(dir, expectedPath) {
  const fail = [];
  const J = (n) => path.join(dir, n);
  const meta = JSON.parse(fs.readFileSync(J("meta.json"), "utf8"));
  const labels = JSON.parse(fs.readFileSync(J("hostile-labels.json"), "utf8"));

  const bucket = {};
  for (const k of ["wellformed", "edge", "hostile"]) {
    bucket[k] = {
      codes: readLines(J("codes-" + k + ".txt")),
      c: readLines(J(k + ".c.txt")),
      js: readLines(J(k + ".js.txt")),
    };
    const b = bucket[k];
    if (b.c.length !== b.codes.length || b.js.length !== b.codes.length) {
      fail.push(k + ": verdict line count does not match the corpus");
    }
  }

  // THE property this ticket exists to establish: from the second emission
  // on, a code is its own fixed point. Asserted on the C side (the JS side
  // is how the bucket was defined) for every well-formed code.
  let notFixed = 0;
  bucket.wellformed.codes.forEach((c, i) => {
    if (bucket.wellformed.c[i] !== "OK " + c) notFixed++;
  });
  if (notFixed !== 0) fail.push(notFixed + " well-formed codes are not C fixed points");

  // And the edge bucket must MEAN something: every code in it is a code
  // this codec emitted that is NOT its own fixed point (the "-0.00" token
  // parses to -0 and re-emits as "0.00"). If this ever hits zero the
  // bucket has gone vacuous and the check is quietly weaker.
  let edgeFixed = 0;
  bucket.edge.codes.forEach((c, i) => {
    if (bucket.edge.c[i] === "OK " + c) edgeFixed++;
  });
  if (edgeFixed !== 0) fail.push(edgeFixed + " edge codes are fixed points after all");

  // BUG 1, asserted POSITIVELY rather than inferred from the two sides
  // agreeing: in a surface field with at least six records, record index 5
  // must carry FOUR numbers and a trailing empty token — no damage digit.
  let sixth = 0;
  let sixthDamaged = 0;
  for (const line of bucket.wellformed.c) {
    if (!line.startsWith("OK ")) continue;
    const f = line.slice(3).split("&");
    for (let t = 2; t <= 7; t++) {
      if (f[t] === "") continue;
      const recs = f[t].split("~");
      if (recs.length < 6) continue;
      sixth++;
      const tok = recs[5].split(",");
      if (!(tok.length === 5 && tok[4] === "")) sixthDamaged++;
    }
  }
  if (sixth === 0) fail.push("no surface field reached the encode.js:39 boundary");
  if (sixthDamaged !== 0)
    fail.push(sixthDamaged + " sixth surfaces kept a damage digit (BUG 1 not carried)");

  const observed = {
    counts: {
      wellformed: meta.wellformed,
      edge: meta.edge,
      hostile: meta.hostile,
      lossyFirstEmission: meta.lossyFirstEmission,
      sixthSurfaceRecords: sixth,
    },
    sha256: {
      "codes-wellformed.txt": sha256(fs.readFileSync(J("codes-wellformed.txt"))),
      "codes-edge.txt": sha256(fs.readFileSync(J("codes-edge.txt"))),
      "codes-hostile.txt": sha256(fs.readFileSync(J("codes-hostile.txt"))),
    },
    // D39 made visible: for each hostile input, what upstream answered and
    // what the port answered. `agree` is false exactly where the port's
    // stricter grammar refuses a code parseFloat would have turned into a
    // NaN-riddled stage.
    hostile: labels.map((label, i) => {
      const c = bucket.hostile.c[i];
      const js = bucket.hostile.js[i];
      return {
        label: label,
        upstream: js === "NULL" ? "null" : "stage",
        port: c === "NULL" ? "reject" : "stage",
        agree: c === js,
      };
    }),
  };
  fs.writeFileSync(J("observed.json"), JSON.stringify(observed, null, 2) + "\n");

  // Both sides answering "stage" but with DIFFERENT bytes would be a real
  // divergence hiding inside an `agree:false` row; call it out separately.
  observed.hostile.forEach((h) => {
    if (h.upstream === "stage" && h.port === "stage" && !h.agree)
      fail.push("hostile '" + h.label + "': both parsed, but the codes differ");
  });

  if (process.env.MLFK_FREEZE_STAGECODE === "1") {
    fs.writeFileSync(expectedPath, JSON.stringify(observed, null, 2) + "\n");
    process.stdout.write("  FROZE " + expectedPath + "\n");
  } else {
    const want = fs.readFileSync(expectedPath, "utf8");
    const got = JSON.stringify(observed, null, 2) + "\n";
    if (want !== got) {
      fs.writeFileSync(J("observed.json"), got);
      fail.push("pins drifted: diff " + expectedPath + " " + J("observed.json"));
    }
  }

  if (fail.length > 0) {
    for (const f of fail) process.stderr.write("STAGECODE FAIL: " + f + "\n");
    process.exit(1);
  }
  process.stdout.write(
    "  judge OK: " + meta.wellformed + " fixed points, " + meta.edge +
      " edge, " + sixth + " sixth-surface records all missing their damage " +
      "digit (BUG 1 carried), " + observed.hostile.filter((h) => !h.agree).length +
      "/" + labels.length + " hostile rows on the D39 side\n"
  );
}

const a = process.argv.slice(2);
switch (a[0]) {
  case "transpile": transpile(a[1], a[2]); break;
  case "gen": gen(a[1], a[2]); break;
  case "ref": ref(a[1], a[2], a[3]); break;
  case "tofixed": tofixed(a[1], a[2]); break;
  case "judge": judge(a[1], a[2]); break;
  default:
    process.stderr.write("usage: stage-code-js-ref.js transpile|gen|ref|tofixed|judge ...\n");
    process.exit(2);
}
