// port/gfx/img1_check.c — host self-check for the IMG1 loader (M4 task A9).
//
// Two proofs, both against a REAL emitted artifact (never a synthetic one):
//
//   --dump <file>   canonical text dump of every image's pixels. Compared
//                   byte-for-byte by check-assets.sh against the independent
//                   JS reader (pipeline/lib/img1.js dumpImg1) — i.e. the C
//                   parser is judged by a second implementation, not by
//                   itself. A malformed file must make this exit nonzero,
//                   which is also how the corruption teeth are checked.
//
//   --blit <file>   an exhaustive 565 unpack/pack round trip over all 65536
//                   codes, then img1_blit vs the raster's OWN rast_blit_rgba on the
//                   equivalent RGBA sprite: same image, same offsets, two
//                   Rasters, memcmp of both the 565 framebuffer AND the ink
//                   plane. img1_blit claims to be bit-identical to the
//                   existing blitter rather than new blend math; this is
//                   what makes that claim measured.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "img1.h"
#include "raster.h"

void gfx_fatal(const char *what) {
  fprintf(stderr, "img1_check: gfx_fatal: %s\n", what ? what : "(null)");
  exit(2);
}

static Raster g_a, g_b;

static int dump(const Img1Set *set) {
  printf("IMG1 count=%d\n", set->count);
  for (int i = 0; i < set->count; i++) {
    const Img1Image *im = &set->img[i];
    printf("img %d %s %d %d\n", i, im->name, im->w, im->h);
    for (int y = 0; y < im->h; y++) {
      printf("row %d %d ", i, y);
      for (int x = 0; x < im->w; x++) {
        printf("%04x", (unsigned)im->rgb565[(size_t)y * im->w + x]);
      }
      putchar(' ');
      for (int x = 0; x < im->w; x++) {
        printf("%02x", (unsigned)im->a8[(size_t)y * im->w + x]);
      }
      putchar('\n');
    }
  }
  return 0;
}

// Offsets exercise: origin, an interior offset, a negative (left/top
// clipped) origin, an origin running off the right/bottom edges, and two
// that straddle the narrow band's edges (band [24,200): y=14 crosses its
// top, y=190 crosses its bottom) — review-a9-1 [M].
static const int OFFS[][2] = { { 0, 0 }, { 37, 21 }, { -9, -13 }, { 210, 228 },
                               { 40, 14 }, { 40, 190 } };
// Clip bands: full screen, and a band that cuts images top and bottom.
static const int BANDS[][2] = { { 0, RAST_H }, { 24, 200 } };

// The lookup API is shipped, advertised by FORMATS.md §7.1 as the
// consumer's access path, and was covered by NOTHING: a deliberately broken
// img1_find passed every other arm of the check (review-a9-1 [M] /
// independent review [M]). Both directions, plus the out-of-domain answers.
static int lookup_api(const Img1Set *set) {
  for (int i = 0; i < set->count; i++) {
    const Img1Image *want = &set->img[i];
    if (img1_at(set, i) != want) {
      fprintf(stderr, "img1_check: img1_at(%d) != entry %d\n", i, i);
      return 1;
    }
    if (img1_find(set, want->name) != want) {
      fprintf(stderr, "img1_check: img1_find(\"%s\") != entry %d\n", want->name, i);
      return 1;
    }
  }
  if (img1_find(set, "nope_") != NULL || img1_find(set, "") != NULL ||
      img1_find(set, NULL) != NULL || img1_find(NULL, "marth") != NULL) {
    fprintf(stderr, "img1_check: img1_find accepted an absent name or NULL set\n");
    return 1;
  }
  // A prefix of a real name must NOT resolve (guards prefix-compare bugs).
  char pfx[IMG1_NAME_MAX + 1];
  snprintf(pfx, sizeof(pfx), "%s", set->img[0].name);
  if (strlen(pfx) > 1) {
    pfx[strlen(pfx) - 1] = '\0';
    if (img1_find(set, pfx) != NULL) {
      fprintf(stderr, "img1_check: img1_find resolved a NAME PREFIX \"%s\"\n", pfx);
      return 1;
    }
  }
  if (img1_at(set, -1) != NULL || img1_at(set, set->count) != NULL ||
      img1_at(NULL, 0) != NULL) {
    fprintf(stderr, "img1_check: img1_at accepted an out-of-range index\n");
    return 1;
  }
  printf("lookup api: %d names resolve both directions; absent/prefix/"
         "out-of-range all NULL\n", set->count);
  return 0;
}

// EXHAUSTIVE 565 round trip. img1_blit's correctness rests on "unpacking
// 565 to 888 by bit replication round-trips through pack565 exactly" — an
// all-values claim, so it is checked over all values rather than argued.
// pack565 is static to raster.c, so this drives the REAL function through
// its public opaque path: rast_blend_px with a==256 stores pack565(col)
// verbatim, so the framebuffer pixel must come back as the original code.
static int roundtrip565(void) {
  for (unsigned c = 0; c <= 0xffff; c++) {
    const unsigned r5 = c >> 11, g6 = (c >> 5) & 0x3fu, b5 = c & 0x1fu;
    RastCol col;
    col.r = (uint8_t)((r5 << 3) | (r5 >> 2));
    col.g = (uint8_t)((g6 << 2) | (g6 >> 4));
    col.b = (uint8_t)((b5 << 3) | (b5 >> 2));
    col.a256 = 256;
    rast_clear(&g_a, 0, 0, 0, 0, RAST_H);
    rast_blend_px(&g_a, 1, 1, col, 256);
    if (g_a.fb[1 * RAST_W + 1] != (uint16_t)c) {
      fprintf(stderr, "img1_check: 565 ROUND-TRIP FAIL: %04x -> rgb(%u,%u,%u) -> %04x\n",
              c, col.r, col.g, col.b, (unsigned)g_a.fb[1 * RAST_W + 1]);
      return 1;
    }
  }
  printf("565 round trip: all 65536 codes survive unpack -> pack565\n");
  return 0;
}

static int blit_equiv(const Img1Set *set) {
  int cases = 0, drew = 0;
  for (int i = 0; i < set->count; i++) {
    const Img1Image *im = &set->img[i];
    uint8_t *rgba = (uint8_t *)malloc((size_t)im->w * im->h * 4);
    if (!rgba) { fprintf(stderr, "img1_check: out of memory\n"); return 1; }
    for (size_t k = 0; k < (size_t)im->w * im->h; k++) {
      const uint16_t c = im->rgb565[k];
      const unsigned r5 = c >> 11, g6 = (c >> 5) & 0x3fu, b5 = c & 0x1fu;
      rgba[k * 4 + 0] = (uint8_t)((r5 << 3) | (r5 >> 2));
      rgba[k * 4 + 1] = (uint8_t)((g6 << 2) | (g6 >> 4));
      rgba[k * 4 + 2] = (uint8_t)((b5 << 3) | (b5 >> 2));
      rgba[k * 4 + 3] = im->a8[k];
    }
    // Ink suppression is a real render mode (raster.h: the background art
    // pass brackets it), so equivalence is proven with ink BOTH ways —
    // an ink-off regression used to survive (review-a9-1 [M]).
    for (int ink = 1; ink >= 0; ink--) {
    rast_ink_enable(ink);
    for (size_t b = 0; b < sizeof(BANDS) / sizeof(BANDS[0]); b++) {
      for (size_t o = 0; o < sizeof(OFFS) / sizeof(OFFS[0]); o++) {
        rast_clear(&g_a, 30, 40, 50, BANDS[b][0], BANDS[b][1]);
        rast_clear(&g_b, 30, 40, 50, BANDS[b][0], BANDS[b][1]);
        img1_blit(&g_a, im, OFFS[o][0], OFFS[o][1]);
        rast_blit_rgba(&g_b, rgba, im->w, im->h, OFFS[o][0], OFFS[o][1]);
        if (memcmp(g_a.fb, g_b.fb, sizeof(g_a.fb)) != 0 ||
            memcmp(g_a.ink, g_b.ink, sizeof(g_a.ink)) != 0) {
          fprintf(stderr, "img1_check: BLIT MISMATCH image %s at (%d,%d) "
                  "band [%d,%d) ink=%d\n", im->name, OFFS[o][0], OFFS[o][1],
                  BANDS[b][0], BANDS[b][1], ink);
          free(rgba);
          rast_ink_enable(1);
          return 1;
        }
        cases++;
        // NON-VACUITY: agreeing on "nothing happened" proves nothing. At
        // the origin, full clip band, every image must actually change the
        // framebuffer AND set ink — so a no-op img1_blit could not pass by
        // matching a no-op comparison.
        if (ink == 1 && b == 0 && o == 0) {
          Raster ref;
          rast_clear(&ref, 30, 40, 50, BANDS[b][0], BANDS[b][1]);
          if (memcmp(g_a.fb, ref.fb, sizeof(ref.fb)) == 0 ||
              memcmp(g_a.ink, ref.ink, sizeof(ref.ink)) == 0) {
            fprintf(stderr, "img1_check: VACUOUS BLIT: image %s drew nothing "
                    "at the origin\n", im->name);
            free(rgba);
            rast_ink_enable(1);
            return 1;
          }
          drew++;
        }
      }
    }
    } // ink
    rast_ink_enable(1);
    free(rgba);
  }
  if (drew != set->count) {
    fprintf(stderr, "img1_check: only %d/%d images proved non-vacuous\n",
            drew, set->count);
    return 1;
  }
  printf("blit-equivalence: %d image/offset/clip cases bit-identical to "
         "rast_blit_rgba (%d/%d images proved non-vacuous)\n",
         cases, drew, set->count);
  return 0;
}

int main(int argc, char **argv) {
  if (argc != 3 || (strcmp(argv[1], "--dump") != 0 && strcmp(argv[1], "--blit") != 0)) {
    fprintf(stderr, "usage: img1_check --dump|--blit <file.img1>\n");
    return 2;
  }
  Img1Set set = { 0 }; // img1.h precondition: zeroed or previously closed
  if (img1_open(&set, argv[2]) != 0) {
    fprintf(stderr, "img1_check: %s\n", img1_error());
    return 1;
  }
  const int rc = (strcmp(argv[1], "--dump") == 0)
    ? dump(&set)
    : (lookup_api(&set) || roundtrip565() || blit_equiv(&set));
  img1_close(&set);
  return rc;
}
