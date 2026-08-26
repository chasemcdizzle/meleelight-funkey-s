// port/foh/foh_tbuild.c — A45 T4, the target builder's engine.
// Contract, scope, and WHY this is a separate TU behind a pointer seam:
// foh_tbuild.h. Read that first.
//
// Upstream is `targetBuilderControls` (src/target/targetbuilder.js:159-855).
// Cited per arm below. The three ported tools are 5 TARGET (:560-571),
// 6 MOVE (:572-618) and 7 DELETE (:619-738); tools 0-4 and 8-9 are the
// spike's T5-T8 and are simply not here.
#include "foh_tbuild.h"

#include <stdio.h>
#include <string.h>

#include "../gfx/raster.h"    // gfx_fatal
#include "../sim/ml_js.h"     // js_round (ECMAScript Math.round)
#include "../sim/ml_ser.h"    // ml_sha256_hex
#include "../sim/stage_code.h" // MlkStage, mlk_encode, mlk_parse (A45 T1)
// The geometry the tools hover and validate with is ALREADY IN THE PORT and
// already replay-verified against upstream (M2 task 1, expected-capture-
// util.json): intersectsAny / distanceToLine / distanceToPolygon /
// lineDistanceToLines, plus manhattanDist. MEASURED: these two headers
// compile standalone and do NOT drag the generated ml_stages.h, which is
// what kept custom_stage.h out of this TU (foh_tbuild.h's note). So the
// builder uses upstream's own proven bodies rather than a second copy.
#include "../../port/fdlibm/fdlibm.h" // fd_atan2 — Math.atan2, the locked
                                     // surface (PLAN §2); never libm's
#include "../sim/util/detect_intersections.h"
#include "../sim/util/lin_alg.h"
#include "foh_persist.h"      // foh_persist_dir, foh_persist_publish

// The SIM's target cap, restated. `port/sim/target/target_play.h:43` owns
// it (`_Static_assert`-tied to upstream's 10-element `targetDestroyed`
// literal, targetplay.js:37) but that header transitively needs the
// GENERATED ml_stages.h, so it cannot be included from port/foh (measured).
// check-tbuild.sh leg [1] compiles the two together and asserts they are
// EQUAL, so this copy cannot drift silently.
//
// R2 IS AN OPEN OWNER RULING AND THIS IS THE SAFE HALF OF IT. The codec
// holds 20 targets (MLK_MAX_TARGETS, upstream's own targetbuilder.js:563
// cap) and the sim holds 10. JS arrays grow, so a 20-target stage genuinely
// plays upstream. The cap is NOT raised here; the BUILDER refuses the 11th
// at the moment the player presses A, which is where the message is useful
// — the design spike's own words, and A45 T2's note ("the builder (T4)
// should refuse the 11th where the message is useful").
#define FOH_TB_PLAYABLE_TARGETS 10

// --- the document -----------------------------------------------------------
//
// MODULE STATE, exactly like upstream's `export var stageTemp`
// (targetbuilder.js:53-72). It is deliberately NOT a FohState field:
// sizeof(MlkStage) is 45,344 bytes (measured this session) against a 7,224
// byte FohState, and every witness in port/foh declares a FohState by value.
// Its LIFETIME is upstream's too — menu.js:87-90 enters the builder without
// resetting stageTemp, so a second visit keeps the document you left.
static MlkStage g_doc;
static bool g_docReady;

// --- polygonMap: BUILDER state, and that is not an omission -----------------
//
// `polygonMap` is which collision surfaces each polygon owns
// (targetbuilder.js:59). It is NOT one of the share code's 14 fields, and
// parseStageCode sets every row to null (encode.js:244 — upstream BUG 2,
// modelled in stage_code.h as polygonMapIsNull[]). So the map cannot survive
// a save, upstream OR here.
//
// It therefore lives here, beside g_doc, rather than inside MlkStage:
// putting the contents in the codec's value model would add a field the
// codec can never write and would change the pinned sizeof(MlkStage) for
// nothing. Keeping it here reproduces upstream's observable behaviour
// exactly — a polygon you drew this session moves with its surfaces, and one
// you loaded from a slot does not. g_doc's own polygonMapCount /
// polygonMapIsNull are kept in step so any consumer of the value model sees
// the same truth.
typedef struct {
  int kind; // FOH_TB_H_GROUND | _CEILING | _WALLL | _WALLR
  int index;
} TbMapEntry;
typedef struct {
  TbMapEntry items[MLK_MAX_POLY_POINTS];
  int count;
} TbMapRow;
static TbMapRow g_map[MLK_MAX_POLYGONS];

// Mirror the map's shape into the value model, so polygonMapCount /
// polygonMapIsNull never disagree with g_map.
static void map_sync(void) {
  g_doc.polygonMapCount = g_doc.polygon.count;
  for (int i = 0; i < MLK_MAX_POLYGONS; i++) {
    g_doc.polygonMapIsNull[i] =
        (i >= g_doc.polygonMapCount) ? false : (g_map[i].count < 0);
  }
}

// A row is NULL when its count is negative — the one encoding that keeps
// "null" distinct from "an empty list", which parsePolygon can legitimately
// produce (stage_code.h: count 0 is legal).
static bool map_is_null(int i) {
  return i < 0 || i >= g_doc.polygonMapCount || g_map[i].count < 0;
}

// Every row null: what a stage loaded from a code has (BUG 2).
static void map_all_null(void) {
  for (int i = 0; i < MLK_MAX_POLYGONS; i++) g_map[i].count = -1;
  map_sync();
}

// The previous frame's crosshair, upstream's prevCrossHairPos /
// prevRealCrossHair (:777-778). MOVE's centerItem reads the DELTA for
// surfaces; for targets and starting points it snaps (:1546-1552), so only
// the snap arms are reachable in T4 — the deltas are kept because the
// clamp arms below rewrite the ungridded position and the pair must stay
// consistent frame to frame.
static double g_prevX, g_prevY;

// gridSizes (targetbuilder.js:80). Index 4 is FREE movement.
static const int kGridSizes[5] = {80, 40, 20, 10, 0};
#define FOH_TB_GRIDS 5

// Math.PI, written as the sim writes it (hit_detection.c:1164 uses the same
// literal): the shortest decimal that round-trips to the IEEE double V8
// gives for Math.PI. A macro rather than M_PI, which is libm's and is not
// guaranteed to be that value.
#define ML_TB_PI 3.141592653589793

// --- the tool cycle ---------------------------------------------------------
//
// toolInfo (:36), complete. The order IS upstream's; the cycle is this array
// and not `0..9` so that a tool which is not built is ABSENT from the cycle
// rather than reachable and inert. With all ten built the two coincide, and
// that coincidence is asserted below rather than assumed.
static const int kToolOrder[] = {
    FOH_TB_TOOL_POLYGON, FOH_TB_TOOL_PLATFORM, FOH_TB_TOOL_WALL,
    FOH_TB_TOOL_LEDGE,   FOH_TB_TOOL_DAMAGE,   FOH_TB_TOOL_TARGET,
    FOH_TB_TOOL_MOVE,    FOH_TB_TOOL_DELETE,   FOH_TB_TOOL_SCALE,
    FOH_TB_TOOL_DRAWMODE};
#define FOH_TB_TOOLS ((int)(sizeof kToolOrder / sizeof kToolOrder[0]))

static const char *const kToolNames[FOH_TB_TOOL_IDS] = {
    "POLYGON", "PLATFORM", "WALL", "LEDGE",  "DAMAGE",
    "TARGET",  "MOVE",     "DELETE", "SCALE", "DRAW MODE"};
// wallTypeList (:29) / damageTypeList (:33), upstream's own order.
static const char *const kWallTypeNames[FOH_TB_WALLTYPES] = {
    "GROUND", "CEILING", "WALL L", "WALL R"};
static const char *const kDamageTypeNames[FOH_TB_DAMAGETYPES] = {
    "FIRE", "ELECTRIC", "SLASH", "DARKNESS"};
// The string that reaches the share code and the sim: damageTypeList's own
// values, lowercase, because `{damageType: "fire"}` is what upstream writes
// and what encode.js:36-40 maps to its digit.
static const char *const kDamageTypeValues[FOH_TB_DAMAGETYPES] = {
    "fire", "electric", "slash", "darkness"};

// Static, and reached only through view(): see foh_tbuild.h's note on why
// these are not exported symbols.
static const char *tool_name(int toolId) {
  for (int i = 0; i < FOH_TB_TOOLS; i++) {
    if (kToolOrder[i] == toolId) return kToolNames[toolId];
  }
  return NULL; // not in the cycle: an unbuilt tool has no name to show
}

// Position of a tool id in the cycle, or -1. Used by the L/R arms; a tool
// that fell out of the cycle would otherwise strand the cursor.
static int tool_pos(int toolId) {
  for (int i = 0; i < FOH_TB_TOOLS; i++) {
    if (kToolOrder[i] == toolId) return i;
  }
  return -1;
}

// --- world <-> upstream-canvas ----------------------------------------------
//
// :179 and its inverse (:433-434, :480-481). The canvas is 1200x750 with the
// origin at (600, 375) and y INVERTED; `scale` is the document's own.
static double to_cx(double wx) { return wx * g_doc.scale + 600.0; }
static double to_cy(double wy) { return wy * -g_doc.scale + 375.0; }
static double to_wx(double cx) { return (cx - 600.0) / g_doc.scale; }
static double to_wy(double cy) { return (cy - 375.0) / -g_doc.scale; }

// --- the five collision lists, addressed by hover kind ----------------------
//
// Upstream indexes stageTemp by the hover kind's own STRING
// (`stageTemp[hoverItem[0]]`), which is why the kind and the list are the
// same thing there. Here it is one lookup, in one place.
static SurfaceList *list_of(int kind) {
  switch (kind) {
    case FOH_TB_H_GROUND: return &g_doc.s.ground;
    case FOH_TB_H_CEILING: return &g_doc.s.ceiling;
    case FOH_TB_H_WALLL: return &g_doc.s.wallL;
    case FOH_TB_H_WALLR: return &g_doc.s.wallR;
    case FOH_TB_H_PLATFORM: return &g_doc.s.platform;
    case FOH_TB_H_LINE: return &g_doc.bgLine;
    default: return NULL;
  }
}

// wallTypeList index -> hover kind (they are the same four strings, in a
// different order: wallTypeList is ground/ceiling/wallL/wallR).
static int wall_kind(int wallTypeIndex) {
  static const int k[FOH_TB_WALLTYPES] = {FOH_TB_H_GROUND, FOH_TB_H_CEILING,
                                          FOH_TB_H_WALLL, FOH_TB_H_WALLR};
  return k[wallTypeIndex];
}

// The `ledge` triple's list letter: MlLedge.list is 'g' | 'p'.
static char ledge_letter(int kind) {
  return kind == FOH_TB_H_PLATFORM ? 'p' : 'g';
}

// --- DEVIATION D51: a new stage starts from a floor -------------------------
//
// Upstream's fresh `stageTemp` (:53-72) has NO ground, NO ceiling and NO
// walls — you build them with the WALL and POLYGON tools, which are T5 and
// T7. A targets-only editor that started there could only ever produce a
// stage with nothing to stand on: the player would spawn, fall through the
// blastzone and the "stage" would be unplayable. That is not a faithful
// port of a feature, it is a feature that cannot be used.
//
// So a NEW stage starts from a minimal playable template: one ground
// surface, upstream's own four starting points and its own blastzone and
// scale. Everything except the ground line is upstream's literal
// (:62 startingPoint, :64 blastzone, :65 scale). The ground is 200 world
// units wide at y = 0, centred — wide enough to stand and move on at the
// default scale 3, well inside the blastzone.
//
// It is a DEVIATION and it is registered as one (MENU-SPEC D51). When T5/T7
// land the WALL and POLYGON tools the honest thing is to keep it as the
// default and let the player delete it, not to go back to an empty stage.
static void doc_template(MlkStage *st) {
  memset(st, 0, sizeof *st);
  // :62 — `[new Vec2D(-10,0), new Vec2D(10,0), new Vec2D(-30,0), new Vec2D(30,0)]`
  const double spx[4] = {-10.0, 10.0, -30.0, 30.0};
  for (int i = 0; i < 4; i++) {
    st->startingPoint[i].x = spx[i];
    st->startingPoint[i].y = 0.0;
  }
  st->startingPointCount = 4;
  // `hasStartingFace` false: the builder's stageTemp genuinely has no
  // startingFace key, and stage_code.h records that an ABSENT key emits
  // "1,1,1,1" while an EMPTY array emits nothing (encode.js:20-30). False
  // is the builder's own shape.
  st->hasStartingFace = false;
  st->startingFaceCount = 0;
  // D51's one non-upstream line: the floor.
  st->s.ground.items[0].p0.x = -100.0;
  st->s.ground.items[0].p0.y = 0.0;
  st->s.ground.items[0].p1.x = 100.0;
  st->s.ground.items[0].p1.y = 0.0;
  st->s.ground.count = 1;
  // :64 — `new Box2D([-250,-250],[250,250])`
  st->blastzone.min.x = -250.0;
  st->blastzone.min.y = -250.0;
  st->blastzone.max.x = 250.0;
  st->blastzone.max.y = 250.0;
  st->scale = 3.0; // :65
}

// Every load and every fresh document starts with NO polygon->surface map:
// a template has no polygons, and a stage read back from a slot has an
// all-null map because the code cannot carry one (encode.js:244, BUG 2).
// Calling this at every document boundary is what makes that bug faithful
// rather than accidental.

// --- the .mlstage file plane ------------------------------------------------
//
// A45 T2's contract (port/sim/target/custom_stage.h), byte for byte:
//   MLSTAGE1\n
//   <share code>\n
//   SUM <64 lowercase hex>\n
// SUM is sha256 over every preceding byte.
//
// THIS IS A SECOND IMPLEMENTATION OF T2's READER AND THAT IS DELIBERATE,
// NOT AN OVERSIGHT. `mlk_slot_load` lives in custom_stage.c, whose header
// transitively includes `sim/sim.h` and therefore the GENERATED ml_stages.h
// — measured this session: port/foh cannot even INCLUDE it, let alone link
// it, without dragging the whole sim into sixteen check scripts. The
// project's answer to "two implementations of one grammar" is the fmt_diff
// discipline: check-tbuild.sh leg [4] runs BOTH over the same corpus of
// files (good, truncated, bad-SUM, over-cap, bad-grammar) and requires them
// to agree on every verdict. A shared body could not be proven to agree;
// this is.
#define TB_FILE_MAX (MLK_CODE_MAX + 128) // == MLK_FILE_MAX (custom_stage.h)
#define TB_NAME_MAX 32

static void slot_name(int slot, char out[TB_NAME_MAX]) {
  if (slot < 0 || slot >= FOH_TB_SLOTS) gfx_fatal("foh_tbuild: slot range");
  snprintf(out, TB_NAME_MAX, "custom%d.mlstage", slot);
}

static bool slot_path(int slot, char *buf, size_t cap) {
  char name[TB_NAME_MAX];
  slot_name(slot, name);
  const int w = snprintf(buf, cap, "%s/%s", foh_persist_dir(), name);
  return w > 0 && (size_t)w < cap;
}

static bool hexdigit(char c) {
  return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f');
}

// --- playability ------------------------------------------------------------

// A surface carries damage exactly when the SIM says it does. Physics reads
// `wall[2] !== undefined ? wall[2].damageType : null` and tests it for
// TRUTHINESS, so props whose damageType is NULL are inert — and those are
// precisely what upstream BUG 1 emits for the sixth surface of every type
// (encode.js:39's `i !== 5`). Refusing those would reject codes the browser
// plays fine, which is why only a real type string counts. This mirrors
// custom_stage.c's list_has_damage; check-tbuild.sh leg [4] proves the two
// agree on a corpus rather than trusting that they do.
static bool list_has_damage(const SurfaceList *l) {
  for (int k = 0; k < l->count; k++) {
    const Surface *sf = &l->items[k];
    if (sf->hasProps && sf->propsDamageType.tag == DT_STR) return true;
  }
  return false;
}

// THE THREE RULES THE SIM ENFORCES, MIRRORED HERE — not a subset.
//
// This is not belt-and-braces, it closes a CRASH PATH found by the
// differential while building this ticket: the FOH decides what
// target-select draws as a playable custom slot, so a stage this function
// waves through but `mlk_stage_playable` refuses would be shown as present,
// launched by A, and then die inside the sim's loud refusal — a crash where
// the player asked for a game. Whatever the sim will refuse, the FOH must
// refuse first, in words, where the player can act on it.
//
// The rule set is custom_stage.c:34-66 verbatim in substance. The MESSAGES
// are shorter because these ones have to fit on a 240 px line.
static const char *doc_unplayable(const MlkStage *st) {
  // (1) R2 — the codec's 20 against the sim's 10. Refuse, never truncate.
  if (st->targetCount < 1) return "place at least 1 target";
  if (st->targetCount > FOH_TB_PLAYABLE_TARGETS) return "10 targets max";
  // (2) the CONCATENATION cap: five lists that each pass their own 64 cap
  //     can still overflow runCollisionRoutine's single 96-entry list.
  {
    const long total = (long)st->s.wallL.count + st->s.wallR.count +
                       st->s.ground.count + st->s.ceiling.count +
                       st->s.platform.count;
    if (total > ML_MAX_LABELLED_SURFACES) return "too many surfaces";
  }
  // (3) the DAMAGE plane, which has never executed and which no golden
  //     covers. A45 T6 owes that golden; until then the refusal is the
  //     marker, and it must appear at LOAD, not mid-match.
  if (list_has_damage(&st->s.ground) || list_has_damage(&st->s.ceiling) ||
      list_has_damage(&st->s.wallL) || list_has_damage(&st->s.wallR) ||
      list_has_damage(&st->s.platform)) {
    return "damaging surface (unsupported)";
  }
  if (st->startingPointCount < 1) return "no starting point";
  return 0;
}

// Read + verify + parse. True and *out filled on success. On false NOTHING
// usable is in *out and *why names the RULE that refused — never a generic
// "bad file", because the player has to be able to act on it.
//
// VALIDATE ON READ, ALWAYS. The file is user-supplied (he may have copied
// it onto the SD card himself) and this device has a MEASURED power-loss
// risk on a journal-less vfat, so a TRUNCATED file is an expected input,
// not a hypothetical one. The read is BOUNDED (a file over the cap is
// refused unread rather than truncated into a parse), the grammar is exact,
// and SUM is verified BEFORE mlk_parse ever sees a byte.
static bool slot_read(int slot, MlkStage *out, const char **why) {
#define RD_FAIL(m)                                                             \
  do {                                                                         \
    if (why) *why = (m);                                                       \
    if (f) fclose(f);                                                          \
    return false;                                                              \
  } while (0)
  FILE *f = 0;
  char path[512];
  if (!slot_path(slot, path, sizeof path)) RD_FAIL("path too long");
  f = fopen(path, "rb");
  if (!f) RD_FAIL("empty");
  // BOUNDED READ. One byte more than the cap is asked for on purpose: a
  // short read then proves the file fits, without a seek/stat race.
  static char buf[TB_FILE_MAX + 1];
  const size_t n = fread(buf, 1, sizeof buf, f);
  if (ferror(f)) RD_FAIL("unreadable");
  if (n > TB_FILE_MAX) RD_FAIL("file too large");
  fclose(f);
  f = 0;
  // GRAMMAR. Exactly three LF-terminated lines, nothing after the third.
  const char kHdr[] = "MLSTAGE1\n";
  const size_t hdr = sizeof kHdr - 1;
  if (n < hdr || memcmp(buf, kHdr, hdr) != 0) RD_FAIL("not a .mlstage file");
  // The SUM line is the LAST 69 bytes: "SUM " + 64 hex + "\n".
  const size_t sumLen = 4 + 64 + 1;
  if (n < hdr + 1 + sumLen) RD_FAIL("truncated");
  const size_t sumAt = n - sumLen;
  if (memcmp(buf + sumAt, "SUM ", 4) != 0) RD_FAIL("no SUM line");
  for (int i = 0; i < 64; i++) {
    if (!hexdigit(buf[sumAt + 4 + (size_t)i])) RD_FAIL("bad SUM digits");
  }
  if (buf[n - 1] != '\n') RD_FAIL("bad SUM line");
  // The code line is everything between the header and the SUM line, and it
  // must be ONE line: exactly one '\n', at its end.
  const size_t codeAt = hdr, codeLen = sumAt - hdr;
  if (codeLen < 2 || buf[sumAt - 1] != '\n') RD_FAIL("bad code line");
  if (memchr(buf + codeAt, '\n', codeLen - 1) != 0) RD_FAIL("bad code line");
  // SUM BEFORE PARSE. A corrupt file must never reach the parser.
  {
    char hex[65];
    ml_sha256_hex(buf, sumAt, hex);
    if (memcmp(hex, buf + sumAt + 4, 64) != 0) RD_FAIL("SUM mismatch");
  }
  // NUL-terminate the code in place (over its own '\n') and parse.
  buf[sumAt - 1] = '\0';
  if (!mlk_parse(buf + codeAt, out, why)) {
    if (why && !*why) *why = "invalid code";
    return false;
  }
  // AND THE PLAYABILITY RULES, HERE, at the read. A slot that the sim will
  // refuse must never read back as present: target-select draws its custom
  // page from this verdict, and a slot shown as playable that dies at launch
  // is a crash where the player asked for a game.
  {
    const char *bad = doc_unplayable(out);
    if (bad) {
      if (why) *why = bad;
      return false;
    }
  }
  return true;
#undef RD_FAIL
}

// Serialise + publish. The ONLY writer. Goes through foh_persist_publish,
// which is foh_persist_save's own atomic publish generalised for exactly
// this caller (A45 T2's instruction, quoted in foh_tbuild.h).
static bool slot_write(int slot, const MlkStage *st, const char **why) {
  static char code[MLK_CODE_MAX];
  const int cn = mlk_encode(st, code, sizeof code);
  if (cn < 0) {
    if (why) *why = "stage too large to encode";
    return false;
  }
  static char file[TB_FILE_MAX];
  const char kHdr[] = "MLSTAGE1\n";
  const size_t hdr = sizeof kHdr - 1;
  // header + code + '\n' + "SUM " + 64 + '\n'
  const size_t body = hdr + (size_t)cn + 1;
  if (body + 69 > sizeof file) {
    if (why) *why = "stage too large to encode";
    return false;
  }
  memcpy(file, kHdr, hdr);
  memcpy(file + hdr, code, (size_t)cn);
  file[hdr + (size_t)cn] = '\n';
  char hex[65];
  ml_sha256_hex(file, body, hex);
  memcpy(file + body, "SUM ", 4);
  memcpy(file + body + 4, hex, 64);
  file[body + 68] = '\n';
  char name[TB_NAME_MAX];
  slot_name(slot, name);
  const char *pubWhy = 0;
  if (!foh_persist_publish(name, file, body + 69, &pubWhy)) {
    if (why) *why = pubWhy ? pubWhy : "save failed";
    return false;
  }
  // A publish that landed but could not prove its directory entry durable
  // is a SUCCESS with a caveat, never a failure (foh_persist.h). It is not
  // surfaced to the player: the bytes are on the card.
  if (why) *why = 0;
  return true;
}

static void slots_scan(bool present[FOH_TB_SLOTS],
                       const char *reason[FOH_TB_SLOTS]) {
  // BY INDEX, no append, no length cursor — D43. A refused slot is reported
  // AS refused and stays in its own place; nothing shifts up to fill it,
  // which is the whole shape of the owner's ruling.
  static MlkStage scratch; // 45 KB — never on the stack
  for (int i = 0; i < FOH_TB_SLOTS; i++) {
    const char *why = 0;
    present[i] = slot_read(i, &scratch, &why);
    reason[i] = present[i] ? 0 : (why ? why : "empty");
  }
}

// --- crosshair --------------------------------------------------------------

// calculateGriddedCrossHair (targetbuilder.js:134-152), verbatim.
// `600 % gridSize` / `375 % gridSize` are INTEGER remainders upstream
// (both operands are integer literals), so they are integer here too.
static void grid_snap(FohState *s) {
  const int g = kGridSizes[s->tbGrid];
  if (s->tbGrid == 4) { // :135-137 free movement
    s->tbX = s->tbUnX;
    s->tbY = s->tbUnY;
    return;
  }
  const double sc = g_doc.scale;
  if (s->tbUnX == 0.0) {
    s->tbX = (double)(600 % g) / sc;
  } else {
    s->tbX = js_round(s->tbUnX / ((double)g / sc)) * (double)g / sc +
             (double)(600 % g) / sc;
  }
  if (s->tbUnY == 0.0) {
    s->tbY = (double)(375 % g) / sc;
  } else {
    s->tbY = js_round(s->tbUnY / ((double)g / -sc)) * (double)g / -sc +
             (double)(375 % g) / sc;
  }
}

// :171-201. The crosshair is the free hand's motion model in world units:
// integrate the stick, snap to the grid, project to upstream's 1200x750
// canvas, and CLAMP BY REWRITING the ungridded position so the two stay
// consistent (the design spike's own correction — "that is a second
// position, not a second clamp").
//
// `foh_hand.h` supplies the shape this follows (D29, the shared free
// cursor): doubles, integrated per frame from a d-pad whose axes are
// -1/0/+1 (DEVIATION D1), clamped to the logical canvas. It is not CALLED
// here because its clamp is to a rect in RASTER pixels while this one must
// rewrite `unGriddedCrossHairPos` in WORLD units — the same reason foh_hand.h
// itself says hit-testing stays with the caller. Same model, same doubles,
// upstream's own body.
static void crosshair_step(FohState *s, const PlatformInput *in) {
  // :171 — X or Y held is the PRECISION modifier (multi 1 vs 5).
  // DEVIATION D50: upstream reads `y || x`; the port reads X ONLY, freeing
  // Y for the grid cycle, because the FunKey-S has no `z` button (the key
  // upstream's :207 binds the grid to — CLAUDE.md's device keysym list is
  // u/d/l/r, a/b/x/y, s, k/n, q). Upstream's two modifier buttons are
  // redundant with each other, so one of them is free by measurement.
  double multi = in->x ? 1.0 : 5.0;
  // :172-174 — `if (targetTool === 8) { multi = 0; }`. The SCALE tool takes
  // the d-pad for its own zoom, and upstream frees it by FREEZING the
  // crosshair rather than by rebinding anything. That is DEVIATION D55's
  // whole content: the conflict this device would otherwise have with SCALE
  // is already solved upstream, so SCALE is carried verbatim.
  if (s->tbTool == FOH_TB_TOOL_SCALE) multi = 0.0;
  const double lsX = (in->right ? 1.0 : 0.0) - (in->left ? 1.0 : 0.0);
  const double lsY = (in->up ? 1.0 : 0.0) - (in->down ? 1.0 : 0.0);
  const double sc = g_doc.scale;
  s->tbUnX += lsX * multi * 3.0 / sc; // :175
  s->tbUnY += lsY * multi * 3.0 / sc; // :176
  grid_snap(s);
  // :178 realCrossHair, then :182-201's four clamp arms verbatim.
  double rx = s->tbX * sc + 600.0, ry = s->tbY * -sc + 375.0;
  if (rx < 0.0) {
    s->tbUnX = -600.0 / sc;
    grid_snap(s);
  }
  if (rx > 1200.0) {
    s->tbUnX = 600.0 / sc;
    grid_snap(s);
  }
  if (ry > 750.0) {
    s->tbUnY = 375.0 / -sc;
    grid_snap(s);
  }
  if (ry < 0.0) {
    s->tbUnY = -375.0 / -sc;
    grid_snap(s);
  }
}

// --- the find* family (targetbuilder.js:1453-1540) --------------------------
//
// Upstream's four hover probes. Each WRITES the module-level `hoverItem` and
// returns whether it found something, and the callers chain them with `if
// (!findX(...))` so the FIRST hit in a fixed priority order wins. That
// chaining is the behaviour, so it is preserved literally: these write
// s->tbHoverKind/Idx and return bool.
//
// A NAMING TRAP, carried but not repeated: upstream's parameters are all
// called `realCrossHair`, and findLine IGNORES ITS OWN — it reads the module
// global `crossHairPos` (WORLD units) instead. findPolygon's is likewise
// passed `crossHairPos` at both call sites (:585, :630), so despite the name
// it too is world. Only findTarget/findStartingPoint's ±5 boxes are world by
// construction. Every probe below is therefore in WORLD units. (/CONTEXT.md:
// almost every defect in this project has been a naming failure.)

static bool find_starting_point(FohState *s) { // :1453-1463
  for (int i = 0; i < g_doc.startingPointCount; i++) {
    const double dx = s->tbX - g_doc.startingPoint[i].x;
    const double dy = s->tbY - g_doc.startingPoint[i].y;
    if ((dx < 0 ? -dx : dx) <= 5.0 && (dy < 0 ? -dy : dy) <= 5.0) {
      s->tbHoverKind = FOH_TB_H_STARTINGPOINT;
      s->tbHoverIdx = i;
      return true;
    }
  }
  return false;
}

static bool find_target(FohState *s) { // :1465-1476
  for (int i = 0; i < g_doc.targetCount; i++) {
    const double dx = s->tbX - g_doc.target[i].x;
    const double dy = s->tbY - g_doc.target[i].y;
    if ((dx < 0 ? -dx : dx) <= 5.0 && (dy < 0 ? -dy : dy) <= 5.0) {
      s->tbHoverKind = FOH_TB_H_TARGET;
      s->tbHoverIdx = i;
      return true;
    }
  }
  return false;
}

// Is surface `j` of list `kind` owned by some polygon? (:1500-1516)
// polygonMap entries are ALL NULL on a stage loaded from a code (upstream
// BUG 2, encode.js:244 — stage_code.h models it), and upstream's own loop
// skips a null row, so an imported stage's surfaces are never "part of a
// polygon". That is the bug we carry, deliberately.
static bool part_of_polygon(int kind, int j) {
  for (int pi = 0; pi < g_doc.polygonMapCount; pi++) {
    if (map_is_null(pi)) continue;
    for (int k = 0; k < g_map[pi].count; k++) {
      if (g_map[pi].items[k].kind == kind && g_map[pi].items[k].index == j) {
        return true;
      }
    }
  }
  return false;
}

// findLine (:1477-1526). `types` is upstream's default list unless a caller
// narrows it; `background` swaps stageTemp[t] for stageTemp.background[t] and
// labels every hit "line"; `ignorePolygon` skips the ownership test.
//
// TWO SHAPES CARRIED VERBATIM, both easy to "clean up" into a different
// function: the comparison is `<=` against a closestDist that STARTS at 11
// (so 11 is a hit and 11.000001 is not), and the FIRST type in the list
// (i === 0) bypasses the polygon-ownership test entirely.
static bool find_line(FohState *s, bool background, const int *types,
                      int ntypes, bool ignorePolygon) {
  bool found = false;
  double closestDist = 11.0;
  const Vec2D at = {s->tbX, s->tbY};
  for (int i = 0; i < ntypes; i++) {
    const SurfaceList *line =
        background ? &g_doc.bgLine : list_of(types[i]);
    if (line == NULL) continue;
    for (int j = 0; j < line->count; j++) {
      Line2 seg;
      seg.a = line->items[j].p0;
      seg.b = line->items[j].p1;
      const double tempDist = distanceToLine(at, seg);
      if (tempDist <= closestDist) {
        if (i == 0) {
          closestDist = tempDist;
          s->tbHoverKind = background ? FOH_TB_H_LINE : types[i];
          s->tbHoverIdx = j;
          found = true;
        } else {
          bool partOfPolygon = false;
          if (!ignorePolygon) partOfPolygon = part_of_polygon(types[i], j);
          if (!partOfPolygon) {
            closestDist = tempDist;
            s->tbHoverKind = types[i];
            s->tbHoverIdx = j;
            found = true;
          }
        }
      }
    }
  }
  return found;
}

// findLine's default `types` (:1477): platform first — and it being first is
// what exempts platforms from the polygon-ownership test above.
static const int kFindLineDefault[5] = {FOH_TB_H_PLATFORM, FOH_TB_H_GROUND,
                                        FOH_TB_H_CEILING, FOH_TB_H_WALLL,
                                        FOH_TB_H_WALLR};

static bool find_polygon(FohState *s, bool background) { // :1528-1540
  const MlkPolygonList *poly = background ? &g_doc.bgPolygon : &g_doc.polygon;
  const Vec2D at = {s->tbX, s->tbY};
  for (int i = 0; i < poly->count; i++) {
    PolygonPts pts;
    pts.count = poly->items[i].count;
    if (pts.count > ML_MAX_LINES) pts.count = ML_MAX_LINES;
    for (int k = 0; k < pts.count; k++) pts.items[k] = poly->items[i].pts[k];
    if (distanceToPolygon(at, &pts) < 5.0) {
      s->tbHoverKind = background ? FOH_TB_H_POLYGONBG : FOH_TB_H_POLYGON;
      s->tbHoverIdx = i;
      return true;
    }
  }
  return false;
}

// centerItem (:1542-1590), all seven arms.
//
// The two POINT arms SNAP to the crosshair; every other arm OFFSETS by the
// frame's delta. That difference is upstream's and it is visible: dragging a
// target teleports it under the cursor, dragging a line carries it.
static void center_item(const FohState *s, int kind, int idx) {
  const double ox = s->tbX - g_prevX, oy = s->tbY - g_prevY;
  switch (kind) {
    case FOH_TB_H_STARTINGPOINT:
      if (idx < 0 || idx >= g_doc.startingPointCount) return;
      g_doc.startingPoint[idx].x = s->tbX;
      g_doc.startingPoint[idx].y = s->tbY;
      break;
    case FOH_TB_H_TARGET:
      if (idx < 0 || idx >= g_doc.targetCount) return;
      g_doc.target[idx].x = s->tbX;
      g_doc.target[idx].y = s->tbY;
      break;
    case FOH_TB_H_PLATFORM:
    case FOH_TB_H_GROUND:
    case FOH_TB_H_CEILING:
    case FOH_TB_H_WALLL:
    case FOH_TB_H_WALLR:
    case FOH_TB_H_LINE: {
      SurfaceList *l = list_of(kind);
      if (l == NULL || idx < 0 || idx >= l->count) return;
      l->items[idx].p0.x += ox;
      l->items[idx].p1.x += ox;
      l->items[idx].p0.y += oy;
      l->items[idx].p1.y += oy;
      break;
    }
    case FOH_TB_H_POLYGON: {
      if (idx < 0 || idx >= g_doc.polygon.count) return;
      MlkPolygon *pg = &g_doc.polygon.items[idx];
      for (int i = 0; i < pg->count; i++) {
        pg->pts[i].x += ox;
        pg->pts[i].y += oy;
        // :1580 — upstream reads polygonMap[item[1]][i], i.e. the i-th OWNED
        // SURFACE, indexed by the same loop counter as the i-th POINT. For a
        // polygon the builder drew those counts are equal; for one loaded
        // from a code the map is NULL and the whole arm is skipped, which is
        // upstream BUG 2 and is exactly why an imported polygon's outline
        // drags away from its collision surfaces. Carried.
        if (!map_is_null(idx) && i < g_map[idx].count) {
          SurfaceList *l = list_of(g_map[idx].items[i].kind);
          const int si = g_map[idx].items[i].index;
          if (l != NULL && si >= 0 && si < l->count) {
            l->items[si].p0.x += ox;
            l->items[si].p1.x += ox;
            l->items[si].p0.y += oy;
            l->items[si].p1.y += oy;
          }
        }
      }
      break;
    }
    case FOH_TB_H_POLYGONBG: {
      if (idx < 0 || idx >= g_doc.bgPolygon.count) return;
      MlkPolygon *pg = &g_doc.bgPolygon.items[idx];
      for (int i = 0; i < pg->count; i++) {
        pg->pts[i].x += ox;
        pg->pts[i].y += oy;
      }
      break;
    }
    default:
      break;
  }
}


// --- the status line --------------------------------------------------------
//
// EVERY refusal in this screen puts a STRING ON THE SCREEN. The ticket this
// implements exists because a refusal was a `deny` sound the owner could not
// hear, so "played a sound" is not an acceptable way to say no anywhere in
// this file. 120 frames == upstream's own toast life (:224 toolInfoTimer).
static void say(FohState *s, const char *msg) {
  s->tbMsg = msg;
  s->tbMsgTimer = 120;
}

// --- entering ---------------------------------------------------------------

static void tb_enter(FohState *s, int slot) {
  if (!g_docReady) { // first entry this process
    doc_template(&g_doc);
    map_all_null();
    g_docReady = true;
    s->tbSlot = -1;
  }
  if (slot >= 0) {
    // targetselect.js:113-119's edit arm: resetStageTemp + load + setEditingStage.
    const char *why = 0;
    if (slot_read(slot, &g_doc, &why)) {
      map_all_null();
      s->tbSlot = slot;
      say(s, "loaded");
    } else {
      doc_template(&g_doc);
      map_all_null();
      s->tbSlot = -1;
      say(s, why ? why : "could not load");
    }
  }
  // menu.js:87-90 enters with editingStage = -1 and does NOT reset
  // stageTemp, so a re-entry keeps the document. Only the VIEW is reset.
  s->tbPaused = false;
  s->tbPauseRow = 0;
  s->tbPane = FOH_TB_PANE_NONE;
  s->tbPaneRow = 0;
  s->tbHoverKind = FOH_TB_H_NONE;
  s->tbGrabKind = FOH_TB_H_NONE;
  s->tbHoldA = false;
  grid_snap(s);
  g_prevX = s->tbX;
  g_prevY = s->tbY;
}

// --- the pause menu ---------------------------------------------------------
//
// :780-844. Upstream's rows are Test / Save / Quit; ours are
// LOAD / SAVE / DELETE / QUIT (foh_tbuild.h explains both changes). The
// cursor arms are upstream's stick thresholds reduced to d-pad edges (D1).
static void pane_open(FohState *s, int pane) {
  s->tbPane = pane;
  s->tbPaneRow = s->tbSlot >= 0 ? s->tbSlot : 0;
}

static FohTbVerdict step_paused(FohState *s, const PlatformInput *in,
                                const PlatformInput *pv) {
  const bool aE = in->a && !pv->a;
  const bool bE = in->b && !pv->b;
  const bool sE = in->start && !pv->start;
  const bool uE = in->up && !pv->up;
  const bool dE = in->down && !pv->down;

  if (s->tbPane != FOH_TB_PANE_NONE) {
    // the slot list
    if (bE) { // back to the pause rows
      s->tbPane = FOH_TB_PANE_NONE;
      foh_snd_push(s, "menuBack");
      return FOH_TB_STAY;
    }
    if (uE) {
      s->tbPaneRow = (s->tbPaneRow + FOH_TB_SLOTS - 1) % FOH_TB_SLOTS;
      foh_snd_push(s, "menuSelect");
      return FOH_TB_STAY;
    }
    if (dE) {
      s->tbPaneRow = (s->tbPaneRow + 1) % FOH_TB_SLOTS;
      foh_snd_push(s, "menuSelect");
      return FOH_TB_STAY;
    }
    if (!aE) return FOH_TB_STAY;
    const int slot = s->tbPaneRow;
    const char *why = 0;
    switch (s->tbPane) {
      case FOH_TB_PANE_LOAD:
        if (slot_read(slot, &g_doc, &why)) {
          map_all_null();
          s->tbSlot = slot;
          s->tbHoverKind = FOH_TB_H_NONE;
          s->tbGrabKind = FOH_TB_H_NONE;
          s->tbHoldA = false;
          s->tbPane = FOH_TB_PANE_NONE;
          s->tbPaused = false;
          foh_snd_push(s, "menuForward");
          say(s, "loaded");
        } else {
          foh_snd_push(s, "deny");
          say(s, why ? why : "empty");
        }
        break;
      case FOH_TB_PANE_SAVE: {
        // Refuse BEFORE writing anything a player would then have to
        // discover is unplayable at launch. This is R2's safe half.
        const char *bad = doc_unplayable(&g_doc);
        if (bad) {
          foh_snd_push(s, "deny");
          say(s, bad);
          break;
        }
        if (slot_write(slot, &g_doc, &why)) {
          s->tbSlot = slot;
          s->tbPane = FOH_TB_PANE_NONE;
          s->tbPaused = false;
          foh_snd_push(s, "menuForward");
          say(s, "saved");
        } else {
          // The named publish failure, ON SCREEN: "disk full",
          // "cannot open the temp file", "rename publish failed"...
          foh_snd_push(s, "deny");
          say(s, why ? why : "save failed");
        }
        break;
      }
      case FOH_TB_PANE_DELETE: {
        char path[512];
        if (!slot_path(slot, path, sizeof path)) {
          foh_snd_push(s, "deny");
          say(s, "path too long");
          break;
        }
        // D43: deleting slot i removes slot i's FILE and touches nothing
        // else. Upstream shifts every higher cookie down
        // (targetselect.js:83-97), which is the same clobbering family the
        // owner ruled against — a shift makes "Custom 4" mean a different
        // stage than it did a second ago, and its records with it.
        if (remove(path) == 0) {
          if (s->tbSlot == slot) s->tbSlot = -1;
          foh_snd_push(s, "menuBack");
          say(s, "deleted");
        } else {
          foh_snd_push(s, "deny");
          say(s, "empty");
        }
        break;
      }
      default: break;
    }
    return FOH_TB_STAY;
  }

  // the four pause rows
  if (uE) {
    s->tbPauseRow = (s->tbPauseRow + FOH_TB_PAUSE_ROWS - 1) % FOH_TB_PAUSE_ROWS;
    foh_snd_push(s, "menuSelect"); // :785
    return FOH_TB_STAY;
  }
  if (dE) {
    s->tbPauseRow = (s->tbPauseRow + 1) % FOH_TB_PAUSE_ROWS;
    foh_snd_push(s, "menuSelect"); // :792
    return FOH_TB_STAY;
  }
  if (sE || bE) { // :838-842 — START closes the pause menu (menuBack)
    s->tbPaused = false;
    s->tbPauseRow = 0;
    foh_snd_push(s, "menuBack");
    return FOH_TB_STAY;
  }
  if (!aE) return FOH_TB_STAY;
  switch (s->tbPauseRow) {
    case FOH_TB_PAUSE_LOAD:
      foh_snd_push(s, "menuForward");
      pane_open(s, FOH_TB_PANE_LOAD);
      break;
    case FOH_TB_PAUSE_SAVE:
      foh_snd_push(s, "menuForward"); // :803
      pane_open(s, FOH_TB_PANE_SAVE);
      break;
    case FOH_TB_PAUSE_DELETE:
      foh_snd_push(s, "menuForward");
      pane_open(s, FOH_TB_PANE_DELETE);
      break;
    default: // QUIT — :832-835 changeGamemode(1)
      foh_snd_push(s, "menuForward");
      return FOH_TB_QUIT;
  }
  return FOH_TB_STAY;
}

// --- the tool arms (targetbuilder.js:277-770) -------------------------------
//
// One function per tool, in upstream's own index order. Every one of them is
// reached from tb_step's switch and nothing else calls them.

// Refusals that would otherwise be a sound the owner cannot hear. Upstream
// draws "Too small" / "Bad angle" / "Walls too close" in an 80x25 CANVAS box
// AT THE CURSOR (:1395-1415); that is 16x5 device pixels here, which cannot
// hold text, so they go to the status line (the design spike's own call).
static void say(FohState *s, const char *msg);

// --- POLYGON (:277-411) -----------------------------------------------------

// currentPolygonLines[k] is (drawingPolygon[k], drawingPolygon[k+1]) — see
// foh.h's note on tbPolyLinesN. Materialised on demand for intersectsAny.
static void poly_lines(const FohState *s, int skipFirst, LineList *out) {
  out->count = 0;
  for (int k = skipFirst; k < s->tbPolyLinesN; k++) {
    if (out->count >= ML_MAX_LINES) break;
    if (k + 1 >= s->tbPolyN) break;
    out->items[out->count].a.x = s->tbPolyX[k];
    out->items[out->count].a.y = s->tbPolyY[k];
    out->items[out->count].b.x = s->tbPolyX[k + 1];
    out->items[out->count].b.y = s->tbPolyY[k + 1];
    out->count++;
  }
}

static void poly_stop(FohState *s) { // stopDrawingPolygon (:154-158)
  s->tbDrawingPoly = false;
  s->tbPolyN = 0;
  s->tbPolyLinesN = 0;
  s->tbDenied = false;
}

// Push one closed loop into the document. Upstream inlines this in the
// close arm (:305-390); it is a function here because it is 60 lines and
// because the cap refusals need one place to live.
static void poly_close(FohState *s) {
  const int n = s->tbPolyN; // the tracking element was popped by the caller
  // :313-318 — signed area, then direction = sign(area) * -1. Upstream
  // writes `direction != 0 && direction != -0`, which is ONE test in C
  // (-0.0 == 0.0); both spellings are kept in this comment rather than in
  // two identical branches.
  double area = 0.0;
  for (int i = 0; i < n; i++) {
    const int nx = (i == n - 1) ? 0 : i + 1;
    area += (s->tbPolyX[nx] - s->tbPolyX[i]) * (s->tbPolyY[nx] + s->tbPolyY[i]);
  }
  const double direction = (area > 0.0 ? 1.0 : (area < 0.0 ? -1.0 : area)) * -1.0;
  if (direction == 0.0) { // a flat line: upstream's "Too small" (:399-400)
    say(s, "too small");
    return;
  }
  MlkPolygonList *plist = s->tbDrawMode ? &g_doc.bgPolygon : &g_doc.polygon;
  if (plist->count >= MLK_MAX_POLYGONS) {
    foh_snd_push(s, "deny");
    say(s, "16 polygons max in this build");
    return;
  }
  if (n > MLK_MAX_POLY_POINTS) {
    foh_snd_push(s, "deny");
    say(s, "32 points max in this build");
    return;
  }
  // Count the collision surfaces this loop would add BEFORE adding any, so
  // the caps refuse a whole polygon rather than half of one. R2's rule at a
  // third site: refuse where the message is useful, never truncate.
  if (!s->tbDrawMode) {
    const long total = (long)g_doc.s.ground.count + g_doc.s.ceiling.count +
                       g_doc.s.wallL.count + g_doc.s.wallR.count +
                       g_doc.s.platform.count;
    if (total + n > ML_MAX_LABELLED_SURFACES) {
      foh_snd_push(s, "deny");
      say(s, "too many surfaces for this build");
      return;
    }
  }
  const int pi = plist->count;
  plist->items[pi].count = 0;
  plist->count++;
  if (!s->tbDrawMode) {
    g_map[pi].count = 0;
    map_sync();
  }
  // :328-380 — walk the loop in the winding direction, writing the point
  // and (foreground only) classifying the edge to it.
  int curIndex = (direction == 1.0) ? 0 : n - 1;
  for (int i = 0; i < n; i++) {
    int nextIndex = curIndex + (int)direction;
    if (nextIndex == -1) {
      nextIndex = n - 1;
    } else if (nextIndex == n) {
      nextIndex = 0;
    }
    const double wx = to_wx(s->tbPolyX[curIndex]);
    const double wy = to_wy(s->tbPolyY[curIndex]);
    plist->items[pi].pts[i].x = wx;
    plist->items[pi].pts[i].y = wy;
    plist->items[pi].count = i + 1;
    if (!s->tbDrawMode) {
      const double ax = to_wx(s->tbPolyX[nextIndex]);
      const double ay = to_wy(s->tbPolyY[nextIndex]);
      double angle = fd_atan2(ay - wy, ax - wx);
      // :349-350 — `if (Math.sign(angle) === -1) angle += twoPi`. Math.sign
      // of -0 is -0, NOT -1, so a -0 angle does NOT get 2pi added; the C
      // test is `angle < 0.0`, which is false for -0.0 too.
      if (angle < 0.0) angle += 2.0 * ML_TB_PI;
      int kind;
      if (angle <= ML_TB_PI / 6.0 || angle >= ML_TB_PI * 11.0 / 6.0) {
        kind = FOH_TB_H_GROUND;
      } else if (angle >= ML_TB_PI * 5.0 / 6.0 && angle <= ML_TB_PI * 7.0 / 6.0) {
        kind = FOH_TB_H_CEILING;
      } else if (angle > ML_TB_PI) {
        kind = FOH_TB_H_WALLR;
      } else {
        kind = FOH_TB_H_WALLL;
      }
      SurfaceList *l = list_of(kind);
      if (l != NULL && l->count < ML_MAX_SURFACES) {
        memset(&l->items[l->count], 0, sizeof l->items[0]);
        l->items[l->count].p0.x = wx;
        l->items[l->count].p0.y = wy;
        l->items[l->count].p1.x = ax;
        l->items[l->count].p1.y = ay;
        g_map[pi].items[g_map[pi].count].kind = kind;
        g_map[pi].items[g_map[pi].count].index = l->count;
        g_map[pi].count++;
        l->count++;
      } else {
        // Only reachable if ONE list overflows while the total does not.
        say(s, "too many surfaces of one kind");
      }
    }
    curIndex = nextIndex;
  }
}

static void tool_polygon(FohState *s, bool aE, bool bE) {
  if (aE) {
    if (!s->tbDrawingPoly) { // :279-288 initiate
      const MlkPolygonList *plist =
          s->tbDrawMode ? &g_doc.bgPolygon : &g_doc.polygon;
      // upstream's cap here is 120 (:280); this build's is MLK_MAX_POLYGONS
      if (plist->count < MLK_MAX_POLYGONS) {
        s->tbPolyLinesN = 0;
        s->tbPolyX[0] = to_cx(s->tbX);
        s->tbPolyY[0] = to_cy(s->tbY);
        s->tbPolyX[1] = s->tbPolyX[0];
        s->tbPolyY[1] = s->tbPolyY[0];
        s->tbPolyN = 2;
        s->tbDrawingPoly = true;
        s->tbDenied = false;
        foh_snd_push(s, "blunthit");
      } else {
        foh_snd_push(s, "deny");
        say(s, "16 polygons max in this build");
      }
      return;
    }
    // :290-395 RELEASE — one more vertex, or close
    const int lg = s->tbPolyN;
    const double rx = to_cx(s->tbX), ry = to_cy(s->tbY);
    const double d0x = rx - s->tbPolyX[0], d0y = ry - s->tbPolyY[0];
    const bool canClose =
        (d0x < 0 ? -d0x : d0x) < 2.0 && (d0y < 0 ? -d0y : d0y) < 2.0; // :292
    if (lg > 3 && !s->tbDenied) s->tbPolyLinesN++; // :293-295
    Line2 next;
    next.a.x = s->tbPolyX[lg - 2];
    next.a.y = s->tbPolyY[lg - 2];
    next.b.x = rx;
    next.b.y = ry;
    LineList rel;
    poly_lines(s, canClose ? 1 : 0, &rel); // :297 the slice(1)
    if (intersectsAny(next, &rel) ||
        (next.a.x == next.b.x && next.a.y == next.b.y)) { // :298
      foh_snd_push(s, "deny");
      say(s, "that edge crosses the outline");
      s->tbDenied = true;
      return;
    }
    foh_snd_push(s, "blunthit");
    s->tbDenied = false;
    if (canClose) {
      s->tbDrawingPoly = false;
      s->tbPolyN--; // :309 pop the duplicated origin
      if (s->tbPolyN >= 3) {
        poly_close(s);
      } else {
        say(s, "too small"); // :401-402
      }
      s->tbPolyN = 0;
      s->tbPolyLinesN = 0;
      return;
    }
    if (s->tbPolyN >= FOH_TB_MAX_POLY_PTS) { // upstream is unbounded
      foh_snd_push(s, "deny");
      say(s, "32 points max in this build");
      return;
    }
    s->tbPolyX[s->tbPolyN] = rx; // :393 push
    s->tbPolyY[s->tbPolyN] = ry;
    s->tbPolyN++;
    return;
  }
  // the non-A branch (:385-409): track, indicate, and B pops
  if (s->tbDrawingPoly) {
    s->tbPolyX[s->tbPolyN - 1] = to_cx(s->tbX); // :386
    s->tbPolyY[s->tbPolyN - 1] = to_cy(s->tbY);
    const double d0x = s->tbPolyX[s->tbPolyN - 1] - s->tbPolyX[0];
    const double d0y = s->tbPolyY[s->tbPolyN - 1] - s->tbPolyY[0];
    if ((d0x < 0 ? -d0x : d0x) < 2.0 && (d0y < 0 ? -d0y : d0y) < 2.0 &&
        s->tbPolyN >= 3) {
      s->tbConnectInd = true; // :389-391
      s->tbConnectX = s->tbPolyX[0];
      s->tbConnectY = s->tbPolyY[0];
    }
  }
  if (bE && s->tbDrawingPoly) { // :396-408
    if (s->tbPolyN <= 2) {
      poly_stop(s);
    } else {
      s->tbPolyN--;
      if (s->tbPolyLinesN > 0) s->tbPolyLinesN--;
    }
    foh_snd_push(s, "menuBack");
  }
}

// --- PLATFORM (:412-458) and WALL (:459-512) --------------------------------
//
// One shape, two guard sets. Both store the drag in CANVAS units because
// upstream's length tests are in canvas pixels and its angle tests are in
// world units — converting early would change which of them bites.

static void drag_begin(FohState *s) {
  s->tbDragX0 = s->tbDragX1 = to_cx(s->tbX);
  s->tbDragY0 = s->tbDragY1 = to_cy(s->tbY);
  s->tbHoldA = true;
}

// The left/right ordering and canvas->world conversion both tools share
// (:431-436, :478-482).
static void drag_ends(const FohState *s, Vec2D *l, Vec2D *r) {
  const int left = (s->tbDragX0 - s->tbDragX1 < 0.0) ? 0 : 1;
  const double lx = left == 0 ? s->tbDragX0 : s->tbDragX1;
  const double ly = left == 0 ? s->tbDragY0 : s->tbDragY1;
  const double rx = left == 0 ? s->tbDragX1 : s->tbDragX0;
  const double ry = left == 0 ? s->tbDragY1 : s->tbDragY0;
  l->x = to_wx(lx);
  l->y = to_wy(ly);
  r->x = to_wx(rx);
  r->y = to_wy(ry);
}

static bool push_surface(FohState *s, int kind, Vec2D a, Vec2D b) {
  SurfaceList *l = list_of(kind);
  if (l == NULL) return false;
  const long total = (long)g_doc.s.ground.count + g_doc.s.ceiling.count +
                     g_doc.s.wallL.count + g_doc.s.wallR.count +
                     g_doc.s.platform.count;
  const bool bg = (kind == FOH_TB_H_LINE);
  if (l->count >= ML_MAX_SURFACES ||
      (!bg && total >= ML_MAX_LABELLED_SURFACES)) {
    foh_snd_push(s, "deny");
    say(s, "too many surfaces for this build");
    return false;
  }
  memset(&l->items[l->count], 0, sizeof l->items[0]);
  l->items[l->count].p0 = a;
  l->items[l->count].p1 = b;
  l->count++;
  return true;
}

static void tool_platform(FohState *s, bool aE, bool aHeld) {
  if (!s->tbHoldA) {
    if (aE) drag_begin(s); // :415-419
    return;
  }
  s->tbDragX1 = to_cx(s->tbX); // :421-424 stretch, :427 release both set it
  s->tbDragY1 = to_cy(s->tbY);
  if (aHeld) return;
  // RELEASE (:426-453)
  const double dx = s->tbDragX0 - s->tbDragX1;
  Vec2D d0 = {s->tbDragX0, s->tbDragY0}, d1 = {s->tbDragX1, s->tbDragY1};
  // :429 — WIDTH >= 10, OR (drawMode AND manhattan >= 10). The `||` means
  // the width test still runs first in drawMode and can pass alone.
  if ((dx < 0 ? -dx : dx) >= 10.0 ||
      (s->tbDrawMode && manhattanDist(d0, d1) >= 10.0)) {
    Vec2D l, r;
    drag_ends(s, &l, &r);
    if (s->tbDrawMode) {
      push_surface(s, FOH_TB_H_LINE, l, r); // :436
    } else {
      const double angle = fd_atan2(r.y - l.y, r.x - l.x); // :438
      const double aa = angle < 0 ? -angle : angle;
      // :441 — `|angle| <= PI/6 && |angle| >= -PI/6`. The second conjunct is
      // vacuous (an absolute value is never below -PI/6) and is carried as
      // written rather than "simplified" away.
      if (aa <= ML_TB_PI / 6.0 && aa >= -ML_TB_PI / 6.0) {
        push_surface(s, FOH_TB_H_PLATFORM, l, r);
      } else {
        say(s, "bad angle"); // :443-445
      }
    }
  } else {
    say(s, "too small"); // :449-451
  }
  s->tbHoldA = false;
  foh_snd_push(s, "blunthit"); // :453 — plays on EVERY release, refused too
}

static void tool_wall(FohState *s, bool aE, bool aHeld) {
  if (!s->tbHoldA) {
    if (aE) drag_begin(s); // :461-465
    return;
  }
  s->tbDragX1 = to_cx(s->tbX);
  s->tbDragY1 = to_cy(s->tbY);
  if (aHeld) return;
  Vec2D d0 = {s->tbDragX0, s->tbDragY0}, d1 = {s->tbDragX1, s->tbDragY1};
  if (manhattanDist(d0, d1) >= 10.0) { // :476 — manhattan, no drawMode arm
    Vec2D l, r;
    drag_ends(s, &l, &r);
    const double angle = fd_atan2(r.y - l.y, r.x - l.x);
    const double aa = angle < 0 ? -angle : angle;
    const int kind = wall_kind(s->tbWallType);
    // :483-492 — the proximity guard runs ONLY for wallL/wallR; for
    // ground/ceiling `distanceToOtherWalls` is upstream's `undefined` and the
    // `!== undefined` test skips the arm. Our empty-list minimum is INFINITY
    // rather than undefined and `INFINITY < 2` is false either way, so the
    // arm is not taken in either language (see get_connected.h's sibling note
    // in detect_intersections.h).
    bool tooClose = false;
    if (kind == FOH_TB_H_WALLL || kind == FOH_TB_H_WALLR) {
      const SurfaceList *other =
          (kind == FOH_TB_H_WALLL) ? &g_doc.s.wallR : &g_doc.s.wallL;
      LineList others;
      others.count = 0;
      for (int i = 0; i < other->count && others.count < ML_MAX_LINES; i++) {
        others.items[others.count].a = other->items[i].p0;
        others.items[others.count].b = other->items[i].p1;
        others.count++;
      }
      Line2 mine;
      mine.a = l;
      mine.b = r;
      tooClose = lineDistanceToLines(mine, &others) < 2.0;
    }
    if (tooClose) {
      say(s, "walls too close"); // :487-490
    } else if (((kind == FOH_TB_H_GROUND || kind == FOH_TB_H_CEILING) &&
                aa <= ML_TB_PI / 6.0) ||
               ((kind == FOH_TB_H_WALLL || kind == FOH_TB_H_WALLR) &&
                aa != 0.0 && aa != ML_TB_PI)) {
      // :493 — EXACT float comparisons on the wall arm, carried verbatim:
      // a wall one ulp off horizontal is legal upstream and is legal here.
      push_surface(s, kind, l, r);
    } else {
      say(s, "bad angle"); // :500-503
    }
  } else {
    say(s, "too small"); // :506-508
  }
  s->tbHoldA = false;
  foh_snd_push(s, "blunthit");
}

// --- LEDGE (:513-541) -------------------------------------------------------

static void tool_ledge(FohState *s, bool aE) {
  s->tbLedgeKind = FOH_TB_H_NONE;
  // :516 — findLine over ["platform","ground"] with ignorePolygon TRUE, so a
  // polygon's own ground edges CAN take a ledge.
  static const int types[2] = {FOH_TB_H_PLATFORM, FOH_TB_H_GROUND};
  if (!find_line(s, false, types, 2, true)) return;
  const SurfaceList *l = list_of(s->tbHoverKind);
  if (l == NULL || s->tbHoverIdx >= l->count) return;
  const Vec2D at = {s->tbX, s->tbY};
  const double toLeft = manhattanDist(at, l->items[s->tbHoverIdx].p0);
  const double toRight = manhattanDist(at, l->items[s->tbHoverIdx].p1);
  s->tbLedgeKind = s->tbHoverKind;
  s->tbLedgeIdx = s->tbHoverIdx;
  s->tbLedgeSide = (toRight < toLeft) ? 1 : 0; // :518-523
  if (!aE) return;
  const char letter = ledge_letter(s->tbLedgeKind);
  for (int j = 0; j < g_doc.ledgeCount; j++) { // :526-533 toggle off
    if (g_doc.ledge[j].list == letter &&
        g_doc.ledge[j].index == (double)s->tbLedgeIdx &&
        g_doc.ledge[j].point == (double)s->tbLedgeSide) {
      for (int k = j; k + 1 < g_doc.ledgeCount; k++) {
        g_doc.ledge[k] = g_doc.ledge[k + 1];
      }
      g_doc.ledgeCount--;
      foh_snd_push(s, "blunthit");
      return;
    }
  }
  if (g_doc.ledgeCount >= ML_MAX_LEDGES) { // upstream is unbounded (:535)
    foh_snd_push(s, "deny");
    say(s, "16 ledges max in this build");
    return;
  }
  g_doc.ledge[g_doc.ledgeCount].list = letter;
  g_doc.ledge[g_doc.ledgeCount].index = (double)s->tbLedgeIdx;
  g_doc.ledge[g_doc.ledgeCount].point = (double)s->tbLedgeSide;
  g_doc.ledgeCount++;
  foh_snd_push(s, "blunthit"); // :539
}

// --- DAMAGE (:542-559) ------------------------------------------------------

static void tool_damage(FohState *s, bool aE) {
  static const int types[4] = {FOH_TB_H_WALLL, FOH_TB_H_WALLR,
                               FOH_TB_H_CEILING, FOH_TB_H_GROUND};
  if (!find_line(s, false, types, 4, true)) {
    s->tbHoverKind = FOH_TB_H_NONE; // :544-546
    return;
  }
  if (!aE) return;
  SurfaceList *l = list_of(s->tbHoverKind);
  if (l == NULL || s->tbHoverIdx >= l->count) return;
  Surface *w = &l->items[s->tbHoverIdx];
  const char *want = kDamageTypeValues[s->tbDamageType];
  // :550-556 — set when the props are ABSENT or carry a DIFFERENT type,
  // otherwise write `{damageType: null}`. Upstream does NOT delete the key,
  // and DT_NULL vs DT_ABSENT are different values to encode.js:36-40, so the
  // distinction is kept rather than collapsed.
  const bool differs = !w->hasProps || !w->propsHasDamageTypeKey ||
                       w->propsDamageType.tag != DT_STR ||
                       strcmp(w->propsDamageType.str, want) != 0;
  w->hasProps = true;
  w->propsHasDamageTypeKey = true;
  if (differs) {
    w->propsDamageType.tag = DT_STR;
    snprintf(w->propsDamageType.str, sizeof w->propsDamageType.str, "%s", want);
  } else {
    w->propsDamageType = damage_null();
  }
  foh_snd_push(s, "menuSelect"); // :557
}

// --- TARGET (:560-571) ------------------------------------------------------

static void tool_target(FohState *s, bool aE) {
  if (!aE) return;
  // Upstream's cap here is 20 (:563). The PORT's is the SIM's 10 — R2,
  // refused where the message is useful rather than by raising a cap that is
  // _Static_assert-tied to upstream's own 10-element targetDestroyed literal.
  if (g_doc.targetCount < FOH_TB_PLAYABLE_TARGETS) {
    g_doc.target[g_doc.targetCount].x = s->tbX;
    g_doc.target[g_doc.targetCount].y = s->tbY;
    g_doc.targetCount++;
    foh_snd_push(s, "blunthit"); // :566
  } else {
    foh_snd_push(s, "deny"); // :568
    say(s, "10 targets max");
  }
}

// --- MOVE (:572-618) --------------------------------------------------------

static void tool_move(FohState *s, bool aE, bool aHeld) {
  if (s->tbGrabKind == FOH_TB_H_NONE) {
    if (s->tbDrawMode) { // :575-580
      if (!find_polygon(s, true)) {
        static const int t[1] = {FOH_TB_H_LINE};
        if (!find_line(s, true, t, 1, false)) s->tbHoverKind = FOH_TB_H_NONE;
      }
    } else { // :582-590 — the priority chain, in upstream's order
      if (!find_starting_point(s)) {
        if (!find_target(s)) {
          if (!find_polygon(s, false)) {
            if (!find_line(s, false, kFindLineDefault, 5, false)) {
              s->tbHoverKind = FOH_TB_H_NONE;
            }
          }
        }
      }
    }
  } else {
    s->tbHoverKind = s->tbGrabKind; // :591-592
    s->tbHoverIdx = s->tbGrabIdx;
  }
  if (s->tbHoverKind == FOH_TB_H_NONE) return;
  if (!s->tbHoldA) {
    if (aE) { // :595-600
      center_item(s, s->tbHoverKind, s->tbHoverIdx);
      s->tbGrabKind = s->tbHoverKind;
      s->tbGrabIdx = s->tbHoverIdx;
      s->tbHoldA = true;
    }
    return;
  }
  center_item(s, s->tbHoverKind, s->tbHoverIdx); // :602-611, both arms
  if (aHeld) return;
  s->tbHoldA = false;
  s->tbGrabKind = FOH_TB_H_NONE;
  foh_snd_push(s, "blunthit"); // :609
}

// --- DELETE (:619-738) ------------------------------------------------------
//
// The most intricate arm in the file, and the reason is bookkeeping: removing
// a surface has to renumber every polygonMap entry and every ledge ABOVE it
// and drop the ledges that pointed AT it. `connected` is spliced too
// (:651, :689, :714) — dead here, see util/get_connected.h.

static void list_splice(SurfaceList *l, int idx) {
  for (int k = idx; k + 1 < l->count; k++) l->items[k] = l->items[k + 1];
  l->count--;
}

// The three things that must happen for every removed surface of `kind` at
// `idx`, in upstream's own order (:667-690, :700-728).
static void after_surface_removed(int kind, int idx) {
  for (int pi = 0; pi < g_doc.polygonMapCount; pi++) { // :668-676
    if (map_is_null(pi)) continue;
    for (int k = 0; k < g_map[pi].count; k++) {
      if (g_map[pi].items[k].kind == kind && g_map[pi].items[k].index > idx) {
        g_map[pi].items[k].index--;
      }
    }
  }
  if (kind != FOH_TB_H_GROUND && kind != FOH_TB_H_PLATFORM) return;
  const char letter = ledge_letter(kind);
  for (int n = 0; n < g_doc.ledgeCount; n++) { // :678-687
    if (g_doc.ledge[n].list != letter) continue;
    if (g_doc.ledge[n].index > (double)idx) {
      g_doc.ledge[n].index--;
    } else if (g_doc.ledge[n].index == (double)idx) {
      for (int k = n; k + 1 < g_doc.ledgeCount; k++) {
        g_doc.ledge[k] = g_doc.ledge[k + 1];
      }
      g_doc.ledgeCount--;
      n--; // upstream's own `n--` after the splice
    }
  }
}

static void polylist_splice(MlkPolygonList *l, int idx) {
  for (int k = idx; k + 1 < l->count; k++) l->items[k] = l->items[k + 1];
  l->count--;
}

static void tool_delete(FohState *s, bool aE) {
  if (s->tbDrawMode) { // :621-627
    if (!find_polygon(s, true)) {
      static const int t[1] = {FOH_TB_H_LINE};
      if (!find_line(s, true, t, 1, false)) s->tbHoverKind = FOH_TB_H_NONE;
    }
  } else {
    if (!find_target(s)) {
      if (!find_polygon(s, false)) {
        if (!find_line(s, false, kFindLineDefault, 5, false)) {
          s->tbHoverKind = FOH_TB_H_NONE;
        }
      }
    }
  }
  if (s->tbHoverKind == FOH_TB_H_NONE || !aE) return;
  const int kind = s->tbHoverKind, idx = s->tbHoverIdx;
  switch (kind) {
    case FOH_TB_H_PLATFORM: // :641-654
    case FOH_TB_H_GROUND:
    case FOH_TB_H_CEILING:
    case FOH_TB_H_WALLL:
    case FOH_TB_H_WALLR: {
      SurfaceList *l = list_of(kind);
      if (l == NULL || idx >= l->count) return;
      // ORDER MATTERS AND IT IS UPSTREAM'S: the platform arm renumbers the
      // ledges BEFORE splicing the list (:642-652), the ground arm splices
      // FIRST and renumbers after (:664-690). Both end in the same place
      // here because after_surface_removed does not read the list, but the
      // two arms are kept distinct in the citation so a future reader
      // checks rather than assumes.
      list_splice(l, idx);
      after_surface_removed(kind, idx);
      foh_snd_push(s, "menuBack");
      break;
    }
    case FOH_TB_H_TARGET: // :656-659
      for (int k = idx; k + 1 < g_doc.targetCount; k++) {
        g_doc.target[k] = g_doc.target[k + 1];
      }
      g_doc.targetCount--;
      foh_snd_push(s, "menuBack");
      break;
    case FOH_TB_H_LINE: // :660-663
      list_splice(&g_doc.bgLine, idx);
      foh_snd_push(s, "menuBack");
      break;
    case FOH_TB_H_POLYGONBG: // :692-695
      polylist_splice(&g_doc.bgPolygon, idx);
      foh_snd_push(s, "menuBack");
      break;
    case FOH_TB_H_POLYGON: { // :696-731
      if (!map_is_null(idx)) {
        for (int j = 0; j < g_map[idx].count; j++) {
          const int t = g_map[idx].items[j].kind;
          const int si = g_map[idx].items[j].index;
          SurfaceList *l = list_of(t);
          if (l == NULL || si >= l->count) continue;
          list_splice(l, si);
          after_surface_removed(t, si);
        }
      }
      // splice the map row, then the polygon (:732-733)
      for (int k = idx; k + 1 < g_doc.polygonMapCount; k++) g_map[k] = g_map[k + 1];
      polylist_splice(&g_doc.polygon, idx);
      map_sync();
      foh_snd_push(s, "menuBack");
      break;
    }
    default:
      break;
  }
  s->tbHoverKind = FOH_TB_H_NONE; // :734
}

// --- SCALE (:739-764) -------------------------------------------------------
//
// DEVIATION D55 (registration only, nothing changed): upstream cycles this
// with the d-pad, which is the crosshair here — but upstream ALSO freezes the
// crosshair while SCALE is active (:172-174, `multi = 0`), so the conflict is
// already solved by upstream's own construction. crosshair_step reads the
// same condition.
static void tool_scale(FohState *s, const PlatformInput *in) {
  if (in->up) { // lsY > 0
    s->tbScaleScroll++;
    if (s->tbScaleScroll > 5) { // every SIXTH frame
      s->tbScaleScroll = 0;
      g_doc.scale += 0.1;
      foh_snd_push(s, "menuSelect");
      if (g_doc.scale > 6.0) g_doc.scale = 6.0; // clamp AFTER the add
    }
  } else if (in->down) {
    s->tbScaleScroll++;
    if (s->tbScaleScroll > 5) {
      s->tbScaleScroll = 0;
      g_doc.scale -= 0.1;
      foh_snd_push(s, "menuSelect");
      if (g_doc.scale < 2.0) g_doc.scale = 2.0;
    }
  } else {
    s->tbScaleScroll = 0;
  }
}

// --- DRAW MODE (:765-770) ---------------------------------------------------

static void tool_drawmode(FohState *s, bool aE) {
  if (!aE) return;
  s->tbDrawMode = 1 - s->tbDrawMode;
  // Upstream plays nothing here and shows nothing; on a 240px screen an
  // invisible mode switch is a trap, so the status line names the plane.
  say(s, s->tbDrawMode ? "background plane" : "collision plane");
}

// --- the editor -------------------------------------------------------------

static FohTbVerdict tb_step(FohState *s, const PlatformInput *in,
                            const PlatformInput *pv) {
  if (s->tbMsgTimer > 0) s->tbMsgTimer--;
  if (s->tbToolTimer > 0) s->tbToolTimer--;
  s->tbConnectInd = false; // :160, cleared every frame
  if (s->tbPaused) return step_paused(s, in, pv);

  const bool aE = in->a && !pv->a;
  const bool bE = in->b && !pv->b;
  const bool sE = in->start && !pv->start;
  const bool yE = in->y && !pv->y;
  const bool lE = in->l && !pv->l;
  const bool rE = in->r && !pv->r;
  const bool uE = in->up && !pv->up;
  const bool dE = in->down && !pv->down;

  // DEVIATION D50: B LEAVES. Upstream's builder has no B exit at all — you
  // leave through START -> Quit (:832-835), which this screen also has. B is
  // added because it is the FOH's universal back edge on every other screen
  // (menu.js:164-190, css.js:186-194, stageselect.js:79,
  // targetselect.js:76-81), and a screen where the back button silently does
  // something else is a trap the owner would find by falling into it. It
  // takes the SAME edge and the SAME sound as Quit.
  //
  // DEVIATION D56 narrows that: while a POLYGON is being drawn, B pops the
  // last vertex, which is upstream's own binding (:396-408) under upstream's
  // own guard (`if (amDrawingPolygon)`). A half-drawn polygon is unfinished
  // work, and backing out of it before backing out of the screen is what a
  // back button is for. The two are not in conflict: the guard decides.
  if (bE && !(s->tbTool == FOH_TB_TOOL_POLYGON && s->tbDrawingPoly)) {
    foh_snd_push(s, "menuBack");
    return FOH_TB_QUIT;
  }
  if (sE) { // :774-777 — START pauses
    s->tbPaused = true;
    s->tbPauseRow = 0;
    s->tbPane = FOH_TB_PANE_NONE;
    s->tbHoldA = false;
    s->tbGrabKind = FOH_TB_H_NONE;
    poly_stop(s);
    foh_snd_push(s, "pause");
    return FOH_TB_STAY;
  }

  crosshair_step(s, in);

  // :207-212's grid cycle, moved from `z` to Y (D50 — no z button on this
  // hardware; the note is at crosshair_step's `multi`).
  if (yE) {
    s->tbGrid = (s->tbGrid + 1) % FOH_TB_GRIDS;
    grid_snap(s);
    foh_snd_push(s, "menuSelect");
    s->tbToolTimer = 120;
  }
  // :213-266's cycles.
  //
  // DEVIATION D54: the TYPE cycles move from the d-pad to X + shoulder.
  // Upstream puts wallType (:231-248) and damageType (:249-267) on d-pad
  // up/down WITHOUT freezing the crosshair, and the d-pad is the crosshair
  // here. X is already the precision modifier (:171, narrowed to X-only by
  // D50), so holding it turns the shoulders from the TOOL cycle into the
  // TYPE cycle. Nothing new is bound and X keeps its d-pad role. Upstream
  // also accepts d-pad left/right for the tool cycle (:213, :221); the port
  // cannot, for the same reason.
  const bool typeMod = in->x;
  if (typeMod && (lE || rE) && s->tbTool == FOH_TB_TOOL_WALL) {
    s->tbWallType = (s->tbWallType + (rE ? 1 : FOH_TB_WALLTYPES - 1)) %
                    FOH_TB_WALLTYPES;
    foh_snd_push(s, "menuSelect");
    s->tbToolTimer = 120;
  } else if (typeMod && (lE || rE) && s->tbTool == FOH_TB_TOOL_DAMAGE) {
    s->tbDamageType = (s->tbDamageType + (rE ? 1 : FOH_TB_DAMAGETYPES - 1)) %
                      FOH_TB_DAMAGETYPES;
    foh_snd_push(s, "menuSelect");
    s->tbToolTimer = 120;
  } else if (lE || rE) {
    int pos = tool_pos(s->tbTool);
    if (pos < 0) pos = 0;
    pos = (pos + (rE ? 1 : FOH_TB_TOOLS - 1)) % FOH_TB_TOOLS;
    s->tbTool = kToolOrder[pos];
    // :226-227 — cycling FORWARD into WALL while drawMode is on skips to
    // MOVE, because tools 2-4 have no background plane to edit.
    if (rE && s->tbDrawMode && s->tbTool == FOH_TB_TOOL_WALL) {
      s->tbTool = FOH_TB_TOOL_MOVE;
    }
    foh_snd_push(s, "menuSelect"); // :215, :224
    s->tbToolTimer = 120;          // :219
    s->tbGrabKind = FOH_TB_H_NONE;
    s->tbHoldA = false;
    poly_stop(s); // :220, :228 stopDrawingPolygon
  }
  // :269-273 — while drawMode is on, tools 2-4 are forced to 1. This runs
  // AFTER the cycle and catches the backward arm too, which :226-227 does not.
  if (s->tbDrawMode && s->tbTool >= FOH_TB_TOOL_WALL &&
      s->tbTool <= FOH_TB_TOOL_DAMAGE) {
    s->tbTool = FOH_TB_TOOL_PLATFORM;
  }

  s->tbHoverKind = FOH_TB_H_NONE; // :163, cleared every frame
  s->tbLedgeKind = FOH_TB_H_NONE; // :164
  switch (s->tbTool) {
    case FOH_TB_TOOL_POLYGON: tool_polygon(s, aE, bE); break;
    case FOH_TB_TOOL_PLATFORM: tool_platform(s, aE, in->a); break;
    case FOH_TB_TOOL_WALL: tool_wall(s, aE, in->a); break;
    case FOH_TB_TOOL_LEDGE: tool_ledge(s, aE); break;
    case FOH_TB_TOOL_DAMAGE: tool_damage(s, aE); break;
    case FOH_TB_TOOL_TARGET: tool_target(s, aE); break;
    case FOH_TB_TOOL_MOVE: tool_move(s, aE, in->a); break;
    case FOH_TB_TOOL_DELETE: tool_delete(s, aE); break;
    case FOH_TB_TOOL_SCALE: tool_scale(s, in); break;
    case FOH_TB_TOOL_DRAWMODE: tool_drawmode(s, aE); break;
    default: break;
  }
  (void)uE;
  (void)dE;
  g_prevX = s->tbX; // :778-779
  g_prevY = s->tbY;
  return FOH_TB_STAY;
}


// --- the renderer's snapshot ------------------------------------------------

static void view_push_list(FohTbView *out, const SurfaceList *l, int kind) {
  for (int i = 0; i < l->count; i++) {
    if (out->nLine >= FOH_TB_MAX_LINES) return;
    const int k = out->nLine++;
    out->lx0[k] = l->items[i].p0.x;
    out->ly0[k] = l->items[i].p0.y;
    out->lx1[k] = l->items[i].p1.x;
    out->ly1[k] = l->items[i].p1.y;
    out->lineKind[k] = kind;
    out->lineIdx[k] = i;
    // A props object with a NULL damageType is INERT — physics reads
    // `wall[2] !== undefined ? wall[2].damageType : null` and tests it for
    // TRUTHINESS (custom_stage.c's list_has_damage says the same), and it is
    // exactly what upstream's DAMAGE toggle leaves behind when you turn one
    // off. So only DT_STR colours a surface.
    out->lineDamage[k] = -1;
    if (l->items[i].hasProps && l->items[i].propsDamageType.tag == DT_STR) {
      for (int d = 0; d < FOH_TB_DAMAGETYPES; d++) {
        if (strcmp(l->items[i].propsDamageType.str, kDamageTypeValues[d]) == 0) {
          out->lineDamage[k] = d;
          break;
        }
      }
    }
  }
}

static void view_push_polys(FohTbView *out, const MlkPolygonList *pl, bool bg) {
  for (int i = 0; i < pl->count; i++) {
    if (out->nPoly >= FOH_TB_MAX_POLYS) return;
    if (out->nPolyPt + pl->items[i].count > FOH_TB_MAX_POLY_POINTS_V) return;
    const int p = out->nPoly++;
    out->polyStart[p] = out->nPolyPt;
    out->polyCount[p] = pl->items[i].count;
    out->polyBg[p] = bg;
    for (int k = 0; k < pl->items[i].count; k++) {
      out->polyX[out->nPolyPt] = pl->items[i].pts[k].x;
      out->polyY[out->nPolyPt] = pl->items[i].pts[k].y;
      out->nPolyPt++;
    }
  }
}

static void tb_view(const FohState *s, FohTbView *out) {
  memset(out, 0, sizeof *out);
  out->scale = g_doc.scale;
  out->toolName = tool_name(s->tbTool);
  out->typeName =
      s->tbTool == FOH_TB_TOOL_WALL
          ? kWallTypeNames[s->tbWallType]
          : (s->tbTool == FOH_TB_TOOL_DAMAGE
                 ? kDamageTypeNames[s->tbDamageType]
                 : NULL);
  out->nTarget = g_doc.targetCount;
  for (int i = 0; i < out->nTarget && i < FOH_TB_MAX_TARGETS; i++) {
    out->tx[i] = g_doc.target[i].x;
    out->ty[i] = g_doc.target[i].y;
  }
  out->nSp = g_doc.startingPointCount;
  for (int i = 0; i < out->nSp && i < FOH_TB_MAX_SP; i++) {
    out->spx[i] = g_doc.startingPoint[i].x;
    out->spy[i] = g_doc.startingPoint[i].y;
  }
  // The five collision lists plus the background line plane. Order matters
  // only for drawing; the KIND travels with each segment so the hover
  // highlight is exact.
  view_push_list(out, &g_doc.s.ground, FOH_TB_H_GROUND);
  view_push_list(out, &g_doc.s.ceiling, FOH_TB_H_CEILING);
  view_push_list(out, &g_doc.s.wallL, FOH_TB_H_WALLL);
  view_push_list(out, &g_doc.s.wallR, FOH_TB_H_WALLR);
  view_push_list(out, &g_doc.s.platform, FOH_TB_H_PLATFORM);
  view_push_list(out, &g_doc.bgLine, FOH_TB_H_LINE);
  view_push_polys(out, &g_doc.polygon, false);
  view_push_polys(out, &g_doc.bgPolygon, true);
  // ledgePos, derived the way encode.js:236 derives it: stage[list][index][side].
  for (int i = 0; i < g_doc.ledgeCount && out->nLedge < FOH_TB_MAX_LEDGES_V;
       i++) {
    const SurfaceList *l = (g_doc.ledge[i].list == 'p') ? &g_doc.s.platform
                                                        : &g_doc.s.ground;
    const int idx = (int)g_doc.ledge[i].index;
    if (idx < 0 || idx >= l->count) continue; // a stale triple draws nothing
    const Vec2D pt = (g_doc.ledge[i].point == 0.0) ? l->items[idx].p0
                                                   : l->items[idx].p1;
    out->ledgeX[out->nLedge] = pt.x;
    out->ledgeY[out->nLedge] = pt.y;
    out->nLedge++;
  }
  slots_scan(out->present, out->reason);
}


// --- the seam ---------------------------------------------------------------

static const FohTbuildOps kOps = {tb_enter, tb_step, tb_view, slots_scan};

// Constructor-installed, the tp_custom_setup / ml_sim_runai_live shape
// (foh_tbuild.h). A build that does not link this TU leaves the pointer
// NULL and refuses VISIBLY on screen.
__attribute__((constructor)) static void tb_install(void) {
  foh_tbuild_ops = &kOps;
}
