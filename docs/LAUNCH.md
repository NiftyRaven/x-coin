# X Coin private launch checklist

This is the operator runbook for a **private launch**. It assumes you are
building from this repository (Phase 1 import + this launch-ready delta).
There is no public seed DNS and no exchange listing. Lottery eligibility
uses a **shared verified-X allowlist** (no live X API keys required).

## Frozen identity

| | Main | Testnet | Regtest |
| --- | --- | --- | --- |
| Magic | `58 46 45 52` (`XFER`) | `58 46 54 4E` (`XFTN`) | `58 46 52 54` (`XFRT`) |
| P2P port | **38443** | **48443** | **28443** |
| RPC port | **38442** | **48442** | **28442** |
| Address (P2PKH) | version **76** (`X…`) | version **140** (`y…`) | version **140** (`y…`) |
| Address (P2SH) | version **139** (`x…`) | version **200** | version **200** |
| WIF | version **204** | version **247** | version **247** |
| Data dir (Unix) | `~/.xcoin` | `~/.xcoin/testnet1` | `~/.xcoin/regtest` |
| User agent | `XCoin` | | |
| BIP44 type | 3844 | 1 | 1 |

Do **not** reuse Ravencoin ports (8767/8766), magic (`RAVN`/`RVNT`/`CROW`), or
base58 versions (60/`R…`, 111/`n…`).

### Genesis (frozen 2026-09-08)

All three networks use the timestamp string

```
X Coin / XFER: Ravencoin hard-fork, assets kept, lottery not mining. 2026-09-08
```

| | Main | Testnet | Regtest |
| --- | --- | --- | --- |
| `nTime` | `1788825600` | `1537466400` | `1524179366` |
| `nNonce` | 1 | 1 | 1 |
| `nBits` | `0x207fffff` | `0x207fffff` | `0x207fffff` |
| `nVersion` | 4 | 2 | 4 |
| Hash algo | X16R (`GetX16RHash`) | X16R | X16R |

Recorded hashes (asserted in `src/chainparams.cpp`; X16R of the frozen
headers — no leading-zero PoW grind):

| | `hashGenesisBlock` | `hashMerkleRoot` |
| --- | --- | --- |
| Main | `db9bcd7597648d68a0f8f1491e0c068faa626090fab516b355cb70e46e5347d0` | `57622a8eb1e132f766eb4e9df94c6acc963860cefe9bea0d25861ef2d1a92a6e` |
| Testnet | `9a3909c86638c73c5cd3e8921936cbdeb7273524ecb85048caa7c62cfe23429b` | same merkle (same coinbase timestamp string) |
| Regtest | `bfce7bfad8116b82f4a0ce4be2fa52e9c9f166248e318ce45728b8fe87451d89` | same merkle |

`nMinimumChainWork` and `defaultAssumeValid` are zero. Checkpoints are empty.

## Publish a seed node

There are no DNS seeds in-tree. For a private mesh:

1. Pick one always-on host with a public (or VPN) IP. Open **TCP 38443**.
2. Run:

   ```bash
   src/xcoind -listen=1 -port=38443 -server \
     -rpcuser=xcoin -rpcpassword=change-me \
     -xaccount=SeedHandle -xallowlist=/shared/verified-x-accounts.txt
   ```

3. Tell every other operator to join with **either**:

   ```bash
   # xcoin.conf
   addnode=<seed-ip>:38443
   # or, for first contact only:
   seednode=<seed-ip>:38443
   ```

4. Optional hard-code later: add `vSeeds.emplace_back("seed.example.com", false);`
   in `CMainParams` and/or run `contrib/seeds/generate-seeds.py` to refresh
   `chainparamsseeds.h`. Until then, `addnode`/`seednode` is the launch path.

Peers gossip lottery heartbeats (`xhb`) after `verack`. Honest nodes that can
connect to the seed (directly or via the mesh) share one active-node set.
Heartbeats without a verified X link are ignored for that set.

## Verified X allowlist (required for lottery)

Only **X-verified** (blue-check / X Premium verified) accounts may run a
lottery-eligible node. The seed publishes the list; every honest node loads
the same file. Unlinked nodes still sync/relay but do not win or produce.

1. Seed operator: confirm each operator’s X handle is verified, then write
   `verified-x-accounts.txt` (see [LOTTERY.md](LOTTERY.md)).
2. Share that file (scp, gist, HTTPS). Do **not** invent a bot or OAuth app.
3. Each operator sets `xaccount=` to **their** handle and points at the file.

```bash
# xcoin.conf
xaccount=YourHandle
xallowlist=/shared/verified-x-accounts.txt
# or copy into the datadir:
#   ~/.xcoin/verified-x-accounts.txt
```

```bash
src/xcoin-cli addxverified YourHandle      # seed can also add live
src/xcoin-cli listxverified
src/xcoin-cli getlotteryinfo               # local_eligible must be true
```

Optional later refresh (secret-free):

```bash
contrib/xcoin/refresh-x-allowlist.sh https://example.com/verified-x-accounts.txt
src/xcoin-cli loadxverified
```

## Join steps (second machine)

```bash
./autogen.sh
./configure --without-gui --disable-bench --disable-tests --with-incompatible-bdb
make -j$(nproc)
src/xcoind -server -addnode=<seed-ip>:38443 \
  -xaccount=YourHandle -xallowlist=/shared/verified-x-accounts.txt
src/xcoin-cli getblockchaininfo
src/xcoin-cli getlotteryinfo
src/xcoin-cli getactivenodes
```

Wait until `getactivenodes` shows the seed and yourself, each with an
`xaccount`. `getlotteryinfo.local_eligible` must be true or this node will
not produce. On main/test the producer thread emits at most one block per
minute when this node is a winner. A wallet is required to produce
(coinbase script).

Regtest does **not** wait on the clock: use `generatetoaddress`.

## Lottery rewards

See [LOTTERY.md](LOTTERY.md). Short form:

- Slot = 60s. Height `h` uses slot `floor(genesisTime/60) + h`.
- Active = verified-X heartbeat within 180s. Id = `Hash160(payout script)`.
  No linked X account ⇒ not eligible.
- `winnerCount = 1 + floor(height / nSubsidyHalvingInterval)` (main interval
  2,100,000; regtest 150).
- Subsidy 5000 XFER, split as evenly as possible; fees to the first winner.
- Coinbase must contain an `OP_RETURN` `XHB1` commitment of the sorted active
  ids and must pay each winner’s script. Mismatches are invalid.

Asset issue burns (same *amounts* as Ravencoin, new addresses):

| Action | Burn | Main address |
| --- | --- | --- |
| Issue root | 500 XFER | `XissueAssetXXXXXXXXXXXXXXXXXXwTyxt` |
| Reissue | 100 XFER | `XreissueAssetXXXXXXXXXXXXXXXZNfDqa` |
| Unique | 5 XFER | `XissueUniqueAssetXXXXXXXXXXXagKZDZ` |

Coinbase is immature for 100 blocks — generate ~110 on regtest before `issue`.

## Operator ports / firewall

- P2P **38443/tcp** must be reachable on the seed (and on any node you want
  others to dial).
- RPC **38442** is local-only by default (`rpcallowip`). Do not expose it.
- Testnet 48443/48442, regtest 28443/28442.

## Binaries

- `src/xcoind` — node / lottery producer
- `src/xcoin-cli` — RPC
- `src/xcoin-tx` — transaction utility
- `src/qt/xcoin-qt` — GUI (optional; configure without `--without-gui`)

## Known risks (not blockers for a private launch)

- **Sybil:** anonymous/bot processes are kept out of the active set by the
  verified-X allowlist. A producer can still commit an allowlisted set they
  control; the coinbase check proves they paid *that* draw. Fine for a
  trusted operator set; not a public-PoS.
- **Clock skew:** main/test wait for wall-clock slot ≥ height slot.
- **No DNS seeds / explorers / audit** — you are the network.
- Upstream `make check` still hard-codes Ravencoin genesis hashes; do not treat
  a red `make check` as a launch blocker. Use the regtest smoke instead.

## Smoke (regtest)

```bash
contrib/xcoin/smoke-regtest.sh
```

Exercises `getlotteryinfo`, on-demand blocks, a single-winner 5000 XFER
coinbase at height 149, a two-winner 2500 XFER split at height 150 (first
regtest halving), and `issue TESTASSET`.

```bash
contrib/xcoin/smoke-gossip.sh
```

Two regtest nodes: after `addnode`, both `getactivenodes` lists match (P2P `xhb`).

```bash
contrib/xcoin/smoke-eligibility.sh
```

Unlinked node is not eligible; `registeractivenode` is rejected until the
handle is linked **and** allowlisted. Linked+allowlisted identities appear
in the active set.
