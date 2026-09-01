// port/gfx/ctl_style.c — the active-control-style + Mod-shoulder cells
// (fix_plan A4). Contract + archaeology + the measured smash/tilt note:
// ctl_style.h.
//
// One TU, two cells, no dependencies. Deliberately NOT header-only (the
// s1_input.h/snd_mixer.h precedent does not apply): the FOH sets these
// from the Controls screen in one TU and reads them in another, so a
// per-TU `static` copy would be a live desync bug.
#include "ctl_style.h"

#include <stddef.h> // offsetof (the binding plane's button table)

// Owner ruling 2026-07-29: Natural is the fresh-install default.
static CtlStyle g_style = CTL_STYLE_DEFAULT;

// Owner ruling 2026-07-29: the Mod shoulder is remappable. false keeps
// the M3-ratified arrangement (Mod on L, shield on R); true is the
// SWAPPED one (Mod on R, shield on L).
//
// DEFAULT FLIPPED TO true — fix_plan A30(a), DEVIATION D30. Owner,
// 2026-08-24: "box is good but L should be shield and R should be mod /
// tilt." This is a pure RELABELING of the two shoulders, not a table
// edit: the ratified BOX chord table is byte-untouched and ctl_roles()
// (s1_input.h:173-184) already reads this cell to decide which shoulder
// carries which role, which .loop/ctl-style-check.sh proved by dumping
// all 2048 combos under BOTH arrangements. It is a no-op in
// NATURAL/NORMAL, where both shoulders shield.
//
// THIS INITIALIZER IS NOT THE PRODUCT'S EFFECTIVE DEFAULT. Every FOH
// binary calls foh_persist_load() at boot, which runs
// foh_persist_defaults() and then ctl_mod_on_r_set(p.modOnR != 0) —
// so the fresh-install value the player actually gets is
// port/foh/foh_persist.c:52 (`p->modOnR = 0`), and the v2/v3 migration
// default at :368. Until the FOH lane flips those two to 1 this flip
// only reaches processes that never load a persist record. Measured,
// not assumed: foh_dev.c:1345 and foh_app.c:459 are the two setters.
static bool g_modOnR = true;

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
  // "-", NOT "?" — the same correction foh_render.c:2788 already made for the
  // target builder's NULL arm, and found here by port/foh/face-lint.js
  // (ticket #24) walking this switch's out-of-domain arm. foh_render.c:2121
  // draws this string through FACE 1, and face 1 carries no '?' at all
  // (foh_font.c's note says why that hole must stay), so what this arm used
  // to return was a check-build gfx_fatal and a placeholder box for the
  // player. The arm is unreachable today — ctl_style_set refuses an
  // out-of-domain value, so ctl_style_get can only hand back a validated one
  // — and this keeps it harmless if it ever becomes reachable.
  default: return "-";
  }
}

bool ctl_mod_on_r_get(void) { return g_modOnR; }

void ctl_mod_on_r_set(bool onR) { g_modOnR = onR; }

const char *ctl_mod_shoulder_name(bool onR) {
  return onR ? "Mod: R" : "Mod: L";
}

// --- BUTTON BINDINGS (fix_plan A31; contract + rationale in ctl_style.h) ----
//
// bind[port][phys] = the LOGICAL button that physical button `phys` drives.
// Fresh install is the IDENTITY, which makes ctl_bind_apply a struct copy —
// so a build with this feature and a build without it are behaviourally the
// same binary until the player changes something.
static int g_bind[CTL_BIND_PORTS][CTL_BTN_COUNT] = {
    {0, 1, 2, 3, 4, 5, 6, 7}, {0, 1, 2, 3, 4, 5, 6, 7},
    {0, 1, 2, 3, 4, 5, 6, 7}, {0, 1, 2, 3, 4, 5, 6, 7}};

// The eight button bits, by OFFSET rather than by a switch: a table is
// total over CtlBtn by construction, so there is no unreachable default
// arm that could return a pointer to the wrong field if the enum grows.
static const size_t kBtnOff[CTL_BTN_COUNT] = {
    offsetof(PlatformInput, a),     offsetof(PlatformInput, b),
    offsetof(PlatformInput, x),     offsetof(PlatformInput, y),
    offsetof(PlatformInput, l),     offsetof(PlatformInput, r),
    offsetof(PlatformInput, start), offsetof(PlatformInput, menu)};

static bool ctl_bind_dom(int port, int btn) {
  return port >= 0 && port < CTL_BIND_PORTS && btn >= 0 &&
         btn < (int)CTL_BTN_COUNT;
}

int ctl_bind_get(int port, int phys) {
  if (!ctl_bind_dom(port, phys)) return phys;
  return g_bind[port][phys];
}

bool ctl_bind_cycle(int port, int phys, int dir) {
  if (!ctl_bind_dom(port, phys) || dir == 0) return false;
  const int n = (int)CTL_BTN_COUNT;
  const int step = dir > 0 ? 1 : n - 1;
  const int want = (g_bind[port][phys] + step) % n;
  // find the physical button that currently holds `want` and swap with it.
  // The table is a permutation, so this always finds exactly one — and when
  // it finds `phys` itself (impossible for a real step, but total) the swap
  // is a no-op rather than a corruption.
  for (int k = 0; k < n; k++) {
    if (g_bind[port][k] == want) {
      g_bind[port][k] = g_bind[port][phys];
      g_bind[port][phys] = want;
      return true;
    }
  }
  return false; // unreachable while the invariant holds
}

void ctl_bind_reset(int port) {
  if (port < 0 || port >= CTL_BIND_PORTS) return;
  for (int i = 0; i < (int)CTL_BTN_COUNT; i++) g_bind[port][i] = i;
}

bool ctl_bind_set_row(int port, const int *slots) {
  if (port < 0 || port >= CTL_BIND_PORTS || !slots) return false;
  bool seen[CTL_BTN_COUNT] = {false};
  for (int i = 0; i < (int)CTL_BTN_COUNT; i++) {
    const int v = slots[i];
    if (v < 0 || v >= (int)CTL_BTN_COUNT || seen[v]) return false;
    seen[v] = true;
  }
  for (int i = 0; i < (int)CTL_BTN_COUNT; i++) g_bind[port][i] = slots[i];
  return true;
}

const char *ctl_btn_name(int btn) {
  switch (btn) {
  case CTL_BTN_A: return "A";
  case CTL_BTN_B: return "B";
  case CTL_BTN_X: return "X";
  case CTL_BTN_Y: return "Y";
  case CTL_BTN_L: return "L";
  case CTL_BTN_R: return "R";
  case CTL_BTN_START: return "START";
  case CTL_BTN_MENU: return "MENU";
  default: return "?";
  }
}

void ctl_bind_apply(int port, const PlatformInput *phys, PlatformInput *out) {
  // Built into a LOCAL and assigned last, so the product path's in-place
  // call (ctl_bind_apply(0, &pin, &pin)) cannot read a half-permuted struct.
  PlatformInput t = *phys; // d-pad, and the SDL_QUIT latch, verbatim
  if (port >= 0 && port < CTL_BIND_PORTS) {
    for (int i = 0; i < (int)CTL_BTN_COUNT; i++) {
      *(bool *)((char *)&t + kBtnOff[g_bind[port][i]]) =
          *(const bool *)((const char *)phys + kBtnOff[i]);
    }
  }
  *out = t;
}
