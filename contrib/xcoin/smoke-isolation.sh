#!/usr/bin/env bash
# Ownership isolation: Bob cannot spend Alice's coins or assets.
# Two wallet.dat files (two datadirs). Sign in with X does not import keys.
# Alice's session on Bob's empty wallet cannot spend Alice's UTXOs.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
XCOIND="${XCOIND:-$ROOT/src/xcoind}"
XCLI="${XCLI:-$ROOT/src/xcoin-cli}"
BASE="${DATADIR:-$ROOT/smoke-isolation-datadir}"
A_DIR="$BASE/alice"
B_DIR="$BASE/bob"
# Ports away from gui-shot (28582) and smoke-benchmark (28542).
A_CLI=("$XCLI" -regtest -datadir="$A_DIR" -rpcport=28642)
B_CLI=("$XCLI" -regtest -datadir="$B_DIR" -rpcport=28652)

if [[ ! -x "$XCOIND" ]]; then
  echo "missing $XCOIND — build first" >&2
  exit 1
fi

rm -rf "$BASE"
mkdir -p "$A_DIR" "$B_DIR"
ALLOW="$BASE/verified-x-accounts.txt"
cat > "$ALLOW" <<'EOF'
alice
bob
EOF

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

wait_height() {
  local -n _cli=$1
  local want=$2
  for _ in $(seq 1 80); do
    local h
    h="$("${_cli[@]}" getblockcount 2>/dev/null || echo 0)"
    if [[ "$h" -ge "$want" ]]; then
      return 0
    fi
    sleep 0.25
  done
  echo "did not reach height $want" >&2
  return 1
}

cleanup() {
  "${A_CLI[@]}" stop >/dev/null 2>&1 || true
  "${B_CLI[@]}" stop >/dev/null 2>&1 || true
  for _ in $(seq 1 50); do
    if ! "${A_CLI[@]}" getblockchaininfo >/dev/null 2>&1 \
       && ! "${B_CLI[@]}" getblockchaininfo >/dev/null 2>&1; then
      sleep 0.3
      return 0
    fi
    sleep 0.2
  done
}
trap cleanup EXIT

echo "== start Alice (funded) first so lottery coinbases stay in her wallet =="
"$XCOIND" -regtest -datadir="$A_DIR" -server -daemon -listen=1 -port=28643 \
  -rpcport=28642 -connect=0 -dnsseed=0 -txindex=1 \
  -xoauthmock=alice -xallowlist="$ALLOW"
wait_rpc A_CLI

CHAIN="$("${A_CLI[@]}" getblockchaininfo)"
echo "$CHAIN" | grep -q '"currency": "XFER"'
echo "$CHAIN" | grep -q '"name": "X Coin"'

ADDR_A="$("${A_CLI[@]}" getnewaddress)"
[[ "$ADDR_A" == y* ]]
echo "alice recv $ADDR_A"

echo "== Alice mines maturity + claims root ALICE =="
"${A_CLI[@]}" generatetoaddress 110 "$ADDR_A" >/dev/null
wait_height A_CLI 110

LINK="$("${A_CLI[@]}" linkxaccount)"
echo "$LINK"
echo "$LINK" | python3 -c '
import json,sys
j=json.load(sys.stdin)
if j.get("xaccount") != "alice":
    sys.exit("expected xaccount=alice")
if j.get("asset") != "ALICE":
    sys.exit("expected root ALICE")
'
"${A_CLI[@]}" listmyassets | grep -q ALICE
BAL_A="$("${A_CLI[@]}" getbalance)"
echo "alice balance $BAL_A"
python3 -c "import sys; b=float('$BAL_A'); sys.exit(0 if b>100 else 1)"

echo "== start Bob (empty wallet.dat) and sync the same tip =="
# Bob is signed in but not allowlisted, so he is not a lottery winner.
# Starting him after Alice's generates keeps coinbases out of his wallet.
"$XCOIND" -regtest -datadir="$B_DIR" -server -daemon -listen=0 -port=28653 \
  -rpcport=28652 -addnode=127.0.0.1:28643 -dnsseed=0 -txindex=1 \
  -xoauthmock=bob
wait_rpc B_CLI
wait_height B_CLI 110
ADDR_B="$("${B_CLI[@]}" getnewaddress)"
[[ "$ADDR_B" == y* ]]
echo "bob recv $ADDR_B"

echo "== Bob signed in as bob cannot spend Alice =="
BAL_B="$("${B_CLI[@]}" getbalance)"
echo "bob balance $BAL_B"
python3 -c "import sys; b=float('$BAL_B'); sys.exit(0 if b==0 else 1)"
if "${B_CLI[@]}" listmyassets | grep -q ALICE; then
  echo "bob must not see Alice's identity asset in listmyassets" >&2
  "${B_CLI[@]}" listmyassets >&2
  exit 1
fi

if "${B_CLI[@]}" sendtoaddress "$ADDR_A" 1 >/tmp/xcoin-iso-bob-send.err 2>&1; then
  echo "bob sendtoaddress must not spend alice coins" >&2
  cat /tmp/xcoin-iso-bob-send.err >&2
  exit 1
fi
grep -qiE "insufficient|not enough|too little" /tmp/xcoin-iso-bob-send.err

if "${B_CLI[@]}" transfer ALICE 1 "$ADDR_B" >/tmp/xcoin-iso-bob-xfer.err 2>&1; then
  echo "bob transfer ALICE must fail" >&2
  cat /tmp/xcoin-iso-bob-xfer.err >&2
  exit 1
fi
# Wallet does not hold ALICE; typed @alice is not a spend path.
if ! grep -qiE "insufficient|not enough|does not|don't own|do not own|wallet|asset|sign in|session" /tmp/xcoin-iso-bob-xfer.err; then
  echo "unexpected transfer error:" >&2
  cat /tmp/xcoin-iso-bob-xfer.err >&2
  exit 1
fi

echo "== Alice session on Bob's empty wallet still cannot spend Alice UTXOs =="
MOCK="$("${B_CLI[@]}" mockxsignin '{"data":{"id":"99","username":"alice"}}')"
echo "$MOCK"
echo "$MOCK" | python3 -c '
import json,sys
j=json.load(sys.stdin)
if j.get("username") != "alice":
    sys.exit("bob node must now be signed in as alice")
'
SESS="$("${B_CLI[@]}" getxsession)"
echo "$SESS" | python3 -c '
import json,sys
j=json.load(sys.stdin)
if j.get("username") != "alice" or j.get("signed_in") is not True:
    sys.exit("expected alice session on bob datadir")
'
# Session on Bob does not import Alice's wallet.dat keys.
python3 -c "import sys; b=float('$("${B_CLI[@]}" getbalance)'); sys.exit(0 if b==0 else 1)"
if "${B_CLI[@]}" listmyassets | grep -q ALICE; then
  echo "alice session on bob wallet must not list ALICE" >&2
  "${B_CLI[@]}" listmyassets >&2
  exit 1
fi
if "${B_CLI[@]}" sendtoaddress "$ADDR_A" 1 >/tmp/xcoin-iso-alice-on-bob-send.err 2>&1; then
  echo "alice session on bob wallet must not send alice coins" >&2
  cat /tmp/xcoin-iso-alice-on-bob-send.err >&2
  exit 1
fi
grep -qiE "insufficient|not enough|too little" /tmp/xcoin-iso-alice-on-bob-send.err
if "${B_CLI[@]}" transfer ALICE 1 "$ADDR_B" >/tmp/xcoin-iso-alice-on-bob-xfer.err 2>&1; then
  echo "alice session on bob wallet must not transfer ALICE" >&2
  cat /tmp/xcoin-iso-alice-on-bob-xfer.err >&2
  exit 1
fi

echo "== Alice's own wallet.dat can still spend her coins =="
BAL_A2="$("${A_CLI[@]}" getbalance)"
python3 -c "import sys; a=float('$BAL_A'); b=float('$BAL_A2'); sys.exit(0 if abs(a-b)<0.00000001 else 1)"
TX="$("${A_CLI[@]}" sendtoaddress "$ADDR_A" 1)"
echo "alice self-send $TX"
[[ -n "$TX" && ${#TX} -eq 64 ]]

echo "smoke-isolation: ok"
