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
- A13 — DONE 2026-08-04 (see docs/AGENT-LOG.md). Three roles, three
  .desktop files, three distinct titles; play install rebuilt as
  /mnt/Applications/meleelight.opk by the new committed producer
  port/gfx/opk/install-play-opk.sh.

- A20 — DONE 2026-08-04 (see docs/AGENT-LOG.md). Per-shot envelopes,
  re-measured over 3 device runs (both shots byte-identical in all three).
  startup band UNCHANGED; title ceiling 60% -> 80% and a NEW >=64-colour
  floor that the old envelope never had. 6 teeth, `.loop/a15-teeth.log`.

- A21 — DONE 2026-08-04 (see docs/AGENT-LOG.md). Cite token re-worded to
  `skips-eq-frames`, cite charset documented in the manifest's own record
  grammar, MANIFEST_SHA256 re-anchored. verify_m4.sh now clears the grammar
  stage. It refuses one stage LATER, on A22 — not on arc-in-flight yet.

- A22 — DONE 2026-08-04, owner route (b) (see docs/AGENT-LOG.md). Bytes
  re-pinned to the committed file, status corrected `reviewed-go` ->
  `arc-in-flight` (which is what the manifest's own UNCLOSED ROWS record
  already said). Step [0] is now CLEAN — 89 producers, all pins match, 185
  cited artifacts exist — and the gate reaches [0b], its real blocker.

## M4 GATE — THE REMAINING BLOCKER (state of record 2026-08-04)
`bash port/sim/device/verify_m4.sh` reaches **[0b]** and refuses on **6**
producers lacking review closure, all `arc-in-flight` (evidence
`.loop/a17-verify-m4-final.log`):
`check-device-fullgame.sh` · `check-device-foh.sh` · `check-device-opk.sh` ·
`expected-assets.json` · `check-assets-expected.js` · `verify_m4.sh`.
This is the TRUE blocker list — earlier pages said five, and the gate had
never reached this stage to contradict them. Closing these is review-arc work
(PROCESS §3 -> §4), not code: the B9 arc owes a fresh round covering the
frame-conservation repair, and the A13/A20 edits to check-device-opk.sh ride
the same in-flight row.

## ARC m4-close-20260805 — ATTEMPTED, CANNOT CLOSE TODAY (all three reviewers)

One combined round was launched over all 8 unclosed producers plus the code
riding them (scope + prompt: `.loop/arc-20260805/`). **All three reviewers are
unavailable for a DIFFERENT reason, each recorded as an artifact:**

| round | reviewer | outcome | why |
|---|---|---|---|
| 1 | codex (primary) | VOID rc=1 | **Out of credits until 2026-08-07** — `ERROR: You've hit your usage limit` |
| 2 | grok (§11 fallback) | VOID rc=0 | `grok --prompt-file … --permission-mode plan` ACKNOWLEDGES and exits after one message. It never reviews, so there is no verdict to read. **The invocation in `port/review/reviewers.sh:27` does not work for this purpose.** |
| 3 | opus5 (§11 fallback) | VOID rc=0, `void-reason: no-verdict` | Produced a REAL review with findings, then wrote `**VERDICT: GO**` — markdown-bolded, so not an anchored line. Correct refusal: the anchor is what stops verdict laundering. |

**PROCESS §11's fallback needs BOTH pair members**, so opus5 alone cannot close
the arc even after its formatting is fixed. **The arc is blocked until codex
returns (2026-08-07) or `reviewers.sh`'s grok invocation is repaired.**

**Round 3's findings were acted on immediately** — both Mediums were driver
regressions from this same session (the manifest's stale "SIX" count and a
line-number citation that rotted in the commit that wrote it). Fixed; see the
2026-08-05 commit. That is the round paying for itself despite being VOID.

**Prompt already hardened** for the re-run: `.loop/arc-20260805/prompt.md` now
states the verdict-line rule literally, with the round-3 failure as the reason.

## A14 SECOND HALF — DESIGN DECIDED 2026-08-05 (do not re-litigate)

**Approach: swap the IMPLEMENTATION, not the call sites.** Rewrite
`foh_text2`/`foh_text` BODIES onto the atlas; leave all 41 call sites alone.

*Why, and what was rejected.* The expensive, risky part of this change is NOT
the 41 mechanical edits — it is the relayout and the frozen-shot re-freeze (15
menu shots plus their device twins). Swapping the body decouples them: land the
render change with call sites untouched, SEE what moved, tune metrics in ONE
function, re-freeze ONCE.
- REJECTED, big-bang (edit all 41 sites + relayout + re-freeze together): a
  wrong shot then cannot be attributed to a call-site edit vs a metric change.
- REJECTED, per-screen behind a flag: it multiplies the one genuinely expensive
  artifact — frozen shots x device twins — by the number of screens, and buys
  granularity on the part that was never risky. It also puts two fonts on
  screen at once mid-transition.

*The seam already exists and needs no adapter.* `gfx_glyph_text` is
`Gfx`-based only at the surface: `blit_mask` calls
`rast_blit_a8mask(&g->rz, ...)`, so the atlas renderer is RASTER-based
underneath. Add `gfx_glyph_text_rz(Raster*, fontId, s, penX, penY, fill,
stroke, strokeFirst)` in `gfx_overlay.c`, have `gfx_glyph_text` delegate to it,
and call it from `foh_text2`. That is an API extraction, NOT a translation
layer — do not build a coordinate-converting wrapper, which is the failure mode
this approach risks.
- Font mapping: italic -> spec 3 (`italic 700 70px Arial` IS upstream's menu
  weight), non-italic -> spec 0.
- `foh_text2_width` must move to `gfx_glyph_text_width`; today it returns
  `(n*7-1)*scale` from FIXED cells, while atlas advances are per-glyph device
  px. **That difference IS the relayout.**
- `foh_font.c` face 2 stays as a LOUD fallback (`gfx_fatal` on a missing
  glyph, never a placeholder box — a silent fallback would have hidden exactly
  the D8 bug). Delete the 6x9 face only once green.
- MEASURE FIRST: diff atlas advance widths against `(n*7-1)*scale` per menu
  string BEFORE touching layout. That predicts which screens shift instead of
  discovering it in shots.

*Deliberately NOT started this session:* the seam alone, with no swap behind
it, is scaffolding for later. It lands with the swap or not at all.

## D20 MARTH WITNESS (#16) — still open, and the instrument was the story

**State:** D20 has a REGRESSION proof (8/8 goldens bit-identical flag-off), a
CRASH proof (the tooth found puff's missing WALLJUMP ECB on its first run) and
a MEASURED scope (marth is the only character it adds). The marth path itself
has still never executed.

**2026-08-05 attempt, and why its null results are trustworthy only after the
third fix.** Twelve synthetic traces were swept and all reported "no
divergence". All twelve were MEANINGLESS, for three separate instrument bugs
found in sequence:
1. `lsX` set but `rawX` left 0 — the real traces set BOTH, because
   interpretInputs derives the processed stick from raw.
2. Slot separators written as `... | | ` — a SPACE where slot 2 begins, so
   every run died `SIM FATAL frame 0: trace: short slot token list` and wrote
   an EMPTY stream. Two empty files `cmp` as identical, so the sweep reported
   "no divergence" while nothing had run at all.
3. The comparison itself was never sanity-checked against a known-good
   positive until last.
Correct row form: `slot0 + "|" + slot1 + "||"` — bare pipes, no spaces, slots
2/3 empty. With the instrument fixed and PROVEN (null vs J1 diverge), six
walk+jump+drift patterns across stages 0 and 1 still produce **zero** marth
wall-window frames. That result is real; the earlier ones were not.

**THE LESSON, which cost more than the feature:** a null result from an
unverified instrument is not evidence of anything. Prove the instrument can
produce a POSITIVE before trusting a negative — here, one `cmp` of a
known-moving input against a null input would have caught all three bugs
immediately, and it was the last thing tried instead of the first.

**Next attempt should stop guessing at geometry:** read the wall segments out
of the STAB1 stage data and aim at a measured x, rather than sweeping
walk-off-and-drift patterns and hoping.

## GATE CONSUMER/PRODUCER GRAMMAR AUDIT — 2026-08-05 (all four verdict consumers)

Triggered by B8 finding `FOH_RE` stale against its own producer. Every anchored
consumer grammar in `verify_m4.sh` was then checked the SAME way: extract the
producer's `echo`/`printf` format, split off shell expansions, and require every
literal chunk to appear in the consumer regex. **No device run needed — a
literal in a format string is derivable from source.**

| consumer | producer | literals | pinned values | verdict |
|---|---|---|---|---|
| `FULLGAME_RE` | `check-device-fullgame.sh:1380` | all present | `12/12` == `N_GOLDENS_PIN=12`; `allow12` == `SKIP_ALLOW_PER_RUN=12`; `g07,g08,m01,m02` == `PINNED_LIVE_AI_SET`; `teeth=25` == last green run | **OK** |
| `TARGET_RE` | `check-device-target.sh` | all present | `teeth=6` == 6 col-0 increments | **OK** |
| `FOH_RE` | `check-device-foh.sh:2328` | **`shots=15` and `vsfinish=1` MISSING** | `teeth=15` was right pre-B8 | **WAS BROKEN — fixed** |
| `OPKFOH_RE` | `check-device-opk.sh` | exact literal | none | **OK** |

**NOT verified, and not claimed:** the DYNAMIC pins that only a device run can
produce — `TARGET_RE`'s `fbwit=4` / `sfxpin=15/31` / per-flow `starts`, and
`FOH_RE`'s `fbwit=15`. Literal STRUCTURE is proven; those VALUES are not.
`teeth` in fullgame is loop-dependent (its tooth at `:2855` is indented and runs
per leg), so 25 is not a col-0 count there — do not "correct" it to 5.

**Why this class matters more than it looks:** a stale consumer only bites when
the leg PASSES, i.e. after every arc closes — the worst possible moment, and
invisible until then because the arc refusal fires first. Two instances now
(FULLGAME_RE 2026-08-01, FOH_RE 2026-08-05). Re-run this audit after ANY edit
to a producer's verdict line.

## PUNCH-LIST STATUS RECONCILIATION 2026-08-04 (read this before starting ANY row)

The rows below are a LOG, not a queue: a completed item keeps its original
bullet and gains a separate `X DONE` line further down the file. Reading a
bullet alone therefore tells you nothing, and a text scan that ignores the
supersession lines reports A3/A4/A5 as open when they are built. That
convention is fine, but it must be READ correctly — this block is the index.

**SUPERSEDED / DONE (marker line cited):** A1 Phase-1 merged (L2180) · A2
(L2118) · A3 (L3128) · A4 (L3137, semantics SETTLED L3190) · A5 (L3148) ·
A9 (L2095) · A11 + A12 (L2180) · A13 (L2006) · A17 (L2199) · A18 (L2200) ·
A20/A21/A22 (2026-08-04) · B3 (L2136) · B4 (L2242, falsified-and-closed
2026-08-23 — it was never open; A19's own row L2493 was stale the same way) ·
B9 (L2196) · U1 (L2331).
**OWNER-DEFERRED:** B11 (2026-08-03, deferred NOT cancelled).
**OWNER-DECIDED, no work:** A10 ("No work now" until after A1) · A19
(restart deliberately skipped — upstream has no restart-match semantics).

**PROBED AGAINST THE TREE 2026-08-04 — genuinely OPEN, with the evidence:**
- ~~**B1 (P1) OPEN.**~~ **WRONG — B1 IS FIXED. Corrected 2026-08-05.**
  `port/gfx/raster.c:87-93` blends the three channels SEPARATELY, which is
  precisely the prescribed fix, and the comment above it (`:81-86`) states it.
  RUN, not read: `rgb(147,14,42)` at `a=87` over `rgb(39,0,91)` now yields
  blue 5-bit **8** (the arithmetic value is 8.96, i.e. the row's "correct 9"
  truncated), against the reported buggy **25**. Landed in `8ff1aff`.
  **HOW THE 2026-08-04 PROBE GOT IT WRONG, because the mistake is the useful
  part:** it grepped for the SYMBOL (`blend565` still present at `:87`) and
  for a COMMENT (`:136` "dodges blend565's blue spill" — true, and about a
  different, still-valid perf choice), and concluded "routed around, not
  fixed". Both observations were accurate and the conclusion was false. A
  probe must exercise the BEHAVIOUR the row describes: compiling the function
  and feeding it the row's own numbers took two minutes and was decisive.
  Grepping for a defect's vocabulary finds the discussion of the defect, not
  the defect.
- **B7 (P1) OPEN.** The interim canonical-phase seam is live —
  `foh_look_canonical` called at `port/foh/foh_app.c:548` and referenced at
  `foh_dev.c:2273`. B7 replaces it with the device→host look-plane injection.
- **B8 (P1) OPEN.** `check-device-fullgame.sh` still reads `teeth=21`
  (`:1375`, `:2048`); B8 is the 21 -> 22 machine-plane tooth.
- **C7 (P2) OPEN.** `--seed 1337` is literally on the play path,
  `port/gfx/opk/mlfk-foh.sh:166`.
- **A14 (P0) OPEN, and its shape is CORRECTED at L2067** — VFXGLYPHS1 has
  ZERO letters, so the swap cannot land as-is; the corrected sequence there
  is binding.
- **A7 (P2) OPEN and BLOCKED** on the owner's `Math.random` ruling.
- **B10 (P1) OPEN but RESTATED:** it says "flip the 12 arc-in-flight rows".
  MEASURED 2026-08-04: the gate refuses on **6**, not 12 — see the M4 GATE
  block above. Use the gate's `[0b]` output, never this count.

**PROBED AND FOUND ALREADY SATISFIED:**
- **B6 (P1) DONE.** `check-device-target.sh:1203` names exactly the missing
  combination — "`--bridge live` + a TARGET launch — the exact play-path
  combination" — and drives it at `:1246`.
- **A6 (P2) effectively DONE** by its own measured 2026-08-03 text; the
  residual is the live-mixer bus push at `foh.c:975-977`, not a screen build.

**ALL 15 NOW PROBED (2026-08-05) — the block below replaces "status unknown".**

*Found ALREADY SATISFIED (close them):*
- **A8** — `.loop/reuse-audit/REPORT.md` exists and its consequences are filed
  as A14/A15/A16/U1/U2. The audit was delivered; the row outlived it.
- **B2** — answered by B9's measurement: title render 16.96 -> 9.06 ms, margin
  -0.29 -> +7.61 ms. The "UNVERIFIED on hardware" claim is stale.
- **B5** — `port/sim/target/target_finish_probe.c` EXISTS; foh.h already says
  "the finish probe owns mechanical coverage". The FINISH arm is covered.
- **C8** — `.loop/review-c1-opus.log` EXISTS. The §11 evidence gap is closed
  (mlfk-foh.sh is arc-in-flight again, but for C7's reasons, not this one).
- **A16** — not work at all: it is a FRAMING note recording that audiomenu/
  credits/keytest/keyboardmenu are transliterations. Nothing to build.

*Confirmed STILL OPEN, with the probe that says so:*
- **A15** (P1) menu-look ORACLE — `port/gfx/capture-canvas.js` contains no menu
  capture; its only `css` hit is a MIME-type string. Not built.
- **U2** (P2) — `gfx-pagelib.js` has ZERO `target` hits, so target rasterization
  still has no browser judge (twin-vs-device only).
- **U4** (P2) — `check-device-render.sh` was last touched at iter 80, BEFORE
  U1's `col8()` rounding changed background pixels. The device evidence it
  pins was never re-taken.
- ~~**B4** (P1) — `FOH_TMATCH` has no exit `ev_trans` in `foh.c` (only the
  entry at `:802`), so target-match is still terminal in the machine.~~
  **WRONG — B4 IS FIXED. Corrected 2026-08-23 (lane M).** The premise is a
  true grep with a false conclusion, and the instrument disproves itself:
  `FOH_MATCH` has no exit `ev_trans` either (only the entry at `foh.c:777`),
  yet the VS match exit demonstrably works. Terminal-in-`foh_tick` is the
  DESIGN — `foh.c:1175-1177` says the driver owns the sim from there — and
  the exit lives one layer out, in `foh_dev.c`: a live target match's START
  quit fires upstream's own `endGame` through `tp_endgame_hook`, sets
  `g_mexit = MEX_TSS` (`foh_dev.c:2803`; the natural finish does the same at
  `:2830` after the 2500 ms hold), and the `foh_phase:` re-entry maps that to
  `foh.screen = FOH_TSS` (`foh_dev.c:3587-3589`) — upstream's
  `changeGamemode(7)` (main.js:1389-1395, the only reachable side of its
  `targetTesting` branch in this port). Landed with the C18/C19/B4/A19
  increment. RUN, not read: `bash port/foh/check-mexit-reentry.sh` →
  `MEXIT REENTRY OK`, exit 0 — its `[4c]` leg drives the real `foh_tick`
  through a live target match, quits it with START, and proves the re-entry
  landed on TARGET SELECT (phase 2 launches a target match with `char=3`, a
  character phase 1's script cannot reach). Tooth proven live the same day:
  flipping `:2803` to `MEX_OS` (the pre-fix "drops to the frontend"
  behaviour) makes that check FAIL at leg B (rc 2, one FOH phase instead of
  two); reverse-edited back byte-identical.
- **C9** (P2) — zero exit-code assertions for C1's boolean in
  `check-device-foh.sh`.
- **C10** (P2) — no launcher grep-assert in either candidate host.
- **C12** (P2) — `ART_SHA="$hsum"` survives at `check-device-foh.sh:1408` as
  the file's ONLY occurrence: assigned, never read. (The `raster.c:122` comment
  item reads as already corrected; the other two need their own look.)
- **C14** (P2) — `oracle/meleelight-harness.patch` does touch `main.js`, so
  cites read off the patched clone still shift. Citation-only.
- **C17** (P2) — `DEV_NEG`/`DEV_POS` still live in `normalize-foh-trace.js` and
  `check-judge-regression.sh`, unvalidated since the 3x injector cadence
  retired. Needs a device session.
- **C13** (P2) — the class is real and recurred THIS session: the manifest's
  UNCLOSED-ROWS prose said SEVEN while the gate printed six (fixed 2026-08-04
  by telling the prose to stop restating the count). Still zero enforcement.

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
  **A14 IS BLOCKED ON THE BROWSER IDENTITY PIN (measured 2026-08-05).**
  `bash port/gfx/check-render.sh` dies before any glyph work:
  `capture-canvas: browser identity pin violated — launched chromium
  151.0.7922.76, pinned chromium 151.0.7922.71` (`capture-canvas.js:153`,
  `.loop/a14-render1.log`). The installed Chrome auto-updated past the pinned
  engine, and every render bound in `expected-render.json` was measured on
  .71 — so NO re-capture of any kind can run here until that is settled, and
  widening the pin is exactly what the error forbids. A14 cannot proceed;
  neither can any other glyph/render re-freeze. **This is environment drift,
  not a code defect, and it was NOT introduced by A14.**
  **THE CHAR SET IS MEASURED AND READY TO APPLY** the moment the engine
  question is settled — one edit to `port/gfx/gfx-pagelib.js` GLYPH_FONTS,
  specs 0 and 3 only (spec 3 IS upstream's menu weight; specs 1/2 stay
  HUD-only), with `:` and ` ` moved OUT of the per-spec prefixes so no glyph
  is emitted twice:
  ```
  MENU_CHARS = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz !$&'()+,-./:>?[]"
  spec0.chars = "0123456789" + MENU_CHARS   spec3.chars = "0123456789" + MENU_CHARS
  ```
  69 + 10 = **79 glyphs per widened spec, zero duplicates** (verified), taking
  the artifact from 43 glyph records to 179. Derivation, in two parts because
  the halves have different domains: (a) every string literal reaching
  `foh_text`/`foh_text2`/`text_in` across `foh_render.c`, `foh.c`,
  `foh_pause.c` = 40 distinct chars; (b) what name entry can PRODUCE — the
  full alphabet, NOT the 22 lowercase letters that happen to occur in
  `randomTags`, because a grid that cannot spell `j`, `q`, `v` or `w` is
  broken by construction. Sample data sizes the symbols; the DOMAIN sizes the
  letters.

  **A14 SCOPE AMENDED 2026-08-04 (measured by the D8-later attempt): the
  planned `A-Z + .,!?&'` addition is NOT ENOUGH, and A14 is now a BLOCKER for
  D8-later.** Face coverage measured directly from `port/foh/foh_font.c`:
  face 1 has 49 glyphs (space, `!'()+,-./`, `0-9`, `:<>`, `A-Z`), face 2 has
  51 (same plus `&?`). **NEITHER FACE HAS A SINGLE LOWERCASE LETTER.**
  Upstream's own `randomTags` (main.js:142) span 54 distinct characters, so
  **25 are unrenderable today**: all 22 lowercase, plus `$`, `[`, `]`
  (`Panda`, `aMSa`, `Westballz`, `HungryBox`, `[A]rmada`, `Hax$`, `(.Y.)`).
  A tag widget therefore cannot draw upstream's own data, and `gfx_fatal`
  fires at the first one (reproduced: `SIM FATAL frame 0: foh_font: no glyph
  for requested character`, `.loop/d8i-flows.log`). The A14 browser re-capture
  must add **a-z and `$[]` as well as A-Z**, in the SAME capture — splitting it
  costs a second re-freeze of a device-consumed frozen artifact.
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
- B4 DONE (C18/C19/B4/A19 increment; re-verified by RUN 2026-08-23, lane M):
  the target-match exit lands on TARGET SELECT in-process, not the frontend
  (`foh_dev.c:2803`/`:2830` → `MEX_TSS` → `foh_phase:` re-entry →
  `FOH_TSS` at `:3587-3589`). Done-check `bash port/foh/check-mexit-reentry.sh`
  → `MEXIT REENTRY OK`; perturbing the quit back to `MEX_OS` makes it fail.
  The one part of the old row that SURVIVES is not a gap: upstream endGame's
  gameEnd/lost-stock/phantom/article resets are still unapplied, and the code
  says why it is unobservable (`foh_dev.c:2782-2789`) — the driver leaves the
  match on the next statement and a re-entry rebuilds the sim from scratch.
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
- B8 — DONE 2026-08-05. T6 in `check-device-foh.sh`: a variant flow inserts
  `I 377 D` before `SHOT 378 menu-top`, stepping menuSelected once, and the
  CANONICAL menu-top shot must differ from the twin (`cmp` rc exactly 1). The
  ledger note said 21 -> 22; the MEASURED count in that file is unconditional
  column-0 `teeth=$((teeth + 1))` sites, **15 -> 16** (21 was the fullgame
  ledger, a different file — read the count, never the row).
  **FOUND WHILE DOING IT — a live M4 blocker nobody had seen:** `verify_m4.sh`'s
  `FOH_RE` was STALE against its own producer. It demanded `shots=13` and had
  no slot for the `vsfinish=1` field, while `check-device-foh.sh:2328` prints
  `shots=15 ... vsfinish=1 ...`. A fully PASSING FOH leg could therefore never
  satisfy the gate — leg [2] would have failed on GRAMMAR after every arc
  closed. Proven mechanically before editing, then repaired: `shots=15`,
  `vsfinish=1`, `teeth=16`, each derived from the producer's source rather
  than from a run. This is the H4 class recurring on a SECOND consumer (the
  first was FULLGAME_RE); the note now sits at the regex.
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
- ~~A19 (P2) quit-to-menu is a process relaunch, not in-process FOH
  re-entry (`ponytail:` marked).~~ **STALE, same fact as B4 — corrected
  2026-08-23 (lane M).** The relaunch is GONE: quit-to-menu is
  `FOH_PAUSE_QUIT_MENU` → `MEX_TITLE` → the in-process `foh_phase:` re-entry,
  and `FOH_PAUSE_RC_MENU 70` plus the mlfk-foh.sh relaunch loop were deleted
  with it (`foh_pause.h:161-189`). Same evidence run as B4's. Restart still
  deliberately skipped — upstream has no restart-match semantics.
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
- C7 — DONE 2026-08-05. `mlfk-foh.sh`'s LIVE branch now derives a per-launch
  seed and RECORDS it (`mlfk-foh.sh: play seed=<n>`) BEFORE using it, which is
  the half the row insisted on: the LAUNCH line's grammar is pinned
  (`check-foh-flows.sh:332`) and carries no seed field, so an unrecorded random
  seed would have made a session unreproducible. Evidence paths keep their
  fixed seeds deliberately — `mlfk.sh:122` and every rig are untouched, because
  a fixed seed is what makes THEM reproducible. COST, paid honestly: this edits
  a `reviewed-go` pinned producer, so its manifest row is downgraded to
  `arc-in-flight` and joins the owed review round (gate now names 7, not 6).
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

  **WJ-later PRE-REGISTRATION, 2026-08-04 (the owner-ratified step) — AND IT
  RESOLVES TO "NO CODE". The row above states the scope BACKWARDS.** The
  faithfulness argument was written up front exactly as ordered, and it says
  the opposite of what the row assumes:
  - **MEASURED: `everyCharWallJump` is a DEAD TOGGLE UPSTREAM.** All five
    occurrences in `src/`: `gameplaymenu.js:53` (the `^= true` toggle),
    `gameplaymenu.js:239` (draws its own row), `css.js:89` + `css.js:109`
    (label + OFF/ON formatter), `settings.js:51` (default `0`). **Zero
    simulation reads.** The only walljump gate in physics is the PER-CHARACTER
    attribute — `player[i].charAttributes.walljump`, `physics.js:134` — which
    the setting never touches.
  - **MENU-SPEC §3 ALREADY CLOSED THIS on 2026-07-29** and says so in terms:
    "Implementing it faithfully means: add the row, persist 0/1, and wire it to
    nothing… **Do not invent a walljump rule to give it meaning; that would be
    a faithfulness violation.**" Its gap table row 4 records our port as
    "present, faithfully DEAD (row + persisted bit, zero MECHANICS consumers)"
    — **CLOSED**.
  - **So "the SIM IGNORES IT" is not the defect. It is the correct
    behaviour**, and the port already reproduces it. Making the setting reach
    the simulation would violate HARD RULE 5 (the browser original is ground
    truth) and would churn the checksum surface to add a rule upstream does
    not have.
  - **The only way this becomes work is as a KNOWING DEVIATION** — an owner
    request for a house rule, in the D18 mould (invented on purpose, registered
    as such), NOT as a port fix. If the owner wants it: the change is one
    disjunction at the `physics.c:775-777` equivalent
    (`canWallJump = FLAGS(S,i)->wallJumpAble || everyCharWallJump`), it is
    SAFE for every frozen golden because the default is `0` and all goldens
    were recorded at the settings defaults (`sim_boot.c` gameSettings note), so
    the flag OFF is bit-identical to today — but it must be registered as a
    deviation first, and the toggle must default OFF forever.
  - ~~DRIVER RECOMMENDATION: close WJ-later as ALREADY SATISFIED.~~
  - **OWNER OVERRODE THE RECOMMENDATION, 2026-08-04, knowingly.** Verbatim:
    *"i want real everyone-walljumps. deliberate deviation."* Presented with
    the measurement above — dead toggle upstream, giving it meaning is a
    departure not a fix — the owner chose the departure. Registered as
    **MENU-SPEC DEVIATION D20** (house rule). The faithfulness paragraph in
    MENU-SPEC §3.3 STAYS as written: it is still the correct description of
    upstream, and D20 is what overrides it, on owner authority. Nothing else
    in this port may cite this as precedent for inventing mechanics.
  - **IMPLEMENTED + VERIFIED 2026-08-04.** One conjunct at `physics.c:368`
    (the per-character ABILITY gate, mirror of `physics.js:134`) — NOT at the
    per-action-state `wallJumpAble` flag at `:777`, which would be a much
    larger and different rule. Plumbed through the three `G.sim` apply sites
    that already existed for turbo/lcancel/tapjump; `sim_host --walljump-all`
    exposes it to the harness. `bash port/sim/check-sim.sh` -> `SIM CONFORMS`,
    all 8 goldens exact — flag-off is bit-identical, as required.
  - **THE FLAG IS NON-VACUOUS, measured from the compiled CTAB1 table:**
    marth `walljump=0`, puff `walljump=0`, fox/falco/falcon `=1`. Upstream
    `attributes.js` simply has no `walljump` key for marth or puff, so the
    read is `undefined` -> falsy. D20 is what lets those two walljump.
  - **OUTSTANDING, and NOT claimed as done — the POSITIVE tooth.** All 6 human
    goldens replay BYTE-IDENTICAL with `--walljump-all` (measured), because
    none of them ever reaches the gate with marth or puff: the arm needs a
    prior wall collision to put `wallJumpTimer` in `[0,120)` AND the stick held
    into the wall. So the goldens cannot witness the new behaviour, and the
    change currently has a regression proof but no POSITIVE proof. Needed: a
    synthetic trace that drives marth or puff into a wall and holds in, showing
    WALLJUMP dispatches with the flag ON and not with it OFF.

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
  **SUPERSEDED IN PART, 2026-08-23 (see A25(b) below): the BINDING is built and
  correct, but the BUTTON does not arrive — A3's owner symptom is live and is
  the SAME defect as A25(b), one hop upstream of every line cited here. A3 is
  REOPENED as blocked on the physical-L keycode measurement, not closed.**
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

## OWNER PLAYTHROUGH #3 (Chase, 2026-08-22) — four new rows, A23-A26

Reported in session on the freshly provisioned play OPK. Each row below is
GROUNDED (file:line measured 2026-08-22, not recalled) and states its
mechanical consequence, because three of the four touch JUDGED surfaces and
will force re-freezes.

**Priority is NOT assigned here** — these arrive while Tier 1 (A14 second
half → D8-later → A7) is mid-flight and the M4 gate is still blocked on the
8 arc-in-flight rows. Owner slots them; the driver does not self-promote.

---

### A23 (P1) — CSS "BACK" wedge: hit-testable + hold-to-back with a red fill bar

**Symptom (owner):** "clicking back button does nothing in top right of the
vs. screen." Wanted: real-Melee behavior — hold it (either by holding the
hand cursor on it, OR by holding the B button) and a small **red bar fills
below the BACK button**; when it fills, it actually backs out.

**Ground truth, measured:**
- The wedge is **DRAWN AT `port/foh/foh_render.c:1367-1372`** — red arrowhead
  at (198,13)/(206,6)/(206,20), gold `"BACK"` text at (210,10).
- It is **already registered as a known gap**: `foh_render.c:222` reads
  *"(a) DRAWN, NOT YET HIT-TESTABLE: the header's BACK wedge and mode
  ribbon"*. This row closes that note — it is not a new discovery.
- The **hold machinery already exists**: `FohState.bHold`
  (`foh.h:438`, "consecutive B frames in CSS (30 = back)"), counted in
  `step_css` at `foh.c:466-477`, firing `css_back()` (`foh.c:431`) on the
  `bHold == 30` **equality** edge. The equality (not `>=`) is upstream
  css.js:188 verbatim and is what makes it fire exactly once per hold —
  `foh.c:453-454` says so explicitly. **Do not convert it to `>=`.**

**So the work is two additions, not a rewrite:**
1. **Hit-test + cursor-hold arm.** Make the drawn wedge a hit region for the
   CSS hand and drive the SAME `bHold` counter when the hand is inside it
   with A held. One counter, two input paths — never a second timer.
2. **The red fill bar.** New draw under the wedge, width proportional to
   `bHold / 30`. Purely presentational, reads the counter, writes nothing.

**FAITHFULNESS — RESOLVE BEFORE CODING (HARD RULE 5).** Upstream meleelight
css.js has the 30-frame `bHold` but **it must be measured whether it draws a
progress bar at all.** Two outcomes, two different rows:
- upstream DOES draw one → this is a fidelity FIX, no deviation needed.
- upstream does NOT → the bar is an **owner-requested DEVIATION toward real
  Melee**, and it gets a D-number and a registered entry the same way D20
  (everyone-walljumps house rule) did. The owner's words — *"behave like how
  melee does"* — are the ratification; record them at the site.
  Measure first via the browser harness against the upstream clone; do not
  guess from memory.

**Mechanical consequence:** CSS is judged by frozen shots and by
`judge-foh-trace.js`. A new bar drawn on the CSS header changes **every CSS
shot** (f01/css, f02/css-cpu, f05/css — the same three B11 makes flaky, so
sequence this against B11 or expect noisy legs). The bar must be COLD in
judged shots (`foh_look_canonical` pins timers to 0 for exactly this reason —
see the `tssTimer` precedent at `foh_render.c:2040-2043`): with `bHold == 0`
the bar is zero-width, so a cold shot is unchanged **only if the bar draws
nothing at zero**. Make that the design, and it costs no re-freeze at all.

`done-check:` a flow leg that holds the cursor on the wedge and backs out,
plus one that holds B, both landing the same `bhold` transition already
emitted by `css_back()`; `DEVICE FOH OK` rc 0; a tooth proving a 29-frame
hold does NOT back out.

---

### A24 (P2) — Controls menu: wrong label, wrong name, wrong order

**Symptom (owner):** the options row says "Keyboard Controls" but opens a
submenu offering "Controller" and "Keyboard" — so the row should just say
**Controls**. Inside, the thing called "Keyboard" **is not a keyboard, it is
the FunKey-S's own buttons**, so it is misnamed. And **Controller is listed
first when we cannot even use a controller on FunKey-S yet** — the FunKey
entry should be first.

**Ground truth, measured — every site that has to move together:**
| Site | Current |
|---|---|
| `foh_render.c:39` | options row label `"KEYBOARD CONTROLS"` |
| `foh_render.c:41` | submenu labels `{"CONTROLLER", "KEYBOARD"}` |
| `foh_render.c:79` | blurbs `"CUSTOMIZE & CALIBRATE CONTROLLER."` / `"CUSTOMIZE KEYBOARD CONTROLS."` |
| `foh_render.c:1125` | width comment pinned to the LONGEST blurb (33 chars) — recompute if blurbs change |
| `foh_render.c:1902` | screen header `"CONTROLLER"` |
| `foh_render.c:1936` | screen header `"KEYBOARD"` |
| `foh.h:22,25,26` | routing comments naming the upstream gameModes |
| `foh.c:236,265` | the same labels restated in comments |
| `port/foh/foh_ctl_labels.h` | check whether it restates any of these |

**Screens behind them:** `FOH_CTRL_PAD` = upstream gameMode 14
(controllermenu.js), `FOH_CTRL_KEY` = gameMode 12 (keyboardmenu.js) —
`foh.h:339-341`. **Renaming is display-only; DO NOT renumber the enum or the
gameModes.** The upstream identity is what the judge grammar and the §9.2/§9.3
notes key on.

**REORDERING IS NOT DISPLAY-ONLY — this is the trap.** The submenu is
index-selected. Swapping the two rows swaps which index routes to which
gameMode, and there is already a live consumer: `foh.h:495` C30(c) — *"Controls
>Keyboard screen's cursor over its TWO settable..."*. Reordering without
re-measuring is exactly the iter-73 stale-nav class that A13 was cleaning up.
**Move the labels and the routing in ONE commit, and re-measure every
selection index that names row 0/1 on this screen.**

**Faithfulness:** upstream menu.js:22-23 is literally `["Controller",
"Keyboard"]`. Renaming and reordering is a **registered deviation** — owner
requested, with the reason on the record (there is no controller path on this
device yet). Register it; do not silently diverge.

**Naming decision needed from owner (do not pick unilaterally):** "FunKey",
"FUNKEY-S", "HANDHELD", or "BUTTONS" for the renamed entry. Driver
recommendation: **"FUNKEY"** — shortest, matches the physical device, and fits
the existing blurb width pin at `foh_render.c:1125`.

**Mechanical consequence:** re-freeze the options + controls shots. If the
renamed string is longer than the 33-char blurb pin, the width comment at
`foh_render.c:1125` and the panel geometry both move.

`done-check:` `DEVICE FOH OK` rc 0 with re-frozen shots; a tooth proving row 0
now routes to the FunKey screen and row 1 to the controller screen.

**DONE 2026-08-23 (lane m-a24) — renames only; the COLLAPSE stays held.**
Shipped as **DEVIATION D25** (MENU-SPEC §9.1 + §12.1). Name chosen:
**`HANDHELD`**, not the driver's `FUNKEY` recommendation — it is the RATIFIED
one (the 2026-08-23 hold note above keeps `CONTROLS` / `HANDHELD` /
`BOX`/`CLASSIC`/`NATURAL` ratified while suspending the collapse), and it reads
as a CATEGORY parallel to `CONTROLLER` rather than as a brand.

Every site in the table above moved, plus two the table did not name — both
found by grep, both live:
- **`port/foh/foh_snd_witness.c:170-173`** drives the chooser BY ROW INDEX
  (`controls-A-pad` sel 0, `controls-A-keyboard` sel 1). Swapping the routing
  without it makes that witness assert the old wiring. Rows swapped and renamed.
- **`port/foh/flows/f04-nav.{flow,expect}`** enters row 0 then row 1, so the
  two Controls SHOT markers and four frozen transition lines swap frames. The
  shot NAMES are unchanged, so `FLOW_SHOTS` (check-foh-flows.sh:288,
  check-device-foh.sh:486) is a set that still matches and needed no edit.

Sites the table asked about, answered: `foh_ctl_labels.h` restates NONE of
these strings (it is the button->action table, keyed on `CtlStyle`), so it did
not move. `foh.h:495` is `optRow`/`optCol`, not a chooser index; the C30(c)
cursor is `FohState.ctlRow` (foh.h:542) and is the ctl SCREEN's own two-row
cursor, unrelated to `menuSelected` on the chooser page — measured, not assumed.

**No re-freeze was needed.** The width pin did not move: the new blurb
`"CUSTOMIZE THE HANDHELD CONTROLS."` is 32 glyphs and
`"CUSTOMIZE & CALIBRATE CONTROLLER."` is still the widest at exactly the pinned
230 px — and that is now MEASURED by the witness (widest-of-set over all 14
blurbs), not counted by hand. Shots are judged pairwise (run A vs run B, host
vs device) and by name inventory, never against frozen images, so no shot was
re-frozen.

**A4's half was already shipped** as C31 (`ctl_style_name` returns `"Classic"`;
`CTL_STYLE_NORMAL` is still enum 0 because `FohPersist.ctlStyle` stores it
verbatim). It had no tooth — only the frozen keyboard screenshot, which
exercises the fresh-install NATURAL. It has one now.

`done-check:` **`bash port/foh/check-controls-labels.sh` -> `CONTROLS LABELS
OK`, exit 0** (host-only). The witness reads the ROW LABEL off the frame, presses
A on that row through the real `foh_tick`, and reads the destination's HEADER
off the frame the machine lands on — so a label and its destination cannot
drift apart. Four negative tests, each required to fail AND to fail only where
its part is: T1 the old chooser labels + old header, **T2 the routing ternary
reverted ALONE with the labels left in place** (the half-swap trap: without it
this check would go green on a build whose first row lies about where it goes),
T3 the old Options row name, T4 `ctl_style_name` back to `"Normal"`.
Also green after the change: `bash port/foh/check-foh-flows.sh`.
**REMAINING (device, not run in this lane):** `DEVICE FOH OK`.

---

### A25 (P1) — Target select: invisible highlight, dead L, and the free-cursor rewrite

Three complaints, **three genuinely different root causes.** Split them; do
not fix them as one.

#### (a) The selection highlight is not visible — measured, and it is real
`foh_render.c:2056` draws the selected tile as a **1-pixel border**:
`hot` = pink `{251,116,155}` / `{255,182,204}` vs `idle` = grey
`{166,166,166}`, around a 100x19 black body. Same at `:2069` for "+ ADD CODE".
One pixel of pink-vs-grey at 240x240 is the whole selection signal. The owner
is right that it does not read.

Cheapest honest fix: **thicken and/or fill**, not a new mechanism — e.g. a
2px border plus a subtly lifted body for the selected tile. Note
`foh_render.c:2049-2050` records that **upstream never brightens a label on
hover, only its border** — so brightening the LABEL is a deviation, while
making the BORDER louder is within the existing upstream idiom. Prefer the
border.

#### (b) L does nothing, R works — this is a DIAGNOSIS row, not an implementation row
**The L arm is already implemented and looks correct**: `foh.c:779-786`,
`in->l` → `p1Char - 1` with wrap, `in->r` → `p1Char + 1` with wrap, both
verbatim from targetselect.js:60-74. The chevrons at `foh_render.c:2074-2077`
deliberately point along the shoulder axis "which is what the L/R caps say" —
so the UI promise is real.
**And L appears bound**: `port/foh/keymap-frozen.txt` has `map l K k`, and
`platform_sdl1.c:117-135` consumes that keymap table as the single source of
truth for every field.
**ISOLATED 2026-08-23 (lane I / A25b). THE FAULT IS THE FIRST HOP ONLY:
physical L button → Linux keycode. Everything downstream of the keycode is
PROVEN GREEN, ON THE DEVICE.** Measured, not inferred:

- **keysym `k` → `in->l` → `p1Char - 1` is a COMMITTED, FROZEN, ON-DEVICE
  ASSERTION and it passes.** `port/foh/flows/f07-target-t02.flow:24` is the
  single line `I 400 K`; `port/foh/flows/f07-target-t02.expect:7` is the
  frozen `S 400 p1char 4` (marth 0 wrapping to falcon 4 — the wrap witness
  the flow header names). `port/sim/target/check-device-target.sh:347`
  drives f06/f07 ON the FunKey-S in leg `[6/8] device legs: fk_input ->
  uinput -> SDL keysyms -> platform_poll` (`:981`) with `--input poll`
  argv-pinned (`:1008-1010`), and normalizes the DEVICE trace against that
  frozen expect. `port/tools/fk_input.c:48-52` injects the FLOW1 letter as
  the QWERTY `KEY_*` code, so the assertion is literally *Linux `KEY_K` →
  SDL → `PlatformInput.l` → `p1Char - 1`, on the real hardware.*
- Re-proven host-side this session by running the real functions (not by
  reading them): the `platform_sdl1.c:117-135` translation arm over a
  synthetic `SDL_GetKeyState` array gives `'k' → l=1` and nothing else
  set; `foh_tick` on `FOH_TSS` then steps p1Char 2→1 on the L edge, 1→2 on
  the R edge, 0→4 on the L wrap, and does nothing on a held L (edge
  semantics correct). So leads (2) and (3) of the previous list are DEAD:
  no earlier arm consumes L, and `pv` is `s->prev` (`foh.c:1089`,
  latched at `:1136`) and is not clobbered.
- **WHY NO CHECK HAS EVER COVERED THE BROKEN HOP.** Every device check
  injects at `/dev/uinput` through *our own* device — `check-device-input.sh`
  states it outright: *"our OWN uinput device — writing the existing event
  device does not inject"*. The FunKey's physical buttons instead come from
  **`fkgpiod`** (`/etc/init.d/S11gpio`), which the rig's quiesce bracket
  STOPS for the duration of a device run (`riglib.sh` `rig_qd_normalize`,
  `check-skip-attrib.sh:165`). So the whole rig is structurally blind to
  physical-button → keycode, by construction, for every button.
- **AND THE `map l K k` ROW WAS NEVER MEASURED ON THIS DEVICE.** Its
  provenance is donor archaeology: `docs/research/funkey-envelope.md:77`
  attributes the letter-keysym set to ssb64's `gfx_present_sdl1.c` ~147-190.
  Of those letters, `u`,`d`,`l`,`r`,`a`,`b`,`s`,`q` are confirmed by owner
  play and the OS Fn+Up chord; `n` (R) is confirmed by this very bug report
  (R works). `k` is the one letter with no independent confirmation, and it
  is exactly the one that does not work.
- `keymap-frozen.txt` is a FROZEN artifact and has NOT been touched. If the
  device measurement below shows physical L emitting something other than
  `KEY_K`, changing that row is a reviewed change with a paired re-measure
  (`--dump-keymap` cmp, the f07 fks derivation, and both `NAV_LINK`s are
  unaffected, but the keymap-swap teeth at `check-device-foh.sh:695`/`:721`
  and the `T-devswap` device tooth at `:51` are).

**WHAT STILL NEEDS THE DEVICE — and it needs NO new code.** The question is
purely kernel-level (if physical L emits `KEY_K`, f07 proves the app WILL see
it; if it does not, that is the bug), so busybox answers it:
`cat /proc/bus/input/devices` to find the fkgpiod keyboard node, then read
`/dev/input/eventN` while pressing L and then R and compare. Three outcomes:
(i) L emits a keycode other than `KEY_K` → the frozen keymap row is wrong;
(ii) L emits nothing at all → fkgpiod config or the physical switch;
(iii) L emits `KEY_K` → reopen, because f07 says that case works.

**Zoom-out (HARD RULE 8) — SETTLED: A3 AND A25(b) ARE THE SAME DEFECT.**
A3's code half is genuinely built and correct — `ctl_roles`
(`s1_input.h:165-183`) gives NORMAL and NATURAL `*shield = (p->l || p->r)`,
and NATURAL is the default — so in the default style L is a real shield
button reading the same `PlatformInput.l` field TSS reads. The 2026-08-03
note above closed A3 as a *stale row* on the strength of that read; that was
half right. The BINDING is not missing; the BUTTON is. A3's original owner
symptom ("L does nothing") and A25(b)'s ("L does nothing") are one fault at
one hop, and R masks it in the match (R already shields) which is why only
TSS — where L and R do *different* things — made it visible.
**Consequences:** (1) do NOT write a TSS-local workaround; (2) the fix, when
the measurement lands, is ONE line at the input layer and it widens the blast
radius to the MATCH as well as the menus — L would start shielding /
air-dodging the moment it starts arriving, which is the intended A3
behaviour but is a live gameplay change, not a menu change; (3) A3 must be
reopened as blocked-on-this-measurement, not left closed as stale.

#### (c) SPEC — free hand cursor on TSS, shared with CSS (owner: "keep things DRY")

**What exists today (measured):**
- **CSS: a real free 2D cursor.** `foh.h:90-94` — *"FREE 2D hand cursor ...
  integrated from d-pad every frame"*; `foh.h:374` *"free hand cursor
  (css.js:64 handPos, DOUBLES, never integers)"*; speed knobs
  `FOH_CURSOR_SPEED` / `FOH_CURSOR_VX` / `FOH_CURSOR_VY` at `foh.h:297-299`
  (already a registered deviation, D3); hand type via
  `foh_css_hand_type()` (`foh.c:374`); hit-testing via `css_cell_at()`
  (`foh.c:393`) and the panel/rail helpers around it.
- **TSS: an index cursor.** `tssCursor` (`foh.h:445`), 0..9 grid + slot 10,
  stepped by d-pad edges at `foh.c:809-828`.
- **SSS: also an index cursor** (`sssCursor`, `foh.h:440`) — and note
  `foh.h:74` records that SSS's *"pointer drag"* was ALREADY rewritten INTO a
  3x2 grid cursor as a deliberate delta.

**That last fact is the most important input to this spec.** The grid cursors
are not an oversight; they are a ratified rewrite of upstream's mouse for a
device with no pointer. **Putting a free cursor back on TSS reverses that
decision for one screen.** The owner has asked for it, so it proceeds — but it
is a **registered deviation needing a D-number**, and the driver should say
plainly: TSS will now differ from SSS in interaction model, so either accept
the inconsistency or plan SSS to follow.

**The DRY extraction — the actual spec:**
1. **Extract, do not duplicate.** Lift the CSS hand into a screen-agnostic
   unit, e.g. `port/foh/foh_hand.{c,h}`: state `{double x, y; int type;}`,
   a step that integrates d-pad input using the existing `FOH_CURSOR_VX/VY`
   knobs, clamping to a caller-supplied bounds rect.
2. **Hit-testing is caller-supplied, not baked in.** The shared unit answers
   "which of these N rects contains the hand" over a table the screen owns.
   CSS's cell/gutter rule (`css_cell_at`: strict `>` / `<`, so the 2px gutter
   is genuinely no cell — D4 forbids a hit region where nothing is drawn)
   becomes the shared rule; TSS inherits it and so gets gutter behavior for
   free.
3. **CSS MUST COME OUT BYTE-IDENTICAL.** This is the hard constraint. CSS is
   covered by frozen shots and by `judge-foh-trace.js` structural traces. The
   extraction is a **pure refactor on the CSS side**: same doubles, same
   integration order, same clamp, same hit predicate. Prove it with the
   EXISTING checks unchanged and green before a single TSS line is written.
   This is the A14-second-half lesson restated — *swap the implementation, not
   the call sites*.
4. **Then, and only then, adopt it on TSS.** Keep the 11 slots' rects as the
   hit table. **Decide and record what happens to d-pad-to-index**: dropping it
   changes the TSS flow scripts; keeping both is two cursors on one screen and
   is worse. Driver recommendation: **replace it**, matching CSS, and re-cut
   the TSS flow legs.
5. **Sequence against A25(a).** The highlight fix and the cursor rewrite both
   re-freeze every TSS shot. **Land them in that order in one arc** so the
   shots are re-frozen ONCE.

**Mechanical consequence:** TSS shots and any TSS flow leg re-freeze; the TSS
transition grammar in `judge-foh-trace.js` gains hand-move edges (CSS's
precedent shows what they look like). `foh_look_canonical`'s cold-shot pinning
must cover the new hand exactly as it pins `tssTimer`.

`done-check:` all three parts green — a tooth proving the selected tile is
distinguishable by the shot judge; L changing the character on device; CSS
checks byte-identical across the extraction; TSS reachable by hand cursor with
re-frozen shots.

---

### A26 (P2) — Hibernate/resume: reopening the FunKey-S should resume the game

**Symptom (owner):** hibernating the FunKey-S (closing it) should resume the
game when opened again.

**Ground truth, measured 2026-08-22:**
- **There is NO suspend/resume handling anywhere in the port.** Grep over
  `port/gfx/*.c` and `port/foh/*.c` for `SIGTERM|SIGSTOP|SIGCONT|SIGUSR|
  signal(|sigaction|suspend|resume|hibernat` returns only two unrelated
  comment hits in `foh_pause.c` (:296, :630).
- **There is no donor to copy.** The ssb64-funkey-s port (the donor CLAUDE.md
  names for A11/A12's overlay) has no suspend/resume either — its only
  `resume` hits are coroutine scheduling.
- **What DOES exist is adjacent and reusable:** `port/foh/foh_persist.{c,h}`
  is already *"the ONE persistence chokepoint"* — a versioned, checksummed,
  deterministic 64-line `MLFKPERSIST4` file on SD, currently holding the 11
  FOH-editable gameSettings plus target records. It is the right home for any
  saved state, and its format discipline (versioned + checksummed + host/device
  `cmp`'d) is the bar a resume file must also meet.

**FIRST STEP IS MEASUREMENT, NOT CODE.** What "hibernate" physically does on
FunKey OS decides the entire shape of this row, and it is currently UNKNOWN
(I tried to inspect `/sys/power/` on device but it dropped off USB first).
Three possible worlds:
1. **Kernel suspend-to-RAM, process untouched** → possibly nothing to do but
   handle the clock jump and resync audio; the game may already survive it.
   **Test this before building anything.**
2. **The app is signalled then killed** → catch the signal, write a resume
   file through `foh_persist`, restore on next boot.
3. **The frontend kills the app outright with no signal** → only a periodic
   checkpoint can work, and the row needs a scope decision (checkpoint what,
   how often, at what cost to the 16.67 ms budget).

**Measure world 1 first** — plug the device in, launch, close the lid, reopen,
and observe. That single experiment may collapse this row to near-zero, and it
costs minutes.

**Scope question for the owner, once the world is known:** resume to the
**menu** (cheap, and probably all that world 2/3 justifies) or resume
**mid-match** (expensive — the sim state is the whole `MlPlayer`/physics plane,
and a mid-match snapshot is a new serialization surface with its own
correctness bar). Driver recommendation: **menu-level resume first**; treat
mid-match resume as a separate, later row.

**Mechanical consequence:** if it becomes a persist-format change, that is a
`MLFKPERSIST5` bump with the twin host/device `cmp` re-run — the header's own
v2/v3/v4 history documents the ritual. **Nothing here may run in the frame
loop** (PLAN §7 / the SD-streaming stall note in CLAUDE.md: SD writes are
multi-second; log to tmpfs, copy on exit).

`done-check:` *(REPLAN — cannot be written until the measurement above says
which world we are in; a placeholder check would violate the CHECKER's
no-placeholder rule.)*

## OWNER PLAYTHROUGH #3, ROUND 2 (Chase, 2026-08-23) — A27-A34

Eight more rows from continued play. Same discipline as A23-A26: grounded at
file:line measured today. **Two of these are outright BUGS in shipped
behavior (A28, A29) and one is a dead control (A34)** — they outrank the
cosmetic rows in the menus lane.

---

### A27 (P1) — CSS mode ribbon: no way to change game mode

**Symptom (owner):** *"there's no way to change between stock mode and
'endless ko fest'... if you click the 'VS Melee' in the CSS it should change
modes."*

**Ground truth:** the ribbon is drawn at `foh_render.c:1358-1364` (`"VS. MELEE"`
inside a 6-point wedge). **It is the SAME registered gap as A23** —
`foh_render.c:222` names both in one breath: *"(a) DRAWN, NOT YET
HIT-TESTABLE: the header's BACK wedge **and mode ribbon**"*.

**Therefore A23 and A27 are ONE piece of work, not two.** Both need the same
thing: CSS header widgets become hit-testable by the hand. Do them in one
change or the second one re-does the first one's plumbing. **Merge A27 into
A23's arc.**

Open question to settle from upstream before coding: what the mode set
actually IS (stock / endless / time?) and whether upstream cycles on click or
opens a picker. Read `menu.js` / `css.js` in the clone — do not invent a mode
list.

---

### A28 (P0, BUG) — constant audio buzz from launch

**Symptom (owner):** *"when I launch the game there's a buzzing sound coming
from the speakers (before music even plays) and the whole game / time."*
Present BEFORE any sound plays and continuously — so this is the idle path,
not a mixing bug.

**Ground truth:** the device opens audio at **44100 Hz / AUDIO_S16LSB / 2ch**
with an exact-spec-or-fail check (`port/gfx/platform_audio_sdl.h:120-150`);
the callback delegates the whole buffer to `g_pa_fill`
(`platform_audio_sdl.h:102-103`) and only memsets to silence on the
`len != granted.size` error path (`:97-101`).

**Ranked hypotheses — test in this order:**
1. **The fill callback does not write silence when zero voices are active**,
   so the driver replays whatever was last in the buffer. This is the classic
   cause of exactly this symptom (constant, present before any sound) and it
   is the FIRST thing to check: does `g_pa_fill` unconditionally initialise
   every sample frame, or does it only add active voices onto an
   uninitialised buffer?
2. **Sample-rate mismatch.** The M1 pipeline converts ALL audio to
   **22050 Hz** (CLAUDE.md M1 task 4: SFX mono 22050, music stereo 22050) but
   the device opens at **44100**. Confirm the mixer resamples; if it does not,
   expect pitch/aliasing artefacts.
3. DC offset in the converted PCM, or a partially-filled final block.

**Note this is NOT contradicted by the M3 evidence:** `verify_m3.sh` asserts
`underruns == 0`, which measures callback *timing*, not sample *content*. A
buffer full of buzz underruns exactly zero times. The existing bar cannot see
this defect — which is itself worth recording.

`done-check:` audio silent at idle on device, by ear AND by a captured buffer
assertion that the idle callback output is all-zero.

---

### A29 (P0, BUG) — CSS character selection is lost on re-entry

**Symptom (owner):** select falco (P1) + marth (P2), start, quit to menu,
return to VS — **P1 shows marth, P2 shows puff.**

**Ground truth, and it names the bug:** marth is char id **0**, puff is char
id **1**. So the observed state is exactly `cssChar[k] = k` — the identity
fill. `FohState.cssChar` is `int cssChar[2]` (`foh.h:429`). Something on the
match-exit path re-initialises the CSS plane to an index-identity default
instead of preserving the player's picks (or instead of restoring the
upstream defaults, which are p1=fox(2)/p2=marth(0) per the oracle's own
documented defaults — note the observed values match NEITHER the picks NOR
those defaults, which is the strongest clue available).

**Start here:** the match→menu re-entry path, and whoever writes `cssChar`
outside `step_css`. `check-mexit-reentry.sh` exists and is the natural home
for the regression tooth.

`done-check:` pick a non-default pair, launch, quit to menu, re-enter CSS,
assert both slots still read the picks; tooth proving an identity-fill
regression fails.

---

### A30 (P1) — control-style DEFAULT button maps are wrong

**Symptom (owner), per style:**
1. **box** — "L should be shield and R should be mod / tilt"; and
   X→grab, A→jump, Y→special, B→attack.
2. **classic** — X→grab, A→jump, Y→special, B→attack, R C-stick (HOLD).
3. **natural** — X→grab, A→jump, Y→special, B→attack, R C-stick (HOLD).

**Ground truth — the current tables:**
- Styles are `CTL_STYLE_NORMAL=0`, `CTL_STYLE_BOX=1`, `CTL_STYLE_NATURAL=2`,
  default NATURAL (`port/gfx/ctl_style.h:93-99`).
- Labels: `foh_ctl_labels.h` — A=ATTACK, B=SPECIAL for every style; X/Y are
  style-dependent (C-layer styles spend X on JUMP, Y on C-STICK HOLD; NATURAL
  spends X on GRAB(Z), Y on JUMP); shoulders shield except BOX's Mod.

**SPLIT THIS ROW — the two halves have wildly different costs.**

**(a) The shoulder request is nearly free and is ALREADY a supported
setting.** `ctl_style.h:69-77` records the 2026-07-29 owner ruling that the
Mod shoulder is a separate, orthogonal cell, and that swapping is a pure
RELABELING which leaves the ratified BOX table untouched (proved by dumping
all 2048 combos under both arrangements). **So (a) is a DEFAULT FLIP of
`modOnR`, not a table edit.**

**(b) Remapping the face buttons is EXPENSIVE and touches ratified ground.**
A→jump / B→attack / Y→special inverts the A=ATTACK, B=SPECIAL assignment that
every style currently shares. For **BOX specifically** this edits the
"Chase-ratified S1 One-Mod + C-layer table, **byte-for-byte**" (`ctl_style.h:
27-31`, B0XX/HayBox lineage) — which is pinned by `port/gfx/s1_sweep.c` and
`check-device-input.sh` across 15 S1 checks. **That is a re-ratification, not
a bug fix.** Do not touch it without the owner signing for it in writing, the
same way D14/D20 were signed.

**ANSWER THE OWNER'S PARENTHETICAL FIRST — it may shrink this row a lot.**
*"(unless there is some other way to grab I don't know about)"*: in Melee,
**shield + A is a grab**, which is why the C-layer styles can afford to spend
X on JUMP. Confirm that arm is live in this port and TELL THE OWNER before
remapping anything — if grab is already reachable from shield+A, the
motivation for X→grab on BOX/CLASSIC largely evaporates.

**And note A31 may subsume most of (b):** if arbitrary rebinding lands, the
defaults are just a starting table the player can change, and the argument
for editing the ratified BOX defaults gets much weaker.

---

### A31 DONE 2026-08-23 (DEVIATION D26) — Controls screen: real rebinding, and three things that should not be there

> **DONE, lane M, one commit on `lane/m-a31`.** All four sub-items shipped.
> Task check: `bash port/foh/check-rebind.sh` -> `REBIND TOOTH OK`, exit 0
> (host-only; 5 orthogonal negatives). `check-controls-labels.sh` stays
> green with two pins re-synced (the `yRow[2]` line it pinned no longer
> exists — the screen has eleven rows now — so the pins follow the lines
> that carry the same coordinates; T1-T4 untouched). Ledger row: MENU-SPEC
> §9.3 D26, which also records which parts of D13's sketch this
> DELIBERATELY does not build (listening mode, hold-A clear, protected
> primaries) and why a permutation makes each unnecessary.
>
> **What shipped, against the four sub-items:**
> 1. **All nine action rows are selectable and bindable.** `FohState.ctlRow`
>    covers ELEVEN rows (`foh.h`: 9 action rows + style + reset). L/R on a
>    button row SWAPS its action with the button that held the next one, so
>    the table is always a permutation. Row 0 (d-pad) is selectable and
>    refuses with `deny` — it drives the control stick, not a button.
> 2. **The `mod` row is gone from the screen.** The CELL survives
>    (`ctl_style.c`, the BOX label table, the persisted record) exactly as
>    the ticket said. Swapping the shoulders is now a plain rebind.
> 3. **`rebind: N/A` answered, then deleted.** It was NOT vestigial: it was
>    the screen saying out loud that D13's rebinder did not exist. The
>    caption now reads `L/R: CHANGE   A: RESET`.
> 4. **`RESET TO DEFAULTS` is row 10** — identity binding + default style +
>    ratified Mod arrangement in one A press.
>
> **The mechanism, in one line:** `ctl_bind_apply()` permutes the polled
> `PlatformInput` at `foh_dev.c`'s new `poll_bound()`, BEFORE the pause
> edge, the system-menu edge, the S1 chord resolver or the raw-key sidecar
> read it — so `s1_input.h`, the three chord tables and every frozen S1
> sweep never see the feature, and the fresh-install identity binding makes
> it a struct copy. The FOH MENU loop keeps the raw `platform_poll` on
> purpose (a player who moved A elsewhere must still reach this screen).
>
> **Per-port: carried in the model AND the format, exposed in the UI for
> port 0 only** — the A33 re-amendment's instruction exactly.
> `MLFKPERSIST5` gains four `bind` rows (one per port, each validated as a
> permutation), so a second controller would be a UI change and not another
> format bump.
>
> **TWO BUGS FOUND AND FIXED WHILE WIRING IT** (both pre-existing, both the
> same "the feature does not survive" class the rebinder would have joined):
> * `foh_dev.c`'s persist arm named ONLY `options-gameplay`, so on the
>   PRODUCT binary a Controls or audio change reached SD only if the player
>   later B-exited an unrelated screen. `foh_app.c` already carried the
>   three-screen form and says so in its own note; the product path never
>   got it. Both drivers now agree.
> * `foh_persist.c`'s block gates named their versions by ENUMERATION
>   (`fromVer == 0 || fromVer == 3`), so the v5 bump silently dropped the
>   `modonr` line for a v4 file and rejected a perfectly good save as
>   corrupt. MEASURED by the new check's v4-migration leg. Every gate is now
>   a `>=` on one effective-version number, which is total over any future
>   bump.
>
> **Device leg outstanding (driver):** `port/foh/check-device-persist.sh`
> was updated for v5 — `PERSIST_BYTES` 1510 -> 1602, 64 -> 68 lines, the
> positional whitelist gained the four `bind` rows with a permutation
> check, the unsupported-version tooth moved v5 -> v6, and the shared
> post-migration expectation gained `v5_defaults`. The FunKey was
> disconnected, so it was NOT RUN. Its `verify_persist_file` was extracted
> and run standalone against a real v5 record (pass) and against four
> perturbed copies (duplicate slot / wrong port order / dropped row / v4
> header — all four rejected), so the format-sensitive half is verified;
> the device legs are not.

### A31 (P1) — Controls screen: real rebinding, and three things that should not be there

**Symptom (owner):** *"you should be able to rebind any of the 'active
mappings'. currently you can't even go to any of those rows — only change
between 'style:' and 'mod'. changing 'mod' changes the controls. we don't
want that. get rid of 'mod' altogether as an option here... what is
'rebind: N/A', why do we even have that section? also, can we have a 'reset
to defaults' button. also, how do we set the controls for different players?"*

**Ground truth:** `foh.h:495` (C30(c)) says the cursor covers exactly **TWO
settable rows** — that is the measured cause of "you can't go to any of those
rows". The nine action labels come from `foh_ctl_labels.h`
(`FOH_CTL_LABEL_ROWS 9`) and are **display-only**: there is no per-action
binding cell anywhere.

**Four sub-items, in dependency order:**
1. **Make the 9 action rows selectable and bindable.** This is the real work:
   a per-action binding table + a listening mode. `foh.h:1023` already
   references a listening-mode design ("hold-A clear, protected primaries") —
   read it before designing a new one.
2. **Delete the `mod` row from this screen.** Owner is explicit. Keep the
   underlying `modOnR` cell (A30(a) needs it, and the BOX table's relabeling
   proof depends on it) — this is a UI removal, not a model removal.
3. **`rebind: N/A`** — establish what it was for and delete it if it is
   vestigial. Owner is right to ask; a row that always reads N/A is dead UI.
4. **"Reset to defaults" button.** Cheap and clearly correct once (1) exists.

**Per-player controls (the owner's last question) is a REAL design question
and should NOT be answered casually.** Today the style/mod cells are
**process-wide** (`ctl_style.h:101`: *"Process-wide active style"*), and
persistence stores ONE `ctlstyle` line. Per-player bindings means a per-port
plane in the model, the UI and `MLFKPERSIST` — a format bump. **It only pays
for itself if A33 lands** (a second physical controller). Recommendation:
**design the binding table per-port from the start** (cheap now, expensive to
retrofit) but **ship the UI editing port 0 only** until A33 says whether more
ports are real.

**Mechanical consequence:** persistence gains a bindings plane →
`MLFKPERSIST5`, with the twin host/device `cmp` ritual its header documents.

---

### A32 (P2, BUG?) — L-cancel default should be ON for all players

**Symptom (owner):** *"defaults for L-cancel should be on for all players
(i see off for p2, p3, p4 for some reason?)"*

**Ground truth, and it raises a sharper question than the row does:**
`lCancelType` is a **SINGLE GLOBAL SCALAR** — `sim_boot.c:433`
(`g->sim.lCancelType = 0`), persisted as one value (`foh_persist.c:95,276`).
There is no per-player L-cancel plane at all. The per-player thing that DOES
exist is `tapJumpOff[4]`.

**So before changing any default, resolve what the owner actually saw:**
either (i) the options screen renders per-player rows for a global setting —
in which case **the display is the bug**, and changing defaults would paper
over it; or (ii) he is reading the `tapJumpOff` rows, which are genuinely
per-player and genuinely default differently for P1 (CLAUDE.md's
`--tapjump-off-p1`). **Measure which before touching a default.**

**Faithfulness note:** `lCancelType 0` is the authored UPSTREAM default
(`sim_boot.c:19`, settings.js:44-56 all-zero). Turning it on by default is a
**deviation** needing a D-number, not a bug fix — flag that to the owner.

---

### A33 (P2, RESEARCH SPIKE) — GameCube adapter over the FunKey-S micro-USB

**Owner-provided sources:** `github.com/secretkeysio/gcadapterdriver` ·
`github.com/ToadKing/wii-u-gc-adapter` ·
`dolphin-emu.org/docs/guides/how-use-official-gc-controller-adapter-wii-u/`
(owner: *"a really good resource"*).

**A SPIKE, NOT A FEATURE** — output is `docs/research/gc-adapter.md` plus a
go/no-go, following the `spikes/` precedent. Questions it must answer, in
feasibility order:
1. **Does the FunKey-S micro-USB port do USB HOST at all** (OTG capable, and
   is the kernel built with host + the needed HID/usbfs support)? If no, the
   spike ends here and everything below is moot. **Answer this first.**
2. Power: the official adapter expects 500 mA and has its own supply leg —
   can this device provide or bypass it?
3. Driver shape: `wii-u-gc-adapter` is libusb userspace → uinput. Is libusb
   present or cross-buildable with the FunKey SDK? Does uinput exist?
4. Latency and CPU cost against the 16.67 ms budget (§A-par: perf is the
   binding constraint on this device).
5. Licensing: any vendored code needs its `NOTICES` entry BEFORE it lands
   (project licensing rule).

**Payoff if GO is large and not just "more players":** it makes the
"CONTROLLER" branch of the Controls menu real (A24), justifies per-player
bindings (A31), and gives a true analog stick, which retires the whole
digital-d-pad compromise documented in `ctl_style.h:14-23` (no walk, no
partial DI angles, no angled f-tilts).

---

### A34 (P2, BUG) — "POWER OFF" does nothing

**Symptom (owner):** *"power off option from funkey-s menu options doesn't
work."*

**Ground truth: it is implemented, so this is a DIAGNOSIS row.**
`foh_pause.c:366` defines `SYS_OPT_POWEROFF` in the system menu
(`{VOLUME, BRIGHTNESS, QUIT, POWER OFF}`), rendered at `:369`, with a live
arm at `:570`. **The option exists and is wired — find out why the action does
not take.**

Leads: whether the arm invokes a real shutdown path (and whether that path
needs privileges the OPK process has), and whether the FunKey frontend expects
the app to EXIT and let the OS power down rather than powering down itself.
Compare against QUIT, which reportedly works — the diff between the two arms
is the whole investigation.

`done-check:` selecting POWER OFF powers the device down from the play OPK;
tooth proving a no-op arm fails.

### A29 / A32 — MEASURED 2026-08-23, both rows change shape

**A32 IS NOT AN L-CANCEL BUG. It is a double-negative label.** Measured in
`render_opt_gameplay` (`foh_render.c:1762-1793`): the gameplay screen has FIVE
rows — `{"TURBO MODE", "L-CANCEL", "FLASH ON L-CANCEL", "EVERYONE WALLJUMPS",
"TAPJUMP OFF"}`. **L-CANCEL is row 1 and is a SINGLE GLOBAL value**
(`vals[1] = kLCancelNames[s->lCancelType]`). The only row with per-player
P1/P2/P3/P4 columns is row 4, **"TAPJUMP OFF"**
(`:1789-1791`, `s->tapJumpOff[k] ? "ON" : "OFF"`).

So the "off for p2, p3, p4" the owner saw is **TAPJUMP OFF**, not L-cancel —
and it is CORRECT: P1 has tap jump disabled because a digital d-pad at full
deflection tap-jumps on every upward DI (`ctl_style.h:14-23`), and P1 is the
only human port on this device today.

**The REAL defect here is the double negative:** a row named "TAPJUMP OFF"
whose value reads "ON" means tap jump is DISABLED. That is genuinely
unreadable, and it is what made the owner misread the screen.
**Recommendation: relabel row 4 to "TAP JUMP" and invert the displayed value**
so ON means tap jump works. Note upstream renders the same double negative
(:242), so this is a small **deviation** — register it.
**No L-cancel default change is warranted** (upstream authored default is 0,
`sim_boot.c:19`); if the owner still wants L-cancel on by default after
reading the above, that is a separate D-numbered deviation.
**Re-open only if A33 lands:** with a second physical controller, P2+ become
human ports and would need `tapJumpOff` too.

**A29 — THERE ARE TWO CHARACTER PLANES, and that is almost certainly the bug.**
- `p1Char` / `p2Char` — the SELECTION plane, `foh_init` sets **both to 0**
  (`foh.c:47-49`, "characterSelections default [0,0,0,0] -> marth/marth").
- `cssChar[2]` — a SEPARATE **TOKEN** plane, `foh.c:342` says so explicitly
  ("the TOKEN plane (css.js:66), **not setCS's**"); written only through
  `css_char_of()` (`foh.c:402`) inside `step_css`, and read by the renderer at
  `foh_render.c:1643`.

The reported display is **marth(0) / puff(1) ALWAYS, regardless of the picks**
— i.e. a constant `{0, 1}`, which is neither `foh_init`'s `{0,0}` nor the
picks. **So the renderer is showing an index-identity, not either plane's real
value.** Investigate in this order: (1) does the CSS panel/token render fall
back to the port index `k` rather than the plane; (2) do the two planes
disagree after match exit — is one preserved and the other reset; (3) only
then look at the re-entry path.
**Do not "fix" this by resetting both planes on entry** — that would trade a
wrong display for a silently discarded selection. The tooth belongs in
`check-mexit-reentry.sh`.

### A28 — MEASURED 2026-08-23 (lane A): THE DIGITAL PATH IS PROVEN CLEAN

All three filed hypotheses are **FALSE, by measurement, not by reading.**

- **H1 (fill leaves the buffer uninitialised) — FALSE.** `snd_mix_fill`
  (`port/gfx/snd_mixer.h:666-712`) opens each frame with `int32_t acc = 0;`
  (`:669`) and **ASSIGNS** both outputs (`:709-710`), never accumulates onto
  them; the voice loop skips inactive voices without touching `out` (`:672`).
  Proved by compiling the real header and driving the real function with a
  POISONED output buffer across every idle state the device can hold —
  bit-zero output in all of them.
- **H2 (22050 pipeline vs 44100 device) — FALSE, and not a second defect.**
  Both planes resample: SFX via `m->step = (SND_SRC_RATE << 16) / SND_OUT_RATE`
  = `0x8000` (`snd_mixer.h:503`, consumed `:670,:678`); music by zero-order
  hold (`:697`).
- **H3 — inapplicable at idle:** emitted samples are exactly 0, so no DC term
  and no partial block.

Also cleared: the music ring is fully written BEFORE `wr` is published
(`snd_mixer.h:270-289`; `foh_dev.c:983-996,1067-1069`), so the malloc'd ring is
never read uninitialised; boot leaves music off (`foh_dev.c:1063,1931`).

**NEW STANDING INSTRUMENT:** `bash port/gfx/check-snd-idle.sh` →
`SND IDLE SILENT cases=7 frames=3245`, exit 0 — compiles `snd_mixer.h`
verbatim and asserts bit-exact silence over 7 idle cases with a poisoned
buffer. **Carries its own tooth as step [2/3]** (a run that skips the fill
must be REJECTED; verified rejecting with `sample 0 ... is -23131`), so it
cannot go vacuous. Zero production files were edited — patching a path that
is provably correct would have been a wrong fix.

**A28 IS THEREFORE STILL OPEN, BUT RELOCATED: the buzz is BELOW our mixer.**
Remaining candidates, both device-side and both needing the FunKey:
1. **Analog/amp idle noise (leading).** The codec amp is energised by
   `SDL_OpenAudio` (`platform_audio_sdl.h:132`) and stays energised for the
   whole process lifetime — which matches the report exactly (starts at
   launch, precedes any sound, lasts the session).
   **CHEAPEST DISCRIMINATOR, RUN THIS FIRST:** launch with `sndpack.bin`
   absent from the data dir — `mlfk-foh.sh` gates on `[ -f "$DATA/sndpack.bin" ]`
   so `platform_audio_start` is never reached (`foh_dev.c:1928-1935`).
   **If the buzz persists with audio never opened, it is not ours at all.**
2. **ALSA xruns at 512 frames / 11.6 ms.** The existing `underruns` counter is
   BLIND to these BY CONSTRUCTION and the header says so
   (`platform_audio_sdl.h:44-51`, "DMA-XRUN BLINDNESS"): an xrun makes the SDL
   write return SOONER, so inter-callback gaps only shrink and the counter
   stays 0. Test with `--audio-samples 1024` (`foh_dev.c:1555,1701`).

### A35 (P2, REGISTERED OBSERVATION — not a defect, do not "fix" casually)

Surfaced by lane A while clearing A28. `docs/research/audio-spike.md:31-34`
records the device granting **22050 stereo exactly**, while the app opens
**44100** and zero-order-hold-upsamples both planes 2x — which places a
full-amplitude spectral image across roughly 11-22 kHz.

**This is NOT A28** (it exists only while audio is PLAYING, and the reported
buzz precedes any sound), which is why lane A flagged it rather than acting.
Recorded so it is not rediscovered later.

**Before touching it, know the blast radius:** the open rate is guarded by an
exact-spec-or-fail check (`platform_audio_sdl.h:120-150`) and changing it
moves the mixer/music fidelity goldens. **Also resolve the apparent tension
first:** the app demands 44100 and hard-fails on any renegotiation, yet audio
works on device — so either the spike measured a different REQUEST than the
app makes, or the doc's reading needs re-taking. Measure before designing.

## A33 CONSEQUENCES — NO-GO, and it CLOSES three standing re-open clauses

**A33 = NO-GO (2026-08-23, lane R; full evidence `docs/research/gc-adapter.md`).**
Q1 gates the ticket and Q1 is NO: **the FunKey-S micro-USB port cannot host a
bus-powered device, and it is settled in HARDWARE, not software.** Four
independent locks, each sufficient alone:
1. **The USB ID pin is deliberately unwired.** FunKey's OWN hardware reference
   says resistor **R4** "should probably not be mounted... this pin should be
   left floating". An OTG cable signals host role by grounding ID; on this
   board ID terminates in an unpopulated pull-up. The V3s SoC is dual-role
   capable; **the board is not wired to use it.**
2. `sun8i-v3s-funkey.dts:210-213` — `dr_mode = "peripheral"`, and `&usbphy`
   declares no ID-detect GPIO, so the kernel does no role detection at all.
3. `linux.config:115` — `CONFIG_USB_MUSB_GADGET=y` with no `HOST`/`DUAL_ROLE`.
   In 4.14 those are a mutually-exclusive Kconfig choice, so host-side MUSB is
   **not compiled**, not merely disabled. Also `# CONFIG_HID is not set`.
4. **Power is a second, independent hardware kill:** `reg_vcc5v0` is a
   `regulator-fixed` with no `gpio` and no `vin-supply` — it is the INCOMING
   5 V rail, not a boost. Nothing can push VBUS outward. The device also caps
   its own charge at 400 mA, under the adapter's 500 mA ask.

The one apparent escape was chased and closed: the DTS does mark
`&ehci0`/`&ohci0` `okay` and both drivers are built in, but the V3s declares
exactly ONE USB PHY, neither carries a `phys` phandle, and there is no second
connector — inherited sunxi board-template boilerplate.

Also measured: **`gcadapterdriver` (owner-provided) is macOS-only** — Xcode
kext/DriverKit, no libusb, no Linux target — so it was never portable here.
The Dolphin guide URL returned **HTTP 403** and was NOT reconstructed from
memory; no claim depends on it.

### THE CONSEQUENCE THAT MATTERS MOST
**The digital-d-pad compromise at `ctl_style.h:14-23` is PERMANENT on this
hardware** — no walk, no partial DI angles, no angled f-tilts, no C-stick from
a real analog stick, ever. That makes `docs/research/b0xx-mapping.md` and the
BOX scheme **the answer, not a stopgap**, and it RAISES the value of A30b/A31
(button layout is now the only lever the player has).

### Three standing clauses are now DEAD — do not act on them
- **A24** — no second controller will ever appear. **NEW DESIGN QUESTION FOR
  THE OWNER (see below).**
- **A31** — the "design per-port, retrofit if A33 lands" clause is dead.
  **Ship UI editing port 0 only, and do not build the per-port plane.** This
  makes A31 materially CHEAPER: no `MLFKPERSIST5` bump for a bindings-per-port
  plane, only for the bindings themselves.
- **A32** — the "re-open if A33 lands, P2+ become human ports" note is dead.
  **P1 stays the only human port.** `tapJumpOff` P1-only is correct forever.

### A24 — OPEN OWNER QUESTION created by this NO-GO
The Controls submenu has two entries, `CONTROLLER` and (renamed) `HANDHELD`.
**`CONTROLLER` is now permanently dead UI.** Two options:
- **(a) Grey it out**, consistent with the owner's 2026-08-23 A10 ruling
  (*"don't hide but gray out"*) — honest about the hardware, consistent
  treatment of dead entries.
- **(b) Collapse the submenu entirely** so `CONTROLS` opens the button config
  directly — a two-entry menu with one live entry is pointless navigation.

**Driver recommendation: (b) collapse, with a one-line note in the doc that
the controller branch was removed for hardware reasons.** A10's grey-out
ruling was about entries whose feature EXISTS upstream but is dead on device;
this is an entry whose feature can never exist at all. But it is the owner's
menu — ask.
**NOTE this also revisits the A24 rename:** `HANDHELD` was chosen partly
because `HANDHELD vs CONTROLLER` is a clean pair for a future with two device
classes. With no such future, if the submenu collapses the label question
mostly disappears; if it stays, `HANDHELD` still reads better than `FUNKEY`.

### A29 — CLOSED 2026-08-23 (lane M). MY FILED HYPOTHESIS WAS FALSE.

**Root cause: `port/foh/foh.c:346-353`**, the rest-slot-2 arm of
`foh_css_token_pos()` — the endGame SNAP slot indexed the roster by the **PORT
index** (`foh_css_cell_x(k)`) instead of the chosen character
(`foh_css_cell_x(c)`). `foh_dev.c:1339-1340` (`tdev_end_game`) puts BOTH tokens
in rest slot 2 on every match exit, so port 0's token snapped to cell 0 (marth)
and port 1's to cell 1 (puff) — the reported constant `{0,1}`.

**THE FILED HYPOTHESIS ("something re-inits `cssChar` to an index identity")
WAS WRONG, and the correction is the useful part.** Measured with a driver
linking the real `foh.c`:
```
after picking:     p1Char=3(falco) p2Char=0(marth) cssChar={3,0} rest={0,0}
after match exit:  p1Char=3(falco) p2Char=0(marth) cssChar={3,0} rest={2,2}
                   tok0 -> cell 0, tok1 -> cell 1
```
**BOTH character planes survive intact and the relaunch uses the real picks.**
Only the token GRAPHIC lied. It read as lost selection because at 240x240 the
token is the ONLY roster-level pick indicator — `render_css` draws no
selected-cell highlight, only a hover one.

**MY `done-check:` FOR THIS ROW WOULD HAVE BEEN VACUOUS.** As written it said
*"assert both slots still read the picks"* — and that assertion **already
passed before the fix**. A row's stated check is not automatically a real
check; this one tested the plane the bug never touched. The real observable is
the TOKEN CELL. Standing lesson: when filing a `done-check:` from a symptom
report, name the observable the REPORTER actually saw, not the state you
suspect underlies it.

**Fix:** one token, `foh_css_cell_x(k)` -> `foh_css_cell_x(c)`, registered as
**DEVIATION D21** (MENU-SPEC ledger + in-file). Upstream genuinely indexes by
port, but it is a TYPO not a design — `setChosenChar` passes two args to a
one-arg callee that drops the second (`css.js:146` vs `:154`). Nothing on the
launch plane moves; this changes only where a token is drawn and hit-tested.

**Tooth:** `bash port/foh/check-css-token-rest.sh` -> `CSS TOKEN REST CHECK OK`,
exit 0 (host-only, data-free, ~3 s). Drives the REAL `foh_tick` through real
CSS gestures with the cursor walked by FEEDBACK (never a copied frame count)
and no character field hand-poked; picks falco(3)/falcon(4), distinct from the
defaults `{0,0}`, from index identity `{0,1}`, and from each other. **T1
negative test** rebuilds against a D21-reverted `foh.c` and requires it to fail
reproducing `{0,1}` — both directions proven. Grammar-pinned to the two
`foh_dev.c` lines it mirrors, so a moved production snap kills the check loudly
instead of testing a stale model.

### A36 (P3, UNCONFIRMED — do NOT "fix" it until it reproduces)

Lane M reported a latent bug in `port/foh/check-mexit-reentry.sh:124-146`:
`tree_fingerprint()` pipes `git ls-files -o` into `xargs shasum`, and git
reports a wholly-untracked NESTED CHECKOUT as a bare directory, so an installed
`oracle/harness/node_modules` would make the check die with
`shasum: ... Is a directory`.

**I COULD NOT REPRODUCE IT ON `agent/auto` (2026-08-23).** Measured: the tree
HAS `oracle/harness/node_modules/` installed, and `git ls-files -o
--exclude-standard` returns it ZERO times because it is gitignored at
`oracle/harness/.gitignore:1` (`node_modules/`), confirmed with
`git check-ignore -v`. The check uses `--exclude-standard`, so the entry never
reaches `xargs`.

So either the lane hit a variant condition (a nested checkout NOT covered by
that ignore — e.g. a dependency shipping its own `.git`), or the diagnosis
generalised from a worktree-local state. **Left UNFIXED deliberately:
`check-mexit-reentry.sh` is a protected check (HARD RULE 3) and the failure
mode is FAIL-CLOSED (it errors, it cannot false-pass), so there is no safety
pressure to patch it blind.** Re-open with an exact reproducer.

## OWNER DECISIONS 2026-08-23 (round 3) — A10 CLOSED, A24 settled

**A10 — CLOSED, NO WORK REQUIRED. The shipped behaviour is already what the
owner wants.** Owner, asked whether the dead netplay entries should be greyed:
*"for A10 yeah just go straight to vs. melee."*

This SUPERSEDES the earlier "don't hide but gray out" answer, which was given
before the driver had established the current state and flagged its cost. The
build ships `FOH_NETPLAY 0`, where menu-top's `VS. Melee` row runs the Local VS
action DIRECTLY (`foh.h:306-311`, menu.js:105) and the whole MPMENU page is
unreachable — so there is nothing on screen to grey, and greying would have
meant RESTORING that page and putting a menu walk between `VS. Melee` and the
CSS. **The owner chose the fast path. A10 requires no code.**
Nothing is deleted (`foh.h:309-311`): `FOH_MENU_BATTLE`, its labels, its four
A-arms, its B-back edge and its judge-registered transitions all still exist
and still compile behind `FOH_NETPLAY 1`. **Do not delete them** — that shell is
what makes this reversible, and C5 already reasoned it through.

**A24 — SETTLED: COLLAPSE THE SUBMENU.** Owner took the driver recommendation.
The Controls submenu (`CONTROLLER` / `HANDHELD`) exists ONLY to make a two-way
choice, and A33 proved one side can never exist on this hardware — so the
options row `CONTROLS` opens the button-config screen DIRECTLY.

Measured current state of the dead branch, so nobody re-derives it:
`render_ctrl_pad` (`foh_render.c:1902-1916`) is already an honest dead end —
it draws `ERROR: NO CONTROLLER DETECTED`, `THE FUNKEY-S HAS NO GAMEPAD PORT`,
`AND NO CALIBRATION TO RUN.` So this is NOT a correctness fix; it removes a
guaranteed-wasted navigation step.

**WHY COLLAPSE HERE BUT GREY ELSEWHERE (the distinction that decided it):**
greying suits a menu that KEEPS live entries beside the dead ones — the page
still has a job. A two-entry chooser whose purpose IS the choice has no job
left once one side dies; greying would preserve a screen that asks a question
with one answer.

**Scope of the collapse (do not over-reach):**
- `FOH_CTRL_PAD` (upstream gameMode 14) and `render_ctrl_pad` are NOT deleted —
  same reversibility logic as `FOH_MENU_BATTLE` above. They become unreachable,
  exactly as `FOH_MENU_BATTLE` already is.
- The enum, the gameMode numbering and the judge-registered transitions do NOT
  get renumbered. Routing changes; identity does not.
- `FOH_MENU_CONTROLS` (the submenu screen) becomes unreachable by the same
  pattern. Register it as a MENU-SPEC deviation with A33 as the cited cause.
- **This lands WITH the A24/A4 renames in ONE ticket** — same files, same
  shots, and doing them apart re-freezes the controls screens twice.

### A33 — AMENDED 2026-08-23. THE POWER KILL IS RETRACTED. READ BEFORE CITING IT.

The A33 section above cited **FOUR** independent locks. **One of them (Q2,
power) was WRONG and is retracted in `docs/research/gc-adapter.md` §2, in
place, so the reasoning stays auditable.**

**What was wrong:** the official adapter has **TWO** USB plugs — black is data
plus logic power, grey is **power-only, for rumble**. Nintendo's own support
page says only the black plug is required; omitting grey merely stops rumble.
So the ticket's **500 mA was never a host-side obligation** — it is an
aggregate including the rumble motors, which sit on the leg that need not come
from the host at all. **No self-powered hub is needed**; the grey leg is a bare
power input any charger feeds. Strike the "powered hub dangling off a keychain
console" framing entirely.

**What survives from Q2, and it is much narrower:** the board cannot *assert*
VBUS on J2 (`reg_vcc5v0` is a `regulator-fixed` INPUT rail — no `gpio`, no
`vin-supply`). But **"cannot source VBUS" is not "the peripheral goes
unpowered"**, and conflating those was the error. A host asserts VBUS both to
feed the device and to signal port power; the correction kills the first
reason, and the second is now recorded as an honest **UNKNOWN**, not a kill.

**THE VERDICT STILL HOLDS — but on ONE leg, not two.** Q1 alone carries it:
`CONFIG_USB_MUSB_GADGET=y` with no `HOST`/`DUAL_ROLE` (mutually exclusive
Kconfig in 4.14, so host code is NOT COMPILED), `dr_mode = "peripheral"` in the
DTS, and the vendor's deliberately floating ID pin. Undoing that means
**rebuilding and reflashing FunKey-OS**, and this project ships an **OPK** —
out of scope in a way power never was.

**DRIVER OBLIGATION, NOT YET DISCHARGED.** With the hub gone and the mA figure
moot, the residual `&ehci0`/`&ohci0` ambiguity (§1.4) is no longer negligible.
**ONE FREE COMMAND on a reconnected device — no adapter, no cable, no charger:**
```
adb -s 12c00003237f5528 shell "sh -lc 'ls -d /sys/bus/usb/devices/usb* 2>&1'"
```
If a root hub is live, §1.4 flips and **A33 deserves a second look.**
**RUN IT BEFORE closing A24 / A31 / A32 on this spike's authority.** Retiring
three tickets on a NO-GO with an untested assumption inside it is exactly the
stale-red class this project keeps paying for.

**CONSEQUENCE FOR THE OWNER'S A24 DECISION (driver duty to surface this):**
the owner ratified collapsing the Controls submenu on the driver's framing
that CONTROLLER is *"permanently dead, settled in hardware."* **That framing
was stronger than the evidence now supports.** The accurate statement is:
*dead for anything this project can ship, because the fix requires reflashing
the OS.* The decision is still defensible on that basis — but it was taken on
the stronger claim, so the owner gets told and may re-affirm or revisit.
**Method lesson recorded by the lane itself:** it reached for a second kill
before the first needed help, and the HTTP-403 on the Dolphin guide is *why* —
the owner had read that page and the lane had not.

## A33 — RE-AMENDED 2026-08-23. **VERDICT INVERTED: NO-GO -> UNKNOWN.**
## THE TWO SECTIONS ABOVE ARE SUPERSEDED. DO NOT ACT ON THEM.

The owner told the spike he can **fork DrUm78's FunKey-OS**, and that collapsed
the NO-GO. The spike's own words on why it was wrong, recorded rather than
silently edited: the verdict rested on *"rebuilding and reflashing FunKey-OS —
an image this project neither owns nor ships"*, which was **pricing the owner's
appetite for work and calling it evidence.**

**THE FORK IS FOUR LINE-EDITS**, measured against the owner's ACTUAL tree
(`DrUm78/FunKey-OS` @ `FunKey-OS-DrUm78`, `DrUm78/linux` @ `v1.0.3-funkey-s` —
byte-identical to upstream in the USB region, so the source reading holds while
the conclusion does not):
1. `sun8i-v3s-funkey.dts:251-254` — `dr_mode = "peripheral"` -> `"host"`
2. `linux.config:115` — `CONFIG_USB_MUSB_GADGET=y` -> `DUAL_ROLE`
3. `funkey_defconfig` — add `BR2_PACKAGE_LIBUSB=y` + `BR2_PACKAGE_EUDEV=y`
~1.5 h Docker build + reflash per DrUm78's README. `uinput` is already present.
**Ordinary work, not a research programme.**

**The ID-pin argument is ALSO walked back** to ASSUMED-strong (not measured):
that resistor blocks *automatic* OTG role negotiation, and a `dr_mode = "host"`
build never consults ID. The 4.14 driver source was not read.

**WHAT ACTUALLY REMAINS IS ONE ELECTRICAL QUESTION.** The board cannot SOURCE
VBUS (`reg_vcc5v0` is a `regulator-fixed` input rail). With the grey-leg
correction that no longer means the adapter goes unpowered — it means the port
never *signals* port power. **Whether a sunxi host port that never drives VBUS
still enumerates a SELF-POWERED device is not answerable from source.**

### §7 is now a COST-ORDERED LADDER, not a verdict
- **Rung 1 — FREE, device only, no hardware:** `ls -d /sys/bus/usb/devices/usb*`
  plus a dmesg grep. `&ehci0`/`&ohci0` are ALREADY `okay` and built in with no
  `phys` phandle on a single-PHY SoC — either boilerplate, or live host
  controllers starved by MUSB. This settles which.
- **Rung 2 — OTG cable + any charger on the grey leg, NO hub:** does anything
  enumerate on the stock image?
- **Rung 3 — ~2 h:** fork, three edits, flash, retest. **This is the real
  answer.**
- **Rung 4:** build the driver, then measure latency (§4).

### TWO RISKS — flagged, not buried
1. **`adb` RUNS OVER PERIPHERAL MODE.** It is how every rig script and every
   gate reaches the device. **Verify dual-role preserves adb BEFORE building
   anything on the new image, and keep a rollback SD.** If dual-role breaks
   adb, the entire verification rig goes with it — that is a far larger loss
   than controller support is a gain.
2. **Latency is a real second risk, not a footnote.** p99 is already
   7.95-10.68 ms against 16.67 ms (~6 ms headroom) and the adapter's poll rate
   was never read.

### THE THREE CLOSURES ARE REVERSED — the spike itself reversed them
The earlier draft told the driver to close **A24 / A31 / A32** on this spike's
authority. **§8 now says explicitly NOT to.** Those "re-open if A33 lands"
clauses **STAY OPEN** until at least rung 3 runs. Concretely:
- **A31** — do NOT assume port-0-only. The per-port binding plane may be needed
  after all. **Design per-port; ship port-0 UI.** (Which is what the ORIGINAL
  recommendation said before the NO-GO overrode it.)
- **A32** — the `tapJumpOff` P1-only re-open note is LIVE again.
- **A24** — see below. **This one already reached the owner and needs saying.**

### A24 — THE OWNER'S DECISION WAS TAKEN ON RETRACTED EVIDENCE
The owner ratified collapsing the Controls submenu on the driver's framing that
CONTROLLER is *"permanently dead, settled in hardware."* **That framing has now
been retracted twice** — first the power leg, now the whole verdict.
**The honest current state: a real controller may be possible for ~2 h of OS
work, pending one free command.** Collapsing the submenu is now the WRONG
default, because the branch it deletes may become live.
**DRIVER ACTION TAKEN: A24 is HELD.** The RENAMES (`CONTROLS`, `HANDHELD`,
`BOX`/`CLASSIC`/`NATURAL`) are unaffected and stay ratified; the COLLAPSE is
suspended pending rung 1. The owner is told, and re-decides.

### A27 — BLOCKED ON THE SIM PLANE (measured by lane M, 2026-08-23)
`setVersusMode(1 - versusMode)` is a **BINARY toggle** (`main.js:140/237`):
0 = "4-man survival test!" (stocks), 1 = "An endless KO fest!" — **exactly the
two modes the owner named.** It is **SIM-VISIBLE, not cosmetic.** Our sim
carries the read sites (`port/sim/physics.c:1308`,
`action_state_shortcuts.c:148`), but three things pin it off:
1. `port/sim/sim/sim_boot.c:423` pins `versusMode = 0`
2. `main.js:1334`'s "every player starts on 1 stock" arm is a no-op comment at
   `sim_boot.c:421`
3. `port/sim/sim/sim_tick.c:380` has the `!versusMode` matchTimer arm commented
   out — **an endless match would run the clock into `sim_fatal`**

All three are outside `port/foh/` and INSIDE `check-sim.sh`'s frozen TU list.
A ribbon that toggles only its own label is a STUB (HARD RULE 2), so lane M
left it drawn and inert and recorded why. **A37 (below) is the prerequisite;
the ribbon hit-test is then ~10 lines in the FOH.**

### A37 (P1, NEW — SIM LANE, prerequisite for A27)
Make `versusMode` real: unpin `sim_boot.c:423`, implement the 1-stock arm at
`sim_boot.c:421`, and restore the `!versusMode` matchTimer arm at
`sim_tick.c:380`. **Touches `check-sim.sh`'s frozen TU list, so it is a
CHECKSUM-SURFACE change** — it must prove all 8 goldens still conform with the
flag OFF (bit-identical, the D20 pattern), and it is a different PLANE from the
menus lane so it does not queue behind it.

### STANDING HAZARD (measured by lane M, 2026-08-23)
**`npm install` in a lane worktree LOOSENS A VERSION PIN.** Provisioning
Playwright rewrote `oracle/harness/package.json` from `"playwright": "1.61.1"`
to `"^1.61.1"`. Lane M caught and reverted it. **Any lane that runs `npm
install` must re-check that file before committing** — a silently loosened pin
is exactly the browser-engine drift that cost a whole session on 2026-08-05.

### A38 (P2, NEW 2026-08-23 — owner-requested) — adopt `write-legible-c`, SCOPED

Owner: *"can we add that we want to utilize this: github.com/7etsuo/write-legible-c
... investigate to see if and how we can incorporate it for claude code and in
general (does it really require the plugin? what is the plugin doing?)"*

**MEASURED by cloning the repo (11 files total), not from the README:**
- **The plugin does NOTHING at runtime.** `plugins/write-legible-c/.claude-plugin/plugin.json`
  contains ONLY metadata — name, version, description, author, homepage,
  keywords, license. **No `hooks`, no `commands`, no scripts, no executable
  anything.** `marketplace.json` is a listing that points at the same folder.
- **The entire payload is TWO markdown files:**
  `skills/write-legible-c/SKILL.md` (99 lines) and
  `references/c-standard.md` (772 lines, 18 sections).
- **It is ALREADY dual-packaged for Claude Code** — a real `.claude-plugin/`
  directory with Claude Code's own `marketplace.json` schema, not Grok-only as
  the README's prose suggests.
- **SO: NO, THE PLUGIN IS NOT REQUIRED.** The skill frontmatter is valid Claude
  Code (name + description + trigger list). Copying
  `plugins/write-legible-c/skills/write-legible-c/` to
  `~/.claude/skills/write-legible-c/` is behaviourally identical to installing
  the plugin. The plugin is a distribution wrapper, nothing more.

**THE DECISIVE COMPATIBILITY FINDING — the skill defers to us.** Its own
"Resolve constraints" section says: *"Follow higher-priority user, repository,
ABI, wire-format, GENERATED-CODE, platform requirements when they conflict with
this skill"*, *"Do not widen a scoped task into a repository-wide rewrite merely
because nearby untouched C predates the standard"*, and *"When a required
constraint forces a deviation, add a comment at the deviation site stating the
constraint precisely."* It also says *"Do not claim compliance when required
checks could not run."* **That is compatible with HARD RULES 1-8 by
construction** — but only if the precedence is stated explicitly, because the
skill auto-triggers on ANY `.c`/`.h` edit.

**IT MUST BE SCOPED BY DIRECTORY. UNSCOPED IT WOULD DESTROY THE PORT.**
| Zone | Verdict |
|---|---|
| `port/foh/**`, `port/gfx/**`, checks/witnesses, `port/tools/**` | **ADOPT.** Our own code, written fresh. This is exactly its target. |
| `port/sim/**` (esp. `characters/**/moves/*.c`, `physics.c`, `action_state_shortcuts.c`) | **FORBID.** |
| `pipeline/build/**` generated `ml_tables.c` / `ml_stages.c` | **FORBID** (generated code; the skill itself exempts this). |
| `port/fdlibm/**`, `port/ryu/**` | **FORBID** — vendored verbatim, provenance-pinned. |

**WHY THE SIM IS FORBIDDEN, concretely.** Those files are deliberately
structure-parallel to upstream JS and carry, ON PURPOSE and under HARD RULE 5:
upstream typos kept verbatim (`hitboxes.FRAMES++`, lowercase `phys.autocancel`,
THROWBACK's `Math.floor(t+0.01<37)` floor-over-comparison); dead arms that are
upstream typos (SIDESPECIALGROUNDHIT reads `phys.timer`, DOWNSPECIALGROUNDENDAIR
reads `player.timer` — both undefined, comparisons always false), commented and
never "fixed"; **infinite recursion carried verbatim** (SIDESPECIALAIR's
grounded arm); and magic numbers that are upstream DATA. The standard's "no
dead code", "no naked literals", "bounded loops, no recursion", "early returns"
would each **break faithfulness and therefore checksum conformance.** JS
compound-assignment grouping (`a += b + c`) already cost 22 one-ulp divergences
when left-flattened — this code is not free to be tidied.

**PROPOSED FORM (do not execute without owner sign-off on the route):**
1. **Install USER-LEVEL** (`~/.claude/skills/write-legible-c/`), NOT in-tree.
   **Licensing consequence, and it is the deciding argument:** vendoring MIT
   code INTO this repo triggers the project rule that `NOTICES` gains an entry
   BEFORE any third-party code lands in-tree. User-level install keeps it out
   of the tree entirely and costs nothing.
2. **State precedence in CLAUDE.md** in one short block: HARD RULES > repo
   conventions > write-legible-c; and the forbidden-zone table above. Without
   this the skill auto-loads on sim files.
3. **Do NOT retrofit.** Applies to code TOUCHED FROM NOW ON in adopted zones
   only — the skill's own "do not widen a scoped task" rule agrees.

**OPEN QUESTION FOR THE OWNER:** its §17 wants a repo-root `AGENTS.md`. This
project already carries its conventions in `CLAUDE.md` + `docs/PROCESS.md` +
`docs/loop/*`. **Recommendation: do NOT add `AGENTS.md`** — a second
conventions file is a divergence hazard, and CLAUDE.md is already the SSOT the
skill's own precedence rule tells it to obey.

### CORRECTION 2026-08-23 — "RE-FREEZING SHOTS" IS NOT A THING. I HAD THIS WRONG.

Surfaced by lane M (A32/A25a) and **verified independently by the driver.**
The driver has repeatedly priced menu work as *"re-freezes all 15 frozen menu
shots plus their device twins"*. **There are no committed screenshot bytes to
re-freeze.** Measured:
- `git ls-files` matching `*.png|bmp|ppm|pgm` returns **5 files, none of them a
  gate reference**: `port/gfx/opk/icon32.png` (the OPK icon) and four
  `spikes/device-feasibility/results/*.png` (spike-era research artifacts).
- **Device shots are judged against a HOST TWIN BUILT IN THE SAME RUN** —
  `port/foh/check-device-foh.sh:1051`:
  `cmp "$df" "$rf" || fail "shot $ctx: device shot != host twin reference"`.
- `check-foh-flows.sh` judges shots **run-A vs run-B** (determinism +
  distinctness), never against bytes on disk.

**THE REAL COST OF A UI CHANGE IS DEVICE TIME TO RE-RUN THE LEGS (30-40 min
each), NOT A RE-FREEZING RITUAL.** Re-read STATE.md's A14 note in that light:
"the 15 shots must be re-taken" means the legs must be RE-RUN, not that
artifacts must be replaced.

**WHAT IS GENUINELY COMMITTED AND CAN MOVE** (do not over-correct the other
way): the per-shot statistical ENVELOPES (A20's colour/coverage bands in
`port/gfx/check-device-opk.sh`), and the frozen DATA planes
`port/gfx/{gfxdata,vfxdata,vfxglyphs}-frozen.txt` plus their SHA pins — A14's
first half already moved all five `VFXGLYPHS_SHA256` pins. Those are pins over
data and statistics, not over pixels.

**Consequence for planning: A14 is cheaper than quoted on the artifact axis and
unchanged on the device axis.** Its real cost is the 41 call-site rewrite plus
device leg time — not a freezing ceremony.

### A36 — **CONFIRMED 2026-08-23** (was UNCONFIRMED). Exact mechanism, exact reproducer.

Lane M (B4) hit it and named the mechanism, which the earlier report had only
generalised: **`oracle/harness/.gitignore`'s rule is `node_modules/` — with a
TRAILING SLASH, so it matches DIRECTORIES ONLY.** A **symlink** named
`node_modules` is therefore untracked-but-NOT-ignored, so
`check-mexit-reentry.sh`'s `tree_fingerprint()` (`:124-146`) lists it, pipes it
to `xargs shasum`, and `shasum` dies on it. **The guard then fails CLOSED.**

That is why the driver could not reproduce it on `agent/auto` (2026-08-23):
there the path is a REAL DIRECTORY, which the trailing-slash rule *does* ignore.
Both observations were correct; only the symlink case trips it.

**PRACTICAL RULE FOR EVERY LANE (this is the part that matters):**
- **DO NOT `npm install` in a lane worktree** — it loosens the playwright pin
  (standing hazard above).
- **DO NOT symlink `oracle/harness/node_modules` in either** — it breaks
  `check-mexit-reentry.sh` exactly as described.
- **DO `cp -R` it** (17 MB, 175 files) and `rm -rf` afterwards.
- `NODE_PATH` does NOT work: `dump-sim-data.js:46` requires an absolute path
  into that directory.

**Still NOT fixed, deliberately.** `check-mexit-reentry.sh` is a protected check
(HARD RULE 3) and the failure mode is fail-CLOSED — it errors, it cannot
false-pass. The one-line `-f` correction is known; it wants a reviewed change,
not a drive-by.

### THE FALSE-TICKET CLASS — named 2026-08-23, and it has now cost TWO P1 rows

**B4 was a false ticket.** `FOH_TMATCH` genuinely has no exit `ev_trans` in
`foh.c` — **but neither does `FOH_MATCH`, and the VS exit demonstrably works.**
`foh.c:1175-1177` says so outright: both match screens are *"terminal for the
FOH machine; the driver owns the sim from here"*. Target test already exits
three ways (START -> upstream's own `endGame` hook; all targets destroyed ->
2500 ms hold; both -> `MEX_TSS` -> `FOH_TSS`), all in `foh_dev.c`, all matching
upstream's `changeGamemode(7)` exactly. **`foh_dev.c:2794-2801` already carried
a `B4:` comment saying it was done.**

**A19 was stale in EXACTLY the same way** and is corrected in the same commit.

**THE CLASS, stated so it stops recurring:** *a probe that asserts ABSENCE must
name the instrument it would FAIL on.* Grepping for `ev_trans` "proves"
FOH_TMATCH is terminal — and applied honestly it proves the VS match is
terminal too, which is false. **The instrument was wrong, not the code.** Two
P1 rows sat "Confirmed STILL OPEN" for months on a grep whose conclusion nobody
re-ran.

This is the THIRD instance this session of a filed row being falsified by
actually running the thing (A29's root cause, A32's "L-cancel" symptom, now
B4+A19), and the SECOND time the 2026-08-04 B1 lesson has repeated verbatim:
**grepping for a defect's vocabulary finds the discussion of the defect, not
the defect.**

### LANE HYGIENE — BETTER node_modules RECIPE (supersedes the cp -R advice)

Lane M (A24) found the clean way, measured: **`npm install --no-save
--no-package-lock playwright@1.61.1`**, then verify `oracle/harness/package.json`
is byte-identical afterwards (it was — sha `1187da80…`). **The pin-loosening
hazard did not fire**, because `--no-save` stops npm rewriting the manifest.
That is cheaper than `cp -R` (17 MB, 175 files) and safer than a symlink (which
breaks `check-mexit-reentry.sh` per A36). **Still verify the manifest sha before
committing** — the guarantee is the verification, not the flag.

### A24 — DONE 2026-08-23 (D25). Two corrections to the driver's own brief.

1. **A4's `NORMAL` -> `CLASSIC` WAS ALREADY SHIPPED** under C31 —
   `ctl_style.c:40` already returns `"Classic"` and the render site uppercases
   it. Zero code needed. It had **no tooth** (only the frozen NATURAL
   screenshot exercised it), and now it has one. *Fourth* instance this session
   of a queued row describing finished work.
2. **The driver's `foh.h:495` warning was MISAIMED.** That site is
   `optRow`/`optCol`; the C30(c) chooser cursor is `FohState.ctlRow`
   (`foh.h:542`). The real live consumers were two the brief did NOT name, both
   found by grep and both of which would have silently asserted the old wiring:
   `foh_snd_witness.c:170-173` (drives the chooser **by row index**) and
   `flows/f04-nav.{flow,expect}` (two Controls SHOT markers + four frozen
   transition lines swap frames). Shot NAMES are unchanged, so the `FLOW_SHOTS`
   inventories in `check-foh-flows.sh:288` / `check-device-foh.sh:486` are sets
   that still match and needed no edit.
   **Lesson: a site table handed down by the driver is a STARTING POINT, not a
   closed set. Grep for index-driven consumers, not just for the strings.**

**The tooth's design is the transferable part:** it reads the row label off the
frame, presses A on that row through the real `foh_tick`, then reads the
DESTINATION SCREEN'S HEADER off the frame the machine actually lands on — so a
label and its destination cannot drift apart. Its **T2 negative reverts the
routing ternary ALONE while leaving the labels in place** (the half-swap trap);
without that test the check goes green on a build whose first row lies about
where it goes.

**Remaining leg:** `DEVICE FOH OK` has NOT been run (no device in-lane). That is
the outstanding half of this row's done-check.

### A24 second half — DONE 2026-08-23 (D27). The chooser is collapsed, behind one digit.

Owner, same session: *"collapse now - make easily revertable though please."*
Built on the `FOH_NETPLAY` precedent because that flag already has exactly the
properties the owner asked for: **`#define FOH_CTL_CHOOSER 0` (`foh.h`), and
flipping that ONE digit restores the pre-D27 route whole.** Nothing is deleted
— `FOH_MENU_CONTROLS`, `FOH_CTRL_PAD`, `render_ctrl_pad`, the chooser labels,
its A ternary and its B-back edge all still compile at either value.

**REVERTIBILITY IS TESTED, NOT ASSERTED.** `check-controls-labels.sh` now
builds `foh_controls_witness.c` at **both** values and asserts both routes:
CFG-0 (the tree as committed) and CFG-1 (derived by rewriting that digit in a
COPY of `foh.h`). **T2 SURVIVES UNCHANGED AND NOW RUNS TWICE** — the chooser's
A ternary is compiled at either flag value, so the flag-0 witness SEATS the
unreachable page directly (the `foh_snd_witness.c` `battle-B` idiom) and
exercises the same line the flag-1 build navigates to. New **T5** is T2's twin
for the collapsed arm: point the D27 dispatch back at the chooser and only the
collapsed route may fail.

**MEASURED, and worth carrying: a quoted `#include` cannot be shadowed by
`-I`.** The obvious way to build CFG-1 — put a modified `foh.h` on the include
path — silently does nothing, because `#include "foh.h"` resolves from the
INCLUDING FILE'S directory first. That leg would then have "passed" against the
shipped configuration: a false green in the exact shape this file exists to
refuse. So the whole `port/foh` half of the build is copied beside the derived
header (every copy `cmp`-proved byte-identical to its original), and the leg
additionally reads the CONFIGURATION'S OWN assertion line out of the output —
the flag-1 line must be present and the flag-0 line absent. A verdict alone
does not say which build produced it.

**CONSUMERS, found by grep and NOT trusting the handed-down list (the A24/D25
lesson held):** `foh.c`'s Options row-2 arm and `step_ctrl`'s B (routing);
`foh_snd_witness.c`'s `options-A-controls` case, which **moves sides in that
table** — the collapsed row is a `changeGamemode` LEAVE, so it emits ONE
`menuForward`, not the menuMODE change's two sounds; `judge-foh-trace.js`,
which gains a **second build profile** parsed live out of the header exactly as
it parses `FOH_NETPLAY`, six chooser edges moving to `ctl` and two collapsed
ones added as `noctl`; `judge-domains.authored.txt` (28 E rows, count re-pinned)
and `check-judge-regression.sh` (profile alternation, `liveProf`, the evaluated
region's second binding, a second header pin, a re-freeze log entry);
`flows/f04-nav.{flow,expect}` (10 transitions, one shot); the `FLOW_SHOTS`
inventories in `check-foh-flows.sh` / `check-device-foh.sh`; and the
`judge-foh-trace.js` producer sha pinned in four device/host checks.
**NOT touched, and measured why:** `foh_app.c` / `foh_dev.c`'s save-on-exit arms
key on `from == "controls-keyboard" && cause == "b"` — the collapse moves the
`to`, so both are unchanged and the Controls screen still persists on exit.

**A33's renderer line was FALSE and is corrected in the same change.**
`foh_render.c` said `THE FUNKEY-S HAS NO GAMEPAD PORT`. The port is physically
there; what is missing is HOST MODE in the shipped OS
(`CONFIG_USB_MUSB_GADGET=y`, no `HOST`/`DUAL_ROLE`, `dr_mode = "peripheral"`,
floating ID pin — `docs/research/gc-adapter.md` §1.4/§2, where the earlier power
kill is retracted in place). It now reads `FUNKEY-OS SHIPS NO USB HOST MODE` —
same 32 characters, so no geometry moved.

**BLOCKER for the driver, not for this lane:** `judge-foh-trace.js` changed, so
its `reviewed-go` row in `port/sim/device/m4-freeze-manifest.txt` (and the
`CLOSURE` row in `m4-closure-ledger.txt`) no longer matches the tree. Those are
arc/review status and were deliberately NOT touched here.

**Remaining leg:** `DEVICE FOH OK` has NOT been run (no device in-lane).

### A37 — DONE 2026-08-23 (lane S). `versusMode` IS REAL. A27 IS UNBLOCKED.

**THE TICKET'S STATED HAZARD WAS BACKWARDS.** A37's row (written by the driver
from lane M's report) warned that *"an endless match would run the clock into
`sim_fatal`"*. Upstream does the OPPOSITE: `main.js:1079` is
`if (!starting && !versusMode) matchTimerTick(input); else { startTimer -= … }`
— in endless mode upstream takes the **else** arm every frame forever, so
`matchTimer` **never ticks and therefore never expires**. The endless loop is
closed instead by `physics.js:980` (`stocks === 0 && versusMode -> stocks = 1`)
and `actionStateShortcuts.js:155` (`isFinalDeath` returns false), **both of
which this port already had.** Fifth premise falsified by measurement this
session.

**Upstream semantics carried, including the quirk:** `versusMode` is **PAGE
state**, not match state — `startGame` never resets it (`main.js:140`,
`:237-239`, toggled `setVersusMode(1 - versusMode)` from `css.js:393`). And
`main.js:1334-1336`'s "1 stock" arm sits **OUTSIDE** the `playerType[n] > -1`
guard in the same loop, so **all four slots get `stocks = 1`, inactive ones
included.** Carried verbatim under HARD RULE 5.

**Changed:** `sim_boot.c` (init moved from `sim_setup_match` to
`sim_boot_page`, because it is page state; the `stocks = 1` arm with its
outside-the-guard quirk); `sim_tick.c` (the `&& versusMode == 0` conjunct
restored); `sim_main.c` (`--versus-endless`, following the `--walljump-all`
precedent); new `port/sim/check-versus-endless.sh`, which **pins
`check-sim.sh`'s sha256 rather than duplicating its TU list** — a pattern worth
copying.

**Results, verbatim.** Flag OFF: `SIM CONFORMS` 8/8 **bit-identical**, plus
`AI LIVE CONFORMS` 10/10 — the D20 bar, met. Flag ON: `VERSUS ENDLESS OK` —
frame-1 stocks `4 4 -> 1 1`; over 29,000 frames of g01, versusMode 0 dies at
`SIM FATAL frame 4749: DEADDOWN: finishGame (final death)` while versusMode 1
runs to the end `SIM OK`. **Tooth:** dropping the conjunct makes the endless run
die at `frame 28890: matchTimer expired` — the guard is load-bearing, proven by
reverse-edit.

**No deviation row: making a ported-but-PINNED upstream feature live is not a
deviation.**

### ⚠ A SIBLING GATE CAUGHT A BREAK `check-sim.sh` ALONE WOULD HAVE SHIPPED
Adding `--versus-endless` to the **usage string** failed `check-ai-live.sh`'s
M2-contract witness, which pins that rejection message to exactly two lines.
**`check-sim.sh` on its own does not catch this.** Resolved by not listing the
flag (identical to `--walljump-all`), with the reason commented at the parse
site. **Standing lesson: on the sim plane, run the SIBLING gates too — the
checksum gate is not the only contract over those bytes.**

### A27 — UNBLOCKED. EXACT WIRING, MEASURED BY LANE S
1. Add a `versusMode` field to FOH state (`foh.h` already carries the note);
   `foh_app.c:535` currently hardcodes `versus=0` in the LAUNCH line.
2. Write it **IMMEDIATELY BEFORE `sim_setup_match`** — `foh_app.c:698` and
   `foh_dev.c:3070`: `G.sim.versusMode = foh.versusMode;`
3. **NOT in the gameSettings block at `foh_app.c:700-713`** where
   `everyCharWallJump` lives. `startGame`'s stocks arm reads the value DURING
   setup, so a post-setup write is TOO LATE. The ordering constraint is
   commented at both the `sim_boot_page` and `sim_main.c` sites.
4. **Render seam for whoever takes it:** `physics.c:1308` restores stocks AFTER
   upstream's `lostStockQueue.push`, so the HUD sees the 0 before the respawn.
   Our C marks that push render-plane no-op — correct today, and exactly the
   seam endless-mode stock icons will need.

### A31 — DONE 2026-08-23 (D26). All four sub-items, and TWO PRE-EXISTING BUGS FIXED.

**The design call, and why it is better than the sketch it replaced.** D13 had
sketched a listening mode ("hold-A clear, protected primaries"). Lane M did NOT
build it, deliberately: **the rows on this screen are PHYSICAL BUTTONS**, so
listening would have to mean *"now press the button you want to swap with"* —
backwards. Instead **L/R rebinds by SWAPPING** the selected row's action with
whoever holds the one it steps onto. That is the idiom every other FOH row
already uses and what the footer already promised.
**The structural win: swapping keeps the table a PERMUTATION**, which retires
THREE of D13's clauses at once — protected primaries, hold-A clear, and
conflict detection — because **no action can ever be left on no button.**
Row 0 (d-pad) is selectable and refuses with `deny`.

**Sub-items:** (1) nine action rows bindable, `ctlRow` now spans eleven rows
(9 actions + style + reset); (2) `mod` row gone from the SCREEN, cell KEPT in
`ctl_style.c` for the BOX label table, the persisted record and A30(a) —
swapping shoulders is now a plain rebind; (3) **`rebind: N/A` was NOT
vestigial** — it was the screen saying out loud that D13's rebinder did not
exist, deleted because the thing it denied now exists; (4) `RESET TO DEFAULTS`
= identity binding + default style + ratified Mod, one A press.

**Mechanism, and its safety property.** `ctl_bind_apply()` permutes the polled
`PlatformInput` in `foh_dev.c`'s new `poll_bound()` — BEFORE the pause edge,
the system-menu edge, the S1 resolver and the raw-key sidecar. So `s1_input.h`,
the three chord tables and **every frozen S1 sweep never see the feature**;
under the identity default it is a struct copy, so **no recorded session and no
frozen stream moves.** The FOH **menu** loop deliberately keeps the RAW poll:
**you must always be able to reach this screen and undo a rebind.**

**Per-port** in the model and in `MLFKPERSIST5` (four `bind` rows); **UI on
port 0 only** — exactly the A33 re-amendment's instruction.

**Persistence: COMPLETE and round-tripping, not partial.** `MLFKPERSIST5` —
68 lines, 1602 bytes, four permutation-validated `bind` rows. Verified
host-side: save->load byte-for-byte; non-permutation and out-of-domain rows
REFUSED; and a **genuine v4 file migrates** (settings + target records +
identity bindings) and republishes as v5.

### TWO PRE-EXISTING BUGS, FOUND BY BUILDING THE ABOVE
1. **SETTINGS SILENTLY DID NOT SAVE ON THE PRODUCT BINARY.** `foh_dev.c`'s
   persist arm named **only** `options-gameplay`, so a Controls or Audio change
   reached SD **only if the player later B-exited an unrelated screen**.
   `foh_app.c` already carried the correct three-screen form AND a note
   explaining why — **the product binary never got it.** This is a real
   user-facing data-loss bug that no check covered.
2. **THE PERSIST VERSION GATES WERE ENUMERATED, NOT ORDERED.**
   `foh_persist.c` gated blocks with `fromVer == 0 || fromVer == 3`, so the v5
   bump silently dropped the `modonr` line for a v4 file and then **rejected a
   good save as corrupt.** Caught by the NEW v4-migration leg. All gates are now
   `>=` on one version number. **Class note: enumerated version gates are a
   latent bug that only fires on the NEXT bump — grep for others.**

**Check:** `bash port/foh/check-rebind.sh` -> `REBIND TOOTH OK`. Overdraw
instrument; every screen claim bound to the PLAY PATH through the real
`foh_tick`. **Five negatives, and the T2/T3 PAIR is the transferable idea:**
T2 makes the screen ignore the binding (labels lie, buttons right); T3 makes
`ctl_bind_apply` an identity copy (labels right, buttons lie). **Each must fail
ALONE, so a half-wired build cannot pass from either side.**

**`check-controls-labels.sh` was updated, NOT weakened; T2 intact.** Two grammar
pins referenced `const int yRow[2] = {176, 190};` and its `foh_text` line, which
no longer exist (eleven rows now). The pins follow the lines carrying the SAME
coordinates (`yStyle = 176`), so the style row is still asserted at x=16/y=176.
The witness gained nine DOWNs to walk to the style row (it used to open there).
T1-T4 untouched.

### ⚠ BLOCKER — DEVICE LEG OWED (driver's, not the lane's)
**`check-device-persist.sh` was UPDATED for v5 but NEVER RUN** (FunKey
disconnected). Changes: `PERSIST_BYTES` 1510->1602, 64->68 lines, positional
whitelist gained the four `bind` rows with a permutation check,
unsupported-version tooth moved v5->v6, shared post-migration expectation gained
`v5_defaults()`.
**Mitigation already done:** `verify_persist_file` was extracted and run
standalone against a real v5 record (pass) and FOUR perturbed copies —
duplicate slot, wrong port order, dropped row, v4 header — **all four
rejected.** So the format-sensitive half IS verified; what is owed is the
DEVICE half: `DEVICE FOH OK` and the reboot round trip.

### A24c — DONE 2026-08-23 (D27). The chooser is collapsed BEHIND A ONE-DIGIT FLAG.

Owner: *"collapse for now - make it easily revertable though please."*

**`#define FOH_CTL_CHOOSER 0`** — `port/foh/foh.h:422`, sited immediately after
`FOH_NETPLAY` with a rationale block in C5's own style. **THE ENTIRE REVERT IS
CHANGING THAT ONE DIGIT TO `1`.** Everything keys off it: `foh.c`'s two routing
arms, `foh_snd_witness.c`'s Options-row case, `foh_rebind_witness.c`'s
navigation, the judge's second build profile, and the witness's own assertions.
**Nothing deleted** — `FOH_MENU_CONTROLS`, `FOH_CTRL_PAD`, `render_ctrl_pad`,
`kMenuText[3]`, the chooser's A ternary and its B-back edge all still compile at
either value.

**BOTH CONFIGURATIONS ARE TESTED — that is what makes the revert real.**
`check-controls-labels.sh` now runs 10 legs: CFG-0 asserts the collapsed route
end to end; **CFG-1 is derived by rewriting that one digit in a copy of
`foh.h`** and asserts the chooser route. The CFG-1 leg additionally reads the
configuration's OWN assertion line out of the output (flag-1 line present,
flag-0 absent) — **because a verdict alone does not say which build produced
it.**

**T2 SURVIVES AND NOW BITES TWICE** (leg [5] at CFG-0, leg [8b] at CFG-1), and
not by luck: the chooser's A ternary compiles at either flag value, so the
flag-0 witness SEATS the unreachable page directly (the `foh_snd_witness.c`
`battle-B` idiom) and exercises the same line the flag-1 build navigates to.
**Consequence worth naming: every D25 label claim stays live at flag 0, so
"nothing was deleted" is a MEASUREMENT, not a promise.** New T5 is T2's twin
for the collapsed arm.

**MEASURED GOTCHA — A FALSE-GREEN CLASS, CARRY THIS:** a quoted
`#include "foh.h"` resolves **from the including file's own directory BEFORE
any `-I`**. So shadowing a header via include path SILENTLY DOES NOTHING, and
the CFG-1 leg would have "passed" while actually testing the shipped
configuration — a false green in a test whose whole job is to prove the other
configuration works. Fixed by copying the whole `port/foh` half of the build
beside the derived header, every copy `cmp`-proved byte-identical.

**Consumers found by grepping, including one NOT on the driver's list** (third
time this session a handed-down site table proved incomplete):
`foh_rebind_witness.c:168`'s `goto_handheld` pressed A twice.
`foh_snd_witness.c:150` **moves sides in the sound table** — the collapsed row
is a `changeGamemode` leave, so ONE `menuForward`, not the menuMode change's
two. `judge-foh-trace.js` gained a second build profile parsed live out of the
header exactly as `FOH_NETPLAY` is (6 chooser edges -> `ctl`, 2 collapsed ->
`noctl`). `f04-nav` down to 10 transitions; shot total **19 -> 17**, re-pinned
in both inventories.

**Judge re-freeze integrity signal:** `judge-grammar.frozen.txt` moved its
`EDGES` / `DECISION_REGION` / `FILE` entries **but `ENFORCE_REGION` did NOT** —
i.e. the judging loop itself is byte-identical. That is the right shape for a
re-freeze: the data moved, the judge did not.

**Gamepad-port line corrected** (`foh_render.c:1979`): "THE FUNKEY-S HAS NO
GAMEPAD PORT" -> **"FUNKEY-OS SHIPS NO USB HOST MODE"**, same 32 chars so no
geometry moved, with A33 cited at the site. Fixed precisely BECAUSE the screen
is unreachable — a revert would otherwise resurrect a false claim.

**Green:** `CONTROLS LABELS OK` · `FOH FLOWS OK (flows=7 shots=17)` ·
`JUDGE REGRESSION OK` · `REBIND TOOTH OK` · `LEGIBILITY CHECK OK` ·
`MEXIT REENTRY OK`. Re-verified `CONTROLS LABELS OK` on the merged bytes.

### THREE OWED ITEMS FROM A24c — DRIVER'S, RECORDED SO THEY DO NOT ROT
1. **THE M4 FREEZE MANIFEST NOW HAS A STALE PIN.** `judge-foh-trace.js`
   changed, so its `reviewed-go` row in `port/sim/device/m4-freeze-manifest.txt`
   and the `CLOSURE` row in `m4-closure-ledger.txt` **no longer match the
   tree.** The lane correctly did NOT touch them (arc/review status is
   driver/owner territory). **M4 is owner-deferred, so this is not urgent — but
   it is now ONE MORE ROW to re-close when M4 resumes, on top of the 8.** Do not
   let it be discovered as a surprise then.
2. **`DEVICE FOH OK` not run** (device disconnected) — the outstanding leg for
   A24, A24c and A31 alike.
3. **`check-live-arms.sh` FAILS at T6** ("the unclamped copy exited 0"). The
   lane measured it as **PRE-EXISTING** by running it in a clean worktree at
   `989892f`. Driver is re-verifying independently. **STATE.md's own standing
   hazard applies: a stale red masks everything downstream of it.** If confirmed
   pre-existing it needs its own row, not a footnote.

### A39 (P1, NEW 2026-08-23) — `check-live-arms.sh` T6 IS VACUOUS. A TOOTH STOPPED BITING.

**Status: RED, and CONFIRMED PRE-EXISTING BY BISECT.** Lane M reported it while
doing A24c; the driver hypothesised **A37** (which changed `matchTimer`
behaviour, and T6 strips a `matchTimer` expiry clamp — a plausible suspect) and
**BISECTED. THE HYPOTHESIS WAS WRONG.**
- At `HEAD` (76eb3b3): `LIVE ARMS FAIL: T6: the unclamped copy exited 0`
- At `d717106` (**the commit immediately BEFORE A37 landed**), with deps copied
  in so the run is valid: **identical failure, identical output lines 55-56.**
**A37 is exonerated. So is A24c. The lane's "pre-existing" claim was correct.**
(Driver's first probe was INCONCLUSIVE — a bare worktree lacks `node_modules`
and died at the sim-data dump, a different error. It was re-run properly rather
than read as confirmation.)

**WHAT IS ACTUALLY BROKEN — read this carefully, it is NOT a crash.**
T6 (`port/foh/check-live-arms.sh:2122-2143`) is a **TOOTH**: it rebuilds a copy
with the HUD expiry clamp REMOVED (`const double mt = raw;`) and **requires
that copy to CRASH**. It is exiting **0** instead.
So the product is presumably fine — the clamp is still in place. **What has been
lost is the PROOF that the clamp is necessary.** Either (a) something else now
prevents the crash so the clamp is no longer load-bearing, or (b) the test's
crash-trigger conditions no longer occur. **Both mean the same thing: someone
could delete that clamp today and no check would object.**

**This is a COVERAGE HOLE, not a functional defect** — which is exactly why it
survived: nothing user-visible points at it. It is also a live instance of
STATE.md's standing hazard (*a stale red masks everything downstream of it*),
because `check-live-arms.sh` cannot go green for ANY reason while T6 is
vacuous, so it currently masks every other arm it tests.

**NEXT STEP:** bisect further back to find where T6 stopped biting, then decide
whether the clamp is still needed (fix the tooth) or genuinely redundant
(retire it with evidence). **Do NOT "fix" this by loosening T6** — HARD RULE 3.
A tooth that cannot bite must be repaired or retired on the record, never
relaxed.

## A33 — RUNG 1 RUN 2026-08-24. **§1.4 FLIPPED. THE HOST CONTROLLERS ARE LIVE.**

Device reconnected; rung 1 executed. **The spike's §1.4 guess — that
`&ehci0`/`&ohci0` were "inherited sunxi board-template boilerplate" — IS
WRONG.** Measured on the device:

```
/sys/bus/usb/devices/usb1   product "EHCI Host Controller"
                            driver ehci-platform, maxchild 1, speed 480
                            at /platform/soc/1c1a000.usb/
/sys/bus/usb/devices/usb2   product "Generic Platform OHCI controller"
                            driver ohci-platform, maxchild 1, speed 12
                            at /platform/soc/1c1a400.usb/
```
And the kernel log shows both **PROBED AND STARTED**, not merely declared:
```
ehci-platform 1c1a000.usb: EHCI Host Controller
ehci-platform 1c1a000.usb: new USB bus registered, assigned bus number 1
ehci-platform 1c1a000.usb: USB 2.0 started, EHCI 1.00
hub 1-0:1.0: USB hub found
ohci-platform 1c1a400.usb: ... assigned bus number 2
hub 2-0:1.0: USB hub found
usb_phy_generic ...: supply vcc not found, using dummy regulator
```

**THE CONSEQUENCE IS LARGE AND IT CHANGES THE WHOLE TICKET.** These are
**two live, bound USB HOST controllers with a root hub each, running RIGHT NOW
on the shipped image.** No fork, no reflash, no four line-edits. The spike's
entire NO-GO/UNKNOWN chain was reasoning about **MUSB at `1c19000`** — the OTG
block behind the micro-USB connector, confirmed `b_peripheral` this session
(which is exactly why `adb` works). **EHCI/OHCI at `1c1a000`/`1c1a400` are a
DIFFERENT HARDWARE BLOCK (V3s "USB1") and it is already in host mode.**

**WHAT IS NOW THE ONLY OPEN QUESTION — and it is PHYSICAL, not software:**
are USB1's D+/D- routed to anything reachable on the FunKey-S board? The
micro-USB is wired to MUSB (proven: adb). USB1's pins would have to surface as
pads, a test point, or an internal header. **That is a board question, settled
with a multimeter or a teardown photo, NOT from source.** Also note
`usb_phy_generic: supply vcc not found, using dummy regulator` — consistent
with the earlier finding that nothing can drive VBUS outward, which the
adapter's self-powered grey leg makes survivable.

**REVISED LADDER (replaces §7's):**
- ~~Rung 1~~ **DONE — and it flipped the answer.**
- **Rung 2 (now the decisive one):** attach a USB device through an OTG cable
  with the grey/power leg fed from a charger, and watch `dmesg` + `ls
  /sys/bus/usb/devices/`. If it enumerates on **bus 1 or 2**, host support
  exists today and A33 goes **GO with zero OS work**. If it enumerates nowhere,
  the micro-USB is MUSB-only and USB1 has no reachable connector.
- **Rung 3 (the fork) is now a FALLBACK, not the main path** — only needed if
  rung 2 shows USB1 is unreachable AND we want the OTG port converted.
- **The `adb`-over-peripheral risk shrinks accordingly:** using USB1 would not
  touch MUSB at all, so the verification rig is not endangered. That risk was
  the single largest cost in the fork plan and it may be avoidable entirely.

**A24 IS UNAFFECTED EITHER WAY** — the collapse sits behind `FOH_CTL_CHOOSER`
and reverts with one digit. But the REASON recorded against it ("no controller
can ever exist") is now **twice-retracted and materially weaker**; if rung 2
enumerates, the chooser should come back.

**METHOD NOTE — third correction to this one spike.** NO-GO -> retraction ->
UNKNOWN -> now §1.4 flipped, each time by MEASUREMENT beating a plausible
reading of source. The spike ITSELF nominated this command as the thing that
would settle §1.4 and told the driver to run it before retiring A24/A31/A32 on
its authority. **That instruction was correct and it paid.**

## A33 — SCHEMATIC ANSWER 2026-08-24: **ROUTED.** And "USB1" NEVER EXISTED.

Full document: `docs/research/gc-adapter-usb1-routing.md`. Answered entirely
from PUBLISHED SOURCES — the owner was right that this is open hardware and did
not need a multimeter.

**THE PREMISE (mine) WAS WRONG. There is no second USB block.**
`/sys/bus/usb/devices/usb1` is a Linux **BUS NUMBER**, not a port number. The
V3s has **exactly one USB port**. The EHCI/OHCI at `1c1a000`/`1c1a400` are the
**HOST-SIDE CONTROLLERS OF THAT SAME PORT**, sharing the same two pins as the
micro-USB connector. **The host controller's D+/D- ARE J2's D+/D-.**
**Nothing to solder. No unpopulated footprint. An OTG cable already reaches
them physically; ONE PHY ROUTING BIT gates them logically.**

**Decisive citations, two sides of the seam, locking together:**
- **Allwinner V3s Datasheet V1.0** memory map names `0x01C1A000`
  **`USB OTG_EHCI0/OHCI0`** — the host controllers *of the OTG port* — and the
  feature list says "**One** USB 2.0 OTG controller with integrated PHY". Pin
  list: 110 `USB-DM`, 111 `USB-DP`; the pin-description table has only
  `USB-DP0`/`USB-DM0`; **zero occurrences of `DP1`/`DM1` in the entire
  document.** (The PDF ships in FunKey's OWN hardware repo,
  `FunKey-S-Hardware/Datasheets/`.)
- **FunKey Rev E KiCad netlist** (`FunKey.net`), verbatim: `/USB_P` = `U3`
  pin 111 + `J2` pin 3; `/USB_N` = `U3` pin 110 + `J2` pin 2. **Three USB nets
  on the whole board.**

**THE MECHANISM — this is why nothing enumerates today.** `phy-sun4i-usb.c`:
V3s sets `.phy0_dual_route = true`, and `sun4i_usb_phy0_reroute()` **clears**
`OTGCTL_ROUTE_MUSB` (`BIT(0)` of `REG_PHY_OTGCTL`) for host, **sets** it for
peripheral. `dr_mode = "peripheral"` -> `id_det = 1` -> routed to MUSB. **One
bit. That is the entire blocker.**

### THREE MORE CORRECTIONS — one of them to the DRIVER'S OWN reading
1. The prior spike's §1.3 "boilerplate ... root hubs that can never enumerate"
   is **WRONG**. Real controllers, real port (`maxchild 1` **is** J2), signal
   path merely switched away.
2. §1.4's *"`dr_mode = "host"` (or `"otg"`)"* — **the parenthetical is wrong.**
   With no `id-det-gpio`, `"otg"` hits an explicit `/* Fallback to peripheral
   mode */` and routes to MUSB. **Only the literal `"host"` works.**
3. **THE DRIVER MIS-READ A LOG LINE 2026-08-24.** I cited
   `usb_phy_generic ...: supply vcc not found, using dummy regulator` as
   evidence about the host controllers. **It is MUSB's**, from
   `drivers/usb/musb/sunxi.c:779`. It says **nothing** about host-side PHY
   state. Sixth premise falsified by measurement this session — and the third
   of mine.

**Fork gotcha, recorded for whoever attempts it:** `ehci0`/`ohci0` carry **no
`phys` phandle**, so EHCI can never init the PHY itself — **MUSB must still
probe successfully for the reroute to happen. Deleting MUSB would kill host
mode too.**

### THE TRADEOFF THE OWNER MUST DECIDE (driver inference — VERIFY BEFORE ACTING)
**`adb` runs over MUSB in gadget mode.** If `dr_mode` becomes literal `"host"`,
the PHY routes away from MUSB, so **`adb` — and with it every device check,
every gate leg and the whole verification rig — very likely stops working on
that image.** Runtime switching is NOT available: it needs ID detection, and
R4 is deliberately unmounted. **On a ONE-PORT device this reads as
controller-OR-adb, not both.** LABELLED INFERENCE from the mechanism above,
not measured — **confirm on a throwaway SD before any real work.**

### STILL OPEN (unchanged, and no rows close on this)
**VBUS.** The board cannot source it; whether a sunxi host port that never
asserts VBUS will still enable and enumerate a **self-powered** device is a
driver-behaviour question no source settles. **That is what a fork actually
tests.** Latency (§4) unchanged. **A24/A31/A32 stay as they are.**

One fetch failed and is flagged rather than papered over: `linux-sunxi.org/V3s`
-> HTTP 403, wanted only as corroboration for a fact already established three
independent ways.

## A25b + A3 — **ROOT CAUSE MEASURED ON DEVICE 2026-08-24. L EMITS THE WRONG KEYCODE.**

Owner pressed L then R while the driver captured `/dev/input/event0` (the single
`fkgpiod` virtual node — `/proc/bus/input/devices` shows exactly one, handlers
`kbd event0`). Decoded 16-byte `input_event` records:
```
KEY_M (code 50) PRESS/REPEAT/RELEASE   <- the physical L SHOULDER
KEY_N (code 49) PRESS/RELEASE          <- the physical R SHOULDER
```
**`port/foh/keymap-frozen.txt` says `map l K k`. The button emits `M`/`m`.**
R's `map r N n` is CORRECT, which is exactly why R worked and L did not.

**This closes the diagnosis lane I opened.** Everything downstream of the
keycode was already proven green ON DEVICE by `f07-target-t02` (it injects `K`
through fk_input -> uinput -> SDL -> `platform_poll` and asserts the frozen
`p1char 4` wrap), and host-side by running the real translation arm. The fault
was always the FIRST hop, and it is a one-token pin error.

**`m` IS FREE** — the frozen keymap's twelve rows use
`u d l r a b x y s k n q`; no `m`. **No collision; the fix is `map l M m`.**

**WHY NO CHECK COULD EVER HAVE CAUGHT IT** (measured earlier by lane I, now
confirmed): every device check injects through **our OWN uinput node**, while
the physical buttons come from **`fkgpiod`**, which the rig's own quiesce
bracket STOPS for the duration of a device run. **The rig is structurally blind
to physical-button -> keycode for EVERY button.** And the `map l K k` row was
never measured on this device — its provenance is donor archaeology from
ssb64 (`docs/research/funkey-envelope.md:77`). Of the twelve letters, `k` was
the ONE with no independent confirmation, and it is precisely the one that was
wrong.

### THE FIX IS ONE TOKEN — BUT `keymap-frozen.txt` IS A FROZEN ARTIFACT
Do NOT drive-by edit it. Per lane I's own analysis, changing that row is a
**reviewed change with a PAIRED RE-MEASURE**: `foh_dev --dump-keymap` is
`cmp`'d against the file by `check-device-foh.sh`, and the **keymap-swap teeth
at `check-device-foh.sh:695` / `:721` and the `T-devswap` device tooth at `:51`
are all in the blast radius** (the `f07` fks derivation and both `NAV_LINK`s are
NOT). **A3 rides this same fix** — it is one defect at two call sites, and once
L arrives it becomes a real shield button in the default NATURAL style
(`s1_input.h:165-183`, `*shield = (p->l || p->r)`), so the blast radius includes
the MATCH, not just the menus.

### A27 — DONE 2026-08-24 (lane M). THE RIBBON IS LIVE, AND IT REACHES THE SIM.

**Owner symptom closed:** *"there's no way to change between stock mode and
'endless ko fest'. If you click the 'VS Melee' in the CSS it should change
modes."* It now does, on upstream's own trigger.

**What A23 had already provided, and what it had not.** A23 built the CSS
header's *first* hit test (the BACK wedge) and the D4 constants idiom that
makes a hit region and a drawn extent the same numbers. That idiom is what
A27 reused; the *plumbing* was not shared, because the BACK wedge's arm runs
BEFORE the hand integrates (upstream's css.js:186, deliberate) while the
ribbon's runs AFTER it (css.js:389) — two different positions in the same
function. So "most of the work already exists" was half true: the pattern
did, the code did not, and the ribbon's own arm is 6 lines.

**Upstream carried verbatim, trigger and all.** `css.js:389-394` is an A
RISING EDGE inside a rect that plays `menuSelect` and calls
`setVersusMode(1 - versusMode)` — binary, no picker, no cycle — sitting
between the port-type boxes (:348) and the CPU-knob grab (:396) inside the
same "hand outside the roster band" arm. Ours sits in the same place for the
same reason: a hand carrying a token cannot toggle the mode here, exactly as
upstream.

**The RECT is ours, and that is the one deviation (D28).** Upstream's is
`y > 100 && y < 160 && x > 380 && x < 910` on a 1200x750 canvas, i.e. a band
BELOW its header, because upstream draws this blurb as loose 1.25x text with
no plate. This FOH's whole silver header is 26 px, so upstream's ratio maps
to y 32..51 — under our header, on the roster row. The BACK wedge's ratio
LANDED on its drawn extent (920/1200 * 240 = 184 = the slab's left edge) and
was used for that reason; this one does not, so the DRAWN EXTENT wins, which
is what D4 actually says. `FOH_CSS_MODE_{X0,X1,Y0,Y1,CAP}` now build the
plate in `css_header` AND hit-test it in `step_css` — one source, so D4 is
mechanically true rather than a comment in two files.

**The LABEL tells the truth, and costs zero re-freezes.** Upstream's own
strings are `"An endless KO fest!"` / `"4-man survival test!"`
(css.js:715-721) — 19 and 20 characters against a 74 px plate that holds 12
glyphs of the 5x7 face. ENDLESS takes upstream's words over two rows
(`ENDLESS` / `KO FEST!`, only the article dropped); STOCK keeps `VS. MELEE`,
the gamemode's own name, because "4-MAN" is a lie on a two-port build (D6)
and because the default state then renders **the bytes it always rendered**.
That is the A23 bar's `bHold > 0` argument reused: every judged CSS shot is
taken in stock mode, so no frozen shot moves — asserted by cmp against a
build whose label is unconditional, not by assertion.

**THE MODE REACHES THE SIMULATION — this is the half that makes it not a
stub.** `G.sim.versusMode = foh.versusMode;` immediately BEFORE
`sim_setup_match` in **both** `foh_app.c` (product) and `foh_dev.c` (rig),
per lane S's measurement, because `startGame` READS it (main.js:1334) to put
every player on 1 stock. The LAUNCH line's hardcoded `versus=0` became
`versus=%d` in both binaries: a launch record that says 0 while launching
endless is a hardcoded output standing in for a real value.

**Check:** `bash port/foh/check-css-mode.sh` -> `CSS MODE CHECK OK (3 teeth)`.
It is two checks in one because A27 has two independent failure modes:
`port/foh/foh_cssmode_witness.c` drives the REAL `foh_tick` and photographs
the REAL `foh_render` (toggle, toggle back, page-state survival across a
match round trip, A inert on the neighbouring BACK wedge), and then a leg
launches a REAL g01 match through `foh_app --bridge verify` and reads the
answer out of the SIM's own checksum stream. **Three teeth, each failing
ALONE** (the A31 T2/T3 pair, extended): T1 makes the label ignore the mode,
T2 makes the hit test write nothing, **T3 moves the bridge line ONE LINE
past `sim_setup_match`** — the exact mistake lane S warned about — and
watches the endless match collapse back onto the 4-stock stream.

**Measured, quotable:** the control leg's frame-1 hash is
`9f4c6df778506d64…`, which is CHECKSUM.md §5's own g01 anchor, and its LAUNCH
record is byte-equal to the frozen `f01-vs-g01.expect` one. The ribbon leg's
frame 1 is `97189d261d22d3a4…`. One A press on a menu widget, measured in the
simulation's checksum.

**Grammar widened, not loosened:** `judge-foh-trace.js` and
`normalize-foh-trace.js` take `versus=([01])` because the produced domain
really did widen today. `check-foh-flows.sh`'s `LAUNCH_RE` is UNTOUCHED — it
reads only the frozen `.expect` files, where 0 is still exactly right, so the
tightness moved to where the value is genuinely fixed.

### A39 — ANSWERED 2026-08-24 (lane M). REPAIRED, NOT RETIRED.

**WHERE IT STOPPED BITING: commit `844b8a6` — "A14: widen the browser glyph
atlas to the measured menu domain (43 -> 179 records)", 2026-08-05.** Not
A37, not A24c, and not anything near them; the driver's bisect only had to
keep going.

**WHY, mechanically.** T6 removes the HUD expiry clamp and required the copy
to ABORT with `glyphs: font 0 has no glyph '-'`. The unclamped finish frame
carries a matchTimer of -0.00004, so the minutes string is `"-1"` and the
seconds are `"-0.00"` — a '-' reaches `gfx_glyph_text(GFX_FONT_T40, …)`,
which is **font 0**. A14 gave specs 0 and 3 `MENU_CHARS`, which contains
`-`; `port/gfx/vfxglyphs-frozen.txt` has carried `GLYPH 0 45` ever since
(measured: 0 occurrences at `bdc4781`, 1 at HEAD). The crash trigger was
deleted by a change about MENU TEXT, which had no way to know it was standing
on this tooth.

**IS THE CLAMP STILL NECESSARY? YES — and its failure mode moved.** Without
it the HUD no longer dies; it silently DRAWS `-1:-0` on the finish frame of
every timed-out match instead of `00:00`. The clamp's actual argument never
depended on the atlas: upstream never renders that frame at all
(`main.js:1243`'s `playing` guard vs `finishGame`'s `playing = false`), so
keeping the text inside the domain upstream draws is still the faithful
answer. A loud abort became a quiet wrong clock — strictly worse to lose.

**THE REPAIR (no assertion relaxed).** T6 now requires the unclamped copy to
COMPLETE (`expect_ok`), to run the SAME match to the SAME expiry frame
(`assert_same_finish`, the rig's own helper), and its `finish-banner.ppm` to
**DIFFER** from ARM B's. That shot is photographed after `gfx_render_frame`
drew the HUD, and the banner is a centred TIME! at y 106..133 while the timer
sits at the top — they do not overlap, so the timer's text is in the picture.
Deleting the clamp is still caught; only the instrument changed. If a future
change ever re-narrows font 0, the copy dies on the missing-glyph path and
`expect_ok` fails LOUDLY rather than the tooth quietly changing meaning.

**A SECOND PRE-EXISTING DEFECT, found by running the rig rather than reading
it.** `check-live-arms.sh`'s no-commit fingerprint could not run at all in a
tree holding an untracked NON-executable file: it built the mode list with
`printf "- %s\n" "$f"`, and bash reads a format beginning with `-` as an
OPTION (`printf: - : invalid option`), so the guard failed CLOSED before any
evidence existed. Every lane that adds a new `.c` beside its check hits it.
Fixed with `printf --`. **Class note: any `printf` whose format may begin
with `-` needs `--`; grep for others before trusting a shell instrument.**

**STANDING LESSON (the transferable half).** A tooth that asserts a
*diagnostic string* is hostage to every unrelated change that can delete that
string. T6 asserted an ABORT MESSAGE; a font-coverage commit five weeks
earlier retired it silently, and nothing was user-visible because nothing was
broken — only unguarded. The repaired form asserts a DIFFERENCE IN OUTPUT,
which no third party can quietly satisfy.

**VERDICT, on a quiet tree:** `bash port/foh/check-live-arms.sh` →
**`LIVE ARMS OK (sysmenu=4 vsfinish=1 drains=3 teeth=15)`**, exit 0 — the
first green this rig has produced since the tooth broke, which also lifts the
masking: the other 14 teeth and every arm it tests are now verified rather
than merely unreported. T6's own line reads *"the unclamped copy reaches frame
210 and draws a DIFFERENT finish frame"*. (A first run was green on all 15
teeth and then failed its no-commit fingerprint because this lane committed
A27 while it ran — the rig's own message calls that case out and says nothing
above it is invalidated; it was re-run clean rather than argued with.)

### A27 — DONE 2026-08-24 (D28). THE MODE RIBBON REACHES THE SIM, PROVEN BY CHECKSUM.

Owner's ask — *"if you click the 'VS Melee' in the CSS it should change modes"*
— is live. Stock <-> endless, `setVersusMode(1 - versusMode)` at upstream's own
site (`css.js:389-394`).

**A23 gave the IDIOM, not the code** — a useful correction to the driver's
"most of your work may already exist". A23's BACK arm runs **before** the hand
integrates (`css.js:186`); the ribbon's runs **after** it (`css.js:389`).
Different positions in the same function. What was reused is the D4 constants
pattern: `FOH_CSS_MODE_{X0,X1,Y0,Y1,CAP}` build the plate in `css_header` AND
hit-test it in `step_css`, one source. The arm is 6 lines, placed inside the
"hand outside the roster band" branch **so a hand carrying a token cannot
toggle the mode** — upstream's own placement.

**D28 is a GEOMETRIC deviation only:** upstream's `y in (100,160)` maps to
y 32..51 here — *under* our 26 px header, on the roster row. The BACK wedge
could use upstream's ratio only because 920/1200x240 = 184 happened to land on
its drawn extent; this one does not, so **the drawn extent wins, which is what
D4 says.**

**PROOF IT IS NOT A STUB — the strongest done-check yet.**
`bash port/foh/check-css-mode.sh` -> `CSS MODE CHECK OK (3 teeth)`. It launches
a REAL g01 match through `foh_app --bridge verify` and **reads the answer out of
the sim's own checksum stream**: stock frame-1 = `9f4c6df7…` (**CHECKSUM.md's
own g01 anchor**, and its LAUNCH record byte-equal to the frozen `f01` one);
endless frame-1 = `97189d26…`. The two LAUNCH records are compared
field-for-field **so the hash difference is ATTRIBUTABLE to the mode**, not
merely coincident with it. **T3 moves the bridge write ONE LINE past
`sim_setup_match` and the endless match collapses back to 4 stocks** — i.e. it
tests exactly the ordering constraint lane S warned about.

**Label:** endless carries upstream's own words on two rows (`ENDLESS` /
`KO FEST!`); **stock keeps `VS. MELEE` under duress, not by preference** —
every judged CSS shot is taken in stock mode and the device was offline, so
nothing could be re-frozen. The check proves the stock frame is **byte-identical**
by cmp against a build whose label is unconditional. **OWNER DECISION AVAILABLE:
if stock should read `STOCK` or `4-MAN`, that is a re-freeze call, not a code
change.**

### A39 — REPAIRED 2026-08-24. AND THE CAUSE IS A LESSON, NOT A BUG.

**Where the tooth stopped biting: commit `844b8a6`, "A14: widen the browser
glyph atlas (43 -> 179 records)", 2026-08-05.** T6 required the unclamped build
to abort with `glyphs: font 0 has no glyph '-'`. **A14 gave atlas specs 0 and 3
`MENU_CHARS`, which contains `-`.** Measured: `GLYPH 0 45` occurs **0** times at
`bdc4781` and **1** from `844b8a6` on. **A change about MENU TEXT silently
deleted a SIM-HUD tooth's trigger.**

**Is the clamp still necessary? YES — repaired, not retired.** Without it the
HUD no longer dies; it **silently draws `-1:-0` instead of `00:00` on the finish
frame of every timed-out match.** The clamp's argument never depended on the
atlas (upstream never renders that frame — `main.js:1243`'s `playing` guard vs
`finishGame`'s `playing = false`). **A loud abort became a quiet wrong clock:
strictly worse to lose.**

**The repair asserts OUTPUT, not a diagnostic string** — the copy must complete,
reach the same expiry frame, and its `finish-banner.ppm` must DIFFER from arm
B's. Nothing relaxed.

**⚠ THE STANDING LESSON (class-level, carry it):** **a tooth that asserts a
DIAGNOSTIC STRING is hostage to any unrelated change that can delete that
string.** Assert the OUTPUT the clamp protects, never the error text it happens
to produce today.

**Second pre-existing defect, found by running the rig:** its no-commit
fingerprint could not run AT ALL in a tree holding an untracked non-executable
file — `printf "- %s\n"` makes bash read the format as an OPTION. **Every lane
that adds a new `.c` beside its check hits this.** Fixed with `printf --`.

**`LIVE ARMS OK (sysmenu=4 vsfinish=1 drains=3 teeth=15)`, exit 0 — the first
green since the tooth broke, which also LIFTS THE MASKING on the other 14
teeth.** A39 closes.

## A28 — DISCRIMINATOR RUN 2026-08-24. **THE BUZZ IS OURS. IT IS ANALOG, NOT DIGITAL.**

Owner ran the cheapest discriminator: `sndpack.bin` moved aside so
`mlfk-foh.sh` omits `--sndpack` and `platform_audio_start` is **never reached**
(`foh_dev.c:1928-1935`). Owner's verdict, verbatim: **"no buzz"**. File
restored immediately.

**So the chain is now pinned at both ends:**
- Audio device NEVER OPENED -> **silent** (owner, this session)
- Audio device OPEN -> **buzzes continuously**, before any sound plays
- The samples we feed it are **bit-exact zero** — proven by
  `check-snd-idle.sh` (7 idle states, poisoned buffer, `SND IDLE SILENT`)

**Therefore the noise enters AFTER the sample stream. It is analog, introduced
by the act of opening the device.** Hypothesis 1 of the A28 row (the codec/amp
being energised by `SDL_OpenAudio` and staying energised for the process
lifetime) is the surviving one.

**MIXER ROUTING IS CLEAN — the classic cause is RULED OUT.** Measured on
device (`amixer`, V3s internal codec, card 0 `V3s_Audio_Codec`):
```
Headphone         49/63  [-14.00dB]  [on]
Headphone Source  'DAC'                      <- NOT a mic/line path
Mic1              [off]  Capture [off]       <- muted both ways
Mixer             Capture [off]              <- no loopback
Mixer Reversed    Capture [off]
DAC               51/63  [-13.92dB]  [on]
```
An unmuted mic routed into the output mixer — the usual suspect for exactly
this symptom — **is not what is happening.** `Mic1 Boost` sits at 33 dB but
`Mic1` is off, so it is inert.

`/proc/asound/pcm` is a single device (`CDC PCM Codec-0`), `hw_params` reads
`closed` at idle — confirming nothing holds the codec open when the game is not
running.

### NEXT — THE OURS-vs-PLATFORM DISCRIMINATOR (owner action, 30 s)
**Launch a DIFFERENT audio app and listen** — `ssb64.opk` is already installed
at `/mnt/Applications/`. If ssb64 buzzes too, the buzz is a **platform trait of
opening the V3s codec on this hardware** and A28 closes as
NOT-OUR-DEFECT (with a possible mitigation, below). If ssb64 is clean, the
difference is in HOW WE OPEN IT — rate, format, period size — and that is
fixable.

**If it proves to be a platform trait, the mitigation to evaluate is analog
gain:** `Headphone` and `DAC` both sit near 80% with ~-14 dB. Amp-generated
noise scales with amp gain while our digital signal does not have to — lowering
analog gain and raising digital level trades headroom for a lower audible noise
floor. **Measure before adopting; do not assume the direction.**

### A28 — 2026-08-24, SECOND DISCRIMINATOR: **NOT A PLATFORM TRAIT.**

Owner: *"ssb64 has no audio not sure why... i launched super mario 64 and it's
fine."* **SM64 opens the same V3s codec on the same device and plays cleanly.**
(ssb64 was inconclusive — it produced no audio at all, so it never opened the
device; SM64 is the valid comparator.)

**So the buzz is not "what this hardware does when you open its codec".
Something about HOW WE OPEN IT differs.** Combined with the first
discriminator, A28 is now bounded on three sides: our samples are bit-exact
zero, the mixer routing is clean, and another app drives the same codec without
buzzing.

**LEADING HYPOTHESIS, and it is the one our instrumentation CANNOT SEE:**
we open **44100 / S16LSB / 2ch / 512 frames** — an 11.6 ms period. A constant
train of DMA xruns at that period would be audible exactly as a continuous
buzz, and **`platform_audio_sdl.h:44-51` documents that our `underruns` counter
is STRUCTURALLY BLIND to them** ("DMA-XRUN BLINDNESS": an xrun makes the SDL
write return SOONER, so inter-callback gaps only shrink and the counter stays
0). **Its green reading has never been evidence about this.**

Second candidate: `docs/research/audio-spike.md:31-34` records the device
granting **22050 stereo exactly**, while we demand 44100 with an
exact-spec-or-fail check. Worth reconciling (A35 already registers the
2x-upsampling observation and the unresolved tension there).

**NEXT MEASUREMENT (driver, needs the game running):** read
`/proc/asound/card0/pcm0p/sub0/hw_params` and `.../status` while the app holds
the device — that yields the ACTUAL negotiated rate/format/period AND the
kernel's own xrun count, which is the number our counter cannot produce.

### A28 — LIVE CODEC STATE MEASURED 2026-08-24 (game held the device)

```
hw_params:  access MMAP_INTERLEAVED · format S16_LE · channels 2
            rate 44100 (granted EXACTLY as requested)
            period_size 512 · buffer_size 1024      <- TWO PERIODS. THE MINIMUM.
sw_params:  start_threshold 1 · stop_threshold 1024
            silence_threshold 0 · silence_size 0    <- NO SILENCE-FILL ON UNDERRUN
status:     state RUNNING throughout · avail 144..448 · avail_max 768 then 560
```

**THE STRUCTURAL FINDING: `buffer_size == 2 x period_size`.** SDL asked for 512
frames and ALSA gave a 1024-frame buffer — the tightest double-buffer
possible, ~23 ms total. Sampled over 12 s the queue ran down to ~256 frames
(**5.8 ms of audio in hand**). SM64, which is clean on this same codec, will
almost certainly be running 4-8 periods.

**HONEST NEGATIVE RESULTS — do not let the hypothesis outrun them:** state was
`RUNNING` on every sample, never `XRUN`; `dmesg` shows no underrun complaints.
**So a constant xrun train is NOT yet demonstrated**, only a very tight buffer.
The xrun hypothesis remains the leading one but is **not proven**, and our own
counter cannot help (documented blind, `platform_audio_sdl.h:44-51`).

**A mechanism that fits ALL the evidence, including the negatives:**
`silence_size 0` means **ALSA does NOT write silence on underrun** — the DMA
re-plays whatever is already in the buffer. A short repeated fragment at an
11.6 ms period is ~86 Hz — audible as a continuous buzz — and brief enough
recoveries may never leave the stream in `XRUN` long enough for a sampled read
to catch it. **UNPROVEN, but it explains silence-in, buzz-out with no visible
xrun.**

**CHEAPEST DECISIVE TEST: raise the buffer and listen.** `--audio-samples 1024`
or `2048` (`foh_dev.c:1555,1701`) gives a 2048/4096-frame buffer (46/93 ms).
The PLAY launcher does not pass the flag, so this needs either a `foh-args`
evidence run or a launcher change. **If the buzz vanishes, it is buffer-related
and the fix is a bigger request; if it survives, the cause is elsewhere and the
2-period finding is a real but separate defect.**

## A26 — ANSWERED 2026-08-24 BY THE OWNER. **HIBERNATE KILLS THE APP.**

Owner: *"hibernating when meleelight was running and then opening the funkey
again it starts at the home screen."*

**That settles the three-worlds question the row posed: it is NOT world 1.**
The kernel does not suspend-and-resume the process transparently — the app is
gone and the frontend is back. So A26 is world 2 (signalled, then killed) or
world 3 (killed outright, no signal), and **resume requires SAVING AND
RESTORING STATE**, not merely surviving a clock jump.

**NEXT, AND IT DECIDES THE WHOLE SHAPE:** determine whether the app receives a
SIGNAL before dying. If it does (SIGTERM/SIGUSR/SIGHUP), a handler can write a
resume record through `foh_persist` — cheap and reliable. If it does not, only
periodic checkpointing can work, and that has a frame-budget cost that must be
measured before it is designed. **Test: install a handler that logs to tmpfs,
hibernate, reopen, read the log.**

**Scope recommendation stands** (and the owner has not ruled on it):
**menu-level resume first** — restoring to the menu is cheap and is probably
all worlds 2/3 justify. **Mid-match resume is a separate, later row**: the sim
state is the whole player/physics plane and snapshotting it is a new
serialization surface with its own correctness bar.

## A28 — **ROOT CAUSE CONFIRMED 2026-08-24: BUFFER STARVATION.** And it was DERIVABLE.

Owner listened at `--audio-samples 2048` (period 2048 / buffer 4096, negotiated
and verified in `hw_params`): **"seems clean to me"**. At the shipped 512 the
same build buzzes. **A28's cause is settled.**

### THE SIZE WAS NEVER A GUESS — WE ALREADY HAD THE NUMBER
Owner asked whether we really have to guess. **No.** The requirement is a
deadline, and both sides of it were already measured in this repo:

- **SDL's `samples` IS the ALSA period.** The app must refill the buffer once
  per period or the DMA starves.
- **512 frames @ 44100 Hz = 11.61 ms.**
- **`docs/research/device-perf.md:34` — worst sim-only p99 = 10.684 ms (g08),
  "6.0 ms of the 16.67 ms frame" remaining.** That is the SIM ALONE, before
  render, present and the audio callback itself.

**So the shipped refill deadline (11.61 ms) is SHORTER THAN ONE FRAME BUDGET
(16.67 ms), and barely above the sim's p99 on its own.** Any frame that runs
long — which is every frame that renders anything — misses the deadline.
**It was structurally guaranteed to starve, and the proof needed no new
measurement.**

**Principled minimum: the period must exceed ONE WHOLE FRAME, with margin.**
| samples | period | vs 16.67 ms frame |
|---|---|---|
| 512 (shipped) | 11.61 ms | **0.70 frames — cannot work** |
| **1024** | 23.22 ms | 1.39 frames — first viable power of two |
| 2048 (tested clean) | 46.44 ms | 2.79 frames |

**Latency cost is real and is why this matters:** 2048 buys ~93 ms of buffer
(~5.6 frames of audio lag) in a fighting game. **1024 is the principled
choice** — but it is a PREDICTION and must be VERIFIED, not adopted.

### THE VERIFICATION INSTRUMENT ALREADY EXISTS (do not eyeball it)
`/proc/asound/card0/pcm0p/sub0/status` exposes **`avail_max`** — the high-water
mark of FREE space. **`avail_max` approaching `buffer_size` IS starvation**, and
it is a number, not an opinion. Measured this session:
- 512/1024: `avail_max` 768 -> only **5.8 ms of audio in hand** at worst
- 2048/4096: `avail_max` 2096 -> **45 ms in hand** at worst

**Turn this into a permanent check** (HARD RULE 8: instrument > one-off): run a
real MATCH (not the title screen — the title does not load the renderer) at the
candidate size and assert `avail_max` stays below a fraction of `buffer_size`.
That converts "does it sound OK to a human" into a gate. **The existing
`underruns` counter cannot do this and never could** — `platform_audio_sdl.h:44-51`
documents it blind to xruns by construction.

**OPEN:** the 1024 retest showed `hw_params: closed` — audio may have FAILED the
exact-spec check at that size (`platform_audio_sdl.h:120-150` demands
`granted.samples == requested`). **Unresolved — the device was power-cycled
mid-test.** If ALSA will not grant exactly 1024, that is a hard constraint and
2048 becomes the floor. **Re-test; do not assume either way.**

### ⚠ HARNESS DEFECT — MINE, AND IT COST THE OWNER HIS DEVICE
The test parked the frontend with `/mnt/disable_frontend`, **which lives on the
SD CARD and therefore SURVIVES A POWER CYCLE.** The owner's device powered off
mid-test and came back hanging at the splash screen with no frontend. Recovered
(marker removed, loop mount cleared, rebooted, `gmenu2x` running, all data
intact) — but **the fixture outlived the test, which is the defect.**
**BINDING FOR ANY FUTURE DEVICE TEST: remove the persistent marker BEFORE
launching the test binary, never after.** A crash at any instant must then
leave a bootable device. Cheap, and it makes the whole class impossible.

## A40 (P1, NEW 2026-08-24, owner-reported) — Marth shieldbreaker charge sound never stops

**Symptom:** *"when I do shieldbreaker with marth (neutral special) even if I do
a really quick one it plays the whole prolonged charging sound instead of just
the charging sound for as long as I'm charging for."*

**THE PLUMBING EXISTS — so this is a DIAGNOSIS row, not a build.** Measured:
- Upstream's own seam is `player.shieldBreakerID = sounds.shieldbreakercharge.play()`
  with `.stop(id)` as the `"shieldbreakercharge.stop"` token — recorded in the
  M2 task-11 marth cluster, replayed via `mv_howl_play_id`.
- **The mixer implements ID-ROUTED STOP** (`port/gfx/snd_mixer.h:25-30`):
  a `.stop`-suffixed token without an id deactivates ALL voices of the base
  name; **stops carrying a play id (`snd_event_stop_id`, fed by
  `ml_snd_stop_id_sink`) are ID-ROUTED — "exactly howler 2.0.12 stop(id)".**

**AND THE DIAGNOSTIC COUNTER ALREADY EXISTS:** `snd_mixer.h:328-334` keeps
`stops`, **`stopsMatched` and `stopsUnmatched`**, with the invariant
`stopsMatched + stopsUnmatched == stops`. **So the first question is answerable
by reading a number, not by guessing:**
1. Is a stop event emitted at all when the charge ends? (`stops` increments?)
2. If yes, does it MATCH a live voice? (**`stopsUnmatched` incrementing means
   the id is wrong or the voice already retired** — a very different bug from
   no-stop-at-all.)
3. Only if both are clean, look at whether the sim emits the stop at the right
   frame vs upstream's release arm.

**Do not "fix" this by truncating the sample or adding a timeout** — the
id-routed stop is upstream's actual mechanism and it is already built. Find
which of the three links is broken.

## A28 — **FIXED 2026-08-24.** Buffer default 512 -> 2048, with the derivation IN THE CODE.

`PLATFORM_AUDIO_SAMPLES_DEFAULT = 2048` at `port/gfx/platform.h:193`, consumed
by `gfx_app.c:571` and (driver one-liner, same day) `foh_dev.c`'s
`audioSamples` default — **which is the line the PLAY PATH actually reads, so
until it landed the buzz persisted in the OPK.** The definition site carries the
full derivation and the 512/1024/2048 frame-multiple table, so **nobody has to
guess this number again.**

**1024 was ESCALATED, not silently decided — and the reasoning is worth
keeping.** The mechanism argues 1024 *should* be granted (ALSA period-size
constraints are an interval plus at most a step, and any constraint admitting
both 512 and 2048 admits 2x512). **But that is an argument, not a measurement,
and the failure mode is TOTAL:** `platform_audio_sdl.h:120-150` demands exact
granting, so a refused size is `sim_fatal` and **the game does not launch at
all**. 512 and 2048 are both MEASURED-granted; 1024 never has been. **2048
ships as the safe floor; lowering it is a ONE-CONSTANT edit once the new
instrument reads green at 1024 on device.**

**THE INSTRUMENT (HARD RULE 8 discharged):**
`bash port/gfx/check-alsa-headroom.sh` -> `ALSA HEADROOM CHECK OK cases=12
default=2048`. `inHand = buffer_size - avail_max`, judged against the same
16.67 ms frame the period is derived from. **Its first two cases are the REAL
DEVICE MEASUREMENTS from this session** — 512/1024 (5.80 ms) **rejected**,
2048/4096 (45.35 ms) accepted — plus both sides of the bar, a
lucky-run-but-short-period rejection, and five "evidence that isn't evidence"
rejections (`closed`, missing/garbled `avail_max`, avail_max > buffer, buffer <
period). **Case [4/4] reads the constant out of `platform.h` and judges it, so a
future regression to 512 fails ON THE HOST.** A device leg later just `cat`s the
two proc files into the same judge.

**`mlfk-foh.sh` was correctly NOT touched** — it is **sha256-pinned in
`m4-freeze-manifest.txt`**, so adding a launcher flag would have broken the M4
gate. Fixing the code default was the right seam.

**Two device checks were silently wrong and are fixed:**
`check-device-audio.sh` and `check-device-music.sh` both PINNED
`AUDIO_SAMPLES=512` into their judge grammar **while passing no
`--audio-samples`** — pinning a number they never requested. They now request
it. **Registered gap: their cadence windows were measured at 512, so re-pinning
to the shipped size needs a device re-measurement.** `check-device-fullgame.sh`
also pins 512 but drives `foh_device`, so it is affected NOW that the one-liner
landed — **re-check it; it is freeze-pinned, so that is an arc, not an edit.**

## A40 — **ROOT-CAUSED 2026-08-24: LINK 2. TWO PLAY-ID COUNTERS WHERE UPSTREAM HAS ONE.**

Measured by running the real `snd_mixer.h` against the real `ml_events.c`
through `foh_dev.c`'s exact wiring, firing marth NSG's own calls
(`NEUTRALSPECIALGROUND.c:99-101` then `:67`):
```
menuPlays=0   simId=1001  voiceId=1001   stops=1 matched=1 unmatched=0
menuPlays=3   simId=1001  voiceId=1004   stops=1 matched=0 unmatched=1   <- voice still sounding
```
**The stop IS emitted (link 1 fine). It does NOT match (link 2 broken).
`voiceId` runs ahead of `simId` by EXACTLY the number of menu SFX played.**

**Cause:** `snd_mixer.h:626/653` advances `m->playCount` on **every**
`snd_event()`, while `ml_events.c:143` advances its counter **only** on
`ml_sound_play()`. `foh_dev.c:842-847` (`foh_snd` — the MENU-plane SFX
chokepoint) calls `snd_event()` directly, **so every title/CSS/SSS click skews
the mixer's ids.** `player.shieldBreakerID` then names a voice that does not
exist, the id-routed stop no-ops, and the **LOOPING** charge sample runs to the
end of the match. **`furaLoopID` has the same exposure.**

The header's claim that both planes "count the same event stream" was **a
PRECONDITION stated as a FACT**; `snd_mixer.h:36-55` now states it as a
precondition and names the violator.

**WHY IT SHIPPED — both fidelity rigs are blind to it BY CONSTRUCTION:**
`snd_render.c` and `snd_reference.js` drive the mixer from a **sim event stream
only**, so the two planes never share a counter there. A second instance of
this session's recurring class: *the check could not see the defect it appeared
to cover.*

### A40 — **FIXED 2026-08-24 (lane A). `snd_event_menu()` + the one caller.**
`snd_mixer.h`: `snd_event_gen(m, name, consumeId)` carries the body;
`snd_event()` (SIM plane) passes true, the new `snd_event_menu()` passes false —
no `playCount++`, voice id 0, an id `ml_howl_play_id` can never mint (its ids
start at 1001), so a menu voice is unreachable by id-routing and stoppable only
by a bare `<name>.stop`. `foh_dev.c`'s `foh_snd` calls it. **`static inline`, so
the uncalled-static `-Werror=unused-function` problem does not arise at all** and
`snd_idle_check.c` needed no `(void)` line. Allocation, stealing, `starts`,
`steals` and the bus snapshot are untouched, so `check-device-foh.sh`'s voice
starts/stops expectations are unchanged by construction.

**GATE: `bash port/gfx/check-snd-playid.sh` → `SND PLAYID OK cases=7
menuplays=22 stops=7`, exit 0.** It links the REAL `port/sim/ml_events.c` and
compiles the REAL `snd_mixer.h` through foh_dev.c's exact wiring, and asserts
the OWNER-VISIBLE outcome — `stopsUnmatched == 0` **and the mixer emits
bit-exact silence** after the sim's id-routed stop — over 7 cases at
menuPlays 0/1/3/11/0/5/2, covering **both** id holders (marth
`shieldbreakercharge`, puff `furaloop`). Measured: `simId == voiceId` at every
case (1001..1007) across 22 menu plays; pre-fix the same run gives
`simId=1002 voiceId=1003 unmatched=1` at the first menuPlays>0 case.
**Two orthogonal teeth, both required to fail, on DIFFERENT assertions:**
`--tooth-legacy` (menu plane back on `snd_event`, i.e. the defect) bites the id
assertion at case 2 while case 1 (menuPlays=0) still passes — so it proves the
MENU PLAY is what breaks routing; `--tooth-deaf` (a stop sink that bookkeeps a
perfectly MATCHED stop and silences nothing) bites only the audible assertion,
which is the half a counter check is structurally blind to. The script also
asserts `foh_dev.c` still calls `snd_event_menu`, so re-pointing the caller
cannot pass by leaving the checker's own copy of the wiring correct.

**Owed (device):** nothing new is device-specific, but the fix is in the play
path, so the standing device audio legs (`check-device-foh.sh`,
`check-device-audio.sh`) are owed a re-run on the next device window — expected
NO-OP (counters they pin are untouched).

## LANE I + DRIVER RE-PIN PASS — 2026-08-24

### A25b/A3 — LANDED, and **THE DRIVER'S PREMISE WAS HALF WRONG**
I briefed `map l K m`... as `map l M m`. **Wrong: the KEYSYM must change, the
FLOW LETTER must not.** `K` is hard-pinned in three places a rename breaks for
zero behavioural gain: `flow-to-fkscript.js:112`
(`KEYMAP_FLOW_LETTERS = "UDLRABXYSKNQ"` — it DIES if the frozen flow letter
differs), `foh_app.c:113` (`case 'K': in.l = true`, in the PRODUCT binary), and
`flows/f07-target-t02.flow:24` (`I 400 K`). **The shipped fix is `map l K m`**
— flow letter `K`, keysym `m` — and the injector resolves letter -> keysym
THROUGH the table, so all 7 flows now inject `m` automatically with zero `k`
left in any generated `.fks`. **Seventh premise falsified by running the code,
and the fourth of mine.**

**Found by grepping for index/duplicate consumers, not from my list:**
`platform_sdl2.c:82` held a SECOND, UNDOCUMENTED copy of the mapping
(`SDL_SCANCODE_K` -> `_M`; scancode space, cannot share the table, now named as
a duplicate). And `port/gfx/s1-session.script` needed 14x `k` -> `m` —
**behaviourally required**: the M3 live session injects L as a keysym and would
have gone SILENTLY DEAD. `s1_input.h` is byte-untouched, so the 15 pinned S1
checks are behaviourally identical.
**Stated loudly, as required: L is now a LIVE SHIELD BUTTON in the default
Natural style — this changes the MATCH, not just the menus.**

### A30(a) modOnR (D29) — LANDED, and it was NOT the one-liner I called it
`ctl_style.c:18` alone is **INERT**: every FOH binary calls `foh_persist_load()`
at boot, which runs `foh_persist_defaults()` and then
`ctl_mod_on_r_set(p.modOnR != 0)`. **Four companion lines were required and are
now landed by the driver:** `foh_persist.c:52` (the REAL fresh-install default),
`foh_persist.c:368` (v2/v3 migration), `foh.c:1198` (RESET-TO-DEFAULTS, which
would otherwise install the RETIRED arrangement), and TWO assertions in
`foh_rebind_witness.c` (`:484` RESET, `:663` cold boot).
**`check-rebind.sh` was RED on exactly those assertions and is now GREEN.**

### A30(b) — BLOCKED, and the OWNER HAS A SELF-CONFLICT TO RESOLVE
Lane I recommends the **DEFAULT-BINDING route, not a table change** — A31's
rebinder already ships, and the Controls screen renders `ctl_bind_get(0, …)`, so
a default row stays truthful automatically. The Natural row would be
`{3,0,2,1,4,5,6,7}`. **Three hard stops kept it unlanded:** the default lives in
`foh_persist.c:71` (and changing it moves an upgrading player's buttons under
him — a reviewed call); the table route would make `foh_ctl_labels.h:44-45` LIE
(it hardcodes ATTACK/SPECIAL and is pinned by `check-foh-flows.sh` leg `[0m]`);
and **BOX is a genuine STOP — `in.a`/`in.b` are NOT style-branched, so any edit
hits the pinned `s1_input_row()`, and X->grab is IMPOSSIBLE by permutation
because BOX never emits `in.z` at all.**

**MEASURED, and it shrinks the ticket: SHIELD + A *IS* A GRAB** — live, every
style, every character (`GUARD.c:76-79`, `GUARDON.c:101-104` dispatch `GRAB` on
a fresh A press while shielding). **So X->grab is a CONVENIENCE, not the only
way to grab, and the case for touching the ratified BOX table is much weaker
than it looked.**

**⚠ OWNER SELF-CONFLICT (his call, not the driver's):** fix_plan A30 records him
asking for **R = mod/tilt** (shipped as D29) *and* **R = C-stick HOLD** for
classic/natural. **R cannot be both.** Freeing Y for `special` in Classic
requires moving its C-layer to R, which in Natural/Classic also takes R off
shield duty — contradicting A3's whole "L and R both shield" premise. **One
permutation cannot serve both styles.**

### DRIVER BATCHED RE-PIN (§A-par.5) — done, and it uncovered a REAL objection
`judge-foh-trace.js` changed TWICE (A24c's `FOH_CTL_CHOOSER` profile, then
A27's `versus=[01]`) and only the first re-froze. Re-pinned its sha across **8
sites in 6 files**, and re-froze `judge-grammar.frozen.txt` via its own producer
(`dump-judge-grammar.js`), NOT by hand. **5 entries moved — and
`ENFORCE_REGION` moved for NEITHER file, i.e. the judging loop is
byte-identical and only the data it accepts changed.** That is the right shape
for a re-freeze.

**Then the check refused again — correctly, and at a deeper level.**
`judge-domains.authored.txt:179` authored `versus` as a FIXED LITERAL `0` on
the reasoning *"the FOH launches VS only; target tests use TLAUNCH"*. **That
reasoning conflated two axes**: VS-vs-target-test (genuinely TLAUNCH's job) with
stock-vs-endless (a property OF a VS match). **Widened to `0 1` WITH A
CITATION**, not to make a run pass: upstream's own binary toggle
(`main.js:140,237-239`; `css.js:393`), and the widening is EARNED — A37 proved
flag-off bit-identical across all 8 goldens and flag-on genuinely different,
and A27's check reads the delta out of the sim's own checksum stream with the
LAUNCH records compared field-for-field so it is ATTRIBUTABLE to this field.
`check-judge-regression.sh` and `check-foh-flows.sh` both GREEN.

### DEVICE LEGS THE DRIVER NOW OWES (all blocked — FunKey unavailable)
**Reviewed sha re-pins:** `keymap-frozen.txt` -> `452b6e41…255ee` at
`check-device-foh.sh:239`, `check-device-target.sh:147`,
`m4-freeze-manifest.txt:438`, `m4-closure-ledger.txt:136`;
`s1-session.script` -> `bac422f8…79cd2` at `m3-freeze-manifest.txt:52`.
**Runs to re-take:** `check-device-foh.sh` (dump/frozen `cmp`, KEYMAP1 grammar,
T12 keymap-swap teeth, T-devswap), `check-device-target.sh` (dump `cmp`),
`check-device-input.sh` + `judge-s1-coverage.js` (the S1 session now injects
`m`), `verify_m3.sh` leg 4 and `verify_m4.sh` (blocked on the pins above),
`check-device-audio.sh` / `check-device-music.sh` (cadence windows measured at
512, now shipping 2048), `check-device-fullgame.sh` (pins 512, affected NOW).
**AND THE ONE NOTHING IN THE RIG CAN DO: press the physical L.** The rig injects
through our own uinput while the buttons come from `fkgpiod`, which the quiesce
bracket stops — that blindness is now written into `docs/PORTABILITY.md`.

## A25c — DONE 2026-08-24 (D29). And the driver's spec was WRONG TWICE.

`port/foh/foh_hand.h` holds the ONE definition of the cursor's motion
(`foh_hand_step` — D1's full-deflection d-pad, D3's rescaled step, the clamp)
and the ONE hit predicate (`foh_hand_hit`), **whose strict comparisons ARE the
CSS's old `css_cell_at` gutter rule, so D4 now holds on BOTH screens by
construction.** Header-only ON PURPOSE: every `port/foh/` check and every
device rig carries an explicit TU list, so a new `.c` would mean editing all of
them including device ones nobody can run today.
**It retired THREE hand-kept copies of the slot geometry** (`foh.c`'s y-guard,
`foh_render.c`'s `overCells` line, `foh_legibility_witness.c`'s `slot_rect`)
rather than adding a fourth — the renderer and the hit test now read the same
tables.

**PROOF THE CSS IS UNCHANGED — the right answer to "is green enough?": NO.**
The lane said so explicitly: the existing CSS checks drive SCRIPTED gestures,
while what moved was a clamp, an integration order and a hit predicate —
properties of EVERY reachable (position, gesture) pair. So `check-hand.sh` leg
[4] is a DIFFERENTIAL: one driver built against the working tree AND against
the **pinned** pre-A25c commit `0e4375e` (pinned, not `HEAD` — `HEAD` becomes
this change the moment it lands and the leg would compare a build against
itself), both fed the same **60,000-frame** pseudorandom stream, dumping hand
bits, machine plane, sound queue, both token positions and a frame hash.
**Byte-identical**, with the sweep's own coverage asserted so it cannot pass by
sitting still.

**TWO DRIVER PREMISES FALSIFIED BY MEASUREMENT (8th and 9th this session):**
1. **`foh_look_canonical` must NOT pin the hand.** My spec said to pin it
   "exactly as it pins `tssTimer`". It pins **TICK-driven** look counters; the
   hand is **navigation-driven**, like `menuColours` and `cssHandX/Y`, neither
   of which it touches. **Pinning would destroy the shot's meaning.**
2. **`f06` needed no re-cut and `f07`'s frozen `.expect` is BYTE-UNCHANGED.**
   The hand re-homes at slot 0's centre on entry exactly as `tssCursor = 0`
   did.

`tssCursor` SURVIVES as the selection value — now WRITTEN by the hand and
**STICKY**, which is the CSS's own hover-selects rule and is load-bearing: a
non-sticky cursor leaves the gutter with no launch target and
`targetRecords[p1Char][tssCursor]` with no index.

### DRIVER FOLLOW-UPS (all mine, all landed this turn)
**D-NUMBER COLLISION: two lanes both claimed D29.** Renumbered A30(a) ->
**D30** across 9 sites in 6 files; A25c keeps D29 (it had 8 in-code references
to A30(a)'s 4). **Class note: parallel lanes cannot self-assign deviation
numbers — the driver must allocate them, or check at merge.**

**T17 WAS THE A39 CLASS, VERBATIM — AND IT WAS ALREADY RED BEFORE MY
RENUMBER** (verified by stashing and re-running, rather than assuming).
`check-foh-flows.sh` T17 injected a **ONE-FRAME DOWN TAP** to prove the
cursor is load-bearing. That was exactly right for the EDGE-driven grid cursor
D29 retired, and is a **no-op for the LEVEL-driven hand** (3.84 px/frame: a
1-frame tap moves 39.5 -> 43.34, still inside slot 0's y 30..49). **So the
tooth silently stopped biting.** REPAIRED, NOT RETIRED: the intent — "moving
the cursor changes which target launches" — is unchanged, re-expressed as a
**6-frame HELD run** (39.5 -> 62.54, inside slot 1), measured against
`foh_tss_slots` exactly as f07's own header records. **This is the SECOND tooth
this session disarmed by a mechanism change; the standing lesson generalises
from "diagnostic strings" to ANY tooth asserting a mechanism.**

**The snd_mirror caught its own widened include chain** — `foh.h` now includes
`foh_hand.h` and T29 died with `fatal error: 'foh_hand.h' file not found`.
**Working as designed**: its comment says "a new include breaks the build
loudly", and the explicit file list is what made it loud instead of silent.
Staged the new header.

`FOH FLOWS OK`, `HAND CHECK OK`, `LEGIBILITY`, `CSS MODE`, `JUDGE REGRESSION`,
`REBIND`, `CTL INPUT`, `ALSA HEADROOM`, `SND IDLE` — all green on merged bytes.

## A41 — CONTROL-SCHEME RE-RATIFICATION, DONE 2026-08-24 (D31/D32/D33)

**Owner ratification, verbatim:** *"L-only shielding is totally fine. I want it
in fact. i also want to be able to edit controls as I explained. as for box, if
it needs a custom menu, that's fine. do it. wtf you can't grab on box?? we want
to be able to"* — plus the earlier X->grab/A->jump/Y->special/B->attack and
"R should be mod / tilt" (shipped as D30).

**THE DRIVER OWES A CORRECTION FIRST.** I told the owner *"BOX never emits
`in.z` at all"* and let it read as **"you cannot grab on box."** **That is
FALSE.** `GUARD.c:75-78` / `GUARDON.c:101-104` dispatch `GRAB` on a fresh A
press while shielding, and that arm is **sim-side and STYLE-INDEPENDENT** — so
shield+A grabbed on BOX all along. What BOX lacked was a **DEDICATED** grab
button. **Tenth premise corrected this session; fifth of mine, and this one
reached the owner and shaped a ratification.**

- **D31 — L-ONLY SHIELDING (Natural/Classic).** `*shield = (p->l || p->r)` ->
  `*shield = p->l`. BOX untouched (already splits its shoulders). **This is the
  unlock**: it frees R, which frees Y, which makes D33 possible.
- **D32 — the C-LAYER MOVES FROM Y TO THE FREED R SHOULDER.**
- **D33 — the FACE PLANE, STYLE-INDEPENDENT, BOX INCLUDED.** The style branch
  in the button plane is DELETED: `in.x = p->a` (jump), `in.a = p->b` (attack),
  `in.b = p->y` (special), **`in.z = p->x` (GRAB — on every style, BOX
  included)**.

### SHOULD NATURAL GET A C-LAYER? YES — AND THE REASONING CORRECTS MINE
I flagged that a C-layer might contradict Natural's "no modifiers, 1:1"
identity. **Measured: that was never why Natural lacked a C-stick.**
`ctl_style.h`'s own text says Natural "gives up the single reachable
C-direction" because **all seven buttons were spent** once tap-jump was forced
off. D31 frees R, so the pressure is gone. **Decisive: without D32, D31 makes
Natural STRICTLY WORSE** — two shields became one and R would do nothing. **A
dead shoulder on the DEFAULT style is worse than a modifier.** Its 3 new rows
re-emit 1.0/1.0/0.7 already present in its own table; **no coordinate was
invented.**

### THE ARITHMETIC NOBODY HAD WRITTEN DOWN — AND THE 4th DECISION IT FORCES
**8 buttons − START(pause) − MENU(pause menu) = 6 gameplay buttons for 7 roles**
(attack, special, jump, grab, shield, Mod, C-layer). **Once grab is real, every
style must drop one.** Natural/Classic have no Mod -> 6 fit exactly. **BOX spent
R on Mod (D30, the owner's own ruling yesterday) -> the C-LAYER is what goes.**
My brief's "Y freed by (2)" does NOT hold on BOX, and the lane did not paper
over it: **BOX's 3 clayer rows are kept BYTE-EXACT AND UNREACHABLE** so a future
ruling costs no re-derivation.
**OWNER DECISION AVAILABLE: BOX now has no C-stick. It bought grab and kept Mod.
Reversible in one line if he would rather drop Mod.** No third option exists —
that is arithmetic, not preference.

### AND THE "OWNER SELF-CONFLICT" I RECORDED WAS NOT ONE
A30(b) recorded the owner asking for R=mod AND R=C-stick as a contradiction.
**It is not:** those rulings are about **DIFFERENT STYLES**, and `ctl_roles` is
**per-style**. R = Mod on BOX; R = C-layer on Natural/Classic. **I filed a
conflict that the code's own structure already resolved.**
What A30(b) *did* prove: **the A31 rebinder CANNOT do this** — `ctl_bind_apply`
permutes PHYSICAL buttons *before* the resolver, so it can never conjure an
`in.z` no style emits. **Grab on BOX was a table change or nothing.**

### PINNED S1 EXPECTATIONS THAT MOVED — 12 GROUPS, NONE WEAKENED
Highlights: `s1_sweep.c` checks 11-13 re-aimed BOX->CLASSIC (BOX has no cs
plane now; coordinates and the `ls neutral` tooth byte-identical); the
2048-combo button invariant now asserts the D33 form **on BOX *and* CLASSIC**
plus `in.z == p.x` — **strictly stronger, two styles where it was one**; the
sweep dump grew a 7th column for the live `in.z`; `judge-s1-coverage.js`
signatures went 24 -> 22 with **4 face-role sigs including GRAB**; its sidecar
pairing re-mapped to the D33 plane. **`s1-session.script`'s PRESSES are
byte-unchanged** (only comments moved), and `check-device-input.sh`'s exact
`S1 SWEEP OK (15 pinned chord checks, 2048 combos …)` line is **unchanged** —
no device script edited.

**VERIFIED ON MERGED BYTES BY THE DRIVER:** `SIM CONFORMS` **8/8** (the safety
net — the input plane feeds the sim and did NOT reach the checksum surface),
plus `CTL INPUT CHECK OK (8 teeth)`, `REBIND TOOTH OK`, `FOH FLOWS OK`,
`CONTROLS LABELS OK`, `HAND CHECK OK`. New teeth **t4-bothshields**,
**t5-clayeronY**, **t6-nograb**, **t7-facescramble**, each failing alone.

### DEVICE LEGS OWED (device unavailable; highest-risk first)
1. **`check-device-input.sh` + `judge-s1-coverage.js`** — the judge and the live
   session are **the only edits the lane could not execute**. Syntax-checked and
   every signature traced to a press >= 5 frames, but **only the device proves
   it.**
2. `verify_m3.sh` leg (4) — the live S1 three-way replay.
3. `verify_m4.sh` leg (2) — menu flows through the real input path (Controls
   shots are A==B/non-blank/distinct, NOT pixel-frozen, so **no re-freeze
   owed**).
4. `install-play-opk.sh` — reinstall so the device play build carries the new
   plane.

### TWO OWNER CALLS, NEITHER BLOCKING
1. **BOX has no C-stick now** (bought grab, kept Mod). One line to reverse.
2. **NATURAL and CLASSIC now differ by exactly ONE chord row** (Classic's
   dedicated shield-drop diagonal). Candidates for collapsing into one style.

## A42 (P0, owner-reported 2026-08-24) — **X->GRAB DOES NOTHING. `z` IS NOT GRAB IN THIS ENGINE.**

Owner: *"grab with X didn't work, nothing happens."* **He is right, and A41's D33
was built on a FALSE PREMISE that was sitting in the code as a comment.**

**MEASURED — every reader of `z` in the entire sim:**
- `port/sim/characters/*/moves/{FORWARDSMASH,UPSMASH,DOWNSMASH}.c`, all of the
  form `if (i0->a || i0->z)`.
- **THAT IS THE COMPLETE LIST. `z` DISPATCHES `GRAB` EXACTLY ZERO TIMES.**
  In this engine **`z` is an ALTERNATE SMASH-ATTACK button, nothing else.**

**THE FOUR REAL GRAB TRIGGERS, measured:**
| Site | Condition |
|---|---|
| `GUARD.c:75` | `i0->a && !i1->a` — **shield + A** |
| `GUARDON.c:101` | same |
| `DASH.c:80` | `i0->lA > 0 \|\| i0->rA > 0` — **ANALOG SHOULDER while dashing** |
| `RUN.c:60-61` | `a` edge **AND** analog shoulder |
| `KNEEBEND.c:66` | `a` edge **AND** analog shoulder |

**So grab is reached by SHIELD+A or by the ANALOG SHOULDER — never by `z`.**

**HOW THE ERROR PROPAGATED (this is the part worth keeping).** `s1_input.h`
carried the comment `// Z: grab (and lightshield-grab upstream)`. **That comment
is TRUE OF REAL MELEE and FALSE OF THIS ENGINE**, and it long predates A41.
I read it, believed it, briefed the lane with it, and the lane implemented
`in.z = p->x` faithfully — so D33 wired X to the SMASH-ALT role and called it
grab. **Nobody checked what the SIM does with `z`. A comment asserting engine
behaviour is not evidence about engine behaviour** — the same class as A39's
diagnostic-string tooth and the third time this session a claim in prose beat a
measurement it never had.
**And no check caught it** because `check-ctl-input.sh` asserts the RESOLVER's
output (`in.z` set from `p->x`) — which is correct — while nothing asserted that
`in.z` REACHES A GRAB. **The check tested the plane it owned and stopped at the
seam.**

### THE FIX — X must reach a REAL grab trigger, not `z`
Options, in order of faithfulness:
1. **X sets the ANALOG SHOULDER (`lA`)** — the dash/run/kneebend grab path, and
   upstream's own "lightshield-grab". Makes X a genuine grab in motion, but
   would also make X lightshield, which may be surprising.
2. **X sets `a` AND shield simultaneously** — synthesises the GUARD path. This
   is what a real Z-grab does in Melee (Z = A + lightshield).
3. **Keep `in.z = p->x` for the smash-alt role AND add a grab route.**
**Recommended: (2)** — it is what Melee's Z button IS, it reaches
`GUARD.c:75`/`KNEEBEND.c:66` which are the standing/jumpsquat grabs a player
expects, and it leaves the ratified chord coordinates alone.
**MUST BE MEASURED, NOT REASONED: build it, then assert an actual `GRAB`
actionState in the sim** — that is precisely the assertion whose absence let
D33 ship broken.

### LANDED 2026-08-24 (lane I, DEVIATION D34) — and the brief's own `z`
### inventory was ALSO short, which is the point
**Measured first, as instructed, and "that is the COMPLETE list" was not.**
The full reader set of `MlInput.z` in this tree is `{FORWARD,UP,DOWN}SMASH.c`
(`i0->a || i0->z`) **plus** `action_state_shortcuts.c:522` checkForAerials
(`(a edge) || (z edge)`) **plus** `physics.c:983` lCancelUpdate (`z` edge,
beside the analog triggers) — plus the AI plane. The CONCLUSION held (zero
`GRAB` dispatches; `z` is an alternate ATTACK button and an L-cancel
trigger), but the inventory did not. *A second-hand measurement is a
citation, not a measurement* — and this ticket exists because a citation was
believed once already.

**THE FIX (option 2, chosen and justified):** X emits `a` + `lA =
S1_ZGRAB_LA` (49/140 — the b0xx-mapping.md §2.2 light-shield level), i.e.
Melee's Z chord. It reaches ALL FIVE grab arms rather than only GUARD's,
because `WAIT.c:56` and `DASH.c:72` take their `lA>0||rA>0` arm into GUARDON,
whose `init -> main -> interrupt` chain runs inside the SAME tick and still
sees the `a` edge. Bounds are load-bearing: `> 0` or no grab arm fires;
`< 1` or `GUARDON.c:21` powershields on every press.
**Nothing was traded away:** every old `z` reader is an `a || z` form and
lCancel's third arm is an `lA` edge, so X keeps its alternate-attack and
L-cancel roles by construction; `z` is dropped rather than kept as a second
name for a bit `a` already sets.

**THE DELIVERABLE — `port/gfx/ctl_seam_witness.c`, leg [4] of
`check-ctl-input.sh`.** Physical button -> real `s1_input_row_style` -> REAL
`sim_game_tick` -> the actionState the engine enters.
**IT REPRODUCED THE OWNER'S BUG ON ITS FIRST RUN, BEFORE ANY FIX:**
```
PROBE style=0 A -> KNEEBEND     PROBE style=0 Y -> NEUTRALSPECIALGROUND
PROBE style=0 B -> JAB1         PROBE style=0 X -> (WAIT)   <-- the shipped bug
```
After the fix: `X -> GRAB` in all three styles, plus X-while-SHIELDING ->
GRAB, X-while-DASHING -> GRAB, and X-AIRBORNE -> ATTACKAIRN (the leg that
pins the alternate-attack role was not traded away).
Teeth **t6-nograb** (zero the light shield -> X reaches no state) and
**t9-seamscramble** (swap SPECIAL/JUMP: every bit still live, but A no longer
reaches KNEEBEND and Y no longer reaches NEUTRALSPECIALGROUND) bite on that
witness; **t10-powershield** holds the `< 1` bound.

**Leg [5]** commits the emitted-vs-renderable vfx comparison — 41 emitted
names vs 45 templates, difference EMPTY — matched on the CALL
(`ml_drawVfx*("<name>"`), never a bare string, with a blindness guard: every
`ml_drawVfx*` call outside `ml_events.{c,h}` must pass a string LITERAL or
the extraction is vacuous and the leg fails loudly. Tooth **T11** removes one
real template from a COPY and requires the comparison to notice.
(41/45, not the hand-run 39/43 — the hand grep undercounted both sides.)

**Runs:** `check-ctl-input.sh` -> `CTL INPUT CHECK OK (11 teeth)` (~44 s) ·
`port/sim/check-sim.sh` -> `SIM CONFORMS` 8/8 · `port/foh/check-rebind.sh` ->
`REBIND TOOTH OK` · `s1_sweep` host-side -> verdict line byte-unchanged.

**PINNED EXPECTATIONS THAT MOVED (behaviour legitimately changed, D34):**
`s1_sweep.c`'s 2048-combo invariants (`in.a == (p.b || p.x)`; `in.lA ==
p.x ? S1_ZGRAB_LA : 0` replacing a flat `lA == 0`; `in.z` moved INTO the
never-set list) and its dump's `z` column, which becomes the `lA` column so
the live plane stays inside the device-vs-host byte comparison;
`judge-s1-coverage.js`'s grab signature and K_X sidecar pairing. None was
weakened — each still pins one exact value, and the two new ones pin a value
that was previously unpinnable because it did not exist.

**OWED, DEVICE (not run — owner's daily driver):**
1. `port/gfx/check-device-input.sh` — its host `s1_sweep` leg is updated and
   green host-side, but legs [4]-[9] (the live S1 session and
   `judge-s1-coverage.js`'s re-cut pairing) need the device. **This is the leg
   that witnesses the fix physically.**
2. `verify_m3.sh` leg (4) — the live S1 three-way replay.
3. `install-play-opk.sh` — reinstall so the device play build carries D34.

**OWED, `port/foh` (NOT this lane's files):** `foh_ctl_labels.h:48` still
reads `"GRAB (Z)"`. That `(Z)` is the last surviving instance of the false
claim. It is pinned by `check-foh-flows.sh` leg [0m]'s 54-row label table, so
the label and its pin must move in ONE commit.

### THE CLASS FIX (HARD RULE 8) — the check must cross the seam
`check-ctl-input.sh` proves the resolver emits bits. **It must also prove those
bits produce the intended ACTION STATE in the sim.** Add an end-to-end leg:
press X -> real resolver -> real sim tick -> assert `actionState == "GRAB"`.
**Every face-button role in D33 deserves the same treatment** (A really jumps,
B really attacks, Y really specials) — otherwise the next remap can ship
equally broken. **This is the instrument rung: one leg that crosses the seam
beats four one-off button fixes.**

## OWNER PLAYTEST 2026-08-24 (post-install) — A43/A44/A45 + the VFX answer

### VFX — **NOTHING IS MISSING. Falcon's neutral B is CORRECT.** (no ticket)
Owner: *"captain falcon's neutral B doesn't show effects when it's charging up,
is it supposed to? any other animations effects we are missing? if so, how are
we missing them? how can we fix that systematically?"*

**Answer 1 — it is faithful.** Upstream's own
`characters/falcon/moves/NEUTRALSPECIALGROUND.js` fires its only two `drawVfx`
calls at **`timer === 50`** (`falconpunch`) and **`timer >= 52 && < 57`**
(`firefoxtail`) — i.e. **at the PUNCH, not during the wind-up. Upstream draws
NO charge effects either.** Our C matches call-for-call (2 emits, same timers).
What fires at `timer === 52` and reads like an effect is
`ml_sound_play("falconpunchbird")` — **the bird is a SOUND, not a visual.**
*(Driver near-miss worth recording: I briefly read that as a missing renderer
template and almost reported a false gap. Grepping a name found it in a
`ml_sound_play` call. Same lesson as A42, one hour later — a name is not
evidence about the plane it lives in.)*

**Answer 2 — the SYSTEMATIC check, run:** every vfx name the sim can emit vs
every template the renderer can draw:
```
ml_drawVfx*("<name>"  across port/sim/   -> 39 distinct names
TPL <name>            in vfxdata-frozen  -> 43 templates
EMITTED BUT NOT RENDERABLE               -> NONE
```
**The renderer is a strict superset of what the sim emits, so no effect can be
silently dropped for want of a template.** (4 templates are unused — upstream
data we carry verbatim.)
**REGISTERED AS A GAP IN THE RIG, NOT A DEFECT:** that comparison is a
one-liner and is **not** a committed check. It should become one — it is
exactly the seam-crossing instrument A42 shows we lack. Folded into A42's class
fix rather than given its own row.

### A43 (P1, owner-reported) — CSS back-out loses the character selection
Owner: *"if you go to the CSS back button while selected as any other character
except falcon, and then come back in, you are selected as falcon... I think the
reason it picks falcon is because when you go to the back button it lets go of
the p1 pin and goes to the 'nearest' character. it should go back to your last
selected character (which we keep track of right?)"*

**His diagnosis is almost certainly right and it is the D21 FAMILY.** A29/D21
fixed the endGame token snap (it indexed the roster by PORT). **This is the
same bug on the BACK-OUT path**: the token is released near the BACK wedge, and
falcon is the LAST cell (id 4) — i.e. the nearest cell to the top-right corner
where BACK lives. **So "nearest cell" is being resolved from the token's
PIXEL POSITION rather than from the stored selection.**
**And yes, we do keep track:** `p1Char`/`p2Char` (the SELECTION plane) survive —
A29 measured exactly that. **The fix is the same shape as D21: on re-entry,
re-home the token on the STORED character, not on wherever it physically sits.**
Start at `css_back()` (`foh.c`) and the CSS entry arm; check
`foh_css_token_pos`'s rest slots — D21 fixed slot 2 (endGame) and this is a
different rest path.

### A44 (P2, owner-reported) — no P3/P4 at the CSS
Owner: *"why can't I turn on player 3 and 4 at the CSS?"*
**MEASURED: the FOH models only TWO PORTS.** `foh.h:599` is `int cssChar[2]`
and the type plane is `p1Type`/`p2Type` (`:602`) — there is no p3/p4 anywhere
in the FOH state. **Upstream supports four** (`playerType` is a 4-array, and the
sim's whole player plane is `[4]` — the sim is ALREADY 4-port: goldens field 4
slots, `tapJumpOff[4]`, the AI drives slots).
**So this is a FOH-side widening, not an engine limitation** — the expensive
half is already done. Scope: 2 more panels on a 240x240 screen, 2 more token
lanes, the type/CPU boxes, `css_ready`'s participant count (already loops 0..3),
persistence, and every CSS check's expectations. **Non-trivial but bounded, and
the sim needs nothing.**

### A45 (P2, owner-reported, big-ticket) — TARGET BUILDER
Owner: *"we need target builder to be implemented (if it exists in the source
code we're basing our port on)"*
**IT EXISTS: `src/target/targetbuilder.js`, 58.5 KB** in the pinned upstream
clone (beside `targetplay.js`, 8.6 KB, which we DID port).
**This is the plane the `+ ADD CODE` slot currently REFUSES** — `foh.c`'s
addcode arm emits `deny` + `ev_refused(s, "addcode")`, registered as
scope-excluded since M4 task 12. **58 KB of jQuery+DOM editor is the single
largest un-ported surface left**, and it is a REWRITE (like the FOH), not a
transliteration. Sequence it with A7/credits as the owner said; it wants its
own arc.

## A42/A43/A40 — ALL THREE LANDED 2026-08-24. `SIM CONFORMS` 8/8 on merged bytes.

### A42 (D34) — X GRABS. **AND THE SEAM INSTRUMENT REPRODUCED THE BUG ON ITS FIRST RUN.**
**My `z` inventory was SHORT BY THREE.** The lane read every call site rather
than trusting the brief: besides the 15 SMASH readers, `z` is also
`action_state_shortcuts.c:522` (checkForAerials) and **`physics.c:983`
(lCancelUpdate)**, plus the AI plane. **So `z` is an alternate ATTACK button AND
an L-CANCEL trigger — not merely "smash-alt" as I recorded it.** Still zero
`GRAB` dispatches.

**The fix is Melee's actual Z chord:** `in.a |= p->x` **and**
`in.lA = S1_ZGRAB_LA` (49/140, the light-shield level, cited from
`docs/research/b0xx-mapping.md` §2.2). **Chosen because ONE chord reaches ALL
FIVE grab arms**, not just GUARD's — `WAIT.c:56`/`DASH.c:72` take their
`lA>0||rA>0` arm into GUARDON, whose `init->main->interrupt` chain runs **inside
the same tick** and still sees the `a` edge. **Both bounds are load-bearing:**
`>0` or no grab arm fires; `<1` or `GUARDON.c:21` **powershields on every
press**.
**The alt-attack role is NOT traded away** — every `z` reader is an `a || z`
form and lCancel's third arm is an `lA` edge, so `a`+`lA` covers all of them
identically. `z` is dropped rather than kept as a second name for a bit `a`
already sets.

**THE INSTRUMENT IS THE TICKET, AND IT PROVED ITSELF.** `ctl_seam_witness.c`
(leg [4] of `check-ctl-input.sh`): physical button -> real
`s1_input_row_style` -> **real `sim_game_tick`** -> the actionState the engine
enters. **Run against the UNFIXED tree it reproduced the owner's symptom
immediately:**
```
A -> KNEEBEND   B -> JAB1   Y -> NEUTRALSPECIALGROUND   X -> (WAIT)
```
After: `X -> GRAB` in all 3 styles, + X-while-SHIELDING -> GRAB, X-while-DASHING
-> GRAB, and **X-AIRBORNE -> ATTACKAIRN** (pinning that the alt-attack role
survived). **Leg [3e] no longer CLAIMS grab — it claims the chord and points at
leg [4], because a bit assertion claiming an action is precisely what shipped.**

**The vfx leg [5] corrected my hand-count: 41 emitted vs 45 templates, not
39/43 — my one-liner undercounted BOTH sides.** Difference still empty. It
matches the CALL, never a bare string, and carries a **BLINDNESS GUARD**: any
`ml_drawVfx*` call outside `ml_events.{c,h}` that does not pass a string literal
**fails the leg loudly**, because one variable-named emit site would silently
make the whole comparison vacuous.

**Pinned S1 expectations moved (D34, none weakened):** the 2048-combo
invariants (`in.a == (p.b||p.x)`; `in.lA == p.x ? S1_ZGRAB_LA : 0` replacing a
flat `lA==0`; `in.z` moved INTO the never-set list) and the dump's `z` column ->
`lA`, **so the live plane stays inside the device-vs-host byte comparison**.

**DRIVER FOLLOW-UP, LANDED:** the lane flagged `foh_ctl_labels.h:48` still
reading **`"GRAB (Z)"`** — *the last surviving instance of the false claim* — in
a file it could not touch. No lane owns `port/foh/` now, so the driver corrected
it **together with the 6 rows of `check-foh-flows.sh`'s pinned 54-row label
table in ONE change** (leg `[0m]` requires exactly that). `FOH FLOWS OK`.

### A43 (D35) — the back-out leaked the TOKEN GRAB, not the pixels
Owner's diagnosis was *nearly* right: he guessed the pin released and snapped to
the nearest character. **The real mechanism is that the GRAB ITSELF leaks across
the back-out**, so the token stays nominally held. The lane's note is the
valuable part: **the spec ALREADY documented the overlap** (*"you exit at frame
30 while still nominally carrying"*) **without noticing its consequence.** The
behaviour was written down; nobody connected it to a bug.

### A40 — the menu plane must not consume a sim `play id`
Landed with `snd_playid_check.c` — **the CLASS is instrumented, not the one
sound**, so puff's `furaLoopID` exposure is covered by the same check.

**VERIFIED ON MERGED BYTES:** `SIM CONFORMS` 8/8 · `CTL INPUT CHECK OK` ·
`CSS TOKEN REST` · `SND IDLE SILENT` · `REBIND TOOTH OK` · `FOH FLOWS OK`.

**DEVICE LEGS OWED (added):** `check-device-input.sh` legs [4]-[9] — the live S1
session and the re-cut `judge-s1-coverage.js` pairing, **the leg that witnesses
the grab fix physically**; `verify_m3.sh` leg (4); `install-play-opk.sh`
reinstall so the device build carries D34/D35/A40.

## A44 — LANE CORRECTLY REFUSED TO SHIP HALF. And the blocker is NARROWER than reported.

**The lane's judgement was right and is the model for this class.** It found two
of its own brief's constraints **mutually exclusive** — *"NO stubs; a port that
can be switched on but cannot actually play is a stub"* vs *"do not touch
`port/sim/**`"* — **named the contradiction instead of silently picking one**,
and committed the measurement with **zero behavioural change**. A FOH-only
widening would have shipped exactly the stub the brief forbade: toggle P3, press
START, receive `deny` + `ev_refused(s,"portconfig")`.

### THE LAYOUT WORRY WAS UNFOUNDED — IT ALREADY FITS, AT ZERO COST
I told it to measure whether 4 panels fit on 240x240 and to report options
rather than shrink anything. **Measured: `FOH_CSS_PANEL_{X0,PITCH,W}` =
`{1,60,58}` (`foh.h:272-275`), so ports 0..3 land at x = 1/61/121/181 and port 3
spans 181..238 — inside the screen.** Better: **`render_css` ALREADY loops
`k = 0..3`** (`foh_render.c:1713`), `css_panel` is fully port-parameterised,
`kPortTint` has 4 entries, the ghost letter is `('1' + port)`, the N/A watermark
arm exists, the CPU rail fits at every k, and **`css_ready` already counts all
four** (`foh.c:770`). **No narrower panels, no 2x2 grid, no options to choose.
The presentation half is essentially free.**

### THE REAL BLOCKER: `sim_setup_match` IS 2-SLOT BY CONSTRUCTION
`port/sim/sim/sim_boot.c:381` — the `i < 2` loop writes the per-slot plane while
the `i = 2..3` loop **hard-pins** `playerType = -1`, `playerPresent = false`,
`currentPlayers = -1`. It is the **only** launch entry point (6 callers,
measured). `foh.c:844-856` already carried this as a registered limitation —
*"delete this arm when `sim_setup_match` carries BOTH ports' type and level"* —
and **A44 re-measures that "both" as four.**

### ⚠ DRIVER CORRECTION — THE ORACLE IS **NOT** THE BLOCKER
The lane reported *"no 4-port oracle... `oracle/meleelight-harness.patch:76-92`
is 2-slot by construction"* and concluded a 4-port match is unverifiable.
**MEASURED, AND THAT IS WRONG.** The patch reads:
```js
export function harnessSetupMatch (cfg){
  for (var i = 0; i < 4; i++) {          // <-- FOUR
    var pc = cfg.players[i];
    if (pc) { playerType[i] = pc.type; ... characterSelections[i] = pc.character; ...
```
**The browser harness is ALREADY 4-PORT.** What is 2-port is
`oracle/harness/run.js:152-157`, which hands it `[p1, p2, null, null]` — **a
CALLER passing two, not a harness accepting two.**
**So a 4-port golden IS recordable**, and the HARD RULE 3 problem the lane
feared (writing `oracle/`) may be avoidable entirely: `port/goldens-m4/` exists
precisely as the M4-era golden machinery that REUSES `oracle/harness/run.js`
**by path** without writing `oracle/`. **The open question is narrower: can a
4-port trace be driven without editing `run.js`** (which IS under `oracle/`), or
does it need a `port/goldens-m4/`-side driver. **That is a small measurement,
not a ratification.**
**Twelfth premise corrected this session — and the first one where a LANE's
premise was wrong rather than mine.** The lesson is symmetric: it read the patch
hunk's *context* and inferred; the loop bound was two lines away.

### THE PATH — sim + oracle lane FIRST, then a short menus follow-up
1. **Widen `sim_setup_match` to a 4-port config with a 2-port-compatible
   wrapper**, so `sim_main.c`/`gfx_app.c`/`gfx_replay.c` and **every existing
   golden stay byte-unchanged** (`SIM CONFORMS` 8/8 is the bar, D20/A37
   pattern).
2. **Record a 4-port golden** via `port/goldens-m4/` — resolve the `run.js`
   question above first.
3. **Then lane M's half is short and well-understood**, against the site
   inventory the lane measured and committed (selection plane, token plane,
   type/CPU plane, bridge/frozen, and the `MLFKPERSIST5 -> v6` bump) — that
   inventory is the ticket's real deliverable and it is now on disk.

**No D-numbers consumed (D37/D38 free). No device legs owed — no compiled byte
changed. The four CSS checks were run twice, before and after, all green.**

## A45 — DESIGN SPIKE DONE 2026-08-24. **THREE OF MY FRAMINGS WERE WRONG.**

Full design: `docs/research/target-builder-design.md` (548 lines). The lane read
**all 1606 lines** of `targetbuilder.js` plus `encode.js`, `targetselect.js`,
`targetplay.js`, `getConnected.js`, and where it claims MEASURED it **executed
upstream's own code** — babel-transpiled out of the read-only clone into
scratch, never modified, never transcribed.

### THE SHARE CODE (the highest-value answer, and a TRAP)
`src/stages/encode.js` — **`createStageCode` / `parseStageCode`.**
**NOT `createTargetCode`, WHICH IS DEAD:** zero callers, and it reads
`stageTemp.box` and `stageTemp.startingPoint.x`, **neither of which that object
has.** *"Anyone grepping for 'target code' ports the wrong 33 lines."*
Format: 14 `&`-separated fields (`~` records, `,` numbers), **whole-stage not
target-layout** — targets are field 11. **Round-trips EXACTLY from the second
emission on** (`c1 === c2`, measured): the first encode is lossy via
`toFixed(2)`, every later one exact. **The code is the canonical form.**
**It parses WITHOUT `strtod`** (banned since iter 38 — the SDK's musl strtod
mis-rounds subnormals): the alphabet is `-?\d+\.\d\d`, so integer hundredths
÷ 100.0 is bit-exact, ~20 lines. **Emitting** needs `toFixed(2)`, which we lack
(`ml_fmt.c` is `String(x)`) — ~60 lines on Ryu, **with `fmt_diff` as a ready
oracle.**

### THREE MEASURED UPSTREAM BUGS (carry-or-fix decisions, not assumptions)
1. **`encode.js:39` `if (i !== 5)`** tests the **surface index** where it meant
   the **type index** -> the 6th surface of EVERY type silently loses its damage
   type. **Seven fire walls in, six out.**
2. **`encode.js:244`**: a decoded stage returns `polygonMap = [null,…]`, so
   re-editing an imported stage lets you drag a polygon's outline away from its
   collision surfaces.
3. **`targetselect.js:164-166` CLOBBERS**: adding A then B then C leaves
   `["B","C","C"]`. **Every added code destroys the previous one**, on both the
   add and the boot-reload path. **NEEDS AN OWNER RULING — there is no oracle
   for this plane**, so faithfulness and usability genuinely conflict.

### THREE FRAMING CORRECTIONS — ALL MINE
1. **IT IS NOT A DOM EDITOR.** **Exactly 9 of 1606 lines touch the DOM**, all in
   the save/share overlay. Tool selection is ALREADY on the shoulders; the
   crosshair is ALREADY `lsX/lsY`. **`foh_hand.h` (D29) supplies the motion
   nearly as-is.** I sized this as "a jQuery+DOM editor → a rewrite like the
   FOH". Wrong: the interaction model is already gamepad-shaped.
2. **THE BLOCKER IS THE SIM, NOT `foh.c`.** `tp_setup_target` /
   `tp_stage_from_ttab1` / `gfx_target_init` all take an **integer id into the
   generated TTAB1 table**, so nothing can load a custom stage. But `MlStageX`
   is a plain runtime struct — **the fix is ONE new filler function**, small,
   and in a different plane than the ticket's lane.
3. **`+ ADD CODE` IS THE LAST PIECE WORTH BUILDING, NOT THE FIRST.** Its whole
   job is accepting a ~1 KB paste on a device with no keyboard.
   **Recommendation: one `.mlstage` file per slot on SD**, published through
   `foh_persist.c`'s existing atomic rename — **better than upstream's paste box
   AND it deletes the D8/A14 dependency entirely.**

### TWO FINDINGS THAT MOVE THE ESTIMATE
- **Every geometry primitive is ALREADY PORTED and replay-verified** —
  `intersectsAny`, `distanceToLine`, `distanceToPolygon`, `lineDistanceToLines`,
  `manhattanDist`, `extremePoint`. **Only `getConnected`'s 89 lines are
  missing.**
- **THE DAMAGE PLANE HAS NEVER EXECUTED**: `damageType` exists on **zero**
  authored stages, so `dealWithDamagingStageCollision`'s five call sites are
  **covered by no golden.** The DAMAGE tool makes them live and **owes its own
  recording.**
- `undo()` has **zero callers** and `undoList` only ever receives `"target"`.
  **Do not port it.**

### TICKET BREAKDOWN (8-11 iterations, ~8-10x A7)
**T1 codec + value model** · T2 custom stages PLAY · T3 TSS custom slots +
verbs · T4 editor core (targets only) · T5 Platform+Wall · T6 Ledge+Damage ·
T7 Polygon · T8 Scale+DrawMode · T9 on-screen code UI (blocked by D8->A14,
**probably unwanted** given correction 3).
**SMALLEST USEFUL FIRST: T1** — the codec alone. No UI, no lane-M file, fully
mechanical proof (differential against upstream's executed `encode.js`), and
every later ticket depends on the value model it defines.
**SMALLEST PLAYER-VISIBLE: T2** — custom stages playable with NO EDITOR AT ALL:
drop a code file on the SD card.
**T1/T2/T5-T8 are `port/sim/` + `port/gfx/` only and can run BESIDE the lane-M
chain**; only T3 and half of T4 need `foh.c`.

### COULD NOT DETERMINE (named, with how to settle each)
ANIM1 rendering outside a match (cosmetic; the doc names the two-file read);
`sizeof(MlSim)` headroom for raising `ML_MAX_TARGETS 10` / `ML_MAX_SURFACES 64`
against a player authoring 20 targets / 120 polygons (host-measurable);
legibility at 240x150 (**owner playtest, not source**); whether the built
browser page truly reloads >1 custom stage (the LOGIC is measured broken).

## A47 (SPIKE, owner-ideated 2026-08-24) — TWO-DEVICE LINK PLAY. **The transport already ships.**

Owner: *"what about connecting two funkey-s where one is the master and the
other is slave and slave sends its inputs to master and master is used for
screen? and also, what about if both sent inputs to each other and game was
sync'd so both players' screens show the same thing... even send pixel data of
the screen to the other device? needs a spike to figure out what is best way,
easiest way with no downside... this could even be a bigger project where it
could apply to other games / emulators on the funkey?"*

### CONNECTIVITY — MEASURED ON THE DEVICE 2026-08-24, and it is decisive
```
/sys/class/net/        lo  sit0          <- loopback + a virtual tunnel. NOTHING REAL.
/sys/class/ieee80211/  none              <- NO WIFI
/sys/class/bluetooth/  none              <- NO BLUETOOTH
/sys/class/udc/        musb-hdrc.1.auto  <- a USB DEVICE CONTROLLER, state "configured"
/lib/modules/4.14.14-funkey/.../gadget/legacy/
        g_ether.ko  g_ncm.ko  g_serial.ko  g_acm_ms.ko   <- ALL PRESENT
built-in: libcomposite, usb_f_fs, usb_f_serial ; configfs gadget "FunKey" is LIVE
```
**THE ONLY POSSIBLE LINK IS USB — and the transport for it is ALREADY ON THE
DEVICE.** `g_ether` / `g_ncm` are USB-ethernet gadgets: **plug two FunKeys
together and you get an IP link**, no soldering, no extra hardware.

**BUT THE A33 CONSTRAINT BINDS EXACTLY HERE, AND IT IS THE WHOLE TICKET.**
USB is asymmetric: **one end must be HOST, the other DEVICE.** Both FunKeys ship
`dr_mode = "peripheral"` (A33, measured), so **two stock devices CANNOT talk —
they are both gadgets.** One of them needs the same four-line
`dr_mode = "host"` fork A33 already scoped.
**So A47 and A33 share a prerequisite.** One OS fork unlocks BOTH the GameCube
adapter and two-device link play. That materially changes A33's cost/benefit:
it is no longer "a controller", it is "the port becomes usable for anything".
**AND THE SAME RISK BINDS: `adb` runs over MUSB gadget.** The HOST-side device
would likely lose adb — survivable for a play unit, fatal for the dev unit.
**Test on a throwaway SD first.**

### WHICH SYNC MODEL — THE ANSWER IS ALREADY BUILT, AND IT IS NOT PIXELS
The owner listed three: (a) slave sends inputs, master renders both;
(b) both send inputs, both simulate; (c) send pixel data.

**(b) IS NEARLY FREE FOR THIS PROJECT SPECIFICALLY, AND IT IS NOT A COINCIDENCE.**
The entire port is built on **deterministic input replay with per-frame
checksums**:
- `oracle/goldens/*.trace.json` ARE input traces; the sim replays them to
  **bit-identical** state.
- `check-sim.sh` proves 8 goldens **exact over 3600 frames**, and
  `check-device-conform.sh` proves the **same streams on the device**.
- **So "both ends simulate from shared inputs" is the thing this codebase has
  already proven it does bit-exactly, host AND device.** The per-frame checksum
  is *already* a desync detector — the hardest part of lockstep netplay is
  built and verified.

**(c) PIXELS IS THE WORST OPTION, by measurement:** 240x240x16bpp = **112.5 KB
per frame**, x60 = **6.75 MB/s**. USB 2.0 could carry it, but the FunKey must
also *render* it, and `device-perf.md:34` records only **~6 ms of headroom** in
a 16.67 ms frame. **Inputs are ~8 bytes per frame per player** — six orders of
magnitude less.
**(a) is a legitimate SIMPLER first rung** (one sim, one screen, second pad
remote) and is genuinely easier — but it gives the second player no screen,
which is a real downside the owner should weigh.

### WHY THIS IS BIGGER THAN THIS GAME (the owner's last question: YES)
`g_ether` + a host-mode fork is a **generic FunKey-to-FunKey IP link.** Nothing
about it is meleelight-specific. Any emulator with netplay (and most have it)
inherits it. **But note the honest asymmetry: OTHER emulators do NOT have this
port's determinism proof**, so link play for them is a much harder correctness
problem than it is here. **We are the easy case, not the representative one.**

### THE SPIKE'S QUESTIONS, in order
1. **Does a host-mode fork actually enumerate a second FunKey as `g_ether`?**
   (rung 3 of A33's ladder + one `modprobe` — settles the whole idea)
2. **Does the HOST end keep `adb`?** If not, the dev unit must stay the gadget.
3. **Latency, measured, not assumed** — USB-ethernet RTT between two devices
   against the 16.67 ms frame. Decides lockstep vs rollback.
4. **Power:** can a bus-powered FunKey run the link, or does the host end need
   its own supply? (A33's VBUS finding applies.)
5. **Which model to build first** — recommend (a) as a rung, (b) as the goal.
**PREREQUISITE: A33 rung 3. Do not start A47 before it.**

## A45-T1 — DONE 2026-08-24 (D39). The stage-code codec, proven DIFFERENTIALLY.

**Value model: `MlkStage` (`port/sim/stage_code.h`), 45,344 bytes measured** —
that number is the input to the spike's cap ruling (R2), so it is recorded here
rather than left to be re-derived. **It REUSES the sim's existing stage
vocabulary** (`Stage`, `Surface`, `MlLedge`, `Box2D`, `Vec2D`) rather than
inventing a parallel one, and adds fields only where envcoll never needed them.
**One of those is load-bearing and subtle:** `hasStartingFace` — an **absent**
key emits `1,1,1,1` while an **empty array** emits nothing, so the flag is the
only way to round-trip the difference.
Caps are **LOUD**: an over-cap code is **rejected with a reason, never
truncated**. *(The other half of R2 — refuse at load vs raise the caps — is an
**owner ruling still owed before T2**.)*

**THE DIFFERENTIAL NEVER TRANSCRIBES UPSTREAM.** `stage-code-js-ref.js` runs
**the clone's OWN babel** (presets read out of its `package.json`) over
`encode.js` + its six deps into a gitignored build dir and `require`s the
result. **The clone is never written.** Two guards: the seven oracle files must
be unmodified w.r.t. the pin — *and note the assertion is "unmodified w.r.t. the
pin", not "clean", because the clone legitimately carries
`meleelight-harness.patch`; "clean" would have been the WRONG assertion* — and
the clone state must be unchanged by the run.
**The interchange is the CODE ITSELF**: both sides parse→encode and `cmp`
judges. Corpus generated by **upstream's own `createStageCode`** over 4000
seeded stages:
- **3892 well-formed + 108 edge codes** — byte-identical, C byte-stable ×2
- **800,075 doubles** — `ml_to_fixed2` vs V8's `Number#toFixed(2)`, cmp-exact
- **42 hostile** — both verdicts frozen in `expected-stage-code.json`

**`toFixed(2)` is exact integer work on the bits, never `round(x*100.0)`.** The
measured reason it matters: **`(2**60).toFixed(2)` is
`"1152921504606846976.00"` — a value `String(x)` CANNOT NAME**, so the
"`ml_fmt_dtoa` plus `.00`" shortcut is wrong. 16 frozen anchors + the 800k
differential prove it.

**Both upstream bugs carried, and BUG 1 asserted POSITIVELY rather than inferred
from agreement:** 13,191 sixth-surface records in the corpus, **every one
missing its damage digit**. BUG 2 is carried **in the value model**
(`polygonMapIsNull`) so T7's MOVE/DELETE arms see the null instead of silently
re-deriving a link upstream does not have. **`createTargetCode` not ported — the
designed trap avoided.**

**IT FOUND AND FIXED A VACUOUS TOOTH MID-BUILD** — the hostile tooth originally
compared against the JS output, which **already differs on 21 rows by D39's own
design**, so it bit for free. All four teeth now compare against the
unperturbed C baseline. *(Fourth vacuous-tooth instance this session; the
CONTEXT.md `Tooth` entry earns itself again.)*

**D39** registers that `mlk_parse` accepts a **strict subset** of
`parseStageCode`'s language — measured: `"aaaaaaaaaaaaaa"` returns a *stage*
upstream, not `null`. Cause is the iter-38 `strtod` ban; **the 21 divergent
hostile rows are ENUMERATED with both verdicts**, so it is reviewable rather
than asserted.

**Gates:** `STAGECODE MATCH` (re-verified on merged bytes) · `SIM CONFORMS` 8/8
· nothing under `oracle/` written. **New TU deliberately NOT added to the frozen
build list — T2 wires it in.**

## A45-T2 — DONE 2026-08-24 (D42, D43). **CUSTOM STAGES PLAY. THE SPIKE'S "ONE NEW FILLER" HELD.**

**The blocker was exactly what the spike measured, and I verified it before
building rather than after.** `tp_setup_target` / `tp_stage_from_ttab1` /
`gfx_target_init` all take an integer id into generated TTAB1, so a stage the
player authored had nowhere to go — but `MlStageX` (`physics.h:74-89`) is a
plain runtime struct, neither generated nor const. **So the sim change is an
ASSIGNMENT, not a conversion:** T1 modelled `MlkStage.s` as the sim's own
`Stage`, so `tp_stage_from_custom` is `out->s = cs->s` plus ledges, blastzone,
`hasConnected = false`, `respawnCount = 0`. Roughly twenty lines.

**THE REAL WORK WAS THE SEAM, AND IT IS A ROOT-CAUSE FIX, NOT AN ARM.** The
lazy-looking move — add a custom `if` to `tp_setup_target` — would have left
`startTargetGame` translated once and *branched* twice. Instead
`tp_setup_target_core(g, charId, playingId, stage, targets, count,
startingPoint)` is the ONE translation and **both** entries route through it:
the TTAB1 entry decodes a generated row, `tp_setup_target_custom` decodes an
`MlkStage`. An authored and a custom stage now differ in **where their geometry
came from and in nothing else** — which is also what makes the done-check a real
differential instead of a self-consistency test.

**Same trick in the renderer, and it cost nothing.** All ten `gfx_target.c` draw
functions read `g_tt`, a `const ml_tstage_t *`. The TTAB1 *rows* are const; the
*pointer* is not. So `gfx_target_init_custom` materialises one TTAB1-shaped row
at runtime and binds it — **every draw function is byte-unchanged**, and the
renderer cannot accidentally treat a custom stage differently because it cannot
tell them apart. (`box` count 0 and `offset [600,375]` are CONSTANTS of this
plane, not missing data: neither is a field of the 14-field code grammar, and
`targetbuilder.js:66` shows no tool ever edits `offset`.)

**THE DONE-CHECK IS A DIFFERENTIAL AGAINST FROZEN BROWSER EVIDENCE.**
`port/sim/target/check-custom-stage.sh` -> **`CUSTOM STAGE PLAYS`**. For every
target golden: re-express its AUTHORED stage as a share code, publish it as a
`.mlstage`, load it back through the custom path, replay the **same** trace, and
require the two runs byte-identical — then judge **both** with the UNCHANGED
`verify-stream.js` + `verify-target-stream.js` against the frozen goldens, so
the differential cannot pass by both sides being wrong the same way. t01 and t02
both: `authored == custom, both == frozen, 2 targets destroyed`. **That is the
ticket's ask — it PLAYS, measured frame by frame, nothing hand-poked.**

**IT IS ONLY SOUND BECAUSE THE ROUND TRIP IS EXACT, AND THAT WAS MEASURED FIRST**
(executed walk over `targets.json`, all 10 authored stages): **210 numbers, ZERO
lossy at `toFixed(2)`**. Had one been lossy the streams would diverge — the
check re-measures it every run rather than trusting the note.

**THE TOOTH THAT DID NOT BITE, AND WHY IT MATTERS.** The play-leg tooth first
shifted **ground surface 0** by 1.00 world units and the stream did not move —
on t01 that surface is a ledge at y=88 the fox never touches. **Fifth instance
this session of the razor-thin-nudge class** (fix_plan rule-12 corollary): a
tooth must perturb the domain that OCCURS, not the first row of it. Shifting
every surface in all five lists bites, and the miss is recorded at the site so
the next author does not re-learn it.

**`getConnected` WAS NOT NEEDED — and the reason is structural, not lucky.**
`connected` is not one of the code grammar's 14 fields at all, so a decoded
stage never had one to recompute; `hasConnected = false` puts the physics reads
in their absent arms exactly like fdest/ystory. `getConnected` is the BUILDER
recomputing after a structural edit (`targetbuilder.js:410` etc.) — **A45 T7's,
not T2's.** Its 89 lines stay unwritten.

**THE DAMAGE PLANE DID NOT BECOME LIVE — IT IS REFUSED AT THE DOOR, LOUDLY.**
A share code CAN carry a damage digit (field `d`, 0..4), so loading one would
have made `dealWithDamagingStageCollision`'s five translated call sites live
with **no golden behind them** — and `target_play.c`'s tick arm would have hit
its stage-damage trap mid-match. `mlk_stage_playable` refuses at load with a
reason naming T6 as the owner of the golden it owes. **The refusal is the
marker, not a stub.** One measured subtlety: props with a **null** `damageType`
are inert (physics tests truthiness) and are exactly what upstream BUG 1 emits
for every sixth surface — refusing those would reject codes upstream plays fine,
so only a real type string refuses.

**D43 (owner ruling): the clobbering is fixed IN THE VALUE MODEL.** Ten slots
**addressed by index**, no append, no length cursor — so upstream's two clobber
sites (`targetselect.js:164-166` add, `:551-552` reload) are *structurally
absent* rather than individually repaired. **That is why the reload half is
fixed too**, which a patch to the add path alone would have missed. Leg [2]
runs upstream's exact data-destroying sequence and proves all three stages
survive.

**R2 — MY RECOMMENDATION, NOT A UNILATERAL CAP CHANGE: REFUSE, DO NOT RAISE.**
The port refuses an 11th target with a reason. Raising `ML_MAX_TARGETS` 10 -> 20
is *possible* (the `ML_MAX_LEDGES` 8->16 capacity precedent) but it would break
the `_Static_assert` tying the cap to upstream's own 10-element
`targetDestroyed` literal (`targetplay.js:37`), and **no authored stage exceeds
9** — so raising it buys nothing today and costs the one compile-time guard that
keeps `targetDestroyed[]` in bounds. **The builder (T4) should refuse the 11th
target at authoring time**, which is where the message is useful. Owner's call;
the safe half ships.

**Gates:** `CUSTOM STAGE PLAYS` · `SIM CONFORMS` 8/8 · `STAGECODE MATCH` ·
`TARGET SIM CONFORMS` (the `tp_setup_target` refactor's regression) · nothing
under `oracle/` written · `port/foh/` untouched (parallel lane).

**WHAT T3 NEEDS FROM THE FOH, and the ONE thing T2 deliberately did not build:**
T2 has **no writer**. It loads and plays; the file arrives by SD card. When T3
or T4 needs to WRITE a `.mlstage`, the correct move is to **generalise
`foh_persist_save`'s existing atomic publish** (`foh_persist.c:506-551` — tmp
write, fsync file, rename, fsync dir, every rc checked, loud on failure) into
`foh_persist_publish(name, buf, n)` and call it — **not** to grow a second
file-writing path. `/mnt` is journal-less vfat mounted `errors=remount-ro`, so
an unchecked write rc is silent data loss, and a free-space check before writing
turns a dying card into a clear message instead of a truncated stage file.
Beyond that T3 needs only: `mlk_slots_scan(dir, &slots)` for the list (presence
by index, a named reason per empty slot), `mlk_slot_load(dir, slot, &stage,
&why)` at play time, then `tp_setup_target_custom` + `gfx_target_init_custom`.
Stages are deliberately **not** held resident — `sizeof(MlkStage)` is ~45 KB and
ten would be ~450 KB on a device that counts them.

## A46 — DONE 2026-08-24. FOUR-PORT MATCH SETUP + A 4-PORT GOLDEN. **AND IT CAUGHT ITSELF ABOUT TO BREAK THE M4 GATE.**

**THE ORACLE ANSWER: YES, AND NO `oracle/` BYTE WAS TOUCHED** (driver-verified:
`git diff HEAD~1 HEAD -- oracle/` is empty). The driver's correction to A44 was
right, and the lane went further: **`pagelib.js`'s `__serializeState` /
`__runFrames` / `__coverage` ALL loop `i < 4`** too. **The ONLY two-port thing
under `oracle/` is a CALLER** — `run.js:152-157`'s literal `[p1,p2,null,null]`.
Solved with **the registered-fallback pattern already in-tree**
(`run-target.js`'s precedent): new `port/goldens-m4/run-4p.js` reuses the oracle
harness bytes **verbatim by path** and changes only `cfg.players`.

**The widening:** `sim_setup_match_ports(g, const SimPortCfg ports[4], stageId)`
is **upstream's patch verbatim** — one `for (i = 0; i < 4)` with its `if (pc)` /
`else` arms. The old `sim_setup_match` **keeps its signature** and becomes a
4-line wrapper, so `gfx_app.c`, `gfx_replay.c` and every other caller are
**literally untouched**, and `sim_main.c` still routes the 8 goldens **through
the wrapper** so they exercise it. **`startGame`'s body was ALREADY 4-port**
(`for (n=0;n<4)` -> `initializePlayers` -> `drawVfx "entrance"` in port order,
then `"start"`), so spawn positions and entrance-vfx ORDER are upstream's,
untouched — **which is exactly the risk A44's lane flagged about doing this
outside the engine.**

**Verified by the driver on merged bytes:** `SIM CONFORMS` 8/8 ·
`FOUR PORT OK` · `VERSUS ENDLESS OK` · `STAGECODE MATCH` (+ the lane's
`AI LIVE CONFORMS`).
**4-port witness `q01`** (fox/falco/puff/marth, battlefield): two fresh browser
runs `IDENTICAL`, `rngCalls=280`, `QUALITY OK` with real KOs and
`stocks=[2,3,4,3]`, and the C replay judged by the **UNCHANGED**
`verify-stream.js`: `STREAM MATCH 3600/3600 frames exact`. Frame-1 envelope
carries `p0..p3`; ports 2/3 sit at upstream's own `startingPoint[2]/[3]`.
Three teeth, each biting alone.

### ⚠ THE NEAR-MISS THAT RESHAPED THE DELIVERABLE — the best catch this session
The lane **first added the 4-port golden to `port/goldens-m4/manifest.json` and
had it GREEN.** Then it measured the CONSUMERS and **reverted it**, for two
independent reasons **either of which is decisive**:
1. **`check-device-fullgame.sh:707,836` enumerates every `^[ms][0-9]{2}$` row of
   that manifest and replays it TWO-PORT on the device** — a 4-port row there
   **breaks the M4 gate**.
2. **`m4-freeze-manifest.txt:424` PINS `manifest.json`'s bytes as
   `reviewed-go`**, and `verify_m4.sh` checks that pin FIRST. **Re-pinning is a
   reviewed change, not a lane's.**
So the 4-port family lives in a NEW `manifest-4p.json` (**the
`manifest-target.json` precedent applied again**), and `manifest.json` is
**byte-identical — driver re-verified `f10e0001…` before and after the merge.**
**A green check is not evidence that a change is safe; the CONSUMERS have to be
measured.** That is the same class as A42's seam, one layer up: the artifact was
valid, and the thing that reads it was not consulted.

`freeze-stream-m4.js` gained a **two-entry `REGISTRIES` table** — row schema and
id grammar chosen by **WHICH REGISTRY, never guessed from the row's shape**, and
the grammars (`^[ms][0-9]{2}$` vs `^q[0-9]{2}$`) are **disjoint so the shared
golden home cannot collide.** Regression-proved by re-recording `s01` and `m01`
through the modified `record-m4.sh`: both print
`unchanged (byte-identical re-freeze)`.

### OPEN / OWED
- **D37 NOT consumed — nothing here is a deviation.** `harnessSetupMatch` and
  `startGame` are 4-port upstream; this made a ported-but-NARROWED call site
  match them. **D37 stays free.**
- **`q01` is deliberately INVISIBLE to `verify_m4.sh` / `check-device-fullgame.sh`.**
  Wiring the 4-port family into the M4 device gate needs a
  `check-device-fullgame.sh` arg-list widening **plus** a re-pin of
  `m4-freeze-manifest.txt` — **both reviewed changes, and the natural follow-up
  ticket.**
- **CPU on ports 2/3 is REFUSED, not stubbed** (`sim_main.c`, `record-m4.sh`,
  the freezer): AIBRIDGE1 is one recorded stream for one CPU slot, so there is
  no C-side replay for it today. **Honest refusal over a silent wrong answer.**
- **LANE M's A44 FOLLOW-UP IS NOW UNBLOCKED**: `sim_setup_match_ports` is the
  entry point a 4-port FOH launch line needs, and A44's committed site inventory
  is the map.

## A7 — DONE 2026-08-24 (D38). **THE CREDITS ARE A STAR FOX SHOOTING GALLERY.**

The 2026-08-03 recon note *"A7 IS NOT A CREDITS ROLL"* is now explained.
`src/menus/credits.js`, 422 lines, gameMode 13: a **100-star warp field**
radiating from centre; **14 `ScrollingText` names** scrolling up at -2 px/frame
with a sideways wobble; **twin lasers from the bottom corners converging on a
rotating reticle**, A fires on an 8-frame cooldown, and the bolt is tested
against the names **on the frame its `life` reaches 15 — so you must LEAD the
target**; X/Y cycle 4 laser colours; START/L/R fast-forward. **Two exits**, both
to gameMode 1: B, and a 2500-frame timer that plays `complete`/`failure` by
score. **It is the only screen with two out edges.**

**THE NAMES WERE NEVER TYPED — and that is the ticket's real safety property.**
The C table was lifted from the pinned clone by a regex over
`new ScrollingText(...)`, and **`check-credits.sh` leg [2] RE-RUNS THAT
EXTRACTION against `$MELEELIGHT_CLONE` EVERY RUN** and requires the table to
match row for row (name, role, blurb, yPos). The witness asserts **upstream
literals** (`"SCHMOO"`, `"TATATAT0"`, `"PROGRAMMER"`), **never
`foh_credits[i].name`** — *asserting the table it renders from would be
self-referential and a placeholder would pass.* A separate assertion draws all
42 authored strings through face 1, so **a credit containing an unmapped glyph
is a `gfx_fatal`, not a silent blank.**

**D38 (FOH-local RNG):** upstream's `Math.random` here **IS the seeded sim
stream** — the same fact that makes the SSS RANDOM slot a registered refusal —
and the 465-draw boot pin means sharing it would **move every golden**. Own
mulberry32, own seed, one `FohState` field; **the FOH stays a pure
`(state, input)` function.** Explicitly NOT an authored star table: those are
values upstream *draws*.
**D12 extended:** the reticle uses the **shared `foh_hand_step`** (A25c/D29), so
`check-hand.sh`'s DRY caller pin moves 2 -> 3 — *which is the review that line's
own text asks for by name.*

**Judge edges: three added, one refusal DELETED**, each cited —
`menu.js:145-149`, `credits.js:236-245`, `credits.js:226-235`. The deletion is
the principled part: **a refusal promises an affordance does nothing, and this
one now opens a screen** (the `audio`/`keyboard` precedent). `WANT` E 28->31 /
R 8->7 and the probe count re-pinned **with the direction argued** — two probes
are exactly what the deleted R row emitted.

**Teeth (3, each proven to fail on ITS OWN line and not the others'):** T1 a
placeholder credit fails the on-screen name assertion; T2 the B exit disabled
fails the exit assertion with names untouched; T3 the panel keyed to the wrong
index fails names-the-right-person with score/exit untouched.

Green on merged bytes: `CREDITS CHECK OK` · `FOH FLOWS OK` ·
`JUDGE REGRESSION OK` · `HAND CHECK OK`.

### ⚠ DRIVER ERROR, MADE AND REVERTED — the freeze manifest
A7 correctly listed four `m4-freeze-manifest.txt` rows as **the driver's to
move** (the lane refused to touch a `reviewed-go` file — the same refusal A46's
lane made). **I then ran a `sed` over that manifest and it rewrote the WHOLE
FILE — 452 lines changed, not 4 rows. Reverted with `git checkout --`.**
**I did exactly what I had told two agents not to do**, on the single file the
M4 gate reads FIRST.
**Correct handling, and it is already the rule:** §A-par.5 — *ONE BATCHED
RE-PIN PASS, DRIVER-ONLY, AT THE END.* **M4 is owner-deferred, so that pass is
not now.** The manifest stays byte-untouched.
**MEASURED STATE for whoever runs that pass: 85 pins OK, 31 stale, 401 rows
whose file is absent** (`.loop/` logs and historical citations). *Note the first
audit I ran reported "116 stale" — that was MY BROKEN LOOP (`shasum` not on PATH
inside it), not the tree. The corrected count is 31.* **A tool that cannot run
reports everything as broken; check the tool before believing the alarm.**

**The one row I DID fix stands** — `port/sim/target/check-device-target.sh`'s
three stale judge-sha references (`8658ae0b` -> `594f1925`). That is a plain
check script, not the reviewed manifest, and A7's lane flagged it as a blocker
it could not reach from its own lane.

### DEVICE LEGS OWED (added)
1. **The credits screen has NEVER RENDERED on the FunKey-S.** Needs a device
   shot and **Chase's eye at 240x240** — the warp field, the 1-px stars and the
   7-px reticle are all guesses about a real panel.
2. `check-device-foh.sh` / `check-device-persist.sh` judge-sha pins updated
   host-side but the checks were **not run**.
3. The four `m4-freeze-manifest.txt` rows above, in the batched pass.

## A45-T2 — DONE 2026-08-24 (D42/D43). **CUSTOM STAGES PLAY. NO EDITOR NEEDED.**

Path: `.mlstage` file -> `mlk_slot_load` (validates) -> `tp_setup_target_custom`
-> `tp_setup_target_core`; renderer via `gfx_target_init_custom`.

**THE SPIKE'S §5.2 WAS RIGHT and the lane verified it before building:**
`MlStageX` is a plain runtime struct and T1 had already modelled `MlkStage.s` as
the sim's OWN `Stage`, so the filler is **~20 lines**.

**IT TOOK THE CLASS FIX OVER THE ARM, and the reasoning is the transferable
part.** An `if (custom)` inside `tp_setup_target` would have left
`startTargetGame` **translated once and BRANCHED twice**. Instead
`tp_setup_target_core(...)` is the ONE translation and both entries route
through it, so **authored and custom stages differ in WHERE THEIR GEOMETRY CAME
FROM AND NOTHING ELSE — which is exactly what makes the done-check a
DIFFERENTIAL rather than a self-consistency test.** Same move in gfx:
`gfx_target_init_custom` materialises a runtime `ml_tstage_t` row, so **all ten
draw functions are byte-unchanged.**

**`.mlstage` contract:** three LF lines — `MLSTAGE1` / the share code /
`SUM <64 hex>` (sha256 over preceding bytes, **reusing `foh_persist.c:154`'s
idiom rather than inventing a second one**). Ten fixed slots
`custom<0..9>.mlstage`. **Validate on read, always:** bounded read, strict
anchored grammar, **SUM verified BEFORE parsing**, then `mlk_parse` +
`mlk_stage_playable`; every refusal names its rule.
**T2 has NO WRITER, deliberately** — files arrive by SD card. When T3/T4 needs
one it must **generalise `foh_persist_save`'s existing atomic publish
(`:506-551`), not grow a second write path.** Stated at the site, in
PORTABILITY and here rather than built, since `port/foh/` is a parallel lane.

**The four open questions, answered:**
- **`getConnected` NOT needed — and structurally, not luckily:** `connected` is
  not one of the grammar's 14 fields, so a decoded stage never had one to
  recompute. T7's problem.
- **The DAMAGE plane did NOT go live** — refused at load, naming T6 as owing the
  golden. **Subtlety worth keeping:** props with a **null** damageType are inert
  (physics tests truthiness) and are **exactly what upstream BUG 1 emits for
  every sixth surface** — so refusing those would reject codes the browser plays
  fine. Only a real type string refuses.
- **R2: REFUSE, DON'T RAISE.** Raising `ML_MAX_TARGETS` 10->20 breaks the
  `_Static_assert` tying it to **upstream's own 10-element `targetDestroyed`
  literal**, and no authored stage exceeds 9. The **builder** (T4) should refuse
  the 11th where the message is useful. **Owner's call; the safe half ships.**
- **D43 (the owner's clobber ruling) is fixed IN THE VALUE MODEL** — ten slots
  by index, no append, no length cursor. **Both clobber sites are structurally
  ABSENT, so the reload half is fixed too** — patching only the add path would
  have missed it, which is what the ruling brief warned about.

**Gates:** `CUSTOM STAGE PLAYS` · `SIM CONFORMS` 8/8 · `STAGECODE MATCH` ·
`TARGET SIM CONFORMS` (the refactor's regression). `oracle/`, `port/foh/` and
the freeze manifest byte-untouched.

**THE DIFFERENTIAL:** for t01/t02, re-express the AUTHORED stage as a code, play
it from a file, `cmp` the full outputs against the authored run, and judge BOTH
with the unchanged `verify-stream.js` / `verify-target-stream.js` against the
frozen goldens. **Both: `authored == custom, both == frozen, 2 targets
destroyed`.** Sound because the lane measured first that **all 210 numbers
across the 10 authored stages are exact at `toFixed(2)`.**

**FIFTH VACUOUS-TOOTH CATCH THIS SESSION:** the play tooth shifts every
collision surface 1.00 unit and requires the stream to move — **its first
version did not bite**, because it shifted ground surface 0, which on t01 is a
ledge at y=88 **the fox never touches.** Fifth razor-thin-nudge no-op measured;
recorded at the site.

### DEVICE-SCRIPT FINDING (driver, 2026-08-24) — relayed to lane P
`/usr/local/sbin/frontend` exposes **`frontend set gmenu2x|retrofe|none`** —
**an OFFICIAL verb for what the rig has been hand-rolling** as a raw
`/mnt/disable_frontend` write. And its `init_frontend()` loop confirms the
failure mode exactly: each pass does
`if [ -f "$DISABLE_FRONTEND_FILE" -o -f "$REBOOTING_FILE" ]; then ... sleep 5`.
**So the owner's device was never broken — it was doing what it had been told,
forever.** `/tmp` is tmpfs while `/mnt` is the SD card, which is the whole
reason the marker outlived the test. Two sibling files have the same
outlive-the-test property and are worth a grep: `/mnt/last_opk`,
`/run/rebooting`.

## LANE P — 2026-08-24. **A34 CLOSED, A26 ANSWERED, D44 CRASH-SAFETY LANDED.**
## And it corrected the driver TWICE.

### ⚠ CORRECTION 1 — MY CRASH-SAFE RULE WAS LITERALLY UNIMPLEMENTABLE
I made it binding that *"the marker must be removed BEFORE the test binary
launches, never after."* **The lane measured that this cannot work:**
`/usr/local/sbin/frontend:74-122` **re-reads the marker at the TOP of every loop
pass and sleeps only 5 s** — so removing it before launch brings gmenu2x back
onto the framebuffer within 5 s, **fighting the test for display and input.**
Two other obvious fixes are also unavailable, both measured: `/mnt` is **vfat**
(no symlink to a tmpfs marker), and `/run/rebooting` — the loop's other,
genuinely-tmpfs park condition — makes the loop **`break`** (`:120-122`), a
**one-way park that removal does not undo.**

**THE ACTUAL FIX IS AT THE OTHER END, AND IT IS BETTER THAN WHAT I ASKED FOR.**
`rig_boot_unpark_install()` appends one idempotent line to `/etc/rc.local`,
which `S03rclocal` runs from `rcS` **BEFORE the login shell ever starts
`frontend init`**. **No SD marker can survive a boot now — whoever wrote it,
whenever they died.** Called from `rig_inherited_restore` step 0 and **FAILS
CLOSED**: a run that cannot install *and verify* it **refuses to continue**
rather than parking a device it cannot guarantee recoverable.
**Why the class is impossible rather than unlikely: it no longer depends on any
script reaching its cleanup.** The worst a power cut can now produce is
*"power it back on"*.
**Proven both directions on the device:** line present -> marker set + reboot ->
`MARKER=GONE, GMENU running`; **tooth** (line cut) -> `MARKER=PRESENT,
GMENU=NONE` — **the owner's "bricked" splash reproduced, then undone.**

### ⚠ CORRECTION 2 — MY `frontend set none` SUGGESTION, MEASURED AND REJECTED
I relayed the OS's official verb as the better path. **Three measured reasons it
is strictly WORSE for this class:**
1. `$HOME` is relocated to `/mnt/FunKey` (`/root/.profile:51`) and
   `/mnt/FunKey/.frontend` **holds a real owner choice (`gmenu2x`)**. `set none`
   **OVERWRITES it** — a SECOND persistent SD file the boot-unpark line does not
   clear. And the unpark verb `set gmenu2x` **hardcodes a frontend the owner may
   not use.**
2. `dsh` runs a **non-login** shell where `HOME=/` and `/` is **ro** — measured:
   `frontend get` dies with `can't create //.frontend: Read-only file system`
   and **falls back to the default `retrofe`, which is not what is running**, so
   `set` would **pkill the wrong name** after already touching the marker.
3. It buys nothing: `set_frontend` is `touch`/`rm -f` plus a pkill the rigs
   already do.
**Sibling files clean** — nothing in `port/` writes `/mnt/last_opk` or
`/run/rebooting`.

### A34 — ROOT CAUSE: **`powerdown handle` IS NOT A SHUTDOWN VERB. IT IS CANCEL.**
`/usr/local/sbin/powerdown` has three: `schedule <delay>` (SIGUSR1 the recorded
app, wait, power down), **`handle` = `pkill -f "powerdown schedule"` — CANCEL a
pending shutdown so the app can take it over** — and `now` (the real one).
`foh_pause.c:570` called **`handle`**; with nothing pending it pkills nothing,
**and busybox `pkill` still exits 0** (measured), so **even the `!= 0` degrade
never fired.** The menu just closed. **That is the owner's symptom exactly.**
**BOTH LEADS IN MY ROW WERE FALSE:** the OPK runs as **uid 0** and inherits a
PATH containing `/usr/local/sbin` (read from the live process environ).
**Neither privilege nor PATH was ever involved.** One call site — no class.
**Fixed to `powerdown now`, and the done-check ran END TO END on hardware:**
OPK rebuilt and installed, launched from the frontend grid via `fk_input`,
drove MENU -> 3x down -> A, **and the device powered off** (adb gone, still down
80 s later — a poweroff, not a reboot).

### A26 — ANSWERED **WITHOUT** THE OWNER, by measuring instead of logging
**No probe was needed — the mechanism is directly observable.** Chain: lid ->
`fkgpiod` (built-in `KEY_POWER`/`KEY_SLEEP` -> `powerdown schedule 0.1`) ->
`kill -USR1 $(pid print)` -> **0.1 s** -> `powerdown now`. `pid print` reads
`/var/run/funkey.pid`, recorded by `frontend:106`.
**Measured while a game ran:** gmenu2x **execs** into `opkrun` -> the launcher,
preserving that pid. For sm64 it is the game binary. **For US it is
`/bin/sh mlfk-foh.sh` — the LAUNCHER SHELL — with `foh_device` as its child**,
because `mlfk-foh.sh` runs the binary in the foreground instead of `exec`ing it.
Sending exactly what `powerdown schedule` sends:
```
BEFORE:      launcher=ALIVE  app=ALIVE
kill -USR1 <launcher>
AFTER_USR1:  launcher=DEAD   app=ALIVE   (orphaned; funkey.pid erased)
```
**ANSWER: YES, a signal arrives — SIGUSR1 — but it lands on the LAUNCHER, not
the app, and its default disposition is terminate. World 2, so the cheap
`foh_persist` route is OPEN. No owner action needed to decide the shape.**
**Design constraint VERIFIED rather than assumed: POSIX `sh` DEFERS TRAPS until
the foreground child completes** (measured: fired 4 s late, exactly when
`sleep 4` ended). **A plain `trap … USR1` in `mlfk-foh.sh` would fire long after
the 0.1 s grace expires.** The fix must background `foh_device` and `wait` — the
platform's own `native_launch.sh` idiom — or handle it in C with the launcher
forwarding. **And the log must go to `/mnt`, not tmpfs: hibernate powers the
device OFF and `/tmp` is wiped.**

### OWED
- **`m4-freeze-manifest.txt:416` is STALE** — `riglib.sh` ->
  `5e9a8f302aa91eb5449aa29052c3b7a8b7b6904954f7b2b77aa6350adfbc8b26`. **The
  lane correctly did not touch it; neither did the driver this time.** Batched
  §A-par.5 pass, when M4 resumes.
- **ONE-TIME DEVICE MODIFICATION, on the record:** `/etc/rc.local` now carries
  `rm -f /mnt/disable_frontend || :   # MLFK-BOOT-UNPARK (D44)`. Idempotent,
  re-verified by any rig run, **and it cannot override a deliberate
  `frontend set none`** (that also writes `.frontend`, read independently).
- **A26 implementation is a SEPARATE ticket** (menu-level resume only;
  mid-match snapshotting is its own serialization surface). **Owner
  confirmation still nice-to-have, not blocking:** launch, close the lid,
  reopen — to confirm the lid really routes through `powerdown schedule` as
  `fkgpiod`'s table says.

## A44 MENUS HALF — DONE 2026-08-24 (D40/D41). **P3/P4 ARE LIVE. D6 IS RETIRED.**

**The ticket's premise was falsified in the GOOD direction.** A44's first lane
refused because `sim_setup_match` pinned slots 2/3 absent; **A46 removed that**,
so **DEVIATION D6 is RETIRED, NOT WEAKENED** — its own text named the condition
for its deletion (3/4-player conformance proven) and **`q01` met it.**

**Planes widened BY PORT, never by index** — selection `selChar[4]`, type
`portType[4]`, token `[FOH_CSS_PORTS]`. **Both use an ANONYMOUS UNION over the
old `p1Char..p4Char` names**, for two reasons worth keeping: a parallel lane's
witness reads `s.p1Char` and could not be touched, **and two hand-synced copies
is exactly CONTEXT.md's costliest defect class. Overlaid storage CANNOT
drift.** The CPU slider and difficulty were **deliberately left 2-wide** — no
CPU on 2/3 means no knob, and widening them would be dead width.

**PERSISTENCE: NO BUMP NEEDED — my brief was wrong.** I specified an
`MLFKPERSIST6`. Measured: `FohPersist` carries **no CSS character or type state
at all** (gameSettings, ctlStyle, modOnR, targetRecords, bind — and
`tapJumpOff[4]`/`bind[4]` are already 4-wide). **There was nothing to migrate.**

**Three deviations, each earned:**
- **D40(a)** — the one hand may grab ANY port's token. Upstream's
  `playerType[j]==1 || i==j` guard defends one human's hand from another's;
  **with one input device it defended nobody and removed the only route to
  giving a HUMAN port a character.** Without it P3 would be switchable-on and
  **permanently marth — the stub HARD RULE 2 forbids.**
- **D40(b)** — ports 2/3 cycle `N/A -> HMN -> N/A`. **CPU honestly absent**, and
  **asserted on the LAUNCH PLANE too, not merely missing from the widget.**
- **D41** — tokens go 2x2. **Four r=9 tokens span 78 px of a 44 px cell**, so
  the clamp would have parked port 0's token a cell LEFT of its own pick —
  **D21's defect re-created.** Discs are now tangent, so no point lies in two
  tokens and the grab loop's j-order cannot silently decide.

**LAUNCH grammar APPENDED, never renumbered** — and it caught a latent bug:
**`p2type` had to widen to admit `-1`**, because with ports 2/3 live *P1+P3 with
P2 off* is legal and `p2type=[01]` **was about to reject a launch the machine
performs.** Nine consumers updated with citations; **nothing loosened** —
`p3type/p4type` are `(-1|0)`, **tighter than `p2type`.** Probe count 1293 ->
1569, **accounted per plane, all up, none removed.**

### ⚠ A COVERAGE TRADE THE LANE FLAGGED FOR REVIEW — DRIVER REVIEWED IT, IT HOLDS
`check-hand.sh` leg [4]'s A25(c) purity differential compares the working tree
against a **PINNED PRE-EXTRACTION COMMIT**. D40 falsifies that comparison under
an A press: **34,150 of 60,000 frames diverge, first at frame 6001 where the
working tree carries PORT 3's TOKEN — machine state the base build HAS NO FIELD
FOR.** So requiring byte equality there would be **asserting something false
about the base build**, not catching a regression.
**Driver's review — three things make this a legitimate trade, not a loss:**
1. **The comparison stays TOTAL on the shared plane** — hand doubles, D3 clamp,
   the cell-hit predicate (hover selects with no button), selection plane, sound
   queue, event counts and ink plane, byte-for-byte over all 60,000 frames.
2. **The A-gated claim MOVED, it did not vanish** — `check-css-p34.sh` drives
   grab, drop and the type tabs through the real `foh_tick` on all four ports
   with its own negatives. **Verified present.**
3. **It guarded its own narrowing against going vacuous**: the two working-tree
   sweeps must DIFFER or `--no-a` is being ignored. **That is the
   vacuous-tooth class this session hit FIVE times, pre-empted by the lane
   itself.**

**Driver follow-up landed:** the lane flagged a **pre-existing** stale literal —
`check-judge-regression.sh` printed *"agree with all 69 authored rows"* while
live rows were 80 before its change. **It correctly refused to guess what "69"
counted** (the sentence names a SUBSET of planes, so the all-rows count is not
the right number either). **Applied the C13 precedent instead — the prose now
stops restating a number it does not compute.** `JUDGE REGRESSION OK`.

**All green on merged bytes:** `CSS P34 (3 teeth)` · `HAND` · `FOH FLOWS` ·
`JUDGE REGRESSION` · `CREDITS` · `SIM CONFORMS` 8/8.

### OWED
- **Device legs:** `check-device-foh.sh`, `check-device-persist.sh`,
  `check-device-fullgame.sh` (reads `f01-vs-g01.bstate.expect`, whose bytes
  moved), `verify_m3.sh`/`verify_m4.sh`, and an `install-play-opk.sh` reinstall.
- **Stale `m4-freeze-manifest.txt` rows** (driver-only, batched, NOT touched):
  `judge-foh-trace.js`, `normalize-foh-trace.js`, `check-device-foh.sh`,
  `riglib.sh`, and all 8 `flows/f0{1,2,3,5}*.{expect,bstate.expect}`.
- **DECISION AVAILABLE, not a blocker:** the play path links live `ai.c`, so a
  **CPU port 2 might run in PLAY even though AIBRIDGE1 cannot REPLAY it.** The
  refusal's ground is **verification, not capability** — now written down in
  three places. If the owner wants CPU on P3/P4 in play, that is a ratification
  about accepting an unreplayable configuration.

## OWNER RULINGS 2026-08-24 (round 4) — CPU on P3/P4, real persistence, puff walljump

**1. ENABLE CPU ON PORTS 3/4 (owner: *"yeah enable the CPu please"*).**
The driver's earlier refusal was **wrong in its stated ground**: `AIBRIDGE1` is
the RECORDED stream used to REPLAY a CPU golden deterministically — **it is not
what makes the AI run.** The play path links the LIVE `ai.c`
(`ml_sim_runai_live`; `AI LIVE CONFORMS` passes), so a CPU on port 3 should
simply work in play. **The refusal's real ground was VERIFICATION, not
capability**, and the driver flattened that into "refused at the sim level" —
then decided a scope question that belonged to the owner. Recorded as a driver
error, not a technical finding.
**ACCEPTED CONSEQUENCE, to be written at the site, not hidden:** 3/4-player CPU
matches are **playable but NOT checksum-verified**, because no golden can
currently replay more than one CPU slot.

**2. A48 (P2, NEW) — widen the AI bridge to more than one CPU slot.**
Owner: *"b file as a ticket please."* `AIBRIDGE1` holds one recorded stream for
one CPU slot (`port/sim/ai_bridge.h`). Widening it would let a multi-CPU match
be recorded as a golden and replayed bit-exactly, closing the verification gap
ruling 1 accepts. **May not be possible** — establish that first. Prerequisite
for a 4-port CPU golden.

**3. A49 (P1, NEW) — the CSS selection must PERSIST, and the pin must return to
the CHARACTER.** Owner: *"i want to MAKE it persistent... right now it just puts
the cursor back where it was when you left. I want it to be last character...
whenever the pin is let go of (going off) it should go back to the character you
had selected."*
**MEASURED: `FohPersist` stores NO character or port-type state at all**
(gameSettings, ctlStyle, modOnR, targetRecords, bind, volumes, tapJumpOff) — so
**picks have NEVER survived a restart, for ANY port including P1/P2.** This is
therefore a NEW feature, not a regression, and it covers all four ports.
**TWO OBSERVABLES, and they are distinct:**
   (a) the `selection plane` survives an app restart (a persist bump), and
   (b) **releasing the pin returns it to the SELECTED CHARACTER** — not to
       where it was dropped, not to the nearest cell.
**(b) IS THE D21/D35 FAMILY, THIRD INSTANCE.** Both prior bugs were the token
being re-homed from something other than the selection. **Read those two blocks
before touching this**, and re-home from `selChar[k]` only.

**4. PUFF WALLJUMP — OPTION 1 RATIFIED.** Owner: *"option1 please for puff wall
jump. falling seems like it would be the best?"* — i.e. **reuse an EXISTING puff
ECB rather than authoring new geometry**, with FALL as his suggested source.
**This keeps every number traceable to the executed-data pipeline**, which is
the property HARD RULE 5 protects; authoring 40 frames of invented collision
geometry would have been the project's first unverifiable engine data.
**The source state is a MEASUREMENT, not a guess** — FALL is the owner's
hypothesis and a good one, but the implementer must compare candidate puff
states against what marth's WALLJUMP ECB actually does and justify the pick.
**Second gap, stated: puff has NO WALLJUMP ANIMATION either** (measured: 0
occurrences in `anim_1_puff.bin` vs 1 for marth), so puff would walljump with no
walljump pose unless an existing animation is reused too.

## A49 — DONE 2026-08-24 (D45/D46). **CPU IS ON ALL FOUR PORTS. THE SELECTION PERSISTS. THE PIN COMES HOME.**

**Ticket 1 — CPU on ports 3/4 (D40(b) RETIRED, not weakened).** The refusal
was expressed in **seven** places and they came out together, because six of
them were consequences of the seventh:

| # | site | what it said | now |
|---|---|---|---|
| 1 | `foh.c` togglePort | `const int wrapAt = j < 2 ? 2 : 1;` | one cycle, `if (*t == 2) *t = -1;` for every port |
| 2 | `foh.c` launch guard | `cpuTooHigh` clause | deleted; the guard is TWO conditions (port 0 HMN, >= 2 participants) |
| 3 | `foh.c` knob-grab loop | `for (j = 0; j < 2; ...)` | `FOH_CSS_PORTS` — which is upstream's own `s < 4` (css.js:397) |
| 4 | `foh.c` / `foh.h` CPU-level plane | two scalars `p1Difficulty` + `difficulty` | `cpuDifficulty[FOH_CSS_PORTS]` under the SAME anonymous-union overlay selChar/portType use — upstream's own `[3,3,3,3]` (main.js:109) |
| 5 | `foh.c` level ev_sel | `k == 0 ? "p1difficulty" : "difficulty"` | per-PORT table `kCssDiffField[4]` (a ternary has room for two answers and there are four ports) |
| 6 | `foh_render.c` | `k < 2 ? foh_css_knob_x(s, k) : 0.0` | `foh_css_knob_x(s, k)` |
| 7 | `foh_launch_witness.c` | verdict table refused every CPU-above-port-1 cell | rows 4/5 uniformly `L`, row 3 refuses only its all-absent cell; **launch 11 -> 26, refuse 70 -> 55** |

**THE DIFFICULTY KNOB NOW DOES SOMETHING FOR PORTS 2/3, and it is witnessed
on the LAUNCH plane, not only in the widget.** `LAUNCH` and `BRIDGE-STATE`
gained **`p3difficulty` / `p4difficulty`** (APPENDED, nothing renumbered).
Without them a P3-CPU-at-level-1 match and a P3-CPU-at-level-4 match emit
byte-identical launch records — CONTEXT.md's "Seam" exactly, each side green
with nothing asserting the crossing. `p3type`/`p4type` widen from `(-1|0)` to
p2type's own `(-1|0|1)` in both the judge and the normalizer.

**ACCEPTED CONSEQUENCE, written at four sites rather than filed:** a 3- or
4-player match with a CPU on port 2 or 3 is **playable but NOT
checksum-verified** — the play path links the LIVE `ai.c` via
`ml_sim_runai_live`, but `AIBRIDGE1` replays ONE CPU slot so no golden covers
a second (**A48**). It is at `foh.c`'s launch guard, `foh_launch_witness.c`'s
verdict table, `judge-domains.authored.txt`'s LAUNCH header and MENU-SPEC
§2.7. It is deliberately NOT expressed as a narrower judge domain: a domain
row that lies about what the screen can emit is how a judge goes vacuous.

**Ticket 2(a) — DEVIATION D45, `MLFKPERSIST6`.** One appended
`sel <c> <c> <c> <c>` row after the v5 `bind` block; 68 -> 69 lines; v1..v5
all MIGRATE (a v5 file has no opinion about characters, so every port takes
the fresh-install marth while every setting, both control stamps, all four
bindings and all 50 target records carry forward). **The bump is proved by
its own round trip**, not by inspection: witness leg [9] saves to disk,
re-loads, asserts every v1..v5 plane survived, then BUILDS A GENUINE v5 FILE
from the v6 bytes (splice out `sel`, restamp the header, recompute the
SHA-256 seal) and requires the loader to migrate rather than reset it.

**ONLY the SELECTION plane is stored, and the answer on port TYPES is NO —
as a decision, not an omission.** Restoring types would boot the CSS already
READY TO FIGHT off a configuration the player last saw in another session;
upstream's fresh state is `playerType = [-1,-1,-1,-1]` with addPlayer arming
port 0, so not persisting them is the FAITHFUL answer too; and since ticket 1
made CPU reachable on ports 2/3, persisting types would make the *unverified*
configuration a device's DEFAULT BOOT STATE. The character survives; arming a
port stays an explicit act. Witness leg [9](iv) ASSERTS the boot state rather
than leaving the choice as prose. Save point = **leaving the CSS by either
exit**, through **one shared predicate `foh_is_save_point`** that both drivers
now ask — A31 measured what two hand-synced copies of that condition cost.

**Ticket 2(b) — DEVIATION D46, the family's third instance, and the
instrument had been asserting the bug.** `foh_cssbacksel_witness.c` leg [B]
carried, since A43, the line *"and that slot really does draw the token off
the pick ... the defect's precondition, reproduced"* — the leave-band rest
formula parking the pin one whole cell right of the selection. That WAS the
ticket. `foh_css_token_pos` now states ONE rule for all three rest paths
(re-homed from `cssChar[k]`, the token plane the hover arm writes in the same
statement as `selChar[k]`), because the failure mode this screen keeps having
is one of three agreeing formulas drifting. D41's clamp went with the
retired formula — every base is `cell_x(c) + DX`, widest case 226 < 240.

**TWO DISARMS FOUND AND NAMED rather than absorbed** (CONTEXT.md's "Tooth"):
1. **`check-css-backsel.sh` T2 went vacuous.** It deleted D35's rest re-home
   and required a mis-drawn pin; under D46 that deletion moves no pixel. The
   tooth MOVED onto the line that now carries the outcome (the rest base,
   perturbed back to upstream's leave-band formula for slot 1 only) and leg
   [B]'s expectation flipped from asserting the defect to asserting the fix.
2. **`check-hand.sh` leg [4]'s "6 distinct roster pairs" floor** fell to 5,
   because D46 moved where released tokens rest and every later grab in the
   sweep starts from a different pixel. Re-stated as what it is FOR and
   strictly stronger: **all five roster cells must be selected**, exhaustive.

`check-css-token-rest.sh`'s T1 was re-scoped (`rest == 2 ? k : c`) to stay
about the endGame snap alone now that one line serves all three paths.

**GREEN:** `CSS P34 CHECK OK (6 teeth)` · `CSS TOKEN REST CHECK OK` ·
`CSS BACK SELECT CHECK OK` · `CSS BACK CHECK OK` · `HAND CHECK OK` ·
`JUDGE REGRESSION OK` · `FOH FLOWS OK` · `CSS MODE CHECK OK` ·
`MEXIT REENTRY OK`.

**OWED (device legs, not run by this lane):** `check-device-foh.sh`,
`check-device-persist.sh` (the v6 format's device twin — the on-device
`mlfk-persist.dat` gains a line), `check-device-fullgame.sh`,
`verify_m3.sh` / `verify_m4.sh`, `install-play-opk.sh` reinstall so the
device build carries D45/D46 and the CPU ports.
**Stale `m4-freeze-manifest.txt` rows (driver-only, batched, NOT touched):**
`judge-foh-trace.js`, `normalize-foh-trace.js`, `check-device-foh.sh`,
`check-foh-flows.sh`, and all 8 `flows/f0{1,2,3,5}*.{expect,bstate.expect}`.

### DRIVER NOTE — a stale comment of the same class that cost the grab button
`port/foh/foh_persist.h:195` still reads `everyCharWallJump ... // DEAD, no sim
readers`. **D20 made it live** (`port/sim/physics.c:400`). Same class as
`s1_input.h`'s "Z: grab" comment, which was true of real Melee, false of this
engine, and shipped a dead button. **A comment is not evidence.**

## PUFF WALLJUMP — DONE 2026-08-24 (D47). **AND #16 IS CLOSED.**

**THE SOURCE ECB IS `WALLTECHJUMP`, NOT `FALL` — and the measurement is the
whole ticket.** For **all FOUR characters that HAVE a WALLJUMP ECB** (marth,
fox, falco, falcon), their **WALLTECHJUMP ECB is BYTE-IDENTICAL to it** — 40
frames, all 160 coordinates, **four independent instances.** So this is not a
resemblance argument: **upstream's own data says a wall-tech-jump ECB IS a
walljump ECB**, and the reused number is the one upstream WOULD have authored,
not an approximation of it. Puff has WALLTECHJUMP with **45** ECB frames against
the **40** that `framesData["WALLJUMP"]` clamps to (`physics.c:1456`) — it
covers.

**THE OWNER'S `FALL` HYPOTHESIS IS REJECTED, AND HIS QUESTION MARK WAS THE RIGHT
INSTINCT.** **Puff's FALL ECB is 8 FRAMES.** Walljump frame 9 would trap
`ecb frame out of range` — **mechanically impossible, not merely a worse fit.**
It also ranked **7th** when marth's own states were ranked by mean
per-coordinate distance to marth's WALLJUMP ECB; **WALLTECHJUMP ranked 1st at
distance exactly 0**, which is how the lane found it.

**HOW IT SURVIVES REGENERATION — it never touches the generated tables.**
`pipeline/stages/tables.js` serializes upstream verbatim and `check-tables.sh`
round-trips the generated C against a fresh executed-JS walk, so a hand-added
puff ECB would be **both erased by the next `node pipeline/run.js` AND a house
rule disguised as upstream data.** The alias lives at the DEVIATION SITE: a new
`walljump_ecb()` in `physics.c` is **the single owner of "which ECB does a
walljump use", called by BOTH the ability gate and the per-frame lookup.**
**That shared owner is deliberate — gate-and-lookup drift is precisely how D20
shipped its crash.**

**ANIMATION ADDRESSED, NOT DEFERRED.** Puff has **0** WALLJUMP occurrences in
`anim_1_puff.bin` and **1** WALLTECHJUMP — same reuse, same measurement, 4 lines
in `gfx_render.c`. Without it **puff would walljump INVISIBLY for up to 40
frames**: the existing `if (!as) return` is upstream-faithful for a state
upstream cannot reach, **but D20 can now reach it.** Inert for the four
characters with their own pose.

**#16 — THE D20 MARTH WITNESS — IS CLOSED.** flag-off
`JUMPAERIALB -> FALLAERIAL`; flag-on **`WALLJUMP` from frame 186.** **The D20
marth path has now executed for the first time.** It needed a DOUBLE JUMP to
hold marth beside the wall — **marth and fox otherwise fall straight past
ystory's wall and SD, which is why six walk-and-drift patterns found zero wall
frames in August.**

**THE RIG PROVES ITS INSTRUMENT ON A POSITIVE FIRST — the thing the 2026-08-05
attempt skipped:** **fox walljumps on the witness trace with NO house rule at
all** (`attributes.walljump` is 1 upstream). **Only then does it trust a null.**
That is the direct answer to this session's five vacuous teeth.

**Results:** `WALLJUMP D47 OK` — which INCLUDES `SIM CONFORMS`, 8/8
`STREAM MATCH`, 3600/3600 exact, **flag-off bit-identical** · `TABLES OK`
(38,832 leaf values bit-exact). Both re-verified by the driver on merged bytes.
**Both teeth bite** (run, then reverse-edited — not `git checkout --`): deleting
the fallback in `walljump_ecb()` drops puff 3 -> 0 walljump frames; deleting it
in `ecb_state()` **reproduces D20's shipped crash EXACTLY** —
`SIM FATAL frame 187: ecb: unknown action state`, one frame after the walljump
fires.

### DRIVER ITEMS FROM THIS LANE
1. **`oracle/harness/.gitignore` has `node_modules/` WITH A TRAILING SLASH**, so
   it matches directories but **not a symlink** — which is exactly the **A36**
   mechanism. The lane suggested a one-char fix. **THE DRIVER IS NOT MAKING IT:
   that file is under `oracle/`, which is READ-ONLY outside M0 (HARD RULE 3).**
   Registered instead — the same discipline given to every lane, and the same
   trap the driver already fell into once today on the freeze manifest.
2. **`port/foh/foh_persist.h:195` still reads `everyCharWallJump ... // DEAD, no
   sim readers`** — **now DOUBLY false** (D20 gave it sim readers; D47 widened
   them). In the parallel lane's file; owed once that lane merges.
3. **NEW GOTCHA CLASS, cost the lane one run: editing a bash script WHILE IT IS
   EXECUTING corrupts the run** — bash re-reads by byte offset, so a mid-run
   comment insert made it execute fragments and exit 1 **after a fully green
   `SIM CONFORMS`**. **Signature: a green sub-result followed by a nonsense
   failure.**

## A49 — DONE 2026-08-25 (D45/D46). CPU ON P3/P4 · `MLFKPERSIST6` · THE PIN RETURNS TO THE PICK.

### Ticket 1 — the CPU refusal was expressed at SEVEN sites, six of them consequences of the first
`foh.c`'s `togglePort` had `const int wrapAt = j < 2 ? 2 : 1;` — **one ternary
that six other sites had been written to agree with.** Now one cycle for all
four ports. The launch guard's `cpuTooHigh` clause is **deleted** (the guard is
now two conditions: port 0 HMN, >= 2 participants); the knob-grab loop is
`FOH_CSS_PORTS`, **which is upstream's own `s < 4`** (`css.js:397`); and the
CPU-level plane went from **two scalars** (`p1Difficulty` + `difficulty`) to
`cpuDifficulty[FOH_CSS_PORTS]` under the same anonymous-union overlay `selChar`
uses — **upstream's `[3,3,3,3]`** (`main.js:109`).
**Note the shape of the smallest fix:** `k == 0 ? "p1difficulty" : "difficulty"`
became a per-port table, **because a ternary has room for two answers and there
are four ports.** That is the 2-vs-4 defect class in one line.

**The knob now DOES something for ports 2/3, and it is witnessed on the launch
plane:** `LAUNCH`/`BRIDGE-STATE` gained `p3difficulty`/`p4difficulty` —
appended, nothing renumbered — **because otherwise a P3-CPU-at-1 and a
P3-CPU-at-4 match emit BYTE-IDENTICAL launch records.** `foh_launch_witness.c`'s
verdict table went **launch 11 -> 26, refuse 70 -> 55.**

**The accepted consequence is WRITTEN AT FOUR SITES, not filed away:** a 3/4-
player match with a CPU on port 2 or 3 is **playable but NOT checksum-verified**
(live `ai.c` plays it; AIBRIDGE1 replays one CPU slot — **A48**). It sits at the
launch guard, the witness verdict table, the authored-domain LAUNCH header and
MENU-SPEC §2.7. **Deliberately NOT expressed as a narrower judge domain — "a
domain row that lies about what the screen can emit is how a judge goes
vacuous."**

### Ticket 2(a) — `MLFKPERSIST6`, and it is PROVED BY ROUND TRIP, not inspection
One appended `sel <c> <c> <c> <c>` row after v5's `bind` block; 68 -> 69 lines;
**v1..v5 all migrate.** Leg [9] saves, reloads, asserts every v1..v5 plane
survived, **then BUILDS A GENUINE v5 FILE OUT OF THE v6 BYTES** — splice out
`sel`, restamp the header, recompute the SHA-256 seal — **and requires migration
rather than reset.**

### The design question — persist port TYPES? **NO, and it is ASSERTED, not prose**
Three independent reasons: restoring types would **boot the CSS already READY TO
FIGHT off another session's configuration**; upstream's fresh state is
`playerType = [-1,-1,-1,-1]` with `addPlayer` arming port 0
(`main.js:107,:495`), **so not persisting is also the FAITHFUL answer**; and
since ticket 1 it would make the **unverified CPU configuration a device's
DEFAULT BOOT STATE.** Leg [9](iv) asserts the boot state.

### Ticket 2(b) — **THE INSTRUMENT HAD BEEN ASSERTING THE BUG SINCE A43**
`foh_css_token_pos` now states **ONE rule for all three rest paths**: a resting
token is drawn on the cell of `cssChar[k]` — the token plane the hover arm
writes **in the same statement** as `selChar[k]`. At boot,
`foh_persist_apply` sets `cssChar[k] = p->selChar[k]` — **re-homed from the
selection, never a pixel, a nearest cell, or a port index.** D41's clamp retired
with the old formula.
**And the tell was already in the tree:** `foh_cssbacksel_witness.c` leg [B] had
carried *"and that slot really does draw the token off the pick … the defect's
precondition, reproduced"* **since A43.** The instrument had been describing the
owner's bug for two days. **That was the ticket.**

### TWO MORE DISARMS FOUND AND NAMED — sixth and seventh this session
1. **`check-css-backsel.sh` T2 went VACUOUS** — D46 subsumed D35's display half,
   so deleting D35's rest re-home **moves no pixel.** Moved onto the line that
   now carries the outcome; leg [B]'s expectation **flipped from asserting the
   defect to asserting the fix.**
2. **`check-hand.sh`'s "6 distinct roster pairs" floor fell to 5** — D46 changed
   where released tokens rest, so every later grab starts from a different
   pixel. **Re-stated as what it is FOR, and strictly stronger: all five roster
   cells must be selected, exhaustive.**

**Green:** `CSS P34 CHECK OK (6 teeth — 3 -> 6, each in a DIFFERENT function)` ·
`CSS TOKEN REST` · `CSS BACK SELECT` · `CSS BACK` · `HAND` · `FOH FLOWS` ·
`JUDGE REGRESSION` · `CSS MODE` · `MEXIT REENTRY`. Re-verified by the driver on
merged bytes.

### DRIVER FOLLOW-UPS
- **`foh_persist.h:195`'s "DEAD, no sim readers" comment is CORRECTED** — it was
  stale by TWO deviations (D20 gave it a sim reader, D47 widened it). **Two
  separate lanes flagged it and neither could touch the file**; `port/foh` was
  free after this merge. Same class as the `z`-is-grab comment.
- **`check-live-arms.sh` re-run on a clean tree** — the lane's run 1 passed every
  leg and tooth and tripped only the closing tree-quietness guard, **because it
  committed mid-flight; the check's own text names that as cause (b), "not a rig
  failure".** *(The driver nearly repeated it: the comment fix above was
  uncommitted when the re-run started. Committed first.)*
- **Device legs owed** (unchanged list, plus): `check-device-persist.sh`'s
  grammar was **updated to v6 host-side — 1602 -> 1614 bytes, 69 lines,
  positional `sel` row, `v6_defaults()`, version tooth moved v6 -> v7 — and is
  UNRUN.**

## A14 SECOND HALF — DONE 2026-08-25 (D48/D49). THE MENUS DRAW FROM THE BROWSER ATLAS.

**The MEASURE-FIRST step delivered and it earned itself twice.** Cap ascent read
from the atlas via `'H'`: font 0 = 6, font 3 = 11, **and font 0's ink cell is
EXACTLY 9 device px — precisely the retired 6x9 face's cell at scale 1.**
Every screen shifts: **upright text shrinks uniformly 0.67-0.88**; italic text
(menu bars, mode titles, CSS header) **moves in BOTH directions** because font 3
has only one size. **Nothing over budget** — tightest is `TARGET BUILDER` at
123.7 px in ~128 px of bar.
**Two things the measurement caught that guessing would not:** font 3 at `up=2`
would set mode titles **24 px tall in an 18 px budget**, so `up=1` is the only
integer that fits; and font 3's `'M'` declares a 19-row box with **SEVEN BLANK
ROWS ON TOP**, so `max(-dy)` is not a usable ascent — hence reading `'H'`.

**The seam is an EXTRACTION, confirmed** — parser, pool, grammar and every loud
failure moved **byte-unchanged**; `gfx_glyph_text`/`gfx_sprite_blit` are
one-line delegations; **no coordinate conversion anywhere**, which was the named
failure mode.
**One FORCED deviation from the brief's letter, and it is measured not taste:**
the atlas is a **new TU** rather than staying in `gfx_overlay.c`, because
`gfx_overlay.c -> gfx_vfx.h -> gfx.h -> sim.h -> ml_stages.h` — **linking it
into a menu witness would have made `check-legibility.sh` depend on
`node pipeline/run.js`.** Measured: `cc -Iport/sim port/gfx/gfx_overlay.c` fails
with `'ml_stages.h' file not found`.

**D49 — binary coverage, and it protected the instrument instead of the run.**
Atlas masks are antialiased, and **source-over at partial alpha is not
idempotent**, so overdrawing AA text ALWAYS changes pixels —
`check-controls-labels.sh` reported 388-474 px of "difference" on labels it was
looking straight at. **Rather than weaken the overdraw instrument, the lane made
the FOH blit binary** (the HUD keeps its AA). **That is the right way round:
the instrument is load-bearing across five checks; the AA was not.**

**`foh_text` DELIBERATELY NOT SWAPPED, though the brief named it** — and the
reason is the best catch in the ticket: **`check-foh-flows.sh`'s banner tooth
requires `'?'` to be ABSENT from face 1, and atlas font 0 HAS it. The swap would
have SILENTLY DEFUSED THE TOOTH.** `decode-pb-glyphs.js` also reads device shots
through `kGlyphs[]`, and `check-device-foh.sh`'s `VSF_BANNER_*` are face-1
metrics. **The tooth is confirmed still biting in the passing run.**

**Face 2 retained as a LOUD fallback**, entry points renamed
`foh_text2_face2*` "so nothing reaches the retired face by habit". Missing glyph
is `gfx_fatal` on both paths. **6x9 face NOT deleted.**

### ⚠ A REGRESSION I FAILED TO CATCH — A49 BROKE `check-rebind.sh`, AND MY VERIFICATION MISSED IT
The lane reported `check-rebind` failing and called it pre-existing. **It was
right, and the driver verified it at HEAD before merging anything: 4 assertions
failed, all v4-migration.** **I merged A49 having run five CSS checks and NOT
this one — the single check most coupled to persistence, since bindings live in
the persist file. A49's own lane did not run it either.** *A check list assembled
from the files a change touches will miss the checks that read what it WROTE.*

**Diagnosed rather than assumed, because "stale test" and "real data loss" look
identical from the failure text.** The witness **synthesises a v4 file out of the
CURRENT one** by stripping rows a v4 never had. A49 appended `sel`; the
constructor stripped only `bind`. **So the fixture was a v4 header over v6
content and the loader rightly refused it.** **A real v4 file from a real old
device never had either row and still migrates — no data-loss bug.**
**Fixed by strengthening, not relaxing:** the constructor now strips `sel` too,
the republish assertions move v5 -> v6 in **both** the witness and the check's
own grammar pin, **and a NEW assertion was added** — the fixture must have
dropped exactly one `sel` row, so **the next version bump fails HERE, loudly,
instead of silently producing a malformed fixture again.** `REBIND TOOTH OK`.

### OWED
- **`check-live-arms.sh` IS KNOWINGLY BROKEN BY THIS CHANGE** — it derives the
  system overlay's expected "VOLUME" bitmap by parsing `kGlyphs2[]` out of
  `foh_font.c`, **a face the renderer no longer draws.** Needs an atlas-based
  decoder. **The lane did NOT write one — unverifiable without a device, and
  "shipping untested decoder JS is worse than naming it."** The 6x9 table stays
  until that lands. *(Note it was settled GREEN on a quiet tree immediately
  before this merge — `LIVE ARMS OK`, 15 teeth — so the break is attributable
  to A14 alone.)*
- **All 15 menu shots + device twins need re-freezing** — glyphs legitimately
  moved.
- Unrun after link-list edits: `check-device-{foh,persist,audio,music,render,fullgame,target}.sh`,
  `riglib.sh`, `check-render.sh`, `check-mixer-fidelity.sh`, `check-ctl-input.sh`,
  `check-mexit-reentry.sh`.
- **Stale `m4-freeze-manifest.txt` rows** (driver-only, batched, NOT edited):
  `riglib.sh`, `check-device-fullgame.sh`, `check-device-target.sh`,
  `check-device-foh.sh`.

### ⚠ FOR THE OWNER'S ACCEPTANCE PLAYTHROUGH — D48's real cost
**The size hierarchy COLLAPSED: mode titles and menu-bar labels now render at
the SAME height, where upstream sets its title larger.** Restoring the step
needs **a second italic capture in the atlas — a DATA-plane change, not a
renderer one.** `foh_text2` is the one function where the metrics move.

*(Lane note, recorded because it explains a non-linear diff: it truncated
`gfx_overlay.c` and `foh_font.c` to zero mid-session with a bad Python heredoc
(`b'''` parsed as a bytes literal), recovered both from git and re-applied. The
final files are verified by the green run.)*
