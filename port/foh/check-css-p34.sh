#!/usr/bin/env bash
# port/foh/check-css-p34.sh — HOST-ONLY tooth punch-list for fix_plan A44,
# the owner's "why can't I turn on player 3 and 4 at the CSS?"
#
# WHAT IT PROVES
# ------------------------------------------------------------------
# port/foh/foh_p34_witness.c drives the REAL foh_tick through the gestures a
# player performs — walk the hand onto port 2's type tab, press A, walk onto
# port 2's token, press A, carry it onto falco's cell, press A — and then
# reads the plane a match launches from. Its own header carries the step list
# and the argument for where it stops.
#
# THE THREE NEGATIVE TESTS below are the reason this file exists rather than a
# green run being reported. Each perturbs ONE line of the real source and
# requires the witness to die AT ITS OWN DIAGNOSTIC, so a witness that passed
# for an unrelated reason is caught:
#   T1 restores upstream's token-ownership guard (undoing DEVIATION D40(a)).
#      Port 2's token stops being grabbable and its character becomes
#      unsettable — the stub A44 exists to not ship.
#   T2 makes the hover-select write the plane by ROSTER INDEX instead of by
#      PORT. That is the exact confusion CONTEXT.md's "Port" row records, and
#      it is what shipped twice on this screen (D21, D35).
#   T3 makes the launch config read port 0's selection for every port, so the
#      screen looks right and the match does not.
#
# Not a gate: a task-level tooth punch-list, host-only and data-free, exactly
# like check-css-token-rest.sh whose structure it follows.
#   bash port/foh/check-css-p34.sh   ->   `CSS P34 CHECK OK`, exit 0
set -euo pipefail

FOH=port/foh
GFX=port/gfx
BUILD=$FOH/build/css-p34

fail() { echo "CSS P34 CHECK FAIL: $1" >&2; exit 1; }
grammar_die() { echo "CSS P34 CHECK FAIL: $1" >&2; exit 2; }
relay_lines() { sed 's/^/  | /'; }

made() { # the artifact must exist and be non-empty
  local f
  for f in "$@"; do
    [ -s "$f" ] || fail "expected artifact missing/empty: $f"
  done
}

# --- [0] run lock (mkdir-atomic, NO reclaim — check-foh-flows pattern) ------
LOCK=$FOH/build/css-p34.lock
mkdir -p "$FOH/build"
mkdir "$LOCK" 2>/dev/null \
  || fail "another run holds $LOCK (remove it only if you are sure no run is live)"
trap 'rmdir "$LOCK" 2>/dev/null || true' EXIT

# --- [0a] no-commit guard (check-css-token-rest.sh's, verbatim) -------------
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
  || fail "could not fingerprint the working tree (the guard fails CLOSED)"
[[ "$git_dirty_before" =~ ^[0-9a-f]{64}$ ]] \
  || fail "the tree fingerprint is not a sha256 (the guard fails CLOSED)"

rm -rf "$BUILD"
mkdir -p "$BUILD"

# --- [1] grammar pins (the three lines the negative tests perturb) ----------
# Each must be textually UNIQUE, or the perturbation below would hit some
# other line and the tooth would prove nothing about the arm it names.
echo "=== [1] grammar pins (one per negative test)"
made "$FOH/foh.c" "$FOH/foh_launch.h" "$FOH/foh_p34_witness.c"

# The trailing comment is what makes this line unique — two other loops in
# foh.c iterate the same bound at the same indent (the type-tab loop and the
# launch guard's participant count), so the bare header would match three
# lines and T1 would perturb whichever came first.
D40_GUARD='      for (int j = 0; j < FOH_CSS_PORTS; j++) { // D40(a): no ownership guard'
n="$(grep -cxF "$D40_GUARD" "$FOH/foh.c")" || true
[ "$n" = 1 ] || grammar_die "foh.c has $n D40(a) token-grab loop headers (want
  exactly 1) — T1 cannot restore upstream's ownership guard unambiguously"

SEL_WRITE='            s->selChar[k] = c; // the SHARED plane, by PORT (A44; foh.h)'
n="$(grep -cxF "$SEL_WRITE" "$FOH/foh.c")" || true
[ "$n" = 1 ] || grammar_die "foh.c has $n by-port selection writes (want exactly
  1) — T2 cannot make the write roster-indexed unambiguously"

CFG_CHAR='    ports[k].character = s->selChar[k];'
n="$(grep -cxF "$CFG_CHAR" "$FOH/foh_launch.h")" || true
[ "$n" = 1 ] || grammar_die "foh_launch.h has $n per-port character rows (want
  exactly 1) — T3 cannot collapse the launch config unambiguously"

# A49 adds three more. Each is the ONE line its own tooth rewrites, and each
# sits in a DIFFERENT function — the type cycle, the token position and the
# persist chokepoint — so the three A49 teeth cannot be one tooth wearing
# three labels.
D40B_CYCLE='          if (*t == 2) *t = -1;'
n="$(grep -cxF "$D40B_CYCLE" "$FOH/foh.c")" || true
[ "$n" = 1 ] || grammar_die "foh.c has $n type-cycle wrap lines (want exactly
  1) — T4 cannot restore DEVIATION D40(b) unambiguously"

D46_BASE='  const double base = (double)(foh_css_cell_x(c) + FOH_CSS_TOKEN_DX);'
n="$(grep -cxF "$D46_BASE" "$FOH/foh.c")" || true
[ "$n" = 1 ] || grammar_die "foh.c has $n resting-token base lines (want
  exactly 1) — T5 cannot restore the leave-band quirk unambiguously"

D45_REHOME='    s->cssChar[k] = p->selChar[k];'
n="$(grep -cxF "$D45_REHOME" "$FOH/foh_persist.c")" || true
[ "$n" = 1 ] || grammar_die "foh_persist.c has $n boot-time token re-home
  lines (want exactly 1) — T6 cannot break the re-home unambiguously"
echo "   all six perturbation targets are textually unique"

# --- [2] the witness, against the REAL tree --------------------------------
# Same objects and same reason as check-css-token-rest.sh's leg [2]:
# port/gfx/ctl_style.c is REQUIRED (foh.c's Controls screen calls into it),
# foh_render.c carries foh_anim_tick which foh_tick calls every tick, and
# fdlibm supplies its sin/cos.
echo "=== [2] P3/P4 witness (real gestures through the real foh_tick)"
CFLAGS_COMMON=(-ffp-contract=off -Wall -Wextra -Werror
               -Ipipeline/build/sim-tables -Iport/ryu -Iport/sim -Ioracle/qjs)
# A49 adds the persistence chokepoint: leg [9] drives a REAL save/load
# through foh_persist.c, so the file format is exercised rather than
# modelled. ml_ser.c/sha256.c are its SHA-256 seal, ml_fmt.c is ml_ser.c's
# own dependency; none of them is optional and none is a stub.
W_SRCS=("$FOH/foh_render.c" "$FOH/foh_font.c" "$FOH/foh_persist.c"
        "$GFX/raster.c" "$GFX/img1.c" "$GFX/ctl_style.c"
        port/sim/ml_ser.c port/sim/ml_fmt.c oracle/qjs/sha256.c
        port/fdlibm/fdlibm.c)
W_OK='^P34 WITNESS OK$'

# The generated M1 tables are a HEADER dependency here (foh_launch.h reaches
# port/sim/sim/sim.h for SimPortCfg, which includes the generated ml_stages.h).
# Regenerating is the documented recipe, and it writes only under pipeline/build.
if [ ! -f pipeline/build/sim-tables/ml_stages.h ]; then
  echo "   regenerating pipeline/build/sim-tables (animations,tables,stages)"
  node pipeline/run.js --only animations,tables,stages \
    --out pipeline/build/sim-tables > "$BUILD/pipeline.log" 2>&1 \
    || { relay_lines < "$BUILD/pipeline.log"; fail "the M1 pipeline stages this check's headers need did not run"; }
fi

cc -O2 "${CFLAGS_COMMON[@]}" -o "$BUILD/w" \
  "$FOH/foh_p34_witness.c" "$FOH/foh.c" "${W_SRCS[@]}" -lm \
  || fail "the witness did not build against the real tree"
made "$BUILD/w"
# HERMETIC PERSISTENCE: a FRESH dir per run, and never the product path
# (/mnt/mlfk-data). Leg [9] writes a real mlfk-persist.dat here; the
# no-commit guard below then proves the run touched nothing else.
PDIR="$BUILD/persist"
rm -rf "$PDIR"; mkdir -p "$PDIR"
MLFK_DATA_DIR="$GFX/data" MLFK_PERSIST_DIR="$PDIR" "$BUILD/w" > "$BUILD/w.out" 2>&1 \
  || { relay_lines < "$BUILD/w.out"; fail "the witness failed against the REAL foh.c"; }
grep -qE "$W_OK" "$BUILD/w.out" \
  || { relay_lines < "$BUILD/w.out"; fail "the witness printed no OK verdict"; }
relay_lines < "$BUILD/w.out"

# --- the tooth runner ------------------------------------------------------
# Copies the two sources into a private dir, rewrites ONE line, rebuilds
# against the real tree for everything else (-I$FOH so the copies' own
# includes resolve to the real headers), and requires: exit 1 (the witness's
# own failure class, never a crash or a build error), NO OK verdict, and the
# NAMED diagnostic. check-css-token-rest.sh's T1 uses the same shape.
run_tooth() { # <id> <file> <old-line> <new-line> <needle> <label>
  local id="$1" file="$2" old="$3" new="$4" needle="$5" label="$6" rc=0
  local dir="$BUILD/$id"
  mkdir -p "$dir"
  cp "$FOH/foh.c" "$FOH/foh_launch.h" "$FOH/foh_p34_witness.c" \
     "$FOH/foh_persist.c" "$dir/"
  node -e '
    const fs = require("fs");
    const [p, oldL, newL] = process.argv.slice(1);
    const raw = fs.readFileSync(p, "utf8");
    if (raw.split(oldL).length - 1 !== 1) {
      console.error("the target line does not occur exactly once");
      process.exit(1);
    }
    fs.writeFileSync(p, raw.replace(oldL, newL));
  ' "$dir/$file" "$old" "$new" \
    || fail "$id: could not derive the perturbed copy of $file"
  cmp -s "$dir/$file" "$FOH/$file" \
    && fail "$id: the perturbed copy is byte-identical to $file (a dead tooth)"
  # -I"$dir" so the copies win, then -I"$FOH" so everything NOT copied (foh.h,
  # foh_hand.h, the rest) still comes from the real tree — the same shape
  # check-css-token-rest.sh's T1 copy-build uses.
  # The perturbable TUs are passed from $dir; everything else still comes
  # from W_SRCS, minus the one copy that would otherwise be linked twice.
  local w_rest=()
  local src
  for src in "${W_SRCS[@]}"; do
    [ "$src" = "$FOH/foh_persist.c" ] || w_rest+=("$src")
  done
  cc -O2 "${CFLAGS_COMMON[@]}" -I"$dir" -I"$FOH" -o "$dir/w" \
    "$dir/foh_p34_witness.c" "$dir/foh.c" "$dir/foh_persist.c" \
    "${w_rest[@]}" -lm \
    || fail "$id: the perturbed copy did not build"
  rm -rf "$dir/persist"; mkdir -p "$dir/persist"
  MLFK_DATA_DIR="$GFX/data" MLFK_PERSIST_DIR="$dir/persist" "$dir/w" \
    > "$dir/w.out" 2>&1 || rc=$?
  [ "$rc" = 1 ] \
    || fail "$id: the perturbed build exited rc $rc, want exactly 1 — rc 0
  means the witness is BLIND to the defect it exists to guard"
  grep -qE "$W_OK" "$dir/w.out" \
    && fail "$id: the perturbed build still printed the OK verdict"
  grep -qF "$needle" "$dir/w.out" \
    || { relay_lines < "$dir/w.out"
         fail "$id: it died, but not at '$needle' — the tooth is failing for
  some other reason and proves nothing"; }
  echo "   $label"
}

# --- [3] T1: upstream's ownership guard restored (D40(a) undone) -----------
# `playerType[j] == 1 || i == j` is upstream's rule and it is right upstream,
# where every human port has its own hand. Here there is one hand, so the
# guard's only effect is to make a HMN port 2 permanently marth.
echo "=== [3] T1: without D40(a), port 2's token cannot be taken"
run_tooth t1-ownership foh.c \
  "$D40_GUARD" \
  "      for (int j = 0; j < FOH_CSS_PORTS; j++) { if (!(foh_css_port_type(s, j) == 1 || j == 0)) continue; // T1" \
  "did not pick it up" \
  "T1: the upstream ownership guard makes port 2's token untouchable, so the
   port can be switched on and never given a character"

# --- [4] T2: the plane written by ROSTER INDEX instead of by PORT ----------
# D21 and D35 were both this. The witness picks falco (3) for port 2 so the
# two indices are distinct and the swap is visible; picking cell 2 would have
# made this tooth pass by coincidence.
echo "=== [4] T2: the selection written by roster index, not by port"
run_tooth t2-portindex foh.c \
  "$SEL_WRITE" \
  "            s->selChar[c] = c; // T2: roster index, not port" \
  "a port index and a roster index have changed places" \
  "T2: writing the selection plane at the ROSTER index leaves port 2 on
   marth and moves an innocent port — the D21/D35 family, caught"

# --- [5] T3: the launch config collapsed onto port 0 -----------------------
# The screen still shows the right thing; only the match is wrong. This is
# the seam CONTEXT.md warns about, and BRIDGE-STATE's new p3/p4 columns are
# the same assertion one layer further out.
echo "=== [5] T3: every port launched with port 0's character"
run_tooth t3-cfgcollapse foh_launch.h \
  "$CFG_CHAR" \
  "    ports[k].character = s->selChar[0]; // T3: port 0's pick for everyone" \
  "the launch config is reading one port's selection for another" \
  "T3: a launch config that reads port 0's selection for every port dies at
   the seam, with the CSS still displaying the right characters"

# --- [6] T4: DEVIATION D40(b) restored — CPU unreachable on ports 2/3 ------
# This is the exact line the owner's first ruling deletes. Putting the
# two-cycle asymmetry back (`wrapAt = j < 2 ? 2 : 1`, expressed as the first
# unreachable value) makes ports 2 and 3 wrap N/A -> HMN -> N/A again, so the
# CPU press the ticket is about does nothing. It must die at leg [1].
echo "=== [6] T4: with D40(b) back, ports 2/3 can never reach CPU"
run_tooth t4-d40b foh.c \
  "$D40B_CYCLE" \
  "          if (*t == (j < 2 ? 2 : 1)) *t = -1; // T4: D40(b) restored" \
  "did not reach CPU(1) on the second press" \
  "T4: restoring D40(b)'s two-state cycle on ports 2/3 kills the CPU press
   the owner asked for, and the witness names the port and the press"

# --- [7] T5: DEVIATION D46 reverted — the leave-band quirk restored --------
# Upstream's second rest formula, verbatim as it stood before A49: a
# different base AND a different pitch, landing a released token one whole
# cell right of the character it just selected. That is what the owner
# reported. The tooth touches ONLY rest slot 1, so it cannot pass by
# breaking the A-drop path leg [4] tests — the two claims stay separable.
echo "=== [7] T5: with the leave-band quirk back, a released pin misses"
run_tooth t5-d46 foh.c \
  "$D46_BASE" \
  "  const double base = s->cssTokenRest[k] == 1 ? (double)(foh_css_cell_x(0) + FOH_CSS_TOKEN_DX + FOH_CSS_TOKEN_LB_DX + FOH_CSS_TOKEN_LB_PITCH * c) : (double)(foh_css_cell_x(c) + FOH_CSS_TOKEN_DX); // T5: D46 reverted" \
  "D46 says a released pin returns to the character that port" \
  "T5: reverting D46 puts a released pin back on the NEXT cell, and the
   witness names the port and the character it should have returned to"

# --- [8] T6: the boot-time token re-home broken (D21/D35/D46 at boot) ------
# The selection still survives the restart; only the PIN lies. That is
# precisely the shape of all three shipped bugs on this screen, and it is why
# the persisted plane is the SELECTION with the token derived from it.
# Perturbing the re-home rather than the selection keeps this orthogonal to
# T5: T5 is the in-session gesture, T6 is the boot.
echo "=== [8] T6: a restart that restores the pick but not the pin"
run_tooth t6-rehome foh_persist.c \
  "$D45_REHOME" \
  "    s->cssChar[k] = 0; // T6: the pin boots on marth whatever was picked" \
  "the token plane is re-homed FROM the selection at boot" \
  "T6: a boot that restores selChar but leaves the token on marth dies at
   the pin assertion, with the selection assertion still passing"

# --- [9] no-commit guard, closing ------------------------------------------
echo "=== [9] no-commit guard (this run must not have touched the tree)"
git_dirty_after="$(tree_fingerprint)" \
  || fail "could not re-fingerprint the working tree (the guard fails CLOSED)"
[ "$git_dirty_before" = "$git_dirty_after" ] \
  || fail "this check modified the working tree (it must only write $BUILD,
  which is ignored) before=$git_dirty_before after=$git_dirty_after"

echo "CSS P34 CHECK OK (6 teeth)"
