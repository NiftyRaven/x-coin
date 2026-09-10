# X Coin

**Ticker XFER.** Subunit **xferon** (1 XFER = 100,000,000 xferons).

Nifty Raven (@NFTRVN on X)

September 2026

---

## Abstract

X Coin is a peer-to-peer electronic cash and native-asset ledger for
people who already have an identity on X. It is designed for one job:
transfer value, hold it, and optionally tokenize the holder and what
they issue.

There is no mining. Block production is a **minute lottery** among
nodes that are signed in as **X Verified** — X’s blue check / X Premium
(and business or government organization checks) as reported by
`GET /2/users/me`. Unverified accounts have no lottery chance. Each
subsidy halving adds one more winner that minute.

Launch is a **fair launch**: no premine, no founder allocation, no IPO.
Height 0 is genesis, not a payday. Spendable lifetime supply is about
**21 billion XFER**.

Sign in with X is required to send, receive, and claim the **one free
root identity asset** assigned to each signed-in X account. The
12-word BIP39 seed that creates wallet keys is unchanged and is not
replaced by that sign-in.

This paper is the product specification. Mechanics that operators and
implementers need in full are in the technical annexes cited at the
end.

## 1. Motivation

Most public coins optimize for hashpower or for a general
smart-contract machine. X Coin optimizes for transfer between people
who already live on X.

That implies four design choices:

1. A native coin (XFER) that anyone with a signed-in wallet can receive
   and send without asking an issuer.
2. A native asset layer in which a person *is* a token and can issue
   children under that identity.
3. Block production that does not require ASICs, mining pools, or a
   public hashrate market.
4. Lottery eligibility tied to a **signed-in X Verified account**, not
   to a typed handle and not to an operator invite list.

X Coin is not an exchange and not a hosted wallet on X.com. It is its
own UTXO ledger. As of this writing the chain remains private until
**12 September 2026**.

## 2. Design at a glance

| | |
| --- | --- |
| Name | X Coin |
| Ticker | XFER |
| Subunit | xferon (1e8 per XFER) |
| Production | Minute lottery. No Proof-of-Work. |
| Eligible set | X Verified, signed-in, running nodes |
| Halvings | Subsidy halves; winner count that minute increases by one |
| Launch | Fair. No premine. No founder allocation. |
| Spendable supply | 20,999,994,999.727 XFER (~21 billion) |
| Identity | Sign in with X. One free root per signed-in account. |
| Keys | 12-word BIP39 / BIP44. Sign-in does not replace the seed. |
| Addresses (main) | Version 76, `X…` |
| P2P / RPC (main) | 38443 / 38442 |
| DNS seeds | None. Peers join with `addnode` / `seednode`. |
| Wallets | Desktop GUI on Linux and Windows (`xcoin-qt`). Practice is always `-regtest`. |
| License | MIT |

Consensus does not call X.com on every block. Sign in with X binds a
real X account to a node’s data directory once. After that, lottery
gossip carries the public `@handle` only.

## 3. Fair launch and supply

X Coin starts with an empty spendable set.

**Height 0** is genesis. The genesis coinbase is serialized in the
block and is never added to the UTXO set. It is not a payday, not a
hidden allocation, and not a founder output. **Height 1** is the first
subsidy: 5000 XFER, produced in the first lottery minute after genesis
when an X Verified eligible node is running.

The genesis timestamp is the lottery clock. Height `h` maps to minute

```
slot = floor(genesisTime / 60) + h
```

Explorers will show that timestamp as the birth of the chain. Connecting
a node does not rewrite height 0.

Subsidy is halved every **2,100,000** blocks (about four years at one
block per minute; every 150 blocks on regtest). Integer right-shifts
eventually dust the last units. Spendable lifetime supply is
**20,999,994,999.727 XFER** — about 21 billion, minus unpaid genesis
and shift dust. `MAX_MONEY` is a 21 billion XFER sanity cap used by
validation. It is not an extra mint.

There is no premine, no founder allocation, and no operator-reserved
supply. Coins that exist are coins that lottery winners produced after
height 0, plus whatever later holders transferred.

This paper does not give investment, legal, or tax advice. XFER is a
protocol unit. Whether, where, or how it is listed is outside the
ledger.

## 4. Lottery, not mining

Proof-of-Work is not the production mechanism. `CheckProofOfWork` does
not grind. There is no hash-rate market and no `-gen` miner.

Full algorithm: [docs/LOTTERY.md](../docs/LOTTERY.md).

### Slot and subsidy

One height, one slot, one coinbase. A chain cannot skip-pay or
double-pay a height.

Winner count that minute is

```
winnerCount(h) = 1 + floor(h / nSubsidyHalvingInterval)
```

On mainnet that is one winner through height 2,099,999; two winners
from 2,100,000; three from 4,200,000; and so on. If the active set is
smaller than `winnerCount`, every active node wins. The subsidy is
split as evenly as possible across those winners; transaction fees go
to the first winner.

### Active set

A node is **active** if it has heartbeated an **X Verified** identity
within the last 180 seconds. Node id is `Hash160` of the payout script.
Peers gossip compact-signed heartbeats (`xhb`) after `verack` and about
every 30 seconds: timestamp, payout script, `@handle`, user id **0**,
verified bit, and a compact payout signature.

The numeric X user id and OAuth tokens never go on the wire. Heartbeats
that are unsigned, unverified, or that try to rebind a live handle are
ignored.

**X Verified** is what X itself reports on `GET /2/users/me`
(`verified` / `verified_type`): the blue check / X Premium, plus
business and government organization checks.
[About the blue check](https://help.x.com/en/managing-your-account/about-x-bluecheck)
per [X’s verification policy](https://help.x.com/en/rules-and-policies/verification-policy).
It is not an operator invite list.

Lottery eligibility on this node is all three of:

1. A valid Sign in with X session (user id + username + proof).
2. That session reports X Verified (`verified == true`).
3. This wallet is running (local payout + heartbeat).

A signed-in handle that is not X Verified can still send and receive.
It has **zero lottery chance**. A verified running wallet cannot be
excluded from the draw.

### Seed and winners

```
seed = SHA256( prevBlockHash || LE64(slot) )
```

Winners are drawn from the sorted active ids with a partial
Fisher–Yates shuffle driven by `SHA256(seed || LE32(i))`. The first
selected winner may produce the block. Honest nodes that agree on the
tip and the slot therefore agree on the draw.

The coinbase must:

1. Include an `OP_RETURN` commitment `XHB1` of the sorted active ids
   used for the draw.
2. Pay the draw: one output per winner, in selection order, the
   correct split of the subsidy (plus fees on the first output).

A mismatch is an invalid block. An empty committed set is invalid.
Lottery pools (`XPL1`, create / join / leave) were removed. One
lottery. One coinbase per height.

On mainnet the producer emits at most one block per wall-clock minute
when this node wins (catch-up may target an older slot; the latch
still holds). There is no slot+120 abort. On testnet the producer
waits until wall-clock slot ≥ height slot. On regtest,
`generatetoaddress` assembles a block on demand (still without
hashing) so tests do not wait on the clock.

## 5. Identity: Sign in with X and the seed

X Coin uses two layers. Both are required for a working wallet. Neither
substitutes for the other.

| Layer | What it is | What it is not |
| --- | --- | --- |
| 12-word BIP39 seed | Private keys in `wallet.dat`. Unchanged 12-word generation (BIP44, mainnet coin type **3844**). | Not an X login. Not replaced by Sign in with X. |
| Sign in with X | Proof that this data directory is linked to one X account. | Not a hosted X.com wallet. Not a replacement for the seed. |

Setup: [docs/XSIGNIN.md](../docs/XSIGNIN.md). Keys: [docs/WALLET.md](../docs/WALLET.md).

**Signing in is what stops impersonation.** A typed string is not an
identity. Anyone could type another person’s handle if that handle were
only checked against a list.

The GUI **Sign in with X** button runs OAuth 2.0 PKCE (authorization
code) against a loopback callback. After approval, the wallet calls
`GET /2/users/me?user.fields=verified,verified_type` and stores **that**
response: X user id, username, expiry, `verified`, `verified_type`, and
an HMAC proof (`xsession.json` plus a per-datadir secret
`xsession.key`). Access tokens are held in RAM for that `users/me` call
and then wiped. They are never written to disk.

Users never paste a Client ID. The operator bakes `xoauthclientid=`
into the shipped package `xcoin.conf` once. An empty Client ID does not
fake success; Sign in with X tells the user the operator has not baked
one. There is no GUI paste box for it.

The GUI shows **@handle** and whether X Verified is true. It does not
show the X user id, tokens, or session file paths. `linkxaccount` for
another person’s handle is rejected when the session is not that
person. Typed `-xaccount=` is ignored unless a session already matches
it.

A valid session proof is required to create a receive address, send,
claim the free root, and issue under that root. Coins can still arrive
at an already-known address. RPC still needs the cookie or
`rpcpassword`; sign-in is not a substitute for RPC authentication.

A copy of this data directory (`xsession.key` and `xsession.json`) is
a copy of **this node’s session**, not of another wallet’s keys. A
copy of `wallet.dat` or of the 12 words is a copy of the keys. The
HMAC is per-datadir. Signing in as Alice on Bob’s empty wallet does
not import Alice’s coins.

## 6. Assets

The user **is** a main asset.

A wallet without Sign in with X cannot create a main (root) asset.
`issue` of a new root is rejected at RPC and at consensus. The only
valid root is the protocol assignment after a valid session: zero burn,
`OP_RETURN` `XID1` plus the normalized handle. That assignment does
**not** require a blue check. One X account → one main asset. Lottery
still requires X Verified.

The dummy prevout used to claim an empty-wallet root is not a spendable
coin and is not a premine.

The root name is the handle, uppercased, in `A-Z 0-9 . _`, up to
**32** characters. A 26-character X handle maps 1:1 and is never
truncated. Short handles are padded; a leading or trailing `_` is
substituted so it remains a valid asset name without colliding with the
stripped form. Reserved product names (`XFER`, `XCOIN`) are not
assignable.

Under that root the owner may issue sub-assets and uniques. They must
own `NAME!` and hold the same session. Users cannot invent a second
main.

| Kind | Example form | Burn |
| --- | --- | --- |
| Root (protocol on X-link) | `HANDLE` + owner token `HANDLE!` | 0 XFER |
| Sub | `HANDLE/CHILD` | 100 XFER |
| Unique | `HANDLE#tag` | 5 XFER |
| Reissue (if reissuable) | same name | 100 XFER |

Identity roots are not reissuable. Restricted, qualifier, tag, and
freeze assets are **removed** — no create, transfer, RPC, or
activation.

Details: [docs/ASSETS.md](../docs/ASSETS.md).

## 7. Network

This is X Coin’s own ledger: own genesis, own UTXO set, own assets,
own lottery. It is not the imported upstream chain and not a snapshot
of that chain.

| | Main | Testnet | Regtest |
| --- | --- | --- | --- |
| Magic | `XFER` | `XFTN` | `XFRT` |
| P2P | 38443 | 48443 | 28443 |
| RPC | 38442 | 48442 | 28442 |
| Data dir (Unix) | `~/.xcoin` | `~/.xcoin/testnet1` | `~/.xcoin/regtest` |

There are no public DNS seeds. A mesh uses `addnode` / `seednode` on
port **38443**. A lone eligible node can produce and store the ledger.
Other people seeing the same tip still need two or more peers.

User agent is `XCoin`. Signed messages use `X Coin Signed Message:\n`.
Mainnet P2PKH addresses start with **X**.

Third-party block explorers, asset explorers, and wallets can speak
the same Bitcoin-family RPC (`getblock`, `getrawtransaction` with
`-txindex=1`, `listassets`, `getassetdata`, `issue`, `transfer`). There
is no in-tree explorer URL. A third-party wallet still cannot mint a
main asset without Sign in with X on that node; subs are issued under
that session’s root. How to wire one:
[docs/THIRD-PARTY.md](../docs/THIRD-PARTY.md).

## 8. Wallets

The wallet is the desktop GUI that ships with the node.

- `xcoin-qt` — desktop GUI. This is the wallet people run.
- `xcoind` — daemon, lottery producer, wallet.
- `xcoin-cli` — RPC.

**Linux x86_64** and **Windows x86_64** packages are folders you open.
Each package has two labeled starts: the real wallet (no `-regtest`)
and **Practice**, which always passes `-regtest`. Practice writes only
the regtest data directory (`~/.xcoin/regtest` on Linux,
`%APPDATA%\XCoin\regtest` on Windows) and never the main ledger or the
main `wallet.dat`. The practice window title includes **[regtest]**.
Practice coins are not main XFER.

There is no mobile wallet and no in-app X.com wallet. Sign in with X
is OAuth in `xcoin-qt` / the node. Download packaged wallets from
[GitHub Releases](https://github.com/NiftyRaven/x-coin/releases).

Creating an address, sending XFER, linking a handle, and claiming the
root are Home buttons (or RPC against `wallet.dat`). Home includes
optional **Provide my node IP** (off by default). Peer IPs stay hidden
unless that box is turned on.

Inherited core behavior remains: UTXO wallet, BIP39 12-word HD seed,
`wallet.dat`, `encryptwallet` / `backupwallet`, fee estimates, P2P,
mempool, and the asset script format.

## 9. Privacy and limits

X Coin is a small UTXO-plus-lottery mesh. It is not an anonymity
network.

**What the protocol keeps local**

- OAuth access tokens never hit disk.
- Session files are this data directory only (mode `0600`) and are not
  gossiped.
- P2P heartbeats carry the public `@handle`, never the numeric X user
  id, never a token, never the HMAC proof.
- The GUI shows `@handle` and verified yes/no.

**What it does not hide**

- Lottery identity is the public handle. Peers who see `xhb` see who
  is competing that minute.
- The ledger is a public UTXO set among connected peers. Amounts and
  asset names in confirmed transactions are visible to those peers and
  to any explorer that indexes them.

**Limits that remain**

- Gossip `xVerified` is compact-signed by the payout key. Honest
  wallets set that bit only from `users/me`. A modified client can
  still assert it on the wire. Consensus checks that a coinbase matches
  the *committed* active set, not “the true mesh.”
- A live handle cannot be rebound. Residual: first-seen after restart
  unless a payout is pinned; eclipse of a node that only talks to
  attacker peers.
- Clock skew can delay a slot; height still maps 1:1.
- There are no checkpoints and no public DNS seeds. An eclipsed node
  follows the heaviest *valid* chain its peers feed it. Connect to a
  known seed.

Threat model: [docs/SECURITY.md](../docs/SECURITY.md). Treat the
lottery as specified here. Do not reintroduce Proof-of-Work as the
production path.

## 10. Provenance and license

Consensus code descends from open upstream and is released under the
**MIT** license. See [COPYING](../COPYING). Copyright headers in
source files still apply.

X Coin is a hard fork of that tree with its own genesis, magic, ports,
address versions, lottery, and identity rules. It is not a rebrand of
the imported network and not an imported UTXO snapshot.

A short list of leftover wire names and internal identifiers — kept
for compatibility, not as product branding — is in
[docs/FORK.md](../docs/FORK.md).

## 11. Conclusion

X Coin is a fair-launched UTXO ledger whose native unit is XFER, whose
block producers are X Verified running nodes chosen each minute by a
deterministic lottery, and whose identity layer binds a wallet to a
real X account without replacing the BIP39 seed.

No premine. No founder allocation. No mining. One free root per
signed-in X account. Desktop wallets on Linux and Windows.

The running node enforces these rules. Algorithms this paper
summarizes are specified in the annexes below.

## Further reading

| Document | Contents |
| --- | --- |
| [docs/LOTTERY.md](../docs/LOTTERY.md) | Lottery algorithm, coinbase rules, RPCs |
| [docs/XSIGNIN.md](../docs/XSIGNIN.md) | Sign in with X, session proof, privacy of login |
| [docs/WALLET.md](../docs/WALLET.md) | BIP39 seed vs X session |
| [docs/ASSETS.md](../docs/ASSETS.md) | Roots, subs, uniques, naming |
| [docs/FORK.md](../docs/FORK.md) | Upstream attribution, leftover names |
| [docs/GO-LIVE.md](../docs/GO-LIVE.md) | Birth timestamp and go-live order |
| [docs/THIRD-PARTY.md](../docs/THIRD-PARTY.md) | Explorers and external wallets |
| [docs/SECURITY.md](../docs/SECURITY.md) | Threat model |
