// ai_input.h — the JS-TYPED input value model + the interpretInputs core
// over it (M2 task 16) <- src/main/main.js:668-860 interpretInputs.
//
// WHY THIS EXISTS (fix_plan §M2 rule 16): the input plane is JS-typed —
// a field holds whatever the last writer assigned. Human slots only ever
// see real booleans in the 12 button fields (ml_input.h's measured
// domain), but the AI writes NUMBERS into aiInputBank button fields
// (ai.js `.l = 0` / `.a = 1.0` / `= inputs.l`), and helper-AI literals
// with missing keys assign UNDEFINED (CPUTech's {lsX,lsY,a} read as
// inputs.l -> aiInputBank[i][0].l = undefined). Consumption everywhere is
// truthiness-only, but the CAPTURED canon (and therefore the byte-exact
// replay) preserves the exact JS value — so the CPU slot's buffer must be
// modeled over tagged values.
//
// CLASS FIX, not a twin: interpretInputs is implemented ONCE, here, over
// the tagged model (`ml_ai_interpret_inputs`). The plain-MlInput
// `ml_interpret_inputs` (interpret_inputs.c) is now a conversion wrapper
// around this core — bool-tagged in, bool-tagged out (asserted; the human
// domain never produces number buttons). One state machine, two value
// domains.
//
// The 8 upstream write-sites of the machine are unchanged from the M2
// task-3 translation; see interpret_inputs.c's history for line-by-line
// provenance comments (kept verbatim below).
#ifndef ML_AI_INPUT_H
#define ML_AI_INPUT_H

#include <math.h>

#include "interpret_inputs.h"
#include "melee_inputs.h" // deaden (the AI-arm re-deaden, main.js:788-795)

// --- tagged JS value (measured input-plane domain: bool | number |
// undefined; anything else is out of domain — strict marshallers fail) ---
typedef enum { AIV_BOOL = 0, AIV_NUM = 1, AIV_UNDEF = 2 } MlAiTag;

typedef struct {
  MlAiTag tag;
  bool b;      // AIV_BOOL
  double num;  // AIV_NUM (exact bits)
} MlAiVal;

static inline MlAiVal aiv_bool(bool b) {
  MlAiVal v; v.tag = AIV_BOOL; v.b = b; v.num = 0; return v;
}
static inline MlAiVal aiv_num(double d) {
  MlAiVal v; v.tag = AIV_NUM; v.b = false; v.num = d; return v;
}
static inline MlAiVal aiv_undef(void) {
  MlAiVal v; v.tag = AIV_UNDEF; v.b = false; v.num = 0; return v;
}

// JS truthiness (the ONLY consumption mode of button fields upstream —
// measured, fix_plan §M2 rule 16).
static inline bool aiv_truthy(MlAiVal v) {
  if (v.tag == AIV_BOOL) return v.b;
  if (v.tag == AIV_NUM) return v.num != 0 && !isnan(v.num);
  return false; // undefined
}

// ToNumber (JS coercion at arithmetic sites; undefined -> the canonical
// quiet NaN — prevention rule 2, docs/M2CAL-REPORT.md §3).
static inline double aiv_tonum(MlAiVal v) {
  if (v.tag == AIV_NUM) return v.num;
  if (v.tag == AIV_BOOL) return v.b ? 1.0 : 0.0;
  return nan(""); // 0x7ff8000000000000
}

static inline bool aiv_eq(MlAiVal a, MlAiVal b) {
  if (a.tag != b.tag) return false;
  if (a.tag == AIV_BOOL) return a.b == b.b;
  if (a.tag == AIV_NUM) {
    // bit equality (canon-level: -0 != 0, NaN == canonical NaN)
    union { double d; unsigned long long u; } x, y;
    x.d = a.num; y.d = b.num;
    if (isnan(a.num) && isnan(b.num)) return true; // canon v1.1 collapse
    return x.u == y.u;
  }
  return true; // both undef
}

// --- the tagged Input / 8-deep buffer (src/input/input.js Input type) ---
typedef struct {
  MlAiVal a, b, x, y, z, l, r, s;
  MlAiVal du, dl, dr, dd;
  MlAiVal lA, rA;
  MlAiVal lsX, lsY;
  MlAiVal csX, csY;
  MlAiVal rawX, rawY;
  MlAiVal rawcsX, rawcsY;
} MlAiInput;

typedef struct {
  MlAiInput slot[8];
} MlAiInputBuffer;

// nullInput() over the tagged model (input.js:42-64: 12 booleans false,
// 10 numbers 0) — byte-for-byte the human nullInput's value plane.
static inline MlAiInput ai_null_input(void) {
  MlAiInput in;
  in.a = aiv_bool(false); in.b = aiv_bool(false); in.x = aiv_bool(false);
  in.y = aiv_bool(false); in.z = aiv_bool(false); in.l = aiv_bool(false);
  in.r = aiv_bool(false); in.s = aiv_bool(false);
  in.du = aiv_bool(false); in.dl = aiv_bool(false);
  in.dr = aiv_bool(false); in.dd = aiv_bool(false);
  in.lA = aiv_num(0); in.rA = aiv_num(0);
  in.lsX = aiv_num(0); in.lsY = aiv_num(0);
  in.csX = aiv_num(0); in.csY = aiv_num(0);
  in.rawX = aiv_num(0); in.rawY = aiv_num(0);
  in.rawcsX = aiv_num(0); in.rawcsY = aiv_num(0);
  return in;
}

static inline void ai_null_inputs(MlAiInputBuffer *buf) {
  for (int k = 0; k < 8; k++) buf->slot[k] = ai_null_input();
}

// --- conversions (the human MlInput domain <-> tagged) ----------------------
static inline MlAiInput ai_from_ml(const MlInput *m) {
  MlAiInput o;
  o.a = aiv_bool(m->a); o.b = aiv_bool(m->b); o.x = aiv_bool(m->x);
  o.y = aiv_bool(m->y); o.z = aiv_bool(m->z); o.l = aiv_bool(m->l);
  o.r = aiv_bool(m->r); o.s = aiv_bool(m->s);
  o.du = aiv_bool(m->du); o.dl = aiv_bool(m->dl);
  o.dr = aiv_bool(m->dr); o.dd = aiv_bool(m->dd);
  o.lA = aiv_num(m->lA); o.rA = aiv_num(m->rA);
  o.lsX = aiv_num(m->lsX); o.lsY = aiv_num(m->lsY);
  o.csX = aiv_num(m->csX); o.csY = aiv_num(m->csY);
  o.rawX = aiv_num(m->rawX); o.rawY = aiv_num(m->rawY);
  o.rawcsX = aiv_num(m->rawcsX); o.rawcsY = aiv_num(m->rawcsY);
  return o;
}

static inline bool aiv_as_bool(MlAiVal v, bool *out) {
  if (v.tag != AIV_BOOL) return false;
  *out = v.b;
  return true;
}
static inline bool aiv_as_num(MlAiVal v, double *out) {
  if (v.tag != AIV_NUM) return false;
  *out = v.num;
  return true;
}

// Human-domain projection: every button must be bool-tagged, every analog
// number-tagged (the core never introduces number buttons on the human
// path — slot 0 is the caller's polled input, history is value copies).
// A violation is a broken invariant, never a guess: out-of-domain trap.
static inline MlInput ml_from_ai(const MlAiInput *t) {
  MlInput o = nullInput();
  bool ok = true;
  ok = ok && aiv_as_bool(t->a, &o.a) && aiv_as_bool(t->b, &o.b) &&
       aiv_as_bool(t->x, &o.x) && aiv_as_bool(t->y, &o.y) &&
       aiv_as_bool(t->z, &o.z) && aiv_as_bool(t->l, &o.l) &&
       aiv_as_bool(t->r, &o.r) && aiv_as_bool(t->s, &o.s) &&
       aiv_as_bool(t->du, &o.du) && aiv_as_bool(t->dl, &o.dl) &&
       aiv_as_bool(t->dr, &o.dr) && aiv_as_bool(t->dd, &o.dd) &&
       aiv_as_num(t->lA, &o.lA) && aiv_as_num(t->rA, &o.rA) &&
       aiv_as_num(t->lsX, &o.lsX) && aiv_as_num(t->lsY, &o.lsY) &&
       aiv_as_num(t->csX, &o.csX) && aiv_as_num(t->csY, &o.csY) &&
       aiv_as_num(t->rawX, &o.rawX) && aiv_as_num(t->rawY, &o.rawY) &&
       aiv_as_num(t->rawcsX, &o.rawcsX) && aiv_as_num(t->rawcsY, &o.rawcsY);
  if (!ok) ml_input_out_of_domain("ml_from_ai: tagged value outside the human input domain");
  return o;
}

// --- interpretInputs core (main.js:668-860, verbatim; ONE implementation
// for both value domains). `polled` is the pollInputs seam result for
// this (frame, slot): the harness-injected input for human slots, THE
// aiInputBank ROW VALUE for AI slots (main.js:678 + input.js:135 — the
// upstream return is a live ALIAS of the bank row; the caller models the
// alias's post-runAI write-through by re-copying the bank into slot 0
// before physics reads the buffer, see ai_bridge.h). Mutates *st exactly
// where upstream mutates the module globals.
static inline void ml_ai_interpret_inputs(MlInputSimState *st, int i,
                                          bool active, double playertype,
                                          const MlAiInputBuffer *inputBuffer,
                                          const MlAiInput *polled,
                                          MlAiInputBuffer *out) {
  (void)playertype; // selects the poll arm upstream (input.js pollInputs);
                    // the poll itself is the caller's seam in both domains

  // main.js:670 let tempBuffer = nullInputs();
  MlAiInputBuffer *tempBuffer = out;
  ai_null_inputs(tempBuffer);

  // main.js:672-676 — keep updating Z and Start all the time, even when
  // paused
  for (int k = 0; k < 7; k++) {
    tempBuffer->slot[7 - k].z = inputBuffer->slot[6 - k].z;
    tempBuffer->slot[7 - k].s = inputBuffer->slot[6 - k].s;
  }

  // main.js:678 tempBuffer[0] = pollInputs(...)
  tempBuffer->slot[0] = *polled;

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
  const int off = (int)pastOffset;
  for (int k = 0; k < 7; k++) {
    const MlAiInput *src = &inputBuffer->slot[7 - k - off];
    MlAiInput *dst = &tempBuffer->slot[7 - k];
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
         (aiv_truthy(tempBuffer->slot[0].z) ||
          aiv_truthy(tempBuffer->slot[1].z))) ||
        (st->mType[i] != ML_MTYPE_KEYBOARD &&
         (aiv_truthy(tempBuffer->slot[0].z) &&
          !aiv_truthy(tempBuffer->slot[1].z)))) {
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
    if (aiv_truthy(tempBuffer->slot[0].s) ||
        aiv_truthy(tempBuffer->slot[1].s) ||
        (st->gameMode == 5 && (aiv_truthy(tempBuffer->slot[0].du) ||
                               aiv_truthy(tempBuffer->slot[1].du)))) {
      st->pause[i][0] = true;
    } else {
      st->pause[i][0] = false;
    }

    // main.js:738-747 — A+L+R+Start (+B) while not playing: startGame /
    // endGame. Match-lifecycle flow, out of the M2 captured domain (the
    // goldens' quality contract keeps every trace mid-match).
    if (!st->playing && (st->gameMode == 3 || st->gameMode == 5) &&
        (aiv_truthy(tempBuffer->slot[0].a) || aiv_truthy(tempBuffer->slot[1].a)) &&
        (aiv_truthy(tempBuffer->slot[0].l) || aiv_truthy(tempBuffer->slot[1].l)) &&
        (aiv_truthy(tempBuffer->slot[0].r) || aiv_truthy(tempBuffer->slot[1].r)) &&
        (aiv_truthy(tempBuffer->slot[0].s) || aiv_truthy(tempBuffer->slot[1].s))) {
      ml_input_out_of_domain("keyboard startGame/endGame combo");
    }

    ml_interpret_pause(st, st->pause[i][0], st->pause[i][1]); // main.js:749
  } else if (st->mType[i] != ML_MTYPE_NULL) { // main.js:751 gamepad controls
    // main.js:753-762
    if (!st->playing && (st->gameMode == 3 || st->gameMode == 5) &&
        (aiv_truthy(tempBuffer->slot[0].a) && aiv_truthy(tempBuffer->slot[0].l) &&
         aiv_truthy(tempBuffer->slot[0].r) && aiv_truthy(tempBuffer->slot[0].s)) &&
        !(aiv_truthy(tempBuffer->slot[1].a) && aiv_truthy(tempBuffer->slot[1].l) &&
          aiv_truthy(tempBuffer->slot[1].r) && aiv_truthy(tempBuffer->slot[1].s))) {
      ml_input_out_of_domain("gamepad startGame/endGame combo");
    }

    // main.js:764-769 — verbatim precedence: s || (du && gameMode == 5)
    if (aiv_truthy(tempBuffer->slot[0].s) ||
        (aiv_truthy(tempBuffer->slot[0].du) && st->gameMode == 5)) {
      st->pause[i][0] = true;
    } else {
      st->pause[i][0] = false;
    }

    // main.js:771-783 — controller reset countdown
    if ((aiv_truthy(tempBuffer->slot[0].z) ||
         aiv_truthy(tempBuffer->slot[0].du)) &&
        aiv_truthy(tempBuffer->slot[0].x) &&
        aiv_truthy(tempBuffer->slot[0].y)) {
      st->controllerResetCountdowns[i] -= 1;
      if (st->controllerResetCountdowns[i] == 0) {
        // triggers code in input.js (setCustomCenters on the next gamepad
        // poll) + console.log + jQuery fade — browser-I/O plane
      }
    } else {
      st->controllerResetCountdowns[i] = 125;
    }

    ml_interpret_pause(st, st->pause[i][0], st->pause[i][1]); // main.js:785
  } else { // main.js:787 AI
    // main.js:788-795 — raw copies then re-deaden in place. NOTE: in the
    // M2 captured domain this arm never runs (the harness pins every
    // active slot's mType to "keyboard", oracle/meleelight-harness.patch
    // — the CPU slot takes the keyboard arm above); translated verbatim
    // for shape fidelity. deaden's argument coerces via ToNumber.
    tempBuffer->slot[0].rawX = tempBuffer->slot[0].lsX;
    tempBuffer->slot[0].rawY = tempBuffer->slot[0].lsY;
    tempBuffer->slot[0].rawcsX = tempBuffer->slot[0].csX;
    tempBuffer->slot[0].rawcsY = tempBuffer->slot[0].csY;
    tempBuffer->slot[0].lsX =
        aiv_num(deaden(aiv_tonum(tempBuffer->slot[0].rawX), ml_deadzoneConst()));
    tempBuffer->slot[0].lsY =
        aiv_num(deaden(aiv_tonum(tempBuffer->slot[0].rawY), ml_deadzoneConst()));
    tempBuffer->slot[0].csX =
        aiv_num(deaden(aiv_tonum(tempBuffer->slot[0].rawcsX), ml_deadzoneConst()));
    tempBuffer->slot[0].csY =
        aiv_num(deaden(aiv_tonum(tempBuffer->slot[0].rawcsY), ml_deadzoneConst()));
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
    if (aiv_truthy(tempBuffer->slot[0].dl) &&
        !aiv_truthy(tempBuffer->slot[1].dl)) {
      st->showLedgeGrabBox[i] ^= true; // player[i].showLedgeGrabBox
    }
    if (aiv_truthy(tempBuffer->slot[0].dd) &&
        !aiv_truthy(tempBuffer->slot[1].dd)) {
      st->showECB[i] ^= true; // player[i].showECB
    }
    if (aiv_truthy(tempBuffer->slot[0].dr) &&
        !aiv_truthy(tempBuffer->slot[1].dr)) {
      st->showHitbox[i] ^= true; // player[i].showHitbox
    }
  }

  if (st->frameByFrame) { // main.js:839-841
    // upstream writes THROUGH the slot-0 alias into the bank row for AI
    // slots; the caller (ai_bridge chain / task 17) mirrors this write on
    // its bank copy when it fires (never in the captured domain — the
    // goldens are mid-match, playing == true, so frameByFrame stays false)
    tempBuffer->slot[0].z = aiv_bool(false);
  }

  // main.js:843 return tempBuffer (already written through *out)
}

#endif // ML_AI_INPUT_H
