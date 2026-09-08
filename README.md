# X Coin (XFER)

X Coin is a **private hard-fork of [Ravencoin v4.8.0](https://github.com/RavenProject/Ravencoin/releases/tag/v4.8.0)** for simple peer-to-peer value transfer (aimed at use between users on X) plus Ravencoin-style **user-created assets/tokens**.

There is **no mining**. Every **minute**, a lottery picks who produces the
block and who shares the reward among **verified-X** active nodes. On each
**halving**, one more winner is added that minute (1 → 2 → 3 …). A node
with no linked X account is not lottery-eligible (it may still sync/relay).

This repository is the product home. Do not treat RavenProject remotes as something to push to.

| | |
| --- | --- |
| Name | X Coin |
| Ticker | **XFER** |
| Subunit | **xferon** (1 XFER = 100,000,000 xferons) |
| Consensus | Minute lottery among active nodes ([docs/LOTTERY.md](docs/LOTTERY.md)) |
| Assets | Kept from Ravencoin (issue / transfer / unique / restricted / …) |
| Upstream | Ravencoin **v4.8.0** (`22549129888d02e0e08fcdb9f96f3c699167e774`) — [docs/FORK.md](docs/FORK.md) |

## Network identity (do not collide with Ravencoin)

| | Main | Testnet | Regtest |
| --- | --- | --- | --- |
| P2P | 38443 | 48443 | 28443 |
| RPC | 38442 | 48442 | 28442 |
| Magic | `XFER` | `XFTN` | `XFRT` |
| Data dir (Unix) | `~/.xcoin` | `~/.xcoin/testnet1` | `~/.xcoin/regtest` |
| Config | `xcoin.conf` | | |

## Build

Dependencies are the same as Ravencoin Core 4.8 (Autotools, Boost, libevent, OpenSSL, Berkeley DB 4.8 for wallets). On Debian/Ubuntu:

```bash
sudo apt-get update
sudo apt-get install -y build-essential libtool autotools-dev automake pkg-config \
    bsdmainutils python3 libevent-dev libboost-all-dev libssl-dev libdb++-dev
./autogen.sh
# BDB 4.8 is the portable wallet default. On modern Debian/Ubuntu use 5.3:
./configure --without-gui --disable-bench --disable-tests --with-incompatible-bdb
make -j$(nproc)
```

Verified in this pass: `xcoind` + `xcoin-cli` linked on Ubuntu 24.04 / gcc 13 / Boost 1.83 / BDB 5.3. Two small compile fixes for that toolchain are in-tree (`init.cpp` Boost.Signals2 disconnect, `lockedpool.cpp` `<stdexcept>`).

`--disable-wallet` works if you omit BDB; a wallet is required to *produce* lottery blocks (coinbase script).

Binaries:

- `src/xcoind` — node / lottery producer
- `src/xcoin-cli` — RPC client
- `src/xcoin-tx` — transaction utility
- `src/qt/xcoin-qt` — GUI (optional)

Upstream `doc/build-*.md` still says `ravend` in places; use the `xcoin*` names.

### Run (regtest — fastest way to exercise the chain)

```bash
src/xcoind -regtest -daemon -server -xaccount=alice -xverified=alice
src/xcoin-cli -regtest getblockchaininfo
src/xcoin-cli -regtest getlotteryinfo   # local_eligible should be true
src/xcoin-cli -regtest getnewaddress   # y… on regtest/testnet; X… on main
src/xcoin-cli -regtest generatetoaddress 1 <address>
src/xcoin-cli -regtest issue TEST_ASSET 1000
```

`generatetoaddress` **assembles** a block; it does not hash. Coinbase is immature for 100 blocks — generate ~110 before `issue`. On main/test the producer thread emits at most one block per minute when this node wins.

Verified on regtest in this pass: distinct genesis, `getlotteryinfo` (1 winner, 5000 XFER), on-demand block, `issue TESTASSET` + owner token `TESTASSET!`.

## RPC (lottery)

- `getlotteryinfo` — next slot, seed, winners, split, local X handle / eligibility
- `getactivenodes` — in-memory registry (includes `xaccount`)
- `registeractivenode` — heartbeat (rejects unlinked/unverified)
- `addxverified` / `listxverified` / `loadxverified` — verified X allowlist

`setgenerate` / `getgenerate` are removed (they only existed to drive PoW).

## Assets

The Ravencoin asset layer is intact: `issue`, `transfer`, `listassets`, unique/qualifier/restricted assets, burn fees. Reserved names now include **XFER** / **XCOIN** as well as RVN/RAVEN. Script-level `OP_RVN_ASSET` markers are unchanged so the protocol still matches the upstream implementation.

## Launch

Private-launch checklist (ports, magic, genesis, seed publish, join, rewards, risks): [docs/LAUNCH.md](docs/LAUNCH.md). Lottery: [docs/LOTTERY.md](docs/LOTTERY.md).

`contrib/xcoin/smoke-regtest.sh` exercises lottery + a single-winner coinbase at height 149 + a two-winner split at height 150 + `issue TESTASSET`. `contrib/xcoin/smoke-gossip.sh` checks that two nodes share one active-node set over P2P `xhb`. `contrib/xcoin/smoke-eligibility.sh` checks that an unlinked node cannot enter the lottery.

Live X.com API keys / OAuth are out of scope. Eligibility is an operator-shared allowlist of X-verified handles (see [docs/LOTTERY.md](docs/LOTTERY.md)). Also out of scope: explorers, DNS seeds, making upstream `make check` green against the new genesis.

## License

MIT. Copyright Bitcoin Core, Raven Core, and X Coin developers. See [COPYING](COPYING). Keep upstream SPDX / copyright headers in source files.
