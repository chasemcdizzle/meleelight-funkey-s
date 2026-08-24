// port/gfx/s1_input.h — the S1 "One-Mod + C-layer" input layer (M3
// task 5; PLAN §6 verbatim, issues #6/#9, prototype
// prototypes/control-mapping/funkeyMapping.js — the verified resolver
// this header owns THREE styles since fix_plan A4 (ctl_style.h)).
//
// The FACE PLANE is the same in all three since the 2026-08-24 owner
// re-ratification (DEVIATIONS D31/D32/D33): A=jump, B=attack, Y=special,
// X=grab(Z), Start=pause. Only the SHOULDERS tell the styles apart:
//   CTL_STYLE_BOX     = S1's coordinate table verbatim — Mod on one
//                       shoulder, shield on the other (SWAPPABLE since
//                       2026-07-29; Mod-on-R by default since D30).
//                       Chase-ratified 2026-07-17; the TABLE is NOT to
//                       be changed. It is the ONE style with no C-layer:
//                       six gameplay buttons cannot hold seven roles.
//   CTL_STYLE_NORMAL  = the same table minus the Mod family: L shields,
//                       R holds the C-layer, plus a dedicated
//                       shield-drop diagonal row (A3 + D31/D32).
//   CTL_STYLE_NATURAL = the ssb64-modelled 1:1 scheme and the DEFAULT —
//                       full deflection, no Mod band; L shields, R holds
//                       the C-layer (D31/D32).
// The pre-A4 entry points s1_resolve()/s1_input_row() stay pinned to
// (BOX, Mod-on-L), so callers that predate the selector still resolve
// the same STICK coordinates; their BUTTON plane moved with D33, which
// is the ratified change, not a regression.
//
// DATA-DRIVEN chord table (a table, not code branches): priority-ordered
// rows matched on {C-layer, Mod, shield, d-pad class, dy sign}; each row
// emits coordinate MAGNITUDES (signs come from the SOCD-resolved d-pad).
// Values are FINAL Melee units on the 1/80 grid, injected at the
// pollInputs seam (ml_poll_inputs copies the row VERBATIM — the
// harness-patched pollInputs form the frozen golden streams were
// recorded under). tasRescale is bypassed BY CONSTRUCTION: it is never
// called on this path (b0xx-mapping.md §3.2 — the documented saturation
// trap).
//
// Contract fixed by fix_plan §M3 conventions ("Input"):
//   - tapJumpOffp1 = true is the CALLER's duty (the --tapjump-off-p1
//     flag on the live app AND on every replay of a recorded session —
//     this header only synthesizes rows).
//   - left stick NEUTRAL while the C-layer shoulder is held (drift
//     freezes — documented S1 sacrifice).
//   - SOCD = neutral (moot on the physical cross d-pad; real on the
//     SDL2 host keyboard).
//   - digital shield emits r=true, rA=1.0 (single-stage trigger; no
//     light shield). l/lA stay false/0.
//
// Header-only ON PURPOSE: gfx_app.c consumes it without a new TU, so
// the reviewed task-4 build lists (check-device-render.sh, riglib's
// gfx_device link line) stay unchanged.
#ifndef GFX_S1_INPUT_H
#define GFX_S1_INPUT_H

#include <stdio.h>
#include <stdlib.h>

#include "../sim/input/input.h" // MlInput, nullInput, deaden, meleeRound
#include "ctl_style.h"          // CtlStyle (fix_plan A4)
#include "platform.h"           // PlatformInput

// d-pad shape classes after SOCD resolution.
typedef enum { S1_PAD_H = 0, S1_PAD_V = 1, S1_PAD_DIAG = 2 } S1Pad;

typedef struct {
  bool clayer;    // row requires the C-layer shoulder held (emits onto cs)
  int mod;        // Mod-ROLE requirement: 1 held, 0 not held, -1 any
  int shield;     // shield-ROLE requirement: 1 held, 0 not held, -1 any
                  // (which SHOULDER carries each role is ctl_roles' job)
  S1Pad pad;      // d-pad class this row serves
  int dySign;     // required sign of dy (-1/+1), 0 = any
  double magX;    // |x| magnitude emitted (sign = dx)
  double magY;    // |y| magnitude emitted (sign = dy)
  const char *name;
} S1ChordRow;

// The S1 coordinate table — PLAN §6 rows verbatim (values from
// docs/research/b0xx-mapping.md §2.2/§5, HayBox Melee20Button lineage;
// checked against meleelight's engine thresholds). FIRST MATCH WINS,
// mirroring the prototype resolver's branch order exactly:
//   clayer > (diag: mod>shield-drop>plain) > horizontal > vertical.
// Behind an accessor so multiple including TUs never trip
// -Wunused-const-variable under -Werror.
static inline const S1ChordRow *s1_chord_table(int *count) {
  static const S1ChordRow T[] = {
      // --- C-layer: d-pad drives the C-stick, left stick neutral.
      //     UNREACHABLE ON BOX since 2026-08-24 (DEVIATION D32/D33):
      //     BOX spends R on Mod and Y on SPECIAL, so ctl_roles never
      //     hands this style clayer=true. The ROWS ARE KEPT BYTE-EXACT
      //     ON PURPOSE — the ratified S1 table stays the ratified S1
      //     table, and if a future ruling ever buys BOX a C-layer
      //     button back, the coordinates are already here, unedited.
      {true, -1, -1, S1_PAD_H, 0, 1.0, 0.0, "clayer-horizontal"},
      {true, -1, -1, S1_PAD_V, 0, 0.0, 1.0, "clayer-vertical"},
      {true, -1, -1, S1_PAD_DIAG, 0, 0.7000, 0.7000, "clayer-diagonal"},
      // --- diagonals (prototype order: mod family, then shield drop,
      //     then plain — R+up-diagonal falls through to plain) ---------
      {false, 1, 1, S1_PAD_DIAG, 0, 0.6375, 0.3750, "L+R-diagonal-wavedash"},
      {false, 1, 0, S1_PAD_DIAG, 0, 0.7375, 0.3125, "L-diagonal-23deg"},
      {false, 0, 1, S1_PAD_DIAG, -1, 0.7000, 0.6875, "R-down-diagonal-shield-drop"},
      {false, 0, -1, S1_PAD_DIAG, 0, 0.7000, 0.7000, "plain-diagonal"},
      // --- horizontals (L+R+cardinal emits the plain Mod value —
      //     PLAN §6 quirk registry) ------------------------------------
      {false, 1, -1, S1_PAD_H, 0, 0.6625, 0.0, "L-horizontal-walk"},
      {false, 0, -1, S1_PAD_H, 0, 1.0, 0.0, "plain-horizontal-dash"},
      // --- verticals (R+straight-down = the same cardinal row:
      //     (0,-1.0) spotdodge) ----------------------------------------
      {false, 1, -1, S1_PAD_V, 0, 0.0, 0.5375, "L-vertical-tilt"},
      {false, 0, -1, S1_PAD_V, 0, 0.0, 1.0, "plain-vertical"},
  };
  *count = (int)(sizeof T / sizeof T[0]);
  return T;
}

// The NORMAL coordinate table (fix_plan A4) — the box table above with
// the Mod family (rows "L+R-diagonal-wavedash", "L-diagonal-23deg",
// "L-horizontal-walk", "L-vertical-tilt") REMOVED and the surviving
// rows' mod requirement relaxed to "any", because in this style L is
// not Mod at all: it is THE shield button, R holding the C-layer instead
// (see ctl_roles below; fix_plan A3 + DEVIATIONS D31/D32). Every magnitude here already appears in the
// ratified box table — 1.0 cardinals, 0.7000 diagonals, and the 0.6875
// shield-drop y. NO new coordinate was invented for this style.
// Same first-match-wins order: clayer > shield-drop diagonal > plain
// diagonal > horizontal > vertical.
static inline const S1ChordRow *ctl_normal_chord_table(int *count) {
  static const S1ChordRow T[] = {
      // --- C-layer (held R since D32): identical to box (the only
      //     C-stick this device can offer) -----------------------------
      {true, -1, -1, S1_PAD_H, 0, 1.0, 0.0, "clayer-horizontal"},
      {true, -1, -1, S1_PAD_V, 0, 0.0, 1.0, "clayer-vertical"},
      {true, -1, -1, S1_PAD_DIAG, 0, 0.7000, 0.7000, "clayer-diagonal"},
      // --- shield + down-diagonal = shield drop (and a legal wavedash
      //     angle). L is the shield shoulder in this style (D31). -------
      {false, -1, 1, S1_PAD_DIAG, -1, 0.7000, 0.6875, "shield-down-diagonal-shield-drop"},
      {false, -1, -1, S1_PAD_DIAG, 0, 0.7000, 0.7000, "plain-diagonal"},
      // --- plain full-range stick: no Mod band in this style ---------
      {false, -1, -1, S1_PAD_H, 0, 1.0, 0.0, "plain-horizontal-dash"},
      // --- shield + straight down = (0,-1.0) spotdodge, same row -----
      {false, -1, -1, S1_PAD_V, 0, 0.0, 1.0, "plain-vertical"},
  };
  *count = (int)(sizeof T / sizeof T[0]);
  return T;
}

// The NATURAL coordinate table (owner ruling 2026-07-29; C-layer rows
// added 2026-08-24, DEVIATION D32) — the ssb64 scheme's stick plane:
// FULL deflection, nothing else. No Mod family. There is no DEDICATED
// shield-drop row either, but the drop is still reachable: GUARD's PASS
// arm needs lsY < -0.65 while its spotdodge arm needs a STRICT
// lsY < -0.7, so the plain -0.7 diagonal lands in the [-0.70,-0.65) drop
// band (GUARD.c:79-99; review-ctl n1 corrected an earlier claim that
// this was lost). No row inspects mod OR shield: Natural has no Mod, and
// its single shield shoulder (L, D31) is not a table dimension here.
// Every magnitude already appears in the ratified BOX table AND in this
// table's own plain rows: 1.0 cardinals, 0.7000 diagonals — the C-layer
// rows invent NO coordinate, they re-emit the same three onto cs.
static inline const S1ChordRow *ctl_natural_chord_table(int *count) {
  static const S1ChordRow T[] = {
      // --- C-layer (held R, D32) — d-pad drives the C-stick ----------
      {true, -1, -1, S1_PAD_H, 0, 1.0, 0.0, "natural-clayer-horizontal"},
      {true, -1, -1, S1_PAD_V, 0, 0.0, 1.0, "natural-clayer-vertical"},
      {true, -1, -1, S1_PAD_DIAG, 0, 0.7000, 0.7000, "natural-clayer-diagonal"},
      {false, -1, -1, S1_PAD_DIAG, 0, 0.7000, 0.7000, "natural-diagonal"},
      {false, -1, -1, S1_PAD_H, 0, 1.0, 0.0, "natural-horizontal"},
      {false, -1, -1, S1_PAD_V, 0, 0.0, 1.0, "natural-vertical"},
  };
  *count = (int)(sizeof T / sizeof T[0]);
  return T;
}

// Style -> chord table. Total over CtlStyle by construction; an
// out-of-domain style is a caller bug, not a runtime condition
// (ctl_style_set refuses those), so it resolves to the default.
static inline const S1ChordRow *ctl_style_table(CtlStyle style, int *count) {
  switch (style) {
  case CTL_STYLE_BOX: return s1_chord_table(count);
  case CTL_STYLE_NORMAL: return ctl_normal_chord_table(count);
  case CTL_STYLE_NATURAL:
  default: return ctl_natural_chord_table(count);
  }
}

// Does this style drive the C-stick from a held SHOULDER (R)? NATURAL
// and CLASSIC yes; BOX spends R on Mod instead (DEVIATION D32).
//
// THE ARITHMETIC, because it is what forces this. The pad has 8 buttons;
// START is pause and MENU is the pause menu, so SIX reach gameplay. The
// roles wanted are seven — attack, special, jump, grab, shield, Mod,
// C-layer — so no style can carry them all. Once grab is a real button
// (D33, owner re-ratification) the styles that have no Mod fit exactly
// six roles in six buttons and keep the C-layer; BOX, which spends a
// shoulder on Mod (D30, owner), is the one that cannot.
static inline bool ctl_style_has_clayer(CtlStyle style) {
  return style != CTL_STYLE_BOX;
}

// The ROLE resolution, in ONE place (fix_plan A3 + the 2026-07-29
// Mod-shoulder swap). `modOnR` is orthogonal to style: it names which
// shoulder carries Mod in BOX, and is a no-op in NATURAL/NORMAL where
// both shoulders shield.
//   BOX     : Mod on one shoulder, shield on the other (swappable).
//   NORMAL  : no Mod; L shields (D31), R holds the C-layer (D32).
//   NATURAL : no Mod; L shields (D31), R holds the C-layer (D32).
//
// L-ONLY SHIELDING (DEVIATION D31, owner 2026-08-24: "L-only shielding
// is totally fine. I want it in fact.") is what FREES R on the non-BOX
// styles, and R carrying the C-layer (D32) is what frees Y for SPECIAL
// (D33). The three changes are one chain, in that order. BOX is
// untouched by D31 — it has always split the two shoulders.
static inline void ctl_roles(CtlStyle style, bool modOnR,
                             const PlatformInput *p, bool *clayer,
                             bool *mod, bool *shield) {
  *clayer = ctl_style_has_clayer(style) ? p->r : false;
  if (style == CTL_STYLE_BOX) {
    *mod = modOnR ? p->r : p->l;
    *shield = modOnR ? p->l : p->r;
  } else {
    *mod = false;
    *shield = p->l;
  }
}

// Resolved chord state (exposed for the unit sweep: row identity +
// pre-deaden quantized coordinates).
typedef struct {
  int dx, dy;                 // SOCD-resolved d-pad
  bool clayer, mod, shield;
  double lsX, lsY, csX, csY;  // quantized, PRE-deaden (raw* plane)
  const char *row;            // matched row name, or "neutral"
} S1Resolved;

// meleeRound == the 1/80 quantizer (melee_inputs.h; js_round semantics).
static inline double s1_q(double x) { return meleeRound(x); }

static inline S1Resolved s1_resolve_style(const PlatformInput *p,
                                          CtlStyle style, bool modOnR) {
  S1Resolved r;
  // SOCD: opposite cardinals -> NEUTRAL axis (meleelight's own keyboard
  // policy; unpressable on the physical cross d-pad).
  r.dx = (p->right ? 1 : 0) - (p->left ? 1 : 0);
  r.dy = (p->up ? 1 : 0) - (p->down ? 1 : 0);
  if (p->right && p->left) r.dx = 0;
  if (p->up && p->down) r.dy = 0;
  ctl_roles(style, modOnR, p, &r.clayer, &r.mod, &r.shield);
  r.lsX = 0.0;
  r.lsY = 0.0;
  r.csX = 0.0;
  r.csY = 0.0;
  r.row = "neutral";
  if (r.dx == 0 && r.dy == 0) return r; // no d-pad: sticks neutral
  const S1Pad pad = (r.dx != 0 && r.dy != 0) ? S1_PAD_DIAG
                    : (r.dx != 0)            ? S1_PAD_H
                                             : S1_PAD_V;
  const int dySign = r.dy < 0 ? -1 : (r.dy > 0 ? 1 : 0);
  int n = 0;
  const S1ChordRow *T = ctl_style_table(style, &n);
  for (int i = 0; i < n; i++) {
    const S1ChordRow *row = &T[i];
    if (row->clayer != r.clayer) continue;
    if (row->mod >= 0 && row->mod != (r.mod ? 1 : 0)) continue;
    if (row->shield >= 0 && row->shield != (r.shield ? 1 : 0)) continue;
    if (row->pad != pad) continue;
    if (row->dySign != 0 && row->dySign != dySign) continue;
    // signs come from the d-pad; magnitudes from the row. dx/dy are
    // ints, magnitudes positive: a -0 coordinate cannot be produced.
    const double x = s1_q((double)r.dx * row->magX);
    const double y = s1_q((double)r.dy * row->magY);
    if (r.clayer) {
      r.csX = x;
      r.csY = y;
    } else {
      r.lsX = x;
      r.lsY = y;
    }
    r.row = row->name;
    return r;
  }
  // Each style's table is total over the reachable
  // {clayer} x {mod} x {shield} x {pad} x {dySign} space (the
  // exhaustive 2048-combo sweep, per style, proves it) — reaching here
  // is a table defect, not a runtime condition.  NOTE the shield and
  // dySign dimensions: the BOX shield-drop row is dySign-guarded, and
  // NORMAL's shield dimension is fed by L||R, not R alone.
  fprintf(stderr, "s1_input: no chord row matched (style=%d modOnR=%d "
                  "clayer=%d mod=%d shield=%d dx=%d dy=%d)\n",
          (int)style, (int)modOnR, (int)r.clayer, (int)r.mod, (int)r.shield,
          r.dx, r.dy);
  abort();
}

// The ratified S1 (== BOX) resolver, pinned. Callers that predate the
// A4 style selector keep this exact behaviour.
static inline S1Resolved s1_resolve(const PlatformInput *p) {
  return s1_resolve_style(p, CTL_STYLE_BOX, false);
}

// The analog level physical X presses into the LEFT trigger to synthesise
// Melee's Z button (DEVIATION D34, fix_plan A42). 49/140 counts is the
// B0XX/HayBox LIGHT SHIELD level, cited verbatim from
// docs/research/b0xx-mapping.md §2.2 ("light shield (analog trigger ≈
// 49/140 counts) ... Mod X turns L into an analog 'Z-light' press").
// Two bounds make it load-bearing, both measured against sim code read
// this session, not chosen for taste:
//   > 0  — DASH.c:72/80, RUN.c:60 and KNEEBEND.c:66 gate their grab arms
//          on `lA > 0 || rA > 0`. A zero here reaches no grab at all.
//   < 1  — GUARDON.c:21 arms powerShieldActive on `max(lA,rA) === 1`.
//          A 1.0 here would powershield on every single grab press.
#define S1_ZGRAB_LA (49.0 / 140.0)

// The pollInputs-seam row: a complete 22-field FINAL Melee-unit Input.
// Mirrors the prototype funkeyPoll assembly exactly: ls/cs deadened
// (0.28 — a structural no-op for every nonzero table value, kept for
// parity), raw* carry the pre-deaden quantized values, digital shield
// r=true rA=1.0. du/dl/dr/dd and z are never set by ANY style; l/lA carry
// the X-grab synthesis below.
static inline MlInput s1_input_row_style(const PlatformInput *p,
                                         CtlStyle style, bool modOnR) {
  const S1Resolved r = s1_resolve_style(p, style, modOnR);
  MlInput in = nullInput();
  // BUTTON plane — STYLE-INDEPENDENT since the 2026-08-24 owner
  // re-ratification (DEVIATION D33: "X->grab, A->jump, Y->special,
  // B->attack", and "wtf you can't grab on box?? we want to be able
  // to"). Every style now emits the same four face roles; only the
  // SHOULDERS still differ by style (ctl_roles above). in.y is left
  // unset: Melee treats X and Y alike, so one jump field is enough and
  // a second would only be a second name for the same bit.
  //
  // X = GRAB is DEVIATION D34, and it replaces a defect. D33 shipped
  // `in.z = p->x` under the comment "Z: grab (and lightshield-grab
  // upstream)". That sentence is TRUE OF REAL MELEE AND FALSE OF THIS
  // ENGINE, and the owner found it the only way left: he pressed X and
  // nothing happened. MEASURED over the whole sim this session, every
  // reader of MlInput.z is {FORWARD,UP,DOWN}SMASH.c's `i0->a || i0->z`,
  // action_state_shortcuts.c:522's checkForAerials `(a edge)||(z edge)`,
  // and physics.c:983's lCancelUpdate — so `z` is an ALTERNATE ATTACK
  // button and an L-cancel trigger. It dispatches GRAB exactly ZERO
  // times. The five real grab arms are GUARD.c:75 / GUARDON.c:101
  // (`a` edge while shielding), DASH.c:80, RUN.c:60 and KNEEBEND.c:66
  // (`a` edge + `lA>0||rA>0`).
  //
  // So X synthesises WHAT MELEE'S Z ACTUALLY IS: A + a light shield.
  // That single chord reaches every one of those arms, because WAIT.c:54
  // and DASH.c:72 both take their `lA>0||rA>0` arm INTO GUARDON, whose
  // init->main->interrupt chain runs inside the SAME tick and still sees
  // the `a` edge (GUARDON.c:28 / :64). Standing, shielding, dashing,
  // running and in jumpsquat all grab. Airborne, checkForAerials' arm
  // fires first, so X is an aerial — which is also what Z does.
  //
  // NOTHING IS TRADED AWAY. Every old `z` reader listed above is an
  // `a || z` or a `(a edge) || (z edge)`, so `in.a` covers them
  // identically, and lCancelUpdate's `lA` edge covers its third arm — X
  // keeps its alternate-attack and L-cancel roles by construction. `z`
  // is therefore dropped rather than kept as a second name for a bit
  // `a` already sets, and the comment that misled two agents goes with
  // it. port/gfx/ctl_seam_witness.c is what proves this paragraph.
  in.a = p->b || p->x; // ATTACK (and the A half of X's Z synthesis)
  in.b = p->y;         // SPECIAL
  in.x = p->a;         // JUMP
  in.s = p->start;
  in.lsX = deaden(r.lsX, ml_deadzoneConst());
  in.lsY = deaden(r.lsY, ml_deadzoneConst());
  in.csX = deaden(r.csX, ml_deadzoneConst());
  in.csY = deaden(r.csY, ml_deadzoneConst());
  in.rawX = r.lsX;
  in.rawY = r.lsY;
  in.rawcsX = r.csX;
  in.rawcsY = r.csY;
  if (r.shield) {
    in.r = true;
    in.rA = 1.0;
  }
  // The LIGHT SHIELD half of X's Z synthesis (D34). It goes on the LEFT
  // trigger so it composes rather than fights: a player holding the
  // shield shoulder (r/rA=1.0) and pressing X still shields at full
  // strength, because every consumer reads max(lA,rA)
  // (action_state_shortcuts.c:299/324).
  if (p->x) in.lA = S1_ZGRAB_LA;
  return in;
}

// The ratified S1 (== BOX) seam row, pinned. Every pre-A4 call site and
// the 15 pinned PLAN §6 checks keep this exact behaviour.
static inline MlInput s1_input_row(const PlatformInput *p) {
  return s1_input_row_style(p, CTL_STYLE_BOX, false);
}

// --- A4 HANDOVER (Controls screen, menus lane) — STATUS -----------------
// Steps 1 and 2 have LANDED (punch-list C30(a), match-exit closure lane).
// What is live today, and what deliberately is not:
//
//   1. DONE — port/gfx/ctl_style.c is on the FOH link line(s).
//   2. DONE, and it is TWO sites, not three. The PRODUCT path is
//      port/foh/foh_dev.c: the target match loop and the VS match loop both
//      call s1_input_row_style(..., ctl_style_get(), ctl_mod_on_r_get()).
//      The two sites do NOT pass the same struct, and that is deliberate:
//      the target loop passes the raw poll `&pin` (foh_dev.c:2610), while the
//      VS loop passes `&gpin` (foh_dev.c:3221), the START-MASKED copy — VS START opens
//      the pause overlay, so it must not also reach the sim as a row bit.
//      Same style plumbing, two different input structs.
//      port/gfx/gfx_app.c's live arm stays on s1_input_row() BY DESIGN — it
//      is not an unfinished site. That binary is the EVIDENCE rig, the
//      producer judge-s1-coverage.js reads, and that judge is BOX-only by
//      construction: its pre-registered chord signatures are the Mod family
//      (walk, tilt, 23deg diagonal, wavedash), which ONLY the BOX table
//      emits. Style-switching that producer would trade the whole Mod
//      family for one C-layer plane — a net coverage LOSS — so the styles
//      are proven host-side by .loop/ctl-style-check.sh and by
//      port/gfx/check-ctl-input.sh instead. Do NOT "finish" gfx_app.c.
//      (Its C-layer signatures moved OUT of that judge on 2026-08-24: BOX
//      no longer has a C-layer at all — DEVIATION D32 — so the cs plane is
//      pinned bit-exactly host-side by s1_sweep.c's checks 11-13, which now
//      run under CLASSIC. Full accounting: MENU-SPEC DEVIATION D32.)
//   3. NOT DONE — the Controls SCREEN itself. This is the remaining C30(c)
//      work and it belongs to the menus lane, not to this header. The play
//      path CONSUMES the cells (ctl_style_get / ctl_mod_on_r_get above), but
//      ctl_style_set/ctl_mod_on_r_set have exactly ONE caller (foh_dev.c's
//      persist chokepoint, :1304) and no FOH UI caller at all, so a player
//      cannot yet change style in-game; only a persisted record can set it.
//      When that screen lands it needs TWO independent rows:
//        style:    ctl_style_set(0..CTL_STYLE_COUNT-1) / ctl_style_name()
//        shoulder: ctl_mod_on_r_set(bool) / ctl_mod_shoulder_name(bool)
//      (the shoulder row only changes BOX; it is a no-op in the other
//      two styles, so the screen may grey it out when style != Box).
//   4. DONE — the persist round trip. after foh_persist_load(&p):
//        ctl_style_set(p.ctlStyle); ctl_mod_on_r_set(p.modOnR != 0);
//      before foh_persist_save(&p):
//        p.ctlStyle = (int)ctl_style_get(); p.modOnR = ctl_mod_on_r_get();
#endif // GFX_S1_INPUT_H
