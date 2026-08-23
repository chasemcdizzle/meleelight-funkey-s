#!/usr/bin/env bash
# port/foh/check-css-token-rest.sh — HOST-ONLY tooth for punch-list A29, the
# owner-reported "CSS forgets my character on re-entry".
#
# WHAT IT PROVES, and what the bug turned out to be
# ------------------------------------------------
# Filed symptom: pick falco (P1) + marth (P2), play, quit, come back to the CSS
# — the roster reads marth and puff, a constant {0, 1} matching neither the
# picks nor foh_init's {0, 0} defaults. The filed hypothesis was that something
# on the match-exit path re-initialises `FohState.cssChar` to an index-identity
# fill.
#
# MEASURED, and it is NOT that. Both character planes survive the exit intact:
# `p1Char`/`p2Char` and `cssChar[]` still read the picks, and the relaunch uses
# them. What lied was the TOKEN. foh_dev.c's tdev_end_game puts both tokens in
# rest slot 2 — upstream's endGame snap, main.js:1382-1385 -> css.js:154-156 —
# and that slot indexed the ROSTER by the PORT number, so port 0's token landed
# on cell 0 (marth) and port 1's on cell 1 (puff) whatever those ports had
# chosen. That IS the {0, 1}. DEVIATION D21 (foh.c's slot-2 arm) makes the snap
# honour the port's chosen character; the argument for deviating, including
# upstream's own two-argument call to a one-argument callee, is written there.
#
# The tooth is port/foh/foh_cssrest_witness.c, which drives the REAL foh_tick
# through the REAL CSS gestures — grab, hover, drop, port-type toggle — with no
# character field ever hand-poked, then applies the A19 match-exit mutation and
# re-asserts. This script builds it, runs it, grammar-pins the two foh_dev.c
# lines the witness hand-copies, and then re-runs it against a COPY of foh.c
# with D21 put back to the port index, which MUST fail — so the witness cannot
# rot into a tautology.
#
# HOST-ONLY and data-free by construction: nothing here needs the pipeline
# tables, an OPK, or the device. `bash port/foh/check-css-token-rest.sh` ->
# `CSS TOKEN REST CHECK OK`, exit 0.
#
# NOT A GATE. This is a task-level tooth.
set -euo pipefail

FOH=port/foh
GFX=port/gfx
BUILD=$FOH/build/css-token-rest

fail() { echo "CSS TOKEN REST CHECK FAIL: $1" >&2; exit 1; }
grammar_die() { echo "CSS TOKEN REST CHECK FAIL: $1" >&2; exit 2; }
relay_lines() { sed 's/^/  | /'; }

made() { # every artifact must exist and be non-empty
  local f
  for f in "$@"; do
    [ -s "$f" ] || fail "expected artifact missing/empty: $f"
  done
}

# one_canonical <file> <needle> <anchored-regex> <ctx> — the PROCESS §3
# permissive-parse guard, carried verbatim from check-mexit-reentry.sh: count
# the lines RESEMBLING the verdict and the lines MATCHING it exactly, and
# require both to be 1. A resemblance that is not an exact match is corruption,
# and two exact matches mean the artifact carries more than one run.
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
LOCK=$FOH/build/css-token-rest.lock
mkdir -p "$FOH/build"
mkdir "$LOCK" 2>/dev/null \
  || fail "another run holds $LOCK (remove it only if you are sure no run is live)"
trap 'rmdir "$LOCK" 2>/dev/null || true' EXIT

# --- [0a] no-commit guard ----------------------------------------------------
# A check must never write a tracked file. This is check-mexit-reentry.sh's
# guard with ONE measured correction: fingerprint status + diff bytes + the
# untracked-but-not-ignored NAME LIST + the CONTENT of every untracked regular
# file (minus the .tokensave scratch DB, which mutates on its own), and fail
# closed if any component cannot be read. The correction is the `-f` test and
# the name list: `git ls-files -o` reports a wholly-untracked nested checkout
# as a bare DIRECTORY (oracle/harness/node_modules is one after
# `cd oracle/harness && npm install`), and feeding that to shasum makes the
# guard die on a tree it should simply have hashed. Folding the name list in
# keeps a directory's appearance or disappearance visible even though its
# bytes are not walked.
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

# --- [1] the two grammar pins ------------------------------------------------
# (a) The witness HAND COPIES foh_dev.c's match-exit mutation of the CSS plane,
#     because tdev_end_game wants a GameState and the re-entry block is inline
#     in main(). That copy is an obligation, so it is pinned rather than hoped
#     for: if either line moves, this dies loudly instead of testing a model of
#     a re-entry the program no longer performs.
echo "=== [1] grammar pins (the model the witness hand-copies)"
made "$FOH/foh_dev.c" "$FOH/foh.c" "$FOH/foh_cssrest_witness.c"
n="$(grep -cxF '  f->cssCarry = -1;' "$FOH/foh_dev.c")" || true
[ "$n" = 1 ] || grammar_die "foh_dev.c has $n 'f->cssCarry = -1;' lines (want 1)
  — tdev_end_game's CSS mutation moved; re-read it and re-sync the witness"
n="$(grep -cxF '  for (int k = 0; k < 2; k++) f->cssTokenRest[k] = 2;' \
  "$FOH/foh_dev.c")" || true
[ "$n" = 1 ] || grammar_die "foh_dev.c has $n lines putting both tokens in rest
  slot 2 (want 1) — the endGame SNAP the witness models moved or changed slot"
# (b) The D21 arm must be textually unique, so the negative test below can
#     perturb exactly it (the A-drop arm computes the same base by design now,
#     and its line would otherwise be indistinguishable).
D21_LINE='    base = (double)(foh_css_cell_x(c) + FOH_CSS_TOKEN_DX); // D21: `c`, not `k`'
n="$(grep -cxF "$D21_LINE" "$FOH/foh.c")" || true
[ "$n" = 1 ] || grammar_die "foh.c has $n D21 arm lines (want exactly 1) — the
  negative test below cannot target the endGame snap arm unambiguously"
echo "   foh_dev.c still snaps both tokens to rest slot 2; foh.c's D21 arm is unique"

# --- [2] the witness against the REAL tree ----------------------------------
# port/gfx/ctl_style.c is REQUIRED, not optional: foh.c's Controls screen calls
# ctl_style_get/ctl_style_set, which live in that TU and nowhere else (the value
# has no mirror in FohState, by design — MENU-SPEC §C30). foh_render.c is here
# for foh_anim_tick, which foh_tick calls every tick; fdlibm supplies its sin/cos.
echo "=== [2] CSS token-rest witness (real gestures through real foh_tick)"
CFLAGS_COMMON=(-ffp-contract=off -Wall -Wextra -Werror -Iport/ryu -Iport/sim
               -Ioracle/qjs)
CTR_SRCS=("$FOH/foh_cssrest_witness.c" "$FOH/foh_render.c" "$FOH/foh_font.c"
          "$GFX/raster.c" "$GFX/img1.c" "$GFX/ctl_style.c" port/fdlibm/fdlibm.c)
CTR_OK='^CSS TOKEN REST OK$'
cc -O2 "${CFLAGS_COMMON[@]}" -o "$BUILD/ctr" "$FOH/foh.c" "${CTR_SRCS[@]}" -lm \
  || fail "the witness did not build against the real tree"
made "$BUILD/ctr"
"$BUILD/ctr" > "$BUILD/ctr.out" 2>&1 || { relay_lines < "$BUILD/ctr.out"
  fail "the witness failed against the REAL foh.c"; }
relay_lines < "$BUILD/ctr.out"
one_canonical "$BUILD/ctr.out" 'CSS TOKEN REST OK' "$CTR_OK" "witness verdict"

# --- [3] T1: the negative test (D21 reverted to the port index) --------------
# Put the endGame snap back on `foh_css_cell_x(k)` — the pre-A29 code, and
# upstream's own index confusion — and require the witness to FAIL, on the
# token assertions and not for some unrelated reason.
echo "=== [3] T1: the pre-A29 (port-indexed) snap must fail the witness"
mkdir -p "$BUILD/tooth-portindex"
node -e '
  const fs = require("fs");
  const [src, dst, line] = process.argv.slice(1);
  const raw = fs.readFileSync(src, "utf8");
  const n = raw.split(line).length - 1;
  if (n !== 1) {
    console.error("T1: found " + n + " copies of the D21 arm (want exactly 1)");
    process.exit(1);
  }
  const back = "    base = (double)(foh_css_cell_x(k) + FOH_CSS_TOKEN_DX); // T1: pre-A29";
  const out = raw.replace(line, back);
  if (out === raw) { console.error("T1: substitution was a no-op"); process.exit(1); }
  fs.writeFileSync(dst, out);
' "$FOH/foh.c" "$BUILD/tooth-portindex/foh.c" "$D21_LINE" \
  || fail "T1: could not derive the port-indexed foh.c copy"
cmp -s "$BUILD/tooth-portindex/foh.c" "$FOH/foh.c" \
  && fail "T1: the perturbed copy is byte-identical to foh.c (dead tooth)"
# -Iport/foh so the COPY's quoted "foh.h" and "../gfx/ctl_style.h" resolve to
# the real tree; every other header still comes from the real tree, exactly as
# check-mexit-reentry.sh's T2 copy-build does it.
cc -O2 "${CFLAGS_COMMON[@]}" -Iport/foh -o "$BUILD/tooth-portindex/ctr" \
  "$BUILD/tooth-portindex/foh.c" "${CTR_SRCS[@]}" -lm \
  || fail "T1: the port-indexed copy did not build"
t1rc=0
"$BUILD/tooth-portindex/ctr" > "$BUILD/tooth-portindex/ctr.out" 2>&1 || t1rc=$?
[ "$t1rc" = 1 ] \
  || fail "T1: the port-indexed build exited rc $t1rc (want exactly 1 — rc 0
  means the witness is BLIND to the defect it exists to guard)"
grep -qE "$CTR_OK" "$BUILD/tooth-portindex/ctr.out" \
  && fail "T1: the port-indexed build still printed the OK verdict"
# It must fail on BOTH ports' post-exit token assertions and on NEITHER plane
# assertion: the whole finding is that the planes were never the problem.
grep -qF "after the match exit: P1's token rests on the cell of the character P1 picked (falco/3, got cell 0)" \
  "$BUILD/tooth-portindex/ctr.out" \
  || { relay_lines < "$BUILD/tooth-portindex/ctr.out"
       fail "T1: P1's token did not land on cell 0 — the tooth is failing for
  some other reason and proves nothing"; }
grep -qF "after the match exit: P2's token rests on the cell of the character P2 picked (falcon/4, got cell 1)" \
  "$BUILD/tooth-portindex/ctr.out" \
  || { relay_lines < "$BUILD/tooth-portindex/ctr.out"
       fail "T1: P2's token did not land on cell 1 — the tooth is failing for
  some other reason and proves nothing"; }
grep -qE '^CSS TOKEN REST FAIL: .*: both planes still read the picks' \
  "$BUILD/tooth-portindex/ctr.out" \
  && { relay_lines < "$BUILD/tooth-portindex/ctr.out"
       fail "T1: a CHARACTER PLANE assertion failed in the perturbed build. The
  perturbation only moves where a token is DRAWN, so this means the copy
  differs from foh.c by more than the D21 arm"; }
echo "   T1: the port-indexed snap reproduces the reported {0, 1} and fails (rc 1)"

# --- [4] no-commit guard -----------------------------------------------------
git_dirty_after="$(tree_fingerprint)" \
  || fail "could not fingerprint the working tree after the run (fails CLOSED)"
[ "$git_dirty_before" = "$git_dirty_after" ] \
  || fail "this check modified the working tree (it must only write $BUILD,
  which is ignored) — before=$git_dirty_before after=$git_dirty_after"

echo "CSS TOKEN REST CHECK OK"
