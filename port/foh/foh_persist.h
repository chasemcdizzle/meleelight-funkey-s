// port/foh/foh_persist.h — the ONE persistence chokepoint (fix_plan §M4
// task 13; pre-registration AGENT-LOG iter 100).
//
// Persists the FOH-editable, sim-consumed gameSettings (ALL ELEVEN keys as of
// v4 — the "subset" wording below predates the completed options screens) and the
// target-test records to SD, upstream-faithfully:
//   - settings: v1's {turbo, lCancelType, tapJumpOff[4]} PLUS v4's
//     {flashOnLCancel, everyCharWallJump, blastzoneWrapping,
//     dustLessPerfectWavedash, phantomThreshold} and the two masterVolume
//     levels — i.e. all eleven gameSettings keys upstream's own B-exit save
//     loop writes (gameplaymenu.js:29-31), plus audiomenu.js:24-25's pair.
//     The historical note below describes v1's narrower subset
//     the options-gameplay screen edits (foh.h iter 88). Defaults are
//     the authored upstream defaults, settings.js:44-56 (all zero).
//     The remaining gameSettings entries are not FOH-editable and keep
//     their compile-time defaults in sim_boot/tp_setup — NOT persisted
//     (registered narrowing, AGENT-LOG iter 100: nothing can change
//     them, so persisting them is dead weight with a live corruption
//     surface).
//   - targetRecords[5][10] doubles (seconds; -1 = no record) — the
//     upstream fresh state, targetplay.js:40. WRITE semantics =
//     main.js:1442-1445 (improve-or-first). Custom slots (>= 10) are
//     scope-excluded (the addcode refusal); chars 0..4 all persist —
//     a REGISTERED DEVIATION from upstream's getTargetCookies i<3
//     read-loop quirk (targetplay.js:156: chars 3/4 are cookied but
//     never re-loaded upstream; ours is a rewritten surface per the
//     task text and loads all five). medalsEarned is DERIVED from
//     records (giveMedals, targetplay.js:165-174) and is not directly
//     persisted upstream either — records are the only persisted
//     truth (medal DISPLAY = the registered iter-99 pipeline-extension
//     deferral).
//
// FILE FORMAT `MLFKPERSIST7` (versioned + checksummed; exactly 78 LF
// lines, deterministic bytes — twin checks cmp host vs device). v2
// added the `ctlstyle` line, v3 the `modonr` line (fix_plan A4), v4
// the seven options lines below (MENU-SPEC §3/§4 — the completed
// gameplay + audio screens), v5 the four `bind` rows (fix_plan A31 /
// DEVIATION D26 — the Controls screen's real rebinder), v6 the `sel`
// row and v7 the `resume` row (fix_plan A26 / DEVIATION D53 — hibernate).
//
// THE LINE COUNT WENT 70 -> 78 WITHOUT THE VERSION MOVING, which is ticket
// #22's whole point rather than an inconsistency: MLFKPERSIST7 is the LAST
// header this build will ever write (see the field-table note below).
// Ticket #25 appended the eight CSS rows — `ptype` through `handtype` — as
// eight table rows, and a v7 file written before they existed still loads,
// because an absent row takes its default.
//
// WHY v4 AND NOT v2-WITH-MORE-LINES (cross-lane collision, resolved at
// the iter-13x merge — read this before renumbering anything): the menus
// lane independently developed its own 62-line `MLFKPERSIST2` while the
// controls lane shipped a 56-line one. Two DIFFERENT layouts under one
// token is unresolvable at load time. The controls lane's lineage is the
// one that actually SHIPPED (its v1/v2/v3 are the only formats that ever
// reached a device), so it is kept verbatim as the migration source set
// and the menus lane's seven lines are APPENDED on top as a NEW version.
// The menus lane's private "v2" is therefore not a migration source and
// deliberately has no arm: no build ever wrote one.
// Older files on disk are MIGRATED, never discarded (review-ctl r1/r2):
// the checksum is verified like any other and every setting plus all 50
// target records are carried forward, emitting
// `foh_persist: migrated from=<1|2|3|4|5|6>` and then `loaded`. Resetting a
// VALID older file would destroy every target-test personal best on the
// owner's device. Every older format is a strict PREFIX of the
// next, so one parse serves them all and the migration only fills what
// is absent:
//   from v2 — ctlStyle carries over UNCHANGED (the CtlStyle enum numbers
//     are frozen for exactly this reason), modOnR takes the ratified 0.
//   from v6 — no `resume` row, so resumeScreen takes FOH_STARTUP, i.e. NO
//     resume armed: a v6 file was written by a build that could not record
//     a screen, so it has no place to send the player and the boot proceeds
//     exactly as it always did. "Nothing armed" is the only migration a
//     resume record can honestly have — inventing a screen here would be
//     the "restores the wrong screen" defect the feature exists to avoid.
//   from v5 — no `sel` row, so every port takes MARTH (0): a v5 file was
//     written by a build that persisted no character at all, so it has no
//     opinion to carry forward, and marth is exactly the selection that
//     device booted its CSS with.
//   from v4 — no `bind` rows, so every port takes the IDENTITY binding:
//     a v4 file was written by a build with no rebinder at all, so the
//     identity is precisely the mapping that device already had.
//   from v1 — no style line at all, so ctlStyle becomes BOX: a v1 file
//     can only have come from a build whose sole mapping was the
//     ratified S1 == BOX, so the upgrade preserves the controls that
//     device already had. modOnR takes the ratified 0.
// NATURAL is the default for a FRESH/reset install only, and an upgrade
// never silently moves a binding. Each version is validated against ITS
// OWN grammar: a v2 file's ctlstyle domain is the historical {0,1}, not
// the current {0,1,2} (review-ctl n1). Any version >= 8 (a FUTURE format
// this build cannot know) takes RESET_VERSION:
//   MLFKPERSIST7
//   turbo [01]
//   lcancel [0-2]
//   tapjump [01] [01] [01] [01]
//   ctlstyle [0-2]
//   modonr [01]
//   rec <c> <s> <hex16>      x50, c-major (c 0..4, s 0..9, in order)
//   flash [01]
//   walljump [01]
//   blastzone [01]
//   dustless [01]
//   phantom <hex16>
//   soundslevel <hex16>
//   musiclevel <hex16>
//   bind <port> <8 slot digits>   x4, port-major (port 0..3, in order);
//                                 each row a PERMUTATION of 0..7
//   sel <c> <c> <c> <c>           [0-4] x4, port-major (fix_plan A49, D45)
//   resume <NN>                   TWO digits, a FohScreen (fix_plan A26, D53)
//   --- the CSS machine plane (ticket #25; owner ruling 2026-08-27) --------
//   ptype <t> <t> <t> <t>         [0-2] x4, port-major. WIRE-BIASED: the
//                                 digit is playerType + 1, so 0 = N/A(-1),
//                                 1 = HMN(0), 2 = CPU(1). playerType's own
//                                 domain has a -1 in it and a file column is
//                                 one unsigned digit; the bias lives in the
//                                 table's `wireBias` column, in ONE place,
//                                 not in the caller.
//   cpudiff <d> <d> <d> <d>       [0-3] x4, port-major. WIRE-BIASED the other
//                                 way: the digit is cpuDifficulty - 1, so the
//                                 slider's 1..4 maps onto the column exactly
//                                 and 0 is not a legal level by construction.
//   vsmode [01]                   0 stock | 1 endless (main.js:140)
//   hand <hex16> <hex16>          cssHandX, cssHandY — the free cursor, in
//                                 doubles, [0, 240] on each axis
//   slider <hex16> x4             cpuSlider[k].x, port-major, [0, 240]
//   carry <c>                     [0-4], WIRE-BIASED: digit is
//                                 whichTokenGrabbed + 1 (0 = holding nothing)
//   cpucarry <c>                  [0-4], same bias, whichCpuGrabbed + 1
//   handtype <h>                  [0-2] handPoint | handOpen | handGrab
//   SUM <sha256-lowercase-hex of ALL preceding bytes>
// The v4, v5, v6 and v7 blocks are APPENDED after the 50 rec rows on purpose:
// it keeps every older version a strict PREFIX through the rec block, so the
// one shared parser still serves v1/v2/v3/v4/v5/v6 and their rec-row line
// indices are unchanged. Migration fills the v4 block with the authored defaults
// (flash/walljump/blastzone/dustless 0, phantom 0.01, soundslevel 0.5,
// musiclevel 0.3) — the same values a fresh install gets, because no
// older file ever carried an opinion about them.
// Doubles are hex16 IEEE-754 bit patterns — NO strtod on any path (the
// iter-38 device-musl strtod class is structurally out). rec domain:
// exactly the -1.0 pattern (bff0000000000000) or finite in [0, 6000)
// (the matchTimer cap, targetplay.js:282).
//
// THE FIELD TABLE, AND WHY VERSION BUMPS ARE RETIRED (ticket #22, ADR
// 0001 — read the ADR; this is the mechanism, not the argument).
// Everything above describes the BYTES, and every byte of it is now
// produced and consumed by ONE declarative table — foh_persist.c's
// FP_FIELDS, one row per persisted field naming its key, its type, its
// offset, its domain and the earliest file version that carries it. The
// writer walks it and the reader walks it, so a field cannot be written
// in a shape the reader does not accept, and adding a field is ONE ROW.
//
// Three consequences, all of them load-bearing:
//
//   * THE SIX MIGRATION ARMS ARE GONE, replaced by the table's `since`
//     column. A v1 file simply has no row with since > 1, so those fields
//     take their absent value; that IS the migration, and it is the same
//     code path as a current file. Every arm's documented outcome above is
//     unchanged and still proven by check-device-persist.sh's T-H9/T-H13/
//     T-H16 byte-for-byte migration teeth.
//
//   * A HISTORICAL VERSION IS PARSED AGAINST ITS OWN FROZEN GRAMMAR:
//     exactly the rows with since <= ver, in order, ALL MANDATORY, and
//     nothing else permitted. A v3 file carrying a v4 line is still
//     corrupt, a v3 file missing `modonr` is still corrupt (T-H10/T-H11/
//     T-H12/T-H14/T-H15/T-H17). Old formats do not become permissive.
//
//   * THE CURRENT VERSION IS PARSED EXTENSIBLY, and that is what retires
//     the bump: known keys in order, UNKNOWN KEYS SKIPPED, ABSENT KEYS
//     DEFAULTED. A future build that appends a row keeps writing
//     MLFKPERSIST7, this build ignores the row it does not know, and that
//     build defaults the row this one did not write. `MLFKPERSIST7` is
//     therefore the LAST version number: >= 8 stays RESET_VERSION because
//     nothing will ever write one. Out-of-order and duplicate keys are
//     still `order` corruption, and a known key's VALUE is still judged by
//     its own grammar and domain — extensible is not permissive.
//
// POINTER-VALUED FIELDS ARE NEVER COPIED. The table's kinds are int and
// double only, so a pointer field cannot be given a row; it must instead
// be declared unpersisted (FP_UNPERSISTED_BYTES) and RECONSTRUCTED after
// the fields land — a raw byte image would restore an address that is
// valid only while the binary is unchanged, which is a trap and not a
// feature. There are none today, and the table kind FP_RECON exists to
// mark the first one.
//
// LOAD (foh_persist_load): strict anchored line-by-line parse. Missing
// file / UNSUPPORTED version (>= 8; v1..v6 migrate, see above) / ANY
// grammar, order, domain, checksum,
// truncation, or size deviation = LOUD reset-to-defaults — one exact
// stderr line per boot (two on a migrating boot — the `migrated`
// prelude then `loaded`), NEVER silent (the qjs getCookie lesson
// inverted for OUR surface: absent Storage silently zeroed every
// gameSettings entry upstream; here absence/corruption RESETS LOUDLY
// to the authored defaults). The bad file is left in place (each boot
// stays loud) until the next save overwrites it.
//
// SAVE (foh_persist_save): canonical bytes -> <dir>/mlfk-persist.tmp
// (fwrite + fflush + fsync + fclose) -> rename() over
// <dir>/mlfk-persist.dat -> best-effort directory fsync (EINVAL/
// ENOTSUP tolerated — the FAT class; anything else fatal). rename is
// the ONLY publish: any failure dies loudly BEFORE the real file is
// touched. Emits `foh_persist: saved` on the durable path; if the
// directory could NOT be opened for the durability fsync (the rename
// published but its dir entry is not proven durable), emits the
// DISTINCT loud token `foh_persist: saved-nodirsync` instead — NEVER a
// silent-degraded plain `saved` (review-100 M3).
//
// DIR: $MLFK_PERSIST_DIR when set (hermetic checks: every check run
// gets a FRESH dir so flows start from defaults); else the product
// path /mnt/mlfk-data (created on first save).
//
// STDERR EVENT GRAMMAR (single emission site, this TU; strict-parsed
// by check-device-persist.sh; committed device checks' summary parsers
// are needle-anywhere and unaffected):
//   foh_persist: loaded
//   foh_persist: migrated from=<1|2|3|4|5|6>  (review-ctl r1/r2 + the
//                2026-07-29 v3 bump and the v4 bump: a VALID older file
//                was carried forward — settings + all 50 records
//                preserved; from=1 also sets ctlStyle to BOX, the only
//                mapping a v1 build had, while from=2 keeps its ctlStyle
//                unchanged and from=3 takes the seven appended v4 option
//                lines at their fresh-install defaults. A PRELUDE line: it
//                always precedes that boot's `loaded`, so a migrating
//                boot emits two lines)
//   foh_persist: reset cause=missing
//   foh_persist: reset cause=version
//   foh_persist: reset cause=corrupt detail=<open|oversize|header|
//                grammar|order|domain|sum|truncated>
//   foh_persist: saved
//   foh_persist: saved-nodirsync   (review-100 M3: dir open failed —
//                the rename published, its dir entry not proven durable)
//   foh_persist: record char=<0-4> tstage=<0-9> improved=<01>
#ifndef FOH_FOH_PERSIST_H
#define FOH_FOH_PERSIST_H

#include <stdbool.h>
#include <stddef.h> // size_t (foh_persist_publish)

#include "../gfx/ctl_style.h" // CTL_BIND_PORTS / CTL_BTN_COUNT (A31)
#include "foh.h"

#define FOH_PERSIST_CHARS 5
#define FOH_PERSIST_TSTAGES 10

typedef struct {
  int turbo;       // settings.js:44 (0)
  int lCancelType; // settings.js:46 (0)
  int tapJumpOff[4]; // settings.js:51-54 (0,0,0,0)
  // fix_plan A4 control style — the CtlStyle enum as a WIRE VALUE:
  // 0 = CTL_STYLE_NORMAL, 1 = CTL_STYLE_BOX, 2 = CTL_STYLE_NATURAL.
  // NATURAL is the fresh-install default (owner ruling 2026-07-29) but is
  // NOT the zero value, so foh_persist_defaults() assigns it explicitly —
  // the numbers are frozen so MLFKPERSIST2 saves keep their scheme.
  // Data only — this TU never installs it. The FOH owns the two calls:
  //   after load:  ctl_style_set(p.ctlStyle);
  //   before save: p.ctlStyle = ctl_style_get();
  // (kept out of foh_persist_apply/collect on purpose: FohState has no
  // style field, and a call here would drag port/gfx/ctl_style.c onto
  // every link line that already carries this TU.)
  int ctlStyle;
  // fix_plan A4 Mod shoulder (owner ruling 2026-07-29), ORTHOGONAL to
  // ctlStyle: 0 = the M3-ratified arrangement (Mod on L, shield on R),
  // 1 = swapped (Mod on R, shield on L). Only BOX is affected. Data
  // only — same chokepoint rules as ctlStyle:
  //   after load:  ctl_mod_on_r_set(p.modOnR != 0);
  //   before save: p.modOnR = ctl_mod_on_r_get();
  int modOnR;
  double targetRecords[FOH_PERSIST_CHARS][FOH_PERSIST_TSTAGES]; // -1
  // --- v4 (MENU-SPEC §3/§4; the options screens completed) ----------------
  // gameSettings is ELEVEN keys (settings.js:44-56) and upstream's B-exit
  // writes EVERY one of them (gameplaymenu.js:29-31), including the three
  // that have no widget. With the row list completed there is no longer a
  // reason for our subset to be narrower than upstream's own save loop, so
  // it is not: all eleven round-trip here.
  int flashOnLCancel;          // settings.js:48 (0) — render.js:125
  // DEAD: zero MECHANICS/GAMEPLAY consumers. Two display-only readers do
  // exist (gameplaymenu.js:239 draws its own row; css.js:1183-1191 prints
  // it in inServerMode) — neither is a mechanic, so the owner ruling
  // "implement it faithfully dead" still holds: row + persisted bit, wired
  // to nothing.
  int everyCharWallJump;       // settings.js:51 (0) — LIVE since D20
  // (physics.c:400 gates the per-character walljump ability on it) and
  // widened by D47 (puff reaches it via the WALLTECHJUMP ECB alias).
  // Was commented "DEAD, no sim readers" until 2026-08-24 — stale by two
  // deviations. A comment is not evidence; see CONTEXT.md.
  int blastzoneWrapping;       // settings.js:47 (0) — DEAD, zero readers
  int dustLessPerfectWavedash; // settings.js:49 (0) — DEAD, zero readers
  double phantomThreshold;     // settings.js:50 (0.01) — CHECKSUM SURFACE
  // audiomenu.js:13 masterVolume [0.5, 0.3] = [sounds, music], persisted
  // raw (:24-25) — the +/-0.1 steps are unrounded upstream, so the stored
  // double carries their dust verbatim.
  double masterVolume[2];
  // --- v5 (fix_plan A31; DEVIATION D26) ---------------------------------
  // The Controls screen's button bindings: bind[port][phys] = the LOGICAL
  // button that physical button `phys` drives (ctl_style.h owns the
  // contract and the CtlBtn wire numbers). Always a PERMUTATION of
  // 0..CTL_BTN_COUNT-1 per port; the loader REFUSES anything else rather
  // than installing a table with an action missing from it.
  //
  // PER-PORT IN THE FORMAT even though only port 0 has a UI: the port
  // dimension is the expensive half to retrofit, and the A33 spike has not
  // closed on whether a second physical controller is possible. Ports 1..3
  // are the identity today and nothing writes them.
  //
  // Same chokepoint rules as ctlStyle — the cells live in ctl_style.c, a
  // different TU, so the DRIVERS move them, not foh_persist_apply/collect:
  //   load:        for (k) ctl_bind_set_row(k, p.bind[k]);
  //   before save: for (k, i) p.bind[k][i] = ctl_bind_get(k, i);
  int bind[CTL_BIND_PORTS][CTL_BTN_COUNT];

  // --- v6 (fix_plan A49; DEVIATION D45) ---------------------------------
  // The CSS SELECTION plane: which character each PORT has chosen
  // (FohState.selChar — CONTEXT.md's "Selection plane"). Owner, verbatim:
  // *"i want to MAKE it persistent please ... I want it to be the last
  // character"*. Upstream cookies no character at all (getGameplayCookies
  // reads gameSettings and nothing else), so persisting one is the
  // deviation, not the omission. MEASURED before writing it: nothing here
  // carried any CSS state, so picks had NEVER survived a restart on any
  // port — this is a new feature, not a regression being repaired.
  //
  // WHAT IS DELIBERATELY NOT HERE, and why — this was a design question,
  // answered, not an oversight:
  //
  //   * the TOKEN plane (FohState.cssChar) is a VIEW of this one, so it is
  //     re-homed FROM it by foh_persist_apply rather than stored beside it.
  //     Storing a view next to the thing it views is precisely CONTEXT.md's
  //     "one thing having two representations that drifted apart" — the
  //     class that has already cost this screen D21, D35 and D46.
  //
  //   * the PORT TYPES and the CPU LEVELS were not persisted either, and
  //     THAT PART HAS BEEN REVERSED — see `portType` below, which states the
  //     new rule, the ruling that made it and what it costs. This bullet is
  //     kept as a stub rather than deleted so that a reader who arrives from
  //     a citation of the old text finds the reversal instead of silence.
  int selChar[FOH_CSS_PORTS];

  // --- THE CSS MACHINE PLANE (ticket #25; OWNER RULING 2026-08-27) --------
  //
  // THE RULE, FIRST. The character select comes back set up the way it was
  // left: port types, CPU levels, the stock/endless mode, where the hand is
  // and what it is holding. Owner report: closing the lid on the CSS gave
  // back a screen with the opponent gone, the mode reverted and the cursor
  // at the top. Only the character survived, because only the character had
  // a row. Each of these is now a row.
  //
  // THIS REVERSES A JUDGEMENT WRITTEN AT `selChar` ABOVE, and the reversal
  // is recorded rather than quietly overwritten. That text refused to
  // persist portType and the CPU levels for two stated reasons, and the
  // owner was asked to choose between (A) persist them anyway, (B) persist
  // presence and difficulty but leave every port disarmed on boot, and (C)
  // persist only the mode and difficulty. He chose A — "persist them
  // anyway". So:
  //
  //   * REASON 1 — "upstream's fresh state is playerType = [-1,-1,-1,-1],
  //     so not persisting is the faithful answer" — is now answered the way
  //     `selChar` itself already answers it. Upstream cookies no CSS state
  //     at all, so faithfulness cannot decide between persisting one CSS
  //     field and persisting six; the deviation was taken at `selChar` and
  //     this is the same deviation, not a new one. What foh_init reproduces
  //     is still upstream's fresh state, and a machine that has never been
  //     saved still gets exactly it (foh_persist_defaults, which now reads
  //     foh.h's CSS COLD-START PLANE so the two cannot drift).
  //
  //   * REASON 2 — the unverified-configuration argument — was NOT wrong
  //     and has NOT been argued away. It has been overruled, and its
  //     consequence is carried here in full:
  //
  //     THE CONSEQUENCE, STATED. This device can now BOOT INTO AN ALREADY
  //     ARMED CHARACTER SELECT — possibly one reading READY TO FIGHT before
  //     the player has touched anything — off a configuration he last saw in
  //     another session. If that session left a 3-or-4-port CPU match set
  //     up, the machine now WAKES INTO A CONFIGURATION NO GOLDEN COVERS: CPU
  //     on ports 2/3 is playable (live ai.c, check-ai-live.sh) but no
  //     recorded trace verifies it, and the launch guard in foh.c carries
  //     that in full. It is a device DEFAULT BOOT STATE now, not something
  //     chosen this session. This is a ruled cost, not a defect report
  //     waiting to be filed: anyone who meets it here has already been told.
  //
  // The token plane is STILL not here and still must not be — see `selChar`.
  // What is persisted below is the MACHINE plane (CONTEXT.md: what the
  // player was doing), and foh_persist_apply re-homes the token FROM the
  // selection exactly as it did before, never from these fields.
  //
  // STRUCT ORDER IS LAYOUT, FILE ORDER IS THE TABLE, and this block sits
  // where it does for TWO layout reasons that are worth a paragraph because
  // neither is visible from the declarations themselves. The file's own
  // order is FP_FIELDS' order in foh_persist.c and is unaffected by either;
  // read the format block at the top of this header for that.
  //
  //   * IT STARTS ON THE DOUBLES. `selChar` ends at an offset that is a
  //     multiple of 8, so putting the two double arrays first opens no
  //     alignment hole. Start with the ints instead and there is a four-byte
  //     gap before them that FP_UNPERSISTED_BYTES would have to declare —
  //     paying for the field twice, and growing the "deliberately not
  //     persisted" number for something that IS persisted, which is the one
  //     edit foh_persist.c's guard comment asks nobody to make casually.
  //
  //   * IT ENDS BEFORE `resumeScreen`, WHICH STAYS THE LAST PERSISTED
  //     MEMBER. check-persist-table.sh leg [9] proves layoutGuard is not
  //     cargo by DELETING it and requiring a newly appended int to become
  //     invisible in the tail padding — and it appends that int after
  //     `int resumeScreen;`, because that member was the tail. Land this
  //     block after it and the anchor is no longer the tail, the
  //     counterfactual stops modelling what it claims to model, and the leg
  //     fails. The tooth is a hostage of this ordering; that is stated here
  //     rather than discovered by whoever moves it next.
  //
  // cssHandX, cssHandY as one row (css.js:64 handPos). DOUBLES, never
  // integers: rounding happens at draw time only, and a hand restored to a
  // rounded pixel would drift its own hit tests by up to half a cell.
  double cssHand[2];
  // cpuSlider[k].x (css.js:72). CONTINUOUS and NOT derivable from
  // cpuDifficulty: upstream's knob keeps the raw hand x it was released at,
  // and re-deriving it from the level would snap it to one of four stops and
  // move where the next grab has to aim. That is why it is a row of its own
  // rather than something foh_persist_apply recomputes.
  double cssSliderX[FOH_CSS_PORTS];
  // playerType[0..3] (main.js:107): -1 N/A, 0 HMN, 1 CPU. FohState overlays
  // this array with p1Type..p4Type in a union; the table names the ARRAY,
  // and naming both would serialise the same storage twice.
  int portType[FOH_CSS_PORTS];
  // cpuDifficulty[0..3] (main.js:109), domain 1..4 (css.js:325-327). Same
  // union caveat as portType: one name, the per-port array.
  int cpuDifficulty[FOH_CSS_PORTS];
  int versusMode;  // FohState.versusMode — 0 stock | 1 endless (main.js:140)
  int cssCarry;    // whichTokenGrabbed[0] (css.js:68), -1 = holding nothing
  int cssCpuCarry; // whichCpuGrabbed[0] (css.js:75), -1 = holding nothing
  // handType[0] (css.js:63): 0 handPoint, 1 handOpen, 2 handGrab. STORED,
  // not derived — upstream's three assignment sites genuinely disagree, so
  // recomputing it at load would draw a sprite the player never saw.
  int cssHandType;

  // --- v7 (fix_plan A26; DEVIATION D53) ---------------------------------
  // HIBERNATE/RESUME. Owner: *"closing screen and opening it doesn't come
  // back to the game though. i want it to."* Closing the lid does NOT
  // suspend this device — `fkgpiod` runs `powerdown schedule 0.1`, which
  // SIGUSR1s the recorded process and powers the machine OFF 100 ms later,
  // so the app is killed and the frontend is what comes back. Resuming
  // therefore means RECORDING A PLACE and going back to it, and this is
  // the row that holds the place.
  //
  // The value is a FohScreen, and FOH_STARTUP (0) means NOTHING ARMED —
  // boot proceeds exactly as it always did. It is DRIVER-OWNED, not
  // FohState-owned, so like ctlStyle it is deliberately outside
  // foh_persist_apply/collect: FohState has no such field, the driver
  // stamps it in its SIGUSR1 arm and consumes it at boot.
  //
  // SCOPE, ruled and not an oversight: MENU-LEVEL ONLY. A mid-match
  // snapshot is the whole MlPlayer/physics plane — a new serialization
  // surface with its own correctness bar — so a match records the screen
  // its own exit would have landed on (the CSS for VS, target select for
  // targets), which is one button from playing again. The mapping is
  // foh_persist_resume_target() below and it is also the file's DOMAIN,
  // so a screen the driver would refuse to restore cannot be stored.
  int resumeScreen;


  // --- LAYOUT GUARD (ticket #22 / ADR 0001) — NOT DATA, NOT PERSISTED ----
  // Persistence is driven by a DECLARATIVE FIELD TABLE (foh_persist.c's
  // FP_FIELDS), and the table's byte total is `_Static_assert`ed against
  // sizeof(FohPersist), so ADDING A FIELD HERE WITHOUT DECIDING WHAT
  // HAPPENS TO IT FAILS THE BUILD. That assertion is the whole reason the
  // ADR chose a table over per-screen save/load: it turns "someone must
  // remember" into "the compiler will not let you forget".
  //
  // This member exists because the assertion would otherwise have a hole.
  // Every other member is an int or a double and the int count is ODD, so
  // the struct carries exactly four bytes of TAIL ALIGNMENT PADDING — and a
  // new `int` appended at the end would land in that padding, leaving
  // sizeof(FohPersist) UNCHANGED and the guard silent. That is precisely
  // the next change anyone will make (tickets #25/#26/#27 each add one).
  // Declaring the padding as a member closes it: the member total now
  // EQUALS sizeof, the assertion is an equality with zero slack, and any
  // added field of any size moves it.
  //
  // MEASURED, not assumed: check-persist-table.sh's static-assert tooth
  // adds an `int` to a COPY of this header and requires the build to FAIL.
  // Delete this member and that tooth goes quiet — the hostage relationship
  // ADR 0001 documents under "Accepted risk", stated here as well.
  //
  // It is never serialised, never read and never written; foh_persist_
  // defaults() zeroes it with everything else. It is accounted for by
  // FP_UNPERSISTED_BYTES in foh_persist.c, which is the ONE place a
  // deliberately-unpersisted field is declared.
  int layoutGuard;
} FohPersist;

typedef enum {
  FOH_PERSIST_LOADED = 0,
  FOH_PERSIST_RESET_MISSING,
  FOH_PERSIST_RESET_VERSION,
  FOH_PERSIST_RESET_CORRUPT
} FohPersistStatus;

// The resolved persistence directory (env override or the product
// default). Stable for the process lifetime.
const char *foh_persist_dir(void);

// Authored upstream defaults (cited above).
void foh_persist_defaults(FohPersist *p);

// A26/D53. Maps the screen the player was ACTUALLY on to the screen a
// resume may legitimately restore, or FOH_STARTUP for "do not resume".
// Idempotent by construction: every value it returns maps to itself, which
// is exactly what makes it usable as the persisted row's domain check —
// the file can only ever hold a screen the driver would restore.
//
// The non-identity rows, each because restoring the literal screen would be
// WORSE THAN NOT RESUMING:
//   FOH_MATCH  -> FOH_CSS   mid-match state is out of scope (see the struct
//   FOH_TMATCH -> FOH_TSS   field); these are the screens the match's own
//                           exit lands on (foh_dev.c's MEX_CSS/MEX_TSS arm).
//   FOH_SSS    -> FOH_CSS   the port types ARE persisted now (ticket #25),
//                           so this row's stated reason is spent; the row
//                           survives it and ticket #27 owns the removal.
//                           foh_persist.c's own arm carries the whole note.
//   FOH_CREDITS-> FOH_MENU_OPTIONS  the credits reticle is placed by the
//                           ENTERING transition (foh.c credX/credY), which a
//                           resume never runs; MENU_OPTIONS is its B-exit.
//   FOH_MENU_BATTLE -> FOH_MENU_TOP  unreachable at FOH_NETPLAY 0.
//   FOH_STARTUP-> FOH_STARTUP  the boot animation is not a place.
FohScreen foh_persist_resume_target(FohScreen sc);

// Load <dir>/mlfk-persist.dat. On ANY reset arm, *p holds the defaults
// on return. Emits exactly one TERMINAL stderr event line (`loaded` or
// one `reset cause=...`); a v1..v5 migration additionally emits
// the `migrated from=<1|2|3|4|5>` PRELUDE line immediately before `loaded`, so
// a migrating boot emits two lines, never one.
FohPersistStatus foh_persist_load(FohPersist *p);

// Atomic save (tmp + fsync + rename). Loud death on any failure.
void foh_persist_save(const FohPersist *p);

// THE ONE PUBLISH (A45 T4). Writes `n` bytes to <persist dir>/<name>
// atomically: <name>.tmp -> fwrite -> fflush -> fsync -> fclose -> rename ->
// fsync(dir). EVERY rc is checked, and free space is checked with statvfs
// BEFORE the temp file is opened, because /mnt is vfat (no journal) mounted
// errors=remount-ro — a full or erroring filesystem starts refusing writes
// part-way through a session, and the one failure a player must never get is
// a silent one.
//
// Returns true on publish. On false NOTHING was published (the temp file is
// removed) and *why — when non-NULL — points at a STATIC string naming the
// step that failed, suitable for putting on screen verbatim.
//
// On SUCCESS *why is NULL, except in the one reviewed case where the rename
// landed but the directory entry could not be proven durable, where it is the
// sentinel FOH_PUBLISH_NODIRSYNC (compare by POINTER — it is that exact
// object, not a message to string-match). That is foh_persist_save's existing
// `saved-nodirsync` arm, kept distinguishable rather than folded into `saved`.
//
// SINGLE WRITE PATH, BY CONTRACT. `port/sim/target/custom_stage.h` states it
// for the .mlstage plane: a second file-writing path in this tree is a defect,
// not an option. Every new producer of a file on this device calls THIS.
extern const char foh_publish_nodirsync[];
#define FOH_PUBLISH_NODIRSYNC foh_publish_nodirsync
bool foh_persist_publish(const char *name, const char *buf, size_t n,
                         const char **why);

// Machine glue (single definition site — no per-driver field lists):
// apply pushes settings + records into the FOH machine state AND BINDS
// that state as the record-refresh target (review-100 M1: the chokepoint
// keeps FohState.targetRecords live in-process); collect pulls the
// settings fields back (records are chokepoint-owned and only change
// through foh_persist_record_update).
void foh_persist_apply(const FohPersist *p, FohState *s);
void foh_persist_collect(FohPersist *p, const FohState *s);

// The finishGame record arm (main.js:1442-1443: improve-or-first).
// Returns true when the record improved (caller saves). On an improve
// it ALSO refreshes the bound FohState's targetRecords entry (the
// apply-time binding) so a same-process return-to-target-select
// renders the new record without a restart (review-100 M1 — the
// stale-PB product bug; ONE mechanism at the ONE write site). Emits the
// `foh_persist: record` event line. Domain-guarded (loud death on an
// out-of-domain char/tstage/time — unreachable from the FOH plane).
bool foh_persist_record_update(FohPersist *p, int ch, int tstage,
                               double matchTimer);

#endif // FOH_FOH_PERSIST_H
