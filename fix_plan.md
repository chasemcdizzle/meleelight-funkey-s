# fix_plan — the loop's priority queue

Current phase: M0

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

1. Commit the pinned upstream clone+patch+build recipe as
   `oracle/build-upstream.sh`: clone schmooblidon/meleelight →
   `git checkout 27af171` → `git apply oracle/meleelight-harness.patch`
   (maintained copy of the spike patch) → prune dead 2018 devDeps
   (`deepstream.io`, `electron*`) + the `postinstall` script from
   package.json → docker `--platform linux/amd64 node:8`
   `npm install --ignore-scripts && npm run animations && npm run build`.
   Idempotent (verifies + skips when already built; `--force` rebuilds
   from pristine pin) —
   done-check: `bash oracle/build-upstream.sh && test -f "${MELEELIGHT_CLONE:-$HOME/.cache/meleelight-funkey-s/upstream}/dist/meleelight.html" && grep -rq __harnessInputs "${MELEELIGHT_CLONE:-$HOME/.cache/meleelight-funkey-s/upstream}/dist/js/"` → exit 0.
2. Productionize the spike harness into `oracle/harness/` (adapt
   run.js/compare.js/init.js/pagelib.js/gen-trace.js from
   `spikes/determinism/harness/`; add a committed package.json pinning
   playwright; parameterize match setup — `--p1/--p2 <char 0-4>`,
   `--stage <0-5>`, `--cpu [--difficulty N]`, `--trace <file>`) and copy
   the 3800-frame spike trace to
   `oracle/goldens/g01-fox-marth-battlefield.trace.json` —
   done-check: `cd oracle/harness && npm install >/dev/null && node run.js --dist "${MELEELIGHT_CLONE:-$HOME/.cache/meleelight-funkey-s/upstream}" --trace ../goldens/g01-fox-marth-battlefield.trace.json --frames 3600 --seed 1337 --out out/g01-a.json && node run.js --dist "${MELEELIGHT_CLONE:-$HOME/.cache/meleelight-funkey-s/upstream}" --trace ../goldens/g01-fox-marth-battlefield.trace.json --frames 3600 --seed 1337 --out out/g01-b.json && node compare.js out/g01-a.json out/g01-b.json` → prints `IDENTICAL checksum streams`, exit 0.
3. Land vendored fdlibm BOTH sides: `port/fdlibm/` C sources (surface:
   sin, cos, tan, atan, atan2, pow — V8's fdlibm-derived ieee754;
   `NOTICES` entry lands FIRST; vendored sources don't count against the
   diff budget), JS ports of the same functions shimming `Math.*` inside
   `oracle/harness/init.js` (pins the oracle against browser libm drift),
   and `oracle/fdlibm-crosscheck/` — C harness + JS harness emitting
   IEEE-754 bit patterns over (a) a deterministic edge-case + seeded sweep
   per function and (b) golden #1's recorded per-call argument stream;
   byte-compare — done-check:
   `bash oracle/fdlibm-crosscheck/run.sh` → prints `CROSSCHECK OK`, exit 0.
4. Freeze the checksum spec as `oracle/CHECKSUM.md`: the spike-proven
   field list (per active player: actionState, timer, percent, stocks,
   hit, hitboxes, phys; plus the aArticles queue), serialization rules
   (sorted keys, `String(x)` shortest-round-trip floats, explicit `-0`,
   `NaN`/`Infinity` literal, booleans `T`/`F`, typed arrays as arrays,
   cycle-safe `cyc`, functions `fn`), SHA-256 per frame, and the explicit
   percentShake exclusion + rationale — done-check:
   `bash -c 'for t in actionState timer percent stocks hitboxes phys articles percentShake SHA-256 mulberry32 "\-0"; do grep -q -- "$t" oracle/CHECKSUM.md || { echo "missing: $t"; exit 1; }; done; echo SPEC OK'` → prints `SPEC OK`, exit 0.
5. Record + freeze golden #1's checksum stream with the fdlibm JS shim
   active: `oracle/record.sh g01` (two fresh browser runs must agree
   before writing `oracle/goldens/g01-fox-marth-battlefield.sha256.json`)
   plus `oracle/harness/verify-stream.js <run.json> <frozen.sha256.json>`
   (exact equality, full length) — done-check:
   `cd oracle/harness && node run.js --dist "${MELEELIGHT_CLONE:-$HOME/.cache/meleelight-funkey-s/upstream}" --trace ../goldens/g01-fox-marth-battlefield.trace.json --frames 3600 --seed 1337 --out out/g01-fresh.json && node verify-stream.js out/g01-fresh.json ../goldens/g01-fox-marth-battlefield.sha256.json` → prints `STREAM MATCH`, exit 0.
6. Build the fdlibm-patched QuickJS oracle runtime + replay rig:
   `oracle/qjs/build.sh` (bellard/quickjs pinned at the exact commit the
   feasibility spike used — record the sha in the script; host `cc` build
   of a qjsmin-style embedder linking `port/fdlibm/` and repointing the
   `Math` table at it at startup; arm cross-build compile-only, no device)
   and `oracle/qjs/replay.sh <golden-id>` (minimal DOM/browser shim
   sufficient to boot the built bundle headless under qjs, feed the trace
   through the same `__harness` seam, emit the per-frame SHA-256 stream,
   compare vs the frozen stream) — done-check:
   `bash oracle/qjs/build.sh && bash oracle/qjs/replay.sh g01` → prints
   `QJS MATCH g01`, exit 0.
7. Record the full golden set per the composition above (g02–g08:
   generate traces with `oracle/harness/gen-trace.js` variants/seeds,
   record + freeze streams via `oracle/record.sh`, write
   `oracle/goldens/manifest.json`) and commit `oracle/verify_goldens.sh`
   (final form = the M0 exit gate; see CLAUDE.md §Commands: per golden
   two fresh browser runs bit-identical to each other AND the frozen
   stream, QuickJS replay reproduces it, manifest coverage = all 5 chars
   + all 6 stages + ≥1 CPU trace) — done-check:
   `bash oracle/verify_goldens.sh` → prints `ALL GOLDENS OK`, exit 0.

## M1 — Data pipeline

(seed items — REPLAN concretizes on phase entry)
1. Animation serializer: executed-JS → packed int16 bezier-path binaries +
   manifest — done-check: …
2. Framedata/attributes/ECB/hitbox/stage-geometry → generated C tables —
   done-check: …
3. Audio conversion: SFX → 22050 mono S16 blobs; music → 22050 stereo S16
   raw PCM — done-check: …
4. Byte-stability + coverage verification (754 files / ~27.9k paths / 5
   chars / 6 stages / ~180 SFX + 8 tracks) — done-check: …

## M2-CAL — Calibration slice

(seed items — REPLAN concretizes on phase entry)
1. Slice harness: record environmentalCollision.js per-frame inputs/outputs
   over golden trace #1 — done-check: …
2. Translate environmentalCollision.js → C (structure-parallel) —
   done-check: …
3. Burn-down loop to bit-identical; record div/KLOC, fix-rate, projection;
   go/no-go per PLAN §4 — done-check: …

## M2 — Sim core checksum-locked, headless

(seed items — REPLAN decomposes the ~15–20k LOC grind on phase entry,
module-by-module in dependency order, each module slice-verified like
M2-CAL before integration)

## M3 — On-device at 60 fps

(seed items — REPLAN concretizes on phase entry: platform seam SDL1.2 TU,
S1 input table wiring, OPK packaging, ADB conformance + perf rig, Chase
ratification playtest escalation)

## M4 — Full-game parity

(seed items — REPLAN concretizes on phase entry: thin C front-of-house
menus, audio mixer per PLAN §7, ai.js port (+tan to fdlibm surface),
target test, SD persistence, full-game trace suite, Chase acceptance)
