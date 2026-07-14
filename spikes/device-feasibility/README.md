# spikes/device-feasibility — on-device feasibility spike (ticket #8)

Measured on a REAL FunKey-S (adb id `12c00003237f5528`) on 2026-07-13.
Numbers + verdicts: `docs/research/device-feasibility-spike.md`.
Raw logs + evidence frames: `results/`.

## Contents

| File | What |
|---|---|
| `extract_anim.js` | Parses REAL meleelight animation data (`src/animations/<char>/<STATE>.js` Int16Array bezier paths) into `data/anim.bin` |
| `data/anim.bin` | fox + marth WAIT/DASH/ATTACKAIRN — 356 paths, 124,576 int16 coords (252 KB) |
| `rastbench.c` | SDL 1.2 240x240 16bpp software vector rasterizer benchmark (Experiment 1): bezier flattening -> nonzero-winding scanline fill (AET), AA on/off, tolerance + zoom variants, baseline clear+flip mode, PPM dump |
| `qjsmin.c` | Minimal QuickJS embedder (no qjsc/repl -> single-command cross-compile); adds `hrtime()` (µs) global + prints VmRSS/VmHWM at exit |
| `simbench.js` | meleelight-shaped sim workload (Experiment 2): REAL Vec2D/linAlg/solveQuadraticEquation functions + anatomy-§3-shaped driver (4x interpretInputs+update, hitDetect x4, executeHits, per-tick deepCopy). Runs under node AND qjsmin |
| `build.sh` | Cross-compiles `rastbench` via docker `jondbell/funkey-s-sdk` |
| `results/` | Raw device logs + rendered-frame evidence (host + device dumps) |

## Repro

```sh
# 0) data (needs a meleelight clone; schmooblidon/meleelight@master)
node extract_anim.js /path/to/meleelight data/anim.bin

# 1) cross-compile rastbench
./build.sh    # -> build-arm/rastbench

# 2) cross-compile the QuickJS runner (bellard/quickjs, tested @ 2026-06-04)
git clone --depth 1 https://github.com/bellard/quickjs /tmp/qjs-src
docker run --rm -v "$PWD":/work -v /tmp/qjs-src:/qjs -w /work jondbell/funkey-s-sdk bash -lc '
  export PATH=/opt/FunKey-sdk-2.3.0/bin:$PATH
  arm-funkey-linux-musleabihf-gcc -O2 -static -D_GNU_SOURCE -DCONFIG_VERSION="\"2026-06-04\"" \
    -I/qjs /qjs/quickjs.c /qjs/cutils.c /qjs/libregexp.c /qjs/libunicode.c /qjs/dtoa.c \
    /qjs/quickjs-libc.c qjsmin.c -o build-arm/qjsmin -lm -lpthread'

# 3) device (FunKey with the adb marker file; see funkey-envelope doc)
adb -s 12c00003237f5528 push build-arm/rastbench build-arm/qjsmin data/anim.bin simbench.js /tmp/
adb -s 12c00003237f5528 shell "touch /mnt/disable_frontend; pkill gmenu2x"
adb -s 12c00003237f5528 shell "sh -lc '/tmp/rastbench /tmp/anim.bin -mode baseline -frames 1200'"
adb -s 12c00003237f5528 shell "sh -lc '/tmp/rastbench /tmp/anim.bin -mode scene -aa 1 -tol 0.25 -zoom 2 -frames 1200'"
adb -s 12c00003237f5528 shell "sh -lc '/tmp/qjsmin /tmp/simbench.js 600 2>&1'"
# cleanup
adb -s 12c00003237f5528 shell "rm -f /tmp/rastbench /tmp/qjsmin /tmp/anim.bin /tmp/simbench.js; rm /mnt/disable_frontend"
```

Host sanity build (headless, dumps PPM): `cc -O2 -DHEADLESS rastbench.c -o rastbench -lm`.
Launch gotchas (login shell for SDL_NOMOUSE, setsid pattern for detached runs,
16bpp verify): docs/research/funkey-envelope.md. These runs are synchronous
`adb shell` commands, so the setsid dance isn't needed.
