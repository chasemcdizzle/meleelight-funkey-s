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
// press); SSS pointer drag -> a 3x2 grid cursor over the 6 VS stages
// (ids == oracle --stage ids) plus the RANDOM slot at cursor 6 —
// VISIBLE but REFUSING (registered exclusion, MEASURED iter 93:
// upstream's A-on-RANDOM arm draws `Math.floor(Math.random() * ...)`,
// stageselect.js:80-84, and Math.random IS the seeded oracle stream —
// a live RANDOM draw would desync live-vs-replay stream prefixes; A on
// slot 6 emits `S <f> refused random`, never a launch). CPU difficulty
// domain is the UPSTREAM SLIDER's 1..4, default 3 (css.js:316-329:
// Math.round((x-off)*3/166)+1 over a 166px-clamped travel;
// cpuDifficulty defaults [3,3,3,3], main.js:109) — NOT the harness's
// 1-9.
//
// CSS MECHANICS (docs/MENU-SPEC.md §2, implementation items 1+2+3+4;
// this REPLACES the iter-88 row-cursor rewrite delta, which the owner
// rejected as unfaithful — 2026-07-27 ruling, MENU-SPEC preamble). The
// CSS is now upstream's own gesture, re-laid-out for 240x240:
//   * a FREE 2D hand cursor in doubles, integrated from the d-pad every
//     frame and clamped to the screen (css.js:195-206; DEVIATION D1 —
//     the d-pad supplies lsX/lsY in {-1,0,+1}; D3 — 12 px/frame on
//     1200x750 is 1.00%/1.60% of the screen, i.e. 2.40/3.84 px/frame
//     here, behind the FOH_CURSOR_SPEED knob below).
//   * the TOKEN GESTURE as the ONLY way to pick a character (there is
//     no value stepper anywhere in css.js): B retrieves YOUR OWN token
//     to the hand inside the roster band (css.js:209-215, guarded
//     playerType[i]==0); A on a token you may take — your own, or any
//     port set to CPU (css.js:298-312, guard playerType[j]==1 || i==j)
//     — grabs it; while carried the token rides the hand and HOVERING a
//     character cell selects it LIVE (css.js:222-226); A drops it and
//     plays that character's announcer (css.js:233/248/263/278/293);
//     leaving the roster band SILENTLY COMMITS whatever was last
//     hovered (css.js:336-347 — there is no invalid-drop rejection).
//   * the port-type box is a CLICKABLE WIDGET (css.js:348-357):
//     cursor over port j's type tab + A runs togglePort(j)
//     (main.js:504-520). DEVIATION D5 narrows the cycle to
//     N/A(-1) -> HMN(0) -> CPU(1) -> N/A (NET is scope-excluded).
//     P1 MAY BE CPU — main.js:504-520 has no port-0 special case — so
//     the machine carries p1Type/p1Difficulty next to p2Type/difficulty.
//   * the CPU level is a GRAB-DRAG KNOB, not a stepper (css.js:316-334,
//     396-408; DEVIATION D7), domain 1..4.
//   * READY TO FIGHT is computed from >=2 non-N/A ports AND no token
//     currently held (css.js:1167-1181), one frame LATE because
//     upstream computes it in the DRAW pass. That pass belongs to the
//     screen a tick ENDS on — drawCSS runs from a separate rAF loop
//     dispatching on the CURRENT gameMode (main.js:1153-1183) — so
//     cssReady is recomputed in foh_tick after any tick whose FINAL
//     screen is the CSS: not on the launch frame, not on the B-hold exit
//     frame, but YES on the SSS->CSS return frame, where a still-held
//     token un-readies the screen. START launches ONLY while ready
//     (css.js:446-451);
//     the d-pad-up and d-pad-right arms (css.js:452-459) are dropped by
//     DEVIATION D2.
// PORT MODEL — a LOCAL DEVIATION, not an upstream state, and it needs an
// owner ruling. It is a HYBRID of upstream's two attachment cases and
// matches neither exactly:
//   * upstream with ports==1 gives ONE hand (main.js:949-954 gates on
//     i < ports) and makes port 2 HMN unreachable, because togglePort's
//     `playerType[i] == 0 && ports <= i` arm skips straight to CPU
//     (main.js:512-519);
//   * upstream with ports==2 gives TWO hands and starts port 2 at HMN.
// We take ONE hand (there is one physical input device) but treat ports
// 0 and 1 as ATTACHED for the type plane, so p2Type==0 (HMN) stays
// reachable — without it the two-HUMAN oracle goldens (g01, g03) could
// not be launched from the menu at all, and the frozen full-stream
// bridges would have no faithful path. togglePort's `ports <= i` arm is
// therefore inert here. CONSEQUENCE, and it is visible to the player:
// with one hand and upstream's ownership guard, another HUMAN port's
// token cannot be taken, so setting P2's character means putting P2 on
// CPU, taking its token, and walking the type box back round to HMN.
// Every step of that is upstream-legal (togglePort never touches
// chosenChar), but the sequence is ours. DEVIATION D6: ports 3 and 4 RENDER (as
// upstream's N-A panels) but their type tabs do not toggle and they own
// no token, because every frozen golden and the whole check-sim.sh
// conformance surface is 2-player. Consequence of one hand + upstream's
// ownership guard: another HUMAN port's token cannot be taken, so P2's
// character is set by putting P2 on CPU, taking its token, and (if a
// human P2 is wanted) toggling the type back — every step upstream-legal.
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

// --- CSS layout + hit geometry (MENU-SPEC DEVIATION D4) ----------------------
// D4: "the semantic widget table in each section is normative; the rectangle
// for each widget is whatever our renderer draws for it. Every widget must be
// hit-testable at its drawn extent, and there must be no hit region without a
// drawn widget (and vice versa)." Upstream's literal rects (the marth cell at
// x in [398,493] of 1200) would put hit regions where nothing is drawn.
//
// These constants are the SINGLE source for BOTH foh_render.c's draws and
// foh.c's hit tests. Keeping one copy is what makes D4 mechanically true
// instead of a comment two files apart could silently break.
//
// D4 EXCEPTIONS, registered rather than glossed — this build does NOT satisfy
// the invariant everywhere, and pretending otherwise would be the lie:
//   (a) DRAWN, NOT YET HIT-TESTABLE: the header's BACK wedge and mode ribbon
//       (foh_render.c css_header). Both are real upstream widgets
//       (css.js:358-363 and :390-395) and both belong to MENU-SPEC item 7,
//       which this arc does not carry. They are left drawn because the design
//       is owner-approved and must not be restyled; the hit arms land with
//       item 7.
//   (b) HIT-TESTABLE, NOT ALWAYS DRAWN: your OWN token stays grabbable while
//       your port is N/A, because upstream's grab guard is
//       `playerType[j] == 1 || i == j` (css.js:300) while its token DRAW is
//       guarded on `playerType[i] > -1` (css.js:1018/1077). That asymmetry is
//       upstream's; it is carried verbatim, not repaired.
#define FOH_CSS_CELL_W 44
#define FOH_CSS_CELL_H 30
#define FOH_CSS_CELL_PITCH 46
#define FOH_CSS_CELL_X0 6
#define FOH_CSS_CELL_Y 32
#define FOH_CSS_PANEL_W 58
#define FOH_CSS_PANEL_PITCH 60
#define FOH_CSS_PANEL_X0 1
#define FOH_CSS_PANEL_Y 96
#define FOH_CSS_PANEL_H 120
#define FOH_CSS_TAB_H 11
#define FOH_CSS_TAB_W 36
// The roster band (css.js:207 — `y < 400 && y > 160`): here it is the strip
// strictly between the silver header (0..25) and the READY ribbon (62..92),
// and it contains exactly the five character cells, the same containment
// upstream has (cells 240..335 inside the band 160..400).
#define FOH_CSS_BAND_TOP 26
#define FOH_CSS_BAND_BOT 62
// Token rest slots: two ports stack inside one cell (upstream stacks four
// 2x2 within a 95 px cell; two fit a 44 px one).
#define FOH_CSS_TOKEN_R 9
#define FOH_CSS_TOKEN_DX 12
#define FOH_CSS_TOKEN_PITCH 20
#define FOH_CSS_TOKEN_Y (FOH_CSS_CELL_Y + 11)
// QUIRK Q1, carried (MENU-SPEC §2.6 "carried verbatim"). Upstream has TWO
// rest formulas and they disagree: the A-drop slot is `419 + 95c + 40k` and
// the leave-band slot is `518 + 93c + 40k` (css.js:288 vs :337). MENU-SPEC
// calls that "a few px"; MEASURED it is a 99 px base offset on a 95 px cell
// pitch, i.e. a leave-band token comes to rest on the NEXT character's cell,
// and the pitch differs by 2 px per cell as well. Mapped onto this layout at
// the same ratios (99/95 of a cell = +48 px here; -2/95 of a cell = -1 px per
// cell), the quirk survives as itself: leave the band and your token rests
// one cell to the right of the character it selected. Faithfulness > tidiness
// — the spec's magnitude claim is wrong, its instruction is not.
#define FOH_CSS_TOKEN_LB_DX 48
#define FOH_CSS_TOKEN_LB_PITCH (FOH_CSS_CELL_PITCH - 1)
// The CPU-level rail, panel-relative. 3 steps of 12 px == the drawn track,
// so difficulty 1..4 lands the knob exactly on the rail's two ends and its
// two interior stops (upstream: a 166 px continuous travel, css.js:325-327).
#define FOH_CSS_RAIL_X0 11
#define FOH_CSS_RAIL_STEP 12
#define FOH_CSS_RAIL_LEN (FOH_CSS_RAIL_STEP * 3)
#define FOH_CSS_RAIL_Y 93
#define FOH_CSS_KNOB_R 6
// css.js:317 parks the dragging hand at cpuSlider.y + 15 — below the knob
// centre, since the hot spot is the fingertip. 15/750 of the canvas = 4.8 px
// here. It stays inside the knob's +/-6 box, so a release-then-regrab still
// hits (upstream has the same property with its +/-25 box).
#define FOH_CSS_DRAG_DY 4.8

static inline int foh_css_cell_x(int k) {
  return FOH_CSS_CELL_X0 + FOH_CSS_CELL_PITCH * k;
}
static inline int foh_css_panel_x(int k) {
  return FOH_CSS_PANEL_X0 + FOH_CSS_PANEL_PITCH * k;
}

// DEVIATION D3 — cursor speed as a fraction of the screen, plus the one
// calibration knob in this whole spec. Upstream moves the CSS hand 12
// px/frame on a 1200x750 canvas (css.js:195-196) = 1.00% of width and 1.60%
// of height per frame; at 240x240 that is 2.40 / 3.84 px/frame, which keeps
// upstream's feel exactly (full-width traversal stays 100 frames, full-height
// 62.5). Hardware feel cannot be judged from source, so the owner tunes ONE
// number here rather than us guessing a curve.
#define FOH_CURSOR_SPEED 1.0
#define FOH_CURSOR_VX (2.40 * FOH_CURSOR_SPEED)
#define FOH_CURSOR_VY (3.84 * FOH_CURSOR_SPEED)

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
  // css — MACHINE state (B3: every field below belongs in a captured shot;
  // the LOOK plane lives in its own block further down and is canonicalised
  // by foh_look_canonical, which must never touch these).
  //
  // The free hand cursor (css.js:64 handPos, DOUBLES, never integers —
  // rounded only at draw time; module scope upstream, so it PERSISTS across
  // CSS entry/exit and is initialised exactly once, in foh_init).
  double cssHandX, cssHandY;
  // whichTokenGrabbed[0] (css.js:68): -1 = carrying nothing, else the port
  // whose token this hand holds. With one hand, upstream's tokenGrabbed[k]
  // and occupiedToken[k] are both exactly `cssCarry == k`, so they are
  // derived rather than stored.
  int cssCarry;
  // whichCpuGrabbed[0] (css.js:75); cpuGrabbed[0] is `cssCpuCarry >= 0`.
  int cssCpuCarry;
  // cpuSlider[k].x (css.js:72), CONTINUOUS: upstream writes the raw hand x
  // into it every drag frame (css.js:324) and the knob keeps that exact
  // position after release, so a re-grab hit-tests where the knob actually
  // is — deriving it back from the integer level would snap it to 4 stops
  // and move the re-grab target.
  double cssSliderX[2];
  // handType[0] (css.js:63): 0 handPoint, 1 handOpen, 2 handGrab. STORED, not
  // derived, because upstream's assignments are path-dependent: an A-drop
  // clears whichTokenGrabbed but leaves handType at 2 for that draw.
  int cssHandType;
  // Which rest slot each port's token last came to: 0 = the A-drop slot
  // (css.js:227-233 class), 1 = the leave-band slot (css.js:337). Upstream's
  // two formulas genuinely disagree, so the resting position is PATH
  // DEPENDENT — see the note at the leave-band arm in foh.c.
  int cssTokenRest[2];
  // readyToFight (css.js:78). Upstream computes it in the DRAW pass
  // (css.js:1167-1181), one frame after the controls that read it, and that
  // pass belongs to the screen the tick ENDS on — so foh_tick recomputes it
  // after any tick whose FINAL screen is the CSS, NOT at the end of the CSS
  // step (which missed the SSS->CSS return and wrongly ran on the B-hold
  // exit). Full derivation at the recompute site in foh.c.
  bool cssReady;
  // characterSelections[0..1] (main.js:59) — the SHARED selection plane: what
  // the match launches with, what the port panels preview, and what the
  // target-select screen's shoulder arms write (targetselect.js:60-69 calls
  // setCS directly).
  int p1Char, p2Char; // 0 marth 1 puff 2 fox 3 falco 4 falcon
  // chosenChar[0..1] (css.js:66) — the CSS's OWN plane, written only by this
  // screen's hover arm, which assigns `chosenChar[k] = c` INLINE and then
  // calls changeCharacter (css.js:222-226; changeCharacter itself, :165-172,
  // only does setCS + the preview doll — it never touches chosenChar). It is
  // what the TOKEN rest position and the port panel's NAME PLATE (css.js:986)
  // are computed from, while the panel PORTRAIT reads the shared plane
  // (css.js:889). Keeping it separate matters: target-select's L/R moves
  // characterSelections without touching chosenChar upstream, so coming back
  // to the CSS afterwards must leave the token where the player left it, not
  // teleport it to the target-test character.
  int cssChar[2];
  // playerType[0..1] (main.js:107): -1 N/A, 0 HMN, 1 CPU. NET (2) is
  // DEVIATION D5; ports 2/3 are pinned N/A by D6 and have no field.
  int p1Type, p2Type;
  // cpuDifficulty[0..1] (main.js:109), 1..4 (slider domain), default 3.
  // `difficulty` is port 1's and keeps its name: it is the field the launch
  // bridge, foh_dev.c and every frozen LAUNCH line already mean by it.
  int p1Difficulty;
  int difficulty;
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

// foh.c: the CSS geometry the renderer must draw at, derived from the same
// machine state the hit tests read (D4's "no hit region without a drawn
// widget, and vice versa" is only true if both sides call these).
//   hand sprite: 0 = handPoint, 1 = handOpen, 2 = handGrab (css.js:1135-1143,
//   selected by band + carry exactly as css.js:208/210/217/304/332/406 do).
int foh_css_hand_type(const FohState *s);
// Token k's position: the hand while carried, else the rest slot its LAST
// DROP PATH puts it in, on the cell of the character port k has chosen.
void foh_css_token_pos(const FohState *s, int k, double *x, double *y);
// The knob's drawn centre for port k, from the continuous slider position.
double foh_css_knob_x(const FohState *s, int k);
double foh_css_knob_y(void);
// Port k's type (-1/0/1) and CPU level (1..4); k >= 2 is D6's pinned N/A.
int foh_css_port_type(const FohState *s, int k);
int foh_css_port_diff(const FohState *s, int k);

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
