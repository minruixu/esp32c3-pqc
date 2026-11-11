#!/bin/sh
# Build and run the differential test of fpr_c3.c. Argument: millions of pairs.
set -e
here="$(cd "$(dirname "$0")" && pwd)"
lib="$here/../../../third_party/c-fn-dsa"
out="${TMPDIR:-/tmp}/test_fpr_host"
${CC:-cc} -O2 -ffp-contract=off -DFNDSA_NEON=0 -DFNDSA_SSE2=0 -DFNDSA_AVX2=0 -I"$lib" \
    -o "$out" "$here/test_fpr_host.c" "$lib/sign_fpr.c"
"$out" "${1:-20}"
