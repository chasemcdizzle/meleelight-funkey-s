#!/usr/bin/env bash
# record-target.sh — record + freeze a TARGET golden's TWO streams into
# port/goldens-m4/ (M4 task 11; the record-m4.sh procedure for target
# mode, REUSING the oracle harness bytes VERBATIM BY PATH via
# run-target.js — HARD RULE 3: oracle/ is read-only; the gitignored
# oracle/harness/out/ scratch dir is the only write target outside
# port/goldens-m4/).
#
# Usage: bash port/goldens-m4/record-target.sh <golden-id> [--refreeze]
#
# Procedure (params ONLY from port/goldens-m4/manifest-target.json):
#   1. the golden's trace must exist — CRAFTED by the committed
#      deterministic generator gen-<id>-trace.js (no gen-trace.js
#      fallback; a missing generator is a loud death);
#   2. TWO fresh browser runs via run-target.js (the UNCHANGED oracle
#      harness init pipeline by path: seeded mulberry32 + virtual clock +
#      fdlibm shim, all default-on);
#   3. both-plane bit-identity is asserted by freeze-target.js;
#   4. check-target-quality.js asserts the TARGET quality contract on
#      run A (gameMode 5, playing, stocks 1, no DEAD, targetsDestroyed >=
#      minTargets, endTargetGame false, wantArticles => maxArticles > 0);
#   5. freeze-target.js writes <name>.sha256.json (player, M0 format) AND
#      <name>.target.sha256.json (target plane);
#   6. self-check: run A vs the frozen PLAYER stream (UNCHANGED
#      verify-stream.js) AND vs the frozen TARGET stream
#      (verify-target-stream.js).
#
# HARDENING (record-m4.sh posture inherited): strict per-key manifest
# grammar (no eval), duplicate rejection, trace === name+'.trace.json',
# a no-reclaim run lock on the fixed out/ paths, and rm-before-produce
# freshness on both run JSONs.
set -euo pipefail
cd "$(dirname "$0")/../.."

die() { echo "record-target.sh: $*" >&2; exit 1; }

ID="${1:?usage: bash port/goldens-m4/record-target.sh <golden-id> [--refreeze]}"
REFREEZE=""
if [ "$#" -gt 2 ]; then
  die "too many arguments (usage: record-target.sh <golden-id> [--refreeze])"
fi
if [ "$#" = 2 ]; then
  case "$2" in
    (--refreeze) REFREEZE="--refreeze" ;;
    (*) die "unknown argument '$2' (only --refreeze is accepted)" ;;
  esac
fi
case "$ID" in
  (*[!a-z0-9-]*|'') die "golden id '$ID' fails the whitelist [a-z0-9-]" ;;
esac

M4G=port/goldens-m4
HARNESS=oracle/harness

DIST="${MELEELIGHT_CLONE:-$HOME/.cache/meleelight-funkey-s/upstream}"
if [ ! -f "$DIST/dist/meleelight.html" ]; then
  die "no built upstream at $DIST — run oracle/build-upstream.sh"
fi

# SHARED strict manifest validation FIRST (review-94 H1, iter 96): the
# whole manifest must pass validate-target-manifest.js before any row is
# trusted (duplicate ids/names/traces, key order, domains, containment).
node "$M4G/validate-target-manifest.js" \
  || die "manifest-target.json failed the shared strict validator"

# Pull params from the target manifest — strict key=value line-parse with
# per-key anchored whitelists (the eval class is dead; PROCESS §3).
out="$(node -e '
  const fs = require("fs");
  const m = JSON.parse(fs.readFileSync("port/goldens-m4/manifest-target.json", "utf8"));
  const g = m.goldens.find((x) => x.id === process.argv[1] || x.name === process.argv[1]);
  if (!g) { console.error("unknown target golden: " + process.argv[1]); process.exit(1); }
  const emit = (k, v) => process.stdout.write(k + "=" + String(v) + "\n");
  emit("name", g.name); emit("trace", g.trace); emit("frames", g.frames);
  emit("seed", g.seed); emit("char", g.char); emit("tstage", g.tstage);
  emit("minTargets", g.minTargets); emit("wantArticles", g.wantArticles);
' "$ID")" || die "manifest read failed for '$ID'"
NAME= TRACE= FRAMES= SEED= CHAR= TSTAGE= MINTARGETS= WANTARTICLES=
n=0
while IFS= read -r line; do
  n=$((n + 1))
  v="${line#*=}"
  case "$line" in
    (name=*)
      [[ "$v" =~ ^t[0-9]{2}(-[a-z0-9]+)+$ ]] || die "manifest grammar — name '$v' fails ^t[0-9]{2}(-[a-z0-9]+)+"
      [ -z "$NAME" ] || die "manifest grammar — duplicate name line"; NAME="$v" ;;
    (trace=*)
      [[ "$v" =~ ^t[0-9]{2}[a-z0-9.-]*$ ]] || die "manifest grammar — trace '$v' fails the whitelist"
      [ -z "$TRACE" ] || die "manifest grammar — duplicate trace line"; TRACE="$v" ;;
    (frames=*)
      [[ "$v" =~ ^[1-9][0-9]{0,5}$ ]] || die "manifest grammar — frames '$v' is not a positive integer"
      [ -z "$FRAMES" ] || die "manifest grammar — duplicate frames line"; FRAMES="$v" ;;
    (seed=*)
      [[ "$v" =~ ^[0-9]{1,10}$ ]] || die "manifest grammar — seed '$v' is not a plain integer"
      [ -z "$SEED" ] || die "manifest grammar — duplicate seed line"; SEED="$v" ;;
    (char=*)
      [[ "$v" =~ ^[0-4]$ ]] || die "manifest grammar — char '$v' outside 0-4"
      [ -z "$CHAR" ] || die "manifest grammar — duplicate char line"; CHAR="$v" ;;
    (tstage=*)
      [[ "$v" =~ ^[0-9]$ ]] || die "manifest grammar — tstage '$v' outside 0-9"
      [ -z "$TSTAGE" ] || die "manifest grammar — duplicate tstage line"; TSTAGE="$v" ;;
    (minTargets=*)
      [[ "$v" =~ ^([1-9]|10)$ ]] || die "manifest grammar — minTargets '$v' outside 1-10"
      [ -z "$MINTARGETS" ] || die "manifest grammar — duplicate minTargets line"; MINTARGETS="$v" ;;
    (wantArticles=*)
      [[ "$v" =~ ^(true|false)$ ]] || die "manifest grammar — wantArticles '$v' is not a boolean"
      [ -z "$WANTARTICLES" ] || die "manifest grammar — duplicate wantArticles line"; WANTARTICLES="$v" ;;
    (*)
      die "manifest grammar — unrecognized param line '$line' (whitelist parse)" ;;
  esac
done <<< "$out"
[ "$n" = 8 ] || die "manifest grammar — got $n param lines, want exactly 8"
if [ "$TRACE" != "$NAME.trace.json" ]; then
  die "manifest grammar — trace '$TRACE' != name-derived '$NAME.trace.json'"
fi

# RUN LOCK (no-reclaim): the shared resource is the fixed out/target-* paths.
mkdir -p "$HARNESS/out"
LOCK="$HARNESS/out/record-target.lock"
if ! mkdir "$LOCK" 2>/dev/null; then
  echo "record-target.sh REFUSED: run lock $LOCK already exists (NO auto-reclaim)." >&2
  echo "  If you are sure no run is live: rm -rf '$LOCK'" >&2
  exit 1
fi
trap 'rm -rf "$LOCK"' EXIT

# CRAFTED-trace dispatch: every target trace comes from its OWN committed
# generator; a missing one is a loud death naming that exact path (never a
# gen-trace.js or sibling fallback that could fabricate a WRONG trace).
if [ ! -f "$M4G/$TRACE" ]; then
  GEN="$M4G/gen-${NAME%%-*}-trace.js"
  [ -f "$GEN" ] || die "trace $M4G/$TRACE is missing and its committed generator $GEN does NOT exist — commit it first (no fallback)"
  echo "record-target.sh: generating crafted trace via $GEN"
  node "$GEN" "$M4G/$TRACE"
fi
test -s "$M4G/$TRACE" || die "trace $M4G/$TRACE missing or empty"

WANT_ART_FLAG=0
[ "$WANTARTICLES" = "true" ] && WANT_ART_FLAG=1

RUN=(node "$M4G/run-target.js" --dist "$DIST" --trace "$M4G/$TRACE"
     --frames "$FRAMES" --seed "$SEED" --char "$CHAR" --tstage "$TSTAGE")

# FRESHNESS (rm-before-produce): stale A/B run JSONs can never masquerade
# as the two fresh independent browser runs.
rm -f "$HARNESS/out/target-$ID-a.json" "$HARNESS/out/target-$ID-b.json"

echo "record-target.sh: $NAME — fresh run A"
"${RUN[@]}" --out "$HARNESS/out/target-$ID-a.json"
test -s "$HARNESS/out/target-$ID-a.json" || die "run A left no fresh run JSON"
echo "record-target.sh: $NAME — fresh run B"
"${RUN[@]}" --out "$HARNESS/out/target-$ID-b.json"
test -s "$HARNESS/out/target-$ID-b.json" || die "run B left no fresh run JSON"

echo "record-target.sh: gameplay-quality contract (mechanical)"
node "$M4G/check-target-quality.js" "$HARNESS/out/target-$ID-a.json" \
  "$MINTARGETS" "$WANT_ART_FLAG"

echo "record-target.sh: freezing BOTH streams into $M4G/"
node "$M4G/freeze-target.js" "$ID" "$HARNESS/out/target-$ID-a.json" \
  "$HARNESS/out/target-$ID-b.json" $REFREEZE

echo "record-target.sh: self-check — run A vs the frozen PLAYER stream"
node "$HARNESS/verify-stream.js" "$HARNESS/out/target-$ID-a.json" \
  "$M4G/$NAME.sha256.json"
echo "record-target.sh: self-check — run A vs the frozen TARGET stream"
node "$M4G/verify-target-stream.js" "$HARNESS/out/target-$ID-a.json" \
  "$M4G/$NAME.target.sha256.json"

echo "RECORDED $ID -> $M4G/$NAME.sha256.json + $M4G/$NAME.target.sha256.json"
