// port/gfx/gfx.h — the scene compositor (M3 task 3): stage + players +
// articles composited into ONE 240x240 RGB565 buffer from live sim state.
//
// Structure-parallel translation of the browser's per-frame gameMode-3
// render sequence (src/main/main.js renderTick "playing" branch), FULL
// as of M4 task 2:
//   clearScreen -> drawBackground (ink-suppressed; not in the IoU mask
//   on either side) -> drawStage -> renderPlayer(i in active slots) ->
//   renderArticles -> renderVfx() -> renderOverlay(true)
// The vfx/overlay/banner planes live in gfx_vfx.c / gfx_overlay.c /
// gfx_bg.c (see gfx_vfx.h). The browser reference capture
// (port/gfx/capture-canvas.js) executes the SAME sequence INCLUDING
// drawBackground — U1 added the background legs, and the canonical render
// of each frame advances the mirrored bg stream (only the extra det/loo
// replays pass bg:false, so the stream advances exactly once per game
// frame) — and masks fg1|fg2|UI. The bg pass draws to a DIFFERENT canvas
// layer than the judged mask, so including it does not change the fg
// pairing; it is what keeps the background legs and the C's bg stream in
// step. The silhouette IoU pairing is honest on both sides.
//
// CAMERA (verbatim, measured): upstream has NO dynamic camera/zoom —
// the world->canvas transform is the static per-stage
//   canvasX = pos.x *  stage.scale + stage.offset[0]
//   canvasY = pos.y * -stage.scale + stage.offset[1]
// into the 1200x750 logical canvas (STAB1 scale/offset; doubles).
// Retarget to the 240x240 device buffer: uniform GFX_K = 0.2
// (= 240/1200, aspect preserved) with vertical letterbox GFX_DY = 45
// (= (240 - 750*0.2)/2); rows outside [45, 195) stay background (the
// rasterizer clips there). Rasterization itself is NOT checksummed and
// uses floats (PLAN §5); everything up to the retarget stays doubles.
//
// The renderer takes `const GameState *` — it structurally cannot write
// sim state; the check additionally proves non-perturbation by
// verify-streaming the render-on replay.
#ifndef GFX_GFX_H
#define GFX_GFX_H

#include "../sim/sim/sim.h"
#include "anim1.h"
#include "raster.h"

#define GFX_K 0.2
#define GFX_DY 45.0
// GFX_LEGIBLE_MIN_DEV_PX (M4 task 3): DELIBERATE, DOCUMENTED device-scale
// adaptation — minimum on-screen stroke width, in DEVICE pixels, for ALL
// stage-surface strokes (ground/ceiling/platforms/walls/ledge ticks/
// moving platforms) when Gfx.legibility is set. Chase's S1-ratification
// playtest (AGENT-LOG 2026-07-17): upstream-faithful lineWidth 1 canvas
// px * GFX_K = 0.2 device px ≈ 0.03 mm of AA-faded coverage on the
// 1.54" 240x240 panel (~6.2 px/mm) — illegible. 2.0 device px ≈ 0.32 mm
// — legible without occluding gameplay. Rasterization is NOT checksummed
// and the adaptation is OFF by default (legibility=0 keeps every path
// byte-identical to the upstream-faithful render — the browser IoU
// reference is compared ONLY against legibility-off renders); the device
// path (--legible on gfx_app) turns it on on BOTH ends of its
// host<->device shot bit-compare. Value frozen here + twin-pinned by
// check-device-render.sh; visual authority = Chase's M4 acceptance
// playthrough (fix_plan §M4 task 3, AGENT-LOG iter 73).
#define GFX_LEGIBLE_MIN_DEV_PX 2.0
#define GFX_CHARS 5
#define GFX_PALETTES 7
#define GFX_PALETTE_COLS 5
#define GFX_REV_CAP 64

typedef struct { uint8_t r, g, b; } GfxRgb;

// Render data plane (EXECUTED out of the upstream page by
// capture-canvas.js --gfxdata; never hand-typed): full palette table
// (SIMDATA1's palettes0 only carries column 0; the renderer needs the
// flash/shield/furafura/shieldstun columns too), pPal, the render-only
// gameSettings.flashOnLCancel, and the per-(char,state) reverseModel
// flags (render-only — not in the sim's asFlags dump).
typedef struct {
  GfxRgb palettes[GFX_PALETTES][GFX_PALETTE_COLS];
  int pPal[4];
  int flashOnLCancel; // gameSettings.flashOnLCancel (0/1)
  int revCount[GFX_CHARS];
  char revStates[GFX_CHARS][GFX_REV_CAP][ML_STR_CAP];
} GfxData;

typedef struct {
  GfxData data;
  Anim1 anim[GFX_CHARS];
  int animLoaded[GFX_CHARS];
  // camera (doubles; STAB1 per active stage)
  double scale;
  double offx, offy;
  // startGame's seeded background draw decides boxFill
  // (stagerender.js drawBackgroundInit): 0 -> rgba(0,0,0,0.1),
  // 1 -> rgba(94,173,255,0.3)
  int backgroundType;
  // canvas-state emulation: fg2.lineWidth persists ACROSS frames
  // (canvas 2d contexts are never reset upstream). Assigned at the
  // exact upstream sites that LEAK the width: drawStage=4, miniView
  // bubble 6 then 1.
  // drawLaserLine's `fg2.lineWidth = 2` is deliberately NOT modelled
  // (review-134 r3 M1): article.js LASER.draw wraps its whole body in
  // fg2.save()/fg2.restore(), so that assignment is DISCARDED when
  // draw() returns and never reaches the next consumer. Writing it here
  // leaked a width upstream never leaks (into the moving-platform stroke
  // and the player temp quad) — see the long note at the
  // laser_pass sites in gfx_render.c, and the per-frame invariant
  // gfx_render_frame asserts.
  double fg2LineWidth;
  // stage-surface legibility adaptation (M4 task 3): 0 = upstream-
  // faithful widths (default; every host IoU path), 1 = clamp stage
  // strokes to GFX_LEGIBLE_MIN_DEV_PX device px (the device path).
  int legibility;
  const ml_stage_t *stab; // active STAB1 stage row
  Raster rz;
} Gfx;

// Load the GFXDATA1 artifact (strict grammar, loud death).
void gfx_data_load(GfxData *d, const char *path);

// Load anim_<charId>_<name>.bin for one character from `dir`.
void gfx_load_anim(Gfx *g, const char *dir, int charId);

// Bind the active stage (STAB1 row + camera constants) + boot state.
void gfx_init(Gfx *g, int stageId, int backgroundType);

// Render one frame of `st` into g->rz (fb + ink plane).
void gfx_render_frame(Gfx *g, const GameState *st);

// U3: the ARTICLE-ONLY ink plane (browser-parity check only; NULL =
// disarmed, which is every shipping build). Armed, gfx_render_frame
// isolates the renderArticles pass — ink plane saved, cleared, articles
// drawn, `fn` called with g->rz.ink holding exactly the article ink, then
// the saved plane OR'd back — so the fg plane every later pass and every
// other dump sees is bit-identical to the disarmed path.
// Same shape as gfx_bg_star_sink, and for the same reason: an aggregate
// silhouette cannot see a feature that is entirely missing from it (U1's
// starfield scored 0.9927 with every star deleted; articles scored
// 0.9143 against a 0.88 bound with renderArticles stubbed out — MEASURED,
// iter 134). The isolation scratch (57,600 B) is allocated HERE when a
// non-NULL sink is installed, so a shipping link carries TWO null
// pointers (the sink `g_art_sink` and the scratch `g_art_pre`) and no
// buffer at all (review-134 r1 M2) — it used to carry 57,600 B of BSS.
void gfx_render_article_sink(void (*fn)(Gfx *));

// The plane saved by that isolation, valid only inside the sink callback
// (review-134 indep-2 M1). It is what makes the C-side containment check
// contingent: `article ink subset of final fg ink` is true for any renderer
// (ink is set-only and is not cleared after the article pass), whereas
// `saved plane subset of final fg ink` holds only if the OR-back actually
// restored it. NULL when the sink is not armed.
const uint8_t *gfx_render_article_pre(void);

// Dump the buffer as binary PPM (P6, 240x240, 565->888 by bit
// replication-free shift like rastbench dump_ppm) and the ink plane as
// binary PGM (P5, 0/255).
void gfx_dump_ppm(const Gfx *g, const char *path);
void gfx_dump_ink_pgm(const Gfx *g, const char *path);
// Same P5 grammar over an arbitrary RAST_W*RAST_H ink plane (the live-plane
// dump above is this with plane == g->rz.ink).
void gfx_dump_plane_pgm(const uint8_t *plane, const char *path);

// Per-pass render-profiler dump (attribution instrument, M4 task 3):
// prints avg ns/frame per pass to stderr in -DMLFK_RENDER_PROF builds;
// EMPTY in default builds (compile-time gated — no hot-path cost).
void gfx_render_prof_dump(void);

#endif // GFX_GFX_H
