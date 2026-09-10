#!/usr/bin/env bash
# Multi-node readiness benchmark: a lone node already has a ledger; a mesh
# of two (or three) nodes shares one tip; a signed-in send is visible on
# the receiver (listtransactions / gettransaction) and to an explorer-less
# peer via getrawtransaction / getblock. No public explorer.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
XCOIND="${XCOIND:-$ROOT/src/xcoind}"
XCLI="${XCLI:-$ROOT/src/xcoin-cli}"
BASE="${DATADIR:-$ROOT/smoke-benchmark-datadir}"
A_DIR="$BASE/a"
B_DIR="$BASE/b"
C_DIR="$BASE/c"
A_CLI=("$XCLI" -regtest -datadir="$A_DIR" -rpcport=28542)
B_CLI=("$XCLI" -regtest -datadir="$B_DIR" -rpcport=28552)
C_CLI=("$XCLI" -regtest -datadir="$C_DIR" -rpcport=28562)
GENESIS_REGTEST="1dc800dace1cc1222e03ba3c7852f60d379539074bba923599d0458aeba2b2e4"

if [[ ! -x "$XCOIND" ]]; then
  echo "missing $XCOIND — build first" >&2
  exit 1
fi

rm -rf "$BASE"
mkdir -p "$A_DIR" "$B_DIR" "$C_DIR"
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
  "${C_CLI[@]}" stop >/dev/null 2>&1 || true
  for _ in $(seq 1 50); do
    if ! "${A_CLI[@]}" getblockchaininfo >/dev/null 2>&1 \
       && ! "${B_CLI[@]}" getblockchaininfo >/dev/null 2>&1 \
       && ! "${C_CLI[@]}" getblockchaininfo >/dev/null 2>&1; then
      sleep 0.3
      return 0
    fi
    sleep 0.2
  done
}
trap cleanup EXIT

START_NS="$(date +%s%N)"

echo "== phase 1: lone node already has a ledger =="
# A single xcoind stores genesis and can extend the chain. A network is
# not required for a local UTXO set (regtest generate; main lottery producer).
"$XCOIND" -regtest -datadir="$A_DIR" -server -daemon -listen=1 -port=28543 \
  -rpcport=28542 -connect=0 -dnsseed=0 -txindex=1 \
  -xoauthmock=alice:verified -xallowlist="$ALLOW"
wait_rpc A_CLI
A_GEN="$("${A_CLI[@]}" getblockhash 0)"
echo "A genesis $A_GEN"
[[ "$A_GEN" == "$GENESIS_REGTEST" ]]
A_CHAIN="$("${A_CLI[@]}" getblockchaininfo)"
echo "$A_CHAIN" | grep -q '"chain": "regtest"'
echo "$A_CHAIN" | grep -q '"blocks": 0'
ADDR_A="$("${A_CLI[@]}" getnewaddress)"
[[ "$ADDR_A" == y* && "$ADDR_A" != R* && "$ADDR_A" != n* ]]
"${A_CLI[@]}" generatetoaddress 5 "$ADDR_A" >/dev/null
[[ "$("${A_CLI[@]}" getblockcount)" -eq 5 ]]
TIP5="$("${A_CLI[@]}" getbestblockhash)"
"${A_CLI[@]}" getblock "$TIP5" true >/dev/null
echo "lone A height 5 tip $TIP5 (local ledger exists)"

echo "== phase 2: connect B (signed-in receiver) and C (unsigned observer) =="
"$XCOIND" -regtest -datadir="$B_DIR" -server -daemon -listen=1 -port=28553 \
  -rpcport=28552 -addnode=127.0.0.1:28543 -dnsseed=0 -txindex=1 \
  -xoauthmock=bob:verified -xallowlist="$ALLOW"
# C has no session: still syncs, relays, inspects blocks/txs by RPC,
# and can send/receive (normal wallet). Root claim / lottery stay gated.
"$XCOIND" -regtest -datadir="$C_DIR" -server -daemon -listen=0 -port=28563 \
  -rpcport=28562 -addnode=127.0.0.1:28543 -dnsseed=0 -txindex=1
wait_rpc B_CLI
wait_rpc C_CLI

B_GEN="$("${B_CLI[@]}" getblockhash 0)"
C_GEN="$("${C_CLI[@]}" getblockhash 0)"
[[ "$B_GEN" == "$GENESIS_REGTEST" && "$C_GEN" == "$GENESIS_REGTEST" ]]
echo "B and C share A's genesis (own X Coin ledger, not an imported snapshot)"

ok_peers=0
for _ in $(seq 1 40); do
  pa="$("${A_CLI[@]}" getpeerinfo | python3 -c 'import json,sys; print(len(json.load(sys.stdin)))')"
  pb="$("${B_CLI[@]}" getpeerinfo | python3 -c 'import json,sys; print(len(json.load(sys.stdin)))')"
  pc="$("${C_CLI[@]}" getpeerinfo | python3 -c 'import json,sys; print(len(json.load(sys.stdin)))')"
  if [[ "$pa" -ge 2 && "$pb" -ge 1 && "$pc" -ge 1 ]]; then
    ok_peers=1
    break
  fi
  sleep 0.4
done
if [[ "$ok_peers" -ne 1 ]]; then
  echo "peers did not connect (A=$pa B=$pb C=$pc)" >&2
  "${A_CLI[@]}" getpeerinfo >&2 || true
  exit 1
fi
echo "peers A=$pa B=$pb C=$pc"

wait_height B_CLI 5
wait_height C_CLI 5
[[ "$("${B_CLI[@]}" getbestblockhash)" == "$TIP5" ]]
[[ "$("${C_CLI[@]}" getbestblockhash)" == "$TIP5" ]]
echo "B and C synced lone-node tip $TIP5"

echo "== mature coinbase on A, then signed-in send A -> B =="
"${A_CLI[@]}" generatetoaddress 105 "$ADDR_A" >/dev/null
wait_height B_CLI 110
wait_height C_CLI 110
TIP110="$("${A_CLI[@]}" getbestblockhash)"
[[ "$("${B_CLI[@]}" getbestblockhash)" == "$TIP110" ]]
[[ "$("${C_CLI[@]}" getbestblockhash)" == "$TIP110" ]]

ADDR_B="$("${B_CLI[@]}" getnewaddress)"
[[ "$ADDR_B" == y* ]]
echo "B receive $ADDR_B"
# Unsigned observer can open a receive address (normal wallet).
ADDR_C="$("${C_CLI[@]}" getnewaddress)"
[[ "$ADDR_C" == y* ]]
echo "C unsigned receive $ADDR_C"

SEND_NS="$(date +%s%N)"
TXID="$("${A_CLI[@]}" sendtoaddress "$ADDR_B" 25)"
echo "A sent $TXID"
"${A_CLI[@]}" gettransaction "$TXID" >/dev/null

ok_mem=0
for _ in $(seq 1 80); do
  in_b="$("${B_CLI[@]}" getrawmempool | python3 -c 'import json,sys; print(1 if "'"$TXID"'" in json.load(sys.stdin) else 0)')"
  in_c="$("${C_CLI[@]}" getrawmempool | python3 -c 'import json,sys; print(1 if "'"$TXID"'" in json.load(sys.stdin) else 0)')"
  if [[ "$in_b" -eq 1 && "$in_c" -eq 1 ]]; then
    ok_mem=1
    break
  fi
  sleep 0.5
done
if [[ "$ok_mem" -ne 1 ]]; then
  echo "mempool did not gossip $TXID to B/C" >&2
  "${B_CLI[@]}" getrawmempool >&2 || true
  "${C_CLI[@]}" getrawmempool >&2 || true
  exit 1
fi
echo "B and C see $TXID in mempool (peers see unconfirmed txs)"

# Explorer-less node RPC: verbose getrawtransaction on the unsigned observer.
C_RAW="$("${C_CLI[@]}" getrawtransaction "$TXID" true)"
echo "$C_RAW" | python3 -c '
import json, sys
tx = json.load(sys.stdin)
if tx.get("txid") != sys.argv[1]:
    sys.exit("observer getrawtransaction txid mismatch")
print("C getrawtransaction (mempool) ok")
' "$TXID"

"${A_CLI[@]}" generatetoaddress 1 "$ADDR_A" >/dev/null
wait_height B_CLI 111
wait_height C_CLI 111
BEST_A="$("${A_CLI[@]}" getbestblockhash)"
BEST_B="$("${B_CLI[@]}" getbestblockhash)"
BEST_C="$("${C_CLI[@]}" getbestblockhash)"
echo "tips A=$BEST_A B=$BEST_B C=$BEST_C"
[[ "$BEST_A" == "$BEST_B" && "$BEST_B" == "$BEST_C" ]]

echo "== B wallet visibility (listtransactions / gettransaction) =="
B_LIST="$("${B_CLI[@]}" listtransactions "*" 20 0)"
echo "$B_LIST" | python3 -c '
import json, sys
txs = json.load(sys.stdin)
want = sys.argv[1]
hits = [t for t in txs if t.get("txid") == want]
if not hits:
    sys.exit("B listtransactions missing send %s" % want)
recv = [t for t in hits if t.get("category") == "receive"]
if not recv:
    sys.exit("B listtransactions has tx but no receive category")
if abs(sum(t.get("amount", 0) for t in recv) - 25) > 1e-6:
    sys.exit("B receive amount is not 25 XFER")
print("B listtransactions receive", recv[0].get("amount"), "conf", recv[0].get("confirmations"))
' "$TXID"
B_GT="$("${B_CLI[@]}" gettransaction "$TXID")"
echo "$B_GT" | python3 -c '
import json, sys
t = json.load(sys.stdin)
if t.get("txid") != sys.argv[1]:
    sys.exit("gettransaction txid mismatch")
if t.get("confirmations", 0) < 1:
    sys.exit("expected >=1 confirmation")
details = t.get("details") or []
recv = [d for d in details if d.get("category") == "receive"]
if not recv or abs(sum(d.get("amount", 0) for d in recv) - 25) > 1e-6:
    sys.exit("gettransaction receive is not 25 XFER")
print("B gettransaction ok confirmations", t.get("confirmations"))
' "$TXID"

echo "== explorer-less peer: getblock + getrawtransaction after confirm =="
C_BLK="$("${C_CLI[@]}" getblock "$BEST_C" true)"
echo "$C_BLK" | python3 -c '
import json, sys
b = json.load(sys.stdin)
if sys.argv[1] not in b.get("tx", []):
    sys.exit("confirmed block on C is missing txid")
print("C getblock contains", sys.argv[1], "height", b.get("height"))
' "$TXID"
"${C_CLI[@]}" getrawtransaction "$TXID" true | python3 -c '
import json, sys
tx = json.load(sys.stdin)
if not tx.get("blockhash"):
    sys.exit("confirmed getrawtransaction missing blockhash")
print("C getrawtransaction confirmed in", tx.get("blockhash"))
'
A_LIST="$("${A_CLI[@]}" listtransactions "*" 20 0)"
echo "$A_LIST" | python3 -c '
import json, sys
txs = json.load(sys.stdin)
want = sys.argv[1]
sends = [t for t in txs if t.get("txid") == want and t.get("category") == "send"]
if not sends:
    sys.exit("A listtransactions missing send")
print("A listtransactions send", sends[0].get("amount"))
' "$TXID"

END_NS="$(date +%s%N)"
SEND_MS=$(( (END_NS - SEND_NS) / 1000000 ))
TOTAL_MS=$(( (END_NS - START_NS) / 1000000 ))
echo "benchmark: send-to-shared-tip ${SEND_MS}ms  wall ${TOTAL_MS}ms"
echo "smoke-benchmark: ok (lone ledger + 3-node visible send)"
