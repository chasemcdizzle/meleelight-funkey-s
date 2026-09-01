#!/usr/bin/env bash
# port/foh/check-rebind.sh — HOST-ONLY tooth for punch-list A31 (DEVIATION
# D26): the Controls > HANDHELD screen's real rebinder.
#
# WHAT THE TICKET WAS (owner, 2026-08-23)
# ---------------------------------------
#   "you should be able to rebind any of the 'active mappings'. currently you
#    can't even go to any of those rows, you can only change between 'style:'
#    and 'mod'. changing 'mod' changes the controls. we don't want that. get
#    rid of 'mod' altogether as an option here ... what is 'rebind: N/A'? why
#    do we even have that section? also, can we have a 'reset to defaults'
#    button please."
#
# WHY THIS IS NOT A SCREEN CHECK. The failure available here is a rebinder
# that repaints a label and rebinds nothing — and no assertion about pixels
# can see it. So the witness BINDS every screen claim to the play path: it
# navigates with real gestures through the real foh_tick, reads the new label
# off the rendered frame, and pushes a PlatformInput through the SAME two
# calls the product makes (ctl_bind_apply then s1_input_row_style), asserting
# the resulting Melee-unit row carries the new action. Leg [1] pins that this
# really is the product's chain: foh_dev.c's poll_bound() must exist with
# exactly that body and the match loops must call it instead of
# platform_poll.
#
# WHAT THIS PROVES. It builds port/foh/foh_rebind_witness.c against the REAL
# tree and runs it (assertions listed in that file's header). Then it re-runs
# the witness against FIVE perturbed copies — one per moving part — and
# requires each to fail, AND to fail ONLY where that part is:
#   T1  the rebind primitive made a no-op        -> the REBIND + DRIVE
#                                                   assertions fail; reset,
#                                                   caret and persist do NOT
#   T2  the renderer reads kAct[i] instead of
#       the bound slot (the LYING SCREEN)        -> only the LABEL assertions
#                                                   fail; the play-path DRIVE
#                                                   assertions do NOT  <-- the
#                                                   half-wired trap, in one
#                                                   test
#   T3  ctl_bind_apply made an identity copy
#       (the screen rebinds, the buttons do not) -> only the DRIVE / play-path
#                                                   assertions fail; the
#                                                   labels do NOT  <-- the
#                                                   same trap from the other
#                                                   side
#   T4  the RESET arm stops clearing the binding -> only the reset assertions
#                                                   fail
#   T5  the persisted `bind` rows dropped from
#       the serializer                           -> only the round-trip
#                                                   assertions fail
# T2 and T3 are the pair that matters: between them, a build in which the
# screen and the buttons disagree cannot pass in either direction.
#
# HOST-ONLY. Nothing here needs the device, an OPK or the sim tables; the one
# host prerequisite is node, for the pipeline's `assets` stage (the renderer
# hard-fails without menu.img1), which this check builds into its OWN dir.
# `bash port/foh/check-rebind.sh` -> `REBIND TOOTH OK`, exit 0.
#
# NOT A GATE. This is the A31 task-level tooth.
set -euo pipefail

FOH=port/foh
GFX=port/gfx
BUILD=$FOH/build/rebind
DATA=$BUILD/data

fail() { echo "REBIND TOOTH FAIL: $1" >&2; exit 1; }
grammar_die() { echo "REBIND TOOTH FAIL: $1" >&2; exit 2; }
relay_lines() { sed 's/^/  | /'; }

made() {
  local f
  for f in "$@"; do
    [ -s "$f" ] || fail "expected artifact missing/empty: $f"
  done
}

# --- [0] run lock (mkdir-atomic, NO reclaim — check-controls-labels.sh's) ----
LOCK=$FOH/build/rebind.lock
mkdir -p "$FOH/build"
mkdir "$LOCK" 2>/dev/null \
  || fail "another run holds $LOCK (remove it only if you are sure no run is live)"
trap 'rmdir "$LOCK" 2>/dev/null || true' EXIT

# --- [0a] no-commit guard (check-controls-labels.sh's, verbatim) -------------
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
# The witness HAND COPIES three palette entries and six coordinates out of the
# renderer, and it CLAIMS to exercise the product's own input chain. Both are
# pinned to EXACTLY ONE occurrence (the check-legibility.sh discipline), so a
# witness that quietly asserts against a screen or a chain that no longer
# exists dies here instead of going green.
echo "=== [1] grammar pins (the renderer geometry + the PRODUCT chain)"
made "$FOH/foh_render.c" "$FOH/foh.c" "$FOH/foh_dev.c" "$GFX/ctl_style.c" \
     "$GFX/ctl_style.h" "$FOH/foh_rebind_witness.c"
pin_n() { # <file> <count> <line> <what it is>
  local f="$1" k="$2" line="$3" what="$4" n
  n="$(grep -cxF "$line" "$f")" || true
  [ "$n" = "$k" ] || grammar_die "$f has $n copies of
  |$line|
  (want exactly $k) — $what. Re-read it and re-sync
  port/foh/foh_rebind_witness.c."
}
pin() { pin_n "$FOH/foh_render.c" 1 "$1" "$2"; }

# the three palette entries the witness overdraws in
pin 'static const RastCol kText = {220, 220, 230, 256};' \
    'the colour an UNSELECTED row`s button name is drawn in'
pin 'static const RastCol kDim = {120, 120, 140, 256};' \
    'the colour an UNSELECTED row`s action and the caption are drawn in'
pin 'static const RastCol kAccent = {255, 200, 60, 256};' \
    'the colour the SELECTED row and the caret are drawn in'
# the row geometry
pin '    const int y = 44 + i * 14;' 'the action rows` y expression'
pin '    foh_text(rz, 16, y, 1, kBtn[i], cur ? kAccent : kText);' \
    'the BUTTON column`s x, face and colour rule'
pin '    foh_text(rz, 96, y, 1, kAct[act], cur ? kAccent : kDim);' \
    'the ACTION column`s x, face and colour rule'
pin '  const int yStyle = 176, yReset = 190;' 'the two non-action rows` y table'
pin '    foh_text(rz, 6, yCur, 1, ">", kAccent);' 'the cursor caret'
pin '  text_center(rz, 204, 1, "L/R: CHANGE   A: RESET", kDim);' \
    'the caption that replaced `REBIND: N/A`'
# THE BOUND-LABEL EXPRESSION — the half a label-only check cannot see
pin '    const int act = (i == 0) ? 0 : ctl_bind_get(0, i - 1) + 1;' \
    'the row`s action index: the LOGICAL button the physical one is bound to.
  Without this the screen renders the pre-A31 fixed table and the rebinder is
  invisible on screen'
# the machine's rebind + reset arms
pin_n "$FOH/foh.c" 1 \
    '        ctl_bind_cycle(0, s->ctlRow - 1, rE ? 1 : -1);' \
    'the REBIND arm — L/R on an action row'
pin_n "$FOH/foh.c" 1 '        ctl_bind_reset(0);' \
    'the RESET arm — A on the reset row'
# THE PRODUCT CHAIN. The witness asserts through ctl_bind_apply +
# s1_input_row_style and claims that IS what the product does. These three
# lines are that claim: the remap helper, and the two match loops calling it.
pin_n "$FOH/foh_dev.c" 1 \
    '  ctl_bind_apply(0, in, in); // in-place is documented safe (ctl_style.c)' \
    'poll_bound()`s remap — the ONE place the play path applies the binding'
pin_n "$FOH/foh_dev.c" 1 \
    '  platform_poll(in);' \
    'poll_bound()`s poll. If a SECOND raw platform_poll appears inside a match
  loop, that loop reads an unremapped plane and the rebinder is half-live'
nbound="$(grep -c 'poll_bound(&' "$FOH/foh_dev.c")" || true
[ "$nbound" = 7 ] \
  || grammar_die "$FOH/foh_dev.c calls poll_bound at $nbound sites (want exactly
  7: the target loop's 3 and the VS loop's 4). A match-loop poll that went back
  to platform_poll would read the PHYSICAL plane while the rest of the loop
  reads the LOGICAL one — mixed planes, spurious pause edges."
nraw="$(grep -c 'platform_poll(&' "$FOH/foh_dev.c")" || true
[ "$nraw" = 3 ] \
  || grammar_die "$FOH/foh_dev.c calls platform_poll directly at $nraw sites
  (want exactly 3: the FOH menu loop's 2 and the VS warm-up poll). Menu
  navigation MUST stay on the physical buttons — a player who moved A
  elsewhere still has to be able to reach this screen and move it back."
echo "   17 hand-copied/claimed lines still read exactly as the witness assumes"

# --- [2] renderer art (pipeline 'assets' stage, this check's own dir) --------
echo "=== [2] renderer art (pipeline 'assets' stage, this check's own dir)"
node pipeline/run.js --only assets --out "$DATA" > "$BUILD/assets.log" 2>&1 \
  || { relay_lines < "$BUILD/assets.log"
       fail "the pipeline's assets stage did not run (host prerequisite: node)"; }
made "$DATA/assets/menu.img1"
echo "   $DATA/assets/menu.img1 built"

# --- [3] the witness against the REAL tree ----------------------------------
CFLAGS_COMMON=(-ffp-contract=off -Wall -Wextra -Werror -Iport/ryu -Iport/sim
               -Ioracle/qjs)
CFLAGS_COPY=(-Iport/foh -Iport/gfx)
REAL_SRCS=("$FOH/foh_rebind_witness.c" "$FOH/foh_render.c" "$FOH/foh.c"
           "$FOH/foh_font.c" "$FOH/foh_persist.c" "$GFX/raster.c" "$GFX/img1.c"
           "$GFX/ctl_style.c" oracle/qjs/sha256.c port/sim/ml_ser.c
           port/sim/ml_fmt.c port/fdlibm/fdlibm.c)
OK_RE='^REBIND OK$'

echo "=== [3] rebind witness (real gestures through real foh_tick)"
mkdir -p "$BUILD/real" "$BUILD/real/persist"
cc -O2 "${CFLAGS_COMMON[@]}" -o "$BUILD/real/reb" "${REAL_SRCS[@]}" -lm \
  || fail "the witness did not build against the real tree"
made "$BUILD/real/reb"
MLFK_DATA_DIR="$DATA" MLFK_PERSIST_DIR="$BUILD/real/persist" \
  "$BUILD/real/reb" > "$BUILD/real/reb.out" 2>&1 \
  || { relay_lines < "$BUILD/real/reb.out"
       fail "the witness failed against the REAL tree"; }
relay_lines < "$BUILD/real/reb.out"
nres="$(grep -cF 'REBIND OK' "$BUILD/real/reb.out")" || true
nexact="$(grep -cE "$OK_RE" "$BUILD/real/reb.out")" || true
[ "$nres" = 1 ] && [ "$nexact" = 1 ] \
  || grammar_die "witness verdict: $nres line(s) resemble 'REBIND OK' and
  $nexact match it exactly (want 1 and 1)"
# the round trip really did write a CURRENT record (and nothing else).
# A49 bumped the format to MLFKPERSIST6 (the `sel` selection row) and A26 to
# MLFKPERSIST7 (the `resume` hibernate row).
# This pin moves with the format ON PURPOSE — it asserts "the save is current",
# and a pin left on an older version asserts the opposite of what it means.
made "$BUILD/real/persist/mlfk-persist.dat"
hdr="$(sed -n 1p "$BUILD/real/persist/mlfk-persist.dat")"
[ "$hdr" = MLFKPERSIST7 ] \
  || grammar_die "the witness's saved record is headed '$hdr', not MLFKPERSIST7"
nbind="$(grep -c '^bind [0-3] [0-7] [0-7] [0-7] [0-7] [0-7] [0-7] [0-7] [0-7]$' \
         "$BUILD/real/persist/mlfk-persist.dat")" || true
[ "$nbind" = 4 ] \
  || grammar_die "the saved record carries $nbind well-formed 'bind' rows (want 4,
  one per CTL_BIND_PORTS — the per-port dimension is in the FORMAT so a second
  controller is a UI change and not another format bump)"

# perturb_build <dir> <source-file> <pairs...> — check-controls-labels.sh's,
# verbatim in method.
perturb_build() {
  local dir="$1" src="$2"; shift 2
  local base; base="$(basename "$src")"
  mkdir -p "$BUILD/$dir" "$BUILD/$dir/persist"
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
  ' "$src" "$BUILD/$dir/$base" "$@" \
    || fail "$dir: could not derive the perturbed $base"
  cmp -s "$BUILD/$dir/$base" "$src" \
    && fail "$dir: the perturbed copy is byte-identical to $src (dead tooth)"
  local srcs=() f
  for f in "${REAL_SRCS[@]}"; do
    if [ "$f" = "$src" ]; then srcs+=("$BUILD/$dir/$base"); else srcs+=("$f"); fi
  done
  cc -O2 "${CFLAGS_COMMON[@]}" "${CFLAGS_COPY[@]}" -o "$BUILD/$dir/reb" \
    "${srcs[@]}" -lm \
    || fail "$dir: the perturbed copy did not build"
  local rc=0
  MLFK_DATA_DIR="$DATA" MLFK_PERSIST_DIR="$BUILD/$dir/persist" \
    "$BUILD/$dir/reb" > "$BUILD/$dir/reb.out" 2>&1 || rc=$?
  [ "$rc" = 1 ] \
    || { relay_lines < "$BUILD/$dir/reb.out"
         fail "$dir: the perturbed build exited rc $rc (want exactly 1 — rc 0
  means the witness is BLIND to the defect it exists to guard)"; }
  grep -qE "$OK_RE" "$BUILD/$dir/reb.out" \
    && fail "$dir: the perturbed build still printed the OK verdict"
  return 0
}

must_fail() {
  grep -qE "$2" "$BUILD/$1/reb.out" \
    || { relay_lines < "$BUILD/$1/reb.out"
         fail "$1: no failure matched /$2/ — $3"; }
}
must_pass() {
  grep -qE "$2" "$BUILD/$1/reb.out" \
    && { relay_lines < "$BUILD/$1/reb.out"
         fail "$1: a failure matched /$2/ — $3"; }
  return 0
}

# --- [4] T1: the rebind primitive made a no-op ------------------------------
# The pre-A31 world seen from inside: the rows are reachable, the cursor
# works, reset works, persistence works — and L/R changes nothing. This is
# the ticket itself.
echo "=== [4] T1: a rebind that does not rebind must fail the witness"
perturb_build t1-nocycle "$GFX/ctl_style.c" \
  '  const int want = (g_bind[port][phys] + step) % n;' \
  '  const int want = g_bind[port][phys]; (void)step; // T1'
must_fail t1-nocycle 'REBIND FAIL: after one RIGHT on the A row: row 1' \
  "the rebound LABEL assertion did not fail — the screen would show a rebind
  that did not happen"
must_fail t1-nocycle 'REBIND FAIL: AND THE BUTTON FOLLOWS' \
  "the play-path assertion did not fail, so a dead rebinder passes"
must_pass t1-nocycle 'REBIND FAIL: (the caret|DOWN reaches row|.*wraps to)' \
  "navigation is not part of this perturbation — if it fails here the copy
  differs from ctl_style.c by more than the one line"
must_pass t1-nocycle 'REBIND FAIL: A on RESET' \
  "the reset arm is not part of this perturbation"
echo "   T1: L/R that rebinds nothing reproduces the ticket and fails (rc 1)"

# --- [5] T2: THE LYING SCREEN (labels ignore the binding) -------------------
# The rebinder WORKS — the buttons really do swap — and the screen keeps
# painting the pre-A31 fixed table. A play-path-only check goes green on it.
echo "=== [5] T2: the screen that ignores the binding must fail"
perturb_build t2-lyingscreen "$FOH/foh_render.c" \
  '    const int act = (i == 0) ? 0 : ctl_bind_get(0, i - 1) + 1;' \
  '    const int act = i; // T2: the pre-A31 fixed table'
must_fail t2-lyingscreen 'REBIND FAIL: after one RIGHT on the A row: row 1' \
  "the LABEL assertion did not fail, so the screen could say anything"
must_pass t2-lyingscreen 'REBIND FAIL: AND THE BUTTON FOLLOWS' \
  "a play-path assertion failed too. It must NOT: this perturbation moves only
  the renderer, and the buttons still rebind correctly — that asymmetry IS the
  half-wired trap"
must_pass t2-lyingscreen 'REBIND FAIL: (the binding SURVIVED|a duplicated slot)' \
  "persistence is not part of this perturbation"
echo "   T2: buttons rebind, screen does not -> only the LABELS fail (rc 1)"

# --- [6] T3: the half-wired other way (screen rebinds, buttons do not) ------
echo "=== [6] T3: the binding that never reaches the buttons must fail"
perturb_build t3-deadapply "$GFX/ctl_style.c" \
  '      *(bool *)((char *)&t + kBtnOff[g_bind[port][i]]) =' \
  '      *(bool *)((char *)&t + kBtnOff[i]) = // T3: identity copy'
must_fail t3-deadapply 'REBIND FAIL: AND THE BUTTON FOLLOWS' \
  "the play-path assertion did not fail — the screen would promise a rebind
  the buttons never make, which is the worst outcome this ticket has"
must_pass t3-deadapply 'REBIND FAIL: after one RIGHT on the A row: row 1' \
  "a LABEL assertion failed too. It must NOT: this perturbation moves only
  ctl_bind_apply, and the screen still reads the (correct) table"
must_pass t3-deadapply 'REBIND FAIL: (the caret|A on RESET restores)' \
  "neither the caret nor the reset arm is part of this perturbation"
echo "   T3: screen rebinds, buttons do not -> only the PLAY PATH fails (rc 1)"

# --- [7] T4: reset stops resetting ------------------------------------------
echo "=== [7] T4: a RESET that leaves the binding alone must fail"
perturb_build t4-noreset "$FOH/foh.c" \
  '        ctl_bind_reset(0);' \
  '        /* T4: reset dropped */;'
must_fail t4-noreset 'REBIND FAIL: A on RESET restores the EXACT identity binding' \
  "the reset assertion did not fail"
must_pass t4-noreset 'REBIND FAIL: AND THE BUTTON FOLLOWS' \
  "the rebind itself is not part of this perturbation"
must_pass t4-noreset 'REBIND FAIL: (the caret|DOWN reaches row)' \
  "navigation is not part of this perturbation"
echo "   T4: a reset that resets nothing fails (rc 1)"

# --- [8] T5: the persisted bind rows silently dropped -----------------------
# The format-bump half. A build that writes the bind rows as the IDENTITY
# would round trip to the identity and the player's rebind would vanish on
# the next boot.
#
# RE-POINTED, not weakened (ticket #22): persistence is now a declarative
# field table (foh_persist.c's FP_FIELDS) walked by one writer, so the
# hand-written `snprintf("bind %d …")` this tooth used to perturb no longer
# exists. The perturbation below is its exact equivalent in the new
# mechanism — the writer emits the ROW INDEX instead of the cell for the one
# row whose domain is a permutation, i.e. the identity for `bind` and
# nothing else — and every assertion underneath is unchanged. Dropping the
# table ROW is not usable here because the static assertion would refuse to
# BUILD such a tree, which perturb_build reads as a broken tooth rather than
# a caught defect; check-persist-table.sh leg [9] is where that is proven.
echo "=== [8] T5: bindings dropped from the persisted record must fail"
# RE-POINTED AGAIN (ticket #25), by the same rule and for the same reason: the
# flag writer gained a wire-bias column and an out-of-column guard, so it is no
# longer one line. The perturbation is unchanged in EFFECT — the one row whose
# domain is a permutation emits its index instead of its cell, i.e. the
# identity for `bind` and nothing else — and it deliberately goes THROUGH the
# bias and the guard rather than around them, so the perturbed build still
# writes a file the loader accepts. A tooth that made the writer die instead
# would prove nothing about whether a binding survives a round trip.
perturb_build t5-nopersist "$FOH/foh_persist.c" \
  '            const int d = *(const int *)cell + f->wireBias;' \
  '            const int d = // T5
                (f->dom == FP_DOM_PERM ? i : *(const int *)cell) + f->wireBias;'
must_fail t5-nopersist 'REBIND FAIL: the binding SURVIVED the save/load round trip' \
  "the round-trip assertion did not fail — a rebind that vanishes on the next
  boot would ship"
must_pass t5-nopersist 'REBIND FAIL: (AND THE BUTTON FOLLOWS|after one RIGHT)' \
  "the in-process rebinder is not part of this perturbation"
must_pass t5-nopersist 'REBIND FAIL: A on RESET' \
  "the reset arm is not part of this perturbation"
echo "   T5: a record that forgets the bindings fails (rc 1)"

# --- [9] no-commit guard -----------------------------------------------------
git_dirty_after="$(tree_fingerprint)" \
  || fail "could not fingerprint the working tree after the run (fails CLOSED)"
[ "$git_dirty_before" = "$git_dirty_after" ] \
  || fail "this check modified the working tree (it must only write $BUILD,
  which is ignored) — before=$git_dirty_before after=$git_dirty_after"

echo "REBIND TOOTH OK"
