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
//     [--music m.pcm --music-volbits <16hex> --music-start <off>,<dur>
//      --music-loop <off>,<dur>]
//
// MUSIC (M4 task 7): the snd_mixer.h music channel driven OFFLINE — the
// SAME ring/refill/fill code the device streams through, run
// synchronously (eager one-chunk refills whenever space >= chunk, ring
// pre-filled before frame 1, exactly the device policy). Window args
// are sounds.json sprite ms values VERBATIM; --music-volbits is the
// SND1 effective-volume IEEE-754 bit pattern (gainQ8 = round(v*256),
// the pack-snd.js formula). Offline may NEVER starve — asserted, and
// the verdict gains ` musout=<n> musstarves=<n> musrefills=<n>` fields
// ONLY when --music is given (the no-music grammar is byte-unchanged —
// check-mixer-fidelity.sh's pinned pattern parses exactly as before).
//
// Verdict grammar (load-bearing; whitelist rule):
//   snd-render OK frames=<n> plays=<n> stops=<n> stopsm=<n> stopsu=<n>
//     steals=<n> maxvoices=<n> bytes=<n>[ musout=<n> musstarves=<n>
//     musrefills=<n>]
// (one line, one trailing newline). stopsm/stopsu = the snd_mixer.h
// matched/unmatched stop-event split (M4 iter 84, review-82 H:
// stopsm + stopsu == stops; a stop event that deactivates no voice —
// howler stale-id/ended-voice no-op — is counted unmatched, so the
// check can pin the measured matched plane instead of inferring it
// from aggregate token counts).
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
//   --tooth-music-gain        music gainQ8+1
//   --tooth-music-loop-beg    music loopBeg+1 source frame (fires the
//                             moment the intro->loop chain lands)
//   --tooth-music-loop-dur    music loopDur+1 source frame (fires only
//                             past the loop WRAP — run on a wrap leg)
//   --tooth-music-underfill   refills withheld after the prefill ->
//                             starves > 0 reported AND output diverges
//                             (starve accounting is load-bearing)
//
// The schedule parser is STRICT full-line (PROCESS §3): every line must
// match the tap grammar exactly, the SNDEV OK terminator with matching
// counts is mandatory, and lastFrame must equal --frames (a schedule
// from a partial run can never render as complete). EXACT-TOKEN (iter
// 86, review-84 M): numerals are 0|[1-9][0-9]* (leading zeros refused)
// and fields are separated by EXACTLY one space with end-of-line
// asserted — sscanf's whitespace elasticity and strtol's leading-zero
// tolerance accepted resembling-but-nonmatching lines (`P 075`,
// `plays=060`), which the whitelist rule forbids; sscanf is gone.
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
// EXACT-TOKEN scanners (iter 86, review-84 M): each consumes from *ps
// and advances it; the caller asserts the exact delimiter that follows.
// scan_num accepts ONLY the tap grammar's numeral 0|[1-9][0-9]* —
// leading zeros, signs, and embedded whitespace are refusals, never
// value-preserving tolerance (PROCESS §3: resembles-but-doesn't-match
// = corruption).
static bool scan_num(const char **ps, unsigned long long *out) {
  const char *s = *ps;
  if (*s < '0' || *s > '9') return false;
  if (s[0] == '0' && s[1] >= '0' && s[1] <= '9') return false; // leading zero
  unsigned long long v = 0;
  while (*s >= '0' && *s <= '9') {
    if (v > (~0ull - 9ull) / 10ull) return false; // overflow guard
    v = v * 10ull + (unsigned long long)(*s - '0');
    s++;
  }
  *out = v;
  *ps = s;
  return true;
}

static bool scan_lit(const char **ps, const char *lit) {
  const size_t n = strlen(lit);
  if (strncmp(*ps, lit, n) != 0) return false;
  *ps += n;
  return true;
}

// [0-9A-Za-z]+ followed (allowStop) by the literal ".stop"; the caller
// asserts the delimiter after the consumed token.
static bool scan_name(const char **ps, bool allowStop, char *out,
                      size_t cap) {
  const char *s = *ps;
  size_t n = 0;
  while ((s[n] >= '0' && s[n] <= '9') || (s[n] >= 'a' && s[n] <= 'z') ||
         (s[n] >= 'A' && s[n] <= 'Z')) {
    n++;
  }
  if (n == 0) return false;
  size_t total = n;
  if (allowStop) {
    if (strncmp(s + n, ".stop", 5) != 0) return false;
    total = n + 5;
  }
  if (total + 1 > cap) return false;
  memcpy(out, s, total);
  out[total] = 0;
  *ps = s + total;
  return true;
}

// tap frames fit long comfortably; anything larger is corruption.
#define EV_FRAME_MAX 0x7fffffffull

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
    const char *s = line;
    if (scan_lit(&s, "SNDEV OK plays=")) {
      // SNDEV OK plays=<n> stops=<n> lastFrame=<n>   (exact tokens)
      unsigned long long tp = 0, ts = 0, lf = 0;
      if (!scan_num(&s, &tp) || !scan_lit(&s, " stops=") ||
          !scan_num(&s, &ts) || !scan_lit(&s, " lastFrame=") ||
          !scan_num(&s, &lf) || *s != 0) {
        sim_fatal("events: malformed terminator line (exact-token grammar)");
      }
      if (tp != plays || ts != stops) {
        sim_fatal("events: terminator counts disagree with the lines");
      }
      if (lf > EV_FRAME_MAX || (long)lf != frames) {
        sim_fatal("events: lastFrame != --frames (partial-run schedule)");
      }
      sawTerm = true;
      continue;
    }
    Ev e;
    memset(&e, 0, sizeof e);
    unsigned long long fr;
    if (line[0] == 'P' && line[1] == ' ') {
      // P <frame> <name>   (single-space fields, end-of-line asserted)
      s = line + 2;
      if (!scan_num(&s, &fr)) {
        sim_fatal("events: bad P frame (0|[1-9][0-9]* exactly)");
      }
      if (!scan_lit(&s, " ")) {
        sim_fatal("events: malformed P line (exactly one space between fields)");
      }
      if (!scan_name(&s, false, e.name, sizeof e.name)) {
        sim_fatal("events: bad P name");
      }
      if (*s != 0) sim_fatal("events: trailing bytes on a P line");
      if (fr > EV_FRAME_MAX) sim_fatal("events: P frame out of range");
      e.frame = (long)fr;
      e.isStop = 0;
      plays++;
    } else if (line[0] == 'S' && line[1] == ' ') {
      // S <frame> <base>.stop <0|1> <idbits16>   (exact tokens)
      s = line + 2;
      if (!scan_num(&s, &fr)) {
        sim_fatal("events: bad S frame (0|[1-9][0-9]* exactly)");
      }
      if (!scan_lit(&s, " ")) {
        sim_fatal("events: malformed S line (exactly one space between fields)");
      }
      if (!scan_name(&s, true, e.name, sizeof e.name)) {
        sim_fatal("events: bad S token");
      }
      if (!scan_lit(&s, " ")) {
        sim_fatal("events: malformed S line (exactly one space between fields)");
      }
      if (s[0] == '0' && s[1] == ' ') e.hasId = 0;
      else if (s[0] == '1' && s[1] == ' ') e.hasId = 1;
      else sim_fatal("events: bad S hasId (0|1 exactly)");
      s += 2;
      uint64_t v = 0;
      for (int k = 0; k < 16; k++) {
        const char c = s[k];
        v <<= 4;
        if (c >= '0' && c <= '9') v |= (uint64_t)(c - '0');
        else if (c >= 'a' && c <= 'f') v |= (uint64_t)(c - 'a' + 10);
        else sim_fatal("events: bad S idbits digit (16 lowercase hex exactly)");
      }
      if (s[16] != 0) sim_fatal("events: trailing bytes on an S line");
      memcpy(&e.id, &v, 8);
      if (fr > EV_FRAME_MAX) sim_fatal("events: S frame out of range");
      e.frame = (long)fr;
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

// --- offline music source (whole file in memory; SndMusicRead shape) --------
static int16_t *g_mus_pcm;
static uint64_t g_mus_frames;

static void mus_mem_read(void *ud, uint64_t fileFrame, int16_t *dst,
                         uint32_t frames) {
  (void)ud;
  if (fileFrame + frames > g_mus_frames) {
    sim_fatal("music: read past the loaded PCM (program/segmentation bug)");
  }
  memcpy(dst, g_mus_pcm + fileFrame * 2, (size_t)frames * 4);
}

// strict "<off>,<dur>" ms-pair parse (exact tokens, the scan_num grammar)
static void parse_ms_pair(const char *s, uint64_t *off, uint64_t *dur) {
  if (!scan_num(&s, off) || !scan_lit(&s, ",") || !scan_num(&s, dur) ||
      *s != 0) {
    sim_fatal("music: bad <offMs>,<durMs> pair (0|[1-9][0-9]* exactly)");
  }
}

int main(int argc, char **argv) {
  const char *packPath = 0, *eventsPath = 0, *outPath = 0;
  const char *toothGain = 0;
  const char *musicPath = 0, *musicVolBits = 0;
  const char *musicStart = 0, *musicLoop = 0;
  long frames = -1;
  bool toothDropFirstStop = false, toothStepSkew = false;
  bool toothStopIdSkew = false, toothStealNewest = false;
  bool toothMusGain = false, toothMusLoopBeg = false, toothMusLoopDur = false;
  bool toothMusUnderfill = false;
  for (int i = 1; i < argc; i++) {
    const char *a = argv[i];
    const bool hasV = i + 1 < argc;
    if (strcmp(a, "--pack") == 0 && hasV) packPath = argv[++i];
    else if (strcmp(a, "--events") == 0 && hasV) eventsPath = argv[++i];
    else if (strcmp(a, "--frames") == 0 && hasV) frames = strtol(argv[++i], 0, 10);
    else if (strcmp(a, "--out") == 0 && hasV) outPath = argv[++i];
    else if (strcmp(a, "--music") == 0 && hasV) musicPath = argv[++i];
    else if (strcmp(a, "--music-volbits") == 0 && hasV) musicVolBits = argv[++i];
    else if (strcmp(a, "--music-start") == 0 && hasV) musicStart = argv[++i];
    else if (strcmp(a, "--music-loop") == 0 && hasV) musicLoop = argv[++i];
    else if (strcmp(a, "--tooth-gain") == 0 && hasV) toothGain = argv[++i];
    else if (strcmp(a, "--tooth-drop-first-stop") == 0) toothDropFirstStop = true;
    else if (strcmp(a, "--tooth-step-skew") == 0) toothStepSkew = true;
    else if (strcmp(a, "--tooth-stop-id-skew") == 0) toothStopIdSkew = true;
    else if (strcmp(a, "--tooth-steal-newest") == 0) toothStealNewest = true;
    else if (strcmp(a, "--tooth-music-gain") == 0) toothMusGain = true;
    else if (strcmp(a, "--tooth-music-loop-beg") == 0) toothMusLoopBeg = true;
    else if (strcmp(a, "--tooth-music-loop-dur") == 0) toothMusLoopDur = true;
    else if (strcmp(a, "--tooth-music-underfill") == 0) toothMusUnderfill = true;
    else {
      fprintf(stderr, "snd_render: bad argument %s\n", a);
      return 1;
    }
  }
  const bool music = musicPath != 0;
  if (!packPath || !eventsPath || !outPath || frames <= 0 ||
      (music != (musicVolBits != 0)) || (music != (musicStart != 0)) ||
      (music != (musicLoop != 0)) ||
      (!music && (toothMusGain || toothMusLoopBeg || toothMusLoopDur ||
                  toothMusUnderfill))) {
    fprintf(stderr, "usage: snd_render --pack p.bin --events e.txt "
                    "--frames N --out pcm.raw [--music m.pcm "
                    "--music-volbits h16 --music-start o,d "
                    "--music-loop o,d] [tooth flags]\n");
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

  if (music) {
    // volbits: EXACTLY 16 lowercase hex digits -> IEEE-754 double ->
    // gainQ8 = round(v*256) (the pack-snd.js formula; v in [0,1]).
    uint64_t vb = 0;
    for (int k = 0; k < 16; k++) {
      const char c = musicVolBits[k];
      vb <<= 4;
      if (c >= '0' && c <= '9') vb |= (uint64_t)(c - '0');
      else if (c >= 'a' && c <= 'f') vb |= (uint64_t)(c - 'a' + 10);
      else sim_fatal("music: bad --music-volbits digit (16 lowercase hex)");
    }
    if (musicVolBits[16] != 0) sim_fatal("music: --music-volbits too long");
    double vol;
    memcpy(&vol, &vb, 8);
    if (!(vol >= 0.0 && vol <= 1.0)) sim_fatal("music: volume outside [0,1]");
    uint64_t so, sd, lo, ld;
    parse_ms_pair(musicStart, &so, &sd);
    parse_ms_pair(musicLoop, &lo, &ld);
    FILE *mf = fopen(musicPath, "rb");
    if (!mf) sim_fatal("music: cannot open --music PCM");
    if (fseek(mf, 0, SEEK_END) != 0) sim_fatal("music: seek failed");
    const long msz = ftell(mf);
    if (msz <= 0) sim_fatal("music: empty PCM");
    if (fseek(mf, 0, SEEK_SET) != 0) sim_fatal("music: seek failed");
    g_mus_pcm = malloc((size_t)msz);
    if (!g_mus_pcm) sim_fatal("music: oom (PCM)");
    if (fread(g_mus_pcm, 1, (size_t)msz, mf) != (size_t)msz) {
      sim_fatal("music: short PCM read");
    }
    fclose(mf);
    snd_music_cfg(&mix.music, (uint16_t)(vol * 256.0 + 0.5), so, sd, lo, ld,
                  (uint64_t)msz);
    g_mus_frames = mix.music.fileFrames;
    if (toothMusGain) mix.music.gainQ8 = (uint16_t)(mix.music.gainQ8 + 1);
    if (toothMusLoopBeg) mix.music.loopBeg += 1;
    if (toothMusLoopDur) mix.music.loopDur += 1;
    // PRE-FILL the whole ring (the device policy: synchronous, before
    // any consumption), then refill one chunk whenever space >= chunk.
    snd_music_fill(&mix.music, 0, SND_MUSIC_RING_FRAMES, mus_mem_read, 0);
    mix.music.wr = SND_MUSIC_RING_FRAMES;
  }

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
    // MUSIC refill (M4 task 7): the device reader-thread policy run
    // synchronously — one chunk whenever free space >= chunk. The
    // underfill tooth withholds every refill after the prefill.
    if (music && !toothMusUnderfill) {
      for (;;) {
        const uint64_t cons = mix.music.outPos >> 1;
        const uint64_t buffered = mix.music.wr - cons;
        if (SND_MUSIC_RING_FRAMES - buffered < SND_MUSIC_CHUNK_FRAMES) break;
        snd_music_fill(&mix.music, mix.music.wr, SND_MUSIC_CHUNK_FRAMES,
                       mus_mem_read, 0);
        mix.music.wr += SND_MUSIC_CHUNK_FRAMES;
        mix.music.refills++;
      }
    }
    snd_mix_fill(&mix, buf, OUT_PER_FRAME);
    if (fwrite(buf, sizeof buf, 1, out) != 1) sim_fatal("PCM write failed");
    bytes += sizeof buf;
  }
  if (evIdx != g_ev_len) sim_fatal("events beyond the last frame");
  if (fclose(out) != 0) sim_fatal("PCM close failed");

  if (mix.stopsMatched + mix.stopsUnmatched != mix.stops) {
    sim_fatal("stop split does not sum to the stop total");
  }
  if (music && !toothMusUnderfill && mix.music.starves != 0) {
    sim_fatal("offline music render starved (the eager refill loop can "
              "never starve — ring/refill bug)");
  }
  printf("snd-render OK frames=%ld plays=%llu stops=%llu stopsm=%" PRIu64
         " stopsu=%" PRIu64 " steals=%" PRIu64 " maxvoices=%" PRIu64
         " bytes=%" PRIu64,
         frames, g_plays, g_stops, mix.stopsMatched, mix.stopsUnmatched,
         mix.steals, mix.maxVoices, bytes);
  if (music) {
    // music fields ONLY with --music: the no-music verdict grammar is
    // byte-unchanged (check-mixer-fidelity.sh's pinned full-line
    // pattern parses exactly as before).
    printf(" musout=%" PRIu64 " musstarves=%" PRIu64 " musrefills=%" PRIu64,
           mix.music.outPos, mix.music.starves, mix.music.refills);
  }
  printf("\n");
  return 0;
}
