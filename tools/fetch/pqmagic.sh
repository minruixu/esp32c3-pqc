#!/bin/bash
# Fetch PQMagic (Apache-2.0) at the commit that third_party/pqmagic was taken
# from. Only needed to refresh the vendored copy or to compare with upstream.
set -e
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
COMMIT=42f5b6e6fe7e4951ffc9437ecd77c9a5af93ff08
DEST="$ROOT/downloads/PQMagic"
if [ ! -d "$DEST/.git" ]; then
    git clone -q https://github.com/pqcrypto-cn/PQMagic.git "$DEST"
fi
git -C "$DEST" fetch -q origin "$COMMIT" 2>/dev/null || git -C "$DEST" fetch -q origin
git -C "$DEST" checkout -q "$COMMIT"
echo "PQMagic at $(git -C "$DEST" rev-parse --short HEAD) in $DEST"
