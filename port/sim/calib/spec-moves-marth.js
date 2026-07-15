// spec-moves-marth.js — M2 task 11 capture spec: characters/marth moves
// (fourth per-char cluster; task 8's recipe — see spec-moves-fox.js for
// the record-type documentation, which applies with fox->marth plus the
// deltas below). Registers window.__capSpecs["moves-marth"]. Injected
// AFTER capturelib.js. See FORMAT.md "The moves-marth spec".
//
// BOUNDARY: every function property of every MARTH-ORIGIN actionStates
// entry (fn identity vs the characters/marth/moves module index — rule
// 15's instrument). Marth-origin entries exist ONLY on table 0: 75 module
// keys — 58 moves x {init,main,interrupt} + 17 x {+land} (ATTACKAIR*x5,
// NEUTRALSPECIALAIR, DOWNSPECIALAIR, SIDESPECIALAIR + the 8 SIDESPECIALAIR
// chain states, UPSPECIAL) = 242 phase fns, PLUS marth's two NON-phase
// move fns: onClank(p, input) on DOWNSPECIAL{GROUND,AIR}
// (hitDetection.js:71-72's specialClank arm) = 244 marth fns, plus the 2
// article init boundaries (LASER/ILLUSION, measurement only — marth has
// ZERO `articles` imports anywhere under characters/marth/, stronger than
// falcon's dead imports; the article count is pinned ZERO) = 246 wrapped.
//
// GOLDENS: marth carriers measured for live coverage — see
// expected-capture-moves-marth.json (candidates g01 fox/MARTH/battlefield,
// g05 MARTH/falco/fdest, g06 falcon/MARTH/fountain; g04 fields no marth).
//
// MARTH DELTAS (measured per-file diffs vs fox/falco/falcon):
// - NEUTRALSPECIAL{GROUND,AIR}: `player[p].shieldBreakerID =
//   sounds.shieldbreakercharge.play()` CONSUMES the Howl play id (a
//   GLOBAL howler counter — chain state advanced by every sound in the
//   match, mostly outside this spec's records): the wrapper records each
//   consumed id in the record's post "sbid" list (oracle-fed seam, the
//   task-5 launch-getter discipline; the replay injects them and re-emits
//   the consumed list). `sounds.shieldbreakercharge.stop(id)` is recorded
//   as the token "shieldbreakercharge.stop" (furaloop.stop's cousin).
//   NOT checksummed (CHECKSUM.md carries no Howl ids) — sweep-time plays
//   shift later live ids only inside THIS capture's own byte-stable runs.
//   The timer-46 arm writes hitboxes.id[i].dmg — upstream this MUTATES
//   the GLOBAL charHitboxes objects (player.js:132 aliases chars data):
//   the sweep exercises the charged arm and RESTORES all 8 dmg fields
//   (rule 12 net-restore); newDmg == the authored 7 for charge < 30
//   (measured live domain {0,1} on g05).
// - SIDESPECIAL* (dancing blade, 18 states + the dancingBladeCombo /
//   dancingBladeAirMobility helper modules): phys.dancingBlade /
//   phys.dancingBladeDisable are runtime-added (rule 16 — ml_player.h
//   presence-modeled this task).
// - DOWNSPECIAL{GROUND,AIR} (counter) carry specialClank + onClank ->
//   DOWNSPECIAL{GROUND,AIR}2; colourOverlay cycles string literals.
// - THROW*: victims dispatched 2-ARG (.init(grabbing, input) — fox's
//   THROWBACK/THROWDOWN are 1-arg); THROWUP's interrupt -1 arm returns
//   false where the other three fall through undefined.
// - THROWN{PUFF,MARTH,FOX}* are GUARDED (grabbedBy===-1 init guard —
//   except THROWNMARTHBACK, which has NONE — plus a -1 main guard and a
//   len-1 clamp whose order varies per file; THROWNPUFFUP wraps its body
//   in a vacuous `if(player[p].phys)`); THROWN{FALCO,FALCON}* are fox's
//   unguarded family.
// - CLIFF*: all 8 keep fox's onLedge===-1 canGrabLedge table-write arm;
//   CLIFFGETUPQUICK sets ledgeRegrabCount = TRUE (others false).
// - marth aerials do NOT call checkForIASA: ATTACKAIRF/B INLINE the
//   aerial-IASA logic (checkForDoubleJump -> shared JUMPAERIALB/F;
//   checkForAerials payload -> marth[a[1]].init). checkForIASA's char-0
//   MARTHMOVES arm (actionStateShortcuts.js:400-401) is therefore
//   dead-by-construction upstream — the C registers the marth module
//   index anyway (mv_register_char_module), faithful to the source.
(() => {
  const EXCLUDE = ["charAttributes", "charHitboxes", "percentShake"];
  const SND_KEYS = ["CLIFFCATCH", "DEAD", "ESCAPEAIR", "ESCAPEB", "ESCAPEF",
                    "ESCAPEN", "FURAFURA", "GUARDOFF", "GUARDON", "JUMP",
                    "JUMPAERIAL", "OTTOTTOWAIT", "TECH"];
  const SETVEL_KEYS = ["DOWNSTANDB", "DOWNSTANDF", "ESCAPEB", "ESCAPEF",
                       "TECHB", "TECHF"];

  window.__capSpecs["moves-marth"] = {
    expectWrapped: 246,

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
      // the marth module index (characters/marth/moves/index.js) — found
      // by fn IDENTITY against the live table (rule 15's measurement);
      // SIDESPECIALGROUND4DOWN/DOWNSPECIALGROUND2 are unique to marth's
      // index; `!ex.default.moves` excludes the marth CHARACTER object
      // (characters/marth/index.js default = {moves, attributes, ecb}):
      const marthTbl = AS.actionStates[0];
      if (!marthTbl || !marthTbl.JAB1) {
        throw new Error("moves-marth: actionStates[0].JAB1 missing");
      }
      const MARTHMOVES = find((ex) =>
        ex.default && typeof ex.default === "object" &&
        ex.default.JAB1 && typeof ex.default.JAB1 === "object" &&
        typeof ex.default.JAB1.init === "function" &&
        ex.default.JAB1.init === marthTbl.JAB1.init &&
        ex.default.SIDESPECIALGROUND4DOWN && ex.default.DOWNSPECIALGROUND2 &&
        !ex.default.WAIT && !ex.default.moves, "marthMovesIndex").default;
      const marthKeys = Object.keys(MARTHMOVES);
      if (marthKeys.length !== 75) {
        throw new Error("moves-marth: expected 75 marth index keys, got " +
                        marthKeys.length);
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

      // --- sound attribution (incl. ".stop" tokens + the Howl play-id
      // oracle seam) --------------------------------------------------------
      // sounds.shieldbreakercharge.play()'s RETURN VALUE is consumed by
      // the sim (player.shieldBreakerID = ...): its wrapper additionally
      // records the id into the frame's sbid list (post envelope).
      let sndWrapped = 0;
      for (const name of Object.keys(SFX.sounds)) {
        const howl = SFX.sounds[name];
        if (!howl || typeof howl.play !== "function") continue;
        const orig = howl.play;
        if (name === "shieldbreakercharge") {
          howl.play = function () {
            const ret = orig.apply(this, arguments);
            const t = top();
            if (t && t.attr) {
              t.snd.push(name);
              t.sbid.push(ret);
            }
            return ret;
          };
        } else {
          howl.play = function () {
            const t = top();
            if (t && t.attr) t.snd.push(name);
            return orig.apply(this, arguments);
          };
        }
        sndWrapped++;
      }
      if (sndWrapped !== 180) {
        throw new Error("moves-marth: expected 180 Howl sounds, wrapped " +
                        sndWrapped);
      }
      for (const stopName of ["furaloop", "shieldbreakercharge"]) {
        const howl = SFX.sounds[stopName];
        const origStop = howl.stop;
        howl.stop = function () {
          const t = top();
          if (t && t.attr) t.snd.push(stopName + ".stop");
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

      // --- article seam (measurement instrument: marth has ZERO article
      // imports — the pin freezes the count at ZERO; a record here means
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

      // --- the marth-move boundary wrapper -----------------------------------
      const wrapMarthFn = (entry, k) => {
        const orig = entry[k];
        const name = entry.name;
        entry[k] = function () {
          if (inScope > 0) return orig.apply(this, arguments);
          const args = Array.prototype.slice.call(arguments);
          // marth phases and onClank are (p[, input]); THROWN* inits can
          // arrive 1-arg from fox/falco THROWBACK/THROWDOWN dispatch sites.
          let inCanon = "null";
          if (args.length > 2) {
            throw new Error("moves-marth: " + name + "." + k +
                            " called with " + args.length + " args");
          }
          if (args.length === 2) {
            if (!isGodInput(args[1])) {
              throw new Error("moves-marth: " + name + "." + k +
                              " arg 1 is not the god input");
            }
            inCanon = inputsCanon(args[1]);
          }
          const argsCanon = "[" + JSON.stringify(k) + "," +
              JSON.stringify(name) + "," + ctx.canon([args[0]]) + "," +
              inCanon + "," + preEnvelope() + "]";
          const fr = { attr: true, rng: [], sbid: [], snd: [], vfx: [] };
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
              ',"sbid":' + ctx.canon(fr.sbid) +
              ',"snd":' + ctx.canon(fr.snd) +
              ',"vfx":' + ctx.canon(fr.vfx) + "}";
          ctx.push("move", argsCanon, ctx.canon(ret), post);
          return ret;
        };
        ctx.wrapped++;
      };

      // --- the non-marth per-char seam logger ------------------------------
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
          if (args.length > 2 ||
              (args.length === 2 && !isGodInput(args[1]))) {
            throw new Error("moves-marth: seam " + name + "." + k +
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
      // are silent chain-safe surface; inside a marth record they are the
      // transparent nested C tree (task 7's bodies).
      const seen = new WeakSet();
      const sharedOrigin = [{}, {}, {}, {}, {}];
      const marthOrigin = [{}, {}, {}, {}, {}];
      const nameMap = [{}, {}, {}, {}, {}];
      const perTableMarth = [0, 0, 0, 0, 0];
      for (let c = 0; c < AS.actionStates.length; c++) {
        const tbl = AS.actionStates[c];
        if (!tbl) continue;
        for (const st of Object.keys(tbl)) {
          const entry = tbl[st];
          if (!entry || typeof entry !== "object") continue;
          if (typeof entry.name !== "string") {
            throw new Error("moves-marth: move object without a name");
          }
          nameMap[c][st] = entry.name;
          const sh = SHARED[st] !== undefined &&
                     typeof SHARED[st].init === "function" &&
                     entry.init === SHARED[st].init;
          sharedOrigin[c][st] = sh;
          const mo = !sh && MARTHMOVES[st] !== undefined &&
                     typeof MARTHMOVES[st].init === "function" &&
                     entry.init === MARTHMOVES[st].init;
          marthOrigin[c][st] = mo;
          if (seen.has(entry)) {
            throw new Error("moves-marth: entry object shared across tables");
          }
          seen.add(entry);
          for (const k of Object.keys(entry)) {
            if (typeof entry[k] !== "function") continue;
            if (mo) {
              wrapMarthFn(entry, k);
              perTableMarth[c]++;
            } else if (!sh) {
              wrapSeamFn(entry, k);
            }
          }
        }
      }
      // measured composition: marth-origin only on table 0, all 75 states,
      // 244 fns (242 phase fns + the 2 onClank)
      for (let c = 0; c < 5; c++) {
        const want = c === 0 ? 244 : 0;
        if (perTableMarth[c] !== want) {
          throw new Error("moves-marth: table " + c + " wrapped " +
                          perTableMarth[c] + " marth fns, expected " + want);
        }
      }
      for (const st of marthKeys) {
        if (!marthOrigin[0][st]) {
          throw new Error(
            "moves-marth: marth table state not marth-origin: " + st);
        }
        if (nameMap[0][st] !== st) {
          throw new Error("moves-marth: marth state key != move name: " + st);
        }
      }
      ctx.declare("move");
      ctx.declare("mdispatch");

      // --- mvData: task-7 dump + the marth extension (rule 15) ---------------
      const mvDataCanon = () => {
        const dump = { chars: {},
                       marth: { data: {}, origin: marthOrigin[0] },
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
        // marth data plane: every own enumerable ARRAY-valued non-function
        // prop of every marth-origin entry (executed data, never retyped:
        // setVelocities incl. UPSPECIAL's pairs, CLIFF*/THROWN* offsets,
        // THROWNPUFFBACK.offsetVel (dead), the canGrabLedge bool pairs).
        for (const st of marthKeys) {
          const entry = AS.actionStates[0][st];
          const d = {};
          let any = false;
          for (const k of Object.keys(entry)) {
            if (Array.isArray(entry[k])) { d[k] = entry[k]; any = true; }
          }
          if (any) dump.marth.data[st] = d;
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
        throw new Error("moves-marth: mvData drifted during the match");
      }
      return 1;
    },

    // Synthetic-domain sweep (rules 11/12): the task-8 scaffolding reused —
    // a REAL upstream playerObject(0, [10,20], 1) injected into inactive
    // slot 3 with characterSelections[3] set to 0 (both restored); a second
    // player in slot 2 covers the THROWN* grabbedBy<p (timer=-1) arm;
    // Math.random swapped for the sweep mulberry32 (0x0badf00d); hitQueue
    // pushes spliced back out. Marth-specific coverage: the NEUTRALSPECIAL
    // charge family (B-held charge + blend + dashDust, release-stop,
    // charge==122 auto-stop, the timer-11 play-id record (sbid), the
    // timer-46 dmg-write arm uncharged AND >=120-charged — the charged
    // write MUTATES the global charHitboxes objects, so the sweep saves
    // and RESTORES all 8 nsg/nsa dmg fields (rule 12 net-restore) — and
    // the timer-50 groundBounce arm), DOLPHINSLASH (angle-multiplier +
    // face-flip arms, the rotated PAIR setVelocities window, the >22
    // drift clamp, all three land-guard disjuncts + the all-false no-init
    // arm), the DANCINGBLADE chains (combo-window disable/enable arms,
    // every UP/DOWN/FORWARD chain dispatch, the 4DOWN multi-hit
    // timer%6 switch incl. the shout6 cutoff, air mobility + landing
    // actionState writes), COUNTER both environments (colour-cycle arms,
    // onClank -> DOWNSPECIAL{GROUND,AIR}2, land), THROW* incl. the
    // 2-arg victim dispatch chains + hq crossings + CATCHCUT arms,
    // all 20 THROWN* inits + guard/clamp arms, all 8 CLIFF*.
    // Unswept (documented): the CLIFF* onLedge===-1 canGrabLedge
    // table-write arm (fox precedent: C traps; mvData finalCheck guards),
    // guarded-THROWN offset overruns past the clamp window and unguarded
    // THROWN{FALCO,FALCON}* -1/overrun arms (upstream throws),
    // interrupt-tail WALK arms shadowed by checkForTilts.
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
      const INaPress = mk({ 0: { a: true }, 1: { a: false } });
      const INb = mk({ 0: { b: true } });
      const INbPress = mk({ 0: { b: true }, 1: { b: false } });
      const INy = mk({ 0: { y: true } });
      const INyBack = mk({ 0: { y: true, lsX: -0.5 } });
      const INaUp = mk({ 0: { a: true, lsY: 0.8 }, 1: { lsY: 0.8 },
                         2: { lsY: 0.8 }, 3: { lsY: 0.8 } });
      const INbDown = mk({ 0: { b: true, lsY: -1 } });
      const INlA = mk({ 0: { lA: 1 } });
      const INstick = mk({ 0: { lsX: 0.5, lsY: 0.5 } });
      const INbackStick = mk({ 0: { lsX: -0.35 } });
      const INlsX08 = mk({ 0: { lsX: 0.8 } });
      const INup = mk({ 0: { lsY: 0.8 } });
      const INdown = mk({ 0: { lsY: -0.8 } });

      const savedP3 = M.player[3];
      const savedT3 = M.playerType[3];
      const savedP2 = M.player[2];
      const savedT2 = M.playerType[2];
      const savedCS3 = M.characterSelections[3];
      const savedRandom = Math.random;
      const savedArticles = ART.aArticles.length;
      const savedHq = HD.hitQueue.length;
      // the global charHitboxes dmg fields the charged shield-breaker
      // sweep mutates (player.js:132 aliases chars data — rule 12
      // net-restore):
      const marthHb = [];
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
        M.characterSelections[3] = 0; // marth
        const fresh = (mut) => {
          M.player[3] = new PJ.playerObject(0, [10, 20], 1);
          if (mut) mut(M.player[3]);
        };
        const T = AS.actionStates[0]; // marth's table
        const call = (name, phase, IN) => {
          T[name][phase](3, IN);
          n++;
        };
        const grab = (p3) => {
          p3.phys.grabbing = 3;
          p3.phys.grabbedBy = 3;
        };
        {
          fresh();
          const ch = M.player[3].charHitboxes;
          for (const mv of ["neutralspecialground", "neutralspecialair"]) {
            for (const id of ["id0", "id1", "id2", "id3"]) {
              marthHb.push([ch[mv][id], ch[mv][id].dmg]);
            }
          }
        }

        // jabs
        fresh(); call("JAB1", "init", INn);
        fresh((p) => { p.timer = 3; }); call("JAB1", "main", INn); // active 4
        fresh((p) => { p.timer = 5; }); call("JAB1", "main", INaPress);
        // ^ frame++ + jabCombo window (2<t<26, a-press)
        fresh((p) => { p.timer = 7; }); call("JAB1", "main", INn); // off 8
        fresh((p) => { p.timer = 20; p.phys.jabCombo = true; });
        call("JAB1", "interrupt", INn); // -> JAB2 (>19)
        fresh((p) => { p.timer = 28; });
        call("JAB1", "interrupt", INn); // WAIT (>27)
        fresh((p) => { p.timer = 27; });
        call("JAB1", "interrupt", INy); // KNEEBEND arm (>26)
        fresh((p) => { p.timer = 27; });
        call("JAB1", "interrupt", INbDown); // marth[b] special dispatch
        fresh((p) => { p.timer = 27; });
        call("JAB1", "interrupt", INbackStick); // TILTTURN tail arm
        fresh(); call("JAB2", "init", INn);
        fresh((p) => { p.timer = 4; }); call("JAB2", "main", INn); // active 5
        fresh((p) => { p.timer = 9; }); call("JAB2", "main", INn); // off 10
        fresh((p) => { p.timer = 20; p.phys.jabCombo = true; });
        call("JAB2", "interrupt", INn); // -> JAB1 (>19)
        fresh((p) => { p.timer = 29; });
        call("JAB2", "interrupt", INn); // WAIT (>28)
        fresh((p) => { p.timer = 28; });
        call("JAB2", "interrupt", INy); // KNEEBEND arm (>27)
        // tilts
        fresh(); call("UPTILT", "init", INn);
        fresh((p) => { p.timer = 5; }); call("UPTILT", "main", INn); // 6
        fresh((p) => { p.timer = 8; }); call("UPTILT", "main", INn); // 9 swap
        fresh((p) => { p.timer = 12; }); call("UPTILT", "main", INn); // 13 off
        fresh((p) => { p.timer = 40; });
        call("UPTILT", "interrupt", INn); // WAIT (>39)
        fresh((p) => { p.timer = 32; });
        call("UPTILT", "interrupt", INn); // checkFor false block (>31)
        fresh(); call("DOWNTILT", "init", INn);
        fresh((p) => { p.timer = 6; }); call("DOWNTILT", "main", INn); // 7
        fresh((p) => { p.timer = 9; }); call("DOWNTILT", "main", INn); // 10
        fresh((p) => { p.timer = 50; });
        call("DOWNTILT", "interrupt", INn); // SQUATWAIT (>49)
        fresh((p) => { p.timer = 20; });
        call("DOWNTILT", "interrupt", INbackStick); // inline TILTTURN arm
        fresh((p) => { p.timer = 20; });
        call("DOWNTILT", "interrupt", INstick); // inline WALK arm
        fresh(); call("FORWARDTILT", "init", INn);
        fresh((p) => { p.timer = 6; });
        call("FORWARDTILT", "main", INn); // active 7
        fresh((p) => { p.timer = 10; });
        call("FORWARDTILT", "main", INn); // off 11
        fresh((p) => { p.timer = 36; });
        call("FORWARDTILT", "interrupt", INn); // WAIT (>35)
        // smashes (charge machine at t==3/7/3)
        fresh(); call("FORWARDSMASH", "init", INn);
        fresh((p) => { p.timer = 3; p.phys.chargeFrames = 4; });
        call("FORWARDSMASH", "main", INa); // charge + smashcharge at 5
        fresh((p) => { p.timer = 3; p.phys.chargeFrames = 59; });
        call("FORWARDSMASH", "main", INa); // charge release at 60
        fresh((p) => { p.timer = 4; }); call("FORWARDSMASH", "main", INn);
        // ^ 5: sword3
        fresh((p) => { p.timer = 9; });
        call("FORWARDSMASH", "main", INn); // 10 active + randomShout
        fresh((p) => { p.timer = 13; });
        call("FORWARDSMASH", "main", INn); // 14 off
        fresh((p) => { p.timer = 50; });
        call("FORWARDSMASH", "interrupt", INn); // WAIT (>49)
        fresh((p) => { p.timer = 48; });
        call("FORWARDSMASH", "interrupt", INn); // checkFor block (>47)
        fresh(); call("UPSMASH", "init", INn);
        fresh((p) => { p.timer = 7; p.phys.chargeFrames = 4; });
        call("UPSMASH", "main", INa); // charge arm at 7
        fresh((p) => { p.timer = 12; });
        call("UPSMASH", "main", INn); // 13 active + sword3 + shout
        fresh((p) => { p.timer = 16; }); call("UPSMASH", "main", INn); // off
        fresh((p) => { p.timer = 55; });
        call("UPSMASH", "interrupt", INn); // WAIT (>54)
        fresh((p) => { p.timer = 46; });
        call("UPSMASH", "interrupt", INn); // checkFor block (>45)
        fresh(); call("DOWNSMASH", "init", INn);
        fresh((p) => { p.timer = 3; p.phys.chargeFrames = 4; });
        call("DOWNSMASH", "main", INa); // charge arm at 3
        fresh((p) => { p.timer = 4; });
        call("DOWNSMASH", "main", INn); // 5 active + shout
        fresh((p) => { p.timer = 7; }); call("DOWNSMASH", "main", INn); // 8
        fresh((p) => { p.timer = 19; });
        call("DOWNSMASH", "main", INn); // 20 dsmash2 swap
        fresh((p) => { p.timer = 22; }); call("DOWNSMASH", "main", INn); // 23
        fresh((p) => { p.timer = 65; });
        call("DOWNSMASH", "interrupt", INn); // WAIT (>64)
        fresh((p) => { p.timer = 62; });
        call("DOWNSMASH", "interrupt", INn); // !inCSS checkFor block
        fresh(); call("ATTACKDASH", "init", INn);
        fresh((p) => { p.timer = 11; });
        call("ATTACKDASH", "main", INn); // 12 active
        fresh((p) => { p.timer = 15; });
        call("ATTACKDASH", "main", INn); // 16 off
        fresh((p) => { p.timer = 1; p.phys.cVel.x = 99; });
        call("ATTACKDASH", "interrupt", INlA); // GRAB arm + dMaxV clamp
        fresh((p) => { p.timer = 40; });
        call("ATTACKDASH", "interrupt", INn); // checkFor block (>39)
        fresh((p) => { p.timer = 50; });
        call("ATTACKDASH", "interrupt", INn); // WAIT (>49)
        // aerials
        fresh(); call("ATTACKAIRN", "init", INn);
        fresh((p) => { p.timer = 5; });
        call("ATTACKAIRN", "main", INn); // 6 active + autoCancel off
        fresh((p) => { p.timer = 6; });
        call("ATTACKAIRN", "main", INn); // 7 frame++
        fresh((p) => { p.timer = 7; });
        call("ATTACKAIRN", "main", INn); // 8 off
        fresh((p) => { p.timer = 14; });
        call("ATTACKAIRN", "main", INn); // 15 nair2 swap
        fresh((p) => { p.timer = 21; });
        call("ATTACKAIRN", "main", INn); // 22 off
        fresh((p) => { p.timer = 24; });
        call("ATTACKAIRN", "main", INn); // 25 autoCancel
        fresh((p) => { p.timer = 50; });
        call("ATTACKAIRN", "interrupt", INn); // FALL (>49)
        fresh((p) => { p.phys.autoCancel = true; });
        call("ATTACKAIRN", "land", INn); // LANDING
        fresh((p) => { p.phys.autoCancel = false; });
        call("ATTACKAIRN", "land", INn); // LANDINGATTACKAIRN
        fresh(); call("ATTACKAIRF", "init", INn);
        fresh((p) => { p.timer = 3; });
        call("ATTACKAIRF", "main", INn); // 4 active
        fresh((p) => { p.timer = 7; });
        call("ATTACKAIRF", "main", INn); // 8 off
        fresh((p) => { p.timer = 26; });
        call("ATTACKAIRF", "main", INn); // 27 autoCancel
        fresh((p) => { p.timer = 34; });
        call("ATTACKAIRF", "interrupt", INn); // FALL (>33)
        fresh((p) => { p.timer = 30; });
        call("ATTACKAIRF", "interrupt", INy); // inline DJ -> JUMPAERIALF
        fresh((p) => { p.timer = 30; });
        call("ATTACKAIRF", "interrupt", INyBack); // inline DJ -> JUMPAERIALB
        fresh((p) => { p.timer = 30; });
        call("ATTACKAIRF", "interrupt", INaUp); // aerial payload ->
        // marth[a[1]].init (the inline analogue of checkForIASA's arm)
        fresh((p) => { p.phys.autoCancel = false; });
        call("ATTACKAIRF", "land", INn);
        fresh(); call("ATTACKAIRB", "init", INn);
        fresh((p) => { p.timer = 6; });
        call("ATTACKAIRB", "main", INn); // 7 active
        fresh((p) => { p.timer = 11; });
        call("ATTACKAIRB", "main", INn); // 12 off
        fresh((p) => { p.timer = 29; });
        call("ATTACKAIRB", "main", INn); // 30 face flip
        fresh((p) => { p.timer = 31; });
        call("ATTACKAIRB", "main", INn); // 32 autoCancel
        fresh((p) => { p.timer = 40; });
        call("ATTACKAIRB", "interrupt", INn); // FALL (>39)
        fresh((p) => { p.timer = 35; });
        call("ATTACKAIRB", "interrupt", INaUp); // aerial payload arm (>34)
        fresh((p) => { p.phys.autoCancel = false; });
        call("ATTACKAIRB", "land", INn);
        fresh(); call("ATTACKAIRU", "init", INn);
        fresh((p) => { p.timer = 4; });
        call("ATTACKAIRU", "main", INn); // 5 active + autoCancel off
        fresh((p) => { p.timer = 8; });
        call("ATTACKAIRU", "main", INn); // 9 off
        fresh((p) => { p.timer = 26; });
        call("ATTACKAIRU", "main", INn); // 27 autoCancel
        fresh((p) => { p.timer = 46; });
        call("ATTACKAIRU", "interrupt", INn); // FALL (>45)
        fresh((p) => { p.phys.autoCancel = false; });
        call("ATTACKAIRU", "land", INn);
        fresh(); call("ATTACKAIRD", "init", INn);
        fresh((p) => { p.timer = 5; });
        call("ATTACKAIRD", "main", INn); // 6 active
        fresh((p) => { p.timer = 9; });
        call("ATTACKAIRD", "main", INn); // 10 off
        fresh((p) => { p.timer = 47; });
        call("ATTACKAIRD", "main", INn); // 48 autoCancel
        fresh((p) => { p.timer = 60; });
        call("ATTACKAIRD", "interrupt", INn); // FALL (>59)
        fresh((p) => { p.phys.autoCancel = false; });
        call("ATTACKAIRD", "land", INn);
        // shield breaker (NEUTRALSPECIAL family): sbid + charge machine
        fresh(); call("NEUTRALSPECIALGROUND", "init", INn);
        fresh((p) => { p.timer = 6; });
        call("NEUTRALSPECIALGROUND", "main", INn); // 7 shieldbreaker1
        fresh((p) => { p.timer = 10; p.phys.cVel.x = 1; });
        call("NEUTRALSPECIALGROUND", "main", INn); // 11: play-id -> sbid
        // (+ the timer<12 sign-decel arm)
        fresh((p) => { p.timer = 12;
                       p.phys.shieldBreakerCharge = 5;
                       p.phys.shieldBreakerChargeAttempt = true;
                       p.phys.shieldBreakerCharging = false; });
        call("NEUTRALSPECIALGROUND", "main", INb); // B-held charge arm:
        // charge 5->6, blend overlay + dashDust (6 % 6 === 0)
        fresh((p) => { p.timer = 12;
                       p.phys.shieldBreakerCharge = 3;
                       p.phys.shieldBreakerChargeAttempt = true;
                       p.phys.shieldBreakerCharging = true; });
        call("NEUTRALSPECIALGROUND", "main", INn); // release-stop arm:
        // timer=42 + shieldbreakercharge.stop token
        fresh((p) => { p.timer = 41;
                       p.phys.shieldBreakerCharge = 30;
                       p.phys.shieldBreakerChargeAttempt = true;
                       p.phys.shieldBreakerCharging = true; });
        call("NEUTRALSPECIALGROUND", "main", INb); // charging loop arm:
        // timer 42 > 41 -> reset to 12
        fresh((p) => { p.timer = 12;
                       p.phys.shieldBreakerCharge = 121;
                       p.phys.shieldBreakerChargeAttempt = true;
                       p.phys.shieldBreakerCharging = true; });
        call("NEUTRALSPECIALGROUND", "main", INb); // 122 auto-stop arm
        fresh((p) => { p.timer = 42;
                       p.phys.shieldBreakerCharge = 0;
                       p.phys.shieldBreakerChargeAttempt = false;
                       p.phys.shieldBreakerCharging = false; });
        call("NEUTRALSPECIALGROUND", "main", INn); // 43 shout + sb2
        fresh((p) => { p.timer = 45;
                       p.phys.shieldBreakerCharge = 0;
                       p.phys.shieldBreakerChargeAttempt = false; });
        call("NEUTRALSPECIALGROUND", "main", INn); // 46: active + dmg 7
        // (uncharged) + sword3
        fresh((p) => { p.timer = 45;
                       p.phys.shieldBreakerCharge = 125;
                       p.phys.shieldBreakerChargeAttempt = false; });
        call("NEUTRALSPECIALGROUND", "main", INn); // 46 CHARGED: dmg 28 +
        // firestronghit (global charHitboxes dmg mutated — restored below)
        fresh((p) => { p.timer = 48;
                       p.phys.shieldBreakerCharge = 0;
                       p.phys.shieldBreakerChargeAttempt = false; });
        call("NEUTRALSPECIALGROUND", "main", INn); // 49 frame++
        fresh((p) => { p.timer = 49;
                       p.phys.shieldBreakerCharge = 125;
                       p.phys.shieldBreakerChargeAttempt = false; });
        call("NEUTRALSPECIALGROUND", "main", INn); // 50 groundBounce vfx
        fresh((p) => { p.timer = 51;
                       p.phys.shieldBreakerCharge = 0;
                       p.phys.shieldBreakerChargeAttempt = false; });
        call("NEUTRALSPECIALGROUND", "main", INn); // 52 off
        fresh((p) => { p.timer = 75; });
        call("NEUTRALSPECIALGROUND", "interrupt", INn); // WAIT (>74)
        fresh(); call("NEUTRALSPECIALAIR", "init", INn);
        fresh((p) => { p.timer = 10; p.phys.cVel.x = 1; p.phys.cVel.y = 1; });
        call("NEUTRALSPECIALAIR", "main", INn); // 11: play-id -> sbid +
        // gravity + 0.02 decel arm
        fresh((p) => { p.timer = 12;
                       p.phys.shieldBreakerCharge = 5;
                       p.phys.shieldBreakerChargeAttempt = true;
                       p.phys.shieldBreakerCharging = false; });
        call("NEUTRALSPECIALAIR", "main", INb); // charge + dashDust
        fresh((p) => { p.timer = 12;
                       p.phys.shieldBreakerCharge = 3;
                       p.phys.shieldBreakerChargeAttempt = true;
                       p.phys.shieldBreakerCharging = true; });
        call("NEUTRALSPECIALAIR", "main", INn); // release-stop arm
        fresh((p) => { p.timer = 45;
                       p.phys.shieldBreakerCharge = 0;
                       p.phys.shieldBreakerChargeAttempt = false; });
        call("NEUTRALSPECIALAIR", "main", INn); // 46 active + dmg 7
        fresh((p) => { p.timer = 45;
                       p.phys.shieldBreakerCharge = 125;
                       p.phys.shieldBreakerChargeAttempt = false; });
        call("NEUTRALSPECIALAIR", "main", INn); // 46 CHARGED (restored)
        fresh((p) => { p.timer = 51;
                       p.phys.shieldBreakerCharge = 0;
                       p.phys.shieldBreakerChargeAttempt = false; });
        call("NEUTRALSPECIALAIR", "main", INn); // 52 off
        fresh((p) => { p.timer = 75; });
        call("NEUTRALSPECIALAIR", "interrupt", INn); // FALL (>74)
        fresh(); call("NEUTRALSPECIALAIR", "land", INn); // state write
        // dolphin slash
        fresh(); call("UPSPECIAL", "init", INn);
        fresh((p) => { p.timer = 2; });
        call("UPSPECIAL", "main", INlsX08); // <6 angle-multiplier arm
        fresh((p) => { p.timer = 4; });
        call("UPSPECIAL", "main", INn); // 5: active window opens
        fresh((p) => { p.timer = 5; p.phys.grounded = true; });
        call("UPSPECIAL", "main", INbackStick); // 6: grounded=false +
        // face flip + upb2 swap + setVel[0]
        fresh((p) => { p.timer = 10;
                       p.phys.upbAngleMultiplier = 0.19634954084936207; });
        call("UPSPECIAL", "main", INn); // rotated setVel window + frame++
        fresh((p) => { p.timer = 21; });
        call("UPSPECIAL", "main", INn); // last setVel index (timer 22 -> 16)
        fresh((p) => { p.timer = 25; p.phys.cVel.x = 5; });
        call("UPSPECIAL", "main", INstick); // >22 drift + 0.36 clamp
        fresh((p) => { p.timer = 40; });
        call("UPSPECIAL", "interrupt", INn); // FALLSPECIAL (>39)
        fresh((p) => { p.phys.cVel.y = -1; });
        call("UPSPECIAL", "land", INn); // guard disjunct 1
        fresh((p) => { p.phys.cVel.y = 5; p.phys.kVel.y = 1;
                       p.phys.ECBp[0] = { x: 0, y: 5 };
                       p.phys.ECB1[0] = { x: 0, y: 10 };
                       p.phys.posPrev.y = -50; });
        call("UPSPECIAL", "land", INn); // guard disjunct 2 (ECBp <= ECB1)
        fresh((p) => { p.phys.cVel.y = 5; p.phys.kVel.y = 1;
                       p.phys.ECBp[0] = { x: 0, y: 10 };
                       p.phys.ECB1[0] = { x: 0, y: 5 };
                       p.phys.posPrev.y = -50; });
        call("UPSPECIAL", "land", INn); // guard all-false: NO init arm
        // counter (ground)
        fresh(); call("DOWNSPECIALGROUND", "init", INn);
        fresh((p) => { p.timer = 4; });
        call("DOWNSPECIALGROUND", "main", INn); // 5: white + marthcounter
        fresh((p) => { p.timer = 6; });
        call("DOWNSPECIALGROUND", "main", INn); // 7: %6<2 grey overlay
        fresh((p) => { p.timer = 8; });
        call("DOWNSPECIALGROUND", "main", INn); // 9: %6<4 purple overlay
        fresh((p) => { p.timer = 10; });
        call("DOWNSPECIALGROUND", "main", INn); // 11: %6>=4 overlay off
        fresh((p) => { p.timer = 29; });
        call("DOWNSPECIALGROUND", "main", INn); // 30 hitboxes off
        fresh((p) => { p.timer = 60; p.phys.grounded = true; });
        call("DOWNSPECIALGROUND", "interrupt", INn); // WAIT (>59)
        fresh((p) => { p.timer = 60; p.phys.grounded = false; });
        call("DOWNSPECIALGROUND", "interrupt", INn); // FALL
        fresh(); T.DOWNSPECIALGROUND.onClank(3, INn); n++; // -> DSG2 chain
        fresh(); call("DOWNSPECIALGROUND2", "init", INn);
        fresh((p) => { p.timer = 3; });
        call("DOWNSPECIALGROUND2", "main", INn); // 4 active
        fresh((p) => { p.timer = 6; });
        call("DOWNSPECIALGROUND2", "main", INn); // frame++
        fresh((p) => { p.timer = 10; });
        call("DOWNSPECIALGROUND2", "main", INn); // 11 off
        fresh((p) => { p.timer = 37; p.phys.grounded = true; });
        call("DOWNSPECIALGROUND2", "interrupt", INn); // WAIT (>36)
        fresh((p) => { p.timer = 37; p.phys.grounded = false; });
        call("DOWNSPECIALGROUND2", "interrupt", INn); // FALL
        // counter (air)
        fresh(); call("DOWNSPECIALAIR", "init", INn);
        fresh((p) => { p.timer = 4; p.phys.cVel.x = 1; p.phys.cVel.y = -2; });
        call("DOWNSPECIALAIR", "main", INn); // 5 white + decel arms
        fresh((p) => { p.timer = 8; });
        call("DOWNSPECIALAIR", "main", INn); // 9 purple
        fresh((p) => { p.timer = 29; });
        call("DOWNSPECIALAIR", "main", INn); // 30 off
        fresh((p) => { p.timer = 60; p.phys.grounded = false; });
        call("DOWNSPECIALAIR", "interrupt", INn); // FALL (>59)
        fresh(); call("DOWNSPECIALAIR", "land", INn); // state write
        fresh(); T.DOWNSPECIALAIR.onClank(3, INn); n++; // -> DSA2 chain
        fresh(); call("DOWNSPECIALAIR2", "init", INn);
        fresh((p) => { p.timer = 3; p.phys.cVel.x = 1; });
        call("DOWNSPECIALAIR2", "main", INn); // 4 active + unclamped decel
        fresh((p) => { p.timer = 10; });
        call("DOWNSPECIALAIR2", "main", INn); // 11 off
        fresh((p) => { p.timer = 37; p.phys.grounded = false; });
        call("DOWNSPECIALAIR2", "interrupt", INn); // FALL (>36)
        // dancing blade (ground chain)
        fresh(); call("SIDESPECIALGROUND", "init", INn);
        fresh((p) => { p.timer = 5; });
        call("SIDESPECIALGROUND", "main", INn); // 6 active + dancingBlade
        fresh((p) => { p.timer = 8; });
        call("SIDESPECIALGROUND", "main", INn); // 9 off
        fresh((p) => { p.timer = 5; p.phys.dancingBlade = false;
                       p.phys.dancingBladeDisable = false; });
        call("SIDESPECIALGROUND", "main", INaPress); // combo t<8: DISABLE arm
        fresh((p) => { p.timer = 9; p.phys.dancingBlade = false;
                       p.phys.dancingBladeDisable = false; });
        call("SIDESPECIALGROUND", "main", INbPress); // combo 8<=t<=26:
        // dancingBlade arm (b-press, disable false)
        fresh((p) => { p.timer = 9; p.phys.dancingBlade = false;
                       p.phys.dancingBladeDisable = true; });
        call("SIDESPECIALGROUND", "main", INbPress); // b-press DISABLED: no-op
        fresh((p) => { p.timer = 5; p.phys.dancingBlade = true; });
        call("SIDESPECIALGROUND", "interrupt", INup); // -> SSG2UP
        fresh((p) => { p.timer = 5; p.phys.dancingBlade = true; });
        call("SIDESPECIALGROUND", "interrupt", INn); // -> SSG2FORWARD
        fresh((p) => { p.timer = 30; p.phys.grounded = true; });
        call("SIDESPECIALGROUND", "interrupt", INn); // WAIT (>29)
        fresh((p) => { p.timer = 30; p.phys.grounded = false; });
        call("SIDESPECIALGROUND", "interrupt", INn); // FALL
        fresh(); call("SIDESPECIALGROUND2FORWARD", "init", INn);
        fresh((p) => { p.timer = 13; });
        call("SIDESPECIALGROUND2FORWARD", "main", INn); // 14 active
        fresh((p) => { p.timer = 16; });
        call("SIDESPECIALGROUND2FORWARD", "main", INn); // 17 off
        fresh((p) => { p.timer = 5; p.phys.dancingBlade = true; });
        call("SIDESPECIALGROUND2FORWARD", "interrupt", INup); // -> SSG3UP
        fresh((p) => { p.timer = 5; p.phys.dancingBlade = true; });
        call("SIDESPECIALGROUND2FORWARD", "interrupt", INdown); // -> SSG3DOWN
        fresh((p) => { p.timer = 5; p.phys.dancingBlade = true; });
        call("SIDESPECIALGROUND2FORWARD", "interrupt", INn); // -> SSG3FORWARD
        fresh((p) => { p.timer = 41; });
        call("SIDESPECIALGROUND2FORWARD", "interrupt", INn); // WAIT (>40)
        fresh(); call("SIDESPECIALGROUND2UP", "init", INn);
        fresh((p) => { p.timer = 11; });
        call("SIDESPECIALGROUND2UP", "main", INn); // 12 active
        fresh((p) => { p.timer = 15; });
        call("SIDESPECIALGROUND2UP", "main", INn); // 16 off
        fresh((p) => { p.timer = 5; p.phys.dancingBlade = true; });
        call("SIDESPECIALGROUND2UP", "interrupt", INn); // -> SSG3FORWARD
        fresh((p) => { p.timer = 41; });
        call("SIDESPECIALGROUND2UP", "interrupt", INn); // WAIT (>40)
        fresh(); call("SIDESPECIALGROUND3FORWARD", "init", INn);
        fresh((p) => { p.timer = 10; });
        call("SIDESPECIALGROUND3FORWARD", "main", INn); // 11 active + setVel
        fresh((p) => { p.timer = 14; });
        call("SIDESPECIALGROUND3FORWARD", "main", INn); // 15 off
        fresh((p) => { p.timer = 5; p.phys.dancingBlade = true; });
        call("SIDESPECIALGROUND3FORWARD", "interrupt", INup); // -> SSG4UP
        fresh((p) => { p.timer = 5; p.phys.dancingBlade = true; });
        call("SIDESPECIALGROUND3FORWARD", "interrupt", INdown); // -> SSG4DOWN
        fresh((p) => { p.timer = 5; p.phys.dancingBlade = true; });
        call("SIDESPECIALGROUND3FORWARD", "interrupt", INn); // -> SSG4FORWARD
        fresh((p) => { p.timer = 47; });
        call("SIDESPECIALGROUND3FORWARD", "interrupt", INn); // WAIT (>46)
        fresh(); call("SIDESPECIALGROUND3UP", "init", INn);
        fresh((p) => { p.timer = 12; });
        call("SIDESPECIALGROUND3UP", "main", INn); // 13 active
        fresh((p) => { p.timer = 17; });
        call("SIDESPECIALGROUND3UP", "main", INn); // 18 off
        fresh((p) => { p.timer = 5; p.phys.dancingBlade = true; });
        call("SIDESPECIALGROUND3UP", "interrupt", INn); // -> SSG4FORWARD
        fresh((p) => { p.timer = 47; });
        call("SIDESPECIALGROUND3UP", "interrupt", INn); // WAIT (>46)
        fresh(); call("SIDESPECIALGROUND3DOWN", "init", INn);
        fresh((p) => { p.timer = 14; });
        call("SIDESPECIALGROUND3DOWN", "main", INn); // 15 active
        fresh((p) => { p.timer = 18; });
        call("SIDESPECIALGROUND3DOWN", "main", INn); // 19 off
        fresh((p) => { p.timer = 5; p.phys.dancingBlade = true; });
        call("SIDESPECIALGROUND3DOWN", "interrupt", INn); // -> SSG4FORWARD
        fresh((p) => { p.timer = 47; });
        call("SIDESPECIALGROUND3DOWN", "interrupt", INn); // WAIT (>46)
        fresh(); call("SIDESPECIALGROUND4FORWARD", "init", INn);
        fresh((p) => { p.timer = 22; });
        call("SIDESPECIALGROUND4FORWARD", "main", INn); // 23 active
        fresh((p) => { p.timer = 26; });
        call("SIDESPECIALGROUND4FORWARD", "main", INn); // 27 off
        fresh((p) => { p.timer = 51; });
        call("SIDESPECIALGROUND4FORWARD", "interrupt", INn); // WAIT (>50)
        fresh(); call("SIDESPECIALGROUND4UP", "init", INn);
        fresh((p) => { p.timer = 19; });
        call("SIDESPECIALGROUND4UP", "main", INn); // 20 active
        fresh((p) => { p.timer = 25; });
        call("SIDESPECIALGROUND4UP", "main", INn); // 26 off
        fresh((p) => { p.timer = 51; });
        call("SIDESPECIALGROUND4UP", "interrupt", INn); // WAIT (>50)
        fresh(); call("SIDESPECIALGROUND4DOWN", "init", INn);
        fresh((p) => { p.timer = 12; });
        call("SIDESPECIALGROUND4DOWN", "main", INn); // 13 %6==1: hb1 +
        // dancingBlade2 + shout6 (13 < 37)
        fresh((p) => { p.timer = 36; });
        call("SIDESPECIALGROUND4DOWN", "main", INn); // 37 %6==1: hb5,
        // NO shout6 (37 < 37 false)
        fresh((p) => { p.timer = 13; });
        call("SIDESPECIALGROUND4DOWN", "main", INn); // 14 %6==2 frame++
        fresh((p) => { p.timer = 15; });
        call("SIDESPECIALGROUND4DOWN", "main", INn); // 16 %6==4 off
        fresh((p) => { p.timer = 38; });
        call("SIDESPECIALGROUND4DOWN", "main", INn); // 39 final off
        fresh((p) => { p.timer = 61; });
        call("SIDESPECIALGROUND4DOWN", "interrupt", INn); // WAIT (>60)
        // dancing blade (air chain)
        fresh((p) => { p.phys.grounded = false;
                       p.phys.sideBJumpFlag = true; });
        call("SIDESPECIALAIR", "init", INn); // jump-flag arm
        fresh((p) => { p.phys.grounded = false;
                       p.phys.sideBJumpFlag = false; });
        call("SIDESPECIALAIR", "init", INn); // air, no flag
        fresh((p) => { p.phys.grounded = true; });
        call("SIDESPECIALAIR", "init", INn); // grounded arm
        fresh((p) => { p.timer = 5; });
        call("SIDESPECIALAIR", "main", INn); // 6 active + dancingBlade
        fresh((p) => { p.timer = 8; });
        call("SIDESPECIALAIR", "main", INn); // 9 off
        fresh((p) => { p.timer = 5; p.phys.dancingBlade = true; });
        call("SIDESPECIALAIR", "interrupt", INup); // -> SSA2UP
        fresh((p) => { p.timer = 5; p.phys.dancingBlade = true; });
        call("SIDESPECIALAIR", "interrupt", INn); // -> SSA2FORWARD
        fresh((p) => { p.timer = 30; p.phys.grounded = false; });
        call("SIDESPECIALAIR", "interrupt", INn); // FALL (>29)
        fresh(); call("SIDESPECIALAIR", "land", INn); // state write
        fresh(); call("SIDESPECIALAIR2FORWARD", "init", INn);
        fresh((p) => { p.timer = 13; });
        call("SIDESPECIALAIR2FORWARD", "main", INn); // 14 active
        fresh((p) => { p.timer = 16; });
        call("SIDESPECIALAIR2FORWARD", "main", INn); // 17 off
        fresh((p) => { p.timer = 5; p.phys.dancingBlade = true; });
        call("SIDESPECIALAIR2FORWARD", "interrupt", INup); // -> SSA3UP
        fresh((p) => { p.timer = 5; p.phys.dancingBlade = true; });
        call("SIDESPECIALAIR2FORWARD", "interrupt", INdown); // -> SSA3DOWN
        fresh((p) => { p.timer = 5; p.phys.dancingBlade = true; });
        call("SIDESPECIALAIR2FORWARD", "interrupt", INn); // -> SSA3FORWARD
        fresh((p) => { p.timer = 41; p.phys.grounded = false; });
        call("SIDESPECIALAIR2FORWARD", "interrupt", INn); // FALL (>40)
        fresh(); call("SIDESPECIALAIR2FORWARD", "land", INn);
        fresh(); call("SIDESPECIALAIR2UP", "init", INn);
        fresh((p) => { p.timer = 11; });
        call("SIDESPECIALAIR2UP", "main", INn); // 12 active
        fresh((p) => { p.timer = 15; });
        call("SIDESPECIALAIR2UP", "main", INn); // 16 off
        fresh((p) => { p.timer = 5; p.phys.dancingBlade = true; });
        call("SIDESPECIALAIR2UP", "interrupt", INn); // -> SSA3FORWARD
        fresh((p) => { p.timer = 41; });
        call("SIDESPECIALAIR2UP", "interrupt", INn); // WAIT/FALL (>40)
        fresh(); call("SIDESPECIALAIR2UP", "land", INn);
        fresh(); call("SIDESPECIALAIR3FORWARD", "init", INn);
        fresh((p) => { p.timer = 10; });
        call("SIDESPECIALAIR3FORWARD", "main", INn); // 11 active + cVel 0
        fresh((p) => { p.timer = 14; });
        call("SIDESPECIALAIR3FORWARD", "main", INn); // 15 off
        fresh((p) => { p.timer = 5; p.phys.dancingBlade = true; });
        call("SIDESPECIALAIR3FORWARD", "interrupt", INup); // -> SSA4UP
        fresh((p) => { p.timer = 5; p.phys.dancingBlade = true; });
        call("SIDESPECIALAIR3FORWARD", "interrupt", INdown); // -> SSA4DOWN
        fresh((p) => { p.timer = 5; p.phys.dancingBlade = true; });
        call("SIDESPECIALAIR3FORWARD", "interrupt", INn); // -> SSA4FORWARD
        fresh((p) => { p.timer = 47; });
        call("SIDESPECIALAIR3FORWARD", "interrupt", INn); // (>46)
        fresh(); call("SIDESPECIALAIR3FORWARD", "land", INn);
        fresh(); call("SIDESPECIALAIR3UP", "init", INn);
        fresh((p) => { p.timer = 12; });
        call("SIDESPECIALAIR3UP", "main", INn); // 13 active
        fresh((p) => { p.timer = 17; });
        call("SIDESPECIALAIR3UP", "main", INn); // 18 off
        fresh((p) => { p.timer = 5; p.phys.dancingBlade = true; });
        call("SIDESPECIALAIR3UP", "interrupt", INn); // -> SSA4FORWARD
        fresh((p) => { p.timer = 47; });
        call("SIDESPECIALAIR3UP", "interrupt", INn); // (>46)
        fresh(); call("SIDESPECIALAIR3UP", "land", INn);
        fresh(); call("SIDESPECIALAIR3DOWN", "init", INn);
        fresh((p) => { p.timer = 14; });
        call("SIDESPECIALAIR3DOWN", "main", INn); // 15 active
        fresh((p) => { p.timer = 18; });
        call("SIDESPECIALAIR3DOWN", "main", INn); // 19 off
        fresh((p) => { p.timer = 5; p.phys.dancingBlade = true; });
        call("SIDESPECIALAIR3DOWN", "interrupt", INn); // -> SSA4FORWARD
        fresh((p) => { p.timer = 47; });
        call("SIDESPECIALAIR3DOWN", "interrupt", INn); // (>46)
        fresh(); call("SIDESPECIALAIR3DOWN", "land", INn);
        fresh(); call("SIDESPECIALAIR4FORWARD", "init", INn);
        fresh((p) => { p.timer = 22; });
        call("SIDESPECIALAIR4FORWARD", "main", INn); // 23 active
        fresh((p) => { p.timer = 26; });
        call("SIDESPECIALAIR4FORWARD", "main", INn); // 27 off
        fresh((p) => { p.timer = 51; });
        call("SIDESPECIALAIR4FORWARD", "interrupt", INn); // (>50)
        fresh(); call("SIDESPECIALAIR4FORWARD", "land", INn);
        fresh(); call("SIDESPECIALAIR4UP", "init", INn);
        fresh((p) => { p.timer = 19; });
        call("SIDESPECIALAIR4UP", "main", INn); // 20 active
        fresh((p) => { p.timer = 25; });
        call("SIDESPECIALAIR4UP", "main", INn); // 26 off
        fresh((p) => { p.timer = 51; });
        call("SIDESPECIALAIR4UP", "interrupt", INn); // (>50)
        fresh(); call("SIDESPECIALAIR4UP", "land", INn);
        fresh(); call("SIDESPECIALAIR4DOWN", "init", INn);
        fresh((p) => { p.timer = 12; });
        call("SIDESPECIALAIR4DOWN", "main", INn); // 13 %6==1 + shout6
        fresh((p) => { p.timer = 36; });
        call("SIDESPECIALAIR4DOWN", "main", INn); // 37 %6==1 no shout6
        fresh((p) => { p.timer = 13; });
        call("SIDESPECIALAIR4DOWN", "main", INn); // 14 frame++
        fresh((p) => { p.timer = 15; });
        call("SIDESPECIALAIR4DOWN", "main", INn); // 16 off
        fresh((p) => { p.timer = 38; });
        call("SIDESPECIALAIR4DOWN", "main", INn); // 39 final off
        fresh((p) => { p.timer = 61; });
        call("SIDESPECIALAIR4DOWN", "interrupt", INn); // (>60)
        fresh(); call("SIDESPECIALAIR4DOWN", "land", INn);
        // throws (marth: 2-arg victim dispatch, self-grab)
        fresh(); call("THROWUP", "init", INn); // init guard arm
        fresh(grab); call("THROWUP", "init", INn); // THROWNMARTHUP chain +
        // shout (nested marth victim: transparent tree)
        fresh((p) => { grab(p); p.phys.releaseFrame = 12; p.timer = 11.5; });
        call("THROWUP", "main", INn); // hq push crossing 12
        fresh((p) => { p.phys.grabbing = 3; p.phys.grabbedBy = 1;
                       p.phys.releaseFrame = 12; p.timer = 1; });
        call("THROWUP", "interrupt", INn); // CATCHCUT (victim's grabbedBy
        // mismatch, timer < 11)
        fresh((p) => { p.phys.grabbing = 3; p.timer = 40; });
        call("THROWUP", "interrupt", INn); // >39 WAIT arm
        fresh((p) => { p.phys.grabbing = -1; p.timer = 1; });
        call("THROWUP", "interrupt", INn); // -1 arm: returns FALSE
        fresh(); call("THROWFORWARD", "init", INn); // guard arm
        fresh(grab); call("THROWFORWARD", "init", INn); // 2-arg dispatch
        fresh((p) => { grab(p); p.phys.releaseFrame = 13; p.timer = 12.5; });
        call("THROWFORWARD", "main", INn); // hq push crossing 13
        fresh((p) => { p.phys.grabbing = 3; p.timer = 28; });
        call("THROWFORWARD", "interrupt", INn); // >27 WAIT arm
        fresh((p) => { p.phys.grabbing = -1; p.timer = 1; });
        call("THROWFORWARD", "interrupt", INn); // -1 bare-return arm
        fresh(); call("THROWBACK", "init", INn); // guard arm
        fresh(grab); call("THROWBACK", "init", INn);
        fresh((p) => { grab(p); p.phys.releaseFrame = 7; p.timer = 6.5; });
        call("THROWBACK", "main", INn); // hq push crossing 7
        fresh((p) => { p.phys.grabbing = 3; p.timer = 40; });
        call("THROWBACK", "interrupt", INn); // >39 WAIT arm
        fresh((p) => { p.phys.grabbing = -1; p.timer = 1; });
        call("THROWBACK", "interrupt", INn); // -1 bare-return arm
        fresh(); call("THROWDOWN", "init", INn); // guard arm
        fresh(grab); call("THROWDOWN", "init", INn);
        fresh((p) => { grab(p); p.phys.releaseFrame = 13; p.timer = 12.5; });
        call("THROWDOWN", "main", INn); // hq push crossing 13
        fresh((p) => { p.phys.grabbing = 3; p.timer = 38; });
        call("THROWDOWN", "interrupt", INn); // >37 WAIT arm
        fresh((p) => { p.phys.grabbing = -1; p.timer = 1; });
        call("THROWDOWN", "interrupt", INn); // -1 bare-return arm
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
        // grabbedBy === -1 init guard — the GUARDED family minus
        // THROWNMARTHBACK (which has NO guard: player[-1] throws upstream)
        fresh(); call("THROWNMARTHUP", "init", INn);
        fresh(); call("THROWNPUFFUP", "init", INn);
        fresh(); call("THROWNFOXFORWARD", "init", INn);
        // main guard + clamp arms
        fresh((p) => { p.phys.grabbedBy = -1; p.timer = 1; });
        call("THROWNMARTHUP", "main", INn); // clamp-then-guard order
        fresh((p) => { p.phys.grabbedBy = -1; p.timer = 1; });
        call("THROWNMARTHDOWN", "main", INn); // guard-then-clamp order
        fresh((p) => { p.phys.grabbedBy = -1; p.timer = 1; });
        call("THROWNPUFFUP", "main", INn); // the vacuous-phys wrapper arm
        fresh((p) => { p.phys.grabbedBy = 3; p.timer = 2; });
        call("THROWNMARTHUP", "main", INn); // in-range pos arm
        fresh((p) => { p.phys.grabbedBy = 3; p.timer = 12; });
        call("THROWNMARTHUP", "main", INn); // clamp arm (offset len 11)
        fresh((p) => { p.phys.grabbedBy = 3; p.timer = 2; });
        call("THROWNMARTHBACK", "main", INn); // no-init-guard file's main
        fresh((p) => { p.phys.grabbedBy = 3; p.timer = 8; });
        call("THROWNPUFFBACK", "main", INn); // face*-1 offset arm
        fresh((p) => { p.phys.grabbedBy = 3; p.timer = 23; });
        call("THROWNPUFFBACK", "main", INn); // clamp arm (len 22)
        fresh((p) => { p.phys.grabbedBy = 3; p.timer = 2; });
        call("THROWNFOXBACK", "main", INn); // face*-1 guarded fox-back
        fresh((p) => { p.phys.grabbedBy = 3; p.timer = 2; });
        call("THROWNFALCOUP", "main", INn); // unguarded family main
        // grabbedBy < p (timer = -1) arm: inject slot 2
        M.playerType[2] = 0;
        M.player[2] = new PJ.playerObject(0, [30, 20], 1);
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
        fresh(cliff(10)); call("CLIFFGETUPQUICK", "main", INn); // pos arm
        fresh(cliff(15)); call("CLIFFGETUPQUICK", "main", INn); // grounding 16
        fresh(cliff(20)); call("CLIFFGETUPQUICK", "main", INn); // pos.x arm
        fresh(cliff(33)); call("CLIFFGETUPQUICK", "interrupt", INn); // regrab
        // TRUE (>32) — this file alone
        fresh(cliff(5)); call("CLIFFGETUPSLOW", "main", INn); // fixed hold
        fresh(cliff(20)); call("CLIFFGETUPSLOW", "main", INn); // offset arm
        fresh(cliff(44)); call("CLIFFGETUPSLOW", "main", INn); // grounding 45
        fresh(cliff(50)); call("CLIFFGETUPSLOW", "main", INn); // setVel arm
        fresh(cliff(59)); call("CLIFFGETUPSLOW", "interrupt", INn); // >58
        fresh(cliff(10)); call("CLIFFESCAPEQUICK", "main", INn); // pos arm
        fresh(cliff(16)); call("CLIFFESCAPEQUICK", "main", INn); // grounding
        fresh(cliff(20)); call("CLIFFESCAPEQUICK", "main", INn); // setVel
        fresh(cliff(49)); call("CLIFFESCAPEQUICK", "interrupt", INn); // >48
        fresh(cliff(5)); call("CLIFFESCAPESLOW", "main", INn); // fixed hold
        fresh(cliff(15)); call("CLIFFESCAPESLOW", "main", INn); // offset arm
        fresh(cliff(26)); call("CLIFFESCAPESLOW", "main", INn); // grounding 27
        fresh(cliff(30)); call("CLIFFESCAPESLOW", "main", INn); // setVel
        fresh(cliff(79)); call("CLIFFESCAPESLOW", "interrupt", INn); // >78
        fresh(cliff(5)); call("CLIFFJUMPQUICK", "main", INn); // pos arm
        fresh(cliff(11)); call("CLIFFJUMPQUICK", "main", INn); // cVel at 12
        fresh(cliff(13)); call("CLIFFJUMPQUICK", "main", INn); // drift arm
        fresh(cliff(51)); call("CLIFFJUMPQUICK", "interrupt", INn); // FALL >50
        fresh(cliff(10)); call("CLIFFJUMPSLOW", "main", INn); // pos arm
        fresh(cliff(18)); call("CLIFFJUMPSLOW", "main", INn); // cVel at 19
        fresh(cliff(20)); call("CLIFFJUMPSLOW", "main", INn); // drift arm
        fresh(cliff(58)); call("CLIFFJUMPSLOW", "interrupt", INn); // FALL >57
        fresh(cliff(10)); call("CLIFFATTACKQUICK", "main", INn); // pos arm
        fresh(cliff(16)); call("CLIFFATTACKQUICK", "main", INn); // grounding
        fresh(cliff(23)); call("CLIFFATTACKQUICK", "main", INn); // 24 active
        // + normalswing2 + randomShout + setVel
        fresh(cliff(25)); call("CLIFFATTACKQUICK", "main", INn); // frame++
        fresh(cliff(27)); call("CLIFFATTACKQUICK", "main", INn); // 28 off
        fresh(cliff(55)); call("CLIFFATTACKQUICK", "interrupt", INn); // >54
        fresh(cliff(5)); call("CLIFFATTACKSLOW", "main", INn); // fixed hold
        fresh(cliff(20)); call("CLIFFATTACKSLOW", "main", INn); // offset arm
        fresh(cliff(31)); call("CLIFFATTACKSLOW", "main", INn); // grounding 32
        fresh(cliff(37)); call("CLIFFATTACKSLOW", "main", INn); // 38 active
        fresh(cliff(40)); call("CLIFFATTACKSLOW", "main", INn); // frame++
        fresh(cliff(41)); call("CLIFFATTACKSLOW", "main", INn); // 42 off
        fresh(cliff(69)); call("CLIFFATTACKSLOW", "interrupt", INn); // >68
        // grab family + downattack + appeal
        fresh(); call("GRAB", "init", INn);
        fresh((p) => { p.timer = 6; }); call("GRAB", "main", INn); // 7
        fresh((p) => { p.timer = 8; }); call("GRAB", "main", INn); // 9 off
        fresh((p) => { p.timer = 31; });
        call("GRAB", "interrupt", INn); // WAIT (>30)
        fresh(); call("CATCHATTACK", "init", INn);
        fresh((p) => { p.timer = 5; });
        call("CATCHATTACK", "main", INn); // 6 active
        fresh((p) => { p.timer = 6; });
        call("CATCHATTACK", "main", INn); // 7 off
        fresh((p) => { p.timer = 25; });
        call("CATCHATTACK", "interrupt", INn); // CATCHWAIT (>24)
        fresh(); call("DOWNATTACK", "init", INn);
        fresh((p) => { p.timer = 0; });
        call("DOWNATTACK", "main", INn); // 1 intangibleTimer
        fresh((p) => { p.timer = 19; });
        call("DOWNATTACK", "main", INn); // 20 active
        fresh((p) => { p.timer = 23; });
        call("DOWNATTACK", "main", INn); // 24 off
        fresh((p) => { p.timer = 29; });
        call("DOWNATTACK", "main", INn); // 30 downattack2 swap
        fresh((p) => { p.timer = 31; });
        call("DOWNATTACK", "main", INn); // 32 off
        fresh((p) => { p.timer = 50; });
        call("DOWNATTACK", "interrupt", INn); // WAIT (>49)
        fresh(); call("APPEAL", "init", INn);
        fresh((p) => { p.timer = 1; }); call("APPEAL", "main", INn); // 2 snd
        fresh((p) => { p.timer = 94; });
        call("APPEAL", "interrupt", INn); // WAIT (>93)
      } finally {
        Math.random = savedRandom;
        M.player[3] = savedP3;
        M.playerType[3] = savedT3;
        M.player[2] = savedP2;
        M.playerType[2] = savedT2;
        M.characterSelections[3] = savedCS3;
        // restore the global charHitboxes dmg fields the charged
        // shield-breaker sweep mutated (rule 12 net-restore)
        for (const [hb, dmg] of marthHb) hb.dmg = dmg;
        // net-restore the article/hq globals the sweep touched (rule 12)
        ART.aArticles.splice(savedArticles);
        HD.hitQueue.splice(savedHq);
      }
      return n;
    },
  };
})();
