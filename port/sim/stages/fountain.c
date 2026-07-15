// port/sim/stages/fountain.c <- src/stages/vs-stages/fountain.js (upstream
// pin 27af171): the movingPlatforms body — the two side platforms'
// module-private platformStates machine (updatePlatform), the
// main.starting reset arm, and the player transfer arms.
//
// - platformStates is module-PRIVATE upstream (a closure `let`); here it
//   is MpSim.ps, chained across records by the replay per fix_plan §M2
//   rule 18 (observable in captures via the run-capture.js served-bytes
//   getter window.__mpFountainPS).
// - updatePlatform draws the SEEDED stream (ml_random) — one draw per
//   destination selection and one per non-base arrival; the draw at a
//   selection is consumed even when the |y|<0.075 base-return arm ignores
//   t (order verbatim).
// - platL/platR/platYMin/platYMax are module constants; additionalOffset
//   is envcoll's exported constant (ML_ADDITIONAL_OFFSET); 22.125/16.125/
//   19.875/0.075 and the timer formulas are upstream code literals.
#include "../environmental_collision.h" // ML_ADDITIONAL_OFFSET
#include "../ml_events.h"               // ml_random (logged seeded draw)
#include "../ml_js.h"                   // js_abs
#include "moving_platforms.h"

static const double platL = 21;
static const double platR = 49.5;
static const double platYMin = 12.375;
static const double platYMax = 27.375;

static void updatePlatform(MpSim *S, int i, int j) {
  MpPlatformState *platformState = &S->ps[j];
  if (platformState->isStatic) { // platformState.state === "static"
    if (platformState->timer < 1) {
      platformState->timer = 0;
      platformState->isStatic = false; // state = "moving"
      double t = ml_random();
      if (js_abs(S->platform[i][0].y) < 0.075) {
        platformState->destination = 19.875;
      } else if (t < 0.3) {
        platformState->destination = -ML_ADDITIONAL_OFFSET;
      } else {
        t = (t - 0.3) / 0.7;
        platformState->destination = platYMin + t * (platYMax - platYMin);
      }
    } else {
      platformState->timer -= 1; // platformState.timer--
    }
  } else {
    const double destination = platformState->destination;
    if (S->platform[i][0].y < destination - 0.075) {
      S->platform[i][0].y += 0.075;
      S->platform[i][1].y += 0.075;
    } else if (S->platform[i][0].y > destination + 0.075) {
      S->platform[i][0].y -= 0.075;
      S->platform[i][1].y -= 0.075;
    } else {
      S->platform[i][0].y = destination;
      S->platform[i][1].y = destination;
      double newTimer;
      if (destination < 0.075) {
        newTimer = 480 + 360 * ml_random();
      } else if (js_abs(destination - 19.875) < 0.075) {
        // platform returning to base height
        newTimer = 0;
      } else {
        newTimer = 240 + 360 * ml_random();
      }
      platformState->isStatic = true; // state = "static"
      platformState->timer = newTimer;
    }
  }
}

void mp_fountain_movingPlatforms(MpSim *S) {
  if (S->starting) { // resets the stage
    S->ps[0].isStatic = false; // [{state:"moving", timer:0, dest:22.125},
    S->ps[0].timer = 0;
    S->ps[0].destination = 22.125;
    S->ps[1].isStatic = false; //  {state:"moving", timer:0, dest:16.125}]
    S->ps[1].timer = 0;
    S->ps[1].destination = 16.125;
    S->platform[1][0].y = 22.125;
    S->platform[1][1].y = 22.125;
    S->platform[2][0].y = 16.125;
    S->platform[2][1].y = 16.125;
  } else {
    updatePlatform(S, 1, 0);
    updatePlatform(S, 2, 1);
    for (int j = 0; j < 4; j++) {
      MpPlayerSlice *p = &S->player[j];
      if (p->grounded) {
        if (p->onSurface[0] == 1 &&
            (p->onSurface[1] == 1 || p->onSurface[1] == 2)) {
          const double plat = p->onSurface[1]; // const plat = ...onSurface[1]
          if (S->platform[(int)plat][0].y < ML_ADDITIONAL_OFFSET) {
            p->pos.y = ML_ADDITIONAL_OFFSET;
            // transfer player from platform to middle ground
            p->onSurface[0] = 0; // onSurface = [0,2]
            p->onSurface[1] = 2;
          }
        } else if (p->onSurface[0] == 0 && p->onSurface[1] == 2) {
          const double x = p->pos.x;
          if (!S->ps[0].isStatic && S->platform[1][0].y < 0.075 &&
              S->ps[0].destination > 0.075 && x >= platL && x <= platR) {
            // transfer player from middle ground to right platform
            p->onSurface[0] = 1; // onSurface = [1,1]
            p->onSurface[1] = 1;
          } else if (!S->ps[1].isStatic && S->platform[2][0].y < 0.075 &&
                     S->ps[1].destination > 0.075 && x >= -platR &&
                     x <= -platL) {
            // transfer player from middle ground to left platform
            p->onSurface[0] = 1; // onSurface = [1,2]
            p->onSurface[1] = 2;
          }
        }
      }
    }
  }
}
