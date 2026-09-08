#!/usr/bin/env bash
# Regtest smoke: lottery + multi-winner coinbase + asset issue.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
XCOIND="${XCOIND:-$ROOT/src/xcoind}"
XCLI="${XCLI:-$ROOT/src/xcoin-cli}"
DATADIR="${DATADIR:-$ROOT/smoke-regtest-datadir}"
CLI=("$XCLI" -regtest -datadir="$DATADIR")

if [[ ! -x "$XCOIND" ]]; then
  echo "missing $XCOIND — build first" >&2
  exit 1
fi

rm -rf "$DATADIR"
mkdir -p "$DATADIR"

"$XCOIND" -regtest -datadir="$DATADIR" -server -daemon -listen=0 \
  -xaccount=smoke1 -xverified=smoke1 -xverified=smoke2
cleanup() {
  "${CLI[@]}" stop >/dev/null 2>&1 || true
  for _ in $(seq 1 50); do
    if ! "${CLI[@]}" getlotteryinfo >/dev/null 2>&1; then
      sleep 0.4
      return 0
    fi
    sleep 0.2
  done
}
trap cleanup EXIT

up=0
for _ in $(seq 1 100); do
  if "${CLI[@]}" getlotteryinfo >/dev/null 2>&1; then
    up=1
    break
  fi
  sleep 0.2
done
if [[ "$up" -ne 1 ]]; then
  echo "xcoind did not become ready" >&2
  tail -20 "$DATADIR/regtest/debug.log" >&2 || true
  exit 1
fi

echo "== genesis / lottery =="
GENESIS="$("${CLI[@]}" getblockhash 0)"
echo "genesis $GENESIS"
[[ "$GENESIS" == "bfce7bfad8116b82f4a0ce4be2fa52e9c9f166248e318ce45728b8fe87451d89" ]]
INFO="$("${CLI[@]}" getlotteryinfo)"
echo "$INFO"
echo "$INFO" | grep -q '"currency": "XFER"'
echo "$INFO" | grep -q '"winner_count": 1'
echo "$INFO" | grep -q '"local_xaccount": "smoke1"'
echo "$INFO" | grep -q '"local_eligible": true'
CHAIN="$("${CLI[@]}" getblockchaininfo)"
echo "$CHAIN" | grep -q '"chain": "regtest"'
echo "$CHAIN" | grep -q '"blocks": 0'

ADDR1="$("${CLI[@]}" getnewaddress)"
ADDR2="$("${CLI[@]}" getnewaddress)"
echo "addr1 $ADDR1"
echo "addr2 $ADDR2"
VA1="$("${CLI[@]}" validateaddress "$ADDR1")"
echo "$VA1" | grep -q '"isvalid": true'
# New prefixes: not Ravencoin main R… / test n… (version 60 / 111).
[[ "$ADDR1" != R* && "$ADDR1" != n* && "$ADDR2" != R* && "$ADDR2" != n* ]]
[[ "$ADDR1" == y* && "$ADDR2" == y* ]]

echo "== register second verified payout + generate past maturity =="
"${CLI[@]}" listxverified | grep -q smoke1
"${CLI[@]}" listxverified | grep -q smoke2
"${CLI[@]}" registeractivenode "$ADDR2" smoke2 >/dev/null
"${CLI[@]}" generatetoaddress 110 "$ADDR1" >/dev/null
HEIGHT="$("${CLI[@]}" getblockcount)"
echo "height $HEIGHT"
[[ "$HEIGHT" -ge 110 ]]
"${CLI[@]}" getblockchaininfo >/dev/null

echo "== asset issue =="
"${CLI[@]}" issue TESTASSET 1000
ASSETS="$("${CLI[@]}" listmyassets)"
echo "$ASSETS"
echo "$ASSETS" | grep -q TESTASSET
echo "$ASSETS" | grep -q 'TESTASSET!'

echo "== two-winner window (regtest halving interval 150) =="
# Height 149: still 1 winner, full 5000 subsidy. Height 150: 2 winners, 2500 subsidy.
NEED=$((149 - HEIGHT))
if [[ "$NEED" -gt 0 ]]; then
  "${CLI[@]}" registeractivenode "$ADDR2" smoke2 >/dev/null
  "${CLI[@]}" generatetoaddress "$NEED" "$ADDR1" >/dev/null
fi
H149="$("${CLI[@]}" getblockhash 149)"
TX149="$("${CLI[@]}" getblock "$H149" true | python3 -c 'import json,sys; print(json.load(sys.stdin)["tx"][0])')"
"${CLI[@]}" getrawtransaction "$TX149" true | python3 -c '
import json, sys
tx = json.load(sys.stdin)
pays = [o for o in tx["vout"] if o.get("value", 0) > 0]
print("height 149 payouts", len(pays), "values", [o["value"] for o in pays])
if len(pays) != 1:
    sys.exit("expected 1 value output at height 149")
if abs(sum(o["value"] for o in pays) - 5000) > 1e-6:
    sys.exit("height 149 subsidy must be 5000")
'
"${CLI[@]}" registeractivenode "$ADDR2" smoke2 >/dev/null
"${CLI[@]}" generatetoaddress 1 "$ADDR1" >/dev/null
H150="$("${CLI[@]}" getblockhash 150)"
TX0="$("${CLI[@]}" getblock "$H150" true | python3 -c 'import json,sys; print(json.load(sys.stdin)["tx"][0])')"
RAW="$("${CLI[@]}" getrawtransaction "$TX0" true)"
echo "$RAW" | python3 -c '
import json, sys
tx = json.load(sys.stdin)
pays = [o for o in tx["vout"] if o.get("value", 0) > 0]
ops = [o for o in tx["vout"] if o["scriptPubKey"].get("type") == "nulldata" or o["scriptPubKey"]["hex"].startswith("6a")]
print("payouts", len(pays), "values", [o["value"] for o in pays], "op_return", len(ops))
if len(pays) < 2:
    sys.exit("expected >=2 value outputs at height 150")
if abs(sum(o["value"] for o in pays) - 2500) > 1e-6:
    sys.exit("height 150 subsidy split must sum to 2500 (first halving)")
if len(ops) < 1:
    sys.exit("expected XHB1 / OP_RETURN commitment")
'
CHAIN2="$("${CLI[@]}" getblockchaininfo)"
echo "$CHAIN2" | grep -q '"blocks": 150'
echo "smoke-regtest: ok"
