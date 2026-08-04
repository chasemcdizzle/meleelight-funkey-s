# fix_plan — the loop's priority queue

Current phase: M4

Rules: items live under their phase heading; an actionable item needs an
exact runnable `done-check:` (see `docs/LOOP.md` §C). Items below are
seeded from PLAN §4's milestone contracts with `done-check: …` — the first
loop iteration on each phase is therefore a REPLAN that decomposes and
concretizes them. REPLAN owns rewriting a phase's section; nothing else
edits other phases' sections.

## M0 — Oracle hardened

(concretized by REPLAN, iter 1, 2026-07-14. Conventions fixed here:
upstream clone lives OUTSIDE the tree at
`${MELEELIGHT_CLONE:-$HOME/.cache/meleelight-funkey-s/upstream}`; golden
traces live at `oracle/goldens/gNN-<p1>-<p2>-<stage>.trace.json` with
frozen streams `gNN-<p1>-<p2>-<stage>.sha256.json` and a
`oracle/goldens/manifest.json` (chars/stage/cpu/frames/seed per golden);
`spikes/determinism/` stays frozen evidence — `oracle/` is the maintained
copy. Golden-set composition (PROVISIONAL, auto-adopted): g01 = existing
3800-frame Fox/Marth/Battlefield trace; g02–g06 = human-vs-human traces
covering the remaining char pairs and the other 5 VS stages; g07–g08 =
human-vs-CPU traces (difficulty 5) on two distinct stages; together every
one of the 5 characters and 6 stages appears ≥1×.)

(no items remain — M0 PASSED its exit gate `bash oracle/verify_goldens.sh`
on the phase-advance iteration, driver-verified; MILESTONE PASS: M0 logged
in docs/AGENT-LOG.md iter 9; issue #14 closed.)

## M1 — Data pipeline

(concretized by REPLAN, iter 9, 2026-07-14. Conventions fixed here: the
pipeline lives at `pipeline/` (host node; it never writes `oracle/`).
Every stage is EXECUTED-JS — data is executed out of the original built
artifacts (the animations bundle via a `window`-shimmed node `require` of
`dist/js/animations.js`; engine tables via an extractor bundle built with
upstream's own docker node:8 webpack toolchain), NEVER hand-transcribed.
Outputs land in `pipeline/build/<label>/` (gitignored by the existing
`build*/` pattern) with ONE deterministic `manifest.json` per run (sorted
keys, stable artifact order, sha256 + byte length per artifact, upstream
git HEAD + per-source-file hashes as provenance, NO timestamps and NO
absolute paths). Pinned coverage lives in `pipeline/expected.json`
(measured-then-frozen from executed runs, like goldens). Binary/table
layouts are specified in `pipeline/FORMATS.md` — PROVISIONAL formats,
little-endian pinned explicitly (host arm64 LE, device ARMv7 LE); the C
side implements against that spec, never against the JS. Executed-coverage
reconciliation (measured this REPLAN): anatomy's "754 animation files" =
744 exported action states + 5 index.js + 5 dead falco files never
required by falco/index.js nor referenced anywhere
(ILLUSIONFX, THROWNDOC{BACK,DOWN,FORWARD,UP}); paths = 27,808 exact;
frames = 27,820; int16 coords = 7,747,148 (~15.5 MB); dist has 204 sfx
wavs of which exactly 180 are referenced as Howls in src/main/sfx.js;
8 music oggs. Audio artifacts are Nintendo-derived: PRIVATE use only,
never distributed — provenance field marks them.)

(task 1 — pipeline skeleton + animations serializer — DONE iter 9:
`bash pipeline/check-animations.sh` → ANIMATIONS OK, exit 0.)

(task 2 — extractor bundle + engine tables → generated C — DONE iter 10:
`bash pipeline/check-tables.sh` → TABLES OK, exit 0. CTAB1 spec:
FORMATS.md §3; extractor at `pipeline/extractor/`, tasks 3-4 extend the
same bundle for stage geometry + sounds.)

(task 3 — stage geometry → generated C tables — DONE iter 11:
`bash pipeline/check-stages.sh` → STAGES OK, exit 0. STAB1 spec:
FORMATS.md §4; the extractor bundle now also exposes window.__stages —
ystory/fountain's god-module imports externals-stubbed, see §4.2.)

(task 4 — audio conversion + executed-JS sound map — DONE iter 12:
`bash pipeline/check-audio.sh` → AUDIO OK, exit 0. SND1 spec:
FORMATS.md §5; ffmpeg 8.1.1 + exact argv + aggregate output sha256
frozen in expected.json audio — a different ffmpeg fails loudly, never
drifts; blobs are Nintendo-derived PRIVATE USE ONLY and live only in
gitignored build output.)

(M1 PASSED its exit gate `bash pipeline/verify_pipeline.sh` on the
phase-advance iteration 14, driver-verified — PIPELINE OK, exit 0,
64s wall; MILESTONE PASS: M1 logged in docs/AGENT-LOG.md iter 14.)

(task 5 — full-pipeline runner + M1 exit gate — DONE iter 13:
`bash pipeline/verify_pipeline.sh` → PIPELINE OK, exit 0. The gate
composes the four unchanged task-level checks + one whole-pipeline
double-run into `build/gate-{a,b}` with manifest byte-identity, full
artifact re-hash, the FULL expected.json contract, gate-a compiled
round-trips (38832 + 412 leaves pinned) and the no-commit guard. §M1 is
empty — the next iteration is the M1 phase-advance; the driver/CHECKER
owns re-running the gate and advancing the phase.)

## M2-CAL — Calibration slice

(concretized by REPLAN, iter 15, 2026-07-14. Conventions fixed here: the
slice lives at `port/sim/` — `environmental_collision.{c,h}` +
`util/{vec2d,lin_alg,find_smallest_within,solve_quadratic_equation,
line_angle,extreme_point,ecb_transform,zip_labels}.{c,h}` structure-parallel
to the JS module paths (drawECB = documented no-op, render-only). The
capture/replay rig lives at `port/sim/calib/`; captures land in
`port/sim/calib/build/` (gitignored `build*/`). CAPTURE APPROACH
(PROVISIONAL, auto-adopted): the module boundary is wrapped AT RUNTIME —
the capture runner serves the untouched built dist but textually exposes
the webpack module cache (`var installedModules = {};` → `+
window.__wpCache = installedModules;` in the served bytes only, disk
untouched), then wraps every exported function of the
environmentalCollision module object post-load (babel CJS exports are
plain writable properties; all external call sites dereference the
namespace object at call time — verified in dist/js/main.js; internal
calls use local bindings and are correctly NOT captured). oracle/ is NOT
modified (HARD RULE 3): the runner reuses oracle/harness/{init,pagelib}.js
+ port/fdlibm/fdlibm.js VERBATIM by path and judges every capture run with
the unchanged verify-stream.js against the frozen golden stream —
instrumentation that perturbs the sim cannot pass. Value serialization
(canon v1, PROVISIONAL deviation from CHECKSUM.md conventions, documented
in port/sim/calib/FORMAT.md): CHECKSUM.md structural rules (sorted nested
keys, T/F/null/undef/fn tokens, arrays element-wise) but numbers as
IEEE-754 bit-pattern hex `d:<16hex>` instead of shortest-round-trip
decimal — injective on doubles both ways, strictly bit-exact (a single
ulp diverges), and keeps the ECMAScript shortest-float formatter out of
the calibration (it is a known one-time M2 component, not part of the
per-KLOC translation rate being measured). Captured goldens: g01
(fox/marth/battlefield — the PLAN §4 anchor trace), g04
(puff/falcon/dreamland — puff movement stresses collision), g06
(falcon/marth/fountain — moving platforms make the stage argument vary
per frame). Full manifest frame counts (3600), manifest params only.)

(task 1 — capture rig — DONE iter 16:
`bash port/sim/calib/check-capture.sh` → CAPTURE OK, exit 0. 186,675
boundary records over g01/g04/g06, byte-stable ×2, 6× STREAM MATCH,
counts pinned in expected-capture.json.)

(task 2 — structure-parallel C translation + replay driver — DONE iter
17: `bash port/sim/calib/check-replay-runs.sh` → REPLAY RAN 119619
records, 0 divergences, exit 0. All three captures replay bit-identical
on the first successful build — comparator teeth proven by negative
tests: transcription-typo build → 11,883 divergences; single corrupted
capture nibble → exactly 1.)
(task 3 — burn-down + metrics + report — DONE iter 18:
`bash port/sim/check-envcoll.sh` → ENVCOLL MATCH, exit 0. 186,675/186,675
records bit-identical across g01/g04/g06; divergence ledger opened AND
closed at zero (0 div/KLOC — root-cause classes were prevented at design
time, see docs/M2CAL-REPORT.md §3); projection 15–30 agent-iterations for
M2's ~14–19 KLOC; VERDICT: GO. Gate negative-tested: report-needle
removal → exit 1; captures deleted → 3 fresh recordings + STREAM MATCH +
ENVCOLL MATCH. §M2-CAL is empty — next iteration is the M2-CAL
phase-advance; the driver/CHECKER owns re-running the gate and advancing
the phase to M2.)

## M2 — Sim core checksum-locked, headless

(concretized by REPLAN, iter 19, 2026-07-14. CONVENTIONS fixed here:

- **Layout**: C sim stays at `port/sim/`, structure-parallel to upstream
  paths — `physics.c`, `hit_detection.c`, `action_state_shortcuts.c`,
  `article.c`, `interpolated_collision.c`, `characters/{shared,fox,falco,
  falcon,marth,puff}/moves/*.c`, `input/`, value models `ml_player.h` /
  `ml_input.h`, flattened game-state struct (the god-module's mutable
  globals become ONE struct; port the logic, never the module graph).
- **MEASURED remaining surface** (upstream lines, executed-recon this
  REPLAN; honest vs the anatomy's ~14–19k shorthand): physics/ 3,841
  (envcoll's 1,343 done) + characters moves 28,716 (5 chars + shared,
  heavily boilerplate-repetitive interrupt chains — the effective unique
  surface is far smaller) + main sim slice (~600 of main.js:
  gameTick mode-3 body, update, matchTimerTick + player.js 167) + input
  (meleeInputs 219 + Input record/buffer slice of input.js) + util
  substrate ~700 + movingPlatforms stage logic. Attributes/ECB/hitbox/
  stage data are M1 CTAB1/STAB1/ANIM1 tables, NOT retranslated.
- **Per-cluster verification (the M2-CAL pattern, PLAN §4 M2)**: every
  cluster gets (a) a module-boundary capture over goldens g01/g04/g06
  via the GENERALIZED rig (`port/sim/calib/`, spec-driven; every capture
  run non-perturbation-guarded by the unchanged verify-stream.js against
  the frozen stream), (b) a structure-parallel C translation, (c) a
  strict replay driver comparing canon-v1 serializations bit-exactly
  (FORMAT.md), counts pinned in `expected-capture-<spec>.json`. Replay of
  every captured record must show 0 divergences (CLUSTER MATCH).
- **Mutation-capture (rig upgrade, task 2)**: boundary functions that
  mutate arguments/globals (physics.js onward: player[p], hitQueue,
  stage, RNG draws, sound events) capture a POST-STATE canon field after
  the return-value field; replay marshals pre-state, calls C, compares
  ret AND post-state bit-exactly. New capture classes discovered become
  rules here (rule 8+).
- **RULE 8 — undef-at-rest / accessor echo (discovered task 1, 84
  divergences prevented-forward)**: ACCESSOR-class functions (raw
  property reads like getXOrYCoord) echo `undefined` VERBATIM —
  ToNumber(undefined)→NaN happens ONLY at the consumer's arithmetic.
  Value models whose fields can hold undefined at rest (frame-1 ECB1;
  the CHECKSUM stream itself serializes those as `undef`!) must model it
  explicitly (JsNum/JsVec2D, vec2d.h); the marshaller's undef→NaN arg
  mapping is valid ONLY for arithmetic-context args, and the
  no-undef-ret pin is per-function with a frozen accessor allowlist
  (expected-capture-<spec>.json `undefRetAllowed`, FORMAT.md).
  EXTENDED task 2: undef-at-rest is not number-only — phys.canWallJump
  holds undefined in 87% of captured snapshots (JsBool, ml_player.h);
  and void MUTATORS legitimately return undef (their value is the
  post-state field) — same frozen-allowlist mechanism, distinct class.
- **RULE 10 — upstream "copy" helpers may ALIAS (discovered task 2)**:
  the sim's deepObjectMerge call sites are 3-arg (physics.js:1070,
  main.js:824), leaving `exclusionList` undefined — which FALSIFIES the
  recursion guard (`typeof obj[key]==='object' && exclusionList && …`,
  deepCopyObject.js:23) so the "deep merge" executes as a SHALLOW per-key
  REFERENCE assignment: prevFrameHitboxes.{active,hitList,id} become
  aliases of hitboxes' arrays, and later writes through one path are
  visible through the other. Never assume an upstream copy helper copies:
  read the EXECUTED, argument-dependent semantics, and model the aliasing
  explicitly when translating mutation clusters (task 5 owns the live
  prevFrameHitboxes aliasing; ml_player.h documents the value-level
  merge semantics ml_hitboxes_merge_from implements).
- **RULE 9 — NaN payloads are not values (discovered task 1, 24
  divergences fixed at class level)**: V8 emits payload NaNs
  (0xfffefffffff6ffff from undefined-arithmetic) and propagates them by
  its own evaluation order; C compilers may legally commute FP adds
  because payloads are unspecified — payload equality across the
  boundary is UNREPRODUCIBLE and semantically meaningless
  (String(NaN)=="NaN" in the real checksum stream, no typed-array
  aliasing of sim values). canon v1.1 collapses ALL NaNs to
  d:7ff8000000000000 on BOTH sides (capturelib dhex + canon.c cb_num);
  measured no-op on the frozen envcoll captures (zero NaNs). Injectivity
  on JS number VALUES (incl. -0) is preserved. NEVER "fix" a NaN-payload
  divergence by reordering C arithmetic to chase V8 — that is
  curve-fitting to unspecified behavior.
- **THE 7 MANDATORY PREVENTION RULES** (docs/M2CAL-REPORT.md §3 — binding
  for EVERY module translation, every task brief): (1) js_max/js_min/
  js_sign (ml_js.h), never libm fmax/fmin; (2) ToNumber(undefined) →
  canonical NaN 0x7ff8…, soundness pinned by capture invariants; (3)
  object KEY PRESENCE modeled explicitly (DT_ABSENT pattern), serialize
  per construction site; (4) fdlibm on both sides for transcendentals;
  (5) `-ffp-contract=off` on every TU; (6) expression SHAPES copied
  verbatim, no algebraic cleanup; (7) capture-FIRST — read real record
  shapes before finalizing C value models, strict marshallers hard-fail
  outside the captured domain.
- **AI policy (PROVISIONAL, auto-adopted)**: PLAN §4 keeps ai.js JS-side
  until M4, but the M2 exit gate covers CPU goldens g07/g08. Resolution:
  AI-as-recorded-input — upstream consumes AI through aiInputBank
  "identically to human input" (anatomy §7), so a non-perturbing capture
  records per-frame aiInputBank state + the AI's seeded-PRNG draw count
  (+ any other verified write-set), and the C sim replays CPU players
  from that recording, advancing its mulberry32 by the recorded draw
  count at the runAI call site. Task 16 hard-verifies the write-set
  recon (aiInputBank + non-checksummed bookkeeping like currentAction/
  currentSubaction only — anything else fails the task). M4's ai.js port
  replaces the bridge and must reproduce the same streams live.
- **Regression every iteration**: `bash port/sim/check-envcoll.sh` stays
  green (the M2 conformance guard per LOOP §E.2 until check-sim.sh
  exists, then both). Never weaken any check; exact equality only.
- **RULE 11 — zero-live-coverage boundaries with a reachable future
  domain get a SYNTHETIC-DOMAIN SWEEP (adopted task 3)**: task 1
  registered "translated anyway, zero live records" as honest-coverage
  debt; where that surface WILL be exercised later (meleeInputs = the M3
  device stick path), the spec defines a deterministic `sweep()` —
  fixed-value calls through the wrapped exports, recorded at frame 0 and
  replayed like live records (FORMAT.md "synthetic-domain sweep").
  Teeth proven: a Math.round-semantics error (naive floor(x+0.5))
  produces 73 sweep divergences and ZERO live ones. Dead-upstream
  surfaces stay documented skips; sweeps never substitute for live
  chain verification.
- **RULE 12 — sweep purity is about NET effect; and negative tests must
  bite occurring values (adopted task 4)**: a sweep MAY touch sim-global
  state when (a) it restores it exactly (the executeIntangibility sweep
  injects a synthetic player into an inactive slot and restores the
  original value) and (b) it never draws the SEEDED stream — swap
  Math.random for a local same-algorithm generator for the duration (the
  KO-shout sweep; the replay mirrors with a sweep RNG for frame-0
  records). The existing ×2 byte-stability + STREAM MATCH guards remain
  the proof of non-perturbation — an unrestored write or a stolen seeded
  draw fails them mechanically. Corollary for comparator teeth: verify a
  negative test actually flips an outcome in the captured domain — trace
  inputs are keyboard-quantized (axes hit no values in (0.78, 0.79]), so
  threshold nudges can be no-op "teeth"; formula-constant/event-name/RNG
  perturbations bite reliably.
- **RULE 14 — RNG chain-order ambiguity is MEASURED-then-pinned, never
  assumed (adopted task 6)**: when a mutation boundary both draws the
  seeded stream ITSELF and contains dispatch windows whose moves may
  draw, the relative order of owner draws vs window draws is not
  recoverable from the record stream. The capture records window draws
  under a DISTINCT name (`Math.randomW`), the pins freeze the measured
  count (zero over g01/g04/g06 for hitdet), and the replay hard-fails on
  one — eager standalone-draw processing stays sound by construction.
  The moves clusters (tasks 7-12: move boundaries draw shouts AND call
  other moves) inherit this instrument.
- **RULE 13 — JS DESUGARINGS are part of the expression shape (adopted
  task 5)**: (a) compound assignment groups its ENTIRE right-hand side:
  `a += b + c` is `a = a + (b + c)` — left-flattening it in C is a 1-ulp
  FP divergence class (measured: 22 live divergences from
  `pos.x += cVel.x + kVel.x`, the ONLY divergence class in the whole
  physics translation); (b) domain traps stand in for upstream THROW
  sites and must sit at the exact lazy dereference point — hoisting a
  trap above the guard that upstream short-circuits on (canGrabLedge[0]
  inside a snap-box test) turns unreached-undefined into a false abort.
  Rule 6's "expression shapes verbatim" includes the operator DESUGARING
  and the evaluation ORDER, not just the arithmetic.
  `firstNonNull.js` (zero importers upstream — dead code),
  `deepValue.js` (target-builder encode only — M4), `randomAnnulusPoint`
  (vfx render-only), `deepCopy/deepCopyObject` (type-specialized in task
  2 where the C value model lives), `createHitBox/createHitboxObject`
  (hitbox value model, task 6), `Segment2D` included in task 1. EXTENDED
  task 3: `pollKeyboardInputs`/`pollGamepadInputs`/`setCustomCenters`/
  `showButton`/`keyboardMap` are browser-I/O plane (DOM key events,
  navigator.getGamepads, jQuery, settings keyMap) with zero live records
  — pollInputs short-circuits to the harness-injected input for human
  slots (the harness patch IS the recorded oracle behavior); their math
  core (meleeInputs) is fully translated + sweep-verified, and the M3
  device frontend synthesizes melee-unit coordinates directly
  (docs/research/b0xx-mapping.md §3). `startScreenPrompt.js` is
  render-only.)

Tasks (dependency order; each < ~400-line diff where possible, per-char
move tasks are data-shaped repetition):

(task 1 — util/math substrate — DONE iter 20:
`bash port/sim/calib/check-util-replay.sh` → UTIL MATCH, exit 0.
1,970,207 boundary records over g01/g04/g06 (34 wrapped functions, 11
modules), byte-stable ×2, 6× STREAM MATCH, 0 divergences after two
class-level fixes that became rules 8 and 9 above. Rig generalized to
--spec (envcoll parity byte-identical, 119,619 records). 16/34 boundary
fns have zero live records over these traces — translated anyway,
report-§6 precedent; Segment2D/Vec2D#dot/detectIntersections' other
exports get their first live records from later clusters' captures.)

(task 2 — player/game-state value model + mutation-capture rig upgrade —
DONE iter 21: `bash port/sim/calib/check-player-model.sh` → PLAYER MODEL
MATCH, exit 0. 21,600 post-update(i) player snapshots over g01/g04/g06
(7,200 each), byte-stable ×2, 6× STREAM MATCH, 0 divergences: per record
canon→MlPlayer→canon round-trip + deep-copy independence probe +
deepObjectMerge property check. Boundary = physics.js's exported
`physics(i,·)` — update(i)'s tail statement (update itself is
main.js-internal to gameTick, not namespace-wrappable). `ml_player.h`
finalized capture-FIRST via the new `survey-shapes.js` instrument: two
hitbox-entry shapes (constructor ActiveHitbox vs aliased chars-data with
per-frame offset arrays), 13 runtime-added presence-modeled fields
(IASATimer, inAerial, hit.reverse, hitboxes.frames, phys.grabTech/
laserCombo/ledgeHangTimer/autocancel/rollOut×7), canWallJump
undef-at-rest (JsBool). Rig upgrade: optional 5th-field POST-STATE canon
(capturelib `post` callback; `postStateFns` pins in check-spec-pins.js).
Projections documented in FORMAT.md: charAttributes/charHitboxes (M1
tables own them), percentShake (CHECKSUM.md §7). Honest coverage: the
merge frames-retention branch has ZERO live cases in these captures
(property-tested only; first live coverage arrives with task 5's physics
capture), and live in-frame prevFrameHitboxes ALIASING — rule 10 — is
task 5's surface.)
(task 3 — interpretInputs + input buffer + meleeInputs — DONE iter 22:
`bash port/sim/calib/check-input-replay.sh` → INPUT MATCH, exit 0.
750,992 boundary records per golden over g01/g04/g06, byte-stable ×2,
6× STREAM MATCH, 0 divergences on the first successful build (rules 1-10
held; js_round added to ml_js.h for JS Math.round semantics — V8's
Float64Round algorithm, ties toward +Inf, -0 preserved).
interpretInputs is main.js-internal to gameTick (not namespace-
wrappable): its OUTPUT is captured as the physics args projection
[i, inputBuffers[i]] and replayed as a full-trace CHAIN — the C state
machine (port/sim/input/interpret_inputs.{c,h} + input.h +
melee_inputs.h, value model ml_input.h, canon bridge
calib/input_canon.{h,c}) rebuilds every frame's 8-deep buffer from ITS
OWN previous output plus the recorded pollInputs injection; z/s
always-shift, pause-aware pastOffset, pause/frameAdvance bookkeeping and
end-of-tick frameByFrame handling verified bit-exactly over 3600 frames
× 2 slots × 3 goldens. meleeInputs' GC scaling/deadzone/quantization has
ZERO live records (the harness path bypasses polling) — covered by the
new synthetic-domain sweep (rule 11): 1,780 fixed executed-upstream
calls per capture. Honest coverage: pastOffset=0 (paused-history freeze),
the pause/frameAdvance edge branches, the AI/gamepad arms and the
startGame/endGame combos have zero live cases — translated verbatim,
guarded by ml_input_out_of_domain traps where behavior would need
another cluster's surface (AI bank = task 16, lifecycle = task 17).)
(task 4 — actionStateShortcuts + state-machine scaffolding + C mulberry32
+ sound-event queue seam — DONE iter 23:
`bash port/sim/calib/check-asshort-replay.sh` → ASSHORT MATCH, exit 0.
97,516 boundary records over g01/g04/g06 (29 wrapped exports + the
Math.random/rngBoot stream records), byte-stable ×2, 6× STREAM MATCH,
0 divergences on the first successful build (rules 1-11 held). C:
`port/sim/action_state_shortcuts.{c,h}` (verbatim translations; read-set
projected args; charAttributes/intangibility via M1 CTAB1 ml_tables —
FIRST consumer of the generated-table data path, live-cross-checked),
`port/sim/ml_rng.h` (mulberry32, chained draw-for-draw over every
recorded seeded draw incl. the off-step pre-frame-1 startGame draw and
the 465-draw boot fast-forward — browser boot count == the qjs boot pin),
`port/sim/ml_events.{c,h}` (sound-event queue seam for the M4 mixer +
dispatch-note seam + logged ml_random), actionStates registry scaffolding
(as_setupActionStates/as_lookup/as_dispatch, driver-self-checked; move
defs stay opaque until tasks 7-12). Post-state envelope
{dsp,mut,rng,snd}; event teeth proven (sound-name typo → 11 sweep
divergences; spurious dispatch → 3 live divergences on g06; RNG
off-by-one → 135; mut constant tweak → 307; corrupted nibble → exactly
1). Honest coverage: turbo interrupts, shieldDepletion break branch,
isFinalDeath true-arms and ALL positive dispatch paths have zero live
records (dispatch verified must-not-fire on every live record + by sweep
events); KO-shout sites + executeIntangibility covered by the rule-12
guarded impure sweep. Gotcha: a 0.79→0.78 threshold negative test bit
NOTHING — trace inputs are keyboard-quantized; negative tests must
perturb across values that OCCUR (see rule 12).)
(task 5 — physics.js core + interpolatedCollision — DONE iter 24:
`bash port/sim/calib/check-physics-replay.sh` → PHYSICS MATCH, exit 0.
47,060 records over g01/g04/g06 (21,600 mutation-captured physics(i,·)
calls with full pre/post envelopes, 21,441 oracle-fed move-dispatch
seams, 120 hitDetection-getter seams, 3,791 live + 105 sweep
interpolatedCollision records, 3 asFlags dumps), byte-stable ×2, 6×
STREAM MATCH, 0 divergences after FOUR class-level fixes (ledger in
docs/AGENT-LOG.md iter 24; the only live-divergence class — compound
assignment grouping — became rule 13). C: `port/sim/physics.{c,h}`
(structure-parallel; MlSim god-module slice; rule-10 alias sites modeled:
prevFrameHitboxes merge aliases + the land() pos-ECB1[0] alias with
capture-probe restoration; ecbSquashData chained module state; edgeOffset
code literal), `port/sim/interpolated_collision.{c,h}`; ml_player.h
gained rule-8 ECB1/ECBp component undef masks + presence-modeled
phys.passing (frame-1 pre-states). Moves stay tasks 7-12 (dispatch
resync seams verify site+args and restore post-state), launch getters
stay task 6 (args verified, rets injected). Honest coverage: the
damaging-stage hq path (no VS-stage damageType — M4 target stages) and
the pos→ECB1 write-through's OBSERVABLE window (grounded movement under
low ceilings) have zero live cases — translated verbatim, hq/probe teeth
proven by negative tests; turbo interrupts remain zero-live
(self-flagging via the seam FIFO if ever reached).)
(task 6 — hitDetection + hitQueue + hitbox value model — DONE iter 25:
`bash port/sim/calib/check-hitdet-replay.sh` → HITDET MATCH, exit 0.
54,977 records over g01/g04/g06 (18,000 mutation-captured pipeline calls
per golden — hitDetect×2/executeHits/checkPhantoms/resetHitQueue per
frame with full pre/post envelopes incl. the hq/phq queue value model —
plus 34 live dispatch seams, 636 pure getter/knockback/collision records
(live + the 164-call rule-11 sweep), the full seeded-RNG chain
draw-for-draw incl. screenShake's 4-draws-per-hit and 301 standalone
draws), byte-stable ×2, 6× STREAM MATCH, 0 replay divergences on the
first successful build (rules 1-13 held; one rule-7 marshal catch:
article passes RAW actionStates crouch/vCancel reads to getKnockback —
bool|undefined truthiness domain). C: `port/sim/hit_detection.{c,h}`
(structure-parallel; both MlHitboxSpec shapes' undefined-key semantics
via hb_* helpers; hitList push/splice write THROUGH the rule-10
prevFrameHitboxes alias; the throw arm's pos reassignment breaks
pos-ECB1; the launch getters are REAL bodies now — task 5's getter seams
stay in ITS replay only), ml_player.h gained presence-modeled
phys.phantomDamage/stageDamageImmunity, ml_events gained ml_sound_stop
("furaloop.stop" token). RNG-order instrument: draws inside a dispatch
window BELOW a hitdet boundary are chain-order-ambiguous → recorded as
`Math.randomW`, measured ZERO, pinned (rule 14). check-spec-pins.js now
STREAMS the JSONL (g06 capture 542 MB > node's 512 MB string cap — all
prior checks re-verified green). Honest coverage: clanks/CATCHCUT/
onClank, phantoms (storedPhantom/checkPhantoms' settle arm), throws
(isThrow rows, THROWNFALCONDIVE), executeGrabTech, shieldbreak,
powershield, jabReset/DOWNDAMAGE, FURASLEEPSTART/furaloop.stop,
ground-bounce, electric hits, cssHits and the stage-damage arm have zero
live records — translated verbatim, teeth proven by 8 negative tests
(POST nibble → exactly 1; hurtWidth 8→12 → 2; kb formula → 8; dispatch
threshold 80→30 → 36; RNG drop → 50; sound typo → 9; alias-mirror skip
→ 16; phantom classification inverted → 14).)
- **RULE 15 — per-char table COMPOSITION is executed data (adopted task
  7)**: upstream builds actionStates[c] as `{...baseActionStates,
  ...charMoves}` plus post-setup data patches (index.js setVelocities/
  posOffset assignments). Which states are shared-origin vs per-char
  OVERRIDES (puff overrides FURAFURA/JUMPAERIALB/JUMPAERIALF), the
  state→name map, and the patched data arrays are all MEASURED from the
  live tables — fn identity against the shared index module works because
  `deepCopyObject(true, ·)` deep-copies data but copies FUNCTIONS by
  reference — and dumped in the capture's frame-0 `mvData` record
  (finalCheck drift-guarded); the C registry is built FROM the dump,
  never from assumed file layout. Tasks 8-12 inherit the instrument
  (their specs extend the same dump/registry).

(task 7 — characters/shared moves — DONE iter 26:
`bash port/sim/calib/check-moves-shared-replay.sh` → MOVES SHARED MATCH,
exit 0. 4,985 + 5,351 + 5,061 = 15,397 records over g01/g04/g06
(14,943 mutation-captured top-level shared-move phase calls with
[phase,name,[slot,...extras],inputs,pre] args and {alias,hq,players,rng,
snd,vfx} post envelopes — 44 frame-0 rule-11/12 sweep records per golden
on a separate sweep RNG chain — plus 210 mdispatch seams, 238 standalone
draws, 3 mvData dumps), byte-stable ×2, 6× STREAM MATCH, 0 replay
divergences on the first successful build (rules 1-14 held; one rule-7
marshal catch during bring-up: playerObject's pos parameter is an ARRAY
[x,y], not {x,y} — the sweep's synthetic player). C: the MlMoveDef table
gets its first real bodies — `port/sim/characters/shared/moves/*.c` (79
files, structure-parallel), `moves_index.c` + `moves.h` (mv_dispatch
through the task-4 as_lookup registry; unregistered states cross the
driver's mv_seam), ml_events gained the vfx queue seam (M3/M4 renderer;
circleDust = 4 seeded draws), MlStageX gained respawnPoints/respawnFace
(REBIRTH's read set). framesData/attributes/charHitboxes("thrown") come
from CTAB1 ml_tables (3rd/4th consumers); index.js data patches +
actionSounds + palettes come from the measured mvData dump (rule 15).
Interrupt fallthrough arms return undefined upstream (SMASHTURN/TILTTURN
tilt arms, OTTOTTO/OTTOTTOWAIT's 2nd GUARDON arm, RUN's chain end,
SQUATWAIT's restart, ENTRANCE, STOPCEIL's FALL arm) — AsTri, carried
verbatim. Teeth: POST nibble → exactly 1; WAIT restart −1 → 1; circleDust
4→3 draws → 96 cascade; footstep typo → 22; GRAB→APPEAL → 8 seam-args;
double-jump direction flip → 0/1/1 (bites only where neutral DJs occur —
rule-12 corollary, 3rd measured instance). Honest coverage: FURAFURA
unswept (init stores the Howl play id into furaLoopID — outside the sim
value domain, C traps), finishGame (isFinalDeath true) trapped as task
17's lifecycle surface, live-stage CLIFF*/REBIRTH init arms beyond live
coverage and all per-char seam arms not fired by the traces are
translated verbatim and seam/trap-guarded. Rule-14 measurement: seam
window draws ZERO over all three goldens (mechanism supports nonzero via
seam rng lists).)
- **RULE 16 — value models measured on HUMAN goldens do not transfer to
  CPU goldens (adopted task 8)**: the AI plane writes its own value
  shapes — ai.js assigns NUMBERS into aiInputBank button fields
  (`.l = 0` / `= 1.0`; the task-3 input model's "buttons are real JS
  booleans" held only over g01/g04/g06). Before reusing a value model on
  a CPU golden, RE-SURVEY the captured domain; widen only after verifying
  the consumption mode (buttons are truthiness-only — measured: no raw
  button propagation into player state anywhere in characters/ or
  actionStateShortcuts.js — so CV_NUM buttons marshal by JS truthiness).
  Task 16's AI-input bridge inherits this: aiInputBank rows are NOT
  shape-identical to human Input objects. EXTENDED task 9: not just the
  AI plane — ANY new golden widens the player-plane value domain for all
  four slots (g05's MARTH fired NEUTRALSPECIAL for the first time across
  all captures: runtime-added phys.shieldBreakerCharge/ChargeAttempt/
  Charging + player.shieldBreakerID entered the domain, caught by the
  rule-7 marshal). A cluster's value model is verified over the UNION of
  goldens captured so far, nothing more — budget a re-survey (or a
  marshal hard-fail round) whenever a spec adopts a new golden.

(task 8 — characters/fox moves — DONE iter 27:
`bash port/sim/calib/check-moves-fox-replay.sh` → MOVES fox MATCH, exit 0.
1738 + 888 + 2934 = 5560 records over g01/g03/g08 — the fox carriers
(PROVISIONAL auto-adopted deviation from the g01/g04/g06 convention:
g04/g06 field no fox slot; g08 is the FIRST CPU-golden capture — AI stays
JS-side, its 1488 seeded draws chain-verify as standalone records), 3758
mutation-captured fox-origin phase calls (168 frame-0 rule-11/12 sweep
records per golden on the sweep RNG chain), 75 article seams (the task-13
boundary: LASER/ILLUSION inits verified name+options bit-exactly in call
order, no resync — inits only read player state), 0 live mdispatch seams
(fox never threw a non-fox live; FIFO teeth negative-test-proven),
byte-stable ×2, 6× STREAM MATCH, 0 replay divergences after two
value-model class fixes (hitbox offsetSingle sub-shape; rule 16's AI
number-buttons). C: `port/sim/characters/fox/moves/*.c` (61 files,
structure-parallel) + fox `moves_index.c`/`moves.h` (module-index
dispatch: direct shared/fox module calls mirror upstream's import graph;
table dispatch only where upstream uses actionStates — THROW* victim
arms); shared moves.h gained mv_assign_hitbox_id (generalized from
thrown_id0), mv_ledge_point, mv_hq_push6 (hitQueue.push value model:
canon-row append on the opaque hq carrier), mv_checkForIASA (REAL
dispatch through shared JUMPAERIALB/F modules + the registered per-char
module index — the task-4 note-based as_checkForIASA stays the asshort
boundary). Fox move-object data arrays (setVelocities*/THROWN* offsets/
CLIFF* offsets incl. authored expressions like -7.74-0.08) come from the
mvData fox dump (rule 15 extended per its recipe). Honest coverage:
SIDESPECIALAIR.init's grounded arm is unsweepable (upstream itself
infinitely recurses), CLIFF*'s onLedge===-1 canGrabLedge TABLE WRITE arm
traps in C (would also drift the finalCheck-guarded mvData dump),
THROWNFALCO*/FALCON* offset overruns trap (upstream throws), THROW*
against a live non-fox victim stays seam-guarded (loud on first live
record).)

(task 9 — characters/falco moves — DONE iter 28:
`bash port/sim/calib/check-moves-falco-replay.sh` → MOVES falco MATCH,
exit 0. 1779 + 1240 + 1619 = 4638 records over g02/g05/g07 — the falco
carriers (g07 = the second CPU golden; AI JS-side, seeded draws
chain-verify), 4218 mutation-captured falco-origin phase calls (217
frame-0 rule-11/12 sweep records per golden on the sweep RNG chain), 83
article seams (falco options carry isFox:false, THROWDOWN's lasers add
partOfThrow:true, ILLUSION always type 0), 0 live mdispatch seams (falco
never threw a non-falco live; FIFO teeth negative-test-proven),
byte-stable ×2, 6× STREAM MATCH, 0 replay divergences after two
value-model class fixes (marth's runtime-added phys.shieldBreaker* trio +
player.shieldBreakerID — rule 16 EXTENDED above: new goldens widen the
whole player-plane domain). C: `port/sim/characters/falco/moves/*.c`
(69 files, structure-parallel; task 8's fox recipe followed exactly) +
falco `moves_index.c`/`moves.h`. Falco structure deltas carried verbatim:
THROW* inits have NO grabbing===-1 guard (interrupt-only bare-return
arm), THROWN* have NO -1 guards/offset clamps (upstream throws — traps),
CLIFF* have NO canGrabLedge table-write arm (ledge[-1] throws —
mv_ledge_point traps), the shine is a 4-sub-state machine per environment
with actionState-write land/platform-drop arms, checkForIASA has NO
char-3 branch (no module registration — a falco IASA aerial payload
dispatches nothing, verbatim). Falco data arrays (THROWN*.offset, CLIFF*
offset/setVelocities, THROWFORWARD/ATTACKDASH/FIREFOXBOUNCE
setVelocities) come from the mvData falco dump (rule 15). Teeth: POST
nibble → exactly 1; FORWARDTILT actives flip (the falco-vs-fox delta) →
11/12/9; NSG laser y 7→8 → 31/13/29 article-args; THROWDOWN hq flag →
exactly 1; THROWUP through marth's table → seam-underflow; circleDust
4→3 → 77/62/42 cascade; THROWFORWARD setVel index → 1; shine AIRTURN
threshold 3→4 → 2 each. Honest coverage: THROW*.init ungrabbed and
snap-family THROWN*.init with grabbedBy=-1 unswept (upstream itself
throws), THROWN* offset overruns trap, live mdispatch zero
(seam-guarded, loud on first live record).)
(task 10 — characters/falcon moves — DONE iter 29:
`bash port/sim/calib/check-moves-falcon-replay.sh` → MOVES falcon MATCH,
exit 0. 1847 + 1316 + 1810 = 4973 records over g03/g04/g06 — the falcon
carriers, back on the g01/g04/g06-era convention with g03 replacing g01
(g01 fields no falcon). CARRIER MEASUREMENT (task-8 convention made
explicit): g07's CPU falcon fired ZERO live falcon-origin moves over its
3600 frames — carrier membership is measured LIVE COVERAGE, never char
presence; the loop's task brief had flagged g07 as a candidate, measured
out. 4633 mutation-captured falcon-origin fn calls (294 frame-0
rule-11/12 sweep records per golden on the sweep RNG chain), 0 live
mdispatch seams, 0 article records — falcon has NO article call sites
(6 dead `articles` imports, measured; pinned ZERO with the replay's
unconsumed-FIFO tripwire), byte-stable ×2, 6× STREAM MATCH, 0 replay
divergences on the FIRST successful build with ZERO value-model changes
(rules 1-16 held; rule 16's re-survey budget was already paid — all
three carriers were captured by prior specs). C:
`port/sim/characters/falcon/moves/*.c` (67 files, structure-parallel;
the 20 THROWN* + GRAB/CATCHATTACK are the task-8 fox translations with
renames — falcon-vs-fox diffs are DATA-ONLY there, offsets served by the
mvData falcon dump) + falcon `moves_index.c`/`moves.h`. NEW dispatch
surface class: falcon's move objects carry NON-phase fns —
onPlayerHit(p) (hitDetection.js:493 specialOnHit; SIDESPECIAL{GROUND,
AIR}) and onWallCollide(p,input,wallFace,wallNum) (physics.js:122
specialWallCollide; DOWNSPECIALGROUND — extras [wallFace(DX_STR),
wallNum(DX_NUM)]) — modeled as `mv_register_special_phases` (shared
moves.h: a driver-registered (state,phase)→MvFn lookup; MlMoveDef keeps
its 5-field shape so 209 existing positional initializers stay
untouched; tasks 11-12 reuse it for puff's NEUTRALSPECIAL* pair).
Falcon structure deltas carried verbatim: THROWN* families are FOX's
shapes (guarded PUFF/MARTH/FOX + unguarded FALCO/FALCON); THROW* keep
fox's grabbing===-1 init guard but fire NO lasers; CLIFF* keep fox's
canGrabLedge table-write trap arm; SIDESPECIALGROUND's runtime
`this.canEdgeCancel` SCALAR table write is C module state
(mv_falcon_ssg_set_canEdgeCancel — reader is physics' flag lookup, task
17); UPSPECIALCATCH/UPSPECIALTHROW draw the seeded stream INLINE (2
draws per firefoxtail ×3) and UPSPECIALCATCH's interrupt pushes a
hitQueue row; dead-arm quirks (SIDESPECIALGROUNDHIT's phys.timer,
DOWNSPECIALGROUNDENDAIR's player.timer — both undefined upstream)
carried as commented dead arms. Teeth: POST nibble → exactly 1;
FORWARDTILT active 9→8 → 9/17/9; THROWUP hq flag → exactly 1; THROWUP
through marth's table → seam-underflow (mdispatch FIFO teeth despite
zero live seams); circleDust 4→3 → 49/103/97; onWallCollide face flip →
3 each; NSG setVelocities index → 324/2/324 (bites live falcon punches —
rule 12); special-phase registration removed → OUT OF DOMAIN at the
first onPlayerHit record; UPSPECIALCATCH inline draws 2→1 → 8 each.
Honest coverage: live falcon throws/raptor-boost hits/wall collides are
zero over g03/g04/g06 (sweep-covered; seams loud on first live record);
THROWN{FALCO,FALCON}* guard arms unswept (upstream throws); the CLIFF*
canGrabLedge arm traps.)
(task 11 — characters/marth moves — DONE iter 30:
`bash port/sim/calib/check-moves-marth-replay.sh` → MOVES marth MATCH,
exit 0. 1608 + 1986 + 1586 = 5180 records over g01/g05/g06 — the marth
carriers, probe-MEASURED live coverage 1082/1417/1054 marth-origin move
records (g04 fields no marth). 4735 mutation-captured marth-origin fn
calls (394 frame-0 rule-11/12 sweep records per golden on the sweep RNG
chain), 0 live mdispatch seams, 0 article records — marth has ZERO
`articles` imports anywhere under characters/marth/ (stronger than
falcon's dead imports; pinned ZERO with the unconsumed-FIFO tripwire),
439 standalone seeded draws, 3 mvData + 3 rngBoot records, byte-stable
×2, 6× STREAM MATCH, 0 replay divergences after ONE translation fix (the
THROWNFALCOBACK face-flip delta, caught by the g01 probe — see the
AGENT-LOG iter 30 ledger). C: `port/sim/characters/marth/moves/*.c`
(75 files, structure-parallel) + marth `moves_index.c`/`moves.h` + the
two helper modules `dancing_blade_{combo,air_mobility}.c`
(characters/marth/*.js level). NEW oracle-fed seam class: `player[p].
shieldBreakerID = sounds.shieldbreakercharge.play()` CONSUMES the Howl
play id — a GLOBAL howler counter (advanced by every sound in the match,
mostly outside this cluster's records: unrecoverable chain state, rule
14's value-plane cousin) — the capture records each consumed id in the
record's post "sbid" list and the replay injects it via mv_howl_play_id,
re-emitting the consumed list (count/order teeth in the post compare +
the value lands in player.shieldBreakerID; 12 live ids on g05).
`sounds.shieldbreakercharge.stop(id)` = the token
"shieldbreakercharge.stop". NEW char-data-plane write instance (falcon
canEdgeCancel's class, VALUE-plane sub-shape): NEUTRALSPECIAL*'s
timer-46 `hitboxes.id[i].dmg = newDmg` MUTATES the global charHitboxes
objects (player.js:132 aliases chars data, no copy) — newDmg == the
authored 7 for charge < 30 (measured live domain {0,1}); a live CHARGED
punch followed by a later nsg/nsa hitbox assign would diverge LOUD at
that init record (C reads pristine CTAB1); the sweep exercises the >=120
arm and restores all 8 dmg fields (rule 12). mv_dispatch's special-phase
arm gained "onClank" (marth DOWNSPECIAL{GROUND,AIR}, hitDetection.js:
71-72's specialClank arm — the task-10 hook reused via
mv_register_special_phases(marth_special_phase)); mv_register_char_
module(0, marth_move_def) makes checkForIASA's char-0 MARTHMOVES arm
REAL dispatch — measured dead-by-construction upstream (only fox/falco/
falcon aerials call checkForIASA; marth's ATTACKAIRF/B INLINE the
aerial-IASA logic incl. `marth[a[1]].init` payload dispatch). Marth
structure deltas carried verbatim: THROW* dispatch victims 2-ARG
(.init(grabbing, input) — fox's THROWBACK/THROWDOWN are 1-arg); THROWUP's
interrupt -1 arm returns false where the other three fall through;
THROWN{PUFF,MARTH,FOX}* are guarded with per-file clamp-vs-guard ORDER
variation (+ THROWNMARTHBACK has NO init guard; THROWNPUFFUP wraps its
body in a vacuous `if(player[p].phys)`); THROWN{FALCO,FALCON}* are fox's
unguarded family with ONE body delta (THROWNFALCOBACK adds `face *= -1`);
all 8 CLIFF* keep fox's canGrabLedge table-write trap arm and
CLIFFGETUPQUICK sets ledgeRegrabCount=TRUE (authored quirk); smashes
charge via the timer==3/7/3 hold machine; dancing blade's
phys.dancingBlade/dancingBladeDisable are runtime-added
(presence-modeled, rule 16). Data arrays (ATTACKDASH/SSG3*/SSG4*/CLIFF*
setVelocities, UPSPECIAL's PAIR setVelocities rotated by
upbAngleMultiplier via fdlibm sin/cos, CLIFF*/THROWN* offsets) come from
the mvData marth dump (rule 15); SIDESPECIAL{GROUND,AIR}4DOWN compute
their charHitboxes key ("db{ground,air}4down"+floor((timer-7)/6)) at
runtime. Teeth: POST nibble → exactly 1; FORWARDTILT active 7→6 →
11/15/11 live; THROWUP hq flag → exactly 1; THROWUP through an
unregistered state → seam-underflow (mdispatch FIFO teeth despite zero
live seams); sbid injection dropped → 2/14/2 (bites g05's live
shieldBreakerID records); blend-overlay string format → 3/6/3;
dancingBladeCombo window 26→9 → exactly 1 each; onClank registration
removed → OUT OF DOMAIN at the first onClank record; UPSPECIAL setVel
index off-by-one → post divergence + the out-of-range trap. Honest
coverage: live marth THROW*/THROWN*/CLIFF*/dancing-blade/counter/
UPSPECIAL are ZERO over g01/g05/g06 — sweep-covered (394 calls incl.
every chain dispatch and the charge family), seams/traps loud on first
live record; a live charged shield-breaker punch (charge >= 30) is the
documented char-data-plane hole (loud on the next hitbox assign). Live
coverage DID include: 531-call live NEUTRALSPECIALGROUND arcs on g05
(the charge window, blend overlay, play-id, and release-stop arms live),
JAB1/JAB2 chains, FORWARDTILT, GRAB, DOWNATTACK (g06), and
ATTACKAIRN/F/B incl. land arms.)
- **RULE 17 — the M1-owned char-data VALUE plane is MEASURED per record
  where upstream writes it (adopted task 12)**: move code writes fields
  of the GLOBAL charHitboxes objects through player.hitboxes.id ALIASES
  (player.js:132 — falcon canEdgeCancel's class): marth's dmg write was
  benign over its live domain (task 11's documented hole), but puff's
  rollout writes id[0..2].dmg after release EVEN WHEN UNCHARGED —
  through whatever STALE id objects the previous move assigned
  (cross-move provenance a value-copy C model cannot track) — and sing
  cycles id[0].size. MEASURED live drift on g04 (jab1 dmg 3→7 from
  frame 1038; 983 records). The class fix is the "chd" PRE-PROJECTION:
  every move record's pre carries the EXECUTED {moveKey:{idN:{dmg,
  size}}} plane and the C's hitbox assigns feed dmg/size from it, never
  from assumed-pristine CTAB1 (dmg/size are the only createHitbox
  fields with upstream write sites — measured by grep). Instrument >
  documented hole: task 17's integration replaces the projection with
  the sim's own live plane; any cluster consuming M1-owned data that
  upstream mutates at runtime inherits this discipline.

(task 12 — characters/puff moves — DONE iter 31:
`bash port/sim/calib/check-moves-puff-replay.sh` → MOVES puff MATCH,
exit 0. 1362 + 1716 + 2935 = 6013 records over g02/g04/g08 — the puff
carriers, probe-measured live coverage 843/1215/1037 (g08's CPU puff
attacks, unlike g07's falcon). 4307 mutation-captured puff-origin fn
calls (404 frame-0 rule-11/12 sweep records per golden on the sweep RNG
chain), 1 LIVE mdispatch seam (g08's CPU puff THROWBACK dispatching
fox's THROWNPUFFBACK — the FIRST live per-char-cluster seam; verified
in call order), 0 article records — puff has ZERO `articles` references
(marth-strength; pinned ZERO with the unconsumed-FIFO tripwire), 1699
standalone seeded draws (1491 on g08's AI plane), 3 mvData + 3 rngBoot
records, byte-stable ×2, 6× STREAM MATCH, 0 replay divergences on the
FIRST successful build (rules 1-17 held; the chd mechanism and the
rollOutTurnTimer widening were built capture-first, before any C ran).
C: `port/sim/characters/puff/moves/*.c` (71 files) + puff
`moves_index.c`/`moves.h` + the two helper modules
`puff_multi_jump_drift.c`/`puff_next_jump.c` (characters/puff/*.js
level; puffNextJump dispatches COMPUTED module-index keys
"AERIALTURN"/"JUMPAERIAL"+(1+jumpsUsed) — rung 6 unreachable, trapped).
Puff OVERRIDES shared FURAFURA/JUMPAERIALB/JUMPAERIALF on table 1
(rule 15's origin map measures them puff-origin there, shared
elsewhere — both directions asserted); puff's FURAFURA is TRIVIAL
(WAIT.init — no furaloop: the task-11 sbid mechanism is NOT needed,
measured, no Howl-id consumption anywhere under characters/puff/).
NEUTRALSPECIAL{GROUND,AIR,GROUNDTURN} onPlayerHit + NEUTRALSPECIALAIR
onWallCollide route through the task-10 mv_register_special_phases
hook; mv_register_char_module(1, puff_move_def) makes checkForIASA's
char-1 PUFFMOVES arm real (dead-by-construction upstream — marth
precedent). Puff structure deltas carried verbatim: ROLLOUT's
mid-body charge-scaled timer advance + always-false NSG/NSA
interrupts; the multijump ladder's cVel.y rungs + AERIALTURN's t===13
handoff; ATTACKAIRN's hitboxes.FRAMES++ / ATTACKAIRB's lowercase
phys.autocancel typos; THROW* fractional timers with floor(+0.01)
crossings + THROWBACK's floor-over-comparison typo + THROWDOWN's
unguarded crossing; the guarded/unguarded THROWN* split with per-file
snap/flip/negX/clamp-order variation; CLIFFGETUPQUICK's lone
ledgeRegrabCount=TRUE. Teeth: POST nibble → exactly 1; FORWARDTILT
active 6→5 → 5/19/5 live; THROWUP hq flag → exactly 1; THROWUP through
an unregistered state → seam-underflow; chd IGNORED (pristine CTAB1) →
2/8/2 — bites g04's LIVE drifted assigns; extra UPSMASH draw → 16 each
(chain cascade); release-vel 0.09→0.08 → 2 sweep-only (live releases
were charge-0 — rule-12 corollary, 4th instance); newDmg base 12→11 →
8/378/8 (bites g04's live rolls massively); onPlayerHit registration
removed → OUT OF DOMAIN at the first record; sing size 12.890→12.5 →
exactly 1. Honest coverage: live puff THROW*/THROWN*/CLIFF*/sing/rest
are zero-to-thin over g02/g04/g08 — sweep-covered (404 calls), seams/
traps loud on first live record; live coverage DID include the FULL
rollout family (g04's 378-record live dmg-write arc + the g08 CPU
rollouts), JAB1/JAB2, tilts, smashes, aerials incl. land arms, the
multijump ladder, GRAB, DOWNATTACK, and puff as grab victim. The
per-char surface is COMPLETE: tasks 7-12 cover all five characters +
shared — next is task 13 (articles).)
- **RULE 18 — module-owned queue/state planes fully enclosed by captured
  boundaries get a CHAIN-VERIFY instrument + lean-when-empty envelopes
  (adopted task 13)**: when every upstream mutation site of a module's
  state is itself a captured boundary (the article queues: spawns,
  per-tick mains, destroys, hit rows), the C replay CHAINS that state
  across records and COMPARES it against each in-match record's pre
  before re-marshaling (authoritative) — a wrong mutation flags at the
  very next record even when that record's own body replays clean
  (instrument > per-record trust; proven by a post-record chain
  corruption tooth: 27/19/27 pure chain divergences, zero record
  divergences). Complement: when the module's driving queue is EMPTY at
  entry the body reads nothing else (loops never run), so the envelope
  is a measured read-set projection gated on queue emptiness
  ("lean-when-empty", FORMAT.md) — capture size drops ~20x with zero
  teeth loss (non-trivial frames carry full state). Task 14's
  movingPlatforms (stage module state) and task 17's integration inherit
  both instruments.

(task 13 — articles — DONE iter 32:
`bash port/sim/calib/check-article-replay.sh` → ARTICLE MATCH, exit 0.
14636 + 14595 + 15998 = 45229 records over g01/g02/g08 — the article
carriers, probe-MEASURED live coverage over ALL SIX fox/falco goldens
(live spawns/hits: g01 27/12 fox lasers battlefield — the 12 live hits
are ZERO-knockback, fox laser kg=bk=sk=0, percent-only; g02 19/6 falco
lasers ystory — the ONLY live kb>0 article hits: live screenShake draws
+ live DAMAGEN2 dispatches; g08 27/21 fox CPU lasers fdest, 1496
standalone AI draws; g03/g05 field ZERO live articles, g07's 19 lasers
never connect; ZERO live ILLUSIONs anywhere — sweep-covered only).
14400 live + 41 sweep pipeline records per golden (lean-when-empty
envelopes, rule 18), 24 sweep + live ainit spawn records, 0 mdispatch
(FIFO teeth negative-test-proven), byte-stable ×2, 6× STREAM MATCH,
0 replay divergences on the FIRST successful build across all six
goldens (rules 1-18 held). C: `port/sim/article.{c,h}` — MlArticle/
MlArticles value model (LASER 13-key / ILLUSION 8-key instances, key
presence == kind; hb = per-article 12-key createHitbox with
offsetSingle — article-OWNED, no rule-17 aliasing), REAL bodies for the
task-8/9 article-init seams (the seam-to-body conversion: ainit args ==
the seam's [name,options] canon byte-identically; task 17 replaces
mv_article_* with direct article.c calls — documented in FORMAT.md),
executeArticleHits dispatches GUARD/SHIELDBREAKFALL/DAMAGEFLYN/DAMAGEN2/
CAPTUREDAMAGE through mv_dispatch into task 7's REAL shared bodies
(hit_detection.c linked for getKnockback/getHitstun/knockbackSounds;
hd_flags for crouch/vCancel; CTAB1 weight). Upstream quirks carried
verbatim: ILLUSION's `isFox || true` always-true, wall-death sweep-0
falsiness, duplicate-destroy splice(-1)-from-END, hit.hitPoint pos
aliasing (unobservable — measured by reading), the evaluated-but-empty
commented clank block, LASER's strokeStyle table write (render-only).
Teeth: fox laser speed 7→6 → 77/23/77 live; screenShake 4→3 → 1/99/1
(bites g02's live kb>0 hits); hitList push drop → 26/14/44 live;
DAMAGEN2 dispatch drop → 6 live on g02 (0/0 elsewhere — kb=0 hits never
dispatch, measured); splice −k drop → 1 each (sweep-only: live dstq len
≤ 1 — rule-12 corollary, 5th instance); foxshinereflect typo + ILLUSION
kg 40→41 → 1 each (sweep); chain corruption → 27/19/27 (rule 18's own
teeth); corrupted POST nibble → exactly 1. Honest coverage: live
reflects/shield-hits/illusions/multi-destroys are ZERO over the carriers
— all sweep-covered (72 calls); no reachable arm is unswept.)
(task 14 — movingPlatforms stage-tick logic — DONE iter 33:
`bash port/sim/calib/check-platforms-replay.sh` → PLATFORMS MATCH,
exit 0. 3787 + 3778 + 3786 records over g01/g02/g06 — carrier membership
by STAGE IDENTITY (only ystory/fountain have non-empty movingPlatforms
bodies upstream — battlefield/dreamland/pstadium/fdest are EMPTY,
measured; the brief's "pstadium if live" resolved: its body is empty,
nothing to translate); g01 battlefield represents the static-stage class
(3600 lean {plat} records pinning the one-call-per-tick contract +
platform-plane non-drift). 3652 movingPlatforms records per golden (3600
live + the 52-call rule-11/12 sweep on the sweep RNG chain), byte-stable
×2, 6× STREAM MATCH, 0 replay divergences on the FIRST successful build
(rules 1-18 held; ledger opened and closed at zero). C:
`port/sim/stages/{moving_platforms.{h,c},ystory.c,fountain.c}` — MpSim
value model (the god-module slice: full platform plane, platformStates,
starting, 4× player {grounded,onSurface,pos} — the rider/transfer loops
are UNGUARDED by playerType upstream; inactive slots hold page-start
CSS-era playerObjects); no M1 tables consumed (the rail/platform
constants are upstream CODE literals, not STAB1 data — the movingPlats
STAB1 presence pins are asserted at install). NEW rig mechanism (the
__wpCache class): run-capture.js's served bytes gain a SECOND injection
— fountain's module-PRIVATE platformStates (a closure `let` no export
reaches) gets a read-only window getter after its declaration
(unique-match hard-fail; quote/newline-free so eval-string-safe; pure
exposure) — making rule 18's chain-verify DIRECT instead of
effects-only, and letting the sweep drive the private machine through
its live entry objects. Rule-18 chain EXTENDED to the whole platform
plane (statics included): "nothing else writes the plane" is now a
per-record measurement, not an enclosure assumption. Live coverage:
g06 fields the full fountain machine (90 starting-arm frames, 27 owner
draws, sink/base-return/both-newTimer arms live); g02 fields Randall's
full rail loop; the rider + all four transfer arms are ZERO live —
sweep-covered. Upstream shapes carried verbatim: ystory's four
sequential non-exclusive rail arms with `move` last-arm-wins
(double-fire corners arm1→arm3 and arm2→arm4; a bottom-right double is
impossible — arm 2 tests before arm 3), the commented-out
`pos.y += move[1]` rider line, fountain's selection draw
consumed-but-ignored on the base-return arm, `timer--` on fractional
timers. Teeth: POST nibble → exactly 1; rail step 0.354845→0.354846 →
7206 live on g02; arrival band 0.075→0.076 → 1927 live on g06; draw
hoisted out of the base arm → 199 on g06; reset 22.125→22.126 → 272 on
g06; chain corruption after a clean record → 2/1 PURE chain divergences
(chain-plat + chain-ps, zero record divergences — rule 18's own teeth);
transfer arm dropped → 1 sweep-only (rule-12 corollary, 6th instance);
rider arm dropped → 8 sweep-only. Task 17 wires mp_movingPlatforms into
the gameTick order and replaces MpSim with the live sim slices.)
(task 15 — ECMAScript shortest-float formatter + CHECKSUM.md `ser` in C
— DONE iter 34: `bash port/sim/calib/check-format.sh` → FORMAT MATCH,
exit 0. C `String(x)` = vendored Ryu core (port/ryu/, ulfjack/ryu pinned
@ 4c0618b0, byte-verbatim — Apache-2.0/BSL-1.0 dual, NOTICES entry +
PROVENANCE.sha256 re-verified every check run) + our ECMA-262
§6.1.6.1.20 steps 6-10 formatting layer (`port/sim/ml_fmt.c`; -0 → "0"
at String level); CHECKSUM.md §3 ser + §4 hash primitives in
`port/sim/ml_ser.c` (the explicit `-0` token per pagelib.js:10-13, T/F,
undef/null/fn/cyc, full JSON.stringify escaping, SHA-256 lowercase hex
via oracle/qjs/sha256.c). DIFFERENTIAL evidence (all `cmp`-byte-exact,
zero divergences on the first full run): (a) adversarial — 5,469,538
deterministic corpus patterns (sha256-pinned; specials/payload NaNs,
every exponent × 8 mantissa templates, subnormal ladders, powers of
2/5/10 ±ulp spreads, 1e21/1e-7 threshold straddles ±64 ulp, known-hard
shortest-repr literals, 5.4M seeded random raw/int/short-decimal/
subnormal patterns) C vs V8 String(x) + oracle numStr, both columns; (b)
captured — 249,225 unique doubles extracted from ALL 55 existing capture
files (every spec, every golden; cold-run floor pinned 26,478); (c)
composite — 39,971 cases from the g01 captures: 36,335 record trees +
3,600 per-frame §2/§3.1 envelopes (fixed-literal key order, 760 with
live articles) + 36 synthetic 4-slot envelopes, C parse→ser→SHA-256 vs
the ORACLE'S OWN ser/__serializeState/__sha256 (extracted from/run as
pagelib.js's actual source — zero transcription on the reference side).
Teeth (perturb→count→restore): n≤21→22 → 27,075; -6→-7 → 16,229; exp
'+' dropped → 2,156,736; -0 dropped → 3,294 adv + 3 live composite; T/F
swap → 17,061; envelope literal-order swap → 3,636 (every envelope);
sha nibble → 39,971 (every case). Gotcha classes: the small-int
trailing-zero fold (ryu d2s.c:475-493) is OUTPUT-neutral here by
construction (small ints < 2^53 < 1e16 never reach exponent form — kept
to mirror ryu's caller, unperturbable tooth documented); zsh `time
pipeline` pipestatus masks a mid-pipe failure — exit codes verified by
direct invocation. Task 17 consumes ml_fmt/ml_ser for checksum-stream
emission; ml_sb_num is the ONLY number emitter the sim serializer may
use.)
(task 16 — AI-input bridge for CPU goldens — DONE iter 35:
`bash port/sim/calib/check-ai-bridge.sh` → AI BRIDGE OK, exit 0.
17812 + 17893 records over g07/g08 (3510 runAI mutation records each —
the CPU slot's !starting frames, args [i], post {bank, bk, rng}; 7020
pollInputs + 7200 physics = the FIRST input-chain capture over the CPU
goldens; 81/162 standalone draws; g08's 1334 attributed AI-window draws:
162 + 1334 == the frozen rngCalls 1496), byte-stable ×2, 4× STREAM
MATCH, 0 replay divergences on the FIRST successful build. WRITE-SET
RECON hard-checked two ways: grep-measured (live ai.js writes =
aiInputBank[i][0].{a,b,csX,csY,l,lA,lsX,lsY,x,y,z} + player[i]
bookkeeping {currentAction,currentSubaction,lastMash,curentAction-typo}
ONLY — player.inputs/phys/timer writes are all inside block comments)
AND runtime-enforced per runAI call (in-page pre/post canon diff of all
4 players minus the allowlist, all 32 bank rows, playerType/cS/
gameSettings/activeStage; wsViol records pinned ZERO + finalCheck
throw). C: `port/sim/ai_bridge.{h,c}` (AIBRIDGE1 artifact loader +
ml_ai_bridge_apply: burn recorded draws bit-verified on the chained
mulberry32, verify never-AI-written fields against the chain, install
the row), `port/sim/input/ai_input.h` — rule 16's CLASS FIX: the
tagged JS-value input model (MlAiVal bool|number|undefined; AI helper
literals assign undefined through missing-key reads) now holds THE
interpretInputs implementation; ml_interpret_inputs became a conversion
wrapper (check-input-replay.sh re-verified bit-exact over g01/g04/g06).
Artifact = build/<id>.ai-bridge.txt (deterministic distillation ×2 +
cmp, build-ai-bridge.js); replay (replay_ai.c) chains BOTH slots
through the shared MlInputSimState, models the pollInputs bank-row
ALIAS (slot 0 re-copied post-runAI), rule-18 chain-verifies the bank at
every poll. Upstream facts pinned: harness mType="keyboard" for CPU
slots → the raw*/deaden AI arm never runs; pollInputs returns the bank
ROW (alias); runAI runs between interpretInputs and physics inside
update(i). Teeth: capture nibble → exactly 1; artifact dropped draw →
1234 (chain-shift cascade); draw-value nibble → exactly 2, chain
intact; bank lsX perturbation → 10 (runAI + slot-0 physics + rule-18
poll + 8-deep history propagation); never-written field s=B1 →
divergences then a chain-state HARD ABORT (the injected s drives the C
pause machine — the keyboard arm is live on CPU slots); in-page junk
write → 3510 wsViol + capture run fails. Honest coverage: undefined
bank values are zero-live on g07/g08 (model + marshal support them —
CPUTech's missing-l literal never fired); runAI-skipped SLEEP frames
zero-live (chain handles them by construction); task 17 consumes the
artifact at the real update(i) call site and replaces the physics-args
projection with the live buffer plane.)
(task 17 — INTEGRATION: the full C gameTick — DONE iter 36:
`bash port/sim/check-sim.sh` → **SIM CONFORMS**, exit 0 — the M2 EXIT
GATE: ALL 8 goldens replayed end-to-end by the headless C sim
(`port/sim/sim/`), judged by the UNCHANGED verify-stream.js against the
frozen streams — 3600/3600 exact hashes per golden, rngCalls equal
(g01..g08: 134/125/119/115/185/160/81/1496), rngCallsOutsideStep == 1,
specVersion 1. Composition: GameState = the flattened god-module struct
(MlSim + HdQueues + MlArticles + MlInputSimState + input chains + the
mp plane + lifecycle lets); boot = player.js constructors +
start()/harnessSetupMatch/startGame verbatim (sim_boot.c; the
startingPoint-array `.x`/`.y` quirk = the frame-1 ECB undef masks;
gameSettings = the settings.js DEFAULTS — fresh-context localStorage
nulls keep them, phantomThreshold 0.01); tick = main.js:1050-1092 in
call order (sim_tick.c) with every driver seam now the REAL upstream
edge (mlp_dispatch/hd_dispatch→mv_dispatch, mlp_hd_*→hit_detection.c,
mv_article_*→article.c — task 13's documented swap, mv_hq_push6→the
live hitQueue, runAI→ml_ai_bridge_apply + the bank-row alias re-copy,
tagged AI buffers projected by rule-16 truthiness); checksum emission =
ser_player + article canon → the fmt_diff §3.1 envelope → SHA-256
(sim_ser.c; sim_frame_envelope + sim_host --dump-frames = the
localization instrument); data planes = SIMDATA1
(calib/dump-sim-data.js: boot-time asFlags/hdFlags/mvData-union dump,
×2 byte-identical, every section byte-equal to the frozen captures'
frame-0 records) + CTAB1/STAB1 regenerated by the pipeline; boot-RNG
parity = 465 boot draws + counter reset + the ONE off-step startGame
draw, counts recovered from the mulberry32 state delta (modular
inverse of 0x6D2B79F5). RULE-17 CLASS INSTRUMENT: the live charHitboxes
plane lives in sim_data.c behind WEAK-default hooks
(mv_chd_assign_note/mv_chd_write_{dmg,size} in shared moves_index.c —
replay drivers keep exact per-record-chd behavior, no driver/script
edits; marth's charged shield-breaker hole closed at class level).
LEDGER: ONE sim divergence in the whole 8-golden composition — g08
f371, hit_detection.c's zero-live THROW arm read `offset.x` off a
thrown offsetSingle CHARDATA hitbox as if array-shaped (the task-8
sub-shape discovered AFTER task 6's translation) → NaN pos; class-fixed
at the site; class = "later-cluster value-model widenings must be
back-propagated to earlier clusters' zero-live arms" (grep-swept: no
other instance; AGENT-LOG iter 36). Re-verified after the fix:
ENVCOLL MATCH, HITDET MATCH (fresh ×2 captures), all six moves +
article clusters 0-divergence against frozen captures. Honest notes:
outOfCameraTimer's ++ sites are render.js-only and the ORACLE ran with
__harnessNoRender — the frozen domain has it ≡ 0 (the render increment
is M3/M4 renderer surface); mv_howl_play_id = monotone counter
(player.shieldBreakerID is not on the checksum surface; M4 mixer owns
real ids); finishGame/matchTimer-expiry + stage-damage hq rows trap;
the SDL2 dev TU deferred to M3 with the platform seam (the headless
host IS the CI/loop backend; no render surface exists to present yet).
done-check: `bash port/sim/check-sim.sh` → prints `SIM CONFORMS`,
exit 0 — this is the M2 exit gate command (§Commands).)

## M3 — On-device at 60 fps

(M3 PASSED its exit gate `bash port/sim/device/verify_m3.sh` → M3 GATE
OK, exit 0, AUTHORITATIVE (all 23 freeze-manifest producers reviewed-go,
anchor verified), driver-run cold as CHECKER at the phase-advance —
MILESTONE PASS: M3 logged in docs/AGENT-LOG.md 2026-07-17. The §H human
gate is CLEARED: Chase's hands-on S1 ratification playtest PASSED
2026-07-17 — the S1 chord table is RATIFIED AS-IS ("controls work
perfectly", "sound is perfect"); the two visual amendments observed
(stage-surface legibility at device scale, invisible vfx) are folded
into §M4 as tasks 3 and 1-2. Issue #18 closed. Play install persists at
/mnt/mlfk-data + /mnt/Applications/meleelight.opk.)

(concretized by REPLAN, iter 37, 2026-07-16. The FunKey-S is ON ADB
(id `12c00003237f5528`) — the iter-36 `m3-device` LOOP STOP is cleared.
M2 passed its exit gate (SIM CONFORMS, driver-verified cold; issue #17
closed). CONVENTIONS fixed here:

- **Device hygiene (hard)**: device writes go ONLY to `/tmp/mlfk` (tmpfs,
  128 MB — measured this REPLAN) and the SD scratch dir `/mnt/mlfk-scratch`;
  both removed at the end of every check script (trap-guarded). Park the
  frontend (`touch /mnt/disable_frontend; pkill gmenu2x`) ONLY for
  SDL-video/live tasks and ALWAYS restore (`rm /mnt/disable_frontend`);
  headless compute runs (conformance, sweeps) don't park. Kill own
  processes on exit; never touch firmware/saves/other SD content; if the
  device drops off ADB, retry/replug-wait briefly then BLOCKER — never
  reset the device.
- **ADB facts (measured this REPLAN)**: this adbd does NOT propagate exit
  codes (`adb shell false` → host exit 0) — every device command runs
  through a helper that appends `; echo MLFK_RC=$?` and the host parses
  it (`port/sim/device/adbsh.sh`). Launch via login shell `sh -lc`
  (`/etc/profile` sets SDL_NOMOUSE=1); detached SDL runs need
  `setsid … </dev/null … & sleep 2`. Big artifacts (format corpus) go to
  `/mnt/mlfk-scratch`, small ones to `/tmp/mlfk`. Device runs are SLOW —
  generous host-side timeouts, foreground.
- **Cross-build recipe**: docker `jondbell/funkey-s-sdk` (SERIAL only),
  `arm-funkey-linux-musleabihf-gcc -O2 -ffp-contract=off -Wall -Wextra
  -Werror -static` over the EXACT TU list of `port/sim/check-sim.sh`;
  `-O3` only ever on the hot raster TU (PLAN §5). Arm binaries land in
  `port/sim/calib/build/device/` (gitignored `build*/`), stamp-cached
  (build-extractor.sh precedent; `MLFK_FORCE_ARM=1` rebuilds).
- **Judgment lives on the HOST**: device outputs are pulled and judged by
  the unchanged `oracle/harness/verify-stream.js` / `cmp` — the device
  never self-reports success. Exact equality only; an armv7 divergence
  from a frozen stream is a REAL finding (fdlibm/flags/promotion class) —
  ledger + class fix, never epsilon.
- **Renderer**: PLAN §5 verbatim — the measured rastbench variant
  (adaptive cubic flatten tol 0.25 px, nonzero-winding scanline fill via
  active-edge table, 4× vertical subsample AA with fractional span-end
  coverage, 565 blend, x-mirror facing, `pos*scale+offset` to 240×240,
  zero frame-loop allocations). Rasterization is NOT checksummed — sim
  state stays the only bit-exact surface; visual checks are STRUCTURAL
  (silhouette overlap vs the oracle's canvas render), measured-then-frozen
  thresholds, never loosened after freezing.
- **Input**: PLAN §6's S1 table verbatim — a data-driven chord table
  injecting FINAL Melee-unit values quantized to 1/80 at the pollInputs
  seam (bypasses tasRescale); tapJumpOffp1=true; left stick neutral while
  the Y C-layer is held; SOCD neutral; digital shield r=true rA=1.0.
- **Audio placement (PROVISIONAL, auto-adopted)**: PLAN §4/M3's EXIT says
  perf "with audio on" while §7's full mixer is listed under M4.
  Resolution: M3 ships the MEASURED audio-spike path (SDL 44100 Hz/
  S16LSB/2ch/512 callback + 8-voice 22050-mono SFX mixer, 16.16 resample,
  Q8 gain, int32 accumulate+clamp) fed by the sim's ml_events sound queue
  with SND1 pipeline blobs, so the p99 gate includes the real
  audio-callback cost; music streaming + full mixer polish stay M4. SND1
  blobs are Nintendo-derived: device scratch/SD only, never committed.
- **Live-session policy (PROVISIONAL, auto-adopted)**: the autonomous
  "live played session" is uinput-injected (own uinput device — writing
  the existing event device does NOT inject; ssb64 fk_input pattern)
  through the REAL SDL keysym path; Chase's hands-on S1 ratification
  playtest is the phase-end HUMAN GATE (sentinel per LOOP §H).)

Tasks (dependency order; each < ~400-line diff where possible):

(task 1 — armv7 correctness rung — DONE iter 38:
`bash port/sim/device/check-device-g01.sh` → DEVICE CONFORMS g01, exit 0.
FOUND + CLASS-FIXED: the SDK's static musl libc.a math was built with
unsafe-FP optimizations — floor/ceil/round are IDENTITY for non-integers,
fmod(0,0)==1.0 and -0 results lose their sign, strtod mis-rounds
subnormals and drops -0. port/fdlibm/fdlibm.c now carries exact
floor/ceil/fmod as STRONG symbol overrides; fmt_diff --gen is strtod-free
where the breakage lives (pin-verified byte-identical corpus);
port/sim/device/mathsweep.c is the standing device-vs-host-libm-anchor
instrument. NEW RULE for all M3/M4 device work: trust NO device-libc
math/parse symbol — differential-sweep it against a host anchor before
relying on it (sqrt/fabs measured healthy: VFP instructions). Device g01
wall clock ~21 s / 3600 frames, sim-only avg ~5.8 ms/frame.)

(task 2 — device conformance ALL 8 goldens + sim-only timing — DONE
iter 43: `bash port/sim/device/check-device-conform.sh` → DEVICE
CONFORMS 8/8 + SIM P99 OK, exit 0 (.loop/m3-task2-donecheck.log). Every
golden replayed on the device, streams judged host-side by the UNCHANGED
verify-stream.js vs the frozen *.sha256.json; g07/g08 rode their
AIBRIDGE1 artifacts (--cpu --difficulty --ai-bridge, check-sim.sh's
feed mirrored). sim_main gained `--timing <file>` (CLOCK_MONOTONIC ns
around tick+hash only, RAM-buffered, written post-run; host + device);
p50/p99 judged host-side by port/sim/device/percentiles.js
(nearest-rank), asserted p99_ns < 16670000 per golden. MEASURED
(docs/research/device-perf.md): p50 4.27-5.81 ms, p99 7.95-10.68 ms —
worst p99 (g08) leaves ~6 ms of frame budget for render+present+audio.
Rig plumbing extracted VERBATIM into port/sim/device/riglib.sh (both
device scripts source it; RIG_SCRIPTS makes every rig script's bytes
stamp input → ONE shared arm build, no stamp ping-pong; corpus
pins/sweeps stay g01-script-owned). Teeth: no-timing probe → pullv
loud death; perturbed RUN-side stream → verify-stream MISMATCH at the
exact frame (frozen-copy perturbation trips the name/integrity seal
first — perturb the run side to prove the frame comparator); 1 ms
threshold probe → SIM P99 FAIL after STREAM MATCH. Regressions green:
check-sim.sh SIM CONFORMS + check-device-g01.sh DEVICE CONFORMS g01
through the refactored lib.)

(task 3 — renderer core, host-side — DONE iter 44:
`bash port/gfx/check-render.sh` → RENDER OK, exit 0
(.loop/m3-task3-donecheck.log). `port/gfx/`: anim1.{h,c} implements
FORMATS.md §2 against the SPEC (bounds-checked, LE byte-assembled,
absent-frame sentinel); raster.{h,c} = THE rastbench measured variant as
a module (adaptive cubic flatten tol 0.25px, nonzero-winding active-edge
scanline fill, 4x vertical subsample AA + fractional span ends, 565
blend, zero frame-loop allocations) + an explicit per-pixel INK plane
(silhouette truth — low-alpha fills are 565-invisible); the ONLY -O3 TU.
gfx_render.c: camera = STAB1 `pos*scale+offset` VERBATIM in doubles
(upstream has NO dynamic zoom — measured) + retarget k=0.2/dy=45
letterbox; stage + renderPlayer (facing flips, colour branch, miniView
bubble, ENTRANCE squash, shield, REBIRTH halo, fg2-lineWidth persistence;
debug overlays trap) + LASER articles (chromaticAberration x3), all
structure-parallel to render.js/stagerender.js/article.js; palettes/
pPal/flashOnLCancel/reverseModel come from the EXECUTED GFXDATA1 dump
(capture-canvas.js), never hand-typed. gfx_replay.c = sim_main's loop
(cited copy; sim_main untouched) + render EVERY frame; stdout judged by
the UNCHANGED verify-stream.js (renderer takes const GameState* and the
render-on stream STREAM MATCHes — non-perturbation proven, not argued).
Browser reference: capture-canvas.js + gfx-pagelib.js on the
run-capture.js served-bytes class (oracle/ untouched), reduced render
sequence per step with Math.random native-swap + outOfCameraTimer
snapshot/restore, STREAM-MATCH-guarded; masks = fg1|fg2 alpha>0,
non-frozen, regenerated per run. IoU frozen 0.91 (seeded 0.90; measured
min 0.9149 f1237 / max 0.9302 over 16 frames; residual = browser-side
1px-line downscale boundary dilation, C-only cells ZERO). Teeth: dy
45→47 → all frames FAIL 0.74; PPM pid-XOR → cmp fails; renderer
sim-write 1e-9 → STREAM MISMATCH frame 2 (run-side; +1.0 variant dies
loudly at the DEADDOWN final-death trap). VFX EXCLUDED both sides —
REGISTERED deferral: the ml_events vfx seam carries names only (pos/face
projected away across 174 verified call sites); seam widening = M4 seed.
Gotcha classes: node -p JSON NUMBERS emit ANSI colour (String() them —
task-17 gotcha, new surface); render instrumentation must guard RNG
stream + render-written sim fields, sufficiency PROVEN by the capture's
STREAM MATCH; silhouette checks need an explicit ink plane.)

(task 4 — platform seam + SDL1.2 device backend + live device render —
DONE iter 50: `bash port/gfx/check-device-render.sh` → DEVICE RENDER OK
(full p99 10.743 ms, render-only p99 2.568 ms, sim p99 7.527 ms, present
p99 1.479 ms, skips 0/3600), exit 0 (.loop/m3-task4-donecheck.log).
The CLAUDE.md three-backend seam is live: `port/gfx/platform.h`
(platform_init/present/poll/quit + PlatformInput, FunKey letter-keysym
map ready for task 5) with platform_headless.c (loop/CI) /
platform_sdl2.c (host dev window, built not run) / platform_sdl1.c
(device: SetVideoMode fallback chain hit step 0 HWSURFACE|DOUBLEBUF,
BitsPerPixel==16 + RGB565 masks verified or bail, ShowCursor off,
SDL_Flip; DYNAMIC libSDL-1.2 asserted — LGPL) — exactly ONE TU per
binary; gfx_app.c = the paced replay app (absolute-deadline schedule,
~30-line frameskip valve skipping RENDER never SIM, RAM-buffered
stream/timing/screenshot written post-run to tmpfs, zero frame-loop
I/O). Device g01 STREAM MATCH 3600/3600 with render+present live;
screenshot (own fb, frame 900) structurally judged AND bit-identical to
the host render; frontend parked/restored (trap). gfx_device joined the
SHARED rig build (ARMBINS + port/gfx in srchash + RIG_SCRIPTS).
GFXDATA: committed gfxdata-frozen.txt (sha-pinned, browser-free device
path; check-render.sh cmp tripwire vs every fresh capture). CLASS
FIXES: device-libm float plane pre-empted (raster ifloorf/iceilf
integer helpers, fd_sin/fd_cos/fd_atan2 routing — render now
cross-platform deterministic; mathsweep gained sqrtf/fabsf columns,
measured healthy); per-script push provenance (G01_BINS — shared-roster
asserts break when the roster grows); pkill -f self-match gotcha.
Teeth: standing in-check valve tooth (1000 ns budget → 119/120 flagged
skips), stream nibble → MISMATCH frame 1800, blank shot → judge death,
pullv freshness → stale dst gone. Perf table:
docs/research/device-perf.md; ~5.9 ms p99 headroom for task-6 audio.)

(task 5 — S1 input layer at the poll seam + uinput live session — DONE
iter 51: `bash port/gfx/check-device-input.sh` → S1 INPUT OK, exit 0
(.loop/m3-task5-donecheck.log). `port/gfx/s1_input.h` (HEADER-ONLY on
purpose — task-4's reviewed TU lists unchanged): data-driven 11-row S1
chord table (PLAN §6 values verbatim; first-match-wins mirrors the
prototype resolver's branch order), SOCD neutral, Y-C-layer forces the
left stick neutral, digital shield r=true rA=1.0, deaden/meleeRound
parity with the prototype funkeyPoll (raw* = pre-deaden quantized);
emits complete 22-field FINAL Melee-unit rows at ml_poll_inputs'
verbatim-injection seam — tasRescale never called. s1_sweep.c asserts
the 15 pinned PLAN §6 checks bit-exactly + 2^11 exhaustive combo dump
byte-stable ×2 + every coordinate on the 1/80 grid. gfx_app gained
--live/--record-trace/--ready-file/--tapjump-off-p1 (recording = golden
trace JSON via ml_sb_num String(x) — bit-exact round-trip proven);
sim_main gained --tapjump-off-p1 (default unchanged; flag proven live
by an up-flick differential). port/tools/fk_input.c = own-uinput-device
injector (ssb64 pattern) playing the committed
port/gfx/s1-session.script (1080-frame session, every chord row
scripted, chords held 15-18 frames — the settling strategy; START
excluded: pause is main.js plane). Live session on the FunKey through
the REAL SDL keysym path: ready-file handshake, detached app + rc-file,
coverage judge 24/24 signatures, FOUR-way byte-identical streams (host
×2 + device replay + the LIVE stream itself), non-vacuity leg (live !=
all-neutral stream). FOUND + class-fixed: musl-1.2 64-bit time_t makes
libc's struct input_event 24 B vs the old kernel's 16 B — short write,
errno 0; fk_input emits the KERNEL's 32-bit ABI struct explicitly (the
iter-38 "trust no device-libc" class extended to kernel-struct
TIMESTAMP ABIs). Instrument exposure: the resolver's quantizer absorbs
table perturbations within ±1/160 of the correct grid point — the
sweep's detectable class is >= half a grid step. riglib.sh additions
per its own contract: RIG_SCRIPTS += check-device-input.sh, ARMBINS +=
fk_input, srchash roots += port/tools.)

(task 6 — audio-on — DONE iter 57:
`bash port/gfx/check-device-audio.sh` → DEVICE AUDIO OK (full p99
12.614 ms, underruns=0, attempts=2; cbs=5166 starts=274 stops=0 skips
0/3600), exit 0 (.loop/m3-task6-donecheck.log). SDL1.2 audio live on
device at the spike config (44100/S16LSB/2ch/512, obtained==requested
asserted) + the spike-math 8-voice mixer (16.16 resample, Q8 gain,
int32 accumulate+clamp; steal-oldest-by-start-seq — the "spike
default" claim was REFUTED: audiotest.c has NO allocation policy) fed
through the ONE ml_snd_sink chokepoint at ml_sound_play/stop enqueue
(sim_tick queue resets untouched; SIM CONFORMS + all audio-on streams
verify — the mixer only READS). SNDPACK1 from the REUSED pipeline
audio stage (pack-snd.js; x2 byte-identical, count=180 pinned, sha
frozen f695...ccb4; Nintendo-derived: build-output + device scratch
only). Underruns = the spike late200 proxy, ASSERTED == 0 (badlen ABI
tripwire == 0; cbs window [4900,5900] frozen, measured 5166; device
starts/stops == host truth 274/0). Retry policy: <= 2 attempts,
skips/underruns legs only — cold attempt 1 hit a NEW burst form of
the transient class (skips=8), attempt 2 clean; p99/stream/audio-
structural legs pass every attempt. Teeth: standing T1 pack-truncation
death / T2 dropped-blob (land) death at first play / T3 underrun
perturbation → judge rc 2 / T4 grammar corruption x2 → rc 1; device
T5 64-sample starvation probe → 17 underruns counted + rejected.
Honest coverage: audible fidelity unverified by construction (M4
seeds: mixer fidelity + music; stop-path live coverage — g01 fires
zero .stop events; skip-burst attribution instrument).)

(task 7 — OPK + frontend launch + M3 exit-gate assembly — DONE iter 58:
`bash port/sim/device/verify_m3.sh` → `M3 GATE OK`, exit 0
(.loop/m3-task7-donecheck.log). All four legs passed cold: [1]
`DEVICE CONFORMS 8/8 + SIM P99 OK`; [2] `DEVICE AUDIO OK (full p99
12.573 ms, underruns=0, attempts=2)`; [3] `OPK LAUNCH OK` (packaged
with the SDK container's mksquashfs 4.4 ONLY, launched via the REAL
gmenu2x FRONTEND driven by the fk_input uinput injector, boot-marker
bin-sha == arm-build stamp record, evidence g01 stream prefix 900/900
== frozen, in-app screenshot judge-shot structural); [4] `S1 INPUT OK`.
NEW: port/gfx/opk/{mlfk.sh (launcher — Exec=script, tmpfs log +
copy-back trap, SD data-dir env chain, boot marker w/ mounted-binary
sha), meleelight.funkey-s.desktop (trailing empty line asserted),
icon32.png (32×32, from OUR renderer's g01 f900 host shot — no
Nintendo asset bytes)}; port/gfx/check-device-opk.sh (leg-3 engine,
arc-pending); port/sim/device/verify_m3.sh (the gate — REUSES the
arc-hardened sub-checks, each verdict parsed by exact anchored grammar,
exactly-one-match posture); port/sim/device/m3-freeze-manifest.txt
(PROCESS §4 reviewed-pin freeze manifest — 23 producers sha256+status,
verify_m3.sh HARD-REFUSES before any leg on drift). riglib RIG_SCRIPTS
+= check-device-opk.sh + verify_m3.sh. MEASURED gmenu2x nav (empirical,
iter 58): conf section/link IGNORED for start; pkill-respawn lands at
the STABLE persisted section (games); nav `n m r a` normalizes the
within-games link then selects MeleeLight; wrong section → no boot
marker → leg FAILS LOUD (fail-closed, never a false pass). NOTE for
the phase-advance iteration: a gate pass is followed by the human-gate
sentinel `LOOP STOP: m3-device — needed: Chase S1 ratification
playtest` (LOOP §F-advance.3/§H) — the GATE does NOT print it (driver
duty). — done-check:
`bash port/sim/device/verify_m3.sh` → prints `M3 GATE OK`, exit 0.)

## M4 — Full-game parity

(concretized by REPLAN, iter 63, 2026-07-17. M3 is COMPLETE (gate + S1
ratification — see the §M3 annotation). CONVENTIONS fixed here:

- **Layout**: front-of-house (menus/CSS/stage-select/options) at
  `port/foh/` — REWRITTEN, not transliterated (anatomy §8: upstream
  menus are jQuery+DOM+canvas hybrids positioned in absolute 1200×750
  px): a C screen/state machine over the EXISTING platform seam +
  renderer, faithful FLOWS (screen graph, selections, options
  semantics, CPU-difficulty slider) and canvas LOOK at 240×240, never
  a DOM port. AI at `port/sim/ai.c` structure-parallel to
  src/main/ai.js (sim plane, checksummed). Target-test sim plane at
  `port/sim/target/`; target-stage data arrives via a NEW pipeline
  stage (executed-JS, extractor-bundle class — the M1 gate's pinned
  contract is EXTENDED measured-then-frozen, never weakened).
  Mixer/music stay at `port/gfx/` (snd_mixer.h precedent). M4-NEW
  golden artifacts (target-test traces, extra CPU traces) live at
  `port/goldens-m4/` with their own manifest.json — HARD RULE 3
  stands: only M0 tasks write `oracle/`; the M4 recorder REUSES
  oracle/harness bytes VERBATIM by path (run-capture.js served-bytes
  class) and every stream is judged by the UNCHANGED
  oracle/harness/verify-stream.js; oracle/goldens/* stay byte-frozen.
- **Checksummed vs NOT (the verification split)**: CHECKSUMMED
  bit-exact vs frozen streams — match traces (the 8 oracle goldens +
  the M4 CPU-difficulty traces) and target-test traces. Target-test
  conformance = the UNCHANGED spec-v1 surface via the unchanged
  verify-stream.js PLUS a SEPARATE per-frame TARGET-PLANE stream
  (targets/box state under the canon serialization rules, own SHA-256
  stream, own frozen file + verifier — PLAN §3's "additional stream"
  precedent, sound-event class). PROVISIONAL (auto-adopted):
  CHECKSUM.md stays at v1 and every frozen golden stays byte-untouched
  — the alternative (spec v2 + re-freeze) requires the PROCESS
  evidence-package machinery and buys nothing. NOT checksummed —
  menus/FOH, rasterization, audio DSP output.
- **Menu verification approach (pinned; PROVISIONAL, auto-adopted)**:
  committed FLOW SCRIPTS (deterministic per-frame input sequences)
  driven BOTH host-headless and on-device through the real input
  path; judges assert (a) the emitted screen-transition trace == a
  frozen structural expectation (whitelist-grammar parse), (b) the
  flow's resulting match parameterization == its pinned expectation
  bit-exactly, (c) the LAUNCHED match's checksum-stream prefix == the
  corresponding frozen golden (the flow→sim bridge IS checksummed
  even though the menu is not), (d) per-screen screenshots pass
  structural judges (judge-shot class, measured-then-frozen criteria;
  host renders byte-stable ×2). NO browser-side IoU for menus — a DOM
  hybrid offers nothing faithful to diff a rewritten screen against;
  visual-look authority = Chase's acceptance playthrough. Every FOH
  surface is Tier A (PROCESS §3).
- **outOfCameraTimer / render-on domain (PROVISIONAL, auto-adopted —
  resolves the iter-36 registered surface)**: the frozen oracle domain
  ran __harnessNoRender, so outOfCameraTimer ≡ 0 throughout; the C
  render plane keeps ZERO sim writes (const GameState* — proven M3)
  and M4 does NOT add upstream's render-side ++ (renderPlayer miniView
  arm). Live play, replays, goldens, and menu-launched matches stay
  ONE domain — the M3 live-session three-way stream identity depends
  on it. Documented deviation from a render-on browser; revisit only
  via a CHECKSUM.md version bump + the PROCESS evidence package.
- **Audio (PLAN §7 verbatim)**: device config 44100/S16LSB/2ch/512
  unchanged; SFX pre-decoded RAM (SNDPACK1); music = the M1 audio
  stage's 22050 stereo S16 PCM STREAMED from SD (88.2 KB/s, 2×64 KB
  double-buffer); sprite Start/Loop windows + effective volumes from
  SND1 sounds.json. FIDELITY check = OFFLINE DETERMINISTIC RENDER
  differential: the mixer is integer-only, so a golden's sound-event
  schedule renders to byte-frozen PCM on the host (×2 stable) and the
  DEVICE's offline render of the same schedule must cmp byte-exact;
  audible authority = Chase (no golden audio-output stream exists —
  iter-57's honest-coverage basis). All PCM/SNDPACK blobs are
  Nintendo-derived: build output + device scratch/SD only, NEVER
  committed.
- **AI (measured this REPLAN)**: `tan` is ALREADY vendored + swept —
  port/fdlibm carries fd_tan since M0 task 3 (fdlibm-crosscheck +
  mathsweep cover it), so PLAN §4/M4's "adds tan" is pre-satisfied;
  ai.c routes every Math.* through fdlibm like all sim TUs. AI draws
  come from the SAME seeded stream; AI inputs flow through the SAME
  aiInputBank/pollInputs seam incl. the bank-row alias + post-runAI
  slot re-copy semantics (M2 task-16 record). The AIBRIDGE1 path is
  RETIRED from the LIVE path once C-AI reproduces g07/g08 — but
  `port/sim/check-sim.sh` (the M2 EXIT GATE) is NEVER edited: its
  bridge-fed form stays the frozen M2 contract; M4 adds live-AI
  checks alongside, never in place of it.
- **Device + process inheritance**: riglib.sh, adbsh RC-echo, hygiene
  dirs (/tmp/mlfk + /mnt/mlfk-scratch, trap-removed), stamp-cached arm
  builds, push provenance, host-side judgment — inherited VERBATIM
  from §M3's conventions. PROCESS.md binds every task:
  pre-registration with refutation shapes; whitelist-grammar for every
  new decision-bearing parser; Tier-A arcs for every non-checksummed
  shipping surface (FOH, mixer/music streamer, legibility, gate
  assembly; the target-plane verifier is Tier A+ — judge path); Tier B
  ≥1 structural round for sim TUs (ai.c, target logic, vfx
  arg-threading). verify_m4.sh carries
  `port/sim/device/m4-freeze-manifest.txt` under the exact verify_m3.sh
  discipline: per-producer sha256+status, MANIFEST_SHA256 same-commit
  anchor, authoritative-mode hard-refusal while any producer is
  arc-pending, DEV mode exit 3, relay-prefix output contract.
- **Scope exclusions (PROVISIONAL, auto-adopted; Chase can
  override)**: target BUILDER (stage editor + share codes), replay
  save/load UI, multiplayer/online, credits screen, frame-by-frame
  debug UI — all outside PLAN §1's solo-parity bar (menus → VS vs CPU
  → target test). Registered here, never silently dropped.)

Tasks (dependency order; each < ~400-line diff where possible — tasks
1, 4 and 9 are mechanical/translation-heavy and pre-register likely
overruns):

- task 1 — **vfx seam widening, sim + capture side** (iter-44
  registered deferral; USER-VALIDATED by the S1 playtest: firefox
  flames + shine invisible). ml_events vfx events widen from name-only
  to the full drawVfx config (pos/face/extras) across the ~174 call
  sites / 112 move TUs + physics/article sites; the capture specs stop
  projecting vfx posts away and the affected captures re-record
  (STREAM-MATCH guarded as always); every affected cluster replay
  0-divergence; SIM CONFORMS unchanged.
  done-check: `bash port/sim/calib/check-vfx-seam.sh` → prints
  `VFX SEAM MATCH`, exit 0 (composes re-recorded capture byte-stability
  ×2, affected cluster replays, and `bash port/sim/check-sim.sh`).
  **DONE (iter 64, 2026-07-17) — committed form**: cold done-check
  `VFX SEAM MATCH` exit 0 (.loop/m4-task1-donecheck.log, ~7 min).
  Affected-cluster list MEASURED (grep + survey; the brief's guess
  widened): moves-* ×6 + article + asshort + physics + **hitdet** (18
  upstream sites — the largest single-file emitter) = 10 specs, all
  re-recorded ×2/STREAM-MATCH/0-divergence; + sim_boot.c entrance/
  start (main.js boot sites, capture-less, structure-verified).
  MlVfx + ml_drawVfx* emitters + ml_vfx_sink in ml_events.{h,c};
  cb_vfx canon in calib/canon.{h,c}; 193 sites translated; capture
  side pushes ctx.canon(cfg) at CALL time (snapshot semantics).
  2592 live full-config events replayed bit-exactly (physics
  wallBounce live ×8; asshort breakShield zero-live — honest-coverage
  note in AGENT-LOG). NEW rule instance (§M2 rule-7 corollary,
  measured twice): widening an OBSERVABLE widens read sets
  transitively — shieldDepletion pre 5→7 keys (+pos/face), puff stage
  projection 5→7 keys (+wallL/wallR); the strict marshals caught both.
  Teeth: nibble→11 (site occurrences exactly), name-only→34 (= all
  non-empty-vfx records), face-drop→11, hitdet-field-drop→14400;
  restores proven by 0-divergence re-replay (never `git checkout` —
  index-restore reverts unstaged work, lesson on record).
- task 2 — **renderer vfx + overlay/banner/background + IoU
  re-freeze**: port renderVfx/dVfx draw fns (circleDust's 4 seeded
  draws already chain), HUD renderOverlay, Ready-GO banner, background
  art into port/gfx; the browser IoU reference includes vfx on BOTH
  sides (capture-canvas.js reduced sequence extended, STREAM-MATCH
  guarded); IoU threshold RE-MEASURED-then-frozen as a NEW pin (the
  old pin retires WITH the exposure it measured; the new one is never
  loosened); render-on C replay still STREAM MATCHes g01.
  done-check: `bash port/gfx/check-render.sh` → prints `RENDER OK`,
  exit 0 (vfx-inclusive pins).
  **DONE (iter 65, 2026-07-18) — committed form**: cold done-check
  `RENDER OK` exit 0 (.loop/m4-task2-donecheck.log; min IoU 0.9032 over
  the 24-frame corpus). port/gfx gained gfx_vfx.{h,c} (all 45 dVfx arms,
  canvas-2d emulation with load-bearing NaN no-ops, render-LOCAL RNG,
  ml_vfx_sink installed BEFORE sim_setup_match), gfx_overlay.c (HUD +
  VFXGLYPHS1 atlas; lost-stock burst derived from stock decrements),
  gfx_bg.c (ink-suppressed background via new rast_ink_enable; live hsla
  boxFill). Executed frozen artifacts vfxdata-frozen.txt +
  vfxglyphs-frozen.txt (x2 byte-stable, cmp-tripwired). Corpus 16→24 +
  synthetic f150 injection (firefox*/shine* zero-live in EVERY golden —
  measured); old 0.91 pin retired with its exposure; NEW pin 0.88 after
  the pre-registered refutation-shape-(a) round (percentShake variance →
  capture render guard zeroes it, guard-class fix; full trail in
  AGENT-LOG iter 65 + .loop/m4-task2-teeth.log). SIM CONFORMS 8/8 +
  VFX SEAM MATCH re-verified (dust pass-through glue adopted). Task-3
  handoff: device scripts must thread --vfxdata/--glyphs into gfx_app.
- task 3 — **stage-surface legibility at device scale** (NEW seed from
  Chase's playtest: upstream-faithful ~1px strokes are illegible on
  the 1.54" panel). Deliberate DOCUMENTED device-scale adaptation: a
  minimum on-screen stroke width for stage surfaces/platforms,
  value measured on the panel then frozen (expected-render.json
  class); rasterization is not checksummed and the IoU masks are
  fg-plane only — sim untouched. Device screenshot judge asserts the
  legibility pin; this task is also the device rung for tasks 1-2
  (vfx live on screen).
  done-check: `bash port/gfx/check-device-render.sh` → prints
  `DEVICE RENDER OK`, exit 0 (with the legibility + vfx pins).
  **IN PROGRESS / BLOCKED (iter 73, 2026-07-18) — landed + verified**:
  legibility flag (--legible, GFX_LEGIBLE_MIN_DEV_PX 2.0 device px,
  device-only — host IoU untouched; twin-pinned + standing in-check
  differential witness), --vfxdata/--glyphs threaded through
  check-device-render.sh / check-device-opk.sh / mlfk.sh (frozen-sha
  pins per the iter-72 rule), arm build gains the vfx render TUs,
  judge-shot criterion-5 retired (reviewed pin change — bg art is
  ink-suppressed by design), arm-gcc-10.2 -Werror class fixes
  (hit_detection.h noreturn decl, overlay buffer), bit-identical
  render optimization round (batch blend prims + cov-window + circle
  table; render p99 13.20 -> 7.06 ms — full measurement trail in
  docs/research/device-perf.md iter-73). GREEN: RENDER OK (host, IoU
  min 0.9041 unchanged pins), SIM CONFORMS 8/8, STREAM MATCH on every
  device attempt, device shot BIT-IDENTICAL to host, full p99 15.51 <
  16.67, render p99 7.06 < 8.0, teeth all fired. **BLOCKED on the
  registered external stall class**: 8/8 paced attempts carried 1-3
  skips from isolated ~7-15 ms kernel-side stalls (frames ~1118-1290
  zone + scattered; adbd-poll/writeback/rig-machinery/swap all refuted
  by isolation probes — device-perf.md). skips==0 stays unweakened;
  task-8's attribution instrument is the closure path (driver may
  re-order it, or retry the done-check cold on fresh device state).
  ALSO REGISTERED: check-device-opk.sh's frozen gmenu2x nav is stale —
  the device inventory changed post-M3 (persistent meleelight.opk +
  4 other OPKs; two same-title MeleeLight entries make the frozen nav
  ambiguous) — failed LOUD exactly as designed; needs a re-measured
  nav + a unique evidence-OPK title (small dedicated iteration).
  check-device-audio.sh is NOT yet threaded with --vfxdata/--glyphs
  (out of task scope; cannot pass until task 6/14 threads it).
  **DONE (iter 74, 2026-07-19 — UNBLOCKED by task 8's attribution +
  mitigation)**: cold done-check `DEVICE RENDER OK (full p99 12.777
  ms, render-only p99 5.598 ms, sim p99 7.429 ms, present p99
  1.400 ms, skips 0/3600)` exit 0
  (.loop/m4-task8-task3-donecheck.log). skips==0 achieved (gate
  UNWEAKENED) via the run-harness low_bat_check quiesce (riglib
  rig_daemon_stop/restore, trap-covered + hard-gated restore); the
  first-ever-reached shot bit-compare then exposed the device musl
  lround shifting HUD glyph anchors 1 px (iter-38 round/trunc class,
  2nd instance) — fixed by fdlibm.c round/lround strong overrides +
  mathsweep rr/lr columns + nm assertion; shot now BIT-IDENTICAL
  host<->device. Full trail: AGENT-LOG iter 74 + addendum.
- task 4 — **ai.js structure-parallel C port, capture-replay
  verified**: port/sim/ai.c (+ helpers) translated from src/main/ai.js
  (1,575 lines) over the MlAiVal tagged-value model; verified by
  strict replay of the M2 ai-spec captures over g07/g08 — every runAI
  record bit-exact (post bank row, bk bookkeeping, seeded draws, zero
  wsViol) — plus a fresh re-record proving the rig unchanged.
  done-check: `bash port/sim/calib/check-ai-replay.sh` → prints
  `AI MATCH`, exit 0.
  **DONE (iter 75, 2026-07-19) — committed form**: cold done-check
  `AI MATCH` exit 0 (.loop/m4-task4-donecheck.log). port/sim/ai.{h,c} —
  all 22 ai.js functions over MlAiSim (god-module slice; tagged bank
  writes with exact literal tags, rule 16; curentAction typo field as
  slice state; fdlibm transcendentals incl. fd_tan; quirks q1-q8
  verbatim); NEW spec-aiport.js (runAI-only wrap, read-set pre
  projection in args, M2-parallel post {bank,bk,rng}, recon wsViol==0,
  153-preset rule-11/12 sweep on a 0x0badf00d mulberry32) + replay_ai_
  port.c (strict marshal, chained RNG, --cover arm table) + expected-
  capture-aiport.json + check-ai-replay.sh; FORMAT.md "The aiport spec".
  0 divergences over 7571 records (3510 live + 153 sweep per golden);
  ZERO divergence-driven fix rounds. Honest coverage MEASURED: 61/64
  arms hit per golden; the 3 zero-hit arms + 5 more surfaces are
  measured-DEAD upstream (incl. the ai.js:1254 curentAction write —
  :228/:1253 contradiction — and REVERSEUPTILT's :279 completion;
  FORMAT.md lists all). Teeth: nibble→1, live-draw drop→1104 cascade,
  bk-write drop→3/1, cta-serializer→3663 (=every record), q2-typo
  "fix"→1 (sweep witness); T4-as-preregistered amended on the measured
  dead site (AGENT-LOG). Regressions: SIM CONFORMS + AI BRIDGE OK
  (bridge path untouched). M2 ai spec/captures/AIBRIDGE1 byte-frozen.
- task 5 — **live CPU integration, bridge retired from the live
  path**: sim_main/gfx_app grow a live-AI mode (C runAI at the
  update(i) site; bank-row alias + post-runAI slot re-copy semantics
  preserved); g07/g08 replay with LIVE AI (no AIBRIDGE1) → UNCHANGED
  verify-stream.js STREAM MATCH vs the frozen streams. PROVISIONAL
  coverage extension: two NEW CPU traces at difficulty 1 and 9,
  recorded browser ×2-identity into port/goldens-m4/ + frozen, then
  live-replayed (rule-16 class: new goldens widen value domains —
  re-survey). check-sim.sh untouched.
  done-check: `bash port/sim/check-ai-live.sh` → prints
  `AI LIVE CONFORMS`, exit 0.
  **DONE (iter 81, 2026-07-19) — committed form**: cold done-check
  `AI LIVE CONFORMS` exit 0 (.loop/m4-task5-donecheck.log). The runAI
  site is two-armed via the `ml_sim_runai_live` POINTER SEAM (sim.h;
  constructor-installed by NEW port/sim/sim/sim_ai_live.c, linked only
  alongside ai.c — the frozen check-sim.sh TU list never sees the new
  symbols and its binary still refuses --cpu without --ai-bridge, the
  in-check M2-contract witness). check-sim.sh BYTE-UNTOUCHED (its
  sha256 is now PINNED inside check-ai-live.sh) — the iter-81 brief's
  "edit check-sim.sh" instruction was refused per HARD RULE 3 + this
  section's conventions (AGENT-LOG iter 81). GameState bank [4]→[4][8]
  (ai.js:357 reads [i][1]); alias slot-0 re-copy common to both arms;
  --ai-cover arm-table diagnostic. NEW goldens (browser ×2-identity,
  M0 quality contract checked MECHANICALLY by
  port/goldens-m4/check-quality.js at record time): m01
  falcon/CPU-marth(d1)/ystory seed 8114 rngCalls=59 (first live CPU on
  a MOVING-PLATFORM stage) · m02 falcon/CPU-fox(d9)/dreamland seed
  8109 rngCalls=411; recorder = port/goldens-m4/record-m4.sh +
  freeze-stream-m4.js (oracle/harness bytes reused BY PATH; M0
  freeze format verbatim; oracle/ read-only). All FOUR CPU goldens
  replay LIVE to STREAM MATCH (unchanged verify-stream.js), ZERO
  divergence rounds. Rule-16 verdict: no capture adoption — the live
  path has no JS→C marshal; the full-trace frozen-stream oracle is the
  stronger check (no trap fired). Coverage delta measured (--ai-cover):
  FOX_REACT/TILT/TURN/SHDL_*/RESPAWN_* + GEN_TW_CLEAR newly live;
  marthAI's whole action block is `pdiff>=2`-gated so d1 proves its
  OFF side (MARTH_* zero-live registered honest). Pre-reg amendments
  (structural, evidenced): m01 fox→falcon (laser damage carries no
  DAMAGE state), pstadium→ystory (wide blastzones suppress the only
  d1 KO source), ladder extension — 24-browser-run cap held exactly.
  Teeth 5/5 (.loop/m4-task5-teeth.log). gfx_app live mode = registered
  deferral to the device tasks (frozen-pinned option surface).
- task 6 — **mixer fidelity + real play-ids + stop-path coverage**
  (iter-57 seeds): real per-play ids through the mixer (mv_howl_play_id
  backed by voice ids; .stop routing for furaloop/shieldbreakercharge),
  looped-SFX sprite windows per SND1; the OFFLINE deterministic render
  differential (host ×2 byte-frozen + device offline render cmp
  byte-exact); stop-path LIVE witness — a committed scenario measured
  to fire .stop through the full path (g01 fires zero .stop events:
  the registered coverage hole closes here, not by assertion).
  done-check: `bash port/gfx/check-mixer-fidelity.sh` → prints
  `MIXER FIDELITY OK`, exit 0.
  **DONE (iter 82, 2026-07-19) — committed form**: cold done-check
  `MIXER FIDELITY OK (goldens=11 diff=bit-identical maxvoices=9
  steals=2 s01stops=4)` exit 0 (.loop/m4-task6-donecheck.log; composes
  check-sim.sh). Differential: sim_host_snd (snd_events_tap.c
  constructor TU; STREAM-MATCH-guarded schedules) → snd_render.c (the
  snd_mixer.h math verbatim, offline) vs snd_reference.js (INDEPENDENT
  impl from SND1 + vendored-howler semantics; unlimited + capped-8
  modes) — BIT-IDENTICAL 11/11 goldens vs the capped reference and
  9/9 vs unlimited where concurrency ≤ 8 (MEASURED: g06/m02 peak 9
  voices, 1 steal each = the registered device-vs-browser exposure;
  zero divergence rounds). Real ids: the id plane lives ONCE in
  ml_events.c (ml_howl_play_id = 1000 + play count, howler-parallel,
  off-surface; ml_sound_stop_id enqueues identical token bytes + feeds
  ml_snd_stop_id_sink; mixer stop(id) = howler semantics incl.
  stale-id no-op; calib ml_howl_id_oracle preserves the marth sbid
  injection; sim_tick.c's dead counter = registered cleanup, no-edit
  window). FOUND+FIXED (zero-coverage latent bug): as_shieldDepletion's
  SHIELDBREAKFALL dispatch was note-only — a depletion break left the
  victim IN GUARD; + SHIELDBREAKFALL.land's missing-normal trap
  contradicted upstream's 2-arg landType-1 call (god-array jank →
  DX_NUM NaN arm; sweep's 3-arg form kept). NEW GOLDEN s01
  (port/goldens-snd/ — location = concurrent-review constraint; fold
  into goldens-m4 post-arc, registered): crafted deterministic trace
  (gen-s01-trace.js), browser ×2 first-attempt, quality contract,
  C replay bit-exact 3600/3600 rngCalls=57; ALL FOUR stop arms live
  (NSG release 191 · NSG auto-122 697 · hitdet-FURAFURA 701 ·
  FURAFURA-wake 1419). Teeth 6/6 incl. stop-id-skew (id routing
  load-bearing) + steal-flip on g06's real steal. Regressions:
  ASSHORT/MOVES SHARED/MOVES marth/HITDET MATCH + RENDER OK.
  Honest: NSA charge arms + FURASLEEP variant zero-live; device rung
  (audio-on + offline-render cmp on device) = task 7/14 deferral.
- task 7 — **music streaming from SD**: the M1 music PCM packed to
  device SD; 2×64 KB double-buffer streamer TU feeding the mixer's
  music voice (Start/Loop sprite windows from sounds.json); device g01
  full match with render + SFX + MUSIC live: p99 < 16.67 ms,
  underruns == 0, buffer-starve counter == 0, skips == 0.
  done-check: `bash port/gfx/check-device-music.sh` → prints
  `DEVICE MUSIC OK`, exit 0.
  **DONE (iter 87, committed form)**: cold done-check `DEVICE MUSIC OK
  (full p99 13.635 ms, underruns 0, starves 0, refill-read p99
  1.594 ms, skips 0/3600)` exit 0 (.loop/m4-task7-donecheck.log).
  snd_mixer.h music channel (ring 32768/chunk 16384 = PLAN §7 2x64 KB;
  Start-once→Loop-repeat sprite program from sounds.json ms windows,
  floor(ms*441/20); ZOH 2x; Q8 per channel pre-clamp; past-EOF =
  silence — the fod quirk verbatim; no-music fill byte-identical,
  proven by the cold MIXER FIDELITY regression); gfx_app.c pthread
  reader (25 ms poll, one 16384-frame chunk when space >= chunk, wr
  under platform_audio_lock, prefill before audio start, --music-lat
  sidecar, separate `gfx_app music:` stderr grammar); offline twin in
  snd_render.c + independent snd_reference.js --music-track. NEW
  `bash port/gfx/check-music-fidelity.sh` → `MUSIC FIDELITY OK
  (goldens=12 tracks=8 diff=bit-identical wraps=2 eofsilence=1)` (12
  goldens with stage-track music + menu-wrap/fod-EOF/targettest
  synthetic legs, 8-track sha pin table, teeth T1-T4 + grammar).
  Device: music PCM staged on REAL SD, sha-verified + T5 corruption
  tooth; refills 80, musout == cbs*512 exact, sidecar refill-read
  p50/p99 0.413/1.594 ms per 64 KiB vs the 743 ms half-ring tolerance;
  SD dd ~21.2 MiB/s. Music selection seam = RENDER-PLANE (main.js:1342
  stage→track switch, no RNG — pre-reg survey); menu/targettest
  SELECTION deferred to FOH tasks 9-12 (windows covered here).
  Registered residual: check-device-music.sh not yet in riglib
  RIG_SCRIPTS (frozen surface) — queue with the Tier-A arc.
- task 8 — **skip-burst attribution instrument** (iters 54/57/62
  registered class: transient single-frame sim spikes, burst form
  measured; kernel/adbd-vs-sim-internal attribution OPEN). Diagnostic
  instrument (Tier B): over-budget frames get an attribution record
  (per-phase ns breakdown, retrospective ring, /proc snapshots
  host-pulled); PRE-REGISTERED run matrix (cold/warm × audio/music)
  with hard cap + refutation shapes; ATTRIBUTION VERDICT recorded in
  the AGENT-LOG entry. Gate posture unchanged — skips==0 is never
  weakened, no wider retry budgets.
  done-check: `bash port/gfx/check-skip-attrib.sh` → prints
  `SKIP ATTRIB RECORDED`, exit 0 (asserts the verdict needle in the
  AGENT-LOG entry, M2CAL-report precedent).
  **DONE (iter 74, 2026-07-19 — driver RE-ORDERED forward + re-specced
  the done-check to `bash port/sim/device/check-skip-attrib.sh` →
  `SKIP ATTRIB OK`, recorded in the iter-74 brief/entry)**: cold
  done-check `SKIP ATTRIB OK (arm=sampler, skips=3/3600, events=37,
  stream MATCH)` exit 0 (.loop/m4-task8-donecheck.log).
  **SKIP ATTRIB VERDICT: (a)** — the stall class is `low_bat_check`
  (FunKey OS battery poller: 2 s shell loop, ~8 forks + blocking
  AXP20x i2c sysfs reads per wake; event comb every ~123 frames =
  2.05 s, phase-random). Matrix: live arms 2-3 skips / 33-34 events;
  quiesce arms ×2 = 0 skips, comb gone. Instrument: gfx_app --attrib
  (per-frame mono/raw clocks + rusage), sk_sampler (fork-free 250 ms
  /proc counters), correlate-skips.js (whitelist grammars), pre/post
  kernel+pidtable snapshots. Mitigation landed in
  check-device-render.sh (task-3 unblock proven). NEW gotcha classes:
  busybox start-stop-daemon -K -x is a NO-OP for script daemons +
  pidof is comm-blind (quiesce = comm-scan + kill-by-pid); adbd
  merges device stderr into pulled streams; bash-3.2 same-statement
  `local` expansion + expansion-error-exits-0-under-EXIT-trap (the
  SKA_OK fail-closed guard). Full data: AGENT-LOG iter 74 + addendum,
  device-perf.md iter-74 tables.
- task 9 — **FOH core + menu flows, host**: port/foh/ screen machine —
  start screen → main menu → CSS (5 chars, human/CPU + difficulty
  slider) → stage select → match launch; options screens for the
  gameSettings the sim consumes; faithful flow graph per upstream
  gameMode dispatch (rewritten). Committed flow scripts + frozen
  structural transition traces + the match-launch bridge assertions
  (conventions block (a)-(c)); host screenshots byte-stable ×2.
  done-check: `bash port/foh/check-foh-flows.sh` → prints
  `FOH FLOWS OK`, exit 0.
  **DONE (iter 88, 2026-07-19) — committed form**: cold done-check
  `FOH FLOWS OK (flows=4 shots=11 bridges=3 streams=MATCH teeth=6)`
  exit 0 (.loop/m4-task9-donecheck.log). port/foh/{foh.h,foh.c,
  foh_render.c,foh_font.c,foh_app.c,judge-foh-trace.js,
  check-foh-flows.sh,flows/} — rewritten machine over the platform
  seam + raster (headless backend = the check's backend; foh_tick
  consumes PlatformInput, device task 10 swaps the feeder to
  platform_poll); flow graph + selection semantics cited per edge
  from upstream (foh.h); excluded/deferred menu entries visible +
  REFUSED with structural events (9 registered tokens). Flows:
  f01-vs-g01 (VS path → FULL 3600-frame stream == frozen g01, via
  UNCHANGED wrap-run/verify-stream — exceeds the (c) prefix bar),
  f02-cpu-m01 (CSS CPU toggle + slider d1 → LIVE C-AI stream ==
  frozen m01), f03-options (sim-consumed settings edits →
  BRIDGE-STATE GameState witness), f04-nav (all refusals + back
  edges + bhold). Frozen: flows/*.expect + *.bstate.expect
  (hand-reviewed then frozen; judge = whitelist grammar with the
  PINNED 15-edge flow graph + T-chain continuity + LAUNCH↔manifest
  cross-bind). MEASURED brief amendment: upstream CPU slider domain
  is 1-4 (css.js:316-329) — d5/d9 goldens are unreachable through
  the faithful UI (m01/d1 is the CPU bridge). Deferrals registered:
  SSS RANDOM slot, menu SFX/music selection + device FOH (task 10),
  persistence (task 13), palettes/tags/versusMode toggle. Teeth 6/6;
  zero divergence rounds; sim/gfx TU bytes untouched (skip proofs on
  record).
- task 10 — **FOH on device**: menus rendered at 240×240 through the
  platform seam, navigated by fk_input scripts on the FunKey;
  per-screen screenshots structurally judged; a full flow boots a live
  match (stream prefix vs frozen g01); OPK launcher enters the FOH
  (not the direct-match path).
  done-check: `bash port/foh/check-device-foh.sh` → prints
  `DEVICE FOH OK`, exit 0.
  **DONE (iter 93, 2026-07-19) — committed form**: cold done-check
  `DEVICE FOH OK (flows=5 shots=13 bridge=1 states=3 opk=1
  p99=13.585ms skips=0 underruns=0 starves=0 starts f01-vs-g01=281
  f02-cpu-m01=16 f03-options=23 f04-nav=39 f05-vs-g03=14 teeth=9)`
  exit 0, FIRST full device attempt (.loop/m4-task10-donecheck.log).
  The review-88 M3 DEFER-BOUND binding honored: all 5 committed flows
  driven through fk_input → uinput → SDL keysyms → platform_poll
  (device argv pinned `--input poll`), judged vs the SAME frozen
  flows/*.expect by judge-foh-trace.js + NORMALIZED-SEQUENCE
  byte-equality (normalize-foh-trace.js — frame elision only; swap
  teeth prove the A/B-swap / direction-reversal / START-drop kill
  chain at the judge). Shots = BYTE-EXACT vs host twin refs (13/13;
  tick-indexed pre-input shots + q-marker settled-state shots).
  f01 FOH-launched match: STREAM MATCH 3600/3600 vs frozen g01 ON
  DEVICE with render+SFX+music (p99 13.585 ms, skips 0, underruns 0,
  starves 0). f02/f03/f05 BRIDGE-STATE byte-exact vs frozen. Menu
  SFX/music wired (foh.c snd tokens, cited; menu track at
  title→menu-top, stage track at LAUNCH per main.js:1341-1360;
  starts/stops == twin per flow; menu.pcm NEW pin, sndpack/bf twin-
  pinned). SSS RANDOM measured SEEDED (stageselect.js:80-84 +
  Math.random = the rngCalls plane) → registered exclusion, slot 6
  visible-but-refusing (`refused random`; f04 extended + re-frozen
  via the designed channel; judge sha re-pinned same commit). OPK:
  mlfk-foh.sh + unique-title evidence OPK (mksquashfs 4.4 only,
  unsquashfs-verified) mounted on device, launcher entered the FOH
  from the mount (boot marker bin == stamp), removed at cleanup; play
  install untouched; frontend-nav launch = task 14 (iter-73 note).
  foh_device joined ARMBINS/riglib (srchash roots += port/foh).
  Registered: foh_dev --bridge live + mlfk-foh.sh live mode are
  review-only this iteration (task 14/acceptance own their exercise).
  Regression: FOH FLOWS OK teeth=16 cold. Caps: device attempts 2/3
  (one host-side pin death), paced legs 6/18, arm rebuilds 1/4,
  browser 0.
- task 11 — **target test, data + sim plane (host)**: NEW pipeline
  stage `targets` (executed-JS extraction of the 10 authored
  target-test stages — box/target/polygonMap machinery that M1 pinned
  empty for VS stages; expected.json extended measured-then-frozen);
  structure-parallel target-play sim logic at port/sim/target/ (incl.
  the currently-TRAPPED stage-damage hq rows + target-mode tick);
  NEW target goldens browser-recorded ×2-identity into
  port/goldens-m4/ (harness bytes verbatim, oracle/ untouched), frozen
  spec-v1 stream + the separate target-plane stream; C sim replays
  them bit-exact host-side.
  done-check: `bash port/sim/target/check-target-sim.sh` → prints
  `TARGET SIM CONFORMS`, exit 0.
  **DONE (iter 94, 2026-07-19) — committed form**: cold done-check
  `TARGET SIM CONFORMS (2 goldens: t01 t02; leaves=718 probe=ok
  teeth=6)` exit 0, FIRST attempt
  (.loop/m4-task11-check-target-sim-run1.log). Pipeline stage `targets`
  (TTAB1, FORMATS.md NEW §6): extractor.entry.js +=
  `stages/targetstages/tstages` → `window.__targetStages` (measured
  import-clean: only Vec2D/Box2D — no externals stubs needed), new
  lib/targets-schema.js + stages/targets.js + lib/targets_check.c +
  lib/targets-dump.js + check-targets.sh (`TARGETS OK`); expected.json
  gained the measured-then-frozen `targets` section (10 stages, 85
  boxes, 90 targets, 60 ledges, 2320 f64 / 280 i32); the M1 GATE is
  EXTENDED not weakened — verify_pipeline.sh runs check-targets.sh and
  a third round-trip block (38832 + 412 + **718** leaves, `PIPELINE OK`
  re-verified cold). Sim plane `port/sim/target/`: target_play.{c,h}
  (MlTargets module state; targetHitDetection/hitTargetCollision/
  articleTargetCollision/destroyTarget/targetTimerTick/startTargetGame
  verbatim incl. the DOUBLE-DESTROY quirk), target_main.c (sim_host_target
  — emits BOTH streams + TFIN), target_hq_probe.c. New goldens
  port/goldens-m4/{manifest-target.json, run-target.js, record-target.sh,
  freeze-target.js, verify-target-stream.js, check-target-quality.js,
  wrap-target.js, gen-t0{1,2}-trace.js}: **t01** fox/tstage1 seed 4801
  (2 LASER/article breaks, maxArticles 1) · **t02** falcon/tstage2 seed
  4802 (2 MELEE breaks, 0 articles); both browser-recorded ×2-identical,
  quality-passed, frozen spec-v1 player stream + the SEPARATE
  target-plane stream; C replays BOTH bit-exact 3600/3600.
  TWO MEASURED REFUTATIONS of this task's own text (do NOT re-litigate):
  (a) **polygonMap does not exist** on any authored stage (VS or target)
  — builder/encode plane only, pinned ABSENT; (b) **no authored stage
  carries damageType** (grep over all 16), so the "untrap the
  stage-damage hq rows" premise is FALSE for authored data — the path
  stays legitimately zero-live, sim_tick.c:355's VS trap STAYS, target
  ticking carries its own same-shape trap, and the already-translated
  CONSUME path (hit_detection executeHits/executeRegularHit) gains
  mechanical coverage via the NEW standing `target_hq_probe` (drop-arm
  tooth proven). Regressions: `SIM CONFORMS` 8/8 + `PIPELINE OK` cold.
  Sim-TU edits were visibility-only (interpolatedHitCircleCollision and
  interpolatedArticleCircleCollision static→extern, matching
  targetplay.js's imports) + ML_MAX_LEDGES 8→16 (capacity; targetstage8
  has 16 ledges) — behavioral identity proven by the bit-exact 8/8.
  Gotcha class: a static key-set grep MISSED targetstage9's
  `ledgePos :` (space before colon) — the schema's own exact-key-set
  hard-throw caught it on the first executed walk, and it is carried
  verbatim as an OPTIONAL key (the fdest-quirk faithfulness precedent);
  measure with the EXECUTED walk, never the source grep alone.
  **HARDENED (iter 96, review-94 round-1 closure)**: shared strict
  manifest validator (validate-target-manifest.js, every consumer),
  frozen-metadata + player-sibling seal binding + exact schema in
  verify-target-stream.js, exact-token wrap grammar, captured
  page-counter frame numbering, nested Vec2D/Box2D exact-key schema,
  ONE ML_MAX_TARGETS 10 pin (triage's "9" refuted — measured authored
  max is 10), x2 browser-identity refusal, frozen-side teeth T7-T12
  (teeth 6->12); frozen artifacts byte-untouched — AGENT-LOG iter 96.
- task 12 — **target test, FOH + device**: target-test select flow +
  timer/records HUD in the FOH; every target golden replayed ON DEVICE
  with render + audio live, streams host-judged (both verifiers), p99
  < 16.67 ms.
  done-check: `bash port/sim/target/check-device-target.sh` → prints
  `DEVICE TARGET CONFORMS`, exit 0.
  **DONE (iter 99, 2026-07-20) — committed form**: cold done-check
  `DEVICE TARGET CONFORMS (goldens=2 flows=2 shots=4 fbwit=4
  p99=13.650ms skips=0 underruns=0 starves=0 starts
  f06-target-t01=15 f07-target-t02=31 teeth=6)` exit 0
  (.loop/m4-task12-devtarget-run4.log; per-leg p99 12.499/13.650 ms).
  FOH grew the REAL target-select screen (foh.h graph 15→18 cited
  edges; `targettest` refusal RETIRED, `addcode` refusal NEW; grid
  cursor + upstream shoulder-wrap char select on the shared
  characterSelections[0]; TLAUNCH record + judge/normalizer grammar,
  judge sha re-pinned same commit ×3 checks) + the timer HUD
  (extracted gfx_render_overlay_timer == renderOverlay(false)) +
  the honest records line ("PERSONAL BEST --:--:--" ≡ fresh-boot -1;
  READ = task 13 registered; medal/dev-time display needs a pipeline
  extension, registered). Flows f06/f07 (frozen .expect +
  TBRIDGE-STATE) drive fk_input→uinput→SDL→platform_poll on device;
  foh_app/foh_dev `--bridge tstate|tverify` replay the FOH-launched
  frozen t01/t02 BIT-IDENTICALLY (the (c) bar exceeded, full streams,
  host AND device, judged by the UNCHANGED verify-stream.js +
  verify-target-stream.js); gfx_target.c renders the mode-5 sequence
  over TTAB1 reusing the IoU-checked player/article passes; music =
  targettest track (NEW pcm pin 0c922c6f…; switch at the TLAUNCH
  seam — registered delta). finishGame is REAL (tp_finish_game +
  tp_finish_hook; post-finish ticks faithful) — **t03 REFUTED**:
  targetstage9's single target is topologically unreachable in the
  authored geometry (measured; sealed region; stale devRecord), so
  the finish arms + the DOUBLE-DESTROY quirk went mechanically live
  via the NEW target_finish_probe (check-target-sim [4b], probes=2)
  instead of a golden; `foh_dev tfinish:` asserted ABSENT on
  committed legs. Capacity class fix: ML_MAX_LABELLED_SURFACES 96
  (targetstage9 concatenates 95 labelled surfaces; SIM CONFORMS 8/8
  cold proves identity). rig_arm_build now produces its own TTAB1
  recipe input (class fix); check-device-target.sh joined
  RIG_SCRIPTS. Registered onward: records persistence (13),
  check-device-foh paired-pin rerun + START-quit endGame + live
  finish exercise (14/acceptance).
- task 13 — **persistence to SD**: settings + target-test
  records/medals at /mnt/mlfk-data through ONE read/write chokepoint
  (atomic write-rename; corrupt/missing file = loud reset-to-defaults,
  never silent zeroes — the qjs getCookie lesson inverted for OUR
  surface); survives power-cycle (two-session device check over ADB).
  done-check: `bash port/foh/check-device-persist.sh` → prints
  `PERSIST OK`, exit 0.
  **DONE (iter 100, 2026-07-20) — committed form**: cold done-check
  `PERSIST OK (sessions=2 powercycle=reboot bootwait=12s legs=5
  pulls=4 roundtrip=byte-exact record=00:14.50 resets missing=1
  loud-corrupt=2 teeth=9)` exit 0 (.loop/m4-task13-donecheck-run2.log;
  attempt 1 died at the reboot DISPATCH — a raw `adb shell "… &"` is
  killed by this old adbd's teardown before the detach takes; the
  measured house detach recipe fixed it, sessions A evidence all
  green both attempts). ONE chokepoint `port/foh/foh_persist.{h,c}`:
  MLFKPERSIST1 (55 LF lines, SUM=sha256 seal, doubles as hex16 bit
  patterns — strtod-free, the iter-38 musl class structurally out),
  strict anchored load with LOUD reset lines (`foh_persist: reset
  cause=missing|version|corrupt detail=…`), atomic tmp+fsync+rename
  save (rename = the only publish; dir-fsync FAT-tolerant), dir =
  MLFK_PERSIST_DIR override else /mnt/mlfk-data. Persists the
  FOH-editable sim-consumed settings subset {turbo,lCancelType,
  tapJumpOff[4]} (settings.js:44-56 defaults) + targetRecords[5][10]
  (-1 fresh, targetplay.js:40); medals NOT persisted — upstream
  cookies only records, medals are DERIVED (giveMedals,
  targetplay.js:165-174; medal/dev-time DISPLAY stays the registered
  pipeline-extension deferral). Save points wired in BOTH drivers:
  options B-exit (gameplaymenu.js:29-33) + the finishGame record arm
  (main.js:1431-1445 improve-or-first) via tp_finish_hook — REAL
  wiring, exercised by the NEW `foh_dev --tooth-persist-finish` arm
  (a genuinely completing run is authored-unreachable, iter-99
  refutation; live finish = acceptance; honest-coverage note in the
  iter-100 entries). Records READ path (task-12 deferral CLOSED):
  FohState.targetRecords → render_tss PERSONAL BEST in the upstream
  format (integer-centisecond C form — registered delta), device
  shot byte-exact vs host twin fed the SAME persisted bytes, and !=
  the defaults control (display load-bearing). HERMETICITY: every
  check run gets a fresh MLFK_PERSIST_DIR (check-foh-flows fresh_
  persist; device checks per-leg tmpfs dirs) — cold FOH FLOWS OK
  (flows=7 … teeth=18) with ALL frozen expectations byte-untouched,
  zero re-freezes. Device file bytes == host-constructed twin at
  every pull (4/4); byte-identical across a REAL measured reboot.
  Teeth 9: corrupt-sum/version/NaN-domain/truncation → exact loud
  resets; torn-tmp inert; read-only-dir save dies with the real file
  byte-unchanged; record-regress no-op; device corrupt → loud reset
  + recovery save == the authored-defaults file + control shot.
  Paired-pin/mechanical edits registered onward to task 14:
  check-device-foh.sh (per-leg persist env + the iter-99-broken twin
  recipe repaired: targets stage + target_play/gfx_target/ml_targets
  TUs) and check-device-target.sh (persist env) — NOT cold-rerun
  here (iter-99 precedent; the gate + driver ritual own it).
- task 14 — **full-game trace suite + M4 exit-gate assembly**:
  `port/sim/device/verify_m4.sh` per the §Commands concretization —
  freeze-manifest discipline first, then [1] full-game conformance on
  device at 60 fps with audio+music (8 match goldens with g07/g08 on
  LIVE C-AI + all port/goldens-m4/ traces), [2] menu flows on device,
  [3] OPK frontend launch into the FOH. NOTE for the phase-advance
  iteration: a gate pass is followed by the human-gate sentinel
  `LOOP STOP: m4-complete — awaiting Chase acceptance playthrough`
  (LOOP §F-advance.3/§H) — the GATE does not print it (driver duty).
  done-check: `bash port/sim/device/verify_m4.sh` → prints
  `M4 GATE OK`, exit 0.

- registered follow-up (NON-gate-blocking; driver ruling 2026-07-26 on
  the review-109 arc CAPPED at round 9): centralize the per-producer
  deadman bodies (check-device-render/music/foh/target/skip-attrib)
  onto riglib's owned-claim recovery machinery (round-6 H3's class
  fix). Bar-(b) hygiene only — reviewer confirmed "no bar-(a) defect"
  five consecutive rounds (nothing can pass bad evidence as clean;
  residual = loud duplicate/stopped-daemon states needing human
  reconciliation). Touching it invalidates five reviewed producer
  pins → its own arc, scheduled AFTER the M4 gate. Evidence:
  .loop/review-109-triage.md §ARC STATUS.

- registered residual (NON-gate-blocking; driver ruling 2026-07-26,
  iter 114): "pacing-contention residual" — rare (≈15/43k frames)
  component-B sleep-window delays up to ~6.4 ms from single-core
  contention (NOT wake latency — hybrid sleep landed; NOT swap —
  eliminated iter 113; NOT paging — majflt 0). Skip requires
  collision with a content sim tail (13.8-15.2 ms); two consecutive
  final-bytes passes ran zero-skip (runs 9/10). Standing gate risk:
  worst vanilla p99 16.104 vs 16.670 (0.566 ms). Reopen trigger: any
  skip in a gate-context run → attribute via MLFK_FULLGAME_ATTRIB=1
  (never reroll); candidate next levers (evidence-gated): identify
  the contending thread via sampler windows, audio-callback
  batch/period audit. Evidence: .loop/m4-t114-lateclass-run9.log,
  AGENT-LOG iter-114.
  - addendum (driver, 2026-07-26, post-iter-114): writer proposed a
    SPIN_NS 3→2 ms retune + one discriminating pass (arithmetic: 2 ms
    clears both observed collisions ×1.4 margin, cuts spin burn
    18%→12%). DENIED for now — the experiment cannot discriminate:
    run-to-run p99 noise on IDENTICAL bytes is ~0.4 ms (run-9 s01
    16.455 vs run-10 16.075), same order as the +0.3-0.8 ms effect it
    would test; one pass has no statistical power, and the skips=0 pin
    is met twice. The R3 "p99 rose 10/12 legs" signal is itself within
    drift. If the class REOPENS (gate-context skip): the pre-named
    lever is SCHED_FIFO RT priority for the frame loop (+ audio thread
    ranked above it; music reader stays CFS) — it subsumes both the
    contention mechanism and the spin-cost question, and post-gate it
    may allow SHRINKING the spin; the 2 ms retune is the cheap second
    lever if RT is refused. Escalation ladder above 3 ms is RETIRED
    (writer's own evidence: more spin feeds the contention).
  - OWNER RULING (Chase, 2026-07-26) — SUPERSEDES the "denied for
    now / reopen-trigger" framing above: the jitter-removal increment
    is SCHEDULED as the FINAL work item, to run AFTER the M4 gate and
    Chase's acceptance playthrough. Scope: (1) SCHED_FIFO RT priority
    for the frame loop (audio callback thread ranked above it; music
    reader stays CFS; sleep-every-frame + rig deadman = runaway
    guard); (2) the writer's SPIN_NS 3→2 ms discriminating retune —
    with gate pressure off, run ENOUGH passes for statistical power
    against the ~0.4 ms run-to-run noise floor (single passes cannot
    discriminate), and under RT the spin may shrink further or go
    away. Goal: remove the pacing-contention jitter class entirely,
    not just meet the pin. NOTE: touching pace.h/foh_dev.c after the
    gate invalidates m4-freeze-manifest pins — the increment carries
    its own done-check (fullgame suite green on new bytes + the
    late-start component evidence) and a manifest re-pin, per the
    riglib re-pin precedent.

- registered follow-up (NON-gate-blocking; iter 115 finding):
  check-device-audio.sh host-build TU list is STALE since the M4
  render-plane TUs (missing gfx_vfx.c/gfx_overlay.c/gfx_bg.c that
  check-device-render.sh:694,709 carries) — link dies on 7 _gfx_*
  symbols, the check has been un-runnable since; NOT in verify_m4's
  producer set, M3 closed/ratified, so gate-inert. Fix = 2-line TU
  update through its own arc + one device run + m3-manifest re-pin;
  schedule with (or after) the post-gate jitter increment. Evidence:
  .loop/m4-t115-reg-audio.log.

- registered residual (iter 117): judge-corruption-grammar exactness —
  the PRESENT column has no single-valued/alias arm (reproduced:
  present:=1000 on all rendered rows → rc 0, p99 13.903→12.808; on a
  shifted s02, a FAILING 16.704→15.161). Both round-9b remedies
  measured WRONG (0.0003 fraction leaks ≤3333-row artifacts;
  distinctPresent>=2 false-rejects genuine valve-tim.txt 1/1). Fix =
  row-count-gated single-valued test (POP_MIN_ROWS design), own arc +
  teeth + 5-site re-pin fan-out; schedule post-gate with the jitter
  increment. Evidence: .loop/review-117-triage.md §9.

## M4 ACCEPTANCE PUNCH LIST (owner playthrough 2026-07-27; ratification pending; AGENT-LOG iter-120)
- A1 (P0) FOH look-fidelity: menus match upstream meleelight (ikneedata
  look) at 240×240 — reference-capture every upstream screen via the
  browser harness first, then per-screen restyle with side-by-side
  judges; flows/judges/manifest re-arcs + re-pins ride each increment.
- A2 (P0) target-test launch crash from the PLAY path (evidence flows
  were green — play-OPK-specific). Diagnose from device, fix, arc.
- A3 (P1) L shoulder = shield/air-dodge (currently unbound).
- A4 (P1) control-style system: box + normal, switchable, normal
  default (owner ratified semantics 2026-07-27).
- A5 (P1) Controls screen selection wired (candidate: style selector).
- A6 (P2) Audio options tab functional. **MEASURED 2026-08-03: ESSENTIALLY
  ALREADY BUILT — this row was stale.** `FOH_OPT_AUDIO` is a complete live
  screen: enum `foh.h:338`, token `foh.c:34`, entry arm `foh.c:242`,
  `step_opt_audio` `foh.c:938-999` (B-exit, row nav, ±0.1 volume carrying
  upstream's float dust), render `foh_render.c:1810-1894`, judged edges
  `judge-foh-trace.js:88-89`, and a committed flow leg with a
  `SHOT options-audio`. RESIDUAL is at most the live-mixer bus push noted at
  `foh.c:975-977` as a pending cross-lane patch — NOT a screen build. A6 is
  therefore the finished TEMPLATE that A7 copies, not work that shares A7's
  remaining cost.
- A7 (P2) Credits functional.
- A8 (P2) reuse audit report to owner.
Sequence: punch list → owner re-play/ratification → post-gate window
(jitter LAST per §rulings).
- A9 (rides A1) portraits/pictures: REUSE upstream's own images for
  character select + stage select (currently no pictures anywhere).
  They live in the built dist/ and the M1 pipeline precedent covers
  executed/derived assets (Nintendo-derived: gitignored build output,
  PRIVATE USE ONLY, never committed — LICENSING hygiene). May be
  subsumed by A1 fidelity work; decide per-screen.
- A10 (decision AFTER A1; owner) battle-mode entries spectate/p2p/
  server = upstream NETWORK features (deepstream) — dead on device.
  Options to present post-A1: hide vs keep-visibly-inert vs greyed.
  No work now.
- A11 (P0) NO pause/quit in match — START does nothing. Implement
  in-match pause menu; REFERENCE/COPY the owner's ssb64-funkey-s
  implementation: ~/code_projects/ssb64-funkey-s/port/gfx/fk_menu.c +
  port/include/fk_menu.h (MIT, owner's own code: modal menu drawn
  over the paused frame, blocks the loop, returns continue|quit).
  NOTE: pausing must NOT perturb replay/checksum domains — pause is a
  live-play-only surface (gate/evidence runs never pause); the frozen
  streams' pause machine exists upstream (bank `s` truthiness reads)
  — study before wiring START so live semantics stay faithful.
- A12 (P1) HOME/MENU button (keysym q) does nothing — wire to the
  same fk_menu-style overlay (quit-to-frontend at minimum), matching
  the ssb64 port's behavior.
- A13 (P2, owner 2026-07-27; do LAST, after testing/fixes settle) app
  title on the FunKey home screen must not read "FOH": the play
  install currently ships meleelight-foh.funkey-s.desktop
  `Name=MeleeLight FOH`. NOTE the constraint that produced it — the
  iter-73 stale-nav class needs the EVIDENCE OPK and the PLAY install
  to carry DISTINCT titles (check-device-opk.sh navigates by title).
  Correct end state: PLAY install `Name=MeleeLight` (its own desktop,
  the meleelight.funkey-s.desktop lineage), EVIDENCE OPK keeps a
  distinct title (e.g. "MeleeLight EV"). Both .desktop files are
  PINNED m4 producers (+ nav pins in check-device-opk.sh): the change
  carries its own arc + re-pin + one device nav verification.

## A8 AUDIT CONSEQUENCES (iter 120; evidence .loop/reuse-audit/REPORT.md)
- A14 (P0, folded into A1) glyph-atlas swap: menus must draw with the
  BROWSER-RASTERIZED VFXGLYPHS1 atlas (port/gfx/vfxglyphs-frozen.txt,
  captured gfx-pagelib.js:17,179, already judged every run and consumed
  by the HUD) instead of the hand-authored 5x7 foh_font.c — extend
  __gfxDumpGlyphs() coverage for menu strings, point foh_render.c at
  gfx_glyphs_load(), keep foh_font.c as a LOUD fallback. Audit F2: the
  largest single look win, and it reuses machinery already shipped.
- A15 (P1, folded into A1) menu-look ORACLE: capture upstream menu draw
  output through the EXISTING browser rig (port/gfx/capture-canvas.js +
  gfx-pagelib.js:402-408 already drive drawStage/renderPlayer/
  renderOverlay; menu draws are ordinary canvas fns on the same layers)
  so the restyle is judged, not eyeballed. Audit F4 — the "nothing to
  diff against" claim was circular.
- A16 (P2) re-frame A6/A7: audiomenu.js (223, zero DOM), credits.js
  (422, zero DOM), keytest.js (260 pure data), keyboardmenu.js (611,
  DOM only in a debug print) are TRANSLITERATIONS, not new features.
- U1 (P1, registered) port/gfx/gfx_bg.c — 236 lines translating
  stagerender.js drawBackground/drawStars — is the ONLY translated
  drawing TU with NO browser-parity check (gfx-pagelib.js:29 excludes
  bg from the judged mask; iou.js:5 judges fg1|fg2|UI only). Two-run
  byte-stability proves determinism, NOT fidelity — and it is what the
  player sees behind every match. Fix: enable drawBackground in the
  capture (one call) + extend the IoU mask; own arc + re-pin.
- U2 (P2) gfx_target.c target-stage rasterization is judged
  twin-vs-device (two copies of the same C agreeing), not vs browser.

## A1 PHASE-0 LANDED (iter 121) — registered follow-ups
- A14 CORRECTED (measured by the Phase-0 writer, supersedes the audit's
  assumption): vfxglyphs-frozen.txt (VFXGLYPHS1) contains ONLY digits
  0-9 plus `:`, `%`, space — **ZERO letters** across the 4 specs at
  gfx-pagelib.js:183-186. The swap therefore CANNOT land as-is. Correct
  sequence (Phase-1 task): add A-Z + `.,!?&'` to the `chars` of specs
  0/3 in port/gfx/gfx-pagelib.js → browser re-capture → re-freeze
  vfxglyphs-frozen.txt (a DEVICE-consumed artifact: check-device-foh.sh
  :1216 pushes it via --glyphs) → re-run check-render.sh + the device
  legs → swap foh_text2 → gfx_glyphs_load() with foh_font.c face 2 as a
  LOUD fallback → delete the 6x9 face once green. Spec 3 is literally
  `italic 700 70px Arial` = upstream's menu weight, and captured heights
  (5/7/9/12-14 px) suit 240x240. NOTE: hand-authoring more glyphs is
  forbidden in the meantime (that is the debt A8-F2 named).
- B1 (P1, NEW — pre-existing, found by the Phase-0 arc) **raster.c:69
  blend565() corrupts blue on EVERY partial-alpha blend**: it packs r+b
  into one field and the red term's low bits spill into blue (verified
  rgb(147,14,42) at a=87 over rgb(39,0,91) → blue 25, correct 9; up to
  24/255 error). Affects every anti-aliased edge the GAME renders, not
  just menus. Deferred by the writer (out of lane: -O3 sim TU behind
  frozen ink pins) and all three reviewers called the deferral
  defensible; new FOH drawing routes around it via px8_over. Fix needs
  its own arc + re-pin + a render-parity re-judge.
- B2 (P1, device-gated) Phase-0 render cost UNVERIFIED on hardware:
  an Opus reviewer estimated the uncached title path >20 ms vs the
  16.67 ms FOH budget (device legs fail on skips != 0). The writer
  cached frame-invariant gradients (~150k per-pixel sqrt+divide
  removed); the RESIDUAL needs measurement by the device lane before
  check-device-foh.sh re-runs. Measure BEFORE Phase 1 adds CSS/SSS art.
- A9 LANDED (iter 122): `assets` pipeline stage → one 74 KB IMG1
  (`assets/menu.img1`: 5 portraits at native 58 px, 6 stage previews +
  RANDOM at 65x24, 3 hand cursors at 24x32; RGB565 + full A8 plane —
  alpha decided by MEASUREMENT: cursors carry 24-34% partial pixels so
  a 1-bit colorkey cannot cover the domain). Loader `port/gfx/img1.{c,h}`
  (img1_open/find/at/blit/close), blit proven bit-identical to
  rast_blit_rgba over 360 cases. `bash pipeline/check-assets.sh` →
  `ASSETS OK`. Arc GO codex r6 (after 5 NO-GO; review caught a
  vertically-flipped resampler self-test and a hidden-white identity
  fast path). NO pinned producer touched — pins live in the SIBLING
  pipeline/expected-assets.json by design.
  FOLLOW-UPS: (a) P2 driver-owned one-commit promotion of the sibling
  pins into pipeline/expected.json + re-pin of expected.json/
  check-expected.js rows (deferred: both are reviewed-go pinned and
  were being edited by another lane at delivery time); (b) DEVICE PATH
  DECISION + provisioning of menu.img1 alongside the other private
  blobs (never committed/distributed — same handling as audio);
  (c) registered deferral: img1_blit makes per-pixel rast_blend_px
  calls from an -O2 TU — upgrade path is a rast_blit_565a8 batch
  primitive in raster.c (pairs naturally with the B1 blend565 fix),
  trigger = measurement once the FOH actually draws these;
  (d) stage_random is emitted for completeness but upstream uses that
  icon only as an onerror fallback — the FOH RANDOM slot stays text.
- A2 DONE (iter 121, device-verified; play OPK 5b658e9c installed).
- **B3 (P0, BLOCKER, driver-owned) Phase-0 restyle is DEVICE-DIVERGENT**:
  device shot != host twin by 5.25% from ~row 28; check-device-foh.sh +
  check-device-target.sh fail on MENU shots at 327a253 (both green at
  pre-restyle HEAD). Root-cause suspect: device-libm unsafe-FP class
  (CLAUDE.md M3 task 1) vs the new gradient/ring/shine float math. Fix =
  make the primitives device/host-identical (route through fdlibm or
  integer math), re-verify BOTH device checks green, then the A2 lane's
  ~2 device runs + manifest re-pins + final both-GO.
- B4 (P1) target-match exit returns to the FRONTEND, not the FOH menus;
  upstream endGame's state resets unapplied on that path.
- B5 (P2) target FINISH arm (all 10 targets destroyed) is unscriptable →
  uncovered by any rig.
- B6 (P1) GATE-BLINDNESS CLASS (A2's lesson, generalize): no rig
  combined the PLAY argv (`--bridge live`) with a non-VS launch kind.
  Add a play-path leg that drives the INSTALLED OPK through in-app input
  to each launch kind (VS + target), so the seam between "target plane
  tested" and "OPK tested" cannot hide another rc-4 class.
- B3 DONE (iter 123): device divergence root-caused to the free-running
  animation tick (NOT libm — falsified); foh_look_canonical() makes
  shots a pure function of machine state on every target; masked
  render-skip regression 66 → 0 via five byte-identical optimizations.
  Device FOH + TARGET green at the pre-restyle baseline ledgers.
- B7 (P1) look-plane INJECTION end state: record the DEVICE look plane
  and inject it into the host twin (oracle-fed-seam idiom), restoring
  cross-target comparison of the four menu shots' animation phase.
  Removes the interim canonical-phase seam (one 12-line fn, one seam).
  Changes two GATE scripts + revisits the iter-93 shot-judge design +
  introduces a device→host dependency in a judge → own Tier A+ arc.
- B8 (P1, required, rides the next FOH increment's arc) machine-plane
  tooth: perturb menuSelected on a CANONICAL shot and assert the bytes
  change — guards the risk that canonicalization makes shots blind to
  menu state. Ledger 21 → 22.
- A1 PHASE 1 COMPLETE IN WORKTREE agent-ac57efe4e1014b4da (NOT yet
  merged — deliberate): CSS + SSS restyled with real IMG1 artwork
  (portraits in port panels + row cells, stage art 1x in thumbs / 2x in
  the big preview, hand_point cursor, READY TO FIGHT swoosh, MELEE
  plate + VS badge, HMN/CPU/N-A tabs, CPU-level box, 8-frame pink thumb
  flash, orange RANDOM). check-foh-flows.sh green, ledger UNCHANGED
  (teeth=21), Phase-0 shots byte-identical. Arc GO (grok + Opus 5 x2 +
  grok confirmation; codex r2 cmp-disqualified). Side-by-sides:
  <worktree>/.loop/restyle-p1/sxs-{css,css-cpu,css-p2,sss,sss-ystory,
  sss-pstadium}.png. Deliberately NOT used: img1_blit (routes through
  the B1-buggy blend565 — cursors carry AA alpha) and stage_random
  (RANDOM stays text). Opus 5 caught that riglib.sh:1447 hashes every
  port/foh/*.c into the shared ARM stamp, so omitting img1.c would have
  broken ALL device rigs — correction taken.
  **MERGE HELD**: the A11/A12 pause lane is live in the main tree and
  both lanes edited FIVE of the same files (check-device-foh.sh,
  check-device-persist.sh, check-device-fullgame.sh, riglib.sh,
  check-device-target.sh) — almost certainly additive entries to the
  same build/pin lists. Driver merges BOTH after the pause lane lands,
  resolving the overlapping hunks (sole-merger rule). Do not merge
  piecemeal.
- A17 (P0, driver-owned, BLOCKING Phase-1 device verification) provision
  menu.img1: generate into the device data dir, push, OPK-stage, and
  export MLFK_MENU_IMG1 (or MLFK_DATA_DIR) from the launcher —
  foh_render calls art_load() on EVERY screen, so the persist/target
  device checks need it too. Link graph already fixed; only the data
  plane is missing. Then run the Phase-1 DEVICE PERF LEG (CSS/SSS p99 +
  skips; writer's estimate ~1.5-3.5 ms/frame, comparable to the title
  screen at p99 13.99 — UNMEASURED).
- A11/A12 DONE (iter 124, device-verified). A1 Phase 1 MERGED (iter 125).
- **B9 (P0, BLOCKER) FOH render-skip margin regressed AT HEAD**: device
  FOH + TARGET checks fail `1 FOH render skips (want 0)`; PROVEN
  pre-existing by a detached control worktree at 8859ddc (control 702
  ticks/1 skip). iter-123 optimized this counter 66→3→1→0 — the margin
  is one frame wide. Fix must buy REAL headroom, not another trim:
  candidate levers = the B1 blend565 fix + a rast_blit_565a8 batch
  primitive (also retires the A9 per-pixel deferral), and/or cutting
  title-path composites. Needs the device; blocks Phase-1 device
  verification (A17) and any device re-pin.
- A18 (P1, driver) m4-freeze-manifest re-pin after B9 clears:
  mlfk-foh.sh, foh_dev.c, riglib.sh, check-device-{foh,persist,target,
  fullgame}.sh + NEW rows foh_pause.c, img1.c; then the anchor.
- A19 (P2) quit-to-menu is a process relaunch, not in-process FOH
  re-entry (`ponytail:` marked). Restart deliberately skipped —
  upstream has no restart-match semantics.
- B9 DONE (iter 126): title render 16.96→9.06 ms, margin −0.29→+7.61 ms
  via three bit-identical -O3 raster run-primitives; menu skips 0. NO
  gate relaxation needed — the owner's "one frame is fine" allowance was
  not spent. A17 DONE (artwork provisioned, playable restyled OPK
  installed + boot-tested). A18 DONE (89 producers, anchor verified).
- B10 (P1) B9 review arc round 5 → GO, then flip the 12 arc-in-flight
  manifest rows. verify_m4.sh correctly refuses until then.
- A13 + check-device-opk.sh inventory: SAME ROOT — the check's frontend
  inventory pin expects the retired /mnt/Applications/meleelight.opk
  while the device carries meleelight-foh.opk. Do the title rename and
  the NAV_LINK/OPK_INVENTORY_PIN re-measure TOGETHER as one reviewed
  change.
- B11 (P2) check-device-{persist,fullgame}.sh wired for artwork +
  syntax-checked but NOT run; fullgame's PRODUCER_PINS 12→14 verified
  only by static count.

## OWNER PLAYTHROUGH #2 (Chase, 2026-07-28) — design approved, MECHANICS + FIDELITY are the theme
Verdict on look: main menu GOOD · CSS design GOOD · SSS design GOOD.
Everything below is functionality/fidelity, not styling.
- **C1 (P0, CRASH) the game crashed while sitting on CSS too long.**
  Root-cause required (no reroll, no guess). Suspects to MEASURE:
  the CSS animation/look plane (hue lerp, ribbon pulse, cursor anim
  counters) overflowing or indexing out of range over long dwell;
  art/IMG1 lifetime (art_load per screen — leak or double-free over
  re-entry); the 57 MB RAM ceiling; timer wrap. Capture on device with
  the app's own log + any core, and reproduce deterministically before
  fixing.
- **C2 (P0) CSS FUNCTIONALITY IS NOT FAITHFUL — spec required first.**
  Upstream CSS is a FREE-MOVING HAND CURSOR, not discrete rows:
  the hand moves anywhere and can click anything; B (funkey face
  button) RETRIEVES your token back to the hand; you then DROP it on
  any character; HMN/CPU is a clickable toggle; **P1 can be set to
  CPU**; a CPU token can also be picked up; and there is currently NO
  way for Chase to select his own character. DESIGN IS KEPT — only the
  interaction model changes. Deliverable order: SPEC first (measured
  from upstream src/menus/css.js), then implementation.
- **C3 (P1) EVERY menu must be as faithful as possible.** Named:
  GAMEPLAY OPTIONS is missing "everyone walljumps" AND is not faithful
  in shape; CREDITS does not work; CONTROLS enters but cannot select
  controller/keyboard. (A16 already measured these as TRANSLITERATIONS
  — upstream audiomenu.js/credits.js/keytest.js/keyboardmenu.js are
  zero-DOM canvas code, not rewrites.)
- **C4 (P1) stage-select previews are FAR TOO DIM** on device — can
  barely see them. Investigate the IMG1 emission path (800x300 → 65x24
  area-average downscale; RGB565 quantization; any alpha/darkening at
  blit) and fix at the correct layer.
- **C5 (P1, owner ruling) HIDE Spectate / P2P / Server** (A10 decided),
  and **VS MELEE goes straight to local VS** for now. Implement behind
  a NAMED FLAG/constant so the battle-mode submenu can be restored
  later without archaeology (a single documented switch, not deletions).

## OWNER DECISIONS on MENU-SPEC deviations (Chase, 2026-07-28) — BINDING
- **D6 ACCEPTED** (ports 3 & 4 stay N/A for now) **+ DEFERRED ITEM D6-later:
  make ports 3 & 4 work as CPUs.** Owner flagged the right risk unprompted:
  a 3/4-participant match is unverified against the oracle AND untested for
  frame budget — 4 players = more sim + more render per frame against a
  measured match p99 ~14.2/16.67 ms. D6-later must therefore carry BOTH a
  conformance leg (goldens/traces at 3-4 players) and a perf leg (p99 +
  skips), not just the menu change.
- **D8 PARTIAL ACCEPTED** (keep random-tag + clear-tag click widgets; no
  text entry) **+ DEFERRED ITEM D8-later: a faithful NAME-ENTRY screen**
  modeled on meleelight's/real Melee's name-entry UI (an on-screen
  character grid driven by the d-pad — the faithful device analogue of
  upstream's browser text input).
- **D1/D3/D4/D7/D9/D10 ACCEPTED** (device-physics deviations, not choices).
- **D2/D5/D11/D12/D13 ACCEPTED** ("consequences of decisions already made").
- **WALLJUMP: option A ACCEPTED** — implement `Everyone Walljumps` as the
  faithful DEAD toggle (row + persisted bit + zero readers, exactly as
  upstream). **+ DEFERRED ITEM WJ-later (do at the END): make it actually
  work** — an explicit, owner-sanctioned DEVIATION from upstream, to be
  designed and registered as such rather than smuggled in.
- **C4-C RULING (Chase, 2026-07-28): TAKE FIX C — the stage-preview
  brightness lift.** Value γ0.75 (driver's recommended row 4; the owner
  said "C please" against a sheet whose recommended row was 0.75).
  **THIS IS AN OWNER-SANCTIONED DEVIATION FROM UPSTREAM (HARD RULE 5).**
  Justification on the record: upstream's own art is near-black (bf mean
  9.18/255, byte-identical to upstream's browser render) and readable
  only because upstream draws it 800x300 on a 1200x750 canvas; at
  130x48 on a 240x240 panel the faithful bytes are, in the owner's
  words, "SUPER dim, can barely see them" — i.e. faithfulness to the
  bytes defeats the purpose the art serves. Scope is NARROW: the
  stagePreview class ONLY (not portraits, not cursors, not any judged
  render surface). γ MUST live behind a NAMED CONSTANT so the value is
  a one-token change after the owner sees it on hardware (row 5 =
  γ0.65 is the next stop if 0.75 reads too dark in the hand).
  Rides the A+B arc + re-freeze (one artifact regeneration, one re-pin).
- C1 DONE (iter 127): 300 s menu timeout root-caused + fixed + shipped
  (OPK 5446bd25). Class instance #2 of "evidence-rig bound governs the
  play path".
- **C6 (P1, USER-VISIBLE, class instance #3) `--frames 10800` ends any
  match past 3:00 and dumps to the frontend** — same class as C1, one
  screen later. NOT a simple removal: `--bridge live` argv-forces
  --record-trace/--record-keys whose buffers are frames-proportional.
  Fix ORDER: make recording opt-in or streaming FIRST, then drop the
  bound.
- C7 (P2) `--seed 1337` fixed on the play path (class instance #4 in
  spirit): rec.json carries no seed field — record the seed BEFORE
  randomising, or replays break.
- C8 (P1) **§11 evidence gap**: C1's Opus 5 fallback GO exists only in
  an agent summary — no .loop/review-c1-opus.log. mlfk-foh.sh's manifest
  row stays `arc-in-flight` until materialized. 2nd instance of this
  failure mode (iter-117 was the 1st) — if it recurs, the standing rule
  needs teeth: no fallback reviewer counts without its log.
- C9 (P2) coverage tooth for C1's changed boolean (3 exit-code
  assertions); cheapest home is check-device-foh.sh (it already builds
  foh_dev_headless; check-foh-flows.sh builds foh_app, not foh_dev).
- C10 (P2) launcher grep-assert restoring the "cannot silently regrow"
  property the refusal shape would have bought.
- **C11 (P1, CLASS FIX — instrument, driver-registered iter 129) make
  manifest CITES mechanically verifiable.** Root: iter-127 flipped
  mlfk-foh.sh to `reviewed-go` citing `.loop/review-c1-grok.log`, whose
  verdict is `**VERDICT: GO**` — MARKDOWN BOLD, zero matches for the
  anchored `^VERDICT: GO$` the manifest's own discipline requires. The
  DRIVER made that error by eyeballing a `tail` after an anchored grep
  returned empty — i.e. exactly the permissive-parse failure PROCESS §3's
  whitelist-grammar rule exists to prevent, committed by the person who
  enforces it. B10's codex round caught it. FIX: the manifest self-check
  should PARSE each row's cite, extract every `.loop/review-*.log` path
  it names, and assert each names a file that exists AND contains ≥1
  anchored `^VERDICT: GO$` — fail closed otherwise. Turns cite integrity
  from a habit into a mechanism. (Also add to the reviewer briefs: emit
  the verdict UNADORNED — no bold, no fence.)
- C12 (P2) B10 residual, 4 items, none behavioural: raster.c:122 comment
  credits span8 with "every poly8 span" (poly8 now bypasses it — r4-fix
  residue); check-device-foh.sh:1152 `ART_SHA="$hsum"` assigned never
  read; primdiff.c:88-97 can't distinguish the doubled clip test (guards
  provably redundant, code right); foh_render.c:134-140,:790-796
  fill_rect/blit_img_rect left per-pixel, unnoted (CSS lane's file).
  Fix the first three + route the last, then round 6 → GO → flip the 7
  rows.
- C13 (P2, registered class — 3 consecutive rounds) manifest
  STATE-OF-RECORD PROSE goes stale vs the row statuses it describes
  (hand-maintained enumeration; any lane may flip a status). Zero
  enforcement weight — the gate parses the status field, never the
  prose. CLASS FIX: derive the enumeration or delete it.
- U1 DONE (iter 130): gfx_bg.c browser-parity check landed (4 frozen
  judgments incl. a STAR-ONLY plane); truncate-vs-round class bug fixed
  in the gradient AND star/mountain colours; drawTunnel's flat-0.15 vs
  upstream's radial gradient REGISTERED (alpha-only, invisible to a
  silhouette judge — needs its own observational leg before fixing).
- **U3 (P1, CLASS — the durable judge-design lesson) audit every
  aggregate visual threshold for the same false-green.** U1 proved a
  whole-plane aggregate passes at 0.9927 with a FEATURE ENTIRELY
  DELETED. Any judge that scores a union of features can be blind to a
  missing one. Sweep: the fg IoU leg (players/articles/vfx/overlay all
  share one mask), judge-shot.js's structural judges, the target-plane
  verifier. Fix shape: per-feature planes, not per-plane aggregates —
  measuring a feature's own plane needs no geometric argument.
- U4 (P2, device-gated, from U1) background PIXELS CHANGED (col8()
  rounding: 24 gradient rows + star/mountain colours ≤1). Any device
  evidence pinning background pixels must be RE-TAKEN — check-device-
  render.sh shot compares, any screenshot-pinning leg. Do it in the
  next device session alongside C6's proofs.
- CSS MECHANICS DONE (iter 131): free cursor + token model + port types
  + ready/launch rule; teeth 21→26; arc GO codex r11.
- **C15 (P0, OWNER RULING NEEDED) the one-hand / two-attached-port
  model.** The CSS lane's implementation matches NEITHER upstream
  attachment case, but without some choice here the two-human goldens
  cannot launch from the menu. Needs Chase's call on how a single
  physical d-pad drives what upstream gives two attached controllers.
- **C16 (P1, device lane) P1-CPU LAUNCH**: sim_setup_match pins
  types[0]=0 and both bridges carry only P2's type/level. The menu can
  now SET a CPU P1; launching one needs foh_dev.c + bridge extension.
  Currently refuses loudly (T19) rather than booting a human P1 under a
  lying record — keep that failure until the bridge is extended.
- C17 (P2) DEV_NEG/DEV_POS device bounds inherited from the retired 3×
  injector cadence, unvalidated, left frozen to fail loud. Revalidate in
  a device session.
- C14 (P2, citation erratum) main.js line cites throughout
  docs/AGENT-LOG.md and docs/MENU-SPEC.md are read off the PATCHED clone
  and run ~6 lines HIGH. Citation-only correction; no behaviour.
- C6 DONE (iter 132): 3:00 match bound GONE; recording opt-in first;
  frame 28,890 × 3 runs, memory flat; OPK 545aae9f installed.
- **C18 (P1, NEWLY REACHABLE) `finishGame` / results path does not
  exist.** At the natural upstream 8-minute match end the sim hits
  `SIM FATAL frame 28890: matchTimer expired (finishGame) — outside the
  golden domain` (RC=3) and drops to the frontend. The 3:00 bound made
  this unreachable; removing it exposed it. The trap is CORRECT and was
  NOT weakened — it guards unvalidated sim behaviour. Fix = port
  upstream's finishGame/results screen (a real feature, and the same
  family as B4's "target-match exit returns to the frontend"). Do them
  together.
- **C15 RULED (Chase, 2026-07-29): OPTION A — one hand, both tokens.**
  Ratifies what the CSS-mechanics lane built: ONE hand cursor (honest —
  one physical device) with BOTH ports treated as attached, so port 2 can
  be HMN and the single hand may grab/drop EITHER port's token.
  **This is an owner-sanctioned DEVIATION (HARD RULE 5)** from upstream's
  `playerType[j]==1 || i==j` guard, which forbids touching another
  human's token. Justification on the record: that guard exists to stop
  one player stealing another player's pick; with a single physical
  device there is no second player to steal from, and the literal
  reading (`ports=1`) makes port 2 permanently CPU — which would make
  human-vs-human matches unreachable from the menus AND leave the
  two-human goldens unlaunchable, i.e. faithfulness to the guard would
  break faithfulness to the game. Upgrade path recorded: Option C
  (a shoulder button hands control to the other port's own cursor) is a
  clean later refinement — the token model is already per-port
  underneath — and would restore the guard verbatim. Must be documented
  in MENU-SPEC §2 as a numbered DEVIATION alongside D1-D13.
- **C19 (P1, owner 2026-07-29) VS pause menu: add a "quit to VS screen"
  entry.** Today the in-match pause overlay offers Resume / Quit to menu
  / Quit to OS; the owner wants a fourth that returns to the **character
  select (CSS)** — i.e. rematch/change-character without going all the
  way out. NOTE the existing constraint (A19): "quit to menu" is
  currently a PROCESS RELAUNCH, not in-process FOH re-entry, so a naive
  copy would relaunch and land on the title screen, not CSS. Doing this
  properly wants the in-process return A19 registered, which also fixes
  B4 (target-match exit lands on the frontend) and pairs with C18 (no
  finishGame/results path — a natural match end has the same "where do I
  land" question). **Treat C18 + C19 + B4 + A19 as ONE increment: the
  match-exit/return plane.** Faithfulness note: upstream's own pause
  machine is on the checksum surface (CHECKSUM.md; the M2 task-16 gotcha
  — the KEYBOARD arm reads the bank's `s` by truthiness), so the exit
  paths must stay outside the sim exactly as A11's pause overlay does
  (NULL-default hook, live-play only).
- **A12b [SUPERSEDED by A12c — the driver misread the report; kept for
  the record] MENU/HOME does nothing in the MENU phase.** Diagnosed read-only:
  foh_dev.c:2221 wires pin.menu → foh_pause_hook but that site is INSIDE
  THE MATCH LOOP, so the overlay only exists during a match. In the FOH
  phase pin.menu drives `qEdge`, the evidence rig's screenshot-marker
  trigger, armed only with --shots-dir; foh_dev.c:1907-1911 says it
  outright: "the OPK PLAY path runs without it, so the player's MENU
  button is a no-op there." NOT the marker-mismatch sim_fatal (that
  death is correctly rig-only). **A12 was under-specified by the driver**
  — the ruling was "matching the ssb64 port", and ssb64's fk_menu opens
  from MENU wherever you are. FIX: MENU opens an overlay in the FOH
  phase too (Resume / Quit to OS at minimum; "Back to title" optional
  from deeper screens). Discriminator already in the file: arm the
  overlay on the MENU edge only when `shotsDir` is UNSET, so the rig's
  q-marker semantics stay byte-identical.
- **A12c (P1, owner-clarified 2026-07-29 — SUPERSEDES A12b) the FUNKEY
  SYSTEM MENU was never implemented.** The owner's "home button bringing
  up the funkey menu" means the STANDARD OS-STYLE overlay, not our pause
  menu. Evidence = his own ssb64 donor
  (~/code_projects/ssb64-funkey-s/port/gfx/fk_menu.c, MIT, the file he
  told us to copy): entries `{VOLUME, BRIGHTNESS, QUIT, POWER OFF}`
  (:146), volume/brightness through the OS shell tools
  (`volume get|set`, `brightness get|set`, :102-103/:117-125), POWER OFF
  → `powerdown handle` (:129), drawn with the **OS's OWN resources**
  /usr/games/menu_resources/{zone_bg.png,arrow_top.png,arrow_bottom.png,
  OpenSans-Bold.ttf} (:27-31) — which is why it LOOKS like the real
  FunKey overlay — hint "A: select   B: back" (:161), safe no-op if
  SDL_ttf/font missing, dimmed-frame fallback if zone_bg is missing
  (:87). **A11's writer recorded "Skipped: volume/brightness from
  fk_menu (not asked)" — correct against the DRIVER'S BRIEF, which was
  wrong.** END STATE = TWO overlays, as ssb64 has: START → the GAME's
  pause menu (Resume / Quit to VS screen / Quit to menu / Quit to OS);
  MENU/HOME → the FUNKEY SYSTEM menu, available EVERYWHERE (match AND
  FOH menus). Open design calls routed to the lane: whether to link
  SDL_ttf + the OS font (verify presence on device first) vs render the
  layout with our bitmap font while still using the OS artwork; copying
  his MIT code is preferred over re-inventing, with the NOTICES entry
  landing BEFORE the code (docs/LICENSING.md).
- **OWNER RATIFICATION (Chase, 2026-07-29) on the two overlays:**
  (a) the GAME pause overlay is APPROVED AS-IS ("good and correct") —
  no redesign; the ONLY addition is C19's "quit back to the VS screen".
  (b) MENU/HOME = the FunKey SYSTEM menu, and the owner explicitly
  authorized **pulling his own ssb64 implementation**: "which ssb64 port
  I made — you can pull that from there, adjust anything if needed."
  So fk_menu.{c,h} is COPIED (MIT, his code) rather than re-authored;
  "adjust anything if needed" = permission to ADAPT to our platform seam
  (renderer/present path, input struct, logging), NOT to redesign —
  VOLUME/BRIGHTNESS/QUIT/POWER OFF, the /usr/games/menu_resources
  artwork, the safe-no-op and dimmed-frame fallbacks and the
  "A: select   B: back" hint all stay. NOTICES entry + header
  attribution land BEFORE the code (docs/LICENSING.md; foh_pause.h is
  the in-tree precedent). Device pre-flight required: volume/brightness/
  powerdown tools present, menu_resources present, SDL_ttf + font load —
  any absence degrades to his own safe-no-op contract, never a faked
  menu.
- **C20 (P1, instrument) DERIVE the file→check consumption map.** PROCESS
  §12.2(4) says re-run the checks whose inputs a merge touched — that
  decision must be mechanical, not a driver judgment call. Write a script
  that parses each check script's source/TU/pin lists (they already
  enumerate them: `rig_srchash`'s find roots, per-check TU arrays,
  PRODUCER_PINS tables, `--anim-dir`/`--gfxdata`/`--glyphs` argv) and
  emits `path → [checks that consume it]`. **DERIVED, never
  hand-maintained** — C13 recorded exactly this failure mode (manifest
  prose enumerations going stale relative to the rows they describe,
  three consecutive review rounds). Consumers: the driver's post-merge
  re-run set, and writer briefs ("your change invalidates X, Y").
- **C21 (P1, instrument) content-addressed shared ARM build cache.**
  Every worktree currently starts with a cold `arm-build.stamp` and
  rebuilds under amd64 emulation even when its sources are byte-identical
  to another lane's completed build. `rig_srchash` ALREADY computes the
  right key (all port/* + oracle/qjs sources + generated ml_tables.c /
  ml_stages.c + every rig script + the resolved docker image id) — the
  work is relocating the stamp+binaries into `~/.cache/mlfk-arm/<key>/`
  and looking up before building. Cannot serve wrong bytes by
  construction (changed input ⇒ different key), but it IS evidence
  machinery: **verify-on-read (re-hash every artifact before use, the
  existing rig_stamp_rehash discipline), never populate from an
  unverified build, keep MLFK_FORCE_ARM=1.** Touches riglib.sh (a pinned
  producer) ⇒ own Tier A arc + re-pin. Same treatment applies to
  `pipeline/build/` artifacts, whose byte-stability the M1 gate already
  proves — key on INPUTS not paths (the C4 lane legitimately changed
  img1.js and its menu.img1 SHOULD differ; a path-keyed cache would have
  served stale bytes).
- **C22 (P2, procedure) device WORK ORDERS.** Host lanes needing hardware
  emit a work order under `.loop/` (commands, evidence to pull, expected
  verdict lines) instead of each taking the device; one device-owning
  lane drains the queue per session. PROCESS §9's batching applied ACROSS
  lanes. Avoids N lock cycles / N frontend park-restore cycles / N cold
  builds for N host lanes.
- **C24 (P1, DEFECT — cost the owner a hung device 2026-07-29) the
  frontend-park deadman does not cover a HOST-side death.** A device
  lane died during the API/classifier outage before its cleanup ran, so
  `/mnt/disable_frontend` stayed on the SD card and EVERY subsequent
  boot deliberately declined to launch gmenu2x — indistinguishable from
  a hang at the splash screen. The owner had a bricked-looking device
  until the driver removed the marker and rebooted. The existing
  deadman (`rig_deadman_quiesce`, the qd claim/reassert machinery)
  guards against the DEVICE-side process dying; it does not fire when
  the HOST agent dies mid-run. Fix candidates: (a) make the park marker
  self-expiring — write it with a deadline the device checks at boot and
  clears (the boot path already reads it, so a stale marker can be
  detected there); (b) an unconditional boot-time reaper that clears any
  park marker older than N minutes; (c) at minimum, a driver pre-flight
  in every device brief that clears stale markers before work starts.
  (a) or (b) is the class fix — (c) is discipline and discipline is what
  just failed. NOTE the marker is a PLAY-PATH hazard, not just a rig
  one: it makes the device look broken to the owner.

- **C25 (P1, CLASS — the cite layer's remaining hole) PRIOR-CLOSURE
  laundering: a cite may carry a GO that belongs to a DIFFERENT arc.**
  `verify_m4.sh`'s cite verification (iter 134) proves every
  `reviewed-go` row cites a log whose TERMINAL anchored verdict is GO,
  which closes the iter-127 adorned/non-terminal class. It does NOT prove
  the GO was about THAT PRODUCER. MEASURED before proposing a rule: 22 of
  59 GO-backed rows have no GO log naming their own producer, so a naive
  "the GO log must name the producer" rule would false-reject 22 rows
  today. Fix = a structured closure record (producer + pinned sha +
  verdict + round) that a cite REFERENCES by id, instead of free-text
  hyphen-soup; ~60 cites to migrate. DRIVER-OWNED (the cite corpus is the
  driver's ledger, not a lane's). Until then the cite layer proves
  "a real GO exists", not "this producer was reviewed".
- **C26 (P2, tooth-resistant residual) appended-transcript / fenced GO.**
  A GO line that is positionally terminal but semantically inert (inside a
  fenced block, or an appended transcript) is indistinguishable from a real
  terminal GO by any anchored grep. A tail-window rule was implemented,
  MEASURED to miss this shape AND to false-RED real logs, and removed
  rather than kept as decoration. Same structural fix as C25.
- **C27 (P2) non-arc cite that DENIES its gate.** `oracle-frozen` /
  `grandfathered-m1|m2` rows must name an exit gate, and do (measured over
  all 24). But a cite is free text: hyphen-soup defeats any delimiter rule
  distinguishing "proven by GATE" from "not proven by GATE". Fix = a
  positive `PROVEN-BY-<GATE>` token form. Rolls into C25's migration.
- **C28 (P1, JUDGE DEFECT — measured) `check-render.sh`'s fg IoU is not
  run-to-run reproducible.** Seven distinct IOU MIN values (0.9004-0.9076)
  across IDENTICAL code with fresh captures; stable only under a fixed
  capture. An aggregate whose own input drifts ~0.008 cannot support a
  tight bound, so the current threshold is loose enough to be blind (this
  is the mechanism behind U3's articles hole: 0.8961 passed a 0.88 bound).
  Fix: judge a FIXED capture, or find and remove the capture
  nondeterminism, before tightening any threshold.
- **C29 (P2, cosmetic dead code) `check-device-foh.sh:1158` `ART_SHA="$hsum"`
  is a dead assignment** — verified the only occurrence repo-wide and
  `$hsum` is already asserted on the preceding line. Owned by the
  match-exit lane (its file); reported, deliberately NOT edited across a
  lane boundary. Also: the manifest header's `UNCLOSED ROWS` narrative
  block names the wrong rows (driver's own prose, stale since iter 132).

- **C30 (P1, INTEGRATION DEBT — the controls feature is NOT REACHABLE yet)
  wire `s1_input_row_style` + link `ctl_style.c`.** The controls lane
  landed three styles (Natural default, Normal, Box) + the orthogonal
  Mod-shoulder swap with MLFKPERSIST3 v1/v2 migration, arc GO codex r6,
  and NOTHING ON THE DEVICE CHANGES until three edits land in files the
  lane deliberately did not touch (lane-boundary discipline):
  (a) `port/foh/foh_dev.c:2240` and `:2730` and `port/gfx/gfx_app.c:836`
      call `s1_input_row(&pin)` -> `s1_input_row_style(&pin,
      ctl_style_get(), ctl_mod_on_r_get())` — **match-exit lane owns
      foh_dev.c**, so this batches with its merge;
  (b) add `port/gfx/ctl_style.c` to the FOH/gfx link lines;
  (c) menus lane: the Controls screen needs two rows (`ctl_style_*`,
      `ctl_mod_*`) + the two persistence calls.
  Until (a)+(b) land, `ctl_style_get()` has no caller and device
  behaviour is byte-identical to today — a green check on an unreachable
  feature. DO NOT mark the controls work done on the strength of its
  arc GO alone.
- **C31 (P2, copy collision) "Natural" vs "Normal" style labels read
  almost identically** in a 240x240 menu. Codex's advice, adopted by the
  lane: rename NORMAL's DISPLAY LABEL only (`ctl_style_name`), never
  delete or renumber the style — the CtlStyle enum values are a FROZEN
  WIRE FORMAT in `FohPersist.ctlStyle` and a renumber silently remaps
  every existing save. Menus lane's call, one line.
- **C32 (P2, doc accuracy — found by the Roy lane, zero behaviour
  change) `oracle/qjs/replay-main.js:47-53` mis-attributes the 464
  in-page boot draws.** The count (and the 465 pin) is CORRECT; the
  comment credits "464x stagerender bgStar constructors". Measured
  actual: bgStars 20x6=120 (`stages/stagerender.js:16`), credits cStars
  100x3=300 (`menus/credits.js:265`), startscreen lightDust 20x2=40,
  menuRandomBox 4 => 464, +1 jQuery expando = 465. Worth correcting
  because the real structure is load-bearing for the Roy work: EVERY
  bound is a fixed literal, NONE derives from character count, which is
  the structural reason a 6th character cannot move the pin. HARD RULE 3
  applies (`oracle/` is read-only outside M0) — comment-only, and only
  if a future M0-touching task is already open.

- **D-RULING (driver, 2026-07-29) FohPersist wire-format collision ->
  MLFKPERSIST4.** Two lanes independently extended `FohPersist` and BOTH
  claimed `MLFKPERSIST2` with DIFFERENT layouts (controls: 57 lines,
  fields before the rec rows; menus: 62 lines, after). Ratified tiebreak:
  **the controls lane's lineage SHIPPED TO A DEVICE**, so its
  PB-preserving v1/v2/v3 migration is kept VERBATIM and the menus lane's
  seven lines append as `MLFKPERSIST4` (64 lines, 1510 bytes). The menus
  lane's private "v2" is deliberately NOT a migration source — no build
  of it ever existed on hardware, so nothing can be carrying it.
  Generalisable rule: **when two unshipped formats collide, the one that
  reached a device wins the version number**; migration sources are
  determined by what physically exists in the field, never by authoring
  order. OPEN: the v3->v4 migration arm has NO tooth while every other
  arm does (menus lane's own finding) — that gap is where a silent
  save-wipe hides, and the owner has a real v1 save on his device.
  `check-device-persist.sh` is updated but UNRUN (device-only).
- **C33 (P1, CLASS — third instance, now named) LIVE STATE PLACED INSIDE
  A STRUCTURE SOMETHING ELSE RESETS.** Measured instances: (1) the FOH
  audio bus lived in `SndMusic`, which `snd_music_cfg` memsets on EVERY
  track switch — volume set in the menu snapped back when a match
  started; class fix was moving both buses to `SndMixer` scope so the
  reset structurally cannot reach them. (2) the M4-task-5 `ml_sim_runai_live`
  pointer seam had to live OUTSIDE GameState precisely because GameState
  is memset (CLAUDE.md M4 task 5 gotcha 1). (3) falcon SSG
  `canEdgeCancel` / the rule-17 charHitboxes plane had to become runtime
  overlays rather than data for the same reason (M2 task 17 gotcha 2).
  INSTRUMENT WORTH BUILDING: grep every `memset`/bulk-init of a struct
  and list which live fields it reaches — this class is invisible at the
  call site and only shows up as "the setting doesn't stick", which is
  exactly how the owner reported it.

## OWNER RULINGS 2026-07-30 — new queued work (Chase ratified 4 decisions)

Ordered. Items R1/R2 gate the merge; R3/R4 are new work created by the rulings.

### R1 — menus: CAPPED closure (blocks everything else)
Owner ruling: menus stops at round 20. **Do not open round 21.** Fix
outstanding Medium+, disposition Lows in writing, write the §3 CAPPED record
naming the recurring class (already measured: *the judge/normalizer surface
admits new loosenings faster than point fixes close them* — 12 point-fix rounds
did not converge; one class fix, freezing 22 decision-table hashes +
`DECISION_REGION`, killed the category and defeated a constructed 5-way stack).
Then discharge the **Tier A+** obligation (independent second reviewer — NOT
grok, recorded unauthenticated — plus archived old-vs-new byte-identity
regression), and close its own **BLOCKER 9** (`ctl_style.c` into the `riglib.sh`
and `check-device-fullgame.sh` link recipes; measured 0 occurrences in both).
Producer + judge changes land ATOMICALLY in one commit.
`done-check:` its CAPPED record exists, Tier A+ second-reviewer log carries a
terminal anchored verdict, `grep -c 'ctl_style.c'` >= 1 in both link recipes,
and its own gates re-run green on final bytes.

### R2 — batched merge + ONE re-pin pass (driver-only; after R1)
Merge match-exit (main tree, AT GO), cite-closure (`agent-a7ec05a2f21b29494`,
CAPPED), render-judge (`agent-a01a4b6eba976b930`, AT GO) and menus together, then
re-pin ONCE (§12.2(3)). **The re-pin is FIVE rows, not the one originally
reported** — driver-measured over all 89 rows: `riglib.sh`,
`check-device-target.sh`, `check-device-foh.sh`, `mlfk-foh.sh` (all
`reviewed-go`), plus `check-device-fullgame.sh` (`arc-in-flight`, B9 OPEN — it
stays arc-in-flight). `verify_m4.sh` is NOT drifted: its row pins the sha of its
bytes EXCLUDING the `^MANIFEST_SHA256=` line, normalized hash `ee6a9444…78975`
matches — any drift audit must normalize that row or it cries wolf.
Cites must name logs whose TERMINAL anchored verdict is GO
(`.loop/review-mexit-r7g.log`, `.loop/review-mexit-r7o.log`) and **must not
carry forward the brace glob** in the current `mlfk-foh.sh` cite (the iter-132
error that broke the gate's line grammar). Fingerprint every merged file
(§12.2(5)); `git apply` prints per-file success and can still roll back
atomically. Every re-pin error mode is a REFUSAL, never a false green.
`done-check:` `bash port/sim/device/verify_m4.sh` reaches (and passes) its PIN
stage; `.loop/cite-teeth.sh` green with no concurrent manifest/gate reader live.

### R3 — successor rig for the never-executed arms (BEFORE any M4 gate attempt)
Owner ruling: **~232 lines have never executed anywhere** (`foh_sysmenu_open`
~157, VS-finish block ~75), and `verify_m4.sh` **cannot reach them by
construction** (`sysOk` requires `!shotsDir`; every committed flow leg passes
`--shots-dir`). Build a committed rig that drives the in-match system menu and
the VS-finish arm (`--match-timer` appears in NO script today), so their first
execution is not the owner's acceptance playthrough.
**PRECONDITION — fix first:** the three unbounded release drains. The new host
injector holds keys at EOF BY DESIGN, so the rig would **HANG rather than
fail**; a hang is strictly worse than a failure for an autonomous loop. Also
fold in here (bytes move anyway, fresh round already owed): the ratified
TIME!/GAME! one-frame latch IF ever fixed, `foh_dev.c:3623`'s unlatched dead
display, the 9 drifted `main.js` citation ranges, and the `gameMode == 5`
comment overclaim.
`done-check:` a committed check drives both arms end-to-end and FAILS when
either arm is broken (prove both directions); no run can hang — a stuck injector
must time out loudly.

### R4 — reviewer-harness verdict artifact (PROCESS change, owner-approved)
The reviewing harness emits **its own** verdict artifact carrying an **arc id**,
the **exact reviewed scope**, and **which rule closed the arc** — so arc closure
is answered by a producer-written record, never by a reader interpreting log
bytes. Approved on FIVE measured failure modes in one day, and derived
INDEPENDENTLY by two lanes from unrelated evidence. Closes: foreign-GO-at-EOF
laundering · the 19 `x-*` cross-artifact rows (no machine-readable arc identity)
· fabricated work-status ledgers · "arc reached GO" under a two-reviewer rule
with one reviewer · corrupt logs whose readable text contradicts their rc
(measured: a log 44.4% NUL containing `DEVICE TARGET CONFORMS` whose run exited
`TARGET_RC=1`).
Supersedes cite-closure residuals 1+2 as their durable fix.
`done-check:` a review round cannot be accepted without its harness-written
artifact; teeth prove a quoted foreign verdict and a truncated/NUL log both FAIL
CLOSED.

## STATUS 2026-07-31 — R1/R2/R3 DONE; R4 held on Tier A+

- **R1 menus CAPPED closure — DONE**, merged in `bed73d6`.
- **R2 batched merge + re-pin — DONE**, `bed73d6` + `14c394b` (17 rows re-pinned,
  iter-132 brace glob removed, anchor `0a5f5957…88ee`, `CITE TEETH OK 17/17`).
- **R3 successor rig — DONE**, merged in `109645c`. Arc CAPPED at 8 rounds.
  Found and fixed a shipping crash (every natural VS timeout aborted the game).
  Driver cold-verified `LIVE ARMS OK` rc 0 and `SIM CONFORMS` rc 0. No re-pin
  owed (0 of 8 touched files are pinned producers).
- **R4 reviewer verdict artifact — BUILT, NOT MERGED.** Driver cold-verified
  `REVIEW ARTIFACT TEETH OK (144/144 fail closed)` rc 0. **Blocked on its own
  Tier A+ obligation**: `arc-closure.sh` is a judge, its 7 rounds were sealed at
  tier A, and sealed artifacts cannot be re-tiered.
  `done-check:` an independent second reviewer (fresh Opus 5, NOT codex, NOT
  R4's session) returns a terminal anchored verdict on the FINAL bytes, AND the
  archived old-vs-new byte-identity regression exists; then merge.

### R5 — device legs, now unblocked (device back online 2026-07-30 22:38)
Run the device evidence deferred all cycle: `check-device-persist.sh` (NEVER
run), plus the device arms of `check-device-foh.sh`, `check-device-target.sh`,
`check-device-fullgame.sh`. R3's capped-closure floor is three device-only facts
(headless present is a success-reporting no-op; headless audio starts no callback
thread; a daemon-owned container escapes a process-group kill) — only hardware
settles them. Take the riglib lock; if `/mnt/disable_frontend` is ever set, use a
SELF-EXPIRING marker (C24).
`done-check:` each device check prints its anchored OK line with rc 0, evidence
pulled and judged HOST-side.

## STATUS 2026-07-31 (late)

- **R4 — owner-ruled SPLIT** (commit `7546485`): keep the producer, demote
  `arc-closure.sh` to a diagnostic (`arc-report.sh`), remove PROCESS.md's
  sanctioned-answer sentence. Split lane in flight.
  `done-check:` no output line asserts an arc is closed; the three known-unsound
  situations are VISIBLE in output where they apply; FORMAT.md §7 states the
  quiescence gap can produce a false GO (measured 3/3), not a refusal; the 185
  teeth re-pointed to diagnostic behaviour rather than deleted.
- **R5 device legs — DONE** (`df72146`): armv7 cross-compile break fixed
  (blocked EVERY device leg), persist p01 un-staled, 2 of R3's 3 floor facts
  settled.
- **R6 device follow-ups — IN FLIGHT** (main tree): persist decoder rewritten
  for the two-region PERSONAL BEST panel + `DECODE_SHA` re-pin; `--match-timer`
  device leg added to `check-device-foh.sh`.
  Already measured green: `PERSIST OK (… powercycle=reboot … teeth=29)` rc 0 and
  `DEVICE FOH OK (… vsfinish=1 … fbwit=23 p99=13.995ms)` rc 0.
  `done-check:` both terminal lines reproduce cold under a driver re-run.

### R4 POST-SPLIT ROUND — ran 2026-08-01, **VERDICT: NO-GO**. Merge NOT authorized.

The owner's budget was *"one Tier A round aimed at tooth-vacuity, then merge."*
The round ran (`.loop/review-r4split-r1-20260801.log`, 429385 bytes, 0 NULs,
terminal anchored `VERDICT: NO-GO`, `CODEX_RC=0`; findings taken from the LAST
`## Findings` block — the log echoes specimen files carrying foreign verdicts,
which is why 5 `VERDICT: GO` strings appear in it and none of them is this
round's). **The merge precondition failed, so the split stays unmerged in
`agent-ad3238e43fc13278b`.**

**The round found exactly what it was pointed at — the teeth ARE partly vacuous,
and the demotion is partly cosmetic:**

- `[MEDIUM]` `check-review-artifact.sh:1220` — **T89a/T94a/T96a assert
  boilerplate `arc-report.sh:1231-1244` prints UNCONDITIONALLY.** All three pass
  with their perturbations REMOVED: three false bits inside the cited `189/189`.
- `[MEDIUM]` `:825` — T55 discards the result with `|| true` and passes only
  because no `r008-*` residue exists. **Passes against `true`.**
- `[MEDIUM]` `:1101` — T82 accepts any nonzero failure when two paths are
  absent. **Passes against `false`**, never reaching reservation.
- `[MEDIUM]` `review-harness.sh:390` — every generated reviewer prompt still
  says "the judge" re-hashes paths before it "call[s] the arc closed".
  **Present-tense authority language survived the demotion.**
- `[HIGH]` `review-harness.sh:290` — **reviewer identity is forgeable by the
  writer the artifact exists to constrain.** The artifact records a fixed public
  descriptor hash while lines 411-416 exec bare `codex`/`grok`/`claude`; an
  exported Bash function or PATH shadow runs as a "built-in" reviewer AND keeps
  the genuine descriptor hash. T35 only covers explicit `--reviewer-cmd`. The
  reviewer's own note on the ceiling: *"strong provenance against a writer
  ultimately requires a driver-owned launcher or external signature."*

**Driver read — this NO-GO is a DIFFERENT SHAPE from the three that shelved the
judge, and that matters for the decision.** Those three each found a FRESH
false-GREEN class in a tool CLAIMING authority — non-convergence. These five are
bounded and individually fixable, and four are the kind of finding a round is
supposed to produce. The exception is the HIGH, which is a **design ceiling, not
a bug**: a tool a writer invokes cannot bind the identity of the reviewer it
invokes, against that same writer.

**OWNER DECISION — the one-round budget is spent.** Three options:
1. **Spend one fix round + one re-review** (5 findings; the 4 MEDIUMs are small).
   The HIGH cannot be closed inside the current design — it would have to be
   accepted as a registered limitation, scoped honestly: the artifact is
   tamper-EVIDENT against accident and sloppiness, not against a determined
   writer.
2. **Merge the producer with the HIGH registered as a stated limitation** and
   fix only the 4 MEDIUMs. Requires deleting or binding the 3 dead teeth FIRST —
   a cited `189/189` containing known-false bits is worse than a smaller honest
   count.
3. **Shelve R4 entirely.** It is a PROCESS tool; nothing on the M4 critical path
   depends on it. Costs nothing to ship, loses the provenance binding.

Driver recommendation: **(2)**, because the producer's value survives the HIGH
once the HIGH is stated rather than implied, and because R4 has now consumed
four review rounds on a tool that ships nothing to the device.

### R7 — B9 arc (blocks a green M4 gate)
`check-device-fullgame.sh` is `arc-in-flight` and red on `skips == 0`. All 12
legs are `STREAM MATCH 3600/3600 exact` in both passes; only skips fail, and they
MIGRATE between passes. NEW attribution (2026-07-31): the skip tracks per-leg
**MMC interrupt count**, not workload — pass 1 g06 = 2820 mmcirq (and the only
leg with `pswpin=7`) -> 7 skips, baseline 198-485; pass 2 m01 = 2227 -> 1 skip.
`low_bat_check` verified quiesced per leg in BOTH passes, so this is a **second
stall source distinct from the closed iter-74 class**, and the pre-leg `sync`
does not suppress it. p99 headroom 439-566 µs.
`done-check:` `check-device-fullgame.sh` prints `FULLGAME CONFORMS 12/12` rc 0 on
two consecutive passes, or the skip bar is re-based on a driver-recorded,
owner-visible measurement.

## OWNER RULING 2026-08-01 — B9 DEFERRED TO LAST; ship first

**R7 (B9 fullgame skip) is RE-ORDERED to the very end of all work** — after the
M4 gate, after Chase's acceptance playthrough, after shipping. Not abandoned:
scheduled last, and Chase will return to it.

**Mechanical consequence — do not paper over it.** The ruling alone does not
make the gate passable:
- `verify_m4.sh:23` — *any `arc-in-flight`/`arc-pending` producer = HARD REFUSAL*,
  and `check-device-fullgame.sh` is `arc-in-flight`.
- The fullgame bar itself contains `skips == 0` (`check-device-fullgame.sh:41`).

**R7a — B9 companion decision (do this instead of the fix, to make shipping
possible).** Two parts:
1. **Close the B9 arc as a §3 CAPPED closure.** It now has real attribution: the
   skip tracks per-leg **MMC interrupt count**, not workload (pass 1 g06 = 2820
   mmcirq and the only leg with `pswpin=7` -> 7 skips; baseline 198-485; pass 2
   m01 = 2227 -> 1 skip), and `low_bat_check` was verified quiesced per leg in
   BOTH passes — a **second stall source** distinct from the closed iter-74
   class. Naming that class IS what a capped record is for.
2. **Ratify a NAMED, BOUNDED, LOUD skip allowance** — the check keeps measuring
   and keeps PRINTING the count; the allowance is an owner-ratified deviation
   with its measurement recorded (D14 precedent); the **p99 bar is unchanged**.
   **A visible ratification, never a quiet edit.** HARD RULE 3 forbids weakening
   a check to make a run pass; the difference is whether the number stays on the
   page and whether the owner ratified it in writing.
`done-check:` the B9 CAPPED record exists and names the class; the fullgame check
still prints its per-leg skip counts; the allowance and its bound are recorded in
STATE.md §rulings; `verify_m4.sh` no longer hard-refuses on that row.

**Standing facts:** all 12 legs are `STREAM MATCH 3600/3600 frames exact` on
every pass — **the simulation is not implicated**, only the timing bar. p99
headroom is 439-566 µs against 16.67 ms.

### B11 — the device shot judge is STRICTER than the rig's own determinism (NEW, 2026-08-01; blocks R-item-1 and M4 gate leg [2])

**Found while re-running `check-device-foh.sh` on `ef31e53`'s final bytes.** The
run failed at `DEVICE FOH FAIL: shot f01-vs-g01/css: device shot != host twin
reference`. It is **NOT a product regression and NOT the B9 skip class** — it is
an unsound judgment in the rig.

**PROVEN root cause (host-only, reproducible in ~1 min).** The device shot is
**byte-identical** to a host twin run in which the flow's `I 708 U` hold is
**37 frames instead of 36**:

| artifact | sha256 |
|---|---|
| `.loop/b11-shot-jitter/device-css.ppm` | `51e0d8db…c00217` |
| `.loop/b11-shot-jitter/host-twin-u37-css.ppm` | `51e0d8db…c00217` (**==**) |
| `.loop/b11-shot-jitter/host-twin-u36-css.ppm` (committed flow) | `3abd60ff…5c7ec91` |

Reproduce: `MLFK_MENU_IMG1=$PWD/pipeline/build/sim-tables/assets/menu.img1
port/foh/build/device-foh/foh_dev_headless --flow .loop/b11-shot-jitter/f01-u37.flow
--input flow --flow-out /tmp/t.txt --shots-dir /tmp/s --pace 0` then
`cmp /tmp/s/css.ppm .loop/b11-shot-jitter/device-css.ppm`.

**Why this is the rig's fault, in the rig's own words.** `f01-vs-g01.flow`'s
header states it: *"The device path drives these scripts through a wall-clock
injector (flow-to-fkscript.js), so a hold can land +/-1 device frame off; anchoring
on a clamp makes leg (a) exact regardless, and every target below keeps more than
one frame of slack on each side of leg (b)."* The slack protects the **logical**
outcome and it worked perfectly — the device trace is **byte-identical** to the
twin trace (`transitions=5, shots=5`, every `S`/`T` on the same frame). What the
slack does not protect is the **resting pixel position**: the hand moves
**3.84 px/frame in y** (D3), so one frame of hold difference relocates it by
3.84 px — and the shot judge is byte-exact. The judge is therefore strictly
stronger than the determinism the injector provides, and passes only when the
jitter happens to land on the authored count.

**B9 is NOT implicated — measured, not assumed.** `f01-vs-g01.dev-applog.txt`:
`976 ticks, 5 transitions, 5 shots, 0 render skips, 0 failed presents`, and the
match phase likewise `0 render skips`. The +1 frame is injector wall-clock
jitter, not a game-side stall.

**Scope (CLASS, HARD RULE 8).** Every shot taken after a *counted* (non-clamped)
hold on a free-cursor screen: `f01-vs-g01/css`, `f02-cpu-m01/css-cpu`, and
`f05-vs-g03`'s css — 3 of the 15 judged shots. `f03-options`' shots sit on
discrete-row menus (no free cursor) and are unaffected. **This is also
`verify_m4.sh` leg [2]'s judgment** ("screenshot judges green"), so the gate
inherits the same intermittency.

**Decision required — OWNER-VISIBLE, because every option touches a judge.**
Driver recommendation: **(c)**, because it keeps byte-exactness intact.
- (a) Re-author the flows so the last positioning move before each shot ends
  against a screen clamp. Cheapest, but the port-2 type tab is mid-screen; there
  is no clamp to anchor on without changing what the flow demonstrates.
- (b) Position-tolerant shot comparison. **Rejected** — this is exactly the
  "weaken a check to make a run pass" that HARD RULE 3 forbids.
- (c) **Judge against the rig's DECLARED tolerance, still byte-exact:** the
  acceptance set for a jitter-exposed shot is the three host twins
  {hold-1, hold, hold+1}, each compared with `cmp`. It admits precisely the ±1
  frame the injector documents and nothing else — a genuine render divergence
  matches none of the three and still fails. The accepted variant is PRINTED, so
  drift stays visible on the page.
`done-check:` `bash port/foh/check-device-foh.sh` -> `DEVICE FOH OK (… shots=15 …)`
rc 0 on **two consecutive device runs**, with the per-shot accepted jitter offset
printed for each jitter-exposed shot; and a tooth proving a 2-frame offset still
FAILS.

### R8 — fix the B9 skip properly (LAST ITEM, post-ship)
Root-cause and remove the mmcirq-correlated stall. Owner will return to this
after shipping. Candidate levers already registered from earlier work: the
post-gate jitter increment (SCHED_FIFO for the frame loop with audio ranked
above, music reader on CFS) plus the SPIN_NS 3->2 ms retune with enough passes
for statistical power.
`done-check:` `check-device-fullgame.sh` prints `FULLGAME CONFORMS 12/12` rc 0 on
two consecutive passes with the ratified allowance removed.

## RECON 2026-08-03 (read-only agents; A14 + A7) — TWO FINDINGS THAT CHANGE PLANS

### A14 IS LARGE, AND THE DRIVER'S PLACEMENT RATIONALE WAS WRONG

**The atlas holds 43 glyphs and ZERO LETTERS.** `port/gfx/vfxglyphs-frozen.txt`
carries only `0-9` (×4 faces), `:` (font0), `%` (font2), space (font3), plus 2
pre-composited sprites (`ready`, `go`). The menus need **72 distinct characters
including all 26 uppercase letters**. Coverage today: **13 of 72**.
- **No face matches.** Menus need `700 35px Arial` upright and
  `italic 900 48px Arial`; the atlas has neither at any size. Font 3 is
  `italic 700 70px` — wrong weight, wrong size, 14 px tall vs face 2's 9.
- **Every layout constant dies.** Atlas advances are 4.449/2.781/4.716/7.786;
  `foh_font.c` advances are 6 and 7. Every column x, `text_in` clip width, the
  65 px SSS strips (`foh_render.c:55-58`) and 44/58 px name-plates (`:46-48`)
  are computed against 6/7 across a 2,198-line file.
- **The code path does not exist.** `gfx_glyphs_load` is unreachable from the
  menu path — `foh_dev.c:1802-1804` actively REJECTS `--glyphs` with no bridge
  mode active, which is exactly how menus run. It also takes `Gfx*` where the
  FOH has a bare `Raster*`.
- **BIGGEST RISK — the iter-72 jitter class reopens at ~10x surface.**
  `expected-render.json` freezes `maxDiffPixels = 16` / `maxChannelDelta = 4`,
  measured against 43 glyphs, under a rule that they are NEVER loosened once
  set. A menu atlas is roughly an order of magnitude more ink. Probe the
  EXTENDED atlas with `port/gfx/glyph-jitter-probe.js` and freeze new tolerances
  BEFORE the first passing done-check — the reverse order re-creates the flaky
  false oracle iter 72 existed to kill.

**DRIVER CORRECTION — the position-3 placement rests on a FALSE premise.** It
was justified by "A14 invalidates every menu shot reference, so land it first
and pay the re-capture once instead of twice." **There are no frozen shot
references.** Zero `.ppm` files are committed; the 15 judged shots are
device-vs-host-twin generated IN THE SAME RUN (`check-device-foh.sh:1583-1584,
:1845, :2068-2069`) and `check-foh-flows.sh` is run-A vs run-B self-consistency.
The `.expect` files are FOHTRACE1 state traces with no pixel content, so the 20
pinned `port/foh/flows/` manifest rows do not move either. Only ONE manifest row
is invalidated by rasterization: `m4-freeze-manifest.txt:459`
(`vfxglyphs-frozen.txt`). **The surviving argument for early placement is much
weaker** — only that A5/A7/D8-later would be authored against face 1's
uppercase-only constraint and re-laid afterwards, and since A14 re-lays EVERY
existing screen regardless, 2-3 more is incremental rather than multiplicative.
**Driver recommendation: move A14 back behind A5/A7 or return it to TIER 2.**
Owner call — the current position-3 slot stands until then.

### A7 IS NOT A CREDITS ROLL, AND IT NEEDS A RATIFICATION BEFORE CODE

Upstream `src/menus/credits.js` (422 lines) is a **Star Fox shooting gallery**:
a 100-star warp field, 14 scrolling wobbling names, a rotating reticle, twin
converging tapered lasers on A, X/Y laser-colour cycling, START/L/R
fast-forward, a 2500-frame (~41.7 s) auto-exit that plays `complete` or
`failure` by whether all 14 names were shot.
- **MENU-SPEC §8 (`docs/MENU-SPEC.md:1202-1291`) already specifies it**,
  including pre-registered **DEVIATION D12** (relative reticle integrating
  `lsX/lsY` — upstream's absolute `rawX/rawY` yields only 9 reachable d-pad
  positions) and **Quirk Q6** (unconditional exit timer). Transliterate against
  that section; do not redesign.
- The port can draw all of it: `fill_rect`, `lineW8`, `ring8`, `arc_pts`,
  `stroke_closed`, `rrect` already exist. Sounds all present in
  `sounds.json` (`foxlaserfire`, `targetBreak`, `complete`, `failure`).
- **BLOCKING — OWNER RATIFICATION REQUIRED BEFORE CODE.** credits.js calls
  `Math.random()` in three places (star spawn/respawn, name x, name wobble).
  **The FOH is RNG-free BY CONSTRUCTION** (`foh_app.c:21-23`), and the SSS
  RANDOM slot is a REGISTERED REFUSAL precisely because `Math.random` is the
  seeded oracle stream (`foh.h:76-80`). Substituting an authored table or an
  index hash is a **NEW DEVIATION CLASS — "we invented values upstream drew"** —
  landing in the one subsystem whose whole contract is "no invented values".
  This needs ratifying up front, the way D12 was pre-registered, or the arc gets
  rejected at review for exactly that reason.
- Face 1 lacks `&`, needed by two info strings; mixed-case names must be
  uppercased at the render site (`foh_render.c:1972` precedent).
- Cost is mostly EVIDENCE, not C: ~200-250 lines of C, but `judge-foh-trace.js`
  EDGES/REFUSED, `judge-grammar.frozen.txt`, the judge sha pinned twice in
  `check-device-foh.sh:237,419`, `judge-domains.authored.txt:223`, flow/shot
  counts (host `flows=7 shots=19`, device `flows=5 shots=15`), and manifest rows
  `:398,:399`. Size: MEDIUM.

**D8-later TICK-4 FINDING (2026-08-03) — THE ITEM'S PREMISE IS
SELF-CONTRADICTORY: THERE IS NOTHING TO BE FAITHFUL TO.**
D8 is a RATIFIED CUT, not an unbuilt feature. `docs/MENU-SPEC.md:514`:
*"DEVIATION D8 — name tags are cut."* Upstream's name entry is a **jQuery HTML
`<input>` overlay** (`css.js:438`, committed by `keys[13]`); the deviation table
at `:1719` records *"Name tags: keep random + clear, cut free-text entry"*, and
`:1575` gives the reason: **"No DOM, no keyboard."**

So "D8-later: a faithful NAME-ENTRY screen" cannot be a transliteration the way
A7 is. **Upstream has NO canvas name-entry code to port** — its implementation is
a DOM widget the browser draws, not game code. Any name-entry screen here must be
an INVENTED on-screen keyboard, which makes "faithful" unavailable by
construction. That is a DESIGN decision, and MENU-SPEC's whole mechanism for
those is pre-registration before code (D12's precedent).

Also measured: `MENU-SPEC.md:620` row 19 lists name tags as **"Not implemented"**
in the port with random/clear surviving the deviation — so even the two behaviours
D8 KEEPS are unbuilt. D8-later therefore splits into two distinct pieces worth
sequencing separately:
  (i) **random-tag + clear-tag** — these DO have upstream code
      (`css.js:415-439`) and are a real transliteration, S-sized
      (`MENU-SPEC.md:1759` groups them with the other CSS secondary widgets:
      *"All are cursor + A once (1) lands"*).
  (ii) **free-text entry** — no upstream reference; needs an invented soft
      keyboard and an owner-ratified UX before any code.

**OWNER DECISION NEEDED:** does D8-later mean (i), (ii), or both? If (ii), the UX
needs pre-registering the way D12 was. NOTHING EDITED.

**A13 OWNER ANSWERS 2026-08-03 (partial — one question still open).** Verbatim:
*"1. meleelight.opk is what I want to name it eventually  2. minimal fix is fine."*
- **Target OPK filename = `meleelight.opk`** for the play install, "eventually" —
  so the FILE rename is sanctioned but not necessarily in this pass.
- **MINIMAL FIX RATIFIED**: change the `Name=` fields in place; do NOT rename the
  `.desktop` files. Accepted consequence, stated on the page so it is not
  rediscovered as a bug: the file called `meleelight-foh.funkey-s.desktop` will
  carry the clean title `Name=MeleeLight`, and `meleelight.funkey-s.desktop` will
  carry `Name=MeleeLight EV`. File names and titles are deliberately crossed.
- **STILL OPEN:** whether `/mnt/Applications/meleelight-foh.opk` (the Jul 29
  build currently installed) is the PERMANENT play install, since
  `OPK_INVENTORY_PIN` must record whatever is really there before NAV_LINK can be
  re-measured.

**A13 TICK-3 FINDING (2026-08-03) — A13 IS ENTANGLED WITH A LIVE DEVICE-STATE
DRIFT, AND THE OPK GATE LEG IS ALREADY RED FOR AN UNRELATED REASON.**
MEASURED against the attached device (`find /mnt -maxdepth 2 -name '*.opk'`,
24 entries each side, AppleDouble `._*` excluded). Exactly ONE substitution vs
`check-device-opk.sh`'s `OPK_INVENTORY_PIN`:
- IN PIN, ABSENT FROM DEVICE: `/mnt/Applications/meleelight.opk`
- ON DEVICE, ABSENT FROM PIN: `/mnt/Applications/meleelight-foh.opk` (Jul 29 2026)

The owner replaced the old play install with the FOH build. Everything else in
the 24-entry inventory matches.

**CONSEQUENCES, all measured not assumed:**
1. `check-device-opk.sh` step [5] fails at the inventory pin — *"the frontend OPK
   inventory differs from the pin the frozen navigation was measured against"*
   (`:817`). **The M4 gate's OPK leg is therefore ALREADY RED, for a device-state
   reason with no code involved.** The guard is behaving exactly as designed —
   it dies loud with a re-measure instruction rather than mis-navigating.
2. **Both NAV_LINK pins are stale.** They were measured against the grid
   *link 0 "Mario 64" · link 1 "MeleeLight" (play install) · link 2 "MeleeLight
   FOH" (ours) · link 3 "PicoArch"* (`:125-129`). With `meleelight.opk` gone,
   "MeleeLight FOH" moves to link 1; the FOH arm's `NAV_LINK=2` (`:280`) and the
   other arm's `NAV_LINK=1` (`:293`, "measured grid link of MeleeLight") both no
   longer describe the device.
3. **A13's rename moves the grid too**, since gmenu2x orders the games grid
   alphabetically by `.desktop Name`. So A13 and this drift MUST be fixed in ONE
   re-measure pass — doing them separately means measuring the grid twice and
   re-pinning twice.

**WHY THIS STOPS HERE — OWNER DECISION, NOT A TASK.** `OPK_INVENTORY_PIN`
encodes *what is installed on Chase's own device*. Re-pinning it silently would
be the driver deciding what his handheld should contain. Two things are needed
before A13 can be executed:
  (a) **Is `meleelight-foh.opk` the intended permanent play install, and should
      `meleelight.opk` stay gone?** If a `meleelight.opk` is coming back, the
      grid gains an entry and the re-measure changes again.
  (b) **Confirm the A13 end-state titles** now that the collision A13 was written
      against has changed shape. With no `meleelight.opk` present, the only title
      collision left is between the FOH play build and the EVIDENCE OPK
      (`mlfk-evidence.opk`, `meleelight.funkey-s.desktop`, `Name=MeleeLight`).
      Minimal fix satisfying A13's stated end state:
      `meleelight-foh.funkey-s.desktop` -> `Name=MeleeLight` and
      `meleelight.funkey-s.desktop` -> `Name=MeleeLight EV`. NOTE this leaves
      file names and titles deliberately crossed (the file called `-foh` carries
      the clean title); renaming the FILES too is more churn and more pin
      surface for zero functional gain — say so if the tidier layout is wanted.

**NOTHING WAS EDITED.** No `.desktop`, no pin, no NAV_LINK. The A13 work itself
is small; it is gated on (a) and (b), and on one device grid re-measure that
should cover the drift and the rename together.

**VERIFICATION SWEEP 2026-08-03 (tick 2) — A13, D8-later and WJ-later are ALL
GENUINELY OPEN.** The stale-row streak ends at three. Each re-verified by direct
read, with scope now precise:

- **A13 OPEN — and the titles are INVERTED relative to the goal.** Both .desktop
  files already exist with distinct names, so the surface looks done; it is not.
  `check-device-opk.sh:270-292` shows the two build paths: `OPK_FOH=1` builds
  **`mlfk-foh-launch.opk`** (bin `foh_device`, the REAL front-end = the PLAY
  install) carrying `meleelight-foh.funkey-s.desktop` -> **`Name=MeleeLight
  FOH`**; the else branch builds `mlfk-evidence.opk` (bin `gfx_device`, the
  evidence build) carrying `meleelight.funkey-s.desktop` -> **`Name=MeleeLight`**.
  **The clean name is on the EVIDENCE build and the play install wears "FOH"** —
  exactly backwards from A13's stated end state. Fix = swap the names (play ->
  `MeleeLight`, evidence -> a distinct title such as `MeleeLight EV`), and
  **re-measure `NAV_LINK`** (`check-device-opk.sh:280`, currently 2 "measured
  grid link of MeleeLight FOH") because the frontend grid orders by title, so
  renaming moves the link the nav pin depends on. Both .desktop files are pinned
  m4 producers.
- **D8-later OPEN.** No name-entry screen exists anywhere: zero hits for
  `FOH_NAME`/`NAME_ENTRY`/`nameEntry` across foh.h, foh.c and foh_render.c.
- **WJ-later OPEN, and precisely scoped: the toggle is PLUMBED but the SIM
  IGNORES IT.** The option exists and is selectable (`foh.c:884`
  `ev_sel(s, "walljump", s->everyCharWallJump)`), and it reaches the launch line
  (`foh_dev.c:2241` emits `walljump=%d`; the grammar is pinned at
  `check-foh-flows.sh:332`). But **`everyCharWallJump` appears NOWHERE under
  `port/sim/`** — the sim's only walljump input is the per-state table flag
  (`physics.c:775-777`, `canWallJump = FLAGS(S,i)->wallJumpAble`). Upstream's
  semantics live at `src/menus/css.js:89,109` and `gameplaymenu.js:53,239`.
  So WJ-later is not "wire a menu row" — it is "make the setting reach and
  change the simulation", which lands on the CHECKSUM surface and therefore
  needs its faithfulness argument made up front.

**TIER 1 REMAINING, ALL FIVE CONFIRMED OPEN: A7 · A13 · D8-later · WJ-later ·
A14.** A7 is blocked on the Math.random ratification; the other four are not
blocked.

**MEASURED 2026-08-03 — TIER 1's FIRST THREE ROWS ARE STALE. A3, A4 AND A5 ARE
ALREADY BUILT.** Verified by direct read, not inference:
- **A3 DONE.** `port/gfx/s1_input.h:165` names it: *"The ROLE resolution, in ONE
  place (fix_plan A3 + the 2026-07-29 Mod-shoulder swap)."* `ctl_roles()`
  (`:173-183`) gives NORMAL and NATURAL `*shield = (p->l || p->r)` — L is a
  second shield button — and BOX puts Mod on one shoulder, shield on the other,
  swappable via `modOnR`. `:11` states it outright: *"with L joining R as a
  second shield button (A3)"*. The row builder emits `in.r = true` on shield
  (`s1_input_row_style`), NOT `in.l` — deliberate: `:37-38` records digital
  shield as `r=true, rA=1.0`, single-stage, no light shield, `l/lA stay
  false/0`. So "currently unbound" has been false since ~2026-07-29.
- **A4 DONE.** Three styles exist, switchable and persisted:
  `CTL_STYLE_BOX|NORMAL|NATURAL` with per-style chord tables
  (`ctl_style_table`, `s1_input.h:151-158`), `ctl_style_get/set`
  (`ctl_style.c:18-22`), persisted through `FohPersist.ctlStyle`.
  **DISCREPANCY TO RESOLVE:** the A4 row says *"box + normal, switchable,
  normal default (owner ratified semantics 2026-07-27)"*, but the build has
  THREE styles and `CTL_STYLE_DEFAULT` resolves to **NATURAL**, described at
  `s1_input.h:12-14` as *"the ssb64-modelled 1:1 scheme and the DEFAULT"*. Either
  the semantics were re-ratified after 2026-07-27 and this row was never
  updated, or the default drifted unratified. **RESOLVED 2026-08-03: NATURAL is correct — the CODE was right and the ROW was
  stale. Owner: "natural should be default." No code change.**
- **A5 DONE.** The Controls screen selects: `foh.c:1055-1075` — up/down toggles
  `ctlRow`, left/right on row 0 cycles the three styles wrapping over
  `CTL_STYLE_COUNT`, row 1 flips the Mod shoulder via `ctl_mod_on_r_set`. Enum
  values are a frozen wire format stored verbatim in persist.

**TIER 1's REAL REMAINING CONTENT is therefore: A7 · A13 · D8-later · WJ-later ·
A14.** A7 and A14 are confirmed open by direct read (A7 is still
`ev_refused(s,"credits")` at `foh.c:261`; `foh_render.c` never calls
`gfx_glyphs_load`). A13, D8-later and WJ-later are NOT yet re-verified — given
three stale rows in a row, verify each before starting it.

## OWNER RULINGS 2026-08-03 (round 2) — A13 unblocked, WJ pre-registered, D8 redirected, A4 settled

Verbatim: *"2. you can replace and do whatever you want. we are always working
here, you are the only one who has ever put .opk on my device. you can delete
that, pin whatever. just use best practices, what our code reflects, what
testing reflects, etc.  3. pre-register it please.  4. why not match the real
melee name select? novel screen (doesn't match the original melee light). that'd
be awesome  5. natural should be default."*

- **A13 FULLY UNBLOCKED.** Owner grants full authority over device OPK state:
  the driver is the only party who has ever installed an OPK there. Sanctioned:
  delete the stale `/mnt/Applications/meleelight-foh.opk`, install under the
  target name **`meleelight.opk`**, re-pin `OPK_INVENTORY_PIN` to measured
  reality, and re-measure both `NAV_LINK` values. Do it by best practice and by
  what the code and tests already reflect. The minimal `Name=` swap stands
  (crossed file/title names accepted, recorded above).
- **WJ-later: PRE-REGISTER, ratified.** The walljump setting must reach the
  simulation, which is the CHECKSUM surface. Write the faithfulness argument and
  the exact change BEFORE code, owner-visible, the way D12 was pre-registered.
- **D8-later REDIRECTED — NOVEL SCREEN, MODELLED ON REAL MELEE.** Owner:
  *"why not match the real melee name select? novel screen (doesn't match the
  original melee light)."* This is a DELIBERATE DEPARTURE from upstream, chosen
  knowingly: upstream's name entry is a jQuery DOM `<input>` with no canvas
  code, so there was never anything to be faithful to (see the tick-4 finding).
  Real Melee's name entry is a LETTER GRID driven by stick + A — which suits a
  d-pad handheld far better than a text box. **This needs registering as a NEW
  DEVIATION in MENU-SPEC** (it is the first invented SCREEN, distinct from
  invented values), including which Melee behaviours are modelled and which are
  not. Scope: (i) random-tag + clear-tag are still a real upstream
  transliteration (`css.js:415-439`) and can land independently; (ii) the letter
  grid is the novel work.
- **A4 SETTLED: NATURAL is the correct default.** The code was RIGHT and the
  punch-list row was stale — no code change. The 2026-07-27 row's *"normal
  default"* is superseded. `CTL_STYLE_DEFAULT` -> NATURAL stands as built.

## OWNER RULING 2026-08-03 — RE-PRIORITIZATION; B11 DEFERRED (Chase, in session)

Verbatim: *"let's skip B11 I am totally done with all this for now. defer it
until after we're done. … A3, A4/A5, A5, A7, A13, D8-later, wj-later are all
priority now (in that order). move everything to after that (preserve order)."*

### The new order of work

**TIER 1 — do these first, in this order:**
1. **A3** (P1) L shoulder = shield/air-dodge. Currently unbound: keymap-frozen.txt
   maps `l -> K k` / `r -> N n` as raw keysyms only, with no shield/air-dodge
   binding behind them.
2. **A4** (P1) control-style system: box + normal, switchable, normal default
   (owner-ratified semantics 2026-07-27). `port/gfx/ctl_style.c` exists and is
   already persisted (foh_app.c:458 load, :511 save) — the plane is built; what
   is missing is the user-facing half.
3. **A5** (P1) Controls screen selection wired (candidate: the style selector).
   NOTE: the owner's message listed "A4/A5, A5" — read as A4 then A5, and the
   duplicate treated as a slip. If A5 was meant to sit somewhere else in the
   order, say so and this list is amended.
4. **A7** (P2) Credits functional. A16 already measured upstream credits.js as
   422 lines of ZERO-DOM canvas code — this is a TRANSLITERATION, not a new
   feature. The menu label "CREDITS" already exists (foh_render.c:39); no
   credits screen exists in the FohScreen enum.
5. **A13** (P2) app title on the FunKey home screen must not read "FOH".
   Constraint that produced it is unchanged: check-device-opk.sh NAVIGATES BY
   TITLE, so the PLAY install and the EVIDENCE OPK must keep DISTINCT titles.
   End state: PLAY = `Name=MeleeLight`, EVIDENCE = a distinct title. Both
   .desktop files are PINNED m4 producers — carries its own arc, re-pin and one
   device nav verification.
6. **D8-later** a faithful NAME-ENTRY screen.
7. **WJ-later** make walljump actually work.
8. **A14** (P0) glyph-atlas swap — menus draw with the BROWSER-RASTERIZED
   VFXGLYPHS1 atlas (`port/gfx/vfxglyphs-frozen.txt`) instead of the
   hand-authored `port/foh/foh_font.c`. Extend `__gfxDumpGlyphs()` to cover the
   menu strings, point `foh_render.c` at `gfx_glyphs_load()`, keep foh_font.c as
   a LOUD fallback.
   **MOVED HERE (tier-1 LAST) 2026-08-03 BY DRIVER JUDGMENT** under the owner's
   second delegation (*"ok move a14 wherever you want"*), after recon REFUTED
   the rationale for its original position-3 slot. NOT an owner ruling;
   amendable. Reasoning, so the choice can be argued with:
   - **The relayout cost is POSITION-NEUTRAL.** The position-3 slot was
     justified by "landing it first pays the shot re-capture once instead of
     twice." That premise was false (no frozen shot references exist). The true
     shape: A14 re-lays whatever screens exist when it lands, and screens built
     AFTER it are authored against the new font. **Either order costs exactly
     ONE relayout pass.** So relayout argues for no position at all, and the
     decision falls to risk sequencing.
   - **Risk sequencing puts it last.** A14 is LARGE (two new faces, ~63 new
     codepoints each, every layout constant in a 2,198-line file, plus a code
     path that does not currently exist), and it carries a LIVE ORACLE HAZARD —
     the iter-72 glyph-jitter class reopening at ~10x surface against a
     never-loosenable `maxDiffPixels = 16`. Nothing functional should queue
     behind that.
   - **It is cosmetic; every item the owner named is functional.** A3/A4/A5/A7/
     A13/D8/WJ are behaviour. A14 is how the text looks. It also belongs with
     A1, the look-fidelity effort it was folded into by the A8 audit — sitting
     at the tier-1/tier-2 boundary puts it adjacent to that work.
   - MEASURED OPEN (2026-08-03): foh_render.c has ZERO references to
     `gfx_glyphs_load` and still cites foh_font.c at :574, :1878, :1926, :1972.
   - COST TO EXPECT: `vfxglyphs-frozen.txt` is a PINNED producer with a
     `reviewed-go` row, so this carries its own arc + re-pin, and glyph
     comparison is already known-touchy (the iter-72 glyph-jitter class fix).
   - NOT a B11 hazard: shot REFERENCES are host twins and the host is
     deterministic — the ±1 jitter is device-injector wall clock only. So the
     re-capture is safe to do with B11 still deferred; the 3 jitter-exposed
     shots stay coin flips on device, unchanged in kind.

**TIER 2 — everything else, ORDER PRESERVED as it stood before this ruling:**
B11 (below), then A1/A14/A15 look-fidelity remainder, A6, A16, U1, U2, D6-later,
C3, the R-items, and R8 last per the 2026-08-01 ruling.

### B11 — DEFERRED (was: blocks R-item-1 and M4 gate leg [2])

B11 is **NOT cancelled and NOT descoped.** The owner's 2026-08-01 ratification
of the three-way byte-exact acceptance set STANDS; only its POSITION moves, to
after Tier 1 and after the rest of Tier 2.

**The mechanical consequence, stated rather than papered over — the same
discipline the 2026-08-01 B9 deferral demanded:**
- B11's defect is INTERMITTENT, not deterministic. `check-device-foh.sh` and
  `verify_m4.sh` leg [2] judge 3 of 15 shots (f01/css, f02/css-cpu, f05/css)
  against a rig whose own injector documents +/-1 device frame of jitter. Those
  legs therefore PASS OR FAIL BY LUCK until B11 lands.
- So while B11 is deferred, **a green FOH leg is not evidence the judgment is
  sound, and a red one is not evidence of a regression.** Any FOH/gate red on
  one of those three shots must be checked against the B11 signature FIRST
  (device shot == a hold+/-1 host twin, byte-exact) before anything is
  "diagnosed".
- The generator is already committed (`1be6abd`,
  `port/foh/make-jitter-flow.js`), validated end to end: of the 3x3 product over
  f01's two counted legs, exactly one (x0/y+1) is byte-identical to the real
  device shot. That work is banked, not lost.

`done-check:` unchanged from the B11 block above — `DEVICE FOH OK (… shots=15 …)`
rc 0 on two consecutive device runs, accepted jitter offset printed per
jitter-exposed shot, plus a tooth proving a 2-frame offset still FAILS.
