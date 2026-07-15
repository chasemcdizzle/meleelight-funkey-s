// moves_index.c <- src/characters/falcon/moves/index.js (M2 task 10):
// the falcon module index (MOVES[...] dispatch surface). See moves.h.
#include "moves.h"

#include <string.h>

// structure-parallel to index.js's export object (67 keys)
static const MlMoveDef *const FALCON_MOVES[] = {
    &falcon_JAB1, &falcon_JAB2, &falcon_JAB3, &falcon_DOWNTILT,
    &falcon_UPTILT, &falcon_FORWARDTILT, &falcon_FORWARDSMASH,
    &falcon_UPSMASH, &falcon_DOWNSMASH, &falcon_ATTACKAIRF,
    &falcon_ATTACKAIRB, &falcon_ATTACKAIRU, &falcon_ATTACKAIRD,
    &falcon_ATTACKAIRN, &falcon_ATTACKDASH, &falcon_UPSPECIAL,
    &falcon_UPSPECIALCATCH, &falcon_UPSPECIALTHROW,
    &falcon_NEUTRALSPECIALAIR, &falcon_NEUTRALSPECIALGROUND,
    &falcon_SIDESPECIALAIR, &falcon_SIDESPECIALAIRHIT,
    &falcon_SIDESPECIALGROUND, &falcon_SIDESPECIALGROUNDTOAIR,
    &falcon_SIDESPECIALGROUNDHIT, &falcon_DOWNSPECIALAIR,
    &falcon_DOWNSPECIALAIRENDAIR, &falcon_DOWNSPECIALAIRENDGROUND,
    &falcon_DOWNSPECIALGROUND, &falcon_DOWNSPECIALGROUNDENDAIR,
    &falcon_DOWNSPECIALGROUNDENDGROUND, &falcon_THROWBACK,
    &falcon_THROWDOWN, &falcon_THROWUP, &falcon_THROWFORWARD,
    &falcon_THROWNPUFFFORWARD, &falcon_THROWNPUFFDOWN,
    &falcon_THROWNPUFFBACK, &falcon_THROWNPUFFUP, &falcon_THROWNMARTHUP,
    &falcon_THROWNMARTHDOWN, &falcon_THROWNMARTHBACK,
    &falcon_THROWNMARTHFORWARD, &falcon_THROWNFOXUP, &falcon_THROWNFOXDOWN,
    &falcon_THROWNFOXBACK, &falcon_THROWNFOXFORWARD,
    &falcon_CLIFFGETUPQUICK, &falcon_CLIFFGETUPSLOW,
    &falcon_CLIFFESCAPEQUICK, &falcon_CLIFFESCAPESLOW,
    &falcon_CLIFFJUMPQUICK, &falcon_CLIFFJUMPSLOW, &falcon_CLIFFATTACKSLOW,
    &falcon_CLIFFATTACKQUICK, &falcon_DOWNATTACK, &falcon_GRAB,
    &falcon_CATCHATTACK, &falcon_THROWNFALCOUP, &falcon_THROWNFALCODOWN,
    &falcon_THROWNFALCOBACK, &falcon_THROWNFALCOFORWARD, &falcon_APPEAL,
    &falcon_THROWNFALCONUP, &falcon_THROWNFALCONDOWN,
    &falcon_THROWNFALCONBACK, &falcon_THROWNFALCONFORWARD,
};

const MlMoveDef *falcon_move_def(const char *name) {
  for (size_t k = 0; k < sizeof FALCON_MOVES / sizeof FALCON_MOVES[0]; k++) {
    if (strcmp(FALCON_MOVES[k]->name, name) == 0) return FALCON_MOVES[k];
  }
  return 0;
}

void falcon_moves_init(MlSim *S, const char *name, double p,
                       const MlInputBuffer in[4]) {
  // upstream MOVES[<name>].init — a missing key is a TypeError there
  const MlMoveDef *def = falcon_move_def(name);
  if (def == 0 || def->init == 0) {
    mv_out_of_domain("falcon MOVES index: unknown move name");
  }
  def->init(S, p, in, 0);
}

// --- special phase surfaces (shared moves.h) ---------------------------------
// upstream: only SIDESPECIAL{GROUND,AIR} carry onPlayerHit and only
// DOWNSPECIALGROUND carries onWallCollide — any other (state, phase)
// combination is a missing-property TypeError there (NULL here ->
// mv_dispatch's mv_out_of_domain).
MvFn falcon_special_phase(const char *state, const char *phase) {
  if (strcmp(phase, "onPlayerHit") == 0) {
    if (strcmp(state, "SIDESPECIALGROUND") == 0) {
      return falcon_SIDESPECIALGROUND_onPlayerHit;
    }
    if (strcmp(state, "SIDESPECIALAIR") == 0) {
      return falcon_SIDESPECIALAIR_onPlayerHit;
    }
  } else if (strcmp(phase, "onWallCollide") == 0) {
    if (strcmp(state, "DOWNSPECIALGROUND") == 0) {
      return falcon_DOWNSPECIALGROUND_onWallCollide;
    }
  }
  return 0;
}
