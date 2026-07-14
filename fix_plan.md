# fix_plan — the loop's priority queue

Current phase: M2-CAL

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

(M1 PASSED its exit gate `bash pipeline/verify_pipeline.sh` on the
phase-advance iteration 14, driver-verified — PIPELINE OK, exit 0,
64s wall; MILESTONE PASS: M1 logged in docs/AGENT-LOG.md iter 14.)

(task 5 — full-pipeline runner + M1 exit gate — DONE iter 13:
`bash pipeline/verify_pipeline.sh` → PIPELINE OK, exit 0. The gate
composes the four unchanged task-level checks + one whole-pipeline
double-run into `build/gate-{a,b}` with manifest byte-identity, full
artifact re-hash, the FULL expected.json contract, gate-a compiled
round-trips (38832 + 412 leaves pinned) and the no-commit guard. §M1 is
empty — the next iteration is the M1 phase-advance; the driver/CHECKER
owns re-running the gate and advancing the phase.)

## M2-CAL — Calibration slice

(concretized by REPLAN, iter 15, 2026-07-14. Conventions fixed here: the
slice lives at `port/sim/` — `environmental_collision.{c,h}` +
`util/{vec2d,lin_alg,find_smallest_within,solve_quadratic_equation,
line_angle,extreme_point,ecb_transform,zip_labels}.{c,h}` structure-parallel
to the JS module paths (drawECB = documented no-op, render-only). The
capture/replay rig lives at `port/sim/calib/`; captures land in
`port/sim/calib/build/` (gitignored `build*/`). CAPTURE APPROACH
(PROVISIONAL, auto-adopted): the module boundary is wrapped AT RUNTIME —
the capture runner serves the untouched built dist but textually exposes
the webpack module cache (`var installedModules = {};` → `+
window.__wpCache = installedModules;` in the served bytes only, disk
untouched), then wraps every exported function of the
environmentalCollision module object post-load (babel CJS exports are
plain writable properties; all external call sites dereference the
namespace object at call time — verified in dist/js/main.js; internal
calls use local bindings and are correctly NOT captured). oracle/ is NOT
modified (HARD RULE 3): the runner reuses oracle/harness/{init,pagelib}.js
+ port/fdlibm/fdlibm.js VERBATIM by path and judges every capture run with
the unchanged verify-stream.js against the frozen golden stream —
instrumentation that perturbs the sim cannot pass. Value serialization
(canon v1, PROVISIONAL deviation from CHECKSUM.md conventions, documented
in port/sim/calib/FORMAT.md): CHECKSUM.md structural rules (sorted nested
keys, T/F/null/undef/fn tokens, arrays element-wise) but numbers as
IEEE-754 bit-pattern hex `d:<16hex>` instead of shortest-round-trip
decimal — injective on doubles both ways, strictly bit-exact (a single
ulp diverges), and keeps the ECMAScript shortest-float formatter out of
the calibration (it is a known one-time M2 component, not part of the
per-KLOC translation rate being measured). Captured goldens: g01
(fox/marth/battlefield — the PLAN §4 anchor trace), g04
(puff/falcon/dreamland — puff movement stresses collision), g06
(falcon/marth/fountain — moving platforms make the stage argument vary
per frame). Full manifest frame counts (3600), manifest params only.)

(task 1 — capture rig — DONE iter 16:
`bash port/sim/calib/check-capture.sh` → CAPTURE OK, exit 0. 186,675
boundary records over g01/g04/g06, byte-stable ×2, 6× STREAM MATCH,
counts pinned in expected-capture.json.)

(task 2 — structure-parallel C translation + replay driver — DONE iter
17: `bash port/sim/calib/check-replay-runs.sh` → REPLAY RAN 119619
records, 0 divergences, exit 0. All three captures replay bit-identical
on the first successful build — comparator teeth proven by negative
tests: transcription-typo build → 11,883 divergences; single corrupted
capture nibble → exactly 1.)
3. Burn-down to bit-identical over ALL records of ALL 3 captures; keep
   the divergence ledger (frame, function, root-cause class, fix,
   minutes); write docs/M2CAL-REPORT.md (div/KLOC, fix-rate, class
   breakdown, wall-clock, M2 projection, go/no-go per PLAN §4) —
   done-check: `bash port/sim/check-envcoll.sh` → prints `ENVCOLL MATCH`,
   exit 0 (rebuilds C, re-records captures fresh if absent, replays all
   3 captures with ZERO divergences over full frame ranges, asserts
   docs/M2CAL-REPORT.md carries the filled metrics table)

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
