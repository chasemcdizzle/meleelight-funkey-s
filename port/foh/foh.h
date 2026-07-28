// port/foh/foh.h — the front-of-house screen machine (fix_plan §M4 task 9;
// pre-registration AGENT-LOG iter 88).
//
// REWRITTEN, not transliterated (§M4 conventions: upstream menus are
// jQuery+DOM+canvas hybrids in absolute 1200x750 px — anatomy §8): a C
// state machine over the EXISTING platform seam + raster, carrying
// upstream's FLOW GRAPH and SELECTION SEMANTICS faithfully while the
// look/navigation are rewritten for the 240x240 d-pad device. Menus are
// NOT checksummed; the match-launch bridge IS (foh_app.c).
//
// FLOW GRAPH (every edge cited from the upstream primary source; gameMode
// is written ONLY via changeGamemode(N), main.js:554-629):
//   FOH screen        upstream mode      entered by
//   startup           gameMode 20        boot
//   title             gameMode 0         startUpTimer==370 (startup.js:50)
//   menu-top          gameMode 1 mm 0    START on title (findPlayers,
//                                        main.js:385; addPlayer = P1 join)
//   menu-battle       gameMode 1 mm 2    A on "VS. Melee" (menu.js:73-75)
//   menu-options      gameMode 1 mm 1    A on "Options" (menu.js:92-97)
//   menu-controls     gameMode 1 mm 3    A on "Keyboard Controls"
//                                        (menu.js:138-141)
//   css               gameMode 2         A on "Local VS" (menu.js:105)
//   sss               gameMode 6         START in CSS while readyToFight
//                                        (css.js:446-451); B held 30
//                                        frames backs to menu (css.js:
//                                        186-191)
//   options-gameplay  gameMode 11        A on "Gameplay" (menu.js:135)
//   match             gameMode 3         A in SSS = setStageSelect +
//                                        startGame (stageselect.js:80-88,
//                                        main.js:1320-1370)
//   target-select     gameMode 7         A on "Target Test" (menu.js:
//                                        77-84: setTargetPlayer +
//                                        pointer reset + stop menu music
//                                        + playTargetTestLoop +
//                                        changeGamemode(7)); B backs to
//                                        menu-top with menuSelected
//                                        UNTOUCHED (still TARGETTEST=1
//                                        — module state, measured) +
//                                        playMenuLoop (targetselect.js:
//                                        76-81)
//   target-match      gameMode 5         A or START on an authored slot
//                                        (targetselect.js:131-146:
//                                        menuForward +
//                                        setActiveStageTarget +
//                                        setTargetStagePlaying +
//                                        startTargetGame(i, false));
//                                        terminal like `match` — the
//                                        driver owns the target sim.
//                                        The finishGame seam
//                                        (main.js:988-990 -> 1420-1423)
//                                        is SIM-plane (target_play.c
//                                        tp_finish_hook); the driver's
//                                        end banner is render-plane
//                                        presentation, NOT a flow edge
//                                        (no committed flow can reach
//                                        all-broken: measured, AGENT-LOG
//                                        iter 99 — the finish probe owns
//                                        mechanical coverage).
// Menu B-backs verbatim (menu.js:164-190): controls -> options cursor 0
// (AUDIOOPTIONS), options -> top cursor 3 (OPTIONS), battle -> top
// cursor 0 (VSMODE); B at top level does nothing. options-gameplay B ->
// menu-options cursor 1 (gameplaymenu.js:25-36; its cookie save is task
// 13's persistence surface — registered).
//
// REWRITE DELTAS (pre-registered, AGENT-LOG iter 88): stick-threshold
// navigation with 10-frame autorepeat -> d-pad EDGES (one step per
// press); CSS hand-cursor/token drag -> a row cursor over {P1 char,
// P2 char, P2 type, P2 difficulty} with L/R value steps (CLAMP at the
// ends); SSS pointer drag -> a 3x2 grid cursor over the 6 VS stages
// (ids == oracle --stage ids) plus the RANDOM slot at cursor 6 —
// VISIBLE but REFUSING (registered exclusion, MEASURED iter 93:
// upstream's A-on-RANDOM arm draws `Math.floor(Math.random() * ...)`,
// stageselect.js:80-84, and Math.random IS the seeded oracle stream —
// a live RANDOM draw would desync live-vs-replay stream prefixes; A on
// slot 6 emits `S <f> refused random`, never a launch). P2 type
// toggles HMN(0) <-> CPU(1) on A (togglePort main.js:510-526 cycles
// -1 -> 0 -> 1 -> 2(network) -> -1; the network arm is scope-excluded
// and -1 would break readyToFight>=2 on a one-input device — domain
// narrowed to {0,1}, registered). CPU difficulty domain is the UPSTREAM
// SLIDER's 1..4, default 3 (css.js:316-329: Math.round((x-off)*3/166)+1
// over a 166px-clamped travel; cpuDifficulty defaults [3,3,3,3],
// main.js:109) — NOT the harness's 1-9.
//
// Menu entries whose screens are excluded/deferred stay VISIBLE with
// their faithful labels (menu.js:19-24) and selecting one emits a
// structural `refused` event — loud and frozen in the flow traces,
// never silence: targetbuilder / credits (conventions scope
// exclusions), audio (mixer volume surface, tasks 10/13),
// controller/keyboard (the S1 mapping is Chase-ratified hardware
// surface), spectate/p2p/server (multiplayer excluded; P2P is dead
// upstream, menu.js:113-116), addcode (the target-select "+ Add Code"
// slot — builder/share-code plane, scope-excluded; customTargetStages
// is EMPTY in the fresh domain so the authored 10 slots + Add Code are
// exactly what upstream shows, targetselect.js:47/:133-140). The
// `targettest` refusal RETIRED in iter 99 — target-select is real.
//
// TARGET-SELECT REWRITE DELTAS (iter 99, task 12): the upstream
// pointer-drag slot picker (targetselect.js:45-57, 250x50 boxes at
// col = floor(j/5), row = j%5) becomes a d-pad GRID CURSOR over the 10
// authored slots (2 cols x 5 rows, same col/row mapping) with the
// addcode slot below (D from a bottom row enters it, U returns —
// the SSS RANDOM-slot pattern); char select keeps the upstream SHOULDER
// arms verbatim (targetselect.js:60-74: L = char-1 WRAP, R = char+1
// WRAP — the du/dd d-pad arms are the same actions and the d-pad drives
// the grid here) writing characterSelections[0] == p1Char (setCS on the
// SAME array — S events keep the p1char token; WRAP on this screen,
// cited, vs the CSS rewrite's clamp). A or START launches
// (targetselect.js:131 accepts either). The trace records
// `TLAUNCH <f> char=<0-4> tstage=<0-9>`. Records HUD (task 13, iter
// 100 — the iter-99 READ deferral closed): targetRecords[5][10] lives
// in FohState (fresh-boot -1 per targetplay.js:40 -> "--:--:--",
// targetselect.js:411-412; else the upstream time format,
// targetselect.js:415-419), populated at boot by the drivers through
// the foh_persist chokepoint (foh_persist.h — the machine itself
// stays I/O-free); the addcode slot (cursor 10) keeps the dashes
// (upstream shows no PB there — registered rewrite delta).
// medalTimes/devRecords display DEFERRED (authored data values — HARD
// RULE 5 forbids hand-retyping; needs a pipeline extension; registered
// AGENT-LOG iter 99; medals are DERIVED from records upstream —
// giveMedals, targetplay.js:165-174 — so nothing is lost in the
// persisted plane).
//
// INPUT SEAM: foh_tick consumes PlatformInput. On the host check the
// rows come from a committed FLOW script (foh_app.c); on device (task
// 10) they come from platform_poll — same machine, different feeder.
// The machine consumes NO RNG and NO wall clock by construction (shot
// byte-stability x2 depends on it).
#ifndef FOH_FOH_H
#define FOH_FOH_H

#include <stdbool.h>
#include <stdint.h>

#include "../gfx/platform.h"
#include "../gfx/raster.h"

typedef enum {
  FOH_STARTUP = 0,
  FOH_TITLE,
  FOH_MENU_TOP,
  FOH_MENU_OPTIONS,  // upstream menuMode SECONDLEVELOPTIONS = 1
  FOH_MENU_BATTLE,   // upstream menuMode MPMENU = 2
  FOH_MENU_CONTROLS, // upstream menuMode CONTROLLERCALIB = 3
  FOH_CSS,
  FOH_SSS,
  FOH_OPT_GAMEPLAY,
  FOH_MATCH,
  FOH_TSS,    // target-select (upstream gameMode 7, targetselect.js)
  FOH_TMATCH, // target-match (terminal; the driver owns the target sim)
  FOH_SCREEN_COUNT
} FohScreen;

// One structural event, drained by the driver after each tick.
typedef enum { FOH_EV_TRANS = 0, FOH_EV_SEL, FOH_EV_LAUNCH } FohEvKind;

typedef struct {
  FohEvKind kind;
  // TRANS: from/to = screen tokens, cause = "timer"/"start"/"a"/"b"/
  // "bhold"/"launch". SEL: field token + int value, or field "refused"
  // with sval = the entry token. LAUNCH: snapshot read via FohState.
  const char *from, *to, *cause;
  const char *field, *sval;
  int val;
} FohEvent;

#define FOH_EV_CAP 8

typedef struct {
  FohScreen screen;
  // startup (upstream menus/startup.js): timer to 370
  int startupTimer;
  // menu (upstream menus/menu.js): cursor + counts per menuMode table
  int menuSelected;
  // css
  int cssRow; // 0 p1 char, 1 p2 char, 2 p2 type, 3 p2 difficulty
  int p1Char, p2Char; // 0 marth 1 puff 2 fox 3 falco 4 falcon
  int p2Type;         // 0 human, 1 cpu
  int difficulty;     // 1..4 (slider domain), default 3
  int bHold;          // consecutive B frames in CSS (30 = back)
  // sss
  int sssCursor; // 0..5 == oracle stage ids; 6 = the refusing RANDOM slot
  // target-select (upstream targetselect.js; header notes): cursor
  // 0..9 == tstage ids (targetStageMapping order), 10 = the refusing
  // addcode slot; the char plane is the SHARED p1Char
  // (characterSelections[0] — setCS writes the same array upstream)
  int tssCursor;
  // options-gameplay (sim-consumed subset; settings.js:44-56 defaults)
  int optRow; // 0 turbo, 1 lCancelType, 2 tapJumpOff columns
  int optCol; // 0..3 (active on the tapJumpOff row)
  int turbo;
  int lCancelType;
  int tapJumpOff[4];
  // target-test records DISPLAY plane (task 13): seconds, -1 = none
  // (upstream fresh state, targetplay.js:40). foh_init sets -1; the
  // drivers overwrite from the foh_persist chokepoint at boot
  // (foh_persist_apply). The machine only READS it (render_tss).
  double targetRecords[5][10];
  // launch record (frozen once screen == FOH_MATCH / FOH_TMATCH):
  // targetMode false -> VS launch (stageSel); true -> target launch
  // (tssStage; char == p1Char)
  int stageSel;
  int tssStage;
  bool targetMode;
  bool launched;
  // --- LOOK / ANIMATION PLANE (M4 A1 restyle, Phase 0) -------------------
  // The FOH was static: foh_render is a pure function of FohState and the
  // shot byte-stability x2 arm depends on that staying true. Upstream's
  // menus animate off frame counters (menuGlobalTimer/menuTimer/menuCycle,
  // menu.js:37-40; angB/angR/mlPos, startscreen.js:5-10), so the port needs
  // a counter too — and it must be a COUNTER, never a clock: `frame` is the
  // tick index, so two runs of the same flow render the same bytes. These
  // fields are advanced by foh_anim_tick (below) and READ by foh_render;
  // no flow edge, selection, or event ever reads them.
  int frame;          // ticks since foh_init (grid shine, rays, wordmark bob)
  double menuHue;     // menu.js menuCurColour (init 238, menu.js:35)
  double menuHueOff;  // menu.js menuColourOffset (menu.js:33)
  double menuColours[4]; // menu.js:34 — MUTABLE, see menu.js:243-252
  int menuTimer;      // menu.js menuTimer, 0..60 (ring pulse phase)
  int menuCycle;      // menu.js menuCycle, 0/1 (second ring on/off)
  int menuPrevSel;    // previous menuSelected — drives the reset-on-move arm
  int menuPrevScreen; // previous screen — a B-back can land on the SAME row
                      // number (battle 0 -> top 0), which upstream still
                      // treats as a menuMove (menu.js:233-235)
  // edge detection
  PlatformInput prev;
  // events emitted by the last tick
  FohEvent ev[FOH_EV_CAP];
  int nev;
  // menu SFX tokens emitted by the last tick (M4 task 10; SND1 Howl
  // names, upstream mapping cited at the emission sites in foh.c —
  // menuSelect/menuForward/menuBack/deny). NOT part of the structural
  // trace (frozen .expects unchanged); consumed by the device app's
  // mixer seam and ignored by the host trace driver.
  const char *snd[FOH_EV_CAP];
  int nsnd;
} FohState;

void foh_init(FohState *s);
void foh_tick(FohState *s, const PlatformInput *in);
const char *foh_screen_token(FohScreen sc);

// foh_render.c: draw the current screen into rz (full-frame clear+draw;
// deterministic — no RNG, no clock).
void foh_render(const FohState *s, Raster *rz);

// foh_render.c: advance the LOOK plane one tick (menu hue lerp, ring pulse,
// global frame counter). Called once per foh_tick, AFTER navigation has
// settled, so the hue chases the new selection. Touches only the fields in
// the "look / animation plane" block above — it can never change a flow
// edge, an event, or a launch record.
void foh_anim_tick(FohState *s);
// Puts the LOOK plane at its resting phase on a COPY of the state, so a
// captured shot is a pure function of the MACHINE state on every target
// (the device's tick number at a q-marker shot is a wall-clock artefact —
// full derivation at the definition in foh_render.c). Shot paths only.
void foh_look_canonical(FohState *s);
// Builds the backdrop caches off the frame budget (call once, before any
// paced loop). Bit-identical to letting them build lazily — see the
// definition in foh_render.c.
void foh_render_warm(Raster *rz);

// foh_font.c: self-authored 5x7 font (scale = integer pixel multiplier).
void foh_text(Raster *rz, int x, int y, int scale, const char *s,
              RastCol col);
int foh_text_width(const char *s, int scale);

// foh_font.c: the self-authored 6x9 heavy display face; italic != 0 shears
// it (synthetic oblique). Advance 7 px per glyph at scale 1.
void foh_text2(Raster *rz, int x, int y, int scale, int italic,
               const char *s, RastCol col);
int foh_text2_width(const char *s, int scale);

#endif // FOH_FOH_H
