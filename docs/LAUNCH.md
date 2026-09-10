# X Coin private launch checklist

Wallet how-to: [README.md](../README.md). Paper: [whitepaper/XCOIN.md](../whitepaper/XCOIN.md).
DEX listing criteria (private; do not submit a listing): [DEX.md](DEX.md).

This is the operator runbook for a **private launch**. There is no public
seed DNS and no exchange listing. Lottery eligibility uses **Sign in with X** plus **X Verified** (blue
check from `users/me`). An optional shared invite / payout-pin file is
not a lottery gate.

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

### Genesis (frozen)

All three networks use the same frozen coinbase timestamp string
(consensus-critical — quoted once in [FORK.md](FORK.md); do not edit it).
**Main `nTime` is `1789197360`** (2026-09-12 03:16:00 America/New_York,
John 3:16). [GO-LIVE.md](GO-LIVE.md).

| | Main | Testnet | Regtest |
| --- | --- | --- | --- |
| `nTime` | `1789197360` | `1537466400` | `1524179366` |
| `nNonce` | 1 | 1 | 1 |
| `nBits` | `0x207fffff` | `0x207fffff` | `0x207fffff` |
| `nVersion` | 4 | 2 | 4 |
| Hash algo | X16R (`GetX16RHash`) | X16R | X16R |

Recorded hashes (asserted in `src/chainparams.cpp`; X16R of the frozen
headers — no leading-zero PoW grind):

| | `hashGenesisBlock` | `hashMerkleRoot` |
| --- | --- | --- |
| Main | `7c790cdb7a233c020cb71346082709a185eb6577643a327349112d425a712beb` | `bacf268e26e66e3c7c3ac634652bc7765c5f6d97c796fe1b288d16b9f0e4b0b5` |
| Testnet | `f30becee764cae209a8c7f00e0bd2de34b7ef4645d7b776dbd066c8d0600c002` | same merkle (same coinbase timestamp string) |
| Regtest | `1dc800dace1cc1222e03ba3c7852f60d379539074bba923599d0458aeba2b2e4` | same merkle |

`nMinimumChainWork` and `defaultAssumeValid` are zero. Checkpoints are empty.
No imported UTXO, assumevalid hash, or checkpoint is inherited.

**Birth of the chain:** main genesis `nTime` is **1789197360**
(12 September 2026 around 3:16 AM ET, John 3:16). Start the first
X Verified node on launch night. Height 0 is that header; height 1
is the first payday. Connecting a peer later does not rewrite
genesis. MAIN emits at most one block per wall-clock minute
(wall-minute latch). There is no slot+120 abort and no two-hour
backfill refuse. Full order: [GO-LIVE.md](GO-LIVE.md).
Third-party explorers/wallets: [THIRD-PARTY.md](THIRD-PARTY.md).

**Fair launch:** no IPO, no premine, no founder allocation. Height 0 is not a
payday. The genesis coinbase (5000 XFER in the serialized tx) is **never
added to the UTXO set** (`ConnectBlock` special-case, same as Bitcoin).
Subsidy starts at height 1 via the lottery. Spendable lifetime supply is
**20,999,994,999.727 XFER** (`5000 >> floor(h / 2,100,000)` for `h ≥ 1`,
integer right-shift; ~21 billion minus unpaid genesis and `>>=` dust).
`MAX_MONEY` is 21,000,000,000 XFER (sanity cap, not the minted total).

**Sign in with X is required to send, receive, and create assets.**
Authentication proves it is you. **No session → no main/root asset.**
Subs and uniques are issued under that signed-in account’s root.
Lottery eligibility additionally requires **X Verified** (X’s blue
check from `users/me`) and a running wallet. Unverified accounts have
zero chance. The operator invite list cannot exclude a verified wallet.
A node without a session can still sync and relay.

## Publish a seed node

There are no DNS seeds in-tree. The operator's running wallet **is**
the first node and the seed:

1. On launch night, double-click **X Coin Wallet**. Opening it starts
   the node and listens on **TCP 38443**. Keep it running.
2. Home says you are the first node. Listen is on. **Provide my node
   IP** is off by default; turn it on to see `host:38443`. That is the
   address others must use. It does not publish the IP by itself.
3. Packaged `xcoin.conf` already has `addnode=172.191.195.221:38443`
   and `seednode=172.191.195.221:38443`. New wallets read that file
   automatically. No terminal. Users do not edit a conf file. Do not
   invent a host. Do not add public DNS seeds.

   Home never lists other people's IPs. Datadir `xcoin.conf`
   `addnode=` still works if someone already has one.

4. Then open the GitHub repo so people download those packages.

Peers gossip **signed** lottery heartbeats (`xhb`) after `verack` (handle
only; user id **0** on the wire). Honest nodes that can connect to the seed
(directly or via the mesh) share one active-node set. Unsigned, unverified,
or sticky-violating heartbeats are ignored for that set.

## X Verified fair lottery

**X Verified** is X’s blue check / X Premium (and org checks) from
`GET /2/users/me`. Official meaning:
[About the blue check](https://help.x.com/en/managing-your-account/about-x-bluecheck)
per [X’s verification policy](https://help.x.com/en/rules-and-policies/verification-policy).
`addxverified` is an optional private-mesh **invite / pin list**, not X Verified
and not a lottery gate.

Only **X Verified** accounts with a running wallet enter the lottery.
Unlinked or unverified (no blue check) nodes still sync/relay but have
**zero chance** to win or produce. Fair code: a verified running wallet
cannot be excluded.

1. Each operator **Signs in with X** so `users/me.verified` is stored
   with the session proof (see [XSIGNIN.md](XSIGNIN.md)).
2. Optionally share an invite list (`verified-x-accounts.txt`) for
   payout pins. Confirming a public profile badge is a human check for
   whom to invite — it does not make the node report X Verified and
   cannot drop a blue-check wallet from the draw.
3. Do not type someone else's handle into `linkxaccount`.

The owner handle for this private test is **`NFTRVN`**. Sign in as
NFTRVN (regtest: `-xoauthmock=NFTRVN` mocks verified=true), then claim
the free root `NFTRVN`. Adding NFTRVN to the invite list is optional
pins, not eligibility.

```bash
# packaged xcoin.conf already bakes the operator Client ID and seed
# xoauthclientid=…   (do not invent one; users never paste it)
# addnode=172.191.195.221:38443
# seednode=172.191.195.221:38443
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

Download the Windows or Linux wallet from Releases (not **Code →
Download ZIP**). Double-click **X Coin Wallet**. If the package
`xcoin.conf` has `addnode=<host>:38443`, you connect with no terminal.

Sign in with X. Developers who are compiling (not the launch-night
path): [INSTALL.md](../INSTALL.md).

Wait until Home shows a connected peer (or `getactivenodes` shows the
seed and yourself, each with an `xaccount`). Lottery needs **X Verified**
and a running wallet. `getlotteryinfo.local_eligible` must be true or
this node will not produce.

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
(protocol assignment on a **signed-in** X-link). No session → no main
asset. Users cannot `issue` a new root. Subs/uniques are issued under
that account’s root. Sub-asset burn **100 XFER**, unique **5 XFER**.
Restricted/qualifier assets are removed.

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
- `src/qt/xcoin-qt` — GUI (1.0.x; configure `--with-gui=qt5`)

## Known risks (accepted for a private launch)

Nothing is hacker-proof. Details: [SECURITY.md](SECURITY.md).

- **Sybil:** anyone who can present X Verified (`users/me.verified`)
  plus a compact-signed payout can heartbeat. Unverified accounts have
  zero chance. The invite list cannot exclude a verified running wallet.
  A producer can commit any script set; validation only checks
  the coinbase matches that commitment.
- **xhb:** gossip is compact-signed by the payout key. A live handle cannot
  be rebound. Residual: first-seen after restart unless you pin a payout;
  eclipse of a lone node.
- **Clock skew / catch-up:** MAIN wall-minute latch (≤1 block/min).
  Testnet waits for wall-clock slot ≥ height slot. Height still maps
  1:1; no skip-pay or double-pay of a slot in consensus. No slot+120
  abort.
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
contrib/xcoin/smoke-package-conf.sh
```

Two regtest nodes: after `addnode`, both `getactivenodes` lists match (P2P `xhb`).

```bash
contrib/xcoin/smoke-eligibility.sh
contrib/xcoin/smoke-xsession.sh
contrib/xcoin/smoke-gui.sh
contrib/xcoin/smoke-sabotage.sh
```

Unlinked node is not eligible; signed-in but not X Verified can send and
receive, not lottery (zero chance). `registeractivenode` is rejected until
the session is X Verified (blue check). A verified running wallet cannot
be excluded by the invite list. `smoke-xsession.sh` proves typed handles cannot send /
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
