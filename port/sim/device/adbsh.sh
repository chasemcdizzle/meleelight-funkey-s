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
# Iter 52 (whitelist grammar, PROCESS §3): measured `adb devices` grammar
# is `<serial><TAB>device` for a healthy device — anchored FULL-line
# match (the old `[[:space:]]*device` prefix match would also accept a
# decorated or state-suffixed line that merely resembles attached).
require_device() {
  adb devices | grep -q "^${DEV}$(printf '\t')device\$" || {
    echo "FATAL: FunKey-S ${DEV} not on ADB in 'device' state (adb devices)" >&2
    return 1
  }
}

# dsh <command-string> — run on the device, echo the device's stdout,
# return the DEVICE command's exit code (not adb's).
#
# The RC marker is emitted with a GUARANTEED leading newline (iter 39,
# review H1): a device command whose output ends without a trailing
# newline can never concatenate into the genuine marker line. The
# marker carries a per-invocation NONCE (iter 40, review H1 round 2):
# the parser accepts ONLY the exact-token marker `MLFK_RC_<token>=`,
# so a forged or stale `MLFK_RC*` line in command output — including
# one printed by a device-side EXIT trap AFTER the genuine marker —
# is ordinary output, never an rc. Last exact-token marker wins. The
# parsed rc must be an integer in 0-255 (bash `return` wraps mod 256 —
# anything unparseable/out-of-range is a 71-class FATAL, never a
# silent wrap).
dsh() {
  local out rc tok
  tok="$RANDOM$RANDOM$$"
  out="$(adb -s "$DEV" shell "$1; printf '\nMLFK_RC_${tok}=%s\n' \$?" </dev/null)" || {
    echo "FATAL: adb shell transport failure" >&2
    return 70
  }
  out="${out//$'\r'/}"
  rc="$(printf '%s\n' "$out" | sed -n "s/^MLFK_RC_${tok}=//p" | tail -1)"
  printf '%s\n' "$out" | grep -v "^MLFK_RC_${tok}=" || true
  if [ -z "$rc" ]; then
    echo "FATAL: no RC marker came back from the device" >&2
    return 71
  fi
  case "$rc" in
    *[!0-9]*)
      echo "FATAL: malformed RC marker from device: '$rc'" >&2
      return 71
      ;;
  esac
  if [ "$rc" -gt 255 ]; then
    echo "FATAL: RC marker out of range (0-255): $rc" >&2
    return 71
  fi
  return "$rc"
}
