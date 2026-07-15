// spec-article.js — M2 task 13 capture spec: articles (src/physics/
// article.js — fox/falco LASER + ILLUSION queues, article hit detection,
// article hit execution). Registers window.__capSpecs.article. Injected
// AFTER capturelib.js. See FORMAT.md "The article spec".
//
// BOUNDARY (13 article-module fns + the per-char seam surface):
// - the 4 gameTick pipeline calls (main.js:1059-1060, 1077-1078):
//   destroyArticles, executeArticles, articlesHitDetection,
//   executeArticleHits — MUTATION-captured with pre/post envelopes over
//   the article queues (aArticles / destroyArticleQueue / articleHitQueue
//   — the checksummed `articles` key of CHECKSUM.md §2 is exactly
//   aArticles);
// - resetAArticles — endGame-only upstream (main.js:1376; goldens never
//   end the match: trace quality contract) — wrapped, PINNED ZERO;
// - articles.LASER.init / articles.ILLUSION.init ("ainit" records) — the
//   move-side spawn boundary (the SAME crossings tasks 8/9 recorded as
//   4-field article seams from the fox/falco side; here they are
//   mutation-captured with pre/post and become REAL C bodies);
// - the 6 exported collision helpers (wallDetection, articleHitCollision,
//   articleShieldCollision, articleHurtCollision,
//   interpolatedArticleCircleCollision, interpolatedArticleHurtCollision)
//   have ONLY internal callers upstream (babel local bindings; targetplay
//   is M4 target mode) — identity-wrapped, pinned 0 records;
// - renderArticles / articles.*.draw are render-only (fg2 canvas) —
//   asserted present, NOT wrapped (drawECB precedent);
// - every NON-shared actionStates entry fn is a SEAM logger (the
//   moves-shared machinery verbatim): executeArticleHits dispatches
//   GUARD/SHIELDBREAKFALL/DAMAGEFLYN/DAMAGEN2/CAPTUREDAMAGE .init — all
//   SHARED-origin on every char table, so the C replay runs task 7's real
//   bodies; a nested dispatch reaching a per-char state records
//   "mdispatch" (measured zero expected; FIFO teeth as in tasks 7-12).
//
// ENVELOPES (lean-when-empty, a measured READ-SET projection — the
// queue-emptiness gate): when the driving queue is empty at entry the
// upstream body reads NOTHING beyond the queues (loop bodies never run),
// so the record carries only the lean queue envelope
// {aArt, ahq, dstq}. Full shapes:
// - ainit / executeArticles(non-empty): pre {aArt, ahq, dstq, playerType,
//   players, stage(5 surface lists — wallDetection's read set)}, post
//   lean (inits/mains write only article state).
// - articlesHitDetection(non-empty): pre {aArt, ahq, dstq, playerType,
//   players}, post {aArt, ahq, dstq, players, snd} (players: the
//   canTurboCancel hasHit write; snd: foxshinereflect).
// - executeArticleHits(non-empty): args [inputs(8-deep x4), pre],
//   pre {aArt, ahq, alias(probe4), characterSelections, dstq, gameMode,
//   gameSettings{tapJumpOffp1..4}, hq(opaque), playerType, players,
//   stage, versusMode} (the moves-record superset — dispatched shared
//   inits run real C bodies), post {aArt, ahq, alias, dstq, hq, players,
//   rng, snd, vfx}.
// - destroyArticles / resetAArticles: lean always (queue-only read/write
//   sets).
// RNG: rngBoot + the moves discipline (owner draws in eah posts —
// screenShake's 4 per landed hit; window draws in mdispatch seam posts;
// everything else standalone "Math.random"). percentShake is native-RNG
// (CHECKSUM.md §7) and never appears.
// hdFlags (frame-0, drift-guarded): executeArticleHits reads
// actionStates[cs[v]][state].crouch/.vCancel — the hitdet spec's 7-key
// dump reused verbatim. mvData (frame-0, drift-guarded): the moves-shared
// chars dump — the C replay registers task 7's shared bodies from its
// measured sharedOrigin map and serves the data seams.
(() => {
  const EXCLUDE = ["charAttributes", "charHitboxes", "percentShake"];
  const FLAG_KEYS = ["canBeGrabbed", "crouch", "downed", "name",
                     "specialClank", "specialOnHit", "vCancel"];
  const SND_KEYS = ["CLIFFCATCH", "DEAD", "ESCAPEAIR", "ESCAPEB", "ESCAPEF",
                    "ESCAPEN", "FURAFURA", "GUARDOFF", "GUARDON", "JUMP",
                    "JUMPAERIAL", "OTTOTTOWAIT", "TECH"];
  const SETVEL_KEYS = ["DOWNSTANDB", "DOWNSTANDF", "ESCAPEB", "ESCAPEF",
                       "TECHB", "TECHF"];
  const ZERO = ["wallDetection", "articleHitCollision",
                "articleShieldCollision", "articleHurtCollision",
                "interpolatedArticleCircleCollision",
                "interpolatedArticleHurtCollision"];

  window.__capSpecs.article = {
    // 5 pipeline/lifecycle (4 gameTick calls + resetAArticles) + 2 ainit
    // + 6 zero-pinned helpers + 1088 per-char seam fns (marth 244 /
    // puff 221 / fox 192 / falco 214 / falcon 217 — the tasks 8-12
    // measured per-char surfaces)
    expectWrapped: 1101,

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
        typeof ex.executeArticles === "function" &&
        typeof ex.destroyArticles === "function" &&
        typeof ex.articlesHitDetection === "function" &&
        typeof ex.executeArticleHits === "function", "article");
      if (!Array.isArray(ART.aArticles) ||
          !Array.isArray(ART.destroyArticleQueue) ||
          !Array.isArray(ART.articleHitQueue)) {
        throw new Error("article spec: exported queues missing");
      }
      // render-only surfaces asserted, never wrapped (drawECB precedent)
      if (typeof ART.renderArticles !== "function" ||
          typeof ART.articles.LASER.draw !== "function" ||
          ART.articles.ILLUSION.noDraw !== true) {
        throw new Error("article spec: render surface shape drifted");
      }

      // --- owner stack (moves-shared semantics verbatim) -----------------
      const stack = [];
      const top = () => (stack.length ? stack[stack.length - 1] : null);

      // --- seeded-RNG stream ----------------------------------------------
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

      // --- sound attribution (incl. ".stop" tokens) ------------------------
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
        throw new Error("article spec: expected 180 Howl sounds, wrapped " +
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

      // --- vfx attribution ---------------------------------------------------
      const VFX = find((ex) => typeof ex.drawVfx === "function", "drawVfx");
      {
        const orig = VFX.drawVfx;
        VFX.drawVfx = function (cfg) {
          const t = top();
          if (t && t.attr) t.vfx.push(cfg.name);
          return orig.apply(this, arguments);
        };
      }

      // --- projections ----------------------------------------------------------
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
      // the article stage read set: wallDetection's five surface lists
      // (the envcoll projection — article.js dereferences nothing else on
      // activeStage in sim code; scale/offset are draw()-only)
      const stageProj = () => {
        const st = AST.getActiveStage();
        if (!st) return null; // pre-setupMatch sweep records
        return {
          ceiling: st.ceiling, ground: st.ground, platform: st.platform,
          wallL: st.wallL, wallR: st.wallR,
        };
      };
      const isGodInput = (v) =>
        Array.isArray(v) && v.length === 4 &&
        v.every((b) => Array.isArray(b));
      const inputsCanon = (v) =>
        "[" + [0, 1, 2, 3].map((k) =>
          M.playerType[k] > -1 ? ctx.canon(v[k].slice(0, 8)) : "null")
          .join(",") + "]";

      // --- envelope builders -------------------------------------------------
      const leanEnv = () =>
        '{"aArt":' + ctx.canon(ART.aArticles) +
        ',"ahq":' + ctx.canon(ART.articleHitQueue) +
        ',"dstq":' + ctx.canon(ART.destroyArticleQueue) + "}";
      const spawnEnv = () => // ainit + non-empty executeArticles pre
        '{"aArt":' + ctx.canon(ART.aArticles) +
        ',"ahq":' + ctx.canon(ART.articleHitQueue) +
        ',"dstq":' + ctx.canon(ART.destroyArticleQueue) +
        ',"playerType":' + ctx.canon(M.playerType) +
        ',"players":' + playersCanon() +
        ',"stage":' + ctx.canon(stageProj()) + "}";
      const ahdPreEnv = () =>
        '{"aArt":' + ctx.canon(ART.aArticles) +
        ',"ahq":' + ctx.canon(ART.articleHitQueue) +
        ',"dstq":' + ctx.canon(ART.destroyArticleQueue) +
        ',"playerType":' + ctx.canon(M.playerType) +
        ',"players":' + playersCanon() + "}";
      const ahdPostEnv = (fr) =>
        '{"aArt":' + ctx.canon(ART.aArticles) +
        ',"ahq":' + ctx.canon(ART.articleHitQueue) +
        ',"dstq":' + ctx.canon(ART.destroyArticleQueue) +
        ',"players":' + playersCanon() +
        ',"snd":' + ctx.canon(fr.snd) + "}";
      const eahPreEnv = () =>
        '{"aArt":' + ctx.canon(ART.aArticles) +
        ',"ahq":' + ctx.canon(ART.articleHitQueue) +
        ',"alias":' + aliasCanon() +
        ',"characterSelections":' + ctx.canon(M.characterSelections) +
        ',"dstq":' + ctx.canon(ART.destroyArticleQueue) +
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
      const eahPostEnv = (fr) =>
        '{"aArt":' + ctx.canon(ART.aArticles) +
        ',"ahq":' + ctx.canon(ART.articleHitQueue) +
        ',"alias":' + aliasCanon() +
        ',"dstq":' + ctx.canon(ART.destroyArticleQueue) +
        ',"hq":' + ctx.canon(HD.hitQueue) +
        ',"players":' + playersCanon() +
        ',"rng":' + ctx.canon(fr.rng) +
        ',"snd":' + ctx.canon(fr.snd) +
        ',"vfx":' + ctx.canon(fr.vfx) + "}";

      // --- the pipeline wrappers ------------------------------------------------
      // preFn() -> {canon, lean}: leanness is decided ONCE at pre time and
      // passed to postFn(fr, lean) / argsFn(args, lean) so the post
      // envelope mirrors it. Lean records must stay event-free (asserted):
      // an empty driving queue means the upstream loop body never runs.
      const wrapArt = (name, preFn, postFn, argsFn) => {
        const orig = ART[name];
        if (typeof orig !== "function") {
          throw new Error("article spec: missing export " + name);
        }
        ART[name] = function () {
          const args = Array.prototype.slice.call(arguments);
          const pre = preFn();
          const argsCanon = "[" +
              argsFn(args, pre.lean).concat([pre.canon]).join(",") + "]";
          const fr = { attr: true, rng: [], snd: [], vfx: [] };
          stack.push(fr);
          let ret;
          try {
            ret = orig.apply(this, arguments);
          } finally {
            stack.pop();
          }
          if (pre.lean && (fr.rng.length || fr.snd.length || fr.vfx.length)) {
            throw new Error("article spec: events escaped lean " + name);
          }
          ctx.push(name, argsCanon, ctx.canon(ret), postFn(fr, pre.lean));
          return ret;
        };
        ctx.declare(name);
        ctx.wrapped++;
      };

      const quietLean = (name) => (fr, lean) => {
        if (fr.rng.length || fr.snd.length || fr.vfx.length) {
          throw new Error("article spec: events escaped " + name);
        }
        void lean;
        return leanEnv();
      };
      wrapArt("destroyArticles",
        () => ({ canon: leanEnv(), lean: true }),
        quietLean("destroyArticles"), () => []);
      wrapArt("resetAArticles", // endGame lifecycle — pinned ZERO live
        () => ({ canon: leanEnv(), lean: true }),
        quietLean("resetAArticles"), () => []);
      wrapArt("executeArticles",
        () => (ART.aArticles.length === 0
                 ? { canon: leanEnv(), lean: true }
                 : { canon: spawnEnv(), lean: false }),
        quietLean("executeArticles"), () => []);
      wrapArt("articlesHitDetection",
        () => (ART.aArticles.length === 0
                 ? { canon: leanEnv(), lean: true }
                 : { canon: ahdPreEnv(), lean: false }),
        (fr, lean) => {
          if (fr.rng.length || fr.vfx.length) {
            throw new Error(
              "article spec: rng/vfx escaped articlesHitDetection");
          }
          return lean ? leanEnv() : ahdPostEnv(fr);
        },
        () => []);
      wrapArt("executeArticleHits",
        () => (ART.articleHitQueue.length === 0
                 ? { canon: leanEnv(), lean: true }
                 : { canon: eahPreEnv(), lean: false }),
        (fr, lean) => (lean ? leanEnv() : eahPostEnv(fr)),
        (args, lean) => {
          if (!lean && !isGodInput(args[0])) {
            throw new Error("article spec: executeArticleHits arg 0 is " +
                            "not the god input");
          }
          return [lean ? "null" : inputsCanon(args[0])];
        });

      // --- the ainit boundary (LASER/ILLUSION spawn records) ---------------------
      const wrapArticleInit = (name) => {
        const obj = ART.articles[name];
        const orig = obj.init;
        obj.init = function (options) {
          const pre = spawnEnv();
          const argsCanon = "[" + JSON.stringify(name) + "," +
              ctx.canon(options) + "," + pre + "]";
          const fr = { attr: true, rng: [], snd: [], vfx: [] };
          stack.push(fr);
          let ret;
          try {
            ret = orig.apply(this, arguments);
          } finally {
            stack.pop();
          }
          if (fr.rng.length || fr.snd.length || fr.vfx.length) {
            throw new Error("article spec: events escaped " + name + ".init");
          }
          ctx.push("ainit", argsCanon, ctx.canon(ret), leanEnv());
          return ret;
        };
        ctx.wrapped++;
      };
      wrapArticleInit("LASER");
      wrapArticleInit("ILLUSION");
      ctx.declare("ainit");

      // --- zero-pinned exports (internal-only callers upstream) ------------------
      for (const name of ZERO) {
        ctx.wrapExport(ART, name, name, null);
      }

      // --- per-char move entries: mdispatch seam loggers --------------------------
      // (moves-shared machinery verbatim; shared entries stay UNWRAPPED —
      // at top level they are chain-safe silent surface, under an article
      // record they are the transparent nested C tree.)
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
            throw new Error("article spec: seam " + name + "." + k +
                            " has an unexpected signature");
          }
          const argsCanon = "[" + JSON.stringify(k) + "," +
              JSON.stringify(name) + "," + ctx.canon([args[0]]) + "]";
          const w = { attr: false, seam: true, rng: [] };
          stack.push(w);
          let ret;
          try {
            ret = orig.apply(this, arguments);
          } finally {
            stack.pop();
          }
          const post = '{"alias":' + aliasCanon() +
              ',"hq":' + ctx.canon(HD.hitQueue) +
              ',"players":' + playersCanon() +
              ',"rng":' + ctx.canon(w.rng) + "}";
          ctx.push("mdispatch", argsCanon, ctx.canon(ret), post);
          return ret;
        };
        ctx.wrapped++;
      };

      const seen = new WeakSet();
      const sharedOrigin = [{}, {}, {}, {}, {}];
      const nameMap = [{}, {}, {}, {}, {}];
      const perTableSeam = [0, 0, 0, 0, 0];
      for (let c = 0; c < AS.actionStates.length; c++) {
        const tbl = AS.actionStates[c];
        if (!tbl) continue;
        for (const st of Object.keys(tbl)) {
          const entry = tbl[st];
          if (!entry || typeof entry !== "object") continue;
          if (typeof entry.name !== "string") {
            throw new Error("article spec: move object without a name");
          }
          nameMap[c][st] = entry.name;
          const sh = SHARED[st] !== undefined &&
                     typeof SHARED[st].init === "function" &&
                     entry.init === SHARED[st].init;
          sharedOrigin[c][st] = sh;
          if (seen.has(entry)) {
            throw new Error("article spec: entry object shared across tables");
          }
          seen.add(entry);
          if (sh) continue; // transparent (task-7 C bodies linked)
          for (const k of Object.keys(entry)) {
            if (typeof entry[k] !== "function") continue;
            wrapSeamFn(entry, k);
            perTableSeam[c]++;
          }
        }
      }
      // measured per-char surfaces (tasks 8-12): marth/puff/fox/falco/falcon
      const WANT_SEAM = [244, 221, 192, 214, 217];
      for (let c = 0; c < 5; c++) {
        if (perTableSeam[c] !== WANT_SEAM[c]) {
          throw new Error("article spec: table " + c + " wrapped " +
                          perTableSeam[c] + " seam fns, expected " +
                          WANT_SEAM[c]);
        }
      }
      ctx.declare("mdispatch");

      // --- hdFlags (hitdet dump reused: eah reads crouch/vCancel) -----------------
      const flagsCanon = () => {
        const dump = {};
        for (let c = 0; c < AS.actionStates.length; c++) {
          const tbl = AS.actionStates[c];
          if (!tbl) continue;
          const chr = {};
          for (const st of Object.keys(tbl)) {
            const e = tbl[st];
            const f = {};
            for (const k of FLAG_KEYS) f[k] = e[k];
            chr[st] = f;
          }
          dump[c] = chr;
        }
        return ctx.canon(dump);
      };
      this._flagsAtInstall = flagsCanon();
      this._flagsCanon = flagsCanon;
      ctx.declare("hdFlags");
      ctx.push("hdFlags", ctx.canon(["all"]), this._flagsAtInstall);

      // --- mvData (moves-shared chars dump: shared registration + data seams) ------
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
      this._ART = ART;
      this._HD = HD;
      this._stack = stack;
      return { moduleIds: moduleIds };
    },

    finalCheck() {
      if (this._flagsCanon() !== this._flagsAtInstall) {
        throw new Error("article spec: actionStates flags drifted");
      }
      if (this._mvDataCanon() !== this._mvDataAtInstall) {
        throw new Error("article spec: mvData drifted during the match");
      }
      return 2;
    },

    // Synthetic-domain sweep (rules 11/12): the zero-live article arms.
    // Owner (slot 2) + victim (slot 3) are REAL upstream playerObject(2,·)
    // injections (restored exactly); Math.random is swapped for the sweep
    // mulberry32 (0x0badf00d) — the C replay mirrors it for ALL frame-0
    // records; article/queue state is torn down between arms by direct
    // frame-0 pokes (the replay marshals every record's PRE, and the
    // chain-verify instrument only arms from the first in-match record).
    // Net restore: both player slots + all three queues emptied — the ×2
    // byte-stability + STREAM MATCH guards prove non-perturbation (a
    // leaked article would enter the checksummed `articles` key).
    sweep() {
      const M = this._M;
      const PJ = this._PJ;
      const ART = this._ART;
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

      const savedP2 = M.player[2], savedT2 = M.playerType[2];
      const savedCS2 = M.characterSelections[2];
      const savedP3 = M.player[3], savedT3 = M.playerType[3];
      const savedCS3 = M.characterSelections[3];
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
        M.playerType[2] = 0; M.characterSelections[2] = 2; // owner: fox
        M.playerType[3] = 0; M.characterSelections[3] = 2; // victim
        const freshOwner = (mut) => {
          M.player[2] = new PJ.playerObject(2, [0, 10], 1);
          if (mut) mut(M.player[2]);
        };
        const freshVictim = (mut) => {
          M.player[3] = new PJ.playerObject(2, [40, 10], 1);
          const p = M.player[3];
          p.actionState = "FALL"; // vCancel:true / crouch:false flags
          p.phys.hurtbox = { min: { x: 36, y: 19 }, max: { x: 44, y: 1 } };
          if (mut) mut(p);
        };
        const clearArt = () => {
          ART.aArticles.splice(0);
          ART.destroyArticleQueue.splice(0);
          ART.articleHitQueue.splice(0);
        };
        const laser = (opts) => {
          ART.articles.LASER.init(opts); n++;
          return ART.aArticles[ART.aArticles.length - 1].instance;
        };
        const illusion = (opts) => {
          ART.articles.ILLUSION.init(opts); n++;
          return ART.aArticles[ART.aArticles.length - 1].instance;
        };
        const at = (inst, x, y) => { // park an article on a point
          inst.pos.x = x; inst.pos.y = y;
          inst.posPrev.x = x; inst.posPrev.y = y;
        };
        const exec = () => { ART.executeArticles(); n++; };
        const destroy = () => { ART.destroyArticles(); n++; };
        const ahd = () => { ART.articlesHitDetection(); n++; };
        const eah = () => { ART.executeArticleHits(IN); n++; };
        const V7 = () => ({ // chars-data-shaped reflect hitbox (type 7)
          offset: [{ x: 0, y: 0 }], size: 12, dmg: 8, angle: 361,
          kg: 100, bk: 0, sk: 0, type: 7, clank: 1, hitGrounded: 1,
          hitAirborne: 1, throwextra: false,
        });

        freshOwner(); freshVictim();

        // --- spawn arms (ainit) + movement ladder --------------------------
        const L1 = laser({ p: 2, x: 8, y: 7, rotate: 0 });    // fox default-isFox arm
        laser({ p: 2, x: 8, y: 9, rotate: 0, isFox: false }); // falco laser
        laser({ p: 2, x: 1, y: 12, rotate: Math.PI * 275 / 180,
                isFox: false, partOfThrow: true });           // throw laser
        illusion({ p: 2, type: 1 });                          // ground kg patch
        illusion({ p: 2, type: 0, isFox: false });            // isFox||true quirk
        exec(); exec(); exec(); exec(); exec(); // posPrev ladder t=2..6
        L1.timer = 200; exec();                 // timer>200 death push
        destroy();                              // splice consumption
        clearArt();
        // wall death: straight-down laser above the main platform
        freshOwner((p) => { p.phys.pos.y = 6; });
        laser({ p: 2, x: 0, y: -1, rotate: -Math.PI / 2 });
        exec(); exec();                          // crosses ground -> collision
        destroy();
        clearArt();
        // duplicate-destroy splice quirk (JS splice(-1) removes from END):
        laser({ p: 2, x: 8, y: 7, rotate: 0 });
        laser({ p: 2, x: 8, y: 9, rotate: 0 });
        ART.destroyArticleQueue.push(0); ART.destroyArticleQueue.push(0);
        destroy();
        clearArt();
        freshOwner();

        // --- articlesHitDetection arms --------------------------------------
        // direct hurt hit, LASER (destroyOnHit push) then eah normal arm
        freshVictim();
        at(laser({ p: 2, x: 8, y: 0, rotate: 0 }), 39.5, 10);
        ahd(); eah(); // DAMAGEN2 low-kb + angle-361->0 + reverse false
        clearArt();
        // hurt hit, ILLUSION (hasHit; type-1 vfx trio at eah)
        freshVictim();
        at(illusion({ p: 2, type: 0 }), 40.5, 10);
        ahd(); eah(); // reverse true arm (article right of victim)
        clearArt();
        // shield hit, LASER -> eah shield arm (push +, GUARD dispatch)
        freshVictim((p) => {
          p.phys.shielding = true;
          p.phys.shieldPositionReal = { x: 40, y: 10 };
          p.phys.shieldSize = 7;
          p.phys.shieldAnalog = 1;
          p.phys.shieldHP = 60;
        });
        at(laser({ p: 2, x: 8, y: 0, rotate: 0 }), 39, 10);
        ahd(); eah();
        clearArt();
        // shield: push cap + negative direction + big damage
        freshVictim((p) => {
          p.phys.shielding = true;
          p.phys.shieldPositionReal = { x: 40, y: 10 };
          p.phys.shieldSize = 7;
          p.phys.shieldAnalog = 0.5;
          p.phys.shieldHP = 60;
        });
        {
          const i = laser({ p: 2, x: 8, y: 0, rotate: 0 });
          i.hb.dmg = 20;
          at(i, 41, 10);
        }
        ahd(); eah();
        clearArt();
        // shieldbreak arm (break exits the row loop: 2nd row unprocessed)
        freshVictim((p) => {
          p.phys.shielding = true;
          p.phys.shieldPositionReal = { x: 40, y: 10 };
          p.phys.shieldSize = 7;
          p.phys.shieldHP = 1;
        });
        at(laser({ p: 2, x: 8, y: 0, rotate: 0 }), 39, 10);
        ahd();
        ART.articleHitQueue.push([0, 3, true]); // crafted 2nd row
        eah();
        clearArt();
        // powershield reflect, LASER (vel flip) then ILLUSION (no vel)
        freshVictim((p) => {
          p.phys.shielding = true;
          p.phys.shieldPositionReal = { x: 40, y: 10 };
          p.phys.shieldSize = 7;
          p.phys.powerShieldReflectActive = true;
        });
        at(laser({ p: 2, x: 8, y: 0, rotate: 0 }), 39, 10);
        ahd(); eah();
        clearArt();
        freshVictim((p) => {
          p.phys.shielding = true;
          p.phys.shieldPositionReal = { x: 40, y: 10 };
          p.phys.shieldSize = 7;
          p.phys.powerShieldReflectActive = true;
        });
        at(illusion({ p: 2, type: 0 }), 39, 10);
        ahd(); eah();
        clearArt();
        // reflect: type-7 hitbox + DOWNSPECIAL sound + vel flip + dmg*1.5
        freshVictim((p) => {
          p.actionState = "DOWNSPECIALGROUND";
          p.hitboxes.active[0] = true;
          p.hitboxes.frame = 0;
          p.hitboxes.id[0] = V7();
        });
        at(laser({ p: 2, x: 8, y: 0, rotate: 0 }), 40, 10);
        ahd();
        clearArt();
        // reflect on ILLUSION: vel-absent check arm, no shine sound
        freshVictim((p) => {
          p.hitboxes.active[1] = true;
          p.hitboxes.frame = 0;
          p.hitboxes.id[1] = V7();
        });
        at(illusion({ p: 2, type: 0 }), 40, 10);
        ahd();
        clearArt();
        // interpolated reflect (sweep circle-vs-circle straddle)
        freshVictim((p) => {
          p.hitboxes.active[0] = true;
          p.hitboxes.frame = 0;
          p.hitboxes.id[0] = V7();
        });
        {
          const i = laser({ p: 2, x: 8, y: 0, rotate: 0 });
          i.timer = 3;
          i.posPrev.x = 30; i.posPrev.y = 10;
          i.pos.x = 50; i.pos.y = 10;
        }
        ahd();
        clearArt();
        // interpolated hurt (sweep circle vs hurtbox AABB)
        freshVictim();
        {
          const i = laser({ p: 2, x: 8, y: 0, rotate: 0 });
          i.timer = 3;
          i.posPrev.x = 30; i.posPrev.y = 10;
          i.pos.x = 55; i.pos.y = 10;
        }
        ahd();
        clearArt();
        // interpolated shield (posPrev-on-shield previous=true arm)
        freshVictim((p) => {
          p.phys.shielding = true;
          p.phys.shieldPositionReal = { x: 40, y: 10 };
          p.phys.shieldSize = 7;
        });
        {
          const i = laser({ p: 2, x: 8, y: 0, rotate: 0 });
          i.timer = 3;
          i.posPrev.x = 40; i.posPrev.y = 10;
          i.pos.x = 70; i.pos.y = 10;
        }
        ahd();
        clearArt();
        // hitList skip + clank read path + hurtBoxState=1 skip + miss
        freshVictim((p) => {
          p.hitboxes.active[0] = true;
          p.hitboxes.frame = 0;
          p.hitboxes.id[0] = { offset: [{ x: 0, y: 0 }], size: 5, dmg: 8,
            angle: 361, kg: 100, bk: 0, sk: 0, type: 0, clank: 1,
            hitGrounded: 1, hitAirborne: 1, throwextra: false };
        });
        {
          const i = illusion({ p: 2, type: 0 }); // clank:true article
          i.hitList.push(3);
          at(i, 40, 10);
        }
        ahd(); // clank loop evaluated; hitList blocks the row
        clearArt();
        freshVictim((p) => { p.phys.hurtBoxState = 1; });
        at(laser({ p: 2, x: 8, y: 0, rotate: 0 }), 40, 10);
        ahd(); // hurtBoxState==1: hurt check skipped entirely
        clearArt();
        freshVictim();
        laser({ p: 2, x: 8, y: 0, rotate: 0 }); // spawns far from victim
        ahd(); // clean miss (no rows)
        clearArt();

        // --- executeArticleHits-only arms (crafted rows) ---------------------
        const hurtRow = (mutV, mutA, kind) => {
          freshVictim(mutV);
          const i = kind === "L"
            ? laser({ p: 2, x: 8, y: 0, rotate: 0 })
            : illusion({ p: 2, type: 0 });
          at(i, 39.5, 10);
          if (mutA) mutA(i);
          ART.articleHitQueue.push([0, 3, false]);
          eah();
          clearArt();
        };
        // kb >= 80 -> DAMAGEFLYN(!isThrow) + angle-361->44 arm
        hurtRow((p) => { p.percent = 150; }, (i) => { i.hb.dmg = 40; }, "L");
        // groundBounce: grounded + angle 270 + kb >= 80
        hurtRow((p) => { p.percent = 150; p.phys.grounded = true; },
                (i) => { i.hb.dmg = 40; i.hb.angle = 270; }, "L");
        // vCancel arm (FALL vCancel:true + vCancelTimer)
        hurtRow((p) => { p.phys.vCancelTimer = 5; }, null, "L");
        // crouch arm (SQUATWAIT crouch:true kb reduction)
        hurtRow((p) => { p.actionState = "SQUATWAIT"; }, null, "L");
        // CAPTUREDAMAGE (grabbedBy > -1, kb <= 50)
        hurtRow((p) => { p.phys.grabbedBy = 2; }, null, "L");
        // THROWNPUFFDOWN skip arm (no dispatch)
        hurtRow((p) => { p.phys.grabbedBy = 2;
                         p.actionState = "THROWNPUFFDOWN"; }, null, "L");
        // grabbedBy > -1 && kb > 50 -> DAMAGEFLYN path
        hurtRow((p) => { p.phys.grabbedBy = 2; p.percent = 150; },
                (i) => { i.hb.dmg = 40; }, "L");
        // blunthit (hurtBoxState 2)
        hurtRow((p) => { p.phys.hurtBoxState = 2; }, null, "L");
      } finally {
        Math.random = savedRandom;
        ART.aArticles.splice(0);
        ART.destroyArticleQueue.splice(0);
        ART.articleHitQueue.splice(0);
        M.player[2] = savedP2; M.playerType[2] = savedT2;
        M.characterSelections[2] = savedCS2;
        M.player[3] = savedP3; M.playerType[3] = savedT3;
        M.characterSelections[3] = savedCS3;
      }
      return n;
    },
  };
})();

