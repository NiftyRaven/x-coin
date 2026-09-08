#!/usr/bin/env bash
# Release-sabotage regressions: unsigned / spoofed identity must not
# send, receive, or steal lottery eligibility.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
XCOIND="${XCOIND:-$ROOT/src/xcoind}"
XCLI="${XCLI:-$ROOT/src/xcoin-cli}"
DATADIR="${DATADIR:-$ROOT/smoke-sabotage-datadir}"
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

cleanup() { stop_node; }
trap cleanup EXIT

echo "== unsigned node cannot send via sendrawtransaction or registeractivenode =="
start_node -xverified=alice:99 -xverified=eve
if "${CLI[@]}" sendrawtransaction 00 >/tmp/xcoin-sabotage-raw.err 2>&1; then
  echo "sendrawtransaction must fail without a session" >&2
  cat /tmp/xcoin-sabotage-raw.err >&2
  exit 1
fi
grep -qi "sign in with x\|session" /tmp/xcoin-sabotage-raw.err
if "${CLI[@]}" registeractivenode >/tmp/xcoin-sabotage-reg.err 2>&1; then
  echo "registeractivenode must fail without a session" >&2
  cat /tmp/xcoin-sabotage-reg.err >&2
  exit 1
fi
grep -qi "sign in with x\|session" /tmp/xcoin-sabotage-reg.err

echo "== listed userid alone cannot make an unlisted handle eligible =="
"${CLI[@]}" mockxsignin '{"data":{"id":"7","username":"eve","verified":true,"verified_type":"blue"}}' >/dev/null
EVE_ADDR="$("${CLI[@]}" getnewaddress)"
# eve is listed without a userid; alice is listed as 99. eve must not
# inherit alice's slot by passing alice's handle / userid.
if "${CLI[@]}" registeractivenode "$EVE_ADDR" alice 99 >/tmp/xcoin-sabotage-steal.err 2>&1; then
  echo "eve must not register as alice" >&2
  cat /tmp/xcoin-sabotage-steal.err >&2
  exit 1
fi
grep -qi "impersonation\|cannot use\|signed in as" /tmp/xcoin-sabotage-steal.err

echo "== wrong session userid is rejected =="
"${CLI[@]}" mockxsignin '{"data":{"id":"99","username":"alice","verified":true,"verified_type":"blue"}}' >/dev/null
ALICE_ADDR="$("${CLI[@]}" getnewaddress)"
if "${CLI[@]}" registeractivenode "$ALICE_ADDR" alice 100 >/tmp/xcoin-sabotage-uid.err 2>&1; then
  echo "mismatched xuserid must be rejected" >&2
  cat /tmp/xcoin-sabotage-uid.err >&2
  exit 1
fi
grep -qi "user id\|userid" /tmp/xcoin-sabotage-uid.err

REG="$("${CLI[@]}" registeractivenode "$ALICE_ADDR" alice 99)"
echo "$REG" | python3 -c '
import json,sys
j=json.load(sys.stdin)
if j.get("xaccount") != "alice":
    sys.exit("expected alice heartbeat")
if int(j.get("xuserid", 0)) != 99:
    sys.exit("expected userid 99")
'

echo "== live alice payout cannot be rebound by eve on this node =="
"${CLI[@]}" mockxsignin '{"data":{"id":"7","username":"eve","verified":true,"verified_type":"blue"}}' >/dev/null
# eve is allowlisted; she may heartbeat her own script, not alice's handle.
if "${CLI[@]}" registeractivenode "$EVE_ADDR" alice >/tmp/xcoin-sabotage-rebind.err 2>&1; then
  echo "eve must not rebind alice" >&2
  cat /tmp/xcoin-sabotage-rebind.err >&2
  exit 1
fi
"${CLI[@]}" registeractivenode "$EVE_ADDR" eve >/dev/null
NODES="$("${CLI[@]}" getactivenodes)"
echo "$NODES" | python3 -c '
import json,sys
n=json.load(sys.stdin)
handles=sorted(x.get("xaccount","") for x in n)
if handles != ["alice", "eve"]:
    sys.exit("expected alice+eve still distinct, got %s" % handles)
print("active handles", handles)
'

echo "== signed-in sendraw still requires a decodable tx (session gate already passed) =="
if "${CLI[@]}" sendrawtransaction 00 >/tmp/xcoin-sabotage-raw2.err 2>&1; then
  echo "junk hex must not be accepted" >&2
  exit 1
fi
grep -qi "decode\|deserialize" /tmp/xcoin-sabotage-raw2.err

echo "smoke-sabotage: ok"
