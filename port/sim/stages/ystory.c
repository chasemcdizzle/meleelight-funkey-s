// port/sim/stages/ystory.c <- src/stages/vs-stages/ystory.js (upstream pin
// 27af171): the movingPlatforms body — Randall's rectangular rail state
// machine, driven purely by platform[0]'s position (no timers, no RNG).
// The four arms are sequential non-exclusive ifs: corner frames execute
// TWO arms in one call (arm 1 then arm 3 at the bottom-left, arm 2 then
// arm 4 at the top-right) and `move` keeps the LAST arm's value —
// evaluation order carried verbatim (fix_plan §M2 rule 13 family).
// Stage data constants (-103.6/-91.7/91.35/103.25/-33.25/-13.65 and the
// 0.354845 step) are upstream CODE literals, not STAB1 data — carried
// verbatim (the physics edgeOffset precedent).
#include "moving_platforms.h"

void mp_ystory_movingPlatforms(MpSim *S) {
  Vec2D *plat = S->platform[0]; // const plat = activeStage.platform[0];
  double move[2] = {0, 0};
  if (plat[0].x <= -103.6 && plat[0].y > -33.25) {
    plat[0].x = -103.6;
    plat[1].x = -91.7;
    plat[0].y -= 0.354845;
    plat[1].y -= 0.354845;
    move[0] = 0;
    move[1] = -0.354845;
  }
  if (plat[0].x >= 91.35 && plat[0].y < -13.65) {
    plat[0].x = 91.35;
    plat[1].x = 103.25;
    plat[0].y += 0.354845;
    plat[1].y += 0.354845;
    move[0] = 0;
    move[1] = 0.354845;
  }
  if (plat[0].y <= -33.25) {
    plat[0].y = -33.25;
    plat[1].y = -33.25;
    plat[0].x += 0.354845;
    plat[1].x += 0.354845;
    move[0] = 0.354845;
    move[1] = 0;
  }
  if (plat[0].y >= -13.65) {
    plat[0].y = -13.65;
    plat[1].y = -13.65;
    plat[0].x -= 0.354845;
    plat[1].x -= 0.354845;
    move[0] = -0.354845;
    move[1] = 0;
  }

  for (int j = 0; j < 4; j++) {
    if (S->player[j].onSurface[0] == 1 && S->player[j].onSurface[1] == 0 &&
        S->player[j].grounded) {
      S->player[j].pos.x += move[0];
      // player[j].phys.pos.y += move[1]; (commented out upstream)
      S->player[j].pos.y = plat[0].y;
    }
  }
}
