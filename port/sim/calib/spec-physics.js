// spec-physics.js — M2 task 5 capture spec: physics.js core +
// interpolatedCollision. Registers window.__capSpecs.physics. Injected
// AFTER capturelib.js. See FORMAT.md "The physics spec".
//
// Wrapped boundary (expectWrapped = 8):
// - physics(i, inputBuffers)  [mutation-captured, 5-field records]
// - sweepCircleVsSweepCircle, sweepCircleVsAABB (interpolatedCollision —
//   pure; live callers are hitDetection + article, both namespace-deref)
// - the 5 hitDetection launch getters (getLaunchAngle,
//   getHorizontal/VerticalVelocity, getHorizontal/VerticalDecay):
//   recorded ONLY while inside a physics call (owner stack) — they are
//   task 6's surface; here they are ORACLE-FED SEAMS (args verified
//   bit-exactly by the replay, returns injected).
//
// Non-boundary instrumentation (asshort pattern):
// - every move-object function (actionStates deep copies + the 5 per-char
//   moves-index tables + the JUMPAERIALB/F module objects) gets a
//   dispatch logger: a TOP-LEVEL dispatch from inside physics produces a
//   5-field "dispatch" record (args [phase, moveName, [slot, ...extras]];
//   post {alias, players}) — the replay verifies the site and RESYNCS the
//   C sim from the recorded post-dispatch state (moves are tasks 7-12).
// - Howl sounds: physics' own direct plays land in the record's snd list.
// - hitQueue rows: mark/collect attribution (hitQueue is REASSIGNED by
//   resetHitQueue each tick, so the array's push cannot be wrapped once;
//   physics-owned rows = rows appended outside dispatch windows).
//
// ALIAS PROBES (M2 rule 10): pre-args and dispatch posts carry, per active
// player, [pos===ECB1[0], hitboxes.active===prevFrameHitboxes.active,
// hitList alias, id alias]; the physics post carries the 3 HITBOX flags
// (C tracks those through merge/land/turnOffHitboxes — compared teeth);
// pos-ECB1 is capture-restored only (a move init inside land() may
// reassign pos invisibly; the next probe restores truth before any
// consumer runs — physics.c note at line 847).
//
// asFlags (frame-0 record): the actionStates per-(char,state) DATA plane
// physics branches on (canEdgeCancel/landType/airborneState/...): dumped
// once for all 5 chars; finalCheck() re-dumps after the last frame and
// hard-fails on ANY drift (proves the flags are static over the match —
// the C table's soundness condition).
(() => {
  const EXCLUDE = ["charAttributes", "charHitboxes", "percentShake"];
  const GETTERS = ["getLaunchAngle", "getHorizontalVelocity",
                   "getVerticalVelocity", "getHorizontalDecay",
                   "getVerticalDecay"];
  const FLAG_KEYS = ["airborneState", "canEdgeCancel", "canGrabLedge",
                     "canPassThrough", "dead", "disableTeeter", "headBonk",
                     "ignoreCollision", "inGrab", "landType", "missfoot",
                     "name", "specialWallCollide", "wallJumpAble"];

  window.__capSpecs.physics = {
    expectWrapped: 8,

    install(ctx) {
      const cache = ctx.cache;
      const moduleIds = {};
      const find = (pred, what) => {
        const m = ctx.findModule(cache, pred, what);
        moduleIds[what] = m.id;
        return m.exports;
      };

      const PHYS = find((ex) =>
        typeof ex.physics === "function" && typeof ex.land === "function",
        "physics");
      const IC = find((ex) =>
        typeof ex.sweepCircleVsSweepCircle === "function" &&
        typeof ex.sweepCircleVsAABB === "function", "interpolatedCollision");
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
      const AST = find((ex) =>
        typeof ex.getActiveStage === "function" &&
        typeof ex.setVsStage === "function", "activeStage");

      // --- owner stack -------------------------------------------------
      const stack = [];
      const top = () => (stack.length ? stack[stack.length - 1] : null);

      // --- projections -------------------------------------------------
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

      const stageProj = () => {
        const st = AST.getActiveStage();
        return {
          blastzone: st.blastzone,
          ceiling: st.ceiling,
          connected: st.connected === undefined ? null : st.connected,
          ground: st.ground,
          ledge: st.ledge,
          platform: st.platform,
          wallL: st.wallL,
          wallR: st.wallR,
        };
      };

      const preEnvelope = () =>
        '{"alias":' + aliasCanon(probe4) +
        ',"characterSelections":' + ctx.canon(M.characterSelections) +
        ',"gameMode":' + ctx.canon(M.gameMode) +
        ',"gameSettings":' + ctx.canon({
          lCancelType: S.gameSettings.lCancelType,
          phantomThreshold: S.gameSettings.phantomThreshold,
          turbo: S.gameSettings.turbo,
        }) +
        ',"playerType":' + ctx.canon(M.playerType) +
        ',"players":' + playersCanon() +
        ',"stage":' + ctx.canon(stageProj()) +
        ',"versusMode":' + ctx.canon(M.versusMode) + "}";

      // --- hitQueue attribution (mark/collect; header note) -------------
      const hqCollect = (fr) => {
        const q = HD.hitQueue;
        while (fr.hqMark < q.length) {
          fr.hq.push(ctx.canon(q[fr.hqMark]));
          fr.hqMark++;
        }
      };
      const hqSkipTo = (fr) => { fr.hqMark = HD.hitQueue.length; };

      // --- the physics boundary (mutation-captured) ---------------------
      const origPhysics = PHYS.physics;
      PHYS.physics = function (i, inputBuffers) {
        const argsCanon = "[" + ctx.canon(i) + "," +
            ctx.canon(inputBuffers[i].slice(0, 4)) + "," + preEnvelope() + "]";
        const fr = { attr: true, phys: true, snd: [], hq: [],
                     hqMark: HD.hitQueue.length };
        stack.push(fr);
        let ret;
        try {
          ret = origPhysics.apply(this, arguments);
        } finally {
          stack.pop();
        }
        hqCollect(fr);
        const post = '{"alias":' + aliasCanon(probe3) +
            ',"hq":[' + fr.hq.join(",") + "]" +
            ',"players":' + playersCanon() +
            ',"snd":[' + fr.snd.map((n) => JSON.stringify(n)).join(",") +
            "]}";
        ctx.push("physics", argsCanon, ctx.canon(ret), post);
        return ret;
      };
      ctx.declare("physics");
      ctx.wrapped++;

      // --- interpolatedCollision (pure, all callers recorded) ------------
      ctx.wrapExport(IC, "sweepCircleVsSweepCircle", "sweepCircleVsSweepCircle",
                     null);
      ctx.wrapExport(IC, "sweepCircleVsAABB", "sweepCircleVsAABB", null);

      // --- hitDetection getters (oracle-fed seams; physics-owned only) ---
      for (const name of GETTERS) {
        const orig = HD[name];
        if (typeof orig !== "function") {
          throw new Error("physics spec: missing hitDetection export " + name);
        }
        HD[name] = function () {
          const t = top();
          const rec = t !== null && t.attr === true && t.phys === true;
          const argsCanon = rec
              ? ctx.canon(Array.prototype.slice.call(arguments)) : null;
          const ret = orig.apply(this, arguments);
          if (rec) ctx.push(name, argsCanon, ctx.canon(ret));
          return ret;
        };
        ctx.declare(name);
        ctx.wrapped++;
      }

      // --- dispatch loggers (asshort discovery, task-5 recording) --------
      const seen = new WeakSet();
      const wrapMove = (entry) => {
        if (!entry || typeof entry !== "object" || seen.has(entry)) return;
        seen.add(entry);
        if (typeof entry.name !== "string") {
          throw new Error("physics spec: move object without a name");
        }
        for (const k of Object.keys(entry)) {
          if (typeof entry[k] !== "function") continue;
          const orig = entry[k];
          entry[k] = function () {
            const t = top();
            const rec = t !== null && t.attr === true && t.phys === true;
            let argsCanon = null;
            if (rec) {
              hqCollect(t); // rows so far belong to physics itself
              const args = Array.prototype.slice.call(arguments);
              // args[1] is the god input array (the physics record already
              // carries the slot's buffer slice) — dropped by projection.
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
              hqSkipTo(t); // rows the move pushed are NOT physics' own
              const post = '{"alias":' + aliasCanon(probe4) +
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
        throw new Error("physics spec: expected 5 moves-index tables, found " +
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
          throw new Error("physics spec: expected 1-2 modules for " + nm +
                          ", found " + found);
        }
      }
      ctx.declare("dispatch");

      // --- sound attribution (physics' own 4 direct sites) ---------------
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
        throw new Error("physics spec: expected 180 Howl sounds, wrapped " +
                        sndWrapped);
      }

      // --- asFlags: the actionStates data plane physics branches on ------
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
      ctx.declare("asFlags");
      ctx.push("asFlags", ctx.canon(["all"]), this._flagsAtInstall);

      this._IC = IC;
      return { moduleIds: moduleIds };
    },

    // Post-run drift check (run-capture.js finalCheck hook): the C flags
    // table is loaded once from the frame-0 asFlags record — sound only if
    // no move-object flag mutated during the match. Hard proof per run.
    finalCheck() {
      if (this._flagsCanon() !== this._flagsAtInstall) {
        throw new Error("physics spec: actionStates flags drifted during " +
                        "the match — the static asFlags table is unsound");
      }
      return 1;
    },

    // Deterministic synthetic-domain sweep (rule 11) for the PURE
    // interpolatedCollision exports: live traffic exercises only the
    // branches real hitboxes reach; these fixed literals cover every
    // region/case arm of sweepCircleVsAABB (8 regions x corner/line/null,
    // the overlap fast-path, the separated fast-exit) and
    // sweepCircleVsSweepCircle's t1/t2 selection lattice. Pure functions
    // of their arguments — no sim state, no RNG.
    sweep() {
      const IC = this._IC;
      const V = (x, y) => ({ x: x, y: y });
      let n = 0;

      // sweepCircleVsSweepCircle: initial overlap; approach+hit; miss;
      // t out of [0,1]; degenerate zero-motion (a1==a2==0 arms of the
      // quadratic); tie/min arm; NaN-producing zero radii sums.
      const ccc = [
        [V(0, 0), 1, V(0, 0), 1, V(1, 0), 1, V(1, 0), 1],       // overlap
        [V(3, 2), 2, V(3.5, 2), 2, V(4, 2), 1, V(4.5, 2), 1],   // overlap, nonzero coords (midpoint-formula teeth)
        [V(-4, 0), 1, V(4, 0), 1, V(4, 0), 1, V(-4, 0), 1],     // crossing
        [V(-4, 0), 0.5, V(-2, 0), 0.5, V(4, 0), 0.5, V(2, 0), 0.5], // miss
        [V(-9, 0), 1, V(-8, 0), 1, V(9, 0), 1, V(8, 0), 1],     // too short
        [V(-3, 0), 1, V(-3, 0), 1, V(3, 0), 1, V(3, 0), 1],     // static apart
        [V(0, 0), 0, V(0, 0), 0, V(0, 0), 0, V(0, 0), 0],       // all zero
        [V(-2, 1), 1, V(1, 1), 2, V(2, -1), 1, V(-1, -1), 2],   // radii grow
        [V(-2, 0), 1, V(2, 0), 3, V(2, 0), 1, V(-2, 0), 3],     // both branches
        [V(-1e-9, 0), 1e-12, V(1e-9, 0), 1e-12, V(5, 0), 1, V(5, 0), 1],
        [V(-6, -6), 2, V(6, 6), 2, V(6, -6), 2, V(-6, 6), 2],   // diagonal
      ];
      for (const a of ccc) {
        IC.sweepCircleVsSweepCircle(a[0], a[1], a[2], a[3], a[4], a[5],
                                    a[6], a[7]);
        n++;
      }

      // sweepCircleVsAABB: box [-2,-1]..[2,1]; all 8 regions, each with a
      // hitting sweep, a missing sweep, and (where reachable) corner vs
      // edge outcomes; plus the overlap fast-path and the separated exit.
      const bl = V(-2, -1);
      const tr = V(2, 1);
      const aabb = [
        [V(0, 0), 1, V(0, 0), 1],          // overlap fast-path (inside)
        [V(-9, 0), 1, V(-8, 0), 1],        // separated fast-exit
        [V(-5, 0), 1, V(0, 0), 1],         // left side, edge hit
        [V(-5, 0), 0.5, V(-4, 0), 0.5],    // left side, miss
        [V(5, 0), 1, V(0, 0), 1],          // right side, edge hit
        [V(5, 0), 0.5, V(4, 0), 0.5],      // right side, miss
        [V(0, 5), 1, V(0, 0), 1],          // top side, edge hit
        [V(0, 5), 0.5, V(0, 4), 0.5],      // top side, miss
        [V(0, -5), 1, V(0, 0), 1],         // bottom side, edge hit
        [V(0, -5), 0.5, V(0, -4), 0.5],    // bottom side, miss
        [V(-5, -4), 1, V(-1, 0), 1],       // bottom-left, corner path
        [V(-5, -4), 0.25, V(-4.5, -3.5), 0.25], // bottom-left, miss
        [V(-5, 4), 1, V(-1, 0), 1],        // top-left, corner path
        [V(-5, 4), 0.25, V(-4.5, 3.5), 0.25],
        [V(5, -4), 1, V(1, 0), 1],         // bottom-right, corner path
        [V(5, -4), 0.25, V(4.5, -3.5), 0.25],
        [V(5, 4), 1, V(1, 0), 1],          // top-right, corner path
        [V(5, 4), 0.25, V(4.5, 3.5), 0.25],
        [V(-5, 0), 3.1, V(-4, 0), 3.1],    // left, immediate corner overlap
        [V(-3.5, -0.5), 1, V(-3.5, -0.5), 1], // static near left, radius hit
        [V(-4, -2.5), 1.2, V(0, -2.5), 1.2],  // bottom, sliding parallel
        [V(3, 2), 0.5, V(-3, -2), 0.5],    // diagonal through the box
        [V(2.5, 1.5), 0.2, V(2.2, 1.2), 0.4], // growing into top-right corner
        [V(0, 0), 0, V(0, 0), 0],          // degenerate zero radius inside
      ];
      for (const a of aabb) {
        IC.sweepCircleVsAABB(a[0], a[1], a[2], a[3], bl, tr);
        n++;
      }
      return n;
    },
  };
})();
