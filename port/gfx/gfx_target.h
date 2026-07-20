// port/gfx/gfx_target.h — the target-mode compositor surface (M4 task
// 12). See gfx_target.c for the structure-parallel notes (mode-5
// renderTick sequence over TTAB1 stages).
#ifndef GFX_GFX_TARGET_H
#define GFX_GFX_TARGET_H

#include "gfx.h"
#include "../sim/target/target_play.h" // MlTargets

// Bind the active TARGET stage (TTAB1 row + camera) + boot state; the
// gfx_init twin for gameMode 5 (g->stab stays NULL — never consulted).
void gfx_target_init(Gfx *g, int tstageId, int backgroundType);

// Render one target-mode frame into g->rz (clear -> background ->
// stage -> targets -> player 0 -> articles -> vfx -> timer overlay).
void gfx_target_frame(Gfx *g, const GameState *st, const MlTargets *tp);

// The finish end-banner frame (COMPLETE!/FAILURE over the last state;
// finishGame :1425-1460 render arm) — the driver calls it after the
// tp_finish_hook fired.
void gfx_target_banner(Gfx *g, const GameState *st, const MlTargets *tp,
                       int complete);

#endif // GFX_GFX_TARGET_H
