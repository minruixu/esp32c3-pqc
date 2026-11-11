#!/bin/bash
# Build the LMS or the XMSS suite for this machine with the ESP32-C3 code
# paths enabled and the SHA accelerator replaced by a model (sha256_hw.h
# here), then run it. Usage: suites/lms/extra/hwmock/run.sh lms|xmss [filter]
set -e
HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$HERE/../../../.." && pwd)"
WHAT="${1:-lms}"
shift || true
OUT="$ROOT/build/host/${WHAT}_hwmock"
mkdir -p "$OUT"
[ -f "$ROOT/build/host/.sdkroot" ] && [ -n "$(cat "$ROOT/build/host/.sdkroot")" ] && export SDKROOT="$(cat "$ROOT/build/host/.sdkroot")"
COMMON=("$HERE/mock.c"
        "$ROOT/components/prims/prims.c" "$ROOT/components/prims/fips202.c"
        "$ROOT/components/prims/sha2.c" "$ROOT/components/prims/sm3.c"
        "$ROOT/components/bench/bench_common.c" "$ROOT/host/bench_host.c")
INC=(-I"$HERE" -I"$ROOT/components/prims/include" -I"$ROOT/components/bench/include" -I"$ROOT/components/bench")
if [ "$WHAT" = lms ]; then
    S="$ROOT/suites/lms/pqc_suite"
    cc -O2 -g -w -DPQC_SUITE_NAME='"lms-hwmock"' "${INC[@]}" -I"$S" \
       "$S/suite.c" "$S/lms.c" "$S/lms_hw.c" "${COMMON[@]}" -o "$OUT/suite_mock"
else
    S="$ROOT/suites/xmss/pqc_suite"
    X="$ROOT/third_party/xmss-reference"
    cc -O2 -g -w -DPQC_SUITE_NAME='"xmss-hwmock"' "${INC[@]}" -I"$S" -I"$S/shim" -I"$X" \
       "$S/suite.c" "$S/hash_ref.c" "$S/hash_opt.c" "$S/hash_hw.c" "$S/core_fast.c" "$S/core_slow.c" \
       "$X/params.c" "$X/hash_address.c" "$X/wots.c" "$X/xmss_commons.c" "$X/utils.c" \
       "${COMMON[@]}" -o "$OUT/suite_mock"
fi
"$OUT/suite_mock" "$@"
