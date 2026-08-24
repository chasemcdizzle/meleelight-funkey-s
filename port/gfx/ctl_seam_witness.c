// port/gfx/ctl_seam_witness.c — THE RESOLVER->SIM SEAM WITNESS (fix_plan
// A42; /CONTEXT.md "Seam").
//
// WHY THIS FILE EXISTS. port/gfx/ctl_input_witness.c proves the resolver
// EMITS the right BITS. Nothing proved those bits REACH AN ACTION, and on
// 2026-08-24 that gap shipped a completely dead feature: DEVIATION D33
// wired physical X to `in.z` under the source comment "Z: grab (and
// lightshield-grab upstream)". That comment is true of REAL MELEE and
// FALSE OF THIS ENGINE. Measured, this session, over the whole sim:
//   readers of MlInput.z  = {FORWARD,UP,DOWN}SMASH.c (`i0->a || i0->z`),
//                           action_state_shortcuts.c:522 checkForAerials
//                           (`(a edge) || (z edge)`), physics.c:983
//                           lCancelUpdate (`z` edge, beside the analog
//                           triggers) -- and the AI plane.
//   GRAB dispatches from z = ZERO.
// `z` is an ALTERNATE ATTACK button (smashes + aerials) and an L-cancel
// trigger. It is not grab. The owner pressed X, nothing happened, and
// BOTH planes' checks were green: the resolver check asserted the bit and
// stopped at the seam.
//
// So this witness crosses it. Physical button -> the REAL
// s1_input_row_style() -> the REAL sim_game_tick() -> the actionState the
// engine actually enters. Every role DEVIATION D33 moved is covered
// (A=jump, B=attack, Y=special, X=grab), in every style, because the next
// remap must not be able to ship the same way.
//
// It also carries the EMITTED-vs-RENDERABLE vfx assertion (/CONTEXT.md
// "Emitted vs renderable") -- but that one is a text comparison over two
// trees, so it lives in the driving script, not here.
//
// Driven by port/gfx/check-ctl-input.sh leg [4]. Host only.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../sim/ml_events.h" // ml_active_rng
#include "../sim/sim/sim.h"
#include "ctl_style.h"
#include "platform.h"
#include "s1_input.h"

#define ML_BOOT_DRAWS 465 // the qjs boot pin (oracle/qjs/replay.sh)

// The match this witness drives. g01's parameters (fox vs marth on
// battlefield, seed 1337) so the plane it exercises is the one the M2
// EXIT GATE already replays frame-for-frame -- no new domain is invented
// here, only a new INPUT.
#define SEAM_SEED 1337u
#define SEAM_P1 2 // fox
#define SEAM_P2 0 // marth
#define SEAM_STAGE 0 // battlefield

static int g_fails;
static int g_probe;

static void want(int cond, const char *what) {
  if (!cond) {
    printf("FAIL %s\n", what);
    g_fails++;
  }
}

static void seam_tick(const MlInput *p1row) {
  const MlInput neutral = nullInput();
  const MlInput *rows[4] = {p1row, &neutral, 0, 0};
  G.frame++;
  sim_game_tick(&G, rows);
}

// A fresh match, ticked past the `starting` window and the spawn fall, so
// player 0 is standing in WAIT with an empty input history. Returns the
// number of warm-up frames burned.
static int seam_fresh_match(void) {
  sim_boot_page(&G);
  ml_active_rng = &G.rng;
  ml_rng_seed(&G.rng, SEAM_SEED);
  for (int k = 0; k < ML_BOOT_DRAWS; k++) (void)ml_rng_next(&G.rng);
  G.rngStateAtReset = G.rng.a;
  sim_setup_match(&G, SEAM_P1, SEAM_P2, 0, 5, SEAM_STAGE);
  G.rngStateAtFrame1 = G.rng.a;
  G.frame = 0;
  const MlInput neutral = nullInput();
  int f = 0;
  // Neutral until the match is live AND the character has landed and
  // settled into WAIT. Bounded: a stall is a failure, never a hang.
  for (; f < 600; f++) {
    seam_tick(&neutral);
    if (!G.starting && strcmp(G.sim.player[0].actionState, "WAIT") == 0) {
      break;
    }
  }
  return f;
}

// Hold `p` through the REAL resolver for up to `frames` ticks and return
// the FIRST actionState player 0 leaves WAIT for. NULL if it never left.
static const char *seam_press(CtlStyle style, const PlatformInput *p,
                              int frames) {
  static char first[ML_STR_CAP];
  seam_fresh_match();
  for (int f = 0; f < frames; f++) {
    const MlInput row = s1_input_row_style(p, style, ctl_mod_on_r_get());
    seam_tick(&row);
    if (strcmp(G.sim.player[0].actionState, "WAIT") != 0) {
      snprintf(first, sizeof first, "%s", G.sim.player[0].actionState);
      return first;
    }
  }
  return 0;
}

// Same, but from a NON-neutral starting state: hold `lead` until player 0
// reaches `leadState` (bounded), then hold `p` and return the first state
// that is neither WAIT nor leadState.
static const char *seam_press_from(CtlStyle style, const PlatformInput *lead,
                                   const char *leadState,
                                   const PlatformInput *p, int frames) {
  static char first[ML_STR_CAP];
  seam_fresh_match();
  int f = 0;
  for (; f < 120; f++) {
    const MlInput row = s1_input_row_style(lead, style, ctl_mod_on_r_get());
    seam_tick(&row);
    if (strcmp(G.sim.player[0].actionState, leadState) == 0) break;
  }
  if (f == 120) return "(lead never reached)";
  for (f = 0; f < frames; f++) {
    const MlInput row = s1_input_row_style(p, style, ctl_mod_on_r_get());
    seam_tick(&row);
    const char *st = G.sim.player[0].actionState;
    if (strcmp(st, leadState) != 0 && strcmp(st, "WAIT") != 0) {
      snprintf(first, sizeof first, "%s", st);
      return first;
    }
  }
  return 0;
}

static void seam_case_from(const char *label, const PlatformInput *lead,
                           const char *leadState, const PlatformInput *p,
                           const char *expect) {
  for (int st = 0; st < CTL_STYLE_COUNT; st++) {
    const char *got =
        seam_press_from((CtlStyle)st, lead, leadState, p, 24);
    if (g_probe) {
      printf("  PROBE style=%d %s -> %s\n", st, label, got ? got : "(none)");
      continue;
    }
    char what[200];
    snprintf(what, sizeof what, "[4] %s reaches %s in style %d (got %s)",
             label, expect, st, got ? got : "(nothing)");
    want(got && strcmp(got, expect) == 0, what);
  }
}

// One physical button, one expected action state, in every style.
static void seam_case(const char *label, const PlatformInput *p,
                      const char *expect) {
  for (int st = 0; st < CTL_STYLE_COUNT; st++) {
    const char *got = seam_press((CtlStyle)st, p, 12);
    if (g_probe) {
      printf("  PROBE style=%d %s -> %s\n", st, label, got ? got : "(WAIT)");
      continue;
    }
    char what[160];
    snprintf(what, sizeof what,
             "[4] physical %s reaches %s in style %d (got %s)", label, expect,
             st, got ? got : "(nothing — still WAIT)");
    want(got && strcmp(got, expect) == 0, what);
  }
}

int main(int argc, char **argv) {
  if (argc < 2) {
    fprintf(stderr, "usage: ctl_seam_witness <simdata.txt> [--probe]\n");
    return 2;
  }
  g_probe = (argc > 2 && strcmp(argv[2], "--probe") == 0);
  sim_boot_page(&G);
  sim_data_load(argv[1]);
  sim_data_register();

  PlatformInput p;
  memset(&p, 0, sizeof p); p.a = true;
  seam_case("A", &p, "KNEEBEND");
  memset(&p, 0, sizeof p); p.b = true;
  seam_case("B", &p, "JAB1");
  memset(&p, 0, sizeof p); p.y = true;
  seam_case("Y", &p, "NEUTRALSPECIALGROUND");
  memset(&p, 0, sizeof p); p.x = true;
  seam_case("X", &p, "GRAB");

  // The routes the fix_plan A42 table names, each crossed end to end.
  // (a) X WHILE SHIELDING — GUARD.c:75's `a` edge. L is the shield
  //     shoulder in every style under the default Mod cell (D30/D31).
  {
    PlatformInput lead, press;
    memset(&lead, 0, sizeof lead); lead.l = true;
    memset(&press, 0, sizeof press); press.l = true; press.x = true;
    seam_case_from("X while SHIELDING", &lead, "GUARD", &press, "GRAB");
  }
  // (b) X WHILE DASHING — DASH.c:72's shoulder arm into GUARDON, whose
  //     same-tick interrupt chain still sees the `a` edge.
  {
    PlatformInput lead, press;
    memset(&lead, 0, sizeof lead); lead.right = true;
    memset(&press, 0, sizeof press); press.right = true; press.x = true;
    seam_case_from("X while DASHING", &lead, "DASH", &press, "GRAB");
  }
  // (c) AIRBORNE, X IS STILL AN ATTACK. D33 gave X the alternate-attack
  //     role via `z`; D34 must not trade one broken role for another, so
  //     this leg pins that the aerial still comes out.
  {
    PlatformInput lead, press;
    memset(&lead, 0, sizeof lead); lead.a = true;
    memset(&press, 0, sizeof press); press.x = true;
    seam_case_from("X while AIRBORNE", &lead, "JUMPF", &press, "ATTACKAIRN");
  }

  if (g_probe) return 0;
  if (g_fails != 0) {
    printf("CTL SEAM WITNESS FAILED (%d)\n", g_fails);
    return 1;
  }
  printf("CTL SEAM OK\n");
  return 0;
}
