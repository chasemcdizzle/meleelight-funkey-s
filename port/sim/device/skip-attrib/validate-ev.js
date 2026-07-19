#!/usr/bin/env node
// port/sim/device/skip-attrib/validate-ev.js — M4 task 8 hardening
// (iter 76, review-73 M): consumer-side WHITELIST validation of the
// correlator's EV records. Deliberately a SEPARATE tool from
// correlate-skips.js — the correlator PRODUCES the lines; validating
// them inside the producer would be self-referential. This runs in
// check-skip-attrib.sh (and in teeth, directly on perturbed copies).
//
// Usage:
//   node validate-ev.js --corr correlate-out.txt --frames N
//     --arm sampler|nosampler --skips N --events N
//
// GRAMMAR (PROCESS §3 whitelist rule — measured from the REAL corpus:
// the committed iter-74 A1-A5 harvest + live correlate-out.txt, 37 EV
// lines re-validated with zero false rejections before shipping):
//   EV|frame=<1-7d>|skipped=<0|1>|sim_ns=<d>|render_ns=<d>
//     |present_ns=<d>|full_ns=<d>|late_start_ns=<d>|d_nivcsw=<d>
//     |d_nvcsw=<d>|d_minflt=<d>|d_majflt=<d>
//   then EITHER the full sampler-window group:
//     |win_ns=<d>|d_ctxt=<d>|d_procs=<d>
//     |cpu{user=<d>,nice=<d>,system=<d>,idle=<d>,iowait=<d>,irq=<d>,
//          softirq=<d>,steal=<d>}
//     |irq{<label>=<d>(,<label>=<d>)*}   (label: [A-Za-z0-9]+ with an
//                                          optional :desc of
//                                          [A-Za-z0-9_.-]+ — kernel
//                                          irq names, spaces already
//                                          collapsed to _)
//     |sirq{<[A-Z_]+>=<d>(,...)*}
//   OR the marker |win=none  OR (nosampler arm only) NEITHER.
//   All <d> are bounded 1-12 digit decimals. Binary outcome per line:
//   exact match -> parsed; resembles-but-doesn't-match -> corruption
//   -> exit 3.
//
// RULES enforced beyond per-line shape:
//   - every line starting with 'EV' must match the full grammar;
//   - frame values strictly increasing (duplicates/reordering =
//     corruption) and within [1, --frames];
//   - EV total == --events; skipped=1 EV count == --skips (the
//     timing-file-derived key — the reconciliation chain);
//   - arm=sampler: EVERY event must carry the full kernel window;
//     win=none = the sampler failed to bracket the phenomenon =
//     evidence incomplete = FAIL CLOSED (rerunning the check IS the
//     re-sample; an unwitnessed skip can never count as attributed);
//   - arm=nosampler: no window fields may appear at all.
"use strict";
const fs = require("fs");

function die(msg) {
  console.error("validate-ev: " + msg);
  process.exit(3);
}

const args = process.argv.slice(2);
const opt = {};
for (let i = 0; i < args.length; i++) {
  const a = args[i];
  const need = () => {
    if (i + 1 >= args.length) die("missing value for " + a);
    return args[++i];
  };
  if (a === "--corr") opt.corr = need();
  else if (a === "--frames") opt.frames = need();
  else if (a === "--arm") opt.arm = need();
  else if (a === "--skips") opt.skips = need();
  else if (a === "--events") opt.events = need();
  else die("bad argument " + a);
}
for (const k of ["corr", "frames", "arm", "skips", "events"]) {
  if (!opt[k]) die("required argument --" + k + " missing");
}
if (!/^[0-9]{1,7}$/.test(opt.frames)) die("--frames not a bounded decimal");
if (!/^[0-9]{1,7}$/.test(opt.skips)) die("--skips not a bounded decimal");
if (!/^[0-9]{1,7}$/.test(opt.events)) die("--events not a bounded decimal");
if (opt.arm !== "sampler" && opt.arm !== "nosampler") die("--arm must be sampler|nosampler");
const FRAMES = parseInt(opt.frames, 10);
const WANT_SKIPS = parseInt(opt.skips, 10);
const WANT_EVENTS = parseInt(opt.events, 10);

const NUM = "[0-9]{1,12}";
const IRQE = "[A-Za-z0-9]+(?::[A-Za-z0-9_.-]+)?=" + NUM;
const SIRQE = "[A-Z_]+=" + NUM;
const WIN_GROUP =
  "\\|win_ns=" + NUM + "\\|d_ctxt=" + NUM + "\\|d_procs=" + NUM +
  "\\|cpu\\{user=" + NUM + ",nice=" + NUM + ",system=" + NUM +
  ",idle=" + NUM + ",iowait=" + NUM + ",irq=" + NUM +
  ",softirq=" + NUM + ",steal=" + NUM + "\\}" +
  "\\|irq\\{(?:" + IRQE + "(?:," + IRQE + ")*)?\\}" +
  "\\|sirq\\{(?:" + SIRQE + "(?:," + SIRQE + ")*)?\\}";
const EV_RE = new RegExp(
  "^EV\\|frame=([0-9]{1,7})\\|skipped=([01])\\|sim_ns=" + NUM +
  "\\|render_ns=" + NUM + "\\|present_ns=" + NUM + "\\|full_ns=" + NUM +
  "\\|late_start_ns=" + NUM + "\\|d_nivcsw=" + NUM + "\\|d_nvcsw=" + NUM +
  "\\|d_minflt=" + NUM + "\\|d_majflt=" + NUM +
  "((" + WIN_GROUP + ")|(\\|win=none))?$"
);

let raw;
try {
  raw = fs.readFileSync(opt.corr, "utf8");
} catch (e) {
  die("cannot read " + opt.corr + ": " + e.message);
}
if (raw.length === 0) die("correlator output is empty");
if (raw[raw.length - 1] !== "\n") die("correlator output does not end with a newline");
const lines = raw.split("\n");
lines.pop();

let total = 0, skipEvs = 0, lastFrame = 0;
for (let i = 0; i < lines.length; i++) {
  const line = lines[i];
  if (!line.startsWith("EV")) continue; // resemblance net: ANY 'EV' prefix
  const m = EV_RE.exec(line);
  if (!m) die("line " + (i + 1) + " resembles an EV record but fails the full grammar (corruption): " + JSON.stringify(line));
  total++;
  const frame = parseInt(m[1], 10);
  const skipped = m[2] === "1";
  // group map: 1=frame 2=skipped 3=outer optional tail 4=window-group
  // alternative 5=win=none alternative (WIN_GROUP itself is built from
  // non-capturing groups only)
  const hasWin = m[4] !== undefined;   // full window group
  const hasNone = m[5] !== undefined;  // |win=none marker
  if (frame < 1 || frame > FRAMES) die("EV frame " + frame + " outside [1," + FRAMES + "]");
  if (frame <= lastFrame) die("EV frames not strictly increasing at frame " + frame + " (duplicate/reordered records = corruption)");
  lastFrame = frame;
  if (opt.arm === "sampler") {
    if (hasNone || !hasWin) {
      die("EV frame " + frame + (skipped ? " (a SKIP)" : "") +
        " carries no bracketing kernel window under the sampler arm — " +
        "evidence incomplete (win=none may not count toward attribution; rerun = re-sample)");
    }
  } else {
    if (hasWin || hasNone) die("EV frame " + frame + " carries sampler-window fields under the nosampler arm — evidence/arm mismatch");
  }
  if (skipped) skipEvs++;
}
if (total !== WANT_EVENTS) die("EV record total " + total + " != events key " + WANT_EVENTS);
if (skipEvs !== WANT_SKIPS) die("skipped=1 EV count " + skipEvs + " != timing-derived skips " + WANT_SKIPS + " (skip reconciliation failed)");

console.log("EV VALID (events=" + total + ", skip_evs=" + skipEvs + ", arm=" + opt.arm + ")");
