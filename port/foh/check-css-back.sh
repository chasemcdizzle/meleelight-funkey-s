#!/usr/bin/env bash
# port/foh/check-css-back.sh — HOST-ONLY tooth for punch-list A23, the
# owner-reported "clicking the back button does nothing in the top right of
# the vs. screen".
#
# WHAT IT PROVES
# --------------
# Filed symptom, verbatim: "clicking the back button does nothing in the top
# right of the vs. screen. I want it to behave like how melee does — holding
# the cursor OR holding the back button starts progressing a little red bar
# that fills below the back button, and when it fills it actually backs out."
#
# MEASURED against the upstream clone (2026-08-23) before any code was
# written, because HARD RULE 5 makes upstream the ground truth and the ticket
# turns on which way this fell:
#   * upstream DOES draw the bar — css.js:735-746, `bestHold` = max over the
#     four ports' bHold[], a red quad from x 1020 to 1020 + 6*bestHold along
#     y 119..125, guarded on `bestHold > 0`. So the bar is a FIDELITY gap this
#     port had not carried, NOT a deviation. It needs no D-number.
#   * upstream's wedge is hit-tested — css.js:358-363, `handPos.y < 160 &&
#     handPos.x > 920` — but as an INSTANT A-click calling changeGamemode(1)
#     outright. The 30-frame hold is upstream's B path only. Making the hand
#     ARM the same counter instead of firing instantly is therefore a real
#     deviation, and it is registered as D22 on the owner's words.
#
# So this check asserts the OWNER'S OBSERVABLES — a bar that fills, a back-out
# that happens, and a 29-frame hold that does not — through the REAL foh_tick
# and the REAL foh_render. The driver is port/foh/foh_cssback_witness.c.
#
# THE TWO NEGATIVE TESTS, because a witness that cannot fail proves nothing:
#   T1  a COPY of foh.c with the D22 arm reverted to upstream's `if (in->b)`
#       must make the witness FAIL, and fail on the CURSOR assertions
#       specifically — not for some unrelated reason.
#   T2  a COPY of foh_render.c with the bar block removed must (a) render a
#       COLD CSS frame BYTE-IDENTICAL to the real tree's, which is the whole
#       no-re-freeze argument stated as a measurement rather than as a hope,
#       and (b) render a 29-frame-hold frame that DIFFERS — so (a) is a fact
#       about the cold state and not about a comparison that never had teeth.
#
# HOST-ONLY. Needs no OPK and no device. It DOES need the renderer's art
# (assets/menu.img1), which it builds itself from the pipeline; that is the
# one thing check-css-token-rest.sh does not need and this does, because the
# subject here is pixels.
#
# `bash port/foh/check-css-back.sh` -> `CSS BACK CHECK OK`, exit 0.
#
# NOT A GATE. This is a task-level tooth.
set -euo pipefail

FOH=port/foh
GFX=port/gfx
BUILD=$FOH/build/css-back
DATA=$BUILD/data

fail() { echo "CSS BACK CHECK FAIL: $1" >&2; exit 1; }
grammar_die() { echo "CSS BACK CHECK FAIL: $1" >&2; exit 2; }
relay_lines() { sed 's/^/  | /'; }

made() { # every artifact must exist and be non-empty
  local f
  for f in "$@"; do
    [ -s "$f" ] || fail "expected artifact missing/empty: $f"
  done
}

# one_canonical <file> <needle> <anchored-regex> <ctx> — the PROCESS §3
# permissive-parse guard, carried verbatim from check-css-token-rest.sh:
# count the lines RESEMBLING the verdict and the lines MATCHING it exactly,
# and require both to be 1.
one_canonical() { # <file> <needle> <regex> <ctx>
  local f="$1" needle="$2" re="$3" ctx="$4" nres nexact
  nres="$(grep -cF "$needle" "$f")" || true
  nexact="$(grep -cE "$re" "$f")" || true
  if [ "$nres" != 1 ] || [ "$nexact" != 1 ]; then
    sed 's/^/  | /' "$f" >&2
    grammar_die "$ctx: $nres line(s) resemble '$needle' and $nexact match it
  exactly (want 1 and 1)."
  fi
}

# --- [0] run lock (mkdir-atomic, NO reclaim — the check-foh-flows pattern) ----
LOCK=$FOH/build/css-back.lock
mkdir -p "$FOH/build"
mkdir "$LOCK" 2>/dev/null \
  || fail "another run holds $LOCK (remove it only if you are sure no run is live)"
trap 'rmdir "$LOCK" 2>/dev/null || true' EXIT

# --- [0a] no-commit guard (check-css-token-rest.sh's, verbatim) --------------
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
mkdir -p "$BUILD" "$DATA"

# --- [1] grammar pins --------------------------------------------------------
# Both perturbations below rewrite exactly ONE line. If either line is not
# unique the perturbation is ambiguous, and a tooth that might be biting
# something else is worse than no tooth — so die loudly instead.
echo "=== [1] grammar pins (the two lines the negative tests perturb)"
made "$FOH/foh.c" "$FOH/foh_render.c" "$FOH/foh_cssback_witness.c"
D22_LINE='  if (in->b || (onBack && in->a)) {'
n="$(grep -cxF "$D22_LINE" "$FOH/foh.c")" || true
[ "$n" = 1 ] || grammar_die "foh.c has $n D22 counter-arm lines (want exactly
  1) — T1 cannot target the cursor path unambiguously"
BAR_LINE='    if (bHold > 0) {'
n="$(grep -cxF "$BAR_LINE" "$FOH/foh_render.c")" || true
[ "$n" = 1 ] || grammar_die "foh_render.c has $n bar-guard lines (want exactly
  1) — T2 cannot remove the bar unambiguously"
# The bar's `> 0` guard IS the no-re-freeze argument (upstream's own
# `bestHold > 0`, css.js:741). A zero-width draw would satisfy the eye and
# still repaint the cold frame, so the guard is pinned as a shape, not just
# as an anchor.
grep -qxF '      poly8(rz, q, 4, red, 256);' "$FOH/foh_render.c" \
  || grammar_die "the bar's fill call moved — re-read css_header before
  trusting anything below"
echo "   the D22 arm and the bar guard are both textually unique"

# --- [2] the renderer's art --------------------------------------------------
# The subject is pixels, so the witness renders real frames, and foh_render
# hard-fails without menu.img1. Built into this check's OWN dir; nothing
# outside $BUILD is written.
echo "=== [2] renderer art (pipeline 'assets' stage, this check's own dir)"
node pipeline/run.js --only assets --out "$DATA" > "$BUILD/assets.log" 2>&1 \
  || { relay_lines < "$BUILD/assets.log"
       fail "the pipeline's assets stage did not run (host prerequisite:
  MELEELIGHT_CLONE, see CLAUDE.md)"; }
made "$DATA/assets/menu.img1"
echo "   $DATA/assets/menu.img1 built"

# --- [3] the witness against the REAL tree -----------------------------------
# port/gfx/ctl_style.c is REQUIRED (foh.c's Controls screen calls into it and
# nowhere else); foh_render.c is here for the draws AND for foh_anim_tick,
# which foh_tick calls every tick; fdlibm supplies its sin/cos.
echo "=== [3] CSS back witness (real gestures + real frames through foh_tick)"
CFLAGS_COMMON=(-ffp-contract=off -Wall -Wextra -Werror -Iport/ryu -Iport/sim
               -Ioracle/qjs)
CB_SRCS=("$FOH/foh_cssback_witness.c" "$FOH/foh_font.c" "$GFX/gfx_glyphs.c" "$GFX/raster.c"
         "$GFX/img1.c" "$GFX/ctl_style.c" port/fdlibm/fdlibm.c)
CB_OK='^CSS BACK OK$'
mkdir -p "$BUILD/real"
cc -O2 "${CFLAGS_COMMON[@]}" -o "$BUILD/real/cb" \
  "$FOH/foh.c" "$FOH/foh_render.c" "${CB_SRCS[@]}" -lm \
  || fail "the witness did not build against the real tree"
made "$BUILD/real/cb"
MLFK_DATA_DIR="$DATA" "$BUILD/real/cb" "$BUILD/real" > "$BUILD/real/cb.out" 2>&1 \
  || { relay_lines < "$BUILD/real/cb.out"
       fail "the witness failed against the REAL tree"; }
relay_lines < "$BUILD/real/cb.out"
one_canonical "$BUILD/real/cb.out" 'CSS BACK OK' "$CB_OK" "witness verdict"
made "$BUILD/real/cold.fb" "$BUILD/real/hold29.fb"

# --- [4] T1: the cursor path deleted must fail the witness -------------------
# Put the counter arm back to upstream's `if (input[i][0].b)` (css.js:186) —
# the pre-A23 code — and require a FAILURE on the cursor assertions.
echo "=== [4] T1: without the D22 arm the cursor path must go dead"
mkdir -p "$BUILD/t1"
node -e '
  const fs = require("fs");
  const [src, dst, line, back] = process.argv.slice(1);
  const raw = fs.readFileSync(src, "utf8");
  const n = raw.split(line).length - 1;
  if (n !== 1) {
    console.error("T1: found " + n + " copies of the D22 arm (want exactly 1)");
    process.exit(1);
  }
  const out = raw.replace(line, back);
  if (out === raw) { console.error("T1: substitution was a no-op"); process.exit(1); }
  fs.writeFileSync(dst, out);
' "$FOH/foh.c" "$BUILD/t1/foh.c" "$D22_LINE" \
  '  if (in->b || (onBack && in->a && 0)) { // T1: hand path dead (pre-A23)' \
  || fail "T1: could not derive the pre-A23 foh.c copy"
cmp -s "$BUILD/t1/foh.c" "$FOH/foh.c" \
  && fail "T1: the perturbed copy is byte-identical to foh.c (dead tooth)"
# -Iport/foh so the COPY's quoted "foh.h" and "../gfx/..." resolve to the real
# tree — check-css-token-rest.sh's T1 copy-build, verbatim.
cc -O2 "${CFLAGS_COMMON[@]}" -Iport/foh -o "$BUILD/t1/cb" \
  "$BUILD/t1/foh.c" "$FOH/foh_render.c" "${CB_SRCS[@]}" -lm \
  || fail "T1: the pre-A23 copy did not build"
t1rc=0
MLFK_DATA_DIR="$DATA" "$BUILD/t1/cb" "$BUILD/t1" > "$BUILD/t1/cb.out" 2>&1 \
  || t1rc=$?
[ "$t1rc" = 1 ] \
  || fail "T1: the pre-A23 build exited rc $t1rc (want exactly 1 — rc 0 means
  the witness is BLIND to the defect it exists to guard)"
grep -qE "$CB_OK" "$BUILD/t1/cb.out" \
  && fail "T1: the pre-A23 build still printed the OK verdict"
# It must fail on the CURSOR back-out — the owner's actual symptom — and NOT
# on the B path, which A23 never touched.
grep -qF 'holding the cursor on the wedge 30 frames BACKS OUT to the menu' \
  "$BUILD/t1/cb.out" \
  || { relay_lines < "$BUILD/t1/cb.out"
       fail "T1: the cursor back-out assertion did not fail — the tooth is
  failing for some other reason and proves nothing"; }
grep -qE '^CSS BACK FAIL: 30 frames of B still backs out' "$BUILD/t1/cb.out" \
  && { relay_lines < "$BUILD/t1/cb.out"
       fail "T1: the B path broke in the perturbed build. The perturbation
  only removes the HAND's path into the counter, so this means the copy
  differs from foh.c by more than the D22 arm"; }
echo "   T1: reverting the arm reproduces 'clicking the back button does"
echo "       nothing' and fails the witness (rc 1)"

# --- [5] T2: the bar removed — cold frame IDENTICAL, held frame DIFFERENT ----
# This is the no-re-freeze argument, measured. The bar's `bHold > 0` guard
# means a cold CSS renders the bytes it rendered before A23 existed, so every
# frozen CSS shot (f01/f02/f05) stays valid without being touched.
echo "=== [5] T2: cold frame unchanged by the bar; held frame changed by it"
mkdir -p "$BUILD/t2"
node -e '
  const fs = require("fs");
  const [src, dst, guard] = process.argv.slice(1);
  const raw = fs.readFileSync(src, "utf8");
  const at = raw.indexOf(guard);
  if (at < 0 || raw.indexOf(guard, at + 1) >= 0) {
    console.error("T2: the bar guard is not unique"); process.exit(1);
  }
  // Replace only the guard CONDITION, so the block still compiles and every
  // other byte of css_header is untouched: a bar that can never draw.
  const out = raw.slice(0, at) + "    if (bHold < 0) { // T2: bar removed" +
              raw.slice(at + guard.length);
  if (out === raw) { console.error("T2: substitution was a no-op"); process.exit(1); }
  fs.writeFileSync(dst, out);
' "$FOH/foh_render.c" "$BUILD/t2/foh_render.c" "$BAR_LINE" \
  || fail "T2: could not derive the bar-less foh_render.c copy"
cmp -s "$BUILD/t2/foh_render.c" "$FOH/foh_render.c" \
  && fail "T2: the perturbed copy is byte-identical to foh_render.c (dead tooth)"
cc -O2 "${CFLAGS_COMMON[@]}" -Iport/foh -o "$BUILD/t2/cb" \
  "$FOH/foh.c" "$BUILD/t2/foh_render.c" "${CB_SRCS[@]}" -lm \
  || fail "T2: the bar-less copy did not build"
# The bar-less build FAILS the witness (it has no bar to grow) — expected, and
# not what T2 measures. What T2 measures is the two frames it wrote on the way.
t2rc=0
MLFK_DATA_DIR="$DATA" "$BUILD/t2/cb" "$BUILD/t2" > "$BUILD/t2/cb.out" 2>&1 \
  || t2rc=$?
[ "$t2rc" = 1 ] \
  || fail "T2: the bar-less build exited rc $t2rc (want exactly 1 — rc 0 means
  the witness cannot see a missing bar)"
made "$BUILD/t2/cold.fb" "$BUILD/t2/hold29.fb"
cmp -s "$BUILD/real/cold.fb" "$BUILD/t2/cold.fb" \
  || { fail "T2(a): the COLD CSS frame CHANGED when the bar was added. Every
  frozen CSS shot is taken in this state, so this means A23 forces a
  re-freeze — which the ticket forbids. Re-read the bar's 'bHold > 0' guard
  (upstream's own, css.js:741) before doing anything else."; }
cmp -s "$BUILD/real/hold29.fb" "$BUILD/t2/hold29.fb" \
  && { fail "T2(b): the 29-frame HELD frame is also identical, so T2(a) proved
  nothing — the two builds are not actually rendering differently"; }
echo "   T2: cold frames byte-identical (0 shots to re-freeze); held frames differ"

# --- [6] no-commit guard -----------------------------------------------------
git_dirty_after="$(tree_fingerprint)" \
  || fail "could not fingerprint the working tree after the run (fails CLOSED)"
[ "$git_dirty_before" = "$git_dirty_after" ] \
  || fail "this check modified the working tree (it must only write $BUILD,
  which is ignored) — before=$git_dirty_before after=$git_dirty_after"

echo "CSS BACK CHECK OK"
