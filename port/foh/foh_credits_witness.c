// port/foh/foh_credits_witness.c — punch-list A7's witness: the CREDITS
// screen, asserted in the OWNER'S terms rather than in ours.
//
// WHAT A7 IS. The Options menu's CREDITS row used to emit `deny` and a
// registered refusal. Upstream's `src/menus/credits.js` (422 lines) is a Star
// Fox shooting gallery — fourteen contributor names scroll up over a warp
// field and you shoot them with a twin-laser reticle, and hitting one prints
// that person's ROLE and what they DID. MENU-SPEC §8 specifies the port,
// including DEVIATION D12 (the reticle is a relative cursor, because
// upstream's absolute stick map gives a d-pad nine reachable positions) and
// Quirk Q6 (the exit timer runs regardless of fast-forward).
//
// WHAT THIS ASSERTS — every one of them through the REAL foh_tick, driven by
// the REAL gestures, with nothing poked into FohState by hand:
//
//   [A7-1] the CREDITS ROW OPENS THE SCREEN. Three DOWNs and A on the menu
//          top reach Options; three DOWNs and A there reach the credits.
//          That is the owner's complaint answered: the row did nothing.
//   [A7-2] the screen RENDERS A REAL CREDITED NAME. "SCHMOO" — upstream's
//          first ScrollingText (credits.js:115) — is proved to be on screen
//          by OVERDRAWING it at the rect the machine claims, in the colour
//          the renderer claims, and requiring ZERO pixels to change. A
//          screen showing an empty list, or a placeholder, fails here.
//   [A7-3] SHOOTING A NAME SCORES IT, and the panel names THE PERSON YOU
//          SHOT. The reticle is walked onto the SECOND name ("Tatatat0")
//          with the real d-pad, A is pressed, and fifteen ticks later the
//          machine says the bolt landed. Then the info panel is asserted by
//          overdraw: it reads "TATATAT0" and "PROGRAMMER" — that person's
//          own name and their own role from credits.js:116 — and NOT the
//          first credit's. The score readout reads "1 HIT".
//          Index 1 rather than 0 on purpose: a panel wired to a constant
//          index would pass on the first name and fail here.
//   [A7-4] B RETURNS TO OPTIONS ON THE RIGHT ROW. credits.js:236-245 leaves
//          menuMode/menuSelected alone, so the cursor is still on CREDITS —
//          asserted as the emitted transition AND as menuSelected == 3.
//   [A7-5] THE 2500-FRAME TIMER EXIT fires on its own and plays `failure`
//          with fewer than fourteen hits (credits.js:226-235), landing on
//          the same screen and row. Q6's flat +2 is what makes the frame
//          count exactly 2500.
//   [A7-6] the fourteen authored credits are the table foh.c carries, in
//          order. The TEXT comparison against the upstream clone lives in
//          port/foh/check-credits.sh (it needs the clone); what is asserted
//          here is that the RENDERED plane reads out of that same table.
//
// THE INSTRUMENT is foh_legibility_witness.c's, carried whole: a rendered
// string is asserted by OVERDRAWING the claimed string, at the claimed place,
// in the claimed colour, and requiring zero changed pixels. Text composites
// opaquely (foh_font.c, `col.a256` == 256 for every colour used here), so
// drawing what is already there is a no-op and drawing anything else is not.
//
// Usage: foh_credits_witness. Prints one line per assertion and `CREDITS OK`
// on success, exit 0. Any failure prints `CREDITS FAIL:` and exits 1. Its
// negative tests live in port/foh/check-credits.sh.
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../gfx/raster.h"
#include "foh.h"

// gfx_fatal is host-provided (raster.h), the foh_legibility_witness.c pattern.
void gfx_fatal(const char *what) {
  fprintf(stderr, "CREDITS FAIL: gfx_fatal: %s\n", what);
  exit(3);
}

// --- the numbers this witness HAND COPIES from foh_render.c -----------------
// Every one is pinned line-for-line by check-credits.sh, the
// check-legibility.sh discipline. If the renderer repaints a colour or moves
// the panel, this dies loudly instead of asserting against a screen that no
// longer exists.
static const RastCol kCredWhite = {255, 255, 255, 256};
static const RastCol kCredShotName = {227, 89, 89, 256};
#define CRED_PANEL_CX 120  // cred_text_center's x for the name and role rows
#define CRED_NAME_Y 181    // the name row's text y
#define CRED_ROLE_Y 192    // the role row's text y
#define CRED_SCORE_CX 215  // the score readout's centre
#define CRED_SCORE_Y 20

// --- THE CREDITED NAMES THIS WITNESS ASSERTS, AS LITERALS -------------------
// NOT `foh_credits[i].name`. That would be SELF-REFERENTIAL: a table edited to
// a placeholder would be rendered as the placeholder and asserted as the
// placeholder, and the check would stay green on the exact defect the ticket
// names. These are upstream's own strings (credits.js:115-116), uppercased for
// face 1 the way the renderer uppercases them, and check-credits.sh leg [2]
// re-extracts them from the pinned clone and requires THESE LITERALS to be
// what upstream says — so the constant below cannot drift either.
#define CRED_W_NAME0 "SCHMOO"     // credits.js:115
#define CRED_W_NAME1 "TATATAT0"   // credits.js:116
#define CRED_W_ROLE1 "PROGRAMMER" // credits.js:116

static int g_fails;

static void ok(const char *what) { printf("  ok  %s\n", what); }

static void bad(const char *what) {
  printf("CREDITS FAIL: %s\n", what);
  g_fails++;
}

static void want(int cond, const char *what) {
  if (cond) ok(what);
  else bad(what);
}

static Raster g_a, g_probe;

static void render_shot(const FohState *s, Raster *rz) {
  FohState look = *s;
  foh_look_canonical(&look);
  memset(rz, 0, sizeof *rz);
  foh_render(&look, rz);
}

static int fb_diff(const Raster *p, const Raster *q) {
  int n = 0;
  for (int i = 0; i < RAST_W * RAST_H; i++) {
    if (p->fb[i] != q->fb[i]) n++;
  }
  return n;
}

// Pixels that change when `text` is drawn over `live` at (x, y) in `col`.
// Zero means the screen already carries exactly that string, in that colour,
// at that spot.
static int overdraw(const Raster *live, int x, int y, const char *text,
                    RastCol col) {
  g_probe = *live;
  foh_text(&g_probe, x, y, 1, text, col);
  return fb_diff(live, &g_probe);
}

// The renderer uppercases at the draw site (foh_render.c's cred_text_center /
// the name loop), so every claim this witness makes about a credit string has
// to be uppercased the same way before it is drawn back.
static void upper_into(char *dst, size_t cap, const char *src) {
  size_t n = 0;
  for (; src[n] && n + 1 < cap; n++) {
    dst[n] = (src[n] >= 'a' && src[n] <= 'z') ? (char)(src[n] - 'a' + 'A')
                                              : src[n];
  }
  dst[n] = 0;
}

// A centred credit string, exactly where cred_text_center puts it.
static int overdraw_centered(const Raster *live, int cx, int y, const char *s,
                             RastCol col) {
  char up[80];
  upper_into(up, sizeof up, s);
  return overdraw(live, cx - foh_text_width(up, 1) / 2, y, up, col);
}

// --- gestures ---------------------------------------------------------------
static void tick_with(FohState *s, size_t off) {
  PlatformInput in;
  memset(&in, 0, sizeof in);
  if (off != (size_t)-1) *(bool *)((char *)&in + off) = true;
  foh_tick(s, &in);
}

static void neutral(FohState *s) { tick_with(s, (size_t)-1); }

static void press(FohState *s, size_t off) {
  tick_with(s, off);
  neutral(s);
}

#define PRESS(s, field) press((s), offsetof(PlatformInput, field))

static void press_n(FohState *s, size_t off, int n) {
  for (int i = 0; i < n; i++) press(s, off);
}

#define PRESS_N(s, field, n) press_n((s), offsetof(PlatformInput, field), (n))

static bool emitted(const FohState *s, const char *from, const char *to,
                    const char *cause) {
  for (int i = 0; i < s->nev; i++) {
    if (s->ev[i].kind != FOH_EV_TRANS) continue;
    if (!strcmp(s->ev[i].from, from) && !strcmp(s->ev[i].to, to) &&
        !strcmp(s->ev[i].cause, cause)) {
      return true;
    }
  }
  return false;
}

static bool played(const FohState *s, const char *name) {
  for (int i = 0; i < s->nsnd; i++) {
    if (!strcmp(s->snd[i], name)) return true;
  }
  return false;
}

// --- [A7-1] the row opens the screen ----------------------------------------
static void open_credits(FohState *s) {
  foh_init(s);
  int guard = 0;
  while (s->screen != FOH_TITLE && guard++ < 100000) neutral(s);
  want(s->screen == FOH_TITLE, "machine reaches the title screen");
  PRESS(s, start);
  want(s->screen == FOH_MENU_TOP, "START leaves the title for the menu top");
  PRESS_N(s, down, 3);
  PRESS(s, a);
  want(s->screen == FOH_MENU_OPTIONS, "menu top row 3 (OPTIONS) opens options");
  PRESS_N(s, down, 3);
  want(s->menuSelected == 3, "three DOWNs land on the CREDITS row");
  // The A EDGE only — no trailing release tick. Two reasons: the emitted
  // event list is drained by the NEXT tick, so the transition has to be read
  // here; and this leaves the caller owning every credits tick, which is what
  // lets the timer leg count them exactly.
  tick_with(s, offsetof(PlatformInput, a));
  want(s->screen == FOH_CREDITS,
       "A on the CREDITS row OPENS THE CREDITS SCREEN (it used to emit deny "
       "and refuse)");
  want(emitted(s, "menu-options", "credits", "a"),
       "and the machine emits the menu-options>credits>a transition");
}

// --- [A7-2] a real credited name is on screen -------------------------------
// The reticle is parked in the bottom-left corner first, so its ring cannot
// sit on the name being asserted; the name is then caught in the middle band
// of the screen, clear of the info panel (y 179..224) and of the score box
// (y 16..32). Both are properties of WHERE, asserted rather than assumed.
static void park_reticle_bottom_left(FohState *s) {
  PlatformInput in;
  memset(&in, 0, sizeof in);
  in.left = true;
  in.down = true;
  for (int i = 0; i < 120; i++) foh_tick(s, &in);
  memset(&in, 0, sizeof in);
  foh_tick(s, &in);
  want(s->credX == 0.0 && s->credY == (double)RAST_H,
       "the reticle clamps into the bottom-left corner, clear of the names");
}

static void assert_name_on_screen(FohState *s) {
  park_reticle_bottom_left(s);
  FohHandRect r[FOH_CRED_NAMES];
  int guard = 0;
  for (;;) {
    foh_credits_name_rects(s, r);
    if (s->credNameRender[0] && r[0].y >= 100 && r[0].y <= 165) break;
    if (guard++ > 4000) {
      bad("the first credit never scrolled into the middle band");
      return;
    }
    neutral(s);
  }
  foh_credits_name_rects(s, r);
  render_shot(s, &g_a);
  char buf[220];
  const int same = overdraw(&g_a, r[0].x, r[0].y, CRED_W_NAME0, kCredWhite);
  snprintf(buf, sizeof buf,
           "the credit \"%s\" (credits.js:115) is ON SCREEN at x %d y %d "
           "(%d px differ from that string drawn over it)",
           CRED_W_NAME0, r[0].x, r[0].y, same);
  want(same == 0, buf);
  // TOOTH: a placeholder would not be there. Same place, same colour.
  const int ph = overdraw(&g_a, r[0].x, r[0].y, "PLACEHOLDER", kCredWhite);
  snprintf(buf, sizeof buf,
           "TOOTH: the same spot does NOT read a placeholder (%d px would "
           "change)", ph);
  want(ph > 0, buf);
  // and it is drawn WHITE, not the shot colour, because nothing is shot yet.
  const int red = overdraw(&g_a, r[0].x, r[0].y, CRED_W_NAME0, kCredShotName);
  snprintf(buf, sizeof buf,
           "and it is drawn UNSHOT-white, not the hit colour (%d px would "
           "change)", red);
  want(red > 0, buf);
}

// --- [A7-3] shoot the second credit -----------------------------------------
// The bolt is tested against the names WHERE THEY WILL BE when it lands, not
// where they are when it is fired (credits.js:205-222 runs the test on the
// frame the shot's life reaches 15). The names keep scrolling in between, so
// the aim point has to LEAD them — which is the actual skill of the upstream
// screen. The lead is not estimated: the state is CLONED and ticked forward
// the exact number of frames, and the rect read off the clone.
//
// Sixteen, not fifteen: a shot fired on tick F is tested on tick F+15, and
// the names scroll once per tick INCLUDING both of those, so relative to the
// state before tick F they have moved sixteen times.
#define CRED_LEAD 16

static void lead_rect(const FohState *s, int i, FohHandRect *out) {
  FohState c = *s;
  for (int k = 0; k < CRED_LEAD; k++) neutral(&c);
  FohHandRect r[FOH_CRED_NAMES];
  foh_credits_name_rects(&c, r);
  *out = r[i];
}

static void shoot_second_credit(FohState *s) {
  const int idx = 1;
  // Walk the reticle onto where the second name WILL be, one d-pad step at a
  // time, re-reading the lead every tick because the target keeps moving.
  int guard = 0;
  for (;;) {
    FohHandRect t;
    lead_rect(s, idx, &t);
    const double cx = (double)t.x + (double)t.w / 2.0;
    const double cy = (double)t.y + (double)t.h / 2.0;
    const bool onTarget = s->credX >= (double)t.x &&
                          s->credX <= (double)(t.x + t.w) &&
                          s->credY >= (double)t.y &&
                          s->credY <= (double)(t.y + t.h);
    if (onTarget && t.y > 0 && t.y + t.h < RAST_H && s->credCool == 0) break;
    if (guard++ > 6000) {
      bad("the reticle never reached the second credit's lead point");
      return;
    }
    PlatformInput in;
    memset(&in, 0, sizeof in);
    const double dx = cx - s->credX, dy = cy - s->credY;
    if (dx > 1.2) in.right = true;
    else if (dx < -1.2) in.left = true;
    if (dy > 1.9) in.down = true;
    else if (dy < -1.9) in.up = true;
    foh_tick(s, &in);
  }
  // FIRE — the real gesture, one A edge.
  tick_with(s, offsetof(PlatformInput, a));
  want(played(s, "foxlaserfire"),
       "A fires the twin lasers and the machine plays foxlaserfire "
       "(credits.js:192)");
  // Fifteen more ticks: the bolt lands on the fifteenth.
  bool heard = false;
  for (int k = 0; k < 15; k++) {
    neutral(s);
    if (played(s, "targetBreak")) heard = true;
  }
  want(heard, "the bolt lands and the machine plays targetBreak "
              "(credits.js:216)");
  want(s->credNameShot[idx],
       "the SECOND credit is marked shot (not some other one)");
  want(s->credScore == 1, "the score is exactly 1");

  // The panel now names THE PERSON WHO WAS SHOT. Overdraw both fields.
  render_shot(s, &g_a);
  char buf[260];
  {
    const int n = overdraw_centered(&g_a, CRED_PANEL_CX, CRED_NAME_Y,
                                    CRED_W_NAME1, kCredWhite);
    snprintf(buf, sizeof buf,
             "the info panel NAMES the credit that was shot: \"%s\" "
             "(%d px differ)", CRED_W_NAME1, n);
    want(n == 0, buf);
  }
  {
    const int n = overdraw_centered(&g_a, CRED_PANEL_CX, CRED_ROLE_Y,
                                    CRED_W_ROLE1, kCredWhite);
    snprintf(buf, sizeof buf,
             "and gives that person's ROLE: \"%s\" (%d px differ)",
             CRED_W_ROLE1, n);
    want(n == 0, buf);
  }
  {
    // TOOTH: it is NOT the FIRST credit's name. A panel wired to a constant
    // index, or one reading the wrong plane, lands here.
    const int n = overdraw_centered(&g_a, CRED_PANEL_CX, CRED_NAME_Y,
                                    CRED_W_NAME0, kCredWhite);
    snprintf(buf, sizeof buf,
             "TOOTH: the panel does NOT name the first credit \"%s\" "
             "(%d px would change)", CRED_W_NAME0, n);
    want(n > 0, buf);
  }
  {
    const int n =
        overdraw_centered(&g_a, CRED_SCORE_CX, CRED_SCORE_Y, "1 HIT",
                          kCredWhite);
    snprintf(buf, sizeof buf,
             "the score box reads \"1 HIT\" (credits.js:310; %d px differ)",
             n);
    want(n == 0, buf);
  }
  // And the shot name is repainted in the hit colour on the SCROLLING plane
  // (credits.js:364-368). The reticle is sitting on top of it right now — it
  // just shot it — so it is parked out of the way first and the name caught
  // in a clear band, the same discipline the unshot assertion uses.
  park_reticle_bottom_left(s);
  {
    FohHandRect r[FOH_CRED_NAMES];
    int guard = 0;
    for (;;) {
      foh_credits_name_rects(s, r);
      if (s->credNameRender[idx] && r[idx].y >= 30 && r[idx].y <= 165) break;
      if (guard++ > 4000) {
        bad("the shot credit never scrolled into a clear band");
        return;
      }
      neutral(s);
    }
    render_shot(s, &g_a);
    const int red =
        overdraw(&g_a, r[idx].x, r[idx].y, CRED_W_NAME1, kCredShotName);
    snprintf(buf, sizeof buf,
             "the shot credit is repainted in the HIT colour on the scrolling "
             "plane (%d px differ)", red);
    want(red == 0, buf);
    const int white =
        overdraw(&g_a, r[idx].x, r[idx].y, CRED_W_NAME1, kCredWhite);
    snprintf(buf, sizeof buf,
             "TOOTH: and is no longer drawn unshot-white (%d px would change)",
             white);
    want(white > 0, buf);
  }
}

// --- [A7-4] B returns to Options on the CREDITS row -------------------------
static void b_returns_to_options(FohState *s) {
  tick_with(s, offsetof(PlatformInput, b));
  want(s->screen == FOH_MENU_OPTIONS, "B leaves the credits for Options");
  want(emitted(s, "credits", "menu-options", "b"),
       "and emits credits>menu-options>b");
  want(played(s, "menuBack"), "with menuBack (credits.js:236)");
  want(s->menuSelected == 3,
       "the Options cursor is STILL on the CREDITS row (credits.js changes "
       "only the gameMode)");
  neutral(s);
  want(s->screen == FOH_MENU_OPTIONS,
       "and the held B does not re-trigger on the menu the next frame");
}

// --- [A7-5] the 2500-frame timer exit ---------------------------------------
static void timer_exit(void) {
  FohState s;
  open_credits(&s);
  // open_credits leaves the machine ON the credits screen having run ZERO
  // credits ticks (it stops on the A edge itself), so `n` is exactly how many
  // ticks the screen lived for.
  int n = 0;
  bool fired = false;
  while (n < 4000) {
    neutral(&s);
    n++;
    if (s.screen != FOH_CREDITS) { fired = true; break; }
  }
  char buf[200];
  snprintf(buf, sizeof buf,
           "the credits end on their own after %d frames "
           "(cScrollingMax 5000 at +2/frame = 2500; credits.js:25,143,226)",
           n);
  want(fired && n == 2500, buf);
  want(emitted(&s, "credits", "menu-options", "timer"),
       "the timer exit emits credits>menu-options>timer");
  want(played(&s, "failure"),
       "and plays `failure`, because fewer than all fourteen were shot "
       "(credits.js:228-231)");
  want(s.screen == FOH_MENU_OPTIONS && s.menuSelected == 3,
       "landing back on Options with the cursor still on CREDITS");
}

// --- [A7-6] the rendered plane reads out of the authored table --------------
// The TEXT itself is compared against the upstream clone by
// check-credits.sh. What is asserted here is the wiring: every one of the
// fourteen rows is non-empty and distinct, so a table that lost a row or
// duplicated one cannot pass.
static void table_shape(void) {
  int bad_rows = 0, dupes = 0;
  for (int i = 0; i < FOH_CRED_NAMES; i++) {
    if (!foh_credits[i].name[0] || !foh_credits[i].position[0] ||
        !foh_credits[i].info[0] || foh_credits[i].y0 <= 0) {
      bad_rows++;
    }
    for (int j = i + 1; j < FOH_CRED_NAMES; j++) {
      if (!strcmp(foh_credits[i].name, foh_credits[j].name)) dupes++;
    }
  }
  want(bad_rows == 0, "all fourteen credits carry a name, a role and a blurb");
  want(dupes == 0, "and no name appears twice");
}

// --- [A7-7] every authored credit string is RENDERABLE in face 1 -----------
// foh_font.c makes an unknown character a hard gfx_fatal, never a blank, and
// A7 had to add '&' to that face for two of the fourteen blurbs
// (credits.js:117-118, :125-126) — NEITHER of which is the row the panel test
// shoots. Drawing all three fields of all fourteen rows is what keeps that
// guard live: a credit containing a character the face lacks kills this
// witness instead of reaching a player.
static void every_string_renders(void) {
  memset(&g_probe, 0, sizeof g_probe);
  for (int i = 0; i < FOH_CRED_NAMES; i++) {
    char up[128];
    upper_into(up, sizeof up, foh_credits[i].name);
    foh_text(&g_probe, 0, 0, 1, up, kCredWhite);
    upper_into(up, sizeof up, foh_credits[i].position);
    foh_text(&g_probe, 0, 0, 1, up, kCredWhite);
    upper_into(up, sizeof up, foh_credits[i].info);
    foh_text(&g_probe, 0, 0, 1, up, kCredWhite);
  }
  ok("all 42 authored credit strings render in face 1 (a missing glyph is a "
     "gfx_fatal, and two blurbs carry the '&' A7 added for them)");
}

// --- [#27] A RESUMED CREDITS SCREEN IS AN ENTERED ONE ----------------------
//
// Ticket #27 removes this screen's resume redirect, and the redirect's stated
// reason was that the reticle is placed by the ENTERING TRANSITION, which a
// resume never runs. ADR 0001 and ticket #26 both expected that to become the
// second resume hook. IT MUST NOT BE ONE: foh_init already places the reticle
// at the identical home (foh.h's FOH_CRED_HOME_X/Y — ONE macro since this
// ticket, exactly so the two placements cannot drift), so a hook would be a
// provable no-op, and a no-op hook is worse than none because its tooth
// cannot bite.
//
// That is a claim about behaviour, so it is measured here rather than argued
// in a comment. The two arrivals are built side by side — one walks the menu,
// one is foh_dev.c's resume arm (a freshly initialised state whose `screen`
// is SET, with no transition run) — and they are required to agree, first on
// the reticle and then, two hundred frames later, ON THE PIXELS. The pixel
// half is what makes this an assertion about the SCREEN rather than about one
// pair of doubles: anything the entering transition sets up and a resume
// misses would show there.
//
// check-credits.sh's T4 moves foh_init's placement in a COPY of foh.c and
// requires this to fail, which is what stops the claim from resting on two
// literals that agree today.
static void resume_is_an_entry(void) {
  FohState entered;
  open_credits(&entered); // ...having run ZERO credits ticks (see open_credits)

  FohState resumed;
  foh_init(&resumed);
  resumed.screen = FOH_CREDITS;
  // ...and the menu cursor, which a real resume restores from the record's
  // `menusel` row (ticket #27's own field). It is set here because
  // foh_look_canonical derives the menu HUE from menuSelected, so leaving it
  // at zero would make the two shots differ for a reason that has nothing to
  // do with the credits screen. This witness does not link foh_persist.c, so
  // the driver's one line is modelled rather than called.
  resumed.menuSelected = entered.menuSelected;

  want(resumed.credX == entered.credX && resumed.credY == entered.credY,
       "a RESUMED credits screen finds its reticle exactly where an ENTERED "
       "one does — which is why the resume redirect could go, and why this "
       "screen needs no resume hook (ticket #27)");
  want(resumed.credX == (double)RAST_W / 2.0 &&
           resumed.credY == (double)RAST_H / 2.0,
       "and that place is the canvas centre (credits.js:23-24), not merely "
       "some value the two happen to share");

  for (int i = 0; i < 200; i++) {
    neutral(&entered);
    neutral(&resumed);
  }
  want(entered.screen == FOH_CREDITS && resumed.screen == FOH_CREDITS,
       "both are still the credits after 200 frames (the 2500-frame timer has "
       "not fired, so what follows compares two LIVE screens)");
  render_shot(&entered, &g_a);
  render_shot(&resumed, &g_probe);
  {
    char buf[240];
    const int d = fb_diff(&g_a, &g_probe);
    snprintf(buf, sizeof buf,
             "and 200 frames in, the resumed screen and the entered one are "
             "the SAME PIXELS (%d differ) — reticle, warp field, scrolling "
             "names and all",
             d);
    want(d == 0, buf);
  }
}

int main(void) {
  FohState s;
  open_credits(&s);
  assert_name_on_screen(&s);
  shoot_second_credit(&s);
  b_returns_to_options(&s);
  timer_exit();
  table_shape();
  every_string_renders();
  resume_is_an_entry();
  if (g_fails) {
    printf("CREDITS: %d assertion(s) failed\n", g_fails);
    return 1;
  }
  printf("CREDITS OK\n");
  return 0;
}
