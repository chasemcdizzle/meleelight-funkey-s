// Injected into the page BEFORE any page script runs (Playwright addInitScript).
// Reads window.__harnessConfig (set by an earlier init script from run.js):
//   { seedRandom: bool, seed: uint32 }
//
// Provides:
//   - deterministic Math.random (mulberry32) when seedRandom, counting calls either way
//   - virtual clock: performance.now / Date.now advance only via __vAdvance
//   - transcendental call counters on Math.*
(() => {
  const cfg = window.__harnessConfig || { seedRandom: true, seed: 42 };

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

  // --- transcendental / libm exposure counters --------------------------
  const tracked = ["sin", "cos", "tan", "asin", "acos", "atan", "atan2",
    "pow", "exp", "log", "sqrt", "cbrt", "hypot", "sinh", "cosh", "tanh"];
  window.__mathCalls = {};
  for (const name of tracked) {
    window.__mathCalls[name] = 0;
    const orig = Math[name];
    if (typeof orig !== "function") continue;
    Math[name] = function (a, b) {
      window.__mathCalls[name]++;
      return arguments.length === 1 ? orig(a) : orig(a, b);
    };
  }
  window.__resetMathCalls = () => {
    for (const k of Object.keys(window.__mathCalls)) window.__mathCalls[k] = 0;
    window.__rngCalls = 0;
    window.__rngCallsOutsideStep = 0;
    window.__rngOutsideStacks = [];
  };

  // --- harness flags (must exist before game scripts evaluate) ----------
  window.__harnessMode = true;      // gameTick stores next buffers instead of setTimeout re-arm
  window.__harnessNoRender = true;  // renderTick keeps rAF alive but draws nothing
  window.__harnessInputs = null;    // per-frame synthetic inputs, set by the step loop
})();
