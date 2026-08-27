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

#endif // ML_SIM_MODSTATE_H
