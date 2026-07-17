// port/gfx/gfx.h — the scene compositor (M3 task 3): stage + players +
// articles composited into ONE 240x240 RGB565 buffer from live sim state.
//
// Structure-parallel translation of the browser's per-frame gameMode-3
// render sequence (src/main/main.js renderTick "playing" branch):
//   clearScreen -> drawStage -> renderPlayer(i in active slots) ->
//   renderArticles
// MINUS drawBackground / renderVfx / renderOverlay (out of scope this
// task — vfx placement data does not exist at the C seam (ml_events
// carries names only; registered follow-up), HUD/banner/backgrounds are
// M4 front-of-house). The browser reference capture
// (port/gfx/capture-canvas.js) executes exactly the same reduced
// sequence, so the silhouette IoU pairing is honest on both sides.
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
  // exact upstream sites: drawStage=4, miniView bubble 6 then 1,
  // drawLaserLine=2.
  double fg2LineWidth;
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

// Dump the buffer as binary PPM (P6, 240x240, 565->888 by bit
// replication-free shift like rastbench dump_ppm) and the ink plane as
// binary PGM (P5, 0/255).
void gfx_dump_ppm(const Gfx *g, const char *path);
void gfx_dump_ink_pgm(const Gfx *g, const char *path);

#endif // GFX_GFX_H
