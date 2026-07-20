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
// FILE FORMAT `MLFKPERSIST1` (versioned + checksummed; exactly 55 LF
// lines, deterministic bytes — twin checks cmp host vs device):
//   MLFKPERSIST1
//   turbo [01]
//   lcancel [0-2]
//   tapjump [01] [01] [01] [01]
//   rec <c> <s> <hex16>      x50, c-major (c 0..4, s 0..9, in order)
//   SUM <sha256-lowercase-hex of ALL preceding bytes>
// Doubles are hex16 IEEE-754 bit patterns — NO strtod on any path (the
// iter-38 device-musl strtod class is structurally out). rec domain:
// exactly the -1.0 pattern (bff0000000000000) or finite in [0, 6000)
// (the matchTimer cap, targetplay.js:282).
//
// LOAD (foh_persist_load): strict anchored line-by-line parse. Missing
// file / version mismatch / ANY grammar, order, domain, checksum,
// truncation, or size deviation = LOUD reset-to-defaults — one exact
// stderr line per boot, NEVER silent (the qjs getCookie lesson
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
// on return. Emits exactly one stderr event line.
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
