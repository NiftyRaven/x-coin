#!/usr/bin/env bash
# Eligibility: unlinked node is not in the lottery; linked+allowlisted is.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
XCOIND="${XCOIND:-$ROOT/src/xcoind}"
XCLI="${XCLI:-$ROOT/src/xcoin-cli}"
DATADIR="${DATADIR:-$ROOT/smoke-eligibility-datadir}"
CLI=("$XCLI" -regtest -datadir="$DATADIR")

if [[ ! -x "$XCOIND" ]]; then
  echo "missing $XCOIND — build first" >&2
  exit 1
fi

rm -rf "$DATADIR"
mkdir -p "$DATADIR"

start_node() {
  "$XCOIND" -regtest -datadir="$DATADIR" -server -daemon -listen=0 "$@"
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
}

stop_node() {
  "${CLI[@]}" stop >/dev/null 2>&1 || true
  for _ in $(seq 1 50); do
    if ! "${CLI[@]}" getlotteryinfo >/dev/null 2>&1; then
      return 0
    fi
    sleep 0.2
  done
}

cleanup() {
  stop_node
}
trap cleanup EXIT

echo "== unlinked node is not eligible =="
start_node
INFO="$("${CLI[@]}" getlotteryinfo)"
echo "$INFO"
echo "$INFO" | python3 -c '
import json,sys
j=json.load(sys.stdin)
if j.get("local_eligible") is not False:
    sys.exit("unlinked node must have local_eligible=false")
if j.get("local_xaccount"):
    sys.exit("unlinked node must have empty local_xaccount")
if j.get("active_nodes", 1) != 0:
    sys.exit("unlinked node must have zero active nodes")
'
NODES="$("${CLI[@]}" getactivenodes)"
echo "$NODES" | python3 -c 'import json,sys; n=json.load(sys.stdin); assert n==[], n'

if "${CLI[@]}" registeractivenode >/dev/null 2>&1; then
  echo "registeractivenode must fail without a linked X account" >&2
  exit 1
fi
"${CLI[@]}" addxverified ghost >/dev/null
# Still unlinked: allowlist alone is not enough.
INFO2="$("${CLI[@]}" getlotteryinfo)"
echo "$INFO2" | python3 -c '
import json,sys
j=json.load(sys.stdin)
if j.get("local_eligible") is not False:
    sys.exit("allowlist without a Sign in with X session must stay ineligible")
if j.get("verified_accounts", 0) < 1:
    sys.exit("addxverified did not stick")
'
stop_node
# Drop the first-run allowlist so the next start is linked but not listed.
rm -f "$DATADIR/regtest/verified-x-accounts.txt"
sleep 0.5

echo "== linked but not allowlisted is not eligible =="
start_node -xoauthmock=ghost
INFO3="$("${CLI[@]}" getlotteryinfo)"
echo "$INFO3"
echo "$INFO3" | python3 -c '
import json,sys
j=json.load(sys.stdin)
if j.get("local_xaccount") != "ghost":
    sys.exit("expected local_xaccount=ghost")
if j.get("local_eligible") is not False:
    sys.exit("linked-but-unlisted must be ineligible")
'
if "${CLI[@]}" registeractivenode >/dev/null 2>&1; then
  echo "registeractivenode must fail when handle is not allowlisted" >&2
  exit 1
fi

echo "== addxverified makes the linked node eligible =="
"${CLI[@]}" addxverified ghost >/dev/null
INFO4="$("${CLI[@]}" getlotteryinfo)"
echo "$INFO4"
echo "$INFO4" | python3 -c '
import json,sys
j=json.load(sys.stdin)
if j.get("local_eligible") is not True:
    sys.exit("linked+allowlisted must be eligible")
if j.get("active_nodes", 0) < 1:
    sys.exit("eligible local node must be in the active set")
'
REG="$("${CLI[@]}" registeractivenode)"
echo "$REG"
echo "$REG" | python3 -c '
import json,sys
j=json.load(sys.stdin)
if j.get("xaccount") != "ghost":
    sys.exit("registeractivenode must echo xaccount")
'
NODES2="$("${CLI[@]}" getactivenodes)"
echo "$NODES2"
echo "$NODES2" | python3 -c '
import json,sys
n=json.load(sys.stdin)
if not n or n[0].get("xaccount") != "ghost":
    sys.exit("getactivenodes must show linked xaccount")
'
ADDR="$("${CLI[@]}" getnewaddress)"
[[ "$ADDR" == y* && "$ADDR" != R* && "$ADDR" != n* ]]
if "${CLI[@]}" registeractivenode "$ADDR" botter >/dev/null 2>&1; then
  echo "unverified second identity must be rejected" >&2
  exit 1
fi
"${CLI[@]}" addxverified botter >/dev/null
"${CLI[@]}" mockxsignin botter >/dev/null
"${CLI[@]}" registeractivenode "$ADDR" botter >/dev/null
COUNT="$("${CLI[@]}" getactivenodes | python3 -c 'import json,sys; print(len(json.load(sys.stdin)))')"
[[ "$COUNT" -ge 2 ]]

echo "smoke-eligibility: ok"
