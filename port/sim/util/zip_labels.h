// zip_labels.h <- src/main/util/zipLabels.js (structure-parallel
// translation). The JS recursion builds [[surface, [label, index]], ...]
// preserving order with indices from `start`; the C loop appends into a
// LabelledSurfaceList identically (concat semantics of the callers are
// realized by appending into the same list).
#ifndef ML_ZIP_LABELS_H
#define ML_ZIP_LABELS_H

#include <assert.h>
#include "../stage_types.h"

// zips labelling information onto a list (appends to `out`)
static inline void zipLabelsInto(LabelledSurfaceList *out,
                                 const SurfaceList *list, char label) {
  for (int i = 0; i < list->count; i++) {
    assert(out->count < ML_MAX_LABELLED_SURFACES);
    out->items[out->count].surface = list->items[i];
    out->items[out->count].type = label;
    out->items[out->count].index = (double)i; // start = 0
    out->count++;
  }
}

#endif // ML_ZIP_LABELS_H
