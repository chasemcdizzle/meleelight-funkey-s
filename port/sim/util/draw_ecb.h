// draw_ecb.h <- src/main/util/drawECB.js — render-only canvas stroke of
// the ECB outline; it reads NO sim state back and mutates nothing the sim
// reads. Documented no-op in the headless port (the harness runs with
// __harnessNoRender anyway; the browser capture executed it against a
// canvas with zero effect on the checksum stream).
#ifndef ML_DRAW_ECB_H
#define ML_DRAW_ECB_H

#include "ecb_transform.h"

static inline void drawECB(ECB ecb, const char *color) {
  (void)ecb;
  (void)color;
}

#endif // ML_DRAW_ECB_H
