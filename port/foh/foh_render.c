// port/foh/foh_render.c — FOH screen rendering (fix_plan §M4 task 9).
// REWRITTEN look at 240x240 over the raster prims (never a DOM port);
// menus are NOT checksummed — visual authority is Chase's acceptance
// playthrough (task 10 owns the device look pass). Deterministic by
// construction: pure function of FohState, no RNG, no clock — the
// check's shot byte-stability x2 depends on it.
#include "foh.h"

#include <stdio.h> // snprintf (the task-13 records line)

// Labels: faithful strings from the upstream tables (cited), uppercased
// for the 5x7 font. These are UI text of a rewritten non-checksummed
// surface, not engine values (HARD RULE 5 concerns data planes).
static const char *kMenuTitle[4] = {
    // menuTitle (menu.js:32)
    "MAIN MENU", "OPTIONS", "BATTLE MODE", "CONTROLS"};
static const char *kMenuText[4][4] = {
    // menuText (menu.js:19-24)
    {"VS. MELEE", "TARGET TEST", "TARGET BUILDER", "OPTIONS"},
    {"AUDIO", "GAMEPLAY", "KEYBOARD CONTROLS", "CREDITS"},
    {"LOCAL VS", "SPECTATE", "P2P", "SERVER"},
    {"CONTROLLER", "KEYBOARD"},
};
static const char *kCharNames[5] = {
    // characters.js:2-8 order == the oracle char ids
    "MARTH", "JIGGLYPUFF", "FOX", "FALCO", "CAPTAIN FALCON"};
static const char *kStageNames[7] = {
    // stageselect.js:13-29 order == the oracle stage ids; slot 6 = the
    // visible-but-refusing RANDOM slot (registered exclusion, foh.h)
    "BATTLEFIELD",  "YOSHI'S STORY",     "POKEMON STADIUM",
    "DREAMLAND",    "FINAL DESTINATION", "FOUNTAIN OF DREAMS",
    "RANDOM"};
static const char *kLCancelNames[3] = {
    // settings.js:46 comment: 0 normal | 1 auto | 2 smash64
    "NORMAL", "AUTO", "SMASH64"};

static const RastCol kBg = {12, 12, 28, 256};
static const RastCol kPanel = {30, 30, 60, 256};
static const RastCol kText = {220, 220, 230, 256};
static const RastCol kDim = {120, 120, 140, 256};
static const RastCol kAccent = {255, 200, 60, 256};
static const RastCol kCursor = {90, 160, 255, 256};

static void fill_rect(Raster *rz, int x, int y, int w, int h, RastCol c) {
  for (int yy = y; yy < y + h; yy++) {
    for (int xx = x; xx < x + w; xx++) {
      rast_blend_px(rz, xx, yy, c, c.a256);
    }
  }
}

static void text_center(Raster *rz, int y, int scale, const char *s,
                        RastCol c) {
  const int w = foh_text_width(s, scale);
  foh_text(rz, (RAST_W - w) / 2, y, scale, s, c);
}

static void header(Raster *rz, const char *title) {
  fill_rect(rz, 0, 0, RAST_W, 24, kPanel);
  text_center(rz, 6, 2, title, kAccent);
}

static void render_startup(const FohState *s, Raster *rz) {
  text_center(rz, 90, 3, "MELEELIGHT", kText);
  text_center(rz, 130, 1, "FUNKEY-S PORT", kDim);
  // deterministic progress bar over the 370-frame startup window
  const int w = (RAST_W - 40) * s->startupTimer / 370;
  fill_rect(rz, 20, 170, RAST_W - 40, 6, kPanel);
  fill_rect(rz, 20, 170, w, 6, kAccent);
}

static void render_title(Raster *rz) {
  text_center(rz, 80, 3, "MELEELIGHT", kText);
  text_center(rz, 150, 1, "PRESS START", kAccent);
}

static void render_menu(const FohState *s, Raster *rz) {
  // screen -> upstream menuMode table index (menu.js:44-47)
  int mm;
  switch (s->screen) {
    case FOH_MENU_TOP: mm = 0; break;
    case FOH_MENU_OPTIONS: mm = 1; break;
    case FOH_MENU_BATTLE: mm = 2; break;
    case FOH_MENU_CONTROLS: mm = 3; break;
    default: gfx_fatal("foh_render: menu render on a non-menu screen");
  }
  const int count = mm == 3 ? 2 : 4;
  header(rz, kMenuTitle[mm]);
  for (int k = 0; k < count; k++) {
    const int y = 60 + 30 * k;
    if (k == s->menuSelected) {
      fill_rect(rz, 16, y - 6, RAST_W - 32, 22, kPanel);
      foh_text(rz, 22, y - 2, 1, ">", kCursor);
    }
    foh_text(rz, 34, y - 2, 1, kMenuText[mm][k],
             k == s->menuSelected ? kText : kDim);
  }
}

static void row_label(Raster *rz, int y, int row, int curRow,
                      const char *label) {
  if (row == curRow) {
    fill_rect(rz, 8, y - 4, RAST_W - 16, 18, kPanel);
    foh_text(rz, 12, y, 1, ">", kCursor);
  }
  foh_text(rz, 24, y, 1, label, row == curRow ? kText : kDim);
}

static void render_css(const FohState *s, Raster *rz) {
  header(rz, "CHARACTER SELECT");
  const int ys[4] = {50, 90, 130, 160};
  row_label(rz, ys[0], 0, s->cssRow, "P1");
  foh_text(rz, 60, ys[0], 1, kCharNames[s->p1Char], kAccent);
  row_label(rz, ys[1], 1, s->cssRow, "P2");
  foh_text(rz, 60, ys[1], 1, kCharNames[s->p2Char], kAccent);
  row_label(rz, ys[2], 2, s->cssRow, "P2 TYPE");
  foh_text(rz, 100, ys[2], 1, s->p2Type == 0 ? "HMN" : "CPU", kAccent);
  row_label(rz, ys[3], 3, s->cssRow, "CPU LEVEL");
  // slider look: 4 ticks (the upstream slider's 1..4 domain), current
  // level filled; dimmed entirely while P2 is human.
  for (int k = 0; k < 4; k++) {
    const RastCol c = (s->p2Type == 1 && k < s->difficulty) ? kAccent : kPanel;
    fill_rect(rz, 100 + k * 18, ys[3], 12, 8, c);
  }
  {
    const char lvl[2] = {(char)('0' + s->difficulty), 0};
    foh_text(rz, 180, ys[3], 1, lvl, s->p2Type == 1 ? kText : kDim);
  }
  text_center(rz, 200, 1, "START: STAGE SELECT", kDim);
  text_center(rz, 215, 1, "HOLD B: BACK", kDim);
}

static void render_sss(const FohState *s, Raster *rz) {
  header(rz, "STAGE SELECT");
  // 3x2 grid of stage tiles, ids 0..5
  for (int k = 0; k < 6; k++) {
    const int col = k % 3, row = k / 3;
    const int x = 10 + col * 75, y = 50 + row * 60;
    fill_rect(rz, x, y, 65, 44, kPanel);
    if (k == s->sssCursor) {
      // cursor frame (4 edges)
      fill_rect(rz, x - 2, y - 2, 69, 2, kCursor);
      fill_rect(rz, x - 2, y + 44, 69, 2, kCursor);
      fill_rect(rz, x - 2, y, 2, 44, kCursor);
      fill_rect(rz, x + 65, y, 2, 44, kCursor);
    }
    const char num[2] = {(char)('0' + k), 0};
    foh_text(rz, x + 4, y + 4, 1, num, kDim);
  }
  // The RANDOM slot (cursor 6): visible but REFUSING (registered
  // exclusion — upstream's arm draws from the seeded stream,
  // stageselect.js:80-84; foh.h header note). Rendered as a wide
  // dimmed tile below the grid.
  {
    const int x = 60, y = 172, w = 120, h = 16;
    fill_rect(rz, x, y, w, h, kPanel);
    if (s->sssCursor == 6) {
      fill_rect(rz, x - 2, y - 2, w + 4, 2, kCursor);
      fill_rect(rz, x - 2, y + h, w + 4, 2, kCursor);
      fill_rect(rz, x - 2, y, 2, h, kCursor);
      fill_rect(rz, x + w, y, 2, h, kCursor);
    }
    foh_text(rz, x + 6, y + 4, 1, "RANDOM", kDim);
  }
  text_center(rz, 194, 1, kStageNames[s->sssCursor], kAccent);
  text_center(rz, 208, 1, "A: FIGHT   B: BACK", kDim);
}

static void render_opt_gameplay(const FohState *s, Raster *rz) {
  header(rz, "GAMEPLAY");
  const int ys[3] = {60, 95, 130};
  row_label(rz, ys[0], 0, s->optRow, "TURBO");
  foh_text(rz, 120, ys[0], 1, s->turbo ? "ON" : "OFF", kAccent);
  row_label(rz, ys[1], 1, s->optRow, "L-CANCEL");
  foh_text(rz, 120, ys[1], 1, kLCancelNames[s->lCancelType], kAccent);
  row_label(rz, ys[2], 2, s->optRow, "TAP JUMP OFF");
  for (int k = 0; k < 4; k++) {
    const int x = 40 + k * 42;
    const int y = ys[2] + 18;
    if (s->optRow == 2 && s->optCol == k) {
      fill_rect(rz, x - 3, y - 3, 38, 16, kPanel);
    }
    const char pn[3] = {'P', (char)('1' + k), 0};
    foh_text(rz, x, y, 1, pn, kDim);
    foh_text(rz, x + 14, y, 1, s->tapJumpOff[k] ? "X" : "-",
             s->tapJumpOff[k] ? kAccent : kDim);
  }
  text_center(rz, 205, 1, "A: CHANGE   B: BACK", kDim);
}

static void render_match(Raster *rz) {
  // The FOH machine is terminal here; the driver owns the sim/renderer.
  text_center(rz, 110, 2, "LAUNCHING", kText);
}

// target-select (upstream drawTSS/drawTSSInit, stages/targetselect.js:
// 231-420, rewritten at 240x240 — foh.h rewrite deltas). Slots are the
// upstream 2-col x 5-row authored layout (col = floor(j/5), row = j%5)
// plus the refusing "+ ADD CODE" slot; the records line is the honest
// fresh-boot value (targetRecords ≡ -1 -> "--:--:--", targetplay.js:40 +
// targetselect.js:411-412; READ/persistence = task 13, medal/dev times
// deferred — foh.h note).
static void render_tss(const FohState *s, Raster *rz) {
  header(rz, "TARGET TEST");
  // char row (shoulder-driven; targetselect.js:60-74)
  foh_text(rz, 12, 32, 1, "L/R:", kDim);
  foh_text(rz, 44, 32, 1, kCharNames[s->p1Char], kAccent);
  // 2x5 grid of authored target stages (ids 0..9 == tstage ids)
  for (int k = 0; k < 10; k++) {
    const int col = k / 5, row = k % 5; // upstream floor(j/5) / j%5
    const int x = 16 + col * 108, y = 48 + row * 24;
    fill_rect(rz, x, y, 96, 18, kPanel);
    if (k == s->tssCursor) {
      fill_rect(rz, x - 2, y - 2, 100, 2, kCursor);
      fill_rect(rz, x - 2, y + 18, 100, 2, kCursor);
      fill_rect(rz, x - 2, y, 2, 18, kCursor);
      fill_rect(rz, x + 96, y, 2, 18, kCursor);
    }
    // "Target "+(i+1) (targetselect.js:93 label class)
    char label[10] = "TARGET ";
    if (k == 9) { label[7] = '1'; label[8] = '0'; label[9] = 0; }
    else { label[7] = (char)('1' + k); label[8] = 0; }
    foh_text(rz, x + 6, y + 5, 1, label,
             k == s->tssCursor ? kText : kDim);
  }
  // the refusing "+ Add Code" slot (builder plane; foh.h note)
  {
    const int x = 60, y = 172, w = 120, h = 14;
    fill_rect(rz, x, y, w, h, kPanel);
    if (s->tssCursor == 10) {
      fill_rect(rz, x - 2, y - 2, w + 4, 2, kCursor);
      fill_rect(rz, x - 2, y + h, w + 4, 2, kCursor);
      fill_rect(rz, x - 2, y, 2, h, kCursor);
      fill_rect(rz, x + w, y, 2, h, kCursor);
    }
    foh_text(rz, x + 6, y + 4, 1, "+ ADD CODE", kDim);
  }
  // records line (task 13 — the READ path through the persist plane):
  // upstream format targetselect.js:411-419 — -1 -> "--:--:--", else
  // "0"+floor(rec/60)+":"+((rec%60).toFixed(2), 5-char left-padded).
  // C form: integer centiseconds cs = (long)(rec*100 + 0.5) — no libc
  // float formatting on the device path (the iter-38/74 musl rounding
  // class; registered formatting delta, AGENT-LOG iter 100). The
  // addcode slot (cursor 10) keeps the dashes (foh.h note).
  {
    char line[40] = "PERSONAL BEST --:--:--";
    if (s->tssCursor <= 9) {
      const double rec = s->targetRecords[s->p1Char][s->tssCursor];
      if (rec != -1.0) {
        const long cs = (long)(rec * 100.0 + 0.5);
        snprintf(line, sizeof line, "PERSONAL BEST 0%ld:%02ld.%02ld",
                 cs / 6000, (cs % 6000) / 100, cs % 100);
      }
    }
    text_center(rz, 194, 1, line, kAccent);
  }
  text_center(rz, 208, 1, "A: GO   B: BACK", kDim);
}

static void render_tmatch(Raster *rz) {
  // Terminal like `match`; the driver owns the target sim/renderer.
  text_center(rz, 110, 2, "LAUNCHING", kText);
}

void foh_render(const FohState *s, Raster *rz) {
  rast_clear(rz, kBg.r, kBg.g, kBg.b, 0, RAST_H);
  switch (s->screen) {
    case FOH_STARTUP: render_startup(s, rz); break;
    case FOH_TITLE: render_title(rz); break;
    case FOH_MENU_TOP:
    case FOH_MENU_OPTIONS:
    case FOH_MENU_BATTLE:
    case FOH_MENU_CONTROLS: render_menu(s, rz); break;
    case FOH_CSS: render_css(s, rz); break;
    case FOH_SSS: render_sss(s, rz); break;
    case FOH_OPT_GAMEPLAY: render_opt_gameplay(s, rz); break;
    case FOH_MATCH: render_match(rz); break;
    case FOH_TSS: render_tss(s, rz); break;
    case FOH_TMATCH: render_tmatch(rz); break;
    default: gfx_fatal("foh_render: invalid screen");
  }
}
