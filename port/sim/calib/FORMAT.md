# port/sim/calib — module-boundary capture format (M2-CAL)

Records every call crossing the exported boundary of
`src/physics/environmentalCollision.js` during an oracle-harness replay of
a golden trace: arguments + return value, per frame. The C translation
(`port/sim/environmental_collision.c`) is replayed against these records
bit-exactly by `port/sim/calib/replay_envcoll.c`.

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

## canon v1 — value serialization

CHECKSUM.md §3's structural rules with ONE deviation: numbers are IEEE-754
bit patterns, not shortest-round-trip decimals.

- number → `d:` + 16 lowercase hex digits (big-endian IEEE-754 double bit
  pattern). Injective on doubles, distinguishes `0`/`-0` and every NaN
  payload; strictly bit-exact — a single-ulp difference is a divergence.
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

## Files

- `<golden-id>.envcoll.jsonl` — the boundary records (gitignored, in
  `port/sim/calib/build/`)
- `<golden-id>.capture-run.json` — the run JSON (meta/coverage/frames)
  used for the verify-stream guard
- `expected-capture.json` — measured-then-frozen per-function call-count
  pins per golden (drift alarm, M1 instrument class)
