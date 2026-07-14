// oracle/qjs/replay-main.js — replay driver for the QuickJS oracle runtime
// (M0 task 6). Runs INSIDE build-host/qjs-oracle.
//
// Mirrors oracle/harness/run.js's boot-and-drive sequence, substituting
// the QuickJS environment shim for headless Chrome, and REUSING the
// browser harness's page-side files verbatim (init.js, pagelib.js) so the
// serialization/hash/step contract (oracle/CHECKSUM.md) has exactly one
// implementation per side, not a re-implementation here.
//
// Usage (via oracle/qjs/replay.sh):
//   qjs-oracle replay-main.js --repo <root> --dist <clone> --trace <file>
//     --frames N --seed S --p1 C --p2 C --stage T [--cpu --difficulty N]
//     --out <run.json>
//
// Output: a run.json with the same {meta, coverage, frames} shape run.js
// emits, verifiable by the UNCHANGED oracle/harness/verify-stream.js.
"use strict";

function arg(name, dflt) {
  var i = scriptArgs.indexOf("--" + name);
  if (i === -1) return dflt;
  var v = scriptArgs[i + 1];
  return v === undefined || (typeof v === "string" && v.indexOf("--") === 0)
    ? true : v;
}
function has(name) { return scriptArgs.indexOf("--" + name) !== -1; }
function intArg(name, dflt) {
  var v = parseInt(arg(name, String(dflt)), 10);
  if (!Number.isInteger(v)) throw new Error("bad --" + name);
  return v;
}

var REPO = arg("repo", "");
var DIST = arg("dist", "");
var TRACE = arg("trace", "");
var OUT = arg("out", "out/qjs-run.json");
var FRAMES = intArg("frames", 3600);
var SEED = intArg("seed", 42);
var P1 = intArg("p1", 2);
var P2 = intArg("p2", 0);
var STAGE = intArg("stage", 0);
var CPU = has("cpu");
var DIFFICULTY = intArg("difficulty", 5);

// Boot-time seeded-RNG draw pin. Measured in the browser harness
// 2026-07-14 (headless Chrome, fdlibm shim, seed-independent): exactly
// 465 Math.random draws happen between page-init and harness-ready, ALL
// at module-evaluation time (1x jQuery expando + 464x stagerender bgStar
// constructors) and stable thereafter. The mulberry32 state is NOT
// re-seeded at setupMatch (only the counters reset), so the QJS boot must
// consume exactly the same number of draws or the in-match stream starts
// from a shifted PRNG state — a divergence that could stay latent on a
// golden whose draws never feed hashed state. Pinned and asserted.
var BOOT_RNG_DRAWS = 465;

if (!REPO || !DIST || !TRACE) {
  throw new Error("replay-main.js: --repo, --dist and --trace are required");
}

(function boot() {
  // 1. harness config BEFORE init.js (same contract as run.js's first
  //    addInitScript). fdlibm:false — under this runtime the Math table
  //    itself IS the vendored fdlibm (repointed in C by qjs_oracle.c
  //    before any JS ran), so init.js's browser-side JS shim step must
  //    not re-wrap it; init.js still installs the seeded PRNG, virtual
  //    clock and call counters.
  globalThis.__harnessConfig = {
    seedRandom: true,
    seed: SEED >>> 0,
    virtualClock: true,
    fdlibm: false,
    captureMath: false,
  };

  // 2. browser-environment shim (documents every global it fakes)
  __evalFile(REPO + "/oracle/qjs/shim.js");

  // 3. prove the C repoint is live: the JS fdlibm port (crosschecked
  //    bit-exact against the C in oracle/fdlibm-crosscheck/) must agree
  //    bitwise with Math.* on a deterministic sweep + edge cases. A build
  //    that silently fell back to host libm dies here, not at frame 1671.
  __evalFile(REPO + "/port/fdlibm/fdlibm.js");
  (function assertFdlibmActive() {
    var fd = globalThis.__fdlibm;
    if (!fd) throw new Error("fdlibm.js did not install __fdlibm");
    var buf = new ArrayBuffer(8);
    var f64 = new Float64Array(buf);
    var u32 = new Uint32Array(buf);
    function bits(x) {
      f64[0] = x;
      return u32[0].toString(16) + ":" + u32[1].toString(16);
    }
    // NaN payloads are invisible to the checksum contract (String(x) ==
    // "NaN" regardless, CHECKSUM.md §3.4) and engines may canonicalize
    // them; NaN-vs-NaN is equal, anything else is compared bitwise.
    function sameBits(a, b) {
      if (a !== a || b !== b) return a !== a && b !== b;
      return bits(a) === bits(b);
    }
    // deterministic xorshift sweep over varied magnitudes + edge cases
    var s = 0x9e3779b9 >>> 0;
    function next() {
      s ^= s << 13; s >>>= 0;
      s ^= s >> 17;
      s ^= s << 5; s >>>= 0;
      return s / 4294967296;
    }
    var inputs = [0, -0, 1, -1, 0.5, Math.PI, 1e-308, 1e308, 2.2250738585072014e-308,
                  6755399441055744, 1e22, NaN, Infinity, -Infinity];
    for (var i = 0; i < 4096; i++) {
      var mag = Math.exp((next() - 0.5) * 60); // ~1e-13 .. 1e13
      inputs.push((next() - 0.5) * 2 * mag);
    }
    var fns1 = ["sin", "cos", "tan", "atan"];
    for (var f = 0; f < fns1.length; f++) {
      var name = fns1[f];
      for (var j = 0; j < inputs.length; j++) {
        var a = Math[name](inputs[j]);
        var b = fd[name](inputs[j]);
        if (!sameBits(a, b)) {
          throw new Error("fdlibm repoint NOT active: Math." + name + "(" +
            inputs[j] + ") = " + a + " (bits " + bits(a) + ") but fdlibm says " +
            b + " (bits " + bits(b) + ")");
        }
      }
    }
    for (var j2 = 0; j2 < inputs.length; j2++) {
      for (var k = 0; k < inputs.length; k += 7) {
        var x = inputs[j2], y = inputs[k];
        if (!sameBits(Math.atan2(x, y), fd.atan2(x, y))) {
          throw new Error("fdlibm repoint NOT active: Math.atan2(" + x + "," + y + ")");
        }
        if (!sameBits(Math.pow(x, y), fd.pow(x, y))) {
          throw new Error("fdlibm repoint NOT active: Math.pow(" + x + "," + y + ")");
        }
      }
    }
    print("qjs replay: fdlibm repoint verified (" + inputs.length +
      " inputs x 6 fns, bitwise)");
  })();

  // 4. harness page files, VERBATIM copies of what the browser runs
  __evalFile(REPO + "/oracle/harness/init.js");
  __evalFile(REPO + "/oracle/harness/pagelib.js");

  // 5. the built bundles (same order the dist page's loader uses)
  var t0 = hrtime();
  __evalFile(DIST + "/dist/js/main.js");
  __evalFile(DIST + "/dist/js/animations.js");
  print("qjs replay: bundles evaluated in " + ((hrtime() - t0) / 1e6).toFixed(1) + "s");

  // 6. boot the game exactly like the dist page: start()
  if (typeof globalThis.start !== "function") {
    throw new Error("bundle did not expose window.start");
  }
  globalThis.start();
  if (!globalThis.__harness || !globalThis.__nextInputBuffers) {
    throw new Error("harness seam missing after start() " +
      "(__harness/__nextInputBuffers)");
  }
  if (globalThis.__rngCalls !== BOOT_RNG_DRAWS) {
    throw new Error("boot consumed " + globalThis.__rngCalls +
      " seeded RNG draws, browser boot consumes " + BOOT_RNG_DRAWS +
      " — PRNG stream misaligned, do NOT proceed (see pin comment)");
  }
  print("qjs replay: booted; boot RNG draws = " + globalThis.__rngCalls +
    " (== browser pin)");
})();

(async function main() {
  var trace = JSON.parse(__readFile(TRACE));

  // match config — identical sequence to run.js:148-160
  globalThis.__trace = trace;
  globalThis.__resetMathCalls(); // count sim exposure, not boot noise
  globalThis.__harness.setupMatch({
    players: [
      { type: 0, character: P1 },
      CPU ? { type: 1, character: P2, difficulty: DIFFICULTY }
          : { type: 0, character: P2 },
      null, null,
    ],
    stage: STAGE,
  });

  // divergence diagnosis, same contract as run.js --capture-frames:
  // store the full serialized state string for the listed frames
  var captureFrames = null;
  if (arg("capture-frames", null)) {
    captureFrames = {};
    String(arg("capture-frames")).split(",").forEach(function (s) {
      captureFrames[+s] = true;
    });
  }

  var frames = [];
  var CHUNK = 120; // transport-only in run.js; kept for progress printing
  var t0 = hrtime();
  for (var done = 0; done < FRAMES; done += CHUNK) {
    var n = Math.min(CHUNK, FRAMES - done);
    var chunk = await globalThis.__runFrames(n, { captureFrames: captureFrames });
    for (var i = 0; i < chunk.length; i++) frames.push(chunk[i]);
  }
  var wallMs = (hrtime() - t0) / 1e3;

  var coverage = globalThis.__coverage();

  __writeFile(OUT, JSON.stringify({
    meta: {
      dist: DIST,
      trace: TRACE,
      frames: FRAMES,
      seed: SEED,
      p1: P1,
      p2: P2,
      stage: STAGE,
      seedRandom: true,
      // fdlibm is ACTIVE here in its strongest form: the runtime's Math
      // table is the C fdlibm (asserted bitwise at boot above)
      fdlibm: true,
      cpu: CPU,
      difficulty: CPU ? DIFFICULTY : null,
      wallMs: Math.round(wallMs),
      runtime: "quickjs",
      bootRngDraws: BOOT_RNG_DRAWS,
    },
    coverage: coverage,
    captured: globalThis.__captured,
    frames: frames,
  }));
  print(OUT + ": " + FRAMES + " frames in " + Math.round(wallMs) + "ms (" +
    (FRAMES / (wallMs / 1000)).toFixed(0) + " fps), rngCalls=" +
    coverage.rngCalls + ", rngCallsOutsideStep=" + coverage.rngCallsOutsideStep);
  globalThis.__replayExit = 0;
})().catch(function (e) {
  print("QJS REPLAY ERROR: " + (e && e.stack ? e + "\n" + e.stack : e));
  globalThis.__replayExit = 1;
});
