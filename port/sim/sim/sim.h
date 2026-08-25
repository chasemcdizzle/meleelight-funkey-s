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
  // NOTE the tick width for matchTimer lives at ML_MATCH_TIMER_TICK below —
  // it is engine data that a non-sim consumer (the HUD) also has to know.
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
//
// A46: this is now the 2-PORT WRAPPER over sim_setup_match_ports below.
// Every pre-A46 caller keeps this signature and this behaviour
// bit-for-bit — check-sim.sh's 8 goldens all run through the wrapper.
void sim_setup_match(GameState *g, int p1, int p2, int p2type,
                     int difficulty, int stageId);

// One port's match-setup slot — harnessSetupMatch's cfg.players[i]
// (oracle/meleelight-harness.patch:76-92, which is FOUR-port by
// construction: `for (var i = 0; i < 4; i++) { var pc = cfg.players[i];
// if (pc) {…} else {…} }`). A46: the engine plane is four ports wide
// (CONTEXT.md "Participant"); only the CALLERS were ever two.
typedef struct {
  int type;       // -1 ABSENT port (the patch's `else` arm); 0 human, 1 cpu
  int character;  // 0 marth 1 puff 2 fox 3 falco 4 falcon (unread when absent)
  int difficulty; // <0 == cfg.players[i].difficulty undefined -> patch:84's 3
} SimPortCfg;

// harnessSetupMatch + startGame over ALL FOUR ports. `ports` is indexed
// by PORT (CONTEXT.md: a player slot 0-3, never a roster index); an
// entry with type == -1 takes the patch's else arm, exactly as slots
// 2/3 unconditionally did before. startGame's own body was ALREADY
// 4-port here (its `for (n = 0; n < 4)` initializePlayers / entrance-vfx
// / stocks loop), so spawn positions and the entrance/start vfx ORDER
// are upstream's, untouched by this widening.
void sim_setup_match_ports(GameState *g, const SimPortCfg ports[4],
                           int stageId);

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

// --- the matchTimer tick, ONE definition -----------------------------------
//
// matchTimerTick subtracts this from `matchTimer` every non-`starting` frame
// (main.js:339, sim_tick.c's matchTimerTick arm). It is upstream's literal,
// carried verbatim and NOT derived from 1/60 — the two differ, and the
// difference is on the checksum surface.
//
// It lives in the header, rather than inline at the single sim site, because
// a NON-sim consumer has to know it: gfx_overlay.c's HUD guard bounds the one
// legitimately-negative matchTimer (the finish frame's) at exactly one tick,
// and a second hand-typed copy of an engine constant is the drift class
// CLAUDE.md HARD RULE 5 exists to forbid (review-r3-r2 Medium). Any change
// here moves the sim and the guard together, and `bash port/sim/check-sim.sh`
// is what proves the sim side did not move by accident.
#define ML_MATCH_TIMER_TICK 0.016667

// --- finishGame seam (punch-list C18) --------------------------------------
//
// Upstream calls finishGame from FIVE sites: matchTimerTick's expiry
// (main.js:338-350) and the four DEAD* KO states at timer===4 when
// isFinalDeath() holds (DEAD{UP,DOWN,LEFT,RIGHT}.js:39). Every golden trace
// stays inside a live match by the quality contract (oracle/goldens/
// manifest.json), so all five were LOUD out-of-domain traps and stay loud:
// this pointer is NULL by default and each site keeps its exact sim_fatal
// message on the NULL arm.
//
// It crosses the SAME link seam as ml_sim_runai_live above: defined in
// sim_tick.c, installed only by the live PLAY driver (foh_dev.c's
// --bridge live arm). No TU on check-sim.sh's frozen list installs it, so
// the M2 EXIT GATE's behavior is preserved bit-for-bit — `SIM CONFORMS` is
// the standing proof. finishGame itself is a BANNER + a 2500 ms hold +
// endGame (main.js:1420-1502); none of that is on the CHECKSUM.md surface,
// and the driver owns all of it, exactly as tp_finish_hook does for targets.
extern void (*ml_sim_finish_hook)(void);

#endif // ML_SIM_H
