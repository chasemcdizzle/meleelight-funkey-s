#!/usr/bin/env bash
# port/foh/check-live-arms.sh — the SUCCESSOR RIG for the two live-play arms
# nothing had ever executed (fix_plan R3; owner resequencing ruling
# 2026-07-30, docs/STATE.md §rulings).
#
# WHY IT EXISTS
# -------------
# Two blocks of shipping code had never run anywhere — not on the host, not on
# the device, not in a match and not in the menus:
#
#   * foh_sysmenu_open (~157 lines, foh_pause.c) — the in-match FunKey SYSTEM
#     menu (VOLUME / BRIGHTNESS / QUIT / POWER OFF);
#   * the VS-FINISH block (~75 lines, foh_dev.c's `if (g_vsFinish)` arm) — the
#     TIME!/GAME! banner, its sfx, the music mute and the 2500 ms hold that
#     upstream's finishGame holds before endGame.
#
# And `verify_m4.sh` cannot reach either BY CONSTRUCTION, which is why running
# the gate more times would never have helped:
#   * the system-menu dispatch predicate is `sysMenu && !shotsDir &&
#     foh_sysmenu_hook` (foh_dev.c), and EVERY committed flow leg passes
#     `--shots-dir` — with a shots dir the MENU keysym is the screenshot
#     marker, so the modal is deliberately disabled there;
#   * `--match-timer`, the only trigger for the VS-finish arm inside a
#     scriptable run, appears in NO check script at all.
# Without this rig, the first execution of those ~232 lines would have been
# Chase's acceptance playthrough.
#
# WHAT IT ALREADY FOUND (first run, before a single assertion was written):
# a SHIPPING CRASH. Driving a VS match to its natural clock end died with
# `glyphs: font 0 has no glyph '-'` / `SIM FATAL frame 210: glyphs: missing
# glyph` — the HUD timer renders a NEGATIVE matchTimer on the finish frame
# (matchTimerTick decrements before it tests), and the T40/T25 atlases carry
# digits and ':' only. Upstream never draws that string because its render is
# gated `playing || frameByFrameRender` and finishGame clears `playing`, so
# the frame is simply never rendered there. Fixed at gfx_overlay.c's timer
# block (expiry clamp, full argument in situ) and guarded by T6 below, which
# rebuilds the unclamped expression and requires the crash back. Every natural
# VS timeout on the device would have aborted the game.
#
# WHAT IT PROVES, and how each claim is bound to evidence
# ------------------------------------------------------
# ARM A — the SYSTEM MENU, driven through the REAL polled input path:
#   [A1] opened MID-MATCH, navigated (RIGHT = volume up, DOWN = next option,
#        LEFT = brightness down) and dismissed with B. Bound to pixels
#        POSITIONALLY: the overlay's own MLFK_MENU_SHOT frame must carry the
#        committed kGlyphs2 bitmap of "VOLUME" at the overlay's own centred
#        origin, pixel for pixel, with nothing of that colour anywhere else in
#        the label band. The expected image is COMPUTED from foh_font.c, never
#        transcribed, so a font edit moves both sides together. Bound to
#        BEHAVIOUR by a second, independent witness: the overlay shells out to
#        `volume`/`brightness`, and this rig puts logging stand-ins on PATH, so
#        the exact 4-line transcript (two opening reads, then `volume set 60`
#        from RIGHT and `brightness set 40` from LEFT-after-DOWN) proves the
#        keypresses the single photographed frame cannot show.
#   [A2] opened MID-MATCH and driven to QUIT, which ends the match EARLY. That
#        shape is unreachable any other way in this leg: the VS loop's only
#        other early exits are the pause overlay (START — never pressed here)
#        and the finish arm (no --match-timer, so upstream's 480 s clock is
#        3600+ frames away).
#   [A3] opened in the FOH MENUS, in the second FOH phase the VS finish
#        creates.
#   [A4] opened inside a TARGET match, driven by the committed f06 flow. With
#        A1/A2 (the VS loop) and A3 (the FOH phase) that is ALL THREE of the
#        function's dispatch sites — they are genuinely different arms (the
#        target one additionally requires `!pin.start` so a simultaneous
#        MENU+START cannot shadow upstream's own endGame quit), and T1/T13
#        tell them apart.
#
# ARM B — the VS-FINISH block, reached the way a player reaches it (the real
#   matchTimer expiry, with --match-timer standing in for upstream's 480 s
#   clock — the SAME sim_tick.c arm, the SAME ml_sim_finish_hook, the same
#   banner and the same endGame), with a REAL audio plane loaded so the
#   block's sfx and music-mute lines are not silent no-ops:
#   [B1] the match ends at the EXPIRY frame, not at its --frames bound;
#   [B2] the banner shot carries the committed font's "TIME!" bitmap at the
#        exact origin tdev_vs_banner_text draws it at — a positional match, so
#        a different word, a shifted origin or a partial render all fail;
#   [B3] the 2500 ms hold ran IN FULL — a two-sided wall window that neither a
#        2300 ms nor a 2900 ms hold can satisfy (T7 and T11 prove each side,
#        200 ms outside the respective boundary; T9's 4500 ms is the gross
#        case);
#   [B4] the block's `g_mexit = MEX_CSS` really re-entered the FOH — a second
#        FOH phase at the tick bound with launched=0, which a single-phase run
#        cannot produce;
#   [B5] the sfx and music-mute lines were reachable at all (both summaries
#        present, i.e. both guards true) and the sfx call FIRED (T8 removes it
#        and the run's voice-start total drops by exactly one).
#
# THE DRAIN — all THREE changed sites are driven, not just the reachable ones:
#   [D1] the system-menu tail, [D4] the pause-overlay tail (opened on START,
#   resumed on B), and [D3] foh_dev.c's post-match tail, which additionally
#   exercises the dead-display latch under MLFK_HEADLESS_PRESENT_FAIL.
#
# BOTH DIRECTIONS, with ONE stated exception. Every witness above has a tooth —
# a COPY of the producer with exactly one thing removed or changed, which must
# FAIL the corresponding assertion (D2, D3-tooth, T1-T8 below). A witness that
# cannot fail proves nothing, and several of these exist because a reviewer
# showed the earlier version stayed green with the behaviour deleted.
#
# THE EXCEPTION, named here so the claim above is not read wider than it is:
# the finish block's MUSIC MUTE (`g_mix.music.on = 0`) is proven REACHED but
# not proven EFFECTIVE, and it has no tooth. `music.on` has exactly one reader
# in the whole tree — snd_mixer.h's fill, which runs on the audio callback
# thread — and the headless backend never starts that thread by design
# (platform_headless.c: accept-and-idle, so a host run can never masquerade as
# a device audio run). A deletion tooth would therefore be GREEN for a reason
# unrelated to the deletion, which is worse than no tooth. Closing it needs the
# DEVICE leg, and the device is offline for this increment; it is registered as
# a deferral in the writer's report rather than papered over.
#
# THE PRECONDITION THIS RIG DEPENDED ON (fixed first, in this same increment):
# three UNBOUNDED release drains (foh_pause.c x2, foh_dev.c x1) versus a host
# injector that HOLDS ITS LAST KEY STATE AT EOF BY DESIGN. A scripted run that
# ends holding a button entered a loop that could never exit, so the rig would
# have HUNG rather than failed — and for an autonomous loop a hang is strictly
# worse than a failure. All three now share foh_drain_release (foh_pause.h),
# which is bounded and names what it waited for. D1/D2 below prove both
# directions of that too, including that the UNBOUNDED shape really does hang.
#
# AND THE RIG ITSELF CANNOT HANG — stated at exactly its real width, because a
# contract that overstates its scope is the thing it was supposed to prevent
# (review-r3-r6 Medium).
#
# BOUNDED: every child that runs a BUILD, a DATA PRODUCER, the PROGRAM UNDER
# TEST, a RECURSIVE DELETE, or an INSTRUMENT whose answer this script turns
# into a decision — legs, the extractor, the pipeline stages, ffmpeg, the
# sim-data dump, every compile, every `rm -rf`, the mask matcher, the
# perturber, the flow converter, the manifest and recipe derivations, and the
# tree fingerprint. Each runs under run_bounded, which kills its whole PROCESS
# GROUP at a named deadline and says what it was waiting for; an exit trap
# reaps any group still live if the script is interrupted, and the sourced
# build recipe is grammar-checked so a new top-level command in it fails closed
# instead of running unbounded.
#
# NOT BOUNDED, and why that is sound: single-file `rm -f`/`cp`/`chmod` and the
# POSIX text utilities (grep/sed/awk/cmp/cat/wc/printf), all reading and
# writing LOCAL REGULAR FILES inside this repo. Their only blocking mode is a
# wedged filesystem, which would equally wedge every bounded child above — so
# bounding them buys nothing that the bounded steps do not already detect
# first, and it would put a fork+poll on every line of the script.
#
# NOT REACHABLE AT ALL: a daemon-owned docker container (below).
#
# ONE THING IS OUT OF REACH, and it is named rather than papered over: a
# DAEMON-owned docker container started by the extractor survives a kill of the
# local client. That is a resource leak, not a hang — this script still
# terminates on its bound — and the alternative (guessing which container is
# ours) was measured killing four unrelated ones. See docker_leak_note.
#
# HOST-ONLY, deliberately (the device is offline; the device legs are
# registered deferrals in the writer's report, not silent omissions). The
# host injector (MLFK_HEADLESS_KEYS, port/gfx/platform_headless.c) advances
# one quantum of virtual time per platform_poll() call, so injected input is
# a pure function of the POLL INDEX and every artifact below is byte-
# reproducible — which is what lets this script cmp shots instead of
# eyeballing them. The navigation is the COMMITTED f01 VS flow, converted by
# the frozen generator the device rigs use, never hand-authored here.
#
# NOT A GATE. Task-level tooth: `bash port/foh/check-live-arms.sh` ->
# `LIVE ARMS OK (...)`, exit 0. Runtime is dominated by builds (one host twin
# plus one single-perturbation copy per tooth) and by pace=1 legs that run in
# real time; budget ~25 minutes.
set -euo pipefail

FOH=port/foh
GFX=port/gfx
CAL=port/sim/calib
TABLES=pipeline/build/live-arms-tables
BUILD=$FOH/build/live-arms
FLOW=$FOH/flows/f01-vs-g01.flow
TFLOW=$FOH/flows/f06-target-t01.flow # the TARGET launch, for the third
                                     # system-menu dispatch site
DEVFOH=$FOH/check-device-foh.sh

fail() { echo "LIVE ARMS FAIL: $1" >&2; exit 1; }
grammar_die() { echo "LIVE ARMS FAIL: $1" >&2; exit 2; }
relay_lines() { sed 's/^/  | /'; }

made() { # every artifact must exist and be non-empty
  local f
  for f in "$@"; do
    [ -s "$f" ] || fail "expected artifact missing/empty: $f"
  done
}

# one_canonical <file> <needle> <anchored-regex> <ctx> — the PROCESS §3
# permissive-parse guard (carried verbatim from check-mexit-reentry.sh):
# `grep -q` accepts a file carrying a canonical line PLUS any number of stale,
# duplicated or MALFORMED lines that merely resemble it. The count of lines
# RESEMBLING the summary must equal the count MATCHING it exactly, and both
# must be 1.
one_canonical() { # <file> <needle> <regex> <ctx>
  local f="$1" needle="$2" re="$3" ctx="$4" nres nexact
  # A TERMINATED file first (review-r3-r5 Low): `grep` matches a final line
  # that has no trailing newline, so a run cut off mid-write can present a
  # canonical-looking last line that is actually a truncation. Decision-bearing
  # text has to be complete before it is parsed.
  if [ -s "$f" ] && [ "$(tail -c 1 "$f" | wc -l | tr -d ' ')" != 1 ]; then
    grammar_die "$ctx: $f does not end with a newline — it is truncated, so its
  last line cannot be trusted as canonical"
  fi
  nres="$(grep -cF "$needle" "$f")" || true
  nexact="$(grep -cE "$re" "$f")" || true
  if [ "$nres" != 1 ] || [ "$nexact" != 1 ]; then
    sed 's/^/  | /' "$f" >&2
    grammar_die "$ctx: $nres line(s) resemble '$needle' and $nexact match it
  exactly (want 1 and 1). A resemblance that is not an exact match is
  corruption; two exact matches mean the artifact carries more than one run."
  fi
}

# --- [0] THE RIG CANNOT HANG -------------------------------------------------
# Every child runs here, under a named deadline. A hang is not a slow pass: it
# consumes the session and reports nothing, so it is converted into a loud,
# specific failure that says exactly what was being waited for. `timeout(1)` is
# NOT used — it is absent on stock macOS, and a rig whose no-hang property
# depends on an optional binary does not have that property.
#
# PROCESS GROUPS, not single PIDs (review-r3-r1 High): `set -m` puts each
# background job in its OWN process group, and the timeout path kills the
# GROUP. That matters because the programs under test fork — the system menu
# shells out to `volume`/`brightness` via popen/system, and the build steps
# fork a compiler per TU — so killing one pid could leave descendants running
# and the rig would look bounded while a child kept the session busy. The
# subshell RUNS rather than `exec`s its command, so shell FUNCTIONS (the
# extracted build recipe) are bounded on the same path as binaries, and the
# group kill still reaches everything either way.
#
# Publishes RUN_RC (the child's exit status, or 124 when killed) and
# RUN_TIMEDOUT (0/1). Deliberately does NOT fail by itself: one leg below
# EXPECTS a timeout and asserts it.
RUN_RC=0
RUN_TIMEDOUT=0
RUN_GROUPS="" # groups believed ACTIVE right now, for the exit trap
run_groups_drop() { # <pid> — keep the trap's list to what is still live
  local keep="" p
  for p in $RUN_GROUPS; do [ "$p" = "$1" ] || keep="$keep $p"; done
  RUN_GROUPS="$keep"
}
run_bounded() { # <seconds> <what> <outfile> <errfile> -- cmd...
  local secs="$1" what="$2" out="$3" err="$4"
  shift 4
  set -m
  ( exec >"$out" 2>"$err"; "$@" ) &
  local pid=$!
  set +m
  RUN_GROUPS="$RUN_GROUPS $pid"
  local i=0 rc=0
  # Group liveness, not just the wrapper's: a child that forks and exits leaves
  # descendants in the same process group, and waiting only on the leader would
  # accept a step whose real work is still running and can still write evidence
  # (review-r3-r3 Medium). `kill -0 -PGID` succeeds while ANY member lives.
  while [ "$i" -lt "$secs" ]; do
    kill -0 -"$pid" 2>/dev/null || kill -0 "$pid" 2>/dev/null || break
    sleep 1
    i=$((i + 1))
  done
  if kill -0 -"$pid" 2>/dev/null || kill -0 "$pid" 2>/dev/null; then
    kill -9 -"$pid" 2>/dev/null || kill -9 "$pid" 2>/dev/null || true
    # BOUNDED reap, and `wait` ONLY once the leader is observed dead
    # (review-r3-r4 High). SIGKILL normally lands at once, but a child wedged
    # in uninterruptible I/O does not die, and an unconditional `wait` on it
    # blocks forever — inside the very function whose job is to stop this
    # script blocking forever. So: poll, and if the leader outlives the poll,
    # say so and DO NOT wait on it.
    local w=0 leaderGone=0
    while [ "$w" -lt 10 ]; do
      if ! kill -0 "$pid" 2>/dev/null; then leaderGone=1; break; fi
      sleep 1
      w=$((w + 1))
    done
    if [ "$leaderGone" = 1 ]; then
      wait "$pid" 2>/dev/null || true # already dead: returns immediately
    else
      echo "   [bounded] '$what' did not die on SIGKILL within 10s; NOT
   blocking on it. Something is wedged in the kernel; find it before trusting
   any later evidence in this run." >&2
    fi
    # Survivors are REPORTED and KEPT on the trap's list rather than dropped,
    # so the exit path signals them again instead of forgetting them.
    if kill -0 -"$pid" 2>/dev/null || [ "$leaderGone" = 0 ]; then
      echo "   [bounded] process group $pid still has members after SIGKILL" >&2
    else
      run_groups_drop "$pid"
    fi
    RUN_RC=124
    RUN_TIMEDOUT=1
    echo "   [bounded] KILLED after ${secs}s while waiting for: $what" >&2
    return 0
  fi
  wait "$pid" || rc=$?
  # The leader is gone; if the GROUP is not, something was left behind. Say so
  # and clear it rather than letting it mutate later evidence.
  if kill -0 -"$pid" 2>/dev/null; then
    echo "   [bounded] $what left descendants in its process group; killing them" >&2
    kill -9 -"$pid" 2>/dev/null || true
  fi
  run_groups_drop "$pid"
  RUN_RC=$rc
  RUN_TIMEDOUT=0
  return 0
}

# DOCKER: THE ONE THING A PROCESS-GROUP KILL CANNOT CONTAIN, stated rather
# than pretended away (review-r3-r2/r3 High). The extractor shells out to
# `docker run`; killing the local client leaves the DAEMON-owned container
# running, and nothing in this script can pass `--cidfile` to that producer
# because it belongs to another lane.
#
# An earlier version tried to contain it BY DIFFERENCE — snapshot `docker ps`,
# kill whatever appeared. That was actively dangerous and it was measured
# doing harm: on a clean exit it compared against a stale snapshot and killed
# FOUR unrelated containers, and even armed correctly it cannot tell a
# container this step started from one another lane started a second later, in
# a tree that routinely has several agents live. Killing another agent's build
# to tidy up after a timeout that may not have happened is a worse failure
# than the leak it prevents.
#
# So: no reaping. What matters for the no-hang property is that THIS SCRIPT
# terminates, and the bounded step guarantees that whether or not a container
# outlives it. A leaked container is a resource leak, not a hang, and the
# operator is told exactly where to look.
docker_leak_note() { # <what>
  command -v docker > /dev/null 2>&1 || return 0
  echo "   [bounded] NOTE: '$1' shells out to docker. Killing the client does
   not stop a daemon-owned container, and this rig deliberately does not guess
   which container is its own. If that step timed out, check \`docker ps\` and
   stop the leftover yourself." >&2
}

# Everything that is not a leg — the extractor, the pipeline, the sim-data
# dump and every compile — goes through here. A build that wedges eats the
# session exactly as a stuck drain would, so "the rig cannot hang" has to mean
# ALL of it, not just the parts that run the program under test.
bounded_step() { # <seconds> <what> <logbase> -- cmd...
  local secs="$1" what="$2" log="$3"
  shift 3
  run_bounded "$secs" "$what" "$log" "$log.err" "$@"
  if [ "$RUN_TIMEDOUT" = 1 ]; then
    docker_leak_note "$what"
    fail "$what did not finish inside ${secs}s and was killed. A build or data
  step that hangs eats the session exactly as an unbounded drain would, which
  is why it is bounded; raise the bound only after proving the step is merely
  slow."
  fi
  if [ "$RUN_RC" != 0 ]; then
    [ -s "$log" ] && sed 's/^/  | /' "$log" >&2
    [ -s "$log.err" ] && sed 's/^/  | /' "$log.err" >&2
    fail "$what failed (rc $RUN_RC)"
  fi
  return 0
}

# The same bound for the small INSTRUMENTS whose answer this script consumes on
# stdout — the font/pixel matcher, the perturber, the flow converter, the
# manifest derivation, the tree fingerprint (review-r3-r2 High: those bypassed
# the bound entirely, so a regression in any of them could hang an unattended
# run). Publishes the captured stdout in BCAP_OUT and returns the child's rc,
# so callers keep their existing `|| fail ...` shape and still see stderr.
#
# NEVER CALL A bcap USER FROM `$( )` (review-r3-r5 High, and it was reproduced:
# command substitution runs the caller in a SUBSHELL, so run_bounded's
# RUN_GROUPS update never reaches the parent — and the parent's trap is what
# reaps a stuck group. Every helper below therefore publishes its answer in a
# GLOBAL and is called as a statement.
BCAP_OUT=""
BCAP_QUIET=0 # set by callers that EXPECT a nonzero rc (the mask teeth)
BCAP_DIR=$FOH/build/live-arms-cap
bcap() { # <seconds> <what> <tag> -- cmd...
  local secs="$1" what="$2" tag="$3"
  shift 3
  mkdir -p "$BCAP_DIR"
  local o="$BCAP_DIR/$tag.out" e="$BCAP_DIR/$tag.err"
  # ORDER MATTERS HERE (review-r3-r6 High). Three things, all of which were
  # wrong at once in the first version:
  #   (1) the sinks are REMOVED first. `exec >"$o"` on a leftover FIFO blocks
  #       the bounded writer at open, and unlinking is the only way to be sure
  #       what is opened is a fresh regular file.
  #   (2) the timeout is checked BEFORE anything reads the output — an
  #       unbounded `cat` of a FIFO after a killed writer hangs forever, in the
  #       helper whose job is to prevent exactly that.
  #   (3) the sink must be a REGULAR file and bounded in size before it is
  #       read into a shell variable.
  rm -f "$o" "$e"
  run_bounded "$secs" "$what" "$o" "$e" "$@"
  if [ "$RUN_TIMEDOUT" = 1 ]; then
    fail "$what did not finish inside ${secs}s and was killed — an instrument
  this script reads an answer from must never be able to hang it."
  fi
  if [ -L "$o" ] || [ ! -f "$o" ]; then
    fail "$what: its output sink $o is not a regular file — refusing to read it"
  fi
  local osz
  osz="$(wc -c < "$o" | tr -d ' ')"
  [ "$osz" -le 1048576 ] \
    || fail "$what: wrote ${osz} bytes to its output sink (cap 1 MiB). These
  instruments answer with a number or nothing; that much output is corruption."
  BCAP_OUT="$(cat "$o")"
  if [ "$RUN_RC" != 0 ]; then
    if [ "$BCAP_QUIET" != 1 ] && [ -s "$e" ]; then
      sed 's/^/  | /' "$e" >&2
    fi
    return "$RUN_RC"
  fi
  return 0
}

# Bounded `rm -rf`. A stale build tree can be enormous or sit on a wedged
# filesystem, and an unbounded recursive delete is exactly the unattended-hang
# shape this rig is built against (review-r3-r5 High).
# Its logs live in a SEPARATE directory, never inside anything it might be
# asked to remove (review-r3-r7 High: with the logs under BCAP_DIR, clearing
# BCAP_DIR deleted the log the remover was writing).
BRM_TAG=0
RMLOG_DIR=$FOH/build/live-arms-rmlog
brm() { # <path...>
  BRM_TAG=$((BRM_TAG + 1))
  mkdir -p "$RMLOG_DIR"
  bounded_step 300 "removing stale build output (call $BRM_TAG)" \
    "$RMLOG_DIR/rm$BRM_TAG.log" rm -rf "$@"
}

# --- [0a] run lock (mkdir-atomic, NO reclaim — the check-foh-flows pattern) ---
LOCK=$FOH/build/live-arms.lock
mkdir -p "$FOH/build"
# THE LOCK COMES FIRST, before anything is deleted (review-r3-r7 High). An
# earlier version cleared the capture directory ABOVE this line, so a second
# invocation would delete a LIVE run's sinks on its way to discovering that it
# could not have the lock.
mkdir "$LOCK" 2>/dev/null || fail "another run holds $LOCK (remove it only if
  you have proven no other run is live)"
# The trap releases the lock AND reaps every process group that is still ACTIVE
# (run_bounded drops each one as it completes, so this never signals a recycled
# pid), on a normal exit, a `fail`, Ctrl-C or a TERM. Without it, aborting this
# script mid-leg would leave a paced foh_dev running for minutes.
live_arms_cleanup() {
  local p
  for p in $RUN_GROUPS; do kill -9 -"$p" 2>/dev/null || true; done
  rmdir "$LOCK" 2>/dev/null || true
}
trap live_arms_cleanup EXIT
trap 'live_arms_cleanup; exit 130' INT
trap 'live_arms_cleanup; exit 143' TERM

# NOW the capture directory can be cleared — under the lock, so no live run's
# sinks are at risk, and boundedly, with the remover's OWN log living outside
# the directory it is removing (review-r3-r7 High). bcap's sinks must never be
# inherited from a previous run: a leftover FIFO or symlink there is exactly
# the hang bcap's ordering rules are written against.
mkdir -p "$RMLOG_DIR"
brm "$BCAP_DIR"

# --- [0b] no-commit guard (content, not status) ------------------------------
# Carried from check-mexit-reentry.sh, and for the same reason: `git status
# --porcelain` alone is blind in this tree, because nearly every file a lane
# touches is ALREADY `M`, so rewriting one leaves the status output
# byte-identical. The fingerprint folds in the tracked DIFF BYTES and a hash of
# every untracked-but-not-ignored file's CONTENT. Every component must succeed
# — a swallowed failure would produce a plausible hash from partial input.
# BOUNDED, like every other producer: `git` can block indefinitely on an index
# lock held by another agent in this tree, and this runs before anything else
# has produced evidence, so a hang here would be the most confusing one
# possible (review-r3-r2 High).
FP_TAG=0
FP_OUT="" # published in a global, never through `$( )` — see the bcap note
tree_fingerprint() {
  FP_TAG=$((FP_TAG + 1))
  bcap 180 "the working-tree fingerprint (call $FP_TAG)" "fingerprint$FP_TAG" \
    bash -c '
      set -euo pipefail
      # HEAD and the STAGED diff are in the fingerprint (review-r3-r3 Medium):
      # `git add` moves bytes from the unstaged diff into the index, leaving
      # BOTH `status --porcelain` (same letters, different column) and
      # `git diff` capable of looking unchanged, and a commit or checkout in
      # another lane moves HEAD under this run entirely. Four inputs: HEAD,
      # staged, unstaged, untracked content.
      head="$(git rev-parse HEAD)"
      status="$(git status --porcelain)"
      # --binary on BOTH diffs (review-r3-r4 Medium): a textual diff of an
      # already-dirty BINARY file says only "Binary files differ", so its
      # contents could change without moving this digest.
      staged="$(git diff --cached --binary)"
      diff="$(git diff --binary)"
      files="$(git ls-files -o --exclude-standard)"
      files="$(printf "%s\n" "$files" | grep -v "^\.tokensave/" || true)"
      if [ -n "$files" ]; then
        # content AND the executable bit: this rig is itself an untracked file
        # while it is being written, and a chmod on it is a real change that a
        # content-only hash cannot see.
        hashes="$(printf "%s\n" "$files" | tr "\n" "\0" | xargs -0 shasum -a 256)"
        modes="$(printf "%s\n" "$files" | while IFS= read -r f; do
          [ -n "$f" ] || continue
          # `--` is REQUIRED on the second one: bash reads a format string
          # that starts with `-` as an option and dies with "invalid option",
          # so this guard failed CLOSED for any tree holding an untracked
          # NON-executable file — i.e. for every lane that adds a new .c
          # alongside its check. Found while running this rig with A27s new
          # witness in the tree (A39, 2026-08-24); it is a repair of the
          # instrument, not a relaxation of anything it asserts.
          if [ -x "$f" ]; then printf "x %s\n" "$f"; else printf -- "- %s\n" "$f"; fi
        done)"
      else
        hashes=""
        modes=""
      fi
      printf "%s\n%s\n%s\n%s\n%s\n%s\n" "$head" "$status" "$staged" "$diff" \
        "$hashes" "$modes" | shasum -a 256 | cut -d" " -f1
    ' || return 1
  FP_OUT="$BCAP_OUT"
  return 0
}
tree_fingerprint \
  || fail "could not fingerprint the working tree (a component command failed —
  the guard fails CLOSED rather than reporting a hash of partial input)"
git_dirty_before="$FP_OUT"
[[ "$git_dirty_before" =~ ^[0-9a-f]{64}$ ]] \
  || fail "tree fingerprint '$git_dirty_before' is not a sha256 digest"

# --- [0c] THE measurement instrument -----------------------------------------
# It reads COMMITTED bytes and measures PRODUCED bytes, and it carries no
# transcribed expectation: the expected image is DERIVED from the same font
# table the renderer compiles, at the same origin the caller draws at, so a
# font edit moves the expectation and the measurement together and cannot
# silently split them.
#
# POSITIONAL, not aggregate (review-r3-r1 High, and it was a real hole): an
# earlier form compared only a COUNT of lit pixels, and a count identifies
# neither the string nor its position — "TIME!" and "VOLE!" have the same 64
# lit texels in the 5x7 face, and a banner drawn at the wrong origin keeps its
# count exactly. So this compares the whole centred bitmap pixel for pixel:
# every expected pixel must carry the drawn colour, AND no pixel of that colour
# may appear anywhere else in the band.
#
# WHAT IT CATCHES, stated at exactly its width (review-r3-r4 Low corrected an
# earlier over-claim): a MISTYPED string, a DISPLACED origin, a PARTIAL render,
# and any EXTRA drawn pixel inside the band. What it cannot catch is a second
# draw of the SAME string at the SAME origin — opaque overdraw is
# framebuffer-idempotent, so no pixel test of any kind can see it — nor extra
# ink outside the band. Neither is a defect this rig is about.
#
#   mask_match <ppm> <font.c> <1|2> <string> <scale> <x0> <y0> <r> <g> <b>
#              <bandY0> <bandY1>
#
# Face geometry is the blitter's own (port/foh/foh_font.c):
#   table 1 (foh_text)  — 5 columns x 7 rows, bit 0x10>>c, advance 6*scale;
#   table 2 (foh_text2) — 6 columns x 9 rows, bit 0x20>>c, advance 7*scale.
# Colour is compared EXACTLY rather than by threshold, because both things
# read here are solid unantialiased RastCol fills whose 565->888 round trip is
# a single known triple. Prints the matched pixel count.
#
# TWO DISTINCT FAILURE STATUSES (review-r3-r5 Medium): a tooth that accepts
# "mask_match failed somehow" is earned by a truncated PPM, a wrong image size,
# a font-parser error or any other corruption, none of which is the deletion it
# claims to prove. So:
#   rc 3 — INSTRUMENT/ARTIFACT error (bad shot, bad font table, bad arguments);
#   rc 4 — a genuine MASK MISMATCH, which is the only status a tooth may accept.
# Publishes the matched pixel count in MASK_PX; never call it from `$( )`.
MASK_TAG=0
MASK_PX=""
mask_match() {
  MASK_TAG=$((MASK_TAG + 1))
  bcap 120 "the mask matcher (call $MASK_TAG)" "mask$MASK_TAG" \
  node -e '
    const fs = require("fs");
    const a = process.argv.slice(1);
    const [ppm, src, table, str] = a;
    const scale = +a[4], x0 = +a[5], y0 = +a[6];
    const R = +a[7], G = +a[8], B = +a[9], by0 = +a[10], by1 = +a[11];
    // Optional 13th argument: "count" skips the mask entirely and reports how
    // many pixels of the sampled colour the band holds. The no-draw controls
    // use it to require ZERO, which excludes a background that happens to
    // supply part of a glyph (review-r3-r6 Medium).
    const countOnly = a[12] === "count";
    // rc 3 = instrument/artifact error; rc 4 = the mask genuinely did not match
    const die = (m) => { console.error("mask_match: " + m); process.exit(3); };
    const nomatch = (m) => { console.error("mask_match MISMATCH: " + m); process.exit(4); };
    const two = table === "2";
    const name = two ? "kGlyphs2" : "kGlyphs";
    const nrow = two ? 9 : 7, ncol = two ? 6 : 5;
    const adv = two ? 7 : 6, topbit = two ? 0x20 : 0x10;

    // --- the font table, parsed AS DATA -------------------------------------
    const text = fs.readFileSync(src, "utf8");
    // Literal regexes (never RegExp-from-string): the two initializer names
    // differ only by a trailing digit, and /\bkGlyphs\s*\[\]/ cannot match
    // kGlyphs2[] because a digit is neither \s nor [.
    const arr = two ? text.match(/\bkGlyphs2\s*\[\]\s*=\s*\{([\s\S]*?)\n\};/)
                    : text.match(/\bkGlyphs\s*\[\]\s*=\s*\{([\s\S]*?)\n\};/);
    if (!arr) die("no " + name + " initializer in " + src);
    const body = arr[1];
    // \x27 is the single quote, spelled numerically so this whole program can
    // live inside one shell single-quoted argument.
    const ENTRY = /\{\x27(\\.|[^\x27\\])\x27,\s*\{\s*([^}]*?)\s*\}\s*\}/g;
    const declared = (body.match(/\{\x27/g) || []).length;
    const map = new Map();
    let m, n = 0;
    while ((m = ENTRY.exec(body)) !== null) {
      let ch = m[1];
      if (ch === "\\\x27") ch = "\x27";
      else if (ch === "\\\\") ch = "\\";
      else if (ch.length === 2 && ch[0] === "\\") ch = ch[1];
      // Duplicate rejection (review-r3-r1 Medium): a Map keeps the LAST entry
      // while the shipping glyph_for/glyph2_for returns the FIRST, so a
      // duplicated glyph would let the instrument and the renderer disagree.
      if (map.has(ch)) die(name + " declares " + JSON.stringify(ch) + " twice");
      const rows = m[2].split(",").map((s) => s.trim()).filter((s) => s.length);
      if (rows.length !== nrow) die("glyph " + JSON.stringify(ch) + " has " + rows.length + " rows (want " + nrow + ")");
      map.set(ch, rows.map((h) => {
        if (!/^0x[0-9a-fA-F]{1,2}$/.test(h)) die("bad row token " + JSON.stringify(h));
        return parseInt(h, 16);
      }));
      n++;
    }
    if (n !== declared) die("parsed " + n + " glyphs != " + declared + " declared entries in " + name);
    let residue = body.replace(/\{\x27(\\.|[^\x27\\])\x27,\s*\{\s*([^}]*?)\s*\}\s*\}/g, "");
    residue = residue.replace(/\/\/[^\n]*/g, "").replace(/[\s,]/g, "");
    if (residue.length !== 0) die("unparsed " + name + " content " + JSON.stringify(residue.slice(0, 40)));

    // --- the expected image -------------------------------------------------
    const want = new Set();
    let px = x0;
    for (const c of str) {
      const g = map.get(c);
      if (!g) die(name + " has no glyph " + JSON.stringify(c));
      for (let r = 0; r < nrow; r++) {
        for (let col = 0; col < ncol; col++) {
          if (!(g[r] & (topbit >> col))) continue;
          for (let sy = 0; sy < scale; sy++) {
            for (let sx = 0; sx < scale; sx++) {
              want.add((y0 + r * scale + sy) * 240 + (px + col * scale + sx));
            }
          }
        }
      }
      px += adv * scale;
    }
    if (want.size === 0) die(JSON.stringify(str) + " renders no pixels at all");

    // --- the produced image -------------------------------------------------
    const buf = fs.readFileSync(ppm);
    const hm = buf.slice(0, 24).toString("latin1").match(/^P6\n(0|[1-9][0-9]*) (0|[1-9][0-9]*)\n255\n/);
    if (!hm) die(ppm + " is not a canonical P6/255 shot");
    const w = +hm[1], h = +hm[2], off = hm[0].length;
    if (w !== 240 || h !== 240) die(ppm + " is " + w + "x" + h + " (want 240x240)");
    if (buf.length - off !== w * h * 3) die(ppm + " pixel bytes " + (buf.length - off) + " != " + (w * h * 3));
    if (!(by0 >= 0 && by1 > by0 && by1 <= h)) die("band " + by0 + ".." + by1 + " out of range");
    const isDrawn = (i) => {
      const o = off + i * 3;
      return buf[o] === R && buf[o + 1] === G && buf[o + 2] === B;
    };
    if (countOnly) {
      let n = 0;
      for (let y = by0; y < by1; y++) {
        for (let x = 0; x < w; x++) if (isDrawn(y * 240 + x)) n++;
      }
      process.stdout.write(String(n));
      process.exit(0);
    }
    // (1) every expected pixel is drawn
    for (const i of want) {
      if (!isDrawn(i)) nomatch("expected pixel (" + (i % 240) + "," + Math.floor(i / 240) + ") is not " + R + "," + G + "," + B + " in " + ppm + " — the render is missing, mistyped or displaced");
    }
    // (2) nothing of that colour anywhere else in the band. This is what makes
    //     the match POSITIONAL: a shifted render lights pixels the mask does
    //     not contain, and a longer string does too.
    for (let y = by0; y < by1; y++) {
      for (let x = 0; x < w; x++) {
        const i = y * 240 + x;
        if (isDrawn(i) && !want.has(i)) nomatch("unexpected drawn pixel (" + x + "," + y + ") outside the expected mask in " + ppm);
      }
    }
    process.stdout.write(String(want.size));
  ' "$@" || return $?
  MASK_PX="$BCAP_OUT"
  return 0
}

# The ONLY way a tooth may consume a mask failure: it must be rc 4, the
# mismatch status, never rc 3 (a broken instrument or a corrupt artifact).
mask_must_mismatch() { # <ctx> -- <mask_match args...>
  local ctx="$1" rc=0
  shift
  BCAP_QUIET=1
  mask_match "$@" || rc=$?
  BCAP_QUIET=0
  [ "$rc" = 4 ] \
    || fail "$ctx: expected a MASK MISMATCH (rc 4) and got rc $rc. rc 0 means
  the witness is BLIND to the perturbation; rc 3 means the instrument or the
  artifact is broken and this tooth proved nothing either way."
  return 0
}

# perturb <src> <dst> <needle> <replacement> <tag> — one single-occurrence
# substitution, asserted unique BEFORE and asserted to have changed the bytes
# AFTER. A tooth built from a copy that is byte-identical to the original, or
# from a needle that matched twice, proves nothing.
#
# SOURCE IDENTITY, checked at every use (review-r3-r7 Medium). This rig runs
# for ~25 minutes and builds a dozen binaries from the same three sources; the
# whole "exactly one change" argument collapses if one of those sources is
# edited (even transiently) between the reference build and a tooth build, and
# an endpoint-only tree fingerprint cannot see an edit that is reverted before
# it runs. So each perturbable source is hashed ONCE at startup and re-verified
# immediately before every copy is derived from it.
SRC_PINS=""
SRC_HASH_TAG=0
src_hash() { # <file> -> BCAP_OUT (bounded, like every other instrument)
  SRC_HASH_TAG=$((SRC_HASH_TAG + 1))
  bcap 120 "hashing $1 for the source-identity pin" "srchash$SRC_HASH_TAG" \
    bash -c 'shasum -a 256 "$1" | cut -d" " -f1' _ "$1" \
    || fail "could not hash $1 for the source-identity pin"
}
src_pin_snapshot() { # <file...>
  local f
  for f in "$@"; do
    src_hash "$f"
    SRC_PINS="$SRC_PINS $f=$BCAP_OUT"
  done
}
src_pin_verify() { # <file>
  local f="$1" want="" pair
  for pair in $SRC_PINS; do
    case "$pair" in "$f="*) want="${pair#*=}" ;; esac
  done
  [ -n "$want" ] \
    || fail "no startup hash recorded for $f — every source this rig builds
  from or measures against must be snapshotted before the first build"
  src_hash "$f"
  [ "$BCAP_OUT" = "$want" ] \
    || fail "$f CHANGED DURING THIS RUN ($want -> $BCAP_OUT). Every binary here
  is built to differ from the reference in exactly one line, and that argument
  is void the moment a base source moves mid-run. Re-run on a quiet tree."
}
# EVERY pinned input, re-verified together (review-r3-r8 Medium: verifying only
# the file being perturbed left the OTHER shared TUs, the font table the pixel
# expectations come from, and the extracted build recipe free to move between
# the reference build and a tooth build — which is exactly the confound the pin
# exists to exclude). Called before the reference build and before every tooth
# build, so "these two binaries differ in one line" is checked, not assumed.
src_pin_verify_all() {
  local pair
  for pair in $SRC_PINS; do src_pin_verify "${pair%%=*}"; done
}

perturb() { # <src> <dst> <needle> <replacement> <tag>
  src_pin_verify "$1"
  mkdir -p "$(dirname "$2")"
  bcap 120 "the perturber ($5)" "perturb-$5" \
  node -e '
    const fs = require("fs");
    const [src, dst, needle, repl, tag] = process.argv.slice(1);
    const raw = fs.readFileSync(src, "utf8");
    const n = raw.split(needle).length - 1;
    if (n !== 1) { console.error(tag + ": found " + n + " occurrences of the needle (want exactly 1)"); process.exit(1); }
    // Function replacement, not a string one: a "$" in the replacement text
    // would otherwise be read as a capture reference and silently mangle it.
    const out = raw.replace(needle, () => repl);
    if (out === raw) { console.error(tag + ": substitution was a no-op"); process.exit(1); }
    fs.writeFileSync(dst, out);
  ' "$1" "$2" "$3" "$4" "$5" || fail "$5: could not derive the perturbed copy"
  made "$2"
  cmp -s "$2" "$1" && fail "$5: the perturbed copy is byte-identical to $1 (dead tooth)"
  return 0
}

# TWO SEPARATE LEDGERS (review-r3-r4 Medium: an earlier single `teeth` counter
# folded D1 and D3 into it, and those are POSITIVE production legs — a run of
# the real build, asserting that it behaves — not perturbed negative controls.
# Counting them as teeth overstated the negative coverage by two).
#   drains — positive witnesses that a real bounded drain fires and names
#            itself, one per drain SITE that this rig can reach;
#   teeth  — perturbed COPIES that must FAIL an assertion the real build passes.
drains=0
teeth=0

# --- [1] data planes ---------------------------------------------------------
# Every step here is bounded: a wedged docker build or a stuck ffmpeg is the
# same session-eating failure an unbounded drain is.
echo "=== [1] data planes (tables/stages/targets/assets + audio + simdata)"
brm "$TABLES"
mkdir -p "$BUILD-pre"
bounded_step 900 "the extractor build" "$BUILD-pre/extractor.log" \
  bash pipeline/extractor/build-extractor.sh
bounded_step 900 "the pipeline data stages" "$BUILD-pre/pipeline.log" \
  node pipeline/run.js --only animations,tables,stages,targets,assets \
    --out "$TABLES"
made "$TABLES/ml_tables.c" "$TABLES/ml_stages.c" "$TABLES/ml_targets.c" \
     "$TABLES/assets/menu.img1" "$TABLES/anim_2_fox.bin"
# ARTIFACT INTEGRITY, using the pipeline's OWN verifier rather than trusting
# that a run which exited 0 wrote what its manifest says (review-r3-r7 Medium).
# It re-hashes every emitted artifact against the manifest entry, so a
# truncated or half-written data plane is caught before a single binary
# consumes it.
#
# The FULL expected-coverage contract (pipeline/lib/check-expected.js) is
# deliberately NOT run here, and that is a scope statement rather than an
# omission: it asserts the whole M1 pin — every stage, all 754 animation files,
# the audio counts — against a COMPLETE pipeline run, which is what
# `bash pipeline/verify_pipeline.sh` (the M1 EXIT GATE) exists to do. This rig
# runs stage SUBSETS on purpose, so that checker cannot pass here by
# construction; duplicating a milestone gate inside a task-level tooth would
# also triple its runtime. Integrity is what this rig needs and is what it now
# gets.
bounded_step 300 "the pipeline artifact-integrity check (data planes)" \
  "$BUILD-pre/verify-artifacts.log" \
  bash -c 'cd pipeline && node lib/verify-artifacts.js "../$1"' _ "$TABLES"
# foh_render's artwork resolution is fatal on a MISSING file by design, so
# point it at THIS run's freshly generated artifact (check-foh-flows.sh's rule).
export MLFK_MENU_IMG1="$PWD/$TABLES/assets/menu.img1"
brm "$BUILD"
mkdir -p "$BUILD"
bounded_step 900 "the sim-data dump" "$BUILD/simdata.log" \
  node "$CAL/dump-sim-data.js" --out "$BUILD/simdata.txt"
made "$BUILD/simdata.txt"

GFXDATA=$GFX/gfxdata-frozen.txt
VFXDATA=$GFX/vfxdata-frozen.txt
GLYPHS=$GFX/vfxglyphs-frozen.txt
made "$GFXDATA" "$VFXDATA" "$GLYPHS" "$FLOW" "$TFLOW" "$FOH/foh_font.c"

# Every source this rig builds a perturbed copy of, plus the font table its
# pixel expectations are derived from, pinned before the first build.
src_pin_snapshot "$FOH/foh_dev.c" "$FOH/foh_pause.c" "$GFX/gfx_overlay.c" \
  "$FOH/foh_font.c" "$DEVFOH"

# --- [1a] the AUDIO plane ----------------------------------------------------
# WHY A RIG THAT IS ABOUT TWO CODE BLOCKS BUILDS AN AUDIO PACK (review-r3-r1
# High, and it was a real gap): two of the VS-finish block's lines are
# `foh_snd(timeUp ? "time" : "game")` and the music mute, and BOTH are
# no-ops without `--sndpack` / `--music-manifest` — `foh_snd` returns
# immediately when `g_have_audio` is false, and the mute body sits under
# `if (g_have_music)`. Running the arm without an audio plane would have left
# those lines exactly as unexecuted as they were before this rig existed,
# which is the one thing R3 exists to end.
#
# The recipe is check-device-foh.sh's, followed by path: the pipeline's own
# audio stage (its ffmpeg version, argv and artifact hashes are pinned inside
# pipeline/expected.json, so a different ffmpeg fails THERE, loudly) then
# port/gfx/pack-snd.js. The artifact SHAs are deliberately NOT re-pinned here —
# check-device-foh.sh owns those pins, and a second copy would be a second
# thing to drift. What is asserted here is byte-stability across two packs and
# the producer's own anchored success grammar.
echo "=== [1a] audio plane (pipeline audio stage + pack-snd.js, byte-stable x2)"
AUDIO_OUT=$BUILD/audio
DIST="${MELEELIGHT_CLONE:-$HOME/.cache/meleelight-funkey-s/upstream}"
brm "$AUDIO_OUT"
bounded_step 1800 "the pipeline audio stage (ffmpeg)" "$BUILD/audio-stage.log" \
  node pipeline/run.js --only audio --dist "$DIST" --out "$AUDIO_OUT"
made "$AUDIO_OUT/sounds.json" "$AUDIO_OUT/audio/music/battlefield.pcm"
bounded_step 300 "the pipeline artifact-integrity check (audio plane)" \
  "$BUILD/verify-artifacts-audio.log" \
  bash -c 'cd pipeline && node lib/verify-artifacts.js "../$1"' _ "$AUDIO_OUT"
for side in a b; do
  rm -f "$BUILD/sndpack-$side.bin"
  bounded_step 300 "pack-snd.js (side $side)" "$BUILD/pack-$side.log" \
    node "$GFX/pack-snd.js" "$AUDIO_OUT" "$BUILD/sndpack-$side.bin"
  made "$BUILD/sndpack-$side.bin"
  one_canonical "$BUILD/pack-$side.log" 'pack-snd OK' \
    '^pack-snd OK count=180 dataBytes=(0|[1-9][0-9]{0,11}) fileBytes=(0|[1-9][0-9]{0,11})$' \
    "pack-snd verdict (side $side)"
done
cmp "$BUILD/sndpack-a.bin" "$BUILD/sndpack-b.bin" \
  || fail "sndpack is not byte-stable across two packs"
cp "$BUILD/sndpack-a.bin" "$BUILD/sndpack.bin"
made "$BUILD/sndpack.bin"
# The music manifest, in foh_dev's strict `track <tok> <path> <volbits> <so>
# <sd> <lo> <ld>` grammar, with every field read out of THIS run's sounds.json
# rather than transcribed. Three rows, one per track a leg here can demand and
# no more (foh_dev refuses a launch whose track is missing, loudly, which is
# how this list was measured rather than guessed): `menu` because the FOH phase
# programs it at boot, `battlefield` because f01 launches there, and
# `targettest` because the A4 target leg switches to it at the TLAUNCH seam.
bcap 120 "the music-manifest derivation" music-manifest \
node -e '
  const fs = require("fs");
  const die = (m) => { console.error(m); process.exit(1); };
  const s = JSON.parse(fs.readFileSync(process.argv[1], "utf8"));
  if (s.formatVersion !== 1) die("sounds.json formatVersion != 1");
  const out = [];
  for (const key of ["menu", "battlefield", "targettest"]) {
    const e = s.music && s.music[key];
    if (!e) die("music." + key + " missing");
    if (e.blob !== "audio/music/" + key + ".pcm") die("non-canonical blob path for " + key);
    const sp = e.sprite;
    if (!sp || !Array.isArray(sp.start) || sp.start.length !== 2 ||
        !Array.isArray(sp.loop) || sp.loop.length !== 2) die("sprite shape for " + key);
    for (const v of [...sp.start, ...sp.loop]) {
      if (!Number.isInteger(v) || v < 0) die("sprite window not a non-negative integer");
    }
    if (!/^[0-9a-f]{16}$/.test(e.volume.bits)) die("volume bits grammar for " + key);
    out.push(["track", key, process.argv[2] + "/" + e.blob, e.volume.bits,
              sp.start[0], sp.start[1], sp.loop[0], sp.loop[1]].join(" "));
  }
  fs.writeFileSync(process.argv[3], out.join("\n") + "\n");
' "$AUDIO_OUT/sounds.json" "$PWD/$AUDIO_OUT" "$BUILD/music.txt" \
  || fail "could not derive the music manifest from this run's sounds.json"
made "$BUILD/music.txt"
[ "$(wc -l < "$BUILD/music.txt" | tr -d ' ')" = 3 ] \
  || fail "the derived music manifest does not carry exactly 3 tracks"
echo "   audio plane: $(wc -c < "$BUILD/sndpack.bin") B sndpack + 3 music tracks"

# --- [2] host twin build, USING check-device-foh.sh's OWN recipe -------------
# The TU list is NOT copied here. It is EXTRACTED from check-device-foh.sh and
# sourced, so there is exactly one definition of "how foh_dev.c is built for
# the host" in the tree and this rig cannot drift from the one that ships.
# check-mexit-reentry.sh extracts the same region the same way; the anchor is
# single-match-asserted so a move dies loudly instead of silently building
# something else.
echo "=== [2] host twin build (recipe extracted from check-device-foh.sh)"
made "$DEVFOH"
RECIPE=$BUILD/recipe.sh
nstart="$(grep -cxF 'CFLAGS_COMMON=(-ffp-contract=off -Wall -Wextra -Werror' \
  "$DEVFOH")" || true
[ "$nstart" = 1 ] || grammar_die "check-device-foh.sh has $nstart CFLAGS_COMMON
  anchor lines (want exactly 1) — the extracted build recipe moved"
awk '
  /^CFLAGS_COMMON=\(-ffp-contract=off -Wall -Wextra -Werror$/ { inr = 1 }
  inr { print }
  inr && /^}$/ { exit }
' "$DEVFOH" > "$RECIPE.raw"
made "$RECIPE.raw"
# The extracted region is not all declarations: it carries ONE top-level
# command, the raster.o compile, which runs at SOURCE time and would therefore
# be the single unbounded child in this script (review-r3-r3 High). Rather
# than re-implement that line here (which is the drift the extraction exists to
# prevent), wrap it in place — bounded_step is already defined in this shell,
# so the sourced file simply calls it. Single-occurrence-asserted like every
# other substitution.
bcap 120 "the recipe top-level wrap" recipe-toplevel \
node -e '
  const fs = require("fs");
  const [src, dst] = process.argv.slice(1);
  let raw = fs.readFileSync(src, "utf8");
  // Both TOP-LEVEL commands the extracted region carries. Each is wrapped in
  // place rather than re-implemented here, so the bound is added without the
  // recipe drifting from the one check-device-foh.sh ships.
  const wrap = [
    ["rm -f \"$BUILD/raster.o\" \"$BUILD/foh_dev_headless\"",
     "bounded_step 300 \"the recipe pre-clean\" \"$BUILD/recipe-clean.log\" "],
    ["cc -O3 \"${CFLAGS_COMMON[@]}\" -c \"$GFX/raster.c\" -o \"$BUILD/raster.o\"",
     "bounded_step 1200 \"the raster.o compile\" \"$BUILD/raster-build.log\" "],
  ];
  for (const [needle, prefix] of wrap) {
    const n = raw.split(needle).length - 1;
    if (n !== 1) { console.error("recipe: " + n + " occurrences of " + JSON.stringify(needle.slice(0, 30)) + " (want 1)"); process.exit(1); }
    raw = raw.replace(needle, () => prefix + needle);
  }
  fs.writeFileSync(dst, raw);
' "$RECIPE.raw" "$RECIPE" || fail "could not bound the recipe's top-level commands"
made "$RECIPE"
# FAIL CLOSED ON ANY NEW TOP-LEVEL CONSTRUCT (review-r3-r5/r6 High). Sourcing
# this file EXECUTES everything at nesting depth 0, so a command added upstream
# silently becomes an unbounded child of this script.
#
# The first version of this guard tested `^[a-z_]+ ` at column 0, which was
# permissive in five separate ways a reviewer enumerated: leading TABS (the
# shell does not care about indentation), commands with no arguments,
# UPPERCASE names, `:` / `.` / `./path`, and assignments containing command
# substitutions. So the check is now an EXACT allow-list over a real
# depth-tracking parse: every depth-0 line must be blank, a comment, or one of
# the four constructs this region is known to contain — and `bounded_step` is
# the only executable form permitted.
bcap 120 "the recipe top-level grammar check" recipe-grammar \
node -e '
  const fs = require("fs");
  const lines = fs.readFileSync(process.argv[1], "utf8").split("\n");
  const ALLOW = [
    /^\s*$/,                                              // blank
    /^\s*#/,                                              // comment
    /^CFLAGS_COMMON=\(-ffp-contract=off -Wall -Wextra -Werror$/,
    /^SIM_TUS=\($/,
    /^build_foh_headless(_sub)?\(\) \{/,
    // The ONLY two executable forms, matched WHOLE (review-r3-r7 High: a
    // prefix anchor would accept `bounded_step ... ; anything-else`). These
    // are exactly the lines the wrap above produces.
    /^bounded_step 300 "the recipe pre-clean" "\$BUILD\/recipe-clean\.log" rm -f "\$BUILD\/raster\.o" "\$BUILD\/foh_dev_headless"$/,
    /^bounded_step 1200 "the raster\.o compile" "\$BUILD\/raster-build\.log" cc -O3 "\$\{CFLAGS_COMMON\[@\]\}" -c "\$GFX\/raster\.c" -o "\$BUILD\/raster\.o"$/,
    /^\)$/, /^\}$/,                                       // closers
  ];
  // A STACK, not a counter: an array literal closes on a line that merely ENDS
  // with ")" (CFLAGS_COMMON continues onto an indented line that does), while a
  // function body closes only on a bare "}". A shared counter conflates them.
  const stack = [];
  const bad = [];
  // COMMAND AND PROCESS SUBSTITUTION ANYWHERE, including inside the array
  // literals (review-r3-r7 High): bash EVALUATES `$( )`, backticks and `<( )`
  // while building an array at source time, so "array bodies are inert" is
  // false. The region legitimately contains only parameter expansions
  // (`"$TABLES"`, `"${CFLAGS_COMMON[@]}"`, `"$@"`), never these.
  lines.forEach((l, i) => {
    if (/\$\(|`|<\(|>\(/.test(l)) {
      bad.push((i + 1) + ": [substitution] " + l);
    }
  });
  lines.forEach((l, i) => {
    const t = l.trim();
    // Depth 0 is what `source` executes; anything inside the array literals or
    // the function body is data or a deferred body.
    if (stack.length === 0 && !ALLOW.some((re) => re.test(l))) bad.push((i + 1) + ": " + l);
    if (stack.length > 0) {
      const want = stack[stack.length - 1];
      if (want === ")" && /\)$/.test(t)) { stack.pop(); return; }
      if (want === "}" && t === "}") { stack.pop(); return; }
      return; // inside a literal or a body: not executed at source time
    }
    if (/^(CFLAGS_COMMON|SIM_TUS)=\(/.test(l) && !/\)$/.test(t)) stack.push(")");
    else if (/^build_foh_headless(_sub)?\(\) \{/.test(l)) stack.push("}");
  });
  if (stack.length !== 0) { console.error("recipe grammar: unbalanced nesting (ended inside " + stack.length + " construct(s))"); process.exit(1); }
  if (bad.length) {
    console.error("recipe grammar: UNBOUNDED or unrecognised depth-0 line(s):\n" + bad.join("\n"));
    process.exit(1);
  }
' "$RECIPE" || grammar_die "the extracted recipe carries a top-level construct
  this rig does not know how to bound. Sourcing it would run that construct
  outside every deadline set here — wrap it in bounded_step in the derivation
  above, and extend the allow-list deliberately."
grep -qF 'bounded_step 1200 "the raster.o compile"' "$RECIPE" \
  || grammar_die "the raster.o compile was not wrapped in the derived recipe"
grep -qxF 'build_foh_headless() { # <foh_dev_src> <out> [extra cc args...]' \
  "$RECIPE" || grammar_die "extracted recipe does not carry build_foh_headless"
grep -qxF 'SIM_TUS=(' "$RECIPE" \
  || grammar_die "extracted recipe does not carry the SIM_TUS list"
grep -qxF '}' "$RECIPE" || grammar_die "extracted recipe has no closing brace"
# A SECOND function derived from the SAME extracted text, differing only in
# WHICH copy of two TUs it compiles. Two teeth below perturb a TU that is not
# foh_dev.c — D2 lives in foh_pause.c and T6 in gfx_overlay.c — and
# build_foh_headless only parameterises foh_dev.c. Derived by substitution
# (never written out by hand) so a tooth build and the production build cannot
# drift apart, and every substitution is single-occurrence-asserted.
RECIPE_P=$BUILD/recipe-sub.sh
bcap 120 "the TU-parameterised recipe derivation" recipe-sub \
node -e '
  const fs = require("fs");
  const [src, dst] = process.argv.slice(1);
  let raw = fs.readFileSync(src, "utf8");
  for (const [needle, repl] of [
    ["build_foh_headless() {", "build_foh_headless_sub() {"],
    ["\"$FOH/foh_pause.c\"", "\"$PAUSE_SRC\""],
    ["\"$GFX/gfx_overlay.c\"", "\"$OVERLAY_SRC\""],
  ]) {
    const n = raw.split(needle).length - 1;
    if (n !== 1) { console.error("recipe-sub: " + n + " occurrences of " + JSON.stringify(needle) + " (want 1)"); process.exit(1); }
    raw = raw.replace(needle, () => repl);
  }
  fs.writeFileSync(dst, raw);
' "$RECIPE" "$RECIPE_P" || fail "could not derive the TU-parameterised recipe"
made "$RECIPE_P"
# The extracted region references exactly $FOH, $GFX, $TABLES and $BUILD
# (measured), all defined above; any name it grows dies here under `set -u`
# instead of expanding to nothing.
# shellcheck disable=SC1090 — both files are derived above, not tracked
. "$RECIPE"
PAUSE_SRC=$FOH/foh_pause.c
OVERLAY_SRC=$GFX/gfx_overlay.c
# shellcheck disable=SC1090
. "$RECIPE_P"
src_pin_verify_all
bounded_step 1200 "the host twin build" "$BUILD/build-host.log" \
  build_foh_headless "$FOH/foh_dev.c" "$BUILD/foh_dev_headless"
made "$BUILD/foh_dev_headless"
echo "   host twin built via the rig's own build_foh_headless()"

# The rig-local OS tools the system menu shells out to (review-r3-r1 High).
# WHY: `foh_sysmenu_open`'s navigation handlers are only OBSERVABLE through
# `volume`/`brightness`, because the overlay photographs its FIRST frame and
# every later keypress changes state the shot cannot see. Deleting RIGHT, DOWN
# and LEFT — and every set_level call — left the earlier version of A1 fully
# green, which is exactly the false green PROCESS §3 is about.
#
# So the leg runs with these on the FRONT of PATH: real, deterministic
# implementations that LOG their argv and answer `get` with a fixed level. The
# resulting transcript is a per-keypress record of the navigation, and A1
# asserts it exactly. They are rig-local and never installed anywhere else.
mkdir -p "$BUILD/bin"
for tool in volume brightness; do
  cat > "$BUILD/bin/$tool" <<EOF
#!/bin/sh
# rig-local stand-in for the FunKey OS tool (port/foh/check-live-arms.sh).
echo "$tool \$*" >> "\$MLFK_OSTOOL_LOG"
if [ "\$1" = get ]; then echo 50; fi
exit 0
EOF
  chmod +x "$BUILD/bin/$tool"
done
made "$BUILD/bin/volume" "$BUILD/bin/brightness"

# --- [3] key scripts: the COMMITTED f01 navigation, markers removed ----------
# The navigation under test is the committed VS flow, converted by the frozen
# generator the device rigs use — never hand-authored here.
#
# ONE mechanical derivation, and it is load-bearing: f01 carries SHOT rows, and
# flow-to-fkscript.js emits a MENU ('q') press for each of them as the rig's
# screenshot marker. Under `--system-menu` (and with no --shots-dir, which is
# exactly the configuration these arms need) every one of those marker presses
# would OPEN the system menu mid-navigation and be swallowed by it. So the
# SHOT rows are stripped from a DERIVED copy of the flow before conversion,
# which is asserted below to differ from the committed flow in nothing else.
# `--flow` still receives the COMMITTED file: shot rows are inert without
# --shots-dir, so the run under test is the committed flow's.
echo "=== [3] key scripts (committed flows, SHOT markers stripped, byte-stable x2)"
# One derivation, run for BOTH committed flows this rig drives: f01 (the VS
# launch, for the FOH and VS dispatch sites and the whole finish arm) and f06
# (the TARGET launch, for the third system-menu dispatch site).
mk_nav() { # <committed flow> <basename>
  local flow="$1" base="$2" v nshot nq
  grep -v '^SHOT ' "$flow" > "$BUILD/$base-noshot.flow"
  made "$BUILD/$base-noshot.flow"
  # provenance: the derived flow is the committed one MINUS shot rows, nothing
  # else
  diff <(grep -v '^SHOT ' "$flow") "$BUILD/$base-noshot.flow" >/dev/null \
    || fail "the derived $base flow is not exactly $flow minus its SHOT rows"
  nshot="$(grep -c '^SHOT ' "$flow")" || true
  [ "$nshot" -ge 1 ] \
    || fail "$flow carries $nshot SHOT rows — the marker-strip derivation is a
  no-op, so this rig is no longer testing what its header says it is"
  for v in "" ".b"; do
    rm -f "$BUILD/$base$v.fks"
    bcap 120 "the frozen flow->fkscript generator ($base side '$v')" \
      "fks-$base$v" \
      node "$FOH/flow-to-fkscript.js" "$BUILD/$base-noshot.flow" \
        "$BUILD/$base$v.fks" \
      || fail "flow-to-fkscript.js failed for $base"
  done
  cmp "$BUILD/$base.fks" "$BUILD/$base.b.fks" \
    || fail "flow-to-fkscript.js output is not byte-stable across two runs ($base)"
  made "$BUILD/$base.fks"
  nq="$(grep -c ' q$' "$BUILD/$base.fks")" || true
  [ "$nq" = 0 ] \
    || fail "the derived $base navigation still carries $nq MENU ('q') press
  lines — under --system-menu each one opens the modal and eats the press"
}
mk_nav "$FLOW" nav
mk_nav "$TFLOW" tnav

# Every leg below is the SAME navigation plus a suffix, so a difference in
# behaviour isolates the suffix and nothing else.
mk_leg() { # <name> <suffix-printf-string> [nav basename, default `nav`]
  local base="${3:-nav}"
  { cat "$BUILD/$base.fks"; printf '%b' "$2"; } > "$BUILD/$1.fks"
  made "$BUILD/$1.fks"
  cmp -s <(head -n "$(wc -l < "$BUILD/$base.fks")" "$BUILD/$1.fks") \
         "$BUILD/$base.fks" \
    || fail "$1.fks is not the committed $base navigation plus a suffix"
}
# A1 — open mid-match, drive VOLUME right, move to BRIGHTNESS, drive it left,
#      dismiss with B. The `s 1100` lands the MENU press ~66 frames into the
#      match (the nav ends at the launch), well past the sim's `starting`
#      window and far from the frame bound.
mk_leg sysm 's 1100\nd q\ns 100\nu q\ns 200\nd r\ns 100\nu r\ns 100\nd d\ns 100\nu d\ns 100\nd l\ns 100\nu l\ns 100\nd b\ns 100\nu b\ns 2000\n'
# A2 — open mid-match, DOWN x2 to QUIT, A. Ends the match early.
mk_leg sysq 's 1100\nd q\ns 100\nu q\ns 200\nd d\ns 100\nu d\ns 100\nd d\ns 100\nu d\ns 100\nd a\ns 100\nu a\ns 2000\n'
# B + A3 — no input at all after the launch: the match runs to its clock
#      expiry, and the MENU press at the tail lands in the SECOND FOH phase
#      the finish creates, exercising the third dispatch site.
mk_leg fin 's 6000\nd q\ns 100\nu q\ns 300\nd b\ns 100\nu b\ns 6000\n'
# D1 — open mid-match and dismiss with B, then NEVER release it. The injector
#      holds B down forever at EOF, so the sysmenu release drain must time out
#      LOUDLY instead of spinning for the rest of the session.
mk_leg stuck 's 1100\nd q\ns 100\nu q\ns 200\nd b\ns 12000\n'
# D3 — no overlay at all; hold A from mid-match onwards so it is STILL DOWN
#      when the clock expiry ends the match. That is the THIRD drain site,
#      foh_dev.c's post-match `match-exit-release`, which the overlay legs
#      never reach, and it is also where the dead-display latch lives.
mk_leg dead 's 2000\nd a\ns 40000\n'
# D4 — the PAUSE overlay (START, not MENU) opened mid-match and dismissed with
#      B that is never released. That is the remaining changed drain site,
#      `pause-release`, which D1 and D3 cannot reach because they never open
#      the game's own pause menu (review-r3-r5 Medium: it was compile-only).
mk_leg pausehold 's 1100\nd s\ns 100\nu s\ns 300\nd b\ns 14000\n'
# A4 — the THIRD system-menu dispatch site: foh_dev.c's TARGET match loop. It
#      is a different arm from the VS one (it additionally requires !pin.start,
#      so a simultaneous MENU+START cannot shadow upstream's own endGame quit),
#      and no other leg here reaches it because every other leg launches VS.
mk_leg tsys 's 1500\nd q\ns 100\nu q\ns 300\nd b\ns 100\nu b\ns 2000\n' tnav

# --- [4] the legs ------------------------------------------------------------
FOH_MAX=1100  # > the committed f01 navigation's launch tick (903, measured
              # below and asserted) with margin; ALSO the SECOND phase's bound,
              # so a leg that re-enters still terminates
FRAMES=400    # match bound; the --match-timer expiry lands at ~210
SEED=1337
LEG_BUDGET_S=180 # generous: the longest leg is ~45 s of paced wall clock

RUN_LEG_ENV=()  # per-leg extra environment (PRESENT_FAIL etc.), reset by caller
RUN_LEG_FLOW="" # override the committed flow a leg drives (the target leg)
run_leg() { # <binary> <legId> <keyscript> [extra args...]
  local bin="$1" id="$2" keys="$3" flow="${RUN_LEG_FLOW:-$FLOW}"
  shift 3
  brm "$BUILD/$id"
  mkdir -p "$BUILD/$id" "$BUILD/$id-persist" "$BUILD/$id/shots"
  # MLFK_MENU_SHOT is the overlays' and the finish arm's OWN self-shot hook
  # (foh_pause.c / foh_dev.c). It is unset in every other check and in the
  # product launcher precisely so that it is inert everywhere except a
  # deliberate demonstration run — this rig IS that run, and those frames are
  # the only way to photograph a modal that freezes the driver's tick loop.
  # env(1) rather than a `VAR=x func` prefix: assignment prefixes on a SHELL
  # FUNCTION leak into the caller's environment in POSIX mode, so the next
  # leg would inherit the previous one's persist dir. exec'd env cannot.
  # PATH is prefixed with the rig-local OS tools so the system menu's popen /
  # system calls reach implementations that log what they were asked to do.
  run_bounded "$LEG_BUDGET_S" "leg $id to finish" \
    "$BUILD/$id/out.txt" "$BUILD/$id/err.txt" \
    env "MLFK_PERSIST_DIR=$PWD/$BUILD/$id-persist" \
        "MLFK_MENU_SHOT=$PWD/$BUILD/$id/shots" \
        "MLFK_HEADLESS_KEYS=$PWD/$keys" \
        "MLFK_OSTOOL_LOG=$PWD/$BUILD/$id/ostools.txt" \
        "PATH=$PWD/$BUILD/bin:$PATH" \
        ${RUN_LEG_ENV[@]+"${RUN_LEG_ENV[@]}"} \
    "$bin" --flow "$flow" --input poll --flow-out "$BUILD/$id/trace.txt" \
      --foh-max "$FOH_MAX" --pace 1 --budget-ns 16666667 \
      --bridge live --simdata "$BUILD/simdata.txt" --seed "$SEED" \
      --bstate-out "$BUILD/$id/bstate.txt" \
      --gfxdata "$GFXDATA" --vfxdata "$VFXDATA" --glyphs "$GLYPHS" \
      --anim-dir "$TABLES" --legible \
      --sndpack "$BUILD/sndpack.bin" --music-manifest "$BUILD/music.txt" \
      --frames "$FRAMES" --record-trace "$BUILD/$id/rec.json" \
      --record-keys "$BUILD/$id/keys.txt" "$@"
  echo "$RUN_RC" > "$BUILD/$id/rc.txt"
  echo "$RUN_TIMEDOUT" > "$BUILD/$id/timedout.txt"
}

expect_ok() { # <legId>
  local id="$1"
  [ "$(cat "$BUILD/$id/timedout.txt")" = 0 ] \
    || fail "leg $id was KILLED at the ${LEG_BUDGET_S}s bound — it hung. That
  is the failure mode the bounded release drain exists to prevent; read
  $BUILD/$id/err.txt before touching anything else."
  [ "$(cat "$BUILD/$id/rc.txt")" = 0 ] \
    || { sed 's/^/  | /' "$BUILD/$id/err.txt" >&2
         fail "leg $id exited rc $(cat "$BUILD/$id/rc.txt") (want 0)"; }
  # No drain may have said ANYTHING on a leg whose script releases every key.
  # The test is on the RESEMBLANCE `foh_drain:`, not on the full timeout
  # grammar (review-r3-r2 Medium): a prefix grep for the exact phrase would
  # have silently ignored a MALFORMED diagnostic, which is the permissive-parse
  # hole one_canonical exists to close. A clean leg produces zero lines from
  # that producer, so zero is the assertion.
  local ndrain
  ndrain="$(grep -cF 'foh_drain:' "$BUILD/$id/err.txt")" || true
  [ "$ndrain" = 0 ] \
    || { grep -F 'foh_drain:' "$BUILD/$id/err.txt" | relay_lines >&2
         fail "leg $id produced $ndrain foh_drain diagnostic line(s), but its
  key script releases every button — either the script, the drain or the
  diagnostic's own grammar is wrong"; }
  # ZERO FAILED PRESENTS on a clean leg (review-r3-r8 High). Both visual
  # witnesses are written from the framebuffer just BEFORE it is presented, so
  # a present that started failing would leave every screenshot, mask and
  # transcript intact while nothing reached a screen. This does not prove the
  # pixels were displayed — see the stated limit below — but it does hold the
  # app's own present accounting to zero, which is the part a host can judge,
  # and D3 proves that counter is live by making every present fail.
  #
  # STATED LIMIT, same class as the music mute: "the framebuffer reached a
  # DISPLAY" is not observable on a host at all, because the headless backend's
  # present is a no-op that reports success by design (platform_headless.c). A
  # tooth replacing a present call with a no-op would therefore be green for a
  # reason unrelated to the perturbation. Closing it needs the DEVICE leg
  # (SDL_Flip's real return plus an in-app screenshot of the presented page);
  # registered as a deferral in the writer's report.
  local fpf mpf
  fpf="$(sed -nE "s/^foh_dev foh: .*, ($NUM) failed presents,.*/\1/p" \
    "$BUILD/$id/err.txt" | head -1)"
  [ "${fpf:-x}" = 0 ] \
    || fail "leg $id reports ${fpf:-<unparsable>} failed presents in its FOH
  summary (want 0). Every screenshot this rig reads is taken one statement
  before a present, so a failing present leaves the evidence intact and the
  screen blank."
  if grep -qE "$MATCH_RE" "$BUILD/$id/err.txt"; then
    mpf="$(sed -nE "s/^foh_dev match: .*, ($NUM) failed presents,.*/\1/p" \
      "$BUILD/$id/err.txt" | head -1)"
    [ "${mpf:-x}" = 0 ] \
      || fail "leg $id reports ${mpf:-<unparsable>} failed presents in its
  MATCH summary (want 0)"
  fi
  made "$BUILD/$id/trace.txt"
  return 0
}

# The single fact every leg's navigation must have achieved before any arm
# assertion means anything: the COMMITTED f01 flow launched a VS match through
# the polled input path. Fully anchored — the producer's grammar is exact.
LAUNCH_RE='^LAUNCH 903 p1=2 p2=0 p2type=0 difficulty=3 stage=0 turbo=0 lcancel=0 flashlcancel=0 walljump=0 tapjump=0,0,0,0 versus=0 p3=0 p4=0 p3type=-1 p4type=-1 p3difficulty=3 p4difficulty=3$'
assert_launched() { # <legId>
  one_canonical "$BUILD/$1/trace.txt" 'LAUNCH ' "$LAUNCH_RE" \
    "$1 f01 VS launch"
  head -1 "$BUILD/$1/trace.txt" | grep -qxF 'FOHTRACE1 flow=f01-vs-g01' \
    || grammar_die "$1 trace line 1 is not the committed f01 header (got
  '$(head -1 "$BUILD/$1/trace.txt")')"
}

# CANONICAL DECIMALS, not `[0-9]+` (review-r3-r2 Medium): the loose form
# accepts a leading zero and an unbounded width, so `007` or a 40-digit run of
# digits would have satisfied a grammar this script then parses a decision out
# of.
#
# TWELVE digits, not twenty (review-r3-r5 Medium): these values go on to be
# SUBTRACTED and COMPARED in bash arithmetic, which is signed 64-bit, so a
# 20-digit token — legal for a UINT64 producer — could wrap a difference and
# turn corruption into a plausible number. 10^12 is far above every value any
# of these fields can physically hold (ticks and frames are thousands, wall
# clock is seconds in ms, audio counters are bounded by run length) and far
# below the wrap point, so anything wider is corruption and is refused by the
# grammar before any arithmetic sees it.
NUM='(0|[1-9][0-9]{0,11})'
MATCH_RE="^foh_dev match: $NUM frames, $NUM render skips, $NUM failed presents, wall $NUM ms, pace=1 budget=16666667 ns\$"
FOH_RE="^foh_dev foh: $NUM ticks, $NUM transitions, $NUM shots, $NUM render skips, $NUM failed presents, launched=[01]\$"
FOH_LAUNCHED_RE="^foh_dev foh: 903 ticks, 5 transitions, $NUM shots, $NUM render skips, $NUM failed presents, launched=1\$"
AUDIO_RE="^foh_dev audio: $NUM callbacks, $NUM underruns, $NUM badlen, $NUM voice starts, $NUM voice stops, $NUM steals, rate=$NUM samples=$NUM channels=$NUM\$"
MUSIC_RE="^foh_dev music: $NUM out frames, $NUM starves, $NUM refills, ring=$NUM chunk=$NUM\$"

# EVERY number this rig decides on comes out of a line that has already been
# proved canonical (review-r3-r1 Medium: the earlier readers accepted arbitrary
# suffixes and any number of resembling lines, then parsed whichever came
# first). `field` asserts the exact anchored grammar with one_canonical and
# only then extracts, so a malformed or duplicated summary is corruption here
# rather than a silently-parsed prefix.
field() { # <legId> <needle> <anchored-regex> <sed-extract> <ctx> -> stdout
  local id="$1" needle="$2" re="$3" ex="$4" ctx="$5" v
  one_canonical "$BUILD/$id/err.txt" "$needle" "$re" "$id $ctx"
  v="$(grep -E "$re" "$BUILD/$id/err.txt" | sed -nE "$ex")"
  [ -n "$v" ] || grammar_die "$id: could not extract $ctx from its canonical line"
  printf '%s' "$v"
}
match_frames() { # <legId> -> stdout
  field "$1" 'foh_dev match:' "$MATCH_RE" \
    's/^foh_dev match: ([0-9]+) frames,.*/\1/p' 'match frame count'
}
match_wall_ms() { # <legId> -> stdout
  field "$1" 'foh_dev match:' "$MATCH_RE" \
    's/^foh_dev match: .*, wall ([0-9]+) ms,.*/\1/p' 'match wall clock'
}
foh_present_fails() { # <legId> -> stdout
  field "$1" 'foh_dev foh:' "$FOH_RE" \
    's/^foh_dev foh: .*, ([0-9]+) failed presents,.*/\1/p' 'FOH failed presents'
}
voice_starts() { # <legId> -> stdout
  field "$1" 'foh_dev audio:' "$AUDIO_RE" \
    's/^foh_dev audio: .*, ([0-9]+) voice starts,.*/\1/p' 'voice starts'
}

# ---------------------------------------------------------------------------
# ARM A — the SYSTEM MENU
# ---------------------------------------------------------------------------
sysmenu=0

echo "=== [4a] ARM A / A1: the system menu opened and navigated MID-MATCH"
run_leg "$BUILD/foh_dev_headless" sysm "$BUILD/sysm.fks" --system-menu
expect_ok sysm
assert_launched sysm
one_canonical "$BUILD/sysm/err.txt" 'foh_dev foh:' "$FOH_LAUNCHED_RE" \
  "A1 single FOH phase, launched"
one_canonical "$BUILD/sysm/err.txt" 'foh_dev match:' "$MATCH_RE" "A1 match summary"
[ "$(match_frames sysm)" = "$FRAMES" ] \
  || fail "A1's match ran $(match_frames sysm) frames, not the full $FRAMES
  bound — something ENDED it, so this leg is not the open-and-dismiss case"
made "$BUILD/sysm/shots/sysmenu-1.ppm"
[ "$(ls "$BUILD/sysm/shots" | wc -l | tr -d ' ')" = 1 ] \
  || fail "A1 produced $(ls "$BUILD/sysm/shots" | wc -l) self-shots (want exactly
  1 — a second one means the modal opened more times than the script asks)"
# THE PIXEL BINDING, positionally. The overlay draws ONE option at a time,
# centred, and the frame it photographs is its FIRST, i.e. the initial VOLUME
# selection. text2_center places a 6-glyph string at
# x = (240 - (6*7-1)*2)/2 = 79 and the caller's
# labelY = RAST_H/2 - labelH/2 - PAD_Y = 120 - 9 - 18 = 93; nine glyph rows at
# scale 2 occupy 93..110, which sits above the bar (128..148) and below the up
# arrow, so the whole band belongs to the label. The colour is the no-artwork
# fallback {236,236,236} — no platform backend outside SDL1 decodes
# zone_bg.png, so a host run always takes that arm — which round-trips through
# 565 to exactly (232,236,232).
mask_match "$BUILD/sysm/shots/sysmenu-1.ppm" "$FOH/foh_font.c" \
  2 VOLUME 2 79 93 232 236 232 93 111 \
  || fail "A1: the system-menu shot does not carry the committed font's
  \"VOLUME\" at the overlay's own centred origin (see the mask_match message
  above for the first pixel that disagreed)"
# THE NAVIGATION, which no shot can show (review-r3-r1 High): the overlay
# photographs its first frame, so RIGHT / DOWN / LEFT all arrive afterwards and
# every pixel assertion above is blind to them. The rig-local OS tools record
# what the overlay actually asked the system to do, in order, and that
# transcript IS the navigation:
#   volume get / brightness get  — read_cmd_int on open, both options;
#   volume set 60                — RIGHT while VOLUME is selected (50 -> 60);
#   brightness set 40            — LEFT after DOWN moved to BRIGHTNESS.
# The last line is the one that cannot be faked by a stuck cursor: it proves
# BOTH that DOWN moved the selection and that LEFT reached the brightness arm.
made "$BUILD/sysm/ostools.txt"
cat > "$BUILD/sysm/ostools.want" <<'EOF'
volume get
brightness get
volume set 60
brightness set 40
EOF
cmp "$BUILD/sysm/ostools.txt" "$BUILD/sysm/ostools.want" \
  || { echo "--- got ---" >&2; relay_lines < "$BUILD/sysm/ostools.txt" >&2
       echo "--- want ---" >&2; relay_lines < "$BUILD/sysm/ostools.want" >&2
       fail "A1: the system menu's OS-tool transcript is not the one its
  navigation must produce. Either read_cmd_int/set_level did not run, or
  RIGHT/DOWN/LEFT did not reach the handlers this leg presses them for."; }
SYS_LABEL_PX=$MASK_PX
echo "   A1: VOLUME mask matched ($SYS_LABEL_PX px, positional) + the exact 4-line OS-tool transcript"
sysmenu=$((sysmenu + 1))

echo "=== [4b] ARM A / A2: the system menu's QUIT ended the match early"
run_leg "$BUILD/foh_dev_headless" sysq "$BUILD/sysq.fks" --system-menu
expect_ok sysq
assert_launched sysq
one_canonical "$BUILD/sysq/err.txt" 'foh_dev foh:' "$FOH_LAUNCHED_RE" \
  "A2 single FOH phase, launched"
one_canonical "$BUILD/sysq/err.txt" 'foh_dev match:' "$MATCH_RE" "A2 match summary"
made "$BUILD/sysq/shots/sysmenu-1.ppm"
A2F="$(match_frames sysq)"
# THE DISCRIMINATOR. In this leg the VS match loop has exactly three ways to
# leave before its bound: the system menu's QUIT, the pause overlay's QUIT
# (START — this script never presses it), and the finish arm (no --match-timer,
# so upstream's 480 s clock is 28,000 frames away). An early end is therefore
# the system menu's QUIT and nothing else.
[ "$A2F" -lt "$FRAMES" ] \
  || fail "A2's match ran $A2F frames (the full $FRAMES bound) — the system
  menu's QUIT did not end it, so nothing here exercised that arm"
[ "$A2F" -gt 1 ] || fail "A2's match ran only $A2F frames"
# ...and it ended on the frame the SCRIPT puts the press on, not merely
# somewhere before the bound (review-r3-r5 Medium). The injected input is a
# pure function of the poll index, so this frame is a fixed property of the
# committed navigation plus this leg's suffix — a drift in either moves it and
# has to be looked at rather than absorbed.
A2F_WANT=67
[ "$A2F" = "$A2F_WANT" ] \
  || fail "A2's match ended on frame $A2F, not the pinned $A2F_WANT. The
  scripted press lands at a fixed poll index, so this moved because the
  navigation, the suffix or the overlay's poll accounting changed. Do not
  re-pin without understanding which."
echo "   A2: match ended on frame $A2F of $FRAMES via the overlay's QUIT"
sysmenu=$((sysmenu + 1))

# ---------------------------------------------------------------------------
# ARM B — the VS-FINISH block (and A3, the FOH-phase dispatch it enables)
# ---------------------------------------------------------------------------
echo "=== [5] ARM B: the VS match reached its clock expiry (--match-timer)"
run_leg "$BUILD/foh_dev_headless" fin "$BUILD/fin.fks" --system-menu --match-timer 2
expect_ok fin
assert_launched fin
one_canonical "$BUILD/fin/err.txt" 'foh_dev match:' "$MATCH_RE" "B match summary"
FINF="$(match_frames fin)"
FINW="$(match_wall_ms fin)"
# [B1] the match ended at the EXPIRY, not at the bound. sim_tick.c decrements
# matchTimer by 0.016667 per frame once `starting` clears (~90 frames at
# startTimer 1.5), so a 2 s clock expires around frame 210. The window below is
# deliberately wide enough to survive a startTimer change and narrow enough to
# exclude both the bound and an early crash.
[ "$FINF" -lt "$FRAMES" ] \
  || fail "B: the match ran $FINF frames (the full $FRAMES bound) — the clock
  never expired, so the finish arm never ran"
[ "$FINF" -gt 120 ] && [ "$FINF" -lt 320 ] \
  || fail "B: the match ended on frame $FINF, outside the 120..320 window a
  2 s clock plus the `starting` window can produce — it ended for some other
  reason and this leg is not evidence about the finish arm"
# [B2] the banner. Its self-shot is written from INSIDE the `if (g_vsFinish)`
# block and nowhere else in the program, so the file existing is already a
# statement about that block; the MASK pins WHICH banner, and where.
# tdev_vs_banner_text draws at scale 4, white, centred:
# x = 120 - foh_text_width("TIME!",4)/2 = 120 - 58 = 62, y = 120 - (7*4)/2 =
# 106, seven glyph rows -> band 106..133. RastCol {255,255,255} packs to 0xFFFF
# and expands back to exactly (248,252,248). The mask compare rejects "some
# other 5-letter word", a shifted origin and a partial render, none of which an
# ink COUNT can see. (It cannot see a second draw of the same string at the
# same origin — opaque overdraw leaves the framebuffer identical — and neither
# can any other pixel test.)
made "$BUILD/fin/shots/finish-banner.ppm"
mask_match "$BUILD/fin/shots/finish-banner.ppm" "$FOH/foh_font.c" \
  1 'TIME!' 4 62 106 248 252 248 106 134 \
  || fail "B: the finish banner is not the committed font's \"TIME!\" at the
  centred origin tdev_vs_banner_text draws it at. A clock expiry must paint
  TIME! (see the mask_match message above for the first pixel that disagreed)."
BAN_PX=$MASK_PX
# [B3] the 2500 ms hold really ran, and ran IN FULL.
#
# THE ARITHMETIC, because the slack is the whole point (review-r3-r2 Medium —
# an earlier 16 ms/frame under-estimate left ~140 ms of unearned room, so a
# 2.35 s hold passed). `wall` covers tStart..tEnd, and the loop pace-sleeps
# after every frame EXCEPT the finish frame, on which it breaks out — so the
# paced portion is exactly (FINF-1) budget periods of 16666667 ns, computed
# here in ns and divided down rather than rounded per frame. Pacing can only
# make a frame LATE, never early, so that term is a true floor. Everything
# above it is the hold.
HOLD_PACED_MS=$(( (FINF - 1) * 16666667 / 1000000 ))
HOLD_FLOOR=$(( HOLD_PACED_MS + 2450 ))
# TWO-SIDED, and TIGHT (review-r3-r4/r5 Medium: `FINF*18 + 4000` admitted a
# 4.3 s hold, and even paced+3700 admitted 3.5 s). The window is now
# [paced+2450, paced+2700], i.e. the hold is pinned to 2500 ms within about
# +/-200 ms. That is sized off MEASUREMENT, not taste: across five runs on this
# host the hold measured 2504-2510 ms (wall 5987-5993 against a 3483 ms paced
# floor), a 6 ms spread, and the hold is a monotonic-deadline loop so it can
# only ever OVERSHOOT — which is why the headroom sits on the ceiling side.
# T7 (2300 ms) proves the floor bites and T11 (2900 ms) proves the ceiling
# does, both within 200 ms of the boundary; T9 (4500 ms) is the gross case.
HOLD_CEIL=$(( HOLD_PACED_MS + 2700 ))
[ "$FINW" -ge "$HOLD_FLOOR" ] && [ "$FINW" -le "$HOLD_CEIL" ] \
  || fail "B: the match reports wall ${FINW} ms for $FINF frames, outside the
  [$HOLD_FLOOR, $HOLD_CEIL] ms window that $FINF paced frames plus the 2500 ms
  finish hold produce — the hold did not run, or did not run in full"
# [B4] the block's destination. `g_mexit = MEX_CSS` re-enters the FOH
# in-process, so the teardown FOH summary is the SECOND phase's: a run that
# reached the tick bound WITHOUT launching, after a match was played. A
# single-phase run cannot produce that shape (its phase ended BY launching).
one_canonical "$BUILD/fin/err.txt" 'foh_dev foh:' \
  "^foh_dev foh: $FOH_MAX ticks, $NUM transitions, $NUM shots, $NUM render skips, $NUM failed presents, launched=0\$" \
  "B second FOH phase (the MEX_CSS re-entry)"
# [B5] the block's AUDIO lines ran at all. `foh_snd()` returns immediately when
# no sndpack was loaded and the music mute sits under `if (g_have_music)`, so
# both are silent no-ops in a run without an audio plane — which is how they
# stayed unexecuted for as long as they did. These two summary lines are
# emitted ONLY under those same two guards, so their presence is the proof that
# the guards were true and the bodies were reachable; T8 then proves the sfx
# call itself fired by removing it and watching the voice count drop.
one_canonical "$BUILD/fin/err.txt" 'foh_dev audio:' "$AUDIO_RE" \
  "B audio summary (proves the sndpack loaded, so foh_snd is not a no-op)"
one_canonical "$BUILD/fin/err.txt" 'foh_dev music:' "$MUSIC_RE" \
  "B music summary (proves g_have_music, so the finish block's mute body ran)"
# STATED LIMIT, not a silent gap (review-r3-r2 Medium, dispositioned): the mute
# LINE is proven REACHED — it sits under `if (g_have_music)` and that summary is
# emitted under the same guard — but its EFFECT is not observable on a host.
# The headless audio backend is accept-and-idle BY DESIGN (platform_headless.c:
# no callback thread ever runs, so a headless run can never masquerade as a
# device audio run), which means nothing ever consumes the music ring and
# `g_mix.music.on` has no reachable consequence to measure. A tooth deleting
# that assignment would therefore be green here for a reason that has nothing
# to do with the assignment. Closing it needs the DEVICE leg (a real callback,
# `underruns == 0` and audible-output accounting across the hold), and the
# device is offline for this increment — registered as a deferral in the
# writer's report rather than papered over with a witness that proves nothing.
FIN_VOICES="$(voice_starts fin)"
[ "$FIN_VOICES" -gt 0 ] \
  || fail "B: the run started $FIN_VOICES voices — the SFX plane is loaded but
  nothing ever played, so the finish sfx cannot have played either"
echo "   B: expiry on frame $FINF, TIME! mask matched ($BAN_PX px), wall ${FINW} ms in [$HOLD_FLOOR,$HOLD_CEIL], re-entered the FOH, $FIN_VOICES voices"
vsfinish=1

# Any tooth that judges the HOLD by comparing wall clock has to be running the
# SAME match first (review-r3-r2 Low): a copy that terminated early for an
# unrelated reason also reports a short wall, and would be credited as a
# hold-duration rejection. So a hold tooth must reach the same expiry frame AND
# still have produced the finish block's own artifact.
assert_same_finish() { # <legId> <tag>
  local id="$1" tag="$2" f
  f="$(match_frames "$id")"
  [ "$f" = "$FINF" ] \
    || fail "$tag: the copy's match ended on frame $f, not $FINF — it did not
  run the same match, so a shorter wall clock says nothing about the hold"
  made "$BUILD/$id/shots/finish-banner.ppm"
}

echo "=== [5a] ARM A / A3: the system menu opened in the FOH menus (3rd site)"
# The same leg's tail presses MENU after the re-entry. The FOH phase is where
# the owner first reported the button doing nothing, and it is the third
# dispatch site — as unreached as the other two.
made "$BUILD/fin/shots/sysmenu-1.ppm"
mask_match "$BUILD/fin/shots/sysmenu-1.ppm" "$FOH/foh_font.c" \
  2 VOLUME 2 79 93 232 236 232 93 111 \
  || fail "A3: the FOH-phase system-menu shot does not carry the committed
  font's \"VOLUME\" at the overlay's centred origin"
# ...and this leg's own OS-tool transcript proves the overlay read the levels
# there too — a second, independent execution of read_cmd_int at the third
# dispatch site.
made "$BUILD/fin/ostools.txt"
cmp <(printf 'volume get\nbrightness get\n') "$BUILD/fin/ostools.txt" \
  || { relay_lines < "$BUILD/fin/ostools.txt" >&2
       fail "A3: the FOH-phase overlay's OS-tool transcript is not exactly the
  two opening reads (this leg presses no level keys inside the menu)"; }
# ...and it is a DIFFERENT frame from A1's, because the overlay dims the live
# frame behind it and the two opened over different screens. Identical bytes
# would mean one of the two shots is not what this rig says it is.
cmp -s "$BUILD/fin/shots/sysmenu-1.ppm" "$BUILD/sysm/shots/sysmenu-1.ppm" \
  && fail "A3: the FOH-phase overlay shot is byte-identical to the in-match
  one, so the two legs did not photograph different screens"
echo "   A3: FOH-phase overlay rendered VOLUME over a different backdrop than A1's"
sysmenu=$((sysmenu + 1))

echo "=== [5c] ARM A / A4: the system menu inside a TARGET match (3rd SITE)"
# review-r3-r7 Medium: A1/A2 both drive the VS loop's dispatch and A3 the FOH
# phase's, so `foh_sysmenu_open` had been ENTERED from two of its three call
# sites. The target loop's arm is a genuinely different one — it additionally
# requires `!pin.start`, so a simultaneous MENU+START cannot shadow upstream's
# own endGame quit — and nothing else here launches a target match.
RUN_LEG_FLOW=$TFLOW
run_leg "$BUILD/foh_dev_headless" tsys "$BUILD/tsys.fks" --system-menu
RUN_LEG_FLOW=
expect_ok tsys
TLAUNCH_RE="^TLAUNCH $NUM char=2 tstage=0\$"
one_canonical "$BUILD/tsys/trace.txt" 'TLAUNCH ' "$TLAUNCH_RE" \
  "A4 f06 TARGET launch"
one_canonical "$BUILD/tsys/err.txt" 'foh_dev match:' "$MATCH_RE" "A4 match summary"
made "$BUILD/tsys/shots/sysmenu-1.ppm"
mask_match "$BUILD/tsys/shots/sysmenu-1.ppm" "$FOH/foh_font.c" \
  2 VOLUME 2 79 93 232 236 232 93 111 \
  || fail "A4: the target-match system-menu shot does not carry the committed
  font's \"VOLUME\" at the overlay's centred origin"
made "$BUILD/tsys/ostools.txt"
cmp <(printf 'volume get\nbrightness get\n') "$BUILD/tsys/ostools.txt" \
  || { relay_lines < "$BUILD/tsys/ostools.txt" >&2
       fail "A4: the target-match overlay's OS-tool transcript is not exactly
  the two opening reads"; }
echo "   A4: the overlay opened inside a TARGET match and rendered VOLUME"
sysmenu=$((sysmenu + 1))

# --- [5b] determinism of the instrument --------------------------------------
# Everything above compares numbers derived from produced bytes, which is only
# meaningful if those bytes are reproducible. The injected input is a pure
# function of the poll index, so a fresh run must reproduce the finish frame,
# the banner shot and the FOH trace exactly.
echo "=== [5b] the ARM B leg is byte-reproducible"
run_leg "$BUILD/foh_dev_headless" fin2 "$BUILD/fin.fks" --system-menu --match-timer 2
expect_ok fin2
[ "$(match_frames fin2)" = "$FINF" ] \
  || fail "the rerun's match ended on frame $(match_frames fin2) but the first
  run ended on $FINF — the scripted input is NOT poll-indexed, so every
  comparison in this script is timing-dependent"
cmp "$BUILD/fin/shots/finish-banner.ppm" "$BUILD/fin2/shots/finish-banner.ppm" \
  || fail "two fresh finish legs produced different banner shots"
cmp "$BUILD/fin/trace.txt" "$BUILD/fin2/trace.txt" \
  || fail "two fresh finish legs produced different FOH traces"
echo "   rerun: same expiry frame ($FINF), byte-identical banner + trace"

# ---------------------------------------------------------------------------
# THE DRAIN: bounded, loud, and proven to have been unbounded before
# ---------------------------------------------------------------------------
echo "=== [6] D1: a stuck button times out LOUDLY instead of hanging"
run_leg "$BUILD/foh_dev_headless" stuck "$BUILD/stuck.fks" --system-menu
[ "$(cat "$BUILD/stuck/timedout.txt")" = 0 ] \
  || fail "D1: the run was KILLED at the ${LEG_BUDGET_S}s bound. A script that
  holds a button at EOF must make the drain TIME OUT, not hang the process —
  that is the whole precondition this increment exists to fix."
[ "$(cat "$BUILD/stuck/rc.txt")" = 0 ] \
  || fail "D1: exited rc $(cat "$BUILD/stuck/rc.txt") (want 0 — a drain timeout
  is a loud diagnostic, not a death: the player keeps their match)"
made "$BUILD/stuck/shots/sysmenu-1.ppm"
# the diagnostic, at its exact committed grammar, naming the drain AND the key
DRAIN_RE='^foh_drain: TIMEOUT sysmenu-release after 600 polls, still held: b$'
one_canonical "$BUILD/stuck/err.txt" 'foh_drain: TIMEOUT' "$DRAIN_RE" \
  "D1 release-drain timeout diagnostic"
echo "   D1: $(grep -m1 '^foh_drain: TIMEOUT' "$BUILD/stuck/err.txt")"
drains=$((drains + 1)) # a POSITIVE witness, not a tooth (D2 is its tooth)

echo "=== [6a2] D4: the PAUSE overlay's drain (the third changed site)"
# The game's own pause menu opens on START and resumes on B. Holding B past the
# resume drives `pause-release`, the one drain D1 and D3 never touch. Its
# result switch is exercised here too: a TIMEOUT keeps RESUME, so the match
# must go on and run to its frame bound rather than ending.
run_leg "$BUILD/foh_dev_headless" pausehold "$BUILD/pausehold.fks"
[ "$(cat "$BUILD/pausehold/timedout.txt")" = 0 ] \
  || fail "D4: the run was KILLED at the bound — the pause-release drain hung"
[ "$(cat "$BUILD/pausehold/rc.txt")" = 0 ] \
  || { sed 's/^/  | /' "$BUILD/pausehold/err.txt" >&2
       fail "D4: exited rc $(cat "$BUILD/pausehold/rc.txt") (want 0)"; }
assert_launched pausehold
D4_DRAIN_RE='^foh_drain: TIMEOUT pause-release after 600 polls, still held: b$'
one_canonical "$BUILD/pausehold/err.txt" 'foh_drain: TIMEOUT' "$D4_DRAIN_RE" \
  "D4 pause-release drain timeout diagnostic"
one_canonical "$BUILD/pausehold/err.txt" 'foh_dev match:' "$MATCH_RE" \
  "D4 match summary"
[ "$(match_frames pausehold)" = "$FRAMES" ] \
  || fail "D4: the match ran $(match_frames pausehold) frames, not the full
  $FRAMES bound — a drain TIMEOUT must keep the overlay's RESUME verdict, so
  the match has to continue"
echo "   D4: $(grep -m1 '^foh_drain: TIMEOUT' "$BUILD/pausehold/err.txt")"
drains=$((drains + 1))

echo "=== [6b] D2: the UNBOUNDED drain really does hang (the tooth)"
# Without this, "the bound fixed a hang" is an assertion about a program nobody
# ran. The copy differs from foh_pause.c in exactly one thing: the loop bound.
perturb "$FOH/foh_pause.c" "$BUILD/tooth-nodrain/foh/foh_pause.c" \
  'for (long i = 0; i < FOH_DRAIN_MAX_POLLS; i++) {' \
  'for (long i = 0; i >= 0; i++) { /* D2: bound removed */' \
  'D2'
# `i >= 0` rather than `;;` deliberately: an empty condition leaves `i` set but
# never read and the production -Werror set rejects that, so the tooth would
# have failed to BUILD rather than failing to TERMINATE — which proves nothing
# about the hang. This form is the pre-fix program: unbounded for every
# reachable iteration count.
PAUSE_SRC=$BUILD/tooth-nodrain/foh/foh_pause.c
# -Iport/foh so the COPY's quoted "foh_pause.h" and its "../gfx/..." includes
# resolve against the real tree (the copy sits outside port/foh, so
# quoted-include resolution alone misses) — check-mexit-reentry.sh's T2 recipe.
src_pin_verify_all
bounded_step 1200 "the D2 build" "$BUILD/build-d2.log" \
  build_foh_headless_sub "$FOH/foh_dev.c" "$BUILD/tooth-nodrain/foh_dev_headless" \
    -Iport/foh -Iport/gfx
PAUSE_SRC=$FOH/foh_pause.c
made "$BUILD/tooth-nodrain/foh_dev_headless"
# THE SAME DEADLINE D1 GETS (review-r3-r6 Medium): a shorter one would let
# ordinary host load on a slow run be mistaken for the intended hang. D1 runs
# the identical script on a binary that differs in exactly one asserted line
# and FINISHES in ~35 s, so 180 s of not finishing is not slowness.
D2_BUDGET_S=$LEG_BUDGET_S
run_leg "$BUILD/tooth-nodrain/foh_dev_headless" legD2 "$BUILD/stuck.fks" \
  --system-menu
RUN_TIMEDOUT="$(cat "$BUILD/legD2/timedout.txt")"
RUN_RC="$(cat "$BUILD/legD2/rc.txt")"
[ "$RUN_TIMEDOUT" = 1 ] \
  || fail "D2: the unbounded copy FINISHED (rc $RUN_RC) on the same stuck-button
  script that D1 uses. Either the perturbation did not remove the bound or the
  hang this increment fixed was never real — in both cases D1 proves nothing."
# WHERE it hung, argued rather than assumed (review-r3-r3 Low). The shot only
# proves the overlay rendered, which happens before the dismissal — so on its
# own it does not exclude an unrelated hang later in the run. What excludes
# that is the pairing: D1 and D2 run the SAME key script through binaries that
# perturb() has asserted differ in exactly ONE line, the drain's loop bound,
# and D1 RUNS TO COMPLETION on it. Any hang that were not the drain would have
# to be present in both, so D1's completion is the exclusion. The shot stays as
# the coarse "it got that far" check.
made "$BUILD/legD2/shots/sysmenu-1.ppm"
# ZERO lines resembling the drain producer, not merely no exact timeout line:
# the whole point of the perturbation is that this build cannot reach the
# diagnostic at all, and a MALFORMED one would slip past a prefix probe.
nd2="$(grep -cF 'foh_drain:' "$BUILD/legD2/err.txt")" || true
[ "$nd2" = 0 ] \
  || { grep -F 'foh_drain:' "$BUILD/legD2/err.txt" | relay_lines >&2
       fail "D2: the unbounded copy produced $nd2 foh_drain line(s), so the
  bound was not actually removed"; }
echo "   D2: the unbounded copy opened the overlay and then hung (killed at ${D2_BUDGET_S}s)"
teeth=$((teeth + 1))

echo "=== [6c] D3: the THIRD drain site + its dead-display latch"
# TWO things D1/D2 cannot reach. (1) foh_dev.c's post-match
# `match-exit-release` drain is a different site from the overlays' — it is the
# one that runs before the `foh_phase:` re-entry — and no overlay leg touches
# it. (2) That site is where the DEAD-DISPLAY LATCH lives, and the latch is
# invisible while presents succeed: every headless present does, so deleting
# `*presentDead = 1` left every other leg green (review-r3-r1 Medium).
#
# So this leg holds A across the clock expiry (the drain therefore runs with an
# input still down and times out at its bound) with MLFK_HEADLESS_PRESENT_FAIL
# turned on, so every present in the run FAILS. With the latch, the drain
# counts ONE failed present and stops presenting; without it, it counts one per
# poll for all 600. The difference is measured against a no-latch COPY rather
# than predicted, so the number does not have to be modelled.
RUN_LEG_ENV=("MLFK_HEADLESS_PRESENT_FAIL=1")
run_leg "$BUILD/foh_dev_headless" dead "$BUILD/dead.fks" --match-timer 2
RUN_LEG_ENV=()
[ "$(cat "$BUILD/dead/timedout.txt")" = 0 ] \
  || fail "D3: the run was KILLED at the bound — the match-exit drain hung"
[ "$(cat "$BUILD/dead/rc.txt")" = 0 ] \
  || { sed 's/^/  | /' "$BUILD/dead/err.txt" >&2
       fail "D3: exited rc $(cat "$BUILD/dead/rc.txt") (want 0)"; }
D3_DRAIN_RE='^foh_drain: TIMEOUT match-exit-release after 600 polls, still held: a$'
one_canonical "$BUILD/dead/err.txt" 'foh_drain: TIMEOUT' "$D3_DRAIN_RE" \
  "D3 match-exit drain timeout diagnostic"
D3_FAILS="$(foh_present_fails dead)"
echo "   D3: $(grep -m1 '^foh_drain: TIMEOUT' "$BUILD/dead/err.txt")"
drains=$((drains + 1)) # positive witness; D3-tooth below is its tooth

echo "=== [6d] D3-tooth: without the latch, one dead display is counted 600 times"
perturb "$FOH/foh_pause.c" "$BUILD/tooth-nolatch/foh/foh_pause.c" \
  '      *presentDead = 1; // stop presenting, keep draining' \
  '      /* D3-tooth: latch removed */' \
  'D3-tooth'
PAUSE_SRC=$BUILD/tooth-nolatch/foh/foh_pause.c
src_pin_verify_all
bounded_step 1200 "the no-latch build" "$BUILD/build-nolatch.log" \
  build_foh_headless_sub "$FOH/foh_dev.c" "$BUILD/tooth-nolatch/foh_dev_headless" \
    -Iport/foh -Iport/gfx
PAUSE_SRC=$FOH/foh_pause.c
RUN_LEG_ENV=("MLFK_HEADLESS_PRESENT_FAIL=1")
run_leg "$BUILD/tooth-nolatch/foh_dev_headless" legD3t "$BUILD/dead.fks" --match-timer 2
RUN_LEG_ENV=()
[ "$(cat "$BUILD/legD3t/rc.txt")" = 0 ] \
  || fail "D3-tooth: the no-latch copy exited rc $(cat "$BUILD/legD3t/rc.txt")"
D3T_FAILS="$(foh_present_fails legD3t)"
# The two runs are the same program apart from the latch, so every present
# outside the drain is common to both and cancels. What is left is exactly the
# drain's own accounting: 1 latched vs FOH_DRAIN_MAX_POLLS unlatched.
[ "$(( D3T_FAILS - D3_FAILS ))" = 599 ] \
  || fail "D3-tooth: the no-latch copy reported $D3T_FAILS failed presents and
  the real build $D3_FAILS, a difference of $(( D3T_FAILS - D3_FAILS )) — want
  exactly 599 (600 unlatched drain polls minus the 1 the latch allows). Either
  the latch is not doing what this rig says, or the difference is coming from
  somewhere other than the drain."
echo "   D3-tooth: failed presents $D3_FAILS (latched) vs $D3T_FAILS (unlatched), delta 599"
teeth=$((teeth + 1))

# ---------------------------------------------------------------------------
# TEETH on the two arms. Each is a COPY of foh_dev.c with exactly one thing
# removed, and each must fail the assertion that thing supports.
# ---------------------------------------------------------------------------
echo "=== [7] T1: without the system-menu install, ARM A's evidence disappears"
perturb "$FOH/foh_dev.c" "$BUILD/tooth-nosys/foh/foh_dev.c" \
  '  if (brLive) foh_sysmenu_hook = foh_sysmenu_open;' \
  '  if (0) foh_sysmenu_hook = foh_sysmenu_open; /* T1 */' \
  'T1'
src_pin_verify_all
bounded_step 1200 "the T1 build" "$BUILD/build-t1.log" \
  build_foh_headless "$BUILD/tooth-nosys/foh/foh_dev.c" \
    "$BUILD/tooth-nosys/foh_dev_headless" -Iport/foh -Iport/gfx
run_leg "$BUILD/tooth-nosys/foh_dev_headless" legT1 "$BUILD/sysq.fks" --system-menu
[ "$(cat "$BUILD/legT1/timedout.txt")" = 0 ] || fail "T1: the copy hung"
[ "$(cat "$BUILD/legT1/rc.txt")" = 0 ] \
  || fail "T1: the copy exited rc $(cat "$BUILD/legT1/rc.txt") — the tooth needs
  it to RUN and produce NO evidence, not to crash"
[ -e "$BUILD/legT1/shots/sysmenu-1.ppm" ] \
  && fail "T1: the copy still photographed a system menu, so A1/A3's shot
  assertions are BLIND to the dispatch being gone"
[ "$(match_frames legT1)" = "$FRAMES" ] \
  || fail "T1: the copy's match still ended early ($(match_frames legT1) frames),
  so A2's early-exit discriminator is BLIND to the QUIT arm being gone"
echo "   T1: no overlay shot, and the match ran its full $FRAMES bound"
teeth=$((teeth + 1))

echo "=== [8] T2: without the banner draw, ARM B's ink assertion goes to zero"
perturb "$FOH/foh_dev.c" "$BUILD/tooth-nobanner/foh/foh_dev.c" \
  '          tdev_vs_banner_text(&g_gfx.rz, timeUp);' \
  '          if (0) tdev_vs_banner_text(&g_gfx.rz, timeUp); /* T2: not drawn */' \
  'T2'
# `if (0)` rather than deleting the call: the banner writer is a static
# function with exactly one caller, so deleting the call makes it unused and
# the production -Werror set rejects the build — the tooth would then prove
# nothing except that -Wunused-function works. This form keeps the program
# identical except that the banner is never painted.
src_pin_verify_all
bounded_step 1200 "the T2 build" "$BUILD/build-t2.log" \
  build_foh_headless "$BUILD/tooth-nobanner/foh/foh_dev.c" \
    "$BUILD/tooth-nobanner/foh_dev_headless" -Iport/foh -Iport/gfx
run_leg "$BUILD/tooth-nobanner/foh_dev_headless" legT2 "$BUILD/fin.fks" \
  --system-menu --match-timer 2
[ "$(cat "$BUILD/legT2/rc.txt")" = 0 ] || fail "T2: the copy exited rc $(cat "$BUILD/legT2/rc.txt")"
made "$BUILD/legT2/shots/finish-banner.ppm"
mask_must_mismatch "T2 (the no-banner copy must fail the TIME! mask)" \
  "$BUILD/legT2/shots/finish-banner.ppm" "$FOH/foh_font.c" \
  1 'TIME!' 4 62 106 248 252 248 106 134
# ...and the band holds NO banner-coloured pixel at all (review-r3-r6 Medium):
# "some mismatch" would still be earned by a background that supplied part of
# the glyphs while a partial draw supplied the rest. With the draw deleted the
# only correct count is zero.
mask_match "$BUILD/legT2/shots/finish-banner.ppm" "$FOH/foh_font.c" \
  1 'TIME!' 4 62 106 248 252 248 106 134 count \
  || fail "T2: could not count the banner band"
[ "$MASK_PX" = 0 ] \
  || fail "T2: the no-banner copy's band still holds $MASK_PX banner-coloured
  pixel(s). The control is contaminated, so a mask mismatch on it does not
  isolate the deleted draw."
echo "   T2: the no-banner shot no longer matches the TIME! mask"
teeth=$((teeth + 1))

echo "=== [9] T3: the mask binds the STRING, not merely 'something drawn'"
# The discriminator itself is an owner-RATIFIED deviation (D14 precedent) and
# is NOT touched; this perturbs a COPY so that the OTHER reachable string is
# painted, and requires the measurement to notice. It is the tooth that makes
# [B2] a statement about TIME! rather than about ink: the perturbed shot must
# FAIL the TIME! mask and PASS the GAME! one.
perturb "$FOH/foh_dev.c" "$BUILD/tooth-gamebanner/foh/foh_dev.c" \
  '          const int timeUp = (G.matchTimer <= 0);' \
  '          const int timeUp = 0; /* T3: force the GAME! arm */' \
  'T3'
src_pin_verify_all
bounded_step 1200 "the T3 build" "$BUILD/build-t3.log" \
  build_foh_headless "$BUILD/tooth-gamebanner/foh/foh_dev.c" \
    "$BUILD/tooth-gamebanner/foh_dev_headless" -Iport/foh -Iport/gfx
run_leg "$BUILD/tooth-gamebanner/foh_dev_headless" legT3 "$BUILD/fin.fks" \
  --system-menu --match-timer 2
[ "$(cat "$BUILD/legT3/rc.txt")" = 0 ] || fail "T3: the copy exited rc $(cat "$BUILD/legT3/rc.txt")"
made "$BUILD/legT3/shots/finish-banner.ppm"
mask_must_mismatch "T3 (the forced-GAME! copy must fail the TIME! mask)" \
  "$BUILD/legT3/shots/finish-banner.ppm" "$FOH/foh_font.c" \
  1 'TIME!' 4 62 106 248 252 248 106 134
mask_match "$BUILD/legT3/shots/finish-banner.ppm" "$FOH/foh_font.c" \
  1 'GAME!' 4 62 106 248 252 248 106 134 \
  || fail "T3: the forced-GAME! copy does not match the GAME! mask either — it
  is failing for some reason other than the string it paints"
T3_PX=$MASK_PX
echo "   T3: the forced arm matches GAME! ($T3_PX px) and FAILS the TIME! mask"
teeth=$((teeth + 1))

echo "=== [10] T4: without the hold at all, the wall window rejects the run"
perturb "$FOH/foh_dev.c" "$BUILD/tooth-nohold/foh/foh_dev.c" \
  '          const uint64_t hold = now_ns() + TFIN_HOLD_NS;' \
  '          const uint64_t hold = now_ns(); /* T4: hold removed */' \
  'T4'
src_pin_verify_all
bounded_step 1200 "the T4 build" "$BUILD/build-t4.log" \
  build_foh_headless "$BUILD/tooth-nohold/foh/foh_dev.c" \
    "$BUILD/tooth-nohold/foh_dev_headless" -Iport/foh -Iport/gfx
run_leg "$BUILD/tooth-nohold/foh_dev_headless" legT4 "$BUILD/fin.fks" \
  --system-menu --match-timer 2
[ "$(cat "$BUILD/legT4/rc.txt")" = 0 ] || fail "T4: the copy exited rc $(cat "$BUILD/legT4/rc.txt")"
assert_same_finish legT4 T4
T4W="$(match_wall_ms legT4)"
[ "$T4W" -lt "$HOLD_FLOOR" ] \
  || fail "T4: the no-hold copy still reported ${T4W} ms for the SAME $FINF
  frames, at or above [B3]'s ${HOLD_FLOOR} ms floor — that assertion is BLIND
  to the hold"
echo "   T4: wall fell to ${T4W} ms on the same $FINF frames (real leg: ${FINW} ms)"
teeth=$((teeth + 1))

echo "=== [10b] T7: a SLIGHTLY shortened hold is rejected too"
# review-r3-r1/r2 Medium: "greater than the frames alone" accepted a hold of
# ~2.2-2.35 s — most of a defect — and T4 only ever tested total removal. This
# shortens the constant by 200 ms, which is close to [B3]'s whole tolerance, so
# passing it makes the window a statement about 2500 ms rather than about "some
# hold happened". 200 rather than 300: it sits ~150 ms under the floor, tight
# enough to pin the tolerance and loose enough that scheduler noise (which only
# ever INCREASES wall clock, and would make this tooth FAIL, never pass) does
# not turn a real bound into a flaky one.
perturb "$FOH/foh_dev.c" "$BUILD/tooth-shorthold/foh/foh_dev.c" \
  '#define TFIN_HOLD_NS 2500000000ull' \
  '#define TFIN_HOLD_NS 2300000000ull /* T7: hold shortened 200 ms */' \
  'T7'
src_pin_verify_all
bounded_step 1200 "the T7 build" "$BUILD/build-t7.log" \
  build_foh_headless "$BUILD/tooth-shorthold/foh/foh_dev.c" \
    "$BUILD/tooth-shorthold/foh_dev_headless" -Iport/foh -Iport/gfx
run_leg "$BUILD/tooth-shorthold/foh_dev_headless" legT7 "$BUILD/fin.fks" \
  --system-menu --match-timer 2
[ "$(cat "$BUILD/legT7/rc.txt")" = 0 ] || fail "T7: the copy exited rc $(cat "$BUILD/legT7/rc.txt")"
assert_same_finish legT7 T7
T7W="$(match_wall_ms legT7)"
[ "$T7W" -lt "$HOLD_FLOOR" ] \
  || fail "T7: a 2300 ms hold still reported ${T7W} ms on the SAME $FINF
  frames, at or above [B3]'s ${HOLD_FLOOR} ms floor — the window does not
  actually pin 2500 ms"
echo "   T7: the 2300 ms hold measured ${T7W} ms, under the ${HOLD_FLOOR} ms floor"
teeth=$((teeth + 1))

echo "=== [10b2] T9: a LENGTHENED hold is rejected too (the window is two-sided)"
# review-r3-r4 Medium: a floor alone lets the hold be arbitrarily LONG, and
# [B3]'s old ceiling admitted ~4.3 s. This lengthens the constant to 4.5 s and
# requires the ceiling to reject it, so the window pins 2500 ms from both
# sides rather than only from below.
perturb "$FOH/foh_dev.c" "$BUILD/tooth-longhold/foh/foh_dev.c" \
  '#define TFIN_HOLD_NS 2500000000ull' \
  '#define TFIN_HOLD_NS 4500000000ull /* T9: hold lengthened 2 s */' \
  'T9'
src_pin_verify_all
bounded_step 1200 "the T9 build" "$BUILD/build-t9.log" \
  build_foh_headless "$BUILD/tooth-longhold/foh/foh_dev.c" \
    "$BUILD/tooth-longhold/foh_dev_headless" -Iport/foh -Iport/gfx
run_leg "$BUILD/tooth-longhold/foh_dev_headless" legT9 "$BUILD/fin.fks" \
  --system-menu --match-timer 2
[ "$(cat "$BUILD/legT9/rc.txt")" = 0 ] || fail "T9: the copy exited rc $(cat "$BUILD/legT9/rc.txt")"
assert_same_finish legT9 T9
T9W="$(match_wall_ms legT9)"
[ "$T9W" -gt "$HOLD_CEIL" ] \
  || fail "T9: a 4500 ms hold still reported ${T9W} ms on the SAME $FINF
  frames, at or below [B3]'s ${HOLD_CEIL} ms ceiling — the window does not
  bound the hold from above"
echo "   T9: the 4500 ms hold measured ${T9W} ms, over the ${HOLD_CEIL} ms ceiling"
teeth=$((teeth + 1))

echo "=== [10b2b] T11: the hold ceiling bites NEAR the boundary, not only far from it"
# review-r3-r5 High: a ceiling that only rejects 4.5 s still certifies a 3 s
# shipping hold. 2900 ms is 200 ms past the accepted maximum — the same
# distance T7 sits below the floor — so the pair of them pin 2500 ms to within
# about +/-200 ms rather than merely "some hold happened".
perturb "$FOH/foh_dev.c" "$BUILD/tooth-nearhold/foh/foh_dev.c" \
  '#define TFIN_HOLD_NS 2500000000ull' \
  '#define TFIN_HOLD_NS 2900000000ull /* T11: hold lengthened 400 ms */' \
  'T11'
src_pin_verify_all
bounded_step 1200 "the T11 build" "$BUILD/build-t11.log" \
  build_foh_headless "$BUILD/tooth-nearhold/foh/foh_dev.c" \
    "$BUILD/tooth-nearhold/foh_dev_headless" -Iport/foh -Iport/gfx
run_leg "$BUILD/tooth-nearhold/foh_dev_headless" legT11 "$BUILD/fin.fks" \
  --system-menu --match-timer 2
[ "$(cat "$BUILD/legT11/rc.txt")" = 0 ] || fail "T11: the copy exited rc $(cat "$BUILD/legT11/rc.txt")"
assert_same_finish legT11 T11
T11W="$(match_wall_ms legT11)"
[ "$T11W" -gt "$HOLD_CEIL" ] \
  || fail "T11: a 2900 ms hold still reported ${T11W} ms on the SAME $FINF
  frames, at or below [B3]'s ${HOLD_CEIL} ms ceiling — the window is too loose
  to pin 2500 ms"
echo "   T11: the 2900 ms hold measured ${T11W} ms, over the ${HOLD_CEIL} ms ceiling"
teeth=$((teeth + 1))

echo "=== [10b4] T12: the system menu's QUIT ARM specifically"
# review-r3-r5 Medium: [A2] said only "a modal opened and the match ended
# early", and its only tooth (T1) removes the WHOLE dispatch. A regression in
# which some other selection — or a plain dismissal — returned result=1 would
# have satisfied it. This changes ONLY what QUIT returns, so the overlay still
# opens, still navigates and still closes on A; what must disappear is the
# early exit.
perturb "$FOH/foh_pause.c" "$BUILD/tooth-noquit/foh/foh_pause.c" \
  '      if (sel == SYS_OPT_QUIT) {
        result = 1;
        done = 1;' \
  '      if (sel == SYS_OPT_QUIT) {
        result = 0; /* T12: QUIT no longer asks the caller to leave */
        done = 1;' \
  'T12'
PAUSE_SRC=$BUILD/tooth-noquit/foh/foh_pause.c
src_pin_verify_all
bounded_step 1200 "the T12 build" "$BUILD/build-t12.log" \
  build_foh_headless_sub "$FOH/foh_dev.c" "$BUILD/tooth-noquit/foh_dev_headless" \
    -Iport/foh -Iport/gfx
PAUSE_SRC=$FOH/foh_pause.c
run_leg "$BUILD/tooth-noquit/foh_dev_headless" legT12 "$BUILD/sysq.fks" --system-menu
[ "$(cat "$BUILD/legT12/rc.txt")" = 0 ] || fail "T12: the copy exited rc $(cat "$BUILD/legT12/rc.txt")"
# the modal still opened and navigated — only the QUIT verdict changed
made "$BUILD/legT12/shots/sysmenu-1.ppm"
[ "$(match_frames legT12)" = "$FRAMES" ] \
  || fail "T12: the copy's match still ended on frame $(match_frames legT12)
  instead of running the full $FRAMES bound, so [A2] is BLIND to the QUIT arm
  itself and is only observing that SOMETHING ended the match"
echo "   T12: QUIT no longer quits — the modal still opens, the match runs all $FRAMES frames"
teeth=$((teeth + 1))

echo "=== [10b5] T13: the TARGET-loop dispatch site specifically"
# T1 removes the hook for every site at once, so it cannot tell the three
# dispatch arms apart. This disables ONLY the target loop's arm, which must
# take A4's evidence away while leaving the VS and FOH sites untouched.
perturb "$FOH/foh_dev.c" "$BUILD/tooth-notgtsys/foh/foh_dev.c" \
  '          if (sysOk && pin.menu && !prevPause.menu && !pin.start) {' \
  '          if (0 && sysOk && pin.menu && !prevPause.menu && !pin.start) { /* T13 */' \
  'T13'
src_pin_verify_all
bounded_step 1200 "the T13 build" "$BUILD/build-t13.log" \
  build_foh_headless "$BUILD/tooth-notgtsys/foh/foh_dev.c" \
    "$BUILD/tooth-notgtsys/foh_dev_headless" -Iport/foh -Iport/gfx
RUN_LEG_FLOW=$TFLOW
run_leg "$BUILD/tooth-notgtsys/foh_dev_headless" legT13 "$BUILD/tsys.fks" --system-menu
RUN_LEG_FLOW=
[ "$(cat "$BUILD/legT13/rc.txt")" = 0 ] || fail "T13: the copy exited rc $(cat "$BUILD/legT13/rc.txt")"
# The MISSING shot only means something once this run is shown to have been the
# same run (review-r3-r8 Medium): a script or flow that never launched a target
# match also produces no target-match overlay shot, and would earn the tooth
# for a reason that has nothing to do with the perturbation.
one_canonical "$BUILD/legT13/trace.txt" 'TLAUNCH ' "$TLAUNCH_RE" \
  "T13 target launch (the tooth must run the SAME match A4 does)"
one_canonical "$BUILD/legT13/err.txt" 'foh_dev match:' "$MATCH_RE" \
  "T13 match summary"
[ "$(match_frames legT13)" = "$(match_frames tsys)" ] \
  || fail "T13: the copy's target match ran $(match_frames legT13) frames but
  A4's ran $(match_frames tsys) — not the same run, so the missing shot is not
  attributable to the disabled dispatch"
[ -e "$BUILD/legT13/shots/sysmenu-1.ppm" ] \
  && fail "T13: the copy still photographed a system menu inside the target
  match, so A4 is BLIND to the target dispatch arm being gone"
# ...and the VS site is untouched by this perturbation: the same binary still
# opens the overlay on the VS leg, so T13 isolates the target arm rather than
# disabling the feature.
run_leg "$BUILD/tooth-notgtsys/foh_dev_headless" legT13vs "$BUILD/sysm.fks" --system-menu
[ "$(cat "$BUILD/legT13vs/rc.txt")" = 0 ] || fail "T13: the VS control exited rc $(cat "$BUILD/legT13vs/rc.txt")"
made "$BUILD/legT13vs/shots/sysmenu-1.ppm"
echo "   T13: no overlay in the target match, while the VS site still opens one"
teeth=$((teeth + 1))

echo "=== [10b3] T10: the system-menu label witness has its own tooth"
# review-r3-r4 Medium: T1 removes the whole dispatch, so no shot exists at all
# and mask_match is never exercised against a REAL shot whose label is missing.
# This deletes only the label draw, leaving the overlay, its shot and its OS-
# tool navigation intact — so the transcript still matches while the pixels do
# not, which is exactly the discrimination [A1] claims.
perturb "$FOH/foh_pause.c" "$BUILD/tooth-nolabel/foh/foh_pause.c" \
  '    text2_center(rz, labelY, 2, kSysOptName[sel], label);' \
  '    if (0) text2_center(rz, labelY, 2, kSysOptName[sel], label); /* T10 */' \
  'T10'
PAUSE_SRC=$BUILD/tooth-nolabel/foh/foh_pause.c
src_pin_verify_all
bounded_step 1200 "the T10 build" "$BUILD/build-t10.log" \
  build_foh_headless_sub "$FOH/foh_dev.c" "$BUILD/tooth-nolabel/foh_dev_headless" \
    -Iport/foh -Iport/gfx
PAUSE_SRC=$FOH/foh_pause.c
run_leg "$BUILD/tooth-nolabel/foh_dev_headless" legT10 "$BUILD/sysm.fks" --system-menu
[ "$(cat "$BUILD/legT10/rc.txt")" = 0 ] || fail "T10: the copy exited rc $(cat "$BUILD/legT10/rc.txt")"
made "$BUILD/legT10/shots/sysmenu-1.ppm"
# the overlay still opened and still navigated — only the label is gone
cmp "$BUILD/legT10/ostools.txt" "$BUILD/sysm/ostools.want" \
  || fail "T10: the label-free copy's OS-tool transcript differs from the real
  build's, so it is not the same program apart from the label draw"
mask_must_mismatch "T10 (the label-free copy must fail the VOLUME mask)" \
  "$BUILD/legT10/shots/sysmenu-1.ppm" "$FOH/foh_font.c" \
  2 VOLUME 2 79 93 232 236 232 93 111
# ...and the label band holds NO label-coloured pixel at all (the T2 argument):
# with the draw deleted, the only correct count for that band is zero.
mask_match "$BUILD/legT10/shots/sysmenu-1.ppm" "$FOH/foh_font.c" \
  2 VOLUME 2 79 93 232 236 232 93 111 count \
  || fail "T10: could not count the label band"
[ "$MASK_PX" = 0 ] \
  || fail "T10: the label-free copy's band still holds $MASK_PX label-coloured
  pixel(s), so the control is contaminated and its mismatch does not isolate
  the deleted draw."
echo "   T10: the label-free overlay still navigates, but fails the VOLUME mask"
teeth=$((teeth + 1))

echo "=== [10c] T8: the finish SFX call really fires"
# review-r3-r1 High: with an audio plane loaded, `foh_snd("time")` starts one
# mixer voice. Removing ONLY that call must drop the run's voice-start total by
# exactly one — everything else in the two runs is identical, so the difference
# is the finish sfx and nothing else. Without this tooth, [B5] would prove only
# that an audio plane was loaded, not that the block's own line ran.
perturb "$FOH/foh_dev.c" "$BUILD/tooth-nosnd/foh/foh_dev.c" \
  '          foh_snd(timeUp ? "time" : "game");' \
  '          if (0) foh_snd(timeUp ? "time" : "game"); /* T8: not played */' \
  'T8'
src_pin_verify_all
bounded_step 1200 "the T8 build" "$BUILD/build-t8.log" \
  build_foh_headless "$BUILD/tooth-nosnd/foh/foh_dev.c" \
    "$BUILD/tooth-nosnd/foh_dev_headless" -Iport/foh -Iport/gfx
run_leg "$BUILD/tooth-nosnd/foh_dev_headless" legT8 "$BUILD/fin.fks" \
  --system-menu --match-timer 2
[ "$(cat "$BUILD/legT8/rc.txt")" = 0 ] || fail "T8: the copy exited rc $(cat "$BUILD/legT8/rc.txt")"
[ "$(match_frames legT8)" = "$FINF" ] \
  || fail "T8: the copy's match ended on frame $(match_frames legT8), not $FINF —
  the two runs are not the same program, so the voice difference is not the sfx"
T8_VOICES="$(voice_starts legT8)"
[ "$(( FIN_VOICES - T8_VOICES ))" = 1 ] \
  || fail "T8: the no-sfx copy started $T8_VOICES voices and the real build
  $FIN_VOICES, a difference of $(( FIN_VOICES - T8_VOICES )) — want exactly 1.
  The finish block's foh_snd call is not the thing being measured."
echo "   T8: voice starts $FIN_VOICES vs $T8_VOICES — the finish sfx is exactly one of them"
teeth=$((teeth + 1))

echo "=== [11] T5: without MEX_CSS, the finish does not re-enter the FOH"
perturb "$FOH/foh_dev.c" "$BUILD/tooth-nocss/foh/foh_dev.c" \
  '          g_mexit = MEX_CSS; // endGame: changeGamemode(2) (main.js:1386-1388)' \
  '          g_mexit = MEX_OS; /* T5: destination changed */' \
  'T5'
src_pin_verify_all
bounded_step 1200 "the T5 build" "$BUILD/build-t5.log" \
  build_foh_headless "$BUILD/tooth-nocss/foh/foh_dev.c" \
    "$BUILD/tooth-nocss/foh_dev_headless" -Iport/foh -Iport/gfx
run_leg "$BUILD/tooth-nocss/foh_dev_headless" legT5 "$BUILD/fin.fks" \
  --system-menu --match-timer 2
[ "$(cat "$BUILD/legT5/rc.txt")" = 0 ] || fail "T5: the copy exited rc $(cat "$BUILD/legT5/rc.txt")"
grep -qE "^foh_dev foh: $FOH_MAX ticks, $NUM transitions, $NUM shots, $NUM render skips, $NUM failed presents, launched=0\$" \
  "$BUILD/legT5/err.txt" \
  && fail "T5: the copy still produced a second FOH phase at the bound, so
  [B4] is BLIND to the finish block's destination"
one_canonical "$BUILD/legT5/err.txt" 'foh_dev foh:' "$FOH_LAUNCHED_RE" \
  "T5 single FOH phase (no re-entry)"
echo "   T5: the finish left the process instead of re-entering the FOH"
teeth=$((teeth + 1))

echo "=== [11b] T6: without the HUD expiry clamp, the finish frame is CORRUPT"
# THE HEADLINE FINDING'S OWN TOOTH. The clamp in gfx_render_overlay_timer is a
# fix this rig measured, and a fix without a witness is an assertion. The copy
# below differs from gfx_overlay.c in exactly one expression — the clamp — and
# must still be caught.
#
# A39 (2026-08-24): THIS TOOTH HAD STOPPED BITING, AND THE REPAIR IS BELOW.
# It used to require the unclamped copy to ABORT: with `mt = raw` the finish
# frame's matchTimer is negative (-0.00004 at --match-timer 2), so
# `Math.floor(mt/60)` is -1 and the minutes string acquires a '-', which font
# 0 did not have — `glyphs: font 0 has no glyph '-'`, a loud gfx_fatal.
#   * WHERE IT STOPPED: commit 844b8a6 (A14, 2026-08-05) widened the browser
#     glyph atlas from 43 to 179 records, and specs 0 and 3 gained MENU_CHARS
#     — which contains '-'. port/gfx/vfxglyphs-frozen.txt has carried
#     `GLYPH 0 45` ever since. A14 was about menu text and had no way to know
#     it was standing on this tooth's trigger.
#   * SO THE CRASH IS GONE, AND THE CLAMP IS NOT. Without it the HUD now
#     silently DRAWS "-1:-0" on the finish frame of every timed-out match
#     instead of "00:00" — a wrong clock rather than a dead process. The
#     clamp's real argument never depended on the atlas anyway (upstream
#     never renders this frame at all: main.js:1243's `playing` guard and
#     finishGame's `playing = false`), so the fix stays and the TOOTH moves
#     to the consequence that exists today.
#   * THE NEW OBSERVABLE is the finish-banner shot. It is photographed AFTER
#     gfx_render_frame drew the HUD and the banner is a centred TIME! at
#     y 106..133, while the timer sits at the top of the screen — so the two
#     do not overlap and the timer's text is IN the picture. Same leg, same
#     args, same expiry frame as ARM B's `fin`, one expression apart: if the
#     clamp does nothing, the two shots are identical, and that is the
#     failure this asserts. Deleting the clamp is still caught; what changed
#     is only which instrument catches it.
# NOT a loosening: the copy must still reach the finish frame, still run the
# same match to the same frame (assert_same_finish), and still be PROVEN to
# render differently. HARD RULE 3 respected — the tooth was repaired, not
# relaxed.
perturb "$GFX/gfx_overlay.c" "$BUILD/tooth-noclamp/gfx/gfx_overlay.c" \
  '    const double mt = raw > 0.0 ? raw : 0.0;' \
  '    const double mt = raw; /* T6: expiry clamp removed */' \
  'T6'
OVERLAY_SRC=$BUILD/tooth-noclamp/gfx/gfx_overlay.c
src_pin_verify_all
bounded_step 1200 "the T6 build" "$BUILD/build-t6.log" \
  build_foh_headless_sub "$FOH/foh_dev.c" "$BUILD/tooth-noclamp/foh_dev_headless" \
    -Iport/foh -Iport/gfx
OVERLAY_SRC=$GFX/gfx_overlay.c
run_leg "$BUILD/tooth-noclamp/foh_dev_headless" legT6 "$BUILD/fin.fks" \
  --system-menu --match-timer 2
expect_ok legT6  # rc 0 AND not killed at the bound; the copy must COMPLETE now
# The copy has to have run the SAME match to the SAME expiry frame, and to
# have produced the finish block's own artifact — otherwise a shot that
# differs says nothing about the clamp (the assert_same_finish argument at
# [5], applied for the same reason).
assert_same_finish legT6 "T6"
# A '-' in font 0 is what USED to abort here and no longer does (A14, above).
# If a future change ever removes it again, this copy will die on the
# missing-glyph path and the leg will fail LOUDLY at expect_ok rather than
# silently changing meaning — so the old trigger is still covered, from the
# other side.
cmp -s "$BUILD/fin/shots/finish-banner.ppm" \
       "$BUILD/legT6/shots/finish-banner.ppm" \
  && fail "T6: the unclamped copy rendered the finish frame BYTE-IDENTICALLY
  to the real build. The clamp is then provably doing nothing on the one
  frame it exists for, so either the leg stopped reaching a negative
  matchTimer or the clamp is genuinely redundant and must be RETIRED on the
  record — never by relaxing this assertion (HARD RULE 3, and A39 is the
  worked example of what to do instead)."
echo "   T6: the unclamped copy reaches frame $FINF and draws a DIFFERENT"
echo "       finish frame (the negative clock the clamp exists to keep off the HUD)"
teeth=$((teeth + 1))

# --- [12] no-commit guard ----------------------------------------------------
tree_fingerprint \
  || fail "could not fingerprint the working tree after the run"
git_dirty_after="$FP_OUT"
[[ "$git_dirty_after" =~ ^[0-9a-f]{64}$ ]] \
  || fail "post-run tree fingerprint '$git_dirty_after' is not a sha256 digest"
[ "$git_dirty_before" = "$git_dirty_after" ] \
  || fail "THE WORKING TREE CHANGED ACROSS THIS RUN — status, tracked diff
  bytes or untracked file content differ from the pre-run fingerprint
  ($git_dirty_before -> $git_dirty_after). Two causes, and they need different
  responses:
    (a) this rig wrote outside the ignored build dirs. That is a defect here
        and it would land in the driver's commit — find it with
        \`git status --porcelain\` and \`git diff --stat\`.
    (b) something ELSE edited the tree while this ran. That is the normal case
        in a multi-agent session and it is not a rig failure: nothing above
        this line is invalidated by it (every leg was judged against artifacts
        this run produced), so re-run when the tree is quiet.
  Neither is quieted by widening the fingerprint."

# --- [13] the verdict, with its counts ASSERTED before they are printed ------
# review-r3-r2 Medium: printing whatever the counters happen to hold makes the
# verdict line a report rather than a contract — deleting a whole leg and its
# increment together still exits 0 and still prints a plausible line. These are
# the counts this script is defined to produce; a change to what it covers has
# to change them here too, deliberately.
WANT_SYSMENU=4
WANT_VSFINISH=1
WANT_DRAINS=3
WANT_TEETH=15
[ "$sysmenu" = "$WANT_SYSMENU" ] && [ "$vsfinish" = "$WANT_VSFINISH" ] \
  && [ "$drains" = "$WANT_DRAINS" ] && [ "$teeth" = "$WANT_TEETH" ] \
  || fail "COVERAGE SHORTFALL: this run counted sysmenu=$sysmenu vsfinish=$vsfinish
  drains=$drains teeth=$teeth, but this script is defined to run
  sysmenu=$WANT_SYSMENU vsfinish=$WANT_VSFINISH drains=$WANT_DRAINS
  teeth=$WANT_TEETH. A leg or a tooth did not run. Do NOT adjust these numbers
  to match the run — adjust them only when the coverage is deliberately
  changed."

echo "LIVE ARMS OK (sysmenu=$sysmenu vsfinish=$vsfinish drains=$drains teeth=$teeth)"
