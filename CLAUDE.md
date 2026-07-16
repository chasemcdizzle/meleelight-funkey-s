# CLAUDE.md — always-loaded rules for the meleelight → FunKey-S port

Faithful C port of browser meleelight (upstream pin `27af171`) to the
FunKey-S, verified by deterministic input-replay + per-frame state
checksums against the browser original. Spec: [`PLAN.md`](./PLAN.md) ·
Loop: [`docs/LOOP.md`](./docs/LOOP.md) · Checker:
[`docs/loop/CHECKER.md`](./docs/loop/CHECKER.md) · Replanner:
[`docs/loop/REPLAN.md`](./docs/loop/REPLAN.md) · Licensing:
[`docs/LICENSING.md`](./docs/LICENSING.md) · Evidence:
`docs/research/`, `spikes/`, `prototypes/`.

## HARD RULES (non-negotiable; every iteration)

1. **Behavior > compiles.** Code must do what's asked, validated against
   the oracle (checksum conformance), never "it builds".
2. **No stubs / placeholders / hardcoded outputs / "TODO later"** as a
   stand-in for real work. Deferrals go under `BLOCKERS` in
   `docs/AGENT-LOG.md`, never buried in code.
3. **Never edit/delete/weaken** any test, the oracle (`oracle/` once it
   exists, `spikes/determinism/` harness + committed golden checksum
   streams), the gates, this file's rules, `docs/LOOP.md`, `docs/loop/*`,
   the caps, or git hooks. Exact-equality checksums NEVER become
   epsilon comparisons. Only M0 tasks may write `oracle/`.
4. **Git safety:** the autonomous run lives on ONE branch, **`agent/auto`**
   (the loop aborts anywhere else); ONE atomic commit per completed
   iteration, clean tree between; **never** force-push, `reset --hard`, or
   delete branches; `main` receives only human/spec commits. **NEVER add,
   push to, or open PRs against `schmooblidon/meleelight` or any upstream
   remote** — upstream clones live outside the tree. **No distribution of
   anything** (binaries, OPKs, assets, forks): private project.
5. **Faithfulness:** the browser original is ground truth; a behavioral
   deviation is a bug even when it "feels better". Engine values (physics
   constants, frame data, thresholds) come from the executed-data pipeline,
   never retyped by hand. Sim math is doubles-only, vendored fdlibm for
   transcendentals, `-ffp-contract=off` on every TU (PLAN §2).
6. **One task per iteration.** Command output → `.loop/*.log`, never into
   the conversation.
7. **Writer ≠ checker.** Completion is confirmed by the CHECKER
   ([`docs/loop/CHECKER.md`](./docs/loop/CHECKER.md)) gating on
   artifacts/exit-codes, never self-claims.
8. **ZOOM OUT (Chase, 2026-07-14).** Before and after fixing anything, ask
   whether it is an instance of a CLASS with a systematic cause; prefer the
   class-level fix when tractable. Hierarchy: **instrument > class fix >
   registered one-off > silent one-off (never)**. One-offs are acceptable
   late-stage/perf only AFTER measurement attributes the hotspot. Every
   root-cause/fix session ends with an explicit zoom-out note in
   `docs/AGENT-LOG.md`.

## SDL / platform seam (single source of truth)

**Thin platform API, three backends, exactly ONE TU linked per target**
(lifted from ssb64's `port/gfx` pattern): SDL 1.2 for the FunKey device ·
SDL2 for host dev · headless for CI/the loop. The renderer knows nothing
about SDL; it exposes a framebuffer the backend presents. Seam:
`platform_init / platform_present / platform_poll / platform_quit` + an
input struct. The headless backend is what makes the autonomous loop
possible — it lands in M2 with the first C build.

## Licensing / provenance rule

Upstream meleelight LICENSE carried **verbatim** (incl. its Nintendo-IP
rider) as `LICENSE-meleelight` — never edit it. `NOTICES` gains an entry
BEFORE any third-party code lands in-tree. No Nintendo-derived asset is
ever distributed (moot privately; kept as hygiene). SDL 1.2 is LGPL:
dynamic linking only. Details: `docs/LICENSING.md`.

## §Gates — MILESTONE EXIT gates only

Run via CHECKER **only on a phase-advance iteration**, never per task.
Per-iteration verification uses the in-progress `fix_plan.md` item's exact
`done-check:` instead. Cells marked *(REPLAN)* are precise definitions
whose runnable command is milestone output — REPLAN concretizes the exact
command into §Commands when the milestone becomes current; the definitions
live in PLAN §4 and are binding.

| Phase | Gate command | Pass condition |
|---|---|---|
| M0 | seed (spike-era, runnable today): `cd spikes/determinism && bash run-experiments.sh "$MELEELIGHT_CLONE"` — final form *(REPLAN)*: `bash oracle/verify_goldens.sh` | every golden trace: two fresh browser runs bit-identical, streams match committed checksums, fdlibm-patched QuickJS runtime reproduces them (5 chars / 6 stages covered) |
| M1 | *(REPLAN)* — two fresh pipeline runs + manifest hash check | byte-stable reruns; coverage = 754 anim files / ~27.9k paths / 5 chars / 6 stages / ~180 SFX + 8 tracks |
| M2-CAL | *(REPLAN)* — slice replay: C `environmental_collision` vs JS over the 3800-frame golden trace | bit-identical full trace + converged burn-down + recorded metrics (div/KLOC, fix-rate, projection). NO-GO → `LOOP STOP: m2-entry-no-go` (a blocker list is NOT a pass) |
| M2 | *(REPLAN)* — headless C sim replays all golden traces | every frame checksum == browser oracle stream, full match length, all goldens |
| M3 | *(REPLAN)* — device conformance + perf run over ADB | device checksums conform; p99 frame < 16.67 ms full match w/ audio; OPK launches from frontend. **HUMAN ESCALATION**: needs the physical device when absent (`LOOP STOP: m3-device`) + Chase's S1 ratification playtest |
| M4 | *(REPLAN)* — full-game trace suite on device | menu-flow scripts + match + target-test traces conform at 60 fps with audio; then **Chase acceptance playthrough** (`LOOP STOP: m4-complete`) |

**Gate concretization (enforced):** when a phase becomes current, REPLAN
turns two distinct things into exact runnable commands (no `…`): (a) each
task's `done-check:` (proves ONE item; every task iteration) and (b) the
phase EXIT gate above, recorded into §Commands (proves the WHOLE phase;
phase-advance only). CHECKER rejects any non-runnable/placeholder check.

## §Commands (verified; the loop appends as each phase defines them)

- **M0 EXIT GATE (concretized by REPLAN, iter 1):**
  `bash oracle/verify_goldens.sh` — for EVERY golden in
  `oracle/goldens/manifest.json`: two fresh browser runs
  (`oracle/harness/run.js`, fdlibm JS shim active) produce checksum
  streams bit-identical to each other AND to the committed
  `oracle/goldens/*.sha256.json`; the fdlibm-patched QuickJS runtime
  (`oracle/qjs/replay.sh`) reproduces the same stream; manifest coverage
  asserts all 5 characters, all 6 VS stages, and ≥1 CPU trace.
  Exact-equality per frame hash, full trace length; any mismatch,
  missing artifact, or coverage shortfall → nonzero exit.
- **M1 EXIT GATE (concretized by REPLAN, iter 9):**
  `bash pipeline/verify_pipeline.sh` — runs the FULL executed-JS data
  pipeline (`pipeline/run.js`, every registered stage) twice fresh into
  `pipeline/build/gate-{a,b}`, asserts the two `manifest.json` are
  byte-identical, re-hashes every artifact against its manifest entry,
  and asserts the pinned coverage contract `pipeline/expected.json`
  (PLAN §4 M1's counts in exact measured-then-frozen form: 754 animation
  files reconciled = 744 executed states + 5 index.js + 5 dead falco
  files; 27,808 paths; 5 chars; 6 stages; 204 SFX wavs with 180 mapped
  sounds; 8 music tracks). Prints `PIPELINE OK`, exit 0; any byte diff,
  hash mismatch, or coverage shortfall → nonzero.
- **Pipeline run (M1 task 1 committed form):**
  `node pipeline/run.js --out pipeline/build/dev [--only animations]
  [--dist "$MELEELIGHT_CLONE"]` — executed-JS serialization of the data
  plane. The animations stage `require`s the BUILT
  `dist/js/animations.js` under a `window` shim in plain node (the
  bundle is pure data construction — Int16Array literals, no DOM, no
  Math — so node vs browser is engine-neutral; verified identical
  counts), emits per-char `ANIM1` binaries (spec `pipeline/FORMATS.md`,
  little-endian pinned, decoder `pipeline/lib/animbin.js` round-trips
  every coord in-run) + one deterministic manifest (sorted keys, no
  timestamps/abs paths). Task-level check:
  `bash pipeline/check-animations.sh` → `ANIMATIONS OK`, exit 0.
- **Engine-table extractor + generated C tables (M1 task 2 committed
  form):** `bash pipeline/extractor/build-extractor.sh` (idempotent,
  stamp-cached; `--force` rebuilds) copies
  `pipeline/extractor/extractor.{entry,config}.js` into the clone and
  webpacks `dist/js/extractor.js` under docker node:8 with upstream's own
  toolchain (babel query mirrors the game build's happypack loader); the
  entry imports ONLY the per-char attributes/ecb data modules (no
  characters/<char>/index.js — that's the god-module boundary) and assigns
  the live `src/main/characters.js` registries to `window.__tables`.
  Stage `tables` (in `node pipeline/run.js`) executes it under a window
  shim and emits `ml_tables.{h,c}` + `tables.json` (format CTAB1,
  FORMATS.md §3): doubles as `UINT64_C(0x…)` bit patterns + shortest
  round-trip decimal comment, decoded via `ml_f64()` memcpy; ints as
  int32/int16 with generator hard-throws on typing violations. Task
  check: `bash pipeline/check-tables.sh` → `TABLES OK` (byte-stability ×2,
  artifact hashes, expected.json coverage, framesData/ECB↔ANIM1 pinned
  reconciliation via `lib/tables-anim-xref.js`, and the round-trip gate:
  `cc -ffp-contract=off` compiles `lib/tables_check.c` + generated
  `ml_tables.c`, its canonical leaf dump must be byte-identical to
  `lib/tables-dump.js`'s fresh executed-JS walk — 38,832 leaf values).
  Gotcha class: framesData/ECB vs animation frame counts is NOT uniform
  equality upstream (26+27 differ, 4+13 no-anim states incl. puff DEAD*
  empty ECBs kept verbatim as frameCount 0/NULL) — cross-checks against
  executed data must be measured-then-frozen reconciliations, never
  assumed identities.
- **Stage geometry → generated C tables (M1 task 3 committed form):** the
  extractor bundle additionally exposes `window.__stages` (upstream's own
  `src/stages/vs-stages/vs-stages.js` aggregator). Stage `stages` (in
  `node pipeline/run.js`) emits `ml_stages.{h,c}` + `stages.json` (format
  STAB1, FORMATS.md §4): 6 VS stages in oracle `--stage` id order, doubles
  as `UINT64_C(0x…)` bit patterns (decode `ml_stage_f64()` — distinct name
  so ml_tables.h can share a TU), ints int32 with hard-throws, empty
  surface lists kept verbatim (fdest platforms, ystory ceilings: count
  0/NULL). Task check: `bash pipeline/check-stages.sh` → `STAGES OK`
  (byte-stability ×2, artifact hashes, expected.json per-stage pins, and
  the compiled-C vs fresh executed-JS dual-dump round trip, 412 leaves).
  Gotcha class (god-module boundary, 2nd instance): ystory/fountain
  top-level-import `main/main`/`stages/activeStage`/
  `physics/environmentalCollision` but only dereference them inside
  their movingPlatforms/updatePlatform bodies (M2 sim logic) — webpack
  `externals` stubs those EXACT request strings (`"var {}"`, incl. the
  RELATIVE forms `../activeStage` etc., matched pre-resolve), and
  build-extractor.sh hard-fails on any `document.` in the bundle (leak
  guard; beware: a `document.` in an entry-file COMMENT trips it —
  comments are bundled verbatim). Upstream renders VS stages from the
  SAME structures physics collides against (one source of truth) —
  `box`/`target`/`background`/`polygonMap` are target-stage machinery,
  pinned empty/absent, schema hard-throws on drift. fdest `ledgePos`
  x=±68.4 while its ground runs ±85.6 — authored upstream quirk
  (battlefield copy-paste), carried verbatim: faithfulness > plausibility.
- **Audio conversion + sound map (M1 task 4 committed form):** the
  extractor bundle additionally executes upstream's own `main/sfx` +
  `main/music` (Howl capture shim + browser-parity `window === global`
  shim in `tables-schema.js loadExtractor` — sfx.js reads back its own
  `window.changeVolume` as a bare global, a detached window object breaks
  it) → `window.__sounds`. Stage `audio` (in `node pipeline/run.js`)
  ffmpeg-converts all 204 `dist/sfx/*.wav` → `audio/sfx/*.pcm` (22050 Hz
  MONO S16LE raw) and all 8 `dist/music/*.ogg` → `audio/music/*.pcm`
  (22050 Hz STEREO S16LE raw; device streams from SD per PLAN §7) and
  emits `sounds.json` (format SND1, FORMATS.md §5): 180 Howl names →
  blob + effective volume (post-load `changeVolume` value; authored
  cfgVolume kept as provenance) + loop, 8 music tracks with sprite
  Start/Loop windows, per-char `actionSounds` state→[[frame,sound]]
  schedules (referential integrity hard-throw). ffmpeg pinned three ways
  in expected.json audio (version — stage hard-fails on mismatch BEFORE
  converting; exact argv; aggregate artifact sha256): a different ffmpeg
  build fails loudly, never drifts. C emission deferred to the M4 mixer
  task BY JUDGMENT (FORMATS.md §5.4 — no C consumer until then). Task
  check: `bash pipeline/check-audio.sh` → `AUDIO OK` (byte-stability ×2,
  artifact hashes, expected.json pins incl. blob shape bytes ≡ 0 mod
  frame size, no-commit guard). PROVENANCE: blobs are Nintendo-derived,
  PRIVATE USE ONLY, gitignored build output only — never distributed,
  never committed. Gotcha class (browser-global identity, 3rd god-module
  cousin): upstream modules assign `window.X` then read bare `X` —
  works in browsers where window IS the global; any node-side shim must
  be `global.window = global`, not a plain object (qjs shim class:
  parity of paths, not just survival).
- **M2-CAL EXIT GATE (concretized by REPLAN, iter 15):**
  `bash port/sim/check-envcoll.sh` — implements PLAN §4/M2-CAL: builds the
  structure-parallel C `port/sim/environmental_collision.c` (+ util slice)
  with `cc -ffp-contract=off`, ensures module-boundary captures exist for
  g01/g04/g06 (records them via `port/sim/calib/run-capture.js` when
  absent — each capture run's checksum stream MUST verify against the
  frozen golden via the unchanged `oracle/harness/verify-stream.js`, so
  instrumentation cannot perturb the sim), then replays EVERY captured
  call of ALL three captures through the C module comparing canon-v1
  serializations (IEEE-754 bit-pattern hex — exact equality, a single ulp
  fails) and asserts `docs/M2CAL-REPORT.md` carries the burn-down metrics
  (div/KLOC, fix-rate, projection, go/no-go). Prints `ENVCOLL MATCH`,
  exit 0; any divergence, stream mismatch, or missing metric → nonzero.
  NO-GO handling per LOOP §H: sentinel `LOOP STOP: m2-entry-no-go`.
- **M2 EXIT GATE (concretized by REPLAN, iter 19):**
  `bash port/sim/check-sim.sh` — implements PLAN §4/M2's EXIT verbatim:
  builds the headless C sim (every TU `cc -O2 -ffp-contract=off -Wall
  -Wextra -Werror`; vendored fdlibm; doubles only), loads the M1 pipeline
  data it consumes (generated `ml_tables.c`/`ml_stages.c` + ANIM1 frame
  counts), then for EVERY golden in `oracle/goldens/manifest.json` (all
  8: g01–g06 human traces, g07–g08 CPU via the recorded AI-input bridge,
  fix_plan §M2 task 16) replays the trace end-to-end and emits a
  verify-stream-compatible run JSON, judged by the UNCHANGED
  `oracle/harness/verify-stream.js` against the frozen
  `oracle/goldens/*.sha256.json` — exact per-frame hash equality over the
  FULL 3600-frame length, plus rngCalls + rngCallsOutsideStep equality
  and the specVersion/trace pins. Prints `SIM CONFORMS`, exit 0; any
  mismatch, length shortfall, or missing golden → nonzero. (Script is
  built by fix_plan §M2 task 17; per-cluster `done-check:`s replay module
  captures and never substitute for this gate.)
- **Module-boundary capture + replay (M2 task 1 committed form; the
  M2-CAL rig generalized):** `node port/sim/calib/run-capture.js
  --spec <envcoll|util> --golden <id>` records a spec's module boundaries
  over a golden (canon v1.1: CHECKSUM.md structure, bit-pattern numbers,
  ALL NaNs collapsed to d:7ff8… — FORMAT.md; every run STREAM-MATCH
  guarded). Task check: `bash port/sim/calib/check-util-replay.sh` →
  `UTIL MATCH`, exit 0 (×2 byte-stable captures, pins via
  check-spec-pins.js incl. the rule-8 undef-ret accessor allowlist,
  strict 0-divergence C replay of every record). New cluster = new
  `spec-<name>.js` + `replay_<name>.c` + `expected-capture-<name>.json` +
  a `check-<name>-replay.sh` composed the same way. Gotcha classes (now
  fix_plan §M2 rules 8/9): accessor fns echo undefined verbatim (model
  undef-at-rest; per-function no-undef-ret pins); NaN PAYLOADS are
  unreproducible V8/C artifacts — canon collapses them, never chase
  payloads by reordering C arithmetic.
- **Player value model + mutation-capture rig upgrade (M2 task 2
  committed form):** `node port/sim/calib/run-capture.js --spec player
  --golden <id>` records per-frame post-update(i) player snapshots as
  5-field records (5th = POST-STATE canon of the projected player[i];
  FORMAT.md — projections: charAttributes/charHitboxes are M1-table
  data, percentShake is CHECKSUM.md §7's timing-dependent exclusion).
  Value model `port/sim/ml_player.h` (MlPlayer/MlPhysics/MlHitboxes/
  MlHitboxSpec; JsBool undef-at-rest; presence flags for 13
  runtime-added fields; type-specialized ml_player_copy +
  ml_hitboxes_merge_from — the sim's 3-arg deepObjectMerge ALIASES, see
  fix_plan §M2 rule 10); canon bridge `port/sim/calib/player_canon.{h,c}`
  is reusable by later clusters' pre/post player states. Task check:
  `bash port/sim/calib/check-player-model.sh` → `PLAYER MODEL MATCH`.
  Capture-FIRST instrument: `node port/sim/calib/survey-shapes.js
  <capture.jsonl>` — per-path type/key-set/length survey; run it on a
  fresh capture BEFORE finalizing any C value model. Gotcha class:
  round-trip model checks are SELF-REFERENTIAL for value edits (a
  corrupted capture nibble round-trips cleanly) — negative tests must
  perturb the C model/serializer (key order, presence flag, merge rule)
  or inject out-of-domain shapes (marshal hard-fail), never the data
  bytes.
- **Input cluster: interpretInputs + 8-deep buffer + meleeInputs (M2
  task 3 committed form):** `node port/sim/calib/run-capture.js --spec
  input --golden <id>` — wraps the input module + all 6 meleeInputs
  exports + physics args-projected to `[i, inputBuffers[i]]`
  (interpretInputs is main.js-internal to gameTick, NOT
  namespace-wrappable — the physics projection is its output boundary;
  same trick as task 2's post-state). C: `port/sim/ml_input.h` (plain
  bool/double — survey-measured, no undef-at-rest in this domain),
  `port/sim/input/{melee_inputs,input}.h` +
  `interpret_inputs.{c,h}` (MlInputSimState = the god-module's
  input-globals slice; ml_input_out_of_domain traps for
  lifecycle/AI/network arms), bridge `port/sim/calib/input_canon.{h,c}`.
  Replay is a full-trace CHAIN (C output feeds next frame's input —
  never the capture's bytes). Task check:
  `bash port/sim/calib/check-input-replay.sh` → `INPUT MATCH`. NEW rig
  facility: spec-defined deterministic `sweep()` (FORMAT.md; fix_plan
  §M2 rule 11) records synthetic-domain calls at frame 0 for boundaries
  with zero live records but a real future domain — sweep teeth proven
  via js_round perturbation (73 sweep-only divergences). ml_js.h gained
  `js_round` (ECMAScript Math.round: ties toward +Inf, -0 preserved —
  V8's Float64Round algorithm; naive floor(x+0.5) is WRONG at
  0.49999999999999994 and at every negative tie).
- **actionStateShortcuts + scaffolding + mulberry32 + sound-event seam
  (M2 task 4 committed form):** `node port/sim/calib/run-capture.js
  --spec asshort --golden <id>` — wraps all 29 exports of
  src/physics/actionStateShortcuts.js with per-function READ-SET arg
  projections + a {dsp,mut,rng,snd} post-state envelope (owner-stack
  event attribution; FORMAT.md "asshort"), records the seeded-RNG stream
  (frame-0 `rngBoot` [seed, 465 boot draws — browser == qjs boot pin] +
  every draw as a standalone `Math.random` record or a post `rng` entry).
  C: `port/sim/action_state_shortcuts.{c,h}` (attributes/intangibility
  from generated CTAB1 ml_tables — first M1-data consumer; actionStates
  registry scaffolding with opaque MlMoveDef until tasks 7-12),
  `port/sim/ml_rng.h` (mulberry32), `port/sim/ml_events.{c,h}`
  (sound-event queue seam the M4 mixer consumes + dispatch notes +
  logged ml_random). Replay chains ONE C mulberry32 draw-for-draw
  across the whole file (off-step startGame draw asserted == 1).
  Task check: `bash port/sim/calib/check-asshort-replay.sh` →
  `ASSHORT MATCH` (regenerates ml_tables via `node pipeline/run.js
  --only animations,tables --out pipeline/build/asshort-tables`).
  Gotcha classes: sweep purity is NET (rule 12 — swapped-RNG KO-shout
  sweep + restored player[3] injection, guarded by ×2 byte-stability +
  STREAM MATCH); threshold-nudge negative tests can be no-ops on
  keyboard-quantized traces — perturb constants/events/RNG instead.
- **physics.js core + interpolatedCollision (M2 task 5 committed form):**
  `node port/sim/calib/run-capture.js --spec physics --golden <id>` —
  mutation-captures `physics(i, inputBuffers)` with a full PRE-state args
  envelope (players/stage/globals/alias probes; stage captured per record
  so fountain's moving platforms are faithful) and a
  {alias,hq,players,snd} POST envelope; ORACLE-FED SEAMS for the
  not-yet-translated surfaces: every top-level move dispatch (tasks 7-12)
  records site+args+post-state and the replay verifies-then-RESYNCS; the
  5 hitDetection launch getters (task 6) record args+ret and the replay
  verifies args bit-exactly and injects the ret. interpolatedCollision's
  2 exports replay pure (live + a 35-call rule-11 sweep). C:
  `port/sim/physics.{c,h}` (MlSim god-module slice; rule-10 alias sites:
  prevFrameHitboxes merge + the land() pos===ECB1[0] alias, probe-driven;
  ecbSquashData chained as module state — the shared nullSquashDatum is
  provably never written) + `port/sim/interpolated_collision.{c,h}`.
  Task check: `bash port/sim/calib/check-physics-replay.sh` →
  `PHYSICS MATCH`. run-capture.js gained an optional spec `finalCheck()`
  post-run hook (the physics spec re-dumps the frame-0 asFlags
  actionStates flag table and hard-fails on in-match drift). Gotcha
  classes (now fix_plan §M2 rule 13): JS COMPOUND ASSIGNMENT groups its
  whole RHS (`a += b + c` is `a = a + (b + c)`) — left-flattening cost 22
  one-ulp live divergences; domain traps must sit at upstream's exact
  lazy dereference (hoisting a canGrabLedge[0] trap above its snap-box
  guard falsely aborts). Rule-8 extension: ECB1/ECBp COMPONENTS hold
  undefined at rest in frame-1 pre-states (undef masks in ml_player.h;
  `phys.passing` became presence-modeled for the same reason). Oracle-fed
  seams BOUND sensitivity: a corrupted pre field that a dispatch resync
  overwrites before any read is mechanically masked (measured) — corrupt
  POST fields for nibble teeth.
- **hitDetection + hitQueue + hitbox value model (M2 task 6 committed
  form):** `node port/sim/calib/run-capture.js --spec hitdet --golden
  <id>` — wraps all 29 hitDetection exports: hitDetect/executeHits/
  checkPhantoms mutation-captured with the uniform pre {alias,
  characterSelections, gameMode, gameSettings, hq, phq, playerType,
  players} / post {alias,hq,phq,players,rng,snd} envelopes (hq/phq = the
  FULL exported queues, marshalled per record — rows enter from THROW
  moves/physics windows, never chained); resetHitQueue/setPhantonQueue
  lean; launch getters + getKnockback/getHitstun/segmentSegmentCollision
  pure (getLaunchAngle's args projected with the LAZILY-read
  player[v].phys.grounded — null iff knockback >= 80); knockbackSounds
  {rng,snd}; 15 internal-only exports pinned ZERO records; move
  dispatches = oracle-fed seams (post {alias,hq,players}); frame-0
  hdFlags dump (canBeGrabbed/crouch/downed/specialClank/specialOnHit/
  vCancel, finalCheck drift-guarded). C: `port/sim/hit_detection.{c,h}`
  (task 5's 5 launch-getter seams are REAL bodies here; screenShake = 4
  logged ml_random draws per regular hit; ml_sound_stop "furaloop.stop";
  hitList push/splice write through the rule-10 alias; both
  MlHitboxSpec shapes' undefined-key reads via hb_* helpers). Task
  check: `bash port/sim/calib/check-hitdet-replay.sh` → `HITDET MATCH`.
  Gotcha classes: (rule 14) owner-draw vs dispatch-window-draw chain
  order is unrecoverable — record window draws as `Math.randomW`, pin
  the measured 0, replay hard-fails; node's ~512 MB readFileSync string
  cap (ERR_STRING_TOO_LONG — g06 hitdet capture is 542 MB):
  check-spec-pins.js STREAMS the JSONL; held object references across
  seam resyncs (executeHits' `hitbox = player[a].hitboxes.id[h]`) read
  the ORIGINAL object under reassignment — model with a by-value copy;
  razor-thin threshold nudges (hurtWidth +0.5, the 0.01 phantom band)
  are no-op teeth on occurring hit margins (rule-12 corollary, 2nd
  measured instance).
- **characters/shared moves (M2 task 7 committed form):** `node
  port/sim/calib/run-capture.js --spec moves-shared --golden <id>` —
  wraps every SHARED-ORIGIN actionStates entry's phase fns (1212 incl.
  the shared JUMPAERIALB/F modules; origin MEASURED by fn identity —
  deepCopyObject deep-copies data but copies functions by reference, and
  puff overrides FURAFURA/JUMPAERIALB/JUMPAERIALF) as mutation-captured
  "move" records ([phase,name,[slot,...extras],inputs(8-deep ×4),pre]
  args; {alias4,hq,players,rng,snd,vfx} post; hq carried OPAQUE — no
  shared move imports the hitQueue), non-shared entries as "mdispatch"
  seams (post {alias,hq,players,rng} — seam rng lists strengthen rule 14:
  window draws replay chain-exact at the seam point; measured 0), and a
  frame-0 mvData dump of the EXECUTED move-data plane (state→name,
  sharedOrigin registry, index.js setVelocities/posOffset patches,
  CAPTUREDAMAGE setPositions, actionSounds rows, palettes — fix_plan §M2
  rule 15). C: `port/sim/characters/shared/moves/*.c` (79 files) +
  `moves_index.c`/`moves.h` — MlMoveDef completed with real phase fn
  pointers (AsTri returns: several upstream interrupt arms FALL THROUGH
  and return undefined — control-flow fallthrough is part of the
  expression shape, rule 13 family); mv_dispatch resolves via the task-4
  as_lookup registry, unregistered states cross mv_seam; ml_events
  gained the vfx queue (drawVfx names; "circleDust" = 4 seeded draws —
  drawVfx.js:15); MlStageX gained respawnPoints/respawnFace. Sweep
  (rules 11/12, 44 calls): a REAL `new playerObject(0, [10,20], 1)` in
  restored slot 3 — NOTE pos is an ARRAY [x,y] (physicsObject reads
  pos[0]/pos[1]); Math.random swapped for a sweep mulberry32
  (0x0badf00d) that the replay mirrors for ALL frame-0 move records.
  Task check: `bash port/sim/calib/check-moves-shared-replay.sh` →
  `MOVES SHARED MATCH`. Gotcha classes: recording shared moves under
  TOP-LEVEL per-char windows is chain-safe (window draws are standalone
  records pushed at draw time) but recording them under an attributing
  record's seam window is NOT (would misorder the chain) — the wrapper's
  scope condition is inScope==0, not stack-empty; razor-thin threshold
  teeth again no-op on keyboard-quantized axes (DJ direction flip bit
  only where neutral double-jumps occur — rule-12 corollary, 3rd
  instance).
- **characters/fox moves (M2 task 8 committed form):** `node
  port/sim/calib/run-capture.js --spec moves-fox --golden <id>` — wraps
  every FOX-ORIGIN actionStates entry's phase fns (192, all on table 2 —
  fn identity vs the characters/fox/moves module index, rule 15's
  instrument extended) + the LASER/ILLUSION article inits (194 wrapped)
  over goldens **g01/g03/g08** (the fox carriers — PROVISIONAL deviation
  from g01/g04/g06, which field no fox; g08 = FIRST CPU-golden capture,
  AI JS-side, its seeded draws chain-verify as standalone records).
  Shared entries stay UNWRAPPED (top level: chain-safe silent surface;
  under a fox record: the transparent nested C tree — task 7's bodies
  linked in); non-fox per-char entries are mdispatch seams (THROWNFOX* on
  victims; zero live, teeth negative-test-proven); articles are 4-field
  FIFO seams (task-13 boundary — inits only READ player state, no RNG:
  args verified bit-exactly, no resync). C: `port/sim/characters/fox/
  moves/*.c` (61 files) + fox moves_index/moves.h — direct MODULE calls
  mirror upstream's import graph (fox→shared, fox→fox, MOVES[payload]);
  TABLE dispatch (mv_dispatch) only at upstream actionStates sites
  (THROW* victim arms; THROWBACK/THROWDOWN dispatch 1-arg, no input).
  hitQueue.push modeled as canon-row append on the opaque hq
  (mv_hq_push6); mv_checkForIASA = checkForIASA with REAL dispatch
  (shared JUMPAERIALB/F MODULE objects — puff's table overrides do NOT
  apply — plus a registered per-char module index); fox move-object data
  arrays (setVelocities*/offsets incl. authored expressions) come from
  the mvData fox dump (rule 15). Task check: `bash
  port/sim/calib/check-moves-fox-replay.sh` → `MOVES fox MATCH`. Gotcha
  classes: charHitboxes entries are ALWAYS 12-key createHitbox objects
  but their offset may be a SINGLE Vec2D (fox throw hitboxes) — the
  hitbox value model gained offsetSingle, and the old CONSTRUCTOR
  fallback for chars data was unreached-wrong (class-fixed in
  mv_assign_hitbox_id); CPU goldens widen VALUE domains (fix_plan §M2
  rule 16: ai.js writes NUMBER buttons into aiInputBank — buttons are
  truthiness-only upstream, marshal by JS truthiness); SIDESPECIALAIR.
  init's grounded arm infinitely recurses UPSTREAM (unsweepable by
  construction); CLIFF*'s onLedge===-1 arm WRITES the move table
  (`this.canGrabLedge = false`) — C traps at the site and the mvData
  finalCheck would catch the drift.
- **characters/falco moves (M2 task 9 committed form):** `node
  port/sim/calib/run-capture.js --spec moves-falco --golden <id>` — task
  8's fox recipe followed exactly over goldens **g02/g05/g07** (the falco
  carriers; g07 = second CPU golden): 214 falco-origin fns (69 module
  keys, table 3) + LASER/ILLUSION inits = 216 wrapped; falco article
  options carry isFox:false (+ partOfThrow:true on THROWDOWN lasers,
  ILLUSION always type 0). C: `port/sim/characters/falco/moves/*.c` (69
  files) + falco moves_index/moves.h; NO char-module registration
  (upstream checkForIASA has no char-3 branch — a falco IASA aerial
  payload dispatches nothing, verbatim). Falco structure deltas (measured
  per-file diffs, carried verbatim): THROW* inits unguarded
  (interrupt-only grabbing===-1 bare return), THROWN* without -1
  guards/offset clamps, CLIFF* without the canGrabLedge table-write arm
  (upstream throws — the mv_player/mv_falco_pair/mv_ledge_point traps),
  shine = 4-sub-state machine per environment with actionState-write
  land/platform-drop arms. Task check: `bash
  port/sim/calib/check-moves-falco-replay.sh` → `MOVES falco MATCH`.
  Gotcha class (rule 16 EXTENDED): ANY new golden widens the player-plane
  value domain for all four slots — g05's marth fired NEUTRALSPECIAL for
  the first time across all captures (runtime-added phys.shieldBreaker*
  trio + player.shieldBreakerID, presence-modeled in ml_player.h after a
  rule-7 marshal hard-fail); re-survey whenever a spec adopts a new
  golden.
- **characters/falcon moves (M2 task 10 committed form):** `node
  port/sim/calib/run-capture.js --spec moves-falcon --golden <id>` — the
  task-8 recipe over goldens **g03/g04/g06** (the falcon carriers —
  MEASURED: g07's CPU falcon fires ZERO live falcon-origin moves, so
  carrier membership is measured live coverage, never char presence):
  217 falcon-origin fns on table 4 (214 phase fns + falcon's NON-phase
  dispatch surfaces onPlayerHit ×2 / onWallCollide ×1 — hitDetection.js:
  493's specialOnHit and physics.js:122's specialWallCollide arms; the
  onWallCollide args canon is `[slot, wallFace, wallNum]`) + the 2
  article inits wrapped as a MEASUREMENT instrument only: falcon has NO
  article call sites (6 dead `articles` imports) — pinned zero, replay
  FIFOs any record as an unconsumed-seam tripwire. C:
  `port/sim/characters/falcon/moves/*.c` (67 files) + falcon
  moves_index/moves.h; special phases route through NEW
  `mv_register_special_phases` (shared moves.h: driver-registered
  (state,phase)→MvFn lookup — MlMoveDef keeps its 5-field shape, no
  initializer churn; unregistered = the upstream missing-property
  TypeError via mv_out_of_domain). The 20 THROWN* + GRAB/CATCHATTACK C
  files are the fox translations renamed (falcon-vs-fox diffs data-only;
  offsets from the mvData falcon dump, rule 15). Gotcha classes:
  SIDESPECIALGROUND writes `this.canEdgeCancel` at RUNTIME — a scalar
  move-table write invisible to the array-only mvData dump: modeled as C
  module state (write-only until task 17 wires physics' flag read);
  UPSPECIALCATCH/UPSPECIALTHROW draw the seeded stream INLINE in move
  code (2 draws per firefoxtail ×3 — chain state even though the values
  are render-only); dead-arm quirks from upstream typos
  (SIDESPECIALGROUNDHIT reads phys.timer, DOWNSPECIALGROUNDENDAIR reads
  player.timer — both undefined, comparisons always false) are carried
  as commented dead arms, never "fixed". Task check:
  `bash port/sim/calib/check-moves-falcon-replay.sh` →
  `MOVES falcon MATCH`.
- **characters/marth moves (M2 task 11 committed form):** `node
  port/sim/calib/run-capture.js --spec moves-marth --golden <id>` — the
  task-8 recipe over goldens **g01/g05/g06** (the marth carriers,
  probe-measured live coverage 1082/1417/1054; g04 fields no marth): 244
  marth-origin fns on table 0 (242 phase fns — 58×3 + 17 land — plus the
  NON-phase `onClank(p,input)` pair on DOWNSPECIAL{GROUND,AIR},
  hitDetection.js:71-72's specialClank arm, routed through the task-10
  `mv_register_special_phases` hook whose mv_dispatch arm now also
  accepts "onClank") + the 2 article inits as the zero-pin tripwire
  (marth has ZERO `articles` imports — measured). C:
  `port/sim/characters/marth/moves/*.c` (75 files) + marth
  moves_index/moves.h + `dancing_blade_{combo,air_mobility}.c`;
  `mv_register_char_module(0, marth_move_def)` makes checkForIASA's
  char-0 MARTHMOVES arm real (measured dead-by-construction upstream —
  marth aerials INLINE their aerial-IASA logic instead). NEW oracle-fed
  seam: `player.shieldBreakerID = sounds.shieldbreakercharge.play()`
  consumes a GLOBAL howler-counter id (unrecoverable chain state) — the
  capture records it in the post "sbid" list, the replay injects it via
  `mv_howl_play_id` and re-emits the consumed list; `.stop(id)` is the
  "shieldbreakercharge.stop" token. Gotcha classes: NEUTRALSPECIAL*'s
  timer-46 `hitboxes.id[i].dmg` write MUTATES the GLOBAL charHitboxes
  plane (player.js:132 aliases chars data — the falcon canEdgeCancel
  class, VALUE-plane sub-shape; benign while charge < 30, the measured
  live domain; the sweep's >=120 arm restores the 8 dmg fields, rule 12);
  a "delta-2" per-file diff can be two SEMANTIC lines, not imports —
  THROWNFALCOBACK's 2-line delta vs fox was the offset DATA plus an ADDED
  `face *= -1` (caught by the probe replay: always read the diff BODY,
  never trust the line count); THROWN guard/clamp ORDER varies per file
  (clamp-first vs guard-first — verbatim per file). Task check:
  `bash port/sim/calib/check-moves-marth-replay.sh` →
  `MOVES marth MATCH`.
- **characters/puff moves (M2 task 12 committed form; the LAST per-char
  cluster — tasks 7-12 now cover all five chars + shared):** `node
  port/sim/calib/run-capture.js --spec moves-puff --golden <id>` — the
  task-8 recipe over goldens **g02/g04/g08** (the puff carriers,
  probe-measured live coverage 843/1215/1037; g08's CPU puff attacks,
  unlike g07's falcon — and fires the FIRST LIVE mdispatch seam of any
  per-char cluster: its THROWBACK dispatches fox's THROWNPUFFBACK).
  221 puff-origin fns on table 1 (217 phase fns + onPlayerHit ×3 on
  NEUTRALSPECIAL{GROUND,AIR,GROUNDTURN} + onWallCollide on
  NEUTRALSPECIALAIR — the task-10 hook) + the 2 article inits pinned
  ZERO (puff has no `articles` references, marth-strength). Puff
  OVERRIDES shared FURAFURA/JUMPAERIALB/JUMPAERIALF on ITS table only
  (rule 15's fn-identity origin map, asserted both directions); puff's
  FURAFURA is trivial (WAIT.init — NO furaloop: the task-11 sbid seam
  measured unnecessary). NEW (fix_plan §M2 rule 17): every move
  record's pre carries **"chd"** — the EXECUTED charHitboxes
  {moveKey:{idN:{dmg,size}}} VALUE plane — because puff WRITES the
  M1-owned plane through STALE id aliases (rollout's post-release dmg
  even uncharged; sing's id[0].size cycles): measured LIVE drift on
  g04 (jab1 dmg 3→7, frame 1038). C `pf_assign_hitbox_id` feeds
  dmg/size from chd, never assumed-pristine CTAB1 (dmg/size = the only
  createHitbox fields with upstream write sites, grep-measured). C:
  `port/sim/characters/puff/moves/*.c` (71 files) + puff moves_index/
  moves.h + `puff_{multi_jump_drift,next_jump}.c` (helper modules;
  puffNextJump's COMPUTED "AERIALTURN/JUMPAERIAL"+(1+jumpsUsed) keys —
  rung 6 unreachable, trapped); rule-8 read/write helpers for the
  runtime-added rollOut* plane (rollOutTurnTimer widened this task —
  rule 16). Gotcha classes: NSG/NSA mains do NOT advance timer at the
  top (mid-body charge-scaled advance — sweep presets must hit EXACT
  arm timers) and their interrupts ALWAYS return false (the WAIT/
  FALLSPECIAL arms fire inits and still return false); THROWBACK's
  window carries a floor-over-COMPARISON typo (`Math.floor(t+0.01<37)`
  — floor of a boolean, truthiness preserved verbatim); upstream typo
  fields hitboxes.FRAMES++ / lowercase phys.autocancel carried
  verbatim; per-file THROWN* snap/flip/negX/clamp-order variation
  (read every body — the task-11 lesson held). Task check:
  `bash port/sim/calib/check-moves-puff-replay.sh` →
  `MOVES puff MATCH`.
- **articles (M2 task 13 committed form):** `node
  port/sim/calib/run-capture.js --spec article --golden <id>` — wraps
  the article module (src/physics/article.js): the 4 gameTick pipeline
  calls mutation-captured over the three article queues (aArticles ==
  CHECKSUM.md §2's checksummed `articles` key) with LEAN-WHEN-EMPTY
  envelopes, articles.{LASER,ILLUSION}.init as first-class "ainit"
  mutation records (the task-8/9 article-seam crossings), resetAArticles
  (endGame-only) + the 6 internal-only collision helpers pinned ZERO,
  per-char entries as mdispatch seam loggers (measured zero). Carriers
  g01/g02/g08 — probe-MEASURED over all six fox/falco goldens (g03/g05
  field zero live articles; g07's lasers never connect; zero live
  ILLUSIONs anywhere — sweep-only, 72 rule-11/12 calls). C:
  `port/sim/article.{c,h}` (MlArticle value model; REAL bodies for the
  task-8/9 init seams — task 17 swaps mv_article_* for direct calls,
  FORMAT.md "The article spec"; executeArticleHits runs task-7 shared
  bodies via mv_dispatch + task-6 hit_detection getters + screenShake's
  4 draws). NEW rig instrument (fix_plan §M2 rule 18): module state
  fully enclosed by captured boundaries is CHAINED in C and
  chain-verified against every in-match record's pre (a wrong queue
  mutation flags at the NEXT record even when its own record replays
  clean); lean-when-empty is the read-set projection gated on the
  driving queue's emptiness (~20x capture-size cut, zero teeth loss).
  Gotcha classes: fox lasers are ZERO-knockback (kg=bk=sk=0 — live fox
  hits are percent-only; kb>0 arms live only on falco carriers), and
  upstream quirks carried verbatim: ILLUSION `isFox || true`
  always-true, duplicate-destroy `splice(queue[k]-k)` going NEGATIVE
  (JS removes from the END), wall-death truthiness (sweep 0 falsy),
  hit.hitPoint aliasing instance.pos (unobservable — measured by
  reading). Task check: `bash port/sim/calib/check-article-replay.sh` →
  `ARTICLE MATCH`.
- **movingPlatforms stage-tick logic (M2 task 14 committed form):** `node
  port/sim/calib/run-capture.js --spec platforms --golden <id>` — wraps
  all six VS stage objects' movingPlatforms (one record per tick;
  main.js:1058 — the FIRST call of the mode-3 tick) with per-stage
  read/write-set envelopes: plat = the FULL platform plane in every
  record (rule 18's chain carrier — statics included, so "nothing else
  writes the plane" is measured per record, not assumed), ystory adds the
  4-slot player slice {grounded,onSurface,pos} (rider loop UNGUARDED by
  playerType upstream — inactive slots are page-start CSS-era
  playerObjects), fountain adds platformStates + starting + owner rng.
  fountain's platformStates is module-PRIVATE (a closure `let`) — exposed
  by a SECOND run-capture.js served-bytes injection (the __wpCache
  mechanism class): a quote-free window getter after the declaration,
  unique-match hard-fail, disk untouched. C: `port/sim/stages/
  {moving_platforms,ystory,fountain}.c` (MpSim slice struct; the rail/
  platform constants are upstream CODE literals, NOT STAB1 data; state
  string "moving"/"static" ↔ isStatic). Carriers g01/g02/g06 by STAGE
  IDENTITY (only ystory/fountain have non-empty bodies — the other four
  are EMPTY upstream; g01 = the static-stage class representative).
  Gotcha classes: ystory's rail arms are sequential NON-exclusive ifs —
  corner frames run TWO arms in one call and `move` keeps the LAST arm's
  value (arm order makes a bottom-right double impossible); fountain's
  selection draw is CONSUMED even when the base-return arm ignores it;
  the sweep restores fountain via the starting arm itself (the reset IS
  the restore — poke-free net restoration). Task check:
  `bash port/sim/calib/check-platforms-replay.sh` → `PLATFORMS MATCH`.
- **ECMAScript float formatter + CHECKSUM.md ser (M2 task 15 committed
  form):** `bash port/sim/calib/check-format.sh` → `FORMAT MATCH`, exit 0
  — proves `port/sim/ml_fmt.c` (`String(x)`: vendored Ryu core at
  port/ryu/, ulfjack/ryu @ 4c0618b0 byte-verbatim, provenance
  sha256-pinned + NOTICES; our ECMA-262 §6.1.6.1.20 steps 6-10 layer on
  Ryu's digits+exponent) and `port/sim/ml_ser.c` (§3 ser primitives:
  explicit `-0` token, T/F, undef/null/fn/cyc, JSON.stringify escaping;
  §4 SHA-256 lowercase hex via oracle/qjs/sha256.c) byte-identical to
  the JS oracle DIFFERENTIALLY: 5.47M-pattern pinned adversarial corpus
  + every unique double in ALL build/*.jsonl captures (~249k; records
  g01 player+article if absent) through C vs V8-String(x)/oracle-numStr,
  plus ~40k composite cases (every g01 player/article record tree + 3636
  full §2/§3.1 fixed-literal-order frame envelopes) C parse→ser→hash vs
  the oracle's OWN pagelib.js ser/__serializeState/__sha256 run under
  `window === global` (references extracted from pagelib source bytes —
  zero transcription). Pins: `expected-format.json`. Task 17 consumes
  ml_fmt/ml_ser for stream emission — ml_sb_num is the only legal number
  emitter. Gotcha classes: JS-reference extraction (eval the oracle's
  own bytes, never transcribe it — transcription bugs mirror on both
  sides of a differential); zsh `time cmd | tee` pipestatus lies about
  mid-pipe failures — verify gate exit codes by direct invocation.
  `bash oracle/build-upstream.sh` — clones/checks out the pin, applies
  `oracle/meleelight-harness.patch`, prunes dead devDeps, builds via
  docker node:8 into `${MELEELIGHT_CLONE:-$HOME/.cache/meleelight-funkey-s/upstream}`;
  idempotent, `--force` rebuilds from the pristine pin.
- **AI-input bridge for CPU goldens (M2 task 16 committed form):** `node
  port/sim/calib/run-capture.js --spec ai --golden <g07|g08>` — wraps
  runAI (mutation record: args [i], post {bank, bk, rng} — the post-runAI
  aiInputBank[i][0] row, the AI-private player bookkeeping, the seeded
  draws consumed in-window) + the spec-input chain pair (pollInputs,
  physics args-projected [i, inputBuffers[i]]) over the CPU goldens, with
  a per-call in-page WRITE-SET RECON (pre/post canon of 4 players minus
  the {currentAction,currentSubaction,curentAction,lastMash} allowlist +
  all 32 bank rows + playerType/cS/gameSettings/activeStage; any other AI
  write → wsViol record, pinned ZERO + finalCheck throw).
  `node port/sim/calib/build-ai-bridge.js <id> <capture> <out>` distills
  the replayable AIBRIDGE1 artifact (format: port/sim/ai_bridge.h; one
  line per runAI: frame, slot, draw bit-patterns, 22 B0/B1/U/N<hex16>
  field tokens in canon key order). C bridge `port/sim/ai_bridge.{h,c}`:
  ml_ai_bridge_apply burns the recorded draws bit-verified on the CHAINED
  mulberry32, verifies the never-AI-written fields (dd,dl,dr,du,r,rA,
  raw*,s) against the chain, installs the row; task 17 calls it at the
  update(i) runAI site. Task check: `bash port/sim/calib/
  check-ai-bridge.sh` → `AI BRIDGE OK`. Gotcha classes: (1) rule-16 CLASS
  FIX — interpretInputs is now implemented ONCE over the tagged JS-value
  input model (port/sim/input/ai_input.h, MlAiVal bool|number|undefined;
  AI helper literals assign undefined via missing-key reads);
  ml_interpret_inputs is a conversion wrapper (task-3 check re-verified).
  (2) upstream pollInputs returns the bank ROW ALIAS for CPU slots and
  runAI runs BETWEEN interpretInputs and physics — slot 0 must be
  re-copied from the bank post-runAI (a skipped write-through = 8-frame
  history-propagating divergences). (3) the harness pins CPU-slot mType
  to "keyboard": the KEYBOARD arm (incl. the pause machine reading the
  bank's s by truthiness) is live on CPU slots; the raw*/deaden AI arm
  never runs in the captured domain. (4) recon canon of FULL players ×2
  per call costs ~nothing (26s captures) — prefer airtight over sampled.
- **Integrated headless sim (M2 task 17 committed form; the M2 EXIT GATE
  is live):** `bash port/sim/check-sim.sh` → `SIM CONFORMS`, exit 0 — the
  §Gates M2 command, now runnable: regenerates CTAB1/STAB1
  (pipeline/build/sim-tables), dumps the SIMDATA1 executed move-data
  plane ×2 byte-identical (`node port/sim/calib/dump-sim-data.js` —
  boot-time asFlags/hdFlags/mvData-union/palettes0, each section
  byte-equal to the frozen captures' frame-0 records), builds
  `port/sim/calib/build/sim_host` from `port/sim/sim/` + every module
  cluster (all TUs `cc -O2 -ffp-contract=off -Wall -Wextra -Werror`),
  then per golden: trace → text (`port/sim/sim/trace-to-txt.js`, IEEE
  bit-pattern tokens), replay, wrap (`port/sim/sim/wrap-run.js`), judge
  with the UNCHANGED verify-stream.js. Manual single-golden run + the
  divergence instrument: `sim_host --trace t.txt --simdata s.txt --seed
  N --p1 N --p2 N --stage N --frames N [--cpu --difficulty N --ai-bridge
  f] [--dump-frames a,b]` — `--dump-frames` prints frame envelopes to
  stderr; byte-diff them against `oracle/harness/run.js
  --capture-frames` output to localize a divergence (M2CAL procedure).
  Gotcha classes: (1) later-cluster value-model widenings must be
  back-propagated to earlier clusters' ZERO-LIVE arms (task 8's
  offsetSingle vs task 6's throw arm — the iter-36 ledger's one sim
  divergence; grep earlier consumers' shape conditionals whenever a
  value model widens); (2) integration seams that must stay LIVE state,
  not data: falcon SSG canEdgeCancel (mlp_flags runtime overlay), the
  rule-17 charHitboxes plane (WEAK-default hooks in shared
  moves_index.c; strong overrides in port/sim/sim/sim_data.c), the
  aiInputBank row alias (buffer slot 0 re-copied post-runAI); (3) draw
  COUNTS are recoverable from mulberry32's state delta (odd additive
  constant ⇒ modular inverse) — no wrapper needed on the hot path;
  (4) bash 3.2 `set -u` rejects empty-array `"${a[@]}"` (use
  `${a[@]+"${a[@]}"}`) and `node -e console.log(<number>)` can emit ANSI
  colour — String() it in scripts.
- **Upstream clone + build (proven twice — determinism spike + prototype):**
  ```
  git clone https://github.com/schmooblidon/meleelight "$MELEELIGHT_CLONE"
  cd "$MELEELIGHT_CLONE" && git checkout 27af171
  # apply the project patch (harness or mapping), drop dead devDeps
  # (deepstream.io, electron*) + the postinstall script, then:
  docker run --rm --platform linux/amd64 -v "$PWD":/app -w /app node:8 \
    bash -c "npm install --ignore-scripts && npm run animations && npm run build"
  ```
  (Full recipe + patch: `spikes/determinism/README.md`,
  `prototypes/control-mapping/README.md`.)
- **Oracle harness (production, M0 task 2 — maintained copy; spike stays
  frozen):** `cd oracle/harness && npm install` (committed package.json pins
  playwright 1.61.1; launches installed Chrome via `channel:"chrome"`, no
  browser download), then
  `node run.js --dist "${MELEELIGHT_CLONE:-$HOME/.cache/meleelight-funkey-s/upstream}" --trace ../goldens/g01-fox-marth-battlefield.trace.json --frames 3600 --seed 1337 --out out/a.json`
  ×2 + `node compare.js out/a.json out/b.json` → `IDENTICAL checksum
  streams`. Match params: `--p1/--p2` char (0 marth · 1 puff · 2 fox ·
  3 falco · 4 falcon), `--stage` (0 battlefield · 1 ystory · 2 pstadium ·
  3 dreamland · 4 fdest · 5 fountain), `--cpu [--difficulty 1-9]` (P2 AI;
  trace P2 column ignored). Defaults p1=2 p2=0 stage=0 = golden #1.
  `gen-trace.js /tmp/t.json 3800 1337` regenerates g01's trace
  byte-identically (sha256 60c332c5…20b9).
- **Oracle harness (spike-era original, frozen):** `spikes/determinism/harness/` —
  `node run.js --dist "$MELEELIGHT_CLONE" --frames 3600 --seed 1337 [--cpu] --out out/a.json`
  twice, then `node compare.js out/a.json out/b.json` → `IDENTICAL`.
  Needs `npm i playwright` next to the harness (uses installed Chrome).
- **arm32 cross-compile (FunKey SDK 2.3.0 via docker `jondbell/funkey-s-sdk`):**
  ```
  docker run --rm -v "$PWD":/work -w /work jondbell/funkey-s-sdk bash -lc \
    'export PATH=/opt/FunKey-sdk-2.3.0/bin:$PATH; arm-funkey-linux-musleabihf-gcc \
     -O2 -ffp-contract=off <srcs> -o build-arm/<out> \
     $(/opt/FunKey-sdk-2.3.0/arm-funkey-linux-musleabihf/sysroot/usr/bin/sdl-config --cflags --libs) -lm'
  ```
  gcc 10.2, musl hard-float, targets Cortex-A7+NEON by default. `sdl-config`
  is NOT on PATH (full path above). Run docker builds SERIALLY. Image is
  amd64 — fine under emulation. `-ffp-contract=off` on every sim TU is a
  hard rule; `-O3` only on the hot raster TU.
- **fdlibm crosscheck (M0 task 3):** `bash oracle/fdlibm-crosscheck/run.sh`
  → `CROSSCHECK OK`, exit 0 — (1) every constant in
  `port/fdlibm/{fdlibm.c,fdlibm.js}` matches its commented bit pattern,
  (2) ~257k-input deterministic sweep C↔JS bit-exact (`cmp`), (3) ≤16-ulp
  sanity vs native Math (guard only, never the gate), (4) golden #1's
  full Math call stream three-way byte-identical (browser/C/JS). Vendored
  surface sin/cos/tan/atan/atan2/pow at `port/fdlibm/` (V8 12.4.254
  `src/base/ieee754.cc` lineage; fdlibm_sin/fdlibm_cos bodies; NOTICES
  entry + Sun/V8 headers carried). The harness shims `Math.*` with fdlibm
  BY DEFAULT (`--native-libm` disables, drift experiments only;
  `--capture-math` records the call stream into output JSON's
  `mathCapture`). Shimmed g01 stream first diverges from the pre-shim
  stream at frame 1671 — browser-libm drift is real; run-to-run identity
  holds (streams freeze in task 5).
- **Checksum spec (M0 task 4, FROZEN):** `oracle/CHECKSUM.md` spec v1 —
  the normative field-list/serialization/SHA-256/frame-boundary/RNG
  contract the C port implements in M2. Any normative change = version
  bump + re-freeze ALL `oracle/goldens/*.sha256.json` in the same change
  (spec §8). Envelope keys are fixed-literal order (NOT sorted — only
  nested objects sort); the seeded stream burns exactly one off-step draw
  at `startGame` (`rngCallsOutsideStep == 1` is the expected value).
- **Golden record + freeze / stream verify (M0 task 5):**
  `bash oracle/record.sh <golden-id> [--refreeze]` — params come ONLY from
  `oracle/goldens/manifest.json` (single param source; g01 seeded, task 7
  extends it): two fresh browser runs (fdlibm shim on by default) →
  `compare.js` identity → `oracle/harness/freeze-stream.js` writes
  `oracle/goldens/<name>.sha256.json` (deterministic — NO timestamps or
  environment data; re-recording an already-frozen golden must print
  `unchanged (byte-identical re-freeze)`, any diff = drift; a DIFFERING
  overwrite needs `--refreeze` and is only legit with a spec version bump,
  CHECKSUM.md §8) → `verify-stream.js` self-check. Verify any run:
  `cd oracle/harness && node verify-stream.js <run.json> ../goldens/<name>.sha256.json`
  → `STREAM MATCH …`, exit 0 — exact string equality per frame, FULL
  length, plus rngCalls + rngCallsOutsideStep equality, specVersion pin
  (parsed live from oracle/CHECKSUM.md — a spec bump without re-freeze
  fails mechanically), trace-sha256/param pins, and the frozen file's
  `streamSha256` integrity seal. g01 frozen: spec v1, 3600 frames,
  rngCalls=134, rngCallsOutsideStep=1, frame-1 hash = the CHECKSUM.md §5
  anchor (9f4c6df7…).
- **QuickJS oracle runtime + golden replay (M0 task 6, committed form):**
  `bash oracle/qjs/build.sh && bash oracle/qjs/replay.sh <golden-id>` →
  `QJS MATCH <id>`, exit 0. build.sh pins bellard/quickjs @
  `42d08be5f28abfdf881110bba3713f6a256d8d97` (VERSION 2026-06-04 —
  byte-matches the feasibility-spike tree; all 18 consumed sources
  sha256-verified), builds `build-host/qjs-oracle` (embedder repoints the
  Math table at `port/fdlibm/` C at startup; C SHA-256, NIST-self-tested;
  `-ffp-contract=off` on every TU) + a static armv7 cross-build
  COMPILE-ONLY (`QJS_SKIP_ARM=1` skips it for fast host iteration; device
  rung is M3). replay.sh boots the built bundle under qjs via
  `oracle/qjs/shim.js` (host-object shims only, each documented) with the
  browser harness's `init.js`/`pagelib.js` VERBATIM, feeds the golden's
  manifest params/trace through `__harness`, and judges with the
  unchanged `verify-stream.js`. Boot guards fail hard: bitwise
  fdlibm-repoint assertion (negative-testable: `QJS_ORACLE_NO_REPOINT=1`)
  and a boot-RNG draw pin (== 465; mulberry32 state is never re-seeded at
  setupMatch, so boot draw count misalignment silently shifts the
  in-match stream). Gotcha class (cost a divergence at frame 403):
  browser FEATURE DETECTION makes missing globals silent path-flips, not
  crashes — absent `Storage`, getCookie returns `""` (not null) and
  getGameplayCookies Number("")-zeroes every gameSettings entry
  (phantomThreshold 0.01→0). Shim for parity of paths, not just survival.
- **Golden set (M0 task 7, committed form):** 8 goldens in
  `oracle/goldens/manifest.json` — g01 fox/marth/battlefield seed 1337 ·
  g02 falco/puff/ystory 7302 · g03 falcon/fox/pstadium 7303 ·
  g04 puff/falcon/dreamland 7344 · g05 marth/falco/fdest 7305 ·
  g06 falcon/marth/fountain 7306 · g07 falco/CPU-falcon(d5)/battlefield
  7307 · g08 fox/CPU-puff(d5)/fdest 7308. All 3600 verified frames;
  manifest `seed` doubles as the gen-trace.js seed
  (`node oracle/harness/gen-trace.js <trace> 3800 <seed>` reproduces every
  trace byte-identically). Trace GAMEPLAY QUALITY CONTRACT (checked at
  record time; documented in the manifest comment): ≥1 KO (DEAD* state),
  ≥1 DAMAGE*/CAPTUREDAMAGE state (real hits, not just SDs), both players
  ≥1 stock at the final frame (match still live — a 0-stock endpoint means
  post-match frames polluted the stream; seed 7314 rejected for exactly
  that). Full gate `bash oracle/verify_goldens.sh` (16 browser runs + 8
  qjs replays) ≈ 4 min. CPU rngCalls vary wildly by matchup (81 g07 vs
  1496 g08) — both deterministic.
- **QuickJS oracle-runtime build (spike-era original, frozen):**
  `spikes/device-feasibility/README.md` step 2 (bellard/quickjs
  @2026-06-04, single gcc invocation via `qjsmin.c`, static, 890 KB).
- **Device access (ADB):** marker file `adb` at SD root → adbd on boot,
  root shell. Park frontend: `touch /mnt/disable_frontend; pkill gmenu2x`
  (restore: `rm /mnt/disable_frontend`). Launch via login shell **`sh -lc`**
  (else SDL_Init dies: "Unable to open mouse"); detached runs need
  `setsid … </dev/null` + trailing `sleep 2`. Buttons arrive as letter
  keysyms: `u/d/l/r` d-pad, `a/b/x/y` face, `s` START, `k`/`n` L/R, `q`
  MENU. Known-good device id: `12c00003237f5528`.
- **OPK packaging:** `mksquashfs $STAGE out.opk -all-root -noappend
  -no-exports -no-xattrs -comp gzip` — **use the SDK container's
  mksquashfs 4.4 ONLY** (newer versions produce an OPK the kernel silently
  fails to mount). `.desktop` file needs a trailing empty line; `Exec=` a
  launcher script, not the binary. OPK mounts read-only: write to `/tmp`
  (RAM, wiped at power-off) or `/mnt` (SD).

## Build/gotcha notes (the loop appends here)

- Log to tmpfs during play, copy to SD on exit (SD streaming = multi-second
  stalls); drive telemetry from the host over ADB (on-device background
  scripts starve at 100% game CPU). Read MemAvailable, not MemFree.
- Screenshot from INSIDE the app (dump own framebuffer) — the kernel fb is
  240×720 (3 flip pages), raw fb reads hit the wrong page. Fn+Up = OS
  screenshot chord → `/mnt/FunKey/snapshots/`.
- SDL_SetVideoMode fallback chain HWSURFACE|DOUBLEBUF → SWSURFACE|DOUBLEBUF
  → SWSURFACE → 0; verify `BitsPerPixel == 16` after init or bail.
- Upstream expected console noise: 404 for `/favicon.ico`; webpack
  localforage warning. Sim frames before ~frame 91 (match `starting`
  window) ignore inputs.
