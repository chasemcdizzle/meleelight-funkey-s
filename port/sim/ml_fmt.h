// ml_fmt.h — ECMAScript Number::toString(x, 10) in C (M2 task 15).
// The exact `String(x)` byte semantics the checksum serializer needs
// (oracle/CHECKSUM.md §3.4): shortest round-trip decimal, injective on
// doubles; integers without ".0"; exponent form exactly where ECMA-262
// mandates it (n > 21 or n <= -6); "NaN"/"Infinity"/"-Infinity" tokens.
// NOTE: String(-0) === "0" (the sign is erased) — the checksum spec's
// explicit "-0" token lives one layer up, in ml_ser.h's ml_sb_num
// (pagelib.js:10-13).
//
// Digits + decimal exponent come from the vendored Ryu core
// (port/ryu/, Ulf Adams, Apache-2.0 OR BSL-1.0 — see NOTICES); the
// ECMA-262 §6.1.6.1.20 formatting layer on top is ours.
#ifndef ML_FMT_H
#define ML_FMT_H

#include <stddef.h>

// Maximum output length: "-2.2250738585072014e-308" = 24 chars + NUL.
#define ML_FMT_DTOA_MAX 32

// Writes String(x) into buf (NUL-terminated), returns the length.
// buf must have room for ML_FMT_DTOA_MAX bytes.
int ml_fmt_dtoa(double x, char *buf);

#endif // ML_FMT_H
