// spec-moves-fox.js — M2 task 8 capture spec: characters/fox moves (the
// per-char move table, first per-char cluster). Registers
// window.__capSpecs["moves-fox"]. Injected AFTER capturelib.js.
// See FORMAT.md "The moves-fox spec".
//
// BOUNDARY: every function property of every FOX-ORIGIN actionStates entry
// (fn identity vs the characters/fox/moves module index — setupActionStates
// deep-copies data, copies FUNCTIONS by reference; rule 15's instrument
// extended per its recipe). Fox-origin entries exist ONLY on table 2 (fox):
// 61 module keys — 49 moves x {init,main,interrupt} + 11 x {+land}
// (ATTACKAIR*x5, SIDESPECIALGROUND/AIR, UPSPECIALCHARGE/LAUNCH,
// FIREFOXBOUNCE, DOWNSPECIALAIR) + UPSPECIAL {init} = 192 fns, plus the 2
// article init boundaries (LASER/ILLUSION) = 194 wrapped.
//
// GOLDENS (PROVISIONAL, auto-adopted): g01/g03/g08 — the fox carriers
// (g04/g06 field no fox slot; the g01/g04/g06 convention's purpose is live
// coverage). g08 is the first CPU-golden capture: the AI plane is JS-side
// (M2 AI policy), its seeded draws land as standalone Math.random records
// and chain-verify like every other draw.
//
// RECORDS:
// - "move" (5-field, mutation-captured): a fox-origin phase entered with NO
//   attributing frame on the stack (callers: physics' state drive +
//   land dispatches, hitdet CATCHATTACK windows, shared-move interrupt
//   chains at top level, other chars' THROW* windows dispatching fox's
//   THROWN<CHAR>* — chain-safe: silent windows' own draws are standalone
//   records pushed at draw time). Nested fox->fox and fox->shared calls are
//   TRANSPARENT (the C tree calls its own bodies — shared bodies are task
//   7's, linked in). args = [phase, name, [slot], inputs|null, pre] — fox
//   phases carry no extras; THROWN{...BACK,...DOWN} inits arrive 1-arg
//   (throwers call .init(grabbing) without input) and record inputs null.
// - "mdispatch" (5-field seam): a NON-fox, NON-shared move fn entered while
//   the top frame is the attributing fox frame (the victim's THROWNFOX*
//   per-char entries from fox THROW* chains). args = [phase, name, [slot]],
//   post = {alias(4), hq, players, rng} (rule-14 instrument, task 7 form).
//   1-arg (p) and 2-arg (p, input) dispatch sites both occur upstream
//   (THROWBACK/THROWDOWN omit input).
// - "article" (4-field seam FIFO): articles.{LASER,ILLUSION}.init(options)
//   entered under the attributing fox frame — the task-13 oracle boundary
//   (article state lives JS-side; inits only READ player state, mutate
//   only the article queues, draw no RNG). args = [name, options], ret
//   echoed (undefined). Article inits outside fox scope (falco lasers,
//   executeArticles' mains) are NOT this task's records.
// - RNG/sounds/vfx channels: the task-7 discipline verbatim (owner draws in
//   move posts, window draws in seam posts, everything else standalone
//   "Math.random"; snd incl. ".stop" tokens; vfx = drawVfx names,
//   circleDust = 4 owner draws).
// - "mvData" (frame-0 record, finalCheck drift-guarded): the task-7 dump
//   (state->name, sharedOrigin, shared data patches, actionSounds,
//   palettes) EXTENDED with fox: {origin: {state->bool} (fox-origin,
//   measured by fn identity), data: {state -> every own enumerable
//   ARRAY-valued non-function prop}} — ATTACKDASH/APPEAL/FIREFOXBOUNCE/
//   THROWFORWARD setVelocities*, THROWN* offset(+offsetVel dead data),
//   CLIFF* offset/setVelocities, canGrabLedge pairs (CLIFF*'s
//   `this.canGrabLedge = false` onLedge===-1 arm would DRIFT this dump and
//   hard-fail finalCheck — the C traps at that exact site). The C fox
//   registry + data plane are built FROM this dump (rule 15), never from
//   assumed file layout.
(() => {
  const EXCLUDE = ["charAttributes", "charHitboxes", "percentShake"];
  const SND_KEYS = ["CLIFFCATCH", "DEAD", "ESCAPEAIR", "ESCAPEB", "ESCAPEF",
                    "ESCAPEN", "FURAFURA", "GUARDOFF", "GUARDON", "JUMP",
                    "JUMPAERIAL", "OTTOTTOWAIT", "TECH"];
  const SETVEL_KEYS = ["DOWNSTANDB", "DOWNSTANDF", "ESCAPEB", "ESCAPEF",
                       "TECHB", "TECHF"];

  window.__capSpecs["moves-fox"] = {
    expectWrapped: 194,

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
      // the fox module index (characters/fox/moves/index.js) — found by fn
      // IDENTITY against the live table (rule 15's measurement), excluding
      // characters/fox/index.js (whose default has .moves, not .JAB1) and
      // falco's index (distinct fns; falco also lacks UPSPECIALLAUNCH):
      const foxTbl = AS.actionStates[2];
      if (!foxTbl || !foxTbl.JAB1) {
        throw new Error("moves-fox: actionStates[2].JAB1 missing");
      }
      const FOXMOVES = find((ex) =>
        ex.default && typeof ex.default === "object" &&
        ex.default.JAB1 && typeof ex.default.JAB1 === "object" &&
        typeof ex.default.JAB1.init === "function" &&
        ex.default.JAB1.init === foxTbl.JAB1.init &&
        ex.default.UPSPECIALLAUNCH && ex.default.THROWNFOXUP &&
        !ex.default.WAIT && !ex.default.moves, "foxMovesIndex").default;
      const foxKeys = Object.keys(FOXMOVES);
      if (foxKeys.length !== 61) {
        throw new Error("moves-fox: expected 61 fox index keys, got " +
                        foxKeys.length);
      }

      // --- owner stack (task-7 semantics verbatim) -----------------------
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
        throw new Error("moves-fox: expected 180 Howl sounds, wrapped " +
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

      // --- article seam (task-13 boundary; FIFO like mdispatch) ------------
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

      // --- the fox-move boundary wrapper -------------------------------------
      const wrapFoxFn = (entry, k) => {
        const orig = entry[k];
        const name = entry.name;
        entry[k] = function () {
          if (inScope > 0) return orig.apply(this, arguments);
          const args = Array.prototype.slice.call(arguments);
          // fox phases are (p[, input]) — no extras anywhere in the set
          if (args.length > 2) {
            throw new Error("moves-fox: " + name + "." + k +
                            " called with " + args.length + " args");
          }
          let inCanon = "null"; // 1-arg THROWN*{BACK,DOWN} dispatch sites
          if (args.length === 2) {
            if (!isGodInput(args[1])) {
              throw new Error("moves-fox: " + name + "." + k +
                              " arg 1 is not the god input");
            }
            inCanon = inputsCanon(args[1]);
          }
          const argsCanon = "[" + JSON.stringify(k) + "," +
              JSON.stringify(name) + "," +
              ctx.canon([args[0]]) + "," +
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

      // --- the non-fox per-char seam logger ------------------------------------
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
            throw new Error("moves-fox: seam " + name + "." + k +
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

      // --- classify + wrap every actionStates entry ------------------------------
      // shared entries stay UNWRAPPED: at top level they are silent
      // (chain-safe — their draws land as standalone records); inside a fox
      // record they are the transparent nested C tree (task 7's bodies).
      const seen = new WeakSet();
      const sharedOrigin = [{}, {}, {}, {}, {}];
      const foxOrigin = [{}, {}, {}, {}, {}];
      const nameMap = [{}, {}, {}, {}, {}];
      const perTableFox = [0, 0, 0, 0, 0];
      for (let c = 0; c < AS.actionStates.length; c++) {
        const tbl = AS.actionStates[c];
        if (!tbl) continue;
        for (const st of Object.keys(tbl)) {
          const entry = tbl[st];
          if (!entry || typeof entry !== "object") continue;
          if (typeof entry.name !== "string") {
            throw new Error("moves-fox: move object without a name");
          }
          nameMap[c][st] = entry.name;
          const sh = SHARED[st] !== undefined &&
                     typeof SHARED[st].init === "function" &&
                     entry.init === SHARED[st].init;
          sharedOrigin[c][st] = sh;
          const fx = !sh && FOXMOVES[st] !== undefined &&
                     typeof FOXMOVES[st].init === "function" &&
                     entry.init === FOXMOVES[st].init;
          foxOrigin[c][st] = fx;
          if (seen.has(entry)) {
            throw new Error("moves-fox: entry object shared across tables");
          }
          seen.add(entry);
          for (const k of Object.keys(entry)) {
            if (typeof entry[k] !== "function") continue;
            if (fx) {
              wrapFoxFn(entry, k);
              perTableFox[c]++;
            } else if (!sh) {
              wrapSeamFn(entry, k);
            }
          }
        }
      }
      // measured composition: fox-origin only on table 2, all 61 states,
      // 192 fns (49x3 + 11x4 + UPSPECIAL's lone init)
      for (let c = 0; c < 5; c++) {
        const want = c === 2 ? 192 : 0;
        if (perTableFox[c] !== want) {
          throw new Error("moves-fox: table " + c + " wrapped " +
                          perTableFox[c] + " fox fns, expected " + want);
        }
      }
      for (const st of foxKeys) {
        if (!foxOrigin[2][st]) {
          throw new Error("moves-fox: fox table state not fox-origin: " + st);
        }
        if (nameMap[2][st] !== st) {
          throw new Error("moves-fox: fox state key != move name: " + st);
        }
      }
      ctx.declare("move");
      ctx.declare("mdispatch");

      // --- mvData: task-7 dump + the fox extension (rule 15) ---------------------
      const mvDataCanon = () => {
        const dump = { chars: {}, fox: { data: {}, origin: foxOrigin[2] },
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
        // fox data plane: every own enumerable ARRAY-valued non-function
        // prop of every fox-origin entry (executed data, never retyped) —
        // includes canGrabLedge, whose CLIFF* runtime write would drift
        // this dump and hard-fail finalCheck.
        for (const st of foxKeys) {
          const entry = AS.actionStates[2][st];
          const d = {};
          let any = false;
          for (const k of Object.keys(entry)) {
            if (Array.isArray(entry[k])) { d[k] = entry[k]; any = true; }
          }
          if (any) dump.fox.data[st] = d;
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
        throw new Error("moves-fox: mvData drifted during the match");
      }
      return 1;
    },

    // Synthetic-domain sweep (rules 11/12): the zero-live fox moves with a
    // reachable future domain. A REAL upstream playerObject(2, [10,20], 1)
    // is injected into inactive slot 3 with characterSelections[3] set to
    // 2 (both restored — the hitdet sweep's injection precedent); a second
    // player in slot 2 covers the THROWN* grabbedBy<p (timer=-1) arm.
    // Math.random is swapped for the sweep mulberry32 (0x0badf00d);
    // article spawns and hitQueue pushes are spliced back out after the
    // sweep (net-restore purity, guarded by x2 byte-stability +
    // STREAM MATCH — a leaked article would enter the checksummed
    // `articles` key and fail the stream). CLIFF* run against the default
    // pre-match stage (its ledge list is real). Unswept (documented):
    // the CLIFF* onLedge===-1 canGrabLedge table-write arm (drifts the
    // finalCheck-guarded mvData dump; C traps at the site), the
    // THROWNFALCO*/FALCON* unclamped offset overruns (upstream throws),
    // and interrupt-tail WALK arms shadowed by checkForTilts.
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
      const INyBack = mk({ 0: { y: true, lsX: -0.5 } });
      const INaUp = mk({ 0: { a: true, lsY: 0.8 }, 1: { lsY: 0.8 },
                         2: { lsY: 0.8 }, 3: { lsY: 0.8 } });
      const INb = mk({ 0: { b: true } });
      const INbDown = mk({ 0: { b: true, lsY: -1 } });
      const INlA = mk({ 0: { lA: 1 } });
      const INstick = mk({ 0: { lsX: 0.5, lsY: 0.5 } });
      const INdown = mk({ 0: { lsY: -1 } });
      const INbackStick = mk({ 0: { lsX: -0.35 } });
      const INplatDrop = mk({ 0: { lsY: -1 } }); // i6.lsY = 0 satisfies >= 0

      const savedP3 = M.player[3];
      const savedT3 = M.playerType[3];
      const savedP2 = M.player[2];
      const savedT2 = M.playerType[2];
      const savedCS3 = M.characterSelections[3];
      const savedRandom = Math.random;
      const savedArticles = ART.aArticles.length;
      const savedHq = HD.hitQueue.length;
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
        M.characterSelections[3] = 2; // fox
        const fresh = (mut) => {
          M.player[3] = new PJ.playerObject(2, [10, 20], 1);
          if (mut) mut(M.player[3]);
        };
        const T = AS.actionStates[2]; // fox's table
        const call = (name, phase, IN) => {
          T[name][phase](3, IN);
          n++;
        };
        const grab = (p3) => {
          p3.phys.grabbing = 3;
          p3.phys.grabbedBy = 3;
        };

        // jabs
        fresh(); call("JAB2", "init", INn);
        fresh((p) => { p.phys.jabCombo = true; p.timer = 5; });
        call("JAB2", "main", INn); // -> JAB3 chain
        fresh(); call("JAB3", "init", INn);
        fresh((p) => { p.timer = 8; }); call("JAB3", "main", INn); // 9%7==2
        fresh((p) => { p.timer = 42; p.phys.jabCombo = true; });
        call("JAB3", "main", INn); // combo restart arm
        // tilts / smashes
        fresh(); call("UPTILT", "init", INn);
        fresh((p) => { p.timer = 28; });
        call("DOWNTILT", "interrupt", INbackStick); // TILTTURN tail arm
        fresh(); call("FORWARDSMASH", "init", INn);
        fresh((p) => { p.timer = 7; p.phys.chargeFrames = 4; });
        call("FORWARDSMASH", "main", INa); // charge + smashcharge at 5
        fresh((p) => { p.timer = 7; p.phys.chargeFrames = 59; });
        call("FORWARDSMASH", "main", INa); // charge release at 60
        fresh((p) => { p.timer = 11; });
        call("FORWARDSMASH", "main", INn); // randomShout at 12 (sweep draw)
        fresh(); call("UPSMASH", "init", INn);
        fresh((p) => { p.timer = 6; }); call("UPSMASH", "main", INn);
        fresh((p) => { p.timer = 9; }); call("UPSMASH", "main", INn);
        fresh(); call("DOWNSMASH", "init", INn);
        fresh((p) => { p.timer = 5; }); call("DOWNSMASH", "main", INn);
        fresh(); call("ATTACKDASH", "init", INn);
        fresh((p) => { p.timer = 3; }); call("ATTACKDASH", "main", INn);
        fresh((p) => { p.timer = 7; }); call("ATTACKDASH", "main", INn);
        fresh((p) => { p.timer = 1; p.phys.cVel.x = 99; });
        call("ATTACKDASH", "interrupt", INlA); // GRAB arm + dMaxV clamp
        // aerials
        fresh(); call("ATTACKAIRU", "init", INn);
        fresh((p) => { p.timer = 7; }); call("ATTACKAIRU", "main", INn);
        fresh((p) => { p.timer = 10; }); call("ATTACKAIRU", "main", INn);
        fresh((p) => { p.phys.autoCancel = true; });
        call("ATTACKAIRU", "land", INn); // LANDING (circleDust: 4 draws)
        fresh((p) => { p.phys.autoCancel = false; });
        call("ATTACKAIRU", "land", INn); // LANDINGATTACKAIRU
        fresh((p) => { p.phys.autoCancel = false; });
        call("ATTACKAIRD", "land", INn);
        fresh((p) => { p.timer = 4; });
        call("ATTACKAIRN", "main", INn); // hitboxes.frames++ NaN quirk
        fresh((p) => { p.IASATimer = 1; p.timer = 5; });
        call("ATTACKAIRN", "interrupt", INy); // IASA -> JUMPAERIALF
        fresh((p) => { p.IASATimer = 1; p.timer = 5; });
        call("ATTACKAIRN", "interrupt", INyBack); // IASA -> JUMPAERIALB
        fresh((p) => { p.IASATimer = 1; p.timer = 5; });
        call("ATTACKAIRN", "interrupt", INaUp); // IASA -> FOXMOVES aerial
        // lasers
        fresh(); call("NEUTRALSPECIALAIR", "init", INn);
        fresh((p) => { p.timer = 9; });
        call("NEUTRALSPECIALAIR", "main", INn); // LASER article at 10
        fresh((p) => { p.timer = 14; p.phys.laserCombo = true; });
        call("NEUTRALSPECIALAIR", "main", INn); // combo arm at 15
        fresh((p) => { p.timer = 16; p.phys.laserCombo = true; });
        call("NEUTRALSPECIALGROUND", "main", INn); // combo arm at 17
        // illusion
        fresh((p) => { p.phys.grounded = true; });
        call("SIDESPECIALGROUND", "init", INn);
        fresh((p) => { p.phys.grounded = true; p.timer = 19; });
        call("SIDESPECIALGROUND", "main", INn); // foxillusion sounds at 20
        fresh((p) => { p.phys.grounded = true; p.timer = 20; });
        call("SIDESPECIALGROUND", "main", INn); // ILLUSION type 1 at 21
        fresh((p) => { p.phys.grounded = true; p.timer = 20; });
        call("SIDESPECIALGROUND", "main", INb); // + b-skip arm
        fresh((p) => { p.phys.grounded = false; p.timer = 30; });
        call("SIDESPECIALGROUND", "main", INn); // airborne cross-call arm
        fresh((p) => { p.phys.grounded = true; p.timer = 64; });
        call("SIDESPECIALGROUND", "interrupt", INn); // WAIT
        fresh((p) => { p.phys.grounded = false; p.timer = 64; });
        call("SIDESPECIALGROUND", "interrupt", INn); // FALLSPECIAL
        fresh((p) => { p.phys.grounded = true; });
        call("SIDESPECIALGROUND", "land", INn); // empty land
        // SIDESPECIALAIR.init's grounded arm is UNSWEEPABLE: main's
        // grounded arm re-enters this.main with grounded still true —
        // upstream itself stack-overflows (never reached live: the air
        // side-B is only dispatched airborne). Documented, not swept.
        fresh((p) => { p.phys.grounded = false; p.phys.cVel.x = 2; });
        call("SIDESPECIALAIR", "init", INn); // airborne cVel arm
        fresh((p) => { p.phys.grounded = false; p.timer = 20; });
        call("SIDESPECIALAIR", "main", INn); // ILLUSION type 0 at 21
        fresh((p) => { p.phys.grounded = false; p.timer = 15;
                       p.phys.cVel.x = 1; });
        call("SIDESPECIALAIR", "main", INn); // decay arms at 16
        fresh((p) => { p.timer = 25; });
        call("SIDESPECIALAIR", "land", INn); // LANDINGFALLSPECIAL
        fresh((p) => { p.timer = 5; });
        call("SIDESPECIALAIR", "land", INn); // actionState only
        // firefox
        fresh(); call("UPSPECIAL", "init", INn); // -> UPSPECIALCHARGE chain
        fresh((p) => { p.timer = 41; });
        call("UPSPECIALCHARGE", "main", INstick); // atan2 at 42
        fresh((p) => { p.timer = 41; p.phys.grounded = true;
                       p.phys.onSurface = [0, 0]; });
        call("UPSPECIALCHARGE", "main", INdown); // +2PI + clamp arms
        fresh((p) => { p.timer = 21; });
        call("UPSPECIALCHARGE", "main", INn); // hitbox flicker even arm
        fresh((p) => { p.timer = 43; });
        call("UPSPECIALCHARGE", "interrupt", INn); // LAUNCH + undef return
        fresh(); call("UPSPECIALCHARGE", "land", INn); // empty land
        fresh((p) => { p.phys.upbAngleMultiplier = 3; });
        call("UPSPECIALLAUNCH", "init", INn); // face = -1 arm
        fresh((p) => { p.phys.upbAngleMultiplier = Math.PI / 2; });
        call("UPSPECIALLAUNCH", "init", INn); // angle === PI/2 arm
        fresh((p) => { p.timer = 5; p.phys.upbAngleMultiplier = 1; });
        call("UPSPECIALLAUNCH", "main", INn); // sin/cos decay arm
        fresh((p) => { p.timer = 30; });
        call("UPSPECIALLAUNCH", "main", INn); // turnOff + rotation reset
        fresh((p) => { p.timer = 35; p.phys.grounded = false; });
        call("UPSPECIALLAUNCH", "main", INn); // fastfall/airDrift arm
        fresh((p) => { p.timer = 10; });
        call("UPSPECIALLAUNCH", "land", INn); // FIREFOXBOUNCE bounce
        fresh((p) => { p.timer = 40; });
        call("UPSPECIALLAUNCH", "land", INn); // impactLand
        fresh((p) => { p.timer = 51; p.phys.grounded = true; });
        call("UPSPECIALLAUNCH", "interrupt", INn); // WAIT
        fresh((p) => { p.timer = 51; p.phys.grounded = false; });
        call("UPSPECIALLAUNCH", "interrupt", INn); // FALLSPECIAL
        fresh(); call("FIREFOXBOUNCE", "init", INn);
        fresh((p) => { p.phys.cVel.x = 2; });
        call("FIREFOXBOUNCE", "main", INn); // decel arm
        fresh((p) => { p.timer = 15; p.phys.grounded = true; });
        call("FIREFOXBOUNCE", "interrupt", INn);
        fresh((p) => { p.timer = 15; p.phys.grounded = false; });
        call("FIREFOXBOUNCE", "interrupt", INn);
        fresh(); call("FIREFOXBOUNCE", "land", INn); // LANDING
        // shine
        fresh((p) => { p.phys.grounded = true; });
        call("DOWNSPECIALGROUND", "init", INn);
        fresh((p) => { p.phys.grounded = true; p.timer = 1; });
        call("DOWNSPECIALGROUND", "main", INn); // reflector swap at 2
        fresh((p) => { p.phys.grounded = true; p.timer = 3;
                       p.shineLoop = 6; });
        call("DOWNSPECIALGROUND", "main", INn); // shineLoop reset + active
        fresh((p) => { p.phys.grounded = true; p.timer = 34; });
        call("DOWNSPECIALGROUND", "main", INn); // face flip at 35
        fresh((p) => { p.phys.grounded = true; p.timer = 10;
                       p.phys.inShine = 25; });
        call("DOWNSPECIALGROUND", "main", INn); // !b release -> timer 36
        fresh((p) => { p.phys.grounded = true; p.timer = 31;
                       p.phys.inShine = 25; });
        call("DOWNSPECIALGROUND", "main", INb); // b-held timer 32 -> 4
        fresh((p) => { p.phys.grounded = true; p.timer = 10; });
        call("DOWNSPECIALGROUND", "main", INbackStick); // lsX*face<0 -> 32
        fresh((p) => { p.phys.grounded = true; p.phys.onSurface = [1, 0];
                       p.timer = 5; });
        call("DOWNSPECIALGROUND", "main", INplatDrop); // platform drop
        fresh((p) => { p.phys.grounded = false; p.timer = 10; });
        call("DOWNSPECIALGROUND", "main", INn); // airborne cross-call
        fresh((p) => { p.timer = 10; });
        call("DOWNSPECIALGROUND", "interrupt", INy); // KNEEBEND cancel
        fresh((p) => { p.timer = 50; p.phys.grounded = true; });
        call("DOWNSPECIALGROUND", "interrupt", INn); // WAIT
        fresh((p) => { p.timer = 50; p.phys.grounded = false; });
        call("DOWNSPECIALGROUND", "interrupt", INn); // FALL
        fresh((p) => { p.phys.grounded = false; });
        call("DOWNSPECIALAIR", "init", INn);
        fresh((p) => { p.phys.grounded = false; p.timer = 6;
                       p.phys.cVel.x = 2; p.phys.cVel.y = -90; });
        call("DOWNSPECIALAIR", "main", INn); // decel + terminalV clamp
        fresh((p) => { p.phys.grounded = false; p.timer = 6;
                       p.phys.cVel.x = -0.5; });
        call("DOWNSPECIALAIR", "main", INn); // small negative decel arm
        fresh((p) => { p.phys.grounded = true; p.timer = 10; });
        call("DOWNSPECIALAIR", "main", INn); // grounded cross-call
        fresh((p) => { p.timer = 10; });
        call("DOWNSPECIALAIR", "interrupt", INy); // JUMPAERIALF + turnOff
        fresh((p) => { p.timer = 10; });
        call("DOWNSPECIALAIR", "interrupt", INyBack); // JUMPAERIALB
        fresh((p) => { p.timer = 10; p.phys.doubleJumped = true; });
        call("DOWNSPECIALAIR", "interrupt", INy); // doubleJumped arm
        fresh((p) => { p.timer = 10; });
        call("DOWNSPECIALAIR", "land", INn); // reflector arm
        fresh((p) => { p.timer = 40; });
        call("DOWNSPECIALAIR", "land", INn); // bare actionState arm
        // throws (self-grab: grabbing = grabbedBy = 3)
        fresh(); call("THROWUP", "init", INn); // grabbing -1 guard
        fresh(grab); call("THROWUP", "init", INn);
        fresh((p) => { grab(p); p.phys.releaseFrame = 34; p.timer = 15.8; });
        call("THROWUP", "main", INn); // LASER crossing 16
        fresh((p) => { grab(p); p.phys.releaseFrame = 8; p.timer = 6.5; });
        call("THROWUP", "main", INn); // hq push crossing 7
        fresh((p) => { p.phys.grabbing = 3; p.phys.releaseFrame = 10;
                       p.timer = 1; });
        call("THROWUP", "interrupt", INn); // CATCHCUT (grabbedBy mismatch)
        fresh((p) => { p.phys.grabbing = 3; p.timer = 34; });
        call("THROWUP", "interrupt", INn); // >33 WAIT arm
        fresh(); call("THROWFORWARD", "init", INn);
        fresh(grab); call("THROWFORWARD", "init", INn); // randomShout draw
        fresh((p) => { grab(p); p.phys.releaseFrame = 11; p.timer = 10.5; });
        call("THROWFORWARD", "main", INn); // hq push crossing 11 + setVel
        fresh(); call("THROWBACK", "init", INn);
        fresh(grab); call("THROWBACK", "init", INn);
        fresh((p) => { grab(p); p.phys.releaseFrame = 16; p.timer = 13.8; });
        call("THROWBACK", "main", INn); // LASER crossing 14
        fresh((p) => { grab(p); p.phys.releaseFrame = 8; p.timer = 9.5; });
        call("THROWBACK", "main", INn); // face-flip crossing 10
        fresh(); call("THROWDOWN", "init", INn);
        fresh(grab); call("THROWDOWN", "init", INn);
        fresh((p) => { grab(p); p.phys.releaseFrame = 33; p.timer = 32.5; });
        call("THROWDOWN", "main", INn); // hq push (true flag) crossing 33
        fresh((p) => { grab(p); p.phys.releaseFrame = 33; p.timer = 22.5; });
        call("THROWDOWN", "main", INn); // LASER crossing 23
        // THROWN* inits (self grabbedBy)
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
        // grabbedBy === -1 init guard (guarded family only)
        fresh(); call("THROWNMARTHUP", "init", INn);
        // main guard + clamp arms
        fresh((p) => { p.phys.grabbedBy = -1; p.timer = 1; });
        call("THROWNMARTHUP", "main", INn); // main -1 guard
        fresh((p) => { p.phys.grabbedBy = 3; p.timer = 6; });
        call("THROWNFOXUP", "main", INn); // local clamp (len 6)
        fresh((p) => { p.phys.grabbedBy = 3; p.timer = 11; });
        call("THROWNFOXFORWARD", "main", INn); // double-clamp quirk (len 11)
        fresh((p) => { p.phys.grabbedBy = 3; p.timer = 8; });
        call("THROWNPUFFUP", "main", INn); // player-timer clamp (len 8)
        // grabbedBy < p (timer = -1) arm: inject slot 2
        M.playerType[2] = 0;
        M.player[2] = new PJ.playerObject(2, [30, 20], 1);
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
        fresh(cliff(23)); call("CLIFFGETUPQUICK", "main", INn); // grounding
        fresh(cliff(25)); call("CLIFFGETUPQUICK", "main", INn); // setVel arm
        fresh(cliff(34)); call("CLIFFGETUPQUICK", "interrupt", INn); // regrab T
        fresh(cliff(12)); call("CLIFFESCAPEQUICK", "main", INn);
        fresh(cliff(21)); call("CLIFFESCAPEQUICK", "main", INn); // grounding
        fresh(cliff(50)); call("CLIFFESCAPEQUICK", "interrupt", INn);
        fresh(cliff(53)); call("CLIFFGETUPSLOW", "main", INn); // grounding
        fresh(cliff(60)); call("CLIFFGETUPSLOW", "interrupt", INn);
        fresh(cliff(53)); call("CLIFFESCAPESLOW", "main", INn); // grounding
        fresh(cliff(80)); call("CLIFFESCAPESLOW", "interrupt", INn);
        fresh(cliff(14)); call("CLIFFJUMPQUICK", "main", INn); // cVel at 15
        fresh(cliff(16)); call("CLIFFJUMPQUICK", "main", INn); // drift arm
        fresh(cliff(52)); call("CLIFFJUMPQUICK", "interrupt", INn); // FALL
        fresh(cliff(19)); call("CLIFFJUMPSLOW", "main", INn); // cVel at 20
        fresh(cliff(52)); call("CLIFFJUMPSLOW", "interrupt", INn);
        fresh(cliff(23)); call("CLIFFATTACKQUICK", "main", INn); // grounding
        fresh(cliff(24)); call("CLIFFATTACKQUICK", "main", INn); // shout arm
        fresh(cliff(55)); call("CLIFFATTACKQUICK", "interrupt", INn);
        fresh(cliff(53)); call("CLIFFATTACKSLOW", "main", INn); // grounding
        fresh(cliff(56)); call("CLIFFATTACKSLOW", "main", INn); // shout arm
        fresh(cliff(70)); call("CLIFFATTACKSLOW", "interrupt", INn);
        // appeal
        fresh(); call("APPEAL", "init", INn);
        fresh((p) => { p.timer = 30; }); call("APPEAL", "main", INn);
        fresh((p) => { p.timer = 89; }); call("APPEAL", "main", INn);
        fresh((p) => { p.timer = 111; }); call("APPEAL", "interrupt", INn);
        // interrupt checkFor-block dispatch payloads
        fresh((p) => { p.timer = 16; });
        call("JAB1", "interrupt", INy); // KNEEBEND arm
        fresh((p) => { p.timer = 16; });
        call("JAB1", "interrupt", INbDown); // MOVES[b] special dispatch
      } finally {
        Math.random = savedRandom;
        M.player[3] = savedP3;
        M.playerType[3] = savedT3;
        M.player[2] = savedP2;
        M.playerType[2] = savedT2;
        M.characterSelections[3] = savedCS3;
        // net-restore the article/hq globals the sweep touched (rule 12)
        ART.aArticles.splice(savedArticles);
        HD.hitQueue.splice(savedHq);
      }
      return n;
    },
  };
})();
