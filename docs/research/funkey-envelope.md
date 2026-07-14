# FunKey-S runtime envelope + reusable inventory (from ssb64-funkey-s)

Research ticket #5. Source: the completed/advanced sibling port at
`/Users/chase/code_projects/ssb64-funkey-s` (read-only; local, private). All numbers below are
*measured on the real device* by that project's autonomous loop unless marked otherwise.
File pointers are into the ssb64 repo unless prefixed otherwise.

---

## 1. The device envelope as actually experienced

### Hardware ground truth
- Allwinner V3s: **single-core Cortex-A7 @ 1.2 GHz** (NEON, VFPv4, hard-float), **64 MB DDR2**,
  **no GPU**, 240×240 IPS panel. (`docs/funkey-target.md`, `CONTEXT.md`)
- Frame budget at 59.94 fps: **~20.0M CPU cycles/frame, total** (sim + render + present + OS).
  (`docs/fps/SIXTY.md` §Budget math)
- I-cache is 32 KB — ssb64 keeps everything at `-O2` and puts `-O3` only on the one hot renderer
  TU specifically to avoid I-cache bloat (`Makefile.funkey`, comment at the `gfx_render.o` rule).

### RAM available to a process (measured, D-0 telemetry)
From a 330-sample @1 Hz run during a live VS match, shipping config
(`docs/DEVICE-BACKLOG.md` §D-0, 2026-07-01):
- Game **VmRSS: constant 22,444 kB** (zero growth over 5.5 min).
- **MemAvailable: min 26,796 kB, p50 27,028 kB** while the game runs.
- MemFree sits at ~1.7 MB — that is page cache, not pressure; read MemAvailable, not MemFree.
- Practical envelope for us: **a ~22 MB RSS game leaves ~26 MB available** → plan meleelight's
  native port around a **≤32 MB total footprint** and it will never be the constraint.
  (meleelight's state + 2D assets are tiny by comparison; RAM is a non-issue *unless* you log
  to tmpfs — see gotchas.)

### CPU headroom / framerate actually achieved (the sobering numbers)
- SSB64's full N64-style software rasterizer (perspective-correct textured triangles,
  combiner/blender emulation, ~320×240 internal, ~80K textured px/frame at 87% fb coverage)
  measured **~9 FPS rendered** on-device even after a −21% instruction-count optimization
  campaign; game speed held at 100% only via frameskip (sim at wall-clock 59.94 Hz, render
  disabled on catch-up frames). (`docs/fps/log.md` "DEVICE CHECKPOINT")
- Observed cost ≈ **1,667 cycles/pixel** — ssb64's own agents judged that "~50× too slow =
  structural waste, not a NEON gap"; estimated ceiling without resolution changes ~22–27 FPS.
- SAFE (bit-exact) optimization ceiling in that codebase: **−26.56% CEst** over 13 iterations;
  big wins were structural/compiler (force-inline hot leaves −15%, `-O3` on the renderer −6%),
  micro-opts were noise. (`docs/fps/log.md`, `docs/fps/results.tsv`)
- The 60fps campaign (`docs/fps/SIXTY.md`) was still ~2.7× over budget when paused.

**What this means for meleelight:** the hard 60fps@240×240 requirement is *achievable but only
because meleelight is not a textured-3D rasterizer*. Budget framing: 240×240 = 57,600 px;
20M cycles/frame ≈ **~347 cycles/pixel for EVERYTHING** if you repaint the full screen every
frame. MeleeLight's browser renderer is vector/canvas 2D (flat/gradient polygon fills, few
entities). Flat-filled spans at <10 cycles/px are fine; anything resembling per-pixel N64
combiner math is instantly dead. Design rules the ssb64 data dictates:
1. Render natively at 240×240 (never render wide and downscale — ssb64's single largest
   *unclaimed* win was rendering pixels the panel never showed).
2. Span-fill polygons, integer/fixed-point interpolants, no per-pixel branching/getenv/calls
   (each of those was a measured multi-percent cost in ssb64).
3. Sim cost matters on ONE core: ssb64 warns that if sim alone eats >20% of the frame you must
   profile it (SIXTY.md item G). MeleeLight's JS physics ported to C should be far under that,
   but ECB collision + hitbox math wants fixed-point care.
4. Keep the frameskip structure anyway (sim at wall-clock rate, render skippable) as a safety
   valve — it is ~30 lines in `port_main.c` and made the game *playable* during the whole
   optimization campaign.

### Display / present path (`port/gfx/gfx_present_sdl1.c` — read this file first)
- `SDL_SetVideoMode(240, 240, 16, …)` with a **fallback chain**
  HWSURFACE|DOUBLEBUF → SWSURFACE|DOUBLEBUF → SWSURFACE → 0 (the FunKey driver may not honor
  the optimistic flags; don't hard-crash).
- **Verify `BitsPerPixel == 16`** after SetVideoMode and bail to headless if not — the blit
  assumes 16bpp.
- Don't assume RGB565: ssb64 builds a **32K-entry LUT via `SDL_MapRGB`** from the *actual*
  surface format (RGBA5551-source → surface pixel), one lookup per pixel. If meleelight renders
  straight into the surface format this whole conversion disappears — even better.
- `SDL_ShowCursor(SDL_DISABLE)`; `SDL_Flip()` to present; lock/unlock via `SDL_MUSTLOCK`.
- The kernel framebuffer is **240×720 (3 flip pages)**; raw fb reads / `fbgrab` of a live game
  hit the wrong page nondeterministically — screenshot from *inside* the app (dump your own
  framebuffer), not from the fb device. (`docs/HANDOFF.md` §ADB)

### Input (measured on-device, not the wiki story)
- The FunKey firmware delivers buttons as SDL 1.2 **keyboard events with MNEMONIC LETTER
  keysyms** (confirmed from on-device key logs — it is NOT the OpenDingux LCTRL/LALT/arrows
  layout): `u/d/l/r` = D-pad, `a/b/x/y` = face buttons, `s` = START, `k`/`n` = L/R shoulders,
  **`q` = the Fn/MENU button**. (`port/gfx/gfx_present_sdl1.c` lines ~147–190)
- Poll with `SDL_GetKeyState` each frame; OR-in desktop keys too so one TU serves device + dev.
- MENU (`q`) should open an in-app menu with a QUIT option (`port/gfx/fk_menu.c` — SDL_ttf
  text menu on the same surface); **Fn+Up** is the OS screenshot chord
  (→ `/mnt/FunKey/snapshots/IMG_NNNN.PNG`).
- ssb64 maps D-pad → full-deflection analog stick (±80); for meleelight the same question
  (smash inputs from digital D-pad — ramped stick or modifier button) is a design item; ssb64's
  planned answer was "D-pad → ramped stick, R = flick/smash" (`port/README.md`).

### Audio
- **No data.** ssb64 never shipped audio: `port/audio/ai_sink.c` is a deliberate *silent sink*
  (accounting only) and the decomp's `audio.c` is DECOMP_EXCLUDEd. The SDK's SDL 1.2 has an
  audio backend, but nothing here measured its latency/CPU cost. Treat the FunKey audio path
  as **unknown territory** for meleelight; budget a spike. (The NOTICES file shows the intended
  plan: lift BattleShip's MIT mixer — never executed.)

---

## 2. Toolchain, packaging, deploy

### Cross toolchain (works today, verified end-to-end)
- Docker image **`jondbell/funkey-s-sdk`** (amd64; runs fine under emulation on Apple Silicon
  for compiling). SDK root `/opt/FunKey-sdk-2.3.0`.
- Cross compiler: **`arm-funkey-linux-musleabihf-gcc` (gcc 10.2)** — already targets
  Cortex-A7 + NEON + hard-float by default (`-mcpu=cortex-a7` measured as a no-op).
- Canonical invocation (CLAUDE.md §Commands):
  ```
  docker run --rm -v "$PWD":/work -w /work jondbell/funkey-s-sdk bash -lc \
    'export PATH=/opt/FunKey-sdk-2.3.0/bin:$PATH; make -f Makefile.funkey funkey-device'
  ```
- **`sdl-config` is NOT on PATH** — full path:
  `/opt/FunKey-sdk-2.3.0/arm-funkey-linux-musleabihf/sysroot/usr/bin/sdl-config`
  (`Makefile.funkey` SDLCFG variable). SDK ships SDL 1.2 + SDL_ttf + freetype + SDL_image.
- musl, ILP32. Two ABI classes that bit ssb64 hard and apply to ANY port: 64-bit `off_t`/
  `time_t` on arm32-musl vs hand-declared 32-bit externs (silent corruption — the 0-byte save
  bug), and pointer-width assumptions (stride from `sizeof`, never a literal). meleelight-from-JS
  will hand-write all its C, so mostly moot — but keep the `abi_extern_audit.sh` idea if any
  libc fn gets hand-declared.
- Run `docker` builds **serially** — parallel SDK containers crashed the VM (SIXTY.md METHOD 6).

### OPK packaging (`packaging/funkey/` — four small files, copy the lot)
- `build-opk.sh`: stage dir → `mksquashfs $STAGE out.opk -all-root -noappend -no-exports
  -no-xattrs -comp gzip`. **CRITICAL: use the SDK container's mksquashfs (4.4).** A newer host
  mksquashfs (4.7) writes a squashfs the FunKey kernel cannot mount and the .opk **silently
  fails to launch** — no error anywhere. (Comment block at the top of `build-opk.sh`.)
- `.funkey-s.desktop` (Name/Comment/Exec/Icon/Categories=games) — **mandatory trailing empty
  line**; 32×32 PNG icon.
- `Exec=` a **launcher shell script**, not the binary. `ssb64.sh` is the template worth copying:
  finds a writable SD dir, sends the live log to **tmpfs not the SD** (SD streaming = 2–3 s
  in-game stalls), copies diagnostics back to SD on exit via a trap, locates data files with an
  env-var override chain, puts the **save file on the SD** (tmpfs is wiped at power-off).
- The OPK mounts **read-only** at runtime — CWD is the squashfs; anything written goes to
  `/tmp` (RAM) or `/mnt` (SD).

### Getting builds onto the device — ADB, not SD-shuffling
(`docs/HANDOFF.md` top section — the single most time-saving discovery in the repo)
- A marker file named `adb` at the SD root makes every boot start **adbd over USB** instead of
  the mass-storage gadget. Root shell, no password. Game runs while ADB is live.
- Deploy: `adb push build-arm/<name>.opk /mnt/Applications/<name>.opk`.
- Launch headlessly, the three mandatory pieces: `sh -lc` (login shell — `/etc/profile` sets
  `SDL_NOMOUSE=1`; without it **SDL_Init dies with "Unable to open mouse"**), `setsid …
  </dev/null` (adbd kills children of a closed session), and a trailing `sleep 2` (setsid must
  exec before the session exits). Park the frontend first:
  `touch /mnt/disable_frontend; pkill gmenu2x`; restore with `rm /mnt/disable_frontend`.
- Button injection without hands: `tools/fk_input.c` — a ~static uinput keyboard binary pushed
  to the SD (`/mnt/fk_input`), tokens like `a`, `hold:u:500`, `sleep:250`. Writing to the
  existing event device does NOT inject; you must create your own uinput device.
- Telemetry: on-device background scripts starve at 100% game CPU (single core) — drive
  sampling from the host, one `adb shell` per sample.

---

## 3. The thin platform-API pattern — liftable nearly wholesale

**Where it lives:** `port/gfx/gfx_present.c` (SDL2 dev), `port/gfx/gfx_present_sdl1.c`
(SDL 1.2 device), `port/gfx/gfx_present_stub.c` (headless CI). Policy statement:
`CLAUDE.md` §"SDL / platform" + `docs/funkey-target.md` §Decision.

**The seam:** exactly one TU is linked per target (chosen in the Makefile — `funkey-device`
links sdl1, `funkey` links stub, host links SDL2). Each TU implements the same five functions
plus three globals:

```c
int  port_sdl_enabled(void);   // env-gated (PORT_SDL / PORT_HEADLESS)
int  port_sdl_init(void);
void port_sdl_present(void);   // pulls pixels via port_gfx_get_framebuffer(&w,&h)
int  port_sdl_poll(void);      // fills port_sdl_button / port_sdl_stick_x / _y; returns quit
void port_sdl_quit(void);
```

The renderer knows nothing about SDL; it exposes `port_gfx_get_framebuffer()`. Each backend is
~100–200 self-contained lines. **Verdict: lift wholesale.** For meleelight rename the seam
(`platform_init/present/poll/quit`), swap the "N64 controller bits" globals for a
meleelight input struct, and decide the internal pixel format up front (rendering directly in
the device's 16bpp surface format kills ssb64's per-pixel LUT conversion entirely). The
headless backend is what makes an autonomous loop possible (deterministic frame dumps, CI
without a display) — take all three backends, not just the device one.

Adjacent liftables in the same layer: `port_main.c`'s crash handler (SA_SIGINFO
`arm_pc/arm_lr/fault_address` logging + `addr2line` on a non-PIE binary — priceless for
on-device crashes), the `[freeze]` hitch logger (logs any frame whose work exceeds N ms with
free-RAM context), and the wall-clock-sim/skippable-render main loop.

---

## 4. Loop / verification infrastructure worth adapting

What the ssb64 loop got right (all of it ran unattended, overnight, for weeks):

- **`CLAUDE.md` HARD RULES** (7 rules): behavior > compiles; no stubs-as-done; never
  edit/weaken tests/oracle/gates; one branch (`agent/auto`), one atomic commit per iteration;
  writer ≠ checker; output to `.loop/*.log` never the conversation. Copy the block verbatim.
- **`docs/LOOP.md`** — one *bounded* iteration per invocation, state on disk, guards (branch,
  clean tree, state files) → orient → decide (task / REPLAN / phase-advance) → small diff →
  verify → commit. Plus an explicit AUTONOMOUS MANDATE section (make decisions, install tools,
  route around blockers, reserve human-stops for device/legal/destructive).
- **`docs/loop/CHECKER.md`** — the independent verifier contract: re-runs the exact
  `done-check:` command, checks artifacts, **tamper-checks the diff** (did the writer touch
  oracle/gates/tests?), returns strict JSON. This caught a real incident: the optimizer
  loosened its own perceptual gate to admit its change (`docs/fps/log.md` "ANTI-CHEAT
  VIOLATION"). Lesson to carry: **gate ownership lives outside the loop.**
- **Phase-gate table** (`CLAUDE.md` §Per-phase gates): conceptual `…` cells until REPLAN
  concretizes each into an exact runnable command; per-task `done-check:` ≠ phase exit gate;
  a designed HUMAN ESCALATION row for the first on-device perf check.
- **`fix_plan.md` discipline**: priority queue where every actionable item carries an exact
  runnable `done-check:` shell command; REPLAN iterations (not ad-hoc judgment) rewrite vague
  items.
- **`scripts/loop.sh`** — external fresh-context driver (`claude -p` per iteration; cannot
  overflow context on a long run), stops on a `LOOP STOP:` sentinel line, `.loop/STOP` file, or
  MAX_ITERS. `LAUNCH.md` is the 40-line arming checklist.
- **The fps harness pattern** (`docs/fps/LOOP.md`, `tools/fps_bench.sh`, `docs/fps/results.tsv`):
  one optimization per iteration; SAFE tier gated by **byte-exact every-frame framebuffer
  hashes** vs a frozen golden (`fps_golden/`); RISK tier gated by a structure-aware regional-ΔE
  metric (`tools/regional_de.py`) with committed un-loosenable thresholds; every iteration
  (kept OR reverted) logged to a TSV ledger. Metric caveat they proved: cachegrind-based cost
  estimates are a *ranker* — device fps is ground truth (−21% CEst bought only +2 device fps).
- **Fail-closed audits wired into packaging**: `tools/stub_audit.sh` (nm scan — no gameplay
  symbol silently resolved to a weak no-op stub) and `tools/abi_extern_audit.sh`, both run by
  `build-opk.sh` and refusing to package on failure.

**Adaptation for meleelight:** the oracle is different (the browser meleelight is a *runnable
reference* — deterministic input replay in the browser vs the C port, pixel/state diffs — far
stronger than ssb64's situation) but the harness shape (frozen goldens, byte-exact SAFE gate,
ΔE RISK gate, TSV ledger, writer≠checker) transfers one-to-one. The fps CEst harness matters
less if we hit 60 early; the device-fps logger (`PORT_FPSLOG` pattern) matters more.

---

## 5. Licensing hygiene

- **`NOTICES`** at repo root: per-upstream attribution blocks (copyright line, URL, license,
  exact files lifted) — written *before* code was copied, as a plan.
- **`docs/LICENSING.md`**: a verdict-style reuse map (what may be lifted, under what terms,
  what is gray-zone, what is banned), plus a one-line provenance rule mirrored into CLAUDE.md
  ("in-tree code is MIT only, with notices; no GPL in-tree").
- **Submodule non-claim pattern**: gray-zone code (the decomp) stays a submodule, no copyright
  claim, no assets shipped, user supplies the ROM/data. The OPK ROM-bundling convenience is
  explicitly local-only and gitignored (`build-opk.sh` OPK_BUNDLE_ROM block).
- **For meleelight**: check MeleeLight's own upstream license FIRST and record it in our
  LICENSING.md before porting a line; carry the NOTICES habit; we ship no Melee assets —
  meleelight's synthesized/vector look is actually the clean case here.

---

## REUSABLE INVENTORY (copy / adapt / leave)

### COPY (near-verbatim)
| Asset | Path (ssb64 repo) |
|---|---|
| SDL 1.2 device present+input backend (240×240 16bpp, format-safe LUT, FunKey keymap, menu hook) | `port/gfx/gfx_present_sdl1.c` |
| Headless + SDL2 dev backends (same 5-fn seam) | `port/gfx/gfx_present_stub.c`, `port/gfx/gfx_present.c` |
| OPK packaging kit (mksquashfs-4.4 recipe + audits + desktop + icon) | `packaging/funkey/build-opk.sh`, `ssb64.funkey-s.desktop` |
| Launcher script patterns (SD detect, tmpfs log + copy-back trap, save-on-SD, env overrides) | `packaging/funkey/ssb64.sh` |
| ADB device-access recipe + uinput button injector | `docs/HANDOFF.md` §ADB, `tools/fk_input.c` |
| Loop protocol + independent checker + hard rules | `docs/LOOP.md`, `docs/loop/CHECKER.md`, `docs/loop/REPLAN.md`, `CLAUDE.md` HARD RULES, `scripts/loop.sh`, `LAUNCH.md` |
| ARM crash handler + freeze logger + sim/render frameskip loop | `port/port_main.c` |
| In-game MENU-button menu (SDL_ttf) | `port/gfx/fk_menu.c` |

### ADAPT
| Asset | Path | Change |
|---|---|---|
| Cross-build Makefile (CROSS prefix, sdl-config path, -O3-hot-TU-only, per-target present TU) | `Makefile.funkey` | swap source lists; keep structure |
| fps/verification harness (goldens, SAFE byte-exact gate, RISK regional-ΔE, TSV ledger) | `tools/fps_bench.sh`, `tools/regional_de.py`, `docs/fps/LOOP.md`, `docs/fps/results.tsv` | oracle = browser meleelight replay instead of frozen self-goldens |
| Phase-gate table + fix_plan discipline | `CLAUDE.md` §Per-phase gates, `fix_plan.md` | meleelight-specific phases (JS→C transliteration gates, parity replay gate, device fps gate) |
| Licensing docs | `NOTICES`, `docs/LICENSING.md` | re-verify against MeleeLight upstream license |
| Porting-kit meta-model (3 layers, device-access triad: access channel / button injector / frame capture) | `docs/PORTING-KIT-PLAN.md` | our project IS its intended "port #2" for the meta-loop layer |

### LEAVE (ssb64/N64-specific)
- The entire softrast + GBI/TMEM renderer (`port/gfx/gfx_render.c`) and all N64 texture/DL
  machinery — meleelight is 2D vector rendering, none of it applies.
- libultra shims, cooperative scheduler, byteswap/reloc/figatree machinery (`port/sys/*`),
  weak-stub system (`port/stubs/*`) — decomp-specific.
- `oracle/gbi_diff.py`, dl-golden cases, `render_delta.sh` (arm-vs-host DL hash oracle) —
  concept transfers, artifacts don't.
- The silent audio sink — we need real audio; nothing to reuse there.

---

## Headline numbers (for the wayfinder)
- **RAM**: 64 MB total; game VmRSS 22.4 MB constant; ~27 MB MemAvailable during play. Not our
  constraint.
- **CPU**: 1× A7 @1.2 GHz = ~20M cycles/frame @60 fps ≈ ~347 cycles/px at 240×240 full repaint.
- **ssb64's textured-3D softrast: ~9 fps device** (≈1,667 cyc/px) after −21% opt; est. ceiling
  22–27 fps. A 2D vector fighter at native 240×240 lives in a different, feasible regime — but
  60 fps must be engineered from day one (span fills, fixed-point, no per-pixel calls), not
  optimized in later.

## Top 3 device gotchas for a new port
1. **mksquashfs version**: package the OPK with the SDK's mksquashfs 4.4 — a newer one produces
   an OPK the kernel can't mount and it **silently** fails to launch.
2. **Launch environment**: SDL_Init dies ("Unable to open mouse") without `SDL_NOMOUSE=1` (set
   by `/etc/profile` — always launch via a login shell / `sh -lc`); ADB launches need
   `setsid … </dev/null … & sleep 2`; the surface may come back non-16bpp or without
   HWSURFACE|DOUBLEBUF — probe a fallback chain and verify bpp. Buttons arrive as letter
   keysyms (`u/d/l/r/a/b/x/y/s/k/n`, MENU=`q`), not the OpenDingux layout.
3. **Storage vs the one core**: streaming logs/dumps to the SD mid-game stalls it for seconds;
   logging to tmpfs eats the scarce free RAM (reclaim thrash); background scripts starve at
   100% game CPU. Log quiet to tmpfs, copy back on exit, drive telemetry from the host over ADB.
