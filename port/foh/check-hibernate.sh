#!/usr/bin/env bash
# port/foh/check-hibernate.sh — fix_plan A26 / DEVIATION D53: hibernate/resume.
#
# THE OWNER-VISIBLE OUTCOME THIS ASSERTS, and nothing narrower:
#   *"closing screen and opening it doesn't come back to the game though.
#     i want it to, is it possible?"*
# Closing the lid runs `powerdown schedule 0.1` -> SIGUSR1 -> 100 ms ->
# `powerdown now`. So the app is killed, not suspended, and "coming back"
# means recording a place on SIGUSR1 and going back to it on the next boot.
# Every leg below drives the REAL code path end to end: the real launcher
# idiom, a real signal, the real foh_persist chokepoint, the real FOH screen
# machine. Nothing is hand-poked into the state, and nothing is asserted from
# a log line alone where a transition trace can say it instead.
#
# LEGS
#   [3] LAUNCHER — the four load-bearing lines are EXTRACTED FROM
#       port/gfx/opk/mlfk-foh.sh (never retyped) and run against a stand-in
#       child, proving the trap fires IMMEDIATELY and the child's real rc
#       reaches opk.rc. Both were measured defects of the obvious forms: a
#       foreground child defers the trap (fired 4 s late), and `wait || rc=$?`
#       leaves a clean exit reported as RC=138.
#   [4] HIBERNATE — a paced FOH run parked on the CSS takes a real SIGUSR1;
#       assert it saved once, named the screen, exited 0, and that the file on
#       disk carries the matching `resume` row.
#   [5] RESUME — boot again on that same persist dir; assert the app comes up
#       ON THE CSS. Judged from the FLOW TRANSITION TRACE (a resumed boot runs
#       no `startup -> title` transition and no `-> css` transition, because it
#       is already there), not merely from the stderr line.
#   [6] TEETH — four, orthogonal:
#       T1 domain    a VALID file (checksum recomputed) whose resume row names
#                    FOH_MATCH — a real screen that is NOT a resume target.
#                    Must be REFUSED as corrupt, not half-applied.
#       T2 absence   the same session with the row absent (a v6 file, migrated).
#                    Must boot cold, loudly, with settings PRESERVED.
#       T3 driver    foh_dev.c's hibernate arm perturbed to skip the stamp.
#       T4 launcher  the extracted idiom reverted to the foreground form.
#
# NOT IN THIS SCRIPT, on purpose: the 100 ms budget. That is a DEVICE number
# and a host measurement of it would be a lie — /mnt is vfat with no journal
# and the save fsyncs twice. It was measured on the FunKey-S directly and the
# numbers are recorded at foh_dev.c's hibernate block (20 real
# foh_persist_save calls against /mnt: min 8.1 ms / median ~12.2 / max 49.6;
# end-to-end signal-to-exit through the real launcher idiom: 20/30/30/60 ms of
# the 100 ms grace). Leg [4] still bounds the HOST latency, which proves the
# arm does exactly one save and no extra work — the part that is portable.
set -euo pipefail

FOH=port/foh
GFX=port/gfx
CAL=port/sim/calib
TABLES=pipeline/build/hib-tables
BUILD=$FOH/build/hibernate
DEVFOH=$FOH/check-device-foh.sh
LAUNCHER=$GFX/opk/mlfk-foh.sh

fail() { echo "HIBERNATE FAIL: $1" >&2; exit 1; }
grammar_die() { echo "HIBERNATE FAIL: $1" >&2; exit 2; }
relay_lines() { sed 's/^/  | /'; }

made() {
  local f
  for f in "$@"; do [ -s "$f" ] || fail "expected artifact missing/empty: $f"; done
}

LOCK=$FOH/build/hibernate.lock
mkdir -p "$FOH/build"
mkdir "$LOCK" 2>/dev/null || fail "another run holds $LOCK (remove it only if
  you have proven no other run is live)"
trap 'rmdir "$LOCK" 2>/dev/null || true' EXIT

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
  printf '%s\n%s\n%s\n' "$status" "$diff" "$hashes" | shasum -a 256 | cut -d' ' -f1
}
git_dirty_before="$(tree_fingerprint)" \
  || fail "could not fingerprint the working tree (fails CLOSED)"
[[ "$git_dirty_before" =~ ^[0-9a-f]{64}$ ]] \
  || fail "tree fingerprint '$git_dirty_before' is not a sha256 digest"

# --- [1] data planes ---------------------------------------------------------
echo "=== [1] data planes (tables/stages/targets/assets)"
if [ ! -s "$TABLES/ml_targets.c" ] || [ ! -s "$TABLES/assets/menu.img1" ]; then
  { bash pipeline/extractor/build-extractor.sh; } 2>&1 | relay_lines
  { node pipeline/run.js --only animations,tables,stages,targets,assets \
      --out "$TABLES"; } 2>&1 | relay_lines
fi
made "$TABLES/ml_tables.c" "$TABLES/ml_stages.c" "$TABLES/ml_targets.c" \
     "$TABLES/assets/menu.img1"
export MLFK_MENU_IMG1="$PWD/$TABLES/assets/menu.img1"
rm -rf "$BUILD"
mkdir -p "$BUILD"

# --- [2] host twin, built by check-device-foh.sh's OWN recipe ----------------
# The TU list is EXTRACTED, never copied: one definition of "how foh_dev.c is
# built for the host" in the tree (the check-mexit-reentry.sh pattern, and its
# anchoring is inherited verbatim so this cannot silently build something else).
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
' "$DEVFOH" > "$RECIPE"
made "$RECIPE"
grep -qxF 'build_foh_headless() { # <foh_dev_src> <out> [extra cc args...]' \
  "$RECIPE" || grammar_die "extracted recipe does not carry build_foh_headless"
grep -qxF 'SIM_TUS=(' "$RECIPE" \
  || grammar_die "extracted recipe does not carry the SIM_TUS list"
# shellcheck disable=SC1090 — derived above, not tracked
. "$RECIPE"
build_foh_headless "$FOH/foh_dev.c" "$BUILD/foh_dev_headless" \
  || fail "host twin build failed"
made "$BUILD/foh_dev_headless"

# The park flow: startup -> title -> menu-top -> CSS by tick 381, then a long
# tail of NOTHING. In flow mode the FOH runs exactly as many ticks as the flow
# is long (foh_dev.c's `fohLimit = g_flow_frames`), so the tail is what buys
# the wall clock to signal in; the screen is the CSS for every tick of it and
# no timer leaves that screen, so the signal cannot race the navigation.
PARK=$BUILD/park.flow
cat > "$PARK" <<'EOF'
FLOW1
I 1 -
I 375 S
I 376 -
I 380 A
I 381 -
END 20000
EOF
made "$PARK"

# The idle flow: NO input at all, and short enough to END on its own. Every
# leg that judges a TRANSITION TRACE uses this one, for two reasons the first
# shape of this check got wrong:
#   * a run that is killed — or that _exit()s on the hibernate path — never
#     flushes --flow-out, so a trace leg must let its run finish;
#   * replaying the NAVIGATING flow against a resumed boot would fire START on
#     the CSS, i.e. attempt a launch, and the trace would then differ for a
#     reason that has nothing to do with resuming.
# With no input, a COLD boot still walks startup -> title on its own timer,
# and that single transition is the whole discriminator.
IDLE=$BUILD/idle.flow
cat > "$IDLE" <<'EOF'
FLOW1
I 1 -
END 900
EOF
made "$IDLE"

# 2 ms/tick: the FOH's only wall-clock coupling is its pace epoch, so a short
# budget runs the same ticks sooner. 381 ticks of navigation ~ 0.8 s; the
# 20000-tick park flow runs ~40 s (longer than any wait below); idle ~1.8 s.
BUDGET=2000000
run_foh() { # <flow> <persist-dir> <stderr-out> <trace-out>
  local flow="$1" pdir="$2" errf="$3" trf="$4"
  MLFK_PERSIST_DIR="$pdir" "$BUILD/foh_dev_headless" \
    --flow "$flow" --input flow --flow-out "$trf" \
    --pace 1 --budget-ns "$BUDGET" > "$errf" 2>&1
}

# --- [3] the launcher idiom, EXTRACTED from mlfk-foh.sh ----------------------
echo "=== [3] launcher signal route (lines extracted from $LAUNCHER)"
made "$LAUNCHER"
IDIOM=$BUILD/idiom.sh
# Anchored, single-match: the four lines that make hibernate reachable at all.
for pat in '    >> "$LOG" 2>&1 &' '  APP=$!' \
           "  trap 'kill -USR1 \"\$APP\" 2>/dev/null' USR1" \
           '  wait "$APP"; rc=$?' \
           '  if [ "$rc" -gt 128 ]; then wait "$APP"; rc=$?; fi'; do
  n="$(grep -cxF "$pat" "$LAUNCHER")" || true
  [ "$n" = 1 ] || grammar_die "mlfk-foh.sh has $n copies of the line
  '$pat' (want exactly 1) — the hibernate signal route moved or was reverted"
done
# Build the test launcher from the launcher's OWN bytes for the four lines
# that matter, with a stand-in child in place of foh_device.
{
  echo '#!/bin/sh'
  echo 'LOG=$1'
  echo 'CHILD=$2'
  echo ': > "$LOG"'
  echo '"$CHILD" \'
  grep -xF '    >> "$LOG" 2>&1 &' "$LAUNCHER"
  grep -xF '  APP=$!' "$LAUNCHER"
  grep -xF "  trap 'kill -USR1 \"\$APP\" 2>/dev/null' USR1" "$LAUNCHER"
  grep -xF '  wait "$APP"; rc=$?' "$LAUNCHER"
  grep -xF '  if [ "$rc" -gt 128 ]; then wait "$APP"; rc=$?; fi' "$LAUNCHER"
  echo 'echo "RC=$rc" > "$LOG.rc"'
} > "$IDIOM"
made "$IDIOM"

# The stand-in: a long-lived child with a USR1 handler that stamps the elapsed
# time and exits with a DISTINCT code, so "the trap fired promptly" and "the
# child's own rc arrived" are two separate observations.
CHILD=$BUILD/child.sh
cat > "$CHILD" <<'EOF'
#!/bin/sh
trap 'echo GOT_USR1; exit 42' USR1
i=0
while [ $i -lt 600 ]; do i=$((i + 1)); sleep 0.05 & wait $!; done
echo TIMED_OUT
exit 9
EOF
chmod +x "$CHILD" "$IDIOM"

idiom_run() { # <label> <script> -> writes $BUILD/<label>.log[.rc]
  local label="$1" script="$2" pid t0 t1 n=0
  rm -f "$BUILD/$label.log" "$BUILD/$label.log.rc"
  /bin/sh "$script" "$BUILD/$label.log" "$CHILD" &
  pid=$!
  sleep 1
  t0="$(date +%s)"
  kill -USR1 "$pid" 2>/dev/null || true
  # Bounded: a DEFERRED trap (the foreground form) cannot beat this.
  while kill -0 "$pid" 2>/dev/null && [ "$n" -lt 30 ]; do sleep 0.2; n=$((n + 1)); done
  t1="$(date +%s)"
  kill -9 "$pid" 2>/dev/null || true
  # 2>/dev/null: T4's launcher dies OF the signal, and bash then prints a job
  # status line ("User defined signal 1") that reads like an error in a passing
  # check. It is the tooth working — see T4.
  wait "$pid" 2>/dev/null || true
  echo "$((t1 - t0))" > "$BUILD/$label.elapsed"
}

idiom_run idiom "$IDIOM"
made "$BUILD/idiom.log" "$BUILD/idiom.log.rc"
grep -qxF 'GOT_USR1' "$BUILD/idiom.log" \
  || fail "[3] the launcher idiom did not deliver SIGUSR1 to its child"
grep -qxF 'RC=42' "$BUILD/idiom.log.rc" \
  || { cat "$BUILD/idiom.log.rc" >&2
       fail "[3] opk.rc did not carry the CHILD's exit code (42). A 138 here is
  the measured \`wait \"\$APP\" || rc=\$?\` defect: a clean exit reported as a
  crash to every evidence parser that reads opk.rc"; }
[ "$(cat "$BUILD/idiom.elapsed")" -le 3 ] \
  || fail "[3] the trap took >3 s to fire — that is the DEFERRED-trap failure
  the background+wait form exists to avoid (the grace is 0.1 s)"
echo "   trap fired immediately, child rc 42 reached opk.rc"

# --- [4] hibernate: a real SIGUSR1 to a real run -----------------------------
echo "=== [4] hibernate (real SIGUSR1 -> the foh_persist chokepoint)"
PDIR=$BUILD/persist
rm -rf "$PDIR"
mkdir -p "$PDIR"
rm -f "$BUILD/hib.err" "$BUILD/hib.trace"
MLFK_PERSIST_DIR="$PDIR" "$BUILD/foh_dev_headless" \
  --flow "$PARK" --input flow --flow-out "$BUILD/hib.trace" \
  --pace 1 --budget-ns "$BUDGET" > "$BUILD/hib.err" 2>&1 &
APPPID=$!
sleep 3   # >> the flow's 381 navigation ticks at 2 ms; parked on the CSS
kill -0 "$APPPID" 2>/dev/null \
  || fail "[4] the app exited before the signal (see $BUILD/hib.err)"
T0=$(date +%s)
kill -USR1 "$APPPID"
n=0
while kill -0 "$APPPID" 2>/dev/null && [ "$n" -lt 50 ]; do sleep 0.1; n=$((n + 1)); done
T1=$(date +%s)
if kill -0 "$APPPID" 2>/dev/null; then
  kill -9 "$APPPID" 2>/dev/null || true
  set +e; wait "$APPPID" >/dev/null 2>&1; set -e
  cat "$BUILD/hib.err" >&2
  fail "[4] the app did not exit within 5 s of SIGUSR1 — the hibernate arm
  never fired, and on the device that is the whole 0.1 s grace gone"
fi
set +e
wait "$APPPID"; APPRC=$?
set -e
[ "$APPRC" = 0 ] || { cat "$BUILD/hib.err" >&2
  fail "[4] the hibernate arm exited $APPRC, want 0"; }
made "$BUILD/hib.err"
grep -qxF 'foh_dev: hibernate from=css resume=css' "$BUILD/hib.err" \
  || { cat "$BUILD/hib.err" >&2
       fail "[4] no 'hibernate from=css resume=css' line — the app either
  missed the signal or was not on the screen this flow parks it on"; }
nsaved="$(grep -cxF 'foh_persist: saved' "$BUILD/hib.err")" || true
[ "$nsaved" = 1 ] \
  || fail "[4] the hibernate arm performed $nsaved saves (want exactly 1) —
  inside a 0.1 s grace a second SD write is a second chance to miss it"
[ "$((T1 - T0))" -le 2 ] \
  || fail "[4] signal-to-exit took $((T1 - T0)) s on the HOST. This does not
  measure the device budget (that number is on the FunKey-S and is recorded at
  foh_dev.c's hibernate block); it proves the arm does one save and no more"
made "$PDIR/mlfk-persist.dat"
grep -qxE '^MLFKPERSIST7$' "$PDIR/mlfk-persist.dat" \
  || grammar_die "[4] the published file is not MLFKPERSIST7"
grep -qxE '^resume 06$' "$PDIR/mlfk-persist.dat" \
  || { grep -n 'resume' "$PDIR/mlfk-persist.dat" >&2 || true
       grammar_die "[4] the file does not carry 'resume 06' (FOH_CSS)"; }
echo "   hibernated on the CSS; one save; resume row published"

# --- [5] resume: the next boot comes back to the CSS -------------------------
echo "=== [5] resume (next boot, same persist dir)"
# The COLD reference: an identical run on a FRESH dir. Same binary, same flow,
# same everything except the resume record — so the only thing the two traces
# can differ by is the feature under test.
COLD=$BUILD/cold-persist
rm -rf "$COLD"; mkdir -p "$COLD"
run_foh "$IDLE" "$COLD" "$BUILD/cold.err" "$BUILD/cold.trace"
made "$BUILD/cold.err" "$BUILD/cold.trace"
grep -qE '^T [0-9]+ startup title timer$' "$BUILD/cold.trace" \
  || grammar_die "[5] the COLD reference walked no 'startup title timer'
  transition — the discriminator this leg judges on is not present, so a pass
  would be vacuous"
! grep -q 'foh_dev: resumed' "$BUILD/cold.err" \
  || fail "[5] the COLD reference resumed — its persist dir is not fresh"

run_foh "$IDLE" "$PDIR" "$BUILD/res.err" "$BUILD/res.trace"
made "$BUILD/res.err" "$BUILD/res.trace"
grep -qxF 'foh_dev: resumed screen=css' "$BUILD/res.err" \
  || { cat "$BUILD/res.err" >&2; fail "[5] the boot did not resume to the CSS"; }
# THE OWNER-VISIBLE ASSERTION, from the transition trace rather than the log:
# the cold boot walks startup -> title on its own timer; the resumed boot is
# already on the CSS and walks nothing.
! grep -qE '^T [0-9]+ startup title timer$' "$BUILD/res.trace" \
  || { grep -E '^T ' "$BUILD/res.trace" >&2
       fail "[5] the RESUMED boot still walked startup -> title: it came up on
  the boot screen, which is exactly the symptom this row exists to fix"; }
echo "   resumed on the CSS; the cold boot's startup walk is absent"

# --- [5b] the TARGET BUILDER resumes into ITSELF, with the work ------------
#
# A45-T4 sent the builder to the menu top on resume, and it was right to:
# the document was not persisted, so resuming into an empty editor would
# have claimed the player's work was still there. 2026-08-26 makes the claim
# TRUE instead of removing it — FohTbuildOps.suspend publishes the document
# as `tbdoc.mlstage` through A45 T2's contract and the same atomic publish
# the SD slots use.
#
# THE ASSERTION IS BYTE-IDENTITY ACROSS THE ROUND TRIP, not a log line. Park
# in the builder, EDIT (place a target, so the document is the player's and
# not D51's template), hibernate, keep the published file; boot again, let
# the resume restore it, hibernate a second time, and require the two
# published documents to be BYTE-IDENTICAL. A resume that silently reset to
# the template passes every log check and fails this one.
echo "=== [5b] the builder resumes into itself, with the document"
TBPARK=$BUILD/tbpark.flow
cat > "$TBPARK" <<'TBEOF'
FLOW1
I 1 -
I 375 S
I 376 -
I 380 D
I 381 -
I 385 D
I 386 -
I 390 A
I 391 -
I 400 N
I 401 -
I 405 N
I 406 -
I 410 N
I 411 -
I 415 N
I 416 -
I 420 N
I 421 -
I 430 A
I 431 -
END 20000
TBEOF
TBDIR=$BUILD/tb-persist
rm -rf "$TBDIR"; mkdir -p "$TBDIR"
rm -f "$BUILD/tbhib.err" "$BUILD/tbhib.trace"
MLFK_PERSIST_DIR="$TBDIR" "$BUILD/foh_dev_headless" \
  --flow "$TBPARK" --input flow --flow-out "$BUILD/tbhib.trace" \
  --pace 1 --budget-ns "$BUDGET" > "$BUILD/tbhib.err" 2>&1 &
TBPID=$!
sleep 3
kill -0 "$TBPID" 2>/dev/null \
  || { cat "$BUILD/tbhib.err" >&2
       fail "[5b] the app exited before the signal"; }
kill -USR1 "$TBPID"
n=0
while kill -0 "$TBPID" 2>/dev/null && [ "$n" -lt 50 ]; do sleep 0.1; n=$((n + 1)); done
if kill -0 "$TBPID" 2>/dev/null; then
  kill -9 "$TBPID" 2>/dev/null || true
  set +e; wait "$TBPID" >/dev/null 2>&1; set -e
  cat "$BUILD/tbhib.err" >&2
  fail "[5b] the app did not exit within 5 s of SIGUSR1"
fi
set +e; wait "$TBPID"; TBRC=$?; set -e
[ "$TBRC" = 0 ] || { cat "$BUILD/tbhib.err" >&2
  fail "[5b] the hibernate arm exited $TBRC, want 0"; }
grep -qxF 'foh_dev: hibernate from=target-builder resume=target-builder' \
  "$BUILD/tbhib.err" \
  || { cat "$BUILD/tbhib.err" >&2
       fail "[5b] the builder did not arm a TARGET-BUILDER resume — either the
  flow did not park there, or suspend() failed and it downgraded"; }
made "$TBDIR/tbdoc.mlstage"
cp "$TBDIR/tbdoc.mlstage" "$BUILD/tbdoc.before"
# THE EDIT MUST BE REAL, or the round trip proves nothing: a document that is
# still D51's template would round-trip just as byte-identically.
TPL=$BUILD/tbdoc.template
rm -rf "$BUILD/tb-tpl"; mkdir -p "$BUILD/tb-tpl"
TPLFLOW=$BUILD/tbtpl.flow
sed '/^I 4[0-3][0-9] /d' "$TBPARK" > "$TPLFLOW"
rm -f "$BUILD/tbtpl.err"
MLFK_PERSIST_DIR="$BUILD/tb-tpl" "$BUILD/foh_dev_headless" \
  --flow "$TPLFLOW" --input flow --flow-out "$BUILD/tbtpl.trace" \
  --pace 1 --budget-ns "$BUDGET" > "$BUILD/tbtpl.err" 2>&1 &
TPLPID=$!
sleep 3
kill -USR1 "$TPLPID" 2>/dev/null || true
n=0
while kill -0 "$TPLPID" 2>/dev/null && [ "$n" -lt 50 ]; do sleep 0.1; n=$((n + 1)); done
kill -9 "$TPLPID" 2>/dev/null || true
set +e; wait "$TPLPID" >/dev/null 2>&1; set -e
made "$BUILD/tb-tpl/tbdoc.mlstage"
cp "$BUILD/tb-tpl/tbdoc.mlstage" "$TPL"
cmp -s "$TPL" "$BUILD/tbdoc.before" \
  && fail "[5b] the edited document is byte-identical to the UNEDITED one —
  the flow's tool walk or its A press did not change the document, so the
  round-trip assertion below would hold for a resume that reset to the
  template (dead leg)"
echo "   the parked document differs from the untouched template"
# ...now boot again on the same dir and let the resume restore it
rm -f "$BUILD/tbres.err" "$BUILD/tbres.trace"
TBRESFLOW=$BUILD/tbres.flow
printf 'FLOW1\nI 1 -\nEND 20000\n' > "$TBRESFLOW"
MLFK_PERSIST_DIR="$TBDIR" "$BUILD/foh_dev_headless" \
  --flow "$TBRESFLOW" --input flow --flow-out "$BUILD/tbres.trace" \
  --pace 1 --budget-ns "$BUDGET" > "$BUILD/tbres.err" 2>&1 &
TBPID2=$!
sleep 2
grep -qxF 'foh_dev: resumed screen=target-builder' "$BUILD/tbres.err" \
  || { kill -9 "$TBPID2" 2>/dev/null || true; cat "$BUILD/tbres.err" >&2
       fail "[5b] the boot did not resume to the target builder"; }
grep -qxF 'foh_dev: builder document restored' "$BUILD/tbres.err" \
  || { kill -9 "$TBPID2" 2>/dev/null || true; cat "$BUILD/tbres.err" >&2
       fail "[5b] the resume did not restore the document"; }
[ -e "$TBDIR/tbdoc.mlstage" ] \
  && { kill -9 "$TBPID2" 2>/dev/null || true
       fail "[5b] tbdoc.mlstage survived the resume — it must be CONSUMED, or
  a later ordinary visit resurrects work the player already got back"; }
kill -USR1 "$TBPID2"
n=0
while kill -0 "$TBPID2" 2>/dev/null && [ "$n" -lt 50 ]; do sleep 0.1; n=$((n + 1)); done
kill -9 "$TBPID2" 2>/dev/null || true
set +e; wait "$TBPID2" >/dev/null 2>&1; set -e
made "$TBDIR/tbdoc.mlstage"
cmp "$BUILD/tbdoc.before" "$TBDIR/tbdoc.mlstage" \
  || fail "[5b] the document did NOT survive the round trip: the file published
  by the second hibernate differs from the first. A resume that lands on the
  builder with someone else's stage is worse than not resuming at all."
echo "   builder resumed with the SAME document, byte for byte; file consumed"

# --- [5c] the CHARACTER SELECT resumes SET UP THE WAY IT WAS LEFT -----------
# Ticket #25 / owner ruling 2026-08-27. The owner's report was that closing
# the lid on the CSS gave back a screen with the opponent gone, the mode
# reverted and the cursor at the top: only the character survived, because
# only the character had a row. Eight rows later, this leg is what says so.
#
# IT IS SHAPED EXACTLY LIKE [5b], because [5b] already learnt the two things
# that make a resume round trip mean anything:
#
#   * THE ASSERTION IS BYTE-IDENTITY ACROSS THE ROUND TRIP, not a log line. A
#     resume that silently reset the CSS to a fresh screen would still print
#     `resumed screen=css` and would still hibernate again cleanly; what it
#     cannot do is publish the same bytes the second time.
#
#   * AND THE PARKED STATE MUST DIFFER FROM A FRESH ONE, per row, or the
#     round trip is a round trip on nothing. A field that was never set sits
#     at its default, survives trivially, and proves only that the default is
#     stable. So the flow below really works the screen — arms port 1 as CPU,
#     drags its level knob to the bottom of the rail, flips the mode ribbon,
#     moves the hand and picks a token up — and every new row is then
#     required to DIFFER from the row the same binary writes on a screen
#     nobody touched. [5b] proves the same thing with its `tbdoc.template`
#     comparison; this is that guard, made per field.
echo "=== [5c] the CSS resumes as it was left (ticket #25)"

# The flow is written in CLAMP-THEN-COUNT form: hold up+left long enough to
# pin the cursor at (0,0) — foh_hand_step clamps, so "long enough" is exact —
# and then count frames out from there. FOH_CURSOR_VX/VY are 2.40 and 3.84
# px/frame (foh_hand.h), so every position below is an exact multiple and no
# step lands on a boundary. Frame budget: the CSS is reached at tick 381 and
# everything here finishes by 801, well inside the 3 s (1500 tick) wait.
#
#   500  hand at (0, 0)                     clamped
#   550  hand at (120.0, 0)                 50 frames right
#   553  hand at (120.0, 11.52)             3 frames down -> inside the mode
#                                           ribbon (x 104..178, y 4..22)
#   555  A                                  versusMode 0 -> 1
#   620  hand at (0, 0)                     clamped again
#   647  hand at (64.8, 103.68)             27 frames down+right -> inside
#                                           port 1's type tab (x 61..97,
#                                           y 96..107)
#   650  A, 654 A                           portType[1] N/A -> HMN -> CPU
#   683  hand at (98.4, 192.0)              onto port 1's knob, which sits at
#                                           (97.156, 189) with a +/-6 box
#   685  A                                  grab the knob
#   720  hand dragged to the rail's left end (x0 = 72) -> level 1
#   722  A                                  release the knob
#   790  hand at (0, 0)                     clamped
#   798  hand at (0, 30.72)                 8 frames down -> inside the roster
#                                           band (26..62) but ABOVE the cells
#                                           (32..62), so nothing is hovered
#                                           and no selection can change
#   800  B                                  grab port 0's token (css.js:209)
CSSPARK=$BUILD/csspark.flow
cat > "$CSSPARK" <<'CSSEOF'
FLOW1
I 1 -
I 375 S
I 376 -
I 380 A
I 381 -
I 400 UL
I 500 R
I 550 D
I 553 -
I 555 A
I 556 -
I 560 UL
I 620 RD
I 647 -
I 650 A
I 651 -
I 654 A
I 655 -
I 660 RD
I 674 D
I 683 -
I 685 A
I 686 -
I 690 L
I 720 -
I 722 A
I 723 -
I 730 U
I 790 D
I 798 -
I 800 B
I 801 -
END 20000
CSSEOF
made "$CSSPARK"

# park <flow> <dir> <label> — run to the wall clock, SIGUSR1, require a clean
# hibernate, leave the published record at <dir>/mlfk-persist.dat.
css_park() { # <flow> <dir> <label>
  local flow="$1" dir="$2" label="$3" pid n
  rm -rf "$dir"; mkdir -p "$dir"
  rm -f "$BUILD/$label.err" "$BUILD/$label.trace"
  MLFK_PERSIST_DIR="$dir" "$BUILD/foh_dev_headless" \
    --flow "$flow" --input flow --flow-out "$BUILD/$label.trace" \
    --pace 1 --budget-ns "$BUDGET" > "$BUILD/$label.err" 2>&1 &
  pid=$!
  sleep 3
  kill -0 "$pid" 2>/dev/null \
    || { relay_lines < "$BUILD/$label.err"
         fail "[5c] $label: app exited before the signal"; }
  kill -USR1 "$pid"
  n=0
  while kill -0 "$pid" 2>/dev/null && [ "$n" -lt 50 ]; do sleep 0.1; n=$((n + 1)); done
  kill -9 "$pid" 2>/dev/null || true
  set +e; wait "$pid" >/dev/null 2>&1; set -e
  grep -qxF 'foh_dev: hibernate from=css resume=css' "$BUILD/$label.err" \
    || { relay_lines < "$BUILD/$label.err"
         fail "[5c] $label: did not hibernate ON THE CSS — the flow no longer
  parks where this leg thinks it does, so everything below judges the wrong
  screen"; }
  made "$dir/mlfk-persist.dat"
}

# the WORKED screen, and the UNTOUCHED one from the same binary and the same
# navigation — the fresh-state reference every row below is compared against.
CSSDIR=$BUILD/css-persist
css_park "$CSSPARK" "$CSSDIR" cssw
CSSFRESH=$BUILD/css-fresh
css_park "$PARK" "$CSSFRESH" cssf

# THE GUARD, PER ROW. Each of ticket #25's eight rows must have been MOVED by
# the flow above. A row that matches the untouched screen was never set, and
# the byte-identity assertion below would then pass on it for free.
css_row() { # <key> <file>
  local r
  r="$(grep -m1 "^$1 " "$2")" || return 1
  printf '%s\n' "$r"
}
for k in ptype cpudiff vsmode hand slider carry cpucarry handtype; do
  a="$(css_row "$k" "$CSSDIR/mlfk-persist.dat")" \
    || grammar_die "[5c] the parked record carries no '$k' row"
  b="$(css_row "$k" "$CSSFRESH/mlfk-persist.dat")" \
    || grammar_die "[5c] the untouched record carries no '$k' row"
  case "$k" in
    cpucarry)
      # THE ONE ROW THIS FLOW CANNOT MOVE, and the reason is a fact about the
      # screen rather than a gap in the flow: a held knob PINS the hand to the
      # rail (css.js:317 forces its y every frame), so a run that ends holding
      # a knob cannot also end holding a token, and the token is what the
      # ticket names. It is parked at its own default here and covered as a
      # value by check-persist-table.sh's seeded fixture instead. Asserted as
      # EQUAL rather than skipped, so that a flow which starts moving it
      # arrives here as a failure to re-read, not as a silent change.
      [ "$a" = "$b" ] \
        || fail "[5c] the parked '$k' row now differs from the untouched one
  ('$a' vs '$b') — the flow reaches a held knob after all, so move this row
  into the guard above instead of exempting it";;
    *)
      [ "$a" != "$b" ] \
        || fail "[5c] the parked '$k' row is IDENTICAL to the untouched one
  ('$a') — the flow did not actually change it, so the round trip below would
  pass on a field that was never set (dead leg)";;
  esac
done
echo "   parked CSS differs from an untouched one on every row the flow moves"

# ...now boot again on the same dir, let the resume restore the screen, and
# hibernate a second time. Byte-identical or a row was dropped on the way.
CSSRESFLOW=$BUILD/cssres.flow
printf 'FLOW1\nI 1 -\nEND 20000\n' > "$CSSRESFLOW"
cp "$CSSDIR/mlfk-persist.dat" "$BUILD/css.before"
rm -f "$BUILD/cssres.err" "$BUILD/cssres.trace"
MLFK_PERSIST_DIR="$CSSDIR" "$BUILD/foh_dev_headless" \
  --flow "$CSSRESFLOW" --input flow --flow-out "$BUILD/cssres.trace" \
  --pace 1 --budget-ns "$BUDGET" > "$BUILD/cssres.err" 2>&1 &
CSSPID2=$!
sleep 2
grep -qxF 'foh_dev: resumed screen=css' "$BUILD/cssres.err" \
  || { kill -9 "$CSSPID2" 2>/dev/null || true
       relay_lines < "$BUILD/cssres.err"
       fail "[5c] the next boot did not resume the CSS"; }
kill -USR1 "$CSSPID2"
n=0
while kill -0 "$CSSPID2" 2>/dev/null && [ "$n" -lt 50 ]; do sleep 0.1; n=$((n + 1)); done
kill -9 "$CSSPID2" 2>/dev/null || true
set +e; wait "$CSSPID2" >/dev/null 2>&1; set -e
made "$CSSDIR/mlfk-persist.dat"
cmp "$BUILD/css.before" "$CSSDIR/mlfk-persist.dat" \
  || { fail "[5c] the CSS did NOT survive the round trip: the record published
  by the second hibernate differs from the first. Every difference is a field
  the resume dropped, re-derived or overwrote — which is the owner's original
  report, restated in bytes."; }
echo "   CSS resumed with the SAME machine plane, byte for byte"

# --- [5d] TARGET SELECT resumes, and its slot list is RE-READ (ticket #26) --
#
# Two halves, and the second is the one that needed a new mechanism.
#
#   HALF 1 — the FIELDS. `tsscur`, `tsspage` and `tsshand` are three more
#   rows through ticket #22's table, so the page the player was on and the
#   slot he had selected come back. The HAND is one of them because this
#   round trip is what proved it had to be: FOH_TSS_HOME is the centre of
#   slot 0 and D29 makes the slot under the hand write the selection, so a
#   resume that restored the cursor and left the hand at home lost the
#   cursor on its FIRST TICK and failed the cmp below at the `tsscur` line.
#   Judged exactly as [5b] and [5c] judge theirs: BYTE IDENTITY across
#   park -> hibernate -> resume -> hibernate, with a per-row dead-tooth guard
#   proving the parked record really differs from the one the same binary
#   writes on an untouched screen.
#
#   HALF 2 — the LIST, which a field table CANNOT carry (ADR 0001). The ten
#   custom slots live on the SD card and the card can be changed while the
#   machine is off; their refusal strings are `const char *` into other
#   translation units. So they are RE-DERIVED by the resume hook, and the
#   only way to prove a re-derivation is to make the saved answer WRONG:
#   this leg edits the slot files between the hibernate and the resume and
#   requires the resumed screen to describe the DISK. A resume that restored
#   a remembered list would satisfy every other assertion here and fail this
#   one, which is the whole reason it is written this way round.
echo "=== [5d] target select resumes and RE-READS its slots (ticket #26)"

# CLAMP-THEN-COUNT, the [5c] idiom. FOH_CURSOR_VX/VY are 2.40/3.84 px/frame
# and the target-select rects are foh_tss_slots(): slot k at
# x = 8 + (k/5)*124, y = 30 + (k%5)*22, 100x19; the page-flip slot 10 at
# (70,142) 100x17. Target select is entered at tick ~391 (S at 375, one D to
# the TARGET TEST row, A at 390) and everything below finishes by 607, well
# inside the 3 s (1500 tick) wait.
#
#   400-459  UL            hand clamped to (0, 0)
#   460-498  RD  39 frames (93.6, 149.76)
#   499-509  R   11 frames (120.0, 149.76) — inside slot 10's box
#   512      A             flip to the CUSTOM page (tssPage 0 -> 1)
#   520-579  UL            clamped to (0, 0) again
#   580-599  RD  20 frames (48.0, 76.8)
#   600-606  D    7 frames (48.0, 103.68) — inside slot 3 (y 96..115)
#
# The A at 512 is the ONLY A on this screen: an A over a real custom slot
# LAUNCHES it, and a match is a different screen with a different record.
TSSPARK=$BUILD/tsspark.flow
cat > "$TSSPARK" <<'TSSEOF'
FLOW1
I 1 -
I 375 S
I 376 -
I 380 D
I 381 -
I 390 A
I 391 -
I 400 UL
I 460 RD
I 499 R
I 510 -
I 512 A
I 513 -
I 520 UL
I 580 RD
I 600 D
I 607 -
END 20000
TSSEOF
made "$TSSPARK"

# A REAL .mlstage, produced by the REAL writer. [5b] published this document
# through foh_persist_publish under A45 T2's three-line contract, and
# foh_tbuild.c states that a slot and the resume document are the SAME
# artifact addressed by a different name — so copying it into a slot is not a
# hand-authored fixture, it is that claim being used.
made "$BUILD/tbdoc.before"

TSSDIR=$BUILD/tss-persist
rm -rf "$TSSDIR"; mkdir -p "$TSSDIR"
# The card AS THE LID CLOSES: slots 3 and 7 carry a stage, everything else is
# empty.
cp "$BUILD/tbdoc.before" "$TSSDIR/custom3.mlstage"
cp "$BUILD/tbdoc.before" "$TSSDIR/custom7.mlstage"
rm -f "$BUILD/tsshib.err" "$BUILD/tsshib.trace"
MLFK_PERSIST_DIR="$TSSDIR" "$BUILD/foh_dev_headless" \
  --flow "$TSSPARK" --input flow --flow-out "$BUILD/tsshib.trace" \
  --pace 1 --budget-ns "$BUDGET" > "$BUILD/tsshib.err" 2>&1 &
TSSPID=$!
sleep 3
kill -0 "$TSSPID" 2>/dev/null \
  || { relay_lines < "$BUILD/tsshib.err"
       fail "[5d] the app exited before the signal"; }
kill -USR1 "$TSSPID"
n=0
while kill -0 "$TSSPID" 2>/dev/null && [ "$n" -lt 50 ]; do sleep 0.1; n=$((n + 1)); done
kill -9 "$TSSPID" 2>/dev/null || true
set +e; wait "$TSSPID" >/dev/null 2>&1; set -e
grep -qxF 'foh_dev: hibernate from=target-select resume=target-select' \
  "$BUILD/tsshib.err" \
  || { relay_lines < "$BUILD/tsshib.err"
       fail "[5d] the flow did not hibernate ON TARGET SELECT — it no longer
  parks where this leg thinks it does, so everything below judges the wrong
  screen"; }
made "$TSSDIR/mlfk-persist.dat"

# THE PARKED RECORD SAYS WHERE HE WAS. Exact rows, not "differs": the flow
# above computes both values, so an off-by-one in the hand arithmetic must
# fail here rather than silently park somewhere else and round-trip fine.
for want in 'tsscur 03' 'tsspage 1'; do
  grep -qxF "$want" "$TSSDIR/mlfk-persist.dat" \
    || { grep -nE '^tss' "$TSSDIR/mlfk-persist.dat" >&2 || true
         fail "[5d] the parked record does not carry '$want' — the flow did
  not leave target select on the custom page with slot 3 selected"; }
done
# ...and both rows must DIFFER from the ones the same binary writes on a
# screen nobody worked, or the round trip below is a round trip on defaults.
# [5c]'s untouched CSS record is exactly that reference: it never visits
# target select, so its two rows are the cold-start pair.
for k in tsscur tsspage tsshand; do
  a="$(grep -m1 "^$k " "$TSSDIR/mlfk-persist.dat")" \
    || grammar_die "[5d] the parked record carries no '$k' row"
  b="$(grep -m1 "^$k " "$CSSFRESH/mlfk-persist.dat")" \
    || grammar_die "[5d] the untouched record carries no '$k' row"
  [ "$a" != "$b" ] \
    || fail "[5d] the parked '$k' row is IDENTICAL to the untouched one
  ('$a') — the flow did not actually change it (dead leg)"
done
echo "   parked on the CUSTOM page with slot 3 selected, all three rows moved"

# THE CARD CHANGES WHILE THE MACHINE IS OFF. Slot 3 — the SELECTED one — is
# taken away, slot 5 appears, and slot 7 is left alone so that "the list was
# re-read" cannot be satisfied by everything simply going absent.
cp "$BUILD/tbdoc.before" "$TSSDIR/custom5.mlstage"
rm -f "$TSSDIR/custom3.mlstage"
DISK_MASK=0000010100   # slots 5 and 7, BY INDEX (D43 — nothing shifts up)
PARK_MASK=0001000100   # what the lid closed on; a RESTORED list would say this
[ "$DISK_MASK" != "$PARK_MASK" ] \
  || grammar_die "[5d] the two masks are equal — the disk edit is a no-op and
  the re-read assertion below would pass for a resume that restored the list"

cp "$TSSDIR/mlfk-persist.dat" "$BUILD/tss.before"
TSSRESFLOW=$BUILD/tssres.flow
printf 'FLOW1\nI 1 -\nEND 20000\n' > "$TSSRESFLOW"
rm -f "$BUILD/tssres.err" "$BUILD/tssres.trace"
MLFK_PERSIST_DIR="$TSSDIR" "$BUILD/foh_dev_headless" \
  --flow "$TSSRESFLOW" --input flow --flow-out "$BUILD/tssres.trace" \
  --pace 1 --budget-ns "$BUDGET" > "$BUILD/tssres.err" 2>&1 &
TSSPID2=$!
sleep 2
tss_die() { kill -9 "$TSSPID2" 2>/dev/null || true
            relay_lines < "$BUILD/tssres.err"; fail "$1"; }
grep -qxF 'foh_dev: resumed screen=target-select' "$BUILD/tssres.err" \
  || tss_die "[5d] the next boot did not resume target select"
# THE HOOK RAN, AND WHAT IT FOUND IS THE DISK. This is the assertion the
# ticket exists for: a resume that restored the remembered list would print
# $PARK_MASK here and would still pass every other line in this leg.
grep -qxF "foh_dev: resume hook tss-slots re-read $DISK_MASK" \
  "$BUILD/tssres.err" \
  || { grep -F 'resume hook' "$BUILD/tssres.err" >&2 || true
       tss_die "[5d] the resumed slot list is not the one on the card (want
  $DISK_MASK). If it says $PARK_MASK the list was RESTORED rather than
  re-read, which is the defect ADR 0001 keeps the hook for; if it says
  0000000000 nothing was readable and the fixture is wrong."; }
# the stage that APPEARED and the one that was left alone, named individually
# so a mask that is right by coincidence cannot pass
! grep -qxF 'foh_dev: resume hook tss-slot 5 absent EMPTY' "$BUILD/tssres.err" \
  || tss_die "[5d] the stage added to the card while the lid was shut did not
  appear after the resume"
! grep -qxF 'foh_dev: resume hook tss-slot 7 absent EMPTY' "$BUILD/tssres.err" \
  || tss_die "[5d] the untouched slot 7 went missing — the re-read is not
  reading, it is guessing"
# ...and the one that was REMOVED is gone AND SAYS WHY, in words the screen
# can draw. "Gone with no reason" is the outcome the reason plane prevents.
grep -qxF 'foh_dev: resume hook tss-slot 3 absent EMPTY' "$BUILD/tssres.err" \
  || tss_die "[5d] the stage removed from the card is still reported present,
  or is absent with no reason under it"
kill -USR1 "$TSSPID2"
n=0
while kill -0 "$TSSPID2" 2>/dev/null && [ "$n" -lt 50 ]; do sleep 0.1; n=$((n + 1)); done
kill -9 "$TSSPID2" 2>/dev/null || true
set +e; wait "$TSSPID2" >/dev/null 2>&1; set -e
made "$TSSDIR/mlfk-persist.dat"
cmp "$BUILD/tss.before" "$TSSDIR/mlfk-persist.dat" \
  || fail "[5d] target select did NOT survive the round trip: the record
  published by the second hibernate differs from the first. The slot list is
  not in that record, so every difference is a field the resume dropped."
echo "   target select resumed on the same page and slot, byte for byte;
   the slot list came from the CARD, not from the record"

# --- [5e/5f/5g] THE REST OF THE FOH RESUMES INTO ITSELF (ticket #27) --------
#
# "Resume means the same thing on every screen, so the player does not have to
# learn which ones are exceptions." Tickets #25 and #26 brought back the two
# screens whose state was rich; these three legs are the rest — the four menu
# and options cursors, stage select, and the credits — and each is shaped
# exactly like [5c]:
#
#   * BYTE IDENTITY ACROSS THE ROUND TRIP, not a log line. A resume that
#     silently reset a screen would still print `resumed screen=...` and would
#     still hibernate again cleanly; what it cannot do is publish the same
#     bytes twice.
#   * AND A PER-ROW GUARD PROVING THE PARKED STATE DIFFERED FROM A FRESH ONE.
#     A cursor left at its default survives trivially and proves only that
#     zero is stable, so each flow below really works its screen and every new
#     row is then required to differ from the row the SAME BINARY writes on a
#     screen nobody touched ([5c]'s untouched CSS record, reused here).
#
# THE HELPERS. css_park() above greps for the CSS by name; these three park on
# three different screens, so the same body is taken once with the screen as
# an argument rather than copied three times.
park_screen() { # <flow> <dir> <label> <screen-token> <leg>
  local flow="$1" dir="$2" label="$3" want="$4" leg="$5" pid n
  rm -rf "$dir"; mkdir -p "$dir"
  rm -f "$BUILD/$label.err" "$BUILD/$label.trace"
  MLFK_PERSIST_DIR="$dir" "$BUILD/foh_dev_headless" \
    --flow "$flow" --input flow --flow-out "$BUILD/$label.trace" \
    --pace 1 --budget-ns "$BUDGET" > "$BUILD/$label.err" 2>&1 &
  pid=$!
  sleep 3
  kill -0 "$pid" 2>/dev/null \
    || { relay_lines < "$BUILD/$label.err"
         fail "$leg $label: the app exited before the signal"; }
  kill -USR1 "$pid"
  n=0
  while kill -0 "$pid" 2>/dev/null && [ "$n" -lt 50 ]; do sleep 0.1; n=$((n + 1)); done
  kill -9 "$pid" 2>/dev/null || true
  set +e; wait "$pid" >/dev/null 2>&1; set -e
  grep -qxF "foh_dev: hibernate from=$want resume=$want" "$BUILD/$label.err" \
    || { relay_lines < "$BUILD/$label.err"
         fail "$leg $label: did not hibernate ON '$want' with '$want' armed.
  Either the flow no longer parks where this leg thinks it does — so every
  assertion below judges the wrong screen — or the resume map redirects that
  screen somewhere else, which is the behaviour ticket #27 removes."; }
  made "$dir/mlfk-persist.dat"
}
# Each new row must have MOVED. `$UNTOUCHED` is [5c]'s fresh CSS record: the
# same binary, the same navigation up to the menu, nothing worked.
UNTOUCHED=$CSSFRESH/mlfk-persist.dat
made "$UNTOUCHED"
rows_moved() { # <leg> <record> <key...>
  local leg="$1" rec="$2" k a b; shift 2
  for k in "$@"; do
    a="$(grep -m1 "^$k " "$rec")" \
      || grammar_die "$leg: the parked record carries no '$k' row"
    b="$(grep -m1 "^$k " "$UNTOUCHED")" \
      || grammar_die "$leg: the untouched record carries no '$k' row"
    [ "$a" != "$b" ] \
      || fail "$leg: the parked '$k' row is IDENTICAL to the untouched one
  ('$a') — the flow did not actually move that cursor, so the round trip
  below would pass on a field that was never set (dead leg)"
  done
}
# Boot again on the same dir, let the resume restore the screen, hibernate a
# second time, and require the two published records to be byte-identical.
resume_roundtrip() { # <dir> <label> <screen-token> <leg>
  local dir="$1" label="$2" want="$3" leg="$4" pid n
  local flow=$BUILD/$label-res.flow
  printf 'FLOW1\nI 1 -\nEND 20000\n' > "$flow"
  cp "$dir/mlfk-persist.dat" "$BUILD/$label.before"
  rm -f "$BUILD/$label-res.err" "$BUILD/$label-res.trace"
  MLFK_PERSIST_DIR="$dir" "$BUILD/foh_dev_headless" \
    --flow "$flow" --input flow --flow-out "$BUILD/$label-res.trace" \
    --pace 1 --budget-ns "$BUDGET" > "$BUILD/$label-res.err" 2>&1 &
  pid=$!
  sleep 2
  grep -qxF "foh_dev: resumed screen=$want" "$BUILD/$label-res.err" \
    || { kill -9 "$pid" 2>/dev/null || true
         relay_lines < "$BUILD/$label-res.err"
         fail "$leg: the next boot did not resume '$want'"; }
  kill -USR1 "$pid"
  n=0
  while kill -0 "$pid" 2>/dev/null && [ "$n" -lt 50 ]; do sleep 0.1; n=$((n + 1)); done
  kill -9 "$pid" 2>/dev/null || true
  set +e; wait "$pid" >/dev/null 2>&1; set -e
  made "$dir/mlfk-persist.dat"
  cmp "$BUILD/$label.before" "$dir/mlfk-persist.dat" \
    || fail "$leg: '$want' did NOT survive the round trip: the record
  published by the second hibernate differs from the first. Every difference
  is a field the resume dropped, re-derived or overwrote."
}

# --- [5e] THE MENU AND OPTIONS ROWS ----------------------------------------
#
# ONE walk that moves all five row cursors and ends on the controls screen:
#
#   375 S                    title -> menu top
#   380/385/390 D            menuSelected 0 -> 3 (OPTIONS)
#   395 A                    -> menu-options (the arm resets menuSelected to 0)
#   400 D, 405 A             row 1 (GAMEPLAY) -> options-gameplay
#   410..425 D x4            optRow 0 -> 4, the tap-jump row (the only row
#                            with columns, gameplaymenu.js:12)
#   430/435 R                optCol 0 -> 2. R and L move the CURSOR here; A is
#                            the only thing that changes a VALUE (:37-59), and
#                            this flow never presses it on this screen.
#   440 B                    -> menu-options, menuSelected := 1
#   445 U, 450 A             row 0 (AUDIO) -> options-audio
#   455 D                    audioRow 0 -> 1 (MUSIC)
#   460 B                    -> menu-options, menuSelected := 0
#   465/470 D                menuSelected -> 2 (CONTROLS)
#   475 A                    -> controls-keyboard (FOH_CTL_CHOOSER is 0, so
#                            row 2 opens the handheld screen directly and
#                            menuSelected is deliberately left at 2)
#   480..495 D x4            ctlRow 0 -> 4. No L/R: those REBIND, and no A:
#                            that is the RESET row's activator.
echo "=== [5e] the menu and options rows come back (ticket #27)"
MENUPARK=$BUILD/menupark.flow
cat > "$MENUPARK" <<'MENUEOF'
FLOW1
I 1 -
I 375 S
I 376 -
I 380 D
I 381 -
I 385 D
I 386 -
I 390 D
I 391 -
I 395 A
I 396 -
I 400 D
I 401 -
I 405 A
I 406 -
I 410 D
I 411 -
I 415 D
I 416 -
I 420 D
I 421 -
I 425 D
I 426 -
I 430 R
I 431 -
I 435 R
I 436 -
I 440 B
I 441 -
I 445 U
I 446 -
I 450 A
I 451 -
I 455 D
I 456 -
I 460 B
I 461 -
I 465 D
I 466 -
I 470 D
I 471 -
I 475 A
I 476 -
I 480 D
I 481 -
I 485 D
I 486 -
I 490 D
I 491 -
I 495 D
I 496 -
END 20000
MENUEOF
made "$MENUPARK"
MENUDIR=$BUILD/menu-persist
park_screen "$MENUPARK" "$MENUDIR" menuw controls-keyboard "[5e]"
rows_moved "[5e]" "$MENUDIR/mlfk-persist.dat" menusel optrow optcol audiorow ctlrow
# ...and the VALUES, named, so a flow that moved a cursor to somewhere other
# than the row this leg describes is a failure rather than a silent pass on a
# different screen state.
for want in 'menusel 2' 'optrow 4' 'optcol 2' 'audiorow 1' 'ctlrow 04'; do
  grep -qxF "$want" "$MENUDIR/mlfk-persist.dat" \
    || { grep -E '^(menusel|optrow|optcol|audiorow|ctlrow) ' \
           "$MENUDIR/mlfk-persist.dat" >&2 || true
         fail "[5e] the parked record does not carry '$want' — the flow no
  longer walks the rows this leg describes"; }
done
echo "   parked with all five row cursors moved off their defaults"
resume_roundtrip "$MENUDIR" menu controls-keyboard "[5e]"
echo "   the menu, options, audio and controls cursors all came back, byte
   for byte"

# --- [5f] STAGE SELECT RESUMES INTO ITSELF (ticket #27) --------------------
#
# The redirect this removes read "stage select launches with port types the
# CSS arms, and those are not persisted". Ticket #25 put them on the card, so
# the reason was spent; ticket #26 corrected the comment and left the
# behaviour for a ticket that would judge it. This is that judgement.
#
# Getting here needs a LEGAL port config (foh.c's START arm: port 0 HMN and at
# least two participants), so the flow arms port 1 first — [5c]'s measured
# navigation, reused: clamp the hand to (0,0), 27 frames down+right lands at
# (64.8, 103.68), inside port 1's type tab (x 61..97, y 96..107), and two A
# edges walk N/A -> HMN -> CPU.
#
#   400-500  UL              100 frames: the hand clamps to (0, 0)
#   500-527  RD   27 frames  (64.8, 103.68), inside port 1's type tab
#   530 A, 534 A             portType[1] N/A -> HMN -> CPU (two participants)
#   600 S                    START -> stage select
#   610/615 R, 620 D         sssCursor 0 -> 1 -> 2 -> 5 (the 3x2 grid's
#                            second row; foh.c step_sss). NO A: A on this
#                            screen LAUNCHES, and a match is a different
#                            screen with a different record.
echo "=== [5f] stage select resumes into itself (ticket #27)"
SSSPARK=$BUILD/ssspark.flow
cat > "$SSSPARK" <<'SSSEOF'
FLOW1
I 1 -
I 375 S
I 376 -
I 380 A
I 381 -
I 400 UL
I 500 RD
I 527 -
I 530 A
I 531 -
I 534 A
I 535 -
I 600 S
I 601 -
I 610 R
I 611 -
I 615 R
I 616 -
I 620 D
I 621 -
END 20000
SSSEOF
made "$SSSPARK"
SSSDIR=$BUILD/sss-persist
park_screen "$SSSPARK" "$SSSDIR" sssw sss "[5f]"
rows_moved "[5f]" "$SSSDIR/mlfk-persist.dat" ssscur
grep -qxF 'ssscur 5' "$SSSDIR/mlfk-persist.dat" \
  || { grep -E '^ssscur ' "$SSSDIR/mlfk-persist.dat" >&2 || true
       fail "[5f] the parked record does not carry 'ssscur 5' — the flow no
  longer walks the grid this leg describes"; }
# THE ROW WHOSE SPENT REASON THIS LEG IS ABOUT. `resume 07` is FOH_SSS; the
# redirect published `resume 06` (the CSS). Named, because a leg that only
# checked the cursor would pass with the redirect still in place.
grep -qxF 'resume 07' "$SSSDIR/mlfk-persist.dat" \
  || { grep -E '^resume ' "$SSSDIR/mlfk-persist.dat" >&2 || true
       fail "[5f] the record armed something other than stage select. If it
  says 'resume 06' the redirect to the CSS is still there, which is the
  behaviour ticket #27 removes."; }
echo "   parked on stage select with the cursor on slot 5, and the record
   arms STAGE SELECT rather than the CSS"
resume_roundtrip "$SSSDIR" sss sss "[5f]"
echo "   stage select resumed on the same slot with the same port config,
   byte for byte"

# --- [5g] THE CREDITS RESUME INTO THEMSELVES (ticket #27) ------------------
#
# The redirect this removes read "its reticle is placed by the entering
# transition, which a resume never runs". ADR 0001 and ticket #26 both
# expected the credits to grow the second RESUME HOOK. They do not get one,
# and the reason is measured rather than argued: foh_init already places the
# reticle at the identical home (foh.h's FOH_CRED_HOME_X/Y — one macro since
# this ticket), so a hook would be a provable no-op whose tooth could not
# bite. THE RETICLE HALF IS NOT ASSERTED HERE: port/foh/check-credits.sh
# compares a RESUMED credits screen against an ENTERED one pixel for pixel,
# and its T4 moves foh_init's placement in a copy of foh.c to prove it. What
# this leg owns is the screen coming back at all, and its record surviving.
#
#   375 S; 380/385/390 D; 395 A     -> menu-options (menuSelected := 0)
#   400/405/410 D                   menuSelected -> 3 (CREDITS)
#   415 A                           -> credits
#   No further input: A on this screen FIRES, B leaves it, and the screen's
#   own 2500-frame timer is far beyond the ~600 ticks before the signal.
echo "=== [5g] the credits resume into themselves (ticket #27)"
CREDPARK=$BUILD/credpark.flow
cat > "$CREDPARK" <<'CREDEOF'
FLOW1
I 1 -
I 375 S
I 376 -
I 380 D
I 381 -
I 385 D
I 386 -
I 390 D
I 391 -
I 395 A
I 396 -
I 400 D
I 401 -
I 405 D
I 406 -
I 410 D
I 411 -
I 415 A
I 416 -
END 20000
CREDEOF
made "$CREDPARK"
CREDDIR=$BUILD/cred-persist
park_screen "$CREDPARK" "$CREDDIR" credw credits "[5g]"
rows_moved "[5g]" "$CREDDIR/mlfk-persist.dat" menusel
# `resume 12` is FOH_CREDITS; the redirect published `resume 03`
# (menu-options, the screen its own B-exit goes to).
grep -qxF 'resume 12' "$CREDDIR/mlfk-persist.dat" \
  || { grep -E '^resume ' "$CREDDIR/mlfk-persist.dat" >&2 || true
       fail "[5g] the record armed something other than the credits. If it
  says 'resume 03' the redirect to menu-options is still there, which is the
  behaviour ticket #27 removes."; }
grep -qxF 'menusel 3' "$CREDDIR/mlfk-persist.dat" \
  || fail "[5g] the parked record does not carry 'menusel 3' — the flow no
  longer leaves the Options cursor on the CREDITS row, so the round trip
  would judge a menu state nobody set"
echo "   parked on the credits, and the record arms the CREDITS rather than
   menu-options"
resume_roundtrip "$CREDDIR" cred credits "[5g]"
echo "   the credits resumed into themselves, byte for byte"

# --- [6] teeth ---------------------------------------------------------------
echo "=== [6] teeth"

# T1 — DOMAIN. A structurally perfect file (checksum RECOMPUTED, so the seal
# passes) whose resume row names FOH_MATCH: a real screen that is not a resume
# target. It must be refused, and refused with a REASON, never half-applied.
T1DIR=$BUILD/t1-persist
rm -rf "$T1DIR"; mkdir -p "$T1DIR"
node -e '
  const fs = require("fs"), crypto = require("crypto");
  const src = process.argv[1], dst = process.argv[2];
  let b = fs.readFileSync(src, "utf8").split("\n");
  const sumIdx = b.findIndex((l) => l.startsWith("SUM "));
  const rIdx = b.findIndex((l) => l.startsWith("resume "));
  if (sumIdx < 0 || rIdx < 0) throw new Error("no SUM/resume row to perturb");
  b[rIdx] = "resume 13";  // FOH_MATCH
  const body = b.slice(0, sumIdx).join("\n") + "\n";
  b[sumIdx] = "SUM " + crypto.createHash("sha256").update(body).digest("hex");
  fs.writeFileSync(dst, b.join("\n"));
' "$PDIR/mlfk-persist.dat" "$T1DIR/mlfk-persist.dat"
made "$T1DIR/mlfk-persist.dat"
grep -qxF 'resume 13' "$T1DIR/mlfk-persist.dat" || fail "T1 was not applied"
run_foh "$IDLE" "$T1DIR" "$BUILD/t1.err" "$BUILD/t1.trace"
grep -qxF 'foh_persist: reset cause=corrupt detail=domain' "$BUILD/t1.err" \
  || { cat "$BUILD/t1.err" >&2
       fail "T1 DID NOT BITE: a resume row naming FOH_MATCH was not refused as
  a domain violation. A screen the driver would not restore must never load."; }
! grep -q 'foh_dev: resumed' "$BUILD/t1.err" \
  || fail "T1 DID NOT BITE: the refused record was applied anyway"
echo "   T1 (domain: resume 13 = FOH_MATCH) bites"

# T2 — ABSENCE. The SAME session as a v6 file: no resume row at all. Must boot
# cold and loudly, and — the half that actually matters — must keep the
# player's SETTINGS. Losing your place is acceptable; losing your settings is not.
T2DIR=$BUILD/t2-persist
rm -rf "$T2DIR"; mkdir -p "$T2DIR"
node -e '
  const fs = require("fs"), crypto = require("crypto");
  const src = process.argv[1], dst = process.argv[2];
  let b = fs.readFileSync(src, "utf8").split("\n");
  const sumIdx = b.findIndex((l) => l.startsWith("SUM "));
  const rIdx = b.findIndex((l) => l.startsWith("resume "));
  if (sumIdx < 0 || rIdx < 0) throw new Error("no SUM/resume row to remove");
  b[0] = "MLFKPERSIST6";           // the version that had no resume row
  // ...nor any of ticket #25s CSS rows, which are v7 rows too. A v6 file
  // carrying one is corrupt rather than migratable, so leaving them here
  // would make this tooth refuse for the wrong reason and stop testing the
  // migration it names.
  const v7 = ["resume ", "ptype ", "cpudiff ", "vsmode ", "hand ",
              "slider ", "carry ", "cpucarry ", "handtype ",
              "tsscur ", "tsspage ", "tsshand ",
              "menusel ", "ssscur ", "optrow ", "optcol ", "audiorow ",
              "ctlrow "];
  for (const k of v7) {
    const i = b.findIndex((l) => l.startsWith(k));
    if (i < 0) throw new Error("no " + k + "row to remove");
    b.splice(i, 1);
  }
  const s2 = b.findIndex((l) => l.startsWith("SUM "));
  const body = b.slice(0, s2).join("\n") + "\n";
  b[s2] = "SUM " + crypto.createHash("sha256").update(body).digest("hex");
  fs.writeFileSync(dst, b.join("\n"));
' "$PDIR/mlfk-persist.dat" "$T2DIR/mlfk-persist.dat"
made "$T2DIR/mlfk-persist.dat"
run_foh "$IDLE" "$T2DIR" "$BUILD/t2.err" "$BUILD/t2.trace"
grep -qxF 'foh_persist: migrated from=6' "$BUILD/t2.err" \
  || { cat "$BUILD/t2.err" >&2
       fail "T2 DID NOT BITE: a v6 file was not migrated"; }
grep -qxF 'foh_persist: loaded' "$BUILD/t2.err" \
  || fail "T2 DID NOT BITE: the migrated v6 file was not LOADED — its settings
  and all 50 target records were discarded, which is the outcome the migration
  rule exists to prevent"
! grep -q 'foh_dev: resumed' "$BUILD/t2.err" \
  || fail "T2 DID NOT BITE: a file with no resume row still resumed somewhere"
grep -qE '^T [0-9]+ startup title timer$' "$BUILD/t2.trace" \
  || fail "T2 DID NOT BITE: no resume record, yet the boot did not start cold"
echo "   T2 (absence: v6 migrates, settings kept, boots cold) bites"

# T3 — DRIVER. foh_dev.c's hibernate arm perturbed so it never stamps the
# screen. Built as a COPY (the check-device-foh.sh T12 pattern); the tree is
# never edited.
echo "   T3: perturbed foh_dev.c copy (the stamp removed)"
mkdir -p "$BUILD/t3/foh"
sed 's|^  g_persist.resumeScreen = (int)foh_persist_resume_target(sc);$|  (void)sc; // T3|' \
  "$FOH/foh_dev.c" > "$BUILD/t3/foh/foh_dev.c"
made "$BUILD/t3/foh/foh_dev.c"
cmp -s "$BUILD/t3/foh/foh_dev.c" "$FOH/foh_dev.c" \
  && grammar_die "T3: the perturbation matched nothing — the hibernate stamp
  line moved, so this tooth would be vacuous"
# -I$FOH: the copy lives outside port/foh, so its own "foh.h" quoted includes
# resolve relative to the COPY's directory, not the original's.
build_foh_headless "$BUILD/t3/foh/foh_dev.c" "$BUILD/t3/foh_dev_t3" "-I$FOH" \
  || fail "T3 build failed"
T3DIR=$BUILD/t3-persist
rm -rf "$T3DIR"; mkdir -p "$T3DIR"
MLFK_PERSIST_DIR="$T3DIR" "$BUILD/t3/foh_dev_t3" \
  --flow "$PARK" --input flow --flow-out "$BUILD/t3.trace" \
  --pace 1 --budget-ns "$BUDGET" > "$BUILD/t3.err" 2>&1 &
tp=$!; sleep 3; kill -USR1 $tp
n=0; while kill -0 $tp 2>/dev/null && [ $n -lt 50 ]; do sleep 0.1; n=$((n + 1)); done
set +e; wait $tp >/dev/null 2>&1; set -e
grep -qxF 'foh_dev: hibernate from=css resume=startup' "$BUILD/t3.err" \
  || { cat "$BUILD/t3.err" >&2
       fail "T3 build did not reach the perturbed arm"; }
grep -qxE '^resume 00$' "$T3DIR/mlfk-persist.dat" \
  || fail "T3 DID NOT BITE: the perturbed build still stamped a screen"
echo "   T3 (driver stamp removed -> nothing armed) bites"

# T4 — LAUNCHER. The extracted idiom reverted to the FOREGROUND form. The trap
# must then be deferred past any usable grace.
echo "   T4: the launcher idiom reverted to the foreground form"
T4=$BUILD/idiom-t4.sh
sed -e 's|^    >> "\$LOG" 2>&1 &$|    >> "$LOG" 2>\&1|' \
    -e 's|^  APP=\$!$|  APP=$$|' \
    -e 's|^  wait "\$APP"; rc=\$?$|  rc=$?|' \
    -e 's|^  if \[ "\$rc" -gt 128 \]; then wait "\$APP"; rc=\$?; fi$|  :|' \
    "$IDIOM" > "$T4"
chmod +x "$T4"
cmp -s "$T4" "$IDIOM" \
  && grammar_die "T4: the revert matched nothing — the idiom's shape changed,
  so this tooth would be vacuous"
# 2>/dev/null on the CALL: the T4 launcher dies OF the signal (see below) and
# bash reports that as a job-status line — "User defined signal 1" — which reads
# like an error in a check that is passing. idiom_run itself never writes to
# stderr, so nothing real is hidden; every T4 verdict is asserted below.
idiom_run t4 "$T4" 2>/dev/null
if grep -qxF 'GOT_USR1' "$BUILD/t4.log" 2>/dev/null; then
  fail "T4 DID NOT BITE: the FOREGROUND form still delivered SIGUSR1 to the
  child in time. The measured behaviour is that POSIX sh defers the trap until
  the foreground child completes (4 s late in the original measurement), which
  is why the shipped form backgrounds and waits."
fi
# WHY it bites, so the job-status noise above is not mystifying: with the child
# in the FOREGROUND the shell blocks on it before it ever reaches the `trap`
# line, so it has no USR1 handler installed at all and takes the default
# disposition — it dies of the signal and the app is never told. That is a
# sharper form of the same defect than the deferral measurement, and the same
# fix answers both.
echo "   T4 (foreground form: the signal never reaches the app) bites"

# T5 — THE HOOK. The re-derivation itself removed from a foh_dev.c COPY, so
# the resume restores its fields and never asks the card. This is the
# perturbation that makes [5d]'s re-read assertion a measurement rather than
# a description: without it, a resume that quietly kept whatever the boot's
# memset left behind would satisfy every log line [5d] greps for except one.
# The tree is never edited (T3's pattern).
echo "   T5: the resume hook stops re-reading the card"
mkdir -p "$BUILD/t5/foh"
sed 's|^        foh_tss_refresh_slots(&foh);$|        // T5: the hook no longer re-reads the card|' \
  "$FOH/foh_dev.c" > "$BUILD/t5/foh/foh_dev.c"
made "$BUILD/t5/foh/foh_dev.c"
cmp -s "$BUILD/t5/foh/foh_dev.c" "$FOH/foh_dev.c" \
  && grammar_die "T5: the perturbation matched nothing — the hook's call moved
  or was reverted, so this tooth would be vacuous"
build_foh_headless "$BUILD/t5/foh/foh_dev.c" "$BUILD/t5/foh_dev_t5" "-I$FOH" \
  || fail "T5 build failed"
# the SAME card [5d] resumed against: slots 5 and 7 present, and a record
# armed for target select.
T5DIR=$BUILD/t5-persist
rm -rf "$T5DIR"; mkdir -p "$T5DIR"
cp "$TSSDIR/mlfk-persist.dat" "$T5DIR/mlfk-persist.dat"
cp "$TSSDIR/custom5.mlstage" "$T5DIR/custom5.mlstage"
cp "$TSSDIR/custom7.mlstage" "$T5DIR/custom7.mlstage"
rm -f "$BUILD/t5.err"
MLFK_PERSIST_DIR="$T5DIR" "$BUILD/t5/foh_dev_t5" \
  --flow "$TSSRESFLOW" --input flow --flow-out "$BUILD/t5.trace" \
  --pace 1 --budget-ns "$BUDGET" > "$BUILD/t5.err" 2>&1 &
t5p=$!
sleep 2
kill -9 "$t5p" 2>/dev/null || true
set +e; wait "$t5p" >/dev/null 2>&1; set -e
grep -qxF 'foh_dev: resumed screen=target-select' "$BUILD/t5.err" \
  || { relay_lines < "$BUILD/t5.err"
       fail "T5 build did not reach the resume at all — the tooth is measuring
  a broken build, not the hook"; }
! grep -qxF "foh_dev: resume hook tss-slots re-read $DISK_MASK" "$BUILD/t5.err" \
  || fail "T5 DID NOT BITE: with the re-derivation removed the resumed screen
  still described the card, so [5d]'s assertion is not measuring the hook"
echo "   T5 (hook stops re-reading -> the card is not described) bites"

# T6 — THE OLD REDIRECTS, PUT BACK BY HAND IN THE RECORD (ticket #27). [5f]
# and [5g] both assert on a `resumed screen=` line, and a line like that is
# only worth what the alternative would have looked like. So each parked
# record is rewritten (checksum RECOMPUTED, so it loads cleanly) to name the
# screen its OLD redirect published — the CSS for stage select, menu-options
# for the credits — and the boot must then land THERE. If it landed on the
# original screen anyway, those legs would be reading a constant.
#
# It also fixes the two arms in place from the other direction: the values are
# `resume 06` and `resume 03`, which are exactly what the removed arms of
# foh_persist_resume_plan() used to return.
t6_redirect() { # <label> <parked-record> <old NN> <old token> <new token>
  local label="$1" rec="$2" nn="$3" oldtok="$4" newtok="$5"
  local dir=$BUILD/t6-$label
  rm -rf "$dir"; mkdir -p "$dir"
  node -e '
    const fs = require("fs"), crypto = require("crypto");
    const [src, dst, nn] = process.argv.slice(1);
    let b = fs.readFileSync(src, "utf8").split("\n");
    const sumIdx = b.findIndex((l) => l.startsWith("SUM "));
    const rIdx = b.findIndex((l) => l.startsWith("resume "));
    if (sumIdx < 0 || rIdx < 0) throw new Error("no SUM/resume row to perturb");
    if (b[rIdx] === "resume " + nn) throw new Error("T6: no-op perturbation");
    b[rIdx] = "resume " + nn;
    const body = b.slice(0, sumIdx).join("\n") + "\n";
    b[sumIdx] = "SUM " + crypto.createHash("sha256").update(body).digest("hex");
    fs.writeFileSync(dst, b.join("\n"));
  ' "$rec" "$dir/mlfk-persist.dat" "$nn" \
    || fail "T6 $label: could not derive the old-redirect record"
  made "$dir/mlfk-persist.dat"
  run_foh "$IDLE" "$dir" "$BUILD/t6-$label.err" "$BUILD/t6-$label.trace"
  grep -qxF "foh_dev: resumed screen=$oldtok" "$BUILD/t6-$label.err" \
    || { relay_lines < "$BUILD/t6-$label.err"
         fail "T6 $label DID NOT BITE: a record naming '$oldtok' did not resume
  there, so the '$newtok' assertion in the leg above is not reading the row"; }
  ! grep -qxF "foh_dev: resumed screen=$newtok" "$BUILD/t6-$label.err" \
    || fail "T6 $label DID NOT BITE: the boot reached '$newtok' anyway"
  echo "   T6 ($label: the old redirect target resumes THERE, not on $newtok) bites"
}
t6_redirect sss "$SSSDIR/mlfk-persist.dat" 06 css sss
t6_redirect credits "$CREDDIR/mlfk-persist.dat" 03 menu-options credits

# T7 — THE CURSORS GO THROUGH FohState, not around it. [5e]'s byte identity
# is satisfied by a resume that applies the record and collects it back; it
# would ALSO be satisfied by a driver that never applied anything and simply
# republished the record it read. This tooth tells the two apart: it hands the
# boot a record carrying a DIFFERENT legal controls row and requires the
# second hibernate to publish that one. Only a value that travelled
# apply -> FohState -> the live screen -> collect can come back.
echo "   T7: a cursor value that must travel through the live screen"
T7DIR=$BUILD/t7-persist
rm -rf "$T7DIR"; mkdir -p "$T7DIR"
node -e '
  const fs = require("fs"), crypto = require("crypto");
  const [src, dst] = process.argv.slice(1);
  let b = fs.readFileSync(src, "utf8").split("\n");
  const sumIdx = b.findIndex((l) => l.startsWith("SUM "));
  const cIdx = b.findIndex((l) => l.startsWith("ctlrow "));
  if (sumIdx < 0 || cIdx < 0) throw new Error("no SUM/ctlrow row to perturb");
  if (b[cIdx] === "ctlrow 07") throw new Error("T7: no-op perturbation");
  b[cIdx] = "ctlrow 07";  // still inside the eleven rows
  const body = b.slice(0, sumIdx).join("\n") + "\n";
  b[sumIdx] = "SUM " + crypto.createHash("sha256").update(body).digest("hex");
  fs.writeFileSync(dst, b.join("\n"));
' "$MENUDIR/mlfk-persist.dat" "$T7DIR/mlfk-persist.dat" \
  || fail "T7: could not derive the perturbed record"
made "$T7DIR/mlfk-persist.dat"
cp "$T7DIR/mlfk-persist.dat" "$BUILD/t7.before"
resume_roundtrip "$T7DIR" t7 controls-keyboard "T7"
grep -qxF 'ctlrow 07' "$T7DIR/mlfk-persist.dat" \
  || { grep -E '^ctlrow ' "$T7DIR/mlfk-persist.dat" >&2 || true
       fail "T7 DID NOT BITE: the republished record does not carry the row
  the boot was handed. The resume is not applying the cursor to the live
  screen, so [5e]'s byte identity was proving only that a file survives being
  read and written."; }
cmp "$BUILD/t7.before" "$T7DIR/mlfk-persist.dat" \
  || fail "T7: the perturbed record did not round-trip byte-identically"
echo "   T7 (a different controls row round-trips through the live screen) bites"

# --- [7] the DEVICE check's format whitelist, run against this real file ----
# check-device-persist.sh carries an EXACT POSITIONAL whitelist for the persist
# format, restated independently of the C loader. A version bump has to move it
# too, and it is a DEVICE check, so its bump would otherwise ship unverified
# until someone next has hardware — which is exactly how it acquired the
# defect this bump found: A49 moved the SUM's line index from 68 to 69 and left
# the recompute at 67, so from that commit the whitelist hashed two lines short
# and would have rejected every genuine file (T-H3's fixture builder had the
# same miss). So the whitelist is EXTRACTED and run here, on the host, against
# the v7 file leg [4] just published and against a real v6 file that must NOT
# pass it. This does not replace the device leg; it stops the bump being blind.
echo "=== [7] check-device-persist.sh's positional whitelist, on a real file"
DEVP=$FOH/check-device-persist.sh
made "$DEVP"
WL=$BUILD/whitelist.sh
for anchor in 'hex_lt() ( LC_ALL=C; [[ "$1" < "$2" ]]; ) # fixed 16-hex: byte order == numeric order' \
              'verify_persist_file() { # <file> <ctx>'; do
  n="$(grep -cxF "$anchor" "$DEVP")" || true
  [ "$n" = 1 ] || grammar_die "[7] check-device-persist.sh has $n copies of
  the anchor '$anchor' (want 1) — the extracted whitelist moved"
done
{
  grep -xF 'PERSIST_BYTES=1927' "$DEVP" \
    || grammar_die "[7] check-device-persist.sh does not pin PERSIST_BYTES=1927"
  awk '
    /^hex_lt\(\) \( LC_ALL=C; \[\[ "\$1" < "\$2" \]\]; \)/ { inr = 1 }
    inr { print }
    inr && /^}$/ { exit }
  ' "$DEVP"
} > "$WL"
made "$WL"
grep -qF 'sed -n 69p' "$WL" \
  || grammar_die "[7] the extracted whitelist does not read line 69 — it was
  not bumped for the v7 resume row"
grep -qF 'sed -n 77p' "$WL" \
  || grammar_die "[7] the extracted whitelist does not read line 77 — it was
  not bumped for ticket #25's CSS machine plane, so a persistence change is
  about to reach hardware with the device check judging the wrong lines"
grep -qF 'sed -n 80p' "$WL" \
  || grammar_die "[7] the extracted whitelist does not read line 80 — it was
  not bumped for ticket #26's target-select trio, so the same blindness one
  ticket later: the device would judge a file two rows shorter than the one
  this build writes"
grep -qF 'sed -n 86p' "$WL" \
  || grammar_die "[7] the extracted whitelist does not read line 86 — it was
  not bumped for ticket #27's six screen cursors. Three tickets running, the
  SAME omission: the device would judge a file six rows shorter than the one
  this build writes, and only hardware would notice"
# shellcheck disable=SC1090 — derived above, not tracked
. "$WL"
verify_persist_file "$PDIR/mlfk-persist.dat" "[7] the v7 file leg [4] published"
echo "   the device whitelist accepts the real v7 file"
# ...and must REJECT a genuine v6 one: a whitelist that passes both is not
# pinning a version at all (it is the T2 fixture, valid in its own grammar).
set +e
( verify_persist_file "$T2DIR/mlfk-persist.dat" "[7] v6" ) >/dev/null 2>&1
wlrc=$?
set -e
[ "$wlrc" != 0 ] \
  || fail "[7] the device whitelist accepted a v6 file as well as a v7 one —
  it is not pinning the current format"
echo "   ...and rejects a genuine v6 file"

# --- no-commit guard ---------------------------------------------------------
git_dirty_after="$(tree_fingerprint)" || fail "post-run fingerprint failed"
[ "$git_dirty_before" = "$git_dirty_after" ] \
  || fail "this check modified tracked files — it must not"

echo "HIBERNATE OK"
