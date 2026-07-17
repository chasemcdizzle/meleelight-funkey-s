// spec-moves-puff.js — M2 task 12 capture spec: characters/puff moves
// (fifth and LAST per-char cluster; task 8's recipe — see spec-moves-fox.js
// for the record-type documentation, which applies with fox->puff plus the
// deltas below). Registers window.__capSpecs["moves-puff"]. Injected AFTER
// capturelib.js. See FORMAT.md "The moves-puff spec".
//
// BOUNDARY: every function property of every PUFF-ORIGIN actionStates
// entry (fn identity vs the characters/puff/moves module index — rule 15's
// instrument). Puff-origin entries exist ONLY on table 1: 71 module keys —
// 56 moves x {init,main,interrupt} + 9 x {+land} (ATTACKAIR*x5,
// DOWNSPECIALAIR, DOWNSPECIALGROUND, SIDESPECIALAIR, UPSPECIAL) = 204, plus
// NEUTRALSPECIALGROUND {init,main,interrupt} = 3, NEUTRALSPECIALAIR
// {init,main,interrupt,land} = 4, NEUTRALSPECIALGROUNDTURN
// {init,main,interrupt} = 3, and the three single-init modules FURAFURA /
// JUMPAERIALB / JUMPAERIALF = 3 -> 217 phase fns, PLUS puff's four
// NON-phase move fns: onPlayerHit(p) on NEUTRALSPECIAL{GROUND,AIR,
// GROUNDTURN} (hitDetection.js:493's specialOnHit arm) and
// onWallCollide(p, input, wallFace, wallNum) on NEUTRALSPECIALAIR
// (physics.js:122's specialWallCollide arm) = 221 puff fns, plus the 2
// article init boundaries (measurement only — puff has ZERO `articles`
// references anywhere under characters/puff/, marth-strength; the article
// count is pinned ZERO) = 223 wrapped.
//
// TABLE OVERRIDES (rule 15's origin map at work): puff's module index
// carries FURAFURA/JUMPAERIALB/JUMPAERIALF, which OVERRIDE the shared
// states on table 1 ({...baseActionStates, ...puffMoves}) — fn identity
// classifies them puff-origin there while every other table keeps the
// shared fns (task 7's surface). Puff's FURAFURA override is TRIVIAL
// (WAIT.init only — no furaloop, no furaLoopID: the shared FURAFURA's
// Howl-id chain state never enters this cluster; measured, so the task-11
// sbid mechanism is NOT needed here — no puff move consumes a Howl id).
//
// THE chd PRE-PROJECTION (NEW, task 12 — the char-data VALUE plane made
// oracle-fed): puff's moves WRITE fields of the GLOBAL charHitboxes
// objects through player[p].hitboxes.id aliases (player.js:132 aliases
// chars data, no copy): NEUTRALSPECIAL{GROUND,AIR} write id[0..2].dmg
// after release EVEN WHEN UNCHARGED — through whatever STALE id objects
// the previous move assigned (cross-move provenance: jab1/thrown/upb/...)
// — and UPSPECIAL (sing) writes id[0].size at t 28/36/69/77/113. Unlike
// marth's benign charge<30 domain, puff's live domain genuinely DRIFTS
// the plane (newDmg 7..18 varies with charge; sing sizes cycle). Instead
// of task 11's documented-hole, every move record's pre envelope carries
// "chd": the EXECUTED {moveKey: {idN: {dmg, size}}} plane of the puff
// char at record time — the C's pf_assign_hitbox_id consumes dmg/size
// from chd (the other 10 createHitbox fields have NO write sites
// anywhere upstream — measured by grep over characters/+physics/+main/ —
// and stay CTAB1's). The plane's evolution is thus measured per record,
// never assumed pristine.
//
// PUFF DELTAS (measured per-file diffs vs marth/fox/falco/falcon):
// - ROLLOUT (NEUTRALSPECIAL{GROUND,AIR} + GROUNDTURN): a movement special
//   with its own charge/launch/turn state machine on runtime-added
//   phys.rollOut* fields (modeled since task 2; rollOutTurnTimer added
//   THIS task — rule 16). NSG/NSA interrupts ALWAYS return false (their
//   WAIT/FALLSPECIAL arms fire the init and still return false —
//   verbatim); NSGT's interrupt returns true from both exit arms.
// - SING (UPSPECIAL): 3 hitbox windows via id[0].size writes (the chd
//   plane); land is an EMPTY function (present, does nothing).
// - REST (DOWNSPECIAL{GROUND,AIR}): identical twins; 1-frame hitbox +
//   intangibleTimer 26; land is a comment-only body.
// - POUND (SIDESPECIAL{GROUND,AIR}): identical twins except AIR carries
//   land (a bare actionState write); air arc rotates airVelocities by
//   phys.upbAngleMultiplier = lsY*PI*(20/180) via sin/cos (fdlibm).
// - Multijump family: AERIALTURN1-5 / JUMPAERIAL1-5 ladder driven by
//   phys.jumpsUsed through the puffNextJump/puffMultiJumpDrift helper
//   modules (characters/puff/*.js — the dancing-blade-helpers analogue);
//   JUMPAERIALB/F are init-only puffNextJump delegates (the table
//   overrides). ATTACKAIRN's t===7 arm increments hitboxes.FRAMES (the
//   runtime-added plural — modeled since task 2) and ATTACKAIRB's t===8
//   arm writes phys.AUTOCANCEL (lowercase — ditto): upstream typos
//   carried verbatim.
// - THROW*: victims dispatched 2-ARG via the TABLE
//   (actionStates[cs[grabbing]].THROWNPUFF*.init(grabbing, input));
//   fractional timers (timer += K/releaseFrame) with Math.floor(+0.01)
//   crossings; THROWBACK's window condition contains upstream's
//   floor-over-comparison typo (`Math.floor(timer + 0.01 < 37)`) —
//   carried verbatim; interrupts' grabbing===-1 arms bare-return
//   (undefined, rule 13).
// - THROWN{PUFF,MARTH,FOX}* are GUARDED (init -1 guard + main -1 guard +
//   len clamp; THROWNPUFFUP nests its guard under a vacuous
//   `if(player[p].phys)` with NO early return; THROWNMARTHFORWARD clamps
//   BEFORE the -1 guard; THROWNMARTH* snap pos to the grabber in init,
//   THROWNPUFF*/THROWNFOX* do not); THROWN{FALCO,FALCON}* are the
//   old-style `this.`-dispatch UNGUARDED family (init pos snap;
//   player[-1] throws upstream — traps); THROWNPUFFBACK's offsetVel arm
//   is COMMENTED OUT upstream (dead data, still dumped).
// - CLIFF*: all 8 keep the onLedge===-1 `this.canGrabLedge = false`
//   table-write arm (C traps; mvData finalCheck guards);
//   CLIFFGETUPQUICK sets ledgeRegrabCount = TRUE (others false).
(() => {
  const EXCLUDE = ["charAttributes", "charHitboxes", "percentShake"];
  const SND_KEYS = ["CLIFFCATCH", "DEAD", "ESCAPEAIR", "ESCAPEB", "ESCAPEF",
                    "ESCAPEN", "FURAFURA", "GUARDOFF", "GUARDON", "JUMP",
                    "JUMPAERIAL", "OTTOTTOWAIT", "TECH"];
  const SETVEL_KEYS = ["DOWNSTANDB", "DOWNSTANDF", "ESCAPEB", "ESCAPEF",
                       "TECHB", "TECHF"];

  window.__capSpecs["moves-puff"] = {
    expectWrapped: 223,

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
      // the puff module index (characters/puff/moves/index.js) — found by
      // fn IDENTITY against the live table (rule 15's measurement);
      // AERIALTURN1/NEUTRALSPECIALGROUNDTURN are unique to puff's index;
      // `!ex.default.moves` excludes the puff CHARACTER object
      // (characters/puff/index.js default = {moves, attributes, ecb}):
      const puffTbl = AS.actionStates[1];
      if (!puffTbl || !puffTbl.JAB1) {
        throw new Error("moves-puff: actionStates[1].JAB1 missing");
      }
      const PUFFMOVES = find((ex) =>
        ex.default && typeof ex.default === "object" &&
        ex.default.JAB1 && typeof ex.default.JAB1 === "object" &&
        typeof ex.default.JAB1.init === "function" &&
        ex.default.JAB1.init === puffTbl.JAB1.init &&
        ex.default.AERIALTURN1 && ex.default.NEUTRALSPECIALGROUNDTURN &&
        !ex.default.WAIT && !ex.default.moves, "puffMovesIndex").default;
      const puffKeys = Object.keys(PUFFMOVES);
      if (puffKeys.length !== 71) {
        throw new Error("moves-puff: expected 71 puff index keys, got " +
                        puffKeys.length);
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

      // --- sound attribution (incl. the furaloop ".stop" token — parity
      // with prior specs; NO Howl id is consumed by any puff move
      // (measured: no `= sounds.` assignment under characters/puff/), so
      // the task-11 sbid seam is not carried) --------------------------------
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
        throw new Error("moves-puff: expected 180 Howl sounds, wrapped " +
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

      // --- article seam (measurement instrument: puff has ZERO article
      // references — the pin freezes the count at ZERO; a record here means
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
          // M4 task 1: NSA onWallCollide's wallBounce vfx reads
          // activeStage.wallL/wallR[wallNum][1].x — the widened config
          // put the wall plane in puff's read set (puff-only widening;
          // the other move specs keep the 5-key projection).
          wallL: st.wallL,
          wallR: st.wallR,
        };
      };
      // the chd plane: the LIVE puff charHitboxes {moveKey: {idN: {dmg,
      // size}}} — the two fields with upstream write sites (measured);
      // player.charHitboxes ALIASES chars data (player.js:132), so any
      // present puff slot reads the one global plane.
      const chdCanon = () => {
        for (let k = 0; k < 4; k++) {
          if (M.playerType[k] > -1 && M.characterSelections[k] === 1) {
            const ch = M.player[k].charHitboxes;
            const out = {};
            for (const mk of Object.keys(ch)) {
              const ids = {};
              for (const ik of Object.keys(ch[mk])) {
                const v = ch[mk][ik];
                if (ik.indexOf("id") === 0 && v && typeof v === "object") {
                  ids[ik] = { dmg: v.dmg, size: v.size };
                }
              }
              out[mk] = ids;
            }
            return ctx.canon(out);
          }
        }
        return "null"; // no puff present (never over the carriers)
      };
      const preEnvelope = () =>
        '{"alias":' + aliasCanon() +
        ',"characterSelections":' + ctx.canon(M.characterSelections) +
        ',"chd":' + chdCanon() +
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

      // --- the puff-move boundary wrapper -----------------------------------
      const wrapPuffFn = (entry, k) => {
        const orig = entry[k];
        const name = entry.name;
        entry[k] = function () {
          if (inScope > 0) return orig.apply(this, arguments);
          const args = Array.prototype.slice.call(arguments);
          // puff phases are (p[, input]); onPlayerHit is (p); onWallCollide
          // is (p, input, wallFace, wallNum) — the only >2-arg site.
          let inCanon = "null";
          let extras = [];
          if (k === "onWallCollide") {
            if (args.length !== 4 || !isGodInput(args[1]) ||
                typeof args[2] !== "string" || typeof args[3] !== "number") {
              throw new Error("moves-puff: " + name +
                              ".onWallCollide unexpected signature");
            }
            inCanon = inputsCanon(args[1]);
            extras = [args[2], args[3]];
          } else {
            if (args.length > 2) {
              throw new Error("moves-puff: " + name + "." + k +
                              " called with " + args.length + " args");
            }
            if (args.length === 2) {
              if (!isGodInput(args[1])) {
                throw new Error("moves-puff: " + name + "." + k +
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

      // --- the non-puff per-char seam logger ------------------------------
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
            throw new Error("moves-puff: seam " + name + "." + k +
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
      // are silent chain-safe surface; inside a puff record they are the
      // transparent nested C tree (task 7's bodies).
      const seen = new WeakSet();
      const sharedOrigin = [{}, {}, {}, {}, {}];
      const puffOrigin = [{}, {}, {}, {}, {}];
      const nameMap = [{}, {}, {}, {}, {}];
      const perTablePuff = [0, 0, 0, 0, 0];
      for (let c = 0; c < AS.actionStates.length; c++) {
        const tbl = AS.actionStates[c];
        if (!tbl) continue;
        for (const st of Object.keys(tbl)) {
          const entry = tbl[st];
          if (!entry || typeof entry !== "object") continue;
          if (typeof entry.name !== "string") {
            throw new Error("moves-puff: move object without a name");
          }
          nameMap[c][st] = entry.name;
          const sh = SHARED[st] !== undefined &&
                     typeof SHARED[st].init === "function" &&
                     entry.init === SHARED[st].init;
          sharedOrigin[c][st] = sh;
          const po = !sh && PUFFMOVES[st] !== undefined &&
                     typeof PUFFMOVES[st].init === "function" &&
                     entry.init === PUFFMOVES[st].init;
          puffOrigin[c][st] = po;
          if (seen.has(entry)) {
            throw new Error("moves-puff: entry object shared across tables");
          }
          seen.add(entry);
          for (const k of Object.keys(entry)) {
            if (typeof entry[k] !== "function") continue;
            if (po) {
              wrapPuffFn(entry, k);
              perTablePuff[c]++;
            } else if (!sh) {
              wrapSeamFn(entry, k);
            }
          }
        }
      }
      // measured composition: puff-origin only on table 1, all 71 states,
      // 221 fns (217 phase fns + 3 onPlayerHit + 1 onWallCollide)
      for (let c = 0; c < 5; c++) {
        const want = c === 1 ? 221 : 0;
        if (perTablePuff[c] !== want) {
          throw new Error("moves-puff: table " + c + " wrapped " +
                          perTablePuff[c] + " puff fns, expected " + want);
        }
      }
      for (const st of puffKeys) {
        if (!puffOrigin[1][st]) {
          throw new Error(
            "moves-puff: puff table state not puff-origin: " + st);
        }
        if (nameMap[1][st] !== st) {
          throw new Error("moves-puff: puff state key != move name: " + st);
        }
      }
      // the OVERRIDE claim, asserted: puff replaces the shared
      // FURAFURA/JUMPAERIALB/JUMPAERIALF on ITS table only
      for (const st of ["FURAFURA", "JUMPAERIALB", "JUMPAERIALF"]) {
        if (sharedOrigin[1][st] || !puffOrigin[1][st]) {
          throw new Error("moves-puff: " + st + " not a puff override");
        }
        for (const c of [0, 2, 3, 4]) {
          if (!sharedOrigin[c][st]) {
            throw new Error("moves-puff: " + st + " not shared on table " + c);
          }
        }
      }
      ctx.declare("move");
      ctx.declare("mdispatch");

      // --- mvData: task-7 dump + the puff extension (rule 15) ---------------
      const mvDataCanon = () => {
        const dump = { chars: {},
                       puff: { data: {}, origin: puffOrigin[1] },
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
        // puff data plane: every own enumerable ARRAY-valued non-function
        // prop of every puff-origin entry (executed data, never retyped:
        // FORWARDSMASH/ATTACKDASH/THROWBACK setVelocities, SIDESPECIAL*
        // groundVelocities+airVelocities, CLIFF* offset/setVelocities,
        // THROWN*.offset, THROWNPUFFBACK.offsetVel (dead), the
        // canGrabLedge bool pairs).
        for (const st of puffKeys) {
          const entry = AS.actionStates[1][st];
          const d = {};
          let any = false;
          for (const k of Object.keys(entry)) {
            if (Array.isArray(entry[k])) { d[k] = entry[k]; any = true; }
          }
          if (any) dump.puff.data[st] = d;
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
        throw new Error("moves-puff: mvData drifted during the match");
      }
      return 1;
    },

    // Synthetic-domain sweep (rules 11/12): the task-8 scaffolding reused —
    // a REAL upstream playerObject(1, [10,20], 1) injected into inactive
    // slot 3 with characterSelections[3] set to 1 (both restored); a second
    // player in slot 2 covers the THROWN* grabbedBy<p (timer=-1) arm;
    // Math.random swapped for the sweep mulberry32 (0x0badf00d); hitQueue
    // pushes spliced back out. Puff-specific rule-12 restoration: the
    // sweep snapshots dmg+size of EVERY puff charHitboxes id object up
    // front and restores them all in the finally — the rollout dmg arms
    // (incl. the STALE-id arm writing through jab1's objects) and the
    // sing size windows mutate the GLOBAL plane. Unswept (documented):
    // CLIFF* onLedge===-1 canGrabLedge table-write arm (C traps; mvData
    // finalCheck guards), unguarded THROWN{FALCO,FALCON}* grabbedBy=-1 /
    // offset-overrun arms and guarded-THROWN overruns past the clamp
    // window (upstream throws), interrupt-tail WALK arms shadowed by
    // checkForTilts, AERIALTURN6/JUMPAERIAL6 (unreachable: every
    // multijump dispatch is jumpsUsed<5-guarded or capped by
    // JUMPAERIAL5's armless interrupt).
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
      const INbDown = mk({ 0: { b: true, lsY: -1 } });
      const INy = mk({ 0: { y: true } });
      const INyBack = mk({ 0: { y: true, lsX: -0.5 } });
      const INaUp = mk({ 0: { a: true, lsY: 0.8 }, 1: { lsY: 0.8 },
                         2: { lsY: 0.8 }, 3: { lsY: 0.8 } });
      const INlA = mk({ 0: { lA: 1 } });
      const INl = mk({ 0: { l: true } });
      const INstick = mk({ 0: { lsX: 0.5, lsY: 0.5 } });
      const INbackStick = mk({ 0: { lsX: -0.35 } });
      const INback5 = mk({ 0: { lsX: -0.5 } });
      const INup = mk({ 0: { lsY: 0.8 } });

      const savedP3 = M.player[3];
      const savedT3 = M.playerType[3];
      const savedP2 = M.player[2];
      const savedT2 = M.playerType[2];
      const savedCS3 = M.characterSelections[3];
      const savedRandom = Math.random;
      const savedArticles = ART.aArticles.length;
      const savedHq = HD.hitQueue.length;
      // the global charHitboxes dmg/size fields the rollout/sing sweep
      // arms mutate (player.js:132 aliases chars data — rule 12
      // net-restore): snapshot the WHOLE puff plane up front.
      const puffHb = [];
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
        M.characterSelections[3] = 1; // puff
        const fresh = (mut) => {
          M.player[3] = new PJ.playerObject(1, [10, 20], 1);
          if (mut) mut(M.player[3]);
        };
        const T = AS.actionStates[1]; // puff's table
        const call = (name, phase, IN) => {
          T[name][phase](3, IN);
          n++;
        };
        const grab = (p3) => {
          p3.phys.grabbing = 3;
          p3.phys.grabbedBy = 3;
        };
        let CH; // the global puff charHitboxes plane (via the fresh player)
        {
          fresh();
          CH = M.player[3].charHitboxes;
          for (const mk2 of Object.keys(CH)) {
            for (const ik of Object.keys(CH[mk2])) {
              const v = CH[mk2][ik];
              if (ik.indexOf("id") === 0 && v && typeof v === "object") {
                puffHb.push([v, v.dmg, v.size]);
              }
            }
          }
        }
        // pre-assign stale ids (a previous move's global objects) for the
        // rollout dmg arms: jab1's three objects — the cross-move
        // provenance case the chd plane exists for.
        const staleIds = (p) => {
          p.hitboxes.id[0] = CH.jab1.id0;
          p.hitboxes.id[1] = CH.jab1.id1;
          p.hitboxes.id[2] = CH.jab1.id2;
        };
        const nsgIds = (p) => {
          p.hitboxes.id[0] = CH.neutralspecialground.id0;
          p.hitboxes.id[1] = CH.neutralspecialground.id1;
          p.hitboxes.id[2] = CH.neutralspecialground.id2;
        };

        // jabs
        fresh(); call("JAB1", "init", INn);
        fresh((p) => { p.timer = 4; }); call("JAB1", "main", INn); // active 5
        fresh((p) => { p.timer = 5; }); call("JAB1", "main", INn); // frame++ 6
        fresh((p) => { p.timer = 6; }); call("JAB1", "main", INn); // off 7
        fresh((p) => { p.timer = 9; });
        call("JAB1", "main", INaPress); // jabCombo window (2<t<26)
        fresh((p) => { p.timer = 8; p.phys.jabCombo = true; });
        call("JAB1", "interrupt", INn); // -> JAB2 (>7)
        fresh((p) => { p.timer = 18; });
        call("JAB1", "interrupt", INn); // WAIT (>17)
        fresh((p) => { p.timer = 16; });
        call("JAB1", "interrupt", INy); // KNEEBEND arm (>15)
        fresh((p) => { p.timer = 16; });
        call("JAB1", "interrupt", INbDown); // puff[b] special dispatch
        fresh((p) => { p.timer = 16; });
        call("JAB1", "interrupt", INaUp); // puff[t] tilt dispatch (UPTILT)
        fresh((p) => { p.timer = 16; });
        call("JAB1", "interrupt", INbackStick); // TILTTURN tail arm
        fresh((p) => { p.timer = 16; });
        call("JAB1", "interrupt", INn); // block, all false
        fresh(); call("JAB2", "init", INn);
        fresh((p) => { p.timer = 5; }); call("JAB2", "main", INn); // active 6
        fresh((p) => { p.timer = 6; }); call("JAB2", "main", INn); // frame++ 7
        fresh((p) => { p.timer = 7; }); call("JAB2", "main", INn); // off 8
        fresh((p) => { p.timer = 8; p.phys.jabCombo = true; });
        call("JAB2", "interrupt", INn); // false (puff JAB2 has NO
        // jabCombo arm — unlike JAB1's; verbatim)
        fresh((p) => { p.timer = 21; });
        call("JAB2", "interrupt", INn); // WAIT (>20)
        fresh((p) => { p.timer = 18; });
        call("JAB2", "interrupt", INy); // KNEEBEND arm (>16)
        fresh((p) => { p.timer = 18; });
        call("JAB2", "interrupt", INn); // block, all false
        // tilts
        fresh(); call("UPTILT", "init", INn);
        fresh((p) => { p.timer = 7; }); call("UPTILT", "main", INn); // 8 active
        fresh((p) => { p.timer = 8; }); call("UPTILT", "main", INn); // 9 fr++
        fresh((p) => { p.timer = 9; });
        call("UPTILT", "main", INn); // 10 uptilt2 swap
        fresh((p) => { p.timer = 12; });
        call("UPTILT", "main", INn); // 13 frame++ (>10<15)
        fresh((p) => { p.timer = 14; }); call("UPTILT", "main", INn); // 15 off
        fresh((p) => { p.timer = 24; });
        call("UPTILT", "interrupt", INn); // WAIT (>23)
        fresh(); call("DOWNTILT", "init", INn);
        fresh((p) => { p.timer = 9; });
        call("DOWNTILT", "main", INn); // 10 active
        fresh((p) => { p.timer = 11; });
        call("DOWNTILT", "main", INn); // 12 frame++
        fresh((p) => { p.timer = 12; });
        call("DOWNTILT", "main", INn); // 13 off
        fresh((p) => { p.timer = 40; });
        call("DOWNTILT", "interrupt", INn); // SQUATWAIT (>39)
        fresh((p) => { p.timer = 30; });
        call("DOWNTILT", "interrupt", INy); // KNEEBEND arm (>29)
        fresh((p) => { p.timer = 30; });
        call("DOWNTILT", "interrupt", INbackStick); // inline TILTTURN arm
        fresh((p) => { p.timer = 30; });
        call("DOWNTILT", "interrupt", INstick); // inline WALK arm
        fresh(); call("FORWARDTILT", "init", INn);
        fresh((p) => { p.timer = 5; });
        call("FORWARDTILT", "main", INn); // 6 active
        fresh((p) => { p.timer = 8; });
        call("FORWARDTILT", "main", INn); // 9 frame++
        fresh((p) => { p.timer = 9; });
        call("FORWARDTILT", "main", INn); // 10 off
        fresh((p) => { p.timer = 28; });
        call("FORWARDTILT", "interrupt", INn); // WAIT (>27)
        // smashes (charge machine at t===4/5/5, a||z held)
        fresh(); call("FORWARDSMASH", "init", INn);
        fresh((p) => { p.timer = 4; p.phys.chargeFrames = 4; });
        call("FORWARDSMASH", "main", INa); // charging + smashcharge at 5
        fresh((p) => { p.timer = 4; p.phys.chargeFrames = 59; });
        call("FORWARDSMASH", "main", INa); // chargeFrames 60 release
        fresh((p) => { p.timer = 4; });
        call("FORWARDSMASH", "main", INn); // no-charge release
        fresh((p) => { p.timer = 5; });
        call("FORWARDSMASH", "main", INn); // 6 randomShout
        fresh((p) => { p.timer = 11; });
        call("FORWARDSMASH", "main", INn); // 12 active + normalswing1
        fresh((p) => { p.timer = 15; });
        call("FORWARDSMASH", "main", INn); // 16 fsmash2 swap
        fresh((p) => { p.timer = 20; });
        call("FORWARDSMASH", "main", INn); // 21 off
        fresh((p) => { p.timer = 45; });
        call("FORWARDSMASH", "interrupt", INn); // WAIT (>44)
        fresh(); call("UPSMASH", "init", INn); // randomShout in INIT
        fresh((p) => { p.timer = 5; p.phys.chargeFrames = 4; });
        call("UPSMASH", "main", INa); // charge arm at t===5
        fresh((p) => { p.timer = 6; });
        call("UPSMASH", "main", INn); // 7 active
        fresh((p) => { p.timer = 8; });
        call("UPSMASH", "main", INn); // 9 frame++ (>7<11)
        fresh((p) => { p.timer = 10; });
        call("UPSMASH", "main", INn); // 11 off
        fresh((p) => { p.timer = 55; });
        call("UPSMASH", "interrupt", INn); // WAIT (>54)
        fresh((p) => { p.timer = 45; });
        call("UPSMASH", "interrupt", INy); // !inCSS block (>44): KNEEBEND
        fresh((p) => { p.timer = 45; });
        call("UPSMASH", "interrupt", INn); // block, all false
        fresh(); call("DOWNSMASH", "init", INn); // randomShout in INIT
        fresh((p) => { p.timer = 5; p.phys.chargeFrames = 4; });
        call("DOWNSMASH", "main", INa); // charge arm at t===5
        fresh((p) => { p.timer = 8; });
        call("DOWNSMASH", "main", INn); // 9 active (4 hitboxes)
        fresh((p) => { p.timer = 9; });
        call("DOWNSMASH", "main", INn); // 10 frame++ (>9<11)
        fresh((p) => { p.timer = 10; });
        call("DOWNSMASH", "main", INn); // 11 off
        fresh((p) => { p.timer = 55; });
        call("DOWNSMASH", "interrupt", INn); // WAIT (>54)
        fresh((p) => { p.timer = 48; });
        call("DOWNSMASH", "interrupt", INbDown); // >47 block: special dispatch
        fresh((p) => { p.timer = 48; });
        call("DOWNSMASH", "interrupt", INn); // block, all false
        fresh(); call("ATTACKDASH", "init", INn);
        fresh((p) => { p.timer = 3; });
        call("ATTACKDASH", "main", INn); // 4 active + setVel
        fresh((p) => { p.timer = 8; });
        call("ATTACKDASH", "main", INn); // 9 dashattack2 swap
        fresh((p) => { p.timer = 14; });
        call("ATTACKDASH", "main", INn); // 15 off
        fresh((p) => { p.timer = 40; });
        call("ATTACKDASH", "interrupt", INn); // WAIT (>39)
        fresh((p) => { p.timer = 1; p.phys.cVel.x = 99; });
        call("ATTACKDASH", "interrupt", INlA); // GRAB arm + dMaxV clamp
        fresh((p) => { p.timer = 38; });
        call("ATTACKDASH", "interrupt", INy); // >38 block: KNEEBEND
        fresh((p) => { p.timer = 38; });
        call("ATTACKDASH", "interrupt", INn); // block, all false
        // aerials
        fresh(); call("ATTACKAIRN", "init", INn);
        fresh((p) => { p.timer = 4; });
        call("ATTACKAIRN", "main", INn); // 5 autoCancel off
        fresh((p) => { p.timer = 5; });
        call("ATTACKAIRN", "main", INn); // 6 active + normalswing2
        fresh((p) => { p.timer = 6; });
        call("ATTACKAIRN", "main", INn); // 7: hitboxes.FRAMES++ (typo quirk)
        fresh((p) => { p.timer = 7; });
        call("ATTACKAIRN", "main", INn); // 8 nair2 swap
        fresh((p) => { p.timer = 20; });
        call("ATTACKAIRN", "main", INn); // 21 frame++ (>8<29)
        fresh((p) => { p.timer = 28; });
        call("ATTACKAIRN", "main", INn); // 29 off
        fresh((p) => { p.timer = 29; });
        call("ATTACKAIRN", "main", INn); // 30 autoCancel on
        fresh((p) => { p.timer = 50; });
        call("ATTACKAIRN", "interrupt", INn); // FALL (>49)
        fresh((p) => { p.phys.autoCancel = true; });
        call("ATTACKAIRN", "land", INn); // LANDING
        fresh((p) => { p.phys.autoCancel = false; });
        call("ATTACKAIRN", "land", INn); // LANDINGATTACKAIRN
        fresh(); call("ATTACKAIRF", "init", INn);
        fresh((p) => { p.timer = 6; });
        call("ATTACKAIRF", "main", INn); // 7 active
        fresh((p) => { p.timer = 7; });
        call("ATTACKAIRF", "main", INn); // 8 frame++
        fresh((p) => { p.timer = 8; });
        call("ATTACKAIRF", "main", INn); // 9 fair2 swap
        fresh((p) => { p.timer = 15; });
        call("ATTACKAIRF", "main", INn); // 16 frame++ (>9<23)
        fresh((p) => { p.timer = 22; });
        call("ATTACKAIRF", "main", INn); // 23 off
        fresh((p) => { p.timer = 34; });
        call("ATTACKAIRF", "main", INn); // 35 autoCancel on
        fresh((p) => { p.timer = 40; });
        call("ATTACKAIRF", "interrupt", INn); // FALL (>39)
        fresh((p) => { p.timer = 35; });
        call("ATTACKAIRF", "interrupt", INy); // multijump -> JUMPAERIALF
        // (puffNextJump -> JUMPAERIAL1: nested puff tree)
        fresh((p) => { p.timer = 35; });
        call("ATTACKAIRF", "interrupt", INyBack); // -> AERIALTURN1 branch
        fresh((p) => { p.timer = 35; });
        call("ATTACKAIRF", "interrupt", INaUp); // aerial payload ->
        // puff[a[1]].init
        fresh((p) => { p.phys.autoCancel = true; });
        call("ATTACKAIRF", "land", INn);
        fresh((p) => { p.phys.autoCancel = false; });
        call("ATTACKAIRF", "land", INn);
        fresh(); call("ATTACKAIRB", "init", INn);
        fresh((p) => { p.timer = 7; });
        call("ATTACKAIRB", "main", INn); // 8: phys.AUTOCANCEL (typo quirk)
        fresh((p) => { p.timer = 8; });
        call("ATTACKAIRB", "main", INn); // 9 active (3 hb) + normalswing1
        fresh((p) => { p.timer = 10; });
        call("ATTACKAIRB", "main", INn); // 11 frame++ (>9<13)
        fresh((p) => { p.timer = 12; });
        call("ATTACKAIRB", "main", INn); // 13 off
        fresh((p) => { p.timer = 25; });
        call("ATTACKAIRB", "main", INn); // 26 autoCancel on
        fresh((p) => { p.timer = 40; });
        call("ATTACKAIRB", "interrupt", INn); // FALL (>39)
        fresh((p) => { p.timer = 31; });
        call("ATTACKAIRB", "interrupt", INy); // multijump -> JUMPAERIALF
        fresh((p) => { p.timer = 31; });
        call("ATTACKAIRB", "interrupt", INaUp); // aerial payload
        fresh((p) => { p.phys.autoCancel = true; });
        call("ATTACKAIRB", "land", INn);
        fresh((p) => { p.phys.autoCancel = false; });
        call("ATTACKAIRB", "land", INn);
        fresh(); call("ATTACKAIRU", "init", INn);
        fresh((p) => { p.timer = 8; });
        call("ATTACKAIRU", "main", INn); // 9 active
        fresh((p) => { p.timer = 10; });
        call("ATTACKAIRU", "main", INn); // 11 frame++ (>9<13)
        fresh((p) => { p.timer = 12; });
        call("ATTACKAIRU", "main", INn); // 13 off
        fresh((p) => { p.timer = 37; });
        call("ATTACKAIRU", "main", INn); // 38 autoCancel on
        fresh((p) => { p.timer = 40; });
        call("ATTACKAIRU", "interrupt", INn); // FALL (>39)
        fresh((p) => { p.timer = 38; });
        call("ATTACKAIRU", "interrupt", INy); // multijump arm (>37)
        fresh((p) => { p.timer = 38; });
        call("ATTACKAIRU", "interrupt", INaUp); // aerial payload
        fresh((p) => { p.phys.autoCancel = true; });
        call("ATTACKAIRU", "land", INn);
        fresh((p) => { p.phys.autoCancel = false; });
        call("ATTACKAIRU", "land", INn);
        fresh(); call("ATTACKAIRD", "init", INn);
        fresh((p) => { p.timer = 3; });
        call("ATTACKAIRD", "main", INn); // 4 autoCancel off
        fresh((p) => { p.timer = 4; });
        call("ATTACKAIRD", "main", INn); // 5 %3==2 active + normalswing2
        fresh((p) => { p.timer = 5; });
        call("ATTACKAIRD", "main", INn); // 6 %3==0 frame++
        fresh((p) => { p.timer = 6; });
        call("ATTACKAIRD", "main", INn); // 7 %3==1 turnOff
        fresh((p) => { p.timer = 39; });
        call("ATTACKAIRD", "main", INn); // 40 autoCancel on
        fresh((p) => { p.timer = 50; });
        call("ATTACKAIRD", "interrupt", INn); // FALL (>49)
        fresh((p) => { p.phys.autoCancel = true; });
        call("ATTACKAIRD", "land", INn);
        fresh((p) => { p.phys.autoCancel = false; });
        call("ATTACKAIRD", "land", INn);
        // multijump family
        fresh(); call("AERIALTURN1", "init", INback5); // cVel.x = lsX*0.5
        fresh(); call("AERIALTURN2", "init", INn); // doubleJumped, 1.59
        fresh(); call("AERIALTURN3", "init", INn); // 1.47
        fresh(); call("AERIALTURN4", "init", INn); // 1.36
        fresh(); call("AERIALTURN5", "init", INn); // 1.25
        fresh((p) => { p.timer = 12; });
        call("AERIALTURN1", "main", INn); // t===13: -> JUMPAERIAL1.main
        fresh((p) => { p.timer = 5; });
        call("AERIALTURN1", "main", INn); // t===6 face flip
        fresh((p) => { p.timer = 8; });
        call("AERIALTURN1", "main", INn); // fastfall + drift arm
        fresh((p) => { p.timer = 8; p.phys.cVel.x = 2; });
        call("AERIALTURN2", "main", INn); // drift friction arm (tempMax 0)
        fresh((p) => { p.timer = 8; p.phys.cVel.x = -2; });
        call("AERIALTURN3", "main", INstick); // drift accel arm (lsX 0.5)
        fresh((p) => { p.timer = 8; });
        call("AERIALTURN1", "interrupt", INaUp); // aerial payload
        fresh((p) => { p.timer = 8; });
        call("AERIALTURN1", "interrupt", INl); // ESCAPEAIR arm
        fresh((p) => { p.timer = 8; });
        call("AERIALTURN1", "interrupt", INbDown); // special payload
        fresh(); call("JUMPAERIAL1", "init", INback5);
        fresh(); call("JUMPAERIAL2", "init", INn);
        fresh(); call("JUMPAERIAL3", "init", INn);
        fresh(); call("JUMPAERIAL4", "init", INn);
        fresh(); call("JUMPAERIAL5", "init", INn);
        fresh((p) => { p.timer = 10; });
        call("JUMPAERIAL1", "main", INn); // fastfall + drift
        fresh((p) => { p.timer = 8; });
        call("JUMPAERIAL1", "interrupt", INaUp); // aerial payload
        fresh((p) => { p.timer = 8; });
        call("JUMPAERIAL1", "interrupt", INl); // ESCAPEAIR arm
        fresh((p) => { p.timer = 8; });
        call("JUMPAERIAL1", "interrupt", INbDown); // special payload
        fresh((p) => { p.timer = 29; });
        call("JUMPAERIAL1", "interrupt", INy); // multijump -> puffNextJump
        // (jumpsUsed 0 -> JUMPAERIAL1 nested)
        fresh((p) => { p.timer = 29; p.phys.jumpsUsed = 3; });
        call("JUMPAERIAL4", "interrupt", INyBack); // -> AERIALTURN4 branch
        fresh((p) => { p.timer = 51; });
        call("JUMPAERIAL1", "interrupt", INn); // FALL (>50)
        fresh((p) => { p.timer = 51; });
        call("JUMPAERIAL5", "interrupt", INn); // FALL (no multijump arm)
        fresh(); call("JUMPAERIALB", "init", INn); // puffNextJump -> JA1
        fresh((p) => { p.phys.jumpsUsed = 4; });
        call("JUMPAERIALB", "init", INback5); // -> AERIALTURN5 branch
        fresh(); call("JUMPAERIALF", "init", INn);
        fresh((p) => { p.phys.jumpsUsed = 4; });
        call("JUMPAERIALF", "init", INn); // -> JUMPAERIAL5
        fresh(); call("FURAFURA", "init", INn); // the trivial override
        // ROLLOUT (NEUTRALSPECIALGROUND) — NOTE: NSG/NSA mains do NOT
        // advance timer at the top (the advance sits mid-body), so presets
        // are the EXACT arm timers; every arm sets the rollOut fields the
        // body reads (a fresh player has none — undefined arithmetic).
        fresh(); call("NEUTRALSPECIALGROUND", "init", INn);
        fresh((p) => { p.timer = 15; p.phys.rollOutChargeAttempt = false;
                       p.phys.rollOutCharging = false;
                       p.phys.rollOutCharge = 0; p.phys.rollOutVel = 0.3;
                       p.phys.rollOutPlayerHit = false; });
        call("NEUTRALSPECIALGROUND", "main", INn); // t===15 dashDust
        fresh((p) => { p.timer = 16; p.phys.rollOutChargeAttempt = true;
                       p.phys.rollOutCharge = 0; });
        call("NEUTRALSPECIALGROUND", "main", INb); // charging arm, charge 1
        fresh((p) => { p.timer = 16; p.phys.rollOutChargeAttempt = true;
                       p.phys.rollOutCharging = true;
                       p.phys.rollOutCharge = 18; });
        call("NEUTRALSPECIALGROUND", "main", INb); // charge 19 + t16 dashDust
        fresh((p) => { p.timer = 20; p.phys.rollOutChargeAttempt = true;
                       p.phys.rollOutCharging = true;
                       p.phys.rollOutCharge = 44; });
        call("NEUTRALSPECIALGROUND", "main", INb); // charge clamp 44
        fresh((p) => { p.timer = 20; p.phys.rollOutChargeAttempt = true;
                       p.phys.rollOutCharging = true;
                       p.phys.rollOutCharge = 25; });
        call("NEUTRALSPECIALGROUND", "main", INn); // CHARGED release: nsg id
        // assigns + 3 sounds + the post-block dmg write (newDmg 14 —
        // GLOBAL mutation, restored)
        fresh((p) => { p.timer = 20; p.phys.rollOutChargeAttempt = true;
                       p.phys.rollOutCharging = true;
                       p.phys.rollOutCharge = 5; staleIds(p); });
        call("NEUTRALSPECIALGROUND", "main", INn); // UNCHARGED release: the
        // STALE-id dmg write (jab1's globals get newDmg 9 — restored)
        fresh((p) => { p.timer = 20; p.phys.rollOutChargeAttempt = false;
                       p.phys.rollOutCharging = false;
                       p.phys.rollOutCharge = 0;
                       p.phys.rollOutDistance = 5;
                       p.phys.rollOutVel = 0.3;
                       p.phys.rollOutPlayerHit = false; staleIds(p); });
        call("NEUTRALSPECIALGROUND", "main", INback5); // rolling: dmg write
        // + cVel + lsX*face < -0.49 -> NEUTRALSPECIALGROUNDTURN chain
        fresh((p) => { p.timer = 20; p.phys.rollOutChargeAttempt = false;
                       p.phys.rollOutCharging = false;
                       p.phys.rollOutCharge = 19;
                       p.phys.rollOutDistance = 9;
                       p.phys.rollOutVel = 2;
                       p.phys.rollOutPlayerHit = false; nsgIds(p); });
        call("NEUTRALSPECIALGROUND", "main", INn); // distance 10: %10
        // dashDust arm + charged-domain dmg write (restored)
        fresh((p) => { p.timer = 28; p.phys.rollOutChargeAttempt = false;
                       p.phys.rollOutCharging = false;
                       p.phys.rollOutCharge = 19;
                       p.phys.rollOutDistance = 5;
                       p.phys.rollOutVel = 2;
                       p.phys.rollOutPlayerHit = false; nsgIds(p); });
        call("NEUTRALSPECIALGROUND", "main", INn); // colourOverlayBool
        // window (28<=t<=34)
        fresh((p) => { p.timer = 20; p.phys.rollOutChargeAttempt = false;
                       p.phys.rollOutCharging = false;
                       p.phys.rollOutCharge = 0;
                       p.phys.rollOutDistance = 100;
                       p.phys.rollOutVel = 0.3;
                       p.phys.rollOutPlayerHit = false; staleIds(p); });
        call("NEUTRALSPECIALGROUND", "main", INn); // distance 101 arm:
        // turnOff + t=46 + cVel*0.6
        fresh((p) => { p.timer = 45; p.phys.rollOutChargeAttempt = false;
                       p.phys.rollOutCharging = false;
                       p.phys.rollOutCharge = 44;
                       p.phys.rollOutDistance = 5;
                       p.phys.rollOutVel = 4.2;
                       p.phys.rollOutPlayerHit = false; nsgIds(p); });
        call("NEUTRALSPECIALGROUND", "main", INn); // t>45 wrap: t=16 + tick
        fresh((p) => { p.timer = 46; p.phys.rollOutChargeAttempt = false;
                       p.phys.rollOutCharging = false;
                       p.phys.rollOutCharge = 0;
                       p.phys.rollOutDistance = 101;
                       p.phys.rollOutPlayerHit = false;
                       p.phys.cVel.x = 1; });
        call("NEUTRALSPECIALGROUND", "main", INn); // t>=46 decel (+ sign)
        fresh((p) => { p.timer = 46; p.phys.rollOutChargeAttempt = false;
                       p.phys.rollOutCharging = false;
                       p.phys.rollOutCharge = 0;
                       p.phys.rollOutDistance = 101;
                       p.phys.rollOutPlayerHit = false;
                       p.phys.cVel.x = -0.05; });
        call("NEUTRALSPECIALGROUND", "main", INn); // decel sign-cross clamp
        fresh((p) => { p.timer = 78; });
        call("NEUTRALSPECIALGROUND", "interrupt", INn); // WAIT arm — and
        // returns FALSE (verbatim quirk)
        fresh((p) => { p.timer = 20; });
        call("NEUTRALSPECIALGROUND", "interrupt", INn); // plain false
        fresh(); T.NEUTRALSPECIALGROUND.onPlayerHit(3); n++; // -> NSA chain
        // ROLLOUT (NEUTRALSPECIALAIR)
        fresh((p) => { p.phys.cVel.y = -3; });
        call("NEUTRALSPECIALAIR", "init", INn); // cVel.y = max(-1.3, ·)
        fresh((p) => { p.timer = 15; p.phys.rollOutChargeAttempt = false;
                       p.phys.rollOutCharging = false;
                       p.phys.rollOutCharge = 0; p.phys.rollOutVel = 0.5;
                       p.phys.rollOutPlayerHit = false; });
        call("NEUTRALSPECIALAIR", "main", INn); // t===15 dashDust
        fresh((p) => { p.timer = 16; p.phys.rollOutChargeAttempt = true;
                       p.phys.rollOutCharge = 0; });
        call("NEUTRALSPECIALAIR", "main", INb); // charging arm
        fresh((p) => { p.timer = 16; p.phys.rollOutChargeAttempt = true;
                       p.phys.rollOutCharging = true;
                       p.phys.rollOutCharge = 20; });
        call("NEUTRALSPECIALAIR", "main", INb); // charge 21 + t16 dashDust
        fresh((p) => { p.timer = 20; p.phys.rollOutChargeAttempt = true;
                       p.phys.rollOutCharging = true;
                       p.phys.rollOutCharge = 44; });
        call("NEUTRALSPECIALAIR", "main", INb); // clamp 44
        fresh((p) => { p.timer = 20; p.phys.rollOutChargeAttempt = true;
                       p.phys.rollOutCharging = true;
                       p.phys.rollOutCharge = 30; });
        call("NEUTRALSPECIALAIR", "main", INn); // CHARGED release (>=21):
        // nsa id0 assign + launch/tickair + dmg write via id[0..2] —
        // id[1]/id[2] stay the fresh player's CONSTRUCTOR objects
        // (per-player, not chars data: nothing global to restore)
        fresh((p) => { p.timer = 20; p.phys.rollOutChargeAttempt = true;
                       p.phys.rollOutCharging = true;
                       p.phys.rollOutCharge = 5; staleIds(p); });
        call("NEUTRALSPECIALAIR", "main", INn); // UNCHARGED release: vel
        // max(0.5, ·) floor + stale dmg write
        fresh((p) => { p.timer = 24; p.phys.rollOutChargeAttempt = false;
                       p.phys.rollOutCharging = false;
                       p.phys.rollOutCharge = 21;
                       p.phys.rollOutDistance = 9;
                       p.phys.rollOutVel = 2;
                       p.phys.rollOutPlayerHit = false; staleIds(p); });
        call("NEUTRALSPECIALAIR", "main", INn); // overlay window (24..28)
        // + %10 dashDust + charged dmg write
        fresh((p) => { p.timer = 39; p.phys.rollOutChargeAttempt = false;
                       p.phys.rollOutCharging = false;
                       p.phys.rollOutCharge = 44;
                       p.phys.rollOutDistance = 5;
                       p.phys.rollOutVel = 4.1;
                       p.phys.rollOutPlayerHit = false; staleIds(p); });
        call("NEUTRALSPECIALAIR", "main", INn); // t>39 wrap: t=16 + tickair
        fresh((p) => { p.timer = 20; p.phys.rollOutChargeAttempt = false;
                       p.phys.rollOutCharging = false;
                       p.phys.rollOutCharge = 0;
                       p.phys.rollOutDistance = 100;
                       p.phys.rollOutVel = 0.5;
                       p.phys.rollOutPlayerHit = false; staleIds(p); });
        call("NEUTRALSPECIALAIR", "main", INn); // distance 101: t=39 +
        // cVel*0.6 + turnOff
        fresh((p) => { p.phys.rollOutPlayerHit = true;
                       p.phys.rollOutPlayerHitTimer = 10;
                       p.phys.rollOutChargeAttempt = false;
                       p.phys.rollOutCharging = false;
                       p.phys.rollOutCharge = 0; p.phys.rollOutVel = 0.5;
                       p.phys.rollOutDistance = 101;
                       p.timer = 50; });
        call("NEUTRALSPECIALAIR", "main", INn); // playerHit advance branch
        fresh((p) => { p.phys.rollOutPlayerHit = true;
                       p.phys.rollOutPlayerHitTimer = 43;
                       p.phys.rollOutChargeAttempt = false;
                       p.phys.rollOutCharging = false;
                       p.phys.rollOutCharge = 0; p.phys.rollOutVel = 0.5;
                       p.phys.rollOutDistance = 101;
                       p.timer = 50; });
        call("NEUTRALSPECIALAIR", "main", INn); // playerHitTimer>42 airDrift
        fresh((p) => { p.timer = 71; });
        call("NEUTRALSPECIALAIR", "interrupt", INn); // FALLSPECIAL arm —
        // returns FALSE (verbatim quirk)
        fresh((p) => { p.phys.rollOutPlayerHit = true; });
        call("NEUTRALSPECIALAIR", "land", INn); // LANDINGFALLSPECIAL
        fresh((p) => { p.phys.rollOutPlayerHit = false;
                       p.phys.rollOutCharge = 25; });
        call("NEUTRALSPECIALAIR", "land", INn); // NSG write + >=21 reassign
        fresh((p) => { p.phys.rollOutPlayerHit = false;
                       p.phys.rollOutCharge = 0; });
        call("NEUTRALSPECIALAIR", "land", INn); // NSG write, no reassign
        fresh((p) => { p.phys.rollOutCharging = false;
                       p.phys.rollOutChargeAttempt = false;
                       p.phys.rollOutPlayerHit = false;
                       p.phys.rollOutVel = 1; p.phys.cVel.x = 1; });
        T.NEUTRALSPECIALAIR.onWallCollide(3, INn, "R", 0); n++; // bounce R
        fresh((p) => { p.phys.rollOutCharging = false;
                       p.phys.rollOutChargeAttempt = false;
                       p.phys.rollOutPlayerHit = false;
                       p.phys.rollOutVel = 1; p.phys.cVel.x = -1; });
        T.NEUTRALSPECIALAIR.onWallCollide(3, INn, "L", 0); n++; // bounce L
        fresh((p) => { p.phys.rollOutCharging = true; });
        T.NEUTRALSPECIALAIR.onWallCollide(3, INn, "R", 0); n++; // no-op arm
        fresh(); T.NEUTRALSPECIALAIR.onPlayerHit(3); n++;
        // ROLLOUT (NEUTRALSPECIALGROUNDTURN)
        fresh((p) => { p.phys.rollOutVel = 2; p.phys.rollOutDistance = 5; });
        call("NEUTRALSPECIALGROUNDTURN", "init", INn);
        fresh((p) => { p.timer = 29; p.phys.rollOutTurnTimer = 5;
                       p.phys.rollOutVel = 2; p.phys.rollOutDistance = 4; });
        call("NEUTRALSPECIALGROUNDTURN", "main", INn); // t wrap (>30 -> 3)
        // + distance%5 dashDust
        fresh((p) => { p.timer = 10; p.phys.rollOutTurnTimer = 5;
                       p.phys.rollOutVel = 2; p.phys.rollOutDistance = 6; });
        call("NEUTRALSPECIALGROUNDTURN", "main", INn); // cVel formula arm
        fresh((p) => { p.phys.rollOutDistance = 101; });
        call("NEUTRALSPECIALGROUNDTURN", "interrupt", INn); // distance>100:
        // -> NSG t=46, returns true
        fresh((p) => { p.phys.rollOutTurnTimer = 29;
                       p.phys.rollOutCharge = 25; p.phys.rollOutVel = 2;
                       nsgIds(p); });
        call("NEUTRALSPECIALGROUNDTURN", "interrupt", INn); // turnTimer>28:
        // relaunch + >=19 reassign + stronghit, returns true
        fresh((p) => { p.phys.rollOutTurnTimer = 29;
                       p.phys.rollOutCharge = 5; p.phys.rollOutVel = 2; });
        call("NEUTRALSPECIALGROUNDTURN", "interrupt", INn); // relaunch,
        // charge<19: no reassign
        fresh((p) => { p.phys.rollOutTurnTimer = 5;
                       p.phys.rollOutDistance = 5; });
        call("NEUTRALSPECIALGROUNDTURN", "interrupt", INn); // false
        fresh(); T.NEUTRALSPECIALGROUNDTURN.onPlayerHit(3); n++;
        // SING (UPSPECIAL) — the id[0].size writes hit the GLOBAL upb.id0
        // (restored)
        fresh((p) => { p.phys.grounded = true; p.phys.cVel.x = 0.5; });
        call("UPSPECIAL", "init", INn); // grounded decel (+)
        fresh((p) => { p.phys.grounded = true; p.phys.cVel.x = -0.5; });
        call("UPSPECIAL", "init", INn); // grounded decel (-)
        fresh((p) => { p.phys.grounded = true; p.phys.cVel.x = 0.05; });
        call("UPSPECIAL", "init", INn); // both-ifs sign-flip quirk
        fresh((p) => { p.phys.grounded = false; p.phys.cVel.y = -99; });
        call("UPSPECIAL", "init", INn); // air: terminal clamp
        fresh((p) => { p.timer = 17; });
        call("UPSPECIAL", "main", INn); // 18 sing1
        fresh((p) => { p.timer = 22; });
        call("UPSPECIAL", "main", INn); // 23 sing vfx
        fresh((p) => { p.timer = 27; });
        call("UPSPECIAL", "main", INn); // 28 active + size 10.937 (global)
        fresh((p) => { p.timer = 35; });
        call("UPSPECIAL", "main", INn); // 36 size 1
        fresh((p) => { p.timer = 68; });
        call("UPSPECIAL", "main", INn); // 69 sing2 + size 10.937
        fresh((p) => { p.timer = 70; });
        call("UPSPECIAL", "main", INn); // 71 sing2 vfx
        fresh((p) => { p.timer = 76; });
        call("UPSPECIAL", "main", INn); // 77 size 1
        fresh((p) => { p.timer = 112; });
        call("UPSPECIAL", "main", INn); // 113 size 12.890
        fresh((p) => { p.timer = 121; });
        call("UPSPECIAL", "main", INn); // 122 sing3 vfx
        fresh((p) => { p.timer = 125; });
        call("UPSPECIAL", "main", INn); // 126 turnOff
        fresh((p) => { p.timer = 30; p.phys.grounded = true;
                       p.phys.cVel.x = 1; });
        call("UPSPECIAL", "main", INn); // grounded traction arm
        fresh((p) => { p.timer = 30; p.phys.grounded = false;
                       p.phys.cVel.x = 1; p.phys.cVel.y = -99; });
        call("UPSPECIAL", "main", INn); // air friction + gravity + clamp
        fresh((p) => { p.timer = 30; p.phys.grounded = false;
                       p.phys.cVel.x = -1; });
        call("UPSPECIAL", "main", INn); // air friction (-) arm
        fresh((p) => { p.timer = 180; p.phys.grounded = true; });
        call("UPSPECIAL", "interrupt", INn); // WAIT (>179)
        fresh((p) => { p.timer = 180; p.phys.grounded = false; });
        call("UPSPECIAL", "interrupt", INn); // FALLSPECIAL
        fresh(); call("UPSPECIAL", "land", INn); // empty body
        // REST (DOWNSPECIAL{GROUND,AIR})
        fresh((p) => { p.phys.grounded = true; p.phys.cVel.x = 0.5; });
        call("DOWNSPECIALGROUND", "init", INn);
        fresh((p) => { p.phys.grounded = false; p.phys.cVel.y = -99; });
        call("DOWNSPECIALGROUND", "init", INn); // air arm
        // (init's nested main covers the t===1 active+intangible arm)
        fresh((p) => { p.timer = 1; });
        call("DOWNSPECIALGROUND", "main", INn); // 2 turnOff
        fresh((p) => { p.timer = 9; });
        call("DOWNSPECIALGROUND", "main", INn); // 10 rest1 + restbubbles
        fresh((p) => { p.timer = 209; });
        call("DOWNSPECIALGROUND", "main", INn); // 210 rest2
        fresh((p) => { p.timer = 30; p.phys.grounded = false;
                       p.phys.cVel.x = 1; p.phys.cVel.y = -99; });
        call("DOWNSPECIALGROUND", "main", INn); // air movement arm
        fresh((p) => { p.timer = 250; p.phys.grounded = true; });
        call("DOWNSPECIALGROUND", "interrupt", INn); // WAIT (>249)
        fresh((p) => { p.timer = 250; p.phys.grounded = false; });
        call("DOWNSPECIALGROUND", "interrupt", INn); // FALL
        fresh(); call("DOWNSPECIALGROUND", "land", INn); // comment-only body
        fresh((p) => { p.phys.grounded = true; p.phys.cVel.x = -0.5; });
        call("DOWNSPECIALAIR", "init", INn);
        fresh((p) => { p.phys.grounded = false; });
        call("DOWNSPECIALAIR", "init", INn);
        fresh((p) => { p.timer = 1; });
        call("DOWNSPECIALAIR", "main", INn);
        fresh((p) => { p.timer = 9; });
        call("DOWNSPECIALAIR", "main", INn);
        fresh((p) => { p.timer = 209; });
        call("DOWNSPECIALAIR", "main", INn);
        fresh((p) => { p.timer = 250; p.phys.grounded = true; });
        call("DOWNSPECIALAIR", "interrupt", INn);
        fresh((p) => { p.timer = 250; p.phys.grounded = false; });
        call("DOWNSPECIALAIR", "interrupt", INn);
        fresh(); call("DOWNSPECIALAIR", "land", INn);
        // POUND (SIDESPECIAL{GROUND,AIR})
        fresh((p) => { p.phys.grounded = true; p.phys.cVel.x = 2; });
        call("SIDESPECIALGROUND", "init", INn); // grounded: cVel.x = 0
        fresh((p) => { p.phys.grounded = false; p.phys.cVel.y = -99; });
        call("SIDESPECIALGROUND", "init", INn); // air: terminal clamp
        fresh((p) => { p.timer = 15; p.phys.grounded = true; });
        call("SIDESPECIALGROUND", "main", INn); // groundVelocities arm
        fresh((p) => { p.timer = 11; p.phys.grounded = false; });
        call("SIDESPECIALGROUND", "main", INup); // t===12: angle decide
        // (upbAngleMultiplier = lsY*PI/9) + active + puffshout1
        fresh((p) => { p.timer = 5; p.phys.grounded = false;
                       p.phys.cVel.x = 1; p.phys.cVel.y = -99; });
        call("SIDESPECIALGROUND", "main", INn); // t<12 friction + gravity
        fresh((p) => { p.timer = 20; p.phys.grounded = false;
                       p.phys.upbAngleMultiplier = 0.19634954084936207; });
        call("SIDESPECIALGROUND", "main", INn); // rotated airVelocities arm
        fresh((p) => { p.timer = 41; p.phys.grounded = false; });
        call("SIDESPECIALGROUND", "main", INn); // airDrift + fastfall arm
        fresh((p) => { p.timer = 20; p.phys.grounded = true; });
        call("SIDESPECIALGROUND", "main", INn); // frame++ window (12<t<28)
        fresh((p) => { p.timer = 27; p.phys.grounded = true; });
        call("SIDESPECIALGROUND", "main", INn); // t===28 turnOff
        fresh((p) => { p.timer = 46; p.phys.grounded = true; });
        call("SIDESPECIALGROUND", "interrupt", INn); // WAIT (>45)
        fresh((p) => { p.timer = 46; p.phys.grounded = false; });
        call("SIDESPECIALGROUND", "interrupt", INn); // FALL
        fresh((p) => { p.phys.grounded = true; p.phys.cVel.x = 2; });
        call("SIDESPECIALAIR", "init", INn);
        fresh((p) => { p.phys.grounded = false; p.phys.cVel.y = -99; });
        call("SIDESPECIALAIR", "init", INn);
        fresh((p) => { p.timer = 15; p.phys.grounded = true; });
        call("SIDESPECIALAIR", "main", INn);
        fresh((p) => { p.timer = 11; p.phys.grounded = false; });
        call("SIDESPECIALAIR", "main", INup);
        fresh((p) => { p.timer = 20; p.phys.grounded = false;
                       p.phys.upbAngleMultiplier = 0.19634954084936207; });
        call("SIDESPECIALAIR", "main", INn);
        fresh((p) => { p.timer = 41; p.phys.grounded = false; });
        call("SIDESPECIALAIR", "main", INn);
        fresh((p) => { p.timer = 46; p.phys.grounded = true; });
        call("SIDESPECIALAIR", "interrupt", INn);
        fresh((p) => { p.timer = 46; p.phys.grounded = false; });
        call("SIDESPECIALAIR", "interrupt", INn);
        fresh(); call("SIDESPECIALAIR", "land", INn); // bare state write
        // throws (puff: 2-arg TABLE victim dispatch, self-grab)
        fresh(); call("THROWUP", "init", INn); // -1 guard arm
        fresh(grab); call("THROWUP", "init", INn); // THROWNPUFFUP chain
        // (nested puff victim: transparent tree)
        fresh((p) => { grab(p); p.phys.releaseFrame = 8; p.timer = 6.5; });
        call("THROWUP", "main", INn); // floor crossing 7: hq push + turnOff
        fresh((p) => { p.phys.grabbing = 3; p.phys.grabbedBy = -1;
                       p.phys.releaseFrame = 12; p.timer = 1;
                       M.player[3].phys.grabbedBy = 1; });
        call("THROWUP", "interrupt", INn); // CATCHCUT (victim's grabbedBy
        // mismatch, t < releaseFrame)
        fresh((p) => { p.phys.grabbing = 3; p.timer = 42; });
        call("THROWUP", "interrupt", INn); // >41: grabbing=-1 + WAIT
        fresh((p) => { p.phys.grabbing = -1; p.timer = 1; });
        call("THROWUP", "interrupt", INn); // -1 bare-return arm (undef)
        fresh(); call("THROWFORWARD", "init", INn); // -1 guard arm
        fresh(grab); call("THROWFORWARD", "init", INn); // + randomShout
        fresh((p) => { grab(p); p.phys.releaseFrame = 12; p.timer = 10; });
        call("THROWFORWARD", "main", INn); // t===11 throwforwardextra swap
        fresh((p) => { grab(p); p.phys.releaseFrame = 12; p.timer = 11; });
        call("THROWFORWARD", "main", INn); // crossing 12 (hq push) + t===12
        // turnOff
        fresh((p) => { p.phys.grabbing = -1; p.phys.releaseFrame = 13;
                       p.timer = 11.5; });
        call("THROWFORWARD", "main", INn); // crossing with grabbing===-1:
        // the bare-return arm inside the window
        fresh((p) => { p.phys.grabbing = 3; p.timer = 36; });
        call("THROWFORWARD", "interrupt", INn); // >35 WAIT arm
        fresh((p) => { p.phys.grabbing = -1; p.timer = 1; });
        call("THROWFORWARD", "interrupt", INn); // -1 bare-return arm
        fresh(); call("THROWBACK", "init", INn); // -1 guard arm
        fresh(grab); call("THROWBACK", "init", INn); // + randomShout
        fresh((p) => { grab(p); p.phys.releaseFrame = 23; p.timer = 14; });
        call("THROWBACK", "main", INn); // setVelocities window (the
        // floor-over-comparison shape)
        fresh((p) => { grab(p); p.phys.releaseFrame = 23; p.timer = 21.5; });
        call("THROWBACK", "main", INn); // crossing 22: hq push + turnOff
        fresh((p) => { p.phys.grabbing = 3; p.timer = 44; });
        call("THROWBACK", "interrupt", INn); // >43 WAIT arm
        fresh((p) => { p.phys.grabbing = -1; p.timer = 1; });
        call("THROWBACK", "interrupt", INn); // -1 bare-return arm
        fresh(); call("THROWDOWN", "init", INn); // -1 guard arm
        fresh(grab); call("THROWDOWN", "init", INn); // + randomShout
        fresh((p) => { grab(p); p.phys.releaseFrame = 61; p.timer = 9; });
        call("THROWDOWN", "main", INn); // t%13===10 active arm
        fresh((p) => { grab(p); p.phys.releaseFrame = 61; p.timer = 10; });
        call("THROWDOWN", "main", INn); // t%13===11 turnOff arm
        fresh((p) => { grab(p); p.phys.releaseFrame = 61; p.timer = 60.5; });
        call("THROWDOWN", "main", INn); // crossing 61: throwdown swap + push
        fresh((p) => { p.phys.grabbing = 3; p.timer = 85; });
        call("THROWDOWN", "interrupt", INn); // >84 WAIT arm
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
        // grabbedBy === -1 init guard — the GUARDED family only
        // (THROWN{FALCO,FALCON}* throw upstream: player[-1])
        fresh(); call("THROWNPUFFUP", "init", INn);
        fresh(); call("THROWNMARTHUP", "init", INn);
        fresh(); call("THROWNFOXFORWARD", "init", INn);
        // main guard + clamp arms
        fresh((p) => { p.phys.grabbedBy = -1; p.timer = 1; });
        call("THROWNPUFFUP", "main", INn); // vacuous-phys nested guard (no
        // early return)
        fresh((p) => { p.phys.grabbedBy = -1; p.timer = 1; });
        call("THROWNPUFFDOWN", "main", INn); // guard-then-clamp order
        fresh((p) => { p.phys.grabbedBy = -1; p.timer = 12; });
        call("THROWNMARTHFORWARD", "main", INn); // CLAMP-then-guard order
        fresh((p) => { p.phys.grabbedBy = 3; p.timer = 2; });
        call("THROWNPUFFUP", "main", INn); // in-range pos arm
        fresh((p) => { p.phys.grabbedBy = 3; p.timer = 12; });
        call("THROWNPUFFUP", "main", INn); // clamp arm (offset len 7)
        fresh((p) => { p.phys.grabbedBy = 3; p.timer = 70; });
        call("THROWNPUFFDOWN", "main", INn); // clamp arm (len 63)
        fresh((p) => { p.phys.grabbedBy = 3; p.timer = 2; });
        call("THROWNPUFFBACK", "main", INn); // face*-1 x arm
        fresh((p) => { p.phys.grabbedBy = 3; p.timer = 30; });
        call("THROWNPUFFBACK", "main", INn); // clamp arm (len 22)
        fresh((p) => { p.phys.grabbedBy = 3; p.timer = 2; });
        call("THROWNPUFFFORWARD", "main", INn);
        fresh((p) => { p.phys.grabbedBy = 3; p.timer = 2; });
        call("THROWNMARTHUP", "main", INn);
        fresh((p) => { p.phys.grabbedBy = 3; p.timer = 10; });
        call("THROWNMARTHUP", "main", INn); // clamp arm (len 8)
        fresh((p) => { p.phys.grabbedBy = 3; p.timer = 2; });
        call("THROWNMARTHDOWN", "main", INn); // *-1 arm
        fresh((p) => { p.phys.grabbedBy = 3; p.timer = 2; });
        call("THROWNMARTHBACK", "main", INn); // plain-face x (no *-1)
        fresh((p) => { p.phys.grabbedBy = 3; p.timer = 2; });
        call("THROWNMARTHFORWARD", "main", INn);
        fresh((p) => { p.phys.grabbedBy = 3; p.timer = 2; });
        call("THROWNFOXUP", "main", INn);
        fresh((p) => { p.phys.grabbedBy = 3; p.timer = 8; });
        call("THROWNFOXUP", "main", INn); // clamp arm (len 5)
        fresh((p) => { p.phys.grabbedBy = 3; p.timer = 2; });
        call("THROWNFOXDOWN", "main", INn);
        fresh((p) => { p.phys.grabbedBy = 3; p.timer = 2; });
        call("THROWNFOXBACK", "main", INn); // face*-1 x arm
        fresh((p) => { p.phys.grabbedBy = 3; p.timer = 2; });
        call("THROWNFOXFORWARD", "main", INn);
        fresh((p) => { p.phys.grabbedBy = -1; p.timer = 1; });
        call("THROWNFOXUP", "main", INn); // -1 main guard
        // unguarded family mains (valid grabbedBy only)
        fresh((p) => { p.phys.grabbedBy = 3; p.timer = 2; });
        call("THROWNFALCOUP", "main", INn);
        fresh((p) => { p.phys.grabbedBy = 3; p.timer = 2; });
        call("THROWNFALCODOWN", "main", INn); // *-1 arm
        fresh((p) => { p.phys.grabbedBy = 3; p.timer = 2; });
        call("THROWNFALCOBACK", "main", INn); // *-1 arm
        fresh((p) => { p.phys.grabbedBy = 3; p.timer = 2; });
        call("THROWNFALCOFORWARD", "main", INn); // (authored-expression
        // offsets — rule 15)
        fresh((p) => { p.phys.grabbedBy = 3; p.timer = 2; });
        call("THROWNFALCONUP", "main", INn);
        fresh((p) => { p.phys.grabbedBy = 3; p.timer = 2; });
        call("THROWNFALCONDOWN", "main", INn);
        fresh((p) => { p.phys.grabbedBy = 3; p.timer = 2; });
        call("THROWNFALCONBACK", "main", INn); // *-1 arm
        fresh((p) => { p.phys.grabbedBy = 3; p.timer = 2; });
        call("THROWNFALCONFORWARD", "main", INn);
        // grabbedBy < p (timer = -1) arm: inject slot 2
        M.playerType[2] = 0;
        M.player[2] = new PJ.playerObject(1, [30, 20], 1);
        fresh((p) => { p.phys.grabbedBy = 2; });
        call("THROWNPUFFUP", "init", INn);
        fresh((p) => { p.phys.grabbedBy = 2; });
        call("THROWNFALCOUP", "init", INn);
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
        fresh(cliff(10)); call("CLIFFGETUPSLOW", "main", INn); // pos arm
        fresh(cliff(33)); call("CLIFFGETUPSLOW", "main", INn); // grounding 34
        fresh(cliff(40)); call("CLIFFGETUPSLOW", "main", INn); // setVel arm
        fresh(cliff(60)); call("CLIFFGETUPSLOW", "interrupt", INn); // >59
        fresh(cliff(5)); call("CLIFFESCAPEQUICK", "main", INn); // pos arm
        fresh(cliff(14)); call("CLIFFESCAPEQUICK", "main", INn); // grounding
        fresh(cliff(20)); call("CLIFFESCAPEQUICK", "main", INn); // setVel
        fresh(cliff(50)); call("CLIFFESCAPEQUICK", "interrupt", INn); // >49
        fresh(cliff(5)); call("CLIFFESCAPESLOW", "main", INn); // pos arm
        fresh(cliff(31)); call("CLIFFESCAPESLOW", "main", INn); // grounding 32
        fresh(cliff(40)); call("CLIFFESCAPESLOW", "main", INn); // setVel
        fresh(cliff(80)); call("CLIFFESCAPESLOW", "interrupt", INn); // >79
        fresh(cliff(5)); call("CLIFFJUMPQUICK", "main", INn); // pos arm
        fresh(cliff(14)); call("CLIFFJUMPQUICK", "main", INn); // cVel at 15
        fresh(cliff(16)); call("CLIFFJUMPQUICK", "main", INn); // drift arm
        fresh(cliff(39)); call("CLIFFJUMPQUICK", "interrupt", INn); // FALL >38
        fresh(cliff(10)); call("CLIFFJUMPSLOW", "main", INn); // pos arm
        fresh(cliff(17)); call("CLIFFJUMPSLOW", "main", INn); // cVel at 18
        fresh(cliff(20)); call("CLIFFJUMPSLOW", "main", INn); // drift arm
        fresh(cliff(39)); call("CLIFFJUMPSLOW", "interrupt", INn); // FALL >38
        fresh(cliff(5)); call("CLIFFATTACKQUICK", "main", INn); // pos arm
        fresh(cliff(14)); call("CLIFFATTACKQUICK", "main", INn); // grounding
        fresh(cliff(18)); call("CLIFFATTACKQUICK", "main", INn); // 19 active
        // + normalswing2 (+ setVel)
        fresh(cliff(21)); call("CLIFFATTACKQUICK", "main", INn); // frame++
        fresh(cliff(23)); call("CLIFFATTACKQUICK", "main", INn); // 24 off
        fresh(cliff(56)); call("CLIFFATTACKQUICK", "interrupt", INn); // >55
        fresh(cliff(5)); call("CLIFFATTACKSLOW", "main", INn); // pos arm
        fresh(cliff(32)); call("CLIFFATTACKSLOW", "main", INn); // grounding 33
        fresh(cliff(40)); call("CLIFFATTACKSLOW", "main", INn); // setVel arm
        fresh(cliff(42)); call("CLIFFATTACKSLOW", "main", INn); // 43 active
        fresh(cliff(50)); call("CLIFFATTACKSLOW", "main", INn); // frame++
        fresh(cliff(59)); call("CLIFFATTACKSLOW", "main", INn); // 60 off
        fresh(cliff(70)); call("CLIFFATTACKSLOW", "interrupt", INn); // >69
        // grab family + downattack + appeal
        fresh(); call("GRAB", "init", INn);
        fresh((p) => { p.timer = 6; }); call("GRAB", "main", INn); // 7 active
        fresh((p) => { p.timer = 7; }); call("GRAB", "main", INn); // frame++
        fresh((p) => { p.timer = 8; }); call("GRAB", "main", INn); // 9 off
        fresh((p) => { p.timer = 31; });
        call("GRAB", "interrupt", INn); // WAIT (>30)
        fresh(); call("CATCHATTACK", "init", INn);
        fresh((p) => { p.timer = 9; });
        call("CATCHATTACK", "main", INn); // 10 active
        fresh((p) => { p.timer = 10; });
        call("CATCHATTACK", "main", INn); // 11 off
        fresh((p) => { p.timer = 31; });
        call("CATCHATTACK", "interrupt", INn); // CATCHWAIT (>30)
        fresh(); call("DOWNATTACK", "init", INn);
        fresh((p) => { p.timer = 0; });
        call("DOWNATTACK", "main", INn); // 1 intangibleTimer 15
        fresh((p) => { p.timer = 19; });
        call("DOWNATTACK", "main", INn); // 20 active + sword2
        fresh((p) => { p.timer = 20; });
        call("DOWNATTACK", "main", INn); // 21 frame++
        fresh((p) => { p.timer = 21; });
        call("DOWNATTACK", "main", INn); // 22 off
        fresh((p) => { p.timer = 29; });
        call("DOWNATTACK", "main", INn); // 30 downattack2 swap
        fresh((p) => { p.timer = 31; });
        call("DOWNATTACK", "main", INn); // 32 off
        fresh((p) => { p.timer = 50; });
        call("DOWNATTACK", "interrupt", INn); // WAIT (>49)
        fresh(); call("APPEAL", "init", INn); // pufftaunt
        fresh((p) => { p.timer = 5; }); call("APPEAL", "main", INn);
        fresh((p) => { p.timer = 101; });
        call("APPEAL", "interrupt", INn); // WAIT (>100)
      } finally {
        Math.random = savedRandom;
        M.player[3] = savedP3;
        M.playerType[3] = savedT3;
        M.player[2] = savedP2;
        M.playerType[2] = savedT2;
        M.characterSelections[3] = savedCS3;
        // restore the global charHitboxes dmg/size fields the rollout/sing
        // sweep arms mutated (rule 12 net-restore — the whole puff plane)
        for (const [hb, dmg, size] of puffHb) { hb.dmg = dmg; hb.size = size; }
        // net-restore the article/hq globals the sweep touched (rule 12)
        ART.aArticles.splice(savedArticles);
        HD.hitQueue.splice(savedHq);
      }
      return n;
    },
  };
})();
