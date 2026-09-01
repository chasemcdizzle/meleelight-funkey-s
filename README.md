# meleelight-funkey-s

A **frame-exact C port** of [meleelight](https://github.com/schmooblidon/meleelight)
— Will Blackett's browser remake of Super Smash Bros. Melee — to the
[FunKey-S](https://www.funkey-project.com/), a 1.54-inch clamshell handheld.

The simulation is not approximated. Every frame, the port serialises its game
state and SHA-256s it, and that hash must equal the browser original's exactly.
Across 8 recorded matches × 3600 frames, 5 characters, 6 stages, on the device
itself. The interface is a rewrite, because a 240×240 screen and a d-pad are
not a 1200×750 canvas and a keyboard.

**Personal hobby project.** What is and is not published here is set out under
[Assets and provenance](#assets-and-provenance) — worth reading before you
download the release.

## What "frame-exact" means

The browser original is the oracle. A recorded input trace is replayed through
both, and the two must agree on a per-frame checksum of the game state — exact
string equality, never a tolerance.

| | |
|---|---|
| Goldens | 8 matches, 3600 frames each |
| Coverage | 5 characters, 6 VS stages, human + CPU |
| Agreement | per-frame SHA-256, full length, plus RNG draw counts |
| Verified on | desktop **and** the FunKey-S hardware |

Getting there meant doubles-only arithmetic, a vendored
[fdlibm](https://www.netlib.org/fdlibm/) for every transcendental, and
`-ffp-contract=off` on every translation unit — because the browser's `Math.sin`
and the ARM compiler's are not the same function, and one ulp of drift
compounds into a different match by frame 1671.

The device's own libc was a problem too: the SDK ships a musl built with
unsafe-FP optimisations, where `floor()` is the identity for non-integers and
`fmod(0,0)` is `1.0`. The port overrides those with exact implementations and
sweeps 432,319 values against a host reference on every run.

## What is deliberately different

The physics are the original's. The **interface is not**, and cannot be — this
screen is 4% of the area the original draws into. Every departure is recorded
with its reasoning in [`docs/MENU-SPEC.md`](docs/MENU-SPEC.md); there are 54 of
them. A few examples:

- the character select, stage select and menus are re-laid-out for 240×240
- the target-stage builder is rebound for a console with no `z` key, where the
  d-pad **is** the cursor
- closing the lid saves your match, your target run, or your half-drawn stage,
  and reopening it puts you back — the original has no lid
- custom target stages arrive as files on the SD card, not pasted share codes

## Verification

Thirty-six host checks in the standard sweep, eleven more that run on the
hardware over ADB, and others for narrower questions. They are not unit tests; each one
is an argument that a specific claim still holds, and most carry deliberate
perturbations ("teeth") that must make them fail — a check that cannot fail is
a check that proves nothing.

```
bash port/sim/check-sim.sh              # the simulation conforms, all 8 goldens
bash port/sim/device/verify_m3.sh       # device conformance + 60 fps + audio
bash port/foh/check-foh-flows.sh        # menu flows, frozen transition traces
```

## Building

**[`docs/DEVELOPMENT.md`](docs/DEVELOPMENT.md) is the from-scratch guide** — it
assumes you have never seen this project and do not own the handheld, and
covers what to install, how to build the oracle, how to get a shell on the
device, and the traps that cost real time. The short version:

```
bash oracle/build-upstream.sh           # clone + build the original at its pin
cd oracle/harness && npm install && cd ../..
node pipeline/run.js --out pipeline/build/dev
bash port/sim/check-sim.sh              # SIM CONFORMS = your build is honest
bash port/gfx/opk/install-play-opk.sh   # device only: build + install the OPK
```

Most of it works without a FunKey-S; the hardware is needed for the device
checks and for playing it.

[`AGENTS.md`](AGENTS.md) is the working contract and the fastest way in
(`CLAUDE.md` imports it); [`PLAN.md`](PLAN.md) is the original strategy
document.

## Credit

**[meleelight](https://github.com/schmooblidon/meleelight) is Will Blackett's**
(`schmooblidon`), MIT licensed, and this port would not exist without it. The
physics, the frame data and the feel are entirely that project's work; what is
here is a translation of it to a different machine, and a great deal of
machinery to prove the translation is honest.

Upstream's licence is carried verbatim as
[`LICENSE-meleelight`](LICENSE-meleelight). Third-party code is listed in
[`NOTICES`](NOTICES) — fdlibm (Sun), Ryu (Ulf Adams), QuickJS (Fabrice Bellard).

## Assets and provenance

**In this repository:** no game assets. No sprite, sound or music file is
committed; the converted audio is gitignored build output produced on the
machine that runs the pipeline. The only images in the tree are an application
icon and screenshots of the author's own device.

**In the release:** the data pack (`mlfk-data.tar.gz`) **does** contain
converted game audio — 8 music tracks and the sound-effect pack — because the
application cannot run without them, and regenerating them needs Docker, a
built copy of upstream and ffmpeg. They are converted from
[meleelight](https://github.com/schmooblidon/meleelight)'s own published
`dist/` directory, which ships the same material.

That is a deliberate decision, not an oversight, and the reasoning — including
the part that argues against it — is in [`docs/LICENSING.md`](docs/LICENSING.md).
Upstream publishing this material does not make it upstream's to license, and
does not make it ours.

Characters and concepts are © Nintendo / HAL Laboratory, as upstream's licence
states. Non-commercial hobby project. If you hold the rights and want this
gone, open an issue and it goes.
