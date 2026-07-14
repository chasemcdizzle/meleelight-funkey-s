// M2-CAL capture page library (injected AFTER oracle/harness/{init,pagelib}.js).
// Wraps the exported boundary of the environmentalCollision webpack module
// with recording wrappers. See port/sim/calib/FORMAT.md.
//
// Requires: window.__wpCache (exposed by run-capture.js's served-bytes
// injection into the webpack bootstrap of dist/js/main.js).
(() => {
  // --- canon v1 (FORMAT.md): CHECKSUM.md structure, bit-pattern numbers ---
  const buf = new ArrayBuffer(8);
  const f64 = new Float64Array(buf);
  const u32 = new Uint32Array(buf);
  f64[0] = 1.0;
  const HI = u32[1] === 0x3ff00000 ? 1 : 0;
  const LO = 1 - HI;
  function dhex(x) {
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
  window.__envcollCanon = (v) => canon(v, new Set());

  // --- the boundary ------------------------------------------------------
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

  window.__envcollBuffer = [];
  window.__envcollCounts = {};
  for (const n of BOUNDARY) window.__envcollCounts[n] = 0;

  window.__envcollInstall = function () {
    const cache = window.__wpCache;
    if (!cache) throw new Error("capture: window.__wpCache missing (bootstrap injection failed)");
    let target = null;
    let targetId = null;
    for (const id of Object.keys(cache)) {
      const ex = cache[id] && cache[id].exports;
      if (ex && typeof ex.runCollisionRoutine === "function" &&
          typeof ex.coordinateInterceptParameter === "function" &&
          typeof ex.moveAlongGround === "function") {
        if (target !== null) throw new Error("capture: TWO modules match the boundary signature");
        target = ex;
        targetId = id;
      }
    }
    if (target === null) throw new Error("capture: environmentalCollision module not found in webpack cache");
    for (const name of BOUNDARY) {
      if (typeof target[name] !== "function") {
        throw new Error("capture: expected export missing/not a function: " + name);
      }
    }
    for (const name of CONSTANTS) {
      if (typeof target[name] !== "number") {
        throw new Error("capture: expected constant export missing: " + name);
      }
    }
    for (const name of BOUNDARY) {
      const orig = target[name];
      const proj = ARG_PROJECTION[name] || null;
      target[name] = function () {
        const args = Array.prototype.slice.call(arguments);
        const ret = orig.apply(this, arguments);
        const f = window.__inStep ? window.__frameCount + 1 : 0;
        const argsC = canon(proj ? proj(args) : args, new Set());
        const retC = canon(ret, new Set());
        window.__envcollBuffer.push(f + "\t" + name + "\t" + argsC + "\t" + retC);
        window.__envcollCounts[name]++;
        return ret;
      };
    }
    return { moduleId: targetId, wrapped: BOUNDARY.length };
  };

  // Drain the buffer as one string (transport to the node runner).
  window.__envcollDrain = function () {
    const s = window.__envcollBuffer.join("\n");
    window.__envcollBuffer = [];
    return s;
  };
})();
