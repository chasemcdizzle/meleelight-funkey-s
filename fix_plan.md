# fix_plan — the loop's priority queue

Current phase: M1

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

2. Extractor bundle + engine tables → generated C: build a
   `pipeline/extractor/` webpack entry with upstream's docker node:8
   toolchain (modeled on `bin/webpack/animations.config.js`; entry
   assigns `window.__tables` from the real modules), execute it, and emit
   character attributes / framesData / intangibility / ECB / hitbox
   constants as generated C tables — doubles serialized as IEEE-754
   uint64 bit patterns with a human-readable comment (port/fdlibm
   constant convention), ints as ints — done-check:
   `bash pipeline/check-tables.sh` → prints `TABLES OK`, exit 0 (two
   fresh runs byte-identical; every emitted value bit-equal to a fresh
   executed-JS walk of the extractor output; counts vs expected.json:
   all 5 chars have attributes + framesData + ECB tables and framesData
   totals cross-check against the ANIM1 per-state frame counts).
3. Stage geometry → generated C tables from the same extractor (6 VS
   stages: polygon/platform/ground/ceiling/wallL/wallR/ledge/ledgePos/
   blastzone/startingPoint/startingFace/respawnPoints/respawnFace/scale/
   offset/movingPlats; geometry doubles as collision — ONE source of
   truth; doubles bit-exact as in task 2) — done-check:
   `bash pipeline/check-stages.sh` → prints `STAGES OK`, exit 0 (two
   fresh runs byte-identical; 6 stages; per-stage element counts ==
   expected.json pins measured from executed data).
4. Audio conversion + sound map: extractor additionally exports the
   `sounds` table (180 Howl names → wav path + volume + sprite windows)
   and the MusicManager track list; ffmpeg (version + exact flags pinned
   in the manifest) converts all 204 `dist/sfx/*.wav` → 22050 Hz mono
   S16LE raw PCM blobs and all 8 `dist/music/*.ogg` → 22050 Hz stereo
   S16LE raw PCM — done-check: `bash pipeline/check-audio.sh` → prints
   `AUDIO OK`, exit 0 (two fresh runs byte-identical; 204 sfx blobs /
   180 mapped sounds / 8 tracks == expected.json; every blob's byte
   length ≡ 0 mod frame size and sample count recorded in the manifest;
   provenance marks Nintendo-derived PRIVATE).
5. Full-pipeline runner + M1 exit gate: `pipeline/verify_pipeline.sh`
   runs the ENTIRE pipeline twice fresh (all registered stages) into
   `pipeline/build/gate-{a,b}`, asserts manifest byte-identity,
   re-verifies every artifact hash, and asserts the FULL expected.json
   coverage contract (754-file animation reconciliation / 27,808 paths /
   5 chars / 6 stages / 204 SFX blobs with 180 mapped sounds / 8 tracks)
   — done-check: `bash pipeline/verify_pipeline.sh` → prints
   `PIPELINE OK`, exit 0 (this IS the M1 exit gate recorded in
   CLAUDE.md §Commands; the phase-advance CHECKER re-runs it).

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
