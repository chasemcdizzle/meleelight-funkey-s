# fix_plan — the loop's priority queue

Current phase: M2

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
- **Skipped-by-judgment util files** (documented, not silent):
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
5. physics.js core + interpolatedCollision — the per-player update
   pipeline (mutation-heavy; pre/post player + stage).
   done-check: `bash port/sim/calib/check-physics-replay.sh` → prints
   `PHYSICS MATCH`, exit 0.
6. hitDetection + hitQueue + hitbox value model (createHitBox/
   createHitboxObject) — queue-ordered resolution, checkPhantoms,
   executeHits; launch-angle transcendentals via fdlibm.
   done-check: `bash port/sim/calib/check-hitdet-replay.sh` → prints
   `HITDET MATCH`, exit 0.
7. characters/shared moves (5,088 ln, 80 files) — move-object template
   {name,init,main,interrupt}; boundary = every move's init/main/
   interrupt with player pre/post-state.
   done-check: `bash port/sim/calib/check-moves-shared-replay.sh` →
   prints `MOVES shared MATCH`, exit 0.
8. characters/fox moves (4,450 ln).
   done-check: `bash port/sim/calib/check-moves-fox-replay.sh` →
   `MOVES fox MATCH`, exit 0.
9. characters/falco moves (4,488 ln).
   done-check: `bash port/sim/calib/check-moves-falco-replay.sh` →
   `MOVES falco MATCH`, exit 0.
10. characters/falcon moves (4,432 ln).
    done-check: `bash port/sim/calib/check-moves-falcon-replay.sh` →
    `MOVES falcon MATCH`, exit 0.
11. characters/marth moves (5,659 ln).
    done-check: `bash port/sim/calib/check-moves-marth-replay.sh` →
    `MOVES marth MATCH`, exit 0.
12. characters/puff moves (4,599 ln).
    done-check: `bash port/sim/calib/check-moves-puff-replay.sh` →
    `MOVES puff MATCH`, exit 0.
13. articles (article.js 639 — fox/falco lasers, ILLUSION etc.) — article
    queues + article hit detection; articles are checksummed
    (CHECKSUM.md §2 `articles` key).
    done-check: `bash port/sim/calib/check-article-replay.sh` → prints
    `ARTICLE MATCH`, exit 0.
14. movingPlatforms stage-tick logic (ystory/fountain updatePlatform +
    pstadium if live) — the M1-externals-stubbed god-module bodies;
    stage pre/post-state capture.
    done-check: `bash port/sim/calib/check-platforms-replay.sh` →
    prints `PLATFORMS MATCH`, exit 0.
15. ECMAScript shortest-float formatter + CHECKSUM.md `ser` in C —
    Ryu/Grisu-class `String(x)` (incl. `-0` token + exponent-form rules)
    + the §3 envelope serializer + SHA-256 per frame; differential
    done-check vs JS `String(x)` over every double in the captured
    player/article snapshots PLUS an adversarial generated sweep
    (subnormals, exponent boundaries, shortest-repr torture cases).
    done-check: `bash port/sim/calib/check-format.sh` → prints
    `FORMAT MATCH`, exit 0.
16. AI-input bridge for CPU goldens — write-set recon hard-check, then
    capture per-frame aiInputBank + RNG-draw counts over g07/g08
    (STREAM MATCH guarded); emit replayable bridge artifacts + pins.
    done-check: `bash port/sim/calib/check-ai-bridge.sh` → prints
    `AI BRIDGE OK`, exit 0.
17. integration: the full C gameTick — flattened game-state struct,
    trace loader (oracle trace JSON), M1 data consumption (CTAB1 tables,
    STAB1 stages, ANIM1 frame counts for timer clamps), mode-3 tick
    order (main.js:1039-1080: resetHitQueue → movingPlatforms →
    destroy/executeArticles → [interpretInputs+update]×4 →
    checkPhantoms → hitDetect×4 → executeHits → article hits →
    matchTimerTick), boot-RNG draw parity (rngCallsOutsideStep == 1),
    checksum-stream emission via task 15's serializer, headless platform
    TU (+ SDL2 dev TU per CLAUDE.md seam policy); replay ALL 8 goldens
    end-to-end vs the ORACLE STREAMS.
    done-check: `bash port/sim/check-sim.sh` → prints `SIM CONFORMS`,
    exit 0 — this is the M2 exit gate command (§Commands).)

## M3 — On-device at 60 fps

(seed items — REPLAN concretizes on phase entry: platform seam SDL1.2 TU,
S1 input table wiring, OPK packaging, ADB conformance + perf rig, Chase
ratification playtest escalation)

## M4 — Full-game parity

(seed items — REPLAN concretizes on phase entry: thin C front-of-house
menus, audio mixer per PLAN §7, ai.js port (+tan to fdlibm surface),
target test, SD persistence, full-game trace suite, Chase acceptance)
