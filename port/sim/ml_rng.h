// ml_rng.h — the tick-owned seeded gameplay PRNG: mulberry32, the exact
// algorithm the oracle harness installs as Math.random
// (oracle/harness/init.js:32-40; oracle/CHECKSUM.md section 6):
//
//   a |= 0; a = (a + 0x6D2B79F5) | 0;
//   let t = Math.imul(a ^ (a >>> 15), 1 | a);
//   t = (t + Math.imul(t ^ (t >>> 7), 61 | t)) ^ t;
//   return ((t ^ (t >>> 14)) >>> 0) / 4294967296;
//
// All JS ops are 32-bit (imul wraps, `t + imul(...)` is exact in doubles
// and re-truncated mod 2^32 by the following `^`), so uint32_t arithmetic
// reproduces the stream bit-for-bit; the draw (u32 / 2^32) is exact in a
// double. State advances by the additive constant per draw, so the state
// after n draws is seed + n*0x6D2B79F5 (mod 2^32) — used to fast-forward
// past the boot-time draws (the qjs boot pin class, CLAUDE.md M0 task 6).
// Verified draw-for-draw against the harness's recorded stream (including
// the one off-step pre-frame-1 startGame draw) by the asshort replay
// (port/sim/calib/replay_asshort.c).
#ifndef ML_RNG_H
#define ML_RNG_H

#include <stdint.h>

typedef struct {
  uint32_t a;
} MlRng;

static inline void ml_rng_seed(MlRng *r, uint32_t seed) { r->a = seed; }

static inline double ml_rng_next(MlRng *r) {
  r->a += 0x6D2B79F5u;
  uint32_t a = r->a;
  uint32_t t = (a ^ (a >> 15)) * (1u | a);
  t = (t + ((t ^ (t >> 7)) * (61u | t))) ^ t;
  t = t ^ (t >> 14);
  return (double)t / 4294967296.0;
}

#endif // ML_RNG_H
