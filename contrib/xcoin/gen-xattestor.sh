#!/usr/bin/env bash
# Generate datadir/xattestor.key (32-byte hex). Seed only.
# A new pubkey is a coordinated wallet release — do not rotate casually.
set -euo pipefail
OUT="${1:-${HOME}/.xcoin/xattestor.key}"
mkdir -p "$(dirname "$OUT")"
umask 077
TMP="$(mktemp)"
openssl ecparam -name secp256k1 -genkey -noout -out "$TMP"
PRIV="$(openssl ec -in "$TMP" -noout -text 2>/dev/null | awk '/priv:/{p=1;next} /pub:/{p=0} p' | tr -d ' \n:')"
PUB="$(openssl ec -in "$TMP" -text -noout -conv_form compressed 2>/dev/null | awk '/pub:/{p=1;next} /ASN1/{p=0} p' | tr -d ' \n:')"
rm -f "$TMP"
if [[ ${#PRIV} -ne 64 ]]; then
  echo "could not parse secp256k1 private key" >&2
  exit 1
fi
printf '%s\n' "$PRIV" > "$OUT"
chmod 600 "$OUT"
echo "wrote $OUT"
echo "compressed pubkey hex (must match chainparams):"
echo "$PUB"
echo "never commit this file or ship it in the user zip"
