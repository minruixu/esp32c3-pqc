#!/bin/bash
# Fetch the LAC reference code (32-bit C version of LAC-v3b). The repository
# carries no license, so the code is kept in downloads/ and never committed.
set -e
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
COMMIT=6cfd85a71c786c76f29447b1b3bc0902dd7399df
DEST="$ROOT/downloads/lac-intel32"
if [ ! -d "$DEST/.git" ]; then
    git clone -q https://github.com/pqc-lac/lac-intel32.git "$DEST"
fi
git -C "$DEST" fetch -q origin "$COMMIT" 2>/dev/null || git -C "$DEST" fetch -q origin
git -C "$DEST" checkout -q "$COMMIT"
echo "LAC at $(git -C "$DEST" rev-parse --short HEAD) in $DEST"
python3 "$ROOT/suites/lac/make_port.py"
