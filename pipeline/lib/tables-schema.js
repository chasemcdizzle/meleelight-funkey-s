"use strict";
// Pinned schema + executed-JS walk for the engine-table extractor output
// (fix_plan §M1 task 2; format CTAB1, pipeline/FORMATS.md §3).
//
// SINGLE SOURCE OF TRUTH on the JS side for field typing and ordering.
// The C side gets the same lists via X-macros generated into ml_tables.h;
// pipeline/lib/tables_check.c walks THOSE, so a schema/emission mismatch
// becomes a byte diff in the round-trip dump comparison.
//
// Typing is MEASURED-THEN-PINNED (AGENT-LOG iter 10): a field listed as
// int32 hard-throws on any non-integer / out-of-range executed value —
// silent coercion is the defect class this file exists to prevent.

const fs = require("fs");
const path = require("path");
const { sha256 } = require("./manifest");

const CHAR_NAMES = ["marth", "puff", "fox", "falco", "falcon"]; // charId order

// Attribute fields (exactly these 46 keys must be present per char).
// waitAnimSpeed is integral at the pin but typed f64 with its siblings
// run/walkAnimSpeed (semantically a speed multiplier).
const ATTR_F64 = [
  "aerialHmaxV", "airFriction", "airMobA", "airMobB", "charScale",
  "dAccA", "dAccB", "dInitV", "dMaxV", "dTInitV",
  "djMomentum", "djMultiplier", "ecbScale", "fHopInitV", "fastFallV",
  "gravity", "groundToAir", "jumpHinitV", "jumpHmaxV", "maxWalk",
  "miniScale", "modelScale", "runAnimSpeed", "sHopInitV",
  "shieldBreakVel", "shieldScale", "terminalV", "traction",
  "waitAnimSpeed", "walkAcc", "walkAnimSpeed", "walkInitV", "walkMaxV",
  "wallJumpVelX", "wallJumpVelY",
];
const ATTR_I32 = [
  "airdodgeIntangible", "dashFrameMax", "dashFrameMin", "jumpSquat",
  "runTurnBreakPoint", "weight",
];
const ATTR_I32V = [["hurtboxOffset", 2], ["ledgeSnapBoxOffset", 3], ["shieldOffset", 2]];
const ATTR_BOOL = ["multiJump", "walljump"];

// Hitbox scalar fields (offset + throwextra handled structurally).
const HB_F64 = ["size"];
const HB_I32 = ["dmg", "angle", "kg", "bk", "sk", "type", "clank",
  "hitGrounded", "hitAirborne"];
const HB_KEYS = new Set(["offset", "throwextra", ...HB_F64, ...HB_I32]);

// ---------------------------------------------------------------------------

function f64bits(v) {
  const buf = Buffer.alloc(8);
  buf.writeDoubleLE(v, 0);
  return buf.readBigUInt64LE(0).toString(16).padStart(16, "0");
}

function checkName(kind, name) {
  if (!/^[A-Za-z0-9_]+$/.test(name)) {
    throw new Error(`${kind} name ${JSON.stringify(name)} is not [A-Za-z0-9_]+`);
  }
}

function asF64(where, v) {
  if (typeof v !== "number" || !Number.isFinite(v)) {
    throw new Error(`${where}: expected finite number, got ${v}`);
  }
  return { bits: f64bits(v), dec: String(v) };
}

function asI32(where, v) {
  if (typeof v !== "number" || !Number.isInteger(v) || Object.is(v, -0) ||
      v < -0x80000000 || v > 0x7fffffff) {
    throw new Error(`${where}: expected int32, got ${v}`);
  }
  return v;
}

function asI16(where, v) {
  const n = asI32(where, v);
  if (n < -0x8000 || n > 0x7fff) throw new Error(`${where}: out of int16 range: ${n}`);
  return n;
}

// Execute the built extractor bundle under a window shim (same pattern as
// stages/animations.js: the bundle is pure data construction — Vec2D /
// createHitbox object literals and constant arithmetic, no Math.*, no DOM
// — so plain node execution of the SAME built artifact is engine-neutral).
function loadTables(distRoot) {
  const srcPath = path.join(distRoot, "dist", "js", "extractor.js");
  if (!fs.existsSync(srcPath)) {
    throw new Error(`missing ${srcPath} — run pipeline/extractor/build-extractor.sh first`);
  }
  const hadWindow = "window" in global;
  const saved = global.window;
  global.window = {};
  try {
    delete require.cache[require.resolve(srcPath)];
    require(srcPath);
    const tables = global.window.__tables;
    if (!tables) throw new Error("extractor bundle did not assign window.__tables");
    return { tables, srcSha256: sha256(fs.readFileSync(srcPath)) };
  } finally {
    if (hadWindow) global.window = saved; else delete global.window;
  }
}

// Walk + validate the live registries into the canonical CTAB1 model.
// Every value is typed per the pinned schema; hard-throws on any surprise.
function buildModel(tables) {
  const ids = tables.charIds;
  const wantIds = { MARTH_ID: 0, PUFF_ID: 1, FOX_ID: 2, FALCO_ID: 3, FALCON_ID: 4 };
  for (const [k, v] of Object.entries(wantIds)) {
    if (ids[k] !== v) throw new Error(`CHARIDS.${k} = ${ids[k]}, expected ${v}`);
  }

  const chars = [];
  for (let charId = 0; charId < CHAR_NAMES.length; charId++) {
    const cn = CHAR_NAMES[charId];
    const where = (s) => `${cn}/${s}`;

    // -- attributes (exact key set) --
    const a = tables.charAttributes[charId];
    if (!a) throw new Error(`${cn}: charAttributes missing`);
    const wantKeys = [...ATTR_F64, ...ATTR_I32,
      ...ATTR_I32V.map(([k]) => k), ...ATTR_BOOL].sort();
    const gotKeys = Object.keys(a).sort();
    if (gotKeys.join(",") !== wantKeys.join(",")) {
      throw new Error(`${cn}: attribute key set drifted:\n got ${gotKeys}\nwant ${wantKeys}`);
    }
    const attributes = { f64: {}, i32: {}, i32v: {}, bool: {} };
    for (const f of ATTR_F64) attributes.f64[f] = asF64(where("attr." + f), a[f]);
    for (const f of ATTR_I32) attributes.i32[f] = asI32(where("attr." + f), a[f]);
    for (const [f, len] of ATTR_I32V) {
      if (!Array.isArray(a[f]) || a[f].length !== len) {
        throw new Error(`${cn}: attr.${f} is not a length-${len} array`);
      }
      attributes.i32v[f] = a[f].map((v, i) => asI32(where(`attr.${f}[${i}]`), v));
    }
    for (const f of ATTR_BOOL) {
      if (typeof a[f] !== "boolean") throw new Error(`${cn}: attr.${f} not boolean`);
      attributes.bool[f] = a[f] ? 1 : 0;
    }

    // -- framesData --
    const framesData = {};
    for (const s of Object.keys(tables.framesData[charId]).sort()) {
      checkName("framesData state", s);
      framesData[s] = asI32(where("frames." + s), tables.framesData[charId][s]);
    }

    // -- intangibility ([start, length]) --
    const intangibility = {};
    for (const s of Object.keys(tables.intangibility[charId]).sort()) {
      checkName("intangibility state", s);
      const v = tables.intangibility[charId][s];
      if (!Array.isArray(v) || v.length !== 2) {
        throw new Error(`${cn}: intangibility.${s} is not [start,length]`);
      }
      intangibility[s] = [asI32(where(`intang.${s}[0]`), v[0]),
        asI32(where(`intang.${s}[1]`), v[1])];
    }

    // -- ecb (per state: frames of [bottomOffsetY, halfWidthX, midY, topY];
    // empty state arrays exist upstream (puff DEAD*) and are kept verbatim
    // as frameCount 0 / NULL frames in C) --
    const ecb = {};
    for (const s of Object.keys(tables.ecb[charId]).sort()) {
      checkName("ecb state", s);
      const frames = tables.ecb[charId][s];
      if (!Array.isArray(frames)) {
        throw new Error(`${cn}: ecb.${s} is not an array`);
      }
      ecb[s] = frames.map((fr, i) => {
        if (!Array.isArray(fr) || fr.length !== 4) {
          throw new Error(`${cn}: ecb.${s}[${i}] is not a quad`);
        }
        return fr.map((v, j) => asI16(where(`ecb.${s}[${i}][${j}]`), v));
      });
    }

    // -- hitboxes (moves -> id0..id3 -> hitbox record) --
    const hitboxes = {};
    for (const mv of Object.keys(tables.hitboxes[charId]).sort()) {
      checkName("hitbox move", mv);
      const obj = tables.hitboxes[charId][mv];
      const move = {};
      for (const idk of Object.keys(obj)) {
        if (!/^id[0-3]$/.test(idk)) throw new Error(`${cn}/${mv}: odd id key ${idk}`);
      }
      for (const idk of ["id0", "id1", "id2", "id3"]) {
        const hb = obj[idk];
        if (hb === undefined) continue;
        const hw = `${cn}/${mv}/${idk}`;
        const gotHbKeys = Object.keys(hb);
        for (const k of gotHbKeys) {
          if (!HB_KEYS.has(k)) throw new Error(`${hw}: unexpected field ${k}`);
        }
        const rec = {};
        const isArr = Array.isArray(hb.offset);
        const offs = isArr ? hb.offset : [hb.offset];
        if (offs.length === 0) throw new Error(`${hw}: empty offset array`);
        rec.offsetIsArray = isArr ? 1 : 0;
        rec.offset = offs.map((p, i) => {
          if (!p || typeof p.x !== "number" || typeof p.y !== "number") {
            throw new Error(`${hw}: offset[${i}] is not a Vec2D`);
          }
          return { x: asF64(`${hw}.offset[${i}].x`, p.x),
            y: asF64(`${hw}.offset[${i}].y`, p.y) };
        });
        for (const f of HB_F64) rec[f] = asF64(`${hw}.${f}`, hb[f]);
        for (const f of HB_I32) rec[f] = asI32(`${hw}.${f}`, hb[f]);
        if (typeof hb.throwextra !== "boolean") {
          throw new Error(`${hw}: throwextra not boolean`);
        }
        rec.throwextra = hb.throwextra ? 1 : 0;
        move[idk] = rec;
      }
      if (Object.keys(move).length === 0) throw new Error(`${cn}/${mv}: no hitboxes`);
      hitboxes[mv] = move;
    }

    chars.push({ charId, name: cn, attributes, framesData, intangibility, ecb, hitboxes });
  }
  return { formatVersion: 1, chars };
}

// Canonical leaf dump (FORMATS.md §3.6): one line per leaf value, the
// EXACT byte stream pipeline/lib/tables_check.c prints from the compiled
// C tables. Doubles print as 16 lowercase hex digits of their IEEE-754
// bit pattern; ints as decimal.
function dumpModel(model, emit) {
  for (const c of model.chars) {
    const cn = c.name;
    for (const f of ATTR_F64) emit(`attr/${cn}/${f}=${c.attributes.f64[f].bits}`);
    for (const f of ATTR_I32) emit(`attr/${cn}/${f}=${c.attributes.i32[f]}`);
    for (const [f, len] of ATTR_I32V) {
      for (let i = 0; i < len; i++) {
        emit(`attr/${cn}/${f}[${i}]=${c.attributes.i32v[f][i]}`);
      }
    }
    for (const f of ATTR_BOOL) emit(`attr/${cn}/${f}=${c.attributes.bool[f]}`);
    for (const [s, n] of Object.entries(c.framesData)) emit(`frames/${cn}/${s}=${n}`);
    for (const [s, v] of Object.entries(c.intangibility)) {
      emit(`intang/${cn}/${s}=${v[0]},${v[1]}`);
    }
    for (const [s, frames] of Object.entries(c.ecb)) {
      frames.forEach((fr, i) => emit(`ecb/${cn}/${s}/${i}=${fr.join(",")}`));
    }
    for (const [mv, move] of Object.entries(c.hitboxes)) {
      for (const idk of ["id0", "id1", "id2", "id3"]) {
        const hb = move[idk];
        if (!hb) continue;
        const p = `hb/${cn}/${mv}/${idk}`;
        emit(`${p}/offsetIsArray=${hb.offsetIsArray}`);
        hb.offset.forEach((v, i) =>
          emit(`${p}/offset[${i}]=${v.x.bits},${v.y.bits}`));
        for (const f of HB_F64) emit(`${p}/${f}=${hb[f].bits}`);
        for (const f of HB_I32) emit(`${p}/${f}=${hb[f]}`);
        emit(`${p}/throwextra=${hb.throwextra}`);
      }
    }
  }
}

module.exports = {
  CHAR_NAMES, ATTR_F64, ATTR_I32, ATTR_I32V, ATTR_BOOL, HB_F64, HB_I32,
  f64bits, loadTables, buildModel, dumpModel,
};
