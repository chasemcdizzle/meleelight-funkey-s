// input_canon.h — canon-v1.1 <-> MlInput bridge (M2 task 3). Strict
// marshalling (prevention rule 7: any shape outside the captured domain
// hard-fails via ml_canon_fail) + byte-exact serialization in the frozen
// 22-key sorted order (ml_input.h). Reusable by later clusters whose
// captures carry Input objects / 8-deep buffers (physics task 5, AI
// bridge task 16).
#ifndef ML_INPUT_CANON_H
#define ML_INPUT_CANON_H

#include "../ml_input.h"
#include "canon.h"

// Provided by the driver (replay_util.c fail() pattern): report and exit 3.
void ml_canon_fail(const char *msg);

// {22 sorted keys} -> MlInput. Hard-fails on missing/extra/misordered keys,
// non-bool booleans, non-number numbers (undef is OUT of this domain —
// measured: zero undef inside Input objects across g01/g04/g06).
MlInput ml_input_from_canon(const CanonVal *v);

// 8-element array of Input objects -> MlInputBuffer.
MlInputBuffer ml_input_buffer_from_canon(const CanonVal *v);

// MlInput -> canon object (sorted 22 keys, T/F + d:<hex16> values).
void ml_input_canon(CanonBuf *b, const MlInput *in);

// MlInputBuffer -> canon array of 8 Input objects.
void ml_input_buffer_canon(CanonBuf *b, const MlInputBuffer *buf);

#endif // ML_INPUT_CANON_H
