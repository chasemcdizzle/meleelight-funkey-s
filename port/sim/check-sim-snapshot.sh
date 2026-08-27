#!/usr/bin/env bash
# port/sim/check-sim-snapshot.sh — ticket #28: the sim's state writes out and
# reads back, exactly.
#
# WHAT THIS CHECK IS FOR
# ----------------------
# A resumable match (#29) needs the running sim written to a file and read
# back into a different process. GameState is ~160 KB of nested structs that
# is NOT a byte image — it carries pointers, and the planes around it carry
# function pointers — so the snapshot is a FIELD TABLE, the same mechanism
# ticket #22 put behind the settings record (ADR 0001). port/sim/sim/
# sim_snapshot.{c,h} is that table; this is what proves it.
#
# TWO BARS, BOTH REQUIRED, NEITHER SUBSUMING THE OTHER (CONTEXT.md, "the same
# state"):
#
#   BYTE ROUND TRIP        write, POISON every persisted byte, read back, and
#                          every row must be byte-identical to what was there;
#                          then write again and the two files must match.
#                          Catches a SERIALISER bug. The poison is the whole
#                          point: without it the comparison is a value against
#                          itself and a dropped field passes forever.
#
#   CHECKSUM CONTINUATION  run a golden; run it again snapshotting at frame K
#                          in one process and restoring into ANOTHER, and the
#                          frames after K must be identical to the
#                          uninterrupted run. Catches a COMPLETENESS bug —
#                          state nobody knew had to be saved. A round trip
#                          cannot find that, because it never knew the field
#                          existed.
#
# The continuation is judged by oracle/harness/verify-stream.js, UNMODIFIED
# (HARD RULE 3), against the frozen oracle/goldens/*.sha256.json — exact
# per-frame hash equality over the full length. Its bytes are pinned below,
# so this check cannot go green against a weakened verifier.
#
# THE M2 EXIT GATE IS UNTOUCHED. port/sim/check-sim.sh's TU list does not
# carry sim_snapshot.c; the two hooks it calls are NULL pointers there (the
# ml_sim_runai_live / tp_custom_setup pattern). Leg [1] does not take that on
# trust: it DERIVES its TU list from check-sim.sh's own build command (so the
# lists cannot drift apart), pins check-sim.sh's bytes, and builds the frozen
# binary WITHOUT sim_snapshot.c to witness that the snapshot environment
# variables do nothing at all there.
#
# HOST ONLY. Prints `SIM SNAPSHOT OK`, exit 0. Any deviation exits nonzero.
set -euo pipefail
cd "$(dirname "$0")/../.."

CAL=port/sim/calib
BUILD=$CAL/build
SIM=port/sim/sim
TABLES=pipeline/build/sim-tables
WORK=$BUILD/snapshot
GATE=port/sim/check-sim.sh

fail() { echo "SIM SNAPSHOT FAIL: $1" >&2; exit 1; }

mkdir -p "$BUILD"
rm -rf "$WORK"
mkdir -p "$WORK"

# --- no-commit guard (every committed check carries one) ---------------------
tree_fingerprint() {
  local status diff files hashes
  status="$(git status --porcelain)" || return 1
  diff="$(git diff)" || return 1
  files="$(git ls-files -o --exclude-standard)" || return 1
  files="$(printf '%s\n' "$files" | grep -v '^\.tokensave/' || true)"
  if [ -n "$files" ]; then
    hashes="$(printf '%s\n' "$files" | tr '\n' '\0' | xargs -0 shasum -a 256)" \
      || return 1
  else
    hashes=""
  fi
  printf '%s\n%s\n%s\n' "$status" "$diff" "$hashes" | shasum -a 256 |
    cut -d' ' -f1
}
dirty_before="$(tree_fingerprint)" \
  || fail "could not fingerprint the working tree (fails CLOSED)"
[[ "$dirty_before" =~ ^[0-9a-f]{64}$ ]] \
  || fail "tree fingerprint '$dirty_before' is not a sha256 digest"

sha_of() { shasum -a 256 "$1" | cut -d' ' -f1; }

# --- [0] pins ----------------------------------------------------------------
# The three claims this check is built on, asserted textually. If a refactor
# removes one, this fails loudly here rather than going quietly vacuous.
echo "=== [0] pins"

# (a) The stream verifier is the oracle's, unmodified. Everything the
#     continuation leg concludes is this file's judgement.
VERIFY_SHA=f420723433b19166b53a80aedf54931ffdfbc6d2505c773fd73b7a13bbcdf60e
[ "$(sha_of oracle/harness/verify-stream.js)" = "$VERIFY_SHA" ] \
  || fail "[0] oracle/harness/verify-stream.js is not the frozen verifier \
(HARD RULE 3). This check judges by it; a changed verifier invalidates every \
verdict below."

# (b) The M2 EXIT GATE's bytes. Ticket #28 must not have touched it, and leg
#     [1] derives its TU list from this file — so a change here is a change to
#     what this check builds, and has to be seen.
GATE_SHA=ce0882bee2a0bb0ad11ac51366ef467c3811d832f9dc932c4eb10dd3ccc4c8cb
[ "$(sha_of $GATE)" = "$GATE_SHA" ] \
  || fail "[0] $GATE changed. It is the M2 EXIT GATE and leg [1] derives its \
TU list from it: re-read the build command, re-measure, and move this pin \
deliberately."

# (c) The mechanism itself: the table, the size assertion and the five
#     alignment-hole assertions that stop a member hiding in the padding.
pin1() { # <file> <exact line>
  local n
  n="$(grep -cxF "$2" "$1")" || true
  [ "$n" = 1 ] \
    || fail "[0] $1 has $n copies of |$2| (want exactly 1) — the mechanism \
this check judges moved; re-read it"
}
pin1 "$SIM/sim_snapshot.c" 'static const SsField SS_TABLE[] = {SS_FIELDS(SS_ROW)};'
pin1 "$SIM/sim_snapshot.c" 'enum { SS_TABLE_BYTES = 0 SS_FIELDS(SS_ROW_BYTES) };'
pin1 "$SIM/sim_snapshot.c" '_Static_assert(sizeof(GameState) == SS_TABLE_BYTES + SS_PAD_BYTES,'
holes="$(grep -c '^_Static_assert(SS_GAP(' "$SIM/sim_snapshot.c")" || true
[ "$holes" = 5 ] \
  || fail "[0] sim_snapshot.c declares $holes alignment-hole assertions (want \
5). Each one is what stops a new member hiding inside GameState's padding \
without changing sizeof; a hole that stops being asserted is a hole a field \
can slip into."
rows="$(grep -cE '^  X\([A-Za-z]' "$SIM/sim_snapshot.c")" || true
[ "$rows" = 24 ] \
  || fail "[0] SS_FIELDS has $rows rows (want 24 — GameState's 24 members). A \
row was added or removed; if that is deliberate the number moves with it."
echo "  [0] OK: verifier + gate pinned, table + 6 static assertions + 24 rows"

# --- [1] the build -----------------------------------------------------------
# The TU list is DERIVED from check-sim.sh's own cc invocation rather than
# copied, so the snapshot build and the gate build cannot drift apart.
echo "=== [1] build"
bash pipeline/extractor/build-extractor.sh
node pipeline/run.js --only animations,tables,stages --out "$TABLES"
test -f "$TABLES/ml_tables.c" || fail "[1] CTAB1 tables missing"
test -f "$TABLES/ml_stages.c" || fail "[1] STAB1 stages missing"

node "$CAL/dump-sim-data.js" --out "$WORK/simdata.txt"
node "$CAL/dump-sim-data.js" --out "$WORK/simdata.b.txt"
cmp "$WORK/simdata.txt" "$WORK/simdata.b.txt" \
  || fail "[1] SIMDATA1 is not byte-stable across two fresh dumps"
rm -f "$WORK/simdata.b.txt"

TU_BLOCK="$(awk '
  /-o "\$BUILD\/sim_host"/ {f=1; next}
  f {print}
  /port\/fdlibm\/fdlibm.c -lm/ {if (f) exit}
' "$GATE")"
[ -n "$TU_BLOCK" ] || fail "[1] could not derive the TU list from $GATE"
case "$TU_BLOCK" in
  *sim_snapshot*) fail "[1] $GATE now links sim_snapshot.c. The M2 EXIT GATE's \
TU list is frozen and the snapshot seam exists precisely so it need not \
change; if this is deliberate it is an owner ruling." ;;
esac

CFLAGS=(-O2 -ffp-contract=off -Wall -Wextra -Werror
        -I"$TABLES" -Iport/ryu -Iport/sim -Ioracle/qjs)

# The snapshot binary: the gate's TUs plus sim_snapshot.c.
eval cc "${CFLAGS[@]}" -o '"$WORK/sim_host_snap"' "$SIM/sim_snapshot.c" \
  "$TU_BLOCK"
# The frozen binary: the gate's TUs and nothing else — the witness for
# "check-sim.sh builds what it always built".
eval cc "${CFLAGS[@]}" -o '"$WORK/sim_host_frozen"' "$TU_BLOCK"
echo "  [1] OK: sim_host_snap + sim_host_frozen (cc -O2 -ffp-contract=off)"

# --- [2] the module-state ledger ---------------------------------------------
# GameState's completeness is a compile-time assertion (leg [0](c)). The state
# that lives in module scope has no such natural guard, so it gets this one:
# every mutable file-scope `static` in the sim is re-derived from the sources
# and diffed against port/sim/sim/sim-modstate.frozen.txt, where each is
# classified persisted / reconstructed / transient with its reason. A new
# static without a classification fails here.
echo "=== [2] module-state ledger"
LEDGER=$SIM/sim-modstate.frozen.txt
test -f "$LEDGER" || fail "[2] the module-state ledger is missing"

# Expand the gate's own token list (globs and $TABLES included) into real
# paths, then keep the C sources.
TU_FILES="$(eval "printf '%s\n' $TU_BLOCK" | grep -E '\.c$' | sort -u)"
[ -n "$TU_FILES" ] || fail "[2] no TUs derived from the gate's build command"

: > "$WORK/modstate.derived"
for f in $TU_FILES "$SIM/sim_snapshot.c"; do
  case "$f" in
    "$TABLES"/*) continue ;;   # generated CTAB1/STAB1 data planes
    oracle/qjs/*|port/fdlibm/*) continue ;;  # vendored
  esac
  [ -f "$f" ] || continue
  /usr/bin/grep -hE '^static[[:space:]]' "$f" \
    | /usr/bin/sed -E 's|//.*$||' \
    | /usr/bin/grep -v '(' \
    | /usr/bin/grep -v '^static const' \
    | /usr/bin/sed -E 's/[[:space:]]*(=.*|;.*)$//' \
    | /usr/bin/sed -E 's/\[[^]]*\]//g' \
    | /usr/bin/sed -E 's/\*//g' \
    | /usr/bin/awk -v F="$f" 'NF>1 {print F" "$NF}' >> "$WORK/modstate.derived" \
    || true
done
sort -u "$WORK/modstate.derived" -o "$WORK/modstate.derived"

grep -vE '^\s*(#|$)' "$LEDGER" | awk '{print $1" "$2}' | sort -u \
  > "$WORK/modstate.ledger"
if ! diff -u "$WORK/modstate.ledger" "$WORK/modstate.derived" \
     > "$WORK/modstate.diff"; then
  cat "$WORK/modstate.diff" >&2
  fail "[2] the sim's mutable module state does not match $LEDGER. A '+' line \
is a static nobody classified: decide whether a snapshot must CARRY it \
(a row in SS_MODULES), whether boot RECONSTRUCTS it, or whether it is \
TRANSIENT within a frame — then say so, with the reason, in the ledger."
fi
nmod="$(wc -l < "$WORK/modstate.ledger" | tr -d ' ')"
grep -c ' persisted ' "$LEDGER" > /dev/null \
  || fail "[2] the ledger classifies nothing as persisted — the classes are \
part of the mechanism, not decoration"
echo "  [2] OK: $nmod mutable module statics, every one classified"

# --- [3] the byte round trip -------------------------------------------------
# In-process, on a live match at frame 1800: save, poison every persisted
# byte, load back, compare every row, save again, compare the files. Then the
# run CONTINUES to 3600 and its stream is judged against the frozen golden —
# so the round trip is proven not to have disturbed the state it restored.
echo "=== [3] byte round trip (g01, frame 1800)"
node "$SIM/trace-to-txt.js" oracle/goldens/g01-fox-marth-battlefield.trace.json \
  "$WORK/g01.trace.txt"
MLFK_SNAP_OUT="$WORK/g01.rt.snap" MLFK_SNAP_AT=1800 MLFK_SNAP_ROUNDTRIP=1 \
  "$WORK/sim_host_snap" --trace "$WORK/g01.trace.txt" \
  --simdata "$WORK/simdata.txt" --seed 1337 --p1 2 --p2 0 --stage 0 \
  --frames 3600 > "$WORK/g01.rt.txt" 2> "$WORK/g01.rt.err" \
  || { cat "$WORK/g01.rt.err" >&2; fail "[3] the round-trip run died"; }
grep -q '^SNAP ROUNDTRIP OK frame 1800 ' "$WORK/g01.rt.err" \
  || { cat "$WORK/g01.rt.err" >&2
       fail "[3] the round trip did not report success"; }
node "$SIM/wrap-run.js" g01 "$WORK/g01.rt.txt" "$WORK/g01.rt.json" > /dev/null
node oracle/harness/verify-stream.js "$WORK/g01.rt.json" \
  oracle/goldens/g01-fox-marth-battlefield.sha256.json \
  || fail "[3] the run that round-tripped its own state no longer conforms"
echo "  [3] OK: every row byte-identical through poison, both files identical,"
echo "         and the run it interrupted still conforms to the frozen golden"

# --- [4] the checksum continuation -------------------------------------------
# THE COMPLETENESS TEST, and the reason this ticket needs two. Run the golden
# straight through; then run it again, snapshotting at the half-way frame in
# one process and RESTORING into another, and require that every frame after
# the restore point is identical to the uninterrupted run. A field nobody knew
# had to be saved shows up here and cannot show up in a round trip, which only
# ever compares the fields it already knows about.
#
# The verdict is not ours: the restored frames are SPLICED onto the
# uninterrupted run's prefix and the whole stream is judged by the UNCHANGED
# oracle/harness/verify-stream.js against the frozen golden — exact per-frame
# hash equality over the full 3600 frames, plus the rngCalls /
# rngCallsOutsideStep pins, which the restored process can only reproduce if
# it carried the RNG counters across too.
echo "=== [4] checksum continuation"

: "${SNAP_GOLDENS:=}"
ids="$SNAP_GOLDENS"
if [ -z "$ids" ]; then
  ids=$(node -e "const m=require('./oracle/goldens/manifest.json');console.log(m.goldens.map(g=>g.id).join(' '))")
fi

conf=0
for id in $ids; do
  eval "$(node -e "
    const m=require('./oracle/goldens/manifest.json');
    const g=m.goldens.find(x=>x.id==='$id');
    if(!g) { console.error('no such golden'); process.exit(1); }
    console.log('name='+g.name);
    console.log('seed='+g.seed);
    console.log('p1='+g.p1); console.log('p2='+g.p2);
    console.log('stage='+g.stage);
    console.log('frames='+g.frames);
    console.log('cpu='+(g.cpu?1:0));
    console.log('difficulty='+(g.difficulty||5));
    console.log('trace='+g.trace);
  ")"
  echo "  == $id ($name)"
  node "$SIM/trace-to-txt.js" "oracle/goldens/$trace" "$WORK/$id.trace.txt" \
    > /dev/null

  # The CPU goldens exercise the AI plane and its seeded draws, and their
  # AIBRIDGE1 artifact is a RECON row: a restored process reloads it and the
  # snapshot carries only the cursor into it (which is real match state, and
  # is verified against the reloaded artifact's identity on the way in).
  bridge_args=()
  if [ "$cpu" = "1" ]; then
    art="$BUILD/$id.ai-bridge.txt"
    if [ ! -f "$art" ]; then
      echo "     AI bridge artifact absent — recording the ai capture"
      node "$CAL/run-capture.js" --spec ai --golden "$id" \
        --out-jsonl "$WORK/$id.ai.jsonl" --out-run "$WORK/$id.ai-run.json"
      node oracle/harness/verify-stream.js "$WORK/$id.ai-run.json" \
        "oracle/goldens/$name.sha256.json" \
        || fail "[4] the $id ai capture perturbed the sim"
      node "$CAL/build-ai-bridge.js" "$id" "$WORK/$id.ai.jsonl" "$art"
    fi
    bridge_args=(--cpu --difficulty "$difficulty" --ai-bridge "$art")
  fi

  # <outfile> [VAR=value ...] — the environment is passed through `env` on
  # purpose: whether a `VAR=x func` prefix survives a shell function differs
  # between bash modes, and a snapshot control that silently leaked into the
  # next run would be an invisible false pass.
  run() {
    local out="$1"; shift
    env "$@" "$WORK/sim_host_snap" \
      --trace "$WORK/$id.trace.txt" --simdata "$WORK/simdata.txt" \
      --seed "$seed" --p1 "$p1" --p2 "$p2" --stage "$stage" \
      --frames "$frames" ${bridge_args[@]+"${bridge_args[@]}"} > "$out"
  }

  # (a) the uninterrupted run, judged in its own right — so a continuation
  #     that matched a BROKEN baseline could not pass quietly.
  run "$WORK/$id.uninterrupted.txt" \
    || fail "[4] $id: the uninterrupted run died"
  node "$SIM/wrap-run.js" "$id" "$WORK/$id.uninterrupted.txt" \
    "$WORK/$id.uninterrupted.json" > /dev/null
  node oracle/harness/verify-stream.js "$WORK/$id.uninterrupted.json" \
    "oracle/goldens/$name.sha256.json" > /dev/null \
    || fail "[4] $id: the uninterrupted run does not conform — the \
continuation below would be measuring the wrong thing"

  at=$(( frames / 2 ))

  # (b) snapshot at the half-way frame, in a process that then exits.
  run "$WORK/$id.prefix.txt" "MLFK_SNAP_OUT=$WORK/$id.snap" \
      "MLFK_SNAP_AT=$at" MLFK_SNAP_STOP=1 2> "$WORK/$id.save.err" \
    || { cat "$WORK/$id.save.err" >&2; fail "[4] $id: the saving run died"; }
  grep -q "^SNAP WROTE .* frame $at " "$WORK/$id.save.err" \
    || { cat "$WORK/$id.save.err" >&2
         fail "[4] $id: no snapshot was written at frame $at"; }

  # (c) restore into a DIFFERENT process and play the rest.
  run "$WORK/$id.resumed.txt" "MLFK_SNAP_IN=$WORK/$id.snap" \
    2> "$WORK/$id.load.err" \
    || { cat "$WORK/$id.load.err" >&2; fail "[4] $id: the resumed run died"; }
  first=$(grep -m1 '^F ' "$WORK/$id.resumed.txt" | awk '{print $2}')
  [ "$first" = "$(( at + 1 ))" ] \
    || fail "[4] $id: the resumed run started at frame $first, not $(( at + 1 ))"

  # Frame-for-frame against the uninterrupted run ... (split through FILES,
  # not pipes: `grep | head` hands grep a SIGPIPE and `set -o pipefail` would
  # then report a check failure that is really a plumbing artefact.)
  grep '^F ' "$WORK/$id.uninterrupted.txt" > "$WORK/$id.frames-a.txt"
  tail -n "$(( frames - at ))" "$WORK/$id.frames-a.txt" > "$WORK/$id.tail-a.txt"
  grep '^F ' "$WORK/$id.resumed.txt" > "$WORK/$id.tail-c.txt"
  cmp "$WORK/$id.tail-a.txt" "$WORK/$id.tail-c.txt" \
    || fail "[4] $id: the frames after the restore point differ from the \
uninterrupted run — something in the sim state was not carried"

  # ... and, spliced onto the prefix, against the FROZEN golden, by the
  # oracle's own verifier. This is the leg's actual verdict.
  {
    head -n "$at" "$WORK/$id.frames-a.txt"
    cat "$WORK/$id.tail-c.txt"
    grep -E '^(RNG |SIM OK$)' "$WORK/$id.resumed.txt"
  } > "$WORK/$id.spliced.txt"
  node "$SIM/wrap-run.js" "$id" "$WORK/$id.spliced.txt" \
    "$WORK/$id.spliced.json" > /dev/null
  node oracle/harness/verify-stream.js "$WORK/$id.spliced.json" \
    "oracle/goldens/$name.sha256.json" \
    || fail "[4] $id: the spliced stream does not conform to the frozen golden"
  conf=$(( conf + 1 ))
done
[ "$conf" -ge 1 ] || fail "[4] no golden was covered"
echo "  [4] OK: $conf golden(s) continued across a process boundary"

# --- no-commit guard ---------------------------------------------------------
dirty_after="$(tree_fingerprint)" \
  || fail "could not re-fingerprint the working tree (fails CLOSED)"
[ "$dirty_before" = "$dirty_after" ] \
  || fail "this check wrote to a tracked file; a check is a reader"

echo "SIM SNAPSHOT OK"
