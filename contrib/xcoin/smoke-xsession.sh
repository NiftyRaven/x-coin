#!/usr/bin/env bash
# Typed foreign handles cannot provision a root. Only a Sign in with X
# session (regtest: mock GET /2/users/me) can.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
XCOIND="${XCOIND:-$ROOT/src/xcoind}"
XCLI="${XCLI:-$ROOT/src/xcoin-cli}"
DATADIR="${DATADIR:-$ROOT/smoke-xsession-datadir}"
CLI=("$XCLI" -regtest -datadir="$DATADIR")

if [[ ! -x "$XCOIND" ]]; then
  echo "missing $XCOIND — build first" >&2
  exit 1
fi

rm -rf "$DATADIR"
mkdir -p "$DATADIR"

"$XCOIND" -regtest -datadir="$DATADIR" -server -daemon -listen=0 \
  -xaccount=nftrvn -xverified=nftrvn -xverified=alice
cleanup() {
  "${CLI[@]}" stop >/dev/null 2>&1 || true
  for _ in $(seq 1 50); do
    if ! "${CLI[@]}" getlotteryinfo >/dev/null 2>&1; then
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

echo "== typed -xaccount is ignored without a session =="
INFO="$("${CLI[@]}" getlotteryinfo)"
echo "$INFO"
echo "$INFO" | python3 -c '
import json,sys
j=json.load(sys.stdin)
if j.get("local_xaccount"):
    sys.exit("typed -xaccount must not become lottery identity")
if j.get("local_eligible") is not False:
    sys.exit("no session must be ineligible")
'
SESS="$("${CLI[@]}" getxsession)"
echo "$SESS"
echo "$SESS" | python3 -c '
import json,sys
j=json.load(sys.stdin)
if j.get("signed_in") is not False:
    sys.exit("expected signed_in=false")
'

echo "== send/receive require a session =="
if "${CLI[@]}" getnewaddress >/tmp/xcoin-xsession-recv.err 2>&1; then
  echo "getnewaddress must fail without a session" >&2
  cat /tmp/xcoin-xsession-recv.err >&2
  exit 1
fi
grep -qi "sign in with x\|session" /tmp/xcoin-xsession-recv.err
if "${CLI[@]}" sendtoaddress ySmokeNoSession111111111111111111 1 >/tmp/xcoin-xsession-send.err 2>&1; then
  echo "sendtoaddress must fail without a session" >&2
  cat /tmp/xcoin-xsession-send.err >&2
  exit 1
fi
grep -qi "sign in with x\|session" /tmp/xcoin-xsession-send.err
if "${CLI[@]}" getaccountaddress "" >/tmp/xcoin-xsession-acct.err 2>&1; then
  echo "getaccountaddress must fail without a session" >&2
  cat /tmp/xcoin-xsession-acct.err >&2
  exit 1
fi
grep -qi "sign in with x\|session" /tmp/xcoin-xsession-acct.err
if "${CLI[@]}" transfer ALICE 1 ySmokeNoSession111111111111111111 >/tmp/xcoin-xsession-xfer.err 2>&1; then
  echo "transfer must fail without a session" >&2
  cat /tmp/xcoin-xsession-xfer.err >&2
  exit 1
fi
grep -qi "sign in with x\|session" /tmp/xcoin-xsession-xfer.err

echo "== typed linkxaccount nftrvn is rejected without session =="
if "${CLI[@]}" linkxaccount nftrvn >/tmp/xcoin-xsession-link.err 2>&1; then
  echo "linkxaccount nftrvn must fail without a session" >&2
  cat /tmp/xcoin-xsession-link.err >&2
  exit 1
fi
grep -qi "sign in with x\|session\|typed" /tmp/xcoin-xsession-link.err

echo "== mock users/me alice; foreign handle still rejected =="
MOCK="$("${CLI[@]}" mockxsignin '{"data":{"id":"99","username":"alice"}}')"
echo "$MOCK"
echo "$MOCK" | python3 -c '
import json,sys
j=json.load(sys.stdin)
if j.get("username") != "alice":
    sys.exit("mock must bind username alice, got %r" % j.get("username"))
if str(j.get("id")) != "99":
    sys.exit("mock must bind user id 99, got %r" % j.get("id"))
'
SESS2="$("${CLI[@]}" getxsession)"
echo "$SESS2"
echo "$SESS2" | python3 -c '
import json,sys
j=json.load(sys.stdin)
if j.get("signed_in") is not True:
    sys.exit("expected signed_in")
if j.get("username") != "alice":
    sys.exit("session username must be alice")
if str(j.get("user_id") or j.get("id")) != "99":
    sys.exit("session must store X user id")
'
RECV="$("${CLI[@]}" getnewaddress)"
echo "session receive $RECV"
[[ "$RECV" == y* ]]

if "${CLI[@]}" linkxaccount nftrvn >/tmp/xcoin-xsession-imp.err 2>&1; then
  echo "linkxaccount nftrvn must fail when signed in as alice" >&2
  cat /tmp/xcoin-xsession-imp.err >&2
  exit 1
fi
grep -qi "impersonation\|cannot use\|signed in as" /tmp/xcoin-xsession-imp.err

echo "== session handle alice can provision the root =="
ADDR="$("${CLI[@]}" getnewaddress)"
# Identity root burn is 0 but the assignment tx still pays a relay fee.
"${CLI[@]}" generatetoaddress 110 "$ADDR" >/dev/null
LINK="$("${CLI[@]}" linkxaccount alice "$ADDR")"
echo "$LINK"
echo "$LINK" | python3 -c '
import json,sys
j=json.load(sys.stdin)
if j.get("xaccount") != "alice":
    sys.exit("expected xaccount=alice, got %r" % j.get("xaccount"))
if j.get("asset") != "ALICE":
    sys.exit("expected root ALICE, got %r" % j.get("asset"))
'
"${CLI[@]}" listmyassets | grep -q ALICE
"${CLI[@]}" getmainasset alice | grep -q ALICE

INFO2="$("${CLI[@]}" getlotteryinfo)"
echo "$INFO2"
echo "$INFO2" | python3 -c '
import json,sys
j=json.load(sys.stdin)
if j.get("local_xaccount") != "alice":
    sys.exit("lottery identity must come from the session")
if j.get("local_eligible") is not True:
    sys.exit("signed-in allowlisted alice must be eligible")
'

echo "smoke-xsession: ok"
