# X Coin

**A peer-to-peer coin for users on X**

Ticker **XFER**. Subunit **xferon** (1 XFER = 100,000,000 xferons).

**Nifty Raven** (@NFTRVN on X)  
Anonymous public identity — display name and handle only.

---

## Abstract

X Coin is a simple peer-to-peer electronic cash and asset system aimed at
people who already have an identity on X. It is the primary transfer coin
for that use: send value, hold it, and optionally tokenize yourself and
the things you issue.

There is **no mining**. Every minute a lottery among **verified active
nodes** chooses who produces the block and who shares that minute’s
subsidy. On each subsidy **halving**, one more winner is added that
minute (1 → 2 → 3 …).

Launch is **fair**: no premine, no founder allocation, no IPO. Height 0
is not a payday. Spendable lifetime supply is about **21 billion XFER**.

Each verified X handle is assigned **one free root identity asset**.
Users cannot create new roots. Under their root they may issue
**sub-assets** (100 XFER burn) and **uniques** (5 XFER burn). Restricted
assets are removed.

This paper is the product specification. The running wallet is the GUI
(`xcoin-qt`) plus the core node (`xcoind` / `xcoin-cli`). There is no
separate mobile wallet. **Sign in with X** is how a node binds a real
X user id; consensus still does not call X.com on every block.

## 1. Purpose

Most coins optimize for hashpower or for a general smart-contract
machine. X Coin optimizes for one job: **transfer between people who
already live on X**.

That implies:

- A native coin (XFER) that anyone can receive and send without asking
  permission from an issuer.
- A native asset layer so a person can *be* a token and can issue
  children under that identity.
- Block production that does not require ASICs, pools, or a public
  hashrate market.
- Eligibility tied to a **signed-in X user id**, not to a typed handle.

X Coin is not a mining network and not an exchange. It is a private,
fair-launched chain whose first users are operators who already know
each other on X.

## 2. Units and supply

| | |
| --- | --- |
| Name | X Coin |
| Ticker | XFER |
| Subunit | xferon (1e8 per XFER) |
| Address (main) | version 76, `X…` |
| Config / datadir | `xcoin.conf` / `~/.xcoin` |
| P2P / RPC (main) | 38443 / 38442 |

Subsidy at height 1 is **5000 XFER**, halved every 2,100,000 blocks
(every 150 blocks on regtest). Height 0 pays nothing; the genesis
coinbase is never added to the UTXO set.

Winner count that minute is `1 + floor(height / halvingInterval)`.
The subsidy is split as evenly as possible across those winners; fees
go to the first winner.

Integer right-shifts eventually dust the last units. Spendable lifetime
supply is **20,999,994,999.727 XFER** — about 21 billion, minus unpaid
genesis and shift dust. `MAX_MONEY` is a 21 billion XFER sanity cap, not
an extra mint.

Fair launch: no premine, no founder output, no hidden allocation. The
people who produce blocks are the people who run eligible nodes that
minute.

## 3. Lottery, not mining

Proof-of-Work is removed as the production mechanism. `CheckProofOfWork`
does not grind. There is no `-gen` miner.

### Slot

Height `h` maps to lottery minute

```
slot = floor(genesisTime / 60) + h
```

One height, one slot, one coinbase. You cannot skip-pay or double-pay a
height on one chain.

### Active set

A node is **active** if it has heartbeated a verified X identity within
the last 180 seconds. Node id is `Hash160(payout script)`. Peers gossip
heartbeats (`xhb`) after `verack`. Heartbeats without a verified link
are ignored.

### Seed and winners

```
seed = SHA256( prevBlockHash || LE64(slot) )
winnerCount(h) = 1 + floor(h / nSubsidyHalvingInterval)
```

Winners are drawn from the sorted active ids with a partial
Fisher–Yates shuffle driven by `SHA256(seed || LE32(i))`. The first
selected winner may produce the block.

The coinbase must:

1. Pay each winner’s script, in selection order, the correct split of
   the subsidy (plus fees on the first output).
2. Include an `OP_RETURN` commitment `XHB1` of the sorted active ids
   used for the draw.

A mismatch is an invalid block. Empty committed set is invalid.

On main and testnet the producer thread waits until wall-clock slot ≥
height slot and emits at most one block per minute when this node wins.
On regtest, `generatetoaddress` assembles a block on demand (still no
hashing) so tests do not wait on the clock.

Full algorithm: [docs/LOTTERY.md](../docs/LOTTERY.md).

## 4. Sign in with X (what stops impersonation)

Consensus still does not call X.com on every block. **X Verified** is
what X itself reports on `GET /2/users/me` (`verified` /
`verified_type`): the blue check / X Premium, plus business and
government org checks.
[About the blue check](https://help.x.com/en/managing-your-account/about-x-bluecheck)
per [X’s verification policy](https://help.x.com/en/rules-and-policies/verification-policy).
The operator invite list (`addxverified`) is **not** X Verified.

**Signing in is what stops impersonation.** A typed string in
`linkxaccount` or `-xaccount=` is not enough. Anyone could type another
person's handle if that handle were only checked against an allowlist.

The GUI **Sign in with X** button runs OAuth 2.0 PKCE (authorization
code). After the loopback callback, the wallet calls
`GET /2/users/me?user.fields=verified,verified_type` and stores **that**
response: X user id, username, expiry, `verified`, `verified_type`, and
an HMAC proof (`xsession.json` + datadir secret
`xsession.key`). **Send and receive** require that proof — authentication
is what proves the wallet is yours. Home shows **this wallet is linked
to @handle** plus the X user id, and whether X Verified is true.
`linkxaccount otherperson` is
rejected when the session is not `otherperson`. This proof does not
replace the 12-word BIP39 seed.

Lottery eligibility is **session plus X Verified** (`verified==true`)
plus a running wallet. Unverified accounts have **zero chance**. The
invite list cannot exclude a verified wallet:

1. A valid Sign in with X session on this node (user id + username).
2. `users/me` reports X Verified (blue / business / government).
3. This wallet is running (local payout + heartbeat).

Typed `-xaccount=` is ignored unless a session already matches it.

Setup: [docs/XSIGNIN.md](../docs/XSIGNIN.md). Example owner handle:
**NFTRVN**.

## 5. Assets: tokenize the world, and yourself

The user **is** a main asset.

When a verified handle is linked, the protocol assigns **one free
root**. Users cannot `issue` a new root. One X account → one main
asset. The assignment is a zero-burn transaction with `OP_RETURN`
`XID1` plus the normalized handle.

The root name is the handle, uppercased, in `A-Z 0-9 . _` (length
3–**32**). A 26-character X handle maps 1:1 (no truncation).
`NFTRVN` → asset `NFTRVN` plus owner token `NFTRVN!`.

Under a root the owner may issue:

| Kind | Example | Burn |
| --- | --- | --- |
| Sub | `NFTRVN/NOTE` | 100 XFER |
| Unique | `NFTRVN#ONE` | 5 XFER |
| Reissue (if reissuable) | same name | 100 XFER |

Identity roots are not reissuable. Restricted, qualifier, tag, and
freeze assets are **removed** — no create, transfer, RPC, or
activation.

A signed-in session is required to create a receive address or send.
Coins can still arrive at an already-known address. Linking is
required to *issue* under your root.

Details: [docs/ASSETS.md](../docs/ASSETS.md).

## 6. Network identity

| | Main | Testnet | Regtest |
| --- | --- | --- | --- |
| Magic | `XFER` | `XFTN` | `XFRT` |
| P2P | 38443 | 48443 | 28443 |
| RPC | 38442 | 48442 | 28442 |
| Data dir (Unix) | `~/.xcoin` | `~/.xcoin/testnet1` | `~/.xcoin/regtest` |

There are no public DNS seeds in-tree. A private mesh uses `addnode` /
`seednode`. Do not publish this repository.

This is **X Coin’s own ledger** (own genesis, UTXO, assets, lottery) —
not Ravencoin’s chain and not an imported snapshot. A lone node already
stores it. Other people seeing the same tip need two or more peers on
port 38443. Wallet **Activity** / `listtransactions` show sends and
receives on this node; peers see mempool and blocks. There is no public
explorer and nothing appears on Ravencoin explorers.

User agent is `XCoin`. Signed messages use `X Coin Signed Message:\n`.

## 7. The wallet (honest status)

The wallet **is** the core wallet that ships with the node:

- `xcoind` — daemon, lottery producer, wallet
- `xcoin-cli` — RPC
- `xcoin-qt` — desktop GUI (X theme: black / white / sharp). This is
  the 1.0 wallet. CLI remains supported.

There is **no** new mobile wallet and **no** X-app wallet. Sign in with
X is OAuth in `xcoin-qt` / the node (PKCE → `users/me` → datadir proof).
It is an identity gate on this node, not a hosted X.com wallet and not
a replacement for the seed.

**12-word BIP39 / BIP44 generation is unchanged** and still required
to create or restore keys. Seed = keys. X session = proof that this
datadir is linked to that X account. [docs/WALLET.md](../docs/WALLET.md).

Creating an address, sending XFER, and linking a handle are RPC or Qt
operations against the core `wallet.dat` in `~/.xcoin`. Send and
receive require the session proof.

What this release changed on top of that core:

- Product name, ports, magic, datadir, binaries
- Lottery instead of mining
- Sign in with X session + X Verified (blue check) and `linkxaccount`
- One free root per handle; user roots forbidden; restricted assets
  removed

What is still inherited core behavior: UTXO wallet, BIP39 12-word HD
seed, `wallet.dat`, `encryptwallet` / `backupwallet`, fee estimates,
P2P, mempool, the asset script format. How to use it:
[README.md](../README.md).

## 8. Risks (accepted for private launch)

- Unverified `users/me` has zero lottery chance; a verified running
  wallet cannot be excluded. Gossip `xVerified` is compact-signed by
  the payout key; a modified client can still lie.
- `xhb` carries a handle, not a signature binding that handle to a
  script.
- Clock skew can delay a slot; height still maps 1:1.
- No public explorer, no DNS seeds, no exchange listing.
  Private-test audit: [docs/AUDIT.md](../docs/AUDIT.md).

Treat the lottery as specified here. Do not reintroduce Proof-of-Work
as the production path.

## 9. Author

**Nifty Raven** — @NFTRVN on X.

This is an anonymous public identity: display name and handle only.
There is no legal name, email, employer, or phone attached to this
release.

## 10. License

MIT. See [COPYING](../COPYING). Consensus code descends from open
upstream; copyright headers in source files still apply. Historical
opcode names and legal notes: [docs/FORK.md](../docs/FORK.md).
