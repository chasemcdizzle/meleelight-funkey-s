#!/usr/bin/env bash
# port/foh/check-css-backsel.sh — HOST-ONLY tooth for punch-list A43, the
# owner-reported "back out of the CSS and you come back in as falcon".
#
# WHAT IT PROVES, and what the bug turned out to be
# ------------------------------------------------
# Filed symptom (owner): "if you go to the CSS back button while selected as
# any other character except for falcon, and then come back in to the game, you
# are selected as falcon... I think the reason it picks falcon is because when
# you go to the back button it lets go of the p1 pin and goes to the 'nearest'
# character."
#
# MEASURED, and the mechanism is NOT a token re-homed from pixels — every rest
# slot in `foh_css_token_pos` already homes on `cssChar[k]` (D21's rule). It is
# a LEAK with a geometry amplifier:
#   * one B press in the roster band BOTH retrieves your token (css.js:209-215)
#     and arms the 30-frame back counter, upstream's deliberate overlap
#     (MENU-SPEC §2.11), so "press B to go back" leaves the CSS CARRYING;
#   * nothing released it. Upstream clears its grab in exactly two arms — the
#     A-drop (css.js:228-232) and the leave-band drop (css.js:341-347) — and
#     css.js:186-194's `changeGamemode(1)` is neither; `changeGamemode` case 2
#     is `drawCSSInit()` alone (main.js:571) and menu.js:106 adds only
#     `positionPlayersInCSS()`, which moves SIM players, not tokens. Carried
#     verbatim, so re-entry found the token still on the hand;
#   * the hover arm then re-selects LIVE from the hand (css.js:222-226, the one
#     site that writes both planes), and D22 puts the BACK wedge directly above
#     roster cell 4 — so the next walk to BACK dragged the token across FALCON
#     and committed it. Always falcon, because falcon is the cell under the
#     wedge. Both planes really did change; the selection was not merely
#     mis-drawn.
# DEVIATION D35 (foh.c's `css_back`) releases every token on the back-out and
# re-homes it on the character its port chose. The SELECTION plane is never
# written there.
#
# The tooth is port/foh/foh_cssbacksel_witness.c, which drives the REAL
# foh_tick through the REAL gestures — grab, hover, drop, B-hold back, wedge
# hold, re-entry — with no character field ever hand-poked, and asserts the
# owner's own observable: after backing out and coming back, both planes AND
# the token's drawn cell read the character he picked.
#
# TWO ORTHOGONAL NEGATIVE TESTS, each of which must fail ALONE (the
# check-rebind.sh T2/T3 standard: one makes the PLUMBING lie, the other the
# DISPLAY):
#   T1 — delete only D35's carry release. The leak returns and the walk to the
#        wedge re-selects falcon: the SELECTION assertions must fail.
#   T2 — delete only D35's rest re-home. The plumbing is fine, so no selection
#        assertion may fail; the leave-band token stays drawn on falco's cell
#        and only the TOKEN CELL assertion must fail.
#
# HOST-ONLY and data-free by construction: nothing here needs the pipeline
# tables, an OPK, or the device. `bash port/foh/check-css-backsel.sh` ->
# `CSS BACK SELECT CHECK OK`, exit 0.
#
# NOT A GATE. This is a task-level tooth.
set -euo pipefail

FOH=port/foh
GFX=port/gfx
BUILD=$FOH/build/css-backsel

fail() { echo "CSS BACK SELECT CHECK FAIL: $1" >&2; exit 1; }
grammar_die() { echo "CSS BACK SELECT CHECK FAIL: $1" >&2; exit 2; }
relay_lines() { sed 's/^/  | /'; }

made() { # every artifact must exist and be non-empty
  local f
  for f in "$@"; do
    [ -s "$f" ] || fail "expected artifact missing/empty: $f"
  done
}

# one_canonical <file> <needle> <anchored-regex> <ctx> — the PROCESS §3
# permissive-parse guard, carried from check-css-token-rest.sh: count the lines
# RESEMBLING the verdict and the lines MATCHING it exactly, and require both to
# be 1. A resemblance that is not an exact match is corruption, and two exact
# matches mean the artifact carries more than one run.
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
LOCK=$FOH/build/css-backsel.lock
mkdir -p "$FOH/build"
mkdir "$LOCK" 2>/dev/null \
  || fail "another run holds $LOCK (remove it only if you are sure no run is live)"
trap 'rmdir "$LOCK" 2>/dev/null || true' EXIT

# --- [0a] no-commit guard ----------------------------------------------------
# A check must never write a tracked file. check-css-token-rest.sh's guard,
# carried verbatim including its `-f` test and name list (git reports a wholly
# untracked NESTED CHECKOUT as a bare directory, and feeding that to shasum
# would make the guard die on a tree it should simply have hashed).
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

# --- [1] the grammar pins ----------------------------------------------------
# D35 is TWO independent statements and the two negative tests below perturb
# one each, so each must be textually unique or a perturbation could silently
# hit the wrong one (or nothing).
echo "=== [1] grammar pins (D35's two statements, one per negative test)"
made "$FOH/foh.c" "$FOH/foh_cssbacksel_witness.c"
D35_CARRY='    s->cssCarry = -1; // D35: release the grab that upstream leaks across'
D35_REST='  for (int k = 0; k < 2; k++) s->cssTokenRest[k] = 0; // D35: re-home on cssChar'
n="$(grep -cxF "$D35_CARRY" "$FOH/foh.c")" || true
[ "$n" = 1 ] || grammar_die "foh.c has $n D35 carry-release lines (want exactly
  1) — T1 cannot target the release unambiguously"
n="$(grep -cxF "$D35_REST" "$FOH/foh.c")" || true
[ "$n" = 1 ] || grammar_die "foh.c has $n D35 rest-rehome lines (want exactly 1)
  — T2 cannot target the re-home unambiguously"
# Both must live inside css_back, not somewhere else that happens to match.
awk '/^static void css_back\(FohState \*s\) \{$/,/^\}$/' "$FOH/foh.c" \
  > "$BUILD/css_back.txt"
made "$BUILD/css_back.txt"
grep -qxF "$D35_CARRY" "$BUILD/css_back.txt" \
  || grammar_die "D35's carry release is not inside css_back() any more"
grep -qxF "$D35_REST" "$BUILD/css_back.txt" \
  || grammar_die "D35's rest re-home is not inside css_back() any more"
echo "   both D35 statements are unique in foh.c and inside css_back()"

# --- [2] the witness against the REAL tree ----------------------------------
# port/gfx/ctl_style.c is REQUIRED, not optional: foh.c's Controls screen calls
# ctl_style_get/ctl_style_set, which live in that TU and nowhere else.
# foh_render.c is here for foh_anim_tick, which foh_tick calls every tick;
# fdlibm supplies its sin/cos. (check-css-token-rest.sh's TU list, unchanged.)
echo "=== [2] CSS back-select witness (real gestures through real foh_tick)"
CFLAGS_COMMON=(-ffp-contract=off -Wall -Wextra -Werror -Iport/ryu -Iport/sim
               -Ioracle/qjs)
CBS_SRCS=("$FOH/foh_cssbacksel_witness.c" "$FOH/foh_render.c" "$FOH/foh_font.c"
          "$GFX/raster.c" "$GFX/img1.c" "$GFX/ctl_style.c" port/fdlibm/fdlibm.c)
CBS_OK='^CSS BACK SELECT OK$'
cc -O2 "${CFLAGS_COMMON[@]}" -o "$BUILD/cbs" "$FOH/foh.c" "${CBS_SRCS[@]}" -lm \
  || fail "the witness did not build against the real tree"
made "$BUILD/cbs"
"$BUILD/cbs" > "$BUILD/cbs.out" 2>&1 || { relay_lines < "$BUILD/cbs.out"
  fail "the witness failed against the REAL foh.c"; }
relay_lines < "$BUILD/cbs.out"
one_canonical "$BUILD/cbs.out" 'CSS BACK SELECT OK' "$CBS_OK" "witness verdict"

# perturb <name> <exact-line> <replacement> — derive a COPY of foh.c with one
# line swapped, build the witness against it and require rc 1. Prints nothing;
# leaves $BUILD/$name/out for the caller's own greps.
perturb() { # <name> <line> <replacement>
  local name="$1" line="$2" repl="$3" rc=0
  mkdir -p "$BUILD/$name"
  node -e '
    const fs = require("fs");
    const [src, dst, line, repl] = process.argv.slice(1);
    const raw = fs.readFileSync(src, "utf8");
    const n = raw.split(line).length - 1;
    if (n !== 1) {
      console.error("found " + n + " copies of the target line (want 1)");
      process.exit(1);
    }
    const out = raw.replace(line, repl);
    if (out === raw) { console.error("substitution was a no-op"); process.exit(1); }
    fs.writeFileSync(dst, out);
  ' "$FOH/foh.c" "$BUILD/$name/foh.c" "$line" "$repl" \
    || fail "$name: could not derive the perturbed foh.c copy"
  cmp -s "$BUILD/$name/foh.c" "$FOH/foh.c" \
    && fail "$name: the perturbed copy is byte-identical to foh.c (dead tooth)"
  # -Iport/foh so the COPY's quoted "foh.h" and "../gfx/ctl_style.h" resolve to
  # the real tree; every other header still comes from the real tree.
  cc -O2 "${CFLAGS_COMMON[@]}" -Iport/foh -o "$BUILD/$name/cbs" \
    "$BUILD/$name/foh.c" "${CBS_SRCS[@]}" -lm \
    || fail "$name: the perturbed copy did not build"
  "$BUILD/$name/cbs" > "$BUILD/$name/out" 2>&1 || rc=$?
  [ "$rc" = 1 ] \
    || fail "$name: the perturbed build exited rc $rc (want exactly 1 — rc 0
  means the witness is BLIND to the defect it exists to guard)"
  grep -qE "$CBS_OK" "$BUILD/$name/out" \
    && fail "$name: the perturbed build still printed the OK verdict"
  return 0
}

# --- [3] T1: the PLUMBING lie (D35's carry release deleted) ------------------
# Put back the pre-A43 back-out, which left the token grabbed. The leak returns
# and the walk to the BACK wedge drags it over falcon, so the SELECTION
# assertions must fail — the whole finding is that both planes really moved.
echo "=== [3] T1: without the carry release, backing out must lose the pick"
perturb tooth-noRelease "$D35_CARRY" \
  '    ; // T1: pre-A43 — the back-out leaves the token grabbed'
grep -qF "A: after walking right across the whole roster: both planes still read the pick (p1Char=falcon cssChar=falcon, want fox/fox)" \
  "$BUILD/tooth-noRelease/out" \
  || { relay_lines < "$BUILD/tooth-noRelease/out"
       fail "T1: the leaked token did not re-select FALCON on the walk to the
  wedge — the tooth is failing for some other reason and proves nothing"; }
echo "   T1: the leak reproduces the owner's falcon and fails (rc 1)"

# --- [4] T2: the DISPLAY lie (D35's rest re-home deleted) --------------------
# Leave the release in place — so no plane can move — and delete only the
# re-home. The leave-band token then stays drawn one cell right of the pick,
# which is the half the owner described as "puts the pin on top of them". ONLY
# the token-cell assertion may fail; a selection failure here would mean the
# copy differs from foh.c by more than this one line.
echo "=== [4] T2: without the rest re-home, the token must come back mis-drawn"
perturb tooth-noRehome "$D35_REST" \
  '  ; // T2: pre-A43 — the rest slot is left wherever the last drop put it'
grep -qF "B: on re-entry: P1's token rests on the cell of the character P1 picked (fox/2, got cell 3)" \
  "$BUILD/tooth-noRehome/out" \
  || { relay_lines < "$BUILD/tooth-noRehome/out"
       fail "T2: the leave-band token did not come back drawn on falco's cell
  — the tooth is failing for some other reason and proves nothing"; }
grep -qE '^CSS BACK SELECT FAIL: .*both planes still read the pick' \
  "$BUILD/tooth-noRehome/out" \
  && { relay_lines < "$BUILD/tooth-noRehome/out"
       fail "T2: a SELECTION assertion failed in the perturbed build. This
  perturbation only moves where a token is DRAWN, so this means the copy
  differs from foh.c by more than the rest re-home"; }
# ORTHOGONALITY, asserted rather than assumed, and in the direction that is
# actually meaningful. T1's failure set NECESSARILY includes token-cell
# failures — a carried token is drawn on the hand, so the leak moves the pixels
# too; that is not a defect of the tooth. What must be true is that T2 CANNOT
# reproduce T1's failure: deleting the re-home leaves the plumbing intact, so
# no selection assertion may move, which is exactly the grep above. The pair
# therefore separates a lost PICK from a lying PIN, and neither perturbation
# can stand in for the other.
grep -qF "A: after walking right across the whole roster: both planes still read the pick (p1Char=fox cssChar=fox, want fox/fox)" \
  "$BUILD/tooth-noRehome/out" \
  || { relay_lines < "$BUILD/tooth-noRehome/out"
       fail "T1 and T2 are not orthogonal: T2's perturbation also lost the
  pick on the walk to the wedge, so it could stand in for T1"; }
echo "   T2: the mis-drawn token fails, the planes stay clean — T1 loses the
   PICK, T2 only lies about the PIN, and neither reproduces the other"

# --- [5] no-commit guard -----------------------------------------------------
git_dirty_after="$(tree_fingerprint)" \
  || fail "could not fingerprint the working tree after the run (fails CLOSED)"
[ "$git_dirty_before" = "$git_dirty_after" ] \
  || fail "this check modified the working tree (it must only write $BUILD,
  which is ignored) — before=$git_dirty_before after=$git_dirty_after"

echo "CSS BACK SELECT CHECK OK"
