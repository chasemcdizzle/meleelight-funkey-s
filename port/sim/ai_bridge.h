// ai_bridge.h — the AI-input bridge (M2 task 16; fix_plan §M2 "AI
// policy"). PLAN §4 keeps ai.js JS-side until M4, but the M2 exit gate
// covers the CPU goldens g07/g08. The bridge replays a CPU slot from a
// RECORDING of the AI's verified write-set instead of executing AI logic:
//
//   - upstream consumes the AI exclusively through aiInputBank[i][0]
//     (input.js:135 pollInputs returns the LIVE ROW as the slot's polled
//     input — an alias: runAI(i) mutates the row between interpretInputs
//     and physics inside update(i), main.js:894-908, so physics reads the
//     fresh values through buffer slot 0);
//   - runAI draws the SEEDED Math.random stream in-tick — the C sim must
//     burn the identical draws at the identical call sites to keep the
//     chain aligned for every later consumer (g08: 1496 in-match rngCalls,
//     nearly all on the AI plane);
//   - the AI's ONLY other writes are its private bookkeeping on player[i]
//     (currentAction / currentSubaction / lastMash + the upstream
//     `curentAction` typo field, ai.js:1254) — read back by nothing but
//     ai.js itself and one DOM debug line (physics.js:1223, render plane),
//     absent from the checksum surface (oracle/CHECKSUM.md §7). Hard-
//     verified per-call: grep-measured statically AND recon-diffed at
//     runtime by the capture spec (spec-ai.js), pinned wsViol == 0.
//
// BRIDGE ARTIFACT (`<golden>.ai-bridge.txt`, built from the ai-spec
// capture by port/sim/calib/build-ai-bridge.js):
//   header  : AIBRIDGE1 <golden> seed=<u32> boot=<n> entries=<n>
//   entry   : <frame> <slot> <ndraws>[ <draw-hex16>]* <22 field tokens>
//   token   : B0 | B1 | U | N<hex16>   (bool false/true, undefined,
//             number by IEEE-754 bit pattern)
// Fields in canon key order: a,b,csX,csY,dd,dl,dr,du,l,lA,lsX,lsY,r,rA,
// rawX,rawY,rawcsX,rawcsY,s,x,y,z. One entry per upstream runAI(i) call
// (update(i)'s !starting && actionState != "SLEEP" arm), in call order.
//
// CONSUMPTION (replay_ai.c now, check-sim.sh's sim in task 17): at the
// runAI call site, ml_ai_bridge_apply(entry, rng, bankRow):
//   1. burns entry->ndraws draws on the CHAINED seeded mulberry32 and
//      bit-compares each against the recording (a dropped/extra draw
//      shifts the chain — caught at the first draw, not frames later);
//   2. verifies the entry's NEVER-AI-WRITTEN fields (the measured
//      complement: dd,dl,dr,du,r,rA,rawX,rawY,rawcsX,rawcsY,s) against
//      the chained bank row — the recording cannot smuggle state through
//      fields the AI provably does not write (C-side write-set teeth);
//   3. installs the recorded row as the new bank row (the AI-written
//      fields: a,b,csX,csY,l,lA,lsX,lsY,x,y,z).
// The caller then models the pollInputs alias by copying the bank row
// into buffer slot 0 before physics consumes the buffer.
#ifndef ML_AI_BRIDGE_H
#define ML_AI_BRIDGE_H

#include "input/ai_input.h"
#include "ml_rng.h"

#define AI_BRIDGE_NFIELDS 22

// canon key order (byte-wise sort of the 22 ASCII keys)
extern const char *const ai_bridge_field_names[AI_BRIDGE_NFIELDS];

// AI write-set membership per field, measured from upstream src/main/
// ai.js (every live `aiInputBank[i][0].<f> =` site; commented-out writes
// excluded): a,b,csX,csY,l,lA,lsX,lsY,x,y,z. false = the bridge VERIFIES
// the field against the chain instead of trusting the recording.
extern const bool ai_bridge_field_ai_written[AI_BRIDGE_NFIELDS];

typedef struct {
  long frame;
  int slot;
  int ndraws;
  double *draws;                     // recorded seeded draws, in order
  MlAiVal field[AI_BRIDGE_NFIELDS];  // post-runAI bank row, canon key order
} MlAiBridgeEntry;

typedef struct {
  char golden[16];
  uint32_t seed;
  long boot;        // boot-time draw count (the qjs boot pin class)
  long nentries;
  MlAiBridgeEntry *entries;
  long cursor;      // sequential consumption position
} MlAiBridge;

// Load an AIBRIDGE1 artifact. Returns 0 on success; on any syntax or
// domain violation prints to stderr and returns nonzero (strict, never a
// guess — prevention rule 7).
int ml_ai_bridge_load(MlAiBridge *br, const char *path);
void ml_ai_bridge_free(MlAiBridge *br);

// Sequential lookup: the next unconsumed entry, or NULL when exhausted.
// (Entries are in upstream call order; the consumer decides whether the
// next entry matches its current (frame, slot) call site.)
const MlAiBridgeEntry *ml_ai_bridge_peek(const MlAiBridge *br);
void ml_ai_bridge_advance(MlAiBridge *br);

// Field-order accessors on the tagged row (canon key order index).
MlAiVal *ai_row_field(MlAiInput *row, int idx);
const MlAiVal *ai_row_field_const(const MlAiInput *row, int idx);

typedef struct {
  int bad_draw;          // index of the first draw mismatch, -1 if none
  double bad_draw_got;   // the chained rng's value at that index
  int bad_field;         // index of the first never-written-field
                         // divergence, -1 if none
} MlAiBridgeApplyResult;

// Apply one entry at the runAI call site (see header comment: burn+verify
// draws, verify never-written fields, install the row). The row is
// installed even on divergence (the recording stays authoritative so one
// divergence does not cascade); the caller counts/reports.
MlAiBridgeApplyResult ml_ai_bridge_apply(const MlAiBridgeEntry *e,
                                         MlRng *rng, MlAiInput *bankRow);

#endif // ML_AI_BRIDGE_H
