// port/foh/foh_persist.h — the ONE persistence chokepoint (fix_plan §M4
// task 13; pre-registration AGENT-LOG iter 100).
//
// Persists the FOH-editable, sim-consumed gameSettings subset and the
// target-test records to SD, upstream-faithfully:
//   - settings: {turbo, lCancelType, tapJumpOff[4]} — the exact subset
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
// FILE FORMAT `MLFKPERSIST3` (versioned + checksummed; exactly 57 LF
// lines, deterministic bytes — twin checks cmp host vs device). v2
// added the `ctlstyle` line and v3 the `modonr` line (fix_plan A4).
// Older files on disk are MIGRATED, never discarded (review-ctl r1/r2):
// the checksum is verified like any other and every setting plus all 50
// target records are carried forward, emitting
// `foh_persist: migrated from=<1|2>` and then `loaded`. Resetting a
// VALID older file would destroy every target-test personal best on the
// owner's device. Both older formats are strict PREFIXES of v3, so one
// parse serves all three and the migration only fills what is absent:
//   from v2 — ctlStyle carries over UNCHANGED (the CtlStyle enum numbers
//     are frozen for exactly this reason), modOnR takes the ratified 0.
//   from v1 — no style line at all, so ctlStyle becomes BOX: a v1 file
//     can only have come from a build whose sole mapping was the
//     ratified S1 == BOX, so the upgrade preserves the controls that
//     device already had. modOnR takes the ratified 0.
// NATURAL is the default for a FRESH/reset install only, and an upgrade
// never silently moves a binding. Each version is validated against ITS
// OWN grammar: a v2 file's ctlstyle domain is the historical {0,1}, not
// the current {0,1,2} (review-ctl n1). Any version >= 4 (a FUTURE format
// this build cannot know) takes RESET_VERSION:
//   MLFKPERSIST3
//   turbo [01]
//   lcancel [0-2]
//   tapjump [01] [01] [01] [01]
//   ctlstyle [0-2]
//   modonr [01]
//   rec <c> <s> <hex16>      x50, c-major (c 0..4, s 0..9, in order)
//   SUM <sha256-lowercase-hex of ALL preceding bytes>
// Doubles are hex16 IEEE-754 bit patterns — NO strtod on any path (the
// iter-38 device-musl strtod class is structurally out). rec domain:
// exactly the -1.0 pattern (bff0000000000000) or finite in [0, 6000)
// (the matchTimer cap, targetplay.js:282).
//
// LOAD (foh_persist_load): strict anchored line-by-line parse. Missing
// file / UNSUPPORTED version (>= 4; v1 and v2 migrate, see above) / ANY
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
//   foh_persist: migrated from=<1|2>  (review-ctl r1/r2 + the 2026-07-29
//                v3 bump: a VALID older file was carried forward —
//                settings + all 50 records preserved; from=1 also sets
//                ctlStyle to BOX, the only mapping a v1 build had, while
//                from=2 keeps its ctlStyle unchanged. A PRELUDE line: it
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

// Load <dir>/mlfk-persist.dat. On ANY reset arm, *p holds the defaults
// on return. Emits exactly one TERMINAL stderr event line (`loaded` or
// one `reset cause=...`); a v1 OR v2 migration additionally emits the
// `migrated from=<1|2>` PRELUDE line immediately before its `loaded`, so
// a migrating boot emits two lines, never one.
FohPersistStatus foh_persist_load(FohPersist *p);

// Atomic save (tmp + fsync + rename). Loud death on any failure.
void foh_persist_save(const FohPersist *p);

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
