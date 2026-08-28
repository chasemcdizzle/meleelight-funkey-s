// port/sim/sim/sim_modstate.h — ticket #28.
//
// THE SIM STATE THAT IS NOT IN GameState.
//
// GameState (sim.h) is the flattened god-module struct and it is where the
// match lives — but it is not ALL of it. A handful of the translated
// modules keep upstream module-private state in a file-scope `static`,
// because that is what upstream keeps it in (physics.js's module `let`s,
// a move object's own scalar property, howler's page-global counter). Those
// bytes are sim state in every sense that matters to a snapshot: lose them
// and the match resumes into a different game.
//
// This header is the ONE place their accessors are declared, and it is
// deliberately dependency-free (<stddef.h> only) so that an owner TU can
// include it without inheriting sim.h's generated-table include path. Three
// of the owners (ml_events.c, sim_data.c, sim_tick.c) are also built by rigs
// that never see pipeline/build/sim-tables, and a heavier include here would
// break them.
//
// THE LEDGER. Which module statics are persisted, which are reconstructed
// and which are transient is written down ONCE, in
// port/sim/sim/sim-modstate.frozen.txt, and check-sim-snapshot.sh's leg [2]
// re-derives the list from the sources and diffs it. Adding a mutable
// file-scope static to a sim TU without classifying it there fails that
// check — the module-state twin of the _Static_assert that guards
// GameState. The reasoning for each row lives in the frozen file.
#ifndef ML_SIM_MODSTATE_H
#define ML_SIM_MODSTATE_H

#include <stddef.h>

// --- sim_data.c: the rule-17 LIVE charHitboxes plane -------------------------
// {char -> moveKey -> id[4] {dmg,size}} plus the per-(player,id) provenance
// that says which plane entry a player's hitbox slot aliases. Upstream this
// is the GLOBAL chars data object, which move code WRITES through stale id
// aliases (puff's rollout dmg, sing's id[0].size, marth's charge dmg) —
// measured live drift, CLAUDE.md M2 task 12 rule 17. Not derivable from
// CTAB1 after a match has started, so it is persisted.
size_t sim_chd_snap_bytes(void);
void sim_chd_snap_save(void *dst);
void sim_chd_snap_load(const void *src);

// --- sim_tick.c: mv_howl_play_id's monotone counter --------------------------
// marth's `player.shieldBreakerID = sounds.shieldbreakercharge.play()`
// consumes howler's page-global id counter. The id is stored IN the player
// (so it rides in GameState) and is read back by `.stop(id)`; the counter
// that mints the next one is module state and must continue where it left
// off, or a resumed match hands out an id a live voice already holds.
double sim_tick_howl_counter_get(void);
void sim_tick_howl_counter_set(double v);

// --- ml_events.c: the play-event count behind ml_howl_play_id ----------------
// The M4-task-6 successor seam to the counter above (ml_events.h): the id is
// derived from the sim's OWN count of play events, so the count is the state.
unsigned long long ml_events_play_count_get(void);
void ml_events_play_count_set(unsigned long long v);

// --- characters/falcon/moves/SIDESPECIALGROUND.c -----------------------------
// `this.canEdgeCancel` — a SCALAR write onto the move object itself, which is
// module state and not player state (CLAUDE.md M2 task 10). Its accessors
// ALREADY EXISTED for the integration seam and are declared where they
// belong, in port/sim/characters/falcon/moves.h — nothing is redeclared
// here. It is named in this comment so every owner is findable from one
// place; sim_snapshot.c wraps those two functions as its module row.

// --- sim/sim_ai_live.c: the LIVE C AI's god-module slice ---------------------
// TICKET #29. The live-AI TUs (port/sim/ai.c, port/sim/sim/sim_ai_live.c) are
// NOT on the M2 EXIT GATE's frozen TU list, so ticket #28's ledger never saw
// them — but the FOH binary that plays a real match links both
// (port/foh/check-device-foh.sh's SIM_TUS), and a resumed CPU match that lost
// their state would resume into a different opponent. Classified by reading
// every write site:
//
//   g_stage   TRANSIENT   — ai_stage_refresh() rebuilds the whole MlAiStage
//                           view from STAB1 + the live stage plane at the top
//                           of EVERY runai_live call, before ml_runAI reads
//                           it. Nothing crosses a frame boundary.
//   g_ai_bound RECONSTRUCTED — the bind-once flag for the POINTER fields
//                           (player[k], bank, aS). Those are addresses inside
//                           this process's GameState, so a restored one would
//                           be a trap; a fresh process rebinds them on its
//                           first call, and ss_load's module row clears the
//                           flag so a same-process restore rebinds too.
//   g_ai      SPLIT        — runai_live REPOPULATES player/bank/aS/playerType/
//                           cS/multiJump/multiJumpUndef/turbo on every call,
//                           so all of those are reconstructed. What it does
//                           NOT repopulate is `hasCurentAction[4]` and
//                           `curentAction[4][]` — ai.js:1254's upstream typo
//                           field, which the port models as slice state — and
//                           THAT is the persisted part. It is carried rather
//                           than dropped even though no upstream reader for it
//                           has been found: "no reader today" is a claim about
//                           upstream that a later cluster could falsify, and
//                           the row costs 4 * (1 + ML_STR_CAP) bytes.
//
// THE LINK SEAM. sim_snapshot.c is built by rigs that do NOT link ai.c (the
// gate's TU list plus sim_snapshot.c — check-sim-snapshot.sh leg [1]), so it
// cannot name a symbol from sim_ai_live.c. These three pointers are the same
// device as ml_sim_runai_live itself: DEFINED (NULL) in sim_tick.c, which is
// on every TU list, and installed by sim_ai_live.c's existing constructor.
// NULL means the row contributes ZERO bytes, so a build without the live AI
// has a different payload total and therefore a different build identity —
// which is correct: a snapshot from the FOH binary is not loadable by the
// bridge-fed one and must say so rather than silently short-read.
extern size_t (*ml_ai_live_snap_bytes)(void);
extern void (*ml_ai_live_snap_save)(void *dst);
extern void (*ml_ai_live_snap_load)(const void *src);

// NOT module state, named here so the next reader does not have to re-derive
// it: ai.c's `ml_ai_cov[]` is a non-static GLOBAL arm-coverage counter array
// (ai.h:82), written by AI_COV() and read only by --ai-cover's stderr dump.
// It is diagnostic instrumentation, never an input to sim logic, so it is
// outside the snapshot by the same rule that keeps frame timings out.

#endif // ML_SIM_MODSTATE_H
