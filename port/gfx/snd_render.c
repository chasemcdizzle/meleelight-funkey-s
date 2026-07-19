// port/gfx/snd_render.c — OFFLINE deterministic mixer render (M4 task 6).
//
// Renders a recorded sound-event schedule (snd_events_tap.c grammar)
// through the snd_mixer.h math VERBATIM — the same header the live
// gfx_app compiles — into raw 44100 Hz stereo S16LE PCM, with no SDL and
// no realtime: per sim frame, all of that frame's events are applied
// (plays via snd_event, stops via snd_event_stop_id), then exactly 735
// output frames are mixed (44100/60 — exact). Deterministic by
// construction (integer-only mixer): two runs are byte-identical, and
// the differential check compares the output against the INDEPENDENT
// reference implementation (snd_reference.js) byte-for-byte.
//
//   snd_render --pack p.bin --events e.txt --frames N --out pcm.raw
//
// Verdict grammar (load-bearing; whitelist rule):
//   snd-render OK frames=<n> plays=<n> stops=<n> steals=<n>
//     maxvoices=<n> bytes=<n>
// (one line, one trailing newline).
//
// TEST-ONLY tooth seams (negative testing, the pack --drop-name
// precedent; the gate path never passes them — the differential would
// fail anyway, which is exactly what the teeth prove):
//   --tooth-gain <name>       gainQ8+1 for one entry after load
//   --tooth-drop-first-stop   ignore the schedule's first stop event
//   --tooth-step-skew         resample step + 1 (16.16 ulp)
//   --tooth-stop-id-skew      +1 on every carried stop id (howler
//                             stale-id no-op semantics -> loops ring on)
//   --tooth-steal-newest      steal policy flip (highest seq wins)
//
// The schedule parser is STRICT full-line (PROCESS §3): every line must
// match the tap grammar exactly, the SNDEV OK terminator with matching
// counts is mandatory, and lastFrame must equal --frames (a schedule
// from a partial run can never render as complete).
//
// PROVENANCE: output PCM derives from Nintendo-derived blobs — PRIVATE
// USE ONLY, gitignored build output, never committed.
#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void sim_fatal(const char *what) {
  fprintf(stderr, "snd_render FATAL: %s\n", what);
  exit(2);
}

#include "snd_mixer.h"

#define OUT_PER_FRAME 735 // 44100 / 60, exact

typedef struct {
  long frame;
  char name[SND_NAME_LEN + 8]; // base name or "<base>.stop"
  int isStop;
  int hasId;
  double id;
} Ev;

static Ev *g_ev;
static size_t g_ev_len, g_ev_cap;
static unsigned long long g_plays, g_stops;

static void ev_push(const Ev *e) {
  if (g_ev_len == g_ev_cap) {
    g_ev_cap = g_ev_cap ? g_ev_cap * 2 : 4096;
    g_ev = realloc(g_ev, g_ev_cap * sizeof *g_ev);
    if (!g_ev) sim_fatal("oom");
  }
  g_ev[g_ev_len++] = *e;
}

// strict token helpers ------------------------------------------------------
static bool tok_long(const char *s, long *out) {
  if (*s < '0' || *s > '9') return false;
  char *end = 0;
  const long v = strtol(s, &end, 10);
  if (end == s || *end != 0) return false;
  *out = v;
  return true;
}

static bool tok_name(const char *s, bool allowStop, char *out, size_t cap) {
  // [0-9A-Za-z]+ optionally followed by the literal ".stop"
  size_t n = 0;
  while ((s[n] >= '0' && s[n] <= '9') || (s[n] >= 'a' && s[n] <= 'z') ||
         (s[n] >= 'A' && s[n] <= 'Z')) {
    n++;
  }
  if (n == 0) return false;
  size_t total = n;
  if (s[n] != 0) {
    if (!allowStop || strcmp(s + n, ".stop") != 0) return false;
    total = n + 5;
  } else if (allowStop) {
    return false; // stop record must carry the .stop suffix
  }
  if (total + 1 > cap) return false;
  memcpy(out, s, total);
  out[total] = 0;
  return true;
}

static void load_events(const char *path, long frames) {
  FILE *f = fopen(path, "r");
  if (!f) sim_fatal("cannot open events file");
  char line[512];
  bool sawTerm = false;
  unsigned long long plays = 0, stops = 0;
  while (fgets(line, sizeof line, f)) {
    size_t n = strlen(line);
    if (n == 0 || line[n - 1] != '\n') {
      sim_fatal("events: unterminated line (truncated schedule)");
    }
    line[--n] = 0;
    if (sawTerm) sim_fatal("events: bytes after the terminator");
    if (strncmp(line, "SNDEV OK ", 9) == 0) {
      unsigned long long tp, ts;
      long lastFrame;
      char tail;
      if (sscanf(line, "SNDEV OK plays=%llu stops=%llu lastFrame=%ld%c",
                 &tp, &ts, &lastFrame, &tail) != 3) {
        sim_fatal("events: malformed terminator line");
      }
      if (tp != plays || ts != stops) {
        sim_fatal("events: terminator counts disagree with the lines");
      }
      if (lastFrame != frames) {
        sim_fatal("events: lastFrame != --frames (partial-run schedule)");
      }
      sawTerm = true;
      continue;
    }
    Ev e;
    memset(&e, 0, sizeof e);
    if (line[0] == 'P' && line[1] == ' ') {
      // P <frame> <name>
      char fnum[32], nm[64];
      char tail[2];
      if (sscanf(line + 2, "%31s %63s%1s", fnum, nm, tail) != 2) {
        sim_fatal("events: malformed P line");
      }
      if (!tok_long(fnum, &e.frame)) sim_fatal("events: bad P frame");
      if (!tok_name(nm, false, e.name, sizeof e.name)) {
        sim_fatal("events: bad P name");
      }
      e.isStop = 0;
      plays++;
    } else if (line[0] == 'S' && line[1] == ' ') {
      // S <frame> <token> <hasId> <idbits16>
      char fnum[32], nm[64], has[8], bits[32];
      char tail[2];
      if (sscanf(line + 2, "%31s %63s %7s %31s%1s", fnum, nm, has, bits,
                 tail) != 4) {
        sim_fatal("events: malformed S line");
      }
      if (!tok_long(fnum, &e.frame)) sim_fatal("events: bad S frame");
      if (!tok_name(nm, true, e.name, sizeof e.name)) {
        sim_fatal("events: bad S token");
      }
      if (strcmp(has, "0") == 0) e.hasId = 0;
      else if (strcmp(has, "1") == 0) e.hasId = 1;
      else sim_fatal("events: bad S hasId");
      if (strlen(bits) != 16) sim_fatal("events: bad S idbits length");
      uint64_t v = 0;
      for (int k = 0; k < 16; k++) {
        const char c = bits[k];
        v <<= 4;
        if (c >= '0' && c <= '9') v |= (uint64_t)(c - '0');
        else if (c >= 'a' && c <= 'f') v |= (uint64_t)(c - 'a' + 10);
        else sim_fatal("events: bad S idbits digit");
      }
      memcpy(&e.id, &v, 8);
      e.isStop = 1;
      stops++;
    } else {
      sim_fatal("events: unrecognized line (grammar violation)");
    }
    if (e.frame < 0 || e.frame > frames) {
      sim_fatal("events: frame outside the run");
    }
    if (g_ev_len > 0 && e.frame < g_ev[g_ev_len - 1].frame) {
      sim_fatal("events: frames not monotone");
    }
    ev_push(&e);
  }
  fclose(f);
  if (!sawTerm) sim_fatal("events: missing SNDEV OK terminator");
  g_plays = plays;
  g_stops = stops;
}

int main(int argc, char **argv) {
  const char *packPath = 0, *eventsPath = 0, *outPath = 0;
  const char *toothGain = 0;
  long frames = -1;
  bool toothDropFirstStop = false, toothStepSkew = false;
  bool toothStopIdSkew = false, toothStealNewest = false;
  for (int i = 1; i < argc; i++) {
    const char *a = argv[i];
    const bool hasV = i + 1 < argc;
    if (strcmp(a, "--pack") == 0 && hasV) packPath = argv[++i];
    else if (strcmp(a, "--events") == 0 && hasV) eventsPath = argv[++i];
    else if (strcmp(a, "--frames") == 0 && hasV) frames = strtol(argv[++i], 0, 10);
    else if (strcmp(a, "--out") == 0 && hasV) outPath = argv[++i];
    else if (strcmp(a, "--tooth-gain") == 0 && hasV) toothGain = argv[++i];
    else if (strcmp(a, "--tooth-drop-first-stop") == 0) toothDropFirstStop = true;
    else if (strcmp(a, "--tooth-step-skew") == 0) toothStepSkew = true;
    else if (strcmp(a, "--tooth-stop-id-skew") == 0) toothStopIdSkew = true;
    else if (strcmp(a, "--tooth-steal-newest") == 0) toothStealNewest = true;
    else {
      fprintf(stderr, "snd_render: bad argument %s\n", a);
      return 1;
    }
  }
  if (!packPath || !eventsPath || !outPath || frames <= 0) {
    fprintf(stderr, "usage: snd_render --pack p.bin --events e.txt "
                    "--frames N --out pcm.raw [tooth flags]\n");
    return 1;
  }

  static SndMixer mix;
  snd_pack_load(&mix, packPath);
  if (toothGain) {
    SndEntry *e = (SndEntry *)(uintptr_t)snd_find(&mix, toothGain);
    if (!e) sim_fatal("--tooth-gain name not in the pack");
    e->gainQ8 = (uint16_t)(e->gainQ8 + 1);
  }
  if (toothStepSkew) mix.step += 1;

  load_events(eventsPath, frames);

  FILE *out = fopen(outPath, "wb");
  if (!out) sim_fatal("cannot open --out");

  int16_t buf[OUT_PER_FRAME * 2];
  size_t evIdx = 0;
  bool droppedOne = false;
  uint64_t bytes = 0;
  for (long f = 0; f <= frames; f++) {
    // frame 0 = match-setup events (before the first mixed frame);
    // frames 1..N each mix 735 output frames AFTER their events land.
    while (evIdx < g_ev_len && g_ev[evIdx].frame == f) {
      const Ev *e = &g_ev[evIdx++];
      if (e->isStop) {
        if (toothDropFirstStop && !droppedOne) {
          droppedOne = true;
          continue;
        }
        double id = e->id;
        if (toothStopIdSkew && e->hasId) id += 1;
        snd_event_stop_id(&mix, e->name, e->hasId, id);
      } else {
        if (toothStealNewest) {
          // policy-flip tooth: emulate steal-newest by pre-checking; the
          // real snd_event is still used when a free slot exists.
          int freeSlot = -1;
          for (int v = 0; v < SND_VOICES; v++) {
            if (mix.voice[v].e == 0) { freeSlot = v; break; }
          }
          if (freeSlot < 0) {
            // deactivate the NEWEST (highest seq) so snd_event's
            // free-slot arm reuses it: net effect = steal-newest.
            uint64_t best = 0;
            int slot = 0;
            for (int v = 0; v < SND_VOICES; v++) {
              if (mix.voice[v].seq >= best) {
                best = mix.voice[v].seq;
                slot = v;
              }
            }
            mix.voice[slot].e = 0;
            mix.steals++; // count it as the steal it is
          }
        }
        snd_event(&mix, e->name);
      }
    }
    if (f == 0) continue; // setup events only; no audio before frame 1
    snd_mix_fill(&mix, buf, OUT_PER_FRAME);
    if (fwrite(buf, sizeof buf, 1, out) != 1) sim_fatal("PCM write failed");
    bytes += sizeof buf;
  }
  if (evIdx != g_ev_len) sim_fatal("events beyond the last frame");
  if (fclose(out) != 0) sim_fatal("PCM close failed");

  printf("snd-render OK frames=%ld plays=%llu stops=%llu steals=%" PRIu64
         " maxvoices=%" PRIu64 " bytes=%" PRIu64 "\n",
         frames, g_plays, g_stops, mix.steals, mix.maxVoices, bytes);
  return 0;
}
