// port/gfx/snd_idle_check.c — HOST-SIDE IDLE-SILENCE PROOF for the audio
// fill callback (A28: "constant audio buzz from launch, before any sound
// plays"). Compiles snd_mixer.h VERBATIM — the same header gfx_app.c and
// foh_dev.c compile — and drives snd_mix_fill directly, which is exactly
// what the SDL callback hands the driver buffer to
// (port/gfx/platform_audio_sdl.h:103).
//
// WHY THIS EXISTS AND WHAT IT CAN SEE. The M3/M4 audio bars assert
// `underruns == 0`, which is a CALLBACK-TIMING proxy (the gap rule in
// platform_audio_sdl.h:21-32). A callback that hands the driver a buffer
// full of noise is perfectly timely: a stale-buffer defect underruns
// exactly zero times. So no existing bar can see sample CONTENT at idle.
// This check judges content, and nothing else.
//
// THE DEFECT SHAPE IT RULES OUT (A28 hypothesis 1, fix_plan): a mixer
// that ADDS active voices onto the driver's buffer instead of
// INITIALISING every sample frame. With zero voices such a fill returns
// without writing, SDL hands the same buffer back next period, and the
// driver replays whatever was last in it forever — audible as a constant
// buzz at the buffer rate (44100/512 = 86 Hz), present from the instant
// audio opens and independent of anything playing. Every case below
// POISONS the output buffer with a non-zero pattern first, so "silent"
// here means "snd_mix_fill wrote every sample", never "the buffer
// happened to be zero".
//
// TOOTH (--tooth-stale): skip the fill and run the same assertion over
// the poisoned buffer — i.e. present the checker with the exact defect
// it is meant to catch. The check script requires this to FAIL; a
// judgment that cannot fail proves nothing.
//
//   snd_idle_check [--tooth-stale]
//
// Verdict grammar (load-bearing; the script matches it anchored):
//   SND IDLE SILENT cases=<n> frames=<n>
#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void sim_fatal(const char *what) {
  fprintf(stderr, "FATAL: %s\n", what);
  exit(2);
}

#include "snd_mixer.h"

// One SDL period at the shipped spec (platform_audio_sdl.h wants
// 44100/S16LSB/2ch; foh_dev.c:1555 and gfx_app.c:567 default to 512
// sample frames). Judged at that size and at an odd size, so a
// block-quantised write bug cannot hide.
#define IDLE_FRAMES 512
#define ODD_FRAMES 173
#define MAX_FRAMES IDLE_FRAMES

static int16_t g_out[MAX_FRAMES * 2];
static bool g_tooth;
static int g_cases;
static long g_frames_judged;

// Poison, fill, and require every emitted sample to be bit-zero.
static void expect_silence(const char *what, SndMixer *m, int frames,
                           int poison) {
  memset(g_out, poison, sizeof g_out);
  if (!g_tooth) snd_mix_fill(m, g_out, frames);
  for (int i = 0; i < frames * 2; i++) {
    if (g_out[i] != 0) {
      fprintf(stderr,
              "FAIL: %s — sample %d of %d is %d, expected 0 (the idle fill "
              "did not initialise the driver buffer)\n",
              what, i, frames * 2, (int)g_out[i]);
      exit(1);
    }
  }
  g_cases++;
  g_frames_judged += frames;
}

// A mixer in the state snd_pack_load leaves behind (snd_mixer.h:502-508)
// with no voices started — i.e. the state audio OPENS in on both the
// play path (foh_dev.c:1928-1935) and the match path (gfx_app.c:764).
static void mixer_at_rest(SndMixer *m) {
  memset(m, 0, sizeof *m);
  m->step = ((uint64_t)SND_SRC_RATE << 16) / SND_OUT_RATE;
  m->sfxBusQ12 = (uint16_t)SND_BUS_UNITY;
  m->musicBusQ12 = (uint16_t)SND_BUS_UNITY;
}

int main(int argc, char **argv) {
  // snd_mixer.h is a whole-mixer header; this TU consumes the fill path
  // only, and -Werror=unused-function does not know that.
  (void)snd_pack_load;
  (void)snd_event;
  (void)snd_event_stop_id;
  (void)snd_music_cfg;
  (void)snd_music_fill;

  for (int i = 1; i < argc; i++) {
    if (strcmp(argv[i], "--tooth-stale") == 0) {
      g_tooth = true;
    } else {
      fprintf(stderr, "usage: snd_idle_check [--tooth-stale]\n");
      return 2;
    }
  }

  static SndMixer m;

  // [1] The literal boot state: a zero-init mixer (both hosts declare
  // g_mix at file scope — foh_dev.c:824, gfx_app.c:411).
  memset(&m, 0, sizeof m);
  expect_silence("zero-init mixer", &m, IDLE_FRAMES, 0xA5);

  // [2] Post-pack-load, nothing playing — the real idle path.
  mixer_at_rest(&m);
  expect_silence("pack-loaded, zero voices", &m, IDLE_FRAMES, 0x5A);
  mixer_at_rest(&m);
  expect_silence("pack-loaded, zero voices (odd block)", &m, ODD_FRAMES, 0x3C);

  // [3] Idle with both master buses at their maxima (the options-audio
  // rails, snd_mixer.h:100-104): a scaled nothing is still nothing.
  mixer_at_rest(&m);
  m.sfxBusQ12 = (uint16_t)(SND_BUS_UNITY * 2u);
  m.musicBusQ12 = 13653u;
  expect_silence("zero voices at max bus", &m, IDLE_FRAMES, 0x11);

  // [4] Voices that HAVE played and finished: e == NULL but the phase,
  // sequence and bus snapshot left behind are stale non-zero. This is
  // the owner's "and the whole game / time" state — after menu SFX, not
  // just at boot.
  mixer_at_rest(&m);
  for (int v = 0; v < SND_VOICES; v++) {
    m.voice[v].e = 0;
    m.voice[v].phase = 0x1234abcdull * (uint64_t)(v + 1);
    m.voice[v].seq = (uint64_t)v + 7;
    m.voice[v].id = 1000 + (uint64_t)v;
    m.voice[v].busQ12 = (uint16_t)SND_BUS_UNITY;
  }
  expect_silence("retired voices (stale phase/bus)", &m, IDLE_FRAMES, 0x7E);

  // [5] Music channel ON but nothing published yet (wr == 0): the starve
  // arm must emit silence, NOT the uninitialised ring malloc'd by
  // snd_music_cfg (snd_mixer.h:174). The ring is deliberately filled
  // with a loud pattern here — if the starve guard ever inverted, this
  // is precisely the constant buzz A28 describes.
  mixer_at_rest(&m);
  m.music.on = 1;
  m.music.gainQ8 = 77;
  m.music.startDur = 1000;
  m.music.loopDur = 1000;
  m.music.fileFrames = 1000;
  m.music.wr = 0;
  m.music.ring = malloc((size_t)SND_MUSIC_RING_FRAMES * 4);
  if (!m.music.ring) sim_fatal("oom (tooth ring)");
  memset(m.music.ring, 0x7F, (size_t)SND_MUSIC_RING_FRAMES * 4);
  expect_silence("music on, nothing published (starve)", &m, IDLE_FRAMES,
                 0x66);
  if (m.music.starves != (uint64_t)IDLE_FRAMES) {
    fprintf(stderr, "FAIL: starve accounting %" PRIu64 " != %d\n",
            m.music.starves, IDLE_FRAMES);
    return 1;
  }

  // [6] A voice that RUNS OUT mid-block: the tail after it retires must
  // be silent, and the voice must have deactivated itself. This is the
  // transition into the idle path that cases 2-4 assert at rest.
  {
    static const int16_t pcm[4] = {32000, -32000, 32000, -32000};
    static SndEntry e;
    memset(&e, 0, sizeof e);
    memcpy(e.name, "tone", 5);
    e.pcm = pcm;
    e.samples = 4;
    e.gainQ8 = 256;
    e.loop = 0;
    mixer_at_rest(&m);
    m.voice[0].e = &e;
    m.voice[0].phase = 0;
    m.voice[0].busQ12 = (uint16_t)SND_BUS_UNITY;
    memset(g_out, 0x2D, sizeof g_out);
    snd_mix_fill(&m, g_out, IDLE_FRAMES);
    // step is 0x8000 (22050 -> 44100), so 4 source samples span 8
    // output frames; everything past that is the idle path.
    if (m.voice[0].e != 0) {
      fprintf(stderr, "FAIL: non-looping voice did not retire at EOF\n");
      return 1;
    }
    if (g_out[0] == 0) {
      fprintf(stderr, "FAIL: audible voice emitted silence — the case is "
                      "not exercising the mixer\n");
      return 1;
    }
    for (int i = 8 * 2; i < IDLE_FRAMES * 2; i++) {
      if (g_out[i] != 0) {
        fprintf(stderr,
                "FAIL: tail after voice EOF — sample %d is %d, expected 0\n",
                i, (int)g_out[i]);
        return 1;
      }
    }
    g_cases++;
    g_frames_judged += IDLE_FRAMES;
  }

  printf("SND IDLE SILENT cases=%d frames=%ld\n", g_cases, g_frames_judged);
  return 0;
}
