# port/sim/device/adbsh.sh — shared ADB helpers for the M3 device rig.
#
# MEASURED (iter 37): this device's adbd does NOT propagate exit codes —
# `adb shell false` returns host exit 0. Every device command therefore
# runs through dsh(), which appends an RC echo and parses it back out
# (fix_plan §M3 "ADB facts"). Old adbd also emits CRLF; dsh strips CRs.
#
# Sourced by the port/sim/device/ and port/gfx/ check scripts.
# Env: FUNKEY_ADB_ID overrides the device id (default: the known-good
# FunKey-S from CLAUDE.md §Commands "Device access").

DEV="${FUNKEY_ADB_ID:-12c00003237f5528}"

# require_device — hard-fail unless $DEV is attached and in `device` state.
require_device() {
  adb devices | grep -q "^${DEV}[[:space:]]*device" || {
    echo "FATAL: FunKey-S ${DEV} not on ADB (adb devices)" >&2
    return 1
  }
}

# dsh <command-string> — run on the device, echo the device's stdout,
# return the DEVICE command's exit code (not adb's).
dsh() {
  local out rc
  out="$(adb -s "$DEV" shell "$1; echo MLFK_RC=\$?" </dev/null)" || {
    echo "FATAL: adb shell transport failure" >&2
    return 70
  }
  out="${out//$'\r'/}"
  rc="$(printf '%s\n' "$out" | sed -n 's/^MLFK_RC=//p' | tail -1)"
  printf '%s\n' "$out" | grep -v '^MLFK_RC=' || true
  if [ -z "$rc" ]; then
    echo "FATAL: no RC marker came back from the device" >&2
    return 71
  fi
  return "$rc"
}
