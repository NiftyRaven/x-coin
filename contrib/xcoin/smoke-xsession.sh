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
if j.get("linked") is not False:
    sys.exit("expected linked=false")
if "xsession.json" not in str(j.get("session_file","")):
    sys.exit("getxsession must name xsession.json")
if "xsession.key" not in str(j.get("secret_file","")):
    sys.exit("getxsession must name xsession.key")
'

echo "== send/receive work without a session =="
RECV="$("${CLI[@]}" getnewaddress)"
echo "unsigned receive $RECV"
[[ "$RECV" == y* ]]
ACCT="$("${CLI[@]}" getaccountaddress "")"
echo "unsigned account $ACCT"
[[ "$ACCT" == y* ]]
if "${CLI[@]}" sendtoaddress "$RECV" 1 >/tmp/xcoin-xsession-send.err 2>&1; then
  echo "sendtoaddress 1 XFER must fail (no coins) without a session" >&2
  cat /tmp/xcoin-xsession-send.err >&2
  exit 1
fi
if grep -qiE "sign in with x required|required to send" /tmp/xcoin-xsession-send.err; then
  echo "sendtoaddress must not require Sign in with X" >&2
  cat /tmp/xcoin-xsession-send.err >&2
  exit 1
fi
if "${CLI[@]}" transfer ALICE 1 "$RECV" >/tmp/xcoin-xsession-xfer.err 2>&1; then
  echo "transfer ALICE must fail (no such asset) without a session" >&2
  cat /tmp/xcoin-xsession-xfer.err >&2
  exit 1
fi
if grep -qiE "sign in with x required|required to send" /tmp/xcoin-xsession-xfer.err; then
  echo "transfer must not require Sign in with X" >&2
  cat /tmp/xcoin-xsession-xfer.err >&2
  exit 1
fi

echo "== unsigned cannot create a main asset =="
if "${CLI[@]}" issue TESTASSET 1 >/tmp/xcoin-xsession-issue.err 2>&1; then
  echo "issue TESTASSET must fail without a session" >&2
  cat /tmp/xcoin-xsession-issue.err >&2
  exit 1
fi
grep -qi "sign in with x\|session\|cannot create main" /tmp/xcoin-xsession-issue.err
if "${CLI[@]}" linkxaccount >/tmp/xcoin-xsession-linkempty.err 2>&1; then
  echo "linkxaccount with no session must fail" >&2
  cat /tmp/xcoin-xsession-linkempty.err >&2
  exit 1
fi
grep -qi "sign in with x\|session" /tmp/xcoin-xsession-linkempty.err

if "${CLI[@]}" linkxaccount nftrvn >/tmp/xcoin-xsession-link.err 2>&1; then
  echo "linkxaccount nftrvn must fail without a session" >&2
  cat /tmp/xcoin-xsession-link.err >&2
  exit 1
fi
grep -qi "sign in with x\|session\|typed" /tmp/xcoin-xsession-link.err

echo "== mock users/me alice (unverified); foreign handle still rejected =="
MOCK="$("${CLI[@]}" mockxsignin '{"data":{"id":"99","username":"alice","verified":false}}')"
echo "$MOCK"
echo "$MOCK" | python3 -c '
import json,sys
j=json.load(sys.stdin)
if j.get("username") != "alice":
    sys.exit("mock must bind username alice, got %r" % j.get("username"))
if str(j.get("id")) != "99":
    sys.exit("mock must bind user id 99, got %r" % j.get("id"))
if j.get("x_verified") is True or j.get("verified") is True:
    sys.exit("alice JSON without blue check must not be X Verified")
'
SESS2="$("${CLI[@]}" getxsession)"
echo "$SESS2"
echo "$SESS2" | python3 -c '
import json,sys
j=json.load(sys.stdin)
if j.get("signed_in") is not True:
    sys.exit("expected signed_in")
if j.get("linked") is not True:
    sys.exit("signed-in session must report linked")
if j.get("username") != "alice":
    sys.exit("session username must be alice")
if str(j.get("user_id") or j.get("id")) != "99":
    sys.exit("session must store X user id")
if j.get("verified") is True or j.get("x_verified") is True:
    sys.exit("unverified alice session must not report x_verified")
if "xsession.json" not in str(j.get("session_file","")):
    sys.exit("linked session must name xsession.json")
if "xsession.key" not in str(j.get("secret_file","")):
    sys.exit("linked session must name xsession.key")
'
RECV="$("${CLI[@]}" getnewaddress)"
echo "session receive $RECV"
[[ "$RECV" == y* ]]

echo "== signed-in but no claimed root cannot issue a sub =="
if "${CLI[@]}" issue ALICE/NOTE 1 >/tmp/xcoin-xsession-subbefore.err 2>&1; then
  echo "issue ALICE/NOTE must fail before linkxaccount" >&2
  cat /tmp/xcoin-xsession-subbefore.err >&2
  exit 1
fi
grep -qi "claim your main\|linkxaccount\|main asset" /tmp/xcoin-xsession-subbefore.err

if "${CLI[@]}" linkxaccount nftrvn >/tmp/xcoin-xsession-imp.err 2>&1; then
  echo "linkxaccount nftrvn must fail when signed in as alice" >&2
  cat /tmp/xcoin-xsession-imp.err >&2
  exit 1
fi
grep -qi "impersonation\|cannot use\|signed in as" /tmp/xcoin-xsession-imp.err

INFO2="$("${CLI[@]}" getlotteryinfo)"
echo "$INFO2"
echo "$INFO2" | python3 -c '
import json,sys
j=json.load(sys.stdin)
if j.get("local_xaccount") != "alice":
    sys.exit("lottery identity must come from the session")
if j.get("local_eligible") is not False:
    sys.exit("signed-in unverified alice must not be lottery-eligible")
if j.get("local_x_verified") is True:
    sys.exit("unverified alice must not report local_x_verified")
'

echo "== mock X Verified alice is lottery-eligible =="
"${CLI[@]}" mockxsignin '{"data":{"id":"99","username":"alice","verified":true,"verified_type":"blue"}}' >/dev/null
INFO3="$("${CLI[@]}" getlotteryinfo)"
echo "$INFO3"
echo "$INFO3" | python3 -c '
import json,sys
j=json.load(sys.stdin)
if j.get("local_xaccount") != "alice":
    sys.exit("lottery identity must stay alice")
if j.get("local_x_verified") is not True:
    sys.exit("verified alice must report local_x_verified")
if j.get("local_eligible") is not True:
    sys.exit("signed-in X Verified alice must be eligible; invite list is not a gate")
'

echo "== wall-clock exp must not sign out a live process =="
# mockxsignin writes exp=GetTime()+365d. Jump mocktime past that.
# 1.0.10 LoadSession still did GetTime()>exp and signed the user out.
"${CLI[@]}" setmocktime 1000000 >/dev/null
"${CLI[@]}" mockxsignin '{"data":{"id":"99","username":"alice","verified":true,"verified_type":"blue"}}' >/dev/null
"${CLI[@]}" setmocktime $((1000000 + 86400 * 365 + 60)) >/dev/null
SESS_LIVE="$("${CLI[@]}" getxsession)"
echo "$SESS_LIVE"
echo "$SESS_LIVE" | python3 -c '
import json,sys
j=json.load(sys.stdin)
if j.get("signed_in") is not True or j.get("username") != "alice":
    sys.exit("live process must stay signed in after exp elapses (got %r)" % j)
if "expired" in str(j.get("error", "")).lower():
    sys.exit("getxsession must not report expired while the process is up")
'
INFO_LIVE="$("${CLI[@]}" getlotteryinfo)"
echo "$INFO_LIVE"
echo "$INFO_LIVE" | python3 -c '
import json,sys
j=json.load(sys.stdin)
if j.get("local_xaccount") != "alice":
    sys.exit("lottery identity must survive mocktime past exp")
if j.get("local_x_verified") is not True:
    sys.exit("X Verified must survive mocktime past exp")
'
"${CLI[@]}" setmocktime 0 >/dev/null

echo "== session handle alice can provision the root =="
ADDR="$("${CLI[@]}" getnewaddress)"
# Identity root burn is 0 but the assignment tx still pays a relay fee.
# generatetoaddress needs an X Verified local heartbeat so the coinbase
# can commit a non-empty active set.
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
# Claim is a mempool tx until the next lottery block.
"${CLI[@]}" generatetoaddress 1 "$ADDR" >/dev/null
MY="$("${CLI[@]}" listmyassets)"
echo "$MY"
echo "$MY" | grep -q ALICE
"${CLI[@]}" getmainasset alice | grep -q ALICE

echo "== headless xcoind keeps xsession.json across stop =="
SESS_PATH="$DATADIR/regtest/xsession.json"
test -f "$SESS_PATH"
"${CLI[@]}" stop >/dev/null
for _ in $(seq 1 50); do
  if ! "${CLI[@]}" getlotteryinfo >/dev/null 2>&1; then
    break
  fi
  sleep 0.2
done
test -f "$SESS_PATH"
"$XCOIND" -regtest -datadir="$DATADIR" -server -daemon -listen=0
up=0
for _ in $(seq 1 100); do
  if "${CLI[@]}" getlotteryinfo >/dev/null 2>&1; then
    up=1
    break
  fi
  sleep 0.2
done
if [[ "$up" -ne 1 ]]; then
  echo "xcoind did not become ready after restart" >&2
  tail -20 "$DATADIR/regtest/debug.log" >&2 || true
  exit 1
fi
SESS3="$("${CLI[@]}" getxsession)"
echo "$SESS3"
echo "$SESS3" | python3 -c '
import json,sys
j=json.load(sys.stdin)
if j.get("signed_in") is not True or j.get("username") != "alice":
    sys.exit("headless restart must keep the alice session")
'
# Session file is not the assignment. VerifyDB must not drop the
# confirmed handle → root (issue ALICE/NOTE uses getmainasset state).
MAIN="$("${CLI[@]}" getmainasset alice)"
echo "$MAIN"
echo "$MAIN" | python3 -c '
import json,sys
j=json.load(sys.stdin)
if j.get("assigned") is not True or j.get("asset") != "ALICE":
    sys.exit("headless restart must keep alice → ALICE (got %r)" % j)
'

echo "== subs only under this signed-in main asset =="
if "${CLI[@]}" issue OTHER/NOTE 1 >/tmp/xcoin-xsession-foreignsub.err 2>&1; then
  echo "issue OTHER/NOTE must fail (not alice's main)" >&2
  cat /tmp/xcoin-xsession-foreignsub.err >&2
  exit 1
fi
cat /tmp/xcoin-xsession-foreignsub.err
"${CLI[@]}" issue ALICE/NOTE 1
"${CLI[@]}" listmyassets | grep -q ALICE/NOTE

echo "smoke-xsession: ok"
