#!/bin/bash
# Host-only memory-safety sweep: build a suite with AddressSanitizer and
# UndefinedBehaviorSanitizer and run it. Reports the first few findings.
# Usage: suites/pqmagic_common/sanitize_host.sh <suite> [filter]
# Needs cmake and ninja on PATH (for example after sourcing ESP-IDF's export.sh).
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
SUITE="$1"; FILTER="$2"
if [ -z "$SDKROOT" ] && [ -f "$ROOT/build/host/.sdkroot" ]; then
    SDK="$(cat "$ROOT/build/host/.sdkroot")"; [ -n "$SDK" ] && export SDKROOT="$SDK"
fi
B="$ROOT/build/host/${SUITE}_sanitize"
cmake -S "$ROOT/host" -B "$B" -G Ninja -D PQC_SUITE="$SUITE" \
    -D CMAKE_C_FLAGS="-fsanitize=address,undefined -fno-omit-frame-pointer" >/dev/null || exit 2
cmake --build "$B" >/dev/null 2>"$B/build.err" || { head -5 "$B/build.err"; exit 2; }
(ulimit -s 65520; "$B/suite_host" $FILTER > "$B/run.out" 2> "$B/run.err")
N=$(grep -c "runtime error\|ERROR: AddressSanitizer" "$B/run.err")
DONE=$(grep -c "^@DONE" "$B/run.out")
echo "[$SUITE sanitize: finished=$DONE findings=$N]"
grep "runtime error\|ERROR: AddressSanitizer\|^SUMMARY" "$B/run.err" | sed "s|$ROOT/||" | sort | uniq -c | sort -rn | head -8
