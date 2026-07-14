/*
 * csweep.c — C side of the fdlibm crosscheck.
 *
 * Reads lines "<fn> <hex16> [<hex16>]" (IEEE-754 bit patterns), applies
 * the vendored port/fdlibm implementation, writes
 * "<fn> <hex16> [<hex16>] -> <hex16>" per line.
 *
 * Build (host): cc -O2 -ffp-contract=off -std=c99 -I../../port/fdlibm \
 *   csweep.c ../../port/fdlibm/fdlibm.c -o out/csweep
 *
 * Usage: csweep <inputs.txt> > outputs.txt
 */
#include <inttypes.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include "fdlibm.h"

static double u2d(uint64_t u) {
  double d;
  memcpy(&d, &u, 8);
  return d;
}

static uint64_t d2u(double d) {
  uint64_t u;
  memcpy(&u, &d, 8);
  return u;
}

int main(int argc, char **argv) {
  if (argc != 2) {
    fprintf(stderr, "usage: csweep <inputs.txt>\n");
    return 2;
  }
  FILE *f = fopen(argv[1], "r");
  if (!f) {
    fprintf(stderr, "cannot open %s\n", argv[1]);
    return 2;
  }
  char fn[16];
  char line[128];
  long n = 0;
  while (fgets(line, sizeof line, f)) {
    uint64_t a = 0, b = 0;
    int got = sscanf(line, "%15s %" SCNx64 " %" SCNx64, fn, &a, &b);
    if (got < 2) continue; /* blank/garbage line */
    double x = u2d(a), r;
    if (strcmp(fn, "sin") == 0) {
      r = fd_sin(x);
    } else if (strcmp(fn, "cos") == 0) {
      r = fd_cos(x);
    } else if (strcmp(fn, "tan") == 0) {
      r = fd_tan(x);
    } else if (strcmp(fn, "atan") == 0) {
      r = fd_atan(x);
    } else if (strcmp(fn, "atan2") == 0) {
      if (got != 3) { fprintf(stderr, "atan2 needs 2 args: %s", line); return 2; }
      r = fd_atan2(x, u2d(b));
    } else if (strcmp(fn, "pow") == 0) {
      if (got != 3) { fprintf(stderr, "pow needs 2 args: %s", line); return 2; }
      r = fd_pow(x, u2d(b));
    } else {
      fprintf(stderr, "unknown fn: %s", line);
      return 2;
    }
    if (got == 3) {
      printf("%s %016" PRIx64 " %016" PRIx64 " -> %016" PRIx64 "\n", fn, a, b, d2u(r));
    } else {
      printf("%s %016" PRIx64 " -> %016" PRIx64 "\n", fn, a, d2u(r));
    }
    n++;
  }
  fclose(f);
  fprintf(stderr, "csweep: %ld evaluations\n", n);
  return 0;
}
