"use strict";
// Deterministic manifest helpers (pipeline/FORMATS.md §1).
// No timestamps, no absolute paths, recursively sorted keys.

const crypto = require("crypto");
const fs = require("fs");

function sha256(bufOrStr) {
  return crypto.createHash("sha256").update(bufOrStr).digest("hex");
}

function sha256File(path) {
  return sha256(fs.readFileSync(path));
}

// Recursively sort object keys so JSON.stringify is order-independent.
//
// CANONICALIZATION MUST BE INJECTIVE (iter 117, review-116-plib-1 [M]6).
// Two ways it was not: an own "__proto__" key on a `{}` accumulator sets
// the temporary object's PROTOTYPE instead of a own property and then
// vanishes from the output (null-prototype accumulator closes it), and
// JSON.stringify silently collapses values that are not in the manifest's
// value domain at all — NaN/Infinity -> null, -0 -> 0, undefined array
// elements -> null, functions/symbols -> dropped or null. A stage that
// emitted any of those would hash as if it had emitted something else.
// The domain is now asserted rather than coerced: genuine manifest data
// (strings, safe finite numbers, booleans, null, arrays, plain objects)
// is untouched, so this is byte-identical on every real run and a hard
// throw on anything outside it.
function sortValue(v, where) {
  const at = where || "$";
  if (Array.isArray(v)) {
    // `.map` SKIPS holes, so Array(1) and [null] serialized identically
    // (review-117-plib-1 [M]); extra own properties on an array vanish
    // entirely. Both are rejected rather than collapsed.
    const own = Object.keys(v);
    if (own.length !== v.length || own.some((k, i) => k !== String(i))) {
      throw new Error(`manifest: array at ${at} has holes or non-index own properties`);
    }
    return v.map((e, i) => {
      if (e === undefined) {
        throw new Error(`manifest: undefined array element at ${at}[${i}] (sparse/undefined is not in the manifest value domain)`);
      }
      return sortValue(e, `${at}[${i}]`);
    });
  }
  if (v && typeof v === "object") {
    // Only PLAIN objects: `new Date(0)` and `{}` both stringify to `{}`
    // through this path otherwise (same [M]).
    const proto = Object.getPrototypeOf(v);
    if (proto !== Object.prototype && proto !== null) {
      throw new Error(`manifest: value at ${at} is a ${v.constructor && v.constructor.name} instance, not a plain object`);
    }
    const out = Object.create(null);
    for (const k of Object.keys(v).sort()) out[k] = sortValue(v[k], `${at}.${k}`);
    return out;
  }
  if (typeof v === "number") {
    if (!Number.isFinite(v)) throw new Error(`manifest: non-finite number at ${at}`);
    if (Object.is(v, -0)) throw new Error(`manifest: negative zero at ${at}`);
    return v;
  }
  if (v === null || typeof v === "string" || typeof v === "boolean") return v;
  throw new Error(`manifest: value of type ${typeof v} at ${at} is not in the manifest value domain`);
}

function stableStringify(v) {
  return JSON.stringify(sortValue(v), null, 2) + "\n";
}

module.exports = { sha256, sha256File, stableStringify };
