// port/gfx/s1_input.h — the S1 "One-Mod + C-layer" input layer (M3
// task 5; PLAN §6 verbatim, issues #6/#9, prototype
// prototypes/control-mapping/funkeyMapping.js — the verified resolver
// this header translates for scheme S1 ONLY: L=Mod, R=shield,
// Y(hold)=C-stick layer, A=attack, B=special, X=jump, Start=pause).
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
//   - left stick NEUTRAL while the Y C-layer is held (drift freezes —
//     documented S1 sacrifice).
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
#include "platform.h"           // PlatformInput

// d-pad shape classes after SOCD resolution.
typedef enum { S1_PAD_H = 0, S1_PAD_V = 1, S1_PAD_DIAG = 2 } S1Pad;

typedef struct {
  bool clayer;    // row requires the Y C-layer held (emits onto cs)
  int mod;        // L requirement: 1 held, 0 not held, -1 any
  int shield;     // R requirement: 1 held, 0 not held, -1 any
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
      // --- Y C-layer: d-pad drives the C-stick, left stick neutral ----
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

static inline S1Resolved s1_resolve(const PlatformInput *p) {
  S1Resolved r;
  // SOCD: opposite cardinals -> NEUTRAL axis (meleelight's own keyboard
  // policy; unpressable on the physical cross d-pad).
  r.dx = (p->right ? 1 : 0) - (p->left ? 1 : 0);
  r.dy = (p->up ? 1 : 0) - (p->down ? 1 : 0);
  if (p->right && p->left) r.dx = 0;
  if (p->up && p->down) r.dy = 0;
  r.clayer = p->y;
  r.mod = p->l;
  r.shield = p->r;
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
  const S1ChordRow *T = s1_chord_table(&n);
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
  // The table is total over {clayer} x {mod} x {pad} (the exhaustive
  // 2048-combo sweep proves it) — reaching here is a table defect.
  fprintf(stderr, "s1_input: no chord row matched (clayer=%d mod=%d "
                  "shield=%d dx=%d dy=%d)\n",
          (int)r.clayer, (int)r.mod, (int)r.shield, r.dx, r.dy);
  abort();
}

// The pollInputs-seam row: a complete 22-field FINAL Melee-unit Input.
// Mirrors the prototype funkeyPoll assembly exactly: ls/cs deadened
// (0.28 — a structural no-op for every nonzero table value, kept for
// parity), raw* carry the pre-deaden quantized values, digital shield
// r=true rA=1.0, y/z/l/du/dl/dr/dd never set by S1.
static inline MlInput s1_input_row(const PlatformInput *p) {
  const S1Resolved r = s1_resolve(p);
  MlInput in = nullInput();
  in.a = p->a;
  in.b = p->b;
  in.x = p->x;
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
  return in;
}

#endif // GFX_S1_INPUT_H
