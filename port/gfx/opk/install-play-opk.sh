#!/usr/bin/env bash
# port/gfx/opk/install-play-opk.sh — provision the PERSISTENT PLAY INSTALL
# (punch-list A13).
#
# WHY THIS EXISTS. mlfk-foh.sh:7 promised that "the persistent play install
# is swapped to this launcher only at the M4 provisioning step (task 14)"
# and that evidence runs use "a separate OPK under a UNIQUE title". The
# separate-title half was never written down as a file: BOTH the play
# install and check-device-opk.sh's throwaway FOH OPK were built from
# meleelight-foh.funkey-s.desktop, so the play install was hand-built and
# carried the check's title. That is how /mnt/Applications/meleelight-foh.opk
# (title "MeleeLight FOH") came to sit on the device while the check's
# OPK_INVENTORY_PIN named /mnt/Applications/meleelight.opk — a hand-made
# install nothing could reproduce, drifting silently away from its pin.
#
# So: THREE roles, THREE .desktop files, THREE titles, none equal.
#   play install   meleelight-play.funkey-s.desktop  "MeleeLight"      <- here
#   M3 evidence    meleelight.funkey-s.desktop       "MeleeLight EV"
#   FOH evidence   meleelight-foh.funkey-s.desktop   "MeleeLight FOH"
# Both evidence OPKs are installed and removed inside a check run; only this
# one persists. Equal titles are the iter-73 stale-nav class: gmenu2x orders
# the games grid alphabetically by .desktop Name and both checks navigate to
# a MEASURED link index, so two entries sharing a title make that index
# ambiguous. Worse for the FOH arm specifically — the play install runs the
# SAME mlfk-foh.sh and writes the SAME boot marker at the same path, so
# landing on the wrong tile could have produced valid-looking evidence
# instead of failing loud.
#
# THIS IS NOT A GATE. It provisions the device; it proves nothing about the
# port. check-device-opk.sh is the gate that launches an OPK through the
# frontend, and its OPK_INVENTORY_PIN is what notices if this install goes
# missing or changes name.
#
# NOT IN riglib.sh's RIG_SCRIPTS, deliberately: that list keys the shared
# arm-build stamp, and this script's bytes cannot change a compiled byte —
# it only packages binaries the stamp already governs. Adding it would force
# every device check to rebuild whenever this file is edited, for nothing.
#
# Usage: bash port/gfx/opk/install-play-opk.sh
#   env: FUNKEY_ADB_ID (device id), MLFK_FORCE_ARM=1 (ignore the build stamp)
set -euo pipefail
cd "$(dirname "$0")/../../.."

GFX=port/gfx
OPKDIR=$GFX/opk
BUILD=$GFX/build
DEVB=port/sim/calib/build/device
TABLES=pipeline/build/sim-tables
FDC=oracle/fdlibm-crosscheck   # rig_srchash hashes csweep.c from here
DEVAPPS=/mnt/Applications
# The rig's own device paths — riglib's lock/recovery-claim and cleanup
# read both. $DSD is the SHARED scratch every device check wipes; the play
# data plane is /mnt/mlfk-data and is never touched here.
DTMP=/tmp/mlfk
DSD=/mnt/mlfk-scratch
FLOWS=port/foh/flows
FOH_FLOW_ID=f01-vs-g01
mkdir -p "$BUILD" "$DEVB"

# The play OPK's own frozen names. OPK_NAME is what OPK_INVENTORY_PIN in
# port/gfx/check-device-opk.sh names — changing it is a two-file change.
OPK_NAME=meleelight.opk
OPK_BIN=foh_device
LAUNCHER=$OPKDIR/mlfk-foh.sh
DESKTOP=$OPKDIR/meleelight-play.funkey-s.desktop
ICON=$OPKDIR/icon32.png
# The SDK container's squashfs-tools version line, EXACT — the same pin
# check-device-opk.sh carries, for the same reason: a different mksquashfs
# writes an OPK the FunKey kernel silently cannot mount.
MKSQ_VERSION_LINE='mksquashfs version 4.4 (2019/08/29)'
# The name this install carried before A13, removed below so the device
# cannot end up with two play installs (and two grid entries).
LEGACY_OPK_NAME=meleelight-foh.opk

source port/sim/device/adbsh.sh   # (also defines $DEV — it keys the lock)
source port/sim/device/riglib.sh

rig_lock_acquire

echo "== [1/4] committed OPK assets (desktop grammar + trailing empty line) =="
made "$DESKTOP" "$LAUNCHER" "$ICON" "$FLOWS/$FOH_FLOW_ID.flow"
# Anchored whole-file grammar, same shape as check-device-opk.sh [1/8]: the
# 9 exact lines this repo ships plus the MANDATORY trailing empty line. The
# Name line is load-bearing beyond cosmetics — it fixes this install's place
# in the alphabetical grid, which both checks' NAV_LINK pins describe.
desktop_want=$'[Desktop Entry]\nName=MeleeLight\nComment=MeleeLight port (private build)\nExec=mlfk-foh.sh\nTerminal=false\nType=Application\nStartupNotify=true\nIcon=icon32\nCategories=games;\n\n'
if ! printf '%s' "$desktop_want" | cmp -s - "$DESKTOP"; then
  echo "FAIL: $DESKTOP does not match the pinned desktop-entry bytes (incl. the mandatory trailing empty line)" >&2
  echo "  If the Name changed on purpose, re-measure both NAV_LINK values in" >&2
  echo "  port/gfx/check-device-opk.sh (:280, :293) in the SAME change." >&2
  exit 1
fi
echo "   .desktop byte-exact vs the pinned entry (Name=MeleeLight)"
# ponytail: no icon content pin here. The unsquashfs cmp below already proves
# the packaged icon IS the committed one, and check-device-opk.sh owns the
# icon's frozen sha for the arms that make claims about it.

echo "== [2/4] armv7 build (shared rig stamp) =="
rig_arm_build
rig_stamp_rehash "$OPK_BIN"
STAMP_BIN_SHA="$(rig_stamp_bin_sha "$OPK_BIN")"

echo "== [3/4] stage + mksquashfs 4.4 (SDK container ONLY) + content verification =="
STAGE=$BUILD/play-opk-stage
rm -rf "$STAGE" "$BUILD/$OPK_NAME" "$BUILD/play-opk-verify"
mkdir -p "$STAGE"
cp "$DEVB/$OPK_BIN" "$LAUNCHER" "$DESKTOP" "$ICON" "$FLOWS/$FOH_FLOW_ID.flow" "$STAGE/"
# The flow is a packaged member: mlfk-foh.sh's live (PLAY) branch runs
# `--flow $DIR/f01-vs-g01.flow` from the OPK mount, not from the data plane.
STAGE_FILES="$OPK_BIN $(basename "$LAUNCHER") $(basename "$DESKTOP") icon32.png $FOH_FLOW_ID.flow"
N_STAGE_WANT="$(printf '%s\n' $STAGE_FILES | wc -l | tr -d ' ')"
chmod +x "$STAGE/$OPK_BIN" "$STAGE/$(basename "$LAUNCHER")"
ssum="$(rig_host_sha256 "$STAGE/$OPK_BIN")" || exit 1
if [ "$ssum" != "$STAMP_BIN_SHA" ]; then
  echo "FAIL: staged $OPK_BIN sha ($ssum) != stamp record ($STAMP_BIN_SHA)" >&2
  exit 1
fi
rm -f "$BUILD/play-mksq-version.txt"
docker run --rm -v "$PWD":/work -w /work "$ARMIMGID" bash -lc '
  set -e
  export PATH=/opt/FunKey-sdk-2.3.0/bin:$PATH
  mksquashfs -version | head -1 > port/gfx/build/play-mksq-version.txt
  mksquashfs port/gfx/build/play-opk-stage port/gfx/build/'"$OPK_NAME"' \
    -all-root -noappend -no-exports -no-xattrs -comp gzip >/dev/null
'
made "$BUILD/play-mksq-version.txt" "$BUILD/$OPK_NAME"
if ! printf '%s\n' "$MKSQ_VERSION_LINE" | cmp -s - "$BUILD/play-mksq-version.txt"; then
  echo "FAIL: container mksquashfs version line is not exactly '$MKSQ_VERSION_LINE':" >&2
  cat "$BUILD/play-mksq-version.txt" >&2
  exit 1
fi
docker run --rm -v "$PWD":/work -w /work "$ARMIMGID" bash -lc '
  set -e
  export PATH=/opt/FunKey-sdk-2.3.0/bin:$PATH
  unsquashfs -d port/gfx/build/play-opk-verify port/gfx/build/'"$OPK_NAME"' >/dev/null
'
# shellcheck disable=SC2086 — STAGE_FILES is our own space-joined list
for f in $STAGE_FILES; do
  made "$BUILD/play-opk-verify/$f"
  cmp "$STAGE/$f" "$BUILD/play-opk-verify/$f"
done
n_stage="$(find "$STAGE" -type f | wc -l | tr -d ' ')"
n_verify="$(find "$BUILD/play-opk-verify" -type f | wc -l | tr -d ' ')"
if [ "$n_stage" != "$N_STAGE_WANT" ] || [ "$n_verify" != "$N_STAGE_WANT" ]; then
  echo "FAIL: OPK content count mismatch (stage $n_stage, unsquashed $n_verify, want $N_STAGE_WANT)" >&2
  exit 1
fi
OPK_SHA="$(rig_host_sha256 "$BUILD/$OPK_NAME")" || exit 1
echo "   OPK contents verified ($N_STAGE_WANT files byte-identical through unsquashfs; opk sha $OPK_SHA)"

echo "== [4/4] install to $DEVAPPS/$OPK_NAME (and retire the pre-A13 name) =="
adb -s "$DEV" push "$BUILD/$OPK_NAME" "$DEVAPPS/" >/dev/null
dsum="$(rig_dev_sha256 "$DEVAPPS/$OPK_NAME")" || exit 1
if [ "$dsum" != "$OPK_SHA" ]; then
  echo "FAIL: device-side OPK sha ($dsum) != host ($OPK_SHA)" >&2
  exit 1
fi
echo "   installed $DEVAPPS/$OPK_NAME (sha $dsum == host)"
rig_dsh_retry "rm -f $DEVAPPS/$LEGACY_OPK_NAME" || true
if ! rig_dsh_retry "test ! -f $DEVAPPS/$LEGACY_OPK_NAME"; then
  echo "FAIL: $DEVAPPS/$LEGACY_OPK_NAME is still present — two play installs, two grid entries, and both NAV_LINK pins wrong" >&2
  exit 1
fi
echo "   pre-A13 name $DEVAPPS/$LEGACY_OPK_NAME verified absent"
rig_no_commit_guard "$BUILD" "$DEVB" "$TABLES"

# The frontend caches the grid it scanned at startup: gmenu2x must restart
# before the new entry appears. Not done here — check-device-opk.sh owns the
# frontend respawn cycle (its step [6]) and does it under a conf backup, and
# a human just power-cycles.
echo "PLAY OPK INSTALLED ($DEVAPPS/$OPK_NAME, title \"MeleeLight\", bin-sha == stamp; restart the frontend to see it)"
