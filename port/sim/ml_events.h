// ml_events.h — the sim's event seams (M2 task 4):
//
// - SOUND-EVENT QUEUE: upstream plays Howls inline (`sounds.X.play()`,
//   src/main/sfx.js). The C sim instead enqueues named sound events here;
//   the M4 mixer task consumes this queue (PLAN section 7). Events are part
//   of the verified module boundary: the asshort capture records every
//   sound played by a boundary call and the replay compares the C queue
//   bit-exactly (FORMAT.md "asshort").
// - DISPATCH-EVENT QUEUE: the state-machine scaffolding seam. Translated
//   code that upstream routes through `actionStates[char][NAME].<phase>` /
//   the moves-index tables notes "<phase>:<NAME>" here; until the move
//   clusters (tasks 7-12) register real implementations the note is the
//   verified observable (compared against the capture's dispatch log).
// - SEEDED RNG DRAWS: `ml_random()` draws from the active mulberry32
//   (ml_rng.h) and logs the value, mirroring the capture's per-boundary
//   draw attribution (oracle/CHECKSUM.md section 6: gameplay RNG is a
//   dedicated, seeded, tick-owned PRNG).
//
// Queues are fixed-cap; overflow is a hard failure (the captured domain's
// per-call maxima are single digits).
#ifndef ML_EVENTS_H
#define ML_EVENTS_H

#include "ml_rng.h"

#define ML_EV_CAP 32
#define ML_EV_DSP_LEN 64

typedef struct {
  int snd_count;
  const char *snd[ML_EV_CAP];
  int dsp_count;
  char dsp[ML_EV_CAP][ML_EV_DSP_LEN];
  int rng_count;
  double rng[ML_EV_CAP];
  // VFX-EVENT QUEUE (M2 task 7): upstream spawns render vfx inline
  // (drawVfx(vfxConfig), src/main/vfx/drawVfx.js). The C sim enqueues the
  // vfx NAME here (render plane; the M3/M4 renderer consumes it). Part of
  // the verified boundary from the moves clusters on: the capture records
  // every owner-attributed drawVfx name and the replay compares this queue
  // bit-exactly. NOTE "circleDust" also consumes 4 seeded draws upstream
  // (drawVfx.js:15-18) — that lives in mv_drawVfx, not here.
  int vfx_count;
  const char *vfx[ML_EV_CAP];
} MlEvents;

extern MlEvents ml_events;

// The active gameplay PRNG ml_random() draws from. The sim owns one
// seeded stream (set once at match setup); the replay driver additionally
// swaps in the sweep generator for frame-0 randomShout records
// (FORMAT.md "asshort sweep").
extern MlRng *ml_active_rng;

void ml_ev_reset(void);
void ml_sound_play(const char *name);
// Howl .stop sites (hitDetection's FURAFURA arm, M2 task 6): enqueued on
// the same queue with a ".stop"-suffixed token (e.g. "furaloop.stop") so
// the M4 mixer can distinguish stop events; the capture records the same
// token, so the queues compare bit-exactly.
void ml_sound_stop(const char *nameDotStop);
void ml_dispatch_note(const char *phase, const char *move);
void ml_vfx(const char *name);
double ml_random(void);

// Provided by the host (replay driver / future sim harness): fatal report.
void ml_events_fail(const char *what);

#endif // ML_EVENTS_H
