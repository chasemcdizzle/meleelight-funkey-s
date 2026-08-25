// foh_snd_witness.c — the FOH SOUND-PLANE witness (C23; menu-fidelity arc
// round 10). A check-owned host leg for check-foh-flows.sh, modelled on
// foh_banner_witness.c.
//
// WHY (review-r9 Standards BLOCKER, and the writer's own escalation): the FOH
// structural trace carries transitions / selections / launches and NOTHING
// ELSE. `FohState.snd[]` — every menu sound the machine emits — was
// COMPLETELY UNOBSERVED by any check. A screen that played the wrong sound,
// played it twice, or fell silent altogether passed all eight green flow
// runs. That is the same class as the U1 star false-green: the judge could
// not see the plane the change lived in. It needs neither device audio nor a
// trace-format change — foh.h:538-539 exposes foh_init/foh_tick and snd[] is
// populated per tick — so it belongs here, in the host check.
//
// TWO PLANES, ONE BINARY (both are "can the user hear the right thing?"):
//
//  [A] EMISSION — drives REAL foh_tick() over a table of crafted one-tick
//      input edges and asserts the EXACT token sequence, in order, with the
//      exact count. Covers: the five menuMove=true arms that emit a SECOND
//      `menuSelect` in the same tick (menu.js:236); the single-sound
//      changeGamemode leaves that must NOT (the negative side — a witness
//      that only checks the doubles would pass if every arm doubled); the
//      `deny` refusals; A-over-B and up-over-down priority inside upstream's
//      one else-if chain; and the audio screen's own arms, including the two
//      that are easy to get wrong (NO A handler at all, and a clamped
//      rail-end step that still clicks).
//
//  [B] AUDIBILITY — drives the REAL snd_mix_fill() over a synthetic
//      SNDPACK1 and a synthetic music ring and asserts that the
//      options-audio master levels actually reach the output samples. This
//      is the host half of the audio wiring; the DEVICE half (real SDL
//      audio, the callback, a human hearing it) is a device work order,
//      .loop/menus-p2-device-workorder-audio.md — this leg does not claim
//      it. What it DOES prove is everything that is not the speaker:
//        - at the upstream DEFAULT levels the fill is BYTE-IDENTICAL to the
//          pre-wire mixer. Asserted against an INDEPENDENT reference — the
//          old no-bus formula `(s * gainQ8) >> 8` recomputed here — not
//          against a re-derivation of the new code. This is the claim that
//          protects the frozen check-mixer-fidelity.sh / check-music-
//          fidelity.sh streams.
//        - level 0.0 is EXACT silence, level 1.0 is strictly louder than
//          default, and the whole 0.0->1.0 sweep is strictly monotonic. A
//          slider that changed a stored number and nothing else fails all
//          three (that was the r9 Spec BLOCKER, measured).
//        - snd_bus_q12's RATIO semantics against a hand-computed pin table.
//          This is the subtle one: SND1's packed gains ALREADY carry the
//          0.5/0.3 defaults (sfx.js:623-624 runs changeVolume at load), so
//          the bus must be level/default, never the raw level, or the
//          default applies twice. The pins are the arithmetic, by hand.
//
// TOOTH (check-side, on COPIES — the committed sources are never touched):
// removing a second `menuSelect` from foh.c, or turning the `else if (dE)`
// cursor arm back into an independent `if`, or dropping the bus multiply out
// of snd_mix_fill, must each make THIS binary exit non-zero before it prints
// its verdict. check-foh-flows.sh drives those perturbations.
//
// Usage: foh_snd_witness <scratch-pack-path>   (all state is synthetic; the
// path is a worktree-local scratch file the witness writes and removes)
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "foh.h"

// foh.c calls gfx_fatal on an invalid screen/token; host-provided here so a
// domain slip is a loud non-zero exit, never a limp-on.
void gfx_fatal(const char *what) {
  fprintf(stderr, "foh_snd_witness: gfx_fatal: %s\n", what);
  exit(3);
}
// snd_mixer.h requires sim_fatal() declared before inclusion (header note).
void sim_fatal(const char *what);
void sim_fatal(const char *what) {
  fprintf(stderr, "foh_snd_witness: sim_fatal: %s\n", what);
  exit(4);
}

#include "../gfx/snd_mixer.h"

static int g_fails;
static void bad(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
static void bad(const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  fputs("foh_snd_witness: FAIL: ", stderr);
  vfprintf(stderr, fmt, ap);
  fputc('\n', stderr);
  va_end(ap);
  g_fails++;
}

// ---------------------------------------------------------------------------
// [A] EMISSION: exact snd[] token sequences out of the real foh_tick
// ---------------------------------------------------------------------------

// One tick's buttons, as a '+'-joined token string (kept readable in the
// case table; parsed here so the table stays declarative).
static void set_buttons(PlatformInput *in, const char *spec) {
  memset(in, 0, sizeof *in);
  const char *p = spec;
  while (*p) {
    const char *e = strchr(p, '+');
    const size_t n = e ? (size_t)(e - p) : strlen(p);
    if (n == 1 && p[0] == 'a') in->a = true;
    else if (n == 1 && p[0] == 'b') in->b = true;
    else if (n == 2 && strncmp(p, "up", 2) == 0) in->up = true;
    else if (n == 4 && strncmp(p, "down", 4) == 0) in->down = true;
    else if (n == 4 && strncmp(p, "left", 4) == 0) in->left = true;
    else if (n == 5 && strncmp(p, "right", 5) == 0) in->right = true;
    else if (n == 5 && strncmp(p, "start", 5) == 0) in->start = true;
    else {
      fprintf(stderr, "foh_snd_witness: bad button token in '%s'\n", spec);
      exit(5);
    }
    if (!e) break;
    p = e + 1;
  }
}

// Join the tick's emitted tokens the same way the table spells them, so a
// mismatch prints both sides verbatim.
static void join_snd(const FohState *s, char *out, size_t cap) {
  out[0] = 0;
  for (int i = 0; i < s->nsnd; i++) {
    if (i) strncat(out, ",", cap - strlen(out) - 1);
    strncat(out, s->snd[i], cap - strlen(out) - 1);
  }
}

typedef struct {
  const char *id;
  FohScreen screen;
  int sel;             // menuSelected (and audioRow, harmlessly, for audio)
  const char *btn;     // buttons pressed on this tick (prev = all released)
  const char *want;    // EXACT comma-joined token sequence ("" = silence)
  const char *wantScr; // screen token after the tick (NULL = don't care)
  int wantSel;         // menuSelected after the tick (-1 = don't care)
} SndCase;

// EVERY row is a citation, not a guess. The doubles are foh.c's
// `snd_push(s, "menuSelect")` sites inside the A/B arms of step_menu
// (menu.js:67/:97/:141/:169/:174/:179 set the local `menuMove` boolean,
// menu.js:236 plays the second sound); the singles are the changeGamemode
// leaves, which do NOT set it. There are FIVE doubles at FOH_CTL_CHOOSER 1
// and FOUR at 0, because D27's collapse turns the Options `CONTROLS` row
// from a menuMODE change into a changeGamemode leave — the one row below
// that the flag rewrites.
static const SndCase kCases[] = {
    // --- the FIVE menuMove=true arms: menuForward/menuBack THEN menuSelect
    {"top-A-options", FOH_MENU_TOP, 3, "a", "menuForward,menuSelect",
     "menu-options", -1},
#if FOH_CTL_CHOOSER
    {"options-A-controls", FOH_MENU_OPTIONS, 2, "a", "menuForward,menuSelect",
     "menu-controls", -1},
#else
    // DEVIATION D27 (foh.h's FOH_CTL_CHOOSER): with the chooser collapsed
    // this row is a changeGamemode(12) LEAVE, so it MOVES SIDES in this
    // table — from the five menuMove=true arms to the single-sound ones. The
    // cursor claim is the other half of the collapse: menuSelected is left
    // ALONE (the chooser it used to reset the cursor for is gone), which is
    // what puts B back on the CONTROLS row.
    {"options-A-controls", FOH_MENU_OPTIONS, 2, "a", "menuForward",
     "controls-keyboard", 2},
#endif
    {"controls-B", FOH_MENU_CONTROLS, 0, "b", "menuBack,menuSelect",
     "menu-options", 0},
    {"options-B", FOH_MENU_OPTIONS, 0, "b", "menuBack,menuSelect", "menu-top",
     3},
    // menu-battle is UNREACHABLE by navigation at FOH_NETPLAY 0 (the shipped
    // build; foh.h:325 + the C5 ruling) but its B arm is compiled
    // unconditionally, so it is driven DIRECTLY here. Guarding it costs
    // nothing and keeps the arm honest for a future FOH_NETPLAY 1 build.
    {"battle-B", FOH_MENU_BATTLE, 0, "b", "menuBack,menuSelect", "menu-top",
     0},
    // --- the NEGATIVE side: changeGamemode leaves emit ONE sound
    {"top-A-vs", FOH_MENU_TOP, 0, "a", "menuForward", "css", 0},
    {"top-A-targettest", FOH_MENU_TOP, 1, "a", "menuForward", "target-select",
     -1},
    {"options-A-audio", FOH_MENU_OPTIONS, 0, "a", "menuForward",
     "options-audio", -1},
    {"options-A-gameplay", FOH_MENU_OPTIONS, 1, "a", "menuForward",
     "options-gameplay", -1},
    // DEVIATION D25 (owner-requested 2026-08-23): the Controls chooser's rows
    // are HANDHELD first, CONTROLLER second, so the ROW INDEXES below are the
    // swapped ones. The sounds are unchanged (both are changeGamemode leaves,
    // one menuForward each); what moved is which index reaches which screen.
    // D27 makes the chooser UNREACHABLE by navigation at FOH_CTL_CHOOSER 0
    // (foh.h), but its arms are compiled unconditionally, so these two and
    // `controls-B` above are driven DIRECTLY here at either flag value — the
    // `battle-B` treatment, for the same reason: it costs nothing and keeps
    // the restorable path honest.
    {"controls-A-handheld", FOH_MENU_CONTROLS, 0, "a", "menuForward",
     "controls-keyboard", -1},
    {"controls-A-pad", FOH_MENU_CONTROLS, 1, "a", "menuForward",
     "controls-controller", -1},
    // A7 (MENU-SPEC §8): the CREDITS row used to REFUSE and now opens a real
    // screen, so it MOVES SIDES in this table exactly as D27's Controls row
    // did — from the `deny` block below to the single-sound changeGamemode
    // leaves. Upstream is menu.js:145-149, inside the one menuForward at :70
    // and with no `menuMove = true`, so it is ONE sound; menuSelected is left
    // alone (credits.js:226-246 changes only the gameMode), which is what
    // puts the cursor back on CREDITS when the screen exits.
    {"options-A-credits", FOH_MENU_OPTIONS, 3, "a", "menuForward", "credits",
     3},
    // The credits screen's OWN sound arms (B -> menuBack, A -> foxlaserfire,
    // the hit -> targetBreak, the timer -> complete/failure) are asserted by
    // port/foh/check-credits.sh's witness rather than added here: this
    // table's length is pinned by check-foh-flows.sh's exact verdict line
    // (`cases=24`), which A7 does not get to move.
    // --- refusals: `deny`, exactly once, and the screen does NOT move
    {"top-A-targetbuilder", FOH_MENU_TOP, 2, "a", "deny", "menu-top", 2},
    // --- cursor arms: ONE menuSelect (menu.js:236 via the up/down arms)
    {"top-up", FOH_MENU_TOP, 0, "up", "menuSelect", "menu-top", 3},
    {"top-down", FOH_MENU_TOP, 0, "down", "menuSelect", "menu-top", 1},
    // --- PRIORITY, all four inside upstream's ONE else-if chain
    //     (menu.js:69,164,192,205 -> A, B, up, down in that order).
    // up beats down: ONE menuSelect and the cursor lands where UP put it.
    // Independent ifs cancelled the cursor AND emitted two menuSelects.
    {"top-up+down", FOH_MENU_TOP, 0, "up+down", "menuSelect", "menu-top", 3},
    // A beats B: B on menu-options would emit "menuBack,menuSelect" and go
    // to menu-top, so the token sequence AND the destination both discriminate.
    {"options-A+B", FOH_MENU_OPTIONS, 0, "a+b", "menuForward", "options-audio",
     -1},
    // A beats up; B beats up.
    {"options-A+up", FOH_MENU_OPTIONS, 0, "a+up", "menuForward",
     "options-audio", -1},
    {"options-B+up", FOH_MENU_OPTIONS, 0, "b+up", "menuBack,menuSelect",
     "menu-top", 3},
    // --- the AUDIO screen (audiomenu.js). Its chain is B -> up -> down ->
    //     right -> left, and it has NO A HANDLER AT ALL (`.a` does not appear
    //     in the file) — so A is SILENT here, which no other screen is.
    {"audio-A-silent", FOH_OPT_AUDIO, 0, "a", "", "options-audio", -1},
    {"audio-B-single", FOH_OPT_AUDIO, 0, "b", "menuBack", "menu-options", -1},
    {"audio-up", FOH_OPT_AUDIO, 0, "up", "menuSelect", "options-audio", -1},
    {"audio-right", FOH_OPT_AUDIO, 0, "right", "menuSelect", "options-audio",
     -1},
    {"audio-left", FOH_OPT_AUDIO, 0, "left", "menuSelect", "options-audio",
     -1},
};

static void run_emission(void) {
  char got[256];
  for (size_t i = 0; i < sizeof kCases / sizeof kCases[0]; i++) {
    const SndCase *c = &kCases[i];
    FohState s;
    foh_init(&s);
    s.screen = c->screen;
    s.menuSelected = c->sel;
    s.audioRow = c->sel;
    memset(&s.prev, 0, sizeof s.prev); // every button released -> rising edge
    PlatformInput in;
    set_buttons(&in, c->btn);
    foh_tick(&s, &in);
    join_snd(&s, got, sizeof got);
    if (strcmp(got, c->want) != 0) {
      bad("[%s] snd sequence is '%s', want '%s' (nsnd=%d)", c->id, got,
          c->want, s.nsnd);
    }
    if (c->wantScr && strcmp(foh_screen_token(s.screen), c->wantScr) != 0) {
      bad("[%s] screen is '%s', want '%s'", c->id,
          foh_screen_token(s.screen), c->wantScr);
    }
    if (c->wantSel >= 0 && s.menuSelected != c->wantSel) {
      bad("[%s] menuSelected is %d, want %d", c->id, s.menuSelected,
          c->wantSel);
    }
  }
}

// The audio screen's VALUE plane, which the token table cannot see: the
// clamped rail-end step must STILL click (audiomenu.js:102/:108 play the
// sound BEFORE the clamps at :104-106/:110-112), and a diagonal must move
// the cursor and leave both volumes alone (lsY is tested before lsX).
static void run_audio_values(void) {
  // rail-end no-op still clicks
  {
    FohState s;
    foh_init(&s);
    s.screen = FOH_OPT_AUDIO;
    s.audioRow = 0;
    s.masterVolume[0] = 1.0;
    memset(&s.prev, 0, sizeof s.prev);
    PlatformInput in;
    set_buttons(&in, "right");
    foh_tick(&s, &in);
    char got[64];
    join_snd(&s, got, sizeof got);
    if (strcmp(got, "menuSelect") != 0) {
      bad("[audio-clamped-click] a clamped rail-end step emitted '%s', want "
          "'menuSelect' (upstream plays before it clamps)", got);
    }
    if (s.masterVolume[0] != 1.0) {
      bad("[audio-clamped-click] the clamp let masterVolume[0] leave 1.0");
    }
  }
  // a step actually moves the level
  {
    FohState s;
    foh_init(&s);
    s.screen = FOH_OPT_AUDIO;
    s.audioRow = 0;
    const double before = s.masterVolume[0];
    memset(&s.prev, 0, sizeof s.prev);
    PlatformInput in;
    set_buttons(&in, "right");
    foh_tick(&s, &in);
    if (!(s.masterVolume[0] > before)) {
      bad("[audio-step] RIGHT did not raise masterVolume[0] (%g -> %g)",
          before, s.masterVolume[0]);
    }
  }
  // diagonal: cursor moves, volumes untouched (lsY before lsX)
  {
    FohState s;
    foh_init(&s);
    s.screen = FOH_OPT_AUDIO;
    s.audioRow = 0;
    const double v0 = s.masterVolume[0], v1 = s.masterVolume[1];
    memset(&s.prev, 0, sizeof s.prev);
    PlatformInput in;
    set_buttons(&in, "up+right");
    foh_tick(&s, &in);
    if (s.audioRow != 1) {
      bad("[audio-diagonal] audioRow is %d, want 1 (up wraps -1 -> 1)",
          s.audioRow);
    }
    if (s.masterVolume[0] != v0 || s.masterVolume[1] != v1) {
      bad("[audio-diagonal] a diagonal moved a volume (%g,%g -> %g,%g); "
          "upstream tests lsY before lsX so it must not",
          v0, v1, s.masterVolume[0], s.masterVolume[1]);
    }
  }
}

// ---------------------------------------------------------------------------
// [B] AUDIBILITY: the master levels reach snd_mix_fill's output samples
// ---------------------------------------------------------------------------

// snd_bus_q12's RATIO semantics, pinned by HAND-COMPUTED arithmetic. These
// are the numbers that make the wire correct rather than merely present: the
// bus is level/default, because the packed gain already carries `default`.
static void run_bus_pins(void) {
  struct {
    double level, dflt;
    unsigned want;
    const char *why;
  } pins[] = {
      // Q12: unity is 4096. sfx (default 0.5): unity at 0.5, 2x at the rail.
      {0.5, SND_SFX_MASTER_DEFAULT, 4096, "sfx default == unity"},
      {1.0, SND_SFX_MASTER_DEFAULT, 8192, "sfx 1.0 == 2x (1.0/0.5)"},
      {0.0, SND_SFX_MASTER_DEFAULT, 0, "sfx 0.0 == silence"},
      {0.25, SND_SFX_MASTER_DEFAULT, 2048, "sfx 0.25 == half unity"},
      // round(0.1/0.5*4096) = round(819.2) = 819
      {0.1, SND_SFX_MASTER_DEFAULT, 819, "sfx 0.1 -> round(819.2)"},
      // round(0.6/0.5*4096) = round(4915.2) = 4915
      {0.6, SND_SFX_MASTER_DEFAULT, 4915, "sfx 0.6 -> round(4915.2)"},
      // music (default 0.3): unity at 0.3, round(1.0/0.3*4096) =
      // round(13653.33) = 13653
      {0.3, SND_MUSIC_MASTER_DEFAULT, 4096, "music default == unity"},
      {1.0, SND_MUSIC_MASTER_DEFAULT, 13653, "music 1.0 -> round(13653.33)"},
      {0.0, SND_MUSIC_MASTER_DEFAULT, 0, "music 0.0 == silence"},
      // round(0.15/0.3*4096) = 2048
      {0.15, SND_MUSIC_MASTER_DEFAULT, 2048, "music 0.15 == half unity"},
  };
  for (size_t i = 0; i < sizeof pins / sizeof pins[0]; i++) {
    const unsigned got = snd_bus_q12(pins[i].level, pins[i].dflt);
    if (got != pins[i].want) {
      bad("[bus-pin] snd_bus_q12(%g, %g) = %u, want %u (%s)", pins[i].level,
          pins[i].dflt, got, pins[i].want, pins[i].why);
    }
  }
  // Trust boundary: an out-of-domain level must not amplify without bound.
  if (snd_bus_q12(9.0, SND_SFX_MASTER_DEFAULT) !=
      snd_bus_q12(1.0, SND_SFX_MASTER_DEFAULT)) {
    bad("[bus-pin] snd_bus_q12 did not clamp a >1 level to the rail");
  }
  if (snd_bus_q12(-3.0, SND_SFX_MASTER_DEFAULT) != 0) {
    bad("[bus-pin] snd_bus_q12 did not floor a negative level to silence");
  }
}

// snd_gain_apply's exact arithmetic, on HAND-COMPUTED vectors including ODD
// and NEGATIVE samples (review-r12 MAJOR). The earlier chained-truncation
// version passed every other case in this witness because the synthetic PCM
// below WAS all EVEN (WIT_PCM_VAL is now odd) — and the real pack has over a
// million odd samples. So the
// vectors are asserted directly, at the one function both channels route
// through, rather than only through the mixer.
static void run_gain_vectors(void) {
  // (1) UNITY IS THE PRE-WIRE EXPRESSION, for every sign and parity. This is
  // the frozen-gate contract, so it is checked exhaustively over a range that
  // includes both S16 rails rather than sampled.
  for (int32_t x = -32768; x <= 32767; x++) {
    for (int g = 0; g < 3; g++) {
      const uint16_t gain[3] = {77, 128, 256};
      const int32_t want = (x * (int32_t)gain[g]) >> 8; // pre-wire, verbatim
      const int32_t got = snd_gain_apply(x, gain[g], (uint16_t)SND_BUS_UNITY);
      if (got != want) {
        bad("[gain-unity] snd_gain_apply(%d, %u, UNITY) = %d, want the "
            "pre-wire %d — the frozen mixer/music streams would move",
            x, gain[g], got, want);
        return; // one report is enough; do not spam 65k lines
      }
    }
  }
  // (2) MASTER 1.0 ON AN OVERWRITE-FREE SOUND REPRODUCES THE AUTHORED SAMPLE.
  // gainQ8 128 encodes the 0.5 default with overwrite 1.0, and bus 8192 is
  // level 1.0 (1.0/0.5 in Q12), so the product is exactly x. The odd and
  // negative rails are the cases chained truncation got wrong: 1 -> 0,
  // -1 -> -2, 32767 -> 32766.
  const struct { int32_t s, want; } kAuthored[] = {
      {1, 1}, {-1, -1}, {3, 3}, {-3, -3}, {32767, 32767}, {-32768, -32768},
      {12345, 12345}, {-12345, -12345},
  };
  for (size_t i = 0; i < sizeof kAuthored / sizeof kAuthored[0]; i++) {
    const int32_t got = snd_gain_apply(kAuthored[i].s, 128, 8192);
    if (got != kAuthored[i].want) {
      bad("[gain-authored] snd_gain_apply(%d, 128, 8192) = %d, want %d "
          "(master 1.0 on an overwrite-free sound must reproduce the authored "
          "sample; chained truncation loses odd samples)",
          kAuthored[i].s, got, kAuthored[i].want);
    }
  }
  // (3) SILENCE is exact at bus 0, for both signs.
  if (snd_gain_apply(32767, 256, 0) != 0 ||
      snd_gain_apply(-32768, 256, 0) != 0) {
    bad("[gain-zero] a zero bus did not produce exact silence");
  }
}

// A synthetic SNDPACK1 with ONE looping sound. gainQ8 = 128 is a REALISTIC
// packed gain: 0.5 (the sfx default) x 1.0 (no volumeOverwrites entry) x 256.
#define WIT_SAMPLES 64
#define WIT_PCM_VAL 1001 // ODD on purpose (review-r12): even-only PCM hid a
                             // chained-truncation bug through the whole mixer path
// The pack path is CALLER-SUPPLIED (review-r10 MINOR): a fixed /tmp name is
// shared across worktrees while the check's run lock is worktree-local, so two
// PROCESS §12 lanes running their checks at once could truncate or unlink each
// other's pack. check-foh-flows.sh passes a path under its own build dir.
static const char *write_pack(const char *path) {
  FILE *f = fopen(path, "wb");
  if (!f) { perror("pack"); exit(6); }
  uint8_t hdr[16] = {'S', 'N', 'D', 'P', 'A', 'C', 'K', '1'};
  const uint32_t count = 1, dataBytes = WIT_SAMPLES * 2;
  memcpy(hdr + 8, &count, 4);      // little-endian host (FORMATS.md §0)
  memcpy(hdr + 12, &dataBytes, 4);
  fwrite(hdr, 1, 16, f);
  uint8_t rec[36];
  memset(rec, 0, sizeof rec);
  memcpy(rec, "tone", 4);
  const uint32_t off = 0, samples = WIT_SAMPLES;
  const uint16_t gainQ8 = 128;
  memcpy(rec + 24, &off, 4);
  memcpy(rec + 28, &samples, 4);
  memcpy(rec + 32, &gainQ8, 2);
  rec[34] = 1; // loop, so the voice never ends mid-window
  rec[35] = 0; // pad
  fwrite(rec, 1, 36, f);
  for (int i = 0; i < WIT_SAMPLES; i++) {
    const int16_t v = WIT_PCM_VAL;
    fwrite(&v, 1, 2, f);
  }
  if (fclose(f) != 0) { perror("pack close"); exit(6); }
  return path;
}

#define WIT_FRAMES 32
// Mix WIT_FRAMES output frames of the looping "tone" at the given sfx level;
// returns out[0] (constant PCM -> every frame is the same) and asserts the
// whole window really is constant (so one sample legitimately stands for it).
static int32_t mix_sfx_at(const char *pack, double level, const char *ctx) {
  SndMixer m;
  snd_pack_load(&m, pack);
  snd_bus_set(&m, level, SND_MUSIC_MASTER_DEFAULT);
  snd_event(&m, "tone");
  int16_t out[WIT_FRAMES * 2];
  snd_mix_fill(&m, out, WIT_FRAMES);
  for (int i = 1; i < WIT_FRAMES; i++) {
    if (out[i * 2] != out[0] || out[i * 2 + 1] != out[0]) {
      bad("[%s] the constant-PCM window is not constant (frame %d: %d/%d vs "
          "%d) — the witness's own premise broke",
          ctx, i, out[i * 2], out[i * 2 + 1], out[0]);
      break;
    }
  }
  free(m.blob);
  free(m.entries);
  return out[0];
}

// SFX SNAPSHOT LIFECYCLE (review-r11 MAJOR): howler's `Sound.init` copies
// `parent._volume` when a sound STARTS (dist:2005), and audiomenu's
// `changeVolume(sounds, level, 0)` writes only the group `_volume` and never
// calls the public `.volume()` API (sfx.js:613/615, groupType 0 skips :618).
// So moving the Sounds slider must affect FUTURE plays ONLY — a click already
// sounding keeps the gain it began with. Mixing every voice against the LIVE
// mixer bus is the plausible-but-wrong implementation, and nothing else in
// this witness can tell the two apart.
static void run_sfx_snapshot(const char *pack) {
  SndMixer m;
  snd_pack_load(&m, pack);
  // start a voice at the DEFAULT level
  snd_bus_set(&m, SND_SFX_MASTER_DEFAULT, SND_MUSIC_MASTER_DEFAULT);
  snd_event(&m, "tone");
  const int32_t refDefault = ((int32_t)WIT_PCM_VAL * 128) >> 8;
  // now move the slider to the top of the rail while it is still sounding
  snd_bus_set(&m, 1.0, SND_MUSIC_MASTER_DEFAULT);
  int16_t out[WIT_FRAMES * 2];
  snd_mix_fill(&m, out, WIT_FRAMES);
  if (out[0] != refDefault) {
    bad("[sfx-snapshot] a voice that STARTED at the default emitted %d after "
        "the slider moved to 1.0, want its start-time gain %d — howler "
        "snapshots parent._volume into each Sound at play time, so the "
        "Sounds slider must not re-gain an already-playing voice",
        out[0], refDefault);
  }
  // a voice started AFTER the move must use the new level
  snd_event(&m, "tone");
  int16_t out2[WIT_FRAMES * 2];
  snd_mix_fill(&m, out2, WIT_FRAMES);
  const int32_t refAuthored = ((int32_t)WIT_PCM_VAL * 256) >> 8;
  // two voices now sound: the old one at default + the new one at full
  if (out2[0] != refDefault + refAuthored) {
    bad("[sfx-snapshot] after starting a second voice at level 1.0 the mix "
        "emitted %d, want %d (old voice %d at its snapshot + new voice %d at "
        "the new level)",
        out2[0], refDefault + refAuthored, refDefault, refAuthored);
  }
  free(m.blob);
  free(m.entries);
}

static void run_sfx_audibility(const char *pack) {
  // (1) BYTE-IDENTITY AT THE DEFAULT, against an INDEPENDENT reference: the
  // pre-wire mixer formula, recomputed here. This is what keeps the frozen
  // audio gates green, so it is asserted exactly, not approximately.
  const int32_t refPreWire = ((int32_t)WIT_PCM_VAL * 128) >> 8;
  const int32_t atDefault = mix_sfx_at(pack, SND_SFX_MASTER_DEFAULT, "sfx-default");
  if (atDefault != refPreWire) {
    bad("[sfx-default] at the upstream default the fill emitted %d but the "
        "PRE-WIRE mixer emits %d — the bus is not transparent at rest, and "
        "the frozen mixer/music fidelity streams would move",
        atDefault, refPreWire);
  }
  // (2) SILENCE at 0.0 — exact, no tolerance.
  const int32_t atZero = mix_sfx_at(pack, 0.0, "sfx-zero");
  if (atZero != 0) {
    bad("[sfx-zero] level 0.0 emitted %d, want exact silence", atZero);
  }
  // (3) FULL SCALE at 1.0 == the AUTHORED amplitude. With no
  // volumeOverwrites entry the authored volume is 1.0, so masterVolume 1.0
  // must play the sample at full scale: (s * 256) >> 8 == s. Independent of
  // how the bus is implemented.
  const int32_t atOne = mix_sfx_at(pack, 1.0, "sfx-one");
  const int32_t refAuthored = ((int32_t)WIT_PCM_VAL * 256) >> 8;
  if (atOne != refAuthored) {
    bad("[sfx-one] level 1.0 emitted %d, want the authored full scale %d",
        atOne, refAuthored);
  }
  // (5) LINEARITY at an ordinary level — review-r10's own counter-example.
  // A no-overwrite sample of 1000 at level 0.1 must emit the exact linear
  // result 100. The Q8-bus/truncating version emitted 99: two chained
  // truncations, one whole output LSB lost at a level a user actually picks.
  // This is asserted against the EXACT ARITHMETIC (1000 * 0.5 * (0.1/0.5) =
  // 100), independent of how the bus is implemented.
  // exact linear result: WIT_PCM_VAL * 0.5 * (0.1/0.5) = WIT_PCM_VAL/10,
  // rounded half-up. Computed from the constant so changing the sample value
  // cannot silently invalidate the case.
  const int32_t wantTenth = (WIT_PCM_VAL + 5) / 10;
  const int32_t atTenth = mix_sfx_at(pack, 0.1, "sfx-tenth");
  if (atTenth != wantTenth) {
    bad("[sfx-tenth] level 0.1 on a no-overwrite %d-sample emitted %d, want "
        "the exact linear %d (chained truncation loses an output LSB)",
        WIT_PCM_VAL, atTenth, wantTenth);
  }
  // (4) STRICT MONOTONICITY across the whole rail. A slider that persists a
  // number and changes nothing audible fails here even if (1)-(3) somehow
  // passed: every step of the ten must raise the output.
  int32_t prev = -1;
  for (int step = 0; step <= 10; step++) {
    const double lvl = step / 10.0;
    const int32_t v = mix_sfx_at(pack, lvl, "sfx-sweep");
    if (v <= prev) {
      bad("[sfx-sweep] level %.1f emitted %d, not strictly above the "
          "previous step's %d — the master level is not reaching the samples",
          lvl, v, prev);
      break;
    }
    prev = v;
  }
}

// Music: same three claims through the REAL sprite/ring path. gainQ8 = 77 is
// a realistic packed music gain (round(0.3 * 256) = 77: the 0.3 default with
// no overwrite).
#define WIT_MUS_GAIN 77
#define WIT_MUS_L 8000
#define WIT_MUS_R (-8000)
static void wit_music_read(void *ud, uint64_t fileFrame, int16_t *dst,
                           uint32_t frames) {
  (void)ud;
  (void)fileFrame;
  for (uint32_t i = 0; i < frames; i++) {
    dst[i * 2] = WIT_MUS_L;
    dst[i * 2 + 1] = WIT_MUS_R;
  }
}

// Mix WIT_FRAMES frames of music-only output at the given music level.
static void mix_music_at(const char *pack, double level, int32_t *l,
                         int32_t *r) {
  SndMixer m;
  snd_pack_load(&m, pack); // no snd_event -> zero active SFX voices
  // A long single-window track: Start 0ms/0ms, Loop 0ms/10000ms, and a file
  // big enough that snd_music_run never hits its EOF-silence boundary.
  // PRODUCT ORDER (review-r10 MAJOR): the bus is set FIRST (boot push) and the
  // track is configured AFTER, at every switch. Using that order here means a
  // bus that any per-track path resets fails EVERY music case, not just the
  // dedicated lifecycle one.
  snd_bus_set(&m, SND_SFX_MASTER_DEFAULT, level);
  // snd_music_cfg ALLOCATES mu->ring (snd_mixer.h) — use that allocation and
  // free it (review-r11 NIT: an earlier version overwrote the pointer with a
  // static buffer and leaked the malloc in every music case).
  snd_music_cfg(&m.music, WIT_MUS_GAIN, 0, 0, 0, 10000, 4ull * 1000000ull);
  snd_music_fill(&m.music, 0, WIT_FRAMES + 4, wit_music_read, 0);
  m.music.wr = WIT_FRAMES + 4;
  m.music.on = 1;
  int16_t out[WIT_FRAMES * 2];
  snd_mix_fill(&m, out, WIT_FRAMES);
  if (m.music.starves != 0) {
    bad("[music] the witness ring starved (%llu) — its own premise broke",
        (unsigned long long)m.music.starves);
  }
  *l = out[0];
  *r = out[1];
  free(m.music.ring);
  free(m.blob);
  free(m.entries);
}

// PRODUCT LIFECYCLE (review-r10 BLOCKER + MAJOR): the device sets the bus at
// BOOT (foh_audio_bus_push, from the persisted plane) and then RECONFIGURES
// the music track later, at every switch (mus_track_program -> snd_music_cfg
// at the menu join, the VS launch and the target launch). So the real order is
//     snd_bus_set  THEN  snd_music_cfg  THEN  mix
// and any per-track re-initialisation that clears the bus silently reverts the
// user's music level to the default the moment a match starts. The earlier
// version of this witness configured the track BEFORE setting the bus, which
// is the opposite order, and it passed while that exact bug was live — so this
// case exists to make the LIFECYCLE, not just the arithmetic, observable.
static void run_music_lifecycle(const char *pack) {
  SndMixer m;
  snd_pack_load(&m, pack);
  // 1. boot push: a NON-default music level the user chose
  snd_bus_set(&m, SND_SFX_MASTER_DEFAULT, 1.0);
  const uint16_t afterPush = snd_music_bus(&m);
  // 2. a track switch, exactly as mus_track_program does it
  snd_music_cfg(&m.music, WIT_MUS_GAIN, 0, 0, 0, 10000, 4ull * 1000000ull);
  const uint16_t afterCfg = snd_music_bus(&m);
  if (afterCfg != afterPush) {
    bad("[music-lifecycle] a track switch reset the music bus %u -> %u; the "
        "user's music level would revert to the default the moment a match "
        "or target run starts (the bus is a MIXER-level preference, not "
        "per-track state)",
        afterPush, afterCfg);
  }
  // 3. and it must still be audible after the switch
  snd_music_fill(&m.music, 0, WIT_FRAMES + 4, wit_music_read, 0);
  m.music.wr = WIT_FRAMES + 4;
  m.music.on = 1;
  int16_t out[WIT_FRAMES * 2];
  snd_mix_fill(&m, out, WIT_FRAMES);
  const int32_t refDefault = ((int32_t)WIT_MUS_L * WIT_MUS_GAIN) >> 8;
  if (!(out[0] > refDefault)) {
    bad("[music-lifecycle] after a track switch, level 1.0 emitted %d which "
        "is not louder than the default's %d — the level did not survive the "
        "switch",
        out[0], refDefault);
  }
  free(m.music.ring);
  free(m.blob);
  free(m.entries);
}

static void run_music_audibility(const char *pack) {
  int32_t l, r;
  // (1) byte-identity at the 0.3 default vs the INDEPENDENT pre-wire formula
  mix_music_at(pack, SND_MUSIC_MASTER_DEFAULT, &l, &r);
  const int32_t refL = ((int32_t)WIT_MUS_L * WIT_MUS_GAIN) >> 8;
  const int32_t refR = ((int32_t)WIT_MUS_R * WIT_MUS_GAIN) >> 8;
  if (l != refL || r != refR) {
    bad("[music-default] at the 0.3 default the fill emitted %d/%d but the "
        "PRE-WIRE mixer emits %d/%d — the music bus is not transparent at "
        "rest",
        l, r, refL, refR);
  }
  // (2) exact silence at 0.0, on BOTH channels
  mix_music_at(pack, 0.0, &l, &r);
  if (l != 0 || r != 0) {
    bad("[music-zero] level 0.0 emitted %d/%d, want exact silence", l, r);
  }
  // (3) strictly louder at 1.0, on both channels, in the right direction
  // (the right channel is negative, so "louder" is more negative).
  mix_music_at(pack, 1.0, &l, &r);
  if (!(l > refL) || !(r < refR)) {
    bad("[music-one] level 1.0 emitted %d/%d, not louder than the default's "
        "%d/%d — the music slider is not reaching the samples",
        l, r, refL, refR);
  }
  // (4) strict monotonicity on the left channel across the rail
  int32_t prev = -1;
  for (int step = 0; step <= 10; step++) {
    int32_t li, ri;
    mix_music_at(pack, step / 10.0, &li, &ri);
    if (li <= prev) {
      bad("[music-sweep] level %.1f emitted %d, not strictly above the "
          "previous step's %d",
          step / 10.0, li, prev);
      break;
    }
    prev = li;
  }
}

int main(int argc, char **argv) {
  if (argc != 2) {
    fprintf(stderr, "usage: foh_snd_witness <scratch-pack-path>\n");
    return 2;
  }
  run_emission();
  run_audio_values();
  run_bus_pins();
  run_gain_vectors();
  const char *pack = write_pack(argv[1]);
  run_sfx_audibility(pack);
  run_sfx_snapshot(pack);
  run_music_audibility(pack);
  run_music_lifecycle(pack);
  remove(pack);
  if (g_fails) {
    fprintf(stderr, "foh_snd_witness: %d failure(s)\n", g_fails);
    return 1;
  }
  // ANCHORED verdict line (PROCESS §3 whitelist rule; the check parses this
  // as a FULL LINE, exactly once). The counts are structural pins: growing a
  // plane without growing the witness moves them and the check dies.
  printf("SND WITNESS OK (cases=%zu buspins=10 gainvec=3 sfx=7 music=6)\n",
         sizeof kCases / sizeof kCases[0]);
  return 0;
}
