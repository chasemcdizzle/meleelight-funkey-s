// spec-aiport.js — M4 task 4 capture spec: the ai.js PORT boundary over
// the CPU goldens g07/g08. Registers window.__capSpecs.aiport. Injected
// AFTER capturelib.js. See FORMAT.md "The aiport spec".
//
// The M2 ai spec (spec-ai.js — FROZEN, untouched) records only the POST
// envelope {bank,bk,rng}; a C-port replay needs the marshalable PRE
// state. This spec wraps runAI ONLY and records per call:
//   args = [i, pre] where pre is the grep-measured runAI READ-SET
//   projection (players / bank rows 0-1 of slot i / cS / playerType /
//   gameSettings.turbo / activeStage geometry), post = {bank, bk, rng}
//   byte-parallel to the M2 spec's envelope.
// Under-projection fails LOUD in the C replay (strict marshal, rule 7).
//
// The M2 write-set RECONCILIATION is carried over verbatim (wsViol
// pinned ZERO; belt and braces on the new capture). RNG: rngBoot +
// attributed runAI draw lists + standalone Math.random records.
//
// SWEEP (rules 11/12): g07's falcon AI hits zero RNG arms and g08's puff
// covers only part of the machine; task 5 (live C-AI, new d1/d9 traces)
// and M4 live play are the reachable future domain. A deterministic
// frame-0 preset battery drives runAI through the otherwise-dead arms:
// all four player slots are swapped for fresh upstream playerObject()s,
// playerType/cS/bank rows/gameSettings.turbo swapped per preset,
// Math.random swapped for a local mulberry32 (seed 0x0badf00d) for the
// sweep window (the replay mirrors it for ALL frame-0 runAI records),
// and EVERYTHING restored in finally — non-perturbation is proven
// mechanically by x2 byte-stability + STREAM MATCH (rule 12).
(() => {
  const BK = ["currentAction", "currentSubaction", "curentAction", "lastMash"];

  window.__capSpecs.aiport = {
    expectWrapped: 1,

    install(ctx) {
      const cache = ctx.cache;
      const moduleIds = {};
      const find = (pred, what) => {
        const m = ctx.findModule(cache, pred, what);
        moduleIds[what] = m.id;
        return m.exports;
      };

      const AI = find((ex) =>
        typeof ex.runAI === "function" &&
        typeof ex.generalAI === "function" &&
        typeof ex.NearestEnemy === "function", "ai");
      const INP = find((ex) =>
        typeof ex.pollInputs === "function" &&
        typeof ex.nullInputs === "function" &&
        Array.isArray(ex.aiInputBank), "input");
      const M = find((ex) =>
        Array.isArray(ex.player) && Array.isArray(ex.playerType) &&
        Array.isArray(ex.characterSelections) &&
        typeof ex.gameTick === "function", "main");
      const S = find((ex) =>
        ex.gameSettings && typeof ex.gameSettings === "object" &&
        Object.prototype.hasOwnProperty.call(ex.gameSettings, "tapJumpOffp1"),
        "settings");
      const AST = find((ex) =>
        typeof ex.getActiveStage === "function" &&
        typeof ex.setVsStage === "function", "activeStage");
      const PJ = find((ex) =>
        typeof ex.playerObject === "function" &&
        typeof ex.physicsObject === "function", "player");

      const bank = INP.aiInputBank;
      if (bank.length !== 4 || !bank.every((r) => Array.isArray(r) && r.length === 8)) {
        throw new Error("aiport spec: aiInputBank is not 4x8");
      }

      // --- seeded-RNG stream (spec-ai mechanism) + sweep-generator hook ---
      const seed = window.__harnessConfig.seed >>> 0;
      const bootDraws = window.__rngCalls;
      const ffState =
        (seed + (Math.imul(bootDraws, 0x6D2B79F5) >>> 0)) % 4294967296;
      ctx.declare("rngBoot");
      ctx.declare("Math.random");
      ctx.declare("runAI");
      ctx.declare("wsViol");
      ctx.push("rngBoot",
        "[" + ctx.canon(seed) + "," + ctx.canon(bootDraws) + "]",
        ctx.canon(ffState));
      let aiWindow = null;  // draw list while inside runAI, else null
      let sweepGen = null;  // non-null only inside sweep(): draws bypass
                            // the seeded stream (rule 12)
      const mr = Math.random;
      Math.random = function () {
        const v = sweepGen ? sweepGen() : mr();
        if (aiWindow) aiWindow.push(v);
        else ctx.push("Math.random", "[]", ctx.canon(v));
        return v;
      };

      // --- the runAI READ-SET projection (pre envelope) -------------------
      // Measured from ai.js by reading + grep (AGENT-LOG iter 75). The
      // top-level `grounded` records the q1 undefined read truthfully;
      // `curentAction` is a conditional own-key.
      const projPlayer = (p) => {
        const o = {
          actionState: p.actionState,
          charAttributes: { multiJump: (p.charAttributes || {}).multiJump },
          currentAction: p.currentAction,
          currentSubaction: p.currentSubaction,
          difficulty: p.difficulty,
          grounded: p.grounded,
          hasHit: p.hasHit,
          hit: { hitlag: p.hit.hitlag, hitstun: p.hit.hitstun },
          lastMash: p.lastMash,
          phys: {
            ECBp: p.phys.ECBp,
            cVel: p.phys.cVel,
            doubleJumped: p.phys.doubleJumped,
            face: p.phys.face,
            fastfalled: p.phys.fastfalled,
            grounded: p.phys.grounded,
            hurtBoxState: p.phys.hurtBoxState,
            jumpsUsed: p.phys.jumpsUsed,
            kVel: p.phys.kVel,
            onLedge: p.phys.onLedge,
            pos: p.phys.pos,
            shieldHP: p.phys.shieldHP,
          },
          timer: p.timer,
        };
        if (Object.prototype.hasOwnProperty.call(p, "curentAction")) {
          o.curentAction = p.curentAction;
        }
        return o;
      };
      const preSnap = (i) => {
        const aS = AST.getActiveStage();
        return ctx.canon({
          as: { ground: aS.ground, ledgePos: aS.ledgePos, platform: aS.platform },
          bank2: [bank[i][0], bank[i][1]],
          cs: M.characterSelections,
          gs: { turbo: S.gameSettings.turbo },
          p: [projPlayer(M.player[0]), projPlayer(M.player[1]),
              projPlayer(M.player[2]), projPlayer(M.player[3])],
          pt: M.playerType,
        });
      };

      // --- write-set recon (spec-ai verbatim) -----------------------------
      const playerMinusBK = (k) => {
        const src = M.player[k];
        const o = {};
        for (const key of Object.keys(src)) {
          if (BK.indexOf(key) === -1) o[key] = src[key];
        }
        return ctx.canon(o);
      };
      const reconSnap = () => {
        const rows = [];
        for (let k = 0; k < 4; k++) {
          for (let j = 0; j < 8; j++) rows.push(ctx.canon(bank[k][j]));
        }
        return {
          players: [playerMinusBK(0), playerMinusBK(1),
                    playerMinusBK(2), playerMinusBK(3)],
          bank: rows,
          pt: ctx.canon(M.playerType),
          cs: ctx.canon(M.characterSelections),
          gs: ctx.canon(S.gameSettings),
          as: ctx.canon(AST.getActiveStage()),
        };
      };
      let wsViolations = 0;
      const viol = (what) => {
        wsViolations++;
        ctx.push("wsViol", ctx.canon([what]), "null");
      };

      // --- runAI wrapper --------------------------------------------------
      const origRunAI = AI.runAI;
      AI.runAI = function (i) {
        if (aiWindow !== null) throw new Error("aiport spec: runAI nested");
        const argsCanon = "[" + ctx.canon(i) + "," + preSnap(i) + "]";
        const pre = reconSnap();
        aiWindow = [];
        const ret = origRunAI.apply(this, arguments);
        const draws = aiWindow;
        aiWindow = null;
        const post = reconSnap();

        for (let k = 0; k < 4; k++) {
          if (post.players[k] !== pre.players[k]) {
            viol("player[" + k + "] (minus bookkeeping) changed");
          }
        }
        for (let k = 0; k < 4; k++) {
          for (let j = 0; j < 8; j++) {
            if (k === i && j === 0) continue;
            if (post.bank[k * 8 + j] !== pre.bank[k * 8 + j]) {
              viol("aiInputBank[" + k + "][" + j + "] changed");
            }
          }
        }
        if (post.pt !== pre.pt) viol("playerType changed");
        if (post.cs !== pre.cs) viol("characterSelections changed");
        if (post.gs !== pre.gs) viol("gameSettings changed");
        if (post.as !== pre.as) viol("activeStage changed");

        const p = M.player[i];
        const postCanon =
          '{"bank":' + ctx.canon(bank[i][0]) +
          ',"bk":' + ctx.canon({ ca: p.currentAction, cs: p.currentSubaction,
                                 cta: p.curentAction, lm: p.lastMash }) +
          ',"rng":[' + draws.map((v) => ctx.canon(v)).join(",") + "]}";
        ctx.push("runAI", argsCanon, ctx.canon(ret), postCanon);
        return ret;
      };
      ctx.wrapped++;

      this._spec_state = {
        getViol: () => wsViolations,
        AI: AI, INP: INP, M: M, S: S, PJ: PJ,
        setSweepGen: (g) => { sweepGen = g; },
      };
      return { moduleIds: moduleIds };
    },

    // Deterministic frame-0 preset battery (header comment). Every call
    // goes through the WRAPPED runAI, so each preset records pre + post +
    // draws exactly like a live record; the C replay treats frame-0
    // records as sweep-RNG records. Restoration is total (finally);
    // proven by x2 byte-stability + STREAM MATCH.
    sweep() {
      const st = this._spec_state;
      const M = st.M, AI = st.AI, S = st.S, PJ = st.PJ;
      const bank = st.INP.aiInputBank;
      let n = 0;

      const IN22 = () => ({
        a: false, b: false, x: false, y: false, z: false, l: false,
        r: false, s: false, du: false, dl: false, dr: false, dd: false,
        lA: 0, rA: 0, lsX: 0, lsY: 0, csX: 0, csY: 0,
        rawX: 0, rawY: 0, rawcsX: 0, rawcsY: 0,
      });

      const saved = {
        p: [M.player[0], M.player[1], M.player[2], M.player[3]],
        pt: [M.playerType[0], M.playerType[1], M.playerType[2], M.playerType[3]],
        cs: [M.characterSelections[0], M.characterSelections[1],
             M.characterSelections[2], M.characterSelections[3]],
        turbo: S.gameSettings.turbo,
        row0: bank[1][0], row1: bank[1][1],
      };
      // local mulberry32 (seed 0x0badf00d) — the replay mirrors it for
      // every frame-0 runAI record
      let a = 0x0badf00d | 0;
      st.setSweepGen(function () {
        a = (a + 0x6D2B79F5) | 0;
        let t = Math.imul(a ^ (a >>> 15), 1 | a);
        t = (t + Math.imul(t ^ (t >>> 7), 61 | t)) ^ t;
        return ((t ^ (t >>> 14)) >>> 0) / 4294967296;
      });

      // Preset runner: fresh world, cpu = slot 1 at char `ch`, then
      // mut(cpu, enemy), then ONE wrapped runAI(1) call. Default cpu pos
      // (-30, 10) deliberately fails the :157 above-platform typo check
      // (isAboveGround(x, x+1) -> "none" for negative x on the page-start
      // stage) so default presets never consume the platform-drop draw.
      const run = (ch, mut) => {
        for (let k = 0; k < 4; k++) {
          M.player[k] = new PJ.playerObject(k === 1 ? ch : 0, [10 + k, 20], 1);
        }
        M.playerType[0] = 0; M.playerType[1] = 1;
        M.playerType[2] = -1; M.playerType[3] = -1;
        M.characterSelections[0] = 0; M.characterSelections[1] = ch;
        M.characterSelections[2] = 0; M.characterSelections[3] = 0;
        S.gameSettings.turbo = false;
        bank[1][0] = IN22(); bank[1][1] = IN22();
        const cpu = M.player[1], en = M.player[0];
        // deterministic defaults (page-start stage: ground +-68.4 at y 0,
        // platforms [-57.6,-20]/[20,57.6] at 27.2, [-18.8,18.8] at 54.4)
        cpu.actionState = "WAIT"; cpu.timer = 0; cpu.difficulty = 5;
        cpu.phys.pos = { x: -30, y: 10 }; cpu.phys.grounded = true;
        cpu.phys.face = 1; cpu.phys.onLedge = -1;
        cpu.phys.cVel = { x: 0, y: 0 }; cpu.phys.kVel = { x: 0, y: 0 };
        cpu.phys.ECBp = [{ x: -30, y: 5 }, { x: -29, y: 8 },
                         { x: -30, y: 11 }, { x: -31, y: 8 }];
        cpu.phys.hurtBoxState = 0; cpu.phys.shieldHP = 60;
        cpu.phys.doubleJumped = false; cpu.phys.jumpsUsed = 0;
        cpu.phys.fastfalled = false;
        en.actionState = "WAIT"; en.phys.pos = { x: 0, y: 0 };
        en.phys.grounded = true; en.phys.onLedge = -1;
        en.phys.cVel = { x: 0, y: 0 }; en.phys.hurtBoxState = 0;
        if (mut) mut(cpu, en);
        AI.runAI(1);
        n++;
      };
      // airborne helper; offstage iff |x| > 68.4 (ECBp[0].y kept >= 0 so
      // in-range positions stay ONSTAGE for isOffstage)
      const air = (cpu, x, y) => {
        cpu.phys.grounded = false;
        cpu.phys.pos = { x: x, y: y };
        cpu.phys.ECBp[0] = { x: x, y: Math.max(0, y - 5) };
      };

      try {
        // --- generalAI generic arms (char 4 = falcon, empty char AI) ----
        run(4);                                         // walk-toward base
        run(3);                                         // falco base
        run(4, (c, e) => { e.actionState = "SLEEP"; }); // NearestEnemy failsafe
        for (let k = 0; k < 5; k++) run(4, (c) => { c.actionState = "CATCHWAIT"; });
        run(4, (c) => { c.actionState = "GRABRELEASE"; c.timer = 3; });
        run(4, (c) => { c.currentAction = "DROPTHROUGHPLATFORM"; c.actionState = "SQUAT"; });
        run(4, (c) => { c.currentAction = "DROPTHROUGHPLATFORM"; }); // clear
        run(4, (c) => { c.currentSubaction = "LEFT"; });             // LR clear
        // RUNOFFPLATFORM machine
        run(4, (c) => { c.currentAction = "RUNOFFPLATFORM"; c.currentSubaction = "LEFT"; c.actionState = "WALK"; });
        run(4, (c) => { c.currentAction = "RUNOFFPLATFORM"; c.currentSubaction = "RIGHT"; c.actionState = "RUN"; });
        run(4, (c) => { c.currentAction = "RUNOFFPLATFORM"; c.currentSubaction = "LEFT"; c.actionState = "FALL"; air(c, 30, 30); c.timer = 2; c.phys.cVel = { x: 0, y: -1 }; });
        run(4, (c) => { c.currentAction = "RUNOFFPLATFORM"; c.currentSubaction = "RIGHT"; c.actionState = "FALL"; air(c, 30, 30); c.timer = 2; c.phys.cVel = { x: 0, y: -1 }; });
        run(4, (c) => { c.currentAction = "RUNOFFPLATFORM"; c.actionState = "SMASHTURN"; c.timer = 1; });
        run(4, (c) => { c.currentAction = "RUNOFFPLATFORM"; c.actionState = "SMASHTURN"; c.timer = 3; });
        run(4, (c) => { c.currentAction = "RUNOFFPLATFORM"; c.actionState = "JUMPF"; });   // :114 clear
        run(4, (c) => { c.currentAction = "RUNOFFPLATFORM"; c.actionState = "FALL"; air(c, 100, 30); }); // :110 offstage clear
        run(4, (c) => { c.currentSubaction = "UPTILT"; });           // :150 clear
        // platform drop (pos (30,30): isAboveGround(30,31) -> platform)
        for (let k = 0; k < 4; k++) {
          run(4, (c, e) => { c.phys.pos = { x: 30, y: 30 }; c.phys.ECBp[0] = { x: 30, y: 25 }; e.phys.pos = { x: 25, y: 0 }; });
        }
        // SHIELD machine (CPUShield). shieldHP 5 -> doSomethingChance 100
        // (tan blowup near breaking): the do-something arm fires
        // DETERMINISTICALLY; shieldHP 20 (~43) leaves the no-op path to
        // the stream.
        for (let k = 0; k < 2; k++) {
          run(4, (c) => { c.currentAction = "SHIELD"; c.actionState = "GUARD"; c.phys.shieldHP = 20; });
        }
        for (let k = 0; k < 2; k++) {
          run(4, (c) => { c.currentAction = "SHIELD"; c.actionState = "GUARD"; c.phys.shieldHP = 5; });
        }
        run(4, (c) => { c.currentAction = "SHIELD"; c.actionState = "GUARD"; c.phys.shieldHP = 5; c.phys.pos = { x: 30, y: 30 }; c.phys.ECBp[0] = { x: 30, y: 25 }; }); // shield-drop platform arm
        run(4, (c) => { c.currentAction = "SHIELD"; c.actionState = "FALL"; }); // clear
        // LEDGESTALL clears (the GRAB completion path is dead upstream —
        // :219 always clears because currentAction "LEDGESTALL" is never
        // in its list; measured this task, carried verbatim)
        run(4, (c) => { c.currentAction = "LEDGESTALL"; c.currentSubaction = "FALL"; c.actionState = "WAIT"; });
        run(4, (c) => { c.currentAction = "LEDGESTALL"; c.currentSubaction = "GRAB"; c.actionState = "CLIFFWAIT"; c.phys.onLedge = 0; c.phys.grounded = false; });
        run(4, (c) => { c.currentAction = "LEDGEATTACK"; });         // :228 clear
        // LEDGEDASH via :233 (CPULedge char arms)
        run(0, (c) => { c.currentAction = "LEDGEDASH"; c.actionState = "CLIFFWAIT"; c.phys.onLedge = 0; c.phys.grounded = false; c.timer = 18; });
        run(0, (c) => { c.currentAction = "LEDGEDASH"; c.actionState = "CLIFFWAIT"; c.phys.onLedge = 0; c.phys.grounded = false; c.timer = 2; });
        run(1, (c) => { c.currentAction = "LEDGEDASH"; c.actionState = "JUMPAERIAL1"; air(c, -30, 10); c.timer = 6; });
        run(1, (c) => { c.currentAction = "LEDGEDASH"; c.actionState = "CLIFFWAIT"; c.phys.onLedge = 0; c.phys.grounded = false; c.timer = 2; });
        run(2, (c) => { c.currentAction = "LEDGEDASH"; c.actionState = "CLIFFWAIT"; c.phys.onLedge = 0; c.phys.grounded = false; c.timer = 5; });
        run(2, (c) => { c.currentAction = "LEDGEDASH"; c.actionState = "CLIFFWAIT"; c.phys.onLedge = 0; c.phys.grounded = false; c.timer = 2; });
        run(4, (c) => { c.currentAction = "LEDGEDASH"; c.actionState = "WAIT"; }); // :234 clear
        // SDI (difficulty == 4 exactly)
        run(4, (c) => { c.difficulty = 4; c.hit.hitlag = 2; air(c, 100, 30); c.timer = 4; });
        run(4, (c) => { c.difficulty = 4; c.hit.hitlag = 2; air(c, 100, 30); c.timer = 5; });
        // CPUrecover (pos.x 100 -> offstage; NOTE the fox side-b arm
        // :1544 is DEAD upstream — its |ydiff|<=10 window contradicts the
        // :1521 falling gate's ydiff>10/25 — measured this task)
        run(2, (c) => { c.actionState = "UPSPECIALCHARGE"; air(c, 100, 30); c.timer = 41; });
        run(2, (c) => { c.actionState = "UPSPECIALCHARGE"; air(c, 100, 30); c.timer = 10; });
        run(2, (c) => { c.actionState = "UPSPECIALLAUNCH"; air(c, 100, 30); });
        for (let k = 0; k < 2; k++) {
          run(2, (c) => { c.actionState = "FALL"; air(c, 100, -30); c.phys.cVel = { x: 0, y: -1 }; });
        }
        run(2, (c) => { c.actionState = "FALL"; air(c, 100, -45); c.phys.cVel = { x: 0, y: -1 }; c.phys.doubleJumped = true; }); // firefox arm
        run(0, (c) => { c.actionState = "FALL"; air(c, 100, 0); c.phys.cVel = { x: 2, y: -1 }; });  // marth side-b arm
        run(0, (c) => { c.actionState = "FALL"; air(c, 100, -65); c.phys.cVel = { x: 0, y: -1 }; c.phys.doubleJumped = true; }); // marth up-b arm
        run(0, (c) => { c.actionState = "UPSPECIAL"; air(c, 100, -10); }); // :1571 arm
        for (let k = 0; k < 2; k++) {
          run(1, (c) => { c.actionState = "FALL"; air(c, 100, -30); c.phys.cVel = { x: 0, y: -1 }; c.phys.doubleJumped = true; c.phys.jumpsUsed = 2; }); // puff multijump
        }
        run(4, (c) => { c.actionState = "FALL"; air(c, 100, -30); c.phys.cVel = { x: 0, y: -1 }; c.phys.doubleJumped = true; }); // generic arm
        // REVERSEUPTILT machine
        run(4, (c) => { c.currentAction = "REVERSEUPTILT"; c.currentSubaction = "REVERSE"; c.actionState = "RUN"; });
        run(4, (c) => { c.currentAction = "REVERSEUPTILT"; c.currentSubaction = "UPTILT"; c.actionState = "RUN"; c.timer = 2; });
        run(4, (c) => { c.currentAction = "REVERSEUPTILT"; c.actionState = "WAIT"; }); // clear
        // MASHING / TECH / SMASHTURN clears + the q2 typo arm
        run(4, (c) => { c.currentAction = "MASHING"; c.timer = 3; });
        run(4, (c) => { c.currentAction = "MASHING"; c.timer = 1; c.lastMash = 5; }); // q2 arm: ca STAYS "MASHING"
        run(4, (c) => { c.currentAction = "SMASHTURN"; c.actionState = "RUN"; c.timer = 1; });
        run(4, (c) => { c.currentAction = "TECH"; });
        run(4, (c) => { c.currentAction = "TECH"; c.actionState = "RUN"; c.hit.hitstun = 3; }); // :405 clear
        // DAMAGEFALL family (in-range airborne with ECBp[0].y >= 0 stays
        // onstage, so !isOffstage holds)
        run(4, (c) => { c.actionState = "DAMAGEFALL"; air(c, 30, 30); c.phys.doubleJumped = true; });
        run(4, (c) => { c.actionState = "DAMAGEFALL"; air(c, 30, 30); });                       // extra=3 arm
        run(4, (c) => { c.actionState = "DAMAGEFLYN"; air(c, 30, 30); c.phys.doubleJumped = true; });
        for (let k = 0; k < 2; k++) {
          run(4, (c) => { c.actionState = "DAMAGEFALL"; air(c, 30, 2); c.hit.hitstun = 5; c.phys.kVel = { x: 0, y: -1 }; }); // CPUTech window
        }
        run(4, (c) => { c.actionState = "DAMAGEFALL"; air(c, 30, 50); c.hit.hitstun = 5; }); // CPUTech no-window
        // DOWNWAIT (CPUMissedTech)
        for (let k = 0; k < 3; k++) run(4, (c) => { c.actionState = "DOWNWAIT"; });
        // CAPTURE mash
        run(4, (c) => { c.actionState = "CAPTUREWAIT"; c.lastMash = 5; });                      // a-arm
        run(4, (c) => { c.actionState = "CAPTUREWAIT"; c.lastMash = 5; bank[1][1].a = true; }); // y-arm
        run(4, (c) => { c.actionState = "CAPTUREWAIT"; c.difficulty = 1; });                    // no-burst arm
        // WAVESHINEANY (CPUWaveshineAny)
        run(4, (c) => { c.currentAction = "WAVESHINEANY"; c.actionState = "WAIT"; });
        run(4, (c) => { c.currentAction = "WAVESHINEANY"; c.actionState = "DOWNSPECIALGROUND"; c.timer = 4; });
        for (let k = 0; k < 2; k++) {
          run(4, (c) => { c.currentAction = "WAVESHINEANY"; c.actionState = "KNEEBEND"; c.timer = 3; });
        }
        // CAPTURECUT -> CPUGrabRelease (fox arm x3, other arm x2)
        for (let k = 0; k < 3; k++) run(2, (c) => { c.actionState = "CAPTURECUT"; });
        for (let k = 0; k < 2; k++) run(0, (c) => { c.actionState = "CAPTURECUT"; });
        run(4, (c) => { c.currentAction = "GRABRELEASE"; c.actionState = "WAIT"; }); // WAIT arm via ca
        run(4, (c) => { c.actionState = "REBIRTHWAIT"; });
        // CLIFFWAIT -> CPULedge main draw (arms are draw-dependent; x6)
        for (let k = 0; k < 6; k++) {
          run(4, (c) => { c.actionState = "CLIFFWAIT"; c.phys.onLedge = 0; c.phys.grounded = false; });
        }
        // TOURNAMENTWINNER paths. NOTE (measured this task): the
        // ai.js:1254 `curentAction` typo write is DEAD upstream — reaching
        // CPULedge with currentAction "TOURNAMENTWINNER" requires paction
        // CLIFF* (else :228 clears it first), while :1253 requires paction
        // "FALLAERIAL". The presets cover both reachable guards.
        run(4, (c) => { c.currentAction = "TOURNAMENTWINNER"; c.actionState = "CLIFFWAIT"; c.phys.onLedge = 0; c.phys.grounded = false; });
        run(4, (c) => { c.currentAction = "TOURNAMENTWINNER"; c.actionState = "FALLAERIAL"; air(c, 30, 30); }); // :228 clear path
        // LEDGEJUMP / LEDGEAIRATTACK / LEDGEAIRATTACK2
        run(4, (c) => { c.currentAction = "LEDGEJUMP"; });                       // grounded clear
        run(4, (c) => { c.currentAction = "LEDGEJUMP"; c.actionState = "FALL"; air(c, 30, 30); });
        run(0, (c) => { c.currentAction = "LEDGEAIRATTACK"; c.actionState = "CLIFFWAIT"; c.phys.onLedge = 0; c.phys.grounded = false; c.timer = 1; });
        run(0, (c) => { c.currentAction = "LEDGEAIRATTACK"; c.actionState = "FALL"; air(c, 30, 30); c.timer = 3; });
        run(1, (c) => { c.currentAction = "LEDGEAIRATTACK"; c.actionState = "FALL"; air(c, 30, 30); c.timer = 3; });
        run(2, (c) => { c.currentAction = "LEDGEAIRATTACK"; c.actionState = "FALL"; air(c, 30, 30); c.timer = 3; });
        run(2, (c) => { c.currentAction = "LEDGEAIRATTACK"; c.actionState = "FALL"; air(c, 30, 30); c.timer = 6; });
        run(4, (c) => { c.currentAction = "LEDGEAIRATTACK2"; c.actionState = "ATTACKAIRN"; air(c, 30, 3); c.phys.ECBp[0] = { x: 30, y: 0 }; c.phys.cVel = { x: 0, y: -1 }; }); // l-cancel + fastfall arms
        run(4, (c) => { c.currentAction = "LEDGEAIRATTACK2"; c.actionState = "ATTACKAIRD"; air(c, 30, 30); c.phys.cVel = { x: 0, y: 1 }; });
        run(4, (c) => { c.currentAction = "LEDGEAIRATTACK2"; c.actionState = "WAIT"; }); // grounded -> clear
        // --- marthAI (cS 0) ---
        run(0, (c) => { c.currentAction = "LEDGESTALL"; c.currentSubaction = "FALL"; c.actionState = "FALL"; air(c, 30, 30); c.timer = 7; });
        run(0, (c) => { c.currentAction = "LEDGESTALL"; c.currentSubaction = "FALL"; c.actionState = "FALL"; air(c, 30, 30); c.timer = 3; });
        run(0, (c) => { c.phys.face = -1; });                     // turn arm
        for (let k = 0; k < 4; k++) {
          run(0, (c, e) => { e.phys.pos = { x: -20, y: 10 }; }); // tilt draws (distx -10, face 1 aligned)
        }
        run(0, (c) => { S.gameSettings.turbo = true; c.hasHit = true; c.actionState = "WALK"; c.phys.face = -1; }); // turbo turn
        run(0, (c, e) => { S.gameSettings.turbo = true; c.hasHit = true; c.actionState = "WALK"; e.phys.pos = { x: -20, y: 10 }; }); // turbo tilt gate
        for (let k = 0; k < 2; k++) {
          run(0, (c, e) => { c.actionState = "DASH"; e.phys.pos = { x: -20, y: 10 }; e.phys.cVel = { x: -2, y: 0 }; }); // react: approaching
        }
        run(0, (c, e) => { c.actionState = "DASH"; e.phys.pos = { x: -20, y: 10 }; e.actionState = "GUARDON"; }); // react: GUARD
        for (let k = 0; k < 2; k++) {
          run(0, (c, e) => { c.actionState = "DASH"; e.phys.pos = { x: -20, y: 10 }; e.phys.cVel = { x: 2, y: 0 }; }); // react: retreat arm (<12.5)
        }
        // --- jiggsAI (cS 1) ---
        run(1, (c) => { c.phys.face = -1; });                     // turn arm
        for (let k = 0; k < 3; k++) {
          run(1, (c, e) => { e.phys.pos = { x: -20, y: 10 }; }); // tilt draws
        }
        run(1, (c, e) => { c.actionState = "DASH"; e.phys.pos = { x: -20, y: 10 }; e.phys.cVel = { x: -2, y: 0 }; }); // react
        // --- foxAI (cS 2) ---
        run(2, (c) => { c.currentAction = "LEDGESTALL"; c.currentSubaction = "FALL"; c.actionState = "FALL"; air(c, 30, 30); c.timer = 1; });
        run(2, (c) => { c.currentAction = "LEDGESTALL"; c.currentSubaction = "FALL"; c.actionState = "FALL"; air(c, 30, 30); c.timer = 3; });
        run(2, (c, e) => { e.actionState = "DEADUP"; });          // RESPAWNMULTISHINE trigger
        run(2, (c, e) => { e.actionState = "REBIRTH"; c.currentAction = "RESPAWNMULTISHINE"; c.currentSubaction = "NONE"; }); // cs -> JUMP
        run(2, (c, e) => { e.actionState = "DEADUP"; c.currentAction = "RESPAWNMULTISHINE"; c.currentSubaction = "SHINE"; });
        run(2, (c, e) => { e.actionState = "DEADUP"; c.currentAction = "RESPAWNMULTISHINE"; c.currentSubaction = "JUMP"; c.actionState = "DOWNSPECIALGROUND"; c.timer = 3; });
        run(2, (c, e) => { e.actionState = "DEADUP"; c.currentAction = "RESPAWNMULTISHINE"; c.currentSubaction = "SHINE2"; c.actionState = "KNEEBEND"; c.timer = 3; });
        run(2, (c) => { c.currentAction = "RESPAWNMULTISHINE"; c.currentSubaction = "JUMP"; c.actionState = "FALL"; air(c, 30, 30); }); // :746 clear + :772 in-arr eval
        for (let k = 0; k < 10; k++) {
          run(2, (c, e) => { e.phys.pos = { x: 60, y: 10 }; }); // SHDL trigger draws (distx -90; ==1 arm is stream-dependent, x10)
        }
        run(2, (c) => { c.currentAction = "SHDL"; c.currentSubaction = "LASER1"; c.actionState = "WAIT"; c.phys.pos = { x: -30, y: 10 }; });
        run(2, (c) => { c.currentAction = "SHDL"; c.currentSubaction = "LASER1"; c.actionState = "KNEEBEND"; c.timer = 3; });
        run(2, (c) => { c.currentAction = "SHDL"; c.currentSubaction = "LASER2"; c.actionState = "FALL"; air(c, 30, 30); c.timer = 10; });
        run(2, (c) => { c.currentAction = "SHDL"; c.currentSubaction = "LASER2"; c.actionState = "FALL"; air(c, 30, 30); c.timer = 2; });
        run(2, (c) => { c.phys.face = -1; });                     // turn arm
        for (let k = 0; k < 6; k++) {
          run(2, (c, e) => { e.phys.pos = { x: -20, y: 10 }; }); // tilt + REVERSEUPTILT draws
        }
        run(2, (c, e) => { c.actionState = "DASH"; e.phys.pos = { x: -20, y: 10 }; e.phys.cVel = { x: -2, y: 0 }; }); // react
      } finally {
        st.setSweepGen(null);
        for (let k = 0; k < 4; k++) M.player[k] = saved.p[k];
        for (let k = 0; k < 4; k++) M.playerType[k] = saved.pt[k];
        for (let k = 0; k < 4; k++) M.characterSelections[k] = saved.cs[k];
        S.gameSettings.turbo = saved.turbo;
        bank[1][0] = saved.row0; bank[1][1] = saved.row1;
      }
      return n;
    },
    finalCheck() {
      const n = this._spec_state.getViol();
      if (n > 0) {
        throw new Error("aiport spec: " + n + " write-set violations — " +
                        "ai.js wrote outside aiInputBank + bookkeeping");
      }
      return 0;
    },
  };
})();
