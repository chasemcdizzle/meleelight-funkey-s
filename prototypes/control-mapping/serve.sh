#!/usr/bin/env bash
# One command: build if needed, then serve the clone on :8765.
# Usage: ./serve.sh /path/to/meleelight-clone
# Open:  http://localhost:8765/dist/meleelight.html
set -euo pipefail

CLONE="${1:?usage: serve.sh /path/to/meleelight-clone}"

if [ ! -f "$CLONE/dist/js/main.js" ] || [ ! -f "$CLONE/src/input/funkey/funkeyMapping.js" ]; then
  "$(dirname "$0")/build.sh" "$CLONE"
fi

echo "==> serving $CLONE on http://localhost:8765/dist/meleelight.html"
cd "$CLONE" && exec python3 -m http.server 8765
