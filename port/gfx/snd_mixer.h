// port/gfx/snd_mixer.h — SNDPACK1 loader + the 8-voice SFX mixer
// (M3 task 6). HEADER-ONLY on purpose (the s1_input.h precedent —
// task-4's reviewed TU lists stay unchanged): included by gfx_app.c
// only. Requires sim_fatal() (sim.h) to be declared before inclusion.
//
// MIXER = the audio-spike measured variant VERBATIM in its math
// (spikes/device-audio/audiotest.c fill_mix; docs/research/
// audio-spike.md §3): 8 voices of 22050 Hz MONO S16 source, 16.16
// fixed-point phase-accumulator resample to the 44100 Hz output rate
// (step = (22050<<16)/44100 = 0x8000), per-voice Q8 gain
// (sv = (s * gainQ8) >> 8), int32 accumulate, clamp to S16, mono voice
// on both stereo channels. Differences from the spike are the parts the
// spike deliberately did not model: real blobs instead of synthesized
// tones, non-looping voices that END (phase past the last sample
// deactivates unless the SND1 loop flag is set), and allocation.
//
// VOICE-ALLOCATION POLICY (documented per the task brief; provenance
// verified in AGENT-LOG iter 57): the spike mixer has NO allocation
// policy — its 8 voices are static loops that never start or stop, so
// "the spike default" does not exist. Adopted policy:
//   STEAL-OLDEST-BY-START-SEQUENCE — a play event takes any inactive
//   voice; when all 8 are busy, the voice with the LOWEST start
//   sequence number (the longest-running one, looping or not) is
//   retired mid-sample and reused. Steals are counted and reported.
// A ".stop"-suffixed event token (ml_events contract: e.g.
// "furaloop.stop") deactivates ALL voices of the base name when it
// arrives WITHOUT an id (snd_event — howler 2.0.12 stop(undefined)
// semantics). M4 task 6: stops that carry a play id
// (snd_event_stop_id, fed by ml_snd_stop_id_sink) are ID-ROUTED —
// exactly howler 2.0.12 stop(id): a matching active voice stops, a
// stale/unknown id is a NO-OP. Voice play-ids are 1000 + play-event
// count (howler's global `++Howler._counter` parallel, and the SAME
// derivation ml_events.c's ml_howl_play_id uses) — the sim-stored id
// and the mixer's voice id agree with no back-channel because both
// count the same event stream.
//
// THREADING: mix state is mutated by the main thread (snd_event) and
// read/advanced by the audio callback (snd_mix_fill). The caller MUST
// bracket snd_event with platform_audio_lock()/unlock(); on the
// headless backend those are no-ops and no callback thread exists, so
// the bookkeeping stays deterministic (host truth for event counts).
//
// SNDPACK1 (produced by port/gfx/pack-snd.js from the pipeline audio
// stage's SND1 artifacts — sounds.json + audio/sfx/*.pcm; all little-
// endian per FORMATS.md §0):
//   offset 0  : magic "SNDPACK1" (8 bytes)
//   offset 8  : u32 count (sound entries)
//   offset 12 : u32 dataBytes (PCM region size)
//   offset 16 : count x 36-byte records, sorted strictly ascending by
//               name (byte order), each:
//                 name[24]  NUL-terminated, [0-9A-Za-z]+ only
//                 u32 offset   (bytes into the PCM region, even)
//                 u32 samples  (S16 mono sample count)
//                 u16 gainQ8   (round(SND1 effective volume * 256), <= 256)
//                 u8  loop     (0|1 — SND1 loop flag)
//                 u8  pad      (0)
//   then      : dataBytes of concatenated 22050 Hz mono S16LE PCM
// File size must equal 16 + count*36 + dataBytes EXACTLY. Every
// structural violation is a LOUD load death (the "dropped SND1 blob"
// tooth class); a runtime lookup of a name not in the pack is a loud
// death too — the sim's sound plane and the SND1 map must agree.
//
// PROVENANCE: pack contents are Nintendo-derived (CLAUDE.md / FORMATS
// §5) — PRIVATE USE ONLY, gitignored build output, pushed only to
// device scratch, never committed.
#ifndef GFX_SND_MIXER_H
#define GFX_SND_MIXER_H

#include <fcntl.h> // POSIX_FADV_* for the music reader's cache advice
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SND_NAME_LEN 24
#define SND_VOICES 8
#define SND_SRC_RATE 22050
#define SND_OUT_RATE 44100

// --- MUSIC channel (M4 task 7) ---------------------------------------------
// A DEDICATED stereo channel (not an SFX voice — matching the browser,
// where music is a separate Howl outside any voice pool). Source: the
// M1 audio stage's 22050 Hz STEREO S16LE PCM streamed via a ring of
// SND_MUSIC_RING_FRAMES source frames = PLAN §7's 2x64 KB double-buffer
// (refill unit = one SND_MUSIC_CHUNK_FRAMES chunk = 64 KB whenever free
// space >= chunk). PORTABILITY Layer 2: both constants are FunKey-tuned
// (measured fit: 88.2 KB/s consumption -> 0.743 s per half; re-measure
// per target/storage).
//
// SPRITE PROGRAM (upstream music.js semantics, pinned from the vendored
// howler 2.0.12 source + the authored onend handlers — AGENT-LOG iter
// 87 pre-registration): play the Start window once, then the Loop
// window repeating. Windows come from sounds.json sprite ms values via
// the DOCUMENTED quantization frames = floor(ms*441/20) (22050/1000 =
// 441/20; boundaries [floor(off), floor(off+dur)) so adjacent windows
// are gapless). A window index past the decoded file is SILENCE (the
// fod quirk carried verbatim: howler's sprite timer counts duration
// regardless of file length — the html5 element just runs dry).
// Resample: zero-order hold 2x (source frame = outFrame >> 1), Q8 gain
// per channel, summed with the SFX accumulator BEFORE the single S16
// clamp. With the channel disabled the fill math is BYTE-IDENTICAL to
// the pre-music mixer (accL == accR == the old mono sum).
//
// THREADING (device): a reader thread produces (fills ring slots >= the
// consumer position and publishes `wr` under platform_audio_lock); the
// audio callback consumes (runs with SDL's audio mutex held, so wr is
// never torn). Offline (snd_render) drives the SAME fill code
// synchronously. A starve (callback needs a source frame not yet
// published) emits silence for that output frame and counts — time
// advances (a starve drops a window, never stretches the track).
#define SND_MUSIC_RING_FRAMES 32768u  /* 2 x 64 KiB halves (PLAN §7) */
#define SND_MUSIC_CHUNK_FRAMES 16384u /* one 64 KiB refill read */

typedef struct {
  int on;
  uint16_t gainQ8;      // round(SND1 effective volume * 256)
  uint64_t startBeg;    // source frames (quantized sprite windows)
  uint64_t startDur;
  uint64_t loopBeg;
  uint64_t loopDur;     // > 0 (validated)
  uint64_t fileFrames;  // stereo frames actually present in the PCM
  int16_t *ring;        // SND_MUSIC_RING_FRAMES stereo frames
  uint64_t wr;          // source-timeline frames produced (published)
  uint64_t outPos;      // 44100 output frames consumed
  uint64_t starves;     // output frames emitted silent for lack of data
  uint64_t refills;     // post-prefill chunk refills performed
} SndMusic;

// ms -> source frames: the documented quantization (header note).
static uint64_t snd_music_ms_to_frames(uint64_t ms) {
  return ms * 441ull / 20ull;
}

// Configure + validate the music channel. Window args are the
// sounds.json sprite ms values VERBATIM; fileBytes is the PCM file's
// byte length. Any inconsistency dies loud.
static void snd_music_cfg(SndMusic *mu, uint16_t gainQ8, uint64_t startOffMs,
                          uint64_t startDurMs, uint64_t loopOffMs,
                          uint64_t loopDurMs, uint64_t fileBytes) {
  memset(mu, 0, sizeof *mu);
  if (gainQ8 > 256) sim_fatal("music: gainQ8 > 256 (volume > 1)");
  if (startOffMs > 1000000000ull || startDurMs > 1000000000ull ||
      loopOffMs > 1000000000ull || loopDurMs > 1000000000ull) {
    sim_fatal("music: sprite window ms out of the sane domain");
  }
  if (fileBytes == 0 || fileBytes % 4 != 0) {
    sim_fatal("music: PCM byte length not a whole stereo S16 frame count");
  }
  mu->gainQ8 = gainQ8;
  mu->startBeg = snd_music_ms_to_frames(startOffMs);
  mu->startDur = snd_music_ms_to_frames(startOffMs + startDurMs) - mu->startBeg;
  mu->loopBeg = snd_music_ms_to_frames(loopOffMs);
  mu->loopDur = snd_music_ms_to_frames(loopOffMs + loopDurMs) - mu->loopBeg;
  if (mu->loopDur == 0) sim_fatal("music: empty loop window");
  mu->fileFrames = fileBytes / 4;
  mu->ring = malloc((size_t)SND_MUSIC_RING_FRAMES * 4);
  if (!mu->ring) sim_fatal("music: oom (ring)");
  mu->on = 1;
}

// Map source-timeline frame t to a contiguous run: *fileIdx is the file
// frame when audible; *silence = 1 for a program-silence run (window
// index past the file — the fod quirk). Returns 1..max frames.
static uint32_t snd_music_run(const SndMusic *mu, uint64_t t, uint32_t max,
                              uint64_t *fileIdx, int *silence) {
  uint64_t idx, remInWindow;
  if (t < mu->startDur) {
    idx = mu->startBeg + t;
    remInWindow = mu->startDur - t;
  } else {
    const uint64_t u = (t - mu->startDur) % mu->loopDur;
    idx = mu->loopBeg + u;
    remInWindow = mu->loopDur - u;
  }
  uint64_t run = remInWindow;
  if (idx >= mu->fileFrames) {
    *silence = 1;
    *fileIdx = 0;
  } else {
    *silence = 0;
    *fileIdx = idx;
    if (idx + run > mu->fileFrames) run = mu->fileFrames - idx; // EOF boundary
  }
  if (run > max) run = max;
  return (uint32_t)run;
}

// Reader callback: fill `frames` stereo frames from file frame
// `fileFrame` into dst (a contiguous file region — guaranteed by
// snd_music_fill's segmentation). Dies loud on any short read.
typedef void (*SndMusicRead)(void *ud, uint64_t fileFrame, int16_t *dst,
                             uint32_t frames);

// --- PCM page-cache advice (M4 task 14 increment 3b) -------------------------
// SD-SWAP PRESSURE CLASS, attributed the previous iteration: the device has
// 57 MB of RAM and its ONLY swap is a 128 MB swap FILE on the SD card, so
// growing page cache pushes anonymous pages out to that card, and the
// resulting SD-controller IRQ storms preempt the paced frame loop (skips
// co-occur with them 100% of the time; majflt is 0 throughout — we never
// fault back, the cost is preemption, not our paging).
//
// The advice lives HERE, beside the reader contract, rather than in either
// app: gfx_app.c and foh_dev.c carry the SAME file-backed SndMusicRead body
// and BOTH are built as ARM device binaries (riglib.sh builds gfx_device
// from gfx_app.c and foh_device from foh_dev.c), so a fix in one app is a
// fix for half the class.
//
// MEASURED domain (sprite windows are MILLISECONDS — snd_music_cfg above):
// every track's loop window is 173-310 s and the PCM files are 15.6-36.3 MB,
// so a ~60 s run NEVER wraps the loop and consumes each byte once. The one
// exception is the start-window -> loop-window jump where loopOff <
// startOff+startDur: fdest re-reads 15 s (1.3 MB), fountain 21.3 s (1.9 MB),
// pstadium 1 ms; the other five tracks' windows are disjoint and re-read
// nothing. Net per run: ~5.3 MB of write-once cache dropped against ≤1.9 MB
// re-read on the two stages that overlap. That trade is deliberately NOT
// guarded with loop-aware branch logic in the audio reader.
//
// ADVISORY BY DEFINITION: a kernel that ignores these calls behaves exactly
// as before, which is why no audio path may depend on them. macOS defines no
// POSIX_FADV_*, so both compile to nothing on the host builds.

// Drop the cache for a music range the reader has just consumed. Never
// touches the readahead AHEAD of the range, never another file.
static inline void snd_music_drop_cache(FILE *f, uint64_t fileFrame,
                                        uint32_t frames) {
#ifdef POSIX_FADV_DONTNEED
  (void)posix_fadvise(fileno(f), (off_t)(fileFrame * 4ull),
                      (off_t)frames * 4, POSIX_FADV_DONTNEED);
#else
  (void)f;
  (void)fileFrame;
  (void)frames;
#endif
}

// Read it as what it is: one strictly sequential pass. Fewer, larger SD
// reads means fewer IRQ events inside the paced window.
static inline void snd_music_seq_hint(FILE *f) {
#ifdef POSIX_FADV_SEQUENTIAL
  (void)posix_fadvise(fileno(f), 0, 0, POSIX_FADV_SEQUENTIAL);
#else
  (void)f;
#endif
}

// Fill `budget` source frames [from, from+budget) into the ring via the
// sprite program + reader. Ring slots >= the consumer position are
// producer-owned, so this runs UNLOCKED; the caller publishes wr AFTER
// it returns (lock discipline — header note).
static void snd_music_fill(SndMusic *mu, uint64_t from, uint32_t budget,
                           SndMusicRead rd, void *ud) {
  uint32_t done = 0;
  while (done < budget) {
    uint64_t idx = 0;
    int sil = 0;
    uint32_t run = snd_music_run(mu, from + done, budget - done, &idx, &sil);
    const uint32_t slot = (uint32_t)((from + done) % SND_MUSIC_RING_FRAMES);
    if (slot + run > SND_MUSIC_RING_FRAMES) {
      run = SND_MUSIC_RING_FRAMES - slot; // never wrap within one read
    }
    if (sil) {
      memset(mu->ring + (size_t)slot * 2, 0, (size_t)run * 4);
    } else {
      rd(ud, idx, mu->ring + (size_t)slot * 2, run);
    }
    done += run;
  }
}

typedef struct {
  char name[SND_NAME_LEN];
  const int16_t *pcm;
  uint32_t samples;
  uint16_t gainQ8;
  uint8_t loop;
} SndEntry;

typedef struct {
  const SndEntry *e; // NULL = inactive
  uint64_t phase;    // 16.16 into e->samples
  uint64_t seq;      // start sequence (steal-oldest key)
  uint64_t id;       // howler play id (1000 + play-event count)
} SndVoice;

typedef struct {
  uint8_t *blob; // whole pack file (entries point into its PCM region)
  SndEntry *entries;
  uint32_t count;
  uint64_t step; // 16.16 resample step (SRC_RATE<<16)/OUT_RATE
  SndVoice voice[SND_VOICES];
  SndMusic music; // M4 task 7: zero-init = disabled (fill byte-identical)
  uint64_t seqCounter;
  uint64_t playCount; // play events consumed (id = 1000 + playCount)
  // event counters (deterministic given the sim's event stream; the
  // check cross-asserts device values against host truth)
  uint64_t starts, stops, steals;
  // M4 iter 84 (review-82 H): `stops` counts stop EVENTS (unchanged M3
  // semantics — the gfx_app summary grammar stays intact); the SPLIT
  // records whether the event deactivated at least one voice (matched)
  // or found none (unmatched — howler stale-id/ended-voice no-op).
  // stopsMatched + stopsUnmatched == stops always.
  uint64_t stopsMatched, stopsUnmatched;
  uint64_t maxVoices; // concurrency high-water (M4 task 6 measurement)
} SndMixer;

static uint32_t snd_rd_u32(const uint8_t *p) {
  return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) |
         ((uint32_t)p[3] << 24);
}
static uint16_t snd_rd_u16(const uint8_t *p) {
  return (uint16_t)((uint32_t)p[0] | ((uint32_t)p[1] << 8));
}

static bool snd_name_ok(const char *n) {
  if (n[0] == 0) return false;
  for (const char *c = n; *c; c++) {
    if (!((*c >= '0' && *c <= '9') || (*c >= 'a' && *c <= 'z') ||
          (*c >= 'A' && *c <= 'Z'))) {
      return false;
    }
  }
  return true;
}

// Load + fully validate a SNDPACK1 file. Any inconsistency dies loud.
static void snd_pack_load(SndMixer *m, const char *path) {
  memset(m, 0, sizeof *m);
  m->step = ((uint64_t)SND_SRC_RATE << 16) / SND_OUT_RATE;
  FILE *f = fopen(path, "rb");
  if (!f) sim_fatal("sndpack: cannot open pack file");
  if (fseek(f, 0, SEEK_END) != 0) sim_fatal("sndpack: seek failed");
  const long fsz = ftell(f);
  if (fsz < 16) sim_fatal("sndpack: file shorter than the header");
  if (fseek(f, 0, SEEK_SET) != 0) sim_fatal("sndpack: seek failed");
  m->blob = malloc((size_t)fsz);
  if (!m->blob) sim_fatal("sndpack: oom");
  if (fread(m->blob, 1, (size_t)fsz, f) != (size_t)fsz) {
    sim_fatal("sndpack: short read");
  }
  fclose(f);
  if (memcmp(m->blob, "SNDPACK1", 8) != 0) sim_fatal("sndpack: bad magic");
  m->count = snd_rd_u32(m->blob + 8);
  const uint32_t dataBytes = snd_rd_u32(m->blob + 12);
  if (m->count == 0 || m->count > 100000) sim_fatal("sndpack: bad count");
  const uint64_t want =
      16ull + (uint64_t)m->count * 36ull + (uint64_t)dataBytes;
  if ((uint64_t)fsz != want) {
    sim_fatal("sndpack: file size != header-declared size (truncated or "
              "corrupt pack)");
  }
  if (dataBytes % 2 != 0) sim_fatal("sndpack: odd PCM region size");
  m->entries = malloc((size_t)m->count * sizeof *m->entries);
  if (!m->entries) sim_fatal("sndpack: oom (entries)");
  const uint8_t *dataBase = m->blob + 16 + (size_t)m->count * 36;
  for (uint32_t i = 0; i < m->count; i++) {
    const uint8_t *r = m->blob + 16 + (size_t)i * 36;
    SndEntry *e = &m->entries[i];
    if (r[SND_NAME_LEN - 1] != 0) sim_fatal("sndpack: name not terminated");
    memcpy(e->name, r, SND_NAME_LEN);
    if (!snd_name_ok(e->name)) sim_fatal("sndpack: bad name bytes");
    if (i > 0 && strcmp(m->entries[i - 1].name, e->name) >= 0) {
      sim_fatal("sndpack: names not strictly ascending");
    }
    const uint32_t off = snd_rd_u32(r + 24);
    e->samples = snd_rd_u32(r + 28);
    e->gainQ8 = snd_rd_u16(r + 32);
    e->loop = r[34];
    if (r[35] != 0) sim_fatal("sndpack: nonzero record pad");
    if (off % 2 != 0) sim_fatal("sndpack: odd PCM offset");
    if (e->samples == 0) sim_fatal("sndpack: empty blob");
    if ((uint64_t)off + (uint64_t)e->samples * 2ull > (uint64_t)dataBytes) {
      sim_fatal("sndpack: blob range outside the PCM region");
    }
    if (e->gainQ8 > 256) sim_fatal("sndpack: gainQ8 > 256 (volume > 1)");
    if (e->loop > 1) sim_fatal("sndpack: bad loop flag");
    e->pcm = (const int16_t *)(const void *)(dataBase + off);
  }
}

static const SndEntry *snd_find(const SndMixer *m, const char *name) {
  uint32_t lo = 0, hi = m->count;
  while (lo < hi) {
    const uint32_t mid = lo + (hi - lo) / 2;
    const int c = strcmp(m->entries[mid].name, name);
    if (c == 0) return &m->entries[mid];
    if (c < 0) lo = mid + 1;
    else hi = mid;
  }
  return 0;
}

// Resolve a "<name>.stop" token's base entry (loud death on a name the
// pack does not carry — the SND1-map/sim-plane agreement guard).
static const SndEntry *snd_stop_base(SndMixer *m, const char *token) {
  const size_t n = strlen(token);
  if (!(n > 5 && strcmp(token + n - 5, ".stop") == 0)) {
    sim_fatal("snd: snd_stop_base on a non-stop token");
  }
  char base[SND_NAME_LEN];
  if (n - 5 >= sizeof base) sim_fatal("snd: stop token name too long");
  memcpy(base, token, n - 5);
  base[n - 5] = 0;
  const SndEntry *e = snd_find(m, base);
  if (!e) sim_fatal("snd: stop event for a name not in the pack");
  return e;
}

// ID-ROUTED stop (M4 task 6; howler 2.0.12 stop(id) semantics — header
// note): hasId -> stop the one active voice of this base name whose
// play id matches (stale/unknown id = no-op); !hasId (upstream passed
// undefined) -> stop ALL voices of the base name. Caller holds
// platform_audio_lock(). Counted once per stop EVENT (the M3 counter
// semantics are unchanged).
static void snd_event_stop_id(SndMixer *m, const char *token, int hasId,
                              double id) {
  const SndEntry *e = snd_stop_base(m, token);
  bool matched = false;
  for (int v = 0; v < SND_VOICES; v++) {
    if (m->voice[v].e != e) continue;
    if (hasId && (double)m->voice[v].id != id) continue;
    m->voice[v].e = 0;
    matched = true;
  }
  m->stops++;
  if (matched) m->stopsMatched++;
  else m->stopsUnmatched++;
}

// One sound event from the sim's queue (ml_snd_sink contract: play
// names verbatim; stop sites arrive as "<name>.stop"). Caller holds
// platform_audio_lock(). A bare stop token (no id available — the
// legacy/undefined arm) stops all voices of the base name.
static void snd_event(SndMixer *m, const char *name) {
  const size_t n = strlen(name);
  if (n > 5 && strcmp(name + n - 5, ".stop") == 0) {
    snd_event_stop_id(m, name, 0, 0);
    return;
  }
  const SndEntry *e = snd_find(m, name);
  if (!e) {
    fprintf(stderr, "snd: unknown sound name '%s'\n", name);
    sim_fatal("snd: play event for a name not in the pack (SND1 map and "
              "sim sound plane disagree)");
  }
  m->playCount++; // howler-parallel global id counter (header note)
  // ID-UNIQUENESS BOUND (iter 86, review-84 L, registered): play ids
  // travel through the sim's JS number plane as doubles, and
  // snd_event_stop_id compares via (double)id — beyond 2^53 that
  // conversion stops being injective and id-routing could silently
  // mis-match. Unreachable by construction (2^53 play events at 60 fps
  // is ~4.8 million years of nonstop play), so this is a cheap
  // fail-loud assert, never a live arm.
  if (m->playCount + 1000ull > (1ull << 53)) {
    sim_fatal("snd: play-id counter reached the 2^53 double-uniqueness "
              "bound (id routing would stop being exact)");
  }
  int slot = -1;
  for (int v = 0; v < SND_VOICES; v++) {
    if (m->voice[v].e == 0) { slot = v; break; }
  }
  if (slot < 0) {
    // steal-oldest-by-start-sequence (policy note in the header)
    uint64_t best = UINT64_MAX;
    for (int v = 0; v < SND_VOICES; v++) {
      if (m->voice[v].seq < best) { best = m->voice[v].seq; slot = v; }
    }
    m->steals++;
  }
  m->voice[slot].e = e;
  m->voice[slot].phase = 0;
  m->voice[slot].seq = ++m->seqCounter;
  m->voice[slot].id = 1000ull + m->playCount;
  m->starts++;
  uint64_t live = 0;
  for (int v = 0; v < SND_VOICES; v++) {
    if (m->voice[v].e) live++;
  }
  if (live > m->maxVoices) m->maxVoices = live;
}

// The audio-callback fill (PlatformAudioFill shape): `frames` stereo
// sample frames. Runs under SDL's callback lock — snd_event cannot
// interleave. Spike math verbatim (see header).
static void snd_mix_fill(void *ud, int16_t *out, int frames) {
  SndMixer *m = (SndMixer *)ud;
  for (int i = 0; i < frames; i++) {
    int32_t acc = 0;
    for (int v = 0; v < SND_VOICES; v++) {
      SndVoice *vc = &m->voice[v];
      if (!vc->e) continue;
      const int16_t s = vc->e->pcm[vc->phase >> 16];
      acc += ((int32_t)s * vc->e->gainQ8) >> 8;
      vc->phase += m->step;
      if ((vc->phase >> 16) >= vc->e->samples) {
        if (vc->e->loop) {
          vc->phase -= ((uint64_t)vc->e->samples << 16);
        } else {
          vc->e = 0;
        }
      }
    }
    // M4 task 7: the music channel joins per-channel BEFORE the single
    // clamp (header note). Disabled: accL == accR == the old mono sum —
    // byte-identical to the pre-music mixer.
    int32_t accL = acc, accR = acc;
    if (m->music.on) {
      SndMusic *mu = &m->music;
      const uint64_t t = mu->outPos >> 1; // zero-order hold 2x upsample
      if (t < mu->wr) {
        const size_t slot = (size_t)(t % SND_MUSIC_RING_FRAMES) * 2;
        accL += ((int32_t)mu->ring[slot] * mu->gainQ8) >> 8;
        accR += ((int32_t)mu->ring[slot + 1] * mu->gainQ8) >> 8;
      } else {
        mu->starves++; // silence for this output frame; time advances
      }
      mu->outPos++;
    }
    if (accL > 32767) accL = 32767;
    if (accL < -32768) accL = -32768;
    if (accR > 32767) accR = 32767;
    if (accR < -32768) accR = -32768;
    out[i * 2] = (int16_t)accL;    // SFX mono on both channels;
    out[i * 2 + 1] = (int16_t)accR; // music stereo (spike shape + music)
  }
}

#endif // GFX_SND_MIXER_H
