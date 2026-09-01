# Development setup, from nothing

This assumes you have never seen this project, do not own a FunKey-S, and do
not know what an OPK is. **You can get quite far without the handheld** — the
simulation is verified on your desktop; the device is only needed for the
device checks and for actually playing it.

## 1. What you need

| | why | note |
|---|---|---|
| **Docker** | the FunKey SDK and a pinned Node 8 both run in containers | the SDK image is amd64; on Apple Silicon it runs under emulation, slowly but correctly |
| **Node 18+** | the pipeline, the oracle harness and most check tooling | |
| **A C compiler** | host builds of the sim and the checks | clang or gcc; the host build is not the device build |
| **Google Chrome** | the ORACLE is the real game in a real browser | Playwright drives your installed Chrome; it does not download one |
| **`ffmpeg`** | audio conversion | version-pinned — the pipeline refuses a different build rather than drifting |
| **`adb`** | talking to the handheld | only for device work |

Roughly 8 GB of disk once the upstream clone, the SDK image and the generated
data are all present.

## 2. Build the oracle

Everything here is verified against the original game, so you need it first.
This clones upstream at its pinned commit, applies the harness patch and builds
it inside the pinned toolchain:

```
bash oracle/build-upstream.sh
```

It lands outside the tree, in `~/.cache/meleelight-funkey-s/upstream` by
default (`$MELEELIGHT_CLONE` overrides). Deliberately outside: upstream is not
vendored here and must never be committed.

Then install the harness that drives it:

```
cd oracle/harness && npm install && cd ../..
```

## 3. Generate the data plane

None of the game's numbers are typed by hand. They are extracted by *executing*
upstream's own JavaScript and serialising what it produces:

```
node pipeline/run.js --out pipeline/build/dev
```

This writes animation binaries, the physics/character tables as generated C,
stage geometry, and converted audio. The audio is **Nintendo-derived, gitignored
and must never be committed or distributed** — see the licence note in the
README.

## 4. Prove it works, on your desktop

```
bash port/sim/check-sim.sh
```

`SIM CONFORMS` means the C simulation reproduced the browser's per-frame state
checksums exactly, for all eight recorded matches. If that passes, your build
is honest and you can change things with a net under you.

Most other checks follow the same shape — run one, read its last line. They
print a verdict, not a test count.

**Run checks one at a time.** Several regenerate `pipeline/build/sim-tables`,
so two at once race and produce a confusing "file not found" failure that looks
like a real regression.

## 5. The handheld (optional)

### Getting a shell on it

The FunKey-S runs Linux and will start `adbd` at boot if it finds a marker
file. With the SD card mounted on your machine:

```
touch /path/to/sdcard/adb          # marker at the card root
```

Boot it, plug in USB, and `adb devices` should list it. From then on:

```
source port/sim/device/adbsh.sh    # gives you `dsh`, which checks exit codes
dsh 'uname -a'
```

That wrapper exists because **this device's `adbd` drops exit codes** — a
failing command looks like a passing one. Never trust a bare `adb shell` result
in a script.

### Running the game on it

```
bash port/gfx/opk/install-play-opk.sh
```

This cross-compiles for armv7 in the SDK container, packages an OPK, installs
it, and verifies the installed hash matches what it built. Restart the frontend
(or reboot) to see the new entry.

An **OPK** is the FunKey's application format: a squashfs image with a
`.desktop` file inside. Two gotchas, both learned painfully:

- package it **only** with the SDK container's `mksquashfs` 4.4 — newer
  versions produce an image the kernel silently fails to mount
- the `.desktop` file needs a trailing empty line, and `Exec=` must point at a
  launcher script rather than the binary

### Poking at it by hand

The OPK mounts read-only. Write to `/tmp` (RAM, wiped at power-off) or `/mnt`
(the SD card). To stop the menu frontend fighting you for the framebuffer:

```
dsh 'touch /mnt/disable_frontend; pkill gmenu2x'
```

Undo it by removing that file. Launch anything graphical through a **login
shell** (`sh -lc '…'`) or SDL will fail to initialise.

## 6. Cross-compiling by hand

```
docker run --rm -v "$PWD":/work -w /work jondbell/funkey-s-sdk bash -lc \
  'export PATH=/opt/FunKey-sdk-2.3.0/bin:$PATH; arm-funkey-linux-musleabihf-gcc \
   -O2 -ffp-contract=off <sources> -o build-arm/<out> \
   $(/opt/FunKey-sdk-2.3.0/arm-funkey-linux-musleabihf/sysroot/usr/bin/sdl-config \
     --cflags --libs) -lm'
```

`-ffp-contract=off` on every simulation file is **not optional** — it is what
stops the compiler fusing multiply-add pairs and changing results by one ulp,
which compounds into a different match a thousand frames later.

Note also that the SDK's `libm` was built with unsafe floating-point
optimisations (`floor()` is the identity for non-integers; `fmod(0,0)` is
`1.0`). The port overrides those with exact implementations. If you add a maths
call, sweep it against a host reference before trusting it.

## 7. Where to read next

- **[`AGENTS.md`](../AGENTS.md)** — the working contract. Read the HARD RULES
  first; they explain why the checks are shaped the way they are.
- **[`PLAN.md`](../PLAN.md)** — the original strategy and the milestone ladder.
- **[`docs/MENU-SPEC.md`](MENU-SPEC.md)** — every deliberate departure from the
  original, with its reasoning.
- **[`docs/AGENT-LOG.md`](AGENT-LOG.md)** — the running decision trail. Long,
  and the most useful thing here when something surprises you.

## 8. If you are an agent

`CLAUDE.md` is one line: it imports `AGENTS.md`. Read that file completely
before editing anything. The rules that matter most in practice:

- **behaviour over compiles** — "it builds" is not evidence
- **no stubs**; a deferral goes in the log, never into the code as a
  placeholder that returns a plausible value
- **never weaken a check to make it pass.** If a check is wrong, say so and
  fix the check deliberately, with its reasoning written down
- **prove every new assertion can fail** — delete the fix, watch it fail,
  restore. A check that passes without its fix is worse than no check
