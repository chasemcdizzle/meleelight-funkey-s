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
  s->p2Type = 0;      // P2 active human (the M3 live-session model)
  s->difficulty = 3;  // cpuDifficulty default (main.js:109)
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
// REGISTERED REWRITE DELTA (AGENT-LOG iter 93): the CSS announcer
// names (token DROP, css.js:233+) are excluded — the rewritten
// row-cursor CSS has no drop gesture.
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

static void step_css(FohState *s, const PlatformInput *in,
                     const PlatformInput *pv) {
  // B held 30 consecutive frames -> back to the menu (css.js:186-191;
  // menuMode is untouched upstream, so we return to menu-battle).
  if (in->b) {
    s->bHold++;
    if (s->bHold == 30) {
      s->bHold = 0;
      s->menuSelected = 0; // LOCALVS — cursor where VS entry left it
      snd_push(s, "menuBack"); // css.js:189
      ev_trans(s, FOH_CSS, FOH_MENU_BATTLE, "bhold");
      return;
    }
  } else {
    s->bHold = 0;
  }
  // START while readyToFight -> stage select (css.js:446-451).
  // readyToFight is structurally true here: both ports are active by
  // construction (P2 type domain {0,1}; css.js:1167-1181).
  if (in->start && !pv->start) {
    snd_push(s, "menuForward"); // css.js:448
    ev_trans(s, FOH_CSS, FOH_SSS, "start");
    return;
  }
  const bool aE = in->a && !pv->a;
  const bool uE = in->up && !pv->up;
  const bool dE = in->down && !pv->down;
  const bool lE = in->left && !pv->left;
  const bool rE = in->right && !pv->right;
  if (uE && s->cssRow > 0) {
    s->cssRow--;
    snd_push(s, "menuSelect"); // nav class (menu.js:236)
  }
  if (dE && s->cssRow < 3) {
    s->cssRow++;
    snd_push(s, "menuSelect");
  }
  if (lE || rE) {
    const int d = rE ? 1 : -1;
    if (s->cssRow == 0) {
      const int v = clampi(s->p1Char + d, 0, 4);
      if (v != s->p1Char) {
        s->p1Char = v;
        snd_push(s, "menuSelect"); // css.js:226 hover-change class
        ev_sel(s, "p1char", v);
      }
    } else if (s->cssRow == 1) {
      const int v = clampi(s->p2Char + d, 0, 4);
      if (v != s->p2Char) {
        s->p2Char = v;
        snd_push(s, "menuSelect"); // css.js:226 hover-change class
        ev_sel(s, "p2char", v);
      }
    } else if (s->cssRow == 3 && s->p2Type == 1) {
      // slider domain 1..4 (css.js:316-329); inactive while P2 human.
      const int v = clampi(s->difficulty + d, 1, 4);
      if (v != s->difficulty) {
        s->difficulty = v;
        snd_push(s, "menuSelect");
        ev_sel(s, "difficulty", v);
      }
    }
  }
  if (aE && s->cssRow == 2) {
    // togglePort narrowed to HMN <-> CPU (header note).
    s->p2Type ^= 1;
    snd_push(s, "menuSelect"); // css.js:175 class
    ev_sel(s, "p2type", s->p2Type);
  }
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
  // LOOK plane only (A1 restyle Phase 0) — advanced after navigation has
  // settled so the hue chases the NEW selection. Reads/writes nothing the
  // flow graph, the event list or the launch record can observe.
  foh_anim_tick(s);
}
