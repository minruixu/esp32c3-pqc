#!/bin/bash
# Host-only cross-check of the hash port: build a PQMagic suite twice, once
# with the project's SM3/Keccak primitives (the normal build) and once with
# PQMagic's own hash/sm3 and hash/keccak code, and compare every fingerprint.
# Usage: suites/pqmagic_common/check_upstream_hash.sh <suite> [filter]
# Needs cmake and ninja on PATH (for example after sourcing ESP-IDF's export.sh).
set -e
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
SUITE="$1"; FILTER="$2"
if [ -z "$SDKROOT" ] && [ -f "$ROOT/build/host/.sdkroot" ]; then
    SDK="$(cat "$ROOT/build/host/.sdkroot")"; [ -n "$SDK" ] && export SDKROOT="$SDK"
fi
for kind in prims upstream; do
    B="$ROOT/build/host/${SUITE}_hashcheck_$kind"
    FLAG=OFF; [ $kind = upstream ] && FLAG=ON
    cmake -S "$ROOT/host" -B "$B" -G Ninja -D PQC_SUITE="$SUITE" -D PQM_UPSTREAM_HASH=$FLAG >/dev/null
    cmake --build "$B" >/dev/null
    "$B/suite_host" $FILTER | grep '^@DIGEST' > "$B/digests.txt" || true
done
A="$ROOT/build/host/${SUITE}_hashcheck_prims/digests.txt"
U="$ROOT/build/host/${SUITE}_hashcheck_upstream/digests.txt"
N=$(wc -l < "$A" | tr -d ' ')
if [ "$N" -gt 0 ] && cmp -s "$A" "$U"; then
    echo "[$SUITE: $N fingerprints identical with the upstream hash code]"
else
    echo "[$SUITE: MISMATCH between the project's primitives and the upstream hash code]"
    diff "$A" "$U" | head -20
    exit 1
fi
