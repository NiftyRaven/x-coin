#!/usr/bin/env bash
# Freeze mainnet genesis nTime to now (or NTIME=unix) and patch hashes.
# Run this at go-live, then start the first eligible node immediately
# so lottery timestamps are the birth of the chain — not a development midnight.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
CPP="$ROOT/src/chainparams.cpp"
XCOIND="${XCOIND:-$ROOT/src/xcoind}"
NTIME="${NTIME:-$(date -u +%s)}"
JOBS="${JOBS:-$(nproc 2>/dev/null || echo 4)}"

if ! grep -q 'mainnet-genesis' "$CPP"; then
  echo "src/chainparams.cpp missing mainnet-genesis marker" >&2
  exit 1
fi

python3 - "$CPP" "$NTIME" <<'PY'
import pathlib, re, sys
path, ntime = pathlib.Path(sys.argv[1]), sys.argv[2]
text = path.read_text()
text2, n = re.subn(
    r'CreateGenesisBlock\(\d+, 1, 0x207fffff, 4, 5000 \* COIN\); // mainnet-genesis',
    f'CreateGenesisBlock({ntime}, 1, 0x207fffff, 4, 5000 * COIN); // mainnet-genesis',
    text,
    count=1,
)
if n != 1:
    sys.exit('failed to patch mainnet-genesis nTime')
path.write_text(text2)
print(f'patched main nTime={ntime}')
PY

if [[ ! -x "$XCOIND" ]]; then
  echo "build xcoind first (configure && make)" >&2
  exit 1
fi
make -j"$JOBS" -C "$ROOT" src/xcoind >/dev/null

capture() {
  set +e
  OUT="$("$XCOIND" -printgenesis 2>&1)"
  RC=$?
  set -e
  printf '%s\n' "$OUT"
  return 0
}

OUT="$(capture)"
LINE="$(printf '%s\n' "$OUT" | grep -E '^XCOIN_GENESIS main ' | tail -1 || true)"
if [[ -z "$LINE" ]]; then
  echo "xcoind -printgenesis did not print XCOIN_GENESIS main" >&2
  printf '%s\n' "$OUT" >&2
  exit 1
fi

HASH="$(printf '%s\n' "$LINE" | python3 -c 'import sys
p=sys.stdin.read().split()
print([x.split("=",1)[1] for x in p if x.startswith("hash=")][0])')"
if [[ "$HASH" != 0x* ]]; then
  HASH="0x$HASH"
fi

python3 - "$CPP" "$HASH" <<'PY'
import pathlib, re, sys
path, newhash = pathlib.Path(sys.argv[1]), sys.argv[2]
text = path.read_text()
text2, n = re.subn(
    r'(CheckGenesis\("main", genesis, consensus.hashGenesisBlock,\n\s+")0x[0-9a-f]+(")',
    r'\g<1>' + newhash + r'\2',
    text,
    count=1,
)
if n != 1:
    sys.exit('failed to patch main genesis hash')
path.write_text(text2)
print(f'patched main hash={newhash}')
PY

make -j"$JOBS" -C "$ROOT" src/xcoind >/dev/null
VERIFY="$("$XCOIND" -printgenesis 2>/dev/null | grep '^XCOIN_GENESIS main ')"
echo "$VERIFY"
echo "main genesis frozen at nTime=$NTIME"
echo "Start the first X Verified node now. Height 1 is the next lottery minute."
echo "Do not make the repository public until that node is running."
