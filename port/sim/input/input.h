// input.h <- src/input/input.js (structure-parallel translation — the SIM
// slice, M2 task 3): the Input record constructor family plus the
// pollInputs seam.
//
// Skipped by judgment (documented, fix_plan §M2 conventions):
// pollKeyboardInputs / pollGamepadInputs / setCustomCenters / showButton /
// keyboardMap are browser-I/O plane (DOM key events, navigator.getGamepads,
// jQuery, settings keyMap) with ZERO live records in the harness domain —
// pollInputs short-circuits to the injected per-frame input for human
// slots before reaching them (the harness patch, oracle/
// meleelight-harness.patch, IS the oracle behavior recorded in the frozen
// streams). Their math core (meleeInputs GC scaling/deadzone/quantization)
// is translated in melee_inputs.h and sweep-verified; the M3 device input
// frontend synthesizes melee-unit coordinates directly (docs/research/
// b0xx-mapping.md §3). aiInputBank + the AI poll path are task 16's
// AI-input bridge.
#ifndef ML_INPUT_INPUT_H
#define ML_INPUT_INPUT_H

#include "../ml_input.h"
#include "melee_inputs.h"

// input.js:42 inputData(list) — the 22-field Input record from the
// positional 18-list. Field mapping carried VERBATIM, including the
// r <- list[5] / l <- list[6] swap (input.js:49-50) and the deadened
// stick fields alongside their raw copies.
static inline MlInput inputData(const MlInputList *list) {
  MlInput in;
  in.a = list->flags[0];
  in.b = list->flags[1];
  in.x = list->flags[2];
  in.y = list->flags[3];
  in.z = list->flags[4];
  in.r = list->flags[5];
  in.l = list->flags[6];
  in.s = list->flags[7];
  in.du = list->flags[8];
  in.dr = list->flags[9];
  in.dd = list->flags[10];
  in.dl = list->flags[11];
  in.lsX = deaden(list->nums[0], ml_deadzoneConst()); // list[12]
  in.lsY = deaden(list->nums[1], ml_deadzoneConst()); // list[13]
  in.csX = deaden(list->nums[2], ml_deadzoneConst()); // list[14]
  in.csY = deaden(list->nums[3], ml_deadzoneConst()); // list[15]
  in.lA = list->nums[4];   // list[16]
  in.rA = list->nums[5];   // list[17]
  in.rawX = list->nums[0];
  in.rawY = list->nums[1];
  in.rawcsX = list->nums[2];
  in.rawcsY = list->nums[3];
  return in;
}

// input.js:42 default InputList: 12x false, 6x 0.
static inline MlInputList ml_default_input_list(void) {
  MlInputList l;
  for (int i = 0; i < 12; i++) l.flags[i] = false;
  for (int i = 0; i < 6; i++) l.nums[i] = 0;
  return l;
}

// input.js:69 nullInput()
static inline MlInput nullInput(void) {
  const MlInputList l = ml_default_input_list();
  return inputData(&l);
}

// input.js:71 nullInputs() — the fresh 8-deep all-neutral buffer.
static inline void nullInputs(MlInputBuffer *out) {
  for (int k = 0; k < 8; k++) {
    const MlInputList l = ml_default_input_list();
    out->slot[k] = inputData(&l);
  }
}

// input.js:122 pollInputs — the sim's fresh-input source, HARNESS-PATCHED
// form (the form the frozen golden streams were recorded under): human
// slots return the injected per-frame trace input VERBATIM (object alias
// upstream -> value copy here). The other branches are out of the M2
// captured domain: AI slots (playertype 1, gameMode 3) read aiInputBank —
// task 16's recorded-input bridge; replay/network/keyboard/gamepad polling
// is browser-I/O plane (see header note). Callers hold the injected input
// for the (frame, slot) being ticked; task 17's trace loader feeds this
// the same way the harness feeds window.__harnessInputs.
static inline MlInput ml_poll_inputs(const MlInput *injected) {
  return *injected;
}

#endif // ML_INPUT_INPUT_H
