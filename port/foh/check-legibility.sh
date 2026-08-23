#!/usr/bin/env bash
# port/foh/check-legibility.sh — HOST-ONLY tooth for punch-list A32 and A25a,
# the two READABILITY defects the owner filed on playthrough #3.
#
# WHAT THE TWO TICKETS TURNED OUT TO BE
# -------------------------------------
# A32 was filed as "L-cancel is off for P2, P3 and P4 by default". Measured in
# render_opt_gameplay: L-CANCEL is row 1 and is a SINGLE GLOBAL value, and the
# only per-player row is row 4 — upstream's "Tapjump off", whose value reads
# "On" when tap jump is DISABLED (gameplaymenu.js:242). The defaults were
# never wrong; the row was a double negative and could not be read. DEVIATION
# D23 relabels it "TAP JUMP" and inverts the value ON DISPLAY ONLY — the
# `tapJumpOff` state plane keeps upstream's polarity, because foh_persist,
# foh_app and foh_dev hand that same bit to the sim.
#
# A25a is "the highlighting around test 1 or + ADD CODE you selected is not
# really visible to the eye", and it is real: the entire selection signal was
# a ONE-PIXEL border going grey -> pink around a black body, which on these
# rects is 242 changed pixels. DEVIATION D24 draws the selected slot's border
# twice and lifts its body off black.
#
# WHAT THIS PROVES. It builds port/foh/foh_legibility_witness.c against the
# REAL tree and runs it: the witness drives the REAL foh_tick through the real
# menu gestures and asserts what is ON SCREEN — the rendered STRING in the
# row-4 cell (by overdrawing the claimed string in the claimed colour and
# requiring zero changed pixels) and the pixel COUNT a slot changes when it
# becomes the selected one. Then it re-runs the witness against two COPIES of
# foh_render.c, one with A32 put back to the double negative and one with the
# A25a highlight put back to its one-pixel border, and requires BOTH to fail —
# so neither half can rot into a tautology. That negative test is the whole
# point: session A29's stated done-check passed before its fix because it
# tested the wrong plane.
#
# HOST-ONLY. Nothing here needs the device, an OPK, or the sim tables; the one
# host prerequisite is node, for the pipeline's `assets` stage (the renderer
# hard-fails without menu.img1), which this check builds into its OWN dir.
# `bash port/foh/check-legibility.sh` -> `LEGIBILITY CHECK OK`, exit 0.
#
# NOT A GATE. This is a task-level tooth.
set -euo pipefail

FOH=port/foh
GFX=port/gfx
BUILD=$FOH/build/legibility
DATA=$BUILD/data

fail() { echo "LEGIBILITY CHECK FAIL: $1" >&2; exit 1; }
grammar_die() { echo "LEGIBILITY CHECK FAIL: $1" >&2; exit 2; }
relay_lines() { sed 's/^/  | /'; }

made() { # every artifact must exist and be non-empty
  local f
  for f in "$@"; do
    [ -s "$f" ] || fail "expected artifact missing/empty: $f"
  done
}

# --- [0] run lock (mkdir-atomic, NO reclaim — the check-foh-flows pattern) ----
LOCK=$FOH/build/legibility.lock
mkdir -p "$FOH/build"
mkdir "$LOCK" 2>/dev/null \
  || fail "another run holds $LOCK (remove it only if you are sure no run is live)"
trap 'rmdir "$LOCK" 2>/dev/null || true' EXIT

# --- [0a] no-commit guard ----------------------------------------------------
# A check must never write a tracked file. Carried verbatim from
# check-css-token-rest.sh, including its two corrections: the `-f` test (a
# wholly-untracked nested checkout is reported as a bare DIRECTORY) and the
# name list (so a directory appearing or disappearing stays visible even
# though its bytes are not walked).
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
# The witness HAND COPIES three palette entries and the row/slot geometry out
# of foh_render.c, because both are file-static there and a witness that
# guessed either would assert against a screen that no longer exists. Every
# copied line is pinned to EXACTLY ONE occurrence, the check-css-token-rest.sh
# discipline.
echo "=== [1] grammar pins (what the witness hand-copies out of foh_render.c)"
made "$FOH/foh_render.c" "$FOH/foh_legibility_witness.c"
pin() { # <line> <what it is>
  local n
  n="$(grep -cxF "$1" "$FOH/foh_render.c")" || true
  [ "$n" = 1 ] || grammar_die "foh_render.c has $n copies of
  |$1|
  (want exactly 1) — $2. Re-read render_opt_gameplay / render_tss and re-sync
  port/foh/foh_legibility_witness.c."
}
# the palette the witness overdraws with
pin 'static const RastCol kText = {220, 220, 230, 256};' \
    'the row-label colour the witness overdraws the label in'
pin 'static const RastCol kDim = {120, 120, 140, 256};' \
    'the colour an OFF value is drawn in'
pin 'static const RastCol kAccent = {255, 200, 60, 256};' \
    'the colour an ON value is drawn in'
# the gameplay row-4 geometry (A32)
pin '  const int ys[5] = {40, 66, 92, 118, 144};' \
    'the gameplay rows` y table — the witness assumes row 4 sits at y 144'
pin '  foh_text(rz, 24, y, 1, label, row == curRow ? kText : kDim);' \
    'row_label`s label origin and colour'
pin '    const int x = 22 + k * 52;' 'the per-port cell x pitch'
pin '    const int y = ys[4] + 16;' 'the per-port cell y'
pin '    foh_text(rz, x + 16, y, 1, tapJumpOn ? "ON" : "OFF",' \
    'the D23 value call — its x offset, its two strings and its polarity'
# the target-select slot geometry (A25a)
pin '    const int x = 8 + col * 124, y = 30 + row * 22;' \
    'the TSS grid origin and pitch'
pin '    const int x = 70, y = 142, w = 100, h = 17;' \
    'the "+ ADD CODE" slot rect'
pin '    rrect(rz, x - 1, y - 1, 102, 21, 0, sel ? hot : idle);' \
    'the inner border rect — 102*21 minus the 100*19 body is the 242-pixel
  ceiling the witness threshold is set above'
pin '    fill_rect(rz, x, y, 100, 19, sel ? slotSel : slotBg);' \
    'the D24 body lift on the grid slots'
pin '    fill_rect(rz, x, y, w, h, sel ? slotSel : slotBg);' \
    'the D24 body lift on the "+ ADD CODE" slot'
# the look pin the witness relies on for a COLD flash in every frame it takes
pin '  s->tssTimer = 0;' \
    'foh_look_canonical`s tssTimer pin — without it the two frames the witness
  compares could differ by the 8-frame hover flash instead of the selection'
echo "   14 hand-copied lines still read exactly as the witness assumes"

# --- [2] renderer art (pipeline 'assets' stage, this check's own dir) --------
# foh_render hard-fails without menu.img1. Built into this check's OWN dir;
# nothing outside $BUILD is written. This is check-css-back.sh's arrangement.
echo "=== [2] renderer art (pipeline 'assets' stage, this check's own dir)"
node pipeline/run.js --only assets --out "$DATA" > "$BUILD/assets.log" 2>&1 \
  || { relay_lines < "$BUILD/assets.log"
       fail "the pipeline's assets stage did not run (host prerequisite: node)"; }
made "$DATA/assets/menu.img1"
echo "   $DATA/assets/menu.img1 built"

# --- [3] the witness against the REAL tree ----------------------------------
# port/gfx/ctl_style.c is REQUIRED: foh.c's Controls screen calls
# ctl_style_get/ctl_style_set, which live in that TU and nowhere else.
# fdlibm supplies the renderer's sin/cos.
CFLAGS_COMMON=(-ffp-contract=off -Wall -Wextra -Werror -Iport/ryu -Iport/sim
               -Ioracle/qjs)
LEG_SRCS=("$FOH/foh_legibility_witness.c" "$FOH/foh.c" "$FOH/foh_font.c"
          "$GFX/raster.c" "$GFX/img1.c" "$GFX/ctl_style.c" port/fdlibm/fdlibm.c)
LEG_OK='^LEGIBILITY OK$'

echo "=== [3] legibility witness (real gestures through real foh_tick)"
mkdir -p "$BUILD/real"
cc -O2 "${CFLAGS_COMMON[@]}" -o "$BUILD/real/leg" "$FOH/foh_render.c" \
  "${LEG_SRCS[@]}" -lm \
  || fail "the witness did not build against the real tree"
made "$BUILD/real/leg"
MLFK_DATA_DIR="$DATA" "$BUILD/real/leg" > "$BUILD/real/leg.out" 2>&1 \
  || { relay_lines < "$BUILD/real/leg.out"
       fail "the witness failed against the REAL foh_render.c"; }
relay_lines < "$BUILD/real/leg.out"
nres="$(grep -cF 'LEGIBILITY OK' "$BUILD/real/leg.out")" || true
nexact="$(grep -cE "$LEG_OK" "$BUILD/real/leg.out")" || true
[ "$nres" = 1 ] && [ "$nexact" = 1 ] \
  || grammar_die "witness verdict: $nres line(s) resemble 'LEGIBILITY OK' and
  $nexact match it exactly (want 1 and 1)"

# perturb <name> <sed-free node substitutions...> — derive a COPY of
# foh_render.c with exact-string replacements, refusing any that is a no-op or
# that is not unique. -Iport/foh so the COPY's quoted "foh.h" and
# "../gfx/ctl_style.h" resolve to the real tree, exactly as
# check-css-token-rest.sh's T1 copy-build does it.
perturb_build() { # <dir> <pairs...>  (pairs: from TAB to)
  local dir="$1"; shift
  mkdir -p "$BUILD/$dir"
  node -e '
    const fs = require("fs");
    const [src, dst, ...pairs] = process.argv.slice(1);
    let raw = fs.readFileSync(src, "utf8");
    for (let i = 0; i < pairs.length; i += 2) {
      const from = pairs[i], to = pairs[i + 1];
      const n = raw.split(from).length - 1;
      if (n !== 1) {
        console.error("perturb: " + n + " copies of |" + from + "| (want 1)");
        process.exit(1);
      }
      if (from === to) { console.error("perturb: no-op substitution"); process.exit(1); }
      raw = raw.replace(from, to);
    }
    fs.writeFileSync(dst, raw);
  ' "$FOH/foh_render.c" "$BUILD/$dir/foh_render.c" "$@" \
    || fail "$dir: could not derive the perturbed foh_render.c"
  cmp -s "$BUILD/$dir/foh_render.c" "$FOH/foh_render.c" \
    && fail "$dir: the perturbed copy is byte-identical to foh_render.c (dead tooth)"
  cc -O2 "${CFLAGS_COMMON[@]}" -Iport/foh -o "$BUILD/$dir/leg" \
    "$BUILD/$dir/foh_render.c" "${LEG_SRCS[@]}" -lm \
    || fail "$dir: the perturbed copy did not build"
  local rc=0
  MLFK_DATA_DIR="$DATA" "$BUILD/$dir/leg" > "$BUILD/$dir/leg.out" 2>&1 || rc=$?
  [ "$rc" = 1 ] \
    || { relay_lines < "$BUILD/$dir/leg.out"
         fail "$dir: the perturbed build exited rc $rc (want exactly 1 — rc 0
  means the witness is BLIND to the defect it exists to guard)"; }
  grep -qE "$LEG_OK" "$BUILD/$dir/leg.out" \
    && fail "$dir: the perturbed build still printed the OK verdict"
  return 0
}

# --- [4] T1: A32 put back to upstream's double negative ---------------------
echo "=== [4] T1: the pre-A32 double-negative row must fail the witness"
perturb_build t1-doublenegative \
  '"EVERYONE WALLJUMPS", "TAP JUMP"};' '"EVERYONE WALLJUMPS", "TAPJUMP OFF"};' \
  '    const int tapJumpOn = !s->tapJumpOff[k];' \
  '    const int tapJumpOn = s->tapJumpOff[k]; // T1: pre-A32 polarity'
# It must fail on the LABEL and on the VALUE, and on NEITHER slot assertion:
# reverting a label and a polarity moves no pixel on the target-select screen.
grep -qF 'LEGIBILITY FAIL: the per-player row'"'"'s LABEL on screen is "TAP JUMP"' \
  "$BUILD/t1-doublenegative/leg.out" \
  || { relay_lines < "$BUILD/t1-doublenegative/leg.out"
       fail "T1: the LABEL assertion did not fail — the tooth is failing for
  some other reason and proves nothing"; }
grep -qF 'LEGIBILITY FAIL: tap jump ENABLED: P1'"'"'s cell on screen reads "ON"' \
  "$BUILD/t1-doublenegative/leg.out" \
  || { relay_lines < "$BUILD/t1-doublenegative/leg.out"
       fail "T1: the ENABLED-reads-ON assertion did not fail — the double
  negative would still be on screen unnoticed"; }
grep -qE '^LEGIBILITY FAIL: slot ' "$BUILD/t1-doublenegative/leg.out" \
  && { relay_lines < "$BUILD/t1-doublenegative/leg.out"
       fail "T1: a target-select SLOT assertion failed too. This perturbation
  only touches the gameplay options screen, so the copy differs from
  foh_render.c by more than the two A32 lines"; }
echo "   T1: the double negative reproduces the owner's misreading and fails (rc 1)"

# --- [5] T2: A25a put back to its one-pixel border --------------------------
# The `(void)slotSel` is not decoration: with both body lifts reverted the
# D24 colour goes unused and -Werror=unused-variable would kill the copy
# before it could be judged. The perturbation must change the PICTURE and
# nothing else.
echo "=== [5] T2: the pre-A25a one-pixel highlight must fail the witness"
perturb_build t2-onepixel \
  '    if (sel) rrect(rz, x - 2, y - 2, 104, 23, 0, hot);  // D24' \
  '    if (sel && 0) rrect(rz, x - 2, y - 2, 104, 23, 0, hot);  // T2' \
  '    fill_rect(rz, x, y, 100, 19, sel ? slotSel : slotBg);' \
  '    (void)slotSel; fill_rect(rz, x, y, 100, 19, slotBg); // T2' \
  '    if (sel) rrect(rz, x - 2, y - 2, w + 4, h + 3, 0, hot);' \
  '    if (sel && 0) rrect(rz, x - 2, y - 2, w + 4, h + 3, 0, hot); // T2' \
  '    fill_rect(rz, x, y, w, h, sel ? slotSel : slotBg);' \
  '    fill_rect(rz, x, y, w, h, slotBg); // T2'
# EVERY slot must go dark, "+ ADD CODE" included — the owner named both.
for k in 0 1 2 3 4 5 6 7 8 9 10; do
  grep -qF "LEGIBILITY FAIL: slot $k " "$BUILD/t2-onepixel/leg.out" \
    || { relay_lines < "$BUILD/t2-onepixel/leg.out"
         fail "T2: slot $k still passed the visibility floor with a one-pixel
  border — the threshold is not biting on that slot"; }
done
# and it must fail ONLY there: a border is not a label.
grep -qE '^LEGIBILITY FAIL: (the per-player|tap jump)' "$BUILD/t2-onepixel/leg.out" \
  && { relay_lines < "$BUILD/t2-onepixel/leg.out"
       fail "T2: a GAMEPLAY-ROW assertion failed in the one-pixel build. This
  perturbation only touches render_tss, so the copy differs from
  foh_render.c by more than the four A25a lines"; }
n1px="$(grep -cE '^LEGIBILITY FAIL: slot ' "$BUILD/t2-onepixel/leg.out")" || true
[ "$n1px" = 11 ] \
  || { relay_lines < "$BUILD/t2-onepixel/leg.out"
       fail "T2: $n1px slot assertions failed (want exactly 11 — one per slot)"; }
echo "   T2: all 11 slots fall back under the visibility floor and fail (rc 1)"

# --- [6] no-commit guard -----------------------------------------------------
git_dirty_after="$(tree_fingerprint)" \
  || fail "could not fingerprint the working tree after the run (fails CLOSED)"
[ "$git_dirty_before" = "$git_dirty_after" ] \
  || fail "this check modified the working tree (it must only write $BUILD,
  which is ignored) — before=$git_dirty_before after=$git_dirty_after"

echo "LEGIBILITY CHECK OK"
