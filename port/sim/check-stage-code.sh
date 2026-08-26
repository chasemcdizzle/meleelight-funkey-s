#!/usr/bin/env bash
# A45 T1 done-check: the stage share-code codec (port/sim/stage_code.c) and
# the custom-stage value model (port/sim/stage_code.h), proven DIFFERENTIALLY
# against upstream's OWN executed src/stages/encode.js — never against a
# transcription of it, because a transcription bug mirrors itself on both
# sides of a differential and proves nothing (the fmt_diff discipline,
# port/sim/calib/check-format.sh).
#
#   1. build every TU with cc -O2 -ffp-contract=off -Wall -Wextra -Werror
#   2. self-test: the value model's size and the frozen toFixed / -0 /
#      polygonMap anchors, so a broken build fails fast and legibly
#   3. toFixed(2) differential: ~800k doubles (the .5 boundary of every
#      hundredth and its two neighbours, thousandths, the named IEEE
#      boundaries, and 500k seeded patterns) formatted by ml_to_fixed2 vs
#      V8's own Number.prototype.toFixed(2) — cmp, byte-exact, and the C
#      side byte-stable across two runs
#   4. transpile upstream's encode.js and its six dependencies OUT of the
#      READ-ONLY pinned clone with the CLONE'S OWN babel, into the
#      gitignored build directory; those seven files are asserted to be the
#      pin's own bytes, and the clone is asserted unchanged by the run
#   5. generate a stage corpus with upstream's createStageCode and split it
#      by upstream's own idempotence into `wellformed` (a code that is its
#      own fixed point) and `edge` (the measured exceptions)
#   6. both corpora through C mlk_parse+mlk_encode and through upstream's
#      parseStageCode+createStageCode — cmp, byte-exact, C byte-stable
#   7. hostile corpus: 42 malformed / out-of-grammar / over-cap inputs, both
#      sides' verdicts frozen in expected-stage-code.json. This is where
#      deviation D39 lives and it is reviewable rather than asserted.
#   8. judge: idempotence both ways, the POSITIVE assertion that upstream
#      BUG 1 occurred (every sixth surface lost its damage digit), and the
#      pin table
#   9. four ORTHOGONAL teeth, each on a scratch COPY of stage_code.c (the
#      tracked file is never edited), each biting a DIFFERENT leg
#
# Prints STAGECODE MATCH, exit 0. Exact equality only; never weakened.
set -euo pipefail
cd "$(dirname "$0")/../.."

CLONE="${MELEELIGHT_CLONE:-$HOME/.cache/meleelight-funkey-s/upstream}"
PIN=27af171d983184c7100be87d4c5ba30c0e35a5ae
REF=port/sim/calib/stage-code-js-ref.js
EXPECTED=port/sim/calib/expected-stage-code.json
B=port/sim/calib/build/stagecode
CFLAGS=(-O2 -ffp-contract=off -Wall -Wextra -Werror -Iport/ryu -Iport/sim)

mkdir -p "$B"

# --- 0. the clone: present, pinned, and untouched --------------------------
if [ ! -d "$CLONE/.git" ]; then
  echo "STAGECODE FAIL: no upstream clone at $CLONE (bash oracle/build-upstream.sh)" >&2
  exit 1
fi
have=$(git -C "$CLONE" rev-parse HEAD)
if [ "$have" != "$PIN" ]; then
  echo "STAGECODE FAIL: clone is at $have, expected the pin $PIN" >&2
  exit 1
fi
# The clone legitimately carries oracle/meleelight-harness.patch, so "clean"
# is the wrong assertion. What must hold is that THE SEVEN FILES THIS CHECK
# TREATS AS THE ORACLE are the pin's own bytes — otherwise a future patch to
# a dependency would silently redefine the reference the differential is
# judged against.
SRCS=(src/stages/encode.js src/main/util/Box2D.js src/main/util/Vec2D.js
      src/main/util/deepValue.js src/main/linAlg.js
      src/stages/util/extremePoint.js src/target/util/getConnected.js)
dirty=$(git -C "$CLONE" status --porcelain -- "${SRCS[@]}")
if [ -n "$dirty" ]; then
  echo "STAGECODE FAIL: the oracle sources are modified in the clone:" >&2
  echo "$dirty" >&2
  exit 1
fi
echo "  oracle sources are the pin's own bytes (${#SRCS[@]} files at $PIN)"

clone_state() { git -C "$CLONE" rev-parse HEAD; git -C "$CLONE" status --porcelain; }
CLONE_BEFORE=$(clone_state)

# --- 1. build --------------------------------------------------------------
cc "${CFLAGS[@]}" -o "$B/stage_code_diff" \
  port/sim/calib/stage_code_diff.c port/sim/stage_code.c port/sim/ml_fmt.c -lm
echo "build OK: $B/stage_code_diff (cc -O2 -ffp-contract=off)"

# --- 2. self-test ----------------------------------------------------------
"$B/stage_code_diff" --self-test

# --- 3. toFixed(2) differential --------------------------------------------
"$B/stage_code_diff" --genhex "$B/fx.hex"
"$B/stage_code_diff" --tofixed "$B/fx.hex" "$B/fx.c.txt"
"$B/stage_code_diff" --tofixed "$B/fx.hex" "$B/fx.c2.txt"
cmp "$B/fx.c.txt" "$B/fx.c2.txt"
node "$REF" tofixed "$B/fx.hex" "$B/fx.js.txt"
cmp "$B/fx.c.txt" "$B/fx.js.txt"
echo "  toFixed(2) differential: $(wc -l < "$B/fx.hex" | tr -d ' ') doubles bit-identical to V8, C byte-stable"

# --- 4. upstream's own bytes, out of the read-only clone -------------------
rm -rf "$B/tp"
mkdir -p "$B/tp"
node "$REF" transpile "$CLONE" "$B/tp"
if [ "$(clone_state)" != "$CLONE_BEFORE" ]; then
  echo "STAGECODE FAIL: the read-only clone changed during the run" >&2
  exit 1
fi
echo "  clone unchanged (HEAD + working tree)"

# --- 5/6/7. the stage-level differential -----------------------------------
node "$REF" gen "$B/tp" "$B"
for k in wellformed edge hostile; do
  # upstream's parseStageCode console.log()s its own rejections; those are
  # data, not diagnostics, so they stay out of the check's output.
  node "$REF" ref "$B/tp" "$B/codes-$k.txt" "$B/$k.js.txt" > "$B/$k.js.log"
  "$B/stage_code_diff" --ref "$B/codes-$k.txt" "$B/$k.c.txt" 2> "$B/$k.c.err"
  "$B/stage_code_diff" --ref "$B/codes-$k.txt" "$B/$k.c2.txt" 2> /dev/null
  cmp "$B/$k.c.txt" "$B/$k.c2.txt"
done
cmp "$B/wellformed.c.txt" "$B/wellformed.js.txt"
cmp "$B/edge.c.txt" "$B/edge.js.txt"
echo "  stage differential: wellformed + edge byte-identical to upstream's executed encode.js"

# --- 7b. the CONNECTED plane (A45 T5) --------------------------------------
# parseStageCode does not just read a code, it DERIVES `connected` from the
# parsed surfaces (encode.js:237) — so it is part of what a custom stage IS,
# and the port derives it in tp_stage_from_custom (util/get_connected.h).
# It needs its own corpus because the buckets above cannot reach it: their
# coordinates are random doubles, so two surfaces coinciding within
# getConnected's 0.001 never happens, which is precisely why this plane was
# wrong on disk from A45 T2 until 2026-08-26 with every check green.
node "$REF" conngen "$B/tp" "$B"
node "$REF" conn "$B/tp" "$B/codes-conn.txt" "$B/conn.js.txt" > "$B/conn.js.log"
"$B/stage_code_diff" --conn "$B/codes-conn.txt" "$B/conn.c.txt"
"$B/stage_code_diff" --conn "$B/codes-conn.txt" "$B/conn.c2.txt"
cmp "$B/conn.c.txt" "$B/conn.c2.txt"
cmp "$B/conn.c.txt" "$B/conn.js.txt"
# ...and the plane the OTHER buckets do reach must agree too: all-null on
# both sides is the shipped corpus's answer and it is asserted, not assumed.
for k in wellformed edge; do
  node "$REF" conn "$B/tp" "$B/codes-$k.txt" "$B/conn-$k.js.txt" > /dev/null
  "$B/stage_code_diff" --conn "$B/codes-$k.txt" "$B/conn-$k.c.txt"
  cmp "$B/conn-$k.c.txt" "$B/conn-$k.js.txt"
done
echo "  connected differential: byte-identical to upstream's executed getConnected"

# --- 8. judge (idempotence, the BUG 1 assertion, the frozen pins) ----------
node "$REF" judge "$B" "$EXPECTED"

# --- 9. teeth --------------------------------------------------------------
# Each tooth perturbs a scratch COPY of stage_code.c — the tracked file is
# never edited, so there is nothing to restore and no index to corrupt.
# Each bites a DIFFERENT leg, so none of them can stand in for another.
tooth() { # <name> <sed-expr> <leg>
  local name=$1 expr=$2 leg=$3
  sed "$expr" port/sim/stage_code.c > "$B/tooth.c"
  if cmp -s "$B/tooth.c" port/sim/stage_code.c; then
    echo "STAGECODE FAIL: tooth '$name' did not perturb anything — its site moved" >&2
    exit 1
  fi
  cc "${CFLAGS[@]}" -o "$B/tooth" \
    port/sim/calib/stage_code_diff.c "$B/tooth.c" port/sim/ml_fmt.c -lm
  # A tooth bites when it CHANGES THE ARTIFACT THIS LEG JUDGES, measured
  # against the unperturbed C baseline. Never against the JS side: the
  # hostile leg's C and JS answers already differ on 21 rows by D39's
  # design, so a tooth compared to hostile.js.txt would "bite" for free
  # and the check would be quietly vacuous.
  local bit=0
  case $leg in
    selftest) "$B/tooth" --self-test > /dev/null 2>&1 || bit=1 ;;
    tofixed)
      "$B/tooth" --tofixed "$B/fx.hex" "$B/tooth.txt" > /dev/null 2>&1
      cmp -s "$B/tooth.txt" "$B/fx.c.txt" || bit=1 ;;
    wellformed|hostile)
      "$B/tooth" --ref "$B/codes-$leg.txt" "$B/tooth.txt" 2>/dev/null || true
      cmp -s "$B/tooth.txt" "$B/$leg.c.txt" || bit=1 ;;
    conn)
      "$B/tooth" --conn "$B/codes-conn.txt" "$B/tooth.txt" 2>/dev/null || true
      cmp -s "$B/tooth.txt" "$B/conn.c.txt" || bit=1 ;;
  esac
  if [ "$bit" != 1 ]; then
    echo "STAGECODE FAIL: tooth '$name' did not bite the $leg leg" >&2
    exit 1
  fi
  echo "  tooth '$name' bites the $leg leg"
}

# The connected plane lives in a HEADER, so its tooth shadows that header
# through an -I directory that precedes -Iport/sim. Same discipline: the
# tracked file is never edited and there is nothing to restore.
htooth() { # <name> <sed-expr> <leg>
  local name=$1 expr=$2 leg=$3
  # A SYMLINK FARM of port/sim, with the one header replaced by a real file.
  # get_connected.h includes "../physics.h", which resolves relative to the
  # INCLUDING FILE's directory, so a bare scratch dir does not compile — the
  # shadow tree has to look like port/sim from the inside.
  rm -rf "$B/htooth" && mkdir -p "$B/htooth/util"
  local f
  for f in port/sim/*.h; do ln -sf "$PWD/$f" "$B/htooth/$(basename "$f")"; done
  for f in port/sim/util/*.h; do
    ln -sf "$PWD/$f" "$B/htooth/util/$(basename "$f")"
  done
  # rm FIRST: writing to a symlink writes through it, into the tracked file.
  rm -f "$B/htooth/util/get_connected.h"
  sed "$expr" port/sim/util/get_connected.h > "$B/htooth/util/get_connected.h"
  if cmp -s "$B/htooth/util/get_connected.h" port/sim/util/get_connected.h; then
    echo "STAGECODE FAIL: tooth '$name' did not perturb anything — its site moved" >&2
    exit 1
  fi
  # -I the shadow BEFORE the real tree, or the real header wins and the
  # tooth silently tests nothing (measured: that is exactly what happened
  # the first time this helper was written).
  cc -I"$B/htooth" "${CFLAGS[@]}" -o "$B/tooth" \
    port/sim/calib/stage_code_diff.c port/sim/stage_code.c port/sim/ml_fmt.c -lm
  local bit=0
  "$B/tooth" --conn "$B/codes-$leg.txt" "$B/tooth.txt" 2>/dev/null || true
  cmp -s "$B/tooth.txt" "$B/$leg.c.txt" || bit=1
  if [ "$bit" != 1 ]; then
    echo "STAGECODE FAIL: tooth '$name' did not bite the $leg leg" >&2
    exit 1
  fi
  echo "  tooth '$name' bites the $leg leg"
}

# (a) "repair" upstream BUG 1 so the sixth surface keeps its damage digit.
tooth carry-bug-1 's/if (i != 5) {/if (1) {/' wellformed
# (b) round toFixed's sub-1 branch one power of two early — 0.005 and its
#     neighbours stop reaching "0.01".
tooth exact-tofixed 's/if (s >= 61) {/if (s >= 60) {/' tofixed
# (c) drop the ledge range check, so codes upstream throws on are accepted.
tooth ledge-range 's/if (!(idx >= 0 \&\& idx < (double)ref->count)) FAIL("ledge out of range");/(void)ref;/' hostile
# (e) WIDEN getConnected's coincidence threshold, so the corpus's deliberate
#     one-hundredth near-misses become links.
#     MEASURED, and it is the razor-thin-nudge class again (6th instance in
#     this project): TIGHTENING the threshold — 0.001 -> 1e-10 — is a
#     provable NO-OP here and was tried first. Every coordinate in a share
#     code is a hundredth, so a chain shares its endpoints EXACTLY and the
#     distance is 0.0, which is below any positive threshold. A tooth has to
#     perturb the domain that OCCURS: 0.01 near-misses do occur, so the tooth
#     goes the other way.
htooth connected-threshold-wide 's/< 0\.001;/< 0.05;/' conn
# (f) mislabel the wallL link as a right wall. getConnected.js:36-47 is
#     ASYMMETRIC (a right-facing wall bounds a ground on its LEFT, a
#     left-facing one on its RIGHT) and physics.js:281-285 / :311-315 branch
#     on that letter, so the LABEL is load-bearing and not decoration.
htooth connected-wall-label 's/gc_link(.l., j)/gc_link(0x72, j)/' conn
# (d) lose the parser'"'"'s -0 (invisible in any emitted code — toFixed erases
#     the sign — so only the value model can see it).
tooth minus-zero-parse 's/\*out = neg ? -d : d;/*out = (neg \&\& d != 0.0) ? -d : d;/' selftest

# --- no-commit guard -------------------------------------------------------
if git status --porcelain -- "$B" | grep -q .; then
  echo "STAGECODE FAIL: build output is not gitignored" >&2
  exit 1
fi

echo "STAGECODE MATCH"
