#!/usr/bin/env bash
# port/foh/check-hand.sh — HOST-ONLY tooth for punch-list A25(c), the owner's
# "free moving cursor (with the hand pointing)" on target-select, and for the
# DRY extraction that put it there.
#
# WHAT IT PROVES, in two halves that fail independently
# -----------------------------------------------------
# [A] THE FEATURE. port/foh/foh_hand_witness.c drives the REAL foh_tick from
#     boot through the real menu path onto target-select and asserts what the
#     owner would SEE: the screen opens hovering TARGET 1; one frame of a held
#     direction moves the hand a FRACTION of a slot (a free cursor, not a
#     per-press index step); the selection follows the hand through the slots
#     it crosses; the hand can sit in the gap between two slots, where it
#     hovers nothing and the selection sticks; all ELEVEN slots are reachable
#     and each hovers itself; A launches the slot the HAND is over; A on
#     "+ ADD CODE" refuses; the hand is clamped to the screen; and the hand
#     is DRAWN, with the renderer's ring landing inside the very rect the hit
#     test used. Nothing is hand-poked — every position is walked until the
#     machine reports it arrived.
#
# [B] THE REFACTOR. A25(c)'s hard constraint is that the CSS comes out of the
#     extraction BYTE-IDENTICAL. The existing CSS checks staying green
#     (check-css-back.sh, check-css-token-rest.sh, check-css-mode.sh) is
#     NECESSARY BUT NOT SUFFICIENT: they drive scripted gestures, so they only
#     visit states someone wrote down, and what moved was a clamp, an
#     integration order and a hit predicate — properties of every reachable
#     (position, gesture) pair. So leg [4] below is a DIFFERENTIAL: the same
#     driver (port/foh/foh_cssdiff_witness.c) is built against the working tree
#     AND against `git show $A25C_BASE:port/foh/{foh.c,foh.h,foh_render.c}` —
#     the PINNED pre-A25(c) commit, not `HEAD`, because `HEAD` becomes this
#     very change the moment it lands and the leg would quietly start comparing
#     a build against itself. Both are fed the identical 60,000-frame
#     pseudorandom button stream on the CSS, and
#     the two dumps must be byte-identical. The dump carries the hand's raw
#     IEEE-754 bits (a one-ulp drift is a changed line), every CSS-observable
#     machine field, the sound queue, both token positions, and an FNV-1a hash
#     of the RENDERED FRAME — which is how the foh_render.c half of the
#     refactor (the cell-hover flag) is covered at full pixel resolution.
#
# THE ORTHOGONAL PAIR is T1 and T3, in the check-rebind.sh T2/T3 discipline —
# one makes the SHARED code lie and one makes the TSS WIRING lie, and each fails
# exactly one half:
#   T1 — the shared HIT PREDICATE swallows the gutter. The CSS differential must
#        come out DIFFERENT; the feature witness must stay GREEN.
#   T3 — the target-select hand stops integrating. The feature witness must FAIL
#        (on the selection-follows-the-hand and launch-what-you-hover lines
#        specifically, not just anywhere); the CSS differential must come out
#        BYTE-IDENTICAL. That is what "the CSS is unaffected" means, as a test
#        rather than a claim.
# T2 is a third tooth answering a different question: it perturbs the shared
# MOTION (the clamp loses a pixel) and must fail BOTH halves. A perturbation of
# genuinely shared code that broke only one screen would mean the extraction is
# not shared at all — some caller kept a private copy — so "both" is the
# assertion, and it is the DRY property itself under test.
#
# HOST-ONLY and device-free by construction. `bash port/foh/check-hand.sh` ->
# `HAND CHECK OK`, exit 0.
#
# NOT A GATE. This is a task-level tooth.
set -euo pipefail

FOH=port/foh
GFX=port/gfx
BUILD=$FOH/build/hand
DATA=$BUILD/data
FRAMES=60000
# The commit this refactor is measured AGAINST: the tree as it stood before
# A25(c). Moving it means re-arguing the "the CSS did not change" claim from a
# different baseline, which is a reviewed decision, not a maintenance edit.
A25C_BASE=0e4375e96d1cfeb4180bd30d7bf5ada484a2830d

fail() { echo "HAND CHECK FAIL: $1" >&2; exit 1; }
grammar_die() { echo "HAND CHECK FAIL: $1" >&2; exit 2; }
relay_lines() { sed 's/^/  | /'; }

made() {
  local f
  for f in "$@"; do [ -s "$f" ] || fail "expected artifact missing/empty: $f"; done
}

# one_canonical <file> <needle> <anchored-regex> <ctx> — the PROCESS §3
# permissive-parse guard, carried from check-css-token-rest.sh.
one_canonical() {
  local f="$1" needle="$2" re="$3" ctx="$4" nres nexact
  nres="$(grep -cF "$needle" "$f")" || true
  nexact="$(grep -cE "$re" "$f")" || true
  if [ "$nres" != 1 ] || [ "$nexact" != 1 ]; then
    sed 's/^/  | /' "$f" >&2
    grammar_die "$ctx: $nres line(s) resemble '$needle' and $nexact match it
  exactly (want 1 and 1)."
  fi
}

# --- [0] run lock (mkdir-atomic, NO reclaim) ---------------------------------
LOCK=$FOH/build/hand.lock
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
mkdir -p "$BUILD"

CFLAGS_COMMON=(-ffp-contract=off -Wall -Wextra -Werror -Iport/ryu -Iport/sim
               -Ioracle/qjs)
# port/gfx/ctl_style.c is REQUIRED, not optional: foh.c's Controls screen calls
# ctl_style_get/ctl_style_set, which live in that TU and nowhere else. fdlibm
# supplies the renderer's sin/cos.
LINK_REST=("$FOH/foh.c" "$FOH/foh_render.c" "$FOH/foh_font.c" "$GFX/raster.c"
           "$GFX/img1.c" "$GFX/ctl_style.c" port/fdlibm/fdlibm.c)
HAND_OK='^HAND OK$'

# --- [1] grammar pins ---------------------------------------------------------
# (a) The extraction must actually BE an extraction. If either call site grew
#     its own copy of the motion or the hit test back, the differential would
#     still pass (a faithful copy is faithful) while the DRY property the owner
#     asked for was quietly gone. So the single definition and both call sites
#     are pinned.
# (b) The differential driver must not reach for post-A25c API, or its HEAD
#     build stops compiling and leg [4] proves nothing.
echo "=== [1] grammar pins (the extraction is one definition, two callers)"
made "$FOH/foh_hand.h" "$FOH/foh.c" "$FOH/foh_render.c" \
     "$FOH/foh_hand_witness.c" "$FOH/foh_cssdiff_witness.c"
pin() { # <file> <line> <what>
  local n
  n="$(grep -cxF "$2" "$1")" || true
  [ "$n" = 1 ] || grammar_die "$1 has $n copies of
  |$2|
  (want exactly 1) — $3"
}
pin "$FOH/foh_hand.h" \
  'static inline void foh_hand_step(double *x, double *y, int up, int down,' \
  'the ONE definition of the hand`s motion'
pin "$FOH/foh_hand.h" \
  'static inline int foh_hand_hit(const FohHandRect *rects, int n, double x,' \
  'the ONE definition of the hit predicate'
n="$(grep -cF 'foh_hand_step(' "$FOH/foh.c")" || true
# A7 (MENU-SPEC §8) re-pinned this 2 -> 3, and it is the review this line asks
# for. The third caller is `step_credits`: MENU-SPEC's DEVIATION D12 says the
# credits reticle is driven as a RELATIVE cursor, "exactly like the CSS
# (css.js:195-206)", because upstream's absolute rawX/rawY map gives a d-pad
# nine reachable positions and makes the screen unplayable. A screen whose
# spec says "the CSS cursor" and then reimplements it is the exact regression
# A25(c) exists to prevent, so calling the shared body IS the compliant
# outcome — the count going UP is the good direction, and ZERO or ONE still
# means a screen grew its own copy back.
[ "$n" = 3 ] || grammar_die "foh.c calls foh_hand_step $n times (want exactly 3
  — the CSS hand, the target-select hand and the credits reticle). A fourth
  caller is fine in principle but must be reviewed here; a DROP means a screen
  grew its own copy of the motion back, which is the thing A25(c) exists to
  prevent."
for f in "$FOH/foh.c" "$FOH/foh_render.c"; do
  n="$(grep -c 'lsX \* FOH_CURSOR_VX' "$f")" || true
  [ "$n" = 0 ] || grammar_die "$f has $n inline copies of the hand integration
  (want 0 — it lives in foh_hand.h now)"
done
for tok in foh_hand_step foh_hand_hit FohHandRect tssHandX tssHandY \
           foh_tss_slots foh_css_cells; do
  n="$(grep -c "$tok" "$FOH/foh_cssdiff_witness.c")" || true
  [ "$n" = 0 ] || grammar_die "foh_cssdiff_witness.c mentions '$tok'. It is
  compiled against $A25C_BASE as well as against the working tree, so it may use ONLY
  the FOH surface that exists on BOTH sides — otherwise the HEAD build fails
  and leg [4] silently stops being a differential."
done
echo "   one definition, two call sites, no inline copies, HEAD-safe driver"

# --- [2] renderer art (pipeline 'assets' stage, this check's OWN dir) --------
echo "=== [2] renderer art (pipeline 'assets' stage, this check's own dir)"
node pipeline/run.js --only assets --out "$DATA" > "$BUILD/assets.log" 2>&1 \
  || { relay_lines < "$BUILD/assets.log"
       fail "the pipeline's assets stage did not run (host prerequisite: node)"; }
made "$DATA/assets/menu.img1"

# --- [3] the feature witness against the REAL tree --------------------------
echo "=== [3] the free-hand witness (real gestures through real foh_tick)"
cc -O2 "${CFLAGS_COMMON[@]}" -o "$BUILD/hand" "$FOH/foh_hand_witness.c" \
  "${LINK_REST[@]}" -lm || fail "the witness did not build against the real tree"
made "$BUILD/hand"
MLFK_DATA_DIR="$DATA" "$BUILD/hand" > "$BUILD/hand.out" 2>&1 \
  || { relay_lines < "$BUILD/hand.out"; fail "the witness failed against the REAL tree"; }
relay_lines < "$BUILD/hand.out"
one_canonical "$BUILD/hand.out" 'HAND OK' "$HAND_OK" "witness verdict"

# --- [4] THE EXTRACTION DIFFERENTIAL: the CSS must be byte-identical --------
# HEAD's foh.c/foh.h/foh_render.c are checked out into a mirror tree whose
# LAYOUT matches the real one, because those files use quoted relative includes
# ("foh.h", "../gfx/raster.h") that resolve against the INCLUDING file's dir.
# Only headers are mirrored for gfx; every .c outside port/foh comes from the
# real tree, so the two builds differ in exactly the three files under test.
echo "=== [4] the extraction differential (CSS, working tree vs $A25C_BASE)"
git cat-file -e "$A25C_BASE^{commit}" 2>/dev/null \
  || fail "the pinned pre-A25(c) commit $A25C_BASE is not in this repository"
head_tree() { # <dir>
  local d="$1" f
  rm -rf "$d"
  mkdir -p "$d/port/foh"
  cp -R "$GFX" "$d/port/gfx"
  for f in foh.c foh.h foh_render.c foh_font.c foh_ctl_labels.h foh_pause.h \
           foh_persist.h; do
    git show "$A25C_BASE:$FOH/$f" > "$d/port/foh/$f" \
      || fail "could not read $A25C_BASE:$FOH/$f"
  done
  cp "$FOH/foh_cssdiff_witness.c" "$d/port/foh/"
}
head_tree "$BUILD/head"
cmp -s "$BUILD/head/port/foh/foh.c" "$FOH/foh.c" \
  && fail "$A25C_BASE's foh.c is byte-identical to the working tree's — this leg
  would be comparing a build against itself (dead differential)."

build_diff() { # <out> <foh-dir-or-empty>
  local out="$1" d="${2:-}"
  if [ -n "$d" ]; then
    cc -O2 "${CFLAGS_COMMON[@]}" -o "$out" \
      "$d/port/foh/foh_cssdiff_witness.c" "$d/port/foh/foh.c" \
      "$d/port/foh/foh_render.c" "$d/port/foh/foh_font.c" \
      "$GFX/raster.c" "$GFX/img1.c" "$GFX/ctl_style.c" port/fdlibm/fdlibm.c -lm
  else
    cc -O2 "${CFLAGS_COMMON[@]}" -o "$out" "$FOH/foh_cssdiff_witness.c" \
      "${LINK_REST[@]}" -lm
  fi
}
build_diff "$BUILD/diff-new" || fail "the differential driver did not build against the working tree"
build_diff "$BUILD/diff-head" "$BUILD/head" \
  || fail "the differential driver did not build against $A25C_BASE (it must use only
  the FOH surface that exists on both sides — see the pin in [1])"
# TWO SWEEPS, and A44 is why there are two.
#
# The A-PRESSING sweep (dump-new.txt) is the one this leg has always run and
# is what the coverage floors below are measured on: it grabs, carries and
# drops, which is the arm the A25(c) extraction moved.
#
# The A-FREE sweep (the `.noa` pair) is the CROSS-BUILD comparison. DEVIATION
# D40 gave the one hand the right to take ANY port's token and gave ports 2/3
# live type tabs, so an A press now reaches machine state $A25C_BASE has no
# field for — MEASURED: 34,150 of the 60,000 A-pressing frames diverge, the
# first at frame 6001 where the working tree carries PORT 3's token (`c=3`).
# Requiring byte equality there would be asserting something false about the
# base build, so this leg compares the plane both builds still share. Every
# D40 arm sits inside an `if (aE)`, so with no A edge the comparison is total
# again — and it stays total: the hand's raw doubles, the D3 clamp, the CELL
# HIT PREDICATE (hover SELECTS with no button, css.js:222-226), the selection
# plane, the sound queue, the event counts and the ink plane are all still
# compared byte for byte over all $FRAMES frames.
# WHAT MOVED, so it is not merely lost: the cross-build claim about the
# A-gated arms is now port/foh/check-css-p34.sh's, which drives grab, drop
# and the type tabs through the real foh_tick on all four ports with its own
# negative tests. The floors below still prove THIS sweep reaches them.
MLFK_DATA_DIR="$DATA" "$BUILD/diff-new" "$FRAMES" > "$BUILD/dump-new.txt" \
  || fail "the working-tree differential run failed"
MLFK_DATA_DIR="$DATA" "$BUILD/diff-new" "$FRAMES" --no-a > "$BUILD/dump-new.noa.txt" \
  || fail "the working-tree A-free differential run failed"
MLFK_DATA_DIR="$DATA" "$BUILD/diff-head" "$FRAMES" --no-a > "$BUILD/dump-head.noa.txt" \
  || fail "the HEAD A-free differential run failed"
made "$BUILD/dump-new.txt" "$BUILD/dump-new.noa.txt" "$BUILD/dump-head.noa.txt"
# The two working-tree sweeps must DIFFER, or `--no-a` is being ignored (an
# older base build silently dropping the flag would make the comparison below
# pass for free — the flag lives in the witness, which BOTH builds compile).
cmp -s "$BUILD/dump-new.txt" "$BUILD/dump-new.noa.txt" \
  && fail "the A-free sweep dumped the same bytes as the A-pressing one, so
  --no-a reached nothing and the differential below is vacuous"
nnoa="$(wc -l < "$BUILD/dump-head.noa.txt" | tr -d ' ')"
[ "$nnoa" = "$FRAMES" ] \
  || fail "the A-free HEAD sweep dumped $nnoa lines, want $FRAMES"
n="$(wc -l < "$BUILD/dump-new.txt" | tr -d ' ')"
[ "$n" = "$FRAMES" ] \
  || fail "the differential dumped $n lines, want $FRAMES (the sweep ended early)"
# The sweep must actually have STAYED on the CSS and actually have USED it: a
# walk that sat on one screen doing nothing would compare equal for free.
non_css="$(grep -vc ' sc=6 ' "$BUILD/dump-new.txt")" || true
[ "$non_css" = 0 ] \
  || fail "$non_css of $FRAMES swept frames were NOT on the CSS — the sweep
  leaked off the screen under test and the differential is weaker than it reads"
carried="$(grep -c ' c=0,' "$BUILD/dump-new.txt")" || true
[ "$carried" -ge 200 ] \
  || fail "only $carried swept frames carried a token (want >= 200) — the drop
  arm, which is the arm whose hit test the extraction moved, is barely covered"
# A49/DEVIATION D46 MOVED THIS FLOOR, and the direction is the point. Until
# A49 a token released outside the band came to rest one cell RIGHT of the
# character it selected (upstream's second rest formula); D46 homes it on the
# selection instead, so every later grab in this sweep starts from a different
# pixel and the walk lands differently. MEASURED consequence: the sweep used
# to reach 6 distinct (cssChar[0], cssChar[1]) PAIRS and now reaches 5 — but
# it reaches ALL FIVE ROSTER CELLS, which is what "the cell hit test is
# exercised" actually means. So the floor is stated as the thing it is for,
# EXHAUSTIVELY: every cell in the roster must be selected by this sweep. That
# is strictly stronger than any pair count — a degenerate sweep cannot satisfy
# it, and 6 mixed pairs could have been three characters across two ports.
cells="$(grep -o 'ch=[0-9],[0-9]' "$BUILD/dump-new.txt" \
  | sed -E 's/^ch=([0-9]),([0-9])$/\1\n\2/' | sort -u | wc -l | tr -d ' ')"
[ "$cells" = 5 ] \
  || fail "the sweep selected $cells of the 5 roster characters (want all 5) —
  the cell hit test is not exercised across the whole roster"
# A44 / DEVIATION D41 — THE TWO MASKED COLUMNS, and why masking them is not
# a weakening. A44 put ports 2 and 3 on the CSS, and four radius-9 tokens at
# pitch 20 do not fit a 44 px cell: laid out in a row they would have spanned
# 78 px and parked port 0's token a whole cell LEFT of the character it chose
# (foh.h's D41 derivation). The tokens therefore moved to upstream's own 2x2
# at r=7 / pitch 14 / DX 15 / y = CELL_Y+9. That moves exactly two columns of
# this dump — the token pixels (`tk*`) and the framebuffer hash (`fb=`) they
# are drawn into — and MEASURED, it moves nothing else: `h=` (the hand's raw
# IEEE-754 bits), `t=`, `c=`, `ch=`, `rest=`, `sl=`, `p=`, `vm=`, `bh=`,
# `sc=`, the event/sound counts, every sound NAME and `ink=` are byte-
# identical across all $FRAMES frames. That set IS what A25(c) extracted and
# what this leg exists to defend: the clamp, the integration order, the hit
# predicate and the machine plane they feed.
#
# THE MASK IS PROVEN LIVE, not assumed: the unmasked comparison must still
# FAIL. If a later change reverts D41's geometry the masked columns stop
# differing, that assertion fires, and the mask is to be deleted in the same
# change rather than left standing over nothing (CONTEXT.md "Tooth" — a mask
# nobody notices is how a differential goes quietly vacuous). And because the
# masked files are byte-EQUAL while the unmasked ones are not, the difference
# is contained in the masked columns BY CONSTRUCTION — no third column can
# have moved without the first cmp catching it.
D41_MASK='s/ tk[0-9]=[0-9a-f]*,[0-9a-f]*//g; s/ fb=[0-9a-f]*//'
sed "$D41_MASK" "$BUILD/dump-head.noa.txt" > "$BUILD/dump-head.masked.txt"
sed "$D41_MASK" "$BUILD/dump-new.noa.txt" > "$BUILD/dump-new.masked.txt"
made "$BUILD/dump-head.masked.txt" "$BUILD/dump-new.masked.txt"
cmp -s "$BUILD/dump-head.noa.txt" "$BUILD/dump-head.masked.txt" \
  && fail "the D41 mask removed nothing from the dump — it no longer names the
  columns it claims to, so the comparison below is unbounded"
cmp "$BUILD/dump-head.masked.txt" "$BUILD/dump-new.masked.txt" \
  || fail "THE CSS CHANGED OUTSIDE THE TOKEN GEOMETRY. The extraction was
  supposed to be a pure refactor on that side: same doubles, same integration
  order, same clamp, same hit predicate. The first differing line is above;
  the fields are the hand's raw IEEE-754 bits, the machine plane, the sound
  queue and the renderer's ink plane. DEVIATION D41 moves the token pixels
  and the framebuffer hash and those two columns are masked out here — a
  difference reported above is in NEITHER of them."
cmp -s "$BUILD/dump-head.noa.txt" "$BUILD/dump-new.noa.txt" \
  && fail "the unmasked dumps are byte-identical, so DEVIATION D41's token
  geometry is no longer in effect. The mask above is now covering nothing:
  delete it and restore the whole-dump comparison, in the same change."
echo "   $FRAMES frames, all on the CSS, $carried carrying, all $cells roster cells:"
echo "   A-free: working tree == $A25C_BASE byte for byte outside D41's two"
echo "   columns, and those two columns really do differ (the mask is live)"

# --- [5] T1: the gutter becomes a hit (the shared PREDICATE lies) -----------
# foh_hand_hit's comparisons are strict on all four sides, which is what makes
# the 2 px gap between two CSS cells genuinely no cell (D4). Relax ONE of them
# and the CSS must move.
echo "=== [5] T1: a non-strict hit predicate must break the differential"
perturb() { # <dir> <file-under-port/foh> <from> <to>
  local dir="$1" rel="$2" from="$3" to="$4"
  rm -rf "$BUILD/$dir"
  mkdir -p "$BUILD/$dir/port/foh"
  cp -R "$GFX" "$BUILD/$dir/port/gfx"
  cp "$FOH"/*.h "$BUILD/$dir/port/foh/"
  cp "$FOH/foh.c" "$FOH/foh_render.c" "$FOH/foh_font.c" \
     "$FOH/foh_cssdiff_witness.c" "$FOH/foh_hand_witness.c" \
     "$BUILD/$dir/port/foh/"
  node -e '
    const fs = require("fs");
    const [src, from, to] = process.argv.slice(1);
    const raw = fs.readFileSync(src, "utf8");
    const n = raw.split(from).length - 1;
    if (n !== 1) { console.error("perturb: " + n + " copies of |" + from + "| (want 1)"); process.exit(1); }
    if (from === to) { console.error("perturb: no-op"); process.exit(1); }
    fs.writeFileSync(src, raw.replace(from, to));
  ' "$BUILD/$dir/port/foh/$rel" "$from" "$to" \
    || fail "$dir: could not derive the perturbed $rel"
  cmp -s "$BUILD/$dir/port/foh/$rel" "$FOH/$rel" \
    && fail "$dir: the perturbed copy is byte-identical to $rel (dead tooth)"
  # `cmp` returning 1 (the files DO differ, which is what we want) would
  # otherwise be this function's exit status and `set -e` would kill the run.
  return 0
}
# run_tooth <dir> <want-diff-rc 0|1> <want-witness-rc 0|1>
run_tooth() {
  local dir="$1" wantDiff="$2" wantWit="$3" rc=0
  cc -O2 "${CFLAGS_COMMON[@]}" -o "$BUILD/$dir/diff" \
    "$BUILD/$dir/port/foh/foh_cssdiff_witness.c" "$BUILD/$dir/port/foh/foh.c" \
    "$BUILD/$dir/port/foh/foh_render.c" "$BUILD/$dir/port/foh/foh_font.c" \
    "$GFX/raster.c" "$GFX/img1.c" "$GFX/ctl_style.c" port/fdlibm/fdlibm.c -lm \
    || fail "$dir: the perturbed differential driver did not build"
  cc -O2 "${CFLAGS_COMMON[@]}" -o "$BUILD/$dir/hand" \
    "$BUILD/$dir/port/foh/foh_hand_witness.c" "$BUILD/$dir/port/foh/foh.c" \
    "$BUILD/$dir/port/foh/foh_render.c" "$BUILD/$dir/port/foh/foh_font.c" \
    "$GFX/raster.c" "$GFX/img1.c" "$GFX/ctl_style.c" port/fdlibm/fdlibm.c -lm \
    || fail "$dir: the perturbed witness did not build"
  # A-FREE and MASKED, for the two reasons leg [4] states at length: an A
  # press reaches DEVIATION D40 arms the pinned base has no field for, and
  # DEVIATION D41 moves the token pixels and the framebuffer hash. The
  # reference on the other side of this cmp is $A25C_BASE's own A-free dump,
  # so both sides must be reduced the same way or every tooth "bites" for
  # A44's reasons instead of its own.
  MLFK_DATA_DIR="$DATA" "$BUILD/$dir/diff" "$FRAMES" --no-a > "$BUILD/$dir/dump.txt" \
    || fail "$dir: the perturbed differential run failed"
  sed "$D41_MASK" "$BUILD/$dir/dump.txt" > "$BUILD/$dir/dump.masked.txt"
  rc=0; cmp -s "$BUILD/head-dump-ref.masked.txt" "$BUILD/$dir/dump.masked.txt" || rc=1
  [ "$rc" = "$wantDiff" ] \
    || fail "$dir: the CSS differential came out $( [ $rc = 0 ] && echo IDENTICAL || echo DIFFERENT ),
  want $( [ "$wantDiff" = 0 ] && echo IDENTICAL || echo DIFFERENT ). A tooth that
  does not bite proves nothing; a tooth that bites where it should not means the
  perturbation reached further than it was meant to."
  rc=0
  MLFK_DATA_DIR="$DATA" "$BUILD/$dir/hand" > "$BUILD/$dir/hand.out" 2>&1 || rc=$?
  [ "$rc" = "$wantWit" ] \
    || { relay_lines < "$BUILD/$dir/hand.out"
         fail "$dir: the feature witness exited rc $rc, want $wantWit"; }
}
cp "$BUILD/dump-head.noa.txt" "$BUILD/head-dump-ref.txt"
cp "$BUILD/dump-head.masked.txt" "$BUILD/head-dump-ref.masked.txt"

# The perturbation WIDENS the rect by the gutter rather than merely relaxing a
# `>` to a `>=`. MEASURED, and it is this project's rule-12 corollary again: a
# `>=` tooth is a no-op here, because the hand's x is 28.0 stepping by 2.4 and
# never lands EXACTLY on a cell edge (6, 52, 98, 144, 190), so the boundary case
# a strictness flip changes is unreachable. Swallowing the 2 px gutter is the
# same property stated in a reachable way.
perturb t1-gutter foh_hand.h \
  '    if (x > (double)rects[i].x && x < (double)(rects[i].x + rects[i].w) &&' \
  '    if (x > (double)rects[i].x && x < (double)(rects[i].x + rects[i].w + 2) && // T1'
run_tooth t1-gutter 1 0
echo "   T1: a hit predicate that swallows the gutter moves the CSS (DIFFERENT)"

# --- [6] T2: the clamp loses a pixel (the shared MOTION lies) ---------------
# This one must fail BOTH halves, and that is the point: the motion really is
# one body now, so a change to it has to be visible on both screens. A tooth
# here that broke only the CSS would mean target-select had kept a private copy.
echo "=== [6] T2: a one-pixel clamp change must break BOTH halves"
perturb t2-clamp foh_hand.h \
  '  if (*x > w) *x = w;' \
  '  if (*x > w - 1.0) *x = w - 1.0; // T2'
run_tooth t2-clamp 1 1
grep -qF "HAND FAIL: walking RIGHT forever stops at the right edge" \
  "$BUILD/t2-clamp/hand.out" \
  || { relay_lines < "$BUILD/t2-clamp/hand.out"
       fail "T2: the right-edge clamp assertion did not fail — the tooth is
  failing for some other reason and proves nothing"; }
echo "   T2: one shared clamp, both screens move (CSS DIFFERENT, witness rc 1)"

# --- [7] T3: the TSS hand stops moving (the FEATURE lies, the CSS does not) --
# The orthogonality claim, as a test: T1/T2 break the CSS and leave the feature
# witness green; T3 breaks the feature and leaves the CSS byte-identical.
echo "=== [7] T3: a frozen target-select hand must fail the witness ONLY"
perturb t3-frozen foh.c \
  '    foh_hand_step(&s->tssHandX, &s->tssHandY, in->up, in->down, in->left,' \
  '    if (0) foh_hand_step(&s->tssHandX, &s->tssHandY, in->up, in->down, in->left, // T3'
run_tooth t3-frozen 0 1
grep -qF 'HAND FAIL: holding DOWN walks the selection' "$BUILD/t3-frozen/hand.out" \
  || { relay_lines < "$BUILD/t3-frozen/hand.out"
       fail "T3: the selection-follows-the-hand assertion did not fail — the
  tooth is failing for some other reason and proves nothing"; }
grep -qF 'HAND FAIL: A launches the slot the HAND is over' "$BUILD/t3-frozen/hand.out" \
  || { relay_lines < "$BUILD/t3-frozen/hand.out"
       fail "T3: the launch-what-you-hover assertion did not fail — the owner's
  actual complaint would go unnoticed"; }
echo "   T3: the feature fails (rc 1) while the CSS stays byte-identical"

# --- [8] no-commit guard -----------------------------------------------------
git_dirty_after="$(tree_fingerprint)" \
  || fail "could not fingerprint the working tree after the run (fails CLOSED)"
[ "$git_dirty_before" = "$git_dirty_after" ] \
  || fail "this check modified the working tree (it must only write $BUILD,
  which is ignored) — before=$git_dirty_before after=$git_dirty_after"

echo "HAND CHECK OK"
