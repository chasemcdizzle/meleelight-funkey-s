// moves_index.c <- src/characters/fox/moves/index.js (M2 task 8):
// the fox module index (MOVES[...] dispatch surface). See moves.h.
#include "moves.h"

#include <string.h>

// structure-parallel to index.js's export object (61 keys)
static const MlMoveDef *const FOX_MOVES[] = {
    &fox_JAB1, &fox_JAB2, &fox_JAB3, &fox_DOWNTILT, &fox_UPTILT,
    &fox_FORWARDTILT, &fox_FORWARDSMASH, &fox_UPSMASH, &fox_DOWNSMASH,
    &fox_ATTACKAIRF, &fox_ATTACKAIRB, &fox_ATTACKAIRU, &fox_ATTACKAIRD,
    &fox_ATTACKAIRN, &fox_ATTACKDASH, &fox_UPSPECIAL, &fox_UPSPECIALCHARGE,
    &fox_UPSPECIALLAUNCH, &fox_FIREFOXBOUNCE, &fox_NEUTRALSPECIALAIR,
    &fox_NEUTRALSPECIALGROUND, &fox_SIDESPECIALAIR, &fox_SIDESPECIALGROUND,
    &fox_DOWNSPECIALAIR, &fox_DOWNSPECIALGROUND, &fox_THROWBACK,
    &fox_THROWDOWN, &fox_THROWUP, &fox_THROWFORWARD, &fox_THROWNPUFFFORWARD,
    &fox_THROWNPUFFDOWN, &fox_THROWNPUFFBACK, &fox_THROWNPUFFUP,
    &fox_THROWNMARTHUP, &fox_THROWNMARTHDOWN, &fox_THROWNMARTHBACK,
    &fox_THROWNMARTHFORWARD, &fox_THROWNFOXUP, &fox_THROWNFOXDOWN,
    &fox_THROWNFOXBACK, &fox_THROWNFOXFORWARD, &fox_CLIFFGETUPQUICK,
    &fox_CLIFFGETUPSLOW, &fox_CLIFFESCAPEQUICK, &fox_CLIFFESCAPESLOW,
    &fox_CLIFFJUMPQUICK, &fox_CLIFFJUMPSLOW, &fox_CLIFFATTACKSLOW,
    &fox_CLIFFATTACKQUICK, &fox_DOWNATTACK, &fox_GRAB, &fox_CATCHATTACK,
    &fox_THROWNFALCOUP, &fox_THROWNFALCODOWN, &fox_THROWNFALCOBACK,
    &fox_THROWNFALCOFORWARD, &fox_THROWNFALCONUP, &fox_THROWNFALCONDOWN,
    &fox_THROWNFALCONBACK, &fox_THROWNFALCONFORWARD, &fox_APPEAL,
};

const MlMoveDef *fox_move_def(const char *name) {
  for (size_t k = 0; k < sizeof FOX_MOVES / sizeof FOX_MOVES[0]; k++) {
    if (strcmp(FOX_MOVES[k]->name, name) == 0) return FOX_MOVES[k];
  }
  return 0;
}

void fox_moves_init(MlSim *S, const char *name, double p,
                    const MlInputBuffer in[4]) {
  // upstream MOVES[<name>].init — a missing key is a TypeError there
  const MlMoveDef *def = fox_move_def(name);
  if (def == 0 || def->init == 0) {
    mv_out_of_domain("fox MOVES index: unknown move name");
  }
  def->init(S, p, in, 0);
}
