#!/usr/bin/env bash
# Fair lottery: X Verified (blue check) + running wallet cannot be excluded.
# Unverified sessions have zero chance even if allowlisted.
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
if j.get("local_x_verified") is not False:
    sys.exit("unlinked node must have local_x_verified=false")
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
rm -f "$DATADIR/regtest/verified-x-accounts.txt"
sleep 0.5

echo "== signed-in but not X Verified has zero chance (can still receive) =="
start_node -xoauthmock=ghost:unverified
INFO3="$("${CLI[@]}" getlotteryinfo)"
echo "$INFO3"
echo "$INFO3" | python3 -c '
import json,sys
j=json.load(sys.stdin)
if j.get("local_xaccount") != "ghost":
    sys.exit("expected local_xaccount=ghost")
if j.get("local_x_verified") is not False:
    sys.exit("ghost:unverified must report local_x_verified=false")
if j.get("local_eligible") is not False:
    sys.exit("unverified session must have zero lottery chance")
if j.get("active_nodes", 1) != 0:
    sys.exit("unverified node must not enter the active set")
'
SESSU="$("${CLI[@]}" getxsession)"
echo "$SESSU" | python3 -c '
import json,sys
j=json.load(sys.stdin)
if j.get("x_verified") is not False:
    sys.exit("getxsession.x_verified must be false for ghost:unverified")
'
RECV="$("${CLI[@]}" getnewaddress)"
[[ "$RECV" == y* && "$RECV" != R* && "$RECV" != n* ]] || { echo "FAIL: unverified ghost must be able to receive, got $RECV" >&2; exit 1; }
echo "  receive $RECV (session proves it is you; lottery closed)"
if "${CLI[@]}" registeractivenode >/dev/null 2>&1; then
  echo "registeractivenode must fail when the session is not X Verified" >&2
  exit 1
fi

echo "== JSON verified:false stays zero chance even if allowlisted =="
"${CLI[@]}" addxverified ghost >/dev/null
"${CLI[@]}" mockxsignin '{"data":{"id":"1","username":"ghost","verified":false}}' >/dev/null
INFO_JSON="$("${CLI[@]}" getlotteryinfo)"
echo "$INFO_JSON" | python3 -c '
import json,sys
j=json.load(sys.stdin)
if j.get("local_eligible") is not False:
    sys.exit("verified:false JSON must not be lottery eligible")
if j.get("local_x_verified") is not False:
    sys.exit("verified:false JSON must report local_x_verified=false")
if j.get("verified_accounts", 0) < 1:
    sys.exit("allowlist must still be populated")
'
if "${CLI[@]}" registeractivenode >/dev/null 2>&1; then
  echo "registeractivenode must fail for verified:false even when allowlisted" >&2
  exit 1
fi
stop_node
rm -f "$DATADIR/regtest/verified-x-accounts.txt"
sleep 0.5

echo "== X Verified + running wallet is eligible without addxverified =="
start_node -xoauthmock=ghost
INFO4="$("${CLI[@]}" getlotteryinfo)"
echo "$INFO4"
echo "$INFO4" | python3 -c '
import json,sys
j=json.load(sys.stdin)
if j.get("local_xaccount") != "ghost":
    sys.exit("expected local_xaccount=ghost")
if j.get("local_x_verified") is not True:
    sys.exit("compact ghost must default to X Verified")
if j.get("local_eligible") is not True:
    sys.exit("verified running wallet must be eligible without the invite list")
if j.get("active_nodes", 0) < 1:
    sys.exit("eligible local node must be in the active set")
if j.get("allowlist_accounts", 1) != 0:
    sys.exit("invite list must still be empty for this fair-entry check")
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
  echo "impersonation of a second identity must be rejected" >&2
  exit 1
fi
"${CLI[@]}" mockxsignin botter >/dev/null
"${CLI[@]}" registeractivenode "$ADDR" botter >/dev/null
# Restore the local session so HeartbeatLocal does not remap @botter
# onto the local payout script (one handle → one node id).
"${CLI[@]}" mockxsignin ghost >/dev/null
COUNT="$("${CLI[@]}" getactivenodes | python3 -c 'import json,sys; print(len(json.load(sys.stdin)))')"
[[ "$COUNT" -ge 2 ]]

echo "smoke-eligibility: ok"
