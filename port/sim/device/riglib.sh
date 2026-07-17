# port/sim/device/riglib.sh — SHARED plumbing for the M3 device rig.
#
# Extracted VERBATIM from check-device-g01.sh after its Tier-A review arc
# closed at VERDICT: GO (.loop/review-42-1.log; iters 38-42) — the block
# comments carry that arc's provenance. This file is the "inheritance
# package" (STATE.md, arc closure): every M3 device check script sources
# it and gets the reviewed plumbing as a CLASS, never a copy-paste.
#
# Contract: source port/sim/device/adbsh.sh FIRST ($DEV, dsh,
# require_device), then this file. Callers must set before use:
#   DEVB   — host build dir (port/sim/calib/build/device)
#   TABLES — generated-tables dir (pipeline/build/sim-tables)
#   FDC    — oracle/fdlibm-crosscheck (srchash input: csweep.c)
#   DTMP DSD — device scratch dirs (/tmp/mlfk, /mnt/mlfk-scratch)
# and call the rig_* functions from a `set -euo pipefail` script running
# at the repo root.
#
# RIG_SCRIPTS — every script whose bytes are ARM-BUILD STAMP INPUT (the
# build recipe/flags live in rig_arm_build's heredoc; a script that
# drives the rig can change what "current" means, so its bytes key the
# cache). ANY new device check script MUST add itself here — both so its
# own edits force a rebuild and so ALL rig scripts always compute the
# SAME stamp (one shared build, no ping-pong between scripts).
RIG_SCRIPTS="port/sim/device/adbsh.sh port/sim/device/riglib.sh \
port/sim/device/check-device-g01.sh port/sim/device/check-device-conform.sh \
port/gfx/check-device-render.sh"

# The armv7 binaries the shared build produces (one docker run).
# gfx_device (M3 task 4) is the SDL1.2 render app: DYNAMICALLY linked
# against the sysroot's libSDL-1.2 (LGPL — dynamic only, CLAUDE.md
# licensing rule; rig_arm_build asserts it), raster TU at -O3 (the ONE
# -O3 TU), everything else -O2; -ffp-contract=off on every TU.
ARMBINS="sim_device csweep_arm fmt_diff_arm mathsweep_arm gfx_device"

# rig_lock_acquire — exclusive rig lock (iter 41, review rounds 1-3
# recurring — the class is closed by REMOVING the cleverness): ONE
# mkdir-atomic lock at a SHARED host path keyed by the DEVICE id — the
# device is the shared resource, so every checkout/worktree on this host
# contends on the SAME lock — and NO reclamation logic at all: no pid
# files, no liveness probes, no auto-delete. An existing lock is a loud
# death telling the operator to remove it by hand; a stranded lock
# (crash in the instant between mkdir and trap install) costs one manual
# rm -rf, which the message spells out. The release trap (rig_cleanup)
# is installed by the CALLER only AFTER acquisition, so a losing
# contender can never release the winner's lock.
rig_lock_acquire() {
  LOCK="${TMPDIR:-/tmp}/mlfk-rig-${DEV}.lock"
  if ! mkdir "$LOCK" 2>/dev/null; then
    local lockage lockmtime
    lockage="unknown"
    if lockmtime="$(stat -f %m "$LOCK" 2>/dev/null || stat -c %Y "$LOCK" 2>/dev/null)"; then
      lockage="$(( $(date +%s) - lockmtime )) s"
    fi
    echo "DEVICE FAIL: rig lock $LOCK already exists (age: $lockage)." >&2
    echo "  Another rig run may be using device $DEV right now. If you" >&2
    echo "  are sure no rig is running, remove it manually: rm -rf '$LOCK'" >&2
    exit 1
  fi
}

# rig_cleanup — hygiene: device scratch never outlives the script.
# Best-effort BY DESIGN (cleanup must never mask the run's real exit
# code) but routed through dsh and VISIBLE when it fails (iter 39,
# review L2). Callers install it: trap rig_cleanup EXIT (AFTER
# rig_lock_acquire).
rig_cleanup() {
  dsh "rm -rf $DTMP $DSD" >/dev/null 2>&1 \
    || echo "WARN: device scratch cleanup failed — $DTMP $DSD may remain on the device" >&2
  rm -rf "$LOCK" 2>/dev/null \
    || echo "WARN: could not release rig lock $LOCK — remove manually: rm -rf '$LOCK'" >&2
}

# rig_devsha_selftest — device-side hash tool self-test (iter 39, review
# M2): every pull is digest-verified via busybox sha256sum on the device
# — prove the tool exists and is correct (empty-input vector) before
# trusting it.
rig_devsha_selftest() {
  local EMPTY_SHA devsha
  EMPTY_SHA=e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855
  devsha="$(dsh "printf '' | sha256sum" | awk 'NF{print $1; exit}')"
  if [ "$devsha" != "$EMPTY_SHA" ]; then
    echo "DEVICE FAIL: device sha256sum missing/broken (got '$devsha')" >&2
    exit 1
  fi
}

# pullv <device-path> <host-dest> — freshness-proven pull (iter 39,
# review M2): the host destination is removed BEFORE the pull (a stale
# file can never be judged), and the device-side sha256 of the source
# must equal the host sha256 of the pulled bytes. Non-empty assert
# (iter 42): closes the both-sides-empty cmp pass.
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
  if ! [ -s "$2" ]; then
    echo "DEVICE FAIL: pulled $2 is empty (device produced no output)" >&2
    return 1
  fi
}

# made <file...> — freshness assert (iter 42, review round 4 — the
# rm-before-produce CLASS, round-2's rm-before-pull generalized to ALL
# produced artifacts): every host-side file a judge/comparison later
# consumes is `rm -f`'d immediately before its producer runs and
# asserted to exist NON-EMPTY immediately after, so a fallible producer
# exiting 0 without writing (node script, generator, no-op redirection)
# can never leave a PRIOR run's file — even a byte-identical one that
# would satisfy a content pin — to be judged. Content pins prove
# CONTENT; only rm-before-produce proves FRESHNESS.
made() {
  local f
  for f in "$@"; do
    if ! [ -s "$f" ]; then
      echo "DEVICE FAIL: artifact $f missing or empty after its producer ran (rm-before-produce freshness guard)" >&2
      exit 1
    fi
  done
}

# rig_srchash — the arm-build stamp key. Inputs (iter 39, review H2):
# sources + generated tables + THE RIG SCRIPTS' OWN BYTES (RIG_SCRIPTS —
# the build recipe/flags live in rig_arm_build's heredoc) + the docker
# image Id ($ARMIMGID, resolved by rig_arm_build before calling this).
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
rig_srchash() {
  set -o pipefail
  local listf brokenf n
  listf="$DEVB/.srclist.$$"
  brokenf="$DEVB/.srcbroken.$$"
  find -L port/sim port/gfx port/fdlibm port/ryu oracle/qjs "$FDC/csweep.c" \
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
  find -L port/sim port/gfx port/fdlibm port/ryu oracle/qjs "$FDC/csweep.c" \
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
    # shellcheck disable=SC2086 — RIG_SCRIPTS is a fixed space-separated list
    shasum -a 256 "$TABLES/ml_tables.c" "$TABLES/ml_stages.c" \
      $RIG_SCRIPTS || exit 1
    printf 'dockerimage %s\n' "$ARMIMGID"
  } | shasum -a 256 | cut -d' ' -f1 || {
    rm -f "$listf"
    return 1
  }
  rm -f "$listf"
}

# rig_stamp_ok — cache-HIT re-verify: the stamp's srchash line must match
# $want AND every recorded binary must still hash to its stamped sha256
# (a stamped-but-tampered binary is never judged).
rig_stamp_ok() {
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

# rig_arm_build — the SHARED armv7 static cross-build (SDK gcc; stamp-
# cached; MLFK_FORCE_ARM=1 ignores the stamp). Builds all of $ARMBINS in
# ONE serial docker run; asserts ELF/ARM type, the fdlibm strong-override
# link (exactly one T definition each of floor/ceil/fmod in sim_device —
# H4 residue, iter 39), and records each binary's sha256 in the stamp.
# Sets: ARMIMGID, want, STAMP.
rig_arm_build() {
  ARMIMG=jondbell/funkey-s-sdk
  ARMIMGID="$(docker image inspect -f '{{.Id}}' "$ARMIMG" 2>/dev/null)" || {
    echo "   docker image $ARMIMG not local — pulling"
    docker pull "$ARMIMG" >/dev/null
    ARMIMGID="$(docker image inspect -f '{{.Id}}' "$ARMIMG")"
  }
  STAMP=$DEVB/arm-build.stamp
  want="$(rig_srchash)"
  local f
  if [ "${MLFK_FORCE_ARM:-0}" != 0 ] || ! rig_stamp_ok; then
    rm -f "$STAMP"
    # freshness (round 4 class): remove the prior binaries before the
    # docker build — a build that exits 0 without compiling must never
    # re-stamp a PRIOR run's binaries as fresh
    for f in $ARMBINS; do rm -f "$DEVB/$f"; done
    rm -f "$DEVB/raster_arm.o" # gfx_device's -O3 intermediate (same class)
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
      # gfx_device (M3 task 4): the SDL1.2 live-render app. DYNAMIC link
      # (SDL 1.2 is LGPL — dynamic only; asserted after the build), -no-pie
      # for a deterministic file(1) signature + addr2line-able crashes.
      # raster.c is the ONE -O3 TU (PLAN 5); every TU -ffp-contract=off.
      SDLCFG=/opt/FunKey-sdk-2.3.0/arm-funkey-linux-musleabihf/sysroot/usr/bin/sdl-config
      GFX=port/gfx
      $CC -O3 -ffp-contract=off -Wall -Wextra -Werror \
        -I"$TABLES" -Iport/ryu -Iport/sim -Ioracle/qjs \
        -c "$GFX/raster.c" -o "$DEVB/raster_arm.o"
      $CC -O2 -ffp-contract=off -Wall -Wextra -Werror -no-pie \
        -I"$TABLES" -Iport/ryu -Iport/sim -Ioracle/qjs \
        $($SDLCFG --cflags) \
        -o "$DEVB/gfx_device" \
        "$DEVB/raster_arm.o" \
        "$GFX/gfx_app.c" "$GFX/platform_sdl1.c" \
        "$GFX/anim1.c" "$GFX/gfx_render.c" \
        "$SIM/sim_boot.c" "$SIM/sim_tick.c" "$SIM/sim_ser.c" \
        "$SIM/sim_data.c" \
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
        oracle/qjs/sha256.c port/fdlibm/fdlibm.c \
        $($SDLCFG --libs) -lm
    '
    for f in $ARMBINS; do
      made "$DEVB/$f"
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
    # M3 task 4: gfx_device runs the SAME sim, so the same overrides must
    # be its linked definitions too (dynamic libm.so is only consulted
    # for symbols the binary does not define — these it defines).
    local nmout s cnt b
    for b in sim_device gfx_device; do
      nmout="$DEVB/.nm-$b.$$"
      if ! nm "$DEVB/$b" > "$nmout"; then
        rm -f "$nmout"
        echo "DEVICE FAIL: nm failed on $b — cannot verify fdlibm overrides" >&2
        exit 1
      fi
      for s in floor ceil fmod; do
        cnt="$(awk -v s="$s" '$2=="T" && $3==s {n++} END {print n+0}' "$nmout")"
        if [ "$cnt" != 1 ]; then
          echo "DEVICE FAIL: expected exactly 1 T definition of $s in $b, found $cnt" >&2
          exit 1
        fi
      done
      rm -f "$nmout"
    done
    # LGPL compliance + launchability (M3 task 4): gfx_device must be
    # DYNAMICALLY linked against the shared libSDL-1.2 — file(1) says
    # "dynamically linked" AND the DT_NEEDED soname string is present in
    # the binary. A silent static libSDL.a link fails BOTH (no soname
    # string, "statically linked"), loudly.
    if ! file "$DEVB/gfx_device" | grep -q "dynamically linked"; then
      echo "DEVICE FAIL: gfx_device is not dynamically linked (SDL 1.2 is LGPL — dynamic only)" >&2
      exit 1
    fi
    if ! grep -q "libSDL-1.2.so.0" "$DEVB/gfx_device"; then
      echo "DEVICE FAIL: gfx_device carries no libSDL-1.2.so.0 NEEDED entry — SDL not dynamically linked" >&2
      exit 1
    fi
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
}

# rig_stamp_rehash <bin...> — rehash ADJACENT to the push (iter 40,
# review M2 round 2): every binary is re-verified against the stamp's
# recorded sha256 immediately before it leaves the host — nothing can
# mutate between the stamp check in rig_arm_build and the push.
rig_stamp_rehash() {
  local f rec cur
  for f in "$@"; do
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
}

# rig_push_provenance <device-dir> <bin...> — PUSH PROVENANCE (iter 41,
# review M round 3): the pre-push rehash proves what the HOST held; this
# proves what the DEVICE received — device-side sha256 (nonce dsh) of
# every pushed binary must equal the stamp's record BEFORE anything
# runs. This is the only observable edge of the concurrent-mutator
# TOCTOU class (dispositioned iter 40): whatever happened host-side, the
# bytes the device executes are now bound to the stamp that was
# sha-verified against the sources.
rig_push_provenance() {
  local ddir devsums f rec dsum
  ddir="$1"; shift
  devsums="$(dsh "cd $ddir && sha256sum $*")" || {
    echo "DEVICE FAIL: device-side sha256 of pushed binaries failed" >&2
    exit 1
  }
  for f in "$@"; do
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
  echo "   push provenance: all $# device-side binaries match the stamp"
}

# rig_no_commit_guard <path...> — build output is never tracked (iter 39,
# review L1: a git ERROR must be loud, never read as clean output).
# Round 2 (iter 40, review L2): --untracked-files=all overrides any
# status.showUntrackedFiles=no config, and `git ls-files` proves
# untracked-ness DIRECTLY — already-tracked-but-clean build output can
# no longer read as clean porcelain.
rig_no_commit_guard() {
  local gitout lsout
  gitout="$(git status --porcelain --untracked-files=all -- "$@")" || {
    echo "DEVICE FAIL: git status failed — cannot prove build output is untracked" >&2
    exit 1
  }
  if [ -n "$gitout" ]; then
    echo "DEVICE FAIL: build output not gitignored:" >&2
    printf '%s\n' "$gitout" >&2
    exit 1
  fi
  lsout="$(git ls-files -- "$@")" || {
    echo "DEVICE FAIL: git ls-files failed — cannot prove build output is untracked" >&2
    exit 1
  }
  if [ -n "$lsout" ]; then
    echo "DEVICE FAIL: build output is TRACKED by git:" >&2
    printf '%s\n' "$lsout" >&2
    exit 1
  fi
}
