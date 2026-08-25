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

static void reach_css(FohState *s) {
  foh_init(s);
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

// The centre of port k's type tab, from the SAME constants foh.c hit-tests
// and foh_render.c draws (DEVIATION D4's single source).
static void tab_centre(int k, double *x, double *y) {
  *x = (double)foh_css_panel_x(k) + (double)FOH_CSS_TAB_W / 2.0;
  *y = (double)FOH_CSS_PANEL_Y + (double)FOH_CSS_TAB_H / 2.0;
}

static const char *const kCharName[5] = {"marth", "puff", "fox", "falco",
                                         "falcon"};

int main(void) {
  FohState s;
  reach_css(&s);

  // --- [1] the type tabs of ports 2 and 3 -----------------------------------
  for (int port = 2; port <= 3; port++) {
    const char *field = port == 2 ? "p3type" : "p4type";
    double tx, ty;
    tab_centre(port, &tx, &ty);
    if (foh_css_port_type(&s, port) != -1) {
      bad("port %d does not start at N/A (-1), it is %d", port,
          foh_css_port_type(&s, port));
    }
    if (!walk_to(&s, tx, ty)) return 1;

    press_a_expect(&s, "menuSelect", field, 0, "turning the port on");
    if (foh_css_port_type(&s, port) != 0) {
      bad("port %d did not become HMN(0) on the first press, it is %d — the "
          "owner's whole ticket is this press", port,
          foh_css_port_type(&s, port));
    }
    press_a_expect(&s, "menuSelect", field, -1, "turning the port off");
    if (foh_css_port_type(&s, port) != -1) {
      bad("port %d's cycle did not wrap N/A after HMN, it is %d — a CPU here "
          "would launch a match the sim cannot replay (D40(b))", port,
          foh_css_port_type(&s, port));
    }
    press_a_expect(&s, "menuSelect", field, 0, "turning the port back on");
    if (foh_css_port_type(&s, port) != 0) {
      bad("port %d did not return to HMN(0), it is %d", port,
          foh_css_port_type(&s, port));
    }
    ok("port %d's type box cycles N/A -> HMN -> N/A under a real A press, and "
       "never reaches CPU", port);
  }

  // --- [2] take port 2's token and give it a character ----------------------
  const int wantChar = 3; // falco — NOT cell 0, and not port 2's index
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

    FohHandRect cells[5];
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
    const int want[FOH_CSS_PORTS] = {0, 0, wantChar, 0};
    for (int k = 0; k < FOH_CSS_PORTS; k++) {
      if (s.selChar[k] != want[k]) {
        bad("port %d's selection is %s, want %s — a port index and a roster "
            "index have changed places (CONTEXT.md \"Port\")", k,
            kCharName[s.selChar[k]], kCharName[want[k]]);
      }
    }
    ok("the selection plane reads marth/marth/%s/marth — only PORT 2 moved",
       kCharName[wantChar]);
  }

  // --- [4] the token plane agrees with the selection ------------------------
  {
    double tx, ty;
    foh_css_token_pos(&s, 2, &tx, &ty);
    FohHandRect cells[5];
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
    const int wantCharCfg[FOH_CSS_PORTS] = {0, 0, wantChar, 0};
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

  if (g_fails) {
    fprintf(stderr, "foh_p34_witness: %d failure(s)\n", g_fails);
    return 1;
  }
  printf("P34 WITNESS OK\n");
  return 0;
}
