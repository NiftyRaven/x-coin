# X Coin (XFER)

Peer-to-peer coin for users on X. **No mining.** Every minute a lottery
among verified active nodes produces the block and splits the subsidy.
Fair launch, no premine. Spendable supply ~21 billion XFER.

**Nifty Raven (@NFTRVN on X).** Anonymous public release — handle and
display name only.

Whitepaper: [whitepaper/XCOIN.md](whitepaper/XCOIN.md).

| | |
| --- | --- |
| Name | X Coin |
| Ticker | **XFER** |
| Subunit | **xferon** (1 XFER = 100,000,000 xferons) |
| Address (main) | `X…` |
| Data dir | `~/.xcoin` |
| Config | `xcoin.conf` |
| P2P / RPC | **38443** / **38442** |

## What the wallet is

The wallet **is the core wallet**: `xcoind` (node + `wallet.dat`) and
`xcoin-cli` (RPC). Optional desktop GUI is `xcoin-qt` if you build it.
There is **no** separate mobile wallet, **no** X-app wallet, and **no**
X.com OAuth login. You do not “log into X” inside this software.

**Done in this tree:** product name/ports/magic, minute lottery, verified-X
allowlist, `linkxaccount` (one free root per handle), user roots forbidden,
restricted assets removed.

**Still inherited core behavior:** UTXO `wallet.dat`, send/receive, fees,
backup/encrypt RPCs, P2P, mempool, asset script format. Qt is the inherited
desktop UI — not a polished new wallet.

Operators confirm X handles **offline** (open the public profile, check the
verified badge) and share an allowlist. The node never calls the X API.

## 1. Build

Known working on Ubuntu (headless):

```bash
sudo apt-get update
sudo apt-get install -y build-essential libtool autotools-dev automake pkg-config \
    bsdmainutils python3 libevent-dev libboost-all-dev libssl-dev libdb++-dev
./autogen.sh
./configure --without-gui --disable-bench --disable-tests --with-incompatible-bdb
make -j$(nproc)
```

Binaries: `src/xcoind`, `src/xcoin-cli`. A wallet (Berkeley DB) is required
to produce lottery blocks. `--disable-wallet` builds a relay-only node.

To also build `src/qt/xcoin-qt`, omit `--without-gui` and install Qt. That
GUI is optional and unverified on this private-test path.

## 2. Start the node (creates / loads the wallet)

Default datadir is `~/.xcoin`. First start creates `wallet.dat` there.
There is no separate `createwallet` app — the daemon **is** the wallet.

```bash
# ~/.xcoin/xcoin.conf  (create the file)
server=1
rpcuser=xcoin
rpcpassword=change-me
# private mesh — no public DNS seeds
# addnode=<seed-ip>:38443
```

```bash
src/xcoind -daemon -server
src/xcoin-cli getblockchaininfo
src/xcoin-cli getwalletinfo
```

Regtest (local, no seed):

```bash
src/xcoind -regtest -daemon -server
src/xcoin-cli -regtest getwalletinfo
```

Stop: `src/xcoin-cli stop` (add `-regtest` on regtest).

Optional: `encryptwallet "pass phrase"` then restart; `backupwallet /path/backup.dat`.

## 3. Get an address, send and receive XFER

```bash
src/xcoin-cli getnewaddress          # main: X…   test/regtest: y…
src/xcoin-cli getbalance
src/xcoin-cli listunspent
src/xcoin-cli sendtoaddress <X-or-y-address> 1.5
```

Give the address to the sender. Incoming XFER appears in `listunspent` /
`getbalance` after it confirms.

You do **not** need a linked X handle to hold, receive, or send XFER.

On main/test, new coinbase is immature for 100 blocks. On **regtest**,
`generatetoaddress` assembles blocks (no hashing) so you can mature funds:

```bash
ADDR=$(src/xcoin-cli -regtest getnewaddress)
src/xcoin-cli -regtest generatetoaddress 110 "$ADDR"
src/xcoin-cli -regtest getbalance
```

`generatetoaddress` is rejected on main/test.

## 4. Link an X handle (allowlist, then free root)

Example handle: **NFTRVN** (owner). Confirm `https://x.com/NFTRVN` offline.
Do not invent OAuth.

```bash
# operator allowlist (any one of these)
src/xcoin-cli addxverified NFTRVN
# or start with:  -xverified=NFTRVN
# or file:        -xallowlist=/shared/verified-x-accounts.txt
#                 (same format as contrib/xcoin/verified-x-accounts.example.txt)

src/xcoin-cli listxverified
src/xcoin-cli linkxaccount NFTRVN          # assigns free root NFTRVN
src/xcoin-cli getmainasset NFTRVN
src/xcoin-cli listmyassets
```

`linkxaccount` pays **0 XFER**. It creates root `NFTRVN` plus owner token
`NFTRVN!`. Users cannot `issue` a new root (`issue TESTASSET` fails).

You can also set the node’s own handle in config:

```
xaccount=NFTRVN
xallowlist=/shared/verified-x-accounts.txt
```

then `linkxaccount` with no arguments uses `-xaccount`.

## 5. Issue a sub or unique

Needs the owner token (`NAME!`) and burn XFER (mature).

```bash
src/xcoin-cli issue NFTRVN/NOTE 1          # sub, burns 100 XFER
src/xcoin-cli issueunique NFTRVN '["ONE"]' # unique NFTRVN#ONE, burns 5 XFER
src/xcoin-cli listmyassets
src/xcoin-cli transfer "NFTRVN/NOTE" 1 <address>
```

Restricted / qualifier / freeze assets are removed.

## 6. Lottery (stay online)

Stay running. The producer heartbeats if this node is linked **and**
allowlisted. At most one block per minute on main/test when you win.

```bash
src/xcoin-cli getlotteryinfo     # local_eligible must be true to win
src/xcoin-cli getactivenodes
src/xcoin-cli registeractivenode # rejected if unlinked / not listed
```

On each halving the winner count increases (1 → 2 → 3 …) and that minute’s
subsidy is split. Algorithm: [docs/LOTTERY.md](docs/LOTTERY.md).

## 7. Private test

```bash
contrib/xcoin/smoke-regtest.sh
contrib/xcoin/smoke-gossip.sh
contrib/xcoin/smoke-eligibility.sh
```

Operator runbook: [docs/LAUNCH.md](docs/LAUNCH.md). Assets: [docs/ASSETS.md](docs/ASSETS.md).

No public DNS seeds. Join with `addnode=<seed-ip>:38443`.

## License

MIT. Consensus code descends from open upstream; the notices in
[COPYING](COPYING) still apply. Historical opcode names and copyright
headers: [docs/FORK.md](docs/FORK.md).
