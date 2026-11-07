#!/bin/bash
# Firmware C++ tests (Linux/WSL, g++ with multilib), in 32-bit like the ESP8266, with ASan/UBSan:
#  - real upload, host and password handlers (security_test.cjs) and focus history (stats_test.cjs);
#  - GIF and JPEG: valid samples must decode and fuzzing must not find a memory error.
# Usage: sim/tests.sh [fuzzing cases per format, default 5000]
# The .cpp files of the first two tests are generated with node; without node (WSL), pass the already generated folder in GEN.
set -euo pipefail
cd "$(dirname "$0")"
N="${1:-5000}"
T=$(mktemp -d /tmp/smalltv-testes.XXXXXX)
G="${GEN:-$T}"
if [ -z "${GEN:-}" ]; then
  node security_test.cjs "$G/sec.cpp"
  node stats_test.cjs "$G/stats.cpp"
fi
export ASAN_OPTIONS=detect_leaks=0 UBSAN_OPTIONS=halt_on_error=1
for t in sec stats; do
  g++ -std=c++17 -fsanitize=address,undefined -g -w "$G/$t.cpp" -o "$T/$t"
  "$T/$t"
done
# full output to a file; an error in any of them stops everything (set -e) and shows the end of the log
for f in gif jpeg; do
  if ! bash "test-$f.sh" "$f-seeds" "$N" > "$T/$f.log" 2>&1; then
    tail -20 "$T/$f.log"
    echo "tests: FAILED in $f (full log in $T/$f.log)"
    exit 1
  fi
done
echo "tests: ok (handlers, history, $N GIF and JPEG fuzzing cases)"
