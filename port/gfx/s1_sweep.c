// port/gfx/s1_sweep.c — S1 chord-table unit sweep (M3 task 5).
//
// Host-side harness over the S1 layer (s1_input.h) — no sim, no SDL:
//   1. Asserts the 15 pinned PLAN §6 chord→coordinate checks (the
//      iter-51 pre-registered enumeration, AGENT-LOG) with BIT-EXACT
//      double equality against independently written literals.
//   2. Exhaustively dumps all 2^11 button combos (d-pad U/D/L/R +
//      Y/L/R + a/b/x/start) as canonical lines (booleans 0/1, doubles
//      as 16-hex-digit big-endian IEEE-754 bit patterns) — the check
//      script runs it twice and cmp's the dumps (byte stability).
//   3. Asserts EVERY emitted coordinate sits on the 1/80 Melee grid
//      (meleeRound(v) bit-== v) and the S1 invariants: y/z/l and the
//      d-pad booleans never set, lA == 0, rA ∈ {0,1} tied to r,
//      ls* == deaden(raw*), C-layer rows keep the left stick neutral.
//
// Exit 0 + final line "S1 SWEEP OK ..." only if everything holds.
#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "s1_input.h"

static int g_fail = 0;

static uint64_t dbits(double x) {
  uint64_t b;
  memcpy(&b, &x, 8);
  return b;
}

static bool deq(double a, double b) { return dbits(a) == dbits(b); }

// Build a PlatformInput from a bitmask (bit order fixed = dump order).
static PlatformInput combo(unsigned m) {
  PlatformInput p;
  memset(&p, 0, sizeof p);
  p.up = (m >> 0) & 1;
  p.down = (m >> 1) & 1;
  p.left = (m >> 2) & 1;
  p.right = (m >> 3) & 1;
  p.y = (m >> 4) & 1; // C-layer
  p.l = (m >> 5) & 1; // Mod
  p.r = (m >> 6) & 1; // shield
  p.a = (m >> 7) & 1;
  p.b = (m >> 8) & 1;
  p.x = (m >> 9) & 1;
  p.start = (m >> 10) & 1;
  return p;
}

typedef struct {
  bool up, down, left, right, y, l, r;
} Pad;

static PlatformInput pad(Pad c) {
  PlatformInput p;
  memset(&p, 0, sizeof p);
  p.up = c.up;
  p.down = c.down;
  p.left = c.left;
  p.right = c.right;
  p.y = c.y;
  p.l = c.l;
  p.r = c.r;
  return p;
}

static void expect_ls(const char *name, PlatformInput p, double ex,
                      double ey) {
  const MlInput in = s1_input_row(&p);
  if (!deq(in.lsX, ex) || !deq(in.lsY, ey) || !deq(in.csX, 0.0) ||
      !deq(in.csY, 0.0)) {
    printf("FAIL %s: ls=(%.17g,%.17g) cs=(%.17g,%.17g) want ls=(%.17g,%.17g) cs=(0,0)\n",
           name, in.lsX, in.lsY, in.csX, in.csY, ex, ey);
    g_fail = 1;
  } else {
    printf("ok   %s ls=(%.4f,%.4f)\n", name, ex, ey);
  }
}

static void expect_cs(const char *name, PlatformInput p, double ex,
                      double ey) {
  const MlInput in = s1_input_row(&p);
  // C-layer: the LEFT STICK MUST BE NEUTRAL while cs is driven.
  if (!deq(in.csX, ex) || !deq(in.csY, ey) || !deq(in.lsX, 0.0) ||
      !deq(in.lsY, 0.0) || !deq(in.rawX, 0.0) || !deq(in.rawY, 0.0)) {
    printf("FAIL %s: cs=(%.17g,%.17g) ls=(%.17g,%.17g) want cs=(%.17g,%.17g) ls neutral\n",
           name, in.csX, in.csY, in.lsX, in.lsY, ex, ey);
    g_fail = 1;
  } else {
    printf("ok   %s cs=(%.4f,%.4f) ls neutral\n", name, ex, ey);
  }
}

int main(void) {
  // ---- the 15 pinned PLAN §6 checks (pre-registered enumeration) ----
  // 1. d-pad cardinal horizontal -> +-1.0 (dash)
  expect_ls("01 plain right (dash)", pad((Pad){.right = true}), 1.0, 0.0);
  expect_ls("01 plain left (dash)", pad((Pad){.left = true}), -1.0, 0.0);
  // 2. d-pad cardinal up -> +1.0
  expect_ls("02 plain up", pad((Pad){.up = true}), 0.0, 1.0);
  // 3. d-pad cardinal down -> -1.0
  expect_ls("03 plain down", pad((Pad){.down = true}), 0.0, -1.0);
  // 4. d-pad diagonal -> (+-0.7, +-0.7); R+up-diagonal falls through
  expect_ls("04 diag up-right", pad((Pad){.up = true, .right = true}), 0.7,
            0.7);
  expect_ls("04 diag down-left", pad((Pad){.down = true, .left = true}),
            -0.7, -0.7);
  expect_ls("04 R+up-diagonal (plain diagonal fallthrough)",
            pad((Pad){.up = true, .right = true, .r = true}), 0.7, 0.7);
  // 5. L + horizontal -> +-0.6625 (walk / f-tilt); L+R+cardinal same
  expect_ls("05 L+right (walk 0.6625)", pad((Pad){.right = true, .l = true}),
            0.6625, 0.0);
  expect_ls("05 L+left (walk -0.6625)", pad((Pad){.left = true, .l = true}),
            -0.6625, 0.0);
  expect_ls("05 L+R+right (plain Mod value — quirk registry)",
            pad((Pad){.right = true, .l = true, .r = true}), 0.6625, 0.0);
  // 6. L + vertical -> +-0.5375 (u-tilt / d-tilt)
  expect_ls("06 L+up (u-tilt 0.5375)", pad((Pad){.up = true, .l = true}),
            0.0, 0.5375);
  expect_ls("06 L+down (d-tilt -0.5375)", pad((Pad){.down = true, .l = true}),
            0.0, -0.5375);
  // 7. L + diagonal -> (+-0.7375, +-0.3125)
  expect_ls("07 L+diag up-right (~23deg)",
            pad((Pad){.up = true, .right = true, .l = true}), 0.7375, 0.3125);
  expect_ls("07 L+diag down-left",
            pad((Pad){.down = true, .left = true, .l = true}), -0.7375,
            -0.3125);
  // 8. L + R + down-diagonal -> (+-0.6375, -0.375) (shallow wavedash);
  //    prototype applies the family to up-diagonals too (verbatim)
  expect_ls("08 L+R+down-right (wavedash 30deg)",
            pad((Pad){.down = true, .right = true, .l = true, .r = true}),
            0.6375, -0.375);
  expect_ls("08 L+R+down-left",
            pad((Pad){.down = true, .left = true, .l = true, .r = true}),
            -0.6375, -0.375);
  expect_ls("08 L+R+up-right (same family, verbatim prototype)",
            pad((Pad){.up = true, .right = true, .l = true, .r = true}),
            0.6375, 0.375);
  // 9. R + down-diagonal -> (+-0.7, -0.6875) (shield drop)
  expect_ls("09 R+down-right (shield drop)",
            pad((Pad){.down = true, .right = true, .r = true}), 0.7, -0.6875);
  expect_ls("09 R+down-left (shield drop)",
            pad((Pad){.down = true, .left = true, .r = true}), -0.7, -0.6875);
  // 10. R + straight down -> (0, -1.0) (spotdodge)
  expect_ls("10 R+down (spotdodge)", pad((Pad){.down = true, .r = true}),
            0.0, -1.0);
  // 11. Y-layer + horizontal -> csX +-1.0, left stick neutral
  expect_cs("11 clayer right", pad((Pad){.right = true, .y = true}), 1.0,
            0.0);
  expect_cs("11 clayer left", pad((Pad){.left = true, .y = true}), -1.0,
            0.0);
  // 12. Y-layer + vertical -> csY +-1.0
  expect_cs("12 clayer up", pad((Pad){.up = true, .y = true}), 0.0, 1.0);
  expect_cs("12 clayer down", pad((Pad){.down = true, .y = true}), 0.0,
            -1.0);
  // 13. Y-layer + diagonal -> (+-0.7, +-0.7) on cs
  expect_cs("13 clayer diag up-right",
            pad((Pad){.up = true, .right = true, .y = true}), 0.7, 0.7);
  expect_cs("13 clayer diag down-left",
            pad((Pad){.down = true, .left = true, .y = true}), -0.7, -0.7);
  // 14. SOCD: opposite cardinals -> neutral axis
  expect_ls("14 SOCD left+right", pad((Pad){.left = true, .right = true}),
            0.0, 0.0);
  expect_ls("14 SOCD up+down", pad((Pad){.up = true, .down = true}), 0.0,
            0.0);
  {
    // SOCD on one axis leaves the other live: L+R+up -> plain up
    expect_ls("14 SOCD left+right+up (vertical survives)",
              pad((Pad){.left = true, .right = true, .up = true}), 0.0, 1.0);
  }
  // 15. digital shield: r=true rA=1.0 (l=false lA=0)
  {
    PlatformInput p = pad((Pad){.r = true});
    const MlInput in = s1_input_row(&p);
    if (in.r != true || !deq(in.rA, 1.0) || in.l != false ||
        !deq(in.lA, 0.0)) {
      printf("FAIL 15 digital shield: r=%d rA=%.17g l=%d lA=%.17g\n",
             (int)in.r, in.rA, (int)in.l, in.lA);
      g_fail = 1;
    } else {
      printf("ok   15 digital shield r=true rA=1.0\n");
    }
  }

  // ---- exhaustive 2^11 dump + invariants --------------------------------
  int rows = 0;
  (void)s1_chord_table(&rows);
  printf("TABLE %d rows\n", rows);
  int grid_bad = 0, inv_bad = 0;
  for (unsigned m = 0; m < 2048; m++) {
    const PlatformInput p = combo(m);
    const S1Resolved r = s1_resolve(&p);
    const MlInput in = s1_input_row(&p);
    // invariants
    if (in.y || in.z || in.l || in.du || in.dl || in.dr || in.dd)
      inv_bad++;
    if (!deq(in.lA, 0.0)) inv_bad++;
    if (p.r ? (!in.r || !deq(in.rA, 1.0)) : (in.r || !deq(in.rA, 0.0)))
      inv_bad++;
    if (in.a != p.a || in.b != p.b || in.x != p.x || in.s != p.start)
      inv_bad++;
    if (!deq(in.lsX, deaden(in.rawX, ml_deadzoneConst())) ||
        !deq(in.lsY, deaden(in.rawY, ml_deadzoneConst())) ||
        !deq(in.csX, deaden(in.rawcsX, ml_deadzoneConst())) ||
        !deq(in.csY, deaden(in.rawcsY, ml_deadzoneConst())))
      inv_bad++;
    if (p.y && (!deq(in.lsX, 0.0) || !deq(in.lsY, 0.0))) inv_bad++;
    // 1/80 grid: every emitted coordinate quantizes to itself
    const double v[8] = {in.lsX, in.lsY, in.csX, in.csY,
                         in.rawX, in.rawY, in.rawcsX, in.rawcsY};
    for (int k = 0; k < 8; k++) {
      if (!deq(meleeRound(v[k]), v[k])) grid_bad++;
    }
    printf("C %04u %d%d%d%d%d%d%d%d%d%d%d %s "
           "%016" PRIx64 " %016" PRIx64 " %016" PRIx64 " %016" PRIx64 " "
           "%016" PRIx64 " %016" PRIx64 " %016" PRIx64 " %016" PRIx64 " "
           "%d%d%d%d%d%d\n",
           m, (int)p.up, (int)p.down, (int)p.left, (int)p.right, (int)p.y,
           (int)p.l, (int)p.r, (int)p.a, (int)p.b, (int)p.x, (int)p.start,
           r.row, dbits(in.lsX), dbits(in.lsY), dbits(in.csX),
           dbits(in.csY), dbits(in.rawX), dbits(in.rawY), dbits(in.rawcsX),
           dbits(in.rawcsY), (int)in.a, (int)in.b, (int)in.x, (int)in.s,
           (int)in.r, (int)deq(in.rA, 1.0));
  }
  if (grid_bad) {
    printf("FAIL grid: %d coordinates off the 1/80 grid\n", grid_bad);
    g_fail = 1;
  }
  if (inv_bad) {
    printf("FAIL invariants: %d violations across the combo sweep\n",
           inv_bad);
    g_fail = 1;
  }
  if (g_fail) {
    printf("S1 SWEEP FAIL\n");
    return 1;
  }
  printf("S1 SWEEP OK (15 pinned chord checks, 2048 combos, all "
         "coordinates on the 1/80 grid)\n");
  return 0;
}
