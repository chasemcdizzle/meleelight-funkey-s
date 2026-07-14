# fix_plan — the loop's priority queue

Current phase: M0

Rules: items live under their phase heading; an actionable item needs an
exact runnable `done-check:` (see `docs/LOOP.md` §C). Items below are
seeded from PLAN §4's milestone contracts with `done-check: …` — the first
loop iteration on each phase is therefore a REPLAN that decomposes and
concretizes them. REPLAN owns rewriting a phase's section; nothing else
edits other phases' sections.

## M0 — Oracle hardened

1. Pin + script the upstream clone/build recipe (clone @27af171, apply
   harness patch, docker node:8 build) as a committed tool — done-check: …
2. Land vendored fdlibm both sides (port/fdlibm/ C sources; JS Math shim in
   the harness init; JS↔C bit-pattern cross-check) — done-check: …
3. Build the fdlibm-patched QuickJS oracle runtime and checksum-verify it
   against the browser on golden trace #1 — done-check: …
4. Productionize the spike harness into oracle/ (recorder, runner,
   comparator; spikes/determinism stays frozen as evidence) — done-check: …
5. Freeze the checksum spec as oracle/CHECKSUM.md (spike-proven field list,
   serialization rules) — done-check: …
6. Record golden traces covering all 5 characters and all 6 VS stages
   (human + CPU), commit frozen checksum streams — done-check: …

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
