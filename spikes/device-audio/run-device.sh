#!/bin/sh
# Run the full audio-spike measurement matrix on an ADB-attached FunKey-S.
# Reproduces logs/01..10. Assumes audiotest was built via ./build.sh.
# Hygiene: only touches /tmp on device + the frontend park marker; restores both.
set -eu
DEV="${FUNKEY_ADB_ID:-12c00003237f5528}"
DIR="$(cd "$(dirname "$0")" && pwd)"
LOG="$DIR/logs"
mkdir -p "$LOG"
A() { adb -s "$DEV" shell "sh -lc \"$*\""; }   # login shell => SDL env from /etc/profile

adb -s "$DEV" push "$DIR/audiotest" /tmp/audiotest
A 'chmod +x /tmp/audiotest'

# Park the frontend (single core: gmenu2x must not compete / hold audio)
A 'touch /mnt/disable_frontend; pkill gmenu2x || true'

A '/tmp/audiotest probe 2>&1'                                    | tee "$LOG/01-probe.log"
A '/tmp/audiotest load 44100 256 60 80 2>&1'                     | tee "$LOG/02-load-44100-256.log"
A '/tmp/audiotest load 44100 512 60 80 2>&1'                     | tee "$LOG/03-load-44100-512.log"
A '/tmp/audiotest load 44100 1024 60 80 2>&1'                    | tee "$LOG/04-load-44100-1024.log"
A '/tmp/audiotest load 22050 256 60 80 2>&1'                     | tee "$LOG/05-load-22050-256.log"
A '/tmp/audiotest mix 44100 512 30 8 1 0 2>&1'                   | tee "$LOG/06-mix-8v-music-noload.log"
A '/tmp/audiotest mix 44100 512 60 8 1 80 2>&1'                  | tee "$LOG/07-mix-8v-music-load80.log"
# SD-read interaction (any big file on the SD; reads only). 08 was the
# page-cache-fooled run kept for the record; 09/10 use O_DIRECT.
SDFILE=/mnt/Applications/ssb64.opk
A "/tmp/audiotest sd 44100 512 30 $SDFILE 2>&1"                  | tee "$LOG/09-sd-direct-44100-512.log"
A "/tmp/audiotest sd 44100 512 30 $SDFILE 80 2>&1"               | tee "$LOG/10-sd-direct-load80-44100-512.log"

# Restore device
A 'rm -f /tmp/audiotest /mnt/disable_frontend'
A 'pgrep gmenu2x >/dev/null || (setsid gmenu2x </dev/null >/dev/null 2>&1 & sleep 2)' || true
echo "done; logs in $LOG"
