#!/usr/bin/env node
// port/gfx/snd_reference.js — the REFERENCE renderer for the offline
// mixer-fidelity differential (M4 task 6).
//
// A SECOND, INDEPENDENT implementation of the documented audible
// semantics — it shares NO code with snd_mixer.h / snd_render.c and
// never reads the SNDPACK: sources are the canonical SND1 artifacts
// directly (sounds.json + audio/sfx/*.pcm, FORMATS.md §5).
//
// Semantics implemented (each pinned from primary sources):
// - howler 2.0.12 voice lifecycle as upstream uses it (vendored
//   node_modules/howler read, never remembered): every play() starts a
//   NEW sound instance with id = ++globalCounter (counter base 1000);
//   there is NO voice cap in the browser; stop(id) stops that one
//   instance; stop(undefined) stops every instance of the Howl;
//   stop(stale-id) is a no-op; loop restarts the source seamlessly.
// - SND1 effective volume (FORMATS.md §5.3: the post-load changeVolume
//   value, bits authoritative) with the documented device quantization
//   gainQ8 = round(volume * 256) (snd_mixer.h SNDPACK1 contract).
// - the documented device resample: 22050 -> 44100 zero-order hold
//   (step (22050<<16)/44100 = 0x8000 exactly => each source sample
//   emitted twice; a non-looping voice emits exactly 2*samples outputs).
// - per-sample mix: sum over voices of (s * gainQ8) >> 8 (32-bit
//   arithmetic shift — JS bitwise ops match C int32 semantics), clamp
//   to S16, mono voice on both stereo channels; 735 output frames per
//   sim frame (44100/60 exact); a frame's events land before its
//   samples.
//
// WHAT THIS CANNOT CATCH (honest statement, PROCESS §8): a shared
// misreading of the documented semantics, and the device adaptations
// themselves (Q8 quantization, ZOH resample quality, the 8-voice cap,
// frame-boundary event quantization) — those are design choices both
// sides implement by documentation. It catches IMPLEMENTATION
// divergence between the two realizations (indexing, lifecycle, gain,
// loop/end boundaries, stop routing, steal-policy effects).
//
// Usage: node snd_reference.js --audio <pipeline-audio-dir>
//          --events <schedule> --frames N --out <pcm.raw>
//          [--voices <cap>]
// --voices <cap> (M4 task 6 basis contingency, AGENT-LOG iter 82): cap
// concurrency at <cap> voices with the DOCUMENTED steal policy
// (steal-oldest-by-start-sequence, snd_mixer.h header) — implemented
// independently here from that documented contract, never from the C
// code. Default: unlimited (browser howler truth). The check runs BOTH:
// unlimited measures true concurrency (and must bit-match the C mixer
// wherever concurrency <= 8); capped must bit-match it EVERYWHERE.
// Verdict grammar (load-bearing):
//   snd-ref OK frames=<n> plays=<n> stops=<n> maxvoices=<n> steals=<n>
//     bytes=<n>
"use strict";
const fs = require("fs");
const path = require("path");

function die(msg) {
  console.error("snd-ref FAIL: " + msg);
  process.exit(2);
}

const args = process.argv.slice(2);
let audioDir = null, eventsPath = null, outPath = null, frames = -1;
let voiceCap = Infinity;
for (let i = 0; i < args.length; i++) {
  const a = args[i];
  const v = () => { if (i + 1 >= args.length) die(a + " needs a value"); return args[++i]; };
  if (a === "--audio") audioDir = v();
  else if (a === "--events") eventsPath = v();
  else if (a === "--frames") frames = parseInt(v(), 10);
  else if (a === "--out") outPath = v();
  else if (a === "--voices") {
    voiceCap = parseInt(v(), 10);
    if (!Number.isInteger(voiceCap) || voiceCap < 1) die("bad --voices");
  }
  else die("bad argument: " + a);
}
if (!audioDir || !eventsPath || !outPath || !Number.isInteger(frames) || frames <= 0) {
  die("usage: snd_reference.js --audio <dir> --events <file> --frames N --out <pcm>");
}

// --- SND1 sound map (canonical artifact, FORMATS.md §5.3) -----------------
const sounds = JSON.parse(
  fs.readFileSync(path.join(audioDir, "sounds.json"), "utf8"));
if (sounds.formatVersion !== 1) die("sounds.json formatVersion != 1");
if (!sounds.sfx || typeof sounds.sfx !== "object") die("sounds.json: no sfx");

const lib = new Map(); // name -> {pcm: Int16Array, gain, loop}
for (const name of Object.keys(sounds.sfx)) {
  const e = sounds.sfx[name];
  const bits = e.volume && e.volume.bits;
  if (typeof bits !== "string" || !/^[0-9a-f]{16}$/.test(bits)) {
    die("bad volume bits for " + name);
  }
  const vol = Buffer.from(bits, "hex").readDoubleBE(0);
  if (!(vol >= 0 && vol <= 1)) die("volume outside [0,1] for " + name);
  if (e.loop !== 0 && e.loop !== 1) die("bad loop flag for " + name);
  const raw = fs.readFileSync(path.join(audioDir, e.blob)); // ENOENT = loud
  if (raw.length === 0 || raw.length % 2 !== 0) die("bad blob: " + e.blob);
  // 22050 Hz mono S16LE (FORMATS.md §5.1)
  const pcm = new Int16Array(raw.buffer, raw.byteOffset, raw.length / 2);
  lib.set(name, { pcm, gain: Math.round(vol * 256), loop: e.loop === 1 });
}

// --- schedule (snd_events_tap.c grammar; STRICT full-line) -----------------
const P_RE = /^P (0|[1-9][0-9]*) ([0-9A-Za-z]+)$/;
const S_RE = /^S (0|[1-9][0-9]*) ([0-9A-Za-z]+)\.stop ([01]) ([0-9a-f]{16})$/;
const T_RE = /^SNDEV OK plays=(0|[1-9][0-9]*) stops=(0|[1-9][0-9]*) lastFrame=(0|[1-9][0-9]*)$/;

const raw = fs.readFileSync(eventsPath, "utf8");
if (raw.length === 0 || raw[raw.length - 1] !== "\n") {
  die("events: missing final newline (truncated schedule)");
}
const lines = raw.slice(0, -1).split("\n");
const events = [];
let sawTerm = false, nPlays = 0, nStops = 0;
for (const line of lines) {
  if (sawTerm) die("events: bytes after the terminator");
  let m;
  if ((m = P_RE.exec(line))) {
    events.push({ frame: parseInt(m[1], 10), stop: false, name: m[2] });
    nPlays++;
  } else if ((m = S_RE.exec(line))) {
    const idBits = Buffer.from(m[4], "hex");
    events.push({
      frame: parseInt(m[1], 10), stop: true, name: m[2],
      hasId: m[3] === "1", id: idBits.readDoubleBE(0),
    });
    nStops++;
  } else if ((m = T_RE.exec(line))) {
    if (parseInt(m[1], 10) !== nPlays || parseInt(m[2], 10) !== nStops) {
      die("events: terminator counts disagree with the lines");
    }
    if (parseInt(m[3], 10) !== frames) {
      die("events: lastFrame != --frames (partial-run schedule)");
    }
    sawTerm = true;
  } else {
    die("events: grammar violation: " + JSON.stringify(line));
  }
}
if (!sawTerm) die("events: missing SNDEV OK terminator");
let prevFrame = 0;
for (const e of events) {
  if (e.frame < prevFrame) die("events: frames not monotone");
  if (e.frame > frames) die("events: frame outside the run");
  prevFrame = e.frame;
}

// --- render ----------------------------------------------------------------
const OUT_PER_FRAME = 735; // 44100/60 exact
let voices = []; // {name, id, startOut, startSeq, pcm, gain, loop, samples}
let playCounter = 0;
let startSeq = 0;
let maxVoices = 0;
let steals = 0;

function expire(nowOut) {
  voices = voices.filter((v) =>
    v.loop || nowOut < v.startOut + 2 * v.samples);
}

const out = Buffer.alloc(frames * OUT_PER_FRAME * 4); // stereo S16LE
let outPos = 0;

let evIdx = 0;
for (let f = 0; f <= frames; f++) {
  const frameStartOut = f === 0 ? 0 : (f - 1) * OUT_PER_FRAME;
  while (evIdx < events.length && events[evIdx].frame === f) {
    const e = events[evIdx++];
    expire(frameStartOut);
    if (e.stop) {
      // howler stop semantics (header note)
      if (e.hasId) {
        voices = voices.filter((v) => !(v.name === e.name && v.id === e.id));
      } else {
        voices = voices.filter((v) => v.name !== e.name);
      }
    } else {
      const s = lib.get(e.name);
      if (!s) die("play event for a name not in the SND1 map: " + e.name);
      playCounter++;
      if (voices.length >= voiceCap) {
        // documented policy: steal-oldest-by-start-sequence
        let oldest = 0;
        for (let k = 1; k < voices.length; k++) {
          if (voices[k].startSeq < voices[oldest].startSeq) oldest = k;
        }
        voices.splice(oldest, 1);
        steals++;
      }
      voices.push({
        name: e.name, id: 1000 + playCounter, startOut: frameStartOut,
        startSeq: ++startSeq,
        pcm: s.pcm, gain: s.gain, loop: s.loop, samples: s.pcm.length,
      });
      if (voices.length > maxVoices) maxVoices = voices.length;
    }
  }
  if (f === 0) continue; // setup events only; no audio before frame 1
  for (let k = 0; k < OUT_PER_FRAME; k++) {
    const outIdx = frameStartOut + k;
    let acc = 0;
    for (const v of voices) {
      const rel = outIdx - v.startOut;
      if (rel < 0) continue;
      let src;
      if (v.loop) {
        src = (rel >> 1) % v.samples;
      } else {
        if (rel >= 2 * v.samples) continue; // ended
        src = rel >> 1;
      }
      acc += (v.pcm[src] * v.gain) >> 8;
    }
    if (acc > 32767) acc = 32767;
    if (acc < -32768) acc = -32768;
    out.writeInt16LE(acc, outPos);
    out.writeInt16LE(acc, outPos + 2);
    outPos += 4;
  }
  // drop ended voices once per frame (keeps the scan bounded)
  expire(f * OUT_PER_FRAME);
}
if (evIdx !== events.length) die("events beyond the last frame");

fs.writeFileSync(outPath, out);
console.log(
  "snd-ref OK frames=" + String(frames) +
  " plays=" + String(nPlays) +
  " stops=" + String(nStops) +
  " maxvoices=" + String(maxVoices) +
  " steals=" + String(steals) +
  " bytes=" + String(out.length));
