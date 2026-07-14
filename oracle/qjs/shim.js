// oracle/qjs/shim.js — browser-environment shim for running the built
// meleelight bundle under the QuickJS oracle runtime (M0 task 6).
//
// PRINCIPLE (fix_plan §M0.6, iteration hard rules): this file provides
// missing HOST OBJECTS the bundle genuinely touches; it never fakes sim
// behavior. The sim core is DOM-free (docs/research/meleelight-anatomy.md
// §8); everything here exists to let the god-module's boot-time chrome
// (jQuery, canvas grabs, cookies, Howler, loaders) evaluate without a
// browser. The per-frame checksum stream against the frozen browser
// golden is the enforcement: if any stub here perturbed sim state, the
// stream would diverge and the replay would fail loudly.
//
// Every shimmed global below carries a "why". Iterated against the real
// bundle: each piece exists because boot crashed without it (or because
// browser/qjs parity demands it), not speculatively.
"use strict";

(function () {
  var G = globalThis;

  // --- window/self: the bundle is a classic browser script ---------------
  // why: webpack output + jQuery + Howler all key off `window`; the
  // harness files (init.js/pagelib.js) write their seams onto `window`.
  G.window = G;
  G.self = G;

  // why: dist/meleelight.html sets `window.offlineMode = true` before the
  // bundle loads and src/main/resize.js reads it (browser parity).
  G.offlineMode = true;

  // --- console --------------------------------------------------------
  // why: quickjs-libc provides console.log only; the bundle calls
  // console.error/warn/info (webpack localforage warning, Howler).
  ["error", "warn", "info", "debug", "trace"].forEach(function (k) {
    if (!console[k]) console[k] = console.log;
  });

  // --- timers: registered, NEVER executed ---------------------------------
  // why: the bundle registers wall-clock timers at boot (jQuery ready,
  // Howler, percentShake during play). Under the browser harness those
  // fire on real time, but every timer consumer is sim-irrelevant BY
  // CONSTRUCTION of the harness patch: the gameTick re-arm is disabled
  // (__harnessMode), percentShake is unhashed AND draws from the native
  // RNG (CHECKSUM.md §7), and nothing else feeds the checksum surface.
  // Never running them is therefore the deterministic choice — no wall
  // clock exists here at all. The stream match against the browser golden
  // is the proof this reasoning holds.
  var timerId = 1;
  G.setTimeout = function () { return timerId++; };
  G.clearTimeout = function () {};
  G.setInterval = function () { return timerId++; };
  G.clearInterval = function () {};
  // why: renderTick() re-arms itself via rAF then returns early
  // (__harnessNoRender). Never invoking the callback parks the render
  // loop entirely — render reads sim state, never writes it (anatomy §3).
  G.requestAnimationFrame = function () { return timerId++; };
  G.cancelAnimationFrame = function () {};

  // --- virtual-clock substrate -------------------------------------------
  // why: oracle/harness/init.js overwrites performance.now with the
  // virtual clock; the object must exist first. Value never leaks into
  // the stream (proven in the determinism spike).
  G.performance = { now: function () { return 0; } };

  // --- events: accepted and dropped ---------------------------------------
  // why: boot registers listeners (gamepadconnected, fullscreenchange,
  // DOMContentLoaded, keyboard). None can ever fire headless: inputs come
  // exclusively through the __harnessInputs seam (harness patch). NOTE
  // DOMContentLoaded parity: in the browser harness the bundle evaluates
  // AFTER that event has fired, so src/main/loadscreen.js's listener
  // (which would re-fetch and re-execute main.js!) never runs there
  // either — never-firing is the browser-equal behavior, not a shortcut.
  function addEventListenerStub() {}
  function removeEventListenerStub() {}
  G.addEventListener = addEventListenerStub;
  G.removeEventListener = removeEventListenerStub;
  G.dispatchEvent = function () { return true; };

  // --- 2D canvas context: permissive no-op recorder ------------------------
  // why: start() grabs the five layer contexts and paints (bg1.fillRect);
  // menus/vfx call dozens of ctx methods. Canvas is WRITE-ONLY for the
  // game (render output); no sim state ever reads a canvas back. A
  // permissive Proxy (any method -> no-op, any property -> settable) is
  // the class-level stub for the whole 2D API surface. The two read-backs
  // that exist anywhere (measureText, getImageData) return fixed shapes.
  function makeContext2D() {
    var store = {};
    return new Proxy(function () {}, {
      get: function (t, k) {
        if (k in store) return store[k];
        if (k === "measureText") return function () { return { width: 0 }; };
        if (k === "getImageData") {
          return function (x, y, w, h) {
            return { width: w, height: h, data: new Uint8ClampedArray(w * h * 4) };
          };
        }
        if (k === "createLinearGradient" || k === "createRadialGradient") {
          return function () { return { addColorStop: function () {} }; };
        }
        if (k === "createPattern") return function () { return {}; };
        if (k === "canvas") return store.canvas;
        return function () {};
      },
      set: function (t, k, v) { store[k] = v; return true; },
    });
  }

  // --- minimal DOM nodes ----------------------------------------------------
  // why: jQuery 1.11.3 + Sizzle run feature detection at module-eval time
  // (createElement, innerHTML, getElementsByTagName, cloneNode, style
  // writes); main.js caches ~60 element handles (cacheDom) and start()
  // grabs the 5 canvas layers. Nodes are inert property bags: writes
  // stick, reads of never-written layout fields yield undefined and the
  // detection code takes its legacy fallback paths (all DOM-only chrome).
  var TEXT_STUB_TYPE = 3;
  function makeTextNode(text) {
    return { nodeType: TEXT_STUB_TYPE, nodeValue: String(text), textContent: String(text) };
  }
  function makeNode(tag) {
    var node = {
      nodeType: 1,
      nodeName: String(tag).toUpperCase(),
      tagName: String(tag).toUpperCase(),
      style: {},           // property bag; CSS is presentation-only here
      attributes: {},
      childNodes: [],
      className: "",
      id: "",
      innerHTML: "",
      value: "",
      checked: false,
      selected: false,
      disabled: false,
      width: 0,
      height: 0,
      ownerDocument: null, // filled after document exists
      parentNode: null,
      appendChild: function (c) {
        node.childNodes.push(c);
        if (c && typeof c === "object") c.parentNode = node;
        return c;
      },
      insertBefore: function (c) { return node.appendChild(c); },
      removeChild: function (c) {
        var i = node.childNodes.indexOf(c);
        if (i !== -1) node.childNodes.splice(i, 1);
        return c;
      },
      remove: function () {},
      setAttribute: function (k, v) { node.attributes[k] = String(v); },
      getAttribute: function (k) {
        return k in node.attributes ? node.attributes[k] : null;
      },
      removeAttribute: function (k) { delete node.attributes[k]; },
      addEventListener: addEventListenerStub,
      removeEventListener: removeEventListenerStub,
      attachEvent: undefined, // stay on the standards path in jQuery
      cloneNode: function () { return makeNode(tag); },
      getElementsByTagName: function (t) {
        // one fresh node: enough for Sizzle/jQuery.support probes that
        // index [0]; wrong-but-harmless for the .length feature flags
        // (they only steer DOM-manipulation quirks we never rely on)
        return [makeNode(t)];
      },
      getContext: function () {
        if (!node.__ctx) {
          node.__ctx = makeContext2D();
          node.__ctx.canvas = node;
        }
        return node.__ctx;
      },
      focus: function () {}, blur: function () {}, click: function () {},
      contains: function () { return false; },
    };
    Object.defineProperty(node, "firstChild", {
      get: function () {
        // jQuery.support reads div.firstChild.nodeType after an innerHTML
        // write we don't parse; a text stub keeps the probe crash-free
        return node.childNodes[0] || makeTextNode("");
      },
    });
    Object.defineProperty(node, "lastChild", {
      get: function () {
        return node.childNodes[node.childNodes.length - 1] || makeTextNode("");
      },
    });
    return node;
  }

  // --- document -------------------------------------------------------------
  // why each member: see inline notes; only members the bundle/boot
  // actually dereferences are present.
  var elementsById = Object.create(null); // stable identity per id (cacheDom)
  var cookieJar = Object.create(null);    // main.js get/setCookie (keyboard, targets)

  var documentElement = makeNode("html"); // Sizzle: isXML checks nodeName === "HTML"
  var body = makeNode("body");
  var head = makeNode("head");            // webpack style-loader appends <style>

  G.document = {
    nodeType: 9,
    documentElement: documentElement,
    body: body,
    head: head,
    readyState: "complete", // jQuery ready defers via setTimeout (never runs — see timers)
    title: "",
    // main.js:234-246 cookie helpers concatenate/split this as a string
    get cookie() {
      var parts = [];
      for (var k in cookieJar) parts.push(k + "=" + cookieJar[k]);
      return parts.join("; ");
    },
    set cookie(str) {
      // "name=value; expires=..." — store the name=value pair only
      var kv = String(str).split(";")[0];
      var eq = kv.indexOf("=");
      if (eq > 0) cookieJar[kv.slice(0, eq).trim()] = kv.slice(eq + 1);
    },
    createElement: function (tag) { return makeNode(tag); },
    createElementNS: function (ns, tag) { return makeNode(tag); }, // gamepad SVG helpers
    createTextNode: function (t) { return makeTextNode(t); },
    createComment: function () { return { nodeType: 8 }; },
    createDocumentFragment: function () { return makeNode("#document-fragment"); },
    getElementById: function (id) {
      // auto-vivify with stable identity: cacheDom() caches handles and
      // later code writes .innerHTML/.style at them (HUD chrome)
      if (!elementsById[id]) {
        elementsById[id] = makeNode("div");
        elementsById[id].id = id;
      }
      return elementsById[id];
    },
    getElementsByTagName: function (t) {
      t = String(t).toLowerCase();
      if (t === "body") return [body];
      if (t === "head") return [head];
      if (t === "html") return [documentElement];
      if (t === "*") return [documentElement, head, body];
      return [];
    },
    getElementsByClassName: function () { return []; },
    getElementsByName: function () { return []; },
    addEventListener: addEventListenerStub,
    removeEventListener: removeEventListenerStub,
    // main.js:309-310 assigns these handler slots directly
    onkeydown: null,
    onkeyup: null,
  };
  documentElement.ownerDocument = G.document;
  body.ownerDocument = G.document;
  head.ownerDocument = G.document;

  // --- navigator / location / screen ---------------------------------------
  // why: input.js polls navigator.getGamepads() every tick (returns the
  // no-controllers shape headless Chrome gives); jQuery + Howler sniff
  // userAgent; location mirrors the harness's serving origin.
  G.navigator = {
    userAgent: "qjs-oracle (meleelight-funkey-s M0)",
    // Howler 2.0.12's setupAudioContext unconditionally runs
    // navigator.appVersion.match(/OS (\d+)_.../) for its iOS sniff;
    // any non-iOS string keeps it on the desktop path (which then finds
    // no AudioContext/Audio here and settles on noAudio=true — the same
    // dead-audio endpoint the browser harness reaches by aborting every
    // sfx/music request).
    appVersion: "5.0 (qjs-oracle)",
    platform: "qjs",
    vendor: "",
    language: "en-US",
    getGamepads: function () { return [null, null, null, null]; },
  };
  G.location = {
    href: "http://localhost/dist/meleelight.html",
    origin: "http://localhost",
    protocol: "http:",
    host: "localhost",
    hostname: "localhost",
    port: "",
    pathname: "/dist/meleelight.html",
    search: "",
    hash: "",
  };
  G.screen = { width: 1280, height: 720, availWidth: 1280, availHeight: 720 };
  G.devicePixelRatio = 1;
  G.innerWidth = 1280;
  G.innerHeight = 720;

  // --- localStorage (in-memory) ---------------------------------------------
  // why: src/main/replay.js reads localStorage.getItem at module eval;
  // main.js cookie helpers can route through it. A fresh browser context
  // has an EMPTY store (getItem -> null) — an empty in-memory map is the
  // browser-equal state, not an approximation.
  // why Storage: main.js:220 feature-detects Web Storage with
  // `typeof(Storage) !== "undefined"` (the DOM interface object, not the
  // instance). Without it, main.js takes its no-storage path where
  // getCookie() returns "" instead of localStorage's null — and
  // getGameplayCookies() treats "" as a stored value, coercing EVERY
  // gameSettings entry to Number("") = 0 (found as the frame-403
  // phantomThreshold 0.01->0 stream divergence). Defining the interface
  // keeps qjs on the exact code path the browser oracle takes.
  G.Storage = function Storage() {};
  var lsStore = Object.create(null);
  G.localStorage = {
    getItem: function (k) { return k in lsStore ? lsStore[k] : null; },
    setItem: function (k, v) { lsStore[k] = String(v); },
    removeItem: function (k) { delete lsStore[k]; },
    clear: function () { lsStore = Object.create(null); },
    key: function (i) { return Object.keys(lsStore)[i] || null; },
    get length() { return Object.keys(lsStore).length; },
  };
  G.sessionStorage = G.localStorage;

  // --- Image ------------------------------------------------------------------
  // why: stagerender/menus create Image()s and set .src at module eval.
  // onload never fires (no network); images are drawImage'd in render
  // only — the browser harness aborts/ignores them equivalently.
  G.Image = function Image() {
    var img = makeNode("img");
    img.complete = false;
    return img;
  };

  // --- Audio -------------------------------------------------------------------
  // why: Howler 2.0.12 detects no WebAudio here (no AudioContext) and
  // falls back to HTML5 audio, constructing `new Audio()` lazily inside
  // Sound.create() at first play() (startGame plays the stage music).
  // This stub is an inert media element: canPlayType() -> "" means no
  // codec is ever supported, so every Howl ends in loaderror and queued
  // plays never fire — the SAME dead-audio endpoint the browser harness
  // reaches by aborting every /sfx//music/ request (run.js:115). Audio is
  // sim-irrelevant: KO-shout/vfx RNG draws happen in the sim step before
  // any sound would play, regardless of load state.
  G.Audio = function Audio() {
    return {
      canPlayType: function () { return ""; },
      addEventListener: addEventListenerStub,
      removeEventListener: removeEventListenerStub,
      load: function () {},
      play: function () {},
      pause: function () {},
      muted: false,
      volume: 1,
      preload: "auto",
      src: "",
      currentTime: 0,
      duration: NaN,
      paused: true,
    };
  };

  // --- XMLHttpRequest ----------------------------------------------------------
  // why: jQuery's ajax transport probes `new window.XMLHttpRequest()` at
  // module eval. Instantiable, but send() completes nothing: the only
  // in-repo XHR user is loadscreen.js whose DOMContentLoaded path is dead
  // in the harness too (see events note above).
  G.XMLHttpRequest = function XMLHttpRequest() {
    this.readyState = 0;
    this.status = 0;
    this.responseText = "";
    this.withCredentials = false;
  };
  G.XMLHttpRequest.prototype.open = function () {};
  G.XMLHttpRequest.prototype.send = function () {};
  G.XMLHttpRequest.prototype.setRequestHeader = function () {};
  G.XMLHttpRequest.prototype.addEventListener = addEventListenerStub;
  G.XMLHttpRequest.prototype.abort = function () {};
  G.XMLHttpRequest.prototype.getResponseHeader = function () { return null; };

  // --- misc window chrome -------------------------------------------------------
  // why: menus call alert/confirm on some click paths (never clicked, but
  // module-level references must not throw); getComputedStyle backs
  // jQuery .css() reads on our inert nodes.
  G.alert = function () {};
  G.confirm = function () { return false; };
  G.prompt = function () { return null; };
  G.getComputedStyle = function (el) {
    return {
      getPropertyValue: function () { return ""; },
      display: "", width: "0px", height: "0px",
    };
  };
  G.matchMedia = function () {
    return { matches: false, addListener: function () {}, removeListener: function () {} };
  };
  G.open = function () { return null; };
  G.focus = function () {};
  G.blur = function () {};
  G.scrollTo = function () {};

  // --- TextEncoder + crypto.subtle: the hash seam ---------------------------
  // why: pagelib.js (reused VERBATIM from the browser harness so the
  // serialization contract has one implementation) hashes via
  // `crypto.subtle.digest("SHA-256", te.encode(s))`. QuickJS has no
  // WebCrypto; __qjs_sha256 is the embedder's C SHA-256 (NIST-self-tested at
  // startup, digest bytes == WebCrypto by construction of SHA-256).
  // encode() is WHATWG TextEncoder: UTF-8, lone surrogates -> U+FFFD.
  G.TextEncoder = function TextEncoder() {};
  G.TextEncoder.prototype.encode = function (str) {
    str = String(str);
    var out = [];
    for (var i = 0; i < str.length; i++) {
      var c = str.charCodeAt(i);
      if (c >= 0xd800 && c <= 0xdbff) {
        var lo = i + 1 < str.length ? str.charCodeAt(i + 1) : 0;
        if (lo >= 0xdc00 && lo <= 0xdfff) {
          c = 0x10000 + ((c - 0xd800) << 10) + (lo - 0xdc00);
          i++;
        } else {
          c = 0xfffd; // unpaired high surrogate
        }
      } else if (c >= 0xdc00 && c <= 0xdfff) {
        c = 0xfffd;   // unpaired low surrogate
      }
      if (c < 0x80) {
        out.push(c);
      } else if (c < 0x800) {
        out.push(0xc0 | (c >> 6), 0x80 | (c & 0x3f));
      } else if (c < 0x10000) {
        out.push(0xe0 | (c >> 12), 0x80 | ((c >> 6) & 0x3f), 0x80 | (c & 0x3f));
      } else {
        out.push(0xf0 | (c >> 18), 0x80 | ((c >> 12) & 0x3f),
                 0x80 | ((c >> 6) & 0x3f), 0x80 | (c & 0x3f));
      }
    }
    return new Uint8Array(out);
  };
  G.crypto = {
    subtle: {
      digest: function (alg, data) {
        if (alg !== "SHA-256") {
          return Promise.reject(new Error("qjs shim: only SHA-256 is provided"));
        }
        try {
          return Promise.resolve(G.__qjs_sha256(data));
        } catch (e) {
          return Promise.reject(e);
        }
      },
    },
  };
})();
