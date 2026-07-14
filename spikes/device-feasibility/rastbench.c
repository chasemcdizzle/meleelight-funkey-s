/*
 * rastbench — on-device software vector-path rasterizer benchmark for the
 * meleelight → FunKey-S feasibility spike (ticket #8, Experiment 1).
 *
 * Renders, each frame, a worst-ish meleelight scene at 240x240 16bpp:
 *   - full clear
 *   - stage: battlefield-ish main platform + 3 floating platforms (polygon fill)
 *   - 2 characters: REAL meleelight animation frames (fox + marth; WAIT/DASH/
 *     ATTACKAIRN cycled), each frame = one closed multi-bezier canvas path,
 *     flattened to polylines and scanline-filled (nonzero winding),
 *     x-mirroring for facing, per-frame motion.
 *   - 4 projectiles: small rotated 4-bezier diamonds through the same pipeline.
 *
 * Variants: AA on/off (4x vertical subsampling + fractional span ends),
 * bezier flattening tolerance, character scale (natural fit vs 2x zoom).
 * Baseline mode: clear + present only (the floor).
 *
 * Build (device):  see build.sh  (SDL 1.2, -O2, VFPv4 hard-float)
 * Build (host):    cc -O2 -DHEADLESS rastbench.c -o rastbench -lm
 *
 * Usage: rastbench <anim.bin> [-mode scene|baseline] [-aa 0|1] [-tol 0.25]
 *                  [-zoom 1|2] [-frames 1200] [-dump /tmp/frame.ppm]
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <math.h>
#include <time.h>

#ifndef HEADLESS
#include <SDL/SDL.h>
#endif

#define W 240
#define H 240
#define SUBS 4              /* AA vertical subsamples per pixel row */
#define MAXEDGES 65536
#define MAXPTS   8192       /* flattened points per path */
#define MAXFRAMES 20000

/* ---------- anim.bin loading ---------- */
typedef struct { uint32_t ncoords; const int16_t *coords; } Path;
typedef struct { uint32_t npaths; Path *paths; } Frame;
typedef struct { uint32_t nframes; Frame *frames; } Anim;

static Anim g_anims[16];
static uint32_t g_nanims;

static void load_anims(const char *fn) {
    FILE *f = fopen(fn, "rb");
    if (!f) { fprintf(stderr, "cannot open %s\n", fn); exit(1); }
    fseek(f, 0, SEEK_END); long sz = ftell(f); fseek(f, 0, SEEK_SET);
    uint8_t *buf = malloc(sz);
    if (fread(buf, 1, sz, f) != (size_t)sz) { fprintf(stderr, "short read\n"); exit(1); }
    fclose(f);
    const uint8_t *p = buf;
    #define RD32() ({ uint32_t v; memcpy(&v, p, 4); p += 4; v; })
    uint32_t magic = RD32();
    if (magic != 0x4E414C4D) { fprintf(stderr, "bad magic %08x\n", magic); exit(1); }
    g_nanims = RD32();
    for (uint32_t a = 0; a < g_nanims; a++) {
        Anim *A = &g_anims[a];
        A->nframes = RD32();
        A->frames = calloc(A->nframes, sizeof(Frame));
        for (uint32_t fi = 0; fi < A->nframes; fi++) {
            Frame *F = &A->frames[fi];
            F->npaths = RD32();
            F->paths = calloc(F->npaths, sizeof(Path));
            for (uint32_t pi = 0; pi < F->npaths; pi++) {
                F->paths[pi].ncoords = RD32();
                F->paths[pi].coords = (const int16_t *)p;
                uint32_t n = F->paths[pi].ncoords;
                p += (n + (n & 1)) * 2;   /* padded to 4 bytes */
            }
        }
    }
    fprintf(stderr, "loaded %u anims (%ld bytes)\n", g_nanims, sz);
}

/* ---------- framebuffer ---------- */
static uint16_t *g_fb;      /* points into SDL surface (or malloc'd headless) */
static int g_pitch16;       /* pitch in uint16 units */

static uint16_t g_col_bg, g_col_stage, g_col_plat, g_col_c1, g_col_c2, g_col_proj;

#ifdef HEADLESS
static uint16_t rgb565(int r, int g, int b) {
    return (uint16_t)(((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3));
}
#else
static SDL_Surface *g_screen;
static uint16_t rgb565(int r, int g, int b) {
    return (uint16_t)SDL_MapRGB(g_screen->format, r, g, b);
}
#endif

/* 16bpp blend col over dst with alpha a (0..256), assuming 565 layout */
static inline uint16_t blend565(uint16_t dst, uint16_t col, unsigned a) {
    unsigned dr = dst & 0xF81F, dg = dst & 0x07E0;   /* r+b packed, g */
    unsigned cr = col & 0xF81F, cg = col & 0x07E0;
    unsigned rr = ((cr * a + dr * (256 - a)) >> 8) & 0xF81F;
    unsigned rg = ((cg * a + dg * (256 - a)) >> 8) & 0x07E0;
    return (uint16_t)(rr | rg);
}

/* ---------- polygon rasterizer (edge table + active edge list) ---------- */
typedef struct { float ymin, ymax, x, dxdy; int dir; } Edge;
static Edge g_edges[MAXEDGES];
static int g_nedges;
static int g_order[MAXEDGES];      /* edge indices sorted by ymin */
static int g_active[4096];
static float g_xs[4096];           /* crossing x per active edge */
static int g_wind[4096];

static uint16_t g_cov[W];          /* AA coverage accumulator, one row (0..256) */

static void edges_reset(void) { g_nedges = 0; }

static void edge_add(float x0, float y0, float x1, float y1) {
    if (y0 == y1) return;
    Edge *e = &g_edges[g_nedges];
    if (g_nedges >= MAXEDGES - 1) { fprintf(stderr, "edge overflow\n"); exit(1); }
    if (y0 < y1) { e->ymin = y0; e->ymax = y1; e->x = x0; e->dir = 1; }
    else         { e->ymin = y1; e->ymax = y0; e->x = x1; e->dir = -1; }
    e->dxdy = (x1 - x0) / (y1 - y0);
    g_nedges++;
}

static int cmp_edge_ymin(const void *a, const void *b) {
    float ya = g_edges[*(const int *)a].ymin, yb = g_edges[*(const int *)b].ymin;
    return (ya > yb) - (ya < yb);
}

/* fill current edge set with colour col; aa = 0/1 */
static void fill_edges(uint16_t col, int aa) {
    if (g_nedges == 0) return;
    float ymin = 1e9f, ymax = -1e9f;
    for (int i = 0; i < g_nedges; i++) {
        g_order[i] = i;
        if (g_edges[i].ymin < ymin) ymin = g_edges[i].ymin;
        if (g_edges[i].ymax > ymax) ymax = g_edges[i].ymax;
    }
    qsort(g_order, g_nedges, sizeof(int), cmp_edge_ymin);
    int y0 = (int)floorf(ymin); if (y0 < 0) y0 = 0;
    int y1 = (int)ceilf(ymax);  if (y1 > H) y1 = H;
    int next = 0, nact = 0;
    int subs = aa ? SUBS : 1;
    float substep = 1.0f / subs;
    unsigned cov_inc = 256 / subs;

    for (int y = y0; y < y1; y++) {
        if (aa) memset(g_cov, 0, sizeof(g_cov));
        for (int s = 0; s < subs; s++) {
            float sy = y + (s + 0.5f) * substep;
            /* admit new edges */
            while (next < g_nedges && g_edges[g_order[next]].ymin <= sy) {
                g_active[nact++] = g_order[next++];
            }
            /* compute crossings, drop dead edges */
            int n = 0;
            for (int i = 0; i < nact; i++) {
                Edge *e = &g_edges[g_active[i]];
                if (e->ymax <= sy) {  /* expired */
                    g_active[i--] = g_active[--nact];
                    continue;
                }
                if (e->ymin <= sy) {
                    g_xs[n] = e->x + (sy - e->ymin) * e->dxdy;
                    g_wind[n] = e->dir;
                    n++;
                }
            }
            /* insertion sort crossings by x */
            for (int i = 1; i < n; i++) {
                float x = g_xs[i]; int w = g_wind[i]; int j = i - 1;
                while (j >= 0 && g_xs[j] > x) { g_xs[j+1] = g_xs[j]; g_wind[j+1] = g_wind[j]; j--; }
                g_xs[j+1] = x; g_wind[j+1] = w;
            }
            /* walk spans, nonzero winding */
            int wind = 0; float spanx = 0;
            for (int i = 0; i < n; i++) {
                int prev = wind;
                wind += g_wind[i];
                if (prev == 0 && wind != 0) spanx = g_xs[i];
                else if (prev != 0 && wind == 0) {
                    float xa = spanx, xb = g_xs[i];
                    if (xa < 0) xa = 0;
                    if (xb > W) xb = W;
                    if (xb <= xa) continue;
                    if (!aa) {
                        int ia = (int)(xa + 0.5f), ib = (int)(xb + 0.5f);
                        if (ib > ia) {
                            uint16_t *row = g_fb + y * g_pitch16;
                            for (int x = ia; x < ib; x++) row[x] = col;
                        }
                    } else {
                        int ia = (int)floorf(xa), ib = (int)floorf(xb);
                        if (ia == ib) {
                            g_cov[ia] += (uint16_t)((xb - xa) * cov_inc);
                        } else {
                            g_cov[ia] += (uint16_t)((ia + 1 - xa) * cov_inc);
                            for (int x = ia + 1; x < ib; x++) g_cov[x] += cov_inc;
                            if (ib < W) g_cov[ib] += (uint16_t)((xb - ib) * cov_inc);
                        }
                    }
                }
            }
        }
        if (aa) {
            uint16_t *row = g_fb + y * g_pitch16;
            for (int x = 0; x < W; x++) {
                unsigned c = g_cov[x];
                if (!c) continue;
                if (c >= 256) row[x] = col;
                else row[x] = blend565(row[x], col, c);
            }
        }
    }
}

/* ---------- bezier flattening ---------- */
static float g_tol = 0.25f;
static float g_px[MAXPTS], g_py[MAXPTS];
static int g_npts;

static void pt_add(float x, float y) {
    if (g_npts < MAXPTS) { g_px[g_npts] = x; g_py[g_npts] = y; g_npts++; }
}

/* fixed-count subdivision derived from control-polygon deviation vs tolerance */
static void flatten_cubic(float x0, float y0, float x1, float y1,
                          float x2, float y2, float x3, float y3) {
    float dx = x3 - x0, dy = y3 - y0;
    float d1 = fabsf((x1 - x0) * dy - (y1 - y0) * dx);
    float d2 = fabsf((x2 - x0) * dy - (y2 - y0) * dx);
    float len = sqrtf(dx * dx + dy * dy) + 1e-6f;
    float dev = (d1 > d2 ? d1 : d2) / len;      /* max ctrl-pt deviation, px */
    int n = 1 + (int)sqrtf(3.0f * dev / g_tol);
    if (n > 24) n = 24;
    float dt = 1.0f / n;
    for (int i = 1; i <= n; i++) {
        float t = i * dt, mt = 1 - t;
        float a = mt * mt * mt, b = 3 * mt * mt * t, c = 3 * mt * t * t, d = t * t * t;
        pt_add(a * x0 + b * x1 + c * x2 + d * x3,
               a * y0 + b * y1 + c * y2 + d * y3);
    }
}

/* draw one meleelight frame (array of Int16Array canvas paths) as one fill */
typedef struct { float tx, ty, sx, sy, rot; } Xform;

static void draw_anim_frame(const Frame *F, const Xform *xf, uint16_t col, int aa) {
    float cr = cosf(xf->rot), sr = sinf(xf->rot);
    edges_reset();
    for (uint32_t pi = 0; pi < F->npaths; pi++) {
        const int16_t *c = F->paths[pi].coords;
        uint32_t n = F->paths[pi].ncoords;
        g_npts = 0;
        float px = c[0] * xf->sx, py = c[1] * xf->sy;
        pt_add(xf->tx + px * cr - py * sr, xf->ty + px * sr + py * cr);
        for (uint32_t k = 2; k + 5 < n; k += 6) {
            float xs[3], ys[3];
            for (int j = 0; j < 3; j++) {
                float mx = c[k + j * 2] * xf->sx, my = c[k + j * 2 + 1] * xf->sy;
                xs[j] = xf->tx + mx * cr - my * sr;
                ys[j] = xf->ty + mx * sr + my * cr;
            }
            float lx = g_px[g_npts - 1], ly = g_py[g_npts - 1];
            flatten_cubic(lx, ly, xs[0], ys[0], xs[1], ys[1], xs[2], ys[2]);
        }
        /* close subpath, emit edges */
        for (int i = 0; i + 1 < g_npts; i++)
            edge_add(g_px[i], g_py[i], g_px[i + 1], g_py[i + 1]);
        edge_add(g_px[g_npts - 1], g_py[g_npts - 1], g_px[0], g_py[0]);
    }
    fill_edges(col, aa);
}

/* convex polygon helper (stage) */
static void draw_poly(const float *xy, int n, uint16_t col, int aa) {
    edges_reset();
    for (int i = 0; i < n; i++) {
        int j = (i + 1) % n;
        edge_add(xy[i * 2], xy[i * 2 + 1], xy[j * 2], xy[j * 2 + 1]);
    }
    fill_edges(col, aa);
}

/* ---------- scene ---------- */
/* 4-bezier diamond projectile, radius r, model coords */
static int16_t g_proj_coords[2 + 4 * 6];
static Path g_proj_path;
static Frame g_proj_frame;

static void proj_init(void) {
    int r = 20;   /* model units; scaled below */
    /* start at (r,0); 4 curved segments through (0,-r),(-r,0),(0,r) back to (r,0) */
    static const int pts[5][2] = {{1,0},{0,-1},{-1,0},{0,1},{1,0}};
    int16_t *c = g_proj_coords;
    c[0] = (int16_t)(pts[0][0] * r); c[1] = (int16_t)(pts[0][1] * r);
    int k = 2;
    for (int i = 0; i < 4; i++) {
        int x0 = pts[i][0] * r, y0 = pts[i][1] * r;
        int x1 = pts[i+1][0] * r, y1 = pts[i+1][1] * r;
        /* control points bulge outward a bit */
        c[k++] = (int16_t)(x0 + (x1 - x0) / 3 + x0 / 4);
        c[k++] = (int16_t)(y0 + (y1 - y0) / 3 + y0 / 4);
        c[k++] = (int16_t)(x0 + 2 * (x1 - x0) / 3 + x1 / 4);
        c[k++] = (int16_t)(y0 + 2 * (y1 - y0) / 3 + y1 / 4);
        c[k++] = (int16_t)x1; c[k++] = (int16_t)y1;
    }
    g_proj_path.ncoords = 2 + 4 * 6;
    g_proj_path.coords = g_proj_coords;
    g_proj_frame.npaths = 1;
    g_proj_frame.paths = &g_proj_path;
}

static void clear_screen(uint16_t col) {
    uint32_t c2 = ((uint32_t)col << 16) | col;
    for (int y = 0; y < H; y++) {
        uint32_t *row = (uint32_t *)(g_fb + y * g_pitch16);
        for (int x = 0; x < W / 2; x++) row[x] = c2;
    }
}

/* anim schedule: cycle WAIT->DASH->ATTACKAIRN, advancing 1 anim frame per
 * rendered frame (like player[i].timer) */
static const Frame *sched_frame(int character /*0=fox,1=marth*/, int f) {
    int base = character * 3;
    int lens[3], total = 0;
    for (int i = 0; i < 3; i++) { lens[i] = g_anims[base + i].nframes; total += lens[i]; }
    int t = f % total;
    for (int i = 0; i < 3; i++) {
        if (t < lens[i]) return &g_anims[base + i].frames[t];
        t -= lens[i];
    }
    return &g_anims[base].frames[0];
}

static void render_scene(int f, int aa, float zoom) {
    clear_screen(g_col_bg);
    /* stage: battlefield-ish main platform (trapezoid) + 3 platforms */
    float main_plat[8] = { 30, 165, 210, 165, 232, 240, 8, 240 };
    draw_poly(main_plat, 4, g_col_stage, aa);
    float p1[8] = { 55, 120, 105, 120, 105, 124, 55, 124 };
    float p2[8] = { 135, 120, 185, 120, 185, 124, 135, 124 };
    float p3[8] = { 95, 78, 145, 78, 145, 82, 95, 82 };
    draw_poly(p1, 4, g_col_plat, aa);
    draw_poly(p2, 4, g_col_plat, aa);
    draw_poly(p3, 4, g_col_plat, aa);

    /* characters: browser draw scale = charScale*(stageScale/4.5); at 240px
     * wide vs 1200 logical -> x0.2; fox charScale 0.35 => ~0.07; zoom var
     * doubles it. Model y is negative-up, canvas y-down: scaleY positive
     * because data is already y-down (feet at 0, head at -190 -> we place
     * ty at feet). */
    float s1 = 0.35f * 0.2f * zoom;   /* fox */
    float s2 = 0.45f * 0.2f * zoom;   /* marth-ish */
    int face1 = ((f / 90) & 1) ? -1 : 1;
    int face2 = ((f / 70) & 1) ? 1 : -1;
    Xform xa = { 80 + 40 * sinf(f * 0.05f), 165, s1 * face1, s1, 0 };
    Xform xb = { 160 + 40 * sinf(f * 0.041f + 2.1f), 165, s2 * face2, s2, 0 };
    draw_anim_frame(sched_frame(0, f), &xa, g_col_c1, aa);
    draw_anim_frame(sched_frame(1, f), &xb, g_col_c2, aa);

    /* 4 projectiles (rotated diamonds, ~8px) */
    for (int i = 0; i < 4; i++) {
        float ang = f * 0.15f + i * 1.6f;
        Xform xp = { 40.0f + 50 * i + 25 * sinf(f * 0.09f + i),
                     100.0f + 30 * sinf(f * 0.13f + i * 0.7f),
                     0.2f * zoom, 0.2f * zoom, ang };
        draw_anim_frame(&g_proj_frame, &xp, g_col_proj, aa);
    }
}

/* ---------- ppm dump ---------- */
static void dump_ppm(const char *fn) {
    FILE *f = fopen(fn, "wb");
    if (!f) return;
    fprintf(f, "P6\n%d %d\n255\n", W, H);
    for (int y = 0; y < H; y++) {
        for (int x = 0; x < W; x++) {
            uint16_t p = g_fb[y * g_pitch16 + x];
            uint8_t rgb[3] = {
                (uint8_t)(((p >> 11) & 31) << 3),
                (uint8_t)(((p >> 5) & 63) << 2),
                (uint8_t)((p & 31) << 3)
            };
            fwrite(rgb, 1, 3, f);
        }
    }
    fclose(f);
    fprintf(stderr, "dumped %s\n", fn);
}

/* ---------- timing ---------- */
static double now_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000.0 + ts.tv_nsec / 1e6;
}

static int cmp_dbl(const void *a, const void *b) {
    double x = *(const double *)a, y = *(const double *)b;
    return (x > y) - (x < y);
}

static double g_traster[MAXFRAMES], g_tflip[MAXFRAMES], g_ttotal[MAXFRAMES];

static void report(const char *name, double *v, int n) {
    static double s[MAXFRAMES];
    memcpy(s, v, n * sizeof(double));
    qsort(s, n, sizeof(double), cmp_dbl);
    double sum = 0;
    for (int i = 0; i < n; i++) sum += s[i];
    printf("%-10s n=%d min=%.3f avg=%.3f p50=%.3f p90=%.3f p99=%.3f max=%.3f ms\n",
           name, n, s[0], sum / n, s[n / 2], s[(int)(n * 0.90)],
           s[(int)(n * 0.99)], s[n - 1]);
}

int main(int argc, char **argv) {
    const char *animfn = NULL, *dumpfn = NULL, *mode = "scene";
    int aa = 0, frames = 1200;
    float zoom = 1.0f;
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "-mode")) mode = argv[++i];
        else if (!strcmp(argv[i], "-aa")) aa = atoi(argv[++i]);
        else if (!strcmp(argv[i], "-tol")) g_tol = atof(argv[++i]);
        else if (!strcmp(argv[i], "-zoom")) zoom = atof(argv[++i]);
        else if (!strcmp(argv[i], "-frames")) frames = atoi(argv[++i]);
        else if (!strcmp(argv[i], "-dump")) dumpfn = argv[++i];
        else animfn = argv[i];
    }
    if (!animfn) { fprintf(stderr, "usage: rastbench anim.bin [opts]\n"); return 1; }
    if (frames > MAXFRAMES) frames = MAXFRAMES;
    load_anims(animfn);
    proj_init();

#ifdef HEADLESS
    g_fb = calloc(W * H, 2);
    g_pitch16 = W;
#else
    if (SDL_Init(SDL_INIT_VIDEO) != 0) { fprintf(stderr, "SDL_Init: %s\n", SDL_GetError()); return 1; }
    static const Uint32 flagchain[] = {
        SDL_HWSURFACE | SDL_DOUBLEBUF, SDL_SWSURFACE | SDL_DOUBLEBUF, SDL_SWSURFACE, 0
    };
    for (int i = 0; i < 4 && !g_screen; i++)
        g_screen = SDL_SetVideoMode(W, H, 16, flagchain[i]);
    if (!g_screen) { fprintf(stderr, "SetVideoMode failed: %s\n", SDL_GetError()); return 1; }
    if (g_screen->format->BitsPerPixel != 16) {
        fprintf(stderr, "not 16bpp (%d) — aborting\n", g_screen->format->BitsPerPixel);
        return 1;
    }
    fprintf(stderr, "surface %dx%d 16bpp masks R=%04x G=%04x B=%04x flags=%08x\n",
            g_screen->w, g_screen->h, (unsigned)g_screen->format->Rmask,
            (unsigned)g_screen->format->Gmask, (unsigned)g_screen->format->Bmask,
            (unsigned)g_screen->flags);
    SDL_ShowCursor(SDL_DISABLE);
#endif

    g_col_bg = rgb565(20, 22, 40);
    g_col_stage = rgb565(90, 90, 100);
    g_col_plat = rgb565(140, 140, 150);
    g_col_c1 = rgb565(230, 120, 40);   /* fox orange */
    g_col_c2 = rgb565(60, 90, 220);    /* marth blue */
    g_col_proj = rgb565(255, 60, 60);

    int baseline = !strcmp(mode, "baseline");
    printf("rastbench mode=%s aa=%d tol=%.2f zoom=%.1f frames=%d\n",
           mode, aa, g_tol, zoom, frames);

    for (int f = 0; f < frames; f++) {
        double t0 = now_ms();
#ifndef HEADLESS
        if (SDL_MUSTLOCK(g_screen)) SDL_LockSurface(g_screen);
        g_fb = (uint16_t *)g_screen->pixels;
        g_pitch16 = g_screen->pitch / 2;
#endif
        if (baseline) clear_screen(g_col_bg);
        else render_scene(f, aa, zoom);
        double t1 = now_ms();
#ifndef HEADLESS
        if (SDL_MUSTLOCK(g_screen)) SDL_UnlockSurface(g_screen);
        SDL_Flip(g_screen);
#endif
        double t2 = now_ms();
        g_traster[f] = t1 - t0;
        g_tflip[f] = t2 - t1;
        g_ttotal[f] = t2 - t0;
        if (dumpfn && f == 30) dump_ppm(dumpfn);
#ifndef HEADLESS
        SDL_PumpEvents();   /* keep SDL happy; not timed separately */
#endif
    }
    /* skip first 30 frames (cache warm/paging) */
    int skip = frames > 100 ? 30 : 0;
    report("raster", g_traster + skip, frames - skip);
    report("flip", g_tflip + skip, frames - skip);
    report("total", g_ttotal + skip, frames - skip);

#ifndef HEADLESS
    SDL_Quit();
#endif
    return 0;
}
