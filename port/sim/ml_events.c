// ml_events.c — sound-event / dispatch-event queues + the seeded-RNG draw
// seam (see ml_events.h).
#include "ml_events.h"

#include <stdio.h>

#include <string.h>

MlEvents ml_events;
MlRng *ml_active_rng = 0;
void (*ml_snd_sink)(const char *name) = 0;   // M3 task 6 (header note)
void (*ml_vfx_sink)(const MlVfx *cfg) = 0;   // M4 task 1 (header note)

void ml_ev_reset(void) {
  ml_events.snd_count = 0;
  ml_events.dsp_count = 0;
  ml_events.rng_count = 0;
  ml_events.vfx_count = 0;
}

// The ONE structure-parallel mirror of upstream drawVfx (drawVfx.js):
// enqueue the config snapshot, then "circleDust"'s 4 seeded draws
// (drawVfx.js:15-18; values render-only, draws are chain state).
void ml_drawVfx_cfg(const MlVfx *cfg) {
  if (ml_events.vfx_count >= ML_EV_CAP) ml_events_fail("vfx queue overflow");
  ml_events.vfx[ml_events.vfx_count++] = *cfg;
  if (ml_vfx_sink) ml_vfx_sink(cfg);
  if (strcmp(cfg->name, "circleDust") == 0) {
    (void)ml_random();
    (void)ml_random();
    (void)ml_random();
    (void)ml_random();
  }
}

void ml_drawVfx_p(const char *name, double x, double y) {
  MlVfx v;
  memset(&v, 0, sizeof v);
  v.name = name;
  v.px = x;
  v.py = y;
  ml_drawVfx_cfg(&v);
}

void ml_drawVfx(const char *name, double x, double y, double face) {
  MlVfx v;
  memset(&v, 0, sizeof v);
  v.name = name;
  v.px = x;
  v.py = y;
  v.has_face = 1;
  v.face = face;
  ml_drawVfx_cfg(&v);
}

void ml_drawVfx_f(const char *name, double x, double y, double face,
                  double f) {
  MlVfx v;
  memset(&v, 0, sizeof v);
  v.name = name;
  v.px = x;
  v.py = y;
  v.has_face = 1;
  v.face = face;
  v.f_kind = ML_VFX_F_NUM;
  v.f_num = f;
  ml_drawVfx_cfg(&v);
}

void ml_drawVfx_fv(const char *name, double x, double y, double face,
                   double fx, double fy) {
  MlVfx v;
  memset(&v, 0, sizeof v);
  v.name = name;
  v.px = x;
  v.py = y;
  v.has_face = 1;
  v.face = face;
  v.f_kind = ML_VFX_F_VEC;
  v.f_x = fx;
  v.f_y = fy;
  ml_drawVfx_cfg(&v);
}

void ml_drawVfx_swing(const char *name, double x, double y, double face,
                      double pNum, const char *swingType, double frame) {
  MlVfx v;
  memset(&v, 0, sizeof v);
  v.name = name;
  v.px = x;
  v.py = y;
  v.has_face = 1;
  v.face = face;
  v.f_kind = ML_VFX_F_SWING;
  v.f_pnum = pNum;
  v.f_swing = swingType;
  v.f_frame = frame;
  ml_drawVfx_cfg(&v);
}

void ml_drawVfx_laser(const char *name, double x, double y, double face,
                      double f, double c1r, double c1g, double c1b,
                      double c2r, double c2g, double c2b) {
  MlVfx v;
  memset(&v, 0, sizeof v);
  v.name = name;
  v.px = x;
  v.py = y;
  v.has_face = 1;
  v.face = face;
  v.f_kind = ML_VFX_F_NUM;
  v.f_num = f;
  v.has_color = 1;
  v.c1r = c1r;
  v.c1g = c1g;
  v.c1b = c1b;
  v.c2r = c2r;
  v.c2g = c2g;
  v.c2b = c2b;
  ml_drawVfx_cfg(&v);
}

void ml_sound_play(const char *name) {
  if (ml_events.snd_count >= ML_EV_CAP) ml_events_fail("sound queue overflow");
  ml_events.snd[ml_events.snd_count++] = name;
  if (ml_snd_sink) ml_snd_sink(name);
}

void ml_sound_stop(const char *nameDotStop) {
  if (ml_events.snd_count >= ML_EV_CAP) ml_events_fail("sound queue overflow");
  ml_events.snd[ml_events.snd_count++] = nameDotStop;
  if (ml_snd_sink) ml_snd_sink(nameDotStop);
}

void ml_dispatch_note(const char *phase, const char *move) {
  if (ml_events.dsp_count >= ML_EV_CAP) ml_events_fail("dispatch queue overflow");
  int n = snprintf(ml_events.dsp[ml_events.dsp_count], ML_EV_DSP_LEN, "%s:%s",
                   phase, move);
  if (n < 0 || n >= ML_EV_DSP_LEN) ml_events_fail("dispatch label overflow");
  ml_events.dsp_count++;
}

double ml_random(void) {
  if (!ml_active_rng) ml_events_fail("ml_random: no active RNG");
  double v = ml_rng_next(ml_active_rng);
  if (ml_events.rng_count >= ML_EV_CAP) ml_events_fail("rng log overflow");
  ml_events.rng[ml_events.rng_count++] = v;
  return v;
}
