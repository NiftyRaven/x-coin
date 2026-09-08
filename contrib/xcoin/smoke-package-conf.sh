#!/usr/bin/env bash
# Throwaway -regtest only. Prove a second wallet joins from package xcoin.conf
# addnode= with no CLI -addnode and no terminal. Never starts mainnet.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
XCOIND="${XCOIND:-$ROOT/src/xcoind}"
XCLI="${XCLI:-$ROOT/src/xcoin-cli}"
BASE="${DATADIR:-$ROOT/smoke-package-conf-datadir}"
A_DIR="$BASE/a"
B_DIR="$BASE/b"
C_DIR="$BASE/c"
PKG="$BASE/pkg"
EMPTY_PKG="$BASE/empty-pkg"
A_CLI=("$XCLI" -regtest -datadir="$A_DIR" -rpcport=29442)
B_CLI=("$XCLI" -regtest -datadir="$B_DIR" -rpcport=29452)
C_CLI=("$XCLI" -regtest -datadir="$C_DIR" -rpcport=29462)

if [[ ! -x "$XCOIND" ]]; then
  echo "missing $XCOIND — build first" >&2
  exit 1
fi

rm -rf "$BASE"
mkdir -p "$A_DIR" "$B_DIR" "$C_DIR" "$PKG" "$EMPTY_PKG"

# Shipped template has addnode commented — no invented host.
cp "$ROOT/contrib/xcoin/xcoin.conf" "$EMPTY_PKG/xcoin.conf"
if grep -E '^[[:space:]]*addnode=' "$EMPTY_PKG/xcoin.conf"; then
  echo "shipped xcoin.conf must not contain an uncommented addnode" >&2
  exit 1
fi
if grep -E '^[[:space:]]*xoauthclientid=[[:alnum:]]' "$EMPTY_PKG/xcoin.conf"; then
  echo "shipped xcoin.conf must not contain a baked Client ID" >&2
  exit 1
fi

# Operator launch-night edit: one real addnode line (loopback only, this test).
cat > "$PKG/xcoin.conf" <<EOF
listen=1
dnsseed=0
addnode=127.0.0.1:29443
EOF

"$XCOIND" -regtest -datadir="$A_DIR" -server -daemon -listen=1 -port=29443 -rpcport=29442 \
  -connect=0 -dnsseed=0 -packageconf=0 -xoauthmock=alice:verified
for _ in $(seq 1 100); do
  if "${A_CLI[@]}" getlotteryinfo >/dev/null 2>&1; then
    break
  fi
  sleep 0.2
done

# B joins only through -packageconf (no -addnode on the command line).
"$XCOIND" -regtest -datadir="$B_DIR" -server -daemon -listen=0 -port=29453 -rpcport=29452 \
  -dnsseed=0 -packageconf="$PKG/xcoin.conf" -xoauthmock=bob:verified

# C uses the shipped commented template: must sit alone (no invented seed).
"$XCOIND" -regtest -datadir="$C_DIR" -server -daemon -listen=0 -port=29463 -rpcport=29462 \
  -dnsseed=0 -packageconf="$EMPTY_PKG/xcoin.conf" -connect=0 -xoauthmock=carol:verified

cleanup() {
  "${A_CLI[@]}" stop >/dev/null 2>&1 || true
  "${B_CLI[@]}" stop >/dev/null 2>&1 || true
  "${C_CLI[@]}" stop >/dev/null 2>&1 || true
  for _ in $(seq 1 50); do
    if ! "${A_CLI[@]}" getlotteryinfo >/dev/null 2>&1 \
       && ! "${B_CLI[@]}" getlotteryinfo >/dev/null 2>&1 \
       && ! "${C_CLI[@]}" getlotteryinfo >/dev/null 2>&1; then
      sleep 0.4
      return 0
    fi
    sleep 0.2
  done
}
trap cleanup EXIT

up=0
for _ in $(seq 1 100); do
  if "${A_CLI[@]}" getlotteryinfo >/dev/null 2>&1 \
     && "${B_CLI[@]}" getlotteryinfo >/dev/null 2>&1 \
     && "${C_CLI[@]}" getlotteryinfo >/dev/null 2>&1; then
    up=1
    break
  fi
  sleep 0.2
done
if [[ "$up" -ne 1 ]]; then
  echo "nodes did not become ready" >&2
  tail -40 "$B_DIR/regtest/debug.log" 2>/dev/null || true
  exit 1
fi

ok=0
for _ in $(seq 1 80); do
  peers_a=$("${A_CLI[@]}" getconnectioncount)
  peers_b=$("${B_CLI[@]}" getconnectioncount)
  if [[ "$peers_a" -ge 1 && "$peers_b" -ge 1 ]]; then
    ok=1
    break
  fi
  sleep 0.5
done
if [[ "$ok" -ne 1 ]]; then
  echo "package addnode did not connect B to A" >&2
  echo "peers_a=$peers_a peers_b=$peers_b" >&2
  "${A_CLI[@]}" getpeerinfo >&2 || true
  "${B_CLI[@]}" getpeerinfo >&2 || true
  exit 1
fi

peers_c=$("${C_CLI[@]}" getconnectioncount)
if [[ "$peers_c" -ne 0 ]]; then
  echo "commented package conf invented a seed (C has $peers_c peers)" >&2
  "${C_CLI[@]}" getpeerinfo >&2 || true
  exit 1
fi

added=$("${B_CLI[@]}" getaddednodeinfo)
echo "$added"
echo "$added" | python3 -c '
import json,sys
j=json.load(sys.stdin)
hosts=[x.get("addednode","") for x in j]
if "127.0.0.1:29443" not in hosts:
    sys.exit("B did not load addnode from package conf, got %r" % hosts)
print("PACKAGE_ADDNODE_OK")
'

echo "package-conf smoke: ok"
