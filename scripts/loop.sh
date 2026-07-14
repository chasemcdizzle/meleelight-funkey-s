#!/usr/bin/env bash
# External fresh-context autonomous loop driver (adapted from ssb64-funkey-s).
# Each iteration is a brand-new `claude -p` reading docs/LOOP.md, so context
# never overflows; all state lives on disk. Stops on a `LOOP STOP:` sentinel,
# a .loop/STOP file, or MAX_ITERS.
#
# Run from the repo root:   MAX_ITERS=300 bash scripts/loop.sh
#
# Permission mode: defaults to interactive ("default" — Claude Code prompts
# for approvals). For unattended overnight runs the OWNER may opt in
# explicitly, e.g.  PERM_MODE=bypassPermissions MAX_ITERS=300 bash scripts/loop.sh
# — do that only with the git-guardrails hook installed as the safety net.
set -uo pipefail
cd "$(dirname "$0")/.."

MAX_ITERS="${MAX_ITERS:-300}"
PERM_MODE="${PERM_MODE:-default}"
mkdir -p .loop

# Refuse to run anywhere but the dedicated branch.
branch="$(git rev-parse --abbrev-ref HEAD)"
if [ "$branch" != "agent/auto" ]; then
  echo "Refusing to run on '$branch'; expected agent/auto. (git switch -c agent/auto)"; exit 1
fi

for ((i=1; i<=MAX_ITERS; i++)); do
  if [ -f .loop/STOP ]; then echo "[$i] .loop/STOP present — exiting."; break; fi
  if tail -n 5 docs/AGENT-LOG.md 2>/dev/null | grep -q '^LOOP STOP:'; then
    echo "[$i] LOOP STOP sentinel found — loop halted for the owner."; break
  fi
  echo "=== iteration $i / $MAX_ITERS  ($(git rev-parse --short HEAD)) ==="
  claude -p "$(cat docs/LOOP.md)" --permission-mode "$PERM_MODE" < /dev/null || \
    echo "[$i] claude exited non-zero; continuing to next iteration."
done

echo "Loop finished. Latest log:"; tail -n 8 docs/AGENT-LOG.md 2>/dev/null || true
