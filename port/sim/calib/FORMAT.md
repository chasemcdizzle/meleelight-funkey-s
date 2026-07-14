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

## JSONL record format

One line per boundary call, tab-separated, in call order:

```
<frame> TAB <fn> TAB <args-canon> TAB <ret-canon> LF
```

- `frame`: 1-based sim frame being ticked (`__frameCount + 1` while
  `__inStep`); `0` = outside a sim step (match setup / stage init).
- `fn`: the exported function name.
- `args-canon`: canon-v1 serialization of the argument list (as an array).
- `ret-canon`: canon-v1 serialization of the return value.

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
  (checked by `check-spec-pins.js`)
