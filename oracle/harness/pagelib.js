// Oracle harness page library (maintained copy; spike original frozen at
// spikes/determinism/harness/pagelib.js).
// Injected via addInitScript (after init.js). Pure page-side helpers:
// stable state serialization, SHA-256, and the frame step loop.
(() => {
  // --- stable serialization ---------------------------------------------
  // Sorted object keys; numbers via String() (shortest round-trip — injective
  // on doubles); -0 / NaN / Infinity made explicit; typed arrays as arrays;
  // functions collapse to "fn"; cycles collapse to "cyc".
  function numStr(v) {
    if (Object.is(v, -0)) return "-0";
    return String(v); // "NaN", "Infinity", "-Infinity" fall out naturally
  }
  function ser(v, seen) {
    if (v === null) return "null";
    const t = typeof v;
    if (t === "number") return numStr(v);
    if (t === "string") return JSON.stringify(v);
    if (t === "boolean") return v ? "T" : "F";
    if (t === "undefined") return "undef";
    if (t === "function") return "fn";
    if (seen.has(v)) return "cyc";
    seen.add(v);
    let out;
    if (Array.isArray(v) || ArrayBuffer.isView(v)) {
      const parts = [];
      for (let i = 0; i < v.length; i++) parts.push(ser(v[i], seen));
      out = "[" + parts.join(",") + "]";
    } else {
      const ks = Object.keys(v).sort();
      const parts = [];
      for (const k of ks) parts.push(JSON.stringify(k) + ":" + ser(v[k], seen));
      out = "{" + parts.join(",") + "}";
    }
    seen.delete(v);
    return out;
  }

  // Checksum surface: per active player -> phys, hit, timer, actionState,
  // percent, stocks, active hitboxes; plus the live article queue.
  window.__serializeState = function () {
    const H = window.__harness;
    const players = H.getPlayers();
    const ptype = H.getPlayerType();
    const seen = new Set();
    const parts = [];
    for (let i = 0; i < 4; i++) {
      if (ptype[i] > -1) {
        const p = players[i];
        parts.push(
          '"p' + i + '":{' +
          '"actionState":' + ser(p.actionState, seen) + "," +
          '"timer":' + ser(p.timer, seen) + "," +
          '"percent":' + ser(p.percent, seen) + "," +
          '"stocks":' + ser(p.stocks, seen) + "," +
          '"hit":' + ser(p.hit, seen) + "," +
          '"hitboxes":' + ser(p.hitboxes, seen) + "," +
          '"phys":' + ser(p.phys, seen) +
          "}");
      }
    }
    parts.push('"articles":' + ser(H.getArticles(), seen));
    return "{" + parts.join(",") + "}";
  };

  // --- SHA-256 (crypto.subtle; localhost is a secure context) ------------
  const te = new TextEncoder();
  window.__sha256 = async function (s) {
    const buf = await crypto.subtle.digest("SHA-256", te.encode(s));
    return Array.from(new Uint8Array(buf))
      .map((b) => b.toString(16).padStart(2, "0"))
      .join("");
  };

  // --- step loop ----------------------------------------------------------
  window.__frameCount = 0;
  window.__trace = null;      // array: frame -> [inputP0|null, ..., inputP3|null]
  window.__captured = {};     // frame -> serialized state (only when capture on)
  window.__asSeen = {};       // actionState -> first frame seen (coverage)
  window.__maxArticles = 0;

  // Run n frames; returns [{f, h}] plus updates coverage. opts.capture
  // stores full serialized state per frame (memory-heavy; used for
  // divergence diagnosis). opts.captureFrames limits capture to a Set-like
  // object {frame:true} when provided.
  window.__runFrames = async function (n, opts) {
    opts = opts || {};
    const out = [];
    const H = window.__harness;
    for (let k = 0; k < n; k++) {
      const f = window.__frameCount;
      // synthetic inputs for this frame (held-last / neutral past trace end)
      if (window.__trace) {
        const idx = Math.min(f, window.__trace.length - 1);
        window.__harnessInputs = window.__trace[idx];
      }
      window.__vAdvance(1000 / 60);
      window.__inStep = true;
      H.step();
      window.__inStep = false;
      window.__frameCount = f + 1;
      const snap = window.__serializeState();
      const h = await window.__sha256(snap);
      out.push({ f: f + 1, h: h });
      if (opts.capture || (opts.captureFrames && opts.captureFrames[f + 1])) {
        window.__captured[f + 1] = snap;
      }
      // coverage bookkeeping
      const players = H.getPlayers();
      const ptype = H.getPlayerType();
      for (let i = 0; i < 4; i++) {
        if (ptype[i] > -1 && !(players[i].actionState in window.__asSeen)) {
          window.__asSeen[players[i].actionState] = f + 1;
        }
      }
      const na = H.getArticles().length;
      if (na > window.__maxArticles) window.__maxArticles = na;
    }
    return out;
  };

  window.__coverage = function () {
    const H = window.__harness;
    const players = H.getPlayers();
    const ptype = H.getPlayerType();
    const stocks = [];
    const percents = [];
    for (let i = 0; i < 4; i++) {
      if (ptype[i] > -1) {
        stocks.push(players[i].stocks);
        percents.push(players[i].percent);
      }
    }
    return {
      actionStatesSeen: window.__asSeen,
      maxArticles: window.__maxArticles,
      stocks: stocks,
      percents: percents,
      matchTimer: H.getMatchTimer(),
      gameMode: H.getGameMode(),
      playing: H.isPlaying(),
      rngCalls: window.__rngCalls,
      rngCallsOutsideStep: window.__rngCallsOutsideStep,
      rngOutsideStacks: window.__rngOutsideStacks,
      mathCalls: window.__mathCalls,
    };
  };
})();
