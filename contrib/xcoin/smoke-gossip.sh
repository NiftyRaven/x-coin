#!/usr/bin/env bash
# Two-node regtest: peers must share one active-node set via xhb gossip.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
XCOIND="${XCOIND:-$ROOT/src/xcoind}"
XCLI="${XCLI:-$ROOT/src/xcoin-cli}"
BASE="${DATADIR:-$ROOT/smoke-gossip-datadir}"
A_DIR="$BASE/a"
B_DIR="$BASE/b"
A_CLI=("$XCLI" -regtest -datadir="$A_DIR" -rpcport=28442)
B_CLI=("$XCLI" -regtest -datadir="$B_DIR" -rpcport=28452)

if [[ ! -x "$XCOIND" ]]; then
  echo "missing $XCOIND — build first" >&2
  exit 1
fi

rm -rf "$BASE"
mkdir -p "$A_DIR" "$B_DIR"

"$XCOIND" -regtest -datadir="$A_DIR" -server -daemon -listen=1 -port=28443 -rpcport=28442 -connect=0 -dnsseed=0
"$XCOIND" -regtest -datadir="$B_DIR" -server -daemon -listen=0 -port=28453 -rpcport=28452 -addnode=127.0.0.1:28443 -dnsseed=0
cleanup() {
  "${A_CLI[@]}" stop >/dev/null 2>&1 || true
  "${B_CLI[@]}" stop >/dev/null 2>&1 || true
}
trap cleanup EXIT

up=0
for _ in $(seq 1 100); do
  if "${A_CLI[@]}" getlotteryinfo >/dev/null 2>&1 && "${B_CLI[@]}" getlotteryinfo >/dev/null 2>&1; then
    up=1
    break
  fi
  sleep 0.2
done
if [[ "$up" -ne 1 ]]; then
  echo "nodes did not become ready" >&2
  exit 1
fi

ok=0
for _ in $(seq 1 40); do
  peers=$("${A_CLI[@]}" getpeerinfo | python3 -c 'import json,sys; print(len(json.load(sys.stdin)))')
  na=$("${A_CLI[@]}" getactivenodes | python3 -c 'import json,sys; print(len(json.load(sys.stdin)))')
  nb=$("${B_CLI[@]}" getactivenodes | python3 -c 'import json,sys; print(len(json.load(sys.stdin)))')
  if [[ "$peers" -ge 1 && "$na" -ge 2 && "$nb" -ge 2 ]]; then
    ok=1
    break
  fi
  sleep 0.5
done
if [[ "$ok" -ne 1 ]]; then
  echo "gossip did not converge" >&2
  "${A_CLI[@]}" getactivenodes >&2 || true
  "${B_CLI[@]}" getactivenodes >&2 || true
  exit 1
fi

export A_JSON="$("${A_CLI[@]}" getactivenodes)"
export B_JSON="$("${B_CLI[@]}" getactivenodes)"
python3 - <<'PY'
import json, os
a = json.loads(os.environ["A_JSON"])
b = json.loads(os.environ["B_JSON"])
ids_a = sorted(n["id"] for n in a)
ids_b = sorted(n["id"] for n in b)
if ids_a != ids_b or len(ids_a) < 2:
    raise SystemExit("active-node sets differ: %s vs %s" % (ids_a, ids_b))
print("gossip ids", ids_a)
PY

echo "smoke-gossip: ok"
