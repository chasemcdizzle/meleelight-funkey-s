// to_list.h <- src/main/util/toList.js (structure-parallel translation).
// Upstream: "temporary workaround for custom stage data being objects and
// not arrays" — a recursive shallow copy of a list. Its ONLY live sim
// call site is physics.js:853, toList(activeStage.ceiling) — a surface
// list — so the C form is the shallow SurfaceList copy (order preserved,
// elements copied by value exactly as the JS concat rebuilds them).
#ifndef ML_TO_LIST_H
#define ML_TO_LIST_H

#include "../stage_types.h"

static inline SurfaceList toList_surfaces(const SurfaceList *list) {
  SurfaceList out;
  out.count = list->count;
  for (int i = 0; i < list->count; i++) {
    out.items[i] = list->items[i];
  }
  return out;
}

#endif // ML_TO_LIST_H
