// port/foh/foh.c — the FOH screen machine (fix_plan §M4 task 9). Flow
// graph + selection semantics per the foh.h header table (every edge
// cited there from the upstream primary source); rewritten navigation
// (d-pad edges, row cursors) per the pre-registered deltas (AGENT-LOG
// iter 88). No RNG, no wall clock, no I/O — a pure (state, input) ->
// state step, so flow traces and screenshots are byte-stable by
// construction.
#include "foh.h"

#include "../gfx/ctl_style.h" // C30(c): the Controls screen's two cells

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
    case FOH_OPT_AUDIO: return "options-audio";
    case FOH_CTRL_PAD: return "controls-controller";
    case FOH_CTRL_KEY: return "controls-keyboard";
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
  // gameSettings defaults (settings.js:44-56): all zero — memset did it,
  // EXCEPT phantomThreshold, whose authored default is 0.01 (settings.js:50)
  // and which is on the checksum surface. Zeroing it is the exact qjs
  // getCookie defect (CLAUDE.md M0 task 6), so it is written explicitly.
  s->phantomThreshold = 0.01;
  // masterVolume = [0.5, 0.3] (audiomenu.js:13) — sounds, music.
  s->masterVolume[0] = 0.5;
  s->masterVolume[1] = 0.3;
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
#if FOH_NETPLAY
          ev_trans(s, sc, FOH_MENU_BATTLE, "a");
#else
          // C5 (owner ruling; foh.h FOH_NETPLAY): the battle page holds
          // nothing but Local VS and three netplay rows, so `VS. Melee`
          // runs the Local VS action itself — menu.js:105's
          // changeGamemode(2) + positionPlayersInCSS, the exact arm the
          // hidden page would have dispatched. menuSelected stays 0, so the
          // CSS's B-hold lands back on `VS. Melee`.
          ev_trans(s, sc, FOH_CSS, "a");
#endif
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
          // menu.js:67/:233/:236 — the LOCAL boolean `menuMove` (it shadows
          // the function name) is set by this arm, and :236 plays a SECOND
          // sound in the SAME tick. Only menuMODE changes set it: the
          // changeGamemode leaves (and VSMODE->MPMENU, :73-75) do NOT.
          snd_push(s, "menuSelect"); // menu.js:236 (menuMove at :97)
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
          // audioMenuSelected (audiomenu.js:15) is MODULE state: it is not
          // reset on entry, so a second visit opens on the row you left.
          // FohState has exactly that lifetime — nothing to do here.
          snd_push(s, "menuForward"); // menu.js:70
          ev_trans(s, sc, FOH_OPT_AUDIO, "a"); // changeGamemode(10), :130
        } else if (s->menuSelected == 1) {
          // menuIndex (gameplaymenu.js:10) is MODULE state too — upstream
          // never resets it on entry either (measured), so the cursor is
          // where you left it. The old unconditional reset here was an
          // unregistered deviation; FohState already has module lifetime.
          snd_push(s, "menuForward"); // menu.js:70
          ev_trans(s, sc, FOH_OPT_GAMEPLAY, "a"); // menu.js:135
        } else if (s->menuSelected == 2) {
          s->menuSelected = 0;
          snd_push(s, "menuForward"); // menu.js:70
          // menu.js:67/:233/:236 — the LOCAL boolean `menuMove` (it shadows
          // the function name) is set by this arm, and :236 plays a SECOND
          // sound in the SAME tick. Only menuMODE changes set it: the
          // changeGamemode leaves (and VSMODE->MPMENU, :73-75) do NOT.
          snd_push(s, "menuSelect"); // menu.js:236 (menuMove at :141)
          ev_trans(s, sc, FOH_MENU_CONTROLS, "a"); // menu.js:138-141
        } else {
          snd_push(s, "deny");
          ev_refused(s, "credits"); // conventions scope exclusion
        }
        break;
      case FOH_MENU_CONTROLS:
        // ["Controller","Keyboard"] (menu.js:22-23). Upstream row 1 is
        // reached by `else`, so ANY non-zero index lands there
        // (menu.js:159) — mirrored by the ternary below.
        snd_push(s, "menuForward"); // menu.js:70 (before any dispatch)
        ev_trans(s, sc, s->menuSelected == 0 ? FOH_CTRL_PAD : FOH_CTRL_KEY,
                 "a"); // :155-157 changeGamemode(14) / :159-161 (12)
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
      // menu.js:169/:174/:179 set the local `menuMove` boolean, so :236
      // plays a SECOND sound in the same tick (all three B backs).
      snd_push(s, "menuSelect"); // menu.js:236
      ev_trans(s, sc, FOH_MENU_OPTIONS, "b");
    } else if (sc == FOH_MENU_OPTIONS) {
      s->menuSelected = 3; // OPTIONS
      snd_push(s, "menuBack"); // menu.js:170-190
      snd_push(s, "menuSelect"); // menu.js:236 (menuMove at :174)
      ev_trans(s, sc, FOH_MENU_TOP, "b");
    } else if (sc == FOH_MENU_BATTLE) {
      s->menuSelected = 0; // VSMODE
      snd_push(s, "menuBack"); // menu.js:170-190
      snd_push(s, "menuSelect"); // menu.js:236 (menuMove at :179)
      ev_trans(s, sc, FOH_MENU_TOP, "b");
    }
    // top level: B does nothing (no upstream arm)
    return;
  }
  // cursor with wrap (menu.js:192-242 wraps via menuCount)
  // upstream's arms are ONE else-if chain (menu.js:69,164,192,205 —
  // A->B->up->down; the A arm at :69 precedes the B arm at :164),
  // so a simultaneous up+down edge runs UP ONLY. Independent ifs cancelled
  // the cursor and emitted TWO menuSelects in one tick.
  if (uE) {
    s->menuSelected = (s->menuSelected + count - 1) % count;
    snd_push(s, "menuSelect"); // menu.js:236
  } else if (dE) {
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
  if (s->cssTokenRest[k] == 2) {
    // endGame's snap slot (main.js:1381-1384 -> css.js:154-156:
    // `tokenPos[index] = charIconPos[index]`).
    //
    // DEVIATION D21 (A29, owner-reported P0). Upstream indexes charIconPos —
    // a per-CHARACTER array — with the PORT number, so upstream's port k
    // snaps to CHARACTER k's icon whatever that port picked: after any match
    // the tokens sit on marth and puff forever. This port used to carry that
    // verbatim and the owner filed it as lost selection, because HERE the
    // token is the ONLY roster-level indicator of who is picked — render_css
    // draws no selected-cell highlight, only a hover one (foh_render.c's
    // css_cell call), where upstream has four large panels and 270 px of
    // margin to disambiguate. So the same quirk that is cosmetic upstream
    // reads as "my pick was discarded" on 240x240.
    //
    // It is also demonstrably an upstream TYPO rather than a designed
    // behaviour: setChosenChar calls `setTokenPosSnapToChar(index,
    // charSelected)` with two arguments (css.js:146) while the callee
    // declares one and drops it (css.js:154). We honour the argument the
    // caller passes. Nothing on the LAUNCH plane moves — p1Char/p2Char and
    // cssChar are untouched by the snap either way; this changes where the
    // token is DRAWN and hit-tested, and that is the whole deviation.
    base = (double)(foh_css_cell_x(c) + FOH_CSS_TOKEN_DX); // D21: `c`, not `k`
  } else if (s->cssTokenRest[k] == 0) { // A-drop (css.js:288 family)
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
#if FOH_NETPLAY
  ev_trans(s, FOH_CSS, FOH_MENU_BATTLE, "bhold");
#else
  // C5: the battle page is hidden, so `menuMode untouched` resolves to the
  // page the player actually came from — menu-top, cursor on VS. Melee
  // (row 0, the same index LOCALVS has on the hidden page).
  ev_trans(s, FOH_CSS, FOH_MENU_TOP, "bhold");
#endif
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
    // The launch KIND, stated by every launch arm rather than left over from
    // the last one (review-mexit-r2 High). Harmless while a process could
    // only ever launch once; with the A19 in-process return it is a real
    // defect — target -> TSS -> B -> menu -> VS -> SSS -> A left targetMode
    // true, so the "VS" launch dispatched through the TARGET bridge.
    s->targetMode = false;
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

// menuVOptions / menuHOptions (gameplaymenu.js:11-12). BOTH are MAX
// INDICES, not counts: five rows, and only the last one has columns.
#define FOH_OPT_ROWMAX 4
static const int kOptColMax[FOH_OPT_ROWMAX + 1] = {0, 0, 0, 0, 3};

static void step_opt_gameplay(FohState *s, const PlatformInput *in,
                              const PlatformInput *pv) {
  // gameplayMenuControls (gameplaymenu.js:23-164) is ONE else-if chain —
  // B -> A -> up -> down -> right -> left -> neutral — so exactly one arm
  // runs per frame. That priority is kept here over d-pad EDGES (the
  // 10-frame autorepeat is the pre-registered rewrite delta, iter 88).
  // DEVIATION D9 (MENU-SPEC §3.4): upstream's diagonal guards are
  // malformed (`!(Math.abs(lsX >= 0.7))` applies Math.abs to a BOOLEAN, so
  // up-left is accepted and up-right is rejected) and its left arm omits
  // stickHoldEach, repeating at 60 Hz instead of 6. Neither is reproduced;
  // the chain's own order already makes vertical win a diagonal.
  const bool bE = in->b && !pv->b;
  const bool aE = in->a && !pv->a;
  const bool uE = in->up && !pv->up;
  const bool dE = in->down && !pv->down;
  const bool lE = in->left && !pv->left;
  const bool rE = in->right && !pv->right;
  if (bE) {
    // gameplaymenu.js:25-36: menuBack, then setCookie over EVERY
    // gameSettings key (the persist chokepoint's save, foh_app/foh_dev),
    // then changeGamemode(1) with menuMode/menuSelected untouched — so the
    // cursor is still on the Gameplay row. The meHost gate (:32's blocking
    // alert on a joined client) is netplay-only and out of scope.
    s->menuSelected = 1;
    snd_push(s, "menuBack"); // gameplaymenu.js:26
    ev_trans(s, FOH_OPT_GAMEPLAY, FOH_MENU_OPTIONS, "b");
    return;
  }
  if (aE) {
    // A is the ONLY value-changing input on this screen (:37-59), and it
    // plays menuSelect unconditionally at :38, before the switch.
    snd_push(s, "menuSelect");
    switch (s->optRow) {
      case 0:
        s->turbo ^= 1; // :41 `turbo ^= true` (coerces, stays 0/1)
        ev_sel(s, "turbo", s->turbo);
        break;
      case 1:
        s->lCancelType++; // :44-47 ++ then wrap >2 -> 0
        if (s->lCancelType > 2) s->lCancelType = 0;
        ev_sel(s, "lcancel", s->lCancelType);
        break;
      case 2:
        s->flashOnLCancel ^= 1; // :50
        ev_sel(s, "flashlcancel", s->flashOnLCancel);
        break;
      case 3:
        s->everyCharWallJump ^= 1; // :53 — the measured-dead toggle
        ev_sel(s, "walljump", s->everyCharWallJump);
        break;
      default:
        // :56 gameSettings["tapJumpOffp" + (menuIndex[1]+1)] ^= true;
        // the field token carries the same 1-based port the key does.
        s->tapJumpOff[s->optCol] ^= 1;
        ev_sel(s,
               s->optCol == 0   ? "tapjump1"
               : s->optCol == 1 ? "tapjump2"
               : s->optCol == 2 ? "tapjump3"
                                : "tapjump4",
               s->tapJumpOff[s->optCol]);
        break;
    }
    return;
  }
  bool moved = false;
  if (uE || dE) {
    s->optRow += uE ? -1 : 1;
    // :62-65 clamps the column against the NEW row BEFORE the wrap. When
    // the row has gone out of range upstream reads menuHOptions[-1] /
    // [5] == undefined and `col > undefined` is FALSE, so no clamp runs —
    // hence the range guard here rather than a saturating index.
    if (s->optRow >= 0 && s->optRow <= FOH_OPT_ROWMAX &&
        s->optCol > kOptColMax[s->optRow]) {
      s->optCol = kOptColMax[s->optRow];
    }
    moved = true;
  } else if (rE) {
    s->optCol++; // :101 (upstream's clamp at :102-104 is commented out)
    moved = true;
  } else if (lE) {
    s->optCol--; // :117
    moved = true;
  }
  if (moved) {
    // :150-163 — one menuSelect per accepted move, then the wraps. The
    // column wraps against the POST-wrap row, and on a single-column row
    // any left/right press wraps straight back to 0: an audible no-op,
    // which is exactly what upstream does.
    snd_push(s, "menuSelect"); // :152
    if (s->optRow < 0) s->optRow = FOH_OPT_ROWMAX;
    else if (s->optRow > FOH_OPT_ROWMAX) s->optRow = 0;
    if (s->optCol > kOptColMax[s->optRow]) s->optCol = 0;
    else if (s->optCol < 0) s->optCol = kOptColMax[s->optRow];
  }
}

// --- AUDIO OPTIONS (upstream menus/audiomenu.js; MENU-SPEC §4) --------------
// audioMenuControls (:16-121) is the same else-if shape as gameplaymenu with
// one loud difference: there is NO A HANDLER AT ALL (measured — `.a` does not
// appear in the file), so A does nothing here. Chain: B -> up -> down ->
// right -> left -> neutral, i.e. lsY is tested before lsX, so a diagonal
// moves the cursor and never touches a volume.
static void step_opt_audio(FohState *s, const PlatformInput *in,
                           const PlatformInput *pv) {
  const bool bE = in->b && !pv->b;
  const bool uE = in->up && !pv->up;
  const bool dE = in->down && !pv->down;
  const bool lE = in->left && !pv->left;
  const bool rE = in->right && !pv->right;
  if (bE) {
    // :20-26 — menuBack, setCookie("soundsLevel"/"musicLevel", ...) and
    // changeGamemode(1) with menuMode/menuSelected untouched, so the cursor
    // is still on the Audio row. Unlike gameplaymenu there is NO meHost
    // gate: audio always saves.
    s->menuSelected = 0;
    snd_push(s, "menuBack"); // :22
    ev_trans(s, FOH_OPT_AUDIO, FOH_MENU_OPTIONS, "b");
    return;
  }
  if (uE || dE) {
    s->audioRow += uE ? -1 : 1; // :30 / :44
    snd_push(s, "menuSelect");  // :95
    if (s->audioRow == -1) s->audioRow = 1; // :96-99
    else if (s->audioRow == 2) s->audioRow = 0;
    return;
  }
  if (rE || lE) {
    // WIRED 2026-07-29 (owner ruling: "yep wire"). The level below is
    // edited, clamped, persisted AND converted for the live mixer by
    // snd_bus_set (port/gfx/snd_mixer.h's AUDIO BUS note) — this machine's
    // stand-in for audiomenu.js:114-120's changeVolume call. The FOH itself
    // stays I/O-free: it owns the VALUE, the app owns the push.
    // The DEVICE app's call site (foh_dev.c) is a pending cross-lane patch,
    // not landed code — MENU-SPEC §4 and the work order say so plainly.
    // Navigation here is rising-edge only (DEVIATION D16, §5.5 row 6):
    // upstream repeats a held direction 1-then-every-10 frames, we step once.
    // :103/:109 — a fixed +/-0.1 step with NO rounding, so the float dust
    // (0.7999999999999999) accumulates exactly as it does upstream and is
    // what gets persisted. menuSelect plays on EVERY accepted step,
    // including one that the clamp turns into a no-op (:102/:108 precede
    // the clamps) — so a rail-end press still clicks.
    const int k = s->audioRow;
    const double before = s->masterVolume[k];
    snd_push(s, "menuSelect");
    s->masterVolume[k] += rE ? 0.1 : -0.1;
    if (s->masterVolume[k] > 1.0) s->masterVolume[k] = 1.0; // :104-106
    if (s->masterVolume[k] < 0.0) s->masterVolume[k] = 0.0; // :110-112
    // The engine push (:114-120, the global changeVolume installed by
    // sfx.js:609) has no counterpart inside this machine: the FOH is
    // I/O-free by construction, so the value is state the app reads. Its
    // consumer is the app's bus push -> snd_bus_set, which
    // scales snd_mixer.h's two accumulate sites by masterVolume/default
    // (the RATIO — the packed gains already carry the 0.5/0.3 defaults, so
    // pushing the raw level would apply the default twice).
    if (s->masterVolume[k] != before) {
      // Traced in TENTHS: the structural plane carries an integer, the
      // machine keeps the raw double (dust included).
      ev_sel(s, k == 0 ? "soundsvol" : "musicvol",
             (int)(s->masterVolume[k] * 10.0 + 0.5));
    }
    return;
  }
}

// --- CONTROLS DESTINATIONS (MENU-SPEC §9) -----------------------------------
// Page 3's two rows finally go somewhere. Neither destination is a port of
// its upstream module, and both say so on screen:
//
//   Controller (gameMode 14, controllermenu.js + gamepadCalibration.js) is
//   MOUSE-ONLY upstream — 14 clickable regions and an SVG diagram, no
//   stick/keyboard arm anywhere (MENU-SPEC §9.2) — and it hard-requires
//   navigator.getGamepads. There is no mouse, no gamepad API and no pad on
//   a FunKey-S, so every calibration primitive is dead by construction. What
//   upstream ITSELF shows when no pad answers is `Error: no controller
//   detected` (gamepadCalibration.js:71), and that is what we show: its own
//   literal string, in its own condition. The one deviation is the way out —
//   upstream forgets to reschedule preCalibrationLoop there, so its menu
//   becomes permanently unresponsive with no route back but a page reload
//   (:71, measured; its only exit is the mouse-only Quit arm at
//   gamepadCalibration.js:165-169). B returns here. Reproducing a soft-lock
//   is not faithfulness. This added exit is DEVIATION D15 (MENU-SPEC §9.2 /
//   §12.1) — NOT the D9 class, which covers only declining to reproduce
//   gameplay-menu input bugs.
//
//   Keyboard (gameMode 12, keyboardmenu.js) is genuinely stick-navigated
//   upstream, and DEVIATION D13 already rules that the port rebinds the 12
//   PHYSICAL buttons rather than 56 keyboard slots. The rebinder itself
//   (listening mode, hold-A clear, protected primaries) is the largest and
//   least load-bearing item in the whole spec and is NOT in this arc — so
//   this screen is the READ-ONLY half of it: the frozen, Chase-ratified S1
//   mapping (port/foh/keymap-frozen.txt) shown as what it is. Registered as
//   the remaining half, never dressed up as the whole.
// C30(c) (driver, 2026-07-29): the control-style and Mod-shoulder cells the
// controls lane shipped in port/gfx/ctl_style.h had NO UI AT ALL — three
// styles and a shoulder swap that no user could ever reach. They live on the
// Controls > Keyboard screen because that is the screen that already answers
// "what do my buttons do"; the Controller row stays upstream's verbatim
// no-controller error (MENU-SPEC §9.2) and gains nothing.
//
// DEVIATION, and it is a knowing one: upstream's keyboardmenu.js is a 56-item
// REBINDER (D13) that we did not build, so this screen is already ours rather
// than a port. Two settable rows on it is a rewrite of a rewritten screen, not
// a drift from a faithful one. The rows follow the audio screen's idiom
// exactly (up/down picks the row, left/right cycles the value, every accepted
// step clicks) so the whole FOH stays one interaction model.
//
// The VALUES are written straight through to ctl_style.c's process cells,
// not mirrored into FohState: they are read by the sim-side input path in a
// different TU, and a second copy here would be a live desync (the exact
// reason ctl_style.c is a TU and not a header — ctl_style.c's own note).
static void step_ctrl(FohState *s, const PlatformInput *in,
                      const PlatformInput *pv) {
  if (s->screen == FOH_CTRL_KEY && !(in->b && !pv->b)) {
    const bool uE = in->up && !pv->up;
    const bool dE = in->down && !pv->down;
    const bool lE = in->left && !pv->left;
    const bool rE = in->right && !pv->right;
    // One else-if chain, up before down, exactly like every other screen
    // (menu.js:164-242's shape): a simultaneous up+down runs UP ONLY.
    if (uE || dE) {
      (void)uE; // two rows only, so up and down land on the same other row
      s->ctlRow ^= 1;
      snd_push(s, "menuSelect");
      return;
    }
    if (rE || lE) {
      if (s->ctlRow == 0) {
        // cycle the three styles, wrapping (CTL_STYLE_COUNT is the domain;
        // the enum VALUES are a frozen wire format — never renumber them,
        // FohPersist.ctlStyle stores them verbatim).
        const int n = (int)CTL_STYLE_COUNT;
        int v = (int)ctl_style_get() + (rE ? 1 : n - 1);
        ctl_style_set(((v % n) + n) % n);
      } else {
        // the Mod shoulder is a two-state swap, so either direction flips it
        ctl_mod_on_r_set(!ctl_mod_on_r_get());
      }
      snd_push(s, "menuSelect");
      return;
    }
  }
  if (in->b && !pv->b) {
    // menuSelected is untouched on the way in and out, so B lands on the
    // row that opened the screen (upstream's page-3 rows are unchanged by
    // either destination).
    snd_push(s, "menuBack");
    ev_trans(s, s->screen, FOH_MENU_CONTROLS, "b");
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
    case FOH_OPT_AUDIO:
      step_opt_audio(s, in, &pv);
      break;
    case FOH_CTRL_PAD:
    case FOH_CTRL_KEY:
      step_ctrl(s, in, &pv);
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
