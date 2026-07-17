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

// --- VFX-EVENT QUEUE (M2 task 7; WIDENED M4 task 1) ----------------------
// Upstream spawns render vfx inline (drawVfx(vfxConfig),
// src/main/vfx/drawVfx.js). The C sim enqueues the FULL vfxConfig here
// as an MlVfx snapshot (upstream drawVfx also snapshots at call time:
// instance.newPos = new Vec2D(cfg.pos.x, cfg.pos.y)); the M4 renderer
// consumes it via ml_vfx_sink. Part of the verified boundary from the
// moves clusters on: the captures record every owner-attributed drawVfx
// CONFIG (canon at call time) and the replays compare this queue
// bit-exactly (FORMAT.md "vfx posts (widened)").
//
// Config domain (measured across all 193 sim-plane call sites,
// AGENT-LOG iter 64): keys ⊆ {name, pos, face, f, color1, color2};
// pos always {x,y}; f is a number (angles/slots/frames), a Vec2D
// (wall/ceiling normals), or the marth swing object
// {pNum, swingType, frame}; color1/color2 are {r,g,b}.
typedef enum {
  ML_VFX_F_NONE = 0, // key "f" absent from the config
  ML_VFX_F_NUM = 1,  // f: <number>
  ML_VFX_F_VEC = 2,  // f: Vec2D {x,y}
  ML_VFX_F_SWING = 3 // f: {pNum, swingType, frame} (marth swords)
} MlVfxFKind;

typedef struct {
  const char *name; // always present (string literal)
  double px, py;    // pos.x / pos.y (always present)
  int has_face;     // 0: key "face" absent
  double face;
  MlVfxFKind f_kind;
  double f_num;         // ML_VFX_F_NUM
  double f_x, f_y;      // ML_VFX_F_VEC
  double f_pnum;        // ML_VFX_F_SWING pNum
  const char *f_swing;  //                swingType (string literal)
  double f_frame;       //                frame
  int has_color;        // color1+color2 present together (laser sites)
  double c1r, c1g, c1b; // color1 {r,g,b}
  double c2r, c2g, c2b; // color2 {r,g,b}
} MlVfx;

typedef struct {
  int snd_count;
  const char *snd[ML_EV_CAP];
  int dsp_count;
  char dsp[ML_EV_CAP][ML_EV_DSP_LEN];
  int rng_count;
  double rng[ML_EV_CAP];
  int vfx_count;
  MlVfx vfx[ML_EV_CAP];
} MlEvents;

extern MlEvents ml_events;

// SOUND SINK (M3 task 6): OPTIONAL tap on the sound-event plane. The
// integrated sim resets the ml_events queues at every tick stage
// (sim_tick.c), so a per-frame consumer cannot read the queue after the
// tick — instead this ONE chokepoint forwards each sound event to a
// registered sink AT ENQUEUE TIME (ml_sound_play / ml_sound_stop; stop
// events arrive with their ".stop"-suffixed token). Default NULL =
// behavior byte-identical to the pre-task-6 sim (every replay rig and
// sim binary leaves it unset). The sink must be READ-ONLY with respect
// to sim state: it may not draw RNG, may not touch players/stage, and
// runs on the sim thread (the gfx_app mixer locks its own audio state).
extern void (*ml_snd_sink)(const char *name);

// VFX SINK (M4 task 1): the ml_snd_sink twin for the vfx plane. The
// integrated sim resets the ml_events queues at every tick stage, so the
// M4 renderer consumes vfx events through this enqueue-time chokepoint
// instead of reading the queue post-tick. Default NULL = behavior
// byte-identical to the pre-task-1 sim (every replay rig leaves it
// unset). Same contract as ml_snd_sink: READ-ONLY wrt sim state, no RNG,
// runs on the sim thread. The cfg pointer is valid only for the call.
extern void (*ml_vfx_sink)(const MlVfx *cfg);

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

// --- drawVfx (src/main/vfx/drawVfx.js), structure-parallel ---------------
// ml_drawVfx_cfg is the ONE central mirror of upstream drawVfx: it
// enqueues the config snapshot and, for name "circleDust" ONLY, consumes
// the 4 seeded draws (drawVfx.js:15-18 — the values are render-only, the
// DRAWS are chain state; order preserved from the M2 mv_drawVfx). The
// shape emitters below cover every measured call-expression key set —
// each C call site passes its upstream expression's values verbatim
// (rule 6/13):
void ml_drawVfx_cfg(const MlVfx *cfg);
// {name, pos}
void ml_drawVfx_p(const char *name, double x, double y);
// {name, pos, face}
void ml_drawVfx(const char *name, double x, double y, double face);
// {name, pos, face, f: number}
void ml_drawVfx_f(const char *name, double x, double y, double face,
                  double f);
// {name, pos, face, f: Vec2D} (wall/ceiling normals)
void ml_drawVfx_fv(const char *name, double x, double y, double face,
                   double fx, double fy);
// {name, pos, face, f: {pNum, swingType, frame}} (marth swing trails)
void ml_drawVfx_swing(const char *name, double x, double y, double face,
                      double pNum, const char *swingType, double frame);
// {name, pos, face, f: number, color1: {r,g,b}, color2: {r,g,b}} (lasers)
void ml_drawVfx_laser(const char *name, double x, double y, double face,
                      double f, double c1r, double c1g, double c1b,
                      double c2r, double c2g, double c2b);
double ml_random(void);

// Provided by the host (replay driver / future sim harness): fatal report.
void ml_events_fail(const char *what);

#endif // ML_EVENTS_H
