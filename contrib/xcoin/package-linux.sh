#!/usr/bin/env bash
# Strip and pack Linux GUI + CLI binaries for private release 1.1.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
OUT="${OUT:-$ROOT/dist}"
VER="${VER:-1.1.0}"
NAME="xcoin-${VER}-linux-x86_64"
STAGE="$OUT/$NAME"

QT="$ROOT/src/qt/xcoin-qt"
DAEMON="$ROOT/src/xcoind"
CLI="$ROOT/src/xcoin-cli"
TX="$ROOT/src/xcoin-tx"

for f in "$QT" "$DAEMON" "$CLI"; do
  if [[ ! -x "$f" ]]; then
    echo "missing $f — build with --with-gui=qt5 first" >&2
    exit 1
  fi
done

rm -rf "$STAGE"
mkdir -p "$STAGE/bin" "$STAGE/docs"
install -m 0755 "$QT" "$STAGE/bin/xcoin-qt"
install -m 0755 "$DAEMON" "$STAGE/bin/xcoind"
install -m 0755 "$CLI" "$STAGE/bin/xcoin-cli"
if [[ -x "$TX" ]]; then
  install -m 0755 "$TX" "$STAGE/bin/xcoin-tx"
fi
strip -s "$STAGE/bin/"* || strip "$STAGE/bin/"*
cp "$ROOT/README.md" "$STAGE/docs/"
cp "$ROOT/docs/RELEASE-1.1.md" "$STAGE/docs/"
cp "$ROOT/docs/LOTTERY.md" "$STAGE/docs/"
cp "$ROOT/whitepaper/XCOIN.md" "$STAGE/docs/WHITEPAPER.md"
cat > "$STAGE/README.txt" <<EOF
X Coin (XFER) ${VER} — Linux x86_64 (private)
Nifty Raven (@NFTRVN on X)

bin/xcoin-qt   desktop GUI (required for 1.1)
bin/xcoind     node / lottery producer
bin/xcoin-cli  RPC

Home: Sign in with X, lottery, optional POOL (create/join/leave).
Provide my node IP is off by default.

Run:  ./bin/xcoin-qt
Regtest: ./bin/xcoin-qt -regtest

Build line:
  ./autogen.sh
  ./configure --with-gui=qt5 --disable-bench --disable-tests --with-incompatible-bdb
  make -j\$(nproc)
EOF

TAR="$OUT/${NAME}.tar.gz"
tar -C "$OUT" -czf "$TAR" "$NAME"
echo "wrote $TAR"
ls -lh "$TAR" "$STAGE/bin"
echo "$TAR"
