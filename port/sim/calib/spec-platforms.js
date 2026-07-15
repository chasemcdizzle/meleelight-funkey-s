// spec-platforms.js — M2 task 14 capture spec: movingPlatforms stage-tick
// logic (src/stages/vs-stages/*.js — the M1-externals-stubbed god-module
// bodies; STAB1 owns the stage DATA, this cluster owns the per-tick
// LOGIC). Registers window.__capSpecs.platforms. Injected AFTER
// capturelib.js. See FORMAT.md "The platforms spec".
//
// BOUNDARY (6 fns): every VS stage object's movingPlatforms — main.js:1058
// calls getActiveStage().movingPlatforms() first thing in the mode-3 tick.
// Upstream bodies (measured): battlefield/dreamland/pstadium/fdest are
// EMPTY; ystory is Randall's rectangular rail machine (platform[0] + a
// rider arm over all four player slots); fountain is the two side
// platforms — module-PRIVATE platformStates (exposed read-only via the
// run-capture.js served-bytes getter window.__mpFountainPS), seeded
// Math.random draws, the main.starting reset arm, and player transfer
// arms. Only the active stage's boundary fires: one record per tick.
//
// ENVELOPES (per-stage measured read/write sets; rule 18):
// - plat = the FULL activeStage.platform plane (not just movingPlats) in
//   EVERY record — the rule-18 chain carrier: the C replay chains the
//   plane and compares it against each in-match record's pre, turning the
//   "nothing else writes the platform plane" enclosure assumption into a
//   per-record measurement (static platforms included).
// - static stages: pre/post {plat} (body reads/writes nothing; lean).
// - ystory: pre/post {plat, players} — players is the read/write slice
//   ALL FOUR slots (the loop is unguarded by playerType; inactive slots
//   hold page-start CSS-era playerObjects): {grounded, onSurface, pos}.
//   ystory draws NOTHING (asserted).
// - fountain: pre {plat, players, ps, starting}, post {plat, players, ps,
//   rng} — ps is platformStates via the injected getter; starting is
//   main's exported let (the reset arm); rng is the record's owner draws.
// RNG: rngBoot + owner draws in fountain posts; everything else
// standalone "Math.random" (no dispatch windows exist — rule 14 vacuous).
(() => {
  const STAGES = [
    { name: "battlefield", movingPlats: [] },
    { name: "ystory", movingPlats: [0] },
    { name: "pstadium", movingPlats: [] },
    { name: "dreamland", movingPlats: [] },
    { name: "fdest", movingPlats: [] },
    { name: "fountain", movingPlats: [1, 2] },
  ];

  window.__capSpecs.platforms = {
    expectWrapped: 6,

    install(ctx) {
      const cache = ctx.cache;
      const moduleIds = {};
      const find = (pred, what) => {
        const m = ctx.findModule(cache, pred, what);
        moduleIds[what] = m.id;
        return m.exports;
      };

      const M = find((ex) =>
        Array.isArray(ex.player) && Array.isArray(ex.playerType) &&
        typeof ex.gameTick === "function" &&
        typeof ex.setStarting === "function", "main");
      const AST = find((ex) =>
        typeof ex.getActiveStage === "function" &&
        typeof ex.setVsStage === "function" &&
        typeof ex.setActiveStageBuilderTestStage === "function",
        "activeStage");
      const PJ = find((ex) =>
        typeof ex.playerObject === "function" &&
        typeof ex.physicsObject === "function", "player");

      const stageMods = {};
      for (const s of STAGES) {
        const mod = find((ex) =>
          ex.default && typeof ex.default === "object" &&
          ex.default.name === s.name &&
          typeof ex.default.movingPlatforms === "function" &&
          Array.isArray(ex.default.movingPlats), "stage:" + s.name);
        // movingPlats presence is M1-owned STAB1 data — assert consistency
        const mp = mod.default.movingPlats;
        if (mp.length !== s.movingPlats.length ||
            !s.movingPlats.every((v, i) => mp[i] === v)) {
          throw new Error("platforms spec: " + s.name +
                          " movingPlats drifted from the STAB1 pin");
        }
        stageMods[s.name] = mod.default;
      }
      // the run-capture.js served-bytes getter for fountain's private state
      if (typeof window.__mpFountainPS !== "function") {
        throw new Error("platforms spec: window.__mpFountainPS missing " +
                        "(run-capture.js platformStates injection failed)");
      }
      const PS = window.__mpFountainPS;
      {
        const ps = PS();
        if (!Array.isArray(ps) || ps.length !== 2 ||
            !ps.every((e) => e && typeof e.state === "string" &&
                             typeof e.timer === "number" &&
                             typeof e.destination === "number")) {
          throw new Error("platforms spec: platformStates shape drifted");
        }
      }

      // --- seeded-RNG stream (the asshort/article discipline) --------------
      const seed = window.__harnessConfig.seed >>> 0;
      const bootDraws = window.__rngCalls;
      const ffState =
        (seed + (Math.imul(bootDraws, 0x6D2B79F5) >>> 0)) % 4294967296;
      ctx.declare("rngBoot");
      ctx.declare("Math.random");
      ctx.push("rngBoot",
        "[" + ctx.canon(seed) + "," + ctx.canon(bootDraws) + "]",
        ctx.canon(ffState));
      // owner attribution: movingPlatforms never nests, so a single
      // current-record cell replaces the moves stack.
      const rngCtx = { cur: null };
      const mr = Math.random;
      Math.random = function () {
        const v = mr();
        if (rngCtx.cur) rngCtx.cur.rng.push(v);
        else ctx.push("Math.random", "[]", ctx.canon(v));
        return v;
      };

      // --- envelope builders -------------------------------------------------
      const platCanon = (st) => ctx.canon(st.platform);
      const playersCanon = () =>
        ctx.canon([0, 1, 2, 3].map((j) => ({
          grounded: M.player[j].phys.grounded,
          onSurface: M.player[j].phys.onSurface,
          pos: M.player[j].phys.pos,
        })));
      const preCanon = (name, st) => {
        if (name === "ystory") {
          return '{"plat":' + platCanon(st) +
                 ',"players":' + playersCanon() + "}";
        }
        if (name === "fountain") {
          return '{"plat":' + platCanon(st) +
                 ',"players":' + playersCanon() +
                 ',"ps":' + ctx.canon(PS()) +
                 ',"starting":' + ctx.canon(M.starting) + "}";
        }
        return '{"plat":' + platCanon(st) + "}"; // static stages: lean
      };
      const postCanon = (name, st, fr) => {
        if (name === "ystory") {
          if (fr.rng.length) {
            throw new Error("platforms spec: ystory drew the seeded stream");
          }
          return '{"plat":' + platCanon(st) +
                 ',"players":' + playersCanon() + "}";
        }
        if (name === "fountain") {
          return '{"plat":' + platCanon(st) +
                 ',"players":' + playersCanon() +
                 ',"ps":' + ctx.canon(PS()) +
                 ',"rng":' + ctx.canon(fr.rng) + "}";
        }
        if (fr.rng.length) {
          throw new Error("platforms spec: " + name + " drew the seeded stream");
        }
        return '{"plat":' + platCanon(st) + "}";
      };

      // --- the boundary wrappers ----------------------------------------------
      for (const s of STAGES) {
        const st = stageMods[s.name];
        const orig = st.movingPlatforms;
        const name = s.name;
        st.movingPlatforms = function () {
          // envelope coherence: the body mutates the activeStage import,
          // the envelope reads the owning stage object — they must agree
          // (upstream only ever calls getActiveStage().movingPlatforms()).
          if (AST.getActiveStage() !== st) {
            throw new Error("platforms spec: " + name +
                            ".movingPlatforms called while not active");
          }
          const pre = preCanon(name, st);
          const fr = { rng: [] };
          rngCtx.cur = fr;
          let ret;
          try {
            ret = orig.apply(this, arguments);
          } finally {
            rngCtx.cur = null;
          }
          ctx.push("movingPlatforms",
            "[" + JSON.stringify(name) + "," + pre + "]",
            ctx.canon(ret), postCanon(name, st, fr));
          return ret;
        };
        ctx.wrapped++;
      }
      ctx.declare("movingPlatforms");

      this._M = M;
      this._AST = AST;
      this._PJ = PJ;
      this._stageMods = stageMods;
      this._PS = PS;
      this._rngCtx = rngCtx;
      return { moduleIds: moduleIds };
    },

    // Synthetic-domain sweep (rules 11/12): the zero-live platform arms.
    // All four player slots get REAL playerObject injections (restored);
    // Math.random is swapped for the sweep mulberry32 (0x0badf00d) — the C
    // replay mirrors it for ALL frame-0 records; ystory's Randall is poked
    // per arm and restored to its authored coordinates; fountain's private
    // platformStates is driven through the injected getter (its entry
    // OBJECTS are live — pokes work) and net-restored by a final
    // starting-arm reset (which rewrites platformStates AND the platform
    // ys to their authored values EXACTLY — the reset arm IS the restore).
    // The ×2 byte-stability + STREAM MATCH guards prove non-perturbation.
    sweep() {
      const M = this._M;
      const AST = this._AST;
      const PJ = this._PJ;
      const SM = this._stageMods;
      const PS = this._PS;
      const rngCtx = this._rngCtx;
      let n = 0;

      const savedRandom = Math.random;
      let a = 0x0badf00d | 0;
      Math.random = function () {
        a = (a + 0x6D2B79F5) | 0;
        let t = Math.imul(a ^ (a >>> 15), 1 | a);
        t = (t + Math.imul(t ^ (t >>> 7), 61 | t)) ^ t;
        const v = ((t ^ (t >>> 14)) >>> 0) / 4294967296;
        if (rngCtx.cur) rngCtx.cur.rng.push(v);
        return v; // unattributed sweep draws are discarded (article model)
      };
      const savedPlayers = [M.player[0], M.player[1], M.player[2], M.player[3]];
      const savedStage = AST.getActiveStage();
      if (M.starting !== true) {
        throw new Error("platforms sweep: starting expected true pre-setupMatch");
      }
      try {
        for (let j = 0; j < 4; j++) {
          M.player[j] = new PJ.playerObject(2, [0, 10], 1);
        }
        const P = (j) => M.player[j].phys;
        const ride = (j, s0, s1, grounded) => {
          P(j).onSurface = [s0, s1];
          P(j).grounded = grounded;
        };
        const posAt = (j, x, y) => { P(j).pos.x = x; P(j).pos.y = y; };

        // ---- ystory: Randall's rail machine --------------------------------
        AST.setVsStage(1);
        const YS = SM.ystory;
        const yp = YS.platform[0];
        const setY = (x0, y0) => {
          yp[0].x = x0; yp[0].y = y0;
          yp[1].x = x0 + 11.9; yp[1].y = y0;
        };
        const callY = () => { YS.movingPlatforms(); n++; };
        // rider on slot 0 ([1,0] grounded); negatives on 1..3
        ride(0, 1, 0, true); posAt(0, -97, -20);
        ride(1, 1, 1, true);  // wrong platform index
        ride(2, 1, 0, false); // not grounded
        ride(3, 0, 0, true);  // on ground, not platform
        setY(-103.6, -20); callY();  // arm 1: left rail, moving down
        setY(91.35, -20); callY();   // arm 2: right rail, moving up
        setY(0, -33.25); callY();    // arm 3: bottom rail, moving right
        setY(0, -13.65); callY();    // arm 4: top rail, moving left
        setY(-103.6, -33.2); callY();// corner: arm 1 then arm 3 same call
        setY(91.35, -13.9); callY(); // corner: arm 2 then arm 4 same call
        setY(91.2, -33.25); callY(); // bottom rail approaching the corner
        setY(0, -20); callY();       // interior: no arm, rider y-snap only
        // restore Randall to the authored STAB1 coordinates
        yp[0].x = -103.6; yp[0].y = -33.25;
        yp[1].x = -91.7; yp[1].y = -33.25;

        // ---- fountain: platformStates machine + transfers -------------------
        AST.setVsStage(5);
        const FT = SM.fountain;
        const fp = FT.platform;
        const callF = () => { FT.movingPlatforms(); n++; };
        const setFY = (i, y) => { fp[i][0].y = y; fp[i][1].y = y; };
        for (let j = 0; j < 4; j++) ride(j, 0, 0, false);
        callF(); // starting reset arm (starting is true pre-setupMatch)
        M.setStarting(false);
        let ps = PS();
        callF(); // arrival at authored dests: else-newTimer arm (2 draws)
        callF(); // static decrement arm ×2
        // timer<1 destination selection: loop until BOTH the t<0.3 sink arm
        // and the t>=0.3 scaled arm have fired for ps[0] (deterministic —
        // the sweep RNG is fixed; ps[1] parked static with a huge timer)
        let seenSink = false, seenScaled = false, guard = 0;
        ps[1].state = "static"; ps[1].timer = 1e9;
        while ((!seenSink || !seenScaled) && guard < 64) {
          guard++;
          ps[0].state = "static"; ps[0].timer = 0.5;
          setFY(1, 10); // |y| >= 0.075: never the base-return arm here
          callF();      // updatePlatform(1,0) timer<1 -> select (1 draw)
          if (ps[0].destination < 0) seenSink = true;
          else seenScaled = true;
        }
        if (!seenSink || !seenScaled) {
          throw new Error("platforms sweep: selection arms not both reached");
        }
        // moving down arm (dest scaled, y above) + moving up arm (y below)
        ps[0].state = "moving"; ps[0].destination = 19.875;
        setFY(1, 30); callF(); // y -= 0.075
        setFY(1, 5); callF();  // y += 0.075
        // arrival with dest < 0.075: the 480+360r newTimer arm
        ps[0].state = "moving"; ps[0].destination = -0.00001;
        setFY(1, -0.00001); callF();
        // base-return selection: |y| < 0.075 -> dest 19.875 (draw consumed)
        ps[0].state = "static"; ps[0].timer = 0.5;
        callF();
        // arrival at 19.875: the newTimer = 0 arm
        ps[0].state = "moving"; ps[0].destination = 19.875;
        setFY(1, 19.875); callF();
        // transfers ----------------------------------------------------------
        // platform -> middle ground, right platform (rider [1,1])
        ps[0].state = "static"; ps[0].timer = 1e9;
        ride(0, 1, 1, true); posAt(0, 30, -0.5);
        setFY(1, -0.5); callF();
        // platform -> middle ground, left platform (rider [1,2])
        ride(1, 1, 2, true); posAt(1, -30, -0.5);
        setFY(2, -0.5); callF();
        setFY(2, 16.125); ps = PS();
        // middle ground -> right platform ([0,2], x in [21,49.5], rising)
        ride(0, 0, 2, true); posAt(0, 30, 0.00001);
        ride(1, 0, 0, false);
        ps[0].state = "moving"; ps[0].destination = 19.875;
        setFY(1, -0.1); callF(); // post-update y = -0.025 < 0.075: transfer
        // middle ground -> left platform (x in [-49.5,-21])
        ride(0, 0, 2, true); posAt(0, -30, 0.00001);
        ps[0].state = "static"; ps[0].timer = 1e9; setFY(1, 19.875);
        ps[1].state = "moving"; ps[1].destination = 19.875;
        setFY(2, -0.1); callF();
        // transfer negatives: x outside the band; platform static; rider
        // not grounded — all three no-op in one choreographed call each
        ride(0, 0, 2, true); posAt(0, 60, 0.00001);
        ps[1].state = "moving"; ps[1].destination = 19.875; setFY(2, -0.1);
        callF(); // x=60 outside both bands
        ride(0, 0, 2, true); posAt(0, 30, 0.00001);
        ps[1].state = "static"; ps[1].timer = 1e9; setFY(2, 16.125);
        ps[0].state = "static"; ps[0].timer = 1e9; setFY(1, -0.1);
        callF(); // right platform low but STATIC: no transfer
        ride(0, 0, 2, false);
        ps[0].state = "moving"; ps[0].destination = 19.875;
        callF(); // not grounded: guard skips the slot
        // net restore: the starting arm rewrites platformStates AND the
        // platform ys to their authored values exactly
        M.setStarting(true);
        setFY(1, 22.125); // pre-reset pokes irrelevant; reset overwrites
        callF();
      } finally {
        Math.random = savedRandom;
        M.player[0] = savedPlayers[0];
        M.player[1] = savedPlayers[1];
        M.player[2] = savedPlayers[2];
        M.player[3] = savedPlayers[3];
        AST.setActiveStageBuilderTestStage(savedStage);
      }
      return n;
    },
  };
})();
