# X Coin (XFER)

X Coin is a **private hard-fork of [Ravencoin v4.8.0](https://github.com/RavenProject/Ravencoin/releases/tag/v4.8.0)** for simple peer-to-peer value transfer (aimed at use between users on X) plus Ravencoin-style **user-created assets/tokens**.

There is **no mining**. A running `xcoind` / wallet is an **active node**. Every **minute**, a lottery picks who produces the block and who shares the reward. On each **halving**, one more winner is added that minute (1 → 2 → 3 …).

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
    bsdmainutils python3 libevent-dev libboost-system-dev libboost-filesystem-dev \
    libboost-chrono-dev libboost-test-dev libboost-thread-dev libssl-dev libdb++-dev
./autogen.sh
./configure --without-gui --disable-bench --disable-tests
make -j$(nproc)
```

`--disable-wallet` works if you omit BDB; a wallet is required to *produce* lottery blocks (coinbase script).

Binaries:

- `src/xcoind` — node / lottery producer
- `src/xcoin-cli` — RPC client
- `src/xcoin-tx` — transaction utility

Upstream `doc/build-*.md` still says `ravend` in places; use the `xcoin*` names.

### Run (regtest — fastest way to exercise the chain)

```bash
src/xcoind -regtest -daemon -server
src/xcoin-cli -regtest getblockchaininfo
src/xcoin-cli -regtest getlotteryinfo
src/xcoin-cli -regtest getnewaddress
src/xcoin-cli -regtest generatetoaddress 1 <address>
src/xcoin-cli -regtest issue TEST_ASSET 1000
```

`generatetoaddress` **assembles** a block; it does not hash. On main/test the producer thread emits at most one block per minute when this node wins.

## RPC (lottery)

- `getlotteryinfo` — next slot, seed, winners, reward split
- `getactivenodes` — in-memory registry
- `registeractivenode` — heartbeat

`setgenerate` / `getgenerate` are removed (they only existed to drive PoW).

## Assets

The Ravencoin asset layer is intact: `issue`, `transfer`, `listassets`, unique/qualifier/restricted assets, burn fees. Reserved names now include **XFER** / **XCOIN** as well as RVN/RAVEN. Script-level `OP_RVN_ASSET` markers are unchanged so the protocol still matches the upstream implementation.

## Honest TODOs (Phase 2)

- Gossip heartbeats so every honest node shares one active set.
- Enforce multi-winner coinbase payouts in validation.
- Freeze a launch genesis (current hashes are placeholders).
- New address prefixes (Phase 1 still uses Ravencoin base58 versions so burn addresses decode).
- Seeds, explorers, production audit, X.com API — out of scope here.
- Full `make check` against the new genesis (upstream tests hard-code Ravencoin hashes).

## License

MIT. Copyright Bitcoin Core, Raven Core, and X Coin developers. See [COPYING](COPYING). Keep upstream SPDX / copyright headers in source files.
