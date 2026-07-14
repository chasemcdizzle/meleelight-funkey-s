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
