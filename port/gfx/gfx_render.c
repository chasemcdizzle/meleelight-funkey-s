// port/gfx/gfx_render.c — scene compositor (M3 task 3). See gfx.h.
//
// Structure-parallel translation of the browser render plane:
//   - stage lines/fills: src/stages/stagerender.js drawStageInit (fg1,
//     drawn once upstream — static content, composited every frame here)
//     + drawStage's per-frame fg2 bits (movingPlats strokes, polygon
//     body fill). Damage lines never draw on VS stages (no
//     SurfaceProperties third element at the pin — STAB1 header note);
//     box/target/background planes are pinned empty/absent on VS.
//   - players: src/main/render.js renderPlayer (facing flips, colour
//     branch, charge jiggle, miniView bubble, ENTRANCE squash, shield
//     disc, REBIRTH halo). Debug overlays (showECB/showHitbox/
//     showLedgeGrabBox) are settings-menu toggles, never set in the
//     harness domain — TRAPPED loudly if ever true, not silently
//     skipped.
//   - articles: src/physics/article.js renderArticles (LASER quad via
//     chromaticAberration x3 passes; ILLUSION is noDraw upstream).
//
// Colour LITERALS that live in upstream render CODE (stage line hexes,
// flash white, charge yellow, laser stroke/fill) are carried verbatim as
// C constants (the task-14 "upstream CODE literals, not pipeline data"
// precedent). Colour DATA PLANES (palettes table, pPal, reverseModel
// flags, flashOnLCancel) come from the executed GFXDATA1 dump — never
// hand-typed.
//
// Canvas-blend fidelity notes (mask-identical, colour-approximate; the
// IoU check judges ink, the PPM is a human artifact):
//   - "screen" composite (chromaticAberration) is drawn source-over.
//   - stroke joins are butt-capped segment quads (canvas miters are
//     sub-pixel at the 0.2 retarget).
#include "gfx.h"
#include "gfx_vfx.h" // M4 task 2: background/vfx/overlay passes + boxFill

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ml_tables.h" // CTAB1 (generated; -I the tables build dir)
#include "../fdlibm/fdlibm.h" // fd_sin/fd_cos/fd_atan2 — device-libm class:
// the device's static musl transcendentals are UNSWEPT (and its floor
// family measured broken, iter 38); all render trig routes through the
// vendored fdlibm doubles — bit-exact host==device AND the more faithful
// translation (the browser renders under the fdlibm Math shim). floor/
// fmod below stay naked: fdlibm.c's strong overrides make them exact on
// every fdlibm.c-linking target (this TU is always linked with it).

// --- camera ---------------------------------------------------------------
// world (stage units) -> canvas px (1200x750, doubles, VERBATIM upstream
// transform) -> screen (240x240 floats, retarget constants).

static double canvas_x(const Gfx *g, double wx) { return wx * g->scale + g->offx; }
static double canvas_y(const Gfx *g, double wy) { return wy * -g->scale + g->offy; }
static float SX(double cx) { return (float)(cx * GFX_K); }
static float SY(double cy) { return (float)(cy * GFX_K + GFX_DY); }
static float SL(double clen) { return (float)(clen * GFX_K); } // lengths

static RastCol col_rgb(GfxRgb c) {
  RastCol r = { c.r, c.g, c.b, 256 };
  return r;
}
static RastCol col_rgba(uint8_t r, uint8_t g, uint8_t b, double a) {
  RastCol c = { r, g, b, 0 };
  double a256 = a * 256.0;
  if (a256 < 0) a256 = 0;
  if (a256 > 256) a256 = 256;
  c.a256 = (uint16_t)(a256 + 0.5);
  return c;
}

// --- GFXDATA1 loader --------------------------------------------------------

static void gd_fail(const char *what, const char *line) {
  fprintf(stderr, "gfxdata: %s: %s\n", what, line ? line : "");
  gfx_fatal("GFXDATA1 artifact violation");
}

void gfx_data_load(GfxData *d, const char *path) {
  memset(d, 0, sizeof *d);
  FILE *f = fopen(path, "r");
  if (!f) gd_fail("cannot open", path);
  char line[256];
  int seenPal = 0, seenPPal = 0, seenFlash = 0, seenEnd = 0;
  if (!fgets(line, sizeof line, f) || strcmp(line, "GFXDATA1\n") != 0) {
    gd_fail("bad magic", line);
  }
  while (fgets(line, sizeof line, f)) {
    if (strcmp(line, "END\n") == 0) { seenEnd = 1; break; }
    if (strncmp(line, "FLASHONLCANCEL ", 15) == 0) {
      const char v = line[15];
      if ((v != '0' && v != '1') || line[16] != '\n') gd_fail("bad flash", line);
      d->flashOnLCancel = v == '1';
      seenFlash = 1;
    } else if (strncmp(line, "PPAL ", 5) == 0) {
      if (sscanf(line + 5, "%d %d %d %d", &d->pPal[0], &d->pPal[1],
                 &d->pPal[2], &d->pPal[3]) != 4) gd_fail("bad ppal", line);
      for (int i = 0; i < 4; i++) {
        if (d->pPal[i] < 0 || d->pPal[i] >= GFX_PALETTES) gd_fail("ppal range", line);
      }
      seenPPal = 1;
    } else if (strncmp(line, "PAL ", 4) == 0) {
      int i, j, r, g, b;
      if (sscanf(line + 4, "%d %d %d %d %d", &i, &j, &r, &g, &b) != 5 ||
          i < 0 || i >= GFX_PALETTES || j < 0 || j >= GFX_PALETTE_COLS ||
          r < 0 || r > 255 || g < 0 || g > 255 || b < 0 || b > 255) {
        gd_fail("bad pal row", line);
      }
      d->palettes[i][j].r = (uint8_t)r;
      d->palettes[i][j].g = (uint8_t)g;
      d->palettes[i][j].b = (uint8_t)b;
      seenPal++;
    } else if (strncmp(line, "REV ", 4) == 0) {
      int c;
      char name[ML_STR_CAP];
      if (sscanf(line + 4, "%d %47s", &c, name) != 2 ||
          c < 0 || c >= GFX_CHARS) gd_fail("bad rev row", line);
      if (d->revCount[c] >= GFX_REV_CAP) gd_fail("rev overflow", line);
      snprintf(d->revStates[c][d->revCount[c]], ML_STR_CAP, "%s", name);
      d->revCount[c]++;
    } else {
      gd_fail("unknown line", line);
    }
  }
  fclose(f);
  if (!seenEnd) gd_fail("missing END", NULL);
  if (seenPal != GFX_PALETTES * GFX_PALETTE_COLS) gd_fail("palette rows != 35", NULL);
  if (!seenPPal || !seenFlash) gd_fail("missing PPAL/FLASHONLCANCEL", NULL);
}

static int gfx_reverse_model(const GfxData *d, int charId, const char *state) {
  for (int k = 0; k < d->revCount[charId]; k++) {
    if (strcmp(d->revStates[charId][k], state) == 0) return 1;
  }
  return 0;
}

// --- init -------------------------------------------------------------------

void gfx_load_anim(Gfx *g, const char *dir, int charId) {
  static const char *const names[GFX_CHARS] = {
    "marth", "puff", "fox", "falco", "falcon"
  };
  if (charId < 0 || charId >= GFX_CHARS) gfx_fatal("gfx: charId out of range");
  if (g->animLoaded[charId]) return;
  char path[1024];
  snprintf(path, sizeof path, "%s/anim_%d_%s.bin", dir, charId, names[charId]);
  anim1_load(&g->anim[charId], path, charId);
  g->animLoaded[charId] = 1;
}

void gfx_init(Gfx *g, int stageId, int backgroundType) {
  if (stageId < 0 || stageId >= ML_STAGE_COUNT) gfx_fatal("gfx: bad stage id");
  g->stab = &ml_stages[stageId];
  g->scale = ml_stage_f64(g->stab->scale);
  g->offx = (double)g->stab->offset[0];
  g->offy = (double)g->stab->offset[1];
  g->backgroundType = backgroundType;
  g->fg2LineWidth = 1.0; // fresh canvas context default
}

// --- stroke helpers ------------------------------------------------------------

// canvas-space segment stroke (width in canvas px)
static void stroke_cseg(Gfx *g, double x0, double y0, double x1, double y1,
                        double w, RastCol col) {
  rast_stroke_seg(&g->rz, SX(x0), SY(y0), SX(x1), SY(y1), SL(w), col);
}

static void stroke_cpoly(Gfx *g, const double *xy, int n, int closed,
                         double w, RastCol col) {
  for (int i = 0; i + 1 < n; i++) {
    stroke_cseg(g, xy[i * 2], xy[i * 2 + 1], xy[i * 2 + 2], xy[i * 2 + 3], w, col);
  }
  if (closed && n > 2) {
    stroke_cseg(g, xy[(n - 1) * 2], xy[(n - 1) * 2 + 1], xy[0], xy[1], w, col);
  }
}

// --- stage ------------------------------------------------------------------------
// Colour literals from upstream render code, carried verbatim:
static const GfxRgb COL_GROUND  = { 0xdb, 0x80, 0xcc }; // "#db80cc"
static const GfxRgb COL_CEILING = { 0xed, 0x67, 0x67 }; // "#ed6767"
static const GfxRgb COL_PLAT    = { 0x47, 0x94, 0xc6 }; // "#4794c6"
static const GfxRgb COL_WALLL   = { 0x47, 0xc6, 0x48 }; // "#47c648"
static const GfxRgb COL_WALLR   = { 0x98, 0x67, 0xde }; // "#9867de"
static const GfxRgb COL_LEDGE   = { 0xE7, 0xA4, 0x4C }; // "#E7A44C"

// stage_w (M4 task 3): the stage-surface LEGIBILITY clamp — a deliberate,
// documented device-scale adaptation (rationale + constant: gfx.h
// GFX_LEGIBLE_MIN_DEV_PX). Applied UNIFORMLY at every stage-surface
// stroke site (stroke_surface_list, ledge ticks, moving-plat fg2
// strokes) and NOWHERE else — players/articles/vfx/overlay keep
// upstream-faithful widths, fills are untouched. With g->legibility == 0
// (default; every browser-IoU-compared path) this is the identity, so
// host faithfulness measurements are byte-unchanged.
static double stage_w(const Gfx *g, double w) {
  if (g->legibility && w * GFX_K < GFX_LEGIBLE_MIN_DEV_PX) {
    return GFX_LEGIBLE_MIN_DEV_PX / GFX_K;
  }
  return w;
}

static void stroke_surface_list(Gfx *g, const ml_stage_surface_t *list,
                                int count, GfxRgb col, double w) {
  w = stage_w(g, w);
  for (int j = 0; j < count; j++) {
    stroke_cseg(g,
                canvas_x(g, ml_stage_f64(list[j].x1)),
                canvas_y(g, ml_stage_f64(list[j].y1)),
                canvas_x(g, ml_stage_f64(list[j].x2)),
                canvas_y(g, ml_stage_f64(list[j].y2)), w, col_rgb(col));
  }
}

static int is_moving_plat(const ml_stage_t *st, int j) {
  for (int k = 0; k < st->movingPlatCount; k++) {
    if (st->movingPlats[k] == j) return 1;
  }
  return 0;
}

// drawStageInit (stagerender.js:48-227, holiday !== 1 arms) — the fg1
// static layer, redrawn per frame here (identical composite).
static void draw_stage_init_layer(Gfx *g) {
  const ml_stage_t *st = g->stab;
  stroke_surface_list(g, st->ground, st->groundCount, COL_GROUND, 1.0);
  stroke_surface_list(g, st->ceiling, st->ceilingCount, COL_CEILING, 1.0);
  for (int j = 0; j < st->platformCount; j++) { // non-moving only
    if (is_moving_plat(st, j)) continue;
    stroke_surface_list(g, &st->platform[j], 1, COL_PLAT, 1.0);
  }
  stroke_surface_list(g, st->wallL, st->wallLCount, COL_WALLL, 1.0);
  stroke_surface_list(g, st->wallR, st->wallRCount, COL_WALLR, 1.0);
  // ledge ticks (lineWidth 2): pA = surface[point], pB = surface[1-point]
  for (int i = 0; i < st->ledgeCount; i++) {
    const ml_stage_ledge_t *e = &st->ledge[i];
    const ml_stage_surface_t *list = e->type == 0 ? st->ground : st->platform;
    const int cnt = e->type == 0 ? st->groundCount : st->platformCount;
    if (e->index < 0 || e->index >= cnt) gfx_fatal("gfx: ledge index range");
    const ml_stage_surface_t *sf = &list[e->index];
    const double ax = ml_stage_f64(e->side == 0 ? sf->x1 : sf->x2);
    const double ay = ml_stage_f64(e->side == 0 ? sf->y1 : sf->y2);
    const double bx = ml_stage_f64(e->side == 0 ? sf->x2 : sf->x1);
    const double by = ml_stage_f64(e->side == 0 ? sf->y2 : sf->y1);
    const double ang = fd_atan2(by - ay, bx - ax);
    const double mag = sqrt((bx - ax) * (bx - ax) + (by - ay) * (by - ay));
    double len = 0.4 * mag;
    const double cap = 20.0 / g->scale;
    if (cap < len) len = cap;
    const double cxp = ax + len * fd_cos(ang), cyp = ay + len * fd_sin(ang);
    stroke_cseg(g, canvas_x(g, ax), canvas_y(g, ay),
                canvas_x(g, cxp), canvas_y(g, cyp), stage_w(g, 2.0),
                col_rgb(COL_LEDGE));
  }
}

// drawStage's per-frame fg2 bits (stagerender.js:262-392, VS arms).
static void draw_stage_fg2(Gfx *g) {
  const ml_stage_t *st = g->stab;
  // ystory takes the Randall arm (bg2 image — excluded layer, no fg2
  // draw); other stages with movingPlats stroke them at the PERSISTED
  // fg2 lineWidth ("fountain" skips submerged platforms).
  const int isYstory = strcmp(st->name, "ystory") == 0;
  const int isFountain = strcmp(st->name, "fountain") == 0;
  if (!isYstory && st->movingPlatCount > 0) {
    for (int k = 0; k < st->movingPlatCount; k++) {
      const int j = st->movingPlats[k];
      if (j < 0 || j >= st->platformCount) gfx_fatal("gfx: movingPlat range");
      const ml_stage_surface_t *pl = &st->platform[j];
      if (isFountain && !(ml_stage_f64(pl->y1) > 0)) continue;
      stroke_cseg(g,
                  canvas_x(g, ml_stage_f64(pl->x1)), canvas_y(g, ml_stage_f64(pl->y1)),
                  canvas_x(g, ml_stage_f64(pl->x2)), canvas_y(g, ml_stage_f64(pl->y2)),
                  stage_w(g, g->fg2LineWidth), col_rgb(COL_PLAT));
    }
  }
  // box: pinned empty on VS stages (STAB1). polygon: the stage body,
  // filled with the LIVE boxFill (stagerender.js module state:
  // drawBackgroundInit baseline; drawStars' per-frame hsla mutation now
  // runs in the C background pass — M4 task 2). Ink-identical to the
  // static fill (alpha > 0 either way); colour is the live-play look.
  const RastCol boxFill = gfx_bg_box_fill();
  for (int j = 0; j < st->polygonCount; j++) {
    const int nv = st->polygonVertCounts[j];
    const ml_stage_vec2b_t *p = st->polygons[j];
    rast_path_reset();
    rast_sub_begin(SX(canvas_x(g, ml_stage_f64(p[0].x))),
                   SY(canvas_y(g, ml_stage_f64(p[0].y))));
    for (int n = 1; n < nv; n++) {
      rast_sub_line(SX(canvas_x(g, ml_stage_f64(p[n].x))),
                    SY(canvas_y(g, ml_stage_f64(p[n].y))));
    }
    rast_sub_close();
    rast_fill(&g->rz, boxFill);
  }
  // damage lines: no VS surface carries SurfaceProperties at the pin —
  // drawDamageLine draws nothing; only the lineWidth=4 assignment before
  // that loop is observable state:
  g->fg2LineWidth = 4.0;
  // target plane: absent on VS stages (STAB1 pins).
}

// --- players -----------------------------------------------------------------------

// unmakeColour for player[i].colourOverlay strings ("rgb(r,g,b)" /
// "rgba(r,g,b,a)"); strict, loud on anything else.
static GfxRgb parse_colour(const char *s, double *alphaOut) {
  int r, gg, b;
  double a = 1.0;
  GfxRgb c;
  *alphaOut = 1.0;
  if (strcmp(s, "white") == 0) { // marth DOWNSPECIAL* literal
    c.r = c.g = c.b = 255;
    return c;
  }
  if (sscanf(s, "rgba(%d ,%d ,%d ,%lf)", &r, &gg, &b, &a) == 4 ||
      sscanf(s, "rgb(%d ,%d ,%d)", &r, &gg, &b) == 3) {
    if (r < 0 || r > 255 || gg < 0 || gg > 255 || b < 0 || b > 255) {
      gfx_fatal("gfx: colourOverlay channel out of range");
    }
    c.r = (uint8_t)r; c.g = (uint8_t)gg; c.b = (uint8_t)b;
    *alphaOut = a;
    return c;
  }
  fprintf(stderr, "gfx: unparseable colour string: %s\n", s);
  gfx_fatal("gfx: colourOverlay parse");
  c.r = c.g = c.b = 0;
  return c;
}

// blendColours (src/main/vfx/blendColours.js), opacity 0.7 sites.
static GfxRgb blend_colours(GfxRgb start, int er, int eg, int eb, double op) {
  GfxRgb c;
  c.r = (uint8_t)floor((double)start.r + ((double)er - start.r) * op);
  c.g = (uint8_t)floor((double)start.g + ((double)eg - start.g) * op);
  c.b = (uint8_t)floor((double)start.b + ((double)eb - start.b) * op);
  return c;
}

// drawArrayPathCompress (render.js:37-69): one ANIM1 frame as a single
// nonzero-winding fill. Transform (canvas semantics):
//   canvasPt = (tX-rpX, tY-rpY) + Rot(rotate) . (v*scale*face + rp)
static void draw_anim_frame(Gfx *g, const Anim1 *anim, const Anim1State *st,
                            uint32_t frameIdx, RastCol col, double face,
                            double tX, double tY, double scaleX, double scaleY,
                            double rotate, double rpX, double rpY) {
  int absent = 0;
  uint32_t cursor = 0;
  const uint32_t nPaths = anim1_frame(anim, st, frameIdx, &cursor, &absent);
  if (absent) return; // spec §2.4: absent frame -> skip draw
  const double cr = fd_cos(rotate), sr = fd_sin(rotate);
  const double tx = tX - rpX, ty = tY - rpY;
  rast_path_reset();
  for (uint32_t j = 0; j < nPaths; j++) {
    const Anim1Path p = anim1_next_path(anim, &cursor);
    if (p.coordCount < 2) continue; // shape contract; container copies verbatim
    double mx = (double)anim1_coord(&p, 0) * scaleX * face + rpX;
    double my = (double)anim1_coord(&p, 1) * scaleY + rpY;
    rast_sub_begin(SX(tx + mx * cr - my * sr), SY(ty + mx * sr + my * cr));
    for (uint32_t k = 2; k + 5 < p.coordCount; k += 6) {
      float q[6];
      for (int t = 0; t < 3; t++) {
        mx = (double)anim1_coord(&p, k + (uint32_t)t * 2) * scaleX * face + rpX;
        my = (double)anim1_coord(&p, k + (uint32_t)t * 2 + 1) * scaleY + rpY;
        q[t * 2] = SX(tx + mx * cr - my * sr);
        q[t * 2 + 1] = SY(ty + mx * sr + my * cr);
      }
      rast_sub_cubic(q[0], q[1], q[2], q[3], q[4], q[5]);
    }
    rast_sub_close();
  }
  rast_fill(&g->rz, col);
}

static int32_t frames_data_lookup(int charId, const char *state, int *found) {
  const ml_frames_entry_t *e = ml_frames_data[charId];
  const int32_t n = ml_frames_count[charId];
  for (int32_t k = 0; k < n; k++) {
    if (strcmp(e[k].name, state) == 0) { *found = 1; return e[k].frames; }
  }
  *found = 0;
  return 0;
}

// renderPlayer (render.js:72-391).
static void render_player(Gfx *g, const GameState *st, int i) {
  const MlPlayer *p = &st->sim.player[i];
  const int charId = (int)st->sim.characterSelections[i];
  if (charId < 0 || charId >= GFX_CHARS) gfx_fatal("gfx: charSel range");
  if (!g->animLoaded[charId]) gfx_fatal("gfx: animations not loaded for char");
  if (p->showLedgeGrabBox || p->showECB || p->showHitbox) {
    gfx_fatal("gfx: debug overlay flags out of captured domain");
  }
  const GfxData *D = &g->data;
  const GfxRgb *pal = D->palettes[D->pPal[i]];

  double temX = canvas_x(g, p->phys.pos.x);
  double temY = canvas_y(g, p->phys.pos.y);
  double face = p->phys.face;
  double frame = floor(p->timer);
  if (frame == 0) frame = 1;
  int fdFound = 0;
  const int32_t fd = frames_data_lookup(charId, p->actionState, &fdFound);
  if (fdFound && frame > (double)fd) frame = (double)fd;

  const Anim1State *as = anim1_find(&g->anim[charId], p->actionState);
  if (!as) return; // animations[c][s] === undefined
  if (frame < 1 || (uint32_t)(frame - 1) >= as->frameCount) {
    return; // animations[c][s][frame-1] === undefined
  }
  const uint32_t frameIdx = (uint32_t)(frame - 1);

  // facing flips (render.js:92-119)
  const size_t slen = strlen(p->actionState);
  if (gfx_reverse_model(D, charId, p->actionState)) {
    face *= -1;
  } else if (strcmp(p->actionState, "TILTTURN") == 0) {
    if (frame > 5) face *= -1;
  } else if (strcmp(p->actionState, "RUNTURN") == 0) {
    if (frame > (double)ml_attributes[charId].runTurnBreakPoint) face *= -1;
  } else if (slen == 11 && strncmp(p->actionState, "AERIALTURN", 10) == 0 &&
             p->timer > 5) {
    face *= -1; // JiGGS MULTIJUMP TURN (substring(0, len-1) quirk)
  } else if (strcmp(p->actionState, "ATTACKAIRB") == 0 && charId == 0) {
    if (frame > 29) face *= -1; // MARTH BAIR
  } else if (strcmp(p->actionState, "THROWBACK") == 0 &&
             (charId == 2 || charId == 3)) {
    if (frame >= 10) face *= -1; // FOX/FALCO BTHROW
  }

  const MlAsFlags *flags = mlp_flags(&st->sim, (double)charId, p->actionState);
  const double sc45 = g->scale / 4.5;
  const double charScale = ml_f64(ml_attributes[charId].charScale);
  const double miniScale = ml_f64(ml_attributes[charId].miniScale);

  if (!flags->dead) {
    // colour branch (render.js:121-164). colA carries an rgba() overlay's
    // alpha (e.g. puff sing "rgba(255, 248, 88, 0.83)").
    GfxRgb col;
    double colA = 1.0;
    if (p->phys.shielding && p->phys.powerShielded && p->hit.hitlag > 0) {
      col = (GfxRgb){255, 255, 255};
    } else if (D->flashOnLCancel &&
               strncmp(p->actionState, "LANDINGATT", 10) == 0 &&
               p->phys.landingLagScaling == 2 &&
               fmod(js_round(p->timer), 3) == 0) {
      col = (GfxRgb){255, 255, 255};
    } else if (fmod(p->phys.intangibleTimer, 9) > 3 ||
               fmod(p->phys.invincibleTimer, 9) > 3 || p->hit.hitlag > 0) {
      col = pal[1];
    } else if (p->phys.charging && fmod(p->phys.chargeFrames, 9) > 3) {
      col = (GfxRgb){252, 255, 91}; // "rgb(252, 255, 91)"
    } else if (strcmp(p->actionState, "FURAFURA") == 0 &&
               fmod(p->timer, 30) < 6) {
      col = pal[3];
    } else if (p->colourOverlayBool) {
      col = parse_colour(p->colourOverlay, &colA);
    } else if (p->shocked > 0) {
      col = fmod(p->shocked, 2) != 0
        ? blend_colours(pal[0], 14, 0, 131, 0.7)
        : blend_colours(pal[0], 255, 255, 255, 0.7);
    } else if (p->burning > 0) {
      const double part = fmod(p->burning, 3);
      if (part != 0) {
        col = part == 1 ? blend_colours(pal[0], 253, 255, 161, 0.7)
                        : blend_colours(pal[0], 198, 57, 5, 0.7);
      } else {
        col = pal[0];
      }
    } else {
      col = pal[0];
    }
    if (fmod(p->phys.chargeFrames, 4) == 3) temX += 2;
    else if (fmod(p->phys.chargeFrames, 4) == 1) temX -= 2;

    // miniView predicate (render.js:170-208; local-only in C — upstream
    // also WRITES phys.outOfCameraTimer here, which the sim consumes;
    // the C renderer must not, and the render-on replay's STREAM MATCH
    // proves it).
    int miniView = 0;
    double mvX = 0, mvY = 0;
    if (temX > 1220 || temX < -20 || temY > 880 || temY < -30) {
      const double pAx = temX - 600, pAy = temY - 375;
      // pAx == 0 divide (review-44 item 5, faithfulness-verified against
      // upstream render.js:173): upstream computes s = pA.y/pA.x with NO
      // zero guard, so JS yields +-Infinity there and we must too. On our
      // IEEE-754 targets (host clang/gcc, armv7 VFP; -ffp-contract=off
      // only — no fast-math, and no FP trap is ever enabled anywhere in
      // the port: no feenableexcept/fesetenv) x/0.0 is Annex-F-defined as
      // +-Inf, matching JS bit-for-bit. NaN is impossible here: pAx == 0
      // means temX == 600, which satisfies neither horizontal arm of the
      // outer guard, so temY > 880 or temY < -30 must hold and
      // |pAy| >= 405. The Inf then propagates exactly like upstream:
      // s*600 = +-Inf fails the first branch; 375/s = +-0 passes the
      // second; pAy > 0 takes the clamp arm (no division), pAy < 0 takes
      // -375/s + offx = -+0 + offx = offx. No guard is added — a guard
      // would change drawn output vs upstream (hard rule 5).
      const double s = pAy / pAx;
      if (-375 <= s * 600 && s * 600 <= 375) {
        miniView = 1;
        if (pAx > 0) { mvX = 1150; mvY = s * 600 + 375; }
        else { mvX = 50; mvY = -s * 600 + 375; }
      } else if (-600 <= 375 / s && 375 / s <= 600) {
        miniView = 1;
        if (pAy > 0) {
          mvY = 700;
          mvX = temX < 50 ? 50 : (temX > 1150 ? 1150 : temX);
        } else {
          mvX = -375 / s + g->offx;
          mvY = 50;
        }
      }
    }

    if (miniView && strcmp(p->actionState, "SLEEP") != 0) {
      // bubble: black disc r35 + pal[0] ring lineWidth 6
      rast_circle(&g->rz, SX(mvX), SY(mvY), SL(35.0), col_rgba(0, 0, 0, 1.0));
      rast_ring(&g->rz, SX(mvX), SY(mvY), SL(35.0), SL(6.0), col_rgb(pal[0]));
      g->fg2LineWidth = 1.0;
      draw_anim_frame(g, &g->anim[charId], as, frameIdx, col_rgba(col.r, col.g, col.b, colA), face,
                      mvX, mvY + 30, miniScale, miniScale,
                      p->rotation, p->rotationPoint.x, p->rotationPoint.y);
    } else if (strcmp(p->actionState, "ENTRANCE") == 0) {
      double syv = charScale * (1.5 - st->startTimer);
      if (charScale < syv) syv = charScale;
      draw_anim_frame(g, &g->anim[charId], as, frameIdx, col_rgba(col.r, col.g, col.b, colA), face,
                      temX, temY, charScale * sc45, syv * sc45,
                      p->rotation, p->rotationPoint.x, p->rotationPoint.y);
    } else {
      draw_anim_frame(g, &g->anim[charId], as, frameIdx, col_rgba(col.r, col.g, col.b, colA), face,
                      temX, temY, charScale * sc45, charScale * sc45,
                      p->rotation, p->rotationPoint.x, p->rotationPoint.y);
    }
  }

  // shield (render.js:235-248)
  if (p->phys.shielding && !(p->phys.powerShielded && p->hit.hitlag > 0)) {
    const double sXc = canvas_x(g, p->phys.shieldPositionReal.x);
    const double sYc = canvas_y(g, p->phys.shieldPositionReal.y);
    const GfxRgb sCol = floor(p->hit.shieldstun) > 0 ? pal[4] : pal[2];
    rast_circle(&g->rz, SX(sXc), SY(sYc), SL(p->phys.shieldSize * g->scale),
                col_rgba(sCol.r, sCol.g, sCol.b, 0.6 * p->phys.shieldAnalog));
  }

  // hasTag: never set in the harness domain (no tag UI) — nothing drawn.

  // REBIRTH halo (render.js:268-279): fill pal[1], stroke pal[0] at the
  // PERSISTED fg2 lineWidth.
  if (strcmp(p->actionState, "REBIRTH") == 0 ||
      strcmp(p->actionState, "REBIRTHWAIT") == 0) {
    const double quad[8] = {
      temX + 18 * sc45, temY + 13.5 * sc45,
      temX + 31.5 * sc45, temY,
      temX - 31.5 * sc45, temY,
      temX - 18 * sc45, temY + 13.5 * sc45,
    };
    float fq[8];
    for (int k = 0; k < 4; k++) {
      fq[k * 2] = SX(quad[k * 2]);
      fq[k * 2 + 1] = SY(quad[k * 2 + 1]);
    }
    rast_poly(&g->rz, fq, 4, col_rgb(pal[1]));
    stroke_cpoly(g, quad, 4, 1, g->fg2LineWidth, col_rgb(pal[0]));
  }
}

// --- articles ------------------------------------------------------------------------
// renderArticles (article.js:187-193) + LASER.draw (article.js:104-119):
// laser quad head->tail with rotated end caps, drawn 3x by
// chromaticAberration (G at 0, R at -vec, B at +vec; opacity 0.8),
// fill + lineWidth-2 stroke per pass. ILLUSION is noDraw.
//
// Colour: upstream stores stroke/fill on the SHARED articles.LASER
// object at init (isFox). No golden fields fox AND falco lasers
// together, so owner-character (fox=2 default-true arm, falco=3
// isFox:false arm) reproduces the observable colours exactly; the
// last-init-wins shared-object quirk is unreachable in the corpus
// (documented exposure, not modeled).
static const GfxRgb LASER_STROKE_FOX = {255, 59, 59};    // "rgba(255, 59, 59,0.6)"
static const GfxRgb LASER_FILL_FOX = {255, 193, 193};    // "rgb(255, 193, 193)"
static const GfxRgb LASER_STROKE_FALCO = {73, 130, 234}; // "rgba(73,130,234,0.6)"
static const GfxRgb LASER_FILL_FALCO = {225, 255, 255};  // "rgb(225, 255, 255)"

static void laser_pass(Gfx *g, const double *hx6, const double *hy6,
                       double ox, double oy, GfxRgb stroke, GfxRgb fill) {
  float fq[12];
  double dq[12];
  for (int k = 0; k < 6; k++) {
    dq[k * 2] = hx6[k] + ox;
    dq[k * 2 + 1] = hy6[k] + oy;
    fq[k * 2] = SX(dq[k * 2]);
    fq[k * 2 + 1] = SY(dq[k * 2 + 1]);
  }
  rast_poly(&g->rz, fq, 6, col_rgba(fill.r, fill.g, fill.b, 0.8));
  stroke_cpoly(g, dq, 6, 1, 2.0, col_rgba(stroke.r, stroke.g, stroke.b, 0.8));
}

static void render_articles(Gfx *g, const GameState *st) {
  for (int i = 0; i < st->arts.count; i++) {
    const MlArticle *a = &st->arts.a[i];
    if (a->kind != ART_LASER) continue; // ILLUSION: noDraw
    const int owner = (int)a->player;
    if (owner < 0 || owner > 3) gfx_fatal("gfx: laser owner range");
    const int ownerChar = (int)st->sim.characterSelections[owner];
    GfxRgb stroke, fill;
    if (ownerChar == 2) { stroke = LASER_STROKE_FOX; fill = LASER_FILL_FOX; }
    else if (ownerChar == 3) { stroke = LASER_STROKE_FALCO; fill = LASER_FILL_FALCO; }
    else gfx_fatal("gfx: laser owner is neither fox nor falco");
    const double hx = canvas_x(g, a->pos.x), hy = canvas_y(g, a->pos.y);
    const double tx = canvas_x(g, a->posPrev.x), ty = canvas_y(g, a->posPrev.y);
    const double d = hx > tx ? 1.0 : -1.0;
    const double r = a->rotate;
    // rotateVector(vx, vy, -r) (render.js:30-34)
    const double cr = fd_cos(-r), sr = fd_sin(-r);
    const double v1x = -4 * cr - 2 * sr, v1y = -4 * sr + 2 * cr;
    const double v2x = 4 * cr - 2 * sr, v2y = 4 * sr + 2 * cr;
    const double v3x = 4 * cr + 2 * sr, v3y = 4 * sr - 2 * cr;
    const double v4x = -4 * cr + 2 * sr, v4y = -4 * sr - 2 * cr;
    const double px[6] = { hx, hx + v1x * d, tx + v2x * d, tx, tx + v3x * d,
                           hx + v4x * d };
    const double py[6] = { hy, hy + v1y, ty + v2y, ty, ty + v3y, hy + v4y };
    const double vx = -0.3 * fd_sin(r) * g->scale;
    const double vy = -0.3 * fd_cos(r) * g->scale;
    laser_pass(g, px, py, 0, 0, stroke, fill);        // G pass site
    laser_pass(g, px, py, -vx, -vy, stroke, fill);    // R pass (translate(-vec))
    laser_pass(g, px, py, vx, vy, stroke, fill);      // B pass (translate(2vec))
    g->fg2LineWidth = 2.0; // drawLaserLine's assignment persists
  }
}

// --- frame -------------------------------------------------------------------------

// Per-pass render profiler (M4 task 3 attribution instrument; seeds the
// task-8 skip-burst instrument): COMPILE-TIME gated — a default build
// carries no timing calls and no branches (PROF expands to the bare
// call), so shipped/gate binaries are unaffected. Build a scratch
// binary with -DMLFK_RENDER_PROF for attribution runs; gfx_app calls
// gfx_render_prof_dump() post-run (empty in default builds).
#ifdef MLFK_RENDER_PROF
#include <time.h>
static uint64_t prof_now_ns(void) {
  struct timespec ts;
  if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) gfx_fatal("prof clock");
  return (uint64_t)ts.tv_sec * 1000000000ull + (uint64_t)ts.tv_nsec;
}
static uint64_t g_prof[8]; // clear,bg,stage1,stage2,players,articles,vfx,overlay
static uint64_t g_prof_frames;
#define PROF(i, call) do { \
    const uint64_t prof_t_ = prof_now_ns(); \
    call; \
    g_prof[i] += prof_now_ns() - prof_t_; \
  } while (0)
void gfx_render_prof_dump(void) {
  static const char *const names[8] = { "clear", "bg", "stage1", "stage2",
                                        "players", "articles", "vfx",
                                        "overlay" };
  fprintf(stderr, "gfx_render prof (%llu frames), avg ns/frame:",
          (unsigned long long)g_prof_frames);
  for (int i = 0; i < 8; i++) {
    fprintf(stderr, " %s=%llu", names[i],
            (unsigned long long)(g_prof_frames ? g_prof[i] / g_prof_frames : 0));
  }
  fprintf(stderr, "\n");
}
#else
#define PROF(i, call) call
void gfx_render_prof_dump(void) {}
#endif

void gfx_render_frame(Gfx *g, const GameState *st) {
  // full gameMode-3 sequence (main.js renderTick "playing" branch):
  // clearScreen -> drawBackground -> drawStage -> renderPlayer x4 ->
  // renderArticles -> renderVfx -> renderOverlay(true). The background
  // pass is ink-suppressed (bg planes are not in the IoU mask on either
  // side — gfx_bg.c note); everything after it inks normally.
  PROF(0, rast_clear(&g->rz, 0, 0, 0, (int)GFX_DY, (int)(GFX_DY + 750.0 * GFX_K)));
  PROF(1, gfx_render_background(g));
  PROF(2, draw_stage_init_layer(g)); // fg1 composite (static)
  PROF(3, draw_stage_fg2(g));        // drawStage per-frame fg2 bits
  for (int i = 0; i < 4; i++) {
    if (st->sim.playerPresent[i]) PROF(4, render_player(g, st, i));
  }
  PROF(5, render_articles(g, st));
  PROF(6, gfx_render_vfx(g, st));
  PROF(7, gfx_render_overlay(g, st));
#ifdef MLFK_RENDER_PROF
  g_prof_frames++;
#endif
}

// --- dumps -------------------------------------------------------------------------

void gfx_dump_ppm(const Gfx *g, const char *path) {
  FILE *f = fopen(path, "wb");
  if (!f) gfx_fatal("gfx: cannot open ppm for writing");
  fprintf(f, "P6\n%d %d\n255\n", RAST_W, RAST_H);
  for (int i = 0; i < RAST_W * RAST_H; i++) {
    const uint16_t p = g->rz.fb[i];
    const uint8_t rgb[3] = {
      (uint8_t)(((p >> 11) & 31) << 3),
      (uint8_t)(((p >> 5) & 63) << 2),
      (uint8_t)((p & 31) << 3),
    };
    if (fwrite(rgb, 1, 3, f) != 3) gfx_fatal("gfx: ppm write failed");
  }
  if (fclose(f) != 0) gfx_fatal("gfx: ppm close failed");
}

void gfx_dump_ink_pgm(const Gfx *g, const char *path) {
  FILE *f = fopen(path, "wb");
  if (!f) gfx_fatal("gfx: cannot open pgm for writing");
  fprintf(f, "P5\n%d %d\n255\n", RAST_W, RAST_H);
  for (int i = 0; i < RAST_W * RAST_H; i++) {
    const uint8_t v = g->rz.ink[i] ? 255 : 0;
    if (fwrite(&v, 1, 1, f) != 1) gfx_fatal("gfx: pgm write failed");
  }
  if (fclose(f) != 0) gfx_fatal("gfx: pgm close failed");
}
