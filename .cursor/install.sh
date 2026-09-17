#!/usr/bin/env bash
# Cloud Agent bootstrap for X Coin (XFER).
# Installs the C++/Qt build toolchain, then configures and compiles the
# xcoind daemon, xcoin-cli, and the xcoin-qt GUI wallet. Safe to re-run:
# apt installs are no-ops when satisfied and make builds incrementally.
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$REPO_ROOT"

export DEBIAN_FRONTEND=noninteractive
sudo apt-get update -qq
sudo apt-get install -y --no-install-recommends \
  build-essential libtool autotools-dev automake pkg-config bsdmainutils \
  python3 ccache \
  libssl-dev libevent-dev libboost-all-dev libdb-dev libdb++-dev \
  libminiupnpc-dev libzmq3-dev \
  qtbase5-dev qttools5-dev qttools5-dev-tools libqt5svg5-dev \
  libprotobuf-dev protobuf-compiler libqrencode-dev \
  xvfb

# Generate the autotools build scripts (idempotent).
./autogen.sh

# Configure once. A system Berkeley DB is used via --with-incompatible-bdb,
# which is fine for development/regtest (see INSTALL.md).
if [ ! -f config.status ]; then
  ./configure \
    --with-gui=qt5 \
    --with-incompatible-bdb \
    --disable-bench \
    --disable-tests \
    --enable-zmq
fi

# Build the daemon, CLI, and Qt wallet. ccache keeps rebuilds fast.
make -j"$(nproc)"

echo "X Coin build complete: src/xcoind, src/xcoin-cli, src/qt/xcoin-qt"
