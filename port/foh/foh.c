// port/foh/foh.c — the FOH screen machine (fix_plan §M4 task 9). Flow
// graph + selection semantics per the foh.h header table (every edge
// cited there from the upstream primary source); rewritten navigation
// (d-pad edges, row cursors) per the pre-registered deltas (AGENT-LOG
// iter 88). No RNG, no wall clock, no I/O — a pure (state, input) ->
// state step, so flow traces and screenshots are byte-stable by
// construction.
#include "foh.h"

#include <string.h>

// Upstream menuMode constants (menu.js:44-47) mapped onto FOH screens.
static const int kMenuCount[FOH_SCREEN_COUNT] = {
    // menuCount = [4, 4, 4, 2] (menu.js:31), indexed here by screen
    [FOH_MENU_TOP] = 4,
    [FOH_MENU_OPTIONS] = 4,
    [FOH_MENU_BATTLE] = 4,
    [FOH_MENU_CONTROLS] = 2,
};

const char *foh_screen_token(FohScreen sc) {
  switch (sc) {
    case FOH_STARTUP: return "startup";
    case FOH_TITLE: return "title";
    case FOH_MENU_TOP: return "menu-top";
    case FOH_MENU_OPTIONS: return "menu-options";
    case FOH_MENU_BATTLE: return "menu-battle";
    case FOH_MENU_CONTROLS: return "menu-controls";
    case FOH_CSS: return "css";
    case FOH_SSS: return "sss";
    case FOH_OPT_GAMEPLAY: return "options-gameplay";
    case FOH_MATCH: return "match";
    case FOH_TSS: return "target-select";
    case FOH_TMATCH: return "target-match";
    default: gfx_fatal("foh: screen token for an invalid screen");
  }
}

void foh_init(FohState *s) {
  memset(s, 0, sizeof *s);
  s->screen = FOH_STARTUP;
  // characterSelections default [0,0,0,0] (main.js:59) -> marth/marth.
  s->p1Char = 0;
  s->p2Char = 0;
  // playerType init is [-1,-1,-1,-1] (main.js:107) and addPlayer (called at
  // main.js:386 when START is pressed on the title) assigns HMN at
  // main.js:495. So
  // the CSS opens with ONE participant, i.e. NOT ready — clicking a second
  // port's type box is what raises READY TO FIGHT (css.js:1167-1181). That is
  // the whole point of MENU-SPEC item 4: the banner is the screen's feedback
  // channel, not decoration.
  s->p1Type = 0;
  s->p2Type = -1;
  s->p1Difficulty = 3; // cpuDifficulty default (main.js:109)
  s->difficulty = 3;
  s->cssCarry = -1;    // whichTokenGrabbed (css.js:68)
  s->cssCpuCarry = -1; // whichCpuGrabbed (css.js:75)
  // cpuSlider[k] init, css.js:72: x = 152+15+166+225k-50 = 283+225k on a rail
  // running [167+225k, 333+225k], i.e. 116/166 = 0.6988 of the way along —
  // NOT the level-3 stop at 2/3, though it reads back as level 3
  // (round(0.6988*3)+1 == 3). Carried as the same fraction of our rail.
  for (int k = 0; k < 2; k++) {
    s->cssSliderX[k] = (double)(foh_css_panel_x(k) + FOH_CSS_RAIL_X0) +
                       (116.0 / 166.0) * (double)FOH_CSS_RAIL_LEN;
  }
  // handPos[0] = (140,700) on upstream's 1200x750 canvas (css.js:64) — the
  // same fraction of this screen. Module scope upstream: set ONCE here and
  // never re-initialised on CSS entry (MENU-SPEC §2.2 property 4).
  s->cssHandX = 140.0 * RAST_W / 1200.0;
  s->cssHandY = 700.0 * RAST_H / 750.0;
  // gameSettings defaults (settings.js:44-56): all zero — memset did it.
  // targetRecords fresh state is -1, NOT 0 (targetplay.js:40) — 0 would
  // read as a valid 0-second record (task 13).
  for (int c = 0; c < 5; c++) {
    for (int t = 0; t < 10; t++) s->targetRecords[c][t] = -1.0;
  }
  // LOOK plane (A1 restyle Phase 0; foh.h). menuColours / menuCurColour
  // literals are menu.js:34-35 — presentation constants of a rewritten,
  // non-checksummed surface, not engine data.
  s->menuHue = 238.0;
  s->menuColours[0] = 238.0;
  s->menuColours[1] = 358.0;
  s->menuColours[2] = 117.0;
  s->menuColours[3] = 55.0;
}

static void ev_push(FohState *s, FohEvent e) {
  if (s->nev >= FOH_EV_CAP) gfx_fatal("foh: event buffer overflow");
  s->ev[s->nev++] = e;
}

// Menu SFX token (M4 task 10; foh.h note). Upstream mapping, cited per
// emission site below: menuSelect = cursor/value steps + option
// toggles (menu.js:236, css.js:226-class, stageselect.js:64/73,
// gameplaymenu.js:38/152); menuForward = confirm transitions + title
// START + css->sss + sss launch (menu.js:70, main.js:388, css.js:448,
// stageselect.js:81); menuBack = every B back incl. bhold
// (menu.js:170-190, css.js:189, stageselect.js:78, gameplaymenu.js:26);
// deny = refused entries (keyboardmenu.js:170 refusal class).
// The iter-93 exclusion of the CSS announcer names is RETIRED: the token
// drop exists now, so css.js:233/248/263/278/293's per-character
// announcer plays at the drop (kCssAnnouncer below; all five are real
// SND1 Howl names, sfx.js:15/305/380/434/572).
static void snd_push(FohState *s, const char *name) {
  if (s->nsnd >= FOH_EV_CAP) gfx_fatal("foh: sound buffer overflow");
  s->snd[s->nsnd++] = name;
}

static void ev_trans(FohState *s, FohScreen from, FohScreen to,
                     const char *cause) {
  FohEvent e;
  memset(&e, 0, sizeof e);
  e.kind = FOH_EV_TRANS;
  e.from = foh_screen_token(from);
  e.to = foh_screen_token(to);
  e.cause = cause;
  ev_push(s, e);
  s->screen = to;
}

static void ev_sel(FohState *s, const char *field, int val) {
  FohEvent e;
  memset(&e, 0, sizeof e);
  e.kind = FOH_EV_SEL;
  e.field = field;
  e.val = val;
  ev_push(s, e);
}

static void ev_refused(FohState *s, const char *entry) {
  FohEvent e;
  memset(&e, 0, sizeof e);
  e.kind = FOH_EV_SEL;
  e.field = "refused";
  e.sval = entry;
  ev_push(s, e);
}

static void ev_launch(FohState *s) {
  FohEvent e;
  memset(&e, 0, sizeof e);
  e.kind = FOH_EV_LAUNCH;
  ev_push(s, e);
}

// Clamp helper for the rewritten value rows (pre-registered: CLAMP at
// the ends, no wrap).
static int clampi(int v, int lo, int hi) {
  return v < lo ? lo : (v > hi ? hi : v);
}

// --- per-screen steps -------------------------------------------------------

static void step_menu(FohState *s, const PlatformInput *in,
                      const PlatformInput *pv) {
  const FohScreen sc = s->screen;
  const int count = kMenuCount[sc];
  const bool aE = in->a && !pv->a;
  const bool bE = in->b && !pv->b;
  const bool uE = in->up && !pv->up;
  const bool dE = in->down && !pv->down;
  if (aE) {
    switch (sc) {
      case FOH_MENU_TOP:
        // menuText[0] = ["VS. Melee","Target Test","Target Builder",
        // "Options"] (menu.js:19-24); routing menu.js:66-100.
        if (s->menuSelected == 0) {
          s->menuSelected = 0; // LOCALVS (menu.js:74)
          snd_push(s, "menuForward"); // menu.js:70
          ev_trans(s, sc, FOH_MENU_BATTLE, "a");
        } else if (s->menuSelected == 1) {
          // TARGETTEST (menu.js:77-84): setTargetPlayer(0) is implicit
          // (slot 0 is the only port); targetPointerPos reset == the
          // rewritten cursor reset; the device app switches the music
          // to the targettest track at the TLAUNCH seam (REGISTERED
          // rewrite delta, AGENT-LOG iter 99 — an SD ring prefill
          // inside the paced FOH loop risks the skips==0 gate;
          // upstream switches here, menu.js:82-83).
          s->tssCursor = 0;
          snd_push(s, "menuForward"); // menu.js:70
          ev_trans(s, sc, FOH_TSS, "a"); // changeGamemode(7), menu.js:84
        } else if (s->menuSelected == 2) {
          snd_push(s, "deny");
          ev_refused(s, "targetbuilder"); // conventions scope exclusion
        } else {
          s->menuSelected = 0; // AUDIOOPTIONS (menu.js:96)
          snd_push(s, "menuForward"); // menu.js:70
          ev_trans(s, sc, FOH_MENU_OPTIONS, "a");
        }
        break;
      case FOH_MENU_BATTLE:
        // ["Local VS","Spectate","P2P","Server"]; only Local VS is
        // in-scope (multiplayer excluded; P2P dead upstream).
        if (s->menuSelected == 0) {
          snd_push(s, "menuForward"); // menu.js:70
          ev_trans(s, sc, FOH_CSS, "a"); // menu.js:105
        } else if (s->menuSelected == 1) {
          snd_push(s, "deny");
          ev_refused(s, "spectate");
        } else if (s->menuSelected == 2) {
          snd_push(s, "deny");
          ev_refused(s, "p2p");
        } else {
          snd_push(s, "deny");
          ev_refused(s, "server");
        }
        break;
      case FOH_MENU_OPTIONS:
        // ["Audio","Gameplay","Keyboard Controls","Credits"]
        if (s->menuSelected == 0) {
          snd_push(s, "deny");
          ev_refused(s, "audio"); // mixer volume surface, tasks 10/13
        } else if (s->menuSelected == 1) {
          s->optRow = 0;
          s->optCol = 0;
          snd_push(s, "menuForward"); // menu.js:70
          ev_trans(s, sc, FOH_OPT_GAMEPLAY, "a"); // menu.js:135
        } else if (s->menuSelected == 2) {
          s->menuSelected = 0;
          snd_push(s, "menuForward"); // menu.js:70
          ev_trans(s, sc, FOH_MENU_CONTROLS, "a"); // menu.js:138-141
        } else {
          snd_push(s, "deny");
          ev_refused(s, "credits"); // conventions scope exclusion
        }
        break;
      case FOH_MENU_CONTROLS:
        // ["Controller","Keyboard"] — both are calibration screens; the
        // S1 mapping is Chase-ratified hardware surface (registered).
        snd_push(s, "deny");
        ev_refused(s, s->menuSelected == 0 ? "controller" : "keyboard");
        break;
      default: gfx_fatal("foh: step_menu on a non-menu screen");
    }
    return;
  }
  if (bE) {
    // menu.js:164-190 verbatim back edges (cursor values included).
    if (sc == FOH_MENU_CONTROLS) {
      s->menuSelected = 0; // AUDIOOPTIONS
      snd_push(s, "menuBack"); // menu.js:170-190
      ev_trans(s, sc, FOH_MENU_OPTIONS, "b");
    } else if (sc == FOH_MENU_OPTIONS) {
      s->menuSelected = 3; // OPTIONS
      snd_push(s, "menuBack"); // menu.js:170-190
      ev_trans(s, sc, FOH_MENU_TOP, "b");
    } else if (sc == FOH_MENU_BATTLE) {
      s->menuSelected = 0; // VSMODE
      snd_push(s, "menuBack"); // menu.js:170-190
      ev_trans(s, sc, FOH_MENU_TOP, "b");
    }
    // top level: B does nothing (no upstream arm)
    return;
  }
  // cursor with wrap (menu.js:192-242 wraps via menuCount)
  if (uE) {
    s->menuSelected = (s->menuSelected + count - 1) % count;
    snd_push(s, "menuSelect"); // menu.js:236
  }
  if (dE) {
    s->menuSelected = (s->menuSelected + 1) % count;
    snd_push(s, "menuSelect"); // menu.js:236
  }
}

// --- CSS: the free hand cursor + the token model (MENU-SPEC §2) -------------
// Structure below follows cssControls (css.js:182-461) arm for arm: B-hold,
// hand motion + clamp, then the three-way branch on (roster band / dragging a
// CPU knob / everything else), then the knob-grab loop and the launch check
// that upstream runs OUTSIDE that branch, then the draw pass's readyToFight.
//
// The announcer that plays when a token is dropped on a cell
// (css.js:233/248/263/278/293). Roster order marth-puff-fox-falco-falcon.
static const char *const kCssAnnouncer[5] = {"marth", "jigglypuff", "fox",
                                             "falco", "falcon"};

int foh_css_port_type(const FohState *s, int k) {
  // DEVIATION D6: ports 3 and 4 are pinned N/A — they render (upstream draws
  // all four type tabs unconditionally, css.js:860-879, and gates only the
  // per-port PREVIEW on playerType > -1, css.js:881-882) but own no type
  // field and no token.
  return k == 0 ? s->p1Type : (k == 1 ? s->p2Type : -1);
}

int foh_css_port_diff(const FohState *s, int k) {
  return k == 0 ? s->p1Difficulty : s->difficulty;
}

void foh_css_token_pos(const FohState *s, int k, double *x, double *y) {
  if (s->cssCarry == k) { // the token rides the hand (css.js:218)
    *x = s->cssHandX;
    *y = s->cssHandY;
    return;
  }
  const int c = s->cssChar[k]; // the TOKEN plane (css.js:66), not setCS's
  // Quirk Q1 (foh.h): the resting slot depends on HOW the token got there.
  double base;
  if (s->cssTokenRest[k] == 0) { // A-drop (css.js:288 family)
    base = (double)(foh_css_cell_x(c) + FOH_CSS_TOKEN_DX);
  } else { // leave-band (css.js:337) — the base/pitch really do differ
    base = (double)(foh_css_cell_x(0) + FOH_CSS_TOKEN_DX +
                    FOH_CSS_TOKEN_LB_DX + FOH_CSS_TOKEN_LB_PITCH * c);
  }
  // Upstream never needs this: its canvas has ~270 px of margin right of the
  // roster, ours has none, so the quirk's +1-cell shift pushes the falcon slot
  // past the right edge. The PAIR is shifted, not each token saturated: two
  // ports must stay DISTINCT and both fully on screen, or they would overlap
  // at x = 239 and the grab loop's j-order would silently decide which one a
  // press takes. Registered as a layout consequence of D4, not a silent fix.
  {
    const double maxBase =
        (double)(RAST_W - 1 - FOH_CSS_TOKEN_PITCH - FOH_CSS_TOKEN_R);
    if (base > maxBase) base = maxBase;
  }
  *x = base + (double)(FOH_CSS_TOKEN_PITCH * k);
  *y = (double)FOH_CSS_TOKEN_Y;
}

int foh_css_hand_type(const FohState *s) { return s->cssHandType; }

double foh_css_knob_x(const FohState *s, int k) { return s->cssSliderX[k]; }
double foh_css_knob_y(void) {
  return (double)(FOH_CSS_PANEL_Y + FOH_CSS_RAIL_Y);
}
// The rail's left end for port k, and the level the slider position reads as.
static double css_rail_x0(int k) {
  return (double)(foh_css_panel_x(k) + FOH_CSS_RAIL_X0);
}
static int css_level_at(int k, double x) {
  // css.js:325-327 with upstream's 166 px travel replaced by ours: the track
  // parameter t in [0,1] maps round(t*3)+1, so the domain is exactly 1..4.
  return (int)((x - css_rail_x0(k)) * 3.0 / (double)FOH_CSS_RAIL_LEN + 0.5) + 1;
}

// Which character cell the hand is over, or -1. Contiguous upstream; here the
// cells are drawn with a 2 px gutter, and D4 forbids a hit region where
// nothing is drawn, so the gutter is genuinely no cell.
static int css_cell_at(double x) {
  for (int c = 0; c < 5; c++) {
    const double x0 = (double)foh_css_cell_x(c);
    if (x > x0 && x < x0 + (double)FOH_CSS_CELL_W) return c;
  }
  return -1;
}

static int *css_char_of(FohState *s, int k) {
  return &s->cssChar[k];
}
static int *css_diff_of(FohState *s, int k) {
  return k == 0 ? &s->p1Difficulty : &s->difficulty;
}

// readyToFight, css.js:1167-1181 verbatim in shape — including the fact that
// `readyPlayers >= 2` is re-decided on every participating port, so the last
// one seen wins, and that a held token breaks out immediately.
// occupiedToken[k] is `cssCarry == k`: with one hand nothing else can hold it.
static bool css_ready(const FohState *s) {
  int readyPlayers = 0;
  bool ready = false;
  for (int k = 0; k < 4; k++) {
    if (foh_css_port_type(s, k) > -1) {
      readyPlayers++;
      ready = (readyPlayers >= 2);
      if (s->cssCarry == k) {
        ready = false;
        break;
      }
    }
  }
  return ready;
}

// The B-hold exit edge (css.js:186-194). menuMode is untouched upstream, so
// we return to menu-battle with its cursor where the VS entry left it.
// The sound is emitted at the counter site, not here (see step_css).
static void css_back(FohState *s) {
  s->menuSelected = 0; // LOCALVS
  ev_trans(s, FOH_CSS, FOH_MENU_BATTLE, "bhold");
}

static void step_css(FohState *s, const PlatformInput *in,
                     const PlatformInput *pv) {
  const bool aE = in->a && !pv->a;
  const bool bE = in->b && !pv->b;
  bool allowRegrab = true; // css.js:183

  // B held 30 consecutive frames -> back to the menu (css.js:186-194;
  // menuMode is untouched upstream, so we return to menu-battle). Note the
  // deliberate overlap with the B-grab below: one B press both starts this
  // counter and retrieves your token (MENU-SPEC §2.11).
  // The counter is NOT reset: upstream's `bHold[i] == 30` equality is what
  // makes this fire exactly once per hold (css.js:188), and resetting would
  // re-arm it 30 frames later while B is still down.
  //
  // The transition is PENDING, not immediate. Upstream calls changeGamemode(1)
  // here and then runs the WHOLE remainder of cssControls (css.js:186-461):
  // the same frame still integrates the hand, can grab or drop a token, can
  // click a type box, and — if START is also down — calls changeGamemode(6),
  // which wins because it is last. Returning here instead would drop all of
  // that, so the pending edge is emitted at the very end of the step and any
  // transition taken in between simply supersedes it.
  bool pendingBack = false;
  if (in->b) {
    s->bHold++;
    if (s->bHold == 30) {
      // The SOUND fires HERE, at upstream's site (css.js:189), not with the
      // deferred transition: upstream plays menuBack before the rest of
      // cssControls runs, so on a B+START frame the pair is menuBack then
      // menuForward, and on a B+A frame the action's sound follows menuBack.
      // Only the structural edge is deferred.
      snd_push(s, "menuBack");
      pendingBack = true;
    }
  } else {
    s->bHold = 0;
  }

  // Hand motion, unconditional every frame (css.js:195-206). DEVIATION D1:
  // the d-pad supplies lsX/lsY at full deflection only; DEVIATION D3 rescales
  // the 12 px/frame step. Position stays in DOUBLES and is clamped to the
  // screen, so the cursor is always recoverable (MENU-SPEC §2.2 property 3).
  {
    const double lsX = (in->right ? 1.0 : 0.0) - (in->left ? 1.0 : 0.0);
    const double lsY = (in->up ? 1.0 : 0.0) - (in->down ? 1.0 : 0.0);
    s->cssHandX += lsX * FOH_CURSOR_VX;
    s->cssHandY += -lsY * FOH_CURSOR_VY;
    // Clamped to the LOGICAL canvas, exactly as upstream clamps to 1200/750
    // (its canvas dimensions, not its last pixel index). Rounding to a pixel
    // happens only at draw time.
    if (s->cssHandX > (double)RAST_W) s->cssHandX = (double)RAST_W;
    else if (s->cssHandX < 0.0) s->cssHandX = 0.0;
    if (s->cssHandY > (double)RAST_H) s->cssHandY = (double)RAST_H;
    else if (s->cssHandY < 0.0) s->cssHandY = 0.0;
  }

  if (s->cssHandY < (double)FOH_CSS_BAND_BOT &&
      s->cssHandY > (double)FOH_CSS_BAND_TOP) {
    // --- roster band (css.js:207-315) ---------------------------------------
    s->cssHandType = 1; // handOpen (css.js:208)
    // (a) B on your OWN token, from anywhere in the band: the token teleports
    // to the hand (css.js:209-215). HMN-only, and only if empty-handed.
    if (bE && s->p1Type == 0 && s->cssCarry == -1) {
      s->cssHandType = 2; // css.js:210
      s->cssCarry = 0;
      ev_sel(s, "carry", 0);
    }
    if (s->cssCarry >= 0) {
      // --- carrying (css.js:216-296) ---
      s->cssHandType = 2; // css.js:217
      const int k = s->cssCarry;
      if (s->cssHandY > (double)FOH_CSS_CELL_Y &&
          s->cssHandY < (double)(FOH_CSS_CELL_Y + FOH_CSS_CELL_H)) {
        const int c = css_cell_at(s->cssHandX);
        if (c >= 0) {
          int *ch = css_char_of(s, k);
          if (*ch != c) {
            // Hovering SELECTS, live — not on press (css.js:222-226 and its
            // four siblings). The != guard is what makes the sound fire once
            // per change rather than every frame. Upstream writes BOTH planes
            // here: `chosenChar[k] = c` inline, then changeCharacter's setCS
            // (css.js:165-166) moves the shared selection — which is why the
            // two are separate fields and only this site writes both.
            *ch = c;
            if (k == 0) s->p1Char = c; else s->p2Char = c;
            snd_push(s, "menuSelect"); // css.js:225
            ev_sel(s, k == 0 ? "p1char" : "p2char", c);
          }
          if (aE) {
            // A drops it into the hovered cell and the character's announcer
            // plays (css.js:227-233). handType is deliberately NOT cleared:
            // upstream leaves it at 2 for this draw, so the grab sprite shows
            // one more frame.
            s->cssCarry = -1;
            s->cssTokenRest[k] = 0; // the A-drop slot (quirk Q1)
            snd_push(s, kCssAnnouncer[c]);
            ev_sel(s, "carry", -1);
          }
        }
      }
    } else {
      // (b) A on a token you may take (css.js:297-313). The guard
      // `playerType[j] == 1 || i == j` is upstream's ownership rule: your own
      // token, or the token of any port set to CPU — never another human's.
      // occupiedToken[j] is false for every j here (nothing is carried).
      // NOTE the `j == 0` arm makes YOUR OWN token grabbable even at N/A,
      // where nothing is drawn (foh.h D4 exception (b)) — upstream's own
      // asymmetry between this guard and its token draw, carried verbatim.
      for (int j = 0; j < 2; j++) { // D6: ports 3/4 own no token
        if (!(foh_css_port_type(s, j) == 1 || j == 0)) continue;
        double tx, ty;
        foh_css_token_pos(s, j, &tx, &ty);
        if (s->cssHandY > ty - (double)FOH_CSS_TOKEN_R &&
            s->cssHandY < ty + (double)FOH_CSS_TOKEN_R &&
            s->cssHandX > tx - (double)FOH_CSS_TOKEN_R &&
            s->cssHandX < tx + (double)FOH_CSS_TOKEN_R) {
          if (aE) {
            s->cssHandType = 2; // css.js:304
            s->cssCarry = j;
            ev_sel(s, "carry", j);
            break;
          }
        }
      }
    }
  } else if (s->cssCpuCarry >= 0) {
    // --- CPU-knob drag (css.js:316-334) -------------------------------------
    // The hand is LOCKED to the rail: y forced, x clamped to the track, and
    // the level re-derived from the position every frame.
    const int k = s->cssCpuCarry;
    const double x0 = css_rail_x0(k);
    // css.js:317 forces the hand to cpuSlider.y + 15 — BELOW the knob centre,
    // because the hand's hot spot is its fingertip and the glove hangs down.
    // 15 px of a 750-tall canvas is 2%, i.e. 4.8 px here (D4 scaling).
    s->cssHandY = foh_css_knob_y() + FOH_CSS_DRAG_DY;
    if (s->cssHandX < x0) s->cssHandX = x0;
    if (s->cssHandX > x0 + (double)FOH_CSS_RAIL_LEN) {
      s->cssHandX = x0 + (double)FOH_CSS_RAIL_LEN;
    }
    // css.js:324: the slider takes the RAW hand x, continuously — the knob
    // follows the hand rather than snapping to the level's four stops, and it
    // keeps that exact position after release so a re-grab hit-tests where the
    // knob is drawn.
    s->cssSliderX[k] = s->cssHandX;
    {
      const int v = css_level_at(k, s->cssSliderX[k]);
      int *d = css_diff_of(s, k);
      if (v != *d) {
        // No sound here: upstream's whole drag arm (css.js:316-334) plays
        // none. The structural event stays — the level IS launch state.
        *d = v;
        ev_sel(s, k == 0 ? "p1difficulty" : "difficulty", v);
      }
    }
    if (aE) { // release (css.js:328-333)
      s->cssCpuCarry = -1;
      s->cssHandType = 0; // css.js:332
      allowRegrab = false;
    }
  } else {
    // --- everything else (css.js:336-357) -----------------------------------
    s->cssHandType = 0; // handPoint (css.js:336)
    // Leaving the roster band while carrying SILENTLY COMMITS whatever was
    // last hovered and returns the token to rest (css.js:336-347). There is no
    // invalid-drop rejection and no snap-back-to-origin.
    //
    // QUIRK Q1 is CARRIED (foh.h FOH_CSS_TOKEN_LB_*): this rest slot uses
    // upstream's OTHER formula, so a token committed by walking out of the
    // band lands one cell right of the character it selected, exactly as
    // upstream's does. MENU-SPEC §2.6 says "carried verbatim" and it is; only
    // the spec's "a few px" magnitude is wrong (measured: 99 px, one cell).
    // QUIRK Q2 — upstream calls setTokenPosValue unconditionally, writing
    // tokenPos[-1] when nothing is carried; our rest position is computed from
    // cssTokenRest[k], so index -1 is unrepresentable and the write is a
    // no-op by construction rather than by a guard.
    if (s->cssCarry >= 0) {
      s->cssTokenRest[s->cssCarry] = 1; // the leave-band slot (quirk Q1)
      s->cssCarry = -1;
      ev_sel(s, "carry", -1);
    }
    // The port-type box is a clickable widget (css.js:348-357): any hand may
    // toggle any port's box. D6 stops the loop at 2.
    for (int j = 0; j < 2; j++) {
      const double px = (double)foh_css_panel_x(j);
      if (s->cssHandY > (double)FOH_CSS_PANEL_Y &&
          s->cssHandY < (double)(FOH_CSS_PANEL_Y + FOH_CSS_TAB_H) &&
          s->cssHandX > px && s->cssHandX < px + (double)FOH_CSS_TAB_W) {
        if (aE) {
          // togglePort, main.js:504-520, with DEVIATION D5: NET (2) is not a
          // reachable state, so the cycle wraps one step early —
          // N/A(-1) -> HMN(0) -> CPU(1) -> N/A. The `ports <= i` HMN-skip arm
          // is inert here (foh.h PORT MODEL note). Upstream also clears the
          // port's name tag; tags are DEVIATION D8 and not in this build.
          int *t = (j == 0) ? &s->p1Type : &s->p2Type;
          (*t)++;
          if (*t == 2) *t = -1;
          snd_push(s, "menuSelect"); // css.js:351
          ev_sel(s, j == 0 ? "p1type" : "p2type", *t);
        }
      }
    }
  }

  // CPU-knob grab (css.js:396-408) — outside the three-way branch, guarded on
  // not already dragging, and on allowRegrab so releasing cannot re-grab on
  // the same frame. occupiedCpu[] is implied: nothing is held here.
  if (s->cssCpuCarry == -1) {
    for (int j = 0; j < 2; j++) {
      if (foh_css_port_type(s, j) != 1) continue;
      const double kx = foh_css_knob_x(s, j), ky = foh_css_knob_y();
      if (s->cssHandY >= ky - (double)FOH_CSS_KNOB_R &&
          s->cssHandY <= ky + (double)FOH_CSS_KNOB_R &&
          s->cssHandX >= kx - (double)FOH_CSS_KNOB_R &&
          s->cssHandX <= kx + (double)FOH_CSS_KNOB_R) {
        if (aE && allowRegrab) {
          s->cssCpuCarry = j;
          s->cssHandType = 2; // css.js:406
          break;
        }
      }
    }
  }

  // Launch (css.js:443-451): START, rising edge, ONLY while ready. The two
  // d-pad arms that follow it upstream (css.js:452-459 — d-pad UP launches
  // unready, d-pad RIGHT force-selects falco) are dropped by DEVIATION D2, so
  // the not-ready state is inert, which is what the banner already says.
  // `cssReady` here is the value the DRAW pass left LAST frame.
  if (s->cssReady && in->start && !pv->start) {
    // LAUNCH-PLANE LIMITATION, registered and LOUD (the D6 family — a
    // launch-plane limit, not a menu-plane one). The menu is faithful: any
    // port may be CPU or N/A, and the ready rule is upstream's. The SIM is
    // not: sim_setup_match pins port 0 to HMN with a fixed level, so a CPU P1
    // would boot a HUMAN P1 and the launch record would be a lie (HARD RULE
    // 2). The N/A arm is upstream's own one-frame race — cssReady is last
    // frame's value (css.js:1167-1181 runs in the draw pass), so toggling a
    // port to N/A and pressing START on the SAME frame reaches here with a
    // configuration the ready rule would have rejected. Both refuse loudly
    // instead of launching something else; delete this arm when
    // sim_setup_match carries both ports' type and level.
    if (!(s->p1Type == 0 && (s->p2Type == 0 || s->p2Type == 1))) {
      snd_push(s, "deny");
      ev_refused(s, "portconfig");
      if (pendingBack) css_back(s);
      return;
    }
    snd_push(s, "menuForward"); // css.js:448
    // START WINS over a pending B-hold exit: upstream's changeGamemode(6) is
    // the later call in the same frame (css.js:448 vs :189).
    ev_trans(s, FOH_CSS, FOH_SSS, "start");
    return;
  }
  // The pending B-hold exit, emitted only now that the rest of the CSS step
  // has run (see the counter above). readyToFight's draw-pass recompute is
  // NOT here — it belongs to the screen the tick ENDS on, and foh_tick owns
  // it (see the note there).
  if (pendingBack) css_back(s);
}

static void step_sss(FohState *s, const PlatformInput *in,
                     const PlatformInput *pv) {
  const bool aE = in->a && !pv->a;
  const bool bE = in->b && !pv->b;
  if (bE) {
    snd_push(s, "menuBack"); // stageselect.js:78
    ev_trans(s, FOH_SSS, FOH_CSS, "b"); // stageselect.js:79
    return;
  }
  if (aE) {
    if (s->sssCursor == 6) {
      // The RANDOM slot REFUSES (registered exclusion, MEASURED:
      // upstream's arm draws from the seeded Math.random stream,
      // stageselect.js:80-84 — a live draw would desync
      // live-vs-replay stream prefixes; foh.h header note).
      snd_push(s, "deny");
      ev_refused(s, "random");
      return;
    }
    // setStageSelect + startGame (stageselect.js:80-88); the launch
    // record freezes here, the driver (foh_app / device app) owns the
    // actual sim boot.
    snd_push(s, "menuForward"); // stageselect.js:81
    s->stageSel = s->sssCursor;
    s->launched = true;
    ev_trans(s, FOH_SSS, FOH_MATCH, "launch");
    ev_launch(s);
    return;
  }
  // 3x2 grid cursor over stage ids 0..5 (== oracle --stage ids) plus
  // the RANDOM slot at 6 (below the grid): D from the bottom row
  // enters it, U returns to the middle bottom tile (4); L/R are
  // no-ops on it (CLAMP semantics, the rewritten-nav class).
  const bool uE = in->up && !pv->up;
  const bool dE = in->down && !pv->down;
  const bool lE = in->left && !pv->left;
  const bool rE = in->right && !pv->right;
  const int before = s->sssCursor;
  if (s->sssCursor == 6) {
    if (uE) s->sssCursor = 4;
  } else {
    if (lE) s->sssCursor = clampi(s->sssCursor - 1, 0, 5);
    if (rE) s->sssCursor = clampi(s->sssCursor + 1, 0, 5);
    if (uE && s->sssCursor >= 3) s->sssCursor -= 3;
    if (dE) {
      if (s->sssCursor <= 2) s->sssCursor += 3;
      else s->sssCursor = 6;
    }
  }
  if (s->sssCursor != before) {
    snd_push(s, "menuSelect"); // stageselect.js:64/73 change class
  }
}

// target-select (upstream stages/targetselect.js tssControls; the
// rewrite deltas + per-edge citations live in foh.h).
static void step_tss(FohState *s, const PlatformInput *in,
                     const PlatformInput *pv) {
  const bool aE = in->a && !pv->a;
  const bool sE = in->start && !pv->start;
  const bool bE = in->b && !pv->b;
  if (bE) {
    // targetselect.js:76-81: menuBack + playMenuLoop + changeGamemode(1);
    // menuSelected UNTOUCHED (module state — still TARGETTEST).
    snd_push(s, "menuBack");
    ev_trans(s, FOH_TSS, FOH_MENU_TOP, "b");
    return;
  }
  // char select — the upstream SHOULDER arms VERBATIM (targetselect.js:
  // 60-74: input.l = char-1 WRAP, input.r = char+1 WRAP; setCS writes
  // characterSelections[0] == p1Char, the SAME array the CSS edits).
  if (in->l && !pv->l) {
    s->p1Char = s->p1Char == 0 ? 4 : s->p1Char - 1; // :62-66 wrap
    snd_push(s, "menuSelect"); // :67
    ev_sel(s, "p1char", s->p1Char);
  } else if (in->r && !pv->r) {
    s->p1Char = s->p1Char == 4 ? 0 : s->p1Char + 1; // :68-73 wrap
    snd_push(s, "menuSelect"); // :74
    ev_sel(s, "p1char", s->p1Char);
  }
  if (aE || sE) { // targetselect.js:131 accepts START or A
    if (s->tssCursor == 10) {
      // "+ Add Code" — builder/share-code plane, scope-excluded
      // (registered; foh.h note): visible but REFUSING.
      snd_push(s, "deny");
      ev_refused(s, "addcode");
      return;
    }
    // targetselect.js:131-146: menuForward + setActiveStageTarget +
    // setTargetStagePlaying + startTargetGame(0, false); the driver
    // owns the actual target sim boot (the LAUNCH seam).
    snd_push(s, "menuForward"); // :132
    s->tssStage = s->tssCursor;
    s->targetMode = true;
    s->launched = true;
    ev_trans(s, FOH_TSS, FOH_TMATCH, "launch");
    ev_launch(s);
    return;
  }
  // grid cursor (rewrite delta, foh.h): authored slots 0..9 at
  // col = cursor/5 (upstream floor(j/5)), row = cursor%5; the addcode
  // slot (10) below — D from a bottom row enters it, U returns to the
  // left column's bottom tile (the SSS RANDOM-slot pattern); CLAMP.
  const bool uE = in->up && !pv->up;
  const bool dE = in->down && !pv->down;
  const bool lE = in->left && !pv->left;
  const bool rE = in->right && !pv->right;
  const int before = s->tssCursor;
  if (s->tssCursor == 10) {
    if (uE) s->tssCursor = 4;
  } else {
    if (lE && s->tssCursor >= 5) s->tssCursor -= 5;
    if (rE && s->tssCursor < 5) s->tssCursor += 5;
    if (uE && s->tssCursor % 5 > 0) s->tssCursor -= 1;
    if (dE) {
      if (s->tssCursor % 5 < 4) s->tssCursor += 1;
      else s->tssCursor = 10;
    }
  }
  if (s->tssCursor != before) {
    snd_push(s, "menuSelect"); // targetselect.js:51 change class
  }
}

static void step_opt_gameplay(FohState *s, const PlatformInput *in,
                              const PlatformInput *pv) {
  const bool aE = in->a && !pv->a;
  const bool bE = in->b && !pv->b;
  if (bE) {
    // gameplaymenu.js:25-36 (cookie save = task-13 persistence,
    // registered); cursor returns to the Gameplay entry.
    s->menuSelected = 1;
    snd_push(s, "menuBack"); // gameplaymenu.js:26
    ev_trans(s, FOH_OPT_GAMEPLAY, FOH_MENU_OPTIONS, "b");
    return;
  }
  const bool uE = in->up && !pv->up;
  const bool dE = in->down && !pv->down;
  const bool lE = in->left && !pv->left;
  const bool rE = in->right && !pv->right;
  {
    const int rowBefore = s->optRow, colBefore = s->optCol;
    if (uE) s->optRow = clampi(s->optRow - 1, 0, 2);
    if (dE) s->optRow = clampi(s->optRow + 1, 0, 2);
    if (s->optRow == 2) {
      if (lE) s->optCol = clampi(s->optCol - 1, 0, 3);
      if (rE) s->optCol = clampi(s->optCol + 1, 0, 3);
    }
    if (s->optRow != rowBefore || s->optCol != colBefore) {
      snd_push(s, "menuSelect"); // nav class (menu.js:236)
    }
  }
  if (aE) {
    if (s->optRow == 0) {
      s->turbo ^= 1; // gameplaymenu.js:40 (turbo ^= true)
      snd_push(s, "menuSelect"); // gameplaymenu.js:38
      ev_sel(s, "turbo", s->turbo);
    } else if (s->optRow == 1) {
      s->lCancelType = (s->lCancelType + 1) % 3; // 0->1->2->0 (:44-48)
      snd_push(s, "menuSelect"); // gameplaymenu.js:38
      ev_sel(s, "lcancel", s->lCancelType);
    } else {
      s->tapJumpOff[s->optCol] ^= 1; // tapJumpOffp{1..4} (:53-58)
      snd_push(s, "menuSelect"); // gameplaymenu.js:152
      // field token carries the 1-based port like the upstream key
      ev_sel(s,
             s->optCol == 0   ? "tapjump1"
             : s->optCol == 1 ? "tapjump2"
             : s->optCol == 2 ? "tapjump3"
                              : "tapjump4",
             s->tapJumpOff[s->optCol]);
    }
  }
}

void foh_tick(FohState *s, const PlatformInput *in) {
  s->nev = 0;
  s->nsnd = 0;
  const PlatformInput pv = s->prev;
  switch (s->screen) {
    case FOH_STARTUP:
      // menus/startup.js: startUpTimer==370 -> changeGamemode(0)
      s->startupTimer++;
      if (s->startupTimer == 370) ev_trans(s, FOH_STARTUP, FOH_TITLE, "timer");
      break;
    case FOH_TITLE:
      // findPlayers (main.js:385): Start joins P1 and enters the menu
      // (sounds.menuForward, main.js:388; the device app also starts
      // the MENU MUSIC on this transition — main.js:390 playMenuLoop).
      if (in->start && !pv.start) {
        snd_push(s, "menuForward");
        ev_trans(s, FOH_TITLE, FOH_MENU_TOP, "start");
      }
      break;
    case FOH_MENU_TOP:
    case FOH_MENU_OPTIONS:
    case FOH_MENU_BATTLE:
    case FOH_MENU_CONTROLS:
      step_menu(s, in, &pv);
      break;
    case FOH_CSS:
      step_css(s, in, &pv);
      break;
    case FOH_SSS:
      step_sss(s, in, &pv);
      break;
    case FOH_OPT_GAMEPLAY:
      step_opt_gameplay(s, in, &pv);
      break;
    case FOH_TSS:
      step_tss(s, in, &pv);
      break;
    case FOH_MATCH:
    case FOH_TMATCH:
      // terminal for the FOH machine; the driver owns the sim from here
      break;
    default: gfx_fatal("foh: tick on an invalid screen");
  }
  s->prev = *in;
  // readyToFight's DRAW-PASS recompute (css.js:1167-1181). Upstream computes
  // it inside drawCSS, and drawCSS runs from a SEPARATE rAF loop that
  // dispatches on the CURRENT gameMode (main.js:1153-1183 at the pin) — so it
  // belongs to the screen the tick ENDS on, not the one it started on:
  //   * CSS -> SSS (launch): the mode is 6 by draw time, drawCSS does not run,
  //     and readyToFight keeps its stale value through the whole SSS.
  //   * SSS -> CSS (B back, stageselect.js:79): the mode is 2 by draw time, so
  //     drawCSS DOES run on the return frame and readiness is recomputed
  //     BEFORE the next control pass. A token still held across the round trip
  //     therefore un-readies the screen, and START cannot relaunch.
  //   * CSS -> menu (B-hold): the mode is 1, so no recompute at all.
  // Keeping this inside step_css got the first and third right and the second
  // WRONG (review round 4/5); gating on the FINAL screen is the whole rule.
  if (s->screen == FOH_CSS) s->cssReady = css_ready(s);
  // LOOK plane only (A1 restyle Phase 0) — advanced after navigation has
  // settled so the hue chases the NEW selection. Reads/writes nothing the
  // flow graph, the event list or the launch record can observe.
  foh_anim_tick(s);
}
