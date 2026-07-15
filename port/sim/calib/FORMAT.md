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

- `moves-falco` (M2 task 9, `spec-moves-falco.js`): the falco per-char
  move set (src/characters/falco/moves/, 69 move objects) + the
  LASER/ILLUSION article-init boundary → `replay_moves_falco.c`
  (translations `port/sim/characters/falco/moves/*.c` + `moves_index.c`
  + `moves.h`; goldens g02/g05/g07 — the falco carriers). See "The
  moves-falco spec" below. Pins: `expected-capture-moves-falco.json`.

- `moves-falcon` (M2 task 10, `spec-moves-falcon.js`): the falcon
  per-char move set (src/characters/falcon/moves/, 67 move objects) +
  the LASER/ILLUSION article-init boundary as a MEASUREMENT instrument
  (falcon has NO article call sites — pinned zero) →
  `replay_moves_falcon.c` (translations
  `port/sim/characters/falcon/moves/*.c` + `moves_index.c` + `moves.h`;
  goldens g03/g04/g06 — the falcon carriers). See "The moves-falcon
  spec" below. Pins: `expected-capture-moves-falcon.json`.

Method records use `Mod#method` names (args prepended with the canon of
`this`); constructor records use `Mod.new` (ret = canon of the built
instance, per-instance closures serializing as `fn`). Argument canon is
computed BEFORE the call (pre-call state — mutation-safe for later
specs); records are pushed after return, so nested boundary calls appear
before their caller's record.

## Capture mechanism (summary; rationale in fix_plan §M2-CAL)

The runner (`run-capture.js`) serves the untouched built dist, but the
served bytes of `dist/js/main.js` get TWO textual injections — the
webpack bootstrap's `var installedModules = {};` gains
`window.__wpCache = installedModules;`, and fountain's module-private
`platformStates` declaration gains a read-only window getter (M2 task
14, "The platforms spec" below) — nothing on disk changes and no sim
behavior changes (both injections are pure exposure). After page load (before `setupMatch`), `capturelib.js`
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

## The moves-falco spec (M2 task 9)

Task 8's fox spec followed exactly (the "moves-fox" section above applies
verbatim with fox→falco) — deltas only:

- Wrapped boundary (216 fns): falco-origin lives ONLY on table 3 — 69
  module keys, 214 fns (53 moves × init/main/interrupt + 13 × +land +
  the 3 init-only delegates UPSPECIAL/DOWNSPECIALAIR/DOWNSPECIALGROUND) —
  plus the two article init boundaries.
- GOLDENS: g02/g05/g07 — the falco carriers (g02 falco/puff/ystory, g05
  marth/falco/fdest, g07 falco/CPU-falcon/battlefield: the second
  CPU-golden capture, AI JS-side as on g08).
- ARTICLE options: falco passes `isFox: false` on every LASER/ILLUSION
  init; the THROWDOWN lasers additionally `partOfThrow: true`; ILLUSION
  is always `type: 0`. The C seams (`mv_article_laser_falco` /
  `mv_article_illusion_falco`) serialize the extra keys in canon-sorted
  position.
- STRUCTURE deltas vs fox (measured per-file diffs; C carries each
  verbatim): falco THROW* inits have NO grabbing===-1 guard (the guard
  lives only in the interrupts' bare-return arm); falco THROWN* have NO
  grabbedBy===-1 guards and NO offset-length clamps (player[-1]/offset
  overruns throw upstream — mv_player/mv_falco_pair trap); falco CLIFF*
  have NO onLedge===-1 canGrabLedge table-write arm (ledge[-1] throws
  upstream — mv_ledge_point traps); the shine is a 4-sub-state machine
  per environment (START/LOOP/END/TURN ×{AIR,GROUND}) whose land/
  platform-drop arms write `actionState` directly; THROWFORWARD's
  setVelocities index is Math.max(0,·)-clamped; falco's APPEAL carries
  no setVelocities data plane.
- checkForIASA: upstream has NO characterSelections==3 branch — a falco
  IASA aerial payload dispatches NOTHING (returns true bare); the falco
  replay registers no char module (mv_register_char_module unused).
- `mvData` carries `falco: {origin, data}` (falco-origin map measured on
  table 3 + every array-valued own prop of falco-origin entries: 20
  THROWN*.offset (+offsetVel dead data), CLIFF* offset/setVelocities,
  THROWFORWARD/ATTACKDASH/FIREFOXBOUNCE setVelocities, canGrabLedge
  pairs), served through `mv_falco_arr`/`mv_falco_pair`/
  `mv_falco_arr_len`.

Value-model extensions measured by this capture (rule 7's marshal caught
both; rule 16's re-survey discipline — falco appears in NO prior capture
golden, and g05's MARTH exercised a move family no prior golden did):
- phys.shieldBreakerCharge (number) / shieldBreakerChargeAttempt (bool) /
  shieldBreakerCharging (bool): runtime-added by marth NEUTRALSPECIAL*
  (presence-modeled, rule 3/8 family).
- player.shieldBreakerID (number): marth's Howl play id, furaLoopID's
  cousin — runtime-added, presence-modeled.

moves-falco sweep (rules 11/12, 217 calls): the task-8 scaffolding reused
(slot-3 playerObject(3,·) + characterSelections[3]=3, slot-2 injection
for grabbedBy<p, sweep mulberry32, article/hq splice-restore, default
pre-match stage cliffs) with falco-measured timer arms; falco-specific
coverage: the shine 4-sub-state machine in BOTH environments (decel
branches, shineLoop reset, platform drops, turn/end/jump-out/re-init
arms, land actionState writes), THROW* laser crossings (THROWUP 18/20/24,
THROWBACK 15/18/21 + face flip, THROWDOWN 23/25/28/31 with partOfThrow,
THROWFORWARD's clamp arm) + hq crossings + the interrupt-only
grabbing===-1 bare-return arms, the no-snap THROWNPUFF{UP,FORWARD} inits
with grabbedBy=-1 (safe upstream: no deref until timer>0), ATTACKAIRD's
dair1→dair2 mid-move swap, and the checkForIASA no-dispatch payload arm.
Unswept (documented): THROW*.init without a grab and snap-family
THROWN*.init with grabbedBy=-1 (upstream itself throws), THROWN* offset
overruns (upstream throws), interrupt-tail WALK arms shadowed by
checkForTilts.

## The moves-falcon spec (M2 task 10)

Task 8's fox spec followed per the per-char recipe (the "moves-fox"
section above applies with fox→falcon) — deltas only:

- Wrapped boundary (219 fns): falcon-origin lives ONLY on table 4 — 67
  module keys, 214 phase fns (53 moves × init/main/interrupt + 13 ×
  +land + SIDESPECIALGROUNDTOAIR's lone init/main pair) PLUS falcon's
  three NON-phase move fns: `onPlayerHit(p)` on SIDESPECIAL{GROUND,AIR}
  (hitDetection.js:493's specialOnHit arm — 1-arg, inputs `null`) and
  `onWallCollide(p, input, wallFace, wallNum)` on DOWNSPECIALGROUND
  (physics.js:122's specialWallCollide arm — the only >2-arg site in the
  set; args canon `[phase, name, [slot, wallFace, wallNum], inputs,
  pre]`) — plus the two article init boundaries as a MEASUREMENT
  instrument.
- ARTICLES: falcon has NONE — it imports `articles` in 6 files and never
  dereferences it (dead imports, measured). The article count is pinned
  ZERO per golden; the replay driver still FIFOs any article record and
  fails it as unconsumed (the measured-claim tripwire).
- GOLDENS: g03/g04/g06 — the falcon carriers, back on the g01/g04/g06-era
  convention with g03 replacing g01 (g01 fields no falcon). g07's CPU
  falcon was MEASURED before pinning: it fires ZERO live falcon-origin
  moves over its 3600 frames (the falco capture's 81-rngCall AI plane
  never attacks with falcon-origin states), so a g07 capture would add
  sweep-only records — carrier membership is measured live coverage,
  never char presence.
- C dispatch: the special phases route through
  `mv_register_special_phases` (shared moves.h — a driver-registered
  (state, phase)→MvFn lookup; MlMoveDef keeps its 5-field shape so the
  209 existing positional initializers stay untouched). Unregistered →
  mv_out_of_domain, the upstream missing-property TypeError.
- STRUCTURE deltas (measured per-file diffs; C carries each verbatim):
  falcon's THROWN* family is byte-identical to FOX's shapes — the
  guarded THROWN{PUFF,MARTH,FOX}* (init/main grabbedBy===-1 guards +
  offset clamps) and the unguarded THROWN{FALCO,FALCON}* (player[-1]/
  overrun throws upstream — traps); the 20 C files are the task-8 fox
  translations with renames (data-only diffs, offsets served by the
  mvData falcon dump); GRAB/CATCHATTACK are byte-identical to fox's
  files. THROW* keep fox's grabbing===-1 init guard but fire NO lasers.
  CLIFF* keep fox's onLedge===-1 canGrabLedge table-write arm (traps).
  SIDESPECIALGROUND writes `this.canEdgeCancel` at runtime — a SCALAR
  move-table write outside the array-only mvData dump, modeled as C
  module state (`mv_falcon_ssg_set_canEdgeCancel`; its only sim reader
  is physics' flag lookup, task 17). UPSPECIALCATCH/UPSPECIALTHROW draw
  the seeded stream INLINE (2 draws per firefoxtail ×3) and
  UPSPECIALCATCH's interrupt pushes `[grabbing,p,0,false,true,false]`
  (mv_hq_push6). Dead-arm quirks carried verbatim:
  SIDESPECIALGROUNDHIT.main reads `player[p].phys.timer` (undefined —
  physicsObject has no timer) and DOWNSPECIALGROUNDENDAIR.main reads
  `player.timer` (the ARRAY's — undefined): the `<`/`===` arms never
  fire. The firefoxtail windows read `id[0].offset[frame]` for their
  render-only positions — `mv_falcon_hb0_off` performs the read for
  crash-fidelity (out-of-range/constructor-offset traps) and discards
  the value.
- checkForIASA: upstream has NO characterSelections==4 branch — a falcon
  IASA aerial payload dispatches NOTHING (falco precedent); no char
  module registration.
- `mvData` carries `falcon: {origin, data}` (67-state origin map on
  table 4 + 85 array-valued own props over 50 states: JAB3/FORWARDSMASH/
  NEUTRALSPECIAL*/SIDESPECIAL*/DOWNSPECIALGROUNDENDAIR setVelocities,
  the PAIR arrays of UPSPECIAL/UPSPECIALTHROW/DOWNSPECIALAIR, 20
  THROWN*.offset (+offsetVel dead data), CLIFF* offset/setVelocities,
  canGrabLedge pairs), served through `mv_falcon_arr`/`mv_falcon_pair`/
  `mv_falcon_arr_len`.
- Value model: NO widening needed — g03/g04/g06 were all captured by
  prior specs (rule 16's budget was already paid by tasks 2-9; falcon's
  raptorBoost/landingMultiplier/upbAngleMultiplier are constructor
  fields modeled since task 2).

moves-falcon sweep (rules 11/12, 294 calls): the task-8 scaffolding
reused (slot-3 playerObject(4,·) + characterSelections[3]=4, slot-2
injection for grabbedBy<p, sweep mulberry32, hq splice-restore, default
pre-match stage cliffs) with falcon-measured timer arms; falcon-specific
coverage: FALCONPUNCH both environments (charge-free 100-frame arc,
setVel/vfx/sound arms, NSA's friction→setVel→fastfall phase arms + the
actionState-write land), the FALCONKICK 6-state family (timer%2
firefoxtail arms, Mid/Late swaps, END{AIR,GROUND} pairs incl. the dead
player.timer arms and the DSGENDAIR land actionState write), the
onWallCollide arms (both wall-face launch arms + the no-op arm; the
UPSPECIALTHROW chain draws 6 sweep-RNG draws), FALCONDIVE (drift/clamp
arms, face flip, the grab arm's pos component writes →
UPSPECIALCATCH → hq push → UPSPECIALTHROW chains, both land-guard arms),
raptor boost both environments + onPlayerHit + the HIT states, THROW*
incl. the fox-style init guards + hq/swap crossings, all 20 THROWN*
inits + guard/clamp arms (guarded family only — the THROWN{FALCO,
FALCON}* family throws upstream on grabbedBy=-1/overrun, unswept), all
8 CLIFF*, and the SIDESPECIALGROUND canEdgeCancel table-write arm
(swept last among SSG calls, table value restored — rule 12
net-restore). Unswept (documented): CLIFF* onLedge===-1 canGrabLedge
table-write arm (C traps; mvData drift-guarded), THROWN* overruns
(upstream throws), interrupt-tail WALK arms shadowed by checkForTilts,
SIDESPECIALGROUNDHIT's phys.timer and DOWNSPECIALGROUNDENDAIR's
player.timer dead arms (unreachable by construction upstream).

## The moves-marth spec (M2 task 11)

Task 8's fox spec followed per the per-char recipe (the "moves-fox"
section above applies with fox→marth) — deltas only:

- Wrapped boundary (246 fns): marth-origin lives ONLY on table 0 — 75
  module keys, 242 phase fns (58 moves × init/main/interrupt + 17 ×
  +land: ATTACKAIR*×5, NEUTRALSPECIALAIR, DOWNSPECIALAIR, SIDESPECIALAIR
  + its 8 chain states, UPSPECIAL) PLUS marth's two NON-phase move fns:
  `onClank(p, input)` on DOWNSPECIAL{GROUND,AIR} (hitDetection.js:71-72's
  specialClank arm — 2-arg, the standard wrapper shape) — 244 marth fns
  + the 2 article inits as the zero-pin tripwire (marth has ZERO
  `articles` imports anywhere under characters/marth/, measured).
- Post envelope gains **"sbid"** (canon-sorted between rng and snd):
  the Howl play ids returned by `sounds.shieldbreakercharge.play()` and
  CONSUMED by the sim (`player[p].shieldBreakerID = ...`,
  NEUTRALSPECIAL* timer-11). Howler ids are a GLOBAL counter
  (Howler._counter, advanced by every sound in the match — mostly
  outside this spec's records): unrecoverable chain state, so they are
  ORACLE-FED (task-5 launch-getter discipline). The replay FIFOs the
  recorded list per record, injects through `mv_howl_play_id`, and
  re-emits the CONSUMED ids into the compared post — count/order
  mismatches bite in the compare and the value itself lands in
  player.shieldBreakerID. NOT checksummed (no Howl id reaches the
  CHECKSUM.md field list) — the ×2 byte-stability + STREAM MATCH guards
  remain the determinism proof. `sounds.shieldbreakercharge.stop(id)`
  is the token "shieldbreakercharge.stop" (furaloop.stop's cousin).
- The timer-46 dmg write (`hitboxes.id[i].dmg = newDmg`) MUTATES the
  global charHitboxes objects upstream (player.js:132 aliases the chars
  data, no copy) — a runtime write to the M1-owned char-data plane
  (falcon canEdgeCancel's class, value-plane sub-shape). newDmg equals
  the authored 7 whenever shieldBreakerCharge < 30 (measured live
  domain {0,1} on g05); a live CHARGED punch followed by another
  nsg/nsa hitbox assign would surface as an init-record divergence
  (loud — C reads pristine CTAB1). The sweep exercises the ≥120 arm and
  RESTORES all 8 dmg fields (rule 12 net-restore).
- Dispatch graph: nearly every marth move imports the marth index
  (`marth[b[1]].init(p,input)` payload dispatch → marth_moves_init);
  marth aerials do NOT call checkForIASA — ATTACKAIRF/B INLINE the
  aerial-IASA logic (checkForDoubleJump → shared JUMPAERIALB/F modules;
  checkForAerials payload → marth[a[1]].init). checkForIASA's char-0
  MARTHMOVES arm (actionStateShortcuts.js:400-401) is therefore
  dead-by-construction upstream (only fox/falco/falcon aerials call
  checkForIASA, and only for their own char) — the replay registers the
  marth module anyway (mv_register_char_module(0, marth_move_def)),
  faithful to the source. THROW* dispatch victims 2-ARG
  (.init(grabbing, input) — unlike fox's 1-arg THROWBACK/THROWDOWN).
- Structure deltas (measured per-file diffs): THROWN{PUFF,MARTH,FOX}*
  are GUARDED (grabbedBy===-1 init guard — EXCEPT THROWNMARTHBACK,
  which has NONE — plus a -1 main guard and a `timer > len → len-1`
  clamp whose ORDER vs the guard varies per file; THROWNPUFFUP wraps
  its body in a vacuous `if(player[p].phys)`); THROWN{FALCO,FALCON}*
  are fox's unguarded family with ONE body delta — THROWNFALCOBACK
  ADDS `face *= -1` in init (caught by the g01 probe replay); CLIFF*
  keep fox's onLedge===-1 canGrabLedge table-write arm (C traps) and
  CLIFFGETUPQUICK sets ledgeRegrabCount=TRUE (others false); THROWUP's
  interrupt -1 arm returns false where the other three throws fall
  through undefined; phys.dancingBlade/dancingBladeDisable are
  runtime-added (rule 16 — presence-modeled in ml_player.h this task).
- mvData extension: `marth: {origin, data}` — array-valued props of
  every marth-origin entry (setVelocities incl. UPSPECIAL's PAIR array,
  ATTACKDASH/SSG3*/SSG4*/CLIFF* setVelocities, CLIFF*/THROWN* offsets,
  THROWNPUFFBACK.offsetVel (dead data), canGrabLedge pairs), served
  through `mv_marth_arr`/`mv_marth_pair`/`mv_marth_arr_len`.
- Value model: ONE widening — phys.dancingBlade/dancingBladeDisable
  (runtime-added by the dancing-blade family; no prior golden fired
  marth SIDESPECIAL*). shieldBreaker* trio + shieldBreakerID were
  already modeled (task 9).

moves-marth sweep (rules 11/12, 394 calls): the task-8 scaffolding
reused (slot-3 playerObject(0,·) + characterSelections[3]=0, slot-2
injection for grabbedBy<p, sweep mulberry32, hq splice-restore, default
pre-match stage cliffs) with marth-measured timer arms; marth-specific
coverage: the NEUTRALSPECIAL charge family (B-held charge + blend +
dashDust at charge%6==0, release-stop + charge==122 auto-stop tokens,
the timer-11 play-id records (sbid), the timer-46 dmg-write arm
uncharged AND ≥120-charged — global dmg fields restored — and the
timer-50 groundBounce arm), DOLPHINSLASH (angle-multiplier + face-flip
arms, the rotated PAIR setVelocities window at pi/16, the >22 drift +
0.36 clamp, all three land-guard disjuncts + the all-false no-init
arm), the DANCINGBLADE chains (combo-window disable/enable/disabled-b
arms, every UP/DOWN/FORWARD chain dispatch across both environments,
the 4DOWN multi-hit timer%6 switch incl. the shout6 cutoff at 37, air
mobility + the landing actionState writes, SIDESPECIALAIR's
sideBJumpFlag/no-flag/grounded init arms), COUNTER both environments
(white/grey/purple colour-cycle arms, onClank → DOWNSPECIAL{GROUND,
AIR}2 chains, the DSA land actionState write), THROW* (guard arms,
2-arg self-grab victim-dispatch chains, hq crossings, CATCHCUT +
WAIT + -1 arms), all 20 THROWN* inits + guard/clamp/order arms, all 8
CLIFF*. Unswept (documented): CLIFF* onLedge===-1 canGrabLedge
table-write arm (C traps; mvData drift-guarded), guarded-THROWN offset
overruns past the clamp window and THROWNMARTHBACK/-unguarded-family
grabbedBy=-1 arms (upstream throws), interrupt-tail WALK arms shadowed
by checkForTilts.



## The moves-puff spec (M2 task 12)

Task 8's fox spec followed per the per-char recipe (the "moves-fox"
section above applies with fox→puff) — deltas only:

- Wrapped boundary (223 fns): puff-origin lives ONLY on table 1 — 71
  module keys, 217 phase fns (56 moves × init/main/interrupt + 9 ×
  +land: ATTACKAIR*×5, DOWNSPECIAL{AIR,GROUND}, SIDESPECIALAIR,
  UPSPECIAL; NEUTRALSPECIALGROUND/GROUNDTURN ×3, NEUTRALSPECIALAIR ×4
  incl. land; and the three single-init TABLE OVERRIDES
  FURAFURA/JUMPAERIALB/JUMPAERIALF) PLUS puff's four NON-phase move fns:
  `onPlayerHit(p)` on NEUTRALSPECIAL{GROUND,AIR,GROUNDTURN}
  (hitDetection.js:493's specialOnHit arm) and `onWallCollide(p, input,
  wallFace, wallNum)` on NEUTRALSPECIALAIR (physics.js:122's
  specialWallCollide arm; args canon `[slot, wallFace, wallNum]`) —
  plus the two article init boundaries as the zero-pin tripwire (puff
  has ZERO `articles` references anywhere under characters/puff/,
  marth-strength; measured).
- TABLE OVERRIDES measured (rule 15's origin map): puff's index REPLACES
  the shared FURAFURA/JUMPAERIALB/JUMPAERIALF on table 1
  ({...baseActionStates, ...puffMoves}) — fn identity classifies them
  puff-origin there and shared on every other table (both directions
  asserted at install). Puff's FURAFURA is TRIVIAL (WAIT.init — no
  furaloop/furaLoopID: the shared version's Howl-id chain state never
  enters this cluster, so the task-11 sbid seam is NOT carried; no puff
  move consumes a Howl id — measured, no `= sounds.` assignment under
  characters/puff/).
- The pre envelope gains **"chd"** (canon-sorted after
  characterSelections): the EXECUTED charHitboxes `{moveKey: {idN:
  {dmg, size}}}` VALUE plane of the puff char at record time. Puff
  WRITES the M1-owned char-data plane at runtime through
  player.hitboxes.id ALIASES (player.js:132): NEUTRALSPECIAL{GROUND,
  AIR}'s post-release dmg writes run EVEN WHEN UNCHARGED — through
  whatever STALE id objects the previous move assigned (cross-move
  provenance) — and UPSPECIAL (sing) cycles id[0].size. Unlike marth's
  benign charge<30 domain, the live domain genuinely DRIFTS the plane
  (MEASURED: g04's jab1 dmg 3→7 from frame 1038, 983 records carry the
  drifted plane). The replay's pf_assign_hitbox_id feeds dmg/size from
  chd instead of assumed-pristine CTAB1 — dmg/size are the ONLY
  createHitbox fields with upstream write sites (measured by grep over
  characters/ + physics/ + main/); everything else stays CTAB1's. The
  falcon-canEdgeCancel / marth-dmg class, resolved at CLASS level.
- Structure deltas (measured per-file diffs): ROLLOUT (NSG/NSA/NSGT) is
  a movement special with its own charge/launch/turn machine on
  runtime-added phys.rollOut* (rollOutTurnTimer presence-modeled THIS
  task — rule 16); NSG/NSA mains do NOT advance timer at the top (the
  charge-scaled advance sits mid-body) and their interrupts ALWAYS
  return false (the WAIT/FALLSPECIAL arms fire the init and still
  return false — verbatim); the multijump ladder (AERIALTURN1-5 /
  JUMPAERIAL1-5, cVel.y rungs 1.65/1.59/1.47/1.36/1.25) dispatches
  through the puffNextJump/puffMultiJumpDrift helper modules with
  COMPUTED index keys ("AERIALTURN"+(1+jumpsUsed) — AERIALTURN6
  unreachable, every dispatch jumpsUsed<5-guarded); ATTACKAIRN's t===7
  increments hitboxes.FRAMES and ATTACKAIRB's t===8 writes
  phys.AUTOCANCEL (lowercase) — upstream typos on runtime-added fields;
  THROW* dispatch victims 2-ARG through the TABLE with fractional
  timers (K/releaseFrame, floor(+0.01) crossings; THROWBACK's window
  carries the floor-over-comparison typo `Math.floor(timer+0.01 < 37)`;
  THROWDOWN's crossing has NO grabbing===-1 guard where FORWARD/BACK
  do); THROWN{PUFF,MARTH,FOX}* are guarded (THROWNPUFFUP's vacuous
  `if(player[p].phys)` nesting, THROWNMARTHFORWARD's clamp-before-guard
  order, THROWNMARTH* init pos snaps, THROWNMARTHBACK's flip-without-
  negated-x) and THROWN{FALCO,FALCON}* are the old-style unguarded
  family (THROWNFALCOBACK flips without negating x; THROWNFALCONDOWN
  has reverseModel but NO flip) — per-file verbatim; CLIFF* keep the
  onLedge===-1 canGrabLedge table-write arm (C traps) and
  CLIFFGETUPQUICK alone sets ledgeRegrabCount=TRUE.
- GOLDENS: g02/g04/g08 — the puff carriers, probe-measured live
  coverage 843/1215/1037 (all three carry live puff moves; g08's CPU
  puff attacks, unlike g07's falcon). g08 carries the FIRST LIVE
  mdispatch seam of any per-char cluster: the CPU puff's THROWBACK
  dispatching fox's THROWNPUFFBACK on the victim's table.
- Value model: ONE widening — phys.rollOutTurnTimer (runtime-added by
  NEUTRALSPECIALGROUNDTURN; no prior golden fired it). The rest of the
  rollout family was modeled since task 2.

moves-puff sweep (rules 11/12, 404 calls): the task-8 scaffolding
reused (slot-3 playerObject(1,·) + characterSelections[3]=1, slot-2
injection for grabbedBy<p, sweep mulberry32, hq splice-restore, default
pre-match stage cliffs — its walls serve the onWallCollide vfx read)
with puff-measured arms; puff-specific coverage: the FULL rollout
machine both environments (charge/clamp/release charged AND uncharged,
the STALE-id dmg arms writing through jab1's global objects, distance/
wrap/decel/turn-chain arms, onWallCollide both faces + the no-op arm,
onPlayerHit ×3), sing's five size windows + both interrupt exits, rest
twins, pound both environments incl. the lsY-angle and rotated-
airVelocities arms + the SIDESPECIALAIR land write, the multijump
ladder (all 10 rung inits, the t===13 handoff, face-flip arm,
puffNextJump both branches from JUMPAERIALB/F and the >28 multijump
arms, JUMPAERIAL5's armless FALL), THROW* fractional-timer crossings +
CATCHCUT/-1 arms, all 20 THROWN* inits + guard/clamp/order arms, all 8
CLIFF*. The sweep snapshots dmg+size of the WHOLE puff charHitboxes
plane up front and restores it in the finally (rule 12 net-restore —
the rollout/sing arms mutate the GLOBAL plane; mid-sweep records
legitimately observe the mutated chd). Unswept (documented): CLIFF*
onLedge===-1 canGrabLedge table-write arm (C traps; mvData
drift-guarded), unguarded THROWN{FALCO,FALCON}* grabbedBy=-1/overrun
arms and guarded-THROWN overruns past the clamp window (upstream
throws), interrupt-tail WALK arms shadowed by checkForTilts,
AERIALTURN6/JUMPAERIAL6 (unreachable by construction).

## The article spec (M2 task 13)

Wrapped boundary (1101 fns): the 4 gameTick article pipeline calls
(destroyArticles, executeArticles, articlesHitDetection,
executeArticleHits — main.js:1059-1060/1077-1078) + resetAArticles
(endGame-only upstream, main.js:1376 — goldens never end the match:
pinned ZERO live) + articles.{LASER,ILLUSION}.init ("ainit" — the SAME
crossings tasks 8/9 recorded as 4-field article seams from the fox/falco
side, here first-class mutation records) + the 6 exported collision
helpers (internal-only callers upstream — identity-wrapped, pinned 0) +
every NON-shared actionStates entry fn as an mdispatch seam logger (the
moves-shared machinery verbatim: marth 244 / puff 221 / fox 192 /
falco 214 / falcon 217). renderArticles / LASER.draw are render-only —
asserted present, never wrapped (drawECB precedent).

GOLDENS g01/g02/g08 — probe-MEASURED live coverage over all six
fox/falco goldens (live ainit / live hit records): g01 27/12 (fox lasers,
battlefield; the 12 hits are ZERO-knockback — fox laser kg=bk=sk=0, so
percent-only), g02 19/6 (falco lasers, ystory — the ONLY live kb>0
article hits: screenShake draws + live DAMAGEN2 dispatch), g08 27/21
(fox CPU lasers, fdest; 1496 standalone AI draws). g03/g05 field ZERO
live articles; g07 spawns 19 lasers that never connect. Every golden
fields ZERO live ILLUSIONs (all live articles are lasers) — illusions
are sweep-covered only.

- **Lean-when-empty envelopes** (a measured READ-SET projection gated on
  queue emptiness): when the driving queue is empty at entry the
  upstream body reads nothing beyond the queues (its loops never run),
  so the record carries only `{aArt, ahq, dstq}` (the three article
  queues; aArt is CHECKSUM.md §2's `articles` key). Full shapes:
  * ainit / executeArticles(aArt non-empty): pre {aArt, ahq, dstq,
    playerType, players, stage} — stage is the FIVE-LIST projection
    (wallDetection's read set, the envcoll rule); post lean (inits/mains
    write only article state; events asserted empty).
  * articlesHitDetection(aArt non-empty): pre {aArt, ahq, dstq,
    playerType, players}; post {aArt, ahq, dstq, players, snd}
    (players: the canTurboCancel hasHit write; snd: foxshinereflect;
    rng/vfx asserted empty).
  * executeArticleHits(ahq non-empty): args [inputs(8-deep ×4), pre],
    pre = the moves-record superset {aArt, ahq, alias(probe4),
    characterSelections, dstq, gameMode, gameSettings{tapJumpOffp1..4},
    hq(opaque), playerType, players, stage, versusMode}; post {aArt,
    ahq, alias, dstq, hq, players, rng, snd, vfx}. Dispatched
    GUARD/SHIELDBREAKFALL/DAMAGEFLYN/DAMAGEN2/CAPTUREDAMAGE inits are
    all SHARED-origin — the replay runs task 7's real bodies; a nested
    per-char dispatch records "mdispatch" (measured ZERO; FIFO teeth
    negative-test-proven).
  * destroyArticles / resetAArticles: lean always (queue-only sets).
- **QUEUE CHAIN instrument** (fix_plan §M2 rule 18): the article queues
  are C module state chained across records — every upstream mutation
  site is a captured boundary — and the replay COMPARES the chained
  state against each in-match record's pre queues before re-marshaling
  them (authoritative). A queue mutation the C got wrong flags at the
  very next record even when that record's own body replays clean
  (negative-test-proven: post-record chain corruption → 27/19/27 pure
  chain divergences, zero record divergences). Frame-0 sweep records
  tear state down by direct pokes — the chain only arms from the first
  in-match record.
- RNG/snd/vfx: the moves discipline (owner draws in eah posts —
  screenShake's 4 per kb>0 hit; window draws in mdispatch seam posts;
  everything else standalone). hdFlags + mvData frame-0 dumps
  (finalCheck drift-guarded) serve hd_flags (crouch/vCancel) and the
  shared-body registration/data seams.
- SEAM-TO-BODY CONVERSION (the task-8/9 un-seaming): the moves-fox/falco
  replays keep their article-seam FIFOs (unchanged, still green); this
  spec captures the SAME upstream crossings at the article module
  boundary and replays them through the REAL C bodies
  (art_laser_init/art_illusion_init). The seam's verified [name,
  options] canon is byte-identical to the ainit record's args prefix, so
  task 17's integration replaces the driver's mv_article_* seams with
  direct article.c calls assembling the same options — verified here
  bit-exactly, spawn-frame main (movement + wall check) included.

article sweep (rules 11/12, 72 calls): owner slot 2 + victim slot 3 as
REAL playerObject(2,·) injections (restored; Math.random swapped for the
sweep mulberry32 that the replay mirrors on frame-0 records; queues
emptied between arms by frame-0 pokes and net-restored). Covers: fox
default-isFox / falco / partOfThrow lasers + both ILLUSION types (the
`options.isFox || true` always-true quirk arm), the posPrev ladder
(timers 2..6), timer>200 death, wall death, the duplicate-destroy
splice(-1) quirk, reflect (type-7 hitbox + DOWNSPECIAL shine sound + vel
flip + dmg*1.5, ILLUSION vel-absent arm, interpolated circle straddle),
shield hits (push both signs + the >2 cap, shieldbreak `break` arm,
powershield reflect LASER/ILLUSION), hurt arms (kb>=80 DAMAGEFLYN,
angle-361 both branches, groundBounce, vCancel, crouch, CAPTUREDAMAGE +
THROWNPUFFDOWN skip, grabbedBy kb>50, blunthit), hitList skip, the
evaluated-but-empty clank loop, hurtBoxState==1 skip, interpolated
hurt/shield arms, and a clean miss. Zero-live surfaces without a sweep
(documented): none — every reachable article arm is either live or
swept; the dead `else` of ILLUSION's ground patch (isFox always true)
is carried verbatim as an unreachable arm.

## The platforms spec (M2 task 14)

Wrapped boundary (6 fns): every VS stage object's `movingPlatforms` —
main.js:1058 calls `getActiveStage().movingPlatforms()` FIRST thing in
the mode-3 tick, so exactly one record fires per frame. Upstream bodies
(measured): battlefield/dreamland/pstadium/fdest are EMPTY; ystory is
Randall's rectangular rail machine (platform[0], no RNG, a rider arm over
ALL FOUR player slots — the loop is unguarded by playerType; inactive
slots hold page-start CSS-era playerObjects); fountain is the two side
platforms' machine: module-PRIVATE `platformStates`, seeded Math.random
draws, the `main.starting` reset arm, and player transfer arms. The
wrapper asserts `getActiveStage() === ` the owning stage object per call
(envelope coherence: the body mutates the activeStage import while the
envelope reads the stage object).

- **The platformStates getter (run-capture.js served-bytes injection #2)**:
  fountain's `platformStates` is a closure `let` no export reaches — the
  served `dist/js/main.js` gains
  `window.__mpFountainPS = function () { return platformStates; };`
  immediately after the declaration (unique-match hard-fail; the injected
  statement carries no quotes/newlines so it is safe inside the webpack
  eval-wrapped module string; behavior-neutral pure exposure — the
  `__wpCache` mechanism class). Disk is never written.
- **Envelopes** (per-stage measured read/write sets): `plat` = the FULL
  `activeStage.platform` plane in EVERY record — the rule-18 chain
  carrier (static platforms included: the chain turns the "nothing else
  writes the platform plane" enclosure assumption into a per-record
  measurement). Shapes:
  * static stages: pre/post `{plat}` (lean — the body reads nothing).
  * ystory: pre/post `{plat, players}`; players = 4×
    `{grounded, onSurface, pos}` (the read/write slice); draws asserted
    ZERO.
  * fountain: pre `{plat, players, ps, starting}`, post
    `{plat, players, ps, rng}` — ps = platformStates via the getter
    (state string "moving"|"static" ↔ C isStatic), rng = owner draws.
- **STAGE-PLANE CHAIN instrument (rule 18)**: the C replay chains the
  platform plane (+ ps on fountain) across records and compares the
  chained state against each in-match record's pre before re-marshaling
  (authoritative); frame-0 sweep records poke state directly — the chain
  arms from the first in-match record.
- RNG: rngBoot + owner draws in fountain posts; everything else
  standalone `Math.random`. No dispatch windows exist (rule 14 vacuous).
  The base-return selection arm CONSUMES its draw and ignores it —
  order verbatim (negative-test: hoisting the draw into the non-base
  arms → 199 divergences on g06).

GOLDENS g01/g02/g06: g02 (ystory) + g06 (fountain) are the only goldens
whose stage has a non-empty body (carrier membership by stage identity —
the movingPlats presence is STAB1-pinned and asserted at install); g01
(battlefield) represents the static-stage class (3600 lean records
pinning the call-per-tick contract + plane non-drift). Live coverage
measured: g06 fields the FULL fountain machine (90 starting-arm frames;
27 owner draws; sink −additionalOffset, base-return 19.875, and both
newTimer formulas all live); g02 fields Randall's full rail loop (~3
circuits, all four arms + both double-fire corners). Zero live anywhere:
the ystory rider arm and all fountain transfer arms — sweep-covered.

platforms sweep (rules 11/12, 52 calls): all four player slots as REAL
`playerObject(2,·)` injections (restored); Math.random swapped for the
sweep mulberry32 (0x0badf00d) that the replay mirrors on frame-0
records; Randall poked per arm and restored to the authored STAB1
coordinates; platformStates driven through the getter (its entry OBJECTS
are live — pokes work) and net-restored by a final starting-arm call
(the reset arm rewrites platformStates AND the platform ys to their
authored values EXACTLY — the reset IS the restore; `starting` is true
pre-setupMatch, asserted). Covers: Randall's four rail arms + the two
real double-fire corners (arm1→arm3 bottom-left, arm2→arm4 top-right —
arm order makes a bottom-right double-fire impossible: arm 2 is tested
before arm 3) + the interior no-arm rider y-snap + rider negatives;
fountain starting reset, arrival (all three newTimer arms incl.
newTimer=0 base-arrival), static decrement, all three destination-
selection arms (the t<0.3 / t>=0.3 split reached by a deterministic
re-arm loop on the fixed sweep chain), both moving directions, all four
transfer arms ([1,1]→ground, [1,2]→ground, ground→[1,1], ground→[1,2])
and three transfer negatives (x outside the band, platform static, rider
not grounded).

## The ai spec (M2 task 16)

The AI-input bridge boundary over the CPU goldens **g07/g08** (the only
goldens with a playertype-1 slot; fix_plan §M2 "AI policy": ai.js stays
JS-side until M4 — the C sim replays CPU slots from this recording).
Wrapped boundary (3 fns): `runAI` (main/ai.js; called from update(i),
main.js:902, when `!starting && actionState != "SLEEP"`), `pollInputs`,
and `physics` args-projected to `[i, inputBuffers[i]]` (the spec-input
chain pair — this spec is ALSO the first input-chain capture over the
CPU goldens).

- **runAI records** (mutation, 5-field): args `[i]`; post envelope
  `{bank, bk, rng}` — `bank` = the post-runAI `aiInputBank[i][0]` row
  (the value plane the C bridge installs; NUMBER/undefined button values
  live here, rule 16 — helper-AI literals with missing keys assign
  undefined through `inputs.<f>` reads), `bk` = the AI-private player
  bookkeeping `{ca: currentAction, cs: currentSubaction, cta:
  curentAction (upstream typo, ai.js:1254), lm: lastMash}` (recorded for
  the M4 ai.js port's future differential; consumed by nothing in M2),
  `rng` = the seeded draws consumed INSIDE the call, in order — the draw
  schedule the C sim burns at the runAI call site (a dropped draw shifts
  the whole chain).
- **The pollInputs alias**: for the CPU slot, upstream pollInputs RETURNS
  `aiInputBank[i][0]` itself (input.js:135) — the poll record's ret is
  the row read at interpretInputs time (= the PREVIOUS runAI's output,
  since runAI runs after interpretInputs inside update(i)), and the
  physics projection's slot 0 is the row's POST-runAI state (the alias
  write-through). The harness pins the CPU slot's mType to "keyboard"
  (oracle/meleelight-harness.patch), so interpretInputs takes the
  KEYBOARD arm for it — the raw*/deaden AI arm (main.js:787-795) never
  runs in the captured domain (its rawX/rawY/rawcsX/rawcsY stay at the
  inputData 0 defaults for the whole trace).
- **WRITE-SET RECONCILIATION (the task's hard-check)**: ai.js's reachable
  write surface is its import graph (player/cS/playerType from main,
  aiInputBank from input, gameSettings, activeStage) + Math.random.
  Grep-measured live assignment targets: `aiInputBank[i][0].{a,b,csX,
  csY,l,lA,lsX,lsY,x,y,z}` (11 of 22 fields; `.r` only in a comment),
  `player[i].{currentAction,currentSubaction,lastMash}` + `cpu.
  curentAction`, `window.isOffstage` (module-load fn def); the
  `player[i].inputs.*` / `phys.*` / `cpu.timer` writes are ALL inside
  block comments. Enforced at runtime EVERY runAI call: canon snapshots
  before/after of all four players (minus the four bookkeeping keys),
  all 32 bank rows, playerType, characterSelections, gameSettings,
  activeStage — any diff outside `aiInputBank[i][0]` pushes a `wsViol`
  record (pinned ZERO) and fails the spec's finalCheck. Single-threaded
  synchronous JS: between the snapshots only runAI runs.
- **AIBRIDGE1 artifact** (`<id>.ai-bridge.txt`, built by
  `build-ai-bridge.js`, format spec in `port/sim/ai_bridge.h`): the
  distilled per-runAI schedule — frame, slot, draw values (bit
  patterns), the 22-field row (B0/B1/U/N<hex16> tokens, canon key
  order). Deterministic transform of the capture (built ×2 + cmp). The
  C replay is DRIVEN by the artifact (task 17's consumption path) and
  cross-checks it against every runAI record; `ml_ai_bridge_apply`
  additionally verifies the artifact's never-AI-written fields
  (dd,dl,dr,du,r,rA,raw*,s) against the C chain before installing —
  the recording cannot smuggle state through fields the AI provably
  does not write.
- **RNG**: rngBoot + attributed runAI draw lists + everything else
  standalone `Math.random` records pushed at draw time (chain-order
  faithful). runAI never nests and dispatches nothing (rule 14 vacuous;
  the wrapper asserts no nesting).
- **No sweep** (rule 11 n/a): the boundary is data-driven — the
  recording IS the behavior; there is no reachable-but-unexercised C
  logic arm (the tagged interpretInputs core's non-keyboard arms are
  the plain core's, already covered by task 3's captures + traps).
- **Rule-18 chain**: the bank row is chained in C across the whole
  trace; every CPU-slot poll ret is compared against the chained row —
  an unrecorded upstream bank write (or a wrong C install) flags at the
  very next record.

Value-model note (rule 16 class fix): `port/sim/input/ai_input.h` now
holds THE interpretInputs implementation, over tagged JS values
(bool|number|undefined per field, `MlAiVal`); the plain-MlInput
`ml_interpret_inputs` is a conversion wrapper around it (human slots'
measured domain is all-bool buttons — asserted on exit). Task 3's
check-input-replay.sh re-verified bit-exact after the rebase.

## The undef-ret allowlist (rule 8)


The no-undef-in-returns pin (soundness of the marshaller's
ToNumber(undefined)→NaN argument mapping) applies per function, not
blanket: ACCESSOR-class functions (e.g. getXOrYCoord — a raw property
read) echo undefined VERBATIM; JS converts only at the consumer's
arithmetic. Such functions are frozen in the spec's expectations file
(`undefRetAllowed`) and their C translations preserve undef-ness
explicitly (JsNum/JsVec2D in vec2d.h). Everything else keeps the strict
invariant.

## The format differential (M2 task 15)

Not a capture spec: `check-format.sh` proves the C ECMAScript formatter
(`port/sim/ml_fmt.c` — the vendored Ryu core under `port/ryu/`, byte-
verbatim at a pinned commit, + the ECMA-262 §6.1.6.1.20 formatting
layer) and the CHECKSUM.md §3 `ser` / §4 hash (`port/sim/ml_ser.c`,
hashing via `oracle/qjs/sha256.c`) byte-identical to the JS oracle by
differential testing:

- **adversarial**: `fmt_diff --gen` emits a deterministic seeded corpus
  (~5.47M bit patterns: specials incl. payload NaNs, every exponent ×
  mantissa templates, subnormal ladders, powers of 2/5/10 ± ulp
  spreads, the 1e21 / 1e-7 ECMA threshold straddles, known-hard
  shortest-repr literals, millions of splitmix64 raw/structured
  patterns; corpus sha256 pinned in `expected-format.json`);
  `fmt_diff --format` (C: `String(x)` TAB ser-num with the `-0` token)
  vs `fmt-js-ref.js` (V8 `String(x)` + numStr EXTRACTED from
  pagelib.js's own source bytes) — `cmp`.
- **captured**: `fmt_diff --extract` scans EVERY `build/*.jsonl` capture
  for canon `d:<hex16>` tokens (all specs/goldens present; g01
  player+article recorded first when absent) and runs the unique set
  through the same differential.
- **composite**: `fmt-composite.js gen` builds a shared case file from
  the g01 player/article captures — every post snapshot and article
  envelope as `V` cases, per-frame CHECKSUM.md §2/§3.1 envelopes
  (fixed-literal key order, slot snapshots + that frame's post `aArt`
  queue, plus synthetic 4-slot envelopes) as `E` cases; `fmt_diff
  --composite` (C parse→ser→SHA-256) vs `fmt-composite.js ref` (the
  ORACLE'S OWN `ser`/`__serializeState`/`__sha256` run under
  `window === global`) — per-case hash + byte length, `cmp`.

Number-domain note (measured): three g01 composite cases carry live
`-0` — the `-0` ser token is live-covered, not just corpus-covered
(dropping it diverges 3 composite cases + 3294 adversarial lines).

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
- `fmt_diff.c` / `fmt-js-ref.js` / `fmt-composite.js` /
  `check-format-pins.js` / `expected-format.json` — the M2 task 15
  format-differential rig (see "The format differential" above; driver
  `check-format.sh`)
