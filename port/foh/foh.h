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
//                                        — UNREACHABLE at FOH_NETPLAY 0
//                                        (owner ruling C5, flag below)
//   menu-options      gameMode 1 mm 1    A on "Options" (menu.js:92-97)
//   menu-controls     gameMode 1 mm 3    A on "Keyboard Controls"
//                                        (menu.js:138-141) — drawn "CONTROLS"
//                                        (DEVIATION D25) — UNREACHABLE at
//                                        FOH_CTL_CHOOSER 0 (DEVIATION D27,
//                                        flag below)
//   options-audio     gameMode 10        A on "Audio" (menu.js:130)
//   controls-controller gameMode 14      A on "Controller" (menu.js:155-157)
//                                        — D25: now the SECOND row —
//                                        UNREACHABLE at FOH_CTL_CHOOSER 0
//                                        (D27: no USB host mode on this OS)
//   controls-keyboard gameMode 12        A on "Keyboard" (menu.js:159-161)
//                                        — D25: now the FIRST row, drawn
//                                        "HANDHELD" (it is this device's own
//                                        buttons, not a keyboard). The TOKENS
//                                        and gameModes above are identity and
//                                        do not move; only the row order and
//                                        the painted labels did. At
//                                        FOH_CTL_CHOOSER 0 the Options row
//                                        opens THIS screen directly (D27).
//   css               gameMode 2         A on "Local VS" (menu.js:105);
//                                        at FOH_NETPLAY 0 that same action
//                                        runs from "VS. Melee" directly
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
// chosenChar), but the sequence is ours.
//
// DEVIATION D6 IS RETIRED (A44, 2026-08-24, owner-reported: "why can't I
// turn on player 3 and 4 at the CSS?"). It said ports 3 and 4 RENDER as
// upstream's N-A panels but own no type field and no token, and the reason
// it gave was never the pixels — it was that sim_setup_match hard-pinned
// slots 2/3 to playerType=-1. **A46 removed that**: sim_setup_match_ports
// (port/sim/sim/sim_boot.c) is upstream's own four-port harnessSetupMatch
// loop, verified by the 4-port golden q01 (STREAM MATCH 3600/3600). So the
// obstacle D6 named is gone, and D6 with it. Two NEW deviations take its
// place, and they are narrower:
//
// DEVIATION D40 — ONE HAND OWNS EVERY TOKEN, and ports 2/3 offer HMN or
// N/A only. Two halves of one ruling:
//   (a) upstream's grab guard is `playerType[j] == 1 || i == j` (css.js:300)
//       — hand i may take its own token or any CPU port's, never another
//       HUMAN's, because upstream has FOUR hands and each human works its
//       own. This device has ONE physical input device and therefore ONE
//       hand (the PORT MODEL above), so that guard does not protect another
//       player's agency here; it removes the only way to choose a
//       character for a human port. Today that is survivable for P2 via the
//       CPU detour described above. For P3/P4 it is NOT: with CPU absent
//       from their cycle (b) there is no detour at all, so a P3 you can
//       switch on but never give a character to is precisely the stub HARD
//       RULE 2 forbids. The one hand may therefore grab ANY port's token.
//       Nothing else about the grab changes; the guard is the deviation.
//   (b) ports 2/3 cycle N/A -> HMN -> N/A. CPU is not offered because the
//       sim refuses a second AI slot (A46's OPEN/OWED: AIBRIDGE1 is one
//       recorded stream for one CPU slot). Absent beats denied-at-START.
// CONSEQUENCE, stated because a player will see it: a HMN port with no
// physical controller stands still. That is already true of P2 today —
// the PORT MODEL note above treats port 1 as ATTACHED so the two-human
// goldens can be launched — and A33's second-controller spike is what
// closes it, not this screen.
//
// DEVIATION D41 — the token 2x2. Four tokens on one 44 px cell need
// upstream's own 2x2 stack rather than the single row two ports read as.
// The four inequalities that fix r/pitch/DX/Y are at FOH_CSS_TOKEN_R.
// It is registered because the numbers moved, not because the shape did:
// the 2x2 is what the constant block always said upstream draws.
//
// Menu entries whose screens are excluded/deferred stay VISIBLE with
// their faithful labels (menu.js:19-24) and selecting one emits a
// structural `refused` event — loud and frozen in the flow traces,
// never silence. LIVE refusals at FOH_NETPLAY 0: targetbuilder
// (conventions scope exclusion), credits (upstream credits.js is a
// 422-line shooting gallery — MENU-SPEC §8/D12; the LAST unbuilt
// screen, registered), random (SSS), addcode (target-select),
// portconfig (CSS launch plane). RETIRED this arc: audio (real screen
// now, MENU-SPEC §4), controller/keyboard (real destinations now,
// MENU-SPEC §9). UNREACHABLE-but-registered at FOH_NETPLAY 0:
// spectate/p2p/server (multiplayer excluded; P2P is dead upstream,
// menu.js:113-116) — their arms are kept whole behind the flag.
// addcode is the target-select "+ Add Code"
// slot — builder/share-code plane, scope-excluded; customTargetStages
// is EMPTY in the fresh domain so the authored 10 slots + Add Code are
// exactly what upstream shows, targetselect.js:47/:133-140). The
// `targettest` refusal RETIRED in iter 99 — target-select is real.
//
// TARGET-SELECT REWRITE DELTAS (iter 99, task 12; the cursor half
// SUPERSEDED by DEVIATION D29, A25(c) — see below): the upstream
// pointer-drag slot picker (targetselect.js:45-57, 250x50 boxes at
// col = floor(j/5), row = j%5) became a d-pad GRID CURSOR over the 10
// authored slots (2 cols x 5 rows, same col/row mapping) with the
// addcode slot below (D from a bottom row entered it, U returned —
// the SSS RANDOM-slot pattern). **D29 REVERSES EXACTLY THAT**, at the
// owner's request after playing it: the 11 slots are hit-tested by the
// FREE HAND CURSOR the CSS already had — extracted to foh_hand.{c,h}
// rather than copied — so upstream's pointer is back on this one
// screen while the SSS keeps its grid. The slot geometry, the col/row
// mapping and the addcode slot's position are unchanged; only what
// moves the selection is. Consequence, stated where it will be read:
// TSS and SSS now differ in interaction model. Full argument and the
// sticky-selection rule at FohState.tssCursor / tssHandX below.
// Char select keeps the upstream SHOULDER
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
#include "foh_hand.h" // the shared free hand cursor (A25c / DEVIATION D29)

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
//   (a) THE LIST IS EMPTY ON THE DRAWN-BUT-DEAD SIDE. It held two widgets:
//       the BACK wedge (left with A23, hit-tested at FOH_CSS_BACK_X0) and
//       the header's MODE RIBBON, which leaves with A27 — it is hit-tested
//       at FOH_CSS_MODE_* below, at exactly the extent css_header draws.
//       What had blocked it was never a rect: its action is
//       `setVersusMode(1 - versusMode)` and versusMode is SIM-VISIBLE, not
//       cosmetic (upstream reads it at physics.js:980 — a KO at 0 stocks
//       refills to 1 — actionStateShortcuts.js:155 (isFinalDeath is never
//       final), main.js:1079 (no matchTimer tick), :1334 (every player
//       starts on 1 stock) and render.js:397 (no clock drawn)), so a ribbon
//       that toggled its own label and nothing else would have been a stub
//       (HARD RULE 2). A37 made the sim plane real — sim_boot.c's page-state
//       init, its stocks arm, and sim_tick.c's `versusMode == 0` conjunct —
//       and the FOH now carries the value from the ribbon to `G.sim` at the
//       launch bridge. Registered: MENU-SPEC DEVIATION D28.
//   (b) HIT-TESTABLE, NOT ALWAYS DRAWN: your OWN token stays grabbable while
//       your port is N/A, because upstream's grab guard is
//       `playerType[j] == 1 || i == j` (css.js:300) while its token DRAW is
//       guarded on `playerType[i] > -1` (css.js:1018/1077). That asymmetry is
//       upstream's; it is carried verbatim, not repaired.
// Ports on this screen. FOUR — upstream's number (main.js's playerType is a
// 4-array), the sim's number since A46 (sim_setup_match_ports), and since
// A44/D40 the number the FOH actually models. CONTEXT.md "Port": a port is a
// player slot 0..3 and is NEVER an index into the 5-character roster.
#define FOH_CSS_PORTS 4
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
// The BACK wedge's hit region (upstream css.js:358 — `y < 160 && x > 920`),
// and the hold bar that reports it (css.js:735-746). A23.
//
// D4 wants the hit region to be the DRAWN extent, and here upstream's own
// proportions land on it exactly: 920/1200 of the canvas is 0.7667, and
// 0.7667 * 240 = 184.0 — which IS the left edge of the black slab css_header
// draws the wedge on (its quad starts at x 184). So the proportional mapping
// and the drawn extent agree to the pixel, and one constant serves both.
// The y half needs no constant: upstream's `y < 160` means "above the roster
// band" (its band opens at 160, css.js:207), and ours opens at
// FOH_CSS_BAND_TOP — the same relationship, reusing the same number that
// already keeps the band and the header from overlapping.
//
// Both bounds are STRICT, as upstream's are.
#define FOH_CSS_BACK_X0 184
// The hold bar, mapped at upstream's own ratios so it fills at upstream's
// rate. Upstream runs it from x 1020 to 1020 + 30*6 = 1200 (the right edge)
// along y 119..125 — the bottom lip of the wedge's gold underline — i.e. it
// starts at 85% of width, gains 0.5% of width per held frame, and ends flush
// with the screen. Here: 204 = 0.85 * 240, 1.2 px/frame = 0.5% of 240, and
// 204 + 30*1.2 = 240, flush with the right edge again. The 1 px left lean
// (203 on the top edge) is upstream's own 1015-vs-1020 skew, which follows
// the slab's lean the same way ours does.
// The MODE RIBBON's plate — the chevron-capped widget between the VS badge
// and the BACK wedge (foh_render.c's css_header) and, since A27, upstream's
// `setVersusMode(1 - versusMode)` click target (css.js:389-394). These four
// numbers ARE the plate: css_header builds its hexagon from them and foh.c
// hit-tests the same box, which is what D4 asks for.
//
// WHY NOT UPSTREAM'S RATIO, the way FOH_CSS_BACK_X0 is. Upstream's rect is
// `y > 100 && y < 160 && x > 380 && x < 910` (css.js:389) on a 1200x750
// canvas — i.e. a band BELOW its header, because upstream draws this blurb
// as loose 1.25x-scaled text at (390,117) with no plate at all (css.js:715-
// 721). This FOH is a REWRITE (foh_font.c header): the whole silver header
// is 26 px tall here, so upstream's proportional band (y 32..51) would land
// under our header, on the roster row, hit-testing pixels the ribbon does
// not own. The BACK wedge's ratio landed on its drawn extent to the pixel
// and was therefore used; this one does not, so the DRAWN EXTENT wins —
// that is the rule D4 states, and the ratio was only ever evidence for it.
// Bounds are strict, as upstream's are.
#define FOH_CSS_MODE_X0 104
#define FOH_CSS_MODE_X1 178
#define FOH_CSS_MODE_Y0 4
#define FOH_CSS_MODE_Y1 22
// The chevron caps' inset: the plate's flat top/bottom run from X0+INSET to
// X1-INSET and the two points sit at the vertical middle.
#define FOH_CSS_MODE_CAP 6
#define FOH_CSS_BACK_BAR_X0 204.0f
#define FOH_CSS_BACK_BAR_LEAN 203.0f
#define FOH_CSS_BACK_BAR_PER_FRAME 1.2f
#define FOH_CSS_BACK_BAR_TOP 24.0f
// Token rest slots: FOUR ports stack inside one cell, in UPSTREAM'S OWN 2x2
// (css.js — the note this block has always carried said so: "upstream stacks
// four 2x2 within a 95 px cell"). Until A44 only two ports existed, so only
// the top row was ever drawn and the 2x2 read as a single row of two.
//
// A44 (2026-08-24, D41) put the other two ports on screen and the row came
// back. The numbers are re-derived, not scaled, and the derivation is the
// whole justification — write it down because the next person to nudge one
// of them needs the four inequalities it satisfies:
//   * a cell is 44x30 at (FOH_CSS_CELL_X0 + 46c, 32). A 2x2 of radius-r
//     discs at column pitch P and row pitch Q occupies P + 2r wide and
//     Q + 2r tall, and BOTH must fit the cell or a token drawn on cell 4
//     leaves the 240 px screen / overlaps the READY ribbon below.
//   * r = 7, P = Q = 14 gives exactly 28 x 28 inside 44 x 30 — the discs
//     are TANGENT, never overlapping, which is what keeps foh.c's grab
//     hit test (strict |dx| < r, |dy| < r) unambiguous: no point can be
//     inside two tokens, so the loop's j-order can never silently decide
//     which port a press takes. That property is load-bearing now that
//     D40 makes all four tokens grabbable; at r = 9 / P = 20 four discs
//     would have spanned 78 px of a 44 px cell and the old clamp would
//     have parked port 0's token a whole cell LEFT of the character it
//     chose — i.e. it would have re-created DEVIATION D21's defect (the
//     token is the only roster-level indicator on this screen) in the
//     one configuration A44 exists to add.
//   * DX = 15 centres the 28 px block in the 44 px cell (15 + 14 + 7 = 36,
//     8 px of margin each side), so the A-drop slot needs NO clamp at all:
//     cell 4's right column lands at 190 + 15 + 14 + 7 = 226 < 240.
//   * TOKEN_Y = CELL_Y + 9 = 41 and Q = 14 put the two rows at 41 and 55,
//     spanning 34..62 — inside the cell's 32..62 exactly, so the lower row
//     never bleeds into the READY TO FIGHT ribbon (y 62..92), which
//     render_css draws AFTER the tokens and would otherwise cover them.
// D41's clamp in foh_css_token_pos is GONE as of DEVIATION D46 (below), and
// the arithmetic on the line above is why: the only arm that could ever
// exceed the right edge was the leave-band formula, and D46 retires it. The
// three surviving rest paths are all `cell_x(c) + DX`, whose widest case is
// cell 4's right column at 226 < 240. A clamp that provably cannot fire is a
// dead branch, so it is deleted rather than left to look load-bearing.
#define FOH_CSS_TOKEN_R 7
#define FOH_CSS_TOKEN_DX 15
#define FOH_CSS_TOKEN_PITCH 14
#define FOH_CSS_TOKEN_ROW_PITCH 14
#define FOH_CSS_TOKEN_Y (FOH_CSS_CELL_Y + 9)
// QUIRK Q1 — and DEVIATION D46 (fix_plan A49, owner-reported P1) RETIRES ITS
// SECOND HALF. Upstream has TWO rest formulas and they disagree: the A-drop
// slot is `419 + 95c + 40k` and the leave-band slot is `518 + 93c + 40k`
// (css.js:288 vs :337). MENU-SPEC called that "a few px"; MEASURED it is a
// 99 px base offset on a 95 px cell pitch, i.e. a leave-band token comes to
// rest on the NEXT character's cell. Mapped onto this layout at the same
// ratios that is +48 px — one whole cell — and the owner filed exactly that:
// *"whenever the pin is let go of (going off) it should go back to the
// character you had selected"*. So the leave-band drop now homes on the
// SELECTION, like the other two rest paths already do (D21, D35).
//
// These two constants are the MEASUREMENT, kept: they record what upstream's
// second formula is and what it works out to here. Nothing computes a
// position from them any more — see foh_css_token_pos (D46).
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

// --- THE CSS COLD-START PLANE (ticket #25) ----------------------------------
// The values foh_init gives this screen on a machine that has never run.
//
// THEY ARE HERE, AND NOT INLINE IN foh_init, BECAUSE THEY NOW HAVE A SECOND
// READER. Ticket #25 persists the CSS machine plane, so
// foh_persist_defaults() must produce EXACTLY what foh_init produces — a
// fresh install has to boot the screen it has always booted. Two copies of
// `140.0 * RAST_W / 1200.0` in two TUs is CONTEXT.md's costliest defect
// class ("one thing having two representations that drifted apart") with a
// silent failure mode: the drift would show up as a cursor that starts
// somewhere else after the first save, months later, in a bug report.
// So there is ONE definition and both callers ask it — the same shape
// FOH_TSS_HOME_X/Y already has for the target-select hand.
//
// handPos[0] = (140,700) on upstream's 1200x750 canvas (css.js:64), taken
// as the same FRACTION of this screen. Module scope upstream: set once at
// boot and never re-initialised on CSS entry (MENU-SPEC §2.2 property 4).
#define FOH_CSS_HAND_HOME_X (140.0 * RAST_W / 1200.0)
#define FOH_CSS_HAND_HOME_Y (700.0 * RAST_H / 750.0)
// cpuDifficulty = [3,3,3,3] (main.js:109).
#define FOH_CSS_DIFF_HOME 3
// playerType = [-1,-1,-1,-1] (main.js:107) with addPlayer arming port 0
// (main.js:495) — upstream's own fresh state, port by port.
static inline int foh_css_type_home(int k) { return k == 0 ? 0 : -1; }
// cpuSlider[k] (css.js:72): x = 152+15+166+225k-50 = 283+225k on a rail
// running [167+225k, 333+225k], i.e. 116/166 of the way along — NOT the
// level-3 stop at 2/3, though it reads back as level 3
// (round(0.6988*3)+1 == 3). Carried as the same fraction of our rail.
static inline double foh_css_slider_home(int k) {
  return (double)(foh_css_panel_x(k) + FOH_CSS_RAIL_X0) +
         (116.0 / 166.0) * (double)FOH_CSS_RAIL_LEN;
}

// DEVIATION D3 — cursor speed, the one calibration knob in this whole spec —
// moved to port/foh/foh_hand.h with the cursor itself (A25c), which foh.h
// includes above, so FOH_CURSOR_SPEED / _VX / _VY still resolve here.

// The five roster cells as a hit table, for foh_hand_hit. ONE definition, two
// callers — foh.c's drop/hover arm and foh_render.c's `hot` flag — which is
// what D4 asks for and what the CSS previously had in two hand-kept copies
// (foh.c's `css_cell_at` plus its enclosing y guard, and foh_render.c's
// `overCells &&` line). The extents are exactly those two tests' extents.
void foh_css_cells(FohHandRect out[5]);

// --- TSS layout + hit geometry (A25c; same D4 contract as the CSS above) -----
// The eleven target-select slots. Ten are upstream's own 2-col x 5-row
// authored layout (targetselect.js:196-203, col = floor(j/5), row = j%5)
// re-pitched for 240x240; the eleventh is the refusing "+ ADD CODE" slot,
// which upstream puts at the head of its CUSTOM column (:206) and which the
// rewrite drops below the grid.
//
// These numbers WERE literals inside render_tss, which was safe only while the
// cursor was an INDEX and nothing hit-tested them. A25(c) makes the hand test
// exactly what the renderer draws, so they move here and both sides read them
// through foh_tss_slots() — the CSS cells' arrangement, one screen later.
#define FOH_TSS_SLOT_X0 8
#define FOH_TSS_SLOT_Y0 30
#define FOH_TSS_SLOT_COL_PITCH 124
#define FOH_TSS_SLOT_ROW_PITCH 22
#define FOH_TSS_SLOT_W 100
#define FOH_TSS_SLOT_H 19
#define FOH_TSS_ADD_X 70
#define FOH_TSS_ADD_Y 142
#define FOH_TSS_ADD_W 100
#define FOH_TSS_ADD_H 17
// 0..9 == the tstage ids, 10 == "+ ADD CODE". Index IS FohState.tssCursor.
#define FOH_TSS_SLOTS 11
// A45 T3/T4 — the ten CUSTOM slots. Upstream's own hard limit
// (targetselect.js:102, :817) and A45 T2's MLK_MAX_SLOTS, restated here
// because foh.h cannot include foh_tbuild.h (that header includes THIS one).
// foh_tbuild.c _Static_asserts the two are equal, and check-tbuild.sh
// re-asserts it against the sim's MLK_MAX_SLOTS, so neither copy can drift.
#define FOH_TB_SLOT_CACHE 10

// drawingPolygon's working length: MLK_MAX_POLY_POINTS committed vertices
// (stage_code.h's cap; upstream is unbounded) plus the one element that
// TRACKS the cursor. Restated here rather than included — port/foh cannot
// reach stage_code.h from foh.h, and check-tbuild.sh leg [1] cross-checks
// every restated cap with a _Static_assert against the sim's own.
#define FOH_TB_MAX_POLY_PTS 33
void foh_tss_slots(FohHandRect out[FOH_TSS_SLOTS]);
// Where the hand is parked when target-select is entered: the centre of slot
// 0, so the screen opens hovering tstage 0 exactly as the index cursor's
// `tssCursor = 0` opened it (menu.js:77-84's targetPointerPos reset).
#define FOH_TSS_HOME_X (FOH_TSS_SLOT_X0 + FOH_TSS_SLOT_W / 2.0)
#define FOH_TSS_HOME_Y (FOH_TSS_SLOT_Y0 + FOH_TSS_SLOT_H / 2.0)

// --- C5: the netplay switch (owner ruling 2026-07-28, fix_plan C5) ----------
// "HIDE Spectate / P2P / Server; VS MELEE goes straight to local VS.
// Implement behind a NAMED FLAG so the battle-mode submenu can be restored
// later without archaeology (a single documented switch, not deletions)."
//
// FOH_NETPLAY 0 (the shipped build): menu-top's `VS. Melee` row runs the
// Local VS action DIRECTLY (menu.js:105 — changeGamemode(2) +
// positionPlayersInCSS), and the whole MPMENU page is unreachable. Nothing
// is deleted: FOH_MENU_BATTLE, its labels/blurbs (foh_render.c kMenuText[2]),
// its four A-arms, its B-back edge and its judge-registered transitions all
// still exist and still compile.
// FOH_NETPLAY 1: the page SHELL and routing come back — `VS. Melee` opens
// the battle page with the cursor on LOCALVS (menu.js:73-75) and the CSS's
// B-hold returns there (css.js:186-194 leaves menuMode untouched). It does
// NOT restore upstream's page 2 "verbatim": Spectate/Server are not
// implemented in this port and still refuse (upstream's own P2P body is
// commented out), so a future netplay arc owns their behaviour.
// SCOPE, stated exactly (review-r2 MAJOR corrected an over-claim here):
// this flag covers the BATTLE PAGE ONLY. DEVIATION D5 — the CSS port-type
// cycle dropping NET (main.js:504-520's four-cycle narrowed to three) — is
// a SEPARATE registered deviation with its own owner acceptance, and it is
// NOT wired to this switch. A future netplay arc has to revisit both, plus
// everything else §11.1 lists; flipping this one restores the page and its
// four judge-registered edges, and nothing more.
#define FOH_NETPLAY 0

// --- D27: the controls-chooser switch (owner ruling 2026-08-23, fix_plan A24)
// "collapse now - make easily revertable though please."
//
// The Controls submenu (`HANDHELD` / `CONTROLLER`, upstream menuMode 3) exists
// ONLY to make a two-way choice, and A33 measured that one side can never
// exist on the shipped FunKey-OS image: `CONFIG_USB_MUSB_GADGET=y` with no
// HOST/DUAL_ROLE (mutually exclusive Kconfig in 4.14, so host code is not
// compiled), `dr_mode = "peripheral"` in the DTS and a deliberately floating
// USB ID pin. The PORT is physically there — the "no gamepad port" framing is
// retracted (docs/research/gc-adapter.md §1.4/§2) — but nothing this project
// ships can drive it, because undoing that means rebuilding and reflashing
// FunKey-OS and this project ships an OPK. A chooser whose purpose IS the
// choice has no job left once one side dies, so the owner chose to collapse
// rather than grey it (the A10 distinction: greying suits a page that KEEPS
// live entries beside the dead ones).
//
// FOH_CTL_CHOOSER 0 (the shipped build): Options row 2 `CONTROLS` runs
// changeGamemode(12) DIRECTLY (menu.js:159-161) and lands on the HANDHELD
// screen; B from that screen returns to Options with the cursor still on the
// row that opened it. FOH_MENU_CONTROLS, FOH_CTRL_PAD and render_ctrl_pad
// become unreachable — exactly as FOH_MENU_BATTLE already is at
// FOH_NETPLAY 0. NOTHING IS DELETED: the chooser screen, its labels
// (foh_render.c kMenuText[3]), its A ternary, its B-back edge, the controller
// destination and all six judge-registered transitions still exist and still
// compile.
// FOH_CTL_CHOOSER 1: the pre-D27 routing comes back whole — `CONTROLS` opens
// the chooser with the cursor on row 0 (menu.js:138-141: the cursor reset and
// the second `menuSelect` that a menuMODE change emits) and both destinations
// return to it. That is the D25 build, unchanged.
//
// REVERTING IS THIS ONE DIGIT. Everything downstream keys off it: the judge's
// build profile (judge-foh-trace.js parses this header exactly the way it
// already parses FOH_NETPLAY), the authored edge authority (`ctl` / `noctl`
// rows in judge-domains.authored.txt) and the sound witness's Options-row
// case. port/foh/check-controls-labels.sh builds the witness at BOTH values
// and asserts BOTH routings, so the flag-on path cannot rot unnoticed.
// SCOPE, stated exactly: this flag covers the CONTROLS CHOOSER ONLY. The
// screen tokens, the upstream gameMode identities (12 / 14) and the D25
// labels and row order are untouched by it — D25 is paint, D27 is
// reachability. THE FROZEN FLOWS ARE FROZEN AT 0 (f04-nav), like every other
// artifact frozen under a profile: flipping this digit without re-freezing
// them fails mechanically in the judge instead of passing quietly.
#define FOH_CTL_CHOOSER 0

typedef enum {
  FOH_STARTUP = 0,
  FOH_TITLE,
  FOH_MENU_TOP,
  FOH_MENU_OPTIONS,  // upstream menuMode SECONDLEVELOPTIONS = 1
  FOH_MENU_BATTLE,   // upstream menuMode MPMENU = 2 (C5: unreachable at
                     // FOH_NETPLAY 0, kept whole)
  FOH_MENU_CONTROLS, // upstream menuMode CONTROLLERCALIB = 3
  FOH_CSS,
  FOH_SSS,
  FOH_OPT_GAMEPLAY,
  FOH_OPT_AUDIO,  // upstream gameMode 10 (menus/audiomenu.js)
  FOH_CTRL_PAD,   // upstream gameMode 14 (controllermenu.js) — §9.2 honest
                  // no-controller state, see the note in foh.c
  FOH_CTRL_KEY,   // upstream gameMode 12 (keyboardmenu.js) — §9.3 D13
                  // reduced form, see the note in foh.c
  FOH_CREDITS,    // upstream gameMode 13 (menus/credits.js) — MENU-SPEC §8,
                  // punch-list A7; the shooting gallery, see foh.c
  FOH_MATCH,
  FOH_TSS,    // target-select (upstream gameMode 7, targetselect.js)
  FOH_TMATCH, // target-match (terminal; the driver owns the target sim)
  // A45 T4 — the TARGET BUILDER (upstream gameMode 4, targetbuilder.js).
  // Entered from menu-top row 2 (menu.js:87-90 `setEditingStage(-1);
  // setTargetBuilder(i); changeGamemode(4)`), left by its pause-menu Quit
  // (:832-835 changeGamemode(1)) or by B (DEVIATION D50). The engine lives
  // in port/foh/foh_tbuild.c behind a pointer seam — see foh_tbuild.h for
  // why, and for what a build without that TU draws instead.
  FOH_TBUILD,
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

// Controls > HANDHELD screen row layout (A31). Rows 0..8 are the nine
// ACTION rows foh_ctl_labels.h names; row 0 is the d-pad (not bindable),
// rows 1..8 are physical buttons CTL_BTN_A..CTL_BTN_MENU, so the button
// index of row r is (r - 1).
#define FOH_CTL_ACTION_ROWS 9
#define FOH_CTL_ROW_STYLE 9
#define FOH_CTL_ROW_RESET 10
#define FOH_CTL_ROWS 11

// --- CREDITS (upstream menus/credits.js, 422 lines; MENU-SPEC §8; A7) ------
//
// WHAT THE SCREEN IS. Not a roll — a Star Fox shooting gallery. Fourteen
// contributor names scroll up over a 100-star warp field and you shoot them
// with a twin-laser reticle; hitting one prints that person's ROLE and what
// they DID in a panel at the bottom. It ends on a 2500-frame timer (or B),
// playing `complete` if all fourteen were hit and `failure` otherwise.
//
// THE NAMES ARE OTHER PEOPLE'S ATTRIBUTION. `foh_credits` below carries
// credits.js:115-132 VERBATIM — spelling, order, roles and blurbs. They were
// extracted from the pinned clone's bytes by a regex over the source rather
// than retyped (the project's "JS-reference extraction" rule: a transcription
// bug in a credit is the worst defect this screen can carry), and
// port/foh/check-credits.sh re-extracts them from the clone at check time and
// requires the C table to agree row for row.
//
// COORDINATE SPACES, stated once because the file uses BOTH:
//   * the NAME PLANE keeps upstream's canvas units verbatim — `y0` below is
//     the ScrollingText yPos literal, the scroll runs at upstream's -2/-3 px
//     per frame, and the exit timer is upstream's cScrollingMax of 5000.
//     Those numbers ARE the content schedule; scaling them would hide it.
//     They convert to raster pixels only at draw/hit time (x/5, y*0.32 —
//     DEVIATION D4's scale, the audio screen's note).
//   * the RETICLE, the STARS and the LASERS live in RASTER pixels, because
//     each is a circle or a straight run and D4's y scale (0.32) differs from
//     its x scale (0.2): carrying them in canvas units would draw the warp
//     field and the reticle as ellipses. Every one of their constants is
//     upstream's divided by 5, cited at the site.
#define FOH_CRED_NAMES 14
#define FOH_CRED_STARS 100
// Shot slots. Upstream's cShots is unbounded, but the domain is not: a fire
// pushes TWO shots and arms an 8-frame cooldown (credits.js:186,196), and a
// shot dies at life 25 (:341), so at most ceil(25/8)+1 = 4 fires * 2 = 8 can
// ever be live. 16 is that bound doubled; foh.c traps on overflow rather than
// dropping a laser silently.
#define FOH_CRED_SHOTS 16

// One ScrollingText's authored content (credits.js:41-45,115-132).
typedef struct {
  const char *name;     // .Text
  const char *position; // .position — the ROLE
  const char *info;     // .information — what they did
  int y0;               // the yPos literal the constructor is called with
} FohCredit;
extern const FohCredit foh_credits[FOH_CRED_NAMES];

// cStar (credits.js:251-258). `dx`/`dy` are `vel*cos(angle)` and
// `vel*sin(angle)` evaluated ONCE at spawn — upstream recomputes both every
// frame from a constant angle (:327-328), so the values are identical and the
// device does not pay 200 transcendentals per frame for them.
typedef struct {
  double x, y; // raster pixels
  double dx, dy;
  int life;
} FohCredStar;

// cShot (credits.js:265-278). Positions are in upstream's Y-FLIPPED laser
// space (`RAST_H - y` at draw time, :347-348). `sx`/`sy` are
// `distance*cos(angle)` and `distance*sin(angle)`, fixed at fire time.
typedef struct {
  double x, y;     // position
  double lx, ly;   // lastPosition
  double l2x, l2y; // lastPosition2
  double tx, ty;   // target — the reticle at FIRE time, y-flipped (:267)
  double vel;
  double sx, sy;
  int life;
  bool live;
} FohCredShot;

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
  //
  // FOUR WIDE as of A49, which is upstream's own width: `cpuSlider` is a
  // FOUR-element array (css.js:72) and the knob-grab loop runs `s < 4`
  // (css.js:397). A44 left it at two because D40(b) kept CPU off ports 2/3,
  // so the third and fourth knobs would have been width nothing could draw
  // or grab. The owner retired D40(b) (fix_plan A49 ruling 1), so all four
  // ports can be CPU and all four knobs are live — foh.c's two knob loops
  // now run to FOH_CSS_PORTS, as upstream's do.
  double cssSliderX[FOH_CSS_PORTS];
  // handType[0] (css.js:63): 0 handPoint, 1 handOpen, 2 handGrab. STORED, not
  // derived, because upstream's assignments are path-dependent: an A-drop
  // clears whichTokenGrabbed but leaves handType at 2 for that draw.
  int cssHandType;
  // Which rest slot each port's token last came to: 0 = the A-drop slot
  // (css.js:227-233 class), 1 = the leave-band slot (css.js:337), 2 = the
  // endGame SNAP slot (main.js:1381-1384 -> css.js:154). Upstream's three
  // formulas genuinely disagree, so the resting position is PATH DEPENDENT
  // — see the notes on each arm in foh_css_token_pos (foh.c). Slot 2 lands
  // on the port's CHOSEN cell under DEVIATION D21 (A29), where upstream
  // lands it on cell `k`; the full argument is on that arm.
  //
  // Slot 2 exists because the A19 in-process return made it OBSERVABLE:
  // endGame snaps every token before landing on the CSS, so a returning
  // player's tokens are where endGame put them, not where the last drag
  // left them (review-mexit-r2 Medium). Before the return existed the CSS
  // was only ever entered before a match, so the snap had nothing to move.
  int cssTokenRest[FOH_CSS_PORTS];
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
  //
  // A44/D40: FOUR ports wide. `selChar[k]` is the plane; `p1Char`/`p2Char`
  // are the SAME STORAGE under their historical names, because port/gfx's
  // ctl_input_witness.c reads `s.p1Char` and this lane may not touch that
  // file. A union, not a second copy: CONTEXT.md's costliest defect class is
  // "one thing having two representations that drifted apart", and two
  // fields kept in sync by hand is exactly that. Overlaid storage cannot
  // drift. WRITE THROUGH `selChar[k]` in new code — a per-port `if (k == 0)
  // ... else ...` chain is how D21 and D35 were both written.
  //
  // A49/DEVIATION D45: this plane is PERSISTED to SD (foh_persist.h).
  // Upstream cookies no character at all, so persisting is the deviation;
  // the owner asked for it in as many words (*"i want to MAKE it persistent
  // ... I want it to be last character"*).
  // The TOKEN plane below is NOT persisted and must never be: it is re-homed
  // FROM this one at load, which is D21/D35's rule applied to boot.
  //
  // "AND ONLY THIS PLANE" USED TO STAND HERE AND NO LONGER DOES. Ticket #25
  // persists the rest of the CSS machine plane too — port types, CPU levels,
  // the mode, the hand, what the hand is holding — so the sentence that told
  // a reader "the character is the only CSS state on the card" would now be
  // false. The token plane's exclusion is unchanged and is the sentence
  // above; it is a rule about VIEWS, not a rule about how much is saved.
  union {
    int selChar[FOH_CSS_PORTS]; // 0 marth 1 puff 2 fox 3 falco 4 falcon
    struct {
      int p1Char, p2Char, p3Char, p4Char;
    };
  };
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
  int cssChar[FOH_CSS_PORTS];
  // playerType[0..3] (main.js:107): -1 N/A, 0 HMN, 1 CPU. NET (2) is
  // DEVIATION D5. A44/D40 made this FOUR wide; the `p1Type`/`p2Type` names
  // are union aliases over the same storage for the ~25 existing readers
  // (same rule as selChar above — overlaid, never copied).
  //
  // DOMAIN IS UNIFORM ACROSS ALL FOUR PORTS as of A49: every port cycles
  // -1 -> 0 -> 1 -> -1 (N/A, HMN, CPU), which is upstream's own togglePort
  // with only D5's NET step removed. A44's DEVIATION D40(b) narrowed ports
  // 2/3 to N/A -> HMN and is now RETIRED, by owner ruling (fix_plan A49
  // ruling 1: *"yeah enable the CPu please"*).
  //
  // D40(b)'s STATED GROUND WAS WRONG and the correction is worth keeping:
  // AIBRIDGE1 is the RECORDED stream used to REPLAY a CPU golden, not what
  // makes the AI run. The play path links the LIVE `ai.c` through
  // `ml_sim_runai_live` (fix_plan A48/`check-ai-live.sh` -> `AI LIVE
  // CONFORMS`), so a CPU on port 2 or 3 plays. What AIBRIDGE1's single slot
  // actually limits is VERIFICATION, not capability — see the ACCEPTED
  // CONSEQUENCE written at the launch guard in foh.c.
  //
  // PERSISTED TO SD as of ticket #25 (OWNER RULING 2026-08-27, option A —
  // the argument, the reversal and the consequence are written out at
  // foh_persist.h's `portType` field). Write through `portType[k]`, never
  // through the union aliases: foh_persist.c's field table addresses this
  // plane BY OFFSET under ONE of the two names, and a table that named both
  // would serialise the same sixteen bytes twice.
  union {
    int portType[FOH_CSS_PORTS];
    struct {
      int p1Type, p2Type, p3Type, p4Type;
    };
  };
  // cpuDifficulty[0..3] (main.js:109 — `[3,3,3,3]`), 1..4 (the slider's
  // domain, css.js:325-327), default 3.
  //
  // A49 widened this from the two scalars `p1Difficulty` + `difficulty` to
  // upstream's own four, because all four ports can now be CPU. It uses the
  // SAME anonymous-union overlay selChar and portType use, for the same
  // reason: `difficulty` is the name the launch bridge, foh_dev.c, foh_app.c
  // and every frozen LAUNCH line already mean by port 1's level, and two
  // fields kept in sync by hand is CONTEXT.md's costliest defect class.
  // Overlaid storage cannot drift. WRITE THROUGH `cpuDifficulty[k]` in new
  // code — and now for a second reason: ticket #25 PERSISTS this plane
  // (`cpudiff` in foh_persist.c's field table), and that table addresses it
  // by offset under ONE name. A row per union alias would write the same
  // four ints four times.
  union {
    int cpuDifficulty[FOH_CSS_PORTS];
    struct {
      int p1Difficulty, difficulty, p3Difficulty, p4Difficulty;
    };
  };
  int bHold;          // consecutive B frames in CSS (30 = back)
  // versusMode (main.js:140), 0 stock | 1 endless — written ONLY by the CSS
  // mode ribbon's `setVersusMode(1 - versusMode)` (css.js:393; A27). It is
  // PAGE state upstream, not match state: startGame never resets it, so it
  // lives here beside the other page-scoped CSS fields, is initialised once
  // by foh_init's memset (upstream's own `= 0`), and survives a match and
  // the MEX_CSS re-entry.
  //
  // IT IS PERSISTED TO SD. OWNER RULING 2026-08-27 (ticket #25, option A),
  // and this comment is a REVERSAL: it used to end "and is NOT persisted to
  // SD — upstream keeps no cookie for it, so a power cycle is the page
  // reload that clears it." The owner reported that as a defect in as many
  // words — an endless KO fest reverts to VS Melee across a lid close — and
  // ruled that the mode comes back. Upstream's absent cookie is therefore
  // evidence about UPSTREAM, not a rule about this port: this device has no
  // page reload, it has a lid, and the two are not the same event. The row
  // is `vsmode` in foh_persist.c's field table.
  //
  // A power cycle no longer clears it, so a machine can boot into ENDLESS
  // with nothing on screen saying the player chose it in another session.
  // That is the ruled consequence and it is stated, not softened; the mode
  // ribbon draws the current mode, which is the whole of the feedback.
  int versusMode;
  // sss
  int sssCursor; // 0..5 == oracle stage ids; 6 = the refusing RANDOM slot
  // target-select (upstream targetselect.js; header notes): cursor
  // 0..9 == tstage ids (targetStageMapping order), 10 = the refusing
  // addcode slot; the char plane is the SHARED p1Char
  // (characterSelections[0] — setCS writes the same array upstream)
  //
  // DEVIATION D29 (A25c, owner-requested): tssCursor is still the SELECTION —
  // what a launch launches and what the renderer rings — but it is no longer
  // STEPPED by the d-pad. The hand below is, and the slot it lands in writes
  // this. The selection is STICKY, which is the CSS's own rule and not a
  // convenience: hovering a cell there SELECTS it (css.js:222-226) and leaving
  // the band does not un-select, so `targetRecords[p1Char][tssCursor]` always
  // has a slot to read and A always has a target to launch. The `!=` guard
  // that fires menuSelect once per change is the same guard, for the same
  // reason.
  int tssCursor;
  // The free hand on target-select (A25c / D29). Doubles, integrated by
  // foh_hand_step from the d-pad, clamped to the screen — the CSS hand's
  // model exactly, minus handType (there is no grab gesture here).
  //
  // WHAT D29 REVERSES, stated plainly because it is the cost of the owner's
  // ask: the header's rewrite deltas record that upstream's target-select and
  // stage-select POINTERS were deliberately rewritten INTO index cursors for a
  // device with no mouse, and the SSS keeps that rewrite. So after A25(c) the
  // two sibling screens differ in interaction model: TSS is a free cursor
  // again, SSS is a 3x2 grid. The owner asked for the cursor here after
  // playing it; either the SSS follows in a later arc or the inconsistency is
  // accepted, and that choice is his, not this file's.
  //
  // Unlike the CSS hand (module scope upstream, set ONCE at boot, MENU-SPEC
  // §2.2 property 4), this one is RE-HOMED on every entry to the screen, at
  // FOH_TSS_HOME_{X,Y}. That is upstream's behaviour, not a deviation:
  // menu.js:77-84 resets targetPointerPos when it opens target-select, which
  // is the same reset the index cursor's `tssCursor = 0` was carrying.
  double tssHandX, tssHandY;
  // --- A45 T3: the CUSTOM page of target-select -----------------------------
  // 0 = the ten AUTHORED stages (tstage ids 0..9), 1 = the ten CUSTOM slots
  // (upstream's "Custom N", targetselect.js:288-294). Upstream draws both
  // families side by side in FOUR columns of a 1200 px canvas; at 240 px the
  // grid is two columns of five (foh_tss_slots), so the same ten rects carry
  // whichever family is on show and slot 10 — which used to be the refusing
  // "+ ADD CODE" — flips between them.
  //
  // It is VIEW state, and it emits nothing: like tssCursor and the hand
  // position, it changes what a launch would launch without being a
  // transition or a selection. What IS observable is the launch itself —
  // FohState.tssStage carries MLK_PLAYING_BASE + slot (10..19) on this page,
  // which is A45 T2's own id space (custom_stage.h), so the TLAUNCH record
  // names the custom slot the player chose.
  int tssPage;
  // Slot presence for the custom page, refreshed on every entry to the
  // screen and after every page flip through foh_tbuild_ops->slots(). Cached
  // rather than rescanned per frame because each scan opens ten files.
  // BY INDEX — D43: an absent or corrupt slot stays in ITS OWN place with
  // its reason, and nothing ever shifts up to fill a hole.
  bool tssSlotPresent[FOH_TB_SLOT_CACHE];
  const char *tssSlotReason[FOH_TB_SLOT_CACHE];
  // --- A45 T4: the TARGET BUILDER's view state ------------------------------
  // The DOCUMENT is not here: sizeof(MlkStage) is 45,344 bytes against this
  // struct's 7,224, and it is module state in foh_tbuild.c exactly as
  // upstream's `stageTemp` is module state in targetbuilder.js:53-72. What
  // lives here is what the machine and the renderer both read. Semantics and
  // upstream citations: port/foh/foh_tbuild.h.
  int tbTool;                 // targetTool (:26) — UPSTREAM'S OWN index
                              // 0..9, see FOH_TB_TOOL_* in foh_tbuild.h.
                              // The CYCLE contains only built tools; the
                              // NUMBERING is upstream's so citations line up.
  int tbGrid;                 // index into gridSizes (:80); 4 == free
  int tbSlot;                 // editingStage (:57); -1 == never saved
  double tbUnX, tbUnY;        // unGriddedCrossHairPos (:24), WORLD units
  double tbX, tbY;            // crossHairPos (:21), grid-snapped
  bool tbPaused;              // builderPaused (:75)
  bool tbHoldA;               // holdingA (:37)
  int tbPauseRow;             // builderPauseSelected (:76)
  int tbPane, tbPaneRow;      // the slot list a pause row opened
  // hoverItem (:73) / grabbedItem (:72). Upstream's is `0` for nothing and
  // `[typeString, index]` otherwise; the port splits that into a KIND
  // (FOH_TB_H_*, with NONE == 0 so the falsy test reads the same) and an
  // index. A45 T4 could encode it in one int because only two kinds were
  // reachable; T5-T7 reach ten.
  int tbHoverKind, tbHoverIdx;
  int tbGrabKind, tbGrabIdx;
  // ledgeHoverItem (:74) — [type, index, side], a THIRD cursor because
  // upstream's LEDGE arm keeps it separate from hoverItem (:513-541).
  int tbLedgeKind, tbLedgeIdx, tbLedgeSide;
  int tbWallType;             // wallTypeIndex (:27) into wallTypeList (:29)
  int tbDamageType;           // damageTypeIndex (:31) into (:33)
  int tbDrawMode;             // drawMode (:44) — 0 | 1, upstream's own type
  int tbScaleScroll;          // scaleScroll (:43), the 6-frame divider
  // The in-progress drag, tools PLATFORM and WALL (:39, :41). CANVAS units
  // (upstream stores realCrossHair here, not the world position).
  double tbDragX0, tbDragY0, tbDragX1, tbDragY1;
  // drawingPolygon (:40) + currentPolygonLines + `denied`, the POLYGON
  // tool's in-progress state. CANVAS units. The last element TRACKS the
  // cursor (:386), so a committed vertex count is tbPolyN - 1.
  bool tbDrawingPoly;         // amDrawingPolygon (:38)
  int tbPolyN;
  double tbPolyX[FOH_TB_MAX_POLY_PTS], tbPolyY[FOH_TB_MAX_POLY_PTS];
  // currentPolygonLines is derivable — line k is (poly[k], poly[k+1]) —
  // so only its LENGTH is state. It is not the same as tbPolyN: it lags by
  // two and it is popped by B independently (:296, :404).
  int tbPolyLinesN;
  bool tbDenied;              // `denied` (:298-302), sticky across frames
  bool tbConnectInd;          // drawConnectIndicator (:389)
  double tbConnectX, tbConnectY;
  int tbToolTimer;            // toolInfoTimer (:35), 120 frames
  // THE VISIBLE REFUSAL. Every "no" this screen says puts a string here and
  // the renderer draws it. The ticket exists because a refusal was a `deny`
  // the owner could not hear, so a sound alone is not an answer anywhere in
  // the builder. Static strings only — never a pointer into a buffer.
  const char *tbMsg;
  int tbMsgTimer;
  // options-gameplay. The row list is upstream's, VERBATIM and complete
  // (MENU-SPEC §3.1; gameplaymenu.js:11-12 `menuVOptions = 4` is a MAX
  // INDEX, so FIVE rows, and `menuHOptions = [0,0,0,0,3]` gives the last
  // row four columns). It is NOT derived from gameSettings — upstream
  // hard-codes it three times (labels :178-182, A-actions :39-59, value
  // strings :228-245) and so do we, in the same order.
  int optRow; // 0 turbo, 1 lCancelType, 2 flashOnLCancel,
              // 3 everyCharWallJump, 4 tapJumpOff columns
  int optCol; // 0..menuHOptions[optRow] (only row 4 has more than one)
  int turbo;
  int lCancelType;
  // render-plane setting (render.js:125 — the white LANDINGATT* flash),
  // implemented at gfx_render.c:443. WIRED: foh_dev.c writes this value
  // over the GFXDATA1 dump's captured copy at match boot on both LIVE
  // paths (VS and target). The frozen EVIDENCE arms deliberately keep the
  // dump's own value, so recorded render references are untouched.
  int flashOnLCancel;
  // MEASURED DEAD upstream (MENU-SPEC §3.3): ZERO MECHANICS/GAMEPLAY
  // consumers in src/. Two DISPLAY-only readers exist and are not
  // mechanics (gameplaymenu.js:239 draws its own row; css.js:1183-1191
  // prints it in inServerMode). The menu writes it, the menu displays it,
  // and no simulation code ever asks
  // for it. Owner ruling (fix_plan, 2026-07-29): implement it faithfully
  // dead — row + persisted bit + wired to nothing. Inventing a walljump
  // rule here would be the faithfulness violation (deferred item WJ-later
  // is an explicit owner-sanctioned deviation, done at the END).
  int everyCharWallJump;
  int tapJumpOff[4];
  // The three gameSettings keys with NO UI ROW (settings.js:47-55;
  // MENU-SPEC §3.2). Upstream's B-exit save loop writes EVERY key
  // (gameplaymenu.js:29-31), and its CSS label tables carry the empty
  // string for exactly these (css.js:85/:87) — that is how upstream marks
  // "not displayable". They live here because the persist chokepoint
  // collects from FohState and the launch bridge reads it; NO row edits
  // them and no renderer draws them.
  // phantomThreshold is the one that matters: default 0.01 (NOT 0) and ON
  // THE CHECKSUM SURFACE (hitDetection.js:335,337,348; physics.js:
  // 1039-1040). The qjs gotcha (CLAUDE.md M0 task 6) is precisely that a
  // missing storage plane Number("")-zeroes it and silently flips physics.
  double phantomThreshold;
  int blastzoneWrapping;         // zero readers in src/ (measured)
  int dustLessPerfectWavedash;   // zero readers in src/ (measured)
  // options-audio (upstream menus/audiomenu.js): masterVolume[0] = sounds,
  // [1] = music (audiomenu.js:13, defaults 0.5/0.3); audioMenuSelected
  // (:15). Steps are a fixed +/-0.1 in DOUBLES with NO rounding, so the
  // float dust (0.7999999999999999) is upstream's own and is what gets
  // persisted (:24-25) — carried verbatim.
  double masterVolume[2];
  int audioRow;
  // C30(c), rewritten by A31 (DEVIATION D26): the Controls > HANDHELD
  // screen's cursor. It used to cover TWO rows (control style, Mod
  // shoulder) — which is exactly the owner's 2026-08-23 complaint, since
  // the nine rows that say what the buttons DO were unreachable. It now
  // covers all of them:
  //
  //   0        the d-pad row. Drives the control STICK, not a button —
  //            selectable so navigation is uniform, but NOT bindable
  //            (L/R on it denies).
  //   1 .. 8   the eight physical buttons, in CtlBtn order
  //            (A B X Y L R START MENU). L/R REBINDS the selected row.
  //   9        control style (L/R cycles the three CtlStyle values).
  //   10       RESET TO DEFAULTS (A activates).
  //
  // The Mod-shoulder row is GONE from the screen (owner: "get rid of mod
  // altogether as an option here") — the cell itself survives in
  // ctl_style.c because the BOX label table still reads it and the
  // persisted record still carries it; what a player wanted it for (L and
  // R the other way round) is now a plain rebind of those two rows.
  //
  // The VALUES do not live here — style and Mod live in ctl_style.c's
  // process cells and the bindings in its per-port table, all of which the
  // sim-side input path reads; this is only which row the cursor is on.
  // Same module lifetime as audioRow: not reset on entry, so a second
  // visit opens on the row you left.
  int ctlRow;
  // --- credits (upstream menus/credits.js; MENU-SPEC §8; A7) -------------
  // MODULE lifetime, exactly like upstream's file-scope `let`s: nothing here
  // is reset by entering the screen except through `credInit`, which is the
  // port of `initc` (credits.js:11) and does upstream's own reset block
  // (:112-138). The star field is deliberately NOT in that block — upstream
  // builds it once at module load (:262-265) and never rebuilds it, so the
  // warp keeps drifting across visits.
  bool credInit;         // initc (:11)
  int credScrollPos;     // cScrollingPos (:26) — the exit timer, +2/frame
  int credScore;         // cScore (:15)
  int credCool;          // shoot_cooldown (:10)
  bool credShootBuf;     // cShootBuffer (:27) — the one-frame-deep buffer
  int credLaser;         // currentLaserColor (:31), 0..3
  double credCursorAngle;// cCursorAngle (:21), DEGREES (:403 divides by 180)
  int credHitTimer;      // lastHit[0] (:29) — 600 frames of info panel
  int credHitIdx;        // lastHit[1]
  bool credHitCleared;   // lastHit[2]
  // The reticle. DEVIATION D12 (MENU-SPEC §8.3): upstream maps rawX/rawY
  // ABSOLUTELY onto the canvas (:169-186), which a d-pad reduces to nine
  // reachable positions; this integrates the d-pad through the SHARED free
  // cursor (foh_hand_step) and clamps, like the CSS and target-select.
  // Raster pixels, doubles, rounded only at draw time.
  double credX, credY;
  // The fourteen ScrollingTexts, in upstream canvas units (see the note at
  // FOH_CRED_NAMES). xPos (:42), yPos (:43), xVal (:50), xMax (:49),
  // xDirection (:51), isShot (:48), canRender (:52).
  int credNameX[FOH_CRED_NAMES];
  int credNameY[FOH_CRED_NAMES];
  int credNameXVal[FOH_CRED_NAMES];
  int credNameXMax[FOH_CRED_NAMES];
  int credNameXDir[FOH_CRED_NAMES];
  bool credNameShot[FOH_CRED_NAMES];
  bool credNameRender[FOH_CRED_NAMES];
  FohCredStar credStar[FOH_CRED_STARS];
  FohCredShot credShot[FOH_CRED_SHOTS];
  // DEVIATION D38 — the FOH-LOCAL random stream. Full argument at its
  // definition in foh.c; in one line, upstream's Math.random IS the seeded
  // oracle stream (the same fact that makes the SSS RANDOM slot a registered
  // refusal above), so the credits' star/name draws get their own generator
  // that the sim's stream never sees.
  uint32_t credRng;
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
  // targetSelectTimer (targetselect.js:51). Upstream increments it inside
  // drawTSS ONLY (:268), so it is NOT the global frame counter: it advances
  // on target-select ticks and nowhere else. LOOK plane — pinned to 0 by
  // foh_look_canonical like every other phase (review-r3).
  int tssTimer;
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

// Is this transition event one of the machine's PERSISTENCE SAVE POINTS?
//
// It lives here, once, for foh_launch.h's reason: two drivers reach the
// save chokepoint — foh_app.c (host) and foh_dev.c (device/dev app) — and
// each used to spell the condition out at its own call site. A31 MEASURED
// the cost of that: the product binary's arm named only `options-gameplay`,
// so a Controls-screen or audio edit reached SD only if the player later
// happened to B-exit an unrelated screen, i.e. it silently vanished on
// restart. One predicate, both drivers, cannot drift.
//
// `ev` must be a FOH_EV_TRANS event; anything else is false.
bool foh_is_save_point(const FohEvent *ev);

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
// Port k's type (-1/0/1) for every k in 0..3 (A44/D40), and its CPU level
// (1..4). The level is only meaningful where type == 1, which D40 confines
// to ports 0/1 — the accessor says so at its own site.
int foh_css_port_type(const FohState *s, int k);
int foh_css_port_diff(const FohState *s, int k);

// foh.c: where credit `k`'s name is DRAWN, in raster pixels — the same D4
// contract as foh_css_cells/foh_tss_slots. `credits.js:53-58`'s size() is the
// hit box AND the drawn glyph run upstream (a monospaced 20 px/char face), so
// the port's box is its own rendered width: the renderer blits at this rect
// and the shot test hit-tests it, from one definition. `w` is
// foh_text_width(uppercased name), `h` is face 1's 7 px (upstream's 23 canvas
// px * D4's 0.32 = 7.36).
void foh_credits_name_rects(const FohState *s, FohHandRect out[FOH_CRED_NAMES]);

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
// A45 T3 — re-read the ten custom slots into FohState's cache. Called on
// entry to target-select and on every page flip (foh.c). Safe with the
// builder engine unlinked: every slot then reports absent, with the reason
// "unavailable in this build".
void foh_tss_refresh_slots(FohState *s);
// A45 T4 — the sound queue's push, reachable from foh_tbuild.c. A wrapper
// over foh.c's static snd_push, so the overflow guard keeps ONE body.
void foh_snd_push(FohState *s, const char *name);

void foh_text(Raster *rz, int x, int y, int scale, const char *s,
              RastCol col);
int foh_text_width(const char *s, int scale);

// foh_font.c: the self-authored 6x9 heavy display face; italic != 0 shears
// it (synthetic oblique). Advance 7 px per glyph at scale 1.
void foh_text2(Raster *rz, int x, int y, int scale, int italic,
               const char *s, RastCol col);
int foh_text2_width(const char *s, int scale);

// --- THE FACE DOMAIN (CONTEXT.md; spec #20 / ticket #21) --------------------
// The set of characters a face can actually draw. It lives in foh_font.c's
// glyph tables and NOWHERE ELSE — these four are the only way to ask about
// it, so no caller has to restate a list that then drifts out of step.
// `face` is 1 (the 5x7 body face) or 2 (the 6x9 display face).
bool foh_face_has(char c, int face);
// The first character of `s` the face cannot draw, or 0 when the whole
// string is drawable. NULL-safe (a null string is drawable: nothing to draw).
char foh_face_undrawable(const char *s, int face);
// Writes the face's whole drawable set into `out` as a NUL-terminated string
// and returns its length; -1 if `cap` cannot hold it. For checks that want to
// ENUMERATE the domain rather than assert a hand-copied list.
int foh_face_domain(int face, char *out, int cap);
// Product builds ONLY (foh_app.c, foh_dev.c): draw a visible placeholder box
// for a character outside the domain instead of dying. Every check build
// leaves this alone and keeps the loud gfx_fatal — see foh_font.c's header
// for why the default is that way round.
void foh_font_enable_placeholder(void);
int foh_font_placeholder_enabled(void);

#endif // FOH_FOH_H
