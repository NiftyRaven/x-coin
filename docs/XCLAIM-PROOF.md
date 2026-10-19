# XPF1 site proof — operator cutover

Policy only. Do not run this until the owner says go live. Do not print or commit a private key. The agent that wrote this did not deploy it and did not open a shell on any server.

Consensus is unchanged. A node with no `-xclaimproofpubkey` still accepts every claim it accepts today. The size exemption for a structurally valid XPF1 (up to 160 bytes) is always on, including while enforcement is off.

Enforcement turns on only when `-xclaimproofpubkey` lists at least one signer. Unknown key id and a bad signature are both `xclaim-proof-bad-sig`. Owner and root addresses that disagree are `xclaim-proof-mismatch`.

## What relays during the transition

1.0.19 and 1.0.20 peers do **not** relay a proof-carrying claim. Their `IsStandard()` rejects an OP_RETURN larger than 83 bytes, and XPF1 does not fit. They still accept a block that already contains the claim, because block connect does not use that relay rule.

The path that works: the user's 1.0.23 wallet (exemption always on) addnode's the seed, and the Hetzner 1.0.23 wallet relays to the seed with `sendrawtransaction`. Upgrade Hetzner first (exemption only, pubkey empty), then the seed with both pubkeys, then turn the site flag on. Until the seed flag is on, unproofed claims from older wallets still enter blocks.

## Keys

Two keys, generated once, before the seed restarts.

- Key id 1, live, on Hetzner. File `/etc/xcoin/xclaim-proof.key`, mode `0600`, owner `root:xcoin`, one line, 64 hex chars. Outside git. Never log it.
- Key id 2, standby. Encrypted at generation with `openssl enc -aes-256-cbc -pbkdf2 -iter 600000`. Ciphertext only, copied to `E:\xcoin vu`. Never left in plaintext on a disk. The seed is configured with **both** pubkeys on the first 1.0.23 start, so starting the standby signer later does not restart `xcoind`.

Shared digest vector: `src/test/data/xpf1-vector.json` (same file in x-coin-web). No private key in that file.

Password for the encrypted standby is `$VPS_SSH_PASSPHRASE`. Do not write it into a unit file or a log.

## 0. Generate both keys (print pubkeys only)

Quiet window: no pending fee-share batch and an empty seed mempool. CI artifacts:

- `X-Coin-1.0.23-Linux-x86_64.tar.gz` and `X-Coin-1.0.23-Windows.zip` (Heavy)
- `X-Coin-1.0.23-Linux-x86_64.tar.gz` and `X-Coin-1.0.23-Windows.zip` from the Light workflow (tag `v1.0.23-light`)
- each workflow's `SHA256SUMS` artifact

```bash
set -euo pipefail
umask 077
install -d -m 0750 -o root -g xcoin /etc/xcoin
gen() {
  tmp=$(mktemp)
  openssl ecparam -name secp256k1 -genkey -noout -out "$tmp"
  priv=$(openssl ec -in "$tmp" -text -noout | awk '/priv:/{getline; gsub(/[: ]/,""); print; exit}')
  if [ "${#priv}" = 66 ] && [ "${priv:0:2}" = "00" ]; then priv=${priv:2}; fi
  test "${#priv}" = 64
  printf '%s\n' "$priv" > "$1"
  chmod 0600 "$1"
  echo "pubkey id $2:"
  openssl ec -in "$tmp" -pubout -conv_form compressed -outform DER | tail -c 33 | od -An -tx1 | tr -d ' \n'
  echo
  shred -u "$tmp" 2>/dev/null || rm -f "$tmp"
  unset priv
}
gen /etc/xcoin/xclaim-proof.key 1
gen /tmp/xclaim-standby.key 2
openssl enc -aes-256-cbc -pbkdf2 -iter 600000 \
  -in /tmp/xclaim-standby.key -out /tmp/xclaim-standby.key.enc
shred -u /tmp/xclaim-standby.key 2>/dev/null || rm -f /tmp/xclaim-standby.key
# Copy only /tmp/xclaim-standby.key.enc to E:\xcoin vu, then shred the VPS copy.
```

Seed argument from the first 1.0.23 start:

```text
-xclaimproofpubkey=1:<live pubkey>,2:<standby pubkey>
```

## 1. Hetzner wallet node (size exemption only)

Host `37.27.246.20`. systemd unit `xcoin-wallet`. Datadir `/var/lib/xcoin-wallet`. Current tree `/opt/xcoin-releases/1.0.19`.

This restarts the operator wallet. It needs an explicit OK. No unlock. No wallet RPC writes.

The running process was started with `-reindex`. The unit does not have `-reindex`, so this restart does not reindex. Set `TimeoutStopSec=600`.

```bash
# unpack the CI tarball to /opt/xcoin-releases/1.0.23 and verify SHA256
# Point ExecStart at the 1.0.23 xcoind. Leave -xclaimproofpubkey unset.
systemctl restart xcoin-wallet
# height matches the seed, peers >= 3, relay and explorer health are 200
```

Rollback: point ExecStart back at 1.0.19 and `systemctl restart xcoin-wallet`.

## 2. Seed (the only block producer)

`azureuser@172.191.195.221`, key `D:\xcoin stuff\xcoin-seed_key.pem`. Not systemd. Hands-off until an explicit OK.

Live process is a manual `./bin/xcoind` in `~/xcoin-launch/xcoin-1.0.20-linux-x86_64` with datadir `/home/azureuser/.xcoin`, listen, server, port 38443, bind `0.0.0.0:38443`, and `-xattestorkey=/home/azureuser/.xcoin/xattestor.key`. Template: `~/xcoin-launch/seed-cutover-1.0.20.sh` (log `seed120.log`).

```bash
# untar to ~/xcoin-launch/xcoin-1.0.23-linux-x86_64 and verify SHA256
# leave the 1.0.20 directory untouched (that is the binary backup)
mkdir -p ~/xcoin-launch/bak-pre-1.0.23
# copy the live cmdline into that directory
# record PRE height, tip hash, and peer count
~/xcoin-launch/xcoin-1.0.20-linux-x86_64/bin/xcoin-cli -datadir=/home/azureuser/.xcoin stop
# wait until the process has exited
# start 1.0.23 with the SAME args plus
# -xclaimproofpubkey=1:<live>,2:<standby>
```

One restart. The chain pauses about one block.

Verify: process is up; within 2 minutes `debug.log` contains `lottery: produced block`; peers return to the pre count; the Hetzner node follows; the explorer tip moves.

Auto-rollback: if there is no new block in 3 minutes, or the process dies, start 1.0.20 again with the identical old cmdline (no `-xclaimproofpubkey`).

## 3. Site

Back up `/opt/x-coin-web/.env` first. Deploy the x-coin-web PR. Set `XCLAIM_PROOF=1` and `XCLAIM_PROOF_KEY_FILE=/etc/xcoin/xclaim-proof.key` (key id 1). Restart the relay. The process refuses to sign if the key file is missing. Do not log the key.

Wallet and site URL list, in order:

1. `https://xferchain.net/api/x/claim-proof`
2. `http://172.191.195.221:38445/claim-proof` (closed until an emergency)

The signature is checked against the configured pubkeys, so the standby URL can be plain HTTP. A substitute response cannot forge a proof.

## 4. Smoke

On the seed, `testmempoolaccept` only (do not broadcast):

- an unproofed claim for an unclaimed test handle returns `xclaim-proof-missing`
- a proof from key id 1 is accepted
- a real claim through the site lands in the next block

## 5. Emergency standby (no new server, no xcoind restart)

The seed already trusts key id 2. On the seed VM only:

```bash
# ciphertext is the file from E:\xcoin vu. Do not decrypt it to disk.
umask 077
# port 38445 was closed. Open it only for this window if wallets must reach it.
python3 contrib/xcoin/xclaim-signer.py \
  --key-id 2 \
  --key-file /path/to/xclaim-standby.key.enc \
  --bind 0.0.0.0 --port 38445
# password: prompt, or XCLAIM_KEY_PASSPHRASE in the environment of this process only
```

The script decrypts into memory, checks the caller's X access token with `GET https://api.twitter.com/2/users/me`, rate-limits, and signs key id 2. It does not start or stop `xcoind`. Stop the script and close port 38445 when the live signer is back. Shred any accidental plaintext.

Needs `coincurve` (`pip install --user coincurve`) for the recoverable signature. `--self-check src/test/data/xpf1-vector.json` checks the digest with no key.

## 6. Rollback order

1. Site: `XCLAIM_PROOF=0` (or unset) and restart the relay. Stop the standby signer if it is running.
2. Seed: back to the 1.0.20 cmdline.
3. Hetzner: ExecStart back to 1.0.19 and restart `xcoin-wallet`.

## Wallet copy

If the signer is down, the wallet says only new claims pause. Coins, Launch, and existing names are unaffected. Send and receive on older wallets are unchanged.
