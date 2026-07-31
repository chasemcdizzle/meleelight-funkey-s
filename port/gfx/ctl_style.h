// port/gfx/ctl_style.h — control-STYLE selector + Mod-shoulder swap
// (fix_plan A4; owner rulings 2026-07-27 and 2026-07-29).
//
// The MODEL ONLY. This header owns the style ENUM, the process-wide
// active-style cell and the Mod-shoulder cell; the per-style chord
// TABLES live next door in s1_input.h (which owns S1ChordRow) so this
// TU has zero dependencies and can be linked anywhere — including the
// FOH, which needs the setters for the Controls screen.
//
// THE THREE STYLES
//   CTL_STYLE_NATURAL (the DEFAULT) — modelled on Chase's ssb64 port
//     (~/code_projects/ssb64-funkey-s/port/gfx/gfx_present_sdl1.c:173-184,
//     read verbatim): direct 1:1, no modifiers, no chords, no C-layer.
//     A=attack, B=special, X=Z (grab), Y=JUMP, L and R both shield,
//     Start=pause, d-pad = the stick at FULL deflection.
//     ONE DOCUMENTED DEVIATION from ssb64, forced by a finding already
//     measured in THIS project: ssb64 maps Y to C-up and takes jump from
//     tap-up, but a digital d-pad at full deflection tap-jumps on every
//     upward DI — which is why tap jump is FORCED OFF for the FunKey
//     player (prototypes/control-mapping/funkeyMapping.js header). With
//     tap jump off and all seven buttons spent, ssb64's mapping would
//     leave Natural with NO jump at all, so Natural spends Y on a real
//     jump button and gives up the single reachable C-direction instead.
//     Up-smash / up-tilt stay reachable via d-pad + A (see below).
//   CTL_STYLE_NORMAL — full-deflection stick, both shoulders shield, but
//     keeps the Y C-layer and the shield-drop row.
//   CTL_STYLE_BOX — the Chase-ratified S1 "One-Mod + C-layer" table,
//     byte-for-byte (PLAN §6; HayBox Melee20Button / B0XX lineage per
//     docs/research/b0xx-mapping.md). Mod + shield on the shoulders,
//     Y(hold) = C-stick layer.
//
// WHY NATURAL STILL GETS SMASHES *AND* TILTS — MEASURED, not assumed
// (asserted by .loop/ctl_smash_check.c against the real sim functions):
// smash detection is EDGE-TRIGGERED on stick velocity, not on magnitude.
// as_checkForSmashes (port/sim/action_state_shortcuts.c:387-393) needs
// full magnitude NOW *and* near-neutral TWO frames ago, while
// as_checkForTilts (:410-430) has NO stick-edge condition at all — only
// the fresh-A-press edge both share. So a fresh d-pad press IS the 0->1
// flick => SMASH, and holding the direction stops matching => TILT.
// The same edge-trigger shape recurs at :287 (fastfall, frames 0 vs 3),
// :541/:545 (dash / smash-turn, 0 vs 2), physics.c:367 (walljump, 0 vs 3)
// and physics.c:783 (0 vs 1) — it is a CLASS, not one site.
// What Natural genuinely LOSES (documented, not papered over): no WALK
// (that needs a sustained intermediate magnitude; full deflection always
// dashes), 8 directions at magnitude 1 only (no partial DI angles, no
// angled f-tilts), and no C-stick. BOX remains the full-surface scheme.
// Natural DOES keep platform shield-drop — I had this wrong and the
// review caught it. The arms are read IN ORDER in BOTH shield states:
// GUARD.c:79-99 and GUARDON.c:105-127 (fresh shielding enters GUARDON).
// Spotdodge (ESCAPEN) needs a STRICT lsY < -0.7, and the LATER PASS
// (drop-through) arm needs lsY < -0.65, so the drop band is
// [-0.70, -0.65): Natural's exact -0.7 diagonal lands in it while missing
// spotdodge. BOX's dedicated shield-drop row (-0.6875) is in the same
// band. Two further gates apply to the drop and are NOT style-specific:
// it is frame-6 EDGE-gated (MV_IN[6].lsY > -0.3 — the smash-edge class
// again, so a HELD diagonal does not re-drop) and needs onSurface[0]==1;
// GUARDON additionally requires timer > 1, so the very first shield frame
// cannot drop. All of this is asserted — coordinates in
// .loop/ctl_style_check.c (A4.9) and the engine arms, their ordering and
// their dispatch binding in .loop/ctl-style-check.sh leg 4, which carries
// a swapped-dispatch mutation tooth so the check cannot go vacuous.
//
// THE MOD SHOULDER is a SEPARATE setting, not a scheme (owner 2026-07-29:
// "the ability to remap the mod to R and L is shield, and you can switch
// that"). It is orthogonal: it applies to BOX independently of the style
// choice, and is a no-op in NATURAL/NORMAL where both shoulders shield.
// Swapping is a pure RELABELING of the two shoulders — the ratified BOX
// table is untouched, which .loop/ctl-style-check.sh proves by dumping
// all 2048 combos under BOTH arrangements and cmp-ing both against the
// pre-change HEAD implementation.
//
// Note the cells here are only consulted by callers that opt into the
// style-aware entry point s1_input_row_style(); s1_input_row() remains
// pinned to BOX with the ratified (Mod=L, shield=R) arrangement, so
// every existing binary and the 15 pinned S1 checks (port/gfx/s1_sweep.c,
// check-device-input.sh) are behaviourally untouched. s1_input.h
// deliberately takes the style and the shoulder arrangement as ARGUMENTS
// rather than reading these cells, so it stays free of ctl_style.c
// symbols and no existing link line has to change. See the handover note
// at the bottom of s1_input.h.
#ifndef GFX_CTL_STYLE_H
#define GFX_CTL_STYLE_H

#include <stdbool.h>

// PERSISTED VALUES — do not renumber. NORMAL/BOX keep the numbers they
// had in MLFKPERSIST2 so existing saves survive the v3 bump with their
// scheme UNCHANGED (foh_persist.h); NATURAL was appended rather than
// inserted at 0 for exactly that reason, and the fresh-install default
// is assigned explicitly in foh_persist_defaults() instead of relying on
// memset-to-zero.
typedef enum {
  CTL_STYLE_NORMAL = 0,  // full-deflection stick, both shoulders shield, C-layer
  CTL_STYLE_BOX = 1,     // ratified S1: Mod + shield on the shoulders, Y C-layer
  CTL_STYLE_NATURAL = 2, // ssb64-modelled 1:1, no modifiers (the DEFAULT)
  CTL_STYLE_COUNT = 3
} CtlStyle;

#define CTL_STYLE_DEFAULT CTL_STYLE_NATURAL

// Process-wide active style. Defaults to CTL_STYLE_DEFAULT.
CtlStyle ctl_style_get(void);

// Set the active style. Takes an INT, not a CtlStyle: the value often
// arrives from a persisted file, and converting an out-of-domain value
// to a narrow enum type at the CALL site is implementation-defined —
// the validation has to happen before any enum conversion (review-ctl
// r1). Out-of-domain values are REFUSED (the cell is left unchanged)
// and the call returns false, so a corrupt byte can never install a
// style that has no table.
bool ctl_style_set(int style);

// Stable display name for the Controls screen ("Natural" / "Classic" /
// "Box"). Returns "?" for an out-of-domain value (never NULL).
const char *ctl_style_name(int style);

// --- Mod shoulder (orthogonal to style; BOX-only effect) ---------------
// false = the RATIFIED arrangement: Mod on L, shield on R.
// true  = swapped: Mod on R, shield on L.
bool ctl_mod_on_r_get(void);
void ctl_mod_on_r_set(bool onR);

// Display name for the Controls screen row ("Mod: L" / "Mod: R").
const char *ctl_mod_shoulder_name(bool onR);

#endif // GFX_CTL_STYLE_H
