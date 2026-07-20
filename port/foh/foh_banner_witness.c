// foh_banner_witness.c — the finish-banner glyph-coverage witness (M4
// micro, iter 103; review-101 round-2 M; AGENT-LOG iter-103
// pre-registration). A check-owned host leg for check-foh-flows.sh.
//
// WHY: gfx_target_banner draws COMPLETE!/FAILURE. Before iter 103 it used
// GFX_FONT_T40, whose atlas glyphs are digits + ':' only, so the FIRST
// real finish would hit gfx_overlay.c's FATAL missing-glyph path and
// ABORT (the bug is latent — no committed device leg reaches the finish
// seam; foh_dev.c draws the banner only when g_tfin_fired, which
// check-device-target.sh's assert_no_tfinish forbids on every green leg).
// The fix routes the banner text through the self-authored FOH 5x7 font
// (gfx_target_banner_text, gfx_target.c). This witness drives that REAL
// banner-text function for BOTH reachable strings and proves the
// missing-glyph fatal is structurally unreachable for them.
//
// LINK (light, check-foh-flows.sh): gfx_target.o + raster.o + foh_font.o,
// with -Wl,-dead_strip dropping the unreferenced scene-frame draws
// (gfx_target_frame/banner/init and their gfx_render_* deps). A revert of
// gfx_target_banner_text to gfx_glyph_text would leave that symbol
// unresolved here (no atlas is linked) -> a LINK FAILURE, so the font
// choice is guarded, not merely the strings.
//
// TOOTH (check-side, on a COPY of gfx_target.c): a banner string carrying
// a genuinely-missing glyph must still die FATAL in foh_font -> this same
// witness binary, rebuilt against the perturbed copy, exits non-zero
// BEFORE "BANNER WITNESS OK". The guard stays (HARD RULE 2); only the
// reachable strings are proven covered.
//
// Usage: foh_banner_witness [--shot-dir <dir>]
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../gfx/gfx_target.h" // gfx_target_banner_text + Raster (via gfx.h)

// gfx_fatal is host-provided (raster.h). On the CONTROL run neither
// reachable string reaches it; on the TOOTH build the missing glyph does
// -> a loud non-zero exit (the tooth's whole point).
void gfx_fatal(const char *what) {
  fprintf(stderr, "foh_banner_witness: gfx_fatal: %s\n", what);
  exit(3);
}

static long render_banner(Raster *rz, int complete) {
  rast_clear(rz, 0, 0, 0, 0, RAST_H); // black fb, full-height clip band
  gfx_target_banner_text(rz, complete); // THE REAL banner-text path
  long ink = 0;
  for (int i = 0; i < RAST_W * RAST_H; i++) {
    if (rz->fb[i] != 0) ink++; // any non-black pixel = drawn
  }
  return ink;
}

static void dump_pgm(const Raster *rz, const char *path) {
  FILE *f = fopen(path, "wb");
  if (!f) { fprintf(stderr, "witness: cannot open shot %s\n", path); exit(2); }
  fprintf(f, "P5\n%d %d\n255\n", RAST_W, RAST_H);
  for (int i = 0; i < RAST_W * RAST_H; i++) {
    // 565 -> 8-bit luma-ish: any non-black pixel is a lit banner texel.
    const unsigned char v = rz->fb[i] ? 255u : 0u;
    if (fputc(v, f) == EOF) { fprintf(stderr, "witness: shot write failed\n"); exit(2); }
  }
  if (fclose(f) != 0) { fprintf(stderr, "witness: shot close failed\n"); exit(2); }
}

int main(int argc, char **argv) {
  const char *shotDir = 0;
  for (int i = 1; i < argc; i++) {
    if (strcmp(argv[i], "--shot-dir") == 0 && i + 1 < argc) shotDir = argv[++i];
    else { fprintf(stderr, "witness: bad argument %s\n", argv[i]); return 2; }
  }

  static Raster rzComplete, rzFailure;
  // Drive the REAL banner-text render for BOTH reachable strings. If
  // either carried a glyph the FOH font lacks, gfx_fatal() would fire
  // here and this process would exit(3) before the verdict.
  const long inkComplete = render_banner(&rzComplete, 1); // "COMPLETE!"
  const long inkFailure = render_banner(&rzFailure, 0);   // "FAILURE"

  int fail = 0;
  if (inkComplete <= 0) {
    fprintf(stderr, "witness FAIL: COMPLETE! banner drew no ink\n");
    fail = 1;
  }
  if (inkFailure <= 0) {
    fprintf(stderr, "witness FAIL: FAILURE banner drew no ink\n");
    fail = 1;
  }
  // Distinctness: the two strings must not render identically (they
  // differ in both length and letters) — a stubbed/blank draw that
  // produced two equal buffers would be caught here.
  int same = 1;
  for (int i = 0; i < RAST_W * RAST_H; i++) {
    if (rzComplete.fb[i] != rzFailure.fb[i]) { same = 0; break; }
  }
  if (same) {
    fprintf(stderr, "witness FAIL: COMPLETE! and FAILURE rendered identically\n");
    fail = 1;
  }

  if (shotDir) {
    char path[1024];
    snprintf(path, sizeof path, "%s/banner-complete.pgm", shotDir);
    dump_pgm(&rzComplete, path);
    snprintf(path, sizeof path, "%s/banner-failure.pgm", shotDir);
    dump_pgm(&rzFailure, path);
  }

  if (fail) {
    fprintf(stderr, "BANNER WITNESS FAIL\n");
    return 1;
  }
  printf("BANNER WITNESS OK (complete_ink=%ld failure_ink=%ld distinct=yes)\n",
         inkComplete, inkFailure);
  return 0;
}
