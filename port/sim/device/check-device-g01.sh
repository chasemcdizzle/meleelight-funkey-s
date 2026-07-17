#!/usr/bin/env bash
# M3 task 1 done-check: the armv7 CORRECTNESS RUNG (fix_plan §M3 task 1).
#
# Validates the entire M2 result on the real FunKey-S CPU before anything
# visual lands. Cross-builds the FULL headless sim (check-sim.sh's exact
# TU list — keep in sync), the fdlibm sweep (oracle/fdlibm-crosscheck)
# and the format differential (calib/fmt_diff.c) as STATIC armv7 binaries
# (SDK gcc, -O2 -ffp-contract=off), pushes them over ADB, and runs ON THE
# DEVICE:
#   [4] the ~257k-input fdlibm sweep      -> cmp vs the host-C output
#       (which the standing M0 gate proves == JS == browser)
#   [5] the exact-math family sweep (mathsweep.c: floor/ceil/sqrt/fabs/
#       fmod/js_round — the sim's whole non-transcendental libm surface)
#       -> cmp vs the HOST-LIBM ANCHOR build (mathsweep_host is
#       deliberately NOT linked with fdlibm.c, so the device's fdlibm.c
#       floor/ceil strong overrides are proven equal to a known-good
#       libm, never merely to themselves). Background: the SDK's static
#       musl libc.a floor/ceil/round are BROKEN (identity for
#       non-integers — measured iter 38); fdlibm.c carries the fix.
#   [6] fmt_diff --self-test, then the FULL adversarial format corpus:
#       GENERATED on the device (cmp vs the host corpus, whose sha is the
#       frozen expected-format.json pin) and formatted on the device
#       (cmp vs the host output, which check-format.sh proves == V8/oracle)
#   [7] the full g01 golden replay        -> pulled and judged by the
#       UNCHANGED oracle/harness/verify-stream.js against the frozen
#       g01 stream (exact per-frame equality, full length, rng pins)
#
# ALL judgment happens on the host; the device never self-reports.
# Device hygiene: writes only under /tmp/mlfk + /mnt/mlfk-scratch, both
# removed on exit (trap). Exact equality only — an armv7 divergence is a
# REAL finding (ledger + class fix), never an epsilon.
#
# Env: FUNKEY_ADB_ID (device id), MLFK_FORCE_ARM=1 (ignore build stamp).
# Exclusive host lock: ${TMPDIR:-/tmp}/mlfk-rig-<device-id>.lock — SHARED
# across checkouts/worktrees (the device is the shared resource); an
# existing lock is a loud death, removed only by hand.
# Prints DEVICE CONFORMS g01, exit 0.
set -euo pipefail
cd "$(dirname "$0")/../../.."

CAL=port/sim/calib
BUILD=$CAL/build
DEVB=$BUILD/device
SIM=port/sim/sim
TABLES=pipeline/build/sim-tables
FDC=oracle/fdlibm-crosscheck
DTMP=/tmp/mlfk
DSD=/mnt/mlfk-scratch
mkdir -p "$DEVB"

source port/sim/device/adbsh.sh # (also defines $DEV — it keys the lock)

# exclusive rig lock (iter 41, review rounds 1-3 recurring — the class
# is closed by REMOVING the cleverness): ONE mkdir-atomic lock at a
# SHARED host path keyed by the DEVICE id — the device is the shared
# resource, so every checkout/worktree on this host contends on the
# SAME lock — and NO reclamation logic at all: no pid files, no
# liveness probes, no auto-delete. An existing lock is a loud death
# telling the operator to remove it by hand; a stranded lock (crash in
# the instant between mkdir and trap install) costs one manual rm -rf,
# which the message spells out. The release trap is installed only
# AFTER acquisition, so a losing contender can never release the
# winner's lock.
LOCK="${TMPDIR:-/tmp}/mlfk-rig-${DEV}.lock"
if ! mkdir "$LOCK" 2>/dev/null; then
  lockage="unknown"
  if lockmtime="$(stat -f %m "$LOCK" 2>/dev/null || stat -c %Y "$LOCK" 2>/dev/null)"; then
    lockage="$(( $(date +%s) - lockmtime )) s"
  fi
  echo "DEVICE FAIL: rig lock $LOCK already exists (age: $lockage)." >&2
  echo "  Another rig run may be using device $DEV right now. If you" >&2
  echo "  are sure no rig is running, remove it manually: rm -rf '$LOCK'" >&2
  exit 1
fi

# hygiene: device scratch never outlives the script. Best-effort BY
# DESIGN (cleanup must never mask the run's real exit code) but routed
# through dsh and VISIBLE when it fails (iter 39, review L2).
rig_cleanup() {
  dsh "rm -rf $DTMP $DSD" >/dev/null 2>&1 \
    || echo "WARN: device scratch cleanup failed — $DTMP $DSD may remain on the device" >&2
  rm -rf "$LOCK" 2>/dev/null \
    || echo "WARN: could not release rig lock $LOCK — remove manually: rm -rf '$LOCK'" >&2
}
trap rig_cleanup EXIT

require_device

# device-side hash tool self-test (iter 39, review M2): every pull below
# is digest-verified via busybox sha256sum on the device — prove the tool
# exists and is correct (empty-input vector) before trusting it.
EMPTY_SHA=e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855
devsha="$(dsh "printf '' | sha256sum" | awk 'NF{print $1; exit}')"
if [ "$devsha" != "$EMPTY_SHA" ]; then
  echo "DEVICE FAIL: device sha256sum missing/broken (got '$devsha')" >&2
  exit 1
fi

# pullv <device-path> <host-dest> — freshness-proven pull (iter 39,
# review M2): the host destination is removed BEFORE the pull (a stale
# file can never be judged), and the device-side sha256 of the source
# must equal the host sha256 of the pulled bytes.
pullv() {
  local dsum hsum
  rm -f "$2"
  adb -s "$DEV" pull "$1" "$2" >/dev/null
  dsum="$(dsh "sha256sum $1" | awk 'NF{print $1; exit}')"
  if [ -z "$dsum" ] || [ "${#dsum}" -ne 64 ]; then
    echo "DEVICE FAIL: no device-side sha256 for $1 (got '$dsum')" >&2
    return 1
  fi
  case "$dsum" in
    *[!0-9a-f]*)
      echo "DEVICE FAIL: bad device-side sha256 for $1 ('$dsum')" >&2
      return 1
      ;;
  esac
  hsum="$(shasum -a 256 "$2" | cut -d' ' -f1)"
  if [ "$dsum" != "$hsum" ]; then
    echo "DEVICE FAIL: pulled $2 != device $1 (device $dsum, host $hsum)" >&2
    return 1
  fi
}

echo "== [1/7] host data plane (M1 tables + SIMDATA1 + g01 trace text) =="
# g01 match params — SINGLE SOURCE: the goldens manifest. Extracted HERE,
# before any device work, with explicit failure checks (iter 39, review
# M4: eval "$(node …)" swallowed a node failure; set -u alone can't catch
# inherited/partial values, so unset first). NO EVAL (iter 40, review M4
# round 2: manifest values are data, never shell text) — parsed
# line-by-line into variables under strict per-field validation; any
# unexpected key, malformed value, or out-of-range param is a loud death.
unset name seed p1 p2 stage frames
gparams="$(node -e "
  const m=require('./oracle/goldens/manifest.json');
  const g=m.goldens.find(x=>x.id==='g01');
  if(!g) throw new Error('g01 missing from manifest');
  console.log('name='+g.name);
  console.log('seed='+g.seed);
  console.log('p1='+g.p1); console.log('p2='+g.p2);
  console.log('stage='+g.stage);
  console.log('frames='+g.frames);
")" || { echo "DEVICE FAIL: g01 manifest param extraction failed" >&2; exit 1; }
if [ -z "$gparams" ]; then
  echo "DEVICE FAIL: g01 manifest param extraction returned nothing" >&2
  exit 1
fi
while IFS='=' read -r gk gv; do
  case "$gk" in
    name)
      if ! [[ "$gv" =~ ^[a-z0-9][a-z0-9-]*$ ]]; then
        echo "DEVICE FAIL: manifest g01.name fails validation ('$gv')" >&2
        exit 1
      fi
      name=$gv
      ;;
    seed|p1|p2|stage|frames)
      if ! [[ "$gv" =~ ^[0-9]+$ ]]; then
        echo "DEVICE FAIL: manifest g01.$gk not a decimal integer ('$gv')" >&2
        exit 1
      fi
      printf -v "$gk" '%s' "$gv"
      ;;
    *)
      echo "DEVICE FAIL: unexpected manifest extraction line '$gk=$gv'" >&2
      exit 1
      ;;
  esac
done <<< "$gparams"
: "$name" "$seed" "$p1" "$p2" "$stage" "$frames" # set -u backstop: all six present
# domain sanity (manifest is trusted-but-verified data)
[ "$frames" -le 5000 ] || { echo "DEVICE FAIL: g01 frames $frames > 5000" >&2; exit 1; }
[ "$stage" -le 5 ] || { echo "DEVICE FAIL: g01 stage $stage > 5" >&2; exit 1; }
[ "$p1" -le 4 ] || { echo "DEVICE FAIL: g01 p1 $p1 > 4" >&2; exit 1; }
[ "$p2" -le 4 ] || { echo "DEVICE FAIL: g01 p2 $p2 > 4" >&2; exit 1; }
bash pipeline/extractor/build-extractor.sh
node pipeline/run.js --only animations,tables,stages --out "$TABLES"
test -f "$TABLES/ml_tables.c"
test -f "$TABLES/ml_stages.c"
node "$CAL/dump-sim-data.js" --out "$DEVB/simdata.txt"
node "$SIM/trace-to-txt.js" \
  oracle/goldens/g01-fox-marth-battlefield.trace.json "$DEVB/g01.trace.txt"

echo "== [2/7] host references (fdlibm sweep + libm anchor + pinned format corpus) =="
# FROZEN corpus size (iter 40, review M3 round 2): the expected line
# count is a LITERAL measured-then-frozen from gen-inputs.js
# (2026-07-16: 257,287 lines — deterministic generator), never
# re-derived from the freshly generated file, so a generator regression
# can no longer satisfy its own trailer checks. The generated corpus
# AND both sweep legs' `n` trailers are asserted against this literal.
CORPUS_LINES=257287
# CORPUS IDENTITY pin (iter 41, review M round 3): the line count pins
# CARDINALITY, not identity — a generator regression repeating one
# valid operand the right number of times passes the count, the
# grammar, and the host==device comparison while losing coverage. The
# corpus is deterministic (measured ×2 byte-identical, 2026-07-16), so
# its sha256 is a measured-then-frozen LITERAL and the freshly
# generated bytes must match BEFORE any use. NOTE: the fdlibm sweep
# [4] and the exact-math sweep [5] consume this ONE generated file, so
# this single literal is the identity pin for BOTH sweeps' corpora
# (the format corpus already carries its own frozen pin via
# check-format-pins.js / expected-format.json).
CORPUS_SHA256=b164802a98932c2c8780febfe2c857d5771d12a327d3465291221861da6b3d05
node "$FDC/gen-inputs.js" "$DEVB/fdlibm-inputs.txt"
corpuslines="$(wc -l < "$DEVB/fdlibm-inputs.txt" | tr -d ' ')"
if [ "$corpuslines" != "$CORPUS_LINES" ]; then
  echo "DEVICE FAIL: generated corpus has $corpuslines lines, frozen pin is $CORPUS_LINES" >&2
  exit 1
fi
corpussha="$(shasum -a 256 "$DEVB/fdlibm-inputs.txt" | cut -d' ' -f1)"
if [ "$corpussha" != "$CORPUS_SHA256" ]; then
  echo "DEVICE FAIL: generated corpus sha256 $corpussha != frozen pin $CORPUS_SHA256" >&2
  exit 1
fi
cc -O2 -ffp-contract=off -std=c99 -Wall -Iport/fdlibm \
  "$FDC/csweep.c" port/fdlibm/fdlibm.c -o "$DEVB/csweep_host"
"$DEVB/csweep_host" "$DEVB/fdlibm-inputs.txt" > "$DEVB/fdlibm-host.txt"
# HOST-LIBM ANCHOR: no fdlibm.c in this link (see header note [5])
cc -O2 -ffp-contract=off -Wall -Wextra -Werror -Iport/sim \
  port/sim/device/mathsweep.c -o "$DEVB/mathsweep_host" -lm
"$DEVB/mathsweep_host" "$DEVB/fdlibm-inputs.txt" > "$DEVB/mathsweep-host.txt"
# review M3: the sweep must have consumed the WHOLE corpus (host leg;
# the device leg is asserted in step [5] after the pull) — judged
# against the FROZEN literal, never the regenerated file
tail -1 "$DEVB/mathsweep-host.txt" | grep -qx "n $CORPUS_LINES" || {
  echo "DEVICE FAIL: host mathsweep trailer != frozen corpus pin ($CORPUS_LINES)" >&2
  exit 1
}
cc -O2 -ffp-contract=off -Wall -Wextra -Werror \
  -Iport/ryu -Iport/sim -Ioracle/qjs \
  -o "$DEVB/fmt_diff_host" \
  "$CAL/fmt_diff.c" "$CAL/canon.c" port/sim/ml_ser.c port/sim/ml_fmt.c \
  oracle/qjs/sha256.c -lm
"$DEVB/fmt_diff_host" --gen "$DEVB/fmt-adv.hex"
node "$CAL/check-format-pins.js" adversarial "$DEVB/fmt-adv.hex"
"$DEVB/fmt_diff_host" --format "$DEVB/fmt-adv.hex" "$DEVB/fmt-adv.host.txt"

echo "== [3/7] armv7 static cross-build (SDK gcc; stamp-cached) =="
# Stamp inputs (iter 39, review H2): sources + generated tables + THIS
# SCRIPT'S OWN BYTES (the build recipe/flags live in the heredoc below)
# + adbsh.sh + the docker image Id. The stamp also records each produced
# binary's sha256; the cache-HIT path re-hashes the cached binaries and
# any mismatch forces a rebuild — a stamped-but-tampered binary is never
# judged.
ARMIMG=jondbell/funkey-s-sdk
ARMIMGID="$(docker image inspect -f '{{.Id}}' "$ARMIMG" 2>/dev/null)" || {
  echo "   docker image $ARMIMG not local — pulling"
  docker pull "$ARMIMG" >/dev/null
  ARMIMGID="$(docker image inspect -f '{{.Id}}' "$ARMIMG")"
}
ARMBINS="sim_device csweep_arm fmt_diff_arm mathsweep_arm"
srchash() {
  # review M1: this runs inside $() where errexit is cleared — pipefail +
  # explicit stage checks + a minimum-file-count floor, so a failed find
  # can never yield a partial-but-plausible hash. Round 3 (iter 41,
  # review M rounds 2-3): find runs with -L so symlinked DIRECTORIES are
  # descended (the compiler globs follow them — a symlinked moves/
  # subtree must be hashed) and file symlinks are hashed THROUGH the
  # link. Under -L, `-type f` matches regular files plus resolvable
  # file links, and `-type l` matches ONLY broken links — so the
  # predicate splits: a SEPARATE broken-link scan over the same roots
  # dies loudly on ANY broken link (never a silent skip), then the
  # `-type f` pass builds the hash list. The list stays -print0/NUL-
  # framed end to end (a newline in a filename can never split one
  # record into two).
  set -o pipefail
  local listf brokenf n
  listf="$DEVB/.srclist.$$"
  brokenf="$DEVB/.srcbroken.$$"
  find -L port/sim port/fdlibm port/ryu oracle/qjs "$FDC/csweep.c" \
    -type l -print0 > "$brokenf" || {
    echo "DEVICE FAIL: srchash: broken-link scan failed" >&2
    rm -f "$brokenf"
    return 1
  }
  if [ -s "$brokenf" ]; then
    echo "DEVICE FAIL: srchash: broken symlink(s) in the source tree:" >&2
    tr '\0' '\n' < "$brokenf" >&2
    rm -f "$brokenf"
    return 1
  fi
  rm -f "$brokenf"
  find -L port/sim port/fdlibm port/ryu oracle/qjs "$FDC/csweep.c" \
    -type f \( -name '*.c' -o -name '*.h' \) -print0 \
    > "$listf" || {
    echo "DEVICE FAIL: srchash: find failed" >&2
    rm -f "$listf"
    return 1
  }
  # round 3 (iter 41, review L): the count pipeline gets an explicit
  # status check (pipefail is set above and $() inherits it) plus an
  # empty/non-numeric guard — errexit is cleared in here, and a bare
  # [ -lt ] on a garbage value errors and reads as "condition false".
  n="$(tr -cd '\0' < "$listf" | wc -c | tr -d ' ')" || {
    echo "DEVICE FAIL: srchash: file-count pipeline failed" >&2
    rm -f "$listf"
    return 1
  }
  case "$n" in
    ''|*[!0-9]*)
      echo "DEVICE FAIL: srchash: non-numeric source-file count ('$n')" >&2
      rm -f "$listf"
      return 1
      ;;
  esac
  if [ "$n" -lt 450 ]; then
    echo "DEVICE FAIL: srchash: only $n source files found (>= 450 expected)" >&2
    rm -f "$listf"
    return 1
  fi
  {
    LC_ALL=C sort -z < "$listf" | xargs -0 shasum -a 256 || exit 1
    shasum -a 256 "$TABLES/ml_tables.c" "$TABLES/ml_stages.c" \
      port/sim/device/check-device-g01.sh port/sim/device/adbsh.sh || exit 1
    printf 'dockerimage %s\n' "$ARMIMGID"
  } | shasum -a 256 | cut -d' ' -f1 || {
    rm -f "$listf"
    return 1
  }
  rm -f "$listf"
}
STAMP=$DEVB/arm-build.stamp
want="$(srchash)"
stamp_ok() {
  [ -f "$STAMP" ] || return 1
  [ "$(sed -n '1s/^srchash=//p' "$STAMP")" = "$want" ] || return 1
  local f rec cur
  for f in $ARMBINS; do
    [ -f "$DEVB/$f" ] || return 1
    rec="$(awk -v f="$f" '$1=="bin" && $2==f {print $3}' "$STAMP")"
    [ -n "$rec" ] || return 1
    cur="$(shasum -a 256 "$DEVB/$f" | cut -d' ' -f1)"
    if [ "$cur" != "$rec" ]; then
      echo "   cached $f sha256 != stamp record — forcing rebuild" >&2
      return 1
    fi
  done
  return 0
}
if [ "${MLFK_FORCE_ARM:-0}" != 0 ] || ! stamp_ok; then
  rm -f "$STAMP"
  echo "   stamp MISS (or forced) — rebuilding armv7 binaries"
  # SERIAL docker only (CLAUDE.md §Commands arm32 recipe). Run by the
  # RESOLVED image Id — the same Id the stamp records — never the
  # mutable tag (iter 40, review M2 round 2: closes the inspect-to-run
  # tag-drift window).
  docker run --rm -v "$PWD":/work -w /work "$ARMIMGID" bash -lc '
    set -e
    export PATH=/opt/FunKey-sdk-2.3.0/bin:$PATH
    CC=arm-funkey-linux-musleabihf-gcc
    DEVB=port/sim/calib/build/device
    TABLES=pipeline/build/sim-tables
    CAL=port/sim/calib
    SIM=port/sim/sim
    # keep this TU list in sync with port/sim/check-sim.sh (the M2 gate)
    $CC -O2 -ffp-contract=off -Wall -Wextra -Werror -static \
      -I"$TABLES" -Iport/ryu -Iport/sim -Ioracle/qjs \
      -o "$DEVB/sim_device" \
      "$SIM/sim_main.c" "$SIM/sim_boot.c" "$SIM/sim_tick.c" \
      "$SIM/sim_ser.c" "$SIM/sim_data.c" \
      "$CAL/canon.c" "$CAL/player_canon.c" \
      port/sim/physics.c port/sim/interpolated_collision.c \
      port/sim/environmental_collision.c port/sim/hit_detection.c \
      port/sim/article.c port/sim/action_state_shortcuts.c \
      port/sim/ml_events.c port/sim/ml_fmt.c port/sim/ml_ser.c \
      port/sim/ai_bridge.c port/sim/input/interpret_inputs.c \
      port/sim/stages/moving_platforms.c port/sim/stages/ystory.c \
      port/sim/stages/fountain.c \
      port/sim/characters/shared/moves_index.c \
      port/sim/characters/shared/moves/*.c \
      port/sim/characters/fox/moves_index.c \
      port/sim/characters/fox/moves/*.c \
      port/sim/characters/falco/moves_index.c \
      port/sim/characters/falco/moves/*.c \
      port/sim/characters/falcon/moves_index.c \
      port/sim/characters/falcon/moves/*.c \
      port/sim/characters/marth/moves_index.c \
      port/sim/characters/marth/dancing_blade_combo.c \
      port/sim/characters/marth/dancing_blade_air_mobility.c \
      port/sim/characters/marth/moves/*.c \
      port/sim/characters/puff/moves_index.c \
      port/sim/characters/puff/puff_multi_jump_drift.c \
      port/sim/characters/puff/puff_next_jump.c \
      port/sim/characters/puff/moves/*.c \
      "$TABLES/ml_tables.c" "$TABLES/ml_stages.c" \
      oracle/qjs/sha256.c \
      port/fdlibm/fdlibm.c -lm
    $CC -O2 -ffp-contract=off -std=c99 -Wall -static -Iport/fdlibm \
      oracle/fdlibm-crosscheck/csweep.c port/fdlibm/fdlibm.c \
      -o "$DEVB/csweep_arm"
    # fdlibm.c linked (exactly like sim_device): floor/ceil overrides live
    $CC -O2 -ffp-contract=off -Wall -Wextra -Werror -static -Iport/sim \
      port/sim/device/mathsweep.c port/fdlibm/fdlibm.c \
      -o "$DEVB/mathsweep_arm" -lm
    $CC -O2 -ffp-contract=off -Wall -Wextra -Werror -static \
      -Iport/ryu -Iport/sim -Ioracle/qjs \
      -o "$DEVB/fmt_diff_arm" \
      "$CAL/fmt_diff.c" "$CAL/canon.c" port/sim/ml_ser.c port/sim/ml_fmt.c \
      oracle/qjs/sha256.c -lm
  '
  for f in $ARMBINS; do
    file "$DEVB/$f" | grep -q "ELF 32-bit LSB executable, ARM" || {
      echo "DEVICE FAIL: $f is not an armv7 static executable" >&2
      exit 1
    }
  done
  # H4 residue (iter 39): the fdlibm strong overrides must BE the linked
  # definitions in the device sim — exactly one T symbol each. Round 2
  # (iter 40, review L1): nm's exit status is checked explicitly (its
  # output goes to a file first — no || true, no grep in the pipeline;
  # a truncated symbol table can never pass on a lucky prefix).
  nmout="$DEVB/.nm-sim_device.$$"
  if ! nm "$DEVB/sim_device" > "$nmout"; then
    rm -f "$nmout"
    echo "DEVICE FAIL: nm failed on sim_device — cannot verify fdlibm overrides" >&2
    exit 1
  fi
  for s in floor ceil fmod; do
    cnt="$(awk -v s="$s" '$2=="T" && $3==s {n++} END {print n+0}' "$nmout")"
    if [ "$cnt" != 1 ]; then
      echo "DEVICE FAIL: expected exactly 1 T definition of $s in sim_device, found $cnt" >&2
      exit 1
    fi
  done
  rm -f "$nmout"
  {
    printf 'srchash=%s\n' "$want"
    for f in $ARMBINS; do
      printf 'bin %s %s\n' "$f" "$(shasum -a 256 "$DEVB/$f" | cut -d' ' -f1)"
    done
  } > "$STAMP"
  echo "   arm binaries rebuilt (stamp $want)"
else
  echo "   arm binaries up to date (stamp $want; cached binaries sha-verified)"
fi

echo "== [4/7] device: fdlibm sweep (armv7 vs host, byte-exact) =="
# rehash ADJACENT to the push (iter 40, review M2 round 2): every binary
# is re-verified against the stamp's recorded sha256 immediately before
# it leaves the host — nothing can mutate between the stamp check in
# step [3] and the push.
for f in $ARMBINS; do
  rec="$(awk -v f="$f" '$1=="bin" && $2==f {print $3}' "$STAMP")"
  if [ -z "$rec" ]; then
    echo "DEVICE FAIL: no stamp sha256 record for $f — refusing to push" >&2
    exit 1
  fi
  cur="$(shasum -a 256 "$DEVB/$f" | cut -d' ' -f1)"
  if [ "$cur" != "$rec" ]; then
    echo "DEVICE FAIL: $f changed between stamp verification and push ($cur != $rec)" >&2
    exit 1
  fi
done
dsh "rm -rf $DTMP $DSD && mkdir -p $DTMP $DSD"
adb -s "$DEV" push \
  "$DEVB/csweep_arm" "$DEVB/fmt_diff_arm" "$DEVB/mathsweep_arm" \
  "$DEVB/sim_device" \
  "$DEVB/fdlibm-inputs.txt" "$DEVB/g01.trace.txt" "$DEVB/simdata.txt" \
  "$DTMP/" >/dev/null
# PUSH PROVENANCE (iter 41, review M round 3): the pre-push rehash
# proves what the HOST held; this proves what the DEVICE received —
# device-side sha256 (nonce dsh) of every pushed binary must equal the
# stamp's record BEFORE anything runs. This is the only observable
# edge of the concurrent-mutator TOCTOU class (dispositioned iter 40):
# whatever happened host-side, the bytes the device executes are now
# bound to the stamp that was sha-verified against the sources.
devsums="$(dsh "cd $DTMP && sha256sum $ARMBINS")" || {
  echo "DEVICE FAIL: device-side sha256 of pushed binaries failed" >&2
  exit 1
}
for f in $ARMBINS; do
  rec="$(awk -v f="$f" '$1=="bin" && $2==f {print $3}' "$STAMP")"
  if [ -z "$rec" ]; then
    echo "DEVICE FAIL: no stamp sha256 record for $f — cannot verify device copy" >&2
    exit 1
  fi
  dsum="$(printf '%s\n' "$devsums" | awk -v f="$f" '$2==f {print $1; exit}')"
  if [ -z "$dsum" ] || [ "${#dsum}" -ne 64 ]; then
    echo "DEVICE FAIL: no device-side sha256 for pushed $f (got '$dsum')" >&2
    exit 1
  fi
  case "$dsum" in
    *[!0-9a-f]*)
      echo "DEVICE FAIL: bad device-side sha256 for pushed $f ('$dsum')" >&2
      exit 1
      ;;
  esac
  if [ "$dsum" != "$rec" ]; then
    echo "DEVICE FAIL: device-side $f sha256 != stamp record (device $dsum, stamp $rec) — refusing to run it" >&2
    exit 1
  fi
done
echo "   push provenance: all $(echo "$ARMBINS" | wc -w | tr -d ' ') device-side binaries match the stamp"
dsh "chmod +x $DTMP/csweep_arm $DTMP/fmt_diff_arm $DTMP/mathsweep_arm $DTMP/sim_device"
dsh "sh -lc '$DTMP/csweep_arm $DTMP/fdlibm-inputs.txt > $DTMP/fdlibm-device.txt'"
pullv "$DTMP/fdlibm-device.txt" "$DEVB/fdlibm-device.txt"
cmp "$DEVB/fdlibm-host.txt" "$DEVB/fdlibm-device.txt"
echo "   fdlibm sweep: device == host ($(wc -l < "$DEVB/fdlibm-device.txt" | tr -d ' ') lines)"
dsh "rm -f $DTMP/fdlibm-device.txt"

echo "== [5/7] device: exact-math family sweep (floor/ceil/sqrt/fabs/fmod/js_round) =="
dsh "sh -lc '$DTMP/mathsweep_arm $DTMP/fdlibm-inputs.txt > $DSD/mathsweep-device.txt'"
pullv "$DSD/mathsweep-device.txt" "$DEVB/mathsweep-device.txt"
# review M3, device leg: the sweep must have consumed the WHOLE corpus
# (judged against the FROZEN literal, never the regenerated file)
tail -1 "$DEVB/mathsweep-device.txt" | grep -qx "n $CORPUS_LINES" || {
  echo "DEVICE FAIL: device mathsweep trailer != frozen corpus pin ($CORPUS_LINES)" >&2
  exit 1
}
cmp "$DEVB/mathsweep-host.txt" "$DEVB/mathsweep-device.txt"
echo "   exact-math sweep: device(fdlibm overrides) == host libm anchor ($(wc -l < "$DEVB/mathsweep-device.txt" | tr -d ' ') lines)"
dsh "rm -f $DTMP/fdlibm-inputs.txt $DSD/mathsweep-device.txt"
rm -f "$DEVB/mathsweep-device.txt" # byte-dup of the kept host anchor

echo "== [6/7] device: formatter self-test + FULL adversarial corpus =="
dsh "sh -lc '$DTMP/fmt_diff_arm --self-test'"
dsh "sh -lc '$DTMP/fmt_diff_arm --gen $DSD/fmt-adv.hex'"
pullv "$DSD/fmt-adv.hex" "$DEVB/fmt-adv.device.hex"
cmp "$DEVB/fmt-adv.hex" "$DEVB/fmt-adv.device.hex"
echo "   corpus generator: device == host (the expected-format.json pinned corpus)"
dsh "sh -lc '$DTMP/fmt_diff_arm --format $DSD/fmt-adv.hex $DSD/fmt-adv.out.txt'"
pullv "$DSD/fmt-adv.out.txt" "$DEVB/fmt-adv.device.txt"
cmp "$DEVB/fmt-adv.host.txt" "$DEVB/fmt-adv.device.txt"
echo "   adversarial format sweep: device == host ($(wc -l < "$DEVB/fmt-adv.device.txt" | tr -d ' ') lines)"
dsh "rm -rf $DSD"
rm -f "$DEVB/fmt-adv.device.hex" "$DEVB/fmt-adv.device.txt" # byte-dups of the kept host copies

echo "== [7/7] device: g01 golden replay, judged by the frozen stream =="
# (match params extracted + validated in step [1] — single source, fail-loud)
t0=$(date +%s)
dsh "sh -lc '$DTMP/sim_device --trace $DTMP/g01.trace.txt --simdata $DTMP/simdata.txt --seed $seed --p1 $p1 --p2 $p2 --stage $stage --frames $frames > $DTMP/g01.sim-out.txt'"
t1=$(date +%s)
pullv "$DTMP/g01.sim-out.txt" "$DEVB/g01.sim-out.device.txt"
node "$SIM/wrap-run.js" g01 "$DEVB/g01.sim-out.device.txt" \
  "$DEVB/g01.sim-run.device.json"
node oracle/harness/verify-stream.js "$DEVB/g01.sim-run.device.json" \
  "oracle/goldens/$name.sha256.json"
echo "   device sim wall clock: $((t1-t0)) s for $frames frames (informational)"
dsh "rm -rf $DTMP"

# no-commit guard: build output is never tracked (iter 39, review L1:
# a git ERROR must be loud, never read as clean output). Round 2 (iter
# 40, review L2): --untracked-files=all overrides any
# status.showUntrackedFiles=no config, and `git ls-files` proves
# untracked-ness DIRECTLY — already-tracked-but-clean build output can
# no longer read as clean porcelain.
gitout="$(git status --porcelain --untracked-files=all -- "$BUILD" "$TABLES")" || {
  echo "DEVICE FAIL: git status failed — cannot prove build output is untracked" >&2
  exit 1
}
if [ -n "$gitout" ]; then
  echo "DEVICE FAIL: build output not gitignored:" >&2
  printf '%s\n' "$gitout" >&2
  exit 1
fi
lsout="$(git ls-files -- "$BUILD" "$TABLES")" || {
  echo "DEVICE FAIL: git ls-files failed — cannot prove build output is untracked" >&2
  exit 1
}
if [ -n "$lsout" ]; then
  echo "DEVICE FAIL: build output is TRACKED by git:" >&2
  printf '%s\n' "$lsout" >&2
  exit 1
fi

echo "DEVICE CONFORMS g01"
