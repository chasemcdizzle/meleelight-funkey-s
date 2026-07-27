#!/usr/bin/env node
// port/gfx/judge-render-timing.js — HOST-side judge of gfx_app's
// --timing artifact (M3 task 4). The device only RECORDS (gfx_app.c
// buffers per-frame CLOCK_MONOTONIC ns in RAM and writes them post-run
// to tmpfs); all judgment happens here on the host, per the M3
// convention. percentiles.js's strict-grammar / nearest-rank precedent,
// extended to the 4-column render rows.
//
// Usage: node judge-render-timing.js <timing.txt> <expectedFrames>
//
// Grammar (HARD-FAILED, exit 3, on any violation — a truncated pull, a
// partial device write, or garbage bytes must never read as numbers):
//   exactly <expectedFrames> lines, each
//     ^<sim> <render> <present> <skipped>$
//   with sim/render/present CANONICAL decimal ns (no leading zeros — the
//   recorders emit PRIu64, foh_dev.c:2236-2237 and gfx_app.c:971-972, so
//   "007" resembles-but-does-not-match the producer's grammar; PROCESS §3)
//   and skipped 0|1; a skipped row
//   MUST carry render == 0 and present == 0 (the app skips BOTH), and a
//   rendered row MUST carry render > 0 (a zero-cost render is a recording
//   bug, not a measurement).
//
// MEASUREMENT PLAUSIBILITY (iter 117; review-116-jrt-2o [M] then
// review-117-jrt-1 [M], which showed the first, zero-only, fix did not
// close the class). The judge's declared threat model is a RECORDER
// REGRESSION, and the gated number is `full`, dominated by `sim`. All of
// these were demonstrated to pass with rc 0 and judge_complete=1 against
// a real 13.903 ms-p99 artifact:
//   sim zeroed              -> 7.147 ms    (zero-only arm closes it)
//   sim collapsed to a small POSITIVE constant  (needs a floor)
//   every column shifted 1000x (ns read as us) -> 13.901 us
//   the fastest row repeated over the whole file -> 6.443 ms
//   the present column zeroed except one surviving row -> 12.807 ms
// Zero is only the most obvious value a broken bracket takes. What
// separates a genuine recording from all of the above is not any single
// row but the SHAPE of the population, so the arms below are the shape
// invariants the measured corpus supports. Corpus: the 80 distinct
// grammar-valid timing artifacts in-tree, 267,120 rows (distinct by exact
// content; the enumeration and per-artifact numbers are regenerable —
// see .loop/review-117-jrt-regression.log). Of those 80, exactly TWO are
// SYNTHETIC tooth fixtures the rig fabricates to be REJECTED —
// port/foh/build/device-fullgame/teeth/t3.tim.txt and teeth/t4.tim.txt,
// written by check-device-fullgame.sh:2688 and :2697 — so every constant
// below is set off the CONSTANT-SETTING corpus of 78 GENUINE artifacts /
// 259,920 rows (review-117-jrt-6o [M]3; the split is enumerated in
// .loop/review-117-jrt-regression.log [1]). Where a genuine-only figure
// differs from the all-80 figure it is called out at the constant.
//
//   SIM_FLOOR_NS   per row, EVERY row (skipped frames still tick the
//                  sim). Corpus min sim = 27,000 ns; the floor is 10,000,
//                  a 2.7x margin below anything ever measured, and a game
//                  tick physically cannot run in 10 us on a 1 GHz A7.
//                  Catches zeroing, positive collapse, and the 1000x unit
//                  shift (3,775 ns) in one arm.
//   TIMED_FLOOR_NS the same, for render and for a NONZERO present. The
//                  floor is 500 ns and catches "collapse every
//                  render/present bracket to 1". CORPUS PROVENANCE
//                  (review-117-jrt-6o [M]3): the 1,000 ns figure this was
//                  set under comes from the GENUINE minimum nonzero
//                  PRESENT — but the matching render figure did NOT: it
//                  came from teeth/t4.tim.txt, a SYNTHETIC artifact
//                  check-device-fullgame.sh:2697 generates to test its own
//                  p99 assertion, which writes literal `1000 1000` rows.
//                  Genuine-only minimum render is 29,000 ns. The floor
//                  STAYS at 500 rather than rising to exploit that: the
//                  rig's own T4 tooth must reach the p99 assertion and
//                  fail THERE with its pinned diagnostic, so a render
//                  floor above 1,000 would break the engine's self-test.
//                  A deliberately loose constant with a stated reason.
//   MIN_PRESENT_POS_FRAC   of RENDERED rows, present > 0. Corpus min
//                  5.78%; threshold 1%, a 5.8x margin. A per-row present
//                  guard is impossible — 127,237 genuine rendered rows
//                  carry present == 0 because a sub-tick present is real
//                  and common — but a column that is zero except for a
//                  token survivor is not a measurement.
//   MIN_DISTINCT_*_FRAC   distinct values / rows. Corpus minima 1.39%
//                  (sim), 2.44% (frame totals) and 11.67% (whole ROWS)
//                  — identical on the genuine-only corpus;
//                  thresholds 0.25% / 1% / 2%, margins 5.5x, 2.4x, 5.8x.
//                  The ROW arm is the one that bounds a stuck or
//                  ring-buffered recorder index: replaying a genuine
//                  18-frame window over 3600 rows leaves every per-column
//                  statistic looking plausible.
//   EXACT SELF-REPEAT   no threshold at all: no genuine artifact in the
//                  corpus repeats with ANY period (row[i] == row[i-p] for
//                  all i >= p, p <= n/2). A recording that does is a
//                  replayed buffer.
// The population arms need a population: they apply only at
// POP_MIN_ROWS = 600 rows or more (the corpus holds genuine artifacts as
// small as 120 rows with a single rendered frame, and a statistic over
// one frame is not a statistic). Every gate leg judges 3600 frames with
// --frames pinned by its caller, so the arms are always live where gate
// evidence is produced, and a short artifact fails the line-count check
// long before it reaches them.
//
// KNOWN RESIDUAL — WHAT THIS JUDGE STRUCTURALLY CANNOT DECIDE
// (review-117-jrt-2 [M], review-117-jrt-3 [M]x2; ESCALATED TO THE DRIVER,
// not closed here, and deliberately not papered over with more statistics).
//
// The arms above reject every recorder regression that leaves a FOOTPRINT
// in these four columns: a stopped or unopened bracket, a unit shift, a
// collapsed column, a replayed buffer, a token-survivor column. They
// cannot reject a recorder that lies SELF-CONSISTENTLY:
//   (i)  a monotone RESCALING preserves ordering, variety, periodicity and
//        every within-column ratio — it is a faithful recording of a
//        different clock. This covers a rescale of ANY NONEMPTY SUBSET of
//        the three columns, independently per column — from a uniform
//        all-three, through any two-column pair, down to a SINGLE column
//        (review-117-jrt-6o [M]2 and review-117-jrt-7o [L]4, which
//        showed the narrower wording was a declaration gap): sim/2 ->
//        9.843 ms, sim/4 -> 8.297, sim/30 -> 7.059, render/2 -> 10.706
//        against a truthful 13.250, surviving on 74-76 of 78 corpus
//        artifacts. A single-column rescale is the MORE plausible
//        regression of the two — one edit at foh_dev.c:2181 moving the
//        bracket's open past sim_game_tick does it — and it is undecidable
//        for the measured reason that genuine sim_p50 runs as low as
//        29,000 ns and the genuine sim SHARE of a frame spans 0.144-0.690,
//        so no floor or ratio bound separates it from a fast run. Only
//        LARGE factors are caught, by SIM_FLOOR_NS (sim/30 of a 27,000 ns
//        row lands at 900);
//   (ii) a PARTIAL collapse tuned to sit inside these thresholds — sim
//        replaced on 399 of every 400 rows, leaving just enough survivors
//        for the variety arms — still lowers the reported p99 materially.
//        (The sibling case, every render and present set to the genuine
//        1,000 ns minimum, STOPPED being a residual once the column-alias
//        arm landed: it makes render and present the same sequence. It is
//        a tooth now, not a residual — review-117-jrt-5 [L].
//        CORRECTION, review-117-jrt-8o [M]1: for one round that sentence
//        overstated the closure. The alias arm keys on the two columns
//        being EQUAL, so the UNEQUAL neighbour — render:=1000 while
//        present:=2000, or render collapsed alone — walked straight past
//        it at rc 0. What actually closes the whole collapse family is the
//        render-column VARIETY arm (MIN_DISTINCT_RENDER_FRAC, added in
//        review-117-jrt-8), because a collapsed column stops varying
//        whatever value it collapses to. Both are teeth now.)
// Both are demonstrated against a real artifact, and both are asserted as
// RESIDUAL lines in .loop/review-117-jrt-regression.log so they can never
// quietly become a claim. Neither is closable by any function of these
// four columns, and not for want of an idea: every further statistic
// (modal frequency was measured for exactly this) either false-rejects
// genuine artifacts — the corpus holds real 3,600-row recordings whose
// every nonzero present is the SAME 1,000 ns, modal frequency 1.0 — or is
// defeated by tuning the mutation one step further. Thin-margin arms would
// trade a real risk of rejecting genuine device evidence for no bound.
//
// AND THE OBVIOUS INDEPENDENT WITNESS IS NOT INDEPENDENT. An earlier
// version of this comment argued that `skips == 0`, which every M4
// consumer asserts, bounds case (i) because the pace governor decides
// skips against the wall deadline. review-117-jrt-3 REFUTED that by
// reading the producer: the deadline (port/foh/foh_dev.c:2156), the skip
// decision (:2185) and these recorded buckets (:2196) all read the SAME
// now_ns() clock, so a clock-rate regression doubles the effective
// deadline while halving the recorded work and no skip need occur;
// `matchWallMs` (:2211) shares it too. The claim is WITHDRAWN, not
// defended.
//
// What would close case (i) is a scale witness on a DIFFERENT clock, and
// it has to be PER COLUMN to cover the single-column member — a
// recorder-side measurement around a known-duration wait, or a host-side
// wall-clock bound on the paced run. Both are producer/rig changes outside
// this file's sanctioned fix set, so they are registered for the driver
// rather than improvised here. Note that such a witness would NOT close
// case (ii), nor the column-alias class above: those are content defects,
// not scale defects, and each has to be met on its own footprint.
// Populations (pre-registered, AGENT-LOG iter 50):
//   full   = sim+render+present over ALL frames (work time; pacing sleep
//            is excluded at the recorder);
//   render = render ns over RENDERED frames only (a skipped frame has no
//            render cost to measure; the skip COUNT is reported so skips
//            can never hide);
//   sim / present likewise reported for the attribution split.
// Percentiles: NEAREST-RANK on the ascending sort (idx = ceil(q*n)-1).
// Output: strict key=value lines for the no-eval shell parser.
"use strict";

const fs = require("fs");

function die(msg) {
  console.error("judge-render-timing: " + msg);
  process.exit(3);
}

const [file, framesArg] = process.argv.slice(2);
if (!file || !framesArg) {
  console.error("usage: node judge-render-timing.js <timing.txt> <expectedFrames>");
  process.exit(1);
}
if (!/^(0|[1-9][0-9]*)$/.test(framesArg)) die("expectedFrames not a canonical decimal integer: " + framesArg);
const expected = parseInt(framesArg, 10);
if (expected <= 0) die("expectedFrames must be positive");
if (!Number.isSafeInteger(expected)) die("expectedFrames exceeds the safe integer range: " + framesArg);

const raw = fs.readFileSync(file, "utf8");
if (raw.length === 0) die("timing file is empty");
if (raw[raw.length - 1] !== "\n") die("timing file does not end with a newline");
const lines = raw.split("\n");
lines.pop();
if (lines.length !== expected) {
  die("timing file has " + lines.length + " lines, expected " + expected);
}

// Measured-then-frozen plausibility constants — see the header block.
const SIM_FLOOR_NS = 10000;
const TIMED_FLOOR_NS = 500;
const POP_MIN_ROWS = 600;
const MIN_PRESENT_POS_FRAC = 0.01;
const MIN_DISTINCT_SIM_FRAC = 0.0025;
const MIN_DISTINCT_RENDER_FRAC = 0.005;
const MIN_DISTINCT_FULL_FRAC = 0.01;
const MIN_DISTINCT_ROW_FRAC = 0.02;
const MAX_COL_ALIAS_FRAC = 0.5;

const full = new Array(expected);
const sim = new Array(expected);
const present = [];
const render = [];
let presentPos = 0;
const distinctSim = new Set();
const distinctRender = new Set();
const distinctFull = new Set();
const distinctRow = new Set();
let skips = 0;
const ROW = /^(0|[1-9][0-9]*) (0|[1-9][0-9]*) (0|[1-9][0-9]*) ([01])$/;
for (let i = 0; i < expected; i++) {
  const m = ROW.exec(lines[i]);
  if (!m) die("line " + (i + 1) + " malformed: " + JSON.stringify(lines[i]));
  const s = parseInt(m[1], 10), r = parseInt(m[2], 10), p = parseInt(m[3], 10);
  if (!Number.isSafeInteger(s) || !Number.isSafeInteger(r) || !Number.isSafeInteger(p)) {
    die("line " + (i + 1) + " exceeds the safe integer range");
  }
  // The sim bracket is opened on EVERY frame (skipped frames still tick
  // the sim); below the floor is a stopped clock, an unopened bracket or
  // a unit shift, not a measurement — and it is the term that would
  // silently halve `full`.
  if (s < SIM_FLOOR_NS) {
    die("line " + (i + 1) + " has sim " + s + " ns, below the " + SIM_FLOOR_NS +
      " ns plausibility floor (recording bug — a game tick cannot run this fast)");
  }
  const skipped = m[4] === "1";
  if (skipped) {
    if (r !== 0 || p !== 0) die("line " + (i + 1) + " skipped but render/present nonzero");
    skips++;
  } else {
    if (r === 0) die("line " + (i + 1) + " rendered but render == 0 (recording bug)");
    // Same floor logic as sim, at the granularity these two columns
    // actually reach: the corpus minimum for render, and for a NONZERO
    // present, is 1,000 ns in both cases (the finest tick any recording
    // in-tree resolves). Anything below TIMED_FLOOR_NS is a collapsed
    // bracket, not a fast one — this is what "collapse every render and
    // present bracket to 1" trips.
    if (r < TIMED_FLOOR_NS) {
      die("line " + (i + 1) + " has render " + r + " ns, below the " + TIMED_FLOOR_NS +
        " ns plausibility floor (recording bug — a collapsed bracket)");
    }
    if (p > 0 && p < TIMED_FLOOR_NS) {
      die("line " + (i + 1) + " has present " + p + " ns, below the " + TIMED_FLOOR_NS +
        " ns plausibility floor (recording bug — a collapsed bracket)");
    }
    render.push(r);
    distinctRender.add(r);
    present.push(p);
    if (p > 0) presentPos++;
  }
  sim[i] = s;
  distinctSim.add(s);
  distinctRow.add(lines[i]);
  // Each addend is individually safe; their SUM need not be. An inexact
  // total must never be emitted under judge_complete=1 (review-116-jrt-1
  // [M]) — the three-term result is checked, not assumed.
  const t = s + r + p;
  if (!Number.isSafeInteger(t)) {
    die("line " + (i + 1) + " sim+render+present exceeds the safe integer range");
  }
  full[i] = t;
  distinctFull.add(t);
}
if (render.length + skips !== expected) die("population bookkeeping error");
if (render.length === 0) die("every frame was skipped — nothing was rendered (a live render run cannot pass this way)");
// Population shape invariants — see the header block. Gated on a real
// population so genuine short diagnostic artifacts are not false-rejected.
if (expected >= POP_MIN_ROWS) {
  if (distinctSim.size < expected * MIN_DISTINCT_SIM_FRAC) {
    die("the sim column carries only " + distinctSim.size + " distinct values over " +
      expected + " rows (recording bug — a repeated, stuck or substituted row " +
      "population, not a measurement)");
  }
  // RENDER-COLUMN VARIETY (review-117-jrt-8 [M] and review-117-jrt-8o [M]1,
  // found independently, REPRODUCED). The sim column has had a variety arm
  // since the first plausibility pass; render had NONE, and that asymmetry
  // was the whole hole. The alias arm added in [R8] only catches a render
  // collapse that lands on the SAME value as present; make the two constants
  // UNEQUAL and every arm passed. Measured against a truthful 13.250 ms
  // artifact, all rc 0 with judge_complete=1:
  //     render:=1000, present:=2000, every rendered row   -> 7.631 ms
  //     the same on 399 of every 400 rendered rows        -> 7.750 ms
  //     render:=1000 only, present untouched              -> 8.714 ms
  //     render:=29000 only (the genuine minimum)          -> 8.742 ms
  // No floor can catch these — 29,000 ns IS the genuine minimum render. What
  // separates them from a recording is that the column stops VARYING, which
  // is the same footprint the sim arm keys on. Genuine corpus minimum render
  // variety is 2.4722% (89 distinct over 3,600 rendered rows,
  // port/gfx/build/mixerfid/s01.app-tim.txt); the threshold is 0.5%, a 4.9x
  // margin below anything ever measured, and it rejects every mutation above
  // (a fully collapsed column is 1 distinct value = 0.0278%).
  if (distinctRender.size < render.length * MIN_DISTINCT_RENDER_FRAC) {
    die("the render column carries only " + distinctRender.size +
      " distinct values over " + render.length + " rendered rows (recording " +
      "bug — a collapsed or substituted column, not a measurement)");
  }
  if (distinctFull.size < expected * MIN_DISTINCT_FULL_FRAC) {
    die("the frame totals carry only " + distinctFull.size + " distinct values over " +
      expected + " rows (recording bug — a repeated, stuck or substituted row " +
      "population, not a measurement)");
  }
  // Whole-ROW variety, the invariant that actually bounds a stuck or
  // ring-buffered recorder index: replaying a genuine 18-frame window over
  // all 3600 rows leaves the per-column counts looking plausible while the
  // row population is 18 rows repeated 200 times. Corpus minimum is
  // 11.67% distinct rows; the threshold is 2%, a 5.8x margin below
  // anything ever measured and 4x above the demonstrated attack.
  if (distinctRow.size < expected * MIN_DISTINCT_ROW_FRAC) {
    die("only " + distinctRow.size + " distinct rows over " + expected +
      " frames (recording bug — a stuck or ring-buffered recorder index " +
      "replays a short window; a genuine run never repeats this much)");
  }
  // COLUMN ALIASING (review-117-jrt-4 [M], demonstrated): assigning
  // tim[f].sim the ADJACENT render expression instead of its own
  // (foh_dev.c:2196 is one edit away from it) leaves the sim column fully
  // varied — 3,495 distinct values, 3,600 distinct rows, every arm above
  // satisfied — while dropping the reported p99 from 15.093 ms to
  // 13.608 ms, enough to certify a run whose truthful p99 is 17.093 ms.
  // A different-clock witness would not catch it either. But it has an
  // exact footprint: the two columns become the SAME sequence. Measured
  // over the corpus, no artifact aliases any pair, and the highest
  // per-row equality is sim==render 4.25%, sim==present 0%, and
  // render==present 50%. CORPUS PROVENANCE (review-117-jrt-6o [M]3): that
  // 50% is NOT genuine — it came from teeth/t4.tim.txt, the synthetic
  // p99-inflation fixture, whose alternating rows USED to read
  // "21670000 1000 1000 0" and so made render == present by construction.
  // That generator was changed in review-117-jrt-8 to emit unequal values
  // ("21670000 1000 2000 0"), precisely so the fixture stops being the only
  // artifact in the corpus with a nonzero rate: regenerated, t4 measures
  // 0.0000% and still trips its p99 assert at 21.673 ms. Over the 78 GENUINE
  // artifacts
  // the render==present rate is 0.0000%: not one genuine in-tree row has
  // render equal to present. This pair therefore now carries the SAME
  // fractional bound as the other two (MAX_COL_ALIAS_FRAC below).
  //
  // CORRECTION OF RECORD (review-117-jrt-7 [M]1 / review-117-jrt-7o [M]3,
  // both reviewers independently, and REPRODUCED here). An earlier draft
  // of this very comment left the pair at exact-sequence equality only,
  // and justified it with the claim that T4 sits at "EXACTLY
  // MAX_COL_ALIAS_FRAC, surviving only because the test is `fr > lim` and
  // not `>=`", so that any tightening would break the rig's self-test.
  // THAT CLAIM WAS FALSE. The pair's limit was the literal `1`, not 0.5,
  // so `>` vs `>=` never entered into T4's survival and T4 cleared the
  // ACTUAL bound by 0.5, not by zero. The false justification was
  // protecting a real hole: with the pair unbounded, collapsing BOTH
  // render and present to 1,000 ns on 399 of every 400 rendered rows
  // passes every arm and drops a truthful 13.250 ms full_p99 to 7.784 ms
  // at rc 0 with judge_complete=1 — a FALSE GREEN on the gate's headline
  // PERF number, the same class rounds 4 and 5 closed.
  // The remedy is measured FREE: substituting MAX_COL_ALIAS_FRAC for the
  // literal 1 changes ZERO judgments across all 80 corpus artifacts
  // (t3 and t4 both still rc 0, and t4 still reaches the p99 assertion it
  // was fabricated to exercise, at 21.672 ms), while the 399/400 collapse
  // above is rejected at 99.75% agreement. Genuine margin is total: 0.0000%
  // measured over 78 genuine artifacts against a 50% bound.
  // So: the arm is EXACT-sequence equality, which no artifact genuine or
  // synthetic exhibits for ANY pair, PLUS the fractional bound on all
  // three pairs, whose genuine maxima are 4.25% / 0% / 0%.
  {
    const eqFrac = (a, b) => {
      let e = 0;
      for (let i = 0; i < a.length; i++) if (a[i] === b[i]) e++;
      return a.length ? e / a.length : 0;
    };
    const simR = [], renR = [], preR = [];
    for (let i = 0; i < expected; i++) {
      const m = ROW.exec(lines[i]);
      if (m[4] === "1") continue;
      simR.push(m[1]); renR.push(m[2]); preR.push(m[3]);
    }
    const pairs = [["sim", "render", simR, renR, MAX_COL_ALIAS_FRAC],
                   ["sim", "present", simR, preR, MAX_COL_ALIAS_FRAC],
                   ["render", "present", renR, preR, MAX_COL_ALIAS_FRAC]];
    for (const [na, nb, a, b, lim] of pairs) {
      const fr = eqFrac(a, b);
      if (fr >= 1 || fr > lim) {
        die("the " + na + " and " + nb + " columns agree on " +
          (fr * 100).toFixed(2) + "% of rendered rows" +
          (fr >= 1 ? " (they are the SAME sequence)" : "") +
          " (recording bug — one bracket was assigned the other's value)");
      }
    }
    // COMPOSITE-SPAN ALIASING (review-117-jrt-5 [M], demonstrated): the
    // same class one bracket wider — sim recorded over t3-t1 (the render
    // AND present span) instead of its own. Every shape arm passes and
    // direct pair equality is 0%, yet the reported p99 drops from a
    // truthful 17.093 ms to 14.708 ms and certifies the run. Its footprint
    // is exact too: sim becomes render + present on every rendered row.
    // No genuine artifact in the corpus exhibits that sequence-wide.
    const composite = simR.map((v, i) => Number(v) === Number(renR[i]) + Number(preR[i]));
    if (composite.length && composite.every(Boolean)) {
      die("sim equals render + present on EVERY rendered row (recording bug — " +
        "the sim bracket was recorded over the render+present span, not its own)");
    }
  }
  // ...and its exact form, which needs no threshold at all: NO genuine
  // artifact in the corpus repeats with ANY period (row[i] == row[i-p] for
  // every i >= p, p <= n/2). A recording that does is a replayed buffer.
  for (let p = 1; p <= expected >> 1; p++) {
    let periodic = true;
    for (let i = p; i < expected; i++) {
      if (lines[i] !== lines[i - p]) { periodic = false; break; }
    }
    if (periodic) {
      die("the whole file repeats with period " + p + " over " + expected +
        " frames (recording bug — a replayed buffer, not a measurement)");
    }
  }
}
if (render.length >= POP_MIN_ROWS && presentPos < render.length * MIN_PRESENT_POS_FRAC) {
  die("only " + presentPos + " of " + render.length + " rendered frames have present > 0 " +
    "(recording bug — the present bracket was not taken; a genuine run measures a " +
    "nonzero present on at least " + (MIN_PRESENT_POS_FRAC * 100) + "% of rendered frames)");
}

function pcts(arr) {
  const a = arr.slice().sort((x, y) => x - y);
  const nr = (q) => a[Math.ceil(q * a.length) - 1];
  return { p50: nr(0.5), p99: nr(0.99), max: a[a.length - 1] };
}
const F = pcts(full), S = pcts(sim), R = pcts(render), P = pcts(present);

function emit(tag, v) {
  console.log(tag + "_ns=" + String(v));
  // `_ns` is the exact integer and is what every consumer COMPARES (the
  // p99 budget test reads full_p99_ns). `_ms` is a human-readable
  // companion: the binary division misrounds exact half-microsecond ties
  // downward (1,000,500 ns prints 1.000, not 1.001 — review-117-jrt-1
  // [L]), which affects 9,917 of the 1,068,480 values in the measured
  // corpus. Left as-is DELIBERATELY: switching to integer quotient
  // arithmetic would change the emitted bytes of ~0.9% of values and so
  // break the Tier A+ byte-identity regression against every archived
  // judgment, to fix a display digit no gate arm reads.
  console.log(tag + "_ms=" + (v / 1e6).toFixed(3));
}
emit("full_p50", F.p50);
emit("full_p99", F.p99);
emit("full_max", F.max);
emit("sim_p50", S.p50);
emit("sim_p99", S.p99);
emit("render_p50", R.p50);
emit("render_p99", R.p99);
emit("render_max", R.max);
emit("present_p50", P.p50);
emit("present_p99", P.p99);
console.log("skips=" + String(skips));
console.log("rendered=" + String(render.length));
// judge_complete integrity terminator (iter 62 — the iter-61
// audio-judge pattern, class completion): emitted ONLY after every
// verdict line above, so truncated/partial judge output can never end
// with it; the shell consumers assert it on the output FILE's BYTES
// (must END with exactly 'judge_complete=1\n' — trailing blank lines
// violate the grammar as written).
console.log("judge_complete=1");
