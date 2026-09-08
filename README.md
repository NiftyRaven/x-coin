# X Coin (XFER)

X Coin is a **private** peer-to-peer coin for simple value transfer (aimed at
use between users on X) plus **user-created assets**. There is **no mining**.
Every **minute**, a lottery picks who produces the block and who shares the
reward among **verified-X** active nodes. On each **halving**, one more winner
is added that minute (1 → 2 → 3 …). A node with no linked X account is not
lottery-eligible (it may still sync and relay).

This repository is the product home.

| | |
| --- | --- |
| Name | X Coin |
| Ticker | **XFER** |
| Subunit | **xferon** (1 XFER = 100,000,000 xferons) |
| Consensus | Minute lottery among active nodes ([docs/LOTTERY.md](docs/LOTTERY.md)) |
| Assets | One free root per verified X handle; subs and uniques ([docs/ASSETS.md](docs/ASSETS.md)) |
| Legal / opcode notes | [docs/FORK.md](docs/FORK.md) |

## Network identity

| | Main | Testnet | Regtest |
| --- | --- | --- | --- |
| P2P | 38443 | 48443 | 28443 |
| RPC | 38442 | 48442 | 28442 |
| Magic | `XFER` | `XFTN` | `XFRT` |
| Data dir (Unix) | `~/.xcoin` | `~/.xcoin/testnet1` | `~/.xcoin/regtest` |
| Config | `xcoin.conf` | | |

Do not reuse upstream v4.8.0 ports, magic, or address versions. Collision table: [docs/FORK.md](docs/FORK.md).

## Build (`--without-gui`)

On Debian/Ubuntu:

```bash
sudo apt-get update
sudo apt-get install -y build-essential libtool autotools-dev automake pkg-config \
    bsdmainutils python3 libevent-dev libboost-all-dev libssl-dev libdb++-dev
./autogen.sh
# BDB 4.8 is the portable wallet default. On modern Debian/Ubuntu use 5.3:
./configure --without-gui --disable-bench --disable-tests --with-incompatible-bdb
make -j$(nproc)
```

Verified: `xcoind` + `xcoin-cli` on Ubuntu 24.04 / gcc 13 / Boost 1.83 / BDB 5.3.

`--disable-wallet` works if you omit BDB; a wallet is required to *produce* lottery blocks (coinbase script).

Binaries:

- `src/xcoind` — node / lottery producer
- `src/xcoin-cli` — RPC client
- `src/xcoin-tx` — transaction utility
- `src/qt/xcoin-qt` — GUI (optional; omit `--without-gui`)

### Run (regtest)

```bash
src/xcoind -regtest -daemon -server -xaccount=NFTRVN -xverified=NFTRVN
src/xcoin-cli -regtest getblockchaininfo
src/xcoin-cli -regtest getlotteryinfo   # local_eligible should be true
src/xcoin-cli -regtest getnewaddress   # y… on regtest/testnet; X… on main
src/xcoin-cli -regtest generatetoaddress 110 <address>
src/xcoin-cli -regtest linkxaccount NFTRVN   # free main/root identity asset
src/xcoin-cli -regtest issue NFTRVN/NOTE 1   # sub, 100 XFER
```

`generatetoaddress` **assembles** a block; it does not hash. Coinbase is immature for 100 blocks — generate ~110 before `linkxaccount` / `issue` of a sub. On main/test the producer thread emits at most one block per minute when this node wins.

## RPC (lottery)

- `getlotteryinfo` — next slot, seed, winners, split, local X handle / eligibility
- `getactivenodes` — in-memory registry (includes `xaccount`)
- `registeractivenode` — heartbeat (rejects unlinked/unverified)
- `addxverified` / `listxverified` / `loadxverified` — verified X allowlist
- `linkxaccount` / `getmainasset` — free root assignment for a verified handle

`setgenerate` / `getgenerate` are removed.

## Assets

See [docs/ASSETS.md](docs/ASSETS.md). Each verified X account is assigned one free main/root asset. Users cannot `issue` a new root. Sub **100 XFER**, unique **5 XFER**. Restricted/qualifier assets are removed. Historical opcode and reserved-name notes: [docs/FORK.md](docs/FORK.md).

## Private test

Checklist: [docs/LAUNCH.md](docs/LAUNCH.md). Lottery: [docs/LOTTERY.md](docs/LOTTERY.md).

```bash
contrib/xcoin/smoke-regtest.sh      # lottery, multi-winner, NFTRVN root, sub/unique
contrib/xcoin/smoke-gossip.sh       # two-node P2P xhb
contrib/xcoin/smoke-eligibility.sh  # unlinked node cannot enter the lottery
```

Live X.com API keys / OAuth are out of scope. Operators verify handles **offline** and share an allowlist (see [docs/LOTTERY.md](docs/LOTTERY.md)). Also out of scope: public DNS seeds, explorers, making upstream `make check` green against the new genesis.

## License

MIT. Copyright Bitcoin Core, Raven Core, and X Coin developers. See [COPYING](COPYING). Keep upstream SPDX / copyright headers in source files. Product branding is X Coin; legal attribution of the imported tree is listed in [docs/FORK.md](docs/FORK.md).
