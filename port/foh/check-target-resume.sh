#!/usr/bin/env bash
# port/foh/check-target-resume.sh — ticket #30: a TARGET RUN survives the lid.
#
# WHAT THIS CHECK IS FOR, AND WHY IT IS NOT check-match-resume.sh AGAIN
# --------------------------------------------------------------------
# Ticket #29 proved that a VS match continues across a real lid close. A VS
# match is players, physics and RNG, and all of it lives in GameState. A
# TARGET RUN is all of that PLUS a second plane: which of the stage's targets
# are broken, how many, and the pending finish edge. That plane lives in
# MlTargets, is NOT on the CHECKSUM.md §2 surface, and has its OWN frozen
# stream and its OWN verifier (port/goldens-m4/verify-target-stream.js).
#
# THAT IS THE WHOLE REASON THIS IS A SEPARATE TICKET AND A SEPARATE CHECK. A
# restore that got the sim perfect and the target plane wrong would pass every
# single assertion #29 wrote — the run would resume at the right frame, the
# player would be in the right place moving the right way, the checksum stream
# would continue — and the player's broken targets would silently come back.
# Leg [5]'s T1 is exactly that failure, produced deliberately, and it is
# INVISIBLE to the sim's verifier and caught only by the target plane's.
#
# THE SHAPE (check-match-resume.sh's, inherited whole because it is the same
# argument — CONTEXT.md's "the same state": a byte round trip catches a
# SERIALISER bug and a continuation catches a COMPLETENESS bug, state nobody
# knew had to be saved):
#
#   A  an uninterrupted run, replayed through the app, judged against BOTH
#      frozen goldens — so a continuation cannot pass by matching a broken
#      baseline;
#   B  the same run with a REAL SIGUSR1 delivered mid-run, taken by the REAL
#      handler and the REAL tdev_hibernate_check arm, which writes the
#      snapshot pair, writes the settings record and _exit(0)s;
#   C  a SECOND PROCESS booted on that same card, which resumes into the run
#      and plays the rest;
#   and then C's frames are compared to A's frame for frame, and spliced onto
#   A's prefix and judged as one stream by BOTH unchanged verifiers.
#
# THE SENDER IS THE ONLY THING SIMULATED. MLFK_HIBERNATE_AT raises SIGUSR1 at
# a known frame, because a continuation has to have a known splice point and
# an external `kill -USR1` races the wall clock. Everything downstream of the
# signal — the handler, the flag, the arm, the ordering, the exit — is the
# product's path. The EXTERNAL sender keeps its own proof in
# port/foh/check-hibernate.sh leg [4].
#
# THREE GOLDENS, and t03 is why there are three:
#   t01  fox / authored tstage 0 — the plain case, and the one whose targets
#        the player actually breaks (2 destroyed by the splice point).
#   t02  falcon / authored tstage 1 — a different character and a different
#        stage, reached by a different walk through the menus.
#   t03  fox / CUSTOM tstage 10 — a stage loaded from a `.mlstage` FILE on the
#        SD card, carrying a DAMAGING ground surface (A45 T6's fire ground).
#        It is the one that exercises the custom path and the damage plane,
#        and it is the only golden for which "where did the stage come from"
#        is a question a resume has to answer. Leg [4]'s last two refusals are
#        about it and about nothing else.
#
# THE LAUNCH IS A REAL WALK THROUGH THE MENUS. foh_dev has no --direct-target
# (its DIRECT arm is VS-only), so each run reaches its target stage the way a
# player does: title -> menu-top -> Target Test -> shoulder to the character
# -> the hand to the slot -> A. The three flow scripts are WRITTEN INTO THE
# SCRATCH BUILD DIR rather than added to port/foh/flows/, deliberately: that
# directory is a frozen corpus with frozen transition expectations, and this
# check has no business growing it. The walks are lifted from the measured
# geometry in flows/f07-target-t02.flow (FOH_CURSOR_VY = 3.84 px/frame; the
# hand homes at slot 0's centre; the addcode slot needs the RIGHT because its
# rect starts at x = 70).
#
# HOST ONLY. Prints `TARGET RESUME OK`, exit 0. Any deviation exits nonzero.
set -euo pipefail
cd "$(dirname "$0")/../.."

FOH=port/foh
GFX=port/gfx
CAL=port/sim/calib
SIM=port/sim/sim
M4G=port/goldens-m4
TABLES=pipeline/build/tres-tables
BUILD=$FOH/build/target-resume
DEVFOH=$FOH/check-device-foh.sh
VERIFY=oracle/harness/verify-stream.js
TVERIFY=$M4G/verify-target-stream.js
WRAPT=$M4G/wrap-target.js

fail() { echo "TARGET RESUME FAIL: $1" >&2; exit 1; }
# A grammar failure is a check that has gone VACUOUS, not a product failure.
# It is named differently on purpose so the two can never be confused in a log.
grammar_die() { echo "TARGET RESUME FAIL (grammar): $1" >&2; exit 2; }
relay_lines() { sed 's/^/  | /'; }
made() {
  local f
  for f in "$@"; do [ -s "$f" ] || fail "expected artifact missing/empty: $f"; done
}
sha_of() { shasum -a 256 "$1" | cut -d' ' -f1; }

LOCK=$FOH/build/target-resume.lock
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

# --- [0] pins ----------------------------------------------------------------
# The claims this check is built on, asserted textually, so that a refactor
# that removes one fails loudly instead of leaving the check quietly vacuous.
echo "=== [0] pins"

# (a) BOTH VERDICTS ARE SOMEONE ELSE'S. verify-stream.js and
# verify-target-stream.js are HARD RULE 3 territory and their bytes are
# pinned, so this check cannot go green against a weakened judge. NEITHER
# DIGEST IS RE-TYPED HERE: the sim's comes from the one place
# check-match-resume.sh takes it from (check-sim-snapshot.sh's VERIFY_SHA) and
# the target plane's from check-foh-flows.sh's PRODUCER_PINS table, which
# already carries it and already binds itself to that file. One number per
# file in the tree, and a re-pin is a reviewed edit in the file that owns it.
VERIFY_SHA_SRC=port/sim/check-sim-snapshot.sh
PRODUCER_PIN_SRC=$FOH/check-foh-flows.sh
made "$VERIFY_SHA_SRC" "$PRODUCER_PIN_SRC" "$VERIFY" "$TVERIFY" "$WRAPT"
VERIFY_SHA="$(grep -oE '^VERIFY_SHA=[0-9a-f]{64}$' "$VERIFY_SHA_SRC" \
  | head -1 | cut -d= -f2)"
[ -n "$VERIFY_SHA" ] \
  || grammar_die "[0] no VERIFY_SHA=<64 hex> line in $VERIFY_SHA_SRC — the pin
  this check borrows has moved, so borrowing it proves nothing"
[ "$(sha_of "$VERIFY")" = "$VERIFY_SHA" ] \
  || fail "[0] $VERIFY does not match the digest pinned in $VERIFY_SHA_SRC.
  The oracle's verifier is never edited to make a run pass (HARD RULE 3)."
pin_of() { # <path> — its digest from the PRODUCER_PINS table, or empty
  grep -oE "^[0-9a-f]{64} $1\$" "$PRODUCER_PIN_SRC" | head -1 | cut -d' ' -f1
}
for pinned in "$TVERIFY" "$WRAPT"; do
  want="$(pin_of "$pinned")"
  [ -n "$want" ] \
    || grammar_die "[0] $PRODUCER_PIN_SRC no longer carries a PRODUCER_PINS row
  for $pinned — the pin this check borrows has moved"
  [ "$(sha_of "$pinned")" = "$want" ] \
    || fail "[0] $pinned does not match the digest pinned in $PRODUCER_PIN_SRC.
  The target plane's verifier is never edited to make a run pass (HARD RULE 3)."
done

# (b) THE ORDERING RULE, STRUCTURALLY. Ticket #29's rule 1, inherited whole:
# the snapshot is written BEFORE the settings record. Asserted by LINE ORDER in
# the source, because it is a machine claim about a ~100 ms grace window that no
# test can watch missing.
HIB_SRC=$FOH/foh_dev.c
made "$HIB_SRC"
TARM_LN="$(grep -n 'foh_target_snap_ops->arm(&G' "$HIB_SRC" | head -1 | cut -d: -f1)"
COL_LN="$(grep -n '^  foh_persist_collect(&g_persist, f);$' "$HIB_SRC" | head -1 | cut -d: -f1)"
SAV_LN="$(grep -n '^  tdev_persist_save();$' "$HIB_SRC" | head -1 | cut -d: -f1)"
[ -n "$TARM_LN" ] && [ -n "$COL_LN" ] && [ -n "$SAV_LN" ] \
  || grammar_die "[0] could not find the target hibernate arm, the collect or
  the save in $HIB_SRC — the three lines this pin is about have moved, so the
  pin is vacuous"
[ "$TARM_LN" -lt "$COL_LN" ] && [ "$COL_LN" -lt "$SAV_LN" ] \
  || fail "[0] the target snapshot is armed at line $TARM_LN, after the settings
  collect ($COL_LN) / save ($SAV_LN). Ticket #29's rule 1, which #30 inherits:
  the snapshot goes FIRST, so that if the grace window is ever missed what is
  lost is the snapshot and never the player's settings — which, on this
  screen, carry his RECORDS."

# (c) THE STAGE IS RE-FOUND BEFORE ANYTHING IS LAUNCHED. The boot pre-flight
# (tdev_tmatch_stage_refound) must sit in the SAME condition as the peek, i.e.
# before `tmatchResume = true`. If it moved below the launch it would still
# refuse — but by then the app would already have set up a run on whatever the
# card now holds, which is the outcome the ticket forbids.
grep -q 'foh_target_snap_ops->peek(&tmatchHdr, &tsWhy) &&' "$HIB_SRC" \
  && grep -q 'tdev_tmatch_stage_refound(&tmatchHdr, &tsWhy)) {' "$HIB_SRC" \
  || grammar_die "[0] the boot arm no longer gates the target resume on
  tdev_tmatch_stage_refound in the same condition as the peek — leg [4]'s two
  card-changed refusals would then be asserting a different code path"

# (d) THE M2 EXIT GATE IS UNTOUCHED. The snapshot writer is linked into the FOH
# build; it must not have joined the gate's frozen TU list.
GATE=port/sim/check-sim.sh
made "$GATE"
! grep -q 'sim_snapshot\|foh_target_snap' "$GATE" \
  || fail "[0] $GATE now links a snapshot TU. The M2 EXIT GATE's TU list is
  frozen, and the whole point of the snapshot seam is that it exists precisely
  where the gate need not change."
echo "    [0] OK: both verifiers pinned from the files that own their digests,"
echo "    snapshot-before-settings ordering asserted, re-find asserted before"
echo "    the launch, M2 TU list untouched"

# --- [1] the data plane and the two binaries ---------------------------------
echo "=== [1] tables, assets, host twin + seam witness"
if [ ! -s "$TABLES/ml_targets.c" ] || [ ! -s "$TABLES/assets/menu.img1" ]; then
  bash pipeline/extractor/build-extractor.sh 2>&1 | relay_lines
  node pipeline/run.js --only animations,tables,stages,targets,assets \
    --out "$TABLES" 2>&1 | relay_lines
fi
made "$TABLES/ml_tables.c" "$TABLES/ml_stages.c" "$TABLES/ml_targets.c" \
  "$TABLES/assets/menu.img1"
export MLFK_DATA_DIR="$PWD/$TABLES"

rm -rf "$BUILD"; mkdir -p "$BUILD"
# THE RECIPE IS EXTRACTED, NEVER RESTATED (check-device-foh.sh's own
# arrangement, as check-hibernate.sh / check-mexit-reentry.sh /
# check-match-resume.sh all do it): a second copy of the app's build would
# drift, and a check that built a DIFFERENT binary from the product's would
# prove something the product does not do.
made "$DEVFOH"
RECIPE=$BUILD/recipe.sh
nstart="$(grep -cxF 'CFLAGS_COMMON=(-ffp-contract=off -Wall -Wextra -Werror' \
  "$DEVFOH")"
[ "$nstart" = 1 ] || grammar_die "[1] check-device-foh.sh has $nstart
  CFLAGS_COMMON anchor lines (want exactly 1) — the extracted recipe has moved"
awk '
  /^CFLAGS_COMMON=\(-ffp-contract=off -Wall -Wextra -Werror$/ { inr = 1 }
  inr { print }
  inr && /^}$/ { exit }
' "$DEVFOH" > "$RECIPE"
made "$RECIPE"
grep -qxF 'build_foh_headless() { # <foh_dev_src> <out> [extra cc args...]' \
  "$RECIPE" || grammar_die "[1] the extracted recipe does not carry
  build_foh_headless"
# The three TUs this feature needs must be IN the product's recipe: without
# them there is no feature, and the check would be building something the
# player never runs.
for tu in sim_snapshot 'foh_target_snap\.c' 'target_play\.c'; do
  grep -q "$tu" "$RECIPE" \
    || grammar_die "[1] the extracted recipe no longer links $tu — the target
  lid seam is gone from the app's own build"
done

# THE WITNESS FIRST, and the order is not arbitrary: the extracted recipe
# rebuilds raster.o and REMOVES $BUILD/foh_dev_headless at source time, so
# sourcing the witness variant afterwards would delete the binary just built.
#
# It is the SAME recipe with foh_target_snap.c REMOVED, and it proves the
# pointer seam is real — the app still links and still runs, and every arm
# takes its "no target snapshot in this build" branch. That is what makes the
# claim in foh_target_snap.h checkable instead of decorative.
RECIPE_NOTS=$BUILD/recipe-nots.sh
sed '/foh_target_snap\.c/d' "$RECIPE" > "$RECIPE_NOTS"
[ "$(wc -c < "$RECIPE")" -gt "$(wc -c < "$RECIPE_NOTS")" ] \
  || grammar_die "[1] deleting foh_target_snap.c from the recipe changed
  nothing — the line moved and the seam witness is vacuous"
# shellcheck disable=SC1090
(. "$RECIPE_NOTS"; build_foh_headless "$FOH/foh_dev.c" "$BUILD/foh_dev_nots") \
  2>&1 | relay_lines
made "$BUILD/foh_dev_nots"
# shellcheck disable=SC1090
(. "$RECIPE"; build_foh_headless "$FOH/foh_dev.c" "$BUILD/foh_dev_headless") \
  2>&1 | relay_lines
made "$BUILD/foh_dev_headless"
echo "    [1] OK: host twin + witness twin built (the witness without the seam TU)"

# --- shared run plumbing ------------------------------------------------------
node "$CAL/dump-sim-data.js" --out "$BUILD/simdata.txt" > /dev/null
made "$BUILD/simdata.txt"
IDLE=$BUILD/idle.flow
printf 'FLOW1\nI 1 -\nEND 10\n' > "$IDLE"
made "$IDLE"
GD=(--gfxdata "$GFX/gfxdata-frozen.txt" --vfxdata "$GFX/vfxdata-frozen.txt"
    --glyphs "$GFX/vfxglyphs-frozen.txt" --anim-dir "$TABLES")

# THE MENU WALKS. Written here rather than into port/foh/flows/ (see the
# header). Each is a real sequence of button edges through the real screens;
# the letters are FLOW letters, resolved to keysyms by the injector.
#
#   S  START      D  d-pad down      A  A            B  B
#   N  R shoulder (targetselect.js:68-73, wraps the character FORWARD)
#   K  L shoulder (:62-66, wraps it BACKWARD)
#   U  d-pad up   DR d-pad down+right
#
# The hand geometry is flows/f07-target-t02.flow's, measured there and not
# re-derived: it homes at slot 0's centre (58.0, 39.5) and moves at
# FOH_CURSOR_VY = 3.84 px per held frame. DOWN+RIGHT for 6 frames reaches slot
# 1 at (72.4, 62.54) — the RIGHT is not decoration, the addcode slot's rect
# starts at x = 70 while the left column starts at x = 8, so a walk at x = 58
# passes BELOW addcode and hits nothing. 22 more DOWN frames reach the addcode
# slot at (72.4, 147.02); 28 UP frames from there land back on slot 0.
write_flow() { # <path> <char 0-4> <tstage>
  local out="$1" ch="$2" ts="$3" k n
  {
    printf 'FLOW1\n'
    printf '# generated by check-target-resume.sh: char=%s tstage=%s\n' "$ch" "$ts"
    printf 'I 1 -\nI 375 S\nI 376 -\nI 380 D\nI 383 -\nI 385 A\nI 386 -\n'
    # the character: marth (0) is where the screen opens, so walk FORWARD with
    # the R shoulder, five frames apart, exactly as f06 does.
    n=390
    for ((k = 0; k < ch; k++)); do
      printf 'I %d N\nI %d -\n' "$n" "$((n + 1))"
      n=$((n + 5))
    done
    if [ "$ts" -ge 10 ]; then
      # a CUSTOM slot: walk to addcode, flip the grid to the custom page, walk
      # back up to slot 0. Only slot 0 is ever published by this check.
      [ "$ts" = 10 ] || grammar_die "write_flow only walks to custom slot 0"
      printf 'I %d DR\nI %d D\nI %d -\n' "$((n + 1))" "$((n + 7))" "$((n + 29))"
      printf 'I %d A\nI %d -\n' "$((n + 35))" "$((n + 36))"
      printf 'I %d U\nI %d -\n' "$((n + 41))" "$((n + 69))"
      printf 'I %d A\nI %d -\nEND %d\n' "$((n + 73))" "$((n + 74))" "$((n + 78))"
    elif [ "$ts" = 1 ]; then
      printf 'I %d DR\nI %d -\n' "$((n + 1))" "$((n + 7))"
      printf 'I %d A\nI %d -\nEND %d\n' "$((n + 11))" "$((n + 12))" "$((n + 16))"
    elif [ "$ts" = 0 ]; then
      printf 'I %d A\nI %d -\nEND %d\n' "$((n + 5))" "$((n + 6))" "$((n + 10))"
    else
      grammar_die "write_flow has no walk for authored tstage $ts"
    fi
  } > "$out"
  made "$out"
}

# publish_slot <dir> — writes the CURRENT golden's custom share code into slot
# 0 as a real `.mlstage` file, in A45 T2's on-disk contract (custom_stage.h:
# three LF lines MLSTAGE1 / code / `SUM <64 hex>` over every preceding byte),
# so the app exercises the REAL loader — grammar, SUM and all.
publish_slot() { # <dir>
  local dir="$1" body="$1/.body"
  [ -n "$CODE" ] || grammar_die "publish_slot called for a golden with no
  customStage — the manifest and this check disagree about $ID"
  printf 'MLSTAGE1\n%s\n' "$CODE" > "$body"
  printf 'MLSTAGE1\n%s\nSUM %s\n' "$CODE" "$(sha_of "$body")" \
    > "$dir/custom0.mlstage"
  rm -f "$body"
  made "$dir/custom0.mlstage"
}

# play <bin> <persist-dir> <tag> [env...] — the run, launched by walking the
# menus. `env` is used rather than a `VAR=x func` prefix on purpose: whether
# such a prefix survives a shell function differs between bash modes, and a
# hibernate control silently leaking into the next run would be an invisible
# false pass (the check-sim-snapshot.sh lesson, inherited).
play() { # <bin> <pdir> <tag> [VAR=value ...]
  local bin="$1" pdir="$2" tag="$3"; shift 3
  env MLFK_PERSIST_DIR="$PWD/$pdir" "$@" "$bin" \
    --flow "$BUILD/$ID.flow" --input flow --flow-out "$BUILD/$tag.trace" \
    --pace 0 --bridge tverify --simdata "$BUILD/simdata.txt" --seed "$SEED" \
    --trace "$BUILD/$ID.trace.txt" --frames "$FRAMES" \
    --out "$BUILD/$tag.stream.txt" --timing "$BUILD/$tag.timing.txt" \
    --bstate-out "$BUILD/$tag.bstate.txt" "${GD[@]}" \
    > "$BUILD/$tag.out.txt" 2> "$BUILD/$tag.err.txt"
}
# resume <bin> <persist-dir> <tag> [env...] — a BOOT with no launch arguments
# at all: whatever happens is what the card said.
resume() { # <bin> <pdir> <tag> [VAR=value ...]
  local bin="$1" pdir="$2" tag="$3"; shift 3
  env MLFK_PERSIST_DIR="$PWD/$pdir" "$@" "$bin" \
    --flow "$IDLE" --input flow --flow-out "$BUILD/$tag.trace" \
    --pace 0 --bridge tverify --simdata "$BUILD/simdata.txt" --seed "$SEED" \
    --trace "$BUILD/$ID.trace.txt" --frames "$FRAMES" \
    --out "$BUILD/$tag.stream.txt" --timing "$BUILD/$tag.timing.txt" \
    --bstate-out "$BUILD/$tag.bstate.txt" "${GD[@]}" \
    > "$BUILD/$tag.out.txt" 2> "$BUILD/$tag.err.txt"
}
# A refused resume lands on TARGET SELECT and plays nothing, so the process
# exits on foh_dev.c's cross-guard ("--bridge but the flow never launched").
# That nonzero is EXPECTED and is not a failure; anything else is.
resume_tss() { # <bin> <pdir> <tag> <label>
  local bin="$1" pdir="$2" tag="$3" label="$4" rc=0
  resume "$bin" "$pdir" "$tag" || rc=$?
  if [ "$rc" != 0 ]; then
    grep -qxF 'foh_dev: --bridge given but the flow never launched' \
      "$BUILD/$tag.err.txt" \
      || { relay_lines < "$BUILD/$tag.err.txt" >&2
           fail "$label: the boot exited $rc for a reason other than never
  launching a run"; }
  fi
  grep -qxF 'foh_dev: resumed screen=target-select' "$BUILD/$tag.err.txt" \
    || { relay_lines < "$BUILD/$tag.err.txt" >&2
         fail "$label: the boot did not land on target select"; }
  ! grep -q 'target run restored' "$BUILD/$tag.err.txt" \
    || fail "$label: a target run was restored when the state was never
  written or was refused"
}

# load_golden <id> — sets ID/NAME/SEED/CHAR/TSTAGE/FRAMES/CODE/FROZEN/TFROZEN
# and materialises the trace and the flow, ALL from the golden's own manifest
# through the SHARED strict validator (the M0 single-param-source convention:
# nothing is re-typed here).
load_golden() {
  ID="$1"
  eval "$(node -e "
    const v = require('./$M4G/validate-target-manifest');
    const g = v.goldenByIdOrName(v.loadValidatedManifest(), '$ID');
    const q = (s) => JSON.stringify(String(s));
    console.log('NAME=' + q(g.name));
    console.log('SEED=' + q(g.seed));
    console.log('CHAR=' + q(g.char));
    console.log('TSTAGE=' + q(g.tstage));
    console.log('FRAMES=' + q(g.frames));
    console.log('TRACE=' + q(g.trace));
    console.log('CODE=' + q(g.customStage || ''));
  ")" || fail "no such target golden: $ID"
  FROZEN=$M4G/$NAME.sha256.json
  TFROZEN=$M4G/$NAME.target.sha256.json
  made "$FROZEN" "$TFROZEN" "$M4G/$TRACE"
  node "$SIM/trace-to-txt.js" "$M4G/$TRACE" "$BUILD/$ID.trace.txt" > /dev/null
  made "$BUILD/$ID.trace.txt"
  write_flow "$BUILD/$ID.flow" "$CHAR" "$TSTAGE"
  # the splice point: half way, so both halves are real runs.
  AT=$(( FRAMES / 2 ))
}

# judge <stream> <ctx> — BOTH planes, BOTH unchanged verifiers. Neither is
# allowed to stand in for the other: the sim's cannot see the target plane and
# the target plane's does not hash the players.
judge() { # <stream.txt> <ctx>
  node "$WRAPT" "$ID" "$1" "$BUILD/$ID.player.json" "$BUILD/$ID.target.json" \
    > /dev/null || fail "$2: the stream is not well-formed producer output"
  made "$BUILD/$ID.player.json" "$BUILD/$ID.target.json"
  node "$VERIFY" "$BUILD/$ID.player.json" "$FROZEN" | relay_lines \
    || fail "$2: the PLAYER stream does not conform to $NAME"
  node "$TVERIFY" "$BUILD/$ID.target.json" "$TFROZEN" | relay_lines \
    || fail "$2: the TARGET stream does not conform to $NAME"
}

# --- [2] the ticket's acceptance criterion -----------------------------------
# A target run survives a lid close and CONTINUES: the frames after the resume
# point are identical to the uninterrupted run, and the whole spliced stream
# conforms to BOTH frozen goldens under the unchanged verifiers.
echo "=== [2] continuation across a real lid close"
conf=0
for id in t01 t02 t03; do
  load_golden "$id"
  echo "  == $id ($NAME) char=$CHAR tstage=$TSTAGE"

  # (a) THE BASELINE, judged in its own right — so a continuation that matched
  # a broken baseline could not pass quietly.
  PA=$BUILD/$id-a-persist; rm -rf "$PA"; mkdir -p "$PA"
  [ -z "$CODE" ] || publish_slot "$PA"
  play "$BUILD/foh_dev_headless" "$PA" "$id-a" \
    || { relay_lines < "$BUILD/$id-a.err.txt" >&2
         fail "[2] $id: the uninterrupted run died"; }
  # the run really reached the stage the manifest names, through the menus.
  grep -qxF "TLAUNCH $(grep -oE '^TLAUNCH [0-9]+' "$BUILD/$id-a.trace" \
    | head -1 | awk '{print $2}') char=$CHAR tstage=$TSTAGE" \
    "$BUILD/$id-a.trace" \
    || { relay_lines < "$BUILD/$id-a.trace" >&2
         fail "[2] $id: the menu walk did not launch char=$CHAR tstage=$TSTAGE
  — the flow this check writes no longer reaches the golden's own stage"; }
  judge "$BUILD/$id-a.stream.txt" "[2] $id uninterrupted"

  # (b) THE LID, half way. A real SIGUSR1 through the real handler; the process
  # writes the pair, saves the settings and _exit(0)s.
  PB=$BUILD/$id-b-persist; rm -rf "$PB"; mkdir -p "$PB"
  [ -z "$CODE" ] || publish_slot "$PB"
  play "$BUILD/foh_dev_headless" "$PB" "$id-b" "MLFK_HIBERNATE_AT=$AT" \
    || { relay_lines < "$BUILD/$id-b.err.txt" >&2
         fail "[2] $id: the hibernating run did not exit 0"; }
  grep -qxF 'foh_dev: hibernate from=target-match resume=target-match' \
    "$BUILD/$id-b.err.txt" \
    || { relay_lines < "$BUILD/$id-b.err.txt" >&2
         fail "[2] $id: the lid did not arm a TARGET resume"; }
  nsaved="$(grep -cxF 'foh_persist: saved' "$BUILD/$id-b.err.txt")" || true
  [ "$nsaved" = 1 ] \
    || fail "[2] $id: the lid wrote the settings record $nsaved times (want 1)"
  made "$PB/mlfk-tmatch.sim" "$PB/mlfk-tmatch.hdr" "$PB/mlfk-persist.dat"
  # the settings record names the target match as the resume screen (15).
  grep -qxE '^resume 15$' "$PB/mlfk-persist.dat" \
    || { grep -n '^resume ' "$PB/mlfk-persist.dat" >&2
         fail "[2] $id: the record's resume row is not 'resume 15' (FOH_TMATCH)"; }
  # ...and the header says what foh_target_snap.c says it says.
  grep -qxE '^MLTMATCH1$' "$PB/mlfk-tmatch.hdr" \
    || grammar_die "[2] $id: the header does not start MLTMATCH1"
  grep -qxE "^TSTAGE 0*$TSTAGE\$" "$PB/mlfk-tmatch.hdr" \
    || grammar_die "[2] $id: the header does not name tstage $TSTAGE"
  grep -qxE "^CHAR $CHAR\$" "$PB/mlfk-tmatch.hdr" \
    || grammar_die "[2] $id: the header does not name char $CHAR"
  grep -qxE "^FRAME 0*$AT\$" "$PB/mlfk-tmatch.hdr" \
    || grammar_die "[2] $id: the header does not name frame $AT"
  # THE PLANE THIS TICKET IS ABOUT, read off the arm's own log line. A run
  # whose targets were all still standing at the splice point would make every
  # comparison below pass for the wrong reason, so the number is asserted
  # rather than hoped for.
  armed="$(grep -oE 'destroyed=[0-9]+/[0-9]+' "$BUILD/$id-b.err.txt" | head -1)"
  [ -n "$armed" ] \
    || grammar_die "[2] $id: the hibernate arm did not report the target plane"
  ndest="${armed#destroyed=}"; ndest="${ndest%%/*}"
  [ "$ndest" -ge 1 ] \
    || fail "[2] $id: the lid closed on a run with $ndest broken targets. The
  whole claim is that BROKEN TARGETS STAY BROKEN, and a splice point with none
  cannot state it — move the splice point, never the assertion."
  echo "     lid at frame $AT with $armed"

  # (c) THE NEXT BOOT, on that card, with NO launch arguments.
  resume "$BUILD/foh_dev_headless" "$PB" "$id-c" \
    || { relay_lines < "$BUILD/$id-c.err.txt" >&2
         fail "[2] $id: the resumed boot died"; }
  grep -qxF 'foh_dev: resumed screen=target-match' "$BUILD/$id-c.err.txt" \
    || { relay_lines < "$BUILD/$id-c.err.txt" >&2
         fail "[2] $id: the boot did not resume into the target run — the
  symptom this ticket exists to fix (the player lands on target select)"; }
  grep -qxF "foh_dev: target run restored frame=$AT $armed" \
    "$BUILD/$id-c.err.txt" \
    || { relay_lines < "$BUILD/$id-c.err.txt" >&2
         fail "[2] $id: the run did not come back at frame $AT with $armed.
  BROKEN TARGETS STAY BROKEN is the acceptance criterion, and this line is
  where the two sides of the lid state the same number."; }
  # NOR DOES IT REPLAY READY--GO! The set-up ss_load requires spawns
  # dVfx/start.js, whose instance lives in the RENDERER and so survives a sim
  # restore. Without the drop the player resumes a run already in progress and
  # watches it count down over their own broken targets. The count is read and
  # required non-zero, so a seam that silently stops dropping fails here rather
  # than passing by doing nothing. (check-match-resume.sh leg [7] is the twin.)
  tv_line="$(grep -m1 '^foh_dev: vfx of the discarded match dropped=' \
    "$BUILD/$id-c.err.txt")" \
    || { relay_lines < "$BUILD/$id-c.err.txt" >&2
         fail "[2] $id: the resumed run never dropped the set-up's vfx, so the
  Ready--Go! banner counts down over a run already in progress"; }
  tv_n="${tv_line##*=}"
  case "$tv_n" in ''|*[!0-9]*) fail "[2] $id: unparsable '$tv_line'";; esac
  [ "$tv_n" -ge 1 ] \
    || fail "[2] $id: dropped $tv_n vfx. The set-up spawns the Ready--Go!
  banner, so zero means the set-up stopped spawning and this seam now guards
  nothing — find out what changed rather than lowering the bound."
  # CONSUMED: a resumed run must not be resumable a second time.
  [ ! -e "$PB/mlfk-tmatch.hdr" ] && [ ! -e "$PB/mlfk-tmatch.sim" ] \
    || fail "[2] $id: the pair survived the resume. The player is playing that
  run now; a crash an hour later must not hand him back the state he left at
  the lid."
  first="$(grep -m1 '^F ' "$BUILD/$id-c.stream.txt" | awk '{print $2}')"
  [ "$first" = "$(( AT + 1 ))" ] \
    || fail "[2] $id: the resumed run started at frame $first, not $(( AT + 1 ))
  — it RESTARTED the run instead of continuing it"

  # (d) FRAME FOR FRAME against the uninterrupted run, BOTH planes. Split
  # through FILES, never pipes: `grep | head` hands grep a SIGPIPE and pipefail
  # would then report a check failure that is really a plumbing artefact.
  grep -E '^[FT] ' "$BUILD/$id-a.stream.txt" > "$BUILD/$id.frames-a.txt"
  tail -n "$(( 2 * (FRAMES - AT) ))" "$BUILD/$id.frames-a.txt" \
    > "$BUILD/$id.tail-a.txt"
  grep -E '^[FT] ' "$BUILD/$id-c.stream.txt" > "$BUILD/$id.tail-c.txt"
  cmp "$BUILD/$id.tail-a.txt" "$BUILD/$id.tail-c.txt" \
    || fail "[2] $id: the resumed run's frames differ from the uninterrupted
  run's. It resumed, but it is not the same run."

  # ...and the whole thing as ONE stream, judged by both frozen goldens. The
  # RNG counters and the TFIN finals come from the RESUMED process, so they
  # are its claim about the run, not the baseline's.
  {
    head -n "$(( 2 * AT ))" "$BUILD/$id.frames-a.txt"
    cat "$BUILD/$id.tail-c.txt"
    grep -E '^(RNG |TFIN |SIM OK$)' "$BUILD/$id-c.stream.txt"
  } > "$BUILD/$id.spliced.txt"
  judge "$BUILD/$id.spliced.txt" "[2] $id spliced"
  conf=$(( conf + 1 ))
done
[ "$conf" = 3 ] || fail "[2] only $conf golden(s) were covered"
echo "    [2] OK: $conf target runs continued across a real hibernate — one of"
echo "    them on a CUSTOM stage loaded from the card — and all three spliced"
echo "    streams were judged by BOTH unchanged verifiers against their goldens"

# --- [3] rule 2: a failed or refused snapshot DISARMS the resume -------------
# Ticket #29's rule, inherited whole: an armed resume with no state behind it
# is worse than no resume at all. Three ways for the write to fail, three times
# the player lands where he landed before this ticket — on target select —
# and the reason is said out loud while the SETTINGS are still saved.
echo "=== [3] a failed or refused snapshot disarms the resume"
load_golden t01
disarm_case() { # <label> <bin> <setup-fn> <expected substring>
  local label="$1" bin="$2" setup="$3" want="$4"
  local pd=$BUILD/d-$label-persist
  rm -rf "$pd"; mkdir -p "$pd"
  "$setup" "$pd"
  play "$bin" "$pd" "d-$label" "MLFK_HIBERNATE_AT=$AT" \
    || { relay_lines < "$BUILD/d-$label.err.txt" >&2
         fail "[3] $label: the hibernating run did not exit 0 — a card that
  cannot be written costs the resume, never the session"; }
  grep -q "target state NOT kept ($want)" "$BUILD/d-$label.err.txt" \
    || { relay_lines < "$BUILD/d-$label.err.txt" >&2
         fail "[3] $label: no 'target state NOT kept ($want)' line"; }
  # THE SETTINGS SURVIVED, which is the half that matters most.
  made "$pd/mlfk-persist.dat"
  grep -qxE '^resume 14$' "$pd/mlfk-persist.dat" \
    || { grep -n '^resume ' "$pd/mlfk-persist.dat" >&2
         fail "[3] $label: the record does not name FOH_TSS (14) — the
  downgrade did not happen, so a resume is armed with nothing behind it"; }
  # ...and the next boot really lands there.
  resume_tss "$BUILD/foh_dev_headless" "$pd" "d-$label-r" "[3] $label"
  echo "     $label -> disarmed, settings kept, boot lands on target select"
}
# (a) NO SEAM IN THE BUILD. The witness binary from leg [1]: the ops pointer is
# NULL, so the app has no way to keep the run, and must say so and downgrade
# rather than arm something it cannot deliver.
noop_setup() { :; }
disarm_case noseam "$BUILD/foh_dev_nots" noop_setup \
  "no target snapshot in this build"
# (b) THE SNAPSHOT WRITE FAILS. The snapshot's name is pre-created as a
# DIRECTORY, so ss_save's rename cannot publish over it — a real, measured
# class on journal-less vfat (a full or remounted-read-only card).
sim_dir_setup() { mkdir -p "$1/mlfk-tmatch.sim"; }
disarm_case simfail "$BUILD/foh_dev_headless" sim_dir_setup \
  "snapshot publish (rename) failed"
# (c) THE HEADER PUBLISH FAILS with the snapshot already on the card. The
# orphan must be cleaned up: 200 KB nothing will ever read is not a resume, it
# is litter on a small card. NON-EMPTY on purpose: ts_arm's first act is to
# remove the header, and remove() on an EMPTY directory succeeds (it rmdirs),
# which would clear the obstacle and make this pass rather than fail.
hdr_dir_setup() { mkdir -p "$1/mlfk-tmatch.hdr"; : > "$1/mlfk-tmatch.hdr/keep"; }
disarm_case hdrfail "$BUILD/foh_dev_headless" hdr_dir_setup \
  "rename publish failed"
[ ! -e "$BUILD/d-hdrfail-persist/mlfk-tmatch.sim" ] \
  || fail "[3] hdrfail: an orphaned snapshot was left on the card"
echo "    [3] OK: three ways for the write to fail, three disarmed resumes"

# --- [4] the read path refuses, by name --------------------------------------
# The pair is a resume token on a card, and the card is the one thing that can
# change while the machine is off. Every refusal names its own rule, and none
# of them is allowed to land the player inside a run.
echo "=== [4] the read path refuses, by name"
GOOD=$BUILD/good-persist; rm -rf "$GOOD"; mkdir -p "$GOOD"
play "$BUILD/foh_dev_headless" "$GOOD" "good-b" "MLFK_HIBERNATE_AT=$AT" \
  || fail "[4] could not produce a good pair"
made "$GOOD/mlfk-tmatch.hdr" "$GOOD/mlfk-tmatch.sim" "$GOOD/mlfk-persist.dat"

reseal() { # <hdr> — recompute the SUM over every preceding line
  node -e '
    const fs = require("fs"), c = require("crypto");
    const p = process.argv[1];
    const b = fs.readFileSync(p, "utf8").split("\n");
    const i = b.findIndex((l) => l.startsWith("SUM "));
    if (i < 0) throw new Error("no SUM line");
    const body = b.slice(0, i).join("\n") + "\n";
    b[i] = "SUM " + c.createHash("sha256").update(body).digest("hex");
    fs.writeFileSync(p, b.join("\n"));
  ' "$1"
}
refuse() { # <label> <mutate-fn> <expected substring> [expect-fresh-run]
  local label="$1" mut="$2" want="$3" fresh="${4:-}"
  local pd=$BUILD/r-$label-persist
  rm -rf "$pd"; mkdir -p "$pd"
  cp "$GOOD/mlfk-tmatch.hdr" "$GOOD/mlfk-tmatch.sim" "$GOOD/mlfk-persist.dat" \
    "$pd/"
  "$mut" "$pd"
  if [ -n "$fresh" ]; then
    # The HEADER was believed and the run was set up; the SNAPSHOT was not.
    # The run sitting in memory is a legal fresh one, so the honest outcome is
    # to say so and play it — never to die holding the player's evening
    # hostage to a bad file.
    resume "$BUILD/foh_dev_headless" "$pd" "r-$label" \
      || { relay_lines < "$BUILD/r-$label.err.txt" >&2
           fail "[4] $label: the boot died instead of playing a fresh run"; }
    grep -q "target state NOT restored ($want)" "$BUILD/r-$label.err.txt" \
      || { relay_lines < "$BUILD/r-$label.err.txt" >&2
           fail "[4] $label: the refusal did not name its rule ('$want')"; }
    local f1
    f1="$(grep -m1 '^F ' "$BUILD/r-$label.stream.txt" | awk '{print $2}')"
    [ "$f1" = 1 ] \
      || fail "[4] $label: the fresh run started at frame $f1, not 1"
  else
    resume_tss "$BUILD/foh_dev_headless" "$pd" "r-$label" "[4] $label"
    grep -q "target run NOT resumed ($want)" "$BUILD/r-$label.err.txt" \
      || { relay_lines < "$BUILD/r-$label.err.txt" >&2
           fail "[4] $label: the refusal did not name its rule ('$want')"; }
  fi
  echo "     $label -> refused: $want"
}

# PERTURB: EVERY EDIT BELOW MUST ACTUALLY EDIT SOMETHING.
#
# MEASURED, and it cost a red sweep: `s/^SUM \(.\)/SUM 0/` flips the SUM's
# first hex digit — unless that digit is already `0`, which is one header in
# sixteen, and #30's TU change happened to produce one (`SUM 0e4fb0...`). The
# fixture then edited NOTHING, the loader correctly believed a valid card, and
# the check reported the CODE as broken. Its siblings are the same shape:
# `BUILD .` -> `BUILD 0` has the same one-in-sixteen dud, `STAGE 00` and the
# 4-digit FRAME both assume a value this flow happens to produce today.
#
# A perturbation that silently perturbs nothing is worse than no tooth at all:
# it reads green forever and then, one build in sixteen, accuses the code. So
# every in-place edit goes through here, and a no-op is a LOUD failure that
# names the fixture rather than the feature.
perturb() { # <file> <sed-expr>... — apply, and prove the bytes moved
  local f="$1"; shift
  local before after
  before="$(shasum -a 256 < "$f")"
  sed -i.bak "$@" "$f"
  rm -f "$f.bak"
  after="$(shasum -a 256 < "$f")"
  [ "$before" != "$after" ] || fail "FIXTURE NO-OP: the perturbation
  $* did not change $f. The tooth it feeds cannot fail, so a green run here
  proves nothing. Fix the fixture (the file's contents moved out from under
  it); do NOT relax whatever assertion just failed downstream."
}

r_nohdr()  { rm -f "$1/mlfk-tmatch.hdr"; }
r_trunc()  { local f=$1/mlfk-tmatch.hdr t; t=$(head -c 100 "$f"); printf '%s' "$t" > "$f"; }
r_sumbad() { perturb "$1/mlfk-tmatch.hdr" -e 's/^SUM 0/SUM 1/' -e t -e 's/^SUM ./SUM 0/'; }
# a field edited WITHOUT resealing: the SUM must catch it, and it must be
# reported as CORRUPT and never as "from a different build". Integrity before
# meaning — the .mlstage rule, inherited whole.
r_stage_unsealed() {
  perturb "$1/mlfk-tmatch.hdr" 's/^TSTAGE 00$/TSTAGE 03/'
}
r_build_sealed() {
  perturb "$1/mlfk-tmatch.hdr" -e 's/^BUILD 0/BUILD 1/' -e t -e 's/^BUILD ./BUILD 0/'
  reseal "$1/mlfk-tmatch.hdr"
}
r_frame_sealed() {
  perturb "$1/mlfk-tmatch.hdr" "s/^FRAME 0*[0-9]\{4\}\$/FRAME 000000001799/"
  reseal "$1/mlfk-tmatch.hdr"
}
r_nosim()  { rm -f "$1/mlfk-tmatch.sim"; }

# The FULL parenthesised reason, never a substring of it: "named its rule" is
# the claim, and a prefix match would accept a refusal that named a different
# rule beginning with the same words.
refuse nohdr      r_nohdr          "no armed target run (no header)"
refuse truncated  r_trunc          "target header is the wrong length"
refuse sum        r_sumbad         "target header SUM mismatch (corrupt or edited)"
refuse unsealed   r_stage_unsealed "target header SUM mismatch (corrupt or edited)"
refuse otherbuild r_build_sealed   "target snapshot is from a different build"
# ...and the two that need the run to be SET UP before they can be asked,
# because they are questions about the PAIR rather than about the header. Both
# refuse in ss_load's / ts_restore's own words, relayed verbatim.
refuse nosim   r_nosim         "no such snapshot file" fresh
refuse framed  r_frame_sealed  "target header and snapshot disagree about the frame" fresh


# THE REFUSAL ONLY THIS SCREEN HAS. A custom target stage is played from a file
# on the SD card, and the card can be edited on a PC while the machine is off.
# Both cases land the player on TARGET SELECT rather than inside a run: a
# resume that cannot re-find its stage must DISARM, and starting a fresh run on
# whatever the card now holds would still be launching something he did not
# ask for.
echo "  == the custom stage, re-found or refused (t03)"
load_golden t03
CGOOD=$BUILD/c-good-persist; rm -rf "$CGOOD"; mkdir -p "$CGOOD"
publish_slot "$CGOOD"
play "$BUILD/foh_dev_headless" "$CGOOD" "c-good-b" "MLFK_HIBERNATE_AT=$AT" \
  || fail "[4] could not produce a good CUSTOM pair"
made "$CGOOD/mlfk-tmatch.hdr" "$CGOOD/mlfk-tmatch.sim" "$CGOOD/custom0.mlstage"
grep -qxE '^TSTAGE 10$' "$CGOOD/mlfk-tmatch.hdr" \
  || grammar_die "[4] the custom pair's header does not name tstage 10"

crefuse() { # <label> <mutate-fn> <expected substring>
  local label="$1" mut="$2" want="$3"
  local pd=$BUILD/c-$label-persist
  rm -rf "$pd"; mkdir -p "$pd"
  cp "$CGOOD/mlfk-tmatch.hdr" "$CGOOD/mlfk-tmatch.sim" \
    "$CGOOD/mlfk-persist.dat" "$CGOOD/custom0.mlstage" "$pd/"
  "$mut" "$pd"
  resume_tss "$BUILD/foh_dev_headless" "$pd" "c-$label" "[4] $label"
  grep -q "target run NOT resumed ($want)" "$BUILD/c-$label.err.txt" \
    || { relay_lines < "$BUILD/c-$label.err.txt" >&2
         fail "[4] $label: the refusal did not name its rule ('$want')"; }
  # DISARMED, not merely declined: nothing half-armed survives the boot.
  [ ! -e "$pd/mlfk-tmatch.hdr" ] \
    || fail "[4] $label: the header survived a refused boot"
  echo "     $label -> refused: $want"
}
# (a) THE STAGE IS GONE. Before this ticket's boot pre-flight this was FATAL —
# the launch bridge's own mlk_slot_load arm `return 1`s — so a player who
# deleted a stage from his card while the lid was shut got an app that died at
# boot. It must land on target select like any other refusal.
c_gone() { rm -f "$1/custom0.mlstage"; }
crefuse gone c_gone "no such slot file"
# (b) THE STAGE WAS EDITED, and correctly RE-SEALED — so the file is perfectly
# valid and the loader has no complaint. Only the SRC line can tell that it is
# not the stage the run was played on, which is the entire reason SRC exists.
c_edited() {
  node -e '
    const fs = require("fs"), c = require("crypto");
    const p = process.argv[1];
    const L = fs.readFileSync(p, "utf8").split("\n");
    if (L[0] !== "MLSTAGE1") throw new Error("not a slot file");
    const before = L[1];
    L[1] = L[1].replace(/^-50\.00,40\.00&/, "-40.00,40.00&");
    if (L[1] === before) {
      throw new Error("the share code moved: the edit this tooth makes no " +
                      "longer changes the stage, so the tooth does not bite");
    }
    const body = L[0] + "\n" + L[1] + "\n";
    L[2] = "SUM " + c.createHash("sha256").update(body).digest("hex");
    fs.writeFileSync(p, L.slice(0, 3).join("\n") + "\n");
  ' "$1/custom0.mlstage"
}
crefuse edited c_edited \
  "the target stage on the card is not the one this run was played on"
echo "    [4] OK: nine refusals, each naming its own rule, and two of them are"
echo "    the card itself having changed under a custom stage"

# --- [5] teeth ---------------------------------------------------------------
# CONTEXT.md, "Tooth": a check that cannot fail is not a check. Leg [2] is the
# claim; these are the two ways of breaking it, and each must be caught.
echo "=== [5] teeth"

# T1 — THE WHOLE REASON THIS TICKET IS NOT A COPY OF #29.
#
# MLFK_SNAP_SKIP=mod:targets makes ss_load read past the TARGET PLANE's row
# without applying it (sim_snapshot.c's completeness tooth). That is precisely
# the bug this ticket exists to prevent: the sim comes back perfect and the
# target plane comes back EMPTY, so the player's broken targets silently stand
# up again.
#
# The assertion is in three parts, and the middle one is the ticket:
#   * the PLAYER stream after the resume is BYTE-IDENTICAL to the
#     uninterrupted run, and the unchanged oracle/harness/verify-stream.js
#     ACCEPTS the spliced player stream — i.e. every assertion ticket #29
#     wrote passes;
#   * the TARGET stream DIFFERS, and the unchanged verify-target-stream.js
#     REJECTS the spliced target stream;
#   * the app itself reports the plane coming back at zero.
# If the first part ever stops holding, this tooth has become an ordinary
# continuation tooth and the sentence above is no longer true of it.
echo "  T1: the target plane's snapshot row dropped on the way back in"
load_golden t01
T1P=$BUILD/t1-persist; rm -rf "$T1P"; mkdir -p "$T1P"
play "$BUILD/foh_dev_headless" "$T1P" t1-b "MLFK_HIBERNATE_AT=$AT" \
  || fail "[5] T1: the hibernating run did not exit 0"
resume "$BUILD/foh_dev_headless" "$T1P" t1-c "MLFK_SNAP_SKIP=mod:targets" \
  || { relay_lines < "$BUILD/t1-c.err.txt" >&2; fail "[5] T1: the boot died"; }
grep -qxF "foh_dev: target run restored frame=$AT destroyed=0/10" \
  "$BUILD/t1-c.err.txt" \
  || { relay_lines < "$BUILD/t1-c.err.txt" >&2
       fail "[5] T1 DID NOT BITE: dropping the target plane's row did not empty
  it, so MLFK_SNAP_SKIP no longer names the row this tooth is about"; }
grep -E '^[FT] ' "$BUILD/t1-c.stream.txt" > "$BUILD/t1.tail-c.txt"
grep '^F ' "$BUILD/t01.tail-a.txt" > "$BUILD/t1.player-a.txt"
grep '^F ' "$BUILD/t1.tail-c.txt"  > "$BUILD/t1.player-c.txt"
grep '^T ' "$BUILD/t01.tail-a.txt" > "$BUILD/t1.target-a.txt"
grep '^T ' "$BUILD/t1.tail-c.txt"  > "$BUILD/t1.target-c.txt"
cmp -s "$BUILD/t1.player-a.txt" "$BUILD/t1.player-c.txt" \
  || fail "[5] T1 IS NOT THE TOOTH IT CLAIMS TO BE: dropping the TARGET plane
  changed the PLAYER stream too, so it no longer demonstrates a failure that
  ticket #29's assertions cannot see. Re-establish the separation or rewrite
  the tooth's claim — do not weaken the comparison."
! cmp -s "$BUILD/t1.target-a.txt" "$BUILD/t1.target-c.txt" \
  || fail "[5] T1 DID NOT BITE: dropping the whole target plane changed NOTHING
  in the resumed TARGET stream, so leg [2]'s comparison is not actually judging
  the restored target plane"
{
  head -n "$(( 2 * AT ))" "$BUILD/t01.frames-a.txt"
  cat "$BUILD/t1.tail-c.txt"
  grep -E '^(RNG |TFIN |SIM OK$)' "$BUILD/t1-c.stream.txt"
} > "$BUILD/t1.spliced.txt"
node "$WRAPT" t01 "$BUILD/t1.spliced.txt" "$BUILD/t1.player.json" \
  "$BUILD/t1.target.json" > /dev/null \
  || fail "[5] T1: the perturbed stream is not well-formed producer output"
node "$VERIFY" "$BUILD/t1.player.json" "$FROZEN" > /dev/null 2>&1 \
  || fail "[5] T1 IS NOT THE TOOTH IT CLAIMS TO BE: the SIM's own verifier
  REJECTED the spliced player stream. The tooth's whole point is that it does
  not, and that only the target plane's verifier catches this."
if node "$TVERIFY" "$BUILD/t1.target.json" "$TFROZEN" > /dev/null 2>&1; then
  fail "[5] T1 DID NOT BITE: the unchanged verify-target-stream.js ACCEPTED a
  stream produced with the target plane's snapshot row dropped"
fi
echo "     T1 bites — and the sim's verifier accepted the very same run, which"
echo "     is exactly the failure ticket #29's assertions cannot see"

# T2 — THE RESUME POINT. A perturbed COPY of foh_dev.c whose target loop starts
# at 0 instead of the restored frame (the check-device-foh.sh T12 /
# check-hibernate.sh T3 pattern; the tree itself is never edited). That is
# precisely the regression the ticket names — a run RESTARTS while claiming to
# have resumed — so leg [2](c)/(d) must catch it.
echo "  T2: a perturbed foh_dev.c copy whose target loop starts at 0"
mkdir -p "$BUILD/t2/foh"
# `0 * tmatchResumeFrom` rather than a bare 0: the variable's only READ is the
# line being perturbed, and dropping it entirely makes the copy fail to build
# under -Werror=unused-but-set-variable — a tooth that cannot compile is not a
# tooth. The loop still starts at frame 0, which is the regression.
sed 's|^        for (long f = tmatchResumeFrom; f < loopMax; f++) {$|        for (long f = 0 * tmatchResumeFrom; f < loopMax; f++) {|' \
  "$FOH/foh_dev.c" > "$BUILD/t2/foh/foh_dev.c"
made "$BUILD/t2/foh/foh_dev.c"
! cmp -s "$FOH/foh_dev.c" "$BUILD/t2/foh/foh_dev.c" \
  || grammar_die "[5] T2: the perturbation matched nothing — the resumed target
  loop's `for` line moved, so this tooth is vacuous"
# shellcheck disable=SC1090
(. "$RECIPE"; build_foh_headless "$BUILD/t2/foh/foh_dev.c" \
  "$BUILD/foh_dev_t2" "-I$FOH") 2>&1 | relay_lines
made "$BUILD/foh_dev_t2"
T2P=$BUILD/t2-persist; rm -rf "$T2P"; mkdir -p "$T2P"
play "$BUILD/foh_dev_t2" "$T2P" t2-b "MLFK_HIBERNATE_AT=$AT" \
  || fail "[5] T2: the hibernating run did not exit 0"
resume "$BUILD/foh_dev_t2" "$T2P" t2-c \
  || { relay_lines < "$BUILD/t2-c.err.txt" >&2; fail "[5] T2: the boot died"; }
f2="$(grep -m1 '^F ' "$BUILD/t2-c.stream.txt" | awk '{print $2}')"
[ "$f2" = 1 ] \
  || fail "[5] T2 DID NOT BITE: the perturbed build still started at frame $f2"
grep -E '^[FT] ' "$BUILD/t2-c.stream.txt" > "$BUILD/t2.tail-c.txt"
! cmp -s "$BUILD/t01.tail-a.txt" "$BUILD/t2.tail-c.txt" \
  || fail "[5] T2 DID NOT BITE: a run replayed from frame 1 produced the same
  tail as one continued from $AT"
echo "     T2 bites"

# --- no-commit guard ----------------------------------------------------------
git_dirty_after="$(tree_fingerprint)" \
  || fail "could not fingerprint the working tree after the run"
[ "$git_dirty_before" = "$git_dirty_after" ] \
  || fail "this check modified the working tree — it writes only into $BUILD
  and $TABLES, both ignored"

echo "TARGET RESUME OK"
