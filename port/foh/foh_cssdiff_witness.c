// port/foh/foh_cssdiff_witness.c — the A25(c) EXTRACTION DIFFERENTIAL.
//
// A25(c)'s hard constraint is that the CSS comes out of the hand-cursor
// extraction BYTE-IDENTICAL: same doubles, same integration order, same clamp,
// same hit predicate. This driver is how that is PROVEN rather than argued.
//
// It is compiled TWICE by port/foh/check-hand.sh — once against the working
// tree, once against the PINNED pre-A25(c) commit — and the two dumps are
// `cmp`d. So this file may use ONLY the FOH surface that exists on BOTH sides:
// none of the extraction's own types, state fields or entry points. If a future
// edit reaches for them here, the old-tree build stops compiling and the check
// would quietly stop being a differential, so check-hand.sh leg [1] greps this
// file for each of those names and dies loudly first. That grep is textual, so
// do not name them even in a comment.
//
// WHY A SWEEP AND NOT A SCRIPT. The existing CSS checks (check-css-back.sh,
// check-css-token-rest.sh, check-css-mode.sh) drive SCRIPTED gestures: they
// are necessary — they say the screen still does the right things — but they
// are not SUFFICIENT for a refactor, because they only visit the states
// someone thought to write down. The refactor moved a clamp, an integration
// order and a hit predicate, so what has to be shown is that no reachable
// (position, gesture) pair moved at all. This drives ~60k frames of a fixed
// pseudorandom button stream through the REAL foh_tick and dumps, EVERY FRAME:
//   * the hand in its raw IEEE-754 bits (a one-ulp drift in the integration or
//     the clamp is a changed line — decimal printing could hide it);
//   * every CSS-observable machine field, plus the sound and event queues,
//     which is where a changed hit predicate would surface as a lost or extra
//     menuSelect;
//   * an FNV-1a hash of the RENDERED FRAME, which covers the foh_render.c half
//     of the refactor (the cell-hover `hot` flag) at full pixel resolution.
//
// TWO BUTTONS ARE WITHHELD, and both for the same reason — they are EXITS, and
// an exit spends the sweep's frames somewhere that is not the subject:
//   * START, because on a READY CSS it LAUNCHES, and FOH_MATCH is terminal to
//     foh_tick (the driver owns the match), so the first launch would end the
//     walk. Measured before this guard existed: 55,289 of 60,000 frames sat in
//     FOH_MATCH and only 4,686 were on the CSS.
//   * B and A while `bHold` has reached 25, which is D22's 30-frame back-out
//     arming window (the B button and the BACK wedge share the one counter).
// Both withholdings are deterministic and read only fields both builds have,
// so they cannot mask a divergence: check-css-back.sh owns the back edge and
// check-foh-flows.sh owns the launch, and neither arm is touched by A25(c).
// A fixed re-entry press is still fed if the machine leaves the CSS anyway, so
// a divergence in WHEN it leaves is itself a dumped difference rather than a
// dead walk.
//
// Usage: `foh_cssdiff <frames>` -> the dump on stdout, exit 0.
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "foh.h"

#include "../gfx/raster.h"

void gfx_fatal(const char *what) {
  fprintf(stderr, "CSSDIFF FAIL: gfx_fatal: %s\n", what);
  exit(3);
}

// The 12 platform buttons, in the fixed order the sweep drives them. Named by
// FIELD so the two builds cannot disagree about which bit is which.
static bool *field(PlatformInput *in, int k) {
  switch (k) {
    case 0: return &in->up;
    case 1: return &in->down;
    case 2: return &in->left;
    case 3: return &in->right;
    case 4: return &in->a;
    case 5: return &in->b;
    case 6: return &in->x;
    case 7: return &in->y;
    case 8: return &in->l;
    case 9: return &in->r;
    case 10: return &in->start;
    default: return &in->menu;
  }
}

// xorshift32 — a fixed, portable, dependency-free stream. Not the sim's RNG on
// purpose: nothing here may touch the seeded chain.
static uint32_t g_rng = 0x5eed1337u;
static uint32_t nextr(void) {
  g_rng ^= g_rng << 13;
  g_rng ^= g_rng >> 17;
  g_rng ^= g_rng << 5;
  return g_rng;
}

static uint64_t bits(double d) {
  uint64_t u;
  memcpy(&u, &d, sizeof u);
  return u;
}

static uint64_t fnv1a(const void *p, size_t n) {
  const unsigned char *b = (const unsigned char *)p;
  uint64_t h = 1469598103934665603ull;
  for (size_t i = 0; i < n; i++) {
    h ^= b[i];
    h *= 1099511628211ull;
  }
  return h;
}

static Raster g_rz;

int main(int argc, char **argv) {
  long frames = 60000;
  if (argc > 1) frames = strtol(argv[1], NULL, 10);
  // `--no-a` suppresses the A button for the whole sweep. A44 added it for
  // check-hand.sh leg [4]'s CROSS-BUILD differential and for nothing else.
  //
  // WHY. Leg [4] replays this sweep against a pre-A25(c) build and requires
  // byte equality, which is only a true statement about surfaces BOTH builds
  // have. DEVIATION D40 gave the single hand the right to grab any port's
  // token and gave ports 2/3 live type tabs, so an A press can now reach
  // machine state the pinned base has no field for (measured: 34,150 of
  // 60,000 dumped frames diverge, first at frame 6001 where the new build
  // carries PORT 3's token — `c=3` — a port that does not exist over there).
  // Every one of those arms is inside an `if (aE)`, so with no A edge the
  // two builds are comparable again and the leg keeps its full dump.
  // The A-PRESSING sweep is still run — on the working tree, where leg [4]
  // measures its own coverage floors from it — and the cross-build claim for
  // the A-gated arms moved to port/foh/check-css-p34.sh, which drives grab,
  // drop and the type tabs on all four ports through the real foh_tick.
  bool noA = false;
  if (argc > 2 && strcmp(argv[2], "--no-a") == 0) noA = true;

  FohState s;
  foh_init(&s);

  // Reach the CSS honestly: the startup timer, START on the title, then A on
  // menu row 0 (`VS. Melee` -> the CSS, owner ruling C5).
  PlatformInput in;
  memset(&in, 0, sizeof in);
  for (int i = 0; i < 380; i++) foh_tick(&s, &in);
  in.start = true;
  foh_tick(&s, &in);
  memset(&in, 0, sizeof in);
  foh_tick(&s, &in);
  in.a = true;
  foh_tick(&s, &in);
  memset(&in, 0, sizeof in);
  foh_tick(&s, &in);
  if (s.screen != FOH_CSS) {
    fprintf(stderr, "CSSDIFF FAIL: did not reach the CSS (screen %d)\n",
            (int)s.screen);
    return 2;
  }

  // The sweep. Each button is re-rolled independently with a HOLD bias (7/8 to
  // keep its level), so directions are held for runs of frames — which is what
  // a free cursor actually gets, and what a per-frame coin flip would never
  // produce (a 50% d-pad barely moves the hand at all).
  bool held[12] = {false};
  for (long f = 0; f < frames; f++) {
    if (s.screen != FOH_CSS) {
      // Fixed re-entry (menu-top row 0 is `VS. Melee`), then carry on. Both
      // builds run it at the same frame or the dump already differs.
      memset(&in, 0, sizeof in);
      foh_tick(&s, &in);
      in.a = true;
      foh_tick(&s, &in);
      memset(&in, 0, sizeof in);
      foh_tick(&s, &in);
      memset(held, 0, sizeof held);
    }
    memset(&in, 0, sizeof in);
    for (int k = 0; k < 12; k++) {
      const uint32_t r = nextr();
      if ((r & 7u) == 0u) held[k] = ((r >> 8) & 1u) != 0u;
      *field(&in, k) = held[k];
    }
    in.start = false;              // never launch (see the header)
    if (noA) in.a = false;         // the cross-build sweep (see main's head)
    if (s.bHold >= 25) {           // never arm the 30-frame back-out
      in.b = false;
      in.a = false;
    }
    foh_tick(&s, &in);
    foh_render(&s, &g_rz);

    printf("%ld h=%016llx,%016llx t=%d c=%d,%d ch=%d,%d rest=%d,%d "
           "sl=%016llx,%016llx p=%d,%d,%d,%d,%d vm=%d bh=%d sc=%d "
           "nev=%d nsnd=%d",
           f, (unsigned long long)bits(s.cssHandX),
           (unsigned long long)bits(s.cssHandY), foh_css_hand_type(&s),
           s.cssCarry, s.cssCpuCarry, s.cssChar[0], s.cssChar[1],
           s.cssTokenRest[0], s.cssTokenRest[1],
           (unsigned long long)bits(s.cssSliderX[0]),
           (unsigned long long)bits(s.cssSliderX[1]), s.p1Char, s.p2Char,
           s.p1Type, s.p2Type, s.p1Difficulty, s.versusMode, s.bHold,
           (int)s.screen, s.nev, s.nsnd);
    for (int i = 0; i < s.nsnd; i++) printf(" s:%s", s.snd[i]);
    // The token positions are the D21/Q1 rest-slot arithmetic — a plane the
    // hit predicate feeds and the renderer draws, so it is dumped too.
    for (int k = 0; k < 2; k++) {
      double tx, ty;
      foh_css_token_pos(&s, k, &tx, &ty);
      printf(" tk%d=%016llx,%016llx", k, (unsigned long long)bits(tx),
             (unsigned long long)bits(ty));
    }
    printf(" fb=%016llx ink=%016llx\n",
           (unsigned long long)fnv1a(g_rz.fb, sizeof g_rz.fb),
           (unsigned long long)fnv1a(g_rz.ink, sizeof g_rz.ink));
  }
  return 0;
}
