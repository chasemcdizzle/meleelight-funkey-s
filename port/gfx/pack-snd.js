#!/usr/bin/env node
// port/gfx/pack-snd.js — build a SNDPACK1 SFX pack (M3 task 6) from the
// pipeline audio stage's SND1 artifacts (FORMATS.md §5): sounds.json's
// sfx map + the audio/sfx/*.pcm blobs (22050 Hz mono S16LE). Format:
// port/gfx/snd_mixer.h header comment (the C loader is the paired
// implementation). Deterministic: entries sorted strictly ascending by
// name; no timestamps; byte-stable across runs (the check builds the
// pack twice and cmp's, then pins its sha256).
//
// Gain: gainQ8 = Math.round(volume * 256) where volume is the SND1
// EFFECTIVE volume (the post-load changeVolume value — what Howler
// actually plays; FORMATS.md §5.3), decoded from its authoritative
// IEEE-754 bit pattern. Loop: the SND1 loop flag verbatim.
//
// --drop-name <name>: TEST-ONLY seam (negative testing — the "dropped
// SND1 blob" tooth): omits one named sound from the pack so the check
// can prove a missing blob is a LOUD runtime death, never silence. The
// gate path never passes it (the pack sha pin would fail anyway).
//
// PROVENANCE: output is Nintendo-derived audio — PRIVATE USE ONLY,
// gitignored build output, pushed only to device scratch, NEVER
// committed (CLAUDE.md / FORMATS.md §5).
'use strict';
const fs = require('fs');
const path = require('path');

function die(msg) {
  console.error('pack-snd FAIL: ' + msg);
  process.exit(1);
}

const args = process.argv.slice(2);
let audioDir = null;
let outPath = null;
let dropName = null;
for (let i = 0; i < args.length; i++) {
  if (args[i] === '--drop-name') {
    if (i + 1 >= args.length) die('--drop-name needs a value');
    dropName = args[++i];
  } else if (audioDir === null) {
    audioDir = args[i];
  } else if (outPath === null) {
    outPath = args[i];
  } else {
    die('unexpected argument: ' + args[i]);
  }
}
if (!audioDir || !outPath) {
  die('usage: pack-snd.js <audio-build-dir> <out.sndpack> [--drop-name n]');
}

const sounds = JSON.parse(
  fs.readFileSync(path.join(audioDir, 'sounds.json'), 'utf8'));
if (sounds.formatVersion !== 1) die('sounds.json formatVersion != 1');
if (!sounds.sfx || typeof sounds.sfx !== 'object') die('sounds.json: no sfx');

const NAME_LEN = 24;
const REC_LEN = 36;

let names = Object.keys(sounds.sfx).sort();
if (new Set(names).size !== names.length) die('duplicate sfx names');
if (dropName !== null) {
  if (!names.includes(dropName)) die('--drop-name not in the sfx map');
  names = names.filter((n) => n !== dropName);
  console.error('pack-snd: TEST MODE — dropped "' + dropName + '"');
}
if (names.length === 0) die('no sfx entries');

const records = [];
const blobs = [];
let dataBytes = 0;
for (const name of names) {
  if (!/^[0-9A-Za-z]+$/.test(name)) die('bad name chars: ' + name);
  if (Buffer.byteLength(name, 'utf8') >= NAME_LEN) {
    die('name too long for SNDPACK1: ' + name);
  }
  const e = sounds.sfx[name];
  if (typeof e.blob !== 'string' || !e.blob.startsWith('audio/sfx/')) {
    die('bad blob path for ' + name);
  }
  const pcm = fs.readFileSync(path.join(audioDir, e.blob)); // ENOENT = loud
  if (pcm.length === 0) die('empty blob: ' + e.blob);
  if (pcm.length % 2 !== 0) die('odd blob byte length: ' + e.blob);
  const bits = e.volume && e.volume.bits;
  if (typeof bits !== 'string' || !/^[0-9a-f]{16}$/.test(bits)) {
    die('bad volume bits for ' + name);
  }
  const vol = Buffer.from(bits, 'hex').readDoubleBE(0);
  if (!(vol >= 0 && vol <= 1)) die('volume outside [0,1] for ' + name);
  const gainQ8 = Math.round(vol * 256);
  if (e.loop !== 0 && e.loop !== 1) die('bad loop flag for ' + name);
  const rec = Buffer.alloc(REC_LEN);
  rec.write(name, 0, 'utf8'); // NUL-padded by alloc
  rec.writeUInt32LE(dataBytes, 24);
  rec.writeUInt32LE(pcm.length / 2, 28);
  rec.writeUInt16LE(gainQ8, 32);
  rec.writeUInt8(e.loop, 34);
  rec.writeUInt8(0, 35);
  records.push(rec);
  blobs.push(pcm);
  dataBytes += pcm.length; // even, so every offset stays even
}

const header = Buffer.alloc(16);
header.write('SNDPACK1', 0, 'ascii');
header.writeUInt32LE(names.length, 8);
header.writeUInt32LE(dataBytes, 12);

const out = Buffer.concat([header, ...records, ...blobs]);
fs.writeFileSync(outPath, out);
// Load-bearing output grammar (the check parses this exact line):
//   pack-snd OK count=<n> dataBytes=<n> fileBytes=<n>
console.log(
  'pack-snd OK count=' + String(names.length) +
  ' dataBytes=' + String(dataBytes) +
  ' fileBytes=' + String(out.length));
