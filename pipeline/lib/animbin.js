"use strict";
// ANIM1 encoder/decoder — reference implementation of pipeline/FORMATS.md §2.
// All multi-byte fields little-endian (host arm64 LE, device ARMv7 LE —
// byte order pinned explicitly, never platform-native by accident).

const MAGIC = Buffer.from("MLA1", "ascii"); // 4D 4C 41 31
const VERSION = 1;
const HEADER_SIZE = 16;
const DIR_ENTRY_SIZE = 12;

function align4(n) { return (n + 3) & ~3; }

// states: Map<string, Array<null | Array<Int16Array|number[]>>>
// Returns Buffer. Throws on any field-width violation (no silent clamps).
function encodeAnim(charId, states) {
  if (!Number.isInteger(charId) || charId < 0 || charId > 0xffff) {
    throw new Error("charId out of u16 range: " + charId);
  }
  const names = [...states.keys()].sort(); // bytewise ASCII (spec §2.2)
  for (const n of names) {
    if (!/^[\x21-\x7e]+$/.test(n)) {
      throw new Error("state name not plain printable ASCII: " + JSON.stringify(n));
    }
  }
  const stateCount = names.length;
  const dirOff = HEADER_SIZE;
  const strOff = dirOff + stateCount * DIR_ENTRY_SIZE;

  // string table
  const nameOffs = [];
  let cursor = strOff;
  const strParts = [];
  for (const n of names) {
    nameOffs.push(cursor);
    const b = Buffer.from(n + "\0", "ascii");
    strParts.push(b);
    cursor += b.length;
  }
  const strEnd = align4(cursor);
  if (strEnd > cursor) strParts.push(Buffer.alloc(strEnd - cursor));

  // frame-offset tables (u32 per frame, per state, directory order)
  const foTabOff = strEnd;
  const framesOffs = []; // per state: absolute offset of its offset table
  let foCursor = foTabOff;
  for (const n of names) {
    framesOffs.push(foCursor);
    foCursor += states.get(n).length * 4;
  }

  // frame records
  const recOff = foCursor; // 4-aligned by construction
  const recParts = [];
  const frameOffsets = []; // flat, state-major then frame order
  let recCursor = recOff;
  for (const n of names) {
    for (const frame of states.get(n)) {
      if (frame === null || frame === undefined) {
        frameOffsets.push(0); // absent frame sentinel (spec §2.4)
        continue;
      }
      if (!Array.isArray(frame)) throw new Error(`state ${n}: frame is not an array`);
      if (frame.length > 0xffff) throw new Error(`state ${n}: pathCount ${frame.length} > u16`);
      frameOffsets.push(recCursor);
      let recBytes = 2;
      for (const path of frame) recBytes += 2 + path.length * 2;
      const rec = Buffer.allocUnsafe(recBytes);
      let p = 0;
      rec.writeUInt16LE(frame.length, p); p += 2;
      for (const path of frame) {
        if (path.length > 0xffff) throw new Error(`state ${n}: coordCount ${path.length} > u16`);
        rec.writeUInt16LE(path.length, p); p += 2;
        for (let i = 0; i < path.length; i++) {
          const v = path[i];
          if (!Number.isInteger(v) || v < -32768 || v > 32767) {
            throw new Error(`state ${n}: coord ${v} not an int16`);
          }
          rec.writeInt16LE(v, p); p += 2;
        }
      }
      recParts.push(rec);
      recCursor += recBytes;
    }
  }
  const fileSize = recCursor;

  // assemble
  const head = Buffer.allocUnsafe(HEADER_SIZE);
  MAGIC.copy(head, 0);
  head.writeUInt16LE(VERSION, 4);
  head.writeUInt16LE(charId, 6);
  head.writeUInt32LE(stateCount, 8);
  head.writeUInt32LE(fileSize, 12);

  const dir = Buffer.allocUnsafe(stateCount * DIR_ENTRY_SIZE);
  for (let i = 0; i < stateCount; i++) {
    dir.writeUInt32LE(nameOffs[i], i * 12);
    dir.writeUInt32LE(states.get(names[i]).length, i * 12 + 4);
    dir.writeUInt32LE(framesOffs[i], i * 12 + 8);
  }

  const foBuf = Buffer.allocUnsafe(frameOffsets.length * 4);
  for (let i = 0; i < frameOffsets.length; i++) foBuf.writeUInt32LE(frameOffsets[i], i * 4);

  const out = Buffer.concat([head, dir, ...strParts, foBuf, ...recParts]);
  if (out.length !== fileSize) {
    throw new Error(`encode size mismatch: computed ${fileSize}, wrote ${out.length}`);
  }
  return out;
}

// Returns { charId, states: Map<string, Array<null | Int16Array[]>> }.
// Validates magic/version/fileSize and every offset before dereferencing.
function decodeAnim(buf) {
  if (buf.length < HEADER_SIZE || !buf.subarray(0, 4).equals(MAGIC)) {
    throw new Error("bad magic");
  }
  const version = buf.readUInt16LE(4);
  if (version !== VERSION) throw new Error("bad version " + version);
  const charId = buf.readUInt16LE(6);
  const stateCount = buf.readUInt32LE(8);
  const fileSize = buf.readUInt32LE(12);
  if (fileSize !== buf.length) {
    throw new Error(`fileSize ${fileSize} != buffer ${buf.length}`);
  }
  const readName = (off) => {
    const end = buf.indexOf(0, off);
    if (end < 0) throw new Error("unterminated name");
    return buf.toString("ascii", off, end);
  };
  const states = new Map();
  let prevName = null;
  for (let i = 0; i < stateCount; i++) {
    const e = HEADER_SIZE + i * DIR_ENTRY_SIZE;
    const name = readName(buf.readUInt32LE(e));
    if (prevName !== null && !(prevName < name)) {
      throw new Error(`directory not sorted: ${prevName} !< ${name}`);
    }
    prevName = name;
    const frameCount = buf.readUInt32LE(e + 4);
    const framesOff = buf.readUInt32LE(e + 8);
    const frames = [];
    for (let f = 0; f < frameCount; f++) {
      const rec = buf.readUInt32LE(framesOff + f * 4);
      if (rec === 0) { frames.push(null); continue; }
      if (rec + 2 > buf.length) throw new Error("frame record out of range");
      let p = rec;
      const pathCount = buf.readUInt16LE(p); p += 2;
      const paths = [];
      for (let j = 0; j < pathCount; j++) {
        const coordCount = buf.readUInt16LE(p); p += 2;
        if (p + coordCount * 2 > buf.length) throw new Error("path out of range");
        const arr = new Int16Array(coordCount);
        for (let k = 0; k < coordCount; k++) { arr[k] = buf.readInt16LE(p); p += 2; }
        paths.push(arr);
      }
      frames.push(paths);
    }
    states.set(name, frames);
  }
  return { charId, states };
}

module.exports = { encodeAnim, decodeAnim, MAGIC, VERSION };
