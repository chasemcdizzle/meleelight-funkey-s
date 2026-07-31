// port/gfx/platform_headless.c — the loop/CI backend (M3 task 4).
//
// No display, no SDL, no input source BY DEFAULT. This is what makes the
// autonomous loop possible (CLAUDE.md "SDL / platform seam"): host
// replays link THIS TU and run the exact same app loop the device runs,
// minus the panel. present() is a no-op that still touches nothing;
// poll() reports an all-false input struct and never requests quit.
//
// Two env-gated negative/instrument seams, both DEFAULT-OFF and both
// byte-identical to the historic behaviour when their variable is absent:
// MLFK_HEADLESS_PRESENT_FAIL (present-failure tooth, iter 52) and
// MLFK_HEADLESS_KEYS (the scripted input source below, review-mexit-r3 H1w).
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "platform.h"
#include "platform_keymap.h" // THE button<->letter SSOT, shared with sdl1

int platform_init(const char *title) {
  (void)title;
  return 0;
}

int platform_present(const uint16_t *fb565) {
  (void)fb565;
  // Negative-testing seam (iter 52, review-50 M2 tooth): report every
  // present as FAILED when MLFK_HEADLESS_PRESENT_FAIL=1, proving the
  // app's failure counter + the check's failed-presents gate end to
  // end. Default (env absent/anything else): success, unchanged.
  const char *t = getenv("MLFK_HEADLESS_PRESENT_FAIL");
  return (t && t[0] == '1' && t[1] == 0) ? 1 : 0;
}

// --- MLFK_HEADLESS_KEYS: the host twin of fk_input (review-mexit-r3 H1w) ----
//
// WHY THIS EXISTS. Every arm punch-list C18/C19/B4/A19 added is reachable only
// under `--bridge live`, which foh_dev.c requires to run with `--input poll`.
// Headless poll reported all-false forever, so the LIVE arms — the target
// START quit, the VS finish banner, the pause overlay, the system menu, and
// the multi-phase `foh_phase:` re-entry those exits drive — had no host
// witness of any kind and could only be exercised on the device. That is what
// made the H1 re-entry defect (a second FOH phase truncating --flow-out)
// reachable in the first place.
//
// So this is the host analogue of port/tools/fk_input.c: the SAME script
// format, deliberately, so a tooth can feed a script produced by the SAME
// frozen generator the device rigs use (port/foh/flow-to-fkscript.js) instead
// of a hand-authored key sequence nobody can audit. It implements the SUBSET of
// fk_input.c's grammar that the generator emits (the deviations are listed
// below), under the same anchored full-line whitelist:
//
//   ""             blank line
//   "#..."         comment ('#' at column 0; NO inline comments)
//   "d <a-z>"      key down (exactly 3 chars)
//   "u <a-z>"      key up
//   "s <digits>"   sleep ms (1-5 digits, 0..60000)
//
// Anything else is corruption and dies LOUDLY before the first poll, exactly
// as fk_input does. A silently-ignored line would make a tooth pass while
// testing nothing, which is the failure this project keeps re-registering.
//
// TWO DELIBERATE DEVIATIONS FROM fk_input, both STRICTER, both stated here
// rather than claimed away (review-mexit-r4). The claim being made is
// "identical grammar for the scripts the frozen generator emits", NOT
// "identical injector":
//
//   (a) LETTER DOMAIN. fk_input.c:143-150 accepts every `d/u <a-z>` and
//       forwards whatever keysym it names. This injector additionally
//       requires the letter to be a kPlatformKeymap row (the lookup +
//       refusal at :160-165) and dies
//       otherwise. Kept stricter ON PURPOSE: on the device an unmapped letter
//       is an inert keypress, but on the host it means the script was written
//       against a keymap this build does not have, and the tooth that fed it
//       would pass while pressing nothing. flow-to-fkscript.js only ever
//       emits mapped letters, so no generated script can tell the two apart.
//
//   (b) EOF HELD STATE. fk_input.c:231-233 releases every key when the script
//       ends; this injector holds the last state forever (see platform_poll).
//       Kept as-is ON PURPOSE: a synthetic release-all at EOF would inject a
//       button EDGE that no line of the script asked for, at a poll index
//       that depends on how long the run continues past the script — exactly
//       the timing-dependence the virtual clock exists to remove. Scripts
//       that need a release write `u <k>`, and every generated one does.
//
// THE CLOCK IS VIRTUAL, AND THAT IS THE POINT. fk_input sleeps against the
// wall clock, so which frame a device press lands on is timing-dependent —
// which is why the device rigs grep their traces instead of cmp'ing them.
// Here the clock advances by exactly one QUANTUM per platform_poll() call.
// Note the unit: a quantum is one poll, NOT one application frame — foh_dev.c
// polls outside its frame loops too (the pre-loop edge prime at :2574, the
// system-menu/pause overlays' own poll+drain, the post-overlay resync), so
// the poll index and the frame index are not the same axis. What the virtual
// clock buys is determinism, not frame-accuracy: the injected input is a pure
// function of the POLL INDEX, the poll sequence is itself deterministic, and
// so a --flow-out trace becomes byte-reproducible run to run. That is what
// lets a tooth cmp two runs' traces instead of pattern-matching them.
//
// Letters resolve through kPlatformKeymap — the same header platform_sdl1.c's
// translation arm and foh_dev's --dump-keymap consume — so this injector can
// never drift into its own private mapping.
// ONE QUANTUM of virtual time, spent per platform_poll() call. Sized at the
// 60 Hz frame period so a script authored in device milliseconds keeps its
// intended shape — NOT because a poll is a frame (it is not; see the unit note
// above).
#define HK_QUANTUM_NS 16666667ull

typedef struct {
  char op;       // 'd' | 'u' | 's'
  int idx;       // keymap row for d/u, else -1
  uint64_t ns;   // sleep duration for 's', else 0
} HkCmd;

static HkCmd *g_hk;
static int g_hkN;
static int g_hkPos;          // next command to apply
static uint64_t g_hkNow;     // virtual clock (ns), advanced one quantum per poll
static uint64_t g_hkWake;    // virtual time the pending sleep expires at
static PlatformInput g_hkHeld; // held button state, persists across polls
static int g_hkLoaded;

static void hk_die(const char *msg, int lineno) {
  if (lineno > 0) {
    fprintf(stderr, "platform_headless: MLFK_HEADLESS_KEYS line %d %s\n",
            lineno, msg);
  } else {
    fprintf(stderr, "platform_headless: MLFK_HEADLESS_KEYS %s\n", msg);
  }
  exit(2);
}

static void hk_load(const char *path) {
  FILE *sf = fopen(path, "r");
  if (!sf) hk_die("cannot be opened", 0);
  int cap = 64;
  g_hk = (HkCmd *)malloc((size_t)cap * sizeof *g_hk);
  if (!g_hk) hk_die("out of memory", 0);
  char line[256];
  int lineno = 0;
  while (fgets(line, sizeof line, sf)) {
    lineno++;
    size_t len = strlen(line);
    if (len == 0 || line[len - 1] != '\n') {
      // no trailing newline: an over-long line or a truncated final write.
      // Both are corruption (fk_input.c makes the same call).
      hk_die(feof(sf) ? "is missing its trailing newline" : "is too long",
             lineno);
    }
    line[--len] = 0;
    if (len == 0) continue;
    if (memchr(line, '\r', len)) hk_die("carries a CR byte", lineno);
    if (line[0] == '#') continue; // full-line comment, column 0 only
    HkCmd c;
    c.op = line[0];
    c.idx = -1;
    c.ns = 0;
    if (line[0] == 'd' || line[0] == 'u') {
      if (len != 3 || line[1] != ' ' || line[2] < 'a' || line[2] > 'z') {
        hk_die("is malformed (want exactly \"d <a-z>\" / \"u <a-z>\")", lineno);
      }
      for (int i = 0; i < PLATFORM_KEYMAP_ROWS; i++) {
        if (kPlatformKeymap[i].keysym == line[2]) { c.idx = i; break; }
      }
      // An unmapped letter is not "no-op input", it is a script written
      // against a keymap this build does not have.
      if (c.idx < 0) hk_die("names a letter no keymap row carries", lineno);
    } else if (line[0] == 's') {
      if (len < 3 || len > 7 || line[1] != ' ') {
        hk_die("is malformed (want \"s <1-5 digits>\")", lineno);
      }
      long ms = 0;
      for (size_t i = 2; i < len; i++) {
        if (line[i] < '0' || line[i] > '9') {
          hk_die("has a non-digit in its sleep value", lineno);
        }
        ms = ms * 10 + (line[i] - '0');
      }
      if (ms > 60000) hk_die("sleeps longer than 60000 ms", lineno);
      c.ns = (uint64_t)ms * 1000000ull;
    } else {
      hk_die("is not one of \"\", \"#...\", \"d k\", \"u k\", \"s N\"", lineno);
    }
    if (g_hkN == cap) {
      cap *= 2;
      HkCmd *bigger = (HkCmd *)realloc(g_hk, (size_t)cap * sizeof *g_hk);
      if (!bigger) hk_die("out of memory", 0);
      g_hk = bigger;
    }
    g_hk[g_hkN++] = c;
  }
  if (ferror(sf)) hk_die("could not be read to the end", 0);
  if (fclose(sf) != 0) hk_die("could not be closed", 0);
  if (g_hkN == 0) hk_die("carries no commands at all", 0);
  memset(&g_hkHeld, 0, sizeof g_hkHeld);
}

void platform_poll(PlatformInput *in) {
  const char *ks = getenv("MLFK_HEADLESS_KEYS");
  if (!ks || !ks[0]) {
    memset(in, 0, sizeof *in); // historic behaviour, byte for byte
    return;
  }
  if (!g_hkLoaded) {
    g_hkLoaded = 1;
    hk_load(ks);
  }
  // Apply every command whose scheduled virtual time has arrived. Held state
  // survives between polls, exactly as hardware does — a `d s` with no `u s`
  // reads as START held down from then on, and it keeps reading that way past
  // the end of the script, where fk_input.c:231-233 would instead release
  // everything. That divergence is deliberate; the header's deviation (b)
  // says why.
  //
  // g_hkWake is an ABSOLUTE schedule offset (the running sum of the script's
  // sleeps), not `now + ns`. Two consequences, both wanted: the schedule
  // cannot drift by a fraction of a frame per sleep, and a sleep shorter than
  // one frame does not silently cost a whole extra poll — the loop keeps
  // draining while the clock is already past the next command's slot.
  while (g_hkPos < g_hkN && g_hkNow >= g_hkWake) {
    const HkCmd *c = &g_hk[g_hkPos];
    g_hkPos++;
    if (c->op == 's') {
      g_hkWake += c->ns;
      continue;
    }
    *platform_keymap_field(&g_hkHeld, c->idx) = (c->op == 'd');
  }
  *in = g_hkHeld;
  g_hkNow += HK_QUANTUM_NS;
}

void platform_quit(void) {}

// A12c artwork loader (platform.h): there is no OS artwork on a host run,
// and no decoder here by design (this backend links no SDL at all). The
// caller's documented fallback — a dimmed snapshot of the live frame — is
// what host/CI runs therefore always render.
int platform_image_load565(const char *path, uint16_t *out, uint8_t *opaque) {
  (void)path;
  (void)out;
  (void)opaque;
  return 0;
}

// --- audio (M3 task 6): ACCEPT-AND-IDLE ---------------------------------------
// No callback thread ever runs headless: start succeeds so the app's
// main-thread event-scheduling path (mixer voice bookkeeping) runs
// deterministically on host truth legs, but no audio is rendered and
// the granted spec reports 0/0/0 — the check pins those fields per leg,
// so a headless run can never masquerade as a device audio run.
static int g_pa_headless_open;

int platform_audio_start(PlatformAudioFill fill, void *ud, int samples) {
  (void)fill;
  (void)ud;
  if (samples <= 0 || samples > 65535) return 1;
  g_pa_headless_open = 1;
  return 0;
}

void platform_audio_stop(void) { g_pa_headless_open = 0; }
void platform_audio_lock(void) {}
void platform_audio_unlock(void) {}

void platform_audio_stats(PlatformAudioStats *out) {
  memset(out, 0, sizeof *out); // zeros: cbs/underruns/badlen, spec 0/0/0
}
