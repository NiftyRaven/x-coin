#!/usr/bin/env bash
# Headless Qt sanity: pre-create a wallet with xcoind (avoids the first-run
# mnemonic modal), then launch xcoin-qt and check lottery RPC.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
QT="${XCOINQT:-}"
if [[ -z "$QT" ]]; then
  if [[ -x "$ROOT/src/qt/xcoin-qt" ]]; then
    QT="$ROOT/src/qt/xcoin-qt"
  elif [[ -x "$ROOT/src/xcoin-qt" ]]; then
    QT="$ROOT/src/xcoin-qt"
  else
    QT="$ROOT/src/qt/xcoin-qt"
  fi
fi
DAEMON="${XCOIND:-$ROOT/src/xcoind}"
CLI="${XCLI:-$ROOT/src/xcoin-cli}"
DATADIR="${DATADIR:-$ROOT/smoke-gui-datadir}"
RPCPORT="${RPCPORT:-28482}"
P2PPORT="${P2PPORT:-28483}"
DISPLAY_NUM="${DISPLAY_NUM:-99}"
CLI_ARGS=(-regtest -datadir="$DATADIR" -rpcport="$RPCPORT")

if [[ ! -x "$QT" ]]; then
  echo "missing $QT — build with --with-gui=qt5 first" >&2
  exit 1
fi
if [[ ! -x "$DAEMON" || ! -x "$CLI" ]]; then
  echo "missing $DAEMON or $CLI" >&2
  exit 1
fi

rm -rf "$DATADIR"
mkdir -p "$DATADIR"
cat > "$DATADIR/xcoin.conf" <<EOF
server=1
rpcuser=xcoin
rpcpassword=xcoin
listen=0
rpcport=$RPCPORT
port=$P2PPORT
EOF

need_xvfb=0
if [[ -z "${DISPLAY:-}" ]]; then
  need_xvfb=1
fi

QT_PID=""
XVFB_PID=""
cleanup() {
  if [[ -n "${QT_PID:-}" ]]; then
    "$CLI" "${CLI_ARGS[@]}" stop >/dev/null 2>&1 || kill "$QT_PID" 2>/dev/null || true
    wait "$QT_PID" 2>/dev/null || true
  fi
  if [[ -n "${XVFB_PID:-}" ]]; then
    kill "$XVFB_PID" 2>/dev/null || true
  fi
}
trap cleanup EXIT

"$DAEMON" -regtest -datadir="$DATADIR" -daemon -server -listen=0 \
  -rpcport="$RPCPORT" -port="$P2PPORT" \
  -xaccount=NFTRVN -xverified=NFTRVN >/tmp/xcoin-gui-precreate.log 2>&1 || true
up=0
for _ in $(seq 1 80); do
  if "$CLI" "${CLI_ARGS[@]}" getlotteryinfo >/dev/null 2>&1; then
    up=1
    break
  fi
  sleep 0.25
done
if [[ "$up" -ne 1 ]]; then
  echo "xcoind did not become ready while pre-creating the wallet" >&2
  tail -40 "$DATADIR/regtest/debug.log" 2>/dev/null || true
  exit 1
fi
"$CLI" "${CLI_ARGS[@]}" stop >/dev/null
for _ in $(seq 1 40); do
  if ! "$CLI" "${CLI_ARGS[@]}" getlotteryinfo >/dev/null 2>&1; then
    break
  fi
  sleep 0.2
done

if [[ "$need_xvfb" -eq 1 ]]; then
  Xvfb ":$DISPLAY_NUM" -screen 0 1400x900x24 >/tmp/xcoin-xvfb.log 2>&1 &
  XVFB_PID=$!
  export DISPLAY=":$DISPLAY_NUM"
  sleep 0.4
fi

"$QT" -regtest -datadir="$DATADIR" \
  -splash=0 -rpcport="$RPCPORT" -port="$P2PPORT" \
  -xaccount=NFTRVN -xverified=NFTRVN \
  >/tmp/xcoin-qt-smoke.log 2>&1 &
QT_PID=$!

up=0
for _ in $(seq 1 80); do
  if "$CLI" "${CLI_ARGS[@]}" getlotteryinfo >/dev/null 2>&1; then
    up=1
    break
  fi
  sleep 0.25
done
if [[ "$up" -ne 1 ]]; then
  echo "xcoin-qt did not become RPC-ready" >&2
  tail -40 /tmp/xcoin-qt-smoke.log >&2 || true
  exit 1
fi

INFO="$("$CLI" "${CLI_ARGS[@]}" getlotteryinfo)"
echo "$INFO"
echo "$INFO" | grep -q '"currency": "XFER"'
echo "$INFO" | grep -q '"local_xaccount": "nftrvn"'

echo "GUI smoke: ok (xcoin-qt pid $QT_PID)"
if [[ "${KEEP_GUI:-0}" != "1" ]]; then
  "$CLI" "${CLI_ARGS[@]}" stop >/dev/null 2>&1 || kill "$QT_PID"
  QT_PID=""
fi
