// dancing_blade_air_mobility.c <-
// src/characters/marth/dancingBladeAirMobility.js (M2 task 11).
#include "moves.h"

void marth_dancingBladeAirMobility(MlSim *S, double p) {
  MlPlayer *pl = mv_player(S, p);
  pl->phys.cVel.y -= 0.06;
  if (pl->phys.cVel.y < -1.5) {
    pl->phys.cVel.y = -1.5;
  }
  if (pl->phys.cVel.x > 0) {
    pl->phys.cVel.x -= 0.0025;
    if (pl->phys.cVel.x < 0) {
      pl->phys.cVel.x = 0;
    }
  } else {
    pl->phys.cVel.x += 0.0025;
    if (pl->phys.cVel.x > 0) {
      pl->phys.cVel.x = 0;
    }
  }
}
