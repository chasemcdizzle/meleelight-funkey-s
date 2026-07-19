// port/sim/sim/sim.h — the integrated headless sim (M2 task 17).
//
// This is the composition layer: the sixteen bit-verified module clusters
// (fix_plan §M2 tasks 1-16) assembled into upstream main.js's match
// lifecycle + mode-3 gameTick, replaying a golden trace end-to-end and
// emitting the CHECKSUM.md stream. Everything here is either a verbatim
// translation of main.js's tick/lifecycle code (cited per function) or
// host wiring between already-verified modules.
//
// STATE: GameState is the flattened god-module struct (fix_plan §M2
// conventions — port the logic, never the module graph): MlSim (task 5's
// players/stage/settings slice) + HdQueues (task 6) + MlArticles (task
// 13) + MlInputSimState/input chains (tasks 3/16) + the moving-platform
// plane (task 14) + the match-lifecycle globals (starting/startTimer/
// matchTimer/playing — main.js module lets).
//
// SEAMS: every `extern` the module clusters declare is implemented by the
// host (sim_tick.c / sim_data.c) with the REAL cross-module call the
// upstream import graph makes — mlp_dispatch -> mv_dispatch, mlp_hd_* ->
// hit_detection.c bodies, mv_article_* -> article.c inits (the task-13
// documented swap), runAI -> the task-16 AI bridge artifact. Traps
// (mv_out_of_domain etc.) abort loudly: HARD RULE 2, no silent stubs.
#ifndef ML_SIM_H
#define ML_SIM_H

#include "../article.h"            // MlArticles (includes hit_detection.h -> physics.h)
#include "../ai_bridge.h"          // MlAiBridge (includes input/ai_input.h)
#include "../input/interpret_inputs.h"
#include "../input/input.h"
#include "../stages/moving_platforms.h"
#include "ml_stages.h"             // STAB1 (generated; -I pipeline build dir)

// --- the flattened game state ------------------------------------------------

typedef struct {
  // task-5 slice: player[4] + stage + settings + physics module state
  MlSim sim;
  // task-6 slice: hitQueue + phantomQueue
  HdQueues hq;
  // task-13 slice: aArticles + destroyArticleQueue + articleHitQueue
  MlArticles arts;
  // task-3 slice: the input cluster's main.js globals
  MlInputSimState inp;
  // input chains (main.js gameTick local `input` becomes oldInputBuffers):
  MlInputBuffer prevBuf[4];      // human slots (and starting-window nulls)
  MlAiInputBuffer prevBufAi[4];  // CPU slots (task 16's tagged chain)
  // aiInputBank[4][8] (input.js:118 aiPlayerN = 8x inputData()): row 0 is
  // the chained AI row; rows 1..7 are page-boot inputData() values —
  // NEVER written upstream (measured: ai.js's only non-[0] access is the
  // READ `aiInputBank[i][1].a`, ai.js:357) — value-identical to
  // ai_null_input(). Widened from [4] single rows for the M4 task-5 live
  // C AI (ml_runAI's MlAiSim.bank wants the full 4x8 plane).
  MlAiInput bank[4][8];
  bool slotIsAi[4];
  // per-tick buffers (gameTick's `input`), rebuilt every frame:
  MlInputBuffer curBuf[4];       // plain projection every consumer reads
  MlAiInputBuffer curBufAi[4];   // tagged truth for AI slots
  // task-14 slice: fountain's module-private platformStates + the
  // page-start player slices for INACTIVE slots (rider loop reads them)
  MpPlatformState ps[2];
  MpPlayerSlice inactiveMp[4];   // used where sim.playerPresent[i] is false
  // match lifecycle (main.js module lets)
  bool starting;                 // main.js:195
  double startTimer;             // main.js:199
  double matchTimer;             // main.js:207
  double stageSelect;            // main.js:187
  MpStageKind stageKind;
  double cpuDifficulty[4];       // main.js:109 [3,3,3,3]
  // RNG (oracle/CHECKSUM.md §6)
  MlRng rng;
  uint32_t rngStateAtReset;      // counters reset before startGame
  uint32_t rngStateAtFrame1;     // rngCallsOutsideStep boundary
  // AI bridge (task 16), loaded for CPU goldens
  MlAiBridge bridge;
  bool hasBridge;
  long frame;                    // 1-based current frame being simulated
} GameState;

extern GameState G; // one match per process (upstream: one page per run)

// --- boot (sim_boot.c) ---------------------------------------------------------

// Build MlStageX (physics' activeStage read set) from the STAB1 tables.
void sim_stage_from_stab1(int stageId, MlStageX *out);

// main.js start()'s player plane: buildPlayerObject(i) for all 4 slots
// with page-boot characterSelections [0,0,0,0], then face=1 and
// actionState="WAIT" (main.js:1546-1550). player.js constructors
// translated verbatim (playerObject/physicsObject/createHitboxes).
void sim_boot_page(GameState *g);

// oracle/meleelight-harness.patch harnessSetupMatch + upstream startGame
// (main.js:1320-1368): playerType/mType/currentPlayers/characterSelections/
// cpuDifficulty/stageSelect, then setVsStage, the ONE seeded background
// draw, changeGamemode(3), buildPlayerObject for active slots (ECB1/ECBp
// = Vec2D(undefined,undefined)x4 — the startingPoint-array .x/.y quirk),
// matchTimer=480, startTimer=1.5, starting=true, playing=true.
// p2type: 0 human, 1 cpu.
void sim_setup_match(GameState *g, int p1, int p2, int p2type,
                     int difficulty, int stageId);

// upstream buildPlayerObject(i) (main.js:1296-1301).
void sim_build_player(GameState *g, int i);

// --- data plane (sim_data.c) ---------------------------------------------------

// Load the SIMDATA1 artifact (port/sim/calib/dump-sim-data.js): the
// executed move-data plane (rule 15) + the asFlags/hdFlags tables the
// physics/hitdet clusters branch on. Aborts loudly on any violation.
void sim_data_load(const char *path);

// Build the actionStates registries for all five chars from the loaded
// name/shared-origin maps (as_setupActionStates; non-shared entries are
// the table char's module defs — puff's FURAFURA/JUMPAERIALB/JUMPAERIALF
// overrides fall out of the measured origin maps), register the per-char
// module indexes (marth 0, puff 1 — checkForIASA's arms) and the composed
// special-phase lookup (falcon/puff/marth; (state,phase) pairs are
// disjoint across chars, and flag tables gate which entries dispatch).
void sim_data_register(void);

// rule-17 live charHitboxes plane: reset to pristine CTAB1 + clear
// provenance (called at match setup).
void sim_chd_reset(void);

// --- per-frame serialization (sim_ser.c) ----------------------------------------

// The CHECKSUM.md §2/§3 frame envelope from the live state: active
// players' 7 allow-listed fields in fixed literal order + the article
// queue, serialized with the task-15 ser (ml_fmt/ml_ser via the canon
// bridge), SHA-256 lowercase hex into out_hex[65].
void sim_frame_hash(const GameState *g, char out_hex[65]);

// Diagnostic: the envelope bytes of the LAST sim_frame_hash call (the
// M2CAL localization instrument — byte-diff vs the oracle harness's
// --capture-frames dump of the same frame).
const char *sim_frame_envelope(size_t *len);

// --- the tick (sim_tick.c) -------------------------------------------------------

// upstream gameTick(oldInputBuffers), mode-3 `playing` branch
// (main.js:1050-1092) + the harness frame-boundary semantics
// (oracle/CHECKSUM.md §5): inputs injected per trace, one tick, hash after.
// `traceRow[i]` = the injected pollInputs result for human slot i (NULL
// entry = inactive slot).
void sim_game_tick(GameState *g, const MlInput *traceRow[4]);

// Fatal host abort (all *_out_of_domain / *_fail seams route here).
void sim_fatal(const char *what) __attribute__((noreturn));

// --- live-AI seam (M4 task 5) ---------------------------------------------------
//
// LINK-SEAM CONSTRAINT: port/sim/check-sim.sh (the M2 EXIT GATE — never
// edited, HARD RULE 3) builds the sim WITHOUT port/sim/ai.c, so no TU on
// its frozen list may reference ml_runAI directly. The live arm therefore
// crosses a POINTER seam: NULL by default (defined in sim_tick.c);
// port/sim/sim/sim_ai_live.c (linked ONLY by builds that also link ai.c,
// e.g. check-ai-live.sh's sim_host_live) installs the real driver via a
// constructor — which runs before main, and the pointers live OUTSIDE
// GameState so sim_boot_page's memset cannot wipe them. In the M2-gate
// build the pointer stays NULL and --cpu without --ai-bridge errors
// exactly as before: the frozen gate's behavior is preserved bit-for-bit.
extern void (*ml_sim_runai_live)(GameState *g, int i);
// Optional diagnostic twin: dump the ml_ai_cov arm table (sim_main
// --ai-cover; stderr; never gating).
extern void (*ml_sim_ai_cov_dump)(void);

#endif // ML_SIM_H
