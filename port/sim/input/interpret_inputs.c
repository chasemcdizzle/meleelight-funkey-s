// interpret_inputs.c <- src/main/main.js:668-860 (structure-parallel
// translation, M2 task 3). See interpret_inputs.h for the model notes.
// Every render/audio/DOM statement is carried as a documented no-op
// comment at its exact upstream position (drawECB precedent, M2-CAL);
// value-plane behavior is verbatim.
#include "interpret_inputs.h"

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
static void interpretPause(MlInputSimState *st, bool pause0, bool pause1) {
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
  // main.js:670 let tempBuffer = nullInputs();
  MlInputBuffer *tempBuffer = out;
  nullInputs(tempBuffer);

  // main.js:672-676 — keep updating Z and Start all the time, even when
  // paused
  for (int k = 0; k < 7; k++) {
    tempBuffer->slot[7 - k].z = inputBuffer->slot[6 - k].z;
    tempBuffer->slot[7 - k].s = inputBuffer->slot[6 - k].s;
  }

  // main.js:678 tempBuffer[0] = pollInputs(gameMode, frameByFrame,
  // mType[i], i, currentPlayers[i], keys, playertype);
  // playertype flows into the poll seam's branch structure; in the captured
  // domain (human slots) it selects the injected-input path (input.h).
  if (playertype == 1 && st->gameMode == 3) {
    // aiInputBank[playerSlot][0] — task 16's AI-input bridge surface
    ml_input_out_of_domain("pollInputs AI path (aiInputBank)");
  }
  tempBuffer->slot[0] = ml_poll_inputs(polled);

  // main.js:680-685
  double pastOffset = 0;
  if ((st->gameMode != 3 && st->gameMode != 5) ||
      (st->playing && (st->pause[i][1] || !st->pause[i][0])) ||
      st->wasFrameByFrame ||
      (!st->playing && st->pause[i][0] && !st->pause[i][1])) {
    pastOffset = 1;
  }

  // main.js:687-689
  st->pause[i][1] = st->pause[i][0];
  st->wasFrameByFrame = false;
  st->frameAdvance[i][1] = st->frameAdvance[i][0];

  // main.js:691-712 — every field except z/s shifts by pastOffset
  // (pastOffset is 0 or 1, so the JS index arithmetic is exact in int)
  const int off = (int)pastOffset;
  for (int k = 0; k < 7; k++) {
    const MlInput *src = &inputBuffer->slot[7 - k - off];
    MlInput *dst = &tempBuffer->slot[7 - k];
    dst->lsX = src->lsX;
    dst->lsY = src->lsY;
    dst->rawX = src->rawX;
    dst->rawY = src->rawY;
    dst->csX = src->csX;
    dst->csY = src->csY;
    dst->rawcsX = src->rawcsX;
    dst->rawcsY = src->rawcsY;
    dst->lA = src->lA;
    dst->rA = src->rA;
    dst->a = src->a;
    dst->b = src->b;
    dst->x = src->x;
    dst->y = src->y;
    dst->r = src->r;
    dst->l = src->l;
    dst->dl = src->dl;
    dst->dd = src->dd;
    dst->dr = src->dr;
    dst->du = src->du;
  }

  // main.js:714-723 — `if (mType !== null)`: mType is the module-global
  // ARRAY, never null; conditional shape carried verbatim
  if (true) {
    if ((st->mType[i] == ML_MTYPE_KEYBOARD &&
         (tempBuffer->slot[0].z || tempBuffer->slot[1].z)) ||
        (st->mType[i] != ML_MTYPE_KEYBOARD &&
         (tempBuffer->slot[0].z && !tempBuffer->slot[1].z))) {
      st->frameAdvance[i][0] = true;
    } else {
      st->frameAdvance[i][0] = false;
    }
  }

  // main.js:725-727
  if (st->frameAdvance[i][0] && !st->frameAdvance[i][1] && !st->playing &&
      st->gameMode != 4) {
    st->frameByFrame = true;
  }

  if (st->mType[i] == ML_MTYPE_KEYBOARD) { // main.js:729 keyboard controls
    // main.js:731-736
    if (tempBuffer->slot[0].s || tempBuffer->slot[1].s ||
        (st->gameMode == 5 &&
         (tempBuffer->slot[0].du || tempBuffer->slot[1].du))) {
      st->pause[i][0] = true;
    } else {
      st->pause[i][0] = false;
    }

    // main.js:738-747 — A+L+R+Start (+B) while not playing: startGame /
    // endGame. Match-lifecycle flow, out of the M2 captured domain (the
    // goldens' quality contract keeps every trace mid-match).
    if (!st->playing && (st->gameMode == 3 || st->gameMode == 5) &&
        (tempBuffer->slot[0].a || tempBuffer->slot[1].a) &&
        (tempBuffer->slot[0].l || tempBuffer->slot[1].l) &&
        (tempBuffer->slot[0].r || tempBuffer->slot[1].r) &&
        (tempBuffer->slot[0].s || tempBuffer->slot[1].s)) {
      ml_input_out_of_domain("keyboard startGame/endGame combo");
    }

    interpretPause(st, st->pause[i][0], st->pause[i][1]); // main.js:749
  } else if (st->mType[i] != ML_MTYPE_NULL) { // main.js:751 gamepad controls
    // main.js:753-762
    if (!st->playing && (st->gameMode == 3 || st->gameMode == 5) &&
        (tempBuffer->slot[0].a && tempBuffer->slot[0].l &&
         tempBuffer->slot[0].r && tempBuffer->slot[0].s) &&
        !(tempBuffer->slot[1].a && tempBuffer->slot[1].l &&
          tempBuffer->slot[1].r && tempBuffer->slot[1].s)) {
      ml_input_out_of_domain("gamepad startGame/endGame combo");
    }

    // main.js:764-769 — verbatim precedence: s || (du && gameMode == 5)
    if (tempBuffer->slot[0].s ||
        (tempBuffer->slot[0].du && st->gameMode == 5)) {
      st->pause[i][0] = true;
    } else {
      st->pause[i][0] = false;
    }

    // main.js:771-783 — controller reset countdown
    if ((tempBuffer->slot[0].z || tempBuffer->slot[0].du) &&
        tempBuffer->slot[0].x && tempBuffer->slot[0].y) {
      st->controllerResetCountdowns[i] -= 1;
      if (st->controllerResetCountdowns[i] == 0) {
        // triggers code in input.js (setCustomCenters on the next gamepad
        // poll) + console.log + jQuery fade — browser-I/O plane
      }
    } else {
      st->controllerResetCountdowns[i] = 125;
    }

    interpretPause(st, st->pause[i][0], st->pause[i][1]); // main.js:785
  } else { // main.js:787 AI
    // main.js:788-795 — raw copies then re-deaden in place
    tempBuffer->slot[0].rawX = tempBuffer->slot[0].lsX;
    tempBuffer->slot[0].rawY = tempBuffer->slot[0].lsY;
    tempBuffer->slot[0].rawcsX = tempBuffer->slot[0].csX;
    tempBuffer->slot[0].rawcsY = tempBuffer->slot[0].csY;
    tempBuffer->slot[0].lsX = deaden(tempBuffer->slot[0].rawX, ml_deadzoneConst());
    tempBuffer->slot[0].lsY = deaden(tempBuffer->slot[0].rawY, ml_deadzoneConst());
    tempBuffer->slot[0].csX = deaden(tempBuffer->slot[0].rawcsX, ml_deadzoneConst());
    tempBuffer->slot[0].csY = deaden(tempBuffer->slot[0].rawcsY, ml_deadzoneConst());
  }

  if (st->showDebug) { // main.js:798-806
    // jQuery axis readouts + updateGamepadSVGState — render plane
  }

  if (st->gameMode == 14) { // main.js:808-810 controller calibration screen
    // updateGamepadSVGState — render plane
  }

  if (st->showDebug || st->gameMode == 14) { // main.js:812-820
    // cycleGamepadColour on x/y edges — render plane (reads tempBuffer
    // edges only; no value-plane writes)
  }

  if (st->giveInputs[i] == true) { // main.js:822-826 multiplayer
    // deepObjectMerge(true, nullInput(), tempBuffer[0]) +
    // updateNetworkInputs — netplay plane, giveInputs[i] never true in the
    // captured domain ({} at rest); trap rather than guess if it ever is
    ml_input_out_of_domain("giveInputs network path");
  }

  if (active) { // main.js:827-837 — d-pad edge debug-display toggles
    if (tempBuffer->slot[0].dl && !tempBuffer->slot[1].dl) {
      st->showLedgeGrabBox[i] ^= true; // player[i].showLedgeGrabBox
    }
    if (tempBuffer->slot[0].dd && !tempBuffer->slot[1].dd) {
      st->showECB[i] ^= true; // player[i].showECB
    }
    if (tempBuffer->slot[0].dr && !tempBuffer->slot[1].dr) {
      st->showHitbox[i] ^= true; // player[i].showHitbox
    }
  }

  if (st->frameByFrame) { // main.js:839-841
    tempBuffer->slot[0].z = false;
  }

  // main.js:843 return tempBuffer (already written through *out)
}

// main.js:1087-1091 (mode-3 gameTick tail; task 17 owns the full tick)
void ml_input_end_of_tick(MlInputSimState *st) {
  if (st->frameByFrame) {
    // frameByFrameRender = true; — render plane
    st->wasFrameByFrame = true;
  }
  st->frameByFrame = false;
}
