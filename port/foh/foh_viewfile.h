// port/foh/foh_viewfile.h — a screen's VIEW STATE, on the card, beside the
// player's save.
//
// WHY THIS EXISTS AS A SHARED THING (2026-09-01).
//
// The persist plane (foh_persist.h) is a field TABLE: every row declared by
// hand, domain-checked, and pinned by a positional device whitelist. That is
// the right shape for settings and records — a few dozen values that other
// screens read and that must survive a format migration.
//
// It is the wrong shape for a screen's own view. The target builder needed
// ~30 scalars plus 33 polygon points; the credits need 14 scrolling names,
// their shot flags, 100 stars and 16 shots — about 700 values that mean
// nothing to any other screen and that no migration will ever have to
// reconcile. Ninety rows of hand-declared table for one screen, and seven
// hundred for another, is a plane collapsing under its own ceremony.
//
// So a screen writes its own file, and the FILE MECHANICS live here: header,
// checksum, atomic publish, bounded read, verify-before-parse, consume. The
// FIELD LIST stays with the screen, because that is the part only the screen
// knows. D62 (the builder) was the first instance and had all of this inline;
// the credits were the second, which is what made it a class.
//
// THE RULES, inherited whole from the .mlstage contract:
//   * INTEGRITY BEFORE MEANING. The SUM is verified before a byte is parsed.
//   * EVERY REFUSAL NAMES ITS RULE, and leaves the caller's state untouched —
//     a screen parses into scratch and commits only on success.
//   * CONSUMED AT RESUME. The file means "where you were when the lid closed",
//     so a later ordinary visit must not resurrect it, and an unreadable one
//     must not be retried on every boot.
//   * A VIEW IS NEVER LOAD-BEARING. Losing it costs a cursor, never the
//     player's work; callers are expected to treat a write failure as
//     non-fatal and a read failure as "start fresh".
#ifndef FOH_VIEWFILE_H
#define FOH_VIEWFILE_H

#include <stdbool.h>
#include <stddef.h>

// Big enough for the credits' ~700 values at 17 bytes a row, with headroom.
#define FOH_VIEW_MAX 32768

typedef struct {
  char buf[FOH_VIEW_MAX];
  size_t n;
  bool overflow; // sticky: one check at publish instead of at every put
} FohViewOut;

// --- writing ---------------------------------------------------------------
void foh_view_begin(FohViewOut *o, const char *magic);
void foh_view_put_int(FohViewOut *o, const char *key, int v);
void foh_view_put_double(FohViewOut *o, const char *key, double v);
// Indexed rows: `<key> <i> <v>`, for arrays. The index is written so a
// transposed or missing row is caught by position on read, not by count.
void foh_view_put_int_at(FohViewOut *o, const char *key, int i, int v);
void foh_view_put_double_at(FohViewOut *o, const char *key, int i, double v);
// Seal with a SUM line and publish atomically. False + *why on failure.
bool foh_view_publish(FohViewOut *o, const char *name, const char **why);

// --- reading ---------------------------------------------------------------
typedef struct {
  char buf[FOH_VIEW_MAX + 1];
  const char *p;   // the read cursor, into buf
  const char *end; // one past the last body byte (the SUM line's start)
  bool bad;        // sticky: one check at the end instead of at every get
  const char *why; // the first rule that refused, never overwritten
} FohViewIn;

// Read, bound, and VERIFY THE SUM. Only then is `p` positioned at the body.
bool foh_view_load(FohViewIn *in, const char *name, const char *magic,
                   const char **why);
int foh_view_get_int(FohViewIn *in, const char *key, int lo, int hi);
double foh_view_get_double(FohViewIn *in, const char *key);
int foh_view_get_int_at(FohViewIn *in, const char *key, int i, int lo, int hi);
double foh_view_get_double_at(FohViewIn *in, const char *key, int i);
// True iff every get succeeded AND the body was consumed exactly. Trailing
// bytes are a refusal: a file with more in it than we read is not our file.
bool foh_view_ok(FohViewIn *in, const char **why);
// Delete it. Failures other than "already gone" are reported, never silent.
void foh_view_consume(const char *name);

#endif
