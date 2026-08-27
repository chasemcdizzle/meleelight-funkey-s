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
  b.splice(rIdx, 1);
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
  grep -xF 'PERSIST_BYTES=1624' "$DEVP" \
    || grammar_die "[7] check-device-persist.sh does not pin PERSIST_BYTES=1624"
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
