// port/foh/foh_persist.c — the ONE persistence chokepoint (fix_plan §M4
// task 13). Contract, format, defaults provenance, and the stderr
// event grammar live in foh_persist.h. Every driver (foh_app.c,
// foh_dev.c) consumes persistence ONLY through this TU — no scattered
// fopen calls (the task's chokepoint law).
#include "foh_persist.h"

#include <errno.h>
#include <fcntl.h>
#include <math.h>
#include <stdarg.h> // fp_addf: the one variadic append into the serialize buffer
#include <stddef.h> // offsetof — the field table's offset column
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/statvfs.h> // foh_persist_publish's free-space check
#include <unistd.h>

#include "../gfx/ctl_style.h" // CTL_STYLE_COUNT (enum only — no link dep)
#include "../sim/ml_ser.h" // ml_sha256_hex (oracle/qjs/sha256.c by path)

// The CURRENT format version. One number: the header literal is built from
// it and every block gate below compares against it.
#define FP_VERSION 7
#define FP_FILE "mlfk-persist.dat"
#define FP_TMP "mlfk-persist.tmp"
#define FP_DEFAULT_DIR "/mnt/mlfk-data"
// 78 lines, ~1.8 KB canonical — anything larger is not ours.
#define FP_CAP 4096
// MLFKPERSIST2's ctlstyle domain was {0 normal, 1 box} — CTL_STYLE_NATURAL
// did not exist yet. FROZEN: never re-point this at CTL_STYLE_COUNT.
#define FP_V2_STYLES 2
#define FP_NEG1_BITS UINT64_C(0xbff0000000000000)
// matchTimer cap (targetplay.js:282: capped < 6000 seconds)
#define FP_TIME_CAP 6000.0
// The logical canvas, on its longer axis — the bound the CSS hand and the
// CPU knobs are clamped to (foh_hand_step's `w`/`h`, css_rail_x0's rail).
// Written as a max rather than as RAST_W so it stays true if the raster ever
// stops being square; both axes share the one FP_DOM_SCREEN domain.
#define FP_SCREEN_MAX ((double)(RAST_W > RAST_H ? RAST_W : RAST_H))

const char *foh_persist_dir(void) {
  static const char *dir = 0;
  if (!dir) {
    const char *env = getenv("MLFK_PERSIST_DIR");
    dir = (env && env[0]) ? env : FP_DEFAULT_DIR;
  }
  return dir;
}

void foh_persist_defaults(FohPersist *p) {
  memset(p, 0, sizeof *p); // settings.js:44-56 — all zero, EXCEPT:
  // Owner ruling 2026-07-29: Natural is the FRESH-INSTALL default. It is
  // assigned explicitly because CTL_STYLE_NORMAL (not NATURAL) is the
  // zero value — the enum numbers are frozen so MLFKPERSIST2 saves keep
  // their scheme across the v3 bump (ctl_style.h).
  p->ctlStyle = (int)CTL_STYLE_DEFAULT;
  // D30 (A30(a), owner 2026-08-23): "box is good but L should be shield and
  // R should be mod / tilt". The Mod shoulder is an ORTHOGONAL cell, not a
  // scheme (ctl_style.h:69-77), and swapping is a pure RELABELING that leaves
  // the ratified BOX table untouched. THIS line is the fresh-install default
  // the player actually gets — ctl_style.c's initializer is overwritten by
  // foh_persist_load(), so without this D30 is inert.
  p->modOnR = 1; // D30: shield on L, Mod on R
  // v4 (MENU-SPEC §3/§4): the three options defaults that are NOT zero.
  // phantomThreshold is ON THE CHECKSUM SURFACE — zeroing it silently
  // flips physics, which is exactly the qjs getCookie defect (CLAUDE.md
  // M0 task 6) — so it is assigned explicitly, never left to the memset.
  // The other four v4 keys ARE zero upstream (settings.js).
  p->phantomThreshold = 0.01; // settings.js:50
  p->masterVolume[0] = 0.5;   // audiomenu.js:13 (sounds)
  p->masterVolume[1] = 0.3;   // audiomenu.js:13 (music)
  for (int c = 0; c < FOH_PERSIST_CHARS; c++) {
    for (int s = 0; s < FOH_PERSIST_TSTAGES; s++) {
      p->targetRecords[c][s] = -1.0; // targetplay.js:40
    }
  }
  // v5 (fix_plan A31): the IDENTITY binding on every port. Not left to the
  // memset for the same reason ctlStyle is not — zero is a legal SLOT
  // value, so a memset would put action 0 on all eight buttons, which is
  // not a permutation at all.
  for (int k = 0; k < CTL_BIND_PORTS; k++) {
    for (int i = 0; i < (int)CTL_BTN_COUNT; i++) p->bind[k][i] = i;
  }
  // v6 (fix_plan A49; DEVIATION D45): the CSS selection. Marth (0) on every
  // port, which is BOTH upstream's fresh state (characterSelections is
  // `[0,0,0,0]`, main.js:59) and foh_init's — so this line is what the
  // memset already gives and is written out anyway, because a default that
  // is only true by accident of being zero is the one that breaks silently
  // when the roster order changes. Same argument ctlStyle carries above.
  for (int k = 0; k < FOH_CSS_PORTS; k++) p->selChar[k] = 0;
  // v7 (fix_plan A26; DEVIATION D53): NO resume armed. Written out for the
  // same reason as selChar — a fresh install must never send the player
  // anywhere but the boot screen, and that must not be true only because
  // FOH_STARTUP happens to be the zero of the enum.
  p->resumeScreen = (int)FOH_STARTUP;
  // ticket #25 (owner ruling 2026-08-27): the CSS machine plane.
  //
  // EVERY VALUE HERE IS foh.h's CSS COLD-START PLANE, ASKED RATHER THAN
  // RETYPED. foh_persist_apply overwrites FohState with this record on every
  // boot, so a machine that has never been saved must get back EXACTLY what
  // foh_init gives it — and the way that promise breaks is not a wrong
  // constant, it is a right constant copied into a second place and then
  // edited in only one of them (CONTEXT.md). There is one definition of each
  // of these and foh.c asks the same one; the provenance citations live
  // there with them.
  for (int k = 0; k < FOH_CSS_PORTS; k++) {
    p->portType[k] = foh_css_type_home(k);
    p->cpuDifficulty[k] = FOH_CSS_DIFF_HOME;
    p->cssSliderX[k] = foh_css_slider_home(k);
  }
  p->versusMode = 0; // stock (main.js:140) — upstream's own `= 0`
  p->cssHand[0] = FOH_CSS_HAND_HOME_X;
  p->cssHand[1] = FOH_CSS_HAND_HOME_Y;
  p->cssCarry = -1;    // holding nothing (css.js:68)
  p->cssCpuCarry = -1; // holding nothing (css.js:75)
  p->cssHandType = 0;  // handPoint (css.js:63)
  // ticket #26: the target-select view plane. Both values are what an ENTRY
  // to that screen produces — `tssCursor = 0` is upstream's own
  // targetPointerPos reset (menu.js:77-84, carried at foh.c's TARGETTEST
  // arm) and page 0 is the AUTHORED grid, which is the only page upstream
  // opens on. Written out rather than left to the memset for the reason
  // every default above is: a value that is right only because it happens
  // to be zero breaks silently the day the zero moves.
  p->tssCursor = 0;
  p->tssPage = 0;
  // ...and the hand at slot 0's centre, which is the same entry (foh.h's
  // FOH_TSS_HOME_{X,Y}, asked rather than retyped — the ticket #25 rule).
  // The two must agree: the hand writes the cursor every frame, so a
  // fresh install whose hand and cursor disagreed would correct itself on
  // the first tick and make one of these two lines a lie.
  p->tssHand[0] = FOH_TSS_HOME_X;
  p->tssHand[1] = FOH_TSS_HOME_Y;
}

// A26/D53 + ticket #26. The contract, and the reason for every non-identity
// row, is at foh_persist.h. This function is BOTH the driver's mapping and
// the file's domain check, so the two can never disagree by editing.
//
// TICKET #26 WIDENED IT TO TWO COLUMNS RATHER THAN ADDING A SECOND MAP.
// Every arm now names a resume TARGET and a resume HOOK — the procedure that
// re-derives what a field table cannot carry (ADR 0001's kept half). The
// switch is still enumerated screen by screen with nothing defaulted and
// `case FOH_SCREEN_COUNT:` closing it, so a new screen is still a compile
// error HERE and its author must answer both questions in the same row.
// That property is the reason the hook lives here instead of in a table of
// its own: it is the one place that must think about a new screen, and its
// exhaustiveness has already caught a real gap when two lanes merged.
//
// `MAP(to, hk)` is only to keep an arm on one line; it introduces no
// indirection and hides no decision — every arm still states both columns.
#define MAP(to, hk) ((FohResumePlan){(to), (hk)})
FohResumePlan foh_persist_resume_plan(FohScreen sc) {
  switch (sc) {
    // out of scope / not a place: nothing is armed
    case FOH_STARTUP: return MAP(FOH_STARTUP, FOH_RESUME_HOOK_NONE);
    // mid-match is a separate serialization surface — record the screen the
    // match's own exit lands on instead (foh_dev.c's MEX_CSS/MEX_TSS arm)
    case FOH_MATCH: return MAP(FOH_CSS, FOH_RESUME_HOOK_NONE);
    // ...and a target match lands on target select, so it picks up that
    // screen's hook through the target it is redirected to — which is why
    // the driver asks for the plan of the screen it LANDED on, not of the
    // screen the file recorded. A target match resumed onto a stale slot
    // list would be the same defect one indirection further away.
    case FOH_TMATCH: return MAP(FOH_TSS, FOH_RESUME_HOOK_TSS_SLOTS);
    // THE STATED REASON FOR THIS ROW IS SPENT, AND THE ROW IS STILL HERE.
    // It read "launches with port types the CSS arms, and those are not
    // persisted" — true when it was written, false since ticket #25 (owner
    // ruling 2026-08-27) put `ptype` and `cpudiff` on the card. A stage
    // select restored now WOULD launch the configuration the player left.
    // Removing the redirect is ticket #27's, not this one's: it is a
    // behaviour change with its own resume trace to freeze, and doing it
    // here would ship it under a ticket that never judged it. What must not
    // happen is this comment continuing to assert the opposite of the code
    // it sits next to, which is the defect class that cost three days on
    // getConnected.
    case FOH_SSS: return MAP(FOH_CSS, FOH_RESUME_HOOK_NONE);
    // its reticle is placed by the entering transition, which a resume
    // never runs; this is the screen its own B-exit goes to (foh.c:1701).
    // ADR 0001 names the credits reticle as the OTHER candidate for a hook;
    // it does not get one, because the redirect already answers it — a
    // screen we never land on has nothing to re-derive. If ticket #27 or a
    // later arc makes the credits resumable, THIS is the row that has to
    // grow a hook, and the redirect is the thing standing in for one now.
    case FOH_CREDITS: return MAP(FOH_MENU_OPTIONS, FOH_RESUME_HOOK_NONE);
    // unreachable at FOH_NETPLAY 0 (foh.h); its B-exit is the menu top
    case FOH_MENU_BATTLE: return MAP(FOH_MENU_TOP, FOH_RESUME_HOOK_NONE);
    // A45-T4 sent the builder to the menu top, because it holds an UNSAVED
    // DOCUMENT that was not persisted and resuming INTO it would present an
    // empty editor and read as "my work is still here" when it was not.
    //
    // 2026-08-26: the document IS persisted now. FohTbuildOps.suspend
    // publishes it as `tbdoc.mlstage` through the same contract and the same
    // atomic publish the SD slots use, so the statement the old redirect
    // refused to make is now TRUE and the builder resumes into itself.
    //
    // THE REFUSAL IS NOT GONE, IT MOVED TO WHERE IT CAN BE CHECKED: this
    // function is a pure domain map, so it cannot know whether the write
    // SUCCEEDED. tdev_hibernate_check calls suspend() first and downgrades
    // this to FOH_MENU_TOP when it fails — which is why MENU_TOP must also
    // remain a legal value here, and it does (it maps to itself below).
    // The builder's document is restored by the driver's enter()+resume()
    // pair, which is the SHAPE this ticket's hook copies but not the same
    // mechanism: that restoration reads a file the suspend WROTE, so it is
    // a load, not a re-derivation, and it belongs to FohTbuildOps.
    case FOH_TBUILD: return MAP(FOH_TBUILD, FOH_RESUME_HOOK_NONE);
    // TARGET SELECT — THE ONE SCREEN WITH A HOOK (ticket #26). Its two
    // persisted fields (`tsscur`, `tsspage`) come back off the card like any
    // other row; its SLOT LIST cannot, because the list describes the card
    // itself and the card may have been changed while the machine was off.
    // So it is recomputed, once, after the fields land. It is listed here on
    // its own rather than in the group below for exactly that reason.
    case FOH_TSS: return MAP(FOH_TSS, FOH_RESUME_HOOK_TSS_SLOTS);
    // Everything else opens with NO entry-time initialisation beyond what
    // foh_init already gives (measured over every ev_trans site in foh.c:
    // the only entry arms that write state are TSS's — now the row above —
    // and the two excluded further up). Listed rather than defaulted so a
    // NEW screen is a compile error here — the one place that must think
    // about it, and since ticket #26 it must think about the hook too.
    case FOH_TITLE:
    case FOH_MENU_TOP:
    case FOH_MENU_OPTIONS:
    case FOH_MENU_CONTROLS:
    case FOH_CSS:
    case FOH_OPT_GAMEPLAY:
    case FOH_OPT_AUDIO:
    case FOH_CTRL_PAD:
    case FOH_CTRL_KEY: return MAP(sc, FOH_RESUME_HOOK_NONE);
    case FOH_SCREEN_COUNT: break;
  }
  return MAP(FOH_STARTUP, FOH_RESUME_HOOK_NONE);
}
#undef MAP

FohScreen foh_persist_resume_target(FohScreen sc) {
  return foh_persist_resume_plan(sc).target;
}

// --- THE FIELD TABLE (ticket #22 / ADR 0001) --------------------------------
//
// ONE declarative row per persisted field. The writer walks it, the reader
// walks it, and nothing else knows the file's shape. The contract, the
// version policy and the reason a table beat a per-screen save/load pair
// are in foh_persist.h and docs/adr/0001-*.md; what follows is only how.
//
// The bytes a row produces are fully determined by its columns:
//
//     <key>' '<idx0>' '<idx1>' '<val0>' '<val1>…<valN-1>'\n'
//
// with `dims` index columns (0, 1 or 2) and `vals` values per line, and
// `rows` lines in total — so `rec` is dims=2/vals=1/50 lines, `bind` is
// dims=1/vals=8/4 lines, `tapjump` is dims=0/vals=4/1 line. Every line is
// FIXED WIDTH, which is what keeps the parse anchored rather than
// tokenising: a value is at a computed offset or the file is corrupt.

typedef enum {
  FP_FLAG = 0, // one decimal digit into an int cell
  FP_U2,       // exactly two decimal digits into an int cell
  FP_HEX64,    // 16 lowercase hex digits = one IEEE-754 double, bit-exact
  // Present in FohPersist, NEVER in the file: RECONSTRUCTED after the
  // fields land rather than copied out of it. This is the kind a
  // pointer-valued field takes (ADR 0001) — a stored address is valid only
  // while the binary is unchanged, so restoring one is a trap. There are
  // none today; the kind exists so the first one has a place to go that is
  // not "copy the bytes".
  FP_RECON
} FpKind;

// Per-value (or, for FP_DOM_PERM, per-line) domain rules. An out-of-domain
// value is CORRUPTION and resets loudly; it is never clamped and never
// quietly repaired. That is the qjs getCookie lesson (CLAUDE.md M0 task 6)
// inverted for our surface, and it is why these are columns rather than
// something the caller is trusted to have checked.
typedef enum {
  FP_DOM_NONE = 0,
  FP_DOM_REC,     // the -1.0 sentinel exactly, or finite in [0, 6000)
  FP_DOM_PHANTOM, // finite, non-negative, <= 1000.0 (the checksum surface)
  FP_DOM_UNIT,    // finite, non-negative, <= 1.0 (audiomenu's clamp)
  FP_DOM_PERM,    // the LINE's values are a permutation of 0..vals-1
  FP_DOM_RESUME,  // a screen foh_persist_resume_target() maps to itself
  FP_DOM_SCREEN   // finite, non-negative, <= FP_SCREEN_MAX (a canvas coord)
} FpDomain;

typedef struct {
  const char *key; // the line's leading token, e.g. "rec"
  FpKind kind;
  size_t off;   // offsetof(FohPersist, <field>)   — set by FP_ROW
  size_t bytes; // sizeof the whole field          — set by FP_ROW
  int dims;     // index columns printed before the values (0, 1, 2)
  int d0, d1;   // index extents; rows = dims?(dims==1?d0:d0*d1):1
  // FROZEN index-digit grammar: the index digit must satisfy 0 <= d < ixN
  // or the line is `grammar`. It is NOT d0/d1 for `rec`, whose historical
  // grammar accepts any decimal digit and reports an out-of-range index as
  // `order` — the distinction is carried, not tidied away.
  int ix0, ix1;
  int vals;   // values per line
  int dmax;   // FP_FLAG: the digit domain is [0, dmax)
  int dmaxV2; // FROZEN historical override used ONLY when ver == 2
  // INT KINDS ONLY. The file column is an UNSIGNED decimal digit, but some
  // fields are not: playerType's own domain is {-1, 0, 1} and the CPU level's
  // is 1..4. `wireBias` is what the field's value is SHIFTED BY to become the
  // column, in ONE place — file digit == value + wireBias, value == digit -
  // wireBias — so FohPersist keeps the field's real value, the machine glue
  // stays a plain copy, and the encoding lives with the format instead of
  // being re-derived at each end. Zero for every row that does not need it.
  int wireBias;
  FpDomain dom;
  // FP_FLAG only: an out-of-range digit reports `domain` instead of
  // `grammar`. One row (`sel`) has always done so; the detail token is part
  // of the stderr grammar check-device-persist.sh strict-parses, so it is a
  // column rather than a judgement call.
  bool rangeIsDomain;
  int since; // the lowest file version that carries this row
  // What the field becomes when the row is ABSENT (an older version that
  // predates it, or — at the current version — a file written before it
  // existed). `hasAbsent` false means "keep whatever foh_persist_defaults()
  // put there", which is the answer for every row but two.
  int absent;
  bool hasAbsent;
} FpField;

// The table, in FILE ORDER. Appending a field is exactly one row here.
//
// `since` is what retired the six migration arms: a v1 file has no row with
// since > 1, so those fields take their absent value and the SAME parse
// serves every version. The two `absent` values that are not the
// fresh-install default are the reviewed migration rulings, verbatim:
// ctlstyle -> BOX (a v1 file can only have come from the build whose one
// mapping was the ratified S1 == BOX) and modonr -> 1 (D29/D30's ratified
// arrangement). Their arguments are at foh_persist.h.
#define FP_FIELDS(X)                                                           \
  X(turbo, .key = "turbo", .kind = FP_FLAG, .vals = 1, .dmax = 2, .since = 1)  \
  X(lCancelType, .key = "lcancel", .kind = FP_FLAG, .vals = 1, .dmax = 3,      \
    .since = 1)                                                                \
  X(tapJumpOff, .key = "tapjump", .kind = FP_FLAG, .vals = 4, .dmax = 2,       \
    .since = 1)                                                                \
  X(ctlStyle, .key = "ctlstyle", .kind = FP_FLAG, .vals = 1,                   \
    .dmax = (int)CTL_STYLE_COUNT, .dmaxV2 = FP_V2_STYLES, .since = 2,          \
    .absent = (int)CTL_STYLE_BOX, .hasAbsent = true)                           \
  X(modOnR, .key = "modonr", .kind = FP_FLAG, .vals = 1, .dmax = 2,            \
    .since = 3, .absent = 1, .hasAbsent = true)                                \
  X(targetRecords, .key = "rec", .kind = FP_HEX64, .dims = 2,                  \
    .d0 = FOH_PERSIST_CHARS, .d1 = FOH_PERSIST_TSTAGES, .ix0 = 10, .ix1 = 10,  \
    .vals = 1, .dom = FP_DOM_REC, .since = 1)                                  \
  X(flashOnLCancel, .key = "flash", .kind = FP_FLAG, .vals = 1, .dmax = 2,     \
    .since = 4)                                                                \
  X(everyCharWallJump, .key = "walljump", .kind = FP_FLAG, .vals = 1,          \
    .dmax = 2, .since = 4)                                                     \
  X(blastzoneWrapping, .key = "blastzone", .kind = FP_FLAG, .vals = 1,         \
    .dmax = 2, .since = 4)                                                     \
  X(dustLessPerfectWavedash, .key = "dustless", .kind = FP_FLAG, .vals = 1,    \
    .dmax = 2, .since = 4)                                                     \
  X(phantomThreshold, .key = "phantom", .kind = FP_HEX64, .vals = 1,           \
    .dom = FP_DOM_PHANTOM, .since = 4)                                         \
  X(masterVolume[0], .key = "soundslevel", .kind = FP_HEX64, .vals = 1,        \
    .dom = FP_DOM_UNIT, .since = 4)                                            \
  X(masterVolume[1], .key = "musiclevel", .kind = FP_HEX64, .vals = 1,         \
    .dom = FP_DOM_UNIT, .since = 4)                                            \
  X(bind, .key = "bind", .kind = FP_FLAG, .dims = 1, .d0 = CTL_BIND_PORTS,     \
    .ix0 = CTL_BIND_PORTS, .vals = (int)CTL_BTN_COUNT,                         \
    .dmax = (int)CTL_BTN_COUNT, .dom = FP_DOM_PERM, .since = 5)                \
  X(selChar, .key = "sel", .kind = FP_FLAG, .vals = FOH_CSS_PORTS,             \
    .dmax = FOH_PERSIST_CHARS, .rangeIsDomain = true, .since = 6)              \
  X(resumeScreen, .key = "resume", .kind = FP_U2, .vals = 1,                   \
    .dom = FP_DOM_RESUME, .since = 7)                                          \
  X(portType, .key = "ptype", .kind = FP_FLAG, .vals = FOH_CSS_PORTS,          \
    .dmax = 3, .wireBias = 1, .since = 7)                                      \
  X(cpuDifficulty, .key = "cpudiff", .kind = FP_FLAG, .vals = FOH_CSS_PORTS,   \
    .dmax = 4, .wireBias = -1, .since = 7)                                     \
  X(versusMode, .key = "vsmode", .kind = FP_FLAG, .vals = 1, .dmax = 2,        \
    .since = 7)                                                                \
  X(cssHand, .key = "hand", .kind = FP_HEX64, .vals = 2,                       \
    .dom = FP_DOM_SCREEN, .since = 7)                                          \
  X(cssSliderX, .key = "slider", .kind = FP_HEX64, .vals = FOH_CSS_PORTS,      \
    .dom = FP_DOM_SCREEN, .since = 7)                                          \
  X(cssCarry, .key = "carry", .kind = FP_FLAG, .vals = 1,                      \
    .dmax = FOH_CSS_PORTS + 1, .wireBias = 1, .since = 7)                      \
  X(cssCpuCarry, .key = "cpucarry", .kind = FP_FLAG, .vals = 1,                \
    .dmax = FOH_CSS_PORTS + 1, .wireBias = 1, .since = 7)                      \
  X(cssHandType, .key = "handtype", .kind = FP_FLAG, .vals = 1, .dmax = 3,     \
    .since = 7)                                                                \
  X(tssCursor, .key = "tsscur", .kind = FP_U2, .vals = 1,                      \
    .dmax = FOH_TSS_SLOTS, .since = 7)                                         \
  X(tssPage, .key = "tsspage", .kind = FP_FLAG, .vals = 1, .dmax = 2,          \
    .since = 7)                                                                \
  X(tssHand, .key = "tsshand", .kind = FP_HEX64, .vals = 2,                    \
    .dom = FP_DOM_SCREEN, .since = 7)

#define FP_ROW(nm, ...)                                                        \
  {.off = offsetof(FohPersist, nm),                                            \
   .bytes = sizeof(((FohPersist *)0)->nm),                                     \
   __VA_ARGS__},
static const FpField FP_TABLE[] = {FP_FIELDS(FP_ROW)};
#define FP_COUNT ((int)(sizeof FP_TABLE / sizeof FP_TABLE[0]))

// THE GUARD THE ADR IS FOR. The table's byte total is derived from the SAME
// list the table is, so the two cannot drift, and it is compared against
// sizeof(FohPersist) at COMPILE TIME. Add a field to FohPersist and this
// stops holding: the author must then add a row (one line, above) or raise
// FP_UNPERSISTED_BYTES with a comment saying why the field is deliberately
// not persisted. "Someone must remember" becomes "the build does not
// compile" — the same mechanism the capacity caps and the exhaustive
// resume-target map already use.
#define FP_ROW_BYTES(nm, ...) +sizeof(((FohPersist *)0)->nm)
enum { FP_TABLE_BYTES = 0 FP_FIELDS(FP_ROW_BYTES) };

// EVERY deliberately-unpersisted byte of FohPersist, declared in ONE place.
// Today that is exactly `layoutGuard` — four bytes that exist to make the
// assertion below an equality with no slack (foh_persist.h explains why the
// tail padding would otherwise swallow the next added int in silence).
// A pointer-valued field would be declared here too, with a note naming the
// code that RECONSTRUCTS it; it is never copied out of the file.
//
// INTERNAL ALIGNMENT PADDING is declared here as well, and there is none
// today (MEASURED: the eight leading ints are exactly 32 bytes, so the
// doubles that follow need no gap; ticket #25's CSS block is placed to keep
// that true of its own two double arrays, and foh_persist.h says so where
// the block starts). A field that RAISES the struct's alignment — a double or a
// pointer placed among the ints — opens a hole that belongs in this number
// with a comment saying where it is. That is a real cost and it is meant to
// be visible; it is not a reason to reorder the struct behind the reader's
// back — but WHERE A NEW MEMBER GOES among its own siblings is a free
// choice, and choosing it so that no hole opens is not the same thing.
#define FP_UNPERSISTED_BYTES (sizeof(int) /* layoutGuard */)

// ROUNDED UP TO THE STRUCT'S OWN ALIGNMENT, and the rounding is not a
// loosening — it is what makes the guard cost EXACTLY ONE ROW.
//
// MEASURED (2026-08-27, RE-MEASURED after ticket #26's two rows): the
// persisted fields are 724 bytes and layoutGuard makes 728, which is already
// a multiple of 8, so today the rounding does nothing. But the int count is
// odd (63), so the NEXT int field takes the total
// to 724 and the compiler pads the struct to 728. Asserting raw equality
// would then fail even though the author did everything right — the field
// AND its row — and the fix would be to bump FP_UNPERSISTED_BYTES, i.e. to
// pay for a field twice and to grow the "deliberately not persisted"
// number for a field that IS persisted. That is a guard that trains people
// to edit it, which is the one thing it must not do.
//
// Rounding to _Alignof keeps it exact where exactness is the point: with
// layoutGuard present the members already fill the struct, so there is no
// slack for a field to hide in, and any addition WITHOUT a row still moves
// sizeof past the rounded total. check-persist-table.sh leg [9] proves both
// halves — an added int and an added double each fail the build here, and
// the same int becomes invisible the moment layoutGuard is removed.
#define FP_ALIGN_UP(n, a) ((((n) + (a) - 1) / (a)) * (a))

_Static_assert(sizeof(FohPersist) ==
                   FP_ALIGN_UP(FP_TABLE_BYTES + FP_UNPERSISTED_BYTES,
                               _Alignof(FohPersist)),
               "FohPersist changed size. Persistence is a FIELD TABLE: add "
               "the new field to FP_FIELDS (one row), or add its size to "
               "FP_UNPERSISTED_BYTES with a comment saying why it is "
               "deliberately not persisted (a pointer is reconstructed, "
               "never copied). Do not just move this number.");

// The `resume` row is two decimal digits. FOH_SCREEN_COUNT is 17 and the
// enum may grow; this used to be a runtime gfx_fatal, which is a strange
// place to learn about a compile-time fact.
_Static_assert((int)FOH_SCREEN_COUNT <= 100,
               "FohScreen outgrew the 2-digit resume row — widen the row "
               "format before growing FohScreen");

// --- table-driven canonical serialization (deterministic bytes) -------------

static uint64_t fp_bits(double d) {
  uint64_t b;
  memcpy(&b, &d, 8);
  return b;
}

// Domain guard for the v4 block's three doubles: finite, non-negative
// (signbit rejects -0.0 too) and <= hi. The qjs getCookie defect class
// (CLAUDE.md M0 task 6) is exactly a silently out-of-domain settings
// value, so these are checked, never trusted.
static bool fp_in_range(double d, double hi) {
  return isfinite(d) && !signbit(d) && d >= 0.0 && d <= hi;
}

static double fp_double(uint64_t b) {
  double d;
  memcpy(&d, &b, 8);
  return d;
}

// The widest line any row may hold. fp_parse_field keeps one line's values
// in a local of this size because the permutation domain needs them
// together; fp_table_check refuses a row that would overrun it.
#define FP_MAX_VALS 16

static int fp_rows(const FpField *f) {
  return f->dims == 0 ? 1 : (f->dims == 1 ? f->d0 : f->d0 * f->d1);
}

// Characters one value occupies on the line.
static int fp_width(FpKind k) {
  return k == FP_HEX64 ? 16 : (k == FP_U2 ? 2 : 1);
}

static size_t fp_stride(FpKind k) {
  return k == FP_HEX64 ? sizeof(double) : sizeof(int);
}

// The COLUMN's value bound for an integer row: legal values are [0, cap).
//
// FP_FLAG: `dmax` is MANDATORY and is at most 10 — one decimal digit is the
// whole column, so the domain and the width are the same fact.
//
// FP_U2: two digits hold 100 values, and `dmax` is OPTIONAL. 0 means "the
// whole column", which is what `resume` takes — its domain is a FUNCTION
// (FP_DOM_RESUME, which is not an interval) and giving it a redundant bound
// would be a second statement of the same rule, i.e. a thing that can drift.
// `tsscur` states one, because eleven slots is exactly the case the FP_U2
// arm's own comment anticipated when it said "no row reaches it today; it is
// here so that the first one that could cannot do it silently".
static int fp_dcap(const FpField *f) {
  if (f->kind == FP_FLAG) return f->dmax;
  return f->dmax ? f->dmax : 100;
}

// The exact byte length of one of f's lines, trailing LF included.
static size_t fp_line_len(const FpField *f) {
  return strlen(f->key) + 1 + 2 * (size_t)f->dims +
         (size_t)f->vals * (size_t)(fp_width(f->kind) + 1);
}

// Address of one flat cell. `e` is the element index, L * vals + i, which
// is exactly the row-major index of the field's own array — so `rec`'s
// (c, s) and `bind`'s (port, phys) land where the C declaration puts them.
static void *fp_cell(const FohPersist *p, const FpField *f, int e) {
  return (void *)((char *)p + f->off + (size_t)e * fp_stride(f->kind));
}

// The table is data, and data can be wrong in ways review does not catch
// (ADR 0001's "Bad"). This runs on every serialize and every load: it is a
// handful of integer comparisons, and it turns a mistyped column into a
// loud death at the chokepoint instead of a file nobody can read.
static void fp_table_check(void) {
  size_t total = 0;
  for (int j = 0; j < FP_COUNT; j++) {
    const FpField *f = &FP_TABLE[j];
    total += f->bytes;
    if (!f->key || !f->key[0]) gfx_fatal("foh_persist: table row has no key");
    for (const char *c = f->key; *c; c++) {
      if (*c < 'a' || *c > 'z') gfx_fatal("foh_persist: table key not [a-z]");
    }
    for (int k = 0; k < j; k++) {
      if (strcmp(FP_TABLE[k].key, f->key) == 0) {
        gfx_fatal("foh_persist: duplicate table key");
      }
    }
    if (f->since < 1 || f->since > FP_VERSION) {
      gfx_fatal("foh_persist: table row `since` outside 1..FP_VERSION");
    }
    if (f->kind == FP_RECON) {
      // never in the file, so it may not claim any of the file's shape
      if (f->dims || f->vals || f->dom || f->hasAbsent || f->wireBias) {
        gfx_fatal("foh_persist: a reconstructed row claims file shape");
      }
      continue;
    }
    // `wireBias` shifts an INT cell into its unsigned column, so it can only
    // sit on an int kind, it cannot coexist with a domain that reads the
    // digits as values (FP_DOM_PERM's slot numbers are the permutation), and
    // an absent value must still land inside the column it would be written
    // to — otherwise a migration would produce a file this build refuses.
    if (f->wireBias != 0) {
      if (f->kind != FP_FLAG && f->kind != FP_U2) {
        gfx_fatal("foh_persist: wireBias on a non-integer row");
      }
      if (f->dom == FP_DOM_PERM) {
        gfx_fatal("foh_persist: wireBias on a permutation row");
      }
    }
    // Both INT kinds, not just FP_FLAG: FP_U2 rows can carry a bounded
    // domain since ticket #26, so the same rule has to reach them or the
    // first two-digit migration default would go unchecked.
    if ((f->kind == FP_FLAG || f->kind == FP_U2) && f->hasAbsent) {
      const int d = f->absent + f->wireBias;
      if (d < 0 || d >= fp_dcap(f)) {
        gfx_fatal("foh_persist: a row's absent value is outside its column");
      }
    }
    if (f->dims < 0 || f->dims > 2 || f->vals < 1) {
      gfx_fatal("foh_persist: table row shape out of range");
    }
    if (f->dims >= 1 && (f->d0 < 1 || f->ix0 < 1 || f->ix0 > 10)) {
      gfx_fatal("foh_persist: table row index-0 extents out of range");
    }
    if (f->dims == 2 && (f->d1 < 1 || f->ix1 < 1 || f->ix1 > 10)) {
      gfx_fatal("foh_persist: table row index-1 extents out of range");
    }
    if (f->kind == FP_FLAG && (f->dmax < 1 || f->dmax > 10)) {
      gfx_fatal("foh_persist: a flag row's digit domain is not one digit");
    }
    // FP_U2's `dmax` is optional (0 == the whole two-digit column), but a
    // stated one must still FIT in two digits, or the row would demand
    // values the format cannot print.
    if (f->kind == FP_U2 && (f->dmax < 0 || f->dmax > 100)) {
      gfx_fatal("foh_persist: a two-digit row's domain is not two digits");
    }
    // fp_parse_field holds one LINE's values in a fixed local (the
    // permutation check needs them together); the bound is asserted here so
    // a too-wide row is caught before any file is opened, not mid-parse.
    if (f->vals > FP_MAX_VALS) {
      gfx_fatal("foh_persist: a table row has more values than a line holds");
    }
    if (f->hasAbsent && f->kind == FP_HEX64) {
      gfx_fatal("foh_persist: an absent value is an int, not a double");
    }
    // THE ROW MUST COVER ITS FIELD, EXACTLY. This is the check that catches
    // a row pointed at the wrong member, or a `vals` that does not match
    // the array it serialises — the offset-table defect class by name.
    if ((size_t)fp_rows(f) * (size_t)f->vals * fp_stride(f->kind) != f->bytes) {
      gfx_fatal("foh_persist: a table row does not cover its field exactly");
    }
  }
  if (total != (size_t)FP_TABLE_BYTES) {
    gfx_fatal("foh_persist: table byte total disagrees with FP_TABLE_BYTES");
  }
}

// Appends one printf-formatted piece, with the buffer contract enforced.
// Loud death on overflow (structurally impossible for the fixed shape).
static size_t fp_addf(char *buf, size_t cap, size_t n, const char *fmt, ...)
    __attribute__((format(printf, 4, 5)));
static size_t fp_addf(char *buf, size_t cap, size_t n, const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  const int w = vsnprintf(buf + n, cap - n, fmt, ap);
  va_end(ap);
  if (w < 0 || (size_t)w >= cap - n) {
    gfx_fatal("foh_persist: serialize overflow");
  }
  return n + (size_t)w;
}

// Emits the full canonical file (SUM line included) into buf; returns
// the byte length. The ONLY writer.
static size_t fp_serialize(const FohPersist *p, char *buf, size_t cap) {
  fp_table_check();
  size_t n = fp_addf(buf, cap, 0, "MLFKPERSIST%d\n", FP_VERSION);
  for (int j = 0; j < FP_COUNT; j++) {
    const FpField *f = &FP_TABLE[j];
    if (f->kind == FP_RECON) continue; // reconstructed, never written
    const int rows = fp_rows(f);
    for (int L = 0; L < rows; L++) {
      n = fp_addf(buf, cap, n, "%s", f->key);
      if (f->dims >= 1) {
        n = fp_addf(buf, cap, n, " %d", f->dims == 2 ? L / f->d1 : L);
      }
      if (f->dims == 2) n = fp_addf(buf, cap, n, " %d", L % f->d1);
      for (int i = 0; i < f->vals; i++) {
        const void *cell = fp_cell(p, f, L * f->vals + i);
        switch (f->kind) {
          case FP_FLAG: {
            // A VALUE THIS BUILD WOULD REFUSE TO READ MUST NEVER BE WRITTEN.
            // That was already the rule for FP_DOM_RESUME below; it is the
            // rule for every flag column, because the alternative is a file
            // that saves cleanly and resets loudly on the next boot, which
            // reads to the player as "it lost my settings" with nothing in
            // the log naming the write that did it. FohState.portType has a
            // NET(2) value in its domain (DEVIATION D5) that this build can
            // never reach, and this is where reaching it would be caught.
            const int d = *(const int *)cell + f->wireBias;
            if (d < 0 || d >= f->dmax) {
              gfx_fatal("foh_persist: field value outside its file column");
            }
            n = fp_addf(buf, cap, n, " %d", d);
            break;
          }
          case FP_U2: {
            // A screen this build would refuse to RESTORE must never be
            // WRITTEN either — the domain is one function, checked on both
            // sides of the file. (The only FP_U2 row is `resume`; the
            // guard is the row's, not the writer's, so it moves with it.)
            if (f->dom == FP_DOM_RESUME) {
              const int sc = *(const int *)cell;
              if (foh_persist_resume_target((FohScreen)sc) != (FohScreen)sc) {
                gfx_fatal("foh_persist: resumeScreen is not a resume target");
              }
            }
            // …and the column's own bound, for the same reason the flag arm
            // above has one: TWO decimal digits is the whole width, so a
            // value outside [0,99] would produce a line the reader cannot
            // parse. TICKET #26 REACHED IT: `tsscur` narrows the bound to
            // its own eleven slots through fp_dcap(), which is the "first
            // row that could" this comment used to be waiting for. (Named
            // `nn` for the format's own `<NN>`, and NOT `d` like the flag
            // arm above: the two lines would otherwise be textually
            // identical, and check-rebind.sh's T5 perturbs one of them BY
            // EXACT LINE and hard-fails on an ambiguous anchor.)
            const int nn = *(const int *)cell + f->wireBias;
            if (nn < 0 || nn >= fp_dcap(f)) {
              gfx_fatal("foh_persist: field value outside its file column");
            }
            n = fp_addf(buf, cap, n, " %02d", nn);
            break;
          }
          case FP_HEX64:
            n = fp_addf(buf, cap, n, " %016llx",
                        (unsigned long long)fp_bits(*(const double *)cell));
            break;
          case FP_RECON: break; // unreachable: skipped above
        }
      }
      n = fp_addf(buf, cap, n, "\n");
    }
  }
  char hex[65];
  ml_sha256_hex(buf, n, hex);
  return fp_addf(buf, cap, n, "SUM %s\n", hex);
}

// --- strict load ------------------------------------------------------------

static FohPersistStatus fp_reset(FohPersist *p, FohPersistStatus st,
                                 const char *detail) {
  foh_persist_defaults(p);
  switch (st) {
    case FOH_PERSIST_RESET_MISSING:
      fprintf(stderr, "foh_persist: reset cause=missing\n");
      break;
    case FOH_PERSIST_RESET_VERSION:
      fprintf(stderr, "foh_persist: reset cause=version\n");
      break;
    case FOH_PERSIST_RESET_CORRUPT:
      fprintf(stderr, "foh_persist: reset cause=corrupt detail=%s\n", detail);
      break;
    default: gfx_fatal("foh_persist: bad reset status");
  }
  return st;
}

static bool fp_is_hex16(const char *s) {
  for (int k = 0; k < 16; k++) {
    const char c = s[k];
    if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'))) return false;
  }
  return true;
}

static uint64_t fp_parse_hex16(const char *s) {
  uint64_t v = 0;
  for (int k = 0; k < 16; k++) {
    const char c = s[k];
    v <<= 4;
    v |= (uint64_t)(c <= '9' ? c - '0' : c - 'a' + 10);
  }
  return v;
}

// The parse cursor. `detail` names the reset detail on failure and is the
// ONLY way a row reports one, so the stderr grammar has a single source.
typedef struct {
  const char *buf;
  size_t sumStart;
  size_t pos;
  int ver;
  const char *detail;
} FpParse;

static bool fp_die(FpParse *ps, const char *detail) {
  ps->detail = detail;
  return false;
}

// Assigns the field's ABSENT value — the row is not in this file, either
// because the version predates it or because the writer did not know it.
static void fp_absent(const FpField *f, FohPersist *v) {
  if (f->kind == FP_RECON || !f->hasAbsent) return; // keep the default
  const int rows = fp_rows(f);
  for (int e = 0; e < rows * f->vals; e++) *(int *)fp_cell(v, f, e) = f->absent;
}

// Parses ALL of f's lines at ps->pos, strictly and anchored: fixed width,
// exact key, exact separators, index progression by POSITION, then each
// value's own shape and domain. Every failure names its detail.
static bool fp_parse_field(FpParse *ps, const FpField *f, FohPersist *v) {
  const size_t klen = strlen(f->key);
  const int w = fp_width(f->kind);
  const size_t need = fp_line_len(f);
  // MLFKPERSIST2 predates CTL_STYLE_NATURAL, so its ctlstyle domain was
  // {0,1}: each version is validated against ITS OWN grammar (review-ctl
  // n1), and accepting a resealed v2 file that says `ctlstyle 2` would
  // install a state no v2 writer could ever have produced.
  const int dmax = (ps->ver == 2 && f->dmaxV2) ? f->dmaxV2 : f->dmax;
  const int rows = fp_rows(f);
  const int ixmax[2] = {f->ix0, f->ix1};
  for (int L = 0; L < rows; L++) {
    const size_t p0 = ps->pos;
    if (ps->sumStart - p0 < need || memcmp(ps->buf + p0, f->key, klen) != 0 ||
        ps->buf[p0 + klen] != ' ' || ps->buf[p0 + need - 1] != '\n') {
      return fp_die(ps, "grammar");
    }
    size_t q = p0 + klen + 1;
    int idx[2] = {0, 0};
    for (int d = 0; d < f->dims; d++) {
      const char c = ps->buf[q];
      if (c < '0' || c >= (char)('0' + ixmax[d])) return fp_die(ps, "grammar");
      if (ps->buf[q + 1] != ' ') return fp_die(ps, "grammar");
      idx[d] = c - '0';
      q += 2;
    }
    // the index columns are the row's own progression, asserted BY POSITION
    if (f->dims == 1 && idx[0] != L) return fp_die(ps, "order");
    if (f->dims == 2 && (idx[0] != L / f->d1 || idx[1] != L % f->d1)) {
      return fp_die(ps, "order");
    }
    int line[FP_MAX_VALS]; // FP_DOM_PERM's working set (fp_table_check bounds it)
    for (int i = 0; i < f->vals; i++) {
      if (ps->buf[q + w] != (i == f->vals - 1 ? '\n' : ' ')) {
        return fp_die(ps, "grammar");
      }
      void *cell = fp_cell(v, f, L * f->vals + i);
      switch (f->kind) {
        case FP_FLAG: {
          const char c = ps->buf[q];
          if (c < '0' || c >= (char)('0' + dmax)) {
            return fp_die(ps, f->rangeIsDomain ? "domain" : "grammar");
          }
          // `line` holds the COLUMN's digits, not the field's values —
          // FP_DOM_PERM judges the permutation in column space, and
          // fp_table_check forbids a bias on a permutation row so the two
          // can never mean different things at once.
          line[i] = c - '0';
          *(int *)cell = line[i] - f->wireBias;
          break;
        }
        case FP_U2: {
          const char a = ps->buf[q], b = ps->buf[q + 1];
          if (a < '0' || a > '9' || b < '0' || b > '9') {
            return fp_die(ps, "grammar");
          }
          line[i] = (a - '0') * 10 + (b - '0');
          if (f->dom == FP_DOM_RESUME) {
            // exactly the screens foh_persist_resume_target() maps to
            // themselves. A screen outside that set is corruption and is
            // never "repaired" to something nearby: a resume that puts the
            // player on the wrong screen is worse than no resume at all.
            if (line[i] >= (int)FOH_SCREEN_COUNT ||
                foh_persist_resume_target((FohScreen)line[i]) !=
                    (FohScreen)line[i]) {
              return fp_die(ps, "domain");
            }
          }
          // …and the row's own column bound (ticket #26): `tsscur` may name
          // eleven slots and nothing else. Two digits are always
          // GRAMMATICAL, so a value the screen cannot hold is `domain`, and
          // it resets loudly like every other out-of-domain value rather
          // than being clamped onto a slot the player never chose.
          if (line[i] >= fp_dcap(f)) return fp_die(ps, "domain");
          *(int *)cell = line[i] - f->wireBias;
          break;
        }
        case FP_HEX64: {
          if (!fp_is_hex16(ps->buf + q)) return fp_die(ps, "grammar");
          const uint64_t bits = fp_parse_hex16(ps->buf + q);
          const double d = fp_double(bits);
          switch (f->dom) {
            case FP_DOM_REC:
              if (bits != FP_NEG1_BITS &&
                  !(isfinite(d) && d >= 0.0 && d < FP_TIME_CAP)) {
                return fp_die(ps, "domain");
              }
              break;
            case FP_DOM_PHANTOM:
              if (!fp_in_range(d, 1000.0)) return fp_die(ps, "domain");
              break;
            case FP_DOM_UNIT:
              if (!fp_in_range(d, 1.0)) return fp_die(ps, "domain");
              break;
            case FP_DOM_SCREEN:
              // A canvas coordinate. The hand and the knobs are CLAMPED to
              // this box every frame, so a value outside it never came from
              // this program — and a restored cursor at NaN or at 1e300 hit-
              // tests nothing at all, which is a screen the player cannot
              // use rather than a screen that looks wrong.
              if (!fp_in_range(d, FP_SCREEN_MAX)) return fp_die(ps, "domain");
              break;
            default: break;
          }
          *(double *)cell = d;
          break;
        }
        case FP_RECON: gfx_fatal("foh_persist: parsed a reconstructed row");
      }
      q += (size_t)w + 1;
    }
    if (f->dom == FP_DOM_PERM) {
      // The row must be a PERMUTATION. A duplicate slot would silently
      // delete an action from the player's controller — the same class as
      // the qjs Number("")-zeroing defect, so it is corruption, never
      // something to repair quietly.
      bool seen[FP_MAX_VALS] = {false};
      for (int i = 0; i < f->vals; i++) {
        if (line[i] < 0 || line[i] >= f->vals || seen[line[i]]) {
          return fp_die(ps, "domain");
        }
        seen[line[i]] = true;
      }
    }
    ps->pos = q;
  }
  return true;
}

// An UNKNOWN row — a key this build has never heard of, which is what a
// file written by a LATER build looks like. It is skipped, not refused,
// and that is the whole of forward compatibility. It still has to LOOK
// like a row (`<lowercase key> <at least one byte>` then LF): skipping
// arbitrary bytes would be the silent-acceptance defect wearing a hat.
static bool fp_skip_unknown(FpParse *ps, size_t klen) {
  size_t q = ps->pos + klen + 1; // past the key and its space
  if (q >= ps->sumStart || ps->buf[q] == '\n') return fp_die(ps, "grammar");
  while (q < ps->sumStart && ps->buf[q] != '\n') q++;
  if (q >= ps->sumStart) return fp_die(ps, "grammar");
  ps->pos = q + 1;
  return true;
}

// The key token at ps->pos: [a-z]+ followed by a space. Returns its length,
// or 0 when the line does not begin like a row at all.
#define FP_KEY_MAX 32
static size_t fp_key_len(const FpParse *ps) {
  size_t k = 0;
  while (ps->pos + k < ps->sumStart && k < FP_KEY_MAX) {
    const char c = ps->buf[ps->pos + k];
    if (c < 'a' || c > 'z') break;
    k++;
  }
  if (k == 0 || k >= FP_KEY_MAX) return 0;
  if (ps->pos + k >= ps->sumStart || ps->buf[ps->pos + k] != ' ') return 0;
  return k;
}

static int fp_find_key(const char *s, size_t klen) {
  for (int j = 0; j < FP_COUNT; j++) {
    if (FP_TABLE[j].kind == FP_RECON) continue; // never in the file
    if (strlen(FP_TABLE[j].key) == klen &&
        memcmp(FP_TABLE[j].key, s, klen) == 0) {
      return j;
    }
  }
  return -1;
}

// Walks the table over the body. TWO POLICIES, and the difference is the
// point (foh_persist.h):
//
//   HISTORICAL version -> its own FROZEN grammar. Exactly the rows with
//   since <= ver, in order, ALL MANDATORY, and nothing else permitted. A
//   v3 file carrying a v4 line is corrupt; a v3 file missing `modonr` is
//   corrupt. Old formats do not become permissive because a new one is.
//
//   CURRENT version -> EXTENSIBLE. Known keys in order, unknown keys
//   skipped, absent keys defaulted. This is what retires the version bump:
//   a later build appends a row under the SAME header, and both builds
//   read both files. Out-of-order and duplicate keys are still `order`,
//   and a known key's value is still judged by its own grammar and domain.
static bool fp_walk(FpParse *ps, FohPersist *v) {
  if (ps->ver != FP_VERSION) {
    for (int j = 0; j < FP_COUNT; j++) {
      const FpField *f = &FP_TABLE[j];
      if (f->kind == FP_RECON) continue;
      if (f->since > ps->ver) {
        fp_absent(f, v);
        continue;
      }
      if (!fp_parse_field(ps, f, v)) return false;
    }
    // nothing may sit between the last content line and the SUM line
    if (ps->pos != ps->sumStart) return fp_die(ps, "grammar");
    return true;
  }
  int next = 0;
  while (ps->pos < ps->sumStart) {
    const size_t klen = fp_key_len(ps);
    if (klen == 0) return fp_die(ps, "grammar");
    const int j = fp_find_key(ps->buf + ps->pos, klen);
    if (j < 0) {
      if (!fp_skip_unknown(ps, klen)) return false;
      continue;
    }
    // a key we know, in a place it cannot be: out of order, or a second
    // copy of a row already taken. Either way the file is not ours.
    if (j < next) return fp_die(ps, "order");
    for (int m = next; m < j; m++) fp_absent(&FP_TABLE[m], v);
    if (!fp_parse_field(ps, &FP_TABLE[j], v)) return false;
    next = j + 1;
  }
  for (int m = next; m < FP_COUNT; m++) fp_absent(&FP_TABLE[m], v);
  return true;
}

FohPersistStatus foh_persist_load(FohPersist *p) {
  fp_table_check();
  char path[512];
  if (snprintf(path, sizeof path, "%s/%s", foh_persist_dir(), FP_FILE) >=
      (int)sizeof path) {
    gfx_fatal("foh_persist: dir path overflow");
  }
  FILE *f = fopen(path, "rb");
  if (!f) {
    if (errno == ENOENT) return fp_reset(p, FOH_PERSIST_RESET_MISSING, 0);
    return fp_reset(p, FOH_PERSIST_RESET_CORRUPT, "open");
  }
  static char buf[FP_CAP + 1];
  const size_t got = fread(buf, 1, sizeof buf, f);
  const bool readOk = ferror(f) == 0;
  fclose(f);
  if (!readOk) return fp_reset(p, FOH_PERSIST_RESET_CORRUPT, "open");
  if (got > FP_CAP) return fp_reset(p, FOH_PERSIST_RESET_CORRUPT, "oversize");
  if (got == 0 || buf[got - 1] != '\n') {
    return fp_reset(p, FOH_PERSIST_RESET_CORRUPT, "truncated");
  }
  // isolate the last line: must be exactly "SUM <64 lowercase hex>\n"
  size_t sumStart = got - 1;
  while (sumStart > 0 && buf[sumStart - 1] != '\n') sumStart--;
  const size_t sumLen = got - sumStart;
  if (sumLen != 4 + 64 + 1 || memcmp(buf + sumStart, "SUM ", 4) != 0 ||
      !fp_is_hex16(buf + sumStart + 4) || !fp_is_hex16(buf + sumStart + 20) ||
      !fp_is_hex16(buf + sumStart + 36) || !fp_is_hex16(buf + sumStart + 52)) {
    return fp_reset(p, FOH_PERSIST_RESET_CORRUPT, "truncated");
  }
  char want[65];
  ml_sha256_hex(buf, sumStart, want);
  if (memcmp(want, buf + sumStart + 4, 64) != 0) {
    return fp_reset(p, FOH_PERSIST_RESET_CORRUPT, "sum");
  }
  FohPersist v;
  foh_persist_defaults(&v);
  size_t pos = 0;
  // line 1: the header, ^MLFKPERSIST[0-9]+$. There are no per-version arms
  // any more — the version is a NUMBER the table's `since` column consumes.
  // fromVer is 0 for the current format and the source version otherwise.
  int fromVer = 0;
  {
    size_t e = pos;
    while (e < sumStart && buf[e] != '\n') e++;
    if (e >= sumStart) return fp_reset(p, FOH_PERSIST_RESET_CORRUPT, "header");
    const size_t len = e - pos;
    if (len <= 11 || memcmp(buf + pos, "MLFKPERSIST", 11) != 0) {
      return fp_reset(p, FOH_PERSIST_RESET_CORRUPT, "header");
    }
    int fv = 0;
    for (size_t k = pos + 11; k < e; k++) {
      if (buf[k] < '0' || buf[k] > '9') {
        return fp_reset(p, FOH_PERSIST_RESET_CORRUPT, "header");
      }
      if (fv < 1000) fv = fv * 10 + (buf[k] - '0');
    }
    // "MLFKPERSIST01" is not version 1; a leading zero was never written by
    // anything, so it is a version this build does not support.
    if (buf[pos + 11] == '0' && len > 12) fv = -1;
    // ANY version this build cannot know (>= 8, or a nonsense 0) takes
    // RESET_VERSION. >= 8 will never exist: version bumps are retired and a
    // new field is a new ROW under the current header (foh_persist.h).
    if (fv < 1 || fv > FP_VERSION) {
      return fp_reset(p, FOH_PERSIST_RESET_VERSION, 0);
    }
    fromVer = (fv == FP_VERSION) ? 0 : fv;
    pos = e + 1;
  }
  FpParse ps = {.buf = buf,
                .sumStart = sumStart,
                .pos = pos,
                .ver = fromVer ? fromVer : FP_VERSION,
                .detail = 0};
  if (!fp_walk(&ps, &v)) {
    return fp_reset(p, FOH_PERSIST_RESET_CORRUPT, ps.detail);
  }
  *p = v;
  // A migrated older file is LOADED, not reset — but it says so loudly,
  // so an upgrade is never silent. The next save republishes it as current.
  if (fromVer) fprintf(stderr, "foh_persist: migrated from=%d\n", fromVer);
  fprintf(stderr, "foh_persist: loaded\n");
  return FOH_PERSIST_LOADED;
}

// --- atomic save ------------------------------------------------------------

// The success-with-a-caveat sentinel. Its ADDRESS is the signal (the header
// says compare by pointer), so it must be one object with external linkage —
// a string literal would be free to be a different object per TU.
const char foh_publish_nodirsync[] = "published, dir entry not proven durable";

// THE ONE PUBLISH (generalised for A45 T4; the body is the pre-T4
// foh_persist_save's, moved rather than rewritten).
//
// WHY IT MOVED. `port/sim/target/custom_stage.h` made this binding when T2
// deliberately shipped no writer: *"the correct move is to generalise
// foh_persist_save's existing publish (foh_persist.c:506-551 — tmp write,
// fsync file, rename, fsync dir, every rc checked, loud on failure) into
// `foh_persist_publish(name, buf, n)` and call it, NOT to grow a second
// file-writing path. /mnt is vfat with no journal and is mounted
// errors=remount-ro, so an unchecked write rc is a silent data loss."*
// Every rc check, the tmp/fsync/rename/dir-fsync ORDER and the FAT-class
// EINVAL/ENOTSUP tolerance below are the reviewed lines, unchanged.
//
// ONE MECHANISM, TWO POLICIES. It REPORTS instead of dying, and each caller
// picks: foh_persist_save keeps its loud death (a settings file that cannot
// be written is a broken device), while the target builder puts the reason
// ON SCREEN so the player can free space and retry. Dying mid-edit would
// destroy the stage he is holding, which is the opposite of a save button.
//
// FREE SPACE IS CHECKED BEFORE THE FIRST BYTE IS OPENED. `errors=remount-ro`
// means a full or erroring vfat starts refusing writes part-way through a
// session, and a half-written tmp that never renames is the GOOD outcome.
// statvfs costs nothing and turns "it just didn't save" into a named rule.
//
// The scratch file is per-NAME (`<name>.tmp`), not the shared FP_TMP, so two
// publishes can never race over one path.
bool foh_persist_publish(const char *name, const char *buf, size_t n,
                         const char **why) {
#define PUB_FAIL(msg)                                                          \
  do {                                                                         \
    if (why) *why = (msg);                                                     \
    return false;                                                              \
  } while (0)
  const char *dir = foh_persist_dir();
  if (mkdir(dir, 0755) != 0 && errno != EEXIST) {
    PUB_FAIL("cannot create the persist dir");
  }
  char tmp[512], fin[512];
  if (snprintf(tmp, sizeof tmp, "%s/%s.tmp", dir, name) >= (int)sizeof tmp ||
      snprintf(fin, sizeof fin, "%s/%s", dir, name) >= (int)sizeof fin) {
    PUB_FAIL("path too long");
  }
  // FREE SPACE FIRST. The payload plus a 64 KB margin: vfat allocates in
  // clusters and the directory entry costs blocks of its own, so "exactly n
  // bytes free" cannot be relied on to land a rename.
  {
    struct statvfs vfs;
    if (statvfs(dir, &vfs) != 0) PUB_FAIL("cannot stat the persist filesystem");
    const unsigned long long avail =
        (unsigned long long)vfs.f_bavail * (unsigned long long)vfs.f_frsize;
    if (avail < (unsigned long long)n + 65536ull) PUB_FAIL("disk full");
  }
  FILE *f = fopen(tmp, "wb");
  if (!f) PUB_FAIL("cannot open the temp file");
  if (fwrite(buf, 1, n, f) != n) {
    fclose(f);
    remove(tmp);
    PUB_FAIL("temp write failed");
  }
  if (fflush(f) != 0) {
    fclose(f);
    remove(tmp);
    PUB_FAIL("temp flush failed");
  }
  if (fsync(fileno(f)) != 0) {
    fclose(f);
    remove(tmp);
    PUB_FAIL("temp fsync failed");
  }
  if (fclose(f) != 0) {
    remove(tmp);
    PUB_FAIL("temp close failed");
  }
  // the ONLY publish: atomic rename over the real file
  if (rename(tmp, fin) != 0) {
    remove(tmp);
    PUB_FAIL("rename publish failed");
  }
  // directory durability, best-effort for the FAT class (EINVAL/
  // ENOTSUP tolerated; a real I/O error is still loud). review-100 M3:
  // open(dir) FAILURE is no longer a silent skip — the rename is
  // published but its directory entry was not proven durable, so the
  // save is reported through a DISTINCT loud token (saved-nodirsync),
  // NEVER the plain `saved`. The fsync EINVAL/ENOTSUP tolerance (the
  // reviewed FAT class) is unchanged and keeps the plain `saved`; real
  // durability is proven end-to-end by the reboot round-trip leg, not
  // by an fsync rc.
  const int dfd = open(dir, O_RDONLY);
  if (dfd < 0) {
    // published, but the directory entry was not proven durable
    if (why) *why = FOH_PUBLISH_NODIRSYNC;
    return true;
  }
  if (fsync(dfd) != 0 && errno != EINVAL && errno != ENOTSUP) {
    close(dfd);
    PUB_FAIL("dir fsync failed");
  }
  close(dfd);
  if (why) *why = 0;
  return true;
#undef PUB_FAIL
}

void foh_persist_save(const FohPersist *p) {
  static char buf[FP_CAP];
  const size_t n = fp_serialize(p, buf, sizeof buf);
  const char *why = 0;
  if (!foh_persist_publish(FP_FILE, buf, n, &why)) {
    // UNCHANGED POLICY for the settings file: any failure is loud death.
    // The message is assembled here rather than passed as a format, because
    // gfx_fatal takes one string (raster.h:114).
    static char msg[256];
    snprintf(msg, sizeof msg, "foh_persist: save failed — %s",
             why ? why : "unknown");
    gfx_fatal(msg);
  }
  fprintf(stderr, why == FOH_PUBLISH_NODIRSYNC
                      ? "foh_persist: saved-nodirsync\n"
                      : "foh_persist: saved\n");
}

// --- machine glue (single definition site) ----------------------------------

// The bound render-state, captured at apply (review-100 M1: the
// same-process stale-PB product bug). foh_persist_record_update
// refreshes bound->targetRecords at the SAME improved-write so a
// same-process return-to-target-select renders the NEW record without
// a restart. ONE mechanism at the ONE write site — no scattered syncs,
// no render read-through. Lifetime: the bound FohState is the driver's
// live main()-scope state; record_update only fires (through the
// finishGame hook) during that scope, so the pointer is always valid.
// NULL until apply — record_update's refresh is a guarded no-op then
// (e.g. the standalone --tooth-persist-finish arm, which loads but
// never applies).
static FohState *g_bound = 0;

void foh_persist_apply(const FohPersist *p, FohState *s) {
  s->turbo = p->turbo;
  s->lCancelType = p->lCancelType;
  s->flashOnLCancel = p->flashOnLCancel;
  s->everyCharWallJump = p->everyCharWallJump;
  s->phantomThreshold = p->phantomThreshold;
  s->blastzoneWrapping = p->blastzoneWrapping;
  s->dustLessPerfectWavedash = p->dustLessPerfectWavedash;
  s->masterVolume[0] = p->masterVolume[0];
  s->masterVolume[1] = p->masterVolume[1];
  for (int k = 0; k < 4; k++) s->tapJumpOff[k] = p->tapJumpOff[k];
  memcpy(s->targetRecords, p->targetRecords, sizeof s->targetRecords);
  // A49/D45: the SELECTION plane, and the TOKEN plane RE-HOMED FROM IT.
  //
  // Writing both here is the whole of observable (b) at boot, and it is
  // D21/D35/D46's rule stated once more: a token is re-homed from the
  // SELECTION, never from anything else. Only `selChar` is on disk — if the
  // token plane were persisted separately the two could come back
  // disagreeing, and a player would boot looking at a character he did not
  // pick, which is CONTEXT.md's costliest defect on this exact screen.
  //
  // cssTokenRest is deliberately NOT touched: foh_init's memset leaves it at
  // 0 (the A-drop slot) and, since D46, every slot draws on the selection
  // anyway. The bound is FOH_CSS_PORTS because the plane is four ports wide.
  for (int k = 0; k < FOH_CSS_PORTS; k++) {
    s->selChar[k] = p->selChar[k];
    s->cssChar[k] = p->selChar[k];
  }
  // ticket #25: the CSS MACHINE plane, restored beside the selection.
  //
  // THE RE-HOME ABOVE IS UNCHANGED AND MUST STAY THAT WAY. `cssChar[k]` is
  // still written from `p->selChar[k]` and from nothing else — not from a
  // token's own position (there is none on disk), not from `k`, and not from
  // anything below. Everything here is the MACHINE plane; the token plane
  // has exactly one source and it is the line above.
  //
  // `portType` and `cpuDifficulty` are written through the PLANE names, not
  // through FohState's p1Type/difficulty union aliases: same storage, and
  // naming the alias is how a four-port loop becomes a one-port loop by
  // accident (foh.h says so at both unions).
  for (int k = 0; k < FOH_CSS_PORTS; k++) {
    s->portType[k] = p->portType[k];
    s->cpuDifficulty[k] = p->cpuDifficulty[k];
    s->cssSliderX[k] = p->cssSliderX[k];
  }
  s->versusMode = p->versusMode;
  s->cssHandX = p->cssHand[0];
  s->cssHandY = p->cssHand[1];
  s->cssCarry = p->cssCarry;
  s->cssCpuCarry = p->cssCpuCarry;
  s->cssHandType = p->cssHandType;
  // ticket #26: the TARGET-SELECT view plane. Three fields, and the list
  // they describe is NOT one of them — `tssSlotPresent`/`tssSlotReason` are
  // re-derived (FOH_RESUME_HOOK_TSS_SLOTS, and foh.c's entry arm for every
  // ordinary arrival). Restoring `tssPage` without that re-derivation would
  // put the player on the CUSTOM grid describing a card nobody had read.
  s->tssCursor = p->tssCursor;
  s->tssPage = p->tssPage;
  s->tssHandX = p->tssHand[0];
  s->tssHandY = p->tssHand[1];
  g_bound = s; // review-100 M1: bind for the record-time refresh
}

void foh_persist_collect(FohPersist *p, const FohState *s) {
  p->turbo = s->turbo;
  p->lCancelType = s->lCancelType;
  p->flashOnLCancel = s->flashOnLCancel;
  p->everyCharWallJump = s->everyCharWallJump;
  p->phantomThreshold = s->phantomThreshold;
  p->blastzoneWrapping = s->blastzoneWrapping;
  p->dustLessPerfectWavedash = s->dustLessPerfectWavedash;
  p->masterVolume[0] = s->masterVolume[0];
  p->masterVolume[1] = s->masterVolume[1];
  for (int k = 0; k < 4; k++) p->tapJumpOff[k] = s->tapJumpOff[k];
  // A49/D45: the SELECTION plane. `cssChar` is NOT collected — it is a view
  // of this one (foh.h), and storing a view is how two representations drift.
  for (int k = 0; k < FOH_CSS_PORTS; k++) p->selChar[k] = s->selChar[k];
  // ticket #25: the CSS MACHINE plane. `cssTokenRest` is deliberately absent
  // for the same reason `cssChar` is — it is token-plane bookkeeping that
  // nothing computes a position from (foh_css_token_pos, DEVIATION D46: all
  // three rest paths home on the SELECTION), so it records a history rather
  // than holding state, and the D21/D35/D46 witnesses are its only readers.
  // `cssReady` is absent because it is DERIVED: foh_tick recomputes it after
  // every tick that ends on this screen, so a stored copy could only be a
  // stale one. `bHold` is absent because it counts frames a button has been
  // held, and on the boot after a lid close no button is held.
  for (int k = 0; k < FOH_CSS_PORTS; k++) {
    p->portType[k] = s->portType[k];
    p->cpuDifficulty[k] = s->cpuDifficulty[k];
    p->cssSliderX[k] = s->cssSliderX[k];
  }
  p->versusMode = s->versusMode;
  p->cssHand[0] = s->cssHandX;
  p->cssHand[1] = s->cssHandY;
  p->cssCarry = s->cssCarry;
  p->cssCpuCarry = s->cssCpuCarry;
  p->cssHandType = s->cssHandType;
  // ticket #26: the TARGET-SELECT view plane. The HAND goes with the cursor
  // and the page, because on this screen it is what HOLDS them (D29: the
  // slot the hand is over writes the selection every frame) — collecting the
  // cursor without it stores a value the next boot overwrites on tick one.
  // The slot cache is absent because it is re-derived, not restored — see
  // the apply side.
  p->tssCursor = s->tssCursor;
  p->tssPage = s->tssPage;
  p->tssHand[0] = s->tssHandX;
  p->tssHand[1] = s->tssHandY;
  // records are chokepoint-owned: they change ONLY through
  // foh_persist_record_update (the finishGame arm), never collected
  // back from the display copy.
}

bool foh_persist_record_update(FohPersist *p, int ch, int tstage,
                               double matchTimer) {
  if (ch < 0 || ch >= FOH_PERSIST_CHARS || tstage < 0 ||
      tstage >= FOH_PERSIST_TSTAGES ||
      !(isfinite(matchTimer) && matchTimer >= 0.0 &&
        matchTimer < FP_TIME_CAP)) {
    gfx_fatal("foh_persist: record update out of domain");
  }
  const double rec = p->targetRecords[ch][tstage];
  // main.js:1442: matchTimer < rec || rec == -1
  const bool improved = (matchTimer < rec) || (rec == -1.0);
  if (improved) {
    p->targetRecords[ch][tstage] = matchTimer;
    // review-100 M1: refresh the bound render copy at the SAME write so
    // a same-process return-to-target-select renders the new record
    // (the chokepoint owns the sync; no driver-side plumbing).
    if (g_bound) g_bound->targetRecords[ch][tstage] = matchTimer;
  }
  fprintf(stderr, "foh_persist: record char=%d tstage=%d improved=%d\n", ch,
          tstage, improved ? 1 : 0);
  return improved;
}
