# X Coin private launch checklist

Wallet how-to: [README.md](../README.md). Paper: [whitepaper/XCOIN.md](../whitepaper/XCOIN.md).
DEX listing criteria (private; do not submit a listing): [DEX.md](DEX.md).

This is the operator runbook for a **private launch**. There is no public
seed DNS and no exchange listing. Lottery eligibility uses a **shared
verified-X allowlist** (no live X API keys required).

## Ledger, visibility, and how many nodes

Answered from consensus / wallet / P2P code (see [AUDIT.md](AUDIT.md)):

1. **Transactions are visible on this node and to peers on this mesh.
   They are not on a public explorer and not on Ravencoin explorers.**
   GUI **Activity** lists wallet history. RPC: `listtransactions`,
   `gettransaction`, `getrawtransaction`, `getblock`. There is no
   in-tree explorer (`DEFAULT_THIRD_PARTY_BROWSERS` is empty). Peers
   see mempool and blocks over P2P. Different genesis / magic / ports
   mean Ravencoin explorers cannot show XFER.

2. **A lone node already has a ledger.** One `xcoind` / `xcoin-qt`
   stores genesis and can extend the chain (regtest `generatetoaddress`;
   main lottery producer when this node is eligible — 
   `fMiningRequiresPeers` is false). A *network* of other people seeing
   the same tip needs ≥2 peers (`addnode` / `seednode`, port **38443**).
   Lottery among active nodes is more meaningful with more than one
   eligible heartbeat; a lone eligible node still produces.

3. **Own ledger, like Ravencoin has its own.** This is a hard fork with
   its own genesis, UTXO, assets (`XID1`, 32-char roots), and lottery.
   No imported snapshot. Users’ XFER lives here only. Hashes: below.

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

Do **not** reuse the imported v4.8.0 ports, magic, or base58 versions.
Collision table: [FORK.md](FORK.md).

### Genesis (frozen 2026-09-08)

All three networks use the same frozen coinbase timestamp string
(consensus-critical — quoted once in [FORK.md](FORK.md); do not edit it).

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
No imported UTXO, assumevalid hash, or checkpoint is inherited.

**Fair launch:** no IPO, no premine, no founder allocation. Height 0 is not a
payday. The genesis coinbase (5000 XFER in the serialized tx) is **never
added to the UTXO set** (`ConnectBlock` special-case, same as Bitcoin).
Subsidy starts at height 1 via the lottery. Spendable lifetime supply is
**20,999,994,999.727 XFER** (`5000 >> floor(h / 2,100,000)` for `h ≥ 1`,
integer right-shift; ~21 billion minus unpaid genesis and `>>=` dust).
`MAX_MONEY` is 21,000,000,000 XFER (sanity cap, not the minted total).

**Sign in with X is required to send and receive.** Authentication
proves it is you and is the key to asset ownership. Lottery eligibility
additionally requires an X-Verified (allowlisted) handle. A node
without a session can still sync and relay.

## Publish a seed node

There are no DNS seeds in-tree. For a private mesh:

1. Pick one always-on host with a public (or VPN) IP. Open **TCP 38443**.
2. Run:

   ```bash
   src/xcoind -listen=1 -port=38443 -server \
     -rpcuser=xcoin -rpcpassword=change-me \
     -xallowlist=/shared/verified-x-accounts.txt
   ```

3. Tell every other operator to put a **trusted peer IP in the config
   file** (`xcoin.conf`). This is a P2P join address, **not** a BIP39
   seed and **not** a required wallet field. Home / Receive / Send never
   auto-list peer addresses (every `xcoin-qt` already *is* a node). If
   you want to give someone *your* listen address (friend, seed), Home
   has an optional **Provide my node IP** control — off by default;
   uncheck and the IP disappears again.

   ```bash
   # xcoin.conf — operators only
   addnode=<trusted-peer-ip>:38443
   # or, for first contact only:
   seednode=<trusted-peer-ip>:38443
   ```

4. Do **not** add public DNS seeds while this repo is private.
   `addnode`/`seednode` in the config file is the launch path. Hardening
   notes: [SECURITY.md](SECURITY.md).

Peers gossip **signed** lottery heartbeats (`xhb`) after `verack`. Honest
nodes that can connect to the seed (directly or via the mesh) share one
active-node set. Unsigned, unlisted, or sticky-violating heartbeats are
ignored for that set.

## Verified X allowlist (required for lottery)

Only **X-verified** (blue-check / X Premium verified) accounts may run a
lottery-eligible node. The seed publishes the list; every honest node loads
the same file. Unlinked nodes still sync/relay but do not win or produce.

1. Seed operator: confirm each operator’s X handle is verified, then write
   `verified-x-accounts.txt` (see [LOTTERY.md](LOTTERY.md)).
2. Confirm each handle **offline** (open `https://x.com/<handle>`, check the
   verified / Premium badge) for the allowlist. Then each operator **Signs
   in with X** on their node (see [XSIGNIN.md](XSIGNIN.md)). Do not type
   someone else's handle into `linkxaccount`.
3. Share that file (scp, gist, HTTPS). Each operator signs in as
   **their** handle and points at the file.

The owner handle for this private test is **`NFTRVN`**. Add it to the
allowlist, sign in as NFTRVN, then claim the free root `NFTRVN`.

```bash
# xcoin.conf
xoauthclientid=YOUR_CLIENT_ID
xallowlist=/shared/verified-x-accounts.txt
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
  -xallowlist=/shared/verified-x-accounts.txt
src/xcoin-cli getblockchaininfo
src/xcoin-cli getlotteryinfo
src/xcoin-cli getactivenodes
```

Wait until `getactivenodes` shows the seed and yourself, each with an
`xaccount`. `getlotteryinfo.local_eligible` must be true or this node will
not produce. On main/test the producer thread emits at most one block per
minute when this node is a winner. A wallet is required to produce
(coinbase script).

Regtest does **not** wait on the clock: use `generatetoaddress` (regtest-only;
rejected on main/test so RPC cannot print blocks).

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

Assets: see [ASSETS.md](ASSETS.md). Main/root identity assets are **free**
(protocol assignment on verified X-link). Users cannot `issue` a new root.
Sub-asset burn **100 XFER**, unique **5 XFER**. Restricted/qualifier assets
are removed.

| Action | Burn | Main address |
| --- | --- | --- |
| Main / root | 0 (protocol `linkxaccount` / `AssignLinkedUserMainAsset` only) | — |
| Sub | 100 XFER | `XissueSubAssetXXXXXXXXXXXXXXcHkFpF` |
| Reissue | 100 XFER | `XreissueAssetXXXXXXXXXXXXXXXZNfDqa` |
| Unique | 5 XFER | `XissueUniqueAssetXXXXXXXXXXXagKZDZ` |

Coinbase is immature for 100 blocks — generate ~110 on regtest before
`linkxaccount` / `issue` of a sub.

## Operator ports / firewall

- P2P **38443/tcp** must be reachable on the seed (and on any node you want
  others to dial). Prefer `-bind=0.0.0.0:38443` on the seed only.
- RPC **38442** is local-only by default (`rpcallowip`). Cookie or
  `rpcuser`/`rpcpassword` is required; there is no unauthenticated spend.
  Do not expose 38442.
- Seed: `-maxconnections=32` (or lower) so a connection flood cannot
  occupy every inbound slot. Ban junk with `setban` / `-banscore`.
- Testnet 48443/48442, regtest 28443/28442.
- Lone node vs eclipse: `fMiningRequiresPeers` is false so one eligible
  node can produce. Other people seeing the **same** tip still need
  `addnode` to a seed you know. Checkpoints are empty — do not peer with
  strangers on release day. See [SECURITY.md](SECURITY.md).

## Binaries

- `src/xcoind` — node / lottery producer
- `src/xcoin-cli` — RPC
- `src/xcoin-tx` — transaction utility
- `src/qt/xcoin-qt` — GUI (optional; configure without `--without-gui`)

## Known risks (accepted for a private launch)

Nothing is hacker-proof. Details: [SECURITY.md](SECURITY.md).

- **Sybil if the allowlist is skipped or empty:** anyone who can sign a
  payout can heartbeat. The allowlist is operator-shared, not a bonded
  identity. A producer can commit any script set; validation only checks
  the coinbase matches that commitment.
- **xhb:** gossip is compact-signed by the payout key. A live handle cannot
  be rebound. Residual: first-seen after restart unless you pin a payout;
  eclipse of a lone node.
- **Clock skew / catch-up:** main/test wait for wall-clock slot ≥ height slot.
  Height still maps 1:1; no skip-pay or double-pay of a slot in consensus.
- **No DNS seeds / public explorers** — you are the network. Private-test
  audit: [AUDIT.md](AUDIT.md).
- Upstream `make check` still hard-codes imported genesis hashes; do not treat
  a red `make check` as a launch blocker. Use the smokes instead.

## Smoke (regtest)

```bash
contrib/xcoin/smoke-regtest.sh
```

Exercises `getlotteryinfo`, genesis unspendable, on-demand blocks, a
single-winner 5000 XFER coinbase at height 149, a two-winner 2500 XFER split
at height 150, protocol main / sub / unique when `linkxaccount` is present
(including owner handle **NFTRVN**), and **signed-in** `sendtoaddress` /
`sendfromaddress`.

```bash
contrib/xcoin/smoke-gossip.sh
```

Two regtest nodes: after `addnode`, both `getactivenodes` lists match (P2P `xhb`).

```bash
contrib/xcoin/smoke-eligibility.sh
contrib/xcoin/smoke-xsession.sh
contrib/xcoin/smoke-gui.sh
contrib/xcoin/smoke-sabotage.sh
```

Unlinked node is not eligible; `registeractivenode` is rejected until the
handle is linked **and** allowlisted. Linked+allowlisted identities appear
in the active set. `smoke-xsession.sh` proves typed handles cannot send /
receive / claim. `smoke-gui.sh` needs `XDG_RUNTIME_DIR` (the script sets
it); do not `pkill -f xcoin-qt`.

```bash
contrib/xcoin/smoke-benchmark.sh
contrib/xcoin/smoke-isolation.sh
```

Three regtest nodes: lone A already has a ledger; B (signed-in) and C
(unsigned observer) connect; A sends to B; B sees the tx
(`listtransactions` / `gettransaction`); all three share one best block;
C inspects the same tx with `getrawtransaction` / `getblock`.

`smoke-isolation.sh` is two wallets: Bob cannot `sendtoaddress` /
`transfer` Alice’s coins or assets; Alice’s session on Bob’s empty
`wallet.dat` still cannot spend Alice’s UTXOs.
