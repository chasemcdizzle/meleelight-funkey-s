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

- `hitdet` (M2 task 6, `spec-hitdet.js`): all 29 exported functions of
  `src/physics/hitDetection.js` + the hitQueue/phantomQueue value model +
  the seeded-RNG chain → `replay_hitdet.c` (translations
  `port/sim/hit_detection.{c,h}`). See "The hitdet spec" below. Pins:
  `expected-capture-hitdet.json`.

- `moves-shared` (M2 task 7, `spec-moves-shared.js`): the shared move set
  (src/characters/shared/moves/, 79 move objects) →
  `replay_moves_shared.c` (translations
  `port/sim/characters/shared/moves/*.c` + `moves_index.c` + `moves.h`).
  See "The moves-shared spec" below. Pins:
  `expected-capture-moves-shared.json`.

- `moves-fox` (M2 task 8, `spec-moves-fox.js`): the fox per-char move set
  (src/characters/fox/moves/, 61 move objects) + the LASER/ILLUSION
  article-init boundary → `replay_moves_fox.c` (translations
  `port/sim/characters/fox/moves/*.c` + `moves_index.c` + `moves.h`;
  goldens g01/g03/g08 — the fox carriers). See "The moves-fox spec"
  below. Pins: `expected-capture-moves-fox.json`.

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

## The hitdet spec (M2 task 6)

Wrapped boundary (29 = every exported function; hitQueue/phantomQueue are
exported ARRAYS, asserted present). Live external callers: main.js's
gameTick pipeline (resetHitQueue / checkPhantoms / hitDetect / executeHits),
physics.js's hitlag exit (the 5 launch getters — task 5's oracle-fed getter
seams are these functions' REAL C translations now), article.js
(getKnockback / getHitstun / knockbackSounds / segmentSegmentCollision).
The other 15 exports have only module-internal callers (babel local
bindings): identity-wrapped, PINNED AT ZERO records — a record appearing
means a new external caller and fails the pins.

- `hitDetect` (args `[p, pre]`), `executeHits` / `checkPhantoms`
  (args `[pre]`): mutation-captured with ONE uniform envelope — pre
  `{alias, characterSelections, gameMode, gameSettings {phantomThreshold},
  hq, phq, playerType, players}`, post `{alias(3-flag), hq, phq, players,
  rng, snd}`. `hq`/`phq` are the FULL exported queues (rows enter from
  other clusters' windows — THROW moves during update, physics'
  damaging-stage rows — so the replay marshals them per record, never
  chains). hq rows: 6/7-element arrays `[v, a, h, shieldHit, isThrow,
  drawBounce(, phantom)]`; `a` is a slot number or physics' collisionData
  object (stage damage). `resetHitQueue`/`setPhantonQueue` are lean
  (queue-only read/write set — they run pre-update when frame-1 players
  still lack prevFrameHitboxes, so no alias probe).
- DISPATCH seams (tasks 7-12): args `[phase, moveName, [slot, ...extras]]`
  (`init`/`onClank`/`onPlayerHit`), post `{alias(4), hq, players}` —
  verified in call order, resynced. phantomQueue is NOT in dispatch posts
  (no move module imports it; a move write would diverge the outer post).
- RNG channel: rngBoot + owner-attributed draws (the asshort discipline).
  hitdet's own draws = screenShake's 4 per regular hit (render shake; the
  DRAWS are stream state) → the post `rng` list. Draws inside a dispatch
  window BELOW a hitdet boundary would be chain-order-ambiguous vs the
  boundary's own draws: recorded as `Math.randomW`, PINNED ZERO (measured),
  replay hard-fails on one. All other draws are standalone `Math.random`
  records burned eagerly in file order (sound because of the W pin).
- `getLaunchAngle` args are projected `[trajectory, knockback, reverse,
  x, y, v, groundedOrNull]` — the 7th mirrors upstream's exact laziness
  (player[v].phys.grounded is read ONLY under knockback < 80; null
  otherwise). `getKnockback`'s hb arg → its `{bk,kg,sk}` read set;
  `crouching`/`vCancel` are bool|undefined truthiness (article passes raw
  actionStates flag reads). `knockbackSounds` args `[type, knockback,
  characterSelections[v]]` with post `{rng, snd}`.
- `hdFlags` (frame-0 record): the per-(char,state) flags hitDetection
  branches on — `{canBeGrabbed, crouch, downed, name, specialClank,
  specialOnHit, vCancel}` — dumped for all 5 chars; finalCheck() re-dumps
  post-run and hard-fails on drift (the C table's soundness proof).
- Sound stops: `sounds.furaloop.stop` (the FURAFURA arm) records the token
  `"furaloop.stop"` in the owner's snd list; C mirrors via
  `ml_sound_stop` on the same queue.

hitdet sweep (rule 11, 164 calls): getKnockback both formula arms/caps/
modifiers, getHitstun, the velocity/decay getters (incl. the kb-0 `-0`
product and getVerticalVelocity's zeroing arms), getLaunchAngle's kb>=80
arms (player[v] never dereferenced — upstream short-circuits) plus kb<80
grounded/deadzone arms via a rule-12 slot-3 player injection (restored),
segmentSegmentCollision's parallel/t/u/hit arms, and the knockbackSounds
type x tier x char lattice via a rule-12 characterSelections[3] injection
(restored). Measured rule-12 corollary (2nd instance): razor-thin
threshold nudges are no-op teeth — hurtWidth 8->8.5 and the 0.01
phantomThreshold band bite NOTHING on occurring hit margins; geometry
teeth need 8->12, phantom teeth need the classification inverted.

## The moves-shared spec (M2 task 7)

Wrapped boundary (1212 fns): every function property of every
SHARED-ORIGIN actionStates entry — sharedOrigin is MEASURED by function
identity against the shared moves-index module (`setupActionStates` runs
`deepCopyObject(true, val)`, which deep-copies data but copies FUNCTIONS
by reference), asserted per char: 243 fns (79 moves × 3 phases + `land`
on DAMAGEN2/ESCAPEAIR/FALLSPECIAL/SHIELDBREAKFALL/STOPCEIL/DOWNDAMAGE)
for chars 0/2/3/4, 234 for puff (char 1 OVERRIDES
FURAFURA/JUMPAERIALB/JUMPAERIALF — tasks 12's surface), plus the shared
JUMPAERIALB/F module objects (checkForIASA's direct import path;
zero-live, and a live puff record through them would fail the seam name
verification loudly — documented caveat). Non-shared actionStates entries
get the SEAM logger below.

- `move` (5-field, mutation-captured): a shared-move phase entered
  OUTSIDE any move record's scope (`inScope == 0`) — callers are physics'
  per-frame state drive, hitdet's inits, and per-char move windows at top
  level (recording under a silent per-char window is chain-safe: the
  window's own draws are standalone records pushed at draw time, so file
  order equals chain order). Nested shared→shared calls are TRANSPARENT
  (the C body calls its own translations). args = `[phase, name,
  [slot, ...extras], inputs, pre]`:
  * extras: the args around `(p, input)` — input sits at index 2 for
    JUMPF/JUMPB/KNEEBEND/WALK `.init` and SHIELDBREAKFALL `.land` (their
    extra precedes it), index 1 otherwise (extras follow). Domain:
    number | boolean | Vec2D (STOPCEIL/WALLDAMAGE normals).
  * inputs: per-slot `input[k].slice(0,8)` (moves read history depth ≤ 6
    — GUARD/GUARDON read `input[p][4]`/`[6]` — and CAPTURE moves read
    `input[grabbedBy]`), null for absent slots.
  * pre: {alias (probe4), characterSelections, gameMode, gameSettings
    {tapJumpOffp1..4}, hq (the FULL exported hitQueue — carried OPAQUE:
    no shared move imports hitDetection's queues), playerType, players,
    stage {ground, ledge, platform, respawnFace, respawnPoints} (null on
    pre-match sweep records), versusMode}.
  post = {alias(4 — pos-ECB1 is C-TRACKED here, moves own the
  reassignment/component-write sites), hq, players, rng (owner draws:
  CAPTUREWAIT's mash wiggle, FURAFURA's vfx jitter, screenShake's 4 per
  DEAD call, drawVfx("circleDust")'s 4), snd (incl. "furaloop.stop"),
  vfx (drawVfx name queue — the ml_vfx seam's oracle)}. ret is
  T | F | undef (several upstream interrupt arms fall through without a
  return — carried verbatim as AS_UNDEF).
- `mdispatch` (5-field seam): a NON-shared move fn entered while the TOP
  frame is the attributing move frame (per-char inits from shared
  interrupt chains; asserted 2-arg `(p, input)`). args =
  `[phase, name, [slot]]`, post = {alias(4), hq, players, rng} — the rng
  list is the rule-14 instrument STRENGTHENED: window draws are carried
  in the seam post and advanced/verified on the chain at the seam point
  (interleaving is recoverable because the seam sits at an exact
  structural point of the C body). Measured ZERO window draws over
  g01/g04/g06. Deeper windows push silent frames (subsumed).
- `mvData` (frame-0 record, finalCheck drift-guarded): the EXECUTED
  move-data plane — per char {state→name} (seam name verification),
  {state→sharedOrigin} (the C registry is built from this measured map),
  the per-char index.js data patches (ESCAPEB/ESCAPEF/DOWNSTANDB/
  DOWNSTANDF/TECHB/TECHF setVelocities, CLIFFCATCH/CLIFFWAIT posOffset),
  CAPTUREDAMAGE.setPositions, actionSounds rows for the shared-move
  sound keys, and palettes[pPal[k]][0] (the FURASLEEP colour-blend
  bases). framesData/charAttributes/charHitboxes("thrown") are NOT
  dumped — the C side reads the M1 CTAB1 tables (3rd/4th consumers of
  the generated data path).
- RNG: rngBoot + the asshort discipline. Frame-0 `move` records are the
  rule-12 sweep and replay on a SEPARATE sweep mulberry32 (seed
  0x0badf00d) chained across all of them; the spec swaps Math.random for
  that generator during the sweep so ESCAPEN's circleDust and DEAD*'s
  screenShake draws never touch the seeded match stream.

moves-shared sweep (rules 11/12, 44 calls): a REAL upstream
`playerObject(0, [10,20], 1)` injected into inactive slot 3
(playerType[3] set and restored — net-restore purity under the ×2
byte-stability + STREAM MATCH guards), neutral 8-deep inputs; covers the
zero-live shared moves with a reachable future domain: rolls/spotdodge/
airdodge (ESCAPE*), techs (TECH*/WALLTECH*/WALLJUMP), WALLDAMAGE +
STOPCEIL both reflect arms (kVel preset) and all three STOPCEIL land
arms, the shieldbreak chain (minus FURAFURA: its init stores the Howl
play id into furaLoopID — outside the sim value domain, the C body traps
there), FURASLEEP* (live-executes blendColours + the rgb() string
formatting against the dumped palette strings), DOWNSTAND*/DOWNDAMAGE,
DEADUP/DEADRIGHT (screenShake on the sweep chain), SLEEP/PASS/MISSFOOT/
THROWNFALCONDIVE, the CAPTUREWAIT/CAPTUREDAMAGE grabbedBy===-1 guards,
and the JUMPF/JUMPB/KNEEBEND/WALK/FALL extra-arg variants. Zero-live
surfaces WITHOUT a sweep (documented): FURAFURA (furaLoopID), CLIFFCATCH/
CLIFFWAIT/REBIRTH init paths beyond their live coverage (need a live
stage), throws/CATCHATTACK/CLIFF* per-char seam arms, and every per-char
dispatch arm not fired by the traces (seams verify must-not-fire on
every live record).

## The moves-fox spec (M2 task 8)

Wrapped boundary (194 fns): every function property of every FOX-ORIGIN
actionStates entry — fox-origin is MEASURED by fn identity against the
characters/fox/moves module index (rule 15's instrument extended per its
recipe), found only on table 2: 61 module keys, 192 fns (49 moves ×
init/main/interrupt + 11 × +land + UPSPECIAL's lone init) — plus the two
article init boundaries (`articles.LASER.init` / `articles.ILLUSION.init`).
Shared-origin entries stay UNWRAPPED (at top level they are chain-safe
silent surface — their draws land as standalone records; inside a fox
record they are the transparent nested C tree, task 7's bodies linked in);
NON-fox per-char entries get the task-7 seam logger.

GOLDENS (PROVISIONAL, auto-adopted): g01/g03/g08 — the fox carriers. The
g01/g04/g06 convention's purpose is live coverage and g04/g06 field no fox
slot. g08 is the first CPU-golden capture: the AI plane stays JS-side (M2
AI policy) and its seeded draws land as standalone `Math.random` records
(1488 on g08), chain-verified draw-for-draw like every other draw.

- `move` (5-field, mutation-captured): a fox-origin phase entered OUTSIDE
  any move record's scope. args = `[phase, name, [slot], inputs|null,
  pre]` — fox phases carry NO extras; THROWN*{BACK,DOWN} inits arrive
  1-arg upstream (throwers call `.init(grabbing)` without input — the
  bodies never read it) and record inputs `null`. pre/post = the task-7
  envelopes verbatim ({alias,hq,players,rng,snd,vfx} post).
- `mdispatch` (5-field seam): a NON-fox, NON-shared move fn entered while
  the top frame is the attributing fox frame — the victim's THROWNFOX*
  per-char entries from fox THROW* chains. Task-7 form; 1-arg and 2-arg
  dispatch sites both occur. Measured ZERO live over g01/g03/g08 (fox
  never threw a non-fox live) — the FIFO's teeth are negative-test-proven
  (a wrong-table dispatch in C → seam-underflow divergence).
- `article` (4-field seam FIFO): `articles.{LASER,ILLUSION}.init(options)`
  under the attributing fox frame — the task-13 oracle boundary. Article
  inits only READ player state, mutate only the JS-side article queues,
  and draw no RNG (LASER's nested main/wallDetection is pure stage reads),
  so no post/resync: args = `[name, options]` verified bit-exactly in call
  order, ret echoed (undefined). Outside fox scope (falco lasers,
  executeArticles' per-frame mains) article calls are NOT this boundary —
  the article sim stays oracle-side until task 13.
- hq: carried OPAQUE as in task 7, EXTENDED with the push model — fox
  THROW* does `hitQueue.push([grabbing, p, 0, false, true, isThrowDown])`;
  the C crosses `mv_hq_push6`, which appends the row's canon to the opaque
  carrier (teeth: flipping the THROWDOWN flag → exactly 1 divergence).
- `mvData` (frame-0, finalCheck drift-guarded): the task-7 dump EXTENDED
  with `fox: {origin: {state→bool}, data: {state → every own enumerable
  ARRAY-valued non-function prop}}` — ATTACKDASH/APPEAL/FIREFOXBOUNCE/
  THROWFORWARD setVelocities*, all 20 THROWN*.offset (incl. authored
  EXPRESSIONS like `-7.74-0.08` — executed data, rule 15), CLIFF*
  offset/setVelocities, canGrabLedge pairs (CLIFF*'s runtime
  `this.canGrabLedge = false` write would drift the dump and hard-fail
  finalCheck; the C traps at that site), THROWNPUFFBACK.offsetVel (dead
  data). The C registry (shared for all chars + fox on table 2) and the
  fox data plane (`mv_fox_arr`/`mv_fox_pair`/`mv_fox_arr_len`) are built
  FROM the dump.

Value-model extensions measured by this capture (rule 7's marshal caught
both):
- hitbox spec `offsetSingle`: the 12-key chars-data key set with a SINGLE
  Vec2D offset — upstream `createHitbox` called with a bare Vec2D (fox
  throw hitboxes, attributes.js:749). ml_player.h/player_canon.c; the old
  CONSTRUCTOR fallback in the charHitboxes assign helper mis-shaped these
  and was unreached by every prior capture (class-fixed in
  `mv_assign_hitbox_id`).
- AI number-valued input BUTTONS (g08): ai.js writes numbers into
  aiInputBank button fields (`.l = 0` / `= 1.0`). Every sim consumer of
  the button fields is truthiness-only (verified — no raw propagation into
  player state), so `ml_input_from_canon` maps CV_NUM buttons by JS
  truthiness. Human-golden captures are unaffected (all booleans).

moves-fox sweep (rules 11/12, 168 calls): a REAL upstream
`playerObject(2, [10,20], 1)` in net-restored slot 3 with
`characterSelections[3] = 2` injected (hitdet-sweep precedent) + a second
slot-2 player for the THROWN* grabbedBy<p (timer=-1) arm; covers jabs 2/3,
tilts/smashes (charge/release/randomShout arms), ATTACKAIRU + land arms +
the ATTACKAIRN `hitboxes.frames++` NaN quirk + the checkForIASA
JUMPAERIALB/F + FOXMOVES-aerial dispatch arms, both lasers' article sites +
laserCombo loops, illusion ground/air article sites + b-skip arms, the full
firefox chain (atan2 angle + grounded clamp arms, launch face arms, bounce)
, the shine family (reflector swap, shineLoop, face flip, release/b-held
arms, platform drop, jump-cancel, double-jump-out), all four THROW* (guard
+ full arms via self-grab grabbing=grabbedBy=3, laser crossings, hq-push
crossings incl. THROWDOWN's true flag, CATCHCUT + WAIT release arms), all
20 THROWN* inits + clamp-quirk arms (THROWNFOXFORWARD's double clamp,
THROWNPUFFUP's player-timer clamp), all 8 CLIFF* (init/offset/grounding/
setVel/attack/interrupt-release arms against the default pre-match stage's
real ledges), APPEAL. Sweep spawns are spliced back out of aArticles and
hitQueue (net-restore purity). Zero-live surfaces WITHOUT a sweep
(documented): SIDESPECIALAIR.init's grounded arm (upstream itself
stack-overflows — main's grounded arm recurses with grounded still true),
the CLIFF* onLedge===-1 canGrabLedge table-write arm (C traps; mvData
drift-guarded), THROWNFALCO*/FALCON* offset overruns past the array end
(upstream throws; C traps at mv_fox_pair), and interrupt-tail WALK arms
shadowed by checkForTilts on the same axis domain.

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
