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

(no items remain — iter 8 completed task 7, the last M0 item; next
iteration is the M0 phase-advance: CHECKER mode=phase-advance runs the
exit gate `bash oracle/verify_goldens.sh` from CLAUDE.md §Gates.)

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
