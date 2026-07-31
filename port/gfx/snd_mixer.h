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

// AUDIO BUS constants (full rationale in the "AUDIO BUS" note past SndMixer).
#define SND_SFX_MASTER_DEFAULT 0.5   // sfx.js:623 changeVolume(sounds, 0.5)
#define SND_MUSIC_MASTER_DEFAULT 0.3 // sfx.js:624 (MusicManager, 0.3)
// Q12, not Q8 (review-r10 MAJOR): the bus multiplies an ALREADY-Q8 packed
// gain, so an 8-bit bus compounds two quantizations and measurably loses a
// whole output LSB at ordinary levels (a no-overwrite 1000-sample at level 0.1
// emitted 99 where exact linear is 100). 12 bits of bus + round-half-up makes
// that example exact and keeps unity byte-identical. Max value is the music
// rail, round(1.0/0.3 * 4096) = 13653, which fits uint16_t with room.
#define SND_BUS_UNITY 4096u
#define SND_BUS_SHIFT 12
#define SND_BUS_HALF (1 << (SND_BUS_SHIFT - 1))

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
  // NOTE (review-r10 BLOCKER): this memset runs on EVERY TRACK SWITCH, not
  // just at boot (mus_track_program -> here, at the menu join and at every
  // VS/target launch). The master music bus therefore must NOT live in this
  // struct — it is a user preference that outlives any track. It lives in
  // SndMixer; see the AUDIO BUS note. An earlier version had it here and
  // silently reverted the user's music level to 0.3 the moment a match started.
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
  // MASTER SFX BUS SNAPSHOT, taken at snd_event (review-r11). This is
  // howler's own semantics, not an optimisation: `Sound.prototype.init`
  // does `self._volume = parent._volume` (howler 2.0.12 dist:2005), so a
  // Sound captures the group volume when it STARTS. audiomenu's
  // `changeVolume(sounds, level, 0)` writes only `audioGroup[key]._volume`
  // (sfx.js:613/615) and never calls the public `.volume()` API, so moving
  // the Sounds slider affects FUTURE plays only — a click already sounding
  // keeps the gain it began with. (Music is the opposite and stays
  // mixer-wide: groupType 1 also calls `.volume(newVolume)` at sfx.js:618,
  // and howler's volume() setter writes every playing sound's gain node
  // live, dist:1088-1100.)
  uint16_t busQ12;
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
  // MASTER buses (see the AUDIO BUS note below). SND_BUS_UNITY == the
  // upstream default already baked into the packed gains. BOTH live here, at
  // MIXER scope, because they are user preferences that must survive every
  // track switch and every voice start; both are set by snd_pack_load, which
  // is a hard prerequisite for any output at all, so a zero-init SndMixer can
  // never mix silently.
  uint16_t sfxBusQ12;
  uint16_t musicBusQ12;
} SndMixer;

// --- AUDIO BUS: the options-audio master levels (menu-fidelity arc) --------
// Upstream's audio screen pushes its two levels straight into the engine:
// `changeVolume(sounds, masterVolume[0], 0)` / `changeVolume(MusicManager,
// masterVolume[1], 1)` (audiomenu.js:114-120), and changeVolume (sfx.js:
// 609-621) overwrites every instance's `_volume` with
// `newVolume * (volumeOverwrites[name] || 1)`.
//
// THE ONE THING THAT MAKES THIS SUBTLE — DO NOT SKIP: the SND1 pipeline
// already records the POST-changeVolume value. sfx.js:623-624 runs
// `changeVolume(sounds, 0.5, 0)` / `changeVolume(MusicManager, 0.3, 1)` at
// load, so every packed `gainQ8` is round(DEFAULT * overwrite * 256), NOT
// round(overwrite * 256) (pipeline/lib/sounds-schema.js:12-15 records both:
// `volume` = the effective post-changeVolume value = the mixer's gain
// source, `cfgVolume` = the authored one). Multiplying the packed gain by
// masterVolume would therefore apply the default TWICE (0.5 * 0.5 at rest).
//
// WHERE EACH BUS APPLIES (measured from howler 2.0.12's own source, not
// assumed — the two channels genuinely differ upstream):
//   * SFX  — `changeVolume(sounds, level, 0)` writes `_volume` on the GROUP
//     only (sfx.js:613/615; groupType 0 skips the `.volume()` call at :618).
//     A howler Sound snapshots `parent._volume` when it starts (dist:2005),
//     so the slider affects FUTURE plays; anything already sounding keeps its
//     gain. Modelled by SndVoice.busQ12, captured in snd_event.
//   * MUSIC — groupType 1 ALSO calls `.volume(newVolume)` (sfx.js:618), and
//     howler's setter updates every playing sound's gain node immediately
//     (dist:1088-1100). So music is LIVE, and its bus stays mixer-wide.
//
// So the faithful live transform is a RATIO against the baked default:
//     effective_new = newVolume * overwrite
//                   = (default * overwrite) * (newVolume / default)
//                   = packed_gain * (newVolume / default)
// i.e. bus = newVolume / default, which is EXACTLY 1.0 at the defaults and
// ranges to 2.0 (sfx: 1.0/0.5) and 3.3333 (music: 1.0/0.3). Above-unity is
// faithful, not a bug: upstream at masterVolume 1.0 really is twice the
// default loudness, and the int32->S16 clamp in snd_mix_fill handles the
// headroom exactly as the browser's own output stage does.
//
// BYTE-IDENTITY AT REST (why the frozen audio gates stay green): at bus ==
// SND_BUS_UNITY the applied math is `(x * 4096 + 2048) >> 12`, which is x for
// every int32 x in range, BOTH SIGNS — so a default-level mixer produces the
// pre-wire bytes EXACTLY. VERIFIED, not asserted: check-mixer-fidelity.sh
// (12 goldens, diff=bit-identical) and check-music-fidelity.sh (12 goldens,
// 8 tracks, diff=bit-identical) both re-run green with this wire in place.
//
// PRECISION — MEASURED, and the residual is REGISTERED, not waved away
// (review-r10 MAJOR corrected an earlier version of this note that claimed
// "<= 1 Q8 step ... below the noise floor"; both halves of that were FALSE):
//   * FIXED here: the bus is Q12, applied PER VOICE, and off the unity path
//     gain+bus are ONE combined 64-bit multiply rounded once (snd_gain_apply).
//     An 8-bit bus compounded two truncations and lost a whole output LSB at
//     ordinary levels (no-overwrite sample 1000 at level 0.1 emitted 99 where
//     exact linear is 100; now 100). Scaling the summed accumulator instead of
//     each voice diverged from per-voice by up to SEVEN output LSBs on
//     polyphonic frames; per-voice removes that entirely. And chaining a Q8
//     truncation into a Q12 one lost LSBs on ODD samples specifically
//     (s=1 -> 0, s=-1 -> -2, s=32767 -> 32766 at master 1.0) — the combined
//     multiply removes that class too (review-r12).
//   * REMAINING, and it is PRE-EXISTING: SNDPACK1 stores each gain as Q8
//     (`u16 gainQ8`), so the AUTHORED product `default * overwrite` is already
//     quantized in the pack before any bus touches it, and a bus can scale
//     that error but never undo it. Worst case is the smallest overwrite
//     (dash, 0.3): packed gain round(0.5*0.3*256) = 38 encodes 0.1484 rather
//     than 0.15. RE-MEASURED after snd_gain_apply landed (the old note said
//     9726, which was the pre-combined-multiply value — review-r13): a
//     full-scale 32767 sample emits 9728 at level 1.0 where exact linear is
//     9830.1, i.e. 1.04% of amplitude (~0.09 dB). The SAME error is already
//     present at the DEFAULT level (4863 vs 4915.1, 1.06%) — the bus neither
//     introduces nor worsens it, which is why the default-level frozen gates
//     cannot see it.
//     This is a property of the FROZEN pack format, not of this wire. It is
//     PRE-EXISTING at the default level (measured just above: 1.06% there
//     vs 1.04% at 1.0), so wiring the bus produces no before/after
//     difference for the frozen gates to see — the error is not new, and
//     not hidden. Removing it means widening SNDPACK1's gain field, which
//     re-freezes a PINNED producer (SNDPACK1 sha256 + both fidelity gates +
//     the device pin) — out of this lane's scope by construction, reported to
//     the driver rather than silently accepted. MENU-SPEC §4 carries the same
//     numbers.
// level (the options-audio [0,1] double) -> Q12 bus gain against `dflt`, the
// master default already baked into the packed gains. Clamps the level to
// [0,1] — the FOH clamps too (foh.c step_opt_audio), this is the trust
// boundary for any other caller — and rounds half-up like pack-snd.js.
static inline uint16_t snd_bus_q12(double level, double dflt) {
  if (!(level > 0.0)) return 0; // <= 0 or NaN -> silence (fails closed)
  if (level > 1.0) level = 1.0;
  return (uint16_t)(level / dflt * (double)SND_BUS_UNITY + 0.5);
}

// Gain ONE sample by its packed Q8 gain and the Q12 master bus.
//
// TWO PATHS ON PURPOSE (review-r12 MAJOR — the earlier version chained two
// truncations and lost whole LSBs on ODD samples, which the witness missed
// because its synthetic PCM was all even; the real pack has >1e6 odd samples):
//
//   * bus == UNITY -> return EXACTLY the pre-wire value `(s*gainQ8) >> 8`,
//     floor and all. This is what keeps check-mixer-fidelity.sh and
//     check-music-fidelity.sh byte-identical, and it is also the path the
//     SHIPPING DEFAULT takes, so the common case costs nothing.
//   * otherwise -> ONE combined multiply in 64-bit at scale 2^20
//     (Q8 x Q12), rounded half-up once. No intermediate truncation exists to
//     lose, so `s=1, gain=128, bus=8192` gives 1 (was 0), `s=-1` gives -1
//     (was -2) and `s=32767` gives 32767 (was 32766) — i.e. master 1.0 with
//     an overwrite-free sound reproduces the AUTHORED sample exactly.
//
// int64 is used only off the unity path, so the device's hot default path
// keeps 32-bit arithmetic. Bound: |s| <= 32768, gainQ8 <= 256, busQ12 <=
// 13653 -> |n| <= 1.15e11, far inside int64.
static inline int32_t snd_gain_apply(int32_t s, uint16_t gainQ8,
                                     uint16_t busQ12) {
  if (busQ12 == (uint16_t)SND_BUS_UNITY) {
    return (s * (int32_t)gainQ8) >> 8; // the pre-wire expression, verbatim
  }
  const int64_t n = (int64_t)s * (int64_t)gainQ8 * (int64_t)busQ12;
  return (int32_t)((n + (int64_t)(1 << (8 + SND_BUS_SHIFT - 1))) >>
                   (8 + SND_BUS_SHIFT));
}

// Read back the effective buses (accessors, so a consumer never depends on
// which struct owns them — r10: musicBus USED to live in SndMusic, where
// snd_music_cfg's per-track memset silently reset it on every track switch).
static inline uint16_t snd_music_bus(const SndMixer *m) {
  return m->musicBusQ12;
}
static inline uint16_t snd_sfx_bus(const SndMixer *m) { return m->sfxBusQ12; }

// The app-facing wire: push the options-audio master levels onto the live
// buses. Callers on a live audio device MUST hold the audio lock (the
// callback reads these fields) — foh_dev.c brackets it with
// platform_audio_lock/unlock. Safe to call at ANY time, including before or
// after a track switch: nothing else writes these two fields.
static inline void snd_bus_set(SndMixer *m, double sfxLevel,
                               double musicLevel) {
  m->sfxBusQ12 = snd_bus_q12(sfxLevel, SND_SFX_MASTER_DEFAULT);
  m->musicBusQ12 = snd_bus_q12(musicLevel, SND_MUSIC_MASTER_DEFAULT);
}

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
  // SFX bus at UNITY (every packed gainQ8 already carries the 0.5 default).
  // Set HERE for the same reason as the music bus above — snd_pack_load is a
  // hard prerequisite for any SFX output, so zero-init silence is unreachable.
  m->sfxBusQ12 = (uint16_t)SND_BUS_UNITY;
  m->musicBusQ12 = (uint16_t)SND_BUS_UNITY;
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
  m->voice[slot].busQ12 = m->sfxBusQ12; // howler Sound.init snapshot
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
      // PER VOICE, not on the sum (r10 MAJOR): scaling the summed accumulator
      // floors once for the whole mix and diverged from per-voice by up to
      // seven output LSBs on polyphonic frames. And with the VOICE'S OWN
      // snapshot, not the live mixer bus (r11 MAJOR): see SndVoice.busQ12 —
      // howler snapshots the group volume into each Sound at play time.
      acc += snd_gain_apply(s, vc->e->gainQ8, vc->busQ12);
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
    // The sfx bus is already applied per voice above.
    int32_t accL = acc, accR = acc;
    if (m->music.on) {
      SndMusic *mu = &m->music;
      const uint64_t t = mu->outPos >> 1; // zero-order hold 2x upsample
      if (t < mu->wr) {
        const size_t slot = (size_t)(t % SND_MUSIC_RING_FRAMES) * 2;
        accL += snd_gain_apply(mu->ring[slot], mu->gainQ8, m->musicBusQ12);
        accR += snd_gain_apply(mu->ring[slot + 1], mu->gainQ8, m->musicBusQ12);
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
