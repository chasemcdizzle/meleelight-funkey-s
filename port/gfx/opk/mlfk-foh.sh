#!/bin/sh
# port/gfx/opk/mlfk-foh.sh — MeleeLight FOH OPK launcher (M4 task 10).
#
# The FOH-generation launcher: it ENTERS THE FRONT-OF-HOUSE (menus ->
# CSS -> SSS -> live match), never the direct-match path — the task-10
# requirement. mlfk.sh (the M3 direct-match launcher) is byte-untouched;
# the persistent play install is swapped to this launcher only at the
# M4 provisioning step (task 14), evidence runs use a separate OPK
# under a UNIQUE title ("MeleeLight FOH" — the iter-73 stale-nav class
# avoided).
#
# Same conventions as mlfk.sh (Exec= a launcher script; OPK mounts
# read-only; tmpfs during play with a log copy-back trap; the data-dir
# env chain; the boot marker written BEFORE the app runs, carrying the
# sha256 of the foh_device bytes ACTUALLY MOUNTED from the OPK; app rc
# in opk.rc as exactly `RC=<n>\n`).
#
# MODES:
#   evidence — when $DATA/foh-args exists, its single line is passed
#     verbatim as the foh_device argument list (the check pins the
#     file's bytes host+device side; the task-10 evidence run is a
#     bounded no-input FOH leg: startup -> title, two tick shots).
#   live (default) — the PLAY path: FOH menus on the real input, then
#     the launched match runs LIVE (S1 chord table) for up to 3 min,
#     with audio when sndpack.bin is present and menu/stage MUSIC when
#     foh-music.txt is present (paths inside it must resolve from $DATA
#     — provisioned by task 13/14). --tapjump-off-p1 presets the S1
#     contract; the player can change it on the options screen.
#
# Boot marker grammar (frozen; check-device-foh.sh parses it anchored):
#   MLFK FOH BOOT
#   bin <64-lowercase-hex>
#   mode <evidence|live>
export SDL_NOMOUSE=1

DIR="$(cd "$(dirname "$0")" && pwd)"
EV=/tmp/mlfk/opkfoh
mkdir -p "$EV" || exit 9
LOG="$EV/mlfk-foh.log"
: > "$LOG"

DATA="${MLFK_DATA_DIR:-}"
DATA_ERR=""
if [ -n "$DATA" ]; then
  # explicit selection must qualify itself; NEVER falls through to the
  # chain (the mlfk.sh iter-60 review-58 L2 rule)
  if [ ! -f "$DATA/simdata.txt" ]; then
    DATA_ERR="MLFK_DATA_DIR=$DATA is set but $DATA/simdata.txt is missing — refusing (explicit data dir must qualify; fallback chain not consulted)"
    DATA=""
  fi
else
  for d in /mnt/mlfk-scratch /mnt/mlfk-data; do
    if [ -f "$d/simdata.txt" ]; then DATA="$d"; break; fi
  done
fi

copyback() {
  if [ -n "$DATA" ] && [ -d "$DATA" ]; then
    mkdir -p "$DATA/mlfk-logs" 2>/dev/null
    cp "$LOG" "$EV/opk.rc" "$DATA/mlfk-logs/" 2>/dev/null
  fi
}
trap copyback EXIT

BINSHA="$(sha256sum "$DIR/foh_device" | cut -c1-64)"
MODE=live
if [ -n "$DATA" ] && [ -f "$DATA/foh-args" ]; then MODE=evidence; fi
{
  echo "MLFK FOH BOOT"
  echo "bin $BINSHA"
  echo "mode $MODE"
} > "$EV/boot-marker"

if [ -n "$DATA_ERR" ]; then
  echo "mlfk-foh.sh: $DATA_ERR" >> "$LOG"
  echo "RC=7" > "$EV/opk.rc"
  exit 7
fi
if [ -z "$DATA" ]; then
  echo "mlfk-foh.sh: no data dir found (MLFK_DATA_DIR / /mnt/mlfk-scratch / /mnt/mlfk-data need simdata.txt)" >> "$LOG"
  echo "RC=8" > "$EV/opk.rc"
  exit 8
fi
echo "mlfk-foh.sh: mode=$MODE data=$DATA bin=$BINSHA" >> "$LOG"

rc=0
if [ "$MODE" = evidence ]; then
  # shellcheck disable=SC2046 — the pinned args file is ours, split on purpose
  "$DIR/foh_device" $(cat "$DATA/foh-args") >> "$LOG" 2>&1 || rc=$?
else
  SND=""
  if [ -f "$DATA/sndpack.bin" ]; then SND="--sndpack $DATA/sndpack.bin"; fi
  MUS=""
  if [ -n "$SND" ] && [ -f "$DATA/foh-music.txt" ]; then
    MUS="--music-manifest $DATA/foh-music.txt"
  fi
  # The PLAY path: poll-mode FOH (real buttons), live S1 match after
  # LAUNCH (10800 frames = 3 min, the mlfk.sh live-session bound);
  # recording goes to tmpfs (the gfx_app --live mandatory-record
  # contract). The FOH phase gets a generous 5-minute tick budget —
  # reaching it without launching just exits cleanly (rc 4 recorded
  # honestly in opk.rc: --bridge given, no launch).
  # A11/A12 "QUIT TO MENU": the in-match pause overlay exits the app with
  # rc 70 (FOH_PAUSE_RC_MENU, port/foh/foh_pause.h) to ask for the menus
  # back. The app BOOTS into the FOH, so relaunching it IS the menus —
  # a clean process boundary instead of the FOH/match outer loop no play
  # arm has (foh_dev.c). Every other rc leaves the loop and returns the
  # player to the frontend, exactly as before. The bound is a safety net,
  # not a policy: a wedged rc-70 loop must not spin forever.
  n=0
  while :; do
  # shellcheck disable=SC2086 — $SND/$MUS are word lists on purpose
  "$DIR/foh_device" --flow "$DIR/f01-vs-g01.flow" --input poll \
    --flow-out "$EV/foh-trace.txt" --foh-max 18000 \
    --pace 1 --budget-ns 16666667 \
    --bridge live --simdata "$DATA/simdata.txt" --seed 1337 \
    --bstate-out "$EV/bstate.txt" \
    --frames 10800 \
    --record-trace "$EV/live-trace.json" --record-keys "$EV/live-keys.txt" \
    --gfxdata "$DATA/gfxdata-frozen.txt" \
    --vfxdata "$DATA/vfxdata-frozen.txt" \
    --glyphs "$DATA/vfxglyphs-frozen.txt" --legible \
    --anim-dir "$DATA" --tapjump-off-p1 $SND $MUS \
    >> "$LOG" 2>&1 || rc=$?
    [ "$rc" = 70 ] || break
    n=$((n + 1))
    [ "$n" -lt 64 ] || { echo "mlfk-foh: rc-70 relaunch bound hit" >> "$LOG"; break; }
    rc=0
  done
fi
echo "RC=$rc" > "$EV/opk.rc"
exit "$rc"
