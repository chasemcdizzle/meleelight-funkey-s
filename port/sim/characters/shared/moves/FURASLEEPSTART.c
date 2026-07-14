// FURASLEEPSTART.c <- src/characters/shared/moves/FURASLEEPSTART.js
// (M2 task 7)
#include "../moves.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>

// palettes[pPal[p]][0] is "rgb(R, G, B)"; substr(4, len-5) strips
// "rgb(" and ")", split(",") + parseInt gives the three components.
// blendColours (main/vfx/blendColours.js) floors the blended channels.
void mv_fura_colour(MlSim *S, double p) {
  MlPlayer *pl = mv_player(S, p);
  const char *pal = mv_palette0(p);
  if (strncmp(pal, "rgb(", 4) != 0) {
    mv_out_of_domain("fura colour: unexpected palette string");
  }
  double start[3];
  {
    const char *s = pal + 4;
    for (int i = 0; i < 3; i++) {
      char *end = 0;
      start[i] = (double)strtol(s, &end, 10); // parseInt semantics here
      if (end == s) mv_out_of_domain("fura colour: palette parseInt");
      s = end;
      while (*s == ',' || *s == ' ') s++;
    }
  }
  const double end3[3] = {207, 45, 190}; // rgb(207, 45, 190)
  const double part = fmod(pl->timer, 30);
  if (part < 25) {
    pl->colourOverlayBool = true;
    double opacity;
    if (part < 13) {
      opacity = js_min(1, part / 12);
    } else {
      opacity = js_max(0, 1 - (part - 12 / 12)); // upstream: 12/12 == 1
    }
    long newCol[3];
    for (int i = 0; i < 3; i++) {
      const double diff = end3[i] - start[i];
      newCol[i] = (long)floor(start[i] + diff * opacity);
    }
    int n = snprintf(pl->colourOverlay, ML_STR_CAP, "rgb(%ld,%ld,%ld)",
                     newCol[0], newCol[1], newCol[2]);
    if (n < 0 || n >= ML_STR_CAP) {
      mv_out_of_domain("fura colour: overlay string overflow");
    }
  } else {
    pl->colourOverlayBool = false;
  }
}

static AsTri mv_init(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  strcpy(pl->actionState, "FURASLEEPSTART");
  pl->timer = 0;
  pl->phys.stuckTimer = 95 + 2 * floor(pl->percent);
  ml_sound_play("fireweakhit");
  mv_dispatch(S, MV_CS(S, p), "FURASLEEPSTART", "main", p, in, 0);
  return AS_UNDEF;
}

static AsTri mv_main(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  pl->timer += 1;
  if (mv_dispatch(S, MV_CS(S, p), "FURASLEEPSTART", "interrupt", p, in, 0) !=
      AS_TRUE) {
    pl->phys.stuckTimer -= 1;
    as_reduceByTraction(true, (int)MV_CS(S, p), &pl->phys.cVel.x);
    mv_fura_colour(S, p);
  }
  return AS_UNDEF;
}

static AsTri mv_interrupt(MlSim *S, double p, const MlInputBuffer in[4],
                          const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  if (pl->phys.stuckTimer <= 0) {
    pl->colourOverlayBool = false;
    mv_dispatch(S, MV_CS(S, p), "FURASLEEPEND", "init", p, in, 0);
    return AS_TRUE;
  } else if (pl->timer > mv_frames(MV_CS(S, p), "FURASLEEPSTART")) {
    pl->colourOverlayBool = false;
    mv_dispatch(S, MV_CS(S, p), "FURASLEEPLOOP", "init", p, in, 0);
    return AS_TRUE;
  } else {
    return AS_FALSE;
  }
}

const MlMoveDef mv_FURASLEEPSTART = {"FURASLEEPSTART", mv_init, mv_main,
                                     mv_interrupt, 0};
