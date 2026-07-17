// spec-hitdet.js — M2 task 6 capture spec: hitDetection + hitQueue +
// phantomQueue + the launch getters. Registers window.__capSpecs.hitdet.
// Injected AFTER capturelib.js. See FORMAT.md "The hitdet spec".
//
// Wrapped boundary (expectWrapped = 29): every exported FUNCTION of
// src/physics/hitDetection.js (hitQueue/phantomQueue are exported arrays,
// asserted present, not wrapped). Live external callers over the goldens:
// - main.js gameTick mode 3: resetHitQueue, checkPhantoms, hitDetect(x4),
//   executeHits — the queue-ordered resolution pipeline (MUTATION-captured
//   with full pre/post envelopes).
// - physics.js hitlagSwitchUpdate: the 5 launch getters (pure records here
//   — task 5 recorded them as oracle-fed seams; this spec replays their
//   REAL C translations).
// - article.js: getKnockback / getHitstun / knockbackSounds /
//   segmentSegmentCollision (pure-ish records; article is task 13).
// Everything else has ONLY internal callers upstream (babel local
// bindings, correctly not captured): wrapped with identity projections and
// pinned at 0 records — a record appearing = a new external caller =
// loud pin failure.
//
// ENVELOPES (the physics-spec discipline): hitDetect/executeHits/
// checkPhantoms are mutation-captured with ONE uniform envelope shape —
// pre {alias, characterSelections, gameMode, gameSettings
// {phantomThreshold}, hq, phq, playerType, players}, post {alias(3), hq,
// phq, players, rng, snd}. hq/phq are the FULL exported hitQueue/
// phantomQueue arrays (canon'd whole): rows enter the queue from OTHER
// clusters' windows (THROW moves push during update; physics pushes
// damaging-stage rows), so the queues are marshalled per record, never
// chained. resetHitQueue/setPhantonQueue are lean (queue-only read/write
// set — they run before update(i), when frame-1 players still lack
// prevFrameHitboxes, so the alias probe must not run).
//
// ORACLE-FED SEAMS: top-level move dispatches from inside a wrapped hitdet
// boundary (CATCHCUT/DAMAGEN2/DAMAGEFLYN/GUARD/SHIELDBREAKFALL/
// FURASLEEPSTART/CAPTUREPULLED/THROWNFALCONDIVE/WAIT/DOWNDAMAGE/
// CAPTUREDAMAGE/CAPTURECUT .init + .onClank/.onPlayerHit) produce 5-field
// "dispatch" records — args [phase, moveName, [slot, ...extras]], post
// {alias(4), hq, players} — verified in call order and RESYNCED by the
// replay (moves are tasks 7-12). phantomQueue is NOT in dispatch posts:
// no move module imports it (grep-verified); a move write would diverge
// the outer post's phq loudly.
//
// RNG channel (asshort discipline): rngBoot + owner-attributed draws.
// hitdet's own draws = screenShake's 4 seeded draws per regular hit
// (main.js:352-358 — fg1.translate is render-only, the DRAWS are stream
// state) -> the boundary record's post `rng` list. Draws inside dispatch
// windows would be "Math.randomW" records (pinned 0 — measured; the
// replay treats any as out-of-domain); all other draws (moves during
// update, startGame) are standalone "Math.random" records burning the
// chain. percentShake uses the stashed native RNG and never appears.
//
// hdFlags (frame-0 record): the actionStates per-(char,state) DATA plane
// hitDetection branches on — {canBeGrabbed, crouch, downed, name,
// specialClank, specialOnHit, vCancel} — dumped for all 5 chars;
// finalCheck() re-dumps after the last frame and hard-fails on ANY drift.
(() => {
  const EXCLUDE = ["charAttributes", "charHitboxes", "percentShake"];
  const FLAG_KEYS = ["canBeGrabbed", "crouch", "downed", "name",
                     "specialClank", "specialOnHit", "vCancel"];

  window.__capSpecs.hitdet = {
    expectWrapped: 29,

    install(ctx) {
      const cache = ctx.cache;
      const moduleIds = {};
      const find = (pred, what) => {
        const m = ctx.findModule(cache, pred, what);
        moduleIds[what] = m.id;
        return m.exports;
      };

      const HD = find((ex) =>
        typeof ex.getLaunchAngle === "function" &&
        typeof ex.hitDetect === "function" &&
        Array.isArray(ex.hitQueue), "hitDetection");
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
      const AS = find((ex) =>
        typeof ex.turboGroundedInterrupt === "function" &&
        typeof ex.checkForIASA === "function" &&
        typeof ex.setupActionStates === "function" &&
        Array.isArray(ex.actionStates), "actionStateShortcuts");
      if (!Array.isArray(HD.phantomQueue)) {
        throw new Error("hitdet spec: phantomQueue export missing");
      }

      // --- owner stack -------------------------------------------------
      const stack = [];
      const top = () => (stack.length ? stack[stack.length - 1] : null);

      // --- seeded-RNG stream (asshort discipline + the W distinction) ---
      const seed = window.__harnessConfig.seed >>> 0;
      const bootDraws = window.__rngCalls;
      const ffState =
        (seed + (Math.imul(bootDraws, 0x6D2B79F5) >>> 0)) % 4294967296;
      ctx.declare("rngBoot");
      ctx.declare("Math.random");
      ctx.declare("Math.randomW");
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
          // W = a draw inside a dispatch window WITH an attributing hitdet
          // frame below (chain-order-ambiguous vs the boundary's own
          // draws) — measured 0 over the goldens, pinned 0, replay traps.
          let below = false;
          for (let i = stack.length - 1; i >= 0; i--) {
            if (stack[i].attr) { below = true; break; }
          }
          ctx.push(below ? "Math.randomW" : "Math.random", "[]", ctx.canon(v));
        }
        return v;
      };

      // --- sound attribution (owner stack; + the one .stop site) --------
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
        throw new Error("hitdet spec: expected 180 Howl sounds, wrapped " +
                        sndWrapped);
      }
      {
        // executeHits' FURAFURA arm: sounds.furaloop.stop(furaLoopID) —
        // the only .stop the module reaches; event token "furaloop.stop".
        const origStop = SFX.sounds.furaloop.stop;
        SFX.sounds.furaloop.stop = function () {
          const t = top();
          if (t && t.attr) t.snd.push("furaloop.stop");
          return origStop.apply(this, arguments);
        };
      }

      // --- vfx attribution (M4 task 1: hitDetection's own drawVfx sites —
      // impactLand/powershield/breakShield/groundBounce/clank/hit-spark
      // families; full config, CALL-TIME canon. Move-dispatch-window vfx
      // stay subsumed by the seam ({attr:false} frame), mirroring the C
      // replay's resync-not-run treatment.) ------------------------------
      {
        const VFX = find((ex) => typeof ex.drawVfx === "function", "drawVfx");
        const origDraw = VFX.drawVfx;
        VFX.drawVfx = function (cfg) {
          const t = top();
          if (t && t.attr) t.vfx.push(ctx.canon(cfg));
          return origDraw.apply(this, arguments);
        };
      }

      // --- projections (physics-spec discipline) ------------------------
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
      const probe3 = (k) => probe4(k).slice(1);
      const aliasCanon = (probe) =>
        ctx.canon([0, 1, 2, 3].map((k) =>
          M.playerType[k] > -1 ? probe(k) : null));

      const preEnvelope = () =>
        '{"alias":' + aliasCanon(probe4) +
        ',"characterSelections":' + ctx.canon(M.characterSelections) +
        ',"gameMode":' + ctx.canon(M.gameMode) +
        ',"gameSettings":' + ctx.canon({
          phantomThreshold: S.gameSettings.phantomThreshold,
        }) +
        ',"hq":' + ctx.canon(HD.hitQueue) +
        ',"phq":' + ctx.canon(HD.phantomQueue) +
        ',"playerType":' + ctx.canon(M.playerType) +
        ',"players":' + playersCanon() + "}";
      const postEnvelope = (fr) =>
        '{"alias":' + aliasCanon(probe3) +
        ',"hq":' + ctx.canon(HD.hitQueue) +
        ',"phq":' + ctx.canon(HD.phantomQueue) +
        ',"players":' + playersCanon() +
        ',"rng":' + ctx.canon(fr.rng) +
        ',"snd":' + ctx.canon(fr.snd) +
        ',"vfx":[' + fr.vfx.join(",") + "]}"; // M4 task 1: full-config vfx

      // --- the three pipeline mutators (uniform envelopes) ---------------
      const wrapPipeline = (name, argProj) => {
        const orig = HD[name];
        HD[name] = function () {
          const args = Array.prototype.slice.call(arguments);
          const argsCanon = "[" +
              argProj(args).concat([preEnvelope()]).join(",") + "]";
          const fr = { attr: true, hd: true, rng: [], snd: [], vfx: [] };
          stack.push(fr);
          let ret;
          try {
            ret = orig.apply(this, arguments);
          } finally {
            stack.pop();
          }
          ctx.push(name, argsCanon, ctx.canon(ret), postEnvelope(fr));
          return ret;
        };
        ctx.declare(name);
        ctx.wrapped++;
      };
      wrapPipeline("hitDetect", (a) => [ctx.canon(a[0])]); // input dropped
      wrapPipeline("executeHits", () => []);               // input dropped
      wrapPipeline("checkPhantoms", () => []);

      // --- the queue setters (lean read/write set: the queues only) ------
      {
        const orig = HD.resetHitQueue;
        HD.resetHitQueue = function () {
          const argsCanon = "[" + ctx.canon(HD.hitQueue) + "]";
          const ret = orig.apply(this, arguments);
          ctx.push("resetHitQueue", argsCanon, ctx.canon(ret),
                   '{"hq":' + ctx.canon(HD.hitQueue) + "}");
          return ret;
        };
        ctx.declare("resetHitQueue");
        ctx.wrapped++;
      }
      {
        const orig = HD.setPhantonQueue;
        HD.setPhantonQueue = function (val) {
          const argsCanon = "[" + ctx.canon(val) + "," +
              ctx.canon(HD.phantomQueue) + "]";
          const ret = orig.apply(this, arguments);
          ctx.push("setPhantonQueue", argsCanon, ctx.canon(ret),
                   '{"phq":' + ctx.canon(HD.phantomQueue) + "}");
          return ret;
        };
        ctx.declare("setPhantonQueue");
        ctx.wrapped++;
      }

      // --- pure boundaries with live external callers ---------------------
      // getLaunchAngle reads player[v].phys.grounded ONLY under
      // knockback < 80 (JS && short-circuit): the projection mirrors that
      // exact laziness — null when the read never happens upstream.
      const PURE = {
        getLaunchAngle: (a) => [a[0], a[1], a[2], a[3], a[4], a[5],
          a[1] < 80 ? M.player[a[5]].phys.grounded : null],
        getHorizontalVelocity: (a) => a,
        getVerticalVelocity: (a) => a,
        getHorizontalDecay: (a) => a,
        getVerticalDecay: (a) => a,
        // hb -> its {bk,kg,sk} read set (article hitboxes carry 12 keys;
        // getKnockback reads exactly these three)
        getKnockback: (a) => [{ bk: a[0].bk, kg: a[0].kg, sk: a[0].sk },
                              a[1], a[2], a[3], a[4], a[5], a[6]],
        getHitstun: (a) => a,
        segmentSegmentCollision: (a) => a,
      };
      for (const name of Object.keys(PURE)) {
        const proj = PURE[name];
        const orig = HD[name];
        if (typeof orig !== "function") {
          throw new Error("hitdet spec: missing export " + name);
        }
        HD[name] = function () {
          const args = Array.prototype.slice.call(arguments);
          const argsCanon = ctx.canon(proj(args)); // PRE-call state
          const fr = { attr: true, hd: false, rng: [], snd: [], vfx: [] };
          stack.push(fr);
          let ret;
          try {
            ret = orig.apply(this, arguments);
          } finally {
            stack.pop();
          }
          if (fr.rng.length || fr.snd.length || fr.vfx.length) {
            throw new Error("hitdet spec: events escaped pure boundary " + name);
          }
          ctx.push(name, argsCanon, ctx.canon(ret));
          return ret;
        };
        ctx.declare(name);
        ctx.wrapped++;
      }
      // knockbackSounds: pure logic + sound events (article's live calls);
      // v is read only as characterSelections[v] — projected to the char id.
      {
        const orig = HD.knockbackSounds;
        HD.knockbackSounds = function (type, knockback, v) {
          const argsCanon = ctx.canon([type, knockback,
                                       M.characterSelections[v]]);
          const fr = { attr: true, hd: false, rng: [], snd: [], vfx: [] };
          stack.push(fr);
          let ret;
          try {
            ret = orig.apply(this, arguments);
          } finally {
            stack.pop();
          }
          if (fr.vfx.length) { // measured zero: no drawVfx in the body
            throw new Error("hitdet spec: vfx escaped knockbackSounds");
          }
          ctx.push("knockbackSounds", argsCanon, ctx.canon(ret),
                   '{"rng":' + ctx.canon(fr.rng) +
                   ',"snd":' + ctx.canon(fr.snd) + "}");
          return ret;
        };
        ctx.declare("knockbackSounds");
        ctx.wrapped++;
      }

      // --- internal-only exports (identity-wrapped, pinned 0 records) ----
      // Upstream callers are all module-internal local bindings (verified:
      // main/physics/article/moves import none of these; targetplay's
      // interpolatedHitCircleCollision is target mode, M4). A record here
      // means a NEW external caller appeared — pins fail loudly.
      const ZERO = ["setHasHit", "hitHitCollision",
                    "interpolatedHitHitCollision", "hitShieldCollision",
                    "interpolatedHitCircleCollision",
                    "interpolatedHitHurtCollision", "hitHurtCollision",
                    "cssHits", "executeShieldHit", "bluntHit",
                    "executeRegularHit", "hitEffectsAndSound", "hitEffect",
                    "executeGrabHits", "executeGrabTech"];
      for (const name of ZERO) {
        ctx.wrapExport(HD, name, name, null);
      }

      // --- dispatch loggers (oracle-fed seams; hitdet-owned only) ---------
      const seen = new WeakSet();
      const wrapMove = (entry) => {
        if (!entry || typeof entry !== "object" || seen.has(entry)) return;
        seen.add(entry);
        if (typeof entry.name !== "string") {
          throw new Error("hitdet spec: move object without a name");
        }
        for (const k of Object.keys(entry)) {
          if (typeof entry[k] !== "function") continue;
          const orig = entry[k];
          entry[k] = function () {
            const t = top();
            const rec = t !== null && t.attr === true && t.hd === true;
            let argsCanon = null;
            if (rec) {
              const args = Array.prototype.slice.call(arguments);
              // init/onClank receive (slot, input[, extras]) — the god
              // input array is dropped by projection (physics-spec rule);
              // onPlayerHit receives (a) only.
              argsCanon = "[" + JSON.stringify(k) + "," +
                  JSON.stringify(entry.name) + "," +
                  ctx.canon([args[0]].concat(args.slice(2))) + "]";
            }
            stack.push({ attr: false });
            let ret;
            try {
              ret = orig.apply(this, arguments);
            } finally {
              stack.pop();
            }
            if (rec) {
              const post = '{"alias":' + aliasCanon(probe4) +
                  ',"hq":' + ctx.canon(HD.hitQueue) +
                  ',"players":' + playersCanon() + "}";
              ctx.push("dispatch", argsCanon, ctx.canon(ret), post);
            }
            return ret;
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
        throw new Error("hitdet spec: expected 5 moves-index tables, found " +
                        idxTables);
      }
      for (const nm of ["JUMPAERIALB", "JUMPAERIALF"]) {
        let found = 0;
        for (const id of Object.keys(cache)) {
          const ex = cache[id] && cache[id].exports;
          if (ex && ex.default && typeof ex.default === "object" &&
              ex.default.name === nm && typeof ex.default.init === "function") {
            wrapMove(ex.default);
            found++;
          }
        }
        if (found < 1 || found > 2) {
          throw new Error("hitdet spec: expected 1-2 modules for " + nm +
                          ", found " + found);
        }
      }
      ctx.declare("dispatch");

      // --- hdFlags: the actionStates data plane hitDetection branches on --
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

      this._HD = HD;
      this._M = M;
      this._stack = stack;
      return { moduleIds: moduleIds };
    },

    // Post-run drift check: the C hdFlags table is loaded once from the
    // frame-0 record — sound only if no move-object flag mutated in-match.
    finalCheck() {
      if (this._flagsCanon() !== this._flagsAtInstall) {
        throw new Error("hitdet spec: actionStates flags drifted during " +
                        "the match — the static hdFlags table is unsound");
      }
      return 1;
    },

    // Deterministic synthetic-domain sweep (rule 11) for the PURE
    // boundaries: live traffic reaches only the arms real hits produce.
    // Pure functions of their arguments except the two rule-12 guarded
    // injections (slot-3 player for getLaunchAngle's grounded read,
    // characterSelections[3] for knockbackSounds' char cases) — both
    // restore the pre-sweep value exactly and draw NO seeded RNG; the x2
    // byte-stability + STREAM MATCH guards prove non-perturbation.
    sweep() {
      const HD = this._HD;
      const M = this._M;
      let n = 0;

      // getKnockback: both formula arms (sk==0 weight-scaled / else set-kb),
      // the 2500 cap, crouching, vCancel, percent floors, zero weight.
      const HB = (bk, kg, sk) => ({ bk: bk, kg: kg, sk: sk });
      const kbs = [
        [HB(0, 100, 0), 10, 10, 0, 100, false, false],
        [HB(0, 100, 0), 10, 10, 55.5, 100, false, false],
        [HB(0, 100, 0), 10, 10, 55.5, 100, true, false],
        [HB(0, 100, 0), 10, 10, 55.5, 100, false, true],
        [HB(0, 100, 0), 10, 10, 55.5, 100, true, true],
        [HB(40, 116, 0), 13, 13, 88.25, 60, false, false],
        [HB(30, 90, 0), 7.5, 7, 12.5, 130, false, false],
        [HB(0, 100, 0), 999, 999, 999, 1, false, false],   // 2500 cap
        [HB(0, 100, 0), 0, 0, 0, 100, false, false],       // zero damage
        [HB(0, 0, 0), 10, 10, 10, 100, false, false],      // kg 0
        [HB(0, 100, 150), 10, 10, 0, 100, false, false],   // sk arm
        [HB(20, 70, 150), 10, 10, 300, 80, true, true],    // sk arm mods
        [HB(0, 40, 60), 3, 3, 0, 120, false, false],
        [HB(0, 100, 150), 999, 999, 0, 1, false, false],   // sk arm cap
      ];
      for (const a of kbs) {
        HD.getKnockback(a[0], a[1], a[2], a[3], a[4], a[5], a[6]);
        n++;
      }

      for (const kb of [0, 10.5, 32.09, 32.1, 79.99, 80, 125, 2500]) {
        HD.getHitstun(kb); n++;
      }

      // launch velocity/decay: quadrant angles, the -0 product at kb 0
      // (0 * cos(180deg) = -0 -> js_round -0 preservation), the
      // grounded/trajectory zeroing arms of getVerticalVelocity.
      const angles = [0, 44, 90, 136, 180, 270, 361, 17.5, -23, 89.999];
      for (const ang of angles) {
        HD.getHorizontalVelocity(120, ang); n++;
        HD.getVerticalVelocity(120, ang, false, ang); n++;
        HD.getHorizontalDecay(ang); n++;
        HD.getVerticalDecay(ang); n++;
      }
      HD.getHorizontalVelocity(0, 180); n++;      // -0 tooth
      HD.getVerticalVelocity(0, 270, false, 90); n++;
      HD.getVerticalVelocity(79, 45, true, 0); n++;   // zeroing arm
      HD.getVerticalVelocity(79, 45, true, 180); n++;
      HD.getVerticalVelocity(79, 45, true, 90); n++;  // traj not 0/180
      HD.getVerticalVelocity(80, 45, true, 0); n++;   // kb not < 80

      // getLaunchAngle, kb >= 80 arms (player[v] never dereferenced —
      // upstream short-circuits on knockback < 80): trajectory-361 both
      // reverse arms, reverse wraparound, DI deadzones/quadrants, the
      // angleOffset cap and sign, the < 0.01 clamp.
      const LA = [
        [361, 80, false, 0, 0, 0], [361, 80, true, 0, 0, 0],
        [361, 100, false, 0.5, 0.5, 0], [0, 100, false, 0, 0, 0],
        [0, 100, true, 0, 0, 0],                    // 180 - 0
        [10, 100, true, 0, 0, 0],                   // reverse, no wrap
        [200, 100, true, 0, 0, 0],                  // 180-200 -> +360 wrap
        [90, 100, false, 0.2874, -0.2874, 0],       // both DI deadzoned
        [90, 100, false, 0.2875, 0, 0],             // x at threshold (kept)
        [90, 100, false, 0, 0.9, 0],                // x==0, y>0 -> diAngle 90
        [90, 100, false, 0, -0.9, 0],               // x==0, y<0 -> 270
        [90, 100, false, -0.8, 0.4, 0],             // x<0 -> +180
        [90, 100, false, 0.8, -0.4, 0],             // y<0 -> +360
        [44, 100, false, 1, 1, 0],                  // rAngle ~0
        [270, 100, false, -1, -1, 0],               // big offsets, cap 18
        [350, 100, false, -1, 1, 0],                // rAngle > 180 wrap
        [1e-3, 100, false, 0.3, 0.3, 0],            // newtraj < 0.01 clamp
        [136, 90, true, 0.7, -0.7, 0],
      ];
      for (const a of LA) {
        HD.getLaunchAngle(a[0], a[1], a[2], a[3], a[4], a[5]); n++;
      }
      // kb < 80 arms read player[v].phys.grounded: rule-12 injection into
      // INACTIVE slot 3, restored exactly.
      {
        const saved = M.player[3];
        try {
          for (const c of [
            [0, 40, false, 0, 0, true],    // deadzone: grounded + traj 0
            [180, 40, true, 0.5, 0, true], // deadzone + reverse
            [0, 40, false, 0.5, 0.5, false], // airborne -> no deadzone
            [90, 40, false, 0.5, 0.5, true], // traj not 0/180
            [361, 31, false, 0.4, 0, true],  // 361 low-kb + deadzone
            [361, 32.2, true, 0, 0.4, false], // 361 high-kb arm at kb<80
          ]) {
            M.player[3] = { phys: { grounded: c[5] } };
            HD.getLaunchAngle(c[0], c[1], c[2], c[3], c[4], 3);
            n++;
          }
        } finally {
          M.player[3] = saved;
        }
      }

      // segmentSegmentCollision: parallel, t<0, t>1, u<0, u>1, hit.
      const V = (x, y) => ({ x: x, y: y });
      const segs = [
        [V(0, 0), V(2, 0), V(0, 1), V(2, 1)],       // parallel
        [V(0, 0), V(2, 0), V(3, -1), V(3, 1)],      // t > 1
        [V(0, 0), V(2, 0), V(-1, -1), V(-1, 1)],    // t < 0
        [V(0, 0), V(2, 0), V(1, 1), V(1, 3)],       // u < 0
        [V(0, 0), V(2, 0), V(1, -3), V(1, -1)],     // u > 1
        [V(0, 0), V(2, 2), V(0, 2), V(2, 0)],       // hit
        [V(0, 0), V(0, 0), V(1, 1), V(2, 2)],       // degenerate a
      ];
      for (const s of segs) {
        HD.segmentSegmentCollision(s[0], s[1], s[2], s[3]); n++;
      }

      // knockbackSounds: the full type x tier lattice + per-char hurt
      // voices (characterSelections[3] injected per case, restored).
      {
        const savedCS = M.characterSelections[3];
        try {
          for (const type of [0, 1, 3, 4, 7]) {
            for (const kb of [30, 75, 120, 200, 300]) {
              for (const c of (kb >= 140 ? [0, 1, 2, 3, 4] : [0])) {
                M.characterSelections[3] = c;
                HD.knockbackSounds(type, kb, 3);
                n++;
              }
            }
          }
        } finally {
          M.characterSelections[3] = savedCS;
        }
      }
      return n;
    },
  };
})();
