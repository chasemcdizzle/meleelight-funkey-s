# M2-CAL report — calibration slice: environmentalCollision.js → C

2026-07-14 · iters 15–18 on `agent/auto` · tracked by issue #16.
This is the go/no-go measurement PLAN §4/M2-CAL demands before the loop
commits to the full M2 rewrite (~14–19k LOC of sim JS). Gate:
`bash port/sim/check-envcoll.sh` → `ENVCOLL MATCH`, exit 0.

## 1. What was measured

- **Slice**: `src/physics/environmentalCollision.js` (1,343 lines, all 13
  exports, no stubs) + its live dependency slice (Vec2D, linAlg,
  findSmallestWithin, solveQuadraticEquation, lineAngle, extremePoint,
  ecbTransform, zipLabels, drawECB-as-noop): **1,689 JS lines ≈ 1.69
  KLOC** → 2,209 lines of C (`port/sim/`), plus a 798-line reusable
  replay rig (`port/sim/calib/`, not translation).
- **Drive data**: every call crossing the module's exported boundary
  during full 3,600-frame oracle replays of three goldens —
  g01 fox/marth/battlefield (119,619 records), g04 puff/falcon/dreamland
  (34,052), g06 falcon/marth/fountain (33,004; includes 4 real corner
  collisions and per-frame-varying platform geometry) — **186,675
  records** total, captured by wrapping the module object in the live
  browser oracle (capture runs verified non-perturbing: 6× STREAM MATCH
  against the frozen golden streams).
- **Comparison**: canon-v1 serialization (IEEE-754 bit-pattern hex —
  injective; a single ulp anywhere in any return value is a divergence).
  Comparator sensitivity was itself negative-tested (§4).

## 2. Measurement table

| Metric | Value |
|---|---|
| Translation surface | 1.69 KLOC JS (1,343 module + 346 deps) |
| Boundary records replayed | 186,675 (3 traces × 3,600 frames) |
| Divergences (replay comparator) | **0** |
| divergences/KLOC | **0.0** |
| Behavioral fixes during burn-down | 0 (nothing to fix) |
| Non-behavioral defects pre-replay | 1 (compile error: -Werror unused variable; 2 min) |
| Fix-rate | n/a — burn-down opened at zero |
| Burn-down convergence | trivially converged (0 → 0; strictly non-increasing, no oscillation) |
| Wall-clock, M2-CAL total | ~40 min (09:05→09:43 + gate/report; task 1 rig ~8 min, task 2 translation+replay ~18 min, orientation/recon ~15 min) |
| Wall-clock per KLOC (translate + converge) | ~25 min/KLOC observed |

## 3. Root-cause class breakdown

Zero replay divergences means the class ledger records **prevented**
classes, not fixed ones. Each was identified during capture/recon and
neutralized at design time — these are now mandatory rules for every M2
module brief:

| # | Class | Instance in this slice | Prevention rule for M2 |
|---|---|---|---|
| 1 | JS Math.max/min/sign ≠ C fmax/fmin (NaN + ±0 semantics) | `Math.max(0, s-ε)`, `Math.sign` compared with `===` | always `js_max/js_min/js_sign` (ml_js.h); never libm min/max |
| 2 | `undefined` flowing through arithmetic (ToNumber → NaN) | frame-1 ECB1 is upstream-uninitialized `{x:undef,y:undef}` | map arg-undef → canonical NaN 0x7ff8…; PIN the "no undef in returns" invariant so the mapping's soundness is machine-checked |
| 3 | object KEY PRESENCE ≠ value (optional `damageType` keys) | touching data with/without `damageType`; value may be null/undef/string | model presence explicitly (DT_ABSENT tag); serialize per construction site |
| 4 | browser-libm transcendentals | `lineAngle` → `Math.atan2` | fdlibm on BOTH sides (fd_atan2), M0's crosscheck already proves bit-equality |
| 5 | FMA contraction | every product-sum in the sweep math | `-ffp-contract=off` on every TU (build scripts enforce) |
| 6 | algebraic "cleanup" during transcription | tempting rewrites of the intercept formula | copy expression SHAPES verbatim; the negative test (one term's `.a`→`.b`) shows a single-token slip = 11,883 divergences — instantly visible |
| 7 | value-model guessing | surface echo shapes, stage projection | capture-FIRST workflow: read real record shapes before finalizing C structs; strict marshaller hard-fails on anything outside the captured domain |

## 4. Comparator sensitivity (negative tests, all restored)

- Transcription typo (one `.a`→`.b` in one term of
  `coordinateInterceptParameter`): **11,883 divergences**, first at
  record 155 — the exact class M2 fears most is loudly visible.
- Single corrupted nibble in a capture copy: **exactly 1 divergence** at
  exactly that record — byte-level comparison confirmed.
- Instructive non-test: +1 ulp on the module constant `additionalOffset`
  (1e-5) → 0 divergences, and that is mathematically CORRECT (the
  1.6e-21 absolute change is below every consumer's result ulp).
  Constant-level ulp probes are not valid comparator tests; code-shape
  perturbations are.

## 5. Projection → M2 effort

Observed: 1.69 KLOC translated AND converged to bit-identical in ONE
agent-iteration (~25 min/KLOC incl. recon), with the capture/replay rig
(reusable, module-agnostic) built in a second iteration (~8 min).

Remaining M2 surface: ~14–19 KLOC ⇒ **8–12 comparable module slices**.
Budgeting honestly for the harder classes the slice did NOT price
(§6 caveats) at 2–3× the observed per-KLOC cost, plus per-module
capture marshallers and integration (game-state struct, tick loop,
checksum-stream serializer with a Ryu-class shortest-float formatter):

**Projected M2 effort ≈ 15–30 agent-iterations** (≈ 2–5 loop-days at
observed cadence). Even the pessimistic end is well under any 10×-budget
alarm (PLAN §4's NO-GO threshold); nothing in the measurement suggests
non-convergence anywhere.

## 6. Caveats (kept honest)

- environmentalCollision is the FRIENDLIEST module class: pure functions,
  no input mutation, no RNG, one transcendental, no string building.
  Unpriced classes for M2: in-place mutation of shared state (player
  object), object-key iteration order, the ECMAScript shortest-float
  formatter for the checksum stream (one-time component; validate
  differentially against captured `String(x)` outputs), and cross-module
  integration. The per-record replay harness localizes all of these the
  same way, so their fix COST should resemble this slice's; their COUNT
  will be nonzero.
- Canon-v1 compares bit patterns, not CHECKSUM.md's decimal strings —
  strictly as sensitive (both injective on doubles), chosen so the
  formatter cost stays out of the translation-rate measurement.
- Five exports (hLineAt/vLineAt/hLineThrough/vLineThrough/lineThrough)
  have zero live call sites in the three traces (translated and compiled
  anyway; they are 5 trivial one-liners exercised indirectly through
  internal callers that ARE covered).

## 7. Verdict per PLAN §4/M2-CAL

- Bit-identical over the full trace set: **yes** — 186,675/186,675
  records, three traces, full 3,600-frame ranges, exact equality.
- Converged burn-down, strictly decreasing, no oscillation: **yes**
  (trivially — it opened at zero and stayed there).
- Projection vs budget: **8–12 slices / 15–30 iterations**, nowhere near
  the >10× NO-GO condition.

**VERDICT: GO** — proceed to M2.
