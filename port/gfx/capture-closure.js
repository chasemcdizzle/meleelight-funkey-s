// port/gfx/capture-closure.js — the capture-INPUT CLOSURE (iter 48,
// review-46 round-2 fix 2): the ONE mechanical enumeration of every
// static repo file the browser render-reference capture loads or reads.
//
// capture-canvas.js resolves its page-init-script / config / trace load
// paths FROM this map (so a new input cannot be loaded without joining
// the closure), records sha256 of EVERY member in the reuse sidecar
// (capture.digests.json), and check-render.sh's MLFK_GFX_REUSE_CANVAS
// path re-derives the same map and REFUSES loudly on any drift, missing
// member, or extra member. CLASS: bind the input CLOSURE, not a
// hand-picked subset — the read-side twin of iter-42's mechanical
// write-site enumeration (content pins prove CONTENT; the closure pin
// proves the cached capture was produced by the CURRENT capture path).
//
// Bootstrap note: oracle/goldens/manifest.json is read by
// capture-canvas.js BEFORE this map can be built (it parameterizes the
// map via the golden's trace filename), so that one read cannot route
// through the map — it is still a closure member and sidecar-bound.
//
// NOT in the closure (each covered elsewhere; keep this list in sync):
//   - the served upstream dist: identity-pinned by servedDistSha256
//     (expected-render.json), asserted against the cached run JSON in
//     reuse mode too (check-render.sh);
//   - the cached run JSON itself: STREAM MATCH-judged against the
//     frozen golden on EVERY run, reuse included;
//   - node / playwright / Chrome: environment, not repo inputs — browser
//     name+version are recorded in the run-JSON meta, and behavioral
//     drift surfaces as a STREAM MATCH or IoU failure, never silently;
//   - the GFXDATA file the capture WROTE: an OUTPUT consumed by later
//     stages, bound separately in the sidecar (key "gfxdata").
"use strict";

const path = require("path");

const REPO = path.resolve(__dirname, "..", "..");

// goldenTrace: the manifest `trace` filename of the captured golden
// (the trace file is a capture input, resolved per golden).
function closureFiles(goldenTrace) {
  if (typeof goldenTrace !== "string" || goldenTrace.length === 0 ||
      goldenTrace.includes("/") || goldenTrace.includes("..")) {
    throw new Error("capture-closure: goldenTrace must be a bare manifest trace filename");
  }
  return {
    // this enumeration itself (editing the closure is itself drift)
    "port/gfx/capture-closure.js": path.join(__dirname, "capture-closure.js"),
    // the capture driver
    "port/gfx/capture-canvas.js": path.join(__dirname, "capture-canvas.js"),
    // page init scripts, in injection order (capture-canvas.js loads
    // these paths FROM this map)
    "port/fdlibm/fdlibm.js": path.join(REPO, "port", "fdlibm", "fdlibm.js"),
    "oracle/harness/init.js": path.join(REPO, "oracle", "harness", "init.js"),
    "oracle/harness/pagelib.js": path.join(REPO, "oracle", "harness", "pagelib.js"),
    "port/gfx/gfx-pagelib.js": path.join(__dirname, "gfx-pagelib.js"),
    // config the capture reads: console allowlist + frozen pins
    "port/gfx/expected-render.json": path.join(__dirname, "expected-render.json"),
    // golden parameterization + the input trace
    "oracle/goldens/manifest.json": path.join(REPO, "oracle", "goldens", "manifest.json"),
    ["oracle/goldens/" + goldenTrace]: path.join(REPO, "oracle", "goldens", goldenTrace),
  };
}

module.exports = { closureFiles };
