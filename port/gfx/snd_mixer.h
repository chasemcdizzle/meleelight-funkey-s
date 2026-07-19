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

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SND_NAME_LEN 24
#define SND_VOICES 8
#define SND_SRC_RATE 22050
#define SND_OUT_RATE 44100

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
  uint64_t seqCounter;
  uint64_t playCount; // play events consumed (id = 1000 + playCount)
  // event counters (deterministic given the sim's event stream; the
  // check cross-asserts device values against host truth)
  uint64_t starts, stops, steals;
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
  for (int v = 0; v < SND_VOICES; v++) {
    if (m->voice[v].e != e) continue;
    if (hasId && (double)m->voice[v].id != id) continue;
    m->voice[v].e = 0;
  }
  m->stops++;
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
    if (acc > 32767) acc = 32767;
    if (acc < -32768) acc = -32768;
    out[i * 2] = (int16_t)acc;     // mono voice on both channels
    out[i * 2 + 1] = (int16_t)acc; // (spike fill_mix stereo shape)
  }
}

#endif // GFX_SND_MIXER_H
