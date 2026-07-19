# PORTABILITY.md — the FunKey-S-specific surface (owner ruling 2026-07-19)

Chase's directive: FunKey-S-specific port machinery must be NOTED and
MANAGEABLE so a future port to another device is easy. This file is the
living inventory. Going forward, any writer that adds device-specific
code/behavior adds a row here in the same commit (the driver enforces
at review); the driver keeps the classification honest.

The architecture already isolates most device specifics behind seams —
the port cost for a new target is the DEVICE-BOUND column, not the sim.

## Layer 0 — device-agnostic by construction (zero port cost)

- The entire sim (`port/sim/` minus `device/`): doubles-only C,
  fdlibm-pinned, no OS calls in the frame path. Bit-exact anywhere with
  IEEE-754 + the vendored math (proven: host arm64 macOS == armv7 musl).
- The data pipeline (`pipeline/`), oracle (`oracle/`), all goldens.
- The renderer core (`port/gfx/gfx_render.c`, `raster.c`, vfx, glyphs
  consumption): draws into a 240x240 RGB565 buffer in memory. The SIZE
  and FORMAT are FunKey-tuned constants (see Layer 2).
- The AI (`port/sim/ai.c`), audio MIXER math, input INTERPRETATION.

## Layer 1 — the platform seam (per-target TU, by design)

`port/gfx/platform.h`: `platform_init/present/poll/quit` + input struct.
Exactly ONE backend TU linked per target — this is the intended port
point (CLAUDE.md §SDL policy, ssb64 lineage):
- `platform_headless.c` — CI/loop (portable)
- `platform_sdl2.c` — host dev (portable)
- `platform_sdl1.c` — **FunKey-S**: SDL1.2, 240x240x16 SetVideoMode
  fallback chain, BitsPerPixel==16 bail, SDL_Flip, the measured
  FBIOPAN_DISPLAY error whitelist (kernel-specific!), audio open
  44100/S16LSB/2ch/512.
A new device = write one new backend TU (+ audio open params).

## Layer 2 — FunKey-tuned constants (port = re-measure + re-pin)

- Screen: 240x240, RGB565, letterbox transform k=0.2/dy=45
  (`Gfx` constants; the browser-parity IoU pins bake these).
- `GFX_LEGIBLE_MIN_DEV_PX = 2.0` (gfx.h) — stroke floor for the 1.54"
  panel; re-derive per panel size/DPI.
- Render perf budget: p99 < 16.67ms, render ≤ 8ms — PLAN §5 numbers
  measured on the V3s Cortex-A7; re-measure per SoC.
- Audio: 22050 SFX / 44100 out, 8 voices, 512 buffer — measured fit for
  this SoC (issue #12); re-measure.
- Input: the S1 chord table (PLAN §6) maps the FunKey's EXACT control
  set (d-pad + 8 buttons, letter keysyms u/d/l/r/a/b/x/y/s/k/n/q).
  A device with real analog or more buttons gets a NEW mapping table
  (data-driven by design — `port/gfx/s1_input.h`); the S1 semantics
  (SOCD, tap-jump-off, digital shield) are Chase-ratified for THIS
  hardware.

## Layer 3 — device-bound machinery (rewrite per target)

- **Device libc/kernel quirk fixes** (the recurring class — 3 instances):
  FunKey SDK's static musl libm is FP-unsafe → exact floor/ceil/fmod/
  round/lround strong overrides in `port/fdlibm/fdlibm.c` (harmless
  elsewhere — KEEP for any target; the mathsweep differential instrument
  re-validates any new toolchain in one run); musl-1.2 time_t vs old
  kernel `input_event` ABI (fk_input.c 16-byte layout); FBIOPAN_DISPLAY
  whitelist (platform_sdl1.c). NEW-TARGET RULE: run mathsweep + the
  ABI-size asserts against the new toolchain FIRST (the instruments are
  the port tool).
- **The device rig** (`port/sim/device/`: adbsh nonce-RC over a broken
  adbd, riglib stamp/pullv/lock/deadman/park, the gmenu2x frontend
  nav, `low_bat_check` quiesce): ~all FunKey-OS-specific. The PATTERNS
  (identity pins, freshness, fail-closed) transfer; the bytes don't.
- **OPK packaging** (mksquashfs 4.4 pin, .desktop, /mnt layout,
  launcher data-dir chain): OPK is the FunKey/gmenu2x format; another
  device has its own packaging.
- **PMIC stall mitigation**: `low_bat_check` is FunKey OS's daemon;
  the skip-attribution INSTRUMENT (`skip-attrib/`) is the portable
  part — run it on any new device to find ITS stall sources.

## Porting recipe (when a new target appears)

1. Toolchain audit: mathsweep + fmt corpus + ABI asserts vs the new
   libc/kernel (Layer 3 instruments). Fix by strong-override, never
   by weakening.
2. New platform backend TU (Layer 1) + audio-open params.
3. Re-measure Layer 2 constants (perf budgets, legibility, audio fit);
   new input mapping table for the control set (human ratification).
4. Rebuild the device rig for the new transport/OS (Layer 3 patterns).
5. The gate structure (conformance/perf/packaging/live-session) ports
   as-is; its evidence producers get re-pinned per PROCESS §4.
