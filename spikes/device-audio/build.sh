#!/bin/sh
# Cross-compile audiotest.c for the FunKey-S via the jondbell/funkey-s-sdk
# docker image. Run from anywhere; output lands next to the source.
set -eu
DIR="$(cd "$(dirname "$0")" && pwd)"
docker run --rm -v "$DIR":/work -w /work jondbell/funkey-s-sdk bash -lc '
  export PATH=/opt/FunKey-sdk-2.3.0/bin:$PATH
  SDLCFG=/opt/FunKey-sdk-2.3.0/arm-funkey-linux-musleabihf/sysroot/usr/bin/sdl-config
  arm-funkey-linux-musleabihf-gcc -O2 -Wall -o audiotest audiotest.c \
    $($SDLCFG --cflags) $($SDLCFG --libs) -lpthread -lm
  file audiotest || true
'
echo "built: $DIR/audiotest"
