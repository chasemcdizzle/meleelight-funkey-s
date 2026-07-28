// port/gfx/img1.c — IMG1 loader + blitter. Spec: pipeline/FORMATS.md §7.
#include "img1.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define IMG1_HDR 12
#define IMG1_DIR 24

static const char *g_err = "";
const char *img1_error(void) { return g_err; }

static int fail(Img1Set *set, const char *why) {
  g_err = why;
  if (set) { free(set->blob); free(set->img); memset(set, 0, sizeof(*set)); }
  return -1;
}

static uint16_t rd16(const uint8_t *p) { return (uint16_t)(p[0] | (p[1] << 8)); }
static uint32_t rd32(const uint8_t *p) {
  return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) |
         ((uint32_t)p[3] << 24);
}

int img1_open(Img1Set *set, const char *path) {
  if (!set) return -1;
  // img1_close, not memset: reopening an already-open set used to drop its
  // owned pointers on the floor (review-a9-1 [M]). img1_close memsets after
  // freeing, so a zeroed or previously-closed set is handled identically —
  // see img1.h for the precondition this relies on.
  img1_close(set);
  if (!path) return fail(set, "img1_open: NULL path");

  // Pixel planes are read as uint16_t* (FORMATS.md §0: little-endian
  // always, C may read directly). Say so out loud rather than silently
  // producing swapped colours if this is ever built for a BE target.
  const uint16_t probe = 1;
  if (*(const uint8_t *)&probe != 1) return fail(set, "img1_open: big-endian host");

  FILE *f = fopen(path, "rb");
  if (!f) return fail(set, "img1_open: cannot open file");
  if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return fail(set, "img1_open: seek failed"); }
  const long len = ftell(f);
  if (len < IMG1_HDR || len > (long)64 * 1024 * 1024) {
    fclose(f);
    return fail(set, "img1_open: implausible file size");
  }
  rewind(f);
  uint8_t *blob = (uint8_t *)malloc((size_t)len);
  if (!blob) { fclose(f); return fail(set, "img1_open: out of memory"); }
  const size_t got = fread(blob, 1, (size_t)len, f);
  fclose(f);
  if (got != (size_t)len) { free(blob); return fail(set, "img1_open: short read"); }
  set->blob = blob;
  set->bytes = (size_t)len;

  if (memcmp(blob, "IMG1", 4) != 0) return fail(set, "img1_open: bad magic");
  const uint32_t count = rd32(blob + 4), total = rd32(blob + 8);
  if (total != (uint32_t)len) return fail(set, "img1_open: header size != file size");
  if (count == 0 || count > 4096) return fail(set, "img1_open: implausible image count");
  if ((uint64_t)IMG1_HDR + (uint64_t)count * IMG1_DIR > total) {
    return fail(set, "img1_open: directory overruns file");
  }

  Img1Image *img = (Img1Image *)calloc(count, sizeof(Img1Image));
  if (!img) return fail(set, "img1_open: out of memory");
  set->img = img;
  set->count = (int)count;

  // Every §7.1 grammar rule is enforced, not just the ones that would
  // crash us (review-a9-1 [M]): a file that parses into plausible-looking
  // wrong images is exactly as bad as one that reads out of bounds.
  const uint32_t dirEnd = IMG1_HDR + count * IMG1_DIR;
  uint64_t prevEnd = dirEnd;
  for (uint32_t i = 0; i < count; i++) {
    const uint8_t *d = blob + IMG1_HDR + (size_t)i * IMG1_DIR;
    if (memchr(d, 0, IMG1_NAME_MAX + 1) == NULL) {
      return fail(set, "img1_open: name field not NUL-terminated");
    }
    memcpy(img[i].name, d, IMG1_NAME_MAX + 1);
    if (img[i].name[0] == '\0') return fail(set, "img1_open: empty image name");
    for (const char *p = img[i].name; *p; p++) {
      if (!((*p >= 'a' && *p <= 'z') || (*p >= '0' && *p <= '9') || *p == '_')) {
        return fail(set, "img1_open: image name outside [a-z0-9_]");
      }
    }
    // Everything after the terminator must be zero padding, so two files
    // cannot differ in bytes yet present identical names.
    for (size_t k = strlen(img[i].name) + 1; k <= IMG1_NAME_MAX; k++) {
      if (d[k] != 0) return fail(set, "img1_open: non-zero bytes after name terminator");
    }
    for (uint32_t j = 0; j < i; j++) {
      if (strcmp(img[i].name, img[j].name) == 0) {
        return fail(set, "img1_open: duplicate image name");
      }
    }
    const uint32_t w = rd16(d + 16), h = rd16(d + 18), off = rd32(d + 20);
    if (w == 0 || h == 0) return fail(set, "img1_open: zero-sized image");
    if (off % 4 != 0) return fail(set, "img1_open: pixel data not 4-aligned");
    // Blocks must follow the directory and must not overlap each other —
    // otherwise dataOff 0 would alias the header as pixels, and two images
    // could silently share bytes.
    if (off < dirEnd) return fail(set, "img1_open: pixel data overlaps the directory");
    if ((uint64_t)off < prevEnd) return fail(set, "img1_open: overlapping image blocks");
    // 64-bit arithmetic: w*h*3 cannot overflow it (w,h <= 65535).
    const uint64_t need = (uint64_t)w * h * 3;
    if ((uint64_t)off + need > total) return fail(set, "img1_open: image data overruns file");
    prevEnd = (uint64_t)off + need;
    img[i].w = (int)w;
    img[i].h = (int)h;
    img[i].rgb565 = (const uint16_t *)(const void *)(blob + off);
    img[i].a8 = blob + off + (size_t)w * h * 2;
  }
  g_err = "";
  return 0;
}

void img1_close(Img1Set *set) {
  if (!set) return;
  free(set->blob);
  free(set->img);
  memset(set, 0, sizeof(*set));
}

const Img1Image *img1_find(const Img1Set *set, const char *name) {
  if (!set || !name) return NULL;
  for (int i = 0; i < set->count; i++) {
    if (strcmp(set->img[i].name, name) == 0) return &set->img[i];
  }
  return NULL;
}

const Img1Image *img1_at(const Img1Set *set, int index) {
  if (!set || index < 0 || index >= set->count) return NULL;
  return &set->img[index];
}

// ponytail: per-pixel rast_blend_px calls from this -O2 TU, deliberately —
// bit-identity with rast_blit_rgba is worth more than speed until speed is
// measured. KNOWN CEILING (review-a9-1 [M]): raster.c moved the equivalent
// glyph/sprite loops into its -O3 TU as batch primitives after profiling
// (raster.c "M4 task 3, measured-hotspot class fix"). UPGRADE PATH: add a
// rast_blit_565a8 batch primitive next to rast_blit_rgba and call it here.
// Not done now because (a) this task may not edit raster.c, and (b) nothing
// draws these images yet — the menu path is not the 60 fps match path, and
// the whole set is 24k pixels. Measure when the FOH actually blits them.
void img1_blit(Raster *rz, const Img1Image *im, int x0, int y0) {
  if (!rz || !im) return;
  for (int y = 0; y < im->h; y++) {
    const int py = y0 + y;
    if (py < rz->clipY0 || py >= rz->clipY1) continue; // rast_blend_px re-checks
    const size_t row = (size_t)y * (size_t)im->w;
    for (int x = 0; x < im->w; x++) {
      const uint8_t a8 = im->a8[row + (size_t)x];
      if (!a8) continue;
      const uint16_t c = im->rgb565[row + (size_t)x];
      // 565 -> 888 by bit replication: pack565() of the result is exactly
      // `c` again, so no colour drifts through the round trip (proven for
      // all 65536 codes by img1_check --blit). MEASURED: plain truncation
      // would satisfy that round trip too — the replicated low bits are not
      // blit-observable — but this is the correct expansion for any future
      // reader of the 888 value, so it is what we store.
      const unsigned r5 = c >> 11, g6 = (c >> 5) & 0x3fu, b5 = c & 0x1fu;
      RastCol col;
      col.r = (uint8_t)((r5 << 3) | (r5 >> 2));
      col.g = (uint8_t)((g6 << 2) | (g6 >> 4));
      col.b = (uint8_t)((b5 << 3) | (b5 >> 2));
      col.a256 = 256;
      // Same alpha conversion as rast_blit_rgba / rast_blit_a8mask.
      rast_blend_px(rz, x0 + x, py, col, ((unsigned)a8 * 256u) / 255u);
    }
  }
}
