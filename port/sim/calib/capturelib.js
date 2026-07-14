// Module-boundary capture engine (injected AFTER oracle/harness/
// {init,pagelib}.js). Generalized for M2 (spec-driven; the M2-CAL
// envcoll spec is built in and its window API is unchanged). Wraps the
// exported boundary of webpack modules with recording wrappers. See
// port/sim/calib/FORMAT.md.
//
// Requires: window.__wpCache (exposed by run-capture.js's served-bytes
// injection into the webpack bootstrap of dist/js/main.js).
//
// Mutation-safety: argument canon is computed BEFORE the call (records
// pre-call argument state); the record is pushed after the return value
// exists, so nested boundary calls appear before their caller's record —
// identical ordering to the M2-CAL rig, and byte-identical output for
// pure boundaries (proven by fresh-vs-frozen envcoll capture cmp).
(() => {
  // --- canon v1 (FORMAT.md): CHECKSUM.md structure, bit-pattern numbers ---
  const buf = new ArrayBuffer(8);
  const f64 = new Float64Array(buf);
  const u32 = new Uint32Array(buf);
  f64[0] = 1.0;
  const HI = u32[1] === 0x3ff00000 ? 1 : 0;
  const LO = 1 - HI;
  function dhex(x) {
    // canon v1.1: ALL NaNs collapse to the canonical quiet NaN. NaN
    // payloads are unobservable in the sim domain (String(NaN) === "NaN"
    // in the real checksum stream; no typed-array aliasing of sim values)
    // and non-reproducible across engines/compilers (V8 emits e.g.
    // 0xfffefffffff6ffff from undefined-arithmetic; C compilers may
    // legally commute FP adds since payloads are unspecified). Injective
    // on JS number VALUES: -0 and every finite/infinite double keep their
    // exact bits. Measured no-op on the frozen envcoll captures (zero
    // NaNs in g01/g04/g06).
    if (x !== x) return "7ff8000000000000";
    f64[0] = x;
    return (u32[HI] >>> 0).toString(16).padStart(8, "0") +
           (u32[LO] >>> 0).toString(16).padStart(8, "0");
  }
  function canon(v, seen) {
    if (v === null) return "null";
    const t = typeof v;
    if (t === "number") return "d:" + dhex(v);
    if (t === "string") return JSON.stringify(v);
    if (t === "boolean") return v ? "T" : "F";
    if (t === "undefined") return "undef";
    if (t === "function") return "fn";
    if (seen.has(v)) return "cyc";
    seen.add(v);
    let out;
    if (Array.isArray(v) || ArrayBuffer.isView(v)) {
      const parts = [];
      for (let i = 0; i < v.length; i++) parts.push(canon(v[i], seen));
      out = "[" + parts.join(",") + "]";
    } else {
      const ks = Object.keys(v).sort();
      const parts = [];
      for (const k of ks) parts.push(JSON.stringify(k) + ":" + canon(v[k], seen));
      out = "{" + parts.join(",") + "}";
    }
    seen.delete(v);
    return out;
  }
  const canon1 = (v) => canon(v, new Set());
  window.__envcollCanon = canon1; // legacy alias (M2-CAL)
  window.__capCanon = canon1;

  // --- generic engine ------------------------------------------------------
  window.__capBuffer = [];
  window.__capCounts = {};
  window.__capSpecs = {};
  // legacy aliases (M2-CAL scripts + pins read these)
  window.__envcollBuffer = window.__capBuffer;
  window.__envcollCounts = window.__capCounts;

  // push one record: <frame> TAB <name> TAB <argsCanon> TAB <retCanon>
  function pushRecord(name, argsCanon, retCanon) {
    const f = window.__inStep ? window.__frameCount + 1 : 0;
    window.__capBuffer.push(f + "\t" + name + "\t" + argsCanon + "\t" + retCanon);
    window.__capCounts[name]++;
  }
  window.__capPush = pushRecord;

  // Locate exactly ONE module in the webpack cache whose exports satisfy
  // `predicate`; two matches (or none) is a hard failure.
  function findModule(cache, predicate, what) {
    let target = null;
    let targetId = null;
    for (const id of Object.keys(cache)) {
      const ex = cache[id] && cache[id].exports;
      if (ex && predicate(ex)) {
        if (target !== null) throw new Error("capture: TWO modules match signature: " + what);
        target = ex;
        targetId = id;
      }
    }
    if (target === null) throw new Error("capture: module not found in webpack cache: " + what);
    return { exports: target, id: targetId };
  }

  // Wrap one exported function (Babel-CJS exports are plain writable
  // properties; external call sites dereference at call time).
  function wrapExport(target, name, recName, proj) {
    if (typeof target[name] !== "function") {
      throw new Error("capture: expected export missing/not a function: " + name);
    }
    const orig = target[name];
    target[name] = function () {
      const args = Array.prototype.slice.call(arguments);
      const argsCanon = canon1(proj ? proj(args) : args); // PRE-call state
      const ret = orig.apply(this, arguments);
      pushRecord(recName, argsCanon, canon1(ret));
      return ret;
    };
    window.__capCounts[recName] = window.__capCounts[recName] || 0;
    return 1;
  }

  // Install a registered spec. A spec is:
  //   { expectWrapped: N,
  //     install(ctx) -> { moduleIds: {...} } }
  // where ctx = { cache, findModule, wrapExport, canon: canon1,
  //               push: pushRecord, declare(name) }.
  // `declare` registers a record name so its count starts at 0 (pins see
  // zero-call functions explicitly). install() must wrap exactly
  // expectWrapped functions (custom installers count via ctx.wrapped++).
  window.__capInstallSpec = function (specName) {
    const spec = window.__capSpecs[specName];
    if (!spec) throw new Error("capture: unknown spec: " + specName);
    const cache = window.__wpCache;
    if (!cache) throw new Error("capture: window.__wpCache missing (bootstrap injection failed)");
    const ctx = {
      cache: cache,
      findModule: findModule,
      canon: canon1,
      push: pushRecord,
      wrapped: 0,
      declare: function (name) {
        window.__capCounts[name] = window.__capCounts[name] || 0;
      },
      wrapExport: function (target, name, recName, proj) {
        this.wrapped += wrapExport(target, name, recName || name, proj);
      },
    };
    const res = spec.install(ctx);
    if (ctx.wrapped !== spec.expectWrapped) {
      throw new Error("capture: spec " + specName + " wrapped " + ctx.wrapped +
                      " functions, expected " + spec.expectWrapped);
    }
    return { spec: specName, wrapped: ctx.wrapped, moduleIds: res.moduleIds };
  };

  // Drain the buffer as one string (transport to the node runner).
  window.__capDrain = function () {
    const s = window.__capBuffer.join("\n");
    window.__capBuffer.length = 0;
    return s;
  };
  window.__envcollDrain = window.__capDrain; // legacy alias

  // --- built-in spec: envcoll (M2-CAL, unchanged boundary) -----------------
  // Every exported FUNCTION of src/physics/environmentalCollision.js.
  const BOUNDARY = [
    "hLineThrough", "hLineAt", "vLineThrough", "vLineAt", "lineThrough",
    "outwardsWallNormal", "coordinateInterceptParameter",
    "coordinateIntercept", "findCollision", "getSameAndOther",
    "moveAlongGround", "groundedECBSquashFactor", "runCollisionRoutine",
  ];
  // Exported constants (presence asserted, not wrapped).
  const CONSTANTS = ["additionalOffset", "smallestECBWidth", "smallestECBHeight"];

  // runCollisionRoutine's stage argument is captured as its module-read
  // projection (FORMAT.md "stage argument"): the five surface lists.
  function projectStage(stage) {
    return {
      ceiling: stage.ceiling, ground: stage.ground, platform: stage.platform,
      wallL: stage.wallL, wallR: stage.wallR,
    };
  }
  const ARG_PROJECTION = {
    runCollisionRoutine: (args) => {
      const out = args.slice();
      out[5] = projectStage(out[5]);
      return out;
    },
  };

  window.__capSpecs.envcoll = {
    expectWrapped: BOUNDARY.length,
    install(ctx) {
      const mod = ctx.findModule(ctx.cache, (ex) =>
        typeof ex.runCollisionRoutine === "function" &&
        typeof ex.coordinateInterceptParameter === "function" &&
        typeof ex.moveAlongGround === "function", "environmentalCollision");
      for (const name of BOUNDARY) {
        if (typeof mod.exports[name] !== "function") {
          throw new Error("capture: expected export missing/not a function: " + name);
        }
      }
      for (const name of CONSTANTS) {
        if (typeof mod.exports[name] !== "number") {
          throw new Error("capture: expected constant export missing: " + name);
        }
      }
      for (const name of BOUNDARY) {
        ctx.wrapExport(mod.exports, name, name, ARG_PROJECTION[name] || null);
      }
      return { moduleIds: { environmentalCollision: mod.id } };
    },
  };

  // Legacy M2-CAL entry point — same return shape as before.
  window.__envcollInstall = function () {
    const r = window.__capInstallSpec("envcoll");
    return { moduleId: r.moduleIds.environmentalCollision, wrapped: r.wrapped };
  };
})();
