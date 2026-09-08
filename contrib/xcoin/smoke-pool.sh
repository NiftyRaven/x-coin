#!/usr/bin/env bash
# Lottery pools: even split among members; tickets only from X Verified running wallets.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
XCOIND="${XCOIND:-$ROOT/src/xcoind}"
XCLI="${XCLI:-$ROOT/src/xcoin-cli}"
BASE="${DATADIR:-$ROOT/smoke-pool-datadir}"
A_DIR="$BASE/alice"
B_DIR="$BASE/bob"
G_DIR="$BASE/ghost"
A_CLI=("$XCLI" -regtest -datadir="$A_DIR" -rpcport=28842)
B_CLI=("$XCLI" -regtest -datadir="$B_DIR" -rpcport=28852)
G_CLI=("$XCLI" -regtest -datadir="$G_DIR" -rpcport=28862)

if [[ ! -x "$XCOIND" ]]; then
  echo "missing $XCOIND — build first" >&2
  exit 1
fi

rm -rf "$BASE"
mkdir -p "$A_DIR" "$B_DIR" "$G_DIR"

wait_rpc() {
  local -n _cli=$1
  for _ in $(seq 1 120); do
    if "${_cli[@]}" getlotteryinfo >/dev/null 2>&1; then
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
  "${G_CLI[@]}" stop >/dev/null 2>&1 || true
  sleep 0.4
}
trap cleanup EXIT

echo "== Alice (X Verified) creates pool =="
"$XCOIND" -regtest -datadir="$A_DIR" -server -daemon -listen=1 -port=28843 \
  -rpcport=28842 -connect=0 -dnsseed=0 -txindex=1 \
  -xoauthmock=alice:verified
wait_rpc A_CLI
"${A_CLI[@]}" createpool "Night Crew" crew1 s3cret
MINE="$("${A_CLI[@]}" getmypool)"
echo "$MINE"
echo "$MINE" | python3 -c '
import json,sys
j=json.load(sys.stdin)
assert j.get("in_pool") in (True, 1)
assert j.get("name")=="Night Crew"
assert j.get("id")=="crew1"
assert j.get("has_password") in (True, 1)
assert "password" not in j
assert j.get("eligible_tickets")==1
assert len(j.get("addresses") or [])==1
'

PUB="$("${A_CLI[@]}" listpools)"
echo "$PUB"
echo "$PUB" | python3 -c '
import json,sys
arr=json.load(sys.stdin)
assert len(arr)==1
j=arr[0]
assert j.get("name")=="Night Crew"
assert "id" not in j
assert "password" not in j
assert len(j.get("addresses") or [])==1
'

echo "== Bob (X Verified) joins; Ghost (unverified) joins =="
"$XCOIND" -regtest -datadir="$B_DIR" -server -daemon -listen=0 -port=28853 \
  -rpcport=28852 -addnode=127.0.0.1:28843 -dnsseed=0 -txindex=1 \
  -xoauthmock=bob:verified
"$XCOIND" -regtest -datadir="$G_DIR" -server -daemon -listen=0 -port=28863 \
  -rpcport=28862 -addnode=127.0.0.1:28843 -dnsseed=0 -txindex=1 \
  -xoauthmock=ghost:unverified
wait_rpc B_CLI
wait_rpc G_CLI
"${B_CLI[@]}" joinpool crew1 s3cret
"${G_CLI[@]}" joinpool crew1 s3cret

echo "== wait until Alice sees three member addresses and two tickets =="
got=0
for _ in $(seq 1 120); do
  ok="$("${A_CLI[@]}" getmypool | python3 -c 'import json,sys; j=json.load(sys.stdin); print(int(len(j.get("addresses") or [])>=3 and int(j.get("eligible_tickets") or 0)>=2))')"
  if [[ "$ok" -eq 1 ]]; then
    got=1
    break
  fi
  sleep 0.5
done
if [[ "$got" -ne 1 ]]; then
  echo "Alice did not learn 3 pool members / 2 tickets" >&2
  "${A_CLI[@]}" getmypool >&2 || true
  "${A_CLI[@]}" listpools >&2 || true
  "${B_CLI[@]}" getmypool >&2 || true
  "${G_CLI[@]}" getmypool >&2 || true
  exit 1
fi

INFO="$("${A_CLI[@]}" getmypool)"
echo "$INFO"
echo "$INFO" | python3 -c '
import json,sys
j=json.load(sys.stdin)
assert len(j.get("addresses") or [])==3, j
# Alice+Bob verified running; ghost unverified has zero tickets
assert j.get("eligible_tickets")==2, j
'

echo "$("${A_CLI[@]}" listpools)" | python3 -c '
import json,sys
j=json.load(sys.stdin)[0]
assert "id" not in j
assert "password" not in j
assert j.get("name")=="Night Crew"
assert len(j.get("addresses") or [])==3
'

echo "== generate a lottery block; coinbase must even-split among 3 members =="
ADDR="$("${A_CLI[@]}" getmypool | python3 -c 'import json,sys; print((json.load(sys.stdin).get("addresses") or [""])[0])')"
"${A_CLI[@]}" generatetoaddress 1 "$ADDR" >/dev/null
HASH="$("${A_CLI[@]}" getblockhash 1)"
BLK="$("${A_CLI[@]}" getblock "$HASH" 2)"
echo "$BLK" | python3 -c '
import json,sys
b=json.load(sys.stdin)
cb=b["tx"][0]
pays=[v for v in cb["vout"] if v.get("value",0)>0]
assert len(pays)==3, pays
vals=sorted(v["value"] for v in pays)
# 5000 XFER even split; remainder satoshis to first outputs
assert abs(sum(vals)-5000)<1e-6, vals
assert vals[0]>=1666.0 and vals[-1]<=1667.0, vals
print("pool coinbase split", vals)
'

echo "== Ghost leaves (unverified leech gone); then last members dissolve =="
"${G_CLI[@]}" leavepool crew1
got=0
for _ in $(seq 1 80); do
  n="$("${A_CLI[@]}" listpools | python3 -c 'import json,sys; a=json.load(sys.stdin); print(len(a[0].get("addresses") or []) if a else 0)')"
  if [[ "$n" -eq 2 ]]; then
    got=1
    break
  fi
  sleep 0.25
done
if [[ "$got" -ne 1 ]]; then
  echo "Alice did not drop Ghost from the pool" >&2
  "${A_CLI[@]}" getmypool >&2 || true
  "${A_CLI[@]}" listpools >&2 || true
  exit 1
fi
echo "$("${A_CLI[@]}" listpools)" | python3 -c '
import json,sys
j=json.load(sys.stdin)[0]
assert len(j.get("addresses") or [])==2, j
assert j.get("eligible_tickets")==2, j
assert "id" not in j
assert "password" not in j
'
"${B_CLI[@]}" leavepool crew1
got=0
for _ in $(seq 1 80); do
  n="$("${A_CLI[@]}" listpools | python3 -c 'import json,sys; a=json.load(sys.stdin); print(len(a[0].get("addresses") or []) if a else 0)')"
  if [[ "$n" -eq 1 ]]; then
    got=1
    break
  fi
  sleep 0.25
done
if [[ "$got" -ne 1 ]]; then
  echo "Alice did not drop Bob from the pool" >&2
  "${A_CLI[@]}" getmypool >&2 || true
  "${A_CLI[@]}" listpools >&2 || true
  exit 1
fi
"${A_CLI[@]}" leavepool crew1
echo "$("${A_CLI[@]}" getmypool)" | python3 -c '
import json,sys
j=json.load(sys.stdin)
assert j.get("in_pool") in (False, 0) or not j.get("in_pool")
assert "id" not in j
'
echo "$("${A_CLI[@]}" listpools)" | python3 -c '
import json,sys
arr=json.load(sys.stdin)
assert arr==[]
'

echo "smoke-pool: ok"
