#!/usr/bin/env bash
# Secret-free allowlist refresh: fetch a published text file, then load it.
# Usage: refresh-x-allowlist.sh <url> [dest]
# Default dest: $HOME/.xcoin/verified-x-accounts.txt
# After this, run: xcoin-cli loadxverified
set -euo pipefail
if [[ $# -lt 1 ]]; then
  echo "usage: $0 <url> [dest]" >&2
  exit 1
fi
URL="$1"
DEST="${2:-$HOME/.xcoin/verified-x-accounts.txt}"
mkdir -p "$(dirname "$DEST")"
curl -fsSL "$URL" -o "$DEST"
echo "wrote $DEST — run: xcoin-cli loadxverified"
