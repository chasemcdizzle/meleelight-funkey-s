// find_smallest_within.h <- src/main/util/findSmallestWithin.js
// (structure-parallel translation)
//
// findSmallestWithin: the JS list is Array<number | null>; C represents a
// nullable number as MaybeNum. The JS recursion ([head, ...tail]) becomes
// a left-to-right loop with identical comparison semantics (NaN heads fail
// `head >= min` exactly as in JS and are skipped).
//
// pickSmallestSweep: JS is generic over {sweep:number}; C keeps one
// implementation over a parallel (present[], sweep[]) view returning the
// picked INDEX (-1 = null). Tie semantics preserved exactly: replacement
// only on strictly-smaller sweep, so the EARLIEST smallest wins.
#ifndef ML_FIND_SMALLEST_WITHIN_H
#define ML_FIND_SMALLEST_WITHIN_H

#include "../ml_js.h"

typedef struct { bool present; double v; } MaybeNum;

static inline MaybeNum maybe_num(double v) {
  MaybeNum m; m.present = true; m.v = v; return m;
}
static inline MaybeNum maybe_null(void) {
  MaybeNum m; m.present = false; m.v = 0; return m;
}

// finds the smallest value t of the list with t > min, t <= max
// (JS doc comment; the CODE tests head >= min && head <= max — translated
// from the code, as always)
static inline MaybeNum findSmallestWithin(const MaybeNum *list, int n,
                                          double min, double max) {
  MaybeNum smallestSoFar = maybe_null();
  for (int i = 0; i < n; i++) {
    if (!list[i].present) continue;               // head === null
    double head = list[i].v;
    if (head >= min && head <= max) {
      if (!smallestSoFar.present) {
        smallestSoFar = maybe_num(head);
      } else if (head > smallestSoFar.v) {
        // keep smallestSoFar
      } else {
        smallestSoFar = maybe_num(head);
      }
    }
  }
  return smallestSoFar;
}

// finds the object with smallest sweeping parameter; returns its index in
// the parallel view, or -1 for null.
static inline int pickSmallestSweep(const bool *present, const double *sweep,
                                    int n) {
  int smallestSoFar = -1;
  for (int i = 0; i < n; i++) {
    if (!present[i]) continue;                    // head === null
    if (smallestSoFar == -1 || sweep[i] < sweep[smallestSoFar]) {
      smallestSoFar = i;
    }
  }
  return smallestSoFar;
}

#endif // ML_FIND_SMALLEST_WITHIN_H
