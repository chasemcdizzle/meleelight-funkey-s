// port/foh/foh_pause.c — in-match pause overlay (punch-list A11/A12).
// Contract, faithfulness argument and provenance: foh_pause.h.

#include "foh_pause.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../gfx/attrib.h" // attrib_mono_ns(): the ONE shared CLOCK_MONOTONIC
#include "../gfx/pace.h"   // pace_sleep_until_ns(): the shared frame pacer
#include "../gfx/platform.h"
#include "foh.h" // foh_text / foh_text2 (foh_font.c) — reused, never copied

FohPauseResult (*foh_pause_hook)(Raster *rz, uint64_t *pausedNs,
                                 long *presentFails) = 0;

#define PAUSE_BUDGET_NS 16666667ull

// The overlay is owner-APPROVED as it stood; C19 adds exactly ONE row, the
// endGame destination for this match kind, in reach right after RESUME.
enum { OPT_RESUME = 0, OPT_SELECT, OPT_MENU, OPT_FRONTEND, OPT_COUNT };

// Widest label decides the panel: foh_text2_width is (n*7-1)*scale, so at
// scale 2 the longest label here ("QUIT TO FRONTEND"-class wording) must stay
// inside the 188 px inner panel below — 13 characters is the budget. Keep any
// new wording inside it.
static const char *opt_name(int i) {
  switch (i) {
    // VS only — the overlay has no target arm to label (foh_pause.h).
    case OPT_SELECT: return "VS SCREEN";
    case OPT_MENU: return "QUIT TO MENU";
    case OPT_FRONTEND: return "QUIT TO OS";
    default: return "RESUME";
  }
}

// Panel geometry (240x240; RAST_W/RAST_H). Four rows at 84/108/132/156, the
// last ending at 176, inside a 46..200 panel.
#define PANEL_X0 26
#define PANEL_X1 214
#define PANEL_Y0 46
#define PANEL_Y1 200
#define ROW0_Y 84
#define ROW_DY 24

static const RastCol kTitle = {255, 200, 60, 256};
static const RastCol kItem = {210, 210, 225, 256};
static const RastCol kItemSel = {255, 240, 200, 256};
static const RastCol kHint = {130, 130, 155, 256};

static inline uint16_t pack565(int r, int g, int b) {
  return (uint16_t)(((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3));
}

static void fb_rect(uint16_t *fb, int x0, int y0, int x1, int y1, uint16_t c) {
  if (x0 < 0) x0 = 0;
  if (y0 < 0) y0 = 0;
  if (x1 > RAST_W) x1 = RAST_W;
  if (y1 > RAST_H) y1 = RAST_H;
  for (int y = y0; y < y1; y++)
    for (int x = x0; x < x1; x++) fb[y * RAST_W + x] = c;
}


// EVIDENCE ONLY (C19 / A12c demonstration). BOTH overlays freeze the driver's
// tick loop, so the rig's tick-indexed shot machinery can never fire while one
// is open — and the rig's marker trigger is the very MENU key that opens the
// system menu. An overlay that cannot be photographed cannot be evidenced, so
// each dumps its OWN frame when MLFK_MENU_SHOT names a directory.
//
// WHO SETS IT (kept accurate — this note used to say "unset in every check",
// which stopped being true when the R3 successor rig landed): the product
// launcher never sets it, and neither does any evidence or conformance leg.
// EXACTLY ONE check does — `port/foh/check-live-arms.sh`, whose whole purpose
// is to execute these overlays and then READ the frames they photograph,
// because an overlay that freezes the driver's tick loop cannot be captured
// by the tick-indexed shot machinery. That is the "deliberate demo run" this
// hook was written for, so it is now exercised rather than merely available.
// Same P6 format as foh_dev.c's write_shot_ppm.
//
// EVERY step is checked and a failure is FATAL (review-mexit-r2 Low). The
// env var is set only on a run whose entire purpose is the resulting file, so
// a truncated path, a full filesystem or a short write must not leave a
// plausible-looking partial PPM behind and report nothing: that is precisely
// PROCESS §3's "a silent pass is the defect". Dying is still safe by
// construction — no player and no conformance leg can reach this path, and
// the one check that does WANTS the loud death. Same discipline as
// foh_dev.c's write_shot_ppm (gfx_fatal is raster.h's host-provided abort,
// already this TU's death path).
static void overlay_selfshot(const uint16_t *fb, const char *name) {
  const char *dir = getenv("MLFK_MENU_SHOT");
  if (!dir || !*dir) return;
  static int seq;
  char path[512];
  if (snprintf(path, sizeof path, "%s/%s-%d.ppm", dir, name, ++seq) >=
      (int)sizeof path) {
    gfx_fatal("MLFK_MENU_SHOT path overflow");
  }
  FILE *f = fopen(path, "wb");
  if (!f) gfx_fatal("cannot open MLFK_MENU_SHOT ppm for writing");
  if (fprintf(f, "P6\n%d %d\n255\n", RAST_W, RAST_H) < 0) {
    gfx_fatal("MLFK_MENU_SHOT header write failed");
  }
  for (int k = 0; k < RAST_W * RAST_H; k++) {
    const uint16_t px = fb[k];
    const uint8_t rgb[3] = {(uint8_t)(((px >> 11) & 0x1F) << 3),
                            (uint8_t)(((px >> 5) & 0x3F) << 2),
                            (uint8_t)((px & 0x1F) << 3)};
    if (fwrite(rgb, 1, 3, f) != 3) gfx_fatal("MLFK_MENU_SHOT write failed");
  }
  if (fclose(f) != 0) gfx_fatal("MLFK_MENU_SHOT close/flush failed");
}

static void text2_center(Raster *rz, int y, int scale, const char *s,
                         RastCol col) {
  const int w = foh_text2_width(s, scale);
  foh_text2(rz, (RAST_W - w) / 2, y, scale, 0, s, col);
}

// --- the shared BOUNDED release drain (foh_pause.h) -------------------------

// Name every action-bearing field that is still down, so the timeout line says
// WHAT it waited for rather than merely that it waited.
//
// GENERATED from platform.h's PLATFORM_ACTION_FIELDS, the same list
// platform_input_idle() is generated from (review-r3-r6 Low: the earlier form
// duplicated the field names here and claimed the two "can never disagree",
// which was exactly backwards — a hand-copied list is how they WOULD). A new
// field now joins the predicate and this diagnostic in one edit or neither.
static void drain_held_names(const PlatformInput *p, char *out, size_t cap) {
#define PLATFORM_X_NAME(f) #f,
  static const char *const kName[] = {PLATFORM_ACTION_FIELDS(PLATFORM_X_NAME)};
#undef PLATFORM_X_NAME
#define PLATFORM_X_DOWN(f) p->f,
  const bool down[] = {PLATFORM_ACTION_FIELDS(PLATFORM_X_DOWN)};
#undef PLATFORM_X_DOWN
  size_t n = 0;
  out[0] = 0;
  for (size_t i = 0; i < sizeof down / sizeof *down; i++) {
    if (!down[i]) continue;
    const size_t need = strlen(kName[i]) + 1; // leading space
    if (n + need + 1 > cap) break;            // truncate, never overflow
    out[n++] = ' ';
    memcpy(out + n, kName[i], need - 1);
    n += need - 1;
    out[n] = 0;
  }
  // `still held:` with nothing after it would be a lie: the loop only reaches
  // the timeout because platform_input_idle() was false on the last poll, so
  // an empty list means the two disagree and that is worth saying out loud.
  // snprintf, not memcpy with a counted length: the earlier form copied a
  // hand-counted 25 bytes of a literal that is neither 25 bytes long nor
  // NUL-terminated at that offset, so `%s` would have read past it
  // (review-r3-r2 Low). ASCII only, for the same reason the count was wrong —
  // a multi-byte dash makes byte arithmetic on a literal a trap.
  if (n == 0) snprintf(out, cap, " (none - idle disagrees)");
}

FohDrainResult foh_drain_release(const char *site, Raster *rz,
                                 long *presentFails, int *presentDead,
                                 PlatformInput *last) {
  PlatformInput in;
  memset(&in, 0, sizeof in);
  for (long i = 0; i < FOH_DRAIN_MAX_POLLS; i++) {
    const uint64_t deadline = attrib_mono_ns() + PAUSE_BUDGET_NS;
    platform_poll(&in);
    if (last) *last = in;
    // The other legitimate way out of a held button. SDL_QUIT is ONE-SHOT
    // (platform_sdl1.c), so it is reported UP rather than swallowed here.
    if (in.quit) return FOH_DRAIN_QUIT;
    if (platform_input_idle(&in)) return FOH_DRAIN_IDLE;
    if (!*presentDead && platform_present(rz->fb) != 0) {
      (*presentFails)++;
      *presentDead = 1; // stop presenting, keep draining
    }
    pace_sleep_until_ns(deadline);
  }
  char held[160];
  drain_held_names(&in, held, sizeof held);
  fprintf(stderr, "foh_drain: TIMEOUT %s after %d polls, still held:%s\n",
          site, FOH_DRAIN_MAX_POLLS, held);
  return FOH_DRAIN_TIMEOUT;
}

FohPauseResult foh_pause_open(Raster *rz, uint64_t *pausedNs,
                              long *presentFails) {
  const uint64_t t0 = attrib_mono_ns();

  // The frozen frame, dimmed ONCE (the fk_menu.c pattern: >>1 & 0x7BEF
  // halves every 565 channel). Static, not stack: 115 KB.
  static uint16_t backdrop[RAST_W * RAST_H];
  for (int i = 0; i < RAST_W * RAST_H; i++)
    backdrop[i] = (uint16_t)((rz->fb[i] >> 1) & 0x7BEF);

  // The match renderer leaves its own clip band set; text must not be
  // scissored by it. Restored on the way out.
  const int clipY0 = rz->clipY0, clipY1 = rz->clipY1;
  rz->clipY0 = 0;
  rz->clipY1 = RAST_H;

  const uint16_t cPanel = pack565(18, 18, 34);
  const uint16_t cEdge = pack565(90, 160, 255);
  const uint16_t cSel = pack565(52, 62, 110);

  // Seed the edge detector from the CURRENT state so the button that
  // opened this menu (still held) cannot also act inside it.
  PlatformInput prev;
  platform_poll(&prev);
  // SDL_QUIT is ONE-SHOT: platform_poll memsets the struct and only sets
  // .quit for events drained by THAT call (platform_sdl1.c:117-121). The
  // seed poll above can therefore swallow a pending quit, so latch it and
  // carry it forward instead of dropping it on the floor.
  int sawQuit = prev.quit ? 1 : 0;

  int sel = OPT_RESUME;
  int shot = 0;
  int presentDead = 0; // latched: once the display is proven dead, count it
                       // ONCE and stop presenting (never abandon the loop
                       // state machine, and never re-count the same fault).
  FohPauseResult out = FOH_PAUSE_RESUME;
  for (;;) {
    const uint64_t deadline = attrib_mono_ns() + PAUSE_BUDGET_NS;

    PlatformInput in;
    platform_poll(&in);
#define EDGE(f) (in.f && !prev.f)
    if (in.quit) sawQuit = 1;
    int done = 0;
    if (sawQuit) { // backend asked us to go (no window manager on device)
      out = FOH_PAUSE_QUIT_FRONTEND;
      done = 1;
    } else if (EDGE(up)) {
      sel = (sel + OPT_COUNT - 1) % OPT_COUNT;
    } else if (EDGE(down)) {
      sel = (sel + 1) % OPT_COUNT;
    } else if (EDGE(a)) {
      switch (sel) {
        case OPT_SELECT: out = FOH_PAUSE_QUIT_SELECT; break;
        case OPT_MENU: out = FOH_PAUSE_QUIT_MENU; break;
        case OPT_FRONTEND: out = FOH_PAUSE_QUIT_FRONTEND; break;
        default: out = FOH_PAUSE_RESUME; break;
      }
      done = 1;
    } else if (EDGE(b) || EDGE(start) || EDGE(menu)) {
      // B cancels; START toggles back out (it opened this). MENU is accepted
      // here too, but ONLY as a way out: MENU no longer OPENS this overlay
      // (review-mexit-r5 Low) — since A12b it opens the FunKey SYSTEM menu,
      // and the match loops dispatch the two on different buttons. Kept
      // because a player who reaches for MENU to dismiss a modal should not be
      // trapped in it.
      out = FOH_PAUSE_RESUME;
      done = 1;
    }
#undef EDGE
    prev = in;
    if (done) break;

    memcpy(rz->fb, backdrop, sizeof backdrop);
    fb_rect(rz->fb, PANEL_X0, PANEL_Y0, PANEL_X1, PANEL_Y1, cEdge);
    fb_rect(rz->fb, PANEL_X0 + 2, PANEL_Y0 + 2, PANEL_X1 - 2, PANEL_Y1 - 2,
            cPanel);
    fb_rect(rz->fb, PANEL_X0 + 6, ROW0_Y + sel * ROW_DY - 4, PANEL_X1 - 6,
            ROW0_Y + sel * ROW_DY + 20, cSel);

    text2_center(rz, PANEL_Y0 + 12, 2, "PAUSED", kTitle);
    for (int i = 0; i < OPT_COUNT; i++)
      text2_center(rz, ROW0_Y + i * ROW_DY, 2, opt_name(i),
                   i == sel ? kItemSel : kItem);
    foh_text(rz, PANEL_X0 + 12, PANEL_Y1 - 18, 1, "A SELECT   B RESUME",
             kHint);

    if (!shot) {
      shot = 1;
      // "pause-vs" unconditionally: this overlay only ever opens in a VS
      // match (foh_pause.h). The old `targetMode ? "pause-target" : ...` arm
      // named a file no run could produce; no check or frozen expectation
      // referenced either name (grep-measured, whole tree).
      overlay_selfshot(rz->fb, "pause-vs");
    }
    if (platform_present(rz->fb) != 0) {
      // A dead display cannot be navigated. Count it into the caller's
      // `failed presents` total (the number the device checks gate on) and
      // hand control back rather than trapping the player in an invisible
      // menu — the match loop's own presents keep counting from there.
      (*presentFails)++;
      presentDead = 1;
      out = FOH_PAUSE_RESUME;
      break;
    }
    pace_sleep_until_ns(deadline);
  }

  // RESUME only: do not hand a STILL-HELD dismiss button to gameplay. The
  // caller's next poll goes straight into the S1 row, so a held A/B would
  // swing an attack on the resume frame and a held START would hit target
  // mode's endGame arm. Keep presenting the frozen frame until release.
  //
  // BOUNDED since the R3 precondition (foh_pause.h): this used to be an
  // unbounded `for (;;)` on the argument that a finger which never lifts is
  // indistinguishable from staying paused. True of a thumb, false of the
  // scripted host injector, which HOLDS ITS LAST KEY STATE AT EOF by design —
  // so the loop could never end and the process hung instead of failing.
  // *pausedNs still keeps the pace epoch honest for however long it waited.
  //
  // presentDead is carried in from the loop above: a failed present must not
  // ABANDON the drain (returning with the button still held is the exact bug
  // this loop exists to prevent), and one dead display must not be counted
  // twice. EVERY action-bearing field is waited on, not just the dismiss
  // buttons — the overlay navigates with up/down too and the caller's next
  // poll goes straight into gameplay (review-mexit-r1 Medium).
  if (out == FOH_PAUSE_RESUME) {
    // `last` is NULL here on purpose: this caller re-seeds by re-polling the
    // moment the overlay returns (foh_dev.c's `platform_poll(&pin)` right
    // after the hook), which is mechanism (b) in foh_pause.h.
    switch (foh_drain_release("pause-release", rz, presentFails, &presentDead,
                              0)) {
      case FOH_DRAIN_QUIT:
        out = FOH_PAUSE_QUIT_FRONTEND;
        break;
      case FOH_DRAIN_TIMEOUT: // diagnosed inside the drain; still a RESUME —
      case FOH_DRAIN_IDLE:    // the caller's re-poll models the held button
        break;
    }
  }

  rz->clipY0 = clipY0;
  rz->clipY1 = clipY1;
  *pausedNs = attrib_mono_ns() - t0;
  return out;
}

// =========================================================================
// The FunKey S SYSTEM menu (punch-list A12b) — VOLUME / BRIGHTNESS / QUIT /
// POWER OFF, opened by MENU/HOME wherever the player is. Contract,
// provenance (Chase's own MIT ssb64 fk_menu.c) and the SDL-rule argument are
// in foh_pause.h. It lives in THIS TU rather than its own because a new TU
// would have to be threaded through five shared build lists (riglib.sh,
// check-device-{foh,persist,target,fullgame}.sh) that other lanes own, and
// the two overlays are the same thing: a modal that freezes the caller's
// loop, presents itself, and returns a verdict. They share the dim, the
// panel helpers, the dead-display latch and the release drain below.
// =========================================================================

int (*foh_sysmenu_hook)(Raster *rz, uint64_t *pausedNs,
                        long *presentFails) = 0;

#define SYS_BUDGET_NS 16666667ull

// The OS's own asset directory. Overridable, but note what that does and does
// not buy (review-mexit-r5 Low): only the SDL1 DEVICE backend implements
// platform_image_load565; both host backends return 0, so a host run always
// renders the dimmed-frame fallback no matter where this points. The override
// exists for a device whose assets live elsewhere; the device itself never
// sets it (fk_menu.c:27).
#define MENU_DIR_DEFAULT "/usr/games/menu_resources/"

// Geometry carried from the donor (fk_menu.c:32-35) — the panel inside
// zone_bg is 180x140 at (30,50), which is where MENU_BG_SQUARE_HEIGHT comes
// from.
#define MENU_BG_SQUARE_HEIGHT 140
#define BAR_W 100
#define BAR_H 20
#define PAD_Y 18

enum { SYS_OPT_VOLUME = 0, SYS_OPT_BRIGHTNESS, SYS_OPT_QUIT, SYS_OPT_POWEROFF, SYS_OPT_COUNT };

static const char *const kSysOptName[SYS_OPT_COUNT] = {"VOLUME", "BRIGHTNESS", "QUIT",
                                                "POWER OFF"};

// zone_bg's panel body is (236,236,236) — the level the hollowed bar cells
// are punched out to, so the bar reads as cut into the panel rather than
// painted on it. Transparency is the ASSET's own (colour key or alpha) and
// is resolved by the platform loader, which hands back an opacity mask.
#define PANEL_LEVEL 236

// --- the zone_bg asset ---------------------------------------------------

// Loaded once per process. `state` is 0 = untried, 1 = loaded, -1 = absent.
static int g_zoneState;
static uint16_t g_zone[RAST_W * RAST_H];
static uint8_t g_zoneOpaque[RAST_W * RAST_H];

static void menu_res_path(char *out, size_t cap, const char *file) {
  const char *dir = getenv("MLFK_MENU_RES");
  if (!dir || !*dir) dir = MENU_DIR_DEFAULT;
  const size_t n = strlen(dir);
  const int needSlash = (n > 0 && dir[n - 1] != '/');
  snprintf(out, cap, "%s%s%s", dir, needSlash ? "/" : "", file);
}

// The OS's own zone_bg.png, decoded by the platform seam (platform.h;
// CLAUDE.md gives exactly ONE TU the SDL API, so the decode cannot live
// here). A failure of ANY kind — no file, no decoder on the device, wrong
// size, unsupported format — leaves g_zoneState at -1 and the overlay falls
// back to a dimmed snapshot of the live frame, which is the donor's own
// documented contract (ssb64 fk_menu.c:83-87).
//
// review-mexit-r1 Medium, and it was a real defect: the first version read
// `zone_bg.bmp` with a hand-rolled BMP parser, and the OS ships PNG only
// (fk_menu.c:29 `MENU_DIR "zone_bg.png"`), so the authentic panel could
// never load on any device — the fallback was not a fallback, it was the
// only path. It also carried its own bounds/truncation/overflow surface,
// which this deletes outright.
static void zone_load(void) {
  if (g_zoneState != 0) return;
  g_zoneState = -1;
  char path[512];
  menu_res_path(path, sizeof path, "zone_bg.png");
  if (platform_image_load565(path, g_zone, g_zoneOpaque)) g_zoneState = 1;
}

// --- the OS shell tools (fk_menu.c:39-47, :102-103) ------------------------

// STRICT grammar (PROCESS §3 whitelist rule; review-mexit-r1 Medium): the
// tool must exit 0 and print exactly one line holding nothing but decimal
// digits in 0..100, optionally followed by a newline. atoi() previously
// accepted "abc" as 0, "50%%" as 50, and a FAILED command's empty output as
// a real reading — a level the menu would then write straight back to the
// OS. Anything that resembles-but-does-not-match is corruption, so the
// documented fallback is used instead.
// RAW BYTES, never fgets (review-mexit-r2 Medium): fgets stops at the
// newline, but an embedded NUL makes strlen() shorter than what was actually
// read, so `50\0junk\n` was consumed whole — leaving nothing to trip the
// "extra output" test — and then measured as the 2 bytes "50". The grammar
// has to be anchored to the byte COUNT fread reports, not to a C string.
// BOUNDED READ, and it does NOT drain to EOF (review-mexit-r5 Low corrects an
// earlier claim here that it did): the single fread below asks for at most
// sizeof buf and deliberately leaves any tail unread, because "the tool wrote
// more than a small integer" is exactly the corruption the grammar rejects. An
// oversized child can therefore meet SIGPIPE at pclose. That is accepted: this
// path runs only for `volume get` / `brightness get`, whose output is one
// short line, and a tool that floods us is already being refused. Do NOT
// "fix" it by draining — draining would make an unbounded child hold the
// sysmenu loop for as long as it cares to write.
static int read_cmd_int(const char *cmd, int fallback) {
  FILE *p = popen(cmd, "r");
  if (!p) return fallback;
  char buf[32];
  const size_t n = fread(buf, 1, sizeof buf, p);
  const int st = pclose(p);
  // n == sizeof buf means the tool printed at least 32 bytes: past the whole
  // legal domain ("100\n" is 4), and the tail is unread, so it is refused
  // rather than measured from a prefix.
  if (st != 0 || n == 0 || n == sizeof buf) return fallback;
  size_t len = n;
  if (buf[len - 1] == '\n') len--; // exactly one optional trailing newline
  if (len == 0 || len > 3) return fallback;
  int v = 0;
  for (size_t i = 0; i < len; i++) {
    // A NUL, a '\r', a '%', a space or a second line all fail here: the
    // whole measured byte range must be decimal digits and nothing else.
    if (buf[i] < '0' || buf[i] > '9') return fallback;
    v = v * 10 + (buf[i] - '0');
  }
  return (v <= 100) ? v : fallback;
}

static void set_level(const char *tool, int pct) {
  char cmd[64];
  snprintf(cmd, sizeof cmd, "%s set %d", tool, pct);
  // Missing tool => non-zero rc and nothing happens, which is the donor's
  // "degrade to a no-op" contract. Explicitly ignored, not silently dropped.
  if (system(cmd) != 0) { /* tool absent or refused — menu stays usable */ }
}

// --- drawing --------------------------------------------------------------

// The donor blits arrow_top.png / arrow_bottom.png; those ship as PNG only,
// so a filled triangle stands in for them (fk_menu.c:156-159).
static void draw_arrow(uint16_t *fb, int cy, int up, uint16_t c) {
  const int hgt = 6, halfW = 9;
  for (int i = 0; i < hgt; i++) {
    const int y = up ? cy - hgt / 2 + i : cy + hgt / 2 - i;
    const int half = (halfW * i) / hgt;
    fb_rect(fb, RAST_W / 2 - half, y, RAST_W / 2 + half + 1, y + 1, c);
  }
}

// Segmented progress bar, FunKey style: filled cells solid, empty cells
// hollowed to the panel colour (fk_menu.c:57-72).
static void draw_bar(uint16_t *fb, int x, int y, int pct, uint16_t fg,
                     uint16_t hollow) {
  const int cells = 10, gap = 2;
  const int cw = (BAR_W - gap * (cells - 1)) / cells;
  const int full = (pct * cells + 50) / 100;
  for (int i = 0; i < cells; i++) {
    const int rx = x + i * (cw + gap);
    fb_rect(fb, rx, y, rx + cw, y + BAR_H, fg);
    if (i >= full)
      fb_rect(fb, rx + 1, y + 1, rx + cw - 1, y + BAR_H - 1, hollow);
  }
}

int foh_sysmenu_open(Raster *rz, uint64_t *pausedNs, long *presentFails) {
  const uint64_t t0 = attrib_mono_ns();
  zone_load();
  const int haveZone = (g_zoneState == 1);

  // The frozen frame, dimmed ONCE — the donor's exact expression
  // (fk_menu.c:138-139), which is also foh_pause.c's.
  static uint16_t backdrop[RAST_W * RAST_H];
  for (int i = 0; i < RAST_W * RAST_H; i++)
    backdrop[i] = (uint16_t)((rz->fb[i] >> 1) & 0x7BEF);

  // The match renderer leaves its own clip band set; text must not be
  // scissored by it. Restored on the way out.
  const int clipY0 = rz->clipY0, clipY1 = rz->clipY1;
  rz->clipY0 = 0;
  rz->clipY1 = RAST_H;

  // Dark grey on the light zone panel; white on the dimmed-frame fallback
  // (fk_menu.c:99-100).
  const RastCol label = haveZone ? (RastCol){85, 85, 85, 256}
                                 : (RastCol){236, 236, 236, 256};
  const RastCol hint = haveZone ? (RastCol){85, 85, 85, 256}
                                : (RastCol){170, 170, 170, 256};
  const uint16_t labelPk = pack565(label.r, label.g, label.b);
  const uint16_t hollowPk =
      pack565(PANEL_LEVEL, PANEL_LEVEL, PANEL_LEVEL);

  int volume = read_cmd_int("volume get", 50);
  int bright = read_cmd_int("brightness get", 50);

  // Seed the edge detector from the CURRENT state so the MENU press that
  // opened this overlay (still held) cannot also act inside it.
  PlatformInput prev;
  platform_poll(&prev);
  int sawQuit = prev.quit ? 1 : 0; // SDL_QUIT is one-shot; latch it

  int sel = SYS_OPT_VOLUME;
  int shot = 0;
  int result = 0;
  int presentDead = 0; // latched: count a dead display ONCE, keep the loop
  for (;;) {
    const uint64_t deadline = attrib_mono_ns() + SYS_BUDGET_NS;

    PlatformInput in;
    platform_poll(&in);
#define EDGE(f) (in.f && !prev.f)
    if (in.quit) sawQuit = 1;
    int done = 0;
    if (sawQuit) {
      result = 1; // the backend asked us to go
      done = 1;
    } else if (EDGE(up)) {
      sel = (sel + SYS_OPT_COUNT - 1) % SYS_OPT_COUNT;
    } else if (EDGE(down)) {
      sel = (sel + 1) % SYS_OPT_COUNT;
    } else if (EDGE(left)) {
      if (sel == SYS_OPT_VOLUME) {
        volume = volume > 0 ? volume - 10 : 0;
        set_level("volume", volume);
      } else if (sel == SYS_OPT_BRIGHTNESS) {
        bright = bright > 0 ? bright - 10 : 0;
        set_level("brightness", bright);
      }
    } else if (EDGE(right)) {
      if (sel == SYS_OPT_VOLUME) {
        volume = volume < 100 ? volume + 10 : 100;
        set_level("volume", volume);
      } else if (sel == SYS_OPT_BRIGHTNESS) {
        bright = bright < 100 ? bright + 10 : 100;
        set_level("brightness", bright);
      }
    } else if (EDGE(a)) {
      if (sel == SYS_OPT_QUIT) {
        result = 1;
        done = 1;
      } else if (sel == SYS_OPT_POWEROFF) {
        // The OS takes the device down here; if it declines, close.
        if (system("powerdown handle") != 0) { /* refused — just close */ }
        done = 1;
      }
    } else if (EDGE(b) || EDGE(menu)) {
      done = 1; // B backs out; MENU toggles back out (it opened this)
    }
#undef EDGE
    prev = in;
    if (done) break;

    memcpy(rz->fb, backdrop, sizeof backdrop);
    if (haveZone) {
      for (int i = 0; i < RAST_W * RAST_H; i++)
        if (g_zoneOpaque[i]) rz->fb[i] = g_zone[i];
    }

    // ONE option at a time, centred — the donor's whole layout idea
    // (fk_menu.c:145-154).
    const int hasBar = (sel == SYS_OPT_VOLUME || sel == SYS_OPT_BRIGHTNESS);
    const int labelH = 9 * 2; // our face at scale 2
    const int labelY = RAST_H / 2 - labelH / 2 - (hasBar ? PAD_Y : 0);
    text2_center(rz, labelY, 2, kSysOptName[sel], label);
    if (hasBar)
      draw_bar(rz->fb, (RAST_W - BAR_W) / 2, RAST_H / 2 - BAR_H / 2 + PAD_Y,
               sel == SYS_OPT_VOLUME ? volume : bright, labelPk, hollowPk);

    const int arrowY = (RAST_H - MENU_BG_SQUARE_HEIGHT) / 4;
    draw_arrow(rz->fb, arrowY, 1, labelPk);
    draw_arrow(rz->fb, RAST_H - arrowY, 0, labelPk);

    {
      const char *s = "A SELECT   B BACK";
      const int w = foh_text_width(s, 1);
      foh_text(rz, (RAST_W - w) / 2, RAST_H - 22, 1, s, hint);
    }

    if (!shot) {
      shot = 1;
      overlay_selfshot(rz->fb, "sysmenu");
    }
    if (platform_present(rz->fb) != 0) {
      // A dead display cannot be navigated. Count it into the caller's
      // `failed presents` total and hand control back rather than trapping
      // the player in an invisible menu — foh_pause.c's argument exactly.
      (*presentFails)++;
      presentDead = 1;
      result = 0;
      break;
    }
    pace_sleep_until_ns(deadline);
  }

  // Do not hand a STILL-HELD dismiss button back to the caller: the very
  // next poll goes into gameplay or the FOH, where a held A/B would act.
  // BOUNDED since the R3 precondition (foh_pause.h — same argument as the
  // pause tail above); *pausedNs keeps the pace epoch honest for the wait.
  // EVERY action-bearing field is waited on: this menu drives volume and
  // brightness with LEFT/RIGHT and moves with UP/DOWN, so a held direction
  // leaked a cursor move or an attack into whatever resumed
  // (review-mexit-r1 Medium).
  //
  // The drain paces at PAUSE_BUDGET_NS, which is SYS_BUDGET_NS: both are the
  // same 16.666667 ms frame period, so sharing one implementation changes no
  // timing here.
  // `last` is NULL for the same reason as the pause tail: both match loops and
  // the FOH phase re-poll into their edge snapshot immediately after this
  // returns (foh_pause.h mechanism (b)).
  switch (foh_drain_release("sysmenu-release", rz, presentFails, &presentDead,
                            0)) {
    case FOH_DRAIN_QUIT:
      result = 1;
      break;
    case FOH_DRAIN_TIMEOUT: // diagnosed inside the drain; the caller's
    case FOH_DRAIN_IDLE:    // re-poll models a still-held button as held
      break;
  }

  rz->clipY0 = clipY0;
  rz->clipY1 = clipY1;
  *pausedNs = attrib_mono_ns() - t0;
  return result;
}
