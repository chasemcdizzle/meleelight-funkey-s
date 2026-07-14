// spec-asshort.js — M2 task 4 capture spec: actionStateShortcuts +
// state-machine scaffolding + the seeded-PRNG (mulberry32) draw stream +
// the sound-event/dispatch-event queues. Registers
// window.__capSpecs.asshort. Injected AFTER capturelib.js.
//
// Boundary: all 29 exported functions of src/physics/actionStateShortcuts.js
// (every call site outside the module dereferences the babel-CJS namespace
// — verified `_actionStateShortcuts.<fn>` in the built bundle; internal
// calls use local bindings and are correctly not captured). The exported
// `actionStates` array is the dispatch-table scaffolding surface: its
// entries' function properties (plus the per-char moves-index tables and
// the shared JUMPAERIALB/F module objects that checkForIASA dispatches
// into) get NON-RECORDING dispatch loggers so a boundary call's DIRECT
// dispatches become part of its post-state.
//
// EVENT ATTRIBUTION (owner stack; FORMAT.md "asshort"):
// - entering a wrapped boundary fn pushes an ATTRIBUTING frame;
// - entering any dispatched move fn pushes a NON-attributing frame (its
//   internals belong to the moves clusters, tasks 7-12);
// - a sound play / RNG draw / dispatch goes to the innermost frame if it
//   is attributing, else (dispatch-frame or empty stack) sounds+dispatches
//   are ignored (other clusters' surface) and RNG draws are emitted as
//   standalone `Math.random` records — keeping the seeded stream's FULL
//   draw order in the JSONL (chain continuity for the C mulberry32).
//
// RNG channel (oracle/CHECKSUM.md section 6): a frame-0 `rngBoot` record
// pins {seed, bootDraws (draws consumed before install, boot-time), the
// fast-forwarded mulberry32 state}; every subsequent seeded draw appears
// either as a standalone `Math.random` record or inside the drawing
// boundary record's post `rng` list — so the replay's single chained C
// mulberry32 must reproduce the stream draw-for-draw, INCLUDING the one
// off-step pre-frame-1 startGame draw (the frame-0 Math.random record).
// percentShake draws window.__nativeRandom directly (harness patch) and
// never appears.
//
// Args are per-function READ-SET projections (the stage-argument
// discipline, FORMAT.md): the exact player/global fields the function
// dereferences (verified against the module source), with `input` always
// projected to input[p].slice(0,4) — the module reads at most history
// depth 3. charAttributes reads are NOT projected: the C side reads the
// M1 CTAB1 tables (ml_tables) by the projected char id — this cluster is
// the first consumer of the generated-table data path. gameSettings
// tapJump reads are projected as values (god-module slice, task 17).
(() => {
  const POST = "POST"; // marker: fn has a 5th post-state field

  window.__capSpecs.asshort = {
    expectWrapped: 29,

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
      const S = find((ex) =>
        ex.gameSettings && typeof ex.gameSettings === "object" &&
        Object.prototype.hasOwnProperty.call(ex.gameSettings, "tapJumpOffp1"),
        "settings");
      const SFX = find((ex) =>
        ex.sounds && ex.sounds.fastfall && ex.sounds.shieldbreak &&
        typeof ex.sounds.fastfall.play === "function", "sfx");

      // --- owner stack (event attribution) -----------------------------
      const stack = [];
      const top = () => (stack.length ? stack[stack.length - 1] : null);

      // --- seeded-RNG stream instrumentation ---------------------------
      // Boot draws happened before install (menu/boot path); the match
      // stream starts here. State after n draws = (seed + n*0x6D2B79F5)
      // mod 2^32 (init.js:32-40 advances state additively per draw).
      const seed = window.__harnessConfig.seed >>> 0;
      const bootDraws = window.__rngCalls;
      const ffState =
        (seed + (Math.imul(bootDraws, 0x6D2B79F5) >>> 0)) % 4294967296;
      ctx.declare("rngBoot");
      ctx.declare("Math.random");
      ctx.push("rngBoot",
        "[" + ctx.canon(seed) + "," + ctx.canon(bootDraws) + "]",
        ctx.canon(ffState));
      const mr = Math.random;
      Math.random = function () {
        const v = mr();
        const t = top();
        if (t && t.attr) t.rng.push(v);
        else ctx.push("Math.random", "[]", ctx.canon(v));
        return v;
      };

      // --- sound-event instrumentation (the M4 mixer seam's oracle) ----
      let sndWrapped = 0;
      for (const name of Object.keys(SFX.sounds)) {
        const howl = SFX.sounds[name];
        if (!howl || typeof howl.play !== "function") continue;
        const orig = howl.play;
        howl.play = function () {
          const t = top();
          if (t && t.attr) t.snd.push(name);
          return orig.apply(this, arguments);
        };
        sndWrapped++;
      }
      if (sndWrapped !== 180) {
        throw new Error("asshort: expected 180 Howl sounds, wrapped " + sndWrapped);
      }

      // --- dispatch loggers (state-machine scaffolding surface) --------
      // Wrap every function property of every move object reachable from:
      // (a) the actionStates deep copies (setupActionStates output),
      // (b) the 5 per-char moves-index tables (checkForIASA's
      //     MARTH/PUFF/FOXMOVES), (c) the shared JUMPAERIALB/F module
      // objects checkForIASA calls directly. Loggers record
      // "<phase>:<moveName>" into the innermost attributing frame and push
      // a non-attributing frame for the move's own execution.
      const seen = new WeakSet();
      const wrapMove = (entry) => {
        if (!entry || typeof entry !== "object" || seen.has(entry)) return;
        seen.add(entry);
        if (typeof entry.name !== "string") {
          throw new Error("asshort: move object without a name");
        }
        for (const k of Object.keys(entry)) {
          if (typeof entry[k] !== "function") continue;
          const orig = entry[k];
          const label = k + ":" + entry.name;
          entry[k] = function () {
            const t = top();
            if (t && t.attr) t.dsp.push(label);
            stack.push({ attr: false });
            try {
              return orig.apply(this, arguments);
            } finally {
              stack.pop();
            }
          };
        }
      };
      for (const tbl of AS.actionStates) {
        if (!tbl) continue;
        for (const n of Object.keys(tbl)) wrapMove(tbl[n]);
      }
      let idxTables = 0;
      for (const id of Object.keys(cache)) {
        const ex = cache[id] && cache[id].exports;
        if (ex && ex.default && typeof ex.default === "object" &&
            ex.default.ATTACKAIRF &&
            typeof ex.default.ATTACKAIRF === "object" &&
            typeof ex.default.ATTACKAIRF.init === "function") {
          idxTables++;
          for (const n of Object.keys(ex.default)) wrapMove(ex.default[n]);
        }
      }
      if (idxTables !== 5) {
        throw new Error("asshort: expected 5 moves-index tables, found " + idxTables);
      }
      // shared + puff-own JUMPAERIALB/F module objects (checkForIASA
      // imports the shared ones; labels are name-based so wrapping every
      // module of that name is sound — WeakSet dedupes table overlaps)
      for (const nm of ["JUMPAERIALB", "JUMPAERIALF"]) {
        let found = 0;
        for (const id of Object.keys(cache)) {
          const ex = cache[id] && cache[id].exports;
          if (ex && ex.default && typeof ex.default === "object" &&
              ex.default.name === nm &&
              typeof ex.default.init === "function") {
            wrapMove(ex.default);
            found++;
          }
        }
        if (found < 1 || found > 2) {
          throw new Error("asshort: expected 1-2 modules for " + nm + ", found " + found);
        }
      }

      // --- projection helpers -------------------------------------------
      const players = () => M.player;
      const chr = (p) => M.characterSelections[p];
      const phys = (p) => players()[p].phys;
      const buf4 = (input, p) => input[p].slice(0, 4);
      const tapJump = (p) => S.gameSettings["tapJumpOffp" + (p + 1)];
      const hbSlice = (p) => {
        const hb = players()[p].hitboxes;
        return { active: hb.active, hitList: hb.hitList };
      };
      // presence-conditional copy (prevention rule 3: key presence per
      // construction site — IASATimer is a runtime-added player field)
      const iasaSlice = (p) => {
        const pl = players()[p];
        const out = {
          doubleJumped: pl.phys.doubleJumped,
          face: pl.phys.face,
          jumpsUsed: pl.phys.jumpsUsed,
          timer: pl.timer,
        };
        if ("IASATimer" in pl) out.IASATimer = pl.IASATimer;
        return out;
      };

      // name -> [argsProj, postMutProj|null]; postMutProj !== null marks a
      // mutation-captured fn (5-field records).
      const BOUNDARY = {
        randomShout: [
          (a) => [a[0]],
          () => ({}),
        ],
        executeIntangibility: [
          (a) => [a[0], chr(a[1]), players()[a[1]].timer, {
            hurtBoxState: phys(a[1]).hurtBoxState,
            intangibleTimer: phys(a[1]).intangibleTimer,
          }],
          (a) => ({
            hurtBoxState: phys(a[1]).hurtBoxState,
            intangibleTimer: phys(a[1]).intangibleTimer,
          }),
        ],
        playSounds: [
          // schedule = the executed actionSounds[char][state] rows (SND1
          // data plane; C table emission is the M4 mixer task, FORMATS.md
          // section 5.4 — the LOGIC is verified here, the data is plumbed)
          (a) => {
            const c = chr(a[1]);
            return [a[0], c, players()[a[1]].timer,
                    CHARS.actionSounds[c][a[0]]];
          },
          () => ({}),
        ],
        isFinalDeath: [
          () => [{
            gameMode: M.gameMode,
            playerType: M.playerType,
            stocks: M.playerType.map((t, j) =>
              t > -1 ? players()[j].stocks : null),
            versusMode: M.versusMode,
          }],
          null,
        ],
        getAngle: [(a) => [a[0], a[1]], null],
        turnOffHitboxes: [
          (a) => [a[0]],
          (a) => hbSlice(a[0]),
        ],
        shieldTilt: [
          (a) => [a[1], chr(a[0]), {
            face: phys(a[0]).face,
            inCSS: players()[a[0]].inCSS,
            pos: phys(a[0]).pos,
            shieldPosition: phys(a[0]).shieldPosition,
          }, buf4(a[2], a[0])],
          (a) => ({
            shieldPosition: phys(a[0]).shieldPosition,
            shieldPositionReal: phys(a[0]).shieldPositionReal,
          }),
        ],
        reduceByTraction: [
          (a) => [a[1], chr(a[0]), phys(a[0]).cVel.x],
          (a) => ({ cVelX: phys(a[0]).cVel.x }),
        ],
        airDrift: [
          (a) => [chr(a[0]), phys(a[0]).cVel.x, buf4(a[1], a[0])],
          (a) => ({ cVelX: phys(a[0]).cVel.x }),
        ],
        fastfall: [
          (a) => [chr(a[0]), {
            cVelY: phys(a[0]).cVel.y,
            fastfalled: phys(a[0]).fastfalled,
          }, buf4(a[1], a[0])],
          (a) => ({
            cVelY: phys(a[0]).cVel.y,
            fastfalled: phys(a[0]).fastfalled,
          }),
        ],
        shieldDepletion: [
          (a) => [chr(a[0]), {
            grounded: phys(a[0]).grounded,
            kDec: phys(a[0]).kDec,
            kVel: phys(a[0]).kVel,
            shieldHP: phys(a[0]).shieldHP,
            shielding: phys(a[0]).shielding,
          }, buf4(a[1], a[0])],
          (a) => ({
            grounded: phys(a[0]).grounded,
            kDec: phys(a[0]).kDec,
            kVel: phys(a[0]).kVel,
            shieldHP: phys(a[0]).shieldHP,
            shielding: phys(a[0]).shielding,
          }),
        ],
        shieldSize: [
          (a) => [a[1], chr(a[0]), { shieldHP: phys(a[0]).shieldHP },
                  buf4(a[2], a[0])],
          (a) => ({
            shieldAnalog: phys(a[0]).shieldAnalog,
            shieldSize: phys(a[0]).shieldSize,
          }),
        ],
        mashOut: [(a) => [buf4(a[1], a[0])], null],
        checkForSmashes: [
          (a) => [phys(a[0]).face, buf4(a[1], a[0])],
          (a) => ({ face: phys(a[0]).face }),
        ],
        checkForTilts: [
          (a) => (a.length > 2
            ? [phys(a[0]).face, buf4(a[1], a[0]), a[2]]
            : [phys(a[0]).face, buf4(a[1], a[0])]),
          null,
        ],
        checkForIASA: [
          (a) => [iasaSlice(a[0]), chr(a[0]), tapJump(a[0]),
                  buf4(a[1], a[0]), a[2]],
          () => ({}),
        ],
        checkForSpecials: [
          (a) => [{
            bTurnaroundDirection: phys(a[0]).bTurnaroundDirection,
            bTurnaroundTimer: phys(a[0]).bTurnaroundTimer,
            face: phys(a[0]).face,
            grounded: phys(a[0]).grounded,
          }, buf4(a[1], a[0])],
          (a) => ({ face: phys(a[0]).face }),
        ],
        checkForAerials: [(a) => [phys(a[0]).face, buf4(a[1], a[0])], null],
        checkForDash: [(a) => [phys(a[0]).face, buf4(a[1], a[0])], null],
        checkForSmashTurn: [(a) => [phys(a[0]).face, buf4(a[1], a[0])], null],
        tiltTurnDashBuffer: [(a) => [phys(a[0]).face, buf4(a[1], a[0])], null],
        checkForTiltTurn: [(a) => [phys(a[0]).face, buf4(a[1], a[0])], null],
        checkForJump: [(a) => [tapJump(a[0]), buf4(a[1], a[0])], null],
        checkForDoubleJump: [(a) => [tapJump(a[0]), buf4(a[1], a[0])], null],
        checkForMultiJump: [(a) => [tapJump(a[0]), buf4(a[1], a[0])], null],
        checkForSquat: [(a) => [buf4(a[1], a[0])], null],
        turboAirborneInterrupt: [
          (a) => [a[0], chr(a[0]), {
            actionState: players()[a[0]].actionState,
            bTurnaroundDirection: phys(a[0]).bTurnaroundDirection,
            bTurnaroundTimer: phys(a[0]).bTurnaroundTimer,
            doubleJumped: phys(a[0]).doubleJumped,
            face: phys(a[0]).face,
            grounded: phys(a[0]).grounded,
            hitboxes: hbSlice(a[0]),
            jumpsUsed: phys(a[0]).jumpsUsed,
          }, tapJump(a[0]), buf4(a[1], a[0])],
          (a) => ({ face: phys(a[0]).face, hitboxes: hbSlice(a[0]) }),
        ],
        turboGroundedInterrupt: [
          (a) => {
            const s = {
              actionState: players()[a[0]].actionState,
              bTurnaroundDirection: phys(a[0]).bTurnaroundDirection,
              bTurnaroundTimer: phys(a[0]).bTurnaroundTimer,
              face: phys(a[0]).face,
              grounded: phys(a[0]).grounded,
              hitboxes: hbSlice(a[0]),
            };
            // rule 3: dashbuffer is runtime-added — presence-conditional
            if ("dashbuffer" in phys(a[0])) s.dashbuffer = phys(a[0]).dashbuffer;
            return [a[0], chr(a[0]), s, tapJump(a[0]), buf4(a[1], a[0])];
          },
          (a) => {
            const out = { face: phys(a[0]).face, hitboxes: hbSlice(a[0]) };
            if ("dashbuffer" in phys(a[0])) out.dashbuffer = phys(a[0]).dashbuffer;
            return out;
          },
        ],
        setupActionStates: [
          (a) => [a[0], Object.keys(a[1]).sort()],
          null,
        ],
      };

      const CHARS = find((ex) =>
        Array.isArray(ex.intangibility) && Array.isArray(ex.actionSounds) &&
        ex.actionSounds.length >= 5, "characters");

      for (const name of Object.keys(BOUNDARY)) {
        const proj = BOUNDARY[name][0];
        const mutProj = BOUNDARY[name][1];
        const orig = AS[name];
        if (typeof orig !== "function") {
          throw new Error("asshort: expected export missing/not a function: " + name);
        }
        AS[name] = function () {
          const args = Array.prototype.slice.call(arguments);
          const argsCanon = ctx.canon(proj(args)); // PRE-call state
          const fr = { attr: true, dsp: [], rng: [], snd: [] };
          stack.push(fr);
          let ret;
          try {
            ret = orig.apply(this, arguments);
          } finally {
            stack.pop();
          }
          if (mutProj !== null) {
            ctx.push(name, argsCanon, ctx.canon(ret), ctx.canon({
              dsp: fr.dsp, mut: mutProj(args, ret), rng: fr.rng, snd: fr.snd,
            }));
          } else {
            if (fr.dsp.length || fr.rng.length || fr.snd.length) {
              throw new Error("asshort: events escaped pure boundary " + name);
            }
            ctx.push(name, argsCanon, ctx.canon(ret));
          }
          return ret;
        };
        ctx.declare(name);
        ctx.wrapped++;
      }

      this._AS = AS;
      this._M = M;
      this._stack = stack;
      return { moduleIds: moduleIds };
    },

    // Deterministic synthetic-domain sweep (rule 11): the pure boundary
    // functions that read NO player state (player[] holds numbers before
    // setupMatch) but have zero/thin live coverage: getAngle (its wide
    // domain arrives with task 6 hitDetection launch angles + ESCAPEAIR),
    // mashOut (grab-mash: the 12-branch chain with upstream's
    // `!input[..] < 0.7` bool<num coercions), checkForSquat, and the
    // checkForJump/DoubleJump/MultiJump tap-jump branches (live
    // gameSettings tapJumpOff==0 everywhere, so the lsY arms are reachable
    // but rarely hit). Fixed literals only; gameSettings is READ, never
    // written (sweep purity).
    sweep() {
      const AS = this._AS;
      let n = 0;

      const A = [0, -0, 1e-9, -1e-9, 0.3, -0.3, 0.5, -0.5, 1, -1, 6.25e-2,
                 -0.7875, 123.456, -9876.5];
      for (const x of A) {
        for (const y of [0, -0, 0.5, -1, 42.42]) {
          AS.getAngle(x, y); n++;
        }
      }

      // synthetic Input literals (all 22 keys; the module reads history
      // depth <= 3, buffers are given 4 deep)
      const IN = (o) => {
        const base = {
          a: false, b: false, x: false, y: false, z: false, l: false,
          r: false, s: false, du: false, dl: false, dr: false, dd: false,
          lA: 0, rA: 0, lsX: 0, lsY: 0, csX: 0, csY: 0,
          rawX: 0, rawY: 0, rawcsX: 0, rawcsY: 0,
        };
        for (const k of Object.keys(o || {})) base[k] = o[k];
        return base;
      };
      const mkbuf = (i0, i1, i2, i3) => [[IN(i0), IN(i1), IN(i2), IN(i3)]];

      // mashOut: one trigger per branch + the coercion oddities
      const mash = [
        [{ a: true }, {}], [{ b: true }, {}], [{ x: true }, {}],
        [{ y: true }, {}],
        [{ lsX: 0.81 }, { lsX: 0.75 }],      // !0.75 -> 0 < 0.7 true arm
        [{ lsX: 0.81 }, { lsX: -0 }],        // !-0 -> 1 < 0.7 false arm
        [{ lsX: -0.81 }, { lsX: 0 }],
        [{ lsY: 0.81 }, { lsY: 0.2 }],
        [{ lsY: -0.85 }, { lsY: -0.5 }],     // !x > -0.7 arm (always true)
        [{ csX: 0.9 }, { csX: 0.1 }],
        [{ csX: -0.9 }, { csX: 0 }],
        [{ csY: 0.85 }, { csY: 0.3 }],
        [{ csY: -0.85 }, { csY: -0.2 }],
        [{}, {}],                            // no branch -> false
        [{ a: true }, { a: true }],          // held, not pressed
      ];
      for (const m of mash) { AS.mashOut(0, mkbuf(m[0], m[1], {}, {})); n++; }

      for (const y of [-0.7, -0.69, -0.6899999999999999, -1, 0, 0.69]) {
        AS.checkForSquat(0, mkbuf({ lsY: y }, {}, {}, {})); n++;
      }

      // tap-jump arms (live tapJumpOff is 0 == false -> enabled)
      const jumps = [
        [{ x: true }, {}, {}, {}],
        [{ x: true }, { x: true }, {}, {}],
        [{ y: true }, {}, {}, {}],
        [{ lsY: 0.67 }, {}, {}, { lsY: 0.1 }],  // tap jump: y3 < 0.2
        [{ lsY: 0.67 }, {}, {}, { lsY: 0.3 }],  // y3 too high
        [{ lsY: 0.66 }, {}, {}, { lsY: 0 }],    // boundary: not > 0.66
        [{ lsY: 0.7 }, { lsY: 0.69 }, {}, {}],  // double-jump tie arm
        [{ lsY: 0.7 }, { lsY: 0.6900000000000001 }, {}, {}],
        [{ lsY: 0.71 }, { lsY: 0.71 }, {}, {}],
        [{}, {}, {}, {}],
      ];
      for (const j of jumps) {
        AS.checkForJump(0, mkbuf(j[0], j[1], j[2], j[3])); n++;
        AS.checkForDoubleJump(0, mkbuf(j[0], j[1], j[2], j[3])); n++;
        AS.checkForMultiJump(0, mkbuf(j[0], j[1], j[2], j[3])); n++;
      }

      // KO-shout RNG sites (randomShout: ZERO live records over
      // g01/g04/g06 — measured). The seeded match stream must not be
      // drawn from pre-match, so the sweep SWAPS Math.random for a LOCAL
      // mulberry32 (fixed seed below, same algorithm) for the duration
      // and restores the instrumented wrapper afterwards — draws still
      // attribute into the boundary record's post `rng` list, and the
      // replay driver mirrors with a sweep-RNG for frame-0 randomShout
      // records (FORMAT.md "asshort sweep"). 48 calls per char cover all
      // shout outcomes of both switch shapes (x5.99 for chars 0/4,
      // x4.99 for 1-3) deterministically.
      {
        const stack = this._stack;
        const saved = Math.random;
        let a = 0x0badf00d | 0;
        Math.random = function () {
          a = (a + 0x6D2B79F5) | 0;
          let t = Math.imul(a ^ (a >>> 15), 1 | a);
          t = (t + Math.imul(t ^ (t >>> 7), 61 | t)) ^ t;
          const v = ((t ^ (t >>> 14)) >>> 0) / 4294967296;
          const tf = stack.length ? stack[stack.length - 1] : null;
          if (tf && tf.attr) tf.rng.push(v);
          return v;
        };
        try {
          for (let c = 0; c <= 4; c++) {
            for (let k = 0; k < 48; k++) { AS.randomShout(c); n++; }
          }
        } finally {
          Math.random = saved;
        }
      }

      // executeIntangibility (ZERO live records — the traces never roll/
      // tech). Reads exactly player[p].timer + phys.{intangibleTimer,
      // hurtBoxState} + intangibility[characterSelections[p]][name]:
      // inject a synthetic player into INACTIVE slot 3 (playerType -1,
      // pre-match value restored after — sweep purity), so the C side's
      // ml_intang CTAB1 lookup gets live-executed cross-checks
      // (characterSelections[3] is 0/marth pre-match: ESCAPEF [4,20],
      // TECHN [1,20], DOWNSTANDF [6,14]).
      {
        const M = this._M;
        const saved = M.player[3];
        try {
          const cases = [
            ["ESCAPEF", 4], ["ESCAPEF", 5], ["ESCAPEF", 1],
            ["TECHN", 1], ["TECHN", 2],
            ["DOWNSTANDF", 6], ["DOWNSTANDF", 0],
          ];
          for (const cse of cases) {
            M.player[3] = {
              timer: cse[1],
              phys: { intangibleTimer: 0, hurtBoxState: 0 },
            };
            AS.executeIntangibility(cse[0], 3); n++;
          }
        } finally {
          M.player[3] = saved;
        }
      }
      return n;
    },
  };
})();
