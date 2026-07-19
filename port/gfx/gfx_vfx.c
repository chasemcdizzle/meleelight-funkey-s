// port/gfx/gfx_vfx.c — vfx render plane (M4 task 2). See gfx_vfx.h.
//
// Structure-parallel to src/main/vfx/: renderVfx.js drives a queue of
// instances (timer++ per render, per-name dVfx draw, pop when
// frames < timer — newly spawned instances ARE iterated in the same pass,
// upstream's for-loop re-reads vfxQueue.length); drawVfx.js spawns
// (template deepCopy + config assign + newPos snapshot + facing = f-or--1
// — note the upstream quirk that a config-supplied `facing` is
// OVERWRITTEN by the f-derived value, so stars render with facing -1;
// carried verbatim).
//
// Canvas emulation: the dVfx bodies are written against the 2d-context
// API (translate/rotate/scale, beginPath/moveTo/lineTo/bezierCurveTo/arc,
// fill/stroke). This TU carries a small matrix-stack + path-buffer
// emulation over the rasterizer; canvas semantics that matter are kept:
//   - non-finite coordinates make the command a no-op (upstream leans on
//     this: burning/firehit spawn fireburst with pos.y = undefined -> NaN
//     -> the arc never draws; a face-less general() draw NaNs out);
//   - fill() fills every subpath accumulated since beginPath (nonzero
//     winding, open subpaths implicitly closed);
//   - fg2.lineWidth persists across frames (shieldup/fireburst/hitFlair
//     end with lineWidth 1 outside save/restore -> g->fg2LineWidth);
//   - gradients are approximated by a representative solid colour and
//     "screen" composites draw source-over (the M3 laser precedent:
//     ink-identical, colour-approximate; the IoU judges ink).
//
// Colour LITERALS in upstream dVfx CODE are carried verbatim as C
// constants; template DATA (paths/frames/colours) comes from the executed
// VFXDATA1 dump, never hand-typed.
#include "gfx_vfx.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ml_tables.h"
#include "../fdlibm/fdlibm.h" // all render trig through vendored fdlibm
#include "../sim/ml_js.h"     // js_round

#define VFX_PI 3.14159265358979323846

// ===========================================================================
// VFXDATA1: generic number-tree + template table
// ===========================================================================

typedef struct VfxNode {
  int isNum;
  double v;              // isNum
  int n;                 // list arity
  struct VfxNode **kid;  // isNum == 0
} VfxNode;

#define VN_POOL 262144
static VfxNode vn_pool[VN_POOL];
static VfxNode *vn_kidpool[VN_POOL];
static int vn_used, vn_kidused;

static VfxNode *vn_alloc(void) {
  if (vn_used >= VN_POOL) gfx_fatal("vfxdata: node pool overflow");
  return &vn_pool[vn_used++];
}

// recursive-descent bracket-stream parser: "[", "]" and String(x) numbers,
// space separated, one stream per line. A list's children are collected
// into a LOCAL buffer first and committed to the pointer pool as one
// contiguous block AFTER all of them (and their descendants) parsed —
// interleaving grandchild pool entries between a node's own kid slots
// was the iter-65 build-time bug this shape prevents.
#define VN_MAXCHILD 8192
static VfxNode *vn_parse(const char **ps) {
  const char *s = *ps;
  while (*s == ' ') s++;
  if (*s == '[') {
    s++;
    VfxNode *node = vn_alloc();
    node->isNum = 0;
    node->n = 0;
    static VfxNode *tmpStack[8][VN_MAXCHILD]; // parse depth is <= 3 measured
    static int tmpDepth;
    if (tmpDepth >= 8) gfx_fatal("vfxdata: nesting too deep");
    VfxNode **tmp = tmpStack[tmpDepth++];
    for (;;) {
      while (*s == ' ') s++;
      if (*s == ']') { s++; break; }
      if (*s == 0) gfx_fatal("vfxdata: unterminated list");
      *ps = s;
      VfxNode *k = vn_parse(ps);
      s = *ps;
      if (node->n >= VN_MAXCHILD) gfx_fatal("vfxdata: list too long");
      tmp[node->n++] = k;
    }
    tmpDepth--;
    if (vn_kidused + node->n > VN_POOL) gfx_fatal("vfxdata: kid pool overflow");
    node->kid = &vn_kidpool[vn_kidused];
    for (int i = 0; i < node->n; i++) vn_kidpool[vn_kidused++] = tmp[i];
    *ps = s;
    return node;
  }
  char *end;
  const double v = strtod(s, &end);
  if (end == s) gfx_fatal("vfxdata: expected number or bracket");
  VfxNode *node = vn_alloc();
  node->isNum = 1;
  node->v = v;
  node->kid = 0;
  node->n = 0;
  *ps = end;
  return node;
}

static const char *g_dbgDraw = "(none)"; // last draw arm, for shape-fail context

// strict accessors (loud on shape drift)
static VfxNode *vn_at(const VfxNode *n, int i) {
  if (!n || n->isNum || i < 0 || i >= n->n) {
    fprintf(stderr, "gfx_vfx: shape violation during %s (idx %d of %d)\n",
            g_dbgDraw, i, n ? n->n : -1);
    gfx_fatal("vfxdata: index out of shape");
  }
  return n->kid[i];
}
static double vn_num(const VfxNode *n) {
  if (!n || !n->isNum) gfx_fatal("vfxdata: expected number leaf");
  return n->v;
}
static double vn_num_at(const VfxNode *n, int i) { return vn_num(vn_at(n, i)); }

// --- template table ---------------------------------------------------------

typedef enum {
  V_BLASTZONE, V_BREAKSHIELD, V_BURNCIRCLE, V_BURNING, V_CEILINGBOUNCE,
  V_CIRCLEDUST, V_CLANK, V_CLIFFCATCHSPARK, V_DASHDUST, V_DOUBLEJUMPRINGS,
  V_ELECTRICHIT, V_ENTRANCE, V_FALCONPUNCH, V_FIREBURST, V_FIREFOXCHARGE,
  V_FIREFOXLAUNCH, V_FIREFOXTAIL, V_FIREHIT, V_FLYINGDUST, V_FURAFURA,
  V_GROUNDBOUNCE, V_HITCURVE, V_HITFLAIR, V_HITSPARKS, V_ILLUSION,
  V_IMPACTLAND, V_LASER, V_LASERSPARK, V_NORMALHIT, V_PHANTASM,
  V_POWERSHIELD, V_POWERSHIELDREFLECT, V_SHIELDUP, V_SHINE, V_SHINELOOP,
  V_SHOCKED, V_SING, V_SING2, V_SING3, V_STAR, V_START, V_SWING,
  V_TARGETDESTROY, V_TECH, V_WALLBOUNCE, V_COUNT
} VfxName;

static const char *const v_names[V_COUNT] = {
  "blastzoneExplosion", "breakShield", "burncircle", "burning",
  "ceilingBounce", "circleDust", "clank", "cliffcatchspark", "dashDust",
  "doubleJumpRings", "electrichit", "entrance", "falconpunch", "fireburst",
  "firefoxcharge", "firefoxlaunch", "firefoxtail", "firehit", "flyingDust",
  "furaFura", "groundBounce", "hitCurve", "hitFlair", "hitSparks",
  "illusion", "impactLand", "laser", "laserSpark", "normalhit", "phantasm",
  "powershield", "powershieldreflect", "shieldup", "shine", "shineloop",
  "shocked", "sing", "sing2", "sing3", "star", "start", "swing",
  "targetDestroy", "tech", "wallBounce"
};

#define TPL_MAXKEYS 8
typedef struct {
  int present;
  int hasFrames; double frames;
  int hasColour; double colour[3];
  int nkeys;
  char keyName[TPL_MAXKEYS][16];
  VfxNode *key[TPL_MAXKEYS];
} VfxTpl;

static VfxTpl g_tpl[V_COUNT];

#define SWORD_MAX 48 // 35 measured swordSwings types
static char g_swordName[SWORD_MAX][40]; // longest measured type is 25 chars
static VfxNode *g_sword[SWORD_MAX];
static int g_nsword;
static int g_vfx_loaded;

static int v_lookup(const char *name) {
  for (int i = 0; i < V_COUNT; i++) {
    if (strcmp(v_names[i], name) == 0) return i;
  }
  return -1;
}

static VfxNode *tpl_key(int tpl, const char *key) {
  const VfxTpl *t = &g_tpl[tpl];
  for (int k = 0; k < t->nkeys; k++) {
    if (strcmp(t->keyName[k], key) == 0) return t->key[k];
  }
  fprintf(stderr, "gfx_vfx: template %s missing key %s\n", v_names[tpl], key);
  gfx_fatal("vfxdata: missing template key");
  return 0;
}

static VfxNode *sword_lookup(const char *type) {
  for (int i = 0; i < g_nsword; i++) {
    if (strcmp(g_swordName[i], type) == 0) return g_sword[i];
  }
  fprintf(stderr, "gfx_vfx: unknown swordSwings type %s\n", type);
  gfx_fatal("vfxdata: unknown sword type");
  return 0;
}

void gfx_vfx_load(const char *path) {
  memset(g_tpl, 0, sizeof g_tpl);
  g_nsword = 0;
  vn_used = 0;
  vn_kidused = 0;
  FILE *f = fopen(path, "r");
  if (!f) gfx_fatal("vfxdata: cannot open artifact");
  char *line = NULL;
  size_t lcap = 0;
  ssize_t n;
  int cur = -1, seenEnd = 0;
  if ((n = getline(&line, &lcap, f)) <= 0 || strcmp(line, "VFXDATA1\n") != 0) {
    gfx_fatal("vfxdata: bad magic");
  }
  while ((n = getline(&line, &lcap, f)) > 0) {
    if (line[n - 1] == '\n') line[--n] = 0;
    if (n == 0) continue;
    if (strcmp(line, "END") == 0) { seenEnd = 1; break; }
    if (strncmp(line, "TPL ", 4) == 0) {
      cur = v_lookup(line + 4);
      if (cur < 0) {
        fprintf(stderr, "gfx_vfx: unknown template in dump: %s\n", line + 4);
        gfx_fatal("vfxdata: unknown template");
      }
      if (g_tpl[cur].present) gfx_fatal("vfxdata: duplicate template");
      g_tpl[cur].present = 1;
    } else if (strncmp(line, "FRAMES ", 7) == 0) {
      if (cur < 0) gfx_fatal("vfxdata: FRAMES outside TPL");
      char *end;
      g_tpl[cur].frames = strtod(line + 7, &end);
      if (*end != 0) gfx_fatal("vfxdata: bad FRAMES");
      g_tpl[cur].hasFrames = 1;
    } else if (strncmp(line, "COLOUR ", 7) == 0) {
      if (cur < 0) gfx_fatal("vfxdata: COLOUR outside TPL");
      if (sscanf(line + 7, "%lf %lf %lf", &g_tpl[cur].colour[0],
                 &g_tpl[cur].colour[1], &g_tpl[cur].colour[2]) != 3) {
        gfx_fatal("vfxdata: bad COLOUR");
      }
      g_tpl[cur].hasColour = 1;
    } else if (strncmp(line, "SKIP ", 5) == 0) {
      // measured non-array template values the draw code never indexes
      // (string colours; hitCurve.svg — upstream's own dead TODO)
    } else if (strncmp(line, "KEY ", 4) == 0) {
      if (cur < 0) gfx_fatal("vfxdata: KEY outside TPL");
      char keyname[16];
      int off = 0;
      if (sscanf(line + 4, "%15s %n", keyname, &off) != 1) {
        gfx_fatal("vfxdata: bad KEY line");
      }
      VfxTpl *t = &g_tpl[cur];
      if (t->nkeys >= TPL_MAXKEYS) gfx_fatal("vfxdata: too many keys");
      snprintf(t->keyName[t->nkeys], sizeof t->keyName[t->nkeys], "%s", keyname);
      const char *s = line + 4 + off;
      t->key[t->nkeys] = vn_parse(&s);
      while (*s == ' ') s++;
      if (*s != 0) gfx_fatal("vfxdata: trailing bytes on KEY line");
      t->nkeys++;
    } else if (strncmp(line, "SWORD ", 6) == 0) {
      char type[40];
      int off = 0;
      if (sscanf(line + 6, "%39s %n", type, &off) != 1) {
        gfx_fatal("vfxdata: bad SWORD line");
      }
      if (g_nsword >= SWORD_MAX) gfx_fatal("vfxdata: sword overflow");
      snprintf(g_swordName[g_nsword], sizeof g_swordName[g_nsword], "%s", type);
      const char *s = line + 6 + off;
      g_sword[g_nsword] = vn_parse(&s);
      while (*s == ' ') s++;
      if (*s != 0) gfx_fatal("vfxdata: trailing bytes on SWORD line");
      g_nsword++;
    } else {
      fprintf(stderr, "gfx_vfx: unknown line: %s\n", line);
      gfx_fatal("vfxdata: unknown line");
    }
  }
  free(line);
  fclose(f);
  if (!seenEnd) gfx_fatal("vfxdata: missing END");
  for (int i = 0; i < V_COUNT; i++) {
    if (!g_tpl[i].present) {
      fprintf(stderr, "gfx_vfx: template missing from dump: %s\n", v_names[i]);
      gfx_fatal("vfxdata: incomplete template table");
    }
    if (!g_tpl[i].hasFrames) {
      // post-alias every template carries frames (vfx.js gives wallBounce/
      // ceilingBounce groundBounce's); a missing one is dump drift.
      fprintf(stderr, "gfx_vfx: template without frames: %s\n", v_names[i]);
      gfx_fatal("vfxdata: template without frames");
    }
  }
  if (g_nsword == 0) gfx_fatal("vfxdata: no swordSwings");
  g_vfx_loaded = 1;
}

// ===========================================================================
// canvas emulation (matrix stack + path buffer over the rasterizer)
// ===========================================================================

static Gfx *g_g;              // bound compositor (install/render passes)
static const GameState *g_st; // live state during the render pass
static MlRng g_rrng;          // render-LOCAL mulberry32 (never the chain)

static double rrand(void) { return ml_rng_next(&g_rrng); }

typedef struct { double a, b, c, d, e, f; } VMat; // x'=ax+cy+e, y'=bx+dy+f

#define VM_STACK 16
static VMat vm_stack[VM_STACK];
static int vm_top;

static void vm_reset(void) {
  vm_top = 0;
  vm_stack[0] = (VMat){1, 0, 0, 1, 0, 0};
}
static void vm_save(void) {
  if (vm_top + 1 >= VM_STACK) gfx_fatal("gfx_vfx: matrix stack overflow");
  vm_stack[vm_top + 1] = vm_stack[vm_top];
  vm_top++;
}
static void vm_restore(void) {
  if (vm_top == 0) gfx_fatal("gfx_vfx: matrix stack underflow");
  vm_top--;
}
static void vm_mul(double a, double b, double c, double d, double e, double f) {
  VMat *m = &vm_stack[vm_top];
  const VMat o = *m;
  m->a = o.a * a + o.c * b;
  m->b = o.b * a + o.d * b;
  m->c = o.a * c + o.c * d;
  m->d = o.b * c + o.d * d;
  m->e = o.a * e + o.c * f + o.e;
  m->f = o.b * e + o.d * f + o.f;
}
static void vm_translate(double tx, double ty) { vm_mul(1, 0, 0, 1, tx, ty); }
static void vm_rotate(double r) {
  const double cr = fd_cos(r), sr = fd_sin(r);
  vm_mul(cr, sr, -sr, cr, 0, 0);
}
static void vm_scale(double sx, double sy) { vm_mul(sx, 0, 0, sy, 0, 0); }
static double vm_sfac(void) {
  const VMat *m = &vm_stack[vm_top];
  const double det = fabs(m->a * m->d - m->b * m->c);
  return sqrt(det);
}

// canvas pt -> device pt (retarget k=0.2 dy=45 folded here)
static void vm_dev(double x, double y, float *ox, float *oy) {
  const VMat *m = &vm_stack[vm_top];
  const double cx = m->a * x + m->c * y + m->e;
  const double cy = m->b * x + m->d * y + m->f;
  *ox = (float)(cx * GFX_K);
  *oy = (float)(cy * GFX_K + GFX_DY);
}

// --- path buffer -------------------------------------------------------------

#define VP_MAXPTS 4096
#define VP_MAXSUB 128
static float vp_x[VP_MAXPTS], vp_y[VP_MAXPTS];
static int vp_n;
static int vp_subStart[VP_MAXSUB];
static int vp_nsub;
static int vp_open; // a subpath is open

static void vp_begin(void) { vp_n = 0; vp_nsub = 0; vp_open = 0; }

static void vp_push(float x, float y) {
  if (vp_n >= VP_MAXPTS) gfx_fatal("gfx_vfx: path point overflow");
  vp_x[vp_n] = x;
  vp_y[vp_n] = y;
  vp_n++;
}

static void vp_moveTo(double x, double y) {
  if (!isfinite(x) || !isfinite(y)) return; // canvas: non-finite -> no-op
  if (vp_nsub >= VP_MAXSUB) gfx_fatal("gfx_vfx: subpath overflow");
  vp_subStart[vp_nsub++] = vp_n;
  vp_open = 1;
  float dx, dy;
  vm_dev(x, y, &dx, &dy);
  vp_push(dx, dy);
}

static void vp_lineTo(double x, double y) {
  if (!isfinite(x) || !isfinite(y)) return;
  if (!vp_open) { vp_moveTo(x, y); return; } // canvas: lineTo with no sub
  float dx, dy;
  vm_dev(x, y, &dx, &dy);
  vp_push(dx, dy);
}

static void vp_bezierTo(double x1, double y1, double x2, double y2,
                        double x3, double y3) {
  if (!isfinite(x1) || !isfinite(y1) || !isfinite(x2) || !isfinite(y2) ||
      !isfinite(x3) || !isfinite(y3)) {
    return;
  }
  if (!vp_open || vp_n == 0) { vp_moveTo(x3, y3); return; }
  float c1x, c1y, c2x, c2y, ex, ey;
  vm_dev(x1, y1, &c1x, &c1y);
  vm_dev(x2, y2, &c2x, &c2y);
  vm_dev(x3, y3, &ex, &ey);
  const float sx = vp_x[vp_n - 1], sy = vp_y[vp_n - 1];
  const int SEG = 12;
  for (int i = 1; i <= SEG; i++) {
    const float t = (float)i / SEG, mt = 1.0f - t;
    const float a = mt * mt * mt, b = 3 * mt * mt * t, c = 3 * mt * t * t,
                d = t * t * t;
    vp_push(a * sx + b * c1x + c * c2x + d * ex,
            a * sy + b * c1y + c * c2y + d * ey);
  }
}

// canvas arc: line from current point to arc start (when a subpath is
// open), then the sweep; |a1-a0| >= 2pi means the full circle.
static void vp_arc(double cx, double cy, double r, double a0, double a1) {
  if (!isfinite(cx) || !isfinite(cy) || !isfinite(r) || !isfinite(a0) ||
      !isfinite(a1)) {
    return;
  }
  if (r < 0) r = -r; // canvas throws; upstream never passes negative here
  double sweep = a1 - a0;
  if (sweep >= 2 * VFX_PI || sweep <= -2 * VFX_PI) sweep = 2 * VFX_PI;
  else if (sweep < 0) sweep += 2 * VFX_PI; // default clockwise sweep
  const int SEG = 32;
  for (int i = 0; i <= SEG; i++) {
    const double t = a0 + sweep * ((double)i / SEG);
    const double px = cx + r * fd_cos(t), py = cy + r * fd_sin(t);
    if (i == 0) {
      if (vp_open && vp_n > 0) vp_lineTo(px, py);
      else vp_moveTo(px, py);
    } else {
      vp_lineTo(px, py);
    }
  }
}

static void vp_closePath(void) {
  // geometric closing is implicit in the rasterizer / stroke loop; track
  // nothing (canvas closePath affects stroking of the closing segment,
  // which the stroke helper always draws for closed==1 callers).
}

static void vp_fill(RastCol col) {
  if (col.a256 == 0) return;
  rast_path_reset();
  int emitted = 0;
  for (int s = 0; s < vp_nsub; s++) {
    const int start = vp_subStart[s];
    const int end = (s + 1 < vp_nsub) ? vp_subStart[s + 1] : vp_n;
    if (end - start < 2) continue;
    rast_sub_begin(vp_x[start], vp_y[start]);
    for (int i = start + 1; i < end; i++) rast_sub_line(vp_x[i], vp_y[i]);
    rast_sub_close();
    emitted = 1;
  }
  if (emitted) rast_fill(&g_g->rz, col);
}

// stroke every subpath as butt-capped segment quads; closed joins the
// last point back to the first (canvas closePath callers).
static void vp_stroke(double lwCanvas, RastCol col, int closed) {
  if (col.a256 == 0) return;
  const float w = (float)(lwCanvas * vm_sfac() * GFX_K);
  if (w <= 0) return;
  for (int s = 0; s < vp_nsub; s++) {
    const int start = vp_subStart[s];
    const int end = (s + 1 < vp_nsub) ? vp_subStart[s + 1] : vp_n;
    for (int i = start; i + 1 < end; i++) {
      rast_stroke_seg(&g_g->rz, vp_x[i], vp_y[i], vp_x[i + 1], vp_y[i + 1], w, col);
    }
    if (closed && end - start > 2) {
      rast_stroke_seg(&g_g->rz, vp_x[end - 1], vp_y[end - 1],
                      vp_x[start], vp_y[start], w, col);
    }
  }
}

// axis-aligned fillRect through the current matrix
static void vfx_fillRect(double x, double y, double w, double h, RastCol col) {
  if (!isfinite(x) || !isfinite(y) || !isfinite(w) || !isfinite(h)) return;
  vp_begin();
  vp_moveTo(x, y);
  vp_lineTo(x + w, y);
  vp_lineTo(x + w, y + h);
  vp_lineTo(x, y + h);
  vp_fill(col);
}

static RastCol vcol(double r, double g, double b, double a) {
  if (a < 0) a = 0;
  if (a > 1) a = 1;
  if (r < 0) r = 0;
  if (r > 255) r = 255;
  if (g < 0) g = 0;
  if (g > 255) g = 255;
  if (b < 0) b = 0;
  if (b > 255) b = 255;
  RastCol c = { (uint8_t)r, (uint8_t)g, (uint8_t)b, (uint16_t)(a * 256.0 + 0.5) };
  return c;
}

// ===========================================================================
// the queue
// ===========================================================================

typedef struct {
  const char *name;
  double px, py;
  int hasFace; double face;
  int faceIsVec; double faceX, faceY;
  int fKind; // 0 none, 1 num, 2 vec, 3 swing
  double fNum, fX, fY;
  double fPnum; const char *fSwing; double fFrame;
  int hasColor; double c1[3], c2[3];
  int hasDust; double dust[4];
  int hasDir; double dirX, dirY, offset;
} VSpawn;

typedef struct {
  int tpl;
  double timer, frames;
  double px, py;              // newPos
  double face;                // NaN == undefined (JS semantics)
  int faceIsVec; double faceX, faceY;
  int facingKind;             // 0 num, 1 vec, 2 swing
  double facing;              // facingKind 0 (f or -1)
  double facingX, facingY;    // facingKind 1
  int swingPnum; char swingType[40]; double swingFrame; // facingKind 2
  int swingSnap; double posNowX, posNowY, posPrevX, posPrevY;
  int hasC1; double c1[3];
  int hasC2; double c2[3];
  int hasCircles; double circles[4];
  int hasTail; double tail[4];
  int hasDir; double dirX, dirY, offset;
  int startReady, startGo;    // dVfx/start.js sound-flag hack (face / [5])
} VInst;

#define VQ_CAP 256
static VInst g_q[VQ_CAP];
static int g_qn;

static void vspawn(const VSpawn *c) {
  const int tpl = v_lookup(c->name);
  if (tpl < 0) {
    fprintf(stderr, "gfx_vfx: spawn of unknown vfx '%s'\n", c->name);
    gfx_fatal("gfx_vfx: unknown vfx name");
  }
  if (!g_vfx_loaded) gfx_fatal("gfx_vfx: spawn before gfx_vfx_load");
  if (g_qn >= VQ_CAP) gfx_fatal("gfx_vfx: vfx queue overflow");
  VInst *v = &g_q[g_qn++];
  memset(v, 0, sizeof *v);
  v->tpl = tpl;
  v->timer = 0;
  v->frames = g_tpl[tpl].frames;
  v->px = c->px;
  v->py = c->py;
  v->face = c->hasFace ? c->face : NAN;
  v->faceIsVec = c->faceIsVec;
  v->faceX = c->faceX;
  v->faceY = c->faceY;
  switch (c->fKind) {
    case 1: v->facingKind = 0; v->facing = c->fNum; break;
    case 2: v->facingKind = 1; v->facingX = c->fX; v->facingY = c->fY; break;
    case 3:
      v->facingKind = 2;
      v->swingPnum = (int)c->fPnum;
      snprintf(v->swingType, sizeof v->swingType, "%s", c->fSwing);
      v->swingFrame = c->fFrame;
      break;
    default: v->facingKind = 0; v->facing = -1; break; // f absent
  }
  if (c->hasColor) {
    v->hasC1 = v->hasC2 = 1;
    memcpy(v->c1, c->c1, sizeof v->c1);
    memcpy(v->c2, c->c2, sizeof v->c2);
  }
  if (c->hasDir) {
    v->hasDir = 1;
    v->dirX = c->dirX;
    v->dirY = c->dirY;
    v->offset = c->offset;
  }
  if (tpl == V_CIRCLEDUST) {
    // drawVfx.js:15-18 — circles from the 4 SEEDED draws (values ride the
    // event as raw u01; the draws themselves were burned by
    // ml_drawVfx_cfg, bit-exact with the browser).
    if (!c->hasDust) gfx_fatal("gfx_vfx: circleDust event without dust draws");
    const double S = g_g->scale;
    v->hasCircles = 1;
    v->circles[0] = c->dust[0] * -2;
    v->circles[1] = (c->dust[1] * -S) - 2;
    v->circles[2] = c->dust[2] * 2;
    v->circles[3] = (c->dust[3] * S) + 2;
  }
}

void gfx_vfx_spawn(const MlVfx *cfg) {
  VSpawn c;
  memset(&c, 0, sizeof c);
  c.name = cfg->name;
  c.px = cfg->px;
  c.py = cfg->py;
  c.hasFace = cfg->has_face;
  c.face = cfg->face;
  switch (cfg->f_kind) {
    case ML_VFX_F_NUM: c.fKind = 1; c.fNum = cfg->f_num; break;
    case ML_VFX_F_VEC: c.fKind = 2; c.fX = cfg->f_x; c.fY = cfg->f_y; break;
    case ML_VFX_F_SWING:
      c.fKind = 3;
      c.fPnum = cfg->f_pnum;
      c.fSwing = cfg->f_swing;
      c.fFrame = cfg->f_frame;
      break;
    default: c.fKind = 0; break;
  }
  if (cfg->has_color) {
    c.hasColor = 1;
    c.c1[0] = cfg->c1r; c.c1[1] = cfg->c1g; c.c1[2] = cfg->c1b;
    c.c2[0] = cfg->c2r; c.c2[1] = cfg->c2g; c.c2[2] = cfg->c2b;
  }
  if (cfg->has_dust) {
    c.hasDust = 1;
    memcpy(c.dust, cfg->dust, sizeof c.dust);
  }
  vspawn(&c);
}

// ===========================================================================
// render-plane spawn helpers (stars.js / lines.js)
// ===========================================================================

// stars.js: n annulus-scattered "star" instances at CANVAS coords tX/tY.
static void vfx_stars(double tX, double tY, double n, double minSpread,
                      double maxSpread) {
  for (int i = 0; i < (int)n; i++) {
    // randomAnnulusPoint(0, 0, rMin, rMax)
    const double t = rrand() * 2 * VFX_PI;
    const double r = (maxSpread - minSpread) * sqrt(rrand()) + minSpread;
    const double dx = r * fd_cos(t), dy = r * fd_sin(t);
    VSpawn c;
    memset(&c, 0, sizeof c);
    c.name = "star";
    c.px = tX;
    c.py = tY;
    // face : [deltaX, deltaY]; facing in the config is OVERWRITTEN by
    // drawVfx (f absent -> -1) — the upstream quirk, carried.
    c.hasFace = 1;
    c.face = NAN;
    c.faceIsVec = 1;
    c.faceX = dx;
    c.faceY = dy;
    (void)(0.4 + 0.8 * rrand()); // stars.js draws for its dead `facing`
    vspawn(&c);
  }
}

static double pow_with_sign(double x, double d) {
  return x < 0 ? -pow(-x, d) : pow(x, d);
}

// lines.js: n "laserSpark" instances fanned over [minAngle, maxAngle].
static void vfx_lines_laserSpark(const double col[3], double px, double py,
                                 double n, double minAngle, double maxAngle,
                                 double cpow) {
  for (int i = 0; i < (int)n; i++) {
    const double theta =
        cpow == 1 ? rrand() * (maxAngle - minAngle) + minAngle
                  : pow_with_sign(rrand() * 2 - 1, cpow) * 0.5 *
                            (maxAngle - minAngle) +
                        0.5 * (minAngle + maxAngle);
    const double offset = rrand();
    VSpawn c;
    memset(&c, 0, sizeof c);
    c.name = "laserSpark";
    c.px = px;
    c.py = py;
    c.hasFace = 1;
    c.face = 0;
    c.hasColor = 1;
    memcpy(c.c1, col, 3 * sizeof(double));
    c.hasDir = 1;
    c.dirX = fd_cos(theta);
    c.dirY = fd_sin(theta);
    c.offset = offset;
    vspawn(&c);
  }
}

// in-draw drawVfx re-spawns (burning/firehit): configs carry `face`
// (the burst radius scale); burning/firehit's fireburst pos.y is
// upstream's own `vfxQueue[posInQueue].y` — UNDEFINED -> NaN, so that
// fireburst never draws (carried verbatim via NaN propagation).
static void vfx_respawn(const char *name, double px, double py, double face) {
  VSpawn c;
  memset(&c, 0, sizeof c);
  c.name = name;
  c.px = px;
  c.py = py;
  c.hasFace = 1;
  c.face = face;
  vspawn(&c);
}

// ===========================================================================
// dVfx draw fns
// ===========================================================================

// drawArrayPath(col, face, tX, tY, path[rows][2], sX, sY) — one fill
static void draw_array_path(RastCol col, double face, double tX, double tY,
                            const VfxNode *path, double sX, double sY) {
  vp_begin();
  for (int j = 0; j < path->n; j++) {
    const VfxNode *pt = vn_at(path, j);
    const double x = vn_num_at(pt, 0) * sX * face + tX;
    const double y = vn_num_at(pt, 1) * sY + tY;
    if (j == 0) vp_moveTo(x, y);
    else vp_lineTo(x, y);
  }
  vp_closePath();
  vp_fill(col);
}

// drawArrayPathNew: rows of arity 2 (moveTo) or 6 (bezier), translated /
// rotated about a rotation point.
static void draw_array_path_new(RastCol col, double face, double tX, double tY,
                                const VfxNode *path, double sX, double sY,
                                double rotate, double rpX, double rpY) {
  vm_save();
  vm_translate(tX - rpX, tY - rpY);
  vm_rotate(rotate);
  vp_begin();
  for (int j = 0; j < path->n; j++) {
    const VfxNode *pt = vn_at(path, j);
    const double x = vn_num_at(pt, 0) * sX * face + rpX;
    const double y = vn_num_at(pt, 1) * sY + rpY;
    if (j == 0) {
      vp_moveTo(x, y);
    } else if (pt->n == 2) {
      vp_moveTo(x, y);
    } else {
      vp_bezierTo(x, y, vn_num_at(pt, 2) * sX * face + rpX,
                  vn_num_at(pt, 3) * sY + rpY,
                  vn_num_at(pt, 4) * sX * face + rpX,
                  vn_num_at(pt, 5) * sY + rpY);
    }
  }
  vp_closePath();
  vp_fill(col);
  vm_restore();
}

// drawArrayPathCompress (render.js): rows are flat [x, y, 6-groups...]
static void draw_array_path_compress(RastCol col, double face, double tX,
                                     double tY, const VfxNode *path, double sX,
                                     double sY, double rotate, double rpX,
                                     double rpY) {
  vm_save();
  vm_translate(tX - rpX, tY - rpY);
  vm_rotate(rotate);
  vp_begin();
  for (int j = 0; j < path->n; j++) {
    const VfxNode *row = vn_at(path, j);
    vp_moveTo(vn_num_at(row, 0) * sX * face + rpX,
              vn_num_at(row, 1) * sY + rpY);
    for (int k = 2; k + 5 < row->n; k += 6) {
      vp_bezierTo(vn_num_at(row, k) * sX * face + rpX,
                  vn_num_at(row, k + 1) * sY + rpY,
                  vn_num_at(row, k + 2) * sX * face + rpX,
                  vn_num_at(row, k + 3) * sY + rpY,
                  vn_num_at(row, k + 4) * sX * face + rpX,
                  vn_num_at(row, k + 5) * sY + rpY);
    }
  }
  vp_closePath();
  vp_fill(col);
  vm_restore();
}

// drawStar (drawStar.js): 2m-point star polygon at the current transform.
static void draw_star(double tX, double tY, double rMin, double rMax,
                      RastCol col) {
  const int m = 4, n = 2 * m;
  vm_save();
  vm_translate(tX, tY);
  vp_begin();
  vp_moveTo(rMax, 0); // theta = 0
  for (int i = 1; i < n + 1; i++) {
    const double r = (i % 2 == 0) ? rMax : rMin;
    const double a = 2 * VFX_PI * i / n;
    vp_lineTo(r * fd_cos(a), r * fd_sin(a));
  }
  vp_closePath();
  vp_fill(col);
  vm_restore();
}

// drawHexagon (drawHexagon.js): hexagonal ring (outer + reversed inner).
static void draw_hexagon(double r, double tX, double tY, double width,
                         RastCol col) {
  vm_save();
  vm_translate(tX, tY);
  double a = r * fd_sin(VFX_PI / 6), b = r * fd_cos(VFX_PI / 6);
  vp_begin();
  vp_moveTo(0, r);
  vp_lineTo(b, r - a);
  vp_lineTo(b, -r + a);
  vp_lineTo(0, -r);
  vp_lineTo(-b, -r + a);
  vp_lineTo(-b, r - a);
  vp_lineTo(0, r);
  const double rs = r - width;
  a = rs * fd_sin(VFX_PI / 6);
  b = rs * fd_cos(VFX_PI / 6);
  vp_moveTo(0, rs);
  vp_lineTo(-b, rs - a);
  vp_lineTo(-b, -rs + a);
  vp_lineTo(0, -rs);
  vp_lineTo(b, -rs + a);
  vp_lineTo(b, rs - a);
  vp_lineTo(0, rs);
  vp_closePath();
  vp_fill(col);
  vm_restore();
}

// dVfx/general.js
static void dv_general(VInst *v, double ang) {
  const VfxTpl *t = &g_tpl[v->tpl];
  if (!t->hasColour) gfx_fatal("gfx_vfx: general() template without colour");
  const double S = g_g->scale;
  const RastCol col = vcol(t->colour[0], t->colour[1], t->colour[2],
                           0.8 * ((v->frames - v->timer) / v->frames));
  vm_save();
  vm_translate(v->px * S + g_g->offx, v->py * -S + g_g->offy);
  vm_rotate(ang);
  const VfxNode *frames = tpl_key(v->tpl, "path");
  const int idx = (int)v->timer - 1;
  if (idx < 0 || idx >= frames->n) gfx_fatal("gfx_vfx: general path frame range");
  draw_array_path(col, v->face, 0, 0, vn_at(frames, idx),
                  0.2 * (S / 4.5), 0.2 * (S / 4.5));
  vm_restore();
}

static const MlPlayer *live_player(int p) {
  if (p < 0 || p > 3) gfx_fatal("gfx_vfx: live player slot range");
  return &g_st->sim.player[p];
}

static int live_char(int p) {
  const int c = (int)g_st->sim.characterSelections[p];
  if (c < 0 || c >= GFX_CHARS) gfx_fatal("gfx_vfx: live char range");
  return c;
}

// dVfx/singGen.js
static void dv_sing_gen(VInst *v, double rMin, double rMax, double notePhase,
                        double posScale, double posPhase) {
  const double S = g_g->scale, OX = g_g->offx, OY = g_g->offy;
  const int p = (int)v->face; // sing configs carry face = slot
  if (!isfinite(v->face)) return;
  const MlPlayer *pl = live_player(p);
  const double frame = v->timer;
  const double posX = pl->phys.pos.x, posY = pl->phys.pos.y + 8;
  const double lrScaling = posScale * pl->phys.face;
  vm_save();
  vm_translate(((posX - 0.8) * S) + OX + lrScaling * fd_cos(frame / 6.5 + posPhase),
               (posY * -S) + OY - 2.5 * fd_sin(frame / 8));
  const double opaqMultiplier = 0.8;
  double opaq = opaqMultiplier;
  if (frame < 6) opaq = opaqMultiplier * frame / 6;
  else if (frame > 25) opaq = opaqMultiplier * (1 - ((frame - 25) / 6));
  // 5 rings
  for (int i = 0; i < 5; i++) {
    vp_begin();
    vp_arc(0, 0, (i + 1) * 1.6 * S, 0, 2 * VFX_PI);
    vp_stroke(3, vcol(244, 212, 45, opaq), 1);
  }
  // 2 orbiting dots
  for (int sgn = -1; sgn <= 1; sgn += 2) {
    vp_begin();
    vp_arc(sgn * -4.5 * S * fd_sin(frame * 0.07 + 0.2),
           sgn * -4.5 * S * fd_cos(frame * 0.07 + 0.2), 3.5 * S, 0, 2 * VFX_PI);
    vp_fill(vcol(191, 82, 146, opaq));
  }
  // 3 notes
  const double angles[3] = { notePhase + frame * 0.1,
                             notePhase + 2 * VFX_PI / 3 + frame * 0.1,
                             notePhase + 4 * VFX_PI / 3 + frame * 0.1 };
  double r = rMax;
  if (frame < 15) r = rMin + frame * (rMax - rMin) / 15;
  opaq += 0.2;
  const double noteCols[3][3] = { {255, 1, 2}, {5, 255, 0}, {12, 0, 255} };
  const VfxNode *singPath = tpl_key(V_SING, "path");
  for (int i = 0; i < 3; i++) {
    draw_array_path_new(vcol(noteCols[i][0], noteCols[i][1], noteCols[i][2], opaq),
                        1, ((r * fd_cos(angles[i]) - 3) * S),
                        ((r * fd_sin(angles[i]) + 3) * -S), singPath,
                        0.7 * (S / 4.5), 0.7 * (S / 4.5), 0, 0, 0);
  }
  vm_restore();
}

// dVfx/start.js — the Ready--Go! banner. Text via the executed glyph
// atlas (composite sprites + c70 digits); sound plays are the M4 mixer's
// surface (task 6), tracked with the same flags upstream uses.
static void dv_start(VInst *v) {
  if (!v->startReady) v->startReady = 1;          // sounds.ready.play()
  if (v->timer >= 90 && !v->startGo) v->startGo = 1; // sounds.go.play()
  if (v->timer < 90) {
    gfx_sprite_blit(g_g, "ready", 240, 420);
    // countdown (italic 700 70px): floor(startTimer*2) + " " + milli[2..3]
    const double st2 = g_st->startTimer * 2;
    const double milli = fmod(st2, 1.0);
    char mstr[8];
    snprintf(mstr, sizeof mstr, "%.2f", milli); // "0.xy"
    char text[16];
    snprintf(text, sizeof text, "%d %c%c", (int)floor(st2), mstr[2], mstr[3]);
    const RastCol fillc = vcol(js_round(v->timer * 2.6),
                               js_round(140 - (v->timer * 1.5)),
                               js_round(255 - (v->timer * 2.6)), 1.0);
    gfx_glyph_text(g_g, GFX_FONT_C70, text, 900 * GFX_K, 500 * GFX_K + GFX_DY,
                   fillc, vcol(0, 0, 0, 1.0), 1);
    // progress bar
    vm_save();
    vfx_fillRect(240, 450, 520, 15, vcol(255, 0, 0, 0.2));
    // rainbow gradient band -> representative solid
    vfx_fillRect(240 + (500 * (v->timer / 90)), 450,
                 520 - (500 * (v->timer / 90)), 15, vcol(0, 255, 255, 1.0));
    vm_restore();
  } else {
    gfx_sprite_blit(g_g, "go", 240, 470);
  }
}

// the shared "burst diamond + circle" shape (normalhit/firehit/electrichit
// case-1 arms; drawn at the current transform origin)
static void dv_burst_shape(RastCol col) {
  vp_begin();
  vp_arc(0, 0, 20, 0, 2 * VFX_PI);
  vp_fill(col);
  vp_begin();
  vp_moveTo(0, 30);
  vp_lineTo(5, 5);
  vp_lineTo(30, 0);
  vp_lineTo(5, -5);
  vp_lineTo(0, -30);
  vp_lineTo(-5, -5);
  vp_lineTo(-30, 0);
  vp_lineTo(-5, 5);
  vp_closePath();
  vp_fill(col);
}

static void dv_draw(VInst *v);

// renderVfx.js — the per-frame pass.
void gfx_render_vfx(Gfx *g, const GameState *st) {
  g_g = g;
  g_st = st;
  vm_reset();
  int npop = 0;
  int pop[VQ_CAP];
  // upstream: for (posInQueue < vfxQueue.length) — length re-read, so
  // same-pass render spawns (stars/lines/fireburst) are iterated too.
  for (int i = 0; i < g_qn; i++) {
    g_q[i].timer++;
    if (g_q[i].frames >= g_q[i].timer) {
      dv_draw(&g_q[i]);
    } else {
      if (npop >= VQ_CAP) gfx_fatal("gfx_vfx: pop overflow");
      pop[npop++] = i;
    }
  }
  // dropFromVfxQueue(pop[k]-k, 1) == stable compaction of the survivors
  if (npop > 0) {
    int w = 0, pi = 0;
    for (int i = 0; i < g_qn; i++) {
      if (pi < npop && pop[pi] == i) { pi++; continue; }
      if (w != i) g_q[w] = g_q[i];
      w++;
    }
    g_qn = w;
  }
}

static void dv_draw(VInst *v) {
  Gfx *g = g_g;
  g_dbgDraw = v_names[v->tpl];
  const double S = g->scale, OX = g->offx, OY = g->offy;
  const double s45 = S / 4.5;
  const double t = v->timer, fr = v->frames;
  const double cX = v->px * S + OX;   // the ubiquitous newPos canvas map
  const double cY = v->py * -S + OY;
  switch (v->tpl) {
    case V_IMPACTLAND:
    case V_DASHDUST:
      dv_general(v, 0);
      break;
    case V_GROUNDBOUNCE:
      dv_general(v, VFX_PI / 2 - v->facing);
      break;
    case V_WALLBOUNCE:
    case V_CEILINGBOUNCE:
      if (v->facingKind != 1) gfx_fatal("gfx_vfx: bounce without normal vec");
      dv_general(v, -fd_atan2(v->facingY, v->facingX) + VFX_PI / 2);
      break;
    case V_FLYINGDUST:
      vp_begin();
      vp_arc(cX, cY, 12 * s45, 0, 2 * VFX_PI);
      vp_fill(vcol(255, 255, 255, 0.7 * ((fr - t) / fr)));
      break;
    case V_FURAFURA:
      vp_begin();
      vp_arc(cX, cY, 5 * s45, 0, 2 * VFX_PI);
      vp_fill(vcol(255, 254, 108, 0.9 * ((fr - t) / fr)));
      break;
    case V_CLANK:
      vp_begin();
      vp_arc(cX, cY, (12 * (t / fr) + 3) * s45, 0, 2 * VFX_PI);
      vp_stroke(10 - (t / 3), vcol(47, 214, 114, 1.0), 1);
      break;
    case V_CLIFFCATCHSPARK:
      vp_begin();
      vp_arc(cX, cY, (12 * (t / fr) + 3) * s45, 0, 2 * VFX_PI);
      vp_stroke(10 - (t / 3), vcol(47, 194, 214, 1.0), 1);
      break;
    case V_FIREBURST:
      // radius face*(t/5); the burning/firehit spawns carry pos.y = NaN
      // (upstream .y-vs-newPos.y quirk) -> vp_arc no-ops, like the browser.
      vp_begin();
      vp_arc(cX, cY, v->face * (t / 5), 0, 2 * VFX_PI);
      vp_stroke(1, vcol(255, 227, 79, 1 - (t / 5)), 1);
      g->fg2LineWidth = 1.0; // fireburst sets lineWidth outside save/restore
      break;
    case V_STAR: {
      // star newPos is already CANVAS-space (stars.js passes canvas tX/tY)
      const double tt = t / fr;
      if (!v->faceIsVec) break; // spawn always render-plane with vec face
      const double x = v->px + v->faceX * (0.9 + 0.35 * tt);
      const double y = v->py + v->faceY * (0.9 + 0.35 * tt) + 0.8 * S * tt * tt;
      const double scale = v->facing; // -1: the drawVfx facing overwrite quirk
      draw_star(x, y, scale * 0.3 * S, scale * 1.1 * S,
                vcol(196, 252, 254, 3 - 4 * tt));
      break;
    }
    case V_BURNCIRCLE: {
      double col[3];
      const double bstart[3] = {253, 255, 161}, bend[3] = {198, 57, 5};
      for (int i = 0; i < 3; i++) {
        col[i] = floor(bstart[i] + (bend[i] - bstart[i]) * (t / 9));
      }
      vp_begin();
      vp_arc(cX, ((v->py + t) * -S) + OY, 3 * S, 0, 2 * VFX_PI);
      vp_fill(vcol(col[0], col[1], col[2], 1 - t / 9));
      break;
    }
    case V_CIRCLEDUST: {
      if (!v->hasCircles) gfx_fatal("gfx_vfx: circleDust without circles");
      for (int n = 0; n < 4; n++) {
        const double x = ((v->px + (v->circles[n] * (1 + (t / fr)))) * S) + OX;
        const double y = ((v->py + (4 * (0 + (t / fr)))) * -S) + OY;
        vp_begin();
        vp_arc(x, y, 12 * s45, 0, 2 * VFX_PI);
        vp_fill(vcol(255, 255, 255, 0.7 * ((fr - t) / fr)));
      }
      break;
    }
    case V_FIREFOXCHARGE: {
      vm_save();
      vm_translate(cX, cY);
      const VfxNode *paths = tpl_key(V_FIREFOXCHARGE, "path");
      const int facing = (int)v->facing;
      if (facing < 0 || facing >= paths->n) {
        gfx_fatal("gfx_vfx: firefoxcharge facing frame range");
      }
      const int secondFrame = (facing + 4) % 10;
      draw_array_path_new(vcol(237, 219, 53, 0.3), v->face, 0, 0,
                          vn_at(paths, secondFrame), 0.35 * s45, 0.5 * s45,
                          0, 0, 0);
      draw_array_path_new(vcol(255, 218, 163, 1.0), v->face, 0, 0,
                          vn_at(paths, facing), 0.35 * s45, 0.5 * s45, 0, 0, 0);
      vm_restore();
      break;
    }
    case V_BREAKSHIELD:
      vp_begin();
      vp_arc(cX, v->py * -S + 430, (10 + (3 * t)) * s45, 0, 2 * VFX_PI);
      vp_fill(vcol(73, 255, 244, 0.9 * ((fr - t) / fr)));
      for (int k = 0; k < 3; k++) {
        vp_begin();
        vp_arc((v->px * S) + 550 + rrand() * 100,
               (v->py * -S) + 380 + rrand() * 100, 8 * s45, 0, 2 * VFX_PI);
        vp_fill(vcol(0xcd, 0x8e, 0xff, 1.0));
      }
      break;
    case V_DOUBLEJUMPRINGS: {
      const VfxNode *rings = tpl_key(V_DOUBLEJUMPRINGS, "rings");
      const RastCol col = vcol(99, 100, 255, 0.7 * ((fr - t) / fr));
      for (int n = 0; n < rings->n; n++) {
        vm_save();
        vm_scale(1, 0.25);
        vp_begin();
        vp_arc(cX, ((v->py * -S) + OY) * 4, t * (40.0 / 8) + n * S, 0,
               2 * VFX_PI);
        vp_stroke(3, col, 1);
        vm_restore();
      }
      break;
    }
    case V_SHIELDUP:
      vp_begin();
      vp_arc(cX, cY, v->facing * S + 10 + (5 * (t - 1)), 0, 2 * VFX_PI);
      vp_stroke(10, vcol(255, 255, 255, 0.8 * ((fr - t) / fr)), 1);
      vp_begin();
      vp_arc(cX, cY, v->facing * S + (5 * (t - 1)), 0, 2 * VFX_PI);
      vp_stroke(5, vcol(255, 255, 255, 0.8 * ((fr - t) / fr)), 1);
      g->fg2LineWidth = 1.0; // shieldup ends with lineWidth = 1 (no save)
      break;
    case V_HITSPARKS: {
      const RastCol col = vcol(143, 128, 233, 0.7);
      const VfxNode *p1 = tpl_key(V_HITSPARKS, "path1");
      const VfxNode *p2 = tpl_key(V_HITSPARKS, "path2");
      draw_array_path(col, v->face, cX + 10, cY, p1, 0.2 * s45, 0.2 * s45);
      draw_array_path(col, v->face, cX + 10, cY, p2, 0.2 * s45, 0.2 * s45);
      vm_save();
      vm_translate(cX, cY);
      vm_rotate(VFX_PI);
      draw_array_path(col, v->face, 0, 0, p1, 0.2 * s45, 0.2 * s45);
      draw_array_path(col, v->face, 0, 0, p2, 0.2 * s45, 0.2 * s45);
      vm_restore();
      break;
    }
    case V_POWERSHIELD:
      if (fmod(t, 2) != 0) {
        vm_save();
        vm_translate(cX, cY);
        const double seed = (rrand() + 1.5) * s45;
        vm_scale(seed, seed);
        for (int i = 0; i < 6; i++) {
          vm_rotate(VFX_PI / 3);
          vp_begin();
          vp_moveTo(0, -15);
          vp_lineTo(6, -23);
          vp_lineTo(0, -40);
          vp_lineTo(-6, -23);
          vp_closePath();
          vp_fill(vcol(255, 255, 255, 0.3));
        }
        vm_restore();
      }
      break;
    case V_BURNING:
      if (t == 1) {
        draw_array_path(vcol(253, 255, 161, 1.0), v->face, cX,
                        ((v->py + 7) * -S) + OY, tpl_key(V_NORMALHIT, "path2"),
                        0.2 * s45, 0.2 * s45);
      }
      // upstream fireburst pos.y reads instance.y (undefined -> NaN)
      vfx_respawn("fireburst", -10 + 20 * rrand() + v->px, NAN, 6);
      vfx_respawn("burncircle", -10 + 20 * rrand() + v->px,
                  -10 + 20 * rrand() + v->py, 1);
      break;
    case V_FIREFOXTAIL: {
      if (!v->hasTail) {
        v->hasTail = 1;
        for (int i = 0; i < 4; i++) v->tail[i] = rrand();
      }
      vm_save();
      vm_translate(cX, ((v->py + 4) * -S) + OY);
      vp_begin();
      vp_arc((-2 + v->tail[0] * 4) * S, (-2 + v->tail[1] * 4) * S, 4 * S, 0,
             2 * VFX_PI);
      vp_fill(vcol(fmax(149, 251 - (t * 5)), fmax(149, 187 - (t * 5)),
                   fmin(149, 90 + (t * 5)), 1 - (t / 15)));
      vp_begin();
      vp_arc((-2 + v->tail[2] * 4) * S, (-2 + v->tail[3] * 4) * S, 2 * S, 0,
             2 * VFX_PI);
      vp_fill(vcol(fmax(149, 223 - (t * 5)), fmin(149, 83 + (t * 5)),
                   fmin(149, 39 + (t * 5)), 1 - (t / 15)));
      vm_restore();
      break;
    }
    case V_HITFLAIR: {
      vp_begin();
      vp_arc(cX, cY, 15, 0, 2 * VFX_PI);
      vp_stroke(5, vcol(146, 217, 164, 0.7 * ((fr - t) / fr)), 1);
      vp_begin();
      vp_moveTo(cX + 3, cY - 3);
      vp_lineTo(cX + 30, cY);
      vp_lineTo(cX + 3, cY + 3);
      vp_lineTo(cX, cY + 30);
      vp_lineTo(cX - 3, cY + 3);
      vp_lineTo(cX - 30, cY);
      vp_lineTo(cX - 3, cY - 3);
      vp_lineTo(cX, cY - 30);
      vp_closePath();
      vp_fill(vcol(146, 217, 164, 0.8 * ((fr - t) / fr)));
      g->fg2LineWidth = 1.0; // hitFlair ends with lineWidth = 1 (no save)
      break;
    }
    case V_LASERSPARK: {
      if (!v->hasDir || !v->hasC1) break; // render-plane spawn only
      const double u = 8 * v->offset + 12 * (t / fr);
      const double px = (v->px + u * v->dirX) * S + OX;
      const double py = (v->py + u * v->dirY) * -S + OY;
      const double op = 0.75 * (3 - 4 * (t / fr));
      // chromaticAberration: 3 translated single-channel passes
      const double vx = 0.3 * v->dirY * S, vy = 0.3 * v->dirX * S;
      const double chans[3][4] = {
        {0, v->c1[1], 0, 0},          // G at 0
        {v->c1[0], 0, 0, -1},         // R at -vec
        {0, 0, v->c1[2], 1},          // B at +vec
      };
      for (int c = 0; c < 3; c++) {
        vm_save();
        vm_translate(chans[c][3] * vx, chans[c][3] * vy); // R at -vec, B at +vec
        vp_begin();
        vp_moveTo(px, py);
        vp_lineTo(px + 4 * S * v->dirX, py - 4 * S * v->dirY);
        vp_stroke(2, vcol(chans[c][0], chans[c][1], chans[c][2], op), 0);
        vm_restore();
      }
      break;
    }
    case V_LASER: {
      if (!v->hasC1) gfx_fatal("gfx_vfx: laser without colors");
      if (t == 1) {
        const double nsp = 8 + floor(6 * rrand());
        const double midAngle =
            v->face == 1 ? v->facing : VFX_PI - v->facing;
        vfx_lines_laserSpark(v->c1, v->px, v->py, nsp,
                             midAngle - 0.75 * VFX_PI / 2,
                             midAngle + 0.75 * VFX_PI / 2, 2);
      }
      vm_save();
      vm_translate(cX, cY);
      vm_rotate(-v->facing * v->face);
      if (t > 3) {
        const double a = 1 - (t - 4) / 6;
        const double offs[3] = {-0.25, -0.1, 0.05};
        const double cols[3][3] = {
          {0, 0, v->c1[2]}, {0, v->c1[1], 0}, {v->c1[0], 0, 0},
        };
        for (int c = 0; c < 3; c++) {
          vp_begin();
          vp_arc(0, 0, (offs[c] + t * 0.6) * S, 0, 2 * VFX_PI);
          vp_stroke(3, vcol(cols[c][0], cols[c][1], cols[c][2], a), 1);
        }
      }
      // chromAb energy quad: 3 single-channel passes at +-vec (0.3S, 0)
      const double op = fmin(1, 1 - (t - 4) / 6);
      const double vx = 0.3 * S;
      const double chans[3][4] = {
        {0, v->c2[1], 0, 0}, {v->c2[0], 0, 0, -1}, {0, 0, v->c2[2], 1},
      };
      for (int c = 0; c < 3; c++) {
        vm_save();
        vm_translate(chans[c][3] * vx, 0); // R at -vec, B at +vec
        vp_begin();
        vp_moveTo((-t * 1) * v->face * S, (-1.6 - t * 1.6) * S);
        vp_lineTo((-2.3 - t * 1) * v->face * S, (-2.4 - t * 1.6) * S);
        vp_lineTo((-2.3 - t * 1) * v->face * S, (2.4 + t * 1.6) * S);
        vp_lineTo((-t * 1) * v->face * S, (1.6 + t * 1.6) * S);
        vp_closePath();
        vp_fill(vcol(chans[c][0], chans[c][1], chans[c][2], op));
        vm_restore();
      }
      vm_restore();
      break;
    }
    case V_FALCONPUNCH: {
      const int p = (int)v->facing;
      const MlPlayer *pl = live_player(p);
      vm_save();
      const int frame = (int)t - 1;
      vm_translate(((pl->phys.pos.x + 2 * pl->phys.face) * S) + OX,
                   ((pl->phys.pos.y + 13) * -S) + OY);
      // linear fire gradient -> representative solid (mid stop)
      const RastCol col = (frame % 2) ? vcol(251, 187, 90, 1.0)
                                      : vcol(210, 59, 26, 1.0);
      const VfxNode *paths = tpl_key(V_FALCONPUNCH, "path");
      if (frame < 0 || frame >= paths->n) {
        gfx_fatal("gfx_vfx: falconpunch frame range");
      }
      draw_array_path_new(col, v->face, 0, 0, vn_at(paths, frame),
                          0.23 * s45, 0.23 * s45, pl->rotation,
                          pl->rotationPoint.x, pl->rotationPoint.y);
      vm_restore();
      break;
    }
    case V_FIREFOXLAUNCH: {
      const int p = (int)v->facing;
      const MlPlayer *pl = live_player(p);
      if (strcmp(pl->actionState, "UPSPECIALLAUNCH") == 0) {
        vm_save();
        const int frame = ((int)pl->timer - 1) % 4;
        vm_translate(cX, cY);
        const RastCol col = (frame % 2) ? vcol(251, 187, 90, 0.9)
                                        : vcol(210, 59, 26, 0.9);
        const VfxNode *paths = tpl_key(V_FIREFOXLAUNCH, "path");
        if (frame < 0 || frame >= paths->n) {
          gfx_fatal("gfx_vfx: firefoxlaunch frame range");
        }
        draw_array_path_new(col, v->face, 0, 0, vn_at(paths, frame),
                            0.35 * s45, 0.35 * s45, pl->rotation,
                            pl->rotationPoint.x, pl->rotationPoint.y);
        vm_restore();
      }
      break;
    }
    case V_SHOCKED: {
      vm_save();
      vm_translate(cX, cY);
      vp_begin();
      vp_arc((-30 + 60 * rrand()) * s45, (-30 + 60 * rrand()) * s45, 4 * s45,
             0, 2 * VFX_PI);
      vp_fill(vcol(209, 181, 255, 1.0));
      // one lightning bolt
      double bx = -30 + 60 * rrand(), by = -30 + 60 * rrand();
      vp_begin();
      vp_moveTo(bx * s45, by * s45);
      for (int seg = 0; seg < 3; seg++) {
        bx += -10 + rrand() * 20;
        by += -10 + rrand() * 20;
        vp_lineTo(bx * s45, by * s45);
      }
      vp_stroke(2, vcol(209, 181, 255, 1.0), 1);
      vm_restore();
      break;
    }
    case V_BLASTZONE: {
      vm_save();
      vm_translate(cX, cY);
      vm_rotate(v->face * VFX_PI / 180);
      RastCol col = vcol(149, 255, 163, 0.8 * ((fr - t) / fr));
      draw_array_path(col, 1, 0, -200 - (20 + (100 * (t / 20))),
                      tpl_key(V_BLASTZONE, "path1"), 1.3, 1.3);
      const VfxNode *a2 = tpl_key(V_BLASTZONE, "svg2Active");
      const VfxNode *a3 = tpl_key(V_BLASTZONE, "svg3Active");
      if (t >= vn_num_at(a2, 0) && t <= vn_num_at(a2, 1)) {
        const VfxNode *sc = vn_at(tpl_key(V_BLASTZONE, "svg2Scale"), (int)t - 1);
        draw_array_path(vcol(166, 223, 255, 1.0), 1, 0, -90,
                        tpl_key(V_BLASTZONE, "path2"),
                        vn_num_at(sc, 0) * 1.5, vn_num_at(sc, 1) * 1.5);
      }
      if (t >= vn_num_at(a3, 0) && t <= vn_num_at(a3, 1)) {
        const VfxNode *sc = vn_at(tpl_key(V_BLASTZONE, "svg3Scale"),
                                  (int)(t - vn_num_at(a3, 0)));
        draw_array_path(vcol(255, 161, 161, 1.0), 1, 0, -90,
                        tpl_key(V_BLASTZONE, "path2"),
                        vn_num_at(sc, 0) * 1.5, vn_num_at(sc, 1) * 1.5);
      }
      col = vcol(242, 255, 93, 0.8 * ((fr - t) / fr));
      draw_array_path(col, 1, 0, 0, tpl_key(V_BLASTZONE, "path4"), 1.5, 1.5);
      if (t < 10) {
        vm_save();
        vm_scale(0.5, 1);
        vp_begin();
        vp_arc(0, 0, (450 * (t / 10) + 170), 0, 2 * VFX_PI);
        vp_fill(vcol(255, 255, 255, 0.8 * ((10 - t) / 10)));
        vm_restore();
      }
      vm_restore();
      break;
    }
    case V_SHINELOOP: {
      const int p = (int)v->face;
      if (!isfinite(v->face)) break;
      const MlPlayer *pl = live_player(p);
      const double tX = (pl->phys.pos.x * S) + OX;
      const double tY = ((pl->phys.pos.y + 6) * -S) + OY;
      const double part = js_round(pl->shineLoop / 2);
      if (part == 1) {
        draw_hexagon(3.5 * S, tX, tY, 7, vcol(0, 0, 229, 1.0));
        draw_hexagon(4 * S, tX, tY, 7, vcol(0, 189, 0, 1.0));
        draw_hexagon(4.5 * S, tX, tY, 7, vcol(52, 0, 0, 1.0));
      } else if (part == 2) {
        draw_hexagon(5.5 * S, tX, tY, 11, vcol(0, 0, 229, 1.0));
        draw_hexagon(6 * S, tX, tY, 11, vcol(0, 189, 0, 1.0));
        draw_hexagon(6.5 * S, tX, tY, 11, vcol(52, 0, 0, 1.0));
      } else if (part == 3) {
        draw_hexagon(7.5 * S, tX, tY, 15, vcol(0, 0, 229, 1.0));
        draw_hexagon(8 * S, tX, tY, 15, vcol(0, 189, 0, 1.0));
        draw_hexagon(8.5 * S, tX, tY, 15, vcol(52, 0, 0, 1.0));
      }
      // else: upstream console.log only
      break;
    }
    case V_TECH: {
      vm_save();
      const RastCol sc = vcol(251, 246, 119, 0.3 * ((fr - t) / fr) + 0.7);
      const RastCol fc = vcol(255, 116, 92, 0.3 * ((fr - t) / fr) + 0.7);
      vm_translate(cX, cY);
      vm_scale(fmin(0.2 * t, 1), fmin(0.2 * t, 1));
      vm_rotate(t * VFX_PI / 8);
      for (int i = 0; i < 4; i++) {
        vm_scale(0.7 + rrand() * 0.6, 0.7 + rrand() * 0.6);
        vm_rotate(i * VFX_PI / 2);
        const double radii[3] = {10, 15, 20};
        for (int r = 0; r < 3; r++) {
          vp_begin();
          vp_arc(0, 0, radii[r] * s45, 1.35 * VFX_PI, 1.65 * VFX_PI);
          vp_stroke(3, sc, 1);
        }
        vp_begin();
        vp_arc(0, 0, 23 * s45, 1.35 * VFX_PI, 1.65 * VFX_PI);
        vp_fill(fc);
      }
      vm_restore();
      break;
    }
    case V_ENTRANCE: {
      vm_save();
      vm_translate(cX, cY);
      double anglePos = t * VFX_PI / 32;
      for (int i = 0; i < 8; i++) {
        const double seed = rrand() - 0.5;
        const double eoX = 35 * fd_cos(anglePos);
        const double eoY = 35 * fd_sin(anglePos) * 0.4;
        // pillar gradient (white 0.3 -> 0) -> representative solid 0.2
        vfx_fillRect(eoX, eoY - (t * 2 + seed * 60), 10 * s45,
                     (t * 2 + seed * 60) * s45, vcol(255, 255, 255, 0.2));
        anglePos += VFX_PI / 4;
      }
      vfx_fillRect(-35, -fmod(t, 15) * 5, 80 * s45, 15 * s45,
                   vcol(163, 255, 203, 0.3));
      vm_restore();
      vm_save();
      vm_translate(cX, cY);
      vm_scale(0.8 + (rrand() * 0.3), 0.2 + (0.2 * rrand()));
      vp_begin();
      vp_arc(5, -t * 3, (35 + fmod(t, 2) * 10) * s45, 0, 2 * VFX_PI);
      vp_stroke(8, vcol(255, 149, 149, 0.8), 1);
      vm_restore();
      break;
    }
    case V_TARGETDESTROY: {
      vm_save();
      const RastCol col = vcol(255, 255, 255, 0.8);
      vm_translate(cX, cY);
      vp_begin();
      vp_arc(0, 0, t * 2, 0, 2 * VFX_PI);
      vp_stroke(3, col, 1);
      vm_scale(s45, s45);
      for (int i = 0; i < 6; i++) {
        vm_rotate(VFX_PI / 3);
        vp_begin();
        vp_moveTo(0, -14 - t * 2);
        vp_lineTo(6, -22 - t * 2);
        vp_lineTo(0, -40 - t * 2);
        vp_lineTo(-6, -22 - t * 2);
        vp_closePath();
        vp_fill(col);
      }
      vm_restore();
      break;
    }
    case V_POWERSHIELDREFLECT: {
      const double frame = t;
      vm_save();
      vm_translate(cX, cY);
      vp_begin();
      vp_arc(0, 0, (13 - frame * 1) * S, 0, 2 * VFX_PI);
      vp_stroke(4, vcol(255, 127, 112, 0.8 - 0.15 * frame), 1);
      // radial gradient discs -> representative solids
      if (frame < 3) {
        vp_begin();
        vp_arc(0, 0, (25 - frame * 2) * S, 0, 2 * VFX_PI);
        vp_fill(vcol(255, 255, 255, 0.5 * (1 - 0.15 * frame)));
      }
      vp_begin();
      vp_arc(0, 0, (10 - frame * 1) * S, 0, 2 * VFX_PI);
      vp_stroke(4, vcol(97, 255, 250, 0.5), 1);
      for (int i = 0; i < 14; i++) {
        vm_rotate(VFX_PI / 7 + (-0.3 + rrand() * 0.6));
        vp_begin();
        vp_moveTo(0, ((15 + rrand() * 10) - frame * 1.5) * S);
        vp_lineTo(0, ((-15 - rrand() * 10) + frame * 1.5) * S);
        vp_stroke(4, vcol(97, 255, 250, 0.5), 0);
      }
      vm_restore();
      break;
    }
    case V_SWING: {
      const int p = v->swingPnum;
      if (v->facingKind != 2) gfx_fatal("gfx_vfx: swing without swing facing");
      const MlPlayer *pl = live_player(p);
      if (!v->swingSnap) {
        v->swingSnap = 1;
        v->posNowX = pl->phys.pos.x;
        v->posNowY = pl->phys.pos.y;
        v->posPrevX = pl->phys.posPrev.x;
        v->posPrevY = pl->phys.posPrev.y;
      }
      const int frame = (int)v->swingFrame;
      const VfxNode *sw = sword_lookup(v->swingType);
      if (frame < 0 || frame + 1 >= sw->n) break; // swordSwings[t][f+1] undefined
      const VfxNode *swordPrev = vn_at(sw, frame);
      const VfxNode *swordNow = vn_at(sw, frame + 1);
      const int charId = live_char(p);
      const double scale = ml_f64(ml_attributes[charId].charScale);
      const double face = pl->phys.face;
      const double soX = OX, soY = OY;
      vp_begin();
      vp_moveTo(((scale * (vn_num_at(vn_at(swordNow, 0), 0) / 4.5 * face) +
                  v->posNowX) * S + soX),
                ((scale * (vn_num_at(vn_at(swordNow, 0), 1) / -4.5) +
                  v->posNowY) * -S + soY));
      vp_lineTo(((scale * (vn_num_at(vn_at(swordNow, 1), 0) / 4.5 * face) +
                  v->posNowX) * S + soX),
                ((scale * (vn_num_at(vn_at(swordNow, 1), 1) / -4.5) +
                  v->posNowY) * -S + soY));
      vp_lineTo(((scale * (vn_num_at(vn_at(swordPrev, 1), 0) / 4.5 * face) +
                  v->posPrevX) * S + soX),
                ((scale * (vn_num_at(vn_at(swordPrev, 1), 1) / -4.5) +
                  v->posPrevY) * -S + soY));
      vp_lineTo(((scale * (vn_num_at(vn_at(swordPrev, 0), 0) / 4.5 * face) +
                  v->posPrevX) * S + soX),
                ((scale * (vn_num_at(vn_at(swordPrev, 0), 1) / -4.5) +
                  v->posPrevY) * -S + soY));
      vp_closePath();
      vp_fill(vcol(46, 217, 255, 0.7 - (0.7 / 5 * t)));
      break;
    }
    case V_NORMALHIT: {
      vm_save();
      vm_translate(cX, cY);
      const int ti = (int)t;
      if (ti == 1) {
        dv_burst_shape(vcol(255, 188, 14, 0.62));
      } else if (ti == 2) {
        draw_array_path(vcol(255, 61, 61, 1.0), v->face, 0, 0,
                        tpl_key(V_NORMALHIT, "path1"), 0.2 * s45, 0.2 * s45);
      } else if (ti == 3) {
        draw_array_path(vcol(150, 208, 255, 1.0), v->face, 0, 0,
                        tpl_key(V_NORMALHIT, "path2"), 0.2 * s45, 0.2 * s45);
      } else if (ti >= 4 && ti <= 7) {
        const VfxNode *p3 = tpl_key(V_NORMALHIT, "path3");
        for (int n = 0; n < p3->n; n++) {
          draw_array_path(vcol(120, 255, 99, 4 / t), v->face, 0, 0,
                          vn_at(p3, n), 0.2 * (t / 7) * s45, 0.2 * (t / 7) * s45);
        }
      }
      vm_restore();
      break;
    }
    case V_FIREHIT: {
      vm_save();
      vm_translate(cX, cY);
      const int ti = (int)t;
      const VfxNode *p3 = tpl_key(V_NORMALHIT, "path3");
      if (ti == 1 || ti == 2) {
        dv_burst_shape(vcol(255, 255, 255, 0.62));
        for (int n = 0; n < p3->n; n++) {
          draw_array_path(vcol(255, 164, 56, 0.8), v->face, 0, 0, vn_at(p3, n),
                          0.15 * s45, 0.15 * s45);
        }
      } else if (ti == 3) {
        for (int n = 0; n < p3->n; n++) {
          draw_array_path(vcol(255, 164, 56, 0.8), v->face, 0, 0, vn_at(p3, n),
                          0.2 * (t / 7) * s45, 0.2 * (t / 7) * s45);
        }
      } else if (ti >= 4 && ti <= 7) {
        for (int n = 0; n < p3->n; n++) {
          draw_array_path(vcol(255, 227, 79, 4 / t), v->face, 0, 0,
                          vn_at(p3, n), 0.1 * (t / 7) * s45, 0.1 * (t / 7) * s45);
        }
      }
      // fireburst with pos.y = instance.y (undefined -> NaN): never draws
      vfx_respawn("fireburst", -10 + 20 * rrand() + v->px, NAN, 8);
      vm_restore();
      break;
    }
    case V_ELECTRICHIT: {
      vm_save();
      vm_translate(cX, cY);
      const int ti = (int)t;
      const VfxNode *p3 = tpl_key(V_NORMALHIT, "path3");
      if (ti == 1) {
        dv_burst_shape(vcol(133, 122, 250, 0.62));
      } else if (ti == 2) {
        draw_array_path(vcol(50, 252, 162, 1.0), v->face, 0, 0,
                        tpl_key(V_NORMALHIT, "path1"), 0.2 * s45, 0.2 * s45);
      } else if (ti == 3) {
        draw_array_path(vcol(0, 0, 0, 1.0), v->face, 0, 0,
                        tpl_key(V_NORMALHIT, "path2"), 0.2 * s45, 0.2 * s45);
      } else if (ti == 4) {
        draw_array_path(vcol(198, 222, 255, 1.0), v->face, 0, 0,
                        tpl_key(V_NORMALHIT, "path2"), 0.2 * s45, 0.2 * s45);
      } else if (ti == 5) {
        for (int n = 0; n < p3->n; n++) {
          draw_array_path(vcol(0, 0, 0, 1.0), v->face, 0, 0, vn_at(p3, n),
                          0.2 * (t / 7) * s45, 0.2 * (t / 7) * s45);
        }
      } else if (ti == 6) {
        for (int n = 0; n < p3->n; n++) {
          draw_array_path(vcol(139, 130, 242, 1.0), v->face, 0, 0,
                          vn_at(p3, n), 0.2 * (t / 7) * s45, 0.2 * (t / 7) * s45);
        }
      }
      if (t < 13) {
        for (int i = 0; i < 2; i++) {
          vp_begin();
          vp_arc((-30 + 60 * rrand()) * s45, (-30 + 60 * rrand()) * s45,
                 4 * s45, 0, 2 * VFX_PI);
          vp_fill(vcol(209, 181, 255, 1.0));
        }
      }
      vp_begin();
      const int bolts = 4 - (int)js_round(t / 4);
      for (int j = 0; j < bolts; j++) {
        double bx = -30 + 60 * rrand(), by = -30 + 60 * rrand();
        vp_moveTo(bx * s45, by * s45);
        for (int seg = 0; seg < 3; seg++) {
          bx += -10 + rrand() * 20;
          by += -10 + rrand() * 20;
          vp_lineTo(bx * s45, by * s45);
        }
      }
      vp_stroke(2, vcol(209, 181, 255, 1.0), 0);
      vm_restore();
      break;
    }
    case V_SHINE: {
      vm_save();
      const double tX = cX, tY = cY;
      const RastCol lightBlue = vcol(196, 252, 254, 0.82);
      const RastCol white = vcol(235, 250, 255, 0.9);
      double r, a, b;
      const int ti = (int)t;
      if (ti == 1) {
        draw_hexagon(5.1 * S, tX, tY, 10, lightBlue);
        draw_hexagon(6 * S, tX, tY, 5, white);
        r = 5.1 * S;
        a = r * fd_sin(VFX_PI / 6);
        b = r * fd_cos(VFX_PI / 6);
        vm_save();
        vm_translate(tX, tY);
        vp_begin();
        vp_moveTo(0, r);
        vp_lineTo(b, r - a);
        vp_lineTo(b, -r + a);
        vp_lineTo(0, -r);
        vp_closePath();
        vp_fill(white);
        vm_restore();
        vfx_stars(tX, tY, 2 + 3 * floor(rrand()), 1.5 * S, 5.5 * S);
      } else if (ti == 2) {
        draw_hexagon(6.6 * S, tX, tY, 10, lightBlue);
        draw_hexagon(7.5 * S, tX, tY, 5, white);
        r = 6.6 * S;
        a = r * fd_sin(VFX_PI / 6);
        b = r * fd_cos(VFX_PI / 6);
        vm_save();
        vm_translate(tX, tY);
        vp_begin();
        vp_moveTo(-b, r - a);
        vp_lineTo(0, r);
        vp_lineTo(b, r - a);
        vp_lineTo(b, -r + a);
        vp_closePath();
        vp_fill(white);
        vm_restore();
        vfx_stars(tX, tY, 3 + 3 * floor(rrand()), 4 * S, 7 * S);
      } else {
        draw_hexagon(8.1 * S, tX, tY, 10, lightBlue);
        draw_hexagon(9 * S, tX, tY, 5, white);
        r = 8.1 * S;
        a = r * fd_sin(VFX_PI / 6);
        b = r * fd_cos(VFX_PI / 6);
        vm_save();
        vm_translate(tX, tY);
        vp_begin();
        vp_moveTo(-b, -r + a);
        vp_lineTo(-b, r - a);
        vp_lineTo(0, r);
        vp_lineTo(b, r - a);
        vp_closePath();
        vp_fill(white);
        vm_restore();
        vfx_stars(tX, tY, 2 + 3 * floor(rrand()), 6 * S, 8 * S);
      }
      vm_restore();
      break;
    }
    case V_ILLUSION: {
      if (fmod(t, 2) == 0) {
        const VfxNode *path = tpl_key(V_ILLUSION, "path");
        draw_array_path_new(vcol(68, 0, 0, 0.75), v->face,
                            ((v->px - 0.3) * S) + OX, ((v->py - 0.3) * -S) + OY,
                            path, 0.35 * s45, 0.35 * s45, 0, 0, 0);
        draw_array_path_new(vcol(0, 244, 0, 0.75), v->face, cX, cY, path,
                            0.35 * s45, 0.35 * s45, 0, 0, 0);
        draw_array_path_new(vcol(0, 0, 255, 0.75), v->face,
                            ((v->px + 0.3) * S) + OX, ((v->py + 0.3) * -S) + OY,
                            path, 0.35 * s45, 0.35 * s45, 0, 0, 0);
      }
      break;
    }
    case V_PHANTASM: {
      if (fmod(t, 2) == 0) {
        const VfxNode *path = tpl_key(V_PHANTASM, "path");
        draw_array_path_compress(vcol(68, 0, 0, 0.75), v->face,
                                 ((v->px - 0.3) * S) + OX,
                                 ((v->py - 0.3) * -S) + OY, path, 0.47 * s45,
                                 0.47 * s45, 0, 0, 0);
        draw_array_path_compress(vcol(0, 244, 0, 0.75), v->face, cX, cY, path,
                                 0.47 * s45, 0.47 * s45, 0, 0, 0);
        draw_array_path_compress(vcol(0, 0, 255, 0.75), v->face,
                                 ((v->px + 0.3) * S) + OX,
                                 ((v->py + 0.3) * -S) + OY, path, 0.47 * s45,
                                 0.47 * s45, 0, 0, 0);
      }
      break;
    }
    case V_SING:
      dv_sing_gen(v, 2, 6, 0, 4.5, 0.5);
      break;
    case V_SING2:
      dv_sing_gen(v, 3, 8, 3.5, -5, 1.3);
      break;
    case V_SING3:
      dv_sing_gen(v, 5, 10, 7.1, 0.2, 0);
      break;
    case V_START:
      dv_start(v);
      break;
    case V_HITCURVE:
      break; // upstream dVfx/hitCurve.js is an empty TODO — draws nothing
    default:
      fprintf(stderr, "gfx_vfx: no draw arm for %s\n", v_names[v->tpl]);
      gfx_fatal("gfx_vfx: unhandled vfx name");
  }
}

// ===========================================================================
// injection (synthetic coverage, expected-render.json "inject")
// ===========================================================================

#define INJ_CAP 16
static long g_inj_frame = -1;
static int g_inj_n;
static struct {
  char name[24];
  double px, py;
  int hasFace; double face;
  int hasF; double f;
} g_inj[INJ_CAP];

void gfx_vfx_inject_load(const char *path) {
  g_inj_frame = -1;
  g_inj_n = 0;
  FILE *f = fopen(path, "r");
  if (!f) gfx_fatal("inject: cannot open table");
  char line[256];
  int seenEnd = 0;
  if (!fgets(line, sizeof line, f) || strcmp(line, "INJECT1\n") != 0) {
    gfx_fatal("inject: bad magic");
  }
  while (fgets(line, sizeof line, f)) {
    if (strcmp(line, "END\n") == 0) { seenEnd = 1; break; }
    if (strncmp(line, "AT ", 3) == 0) {
      g_inj_frame = strtol(line + 3, 0, 10);
      if (g_inj_frame <= 0) gfx_fatal("inject: bad AT frame");
    } else if (strncmp(line, "V ", 2) == 0) {
      if (g_inj_n >= INJ_CAP) gfx_fatal("inject: table overflow");
      char faceTok[32], fTok[32];
      if (sscanf(line + 2, "%23s %lf %lf %31s %31s", g_inj[g_inj_n].name,
                 &g_inj[g_inj_n].px, &g_inj[g_inj_n].py, faceTok, fTok) != 5) {
        gfx_fatal("inject: bad V line");
      }
      if (strcmp(faceTok, "-") == 0) {
        g_inj[g_inj_n].hasFace = 0;
      } else {
        g_inj[g_inj_n].hasFace = 1;
        char *end;
        g_inj[g_inj_n].face = strtod(faceTok, &end);
        if (*end) gfx_fatal("inject: bad face token");
      }
      if (strcmp(fTok, "-") == 0) {
        g_inj[g_inj_n].hasF = 0;
      } else {
        g_inj[g_inj_n].hasF = 1;
        char *end;
        g_inj[g_inj_n].f = strtod(fTok, &end);
        if (*end) gfx_fatal("inject: bad f token");
      }
      g_inj_n++;
    } else {
      gfx_fatal("inject: unknown line");
    }
  }
  fclose(f);
  if (!seenEnd) gfx_fatal("inject: missing END");
  if (g_inj_frame < 0 || g_inj_n == 0) gfx_fatal("inject: empty table");
}

void gfx_vfx_inject_fire(long frame) {
  if (g_inj_frame < 0 || frame != g_inj_frame) return;
  for (int i = 0; i < g_inj_n; i++) {
    VSpawn c;
    memset(&c, 0, sizeof c);
    c.name = g_inj[i].name;
    c.px = g_inj[i].px;
    c.py = g_inj[i].py;
    c.hasFace = g_inj[i].hasFace;
    c.face = g_inj[i].face;
    if (g_inj[i].hasF) {
      c.fKind = 1;
      c.fNum = g_inj[i].f;
    }
    vspawn(&c);
  }
}

// ===========================================================================
// install
// ===========================================================================

static void vfx_sink(const MlVfx *cfg) { gfx_vfx_spawn(cfg); }

void gfx_vfx_install(Gfx *g) {
  if (!g_vfx_loaded) gfx_fatal("gfx_vfx: install before gfx_vfx_load");
  g_g = g;
  g_qn = 0;
  ml_rng_seed(&g_rrng, 0xC0FFEE42u); // render-LOCAL stream, never the chain
  vm_reset();
  gfx_overlay_reset();
  gfx_bg_reset();
  ml_vfx_sink = vfx_sink;
}
