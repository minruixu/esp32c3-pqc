#!/bin/bash
# Build the SLH-DSA suite for this machine with the ESP32-C3 code paths
# enabled and the SHA accelerator replaced by a model (sha256_hw.h here),
# then run it. Usage: suites/slhdsa/hosttest/run.sh [filter]
set -e
HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$HERE/../../.." && pwd)"
SLH="$ROOT/third_party/slhdsa-c"
S="$ROOT/suites/slhdsa/pqc_suite"
OUT="$ROOT/build/host/slhdsa_mock"
mkdir -p "$OUT"
[ -f "$ROOT/build/host/.sdkroot" ] && [ -n "$(cat "$ROOT/build/host/.sdkroot")" ] && export SDKROOT="$(cat "$ROOT/build/host/.sdkroot")"
cc -O2 -g -w -DSLH_PQC_PRIMS -DPQC_SUITE_NAME='"slhdsa-mock"' \
   -I"$HERE" -I"$S" -I"$SLH" -I"$ROOT/components/prims/include" \
   -I"$ROOT/components/bench/include" -I"$ROOT/components/bench" \
   "$S/suite.c" "$S/slh_glue.c" "$S/slh_sha2_c3.c" "$S/slh_shake_fast.c" "$HERE/mock.c" \
   "$SLH/slh_dsa.c" "$SLH/slh_sha2.c" "$SLH/slh_shake.c" "$SLH/sha2_256.c" "$SLH/sha2_512.c" \
   "$SLH/sha3_api.c" "$SLH/sha3_f1600.c" \
   "$ROOT/components/prims/prims.c" "$ROOT/components/prims/fips202.c" "$ROOT/components/prims/sha2.c" \
   "$ROOT/components/prims/sm3.c" \
   "$ROOT/components/bench/bench_common.c" "$ROOT/host/bench_host.c" \
   -o "$OUT/suite_mock"
"$OUT/suite_mock" "$@"
