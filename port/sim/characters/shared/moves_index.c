// moves_index.c <- src/characters/shared/moves/index.js (M2 task 7):
// the shared move table + mv_dispatch + the helpers every shared move
// body shares (drawVfx/screenShake/playSounds/framesData/attribute reads,
// the rule-10 alias write helpers). See moves.h.
#include "moves.h"

#include <string.h>

#include "../../ml_events.h"
#include "ml_tables.h"

// --- the shared move table (structure-parallel to index.js) -----------------
static const MlMoveDef *const MV_SHARED[] = {
    &mv_WAIT, &mv_DASH, &mv_RUN, &mv_SMASHTURN, &mv_TILTTURN, &mv_RUNBRAKE,
    &mv_RUNTURN, &mv_WALK, &mv_KNEEBEND, &mv_JUMPF, &mv_JUMPB, &mv_LANDING,
    &mv_ESCAPEAIR, &mv_LANDINGFALLSPECIAL, &mv_FALL, &mv_FALLAERIAL,
    &mv_FALLSPECIAL, &mv_SQUAT, &mv_SQUATWAIT, &mv_SQUATRV, &mv_JUMPAERIALF,
    &mv_JUMPAERIALB, &mv_PASS, &mv_GUARDON, &mv_GUARD, &mv_GUARDOFF,
    &mv_CLIFFCATCH, &mv_CLIFFWAIT, &mv_DEADLEFT, &mv_DEADRIGHT, &mv_DEADUP,
    &mv_DEADDOWN, &mv_REBIRTH, &mv_REBIRTHWAIT, &mv_DAMAGEFLYN,
    &mv_DAMAGEFALL, &mv_DAMAGEN2, &mv_LANDINGATTACKAIRN,
    &mv_LANDINGATTACKAIRF, &mv_LANDINGATTACKAIRB, &mv_LANDINGATTACKAIRD,
    &mv_LANDINGATTACKAIRU, &mv_ESCAPEB, &mv_ESCAPEF, &mv_ESCAPEN,
    &mv_DOWNBOUND, &mv_DOWNWAIT, &mv_DOWNDAMAGE, &mv_DOWNSTANDN,
    &mv_DOWNSTANDB, &mv_DOWNSTANDF, &mv_TECHN, &mv_TECHB, &mv_TECHF,
    &mv_SHIELDBREAKFALL, &mv_SHIELDBREAKDOWNBOUND, &mv_SHIELDBREAKSTAND,
    &mv_FURAFURA, &mv_CAPTUREPULLED, &mv_CAPTUREWAIT, &mv_CATCHWAIT,
    &mv_CAPTURECUT, &mv_CATCHCUT, &mv_CAPTUREDAMAGE, &mv_WALLDAMAGE,
    &mv_WALLTECH, &mv_WALLJUMP, &mv_WALLTECHJUMP, &mv_OTTOTTO,
    &mv_OTTOTTOWAIT, &mv_MISSFOOT, &mv_FURASLEEPSTART, &mv_FURASLEEPLOOP,
    &mv_FURASLEEPEND, &mv_STOPCEIL, &mv_TECHU, &mv_SLEEP, &mv_ENTRANCE,
    &mv_THROWNFALCONDIVE,
};

const MlMoveDef *mv_shared_def(const char *name) {
  for (size_t k = 0; k < sizeof MV_SHARED / sizeof MV_SHARED[0]; k++) {
    if (strcmp(MV_SHARED[k]->name, name) == 0) return MV_SHARED[k];
  }
  return 0;
}

// --- dispatch -----------------------------------------------------------------
AsTri mv_dispatch(MlSim *S, double charId, const char *state,
                  const char *phase, double slot, const MlInputBuffer in[4],
                  const MvX *ex) {
  const MlMoveDef *def = as_lookup((int)charId, state);
  if (def == 0) return mv_seam(S, charId, state, phase, slot, ex);
  MvFn fn = 0;
  if (strcmp(phase, "init") == 0) fn = def->init;
  else if (strcmp(phase, "main") == 0) fn = def->main_;
  else if (strcmp(phase, "interrupt") == 0) fn = def->interrupt;
  else if (strcmp(phase, "land") == 0) fn = def->land;
  else mv_out_of_domain("mv_dispatch: unknown phase");
  // upstream: calling a property the move object lacks is a TypeError
  if (fn == 0) mv_out_of_domain("mv_dispatch: phase missing on shared move");
  return fn(S, slot, in, ex);
}

// --- data-plane helpers ---------------------------------------------------------
const ml_attributes_t *mv_attr(double charId) {
  const int c = (int)charId;
  if (c < 0 || c >= ML_CHARS) mv_out_of_domain("charAttributes char id");
  return &ml_attributes[c];
}

double mv_frames(double charId, const char *state) {
  const int c = (int)charId;
  if (c < 0 || c >= ML_CHARS) mv_out_of_domain("framesData char id");
  for (int32_t k = 0; k < ml_frames_count[c]; k++) {
    if (strcmp(ml_frames_data[c][k].name, state) == 0) {
      return (double)ml_frames_data[c][k].frames;
    }
  }
  // framesData[c][state] === undefined: `timer > undefined` is false —
  // NaN comparisons are false in C too (rule 2's canonical-NaN mapping).
  return js_nan();
}

// --- event helpers ---------------------------------------------------------------
void mv_drawVfx(const char *name) {
  ml_vfx(name);
  if (strcmp(name, "circleDust") == 0) {
    // drawVfx.js:15-18 — 4 seeded draws (values render-only, discarded).
    (void)ml_random();
    (void)ml_random();
    (void)ml_random();
    (void)ml_random();
  }
}

void mv_screenShake(void) {
  // main.js:352-358 — 4 seeded draws; fg1.translate is render-only.
  (void)ml_random();
  (void)ml_random();
  (void)ml_random();
  (void)ml_random();
}

void mv_playSounds(MlSim *S, const char *state, double p) {
  const double c = S->characterSelections[(int)p];
  const AsSoundRow *rows = 0;
  const int n = mv_actionSounds(c, state, &rows);
  // playSounds on a missing schedule reads `.length` of undefined upstream.
  if (n < 0) mv_out_of_domain("playSounds: missing actionSounds schedule");
  as_playSounds(rows, n, mv_player(S, p)->timer);
}

bool mv_isFinalDeath(MlSim *S) {
  AsFinalDeathState st;
  st.gameMode = S->gameMode;
  st.versusMode = S->versusMode;
  for (int k = 0; k < 4; k++) {
    st.playerType[k] = S->playerType[k];
    st.stocksPresent[k] = S->playerPresent[k];
    st.stocks[k] = S->playerPresent[k] ? S->player[k].stocks : 0;
  }
  return as_isFinalDeath(&st);
}

// --- player / alias helpers (the physics.c discipline) ---------------------------
MlPlayer *mv_player(MlSim *S, double i) {
  const int k = (int)i;
  if (k < 0 || k > 3 || !S->playerPresent[k]) {
    mv_out_of_domain("player deref on absent slot");
  }
  return &S->player[k];
}

void mv_pos_set_x(MlSim *S, double i, double v) {
  MlPlayer *p = mv_player(S, i);
  p->phys.pos.x = v;
  if (S->aliasPosEcb1[(int)i]) {
    p->phys.ECB1[0].x = v;
    p->phys.ecb1Undef &= (uint8_t)~1u; // component now a number (rule 8)
  }
}

void mv_pos_set_y(MlSim *S, double i, double v) {
  MlPlayer *p = mv_player(S, i);
  p->phys.pos.y = v;
  if (S->aliasPosEcb1[(int)i]) {
    p->phys.ECB1[0].y = v;
    p->phys.ecb1Undef &= (uint8_t)~2u;
  }
}

void mv_pos_reassign(MlSim *S, double i, Vec2D v) {
  mv_player(S, i)->phys.pos = v;
  S->aliasPosEcb1[(int)i] = false; // fresh Vec2D breaks the alias
}

void mv_turnOffHitboxes(MlSim *S, double i) {
  as_turnOffHitboxes(&mv_player(S, i)->hitboxes);
  // hitboxes.active/hitList are REASSIGNED fresh arrays upstream:
  S->aliasHbActive[(int)i] = false;
  S->aliasHbHitList[(int)i] = false;
}

void mv_assign_hitbox_id(MlSim *S, double p, const char *moveKey, int srcIdx,
                         int dstIdx) {
  MlPlayer *pl = mv_player(S, p);
  const int c = (int)S->characterSelections[(int)p];
  if (c < 0 || c >= ML_CHARS) mv_out_of_domain("charHitboxes char id");
  if (srcIdx < 0 || srcIdx > 3 || dstIdx < 0 || dstIdx > 3) {
    mv_out_of_domain("charHitboxes id index");
  }
  const ml_hitbox_t *src = 0;
  for (int32_t k = 0; k < ml_hitbox_move_count[c]; k++) {
    if (strcmp(ml_hitbox_moves[c][k].name, moveKey) == 0) {
      src = ml_hitbox_moves[c][k].id[srcIdx];
      break;
    }
  }
  if (src == 0) mv_out_of_domain("charHitboxes id entry missing");
  // Every charHitboxes entry is a 12-key createHitbox object (chars-data
  // shape); its offset is a per-frame Vec2D ARRAY or (throw hitboxes) a
  // SINGLE Vec2D — ml_player.h offsetSingle (M2 task 8 class fix: the old
  // CONSTRUCTOR fallback here mis-shaped single-offset chars data; it was
  // unreached by every prior capture).
  MlHitboxSpec hb;
  memset(&hb, 0, sizeof hb);
  hb.shape = ML_HB_CHARDATA;
  if (src->offsetIsArray) {
    if (src->offsetCount > ML_HB_OFFSET_CAP) {
      mv_out_of_domain("charHitboxes id offset array over cap");
    }
    hb.offsetLen = src->offsetCount;
    for (int32_t k = 0; k < src->offsetCount; k++) {
      hb.offsetArr[k].x = ml_f64(src->offset[k].x);
      hb.offsetArr[k].y = ml_f64(src->offset[k].y);
    }
  } else {
    if (src->offsetCount != 1) mv_out_of_domain("charHitboxes id offset shape");
    hb.offsetSingle = true;
    hb.offset.x = ml_f64(src->offset[0].x);
    hb.offset.y = ml_f64(src->offset[0].y);
  }
  hb.clank = (double)src->clank;
  hb.hitAirborne = (double)src->hitAirborne;
  hb.hitGrounded = (double)src->hitGrounded;
  hb.throwextra = src->throwextra != 0;
  hb.size = ml_f64(src->size);
  hb.dmg = (double)src->dmg;
  hb.angle = (double)src->angle;
  hb.kg = (double)src->kg;
  hb.bk = (double)src->bk;
  hb.sk = (double)src->sk;
  hb.type = (double)src->type;
  pl->hitboxes.id[dstIdx] = hb;
  // element write THROUGH the id alias mirrors (rule 10):
  if (S->aliasHbId[(int)p]) pl->phys.prevFrameHitboxes.id[dstIdx] = hb;
}

void mv_assign_thrown_id0(MlSim *S, double p) {
  mv_assign_hitbox_id(S, p, "thrown", 0, 0);
}

// activeStage[l[0]][l[1]][l[2]] for l = activeStage.ledge[onLedge] — the
// generalized CLIFF* coordinate read (M2 task 8).
Vec2D mv_ledge_point(MlSim *S, double onLedge, const char *what) {
  const int idx = (int)onLedge;
  if (onLedge != (double)idx || idx < 0 || idx >= S->stage.ledgeCount) {
    // ledge[<bad>] is undefined; activeStage[l[0]] then THROWS upstream
    mv_out_of_domain(what);
  }
  const MlLedge *l = &S->stage.ledge[idx];
  const SurfaceList *list =
      l->list == 'g' ? &S->stage.s.ground : &S->stage.s.platform;
  const int si = (int)l->index;
  if (si < 0 || si >= list->count) mv_out_of_domain(what);
  const Surface *sf = &list->items[si];
  return ((int)l->point == 0) ? sf->p0 : sf->p1;
}

// --- checkForIASA with real dispatch (actionStateShortcuts.js:388-416) ------
static MvCharModuleLookup g_char_module[AS_CHARS];

void mv_register_char_module(int charId, MvCharModuleLookup lookup) {
  if (charId < 0 || charId >= AS_CHARS) {
    mv_out_of_domain("mv_register_char_module: char id");
  }
  g_char_module[charId] = lookup;
}

static void mv_char_module_init(MlSim *S, int charId, const char *name,
                                double p, const MlInputBuffer in[4]) {
  // upstream: <CHAR>MOVES[a[1]].init(p, input) — the per-char module
  // index, NOT the actionStates table (identical fns by reference, but
  // the module path is the faithful one). Unregistered char = a cluster
  // not yet translated: out of this replay's domain.
  if (g_char_module[charId] == 0) {
    mv_out_of_domain("checkForIASA: char module not registered");
  }
  const MlMoveDef *def = g_char_module[charId](name);
  if (def == 0 || def->init == 0) {
    mv_out_of_domain("checkForIASA: unknown aerial move name");
  }
  def->init(S, p, in, 0);
}

AsTri mv_checkForIASA(MlSim *S, double p, const MlInputBuffer in[4],
                      bool isAerial) {
  MlPlayer *pl = mv_player(S, p);
  // absent IASATimer: `timer > undefined` is false (NaN comparison)
  if (pl->hasIASATimer ? (pl->timer > pl->IASATimer) : false) {
    if (isAerial) {
      const AsPair a = as_checkForAerials(pl->phys.face, MV_IN(in, p));
      if ((as_checkForDoubleJump(S->tapJumpOff[(int)p], MV_IN(in, p)) &&
           (!pl->phys.doubleJumped)) ||
          (as_checkForMultiJump(S->tapJumpOff[(int)p], MV_IN(in, p)) &&
           pl->phys.jumpsUsed < 5 && mv_attr(MV_CS(S, p))->multiJump != 0)) {
        if (MV_IN(in, p)[0].lsX * pl->phys.face < -0.3) {
          mv_JUMPAERIALB.init(S, p, in, 0); // the shared MODULE object
        } else {
          mv_JUMPAERIALF.init(S, p, in, 0);
        }
        return AS_TRUE;
      } else if (a.flag) {
        const double c = MV_CS(S, p);
        if (c == 0) {
          mv_char_module_init(S, 0, mv_pair_str(&a), p, in); // MARTHMOVES
        } else if (c == 1) {
          mv_char_module_init(S, 1, mv_pair_str(&a), p, in); // PUFFMOVES
        } else if (c == 2) {
          mv_char_module_init(S, 2, mv_pair_str(&a), p, in); // FOXMOVES
        }
        // chars 3/4 dispatch nothing upstream (no branch) — verbatim
        return AS_TRUE;
      } else {
        return AS_FALSE;
      }
    } else {
      // upstream's non-aerial arm is empty
    }
  }
  return AS_UNDEF; // falls off the end -> undefined
}
