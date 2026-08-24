// port/gfx/snd_playid_check.c — A40 PLAY-ID AGREEMENT PROOF: a sound the
// SIM started must still stop after the MENU plane has been playing.
//
// THE OWNER-VISIBLE DEFECT (A40): "when I do shieldbreaker with marth
// even if I do a really quick one it plays the whole prolonged charging
// sound instead of just the charging sound for as long as I'm charging."
// marth's NEUTRALSPECIALGROUND.c:99-101 stores
// `player.shieldBreakerID = ml_howl_play_id("shieldbreakercharge")` and
// releases it at :67 with an id-routed `"shieldbreakercharge.stop"`.
// The sample LOOPS, so an id that matches no voice means the charge
// sound runs to the end of the match. FURAFURA's furaLoopID is the same
// class and is judged here too.
//
// WHAT THIS DRIVES — the real thing, not a model of it:
//   * the REAL port/sim/ml_events.c (linked, not reimplemented): its
//     private play counter is what ml_howl_play_id() mints ids from;
//   * the REAL port/gfx/snd_mixer.h (compiled verbatim, the same header
//     foh_dev.c and gfx_app.c compile): its m->playCount is what voice
//     ids are minted from;
//   * foh_dev.c's EXACT wiring — app_snd_sink / app_snd_stop_sink on the
//     sim plane (foh_dev.c:826-838) and foh_snd on the menu plane
//     (foh_dev.c:840-853). The defect lives in the CROSSING of those two
//     planes, which is why both planes' own checks were green: the
//     mixer-fidelity rigs (snd_render.c, snd_reference.js) drive the
//     mixer from a sim event stream only, so the two counters never
//     share a mixer there and the skew cannot appear.
//
// WHAT IS ASSERTED is the outcome the owner reported, never an internal
// id value: after N menu sounds, a sim-started LOOPING voice that the
// sim then stops by id must (a) leave `stopsUnmatched` at 0 and (b) make
// the mixer emit bit-exact silence. (b) is the audible claim; (a) says
// why. Both are checked, because a stop that silences by accident (e.g.
// a voice steal) would satisfy (b) alone.
//
// TWO TEETH, one per assertion, each required to FAIL (a judgment that
// cannot fail proves nothing):
//   --tooth-legacy  routes the menu plane back through snd_event() — the
//     pre-fix wiring, i.e. THE DEFECT ITSELF. Bites on (a). The first
//     case is menuPlays=0 and passes even with this armed, deliberately:
//     the tooth failing at case 2 proves the MENU PLAY is what breaks
//     routing, not that the harness rejects everything.
//   --tooth-deaf  makes the stop sink bookkeep a perfectly MATCHED stop
//     and touch no voice. (a) is structurally blind to it; only (b), the
//     owner-visible claim, can catch it. Orthogonal on purpose — the two
//     teeth fail on different assertions and the script checks which.
//
//   snd_playid_check <pack-path-to-write> [--tooth-legacy|--tooth-deaf]
//
// Verdict grammar (load-bearing; the script matches it anchored):
//   SND PLAYID OK cases=<n> menuplays=<n> stops=<n>
#include <inttypes.h>
#include <stdarg.h>
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

#include "../sim/ml_events.h"

void ml_events_fail(const char *what) { sim_fatal(what); }

// --- foh_dev.c's audio wiring, VERBATIM in shape (foh_dev.c:824-853) ------

static SndMixer g_mix;
static bool g_have_audio = true;
static bool g_tooth_legacy; // menu plane back on snd_event (the defect)
static bool g_tooth_deaf;   // a stop that BOOKKEEPS perfectly and silences
                            // nothing (see the header note)

static void app_snd_sink(const char *name) {
  const size_t n = strlen(name);
  if (n > 5 && strcmp(name + n - 5, ".stop") == 0) return; // id sink owns it
  snd_event(&g_mix, name);
}

static void app_snd_stop_sink(const char *token, int hasId, double id) {
  if (g_tooth_deaf) {
    // Report a clean, matched stop and leave the voice sounding — the
    // regression that assertion (a) is structurally blind to.
    g_mix.stops++;
    g_mix.stopsMatched++;
    return;
  }
  snd_event_stop_id(&g_mix, token, hasId, id);
}

static void foh_snd(const char *name) {
  if (!g_have_audio) return;
  if (g_tooth_legacy) {
    snd_event(&g_mix, name); // pre-A40: the menu burns a sim play id
  } else {
    snd_event_menu(&g_mix, name);
  }
}

// --- a synthetic SNDPACK1 (foh_snd_witness.c:428 write_pack precedent) ----
//
// Three entries, sorted ascending by name as the format demands. The two
// sim-plane sounds LOOP, which is the property that makes A40 audible for
// the whole match instead of for one sample length; "menuclick" does not,
// so it retires on its own like a real UI blip. PCM is a non-zero
// constant, so "silent" below always means "the voice stopped", never
// "the sample happened to be zero here". Contents are synthetic on
// purpose: the shipped pack is Nintendo-derived and gitignored, and this
// check asserts routing, not sample data.
#define PC_SAMPLES 512
#define PC_CLICK_SAMPLES 8
#define PC_PCM_VAL 1001 // odd (foh_snd_witness.c:422 precedent)

static void write_pack(const char *path) {
  static const struct {
    const char *name;
    uint32_t samples;
    uint8_t loop;
  } ents[3] = {{"furaloop", PC_SAMPLES, 1},
               {"menuclick", PC_CLICK_SAMPLES, 0},
               {"shieldbreakercharge", PC_SAMPLES, 1}};
  uint32_t total = 0;
  for (int i = 0; i < 3; i++) total += ents[i].samples;

  FILE *f = fopen(path, "wb");
  if (!f) { perror("pack"); exit(6); }
  uint8_t hdr[16] = {'S', 'N', 'D', 'P', 'A', 'C', 'K', '1'};
  const uint32_t count = 3, dataBytes = total * 2;
  memcpy(hdr + 8, &count, 4); // little-endian host (FORMATS.md §0)
  memcpy(hdr + 12, &dataBytes, 4);
  fwrite(hdr, 1, 16, f);
  uint32_t off = 0;
  for (int i = 0; i < 3; i++) {
    uint8_t rec[36];
    memset(rec, 0, sizeof rec);
    memcpy(rec, ents[i].name, strlen(ents[i].name));
    const uint32_t samples = ents[i].samples;
    const uint16_t gainQ8 = 128;
    memcpy(rec + 24, &off, 4);
    memcpy(rec + 28, &samples, 4);
    memcpy(rec + 32, &gainQ8, 2);
    rec[34] = ents[i].loop;
    fwrite(rec, 1, 36, f);
    off += samples * 2;
  }
  for (uint32_t i = 0; i < total; i++) {
    const int16_t v = PC_PCM_VAL;
    if (fwrite(&v, 2, 1, f) != 1) exit(6);
  }
  if (fclose(f) != 0) exit(6);
}

// --- judgment --------------------------------------------------------------

#define WIN_FRAMES 64
static int16_t g_out[WIN_FRAMES * 2];
static int g_cases;
static int g_menu_total;

static void fail(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
static void fail(const char *fmt, ...) {
  va_list ap;
  fputs("FAIL: ", stderr);
  va_start(ap, fmt);
  vfprintf(stderr, fmt, ap);
  va_end(ap);
  fputc('\n', stderr);
  exit(1);
}

// Mix one window and report whether ANY emitted sample is non-zero.
static bool mix_window_audible(void) {
  memset(g_out, 0, sizeof g_out);
  snd_mix_fill(&g_mix, g_out, WIN_FRAMES);
  for (int i = 0; i < WIN_FRAMES * 2; i++) {
    if (g_out[i] != 0) return true;
  }
  return false;
}

// The play id the MIXER gave the newest voice of this name — the other
// half of the pair whose agreement is the whole ticket. Reported, never
// asserted directly: the assertions are the owner-visible outcome.
static uint64_t voice_id_of(const char *name) {
  uint64_t best = 0, bestSeq = 0;
  for (int v = 0; v < SND_VOICES; v++) {
    if (!g_mix.voice[v].e) continue;
    if (strcmp(g_mix.voice[v].e->name, name) != 0) continue;
    if (g_mix.voice[v].seq >= bestSeq) {
      bestSeq = g_mix.voice[v].seq;
      best = g_mix.voice[v].id;
    }
  }
  return best;
}

// ONE app session, in order: click through the menus, start the looping
// sim voice, release it. The mixer and ml_events.c's counter both persist
// across cases exactly as they do in the running app (neither is ever
// reset — Howler._counter is page-global).
static void case_charge(const char *sound, int menuPlays) {
  const uint64_t stopsBefore = g_mix.stops;

  for (int i = 0; i < menuPlays; i++) foh_snd("menuclick");
  g_menu_total += menuPlays;
  // Let the (non-looping) menu clicks play out, so the audibility
  // judgment below is about the charge voice and nothing else.
  (void)mix_window_audible();
  (void)mix_window_audible();
  if (mix_window_audible()) {
    fail("[%s menuPlays=%d] the menu clicks did not retire — this case's "
         "own premise broke",
         sound, menuPlays);
  }

  // The sim plane, exactly as marth NEUTRALSPECIALGROUND.c:99-101 does it.
  ml_sound_play(sound);
  const double id = ml_howl_play_id(sound);
  const uint64_t voiceId = voice_id_of(sound);

  if (!mix_window_audible()) {
    fail("[%s menuPlays=%d] the sim-started voice is not sounding — this "
         "case's own premise broke",
         sound, menuPlays);
  }

  // The release arm (NEUTRALSPECIALGROUND.c:67 / FURAFURA's furaLoopID).
  char tok[SND_NAME_LEN + 8];
  snprintf(tok, sizeof tok, "%s.stop", sound);
  ml_sound_stop_id(tok, 1, id);

  if (g_mix.stops != stopsBefore + 1) {
    fail("[%s menuPlays=%d] the stop event did not reach the mixer at all "
         "(stops %" PRIu64 " -> %" PRIu64 ") — link 1, not A40",
         sound, menuPlays, stopsBefore, g_mix.stops);
  }
  if (g_mix.stopsUnmatched != 0) {
    fail("[%s menuPlays=%d] id-routed stop matched NO voice "
         "(stopsUnmatched=%" PRIu64 ", simId=%.0f, voiceId=%" PRIu64 ") — the "
         "sim's play id names a voice that does not exist: the two play-id "
         "counters have drifted by the number of menu sounds played (A40)",
         sound, menuPlays, g_mix.stopsUnmatched, id, voiceId);
  }
  if (mix_window_audible()) {
    fail("[%s menuPlays=%d] THE SOUND IS STILL PLAYING after its own stop — "
         "the owner-visible A40 symptom (a looping charge sample that runs "
         "to the end of the match)",
         sound, menuPlays);
  }
  printf("  case[%-19s] menuPlays=%-2d simId=%.0f voiceId=%" PRIu64
         " stops=%" PRIu64 " matched=%" PRIu64 " unmatched=%" PRIu64 "\n",
         sound, menuPlays, id, voiceId, g_mix.stops, g_mix.stopsMatched,
         g_mix.stopsUnmatched);
  g_cases++;
}

int main(int argc, char **argv) {
  // snd_mixer.h is a whole-mixer header; this TU consumes the SFX event
  // path only, and -Werror=unused-function does not know that.
  (void)snd_bus_set;
  (void)snd_music_cfg;
  (void)snd_music_fill;

  const char *pack = 0;
  for (int i = 1; i < argc; i++) {
    if (strcmp(argv[i], "--tooth-legacy") == 0) {
      g_tooth_legacy = true;
    } else if (strcmp(argv[i], "--tooth-deaf") == 0) {
      g_tooth_deaf = true;
    } else if (!pack) {
      pack = argv[i];
    } else {
      fprintf(stderr, "usage: snd_playid_check <pack-path-to-write> [--tooth-legacy|--tooth-deaf]\n");
      return 2;
    }
  }
  if (!pack) {
    fprintf(stderr, "usage: snd_playid_check <pack-path-to-write> [--tooth-legacy|--tooth-deaf]\n");
    return 2;
  }

  write_pack(pack);
  snd_pack_load(&g_mix, pack);
  ml_snd_sink = app_snd_sink;
  ml_snd_stop_id_sink = app_snd_stop_sink;

  // [1] menuPlays=0 — the two counters cannot have drifted yet. This case
  //     passes WITH the tooth armed, on purpose (see the header note).
  case_charge("shieldbreakercharge", 0);
  // [2] one menu sound: the smallest drift that breaks id routing.
  case_charge("shieldbreakercharge", 1);
  // [3] the reported reproduction (menuPlays=3: simId 1001 vs voiceId 1004).
  case_charge("shieldbreakercharge", 3);
  // [4] a realistic pre-match click count (title + CSS + SSS navigation).
  case_charge("shieldbreakercharge", 11);
  // [5-6] SAME CLASS, other holder: puff's FURAFURA furaLoopID.
  case_charge("furaloop", 0);
  case_charge("furaloop", 5);
  // [7] back to marth after puff, so the ids in play are nowhere near
  //     their starting values and no case can pass by both counters
  //     happening to sit at 0.
  case_charge("shieldbreakercharge", 2);

  printf("SND PLAYID OK cases=%d menuplays=%d stops=%" PRIu64 "\n", g_cases,
         g_menu_total, g_mix.stops);
  return 0;
}
