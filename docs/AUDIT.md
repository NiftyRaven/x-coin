# X Coin (XFER) private-test audit

**Nifty Raven** (@NFTRVN on X)  
Anonymous public identity — display name and handle only.

This is a **private** full-test audit. There is no public explorer and
no public DNS seed. Do not publish this repository.

## Owner questions (from code)

### 1. Are transactions visible?

**Yes on this node. Yes to peers on this private mesh. No on Ravencoin explorers. No in-tree block explorer.**

| Surface | What a user sees |
| --- | --- |
| GUI Activity tab | Wallet history (`RavenGUI` action `&Activity` → `TransactionTableModel`) |
| `listtransactions` / `gettransaction` | Wallet send/receive, including category and confirmations |
| `getrawtransaction` / `getblock` | Explorer-less node RPC (any peer with `-txindex` can inspect any tx; without it, mempool + wallet txs) |
| Public explorer | **None.** `DEFAULT_THIRD_PARTY_BROWSERS` is empty. Docs do not invent one. |
| Ravencoin explorers | **Never.** Different genesis, magic `XFER`, ports, and address versions. |

A signed-in wallet sees its own sends and receives. Connected peers see
the same mempool and blocks (inventory + `getdata`). An unsigned node
still syncs and can inspect `getblock` / `getrawtransaction`; it cannot
create a receive address or send.

Proof: `contrib/xcoin/smoke-benchmark.sh` (A sends to B; B
`listtransactions` / `gettransaction`; unsigned C sees mempool + block).

### 2. Does the chain only exist if there is more than one node?

**No. A lone node already has a ledger. A network of other people
requires two or more peers.**

- One `xcoind` / `xcoin-qt` writes the **frozen** genesis to its datadir
  and can extend the chain. Main genesis `nTime` is **1788825600** and
  stays that value. Do not re-run `freeze-genesis.sh` at go-live.
  Height 1 is the first payday. MAIN emits at most one block per
  wall-clock minute. Regtest:
  `generatetoaddress` (on-demand). Main / test: the lottery producer
  emits a block when this node is eligible and is a winner
  (`fMiningRequiresPeers = false`). Height 0 is stored even if nobody
  else is online.
- Other people seeing **the same** chain requires P2P: `addnode` /
  `seednode`, port **38443**, magic `XFER`. That is a mesh, not a
  condition for the ledger to exist.
- The lottery among *active* nodes is more meaningful with more than
  one eligible heartbeat. A lone eligible node still produces (it is
  the only winner).

Do not claim a lone node has no ledger.

### 3. Just like Ravencoin — its own ledger?

**Yes. This is a hard fork with its own genesis, UTXO, assets, and
lottery. It is not Ravencoin’s chain and does not import a snapshot.**

| | X Coin | Imported v4.8.0 network |
| --- | --- | --- |
| Genesis timestamp | `X Coin / XFER: Ravencoin hard-fork, assets kept, lottery not mining. 2026-09-08` | Different |
| Main `hashGenesisBlock` | `db9bcd7597648d68a0f8f1491e0c068faa626090fab516b355cb70e46e5347d0` | Different |
| Test | `9a3909c86638c73c5cd3e8921936cbdeb7273524ecb85048caa7c62cfe23429b` | Different |
| Regtest | `bfce7bfad8116b82f4a0ce4be2fa52e9c9f166248e318ce45728b8fe87451d89` | Different |
| Merkle (all three) | `57622a8eb1e132f766eb4e9df94c6acc963860cefe9bea0d25861ef2d1a92a6e` | Different |
| Magic / P2P | `XFER` / **38443** | `RAVN` / 8767 |
| Main address | version 76 `X…` | version 60 `R…` |
| Test / regtest address | version 140 `y…` | version 111 `n…` |
| Production | Minute lottery, no PoW miner | PoW |
| Assets | 32-char roots, free `XID1` identity, no user roots | 30-char roots, user `issue` |

`ConnectBlock` skips the genesis coinbase (unspendable; same Bitcoin
special-case). `nMinimumChainWork` and `defaultAssumeValid` are zero;
checkpoints are empty. Users’ XFER lives on **this** UTXO set only.

Replay into Ravencoin fails: different genesis, different magic, and
different address prefixes (`X` / `y` vs `R` / `n`).

## Consensus

- **Subsidy:** 0 at height 0; 5000 XFER at height 1; `>>=` each
  `nSubsidyHalvingInterval` (2,100,000 main/test, 150 regtest).
  Spendable lifetime **20,999,994,999.727 XFER**.
- **Lottery:** `src/lottery.{h,cpp}`. Coinbase must pay winners and
  commit `XHB1`. Lottery pools (`src/pool`, `XPL1`, `xpl`) were
  removed. Empty committed set is invalid.
- **No PoW miner:** `CheckProofOfWork` always returns true. `-gen` /
  `setgenerate` removed. `generatetoaddress` is **regtest-only**.
- **Assets:** protocol `XID1` root on **signed-in** X-link (`linkxaccount`);
  no session → no main asset. User `issue` of a new root is invalid.
  Subs/uniques only under that account’s `NAME!`. Restricted assets stay off.
- **Sign in with X:** send / receive / own require a session.
  Lottery requires X Verified (blue check) plus a running wallet.
  Unverified = zero chance. Invite list cannot exclude a verified wallet.
  Session ≠ BIP39 seed.

## P2P

- No DNS seeds in-tree (`vSeeds.clear()`). Leftover imported IP arrays
  in `chainparamsseeds.h` are empty and unused. Do not add public seeds.
- Join path: `addnode=` / `seednode=` on **38443** (operator config).
  Home may optionally show *your* listen address (**Provide my node IP**,
  off by default). It never lists other people's IPs.
- Heartbeats: signed `xhb` after `verack` (handle + user id **0**).
  Unsigned, unverified, or sticky-violating heartbeats are ignored (or
  banned, if the compact sig is missing / forged). See [SECURITY.md](SECURITY.md).

## Wallet / privacy

- First-run BIP39 12-word still creates keys. OAuth does not replace
  the seed. `xsession.json` is identity proof, not a key.
- Spend only keys in **this** `wallet.dat`. Another signed-in identity
  cannot send from inside your wallet. Signing in as @alice on Bob's
  empty wallet does not import Alice's UTXOs. Proof:
  `contrib/xcoin/smoke-isolation.sh`.
- Txs on this mesh are visible to every connected peer (normal UTXO
  gossip). They are not on Ravencoin explorers.

## Verdict

**Ready for a private full test** among operators who share a **known**
seed IP. An invite / payout-pin file is optional (not a lottery gate).
Not ready for a public launch. Not hacker-proof. Threat model:
[SECURITY.md](SECURITY.md).

Third parties can stand up a **block explorer** or **asset explorer**
against `-txindex=1` `xcoind` (`getblock`, `getrawtransaction`,
`listassets`, `getassetdata`). A third-party **wallet** can use the same
send/issue RPC; it still cannot mint a main asset without Sign in with X
on that node. No in-tree explorer URL until the owner publishes one.

Hardened (was a real gap; now closed for the cheap cases):

- Gossip `xhb` must be signed by the payout key; a live handle cannot be
  rebound to an attacker script.
- Invite-list match is the handle (optional pins). A listed userid alone
  cannot authorize a different handle.
- `sendrawtransaction` requires the same session as `sendtoaddress`.

Accepted residual (not blockers for a private test):

- Eclipse / lone-node: `fMiningRequiresPeers` is false; checkpoints empty.
  Connect to a seed you know (`addnode`).
- First-seen handle grab after restart unless the payout is pinned.
- Producer commits the active set; validation checks the coinbase against
  that commitment, not an oracle of “who is really online.”
- No public explorer URL until go-live. RPC is enough for third-party
  explorers/wallets: [THIRD-PARTY.md](THIRD-PARTY.md). Go-live:
  [GO-LIVE.md](GO-LIVE.md).
- Upstream `make check` still hard-codes imported genesis hashes.

## Smokes

```bash
contrib/xcoin/smoke-xsession.sh
contrib/xcoin/smoke-eligibility.sh
contrib/xcoin/smoke-regtest.sh
contrib/xcoin/smoke-gossip.sh
contrib/xcoin/smoke-gui.sh          # sets XDG_RUNTIME_DIR; do not pkill -f xcoin-qt
contrib/xcoin/smoke-benchmark.sh    # lone ledger + 3-node visible send
contrib/xcoin/smoke-isolation.sh    # two wallets: Bob cannot spend Alice
contrib/xcoin/smoke-sabotage.sh      # unsigned sendraw / handle steal rejected
```
