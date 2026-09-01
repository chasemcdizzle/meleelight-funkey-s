// port/foh/foh.c — the FOH screen machine (fix_plan §M4 task 9). Flow
// graph + selection semantics per the foh.h header table (every edge
// cited there from the upstream primary source); rewritten navigation
// (d-pad edges, row cursors) per the pre-registered deltas (AGENT-LOG
// iter 88). No wall clock, no I/O — a pure (state, input) -> state step,
// so flow traces and screenshots are byte-stable by construction.
//
// "NO RNG" WAS TRUE UNTIL A7, AND THE AMENDMENT IS NARROW. The credits
// screen (MENU-SPEC §8) is a shooting gallery whose star field and name
// placement upstream draws from Math.random. That is still not the seeded
// stream: DEVIATION D38 gives the credits a generator of its own, seeded
// from a compile-time constant and living in FohState, so the step stays a
// pure function of (state, input) and two runs of the same flow still
// render the same bytes. `foh_rand` below is the ONLY generator in this
// file and `credRng` the only state it touches.
#include "foh.h"

#include "../gfx/ctl_style.h" // C30(c): the Controls screen's two cells
#include "../fdlibm/fdlibm.h" // A7: the credits star field's spawn angle

#include <string.h>

#include "foh_tbuild.h" // A45 T4: the builder engine seam (may be unlinked)

// Upstream menuMode constants (menu.js:44-47) mapped onto FOH screens.
// A45 T4 — the target builder's engine seam. DEFINED here, in the TU every
// build already links, so foh.c can READ it without depending on the engine:
// NULL means foh_tbuild.c was not linked, and render_tbuild draws the
// unavailable notice instead. foh_tbuild.h explains why it is a pointer.
const FohTbuildOps *foh_tbuild_ops = 0;

// menuCount = [4, 4, 4, 2] (menu.js:31) MOVED to foh.h as foh_menu_count()
// (ticket #27). It was a table here, a second copy in foh_render.c's range
// guard, and ticket #27 needed a third reader in foh_persist.c, which cannot
// link this TU — so it is one inline function all three ask. Nothing about
// the values changed.

// --- A7: the credits (upstream menus/credits.js; MENU-SPEC §8) -------------
//
// THE FOURTEEN CREDITS, VERBATIM (credits.js:115-132).
//
// These are other people's attribution and the bytes below are not typed by
// hand: they were lifted out of the pinned clone by a regex over
// `new ScrollingText(...)`, and port/foh/check-credits.sh re-runs that
// extraction against `$MELEELIGHT_CLONE` on every run and requires this table
// to match row for row, in order — name, role, blurb and yPos. A dropped or
// mis-spelled contributor therefore fails a check rather than shipping.
//
// Mixed case is upstream's; face 1 is uppercase-only (foh_font.c), so the
// renderer uppercases at the DRAW site (the foh_render.c:foh_upper precedent),
// leaving the authored strings here untouched.
const FohCredit foh_credits[FOH_CRED_NAMES] = {
    {"Schmoo", "Creator, Main Developer", "Made the game.", 800},
    {"Tatatat0", "Programmer", "Created the AI and credits.", 1100},
    {"bites", "Animation Assistant, Level Design",
     "Helped develop animation process & designed target stages.", 1400},
    {"shf", "Programmer, Mathematician",
     "Input conversion and environmental collision.", 1700},
    {"Nehgromancer", "Programmer", "Refactoring and networking.", 2000},
    {"BonesMalones", "Programmer", "Refactoring and optimization.", 2300},
    {"TJohnW", "Programmer", "Refactoring and code quality.", 2400},
    {"WwwWario", "Support", "Helping users troubleshoot and being a homie!",
     2700},
    {"Mrjhrock2010", "Support",
     "Helping people out and trash talking in netplay.", 3000},
    {"zircon", "Musician", "Smash Superstars (Menu Theme)", 3300},
    {"Buoy", "Musician",
     "Rush of the Rainforest (YStory Theme) & Target Blitz (Target Theme)",
     3600},
    {"Tom Mauritzon", "Musician", "Mega Helix (PStadium Theme)", 3900},
    {"Rozen", "Musician", "Kumite (Battlefield Theme)", 4200},
    {"Zack Parrish", "Musician", "Sunny Side Up (Dreamland Theme)", 4500},
};

// DEVIATION D38 — THE FOH-LOCAL RANDOM STREAM (owner-delegated, driver
// ruling; MENU-SPEC §8/§12.1).
//
// credits.js calls Math.random in three places: the star constructor and its
// respawn (:252-255, :321-323), a name's starting x (:42) and its wobble
// amplitude and direction (:49,:51). In the browser those draws come off the
// SEEDED stream — the same fact that makes the SSS RANDOM slot a registered
// refusal (foh.h) — and the sim's stream carries a 465-draw boot pin whose
// position the launched match's checksums depend on. Sharing it would move
// every golden by however many stars a player watched.
//
// So the credits get their own generator. It is mulberry32 (the same
// ALGORITHM the sim uses, no shared state and no shared header), seeded from
// a compile-time constant, and its whole state is `FohState.credRng` — which
// means the screen is still a pure function of (state, input): the same flow
// replays to the same pixels, twice, forever, and the sim's stream never sees
// a draw. What this deliberately does NOT do is invent an authored table of
// star positions: those are values upstream DRAWS, and drawing them from a
// different generator is a smaller departure than writing them down.
#define FOH_CRED_SEED 0xC0FFEE13u // arbitrary, fixed; 13 = upstream's gameMode

static double foh_rand(FohState *s) {
  // mulberry32, the sim's ml_rng.h body restated over the FOH's own state
  // (a cross-TU include from port/sim into the menu machine would make the
  // two planes look connected, which is the exact thing D38 separates).
  uint32_t z = (s->credRng += 0x6D2B79F5u);
  z = (z ^ (z >> 15)) * (z | 1u);
  z ^= z + (z ^ (z >> 7)) * (z | 61u);
  return (double)((z ^ (z >> 14)) >> 0) / 4294967296.0;
}

// D4's scale, the audio screen's note: upstream's 1200x750 canvas onto
// 240x240. Named so the conversions read as conversions.
#define CRED_CX(x) ((double)(x) / 5.0)
#define CRED_CY(y) ((double)(y) * 0.32)
// The warp field's origin. Upstream radiates from its CANVAS centre
// (600,375); ours radiates from the 240x240 centre and scales every radial
// constant by 1/5. Using D4's y scale on a radius would draw the field as an
// ellipse, and using 1/5 on the y ORIGIN would leave the bottom 90 px
// starless — the field is round in upstream's own space and is kept round.
#define CRED_STAR_CX 120.0
#define CRED_STAR_CY 120.0

// cStar's constructor / respawn (credits.js:251-258 / :320-326). `fresh`
// picks the constructor's life (a random age, so the initial hundred are
// scattered) over the respawn's (:322, which starts them at the centre).
static void cred_star_spawn(FohState *s, FohCredStar *st, bool fresh) {
  const double vel = (4.0 + foh_rand(s) * 4.0) / 5.0; // :253 / :321, /5 = D4
  const double life = fresh ? (foh_rand(s) * 100.0 + 10.0 * (vel * 5.0 - 4.0))
                            : (10.0 * (vel * 5.0 - 4.0)); // :254 / :322
  const double angle = 6.283185307179586 * foh_rand(s); // twoPi (:8), :255
  // JS Math.round: ties toward +Infinity. Every value here is positive, so
  // floor(x + 0.5) is that function on this domain.
  st->life = (int)(life + 0.5);
  st->dx = vel * fd_cos(angle);
  st->dy = vel * fd_sin(angle);
  st->x = CRED_STAR_CX + st->dx * (double)st->life; // :256 / :324
  st->y = CRED_STAR_CY + st->dy * (double)st->life;
}

// The reset block, `initc` (credits.js:112-138). NOT the stars.
static void cred_reset(FohState *s) {
  s->credScrollPos = 0; // :113
  s->credHitTimer = 0;  // :114 lastHit = [0, 0, false]
  s->credHitIdx = 0;
  s->credHitCleared = false;
  for (int i = 0; i < FOH_CRED_NAMES; i++) {
    // ScrollingText's constructor, in constructor order so the draw sequence
    // is upstream's (credits.js:41-51).
    s->credNameX[i] =
        (int)(foh_rand(s) * (double)(1200 / 2)) + 1200 / 4; // :42
    s->credNameY[i] = foh_credits[i].y0;                    // :43
    s->credNameShot[i] = false;                             // :48
    s->credNameXMax[i] = (int)(foh_rand(s) * 150.0) + 50;   // :49
    s->credNameXVal[i] = 0;                                 // :50
    // UPSTREAM QUIRK, carried: `Math.floor(Math.random() + 1)` is ALWAYS 1,
    // because Math.random() is in [0,1) (:51). The draw still happens — it
    // is a position in the stream — and the value is still 1. The wobble
    // does reverse later, in scrollY (:85-91).
    s->credNameXDir[i] = (int)(foh_rand(s) + 1.0);
    s->credNameRender[i] = false; // :52
  }
  s->credScore = 0;        // :134
  s->credShootBuf = false; // :135
  s->credCursorAngle = 0;  // :136
  for (int n = 0; n < FOH_CRED_SHOTS; n++) s->credShot[n].live = false;
  s->credInit = false; // :137
}

// D4's contract: the rect the renderer blits the name into IS the rect the
// laser hit-tests. Upstream gets that for free — its size() (:53-58) is
// `20 px * Text.length`, the exact advance of the monospaced 36 px Consolas
// it draws with. Face 1's advance is 6 px, so the port's box is
// foh_text_width of the UPPERCASED name, which is what appears on screen.
void foh_credits_name_rects(const FohState *s, FohHandRect out[FOH_CRED_NAMES]) {
  for (int i = 0; i < FOH_CRED_NAMES; i++) {
    char up[32];
    int n = 0;
    for (const char *p = foh_credits[i].name; *p && n < (int)sizeof up - 1; p++) {
      up[n++] = (*p >= 'a' && *p <= 'z') ? (char)(*p - 'a' + 'A') : *p;
    }
    up[n] = 0;
    const int w = foh_text_width(up, 1);
    int x = (int)CRED_CX(s->credNameX[i]);
    // 240 px LAYOUT ADAPTATION (not a behaviour change): upstream's x range
    // is [300,900) canvas px and its longest name is 13 * 20 = 260 px, so a
    // name always fits its 1200 px canvas. Face 1 is proportionally wider
    // (6/240 vs 20/1200), so the same fraction can run off a 240 px screen;
    // the draw x is pulled back to keep the whole name visible. The hit box
    // moves with it, because they are the same rect.
    if (x + w > RAST_W) x = RAST_W - w;
    if (x < 0) x = 0;
    out[i].x = x;
    out[i].w = w;
    // size()[1] is [yPos-23, yPos] (:55): the glyph run's top and baseline.
    // 23 canvas px * D4's 0.32 = 7.36, and face 1 is exactly 7 px tall.
    out[i].y = (int)CRED_CY(s->credNameY[i]) - 7;
    out[i].h = 7;
  }
}

bool foh_is_save_point(const FohEvent *ev) {
  if (ev->kind != FOH_EV_TRANS) return false;
  // (1) Upstream's own save point: the options screens' B-exit, which is
  //     where gameplaymenu.js:29-31 and audiomenu.js:24-25 setCookie. The
  //     Controls screen joins them by the A31 argument (foh.h) — it edits a
  //     setting through the same chokepoint and owes the same durability.
  if ((strcmp(ev->from, "options-gameplay") == 0 ||
       strcmp(ev->from, "options-audio") == 0 ||
       strcmp(ev->from, "controls-keyboard") == 0) &&
      strcmp(ev->cause, "b") == 0) {
    return true;
  }
  // (2) A49/DEVIATION D45: LEAVING THE CSS, by either exit. Upstream has no
  //     save point here because upstream cookies no character at all; this
  //     is where the persisted selection is written.
  //
  //     NOT the hover arm, deliberately. Hovering re-selects LIVE on every
  //     frame the cursor crosses a cell (css.js:222-226), so saving there
  //     would put an SD write inside a drag — the product path's persist dir
  //     is /mnt/mlfk-data, and foh_dev.c's own note records that SD writes
  //     are multi-second on this device. The exit is the one moment the
  //     selection is both settled and certain to have been reached.
  //
  //     Both causes count: `start` (into the stage select, on the way to a
  //     match) and `bhold` (back to the menu). A player who picks a
  //     character and backs out has still picked it — that is the exact
  //     gesture A43/D35 exists for, and the owner's words name it.
  if (strcmp(ev->from, "css") == 0) return true;
  return false;
}

const char *foh_screen_token(FohScreen sc) {
  switch (sc) {
    case FOH_STARTUP: return "startup";
    case FOH_TITLE: return "title";
    case FOH_MENU_TOP: return "menu-top";
    case FOH_MENU_OPTIONS: return "menu-options";
    case FOH_MENU_BATTLE: return "menu-battle";
    case FOH_MENU_CONTROLS: return "menu-controls";
    case FOH_CSS: return "css";
    case FOH_SSS: return "sss";
    case FOH_OPT_GAMEPLAY: return "options-gameplay";
    case FOH_OPT_AUDIO: return "options-audio";
    case FOH_CTRL_PAD: return "controls-controller";
    case FOH_CTRL_KEY: return "controls-keyboard";
    case FOH_CREDITS: return "credits";
    case FOH_MATCH: return "match";
    case FOH_TSS: return "target-select";
    case FOH_TMATCH: return "target-match";
    case FOH_TBUILD: return "target-builder";
    default: gfx_fatal("foh: screen token for an invalid screen");
  }
}

void foh_init(FohState *s) {
  memset(s, 0, sizeof *s);
  s->screen = FOH_STARTUP;
  // characterSelections default [0,0,0,0] (main.js:59) -> marth on every
  // port. memset already wrote the four zeros; the loop states upstream's
  // own initialiser rather than inheriting it, the way versusMode's `= 0` is
  // stated in sim_boot.c for the same reason.
  for (int k = 0; k < FOH_CSS_PORTS; k++) s->selChar[k] = 0;
  // playerType init is [-1,-1,-1,-1] (main.js:107) and addPlayer (called at
  // main.js:386 when START is pressed on the title) assigns HMN at
  // main.js:495. So
  // the CSS opens with ONE participant, i.e. NOT ready — clicking a second
  // port's type box is what raises READY TO FIGHT (css.js:1167-1181). That is
  // the whole point of MENU-SPEC item 4: the banner is the screen's feedback
  // channel, not decoration.
  // A44: ports 2/3 join ports 1's N/A rather than being pinned there — the
  // difference is that the type box can now walk them to HMN (D40).
  //
  // EVERY COLD VALUE ON THIS SCREEN NOW COMES FROM foh.h's CSS COLD-START
  // PLANE (ticket #25) rather than from a literal here. foh_persist_defaults()
  // is the second reader: the CSS machine plane is persisted now, so a fresh
  // install's record must reproduce THIS state exactly, and two copies of
  // these formulas in two TUs is the drift CONTEXT.md names as this project's
  // costliest defect class. The provenance citations moved with them.
  for (int k = 0; k < FOH_CSS_PORTS; k++) s->portType[k] = foh_css_type_home(k);
  // A49 made this four wide because every port can now be CPU; upstream's own
  // literal was always four long.
  for (int k = 0; k < FOH_CSS_PORTS; k++) {
    s->cpuDifficulty[k] = FOH_CSS_DIFF_HOME;
  }
  s->cssCarry = -1;    // whichTokenGrabbed (css.js:68)
  s->cssCpuCarry = -1; // whichCpuGrabbed (css.js:75)
  // FOH_CSS_PORTS, not 2: `cpuSlider` is a four-element literal upstream
  // (css.js:72) and A49 gave ports 2/3 a CPU type to draw a knob for.
  for (int k = 0; k < FOH_CSS_PORTS; k++) {
    s->cssSliderX[k] = foh_css_slider_home(k);
  }
  s->cssHandX = FOH_CSS_HAND_HOME_X;
  s->cssHandY = FOH_CSS_HAND_HOME_Y;
  // The TSS hand (D29) is re-homed on every entry to that screen, so this is
  // only the cold value — but memset's (0,0) would leave a boot-time state
  // whose hand hovers nothing while tssCursor reads 0, and nothing in the FOH
  // should ever be internally inconsistent just because it is unreachable.
  s->tssHandX = FOH_TSS_HOME_X;
  s->tssHandY = FOH_TSS_HOME_Y;
  // gameSettings defaults (settings.js:44-56): all zero — memset did it,
  // EXCEPT phantomThreshold, whose authored default is 0.01 (settings.js:50)
  // and which is on the checksum surface. Zeroing it is the exact qjs
  // getCookie defect (CLAUDE.md M0 task 6), so it is written explicitly.
  s->phantomThreshold = 0.01;
  // masterVolume = [0.5, 0.3] (audiomenu.js:13) — sounds, music.
  s->masterVolume[0] = 0.5;
  s->masterVolume[1] = 0.3;
  // --- credits module load (menus/credits.js top level) -------------------
  // `initc = true` (:11), so the FIRST tick on the screen runs the reset
  // block; the star field is built ONCE here, exactly where upstream builds
  // it (:262-265, module load), which is why it keeps drifting between
  // visits. The RNG (DEVIATION D38) is seeded before the first draw.
  s->credRng = FOH_CRED_SEED;
  s->credInit = true;
  for (int n = 0; n < FOH_CRED_STARS; n++) {
    cred_star_spawn(s, &s->credStar[n], true);
  }
  // cPlayerXPos/cPlayerYPos = canvas centre (:23-24). D12 makes the reticle
  // relative, so this is where it STARTS; step_menu re-homes it on every
  // entry, because upstream's absolute map re-centres it whenever the stick
  // is neutral and losing that would be a behaviour change D12 never asked
  // for.
  //
  // TICKET #27 MADE THIS LINE LOAD-BEARING BEYOND THE COLD BOOT. Credits now
  // resumes into itself, and a resume never runs the entering transition —
  // so THIS is what places the reticle on a resumed credits screen. It is
  // the same macro the entry writes (foh.h's FOH_CRED_HOME_X/Y), which is
  // what makes "the resume arrives where an entry would" true by
  // construction instead of by two copies of a literal agreeing.
  s->credX = FOH_CRED_HOME_X;
  s->credY = FOH_CRED_HOME_Y;
  // targetRecords fresh state is -1, NOT 0 (targetplay.js:40) — 0 would
  // read as a valid 0-second record (task 13).
  // BOTH halves (D59). foh_state_record reads 0.0 as a valid ZERO-SECOND
  // record, so leaving the custom plane memset-zero here would make a fresh
  // FohState claim a perfect time on every custom slot. The product masks it
  // via foh_persist_apply, which is exactly why it went unnoticed — this is
  // the PUBLIC initialiser and it has to be right on its own.
  for (int c = 0; c < 5; c++) {
    for (int t = 0; t < 10; t++) s->targetRecordsCustom[c][t] = -1.0;
    for (int t = 0; t < 10; t++) s->targetRecords[c][t] = -1.0;
  }
  // LOOK plane (A1 restyle Phase 0; foh.h). menuColours / menuCurColour
  // literals are menu.js:34-35 — presentation constants of a rewritten,
  // non-checksummed surface, not engine data.
  s->menuHue = 238.0;
  s->menuColours[0] = 238.0;
  s->menuColours[1] = 358.0;
  s->menuColours[2] = 117.0;
  s->menuColours[3] = 55.0;
}

static void ev_push(FohState *s, FohEvent e) {
  if (s->nev >= FOH_EV_CAP) gfx_fatal("foh: event buffer overflow");
  s->ev[s->nev++] = e;
}

// Menu SFX token (M4 task 10; foh.h note). Upstream mapping, cited per
// emission site below: menuSelect = cursor/value steps + option
// toggles (menu.js:236, css.js:226-class, stageselect.js:64/73,
// gameplaymenu.js:38/152); menuForward = confirm transitions + title
// START + css->sss + sss launch (menu.js:70, main.js:388, css.js:448,
// stageselect.js:81); menuBack = every B back incl. bhold
// (menu.js:170-190, css.js:189, stageselect.js:78, gameplaymenu.js:26);
// deny = refused entries (keyboardmenu.js:170 refusal class).
// The iter-93 exclusion of the CSS announcer names is RETIRED: the token
// drop exists now, so css.js:233/248/263/278/293's per-character
// announcer plays at the drop (kCssAnnouncer below; all five are real
// SND1 Howl names, sfx.js:15/305/380/434/572).
static void snd_push(FohState *s, const char *name) {
  if (s->nsnd >= FOH_EV_CAP) gfx_fatal("foh: sound buffer overflow");
  s->snd[s->nsnd++] = name;
}

// The same push, reachable from foh_tbuild.c (A45 T4). It is a WRAPPER, not
// a second implementation: the overflow guard has exactly one body, so the
// builder cannot overflow the queue by a route this file does not police.
void foh_snd_push(FohState *s, const char *name) { snd_push(s, name); }

static void ev_trans(FohState *s, FohScreen from, FohScreen to,
                     const char *cause) {
  FohEvent e;
  memset(&e, 0, sizeof e);
  e.kind = FOH_EV_TRANS;
  e.from = foh_screen_token(from);
  e.to = foh_screen_token(to);
  e.cause = cause;
  ev_push(s, e);
  s->screen = to;
}

static void ev_sel(FohState *s, const char *field, int val) {
  FohEvent e;
  memset(&e, 0, sizeof e);
  e.kind = FOH_EV_SEL;
  e.field = field;
  e.val = val;
  ev_push(s, e);
}

static void ev_refused(FohState *s, const char *entry) {
  FohEvent e;
  memset(&e, 0, sizeof e);
  e.kind = FOH_EV_SEL;
  e.field = "refused";
  e.sval = entry;
  ev_push(s, e);
}

static void ev_launch(FohState *s) {
  FohEvent e;
  memset(&e, 0, sizeof e);
  e.kind = FOH_EV_LAUNCH;
  ev_push(s, e);
}

// Clamp helper for the rewritten value rows (pre-registered: CLAMP at
// the ends, no wrap).
static int clampi(int v, int lo, int hi) {
  return v < lo ? lo : (v > hi ? hi : v);
}

// --- per-screen steps -------------------------------------------------------

static void step_menu(FohState *s, const PlatformInput *in,
                      const PlatformInput *pv) {
  const FohScreen sc = s->screen;
  const int count = foh_menu_count(sc);
  const bool aE = in->a && !pv->a;
  const bool bE = in->b && !pv->b;
  const bool uE = in->up && !pv->up;
  const bool dE = in->down && !pv->down;
  if (aE) {
    switch (sc) {
      case FOH_MENU_TOP:
        // menuText[0] = ["VS. Melee","Target Test","Target Builder",
        // "Options"] (menu.js:19-24); routing menu.js:66-100.
        if (s->menuSelected == 0) {
          s->menuSelected = 0; // LOCALVS (menu.js:74)
          snd_push(s, "menuForward"); // menu.js:70
#if FOH_NETPLAY
          ev_trans(s, sc, FOH_MENU_BATTLE, "a");
#else
          // C5 (owner ruling; foh.h FOH_NETPLAY): the battle page holds
          // nothing but Local VS and three netplay rows, so `VS. Melee`
          // runs the Local VS action itself — menu.js:105's
          // changeGamemode(2) + positionPlayersInCSS, the exact arm the
          // hidden page would have dispatched. menuSelected stays 0, so the
          // CSS's B-hold lands back on `VS. Melee`.
          ev_trans(s, sc, FOH_CSS, "a");
#endif
        } else if (s->menuSelected == 1) {
          // TARGETTEST (menu.js:77-84): setTargetPlayer(0) is implicit
          // (slot 0 is the only port); targetPointerPos reset == the
          // rewritten cursor reset; the device app switches the music
          // to the targettest track at the TLAUNCH seam (REGISTERED
          // rewrite delta, AGENT-LOG iter 99 — an SD ring prefill
          // inside the paced FOH loop risks the skips==0 gate;
          // upstream switches here, menu.js:82-83).
          s->tssCursor = 0;
          // D29: the hand IS the cursor now, so targetPointerPos's reset is a
          // position, not an index. Home is slot 0's centre, so the screen
          // opens hovering tstage 0 exactly as `tssCursor = 0` opened it.
          s->tssHandX = FOH_TSS_HOME_X;
          s->tssHandY = FOH_TSS_HOME_Y;
          // ...and RE-READ THE CARD (ticket #26). `tssPage` is persisted
          // now, so this entry can land on the CUSTOM grid, and the cache
          // behind that grid describes ten files on an SD card the player
          // may have edited on a PC since the last visit. Before this line
          // the only refresh was the page-flip arm in step_tss — which is
          // exactly the arrival a restored page SKIPS, so every custom slot
          // would have drawn dead with no reason under it.
          //
          // Same re-derivation the RESUME HOOK performs
          // (FOH_RESUME_HOOK_TSS_SLOTS, foh_persist.h): entry and resume are
          // the two ways to arrive here without flipping, and they call the
          // same function rather than growing a second answer.
          //
          // It is I/O in a file whose header promises none, and that promise
          // was already qualified — the page-flip arm has opened these ten
          // files since A45 T3. The seam is foh_tbuild_ops (NULL when the
          // builder TU is unlinked, where this becomes a memset), and it
          // fires on entry and flip only, never per frame.
          foh_tss_refresh_slots(s);
          snd_push(s, "menuForward"); // menu.js:70
          ev_trans(s, sc, FOH_TSS, "a"); // changeGamemode(7), menu.js:84
        } else if (s->menuSelected == 2) {
          // A45 T4 — TARGETBUILDER (menu.js:87-90): `setEditingStage(-1);
          // setTargetBuilder(i); changeGamemode(4)`. This row REFUSED until
          // now (`deny` + ev_refused "targetbuilder"), which is the defect
          // the owner filed as *"nothing happened"*. It opens the editor.
          //
          // slot -1 == upstream's setEditingStage(-1): a document with no
          // slot yet. Note that upstream does NOT reset stageTemp here, so a
          // second visit keeps what you were editing — module lifetime,
          // carried verbatim (foh_tbuild.c's tb_enter).
          //
          // menuSelected is deliberately left alone, so the builder's exit
          // lands back on the TARGET BUILDER row, like every other
          // changeGamemode leave in this switch.
          if (foh_tbuild_ops) foh_tbuild_ops->enter(s, -1);
          snd_push(s, "menuForward"); // menu.js:70
          ev_trans(s, sc, FOH_TBUILD, "a"); // changeGamemode(4), :90
        } else {
          s->menuSelected = 0; // AUDIOOPTIONS (menu.js:96)
          snd_push(s, "menuForward"); // menu.js:70
          // menu.js:67/:233/:236 — the LOCAL boolean `menuMove` (it shadows
          // the function name) is set by this arm, and :236 plays a SECOND
          // sound in the SAME tick. Only menuMODE changes set it: the
          // changeGamemode leaves (and VSMODE->MPMENU, :73-75) do NOT.
          snd_push(s, "menuSelect"); // menu.js:236 (menuMove at :97)
          ev_trans(s, sc, FOH_MENU_OPTIONS, "a");
        }
        break;
      case FOH_MENU_BATTLE:
        // ["Local VS","Spectate","P2P","Server"]; only Local VS is
        // in-scope (multiplayer excluded; P2P dead upstream).
        if (s->menuSelected == 0) {
          snd_push(s, "menuForward"); // menu.js:70
          ev_trans(s, sc, FOH_CSS, "a"); // menu.js:105
        } else if (s->menuSelected == 1) {
          snd_push(s, "deny");
          ev_refused(s, "spectate");
        } else if (s->menuSelected == 2) {
          snd_push(s, "deny");
          ev_refused(s, "p2p");
        } else {
          snd_push(s, "deny");
          ev_refused(s, "server");
        }
        break;
      case FOH_MENU_OPTIONS:
        // ["Audio","Gameplay","Keyboard Controls","Credits"] upstream; row 2
        // is drawn "CONTROLS" here (DEVIATION D25 — it opens a chooser, so
        // upstream named it after one of its own destinations). Row INDEXES
        // and actions are upstream's, untouched.
        if (s->menuSelected == 0) {
          // audioMenuSelected (audiomenu.js:15) is MODULE state: it is not
          // reset on entry, so a second visit opens on the row you left.
          // FohState has exactly that lifetime — nothing to do here.
          snd_push(s, "menuForward"); // menu.js:70
          ev_trans(s, sc, FOH_OPT_AUDIO, "a"); // changeGamemode(10), :130
        } else if (s->menuSelected == 1) {
          // menuIndex (gameplaymenu.js:10) is MODULE state too — upstream
          // never resets it on entry either (measured), so the cursor is
          // where you left it. The old unconditional reset here was an
          // unregistered deviation; FohState already has module lifetime.
          snd_push(s, "menuForward"); // menu.js:70
          ev_trans(s, sc, FOH_OPT_GAMEPLAY, "a"); // menu.js:135
        } else if (s->menuSelected == 2) {
#if FOH_CTL_CHOOSER
          s->menuSelected = 0;
          snd_push(s, "menuForward"); // menu.js:70
          // menu.js:67/:233/:236 — the LOCAL boolean `menuMove` (it shadows
          // the function name) is set by this arm, and :236 plays a SECOND
          // sound in the SAME tick. Only menuMODE changes set it: the
          // changeGamemode leaves (and VSMODE->MPMENU, :73-75) do NOT.
          snd_push(s, "menuSelect"); // menu.js:236 (menuMove at :141)
          ev_trans(s, sc, FOH_MENU_CONTROLS, "a"); // menu.js:138-141
#else
          // DEVIATION D27 (MENU-SPEC §12.1, owner ruling 2026-08-23; the
          // switch and its whole rationale live at foh.h's FOH_CTL_CHOOSER).
          // The chooser is collapsed, so this row IS the changeGamemode(12)
          // leave that upstream's chooser row 1 was (menu.js:159-161) — and
          // it therefore takes that arm's SOUND SHAPE, not this arm's:
          // upstream's second `menuSelect` is emitted by the local `menuMove`
          // boolean, which ONLY a menuMODE change sets (menu.js:141 vs :159).
          // No menuMode change happens here any more, so it is ONE
          // menuForward — the same single sound every other changeGamemode
          // leave in this switch emits, and foh_snd_witness.c asserts it.
          // menuSelected is deliberately NOT reset: with no chooser to open a
          // cursor on, leaving it alone is what makes step_ctrl's B land back
          // on the CONTROLS row that opened the screen (step_ctrl says so in
          // its own note).
          snd_push(s, "menuForward"); // menu.js:70
          ev_trans(s, sc, FOH_CTRL_KEY, "a"); // changeGamemode(12), :159-161
#endif
        } else {
          // A7 (MENU-SPEC §8). Upstream: `setCreditsPlayer(i);
          // changeGamemode(13)` (menu.js:145-149), inside the ONE
          // menuForward at :70 — no `menuMove = true`, so it is a SINGLE
          // sound, like every other changeGamemode leave in this switch.
          // menuSelected is NOT touched, which is what puts the cursor back
          // on the CREDITS row when the screen exits (credits.js:226-246
          // changes only the gameMode).
          //
          // creditsPlayer has no port here — the FOH has one input device
          // (foh.h's ONE-HAND note), so the polled port is always 0.
          snd_push(s, "menuForward"); // menu.js:70
          // D12's reticle is relative, so entering has to put it somewhere.
          // Upstream's absolute map parks it dead centre whenever the stick
          // is neutral (credits.js:169-173), and that is the observable
          // being preserved. The tssHandX re-home above is the precedent.
          // Ticket #27: the SAME macro foh_init writes, so a resumed credits
          // screen (which skips this transition) is placed identically.
          s->credX = FOH_CRED_HOME_X;
          s->credY = FOH_CRED_HOME_Y;
          ev_trans(s, sc, FOH_CREDITS, "a"); // changeGamemode(13), :147
        }
        break;
      case FOH_MENU_CONTROLS:
        // DEVIATION D25 (MENU-SPEC §12.1, owner-requested 2026-08-23) — THE
        // ROWS ARE SWAPPED. Upstream is ["Controller","Keyboard"]
        // (menu.js:22-23) with Controller first; we draw ["HANDHELD",
        // "CONTROLLER"] (foh_render.c kMenuText[3]) because the FunKey-S has
        // no controller path today, so the reachable destination goes first.
        // THIS TERNARY IS THE OTHER HALF OF THAT RENAME — the screen is
        // INDEX-selected, so moving the labels without moving the routing
        // sends row 0 to the wrong destination. The two move together, and
        // port/foh/check-controls-labels.sh binds them: it asserts the
        // rendered ROW LABEL and the rendered HEADER of the screen that row
        // opens, and its T2 reverts exactly this line and must fail.
        // The upstream gameMode identities are UNCHANGED: FOH_CTRL_KEY is
        // still 12 and FOH_CTRL_PAD still 14, and so are the screen tokens.
        // Upstream row 1 is reached by `else`, so ANY non-zero index lands
        // there (menu.js:159) — that shape is mirrored below, on the row that
        // is now second.
        snd_push(s, "menuForward"); // menu.js:70 (before any dispatch)
        ev_trans(s, sc, s->menuSelected == 0 ? FOH_CTRL_KEY : FOH_CTRL_PAD,
                 "a"); // :159-161 changeGamemode(12) / :155-157 (14)
        break;
      default: gfx_fatal("foh: step_menu on a non-menu screen");
    }
    return;
  }
  if (bE) {
    // menu.js:164-190 verbatim back edges (cursor values included).
    if (sc == FOH_MENU_CONTROLS) {
      s->menuSelected = 0; // AUDIOOPTIONS
      snd_push(s, "menuBack"); // menu.js:170-190
      // menu.js:169/:174/:179 set the local `menuMove` boolean, so :236
      // plays a SECOND sound in the same tick (all three B backs).
      snd_push(s, "menuSelect"); // menu.js:236
      ev_trans(s, sc, FOH_MENU_OPTIONS, "b");
    } else if (sc == FOH_MENU_OPTIONS) {
      s->menuSelected = 3; // OPTIONS
      snd_push(s, "menuBack"); // menu.js:170-190
      snd_push(s, "menuSelect"); // menu.js:236 (menuMove at :174)
      ev_trans(s, sc, FOH_MENU_TOP, "b");
    } else if (sc == FOH_MENU_BATTLE) {
      s->menuSelected = 0; // VSMODE
      snd_push(s, "menuBack"); // menu.js:170-190
      snd_push(s, "menuSelect"); // menu.js:236 (menuMove at :179)
      ev_trans(s, sc, FOH_MENU_TOP, "b");
    }
    // top level: B does nothing (no upstream arm)
    return;
  }
  // cursor with wrap (menu.js:192-242 wraps via menuCount)
  // upstream's arms are ONE else-if chain (menu.js:69,164,192,205 —
  // A->B->up->down; the A arm at :69 precedes the B arm at :164),
  // so a simultaneous up+down edge runs UP ONLY. Independent ifs cancelled
  // the cursor and emitted TWO menuSelects in one tick.
  if (uE) {
    s->menuSelected = (s->menuSelected + count - 1) % count;
    snd_push(s, "menuSelect"); // menu.js:236
  } else if (dE) {
    s->menuSelected = (s->menuSelected + 1) % count;
    snd_push(s, "menuSelect"); // menu.js:236
  }
}

// --- CSS: the free hand cursor + the token model (MENU-SPEC §2) -------------
// Structure below follows cssControls (css.js:182-461) arm for arm: B-hold,
// hand motion + clamp, then the three-way branch on (roster band / dragging a
// CPU knob / everything else), then the knob-grab loop and the launch check
// that upstream runs OUTSIDE that branch, then the draw pass's readyToFight.
//
// The announcer that plays when a token is dropped on a cell
// (css.js:233/248/263/278/293). Roster order marth-puff-fox-falco-falcon.
static const char *const kCssAnnouncer[5] = {"marth", "jigglypuff", "fox",
                                             "falco", "falcon"};

// The per-port `S` trace field names. A44 widened both planes to four ports
// (foh.h), and these tables are what keeps the widening honest in the trace:
// the field a write reports is indexed by the PORT that was written, so a
// write to selChar[2] can only ever announce itself as `p3char`. The old
// `k == 0 ? "p1char" : "p2char"` conditional had no room for a third answer
// and would have silently reported port 2's write as port 1's — which is
// CONTEXT.md's "Port is never an index into something else" one layer up.
static const char *const kCssCharField[FOH_CSS_PORTS] = {"p1char", "p2char",
                                                         "p3char", "p4char"};
static const char *const kCssTypeField[FOH_CSS_PORTS] = {"p1type", "p2type",
                                                         "p3type", "p4type"};
// Same rule, same reason, for the CPU-level plane A49 widened to four. The
// first two names are the ones every frozen trace already carries: port 0's
// is `p1difficulty` and port 1's is the bare `difficulty` (foh.h). Ports 2/3
// APPEND rather than renumber, exactly as A44's LAUNCH tokens did.
static const char *const kCssDiffField[FOH_CSS_PORTS] = {
    "p1difficulty", "difficulty", "p3difficulty", "p4difficulty"};

int foh_css_port_type(const FohState *s, int k) {
  // A44/D40: every port owns a real type field now. Upstream draws all four
  // type tabs unconditionally (css.js:860-879) and gates only the per-port
  // PREVIEW on playerType > -1 (css.js:881-882), which is exactly what
  // css_panel does — so this accessor is now just the plane.
  return s->portType[k];
}

int foh_css_port_diff(const FohState *s, int k) {
  // A49: the CPU-level plane is FOUR wide, upstream's own width
  // (cpuDifficulty = [3,3,3,3], main.js:109), because D40(b) is retired and
  // every port can be CPU. The old two-wide accessor answered a k >= 2 read
  // with PORT 1's level — harmless only while nothing could draw or launch
  // it, and a lie the moment either could.
  return s->cpuDifficulty[k];
}

void foh_css_token_pos(const FohState *s, int k, double *x, double *y) {
  if (s->cssCarry == k) { // the token rides the hand (css.js:218)
    *x = s->cssHandX;
    *y = s->cssHandY;
    return;
  }
  const int c = s->cssChar[k]; // the TOKEN plane (css.js:66), not setCS's
  //
  // ONE RULE FOR EVERY RESTING TOKEN: it is drawn on the cell of the
  // character its PORT chose. Upstream has three rest formulas and they
  // disagree with each other and with the selection; both DEVIATIONS this
  // screen has shipped for were one of those disagreements, and A49 is the
  // third and last.
  //
  //   slot 0 — the A-drop (css.js:288 family). Already the selection.
  //   slot 2 — endGame's snap (main.js:1381-1384 -> css.js:154-156,
  //            `tokenPos[index] = charIconPos[index]`). DEVIATION D21 (A29,
  //            owner-reported P0): upstream indexes charIconPos — a
  //            per-CHARACTER array — with the PORT number, so after any
  //            match the tokens sit on marth and puff forever whatever the
  //            ports picked. Demonstrably an upstream TYPO, not a design:
  //            setChosenChar calls `setTokenPosSnapToChar(index,
  //            charSelected)` with two arguments (css.js:146) while the
  //            callee declares one and drops it (css.js:154). We honour the
  //            argument the caller passes.
  //   slot 1 — the leave-band drop (css.js:337). DEVIATION D46 (fix_plan
  //            A49, owner-reported P1): upstream's second formula has a
  //            different base AND a different pitch, which MEASURED here is
  //            a whole cell to the right of the character it just selected
  //            (foh.h, FOH_CSS_TOKEN_LB_DX). The owner: *"whenever the pin
  //            is let go of (going off) it should go back to the character
  //            you had selected"*. So it homes on the selection too.
  //
  // The rule is written ONCE rather than three times agreeing, because the
  // failure mode this screen keeps having is one of the three drifting.
  // `cssTokenRest[k]` still RECORDS which path the token came to rest by —
  // foh_dev.c writes the endGame snap, step_css writes the other two, and
  // the D21/D35/D46 witnesses read it to prove they reproduced the
  // precondition they claim to. Nothing computes a POSITION from it.
  //
  // Nothing on the LAUNCH plane moves under any of this: selChar/cssChar are
  // untouched by where a token is drawn. This is the DRAW and hit-test
  // plane, and that is the whole of both deviations.
  //
  // No clamp: every base is `cell_x(c) + DX`, whose widest case is cell 4's
  // right column at 226 < 240 (foh.h's D41 derivation). D41's clamp existed
  // for the leave-band formula alone and went with it — a branch that
  // provably cannot fire is not a guard.
  const double base = (double)(foh_css_cell_x(c) + FOH_CSS_TOKEN_DX);
  // D41: upstream's own 2x2 — port k sits at column k % 2, row k / 2, so
  // ports 0/1 keep the top row they have always had and 2/3 open the second.
  *x = base + (double)(FOH_CSS_TOKEN_PITCH * (k % 2));
  *y = (double)(FOH_CSS_TOKEN_Y + FOH_CSS_TOKEN_ROW_PITCH * (k / 2));
}

int foh_css_hand_type(const FohState *s) { return s->cssHandType; }

double foh_css_knob_x(const FohState *s, int k) { return s->cssSliderX[k]; }
double foh_css_knob_y(void) {
  return (double)(FOH_CSS_PANEL_Y + FOH_CSS_RAIL_Y);
}
// The rail's left end for port k, and the level the slider position reads as.
static double css_rail_x0(int k) {
  return (double)(foh_css_panel_x(k) + FOH_CSS_RAIL_X0);
}
static int css_level_at(int k, double x) {
  // css.js:325-327 with upstream's 166 px travel replaced by ours: the track
  // parameter t in [0,1] maps round(t*3)+1, so the domain is exactly 1..4.
  return (int)((x - css_rail_x0(k)) * 3.0 / (double)FOH_CSS_RAIL_LEN + 0.5) + 1;
}

// The five character cells as a hit table (foh.h). Contiguous upstream; here
// the cells are drawn with a 2 px gutter, and D4 forbids a hit region where
// nothing is drawn, so the gutter is genuinely no cell — which is
// foh_hand_hit's strict rule, and why that rule lives in the shared unit.
//
// A25(c): this REPLACES `css_cell_at(x)` and the y guard that used to wrap its
// only call site. The two together tested exactly this rect (x strictly inside
// the cell, y strictly inside FOH_CSS_CELL_Y + FOH_CSS_CELL_H), and every cell
// shares the same y, so folding them into one point-in-rect sweep is an
// identity — proven bit for bit by check-hand.sh's differential, not asserted.
void foh_css_cells(FohHandRect out[FOH_CSS_CHARS]) {
  // Adding a character means moving FOH_CSS_CHARS, and every walker of the
  // roster — this table and its two hit tests — moves with it because they
  // all read the constant.
  //
  // The parameter's `[FOH_CSS_CHARS]` is DOCUMENTATION, not enforcement: C
  // adjusts an array parameter to a pointer, so a caller passing `cells[5]`
  // still compiles. Said plainly because the comment here used to claim the
  // build would catch it, which is the kind of false comment that costs a
  // later reader an hour.
  for (int c = 0; c < FOH_CSS_CHARS; c++) {
    out[c].x = foh_css_cell_x(c);
    out[c].y = FOH_CSS_CELL_Y;
    out[c].w = FOH_CSS_CELL_W;
    out[c].h = FOH_CSS_CELL_H;
  }
}

// The eleven target-select slots (foh.h): upstream's 2x5 authored grid in its
// own col = floor(j/5) / row = j%5 order, then the refusing "+ ADD CODE" slot.
// render_tss draws from this same table.
void foh_tss_slots(FohHandRect out[FOH_TSS_SLOTS]) {
  for (int k = 0; k < 10; k++) {
    out[k].x = FOH_TSS_SLOT_X0 + (k / 5) * FOH_TSS_SLOT_COL_PITCH;
    out[k].y = FOH_TSS_SLOT_Y0 + (k % 5) * FOH_TSS_SLOT_ROW_PITCH;
    out[k].w = FOH_TSS_SLOT_W;
    out[k].h = FOH_TSS_SLOT_H;
  }
  out[10].x = FOH_TSS_ADD_X;
  out[10].y = FOH_TSS_ADD_Y;
  out[10].w = FOH_TSS_ADD_W;
  out[10].h = FOH_TSS_ADD_H;
}

static int *css_char_of(FohState *s, int k) {
  return &s->cssChar[k];
}

// readyToFight, css.js:1167-1181 verbatim in shape — including the fact that
// `readyPlayers >= 2` is re-decided on every participating port, so the last
// one seen wins, and that a held token breaks out immediately.
// occupiedToken[k] is `cssCarry == k`: with one hand nothing else can hold it.
static bool css_ready(const FohState *s) {
  int readyPlayers = 0;
  bool ready = false;
  for (int k = 0; k < 4; k++) {
    if (foh_css_port_type(s, k) > -1) {
      readyPlayers++;
      ready = (readyPlayers >= 2);
      if (s->cssCarry == k) {
        ready = false;
        break;
      }
    }
  }
  return ready;
}

// The B-hold exit edge (css.js:186-194). menuMode is untouched upstream, so
// we return to menu-battle with its cursor where the VS entry left it.
// The sound is emitted at the counter site, not here (see step_css).
static void css_back(FohState *s) {
  // DEVIATION D35 (A43, owner-reported P1). Leaving the CSS RELEASES every
  // token and re-homes it on the character its port actually chose — the
  // A-drop rest slot, `foh_css_cell_x(cssChar[k])`. It is D21's rule (a token
  // is re-homed from the SELECTION plane, never from where it physically
  // sits) applied to the second rest path.
  //
  // MEASURED, and the reported symptom is a CHAIN, not a redraw. Upstream's
  // grab is module state that survives a gamemode change: `tokenGrabbed[]` /
  // `whichTokenGrabbed[]` (css.js:67-68) are cleared by exactly two arms —
  // the A-drop (css.js:228-232 and siblings) and the leave-band drop
  // (css.js:341-347) — and the back-out at css.js:186-194 calls
  // `changeGamemode(1)` without touching either. `changeGamemode` case 2 is
  // `drawCSSInit()` alone (main.js:571) and menu.js:105-106's re-entry adds
  // only `positionPlayersInCSS()`, which positions the four SIM players
  // (main.js:528-535) and no token. So upstream re-enters its CSS with the
  // token still glued to the hand, and this port carried that verbatim.
  //
  // Here that chain has somewhere to go that upstream's has not. One B press
  // in the roster band BOTH grabs your token (css.js:209-215) and arms the
  // 30-frame back counter — upstream's own deliberate overlap, MENU-SPEC
  // §2.11 — so "press B to go back" leaves the CSS carrying. On re-entry the
  // token still rides the hand, and the hover arm re-selects LIVE from the
  // hand's position (css.js:222-226, the one site that writes both planes).
  // D22 puts the BACK wedge at x > 184, y < 26 — directly above roster cell 4
  // — so the next walk to BACK drags the carried token across falcon and
  // COMMITS it. Both planes then genuinely read falcon, which is the owner's
  // "you come back in and you are selected as falcon", and why it is always
  // falcon: cell 4 is the last cell, the one the BACK wedge sits over.
  // Upstream's wedge is an instant A-click (D22) on a 1200 px canvas whose
  // roster ends 300 px short of the hand's clamp, so the same leak never
  // drags a token over the last cell there.
  //
  // The SELECTION PLANE IS NOT TOUCHED by this arm, deliberately: the hover
  // already committed a choice and discarding it here would trade a wrong
  // display for a silently dropped pick. Only where the tokens are DRAWN and
  // hit-tested moves — the same scope D21 has. The rest slot is written for
  // both ports, not just the carried one, so re-entry always shows the
  // selection rather than whichever of Q1's two formulas last applied.
  if (s->cssCarry >= 0) {
    s->cssCarry = -1; // D35: release the grab that upstream leaks across
    ev_sel(s, "carry", -1);
  }
  // D35: re-home EVERY port on its own cssChar. The bound is FOH_CSS_PORTS
  // and not 2 because A44 gave ports 2/3 tokens: leaving two of the four
  // parked on whichever of Q1's formulas last applied is the same defect
  // D35 exists to close, just two ports further right.
  for (int k = 0; k < FOH_CSS_PORTS; k++) s->cssTokenRest[k] = 0;
  s->menuSelected = 0; // LOCALVS
#if FOH_NETPLAY
  ev_trans(s, FOH_CSS, FOH_MENU_BATTLE, "bhold");
#else
  // C5: the battle page is hidden, so `menuMode untouched` resolves to the
  // page the player actually came from — menu-top, cursor on VS. Melee
  // (row 0, the same index LOCALVS has on the hidden page).
  ev_trans(s, FOH_CSS, FOH_MENU_TOP, "bhold");
#endif
}

static void step_css(FohState *s, const PlatformInput *in,
                     const PlatformInput *pv) {
  const bool aE = in->a && !pv->a;
  const bool bE = in->b && !pv->b;
  bool allowRegrab = true; // css.js:183

  // B held 30 consecutive frames -> back to the menu (css.js:186-194;
  // menuMode is untouched upstream, so we return to menu-battle). Note the
  // deliberate overlap with the B-grab below: one B press both starts this
  // counter and retrieves your token (MENU-SPEC §2.11).
  // The counter is NOT reset: upstream's `bHold[i] == 30` equality is what
  // makes this fire exactly once per hold (css.js:188), and resetting would
  // re-arm it 30 frames later while B is still down.
  //
  // The transition is PENDING, not immediate. Upstream calls changeGamemode(1)
  // here and then runs the WHOLE remainder of cssControls (css.js:186-461):
  // the same frame still integrates the hand, can grab or drop a token, can
  // click a type box, and — if START is also down — calls changeGamemode(6),
  // which wins because it is last. Returning here instead would drop all of
  // that, so the pending edge is emitted at the very end of the step and any
  // transition taken in between simply supersedes it.
  //
  // DEVIATION D22 (A23, owner-requested 2026-08-22) — the SECOND input path
  // into this ONE counter. Upstream's BACK wedge is an INSTANT click: css.js:
  // 358-363 tests `handPos.y < 160 && handPos.x > 920` on the A rising edge
  // and calls changeGamemode(1) outright, with no hold and no bar. The owner
  // asked for the arcade behaviour instead — "I want it to behave like how
  // melee does ... holding the cursor OR holding the back button starts
  // progressing a little red bar that fills below the back button, and when
  // it fills it actually backs out" (playthrough #3, 2026-08-22). Those words
  // are the ratification; MENU-SPEC's D22 row carries the argument.
  //
  // The deviation is NARROW and it is only the TRIGGER. The bar itself is
  // upstream's, verbatim in shape and rate (css.js:735-746, drawn in
  // foh_render.c's css_header), and the 30-frame counter, its `== 30`
  // equality and its menuBack are upstream's too. What deviates is that the
  // hand ARMS the counter rather than firing changeGamemode(1) instantly.
  // One counter, two input paths — never a second timer, so the bar reports
  // both paths without knowing which is driving it.
  //
  // The hand position read here is LAST frame's — the counter runs at
  // upstream's site (css.js:186), which is BEFORE the hand integrates
  // (css.js:195). That is deliberate: last frame's position is the one the
  // draw pass rendered, i.e. the wedge the player can SEE the cursor on. It
  // is the same last-frame-value idiom `cssReady` uses at the launch arm
  // below. Bounds are strict, as upstream's are, and no other CSS widget can
  // be reached with the hand this high: the panel tabs begin at
  // FOH_CSS_PANEL_Y (96) and the roster band at FOH_CSS_BAND_TOP (26), so
  // holding A here cannot also grab, drop or toggle anything.
  const bool onBack = s->cssHandY < (double)FOH_CSS_BAND_TOP &&
                      s->cssHandX > (double)FOH_CSS_BACK_X0;
  bool pendingBack = false;
  if (in->b || (onBack && in->a)) {
    s->bHold++;
    if (s->bHold == 30) {
      // The SOUND fires HERE, at upstream's site (css.js:189), not with the
      // deferred transition: upstream plays menuBack before the rest of
      // cssControls runs, so on a B+START frame the pair is menuBack then
      // menuForward, and on a B+A frame the action's sound follows menuBack.
      // Only the structural edge is deferred.
      snd_push(s, "menuBack");
      pendingBack = true;
    }
  } else {
    s->bHold = 0;
  }

  // Hand motion, unconditional every frame (css.js:195-206). The body moved to
  // foh_hand_step (A25c) so target-select can run the same one; the deviations
  // it carries (D1 full-deflection d-pad, D3 the rescaled step) and the clamp
  // to the LOGICAL canvas are documented there, unchanged.
  foh_hand_step(&s->cssHandX, &s->cssHandY, in->up, in->down, in->left,
                in->right, (double)RAST_W, (double)RAST_H);

  if (s->cssHandY < (double)FOH_CSS_BAND_BOT &&
      s->cssHandY > (double)FOH_CSS_BAND_TOP) {
    // --- roster band (css.js:207-315) ---------------------------------------
    s->cssHandType = 1; // handOpen (css.js:208)
    // (a) B on your OWN token, from anywhere in the band: the token teleports
    // to the hand (css.js:209-215). HMN-only, and only if empty-handed.
    if (bE && s->p1Type == 0 && s->cssCarry == -1) {
      s->cssHandType = 2; // css.js:210
      s->cssCarry = 0;
      ev_sel(s, "carry", 0);
    }
    if (s->cssCarry >= 0) {
      // --- carrying (css.js:216-296) ---
      s->cssHandType = 2; // css.js:217
      const int k = s->cssCarry;
      {
        FohHandRect cells[FOH_CSS_CHARS];
        foh_css_cells(cells);
        const int c = foh_hand_hit(cells, FOH_CSS_CHARS, s->cssHandX, s->cssHandY);
        if (c >= 0) {
          int *ch = css_char_of(s, k);
          if (*ch != c) {
            // Hovering SELECTS, live — not on press (css.js:222-226 and its
            // four siblings). The != guard is what makes the sound fire once
            // per change rather than every frame. Upstream writes BOTH planes
            // here: `chosenChar[k] = c` inline, then changeCharacter's setCS
            // (css.js:165-166) moves the shared selection — which is why the
            // two are separate fields and only this site writes both.
            *ch = c;
            s->selChar[k] = c; // the SHARED plane, by PORT (A44; foh.h)
            snd_push(s, "menuSelect"); // css.js:225
            ev_sel(s, kCssCharField[k], c);
          }
          if (aE) {
            // A drops it into the hovered cell and the character's announcer
            // plays (css.js:227-233). handType is deliberately NOT cleared:
            // upstream leaves it at 2 for this draw, so the grab sprite shows
            // one more frame.
            s->cssCarry = -1;
            s->cssTokenRest[k] = 0; // the A-drop slot (quirk Q1)
            snd_push(s, kCssAnnouncer[c]);
            ev_sel(s, "carry", -1);
          }
        }
      }
    } else {
      // (b) A on a token you may take (css.js:297-313).
      //
      // DEVIATION D40 (A44, owner-reported "why can't I turn on player 3 and
      // 4"). Upstream's guard here is `playerType[j] == 1 || i == j`: hand i
      // takes its own token or any CPU port's, never another human's. That
      // guard protects one human from another human's hand — upstream has
      // FOUR hands. This device has ONE (foh.h PORT MODEL), so the guard
      // protects nobody and instead removes the only way to give a HUMAN
      // port a character. For P2 that was survivable via the CPU detour D6
      // documented. For P3/P4 it is not: D40(b) leaves CPU off their cycle,
      // so there is no detour and a switched-on P3 could never be given a
      // character — the stub HARD RULE 2 forbids. The one hand therefore
      // owns every token. Everything else about the grab is upstream's.
      //
      // The N/A asymmetry SURVIVES and widens with it: a port's token stays
      // grabbable while nothing is drawn for it (foh.h D4 exception (b)),
      // which is upstream's own mismatch between this guard and its token
      // draw. Carried verbatim rather than tidied, for all four ports.
      //
      // occupiedToken[j] is false for every j here (nothing is carried).
      // The 2x2 (D41) is tangent, not overlapping, so at most one j can hit.
      for (int j = 0; j < FOH_CSS_PORTS; j++) { // D40(a): no ownership guard
        double tx, ty;
        foh_css_token_pos(s, j, &tx, &ty);
        if (s->cssHandY > ty - (double)FOH_CSS_TOKEN_R &&
            s->cssHandY < ty + (double)FOH_CSS_TOKEN_R &&
            s->cssHandX > tx - (double)FOH_CSS_TOKEN_R &&
            s->cssHandX < tx + (double)FOH_CSS_TOKEN_R) {
          if (aE) {
            s->cssHandType = 2; // css.js:304
            s->cssCarry = j;
            ev_sel(s, "carry", j);
            break;
          }
        }
      }
    }
  } else if (s->cssCpuCarry >= 0) {
    // --- CPU-knob drag (css.js:316-334) -------------------------------------
    // The hand is LOCKED to the rail: y forced, x clamped to the track, and
    // the level re-derived from the position every frame.
    const int k = s->cssCpuCarry;
    const double x0 = css_rail_x0(k);
    // css.js:317 forces the hand to cpuSlider.y + 15 — BELOW the knob centre,
    // because the hand's hot spot is its fingertip and the glove hangs down.
    // 15 px of a 750-tall canvas is 2%, i.e. 4.8 px here (D4 scaling).
    s->cssHandY = foh_css_knob_y() + FOH_CSS_DRAG_DY;
    if (s->cssHandX < x0) s->cssHandX = x0;
    if (s->cssHandX > x0 + (double)FOH_CSS_RAIL_LEN) {
      s->cssHandX = x0 + (double)FOH_CSS_RAIL_LEN;
    }
    // css.js:324: the slider takes the RAW hand x, continuously — the knob
    // follows the hand rather than snapping to the level's four stops, and it
    // keeps that exact position after release so a re-grab hit-tests where the
    // knob is drawn.
    s->cssSliderX[k] = s->cssHandX;
    {
      const int v = css_level_at(k, s->cssSliderX[k]);
      if (v != s->cpuDifficulty[k]) {
        // No sound here: upstream's whole drag arm (css.js:316-334) plays
        // none. The structural event stays — the level IS launch state.
        s->cpuDifficulty[k] = v;
        // A49: the field name comes from a per-PORT table, never a `k == 0 ?`
        // ternary. A ternary has room for two answers and there are four
        // ports, so port 2's knob would have reported itself as port 1's —
        // CONTEXT.md's "Port is never an index into something else", which is
        // the same table kCssCharField/kCssTypeField exist for.
        ev_sel(s, kCssDiffField[k], v);
      }
    }
    if (aE) { // release (css.js:328-333)
      s->cssCpuCarry = -1;
      s->cssHandType = 0; // css.js:332
      allowRegrab = false;
    }
  } else {
    // --- everything else (css.js:336-357) -----------------------------------
    s->cssHandType = 0; // handPoint (css.js:336)
    // Leaving the roster band while carrying SILENTLY COMMITS whatever was
    // last hovered and returns the token to rest (css.js:336-347). There is no
    // invalid-drop rejection and no snap-back-to-origin.
    //
    // QUIRK Q1 is CARRIED (foh.h FOH_CSS_TOKEN_LB_*): this rest slot uses
    // upstream's OTHER formula, so a token committed by walking out of the
    // band lands one cell right of the character it selected, exactly as
    // upstream's does. MENU-SPEC §2.6 says "carried verbatim" and it is; only
    // the spec's "a few px" magnitude is wrong (measured: 99 px, one cell).
    // QUIRK Q2 — upstream calls setTokenPosValue unconditionally, writing
    // tokenPos[-1] when nothing is carried; our rest position is computed from
    // cssTokenRest[k], so index -1 is unrepresentable and the write is a
    // no-op by construction rather than by a guard.
    if (s->cssCarry >= 0) {
      s->cssTokenRest[s->cssCarry] = 1; // the leave-band slot (quirk Q1)
      s->cssCarry = -1;
      ev_sel(s, "carry", -1);
    }
    // The port-type box is a clickable widget (css.js:348-357): any hand may
    // toggle any port's box. A44 runs the loop over all four ports.
    for (int j = 0; j < FOH_CSS_PORTS; j++) {
      const double px = (double)foh_css_panel_x(j);
      if (s->cssHandY > (double)FOH_CSS_PANEL_Y &&
          s->cssHandY < (double)(FOH_CSS_PANEL_Y + FOH_CSS_TAB_H) &&
          s->cssHandX > px && s->cssHandX < px + (double)FOH_CSS_TAB_W) {
        if (aE) {
          // togglePort, main.js:504-520, with DEVIATION D5: NET (2) is not a
          // reachable state, so the cycle wraps one step early —
          // N/A(-1) -> HMN(0) -> CPU(1) -> N/A. The `ports <= i` HMN-skip arm
          // is inert here (foh.h PORT MODEL note). Upstream also clears the
          // port's name tag; tags are DEVIATION D8 and not in this build.
          //
          // A49: ONE CYCLE FOR ALL FOUR PORTS. A44's DEVIATION D40(b) wrapped
          // ports 2/3 a step earlier (`wrapAt = j < 2 ? 2 : 1`) so CPU was
          // unreachable there; the owner RETIRED it (*"yeah enable the CPu
          // please"*), because D40(b)'s stated ground was wrong — see the
          // launch guard below and foh.h's note on portType. So this is
          // upstream's togglePort with only D5's NET step removed, for every
          // port, and the two-cycle asymmetry is gone rather than widened.
          int *t = &s->portType[j];
          (*t)++;
          if (*t == 2) *t = -1;
          snd_push(s, "menuSelect"); // css.js:351
          ev_sel(s, kCssTypeField[j], *t);
        }
      }
    }

    // The MODE RIBBON (css.js:389-394; A27, the owner's "if you click the
    // 'VS Melee' in the CSS it should change modes"). Upstream's own arm,
    // carried whole: an A RISING EDGE inside the widget's rect plays
    // menuSelect and calls `setVersusMode(1 - versusMode)` — a BINARY
    // toggle, not a cycle and not a picker. Nothing about the trigger
    // deviates (the BACK wedge's D22 hold is a different widget and a
    // different owner request); what D28 registers is the plate's rect and
    // its label, because upstream draws neither.
    //
    // POSITION IS UPSTREAM'S, not convenience: this sits between the
    // port-type boxes (css.js:348-357, above) and the CPU-knob grab
    // (css.js:396-408, below), inside the same "hand outside the roster
    // band" arm, so a hand carrying a token can no more toggle the mode
    // here than it can upstream. It reads THIS frame's hand position —
    // upstream's :389 runs after the hand integrates at :195 — unlike the
    // BACK counter above, which upstream runs before it.
    if (s->cssHandY > (double)FOH_CSS_MODE_Y0 &&
        s->cssHandY < (double)FOH_CSS_MODE_Y1 &&
        s->cssHandX > (double)FOH_CSS_MODE_X0 &&
        s->cssHandX < (double)FOH_CSS_MODE_X1) {
      if (aE) {
        snd_push(s, "menuSelect");            // css.js:392
        s->versusMode = 1 - s->versusMode;    // css.js:393
      }
    }
  }

  // CPU-knob grab (css.js:396-408) — outside the three-way branch, guarded on
  // not already dragging, and on allowRegrab so releasing cannot re-grab on
  // the same frame. occupiedCpu[] is implied: nothing is held here.
  if (s->cssCpuCarry == -1) {
    // FOH_CSS_PORTS, which is upstream's own bound: `for (var s = 0; s < 4;
    // s++)` at css.js:397 over the four-element cpuSlider (css.js:72). A44
    // capped it at 2 because D40(b) made a knob on ports 2/3 undrawable;
    // A49 retires D40(b), so all four knobs are real and grabbable.
    for (int j = 0; j < FOH_CSS_PORTS; j++) {
      if (foh_css_port_type(s, j) != 1) continue;
      const double kx = foh_css_knob_x(s, j), ky = foh_css_knob_y();
      if (s->cssHandY >= ky - (double)FOH_CSS_KNOB_R &&
          s->cssHandY <= ky + (double)FOH_CSS_KNOB_R &&
          s->cssHandX >= kx - (double)FOH_CSS_KNOB_R &&
          s->cssHandX <= kx + (double)FOH_CSS_KNOB_R) {
        if (aE && allowRegrab) {
          s->cssCpuCarry = j;
          s->cssHandType = 2; // css.js:406
          break;
        }
      }
    }
  }

  // Launch (css.js:443-451): START, rising edge, ONLY while ready. The two
  // d-pad arms that follow it upstream (css.js:452-459 — d-pad UP launches
  // unready, d-pad RIGHT force-selects falco) are dropped by DEVIATION D2, so
  // the not-ready state is inert, which is what the banner already says.
  // `cssReady` here is the value the DRAW pass left LAST frame.
  if (s->cssReady && in->start && !pv->start) {
    // LAUNCH-PLANE LIMITATION, registered and LOUD. What it used to refuse
    // was "anything other than exactly two ports", because sim_setup_match
    // hard-pinned slots 2/3 absent. **A46 retired that half** —
    // sim_setup_match_ports (port/sim/sim/sim_boot.c) is upstream's own
    // four-port harnessSetupMatch loop, and the launch site below now calls
    // it — so a 3- or 4-port configuration LAUNCHES rather than denying.
    //
    // THREE conditions remain, and each names the thing that is still not
    // real rather than a shape this screen dislikes:
    //   (1) port 0 must be HMN. A CPU port 0 has no input source on the
    //       launch path (the AI plane is fed per-slot and port 0 is the
    //       one the physical controller drives), so booting one would make
    //       the LAUNCH record a lie — HARD RULE 2. Unchanged refusal.
    //   (2) at least two participating ports. This is upstream's own ready
    //       rule re-checked at the seam: `cssReady` is LAST frame's value
    //       (css.js:1167-1181 runs in the draw pass), so toggling a port to
    //       N/A and pressing START on the SAME frame reaches here with a
    //       configuration the ready rule would have rejected. Counting the
    //       ports rather than testing two named ones is what makes
    //       P1 + P3 (with P2 off) a legal match, which it is upstream.
    // Delete (1) when a CPU port 0 has a real input source.
    //
    // A49 DELETED A THIRD CONDITION — "no CPU above port 1" (D40(b)) — by
    // OWNER RULING, and this is the ACCEPTED CONSEQUENCE, written where the
    // next reader of this guard will hit it rather than filed somewhere:
    //
    //   *** A MATCH WITH A CPU ON PORT 2 OR 3 IS PLAYABLE BUT NOT
    //   *** CHECKSUM-VERIFIED. The play path runs the LIVE C `ai.c` through
    //   *** `ml_sim_runai_live`, so the AI genuinely plays (fix_plan A48,
    //   *** `check-ai-live.sh` -> `AI LIVE CONFORMS`). What is missing is
    //   *** REPLAY: `AIBRIDGE1` (port/sim/ai_bridge.h) holds ONE recorded
    //   *** stream for ONE CPU slot, so no golden can currently replay a
    //   *** second one and no such configuration is covered by the M2/M3/M4
    //   *** conformance gates. fix_plan A48 tracks widening the bridge.
    //
    // D40(b) refused this configuration on the ground that "the sim refuses
    // it". MEASURED, that was wrong: AIBRIDGE1 is the RECORDED stream used to
    // REPLAY a CPU golden, not what makes the AI run. The real ground was
    // VERIFICATION COVERAGE, which is a scope call the owner owns and has now
    // made. Refusing here again would be re-deciding it.
    //
    // TICKET #25 (owner ruling 2026-08-27) WIDENED THAT CONSEQUENCE, and it
    // is restated here rather than left for the reader to infer, because
    // this guard is where a reader arrives asking "how does the machine end
    // up in that configuration?" — and the answer changed:
    //
    //   *** THE PORT TYPES AND CPU LEVELS ARE NOW PERSISTED TO SD. So the
    //   *** unverified configuration above is no longer only something a
    //   *** player assembles during a session. It can be the state the
    //   *** device BOOTS INTO: close the lid on a 3-or-4-port CPU match's
    //   *** character select and the next power-on comes back to it, armed,
    //   *** possibly reading READY TO FIGHT before anything is touched.
    //   *** The ruling was made knowing that; foh_persist.h's CSS machine
    //   *** plane carries the full statement and the reasons it overrode.
    {
      int participants = 0;
      for (int j = 0; j < FOH_CSS_PORTS; j++) {
        if (s->portType[j] > -1) participants++;
      }
      if (s->portType[0] != 0 || participants < 2) {
        snd_push(s, "deny");
        ev_refused(s, "portconfig");
        if (pendingBack) css_back(s);
        return;
      }
    }
    snd_push(s, "menuForward"); // css.js:448
    // START WINS over a pending B-hold exit: upstream's changeGamemode(6) is
    // the later call in the same frame (css.js:448 vs :189).
    ev_trans(s, FOH_CSS, FOH_SSS, "start");
    return;
  }
  // The pending B-hold exit, emitted only now that the rest of the CSS step
  // has run (see the counter above). readyToFight's draw-pass recompute is
  // NOT here — it belongs to the screen the tick ENDS on, and foh_tick owns
  // it (see the note there).
  if (pendingBack) css_back(s);
}

static void step_sss(FohState *s, const PlatformInput *in,
                     const PlatformInput *pv) {
  const bool aE = in->a && !pv->a;
  const bool bE = in->b && !pv->b;
  if (bE) {
    snd_push(s, "menuBack"); // stageselect.js:78
    ev_trans(s, FOH_SSS, FOH_CSS, "b"); // stageselect.js:79
    return;
  }
  if (aE) {
    if (s->sssCursor == 6) {
      // The RANDOM slot REFUSES (registered exclusion, MEASURED:
      // upstream's arm draws from the seeded Math.random stream,
      // stageselect.js:80-84 — a live draw would desync
      // live-vs-replay stream prefixes; foh.h header note).
      snd_push(s, "deny");
      ev_refused(s, "random");
      return;
    }
    // setStageSelect + startGame (stageselect.js:80-88); the launch
    // record freezes here, the driver (foh_app / device app) owns the
    // actual sim boot.
    snd_push(s, "menuForward"); // stageselect.js:81
    s->stageSel = s->sssCursor;
    // The launch KIND, stated by every launch arm rather than left over from
    // the last one (review-mexit-r2 High). Harmless while a process could
    // only ever launch once; with the A19 in-process return it is a real
    // defect — target -> TSS -> B -> menu -> VS -> SSS -> A left targetMode
    // true, so the "VS" launch dispatched through the TARGET bridge.
    s->targetMode = false;
    s->launched = true;
    ev_trans(s, FOH_SSS, FOH_MATCH, "launch");
    ev_launch(s);
    return;
  }
  // 3x2 grid cursor over stage ids 0..5 (== oracle --stage ids) plus
  // the RANDOM slot at 6 (below the grid): D from the bottom row
  // enters it, U returns to the middle bottom tile (4); L/R are
  // no-ops on it (CLAMP semantics, the rewritten-nav class).
  const bool uE = in->up && !pv->up;
  const bool dE = in->down && !pv->down;
  const bool lE = in->left && !pv->left;
  const bool rE = in->right && !pv->right;
  const int before = s->sssCursor;
  if (s->sssCursor == 6) {
    if (uE) s->sssCursor = 4;
  } else {
    if (lE) s->sssCursor = clampi(s->sssCursor - 1, 0, 5);
    if (rE) s->sssCursor = clampi(s->sssCursor + 1, 0, 5);
    if (uE && s->sssCursor >= 3) s->sssCursor -= 3;
    if (dE) {
      if (s->sssCursor <= 2) s->sssCursor += 3;
      else s->sssCursor = 6;
    }
  }
  if (s->sssCursor != before) {
    snd_push(s, "menuSelect"); // stageselect.js:64/73 change class
  }
}

// target-select (upstream stages/targetselect.js tssControls; the
// rewrite deltas + per-edge citations live in foh.h).
// A45 T3 — refresh the custom-slot presence cache. Called on entry to
// target-select and on every page flip, never per frame: one scan opens ten
// files, and the set can only change by leaving this screen (the builder is
// the only writer).
//
// D43 — BY INDEX. Slot i is reported in position i whether it is present,
// empty or corrupt; nothing is compacted, nothing shifts up. That is the
// owner's ruling made structural: there is no length here to be off by one.
void foh_tss_refresh_slots(FohState *s) {
  for (int i = 0; i < FOH_TB_SLOT_CACHE; i++) {
    s->tssSlotPresent[i] = false;
    s->tssSlotReason[i] = "UNAVAILABLE IN THIS BUILD";
  }
  if (foh_tbuild_ops) {
    foh_tbuild_ops->slots(s->tssSlotPresent, s->tssSlotReason);
  }
}

static void step_tss(FohState *s, const PlatformInput *in,
                     const PlatformInput *pv) {
  const bool aE = in->a && !pv->a;
  const bool sE = in->start && !pv->start;
  const bool bE = in->b && !pv->b;
  if (bE) {
    // targetselect.js:76-81: menuBack + playMenuLoop + changeGamemode(1);
    // menuSelected UNTOUCHED (module state — still TARGETTEST).
    snd_push(s, "menuBack");
    ev_trans(s, FOH_TSS, FOH_MENU_TOP, "b");
    return;
  }
  // The FREE HAND (DEVIATION D29, A25c), integrated and hit-tested HERE —
  // before the launch arm, not after it, which is the whole owner-visible
  // point: A launches the slot the hand is over THIS frame. It is also the
  // CSS's own order (css.js:195-206 integrates, then every widget arm reads
  // the new position) and upstream's on this screen, where the pointer moves
  // and the click is tested against where it now is.
  //
  // The selection is STICKY: a hand in a gutter, or below the grid, leaves
  // tssCursor where it was. See the argument at FohState.tssCursor.
  {
    const int before = s->tssCursor;
    foh_hand_step(&s->tssHandX, &s->tssHandY, in->up, in->down, in->left,
                  in->right, (double)RAST_W, (double)RAST_H);
    FohHandRect slots[FOH_TSS_SLOTS];
    foh_tss_slots(slots);
    const int hit =
        foh_hand_hit(slots, FOH_TSS_SLOTS, s->tssHandX, s->tssHandY);
    if (hit >= 0) s->tssCursor = hit;
    if (s->tssCursor != before) {
      // targetselect.js:51 changes the slot's class on hover; the `!=` guard
      // is the CSS drop arm's, so the sound fires once per CHANGE rather than
      // on every frame the hand sits in a slot.
      snd_push(s, "menuSelect");
    }
  }
  // char select — the upstream SHOULDER arms VERBATIM (targetselect.js:
  // 60-74: input.l = char-1 WRAP, input.r = char+1 WRAP; setCS writes
  // characterSelections[0] == p1Char, the SAME array the CSS edits).
  if (in->l && !pv->l) {
    s->p1Char = s->p1Char == 0 ? 4 : s->p1Char - 1; // :62-66 wrap
    snd_push(s, "menuSelect"); // :67
    ev_sel(s, "p1char", s->p1Char);
  } else if (in->r && !pv->r) {
    s->p1Char = s->p1Char == 4 ? 0 : s->p1Char + 1; // :68-73 wrap
    snd_push(s, "menuSelect"); // :74
    ev_sel(s, "p1char", s->p1Char);
  }
  if (aE || sE) { // targetselect.js:131 accepts START or A
    if (s->tssCursor == 10) {
      // A45 T3 — THE PAGE FLIP. This slot REFUSED until now (`deny` +
      // ev_refused "addcode"): upstream's "+ Add Code" opens an HTML
      // textarea to paste a ~1 KB share code into (targetselect.js:132-136),
      // and this device has no clipboard, no keyboard and no network. A45 T2
      // (DEVIATION D42) already replaced that transport with a FILE on the
      // SD card, which left this slot the job the design spike named for it:
      // showing the custom stages that were found.
      //
      // So it flips the grid between the ten AUTHORED stages and the ten
      // CUSTOM slots (upstream's "Custom N", :288-294). Upstream shows both
      // families at once in four columns of a 1200 px canvas; 240 px holds
      // two, so the same ten rects carry whichever family is on show.
      s->tssPage = s->tssPage ? 0 : 1;
      foh_tss_refresh_slots(s);
      snd_push(s, "menuSelect");
      return;
    }
    if (s->tssPage) {
      // A CUSTOM slot. `targetSelected > 9` upstream (:140-142):
      // setActiveStageCustomTarget(targetSelected - 10) then
      // setTargetStagePlaying(targetSelected) — so the id CARRIED is the
      // 10 + slot one, which is exactly A45 T2's MLK_PLAYING_BASE + slot
      // (custom_stage.h). The driver's launch seam reads tssStage >= 10 and
      // routes through tp_setup_target_custom instead of tp_setup_target.
      if (!s->tssSlotPresent[s->tssCursor]) {
        // VISIBLY refused: the renderer draws tssSlotReason for the hovered
        // slot, so "empty" or "SUM mismatch" is on screen, not just a beep.
        snd_push(s, "deny");
        return;
      }
      snd_push(s, "menuForward"); // :132
      s->tssStage = FOH_TB_SLOT_CACHE + s->tssCursor;
      s->targetMode = true;
      s->launched = true;
      ev_trans(s, FOH_TSS, FOH_TMATCH, "launch");
      ev_launch(s);
      return;
    }
    // targetselect.js:131-146: menuForward + setActiveStageTarget +
    // setTargetStagePlaying + startTargetGame(0, false); the driver
    // owns the actual target sim boot (the LAUNCH seam).
    snd_push(s, "menuForward"); // :132
    s->tssStage = s->tssCursor;
    s->targetMode = true;
    s->launched = true;
    ev_trans(s, FOH_TSS, FOH_TMATCH, "launch");
    ev_launch(s);
    return;
  }
  // D29 retired the d-pad GRID CURSOR that stood here (rewrite delta, foh.h):
  // the hand above is what moves the selection now, and `pv` is used only by
  // the button edges this function still reads.
}

// menuVOptions / menuHOptions (gameplaymenu.js:11-12). BOTH are MAX
// INDICES, not counts: five rows, and only the last one has columns.
// The COUNTS are foh.h's FOH_OPT_ROWS / FOH_OPT_COLS since ticket #27 (the
// persist table needs them as column bounds and cannot link this TU), so
// they are derived here rather than restated — one fact, two spellings of
// it, and the -1 says which spelling this one is.
#define FOH_OPT_ROWMAX (FOH_OPT_ROWS - 1)
static const int kOptColMax[FOH_OPT_ROWMAX + 1] = {0, 0, 0, 0,
                                                   FOH_OPT_COLS - 1};

static void step_opt_gameplay(FohState *s, const PlatformInput *in,
                              const PlatformInput *pv) {
  // gameplayMenuControls (gameplaymenu.js:23-164) is ONE else-if chain —
  // B -> A -> up -> down -> right -> left -> neutral — so exactly one arm
  // runs per frame. That priority is kept here over d-pad EDGES (the
  // 10-frame autorepeat is the pre-registered rewrite delta, iter 88).
  // DEVIATION D9 (MENU-SPEC §3.4): upstream's diagonal guards are
  // malformed (`!(Math.abs(lsX >= 0.7))` applies Math.abs to a BOOLEAN, so
  // up-left is accepted and up-right is rejected) and its left arm omits
  // stickHoldEach, repeating at 60 Hz instead of 6. Neither is reproduced;
  // the chain's own order already makes vertical win a diagonal.
  const bool bE = in->b && !pv->b;
  const bool aE = in->a && !pv->a;
  const bool uE = in->up && !pv->up;
  const bool dE = in->down && !pv->down;
  const bool lE = in->left && !pv->left;
  const bool rE = in->right && !pv->right;
  if (bE) {
    // gameplaymenu.js:25-36: menuBack, then setCookie over EVERY
    // gameSettings key (the persist chokepoint's save, foh_app/foh_dev),
    // then changeGamemode(1) with menuMode/menuSelected untouched — so the
    // cursor is still on the Gameplay row. The meHost gate (:32's blocking
    // alert on a joined client) is netplay-only and out of scope.
    s->menuSelected = 1;
    snd_push(s, "menuBack"); // gameplaymenu.js:26
    ev_trans(s, FOH_OPT_GAMEPLAY, FOH_MENU_OPTIONS, "b");
    return;
  }
  if (aE) {
    // A is the ONLY value-changing input on this screen (:37-59), and it
    // plays menuSelect unconditionally at :38, before the switch.
    snd_push(s, "menuSelect");
    switch (s->optRow) {
      case 0:
        s->turbo ^= 1; // :41 `turbo ^= true` (coerces, stays 0/1)
        ev_sel(s, "turbo", s->turbo);
        break;
      case 1:
        s->lCancelType++; // :44-47 ++ then wrap >2 -> 0
        if (s->lCancelType > 2) s->lCancelType = 0;
        ev_sel(s, "lcancel", s->lCancelType);
        break;
      case 2:
        s->flashOnLCancel ^= 1; // :50
        ev_sel(s, "flashlcancel", s->flashOnLCancel);
        break;
      case 3:
        s->everyCharWallJump ^= 1; // :53 — the measured-dead toggle
        ev_sel(s, "walljump", s->everyCharWallJump);
        break;
      default:
        // :56 gameSettings["tapJumpOffp" + (menuIndex[1]+1)] ^= true;
        // the field token carries the same 1-based port the key does.
        s->tapJumpOff[s->optCol] ^= 1;
        ev_sel(s,
               s->optCol == 0   ? "tapjump1"
               : s->optCol == 1 ? "tapjump2"
               : s->optCol == 2 ? "tapjump3"
                                : "tapjump4",
               s->tapJumpOff[s->optCol]);
        break;
    }
    return;
  }
  bool moved = false;
  if (uE || dE) {
    s->optRow += uE ? -1 : 1;
    // :62-65 clamps the column against the NEW row BEFORE the wrap. When
    // the row has gone out of range upstream reads menuHOptions[-1] /
    // [5] == undefined and `col > undefined` is FALSE, so no clamp runs —
    // hence the range guard here rather than a saturating index.
    if (s->optRow >= 0 && s->optRow <= FOH_OPT_ROWMAX &&
        s->optCol > kOptColMax[s->optRow]) {
      s->optCol = kOptColMax[s->optRow];
    }
    moved = true;
  } else if (rE) {
    s->optCol++; // :101 (upstream's clamp at :102-104 is commented out)
    moved = true;
  } else if (lE) {
    s->optCol--; // :117
    moved = true;
  }
  if (moved) {
    // :150-163 — one menuSelect per accepted move, then the wraps. The
    // column wraps against the POST-wrap row, and on a single-column row
    // any left/right press wraps straight back to 0: an audible no-op,
    // which is exactly what upstream does.
    snd_push(s, "menuSelect"); // :152
    if (s->optRow < 0) s->optRow = FOH_OPT_ROWMAX;
    else if (s->optRow > FOH_OPT_ROWMAX) s->optRow = 0;
    if (s->optCol > kOptColMax[s->optRow]) s->optCol = 0;
    else if (s->optCol < 0) s->optCol = kOptColMax[s->optRow];
  }
}

// --- AUDIO OPTIONS (upstream menus/audiomenu.js; MENU-SPEC §4) --------------
// audioMenuControls (:16-121) is the same else-if shape as gameplaymenu with
// one loud difference: there is NO A HANDLER AT ALL (measured — `.a` does not
// appear in the file), so A does nothing here. Chain: B -> up -> down ->
// right -> left -> neutral, i.e. lsY is tested before lsX, so a diagonal
// moves the cursor and never touches a volume.
static void step_opt_audio(FohState *s, const PlatformInput *in,
                           const PlatformInput *pv) {
  const bool bE = in->b && !pv->b;
  const bool uE = in->up && !pv->up;
  const bool dE = in->down && !pv->down;
  const bool lE = in->left && !pv->left;
  const bool rE = in->right && !pv->right;
  if (bE) {
    // :20-26 — menuBack, setCookie("soundsLevel"/"musicLevel", ...) and
    // changeGamemode(1) with menuMode/menuSelected untouched, so the cursor
    // is still on the Audio row. Unlike gameplaymenu there is NO meHost
    // gate: audio always saves.
    s->menuSelected = 0;
    snd_push(s, "menuBack"); // :22
    ev_trans(s, FOH_OPT_AUDIO, FOH_MENU_OPTIONS, "b");
    return;
  }
  if (uE || dE) {
    s->audioRow += uE ? -1 : 1; // :30 / :44
    snd_push(s, "menuSelect");  // :95
    if (s->audioRow == -1) s->audioRow = 1; // :96-99
    else if (s->audioRow == 2) s->audioRow = 0;
    return;
  }
  if (rE || lE) {
    // WIRED 2026-07-29 (owner ruling: "yep wire"). The level below is
    // edited, clamped, persisted AND converted for the live mixer by
    // snd_bus_set (port/gfx/snd_mixer.h's AUDIO BUS note) — this machine's
    // stand-in for audiomenu.js:114-120's changeVolume call. The FOH itself
    // stays I/O-free: it owns the VALUE, the app owns the push.
    // The DEVICE app's call site (foh_dev.c) is a pending cross-lane patch,
    // not landed code — MENU-SPEC §4 and the work order say so plainly.
    // Navigation here is rising-edge only (DEVIATION D16, §5.5 row 6):
    // upstream repeats a held direction 1-then-every-10 frames, we step once.
    // :103/:109 — a fixed +/-0.1 step with NO rounding, so the float dust
    // (0.7999999999999999) accumulates exactly as it does upstream and is
    // what gets persisted. menuSelect plays on EVERY accepted step,
    // including one that the clamp turns into a no-op (:102/:108 precede
    // the clamps) — so a rail-end press still clicks.
    const int k = s->audioRow;
    const double before = s->masterVolume[k];
    snd_push(s, "menuSelect");
    s->masterVolume[k] += rE ? 0.1 : -0.1;
    if (s->masterVolume[k] > 1.0) s->masterVolume[k] = 1.0; // :104-106
    if (s->masterVolume[k] < 0.0) s->masterVolume[k] = 0.0; // :110-112
    // The engine push (:114-120, the global changeVolume installed by
    // sfx.js:609) has no counterpart inside this machine: the FOH is
    // I/O-free by construction, so the value is state the app reads. Its
    // consumer is the app's bus push -> snd_bus_set, which
    // scales snd_mixer.h's two accumulate sites by masterVolume/default
    // (the RATIO — the packed gains already carry the 0.5/0.3 defaults, so
    // pushing the raw level would apply the default twice).
    if (s->masterVolume[k] != before) {
      // Traced in TENTHS: the structural plane carries an integer, the
      // machine keeps the raw double (dust included).
      ev_sel(s, k == 0 ? "soundsvol" : "musicvol",
             (int)(s->masterVolume[k] * 10.0 + 0.5));
    }
    return;
  }
}

// --- CREDITS (upstream menus/credits.js; MENU-SPEC §8; punch-list A7) ------
//
// A TRANSLITERATION of `credits(p, input)` (credits.js:102-247), in upstream's
// own statement order, PLUS the two motion loops that upstream puts in
// `drawCredits` (:318-357). Those two are folded in at the bottom of this
// function on purpose: upstream runs its logic on a fixed timer and its
// drawing on requestAnimationFrame, so the star and laser motion is tied to
// the RENDER rate there (and is skipped outright in its 30 fps mode,
// main.js:1163). This port renders as a PURE FUNCTION of FohState — that is
// what makes every shot byte-stable — so anything that moves has to move in
// the tick. One tick = one logic pass then one draw pass, which is the frame
// upstream is trying to have.
//
// TWO EXITS, both to gameMode 1 (:224-246), and gameMode 1 is the MENU with
// menuMode and menuSelected untouched — so both land back on Options with the
// cursor still on CREDITS. Upstream additionally pokes `input[p][1].b = true`
// (:233,:241) so the same B press cannot read as a fresh edge on the menu the
// next frame; foh_tick already stores the whole input as `prev` at the end of
// every tick, so a held B is recorded and no edge appears. Same observable,
// no poke needed.
//
// QUIRK Q6 (MENU-SPEC §8.4), carried verbatim: `cScrollingPos` advances by a
// flat +2 at :143 REGARDLESS of the START/L/R fast-forward, which only speeds
// the names up (:145-153). Holding fast-forward therefore runs the names off
// the top well before the mode ends, and the mode still ends at frame 2500.
static void step_credits(FohState *s, const PlatformInput *in,
                         const PlatformInput *pv) {
  const bool aE = in->a && !pv->a;
  const bool bE = in->b && !pv->b;
  const bool xE = in->x && !pv->x;
  const bool yE = in->y && !pv->y;

  // :104-111 — X cycles the laser colour forward, Y back, wrapping in both
  // directions across the four `laserColors` (:32-37; the colours themselves
  // live in the renderer).
  if (xE) s->credLaser = (s->credLaser == 3) ? 0 : s->credLaser + 1;
  if (yE) s->credLaser = (s->credLaser == 0) ? 3 : s->credLaser - 1;

  if (s->credInit) cred_reset(s); // :112-138

  if (s->credCursorAngle >= 360.0) s->credCursorAngle = 0.0; // :140-142
  s->credScrollPos -= -2;                                    // :143, Q6

  // :145-153 — START **or** either shoulder, held (level, not edge).
  int yDif;
  if (in->start || in->l || in->r) {
    s->credCursorAngle += 4.5;
    yDif = -3; // Math.round(cScrollingSpeed * 1.5)
  } else {
    s->credCursorAngle += 3.0;
    yDif = -2; // Math.round(cScrollingSpeed)
  }

  for (int i = 0; i < FOH_CRED_NAMES; i++) {
    // scrollY (:84-93): the wobble reverses at +/-xMax, then one px of
    // sideways drift and the scroll step.
    if (s->credNameXVal[i] == s->credNameXMax[i] && s->credNameXDir[i] == 1) {
      s->credNameXDir[i] = 0;
    } else if (s->credNameXVal[i] == -1 * s->credNameXMax[i] &&
               s->credNameXDir[i] == 0) {
      s->credNameXDir[i] = 1;
    }
    s->credNameX[i] += -1 + (2 * s->credNameXDir[i]);
    s->credNameXVal[i] += -1 + (2 * s->credNameXDir[i]);
    s->credNameY[i] += yDif;
    // checkIfShouldRender(cYSize) (:59-66) — size()[1] is [yPos-23, yPos].
    s->credNameRender[i] =
        (s->credNameY[i] - 23 < 750) && (s->credNameY[i] > 0);
  }

  // :158-166 — the info panel's 600-frame dwell, then it clears.
  if (s->credHitTimer > 0) s->credHitTimer -= 1;
  else if (!s->credHitCleared) s->credHitCleared = true;

  // :168-186 replaced by DEVIATION D12 (MENU-SPEC §8.3). Upstream maps the
  // UNDEADENED stick absolutely onto the canvas, which a d-pad reduces to
  // nine reachable positions and makes the screen unplayable (it always ends
  // on `failure`). This is the CSS/target-select cursor instead — the same
  // shared body, the same clamp to the logical canvas — and it is the only
  // credits semantic that changes.
  foh_hand_step(&s->credX, &s->credY, in->up, in->down, in->left, in->right,
                (double)RAST_W, (double)RAST_H);

  // :188-203 — fire. One A edge inside the cooldown is REMEMBERED (a
  // one-frame-deep buffer) and fires the instant the cooldown expires.
  if (s->credCool == 0) {
    if (aE || s->credShootBuf) {
      snd_push(s, "foxlaserfire"); // :192
      // The twin lasers: same target, one from each bottom corner
      // (:193-194 — position (0,0) and (1200,0) in the y-flipped laser
      // space, i.e. the two bottom corners of the canvas).
      for (int type = 0; type < 2; type++) {
        int slot = -1;
        for (int n = 0; n < FOH_CRED_SHOTS; n++) {
          if (!s->credShot[n].live) { slot = n; break; }
        }
        // Unreachable by construction (the bound is derived at
        // FOH_CRED_SHOTS); loud rather than a silently dropped laser.
        if (slot < 0) gfx_fatal("foh: credits shot table overflowed");
        FohCredShot *sh = &s->credShot[slot];
        const double px = (type == 0) ? 0.0 : (double)RAST_W;
        const double py = 0.0;
        const double tx = s->credX;                    // :193 target.x
        const double ty = (double)RAST_H - s->credY;   // :267's 750 - y
        const double dx = tx - px, dy = ty - py;
        // sx/sy are `distance * cos(angle)` and `distance * sin(angle)` for
        // upstream's angle (:271-274), in closed form. With
        // angle = atan(dy/dx), cos(angle) = |dx|/hyp and
        // sin(angle) = dy*sgn(dx)/hyp, so distance*cos == dx*sgn(dx) and
        // distance*sin == dy*sgn(dx) — no transcendental, and no rounding
        // difference either, because atan's own range (-pi/2, pi/2) is what
        // makes the identity exact. dx == 0 gives JS atan(+/-Infinity) =
        // +/-pi/2, which is the sgn = +1 branch, so it needs no case of its
        // own. `type` adds pi (:272-274), i.e. negates both.
        const double sgn = (dx < 0.0) ? -1.0 : 1.0;
        sh->sx = (type ? -1.0 : 1.0) * dx * sgn;
        sh->sy = (type ? -1.0 : 1.0) * dy * sgn;
        // ONE non-reproduction, stated: with the reticle exactly on the
        // corner a laser starts from, upstream's 0/0 makes angle and
        // distance NaN and the bolt vanishes into NaN coordinates. Here the
        // same case gives a zero step, so the bolt sits on the corner for
        // its 25 frames. Feeding NaN to the rasteriser is not faithfulness.
        // :267 — the target pair, kept in the y-FLIPPED laser space exactly
        // as upstream stores it, and un-flipped again at the hit test
        // (:211's `750 - target.y`).
        sh->tx = tx;
        sh->ty = ty;
        sh->x = sh->lx = sh->l2x = px; // :268-270
        sh->y = sh->ly = sh->l2y = py;
        sh->vel = 0.3; // :266
        sh->life = 0;
        sh->live = true;
      }
      s->credCool = 8; // :196
      s->credShootBuf = false;
    }
  } else {
    if (aE) s->credShootBuf = true; // :199-201
    s->credCool -= 1;
  }

  // :205-222 — a bolt lands on frame 15 of its life and is tested against the
  // names WHERE THEY ARE NOW. The inner loop keeps the LAST match rather than
  // breaking (:210-214), and the twin bolts land together: the first marks
  // the name shot, the second finds `isShot` already true and scores nothing
  // (checkIfShot, :72-83).
  FohHandRect rect[FOH_CRED_NAMES];
  foh_credits_name_rects(s, rect);
  for (int n = 0; n < FOH_CRED_SHOTS; n++) {
    if (!s->credShot[n].live || s->credShot[n].life != 15) continue;
    int made = -1;
    // The bolt's aim point, back in screen coordinates: upstream stores the
    // target y-flipped and un-flips it here (:211's `750 - target.y`).
    const double px = s->credShot[n].tx;
    const double py = (double)RAST_H - s->credShot[n].ty;
    for (int i = 0; i < FOH_CRED_NAMES; i++) {
      if (s->credNameShot[i]) continue;
      // isHovering (:67-73) — INCLUSIVE on all four sides, which is why this
      // is not foh_hand_hit (that one is strict, for the CSS's gutters).
      if (px >= (double)rect[i].x && px <= (double)(rect[i].x + rect[i].w) &&
          py >= (double)rect[i].y && py <= (double)(rect[i].y + rect[i].h)) {
        made = i;
      }
    }
    if (made >= 0) {
      s->credNameShot[made] = true;
      snd_push(s, "targetBreak"); // :216
      s->credHitCleared = false;  // :217
      s->credHitTimer = 600;      // :218
      s->credHitIdx = made;       // :219
      s->credScore += 1;          // :220
    }
  }

  // :224-246 — the two exits. The timer is checked FIRST, so a B pressed on
  // the very last frame still ends on complete/failure.
  if (s->credScrollPos >= 5000) { // cScrollingMax (:25)
    snd_push(s, s->credScore == FOH_CRED_NAMES ? "complete" : "failure");
    s->credInit = true;
    for (int n = 0; n < FOH_CRED_SHOTS; n++) s->credShot[n].live = false;
    s->credHitTimer = 0;
    s->credHitIdx = 0;
    s->credHitCleared = false;
    ev_trans(s, FOH_CREDITS, FOH_MENU_OPTIONS, "timer"); // changeGamemode(1)
    return;
  }
  if (bE) {
    s->credInit = true;
    snd_push(s, "menuBack"); // :236
    for (int n = 0; n < FOH_CRED_SHOTS; n++) s->credShot[n].live = false;
    s->credHitTimer = 0;
    s->credHitIdx = 0;
    s->credHitCleared = false;
    ev_trans(s, FOH_CREDITS, FOH_MENU_OPTIONS, "b"); // changeGamemode(1)
    return;
  }

  // --- the draw pass's motion (credits.js:318-357), see the header note ---
  for (int n = 0; n < FOH_CRED_STARS; n++) {
    FohCredStar *st = &s->credStar[n];
    st->life++;                                          // :319
    if (st->life == 200) cred_star_spawn(s, st, false);  // :320-326
    st->x += st->dx;                                     // :327
    st->y += st->dy;                                     // :328
  }
  for (int n = 0; n < FOH_CRED_SHOTS; n++) {
    FohCredShot *sh = &s->credShot[n];
    if (!sh->live) continue;
    sh->life++;      // :335
    sh->vel *= 0.77; // :336 — a decaying fraction of the whole distance, so
                     // the bolt converges on the reticle and stops
    sh->l2x = sh->lx; // :337
    sh->l2y = sh->ly;
    sh->lx = sh->x; // :338
    sh->ly = sh->y;
    sh->x += sh->vel * sh->sx; // :339
    sh->y += sh->vel * sh->sy; // :340
    if (sh->life == 25) sh->live = false; // :341-342
  }
}

// --- CONTROLS DESTINATIONS (MENU-SPEC §9) -----------------------------------
// Page 3's two rows finally go somewhere. Neither destination is a port of
// its upstream module, and both say so on screen:
//
//   Controller (gameMode 14, controllermenu.js + gamepadCalibration.js) is
//   MOUSE-ONLY upstream — 14 clickable regions and an SVG diagram, no
//   stick/keyboard arm anywhere (MENU-SPEC §9.2) — and it hard-requires
//   navigator.getGamepads. There is no mouse, no gamepad API and no pad on
//   a FunKey-S, so every calibration primitive is dead by construction. What
//   upstream ITSELF shows when no pad answers is `Error: no controller
//   detected` (gamepadCalibration.js:71), and that is what we show: its own
//   literal string, in its own condition. The one deviation is the way out —
//   upstream forgets to reschedule preCalibrationLoop there, so its menu
//   becomes permanently unresponsive with no route back but a page reload
//   (:71, measured; its only exit is the mouse-only Quit arm at
//   gamepadCalibration.js:165-169). B returns here. Reproducing a soft-lock
//   is not faithfulness. This added exit is DEVIATION D15 (MENU-SPEC §9.2 /
//   §12.1) — NOT the D9 class, which covers only declining to reproduce
//   gameplay-menu input bugs.
//
//   Keyboard (gameMode 12, keyboardmenu.js) is genuinely stick-navigated
//   upstream, and DEVIATION D13 already rules that the port rebinds the 12
//   PHYSICAL buttons rather than 56 keyboard slots. The rebinder itself
//   (listening mode, hold-A clear, protected primaries) is the largest and
//   least load-bearing item in the whole spec and is NOT in this arc — so
//   this screen is the READ-ONLY half of it: the frozen, Chase-ratified S1
//   mapping (port/foh/keymap-frozen.txt) shown as what it is. Registered as
//   the remaining half, never dressed up as the whole.
// C30(c) (driver, 2026-07-29): the control-style and Mod-shoulder cells the
// controls lane shipped in port/gfx/ctl_style.h had NO UI AT ALL — three
// styles and a shoulder swap that no user could ever reach. They live on the
// Controls > Keyboard screen because that is the screen that already answers
// "what do my buttons do"; the Controller row stays upstream's verbatim
// no-controller error (MENU-SPEC §9.2) and gains nothing.
//
// DEVIATION, and it is a knowing one: upstream's keyboardmenu.js is a 56-item
// REBINDER (D13) that we did not build, so this screen is already ours rather
// than a port. Settable rows on it are a rewrite of a rewritten screen, not
// a drift from a faithful one. The rows follow the audio screen's idiom
// exactly (up/down picks the row, left/right cycles the value, every accepted
// step clicks) so the whole FOH stays one interaction model.
//
// A31 (DEVIATION D26, owner 2026-08-23) made the NINE ACTION ROWS settable
// too, which is what the owner asked for and what D13 always owed: "you
// should be able to rebind any of the active mappings ... currently you
// can't even go to any of those rows".
//
// L/R REBINDS, rather than an A-to-listen mode. That is the whole reason
// this is cheap AND consistent: the screen's rows are PHYSICAL BUTTONS, so
// a listening mode would have to mean "press the button you want to swap
// with", which reads backwards; L/R "cycle the value on this row" is the
// idiom every other FOH row already uses, and it is what the screen's own
// footer has always promised. Each step swaps the two rows' actions
// (ctl_style.h: the table is a PERMUTATION), so no action can be lost and
// no "protected primaries" rule is needed to keep PAUSE reachable.
//
// The Mod-shoulder row is GONE (owner: "get rid of mod altogether as an
// option here"). The CELL stays — the BOX label table reads it, the
// persisted record carries it, and A30(a) wants it — but nothing on this
// screen writes it any more except the reset row, which restores its
// ratified default. Swapping L and R is now an ordinary rebind.
//
// The VALUES are written straight through to ctl_style.c's process cells,
// not mirrored into FohState: they are read by the sim-side input path in a
// different TU, and a second copy here would be a live desync (the exact
// reason ctl_style.c is a TU and not a header — ctl_style.c's own note).
static void step_ctrl(FohState *s, const PlatformInput *in,
                      const PlatformInput *pv) {
  if (s->screen == FOH_CTRL_KEY && !(in->b && !pv->b)) {
    const bool uE = in->up && !pv->up;
    const bool dE = in->down && !pv->down;
    const bool lE = in->left && !pv->left;
    const bool rE = in->right && !pv->right;
    const bool aE = in->a && !pv->a;
    // One else-if chain, up before down, exactly like every other screen
    // (menu.js:164-242's shape): a simultaneous up+down runs UP ONLY.
    if (uE || dE) {
      s->ctlRow = (s->ctlRow + (uE ? FOH_CTL_ROWS - 1 : 1)) % FOH_CTL_ROWS;
      snd_push(s, "menuSelect");
      return;
    }
    // A is the RESET row's activator and nothing else's. Every other row on
    // this screen is an L/R row, so A there is a refusal, not a silence —
    // the same `deny` every other refusing FOH arm emits.
    if (aE) {
      if (s->ctlRow == FOH_CTL_ROW_RESET) {
        // "reset to defaults" (owner 2026-08-23) = everything THIS SCREEN
        // owns, restored to the fresh-install record: the identity binding,
        // the ratified default style, the ratified Mod arrangement. It does
        // NOT touch gameplay/audio settings — they are other screens' rows.
        ctl_bind_reset(0);
        ctl_style_set((int)CTL_STYLE_DEFAULT);
        ctl_mod_on_r_set(true); // D30: RESET must install the CURRENT default
        snd_push(s, "menuSelect");
      } else {
        snd_push(s, "deny");
      }
      return;
    }
    if (rE || lE) {
      if (s->ctlRow == FOH_CTL_ROW_STYLE) {
        // cycle the three styles, wrapping (CTL_STYLE_COUNT is the domain;
        // the enum VALUES are a frozen wire format — never renumber them,
        // FohPersist.ctlStyle stores them verbatim).
        const int n = (int)CTL_STYLE_COUNT;
        int v = (int)ctl_style_get() + (rE ? 1 : n - 1);
        ctl_style_set(((v % n) + n) % n);
      } else if (s->ctlRow >= 1 && s->ctlRow < FOH_CTL_ACTION_ROWS) {
        // THE REBIND. Row r holds physical button (r - 1); the cycle hands
        // it the next/previous logical action and swaps with whoever had it.
        ctl_bind_cycle(0, s->ctlRow - 1, rE ? 1 : -1);
      } else {
        // row 0 is the d-pad — it drives the control STICK, which is not one
        // of the eight bindable buttons — and the reset row has no value to
        // cycle. Both refuse out loud rather than eating the press.
        snd_push(s, "deny");
        return;
      }
      snd_push(s, "menuSelect");
      return;
    }
  }
  if (in->b && !pv->b) {
    // menuSelected is untouched on the way in and out, so B lands on the
    // row that opened the screen (upstream's page-3 rows are unchanged by
    // either destination).
    snd_push(s, "menuBack");
#if FOH_CTL_CHOOSER
    ev_trans(s, s->screen, FOH_MENU_CONTROLS, "b");
#else
    // DEVIATION D27 (foh.h's FOH_CTL_CHOOSER): with the chooser collapsed the
    // page this screen was opened FROM is the Options page, so that is where
    // B returns. The invariant above carries over unchanged and is what makes
    // it land on the CONTROLS row: nothing on the way in touched
    // menuSelected, so it is still 2. ONE menuBack, exactly as before and at
    // either flag value: the second `menuSelect` of a menuMODE change is
    // emitted in menu.js by the local `menuMove` boolean, and this arm is
    // keyboardmenu.js's, which never sets it.
    ev_trans(s, s->screen, FOH_MENU_OPTIONS, "b");
#endif
  }
}

void foh_tick(FohState *s, const PlatformInput *in) {
  s->nev = 0;
  s->nsnd = 0;
  const PlatformInput pv = s->prev;
  switch (s->screen) {
    case FOH_STARTUP:
      // menus/startup.js: startUpTimer==370 -> changeGamemode(0)
      s->startupTimer++;
      if (s->startupTimer == 370) ev_trans(s, FOH_STARTUP, FOH_TITLE, "timer");
      break;
    case FOH_TITLE:
      // findPlayers (main.js:385): Start joins P1 and enters the menu
      // (sounds.menuForward, main.js:388; the device app also starts
      // the MENU MUSIC on this transition — main.js:390 playMenuLoop).
      if (in->start && !pv.start) {
        snd_push(s, "menuForward");
        ev_trans(s, FOH_TITLE, FOH_MENU_TOP, "start");
      }
      break;
    case FOH_MENU_TOP:
    case FOH_MENU_OPTIONS:
    case FOH_MENU_BATTLE:
    case FOH_MENU_CONTROLS:
      step_menu(s, in, &pv);
      break;
    case FOH_CSS:
      step_css(s, in, &pv);
      break;
    case FOH_SSS:
      step_sss(s, in, &pv);
      break;
    case FOH_OPT_GAMEPLAY:
      step_opt_gameplay(s, in, &pv);
      break;
    case FOH_OPT_AUDIO:
      step_opt_audio(s, in, &pv);
      break;
    case FOH_CTRL_PAD:
    case FOH_CTRL_KEY:
      step_ctrl(s, in, &pv);
      break;
    case FOH_CREDITS:
      step_credits(s, in, &pv);
      break;
    case FOH_TSS:
      step_tss(s, in, &pv);
      break;
    // A45 T4. The engine is a separate TU behind a pointer (foh_tbuild.h).
    // With it linked, step() drives the editor and returns a verdict; the
    // TRANSITION is issued here, because ev_trans/snd_push are this file's.
    // WITHOUT it linked the screen is still reachable and still leaves on B
    // — render_tbuild draws the unavailable notice on screen, so the refusal
    // is READABLE rather than a button that appears to do nothing.
    case FOH_TBUILD: {
      FohTbVerdict v = FOH_TB_STAY;
      if (foh_tbuild_ops) {
        v = foh_tbuild_ops->step(s, in, &pv);
      } else if (in->b && !pv.b) {
        snd_push(s, "menuBack");
        v = FOH_TB_QUIT;
      }
      if (v == FOH_TB_QUIT) {
        // targetbuilder.js:833-835 — Quit is changeGamemode(1), i.e. the
        // menu, however the builder was entered. menuSelected is untouched,
        // so it lands back on the TARGET BUILDER row that opened it.
        ev_trans(s, FOH_TBUILD, FOH_MENU_TOP, "b");
      }
      break;
    }
    case FOH_MATCH:
    case FOH_TMATCH:
      // terminal for the FOH machine; the driver owns the sim from here
      break;
    default: gfx_fatal("foh: tick on an invalid screen");
  }
  s->prev = *in;
  // readyToFight's DRAW-PASS recompute (css.js:1167-1181). Upstream computes
  // it inside drawCSS, and drawCSS runs from a SEPARATE rAF loop that
  // dispatches on the CURRENT gameMode (main.js:1153-1183 at the pin) — so it
  // belongs to the screen the tick ENDS on, not the one it started on:
  //   * CSS -> SSS (launch): the mode is 6 by draw time, drawCSS does not run,
  //     and readyToFight keeps its stale value through the whole SSS.
  //   * SSS -> CSS (B back, stageselect.js:79): the mode is 2 by draw time, so
  //     drawCSS DOES run on the return frame and readiness is recomputed
  //     BEFORE the next control pass. A token still held across the round trip
  //     therefore un-readies the screen, and START cannot relaunch.
  //   * CSS -> menu (B-hold): the mode is 1, so no recompute at all.
  // Keeping this inside step_css got the first and third right and the second
  // WRONG (review round 4/5); gating on the FINAL screen is the whole rule.
  if (s->screen == FOH_CSS) s->cssReady = css_ready(s);
  // LOOK plane only (A1 restyle Phase 0) — advanced after navigation has
  // settled so the hue chases the NEW selection. Reads/writes nothing the
  // flow graph, the event list or the launch record can observe.
  foh_anim_tick(s);
}
