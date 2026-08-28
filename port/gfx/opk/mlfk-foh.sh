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
  # PUNCH-LIST C7: the PLAY path must not run a fixed RNG seed. `--seed 1337`
  # here meant every real match a player ever started shared one RNG stream —
  # a port artefact, not upstream behaviour (the browser game runs on the
  # platform Math.random; the seeded mulberry32 exists for the ORACLE, and
  # every evidence path keeps its fixed seed precisely because that is what
  # makes it reproducible — mlfk.sh:122 and the rigs are deliberately
  # untouched).
  #
  # THE SEED IS RECORDED BEFORE IT IS USED, which is the whole point of the
  # punch-list row: the LAUNCH line's grammar is pinned
  # (check-foh-flows.sh:332) and carries NO seed field, so without this line a
  # randomised session could never be reproduced. This log is that record; it
  # is not grammar-pinned (check-device-foh.sh pins `foh_dev foh:` lines only).
  SEED="$(( ($(date +%s) + $$) % 2147483647 ))"
  echo "mlfk-foh.sh: play seed=$SEED" >> "$LOG"
  # A26 / DEVIATION D53 — HIBERNATE. The app is BACKGROUNDED and `wait`ed on,
  # and the USR1 trap forwards to it. Every word of that is forced:
  #
  #   * Closing the lid runs `powerdown schedule 0.1`, which sends SIGUSR1 to
  #     the pid `frontend:106` recorded and powers the device off 100 ms
  #     later. gmenu2x execs into opkrun and then into THIS SCRIPT, so the pid
  #     it recorded is the SHELL's, not the app's — MEASURED: `kill -USR1
  #     <launcher>` left `launcher=DEAD app=ALIVE` (orphaned), because a shell
  #     with no USR1 trap takes the default disposition and terminates.
  #     So the app must be told, and only this script can tell it.
  #
  #   * The trap CANNOT be added to the old foreground form. POSIX sh DEFERS
  #     traps until a foreground child completes — MEASURED firing 4 s late,
  #     exactly when a `sleep 4` ended, which is 40x past the whole grace.
  #     Backgrounding + `wait` is the platform's own native_launch.sh idiom
  #     and makes the trap fire immediately (measured: forward + save + exit
  #     in 20-60 ms of the 100 ms budget).
  #
  #   * `wait` is REPEATED because a trap-interrupted `wait` returns 128+signo
  #     for the interruption, not the child's status; the second `wait` on the
  #     already-known job yields the real one. Written as `wait "$APP"; rc=$?`
  #     and never `wait "$APP" || rc=$?` — the `||` form leaves rc at the
  #     stale 138 when the retry succeeds (measured: `RC=138` in opk.rc for a
  #     clean exit, which the FOH evidence parsers read as a crash).
  #
  #   * Backgrounding does NOT cost the app its input, which is the obvious
  #     worry: an asynchronous list gets /dev/null on stdin, and this SDL 1.2
  #     backend takes its buttons through SDL_PollEvent. Checked rather than
  #     reasoned about — check-device-foh.sh:2173 already runs `foh_device
  #     --input poll` on THIS device in exactly this shape (`... & wait $!`,
  #     the whole thing `</dev/null`), driven through the real SDL keysym
  #     path by the uinput injector, and it is green. So the shape is proven
  #     on the hardware before this row ever used it.
  #
  # The evidence branch above is deliberately UNCHANGED: nothing sends it
  # SIGUSR1, its rc handling is pinned by the rigs, and one shape per branch
  # beats one shared shape neither branch is written for.
  # shellcheck disable=SC2086 — $SND/$MUS are word lists on purpose
  "$DIR/foh_device" --flow "$DIR/f01-vs-g01.flow" --input poll \
    --flow-out "$EV/foh-trace.txt" \
    --pace 1 --budget-ns 16666667 \
    --bridge live --simdata "$DATA/simdata.txt" --seed "$SEED" \
    --bstate-out "$EV/bstate.txt" \
    --gfxdata "$DATA/gfxdata-frozen.txt" \
    --vfxdata "$DATA/vfxdata-frozen.txt" \
    --glyphs "$DATA/vfxglyphs-frozen.txt" --legible \
    --anim-dir "$DATA" --tapjump-off-p1 --system-menu $SND $MUS \
    >> "$LOG" 2>&1 &
  APP=$!
  # A45/#23 — OPENING THE LID BRINGS THE GAME BACK.
  #
  # THIRD DESIGN, and the two it replaces are both recorded because each was
  # wrong in a way the next one had to answer.
  #
  # v1 called `instant_play save` AT LAUNCH, to keep the ~100 ms lid grace
  # free. That verb's last statement is `exec powerdown now` — it IS the
  # shutdown, not a record-this verb — so starting the game powered the
  # console off, and because v2 pointed the record at this script the next
  # boot did it again. A loop.
  #
  # v2 moved the call into the USR1 trap, where the platform intends it. That
  # fixed the loop and lost the feature to two things at once, both MEASURED
  # on the owner's device: a 20 s "has this session survived" guard refused to
  # arm a lid close that came at ~15 s (startup alone is 370 frames, ~6 s), and
  # even past it the trap has ~40 ms left after forwarding the signal, against
  # two script execs.
  #
  # v3: WRITE THE RECORD DIRECTLY. `instant_play save` does two separable
  # things and we only want one of them. The file it produces is a shell
  # snippet in a documented shape, so this writes that shape itself and never
  # invokes the verb — no shutdown at launch, and nothing to race at the lid
  # because the file is already there when the signal arrives.
  #
  # The crash-loop guard stays but becomes a LIVENESS check rather than a
  # stopwatch: the record is written 5 s in, and only if the app is still
  # running. An instant death arms nothing, and every real session passes 5 s
  # long before a lid can close on it. It is deleted on a clean exit, so only
  # a session that DIED comes back.
  #
  # The path written is ABSOLUTE ($DIR is the OPK mount point), so nothing
  # here needs /var/run/pid_path and nothing calls `pid record` — see the
  # note at the write itself, where that call turned out to be defect 5.
  # `instant_play load` re-mounts the OPK named by /mnt/last_opk, which
  # opkrun writes at launch and the frontend loop clears afterwards, so it is
  # live for exactly the window a lid close falls in.
  RESUME_ARM_AFTER=5
  (
    sleep "$RESUME_ARM_AFTER"
    if kill -0 "$APP" 2>/dev/null; then
      # NO `pid record` HERE, and its absence is the fix for defect 5.
      #
      # v1/v2 called it because `instant_play save` ignores the path handed
      # to it and rebuilds it from /var/run/pid_path. v3 writes the record
      # itself with an absolute path, so nothing needs pid_path any more —
      # and leaving the call in was actively harmful.
      #
      # MEASURED BY READING, after three failed device tests: `powerdown
      # schedule` sends SIGUSR1 to `pid print`, i.e. the pid last RECORDED.
      # frontend:106 records the launcher, which is why the launcher can have
      # a USR1 trap at all (A26). Recording the APP's pid re-aims the lid
      # signal AT THE APP, so the launcher's trap never fires, HIBERNATING
      # stays 0, and the clean-exit path deletes the record that was armed
      # correctly seconds earlier. Every symptom fits: the player's state
      # saved (`resume 06` was right on the card) and the relaunch vanished.
      # The shape `instant_play save` emits, minus its `exec powerdown now`.
      # Recording THIS SCRIPT rather than the binary is deliberate: a resumed
      # session then re-arms the next one, and inherits the USR1 trap and the
      # clean-exit removal instead of losing both.
      # THE RECORD MOUNTS ITS OWN OPK. Everything below /opk — this script,
      # the binary, the data — exists only while that squashfs is mounted, and
      # `instant_play load` mounts it ONLY if /mnt/last_opk is readable at boot.
      # That file belongs to opkrun, not to us. MEASURED on the device, both
      # arms: with it present the resume works end to end; with it absent the
      # boot prints `/opk/mlfk-foh.sh: not found` and walks on to the frontend,
      # having done nothing a player can see. Depending on another process's
      # bookkeeping to survive a power cut is the fragile half of this feature,
      # so the record stops depending on it and mounts the OPK itself. If the
      # boot already mounted it the mount fails and is discarded; the record is
      # correct either way. It also re-writes /mnt/last_opk so `instant_play
      # load`'s own trailing `umount /opk; rm last_opk` still tidies up after
      # the resumed session, exactly as it does after a normal one.
      #
      # AND THE `&` STAYS ON THE ARGS LINE. `instant_play save` builds its args
      # with a trailing SPACE and no newline before appending its heredoc, so
      # its `&` lands on line 1: `'/opk/mlfk-foh.sh' &`. Putting a \n there
      # instead leaves `&` alone on the next line, which is a shell SYNTAX
      # ERROR — and since `instant_play load` consumes this with `source`, the
      # shell runs line 1 in the FOREGROUND, then errors, then never reaches
      # `pid record`. The game comes back unable to hibernate again and with
      # the frontend stuck behind it. That was defect 6.
      OPKFILE="$(cat /mnt/last_opk 2>/dev/null)"
      if [ -n "$OPKFILE" ]; then
        printf "mount -t squashfs '%s' /opk 2>/dev/null\necho -n '%s' > /mnt/last_opk\n'%s' &\npid record \$!\nwait \$!\npid erase\n" \
          "$OPKFILE" "$OPKFILE" "$DIR/mlfk-foh.sh" > /mnt/instant_play 2>/dev/null || true
        sync 2>/dev/null || true
        echo "mlfk-foh.sh: relaunch armed" >> "$LOG"
        # AND ON THE CARD. $LOG lives in /tmp, which is tmpfs, so a lid close
        # erases exactly the evidence a lid-close bug needs. Three diagnoses in
        # this feature were guesses for want of this one line, and the fourth
        # was found by reading a device instead. It says "armed" only where
        # something was actually armed.
        echo "armed $(date +%s)" >> /mnt/mlfk-resume.log 2>/dev/null || true
      else
        # Nothing to arm and nothing to blame later: say so on the card.
        echo "no-opkfile $(date +%s)" >> /mnt/mlfk-resume.log 2>/dev/null || true
      fi
    fi
  ) &
  RESUME_TIMER=$!

  # The trap is a plain forward again: the record is already on the card by
  # the time any lid close can happen, so the grace window buys the PLAYER's
  # state and nothing else competes for it.
  # HIBERNATING marks WHY the app is about to exit, and it is load-bearing.
  # MEASURED: a lid close DOES reach the clean-exit path below. The app takes
  # SIGUSR1, publishes the player's state and `_exit(0)`s — a clean exit — so
  # `wait` returns normally and the launcher walks straight into the `rm` that
  # is meant for "the player quit". Without this flag the lid deletes the
  # relaunch record it just armed, which is exactly what the first v3 test
  # measured. The comment that said "a lid close never reaches this line" was
  # simply wrong, and it is the third time in this feature that a claim in a
  # comment outlived the code it described.
  HIBERNATING=0
  trap 'HIBERNATING=1; echo "trap $(date +%s)" >> /mnt/mlfk-resume.log 2>/dev/null; kill -USR1 "$APP" 2>/dev/null' USR1
  wait "$APP"; rc=$?
  if [ "$rc" -gt 128 ]; then wait "$APP"; rc=$?; fi
  # A PLAYER QUITTING IS NOT A LID CLOSE. Only the former spends the record.
  kill "$RESUME_TIMER" 2>/dev/null || true
  if [ "$HIBERNATING" = 0 ]; then
    echo "quit rc=$rc $(date +%s) — record spent" >> /mnt/mlfk-resume.log 2>/dev/null || true
    rm -f /mnt/instant_play 2>/dev/null || true
  else
    echo "hibernate rc=$rc $(date +%s) — record kept" >> /mnt/mlfk-resume.log 2>/dev/null || true
    echo "mlfk-foh.sh: hibernating — relaunch record kept" >> "$LOG"
  fi
fi
echo "RC=$rc" > "$EV/opk.rc"
exit "$rc"
