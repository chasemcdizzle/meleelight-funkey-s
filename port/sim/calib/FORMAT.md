# port/sim/calib — module-boundary capture format (M2-CAL, generalized for M2)

Records every call crossing the exported boundary of a capture SPEC's
modules during an oracle-harness replay of a golden trace: arguments +
return value, per frame. The C translations are replayed against these
records bit-exactly by the spec's replay driver.

Specs (`node run-capture.js --spec <name>`):
- `envcoll` (default, M2-CAL): `src/physics/environmentalCollision.js`
  (13 exports) → `replay_envcoll.c`. CLI/filenames/meta unchanged from
  the M2-CAL rig (regression-proven: post-generalization fresh capture is
  byte-identical to the frozen one).
- `util` (M2 task 1, `spec-util.js`): the util/math substrate — Vec2D
  module (getXOrYCoord/putXOrYCoord/flipXOrY + prototype method
  `Vec2D#dot`; the trivial two-field constructor is deliberately not
  wrapped), full linAlg (11), solveQuadraticEquation, lineAngle,
  findSmallestWithin (2), extremePoint, ecbTransform (5), zipLabels,
  toList, detectIntersections (4), Segment2D (constructor `Segment2D.new`
  + per-instance closure methods `Segment2D#segLength`/`#project`,
  recorded with [thisState, ...args]) — 34 wrapped functions →
  `replay_util.c`. Pins: `expected-capture-util.json` via
  `check-spec-pins.js`.
- `player` (M2 task 2, `spec-player.js`): per-frame post-`update(i)`
  player snapshots for the player VALUE MODEL (`port/sim/ml_player.h`) —
  wraps physics.js's exported `physics(i, inputBuffers)`, the tail
  statement of main.js `update(i)` (update itself is called through the
  local module binding inside gameTick and is NOT namespace-wrappable;
  post-physics state == post-update state). MUTATION-CAPTURED: the
  return is void, the value is the post-state field (below) = canon of
  the projected `player[i]` after the call. Replay:
  `replay_player.c` (canon→struct→canon round-trip + deep-copy
  independence probe + deepObjectMerge property check). Pins:
  `expected-capture-player.json`.
- `input` (M2 task 3, `spec-input.js`): the input cluster — input module
  (pollInputs/inputData/nullInput/nullInputs), all 6 meleeInputs exports,
  and `physics` args-projected to `[i, inputBuffers[i]]`: interpretInputs
  (main.js:668) is main.js-internal to gameTick (direct calls in the
  built bundle, not namespace-dereferenced) so its OUTPUT is captured at
  the physics boundary — inputBuffers[i] is exactly the 8-deep buffer
  interpretInputs returned for slot i this frame (or gameTick:919's fresh
  nullInputs() during the ~90-frame 'starting' window; the same
  wrappability trick as the player spec). Replay: `replay_input.c` —
  pure records marshal→call→compare; the CHAIN records (a pollInputs
  ret = the injected per-frame input, the following physics args = the
  expected buffer) drive the C interpretInputs state machine
  (`port/sim/input/`) over the full 3600-frame recurrence, chained from
  the C values, never the capture's. Pins: `expected-capture-input.json`.
  Value bridge: `input_canon.{h,c}` (strict 22-key MlInput marshal +
  serializer, reusable by tasks 5/16).

- `asshort` (M2 task 4, `spec-asshort.js`): all 29 exported functions of
  `src/physics/actionStateShortcuts.js` + the seeded-PRNG draw stream +
  the sound-event/dispatch-event queues → `replay_asshort.c` (translations
  `port/sim/action_state_shortcuts.{c,h}`, seams `port/sim/ml_events.{c,h}`
  + `port/sim/ml_rng.h`). See "The asshort spec" below. Pins:
  `expected-capture-asshort.json`.

- `physics` (M2 task 5, `spec-physics.js`): physics.js core +
  interpolatedCollision → `replay_physics.c` (translations
  `port/sim/physics.{c,h}` + `port/sim/interpolated_collision.{c,h}`).
  See "The physics spec" below. Pins: `expected-capture-physics.json`.

Method records use `Mod#method` names (args prepended with the canon of
`this`); constructor records use `Mod.new` (ret = canon of the built
instance, per-instance closures serializing as `fn`). Argument canon is
computed BEFORE the call (pre-call state — mutation-safe for later
specs); records are pushed after return, so nested boundary calls appear
before their caller's record.

## Capture mechanism (summary; rationale in fix_plan §M2-CAL)

The runner (`run-capture.js`) serves the untouched built dist, but the
served bytes of `dist/js/main.js` get ONE textual injection into the
webpack bootstrap — `var installedModules = {};` gains
`window.__wpCache = installedModules;` — nothing on disk changes and no
sim code changes. After page load (before `setupMatch`), `capturelib.js`
locates the environmentalCollision module object in the cache by its
export signature and replaces each exported function with a recording
wrapper. Babel-CJS exports are plain writable properties and every
external call site dereferences the module namespace at call time, so all
cross-module calls are captured; internal calls use local bindings and are
(correctly) not.

Non-perturbation guard: each capture run emits a normal harness run JSON
whose checksum stream MUST pass the unchanged
`oracle/harness/verify-stream.js` against the frozen golden stream.

## Synthetic-domain sweep (M2 task 3; fix_plan §M2 rule 11)

A spec MAY define `sweep()`: a DETERMINISTIC (fixed literal values, no
RNG) set of extra calls through the WRAPPED exports, which
`run-capture.js` executes after install and BEFORE `setupMatch` — the
records land at frame 0 ahead of every sim-step record and are real
executed-upstream calls, replayed like live ones. Purpose: boundary
functions with ZERO live records over the goldens but a reachable later
domain (the input spec's meleeInputs surface — device sticks in M3 —
gets 1,780 sweep records spanning every engine threshold in
docs/research/b0xx-mapping.md §3.1, both zeros, quantization ties, and
the degenerate-cardinals fallback) get bit-exact verification instead of
translate-and-hope. Sweeps must be pure (no sim/RNG state): the
verify-stream guard judges the whole run regardless.

## JSONL record format

One line per boundary call, tab-separated, in call order:

```
<frame> TAB <fn> TAB <args-canon> TAB <ret-canon> [TAB <post-canon>] LF
```

- `frame`: 1-based sim frame being ticked (`__frameCount + 1` while
  `__inStep`); `0` = outside a sim step (match setup / stage init).
- `fn`: the exported function name.
- `args-canon`: canon-v1 serialization of the argument list (as an array).
- `ret-canon`: canon-v1 serialization of the return value.
- `post-canon` (mutation-capture, M2 task 2): present for exactly the
  functions frozen in the spec's expectations `postStateFns` — boundaries
  that mutate arguments/globals record the canon of the (projected)
  mutated state AFTER the call returns; the replay driver marshals
  pre-state, calls C, and compares ret AND post-state bit-exactly.
  Void mutators additionally appear in `undefRetAllowed` (their ret is
  the literal `undef` by construction — the value is the post-state).
  Post-state canon MAY contain `undef` (undef-at-rest fields are modeled,
  rule 8); the no-undef pin applies to the ret field only. 4-field
  records are byte-identical to the M2-CAL format.

## canon v1.1 — value serialization

CHECKSUM.md §3's structural rules with ONE deviation: numbers are IEEE-754
bit patterns, not shortest-round-trip decimals.

- number → `d:` + 16 lowercase hex digits (big-endian IEEE-754 double bit
  pattern), with ALL NaNs collapsed to the canonical quiet NaN
  `d:7ff8000000000000` (v1.1, M2 task 1). Injective on JS number VALUES:
  distinguishes `0`/`-0` and every finite/infinite double bit-exactly — a
  single-ulp difference is a divergence. NaN payloads are collapsed
  because they are semantically unobservable in the sim domain
  (`String(NaN) === "NaN"` in the real checksum stream; no typed-array
  aliasing of sim values) and non-reproducible: V8 emits payload NaNs
  from undefined-arithmetic (0xfffefffffff6ffff observed) and propagates
  them by its own evaluation order, while C compilers may legally commute
  FP adds since payloads are unspecified — measured: 24 solveQuadratic
  divergences under v1 raw-bit NaNs, 0 under v1.1; and v1.1 is a measured
  no-op on the frozen envcoll captures (zero NaNs in g01/g04/g06).
- string → `JSON.stringify` (double-quoted, JSON escaping)
- boolean → `T` / `F` · null → `null` · undefined → `undef` ·
  function → `fn`
- array / typed array → `[` elements `,`-joined `]`
- object → `{` own enumerable keys sorted (byte-wise; all keys ASCII),
  each `"key":value`, `,`-joined `}`
- ancestor cycle → `cyc` (path-based, per CHECKSUM.md §3.9; the domain
  here is trees, so `cyc` never legitimately appears)

Deviation rationale (PROVISIONAL, REPLAN iter 15): the ECMAScript
shortest-float formatter is a known one-time M2 component priced
separately; the calibration measures the TRANSLATION divergence rate, and
bit-pattern comparison is equally injective and strictly as sensitive.

## The stage argument (projection rule)

`runCollisionRoutine`'s `stage` argument is captured as its module-read
projection `{ceiling, ground, platform, wallL, wallR}` — the module
dereferences exactly these five surface lists (verified by grep over the
module source; everything else on the upstream Stage object is
render/blastzone/moving-platform machinery the module never reads).
Captured at call time, so per-frame moving-platform mutations (fountain)
are recorded faithfully.

## Player spec projections (M2 task 2)

The `player` post-state snapshot is `player[i]` with exactly three keys
projected out (spec-player.js; the same projection discipline as the
stage argument above):

- `charAttributes` / `charHitboxes` — immutable per-character table data;
  the C sim consumes the M1-generated tables (ml_tables, CTAB1), never a
  snapshot copy. Zero in-match writes upstream (the only assignments are
  menu-time css.js:150/171).
- `percentShake` — written OFF-TICK by wall-clock setTimeout callbacks
  from the stashed NATIVE RNG (oracle/CHECKSUM.md §7: the one player
  field excluded from the checksum surface). Including it would make
  capture byte-stability timing-dependent by construction.

The `physics` args are projected to `[i]` (slot index only): the
inputBuffers argument is the input cluster's surface (fix_plan §M2 tasks
3/5 capture it at their own boundaries).

## The asshort spec (M2 task 4)

Args are per-function READ-SET projections (the stage-argument discipline
below): the exact player/global fields the function dereferences, verified
against the module source and frozen in `spec-asshort.js`'s BOUNDARY
table. Common projections: `input` → `input[p].slice(0,4)` (the module
reads history depth ≤ 3; entries reuse the input spec's 22-key Input
canon/marshal); `characterSelections[p]` → a `char` id arg (the C side
reads charAttributes/intangibility from the M1 CTAB1 tables `ml_tables`,
never from snapshots — this cluster is the first consumer of the generated
data path); `gameSettings["tapJumpOffp"+(p+1)]` → the read VALUE (the
god-module settings slice is task 17's; the captured domain is the number
0 and the upstream `== false` loose-eq is therefore `v == 0`);
`actionSounds[char][state]` → the schedule rows as an arg (SND1 data
plane; C table emission is the M4 mixer task, FORMATS.md §5.4 — this task
verifies the LOGIC). `isFinalDeath`'s args are a projected globals slice
{gameMode, playerType, stocks (null for inactive slots), versusMode} —
measured: versusMode is 0 in harness matches, so the stocks loop IS the
live path.

EVENT ATTRIBUTION (owner stack): entering a wrapped boundary fn pushes an
attributing frame; entering any dispatched move fn (`actionStates` deep
copies, the 5 per-char moves-index tables, the shared JUMPAERIALB/F module
objects — all their function properties get non-recording loggers) pushes
a non-attributing frame. A sound play / seeded-RNG draw / dispatch is
credited to the innermost frame if attributing; otherwise sounds and
dispatches are ignored (they belong to the moves clusters) and RNG draws
are emitted as standalone `Math.random` records. Mutation-captured
boundaries carry the 5th post field as the envelope
`{"dsp":[...],"mut":{...},"rng":[...],"snd":[...]}` (sorted keys): direct
dispatch notes as `"<phase>:<MOVENAME>"` strings, the fn's mutated
write-set, seeded draws consumed, sound names played.

RNG channel records (oracle/CHECKSUM.md §6): a frame-0 `rngBoot` record
(`args [seed, bootDraws]`, ret = the fast-forwarded mulberry32 state;
bootDraws pinned 465 — the same count the qjs oracle boot pin froze)
opens the file; every seeded draw thereafter appears exactly once (as a
standalone record or inside a post `rng` list), so the replay chains ONE
C mulberry32 (`port/sim/ml_rng.h`) through the whole file draw-for-draw —
including the single off-step pre-frame-1 `startGame` draw, asserted as
exactly one standalone frame-0 draw. percentShake uses the stashed native
RNG upstream and never appears.

asshort sweep (rule 11, measured zero-live surfaces): getAngle, mashOut
(the `!input < 0.7` bool<num coercion arms), checkForSquat, the
checkForJump/DoubleJump/MultiJump tap-jump arms (synthetic 22-key Input
literals) — plus two guarded impure extensions, both restore-proven by
the ×2 byte-stability and STREAM MATCH guards: (a) the KO-shout sites
(`randomShout` ×48 per char, covering every shout outcome of both switch
shapes): Math.random is SWAPPED for a local mulberry32 seeded 0x0badf00d
for the duration (drawing the seeded match stream pre-match would shift
every subsequent draw and fail the stream guard); the replay driver
mirrors with a sweep generator for frame-0 randomShout records. (b)
executeIntangibility: a synthetic player is injected into INACTIVE slot 3
(playerType −1; pre-match value restored) so the C `ml_intang` CTAB1
lookup gets live-executed cross-checks (marth ESCAPEF/TECHN/DOWNSTANDF,
trigger and no-trigger frames).

Zero-live surfaces WITHOUT a sweep (documented honest coverage, all
translated verbatim): the turbo interrupts (turbo mode off in all
goldens; they read live player state and dispatch real moves — unsweepable
without perturbation), shieldDepletion's break branch, isFinalDeath's
true outcomes (match-end path, task 17's lifecycle surface), and every
POSITIVE dispatch path (checkForIASA never fires an aerial/jump cancel in
these traces) — the dispatch seam is verified negatively on every live
record (a C translation that spuriously dispatches diverges: proven, 3
divergences on g06 in the negative test) and positively by the sweep's
sound/rng events only.

## The physics spec (M2 task 5)

Wrapped boundary (8): `physics(i, inputBuffers)` (mutation-captured),
interpolatedCollision's `sweepCircleVsSweepCircle`/`sweepCircleVsAABB`
(pure; live callers hitDetection + article), and the 5 hitDetection launch
getters (`getLaunchAngle`, `getHorizontal/VerticalVelocity`,
`getHorizontal/VerticalDecay`) recorded ONLY while inside a physics call.

The physics record:

- args = `[i, inputBuffers[i].slice(0,4), pre]` — physics reads input
  history depth ≤ 3 (22-key Input canon, input_canon bridge). `pre` is the
  full PRE-call state envelope (sorted keys): `alias` (probe, below),
  `characterSelections`, `gameMode`, `gameSettings`
  {lCancelType, phantomThreshold, turbo}, `playerType`, `players` (the
  projected player canon per active slot, null for inactive — same
  projection as the player spec; frame-1 pre-states legitimately lack
  `phys.passing` and carry `undef` ECB1/ECBp components, both
  presence-modeled in ml_player.h per rule 8), `stage` (module-read
  projection of activeStage: the five surface lists + `connected`
  (normalized: absent key → null) + `ledge` + `blastzone` — captured per
  record, so fountain's per-frame moving-platform mutations are recorded
  faithfully), `versusMode`.
- post = `{"alias":…,"hq":[…],"players":…,"snd":[…]}` — post-call alias
  probe (3 hitbox flags, compared against the C tracking), physics' OWN
  hitQueue rows (mark/collect attribution: hitQueue is reassigned by
  resetHitQueue each tick so its `push` cannot be wrapped once; rows
  appended during dispatch windows belong to the moves), post players,
  physics' own direct sound plays.

ORACLE-FED SEAMS (moves are tasks 7-12, the getters task 6): every
top-level move dispatch from inside physics produces a `dispatch` record —
args `[phase, moveObject.name, [slot, ...extraArgs]]` (the input array arg
is dropped by projection), post `{alias, players}`. The replay VERIFIES
the site (phase + expected name via the asFlags table + slot/extras
bit-exactly, in call order — FIFO) and RESYNCS the C sim from the recorded
post-dispatch state. Getter records: args verified bit-exactly, the
recorded return injected. A C translation that reaches a seam out of
order, with different arguments, or not at all, diverges. (Corollary
measured in the negative tests: a corrupted PRE field that is overwritten
by the next dispatch resync before any observable read is MASKED — the
oracle-fed seam bounds each record's sensitivity to the state the C code
actually consumes.)

ALIAS PROBES (rule 10): per active player
`[pos===ECB1[0], active, hitList, id aliased]` in pre-args and dispatch
posts (restored into the C flags — a move init inside land() may reassign
pos invisibly, so pos-ECB1 is capture-restored, never C-predicted); the
physics post carries the 3 HITBOX flags only, which the C side tracks
through merge/land/turnOffHitboxes and must match (teeth: untracked merge
flags → 26 divergences on g01). The pos→ECB1[0] component WRITE-THROUGH
is modeled in C (physics.c pos_set_*) but has zero OBSERVABLE live
coverage on g01/g04/g06 (its visible window is grounded movement under a
low ceiling — moveAlongGround; no VS-stage golden reaches one): documented
honest coverage, first observable domain = target-stage/pstadium work.

`asFlags` (frame-0 record): the actionStates per-(char,state) flag DATA
physics branches on (canEdgeCancel, disableTeeter, inGrab, headBonk,
specialWallCollide, canPassThrough, dead, missfoot, ignoreCollision,
wallJumpAble (assigned verbatim to canWallJump — JsBool undef-at-rest),
landType, airborneState, canGrabLedge, name), dumped for all 5 chars.
run-capture.js's post-run `finalCheck()` hook re-dumps and hard-fails on
ANY drift — the mechanical soundness proof for the C side's static table.
Measured domain note: `canGrabLedge` is undef | false | [bool,bool] —
`false[k]` is undefined in JS (falsy element reads), `undefined[k]` throws
(the C read traps, placed at the EXACT lazy read point upstream reaches).

physics sweep (rule 11): 35 fixed pure calls through the wrapped
interpolatedCollision exports covering sweepCircleVsAABB's 8 region cases
(corner/line/null/overlap/separated arms) and sweepCircleVsSweepCircle's
overlap + t1/t2 selection lattice. ecbSquashData is physics.js
module-PRIVATE state (unreadable from outside): the replay CHAINS it in C
across the whole file from the nullSquashDatum initial value — the shared
JS object is provably never mutated (physics.h note 3).

## The undef-ret allowlist (rule 8)

The no-undef-in-returns pin (soundness of the marshaller's
ToNumber(undefined)→NaN argument mapping) applies per function, not
blanket: ACCESSOR-class functions (e.g. getXOrYCoord — a raw property
read) echo undefined VERBATIM; JS converts only at the consumer's
arithmetic. Such functions are frozen in the spec's expectations file
(`undefRetAllowed`) and their C translations preserve undef-ness
explicitly (JsNum/JsVec2D in vec2d.h). Everything else keeps the strict
invariant.

## Files

- `<golden-id>.envcoll.jsonl` — the M2-CAL boundary records (gitignored,
  in `port/sim/calib/build/`)
- `<golden-id>.capture-run.json` — the envcoll run JSON (meta/coverage/
  frames) used for the verify-stream guard
- `<golden-id>.<spec>.jsonl` / `<golden-id>.<spec>-run.json` — the same
  pair for later specs (util, ...)
- `expected-capture.json` — measured-then-frozen per-function call-count
  pins per golden for envcoll (drift alarm, M1 instrument class)
- `expected-capture-<spec>.json` — the same for later specs
  (checked by `check-spec-pins.js`; may add `postStateFns` — functions
  whose records carry the 5th post-state field)
- `survey-shapes.js` — capture-FIRST instrument (rule 7): per-path
  type/key-set/length report over a capture's canon fields; run it on a
  fresh capture BEFORE finalizing any C value model
