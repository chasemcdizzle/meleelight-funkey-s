// port/foh/foh_launchkind_witness.c — the LAUNCH KIND regression witness
// (review-mexit-r3 Medium "M6"; the r2 High class fix at foh.c:730).
//
// THE DEFECT THIS GUARDS. `FohState.targetMode` is the launch KIND, and until
// punch-list A19 a process could only ever launch once, so leaving it set was
// harmless. A19 made "leave the match" return to the FOH menus IN-PROCESS, and
// then the sequence
//
//     target LAUNCH  ->  match  ->  quit  ->  TSS  -B->  menu top  ->  VS
//                                                        ->  SSS  -A->  launch
//
// launched a VS match with targetMode still TRUE, because only the TSS arm
// (foh.c:800) ever wrote the field. foh_dev.c dispatches the whole match on
// that flag (`tgtLive = brLive && foh.targetMode`, and the bridge cross-guards
// at foh_dev.c:2414/:2419), so the "VS" launch ran the TARGET driver. The class fix is
// that EVERY launch arm states its own kind — foh.c:730's `targetMode = false`.
//
// WHY A UNIT DRIVER. The defect needs targetMode to be TRUE before the VS
// launch, and the only writer that sets it true is a real target launch, which
// on the flow-fed rigs ends the FOH phase. So no committed flow can reach the
// second launch: the witness has to survive a match exit, which is exactly
// what this driver reproduces — it applies a HAND COPY of foh_dev.c's A19
// re-entry mutation between the two launches. That copy carries a MANUAL
// synchronisation obligation, spelled out at the site; nothing here detects
// drift in the production re-entry block, and the earlier claim in this
// preamble that the assertions would notice was false (review-mexit-r5 Low).
//
// The state machine here is the REAL one: every transition below is driven by
// feeding button edges to foh_tick(). The one shortcut is stated where it is
// taken (the CSS free-cursor walk is skipped by putting the machine on the
// SSS directly) and is deliberately NOT hidden: the subject of this witness is
// the LAUNCH ARMS, and the CSS navigation is already covered end to end by
// check-foh-flows.sh's f01/f05 flows.
//
// Prints one line per assertion and `LAUNCH KIND OK` on success, exit 0. Any
// failure prints `LAUNCH KIND FAIL: ...` and exits 1. Its negative test lives
// in port/foh/check-mexit-reentry.sh: a COPY of foh.c with the class fix
// removed must make this driver fail.
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "foh.h"

// gfx_fatal is host-provided (raster.h), the foh_banner_witness.c pattern.
// Nothing this driver reaches should trip it; if something does, that is a
// failure of the witness, not a pass.
void gfx_fatal(const char *what) {
  fprintf(stderr, "LAUNCH KIND FAIL: gfx_fatal: %s\n", what);
  exit(3);
}

static int g_fails;

static void ok(const char *what) { printf("  ok  %s\n", what); }

static void bad(const char *what) {
  printf("LAUNCH KIND FAIL: %s\n", what);
  g_fails++;
}

static void want(int cond, const char *what) {
  if (cond) ok(what);
  else bad(what);
}

// One tick with a single button held, then one neutral tick — i.e. exactly one
// rising edge, which is what every FOH arm reads. Returns the number of LAUNCH
// events the pressed tick emitted (the neutral tick cannot emit one).
static int press(FohState *s, size_t off) {
  PlatformInput in;
  memset(&in, 0, sizeof in);
  if (off != (size_t)-1) *(bool *)((char *)&in + off) = true;
  foh_tick(s, &in);
  int launches = 0;
  for (int e = 0; e < s->nev; e++) {
    if (s->ev[e].kind == FOH_EV_LAUNCH) launches++;
  }
  memset(&in, 0, sizeof in);
  foh_tick(s, &in);
  return launches;
}

#define PRESS(s, field) press((s), offsetof(PlatformInput, field))

int main(void) {
  FohState s;
  foh_init(&s);

  // --- boot to the menu top -------------------------------------------------
  // STARTUP -> TITLE is the timer transition; TITLE -> MENU_TOP is START.
  // Tick until the machine is off the title rather than pinning the timer
  // constant here (that constant is the title screen's business, not this
  // witness's).
  {
    PlatformInput neutral;
    memset(&neutral, 0, sizeof neutral);
    int guard = 0;
    while (s.screen != FOH_TITLE && guard++ < 100000) foh_tick(&s, &neutral);
    want(s.screen == FOH_TITLE, "machine reaches the title screen");
  }
  PRESS(&s, start);
  want(s.screen == FOH_MENU_TOP, "START leaves the title for the menu top");

  // --- launch 1: the TARGET path (f06's own edges: D, A, then A on slot 0) --
  PRESS(&s, down);
  PRESS(&s, a);
  want(s.screen == FOH_TSS, "menu top -> target select");
  want(!s.targetMode, "targetMode is still false BEFORE any launch");
  int n = PRESS(&s, a);
  want(n == 1, "target select A emits exactly one LAUNCH event");
  want(s.launched, "the target launch set `launched`");
  want(s.targetMode, "the TARGET launch set targetMode TRUE (foh.c:800)");
  want(s.screen == FOH_TMATCH, "the target launch landed on FOH_TMATCH");

  // --- the A19 in-process return, as foh_dev.c performs it ------------------
  // foh_dev.c's re-entry block keeps the state machine, puts the screen back,
  // and clears only what a match invalidates. The five assignments below are a
  // HAND COPY of it.
  //
  // MANUAL SYNCHRONISATION OBLIGATION, stated rather than wished away
  // (review-mexit-r4 Low; an earlier version of this note claimed the
  // assertions below would "notice" a drifted re-entry block, which is false —
  // they only check what they name). This witness does NOT call the production
  // re-entry: that block lives inside foh_dev.c's main(), is not a function,
  // and also does tdev_end_game()/music/`foh.prev` seeding from a post-drain
  // poll, none of which a unit driver has. So:
  //
  //   IF YOU ADD OR CHANGE A FIELD in foh_dev.c's `foh_phase:` re-entry block
  //   (search "A19: the in-process return"), MIRROR IT HERE.
  //
  // What that costs if someone forgets: this witness would keep testing the
  // OLD re-entry shape. It would still catch the defect it exists for (a
  // launch arm that does not state its kind), because targetMode is not
  // touched by either shape — but it would stop being a faithful model of the
  // real return. The honest scope of this driver is "the launch ARMS across a
  // match exit", not "the re-entry block is correct"; the re-entry block's own
  // witness is port/foh/check-mexit-reentry.sh, which runs the real thing.
  //
  // `foh.prev` is deliberately NOT seeded here: foh_dev.c seeds it from a
  // post-overlay drain poll so a still-held button is not read as a fresh
  // press, and this driver only ever feeds clean press/release pairs.
  s.launched = false;
  s.bHold = 0;
  s.nev = 0;
  s.nsnd = 0;
  s.screen = FOH_TSS; // MEX_TSS -> changeGamemode(7) (main.js:1395-1400)
  want(s.targetMode,
       "targetMode SURVIVES the match exit (it is not cleared on re-entry — "
       "which is the whole reason the launch arms must state their own kind)");

  // --- back out to the menus through the REAL B arm -------------------------
  PRESS(&s, b);
  want(s.screen == FOH_MENU_TOP, "target select B returns to the menu top");
  want(s.targetMode, "targetMode is STILL true at the menu top (the defect's "
                     "precondition, reproduced)");

  // --- launch 2: the VS path ------------------------------------------------
  // SHORTCUT, stated: the CSS is a free-cursor screen whose walk is covered by
  // check-foh-flows.sh's committed f01/f05 flows. This witness is about the
  // launch ARMS, so it puts the machine on the stage select directly. The
  // launch itself is the real step_sss arm, driven by a real A edge.
  s.screen = FOH_SSS;
  n = PRESS(&s, a);
  want(n == 1, "stage select A emits exactly one LAUNCH event");
  want(s.launched, "the VS launch set `launched`");
  want(s.screen == FOH_MATCH, "the VS launch landed on FOH_MATCH");
  want(!s.targetMode,
       "the VS launch CLEARED targetMode (foh.c:730) — without this the "
       "second match dispatches through the TARGET driver");

  if (g_fails) {
    printf("LAUNCH KIND FAIL: %d assertion(s) failed\n", g_fails);
    return 1;
  }
  printf("LAUNCH KIND OK\n");
  return 0;
}
