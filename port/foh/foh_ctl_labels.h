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

// Follows ctl_roles() + s1_input_row_style() in port/gfx/s1_input.h.
// Since the 2026-08-24 owner re-ratification (DEVIATIONS D31/D32/D33)
// the FACE rows are STYLE-INDEPENDENT — A jump, B attack, X grab, Y
// special, in every style, BOX included. Only the shoulders differ:
//   L : shield in every style, EXCEPT the BOX arrangement that puts Mod
//       there (modOnR == false).
//   R : Mod in BOX (on the shoulder modOnR names); the C-layer hold in
//       NATURAL and NORMAL. BOX is the one style with no C-layer — six
//       gameplay buttons cannot carry seven roles (ctl_style.h).
static inline bool foh_ctl_has_clayer(CtlStyle style) {
  return style != CTL_STYLE_BOX;
}

static inline void foh_ctl_labels(CtlStyle style, bool modOnR,
                                  const char *out[FOH_CTL_LABEL_ROWS]) {
  const bool clayer = foh_ctl_has_clayer(style);
  const bool boxMod = (style == CTL_STYLE_BOX);
  out[0] = "CONTROL STICK";
  out[1] = "JUMP";
  out[2] = "ATTACK";
  // A42/D34: NOT "(Z)". Measured — `z` never dispatches GRAB in this engine
  // (it is an alternate ATTACK + an lCancel trigger); grab is reached by the
  // A+lightshield chord X now sends. The old "(Z)" label was the last
  // surviving instance of the claim that misled two agents into shipping a
  // grab button that did nothing. See CONTEXT.md, entry `z`.
  out[3] = "GRAB";
  out[4] = "SPECIAL";
  out[5] = boxMod ? (modOnR ? "SHIELD" : "MOD / TILT") : "SHIELD";
  out[6] = boxMod ? (modOnR ? "MOD / TILT" : "SHIELD")
                  : (clayer ? "C-STICK (HOLD)" : "SHIELD");
  out[7] = "PAUSE";
  out[8] = "PAUSE MENU";
}

#endif // FOH_CTL_LABELS_H
