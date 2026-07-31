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
#     the launched match runs LIVE (S1 chord table) UNBOUNDED and
#     UNRECORDED — the player leaves via the pause overlay (C6),
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
# QUALIFY = simdata.txt AND assets/menu.img1 (A1 restyle Phase 1). The FOH's
# CSS/SSS screens render REAL upstream artwork from the IMG1 pack and
# foh_render's art_load treats a missing pack as FATAL, so a data dir without
# it cannot serve this launcher. Folding artwork into the EXISTING qualify
# predicate (rather than adding a late check) keeps one rule, one pair of exit
# codes, and — the point — makes the fallback chain pick a dir that has BOTH:
# a dir with simdata but no artwork must not win and then die mid-boot, and
# the sim plane and the artwork plane must never come from DIFFERENT mounts.
if [ -n "$DATA" ]; then
  # explicit selection must qualify itself; NEVER falls through to the
  # chain (the mlfk.sh iter-60 review-58 L2 rule)
  if [ ! -f "$DATA/simdata.txt" ]; then
    DATA_ERR="MLFK_DATA_DIR=$DATA is set but $DATA/simdata.txt is missing — refusing (explicit data dir must qualify; fallback chain not consulted)"
    DATA=""
  elif [ ! -f "$DATA/assets/menu.img1" ]; then
    DATA_ERR="MLFK_DATA_DIR=$DATA is set but $DATA/assets/menu.img1 is missing — refusing (menu artwork is required; produced by \`node pipeline/run.js --only assets\`)"
    DATA=""
  fi
else
  for d in /mnt/mlfk-scratch /mnt/mlfk-data; do
    if [ -f "$d/simdata.txt" ] && [ -f "$d/assets/menu.img1" ]; then DATA="$d"; break; fi
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
  # The message must name the WHOLE qualify predicate: after the A1 restyle
  # a dir with simdata.txt but no artwork is silently skipped, and a
  # diagnostic naming only simdata.txt sends the reader looking for a file
  # that is already there (review-b9-2-codex [L]).
  echo "mlfk-foh.sh: no data dir found (MLFK_DATA_DIR / /mnt/mlfk-scratch / /mnt/mlfk-data each need BOTH simdata.txt AND assets/menu.img1)" >> "$LOG"
  echo "RC=8" > "$EV/opk.rc"
  exit 8
fi
# Artwork comes from the SAME qualified data dir as everything else — set
# explicitly so art_load never consults its own mount-point fallback chain and
# cannot pair this run's simdata with some other mount's artwork.
MLFK_MENU_IMG1="$DATA/assets/menu.img1"
export MLFK_MENU_IMG1

echo "mlfk-foh.sh: mode=$MODE data=$DATA bin=$BINSHA art=$MLFK_MENU_IMG1" >> "$LOG"

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
  # LAUNCH, for as long as the player keeps playing.
  #
  # NO --frames AND NO --record-trace/--record-keys HERE (punch-list C6).
  # This branch used to pass the evidence rigs' shape — `--frames 10800`
  # (exactly 180 s at 60 fps) plus the mandatory recording — so any match
  # past 3:00 ENDED and dumped the player back to the frontend mid-play:
  # the same user-visible symptom as C1, one screen later.
  # The three flags stand or fall together and foh_dev enforces that,
  # because the recording is a RAM buffer (~444 B/frame, MlSb doubling
  # peak ~2x) that would exhaust this device's ~37 MB MemAvailable after
  # ~11.6 min — dropping the bound while keeping the recording would have
  # traded a clean 3:00 exit for an OOM kill at ~12 min. Nothing reads
  # this path's recording: every evidence consumer drives its OWN app run
  # with its OWN --record-trace and its OWN bound, and those are untouched
  # (check-device-target.sh [6b]/[6c] run foh_device; check-device-input.sh
  # runs gfx_device — different binary, same discipline).
  # So: do NOT reintroduce --frames or the --record-* pair on this line.
  #
  # NO --foh-max HERE (punch-list C1). This branch used to pass the
  # evidence rigs' tick budget (18000 = exactly 300 s at 60 fps), so
  # the menus timed out: the FOH loop ran out, nothing had launched,
  # and foh_device exited rc 4 straight back to the frontend — the
  # owner's "crashed while sitting on CHARACTER SELECT" (their SD log
  # carries that rc 4 and `--bridge given but the flow never
  # launched`; reproduced with zero input in 300 s, memory flat).
  # foh_dev reads the ABSENCE of --foh-max here (under --bridge live) as
  # "unbounded FOH". Absence rather than a magic value, because a budget
  # that IS passed must keep meaning what it says: check-device-target.sh's
  # live play leg — the punch-list A2 regression guard — deliberately runs
  # --bridge live WITH --foh-max 1400 so it terminates if its navigation
  # fails. So: do NOT reintroduce --foh-max on this line. The evidence
  # branch above is unaffected; it reads foh-args, which the rigs pin.
  # ONE launch, no relaunch loop (review-mexit-r5 Low, HARD RULE 2). This used
  # to run the app in a `while` that re-launched it on exit code 70
  # (FOH_PAUSE_RC_MENU) to "put the FOH menus back". Punch-list A19 made that
  # return happen IN-PROCESS via foh_dev.c's `foh_phase:` re-entry, so nothing
  # emits 70 any more and nothing can — the constant and the loop were kept
  # only for a hypothetical future arm, which is scaffolding for later. Worse
  # than dead: an unexpected rc 70 from some future defect would have been
  # SILENTLY answered by relaunching the game instead of surfacing. The app now
  # runs once and every exit code is reported as itself. Re-add a loop only
  # alongside a live producer of the code it answers.
  # shellcheck disable=SC2086 — $SND/$MUS are word lists on purpose
  "$DIR/foh_device" --flow "$DIR/f01-vs-g01.flow" --input poll \
    --flow-out "$EV/foh-trace.txt" \
    --pace 1 --budget-ns 16666667 \
    --bridge live --simdata "$DATA/simdata.txt" --seed 1337 \
    --bstate-out "$EV/bstate.txt" \
    --gfxdata "$DATA/gfxdata-frozen.txt" \
    --vfxdata "$DATA/vfxdata-frozen.txt" \
    --glyphs "$DATA/vfxglyphs-frozen.txt" --legible \
    --anim-dir "$DATA" --tapjump-off-p1 --system-menu $SND $MUS \
    >> "$LOG" 2>&1 || rc=$?
fi
echo "RC=$rc" > "$EV/opk.rc"
exit "$rc"
