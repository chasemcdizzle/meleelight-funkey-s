// foh_ctl_labels.h — the Controls screen's ACTION LABEL table, derived from
// the two control cells the input path actually reads.
//
// WHY THIS IS A HEADER AND NOT INLINE IN foh_render.c (review-r15 MAJOR):
// the labels are a pure function of (style, modOnR), but inline in the draw
// routine they were unreachable for any test — the only coverage was the
// frozen keyboard screenshot, which exercises the FRESH-INSTALL style
// (NATURAL) and nothing else, so the BOX and NORMAL label rows could drift
// silently. Extracted here, check-foh-flows.sh leg [0m] COMPILES this header
// and pins all three styles x Mod-on-L/R against a frozen table.
//
// The C-layer predicate is DEFINED at port/gfx/s1_input.h:162
// (`ctl_style_has_clayer`). This TU cannot include that header — it drags the
// sim input + platform planes into a UI TU — and the header is owned by
// another in-flight lane, so the truth table is restated here ONCE and leg
// [0m] compares the two EXPRESSIONS textually, so neither side can move
// alone. Collapse onto the shared predicate when s1_input.h is next free
// (registered duplicate, not a silent one).
#ifndef FOH_CTL_LABELS_H
#define FOH_CTL_LABELS_H

#include "../gfx/ctl_style.h"

#include <stdbool.h>

#define FOH_CTL_LABEL_ROWS 9

// Follows ctl_roles() + s1_input_row_style() in port/gfx/s1_input.h:160-185,
// :280-284:
//   X / Y : C-layer styles (BOX, NORMAL) spend X on jump and Y on the C-stick
//           layer; NATURAL spends X on grab (Z) and Y on jump.
//   L / R : only BOX carries Mod, on the shoulder modOnR names; NORMAL and
//           NATURAL shield on BOTH shoulders.
static inline bool foh_ctl_has_clayer(CtlStyle style) {
  return style == CTL_STYLE_BOX || style == CTL_STYLE_NORMAL;
}

static inline void foh_ctl_labels(CtlStyle style, bool modOnR,
                                  const char *out[FOH_CTL_LABEL_ROWS]) {
  const bool clayer = foh_ctl_has_clayer(style);
  const bool boxMod = (style == CTL_STYLE_BOX);
  out[0] = "CONTROL STICK";
  out[1] = "ATTACK";
  out[2] = "SPECIAL";
  out[3] = clayer ? "JUMP" : "GRAB (Z)";
  out[4] = clayer ? "C-STICK (HOLD)" : "JUMP";
  out[5] = boxMod ? (modOnR ? "SHIELD" : "MOD / TILT") : "SHIELD";
  out[6] = boxMod ? (modOnR ? "MOD / TILT" : "SHIELD") : "SHIELD";
  out[7] = "PAUSE";
  out[8] = "PAUSE MENU";
}

#endif // FOH_CTL_LABELS_H
