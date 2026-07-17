// port/gfx/anim1.c — ANIM1 reader (pipeline/FORMATS.md §2). See anim1.h.
#include "anim1.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint16_t rd16(const uint8_t *p) {
  return (uint16_t)((uint16_t)p[0] | ((uint16_t)p[1] << 8));
}
static uint32_t rd32(const uint8_t *p) {
  return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
         ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

#define HEADER_SIZE 16u
#define DIR_ENTRY_SIZE 12u

static void a1_fail(const char *path, const char *what) {
  fprintf(stderr, "anim1: %s: %s\n", path, what);
  gfx_fatal("ANIM1 artifact violation");
}

void anim1_load(Anim1 *a, const char *path, int expectCharId) {
  FILE *f = fopen(path, "rb");
  if (!f) a1_fail(path, "cannot open");
  if (fseek(f, 0, SEEK_END) != 0) a1_fail(path, "seek failed");
  long sz = ftell(f);
  if (sz < (long)HEADER_SIZE) a1_fail(path, "shorter than the header");
  if (fseek(f, 0, SEEK_SET) != 0) a1_fail(path, "seek failed");
  a->buf = malloc((size_t)sz);
  if (!a->buf) a1_fail(path, "oom");
  if (fread(a->buf, 1, (size_t)sz, f) != (size_t)sz) a1_fail(path, "short read");
  fclose(f);
  a->size = (uint32_t)sz;

  // §2.1 header
  if (memcmp(a->buf, "MLA1", 4) != 0) a1_fail(path, "bad magic");
  if (rd16(a->buf + 4) != 1) a1_fail(path, "bad version");
  a->charId = rd16(a->buf + 6);
  if (expectCharId >= 0 && a->charId != (uint16_t)expectCharId) {
    a1_fail(path, "charId disagrees with the requested character");
  }
  a->stateCount = rd32(a->buf + 8);
  if (rd32(a->buf + 12) != a->size) a1_fail(path, "fileSize != actual bytes");

  // §2.2 directory (validate every offset before use; assert sort order)
  if (HEADER_SIZE + (uint64_t)a->stateCount * DIR_ENTRY_SIZE > a->size) {
    a1_fail(path, "directory exceeds file");
  }
  a->dir = calloc(a->stateCount ? a->stateCount : 1, sizeof(Anim1State));
  if (!a->dir) a1_fail(path, "oom");
  const char *prev = NULL;
  for (uint32_t i = 0; i < a->stateCount; i++) {
    const uint8_t *e = a->buf + HEADER_SIZE + (size_t)i * DIR_ENTRY_SIZE;
    const uint32_t nameOff = rd32(e);
    const uint32_t frameCount = rd32(e + 4);
    const uint32_t framesOff = rd32(e + 8);
    if (nameOff >= a->size) a1_fail(path, "name offset out of range");
    // NUL termination inside the buffer:
    const uint8_t *nul = memchr(a->buf + nameOff, 0, a->size - nameOff);
    if (!nul) a1_fail(path, "unterminated state name");
    const char *name = (const char *)(a->buf + nameOff);
    if (prev && strcmp(prev, name) >= 0) a1_fail(path, "directory not sorted");
    prev = name;
    if ((uint64_t)framesOff + (uint64_t)frameCount * 4u > a->size) {
      a1_fail(path, "frame-offset table out of range");
    }
    a->dir[i].name = name;
    a->dir[i].frameCount = frameCount;
    a->dir[i].framesOff = framesOff;
  }
}

const Anim1State *anim1_find(const Anim1 *a, const char *stateName) {
  uint32_t lo = 0, hi = a->stateCount;
  while (lo < hi) {
    const uint32_t mid = lo + (hi - lo) / 2;
    const int c = strcmp(a->dir[mid].name, stateName);
    if (c == 0) return &a->dir[mid];
    if (c < 0) lo = mid + 1; else hi = mid;
  }
  return NULL;
}

uint32_t anim1_frame(const Anim1 *a, const Anim1State *st,
                     uint32_t frameIdx, uint32_t *cursor, int *absent) {
  if (frameIdx >= st->frameCount) gfx_fatal("anim1: frame index out of range");
  const uint32_t rec = rd32(a->buf + st->framesOff + (size_t)frameIdx * 4u);
  if (rec == 0) { *absent = 1; *cursor = 0; return 0; } // spec §2.4
  *absent = 0;
  if ((uint64_t)rec + 2u > a->size) gfx_fatal("anim1: frame record out of range");
  *cursor = rec + 2u;
  return rd16(a->buf + rec);
}

Anim1Path anim1_next_path(const Anim1 *a, uint32_t *cursor) {
  if ((uint64_t)*cursor + 2u > a->size) gfx_fatal("anim1: path header out of range");
  Anim1Path p;
  p.coordCount = rd16(a->buf + *cursor);
  *cursor += 2u;
  if ((uint64_t)*cursor + (uint64_t)p.coordCount * 2u > a->size) {
    gfx_fatal("anim1: path coords out of range");
  }
  p.coords = a->buf + *cursor;
  *cursor += p.coordCount * 2u;
  return p;
}
