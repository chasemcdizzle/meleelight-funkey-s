// ml_input.h — the input value model (M2 task 3) <- src/input/input.js
// `Input` / `InputBuffer` / `InputList` types.
//
// Measured domain (survey-shapes.js over the g01/g04/g06 input captures,
// capture-FIRST rule 7): every Input object carries exactly the 22 keys
// below, booleans are real JS booleans (never undef-at-rest) and the ten
// analog fields are real numbers — so plain C bool/double fields are the
// faithful model (unlike ml_player.h's JsBool/presence-flag cases). The
// strict marshaller (port/sim/calib/input_canon.c) hard-fails if a future
// capture ever violates this.
//
// Canon key order (byte-wise sort of the 22 ASCII keys — uppercase sorts
// before lowercase): a, b, csX, csY, dd, dl, dr, du, l, lA, lsX, lsY, r,
// rA, rawX, rawY, rawcsX, rawcsY, s, x, y, z.
#ifndef ML_INPUT_H
#define ML_INPUT_H

#include <stdbool.h>

// src/input/input.js:16-37 `export type Input`.
typedef struct {
  bool a, b, x, y, z, l, r, s;
  bool du, dl, dr, dd;
  double lA, rA;
  double lsX, lsY;
  double csX, csY;
  double rawX, rawY;
  double rawcsX, rawcsY;
} MlInput;

// src/input/input.js:38 `export type InputBuffer = Array<Input>` — always
// 8 deep in the sim (nullInputs()); slot 0 = current frame, higher = older.
typedef struct {
  MlInput slot[8];
} MlInputBuffer;

// src/input/input.js:40 `type InputList` — the positional 18-element
// argument of inputData: 12 booleans then 6 numbers. Index mapping is
// inputData's job (input.h); note upstream maps list[5]->r, list[6]->l
// (swapped relative to the Input type's field comment order) — carried
// verbatim.
typedef struct {
  bool flags[12];   // list[0..11]
  double nums[6];   // list[12..17]
} MlInputList;

#endif // ML_INPUT_H
