// port/gfx/gfx_target.h — the target-mode compositor surface (M4 task
// 12). See gfx_target.c for the structure-parallel notes (mode-5
// renderTick sequence over TTAB1 stages).
#ifndef GFX_GFX_TARGET_H
#define GFX_GFX_TARGET_H

#include "gfx.h"
#include "../sim/target/target_play.h" // MlTargets
#include "../sim/target/custom_stage.h" // MlkStage (A45 T2)

// Bind the active TARGET stage (TTAB1 row + camera) + boot state; the
// gfx_init twin for gameMode 5 (g->stab stays NULL — never consulted).
void gfx_target_init(Gfx *g, int tstageId, int backgroundType);

// A45 T2 (D42): the same, for a stage that has no TTAB1 id — a custom
// stage decoded from a .mlstage share code. Materialises a runtime TTAB1
// row and binds it, so every draw path stays identical to the authored
// one. Dies loudly if the stage is not mlk_stage_playable.
void gfx_target_init_custom(Gfx *g, const MlkStage *cs, int backgroundType);

// Render one target-mode frame into g->rz (clear -> background ->
// stage -> targets -> player 0 -> articles -> vfx -> timer overlay).
void gfx_target_frame(Gfx *g, const GameState *st, const MlTargets *tp);

// The finish end-banner TEXT (COMPLETE!/FAILURE) via the self-authored
// FOH 5x7 font (foh_text) — atlas font 0 lacks letters, so the atlas
// path would FATAL on the first real finish (review-101 round-2 M, iter
// 103). Split out so the banner glyph coverage is host-drivable without
// the scene stack (foh_banner_witness.c). Deps: foh_text + raster only.
void gfx_target_banner_text(Raster *rz, int complete);

// The finish end-banner frame (COMPLETE!/FAILURE over the last state;
// finishGame :1425-1460 render arm) — the driver calls it after the
// tp_finish_hook fired. = gfx_target_frame + gfx_target_banner_text.
void gfx_target_banner(Gfx *g, const GameState *st, const MlTargets *tp,
                       int complete);

#endif // GFX_GFX_TARGET_H
