#!/usr/bin/env bash
# port/foh/check-face-lint.sh — HOST-ONLY. Spec #20, ticket #24:
# THE FACE-DOMAIN LINT OVER THE RENDERER'S NAME TABLES.
#
# WHY A THIRD CHECK, WHEN #21 ALREADY LANDED
# ------------------------------------------
# A string is only drawable if every character in it is in the face
# (CONTEXT.md, "Face domain"). Ticket #21 built two defences and they are both
# RUNTIME defences: the witness now renders every frame it simulates, so any
# string a test reaches is drawn and judged, and a miss is a loud gfx_fatal in
# a check build / a visible placeholder box in the product.
#
# Neither of those reaches a NAME TABLE. Tool names, wall types, damage types,
# pause rows, character names, the fourteen credits — that data reaches the
# renderer with no call site of its own, so the runtime check only ever sees
# the ONE row a test happened to draw, and no test visits all ten tool names
# and all four damage types. This check reads the tables instead of waiting to
# be shown one.
#
# WHAT IT PROVES
#   [1] the domain is READ FROM THE FONT. port/foh/face_lint_probe.c links the
#       real foh_font.c and dumps `foh_face_domain` for both faces. Nothing
#       here restates a list of characters — restating a constant instead of
#       reading it is how four separate device checks went stale in one week
#       in this project.
#   [2] every name table that reaches the renderer is checked against it, and
#       an out-of-domain character in any of them fails NAMING the entry
#       (file, line, table, index, the string, the offending byte).
#   [3] the census is CLOSED IN BOTH DIRECTIONS: a new string-bearing table
#       that nobody classified fails, and a registry row whose table has
#       vanished fails. That is the anti-staleness half.
#   [4] the transform is pinned. Two drawn planes are folded to upper case at
#       the DRAW site (foh_upper); the lint models that, so leg [2] pins
#       foh_upper's body textually — if the fold changes, the model must.
#   [5] TEETH. Five perturbations, each on a COPY, each required to fail for
#       ITS OWN reason: a glyph removed from the font (which proves the domain
#       is genuinely read), a lowercase name in a tool table, an unregistered
#       new table, a deleted registered table, and a Controls-screen label
#       that only exists for one (style, Mod) combination.
#
# WHAT IT DOES NOT COVER is stated by the lint itself, on every run, and at
# length in port/foh/face-lint.js's header: a source scan cannot see through
# snprintf, and several drawn strings are assembled from a table lookup plus a
# number. This is a NET UNDER the runtime check, never a replacement for it.
#
# THE HOLE IN FACE 1 IS DELIBERATE. face 1 has no '?', and
# check-foh-flows.sh's finish-banner tooth is hostage to that. Nothing here
# may be "fixed" by widening face 1: the lint's own failure text says so, and
# leg [2] pins the two faces' shapes so a widening cannot pass unnoticed.
#
# HOST-ONLY. No device, no OPK, no ADB, no node pipeline stages — just cc and
# node. `bash port/foh/check-face-lint.sh` -> `FACE LINT CHECK OK`, exit 0.
#
# NOT A GATE. This is a task-level check with teeth.
set -euo pipefail

FOH=port/foh
GFX=port/gfx
BUILD=$FOH/build/facelint

fail() { echo "FACE LINT CHECK FAIL: $1" >&2; exit 1; }
grammar_die() { echo "FACE LINT CHECK FAIL: $1" >&2; exit 2; }
relay_lines() { sed 's/^/  | /'; }
made() {
  local f
  for f in "$@"; do
    [ -s "$f" ] || fail "expected artifact missing/empty: $f"
  done
}

# --- [0] run lock (mkdir-atomic, NO reclaim — check-legibility.sh's pattern) --
LOCK=$FOH/build/facelint.lock
mkdir -p "$FOH/build"
mkdir "$LOCK" 2>/dev/null \
  || fail "another run holds $LOCK (remove it only if you are sure no run is live)"
trap 'rmdir "$LOCK" 2>/dev/null || true' EXIT

# --- [0a] no-commit guard ----------------------------------------------------
# Carried verbatim from check-legibility.sh, including both of its recorded
# corrections: the `-f` test (a wholly-untracked nested checkout is reported as
# a bare DIRECTORY) and the name list (so a directory appearing or
# disappearing stays visible even though its bytes are not walked).
tree_fingerprint() {
  local status diff files hashes f
  status="$(git status --porcelain)" || return 1
  diff="$(git diff)" || return 1
  files="$(git ls-files -o --exclude-standard)" || return 1
  files="$(printf '%s\n' "$files" | grep -v '^\.tokensave/' || true)"
  hashes=""
  while IFS= read -r f; do
    [ -n "$f" ] || continue
    [ -f "$f" ] || continue
    hashes="$hashes$(shasum -a 256 "$f")" || return 1
    hashes="$hashes"$'\n'
  done <<< "$files"
  printf '%s\n%s\n%s\n%s\n' "$status" "$diff" "$files" "$hashes" \
    | shasum -a 256 | cut -d' ' -f1
}
git_dirty_before="$(tree_fingerprint)" \
  || fail "could not fingerprint the working tree (guard fails CLOSED)"
[[ "$git_dirty_before" =~ ^[0-9a-f]{64}$ ]] \
  || fail "tree fingerprint is not a sha256 (guard fails CLOSED)"

rm -rf "$BUILD"
mkdir -p "$BUILD"
made "$FOH/foh_font.c" "$FOH/face_lint_probe.c" "$FOH/face-lint.js"

# --- [1] the grammar pins ----------------------------------------------------
# Two things the lint MODELS rather than executes. A model is only safe while
# the thing it models still looks like it did, so both are pinned to exactly
# one occurrence — the check-legibility.sh discipline.
echo "=== [1] grammar pins (what the lint models rather than executes)"
pin() { # <file> <line> <what it is>
  local n
  n="$(grep -cxF "$2" "$1")" || true
  [ "$n" = 1 ] || grammar_die "$1 has $n copies of
  |$2|
  (want exactly 1) — $3."
}
# (a) THE UPPERCASE FOLD. Two drawn planes (the credits, the Controls style
#     row) are authored mixed-case and folded at the DRAW site, so the lint
#     checks the folded form. If foh_upper stops being a plain a-z fold — if
#     it ever mapped something else, or stopped mapping — the lint's model
#     would be checking a string the renderer never draws.
pin "$FOH/foh_render.c" 'static void foh_upper(char *p) {' \
    'the draw-site uppercase fold the lint models (xform: upper in
  port/foh/face-lint.js). Re-read it and re-sync the model'
pin "$FOH/foh_render.c" "    if (*p >= 'a' && *p <= 'z') *p = (char)(*p - 'a' + 'A');" \
    'foh_upper`s body — the lint models exactly this fold (JS toUpperCase over
  ASCII) and nothing else'
# (b) THE TWO DOMAIN ACCESSORS. The probe asks the font through these; if
#     either were to grow a second definition the probe could bind the wrong
#     one and dump a domain no glyph table stands behind.
pin "$FOH/foh_font.c" 'int foh_face_domain(int face, char *out, int cap) {' \
    'the font`s own domain enumerator — the probe`s only source of truth'
pin "$FOH/foh_font.c" 'bool foh_face_has(char c, int face) {' \
    'the font`s membership predicate'
echo "   4 modelled lines still read exactly as the lint assumes"

# --- [2] the probe: the domain, straight out of foh_font.c -------------------
# The probe never DRAWS. This is a check build, so foh_font.c's loud arm is
# live and a drawn miss would kill the probe instead of reporting; it asks
# rather than tries.
echo "=== [2] the probe (foh_face_domain + the name sets that are not arrays)"
CFLAGS_COMMON=(-ffp-contract=off -Wall -Wextra -Werror -Iport/ryu -Iport/sim
               -Ioracle/qjs)
build_probe() { # <outdir> [extra -I ...]
  local out="$1"; shift
  mkdir -p "$out"
  cc -O1 "${CFLAGS_COMMON[@]}" "$@" -I"$FOH" -o "$out/probe" \
    "$FOH/face_lint_probe.c" "$FOH/foh_font.c" "$GFX/raster.c" \
    "$GFX/ctl_style.c" port/fdlibm/fdlibm.c -lm
}
build_probe "$BUILD/real" || fail "the face-domain probe did not build"
"$BUILD/real/probe" > "$BUILD/real/probe.txt" 2> "$BUILD/real/probe.err" \
  || { relay_lines < "$BUILD/real/probe.err"
       fail "the face-domain probe did not run"; }
made "$BUILD/real/probe.txt"
grep -qx 'END' "$BUILD/real/probe.txt" \
  || fail "the probe dump has no END marker — it is truncated"
# The two faces' SHAPE, asserted here and not merely printed. face 1 must
# carry NO lowercase and NO '?' (foh_font.c's note: check-foh-flows.sh's
# finish-banner tooth lives in that hole and a widening would defuse it);
# face 2 must carry '?' (that is what makes the tooth's face-2 escape real).
node -e '
  const fs = require("fs");
  const lines = fs.readFileSync(process.argv[1], "utf8").split("\n");
  const dom = {};
  for (const ln of lines) {
    const m = /^DOMAIN ([12]) (\d+)((?: [0-9a-f]{2})+)$/.exec(ln);
    if (!m) continue;
    dom[m[1]] = m[3].trim().split(" ").map((h) => String.fromCharCode(parseInt(h, 16)));
  }
  if (!dom["1"] || !dom["2"]) { console.error("missing a DOMAIN line"); process.exit(1); }
  const low = (a) => a.filter((c) => c >= "a" && c <= "z");
  if (low(dom["1"]).length || low(dom["2"]).length) {
    console.error("a face carries LOWERCASE glyphs: " +
      JSON.stringify([low(dom["1"]), low(dom["2"])]));
    process.exit(1);
  }
  if (dom["1"].includes("?")) {
    console.error("face 1 has grown a question-mark glyph. check-foh-flows.sh " +
      "draws \"COMPLETE?\" through face 1 to prove the loud missing-glyph " +
      "failure, and this defuses that tooth");
    process.exit(1);
  }
  if (!dom["2"].includes("?")) {
    console.error("face 2 has LOST its question-mark glyph. Menu strings that " +
      "need one render in face 2 precisely because face 1 must not have it");
    process.exit(1);
  }
  const only1 = dom["1"].filter((c) => !dom["2"].includes(c));
  console.log("   face1=" + dom["1"].length + " face2=" + dom["2"].length +
    " glyphs; no lowercase in either; question mark in face 2 only" +
    (only1.length ? "; face1-only=" + JSON.stringify(only1) : ""));
' "$BUILD/real/probe.txt" || fail "the face domain is not what the tree believes"
nrt="$(grep -c '^RUNTIME ' "$BUILD/real/probe.txt")" || true
[ "$nrt" -ge 58 ] \
  || fail "the probe enumerated only $nrt runtime name-set rows (want >= 58:
  9 Controls labels x 3 styles x 2 Mod shoulders, plus the 4 style names) —
  a probe that walks fewer combinations checks fewer strings"
echo "   $nrt runtime-enumerated strings (the name sets that are not arrays)"

# --- [3] THE LINT against the REAL tree --------------------------------------
echo "=== [3] the lint over every name table in the committed tree"
run_lint() { # <outdir> <probe-dump> [--overlay dir]
  local out="$1"; shift
  local dump="$1"; shift
  mkdir -p "$out"
  local rc=0
  node "$FOH/face-lint.js" --probe "$dump" "$@" \
    > "$out/lint.out" 2> "$out/lint.err" || rc=$?
  echo "$rc" > "$out/lint.rc"
  return 0
}
run_lint "$BUILD/real" "$BUILD/real/probe.txt"
rc="$(cat "$BUILD/real/lint.rc")"
if [ "$rc" != 0 ]; then
  relay_lines < "$BUILD/real/lint.err"
  relay_lines < "$BUILD/real/lint.out"
  fail "the lint failed against the REAL tree (rc $rc)"
fi
relay_lines < "$BUILD/real/lint.out"
grep -qx 'FACE LINT PASS' "$BUILD/real/lint.out" \
  || grammar_die "the lint exited 0 without printing its verdict line"

# --- TEETH -------------------------------------------------------------------
# Every tooth perturbs a COPY under $BUILD; the committed tree is never
# written. Each is required to fail for ITS OWN reason (a tooth that fails for
# any reason proves only that something broke), and the two source teeth are
# additionally required NOT to disturb the other legs' verdicts.
overlay_of() { # <dir> <path-under-repo> <from> <to>  — exact, unique, non-no-op
  local dir="$1" rel="$2" from="$3" to="$4"
  mkdir -p "$BUILD/$dir/$(dirname "$rel")"
  node -e '
    const fs = require("fs");
    const [src, dst, from, to] = process.argv.slice(1);
    const raw = fs.readFileSync(src, "utf8");
    const n = raw.split(from).length - 1;
    if (n !== 1) { console.error("overlay: " + n + " copies of |" + from + "| (want 1)"); process.exit(1); }
    if (from === to) { console.error("overlay: no-op substitution"); process.exit(1); }
    fs.writeFileSync(dst, raw.replace(from, to));
  ' "$rel" "$BUILD/$dir/$rel" "$from" "$to" \
    || fail "$dir: could not derive the perturbed $rel"
  cmp -s "$BUILD/$dir/$rel" "$rel" \
    && fail "$dir: the perturbed copy is byte-identical to $rel (dead tooth)"
  return 0
}
bites() { # <dir> <substring the lint's FAIL output must carry> <why it matters>
  local dir="$1" want="$2" why="$3"
  local rc; rc="$(cat "$BUILD/$dir/lint.rc")"
  [ "$rc" != 0 ] \
    || { relay_lines < "$BUILD/$dir/lint.out"
         fail "$dir: the lint PASSED the perturbed tree — $why"; }
  grep -qF "$want" "$BUILD/$dir/lint.err" \
    || { relay_lines < "$BUILD/$dir/lint.err"
         fail "$dir: the lint failed, but not with |$want| — the tooth is
  biting for some other reason and proves nothing"; }
  return 0
}
nfails() { grep -c '^FACE LINT FAIL: ' "$BUILD/$1/lint.err" || true; }

# --- [T1] the DOMAIN is read from the font, not restated --------------------
# The strongest of the five: remove ONE glyph from foh_font.c's face-1 table
# and rebuild the probe from the copy. Nothing in the lint changes. If the
# domain were restated anywhere, the tables that use '&' would still pass.
echo "=== [T1] a glyph removed from foh_font.c must make its users fail"
overlay_of t1-noamp "$FOH/foh_font.c" \
  "    {'&', {0x0C, 0x12, 0x12, 0x0C, 0x1A, 0x12, 0x0D}}," \
  "    // T1: face 1's '&' removed"
mkdir -p "$BUILD/t1-noamp/build"
cc -O1 "${CFLAGS_COMMON[@]}" -I"$FOH" -o "$BUILD/t1-noamp/build/probe" \
  "$FOH/face_lint_probe.c" "$BUILD/t1-noamp/$FOH/foh_font.c" "$GFX/raster.c" \
  "$GFX/ctl_style.c" port/fdlibm/fdlibm.c -lm \
  || fail "T1: the probe did not build against the perturbed font"
"$BUILD/t1-noamp/build/probe" > "$BUILD/t1-noamp/probe.txt" 2>&1 \
  || fail "T1: the perturbed probe did not run"
run_lint "$BUILD/t1-noamp" "$BUILD/t1-noamp/probe.txt"
bites t1-noamp "'&' (0x26) is not in face1" \
  "the lint is not reading the domain out of foh_font.c — a restated copy
  somewhere would pass exactly like this"
# and it must bite on the CREDITS, which is the table that carries the '&'
# through the uppercase fold. That is the leg the array scan would have
# missed entirely (foh_credits is an array of STRUCTS).
grep -qF 'port/foh/foh.c' "$BUILD/t1-noamp/lint.err" \
  || { relay_lines < "$BUILD/t1-noamp/lint.err"
       fail "T1: the credits table did not fail — the struct-array scan is
  not reaching it, and the fourteen credits are the longest drawn strings
  in the tree"; }
echo "   T1: removing face 1's '&' fails the tables that draw one ($(nfails t1-noamp) entries named)"

# --- [T2] a lowercase entry in a name table ---------------------------------
# The shipped defect, in its exact shape: a name table entry with a lowercase
# letter. No test draws DELETE; the runtime check would never see it.
echo "=== [T2] a lowercase tool name must fail, naming the entry"
overlay_of t2-lowercase "$FOH/foh_tbuild.c" \
  '"DELETE", "SCALE", "DRAW MODE"};' \
  '"Delete", "SCALE", "DRAW MODE"};'
run_lint "$BUILD/t2-lowercase" "$BUILD/real/probe.txt" \
  --overlay "$BUILD/t2-lowercase"
bites t2-lowercase 'kToolNames[7]' \
  "a lowercase name-table entry is the exact defect this ticket exists for"
bites t2-lowercase "'e' (0x65) is not in face1" \
  "the failure must name the OFFENDING CHARACTER, not merely the table"
[ "$(nfails t2-lowercase)" = 1 ] \
  || { relay_lines < "$BUILD/t2-lowercase/lint.err"
       fail "T2: $(nfails t2-lowercase) failures (want exactly 1) — this
  perturbation touches one entry, so anything else failing means the copy
  differs from foh_tbuild.c by more than that entry"; }
echo "   T2: the lowercase entry is named (file, line, table, index, byte)"

# --- [T3] a NEW table nobody classified -------------------------------------
# The anti-staleness half, forward direction: a name table can be added to a
# drawn TU without touching this lint, and that must not be silent.
echo "=== [T3] an unregistered new table must fail"
overlay_of t3-newtable "$FOH/foh_render.c" \
  'static const char *kLCancelNames[3] = {' \
  'static const char *kT3NewNames[2] = {"one", "two"};
static const char *kLCancelNames[3] = {'
run_lint "$BUILD/t3-newtable" "$BUILD/real/probe.txt" \
  --overlay "$BUILD/t3-newtable"
bites t3-newtable 'kT3NewNames' \
  "a new name table must not arrive unclassified — that is how the census
  goes stale"
bites t3-newtable 'has never been told about' \
  "the refusal must say WHAT to do about it"
echo "   T3: the unclassified table is refused by name"

# --- [T4] a registered table that has VANISHED ------------------------------
# The anti-staleness half, reverse direction: a registry describing tables
# that are not there is exactly how a check goes quietly vacuous.
echo "=== [T4] a registry row whose table is gone must fail"
overlay_of t4-gonetable "$FOH/foh_tbuild.c" \
  'static const char *const kWallTypeNames[FOH_TB_WALLTYPES] = {' \
  'static const char *const kWallTypeNamesT4[FOH_TB_WALLTYPES] = {'
run_lint "$BUILD/t4-gonetable" "$BUILD/real/probe.txt" \
  --overlay "$BUILD/t4-gonetable"
bites t4-gonetable 'kWallTypeNames#0' \
  "the registry must not be allowed to describe a table that is not there"
bites t4-gonetable 'quietly vacuous' \
  "the refusal must say why a stale registry matters"
echo "   T4: the vanished table is refused by name"

# --- [T5] the label that only exists for ONE (style, Mod) combination -------
# foh_ctl_labels' rows 5 and 6 are chosen by a conditional, so four of its
# strings exist only for particular inputs. This tooth perturbs the arm that
# is reached by BOX + Mod-on-L alone — a source scan of array initializers
# would never see it, and only the probe's six-combination walk does.
echo "=== [T5] a conditional Controls label must be walked, not sampled"
overlay_of t5-condlabel "$FOH/foh_ctl_labels.h" \
  '  out[5] = boxMod ? (modOnR ? "SHIELD" : "MOD / TILT") : "SHIELD";' \
  '  out[5] = boxMod ? (modOnR ? "SHIELD" : "Mod / tilt") : "SHIELD";'
# The probe's `#include "foh_ctl_labels.h"` is QUOTED, so it resolves next to
# the INCLUDING FILE before it looks at any -I. The perturbed header is only
# reachable if the probe itself is compiled from the overlay directory; the
# cmp below is what proves the overlay actually took.
cp "$FOH/face_lint_probe.c" "$BUILD/t5-condlabel/$FOH/face_lint_probe.c"
cc -O1 "${CFLAGS_COMMON[@]}" -I"$FOH" \
  -o "$BUILD/t5-condlabel/probe" \
  "$BUILD/t5-condlabel/$FOH/face_lint_probe.c" "$FOH/foh_font.c" \
  "$GFX/raster.c" "$GFX/ctl_style.c" port/fdlibm/fdlibm.c -lm \
  || fail "T5: the probe did not build against the perturbed label header"
"$BUILD/t5-condlabel/probe" > "$BUILD/t5-condlabel/probe.txt" 2>&1 \
  || fail "T5: the perturbed probe did not run"
cmp -s "$BUILD/t5-condlabel/probe.txt" "$BUILD/real/probe.txt" \
  && fail "T5: the perturbed probe dumped the SAME rows as the real one — the
  overlay include did not take, so this tooth proves nothing"
run_lint "$BUILD/t5-condlabel" "$BUILD/t5-condlabel/probe.txt"
bites t5-condlabel 'foh_ctl_labels' \
  "the conditional Controls labels must be walked over every (style, Mod)
  combination, not sampled at the default one"
bites t5-condlabel "'o' (0x6f) is not in face1" \
  "the failure must name the offending character"
echo "   T5: the BOX + Mod-on-L only label is reached and named"

# --- [4] the lint REFUSES to run without a domain ---------------------------
# The acceptance criterion "read from the font, never restated" has a second
# half: if the dump is missing or truncated, the lint must STOP, not fall back
# to a built-in list. There is no built-in list to fall back to, and this is
# what says so mechanically.
echo "=== [4] no domain, no verdict"
: > "$BUILD/empty.txt"
run_lint "$BUILD/nodomain" "$BUILD/empty.txt"
[ "$(cat "$BUILD/nodomain/lint.rc")" = 2 ] \
  || { relay_lines < "$BUILD/nodomain/lint.err"
       fail "the lint ran without a domain dump (rc $(cat "$BUILD/nodomain/lint.rc"))
  — it must refuse rather than assume a face"; }
head -c 200 "$BUILD/real/probe.txt" | grep -v '^END$' > "$BUILD/trunc.txt" || true
run_lint "$BUILD/trunc" "$BUILD/trunc.txt"
[ "$(cat "$BUILD/trunc/lint.rc")" = 2 ] \
  || fail "the lint accepted a TRUNCATED domain dump — a short domain would
  look like a small face and pass everything it could not see"
grep -qF 'truncated' "$BUILD/trunc/lint.err" \
  || fail "the truncated-dump refusal does not say what is wrong"
echo "   an absent domain and a truncated domain are both refusals (rc 2)"

# --- [5] no-commit guard -----------------------------------------------------
git_dirty_after="$(tree_fingerprint)" \
  || fail "could not fingerprint the working tree after the run (fails CLOSED)"
[ "$git_dirty_before" = "$git_dirty_after" ] \
  || fail "this check modified the working tree (it must only write $BUILD,
  which is ignored) — before=$git_dirty_before after=$git_dirty_after"

echo "FACE LINT CHECK OK"
