// port/gfx/gfx_target.c — the TARGET-MODE scene compositor (M4 task 12).
//
// Structure-parallel translation of the browser's per-frame gameMode-5
// render sequence (main.js renderTick, the mode-5 "playing" branch):
//   clearScreen -> drawBackground -> drawStage -> renderPlayer(0) ->
//   renderArticles -> renderVfx() -> renderOverlay(false)
// over a TARGET stage (TTAB1 — ml_targets.h) instead of a VS stage
// (STAB1): the same static world->canvas camera (scale/offset from the
// authored stage), the drawStageInit surface strokes + ledge ticks
// (stagerender.js:48-227, holiday !== 1 arms — the SAME drawing
// gfx_render.c performs for VS lists, restated over the TTAB1 types),
// drawStage's per-frame arms that exist on authored target data
// (boxFill fillRect per box, stagerender.js:285-289; polygon/damage
// planes are pinned ABSENT on authored target stages — iter 94), and
// the TARGET draws: 5 concentric fill circles per undestroyed target,
// radius (25 - j*5) * (scale/4.5), alternating red/white
// (stagerender.js:365-390, holiday 0). renderOverlay(false) = the
// timer-only overlay (gfx_render_overlay_timer — the target-mode
// timer/records HUD's in-match half; records display is the FOH
// screen's surface, task 13 note).
//
// The player/article passes are gfx_render.c's OWN bytes
// (gfx_render_player_pass / gfx_render_articles_pass — visibility
// wrappers), so target-mode player rendering can never drift from the
// IoU-checked VS renderer. Rasterization is NOT checksummed; the
// device check judges twin-vs-device shots byte-exact.
//
// The COMPLETE!/FAILURE end banner (finishGame :1425-1460 render arm)
// is drawn by gfx_target_banner when the driver's finish hook has
// fired — reachable only through the finish seam (mechanically covered
// by target_finish_probe; no committed flow reaches it — the AGENT-LOG
// iter-99 refutation + registered honest-coverage note).
#include "gfx.h"
#include "gfx_vfx.h"

#include <string.h>

#include "ml_targets.h" // TTAB1 (generated; -I the tables build dir)
#include "../sim/target/target_play.h"
#include "../sim/target/custom_stage.h" // A45 T2: MlkStage + mlk_stage_playable
#include "../foh/foh.h" // M4 iter 103: the self-authored FOH 5x7 font
                        // (foh_text) — the frozen VFXGLYPHS atlas font 0
                        // carries only digits + ':', so the banner's
                        // letters must come from the letter-complete FOH
                        // font (review-101 round-2 M; AGENT-LOG iter 103).

// --- camera (gfx_render.c statics restated; same doubles path) --------------
static double tcanvas_x(const Gfx *g, double wx) { return wx * g->scale + g->offx; }
static double tcanvas_y(const Gfx *g, double wy) { return wy * -g->scale + g->offy; }
static float TSX(double cx) { return (float)(cx * GFX_K); }
static float TSY(double cy) { return (float)(cy * GFX_K + GFX_DY); }
static float TSL(double clen) { return (float)(clen * GFX_K); }

// stage_w legibility clamp (gfx.h GFX_LEGIBLE_MIN_DEV_PX; the M4 task-3
// documented device-scale adaptation — same rule as gfx_render.c's)
static double tstage_w(const Gfx *g, double w) {
  if (g->legibility && w * GFX_K < GFX_LEGIBLE_MIN_DEV_PX) {
    return GFX_LEGIBLE_MIN_DEV_PX / GFX_K;
  }
  return w;
}

static void tstroke_cseg(Gfx *g, double x0, double y0, double x1, double y1,
                         double w, RastCol col) {
  rast_stroke_seg(&g->rz, TSX(x0), TSY(y0), TSX(x1), TSY(y1), TSL(w), col);
}

// colour literals: upstream render CODE (stagerender.js), carried
// verbatim — the gfx_render.c constants restated for this TU.
static const RastCol TCOL_GROUND  = { 0xdb, 0x80, 0xcc, 256 }; // "#db80cc"
static const RastCol TCOL_CEILING = { 0xed, 0x67, 0x67, 256 }; // "#ed6767"
static const RastCol TCOL_PLAT    = { 0x47, 0x94, 0xc6, 256 }; // "#4794c6"
static const RastCol TCOL_WALLL   = { 0x47, 0xc6, 0x48, 256 }; // "#47c648"
static const RastCol TCOL_WALLR   = { 0x98, 0x67, 0xde, 256 }; // "#9867de"

static const ml_tstage_t *g_tt; // the bound target stage (TTAB1 row)

void gfx_target_init(Gfx *g, int tstageId, int backgroundType) {
  if (tstageId < 0 || tstageId >= ML_TSTAGE_COUNT) {
    gfx_fatal("gfx_target: bad tstage id");
  }
  g_tt = &ml_tstages[tstageId];
  g->stab = 0; // the VS stage plane is never consulted in target mode
  g->scale = ml_target_f64(g_tt->scale);
  g->offx = (double)g_tt->offset[0];
  g->offy = (double)g_tt->offset[1];
  g->backgroundType = backgroundType;
  g->fg2LineWidth = 1.0; // fresh canvas context default
}

// --- A45 T2 (D42): binding a CUSTOM stage ----------------------------------
//
// Every draw function below reads g_tt, a `const ml_tstage_t *`. The TTAB1
// rows are const generated data, but the POINTER is not and ml_tstage_t is
// an ordinary struct — so a custom stage binds by materialising one row at
// runtime and pointing g_tt at it. All ten draw functions stay
// byte-unchanged, which is the point: the renderer cannot accidentally
// treat a custom stage differently from an authored one, because it cannot
// tell them apart. (The alternative — a second MlkStage plane plus an `if`
// in every draw — is ten branches to keep in sync for no behaviour.)
static struct {
  ml_tstage_t row;
  ml_tstage_surface_t ground[ML_MAX_SURFACES], ceiling[ML_MAX_SURFACES],
      platform[ML_MAX_SURFACES], wallL[ML_MAX_SURFACES],
      wallR[ML_MAX_SURFACES];
  ml_tstage_ledge_t ledge[ML_MAX_LEDGES];
  ml_tstage_vec2b_t target[ML_MAX_TARGETS];
} g_custom;

static uint64_t tbits(double d) {
  uint64_t b;
  memcpy(&b, &d, 8);
  return b;
}

static int32_t tcopy_list(ml_tstage_surface_t *dst, const SurfaceList *src) {
  if (src->count > ML_MAX_SURFACES) {
    gfx_fatal("gfx_target: custom surface list over cap");
  }
  for (int k = 0; k < src->count; k++) {
    dst[k].x1 = tbits(src->items[k].p0.x);
    dst[k].y1 = tbits(src->items[k].p0.y);
    dst[k].x2 = tbits(src->items[k].p1.x);
    dst[k].y2 = tbits(src->items[k].p1.y);
  }
  return (int32_t)src->count;
}

// NOTE: this deliberately calls NOTHING from custom_stage.c. Six scripts
// link gfx_target.c (check-foh-flows.sh, check-device-foh.sh,
// check-device-persist.sh, check-device-target.sh, check-device-fullgame.sh,
// riglib.sh) and a cross-TU call here would force two new TUs into every one
// of their link sets — including port/foh/'s, a lane this ticket must not
// touch. MlkStage is used as a TYPE only, which costs no link edge. Content
// validation is the LOADER's (mlk_slot_load -> mlk_stage_playable): nothing
// reaches this function unvalidated. What is left is the bounds this
// function's OWN copies need, and those are checked here, loudly, rather
// than assumed from a caller's promise.
void gfx_target_init_custom(Gfx *g, const MlkStage *cs, int backgroundType) {
  if (cs->startingPointCount < 1) gfx_fatal("gfx_target: no starting point");
  if (cs->ledgeCount > ML_MAX_LEDGES) gfx_fatal("gfx_target: ledges over cap");
  if (cs->targetCount < 0 || cs->targetCount > ML_MAX_TARGETS) {
    gfx_fatal("gfx_target: targets over cap");
  }
  ml_tstage_t *r = &g_custom.row;
  memset(r, 0, sizeof *r);
  r->name = "custom";
  r->startingPoint.x = tbits(cs->startingPoint[0].x);
  r->startingPoint.y = tbits(cs->startingPoint[0].y);
  r->groundCount = tcopy_list(g_custom.ground, &cs->s.ground);
  r->ground = g_custom.ground;
  r->ceilingCount = tcopy_list(g_custom.ceiling, &cs->s.ceiling);
  r->ceiling = g_custom.ceiling;
  r->platformCount = tcopy_list(g_custom.platform, &cs->s.platform);
  r->platform = g_custom.platform;
  r->wallLCount = tcopy_list(g_custom.wallL, &cs->s.wallL);
  r->wallL = g_custom.wallL;
  r->wallRCount = tcopy_list(g_custom.wallR, &cs->s.wallR);
  r->wallR = g_custom.wallR;
  for (int k = 0; k < cs->ledgeCount; k++) {
    g_custom.ledge[k].type = cs->ledge[k].list == 'g' ? 0 : 1;
    g_custom.ledge[k].index = (int32_t)cs->ledge[k].index;
    g_custom.ledge[k].side = (int32_t)cs->ledge[k].point;
  }
  r->ledgeCount = (int32_t)cs->ledgeCount;
  r->ledge = cs->ledgeCount ? g_custom.ledge : 0;
  for (int k = 0; k < cs->targetCount; k++) {
    g_custom.target[k].x = tbits(cs->target[k].x);
    g_custom.target[k].y = tbits(cs->target[k].y);
  }
  r->targetCount = (int32_t)cs->targetCount;
  r->target = g_custom.target;
  // boxCount 0 and offset [600,375] are CONSTANTS of this plane, not
  // missing data: `box` is authored-stage machinery and is not a field of
  // the share-code grammar at all (encode.js has 14 fields, none of them
  // box), so tdraw_stage_fg2's loop runs zero times exactly as it does for
  // an authored stage with none; and the builder's stageTemp carries
  // offset [600, 375] which NO tool ever edits (targetbuilder.js:66) and
  // encode.js never emits.
  r->offset[0] = 600;
  r->offset[1] = 375;
  r->scale = tbits(cs->scale);

  g_tt = r;
  g->stab = 0; // the VS stage plane is never consulted in target mode
  g->scale = cs->scale;
  g->offx = (double)r->offset[0];
  g->offy = (double)r->offset[1];
  g->backgroundType = backgroundType;
  g->fg2LineWidth = 1.0;
}

static void tstroke_list(Gfx *g, const ml_tstage_surface_t *list,
                         int32_t count, RastCol col, double w) {
  w = tstage_w(g, w);
  for (int32_t j = 0; j < count; j++) {
    tstroke_cseg(g, tcanvas_x(g, ml_target_f64(list[j].x1)),
                 tcanvas_y(g, ml_target_f64(list[j].y1)),
                 tcanvas_x(g, ml_target_f64(list[j].x2)),
                 tcanvas_y(g, ml_target_f64(list[j].y2)), w, col);
  }
}

// drawStageInit (stagerender.js:48-227, holiday !== 1 arms) over the
// TTAB1 lists: ground/ceiling/platform/wallL/wallR strokes (target
// stages author NO movingPlats — every platform takes the static arm).
static void tdraw_stage_init(Gfx *g) {
  tstroke_list(g, g_tt->ground, g_tt->groundCount, TCOL_GROUND, 1.0);
  tstroke_list(g, g_tt->ceiling, g_tt->ceilingCount, TCOL_CEILING, 1.0);
  tstroke_list(g, g_tt->platform, g_tt->platformCount, TCOL_PLAT, 1.0);
  tstroke_list(g, g_tt->wallL, g_tt->wallLCount, TCOL_WALLL, 1.0);
  tstroke_list(g, g_tt->wallR, g_tt->wallRCount, TCOL_WALLR, 1.0);
  // ledge ticks (the gfx_render.c ledge arm is VS-specific over STAB1;
  // authored target-stage ledges are AI-only upstream and target mode
  // fields no CPU — the fg1 tick is omitted; registered render delta,
  // AGENT-LOG iter 99: cosmetic-only, acceptance playthrough authority).
}

// drawStage per-frame (stagerender.js:262-392, target-stage arms):
// boxFill fillRect per authored box (:285-289); polygon/background/
// damage planes pinned absent; fg2.lineWidth = 4 assignment persists.
static void tdraw_stage_fg2(Gfx *g) {
  const RastCol boxFill = gfx_bg_box_fill();
  for (int32_t j = 0; j < g_tt->boxCount; j++) {
    const double minX = ml_target_f64(g_tt->box[j].minX);
    const double minY = ml_target_f64(g_tt->box[j].minY);
    const double maxX = ml_target_f64(g_tt->box[j].maxX);
    const double maxY = ml_target_f64(g_tt->box[j].maxY);
    // fillRect(minX, maxY(top), w, h) in canvas space -> device rect
    const float x0 = TSX(tcanvas_x(g, minX));
    const float y0 = TSY(tcanvas_y(g, maxY));
    const float x1 = TSX(tcanvas_x(g, maxX));
    const float y1 = TSY(tcanvas_y(g, minY));
    rast_path_reset();
    rast_sub_begin(x0, y0);
    rast_sub_line(x1, y0);
    rast_sub_line(x1, y1);
    rast_sub_line(x0, y1);
    rast_sub_close();
    rast_fill(&g->rz, boxFill);
  }
  g->fg2LineWidth = 4.0; // stagerender.js:341 (persists; canvas state)
}

// the targets (stagerender.js:365-390, holiday 0): 5 concentric fill
// circles, radius (25 - j*5) * (scale/4.5), alternating red/white,
// undestroyed only (the render reads the target module's plane const).
static void tdraw_targets(Gfx *g, const MlTargets *tp) {
  const RastCol red = { 255, 0, 0, 256 };
  const RastCol white = { 255, 255, 255, 256 };
  for (int t = 0; t < tp->targetCount; t++) {
    if (tp->targetDestroyed[t]) continue;
    const double cx = tcanvas_x(g, tp->target[t].x);
    const double cy = tcanvas_y(g, tp->target[t].y);
    for (int j = 0; j < 5; j++) {
      const double r = (25 - j * 5) * (g->scale / 4.5);
      rast_circle(&g->rz, TSX(cx), TSY(cy), TSL(r), (j % 2) ? white : red);
    }
  }
}

void gfx_target_frame(Gfx *g, const GameState *st, const MlTargets *tp) {
  if (!g_tt) gfx_fatal("gfx_target: frame before gfx_target_init");
  // the mode-5 renderTick sequence (main.js; header note)
  rast_clear(&g->rz, 0, 0, 0, (int)GFX_DY, (int)(GFX_DY + 750.0 * GFX_K));
  gfx_render_background(g);
  tdraw_stage_init(g);
  tdraw_stage_fg2(g);
  tdraw_targets(g, tp);
  gfx_render_player_pass(g, st, 0); // renderPlayer(targetBuilder=0)
  gfx_render_articles_pass(g, st);
  gfx_render_vfx(g, st);
  gfx_render_overlay_timer(g, st); // renderOverlay(false): timer only
}

// the finish banner TEXT (finishGame :1425-1460 render arm; Complete! /
// Failure per the :1431 strict equality — text only at 240x240; the
// gradient plane is a rewritten-look surface).
//
// FONT (review-101 round-2 M, iter 103): drawn with the self-authored
// FOH 5x7 font (foh_text), NOT the frozen VFXGLYPHS atlas. Atlas font 0
// (GFX_FONT_T40) carries ONLY digits + ':' (the timer's alphabet), so
// gfx_glyph_text on `COMPLETE!`/`FAILURE` would hit gfx_overlay.c's
// FATAL missing-glyph path and ABORT the first real finish. The FOH
// font is letter-complete; single-colour (no VFX stroke) is a
// registered rewrite-delta (the banner is a rewritten-look surface,
// header above; FOH/menus are NOT checksummed — visual authority is
// Chase's acceptance playthrough).
//
// Split from gfx_target_banner so the banner's glyph coverage is
// host-drivable WITHOUT the scene-frame stack (foh_banner_witness.c via
// check-foh-flows.sh links gfx_target.o + raster.o + foh_font.o and
// dead-strips the unreferenced frame draws). Deps here: foh_text +
// raster only.
void gfx_target_banner_text(Raster *rz, int complete) {
  const RastCol white = { 255, 255, 255, 256 };
  const char *text = complete ? "COMPLETE!" : "FAILURE"; // :1452/:1465
  const int scale = 4; // 5x7 glyph -> 20x28 device px, banner-legible
  const int tw = foh_text_width(text, scale);
  const int th = 7 * scale;
  foh_text(rz, 120 - tw / 2, 120 - th / 2, scale, text, white); // centred
}

// The finish end-banner: redraw the last frame, then overlay the
// COMPLETE!/FAILURE text. Drawn OVER the last frame by the driver once
// the finish hook fired.
void gfx_target_banner(Gfx *g, const GameState *st, const MlTargets *tp,
                       int complete) {
  gfx_target_frame(g, st, tp);
  gfx_target_banner_text(&g->rz, complete);
}
