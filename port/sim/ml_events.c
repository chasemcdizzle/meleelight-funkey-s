// ml_events.c — sound-event / dispatch-event queues + the seeded-RNG draw
// seam (see ml_events.h).
#include "ml_events.h"

#include <stdio.h>

MlEvents ml_events;
MlRng *ml_active_rng = 0;
void (*ml_snd_sink)(const char *name) = 0; // M3 task 6 (header note)

void ml_ev_reset(void) {
  ml_events.snd_count = 0;
  ml_events.dsp_count = 0;
  ml_events.rng_count = 0;
  ml_events.vfx_count = 0;
}

void ml_vfx(const char *name) {
  if (ml_events.vfx_count >= ML_EV_CAP) ml_events_fail("vfx queue overflow");
  ml_events.vfx[ml_events.vfx_count++] = name;
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
