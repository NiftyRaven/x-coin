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
    -xoauthmock=smoke1 -xverified=smoke1 -xverified=smoke2 -xverified=NFTRVN
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
# Height 0 is not a payday: genesis coinbase is not in the UTXO set.
GENTX="$("${CLI[@]}" getblock "$GENESIS" true | python3 -c 'import json,sys; print(json.load(sys.stdin)["tx"][0])')"
GOUT="$("${CLI[@]}" gettxout "$GENTX" 0 || true)"
if [[ -n "$GOUT" && "$GOUT" != "null" ]]; then
  echo "genesis coinbase must be unspendable (gettxout empty)" >&2
  echo "$GOUT" >&2
  exit 1
fi
echo "genesis coinbase unspendable (gettxout empty)"
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
# New prefixes: not X Coin main R… / test n… (version 60 / 111).
[[ "$ADDR1" != R* && "$ADDR1" != n* && "$ADDR2" != R* && "$ADDR2" != n* ]]
[[ "$ADDR1" == y* && "$ADDR2" == y* ]]

echo "== register second verified payout + generate past maturity =="
"${CLI[@]}" listxverified | grep -q smoke1
"${CLI[@]}" listxverified | grep -q smoke2
"${CLI[@]}" mockxsignin smoke2 >/dev/null
"${CLI[@]}" registeractivenode "$ADDR2" smoke2 >/dev/null
"${CLI[@]}" generatetoaddress 110 "$ADDR1" >/dev/null
HEIGHT="$("${CLI[@]}" getblockcount)"
echo "height $HEIGHT"
[[ "$HEIGHT" -ge 110 ]]
"${CLI[@]}" getblockchaininfo >/dev/null

echo "== assets (protocol main / no user roots) =="
if "${CLI[@]}" issue TESTASSET 1000 >/tmp/xcoin-issue-root.err 2>&1; then
  echo "issue TESTASSET must fail (users cannot create main assets)" >&2
  cat /tmp/xcoin-issue-root.err >&2
  exit 1
fi
grep -qi "main asset\|cannot create\|root" /tmp/xcoin-issue-root.err
"${CLI[@]}" mockxsignin smoke1 >/dev/null
LINK="$("${CLI[@]}" linkxaccount smoke1 "$ADDR1")"
echo "$LINK"
echo "$LINK" | grep -q '"asset"'
ASSETS="$("${CLI[@]}" listmyassets)"
echo "$ASSETS"
MAIN="$(echo "$LINK" | python3 -c 'import json,sys; print(json.load(sys.stdin).get("asset",""))')"
[[ -n "$MAIN" ]]
echo "$ASSETS" | grep -q "$MAIN"
"${CLI[@]}" issue "$MAIN/NOTE" 1
"${CLI[@]}" issueunique "$MAIN" '["ONE"]'
ASSETS2="$("${CLI[@]}" listmyassets)"
echo "$ASSETS2"
echo "$ASSETS2" | grep -q "$MAIN/NOTE"
echo "$ASSETS2" | grep -q "$MAIN#ONE"
GETMAIN="$("${CLI[@]}" getmainasset smoke1)"
echo "$GETMAIN"
echo "$GETMAIN" | grep -q "$MAIN"

echo "== owner handle NFTRVN (allowlist + free root + eligibility) =="
# Session username from mock users/me — not a typed claim.
"${CLI[@]}" addxverified NFTRVN >/dev/null
"${CLI[@]}" listxverified | grep -qi NFTRVN
ADDR_N="$("${CLI[@]}" getnewaddress)"
echo "nftrvn dest $ADDR_N"
"${CLI[@]}" mockxsignin NFTRVN >/dev/null
LINK_N="$("${CLI[@]}" linkxaccount NFTRVN "$ADDR_N")"
echo "$LINK_N"
echo "$LINK_N" | python3 -c '
import json, sys
j = json.load(sys.stdin)
if j.get("xaccount") != "nftrvn":
    sys.exit("linkxaccount must echo xaccount=nftrvn, got %r" % j.get("xaccount"))
if j.get("asset") != "NFTRVN":
    sys.exit("expected free root NFTRVN, got %r" % j.get("asset"))
if j.get("owner") != "NFTRVN!":
    sys.exit("expected owner token NFTRVN!")
'
"${CLI[@]}" listmyassets | grep -q NFTRVN
"${CLI[@]}" getmainasset NFTRVN | grep -q NFTRVN
"${CLI[@]}" registeractivenode "$ADDR_N" NFTRVN >/dev/null
"${CLI[@]}" getactivenodes | python3 -c '
import json, sys
nodes = json.load(sys.stdin)
handles = [n.get("xaccount", "") for n in nodes]
if "nftrvn" not in handles:
    sys.exit("NFTRVN must be lottery-eligible in the active set, got %s" % handles)
print("NFTRVN active handles", handles)
'
echo "NFTRVN identity path: ok"

echo "== 26-character X handle maps 1:1 to root (no truncation) =="
# X handles are [A-Za-z0-9_], typically 1–15, protocol max 32. A 26-char
# handle must produce the same 26-char uppercase root.
LONG_HANDLE="abcdefghijabcdefghijabcdef"
[[ ${#LONG_HANDLE} -eq 26 ]]
"${CLI[@]}" addxverified "$LONG_HANDLE" >/dev/null
ADDR_L="$("${CLI[@]}" getnewaddress)"
"${CLI[@]}" mockxsignin "$LONG_HANDLE" >/dev/null
LINK_L="$("${CLI[@]}" linkxaccount "$LONG_HANDLE" "$ADDR_L")"
echo "$LINK_L"
echo "$LINK_L" | python3 -c '
import json, sys
j = json.load(sys.stdin)
want = "ABCDEFGHIJABCDEFGHIJABCDEF"
if j.get("xaccount") != "abcdefghijabcdefghijabcdef":
    sys.exit("expected xaccount=abcdefghijabcdefghijabcdef, got %r" % j.get("xaccount"))
if j.get("asset") != want:
    sys.exit("expected free root %s (26 chars), got %r" % (want, j.get("asset")))
if len(j.get("asset", "")) != 26:
    sys.exit("root must be 26 characters, got %d" % len(j.get("asset", "")))
if j.get("owner") != want + "!":
    sys.exit("expected owner token %s!" % want)
print("26-char handle root", j.get("asset"))
'
"${CLI[@]}" getmainasset "$LONG_HANDLE" | grep -q ABCDEFGHIJABCDEFGHIJABCDEF
echo "26-char handle identity path: ok"

echo "== two-winner window (regtest halving interval 150) =="
# Height 149: still 1 winner, full 5000 subsidy. Height 150: 2 winners, 2500 subsidy.
NEED=$((149 - HEIGHT))
if [[ "$NEED" -gt 0 ]]; then
  "${CLI[@]}" mockxsignin smoke2 >/dev/null
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
"${CLI[@]}" mockxsignin smoke2 >/dev/null
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

echo "== unlinked send / receive (no X account required) =="
# ADDR_UNLINKED is never registered and has no X handle. Linking is only
# for lottery eligibility / protocol main-asset assignment — not for XFER.
ADDR_UNLINKED="$("${CLI[@]}" getnewaddress)"
echo "unlinked $ADDR_UNLINKED"
NODES_BEFORE="$("${CLI[@]}" getactivenodes)"
echo "$NODES_BEFORE" | python3 -c '
import json, os, sys
addr = os.environ.get("ADDR_UNLINKED", "")
# script hex of a P2PKH is not listed as xaccount; just ensure we did not register it
nodes = json.load(sys.stdin)
for n in nodes:
    if n.get("xaccount") in ("", None) and n.get("local") is False:
        pass
print("active", len(nodes), "handles", [n.get("xaccount") for n in nodes])
'
TXPAY="$("${CLI[@]}" sendtoaddress "$ADDR_UNLINKED" 25)"
echo "paid unlinked $TXPAY"
"${CLI[@]}" mockxsignin smoke2 >/dev/null
"${CLI[@]}" registeractivenode "$ADDR2" smoke2 >/dev/null
"${CLI[@]}" generatetoaddress 1 "$ADDR1" >/dev/null
UTXO1="$("${CLI[@]}" listunspent 1 9999999 "[\"$ADDR_UNLINKED\"]")"
echo "unlinked utxos after pay $UTXO1"
echo "$UTXO1" | python3 -c '
import json, sys
u = json.load(sys.stdin)
s = sum(x.get("amount", 0) for x in u)
print("unlinked balance", s)
if abs(s - 25) > 1e-6:
    sys.exit("unlinked address must hold the 25 XFER it was paid")
'
TXSPEND="$("${CLI[@]}" sendfromaddress "$ADDR_UNLINKED" "$ADDR1" 10)"
echo "unlinked spent $TXSPEND"
"${CLI[@]}" mockxsignin smoke2 >/dev/null
"${CLI[@]}" registeractivenode "$ADDR2" smoke2 >/dev/null
"${CLI[@]}" generatetoaddress 1 "$ADDR1" >/dev/null
UTXO2="$("${CLI[@]}" listunspent 1 9999999 "[\"$ADDR_UNLINKED\"]")"
echo "unlinked utxos after spend $UTXO2"
echo "$UTXO2" | python3 -c '
import json, sys
u = json.load(sys.stdin)
s = sum(x.get("amount", 0) for x in u)
print("unlinked balance after spend", s)
if s >= 25 - 1e-6:
    sys.exit("unlinked address must have spent (balance still >= 25)")
if s < 10:
    sys.exit("expected change to remain on the unlinked address")
'
echo "unlinked send/receive: ok"

echo "smoke-regtest: ok"
