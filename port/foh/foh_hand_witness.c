// port/foh/foh_hand_witness.c — the FREE HAND ON TARGET-SELECT witness
// (punch-list A25(c), owner playthrough #3; MENU-SPEC DEVIATION D29).
//
// THE OWNER'S ASK, verbatim: *"here I want to utilize our cursor logic like we
// have at the character select screen where it's a free moving cursor (with
// the hand pointing). want to keep things DRY here — this needs to be spec'd
// out and in a smart way."*
//
// So this witness asserts what HE would see, and nothing weaker:
//   [1] target-select opens with the hand parked on TARGET 1, so the screen
//       still has a selection the moment it appears;
//   [2] holding a direction moves the hand CONTINUOUSLY — a sub-slot step
//       every frame, not a jump per press — and the selection follows it
//       through the slots it crosses;
//   [3] the hand can be parked in the 3 px gap BETWEEN two slots, where it
//       hovers nothing and the selection STICKS (D4's rule, inherited free
//       from the CSS by sharing foh_hand_hit's strict comparisons);
//   [4] every one of the ELEVEN slots — both columns, and "+ ADD CODE" — is
//       reachable by walking the hand, and each one hovers ITSELF;
//   [5] A on the hovered slot LAUNCHES THAT SLOT (`tstage` == the slot the
//       hand is over), and A over "+ ADD CODE" refuses instead;
//   [6] the hand is drawn, and it is CLAMPED to the screen, so it can never be
//       walked somewhere the player cannot see or recover it;
//   [7] the RENDERER rings the slot the hand is in, at the rect the hit test
//       used — the D4 obligation the extraction is supposed to make structural
//       (foh_tss_slots is the one table, foh.c and foh_render.c both read it).
//
// NOTHING IS HAND-POKED. Every position below is reached by feeding button
// levels to the REAL foh_tick and walking until the machine REPORTS it is
// where we wanted (the check-css-token-rest.sh feedback pattern); the
// navigation into target-select is the real menu path. The only literals are
// the ones the player's eye owns — slot indices and the launch record.
//
// Prints one line per assertion and `HAND OK` on success, exit 0; any failure
// prints `HAND FAIL: ...` and exits 1. Its negative tests live in
// port/foh/check-hand.sh.
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "foh.h"

#include "../gfx/raster.h"

void gfx_fatal(const char *what) {
  fprintf(stderr, "HAND FAIL: gfx_fatal: %s\n", what);
  exit(3);
}

static int g_fails;
static void ok(const char *what) { printf("  ok  %s\n", what); }
static void bad(const char *what) {
  printf("HAND FAIL: %s\n", what);
  g_fails++;
}
static void want(int cond, const char *what) {
  if (cond) ok(what); else bad(what);
}

// --- driving the real machine ------------------------------------------------
static void tick_neutral(FohState *s, int n) {
  PlatformInput in;
  memset(&in, 0, sizeof in);
  for (int i = 0; i < n; i++) foh_tick(s, &in);
}

static void press(FohState *s, size_t off) {
  PlatformInput in;
  memset(&in, 0, sizeof in);
  *(bool *)((char *)&in + off) = true;
  foh_tick(s, &in);
  memset(&in, 0, sizeof in);
  foh_tick(s, &in);
}
#define PRESS(s, field) press((s), offsetof(PlatformInput, field))

// Hold `off` until `pred` holds, then release. A free cursor is LEVEL driven,
// so a held direction IS the gesture; the guard makes a walk that never
// arrives FAIL rather than silently continue from wherever it stopped.
static bool walk(FohState *s, size_t off, bool (*pred)(const FohState *)) {
  PlatformInput in;
  memset(&in, 0, sizeof in);
  *(bool *)((char *)&in + off) = true;
  for (int i = 0; i < 4000 && !pred(s); i++) foh_tick(s, &in);
  const bool got = pred(s);
  tick_neutral(s, 1);
  return got;
}
#define WALK(s, field, pred) walk((s), offsetof(PlatformInput, field), (pred))

// Walk the hand to a target POINT: hold the axis until the coordinate passes
// it. Two straight moves, which is exactly how a player reaches a slot.
static bool walk_to(FohState *s, double tx, double ty) {
  PlatformInput in;
  for (int axis = 0; axis < 2; axis++) {
    for (int i = 0; i < 4000; i++) {
      const double cur = axis == 0 ? s->tssHandX : s->tssHandY;
      const double want_ = axis == 0 ? tx : ty;
      if (cur == want_ || (cur < want_ ? cur + 2.0 > want_ : cur - 2.0 < want_))
        break;
      memset(&in, 0, sizeof in);
      if (axis == 0) { if (cur < want_) in.right = true; else in.left = true; }
      else { if (cur < want_) in.down = true; else in.up = true; }
      foh_tick(s, &in);
    }
  }
  tick_neutral(s, 1);
  const double dx = s->tssHandX - tx, dy = s->tssHandY - ty;
  return (dx > -2.5 && dx < 2.5) && (dy > -2.5 && dy < 2.5);
}

// Boot -> title -> menu-top -> "Target Test" -> target-select, the real path.
static void to_tss(FohState *s) {
  foh_init(s);
  tick_neutral(s, 380);          // startUpTimer 370 (startup.js:50)
  PRESS(s, start);               // title -> menu-top (main.js:385)
  PRESS(s, down);                // menu row 0 -> row 1 (TARGET TEST)
  PRESS(s, a);                   // menu.js:77-84 -> target-select
  if (s->screen != FOH_TSS) {
    printf("HAND FAIL: could not reach target-select (screen %d)\n",
           (int)s->screen);
    exit(1);
  }
}

// --- assertion helpers -------------------------------------------------------
static FohHandRect g_slot[FOH_TSS_SLOTS];

static bool at_top(const FohState *s) { return s->tssHandY <= 0.0; }
static bool at_left(const FohState *s) { return s->tssHandX <= 0.0; }
static bool at_bottom(const FohState *s) { return s->tssHandY >= (double)RAST_H; }
static bool at_right(const FohState *s) { return s->tssHandX >= (double)RAST_W; }

static double mid_x(int k) { return g_slot[k].x + g_slot[k].w / 2.0; }
static double mid_y(int k) { return g_slot[k].y + g_slot[k].h / 2.0; }

static Raster g_a, g_b;
// The hand's rounded position in each of those two frames, measured at shot
// time so the sprite box below is where the blit actually went.
static int g_ax, g_ay, g_bx, g_by;

static void shot(const FohState *s, Raster *rz) {
  FohState look = *s;
  foh_look_canonical(&look);
  foh_render(&look, rz);
}

// Is (x,y) inside r, grown by `pad` on every side?
static int in_grown(const FohHandRect *r, int x, int y, int pad) {
  return x >= r->x - pad && x < r->x + r->w + pad && y >= r->y - pad &&
         y < r->y + r->h + pad;
}

int main(void) {
  foh_tss_slots(g_slot);
  FohState s;

  // --- [1] the screen opens with the hand on TARGET 1 -----------------------
  to_tss(&s);
  want(s.tssCursor == 0, "target-select opens with the hand hovering TARGET 1");
  {
    FohHandRect probe[FOH_TSS_SLOTS];
    foh_tss_slots(probe);
    want(foh_hand_hit(probe, FOH_TSS_SLOTS, s.tssHandX, s.tssHandY) == 0,
         "  ...and the hand is really INSIDE slot 0's drawn rect, not merely"
         " on an index that happens to read 0");
  }

  // --- [2] motion is CONTINUOUS, and one press is not one slot --------------
  {
    const double y0 = s.tssHandY;
    PlatformInput in;
    memset(&in, 0, sizeof in);
    in.down = true;
    foh_tick(&s, &in);
    const double step = s.tssHandY - y0;
    tick_neutral(&s, 1);
    want(step > 0.0 && step < (double)FOH_TSS_SLOT_ROW_PITCH,
         "one frame of DOWN moves the hand a FRACTION of a slot (a free"
         " cursor, not a per-press index step)");
    want(s.tssCursor == 0,
         "  ...and that single frame does NOT change the selection — the"
         " hand is still inside slot 0");
  }

  // --- [3] the selection follows the hand through the slots it crosses ------
  {
    to_tss(&s);
    int seen[5] = {0}, order = 0, bad_order = 0;
    PlatformInput in;
    memset(&in, 0, sizeof in);
    in.down = true;
    for (int i = 0; i < 200 && s.tssCursor <= 4; i++) {
      const int before = s.tssCursor;
      foh_tick(&s, &in);
      if (s.tssCursor != before) {
        if (s.tssCursor <= 4) {
          if (s.tssCursor != before + 1) bad_order = 1;
          seen[s.tssCursor] = 1;
          order++;
        }
      }
    }
    tick_neutral(&s, 1);
    want(seen[1] && seen[2] && seen[3] && seen[4] && order == 4 && !bad_order,
         "holding DOWN walks the selection 0 -> 1 -> 2 -> 3 -> 4 in order,"
         " one slot at a time, with no skips");
  }

  // --- [4] the gutter hovers NOTHING and the selection STICKS ---------------
  {
    to_tss(&s);
    // The 3 px gap between slot 0's bottom edge and slot 1's top edge.
    const double gy = (double)(g_slot[0].y + g_slot[0].h) + 1.5;
    want(walk_to(&s, mid_x(0), gy),
         "the hand can be parked in the gap between TARGET 1 and TARGET 2");
    FohHandRect probe[FOH_TSS_SLOTS];
    foh_tss_slots(probe);
    want(foh_hand_hit(probe, FOH_TSS_SLOTS, s.tssHandX, s.tssHandY) == -1,
         "  ...where it hovers NO slot (D4: no hit region where nothing is"
         " drawn — the CSS gutter rule, inherited)");
    want(s.tssCursor == 0,
         "  ...and the selection STICKS at TARGET 1 rather than clearing, so"
         " A always has a target and the records row always has a slot");
  }

  // --- [5] all ELEVEN slots are reachable, and each hovers ITSELF -----------
  {
    to_tss(&s);
    int reached = 0, wrong = 0;
    for (int k = 0; k < FOH_TSS_SLOTS; k++) {
      if (!walk_to(&s, mid_x(k), mid_y(k))) continue;
      reached++;
      if (s.tssCursor != k) wrong++;
    }
    want(reached == FOH_TSS_SLOTS && wrong == 0,
         "walking the hand to each of the 11 slots (both columns and"
         " + ADD CODE) selects that slot and no other");
  }

  // --- [6] A launches the slot the hand is OVER -----------------------------
  {
    // Slot 7 — column 1, row 2: nothing about it is a default, and it is not
    // where any index cursor would have started.
    to_tss(&s);
    want(walk_to(&s, mid_x(7), mid_y(7)), "the hand walks to TARGET 8");
    want(s.tssCursor == 7, "  ...and TARGET 8 is selected");
    // ONE tick, not PRESS's press+release pair: foh_tick clears the event and
    // sound queues at the top of every tick (foh.c:1280-1281, the driver
    // drains them per tick), so the release tick would erase what we came to
    // read.
    PlatformInput in;
    memset(&in, 0, sizeof in);
    in.a = true;
    foh_tick(&s, &in);
    want(s.launched && s.targetMode && s.tssStage == 7,
         "A launches the slot the HAND is over (tstage 7), not slot 0 and not"
         " some index the d-pad left behind");
    int sawLaunch = 0;
    for (int i = 0; i < s.nev; i++)
      if (s.ev[i].kind == FOH_EV_LAUNCH) sawLaunch = 1;
    want(s.screen == FOH_TMATCH && sawLaunch,
         "  ...and the launch reaches the trace as a real TLAUNCH event");
  }

  // --- [7] A over + ADD CODE refuses, and launches nothing ------------------
  {
    to_tss(&s);
    want(walk_to(&s, mid_x(10), mid_y(10)), "the hand walks to + ADD CODE");
    want(s.tssCursor == 10, "  ...and + ADD CODE is selected");
    PRESS(&s, a);
    want(!s.launched && s.screen == FOH_TSS,
         "A on + ADD CODE refuses (the builder plane is scope-excluded) and"
         " launches nothing");
  }

  // --- [8] the hand is clamped to the screen, so it is always recoverable ---
  {
    to_tss(&s);
    want(WALK(&s, up, at_top) && s.tssHandY == 0.0,
         "walking UP forever stops at the top edge (y == 0)");
    want(WALK(&s, left, at_left) && s.tssHandX == 0.0,
         "walking LEFT forever stops at the left edge (x == 0)");
    want(WALK(&s, down, at_bottom) && s.tssHandY == (double)RAST_H,
         "walking DOWN forever stops at the bottom edge");
    want(WALK(&s, right, at_right) && s.tssHandX == (double)RAST_W,
         "walking RIGHT forever stops at the right edge");
    // From the far corner the player can still get back to a slot.
    want(walk_to(&s, mid_x(0), mid_y(0)) && s.tssCursor == 0,
         "  ...and from that corner the hand walks back onto TARGET 1");
  }

  // --- [9] the hand is DRAWN, and it moves on screen ------------------------
  {
    to_tss(&s);
    walk_to(&s, mid_x(0), mid_y(0));
    // MEASURED, not assumed: walk_to lands within a step of the target, and
    // the sprite is blitted from the hand's ACTUAL rounded position. Using the
    // slot midpoint here instead cost a false failure of the [10] assertion
    // below, two pixels wide.
    g_ax = (int)(s.tssHandX + 0.5);
    g_ay = (int)(s.tssHandY + 0.5);
    shot(&s, &g_a);
    walk_to(&s, mid_x(9), mid_y(9));
    g_bx = (int)(s.tssHandX + 0.5);
    g_by = (int)(s.tssHandY + 0.5);
    shot(&s, &g_b);
    int diffs = 0, nearHandA = 0, nearHandB = 0;
    // The hot spot is the fingertip at sprite (10,7) of a 24x32 asset, so the
    // sprite covers [x-10, x+14) x [y-7, y+25).
    const int ax = g_ax, ay = g_ay, bx = g_bx, by = g_by;
    for (int y = 0; y < RAST_H; y++) {
      for (int x = 0; x < RAST_W; x++) {
        const int i = y * RAST_W + x;
        if (g_a.fb[i] == g_b.fb[i]) continue;
        diffs++;
        if (x >= ax - 10 && x < ax + 14 && y >= ay - 7 && y < ay + 25)
          nearHandA++;
        if (x >= bx - 10 && x < bx + 14 && y >= by - 7 && y < by + 25)
          nearHandB++;
      }
    }
    want(diffs > 0, "moving the hand changes the rendered frame");
    want(nearHandA > 40 && nearHandB > 40,
         "  ...and the change includes a sprite-sized patch at BOTH hand"
         " positions — the pointing hand is really drawn where it is");
  }

  // --- [10] the RING is drawn at the rect the hit test used (D4) ------------
  {
    // Two frames whose ONLY selection difference is slot 0 vs slot 9, and
    // whose hands sit at those two slots. Every changed pixel must lie in one
    // of those two slot rects (grown by the 2 px D24 ring) or under one of the
    // two hand sprites. If render_tss drew its slots anywhere other than
    // foh_tss_slots' rects, the ring pixels would land outside and this bites.
    int stray = 0;
    const int ax = g_ax, ay = g_ay, bx = g_bx, by = g_by;
    int ring0 = 0, ring9 = 0;
    for (int y = 0; y < RAST_H; y++) {
      for (int x = 0; x < RAST_W; x++) {
        const int i = y * RAST_W + x;
        if (g_a.fb[i] == g_b.fb[i]) continue;
        const int hand = (x >= ax - 10 && x < ax + 14 && y >= ay - 7 &&
                          y < ay + 25) ||
                         (x >= bx - 10 && x < bx + 14 && y >= by - 7 &&
                          y < by + 25);
        const int s0 = in_grown(&g_slot[0], x, y, 2);
        const int s9 = in_grown(&g_slot[9], x, y, 2);
        if (s0) ring0++;
        if (s9) ring9++;
        if (!hand && !s0 && !s9) stray++;
      }
    }
    want(stray == 0,
         "every pixel that changes between a slot-0 selection and a slot-9"
         " selection lies inside those two slots' OWN rects (or under the"
         " hand) — the renderer and the hit test read one table");
    want(ring0 > 100 && ring9 > 100,
         "  ...and both slots really are re-drawn (D24's 2 px ring and lifted"
         " body, not a stray pixel or two)");
  }

  if (g_fails) {
    printf("HAND FAIL: %d assertion(s) failed\n", g_fails);
    return 1;
  }
  printf("HAND OK\n");
  return 0;
}
