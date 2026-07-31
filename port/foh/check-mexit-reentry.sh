#!/usr/bin/env bash
# port/foh/check-mexit-reentry.sh — HOST-ONLY witness for the multi-phase FOH
# re-entry that punch-list A19 introduced (review-mexit-r3 High "H1w").
#
# WHAT IT PROVES, and why it had to be written
# --------------------------------------------
# A19 made "leave the match" return to the FOH menus IN-PROCESS: foh_dev.c's
# `foh_phase:` label runs the menu phase again. That created a run shape no
# check had ever produced — TWO FOH phases in ONE process — and review-mexit-r3
# found that the second phase re-ran the FOH artifact flush. The FOHTRACE1
# header is emitted once at boot, BEFORE the label, while the re-entry resets
# the trace buffer, so a second flush rewrote --flow-out as a HEADERLESS single
# line (`END <t> transitions=0`). That is not a trace: judge-foh-trace.js and
# normalize-foh-trace.js both reject it, and check-device-target.sh's live leg
# [6b] — the punch-list A2 regression guard — lost the `TLAUNCH ... char=2
# tstage=0` line it exists to assert. The fix is the `matchesRun == 0` guard on
# the flush; this script is its tooth.
#
# It asserts, all on the host:
#   [A] a SINGLE-phase live target run's --flow-out (the reference);
#   [B] a TWO-phase run — same navigation, plus a START quit that fires
#       upstream's endGame (main.js:1013-1015) -> MEX_TSS -> the re-entry —
#       produces a --flow-out BYTE-IDENTICAL to [A], while its teardown
#       summary proves a second FOH phase really ran (launched=0 after a
#       match was played);
#   [T1] a PERTURBED COPY of foh_dev.c whose ONLY delta is the guard removed
#       DIVERGES from the reference (cmp rc exactly 1) and produces exactly the
#       headerless single-line shape the defect produced. Both directions
#       proven, so the tooth cannot rot into a tautology.
#   [D] the two-phase run is byte-reproducible across two fresh runs;
#   [6] the LAUNCH KIND witness (review-mexit-r3 Medium "M6"): both launch
#       kinds driven through the real foh_tick across an A19 match exit, plus
#       [T2] a COPY of foh.c with the r2 class fix (`targetMode = false` at
#       foh.c:730) removed, which must FAIL that witness.
#
# HOST-ONLY, and that is a design choice, not a shortcut: the device rig
# (check-device-target.sh) can only GREP its live traces, because fk_input
# sleeps against the wall clock and so the tick a press lands on is
# timing-dependent. The host input source used here (MLFK_HEADLESS_KEYS,
# port/gfx/platform_headless.c) advances one QUANTUM of virtual time per
# platform_poll() call — a quantum is one poll, not one application frame — so
# injected input is a pure function of the poll INDEX and traces become
# byte-comparable. The script the injector consumes is
# produced by the SAME frozen generator the device rigs use
# (port/foh/flow-to-fkscript.js) over the SAME committed flow
# (port/foh/flows/f06-target-t01.flow), so the navigation under test is the
# committed one and not something authored here.
#
# NOT A GATE. This is a task-level tooth: `bash port/foh/check-mexit-reentry.sh`
# -> `MEXIT REENTRY OK`, exit 0.
set -euo pipefail

FOH=port/foh
GFX=port/gfx
CAL=port/sim/calib
TABLES=pipeline/build/mexit-tables
BUILD=$FOH/build/mexit-reentry
FLOW=$FOH/flows/f06-target-t01.flow
DEVFOH=$FOH/check-device-foh.sh

fail() { echo "MEXIT REENTRY FAIL: $1" >&2; exit 1; }
grammar_die() { echo "MEXIT REENTRY FAIL: $1" >&2; exit 2; }
relay_lines() { sed 's/^/  | /'; }

made() { # every artifact must exist and be non-empty
  local f
  for f in "$@"; do
    [ -s "$f" ] || fail "expected artifact missing/empty: $f"
  done
}

# one_canonical <file> <needle> <anchored-regex> <ctx> — the PROCESS §3
# permissive-parse guard (review-mexit-r5 Medium). `grep -q` + `head -1` was
# not enough: it accepts a file carrying a canonical line PLUS any number of
# stale, duplicated or MALFORMED lines that merely resemble it, and then
# happily parses whichever came first. Here the count of lines RESEMBLING the
# summary (the fixed needle) must equal the count of lines MATCHING it exactly,
# and both must be 1. Anything else is corruption, not a pass.
one_canonical() { # <file> <needle> <regex> <ctx>
  local f="$1" needle="$2" re="$3" ctx="$4" nres nexact
  nres="$(grep -cF "$needle" "$f")" || true
  nexact="$(grep -cE "$re" "$f")" || true
  if [ "$nres" != 1 ] || [ "$nexact" != 1 ]; then
    sed 's/^/  | /' "$f" >&2
    grammar_die "$ctx: $nres line(s) resemble '$needle' and $nexact match it
  exactly (want 1 and 1). A resemblance that is not an exact match is
  corruption; two exact matches mean the artifact carries more than one run."
  fi
}

# --- [0] run lock (mkdir-atomic, NO reclaim — the check-foh-flows pattern) ----
LOCK=$FOH/build/mexit-reentry.lock
mkdir -p "$FOH/build"
mkdir "$LOCK" 2>/dev/null || fail "another run holds $LOCK (remove it only if
  you have proven no other run is live)"
trap 'rmdir "$LOCK" 2>/dev/null || true' EXIT

# --- [0a] no-commit guard ----------------------------------------------------
# This tooth writes only under pipeline/build and port/foh/build, both ignored.
# Assert it rather than trust it: a tooth that dirties the tree would make the
# driver commit build output.
#
# CONTENT, not status (review-mexit-r5 Medium). `git status --porcelain` alone
# is blind in exactly this tree: nearly every file this lane touches is ALREADY
# `M` or already untracked, so rewriting one of them leaves the status output
# byte-identical and the guard passes. The fingerprint therefore folds in the
# tracked DIFF BYTES and a hash of every untracked-but-not-ignored file's
# CONTENT, so any edit to any of them changes it.
#
# EVERY untracked non-ignored file's content is hashed, with ONE named
# exclusion (review-mexit-r6b Medium corrected an earlier four-directory scope
# that silently omitted the rest of the tree while the comment claimed
# otherwise). The exclusion is `.tokensave/**`, agent-tooling scratch that is
# untracked-but-not-ignored and mutates on its own — `tokensave.db` and its
# `-wal`/`-shm` are a live SQLite database, so hashing it made the guard change
# its own answer between two calls and report a tree change it had not caused.
# Naming the exclusion keeps that honest; anything else that starts flaking
# should be ignored in .gitignore, not quietly dropped here.
#
# FAIL CLOSED. Every component must succeed and the result must be a real
# digest: the earlier shape swallowed `git ls-files`/hashing failures with
# `|| true` and still produced a plausible-looking hash, so a broken guard
# would have looked like a passing one.
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
  printf '%s\n%s\n%s\n' "$status" "$diff" "$hashes" | shasum -a 256 \
    | cut -d' ' -f1
}
git_dirty_before="$(tree_fingerprint)" \
  || fail "could not fingerprint the working tree (a component command failed —
  the guard fails CLOSED rather than reporting a hash of partial input)"
# a sha256 hex digest and nothing else, so a silently-empty pipeline cannot
# masquerade as an answer
[[ "$git_dirty_before" =~ ^[0-9a-f]{64}$ ]] \
  || fail "tree fingerprint '$git_dirty_before' is not a sha256 digest"

# --- [1] data planes ---------------------------------------------------------
echo "=== [1] data planes (tables/stages/targets/assets + simdata)"
rm -rf "$TABLES"
{ bash pipeline/extractor/build-extractor.sh; } 2>&1 | relay_lines
{ node pipeline/run.js --only animations,tables,stages,targets,assets \
    --out "$TABLES"; } 2>&1 | relay_lines
made "$TABLES/ml_tables.c" "$TABLES/ml_stages.c" "$TABLES/ml_targets.c" \
     "$TABLES/assets/menu.img1" "$TABLES/anim_2_fox.bin"
# foh_render's artwork resolution is fatal on a MISSING file by design, so
# point it at THIS run's freshly generated artifact (check-foh-flows.sh's rule).
export MLFK_MENU_IMG1="$PWD/$TABLES/assets/menu.img1"
rm -rf "$BUILD"
mkdir -p "$BUILD"
node "$CAL/dump-sim-data.js" --out "$BUILD/simdata.txt" 2>&1 | relay_lines
made "$BUILD/simdata.txt"

GFXDATA=$GFX/gfxdata-frozen.txt
VFXDATA=$GFX/vfxdata-frozen.txt
GLYPHS=$GFX/vfxglyphs-frozen.txt
made "$GFXDATA" "$VFXDATA" "$GLYPHS" "$FLOW"

# --- [2] build the host twin, USING check-device-foh.sh's OWN recipe ---------
# The TU list is NOT copied here. It is EXTRACTED from check-device-foh.sh and
# sourced, so there is exactly one definition of "how foh_dev.c is built for
# the host" in the tree and this tooth cannot drift away from the rig that
# ships it. The extraction is anchored and single-match-asserted: if that
# region is edited or moved, this dies loudly instead of silently building
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
' "$DEVFOH" > "$RECIPE"
made "$RECIPE"
grep -qxF 'build_foh_headless() { # <foh_dev_src> <out> [extra cc args...]' \
  "$RECIPE" || grammar_die "extracted recipe does not carry build_foh_headless"
grep -qxF 'SIM_TUS=(' "$RECIPE" \
  || grammar_die "extracted recipe does not carry the SIM_TUS list"
grep -qxF '}' "$RECIPE" || grammar_die "extracted recipe has no closing brace"
# The extracted region references exactly $FOH, $GFX, $TABLES and $BUILD
# (measured), all defined above; any name it grows dies here under `set -u`
# instead of expanding to nothing. Sourcing it compiles raster.o and defines
# build_foh_headless(); the CALL is ours, because the rig's call line sits
# after the extracted region.
# shellcheck disable=SC1090 — the sourced file is derived above, not tracked
. "$RECIPE"
build_foh_headless "$FOH/foh_dev.c" "$BUILD/foh_dev_headless" \
  || fail "host twin build failed"
made "$BUILD/foh_dev_headless"
echo "   host twin built via the rig's own build_foh_headless()"

# --- [3] the fk_input script (frozen generator, committed flow) --------------
echo "=== [3] key scripts (flow-to-fkscript.js, byte-stable x2)"
for v in "" ".b"; do
  rm -f "$BUILD/nav$v.fks"
  node "$FOH/flow-to-fkscript.js" "$FLOW" "$BUILD/nav$v.fks" >/dev/null \
    || fail "flow-to-fkscript.js failed"
done
cmp "$BUILD/nav.fks" "$BUILD/nav.b.fks" \
  || fail "flow-to-fkscript.js output is not byte-stable across two runs"
made "$BUILD/nav.fks"
# leg A: navigation + real gameplay, and NOTHING else. The match then ends at
# its FRAME BOUND, which raises no MatchExit, so the process runs exactly ONE
# FOH phase. The gameplay tail is check-device-target.sh's live-leg sequence
# verbatim (jabs only — deliberately no movement, so fox cannot self-destruct
# and switch which exit arm runs) and it also carries the START press past the
# sim's `starting` window, which ignores inputs for the first ~91 frames.
{
  cat "$BUILD/nav.fks"
  printf 's 1500\nd a\ns 120\nu a\ns 900\nd a\ns 120\nu a\ns 2600\n'
} > "$BUILD/legA.fks"
# leg B: byte-identical to leg A plus a START press. In target mode START is
# upstream's own endGame quit (main.js:1013-1015) -> MEX_TSS -> the re-entry.
# The four appended lines are, again, the device live leg's own.
{
  cat "$BUILD/legA.fks"
  printf 'd s\ns 100\nu s\ns 2000\n'
} > "$BUILD/legB.fks"
# the ONLY delta between the two legs is that START press
cmp -s <(head -n "$(wc -l < "$BUILD/legA.fks")" "$BUILD/legB.fks") \
       "$BUILD/legA.fks" \
  || fail "leg B is not leg A plus a suffix — the two legs differ in their
  navigation, so a trace difference would not isolate the START quit"
made "$BUILD/legA.fks" "$BUILD/legB.fks"
cmp -s "$BUILD/legA.fks" "$BUILD/legB.fks" \
  && fail "leg A and leg B key scripts are identical (dead tooth)"
# leg C: leg B, then — after the re-entry has landed — ONE shoulder press and
# ONE A press. Its job is to pin WHICH MatchExit was raised (review-mexit-r5
# Medium): leg B's discriminators prove only "an early match exit followed by
# some second FOH phase", which MEX_CSS or MEX_TITLE would also satisfy.
#
# The shoulder press is what makes the evidence PHASE-BOUND (review-mexit-r6b
# Medium — the first version of this leg was vacuous, because leg A's own
# phase-1 target match produced a byte-identical `char=2` bstate and satisfied
# every [4c] predicate). `n` is the R shoulder, which on target select is
# upstream's char+1 WRAP (targetselect.js:68-74). The committed f06 navigation
# presses it TWICE, landing on char=2 (fox); a THIRD press can therefore only
# happen after the re-entry, and it lands on char=3 (falco). --bstate-out is
# LAST-MATCH scope, so `char=3` in it is a fact only a phase-2 launch can
# produce — leg A's script has no third `n` and can never write it.
{
  cat "$BUILD/legB.fks"
  printf 's 1500\nd n\ns 120\nu n\ns 400\nd a\ns 120\nu a\ns 2000\n'
} > "$BUILD/legC.fks"
made "$BUILD/legC.fks"

# --- [4] the two legs --------------------------------------------------------
FOH_MAX=800   # > the last injected press's poll index (~545) with margin;
              # ALSO the second phase's bound, so the tooth terminates
FRAMES=600    # 10 s match bound; leg B's START lands at ~frame 315, i.e. past
              # the sim's input-ignoring `starting` window and well under it
SEED=1337

run_leg() { # <binary> <legId> <keyscript>
  local bin="$1" id="$2" keys="$3" rc=0
  rm -rf "$BUILD/$id"
  mkdir -p "$BUILD/$id" "$BUILD/$id-persist"
  MLFK_PERSIST_DIR="$PWD/$BUILD/$id-persist" \
  MLFK_HEADLESS_KEYS="$PWD/$keys" \
  "$bin" --flow "$FLOW" --input poll --flow-out "$BUILD/$id/trace.txt" \
    --foh-max "$FOH_MAX" --pace 1 --budget-ns 16666667 \
    --bridge live --simdata "$BUILD/simdata.txt" --seed "$SEED" \
    --bstate-out "$BUILD/$id/bstate.txt" \
    --gfxdata "$GFXDATA" --vfxdata "$VFXDATA" --glyphs "$GLYPHS" \
    --anim-dir "$TABLES" --legible --tapjump-off-p1 \
    --frames "$FRAMES" --record-trace "$BUILD/$id/rec.json" \
    --record-keys "$BUILD/$id/keys.txt" \
    > "$BUILD/$id/out.txt" 2> "$BUILD/$id/err.txt" || rc=$?
  echo "$rc" > "$BUILD/$id/rc.txt"
}

echo "=== [4] leg A (single phase) + leg B (two phases)"
run_leg "$BUILD/foh_dev_headless" legA "$BUILD/legA.fks"
[ "$(cat "$BUILD/legA/rc.txt")" = 0 ] \
  || { sed 's/^/  A| /' "$BUILD/legA/err.txt" >&2
       fail "leg A exited rc $(cat "$BUILD/legA/rc.txt") (want 0)"; }
run_leg "$BUILD/foh_dev_headless" legB "$BUILD/legB.fks"
[ "$(cat "$BUILD/legB/rc.txt")" = 0 ] \
  || { sed 's/^/  B| /' "$BUILD/legB/err.txt" >&2
       fail "leg B exited rc $(cat "$BUILD/legB/rc.txt") (want 0)"; }
made "$BUILD/legA/trace.txt" "$BUILD/legB/trace.txt"

# (i) leg A really launched a TARGET match through the polled input path — the
#     same line check-device-target.sh's leg [6b] asserts.
#     STRICT like every other decision-bearing parse here (review-mexit-r7
#     Low): a bare existence grep would accept a trace carrying the line plus
#     any number of malformed near-misses.
TL_RE='^TLAUNCH [0-9]+ char=2 tstage=0$'
one_canonical "$BUILD/legA/trace.txt" 'TLAUNCH' "$TL_RE" \
  "leg A target launch"
# (ii) leg A is a WELL-FORMED single-phase FOHTRACE1 document. The header is
#      `FOHTRACE1 flow=<id>` (foh_dev.c's boot emit), and it is exactly what a
#      second phase's flush destroys.
HDR_RE='^FOHTRACE1 flow=f06-target-t01$'
head -1 "$BUILD/legA/trace.txt" | grep -qE "$HDR_RE" \
  || grammar_die "leg A trace line 1 does not match $HDR_RE (got
  '$(head -1 "$BUILD/legA/trace.txt")')"
nend="$(grep -cE '^END [0-9]+ transitions=[0-9]+$' "$BUILD/legA/trace.txt")" || true
[ "$nend" = 1 ] || grammar_die "leg A trace carries $nend END lines (want 1)"
# (iii) leg A ran ONE phase: it ended by LAUNCHING, and no second phase
#       overwrote the teardown summary's launched flag.
# STRICT, like every other decision-bearing parse in this script
# (review-mexit-r6b Medium): leg A is the REFERENCE every cmp below is taken
# against, so a stale or duplicated summary in its log must be corruption, not
# a pass. Its phase-1 navigation is the committed f06 flow, so `4 transitions`
# is pinned too — that is the count leg C's phase 2 must NOT have.
one_canonical "$BUILD/legA/err.txt" 'foh_dev foh:' \
  '^foh_dev foh: [0-9]+ ticks, 4 transitions, [0-9]+ shots, [0-9]+ render skips, [0-9]+ failed presents, launched=1$' \
  "leg A single-phase FOH summary"
one_canonical "$BUILD/legA/err.txt" 'foh_dev match:' \
  '^foh_dev match: [0-9]+ frames, [0-9]+ render skips, [0-9]+ failed presents, wall [0-9]+ ms, pace=1 budget=16666667 ns$' \
  "leg A match summary"
aframes="$(sed -nE 's/^foh_dev match: ([0-9]+) frames,.*/\1/p' "$BUILD/legA/err.txt")"
[ "$aframes" = "$FRAMES" ] \
  || fail "leg A's match ran $aframes frames, not the full $FRAMES bound — it
  ended EARLY, so it is not the single-phase reference this tooth needs"

# (iv) leg B really ran a SECOND FOH phase. The teardown FOH summary reports
#      the LAST phase (the counters are reset per phase, ARTIFACT SCOPE rule
#      (2)), so `launched=0` at the FOH_MAX bound AFTER a match was played is
#      the discriminator: a single-phase run cannot produce it.
#      review-mexit-r4 L: this is a FUNCTION and not a straight-line block
#      because [4d] has to re-assert it on the RERUN. Comparing only the
#      deliberately-preserved first-phase trace would pass even if leg B2 had
#      missed its START press and never re-entered at all, i.e. the
#      reproducibility claim would be made about the wrong program.
assert_two_phase() { # <legId>  — publishes the match frame count as LAST_MFRAMES
  local lg="$1" mf e="$BUILD/$1/err.txt"
  # (1) a SECOND FOH phase ran: the teardown FOH summary is the FINAL phase's
  #     (ARTIFACT SCOPE rule (2)), so `launched=0` at the FOH_MAX bound AFTER a
  #     match was played is a shape a single-phase run cannot produce.
  one_canonical "$e" 'foh_dev foh:' \
    "^foh_dev foh: $FOH_MAX ticks, [0-9]+ transitions, [0-9]+ shots, [0-9]+ render skips, [0-9]+ failed presents, launched=0\$" \
    "$lg second FOH phase"
  # (2) a match was played and it ended EARLY, i.e. a MatchExit was raised
  one_canonical "$e" 'foh_dev match:' \
    '^foh_dev match: [0-9]+ frames, [0-9]+ render skips, [0-9]+ failed presents, wall [0-9]+ ms, pace=1 budget=16666667 ns$' \
    "$lg match summary"
  mf="$(sed -nE 's/^foh_dev match: ([0-9]+) frames,.*/\1/p' "$e")"
  [ -n "$mf" ] || grammar_die "could not read $lg's match frame count"
  [ "$mf" -lt "$FRAMES" ] \
    || fail "$lg match ran $mf frames (>= the $FRAMES bound) — START did
  not end it, so no MatchExit was raised"
  [ "$mf" -gt 1 ] || fail "$lg match ran only $mf frames"
  LAST_MFRAMES="$mf"
}
LAST_MFRAMES=
assert_two_phase legB
mframes=$LAST_MFRAMES

# (v) THE H1 ASSERTION: the two-phase run's FOH artifact is the FIRST phase's,
#     byte for byte.
cmp "$BUILD/legA/trace.txt" "$BUILD/legB/trace.txt" \
  || { echo "--- leg A ---" >&2; sed 's/^/  A| /' "$BUILD/legA/trace.txt" >&2
       echo "--- leg B ---" >&2; sed 's/^/  B| /' "$BUILD/legB/trace.txt" >&2
       fail "two-phase --flow-out != single-phase reference (H1 regression:
  the second FOH phase overwrote the artifact)"; }
echo "   [A]==[B] byte-identical ($(wc -c < "$BUILD/legB/trace.txt") B)"

# --- [4c] WHICH MatchExit? pin the destination screen -----------------------
# review-mexit-r5 Medium: leg B proves "an early match exit, then a second FOH
# phase". A regression that raised MEX_CSS or MEX_TITLE instead of MEX_TSS
# satisfies every one of those assertions — including [4d]'s same-frame
# equality — because none of them observes WHERE the re-entry landed. The
# second phase's own trace cannot answer it either: the H1 guard deliberately
# never flushes it.
#
# So make the second phase ANSWER, using artifacts that already exist and
# already have strict grammars. Three facts, and it takes ALL THREE — the first
# version of this leg asserted only the first two and was VACUOUS, because
# leg A's own phase-1 target match satisfies them byte for byte
# (review-mexit-r6b Medium):
#
#   (i)  the last match was a TARGET match. `targetMode = true` is written at
#        EXACTLY ONE site in the whole state machine — foh.c:800, inside
#        step_tss — so a target launch can only come from FOH_TSS, which is
#        endGame's gameMode-5 destination (changeGamemode(7),
#        main.js:1395-1400). NOT phase-bound on its own.
#   (ii) the last FOH phase carried exactly ONE transition and it launched.
#        Phase 1 needs FOUR (startup->title->menu-top->target-select->launch);
#        a phase that launches after exactly one transition never navigated,
#        i.e. it BEGAN on the select screen. Leg A's phase-1 summary is
#        `528 ticks, 4 transitions`, so this alone already separates them —
#        but it is an argument about counts, not a binding.
#   (iii) THE BINDING: the last match's CHARACTER is one the phase-1 script
#        cannot select. `n` (R shoulder) is char+1 WRAP on target select
#        (targetselect.js:68-74); the committed f06 navigation presses it twice
#        and stops at char=2, and leg C presses it a THIRD time AFTER the
#        re-entry. --bstate-out is LAST-MATCH scope (ARTIFACT SCOPE rule (3)),
#        so `char=3` in leg C's bstate is a fact only a launch made after the
#        re-entry can produce. Leg A's bstate is asserted to be char=2 in the
#        same breath, so the discriminator is proven LIVE rather than assumed.
echo "=== [4c] the re-entry landed on TARGET SELECT (MEX_TSS), not some other exit"
run_leg "$BUILD/foh_dev_headless" legC "$BUILD/legC.fks"
[ "$(cat "$BUILD/legC/rc.txt")" = 0 ] \
  || { sed 's/^/  C| /' "$BUILD/legC/err.txt" >&2
       fail "leg C exited rc $(cat "$BUILD/legC/rc.txt") (want 0)"; }
made "$BUILD/legC/bstate.txt" "$BUILD/legA/bstate.txt"
# (iii) first, because it is the one that binds the rest to phase 2. Both
#       regexes are FULLY anchored — no trailing `.*` (review-mexit-r6b Low):
#       the producer's grammar is exact, so accepting a suffix would accept a
#       shape it never emits.
BST_RE='^TBRIDGE-STATE char=%s tstage=0 gamemode=5 targets=10 playing=1 starting=1 stocks=1$'
# shellcheck disable=SC2059 — BST_RE is our own format string, not user input
one_canonical "$BUILD/legA/bstate.txt" 'BRIDGE-STATE' \
  "$(printf "$BST_RE" 2)" "leg A phase-1 launch character"
# shellcheck disable=SC2059
one_canonical "$BUILD/legC/bstate.txt" 'BRIDGE-STATE' \
  "$(printf "$BST_RE" 3)" "leg C PHASE-2 launch (char=3 — unreachable in phase 1)"
# (ii) the final FOH phase launched after exactly one transition, i.e. it began
#      on the select screen rather than navigating there
one_canonical "$BUILD/legC/err.txt" 'foh_dev foh:' \
  '^foh_dev foh: [0-9]+ ticks, 1 transitions, [0-9]+ shots, [0-9]+ render skips, [0-9]+ failed presents, launched=1$' \
  "leg C second FOH phase launched from the select screen"
grep -qE "^foh_dev foh: $FOH_MAX ticks, " "$BUILD/legC/err.txt" \
  && fail "[4c]: leg C's second phase ran to the $FOH_MAX bound, so it never
  launched and the bstate above is the FIRST match's"
# H1 still holds with a phase-2 LAUNCH in play — a strictly stronger case than
# leg B, because phase 2 here both launched and produced a match of its own.
cmp "$BUILD/legA/trace.txt" "$BUILD/legC/trace.txt" \
  || fail "[4c]: leg C's --flow-out != the single-phase reference — a second
  FOH phase that LAUNCHES still overwrote the artifact"
echo "   [4c] phase 2 launched a TARGET match (TBRIDGE-STATE gamemode=5) and
   the artifact is still phase 1's"

# --- [4d] determinism of the instrument -------------------------------------
# The whole design rests on the injected input being a pure function of the
# POLL INDEX (not of wall clock, and not of a frame index — see the unit note
# in platform_headless.c), so prove that rather than asserting it: a fresh
# two-phase run must reproduce the trace byte for byte AND quit on the same
# match frame.
echo "=== [4d] two-phase run is byte-reproducible"
run_leg "$BUILD/foh_dev_headless" legB2 "$BUILD/legB.fks"
[ "$(cat "$BUILD/legB2/rc.txt")" = 0 ] \
  || fail "leg B rerun exited rc $(cat "$BUILD/legB2/rc.txt")"
# the rerun must be the SAME PROGRAM: a second FOH phase after an early
# START-quit match, not merely a run that produced bytes (review-mexit-r4 L).
# The rerun must be a TWO-PHASE run before its trace means anything
# (review-mexit-r4 Low). The trace compared below is deliberately the FIRST
# phase's, so a rerun whose START press missed — no quit, no MEX_TSS, no
# re-entry — would produce the identical bytes and pass a comparison that
# proved nothing. assert_two_phase reasserts EVERY discriminator leg B had to
# satisfy; it is called rather than restated so the two runs cannot be judged
# by two copies of the same regex that drift apart.
assert_two_phase legB2
m2frames=$LAST_MFRAMES
# ...and the early exit must land on the SAME frame, which is the actual
# reproducibility claim: the injected input is a pure function of the poll
# index, so the quit frame cannot drift between runs.
[ "$m2frames" = "$mframes" ] \
  || fail "[4d]: leg B quit on frame $mframes but its rerun quit on frame
  $m2frames — the scripted input is NOT poll-indexed, so every cmp in this
  script is timing-dependent"
cmp "$BUILD/legB/trace.txt" "$BUILD/legB2/trace.txt" \
  || fail "two fresh two-phase runs produced different traces"
echo "   rerun: second phase reached, match quit on frame $m2frames (== leg B)"

# --- [5] T1: the NEGATIVE test (perturbed copy, guard removed) --------------
# The tooth-kmcopy pattern (check-device-foh.sh T12): build a COPY of foh_dev.c
# whose ONLY delta is the H1 guard removed, and prove it FAILS the comparison
# above. Without this the cmp could pass for reasons unrelated to the guard.
echo "=== [5] T1 negative test (copy with the matchesRun guard removed)"
rm -rf "$BUILD/tooth-noguard"
mkdir -p "$BUILD/tooth-noguard/foh"
node -e '
  const fs = require("fs");
  const [src, dst] = process.argv.slice(1);
  const raw = fs.readFileSync(src, "utf8");
  // The guard, exactly as committed. An indented one-line `if`, so an
  // unrelated `matchesRun == 0` elsewhere cannot be hit by accident.
  const guard = "\n  if (matchesRun == 0) {\n";
  const n = raw.split(guard).length - 1;
  if (n !== 1) {
    console.error("T1: found " + n + " copies of the H1 guard (want exactly 1)");
    process.exit(1);
  }
  // Replace the CONDITION only, keeping the block and its braces, so the
  // copy is the pre-fix program and nothing else.
  const out = raw.replace(guard, "\n  if (1 /* T1: H1 guard removed */) {\n");
  if (out === raw) { console.error("T1: substitution was a no-op"); process.exit(1); }
  fs.writeFileSync(dst, out);
' "$FOH/foh_dev.c" "$BUILD/tooth-noguard/foh/foh_dev.c" \
  || fail "T1: could not derive the unguarded foh_dev.c copy"
made "$BUILD/tooth-noguard/foh/foh_dev.c"
cmp -s "$BUILD/tooth-noguard/foh/foh_dev.c" "$FOH/foh_dev.c" \
  && fail "T1: the perturbed copy is byte-identical to the real file (dead tooth)"
build_foh_headless "$BUILD/tooth-noguard/foh/foh_dev.c" \
  "$BUILD/tooth-noguard/foh_dev_headless" -Iport/foh -Iport/gfx \
  || fail "T1: the unguarded copy did not build"
made "$BUILD/tooth-noguard/foh_dev_headless"
run_leg "$BUILD/tooth-noguard/foh_dev_headless" legT1 "$BUILD/legB.fks"
[ "$(cat "$BUILD/legT1/rc.txt")" = 0 ] \
  || fail "T1: the unguarded copy exited rc $(cat "$BUILD/legT1/rc.txt") (the
  tooth needs it to RUN and produce a wrong artifact, not to crash)"
made "$BUILD/legT1/trace.txt"
# it must run the same two phases (same discriminator as leg B)
# SAME strict helper as leg B (review-mexit-r6b Medium): T1 must be the SAME
# PROGRAM as leg B in every respect except the removed guard, so it has to
# satisfy every discriminator leg B does — a permissive `grep -q` here would
# let a T1 run that never re-entered "prove" the defect by accident.
assert_two_phase legT1
[ "$LAST_MFRAMES" = "$mframes" ] \
  || fail "T1: the unguarded copy quit on frame $LAST_MFRAMES but leg B quit on
  frame $mframes — the two runs are not the same program, so the cmp below is
  not isolating the guard"
t1rc=0
cmp -s "$BUILD/legA/trace.txt" "$BUILD/legT1/trace.txt" || t1rc=$?
[ "$t1rc" = 1 ] \
  || fail "T1: unguarded-copy trace vs the reference cmp rc $t1rc (want exactly
  1 — rc 0 means the cmp in [4] is BLIND to the H1 defect)"
# ...and it must fail in exactly the measured way: headerless, one END line.
head -1 "$BUILD/legT1/trace.txt" | grep -qE "$HDR_RE" \
  && fail "T1: the unguarded trace still carries the FOHTRACE1 header — the
  defect this tooth was written against is not the one being reproduced"
t1lines="$(wc -l < "$BUILD/legT1/trace.txt" | tr -d ' ')"
[ "$t1lines" = 1 ] \
  || fail "T1: the unguarded trace has $t1lines lines (the measured defect
  shape is exactly 1: a bare END)"
grep -qE '^END [0-9]+ transitions=0$' "$BUILD/legT1/trace.txt" \
  || fail "T1: the unguarded trace's single line is not 'END <t> transitions=0'"
echo "   T1 reproduces the defect (headerless 1-line trace; cmp rc 1)"

# --- [6] LAUNCH KIND witness + its negative test (review-mexit-r3 M6) -------
# The r2 High class fix (foh.c:730 — every launch arm states its own kind) had
# no automated witness at all. port/foh/foh_launchkind_witness.c is one: it
# drives BOTH launch kinds through the real foh_tick across a simulated A19
# match exit. Here it is built, run, and then re-run against a COPY of foh.c
# with the fix removed, which must FAIL — so the witness cannot rot into a
# tautology either.
echo "=== [6] launch-kind witness (both kinds through foh_tick) + T2"
# port/gfx/ctl_style.c is REQUIRED here, not optional: foh.c's controls screen
# calls ctl_style_get/ctl_style_set, which live in that TU and nowhere else
# (the value has no mirror in FohState, by design — MENU-SPEC §C30). Every
# recipe that links foh.c must carry it; omitting it is an undefined-symbol
# link failure, not a silent degradation.
LKW_SRCS=("$FOH/foh_launchkind_witness.c" "$FOH/foh_render.c" "$FOH/foh_font.c"
          "$GFX/raster.c" "$GFX/img1.c" "$GFX/ctl_style.c" port/fdlibm/fdlibm.c)
LKW_OK='^LAUNCH KIND OK$'
rm -f "$BUILD/lkw" "$BUILD/lkw.out"
cc -O2 "${CFLAGS_COMMON[@]}" -o "$BUILD/lkw" "$FOH/foh.c" \
  "${LKW_SRCS[@]}" -lm || fail "launch-kind witness did not build"
made "$BUILD/lkw"
"$BUILD/lkw" > "$BUILD/lkw.out" 2>&1 || { relay_lines < "$BUILD/lkw.out"
  fail "launch-kind witness failed against the REAL foh.c"; }
relay_lines < "$BUILD/lkw.out"
# anchored success grammar: the verdict line exactly once, and no other
# resemblance to it anywhere in the output (PROCESS §3 permissive-parse guard)
one_canonical "$BUILD/lkw.out" 'LAUNCH KIND OK' "$LKW_OK" \
  "launch-kind witness verdict"
# T2 — the negative test: delete the class fix, keep everything else.
rm -rf "$BUILD/tooth-nokind"
mkdir -p "$BUILD/tooth-nokind"
node -e '
  const fs = require("fs");
  const [src, dst] = process.argv.slice(1);
  const raw = fs.readFileSync(src, "utf8");
  const fix = "\n    s->targetMode = false;\n";
  const n = raw.split(fix).length - 1;
  if (n !== 1) {
    console.error("T2: found " + n + " copies of the class fix (want exactly 1)");
    process.exit(1);
  }
  const out = raw.replace(fix, "\n    /* T2: class fix removed */\n");
  if (out === raw) { console.error("T2: substitution was a no-op"); process.exit(1); }
  fs.writeFileSync(dst, out);
' "$FOH/foh.c" "$BUILD/tooth-nokind/foh.c" \
  || fail "T2: could not derive the fix-free foh.c copy"
cmp -s "$BUILD/tooth-nokind/foh.c" "$FOH/foh.c" \
  && fail "T2: the perturbed copy is byte-identical to foh.c (dead tooth)"
# -Iport/foh so the COPY's quoted "foh.h" resolves to the real tree's header
# (the copy sits outside port/foh, so quoted-include resolution alone misses);
# every other header still comes from the real tree, exactly as the T12
# copy-build tooth in check-device-foh.sh does it.
cc -O2 "${CFLAGS_COMMON[@]}" -Iport/foh -o "$BUILD/tooth-nokind/lkw" \
  "$BUILD/tooth-nokind/foh.c" "${LKW_SRCS[@]}" -lm \
  || fail "T2: the fix-free copy did not build"
t2rc=0
"$BUILD/tooth-nokind/lkw" > "$BUILD/tooth-nokind/lkw.out" 2>&1 || t2rc=$?
[ "$t2rc" = 1 ] \
  || fail "T2: the fix-free build exited rc $t2rc (want exactly 1 — rc 0 means
  the launch-kind witness is BLIND to the defect it exists to guard)"
grep -qE "$LKW_OK" "$BUILD/tooth-nokind/lkw.out" \
  && fail "T2: the fix-free build still printed the OK verdict"
grep -qE '^LAUNCH KIND FAIL: the VS launch CLEARED targetMode' \
  "$BUILD/tooth-nokind/lkw.out" \
  || { relay_lines < "$BUILD/tooth-nokind/lkw.out"
       fail "T2: the fix-free build failed, but NOT on the targetMode
  assertion — it is failing for some other reason and proves nothing"; }
echo "   T2: the fix-free foh.c copy fails the witness (rc 1, on the right line)"

# --- [7] no-commit guard ----------------------------------------------------
git_dirty_after="$(tree_fingerprint)" \
  || fail "could not fingerprint the working tree after the run"
[[ "$git_dirty_after" =~ ^[0-9a-f]{64}$ ]] \
  || fail "post-run tree fingerprint '$git_dirty_after' is not a sha256 digest"
[ "$git_dirty_before" = "$git_dirty_after" ] \
  || fail "THE WORKING TREE CHANGED ACROSS THIS RUN — status, tracked diff
  bytes or untracked file content differ from the pre-run fingerprint
  ($git_dirty_before -> $git_dirty_after). Two causes, and they need different
  responses:
    (a) this tooth wrote outside the ignored build dirs. That is a defect here
        and it would land in the driver's commit — find it with
        \`git status --porcelain\` and \`git diff --stat\`.
    (b) something ELSE edited the tree while this ran. That is the normal case
        in a multi-agent session and it is not a tooth failure: nothing above
        this line is invalidated by it (every leg was judged against artifacts
        this run produced), so re-run when the tree is quiet.
  Neither is quieted by widening the fingerprint."

echo "MEXIT REENTRY OK"
