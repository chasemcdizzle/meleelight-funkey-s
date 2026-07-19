// SHIELDBREAKFALL.c <- src/characters/shared/moves/SHIELDBREAKFALL.js
// (M2 task 7)
#include "../moves.h"

static AsTri mv_init(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  strcpy(pl->actionState, "SHIELDBREAKFALL");
  pl->timer = 0;
  mv_dispatch(S, MV_CS(S, p), "SHIELDBREAKFALL", "main", p, in, 0);
  return AS_UNDEF;
}

static AsTri mv_main(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  const ml_attributes_t *at = mv_attr(MV_CS(S, p));
  pl->timer += 1;
  if (mv_dispatch(S, MV_CS(S, p), "SHIELDBREAKFALL", "interrupt", p, in, 0) !=
      AS_TRUE) {
    pl->phys.intangibleTimer = 1;
    pl->phys.cVel.y -= ml_f64(at->gravity);
  }
  return AS_UNDEF;
}

static AsTri mv_interrupt(MlSim *S, double p, const MlInputBuffer in[4],
                          const MvX *ex) {
  (void)ex;
  MlPlayer *pl = mv_player(S, p);
  if (pl->timer > mv_frames(MV_CS(S, p), "SHIELDBREAKFALL")) {
    mv_dispatch(S, MV_CS(S, p), "SHIELDBREAKFALL", "init", p, in, 0);
    return AS_TRUE;
  } else {
    return AS_FALSE;
  }
}

static AsTri mv_land(MlSim *S, double p, const MlInputBuffer in[4],
                     const MvX *ex) {
  // land(p, normal, input) — but upstream's ONLY caller is the landType-1
  // arm `land(i, input)` (physics.js:432, TWO args): `normal` receives
  // the god input array and `input` is undefined (rule-13 family:
  // argument-shape jank carried verbatim; LIVE since M4 task 6's
  // depletion-break fix — the old missing-normal trap here contradicted
  // upstream and blocked the whole shield-break chain). Observables of
  // the jank: SHIELDBREAKDOWNBOUND.init's groundBounce vfx `f: normal`
  // — a non-numeric value whose every downstream renderer use is NaN
  // arithmetic (the task-2 load-bearing-NaN class) — modeled as
  // DX_NUM NaN; the undefined `input` is never dereferenced on the
  // init-frame path (timer-only interrupt at timer 1 — measured by
  // reading; later frames redispatch with the real buffers).
  MvX x;
  if (ex != 0 && ex->count >= 1) {
    // the rule-11 sweep domain: a synthetic 3-arg land(p, normal, input)
    // call with a REAL normal — forwarded verbatim (the pre-task-6
    // behavior, still exercised by the moves-shared capture replay)
    x = *ex;
    x.count = 1;
  } else {
    // the LIVE physics landType-1 path (2-arg; jank note above)
    x.count = 1;
    x.x[0].kind = DX_NUM;
    x.x[0].num = js_nan(); // the god-array's numeric coercion
  }
  mv_dispatch(S, MV_CS(S, p), "SHIELDBREAKDOWNBOUND", "init", p, in, &x);
  return AS_UNDEF;
}

const MlMoveDef mv_SHIELDBREAKFALL = {"SHIELDBREAKFALL", mv_init, mv_main,
                                      mv_interrupt, mv_land};
