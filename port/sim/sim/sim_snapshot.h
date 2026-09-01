// port/sim/sim/sim_snapshot.h — ticket #28: the sim state writes out and
// reads back, exactly.
//
// WHAT THIS IS FOR. A resumable match (#29) needs the running sim written to
// a file and read back into a process that is not the one that wrote it.
// GameState is ~160 KB of nested structs and it is NOT a byte image: it
// carries pointers (the AI bridge's loaded artifact), and the planes around
// it carry function pointers (the actionStates move tables, the live-AI and
// finish seams). A stored address is valid only while the binary is
// unchanged and only within one process, so restoring one is a trap.
//
// SO THIS IS A FIELD TABLE, and deliberately the SAME mechanism ticket #22
// (ADR 0001) put behind the settings record: rows with a kind, an offset and
// a size, with pointer-valued fields marked RECONSTRUCTED rather than
// copied, and the table's byte total `_Static_assert`ed against
// sizeof(GameState) so that adding a field to the sim state without a
// decision does not compile. One mechanism in the codebase, not two.
//
// TWO BARS, TWO TESTS (CONTEXT.md, "the same state"):
//   - BYTE ROUND TRIP — write, poison, read back, and every persisted row
//     must be byte-identical to what was there before; then write again and
//     the two files must be identical. Catches a SERIALISER bug.
//   - CHECKSUM CONTINUATION — run a golden, run it again snapshotting and
//     restoring part way, and the frames after the restore point must be
//     identical to the uninterrupted run. Catches a COMPLETENESS bug: state
//     nobody knew had to be saved. A round trip cannot find that, because it
//     never knew the field existed.
// Both are run by port/sim/check-sim-snapshot.sh. The stream verifier that
// judges the second is oracle/harness/verify-stream.js, UNMODIFIED.
//
// THE FILE. Modelled on port/sim/target/custom_stage.c's .mlstage contract,
// which is modelled on foh_persist.c's SUM idiom — bounded read, exact
// anchored grammar, checksum verified BEFORE anything is parsed:
//
//   "MLSIM1\n"                                     7 bytes
//   "BUILD " <64 lowercase hex> "\n"              71 bytes
//   "BYTES " <20 zero-padded decimal digits> "\n" 27 bytes
//   <payload>                                      exactly BYTES bytes
//   "SUM " <64 lowercase hex> "\n"                69 bytes   (sha256 over
//                                                             every preceding
//                                                             byte)
//
// The payload is raw field bytes in table order, so it is THIS BUILD'S
// representation and no other's — which is exactly why BUILD exists and why
// a mismatch is refused by name rather than tolerated. There is no
// portability claim here and none is wanted: a snapshot is a resume token
// for one binary, not an interchange format.
//
// THE SEAM. sim_main.c calls two hooks that are NULL unless this TU is
// linked (the ml_sim_runai_live / tp_custom_setup pattern, CLAUDE.md): the
// M2 EXIT GATE's frozen TU list does not include sim_snapshot.c, so
// `bash port/sim/check-sim.sh` builds and behaves bit-for-bit as before.
#ifndef ML_SIM_SNAPSHOT_H
#define ML_SIM_SNAPSHOT_H

#include <stdbool.h>
#include <stddef.h>

#include "sim.h"

// The build identity this binary writes into a snapshot and demands back:
// sha256 over a canonical descriptor of the field table (every row's key,
// kind, offset and size), sizeof(GameState), the module-state rows and their
// sizes, the payload total, and MLSNAP_BUILD_TAG. Lowercase hex, NUL
// terminated.
//
// WHAT IT CATCHES, EXACTLY: any change to the sim state's LAYOUT or to the
// table that describes it, and — through the tag, which defaults to this
// TU's own build stamp — any REBUILD. It is deliberately conservative in
// that direction: refusing a snapshot that would in fact have restored fine
// costs a resumed match, and accepting one that would not costs a corrupted
// one. What it does NOT catch on its own is a behaviour change with an
// identical layout in a binary built from the same stamp; the tag is the
// column that exists for a build system to pin something stronger into
// (-DMLSNAP_BUILD_TAG='"<vcs id>"').
void ss_build_identity(char out[65]);

// Total payload bytes this build writes. Derived from the table.
size_t ss_payload_bytes(void);

// Write `g` (plus the module-state rows) to `path`. Returns true, or false
// with *reason naming what failed. Writes via a temp file and rename, so a
// power loss cannot leave a half-written snapshot under the real name — the
// foh_persist_save atomic-publish discipline, not a second write path
// invented here.
bool ss_save(const GameState *g, const char *path, const char **reason);

// Read `path` back over `g` (plus the module-state rows). Returns true, or
// false with *reason naming the rule that refused it. EVERY refusal names
// its rule, and integrity comes before meaning: a corrupt file is refused as
// corrupt, never as "from a different build".
//
// PRECONDITION: `g` must already have been booted and set up (the RECON rows
// — today the AI bridge's loaded artifact — are reconstructed by the caller
// before this runs, not by this function).
bool ss_load(GameState *g, const char *path, const char **reason);

// The byte round trip, in one process: save, poison every persisted byte,
// load back, compare every row against what was there, save again and
// compare the two files. Leaves `g` and the module state exactly as it found
// them (proven, not assumed — that is what the comparison is). Returns true,
// or false with *reason naming the first row that differed.
bool ss_round_trip(GameState *g, const char *pathA, const char *pathB,
                   const char **reason);

#endif // ML_SIM_SNAPSHOT_H
