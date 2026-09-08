# X Coin lottery consensus

X Coin does **not** use Proof-of-Work. Once `xcoind` (or `xcoin-qt`) is
running, that process is an **active node**. Every **minute** a lottery picks
who may produce the next block and who shares that minute’s subsidy.

This document is the source of truth for the algorithm.

## Units and schedule

| Item | Value |
| --- | --- |
| Ticker | **XFER** |
| Subunit | **xferon** — 1 XFER = 100,000,000 xferons |
| Slot length | 60 seconds (one lottery minute) |
| Height mapping | height `h` uses slot `floor(genesisTime / 60) + h` |
| Target spacing | 1 block per minute (same as Ravencoin’s clock, without hashing) |
| Subsidy | 5000 XFER at height 1, halved every `nSubsidyHalvingInterval` blocks (2,100,000 on main/test, 150 on regtest) |

Genesis (height 0) is not a lottery block.

## Active node

A node is **active** if it has heartbeated within the last **180 seconds**.

- Node **id** = `Hash160(payout script)`. The script is what coinbase will pay.
- On start, the node loads or creates a payout script in `lottery-payout.dat`
  under the data directory (`~/.xcoin` on Unix). When a wallet is present the
  producer adopts the wallet mining script as the local payout.
- The producer thread heartbeats that script every second.
- `registeractivenode` records a heartbeat for the local script or a supplied
  **address / script hex**.
- Peers gossip `xhb` messages (`int64 timestamp` + `CScript`) after `verack`
  and about every 30 seconds. Honest nodes that can talk to each other
  therefore share one sorted active set for a given slot.

## Deterministic seed

```
seed = SHA256( prevBlockHash || LE64(slot) )
```

`prevBlockHash` is the hash of the current tip. `slot` is the lottery minute
for the **next** height. Honest nodes that agree on the tip and the slot
therefore agree on `seed`.

## Winner count (halvings)

```
winnerCount(height) = 1 + floor(height / nSubsidyHalvingInterval)
```

| Heights (mainnet interval 2,100,000) | Winners that minute |
| --- | --- |
| 1 … 2,099,999 | 1 |
| 2,100,000 … 4,199,999 | 2 |
| 4,200,000 … 6,299,999 | 3 |
| … | +1 per completed halving |

If the active set is smaller than `winnerCount`, every active node wins.

## Selection

1. Take the active ids, already sorted as unsigned 160-bit integers (std::map order).
2. Draw `k = min(winnerCount, n)` winners with a partial Fisher–Yates shuffle.
3. At step `i`, `r = SHA256(seed || LE32(i))` (first 8 bytes, little-endian) and
   swap index `i` with `i + (r % (n - i))`.
4. The **first** selected winner is the block producer for that slot.

Same active set + same seed ⇒ same winner list on every honest node.

## Rewards and coinbase

The Ravencoin subsidy function is unchanged (5000 XFER, right-shifted each
halving). That amount is split across the winners:

```
base = total / k
remainder = total % k   // extra xferons go to the first `remainder` winners
```

Fees (if any) are added to the **first** winner’s output.

The producer writes:

1. One `CTxOut` per winner, in selection order, paying `Hash160(script) == winner id`.
2. An `OP_RETURN` commitment: magic `XHB1` + uint32 LE count + concatenated
   20-byte ids (must be sorted unique). This is the active set used for the draw.

`ContextualCheckBlock` rejects a height ≥ 1 coinbase that is missing the
commitment, pays the wrong count/scripts, or splits the subsidy incorrectly.

## Block production

- **Main / test:** the producer thread heartbeats, waits until wall-clock slot
  ≥ height slot, and if this node is a winner it calls `CreateNewBlock` and
  `ProcessNewBlock`. There is **no** nonce grind. `CheckProofOfWork` is a
  no-op.
- **Regtest:** the producer only heartbeats. Use `generatetoaddress` /
  `generate` to assemble blocks on demand (still no PoW). The destination
  script is heartbeated so it is in the active set.
- Removed / gutted: `RavenMiner` hash loop, `-gen` / `setgenerate` as a miner,
  KawPoW submit helpers (`pprpcsb`, `getkawpowhash`).

## RPCs

| Command | Purpose |
| --- | --- |
| `getlotteryinfo` | Next-height draw: slot, seed, winners, split, local id |
| `getactivenodes` | Current registry (id, script hex, lastseen) |
| `registeractivenode (payout)` | Heartbeat this node or an address / script hex |
| `generatetoaddress` | Regtest / on-demand assembly (not mining) |

## Security notes

- **Sybil:** one operator can run many processes, or a producer can commit an
  active set of scripts they control. Validation only proves the coinbase
  matches the *committed* set. Acceptable for a private trusted mesh; not a
  bonded public lottery.
- Clock skew can delay a slot; height still maps deterministically once a
  block exists.

Treat the lottery as specified here; do not reintroduce PoW as the production path.
