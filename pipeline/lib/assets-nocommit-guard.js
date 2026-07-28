#!/usr/bin/env node
"use strict";
// Provenance guard for the Nintendo-derived menu artwork (FORMATS.md §7):
// no source PNG and no emitted menu.img1 may be committed, under ANY name.
// Usage: node lib/assets-nocommit-guard.js <run-dir>
//
// BY CONTENT, NOT BY NAME (review-a9-3 [M]): a filename scan only caught
// *.img1, so a renamed or re-extensioned copy stayed green. The forbidden
// set is built from the run manifest itself — the 15 source PNG sha256s
// plus the emitted menu.img1 sha256.
//
// AND BOTH PLANES, NOT JUST THE WORKING TREE (review-a9-4 [M]): the first
// version enumerated INDEX paths but hashed WORKING-TREE bytes, so a
// forbidden blob that was staged and then replaced or deleted on disk stayed
// commit-ready and invisible. What gets committed is the INDEX, so the index
// blobs are hashed directly (`git ls-files --stage` + `git cat-file
// --batch`); the working tree is scanned as well, and a tracked path that
// cannot be read is a FAILURE unless git itself reports it deleted — never a
// silent skip.

const fs = require("fs");
const path = require("path");
const crypto = require("crypto");
const { execFileSync } = require("child_process");

const runDir = path.resolve(process.argv[2] || "");
if (!process.argv[2]) {
  console.error("usage: assets-nocommit-guard.js <run-dir>");
  process.exit(1);
}
const REPO = path.join(__dirname, "..", "..");
const sha256 = (buf) => crypto.createHash("sha256").update(buf).digest("hex");
const git = (args, opts) =>
  execFileSync("git", ["-C", REPO, ...args], { maxBuffer: 1 << 30, ...opts });

let failures = 0;
const fail = (msg) => { console.error("NOCOMMIT-FAIL: " + msg); failures++; };

// ---- the forbidden content set, from the run manifest --------------------
const mf = JSON.parse(fs.readFileSync(path.join(runDir, "manifest.json"), "utf8"))
  .stages.assets;
const forbidden = new Map();
for (const s of mf.sources) {
  if (s.path.startsWith("dist/")) forbidden.set(s.sha256, s.path);
}
for (const a of mf.artifacts) {
  if (a.path.endsWith(".img1")) forbidden.set(a.sha256, a.path);
}
const EXPECT_FORBIDDEN = 16; // 15 upstream PNGs + menu.img1
if (forbidden.size !== EXPECT_FORBIDDEN) {
  console.error(`NOCOMMIT-FAIL: forbidden set has ${forbidden.size} entries, ` +
    `expected ${EXPECT_FORBIDDEN} (15 source PNGs + menu.img1)`);
  process.exit(1);
}

// ---- plane 1: the INDEX (what a commit would actually contain) -----------
// `ls-files --stage -z` rows: "<mode> <oid> <stage>\t<path>\0"
const stageRows = git(["ls-files", "--stage", "-z"]).toString("utf8")
  .split("\0").filter(Boolean).map((row) => {
    const m = /^([0-7]{6}) ([0-9a-f]{40,64}) ([0-3])\t([\s\S]+)$/.exec(row);
    if (!m) { fail(`unparsable ls-files --stage row: ${JSON.stringify(row)}`); return null; }
    return { mode: m[1], oid: m[2], stage: m[3], path: m[4] };
  }).filter(Boolean);
if (stageRows.length === 0) fail("git ls-files --stage listed nothing");

// Hash every index blob through ONE `cat-file --batch` process.
const byOid = new Map();
for (const r of stageRows) {
  if (r.mode === "160000") continue; // gitlink (submodule): no blob content
  if (!byOid.has(r.oid)) byOid.set(r.oid, []);
  byOid.get(r.oid).push(r.path);
}
// FAIL-CLOSED FRAMING (PROCESS §3; review-a9-5 [M]). Counting responses is
// not enough: a duplicated benign response can stand in for a forbidden
// one at an identical count, and the forbidden blob is then never hashed.
// Response n must therefore BE requested OID n, must be a blob, and its
// frame must be complete — anything else fails rather than being skipped.
if (byOid.size > 0) {
  const oids = [...byOid.keys()];
  const out = git(["cat-file", "--batch"], { input: oids.join("\n") + "\n" });
  let off = 0;
  for (let i = 0; i < oids.length; i++) {
    const nl = out.indexOf(0x0a, off);
    if (nl < 0) { fail(`cat-file --batch: truncated header for ${oids[i]}`); break; }
    const hdr = out.toString("utf8", off, nl);
    const m = /^([0-9a-f]{40,64}) ([a-z]+) (0|[1-9][0-9]*)$/.exec(hdr);
    if (!m) {
      fail(`cat-file --batch: response ${i} has a malformed header ` +
        `${JSON.stringify(hdr)} (a "missing"/"ambiguous" response lands here)`);
      break;
    }
    const [, oid, type, sizeStr] = m;
    if (oid !== oids[i]) {
      fail(`cat-file --batch: response ${i} is ${oid}, requested ${oids[i]} ` +
        `— responses must be one-to-one and in order`);
      break;
    }
    if (type !== "blob") {
      fail(`cat-file --batch: ${oid} is a ${type}, expected blob`);
      break;
    }
    const size = Number(sizeStr);
    if (nl + 1 + size + 1 > out.length) {
      fail(`cat-file --batch: ${oid} frame is truncated (declares ${size} bytes)`);
      break;
    }
    if (out[nl + 1 + size] !== 0x0a) {
      fail(`cat-file --batch: ${oid} is not followed by its separator newline`);
      break;
    }
    const h = sha256(out.slice(nl + 1, nl + 1 + size));
    off = nl + 1 + size + 1;
    if (forbidden.has(h)) {
      for (const p of byOid.get(oid)) {
        fail(`Nintendo-derived content is STAGED IN THE INDEX at ${p} ` +
          `(content of ${forbidden.get(h)})`);
      }
    }
  }
  if (failures === 0 && off !== out.length) {
    fail(`cat-file --batch: ${out.length - off} trailing byte(s) after the ` +
      `${oids.length} expected responses`);
  }
}

// ---- plane 2: the WORKING TREE ------------------------------------------
// A tracked path missing from disk is only acceptable if git agrees it is
// deleted; anything else unreadable is a failure, not a skip.
const deleted = new Set(
  git(["ls-files", "--deleted", "-z"]).toString("utf8").split("\0").filter(Boolean));
let scanned = 0;
for (const r of stageRows) {
  if (r.mode === "160000") continue;
  const abs = path.join(REPO, r.path);
  let buf;
  try {
    buf = fs.readFileSync(abs);
  } catch (e) {
    if (deleted.has(r.path) && e.code === "ENOENT") continue;
    fail(`tracked path ${r.path} could not be read (${e.code}) and git does ` +
      `not report it deleted`);
    continue;
  }
  scanned++;
  const h = sha256(buf);
  if (forbidden.has(h)) {
    fail(`Nintendo-derived content is IN THE WORKING TREE at ${r.path} ` +
      `(content of ${forbidden.get(h)})`);
  }
}

if (failures > 0) {
  console.error(`assets-nocommit-guard: ${failures} failure(s)`);
  process.exit(1);
}
console.log(`no-commit guard: none of the ${EXPECT_FORBIDDEN} forbidden blobs ` +
  `appears in the index (${byOid.size} objects) or the working tree ` +
  `(${scanned} files)`);
