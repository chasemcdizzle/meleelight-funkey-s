#!/bin/sh
# Cross-compile rastbench (and optionally QuickJS) for the FunKey-S
# (armv7 Cortex-A7, musl hard-float) via the jondbell/funkey-s-sdk docker image.
# Run from this directory. Output: build-arm/rastbench
set -e
mkdir -p build-arm
docker run --rm -v "$PWD":/work -w /work jondbell/funkey-s-sdk bash -lc '
  export PATH=/opt/FunKey-sdk-2.3.0/bin:$PATH
  SDLCFG=/opt/FunKey-sdk-2.3.0/arm-funkey-linux-musleabihf/sysroot/usr/bin/sdl-config
  arm-funkey-linux-musleabihf-gcc -O2 -ffp-contract=off -Wall \
    rastbench.c -o build-arm/rastbench $($SDLCFG --cflags --libs) -lm
  arm-funkey-linux-musleabihf-strip build-arm/rastbench
  file build-arm/rastbench || true
'
echo "built build-arm/rastbench"
