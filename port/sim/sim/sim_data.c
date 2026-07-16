// sim_data.c — the integrated sim host's DATA PLANE (M2 task 17; sim.h).
//
// Loads the SIMDATA1 artifact (port/sim/calib/dump-sim-data.js): five
// tab-labeled canon-v1.1 lines — asFlags / hdFlags / mvChars / mvCharData /
// palettes0 — and serves them through STRONG definitions of the extern
// data seams the module clusters declare:
//   - mlp_flags (physics.h)  — with the falcon SIDESPECIALGROUND
//     canEdgeCancel RUNTIME overlay (falcon/moves/SIDESPECIALGROUND.c's
//     module state; its only sim reader is this flag lookup)
//   - hd_flags (hit_detection.h)
//   - mv_setVelocity / mv_setPosition_capturedamage /
//     mv_posOffsetCliffCatch / mv_actionSounds / mv_palette0
//     (characters/shared/moves.h) — lifted from replay_moves_puff.c
//   - mv_{marth,puff,fox,falco,falcon}_{arr,pair,arr_len}
//     (characters/<char>/moves.h) — identical semantics per char, lifted
//     from the per-char replay drivers (replay_moves_fox.c et al.)
//   - the rule-17 LIVE charHitboxes plane: strong mv_chd_assign_note /
//     mv_chd_write_dmg / mv_chd_write_size (overriding the weak no-ops in
//     shared moves_index.c) + pf_assign_hitbox_id (puff/moves.h) +
//     sim_chd_reset
// plus sim_data_register(): the actionStates registries for all five
// chars, the checkForIASA char-module indexes, and the composed
// special-phase lookup.
//
// The canon parser's ONE global bump arena is reset by the per-frame
// serializer, so every loader here DEEP-COPIES into static storage and
// retains no CanonVal or arena-string pointer after sim_data_load returns.
// All fatal paths route to sim_fatal (loud; HARD RULE 2 — no silent
// stubs, no guessed shapes: any artifact byte outside the lifted loaders'
// domain aborts).
#include "sim.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../calib/canon.h"
#include "../characters/falco/moves.h"
#include "../characters/falcon/moves.h"
#include "../characters/fox/moves.h"
#include "../characters/marth/moves.h"
#include "../characters/puff/moves.h"

// --- fatal helpers (everything routes to sim_fatal) ---------------------------

static void sd_fatal(const char *what) __attribute__((noreturn));
static void sd_fatal(const char *what) {
  static char buf[512];
  snprintf(buf, sizeof buf, "sim_data: %s", what);
  sim_fatal(buf);
}

static void sd_fatal2(const char *what, const char *detail)
    __attribute__((noreturn));
static void sd_fatal2(const char *what, const char *detail) {
  static char buf[512];
  snprintf(buf, sizeof buf, "sim_data: %s (%s)", what,
           detail ? detail : "?");
  sim_fatal(buf);
}

// --- small marshal helpers (the replay drivers' cv_* discipline) --------------

static double cv_num(const CanonVal *v) {
  if (v->type != CV_NUM) sd_fatal("expected number");
  return v->num;
}
static bool cv_bool(const CanonVal *v) {
  if (v->type != CV_BOOL) sd_fatal("expected boolean");
  return v->b;
}
static const char *cv_str(const CanonVal *v) {
  if (v->type != CV_STR) sd_fatal("expected string");
  return v->str;
}
static const CanonVal *obj_get(const CanonVal *o, const char *key) {
  if (o->type != CV_OBJ) sd_fatal("expected object");
  for (int i = 0; i < o->nkeys; i++) {
    if (strcmp(o->keys[i], key) == 0) return o->vals[i];
  }
  return 0;
}
static const CanonVal *obj_req(const CanonVal *o, const char *key) {
  const CanonVal *v = obj_get(o, key);
  if (!v) sd_fatal2("missing object key", key);
  return v;
}
// truthiness domain: undefined|bool|number (the flag dumps' raw values)
static bool ft_truthy(const CanonVal *v, const char *what) {
  if (v->type == CV_UNDEF) return false;
  if (v->type == CV_BOOL) return v->b;
  if (v->type == CV_NUM) return v->num != 0 && v->num == v->num;
  sd_fatal2("flag truthiness domain", what);
}
static void sd_copy(char *dst, size_t cap, const char *s, const char *what) {
  if (strlen(s) >= cap) sd_fatal2("string too long", what);
  strcpy(dst, s);
}

// --- the asFlags table (mlp_flags seam) ---------------------------------------
// Loader lifted nearly verbatim from replay_physics.c load_flags (the
// capture's frame-0 asFlags dump shape: {charId: {state: {14 flag keys}}}).

#define SD_FT_STATES 256
#define SD_FT_STR 48

typedef struct {
  char state[SD_FT_STR];
  char name[SD_FT_STR];
  MlAsFlags f;
} SdAsEntry;

static SdAsEntry g_as[ML_CHARS][SD_FT_STATES];
static int g_as_count[ML_CHARS];
static bool g_as_loaded = false;

static void load_asflags(const CanonVal *dump) {
  if (g_as_loaded) sd_fatal("asFlags: seen twice");
  if (dump->type != CV_OBJ) sd_fatal("asFlags: expected object");
  for (int ci = 0; ci < dump->nkeys; ci++) {
    const int c = atoi(dump->keys[ci]);
    if (c < 0 || c >= ML_CHARS) sd_fatal("asFlags: char key out of range");
    const CanonVal *tbl = dump->vals[ci];
    if (tbl->type != CV_OBJ) sd_fatal("asFlags: expected state table object");
    if (tbl->nkeys > SD_FT_STATES) sd_fatal("asFlags: too many states");
    g_as_count[c] = tbl->nkeys;
    for (int si = 0; si < tbl->nkeys; si++) {
      SdAsEntry *e = &g_as[c][si];
      memset(e, 0, sizeof *e);
      sd_copy(e->state, SD_FT_STR, tbl->keys[si], "asFlags state name");
      const CanonVal *fo = tbl->vals[si];
      if (fo->type != CV_OBJ || fo->nkeys != 14) {
        sd_fatal("asFlags: flag key count");
      }
      const CanonVal *v;
      v = obj_req(fo, "name");
      sd_copy(e->name, SD_FT_STR, cv_str(v), "asFlags move name");
      e->f.name = e->name;
      e->f.canEdgeCancel =
          ft_truthy(obj_req(fo, "canEdgeCancel"), "canEdgeCancel");
      e->f.disableTeeter =
          ft_truthy(obj_req(fo, "disableTeeter"), "disableTeeter");
      e->f.inGrab = ft_truthy(obj_req(fo, "inGrab"), "inGrab");
      e->f.headBonk = ft_truthy(obj_req(fo, "headBonk"), "headBonk");
      e->f.specialWallCollide =
          ft_truthy(obj_req(fo, "specialWallCollide"), "specialWallCollide");
      e->f.canPassThrough =
          ft_truthy(obj_req(fo, "canPassThrough"), "canPassThrough");
      e->f.dead = ft_truthy(obj_req(fo, "dead"), "dead");
      e->f.missfoot = ft_truthy(obj_req(fo, "missfoot"), "missfoot");
      e->f.ignoreCollision =
          ft_truthy(obj_req(fo, "ignoreCollision"), "ignoreCollision");
      v = obj_req(fo, "wallJumpAble");
      if (v->type == CV_UNDEF) e->f.wallJumpAble = js_bool_undef();
      else if (v->type == CV_BOOL) e->f.wallJumpAble = js_bool(v->b);
      else sd_fatal("asFlags: wallJumpAble domain");
      v = obj_req(fo, "landType");
      if (v->type == CV_UNDEF) e->f.hasLandType = false;
      else { e->f.hasLandType = true; e->f.landType = cv_num(v); }
      v = obj_req(fo, "airborneState");
      if (v->type == CV_UNDEF) e->f.hasAirborneState = false;
      else {
        e->f.hasAirborneState = true;
        sd_copy(e->f.airborneState, ML_STR_CAP, cv_str(v),
                "asFlags airborneState");
      }
      v = obj_req(fo, "canGrabLedge");
      if (v->type == CV_UNDEF) {
        // canGrabLedge[0] on undefined THROWS upstream — hasCanGrabLedge
        // false makes the C read trap (mirrors the throw).
        e->f.hasCanGrabLedge = false;
      } else if (v->type == CV_BOOL) {
        // measured domain includes plain `false`: boolean[0] is undefined
        // in JS (no throw) — both element reads are falsy.
        e->f.hasCanGrabLedge = true;
        e->f.canGrabLedge[0] = false;
        e->f.canGrabLedge[1] = false;
      } else {
        if (v->type != CV_ARR || v->count != 2) {
          sd_fatal("asFlags: canGrabLedge shape");
        }
        e->f.hasCanGrabLedge = true;
        e->f.canGrabLedge[0] = ft_truthy(v->items[0], "canGrabLedge[0]");
        e->f.canGrabLedge[1] = ft_truthy(v->items[1], "canGrabLedge[1]");
      }
    }
  }
  g_as_loaded = true;
}

const MlAsFlags *mlp_flags(const MlSim *sim, double charId,
                           const char *state) {
  (void)sim;
  const int c = (int)charId;
  if (!g_as_loaded) sd_fatal("mlp_flags before sim_data_load");
  if (c < 0 || c >= ML_CHARS) sd_fatal("mlp_flags: char id out of range");
  for (int k = 0; k < g_as_count[c]; k++) {
    if (strcmp(g_as[c][k].state, state) != 0) continue;
    // falcon SIDESPECIALGROUND's canEdgeCancel is RUNTIME move-table state
    // (SIDESPECIALGROUND.c: `this.canEdgeCancel = false/true` at init /
    // timer>16) — overlay the module state onto the dumped entry.
    if (c == 4 && strcmp(state, "SIDESPECIALGROUND") == 0) {
      static MlAsFlags scratch;
      scratch = g_as[c][k].f;
      scratch.canEdgeCancel = mv_falcon_ssg_get_canEdgeCancel();
      return &scratch;
    }
    return &g_as[c][k].f;
  }
  sd_fatal2("mlp_flags: unknown action state", state);
}

// --- the hdFlags table (hd_flags seam) ----------------------------------------
// Loader lifted from replay_hitdet.c load_flags ({charId: {state: {7 keys}}}).

typedef struct {
  char state[SD_FT_STR];
  char name[SD_FT_STR];
  HdFlags f;
} SdHdEntry;

static SdHdEntry g_hd[ML_CHARS][SD_FT_STATES];
static int g_hd_count[ML_CHARS];
static bool g_hd_loaded = false;

static void load_hdflags(const CanonVal *dump) {
  if (g_hd_loaded) sd_fatal("hdFlags: seen twice");
  if (dump->type != CV_OBJ) sd_fatal("hdFlags: expected object");
  for (int ci = 0; ci < dump->nkeys; ci++) {
    const int c = atoi(dump->keys[ci]);
    if (c < 0 || c >= ML_CHARS) sd_fatal("hdFlags: char key out of range");
    const CanonVal *tbl = dump->vals[ci];
    if (tbl->type != CV_OBJ) sd_fatal("hdFlags: expected state table object");
    if (tbl->nkeys > SD_FT_STATES) sd_fatal("hdFlags: too many states");
    g_hd_count[c] = tbl->nkeys;
    for (int si = 0; si < tbl->nkeys; si++) {
      SdHdEntry *e = &g_hd[c][si];
      memset(e, 0, sizeof *e);
      sd_copy(e->state, SD_FT_STR, tbl->keys[si], "hdFlags state name");
      const CanonVal *fo = tbl->vals[si];
      if (fo->type != CV_OBJ || fo->nkeys != 7) {
        sd_fatal("hdFlags: flag key count");
      }
      sd_copy(e->name, SD_FT_STR, cv_str(obj_req(fo, "name")),
              "hdFlags move name");
      e->f.name = e->name;
      e->f.canBeGrabbed =
          ft_truthy(obj_req(fo, "canBeGrabbed"), "canBeGrabbed");
      e->f.crouch = ft_truthy(obj_req(fo, "crouch"), "crouch");
      e->f.downed = ft_truthy(obj_req(fo, "downed"), "downed");
      e->f.specialClank =
          ft_truthy(obj_req(fo, "specialClank"), "specialClank");
      e->f.specialOnHit =
          ft_truthy(obj_req(fo, "specialOnHit"), "specialOnHit");
      e->f.vCancel = ft_truthy(obj_req(fo, "vCancel"), "vCancel");
    }
  }
  g_hd_loaded = true;
}

const HdFlags *hd_flags(double charId, const char *state) {
  const int c = (int)charId;
  if (!g_hd_loaded) sd_fatal("hd_flags before sim_data_load");
  if (c < 0 || c >= ML_CHARS) sd_fatal("hd_flags: char id out of range");
  for (int k = 0; k < g_hd_count[c]; k++) {
    if (strcmp(g_hd[c][k].state, state) == 0) return &g_hd[c][k].f;
  }
  sd_fatal2("hd_flags: unknown action state", state);
}

// --- the mvChars tables (the task-7 chars dump; loader lifted from
// replay_moves_puff.c load_mvdata's "chars" section) ---------------------------

#define SD_STATES 192
#define SD_NAME 40
#define SD_ARR_CAP 64
#define SD_SND_KEYS 16
#define SD_SND_ROWS 16

typedef struct {
  int nStates;
  char state[SD_STATES][SD_NAME];
  char name[SD_STATES][SD_NAME];
  bool shared[SD_STATES];
  double setVel[6][SD_ARR_CAP];
  int setVelLen[6];
  Vec2D posOffCC[SD_ARR_CAP];
  int posOffCCLen;
  double setPosCD[SD_ARR_CAP];
  int setPosCDLen;
  int nSndKeys;
  char sndKey[SD_SND_KEYS][SD_NAME];
  AsSoundRow sndRows[SD_SND_KEYS][SD_SND_ROWS];
  char sndName[SD_SND_KEYS][SD_SND_ROWS][SD_NAME];
  int sndLen[SD_SND_KEYS];
} SdCharData;

static const char *const SD_SETVEL_KEYS[6] = {
    "DOWNSTANDB", "DOWNSTANDF", "ESCAPEB", "ESCAPEF", "TECHB", "TECHF"};

static SdCharData g_mv[ML_CHARS];
static char g_palette0[4][SD_NAME];
static bool g_mv_loaded = false;

static int sd_state_idx(int c, const char *state) {
  for (int i = 0; i < g_mv[c].nStates; i++) {
    if (strcmp(g_mv[c].state[i], state) == 0) return i;
  }
  return -1;
}

static void load_mvchars(const CanonVal *chars) {
  if (g_mv_loaded) sd_fatal("mvChars: seen twice");
  if (chars->type != CV_OBJ || chars->nkeys != ML_CHARS) {
    sd_fatal("mvChars shape");
  }
  for (int ci = 0; ci < chars->nkeys; ci++) {
    const int c = atoi(chars->keys[ci]);
    if (c < 0 || c >= ML_CHARS) sd_fatal("mvChars char key");
    SdCharData *d = &g_mv[c];
    const CanonVal *cd = chars->vals[ci];
    if (cd->type != CV_OBJ || cd->nkeys != 7) sd_fatal("mvChars char shape");
    const CanonVal *names = obj_req(cd, "name");
    const CanonVal *shared = obj_req(cd, "shared");
    if (names->type != CV_OBJ || shared->type != CV_OBJ ||
        names->nkeys != shared->nkeys || names->nkeys > SD_STATES) {
      sd_fatal("mvChars name/shared shape");
    }
    d->nStates = names->nkeys;
    for (int i = 0; i < names->nkeys; i++) {
      sd_copy(d->state[i], SD_NAME, names->keys[i], "state key");
      sd_copy(d->name[i], SD_NAME, cv_str(names->vals[i]), "move name");
      if (strcmp(shared->keys[i], names->keys[i]) != 0) {
        sd_fatal("mvChars shared key order");
      }
      d->shared[i] = cv_bool(shared->vals[i]);
      if (d->shared[i] && strcmp(d->state[i], d->name[i]) != 0) {
        sd_fatal("mvChars shared state key != move name");
      }
    }
    const CanonVal *sv = obj_req(cd, "setVelocities");
    if (sv->type != CV_OBJ || sv->nkeys != 6) sd_fatal("mvChars setVelocities");
    for (int k = 0; k < 6; k++) {
      const CanonVal *arr = obj_req(sv, SD_SETVEL_KEYS[k]);
      if (arr->type != CV_ARR || arr->count > SD_ARR_CAP) {
        sd_fatal("mvChars setVel shape");
      }
      d->setVelLen[k] = arr->count;
      for (int i = 0; i < arr->count; i++) {
        d->setVel[k][i] = cv_num(arr->items[i]);
      }
    }
    const CanonVal *cc = obj_req(cd, "posOffsetCliffCatch");
    if (cc->type != CV_ARR || cc->count > SD_ARR_CAP) {
      sd_fatal("mvChars posOffsetCC shape");
    }
    d->posOffCCLen = cc->count;
    for (int i = 0; i < cc->count; i++) {
      const CanonVal *pair = cc->items[i];
      if (pair->type != CV_ARR || pair->count != 2) {
        sd_fatal("mvChars posOffsetCC pair");
      }
      d->posOffCC[i].x = cv_num(pair->items[0]);
      d->posOffCC[i].y = cv_num(pair->items[1]);
    }
    (void)obj_req(cd, "posOffsetCliffWait"); // dumped; no C consumer here
    const CanonVal *sp = obj_req(cd, "setPositionsCaptureDamage");
    if (sp->type != CV_ARR || sp->count > SD_ARR_CAP) {
      sd_fatal("mvChars setPositions shape");
    }
    d->setPosCDLen = sp->count;
    for (int i = 0; i < sp->count; i++) d->setPosCD[i] = cv_num(sp->items[i]);
    const CanonVal *snd = obj_req(cd, "actionSounds");
    if (snd->type != CV_OBJ || snd->nkeys > SD_SND_KEYS) {
      sd_fatal("mvChars actionSounds");
    }
    d->nSndKeys = snd->nkeys;
    for (int k = 0; k < snd->nkeys; k++) {
      sd_copy(d->sndKey[k], SD_NAME, snd->keys[k], "actionSounds key");
      const CanonVal *rows = snd->vals[k];
      if (rows->type != CV_ARR || rows->count > SD_SND_ROWS) {
        sd_fatal("mvChars snd rows");
      }
      d->sndLen[k] = rows->count;
      for (int i = 0; i < rows->count; i++) {
        const CanonVal *row = rows->items[i];
        if (row->type != CV_ARR || row->count != 2) {
          sd_fatal("mvChars snd row shape");
        }
        d->sndRows[k][i].frame = cv_num(row->items[0]);
        sd_copy(d->sndName[k][i], SD_NAME, cv_str(row->items[1]),
                "snd name");
        d->sndRows[k][i].name = d->sndName[k][i];
      }
    }
  }
  g_mv_loaded = true;
}

// --- the mvChars-backed data seams (shared moves.h; lifted verbatim from
// replay_moves_puff.c) ----------------------------------------------------------

double mv_setVelocity(double charId, const char *moveKey, double idx) {
  const int c = (int)charId;
  if (c < 0 || c >= ML_CHARS) sd_fatal("setVelocity char");
  for (int k = 0; k < 6; k++) {
    if (strcmp(SD_SETVEL_KEYS[k], moveKey) == 0) {
      const int i = (int)idx;
      if (idx != (double)i || i < 0 || i >= g_mv[c].setVelLen[k]) {
        return js_nan();
      }
      return g_mv[c].setVel[k][i];
    }
  }
  sd_fatal2("setVelocity move key", moveKey);
}

double mv_setPosition_capturedamage(double charId, double idx) {
  const int c = (int)charId;
  if (c < 0 || c >= ML_CHARS) sd_fatal("setPositions char");
  const int i = (int)idx;
  if (idx != (double)i || i < 0 || i >= g_mv[c].setPosCDLen) return js_nan();
  return g_mv[c].setPosCD[i];
}

bool mv_posOffsetCliffCatch(double charId, double idx, Vec2D *out) {
  const int c = (int)charId;
  if (c < 0 || c >= ML_CHARS) sd_fatal("posOffset char");
  const int i = (int)idx;
  if (idx != (double)i || i < 0 || i >= g_mv[c].posOffCCLen) return false;
  *out = g_mv[c].posOffCC[i];
  return true;
}

int mv_actionSounds(double charId, const char *state, const AsSoundRow **rows) {
  const int c = (int)charId;
  if (c < 0 || c >= ML_CHARS) sd_fatal("actionSounds char");
  for (int k = 0; k < g_mv[c].nSndKeys; k++) {
    if (strcmp(g_mv[c].sndKey[k], state) == 0) {
      *rows = g_mv[c].sndRows[k];
      return g_mv[c].sndLen[k];
    }
  }
  return -1; // key absent (the caller traps on `.length` of undefined)
}

const char *mv_palette0(double slot) {
  const int k = (int)slot;
  if (k < 0 || k > 3) sd_fatal("palette slot");
  return g_palette0[k];
}

// --- the mvCharData plane (per-char move-object data arrays; value handling
// lifted from replay_moves_puff.c's puff.data / replay_moves_fox.c's fox.data
// loaders — element kind uniform per array, measured across all five chars) ----

#define SD_ARRS 256

typedef enum { SD_K_NUM, SD_K_PAIR, SD_K_BOOL } SdArrKind;

typedef struct {
  char state[SD_NAME];
  char key[SD_NAME];
  SdArrKind kind;
  int count;
  double nums[SD_ARR_CAP];
  Vec2D pairs[SD_ARR_CAP];
  bool bools[SD_ARR_CAP];
} SdArr;

static SdArr g_arr[ML_CHARS][SD_ARRS];
static int g_arr_count[ML_CHARS];
static bool g_arr_loaded = false;

static void load_mvchardata(const CanonVal *dump) {
  if (g_arr_loaded) sd_fatal("mvCharData: seen twice");
  if (!g_mv_loaded) sd_fatal("mvCharData before mvChars");
  if (dump->type != CV_OBJ || dump->nkeys != ML_CHARS) {
    sd_fatal("mvCharData shape");
  }
  for (int ci = 0; ci < dump->nkeys; ci++) {
    const int c = atoi(dump->keys[ci]);
    if (c < 0 || c >= ML_CHARS) sd_fatal("mvCharData char key");
    const CanonVal *cdata = dump->vals[ci];
    if (cdata->type != CV_OBJ) sd_fatal("mvCharData char shape");
    for (int si = 0; si < cdata->nkeys; si++) {
      const CanonVal *entry = cdata->vals[si];
      if (entry->type != CV_OBJ) sd_fatal("mvCharData entry shape");
      const int sidx = sd_state_idx(c, cdata->keys[si]);
      // move-object data arrays live on the CHAR-ORIGIN move objects (the
      // shared moves' data lives in mvChars): the per-char drivers checked
      // their origin maps; non-shared == char-origin here.
      if (sidx == -1 || g_mv[c].shared[sidx]) {
        sd_fatal2("mvCharData on non-char-origin state", cdata->keys[si]);
      }
      for (int ki = 0; ki < entry->nkeys; ki++) {
        const CanonVal *arr = entry->vals[ki];
        if (arr->type != CV_ARR || arr->count > SD_ARR_CAP) {
          sd_fatal("mvCharData array shape");
        }
        if (g_arr_count[c] >= SD_ARRS) sd_fatal("mvCharData over cap");
        SdArr *a = &g_arr[c][g_arr_count[c]++];
        sd_copy(a->state, SD_NAME, cdata->keys[si], "mvCharData state");
        sd_copy(a->key, SD_NAME, entry->keys[ki], "mvCharData key");
        a->count = arr->count;
        if (arr->count == 0) sd_fatal("mvCharData empty array");
        // element kind is uniform per array (measured: scalar rows, [x,y]
        // pairs, or the canGrabLedge booleans)
        if (arr->items[0]->type == CV_NUM) {
          a->kind = SD_K_NUM;
          for (int i = 0; i < arr->count; i++) {
            a->nums[i] = cv_num(arr->items[i]);
          }
        } else if (arr->items[0]->type == CV_BOOL) {
          a->kind = SD_K_BOOL;
          for (int i = 0; i < arr->count; i++) {
            a->bools[i] = cv_bool(arr->items[i]);
          }
        } else if (arr->items[0]->type == CV_ARR) {
          a->kind = SD_K_PAIR;
          for (int i = 0; i < arr->count; i++) {
            const CanonVal *pair = arr->items[i];
            if (pair->type != CV_ARR || pair->count != 2) {
              sd_fatal("mvCharData pair shape");
            }
            a->pairs[i].x = cv_num(pair->items[0]);
            a->pairs[i].y = cv_num(pair->items[1]);
          }
        } else {
          sd_fatal("mvCharData element kind");
        }
      }
    }
  }
  g_arr_loaded = true;
}

static const SdArr *sd_arr_find(int c, const char *state, const char *key) {
  for (int i = 0; i < g_arr_count[c]; i++) {
    if (strcmp(g_arr[c][i].state, state) == 0 &&
        strcmp(g_arr[c][i].key, key) == 0) {
      return &g_arr[c][i];
    }
  }
  return 0;
}

// this.<key>[idx] scalar read — undefined (missing idx) -> NaN; missing
// (state,key) or non-scalar array traps (the drivers' exact semantics).
static double sd_char_arr(int c, const char *state, const char *key,
                          double idx, const char *what) {
  const SdArr *a = sd_arr_find(c, state, key);
  if (a == 0 || a->kind != SD_K_NUM) sd_fatal2("arr lookup", what);
  const int i = (int)idx;
  if (idx != (double)i || i < 0 || i >= a->count) {
    return js_nan(); // this.<key>[<oob>] === undefined -> NaN in arithmetic
  }
  return a->nums[i];
}

// this.<key>[idx] pair read -> [x,y]; false = out of range (upstream `[0]`
// on undefined THROWS — the caller traps).
static bool sd_char_pair(int c, const char *state, const char *key,
                         double idx, Vec2D *out, const char *what) {
  const SdArr *a = sd_arr_find(c, state, key);
  if (a == 0 || a->kind != SD_K_PAIR) sd_fatal2("pair lookup", what);
  const int i = (int)idx;
  if (idx != (double)i || i < 0 || i >= a->count) return false;
  *out = a->pairs[i];
  return true;
}

// this.<key>.length — missing key traps.
static double sd_char_arr_len(int c, const char *state, const char *key,
                              const char *what) {
  const SdArr *a = sd_arr_find(c, state, key);
  if (a == 0) sd_fatal2("arr_len lookup", what);
  return (double)a->count;
}

double mv_marth_arr(const char *state, const char *key, double idx) {
  return sd_char_arr(0, state, key, idx, "mv_marth_arr");
}
bool mv_marth_pair(const char *state, const char *key, double idx,
                   Vec2D *out) {
  return sd_char_pair(0, state, key, idx, out, "mv_marth_pair");
}
double mv_marth_arr_len(const char *state, const char *key) {
  return sd_char_arr_len(0, state, key, "mv_marth_arr_len");
}

double mv_puff_arr(const char *state, const char *key, double idx) {
  return sd_char_arr(1, state, key, idx, "mv_puff_arr");
}
bool mv_puff_pair(const char *state, const char *key, double idx,
                  Vec2D *out) {
  return sd_char_pair(1, state, key, idx, out, "mv_puff_pair");
}
double mv_puff_arr_len(const char *state, const char *key) {
  return sd_char_arr_len(1, state, key, "mv_puff_arr_len");
}

double mv_fox_arr(const char *state, const char *key, double idx) {
  return sd_char_arr(2, state, key, idx, "mv_fox_arr");
}
bool mv_fox_pair(const char *state, const char *key, double idx, Vec2D *out) {
  return sd_char_pair(2, state, key, idx, out, "mv_fox_pair");
}
double mv_fox_arr_len(const char *state, const char *key) {
  return sd_char_arr_len(2, state, key, "mv_fox_arr_len");
}

double mv_falco_arr(const char *state, const char *key, double idx) {
  return sd_char_arr(3, state, key, idx, "mv_falco_arr");
}
bool mv_falco_pair(const char *state, const char *key, double idx,
                   Vec2D *out) {
  return sd_char_pair(3, state, key, idx, out, "mv_falco_pair");
}
double mv_falco_arr_len(const char *state, const char *key) {
  return sd_char_arr_len(3, state, key, "mv_falco_arr_len");
}

double mv_falcon_arr(const char *state, const char *key, double idx) {
  return sd_char_arr(4, state, key, idx, "mv_falcon_arr");
}
bool mv_falcon_pair(const char *state, const char *key, double idx,
                    Vec2D *out) {
  return sd_char_pair(4, state, key, idx, out, "mv_falcon_pair");
}
double mv_falcon_arr_len(const char *state, const char *key) {
  return sd_char_arr_len(4, state, key, "mv_falcon_arr_len");
}

// --- palettes0 -----------------------------------------------------------------

static bool g_pal_loaded = false;

static void load_palettes0(const CanonVal *pal) {
  if (g_pal_loaded) sd_fatal("palettes0: seen twice");
  if (pal->type != CV_ARR || pal->count != 4) sd_fatal("palettes0 shape");
  for (int k = 0; k < 4; k++) {
    sd_copy(g_palette0[k], SD_NAME, cv_str(pal->items[k]), "palette string");
  }
  g_pal_loaded = true;
}

// --- sim_data_load: the SIMDATA1 artifact reader --------------------------------
// Format (dump-sim-data.js): line 1 "SIMDATA1", then 5 tab-separated lines
// "<label>\t<canon v1.1>" in the fixed order below. Lines are parsed one at
// a time — canon_arena_reset between lines; every loader above deep-copies,
// so no arena pointer survives this function.

static bool g_loaded = false;

static void sd_strip(char *line, ssize_t *n) {
  while (*n > 0 && (line[*n - 1] == '\n' || line[*n - 1] == '\r')) {
    line[--*n] = 0;
  }
}

void sim_data_load(const char *path) {
  if (g_loaded) sd_fatal("sim_data_load: called twice");
  FILE *f = fopen(path, "r");
  if (!f) sd_fatal2("cannot open SIMDATA1 artifact", path);
  char *line = NULL;
  size_t cap = 0;
  ssize_t n = getline(&line, &cap, f);
  if (n <= 0) sd_fatal("empty artifact");
  sd_strip(line, &n);
  if (strcmp(line, "SIMDATA1") != 0) sd_fatal2("bad artifact magic", line);
  static const char *const LABELS[5] = {"asFlags", "hdFlags", "mvChars",
                                        "mvCharData", "palettes0"};
  for (int li = 0; li < 5; li++) {
    n = getline(&line, &cap, f);
    if (n <= 0) sd_fatal2("artifact truncated before", LABELS[li]);
    sd_strip(line, &n);
    char *tab = strchr(line, '\t');
    if (!tab) sd_fatal2("malformed artifact line", LABELS[li]);
    *tab = 0;
    if (strcmp(line, LABELS[li]) != 0) {
      sd_fatal2("unexpected artifact label", line);
    }
    canon_arena_reset();
    const char *err = 0;
    const CanonVal *v = canon_parse(tab + 1, &err);
    if (!v) sd_fatal2("canon parse failure", err);
    switch (li) {
      case 0: load_asflags(v); break;
      case 1: load_hdflags(v); break;
      case 2: load_mvchars(v); break;
      case 3: load_mvchardata(v); break;
      case 4: load_palettes0(v); break;
    }
  }
  while ((n = getline(&line, &cap, f)) > 0) {
    sd_strip(line, &n);
    if (n > 0) sd_fatal("trailing content in artifact");
  }
  free(line);
  fclose(f);
  canon_arena_reset(); // nothing retained: all loaders deep-copied
  g_loaded = true;
}

// --- sim_data_register: registries + module indexes + special phases ------------

// The composed special-phase lookup: falcon / puff / marth each return NULL
// on an unknown (state, phase) — no pre-filtering needed, plain chaining;
// the (state, phase) pairs are disjoint across the three chars. A pair no
// char owns returns NULL and mv_dispatch traps (the upstream
// missing-property TypeError).
static MvFn sim_special_phase(const char *state, const char *phase) {
  MvFn fn = falcon_special_phase(state, phase);
  if (fn == 0) fn = puff_special_phase(state, phase);
  if (fn == 0) fn = marth_special_phase(state, phase);
  return fn;
}

void sim_data_register(void) {
  if (!g_loaded) sd_fatal("sim_data_register before sim_data_load");
  // char id -> per-char module index (upstream's five moves/index.js maps)
  static const MvCharModuleLookup CHAR_MODULE[ML_CHARS] = {
      marth_move_def, puff_move_def, fox_move_def, falco_move_def,
      falcon_move_def};
  static AsMoveEntry entries[ML_CHARS][AS_MAX_STATES];
  for (int c = 0; c < ML_CHARS; c++) {
    const SdCharData *d = &g_mv[c];
    if (d->nStates > AS_MAX_STATES) {
      sd_fatal("actionStates registry over AS_MAX_STATES");
    }
    for (int i = 0; i < d->nStates; i++) {
      // shared-origin states resolve to the task-7 shared C bodies; every
      // other state is the table char's own module def (the loader asserts
      // shared implies state key == move name, so name-keyed lookup is the
      // drivers' state-keyed lookup).
      const MlMoveDef *def = d->shared[i] ? mv_shared_def(d->name[i])
                                          : CHAR_MODULE[c](d->name[i]);
      if (def == 0) sd_fatal2("state has no C body", d->state[i]);
      entries[c][i].name = d->state[i];
      entries[c][i].def = def;
    }
    as_setupActionStates(c, entries[c], d->nStates);
  }
  // checkForIASA's per-char module arms (actionStateShortcuts.js:400-406):
  // MARTHMOVES (c==0), PUFFMOVES (c==1), FOXMOVES (c==2) — all three
  // static imports are live upstream and mv_checkForIASA dispatches each
  // (the fox arm is LIVE for fox aerials; the fox replay driver registers
  // it too). Chars 3/4 have no arm — nothing to register.
  mv_register_char_module(0, marth_move_def);
  mv_register_char_module(1, puff_move_def);
  mv_register_char_module(2, fox_move_def);
  mv_register_special_phases(sim_special_phase);
}

// --- the rule-17 LIVE charHitboxes value plane -----------------------------------
// Upstream player.hitboxes.id[j] ALIASES the GLOBAL charHitboxes objects
// (player.js:132): dmg/size writes through those aliases mutate the global
// plane and every later assign reads the LIVE values (puff rollout/sing,
// marth NEUTRALSPECIAL charge — shared moves.h note). The plane is
// {char -> moveKey -> id[4] {dmg, size}} initialized from CTAB1
// (ml_hitbox_moves; dmg is int-typed there, size double-bits), plus
// per-(player, idIdx) PROVENANCE: which plane entry that player id slot
// aliases. moveKeys are kept as CTAB1 entry INDEXES (the ml_hitbox_moves
// name pointers are stable; matched once by strcmp at assign time).

#define SD_CHD_MOVES 96 // >= max ml_hitbox_move_count (62 marth)

typedef struct {
  bool has[4];
  double dmg[4];
  double size[4];
} SdChdMove;

typedef struct {
  bool valid;
  int chr;  // char table the aliased global object belongs to
  int move; // ml_hitbox_moves[chr] entry index
  int src;  // id index within that entry
} SdChdProv;

static SdChdMove g_chd[ML_CHARS][SD_CHD_MOVES];
static SdChdProv g_prov[4][4];
static bool g_chd_ready = false;

void sim_chd_reset(void) {
  for (int c = 0; c < ML_CHARS; c++) {
    if (ml_hitbox_move_count[c] > SD_CHD_MOVES) {
      sd_fatal("charHitboxes plane over cap");
    }
    for (int32_t k = 0; k < ml_hitbox_move_count[c]; k++) {
      SdChdMove *m = &g_chd[c][k];
      memset(m, 0, sizeof *m);
      for (int j = 0; j < 4; j++) {
        const ml_hitbox_t *id = ml_hitbox_moves[c][k].id[j];
        if (id == 0) continue; // absent upstream
        m->has[j] = true;
        m->dmg[j] = (double)id->dmg;
        m->size[j] = ml_f64(id->size);
      }
    }
  }
  memset(g_prov, 0, sizeof g_prov);
  g_chd_ready = true;
}

static int sd_chd_move_idx(int c, const char *moveKey) {
  for (int32_t k = 0; k < ml_hitbox_move_count[c]; k++) {
    if (strcmp(ml_hitbox_moves[c][k].name, moveKey) == 0) return (int)k;
  }
  return -1;
}

// STRONG override of the weak no-op in shared moves_index.c: called by
// mv_assign_hitbox_id AFTER it wrote the pristine-CTAB1 spec into
// player[p].hitboxes.id[dst] (and mirrored the whole struct through the
// id alias). Records provenance, then overlays the LIVE plane's dmg/size
// onto the just-assigned hitbox — mirroring through prevFrameHitboxes
// exactly like the assign body does for the whole struct.
void mv_chd_assign_note(MlSim *S, double p, const char *moveKey, int srcIdx,
                        int dstIdx) {
  if (!g_chd_ready) sd_fatal("chd assign before sim_chd_reset");
  const int ip = (int)p;
  if (ip < 0 || ip > 3 || srcIdx < 0 || srcIdx > 3 || dstIdx < 0 ||
      dstIdx > 3) {
    sd_fatal("chd assign: index out of range");
  }
  const int c = (int)S->characterSelections[ip];
  if (c < 0 || c >= ML_CHARS) sd_fatal("chd assign: char id");
  const int k = sd_chd_move_idx(c, moveKey);
  if (k == -1 || !g_chd[c][k].has[srcIdx]) {
    sd_fatal2("chd assign: plane entry missing", moveKey);
  }
  SdChdProv *pr = &g_prov[ip][dstIdx];
  pr->valid = true;
  pr->chr = c;
  pr->move = k;
  pr->src = srcIdx;
  MlPlayer *pl = &S->player[ip];
  pl->hitboxes.id[dstIdx].dmg = g_chd[c][k].dmg[srcIdx];
  pl->hitboxes.id[dstIdx].size = g_chd[c][k].size[srcIdx];
  if (S->aliasHbId[ip]) {
    pl->phys.prevFrameHitboxes.id[dstIdx].dmg = g_chd[c][k].dmg[srcIdx];
    pl->phys.prevFrameHitboxes.id[dstIdx].size = g_chd[c][k].size[srcIdx];
  }
}

// STRONG overrides: hitboxes.id[idx].dmg/size writes went through the
// global-object alias upstream — carry them into the live plane. A write
// without provenance on a CONSTRUCTOR-shaped hitbox targets a player-local
// ActiveHitbox (no global object involved): no-op. On a CHARDATA-shaped
// hitbox it means the host lost track of which global object the slot
// aliases — loud, never silent.
void mv_chd_write_dmg(MlSim *S, double p, int idx, double v) {
  const int ip = (int)p;
  if (ip < 0 || ip > 3 || idx < 0 || idx > 3) sd_fatal("chd write: index");
  const SdChdProv *pr = &g_prov[ip][idx];
  if (pr->valid) {
    g_chd[pr->chr][pr->move].dmg[pr->src] = v;
    return;
  }
  if (S->player[ip].hitboxes.id[idx].shape == ML_HB_CONSTRUCTOR) return;
  sd_fatal("chd write without provenance");
}

void mv_chd_write_size(MlSim *S, double p, int idx, double v) {
  const int ip = (int)p;
  if (ip < 0 || ip > 3 || idx < 0 || idx > 3) sd_fatal("chd write: index");
  const SdChdProv *pr = &g_prov[ip][idx];
  if (pr->valid) {
    g_chd[pr->chr][pr->move].size[pr->src] = v;
    return;
  }
  if (S->player[ip].hitboxes.id[idx].shape == ML_HB_CONSTRUCTOR) return;
  sd_fatal("chd write without provenance");
}

// puff's chd-fed assign (puff/moves.h): in the integrated sim the shared
// assign + the strong note above ARE the live-plane path — provenance and
// the live dmg/size overlay happen in mv_chd_assign_note.
void pf_assign_hitbox_id(MlSim *S, double p, const char *moveKey, int srcIdx,
                         int dstIdx) {
  mv_assign_hitbox_id(S, p, moveKey, srcIdx, dstIdx);
}

// --- standalone sanity-parse main (compile with -DSIM_DATA_TEST_MAIN) ----------
// Links the moves/action_state TUs (the check-moves-puff-replay.sh list
// plus ALL FIVE char move dirs) + canon.c; the host externs the link
// demands are stubbed below (never reached by load/register alone).
#ifdef SIM_DATA_TEST_MAIN

void sim_fatal(const char *what) {
  fprintf(stderr, "SIM FATAL: %s\n", what);
  exit(3);
}

void mv_out_of_domain(const char *what) { sim_fatal(what); }
void ml_asshort_out_of_domain(const char *what) { sim_fatal(what); }
void ml_events_fail(const char *what) { sim_fatal(what); }

AsTri mv_seam(MlSim *S, double charId, const char *state, const char *phase,
              double slot, const MvX *ex) {
  (void)S; (void)charId; (void)state; (void)phase; (void)slot; (void)ex;
  sim_fatal("mv_seam reached in test main");
}
void mv_hq_push6(MlSim *S, double a, double b, double c, bool d, bool e,
                 bool f) {
  (void)S; (void)a; (void)b; (void)c; (void)d; (void)e; (void)f;
  sim_fatal("mv_hq_push6 reached in test main");
}
double mv_howl_play_id(const char *name) {
  (void)name;
  sim_fatal("mv_howl_play_id reached in test main");
}
void mv_article_laser(MlSim *S, double p, double x, double y, double rotate) {
  (void)S; (void)p; (void)x; (void)y; (void)rotate;
  sim_fatal("mv_article_laser reached in test main");
}
void mv_article_illusion(MlSim *S, double p, double type) {
  (void)S; (void)p; (void)type;
  sim_fatal("mv_article_illusion reached in test main");
}
void mv_article_laser_falco(MlSim *S, double p, double x, double y,
                            double rotate, bool partOfThrow) {
  (void)S; (void)p; (void)x; (void)y; (void)rotate; (void)partOfThrow;
  sim_fatal("mv_article_laser_falco reached in test main");
}
void mv_article_illusion_falco(MlSim *S, double p, double type) {
  (void)S; (void)p; (void)type;
  sim_fatal("mv_article_illusion_falco reached in test main");
}

int main(int argc, char **argv) {
  if (argc != 2) {
    fprintf(stderr, "usage: sim_data_test <simdata.txt>\n");
    return 1;
  }
  sim_data_load(argv[1]);
  sim_data_register();
  sim_chd_reset();
  long planeTotal = 0;
  for (int c = 0; c < ML_CHARS; c++) {
    int shared = 0;
    for (int i = 0; i < g_mv[c].nStates; i++) {
      if (g_mv[c].shared[i]) shared++;
    }
    long plane = 0;
    for (int32_t k = 0; k < ml_hitbox_move_count[c]; k++) {
      for (int j = 0; j < 4; j++) {
        if (g_chd[c][k].has[j]) plane++;
      }
    }
    planeTotal += plane;
    printf("char %d (%s): %d states registered (%d shared, %d char-origin), "
           "%d asFlags, %d hdFlags, %d data arrays, %d snd keys, "
           "%ld plane id entries\n",
           c, ml_char_names[c], g_mv[c].nStates, shared,
           g_mv[c].nStates - shared, g_as_count[c], g_hd_count[c],
           g_arr_count[c], g_mv[c].nSndKeys, plane);
  }
  printf("palettes0: [%s] [%s] [%s] [%s]\n", g_palette0[0], g_palette0[1],
         g_palette0[2], g_palette0[3]);
  printf("SIM DATA OK (%ld live plane id entries total)\n", planeTotal);
  return 0;
}

#endif // SIM_DATA_TEST_MAIN
