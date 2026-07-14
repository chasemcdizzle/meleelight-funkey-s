#!/usr/bin/env bash
# Build a funkey-patched meleelight clone with the proven docker node:8 recipe
# (spikes/determinism README). Usage: ./build.sh /path/to/meleelight-clone
set -euo pipefail

CLONE="${1:?usage: build.sh /path/to/meleelight-clone}"
cd "$CLONE"

# one-time: apply the mapping layer if not already present
if [ ! -f src/input/funkey/funkeyMapping.js ]; then
  echo "==> applying funkey-mapping.patch"
  git apply "$(dirname "$0")/funkey-mapping.patch"
fi

DOCKER="docker run --rm --platform linux/amd64 -v $PWD:/app -w /app node:8"

if [ ! -d node_modules ]; then
  echo "==> npm install (docker node:8; postinstall/electron/deepstream already dropped by patch)"
  $DOCKER bash -c "npm install --ignore-scripts"
fi

if [ ! -f dist/js/animations.js ]; then
  echo "==> building animations bundle (one-time, ~30MB)"
  $DOCKER bash -c "npm run animations"
fi

echo "==> building main bundle"
$DOCKER bash -c "npm run build"
echo "==> done: $CLONE/dist/js/main.js"
