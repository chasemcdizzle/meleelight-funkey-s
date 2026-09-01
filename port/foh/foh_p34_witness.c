// foh_p34_witness.c — THE P3/P4 TICKET, driven as gestures (fix_plan A44).
//
// THE ASK, in the owner's words: "why can't I turn on player 3 and 4 at the
// CSS?" This witness answers it the only way an answer can be checked — by
// performing the gestures a player performs, through the REAL foh_tick, and
// then reading the plane a match would actually launch from.
//
// WHAT IT DRIVES, end to end, with no state poked by hand except where a
// negative case says so:
//   1. walk the hand onto PORT 2's type tab and press A          -> HMN
//   2. press A twice more                                        -> N/A -> HMN
//      (the cycle has TWO states here, not three: DEVIATION D40(b) keeps CPU
//      off ports 2/3 because the sim can replay one AI slot, A46 OPEN/OWED.
//      A third press landing on CPU is the failure this step exists for.)
//   3. the same for PORT 3
//   4. walk onto PORT 2's TOKEN and press A                      -> carried
//      (DEVIATION D40(a): one input device, one hand, and it may take any
//      port's token. Without it a HMN port 2 could never be given a
//      character at all, which is the stub HARD RULE 2 forbids.)
//   5. carry it onto falco's cell and press A                    -> dropped
//   6. assert the SELECTION plane, per port: port 2 chose falco and NO OTHER
//      PORT MOVED. This is the D21/D35 family's assertion and it is written
//      per port on purpose — both shipped bugs in this screen's history were
//      a port index and a roster index changing places (CONTEXT.md "Port").
//   7. assert the TOKEN plane agrees with the selection: port 2's token is
//      drawn inside the cell of the character port 2 chose, not wherever it
//      physically came to rest.
//   8. switch port 1 OFF, leaving P1 + P3, and assert the config
//      foh_launch_ports hands the sim — field for field, per port.
//   9. press START and assert it LAUNCHES. Before A44 this exact
//      configuration produced `deny` + `refused portconfig`.
//
// WHERE THE SIM HALF IS PROVEN, since this witness stops at the seam: the
// seam IS `SimPortCfg[4]`, and step 8 asserts every field of it. The other
// side of that struct — sim_setup_match_ports, upstream's own four-port
// harnessSetupMatch loop — is proven by fix_plan A46's four-port golden q01
// (two fresh browser runs IDENTICAL, then `STREAM MATCH 3600/3600 frames
// exact` through the UNCHANGED verify-stream.js). Re-linking the whole sim
// here would re-run that, not add to it. What A44 owed was the FOH's half,
// and additionally the LAUNCH and BRIDGE-STATE records now carry ports 2/3
// (BRIDGE-STATE reads them back out of the GameState), so any later 4-port
// flow is judged across the crossing without further work.
//
// Usage: foh_p34_witness (no arguments)
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "foh.h"
#include "foh_launch.h"
#include "foh_persist.h"
#include "../sim/ml_ser.h" // ml_sha256_hex, for the v5 migration arm

void gfx_fatal(const char *what) {
  fprintf(stderr, "foh_p34_witness: gfx_fatal: %s\n", what);
  exit(3);
}
void sim_fatal(const char *what);
void sim_fatal(const char *what) {
  fprintf(stderr, "foh_p34_witness: sim_fatal: %s\n", what);
  exit(3);
}

static int g_fails;
static void bad(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
static void bad(const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  fprintf(stderr, "foh_p34_witness: FAIL: ");
  vfprintf(stderr, fmt, ap);
  fprintf(stderr, "\n");
  va_end(ap);
  g_fails++;
}

static void ok(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
static void ok(const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  printf("  ok ");
  vprintf(fmt, ap);
  printf("\n");
  va_end(ap);
}

// --- gesture primitives ------------------------------------------------------
// The hand is a free cursor integrated from the d-pad (DEVIATION D1/D3), so
// "put the hand here" is a WALK, never an assignment: assigning cssHandX/Y
// would skip foh_hand_step and test a machine the player cannot drive.

static bool tick_held(FohState *s, bool up, bool dn, bool lf, bool rt) {
  PlatformInput in;
  memset(&in, 0, sizeof in);
  in.up = up;
  in.down = dn;
  in.left = lf;
  in.right = rt;
  foh_tick(s, &in);
  return s->screen == FOH_CSS;
}

// Walk to (tx, ty) one axis at a time. Returns false if the cursor could not
// get within a pixel inside the frame budget — a loud failure, never a
// silent "close enough", because every assertion after it is about where the
// hand ended up.
static bool walk_to(FohState *s, double tx, double ty) {
  for (int axis = 0; axis < 2; axis++) {
    const double want = axis == 0 ? tx : ty;
    const double from = axis == 0 ? s->cssHandX : s->cssHandY;
    if (want == from) continue;
    const bool neg = want < from;
    // Step until the target is CROSSED, not until it is hit exactly: the
    // cursor moves a fixed sub-pixel distance per frame (DEVIATION D3's one
    // calibration knob), so a target that is not a whole number of steps away
    // would be straddled forever. Every destination here is a rect CENTRE, so
    // landing within one step of it is landing inside the rect.
    for (int i = 0; i <= 4000; i++) {
      const double cur = axis == 0 ? s->cssHandX : s->cssHandY;
      if (neg ? cur <= want : cur >= want) break;
      if (i == 4000) {
        bad("the hand did not reach (%.1f,%.1f) in 4000 frames (stuck at "
            "%.2f,%.2f)", tx, ty, s->cssHandX, s->cssHandY);
        return false;
      }
      if (!tick_held(s, axis == 1 && neg, axis == 1 && !neg,
                     axis == 0 && neg, axis == 0 && !neg)) {
        bad("the walk to (%.1f,%.1f) left the CSS", tx, ty);
        return false;
      }
    }
  }
  return true;
}

// One A press: a rising edge needs a released frame in front of it, and the
// machine reads `aE = in->a && !pv->a`.
static void press_a(FohState *s) {
  PlatformInput in;
  memset(&in, 0, sizeof in);
  foh_tick(s, &in); // released
  in.a = true;
  foh_tick(s, &in); // rising edge
  memset(&in, 0, sizeof in);
  foh_tick(s, &in); // released again
}

static bool snd_has(const FohState *s, const char *name) {
  for (int i = 0; i < s->nsnd; i++) {
    if (strcmp(s->snd[i], name) == 0) return true;
  }
  return false;
}

// Did this tick's event queue carry `S <field> <val>`?
static bool saw_sel(const FohState *s, const char *field, int val) {
  for (int i = 0; i < s->nev; i++) {
    const FohEvent *e = &s->ev[i];
    if (e->kind == FOH_EV_SEL && e->field && !e->sval &&
        strcmp(e->field, field) == 0 && e->val == val) {
      return true;
    }
  }
  return false;
}

// A single A press that must also emit a named sound and a named S event.
static void press_a_expect(FohState *s, const char *sound, const char *field,
                           int val, const char *what) {
  PlatformInput in;
  memset(&in, 0, sizeof in);
  foh_tick(s, &in);
  in.a = true;
  foh_tick(s, &in);
  if (sound && !snd_has(s, sound)) {
    bad("%s: the press emitted no '%s' sound", what, sound);
  }
  if (field && !saw_sel(s, field, val)) {
    bad("%s: the press emitted no 'S %s %d' event", what, field, val);
  }
  memset(&in, 0, sizeof in);
  foh_tick(s, &in);
}

// The REAL boot order, taken from foh_app.c: foh_init, then the persist
// chokepoint's load+apply, then the tick loop. `p` NULL means a boot with no
// persisted record, which is every leg but the last one.
static void reach_css_boot(FohState *s, const FohPersist *p) {
  foh_init(s);
  if (p) foh_persist_apply(p, s);
  PlatformInput in;
  memset(&in, 0, sizeof in);
  for (int i = 0; i < 380; i++) foh_tick(s, &in); // startup timer
  in.start = true;
  foh_tick(s, &in);
  memset(&in, 0, sizeof in);
  foh_tick(s, &in);
  in.a = true;
  foh_tick(s, &in); // menu row 0 == VS. Melee (owner ruling C5)
  memset(&in, 0, sizeof in);
  foh_tick(s, &in);
  if (s->screen != FOH_CSS) {
    fprintf(stderr, "foh_p34_witness: did not reach the CSS\n");
    exit(3);
  }
}

static void reach_css(FohState *s) { reach_css_boot(s, 0); }

// The centre of port k's type tab, from the SAME constants foh.c hit-tests
// and foh_render.c draws (DEVIATION D4's single source).
static void tab_centre(int k, double *x, double *y) {
  *x = (double)foh_css_panel_x(k) + (double)FOH_CSS_TAB_W / 2.0;
  *y = (double)FOH_CSS_PANEL_Y + (double)FOH_CSS_TAB_H / 2.0;
}

static const char *const kCharName[5] = {"marth", "puff", "fox", "falco",
                                         "falcon"};

// Which cell is port k's token drawn in? Derived from the SAME rects foh.c
// hit-tests, never from a remembered pixel — that is the whole subject of
// D21/D35/D46, so the instrument must not restate the bug it is looking for.
// Returns -1 if the token is not inside any cell.
static int token_cell(const FohState *s, int k) {
  double tx, ty;
  foh_css_token_pos(s, k, &tx, &ty);
  FohHandRect cells[FOH_CSS_CHARS];
  foh_css_cells(cells);
  for (int c = 0; c < 5; c++) {
    if (tx > (double)cells[c].x && tx < (double)(cells[c].x + cells[c].w) &&
        ty > (double)cells[c].y && ty < (double)(cells[c].y + cells[c].h)) {
      return c;
    }
  }
  return -1;
}

// Walk the hand to the centre of cell c.
static bool walk_to_cell(FohState *s, int c) {
  FohHandRect cells[FOH_CSS_CHARS];
  foh_css_cells(cells);
  return walk_to(s, (double)cells[c].x + (double)cells[c].w / 2.0,
                 (double)cells[c].y + (double)cells[c].h / 2.0);
}

// Cycle port k's type tab until it reads `want`, by REAL A presses on the
// real tab. Loud rather than looping forever if the cycle cannot reach it.
static bool set_port_type(FohState *s, int k, int want) {
  double tx, ty;
  tab_centre(k, &tx, &ty);
  if (!walk_to(s, tx, ty)) return false;
  for (int i = 0; i < 8; i++) {
    if (foh_css_port_type(s, k) == want) return true;
    press_a(s);
  }
  bad("port %d's type box never reached %d in a full cycle (it is %d)", k,
      want, foh_css_port_type(s, k));
  return false;
}

int main(void) {
  FohState s;
  reach_css(&s);

  // --- [1] the type tab of EVERY port, all four, independently --------------
  // A49 (owner ruling 1, *"yeah enable the CPu please"*) retires DEVIATION
  // D40(b): ports 2/3 used to cycle N/A -> HMN -> N/A with CPU unreachable.
  // All four ports now run upstream's own three-state cycle, so this leg is
  // driven over all four rather than the two A44 added — a rule that only
  // holds on the ports someone remembered to check is not a rule.
  {
    static const char *const kTypeField[FOH_CSS_PORTS] = {"p1type", "p2type",
                                                          "p3type", "p4type"};
    for (int port = 0; port < FOH_CSS_PORTS; port++) {
      const char *field = kTypeField[port];
      double tx, ty;
      // foh_init arms port 0 at HMN (addPlayer, main.js:495) and leaves the
      // other three at N/A (main.js:107). Assert the start state rather than
      // assuming it, then walk each port round to N/A so the three presses
      // below mean the same thing on every port.
      const int wantStart = port == 0 ? 0 : -1;
      if (foh_css_port_type(&s, port) != wantStart) {
        bad("port %d does not start at %d, it is %d", port, wantStart,
            foh_css_port_type(&s, port));
      }
      tab_centre(port, &tx, &ty);
      if (!walk_to(&s, tx, ty)) return 1;
      while (foh_css_port_type(&s, port) != -1) press_a(&s);

      press_a_expect(&s, "menuSelect", field, 0, "turning the port on");
      if (foh_css_port_type(&s, port) != 0) {
        bad("port %d did not become HMN(0) on the first press, it is %d",
            port, foh_css_port_type(&s, port));
      }
      press_a_expect(&s, "menuSelect", field, 1, "turning the port to CPU");
      if (foh_css_port_type(&s, port) != 1) {
        bad("port %d did not reach CPU(1) on the second press, it is %d — "
            "this press IS the owner's ticket, and D40(b) is why it used to "
            "wrap to N/A on ports 2 and 3", port, foh_css_port_type(&s, port));
      }
      press_a_expect(&s, "menuSelect", field, -1, "turning the port off");
      if (foh_css_port_type(&s, port) != -1) {
        bad("port %d's cycle did not wrap to N/A after CPU, it is %d", port,
            foh_css_port_type(&s, port));
      }
      ok("port %d's type box cycles N/A -> HMN -> CPU -> N/A under real A "
         "presses", port);
    }
    // ...and leave the machine where legs [2]-[6] expect it: ports 0, 2 and 3
    // HMN and port 1 N/A, which is where A44's version of this leg ended.
    static const int kAfterLeg1[FOH_CSS_PORTS] = {0, -1, 0, 0};
    for (int k = 0; k < FOH_CSS_PORTS; k++) {
      if (!set_port_type(&s, k, kAfterLeg1[k])) return 1;
    }
    for (int k = 0; k < FOH_CSS_PORTS; k++) {
      if (foh_css_port_type(&s, k) != kAfterLeg1[k]) {
        bad("port %d was left at %d after leg [1], want %d", k,
            foh_css_port_type(&s, k), kAfterLeg1[k]);
      }
    }
  }

  // --- [2] take port 2's token and give it a character ----------------------
  const int wantChar = 3; // falco — NOT cell 0, and not port 2's index
  // D61: THE BASELINE IS TAKEN, NOT ASSUMED. Leg [1] walks all four ports
  // through CPU, and a port that becomes a CPU now picks its own character,
  // so "everything else is still marth" stopped being true — for a reason
  // that has nothing to do with what this witness is about. What it IS about
  // survives verbatim and is stated against this baseline below: moving PORT
  // 2's selection must move NO OTHER PORT's. Reading the baseline here rather
  // than writing `{0, 0, x, 0}` also means this leg keeps its edge whatever a
  // future roster or a future pick does.
  int baseSel[FOH_CSS_PORTS];
  for (int k = 0; k < FOH_CSS_PORTS; k++) baseSel[k] = s.selChar[k];
  {
    double tx, ty;
    foh_css_token_pos(&s, 2, &tx, &ty);
    if (!walk_to(&s, tx, ty)) return 1;
    press_a(&s);
    if (s.cssCarry != 2) {
      bad("A over port 2's token did not pick it up (carry is %d) — without "
          "DEVIATION D40(a) the one hand may only take port 0's, so a HMN "
          "port 2 could never be given a character", s.cssCarry);
      fprintf(stderr, "foh_p34_witness: %d failure(s)\n", g_fails + 1);
      return 1;
    }
    ok("the one hand picked up PORT 2's token (D40(a))");

    FohHandRect cells[FOH_CSS_CHARS];
    foh_css_cells(cells);
    const double cx = (double)cells[wantChar].x + (double)cells[wantChar].w / 2.0;
    const double cy = (double)cells[wantChar].y + (double)cells[wantChar].h / 2.0;
    if (!walk_to(&s, cx, cy)) return 1;
    if (s.selChar[2] != wantChar) {
      bad("hovering %s's cell while carrying port 2's token did not select it "
          "for port 2 (port 2 reads %s)", kCharName[wantChar],
          kCharName[s.selChar[2]]);
    }
    // The announcer and the `carry -1` release are both read ON THE RISING
    // EDGE's tick — the sound queue is per tick, so checking after a released
    // frame would find it empty and pass for the wrong reason.
    press_a_expect(&s, kCharName[wantChar], "carry", -1, "dropping the token");
    if (s.cssCarry != -1) bad("A over the cell did not drop the token");
    ok("carrying it onto %s's cell selected %s for PORT 2 and dropped it "
       "there", kCharName[wantChar], kCharName[wantChar]);
  }

  // --- [3] the selection plane, PER PORT ------------------------------------
  // The D21/D35 family: what a port launches with is its SELECTION, and no
  // other port's selection may move because this one did.
  {
    int want[FOH_CSS_PORTS];
    for (int k = 0; k < FOH_CSS_PORTS; k++) want[k] = baseSel[k];
    want[2] = wantChar; // the ONLY port this leg's gesture touched
    for (int k = 0; k < FOH_CSS_PORTS; k++) {
      if (s.selChar[k] != want[k]) {
        bad("port %d's selection is %s, want %s — a port index and a roster "
            "index have changed places (CONTEXT.md \"Port\")", k,
            kCharName[s.selChar[k]], kCharName[want[k]]);
      }
    }
    ok("the selection plane moved PORT 2 to %s and no other port (baseline "
       "%s/%s/-/%s carried through the D61 CPU picks)", kCharName[wantChar],
       kCharName[baseSel[0]], kCharName[baseSel[1]], kCharName[baseSel[3]]);
  }

  // --- [4] the token plane agrees with the selection ------------------------
  {
    double tx, ty;
    foh_css_token_pos(&s, 2, &tx, &ty);
    FohHandRect cells[FOH_CSS_CHARS];
    foh_css_cells(cells);
    const FohHandRect *c = &cells[wantChar];
    if (!(tx > (double)c->x && tx < (double)(c->x + c->w) &&
          ty > (double)c->y && ty < (double)(c->y + c->h))) {
      bad("port 2's token rests at (%.1f,%.1f), outside %s's cell "
          "(%d..%d, %d..%d) — the token is the only roster-level indicator on "
          "this screen (D21), so it must be drawn on the character the port "
          "chose", tx, ty, kCharName[wantChar], c->x, c->x + c->w, c->y,
          c->y + c->h);
    }
    // ...and it must be DISTINGUISHABLE from port 0's, or four ports on one
    // character would be one token as far as the player can tell (D41).
    double zx, zy;
    foh_css_token_pos(&s, 0, &zx, &zy);
    if (tx == zx && ty == zy) {
      bad("ports 0 and 2 draw their tokens at the same point (%.1f,%.1f)",
          tx, ty);
    }
    ok("port 2's token is drawn inside %s's cell, at its own 2x2 slot (D41)",
       kCharName[wantChar]);
  }

  // --- [5] the config the sim is handed -------------------------------------
  // Port 1 OFF, so this is P1 + P3: the configuration that did not exist
  // before A44 and that the old two-port guard refused by construction.
  {
    double tx, ty;
    tab_centre(1, &tx, &ty);
    if (!walk_to(&s, tx, ty)) return 1;
    // port 1 starts N/A in foh_init, so one press makes it HMN; walk it back
    // round to N/A through its own three-state cycle (it keeps CPU).
    while (foh_css_port_type(&s, 1) != -1) press_a(&s);
    tab_centre(3, &tx, &ty);
    if (!walk_to(&s, tx, ty)) return 1;
    while (foh_css_port_type(&s, 3) != -1) press_a(&s);

    const int wantType[FOH_CSS_PORTS] = {0, -1, 0, -1};
    for (int k = 0; k < FOH_CSS_PORTS; k++) {
      if (foh_css_port_type(&s, k) != wantType[k]) {
        bad("port %d's type is %d, want %d before the launch config is read",
            k, foh_css_port_type(&s, k), wantType[k]);
      }
    }

    SimPortCfg ports[4];
    foh_launch_ports(&s, ports);
    // D61: the same baseline as leg [3]'s, for the same reason. What this
    // asserts is that the LAUNCH CONFIG reads each port's own selection —
    // the port-index bug — so it is stated against whatever the selection
    // plane actually holds, never against a roster value typed twice.
    int wantCharCfg[FOH_CSS_PORTS];
    for (int k = 0; k < FOH_CSS_PORTS; k++) wantCharCfg[k] = s.selChar[k];
    for (int k = 0; k < FOH_CSS_PORTS; k++) {
      if (ports[k].type != wantType[k]) {
        bad("cfg.players[%d].type is %d, want %d", k, ports[k].type,
            wantType[k]);
      }
      if (ports[k].character != wantCharCfg[k]) {
        bad("cfg.players[%d].character is %s, want %s — the launch config is "
            "reading one port's selection for another", k,
            kCharName[ports[k].character], kCharName[wantCharCfg[k]]);
      }
      // difficulty is `undefined` for every port that is not a CPU
      // (harness patch:84 takes 3 when it is), and none of these are.
      if (ports[k].difficulty != -1) {
        bad("cfg.players[%d].difficulty is %d, want -1 (undefined) on a "
            "non-CPU port", k, ports[k].difficulty);
      }
    }
    ok("the four-port config hands the sim {HMN %s, absent, HMN %s, absent}",
       kCharName[0], kCharName[wantChar]);
  }

  // --- [6] and it LAUNCHES ---------------------------------------------------
  {
    s.cssReady = true; // the ready rule is a DRAW-pass value; the guard is
                       // what is under test here, exactly as the grid witness
                       // in check-foh-flows.sh does it.
    PlatformInput in;
    memset(&in, 0, sizeof in);
    foh_tick(&s, &in);
    in.start = true;
    foh_tick(&s, &in);
    if (strcmp(foh_screen_token(s.screen), "sss") != 0) {
      bad("START on a P1 + P3 match did not reach the stage select (screen is "
          "'%s') — this is the configuration the owner asked for and the "
          "pre-A44 guard refused", foh_screen_token(s.screen));
    }
    if (!snd_has(&s, "menuForward")) {
      bad("the launch played no menuForward");
    }
    ok("START on P1 + P3 (P2 and P4 off) launches — it used to deny");
  }


  // ==========================================================================
  // [7] fix_plan A49 ticket 1 — A CPU ON PORTS 2 AND 3, WITH A REAL LEVEL,
  //     AND IT LAUNCHES.
  //
  // The owner's words: *"yeah enable the CPu please"*. A44's DEVIATION D40(b)
  // refused it on the ground that "the sim refuses it"; MEASURED, that was
  // wrong. AIBRIDGE1 is the RECORDED stream used to REPLAY a CPU golden, not
  // what makes the AI run — the play path links the live C ai.c through
  // ml_sim_runai_live. What AIBRIDGE1's single slot limits is checksum
  // COVERAGE, which is the owner's call and he has made it.
  //
  // This leg drives BOTH new CPU ports and gives them DIFFERENT levels on
  // purpose: the CPU-level plane was two scalars until A49, so a widening
  // that collapsed ports 2/3 onto port 1's field would pass a one-port test
  // and fail here.
  {
    FohState c;
    reach_css(&c);
    const int cpuChar[FOH_CSS_PORTS] = {0, 0, 4, 1}; // falcon on 2, puff on 3

    if (!set_port_type(&c, 2, 1)) return 1;
    if (!set_port_type(&c, 3, 1)) return 1;
    if (foh_css_port_type(&c, 2) != 1 || foh_css_port_type(&c, 3) != 1) {
      bad("ports 2 and 3 did not both reach CPU(1) (%d, %d)",
          foh_css_port_type(&c, 2), foh_css_port_type(&c, 3));
    }
    ok("ports 2 and 3 are CPU, reached by real A presses on their own tabs");

    // Give each of them a character, so the launch config below is not just
    // reading marth back out of a memset.
    for (int k = 2; k <= 3; k++) {
      double tx, ty;
      foh_css_token_pos(&c, k, &tx, &ty);
      if (!walk_to(&c, tx, ty)) return 1;
      press_a(&c);
      if (c.cssCarry != k) {
        bad("could not pick up port %d's token (carry is %d)", k, c.cssCarry);
        return 1;
      }
      if (!walk_to_cell(&c, cpuChar[k])) return 1;
      press_a(&c);
    }

    // --- the knob, dragged. Port 2 to the rail's LEFT end (level 1) and port
    // 3 to its RIGHT end (level 4): the two extremes, so a knob that silently
    // addressed the wrong port's rail would land off its own track.
    const int wantLevel[FOH_CSS_PORTS] = {3, 3, 1, 4};
    for (int k = 2; k <= 3; k++) {
      const double x0 = (double)(foh_css_panel_x(k) + FOH_CSS_RAIL_X0);
      if (!walk_to(&c, foh_css_knob_x(&c, k), foh_css_knob_y())) return 1;
      press_a(&c);
      if (c.cssCpuCarry != k) {
        bad("A on port %d's CPU knob did not grab it (cpu carry is %d) — "
            "before A49 the grab loop stopped at port 1", k, c.cssCpuCarry);
        return 1;
      }
      // Drag along the rail. The machine forces the hand's y while dragging,
      // so the walk is x-only: passing the CURRENT y makes walk_to skip that
      // axis rather than fight the clamp forever.
      const double railX = k == 2 ? x0 : x0 + (double)FOH_CSS_RAIL_LEN;
      if (!walk_to(&c, railX, c.cssHandY)) return 1;
      press_a(&c); // release (css.js:328-333)
      if (c.cssCpuCarry != -1) bad("port %d's knob was not released", k);
      if (foh_css_port_diff(&c, k) != wantLevel[k]) {
        bad("port %d's CPU level is %d after dragging its knob to the rail's "
            "%s end, want %d", k, foh_css_port_diff(&c, k),
            k == 2 ? "left" : "right", wantLevel[k]);
      }
    }
    // ORTHOGONALITY BY PORT, which is the whole reason to drive two knobs:
    // ports 0/1 must still read the default. A two-wide plane aliased behind
    // a `k == 0 ? p1Difficulty : difficulty` accessor would have moved port
    // 1's level twice here and reported it as ports 2 and 3.
    for (int k = 0; k < FOH_CSS_PORTS; k++) {
      if (foh_css_port_diff(&c, k) != wantLevel[k]) {
        bad("port %d's CPU level is %d, want %d — the four levels are not "
            "four independent fields", k, foh_css_port_diff(&c, k),
            wantLevel[k]);
      }
    }
    ok("port 2's knob reads level 1 and port 3's reads level 4, while ports "
       "0 and 1 keep the default 3 — four independent knobs");

    // --- the launch config the sim is handed --------------------------------
    SimPortCfg ports[4];
    foh_launch_ports(&c, ports);
    const int wantType[FOH_CSS_PORTS] = {0, -1, 1, 1};
    for (int k = 0; k < FOH_CSS_PORTS; k++) {
      if (ports[k].type != wantType[k]) {
        bad("cfg.players[%d].type is %d, want %d", k, ports[k].type,
            wantType[k]);
      }
      if (ports[k].character != cpuChar[k]) {
        bad("cfg.players[%d].character is %s, want %s", k,
            kCharName[ports[k].character], kCharName[cpuChar[k]]);
      }
      // The harness patch reads cfg.players[i].difficulty and takes 3 when it
      // is undefined; -1 is this port's spelling of undefined, and it belongs
      // on exactly the non-CPU ports.
      const int wantDiff = wantType[k] == 1 ? wantLevel[k] : -1;
      if (ports[k].difficulty != wantDiff) {
        bad("cfg.players[%d].difficulty is %d, want %d — the level the player "
            "set on port %d's knob is what the match must be played at", k,
            ports[k].difficulty, wantDiff, k);
      }
    }
    ok("the launch config carries CPU falcon@1 on port 2 and CPU puff@4 on "
       "port 3, with -1 (undefined) on the two non-CPU ports");

    // --- and it LAUNCHES ----------------------------------------------------
    c.cssReady = true; // a DRAW-pass value; the launch GUARD is under test
    PlatformInput in;
    memset(&in, 0, sizeof in);
    foh_tick(&c, &in);
    in.start = true;
    foh_tick(&c, &in);
    if (strcmp(foh_screen_token(c.screen), "sss") != 0) {
      bad("START on HMN + CPU(2) + CPU(3) did not reach the stage select "
          "(screen is '%s') — the pre-A49 guard's `cpuTooHigh` clause "
          "refused exactly this and emitted `refused portconfig`",
          foh_screen_token(c.screen));
    }
    if (!snd_has(&c, "menuForward")) bad("the CPU launch played no menuForward");
    ok("START on a P1 + CPU P3 + CPU P4 match launches — it used to deny");
  }

  // ==========================================================================
  // [8] fix_plan A49 ticket 2(b) / DEVIATION D46 — LETTING GO OF THE PIN PUTS
  //     IT BACK ON THE CHARACTER YOU SELECTED.
  //
  // Owner: *"whenever the pin is let go of (going off) it should go back to
  // the character you selected"*. "Going off" is the LEAVE-BAND drop
  // (css.js:336-347), upstream's second rest formula, which MEASURED lands a
  // whole cell to the RIGHT of the character just selected (foh.h,
  // FOH_CSS_TOKEN_LB_DX). This is the D21/D35 family's third instance: in all
  // three the token was re-homed from something that was not the selection.
  //
  // Driven on ALL FOUR PORTS with FOUR DISTINCT non-default characters, none
  // of which equals its own port index — so a port/roster mix-up (the exact
  // confusion D21 and D35 were) cannot pass by coincidence.
  FohState d;
  const int pick[FOH_CSS_PORTS] = {3, 4, 1, 2}; // falco falcon puff fox
  {
    reach_css(&d);
    for (int k = 0; k < FOH_CSS_PORTS; k++) {
      double tx, ty;
      foh_css_token_pos(&d, k, &tx, &ty);
      if (!walk_to(&d, tx, ty)) return 1;
      press_a(&d);
      if (d.cssCarry != k) {
        bad("could not pick up port %d's token (carry is %d)", k, d.cssCarry);
        return 1;
      }
      if (!walk_to_cell(&d, pick[k])) return 1;
      if (d.selChar[k] != pick[k]) {
        bad("hovering %s's cell while carrying port %d's token selected %s "
            "instead", kCharName[pick[k]], k, kCharName[d.selChar[k]]);
      }
      // THE GESTURE UNDER TEST: carry it UP out of the roster band and let go
      // there. No A press — leaving the band is what commits and drops it.
      if (!walk_to(&d, d.cssHandX, (double)FOH_CSS_BAND_TOP - 2.0)) return 1;
      if (d.cssCarry != -1) {
        bad("carrying port %d's token out of the band did not drop it", k);
      }
      if (d.cssTokenRest[k] != 1) {
        bad("port %d's token came to rest in slot %d, want 1 — this leg has "
            "not reproduced the LEAVE-BAND path it claims to test", k,
            d.cssTokenRest[k]);
      }
      if (token_cell(&d, k) != pick[k]) {
        bad("port %d let go of its pin over %s and it came to rest on cell "
            "%d — D46 says a released pin returns to the character that port "
            "SELECTED, never to a pixel formula", k, kCharName[pick[k]],
            token_cell(&d, k));
      }
    }
    // ...and no port disturbed another. Asserted AFTER all four, so a rule
    // that only holds for the port most recently touched fails here.
    for (int k = 0; k < FOH_CSS_PORTS; k++) {
      if (d.selChar[k] != pick[k] || token_cell(&d, k) != pick[k]) {
        bad("port %d ended on selection %s / token cell %d, want %s / %d",
            k, kCharName[d.selChar[k]], token_cell(&d, k),
            kCharName[pick[k]], pick[k]);
      }
    }
    ok("all four ports: letting the pin go outside the band puts it back on "
       "falco/falcon/puff/fox — the character each port selected");
  }

  // ==========================================================================
  // [9] fix_plan A49 ticket 2(a) / DEVIATION D45 — THE SELECTION SURVIVES A
  //     RESTART, AND THE PIN COMES BACK ON IT.
  //
  // MEASURED before this ticket: FohPersist carried no CSS state at all, so a
  // pick had NEVER survived a restart on any port. The restart here is the
  // real one — foh_persist_save to disk, a FRESH FohState, foh_persist_load,
  // and foh_app.c's own boot order (foh_init -> apply -> tick) — not a struct
  // copied over a struct.
  {
    // (i) the SAVE POINT itself. Both drivers now ask one shared predicate
    // (foh_is_save_point), so this asserts the wiring, not a copy of it.
    PlatformInput in;
    memset(&in, 0, sizeof in);
    bool sawSavePoint = false;
    // The event buffer is PER TICK — foh_tick clears it — so the scan has to
    // happen inside the loop. Draining it once at the end would have found an
    // empty buffer and reported the wiring missing, which is how an
    // instrument passes or fails for a reason that is not its subject.
    for (int i = 0; i < 40; i++) {
      in.b = true;
      foh_tick(&d, &in);
      for (int e = 0; e < d.nev; e++) {
        if (d.ev[e].kind == FOH_EV_TRANS && strcmp(d.ev[e].from, "css") == 0 &&
            foh_is_save_point(&d.ev[e])) {
          sawSavePoint = true;
        }
      }
    }
    if (!sawSavePoint) {
      bad("backing out of the CSS emitted no transition that "
          "foh_is_save_point() calls a save point — the selection would be "
          "collected by nobody and the restart below would be a fiction");
    }
    if (strcmp(foh_screen_token(d.screen), "css") == 0) {
      bad("30 frames of B did not leave the CSS");
    }
    ok("leaving the CSS is a persistence save point, through the one "
       "predicate both drivers ask");

    // (ii) collect -> save -> load, on disk.
    FohPersist saved;
    foh_persist_defaults(&saved);
    foh_persist_collect(&saved, &d);
    for (int k = 0; k < FOH_CSS_PORTS; k++) {
      if (saved.selChar[k] != pick[k]) {
        bad("foh_persist_collect took port %d's selection as %s, want %s", k,
            kCharName[saved.selChar[k]], kCharName[pick[k]]);
      }
    }
    foh_persist_save(&saved);

    FohPersist loaded;
    const FohPersistStatus st = foh_persist_load(&loaded);
    if (st != FOH_PERSIST_LOADED) {
      bad("re-loading the file this run just wrote returned status %d, want "
          "FOH_PERSIST_LOADED(0) — a bump that cannot read its own output "
          "would silently reset every setting on the owner's device",
          (int)st);
      fprintf(stderr, "foh_p34_witness: %d failure(s)\n", g_fails);
      return 1;
    }
    // The bump must not have cost anything that was already persisted. These
    // are the v1..v5 planes, checked through the SAME round trip.
    if (loaded.ctlStyle != saved.ctlStyle || loaded.modOnR != saved.modOnR ||
        loaded.turbo != saved.turbo || loaded.lCancelType != saved.lCancelType ||
        loaded.phantomThreshold != saved.phantomThreshold ||
        loaded.masterVolume[0] != saved.masterVolume[0] ||
        loaded.masterVolume[1] != saved.masterVolume[1]) {
      bad("MLFKPERSIST7 did not round-trip the settings v1..v5 already "
          "carried — a half-done bump drops the player's settings, which is "
          "worse than not shipping the feature");
    }
    for (int c = 0; c < FOH_PERSIST_CHARS; c++) {
      for (int t = 0; t < FOH_PERSIST_TSTAGES; t++) {
        if (loaded.targetRecords[c][t] != saved.targetRecords[c][t]) {
          bad("MLFKPERSIST7 did not round-trip target record [%d][%d]", c, t);
        }
      }
    }

    // (iii) THE RESTART. A fresh machine, booted the way foh_app.c boots it.
    FohState r;
    reach_css_boot(&r, &loaded);
    for (int k = 0; k < FOH_CSS_PORTS; k++) {
      if (r.selChar[k] != pick[k]) {
        bad("after a restart port %d's selection is %s, want %s — the owner "
            "asked for the LAST CHARACTER", k, kCharName[r.selChar[k]],
            kCharName[pick[k]]);
      }
      if (token_cell(&r, k) != pick[k]) {
        bad("after a restart port %d's pin sits on cell %d, want %d — the "
            "token plane is re-homed FROM the selection at boot (D21/D35/D46 "
            "again), never left on a memset marth", k, token_cell(&r, k),
            pick[k]);
      }
    }
    ok("after a real save/load/reboot all four ports read falco/falcon/puff/"
       "fox, and every pin sits on its own port's character");

    // (iv) THE DESIGN ANSWER, asserted rather than only written down: the
    // port TYPES are deliberately NOT persisted (foh_persist.h carries the
    // argument). A restart must open the CSS on foh_init's own state — port 0
    // HMN, the rest N/A — and NOT ready to fight off a configuration the
    // player last saw in another session. It matters more since A49: a CPU on
    // port 2 or 3 is playable but not checksum-verified, and that must never
    // be a device's default boot state.
    for (int k = 0; k < FOH_CSS_PORTS; k++) {
      if (foh_css_port_type(&r, k) != (k == 0 ? 0 : -1)) {
        bad("after a restart port %d's type is %d, want %d — types are NOT "
            "persisted, by design", k, foh_css_port_type(&r, k),
            k == 0 ? 0 : -1);
      }
      if (foh_css_port_diff(&r, k) != 3) {
        bad("after a restart port %d's CPU level is %d, want the default 3",
            k, foh_css_port_diff(&r, k));
      }
    }
    ok("...while the port TYPES and CPU levels are NOT restored: the CSS "
       "opens on HMN/N-A/N-A/N-A, exactly as a fresh install does");

    // (v) THE MIGRATION ARM. An OLDER file must be carried forward, never
    // reset: resetting a valid v5 record would destroy every target-test
    // personal best on the owner's device. Build a genuine v5 file from the
    // CURRENT bytes on disk — drop EVERY row appended after v5, restamp the
    // header, recompute the seal — and require the loader to migrate it.
    //
    // "EVERY row appended after v5" is the whole discipline, and it is why
    // this block has to be revisited on every bump: A26 added `resume` and a
    // fixture that dropped only `sel` was a v5 header over v7 content, which
    // the loader rightly refuses — so the tooth failed as a MIGRATION defect
    // that did not exist. A49 left the same trap for A26 in the rebind
    // witness. If you append a row to the format, strip it here.
    {
      char path[512];
      snprintf(path, sizeof path, "%s/mlfk-persist.dat", foh_persist_dir());
      FILE *f = fopen(path, "rb");
      if (!f) { bad("cannot re-open the file just saved"); return 1; }
      static char buf[8192];
      size_t n = fread(buf, 1, sizeof buf - 1, f);
      fclose(f);
      buf[n] = 0;
      char *sum = strstr(buf, "\nSUM ");
      if (!sum) {
        bad("the saved file has no SUM line");
        return 1;
      }
      // Splice out every post-v5 row, LAST FIRST so the earlier pointers stay
      // valid, then restamp the header to v5.
      // ticket #25 appended eight more v7 rows, ticket #26 three more and
      // ticket #27 six more; a v5 file carries none of them either, and
      // leaving one in would make this file CORRUPT rather than migratable —
      // the leg would then refuse for a reason that has nothing to do with
      // the migration it is named after. LAST FIRST, so this list is in
      // REVERSE file order and ticket #27's six lead it.
      // D59's `crec` is post-v5 too, and unlike every row below it there are
      // FIFTY of them — one splice would leave forty-nine behind and the file
      // would be corrupt rather than migratable. It is LAST in the file
      // (appended after ctlrow), so it goes first, and the loop runs until
      // none remain rather than a counted fifty: the count is the format's
      // business, not this witness's.
      for (;;) {
        char *row = strstr(buf, "\ncrec ");
        if (!row) break;
        sum = strstr(buf, "\nSUM ");
        if (!sum || row > sum) {
          bad("a `crec` row sits after the SUM line");
          return 1;
        }
        memmove(row, strchr(row + 1, '\n'), strlen(strchr(row + 1, '\n')) + 1);
      }
      static const char *const kPostV5[] = {
          "\nctlrow ",   "\naudiorow ", "\noptcol ",   "\noptrow ",
          "\nssscur ",   "\nmenusel ",  "\ntsshand ",  "\ntsspage ",
          "\ntsscur ",   "\nhandtype ", "\ncpucarry ", "\ncarry ",
          "\nslider ",   "\nhand ",     "\nvsmode ",   "\ncpudiff ",
          "\nptype ",    "\nresume ",   "\nsel "};
      for (size_t r = 0; r < sizeof kPostV5 / sizeof *kPostV5; r++) {
        char *row = strstr(buf, kPostV5[r]);
        sum = strstr(buf, "\nSUM ");
        if (!row || !sum || row > sum) {
          bad("the saved file has no `%s` row before its SUM — an appended "
              "block is not where the format says it is", kPostV5[r] + 1);
          return 1;
        }
        memmove(row, strchr(row + 1, '\n'), strlen(strchr(row + 1, '\n')) + 1);
      }
      memcpy(buf + 11, "5", 1); // "MLFKPERSIST<current>" -> "...5"
      char *body = strstr(buf, "\nSUM ");
      const size_t bodyLen = (size_t)(body - buf) + 1;
      char hex[65];
      ml_sha256_hex(buf, bodyLen, hex);
      snprintf(body + 1, sizeof buf - bodyLen - 1, "SUM %s\n", hex);
      f = fopen(path, "wb");
      if (!f) { bad("cannot rewrite the file as v5"); return 1; }
      fwrite(buf, 1, strlen(buf), f);
      fclose(f);

      FohPersist mig;
      const FohPersistStatus ms = foh_persist_load(&mig);
      if (ms != FOH_PERSIST_LOADED) {
        bad("a valid MLFKPERSIST5 file did not MIGRATE, it returned status "
            "%d — an upgrade must never discard a player's settings and "
            "target records", (int)ms);
      }
      if (mig.ctlStyle != saved.ctlStyle || mig.modOnR != saved.modOnR ||
          mig.masterVolume[0] != saved.masterVolume[0]) {
        bad("the v5 migration lost settings the older format did carry");
      }
      for (int k = 0; k < FOH_CSS_PORTS; k++) {
        if (mig.selChar[k] != 0) {
          bad("the v5 migration invented a selection for port %d (%s) — a v5 "
              "file has no opinion about characters, so every port must take "
              "the fresh-install marth", k, kCharName[mig.selChar[k]]);
        }
      }
      ok("an MLFKPERSIST5 file MIGRATES: its settings and records survive and "
         "every port takes marth, because v5 never stored a character");
    }
  }

  if (g_fails) {
    fprintf(stderr, "foh_p34_witness: %d failure(s)\n", g_fails);
    return 1;
  }
  printf("P34 WITNESS OK\n");
  return 0;
}
