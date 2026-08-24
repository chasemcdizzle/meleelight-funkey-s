#!/usr/bin/env bash
# port/gfx/check-ctl-input.sh — the L-SHOULDER check (fix_plan A25b + A3
# + A30(a) / DEVIATION D30). HOST ONLY: it needs no FunKey, which is the
# point — the defect it locks down is one the device rig is structurally
# blind to (every device check injects through our OWN uinput node, while
# the physical buttons come from fkgpiod, which the rig's quiesce bracket
# stops for the duration of a run).
#
# Leg [1] builds port/gfx/ctl_input_witness.c against the REAL tree and
# runs its assertions (the device translation arm, the KEYMAP1 SSOT,
# L-only shielding, the R C-layer, the D33 face plane INCLUDING grab on
# BOX, L-char-steps-in-target-select, the Mod-shoulder default).
# Leg [2] rebuilds the witness eight more times against PERTURBED COPIES of
# the tree — one file changed per copy — and requires each to FAIL. The
# teeth are orthogonal by construction: each perturbs a different file and
# each fails a different assertion, so no single leg can be carrying the
# whole check.
#
# Usage: bash port/gfx/check-ctl-input.sh
set -eu

cd "$(dirname "$0")/../.."
ROOT=$PWD
BUILD=.loop/ctl-input
GFX=port/gfx
FOH=port/foh
FROZEN=$FOH/keymap-frozen.txt

fail() { echo "CTL INPUT CHECK FAIL: $*" >&2; exit 1; }

CFLAGS=(-O2 -ffp-contract=off -Wall -Wextra -Werror
        -Iport/ryu -Iport/sim -Ioracle/qjs)
# The witness's own TU list. foh_render.c/foh_font.c/raster.c/img1.c are
# here for ONE symbol — foh_anim_tick, which foh_tick calls — and the
# ml_ser/ml_fmt/sha256/fdlibm tail is what foh_render.c needs in turn.
# Linking them is how leg [3b] gets to drive the REAL foh_tick instead of
# a re-implementation of it.
srcs_for() { # $1 = tree root (the real tree, or a perturbed copy)
  echo "$1/$GFX/ctl_input_witness.c $1/$GFX/ctl_style.c $1/$FOH/foh.c \
$1/$FOH/foh_render.c $1/$FOH/foh_font.c $1/$GFX/raster.c $1/$GFX/img1.c \
$ROOT/oracle/qjs/sha256.c $ROOT/port/sim/ml_ser.c $ROOT/port/sim/ml_fmt.c \
$ROOT/port/fdlibm/fdlibm.c"
}

rm -rf "$BUILD"
mkdir -p "$BUILD"

# --- [1] the witness against the REAL tree ----------------------------------
echo "=== [1] witness vs the real tree"
# shellcheck disable=SC2046
cc "${CFLAGS[@]}" -o "$BUILD/w" $(srcs_for "$ROOT") -lm \
  || fail "the witness did not build against the real tree"
"$BUILD/w" "$ROOT/$FROZEN" || fail "the witness FAILED against the real tree"

# --- [2] teeth: one perturbed file per copy, each must kill the witness ------
echo "=== [2] teeth (perturbed copies)"
teeth=0

# mk_copy <name> — a full source copy of port/gfx + port/foh, so a quoted
# include inside the copy resolves BESIDE the copy (the T12 idiom from
# check-device-foh.sh). Copies, not symlinks: a symlinked TU's quoted
# includes can resolve through the link and silently bypass the tooth.
mk_copy() {
  local d="$BUILD/$1"
  rm -rf "$d"
  mkdir -p "$d/port"
  cp -R "$ROOT/$GFX" "$d/port/gfx"
  cp -R "$ROOT/$FOH" "$d/port/foh"
  echo "$d"
}

# tooth <name> <file-under-copy> <perl-substitution> <what it should break>
tooth() {
  local name=$1 rel=$2 subst=$3 what=$4 d rc
  d=$(mk_copy "$name")
  perl -0pi -e "$subst" "$d/$rel" || fail "$name: substitution errored"
  if cmp -s "$d/$rel" "$ROOT/$rel"; then
    fail "$name: the perturbation was a NO-OP (dead tooth) on $rel"
  fi
  # shellcheck disable=SC2046
  cc "${CFLAGS[@]}" -o "$d/w" $(srcs_for "$d") -lm 2>"$d/build.log" || {
    fail "$name: the perturbed copy did not BUILD — a compile error is not
  evidence that the assertion bites. See $d/build.log"
  }
  rc=0
  "$d/w" "$d/$FROZEN" > "$d/run.log" 2>&1 || rc=$?
  [ "$rc" = 1 ] \
    || fail "$name: perturbed witness exited $rc (want exactly 1). It was
  supposed to break: $what"
  grep -q "^FAIL " "$d/run.log" \
    || fail "$name: the witness exited 1 with no FAIL line — wrong failure mode"
  echo "   $name OK: $what"
  teeth=$((teeth + 1))
}

# T1 — THE FIX ITSELF. Put the pre-2026-08-24 keysym back. The witness
# must die: 'm' no longer reaches .l, and the arm/SSOT/target-select legs
# all go red together, which is exactly the shape of the shipped bug.
tooth t1-oldkeysym port/gfx/platform_keymap.h \
  "s!\\{\"l\", 'K', 'm',!\\{\"l\", 'K', 'k',!" \
  "restoring keysym 'k' on the l row kills the witness"

# T2 — the SSOT comparison is not vacuous. Perturb only the FROZEN FILE,
# leaving the compiled table right: leg [2] must notice the drift while
# the arm keeps working.
tooth t2-frozendrift port/foh/keymap-frozen.txt \
  "s!^map l K m\$!map l K z!m" \
  "a frozen keymap that drifts from the compiled table is caught"

# T3 — the Mod-shoulder default is really read. Flip the cell back.
tooth t3-modonl port/gfx/ctl_style.c \
  "s!static bool g_modOnR = true;!static bool g_modOnR = false;!" \
  "reverting the Mod shoulder to L is caught"

# T4 — L's SHIELD claim is about the real role resolver, not a comment.
# Re-cut for DEVIATION D31: the expression is L-ONLY now, so the tooth
# RESTORES the pre-D31 both-shoulders form. That is the exact regression
# that would silently un-free R and take the C-layer down with it.
tooth t4-bothshields port/gfx/s1_input.h \
  "s!\\*shield = p->l;!*shield = (p->l || p->r);!" \
  "letting R shield again (the pre-D31 form) is caught"

# T5 — the C-layer really moved to the shoulder (D32). Put it back on Y:
# the styles that have a C-layer keep one, so nothing crashes, but R stops
# driving cs and leg [3d] dies.
tooth t5-clayeronY port/gfx/s1_input.h \
  "s!\\*clayer = ctl_style_has_clayer\\(style\\) \\? p->r : false;!*clayer = ctl_style_has_clayer(style) ? p->y : false;!" \
  "moving the C-layer back onto Y is caught"

# T6 — BOX GRABS (D33), which is the ruling the owner stated in so many
# words. Take the grab bit away and leg [3e] must die.
tooth t6-nograb port/gfx/s1_input.h \
  "s!  in.z = p->x; // GRAB!  in.z = false; // GRAB!" \
  "a pad that cannot grab is caught"

# T7 — and the face plane is not just SOME permutation: swap jump and
# attack, which compiles and emits four live bits, and [3e] still bites.
tooth t7-facescramble port/gfx/s1_input.h \
  "s!  in.a = p->b; // ATTACK\n  in.b = p->y; // SPECIAL\n  in.x = p->a; // JUMP!  in.a = p->a; // ATTACK\n  in.b = p->y; // SPECIAL\n  in.x = p->b; // JUMP!" \
  "scrambling jump/attack across the face plane is caught"

# T8 — and the target-select step is the real foh_tick arm.
tooth t8-nostep port/foh/foh.c \
  "s!s->p1Char = s->p1Char == 0 \\? 4 : s->p1Char - 1;!s->p1Char = s->p1Char;!" \
  "a dead L arm in target-select is caught"

[ "$teeth" = 8 ] || fail "expected 8 teeth, ran $teeth"
echo "CTL INPUT CHECK OK ($teeth teeth)"
