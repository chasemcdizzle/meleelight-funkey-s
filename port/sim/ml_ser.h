// ml_ser.h — CHECKSUM.md §3 serialization primitives + §4 hash in C
// (M2 task 15). The byte-exact counterpart of the oracle's `ser`
// (oracle/harness/pagelib.js:10-37) and `__sha256`
// (oracle/harness/pagelib.js:66-73), for the C sim's checksum-stream
// emission (fix_plan §M2 task 17).
//
// This header provides the LEAF emitters plus a growable byte buffer;
// the tree walk itself is the caller's (pagelib's __serializeState,
// pagelib.js:41-64, hand-builds the envelope the same way):
//   - numbers   -> ml_sb_num      (§3.4: String(x) via ml_fmt_dtoa,
//                                  EXCEPT -0 which serializes as the
//                                  explicit token "-0" — the
//                                  Object.is(v, -0) check of
//                                  pagelib.js:10-13; NaN/Infinity/
//                                  -Infinity fall out of String(x))
//   - strings   -> ml_sb_jsonstr  (§3.5: JSON.stringify — double-quoted,
//                                  JSON escaping; pagelib.js:18)
//   - booleans  -> ml_sb_bool     (§3.6: unquoted T / F; pagelib.js:19)
//   - null      -> ml_sb_puts(b, "null")   (§3.7, pagelib.js:15)
//   - undefined -> ml_sb_puts(b, "undef")  (§3.7, pagelib.js:20)
//   - functions -> ml_sb_puts(b, "fn")     (§3.8, pagelib.js:21)
//   - cycles    -> ml_sb_puts(b, "cyc")    (§3.9, pagelib.js:22)
//   - arrays / typed arrays: "[" elements ","-joined "]" (§3.3,
//     pagelib.js:25-28); nested objects: own enumerable keys SORTED
//     (byte-wise — all domain keys are ASCII), each JSON.stringify(key)
//     ":" value, ","-joined inside "{...}" (§3.2, pagelib.js:30-33).
//   - THE ENVELOPE keys are fixed-literal order, NOT sorted (§3.1,
//     pagelib.js:50-62): active players in slot order p0..p3, then
//     "articles"; inside each player block the literal order
//     actionState, timer, percent, stocks, hit, hitboxes, phys.
//
// Hash (§4): SHA-256 over the UTF-8 bytes of the serialized string,
// lowercase hex — ml_sha256_hex links oracle/qjs/sha256.c (FIPS 180-4,
// self-tested; the same TU the QuickJS oracle runtime uses).
#ifndef ML_SER_H
#define ML_SER_H

#include <stdbool.h>
#include <stddef.h>

typedef struct {
  char *buf; // NUL-terminated
  size_t len, cap;
} MlSb;

void ml_sb_init(MlSb *b);
void ml_sb_free(MlSb *b);
void ml_sb_reset(MlSb *b); // len = 0, keeps the allocation

void ml_sb_putc(MlSb *b, char c);
void ml_sb_puts(MlSb *b, const char *s);
void ml_sb_write(MlSb *b, const char *s, size_t n);

// CHECKSUM.md §3.4 (pagelib.js numStr, pagelib.js:10-13): "-0" for
// negative zero, String(x) for everything else (incl. "NaN",
// "Infinity", "-Infinity").
void ml_sb_num(MlSb *b, double x);

// CHECKSUM.md §3.5: JSON.stringify(s) for a UTF-8 string (ECMA-262
// QuoteJSONString): escapes `"` `\` \b \t \n \f \r and other control
// chars < 0x20 as \u00xx (lowercase hex); bytes >= 0x20 pass through
// verbatim. (Domain strings are ASCII without escapes — canon.c's
// parser hard-fails on backslash — but the full rule is implemented.)
void ml_sb_jsonstr(MlSb *b, const char *s);

// CHECKSUM.md §3.6: T / F.
void ml_sb_bool(MlSb *b, bool v);

// CHECKSUM.md §4: lowercase-hex SHA-256 of `len` bytes; out_hex gets 64
// hex chars + NUL.
void ml_sha256_hex(const void *data, size_t len, char out_hex[65]);

#endif // ML_SER_H
