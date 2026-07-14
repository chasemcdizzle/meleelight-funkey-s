# oracle/CHECKSUM.md — the frozen per-frame checksum specification

**Spec version: 1** (frozen 2026-07-14, M0 task 4). This document is
NORMATIVE and part of the oracle: CLAUDE.md HARD RULE 3 applies (never
edit/weaken; only M0 tasks may write `oracle/`). It is the contract the
browser oracle harness implements today and the contract the C port (M2)
must implement byte-for-byte. Every rule below cites the code that defines
it; the code paths cited are the source of truth this spec was transcribed
from — if code and spec are ever found to disagree, that is a defect to be
raised, not silently resolved.

Implementations:
- Browser oracle: `oracle/harness/pagelib.js` (serialization + hashing +
  step loop), `oracle/harness/init.js` (PRNG + clock + fdlibm shim),
  `oracle/meleelight-harness.patch` (game-side hooks).
- Upstream citations are to `schmooblidon/meleelight` @ pin `27af171`
  WITH `oracle/meleelight-harness.patch` applied (the tree
  `oracle/build-upstream.sh` produces), e.g. `upstream:src/main/player.js`.

## 1. Conformance channels

A run of a golden trace produces, and a conforming implementation must
reproduce:

1. **The checksum stream** (binding, primary): one SHA-256 hash per
   simulated frame, frames `1..N`, compared by EXACT string equality over
   the FULL trace length. Never epsilon, never prefix (CLAUDE.md rule 3).
2. **The RNG draw count** (binding, secondary): the cumulative number of
   seeded-PRNG draws at end of run must be equal
   (`oracle/harness/compare.js:21-22` reports equal/DIFFER). Equal hashes
   with unequal draw counts = nonconforming (latent divergence).
3. **Off-step draw diagnostic** (diagnostic, expected value): draws made
   outside a sim step are counted separately
   (`oracle/harness/init.js:49-54`). Expected value under this spec: exactly
   **1** (the `startGame` background draw, §6) — any other value means a new
   off-tick consumer leaked onto the gameplay stream and must be
   investigated. Observed 1 in every recorded run to date.

## 2. Checksum surface — the field list

Defined in `oracle/harness/pagelib.js:41-64` (`__serializeState`). The
state string is a hand-built envelope:

```
{"p0":{...},"p1":{...},"articles":[...]}
```

- **Per ACTIVE player** (slot `i` in `0..3` with `playerType[i] > -1`,
  read via the `__harness.getPlayers()` / `getPlayerType()` hooks,
  `oracle/meleelight-harness.patch:99-100`), key `"p<i>"`, exactly these
  seven fields of the upstream `playerObject`
  (`upstream:src/main/player.js:127-167`):

  | key | upstream field | def |
  |---|---|---|
  | `actionState` | `player[i].actionState` (string) | player.js:128 |
  | `timer` | `player[i].timer` (action-state frame timer) | player.js:130 |
  | `percent` | `player[i].percent` | player.js:147 |
  | `stocks` | `player[i].stocks` | player.js:148 |
  | `hit` | `player[i].hit` (knockback/hitlag/hitstun/angle/hitPoint/powershield/shieldstun) | player.js:138-146 |
  | `hitboxes` | `player[i].hitboxes` (createHitboxes: active/frame/id[4]/hitList) | player.js:137, 17-24 |
  | `phys` | `player[i].phys` — the ENTIRE `physicsObject` (pos, velocities, ECBs, shield, ledge, timers, …) | player.js:127, 25-125 |

- **Articles**: key `"articles"` = the live `aArticles` queue
  (`upstream:src/physics/article.js:24`, exposed via `getArticles()`,
  patch:101), serialized in full — each entry is
  `{name, player, instance}` with the article's whole physics instance
  (pos/posPrev/hb/ecb/…, article.js:55-67).

Inactive slots are omitted entirely (no `"p2"`/`"p3"` key for a 2-player
match). ~10.6 KB serialized per frame for 2 players (spike measurement).

**Exclusions** (allow-list is exhaustive — anything not listed is NOT
hashed): see §7 for `percentShake` (the load-bearing exclusion) and the
rationale for the other unlisted fields.

## 3. Serialization rules (function `ser`, `oracle/harness/pagelib.js:10-37`)

The state string is built by these exact rules; the C port must reproduce
the bytes, not just the values.

1. **Envelope keys are FIXED-ORDER literals, not sorted**
   (pagelib.js:50-62): active players in slot order `p0,p1,p2,p3`, then
   `articles`; within each player block the fixed order
   `actionState, timer, percent, stocks, hit, hitboxes, phys`. (Note this
   is NOT the sorted order — sorted would put `hit` second. PLAN §3's
   shorthand "sorted keys" is precise only for nested objects; this
   envelope rule is the binding one. Recorded as a spec-precision finding,
   not a code change.)
2. **Nested objects: own enumerable keys, sorted** by JavaScript default
   `Array.prototype.sort()` — UTF-16 code-unit lexicographic (all keys in
   the domain are ASCII, so bytewise lexicographic) — each emitted as
   `JSON.stringify(key) + ":" + value` and joined with `,` inside `{…}`
   (pagelib.js:30-33).
3. **Arrays AND typed arrays** (`Array.isArray(v) || ArrayBuffer.isView(v)`)
   serialize element-by-element over `0..length-1`, joined with `,` inside
   `[…]` (pagelib.js:25-28). Typed arrays thus look exactly like plain
   arrays of their numbers.
4. **Numbers**: ECMAScript `String(x)` — the ECMA-262 `Number::toString`
   shortest-round-trip decimal, injective on doubles (integers without
   `.0`; exponent form exactly where ECMA-262 mandates it) — EXCEPT
   negative zero, which `String` erases and the spec makes explicit as the
   token `-0` (`Object.is(v, -0)` check, pagelib.js:10-13). `NaN`,
   `Infinity`, `-Infinity` serialize as those exact unquoted tokens
   (fall out of `String(x)`, pagelib.js:12). The C port needs an
   ECMAScript-compatible shortest-float formatter (Ryu/Grisu class) plus
   the `-0` special case.
5. **Strings**: `JSON.stringify(s)` (pagelib.js:18) — double-quoted with
   JSON escaping.
6. **Booleans**: `T` / `F` unquoted (pagelib.js:19).
7. **`null`** → `null`; **`undefined`** → `undef` (pagelib.js:15,20).
8. **Functions** → the token `fn` (pagelib.js:21).
9. **Cycles** → the token `cyc`, detected PATH-based: the seen-set is
   added-to before recursion and deleted-from after (pagelib.js:23-24,35),
   so only true ancestor cycles collapse; shared (DAG) references
   serialize in full at every occurrence. One shared seen-set spans the
   whole frame snapshot (pagelib.js:45).
10. **Domain assumption**: values reachable from the surface are only
    null/number/string/boolean/undefined/function, plain objects, arrays,
    and numeric typed arrays. (`ArrayBuffer.isView` would also match a
    `DataView` and emit `[]`; no `DataView`, `Symbol`, or `BigInt` exists
    in the domain — encountering one is a spec violation to raise.)

## 4. Hash

SHA-256 over the **UTF-8 encoding** (`TextEncoder`) of the serialized
state string; output is **lowercase hex, 64 chars**
(`oracle/harness/pagelib.js:66-73`, WebCrypto `crypto.subtle.digest`).
One hash per frame; the stream is the ordered list
`[{f: 1, h: "…"}, …, {f: N, h: "…"}]` (pagelib.js:104).

## 5. Frame boundary semantics

The step loop (`oracle/harness/pagelib.js:86-120`, driven in chunks of 120
by `oracle/harness/run.js:170-177` — chunking is transport only, no
semantic effect):

For frame number `F = f+1` where `f` = frames already completed (0-based):

1. **Inputs**: `window.__harnessInputs = trace[min(f, trace.length-1)]`
   (held-last past trace end; pagelib.js:93-96). The trace is indexed by
   PRE-step frame count: `trace[0]` feeds the first tick. Injection point:
   top of `pollInputs` for human slots; AI slots keep reading
   `aiInputBank` (`oracle/meleelight-harness.patch:9-15`). Inputs during
   the match `starting` window (until ~frame 91) do not affect the sim.
2. **Virtual clock** advances exactly `1000/60` ms (pagelib.js:97;
   `performance.now`/`Date.now` are backed by it, init.js:58-66 — proven
   irrelevant to sim state, kept as hygiene).
3. **One tick**: `__inStep = true; __harness.step(); __inStep = false`
   (pagelib.js:98-100) where `step()` = `gameTick(window.__nextInputBuffers)`
   (patch:98) — the full upstream sim frame; in harness mode `gameTick`
   stores next-frame buffers instead of re-arming `setTimeout` (patch:49-53).
4. **Checksum**: the state is serialized and hashed AFTER the tick
   returns (pagelib.js:102-104). Frame F's hash = post-tick state after F
   ticks since `setupMatch`/`startGame`.

Frame numbering is **1-based**; the pre-frame-1 state (right after match
setup) is never hashed. Informative anchor: with seed 1337, P1 fox / P2
marth / battlefield, frame 1's hash is
`9f4c6df778506d64ae34dde40dcb2f85c8bd38ff2214261cdd32e0cd3f3fb22d`
(identical across every recorded run of that config to date; the frozen
golden streams land in M0 task 5).

## 6. The RNG channel

- **Generator**: **mulberry32**, seeded with the run's uint32 seed,
  installed as `Math.random` BEFORE any page script runs
  (`oracle/harness/init.js:32-56`). Exact algorithm (init.js:32-40):

  ```js
  a |= 0; a = (a + 0x6D2B79F5) | 0;
  let t = Math.imul(a ^ (a >>> 15), 1 | a);
  t = (t + Math.imul(t ^ (t >>> 7), 61 | t)) ^ t;
  return ((t ^ (t >>> 14)) >>> 0) / 4294967296;
  ```

  All ops are 32-bit integer (`imul`, shifts); the draw is
  `u32 / 2^32 ∈ [0,1)`. Trivially portable to C (`uint32_t` state).
- **Counting**: every wrapper call increments `__rngCalls`; calls with
  `__inStep == false` also increment `__rngCallsOutsideStep`
  (init.js:47-56). Counters reset at match setup, before `startGame`
  (`oracle/harness/run.js:150`), so counts cover the match only.
- **The one legitimate off-step draw**: `startGame` consumes exactly one
  seeded draw for the cosmetic background pick
  (`upstream:src/main/main.js:1322`, `setBackgroundType(Math.round(Math.random()))`)
  during `setupMatch`, before frame 1. It IS part of the seeded stream —
  the C port's sim PRNG must burn one draw at match start (or the port
  must document pinning the stream offset by other means) — and it is the
  expected `rngCallsOutsideStep == 1` of §1.3.
- **Reference draw counts** (golden #1 class, 3600 frames): 134 total
  (2 humans; incl. the setup draw), 637 with one CPU (spike table,
  `docs/research/determinism-spike.md`).
- **What is NOT on the stream**: `percentShake` (§7) draws from the
  stashed native RNG (`window.__nativeRandom`, init.js:44-45) and bypasses
  the wrapper entirely — its draws appear in NO counter and perturb
  nothing.
- Transcendentals are pinned separately: the harness shims
  sin/cos/tan/atan/atan2/pow to the vendored fdlibm by default
  (init.js:22-29; `port/fdlibm/`); the frozen streams are fdlibm streams.

## 7. The percentShake exclusion (and exclusions generally)

`player[i].percentShake` (`upstream:src/main/player.js:153`) is **excluded
from the checksum surface**, by two independent mechanisms, both required:

1. **Not serialized**: it is not one of the seven allow-listed player
   fields (§2) — the spike identified it as the ONLY player-object field
   written off-tick.
2. **RNG-isolated**: upstream wrote it from wall-clock `setTimeout`
   callbacks (20/40/60/80 ms) calling `Math.random` — off-step,
   wall-clock-timed draws that stole from the shared seeded stream and
   desynced CPU matches run-vs-run (spike, divergence at ~frame 2690 with
   draw counts 487 vs 525). The harness patch routes it to the native RNG
   (`oracle/meleelight-harness.patch:26-41`; patched
   `upstream:src/main/main.js:361-372`).

**Rationale**: it is HUD-only visual shake (the percent display wiggle) —
no sim code reads it back — and it is written at wall-clock-dependent
times, so hashing it would make the stream timing-dependent by
construction. Class lesson (spike zoom-out): gameplay RNG must be a
dedicated, seeded, tick-owned PRNG; cosmetic/async consumers (vfx, HUD
shake, KO shouts, menu sparkle — ~127 of 134 upstream `Math.random` sites)
must not share it. The C port implements this structurally.

**Other unlisted state** (informative rationale, same allow-list rule):
render/HUD-only fields (`rotation`, `colourOverlay`, `miniView*`,
`showECB`, …), global presentation state (`matchTimer` HUD value, camera /
`screenShake`, vfx queue), and sim-internal player fields not in the seven
(`prevActionState`, `lastMash`, `hasHit`, `shocked`, `burning`, …) are not
hashed. The surface is a divergence DETECTOR, not a full state dump: any
divergence in unlisted sim-affecting state provably propagates into
`phys`/`timer`/`actionState`/`hit` within frames (3600-frame spike
evidence + experiment B catching a single 1.0-unit `phys.pos.x` wiggle).
Growing the surface (e.g. M4's sound-event queue, PLAN §3) is a spec
version bump per §8.

## 8. Versioning — the freeze rule

**Any change to any normative rule in this file invalidates every frozen
checksum stream.** Specifically:

- Normative content = §§1-7: the field list and envelope, every
  serialization rule, the hash and its encoding, the frame-boundary
  semantics, the PRNG algorithm/seeding/draw sites, the exclusions.
- A change to ANY of it (including "harmless" additions like a new
  serialized field, a float-format tweak, hashing pre-tick instead of
  post-tick, or moving the `startGame` draw) produces a DIFFERENT stream
  for the same trace. Old and new streams are incomparable; exact
  equality across spec versions is meaningless.
- Therefore: any such change requires (a) bumping **Spec version** at the
  top of this file, (b) re-recording and re-freezing EVERY golden stream
  (`oracle/goldens/*.sha256.json`) against the new spec IN THE SAME
  CHANGE, and (c) re-running the M0 exit gate (`oracle/verify_goldens.sh`)
  before anything downstream consumes the streams. A frozen stream file
  is only valid against the spec version it was recorded under.
- Guardrails already in force: CLAUDE.md HARD RULE 3 (never weaken the
  oracle; exact equality never becomes epsilon; only M0 tasks write
  `oracle/`), and writer ≠ checker (rule 7). After M0, editing this file
  outside an explicit, Chase-visible re-freeze is a rule-3 violation.

## 9. Findings recorded while freezing (code read end-to-end 2026-07-14)

No code changes were needed; two precision points and one behavior are
pinned here so the C port doesn't guess:

1. Envelope key order is fixed-literal, NOT sorted (§3.1) — the "sorted
   keys" shorthand in PLAN §3 / fix_plan under-specified this.
2. Cycle detection is path-based; shared references duplicate (§3.9).
3. The seeded stream includes exactly one off-step draw
   (`startGame`/`setBackgroundType`) before frame 1 (§6) — visible as
   `rngCallsOutsideStep == 1` in every recorded run.
