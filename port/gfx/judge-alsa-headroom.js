// port/gfx/judge-alsa-headroom.js — ALSA playback-headroom judge (A28).
//
// TURNS "DOES IT SOUND OK" INTO A NUMBER. The audio buzz A28 chased for
// three sessions was buffer starvation, and the only instrument we had
// for it was the owner's ear: `platform_audio_sdl.h:44-51` documents the
// `underruns` counter as blind to DMA xruns BY CONSTRUCTION (a callback
// that is perfectly timely still starves the ring if the period is
// shorter than a frame), so no existing bar could see this.
//
// The kernel already exposes the number. While a PCM is open,
//   /proc/asound/card0/pcm0p/sub0/hw_params   -> rate, period_size, buffer_size
//   /proc/asound/card0/pcm0p/sub0/status      -> avail_max
// `avail_max` is the HIGH-WATER MARK of FREE space in the ring since the
// stream started. So the worst-case audio the app ever had IN HAND is
//   inHand = buffer_size - avail_max     (sample frames)
// and `avail_max` approaching `buffer_size` IS starvation — as a number.
//
// THE BAR, derived from the same frame budget the period size is derived
// from (platform.h PLATFORM_AUDIO_SAMPLES_DEFAULT): the refill runs on
// the game thread, so the worst-case audio in hand must EXCEED ONE WHOLE
// 16.67 ms frame. Anything less means one long frame can empty the ring.
//
// MEASURED, 2026-08-24 (fix_plan A28), judged by this bar:
//   512/1024  avail_max 768  -> 256 frames =  5.80 ms  FAIL (this buzzed)
//   2048/4096 avail_max 2096 -> 2000 frames = 45.35 ms PASS (clean by ear)
//
// HOST-JUDGED BY DESIGN (the M3/M4 convention: evidence is pulled from
// the device, judged on the host). The device leg is `cat` of the two
// proc files; this judge never touches a device.
//
//   node port/gfx/judge-alsa-headroom.js <hw_params-file> <status-file>
//
// Verdict grammar (load-bearing; check-alsa-headroom.sh matches anchored):
//   ALSA HEADROOM OK rate=<n> period=<n> buffer=<n> availMax=<n> inHand=<n> <x.xx>ms
// Anything else -> `FAIL: ...` on stderr, exit 1.
"use strict";

const FRAME_MS = 1000 / 60; // 16.666… — the 60 fps frame the whole port targets

function die(msg) {
  process.stderr.write("FAIL: " + msg + "\n");
  process.exit(1);
}

function read(path, what) {
  let txt;
  try {
    txt = require("fs").readFileSync(path, "utf8");
  } catch (e) {
    die(what + " unreadable (" + path + "): " + e.message);
  }
  // A PCM that is not open reports exactly this. It is not a parse
  // failure and must not be reported as one: it means the run under
  // test never had audio open, so there is nothing to judge.
  if (/^\s*closed\s*$/.test(txt)) {
    die(what + " reads `closed` — the PCM was not open when this was " +
        "sampled, so there is no headroom evidence in it (" + path + ")");
  }
  return txt;
}

// `key: value` / `key   : value`, one per line. Values may carry a
// trailing parenthesised form (`rate: 44100 (44100/1)`) — take the
// leading integer only, and require it to be the WHOLE leading token so
// a garbled field cannot be read as a plausible number.
function field(txt, key, path) {
  const re = new RegExp("^\\s*" + key + "\\s*:\\s*(\\S+)", "m");
  const m = re.exec(txt);
  if (!m) die("no `" + key + "` field in " + path);
  if (!/^\d+$/.test(m[1])) {
    die("`" + key + "` is not an integer in " + path + ": " + m[1]);
  }
  const v = Number(m[1]);
  if (!Number.isSafeInteger(v)) die("`" + key + "` out of range in " + path);
  return v;
}

const [hwPath, stPath] = process.argv.slice(2);
if (!hwPath || !stPath) {
  process.stderr.write(
    "usage: node judge-alsa-headroom.js <hw_params> <status>\n");
  process.exit(2);
}

const hw = read(hwPath, "hw_params");
const st = read(stPath, "status");

const rate = field(hw, "rate", hwPath);
const period = field(hw, "period_size", hwPath);
const buffer = field(hw, "buffer_size", hwPath);
const availMax = field(st, "avail_max", stPath);

if (rate <= 0) die("nonsense rate " + rate);
if (period <= 0 || buffer <= 0) die("nonsense period/buffer " + period + "/" + buffer);
if (buffer < period) {
  die("buffer_size " + buffer + " < period_size " + period +
      " — the ring cannot hold one period; hw_params is not describing a " +
      "usable stream");
}
if (availMax > buffer) {
  die("avail_max " + availMax + " exceeds buffer_size " + buffer +
      " — free space cannot exceed the ring; the two files do not describe " +
      "the same stream (were they sampled from the same run?)");
}

const inHand = buffer - availMax;
const ms = (inHand * 1000) / rate;
const periodMs = (period * 1000) / rate;

// The period is the refill deadline; it must clear one frame on its own,
// independently of what the ring happened to reach at runtime. A period
// under a frame is a defect even on a run that got lucky.
if (periodMs <= FRAME_MS) {
  die("period_size " + period + " = " + periodMs.toFixed(2) + " ms at " +
      rate + " Hz — SHORTER THAN ONE " + FRAME_MS.toFixed(2) + " ms frame. " +
      "The refill deadline is inside the frame budget; every frame that " +
      "renders anything misses it (platform.h PLATFORM_AUDIO_SAMPLES_DEFAULT)");
}
if (ms <= FRAME_MS) {
  die("worst-case audio in hand was " + inHand + " frames = " +
      ms.toFixed(2) + " ms (avail_max " + availMax + " of buffer_size " +
      buffer + ") — one " + FRAME_MS.toFixed(2) + " ms frame can drain the " +
      "ring, which is starvation");
}

process.stdout.write(
  "ALSA HEADROOM OK rate=" + rate + " period=" + period + " buffer=" + buffer +
  " availMax=" + availMax + " inHand=" + inHand + " " + ms.toFixed(2) + "ms\n");
