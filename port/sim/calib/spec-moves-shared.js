// spec-moves-shared.js — M2 task 7 capture spec: characters/shared moves
// (the move-object template {name, init, main, interrupt(, land)}).
// Registers window.__capSpecs["moves-shared"]. Injected AFTER capturelib.js.
// See FORMAT.md "The moves-shared spec".
//
// BOUNDARY: every function property of every SHARED-ORIGIN actionStates
// entry (fn-identity vs the shared moves-index module: setupActionStates
// deep-copies each char's table, functions copied BY REFERENCE — measured;
// puff's FURAFURA/JUMPAERIALB/JUMPAERIALF are per-char OVERRIDES, tasks
// 8-12) plus the shared JUMPAERIALB/F module objects (checkForIASA's
// direct import path; zero-live). Per char 79 shared entries x 3 phase fns
// (+ land on DAMAGEN2/ESCAPEAIR/FALLSPECIAL/SHIELDBREAKFALL/STOPCEIL/
// DOWNDAMAGE) = 243; puff loses her 3 overrides (9 fns) = 234;
// 4x243 + 234 + 2x3 module fns = 1212 wrapped.
//
// RECORDS:
// - "move" (5-field, mutation-captured): a shared-move phase entered with
//   NO attributing frame on the stack (callers: physics' per-frame state
//   drive, hitdet inits, gameTick setup, per-char move windows). Nested
//   shared->shared calls are TRANSPARENT (the C translation calls its own
//   bodies — structure parallelism). args = [phase, name,
//   [slot, ...extras], inputs, pre] where inputs = per-slot 8-deep buffer
//   slices (moves read input[p][0..6] and input[grabbedBy]) and pre =
//   {alias(probe4), characterSelections, gameMode, gameSettings
//   {tapJumpOffp1..4}, hq, playerType, players, stage {ground, ledge,
//   platform, respawnFace, respawnPoints} | null, versusMode}. post =
//   {alias(probe4), hq, players, rng, snd, vfx}.
// - "mdispatch" (5-field seam): a NON-shared move fn entered while the
//   top frame is the attributing move frame (per-char inits from shared
//   interrupt chains: APPEAL/GRAB/THROW*/ATTACK*/SPECIAL*/CLIFF* variants
//   + the checkFor* payload names; asserted 2-arg (p, input)). args =
//   [phase, name, [slot]], post = {alias(4), hq, players, rng} — the rng
//   list carries the WINDOW's seeded draws so the replay advances the
//   chain at the seam (strengthens rule 14: window draws become
//   chain-exact instead of pinned-zero; seams sit at exact structural
//   points of the C body, so owner/window interleaving is recoverable).
//   Deeper windows push silent non-attributing frames (subsumed).
// - RNG channel (asshort discipline): rngBoot + owner draws in move
//   records' post rng, window draws in seam posts, everything else
//   standalone "Math.random" records burned in file order.
// - sounds/vfx: owner-attributed name queues (snd incl. ".stop" tokens;
//   vfx = drawVfx names — circleDust's 4 seeded draws are owner draws).
// - "mvData" (frame-0 record, finalCheck drift-guarded): the executed
//   move-data plane — per-char {state->name}, {state->sharedOrigin},
//   setVelocities (ESCAPEB/ESCAPEF/DOWNSTANDB/DOWNSTANDF/TECHB/TECHF),
//   CLIFFCATCH/CLIFFWAIT posOffset, CAPTUREDAMAGE setPositions,
//   actionSounds rows for the shared-move keys, palettes[pPal[k]][0].
(() => {
  const EXCLUDE = ["charAttributes", "charHitboxes", "percentShake"];
  const LAND_MOVES = ["DAMAGEN2", "ESCAPEAIR", "FALLSPECIAL",
                      "SHIELDBREAKFALL", "STOPCEIL", "DOWNDAMAGE"];
  // input-argument position per "<NAME>.<phase>" (default 1):
  const INPUT_AT_2 = {
    "JUMPF.init": 1, "JUMPB.init": 1, "KNEEBEND.init": 1, "WALK.init": 1,
    "SHIELDBREAKDOWNBOUND.init": 1, "SHIELDBREAKFALL.land": 1,
  };
  const SND_KEYS = ["CLIFFCATCH", "DEAD", "ESCAPEAIR", "ESCAPEB", "ESCAPEF",
                    "ESCAPEN", "FURAFURA", "GUARDOFF", "GUARDON", "JUMP",
                    "JUMPAERIAL", "OTTOTTOWAIT", "TECH"];
  const SETVEL_KEYS = ["DOWNSTANDB", "DOWNSTANDF", "ESCAPEB", "ESCAPEF",
                       "TECHB", "TECHF"];

  window.__capSpecs["moves-shared"] = {
    expectWrapped: 1212,

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
      const HD = find((ex) =>
        typeof ex.getLaunchAngle === "function" &&
        typeof ex.hitDetect === "function" &&
        Array.isArray(ex.hitQueue), "hitDetection");
      const AST = find((ex) =>
        typeof ex.getActiveStage === "function" &&
        typeof ex.setVsStage === "function", "activeStage");
      const CHARS = find((ex) =>
        Array.isArray(ex.intangibility) && Array.isArray(ex.actionSounds) &&
        ex.actionSounds.length >= 5, "characters");
      const PJ = find((ex) =>
        typeof ex.playerObject === "function" &&
        typeof ex.physicsObject === "function" &&
        typeof ex.createHitboxes === "function", "player");
      // the shared moves-index module (characters/shared/moves/index.js):
      const SHARED = find((ex) =>
        ex.default && typeof ex.default === "object" &&
        ex.default.WAIT && typeof ex.default.WAIT === "object" &&
        typeof ex.default.WAIT.init === "function" &&
        ex.default.THROWNFALCONDIVE && !ex.default.ATTACKAIRF,
        "sharedMovesIndex").default;
      const sharedKeys = Object.keys(SHARED);
      if (sharedKeys.length !== 79) {
        throw new Error("moves-shared: expected 79 shared index keys, got " +
                        sharedKeys.length);
      }
      // the vfx module: drawVfx must be reachable only through move code
      // here; its circleDust RNG is attributed via the Math.random wrap.

      // --- owner stack -------------------------------------------------
      // frames: {attr:true, rng, snd, vfx} (move record) |
      //         {attr:false, seam:true, rng} (recorded window) |
      //         {attr:false} (silent window: a per-char move outside any
      //         move record's scope — physics driving a per-char state)
      // inScope = an attr or seam frame is on the stack (a move record's
      // scope): shared calls there are transparent (nested C tree /
      // seam-subsumed); OUTSIDE any scope a shared call is a RECORD even
      // under a silent per-char window (chain-safe: window draws are
      // standalone records pushed at draw time, file-ordered around it).
      const stack = [];
      const top = () => (stack.length ? stack[stack.length - 1] : null);
      let inScope = 0;

      // --- seeded-RNG stream --------------------------------------------
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
        if (t && t.attr) {
          t.rng.push(v);
        } else {
          let credited = false;
          for (let i = stack.length - 1; i >= 0; i--) {
            if (stack[i].attr || stack[i].seam) {
              stack[i].rng.push(v);
              credited = true;
              break;
            }
          }
          if (!credited) ctx.push("Math.random", "[]", ctx.canon(v));
        }
        return v;
      };

      // --- sound attribution (incl. the furaloop.stop token) -------------
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
        throw new Error("moves-shared: expected 180 Howl sounds, wrapped " +
                        sndWrapped);
      }
      {
        const origStop = SFX.sounds.furaloop.stop;
        SFX.sounds.furaloop.stop = function () {
          const t = top();
          if (t && t.attr) t.snd.push("furaloop.stop");
          return origStop.apply(this, arguments);
        };
      }

      // --- vfx attribution (drawVfx name queue) ---------------------------
      const VFX = find((ex) => typeof ex.drawVfx === "function", "drawVfx");
      {
        const orig = VFX.drawVfx;
        VFX.drawVfx = function (cfg) {
          const t = top();
          if (t && t.attr) t.vfx.push(cfg.name);
          return orig.apply(this, arguments);
        };
      }

      // --- projections -----------------------------------------------------
      const projectPlayer = (p) => {
        const out = {};
        for (const k of Object.keys(p)) {
          if (EXCLUDE.indexOf(k) === -1) out[k] = p[k];
        }
        return out;
      };
      const playersCanon = () =>
        "[" + [0, 1, 2, 3].map((k) =>
          M.playerType[k] > -1 ? ctx.canon(projectPlayer(M.player[k]))
                               : "null").join(",") + "]";
      const probe4 = (k) => {
        const pl = M.player[k];
        return [pl.phys.pos === pl.phys.ECB1[0],
                pl.hitboxes.active === pl.phys.prevFrameHitboxes.active,
                pl.hitboxes.hitList === pl.phys.prevFrameHitboxes.hitList,
                pl.hitboxes.id === pl.phys.prevFrameHitboxes.id];
      };
      const aliasCanon = () =>
        ctx.canon([0, 1, 2, 3].map((k) =>
          M.playerType[k] > -1 ? probe4(k) : null));
      const stageProj = () => {
        const st = AST.getActiveStage();
        if (!st) return null; // sweep records run before setupMatch
        return {
          ground: st.ground,
          ledge: st.ledge,
          platform: st.platform,
          respawnFace: st.respawnFace,
          respawnPoints: st.respawnPoints,
        };
      };
      const preEnvelope = () =>
        '{"alias":' + aliasCanon() +
        ',"characterSelections":' + ctx.canon(M.characterSelections) +
        ',"gameMode":' + ctx.canon(M.gameMode) +
        ',"gameSettings":' + ctx.canon({
          tapJumpOffp1: S.gameSettings.tapJumpOffp1,
          tapJumpOffp2: S.gameSettings.tapJumpOffp2,
          tapJumpOffp3: S.gameSettings.tapJumpOffp3,
          tapJumpOffp4: S.gameSettings.tapJumpOffp4,
        }) +
        ',"hq":' + ctx.canon(HD.hitQueue) +
        ',"playerType":' + ctx.canon(M.playerType) +
        ',"players":' + playersCanon() +
        ',"stage":' + ctx.canon(stageProj()) +
        ',"versusMode":' + ctx.canon(M.versusMode) + "}";
      const isGodInput = (v) =>
        Array.isArray(v) && v.length === 4 &&
        v.every((b) => Array.isArray(b));
      const inputsCanon = (v) =>
        "[" + [0, 1, 2, 3].map((k) =>
          M.playerType[k] > -1 ? ctx.canon(v[k].slice(0, 8)) : "null")
          .join(",") + "]";

      // --- the shared-move boundary wrapper --------------------------------
      const wrapSharedFn = (entry, k) => {
        const orig = entry[k];
        const name = entry.name;
        const inputAt = INPUT_AT_2[name + "." + k] !== undefined ? 2 : 1;
        entry[k] = function () {
          // transparent inside a move record's scope: nested
          // shared->shared calls belong to the enclosing record; calls
          // under seam windows are subsumed by the seam post.
          if (inScope > 0) return orig.apply(this, arguments);
          const args = Array.prototype.slice.call(arguments);
          const inp = args[inputAt];
          if (!isGodInput(inp)) {
            throw new Error("moves-shared: " + name + "." + k +
                            " arg " + inputAt + " is not the god input");
          }
          const extras = inputAt === 2 ? [args[1]]
                                       : args.slice(2);
          const argsCanon = "[" + JSON.stringify(k) + "," +
              JSON.stringify(name) + "," +
              ctx.canon([args[0]].concat(extras)) + "," +
              inputsCanon(inp) + "," + preEnvelope() + "]";
          const fr = { attr: true, rng: [], snd: [], vfx: [] };
          stack.push(fr);
          inScope++;
          let ret;
          try {
            ret = orig.apply(this, arguments);
          } finally {
            stack.pop();
            inScope--;
          }
          const post = '{"alias":' + aliasCanon() +
              ',"hq":' + ctx.canon(HD.hitQueue) +
              ',"players":' + playersCanon() +
              ',"rng":' + ctx.canon(fr.rng) +
              ',"snd":' + ctx.canon(fr.snd) +
              ',"vfx":' + ctx.canon(fr.vfx) + "}";
          ctx.push("move", argsCanon, ctx.canon(ret), post);
          return ret;
        };
        ctx.wrapped++;
      };

      // --- the per-char (non-shared) seam logger -----------------------------
      const wrapSeamFn = (entry, k) => {
        const orig = entry[k];
        const name = entry.name;
        entry[k] = function () {
          const t = top();
          if (t === null || t.attr !== true) {
            // not under an attributing move frame: not this task's record
            stack.push({ attr: false });
            try {
              return orig.apply(this, arguments);
            } finally {
              stack.pop();
            }
          }
          const args = Array.prototype.slice.call(arguments);
          if (args.length !== 2 || !isGodInput(args[1])) {
            throw new Error("moves-shared: seam " + name + "." + k +
                            " is not a 2-arg (p, input) call");
          }
          const argsCanon = "[" + JSON.stringify(k) + "," +
              JSON.stringify(name) + "," + ctx.canon([args[0]]) + "]";
          const w = { attr: false, seam: true, rng: [] };
          stack.push(w);
          inScope++;
          let ret;
          try {
            ret = orig.apply(this, arguments);
          } finally {
            stack.pop();
            inScope--;
          }
          const post = '{"alias":' + aliasCanon() +
              ',"hq":' + ctx.canon(HD.hitQueue) +
              ',"players":' + playersCanon() +
              ',"rng":' + ctx.canon(w.rng) + "}";
          ctx.push("mdispatch", argsCanon, ctx.canon(ret), post);
          return ret;
        };
      };

      // --- classify + wrap every actionStates entry ---------------------------
      const seen = new WeakSet();
      const sharedOrigin = [{}, {}, {}, {}, {}]; // [char][stateKey] = bool
      const nameMap = [{}, {}, {}, {}, {}];
      const perTableWrapped = [0, 0, 0, 0, 0];
      for (let c = 0; c < AS.actionStates.length; c++) {
        const tbl = AS.actionStates[c];
        if (!tbl) continue;
        for (const st of Object.keys(tbl)) {
          const entry = tbl[st];
          if (!entry || typeof entry !== "object") continue;
          if (typeof entry.name !== "string") {
            throw new Error("moves-shared: move object without a name");
          }
          nameMap[c][st] = entry.name;
          const sh = SHARED[st] !== undefined &&
                     typeof SHARED[st].init === "function" &&
                     entry.init === SHARED[st].init;
          sharedOrigin[c][st] = sh;
          if (seen.has(entry)) {
            throw new Error("moves-shared: entry object shared across tables");
          }
          seen.add(entry);
          for (const k of Object.keys(entry)) {
            if (typeof entry[k] !== "function") continue;
            if (sh) {
              wrapSharedFn(entry, k);
              perTableWrapped[c]++;
            } else {
              wrapSeamFn(entry, k);
            }
          }
        }
      }
      // measured composition: 243 per char, puff (char 1) overrides 3 moves
      for (let c = 0; c < 5; c++) {
        const want = c === 1 ? 234 : 243;
        if (perTableWrapped[c] !== want) {
          throw new Error("moves-shared: char " + c + " wrapped " +
                          perTableWrapped[c] + " shared fns, expected " + want);
        }
        for (const st of sharedKeys) {
          if (!(st in sharedOrigin[c])) {
            throw new Error("moves-shared: char " + c + " missing state " + st);
          }
        }
        const overrides = sharedKeys.filter((st) => !sharedOrigin[c][st]);
        if (c === 1) {
          const wantOv = ["FURAFURA", "JUMPAERIALB", "JUMPAERIALF"];
          if (overrides.sort().join(",") !== wantOv.join(",")) {
            throw new Error("moves-shared: puff overrides " + overrides);
          }
        } else if (overrides.length !== 0) {
          throw new Error("moves-shared: char " + c + " overrides " + overrides);
        }
      }
      // the shared JUMPAERIALB/F MODULE objects (checkForIASA import path;
      // zero-live — a record through them for puff would be a measured
      // domain extension, caught by the seam/name verification):
      for (const nm of ["JUMPAERIALB", "JUMPAERIALF"]) {
        const mod = SHARED[nm];
        if (seen.has(mod)) {
          throw new Error("moves-shared: shared module aliased into a table");
        }
        seen.add(mod);
        for (const k of Object.keys(mod)) {
          if (typeof mod[k] !== "function") continue;
          wrapSharedFn(mod, k);
        }
      }
      ctx.declare("move");
      ctx.declare("mdispatch");

      // --- mvData: the executed move-data plane (frame-0 dump) ----------------
      const mvDataCanon = () => {
        const dump = { chars: {}, palettes0: [] };
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
          dump.chars[c] = cd;
        }
        for (let k = 0; k < 4; k++) {
          dump.palettes0.push(M.palettes[M.pPal[k]][0]);
        }
        return ctx.canon(dump);
      };
      this._mvDataAtInstall = mvDataCanon();
      this._mvDataCanon = mvDataCanon;
      ctx.declare("mvData");
      ctx.push("mvData", ctx.canon(["all"]), this._mvDataAtInstall);

      this._M = M;
      this._AS = AS;
      this._PJ = PJ;
      this._stack = stack;
      return { moduleIds: moduleIds };
    },

    // drift guard: the C side's mvData tables are loaded from the frame-0
    // record — sound only if none of the dumped data mutates in-match.
    finalCheck() {
      if (this._mvDataCanon() !== this._mvDataAtInstall) {
        throw new Error("moves-shared: mvData drifted during the match");
      }
      return 1;
    },

    // Synthetic-domain sweep (rules 11/12): the zero-live shared moves with
    // a reachable future domain (rolls/techs/walljumps/shieldbreak/
    // FURASLEEP/DEAD variants...). A REAL upstream playerObject(0, ...) is
    // injected into inactive slot 3 (playerType[3] set for the duration and
    // restored — rule-12 net-restore purity, guarded by x2 byte-stability +
    // STREAM MATCH), inputs are a fixed all-neutral god array, and
    // Math.random is SWAPPED for a local mulberry32 (seed 0x0badf00d) so
    // ESCAPEN's circleDust and DEAD*'s screenShake draws never touch the
    // seeded match stream (the replay mirrors with a sweep RNG chained
    // across all frame-0 move records). FURAFURA stays unswept: its init
    // stores the Howl play id (furaLoopID), which is outside the sim value
    // domain (the C body traps there by design).
    sweep() {
      const M = this._M;
      const AS = this._AS;
      const PJ = this._PJ;
      const stack = this._stack;
      let n = 0;

      const IN22 = () => ({
        a: false, b: false, x: false, y: false, z: false, l: false,
        r: false, s: false, du: false, dl: false, dr: false, dd: false,
        lA: 0, rA: 0, lsX: 0, lsY: 0, csX: 0, csY: 0,
        rawX: 0, rawY: 0, rawcsX: 0, rawcsY: 0,
      });
      const buf8 = () => {
        const b = [];
        for (let i = 0; i < 8; i++) b.push(IN22());
        return b;
      };
      const IN = [buf8(), buf8(), buf8(), buf8()];

      const savedP3 = M.player[3];
      const savedT3 = M.playerType[3];
      const savedRandom = Math.random;
      let a = 0x0badf00d | 0;
      Math.random = function () {
        a = (a + 0x6D2B79F5) | 0;
        let t = Math.imul(a ^ (a >>> 15), 1 | a);
        t = (t + Math.imul(t ^ (t >>> 7), 61 | t)) ^ t;
        const v = ((t ^ (t >>> 14)) >>> 0) / 4294967296;
        const tf = stack.length ? stack[stack.length - 1] : null;
        if (tf && tf.attr) {
          tf.rng.push(v);
        } else {
          for (let i = stack.length - 1; i >= 0; i--) {
            if (stack[i].attr || stack[i].seam) { stack[i].rng.push(v); break; }
          }
        }
        return v;
      };
      try {
        M.playerType[3] = 0;
        const fresh = (mut) => {
          // playerObject -> physicsObject reads pos as an ARRAY [x, y]
          M.player[3] = new PJ.playerObject(0, [10, 20], 1);
          if (mut) mut(M.player[3]);
        };
        const T = AS.actionStates[0]; // marth's table (charSelections[3]=0)
        const call = (name, phase, extra) => {
          if (extra === undefined) T[name][phase](3, IN);
          else if (phase === "landN") T[name].land(3, extra, IN); // (p,normal,input)
          else T[name][phase](3, IN, extra);
          n++;
        };
        // rolls / spotdodge / airdodge (executeIntangibility + setVelocities)
        fresh(); call("ESCAPEB", "init");
        fresh(); call("ESCAPEF", "init");
        fresh(); call("ESCAPEN", "init"); // circleDust: 4 sweep draws
        fresh(); call("ESCAPEAIR", "init");
        fresh(); call("ESCAPEAIR", "land");
        // techs
        fresh(); call("TECHN", "init");
        fresh(); call("TECHB", "init");
        fresh(); call("TECHF", "init");
        fresh(); call("TECHU", "init");
        // walls
        fresh(); call("WALLTECH", "init");
        fresh(); call("WALLTECHJUMP", "init");
        fresh((p3) => { p3.phys.wallJumpCount = 2; });
        call("WALLJUMP", "init");
        fresh(); call("WALLDAMAGE", "init", { x: -1, y: 0 }); // no-reflect arm
        fresh((p3) => {
          p3.phys.kVel = { x: 5, y: -3 };
          p3.phys.kDec = { x: 0.2, y: -0.1 };
        });
        call("WALLDAMAGE", "init", { x: 1, y: 0 }); // reflect arm
        // ceiling
        fresh(); call("STOPCEIL", "init"); // normal = null default
        fresh((p3) => {
          p3.phys.kVel = { x: 1, y: 4 };
          p3.phys.kDec = { x: 0.1, y: 0.2 };
          p3.hit.hitstun = 20;
        });
        call("STOPCEIL", "init", { x: 0, y: -1 }); // reflect arm
        fresh((p3) => { p3.hit.hitstun = 10; });
        call("STOPCEIL", "land");
        fresh((p3) => { p3.hit.hitstun = 10; p3.phys.techTimer = 5; });
        call("STOPCEIL", "land"); // TECHN arm
        fresh(); call("STOPCEIL", "land"); // LANDING arm
        // shieldbreak chain (FURAFURA itself excluded: furaLoopID)
        fresh(); call("SHIELDBREAKFALL", "init");
        fresh(); call("SHIELDBREAKFALL", "landN", 0.5); // land(p,normal,input)
        fresh(); T.SHIELDBREAKDOWNBOUND.init(3, 0.5, IN); n++; // (p,normal,in)
        fresh(); call("SHIELDBREAKSTAND", "init");
        // fura sleep (palette blend + rgb string formatting)
        fresh((p3) => { p3.percent = 42.5; });
        call("FURASLEEPSTART", "init");
        fresh((p3) => { p3.phys.stuckTimer = 30; });
        call("FURASLEEPLOOP", "init");
        fresh(); call("FURASLEEPEND", "init");
        // downed stands
        fresh(); call("DOWNSTANDN", "init");
        fresh(); call("DOWNSTANDB", "init");
        fresh(); call("DOWNSTANDF", "init");
        fresh((p3) => { p3.phys.grounded = true; p3.hit.hitstun = 5; });
        call("DOWNDAMAGE", "init");
        fresh(); call("DOWNDAMAGE", "land");
        // deaths (screenShake: 4 sweep draws each; timer 1 -> no REBIRTH)
        fresh(); call("DEADUP", "init");
        fresh(); call("DEADRIGHT", "init");
        // misc zero-live
        fresh(); call("MISSFOOT", "init");
        fresh(); call("SLEEP", "init");
        fresh(); call("PASS", "init");
        fresh(); call("THROWNFALCONDIVE", "init");
        fresh(); call("CAPTUREWAIT", "init"); // grabbedBy -1 early return
        fresh(); call("CAPTUREDAMAGE", "init"); // grabbedBy -1 arm in main
        fresh((p3) => { p3.phys.jumpType = 0; });
        T.JUMPF.init(3, 0, IN); n++; // short-hop arm
        fresh(); T.JUMPB.init(3, 1, IN); n++; // full-hop arm
        fresh(); T.KNEEBEND.init(3, 1, IN); n++;
        fresh(); T.WALK.init(3, false, IN); n++; // no-initV arm
        fresh((p3) => { p3.phys.grounded = true; });
        T.FALL.init(3, IN, true); n++; // disableInputs arm
      } finally {
        Math.random = savedRandom;
        M.player[3] = savedP3;
        M.playerType[3] = savedT3;
      }
      return n;
    },
  };
})();
