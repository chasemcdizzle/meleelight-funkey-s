// json-dup-key-scan.js — THE shared string-aware duplicate-JSON-key
// scanner for the target rig (review-96 C-M1, iter 98; PROCESS §3
// whitelist rule). JSON.parse is silently last-wins on duplicate keys,
// so every JSON-parsing DECISION consumer in the rig
// (validate-target-manifest.js, verify-target-stream.js,
// freeze-target.js; wrap-target.js consumes the manifest through the
// shared validator) scans the RAW bytes with this walker BEFORE any
// JSON.parse. The refuted byte-literal '"key":' token-count guard
// (invisible to the live-confirmed `"id" : "t02"` whitespace spelling)
// is DELETED in favor of this — no regex spellings.
//
// Method: a proper tokenizer over the raw text — string literals
// scanned with full escape handling and DECODED for comparison (so
// `"id"` duplicates `"id"`), JSON whitespace skipped between
// tokens (so `"id" : "t02"` is the same key as `"id":"t02"`), and a
// structural mini-parser tracking a scope stack: every OBJECT scope
// carries its own key set; a repeated key at ANY scope throws naming
// the key and its path. Structural anomalies the walker cannot
// attribute (unterminated string, bad escape, misplaced token,
// trailing content) also throw — fail closed; JSON.parse behind the
// scanner remains the full syntax authority.
//
// API: assertNoDuplicateKeys(raw, what) -> undefined, or throws
// Error("<what>: duplicate JSON key \"k\" at <path> — ..."). Never
// mutates, never parses values; O(n) single pass.
"use strict";

const WS = " \t\n\r";

function assertNoDuplicateKeys(raw, what) {
  if (typeof raw !== "string") {
    throw new Error(what + ": dup-key scan needs the raw text, got " + typeof raw);
  }
  let i = 0;
  const n = raw.length;

  function scanDie(msg) {
    throw new Error(what + ": " + msg);
  }
  function skipWs() {
    while (i < n && WS.indexOf(raw[i]) !== -1) i++;
  }
  // Scan a string literal starting at raw[i] === '"'; returns the
  // DECODED string content and leaves i past the closing quote.
  function scanString(where) {
    let out = "";
    i++; // opening quote
    for (;;) {
      if (i >= n) scanDie("unterminated string literal in " + where);
      const c = raw[i];
      if (c === '"') { i++; return out; }
      if (c === "\\") {
        i++;
        if (i >= n) scanDie("unterminated escape in " + where);
        const e = raw[i];
        if (e === '"') out += '"';
        else if (e === "\\") out += "\\";
        else if (e === "/") out += "/";
        else if (e === "b") out += "\b";
        else if (e === "f") out += "\f";
        else if (e === "n") out += "\n";
        else if (e === "r") out += "\r";
        else if (e === "t") out += "\t";
        else if (e === "u") {
          const hex = raw.slice(i + 1, i + 5);
          if (!/^[0-9a-fA-F]{4}$/.test(hex)) {
            scanDie("bad \\u escape in " + where);
          }
          out += String.fromCharCode(parseInt(hex, 16));
          i += 4;
        } else scanDie("bad escape \\" + e + " in " + where);
        i++;
      } else {
        out += c;
        i++;
      }
    }
  }
  // Consume a primitive token (number/true/false/null — content is not
  // the scanner's concern, structure is).
  function scanPrimitive(where) {
    const start = i;
    while (i < n && '{}[]:,"'.indexOf(raw[i]) === -1 &&
           WS.indexOf(raw[i]) === -1) i++;
    if (i === start) {
      scanDie("unexpected token " + JSON.stringify(raw[i]) + " in " + where);
    }
  }

  function scanValue(pathStr) {
    skipWs();
    if (i >= n) scanDie("unexpected end of input in " + pathStr);
    const c = raw[i];
    if (c === "{") { scanObject(pathStr); return; }
    if (c === "[") { scanArray(pathStr); return; }
    if (c === '"') { scanString(pathStr); return; }
    if (c === "}" || c === "]" || c === ":" || c === ",") {
      scanDie("unexpected " + JSON.stringify(c) + " where a value was " +
        "expected in " + pathStr);
    }
    scanPrimitive(pathStr);
  }

  function scanObject(pathStr) {
    i++; // '{'
    const keys = Object.create(null);
    skipWs();
    if (i < n && raw[i] === "}") { i++; return; }
    for (;;) {
      skipWs();
      if (i >= n || raw[i] !== '"') {
        scanDie("object key is not a string literal in " + pathStr);
      }
      const key = scanString(pathStr + ".<key>");
      if (keys[key] !== undefined) {
        scanDie("duplicate JSON key " + JSON.stringify(key) + " at " +
          pathStr + " — JSON.parse is silently last-wins; corruption, refuse");
      }
      keys[key] = true;
      skipWs();
      if (i >= n || raw[i] !== ":") {
        scanDie("missing ':' after key " + JSON.stringify(key) + " in " + pathStr);
      }
      i++;
      scanValue(pathStr + "." + key);
      skipWs();
      if (i < n && raw[i] === ",") { i++; continue; }
      if (i < n && raw[i] === "}") { i++; return; }
      scanDie("expected ',' or '}' in object " + pathStr);
    }
  }

  function scanArray(pathStr) {
    i++; // '['
    skipWs();
    if (i < n && raw[i] === "]") { i++; return; }
    let idx = 0;
    for (;;) {
      scanValue(pathStr + "[" + idx + "]");
      skipWs();
      if (i < n && raw[i] === ",") { i++; idx++; continue; }
      if (i < n && raw[i] === "]") { i++; return; }
      scanDie("expected ',' or ']' in array " + pathStr);
    }
  }

  scanValue("$");
  skipWs();
  if (i !== n) {
    scanDie("trailing content after the JSON document (offset " + i + ")");
  }
}

module.exports = { assertNoDuplicateKeys };
