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

(task 5 — full-pipeline runner + M1 exit gate — DONE iter 13:
`bash pipeline/verify_pipeline.sh` → PIPELINE OK, exit 0. The gate
composes the four unchanged task-level checks + one whole-pipeline
double-run into `build/gate-{a,b}` with manifest byte-identity, full
artifact re-hash, the FULL expected.json contract, gate-a compiled
round-trips (38832 + 412 leaves pinned) and the no-commit guard. §M1 is
empty — the next iteration is the M1 phase-advance; the driver/CHECKER
owns re-running the gate and advancing the phase.)

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
