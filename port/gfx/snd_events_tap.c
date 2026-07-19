// port/gfx/snd_events_tap.c — sound-event schedule recorder (M4 task 6).
//
// Linked ALONGSIDE the integrated headless sim (the sim_ai_live.c
// constructor pattern: no sim_main.c edit) to record every sound event a
// golden replay fires, with its frame and — for stops — the id the sim
// passed (ml_snd_stop_id_sink). The offline render differential consumes
// this schedule (snd_render.c / snd_reference.js).
//
// Schedule file grammar (whitelist rule, PROCESS §3 — both consumers
// parse it strictly, full-line):
//   P <frame> <name>
//   S <frame> <token> <hasId> <idbits16>
//   SNDEV OK plays=<n> stops=<n> lastFrame=<n>
// One line per event in enqueue order; <frame> = G.frame (0 during match
// setup, 1..N in the loop); <idbits16> = 16 lowercase hex digits of the
// IEEE-754 bit pattern of the id the sim passed (exact, no decimal
// round-trip); hasId 0|1. The terminator line is MANDATORY — a consumer
// that does not find it fails closed (truncated-write class).
// lastFrame = G.frame at exit: because this writes from an atexit hook,
// an early exit(≠0) still writes a terminated file — the CHECK asserts
// the producing run's rc==0 + STREAM MATCH AND that lastFrame equals
// the golden's frame count, so a partial schedule can never pose as
// complete.
//
// Control: env ML_SND_EVENTS_OUT names the output file. The env var is
// REQUIRED — this TU is only linked into the dedicated sim_host_snd
// build, and a tap that silently recorded nothing would be the classic
// silent-instrument hole. Events are buffered in RAM and written by an
// atexit hook (the sim's normal exit path; sim_fatal aborts skip the
// write and the missing terminator fails the consumer — fail closed).
//
// Sink contract honored: read-only wrt sim state, no RNG (ml_events.h).
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../sim/sim/sim.h" // GameState G (G.frame)
#include "../sim/ml_events.h"

typedef struct {
  long frame;
  const char *name; // string literal from the sim (static lifetime)
  int isStop;
  int hasId;
  double id;
} SndEvRec;

static SndEvRec *g_ev;
static size_t g_ev_len, g_ev_cap;
static const char *g_out_path;
static unsigned long long g_plays, g_stops;

static void tap_fail(const char *what) {
  fprintf(stderr, "snd_events_tap: %s\n", what);
  exit(1);
}

static void tap_push(const char *name, int isStop, int hasId, double id) {
  if (g_ev_len == g_ev_cap) {
    g_ev_cap = g_ev_cap ? g_ev_cap * 2 : 4096;
    g_ev = realloc(g_ev, g_ev_cap * sizeof *g_ev);
    if (!g_ev) tap_fail("oom");
  }
  SndEvRec *r = &g_ev[g_ev_len++];
  r->frame = G.frame;
  r->name = name;
  r->isStop = isStop;
  r->hasId = hasId;
  r->id = id;
  if (isStop) g_stops++;
  else g_plays++;
}

static void tap_play(const char *name) {
  const size_t n = strlen(name);
  if (n > 5 && strcmp(name + n - 5, ".stop") == 0) return; // id sink owns stops
  tap_push(name, 0, 0, 0);
}

static void tap_stop(const char *token, int hasId, double id) {
  tap_push(token, 1, hasId, id);
}

static void tap_write(void) {
  FILE *f = fopen(g_out_path, "w");
  if (!f) tap_fail("cannot open ML_SND_EVENTS_OUT for writing");
  for (size_t i = 0; i < g_ev_len; i++) {
    const SndEvRec *r = &g_ev[i];
    if (r->isStop) {
      uint64_t bits;
      memcpy(&bits, &r->id, 8);
      fprintf(f, "S %ld %s %d %016" PRIx64 "\n", r->frame, r->name,
              r->hasId, bits);
    } else {
      fprintf(f, "P %ld %s\n", r->frame, r->name);
    }
  }
  fprintf(f, "SNDEV OK plays=%llu stops=%llu lastFrame=%ld\n", g_plays,
          g_stops, G.frame);
  if (fclose(f) != 0) tap_fail("schedule write failed");
}

__attribute__((constructor)) static void tap_install(void) {
  g_out_path = getenv("ML_SND_EVENTS_OUT");
  if (!g_out_path || !g_out_path[0]) {
    tap_fail("ML_SND_EVENTS_OUT is required (this build exists to record "
             "the sound-event schedule; a silent no-tap run is the hole "
             "this guard closes)");
  }
  ml_snd_sink = tap_play;
  ml_snd_stop_id_sink = tap_stop;
  if (atexit(tap_write) != 0) tap_fail("atexit failed");
}
