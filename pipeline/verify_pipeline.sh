#!/usr/bin/env bash
# M1 EXIT GATE (fix_plan §M1 task 5; CLAUDE.md §Commands "M1 EXIT GATE",
# concretized by REPLAN iter 9). Proves the WHOLE M1 phase:
#   a. byte-determinism — from a clean slate, the ENTIRE pipeline (every
#      registered stage) run twice fresh into build/gate-{a,b} produces
#      byte-identical manifest.json; verify-artifacts then re-hashes every
#      artifact in BOTH runs against its manifest entry (identical
#      manifests + per-file hash verification in both dirs => every
#      artifact byte-identical across the two runs) and rejects strays,
#   b. every task-level stage check still passes UNCHANGED
#      (check-animations / check-tables / check-stages / check-audio,
#      each with its own fresh double-run and stage-specific gates),
#   c. the FULL pinned coverage contract pipeline/expected.json holds on
#      the integrated run (check-expected.js default = every pinned
#      section: 5 chars / 744 states / 27,808 paths / the live 754-file
#      reconciliation / 6 stages / 204 SFX blobs with 180 mapped sounds /
#      8 tracks / ffmpeg pins + frozen audio aggregate),
#   d. compiled round-trips against the INTEGRATED run's own artifacts:
#      generated ml_tables.c and ml_stages.c compile (cc -ffp-contract=off)
#      and their canonical leaf dumps are byte-identical to fresh
#      executed-JS walks — exactly 38832 + 412 leaf values (pinned),
#      plus the framesData/ECB<->ANIM1 xref reconciliation on gate-a,
#   e. no-commit guard — ALL pipeline build output (incl. Nintendo-derived
#      audio blobs) is gitignored; nothing under build/ tracked or staged.
# Prints PIPELINE OK and exits 0 on success. The phase-advance CHECKER
# re-runs this command; it is deliberately self-contained (composes the
# committed checks, never reimplements their assertions).
set -euo pipefail
cd "$(dirname "$0")"

DIST="${MELEELIGHT_CLONE:-$HOME/.cache/meleelight-funkey-s/upstream}"
GATE_T0=$SECONDS

# ---- shared prerequisite: extractor bundle (idempotent, stamp-cached) ----
bash extractor/build-extractor.sh

# ---- (b) all four task-level stage checks, unchanged ---------------------
for chk in check-animations.sh check-tables.sh check-stages.sh check-audio.sh; do
  echo "=== gate: $chk ==="
  bash "$chk"
done

# ---- (a) whole-pipeline double-run from a clean slate --------------------
echo "=== gate: full-pipeline double-run ==="
rm -rf build/gate-a build/gate-b
node run.js --dist "$DIST" --out build/gate-a
node run.js --dist "$DIST" --out build/gate-b

cmp build/gate-a/manifest.json build/gate-b/manifest.json \
  || { echo "FAIL: full-run manifests differ between fresh runs (byte-stability)"; exit 1; }
echo "byte-stability: two fresh FULL-pipeline runs -> identical manifest.json"

node lib/verify-artifacts.js build/gate-a
node lib/verify-artifacts.js build/gate-b

# ---- (c) FULL coverage contract (default: every pinned section) ----------
node lib/check-expected.js build/gate-a "$DIST"

# ---- (d) round-trips against the integrated run's own artifacts ----------
echo "=== gate: compiled round-trips on gate-a ==="
node lib/tables-anim-xref.js build/gate-a

RT=build/gate-rt
rm -rf "$RT"; mkdir -p "$RT"

cc -std=c99 -O1 -ffp-contract=off -Wall -Wextra -Werror \
  -Ibuild/gate-a -o "$RT/tables_check" lib/tables_check.c build/gate-a/ml_tables.c
"$RT/tables_check" > "$RT/tables-c.dump"
node lib/tables-dump.js "$DIST" > "$RT/tables-js.dump"
cmp "$RT/tables-c.dump" "$RT/tables-js.dump" \
  || { echo "FAIL: C tables dump != fresh executed-JS walk (round-trip, gate-a)"; exit 1; }
TAB_LEAVES=$(wc -l < "$RT/tables-c.dump" | tr -d ' ')
[ "$TAB_LEAVES" -eq 38832 ] \
  || { echo "FAIL: tables round-trip leaf count $TAB_LEAVES != pinned 38832"; exit 1; }

cc -std=c99 -O1 -ffp-contract=off -Wall -Wextra -Werror \
  -Ibuild/gate-a -o "$RT/stages_check" lib/stages_check.c build/gate-a/ml_stages.c
"$RT/stages_check" > "$RT/stages-c.dump"
node lib/stages-dump.js "$DIST" > "$RT/stages-js.dump"
cmp "$RT/stages-c.dump" "$RT/stages-js.dump" \
  || { echo "FAIL: C stage dump != fresh executed-JS walk (round-trip, gate-a)"; exit 1; }
STG_LEAVES=$(wc -l < "$RT/stages-c.dump" | tr -d ' ')
[ "$STG_LEAVES" -eq 412 ] \
  || { echo "FAIL: stages round-trip leaf count $STG_LEAVES != pinned 412"; exit 1; }

echo "round-trip: compiled C == fresh executed-JS walk on the integrated run" \
  "($TAB_LEAVES + $STG_LEAVES leaf values, bit-exact)"
rm -rf "$RT"

# ---- (e) no-commit guard for ALL build output -----------------------------
git check-ignore -q build/gate-a \
  || { echo "FAIL: pipeline/build is not gitignored"; exit 1; }
TRACKED="$(git ls-files build; git status --porcelain -- build)"
[ -z "$TRACKED" ] \
  || { echo "FAIL: files under pipeline/build are tracked/staged:"; echo "$TRACKED"; exit 1; }
echo "no-commit guard: build output gitignored, nothing tracked"

echo "gate wall-time: $((SECONDS - GATE_T0))s"
echo "PIPELINE OK"
