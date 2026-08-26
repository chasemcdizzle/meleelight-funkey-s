// port/sim/target/custom_stage.h — A45 T2 (D42/D43): a PLAYER-AUTHORED
// target stage reaches the sim.
//
// T1 (D39) landed the codec: a share code <-> MlkStage, proven byte-for-
// byte against upstream's own executed encode.js. T1 stopped one step
// short of playable, because every entry into target mode was keyed by an
// integer id into the generated TTAB1 table:
//
//   tp_stage_from_ttab1(int tstageId, MlStageX *)      target_play.h
//   tp_setup_target(GameState *, int charId, int tstageId)
//   gfx_target_init(Gfx *, int tstageId, int bg)       gfx_target.h
//
// so a stage that is not baked into the pipeline had nowhere to go. The
// design spike's §5.2 reading was correct and I re-verified it before
// building: MlStageX (physics.h:74-89) is a PLAIN RUNTIME STRUCT, neither
// generated nor const, so the whole blocker dissolves into ONE filler —
// tp_stage_from_custom — plus a custom arm on each of the two setup
// entries. No redesign, no new stage abstraction, no id allocation.
//
// This header owns the three things that follow from that:
//   1. the filler and the custom setup entry (the sim side);
//   2. mlk_stage_playable — the ONE validator both the file loader and
//      the setup entry route through, so a stage can never reach the sim
//      by a path that skipped a check;
//   3. the .mlstage file plane: ten FIXED slots, addressed by index.
//
// ---------------------------------------------------------------------
// TRANSPORT: one .mlstage file per slot (spike §3.4)
//
// Upstream's `+ ADD CODE` accepts a ~1 KB string pasted into an HTML
// textarea (targetselect.js:132-136). This device has no clipboard, no
// keyboard and no network, and its on-screen letter grid (D8) is blocked
// behind A14; entering 1 KB on a d-pad grid is ~1000 presses. It DOES
// have an SD card the owner mounts. So a code arrives as a file, and the
// D8/A14 dependency is deleted rather than worked around.
//
// ON-DISK CONTRACT — exactly three LF-terminated lines, no trailing byte:
//
//   MLSTAGE1\n
//   <share code — createStageCode's own alphabet>\n
//   SUM <64 lowercase hex>\n
//
// SUM is sha256 over every preceding byte, the foh_persist.c convention
// (foh_persist.c:154 emits it, :218-222 isolates it) reused verbatim so
// there is one integrity idiom on this device, not two.
//
// VALIDATE ON READ, ALWAYS. A .mlstage is user-supplied and this device
// has a MEASURED power-loss risk on a journal-less vfat /mnt, so a
// truncated file is an expected input, not a hypothetical one. The load
// path therefore: bounds the read (a file over the cap is refused, never
// partially read), requires the exact grammar, verifies SUM BEFORE the
// code is parsed, and only then runs mlk_parse + mlk_stage_playable.
// Every refusal names its rule. A corrupt file cannot reach the sim.
//
// WRITING IS NOT T2's. T2 loads and plays; the file arrives by SD card.
// When T3/T4 need to WRITE one, the correct move is to generalise
// foh_persist_save's existing publish (foh_persist.c:506-551 — tmp write,
// fsync file, rename, fsync dir, every rc checked, loud on failure) into
// `foh_persist_publish(name, buf, n)` and call it, NOT to grow a second
// file-writing path. /mnt is vfat with no journal and is mounted
// errors=remount-ro, so an unchecked write rc is a silent data loss.
// port/foh/ is a parallel lane this ticket must not touch, so that is
// stated here and reported rather than built.
//
// ---------------------------------------------------------------------
// D43 (owner ruling, 2026-08-24, verbatim: "ok let's fix it like you
// say"): upstream's custom-stage list CLOBBERS. targetselect.js:164-166
// (and identically :551-552 on the boot reload) does
//
//     customTargetStages[customTargetStages.length - 1] = stage;
//     setCustomTargetStages(customTargetStages.length, ...);
//
// so adding A then B then C leaves ["B","C","C"] — every add destroys the
// previous stage, and because the cookies are written correctly the data
// is destroyed AGAIN on every load. The port does not carry it. Rationale
// on the record: it is user data loss; no golden covers this plane
// (CHECKSUM.md is players + articles), so no checksum stream can diverge;
// and it is menu bookkeeping, not gameplay.
//
// The fix is not a patch to two call sites, it is the value model: the
// custom plane here is a FIXED ARRAY OF MLK_MAX_SLOTS SLOTS ADDRESSED BY
// INDEX, with no append and no length cursor. Slot i is the file
// custom<i>.mlstage and nothing else; reading slot i cannot touch slot j;
// there is no `length - 1` to be off by one. Both of upstream's clobber
// sites are structurally absent rather than individually repaired, which
// is also why fixing the add path alone (the reported half) would have
// left the reload half live.
#ifndef ML_CUSTOM_STAGE_H
#define ML_CUSTOM_STAGE_H

#include "../stage_code.h" // MlkStage
#include "target_play.h"   // MlStageX, GameState, ML_MAX_TARGETS

// Ten slots — upstream's own hard limit (targetselect.js:817, :102).
#define MLK_MAX_SLOTS 10

// targetStagePlaying for custom slot i. Upstream's target-select list
// puts the custom stages after the 10 authored ones (targetselect.js:288-
// 294 "Custom N"), so slot i is id 10 + i and the authored 0..9 keep
// theirs. Nothing in the sim indexes a table with it — it is carried
// state (targetplay.js:36) that the FOH reads back.
#define MLK_PLAYING_BASE MLK_MAX_SLOTS

// Longest legal .mlstage on disk: header + code + SUM + slack. A file
// larger than this is REFUSED unread rather than truncated into a parse.
#define MLK_FILE_MAX (MLK_CODE_MAX + 128)

// --- the ONE validator ------------------------------------------------
//
// True iff this stage may be handed to the sim. On false, *reason (when
// non-NULL) names the rule — the same loud-refusal discipline as T1's
// caps: rejected with a reason, NEVER silently truncated or clamped.
//
// mlk_parse already enforces the codec's own caps (<= ML_MAX_SURFACES per
// list, <= ML_MAX_LEDGES with every ledge index in range of its list,
// polygon caps, a starting point present). What is left is exactly the
// three places where the CODEC's domain is wider than the SIM's:
//
//  (1) TARGETS. MlkStage holds MLK_MAX_TARGETS (20 — targetbuilder.js:563's
//      own cap) but the sim holds ML_MAX_TARGETS (10), _Static_assert-tied
//      to upstream's 10-element targetDestroyed literal (targetplay.js:37).
//      JS arrays grow, so a 20-target stage genuinely plays upstream. This
//      is design risk R2 and it is an OWNER RULING, not mine: refusing
//      loudly is the safe half and is what ships here. Raising the cap is
//      possible (it is a capacity change like ML_MAX_LEDGES 8->16 in iter
//      94) and costs RAM in MlSim; measured and reported, not taken.
//
//  (2) THE CONCAT CAP. runCollisionRoutine builds walls ++ grounds ++
//      ceilings ++ platforms into ONE list bounded by
//      ML_MAX_LABELLED_SURFACES (96). Four lists of 64 pass the per-list
//      cap and overflow the concatenation, so the SUM must be checked
//      here or a legal-looking code walks off that array.
//
//  (3) THE DAMAGE PLANE, WHICH HAS NEVER EXECUTED. MEASURED
//      (pipeline/lib/targets-schema.js:25,:101): damageType exists on
//      ZERO authored stages, VS or target. dealWithDamagingStageCollision
//      is translated (physics.c, five call sites) and covered by NO
//      golden, and target_play.c's tick arm traps outright if physics ever
//      pushes a stage-damage hitQueue row. A share code CAN carry a damage
//      digit (encode.js field d, 0..4), so loading one would make dead
//      code live with no oracle behind it. T2 REFUSES it at load, where
//      the reason is legible, instead of letting it become a mid-match
//      fatal. Lifting this refusal is A45 T6's job and T6 OWES A RECORDED
//      DAMAGE GOLDEN for it — the refusal is the marker, not a stub.
bool mlk_stage_playable(const MlkStage *st, const char **reason);

// --- the sim side -----------------------------------------------------

// The tp_stage_from_ttab1 twin: MlkStage -> the physics read set. Caller
// must have passed mlk_stage_playable (this asserts it and dies loudly
// rather than trusting).
//
// hasConnected is TRUE, and the plane is DERIVED here — corrected
// 2026-08-26 (A45 T5 prerequisite). `connected` is indeed not one of the
// share-code grammar's 14 fields, but upstream does not read it out of the
// code: parseStageCode ENDS with `stage.connected = getConnected(stage)`
// (encode.js:237), so every custom stage upstream plays has one, computed
// from its own surfaces. The previous note here read the grammar and
// concluded "NOT NEEDED", which was true about the grammar and wrong about
// the behaviour. It was invisible to every check because the authored
// corpus yields ZERO links (measured, all ten stages) and an all-null
// connected takes the same physics arm as an absent one — the divergence
// only appears once a player DRAWS, which A45 T5/T7 are about to let them
// do. Full argument + measurement: util/get_connected.h.
//
// respawnCount stays 0 like every authored target stage: target mode's
// isFinalDeath() is unconditionally true (actionStateShortcuts.js:153,
// gameMode == 5), so REBIRTH is unreachable and a dispatch traps.
void tp_stage_from_custom(const MlkStage *cs, MlStageX *out);

// setActiveStageCustomTarget(slot) (activeStage.js:83) + the
// targetselect.js:143-146 entry + startTargetGame(0, false) — i.e.
// tp_setup_target for a stage that has no TTAB1 id. Routes through
// tp_setup_target_core, so it and the authored entry share ONE
// translation of startTargetGame. Dies loudly if !mlk_stage_playable.
void tp_setup_target_custom(GameState *g, int charId, int slot,
                            const MlkStage *cs);

// --- the .mlstage file plane ------------------------------------------

// "<dir>/custom<slot>.mlstage". False (and nothing written) on overflow
// or a slot outside 0..MLK_MAX_SLOTS-1.
bool mlk_slot_path(const char *dir, int slot, char *buf, size_t cap);

// Read + verify + parse slot `slot`. True and *out filled on success.
// False on ANY of: absent file, unreadable, over MLK_FILE_MAX, bad
// grammar, SUM mismatch, mlk_parse rejection, mlk_stage_playable
// rejection — with *reason (when non-NULL) naming which. NEVER partially
// fills a stage the caller might use; NEVER truncates a long file into a
// short parse. Absent is reported as a refusal like any other so a caller
// cannot confuse "no such slot" with "loaded".
bool mlk_slot_load(const char *dir, int slot, MlkStage *out,
                   const char **reason);

// The ten slots as the menu sees them (D43: presence BY INDEX; no list,
// no length, no append). `reason[i]` is NULL when present[i], else the
// refusal string — so a corrupt slot shows up as a named refusal in the
// UI instead of silently vanishing or, worse, shifting the others up.
// Stages are deliberately NOT held here: sizeof(MlkStage) is ~45 KB, so
// ten resident stages would be ~450 KB of a device that counts them.
// One is parsed on demand at play time (mlk_slot_load).
typedef struct {
  bool present[MLK_MAX_SLOTS];
  const char *reason[MLK_MAX_SLOTS];
} MlkSlots;

void mlk_slots_scan(const char *dir, MlkSlots *out);

#endif // ML_CUSTOM_STAGE_H
