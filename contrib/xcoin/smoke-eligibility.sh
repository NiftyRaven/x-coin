#!/usr/bin/env bash
# Eligibility: unlinked node is not in the lottery.
# Signed-in but not X Verified (no blue check) can send/receive, not lottery.
# X Verified is GET /2/users/me verified — not addxverified.
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
  mkdir -p "$DATADIR"
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
      sleep 0.3
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
# Still unlinked: invite list alone is not enough, and is not X Verified.
INFO2="$("${CLI[@]}" getlotteryinfo)"
echo "$INFO2" | python3 -c '
import json,sys
j=json.load(sys.stdin)
if j.get("local_eligible") is not False:
    sys.exit("invite list without a Sign in with X session must stay ineligible")
if j.get("allowlist_accounts", j.get("verified_accounts", 0)) < 1:
    sys.exit("addxverified did not stick")
'
stop_node
mkdir -p "$DATADIR/regtest"
rm -f "$DATADIR/regtest/verified-x-accounts.txt"
sleep 0.5

echo "== signed-in but not X Verified (no blue check) is not lottery-eligible =="
start_node -xoauthmock=ghost:unverified
INFO3="$("${CLI[@]}" getlotteryinfo)"
echo "$INFO3"
echo "$INFO3" | python3 -c '
import json,sys
j=json.load(sys.stdin)
if j.get("local_xaccount") != "ghost":
    sys.exit("expected local_xaccount=ghost")
if j.get("local_eligible") is not False:
    sys.exit("signed-in unverified must be ineligible")
if j.get("local_x_verified") is True:
    sys.exit("ghost:unverified must not report local_x_verified")
'
SESS="$("${CLI[@]}" getxsession)"
echo "$SESS"
echo "$SESS" | python3 -c '
import json,sys
j=json.load(sys.stdin)
if j.get("signed_in") is not True:
    sys.exit("expected signed_in")
if j.get("verified") is True or j.get("x_verified") is True:
    sys.exit("ghost:unverified must not be X Verified")
'
if "${CLI[@]}" registeractivenode >/dev/null 2>&1; then
  echo "registeractivenode must fail when session is not X Verified" >&2
  exit 1
fi

echo "== signed-in but not X Verified can still receive =="
RECV="$("${CLI[@]}" getnewaddress)"
[[ "$RECV" == y* && "$RECV" != R* && "$RECV" != n* ]] || { echo "FAIL: signed-in ghost must be able to receive, got $RECV" >&2; exit 1; }
echo "  receive $RECV (session proves it is you; lottery still closed)"

echo "== invite list without a blue check still cannot lottery =="
"${CLI[@]}" addxverified ghost >/dev/null
INFO4a="$("${CLI[@]}" getlotteryinfo)"
echo "$INFO4a" | python3 -c '
import json,sys
j=json.load(sys.stdin)
if j.get("local_eligible") is not False:
    sys.exit("allowlist without users/me.verified must not be eligible (invite list is not X Verified)")
'

echo "== mock X Verified (blue check) makes the linked node eligible =="
"${CLI[@]}" mockxsignin ghost:verified >/dev/null
INFO4="$("${CLI[@]}" getlotteryinfo)"
echo "$INFO4"
echo "$INFO4" | python3 -c '
import json,sys
j=json.load(sys.stdin)
if j.get("local_x_verified") is not True:
    sys.exit("ghost:verified must report local_x_verified")
if j.get("local_eligible") is not True:
    sys.exit("signed-in X Verified must be eligible (invite list is not a lottery gate)")
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
  echo "foreign identity must be rejected" >&2
  exit 1
fi
"${CLI[@]}" mockxsignin botter:verified >/dev/null
"${CLI[@]}" registeractivenode "$ADDR" botter >/dev/null
# Restore the local session so HeartbeatLocal does not remap @botter
# onto the local payout script (one handle → one node id).
"${CLI[@]}" mockxsignin ghost:verified >/dev/null
COUNT="$("${CLI[@]}" getactivenodes | python3 -c 'import json,sys; print(len(json.load(sys.stdin)))')"
[[ "$COUNT" -ge 2 ]]

echo "== compact NFTRVN mock defaults to X Verified =="
"${CLI[@]}" mockxsignin NFTRVN >/dev/null
SESSN="$("${CLI[@]}" getxsession)"
echo "$SESSN" | python3 -c '
import json,sys
j=json.load(sys.stdin)
if j.get("username") != "nftrvn":
    sys.exit("expected nftrvn")
if j.get("x_verified") is not True:
    sys.exit("NFTRVN mock must default to X Verified (blue check)")
'
INFO_N="$("${CLI[@]}" getlotteryinfo)"
echo "$INFO_N" | python3 -c '
import json,sys
j=json.load(sys.stdin)
if j.get("local_eligible") is not True:
    sys.exit("NFTRVN mock (verified=true) must be lottery-eligible")
'
# Restore ghost so the node stays two-handle consistent if anything else runs.
"${CLI[@]}" mockxsignin ghost:verified >/dev/null

echo "smoke-eligibility: ok"
