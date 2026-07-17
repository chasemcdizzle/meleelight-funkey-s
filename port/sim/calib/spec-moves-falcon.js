// spec-moves-falcon.js — M2 task 10 capture spec: characters/falcon moves
// (third per-char cluster; task 8's fox recipe followed — see
// spec-moves-fox.js for the record-type documentation, which applies with
// fox->falcon plus the deltas below). Registers
// window.__capSpecs["moves-falcon"]. Injected AFTER capturelib.js.
// See FORMAT.md "The moves-falcon spec".
//
// BOUNDARY: every function property of every FALCON-ORIGIN actionStates
// entry (fn identity vs the characters/falcon/moves module index — rule
// 15's instrument). Falcon-origin entries exist ONLY on table 4: 67 module
// keys — 53 moves x {init,main,interrupt} + 13 x {+land} (ATTACKAIR*x5,
// NEUTRALSPECIALAIR, SIDESPECIAL{AIR,AIRHIT,GROUND}, DOWNSPECIALAIR,
// DOWNSPECIAL{AIR,GROUND}ENDAIR, UPSPECIAL, UPSPECIALCATCH) +
// SIDESPECIALGROUNDTOAIR's lone {init,main} = 214 phase fns, PLUS falcon's
// three NON-phase move fns (upstream-unique dispatch surfaces):
// onPlayerHit(p) on SIDESPECIAL{GROUND,AIR} (hitDetection.js:493, the
// specialOnHit arm) and onWallCollide(p,input,wallFace,wallNum) on
// DOWNSPECIALGROUND (physics.js:122, the specialWallCollide arm) = 217
// falcon fns, plus the 2 article init boundaries (LASER/ILLUSION,
// measurement only — falcon imports `articles` in 6 files but NEVER
// dereferences it: dead imports, zero call sites; the article-seam count
// is pinned ZERO) = 219 wrapped.
//
// GOLDENS: falcon carriers measured for live coverage — see
// expected-capture-moves-falcon.json (candidates g03 falcon/fox/pstadium,
// g04 puff/falcon/dreamland, g06 falcon/marth/fountain, g07's CPU falcon).
//
// STRUCTURE NOTES vs fox/falco (measured per-file diffs): falcon's
// THROWN* family is byte-identical to FOX's shape (grabbedBy===-1 guards
// AND offset-length clamps present) — offsets differ only as data (mvData);
// falcon's THROW* have fox's grabbing===-1 init guard but fire NO lasers;
// falcon's CLIFF* keep fox's onLedge===-1 canGrabLedge table-write arm;
// SIDESPECIALGROUND writes `this.canEdgeCancel` (a SCALAR move-table
// write — outside the array-only mvData dump; the C models it as module
// state); UPSPECIALCATCH/UPSPECIALTHROW draw Math.random INLINE (2 draws
// per firefoxtail spawn x3) and UPSPECIALCATCH pushes a hitQueue row from
// its interrupt; SIDESPECIALGROUNDHIT.main reads player[p].phys.timer
// (undefined upstream — physicsObject has no timer) and
// DOWNSPECIALGROUNDENDAIR.main reads player.timer (the ARRAY — undefined):
// both arms are dead code carried verbatim.
(() => {
  const EXCLUDE = ["charAttributes", "charHitboxes", "percentShake"];
  const SND_KEYS = ["CLIFFCATCH", "DEAD", "ESCAPEAIR", "ESCAPEB", "ESCAPEF",
                    "ESCAPEN", "FURAFURA", "GUARDOFF", "GUARDON", "JUMP",
                    "JUMPAERIAL", "OTTOTTOWAIT", "TECH"];
  const SETVEL_KEYS = ["DOWNSTANDB", "DOWNSTANDF", "ESCAPEB", "ESCAPEF",
                       "TECHB", "TECHF"];

  window.__capSpecs["moves-falcon"] = {
    expectWrapped: 219,

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
      const SHARED = find((ex) =>
        ex.default && typeof ex.default === "object" &&
        ex.default.WAIT && typeof ex.default.WAIT === "object" &&
        typeof ex.default.WAIT.init === "function" &&
        ex.default.THROWNFALCONDIVE && !ex.default.ATTACKAIRF,
        "sharedMovesIndex").default;
      const ART = find((ex) =>
        ex.articles && ex.articles.LASER && ex.articles.ILLUSION &&
        typeof ex.articles.LASER.init === "function" &&
        typeof ex.executeArticles === "function", "article");
      // the falcon module index (characters/falcon/moves/index.js) — found
      // by fn IDENTITY against the live table (rule 15's measurement);
      // UPSPECIALCATCH/UPSPECIALTHROW are unique to falcon's index:
      const falconTbl = AS.actionStates[4];
      if (!falconTbl || !falconTbl.JAB1) {
        throw new Error("moves-falcon: actionStates[4].JAB1 missing");
      }
      const FALCONMOVES = find((ex) =>
        ex.default && typeof ex.default === "object" &&
        ex.default.JAB1 && typeof ex.default.JAB1 === "object" &&
        typeof ex.default.JAB1.init === "function" &&
        ex.default.JAB1.init === falconTbl.JAB1.init &&
        ex.default.UPSPECIALCATCH && ex.default.UPSPECIALTHROW &&
        !ex.default.WAIT && !ex.default.moves, "falconMovesIndex").default;
      const falconKeys = Object.keys(FALCONMOVES);
      if (falconKeys.length !== 67) {
        throw new Error("moves-falcon: expected 67 falcon index keys, got " +
                        falconKeys.length);
      }

      // --- owner stack (task-7/8 semantics verbatim) ----------------------
      const stack = [];
      const top = () => (stack.length ? stack[stack.length - 1] : null);
      let inScope = 0;

      // --- seeded-RNG stream ---------------------------------------------
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

      // --- sound attribution (incl. ".stop" tokens) -----------------------
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
        throw new Error("moves-falcon: expected 180 Howl sounds, wrapped " +
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

      // --- vfx attribution -------------------------------------------------
      const VFX = find((ex) => typeof ex.drawVfx === "function", "drawVfx");
      {
        const orig = VFX.drawVfx;
        VFX.drawVfx = function (cfg) {
          const t = top();
          if (t && t.attr) t.vfx.push(ctx.canon(cfg)); // M4 task 1: full config, CALL-TIME canon (snapshot semantics)
          return orig.apply(this, arguments);
        };
      }

      // --- article seam (measurement instrument: falcon has NO article
      // call sites — the pin freezes the count at ZERO; a record here means
      // the measured claim broke) --------------------------------------------
      const wrapArticleInit = (name) => {
        const obj = ART.articles[name];
        const orig = obj.init;
        obj.init = function (options) {
          const t = top();
          if (t !== null && t.attr === true) {
            const argsCanon = "[" + JSON.stringify(name) + "," +
                ctx.canon(options) + "]";
            const ret = orig.apply(this, arguments);
            ctx.push("article", argsCanon, ctx.canon(ret));
            return ret;
          }
          return orig.apply(this, arguments);
        };
        ctx.wrapped++;
      };
      wrapArticleInit("LASER");
      wrapArticleInit("ILLUSION");
      ctx.declare("article");

      // --- projections ------------------------------------------------------
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

      // --- the falcon-move boundary wrapper -----------------------------------
      const wrapFalconFn = (entry, k) => {
        const orig = entry[k];
        const name = entry.name;
        entry[k] = function () {
          if (inScope > 0) return orig.apply(this, arguments);
          const args = Array.prototype.slice.call(arguments);
          // falcon phases are (p[, input]); onPlayerHit is (p);
          // onWallCollide is (p, input, wallFace, wallNum) — the ONLY
          // >2-arg site in the set (physics.js:122).
          let extras = [];
          let inCanon = "null"; // 1-arg sites: THROWN*{BACK,DOWN} inits,
                                // onPlayerHit
          if (k === "onWallCollide") {
            if (args.length !== 4 || !isGodInput(args[1]) ||
                typeof args[2] !== "string" ||
                typeof args[3] !== "number") {
              throw new Error("moves-falcon: " + name +
                              ".onWallCollide unexpected signature");
            }
            inCanon = inputsCanon(args[1]);
            extras = [args[2], args[3]];
          } else {
            if (args.length > 2) {
              throw new Error("moves-falcon: " + name + "." + k +
                              " called with " + args.length + " args");
            }
            if (args.length === 2) {
              if (!isGodInput(args[1])) {
                throw new Error("moves-falcon: " + name + "." + k +
                                " arg 1 is not the god input");
              }
              inCanon = inputsCanon(args[1]);
            }
          }
          const argsCanon = "[" + JSON.stringify(k) + "," +
              JSON.stringify(name) + "," +
              ctx.canon([args[0]].concat(extras)) + "," +
              inCanon + "," + preEnvelope() + "]";
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
              ',"vfx":[' + fr.vfx.join(",") + ']' + "}";
          ctx.push("move", argsCanon, ctx.canon(ret), post);
          return ret;
        };
        ctx.wrapped++;
      };

      // --- the non-falcon per-char seam logger ------------------------------
      const wrapSeamFn = (entry, k) => {
        const orig = entry[k];
        const name = entry.name;
        entry[k] = function () {
          const t = top();
          if (t === null || t.attr !== true) {
            stack.push({ attr: false });
            try {
              return orig.apply(this, arguments);
            } finally {
              stack.pop();
            }
          }
          const args = Array.prototype.slice.call(arguments);
          // (p) — THROWBACK/THROWDOWN dispatch sites — or (p, input)
          if (args.length > 2 ||
              (args.length === 2 && !isGodInput(args[1]))) {
            throw new Error("moves-falcon: seam " + name + "." + k +
                            " has an unexpected signature");
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

      // --- classify + wrap every actionStates entry --------------------------
      // shared entries stay UNWRAPPED (task-8 scope rule): at top level they
      // are silent chain-safe surface; inside a falcon record they are the
      // transparent nested C tree (task 7's bodies).
      const seen = new WeakSet();
      const sharedOrigin = [{}, {}, {}, {}, {}];
      const falconOrigin = [{}, {}, {}, {}, {}];
      const nameMap = [{}, {}, {}, {}, {}];
      const perTableFalcon = [0, 0, 0, 0, 0];
      for (let c = 0; c < AS.actionStates.length; c++) {
        const tbl = AS.actionStates[c];
        if (!tbl) continue;
        for (const st of Object.keys(tbl)) {
          const entry = tbl[st];
          if (!entry || typeof entry !== "object") continue;
          if (typeof entry.name !== "string") {
            throw new Error("moves-falcon: move object without a name");
          }
          nameMap[c][st] = entry.name;
          const sh = SHARED[st] !== undefined &&
                     typeof SHARED[st].init === "function" &&
                     entry.init === SHARED[st].init;
          sharedOrigin[c][st] = sh;
          const fc = !sh && FALCONMOVES[st] !== undefined &&
                     typeof FALCONMOVES[st].init === "function" &&
                     entry.init === FALCONMOVES[st].init;
          falconOrigin[c][st] = fc;
          if (seen.has(entry)) {
            throw new Error("moves-falcon: entry object shared across tables");
          }
          seen.add(entry);
          for (const k of Object.keys(entry)) {
            if (typeof entry[k] !== "function") continue;
            if (fc) {
              wrapFalconFn(entry, k);
              perTableFalcon[c]++;
            } else if (!sh) {
              wrapSeamFn(entry, k);
            }
          }
        }
      }
      // measured composition: falcon-origin only on table 4, all 67 states,
      // 217 fns (214 phase fns + 2 onPlayerHit + 1 onWallCollide)
      for (let c = 0; c < 5; c++) {
        const want = c === 4 ? 217 : 0;
        if (perTableFalcon[c] !== want) {
          throw new Error("moves-falcon: table " + c + " wrapped " +
                          perTableFalcon[c] + " falcon fns, expected " +
                          want);
        }
      }
      for (const st of falconKeys) {
        if (!falconOrigin[4][st]) {
          throw new Error(
            "moves-falcon: falcon table state not falcon-origin: " + st);
        }
        if (nameMap[4][st] !== st) {
          throw new Error("moves-falcon: falcon state key != move name: " +
                          st);
        }
      }
      ctx.declare("move");
      ctx.declare("mdispatch");

      // --- mvData: task-7 dump + the falcon extension (rule 15) ---------------
      const mvDataCanon = () => {
        const dump = { chars: {},
                       falcon: { data: {}, origin: falconOrigin[4] },
                       palettes0: [] };
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
        // falcon data plane: every own enumerable ARRAY-valued non-function
        // prop of every falcon-origin entry (executed data, never retyped).
        // NOTE SIDESPECIALGROUND.canEdgeCancel is a runtime-written SCALAR
        // (outside this array-only dump — modeled as C module state).
        for (const st of falconKeys) {
          const entry = AS.actionStates[4][st];
          const d = {};
          let any = false;
          for (const k of Object.keys(entry)) {
            if (Array.isArray(entry[k])) { d[k] = entry[k]; any = true; }
          }
          if (any) dump.falcon.data[st] = d;
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
      this._HD = HD;
      this._ART = ART;
      this._stack = stack;
      return { moduleIds: moduleIds };
    },

    finalCheck() {
      if (this._mvDataCanon() !== this._mvDataAtInstall) {
        throw new Error("moves-falcon: mvData drifted during the match");
      }
      return 1;
    },

    // Synthetic-domain sweep (rules 11/12): the task-8 scaffolding reused —
    // a REAL upstream playerObject(4, [10,20], 1) injected into inactive
    // slot 3 with characterSelections[3] set to 4 (both restored); a second
    // player in slot 2 covers the THROWN* grabbedBy<p (timer=-1) arm;
    // Math.random swapped for the sweep mulberry32 (0x0badf00d); hitQueue
    // pushes spliced back out (net-restore purity, guarded by x2
    // byte-stability + STREAM MATCH). CLIFF* run against the default
    // pre-match stage. Falcon-specific coverage: FALCONPUNCH (both
    // environments, setVel/vfx/sound arms), the FALCONKICK 6-state family
    // (incl. the onWallCollide UPSPECIALTHROW arm + its inline sweep-RNG
    // firefoxtail draws), FALCONDIVE (UPSPECIAL grab arm ->
    // UPSPECIALCATCH -> UPSPECIALTHROW chain, hq push, inline draws),
    // raptor boost (both environments + onPlayerHit + the HIT states),
    // THROW* incl. the fox-style grabbing===-1 init guards, all 20
    // THROWN* inits + guard/clamp arms, all 8 CLIFF*.
    // SIDESPECIALGROUND's `this.canEdgeCancel = true` main arm (timer>16)
    // is swept LAST among SSG calls and the table value restored (rule 12
    // net-restore; the value is read only by physics on live SSG frames,
    // where init has always re-cleared it first).
    // Unswept (documented): the CLIFF* onLedge===-1 canGrabLedge
    // table-write arm (fox precedent: C traps; mvData finalCheck guards),
    // THROWN* offset overruns past the clamp window (upstream throws),
    // interrupt-tail WALK arms shadowed by checkForTilts,
    // SIDESPECIALGROUNDHIT's phys.timer arm and DOWNSPECIALGROUNDENDAIR's
    // player.timer arms (dead upstream — undefined comparisons).
    sweep() {
      const M = this._M;
      const AS = this._AS;
      const PJ = this._PJ;
      const HD = this._HD;
      const ART = this._ART;
      const stack = this._stack;
      let n = 0;

      const IN22 = (over) => {
        const o = {
          a: false, b: false, x: false, y: false, z: false, l: false,
          r: false, s: false, du: false, dl: false, dr: false, dd: false,
          lA: 0, rA: 0, lsX: 0, lsY: 0, csX: 0, csY: 0,
          rawX: 0, rawY: 0, rawcsX: 0, rawcsY: 0,
        };
        if (over) for (const k of Object.keys(over)) o[k] = over[k];
        return o;
      };
      const buf8 = (overByDepth) => {
        const b = [];
        for (let i = 0; i < 8; i++) {
          b.push(IN22(overByDepth ? overByDepth[i] : null));
        }
        return b;
      };
      // god input with per-depth overrides on SLOT 3 only
      const mk = (overByDepth) =>
        [buf8(), buf8(), buf8(), buf8(overByDepth)];
      const INn = mk(null);
      const INa = mk({ 0: { a: true } });
      const INy = mk({ 0: { y: true } });
      const INb = mk({ 0: { b: true } });
      const INyBack = mk({ 0: { y: true, lsX: -0.5 } });
      const INaUp = mk({ 0: { a: true, lsY: 0.8 }, 1: { lsY: 0.8 },
                         2: { lsY: 0.8 }, 3: { lsY: 0.8 } });
      const INbDown = mk({ 0: { b: true, lsY: -1 } });
      const INlA = mk({ 0: { lA: 1 } });
      const INstick = mk({ 0: { lsX: 0.5, lsY: 0.5 } });
      const INbackStick = mk({ 0: { lsX: -0.35 } });

      const savedP3 = M.player[3];
      const savedT3 = M.playerType[3];
      const savedP2 = M.player[2];
      const savedT2 = M.playerType[2];
      const savedCS3 = M.characterSelections[3];
      const savedRandom = Math.random;
      const savedArticles = ART.aArticles.length;
      const savedHq = HD.hitQueue.length;
      const SSG = AS.actionStates[4].SIDESPECIALGROUND;
      const savedCanEdgeCancel = SSG.canEdgeCancel;
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
        M.characterSelections[3] = 4; // falcon
        const fresh = (mut) => {
          M.player[3] = new PJ.playerObject(4, [10, 20], 1);
          if (mut) mut(M.player[3]);
        };
        const T = AS.actionStates[4]; // falcon's table
        const call = (name, phase, IN) => {
          T[name][phase](3, IN);
          n++;
        };
        const grab = (p3) => {
          p3.phys.grabbing = 3;
          p3.phys.grabbedBy = 3;
        };

        // jabs (falcon: JAB1 interrupt is jabCombo/WAIT only — no
        // checkFor block; the payload-dispatch arms live in JAB3 etc.)
        fresh(); call("JAB1", "init", INn);
        fresh((p) => { p.timer = 2; }); call("JAB1", "main", INn); // active 3
        fresh((p) => { p.timer = 9; p.phys.jabCombo = true; });
        call("JAB1", "interrupt", INn); // -> JAB2 (>8)
        fresh(); call("JAB2", "init", INn);
        fresh((p) => { p.timer = 4; }); call("JAB2", "main", INn); // active 5
        fresh((p) => { p.timer = 8; p.phys.jabCombo = true; });
        call("JAB2", "interrupt", INn); // -> JAB3 (>7)
        fresh(); call("JAB3", "init", INn);
        fresh((p) => { p.timer = 5; }); call("JAB3", "main", INn); // active 6
        fresh((p) => { p.timer = 8; }); call("JAB3", "main", INn); // late 9
        fresh((p) => { p.timer = 12; }); call("JAB3", "main", INn); // off 13
        fresh((p) => { p.timer = 23; });
        call("JAB3", "interrupt", INy); // KNEEBEND arm (>22)
        fresh((p) => { p.timer = 23; });
        call("JAB3", "interrupt", INbDown); // MOVES[b] special dispatch
        fresh((p) => { p.timer = 23; });
        call("JAB3", "interrupt", INbackStick); // TILTTURN tail arm
        // tilts
        fresh(); call("UPTILT", "init", INn);
        fresh((p) => { p.timer = 16; }); call("UPTILT", "main", INn); // 17
        fresh((p) => { p.timer = 21; }); call("UPTILT", "main", INn); // off
        fresh((p) => { p.timer = 38; });
        call("UPTILT", "interrupt", INn); // checkFor false block (>37)
        fresh(); call("FORWARDTILT", "init", INn);
        fresh((p) => { p.timer = 8; });
        call("FORWARDTILT", "main", INn); // active 9
        fresh((p) => { p.timer = 11; });
        call("FORWARDTILT", "main", INn); // off 12
        fresh(); call("DOWNTILT", "init", INn);
        fresh((p) => { p.timer = 9; }); call("DOWNTILT", "main", INn); // 10
        fresh((p) => { p.timer = 15; }); call("DOWNTILT", "main", INn); // 16
        fresh((p) => { p.timer = 36; });
        call("DOWNTILT", "interrupt", INn); // SQUATWAIT (>35)
        // smashes
        fresh(); call("FORWARDSMASH", "init", INn);
        fresh((p) => { p.timer = 10; p.phys.chargeFrames = 4; });
        call("FORWARDSMASH", "main", INa); // charge + smashcharge at 5
        fresh((p) => { p.timer = 10; p.phys.chargeFrames = 59; });
        call("FORWARDSMASH", "main", INa); // charge release at 60
        const fsId = (p) => {
          p.hitboxes.id[0] = p.charHitboxes.fsmash.id0; // init's assignment
        };
        fresh((p) => { fsId(p); p.timer = 17; });
        call("FORWARDSMASH", "main", INn); // 18: active + randomShout +
        // fireweakhit + firefoxtail window (reads id[0].offset[frame])
        fresh((p) => { fsId(p); p.timer = 19; });
        call("FORWARDSMASH", "main", INn); // frame++ + firefoxtail
        fresh((p) => { p.timer = 21; });
        call("FORWARDSMASH", "main", INn); // 22 turnoff
        fresh((p) => { p.timer = 60; });
        call("FORWARDSMASH", "interrupt", INn); // checkFor block (>59)
        fresh(); call("UPSMASH", "init", INn); // randomShout in init
        fresh((p) => { p.timer = 8; p.phys.chargeFrames = 4; });
        call("UPSMASH", "main", INa); // charge arm at 8
        fresh((p) => { p.timer = 20; }); call("UPSMASH", "main", INn); // 21
        fresh((p) => { p.timer = 22; }); call("UPSMASH", "main", INn); // off
        fresh((p) => { p.timer = 26; });
        call("UPSMASH", "main", INn); // upsmash2 swap at 27
        fresh((p) => { p.timer = 28; }); call("UPSMASH", "main", INn); // off
        fresh(); call("DOWNSMASH", "init", INn);
        fresh((p) => { p.timer = 14; p.phys.chargeFrames = 4; });
        call("DOWNSMASH", "main", INa); // charge arm at 14
        fresh((p) => { p.timer = 18; });
        call("DOWNSMASH", "main", INn); // 19 active + randomShout
        fresh((p) => { p.timer = 22; }); call("DOWNSMASH", "main", INn);
        fresh((p) => { p.timer = 28; });
        call("DOWNSMASH", "main", INn); // dsmash2 swap at 29
        fresh((p) => { p.timer = 32; }); call("DOWNSMASH", "main", INn);
        fresh((p) => { p.timer = 45; });
        call("DOWNSMASH", "interrupt", INn); // !inCSS checkFor block
        fresh(); call("ATTACKDASH", "init", INn);
        fresh((p) => { p.timer = 6; }); call("ATTACKDASH", "main", INn); // 7
        fresh((p) => { p.timer = 9; });
        call("ATTACKDASH", "main", INn); // late swap at 10
        fresh((p) => { p.timer = 16; }); call("ATTACKDASH", "main", INn);
        fresh((p) => { p.timer = 27; });
        call("ATTACKDASH", "main", INn); // >=27 else cVel arm
        fresh((p) => { p.timer = 1; p.phys.cVel.x = 99; });
        call("ATTACKDASH", "interrupt", INlA); // GRAB arm + dMaxV clamp
        fresh((p) => { p.timer = 38; });
        call("ATTACKDASH", "interrupt", INn); // checkFor block (>37)
        // aerials
        fresh(); call("ATTACKAIRN", "init", INn);
        fresh((p) => { p.timer = 6; });
        call("ATTACKAIRN", "main", INn); // 7 active
        fresh((p) => { p.timer = 8; });
        call("ATTACKAIRN", "main", INn); // frames++ NaN quirk (8..12)
        fresh((p) => { p.timer = 19; });
        call("ATTACKAIRN", "main", INn); // nair2 swap at 20
        fresh((p) => { p.timer = 29; });
        call("ATTACKAIRN", "main", INn); // 30 off
        fresh((p) => { p.timer = 33; });
        call("ATTACKAIRN", "main", INn); // 34 autoCancel
        fresh((p) => { p.IASATimer = 1; p.timer = 5; });
        call("ATTACKAIRN", "interrupt", INy); // IASA -> JUMPAERIALF
        fresh((p) => { p.IASATimer = 1; p.timer = 5; });
        call("ATTACKAIRN", "interrupt", INyBack); // IASA -> JUMPAERIALB
        fresh((p) => { p.IASATimer = 1; p.timer = 5; });
        call("ATTACKAIRN", "interrupt", INaUp); // IASA aerial payload: NO
        // dispatch for char 4 upstream (no branch) — returns true bare
        fresh((p) => { p.timer = 45; });
        call("ATTACKAIRN", "interrupt", INn); // FALL (>44)
        fresh((p) => { p.phys.autoCancel = true; });
        call("ATTACKAIRN", "land", INn); // LANDING (circleDust: 4 draws)
        fresh((p) => { p.phys.autoCancel = false; });
        call("ATTACKAIRN", "land", INn); // LANDINGATTACKAIRN
        fresh(); call("ATTACKAIRF", "init", INn);
        fresh((p) => { p.timer = 13; });
        call("ATTACKAIRF", "main", INn); // 14 active
        fresh((p) => { p.timer = 16; });
        call("ATTACKAIRF", "main", INn); // 17 late swap
        fresh((p) => { p.timer = 30; });
        call("ATTACKAIRF", "main", INn); // 31 off
        fresh((p) => { p.timer = 34; });
        call("ATTACKAIRF", "main", INn); // 35 autoCancel
        fresh((p) => { p.phys.autoCancel = false; });
        call("ATTACKAIRF", "land", INn); // LANDINGATTACKAIRF
        fresh(); call("ATTACKAIRB", "init", INn);
        fresh((p) => { p.timer = 9; });
        call("ATTACKAIRB", "main", INn); // 10 active
        fresh((p) => { p.timer = 13; });
        call("ATTACKAIRB", "main", INn); // 14 swap
        fresh((p) => { p.timer = 17; });
        call("ATTACKAIRB", "main", INn); // 18 off
        fresh((p) => { p.timer = 20; });
        call("ATTACKAIRB", "main", INn); // 21 autoCancel
        fresh((p) => { p.phys.autoCancel = false; });
        call("ATTACKAIRB", "land", INn);
        fresh(); call("ATTACKAIRU", "init", INn);
        fresh((p) => { p.timer = 5; });
        call("ATTACKAIRU", "main", INn); // 6 active
        fresh((p) => { p.timer = 9; });
        call("ATTACKAIRU", "main", INn); // 10 upairMid swap
        fresh((p) => { p.timer = 13; });
        call("ATTACKAIRU", "main", INn); // 14 off
        fresh((p) => { p.timer = 21; });
        call("ATTACKAIRU", "main", INn); // 22 autoCancel (else-if arm)
        fresh((p) => { p.phys.autoCancel = false; });
        call("ATTACKAIRU", "land", INn);
        fresh(); call("ATTACKAIRD", "init", INn); // falconshout5
        fresh((p) => { p.timer = 15; });
        call("ATTACKAIRD", "main", INn); // 16 active
        fresh((p) => { p.timer = 17; });
        call("ATTACKAIRD", "main", INn); // frames++ NaN quirk (17..20)
        fresh((p) => { p.timer = 20; });
        call("ATTACKAIRD", "main", INn); // 21 off
        fresh((p) => { p.timer = 35; });
        call("ATTACKAIRD", "main", INn); // 36 autoCancel
        fresh((p) => { p.phys.autoCancel = false; });
        call("ATTACKAIRD", "land", INn);
        // falcon punch
        fresh(); call("NEUTRALSPECIALGROUND", "init", INn);
        fresh((p) => { p.timer = 49; });
        call("NEUTRALSPECIALGROUND", "main", INn); // falconpunch vfx at 50
        const punchId = (p) => {
          p.hitboxes.id[0] = p.charHitboxes.falconpunchair.id0;
        };
        fresh((p) => { punchId(p); p.timer = 51; });
        call("NEUTRALSPECIALGROUND", "main", INn); // 52 active + 3 sounds
        fresh((p) => { punchId(p); p.timer = 54; });
        call("NEUTRALSPECIALGROUND", "main", INn); // setVel + firefoxtail
        fresh((p) => { p.timer = 56; });
        call("NEUTRALSPECIALGROUND", "main", INn); // 57 off + setVel
        fresh((p) => { p.timer = 100; });
        call("NEUTRALSPECIALGROUND", "interrupt", INn); // WAIT (>99)
        fresh(); call("NEUTRALSPECIALAIR", "init", INn);
        fresh((p) => { p.timer = 10; p.phys.cVel.x = 2; p.phys.cVel.y = 1; });
        call("NEUTRALSPECIALAIR", "main", INn); // <50 friction/gravity arm
        fresh((p) => { p.timer = 49; });
        call("NEUTRALSPECIALAIR", "main", INn); // 50: setVel + vfx
        fresh((p) => { punchId(p); p.timer = 51; });
        call("NEUTRALSPECIALAIR", "main", INn); // 52 active + sounds
        fresh((p) => { p.timer = 70; });
        call("NEUTRALSPECIALAIR", "main", INn); // >=65 fastfall/airDrift
        fresh((p) => { p.timer = 100; });
        call("NEUTRALSPECIALAIR", "interrupt", INn); // FALL (>99)
        fresh(); call("NEUTRALSPECIALAIR", "land", INn); // actionState write
        // raptor boost
        fresh((p) => { p.phys.grounded = true; });
        call("SIDESPECIALGROUND", "init", INn);
        fresh((p) => { p.timer = 2; });
        call("SIDESPECIALGROUND", "main", INn); // <=4 setVel1 arm
        fresh((p) => { p.timer = 10; });
        call("SIDESPECIALGROUND", "main", INn); // 5..16 zero arm
        fresh((p) => { p.timer = 14; });
        call("SIDESPECIALGROUND", "main", INn); // 15 active + fireweakhit
        fresh((p) => { p.phys.raptorBoost = true; });
        call("SIDESPECIALGROUND", "interrupt", INn); // -> SSGHIT
        fresh((p) => { p.timer = 80; p.phys.grounded = true; });
        call("SIDESPECIALGROUND", "interrupt", INn); // WAIT (>79)
        fresh((p) => { p.timer = 80; p.phys.grounded = false; });
        call("SIDESPECIALGROUND", "interrupt", INn); // FALLSPECIAL
        fresh(); call("SIDESPECIALGROUND", "land", INn); // empty land
        fresh(); T.SIDESPECIALGROUND.onPlayerHit(3); n++; // 1-arg
        // the canEdgeCancel table-write arm LAST among SSG sweeps
        // (table value restored in finally — rule 12 net-restore):
        fresh((p) => { p.timer = 20; });
        call("SIDESPECIALGROUND", "main", INn); // >16: table write + setVel2
        fresh((p) => { p.timer = 34; });
        call("SIDESPECIALGROUND", "main", INn); // 35 turnoff (+ table write)
        fresh(); call("SIDESPECIALGROUNDHIT", "init", INn);
        const ssghId = (p) => {
          p.hitboxes.id[0] = p.charHitboxes.raptorboostgroundhit.id0;
        };
        fresh((p) => { ssghId(p); p.timer = 3; });
        call("SIDESPECIALGROUNDHIT", "main", INn); // 4 active (phys.timer
        // undefined -> else cVel 0 arm, dead-arm quirk) + firefoxtail
        fresh((p) => { ssghId(p); p.timer = 5; });
        call("SIDESPECIALGROUNDHIT", "main", INn); // frame++ + firefoxtail
        fresh((p) => { p.timer = 8; });
        call("SIDESPECIALGROUNDHIT", "main", INn); // 9 off
        fresh((p) => { p.timer = 26; });
        call("SIDESPECIALGROUNDHIT", "interrupt", INn); // WAIT (>25)
        fresh((p) => { p.phys.cVel.x = 99; });
        call("SIDESPECIALGROUNDTOAIR", "init", INn); // aerialHmaxV clamp
        fresh((p) => { p.phys.cVel.x = 0.5; });
        call("SIDESPECIALGROUNDTOAIR", "main", INn); // main -> this.init
        fresh(); call("SIDESPECIALAIR", "init", INn);
        fresh((p) => { p.timer = 16; });
        call("SIDESPECIALAIR", "main", INn); // 17 active + setVel
        fresh((p) => { p.timer = 30; });
        call("SIDESPECIALAIR", "main", INn); // >=30 cVel.y -= 0.05
        fresh((p) => { p.timer = 34; });
        call("SIDESPECIALAIR", "main", INn); // 35 off
        fresh((p) => { p.phys.raptorBoost = true; });
        call("SIDESPECIALAIR", "interrupt", INn); // -> SSAHIT
        fresh((p) => { p.timer = 80; p.phys.grounded = false; });
        call("SIDESPECIALAIR", "interrupt", INn); // FALLSPECIAL
        fresh(); call("SIDESPECIALAIR", "land", INn); // LANDINGFALLSPECIAL
        fresh(); T.SIDESPECIALAIR.onPlayerHit(3); n++;
        fresh(); call("SIDESPECIALAIRHIT", "init", INn);
        const ssahId = (p) => {
          p.hitboxes.id[0] = p.charHitboxes.raptorboostairhit.id0;
        };
        fresh((p) => { ssahId(p); p.timer = 3; });
        call("SIDESPECIALAIRHIT", "main", INn); // 4 active + firefoxtail
        fresh((p) => { ssahId(p); p.timer = 5; });
        call("SIDESPECIALAIRHIT", "main", INn); // frame++ + firefoxtail
        fresh((p) => { p.timer = 8; });
        call("SIDESPECIALAIRHIT", "main", INn); // 9 off
        fresh((p) => { p.timer = 46; });
        call("SIDESPECIALAIRHIT", "interrupt", INn); // FALLSPECIAL (>45)
        fresh(); call("SIDESPECIALAIRHIT", "land", INn);
        // falcon kick
        fresh((p) => { p.phys.grounded = true; });
        call("DOWNSPECIALGROUND", "init", INn); // falconkickshout
        fresh((p) => { p.timer = 13; });
        call("DOWNSPECIALGROUND", "main", INn); // 14 active + falconkick +
        // >=12 cVel (timer 14 even: no vfx)
        fresh((p) => { p.timer = 12; });
        call("DOWNSPECIALGROUND", "main", INn); // timer 13 odd: firefoxtail
        fresh((p) => { p.timer = 16; });
        call("DOWNSPECIALGROUND", "main", INn); // 17 mid swap
        fresh((p) => { p.timer = 24; });
        call("DOWNSPECIALGROUND", "main", INn); // 25 late swap
        fresh((p) => { p.timer = 32; });
        call("DOWNSPECIALGROUND", "main", INn); // 33 off
        fresh((p) => { p.timer = 40; p.phys.grounded = true; });
        call("DOWNSPECIALGROUND", "interrupt", INn); // ENDGROUND (>39)
        fresh((p) => { p.timer = 40; p.phys.grounded = false; });
        call("DOWNSPECIALGROUND", "interrupt", INn); // ENDAIR
        fresh((p) => { p.phys.face = -1; });
        T.DOWNSPECIALGROUND.onWallCollide(3, INn, "R", 0); n++;
        // ^ UPSPECIALTHROW arm: grounded=false + init (6 sweep draws,
        //   falconyes, 3 firefoxtail)
        fresh((p) => { p.phys.face = 1; });
        T.DOWNSPECIALGROUND.onWallCollide(3, INn, "L", 1); n++; // same arm
        fresh((p) => { p.phys.face = 1; });
        T.DOWNSPECIALGROUND.onWallCollide(3, INn, "R", 0); n++; // no-op arm
        fresh((p) => { p.phys.grounded = true; });
        call("DOWNSPECIALGROUNDENDGROUND", "init", INn); // land sound
        fresh((p) => { p.timer = 5; p.phys.grounded = true;
                       p.phys.cVel.x = 2; });
        call("DOWNSPECIALGROUNDENDGROUND", "main", INn); // grounded decel
        fresh((p) => { p.timer = 5; p.phys.grounded = false; });
        call("DOWNSPECIALGROUNDENDGROUND", "main", INn); // air arm
        fresh((p) => { p.timer = 31; p.phys.grounded = true; });
        call("DOWNSPECIALGROUNDENDGROUND", "interrupt", INn); // WAIT (>30)
        fresh((p) => { p.timer = 31; p.phys.grounded = false; });
        call("DOWNSPECIALGROUNDENDGROUND", "interrupt", INn); // FALL
        fresh(); call("DOWNSPECIALGROUNDENDAIR", "init", INn);
        fresh((p) => { p.timer = 3; });
        call("DOWNSPECIALGROUNDENDAIR", "main", INn); // player.timer dead
        // arms; timer<=7 else: setVelocities read
        fresh((p) => { p.timer = 10; });
        call("DOWNSPECIALGROUNDENDAIR", "main", INn); // >7 gravity arm
        fresh((p) => { p.timer = 31; p.phys.grounded = false; });
        call("DOWNSPECIALGROUNDENDAIR", "interrupt", INn); // FALL
        fresh(); call("DOWNSPECIALGROUNDENDAIR", "land", INn); // state write
        fresh(); call("DOWNSPECIALAIR", "init", INn); // falconkickshout
        fresh((p) => { p.timer = 5; });
        call("DOWNSPECIALAIR", "main", INn); // <17 pair setVel arm
        fresh((p) => { p.timer = 14; });
        call("DOWNSPECIALAIR", "main", INn); // 15 active + falconkick
        fresh((p) => { p.timer = 17; });
        call("DOWNSPECIALAIR", "main", INn); // 18 mid swap + >=17 cVel arm
        // (timer 18 even: no vfx)
        fresh((p) => { p.timer = 18; });
        call("DOWNSPECIALAIR", "main", INn); // timer 19 odd: firefoxtail
        fresh((p) => { p.timer = 25; });
        call("DOWNSPECIALAIR", "main", INn); // 26 late swap
        fresh((p) => { p.timer = 28; });
        call("DOWNSPECIALAIR", "main", INn); // 29 frame++ tail
        fresh((p) => { p.timer = 30; });
        call("DOWNSPECIALAIR", "interrupt", INn); // ENDAIR (>29)
        fresh(); call("DOWNSPECIALAIR", "land", INn); // ENDGROUND init
        fresh(); call("DOWNSPECIALAIRENDGROUND", "init", INn); // groundBounce
        fresh((p) => { p.phys.cVel.x = 2; });
        call("DOWNSPECIALAIRENDGROUND", "main", INn); // 1 active + decel
        fresh((p) => { p.timer = 2; });
        call("DOWNSPECIALAIRENDGROUND", "main", INn); // 3 off
        fresh((p) => { p.timer = 46; });
        call("DOWNSPECIALAIRENDGROUND", "interrupt", INn); // WAIT (>45)
        fresh(); call("DOWNSPECIALAIRENDAIR", "init", INn);
        fresh((p) => { p.phys.cVel.x = 1; p.phys.cVel.y = 1; });
        call("DOWNSPECIALAIRENDAIR", "main", INn); // gravity/friction
        fresh((p) => { p.timer = 30; });
        call("DOWNSPECIALAIRENDAIR", "interrupt", INn); // FALL (>29)
        fresh(); call("DOWNSPECIALAIRENDAIR", "land", INn); // ENDGROUND
        // falcon dive
        fresh(); call("UPSPECIAL", "init", INn); // groundBounce vfx
        fresh((p) => { p.timer = 5; });
        call("UPSPECIAL", "main", INn); // pair setVel arms + drift clamps
        fresh((p) => { p.timer = 12; p.phys.grounded = true; });
        call("UPSPECIAL", "main", INbackStick); // 13: grounded=false +
        // face flip + active
        fresh((p) => { p.timer = 13; });
        call("UPSPECIAL", "main", INn); // 14 falcondive2 swap + sound
        fresh((p) => { p.timer = 20; p.phys.cVel.x = 5; });
        call("UPSPECIAL", "main", INstick); // drift + 0.952 clamp arms
        fresh((p) => { p.timer = 33; });
        call("UPSPECIAL", "main", INn); // 34 off
        fresh((p) => { p.timer = 65; });
        call("UPSPECIAL", "interrupt", INn); // FALLSPECIAL (>64)
        fresh((p) => { grab(p); });
        call("UPSPECIAL", "interrupt", INn); // grab arm: pos writes +
        // UPSPECIALCATCH chain
        fresh((p) => { p.phys.cVel.y = -1; });
        call("UPSPECIAL", "land", INn); // LANDINGFALLSPECIAL guard true
        fresh((p) => { p.phys.cVel.y = 5; p.phys.kVel.y = 1;
                       p.phys.ECBp[0] = { x: 0, y: 10 };
                       p.phys.ECB1[0] = { x: 0, y: 5 };
                       p.phys.posPrev.y = -50; });
        call("UPSPECIAL", "land", INn); // guard all-false: NO init arm
        fresh(); call("UPSPECIALCATCH", "init", INn);
        fresh((p) => { p.timer = 1; });
        call("UPSPECIALCATCH", "main", INn); // 2: active + 6 draws + 3 vfx
        fresh((p) => { p.timer = 3; });
        call("UPSPECIALCATCH", "main", INn); // 4 off
        fresh((p) => { grab(p); p.timer = 17; });
        call("UPSPECIALCATCH", "interrupt", INn); // >16 grabbed arm:
        // falcondivethrow + hq push + UPSPECIALTHROW (draws)
        fresh((p) => { p.timer = 17; });
        call("UPSPECIALCATCH", "interrupt", INn); // >16 ungrabbed arm
        fresh((p) => { p.timer = 5; });
        call("UPSPECIALCATCH", "interrupt", INn); // grabbing===-1 bare ret
        fresh((p) => { p.phys.grabbing = 3; p.timer = 5; });
        call("UPSPECIALCATCH", "interrupt", INn); // grabbedBy mismatch ->
        // "exiting" UPSPECIALTHROW arm
        fresh(); call("UPSPECIALCATCH", "land", INn); // empty land
        fresh(); call("UPSPECIALTHROW", "init", INn); // 6 draws + falconyes
        fresh((p) => { p.timer = 5; });
        call("UPSPECIALTHROW", "main", INn); // pair setVel
        fresh((p) => { p.timer = 61; });
        call("UPSPECIALTHROW", "interrupt", INn); // FALL (>60)
        // throws (falcon keeps fox's grabbing===-1 INIT guard — sweepable)
        fresh(); call("THROWUP", "init", INn); // init guard arm
        fresh(grab); call("THROWUP", "init", INn); // THROWNFALCONUP chain
        fresh((p) => { grab(p); p.phys.releaseFrame = 34; p.timer = 10.9; });
        call("THROWUP", "main", INn); // throwupextra crossing 11
        fresh((p) => { grab(p); p.phys.releaseFrame = 8; p.timer = 14.5; });
        call("THROWUP", "main", INn); // hq push floor-crossing 15
        fresh((p) => { p.phys.grabbing = 3; p.phys.releaseFrame = 10;
                       p.timer = 1; });
        call("THROWUP", "interrupt", INn); // CATCHCUT (grabbedBy mismatch)
        fresh((p) => { p.phys.grabbing = 3; p.timer = 44; });
        call("THROWUP", "interrupt", INn); // >43 WAIT arm
        fresh((p) => { p.phys.grabbing = -1; p.timer = 1; });
        call("THROWUP", "interrupt", INn); // bare-return arm (undef)
        fresh(); call("THROWBACK", "init", INn); // init guard arm
        fresh(grab); call("THROWBACK", "init", INn); // randomShout + 1-arg
        // THROWNFALCONBACK dispatch
        fresh((p) => { grab(p); p.phys.releaseFrame = 34; p.timer = 11.9; });
        call("THROWBACK", "main", INn); // throwbackextra crossing 12
        fresh((p) => { grab(p); p.phys.releaseFrame = 34; p.timer = 19.9; });
        call("THROWBACK", "main", INn); // >=20 off + floor hq push
        fresh((p) => { p.phys.grabbing = 3; p.timer = 50; });
        call("THROWBACK", "interrupt", INn); // >49 WAIT arm
        fresh((p) => { p.phys.grabbing = -1; p.timer = 1; });
        call("THROWBACK", "interrupt", INn); // bare-return arm
        fresh(); call("THROWDOWN", "init", INn); // init guard arm
        fresh(grab); call("THROWDOWN", "init", INn); // randomShout + 1-arg
        fresh((p) => { grab(p); p.phys.releaseFrame = 33; p.timer = 15.9; });
        call("THROWDOWN", "main", INn); // hq push (true flag) crossing 16
        fresh((p) => { p.phys.grabbing = 3; p.timer = 32; });
        call("THROWDOWN", "interrupt", INn); // >31 WAIT arm
        fresh(); call("THROWFORWARD", "init", INn); // init guard arm
        fresh(grab); call("THROWFORWARD", "init", INn); // randomShout
        fresh((p) => { grab(p); p.phys.releaseFrame = 34; p.timer = 10.9; });
        call("THROWFORWARD", "main", INn); // throwforwardextra crossing 11
        fresh((p) => { grab(p); p.phys.releaseFrame = 18; p.timer = 17.5; });
        call("THROWFORWARD", "main", INn); // >=18 off + floor hq push
        fresh((p) => { p.phys.grabbing = 3; p.timer = 40; });
        call("THROWFORWARD", "interrupt", INn); // >39 WAIT arm
        // THROWN* inits (self grabbedBy) — fox-family guards/clamps
        const THROWN = [
          "THROWNPUFFFORWARD", "THROWNPUFFDOWN", "THROWNPUFFBACK",
          "THROWNPUFFUP", "THROWNMARTHUP", "THROWNMARTHDOWN",
          "THROWNMARTHBACK", "THROWNMARTHFORWARD", "THROWNFOXUP",
          "THROWNFOXDOWN", "THROWNFOXBACK", "THROWNFOXFORWARD",
          "THROWNFALCOUP", "THROWNFALCODOWN", "THROWNFALCOBACK",
          "THROWNFALCOFORWARD", "THROWNFALCONUP", "THROWNFALCONDOWN",
          "THROWNFALCONBACK", "THROWNFALCONFORWARD",
        ];
        for (const nm of THROWN) {
          fresh((p) => { p.phys.grabbedBy = 3; });
          call(nm, "init", INn);
        }
        // grabbedBy === -1 init guard — GUARDED family only
        // (THROWN{PUFF,MARTH,FOX}*; the THROWN{FALCO,FALCON}* family has
        // NO -1 guards and NO clamps: player[-1] throws upstream, unswept)
        fresh(); call("THROWNMARTHUP", "init", INn);
        // main guard + clamp arms
        fresh((p) => { p.phys.grabbedBy = -1; p.timer = 1; });
        call("THROWNMARTHUP", "main", INn); // main -1 guard
        fresh((p) => { p.phys.grabbedBy = 3; p.timer = 2; });
        call("THROWNFOXUP", "main", INn); // in-range pos arm
        fresh((p) => { p.phys.grabbedBy = 3; p.timer = 9; });
        call("THROWNFOXUP", "main", INn); // clamp arm (offset len 9)
        fresh((p) => { p.phys.grabbedBy = 3; p.timer = 2; });
        call("THROWNMARTHDOWN", "main", INn);
        fresh((p) => { p.phys.grabbedBy = 3; p.timer = 27; });
        call("THROWNPUFFBACK", "main", INn); // clamp (len 27) + face*-1
        fresh((p) => { p.phys.grabbedBy = 3; p.timer = 2; });
        call("THROWNFALCOBACK", "main", INn);
        // grabbedBy < p (timer = -1) arm: inject slot 2
        M.playerType[2] = 0;
        M.player[2] = new PJ.playerObject(4, [30, 20], 1);
        fresh((p) => { p.phys.grabbedBy = 2; });
        call("THROWNMARTHUP", "init", INn);
        fresh((p) => { p.phys.grabbedBy = 2; });
        call("THROWNFOXDOWN", "init", INn);
        M.player[2] = savedP2;
        M.playerType[2] = savedT2;
        // cliffs (default pre-match stage carries a real ledge list)
        const cliff = (t) => (p) => { p.phys.onLedge = 0; if (t) p.timer = t; };
        for (const nm of ["CLIFFGETUPQUICK", "CLIFFGETUPSLOW",
                          "CLIFFESCAPEQUICK", "CLIFFESCAPESLOW",
                          "CLIFFJUMPQUICK", "CLIFFJUMPSLOW",
                          "CLIFFATTACKQUICK", "CLIFFATTACKSLOW"]) {
          fresh(cliff(0)); call(nm, "init", INn);
        }
        fresh(cliff(13)); call("CLIFFGETUPQUICK", "main", INn); // offset arm
        fresh(cliff(19)); call("CLIFFGETUPQUICK", "main", INn); // grounding
        fresh(cliff(21)); call("CLIFFGETUPQUICK", "main", INn); // setVel arm
        fresh(cliff(33)); call("CLIFFGETUPQUICK", "interrupt", INn); // regrab T
        fresh(cliff(12)); call("CLIFFESCAPEQUICK", "main", INn);
        fresh(cliff(17)); call("CLIFFESCAPEQUICK", "main", INn); // grounding
        fresh(cliff(49)); call("CLIFFESCAPEQUICK", "interrupt", INn);
        fresh(cliff(37)); call("CLIFFGETUPSLOW", "main", INn); // grounding
        fresh(cliff(59)); call("CLIFFGETUPSLOW", "interrupt", INn);
        fresh(cliff(37)); call("CLIFFESCAPESLOW", "main", INn); // grounding
        fresh(cliff(79)); call("CLIFFESCAPESLOW", "interrupt", INn);
        fresh(cliff(11)); call("CLIFFJUMPQUICK", "main", INn); // cVel at 12
        fresh(cliff(13)); call("CLIFFJUMPQUICK", "main", INn); // drift arm
        fresh(cliff(43)); call("CLIFFJUMPQUICK", "interrupt", INn); // FALL
        fresh(cliff(18)); call("CLIFFJUMPSLOW", "main", INn); // cVel at 19
        fresh(cliff(54)); call("CLIFFJUMPSLOW", "interrupt", INn);
        fresh(cliff(21)); call("CLIFFATTACKQUICK", "main", INn); // grounding
        fresh(cliff(23)); call("CLIFFATTACKQUICK", "main", INn); // active 24
        fresh(cliff(26)); call("CLIFFATTACKQUICK", "main", INn); // frame++
        fresh(cliff(29)); call("CLIFFATTACKQUICK", "main", INn); // 30 off
        fresh(cliff(32)); call("CLIFFATTACKQUICK", "main", INn); // <33 off
        // of setVel window (timer 33: neither offset nor setVel arm)
        fresh(cliff(55)); call("CLIFFATTACKQUICK", "interrupt", INn);
        fresh(cliff(28)); call("CLIFFATTACKSLOW", "main", INn); // grounding
        fresh(cliff(36)); call("CLIFFATTACKSLOW", "main", INn); // active 37
        fresh(cliff(40)); call("CLIFFATTACKSLOW", "main", INn); // 41 off
        fresh(cliff(69)); call("CLIFFATTACKSLOW", "interrupt", INn);
        // grab family + downattack + appeal
        fresh(); call("GRAB", "init", INn);
        fresh((p) => { p.timer = 6; }); call("GRAB", "main", INn); // 7
        fresh((p) => { p.timer = 8; }); call("GRAB", "main", INn); // 9 off
        fresh((p) => { p.timer = 31; });
        call("GRAB", "interrupt", INn); // WAIT (>30)
        fresh(); call("CATCHATTACK", "init", INn);
        fresh((p) => { p.timer = 9; });
        call("CATCHATTACK", "main", INn); // 10 active
        fresh((p) => { p.timer = 10; });
        call("CATCHATTACK", "main", INn); // 11 off
        fresh((p) => { p.timer = 25; });
        call("CATCHATTACK", "interrupt", INn); // CATCHWAIT (>24)
        fresh(); call("DOWNATTACK", "init", INn);
        fresh((p) => { p.timer = 0; });
        call("DOWNATTACK", "main", INn); // 1 intangibleTimer
        fresh((p) => { p.timer = 18; });
        call("DOWNATTACK", "main", INn); // 19 active x4
        fresh((p) => { p.timer = 20; });
        call("DOWNATTACK", "main", INn); // 21 off
        fresh((p) => { p.timer = 27; });
        call("DOWNATTACK", "main", INn); // 28 downattack2 swap
        fresh((p) => { p.timer = 29; });
        call("DOWNATTACK", "main", INn); // 30 off
        fresh((p) => { p.timer = 50; });
        call("DOWNATTACK", "interrupt", INn); // WAIT (>49)
        fresh(); call("APPEAL", "init", INn); // falcontaunt
        fresh((p) => { p.timer = 2; }); call("APPEAL", "main", INn);
        fresh((p) => { p.timer = 61; });
        call("APPEAL", "interrupt", INn); // WAIT (>60)
      } finally {
        Math.random = savedRandom;
        M.player[3] = savedP3;
        M.playerType[3] = savedT3;
        M.player[2] = savedP2;
        M.playerType[2] = savedT2;
        M.characterSelections[3] = savedCS3;
        SSG.canEdgeCancel = savedCanEdgeCancel; // the scalar table write
        // net-restore the article/hq globals the sweep touched (rule 12)
        ART.aArticles.splice(savedArticles);
        HD.hitQueue.splice(savedHq);
      }
      return n;
    },
  };
})();
