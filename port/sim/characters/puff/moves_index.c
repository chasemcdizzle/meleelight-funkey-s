// moves_index.c <- src/characters/puff/moves/index.js (M2 task 12):
// the puff module index (puff[...] dispatch surface — imported by nearly
// every puff move file; also the target of puffNextJump's computed keys).
// See moves.h.
#include "moves.h"

#include <string.h>

// structure-parallel to index.js's export object (71 keys)
static const MlMoveDef *const PUFF_MOVES[] = {
    &puff_AERIALTURN1, &puff_AERIALTURN2, &puff_AERIALTURN3,
    &puff_AERIALTURN4, &puff_AERIALTURN5, &puff_ATTACKAIRB,
    &puff_ATTACKAIRD, &puff_ATTACKAIRF, &puff_ATTACKAIRN, &puff_ATTACKAIRU,
    &puff_ATTACKDASH, &puff_CATCHATTACK, &puff_CLIFFATTACKQUICK,
    &puff_CLIFFATTACKSLOW, &puff_CLIFFESCAPEQUICK, &puff_CLIFFESCAPESLOW,
    &puff_CLIFFGETUPQUICK, &puff_CLIFFGETUPSLOW, &puff_CLIFFJUMPQUICK,
    &puff_CLIFFJUMPSLOW, &puff_DOWNATTACK, &puff_DOWNSMASH,
    &puff_DOWNSPECIALAIR, &puff_DOWNSPECIALGROUND, &puff_DOWNTILT,
    &puff_FORWARDSMASH, &puff_FORWARDTILT, &puff_FURAFURA, &puff_GRAB,
    &puff_JAB1, &puff_JAB2, &puff_JUMPAERIAL1, &puff_JUMPAERIAL2,
    &puff_JUMPAERIAL3, &puff_JUMPAERIAL4, &puff_JUMPAERIAL5,
    &puff_JUMPAERIALB, &puff_JUMPAERIALF, &puff_NEUTRALSPECIALAIR,
    &puff_NEUTRALSPECIALGROUND, &puff_NEUTRALSPECIALGROUNDTURN,
    &puff_SIDESPECIALAIR, &puff_SIDESPECIALGROUND, &puff_THROWBACK,
    &puff_THROWDOWN, &puff_THROWFORWARD, &puff_THROWNFOXBACK,
    &puff_THROWNFOXDOWN, &puff_THROWNFOXFORWARD, &puff_THROWNFOXUP,
    &puff_THROWNMARTHBACK, &puff_THROWNMARTHDOWN, &puff_THROWNMARTHFORWARD,
    &puff_THROWNMARTHUP, &puff_THROWNPUFFBACK, &puff_THROWNPUFFDOWN,
    &puff_THROWNPUFFFORWARD, &puff_THROWNPUFFUP, &puff_THROWUP,
    &puff_UPSMASH, &puff_UPSPECIAL, &puff_UPTILT, &puff_THROWNFALCOUP,
    &puff_THROWNFALCODOWN, &puff_THROWNFALCOBACK, &puff_THROWNFALCOFORWARD,
    &puff_THROWNFALCONUP, &puff_THROWNFALCONDOWN, &puff_THROWNFALCONBACK,
    &puff_THROWNFALCONFORWARD, &puff_APPEAL,
};

const MlMoveDef *puff_move_def(const char *name) {
  for (size_t k = 0; k < sizeof PUFF_MOVES / sizeof PUFF_MOVES[0]; k++) {
    if (strcmp(PUFF_MOVES[k]->name, name) == 0) return PUFF_MOVES[k];
  }
  return 0;
}

void puff_moves_init(MlSim *S, const char *name, double p,
                     const MlInputBuffer in[4]) {
  // upstream puff[<name>].init — a missing key is a TypeError there
  // (puffNextJump's AERIALTURN6/JUMPAERIAL6 would land here; unreachable:
  // every multijump dispatch is jumpsUsed<5-guarded)
  const MlMoveDef *def = puff_move_def(name);
  if (def == 0 || def->init == 0) {
    mv_out_of_domain("puff module index: unknown move name");
  }
  def->init(S, p, in, 0);
}

// --- special phase surfaces (shared moves.h) ---------------------------------
// upstream: onPlayerHit lives on NEUTRALSPECIAL{GROUND,AIR,GROUNDTURN}
// (hitDetection.js:493's specialOnHit arm), onWallCollide on
// NEUTRALSPECIALAIR (physics.js:122's specialWallCollide arm) — any
// other (state, phase) combination is a missing-property TypeError
// there (NULL here -> mv_out_of_domain).
MvFn puff_special_phase(const char *state, const char *phase) {
  if (strcmp(phase, "onPlayerHit") == 0) {
    if (strcmp(state, "NEUTRALSPECIALGROUND") == 0) {
      return puff_NEUTRALSPECIALGROUND_onPlayerHit;
    }
    if (strcmp(state, "NEUTRALSPECIALAIR") == 0) {
      return puff_NEUTRALSPECIALAIR_onPlayerHit;
    }
    if (strcmp(state, "NEUTRALSPECIALGROUNDTURN") == 0) {
      return puff_NEUTRALSPECIALGROUNDTURN_onPlayerHit;
    }
  }
  if (strcmp(phase, "onWallCollide") == 0) {
    if (strcmp(state, "NEUTRALSPECIALAIR") == 0) {
      return puff_NEUTRALSPECIALAIR_onWallCollide;
    }
  }
  return 0;
}
