// foh_launch_witness.c — the CSS LAUNCH-GUARD grid witness (codex review
// round 17/18 finding B1). A check-owned host leg for check-foh-flows.sh,
// built and linked exactly like foh_snd_witness.c (same [2] objects, same
// -Wl,-dead_strip link recipe).
//
// WHY. foh.c's CSS START arm carries the D6 launch-plane refusal:
//
//     if (!(s->p1Type == 0 && (s->p2Type == 0 || s->p2Type == 1))) {
//       snd_push(s, "deny"); ev_refused(s, "portconfig"); ...  return;
//     }
//
// Every committed flow drives the ONE configuration the shipped menu can
// actually reach (P1 HMN, P2 HMN or CPU), so the refusing side of that
// predicate had NO behavioral coverage at all: the arm could be deleted,
// inverted, or narrowed and every green flow would stay green. A static
// read of the condition is not coverage either — it is the same expression
// the code already contains, so restating it here would prove only that a
// string equals itself.
//
// WHAT THIS PROVES. The table below is AUTHORED, one row per cell of the
// full (p1Type, p2Type) grid over {-1, 0, 1} — the three values the type
// plane can hold (foh.h:110/:134: -1 = N/A, 0 = HMN, 1 = CPU). Each row
// states, by hand, what the port configuration MEANS and therefore what
// the machine owes: either a launch (menuForward + a css->sss transition
// caused by "start") or a refusal (deny + a `refused portconfig` selection
// event, with the screen NOT moving). Nothing in a row is computed from
// the predicate under test, so narrowing or widening the guard moves rows
// and this witness fails.
//
// The grid is driven through the REAL foh_tick with cssReady forced true.
// Forcing it is the point: the ready rule and the launch guard are two
// different gates (foh.c's own note — cssReady is LAST frame's draw-pass
// value, which is exactly why the N/A race reaches the guard at all), and
// this witness is about the second one. A cell that the ready rule would
// also have stopped is still a cell the guard must refuse on its own.
//
// Usage: foh_launch_witness   (no arguments)
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "foh.h"

// Host-provided fatals (raster.h / the sim slice), same stubs as the
// sound witness: a fatal here is a loud non-zero exit, never a pass.
void gfx_fatal(const char *what) {
  fprintf(stderr, "foh_launch_witness: gfx_fatal: %s\n", what);
  exit(3);
}
void sim_fatal(const char *what);
void sim_fatal(const char *what) {
  fprintf(stderr, "foh_launch_witness: sim_fatal: %s\n", what);
  exit(3);
}

static int g_fails;
static void bad(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
static void bad(const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  fprintf(stderr, "foh_launch_witness: FAIL: ");
  vfprintf(stderr, fmt, ap);
  fprintf(stderr, "\n");
  va_end(ap);
  g_fails++;
}

// Join the tick's emitted sound tokens, so a mismatch prints both sides.
static void join_snd(const FohState *s, char *out, size_t cap) {
  out[0] = 0;
  for (int i = 0; i < s->nsnd; i++) {
    if (i) strncat(out, ",", cap - strlen(out) - 1);
    strncat(out, s->snd[i], cap - strlen(out) - 1);
  }
}

typedef struct {
  int p1Type, p2Type;
  int wantLaunch;    // 1 = css->sss "start"; 0 = refused "portconfig"
  const char *snd;   // EXACT comma-joined sound tokens for the tick
  const char *why;   // the authored CLAIM this row makes, in words
} GridRow;

// THE AUTHORED GRID. Nine rows, one per cell, in row-major (p1, p2) order
// over {-1, 0, 1}. `wantLaunch` is written from what the configuration
// MEANS, never from the predicate:
//
//   p1 must be a HUMAN because the sim's setup pins port 0 to HMN with a
//   fixed level (foh.c's D6 note) — so every p1 != 0 cell owes a refusal,
//   whatever p2 holds.
//   p2 may be HUMAN or CPU because the sim carries p2Type and difficulty
//   — so those two cells owe a launch.
//   p2 == -1 (N/A) owes a refusal even under a HUMAN p1: a one-port match
//   is a configuration the ready rule would have rejected, and it reaches
//   the guard only through upstream's own one-frame draw-pass race.
static const GridRow kGrid[] = {
    {-1, -1, 0, "deny", "P1 N/A: no human in port 0, and no second port either"},
    {-1, 0, 0, "deny", "P1 N/A: port 0 must be the human the sim pins"},
    {-1, 1, 0, "deny", "P1 N/A with a CPU P2: still no human port 0"},
    {0, -1, 0, "deny", "P2 N/A: a one-port match the ready rule would refuse"},
    {0, 0, 1, "menuForward", "HMN vs HMN: the shipped configuration"},
    {0, 1, 1, "menuForward", "HMN vs CPU: the sim carries p2Type + difficulty"},
    {1, -1, 0, "deny", "P1 CPU: the sim would boot a HUMAN P1 (a lie)"},
    {1, 0, 0, "deny", "P1 CPU vs a human P2: the port roles are swapped"},
    {1, 1, 0, "deny", "CPU vs CPU: no human port at all"},
};

int main(void) {
  int nLaunch = 0, nRefuse = 0;
  char got[128];
  for (size_t i = 0; i < sizeof kGrid / sizeof kGrid[0]; i++) {
    const GridRow *r = &kGrid[i];
    FohState s;
    foh_init(&s);
    s.screen = FOH_CSS;
    // The launch guard is what is under test, not the ready rule: force
    // the gate in front of it open so every cell actually reaches it.
    s.cssReady = true;
    s.p1Type = r->p1Type;
    s.p2Type = r->p2Type;
    memset(&s.prev, 0, sizeof s.prev); // all released -> START is a rising edge
    PlatformInput in;
    memset(&in, 0, sizeof in);
    in.start = true;
    foh_tick(&s, &in);

    join_snd(&s, got, sizeof got);
    if (strcmp(got, r->snd) != 0) {
      bad("(p1=%d,p2=%d) sound plane is '%s', want '%s' [%s]", r->p1Type,
          r->p2Type, got, r->snd, r->why);
    }
    if (s.nev != 1) {
      bad("(p1=%d,p2=%d) emitted %d events, want exactly 1 [%s]", r->p1Type,
          r->p2Type, s.nev, r->why);
      continue;
    }
    const FohEvent *e = &s.ev[0];
    if (r->wantLaunch) {
      nLaunch++;
      if (e->kind != FOH_EV_TRANS || !e->from || !e->to || !e->cause ||
          strcmp(e->from, "css") != 0 || strcmp(e->to, "sss") != 0 ||
          strcmp(e->cause, "start") != 0) {
        bad("(p1=%d,p2=%d) want the css->sss 'start' transition [%s]",
            r->p1Type, r->p2Type, r->why);
      }
      if (strcmp(foh_screen_token(s.screen), "sss") != 0) {
        bad("(p1=%d,p2=%d) screen is '%s', want 'sss' [%s]", r->p1Type,
            r->p2Type, foh_screen_token(s.screen), r->why);
      }
    } else {
      nRefuse++;
      if (e->kind != FOH_EV_SEL || !e->field || !e->sval ||
          strcmp(e->field, "refused") != 0 ||
          strcmp(e->sval, "portconfig") != 0) {
        bad("(p1=%d,p2=%d) want the 'refused portconfig' event [%s]",
            r->p1Type, r->p2Type, r->why);
      }
      // A refusal that still moved the screen would be a launch wearing a
      // deny sound — the screen is the load-bearing half of the claim.
      if (strcmp(foh_screen_token(s.screen), "css") != 0) {
        bad("(p1=%d,p2=%d) refused but the screen moved to '%s' [%s]",
            r->p1Type, r->p2Type, foh_screen_token(s.screen), r->why);
      }
    }
  }

  // Shape pins: the grid must stay the FULL 3x3, and it must keep both
  // sides. An all-refuse table (a guard inverted to refuse everything)
  // would satisfy every row above only if the rows said so — these pins
  // make the split itself a declared, checkable fact.
  if (nLaunch + nRefuse != 9) {
    bad("drove %d cells, want the full 3x3 grid", nLaunch + nRefuse);
  }
  if (nLaunch != 2) bad("%d launching cells, want 2", nLaunch);
  if (nRefuse != 7) bad("%d refusing cells, want 7", nRefuse);

  if (g_fails) {
    // Trailer grammar mirrors foh_snd_witness.c so the check side can parse
    // the WHOLE output, not just look for a substring.
    fprintf(stderr, "foh_launch_witness: %d failure(s)\n", g_fails);
    return 1;
  }
  printf("LAUNCH GUARD WITNESS OK (cells=9 launch=2 refuse=7)\n");
  return 0;
}
