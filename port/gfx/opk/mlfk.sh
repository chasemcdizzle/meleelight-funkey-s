#!/bin/sh
# port/gfx/opk/mlfk.sh — MeleeLight OPK launcher (M3 task 7).
#
# Exec target of meleelight.funkey-s.desktop (Exec= a launcher script,
# never the binary — CLAUDE.md OPK packaging rule; ssb64 ssb64.sh
# pattern, docs/research/funkey-envelope.md §2). The OPK mounts
# READ-ONLY at /opk: everything written goes to tmpfs (/tmp/mlfk/opk —
# the device-hygiene tree) with a log copy-back trap to the SD data
# dir on exit (SD streaming mid-game = multi-second stalls; tmpfs
# during play, copy back after — CLAUDE.md gotcha).
#
# DATA-DIR ENV CHAIN (the app's SD data: SIMDATA1, golden trace texts,
# GFXDATA, ANIM1 bins, SNDPACK1):
#   1. $MLFK_DATA_DIR when set (check-device-opk.sh sets nothing — it
#      provisions /mnt/mlfk-scratch, found by the chain);
#   2. /mnt/mlfk-scratch (the rig scratch dir — evidence runs);
#   3. /mnt/mlfk-data   (the persistent SD install for human play —
#      provisioned at the M3 human gate, survives rig cleanup).
# A dir qualifies only if it carries simdata.txt (the one file every
# mode needs).
#
# MODES:
#   evidence — when $DATA/opk-args exists, its single line is passed
#     verbatim as the gfx_device argument list (the check pins the
#     file's bytes host+device side; it selects a bounded paced g01
#     replay with --shot evidence). This is how the M3 gate proves the
#     FRONTEND path launches the real app.
#   live (default) — a paced live S1 match (fox vs marth on
#     battlefield, 10800 frames = 3 min, audio on when sndpack.bin is
#     present, --tapjump-off-p1 per the PLAN §6 S1 contract). The
#     Chase ratification playtest path.
#
# BOOT MARKER (PROCESS §4 installed-artifact identity at launch
# evidence time): written BEFORE the app runs; carries the sha256 of
# the gfx_device bytes ACTUALLY MOUNTED from the OPK, which the host
# check compares against the arm-build stamp record. Grammar (frozen;
# check-device-opk.sh parses it anchored):
#   MLFK OPK BOOT
#   bin <64-lowercase-hex>
#   mode <evidence|live>
# App exit code lands in opk.rc as exactly `RC=<n>\n` (detached
# frontend runs drop exit codes — the rc file is the only truthful
# exit evidence; task-4/5 rc-file pattern).
export SDL_NOMOUSE=1

DIR="$(cd "$(dirname "$0")" && pwd)"
EV=/tmp/mlfk/opk
mkdir -p "$EV" || exit 9
LOG="$EV/mlfk.log"
: > "$LOG"

DATA="${MLFK_DATA_DIR:-}"
if [ -z "$DATA" ]; then
  for d in /mnt/mlfk-scratch /mnt/mlfk-data; do
    if [ -f "$d/simdata.txt" ]; then DATA="$d"; break; fi
  done
fi

copyback() {
  # log copy-back (SD only after the run; best-effort, never masks rc)
  if [ -n "$DATA" ] && [ -d "$DATA" ]; then
    mkdir -p "$DATA/mlfk-logs" 2>/dev/null
    cp "$LOG" "$EV/opk.rc" "$DATA/mlfk-logs/" 2>/dev/null
  fi
}
trap copyback EXIT

BINSHA="$(sha256sum "$DIR/gfx_device" | cut -c1-64)"
MODE=live
if [ -n "$DATA" ] && [ -f "$DATA/opk-args" ]; then MODE=evidence; fi
{
  echo "MLFK OPK BOOT"
  echo "bin $BINSHA"
  echo "mode $MODE"
} > "$EV/boot-marker"

if [ -z "$DATA" ]; then
  echo "mlfk.sh: no data dir found (MLFK_DATA_DIR / /mnt/mlfk-scratch / /mnt/mlfk-data need simdata.txt)" >> "$LOG"
  echo "RC=8" > "$EV/opk.rc"
  exit 8
fi
echo "mlfk.sh: mode=$MODE data=$DATA bin=$BINSHA" >> "$LOG"

rc=0
if [ "$MODE" = evidence ]; then
  # shellcheck disable=SC2046 — the pinned args file is ours, split on purpose
  "$DIR/gfx_device" $(cat "$DATA/opk-args") >> "$LOG" 2>&1 || rc=$?
else
  SND=""
  if [ -f "$DATA/sndpack.bin" ]; then SND="--sndpack $DATA/sndpack.bin"; fi
  # shellcheck disable=SC2086 — $SND is empty-or-two-words on purpose
  "$DIR/gfx_device" --live \
    --simdata "$DATA/simdata.txt" --gfxdata "$DATA/gfxdata-frozen.txt" \
    --anim-dir "$DATA" \
    --seed 1337 --p1 2 --p2 0 --stage 0 --frames 10800 \
    --pace 1 --budget-ns 16666667 --tapjump-off-p1 $SND \
    --out "$EV/live-out.txt" --timing "$EV/live-tim.txt" \
    --record-trace "$EV/live-trace.json" --record-keys "$EV/live-keys.txt" \
    >> "$LOG" 2>&1 || rc=$?
fi
echo "RC=$rc" > "$EV/opk.rc"
exit "$rc"
