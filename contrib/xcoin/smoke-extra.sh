#!/usr/bin/env bash
# Extra regtest coverage: node start, throwaway wallet, send/receive,
# 26- and 32-character handle mapping, typed handle rejected, pool leave.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
XCOIND="${XCOIND:-$ROOT/src/xcoind}"
XCLI="${XCLI:-$ROOT/src/xcoin-cli}"
BASE="${DATADIR:-$ROOT/smoke-extra-datadir}"
A_DIR="$BASE/a"
B_DIR="$BASE/b"
A_CLI=("$XCLI" -regtest -datadir="$A_DIR" -rpcport=28942)
B_CLI=("$XCLI" -regtest -datadir="$B_DIR" -rpcport=28952)

if [[ ! -x "$XCOIND" || ! -x "$XCLI" ]]; then
  echo "missing $XCOIND or $XCLI" >&2
  exit 1
fi

rm -rf "$BASE"
mkdir -p "$A_DIR" "$B_DIR"

wait_rpc() {
  local -n _cli=$1
  for _ in $(seq 1 120); do
    if "${_cli[@]}" getblockchaininfo >/dev/null 2>&1; then
      return 0
    fi
    sleep 0.2
  done
  echo "node did not become ready" >&2
  return 1
}

cleanup() {
  "${A_CLI[@]}" stop >/dev/null 2>&1 || true
  "${B_CLI[@]}" stop >/dev/null 2>&1 || true
  sleep 0.3
}
trap cleanup EXIT

echo "== node start on throwaway regtest datadirs =="
"$XCOIND" -regtest -datadir="$A_DIR" -server -daemon -listen=1 -port=28943 \
  -rpcport=28942 -connect=0 -dnsseed=0 -txindex=1 \
  -xoauthmock=alice:verified
"$XCOIND" -regtest -datadir="$B_DIR" -server -daemon -listen=0 -port=28953 \
  -rpcport=28952 -addnode=127.0.0.1:28943 -dnsseed=0 \
  -xoauthmock=bob:verified
wait_rpc A_CLI
wait_rpc B_CLI

CHAIN="$("${A_CLI[@]}" getblockchaininfo)"
echo "$CHAIN" | python3 -c 'import json,sys; j=json.load(sys.stdin); assert j["chain"]=="regtest"'
echo "$CHAIN" | grep -q '"currency": "XFER"'

echo "== wallet.dat created under datadir/regtest, not the main ledger path =="
"${A_CLI[@]}" getwalletinfo >/dev/null
test -f "$A_DIR/regtest/wallet.dat"
test ! -f "$A_DIR/wallet.dat"

echo "== send / receive =="
ADDR_A="$("${A_CLI[@]}" getnewaddress)"
ADDR_B="$("${B_CLI[@]}" getnewaddress)"
"${A_CLI[@]}" generatetoaddress 110 "$ADDR_A" >/dev/null
TXID="$("${A_CLI[@]}" sendtoaddress "$ADDR_B" 12.5)"
echo "txid $TXID"
"${A_CLI[@]}" generatetoaddress 1 "$ADDR_A" >/dev/null
got=0
for _ in $(seq 1 50); do
  BAL="$("${B_CLI[@]}" getbalance)"
  if python3 -c "import sys; raise SystemExit(0 if float(sys.argv[1])>=12.4 else 1)" "$BAL"; then
    got=1
    echo "B balance $BAL"
    break
  fi
  sleep 0.25
done
[[ "$got" -eq 1 ]]

echo "== 26-character handle maps 1:1; 32-character handle fits =="
H26="abcdefghijabcdefghijabcdef"
H32="$(python3 -c 'print("a"*32)')"
[[ ${#H26} -eq 26 && ${#H32} -eq 32 ]]
"${A_CLI[@]}" mockxsignin "${H26}:verified" >/dev/null
LINK26="$("${A_CLI[@]}" linkxaccount "$H26" "$ADDR_A")"
echo "$LINK26"
echo "$LINK26" | python3 -c '
import json,sys
j=json.load(sys.stdin)
assert j.get("asset")=="ABCDEFGHIJABCDEFGHIJABCDEF"
assert len(j.get("asset"))==26
'
ADDR32="$("${A_CLI[@]}" getnewaddress)"
"${A_CLI[@]}" mockxsignin "${H32}:verified" >/dev/null
LINK32="$("${A_CLI[@]}" linkxaccount "$H32" "$ADDR32")"
echo "$LINK32"
echo "$LINK32" | python3 -c '
import json,sys
j=json.load(sys.stdin)
assert j.get("asset")=="A"*32
assert len(j.get("asset"))==32
'

echo "== typed handle cannot claim another account or Verified X =="
"${A_CLI[@]}" mockxsignin alice:verified >/dev/null
if "${A_CLI[@]}" linkxaccount NFTRVN "$ADDR_A" >/tmp/xcoin-typed-handle.err 2>&1; then
  echo "linkxaccount NFTRVN must fail while signed in as alice" >&2
  cat /tmp/xcoin-typed-handle.err >&2
  exit 1
fi
grep -qi "sign in\|session\|mismatch\|not" /tmp/xcoin-typed-handle.err
# Unverified mock is signed in but not lottery-eligible
"${A_CLI[@]}" mockxsignin ghost:unverified >/dev/null
INFO="$("${A_CLI[@]}" getlotteryinfo)"
echo "$INFO"
echo "$INFO" | python3 -c '
import json,sys
j=json.load(sys.stdin)
assert j.get("local_xaccount")=="ghost"
assert j.get("local_eligible") is False
'

echo "== pool create / leave =="
"${A_CLI[@]}" mockxsignin alice:verified >/dev/null
"${A_CLI[@]}" createpool "Audit Pool" audit1 p4ss
"${A_CLI[@]}" getmypool | python3 -c 'import json,sys; j=json.load(sys.stdin); assert j.get("id")=="audit1" and j.get("in_pool") in (True,1)'
"${A_CLI[@]}" leavepool audit1
"${A_CLI[@]}" getmypool | python3 -c 'import json,sys; j=json.load(sys.stdin); assert not j.get("in_pool")'

echo "extra smokes: ok"
