// moves_index.c <- src/characters/falco/moves/index.js (M2 task 9):
// the falco module index (MOVES[...] dispatch surface). See moves.h.
#include "moves.h"

#include <string.h>

// structure-parallel to index.js's export object (69 keys)
static const MlMoveDef *const FALCO_MOVES[] = {
    &falco_JAB1, &falco_JAB2, &falco_JAB3, &falco_DOWNTILT, &falco_UPTILT,
    &falco_FORWARDTILT, &falco_FORWARDSMASH, &falco_UPSMASH,
    &falco_DOWNSMASH, &falco_ATTACKAIRF, &falco_ATTACKAIRB,
    &falco_ATTACKAIRU, &falco_ATTACKAIRD, &falco_ATTACKAIRN,
    &falco_ATTACKDASH, &falco_UPSPECIAL, &falco_UPSPECIALCHARGE,
    &falco_UPSPECIALLAUNCH, &falco_FIREFOXBOUNCE, &falco_NEUTRALSPECIALAIR,
    &falco_NEUTRALSPECIALGROUND, &falco_SIDESPECIALAIR,
    &falco_SIDESPECIALGROUND, &falco_DOWNSPECIALAIR,
    &falco_DOWNSPECIALAIRSTART, &falco_DOWNSPECIALAIRLOOP,
    &falco_DOWNSPECIALAIREND, &falco_DOWNSPECIALAIRTURN,
    &falco_DOWNSPECIALGROUND, &falco_DOWNSPECIALGROUNDSTART,
    &falco_DOWNSPECIALGROUNDLOOP, &falco_DOWNSPECIALGROUNDEND,
    &falco_DOWNSPECIALGROUNDTURN, &falco_THROWBACK, &falco_THROWDOWN,
    &falco_THROWUP, &falco_THROWFORWARD, &falco_THROWNPUFFFORWARD,
    &falco_THROWNPUFFDOWN, &falco_THROWNPUFFBACK, &falco_THROWNPUFFUP,
    &falco_THROWNMARTHUP, &falco_THROWNMARTHDOWN, &falco_THROWNMARTHBACK,
    &falco_THROWNMARTHFORWARD, &falco_THROWNFOXUP, &falco_THROWNFOXDOWN,
    &falco_THROWNFOXBACK, &falco_THROWNFOXFORWARD, &falco_THROWNFALCOUP,
    &falco_THROWNFALCODOWN, &falco_THROWNFALCOBACK,
    &falco_THROWNFALCOFORWARD, &falco_THROWNFALCONUP,
    &falco_THROWNFALCONDOWN, &falco_THROWNFALCONBACK,
    &falco_THROWNFALCONFORWARD, &falco_CLIFFGETUPQUICK,
    &falco_CLIFFGETUPSLOW, &falco_CLIFFESCAPEQUICK, &falco_CLIFFESCAPESLOW,
    &falco_CLIFFJUMPQUICK, &falco_CLIFFJUMPSLOW, &falco_CLIFFATTACKSLOW,
    &falco_CLIFFATTACKQUICK, &falco_DOWNATTACK, &falco_GRAB,
    &falco_CATCHATTACK, &falco_APPEAL,
};

const MlMoveDef *falco_move_def(const char *name) {
  for (size_t k = 0; k < sizeof FALCO_MOVES / sizeof FALCO_MOVES[0]; k++) {
    if (strcmp(FALCO_MOVES[k]->name, name) == 0) return FALCO_MOVES[k];
  }
  return 0;
}

void falco_moves_init(MlSim *S, const char *name, double p,
                      const MlInputBuffer in[4]) {
  // upstream MOVES[<name>].init — a missing key is a TypeError there
  const MlMoveDef *def = falco_move_def(name);
  if (def == 0 || def->init == 0) {
    mv_out_of_domain("falco MOVES index: unknown move name");
  }
  def->init(S, p, in, 0);
}
