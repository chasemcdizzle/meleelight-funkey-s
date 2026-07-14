// Oracle harness page-init (maintained copy; spike original frozen at
// spikes/determinism/harness/init.js).
// Injected into the page BEFORE any page script runs (Playwright addInitScript).
// Reads window.__harnessConfig (set by an earlier init script from run.js):
//   { seedRandom: bool, seed: uint32, virtualClock: bool, fdlibm: bool,
//     captureMath: bool }
//
// Provides:
//   - deterministic Math.random (mulberry32) when seedRandom, counting calls either way
//   - virtual clock: performance.now / Date.now advance only via __vAdvance
//   - vendored-fdlibm Math shim (sin/cos/tan/atan/atan2/pow) unless
//     fdlibm === false — pins the oracle against browser libm drift
//   - transcendental call counters on Math.*; with captureMath, a
//     bit-pattern log of every shimmed call (oracle/fdlibm-crosscheck/)
(() => {
  const cfg = window.__harnessConfig || { seedRandom: true, seed: 42 };

  // --- vendored fdlibm Math shim (PLAN §2, M0 task 3) --------------------
  // port/fdlibm/fdlibm.js is injected by run.js BEFORE this script and
  // exposes window.__fdlibm. Fail hard if it is missing: a run silently
  // using native browser libm would record the wrong stream.
  const FD_SURFACE = ["sin", "cos", "tan", "atan", "atan2", "pow"];
  if (cfg.fdlibm !== false) {
    if (!window.__fdlibm) {
      throw new Error("harness: fdlibm shim required but fdlibm.js was not injected");
    }
    for (const name of FD_SURFACE) Math[name] = window.__fdlibm[name];
    window.__fdlibmActive = true;
  }

  // --- seeded PRNG -----------------------------------------------------
  function mulberry32(a) {
    return function () {
      a |= 0;
      a = (a + 0x6d2b79f5) | 0;
      let t = Math.imul(a ^ (a >>> 15), 1 | a);
      t = (t + Math.imul(t ^ (t >>> 7), 61 | t)) ^ t;
      return ((t ^ (t >>> 14)) >>> 0) / 4294967296;
    };
  }
  window.__rngCalls = 0;
  window.__rngCallsOutsideStep = 0;   // draws NOT made inside a sim step
  window.__rngOutsideStacks = [];     // first few call stacks of those
  const nativeRandom = Math.random.bind(Math);
  window.__nativeRandom = nativeRandom; // for cosmetic-only consumers (percentShake patch)
  const rng = mulberry32(cfg.seed >>> 0);
  Math.random = function () {
    window.__rngCalls++;
    if (!window.__inStep) {
      window.__rngCallsOutsideStep++;
      if (window.__rngOutsideStacks.length < 10) {
        window.__rngOutsideStacks.push(new Error("rng-outside-step").stack);
      }
    }
    return cfg.seedRandom ? rng() : nativeRandom();
  };

  // --- virtual clock (skippable via cfg.virtualClock=false) --------------
  let vnow = 0;
  window.__vAdvance = (ms) => { vnow += ms; };
  window.__vNow = () => vnow;
  if (cfg.virtualClock !== false) {
    const epoch = 1500000000000; // fixed fake wall-clock origin
    performance.now = () => vnow;
    Date.now = () => epoch + vnow;
  }

  // --- transcendental / libm exposure counters + capture ----------------
  // With cfg.captureMath, every call to a shimmed surface function is
  // logged in the crosscheck line format "<fn> <hex16> [<hex16>] -> <hex16>"
  // (IEEE-754 bit patterns) for oracle/fdlibm-crosscheck/ replay.
  const capBuf = new ArrayBuffer(8);
  const capF64 = new Float64Array(capBuf);
  const capU32 = new Uint32Array(capBuf);
  capF64[0] = 1.0;
  const capHI = capU32[1] === 0x3ff00000 ? 1 : 0;
  const capLO = 1 - capHI;
  const hexBits = (x) => {
    capF64[0] = x;
    return ((capU32[capHI] >>> 0).toString(16).padStart(8, "0") +
            (capU32[capLO] >>> 0).toString(16).padStart(8, "0"));
  };
  window.__mathCapture = [];
  const captureMath = cfg.captureMath === true;

  const tracked = ["sin", "cos", "tan", "asin", "acos", "atan", "atan2",
    "pow", "exp", "log", "sqrt", "cbrt", "hypot", "sinh", "cosh", "tanh"];
  window.__mathCalls = {};
  for (const name of tracked) {
    window.__mathCalls[name] = 0;
    const orig = Math[name];
    if (typeof orig !== "function") continue;
    const isSurface = FD_SURFACE.indexOf(name) !== -1;
    Math[name] = function (a, b) {
      window.__mathCalls[name]++;
      const r = arguments.length === 1 ? orig(a) : orig(a, b);
      if (captureMath && isSurface) {
        window.__mathCapture.push(
          arguments.length === 1
            ? name + " " + hexBits(a) + " -> " + hexBits(r)
            : name + " " + hexBits(a) + " " + hexBits(b) + " -> " + hexBits(r));
      }
      return r;
    };
  }
  window.__resetMathCalls = () => {
    for (const k of Object.keys(window.__mathCalls)) window.__mathCalls[k] = 0;
    window.__mathCapture = [];
    window.__rngCalls = 0;
    window.__rngCallsOutsideStep = 0;
    window.__rngOutsideStacks = [];
  };

  // --- harness flags (must exist before game scripts evaluate) ----------
  window.__harnessMode = true;      // gameTick stores next buffers instead of setTimeout re-arm
  window.__harnessNoRender = true;  // renderTick keeps rAF alive but draws nothing
  window.__harnessInputs = null;    // per-frame synthetic inputs, set by the step loop
})();
