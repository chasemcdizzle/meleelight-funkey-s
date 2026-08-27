// port/sim/sim/sim_snapshot.c — ticket #28. See sim_snapshot.h for the
// contract, the file grammar and why this is a field table rather than a
// hand-written serialiser.
#include "sim_snapshot.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "sha256.h" // oracle/qjs/sha256.c (-Ioracle/qjs) — the same SUM
                    // primitive foh_persist.c and custom_stage.c use.
#include "sim_modstate.h"
#include "../characters/falcon/moves.h" // mv_falcon_ssg_{get,set}_canEdgeCancel

// ---------------------------------------------------------------------------
// THE FIELD TABLE
// ---------------------------------------------------------------------------

typedef enum {
  // Raw bytes of the member, copied out and back. Every plane of the match
  // that is plain data.
  SS_POD = 0,
  // Present in GameState, NEVER in the file: RECONSTRUCTED after the fields
  // land rather than copied out of it. This is the kind a pointer-valued
  // field takes (ADR 0001's rule, inherited whole) — a stored address is
  // valid only inside the process that stored it, so restoring one is a
  // trap. The row's `recon` column names the code that rebuilds it.
  SS_RECON,
  // The one field that is BOTH: MlAiBridge carries the loaded AIBRIDGE1
  // artifact (pointers — reconstructed by sim_main's ml_ai_bridge_load) and
  // a cursor into it (the consumption position — real match state, and lost
  // state if it is not carried). The codec below writes the identifying
  // header plus the cursor, verifies the header against the live artifact on
  // load, and never touches a pointer.
  SS_BRIDGE
} SsKind;

// The table, in GameState DECLARATION order (sim.h). Adding a member to
// GameState is exactly one row here — or a raised SS_PAD_BYTES with a
// comment saying why the bytes are deliberately not persisted. The
// _Static_assert below is what makes that a build failure rather than
// something someone has to remember.
//
// `recon` is non-NULL only for SS_RECON/SS_BRIDGE rows and names the code
// that rebuilds the field. It is part of the build identity (see
// ss_build_identity), so retitling a reconstructor invalidates old
// snapshots — which is correct: the reconstruction is part of the contract.
#define SS_FIELDS(X)                                                          \
  X(sim, .key = "sim", .kind = SS_POD)                                        \
  X(hq, .key = "hq", .kind = SS_POD)                                          \
  X(arts, .key = "arts", .kind = SS_POD)                                      \
  X(inp, .key = "inp", .kind = SS_POD)                                        \
  X(prevBuf, .key = "prevBuf", .kind = SS_POD)                                \
  X(prevBufAi, .key = "prevBufAi", .kind = SS_POD)                            \
  X(bank, .key = "bank", .kind = SS_POD)                                      \
  X(slotIsAi, .key = "slotIsAi", .kind = SS_POD)                              \
  X(curBuf, .key = "curBuf", .kind = SS_POD)                                  \
  X(curBufAi, .key = "curBufAi", .kind = SS_POD)                              \
  X(ps, .key = "ps", .kind = SS_POD)                                          \
  X(inactiveMp, .key = "inactiveMp", .kind = SS_POD)                          \
  X(starting, .key = "starting", .kind = SS_POD)                              \
  X(startTimer, .key = "startTimer", .kind = SS_POD)                          \
  X(matchTimer, .key = "matchTimer", .kind = SS_POD)                          \
  X(stageSelect, .key = "stageSelect", .kind = SS_POD)                        \
  X(stageKind, .key = "stageKind", .kind = SS_POD)                            \
  X(cpuDifficulty, .key = "cpuDifficulty", .kind = SS_POD)                    \
  X(rng, .key = "rng", .kind = SS_POD)                                        \
  X(rngStateAtReset, .key = "rngStateAtReset", .kind = SS_POD)                \
  X(rngStateAtFrame1, .key = "rngStateAtFrame1", .kind = SS_POD)              \
  X(bridge, .key = "bridge", .kind = SS_BRIDGE,                               \
    .recon = "ml_ai_bridge_load (sim_main.c) before ss_load")                 \
  X(hasBridge, .key = "hasBridge", .kind = SS_POD)                            \
  X(frame, .key = "frame", .kind = SS_POD)

typedef struct {
  const char *key;
  SsKind kind;
  size_t off;   // offsetof(GameState, <field>)  — set by SS_ROW
  size_t bytes; // sizeof the whole field        — set by SS_ROW
  const char *recon;
} SsField;

#define SS_ROW(nm, ...)                                                       \
  {.off = offsetof(GameState, nm),                                            \
   .bytes = sizeof(((GameState *)0)->nm),                                     \
   __VA_ARGS__},

static const SsField SS_TABLE[] = {SS_FIELDS(SS_ROW)};
#define SS_COUNT ((int)(sizeof SS_TABLE / sizeof SS_TABLE[0]))

// --- THE GUARD THE MECHANISM IS FOR -----------------------------------------
//
// The table's byte total is derived from the SAME list the table is, so the
// two cannot drift, and it is compared against sizeof(GameState) at COMPILE
// TIME. Add a member to GameState and this stops holding.
#define SS_ROW_BYTES(nm, ...) +sizeof(((GameState *)0)->nm)
enum { SS_TABLE_BYTES = 0 SS_FIELDS(SS_ROW_BYTES) };

// EVERY byte of GameState that no row covers, declared in ONE place: the
// five ALIGNMENT HOLES between members, and nothing else (there is no
// trailing padding — asserted by the equality below, which has no slack).
//
// Each hole is written as an ALIGNMENT EXPRESSION rather than a measured
// number, and each is then asserted equal to the hole the compiler actually
// left. That is why this file needs no per-architecture number: armv7
// (`long` and pointers 4 bytes wide) moves three of the five, and each
// expression moves with it. It is also a real guard — if a change elsewhere
// alters GameState's internal alignment, the total stops matching and the
// author is told which hole moved rather than being told only that the size
// changed.
//
// WHAT IT DOES NOT CATCH, said plainly. A `bool` added INTO one of these
// holes changes neither sizeof(GameState) nor any offsetof, so no
// compile-time expression over this struct can see it: the size assertion
// passes and so does every gap assertion. The member would go unpersisted and
// the build would be silent about it. That hole is closed OUTSIDE the
// compiler, by check-sim-snapshot.sh leg [7], which derives GameState's
// member list from sim.h and diffs it against SS_FIELDS' row keys — a member
// with no row fails there, wherever it sits. (foh_persist.c closes the same
// hole by having no slack at all to hide in; GameState is the sim's core
// struct and reordering it to buy that is not worth the risk, so the guard
// moves to the check instead of the layout moving to suit the guard.)
#define SS_GAP(cur, next)                                                     \
  (offsetof(GameState, next) -                                                \
   (offsetof(GameState, cur) + sizeof(((GameState *)0)->cur)))

#define SS_GAP_SLOTISAI (_Alignof(MlInputBuffer) - sizeof(((GameState *)0)->slotIsAi))
#define SS_GAP_STARTING (_Alignof(double) - sizeof(((GameState *)0)->starting))
#define SS_GAP_STAGEKIND (_Alignof(double) - sizeof(((GameState *)0)->stageKind))
#define SS_GAP_BRIDGE (_Alignof(MlAiBridge) >= 8 ? (size_t)4 : (size_t)0)
#define SS_GAP_HASBRIDGE (_Alignof(long) - sizeof(((GameState *)0)->hasBridge))

_Static_assert(SS_GAP(slotIsAi, curBuf) == SS_GAP_SLOTISAI,
               "GameState's slotIsAi/curBuf alignment hole is not the size\n"
               "its alignment expression says it is. The struct's internal\n"
               "layout moved: re-read SS_PAD_BYTES above (and note that a\n"
               "member hiding INSIDE a hole is invisible here by construction\n"
               "— check-sim-snapshot.sh leg [7] is what catches that).");

_Static_assert(SS_GAP(starting, startTimer) == SS_GAP_STARTING,
               "GameState's starting/startTimer alignment hole is not the size\n"
               "its alignment expression says it is. The struct's internal\n"
               "layout moved: re-read SS_PAD_BYTES above (and note that a\n"
               "member hiding INSIDE a hole is invisible here by construction\n"
               "— check-sim-snapshot.sh leg [7] is what catches that).");

_Static_assert(SS_GAP(stageKind, cpuDifficulty) == SS_GAP_STAGEKIND,
               "GameState's stageKind/cpuDifficulty alignment hole is not the size\n"
               "its alignment expression says it is. The struct's internal\n"
               "layout moved: re-read SS_PAD_BYTES above (and note that a\n"
               "member hiding INSIDE a hole is invisible here by construction\n"
               "— check-sim-snapshot.sh leg [7] is what catches that).");

_Static_assert(SS_GAP(rngStateAtFrame1, bridge) == SS_GAP_BRIDGE,
               "GameState's rngStateAtFrame1/bridge alignment hole is not the size\n"
               "its alignment expression says it is. The struct's internal\n"
               "layout moved: re-read SS_PAD_BYTES above (and note that a\n"
               "member hiding INSIDE a hole is invisible here by construction\n"
               "— check-sim-snapshot.sh leg [7] is what catches that).");

_Static_assert(SS_GAP(hasBridge, frame) == SS_GAP_HASBRIDGE,
               "GameState's hasBridge/frame alignment hole is not the size\n"
               "its alignment expression says it is. The struct's internal\n"
               "layout moved: re-read SS_PAD_BYTES above (and note that a\n"
               "member hiding INSIDE a hole is invisible here by construction\n"
               "— check-sim-snapshot.sh leg [7] is what catches that).");

#define SS_PAD_BYTES                                                          \
  (SS_GAP_SLOTISAI + SS_GAP_STARTING + SS_GAP_STAGEKIND + SS_GAP_BRIDGE +     \
   SS_GAP_HASBRIDGE)

_Static_assert(sizeof(GameState) == SS_TABLE_BYTES + SS_PAD_BYTES,
               "GameState changed size. The sim-state SNAPSHOT FIELD TABLE: "
               "add the new member to SS_FIELDS (one row), marking it "
               "SS_RECON if it is a pointer (a pointer is reconstructed, "
               "never copied) — or, if it is deliberately not part of the "
               "match, say so here and account for its bytes. Ticket #28.");

// ---------------------------------------------------------------------------
// THE MODULE-STATE ROWS
// ---------------------------------------------------------------------------
//
// Sim state that is NOT in GameState because upstream keeps it in module
// scope and the port is structure-parallel (sim_modstate.h states the case,
// and sim-modstate.frozen.txt is the ledger of every mutable file-scope
// static in the sim with its classification). These are appended to the
// payload after the GameState rows, in this order.

static size_t mod_scalar_double_bytes(void) { return sizeof(double); }
static void mod_howl_save(void *dst) {
  const double v = sim_tick_howl_counter_get();
  memcpy(dst, &v, sizeof v);
}
static void mod_howl_load(const void *src) {
  double v;
  memcpy(&v, src, sizeof v);
  sim_tick_howl_counter_set(v);
}

static size_t mod_playcount_bytes(void) { return sizeof(unsigned long long); }
static void mod_playcount_save(void *dst) {
  const unsigned long long v = ml_events_play_count_get();
  memcpy(dst, &v, sizeof v);
}
static void mod_playcount_load(const void *src) {
  unsigned long long v;
  memcpy(&v, src, sizeof v);
  ml_events_play_count_set(v);
}

static size_t mod_ssg_bytes(void) { return sizeof(uint8_t); }
static void mod_ssg_save(void *dst) {
  const uint8_t v = mv_falcon_ssg_get_canEdgeCancel() ? 1u : 0u;
  memcpy(dst, &v, sizeof v);
}
static void mod_ssg_load(const void *src) {
  uint8_t v;
  memcpy(&v, src, sizeof v);
  mv_falcon_ssg_set_canEdgeCancel(v != 0);
}

typedef struct {
  const char *key;
  size_t (*bytes)(void);
  void (*save)(void *dst);
  void (*load)(const void *src);
} SsModule;

static const SsModule SS_MODULES[] = {
    // The rule-17 live charHitboxes plane. Move code writes it through stale
    // id aliases (measured: puff's jab1 dmg 3->7 on g04) and it is not
    // recoverable from CTAB1 once a match has started.
    {"mod:chd", sim_chd_snap_bytes, sim_chd_snap_save, sim_chd_snap_load},
    // marth's shieldBreakerID mint (sim_tick.c) and its M4 successor count
    // (ml_events.c). The ids they hand out are stored in players, so a
    // resumed match that restarted either counter would re-issue a live id.
    {"mod:howl", mod_scalar_double_bytes, mod_howl_save, mod_howl_load},
    {"mod:playcount", mod_playcount_bytes, mod_playcount_save,
     mod_playcount_load},
    // falcon SIDESPECIALGROUND's `this.canEdgeCancel` — a move-object scalar,
    // i.e. module state, read back by physics' edge-cancel arm.
    {"mod:falconSsgEdgeCancel", mod_ssg_bytes, mod_ssg_save, mod_ssg_load},
};
#define SS_MOD_COUNT ((int)(sizeof SS_MODULES / sizeof SS_MODULES[0]))

// ---------------------------------------------------------------------------
// wire sizes
// ---------------------------------------------------------------------------

// The AI-bridge codec's wire form: the four identifying header fields (which
// are VERIFIED on load, never installed) followed by the cursor (which is
// the state).
#define SS_BRIDGE_WIRE (sizeof(((MlAiBridge *)0)->golden) + sizeof(uint32_t) + \
                        sizeof(long) + sizeof(long) + sizeof(long))

static size_t row_wire_bytes(const SsField *f) {
  switch (f->kind) {
  case SS_POD:
    return f->bytes;
  case SS_RECON:
    return 0;
  case SS_BRIDGE:
    return SS_BRIDGE_WIRE;
  }
  return 0;
}

size_t ss_payload_bytes(void) {
  size_t n = 0;
  for (int j = 0; j < SS_COUNT; j++) n += row_wire_bytes(&SS_TABLE[j]);
  for (int j = 0; j < SS_MOD_COUNT; j++) n += SS_MODULES[j].bytes();
  return n;
}

// ---------------------------------------------------------------------------
// build identity
// ---------------------------------------------------------------------------

// The default is this TU's own build stamp, which makes ANY rebuild a
// different build. That is deliberately conservative (sim_snapshot.h says
// what it does and does not catch); a build system with something stronger
// to say pins it here.
#ifndef MLSNAP_BUILD_TAG
#define MLSNAP_BUILD_TAG (__DATE__ " " __TIME__)
#endif

#define SS_MAGIC "MLSIM1"

static void hex32(const uint8_t d[32], char out[65]) {
  static const char *H = "0123456789abcdef";
  for (int k = 0; k < 32; k++) {
    out[2 * k] = H[d[k] >> 4];
    out[2 * k + 1] = H[d[k] & 15];
  }
  out[64] = 0;
}

void ss_build_identity(char out[65]) {
  // A canonical descriptor, one line per fact. Built into a bounded buffer;
  // the append is length-checked so a future table cannot silently truncate
  // the thing the identity is computed over.
  static char desc[16384];
  size_t n = 0;
  int w = snprintf(desc + n, sizeof desc - n,
                   "%s\nstate %zu\nrows %d\nmods %d\npayload %zu\n", SS_MAGIC,
                   sizeof(GameState), SS_COUNT, SS_MOD_COUNT,
                   ss_payload_bytes());
  if (w < 0 || (size_t)w >= sizeof desc - n) sim_fatal("ss identity overflow");
  n += (size_t)w;
  for (int j = 0; j < SS_COUNT; j++) {
    const SsField *f = &SS_TABLE[j];
    w = snprintf(desc + n, sizeof desc - n, "f %s %d %zu %zu %zu %s\n", f->key,
                 (int)f->kind, f->off, f->bytes, row_wire_bytes(f),
                 f->recon ? f->recon : "-");
    if (w < 0 || (size_t)w >= sizeof desc - n)
      sim_fatal("ss identity overflow");
    n += (size_t)w;
  }
  for (int j = 0; j < SS_MOD_COUNT; j++) {
    w = snprintf(desc + n, sizeof desc - n, "m %s %zu\n", SS_MODULES[j].key,
                 SS_MODULES[j].bytes());
    if (w < 0 || (size_t)w >= sizeof desc - n)
      sim_fatal("ss identity overflow");
    n += (size_t)w;
  }
  w = snprintf(desc + n, sizeof desc - n, "tag %s\n", MLSNAP_BUILD_TAG);
  if (w < 0 || (size_t)w >= sizeof desc - n) sim_fatal("ss identity overflow");
  n += (size_t)w;

  uint8_t dig[32];
  sha256((const uint8_t *)desc, n, dig);
  hex32(dig, out);
}

// ---------------------------------------------------------------------------
// the file
// ---------------------------------------------------------------------------

#define SS_HDR_BYTES ((size_t)(7 + 71 + 27)) // magic + BUILD + BYTES lines
#define SS_SUM_BYTES ((size_t)(4 + 64 + 1))  // "SUM " + hex + "\n"

static size_t file_bytes(void) {
  return SS_HDR_BYTES + ss_payload_bytes() + SS_SUM_BYTES;
}

// --- the AI-bridge codec ------------------------------------------------------

static void bridge_put(const MlAiBridge *br, uint8_t *dst) {
  size_t n = 0;
  memcpy(dst + n, br->golden, sizeof br->golden);
  n += sizeof br->golden;
  memcpy(dst + n, &br->seed, sizeof br->seed);
  n += sizeof br->seed;
  memcpy(dst + n, &br->boot, sizeof br->boot);
  n += sizeof br->boot;
  memcpy(dst + n, &br->nentries, sizeof br->nentries);
  n += sizeof br->nentries;
  memcpy(dst + n, &br->cursor, sizeof br->cursor);
}

// Verifies the identifying header against the LIVE artifact (which the
// caller reconstructed) and installs only the cursor. A snapshot taken
// against a different AI recording is refused by name rather than resumed
// into the wrong AI stream.
static bool bridge_get(MlAiBridge *br, const uint8_t *src,
                       const char **reason) {
  char golden[sizeof br->golden];
  uint32_t seed;
  long boot, nentries, cursor;
  size_t n = 0;
  memcpy(golden, src + n, sizeof golden);
  n += sizeof golden;
  memcpy(&seed, src + n, sizeof seed);
  n += sizeof seed;
  memcpy(&boot, src + n, sizeof boot);
  n += sizeof boot;
  memcpy(&nentries, src + n, sizeof nentries);
  n += sizeof nentries;
  memcpy(&cursor, src + n, sizeof cursor);
  if (memcmp(golden, br->golden, sizeof golden) != 0 || seed != br->seed ||
      boot != br->boot || nentries != br->nentries) {
    *reason = "snapshot was taken against a different AI bridge artifact";
    return false;
  }
  if (cursor < 0 || cursor > nentries) {
    *reason = "AI bridge cursor out of range";
    return false;
  }
  br->cursor = cursor;
  return true;
}

// --- payload ------------------------------------------------------------------

static void payload_put(const GameState *g, uint8_t *p) {
  size_t at = 0;
  for (int j = 0; j < SS_COUNT; j++) {
    const SsField *f = &SS_TABLE[j];
    const size_t w = row_wire_bytes(f);
    if (w == 0) continue; // SS_RECON: never written
    if (f->kind == SS_BRIDGE) {
      bridge_put((const MlAiBridge *)(const void *)((const char *)g + f->off),
                 p + at);
    } else {
      memcpy(p + at, (const char *)g + f->off, w);
    }
    at += w;
  }
  for (int j = 0; j < SS_MOD_COUNT; j++) {
    SS_MODULES[j].save(p + at);
    at += SS_MODULES[j].bytes();
  }
}

// MLFK_SNAP_SKIP — the COMPLETENESS TOOTH (sim_snapshot.h, CONTEXT.md
// "Tooth"). Naming a row here makes ss_load read past it without applying
// it, which is exactly the shape of the bug the continuation test exists to
// catch: a field that was never saved. It is an env-gated diagnostic in the
// same family as oracle/qjs's QJS_ORACLE_NO_REPOINT=1, and
// check-sim-snapshot.sh uses it to prove the continuation test can still
// fail. It is never set by any product path.
static const char *skip_key(void) {
  static const char *k;
  static bool asked;
  if (!asked) {
    asked = true;
    k = getenv("MLFK_SNAP_SKIP");
    if (k && *k == 0) k = NULL;
  }
  return k;
}

static bool payload_get(GameState *g, const uint8_t *p, const char **reason) {
  const char *skip = skip_key();
  size_t at = 0;
  for (int j = 0; j < SS_COUNT; j++) {
    const SsField *f = &SS_TABLE[j];
    const size_t w = row_wire_bytes(f);
    if (w == 0) continue; // SS_RECON: the caller rebuilt it
    const bool skipThis = skip && strcmp(skip, f->key) == 0;
    if (!skipThis) {
      if (f->kind == SS_BRIDGE) {
        if (!bridge_get((MlAiBridge *)(void *)((char *)g + f->off), p + at,
                        reason))
          return false;
      } else {
        memcpy((char *)g + f->off, p + at, w);
      }
    }
    at += w;
  }
  for (int j = 0; j < SS_MOD_COUNT; j++) {
    const size_t w = SS_MODULES[j].bytes();
    if (!(skip && strcmp(skip, SS_MODULES[j].key) == 0))
      SS_MODULES[j].load(p + at);
    at += w;
  }
  return true;
}

// --- save ---------------------------------------------------------------------

bool ss_save(const GameState *g, const char *path, const char **reason) {
  *reason = NULL;
  const size_t plen = ss_payload_bytes();
  const size_t total = file_bytes();
  uint8_t *buf = malloc(total);
  if (!buf) {
    *reason = "out of memory";
    return false;
  }
  char id[65];
  ss_build_identity(id);
  const int w = snprintf((char *)buf, SS_HDR_BYTES + 1,
                         SS_MAGIC "\nBUILD %s\nBYTES %020zu\n", id, plen);
  if (w < 0 || (size_t)w != SS_HDR_BYTES) {
    free(buf);
    *reason = "header format";
    return false;
  }
  payload_put(g, buf + SS_HDR_BYTES);
  uint8_t dig[32];
  char hex[65];
  sha256(buf, SS_HDR_BYTES + plen, dig);
  hex32(dig, hex);
  const int w2 = snprintf((char *)buf + SS_HDR_BYTES + plen, SS_SUM_BYTES + 1,
                          "SUM %s\n", hex);
  if (w2 < 0 || (size_t)w2 != SS_SUM_BYTES) {
    free(buf);
    *reason = "SUM format";
    return false;
  }

  // Atomic publish: write a sibling temp then rename over the real name, so
  // a power loss cannot leave a half-written snapshot under a name a resume
  // would trust. This is foh_persist_save's discipline, not a second one.
  char tmp[1024];
  const int wp = snprintf(tmp, sizeof tmp, "%s.tmp", path);
  if (wp < 0 || (size_t)wp >= sizeof tmp) {
    free(buf);
    *reason = "snapshot path too long";
    return false;
  }
  FILE *f = fopen(tmp, "wb");
  if (!f) {
    free(buf);
    *reason = "cannot open snapshot for writing";
    return false;
  }
  const size_t got = fwrite(buf, 1, total, f);
  const bool wrote = got == total;
  const bool closed = fclose(f) == 0;
  free(buf);
  if (!wrote || !closed) {
    remove(tmp);
    *reason = "snapshot write failed";
    return false;
  }
  if (rename(tmp, path) != 0) {
    remove(tmp);
    *reason = "snapshot publish (rename) failed";
    return false;
  }
  return true;
}

// --- load ---------------------------------------------------------------------

bool ss_load(GameState *g, const char *path, const char **reason) {
  *reason = NULL;
  const size_t plen = ss_payload_bytes();
  const size_t total = file_bytes();

  // BOUNDED READ. One byte of headroom so a file of exactly the right size
  // still hits EOF, and one byte over is caught as too large instead of
  // being silently truncated into a plausible-looking snapshot.
  uint8_t *buf = malloc(total + 1);
  if (!buf) {
    *reason = "out of memory";
    return false;
  }
  FILE *f = fopen(path, "rb");
  if (!f) {
    free(buf);
    *reason = "no such snapshot file";
    return false;
  }
  const size_t n = fread(buf, 1, total + 1, f);
  const bool err = ferror(f) != 0;
  const bool more = feof(f) == 0;
  fclose(f);
  if (err) {
    free(buf);
    *reason = "snapshot read error";
    return false;
  }
  if (more || n > total) {
    free(buf);
    *reason = "snapshot file too large";
    return false;
  }

  // --- INTEGRITY FIRST, MEANING SECOND. Nothing below reads a field until
  // the SUM has verified; a corrupt file is refused as corrupt, not as
  // "from a different build".
  const size_t magic = sizeof(SS_MAGIC) - 1;
  if (n < SS_HDR_BYTES + SS_SUM_BYTES ||
      memcmp(buf, SS_MAGIC "\n", magic + 1) != 0) {
    free(buf);
    *reason = n < SS_HDR_BYTES + SS_SUM_BYTES
                  ? "truncated snapshot (shorter than a header)"
                  : "not a snapshot file (bad magic line)";
    return false;
  }
  if (n != total) {
    free(buf);
    *reason = "truncated snapshot (wrong length for this build)";
    return false;
  }
  if (buf[n - 1] != '\n') {
    free(buf);
    *reason = "truncated snapshot (no final newline)";
    return false;
  }
  const size_t sumStart = n - SS_SUM_BYTES;
  if (memcmp(buf + sumStart, "SUM ", 4) != 0) {
    free(buf);
    *reason = "missing SUM line";
    return false;
  }
  {
    uint8_t dig[32];
    char hex[65];
    sha256(buf, sumStart, dig);
    hex32(dig, hex);
    for (int k = 0; k < 64; k++) {
      const char c = (char)buf[sumStart + 4 + k];
      if (c != hex[k]) {
        const bool isHex = (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f');
        free(buf);
        *reason = isHex ? "SUM mismatch (snapshot corrupt or edited)"
                        : "malformed SUM (not 64 lowercase hex)";
        return false;
      }
    }
  }

  // --- grammar, then identity, then the fields.
  if (memcmp(buf + magic + 1, "BUILD ", 6) != 0 ||
      buf[magic + 1 + 6 + 64] != '\n') {
    free(buf);
    *reason = "malformed BUILD line";
    return false;
  }
  {
    char id[65];
    ss_build_identity(id);
    if (memcmp(buf + magic + 1 + 6, id, 64) != 0) {
      free(buf);
      *reason = "snapshot is from a different build (build identity mismatch)";
      return false;
    }
  }
  const size_t bytesLine = magic + 1 + 71;
  if (memcmp(buf + bytesLine, "BYTES ", 6) != 0 ||
      buf[bytesLine + 6 + 20] != '\n') {
    free(buf);
    *reason = "malformed BYTES line";
    return false;
  }
  {
    size_t declared = 0;
    for (int k = 0; k < 20; k++) {
      const char c = (char)buf[bytesLine + 6 + k];
      if (c < '0' || c > '9') {
        free(buf);
        *reason = "malformed BYTES line (not 20 decimal digits)";
        return false;
      }
      declared = declared * 10 + (size_t)(c - '0');
    }
    if (declared != plen) {
      free(buf);
      *reason = "snapshot payload length disagrees with this build";
      return false;
    }
  }

  const bool ok = payload_get(g, buf + SS_HDR_BYTES, reason);
  free(buf);
  return ok;
}

// ---------------------------------------------------------------------------
// the byte round trip
// ---------------------------------------------------------------------------

#define SS_POISON 0xA5

bool ss_round_trip(GameState *g, const char *pathA, const char *pathB,
                   const char **reason) {
  *reason = NULL;
  const size_t plen = ss_payload_bytes();
  GameState *keep = malloc(sizeof *keep);
  uint8_t *modKeep = malloc(plen ? plen : 1);
  uint8_t *modNow = malloc(plen ? plen : 1);
  uint8_t *poison = malloc(plen ? plen : 1);
  if (!keep || !modKeep || !modNow || !poison) {
    free(keep);
    free(modKeep);
    free(modNow);
    free(poison);
    *reason = "out of memory";
    return false;
  }
  *keep = *g;
  {
    size_t at = 0;
    for (int j = 0; j < SS_MOD_COUNT; j++) {
      SS_MODULES[j].save(modKeep + at);
      at += SS_MODULES[j].bytes();
    }
  }

  bool ok = ss_save(g, pathA, reason);

  // POISON every persisted byte. Without this the read-back would compare a
  // value against itself and a serialiser that dropped a field would pass
  // forever. SS_RECON rows are deliberately NOT poisoned — being
  // reconstructed by the caller is what the kind means, and the bridge's
  // pointer plane is the caller's to own.
  if (ok) {
    memset(poison, SS_POISON, plen);
    for (int j = 0; j < SS_COUNT; j++) {
      const SsField *f = &SS_TABLE[j];
      if (f->kind == SS_POD) {
        memset((char *)g + f->off, SS_POISON, f->bytes);
      } else if (f->kind == SS_BRIDGE) {
        MlAiBridge *br = (MlAiBridge *)(void *)((char *)g + f->off);
        br->cursor = -424242; // the one persisted scalar
      }
    }
    {
      size_t at = 0;
      for (int j = 0; j < SS_MOD_COUNT; j++) {
        SS_MODULES[j].load(poison + at);
        at += SS_MODULES[j].bytes();
      }
    }
  }

  if (ok) ok = ss_load(g, pathA, reason);

  // Every persisted row must have come back byte-identical.
  if (ok) {
    for (int j = 0; j < SS_COUNT && ok; j++) {
      const SsField *f = &SS_TABLE[j];
      static char msg[160];
      if (f->kind == SS_POD) {
        if (memcmp((const char *)g + f->off, (const char *)keep + f->off,
                   f->bytes) != 0) {
          snprintf(msg, sizeof msg, "row '%s' did not round trip", f->key);
          *reason = msg;
          ok = false;
        }
      } else if (f->kind == SS_BRIDGE) {
        const MlAiBridge *a = (const MlAiBridge *)(const void *)((const char *)g + f->off);
        const MlAiBridge *b = (const MlAiBridge *)(const void *)((const char *)keep + f->off);
        if (a->cursor != b->cursor) {
          snprintf(msg, sizeof msg, "row '%s' cursor did not round trip",
                   f->key);
          *reason = msg;
          ok = false;
        }
      }
    }
  }
  if (ok) {
    size_t at = 0;
    for (int j = 0; j < SS_MOD_COUNT && ok; j++) {
      const size_t w = SS_MODULES[j].bytes();
      SS_MODULES[j].save(modNow + at);
      if (memcmp(modNow + at, modKeep + at, w) != 0) {
        static char msg[160];
        snprintf(msg, sizeof msg, "module row '%s' did not round trip",
                 SS_MODULES[j].key);
        *reason = msg;
        ok = false;
      }
      at += w;
    }
  }

  // And writing the restored state again must produce the same bytes.
  if (ok) ok = ss_save(g, pathB, reason);
  if (ok) {
    FILE *fa = fopen(pathA, "rb");
    FILE *fb = fopen(pathB, "rb");
    if (!fa || !fb) {
      if (fa) fclose(fa);
      if (fb) fclose(fb);
      *reason = "cannot reopen snapshots to compare";
      ok = false;
    } else {
      int ca, cb;
      size_t at = 0;
      do {
        ca = fgetc(fa);
        cb = fgetc(fb);
        if (ca != cb) {
          static char msg[160];
          snprintf(msg, sizeof msg, "snapshots differ at byte %zu", at);
          *reason = msg;
          ok = false;
          break;
        }
        at++;
      } while (ca != EOF);
      fclose(fa);
      fclose(fb);
    }
  }

  free(keep);
  free(modKeep);
  free(modNow);
  free(poison);
  return ok;
}

// ---------------------------------------------------------------------------
// THE DRIVER SEAM
// ---------------------------------------------------------------------------
//
// sim_main.c calls two function pointers that are NULL unless this TU is
// linked (the ml_sim_runai_live / tp_custom_setup pattern): the M2 EXIT
// GATE's frozen TU list does not carry sim_snapshot.c, so check-sim.sh
// builds the same binary it always did. The controls are ENVIRONMENT
// VARIABLES rather than new command-line flags for the same reason —
// sim_main.c's argument parsing (and the two-line rejection message
// check-ai-live.sh pins) is left exactly as it was.
//
//   MLFK_SNAP_OUT=<path>   write a snapshot ...
//   MLFK_SNAP_AT=<frame>   ... after this frame's tick and hash
//   MLFK_SNAP_STOP=1       and exit(0) immediately after writing
//   MLFK_SNAP_ROUNDTRIP=1  also do the in-process byte round trip there
//   MLFK_SNAP_IN=<path>    restore before the frame loop and resume
//   MLFK_SNAP_SKIP=<row>   the completeness tooth (see skip_key above)

static void snap_die(const char *what, const char *why) {
  fprintf(stderr, "SNAP FAIL: %s: %s\n", what, why ? why : "(no reason)");
  exit(2);
}

static void snap_frame(GameState *g, long frame) {
  const char *out = getenv("MLFK_SNAP_OUT");
  const char *at = getenv("MLFK_SNAP_AT");
  if (!out || !at) return;
  if (strtol(at, NULL, 10) != frame) return;
  const char *why = NULL;
  if (getenv("MLFK_SNAP_ROUNDTRIP")) {
    char pathB[1024];
    const int w = snprintf(pathB, sizeof pathB, "%s.b", out);
    if (w < 0 || (size_t)w >= sizeof pathB) snap_die("round trip", "path");
    if (!ss_round_trip(g, out, pathB, &why)) snap_die("round trip", why);
    fprintf(stderr, "SNAP ROUNDTRIP OK frame %ld payload %zu\n", frame,
            ss_payload_bytes());
  } else if (!ss_save(g, out, &why)) {
    snap_die("save", why);
  }
  fprintf(stderr, "SNAP WROTE %s frame %ld payload %zu\n", out, frame,
          ss_payload_bytes());
  if (getenv("MLFK_SNAP_STOP")) exit(0);
}

static long snap_boot(GameState *g) {
  const char *in = getenv("MLFK_SNAP_IN");
  if (!in) return 0;
  const char *why = NULL;
  if (!ss_load(g, in, &why)) snap_die("restore", why);
  fprintf(stderr, "SNAP RESTORED %s frame %ld\n", in, g->frame);
  return g->frame; // the loop resumes at the frame AFTER the one saved
}

__attribute__((constructor)) static void install_snapshot_seam(void) {
  ml_sim_snap_frame = snap_frame;
  ml_sim_snap_boot = snap_boot;
}
