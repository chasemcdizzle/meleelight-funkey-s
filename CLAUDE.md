# CLAUDE.md — always-loaded rules for the meleelight → FunKey-S port

Faithful C port of browser meleelight (upstream pin `27af171`) to the
FunKey-S, verified by deterministic input-replay + per-frame state
checksums against the browser original. Spec: [`PLAN.md`](./PLAN.md) ·
Loop: [`docs/LOOP.md`](./docs/LOOP.md) · Checker:
[`docs/loop/CHECKER.md`](./docs/loop/CHECKER.md) · Replanner:
[`docs/loop/REPLAN.md`](./docs/loop/REPLAN.md) · Licensing:
[`docs/LICENSING.md`](./docs/LICENSING.md) · Evidence:
`docs/research/`, `spikes/`, `prototypes/`.

## HARD RULES (non-negotiable; every iteration)

1. **Behavior > compiles.** Code must do what's asked, validated against
   the oracle (checksum conformance), never "it builds".
2. **No stubs / placeholders / hardcoded outputs / "TODO later"** as a
   stand-in for real work. Deferrals go under `BLOCKERS` in
   `docs/AGENT-LOG.md`, never buried in code.
3. **Never edit/delete/weaken** any test, the oracle (`oracle/` once it
   exists, `spikes/determinism/` harness + committed golden checksum
   streams), the gates, this file's rules, `docs/LOOP.md`, `docs/loop/*`,
   the caps, or git hooks. Exact-equality checksums NEVER become
   epsilon comparisons. Only M0 tasks may write `oracle/`.
4. **Git safety:** the autonomous run lives on ONE branch, **`agent/auto`**
   (the loop aborts anywhere else); ONE atomic commit per completed
   iteration, clean tree between; **never** force-push, `reset --hard`, or
   delete branches; `main` receives only human/spec commits. **NEVER add,
   push to, or open PRs against `schmooblidon/meleelight` or any upstream
   remote** — upstream clones live outside the tree. **No distribution of
   anything** (binaries, OPKs, assets, forks): private project.
5. **Faithfulness:** the browser original is ground truth; a behavioral
   deviation is a bug even when it "feels better". Engine values (physics
   constants, frame data, thresholds) come from the executed-data pipeline,
   never retyped by hand. Sim math is doubles-only, vendored fdlibm for
   transcendentals, `-ffp-contract=off` on every TU (PLAN §2).
6. **One task per iteration.** Command output → `.loop/*.log`, never into
   the conversation.
7. **Writer ≠ checker.** Completion is confirmed by the CHECKER
   ([`docs/loop/CHECKER.md`](./docs/loop/CHECKER.md)) gating on
   artifacts/exit-codes, never self-claims.
8. **ZOOM OUT (Chase, 2026-07-14).** Before and after fixing anything, ask
   whether it is an instance of a CLASS with a systematic cause; prefer the
   class-level fix when tractable. Hierarchy: **instrument > class fix >
   registered one-off > silent one-off (never)**. One-offs are acceptable
   late-stage/perf only AFTER measurement attributes the hotspot. Every
   root-cause/fix session ends with an explicit zoom-out note in
   `docs/AGENT-LOG.md`.

## SDL / platform seam (single source of truth)

**Thin platform API, three backends, exactly ONE TU linked per target**
(lifted from ssb64's `port/gfx` pattern): SDL 1.2 for the FunKey device ·
SDL2 for host dev · headless for CI/the loop. The renderer knows nothing
about SDL; it exposes a framebuffer the backend presents. Seam:
`platform_init / platform_present / platform_poll / platform_quit` + an
input struct. The headless backend is what makes the autonomous loop
possible — it lands in M2 with the first C build.

## Licensing / provenance rule

Upstream meleelight LICENSE carried **verbatim** (incl. its Nintendo-IP
rider) as `LICENSE-meleelight` — never edit it. `NOTICES` gains an entry
BEFORE any third-party code lands in-tree. No Nintendo-derived asset is
ever distributed (moot privately; kept as hygiene). SDL 1.2 is LGPL:
dynamic linking only. Details: `docs/LICENSING.md`.

## §Gates — MILESTONE EXIT gates only

Run via CHECKER **only on a phase-advance iteration**, never per task.
Per-iteration verification uses the in-progress `fix_plan.md` item's exact
`done-check:` instead. Cells marked *(REPLAN)* are precise definitions
whose runnable command is milestone output — REPLAN concretizes the exact
command into §Commands when the milestone becomes current; the definitions
live in PLAN §4 and are binding.

| Phase | Gate command | Pass condition |
|---|---|---|
| M0 | seed (spike-era, runnable today): `cd spikes/determinism && bash run-experiments.sh "$MELEELIGHT_CLONE"` — final form *(REPLAN)*: `bash oracle/verify_goldens.sh` | every golden trace: two fresh browser runs bit-identical, streams match committed checksums, fdlibm-patched QuickJS runtime reproduces them (5 chars / 6 stages covered) |
| M1 | *(REPLAN)* — two fresh pipeline runs + manifest hash check | byte-stable reruns; coverage = 754 anim files / ~27.9k paths / 5 chars / 6 stages / ~180 SFX + 8 tracks |
| M2-CAL | *(REPLAN)* — slice replay: C `environmental_collision` vs JS over the 3800-frame golden trace | bit-identical full trace + converged burn-down + recorded metrics (div/KLOC, fix-rate, projection). NO-GO → `LOOP STOP: m2-entry-no-go` (a blocker list is NOT a pass) |
| M2 | *(REPLAN)* — headless C sim replays all golden traces | every frame checksum == browser oracle stream, full match length, all goldens |
| M3 | *(REPLAN)* — device conformance + perf run over ADB | device checksums conform; p99 frame < 16.67 ms full match w/ audio; OPK launches from frontend. **HUMAN ESCALATION**: needs the physical device when absent (`LOOP STOP: m3-device`) + Chase's S1 ratification playtest |
| M4 | *(REPLAN)* — full-game trace suite on device | menu-flow scripts + match + target-test traces conform at 60 fps with audio; then **Chase acceptance playthrough** (`LOOP STOP: m4-complete`) |

**Gate concretization (enforced):** when a phase becomes current, REPLAN
turns two distinct things into exact runnable commands (no `…`): (a) each
task's `done-check:` (proves ONE item; every task iteration) and (b) the
phase EXIT gate above, recorded into §Commands (proves the WHOLE phase;
phase-advance only). CHECKER rejects any non-runnable/placeholder check.

## §Commands (verified; the loop appends as each phase defines them)

- **Upstream clone + build (proven twice — determinism spike + prototype):**
  ```
  git clone https://github.com/schmooblidon/meleelight "$MELEELIGHT_CLONE"
  cd "$MELEELIGHT_CLONE" && git checkout 27af171
  # apply the project patch (harness or mapping), drop dead devDeps
  # (deepstream.io, electron*) + the postinstall script, then:
  docker run --rm --platform linux/amd64 -v "$PWD":/app -w /app node:8 \
    bash -c "npm install --ignore-scripts && npm run animations && npm run build"
  ```
  (Full recipe + patch: `spikes/determinism/README.md`,
  `prototypes/control-mapping/README.md`.)
- **Oracle harness (runnable today):** `spikes/determinism/harness/` —
  `node run.js --dist "$MELEELIGHT_CLONE" --frames 3600 --seed 1337 [--cpu] --out out/a.json`
  twice, then `node compare.js out/a.json out/b.json` → `IDENTICAL`.
  Needs `npm i playwright` next to the harness (uses installed Chrome).
- **arm32 cross-compile (FunKey SDK 2.3.0 via docker `jondbell/funkey-s-sdk`):**
  ```
  docker run --rm -v "$PWD":/work -w /work jondbell/funkey-s-sdk bash -lc \
    'export PATH=/opt/FunKey-sdk-2.3.0/bin:$PATH; arm-funkey-linux-musleabihf-gcc \
     -O2 -ffp-contract=off <srcs> -o build-arm/<out> \
     $(/opt/FunKey-sdk-2.3.0/arm-funkey-linux-musleabihf/sysroot/usr/bin/sdl-config --cflags --libs) -lm'
  ```
  gcc 10.2, musl hard-float, targets Cortex-A7+NEON by default. `sdl-config`
  is NOT on PATH (full path above). Run docker builds SERIALLY. Image is
  amd64 — fine under emulation. `-ffp-contract=off` on every sim TU is a
  hard rule; `-O3` only on the hot raster TU.
- **QuickJS oracle-runtime build:** `spikes/device-feasibility/README.md`
  step 2 (bellard/quickjs @2026-06-04, single gcc invocation via `qjsmin.c`,
  static, 890 KB). M0 repoints its Math table at the vendored fdlibm.
- **Device access (ADB):** marker file `adb` at SD root → adbd on boot,
  root shell. Park frontend: `touch /mnt/disable_frontend; pkill gmenu2x`
  (restore: `rm /mnt/disable_frontend`). Launch via login shell **`sh -lc`**
  (else SDL_Init dies: "Unable to open mouse"); detached runs need
  `setsid … </dev/null` + trailing `sleep 2`. Buttons arrive as letter
  keysyms: `u/d/l/r` d-pad, `a/b/x/y` face, `s` START, `k`/`n` L/R, `q`
  MENU. Known-good device id: `12c00003237f5528`.
- **OPK packaging:** `mksquashfs $STAGE out.opk -all-root -noappend
  -no-exports -no-xattrs -comp gzip` — **use the SDK container's
  mksquashfs 4.4 ONLY** (newer versions produce an OPK the kernel silently
  fails to mount). `.desktop` file needs a trailing empty line; `Exec=` a
  launcher script, not the binary. OPK mounts read-only: write to `/tmp`
  (RAM, wiped at power-off) or `/mnt` (SD).

## Build/gotcha notes (the loop appends here)

- Log to tmpfs during play, copy to SD on exit (SD streaming = multi-second
  stalls); drive telemetry from the host over ADB (on-device background
  scripts starve at 100% game CPU). Read MemAvailable, not MemFree.
- Screenshot from INSIDE the app (dump own framebuffer) — the kernel fb is
  240×720 (3 flip pages), raw fb reads hit the wrong page. Fn+Up = OS
  screenshot chord → `/mnt/FunKey/snapshots/`.
- SDL_SetVideoMode fallback chain HWSURFACE|DOUBLEBUF → SWSURFACE|DOUBLEBUF
  → SWSURFACE → 0; verify `BitsPerPixel == 16` after init or bail.
- Upstream expected console noise: 404 for `/favicon.ico`; webpack
  localforage warning. Sim frames before ~frame 91 (match `starting`
  window) ignore inputs.
