#!/usr/bin/env bash
# Harder anti-abuse / lottery-eligibility / xsession cases on throwaway
# -regtest datadirs. Complements smoke-eligibility, smoke-xsession,
# smoke-sabotage, and smoke-isolation.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
XCOIND="${XCOIND:-$ROOT/src/xcoind}"
XCLI="${XCLI:-$ROOT/src/xcoin-cli}"
BASE="${DATADIR:-$ROOT/smoke-abuse-datadir}"

if [[ ! -x "$XCOIND" || ! -x "$XCLI" ]]; then
  echo "missing $XCOIND or $XCLI" >&2
  exit 1
fi

rm -rf "$BASE"
mkdir -p "$BASE"

wait_rpc() {
  local -n _cli=$1
  for _ in $(seq 1 120); do
    if "${_cli[@]}" getblockchaininfo >/dev/null 2>&1; then
      return 0
    fi
    sleep 0.2
  done
  echo "node did not become ready" >&2
  return 1
}

stop_cli() {
  local -n _cli=$1
  "${_cli[@]}" stop >/dev/null 2>&1 || true
  for _ in $(seq 1 50); do
    if ! "${_cli[@]}" getblockchaininfo >/dev/null 2>&1; then
      sleep 0.2
      return 0
    fi
    sleep 0.2
  done
}

# ---------------------------------------------------------------------------
# Phase 1: bot / unverified cannot lottery; can still receive
# ---------------------------------------------------------------------------
echo "== bot / unverified account cannot lottery =="
BOT_DIR="$BASE/bot"
mkdir -p "$BOT_DIR"
BOT_CLI=("$XCLI" -regtest -datadir="$BOT_DIR" -rpcport=28342)
"$XCOIND" -regtest -datadir="$BOT_DIR" -server -daemon -listen=0 -port=28343 \
  -rpcport=28342 -connect=0 -dnsseed=0 \
  -xoauthmock=botter:unverified
wait_rpc BOT_CLI
INFO="$("${BOT_CLI[@]}" getlotteryinfo)"
echo "$INFO"
echo "$INFO" | python3 -c '
import json,sys
j=json.load(sys.stdin)
if j.get("local_xaccount") != "botter":
    sys.exit("expected local_xaccount=botter")
if j.get("local_eligible") is not False:
    sys.exit("bot / unverified must have zero lottery chance")
if j.get("local_x_verified") is True:
    sys.exit("botter:unverified must not report local_x_verified")
'
SESS="$("${BOT_CLI[@]}" getxsession)"
echo "$SESS" | python3 -c '
import json,sys
j=json.load(sys.stdin)
if j.get("signed_in") is not True:
    sys.exit("bot is signed in (session exists)")
if j.get("verified") is True or j.get("x_verified") is True:
    sys.exit("bot must not be X Verified")
'
if "${BOT_CLI[@]}" registeractivenode >/tmp/xcoin-abuse-bot-reg.err 2>&1; then
  echo "registeractivenode must fail for unverified bot" >&2
  cat /tmp/xcoin-abuse-bot-reg.err >&2
  exit 1
fi
grep -qiE "verified|sign in|session|lottery" /tmp/xcoin-abuse-bot-reg.err
BOT_ADDR="$("${BOT_CLI[@]}" getnewaddress)"
[[ "$BOT_ADDR" == y* ]]
echo "  bot can receive $BOT_ADDR (lottery still closed)"
stop_cli BOT_CLI

# ---------------------------------------------------------------------------
# Phase 2: typed handle cannot claim Verified X or someone else's root
# ---------------------------------------------------------------------------
echo "== typed handle cannot claim Verified X or someone else's root =="
TYP_DIR="$BASE/typed"
mkdir -p "$TYP_DIR"
TYP_CLI=("$XCLI" -regtest -datadir="$TYP_DIR" -rpcport=28342)
"$XCOIND" -regtest -datadir="$TYP_DIR" -server -daemon -listen=0 -port=28343 \
  -rpcport=28342 -connect=0 -dnsseed=0 \
  -xaccount=nftrvn -xverified=nftrvn -xverified=alice \
  -xoauthmock=alice:verified
wait_rpc TYP_CLI
ADDR_T="$("${TYP_CLI[@]}" getnewaddress)"
"${TYP_CLI[@]}" generatetoaddress 110 "$ADDR_T" >/dev/null
if "${TYP_CLI[@]}" linkxaccount NFTRVN "$ADDR_T" >/tmp/xcoin-abuse-typed.err 2>&1; then
  echo "linkxaccount NFTRVN must fail while signed in as alice" >&2
  cat /tmp/xcoin-abuse-typed.err >&2
  exit 1
fi
grep -qiE "impersonation|cannot use|signed in as" /tmp/xcoin-abuse-typed.err
if "${TYP_CLI[@]}" registeractivenode "$ADDR_T" nftrvn >/tmp/xcoin-abuse-typed-reg.err 2>&1; then
  echo "registeractivenode nftrvn must fail while signed in as alice" >&2
  cat /tmp/xcoin-abuse-typed-reg.err >&2
  exit 1
fi
grep -qiE "impersonation|cannot use|signed in as" /tmp/xcoin-abuse-typed-reg.err
LINK_OK="$("${TYP_CLI[@]}" linkxaccount alice "$ADDR_T")"
echo "$LINK_OK" | python3 -c '
import json,sys
j=json.load(sys.stdin)
if j.get("xaccount") != "alice" or j.get("asset") != "ALICE":
    sys.exit("alice session must claim ALICE, got %r" % j)
'
stop_cli TYP_CLI

# ---------------------------------------------------------------------------
# Phase 3: operator invite list cannot exclude a verified wallet
# ---------------------------------------------------------------------------
echo "== operator invite list cannot exclude a verified wallet =="
INV_DIR="$BASE/invite"
mkdir -p "$INV_DIR"
ALLOW="$BASE/invite-only-alice.txt"
echo "alice" > "$ALLOW"
INV_CLI=("$XCLI" -regtest -datadir="$INV_DIR" -rpcport=28342)
"$XCOIND" -regtest -datadir="$INV_DIR" -server -daemon -listen=0 -port=28343 \
  -rpcport=28342 -connect=0 -dnsseed=0 \
  -xallowlist="$ALLOW" -xoauthmock=zed:verified
wait_rpc INV_CLI
INFO_Z="$("${INV_CLI[@]}" getlotteryinfo)"
echo "$INFO_Z"
echo "$INFO_Z" | python3 -c '
import json,sys
j=json.load(sys.stdin)
if j.get("local_xaccount") != "zed":
    sys.exit("expected local_xaccount=zed")
if j.get("local_x_verified") is not True:
    sys.exit("zed:verified must report local_x_verified")
if j.get("local_eligible") is not True:
    sys.exit("invite list must not exclude a verified running wallet")
'
REG_Z="$("${INV_CLI[@]}" registeractivenode)"
echo "$REG_Z" | python3 -c '
import json,sys
j=json.load(sys.stdin)
if j.get("xaccount") != "zed":
    sys.exit("zed heartbeat must succeed off the invite list")
'
stop_cli INV_CLI

# ---------------------------------------------------------------------------
# Phase 4: session file / mock mismatch is not signed in; send/receive still work
# ---------------------------------------------------------------------------
echo "== tampered xsession.json (username rewrite, old proof) is not signed in =="
SES_DIR="$BASE/session"
mkdir -p "$SES_DIR"
SES_CLI=("$XCLI" -regtest -datadir="$SES_DIR" -rpcport=28342)
"$XCOIND" -regtest -datadir="$SES_DIR" -server -daemon -listen=0 -port=28343 \
  -rpcport=28342 -connect=0 -dnsseed=0 \
  -xoauthmock=alice:verified
wait_rpc SES_CLI
BEFORE="$("${SES_CLI[@]}" getnewaddress)"
[[ "$BEFORE" == y* ]]
SESS_PATH="$("${SES_CLI[@]}" getxsession | python3 -c 'import json,sys; print(json.load(sys.stdin)["session_file"])')"
python3 - "$SESS_PATH" <<'PY'
import json, sys
path = sys.argv[1]
obj = json.load(open(path, encoding="utf-8"))
obj["username"] = "eve"
json.dump(obj, open(path, "w", encoding="utf-8"))
print("rewrote username to eve, kept alice proof")
PY
AFTER="$("${SES_CLI[@]}" getnewaddress)"
[[ "$AFTER" == y* ]]
if "${SES_CLI[@]}" linkxaccount eve >/tmp/xcoin-abuse-tamper-link.err 2>&1; then
  echo "linkxaccount must fail after xsession.json username/proof mismatch" >&2
  cat /tmp/xcoin-abuse-tamper-link.err >&2
  exit 1
fi
grep -qiE "sign in with x|session" /tmp/xcoin-abuse-tamper-link.err
if "${SES_CLI[@]}" sendtoaddress "$AFTER" 1 >/tmp/xcoin-abuse-tamper-send.err 2>&1; then
  echo "sendtoaddress 1 XFER must fail (no coins) after tamper, not succeed" >&2
  cat /tmp/xcoin-abuse-tamper-send.err >&2
  exit 1
fi
if grep -qiE "sign in with x required|required to send" /tmp/xcoin-abuse-tamper-send.err; then
  echo "sendtoaddress must not require Sign in with X after tamper" >&2
  cat /tmp/xcoin-abuse-tamper-send.err >&2
  exit 1
fi
TAMPER_SESS="$("${SES_CLI[@]}" getxsession)"
echo "$TAMPER_SESS" | python3 -c '
import json,sys
j=json.load(sys.stdin)
if j.get("signed_in") is True:
    sys.exit("tampered proof must not report signed_in")
'

echo "== foreign xsession.json on a datadir with a different xsession.key is not signed in =="
# Restore a valid alice session, snapshot the json, then sign in as bob
# (new proof under the same key). Dropping alice's json back is a
# mockxsignin / file mismatch: proof no longer matches current secret+fields
# wait — same key, alice json should still verify if we restore alice's json.
# The foreign-key case needs a second datadir.
stop_cli SES_CLI

KEY_A="$BASE/key-alice"
KEY_B="$BASE/key-bob"
mkdir -p "$KEY_A" "$KEY_B"
AKEY_CLI=("$XCLI" -regtest -datadir="$KEY_A" -rpcport=28342)
BKEY_CLI=("$XCLI" -regtest -datadir="$KEY_B" -rpcport=28352)
"$XCOIND" -regtest -datadir="$KEY_A" -server -daemon -listen=0 -port=28343 \
  -rpcport=28342 -connect=0 -dnsseed=0 -xoauthmock=alice:verified
"$XCOIND" -regtest -datadir="$KEY_B" -server -daemon -listen=0 -port=28353 \
  -rpcport=28352 -connect=0 -dnsseed=0 -xoauthmock=bob:verified
wait_rpc AKEY_CLI
wait_rpc BKEY_CLI
A_JSON="$KEY_A/regtest/xsession.json"
B_JSON="$KEY_B/regtest/xsession.json"
test -f "$A_JSON" && test -f "$B_JSON"
# Copy Alice's session file onto Bob's datadir (Bob's HMAC key stays).
cp "$A_JSON" "$B_JSON"
BOB_RECV="$("${BKEY_CLI[@]}" getnewaddress)"
[[ "$BOB_RECV" == y* ]]
if "${BKEY_CLI[@]}" linkxaccount alice >/tmp/xcoin-abuse-foreign-sess.err 2>&1; then
  echo "bob datadir must not accept alice xsession.json (different xsession.key)" >&2
  cat /tmp/xcoin-abuse-foreign-sess.err >&2
  exit 1
fi
grep -qiE "sign in with x|session" /tmp/xcoin-abuse-foreign-sess.err
echo "$("${BKEY_CLI[@]}" getxsession)" | python3 -c '
import json,sys
j=json.load(sys.stdin)
if j.get("signed_in") is True:
    sys.exit("foreign xsession.json must not report signed_in")
'
# mockxsignin on Bob restores a matching session.
"${BKEY_CLI[@]}" mockxsignin bob:verified >/dev/null
echo "$("${BKEY_CLI[@]}" getxsession)" | python3 -c '
import json,sys
j=json.load(sys.stdin)
if j.get("signed_in") is not True:
    sys.exit("mockxsignin bob must restore signed_in")
'
stop_cli AKEY_CLI
stop_cli BKEY_CLI

# ---------------------------------------------------------------------------
# Phase 5: two distinct verified handles on two wallets; send/receive works
# ---------------------------------------------------------------------------
echo "== two nodes, two different verified mocks, both eligible; send/receive =="
MV_BASE="$BASE/multiverified"
A_DIR="$MV_BASE/a"
B_DIR="$MV_BASE/b"
mkdir -p "$A_DIR" "$B_DIR"
A_CLI=("$XCLI" -regtest -datadir="$A_DIR" -rpcport=28342)
B_CLI=("$XCLI" -regtest -datadir="$B_DIR" -rpcport=28352)
"$XCOIND" -regtest -datadir="$A_DIR" -server -daemon -listen=1 -port=28343 \
  -rpcport=28342 -connect=0 -dnsseed=0 -txindex=1 \
  -xoauthmock=alice:verified
"$XCOIND" -regtest -datadir="$B_DIR" -server -daemon -listen=0 -port=28353 \
  -rpcport=28352 -addnode=127.0.0.1:28343 -dnsseed=0 \
  -xoauthmock=bob:verified
wait_rpc A_CLI
wait_rpc B_CLI
echo "$("${A_CLI[@]}" getlotteryinfo)" | python3 -c '
import json,sys
j=json.load(sys.stdin)
if j.get("local_xaccount") != "alice":
    sys.exit("alice node expected alice")
if j.get("local_eligible") is not True:
    sys.exit("verified alice must be eligible")
'
echo "$("${B_CLI[@]}" getlotteryinfo)" | python3 -c '
import json,sys
j=json.load(sys.stdin)
if j.get("local_xaccount") != "bob":
    sys.exit("bob node expected bob")
if j.get("local_eligible") is not True:
    sys.exit("verified bob must be eligible")
'
ADDR_A="$("${A_CLI[@]}" getnewaddress)"
ADDR_B="$("${B_CLI[@]}" getnewaddress)"
"${A_CLI[@]}" generatetoaddress 110 "$ADDR_A" >/dev/null
TX="$("${A_CLI[@]}" sendtoaddress "$ADDR_B" 7.25)"
echo "alice->bob tx $TX"
"${A_CLI[@]}" generatetoaddress 1 "$ADDR_A" >/dev/null
got=0
for _ in $(seq 1 50); do
  BAL="$("${B_CLI[@]}" getbalance)"
  if python3 -c "import sys; raise SystemExit(0 if float(sys.argv[1])>=7.2 else 1)" "$BAL"; then
    got=1
    echo "bob balance $BAL"
    break
  fi
  sleep 0.25
done
[[ "$got" -eq 1 ]]
stop_cli A_CLI
stop_cli B_CLI

# ---------------------------------------------------------------------------
# Phase 6: same verified handle on two wallets — live binding rejects spoof
# ---------------------------------------------------------------------------
echo "== same verified handle on two wallets: observer keeps one live alice =="
DUP_BASE="$BASE/dup-alice"
C_DIR="$DUP_BASE/carol"
A_DIR="$DUP_BASE/alice-a"
B_DIR="$DUP_BASE/alice-b"
mkdir -p "$C_DIR" "$A_DIR" "$B_DIR"
C_CLI=("$XCLI" -regtest -datadir="$C_DIR" -rpcport=28342)
A_CLI=("$XCLI" -regtest -datadir="$A_DIR" -rpcport=28352)
B_CLI=("$XCLI" -regtest -datadir="$B_DIR" -rpcport=28362)
"$XCOIND" -regtest -datadir="$C_DIR" -server -daemon -listen=1 -port=28343 \
  -rpcport=28342 -connect=0 -dnsseed=0 \
  -xoauthmock=carol:verified
wait_rpc C_CLI
"$XCOIND" -regtest -datadir="$A_DIR" -server -daemon -listen=1 -port=28353 \
  -rpcport=28352 -addnode=127.0.0.1:28343 -dnsseed=0 \
  -xoauthmock=alice:verified
wait_rpc A_CLI
# Wait until Carol sees the first live @alice.
saw_alice=0
for _ in $(seq 1 40); do
  NODES="$("${C_CLI[@]}" getactivenodes)"
  if echo "$NODES" | python3 -c '
import json,sys
n=json.load(sys.stdin)
print("carol sees", [x.get("xaccount") for x in n], file=sys.stderr)
sys.exit(0 if any(x.get("xaccount")=="alice" for x in n) else 1)
'; then
    saw_alice=1
    break
  fi
  sleep 0.25
done
if [[ "$saw_alice" -ne 1 ]]; then
  echo "carol never saw first alice heartbeat" >&2
  "${C_CLI[@]}" getactivenodes >&2 || true
  exit 1
fi
ALICE_SCRIPT="$("${C_CLI[@]}" getactivenodes | python3 -c '
import json,sys
n=json.load(sys.stdin)
al=[x for x in n if x.get("xaccount")=="alice"]
assert len(al)==1, al
print(al[0]["script"])
')"
# Second wallet also signed in as alice (duplicate session / spoof).
"$XCOIND" -regtest -datadir="$B_DIR" -server -daemon -listen=0 -port=28363 \
  -rpcport=28362 -addnode=127.0.0.1:28343 -dnsseed=0 \
  -xoauthmock=alice:verified
wait_rpc B_CLI
echo "$("${B_CLI[@]}" getlotteryinfo)" | python3 -c '
import json,sys
j=json.load(sys.stdin)
if j.get("local_xaccount") != "alice":
    sys.exit("spoof wallet still has a local alice session (HMAC is per-datadir)")
if j.get("local_eligible") is not True:
    sys.exit("local session on the second wallet is still X Verified")
'
# Give the second heartbeat time to arrive; Carol must keep the first script.
sleep 2
for _ in $(seq 1 20); do
  "${C_CLI[@]}" getactivenodes | python3 -c '
import json,sys
n=json.load(sys.stdin)
al=[x for x in n if x.get("xaccount")=="alice"]
if len(al) != 1:
    sys.exit("observer must not list two live alice nodes, got %s" % al)
' && break
  sleep 0.25
done
NODES_C="$("${C_CLI[@]}" getactivenodes)"
echo "$NODES_C"
echo "$NODES_C" | python3 -c "
import json,sys
n=json.load(sys.stdin)
al=[x for x in n if x.get('xaccount')=='alice']
if len(al) != 1:
    sys.exit('duplicate @alice must be rejected on the observer, got %s' % al)
if al[0].get('script') != '''$ALICE_SCRIPT''':
    sys.exit('live @alice rebound to the spoof wallet')
print('observer kept first live alice; second wallet did not steal the handle')
"
# Document: each datadir can hold its own HMAC session for the same handle.
# Network lottery identity is one live handle → one payout script (TTL 180s).
stop_cli B_CLI
stop_cli A_CLI
stop_cli C_CLI

echo "smoke-abuse: ok"
