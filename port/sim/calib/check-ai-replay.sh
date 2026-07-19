#!/usr/bin/env bash
# M4 task 4 done-check: the ai.js structure-parallel C port, verified by
# strict record-by-record replay of the aiport captures. For each CPU
# golden (g07 g08):
#   - two fresh aiport-spec capture runs produce byte-identical JSONL
#     (determinism; the frame-0 sweep battery + the in-page write-set
#     recon run on both)
#   - BOTH runs' checksum streams pass the unchanged verify-stream.js
#     against the frozen golden (instrumentation + sweep non-perturbation
#     guard — rule 12: an unrestored sweep write or a stolen seeded draw
#     fails here mechanically)
#   - measured-then-frozen pins hold (expected-capture-aiport.json;
#     wsViol pinned ZERO — the recon allowlist is PER-SLOT since iter
#     77: a foreign-slot bookkeeping write is a wsViol)
#   - EVERY record replays through the C port bit-identically (--strict):
#     runAI records marshal the recorded pre state (strict, rule 7) into
#     MlAiSim/MlPlayer/tagged bank rows, call port/sim/ai.c's ml_runAI on
#     the chained C mulberry32 (frame-0 records on the mirrored sweep
#     generator), and compare the {bank, bk, rng} post envelope
#     byte-for-byte (bk = the FOUR-SLOT bookkeeping array, iter 77);
#     standalone draws chain-verify draw-for-draw.
# Leaves run A's artifacts in port/sim/calib/build/ as the canonical
# captures. Prints AI MATCH, exit 0. Never weakened: exact equality only.
#
# HARDENING CLASSES (iter 77 — review-75 round-1 closure,
# .loop/review-75-triage.md; the check-vfx-seam.sh aggregator patterns
# host-side):
#   - CORPUS INVENTORY PIN (M1): CORPUS asserted exactly 2 unique ids
#     AND == the expected-capture-aiport.json golden keys (both
#     directions) BEFORE the lock and any run — a merge that drops or
#     duplicates a golden dies here, never a half-corpus pass.
#   - RUN LOCK (review-75 Medium; iter-41/66 no-reclaim pattern): ONE
#     mkdir-atomic lock beside the shared build/ capture files; NO
#     reclamation — an existing lock is a loud refusal; the release
#     trap installs only AFTER acquisition.
#   - FRESHNESS (review-75 High): all four capture outputs are removed
#     BEFORE each golden's runs and test -s asserted after each run — a
#     recorder that no-ops can never leave stale bytes to replay.
#   - EVIDENCE COMPLETENESS (M4): the replay runs with --expect fed
#     from the pins file (per-record-type inventory; truncation =
#     corruption death) — --strict REQUIRES it.
#   - COVERAGE GATE (M7): the replay runs with --cover --cover-gate 61
#     (live arms pinned per golden, measured iter 75; the 3
#     documented-dead arms pinned ZERO).
#   - HYGIENE GUARD RC CASE-SPLIT (M2): the no-commit guard captures
#     git's exit code explicitly — a status read error is corruption,
#     never a clean pass.
#
# The AIBRIDGE1 path (check-ai-bridge.sh, M2 task 16) is untouched by
# this check — fix_plan §M4 task 5 retires it from the live path.
set -euo pipefail
cd "$(dirname "$0")/../../.."

CAL=port/sim/calib
BUILD=$CAL/build
PINS=$CAL/expected-capture-aiport.json

fail() { echo "AI REPLAY FAIL: $*" >&2; exit 1; }

# CORPUS INVENTORY PIN (iter 77, review-75 M1) — before the lock and
# before anything runs.
CORPUS=(g07 g08)
if [ "${#CORPUS[@]}" != 2 ]; then
  fail "corpus inventory pin — CORPUS has ${#CORPUS[@]} entries, want exactly 2 (a dropped/added golden is corruption, never a partial pass)"
fi
dupes="$(printf '%s\n' "${CORPUS[@]}" | sort | uniq -d)"
if [ -n "$dupes" ]; then
  fail "corpus inventory pin — duplicate CORPUS entries: $dupes"
fi
pinkeys="$(node -e "const e=require('./$PINS');process.stdout.write(Object.keys(e.goldens).sort().join(' '))")"
corpuskeys="$(printf '%s\n' "${CORPUS[@]}" | sort | tr '\n' ' ')"
corpuskeys="${corpuskeys% }"
if [ "$pinkeys" != "$corpuskeys" ]; then
  fail "corpus inventory pin — CORPUS {$corpuskeys} != expected-pin keys {$pinkeys} (both directions must agree; an untested pinned golden or an unpinned tested golden is corruption)"
fi

# RUN LOCK (iter 77, review-75; the iter-41/66 no-reclaim pattern): the
# shared resource is this checkout's build/ capture files.
mkdir -p "$BUILD"
LOCK="$BUILD/ai-replay.lock"
if ! mkdir "$LOCK" 2>/dev/null; then
  lockage="unknown"
  if lockmtime="$(stat -f %m "$LOCK" 2>/dev/null || stat -c %Y "$LOCK" 2>/dev/null)"; then
    lockage="$(( $(date +%s) - lockmtime )) s"
  fi
  echo "AI REPLAY REFUSED: run lock $LOCK already exists (age: $lockage)." >&2
  echo "  Another check-ai-replay.sh run may be rewriting the shared captures" >&2
  echo "  in $BUILD right now. NO auto-reclaim (iter-41 posture). If you are" >&2
  echo "  sure no run is live, remove it manually: rm -rf '$LOCK'" >&2
  exit 1
fi
trap 'rm -rf "$LOCK"' EXIT

# build: every TU with -ffp-contract=off (PLAN §2); fdlibm linked for the
# ai.js transcendental surface (pow/atan/cos/sin/tan)
cc -O2 -ffp-contract=off -Wall -Wextra -Werror \
  -o "$BUILD/ai_port_replay" \
  "$CAL/replay_ai_port.c" port/sim/ai.c "$CAL/canon.c" \
  port/sim/ai_bridge.c port/sim/ml_events.c port/fdlibm/fdlibm.c \
  -lm
echo "build OK: $BUILD/ai_port_replay (cc -O2 -ffp-contract=off)"

for id in "${CORPUS[@]}"; do
  name=$(node -e "const m=require('./oracle/goldens/manifest.json');console.log(m.goldens.find(g=>g.id==='$id').name)")
  # per-record-type inventory from the pins file (single source; the
  # replay's --expect evidence-completeness gate, review-75 M4)
  expect="$(node -e "const p=require('./$PINS').goldens['$id'];process.stdout.write('records='+p.records+',runAI='+p.counts['runAI']+',Math.random='+p.counts['Math.random']+',rngBoot='+p.counts['rngBoot'])")"
  # FRESHNESS (review-75 High): stale outputs die BEFORE the fresh runs
  rm -f "$BUILD/$id.aiport.jsonl" "$BUILD/$id.aiport-run.json" \
        "$BUILD/$id.aiport.b.jsonl" "$BUILD/$id.aiport-run.b.json"
  echo "== $id ($name): aiport capture run A"
  node "$CAL/run-capture.js" --spec aiport --golden "$id" \
    --out-jsonl "$BUILD/$id.aiport.jsonl" --out-run "$BUILD/$id.aiport-run.json"
  test -s "$BUILD/$id.aiport.jsonl" && test -s "$BUILD/$id.aiport-run.json" \
    || fail "$id run A left no fresh capture (freshness contract)"
  echo "== $id: aiport capture run B"
  node "$CAL/run-capture.js" --spec aiport --golden "$id" \
    --out-jsonl "$BUILD/$id.aiport.b.jsonl" --out-run "$BUILD/$id.aiport-run.b.json"
  test -s "$BUILD/$id.aiport.b.jsonl" && test -s "$BUILD/$id.aiport-run.b.json" \
    || fail "$id run B left no fresh capture (freshness contract)"
  cmp "$BUILD/$id.aiport.jsonl" "$BUILD/$id.aiport.b.jsonl"
  echo "   capture byte-identical across two fresh runs"
  node oracle/harness/verify-stream.js "$BUILD/$id.aiport-run.json"   "oracle/goldens/$name.sha256.json"
  node oracle/harness/verify-stream.js "$BUILD/$id.aiport-run.b.json" "oracle/goldens/$name.sha256.json"
  node "$CAL/check-spec-pins.js" aiport "$id" "$BUILD/$id.aiport.jsonl" "$BUILD/$id.aiport-run.json"
  rm -f "$BUILD/$id.aiport.b.jsonl" "$BUILD/$id.aiport-run.b.json"
  # bit-exact replay, full record set (pre marshal + C runAI + post
  # compare) + the evidence-completeness inventory + the coverage gate
  # (61 live arms per golden, measured iter 75; dead arms pinned ZERO)
  "$BUILD/ai_port_replay" "$BUILD/$id.aiport.jsonl" --strict --max-print 5 \
    --expect "$expect" --cover --cover-gate 61
done

# no-commit guard (rc CASE-SPLIT, iter 77 review-75 M2): captures are
# build output, never tracked; an errored status read is corruption.
rc=0
dirty="$(git status --porcelain -- "$BUILD")" || rc=$?
if [ "$rc" -ne 0 ]; then
  fail "no-commit guard — git status rc $rc (a status read error is CORRUPT evidence, never a clean pass)"
fi
if [ -n "$dirty" ]; then
  echo "AI REPLAY FAIL: build output not gitignored:" >&2
  printf '%s\n' "$dirty" >&2
  exit 1
fi

echo "AI MATCH"
