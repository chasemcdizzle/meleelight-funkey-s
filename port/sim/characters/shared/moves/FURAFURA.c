// FURAFURA.c <- src/characters/shared/moves/FURAFURA.js (M2 task 7)
// (Puff OVERRIDES this state with her own module — task 12; this shared
// body is registered for the other four characters.)
#include "../moves.h"

#include <math.h>

static AsTri mv_init(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  strcpy(pl->actionState, "FURAFURA");
  pl->timer = 0;
  pl->phys.stuckTimer = 490;
  // furaFura vfx position jitter: 2 seeded draws (x expr, then y expr —
  // sequenced via locals; C arg evaluation order is unspecified)
  const double vfxX = pl->phys.pos.x + (4 + ml_random() * 2) * pl->phys.face;
  const double vfxY = pl->phys.pos.y + 11 + ml_random() * 3;
  ml_drawVfx("furaFura", vfxX, vfxY, pl->phys.face);
  // player[p].furaLoopID = sounds.furaloop.play() — one upstream
  // expression: the play event + the consumed howler id (M4 task 6:
  // ml_howl_play_id replaces the old rule-7 trap; the id is off the
  // checksum surface and derives from the sim's own play count).
  ml_sound_play("furaloop");
  pl->furaLoopID = ml_howl_play_id("furaloop");
  mv_dispatch(S, MV_CS(S, p), "FURAFURA", "main", p, in, 0);
  return AS_UNDEF;
}

static AsTri mv_main(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  pl->timer += 1;
  if (mv_dispatch(S, MV_CS(S, p), "FURAFURA", "interrupt", p, in, 0) !=
      AS_TRUE) {
    if (fmod(pl->timer, 100) == 65) {
      const AsSoundRow *rows = 0;
      const int n = mv_actionSounds(MV_CS(S, p), "FURAFURA", &rows);
      if (n < 1) mv_out_of_domain("FURAFURA: actionSounds[..][0][1]");
      ml_sound_play(rows[0].name);
    }
    as_reduceByTraction(true, (int)MV_CS(S, p), &pl->phys.cVel.x);
    if (fmod(pl->timer, 49) == 0) {
      const double vfxX =
          pl->phys.pos.x + (3 + ml_random() * 2) * pl->phys.face;
      const double vfxY = pl->phys.pos.y + 11 + ml_random() * 3;
      ml_drawVfx("furaFura", vfxX, vfxY, pl->phys.face);
    }
    if (fmod(pl->timer, 49) == 20) {
      const double vfxX =
          pl->phys.pos.x + (5 + ml_random() * 2) * pl->phys.face;
      const double vfxY = pl->phys.pos.y + 8 + ml_random() * 3;
      ml_drawVfx("furaFura", vfxX, vfxY, pl->phys.face);
    }
    if (pl->phys.shieldHP > 30) {
      pl->phys.shieldHP = 30;
    }
    pl->phys.stuckTimer -= 1;
    if (as_mashOut(MV_IN(in, p))) {
      pl->phys.stuckTimer -= 3;
    }
  }
  return AS_UNDEF;
}

static AsTri mv_interrupt(MlSim *S, double p, const MlInputBuffer in[4],
                          const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  if (pl->phys.stuckTimer <= 0) {
    // sounds.furaloop.stop(player[p].furaLoopID) — id-routed (M4 task 6)
    ml_sound_stop_id("furaloop.stop", 1, pl->furaLoopID);
    mv_dispatch(S, MV_CS(S, p), "WAIT", "init", p, in, 0);
    return AS_TRUE;
  } else if (pl->timer > mv_frames(MV_CS(S, p), "FURAFURA")) {
    pl->timer = 1;
    return AS_FALSE;
  } else {
    return AS_FALSE;
  }
}

const MlMoveDef mv_FURAFURA = {"FURAFURA", mv_init, mv_main, mv_interrupt, 0};
