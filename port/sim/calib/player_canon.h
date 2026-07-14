// player_canon.h — canon <-> MlPlayer bridge for the M2 capture/replay rig
// (M2 task 2; FORMAT.md "player spec"). Strict marshaller (prevention rule
// 7: any shape outside the captured domain hard-fails via pc_fail) and the
// sorted-key canon serializer whose output must be byte-identical to the
// browser capture's canon for every representable value. Reused by later
// mutation clusters (physics/hitDetection/moves) for pre/post player
// states.
#ifndef ML_PLAYER_CANON_H
#define ML_PLAYER_CANON_H

#include "../ml_player.h"
#include "canon.h"

// Provided by the driver: report a marshal failure (with record context)
// and abort. Never returns.
extern void pc_fail(const char *msg);

// Strict marshal: parsed canon tree -> MlPlayer (captured domain only).
void cv_player(const CanonVal *v, MlPlayer *out);

// Serialize: MlPlayer -> canon v1.1 bytes (sorted-key order).
void ser_player(CanonBuf *b, const MlPlayer *p);

// Exposed for the merge property check in replay_player.c.
void cv_hitboxes(const CanonVal *v, MlHitboxes *out);
void ser_hitboxes(CanonBuf *b, const MlHitboxes *h);

#endif // ML_PLAYER_CANON_H
