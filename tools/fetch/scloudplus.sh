#!/bin/bash
# Fetch the SCloud+ reference implementation. Its repository carries no
# license file, so the code is kept in downloads/ and never committed.
set -e
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
COMMIT=${SCLOUDPLUS_COMMIT:-}
DEST="$ROOT/downloads/scloudplus"
if [ ! -d "$DEST/.git" ]; then
    git clone -q https://github.com/scloudplus/scloudplus.git "$DEST"
fi
if [ -n "$COMMIT" ]; then
    git -C "$DEST" fetch -q origin
    git -C "$DEST" checkout -q "$COMMIT"
fi
echo "SCloud+ at $(git -C "$DEST" rev-parse --short HEAD) in $DEST"
python3 "$ROOT/suites/scloudplus/make_port.py"
