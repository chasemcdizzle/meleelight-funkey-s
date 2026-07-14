#!/usr/bin/env bash
# oracle/qjs/build.sh — build the fdlibm-patched QuickJS oracle runtime
# (M0 task 6; fix_plan §M0.6).
#
# Produces:
#   oracle/qjs/build-host/qjs-oracle   host build (the done-check target;
#                                      the M0 gate replays goldens with it)
#   oracle/qjs/build-arm/qjs-oracle    armv7 FunKey-S cross-build,
#                                      COMPILE-ONLY this milestone (device
#                                      rung is M3). Skippable for fast
#                                      host iteration: QJS_SKIP_ARM=1.
#
# QuickJS pin (bellard/quickjs):
#   commit 42d08be5f28abfdf881110bba3713f6a256d8d97 (2026-06-13,
#   VERSION 2026-06-04 — the release the device-feasibility spike ran;
#   every compiled source byte-matches the spike's /tmp/qjs-src tree).
#   Every source consumed is sha256-verified below; a checkout that
#   drifts fails the build.
#
# The Math table repoint (sin/cos/tan/atan/atan2/pow -> port/fdlibm/) is
# done by qjs_oracle.c at runtime startup; this script's job is to link
# the C fdlibm in with -ffp-contract=off (CLAUDE.md hard rule 5).
set -euo pipefail
cd "$(dirname "$0")"

QJS_PIN=42d08be5f28abfdf881110bba3713f6a256d8d97
QJS_SRC="${QJS_SRC:-$HOME/.cache/meleelight-funkey-s/quickjs}"
QJS_REPO=https://github.com/bellard/quickjs

# --- fetch + pin ------------------------------------------------------------
if [ ! -d "$QJS_SRC/.git" ]; then
  echo "build.sh: cloning $QJS_REPO -> $QJS_SRC"
  git clone "$QJS_REPO" "$QJS_SRC"
fi
if ! git -C "$QJS_SRC" cat-file -e "$QJS_PIN^{commit}" 2>/dev/null; then
  git -C "$QJS_SRC" fetch origin
fi
git -C "$QJS_SRC" checkout -q "$QJS_PIN"

# --- verify every consumed source against the pin manifest ------------------
# (sha256 of each file at commit 42d08be5; matches the feasibility spike's
# tree file-for-file — see docs/AGENT-LOG.md iter 7.)
verify() {
  local want="$1" f="$2" got
  got=$(shasum -a 256 "$QJS_SRC/$f" | cut -d' ' -f1)
  if [ "$got" != "$want" ]; then
    echo "build.sh: PIN VIOLATION: $f sha256 $got != $want" >&2
    exit 1
  fi
}
verify a68622cecb806f39bf24738c376a0a73032ea8913478cad702e0265c46f7999f quickjs.c
verify b73a403a59da30726257ddbdf5e399298941c1def997782ee0d4d33f796a80a2 cutils.c
verify fdfcf86167029ab2c39c35730d4ae6875b7bbeb9406a0fcc886de4e9b1f5e2ac libregexp.c
verify 26203ae888c0582e7d0e2113f13db0c9b39dc7b0b3836d68fa308c54f7a0898c libunicode.c
verify af5abd68fa9806d1a19bdd5f2daef00d5fd0990ae56311382dfed4700343f074 dtoa.c
verify 8c6c42619ab4edf1548b051c73bb2bee670003415b1de30d2910d71917859bce quickjs-libc.c
verify 2165f47772af9faee1798999a599fa9de850d1bf0259502dade1c25d4a588316 quickjs.h
verify cfb4a6ab9533461a8abf8083f7d9f9a84d6bb26c92a7dd718984a7a1ddd754eb quickjs-libc.h
verify d2da6d06a75b9e6c116c82b7a41df6bcc170c8b1779f374fa953ecf688eda647 cutils.h
verify f5c5ef1899f224dcc718422a22c789ef9d8b5b837b622818fcfaf681ae793446 libregexp.h
verify 2a98d646089f3a72f25480116297c5fdb5b28fd9f815447273951f4ade4a115e libregexp-opcode.h
verify ce310152bc80d7415dcb657e23abd9a40bf83e393c0d05d325dae384bb01d259 libunicode.h
verify cf782bc7a07549e976f606bd3cb8555858482b279574554dcb8d46412986006c libunicode-table.h
verify 462070f7678a894b9f3612422f280f4dce412eb360a5cc585a513f407fc23114 dtoa.h
verify 181c6b25d9d0e4853315d36a6e6fab3b2c3554271693d898a439384ba0f67c2b list.h
verify 342194359417664e652f54e7d682ee2ef91dd982e91f74decbde992a2c848236 quickjs-atom.h
verify a9c60a8c9366820733d9d2acbb805f48b2e9e2778df526e6bcb845fdb6b47834 quickjs-opcode.h
verify 234057c4079458cba9862d4bbb6f8a059ac2e4a194947673da0ad38ca16a37fd VERSION

CONFIG_VERSION=$(cat "$QJS_SRC/VERSION")
QJS_C="quickjs.c cutils.c libregexp.c libunicode.c dtoa.c quickjs-libc.c"

# --- host build (the done-check target) --------------------------------------
# fdlibm.c and the embedder get -ffp-contract=off (hard rule 5: every TU
# touching sim math). The QuickJS core TUs get it too — QuickJS itself
# evaluates the sim's +,-,*,/; keeping fused-multiply-add off everywhere
# removes a whole class of "compiler contracted my doubles" divergences.
echo "build.sh: host build (cc = ${CC:-cc})"
mkdir -p build-host
SRCS=""
for f in $QJS_C; do SRCS="$SRCS $QJS_SRC/$f"; done
${CC:-cc} -O2 -ffp-contract=off -D_GNU_SOURCE \
  -DCONFIG_VERSION="\"$CONFIG_VERSION\"" \
  -I"$QJS_SRC" -I../../port/fdlibm \
  $SRCS ../../port/fdlibm/fdlibm.c sha256.c qjs_oracle.c \
  -o build-host/qjs-oracle -lm -lpthread
echo "build.sh: built build-host/qjs-oracle"

# smoke: sha256 self-test runs at startup; a trivial eval proves the repoint
build-host/qjs-oracle /dev/stdin <<'EOF'
if (Math.pow(2, 3) !== 8) throw new Error("Math.pow broken after repoint");
globalThis.__replayExit = 0;
EOF
echo "build.sh: host smoke OK (sha256 self-test + repointed Math callable)"

# --- armv7 cross-build (COMPILE-ONLY; the device rung is M3) -----------------
if [ "${QJS_SKIP_ARM:-0}" = "1" ]; then
  echo "build.sh: QJS_SKIP_ARM=1 — skipping armv7 cross-build"
else
  echo "build.sh: armv7 cross-build (docker jondbell/funkey-s-sdk, static)"
  mkdir -p build-arm
  docker run --rm -v "$PWD/../..":/repo -v "$QJS_SRC":/qjs \
    -w /repo/oracle/qjs jondbell/funkey-s-sdk bash -lc '
    export PATH=/opt/FunKey-sdk-2.3.0/bin:$PATH
    arm-funkey-linux-musleabihf-gcc -O2 -ffp-contract=off -static \
      -D_GNU_SOURCE -DCONFIG_VERSION="\"'"$CONFIG_VERSION"'\"" \
      -I/qjs -I/repo/port/fdlibm \
      /qjs/quickjs.c /qjs/cutils.c /qjs/libregexp.c /qjs/libunicode.c \
      /qjs/dtoa.c /qjs/quickjs-libc.c \
      /repo/port/fdlibm/fdlibm.c sha256.c qjs_oracle.c \
      -o build-arm/qjs-oracle -lm -lpthread
    arm-funkey-linux-musleabihf-strip build-arm/qjs-oracle
    file build-arm/qjs-oracle || true
  '
  echo "build.sh: built build-arm/qjs-oracle (compile-only; not run — M3)"
fi

echo "QJS BUILD OK (pin $QJS_PIN, VERSION $CONFIG_VERSION)"
