// moves_index.c <- src/characters/marth/moves/index.js (M2 task 11):
// the marth module index (marth[...] dispatch surface — imported by nearly
// every marth move file). See moves.h.
#include "moves.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// structure-parallel to index.js's export object (75 keys)
static const MlMoveDef *const MARTH_MOVES[] = {
    &marth_ATTACKAIRB, &marth_ATTACKAIRD, &marth_ATTACKAIRF,
    &marth_ATTACKAIRN, &marth_ATTACKAIRU, &marth_ATTACKDASH,
    &marth_CATCHATTACK, &marth_CLIFFATTACKQUICK, &marth_CLIFFATTACKSLOW,
    &marth_CLIFFESCAPEQUICK, &marth_CLIFFESCAPESLOW, &marth_CLIFFGETUPQUICK,
    &marth_CLIFFGETUPSLOW, &marth_CLIFFJUMPQUICK, &marth_CLIFFJUMPSLOW,
    &marth_DOWNATTACK, &marth_DOWNSMASH, &marth_DOWNSPECIALAIR,
    &marth_DOWNSPECIALAIR2, &marth_DOWNSPECIALGROUND,
    &marth_DOWNSPECIALGROUND2, &marth_DOWNTILT, &marth_FORWARDSMASH,
    &marth_FORWARDTILT, &marth_GRAB, &marth_JAB1, &marth_JAB2,
    &marth_NEUTRALSPECIALAIR, &marth_NEUTRALSPECIALGROUND,
    &marth_SIDESPECIALAIR, &marth_SIDESPECIALAIR2FORWARD,
    &marth_SIDESPECIALAIR2UP, &marth_SIDESPECIALAIR3DOWN,
    &marth_SIDESPECIALAIR3FORWARD, &marth_SIDESPECIALAIR3UP,
    &marth_SIDESPECIALAIR4DOWN, &marth_SIDESPECIALAIR4FORWARD,
    &marth_SIDESPECIALAIR4UP, &marth_SIDESPECIALGROUND,
    &marth_SIDESPECIALGROUND2FORWARD, &marth_SIDESPECIALGROUND2UP,
    &marth_SIDESPECIALGROUND3DOWN, &marth_SIDESPECIALGROUND3FORWARD,
    &marth_SIDESPECIALGROUND3UP, &marth_SIDESPECIALGROUND4DOWN,
    &marth_SIDESPECIALGROUND4FORWARD, &marth_SIDESPECIALGROUND4UP,
    &marth_THROWBACK, &marth_THROWDOWN, &marth_THROWFORWARD,
    &marth_THROWNFOXBACK, &marth_THROWNFOXDOWN, &marth_THROWNFOXFORWARD,
    &marth_THROWNFOXUP, &marth_THROWNMARTHBACK, &marth_THROWNMARTHDOWN,
    &marth_THROWNMARTHFORWARD, &marth_THROWNMARTHUP, &marth_THROWNPUFFBACK,
    &marth_THROWNPUFFDOWN, &marth_THROWNPUFFFORWARD, &marth_THROWNPUFFUP,
    &marth_THROWUP, &marth_UPSMASH, &marth_UPSPECIAL, &marth_UPTILT,
    &marth_THROWNFALCOUP, &marth_THROWNFALCODOWN, &marth_THROWNFALCOBACK,
    &marth_THROWNFALCOFORWARD, &marth_THROWNFALCONUP,
    &marth_THROWNFALCONDOWN, &marth_THROWNFALCONBACK,
    &marth_THROWNFALCONFORWARD, &marth_APPEAL,
};

const MlMoveDef *marth_move_def(const char *name) {
  for (size_t k = 0; k < sizeof MARTH_MOVES / sizeof MARTH_MOVES[0]; k++) {
    if (strcmp(MARTH_MOVES[k]->name, name) == 0) return MARTH_MOVES[k];
  }
  return 0;
}

void marth_moves_init(MlSim *S, const char *name, double p,
                      const MlInputBuffer in[4]) {
  // upstream marth[<name>].init — a missing key is a TypeError there
  const MlMoveDef *def = marth_move_def(name);
  if (def == 0 || def->init == 0) {
    mv_out_of_domain("marth module index: unknown move name");
  }
  def->init(S, p, in, 0);
}

// --- special phase surfaces (shared moves.h) ---------------------------------
// upstream: only DOWNSPECIAL{GROUND,AIR} carry onClank (specialClank arm,
// hitDetection.js:71-72) — any other (state, phase) combination is a
// missing-property TypeError there (NULL here -> mv_out_of_domain).
MvFn marth_special_phase(const char *state, const char *phase) {
  if (strcmp(phase, "onClank") == 0) {
    if (strcmp(state, "DOWNSPECIALGROUND") == 0) {
      return marth_DOWNSPECIALGROUND_onClank;
    }
    if (strcmp(state, "DOWNSPECIALAIR") == 0) {
      return marth_DOWNSPECIALAIR_onClank;
    }
  }
  return 0;
}

// --- the shield-breaker charge tint (NEUTRALSPECIAL{GROUND,AIR} main) --------
// let originalColour = palettes[pPal[p]][0];            // "rgb(R, G, B)"
// originalColour = originalColour.substr(4, len - 5);   // "R, G, B"
// const colourArray = originalColour.split(",");
// const newCol = blendColours(colourArray, [117, 50, 227],
//                             Math.min(1, charge / 120));
// colourOverlay = "rgb(" + newCol[0] + "," + newCol[1] + "," + newCol[2]
//                 + ")";  // blendColours parseInts + FLOORS each channel
void marth_blend_overlay(MlSim *S, double p) {
  MlPlayer *pl = mv_player(S, p);
  const char *pal = mv_palette0(p);
  if (strncmp(pal, "rgb(", 4) != 0) {
    mv_out_of_domain("marth blend: unexpected palette string");
  }
  double start[3];
  {
    const char *s = pal + 4;
    for (int i = 0; i < 3; i++) {
      char *end = 0;
      start[i] = (double)strtol(s, &end, 10); // parseInt semantics here
      if (end == s) mv_out_of_domain("marth blend: palette parseInt");
      s = end;
      while (*s == ',' || *s == ' ') s++;
    }
  }
  const double end3[3] = {117, 50, 227};
  const double opacity = js_min(1, pl->phys.shieldBreakerCharge / 120);
  long newCol[3];
  for (int i = 0; i < 3; i++) {
    const double diff = end3[i] - start[i];
    newCol[i] = (long)floor(start[i] + diff * opacity);
  }
  int n = snprintf(pl->colourOverlay, ML_STR_CAP, "rgb(%ld,%ld,%ld)",
                   newCol[0], newCol[1], newCol[2]);
  if (n < 0 || n >= ML_STR_CAP) {
    mv_out_of_domain("marth blend: overlay string overflow");
  }
  pl->colourOverlayBool = true;
}
