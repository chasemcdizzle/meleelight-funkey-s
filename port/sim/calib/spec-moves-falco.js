// spec-moves-falco.js — M2 task 9 capture spec: characters/falco moves
// (second per-char cluster; task 8's fox spec followed exactly — see
// spec-moves-fox.js for the record-type documentation, which applies
// verbatim with fox->falco). Registers window.__capSpecs["moves-falco"].
// Injected AFTER capturelib.js. See FORMAT.md "The moves-falco spec".
//
// BOUNDARY: every function property of every FALCO-ORIGIN actionStates
// entry (fn identity vs the characters/falco/moves module index — rule 15's
// instrument). Falco-origin entries exist ONLY on table 3: 69 module keys —
// 53 moves x {init,main,interrupt} + 13 x {+land} (ATTACKAIR*x5,
// SIDESPECIALAIR, UPSPECIALCHARGE/LAUNCH, FIREFOXBOUNCE,
// DOWNSPECIALAIR{START,LOOP,END,TURN}) + 3 init-only delegates (UPSPECIAL,
// DOWNSPECIALAIR, DOWNSPECIALGROUND) = 214 fns, plus the 2 article init
// boundaries (LASER/ILLUSION) = 216 wrapped.
//
// GOLDENS: g02/g05/g07 — the falco carriers (g02 falco/puff/ystory,
// g05 marth/falco/fdest, g07 falco/CPU-falcon/battlefield — the second
// CPU-golden capture; the AI plane stays JS-side per the M2 AI policy and
// its seeded draws land as standalone Math.random records).
//
// STRUCTURE NOTES vs fox (measured): falco's THROW* init has NO
// grabbing===-1 guard; falco's THROWN* have NO grabbedBy===-1 guards and
// NO offset clamps; falco's CLIFF* have NO canGrabLedge table-write arm;
// falco's article options carry isFox:false (+ partOfThrow:true on the
// THROWDOWN lasers); falco's shine is the 4-sub-state machine.
(() => {
  const EXCLUDE = ["charAttributes", "charHitboxes", "percentShake"];
  const SND_KEYS = ["CLIFFCATCH", "DEAD", "ESCAPEAIR", "ESCAPEB", "ESCAPEF",
                    "ESCAPEN", "FURAFURA", "GUARDOFF", "GUARDON", "JUMP",
                    "JUMPAERIAL", "OTTOTTOWAIT", "TECH"];
  const SETVEL_KEYS = ["DOWNSTANDB", "DOWNSTANDF", "ESCAPEB", "ESCAPEF",
                       "TECHB", "TECHF"];

  window.__capSpecs["moves-falco"] = {
    expectWrapped: 216,

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
      // the falco module index (characters/falco/moves/index.js) — found by
      // fn IDENTITY against the live table (rule 15's measurement);
      // DOWNSPECIALGROUNDSTART is unique to falco's index (fox lacks it,
      // characters/falco/index.js's default has .moves instead):
      const falcoTbl = AS.actionStates[3];
      if (!falcoTbl || !falcoTbl.JAB1) {
        throw new Error("moves-falco: actionStates[3].JAB1 missing");
      }
      const FALCOMOVES = find((ex) =>
        ex.default && typeof ex.default === "object" &&
        ex.default.JAB1 && typeof ex.default.JAB1 === "object" &&
        typeof ex.default.JAB1.init === "function" &&
        ex.default.JAB1.init === falcoTbl.JAB1.init &&
        ex.default.DOWNSPECIALGROUNDSTART && ex.default.THROWNFALCOUP &&
        !ex.default.WAIT && !ex.default.moves, "falcoMovesIndex").default;
      const falcoKeys = Object.keys(FALCOMOVES);
      if (falcoKeys.length !== 69) {
        throw new Error("moves-falco: expected 69 falco index keys, got " +
                        falcoKeys.length);
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
        throw new Error("moves-falco: expected 180 Howl sounds, wrapped " +
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
          if (t && t.attr) t.vfx.push(cfg.name);
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

      // --- the falco-move boundary wrapper -----------------------------------
      const wrapFalcoFn = (entry, k) => {
        const orig = entry[k];
        const name = entry.name;
        entry[k] = function () {
          if (inScope > 0) return orig.apply(this, arguments);
          const args = Array.prototype.slice.call(arguments);
          // falco phases are (p[, input]) — no extras anywhere in the set
          if (args.length > 2) {
            throw new Error("moves-falco: " + name + "." + k +
                            " called with " + args.length + " args");
          }
          let inCanon = "null"; // 1-arg THROWN*{BACK,DOWN} dispatch sites
          if (args.length === 2) {
            if (!isGodInput(args[1])) {
              throw new Error("moves-falco: " + name + "." + k +
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
              ',"vfx":' + ctx.canon(fr.vfx) + "}";
          ctx.push("move", argsCanon, ctx.canon(ret), post);
          return ret;
        };
        ctx.wrapped++;
      };

      // --- the non-falco per-char seam logger ------------------------------
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
            throw new Error("moves-falco: seam " + name + "." + k +
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
      // are silent chain-safe surface; inside a falco record they are the
      // transparent nested C tree (task 7's bodies).
      const seen = new WeakSet();
      const sharedOrigin = [{}, {}, {}, {}, {}];
      const falcoOrigin = [{}, {}, {}, {}, {}];
      const nameMap = [{}, {}, {}, {}, {}];
      const perTableFalco = [0, 0, 0, 0, 0];
      for (let c = 0; c < AS.actionStates.length; c++) {
        const tbl = AS.actionStates[c];
        if (!tbl) continue;
        for (const st of Object.keys(tbl)) {
          const entry = tbl[st];
          if (!entry || typeof entry !== "object") continue;
          if (typeof entry.name !== "string") {
            throw new Error("moves-falco: move object without a name");
          }
          nameMap[c][st] = entry.name;
          const sh = SHARED[st] !== undefined &&
                     typeof SHARED[st].init === "function" &&
                     entry.init === SHARED[st].init;
          sharedOrigin[c][st] = sh;
          const fc = !sh && FALCOMOVES[st] !== undefined &&
                     typeof FALCOMOVES[st].init === "function" &&
                     entry.init === FALCOMOVES[st].init;
          falcoOrigin[c][st] = fc;
          if (seen.has(entry)) {
            throw new Error("moves-falco: entry object shared across tables");
          }
          seen.add(entry);
          for (const k of Object.keys(entry)) {
            if (typeof entry[k] !== "function") continue;
            if (fc) {
              wrapFalcoFn(entry, k);
              perTableFalco[c]++;
            } else if (!sh) {
              wrapSeamFn(entry, k);
            }
          }
        }
      }
      // measured composition: falco-origin only on table 3, all 69 states,
      // 214 fns (53x3 + 13x4 + the 3 init-only delegates)
      for (let c = 0; c < 5; c++) {
        const want = c === 3 ? 214 : 0;
        if (perTableFalco[c] !== want) {
          throw new Error("moves-falco: table " + c + " wrapped " +
                          perTableFalco[c] + " falco fns, expected " + want);
        }
      }
      for (const st of falcoKeys) {
        if (!falcoOrigin[3][st]) {
          throw new Error("moves-falco: falco table state not falco-origin: " +
                          st);
        }
        if (nameMap[3][st] !== st) {
          throw new Error("moves-falco: falco state key != move name: " + st);
        }
      }
      ctx.declare("move");
      ctx.declare("mdispatch");

      // --- mvData: task-7 dump + the falco extension (rule 15) ---------------
      const mvDataCanon = () => {
        const dump = { chars: {}, falco: { data: {}, origin: falcoOrigin[3] },
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
        // falco data plane: every own enumerable ARRAY-valued non-function
        // prop of every falco-origin entry (executed data, never retyped).
        for (const st of falcoKeys) {
          const entry = AS.actionStates[3][st];
          const d = {};
          let any = false;
          for (const k of Object.keys(entry)) {
            if (Array.isArray(entry[k])) { d[k] = entry[k]; any = true; }
          }
          if (any) dump.falco.data[st] = d;
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
        throw new Error("moves-falco: mvData drifted during the match");
      }
      return 1;
    },

    // Synthetic-domain sweep (rules 11/12): the task-8 scaffolding reused —
    // a REAL upstream playerObject(3, [10,20], 1) injected into inactive
    // slot 3 with characterSelections[3] set to 3 (both restored); a second
    // player in slot 2 covers the THROWN* grabbedBy<p (timer=-1) arm;
    // Math.random swapped for the sweep mulberry32 (0x0badf00d); article
    // spawns and hitQueue pushes spliced back out (net-restore purity,
    // guarded by x2 byte-stability + STREAM MATCH). CLIFF* run against the
    // default pre-match stage. Falco-specific coverage: the shine
    // 4-sub-state machine (both environments), THROW* laser crossings +
    // partOfThrow rows, the interrupt-only grabbing===-1 bare-return arms,
    // the no-snap THROWNPUFF{UP,FORWARD,DOWN} grabbedBy=-1 inits (safe:
    // no deref until timer>0). Unswept (documented): THROW*.init without a
    // grab and snap-family THROWN*.init with grabbedBy=-1 (upstream itself
    // throws — actionStates[undefined]/player[-1]), THROWN* offset
    // overruns (upstream throws), interrupt-tail WALK arms shadowed by
    // checkForTilts.
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
        M.characterSelections[3] = 3; // falco
        const fresh = (mut) => {
          M.player[3] = new PJ.playerObject(3, [10, 20], 1);
          if (mut) mut(M.player[3]);
        };
        const T = AS.actionStates[3]; // falco's table
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
        fresh((p) => { p.phys.jabCombo = true; p.timer = 6; });
        call("JAB2", "main", INn); // -> JAB3 chain (falco: > 6)
        fresh(); call("JAB3", "init", INn);
        fresh((p) => { p.timer = 8; }); call("JAB3", "main", INn); // 9%7==2
        fresh((p) => { p.timer = 42; p.phys.jabCombo = true; });
        call("JAB3", "main", INn); // combo restart arm (43)
        // tilts / smashes
        fresh(); call("UPTILT", "init", INn);
        fresh((p) => { p.timer = 4; }); call("UPTILT", "main", INn); // ===5
        fresh(); call("FORWARDTILT", "init", INn);
        fresh((p) => { p.timer = 6; });
        call("FORWARDTILT", "main", INn); // ===7 actives [T,T,T,F]
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
        fresh((p) => { p.timer = 6; }); call("UPSMASH", "main", INn); // ===7
        fresh((p) => { p.timer = 10; });
        call("UPSMASH", "main", INn); // ===11 (falco)
        fresh((p) => { p.timer = 15; });
        call("UPSMASH", "main", INn); // ===16 turnoff (falco)
        fresh(); call("DOWNSMASH", "init", INn);
        fresh((p) => { p.timer = 5; }); call("DOWNSMASH", "main", INn);
        fresh(); call("ATTACKDASH", "init", INn);
        fresh((p) => { p.timer = 3; }); call("ATTACKDASH", "main", INn);
        fresh((p) => { p.timer = 7; }); call("ATTACKDASH", "main", INn);
        fresh((p) => { p.timer = 1; p.phys.cVel.x = 99; });
        call("ATTACKDASH", "interrupt", INlA); // GRAB arm + dMaxV clamp
        // aerials
        fresh(); call("ATTACKAIRU", "init", INn);
        fresh((p) => { p.timer = 7; });
        call("ATTACKAIRU", "main", INn); // ===8 (sword1)
        fresh((p) => { p.timer = 10; }); call("ATTACKAIRU", "main", INn);
        fresh((p) => { p.phys.autoCancel = true; });
        call("ATTACKAIRU", "land", INn); // LANDING (circleDust: 4 draws)
        fresh((p) => { p.phys.autoCancel = false; });
        call("ATTACKAIRU", "land", INn); // LANDINGATTACKAIRU
        fresh((p) => { p.timer = 23; });
        call("ATTACKAIRB", "main", INn); // ===24 (falco)
        fresh(); call("ATTACKAIRD", "init", INn);
        fresh((p) => { p.timer = 4; });
        call("ATTACKAIRD", "main", INn); // ===5 hitspin
        fresh((p) => { p.timer = 7; });
        call("ATTACKAIRD", "main", INn); // frame++ window
        fresh((p) => { p.timer = 14; });
        call("ATTACKAIRD", "main", INn); // ===15 dair2 swap
        fresh((p) => { p.timer = 24; });
        call("ATTACKAIRD", "main", INn); // ===25 turnoff
        fresh((p) => { p.phys.autoCancel = false; });
        call("ATTACKAIRD", "land", INn); // LANDINGATTACKAIRD
        fresh((p) => { p.timer = 4; });
        call("ATTACKAIRN", "main", INn); // hitboxes.frames++ NaN quirk
        fresh((p) => { p.IASATimer = 1; p.timer = 5; });
        call("ATTACKAIRN", "interrupt", INy); // IASA -> JUMPAERIALF
        fresh((p) => { p.IASATimer = 1; p.timer = 5; });
        call("ATTACKAIRN", "interrupt", INyBack); // IASA -> JUMPAERIALB
        fresh((p) => { p.IASATimer = 1; p.timer = 5; });
        call("ATTACKAIRN", "interrupt", INaUp); // IASA aerial payload: NO
        // dispatch for char 3 upstream (no branch) — returns true bare
        // lasers
        fresh(); call("NEUTRALSPECIALAIR", "init", INn);
        fresh((p) => { p.timer = 6; });
        call("NEUTRALSPECIALAIR", "main", INn); // foxlasercock at 7
        fresh((p) => { p.timer = 12; });
        call("NEUTRALSPECIALAIR", "main", INn); // LASER article at 13
        fresh((p) => { p.timer = 20; p.phys.laserCombo = true; });
        call("NEUTRALSPECIALAIR", "main", INn); // combo arm at 21
        fresh((p) => { p.timer = 20; });
        call("NEUTRALSPECIALGROUND", "main", INb); // combo window 15..28
        fresh((p) => { p.timer = 22; });
        call("NEUTRALSPECIALGROUND", "main", INn); // LASER article at 23
        fresh((p) => { p.timer = 30; p.phys.laserCombo = true; });
        call("NEUTRALSPECIALGROUND", "main", INn); // combo arm at 31
        // phantasm (falco: no grounded-arm recursion, ILLUSION type 0)
        fresh(); call("SIDESPECIALGROUND", "init", INn);
        fresh((p) => { p.timer = 15; });
        call("SIDESPECIALGROUND", "main", INn); // phantasm sounds at 16
        fresh((p) => { p.timer = 16; });
        call("SIDESPECIALGROUND", "main", INn); // cVel burst at 17
        fresh((p) => { p.timer = 17; });
        call("SIDESPECIALGROUND", "main", INn); // ILLUSION type 0 at 18
        fresh((p) => { p.timer = 17; });
        call("SIDESPECIALGROUND", "main", INb); // + b-skip arm
        fresh((p) => { p.timer = 18; });
        call("SIDESPECIALGROUND", "main", INb); // 16..19 skip -> 20 arm
        fresh((p) => { p.timer = 24; });
        call("SIDESPECIALGROUND", "main", INn); // >20 decel
        fresh((p) => { p.timer = 60; });
        call("SIDESPECIALGROUND", "interrupt", INn); // WAIT (>59)
        fresh((p) => { p.phys.cVel.x = 2; });
        call("SIDESPECIALAIR", "init", INn); // cVel *= 0.667 + decel
        fresh((p) => { p.timer = 5; p.phys.cVel.x = 1; });
        call("SIDESPECIALAIR", "main", INn); // <=15 decel arm
        fresh((p) => { p.timer = 30; });
        call("SIDESPECIALAIR", "main", INn); // >=25 gravity + >20 decel
        fresh((p) => { p.timer = 15; });
        call("SIDESPECIALAIR", "main", INn); // sounds at 16
        fresh((p) => { p.timer = 16; });
        call("SIDESPECIALAIR", "main", INn); // cVel burst at 17
        fresh((p) => { p.timer = 17; });
        call("SIDESPECIALAIR", "main", INn); // ILLUSION type 0 at 18
        fresh((p) => { p.timer = 18; });
        call("SIDESPECIALAIR", "main", INb); // 16..19 skip -> 20 arm
        fresh((p) => { p.timer = 59; });
        call("SIDESPECIALAIR", "interrupt", INn); // FALLSPECIAL (>58)
        fresh((p) => { p.timer = 25; });
        call("SIDESPECIALAIR", "land", INn); // LANDINGFALLSPECIAL
        fresh((p) => { p.timer = 5; });
        call("SIDESPECIALAIR", "land", INn); // actionState only
        // firebird
        fresh(); call("UPSPECIAL", "init", INn); // -> UPSPECIALCHARGE chain
        fresh((p) => { p.timer = 41; });
        call("UPSPECIALCHARGE", "main", INstick); // atan2 at 42
        fresh((p) => { p.timer = 41; p.phys.grounded = true;
                       p.phys.onSurface = [0, 0]; });
        call("UPSPECIALCHARGE", "main", INdown); // +2PI + clamp arms
        fresh((p) => { p.timer = 43; });
        call("UPSPECIALCHARGE", "interrupt", INn); // LAUNCH + undef return
        fresh(); call("UPSPECIALCHARGE", "land", INn); // empty land
        fresh((p) => { p.phys.upbAngleMultiplier = 3; });
        call("UPSPECIALLAUNCH", "init", INn); // face = -1 arm
        fresh((p) => { p.phys.upbAngleMultiplier = Math.PI / 2; });
        call("UPSPECIALLAUNCH", "init", INn); // angle === PI/2 arm
        fresh((p) => { p.timer = 5; p.phys.upbAngleMultiplier = 1; });
        call("UPSPECIALLAUNCH", "main", INn); // 0.17 sin/cos decay arm
        fresh((p) => { p.timer = 22; });
        call("UPSPECIALLAUNCH", "main", INn); // ===23 turnOff + rot reset
        fresh((p) => { p.timer = 30; p.phys.grounded = false; });
        call("UPSPECIALLAUNCH", "main", INn); // >=23 fastfall/airDrift arm
        fresh((p) => { p.timer = 10; });
        call("UPSPECIALLAUNCH", "land", INn); // FIREFOXBOUNCE bounce (<23)
        fresh((p) => { p.timer = 40; });
        call("UPSPECIALLAUNCH", "land", INn); // impactLand
        fresh((p) => { p.timer = 43; p.phys.grounded = true; });
        call("UPSPECIALLAUNCH", "interrupt", INn); // WAIT (>42)
        fresh((p) => { p.timer = 43; p.phys.grounded = false; });
        call("UPSPECIALLAUNCH", "interrupt", INn); // FALLSPECIAL
        fresh(); call("FIREFOXBOUNCE", "init", INn);
        fresh((p) => { p.phys.cVel.x = 2; });
        call("FIREFOXBOUNCE", "main", INn); // decel arm
        fresh((p) => { p.timer = 15; p.phys.grounded = true; });
        call("FIREFOXBOUNCE", "interrupt", INn);
        fresh((p) => { p.timer = 15; p.phys.grounded = false; });
        call("FIREFOXBOUNCE", "interrupt", INn);
        fresh(); call("FIREFOXBOUNCE", "land", INn); // LANDING
        // shine (the falco 4-sub-state machine, both environments)
        fresh((p) => { p.phys.grounded = true; });
        call("DOWNSPECIALGROUND", "init", INn); // -> GROUNDSTART chain
        fresh((p) => { p.phys.grounded = false; });
        call("DOWNSPECIALAIR", "init", INn); // -> AIRSTART chain
        fresh((p) => { p.timer = 1; p.phys.cVel.x = 2; });
        call("DOWNSPECIALAIRSTART", "main", INn); // >0.85 decel + turnoff
        fresh((p) => { p.timer = 1; p.phys.cVel.x = 0.5; });
        call("DOWNSPECIALAIRSTART", "main", INn); // small decel branch
        fresh((p) => { p.timer = 1; p.phys.cVel.x = -2; });
        call("DOWNSPECIALAIRSTART", "main", INn); // <-0.85 branch
        fresh((p) => { p.timer = 1; p.phys.cVel.x = -0.5; });
        call("DOWNSPECIALAIRSTART", "main", INn); // small negative branch
        fresh((p) => { p.timer = 4; });
        call("DOWNSPECIALAIRSTART", "interrupt", INn); // -> AIRLOOP (>3)
        fresh(); call("DOWNSPECIALAIRSTART", "land", INn);
        fresh(); call("DOWNSPECIALAIRLOOP", "init", INn);
        fresh((p) => { p.timer = 1; p.shineLoop = 6; });
        call("DOWNSPECIALAIRLOOP", "main", INn); // shineLoop reset arm
        fresh((p) => { p.timer = 1; p.shineLoop = 3; });
        call("DOWNSPECIALAIRLOOP", "main", INn);
        fresh(); call("DOWNSPECIALAIRLOOP", "interrupt", INbackStick);
        fresh((p) => { p.phys.inShine = 25; });
        call("DOWNSPECIALAIRLOOP", "interrupt", INn); // -> AIREND
        fresh(); call("DOWNSPECIALAIRLOOP", "interrupt", INy); // DJ -> JAF
        fresh(); call("DOWNSPECIALAIRLOOP", "interrupt", INyBack); // -> JAB
        fresh((p) => { p.timer = 29; });
        call("DOWNSPECIALAIRLOOP", "interrupt", INn); // re-init arm (>28)
        fresh(); call("DOWNSPECIALAIRLOOP", "land", INn);
        fresh(); call("DOWNSPECIALAIREND", "init", INn);
        fresh((p) => { p.timer = 1; p.phys.cVel.x = 2; });
        call("DOWNSPECIALAIREND", "main", INn);
        fresh((p) => { p.timer = 19; });
        call("DOWNSPECIALAIREND", "interrupt", INn); // FALL (>18)
        fresh(); call("DOWNSPECIALAIREND", "land", INn);
        fresh(); call("DOWNSPECIALAIRTURN", "init", INn);
        fresh((p) => { p.timer = 1; p.shineLoop = 3; });
        call("DOWNSPECIALAIRTURN", "main", INn);
        fresh((p) => { p.timer = 4; });
        call("DOWNSPECIALAIRTURN", "interrupt", INn); // face flip -> LOOP
        fresh(); call("DOWNSPECIALAIRTURN", "land", INn);
        fresh((p) => { p.phys.grounded = true; });
        call("DOWNSPECIALGROUNDSTART", "init", INn);
        fresh((p) => { p.phys.grounded = true; p.timer = 1;
                       p.phys.onSurface = [1, 0]; });
        call("DOWNSPECIALGROUNDSTART", "main", INplatDrop); // platform drop
        fresh((p) => { p.phys.grounded = true; p.timer = 4; });
        call("DOWNSPECIALGROUNDSTART", "interrupt", INn); // -> GROUNDLOOP
        fresh((p) => { p.phys.grounded = true; });
        call("DOWNSPECIALGROUNDLOOP", "init", INn);
        fresh((p) => { p.phys.grounded = true; p.timer = 1;
                       p.phys.onSurface = [1, 0]; });
        call("DOWNSPECIALGROUNDLOOP", "main", INplatDrop); // platform drop
        fresh((p) => { p.phys.grounded = true; p.timer = 1;
                       p.shineLoop = 6; });
        call("DOWNSPECIALGROUNDLOOP", "main", INn); // shineLoop reset
        fresh((p) => { p.phys.grounded = true; });
        call("DOWNSPECIALGROUNDLOOP", "interrupt", INbackStick); // -> TURN
        fresh((p) => { p.phys.grounded = true; p.phys.inShine = 25; });
        call("DOWNSPECIALGROUNDLOOP", "interrupt", INn); // -> GROUNDEND
        fresh((p) => { p.phys.grounded = true; });
        call("DOWNSPECIALGROUNDLOOP", "interrupt", INy); // KNEEBEND arm
        fresh((p) => { p.phys.grounded = true; p.timer = 29; });
        call("DOWNSPECIALGROUNDLOOP", "interrupt", INn); // re-init arm
        fresh((p) => { p.phys.grounded = true; });
        call("DOWNSPECIALGROUNDEND", "init", INn);
        fresh((p) => { p.phys.grounded = true; p.timer = 19; });
        call("DOWNSPECIALGROUNDEND", "interrupt", INn); // WAIT (>18)
        fresh((p) => { p.phys.grounded = true; });
        call("DOWNSPECIALGROUNDTURN", "init", INn);
        fresh((p) => { p.phys.grounded = true; p.timer = 1;
                       p.shineLoop = 3; });
        call("DOWNSPECIALGROUNDTURN", "main", INn);
        fresh((p) => { p.phys.grounded = true; p.timer = 4; });
        call("DOWNSPECIALGROUNDTURN", "interrupt", INn); // face flip -> LOOP
        // throws (self-grab: grabbing = grabbedBy = 3; falco inits REQUIRE
        // a grab — no -1 guard upstream)
        fresh(grab); call("THROWUP", "init", INn);
        fresh((p) => { grab(p); p.phys.releaseFrame = 34; p.timer = 13.9; });
        call("THROWUP", "main", INn); // foxlasercock crossing 14
        fresh((p) => { grab(p); p.phys.releaseFrame = 34; p.timer = 17.9; });
        call("THROWUP", "main", INn); // LASER crossing 18
        fresh((p) => { grab(p); p.phys.releaseFrame = 34; p.timer = 19.9; });
        call("THROWUP", "main", INn); // LASER crossing 20
        fresh((p) => { grab(p); p.phys.releaseFrame = 34; p.timer = 23.9; });
        call("THROWUP", "main", INn); // LASER crossing 24
        fresh((p) => { grab(p); p.phys.releaseFrame = 34; p.timer = 32.9; });
        call("THROWUP", "main", INn); // holster crossing 33
        fresh((p) => { grab(p); p.phys.releaseFrame = 8; p.timer = 6.5; });
        call("THROWUP", "main", INn); // hq push crossing 7
        fresh((p) => { p.phys.grabbing = 3; p.phys.releaseFrame = 10;
                       p.timer = 1; });
        call("THROWUP", "interrupt", INn); // CATCHCUT (grabbedBy mismatch)
        fresh((p) => { p.phys.grabbing = 3; p.timer = 39; });
        call("THROWUP", "interrupt", INn); // >38 WAIT arm
        fresh((p) => { p.phys.grabbing = -1; p.timer = 1; });
        call("THROWUP", "interrupt", INn); // bare-return arm (undef)
        fresh(grab); call("THROWBACK", "init", INn); // randomShout draw
        fresh((p) => { grab(p); p.phys.releaseFrame = 34; p.timer = 9.9; });
        call("THROWBACK", "main", INn); // face-flip crossing 10
        fresh((p) => { grab(p); p.phys.releaseFrame = 34; p.timer = 13.9; });
        call("THROWBACK", "main", INn); // foxlasercock crossing 14
        fresh((p) => { grab(p); p.phys.releaseFrame = 34; p.timer = 14.9; });
        call("THROWBACK", "main", INn); // LASER crossing 15
        fresh((p) => { grab(p); p.phys.releaseFrame = 34; p.timer = 17.9; });
        call("THROWBACK", "main", INn); // LASER crossing 18
        fresh((p) => { grab(p); p.phys.releaseFrame = 34; p.timer = 20.9; });
        call("THROWBACK", "main", INn); // LASER crossing 21
        fresh((p) => { grab(p); p.phys.releaseFrame = 10; p.timer = 8.5; });
        call("THROWBACK", "main", INn); // hq push crossing 9
        fresh((p) => { p.phys.grabbing = -1; p.timer = 1; });
        call("THROWBACK", "interrupt", INn); // bare-return arm (undef)
        fresh(grab); call("THROWDOWN", "init", INn);
        fresh((p) => { grab(p); p.phys.releaseFrame = 34; p.timer = 21.9; });
        call("THROWDOWN", "main", INn); // foxlasercock crossing 22
        fresh((p) => { grab(p); p.phys.releaseFrame = 34; p.timer = 22.9; });
        call("THROWDOWN", "main", INn); // partOfThrow LASER crossing 23
        fresh((p) => { grab(p); p.phys.releaseFrame = 34; p.timer = 24.9; });
        call("THROWDOWN", "main", INn); // partOfThrow LASER crossing 25
        fresh((p) => { grab(p); p.phys.releaseFrame = 34; p.timer = 27.9; });
        call("THROWDOWN", "main", INn); // partOfThrow LASER crossing 28
        fresh((p) => { grab(p); p.phys.releaseFrame = 34; p.timer = 30.9; });
        call("THROWDOWN", "main", INn); // partOfThrow LASER crossing 31
        fresh((p) => { grab(p); p.phys.releaseFrame = 33; p.timer = 32.5; });
        call("THROWDOWN", "main", INn); // hq push (true flag) crossing 33
        fresh((p) => { p.phys.grabbing = 3; p.timer = 44; });
        call("THROWDOWN", "interrupt", INn); // >43 WAIT arm
        fresh(grab); call("THROWFORWARD", "init", INn); // randomShout draw
        fresh((p) => { grab(p); p.phys.releaseFrame = 12; p.timer = 10.4; });
        call("THROWFORWARD", "main", INn); // hq push crossing 11 + setVel
        fresh((p) => { grab(p); p.phys.releaseFrame = 34; p.timer = 0; });
        call("THROWFORWARD", "main", INn); // Math.max(0,·) index clamp arm
        fresh((p) => { p.phys.grabbing = 3; p.timer = 34; });
        call("THROWFORWARD", "interrupt", INn); // >33 WAIT arm
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
        // the no-snap puff family tolerates grabbedBy=-1 (timer=-1 arm,
        // no deref until timer>0) — falco-specific safe arm
        fresh((p) => { p.phys.grabbedBy = -1; });
        call("THROWNPUFFUP", "init", INn);
        fresh((p) => { p.phys.grabbedBy = -1; });
        call("THROWNPUFFFORWARD", "init", INn);
        // main pos-assignment arms (falco: no clamps)
        fresh((p) => { p.phys.grabbedBy = 3; p.timer = 2; });
        call("THROWNPUFFUP", "main", INn);
        fresh((p) => { p.phys.grabbedBy = 3; p.timer = 2; });
        call("THROWNFOXUP", "main", INn);
        fresh((p) => { p.phys.grabbedBy = 3; p.timer = 2; });
        call("THROWNMARTHDOWN", "main", INn); // face*-1 arm
        fresh((p) => { p.phys.grabbedBy = 3; p.timer = 2; });
        call("THROWNFALCOBACK", "main", INn);
        // grabbedBy < p (timer = -1) arm: inject slot 2
        M.playerType[2] = 0;
        M.player[2] = new PJ.playerObject(3, [30, 20], 1);
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
        // appeal (falco: no setVelocities plane; falcotaunt at 3)
        fresh(); call("APPEAL", "init", INn);
        fresh((p) => { p.timer = 2; }); call("APPEAL", "main", INn);
        fresh((p) => { p.timer = 116; }); call("APPEAL", "interrupt", INn);
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
