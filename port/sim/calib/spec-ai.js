// spec-ai.js — M2 task 16 capture spec: the AI-input bridge boundary over
// the CPU goldens g07/g08. Registers window.__capSpecs.ai. Injected AFTER
// capturelib.js. See FORMAT.md "The ai spec".
//
// The bridge = AI-as-recorded-input (fix_plan §M2 "AI policy"): ai.js
// stays JS-side until M4; this spec records everything the C sim needs to
// replay a CPU slot without executing AI logic, and HARD-VERIFIES that
// the AI writes nothing else (the write-set reconciliation).
//
// Boundaries (3 wrapped functions):
// - runAI (src/main/ai.js, called from update(i) at main.js:902 when
//   !starting && actionState != "SLEEP"): mutation record "runAI" with
//   args [i] and post envelope {bank, bk, rng} — bank = the post-runAI
//   aiInputBank[i][0] row (the value plane the C bridge installs), bk =
//   the AI-private player bookkeeping {ca: currentAction, cs:
//   currentSubaction, cta: curentAction (upstream typo field, ai.js:1254),
//   lm: lastMash}, rng = the seeded draws consumed INSIDE this call, in
//   order (the draw schedule the C sim must burn at this call site).
// - pollInputs (src/input/input.js): the chain's per-(frame,slot) fresh
//   input. For the CPU slot the ret IS aiInputBank[i][0] read at
//   interpretInputs time — i.e. the PREVIOUS runAI's output (runAI runs
//   after interpretInputs inside the same update(i)): every poll record
//   re-verifies the C bridge's chained bank row (rule 18).
// - physics (args projected to [i, inputBuffers[i]], the spec-input
//   trick): interpretInputs' observable output. For the CPU slot the
//   buffer's slot 0 is the LIVE bank row (pollInputs returns the alias),
//   so the projection captures the post-runAI values physics actually
//   consumed — the bridged chain must reproduce it bit-exactly.
//
// WRITE-SET RECONCILIATION (hard-check, in-page, EVERY runAI call): the
// reachable write surface of ai.js is its import graph — player, cS
// (characterSelections), playerType from main; aiInputBank from input;
// gameSettings from settings; aS (activeStage) — plus Math.random.
// Grep-measured live assignment targets: aiInputBank[i][0].{a,b,csX,csY,
// l,lA,lsX,lsY,x,y,z}, player[i].{currentAction,currentSubaction,lastMash}
// + cpu.curentAction (typo), window.isOffstage (module-load fn def);
// everything else (player[i].inputs.*, phys.*, cpu.timer, aiInputBank
// [i][0].r) is inside block comments. The recon enforces this at runtime:
// canon snapshots BEFORE and AFTER each runAI call of (a) all four
// player objects with the four bookkeeping keys excluded, (b) all 32
// aiInputBank rows, (c) playerType, characterSelections, gameSettings,
// activeStage — any post != pre outside aiInputBank[i][0] pushes a
// "wsViol" record (pinned ZERO in expected-capture-ai.json) and fails
// finalCheck loudly. Synchronous single-threaded JS: between the two
// snapshots ONLY runAI runs, so a diff is exactly an AI write.
//
// RNG: rngBoot + attributed runAI draws + everything else standalone
// "Math.random" records pushed at draw time (chain-order faithful; no
// dispatch windows — runAI calls only its own module's helpers, rule 14
// vacuous: enforced by the no-nesting assert on the runAI window).
// No sweep (rule 11 n/a): the boundary is data-driven and fully live —
// every reachable behavior IS the recording.
(() => {
  // AI-private bookkeeping keys on player[i] (the allowlist; includes the
  // upstream `curentAction` typo field, ai.js:1254)
  const BK = ["currentAction", "currentSubaction", "curentAction", "lastMash"];

  window.__capSpecs.ai = {
    expectWrapped: 3,

    install(ctx) {
      const cache = ctx.cache;
      const moduleIds = {};
      const find = (pred, what) => {
        const m = ctx.findModule(cache, pred, what);
        moduleIds[what] = m.id;
        return m.exports;
      };

      const AI = find((ex) =>
        typeof ex.runAI === "function" &&
        typeof ex.generalAI === "function" &&
        typeof ex.NearestEnemy === "function", "ai");
      const INP = find((ex) =>
        typeof ex.pollInputs === "function" &&
        typeof ex.nullInputs === "function" &&
        Array.isArray(ex.aiInputBank), "input");
      const M = find((ex) =>
        Array.isArray(ex.player) && Array.isArray(ex.playerType) &&
        Array.isArray(ex.characterSelections) &&
        typeof ex.gameTick === "function", "main");
      const S = find((ex) =>
        ex.gameSettings && typeof ex.gameSettings === "object" &&
        Object.prototype.hasOwnProperty.call(ex.gameSettings, "tapJumpOffp1"),
        "settings");
      const AST = find((ex) =>
        typeof ex.getActiveStage === "function" &&
        typeof ex.setVsStage === "function", "activeStage");
      const PHYS = find((ex) =>
        typeof ex.physics === "function" &&
        typeof ex.land === "function", "physics");

      const bank = INP.aiInputBank;
      if (bank.length !== 4 || !bank.every((r) => Array.isArray(r) && r.length === 8)) {
        throw new Error("ai spec: aiInputBank is not 4x8");
      }

      // --- seeded-RNG stream (task-7 mechanism; no owner stack — runAI
      // is the only attributing window and never nests) ------------------
      const seed = window.__harnessConfig.seed >>> 0;
      const bootDraws = window.__rngCalls;
      const ffState =
        (seed + (Math.imul(bootDraws, 0x6D2B79F5) >>> 0)) % 4294967296;
      ctx.declare("rngBoot");
      ctx.declare("Math.random");
      ctx.declare("runAI");
      ctx.declare("wsViol");
      ctx.push("rngBoot",
        "[" + ctx.canon(seed) + "," + ctx.canon(bootDraws) + "]",
        ctx.canon(ffState));
      let aiWindow = null; // draw list while inside runAI, else null
      const mr = Math.random;
      Math.random = function () {
        const v = mr();
        if (aiWindow) aiWindow.push(v);
        else ctx.push("Math.random", "[]", ctx.canon(v));
        return v;
      };

      // --- write-set recon snapshots ------------------------------------
      // player[k] canon with the AI bookkeeping keys excluded (the diff of
      // THOSE is expected; everything else must be byte-identical)
      const playerMinusBK = (k) => {
        const src = M.player[k];
        const o = {};
        for (const key of Object.keys(src)) {
          if (BK.indexOf(key) === -1) o[key] = src[key];
        }
        return ctx.canon(o);
      };
      const reconSnap = () => {
        const rows = [];
        for (let k = 0; k < 4; k++) {
          for (let j = 0; j < 8; j++) rows.push(ctx.canon(bank[k][j]));
        }
        return {
          players: [playerMinusBK(0), playerMinusBK(1),
                    playerMinusBK(2), playerMinusBK(3)],
          bank: rows,
          pt: ctx.canon(M.playerType),
          cs: ctx.canon(M.characterSelections),
          gs: ctx.canon(S.gameSettings),
          as: ctx.canon(AST.getActiveStage()),
        };
      };
      let wsViolations = 0;
      const viol = (what) => {
        wsViolations++;
        ctx.push("wsViol", ctx.canon([what]), "null");
      };

      // --- runAI wrapper (hand-wrapped: needs the pre/post recon and the
      // draw window around the original call) ----------------------------
      const origRunAI = AI.runAI;
      AI.runAI = function (i) {
        if (aiWindow !== null) throw new Error("ai spec: runAI nested");
        const argsCanon = ctx.canon([i]);
        const pre = reconSnap();
        aiWindow = [];
        const ret = origRunAI.apply(this, arguments);
        const draws = aiWindow;
        aiWindow = null;
        const post = reconSnap();

        // reconciliation: everything outside aiInputBank[i][0] identical
        for (let k = 0; k < 4; k++) {
          if (post.players[k] !== pre.players[k]) {
            viol("player[" + k + "] (minus bookkeeping) changed");
          }
        }
        for (let k = 0; k < 4; k++) {
          for (let j = 0; j < 8; j++) {
            if (k === i && j === 0) continue;
            if (post.bank[k * 8 + j] !== pre.bank[k * 8 + j]) {
              viol("aiInputBank[" + k + "][" + j + "] changed");
            }
          }
        }
        if (post.pt !== pre.pt) viol("playerType changed");
        if (post.cs !== pre.cs) viol("characterSelections changed");
        if (post.gs !== pre.gs) viol("gameSettings changed");
        if (post.as !== pre.as) viol("activeStage changed");

        const p = M.player[i];
        const postCanon =
          '{"bank":' + ctx.canon(bank[i][0]) +
          ',"bk":' + ctx.canon({ ca: p.currentAction, cs: p.currentSubaction,
                                 cta: p.curentAction, lm: p.lastMash }) +
          ',"rng":[' + draws.map((v) => ctx.canon(v)).join(",") + "]}";
        ctx.push("runAI", argsCanon, ctx.canon(ret), postCanon);
        return ret;
      };
      ctx.wrapped++;

      // --- the input chain (spec-input's boundary pair) ------------------
      ctx.wrapExport(INP, "pollInputs");
      ctx.wrapExport(PHYS, "physics", "physics",
        (args) => [args[0], args[1][args[0]]]);

      this._spec_state = { getViol: () => wsViolations };
      return { moduleIds: moduleIds };
    },

    // Hard-fail the capture run on any write-set violation (belt to the
    // pins' braces: wsViol is also pinned ZERO).
    finalCheck() {
      const n = this._spec_state.getViol();
      if (n > 0) {
        throw new Error("ai spec: " + n + " write-set violations — the AI " +
                        "wrote outside aiInputBank + bookkeeping; the " +
                        "bridge premise is broken");
      }
      return 0;
    },
  };
})();
