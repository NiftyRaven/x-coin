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

"$XCOIND" -regtest -datadir="$DATADIR" -server -daemon -listen=0
cleanup() {
  "${CLI[@]}" stop >/dev/null 2>&1 || true
}
trap cleanup EXIT

for _ in $(seq 1 50); do
  if "${CLI[@]}" getblockchaininfo >/dev/null 2>&1; then
    break
  fi
  sleep 0.2
done

echo "== genesis / lottery =="
GENESIS="$("${CLI[@]}" getblockhash 0)"
echo "genesis $GENESIS"
INFO="$("${CLI[@]}" getlotteryinfo)"
echo "$INFO"
echo "$INFO" | grep -q '"currency": "XFER"'
echo "$INFO" | grep -q '"winner_count": 1'

ADDR1="$("${CLI[@]}" getnewaddress)"
ADDR2="$("${CLI[@]}" getnewaddress)"
echo "addr1 $ADDR1"
echo "addr2 $ADDR2"
[[ "$ADDR1" == y* ]] || { echo "expected y… regtest prefix, got $ADDR1" >&2; exit 1; }

echo "== register second payout + generate past maturity =="
"${CLI[@]}" registeractivenode "$ADDR2" >/dev/null
"${CLI[@]}" generatetoaddress 110 "$ADDR1" >/dev/null
HEIGHT="$("${CLI[@]}" getblockcount)"
echo "height $HEIGHT"
[[ "$HEIGHT" -ge 110 ]]

echo "== asset issue =="
"${CLI[@]}" issue TESTASSET 1000
ASSETS="$("${CLI[@]}" listmyassets)"
echo "$ASSETS"
echo "$ASSETS" | grep -q TESTASSET
echo "$ASSETS" | grep -q 'TESTASSET!'

echo "== two-winner window (regtest halving interval 150) =="
NEED=$((150 - HEIGHT + 2))
if [[ "$NEED" -gt 0 ]]; then
  "${CLI[@]}" registeractivenode "$ADDR2" >/dev/null
  "${CLI[@]}" generatetoaddress "$NEED" "$ADDR1" >/dev/null
fi
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
if abs(sum(o["value"] for o in pays) - 5000) > 1e-6:
    sys.exit("subsidy split must sum to 5000")
'
echo "smoke-regtest: ok"
