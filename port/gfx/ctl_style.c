// port/gfx/ctl_style.c — the active-control-style + Mod-shoulder cells
// (fix_plan A4). Contract + archaeology + the measured smash/tilt note:
// ctl_style.h.
//
// One TU, two cells, no dependencies. Deliberately NOT header-only (the
// s1_input.h/snd_mixer.h precedent does not apply): the FOH sets these
// from the Controls screen in one TU and reads them in another, so a
// per-TU `static` copy would be a live desync bug.
#include "ctl_style.h"

// Owner ruling 2026-07-29: Natural is the fresh-install default.
static CtlStyle g_style = CTL_STYLE_DEFAULT;

// Owner ruling 2026-07-29: the Mod shoulder is remappable. false keeps
// the M3-ratified arrangement (Mod on L, shield on R).
static bool g_modOnR = false;

CtlStyle ctl_style_get(void) { return g_style; }

bool ctl_style_set(int style) {
  if (style < 0 || style >= (int)CTL_STYLE_COUNT) return false;
  g_style = (CtlStyle)style; // validated above: the cast is now total
  return true;
}

// C31 (driver, 2026-07-29): NORMAL's DISPLAY LABEL only. "Normal" and
// "Natural" share a prefix and a length and are near-indistinguishable in the
// 240x240 5x7 font — the owner is about to choose between exactly these two,
// so the label must not be the ambiguous part of the decision. "Classic"
// reads correctly for what NORMAL is (full-deflection stick, both shoulders
// shield, Y C-layer kept) and shares no prefix with "Natural" or "Box".
//
// THE ENUM IS A FROZEN WIRE FORMAT: CtlStyle values are stored verbatim in
// FohPersist.ctlStyle, so renumbering or removing a style silently remaps
// every save on disk — including the owner's. This change is the STRING and
// nothing else; CTL_STYLE_NORMAL is still 0.
const char *ctl_style_name(int style) {
  switch (style) {
  case CTL_STYLE_NATURAL: return "Natural";
  case CTL_STYLE_NORMAL: return "Classic";
  case CTL_STYLE_BOX: return "Box";
  default: return "?";
  }
}

bool ctl_mod_on_r_get(void) { return g_modOnR; }

void ctl_mod_on_r_set(bool onR) { g_modOnR = onR; }

const char *ctl_mod_shoulder_name(bool onR) {
  return onR ? "Mod: R" : "Mod: L";
}
