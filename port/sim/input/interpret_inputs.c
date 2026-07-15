// interpret_inputs.c <- src/main/main.js:668-860 (structure-parallel
// translation, M2 task 3; re-based on the tagged core in M2 task 16).
// See interpret_inputs.h for the model notes.
//
// M2 task 16 (fix_plan §M2 rule 16 class fix): interpretInputs is now
// implemented ONCE, over the tagged JS-value model, in
// port/sim/input/ai_input.h (`ml_ai_interpret_inputs`) — the AI plane
// writes NUMBER/undefined button values into aiInputBank, so the CPU
// slot's buffer needs tagged values while the human slots' measured
// domain stays plain bool/double. This MlInput entry point is a
// conversion wrapper around that single core: bool/number tags in, the
// same tags out (asserted — the core introduces no number buttons on the
// human path). Behavior re-verified bit-exactly by the unchanged
// check-input-replay.sh over g01/g04/g06 after the rebase.
#include "interpret_inputs.h"

#include "ai_input.h"

void ml_input_sim_state_init(MlInputSimState *st) {
  st->gameMode = 20;              // main.js:124
  st->playing = false;            // main.js:114
  st->frameByFrame = false;       // main.js:116
  st->wasFrameByFrame = false;    // main.js:117
  st->showDebug = false;          // main.js:122
  for (int i = 0; i < 4; i++) {
    st->mType[i] = ML_MTYPE_NULL;
    st->currentPlayers[i] = -1;
    st->pause[i][0] = true;       // main.js:165
    st->pause[i][1] = true;
    st->frameAdvance[i][0] = true; // main.js:166
    st->frameAdvance[i][1] = true;
    st->controllerResetCountdowns[i] = 0; // main.js:69
    st->giveInputs[i] = false;    // streamclient giveInputs = {} at rest
    st->showLedgeGrabBox[i] = false;
    st->showECB[i] = false;
    st->showHitbox[i] = false;
  }
}

// main.js:847 interpretPause(pause0, pause1)
void ml_interpret_pause(MlInputSimState *st, bool pause0, bool pause1) {
  if (pause0 && !pause1) {
    if (st->gameMode == 3 || st->gameMode == 5) {
      st->playing ^= true;
      if (!st->playing) {
        // sounds.pause.play(); changeVolume(MusicManager, ...);
        // renderForeground(); — audio/render plane (sound-event queue seam
        // arrives with task 4; nothing here is on the checksum surface)
      } else {
        // changeVolume(MusicManager, masterVolume[1], 1); — audio plane
      }
    }
  }
}

void ml_interpret_inputs(MlInputSimState *st, int i, bool active,
                         double playertype, const MlInputBuffer *inputBuffer,
                         const MlInput *polled, MlInputBuffer *out) {
  // main.js:678's poll seam, plain-input domain: playertype 1 in versus
  // reads aiInputBank — that path carries tagged values and lives on the
  // tagged core directly (ai_bridge.h chain / task 17); through THIS
  // entry point it is out of domain (never a silent guess).
  if (playertype == 1 && st->gameMode == 3) {
    ml_input_out_of_domain("pollInputs AI path (aiInputBank) via the plain-MlInput entry point");
  }
  const MlInput p0 = ml_poll_inputs(polled);

  MlAiInputBuffer tin, tout;
  for (int k = 0; k < 8; k++) tin.slot[k] = ai_from_ml(&inputBuffer->slot[k]);
  const MlAiInput tpolled = ai_from_ml(&p0);

  ml_ai_interpret_inputs(st, i, active, playertype, &tin, &tpolled, &tout);

  for (int k = 0; k < 8; k++) out->slot[k] = ml_from_ai(&tout.slot[k]);
}

// main.js:1087-1091 (mode-3 gameTick tail; task 17 owns the full tick)
void ml_input_end_of_tick(MlInputSimState *st) {
  if (st->frameByFrame) {
    // frameByFrameRender = true; — render plane
    st->wasFrameByFrame = true;
  }
  st->frameByFrame = false;
}
