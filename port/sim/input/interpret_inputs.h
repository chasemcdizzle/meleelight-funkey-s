// interpret_inputs.h <- src/main/main.js:668-860 interpretInputs +
// interpretPause (structure-parallel translation, M2 task 3). The
// god-module's input-cluster mutable globals become MlInputSimState —
// one slice of the flattened game-state struct task 17 assembles (fix_plan
// §M2 conventions: port the logic, never the module graph).
//
// interpretInputs builds the frame's 8-deep buffer from the previous
// frame's buffer: slot 0 = the freshly polled input; z/s history ALWAYS
// shifts by one (main.js:673-676 — "keep updating Z and Start all the
// time, even when paused"); every other field shifts by the pause-aware
// pastOffset (main.js:680-712): 1 while playing normally, 0 when the
// history is frozen (paused without a fresh unpause edge). In the M2
// captured domain (versus mode, never paused) pastOffset is always 1 and
// the pause/frameAdvance edges never fire — translated verbatim anyway,
// with live coverage limited to that path (honest-coverage note,
// docs/AGENT-LOG.md iter 22).
#ifndef ML_INTERPRET_INPUTS_H
#define ML_INTERPRET_INPUTS_H

#include "input.h"

// mType[i]: "keyboard" | GamepadInfo | null (AI). The harness pins every
// active slot to "keyboard" (oracle/meleelight-harness.patch
// harnessSetupMatch); the gamepad arm exists for shape fidelity.
typedef enum {
  ML_MTYPE_NULL = 0,
  ML_MTYPE_KEYBOARD,
  ML_MTYPE_GAMEPAD,
} MlMType;

// The input cluster's slice of main.js module state (initial values are
// the module-load literals; match setup overrides are the caller's job —
// harnessSetupMatch + startGame in the capture domain, task 17 live).
typedef struct {
  double gameMode;                       // main.js:124 (init 20; versus = 3)
  bool playing;                          // main.js:114 (startGame -> true)
  bool frameByFrame;                     // main.js:116
  bool wasFrameByFrame;                  // main.js:117
  bool showDebug;                        // main.js:122 (render plane)
  MlMType mType[4];                      // main.js mType (per-slot)
  double currentPlayers[4];              // controller index per slot
  bool pause[4][2];                      // main.js:165 [[true,true] x4]
  bool frameAdvance[4][2];               // main.js:166 [[true,true] x4]
  double controllerResetCountdowns[4];   // main.js:69 [0,0,0,0]
  bool giveInputs[4];                    // streamclient giveInputs[i] === true
                                         // (multiplayer plane; {} at rest ->
                                         // false for every slot)
  // player[i] debug-display toggles flipped by the d-pad edges
  // (main.js:827-837). Not on the checksum surface; owned by the player
  // object upstream — task 17 wires these into MlPlayer.
  bool showLedgeGrabBox[4], showECB[4], showHitbox[4];
} MlInputSimState;

// Module-load initial values (pre-match; the caller then applies the match
// setup: gameMode = 3, playing = true, mType/currentPlayers per slot).
void ml_input_sim_state_init(MlInputSimState *st);

// main.js:668 interpretInputs(i, active, playertype, inputBuffer) -> the
// new 8-deep buffer in *out. `polled` is the pollInputs seam result for
// this (frame, slot) — see input.h ml_poll_inputs. Mutates *st exactly
// where upstream mutates the module globals.
void ml_interpret_inputs(MlInputSimState *st, int i, bool active,
                         double playertype, const MlInputBuffer *inputBuffer,
                         const MlInput *polled, MlInputBuffer *out);

// main.js mode-3 gameTick end-of-tick input bookkeeping (main.js:1087-1091):
// a frameByFrame tick records wasFrameByFrame, then frameByFrame clears.
// Task 17 owns the full tick; the replay driver applies this at each frame
// boundary to keep the chained state faithful.
void ml_input_end_of_tick(MlInputSimState *st);

// Out-of-captured-domain trap (never a silent stub, HARD RULE 2): branches
// whose upstream behavior is match-lifecycle or another cluster's surface
// (startGame/endGame combo, AI aiInputBank poll) call this instead of
// guessing. The link target defines it: the replay driver exits 3
// (MARSHAL-FAIL class); the future sim wires the real flow (task 17).
void ml_input_out_of_domain(const char *what);

#endif // ML_INTERPRET_INPUTS_H
