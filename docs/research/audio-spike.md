# Device audio spike — SDL 1.2 audio on the real FunKey-S

Wayfinder ticket #12. First-ever measurement of the FunKey-S audio path
(ssb64 shipped a deliberate silent sink; this path was unmeasured until now).

- Device: FunKey-S, FunKey-OS 2.3.0 (Zen Zebu), kernel 4.14.14-funkey,
  single Cortex-A7 @ 1.2 GHz, ADB id `12c00003237f5528`, measured 2026-07-14.
- Test program: `spikes/device-audio/audiotest.c`, cross-compiled with the
  `jondbell/funkey-s-sdk` docker image (SDK SDL 1.2, musl hard-float).
  Build: `spikes/device-audio/build.sh`; run matrix:
  `spikes/device-audio/run-device.sh`; raw logs: `spikes/device-audio/logs/`.
- Method notes: frontend parked (`touch /mnt/disable_frontend; pkill gmenu2x`)
  for every run; launched via login shell (`sh -lc`). "Late callback" =
  inter-callback interval exceeding 1.25×/1.5×/2× the nominal period,
  measured with CLOCK_MONOTONIC inside the SDL audio callback — the
  starvation/underrun proxy. Callback CPU cost measured with
  CLOCK_THREAD_CPUTIME_ID around the fill code only.

## 1. Does SDL 1.2 audio work? What does SDL_OpenAudio grant?

**Yes.** Driver is **`alsa`**. Every requested combination was granted
*exactly* — no renegotiation (log `01-probe.log`):

| Request (S16, stereo) | Obtained freq | Obtained format | ch | samples | nominal cb period |
|---|---|---|---|---|---|
| 44100 / 256  | 44100 | 0x8010 (S16LSB) | 2 | 256  | 5.80 ms |
| 44100 / 512  | 44100 | 0x8010 | 2 | 512  | 11.61 ms |
| 44100 / 1024 | 44100 | 0x8010 | 2 | 1024 | 23.22 ms |
| 44100 / 2048 | 44100 | 0x8010 | 2 | 2048 | 46.44 ms |
| 22050 / 256  | 22050 | 0x8010 | 2 | 256  | 11.61 ms |
| 22050 / 512  | 22050 | 0x8010 | 2 | 512  | 23.22 ms |
| 22050 / 1024 | 22050 | 0x8010 | 2 | 1024 | 46.44 ms |
| 22050 / 2048 | 22050 | 0x8010 | 2 | 2048 | 92.88 ms |

Callback cadence matched nominal in every probe (audible tone confirmed by
callback accounting; 300 ms per config).

## 2. Latency proxy — smallest clean buffer under ~80 % CPU load

60 s tone per config with a busy thread at ~80 % duty cycle (8 ms spin /
2 ms sleep per 10 ms period) on the single core (logs `02`–`05`):

| Config | nominal | cbs (got/expected) | late >1.25× | late >1.5× | late >2× | max gap |
|---|---|---|---|---|---|---|
| 44100 / 256  | 5.80 ms  | 10324 / 10336 | 98 | 13 | **8** | 17.6 ms |
| 44100 / 512  | 11.61 ms | 5164 / 5168   | 4  | 1  | **0** | 19.1 ms |
| 44100 / 1024 | 23.22 ms | 2582 / 2584   | 1  | 0  | **0** | 30.4 ms |
| 22050 / 256  | 11.61 ms | 5163 / 5168   | 5  | 3  | **0** | 18.6 ms |

- **44100/256 is dirty under load**: 8 gaps of >2 periods in 60 s (~1 per
  7.5 s) — each one drains more than the queued audio → audible dropouts.
- **44100/512 is the smallest clean buffer**: zero >2× gaps in 60 s; the
  single 1.5× event (19.1 ms gap) is still under the ~23 ms of audio queued
  by two 512-sample periods → inaudible. 22050/256 behaves the same (same
  11.6 ms period), slightly worse tail.
- Practical output latency at 44100/512 ≈ 2 periods ≈ **~23 ms**, ~1.4
  game frames — fine for SFX feel.

## 3. Mixing cost — 8 voices + music in the callback

Mixer modeled on the intended port design: 8 looping **22050 Hz mono**
voices resampled to output rate with a 16.16 fixed-point phase accumulator
+ per-voice Q8 gain, plus 1 **pre-decoded stereo music** stream at output
rate read from RAM, int32 accumulate + clamp, S16 stereo out. At
44100/512 (logs `06`, `07`):

| Scenario | cb CPU avg | cb CPU max | audio CPU per 60 fps frame | % of 16.67 ms budget | late >2× |
|---|---|---|---|---|---|
| 8 voices + music, idle    | 145.4 µs | 834 µs | 0.209 ms | **1.25 %** | 0 |
| 8 voices + music, 80 % load | 144.6 µs | 885 µs | 0.208 ms | **1.25 %** | 0 |
| (tone baseline, 80 % load)  | 96.2 µs  | 852 µs | 0.138 ms | 0.83 % | 0 |

- Full 8-voice + music mix costs **1.25 % of the frame budget** (~0.21 ms
  per 60 fps frame, ~250 K cycles/s of the 1.2 GHz core). The 8-voice
  increment over the sine baseline is ~0.4 % of budget → **per-voice cost
  ≈ 0.05 % of budget**. Headroom is enormous: even 16 voices + music stays
  under ~2 %.
- Mixing under 80 % load produced **zero hard-late callbacks over 60 s** —
  identical to the tone baseline. The mixer adds no underrun risk.

### Music RAM/streaming cost (pre-decoded PCM, per the spike's constraint)

Ogg decode was *not* measured (out of scope). If music ships as raw PCM:

| Strategy | Rate | RAM cost | Verdict |
|---|---|---|---|
| Whole track pre-decoded, 44100 stereo S16 | 176.4 KB/s | ~31 MB / 3-min track | impossible (≤32 MB total budget) |
| Whole track pre-decoded, 22050 stereo S16 | 88.2 KB/s | ~15.5 MB / 3-min track | still too much |
| **Streamed raw PCM from SD, 22050 stereo S16** | **88.2 KB/s** | **2×64 KB double buffer = 128 KB** | **the answer** |
| Streamed, 22050 mono S16 | 44.1 KB/s | 128 KB | fallback if SD space matters |

SD-space cost of pre-converting the 8 upstream tracks (29 MB of ogg) to
22050 stereo S16 raw: roughly 120–130 MB on the SD — cheap. The ~180 wav
SFX (7.1 MB as wav) pre-decoded to 22050 mono S16 in RAM stay in the
low-single-digit MB — within budget.

## 4. SD-card reads during playback

Tone at 44100/512 while a thread loops **O_DIRECT** 256 KB reads of a
13 MB file on the SD (logs `09`, `10`; log `08` is an earlier run that was
fooled by the page cache — kept for the record):

| Scenario | SD throughput | late >1.5× | late >2× | max gap |
|---|---|---|---|---|
| SD reads, idle | 21.3 MB/s | 0 | 0 | 13.7 ms |
| SD reads + 80 % load | 21.3 MB/s | 1 | 0 | 17.9 ms |

**SD reads do not disturb audio**, even combined with 80 % CPU load —
clean at full 21.3 MB/s sustained read rate. Music streaming needs only
88 KB/s (0.4 % of that). Note this contradicts nothing in the envelope
doc: its warning was about SD *writes/log streaming* stalling the game
loop, which remains true; audio-thread integrity is unaffected by reads.

## Verdict

- **Recommended SDL audio config: 44100 Hz, AUDIO_S16LSB, 2 channels,
  512-sample buffer** (~11.6 ms callback period, ~23 ms output latency).
  Drop to 1024 samples only if real-game hitches ever show up (none did at
  80 % synthetic load). 44100/256 is not safe under load.
- **Measured mix headroom:** 8 SFX voices + 1 music stream = **1.25 % of
  the 16.67 ms frame budget**; ~0.05 %/voice marginal cost. Audio is a
  rounding error next to rendering.
- **Recommendation: full SFX + music**, with one constraint: **music is
  pre-decoded to raw PCM at build/install time and streamed from SD at
  22050 Hz stereo S16 (88.2 KB/s, ~128 KB RAM double-buffer, ~120–130 MB
  SD space for all 8 tracks)** — whole-track PCM in RAM does not fit the
  64 MB device, and runtime ogg decode is unnecessary risk/complexity that
  was deliberately left unmeasured. SFX stay pre-decoded in RAM at
  22050 Hz mono. Keep ogg only as the *source* format in the repo; the
  device never sees it.

*(Spec impact: audio section can commit to full SFX+music; ogg stays in —
as an offline source format, not a runtime one.)*
