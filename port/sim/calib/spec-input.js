// spec-input.js — M2 task 3 capture spec: interpretInputs + the 8-deep
// input buffer + meleeInputs (the input cluster). Registers
// window.__capSpecs.input. Injected AFTER capturelib.js.
//
// Boundaries (11 wrapped functions across 3 modules):
// - input module (src/input/input.js): pollInputs (interpretInputs'
//   fresh-input source, main.js:678 — the ONLY live call site; ret is the
//   Input the harness injects for human slots), inputData, nullInput,
//   nullInputs (gameTick main.js:919 + interpretInputs main.js:670).
// - meleeInputs module (src/input/meleeInputs.js): all 6 exports —
//   deaden (live via inputData's cross-module calls), meleeRound,
//   tasRescale, scaleToGCTrigger, scaleToUnitAxes, scaleToMeleeAxes
//   (the GC-axis scaling/deadzone/quantization core; zero live records
//   over human-trace goldens — the harness path bypasses polling — so
//   the synthetic-domain sweep below gives them real executed records).
// - physics module: physics(i, inputBuffers), args projected to
//   [i, inputBuffers[i]] — inputBuffers[i] is EXACTLY interpretInputs'
//   return value for slot i this frame (gameTick main.js:1065-1067:
//   input[i] = interpretInputs(...); update(i, input) tail-calls
//   physics(i, input)), or the fresh nullInputs() from main.js:919 during
//   the ~91-frame 'starting' window when interpretInputs is skipped.
//   interpretInputs itself is main.js-internal to gameTick (built bundle:
//   direct `input[i] = interpretInputs(` calls, NOT namespace-
//   dereferenced) and therefore not wrappable — the physics args
//   projection is its observable output boundary, the same trick as the
//   player spec's post-state (task 2). Ret is void (undefRetAllowed).
//
// The C replay (replay_input.c) chains: per (frame, slot) it feeds the
// recorded pollInputs ret through the C interpretInputs + buffer state
// machine and compares the produced 8-deep buffer against the physics
// args projection bit-exactly — a full-trace recurrence check of the
// buffer semantics (z/s always-shift, pause-aware pastOffset, pause/
// frameAdvance bookkeeping).
(() => {
  // --- deterministic synthetic-domain sweep values -------------------------
  // Fixed literal arrays (no RNG): melee-unit axis values spanning every
  // threshold in the engine's input model (deadzone 0.28, tilt 0.3,
  // u/d-smash 0.66, crouch/spotdodge 0.69/0.7, dash 0.79 — docs/research/
  // b0xx-mapping.md §3.1), the exact 1/80-quantization neighbours, both
  // zeros, and out-of-unit values that exercise toInterval/unitRetract
  // saturation. Trigger values span scaleToGCTrigger's 0.3 floor / 1 cap /
  // |scale|<0.001 zero branch.
  const S = [
    -1, -0.999, -0.95, -0.8, -0.79, -0.7875, -0.7, -0.6875, -0.6625, -0.5,
    -0.35, -0.3, -0.2875, -0.28, -0.2799999999999999, -0.15, -0.00625,
    -1e-9, -0, 0, 1e-9, 0.00625, 0.15, 0.2799999999999999, 0.28, 0.2875,
    0.3, 0.35, 0.5, 0.6625, 0.6875, 0.7, 0.7875, 0.79, 0.8, 0.95, 0.999, 1,
  ];
  const CLAMP = [-2.5, -1.6, -1.59375, -1.1, 1.1, 1.59375, 1.6, 2.5];
  const GRID = [-1, -0.79, -0.35, -0.28, -0.00625, 0, 0.2875, 0.35, 0.7, 1];
  // plausible simulated-GC stick cardinals (StickCardinals shape read by
  // stickExtremePoints: center{x,y}/left/right/down/up) + a degenerate one
  // (all points == center -> singular matrices -> inverseMatrix null ->
  // renormaliseAxisInput's [x,y] fallback).
  const CARD1 = { center: { x: -0.0039, y: 0.0118 },
                  left: -0.7961, right: 0.7882, down: 0.7725, up: -0.8118 };
  const CARD2 = { center: { x: 0.05, y: -0.04 },
                  left: -1, right: 0.9, down: 0.85, up: -0.95 };
  const DEGEN = { center: { x: 0.1, y: -0.2 },
                  left: 0.1, right: 0.1, down: -0.2, up: -0.2 };

  window.__capSpecs.input = {
    expectWrapped: 11,
    install(ctx) {
      const moduleIds = {};
      const find = (pred, what) => {
        const m = ctx.findModule(ctx.cache, pred, what);
        moduleIds[what] = m.id;
        return m.exports;
      };

      const inputMod = find((ex) =>
        typeof ex.pollInputs === "function" &&
        typeof ex.nullInputs === "function" &&
        Array.isArray(ex.aiInputBank), "input");
      ctx.wrapExport(inputMod, "pollInputs");
      ctx.wrapExport(inputMod, "inputData");
      ctx.wrapExport(inputMod, "nullInput");
      ctx.wrapExport(inputMod, "nullInputs");

      const meleeMod = find((ex) =>
        typeof ex.scaleToMeleeAxes === "function" &&
        typeof ex.deaden === "function" &&
        typeof ex.meleeRound === "function", "meleeInputs");
      for (const n of ["deaden", "meleeRound", "tasRescale",
                       "scaleToGCTrigger", "scaleToUnitAxes",
                       "scaleToMeleeAxes"]) {
        ctx.wrapExport(meleeMod, n);
      }

      const physicsMod = find((ex) =>
        typeof ex.physics === "function" &&
        typeof ex.land === "function", "physics");
      // args projection: [i, inputBuffers[i]] — slot index + THE slot's
      // 8-deep buffer (= interpretInputs' return this frame). The other
      // slots' buffers and the player mutation surface belong to other
      // specs (player: task 2; physics core: task 5).
      ctx.wrapExport(physicsMod, "physics", "physics",
        (args) => [args[0], args[1][args[0]]]);

      this._mods = { input: inputMod, melee: meleeMod };
      return { moduleIds: moduleIds };
    },

    // Deterministic synthetic-domain sweep (run-capture.js calls this
    // AFTER install, BEFORE setupMatch -> records at frame 0). Calls go
    // through the wrapped exports, so they are recorded and replayed like
    // live records — real upstream executions, not golden-path traffic.
    sweep() {
      const melee = this._mods.melee;
      const input = this._mods.input;
      let n = 0;

      for (const x of S.concat(CLAMP)) { melee.deaden(x); n++; }
      for (const x of GRID) {           // explicit custom deadzone arg
        melee.deaden(x, 0.1); melee.deaden(x, 0.5); n += 2;
      }
      for (const x of S.concat(CLAMP)) { melee.meleeRound(x); n++; }
      for (const x of S) {
        for (const y of GRID) { melee.tasRescale(x, y); n++; }
      }
      for (const x of GRID) {           // explicit isDeadzoned arg (ignored
        melee.tasRescale(x, 0.35, true); n++; // upstream — carried verbatim)
      }
      for (const t of [-0.2, -0, 0, 0.1, 0.29, 0.3, 0.42, 0.5, 0.9, 0.99, 1, 1.2]) {
        for (const os of [[0, 1], [0.05, 0.9], [-0.1, 1.2], [0, 0.0005],
                          [0.1, -0.5], [-0.3, -0.0009]]) {
          melee.scaleToGCTrigger(t, os[0], os[1]); n++;
        }
      }
      for (const x of GRID) {
        for (const y of GRID) {
          for (const card of [null, CARD1, CARD2, DEGEN]) {
            melee.scaleToUnitAxes(x, y, card, 0, 0); n++;
          }
          melee.scaleToUnitAxes(x, y, CARD1, 0.03, -0.02); n++;
          for (const card of [null, CARD1]) {
            melee.scaleToMeleeAxes(x, y, true, card, 0.01, -0.03); n++;
            melee.scaleToMeleeAxes(x, y, false, card, 0.01, -0.03); n++;
            melee.scaleToMeleeAxes(x, y, false, card); n++; // default centers
          }
          melee.scaleToMeleeAxes(x, y, false, DEGEN, 0, 0); n++;
        }
      }

      // input record constructors through the exports (the live sim calls
      // inputData only via input.js-internal bindings — these give the
      // 18-list -> 22-field mapping real cross-boundary records).
      input.nullInput(); n++;
      input.nullInputs(); n++;
      input.inputData(); n++; // default list
      const lists = [
        [true, false, true, false, true, false, true, false, true, false,
         true, false, 0.35, -0.79, 0.2799999999999999, -0.28, 0.42, 1],
        [false, true, false, true, false, true, false, true, false, true,
         false, true, -1, 1, -0, 0.6625, 0, 0.3],
        [true, true, true, true, true, true, true, true, true, true,
         true, true, 1.1, -1.1, 0.00625, -0.00625, 0.9, -0.2],
      ];
      for (const l of lists) { input.inputData(l); n++; }
      return n;
    },
  };
})();
