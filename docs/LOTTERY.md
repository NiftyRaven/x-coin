# X Coin lottery consensus

X Coin does **not** use Proof-of-Work. Every **minute** a lottery picks who
may produce the next block and who shares that minute’s subsidy.

**Fork choice (law):** one height is one slot is one block. The first-drawn
winner of `SHA256(parentHash || LE64(slot))` on the committed `XHB1` set
produces that block. Every other drawn winner is paid on **that same
coinbase**, not with a second block. If two valid blocks share a parent,
keep the smaller **block hash**. A node that already has a valid block for
height `h` must not emit another. There are **no pools**. There is no `XPL1`.

Sign-in with X is a **local login hint** that binds this datadir to one
handle. Consensus does not call X.com on every block and does not treat
X as an oracle of truth. Gossip can lie. The coinbase must match the
committed set; that is all validation proves.

This document is the source of truth for the algorithm.
See also [FORK-CHOICE.md](FORK-CHOICE.md).

## Units and schedule

| Item | Value |
| --- | --- |
| Ticker | **XFER** |
| Subunit | **xferon** — 1 XFER = 100,000,000 xferons |
| Slot length | 60 seconds (one lottery minute) |
| Height mapping | height `h` uses slot `floor(genesisTime / 60) + h` |
| Target spacing | 1 block per minute |
| Subsidy | 5000 XFER at height **1**, halved every `nSubsidyHalvingInterval` blocks (2,100,000 on main/test, 150 on regtest). Height 0 pays **0**. |
| Spendable lifetime | **20,999,994,999.727 XFER** |

Genesis (height 0) is not a lottery block and is not a payday. The serialized genesis coinbase is never added to the UTXO set (`ConnectBlock` skips it).

## Eligibility

A node is lottery-eligible when it has a linked X identity that this node
believes is verified, and it is heartbeating a payout script.
Unlinked nodes still sync and relay. They do not enter the active set.

X Verified on this node comes from the local session (Sign in with X) or,
on older operator meshes, a shared allowlist. Neither is an oracle.
Modified clients can assert verified on the wire. Consensus only checks
that the coinbase pays the **committed** `XHB1` set.

## Active node

A node is **active** if it has heartbeated a linked identity within the last
**180 seconds**.

- Node **id** = `Hash160(payout script)`.
- One handle maps to one active node.
- Peers gossip `xhb` after `verack` and about every 30 seconds.

## Deterministic seed

```
seed = SHA256( prevBlockHash || LE64(slot) )
```

Honest nodes that agree on the tip and the slot agree on `seed`.

## Winner count

```
winnerCount(height) = 1 + floor(height / nSubsidyHalvingInterval)
```

| Heights (mainnet interval 2,100,000) | Winners that minute |
| --- | --- |
| 1 … 2,099,999 | 1 |
| 2,100,000 … 4,199,999 | 2 |
| … | +1 per completed halving |

If the active set is smaller than `winnerCount`, every active node wins.
That is a **payee tie inside one coinbase**, not two blocks.

## Selection

1. Take the active ids, sorted as unsigned 160-bit integers.
2. Draw `k = min(winnerCount, n)` winners with a partial Fisher–Yates shuffle.
3. At step `i`, `r = SHA256(seed || LE32(i))` (first 8 bytes, little-endian) and
   swap index `i` with `i + (r % (n - i))`.
4. `winners[0]` is the only producer. `winners[1..)` are paid on the same tx.

Same active set + same seed ⇒ same winner list on every honest node.

## Rewards and coinbase

```
base = total / k
remainder = total % k   // extra xferons go to winners[0..remainder)
```

Fees go to `winners[0]`. The coinbase txid is the receipt of that split.

The producer writes:

1. One `CTxOut` per winner, in selection order.
2. An `OP_RETURN` `XHB1` commitment of the sorted unique active ids.

No `XPL1`. No pool member list. A mismatch is an invalid block.
An empty committed set is invalid.

## Block production and fork choice

- Main / test: wait until wall-clock slot ≥ height slot. Emit at most one
  block per minute, and only if this node is `winners[0]` and no valid
  block for that height is already known.
- Two valid siblings: keep the smaller block hash. Longest valid chain wins.
- Launch `nMinReorganizationPeers` must be **1** (today it is 4 — fix before go-live).
- Regtest: `generatetoaddress` assembles on demand. No hashing.
- `CheckProofOfWork` is a no-op. Do not bring mining back.

## Seed node

There is no invite mesh. The first listen node on release night is the
public seed on port **38443**. Packaged wallets must ship that IP as
`seednode=` and `addnode=`. Until that IP is baked, peers do not find
each other automatically. `CMainParams` currently clears `vSeeds` and
`vFixedSeeds`. Leftover Ravencoin seeds in `chainparamsseeds.h` use port
8767 and must not be loaded.

## RPCs

| Command | Purpose |
| --- | --- |
| `getlotteryinfo` | Next-height draw: slot, seed, winners, split, local id |
| `getactivenodes` | Current registry |
| `registeractivenode` | Heartbeat this node or an address / script hex |
| `generatetoaddress` | **Regtest only** |

## Security notes

- Consensus checks coinbase vs committed set, not “the true mesh.”
- Clock skew delays a slot; height still maps 1:1.
- Empty committed set ⇒ invalid.
- Two competing blocks are resolved by hash, not by splitting subsidy across forks.
