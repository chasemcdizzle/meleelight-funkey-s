#!/usr/bin/env bash
# port/foh/check-match-resume.sh — ticket #29: a match survives the lid.
#
# WHAT THIS CHECK IS FOR
# ----------------------
# Closing the lid mid-match and opening it again must CONTINUE that match —
# same percent, stocks, positions, clock and opponent — not restart it and not
# land on the character select. "Continue" is not a thing you can eyeball, so
# it is judged the only way this project judges a sim: by CONTINUATION of the
# per-frame checksum stream, through the UNCHANGED
# oracle/harness/verify-stream.js, against a FROZEN golden.
#
# THE SHAPE, and why it is this shape (CONTEXT.md, "the same state"): a byte
# round trip catches a serialiser bug and a continuation catches a
# COMPLETENESS bug — state nobody knew had to be saved. Ticket #28 owns both
# for the sim in isolation (port/sim/check-sim-snapshot.sh). What #28 could
# NOT prove is that the thing driving them is a lid: its controls are
# environment variables read by sim_main.c, which the app does not link. So
# this check drives the REAL path end to end:
#
#   A  an uninterrupted match, replayed through the app, judged against the
#      frozen golden — so a continuation cannot pass by matching a broken
#      baseline;
#   B  the same match with a REAL SIGUSR1 delivered mid-match, taken by the
#      REAL handler and the REAL tdev_hibernate_check arm, which writes the
#      snapshot, writes the settings record and _exit(0)s;
#   C  a SECOND PROCESS booted on that same card, which resumes into the match
#      and plays the rest;
#   and then C's frames are compared to A's frame for frame, and spliced onto
#   A's prefix and judged as one stream by the oracle's own verifier.
#
# THE SENDER IS THE ONLY THING SIMULATED. MLFK_HIBERNATE_AT raises SIGUSR1 at
# a known frame because a continuation has to splice at a known frame and an
# external `kill -USR1` races the wall clock. Everything downstream of the
# signal — the handler, the flag, the arm, the ordering, the exit — is the
# product path. The EXTERNAL sender keeps its own proof in
# port/foh/check-hibernate.sh leg [4], which signals a real running process and
# never sets this variable. NOT PROVEN HERE, and named rather than implied: no
# leg below delivers the signal from outside the process, so "an external kill
# mid-MATCH arms this" rests on leg [4]'s proof that the route works plus the
# fact that the arm is the same arm.
#
# TWO GOLDENS, chosen for what they cover rather than for being two:
#   g01  fox vs marth on battlefield  — human inputs, static stage.
#   m01  falcon vs a d1 CPU on ystory — the LIVE C ai.c (--cpu-live), whose
#        module state ticket #29 classified and put in the snapshot, and a
#        stage with MOVING PLATFORMS, whose per-frame plane is module state
#        too (sim-modstate.frozen.txt). A resume that dropped either would
#        pass on g01 and fail here.
# m01's CPU is difficulty 1 and that is not cosmetic: the FOH's own persisted
# cpudiff column is 1..4, so the oracle's d5 CPU goldens cannot round-trip
# through a settings record at all. m01 is the CPU golden this seam can carry.
#
# HOST ONLY. Prints `MATCH RESUME OK`, exit 0. Any deviation exits nonzero.
set -euo pipefail
cd "$(dirname "$0")/../.."

FOH=port/foh
GFX=port/gfx
CAL=port/sim/calib
SIM=port/sim/sim
TABLES=pipeline/build/mres-tables
BUILD=$FOH/build/match-resume
DEVFOH=$FOH/check-device-foh.sh
VERIFY=oracle/harness/verify-stream.js

fail() { echo "MATCH RESUME FAIL: $1" >&2; exit 1; }
grammar_die() { echo "MATCH RESUME FAIL: $1" >&2; exit 2; }
relay_lines() { sed 's/^/  | /'; }
made() {
  local f
  for f in "$@"; do [ -s "$f" ] || fail "expected artifact missing/empty: $f"; done
}
sha_of() { shasum -a 256 "$1" | cut -d' ' -f1; }

LOCK=$FOH/build/match-resume.lock
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
# The claims this check is built on, asserted textually, so a refactor that
# removes one fails loudly here rather than making the check quietly vacuous.
echo "=== [0] pins"

# (a) THE VERDICT IS THE ORACLE'S. verify-stream.js is HARD RULE 3 territory
#     and its bytes are pinned, so this check cannot go green against a
#     weakened verifier. The digest is the same one check-sim-snapshot.sh
#     pins, read from THERE rather than re-typed — one number in the tree.
VERIFY_SHA_SRC=port/sim/check-sim-snapshot.sh
made "$VERIFY_SHA_SRC" "$VERIFY"
VERIFY_SHA="$(grep -oE '^VERIFY_SHA=[0-9a-f]{64}$' "$VERIFY_SHA_SRC" \
  | head -1 | cut -d= -f2)"
[ -n "$VERIFY_SHA" ] \
  || grammar_die "[0] no sha256 pin found in $VERIFY_SHA_SRC — the verifier
  pin this check inherits has moved"
[ "$(sha_of "$VERIFY")" = "$VERIFY_SHA" ] \
  || fail "[0] $VERIFY does not match the digest pinned in $VERIFY_SHA_SRC.
  The stream verifier is the oracle's and is never edited (HARD RULE 3)."

# (b) THE ORDERING RULE, STRUCTURALLY. Rule 1 of ticket #29: the snapshot is
#     written BEFORE the settings record, because the settings record is what
#     ARMS the resume — so an interruption between them leaves the previous
#     settings file intact and nothing armed. Leg [3] proves the consequence
#     behaviourally; this proves the order itself, which a behaviour test on a
#     machine that never misses the window cannot see.
HIB_SRC=$FOH/foh_dev.c
made "$HIB_SRC"
ARM_LN="$(grep -n 'foh_match_snap_ops->arm(&G' "$HIB_SRC" | head -1 | cut -d: -f1)"
COL_LN="$(grep -n '^  foh_persist_collect(&g_persist, f);$' "$HIB_SRC" | head -1 | cut -d: -f1)"
SAV_LN="$(grep -n '^  tdev_persist_save();$' "$HIB_SRC" | head -1 | cut -d: -f1)"
[ -n "$ARM_LN" ] && [ -n "$COL_LN" ] && [ -n "$SAV_LN" ] \
  || grammar_die "[0] could not find the hibernate arm, the collect and the
  save in $HIB_SRC — the three lines this ordering pin is about have moved,
  so the pin would be vacuous"
[ "$ARM_LN" -lt "$COL_LN" ] && [ "$COL_LN" -lt "$SAV_LN" ] \
  || fail "[0] the match snapshot is armed at line $ARM_LN, after the settings
  collect ($COL_LN) / save ($SAV_LN). Ticket #29 rule 1: the snapshot goes
  first, so that if the ~100 ms grace window is ever missed what is lost is
  the snapshot and never the player's settings."

# (c) THE M2 EXIT GATE IS UNTOUCHED. sim_snapshot.c joined the FOH build in
#     this ticket; it must NOT have joined the gate's frozen TU list.
GATE=port/sim/check-sim.sh
made "$GATE"
! grep -q 'sim_snapshot' "$GATE" \
  || fail "[0] $GATE now links sim_snapshot.c. The M2 EXIT GATE's TU list is
  frozen and the snapshot seam exists precisely so it need not change."
echo "  [0] OK: verifier pinned, snapshot-before-settings ordering asserted,"
echo "         the M2 gate's TU list still free of the snapshot"

# --- [1] data planes + the two binaries --------------------------------------
echo "=== [1] data planes and binaries"
if [ ! -s "$TABLES/ml_targets.c" ] || [ ! -s "$TABLES/assets/menu.img1" ]; then
  { bash pipeline/extractor/build-extractor.sh; } 2>&1 | relay_lines
  { node pipeline/run.js --only animations,tables,stages,targets,assets \
      --out "$TABLES"; } 2>&1 | relay_lines
fi
made "$TABLES/ml_tables.c" "$TABLES/ml_stages.c" "$TABLES/ml_targets.c" \
     "$TABLES/assets/menu.img1"
export MLFK_DATA_DIR="$PWD/$TABLES"
rm -rf "$BUILD"
mkdir -p "$BUILD"

# The recipe is EXTRACTED from check-device-foh.sh, never copied — the
# check-hibernate.sh / check-mexit-reentry.sh pattern, with its anchoring
# inherited verbatim so this cannot silently build something else.
made "$DEVFOH"
RECIPE=$BUILD/recipe.sh
nstart="$(grep -cxF 'CFLAGS_COMMON=(-ffp-contract=off -Wall -Wextra -Werror' \
  "$DEVFOH")" || true
[ "$nstart" = 1 ] || grammar_die "[1] check-device-foh.sh has $nstart
  CFLAGS_COMMON anchor lines (want exactly 1) — the extracted recipe moved"
awk '
  /^CFLAGS_COMMON=\(-ffp-contract=off -Wall -Wextra -Werror$/ { inr = 1 }
  inr { print }
  inr && /^}$/ { exit }
' "$DEVFOH" > "$RECIPE"
made "$RECIPE"
grep -qxF 'build_foh_headless() { # <foh_dev_src> <out> [extra cc args...]' \
  "$RECIPE" || grammar_die "[1] extracted recipe does not carry build_foh_headless"
# The two TUs this ticket added must be IN the recipe: without them there is
# no feature, and a check that built them itself would be proving something
# the product does not do.
grep -q 'sim_snapshot\.c' "$RECIPE" \
  || grammar_die "[1] the extracted recipe no longer links sim_snapshot.c —
  the match snapshot's writer/reader is gone from the app's build"
grep -q 'foh_match_snap\.c' "$RECIPE" \
  || grammar_die "[1] the extracted recipe no longer links foh_match_snap.c —
  the lid seam is gone from the app's build"
# THE WITNESS FIRST, and the order is not arbitrary: the extracted recipe
# rebuilds raster.o and REMOVES $BUILD/foh_dev_headless at source time (that is
# check-device-foh.sh's own line, inherited whole), so sourcing the witness
# variant afterwards would delete the binary this check just built.
#
# It is the same recipe with foh_match_snap.c REMOVED, and it proves the
# pointer seam is real — the app still links and still runs, and every arm
# takes its "no match snapshot in this build" branch — which is what makes the
# claim in foh_match_snap.h checkable instead of decorative.
RECIPE_NOMS=$BUILD/recipe-noms.sh
sed '/foh_match_snap\.c/d' "$RECIPE" > "$RECIPE_NOMS"
cmp -s "$RECIPE" "$RECIPE_NOMS" \
  && grammar_die "[1] the witness recipe matched the real one — the
  foh_match_snap.c line moved, so the seam witness would be vacuous"
(
  # shellcheck disable=SC1090
  . "$RECIPE_NOMS"
  build_foh_headless "$FOH/foh_dev.c" "$BUILD/foh_dev_noms"
) || fail "[1] the seam witness build failed: foh_dev.c cannot be built
  without foh_match_snap.c, so the NULL-ops arm is unreachable and the
  header's claim about it is false"
made "$BUILD/foh_dev_noms"

# shellcheck disable=SC1090 — derived above, not tracked
. "$RECIPE"
build_foh_headless "$FOH/foh_dev.c" "$BUILD/foh_dev_headless" \
  || fail "[1] host twin build failed"
made "$BUILD/foh_dev_headless"
echo "  [1] OK: host twin + a witness twin built without the seam TU"

# --- shared run plumbing ------------------------------------------------------
node "$CAL/dump-sim-data.js" --out "$BUILD/simdata.txt" > /dev/null
made "$BUILD/simdata.txt"
IDLE=$BUILD/idle.flow
printf 'FLOW1\nI 1 -\nEND 10\n' > "$IDLE"
made "$IDLE"

GD=(--gfxdata "$GFX/gfxdata-frozen.txt" --vfxdata "$GFX/vfxdata-frozen.txt"
    --glyphs "$GFX/vfxglyphs-frozen.txt" --anim-dir "$TABLES")

# play <bin> <persist-dir> <tag> [env...] — a DIRECT match launch, i.e. the
# same launch seam a menu launch reaches, with the FOH phase skipped. `env` is
# used rather than a `VAR=x func` prefix on purpose: whether such a prefix
# survives a shell function differs between bash modes, and a hibernate
# control that silently leaked into the next run would be an invisible false
# pass (the check-sim-snapshot.sh lesson, inherited).
play() { # <bin> <pdir> <tag> [VAR=value ...]
  local bin="$1" pdir="$2" tag="$3"; shift 3
  env MLFK_PERSIST_DIR="$PWD/$pdir" "$@" "$bin" \
    --p1 "$P1" --p2 "$P2" --p2type "$P2TYPE" --difficulty "$DIFF" \
    --stage "$STAGE" --pace 0 ${CPUARG[@]+"${CPUARG[@]}"} \
    --bridge verify --simdata "$BUILD/simdata.txt" --seed "$SEED" \
    --trace "$BUILD/$ID.trace.txt" --frames "$FRAMES" \
    --out "$BUILD/$tag.stream.txt" --timing "$BUILD/$tag.timing.txt" \
    --bstate-out "$BUILD/$tag.bstate.txt" "${GD[@]}" \
    > "$BUILD/$tag.out.txt" 2> "$BUILD/$tag.err.txt"
}

# resume <bin> <persist-dir> <tag> [env...] — a BOOT on that card. No launch
# arguments at all: everything the match needs comes off the card, which is
# the point.
resume() { # <bin> <pdir> <tag> [VAR=value ...]
  local bin="$1" pdir="$2" tag="$3"; shift 3
  env MLFK_PERSIST_DIR="$PWD/$pdir" "$@" "$bin" \
    --flow "$IDLE" --input flow --flow-out "$BUILD/$tag.trace" --pace 0 \
    ${CPUARG[@]+"${CPUARG[@]}"} \
    --bridge verify --simdata "$BUILD/simdata.txt" --seed "$SEED" \
    --trace "$BUILD/$ID.trace.txt" --frames "$FRAMES" \
    --out "$BUILD/$tag.stream.txt" --timing "$BUILD/$tag.timing.txt" \
    --bstate-out "$BUILD/$tag.bstate.txt" "${GD[@]}" \
    > "$BUILD/$tag.out.txt" 2> "$BUILD/$tag.err.txt"
}

# resume_css <bin> <pdir> <tag> — a boot that is EXPECTED to land on the
# character select. Such a run legitimately exits nonzero: with an evidence
# bridge attached and no match ever launched, foh_dev.c's A2 cross-guard fires
# ("--bridge given but the flow never launched"), which is the very thing this
# check wants to see — no match was played. So the exit code is not the
# assertion; the two lines are, and an exit for any OTHER reason is still a
# failure.
resume_css() { # <bin> <pdir> <tag> <label>
  local bin="$1" pdir="$2" tag="$3" label="$4" rc=0
  resume "$bin" "$pdir" "$tag" || rc=$?
  if [ "$rc" != 0 ]; then
    grep -qxF 'foh_dev: --bridge given but the flow never launched' \
      "$BUILD/$tag.err.txt" \
      || { relay_lines < "$BUILD/$tag.err.txt" >&2
           fail "$label: the boot exited $rc for a reason other than never
  launching a match"; }
  fi
  grep -qxF 'foh_dev: resumed screen=css' "$BUILD/$tag.err.txt" \
    || { relay_lines < "$BUILD/$tag.err.txt" >&2
         fail "$label: the boot did not land on the character select"; }
  ! grep -q 'match restored' "$BUILD/$tag.err.txt" \
    || fail "$label: a match was restored from state that was never written
  or was refused"
}

# load_golden <id> — sets ID/NAME/SEED/P1/P2/STAGE/FRAMES/DIFF/P2TYPE/CPUARG,
# MANIFEST and FROZEN from the golden's own manifest (the M0 single-param-
# source convention: nothing here is re-typed).
load_golden() {
  ID="$1"
  case "$ID" in
    g*) MANIFEST=oracle/goldens/manifest.json; GDIR=oracle/goldens ;;
    m*) MANIFEST=port/goldens-m4/manifest.json; GDIR=port/goldens-m4 ;;
    *) grammar_die "unknown golden family: $ID" ;;
  esac
  eval "$(node -e "
    const m=require('./$MANIFEST');
    const g=m.goldens.find(x=>x.id==='$ID');
    if(!g){console.error('no such golden');process.exit(1);}
    console.log('NAME='+g.name);
    console.log('SEED='+g.seed);
    console.log('P1='+g.p1); console.log('P2='+g.p2);
    console.log('STAGE='+g.stage); console.log('FRAMES='+g.frames);
    console.log('CPU='+(g.cpu?1:0));
    console.log('DIFF='+(g.difficulty||1));
    console.log('TRACE='+g.trace);
  ")"
  FROZEN=$GDIR/$NAME.sha256.json
  made "$FROZEN"
  P2TYPE=0
  CPUARG=()
  if [ "$CPU" = 1 ]; then
    P2TYPE=1
    # --cpu-live: the REAL C ai.c drives the opponent (M4 task 5). No
    # AIBRIDGE1 artifact is involved, so the AI's own module state is
    # genuinely live and genuinely has to survive the lid.
    CPUARG=(--cpu-live)
    [ "$DIFF" -ge 1 ] && [ "$DIFF" -le 4 ] \
      || grammar_die "golden $ID is difficulty $DIFF, outside the FOH's own
  persisted cpudiff column (1..4). A settings record cannot carry it, so a
  hibernate on it would die in the save rather than test this feature."
  fi
  node "$SIM/trace-to-txt.js" "$GDIR/$TRACE" "$BUILD/$ID.trace.txt" > /dev/null
  made "$BUILD/$ID.trace.txt"
  AT=$(( FRAMES / 2 ))
}

judge() { # <id> <stream> <label>
  node "$SIM/wrap-run.js" "$1" "$2" "$2.json" "$MANIFEST" > /dev/null \
    || fail "$3: the stream is not a well-formed run"
  node "$VERIFY" "$2.json" "$FROZEN" \
    || fail "$3: the stream does not conform to the frozen golden"
}

# --- [2] the continuation -----------------------------------------------------
# THE TICKET'S ACCEPTANCE CRITERION, and the only leg that can prove it: the
# frames after the resume point are identical to an uninterrupted run, and the
# whole spliced stream conforms to the frozen golden under the oracle's own
# verifier.
echo "=== [2] continuation across a real lid close"
conf=0
for id in g01 m01; do
  load_golden "$id"
  echo "  == $id ($NAME)"

  # (a) THE BASELINE, judged in its own right — so a continuation that matched
  #     a broken baseline could not pass quietly.
  PA=$BUILD/$id-a-persist; rm -rf "$PA"; mkdir -p "$PA"
  play "$BUILD/foh_dev_headless" "$PA" "$id-a" \
    || { relay_lines < "$BUILD/$id-a.err.txt" >&2
         fail "[2] $id: the uninterrupted run died"; }
  judge "$id" "$BUILD/$id-a.stream.txt" "[2] $id uninterrupted"

  # (b) THE LID, at the half-way frame. A real SIGUSR1 through the real
  #     handler; the process writes the pair, saves the settings and _exit(0)s.
  PB=$BUILD/$id-b-persist; rm -rf "$PB"; mkdir -p "$PB"
  play "$BUILD/foh_dev_headless" "$PB" "$id-b" "MLFK_HIBERNATE_AT=$AT" \
    || { relay_lines < "$BUILD/$id-b.err.txt" >&2
         fail "[2] $id: the hibernating run did not exit 0"; }
  grep -qxF 'foh_dev: hibernate from=match resume=match' "$BUILD/$id-b.err.txt" \
    || { relay_lines < "$BUILD/$id-b.err.txt" >&2
         fail "[2] $id: the lid did not arm a MATCH resume"; }
  nsaved="$(grep -cxF 'foh_persist: saved' "$BUILD/$id-b.err.txt")" || true
  [ "$nsaved" = 1 ] \
    || fail "[2] $id: the hibernate arm performed $nsaved settings saves (want
  exactly 1) — inside a 0.1 s grace a second SD write is a second chance to
  miss it"
  made "$PB/mlfk-match.sim" "$PB/mlfk-match.hdr" "$PB/mlfk-persist.dat"
  grep -qxE '^resume 13$' "$PB/mlfk-persist.dat" \
    || { grep -n 'resume' "$PB/mlfk-persist.dat" >&2 || true
         grammar_die "[2] $id: the card does not carry 'resume 13' (FOH_MATCH)"; }
  # The header says which match it is, in the fixed grammar foh_match_snap.c
  # anchors on. Checked here so a silently reshaped header fails at the write,
  # not three legs later at a read.
  grep -qxE '^MLMATCH1$' "$PB/mlfk-match.hdr" \
    || grammar_die "[2] $id: the match header is not MLMATCH1"
  grep -qxE "^STAGE 0*$STAGE\$" "$PB/mlfk-match.hdr" \
    || grammar_die "[2] $id: the header does not name stage $STAGE"
  grep -qxE "^FRAME 0*$AT\$" "$PB/mlfk-match.hdr" \
    || grammar_die "[2] $id: the header does not name frame $AT"

  # (c) THE NEXT BOOT, on that card, with NO launch arguments.
  resume "$BUILD/foh_dev_headless" "$PB" "$id-c" \
    || { relay_lines < "$BUILD/$id-c.err.txt" >&2
         fail "[2] $id: the resumed boot died"; }
  grep -qxF 'foh_dev: resumed screen=match' "$BUILD/$id-c.err.txt" \
    || { relay_lines < "$BUILD/$id-c.err.txt" >&2
         fail "[2] $id: the boot did not resume into the match — this is the
  symptom the ticket exists to fix (the player lands on the character select)"; }
  grep -qxF "foh_dev: match restored frame=$AT" "$BUILD/$id-c.err.txt" \
    || { relay_lines < "$BUILD/$id-c.err.txt" >&2
         fail "[2] $id: the match was not RESTORED at frame $AT"; }
  # CONSUMED: a resumed match must not be resumable a second time.
  [ ! -e "$PB/mlfk-match.hdr" ] && [ ! -e "$PB/mlfk-match.sim" ] \
    || fail "[2] $id: the pair survived the resume. The player is playing that
  match now; a crash later must not hand back the state he left at the lid."
  first="$(grep -m1 '^F ' "$BUILD/$id-c.stream.txt" | awk '{print $2}')"
  [ "$first" = "$(( AT + 1 ))" ] \
    || fail "[2] $id: the resumed run started at frame $first, not $(( AT + 1 ))
  — it RESTARTED the match instead of continuing it"

  # (d) FRAME FOR FRAME against the uninterrupted run. Split through FILES,
  #     never pipes: `grep | head` hands grep a SIGPIPE and pipefail would then
  #     report a check failure that is really a plumbing artefact.
  grep '^F ' "$BUILD/$id-a.stream.txt" > "$BUILD/$id.frames-a.txt"
  tail -n "$(( FRAMES - AT ))" "$BUILD/$id.frames-a.txt" > "$BUILD/$id.tail-a.txt"
  grep '^F ' "$BUILD/$id-c.stream.txt" > "$BUILD/$id.tail-c.txt"
  cmp "$BUILD/$id.tail-a.txt" "$BUILD/$id.tail-c.txt" \
    || fail "[2] $id: the frames after the resume differ from the uninterrupted
  run — something the match was carrying did not survive the lid"

  # (e) ...and the verdict, which is not ours: spliced onto the prefix and
  #     judged whole by the unchanged verifier against the frozen golden. The
  #     RNG trailer comes from the RESUMED process, so its rngCalls pin can
  #     only hold if the counters crossed the lid too.
  {
    head -n "$AT" "$BUILD/$id.frames-a.txt"
    cat "$BUILD/$id.tail-c.txt"
    grep -E '^(RNG |SIM OK$)' "$BUILD/$id-c.stream.txt"
  } > "$BUILD/$id.spliced.txt"
  judge "$id" "$BUILD/$id.spliced.txt" "[2] $id spliced"
  conf=$(( conf + 1 ))
done
[ "$conf" = 2 ] || fail "[2] only $conf golden(s) were covered"
echo "  [2] OK: $conf matches continued across a real hibernate, both spliced"
echo "         streams judged by the unchanged verifier against their goldens"

# --- [3] the rules: a failed or refused snapshot DISARMS the resume ----------
# Rule 2 of the ticket, which is the rule the target builder's document resume
# already follows: an armed resume with no state behind it is worse than no
# resume. Three ways to fail the write, three times the player must land where
# he landed before this ticket — on the character select — with the reason
# said out loud and his SETTINGS still saved.
echo "=== [3] a failed or refused snapshot disarms the resume"
load_golden g01

disarm_case() { # <label> <bin> <setup-fn> <expected substring>
  local label="$1" bin="$2" setup="$3" want="$4"
  local pd=$BUILD/d-$label-persist
  rm -rf "$pd"; mkdir -p "$pd"
  "$setup" "$pd"
  play "$bin" "$pd" "d-$label" "MLFK_HIBERNATE_AT=$AT" \
    || { relay_lines < "$BUILD/d-$label.err.txt" >&2
         fail "[3] $label: the hibernating run did not exit 0 — a card it
  cannot write to must cost the resume, never the session"; }
  grep -q "match state NOT kept ($want)" "$BUILD/d-$label.err.txt" \
    || { relay_lines < "$BUILD/d-$label.err.txt" >&2
         fail "[3] $label: no 'match state NOT kept ($want)' line"; }
  # THE SETTINGS SURVIVED — the half of rule 1 that matters to the player.
  made "$pd/mlfk-persist.dat"
  grep -qxE '^resume 06$' "$pd/mlfk-persist.dat" \
    || { grep -n 'resume' "$pd/mlfk-persist.dat" >&2 || true
         fail "[3] $label: the card is armed for something other than FOH_CSS
  (06) even though nothing was written. That is the lie the old FOH_MATCH ->
  FOH_CSS redirect existed to prevent."; }
  # ...and the boot goes to the character select and says nothing about a match.
  resume_css "$bin" "$pd" "d-$label-r" "[3] $label"
  echo "     $label -> disarmed, settings kept, boot lands on the CSS"
}

# (a) NO SEAM IN THE BUILD. The witness binary from leg [1]: the ops pointer
#     is NULL, so the app has no way to keep a match, and it must say so and
#     downgrade rather than arm something it cannot deliver.
noop_setup() { :; }
disarm_case noseam "$BUILD/foh_dev_noms" noop_setup \
  "no match snapshot in this build"

# (b) THE SNAPSHOT WRITE FAILS. The snapshot's name is pre-created as a
#     DIRECTORY, so ss_save's rename cannot publish over it — a real, measured
#     class on this journal-less vfat (a full or remounted-read-only card),
#     not a hypothetical.
sim_dir_setup() { mkdir -p "$1/mlfk-match.sim"; }
disarm_case simfail "$BUILD/foh_dev_headless" sim_dir_setup \
  "snapshot publish (rename) failed"

# (c) THE HEADER PUBLISH FAILS, with the snapshot already on the card. The
#     orphan must be cleaned up: 160 KB nothing will ever read is not a
#     resume, it is litter on a small card.
# NON-EMPTY on purpose: ms_arm's first act is to remove the header, and
# remove() on an EMPTY directory succeeds (it is rmdir), which would clear the
# obstacle and make this case pass rather than fail. A non-empty directory
# survives that removal and the publish's rename then really cannot land.
hdr_dir_setup() { mkdir -p "$1/mlfk-match.hdr"; : > "$1/mlfk-match.hdr/keep"; }
disarm_case hdrfail "$BUILD/foh_dev_headless" hdr_dir_setup \
  "rename publish failed"
[ ! -e "$BUILD/d-hdrfail-persist/mlfk-match.sim" ] \
  || fail "[3] hdrfail: the orphaned snapshot was left on the card"
echo "  [3] OK: three ways to fail the write, three disarmed resumes"

# --- [4] the read path refuses, by name --------------------------------------
# A snapshot is a resume token and the card is the one thing that can change
# while the machine is off. Every refusal names its rule, integrity comes
# before meaning, and a refusal lands the player on the character select —
# never in a wrong game.
echo "=== [4] the read path refuses, by name"
GOOD=$BUILD/good-persist
rm -rf "$GOOD"; mkdir -p "$GOOD"
play "$BUILD/foh_dev_headless" "$GOOD" good "MLFK_HIBERNATE_AT=$AT" \
  || fail "[4] could not produce a good armed card"
made "$GOOD/mlfk-match.hdr" "$GOOD/mlfk-match.sim" "$GOOD/mlfk-persist.dat"

reseal() { # <file> — recompute the SUM over the four preceding lines
  node -e '
    const fs=require("fs"), c=require("crypto");
    const p=process.argv[1];
    const b=fs.readFileSync(p,"utf8").split("\n");
    const i=b.findIndex(l=>l.startsWith("SUM "));
    if(i<0) throw new Error("no SUM line");
    const body=b.slice(0,i).join("\n")+"\n";
    b[i]="SUM "+c.createHash("sha256").update(body).digest("hex");
    fs.writeFileSync(p,b.join("\n"));
  ' "$1"
}

refuse() { # <label> <mutate-fn> <expected substring> [expect-fresh-match]
  local label="$1" mut="$2" want="$3" fresh="${4:-}"
  local pd=$BUILD/r-$label-persist
  rm -rf "$pd"; mkdir -p "$pd"
  cp "$GOOD/mlfk-match.hdr" "$GOOD/mlfk-match.sim" "$GOOD/mlfk-persist.dat" "$pd/"
  "$mut" "$pd"
  local rc=0
  if [ -n "$fresh" ]; then
    resume "$BUILD/foh_dev_headless" "$pd" "r-$label" \
      || { relay_lines < "$BUILD/r-$label.err.txt" >&2
           fail "[4] $label: the boot died instead of playing a fresh match"; }
  else
    resume "$BUILD/foh_dev_headless" "$pd" "r-$label" || rc=$?
  fi
  grep -q -- "$want" "$BUILD/r-$label.err.txt" \
    || { relay_lines < "$BUILD/r-$label.err.txt" >&2
         fail "[4] $label: the refusal did not name its rule ('$want')"; }
  if [ -n "$fresh" ]; then
    # The header was believed; the SNAPSHOT was not. The set-up match in
    # memory is a legal fresh one, so the honest outcome is to say so and play
    # it — never to die holding the player's evening hostage to a bad file.
    grep -qxF "foh_dev: resumed screen=match" "$BUILD/r-$label.err.txt" \
      || fail "[4] $label: expected the header to be believed"
    first="$(grep -m1 '^F ' "$BUILD/r-$label.stream.txt" | awk '{print $2}')"
    [ "$first" = 1 ] \
      || fail "[4] $label: a match whose state could not be loaded started at
  frame $first instead of playing a fresh one from 1"
  else
    if [ "$rc" != 0 ]; then
      grep -qxF 'foh_dev: --bridge given but the flow never launched' \
        "$BUILD/r-$label.err.txt" \
        || { relay_lines < "$BUILD/r-$label.err.txt" >&2
             fail "[4] $label: the boot exited $rc for a reason other than
  never launching a match"; }
    fi
    # The screen line is printed AFTER the match arm decides (foh_dev.c says
    # why), so on a refusal it reads `css` and never `match` — the log states
    # where the boot landed, not where it hoped to.
    grep -qxF 'foh_dev: resumed screen=css' "$BUILD/r-$label.err.txt" \
      || { relay_lines < "$BUILD/r-$label.err.txt" >&2
           fail "[4] $label: the refusal did not land on the character select"; }
    ! grep -qxF 'foh_dev: resumed screen=match' "$BUILD/r-$label.err.txt" \
      || fail "[4] $label: the boot announced a match resume it then refused"
    ! grep -q 'match restored' "$BUILD/r-$label.err.txt" \
      || fail "[4] $label: a refused card was restored anyway"
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

m_nohdr() { rm -f "$1/mlfk-match.hdr"; }
m_trunc() { local f=$1/mlfk-match.hdr; local t; t=$(head -c 100 "$f"); printf '%s' "$t" > "$f"; }
m_sumbad() { perturb "$1/mlfk-match.hdr" -e 's/^SUM 0/SUM 1/' -e t -e 's/^SUM ./SUM 0/'; }
m_stage_unsealed() {
  # A field edited WITHOUT resealing: the SUM must catch it, and it must be
  # reported as CORRUPT and not as "from a different build". Integrity before
  # meaning — the .mlstage rule, inherited whole.
  perturb "$1/mlfk-match.hdr" 's/^STAGE 00$/STAGE 03/'
}
m_build_sealed() {
  perturb "$1/mlfk-match.hdr" -e 's/^BUILD 0/BUILD 1/' -e t -e 's/^BUILD ./BUILD 0/'
  reseal "$1/mlfk-match.hdr"
}
m_frame_sealed() {
  perturb "$1/mlfk-match.hdr" 's/^FRAME 0*[0-9]\{4\}$/FRAME 000000001799/'
  reseal "$1/mlfk-match.hdr"
}
m_nosim() { rm -f "$1/mlfk-match.sim"; }

refuse nohdr        m_nohdr        "no armed match (no header)"
refuse truncated    m_trunc        "match header is the wrong length"
refuse sum          m_sumbad       "match header SUM mismatch"
refuse unsealed     m_stage_unsealed "match header SUM mismatch"
refuse otherbuild   m_build_sealed "match snapshot is from a different build"
refuse framedisagree m_frame_sealed "disagree about the frame" fresh
refuse nosnapshot   m_nosim        "no such snapshot file" fresh
echo "  [4] OK: 7 refusals, each by name; integrity is checked before identity"

# --- [5] teeth ---------------------------------------------------------------
# CONTEXT.md, "Tooth": a deliberate perturbation asserting that leg [2] can
# still fail, and asserting the OUTCOME protected rather than a message.
echo "=== [5] teeth"
load_golden g01

# T1 — A DROPPED SNAPSHOT ROW. MLFK_SNAP_SKIP is #28's completeness tooth
# (sim_snapshot.c): the named row is read past and never applied, which is
# exactly the shape of "a field nobody knew had to be saved". The resumed
# match must then diverge from the uninterrupted one, and the spliced stream
# must be REJECTED by the unchanged verifier.
T1P=$BUILD/t1-persist; rm -rf "$T1P"; mkdir -p "$T1P"
play "$BUILD/foh_dev_headless" "$T1P" t1-b "MLFK_HIBERNATE_AT=$AT" \
  || fail "[5] T1: the hibernating run failed"
resume "$BUILD/foh_dev_headless" "$T1P" t1-c "MLFK_SNAP_SKIP=sim" \
  || { relay_lines < "$BUILD/t1-c.err.txt" >&2; fail "[5] T1: the boot died"; }
grep '^F ' "$BUILD/t1-c.stream.txt" > "$BUILD/t1.tail-c.txt"
! cmp -s "$BUILD/g01.tail-a.txt" "$BUILD/t1.tail-c.txt" \
  || fail "[5] T1 DID NOT BITE: dropping the whole `sim` row on the way back in
  changed NOTHING in the resumed stream, so leg [2]'s comparison is not
  actually judging the restored state"
{
  head -n "$AT" "$BUILD/g01.frames-a.txt"
  cat "$BUILD/t1.tail-c.txt"
  grep -E '^(RNG |SIM OK$)' "$BUILD/t1-c.stream.txt"
} > "$BUILD/t1.spliced.txt"
if node "$SIM/wrap-run.js" g01 "$BUILD/t1.spliced.txt" \
     "$BUILD/t1.spliced.json" "$MANIFEST" > /dev/null 2>&1 \
   && node "$VERIFY" "$BUILD/t1.spliced.json" "$FROZEN" > /dev/null 2>&1; then
  fail "[5] T1 DID NOT BITE: the unchanged verifier ACCEPTED a stream produced
  with a snapshot row dropped"
fi
echo "   T1 (a dropped snapshot row) bites"

# T2 — THE RESUME POINT. A perturbed foh_dev.c COPY whose match loop starts at
# 0 instead of the restored frame (the check-device-foh.sh T12 / check-
# hibernate.sh T3 pattern; the tree is never edited). That is precisely the
# regression the ticket names — a match that RESTARTS while claiming to have
# resumed — and leg [2](d) must catch it.
echo "   T2: perturbed foh_dev.c copy (the loop starts at 0 again)"
mkdir -p "$BUILD/t2/foh"
sed 's|^      for (long f = matchResumeFrom; f < matchMax; f++) {$|      for (long f = 0; f < matchMax; f++) { // T2|' \
  "$FOH/foh_dev.c" > "$BUILD/t2/foh/foh_dev.c"
made "$BUILD/t2/foh/foh_dev.c"
cmp -s "$BUILD/t2/foh/foh_dev.c" "$FOH/foh_dev.c" \
  && grammar_die "T2: the perturbation matched nothing — the resumed loop's
  start moved, so this tooth would be vacuous"
build_foh_headless "$BUILD/t2/foh/foh_dev.c" "$BUILD/foh_dev_t2" "-I$FOH" \
  || fail "[5] T2 build failed"
T2P=$BUILD/t2-persist; rm -rf "$T2P"; mkdir -p "$T2P"
play "$BUILD/foh_dev_t2" "$T2P" t2-b "MLFK_HIBERNATE_AT=$AT" \
  || fail "[5] T2: the hibernating run failed"
resume "$BUILD/foh_dev_t2" "$T2P" t2-c \
  || { relay_lines < "$BUILD/t2-c.err.txt" >&2; fail "[5] T2: the boot died"; }
t2first="$(grep -m1 '^F ' "$BUILD/t2-c.stream.txt" | awk '{print $2}')"
[ "$t2first" = 1 ] \
  || fail "[5] T2 DID NOT BITE: the perturbed build still started at frame
  $t2first — the perturbation did not do what it says"
grep '^F ' "$BUILD/t2-c.stream.txt" > "$BUILD/t2.tail-c.txt"
! cmp -s "$BUILD/g01.tail-a.txt" "$BUILD/t2.tail-c.txt" \
  || fail "[5] T2 DID NOT BITE: a match that replayed from frame 1 produced the
  same tail as one that continued from $AT"
echo "   T2 (a resumed match that restarts) bites"

# --- no-commit guard ----------------------------------------------------------
git_dirty_after="$(tree_fingerprint)" \
  || fail "could not fingerprint the working tree after the run"
[ "$git_dirty_before" = "$git_dirty_after" ] \
  || fail "this check modified the working tree — a check writes only into
  $BUILD and $TABLES, both ignored"

echo "MATCH RESUME OK"
