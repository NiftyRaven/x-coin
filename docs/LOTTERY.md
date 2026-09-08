# X Coin lottery consensus

X Coin does **not** use Proof-of-Work. Once `xcoind` (or the Qt wallet) is
running, that process is an **active node**. Every **minute** a lottery picks
who may produce the next block and who shares that minute’s subsidy.

This document is the source of truth for the algorithm. Phase 1 implements the
math, an in-memory active-node registry, a producer thread, and RPCs
(`getlotteryinfo`, `getactivenodes`, `registeractivenode`). Phase 2 must make
the active set identical across honest peers and enforce the coinbase split.

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

Phase 1:

- On start, the node loads or creates a 160-bit id in `lottery-nodeid.dat` under the data directory (`~/.xcoin` on Unix).
- The producer thread heartbeats that id every second.
- `registeractivenode` records a heartbeat for the local id or a supplied id.
- The registry is **in-memory** and **not yet gossiped**. Two isolated nodes do not see each other.

Phase 2 leftover: a P2P heartbeat inventory so every honest node computes the same sorted active set for a given slot.

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

## Rewards

The Ravencoin subsidy function is unchanged (5000 XFER, right-shifted each
halving). That amount is split across the winners:

```
base = total / k
remainder = total % k   // extra xferons go to the first `remainder` winners
```

Phase 1 still builds a single coinbase output (the producing node’s wallet
script) via `BlockAssembler`. `SplitReward` is implemented and returned by
`getlotteryinfo`. Phase 2 leftover: coinbase must pay each winner its share,
and validation must reject a mismatch.

## Block production (Phase 1)

- **Main / test:** the producer thread heartbeats, waits until wall-clock slot
  ≥ height slot, and if this node is a winner it calls `CreateNewBlock` and
  `ProcessNewBlock`. There is **no** nonce grind. `CheckProofOfWork` is a
  no-op.
- **Regtest:** the producer only heartbeats. Use `generatetoaddress` /
  `generate` to assemble blocks on demand (still no PoW).
- Removed / gutted: `RavenMiner` hash loop, `-gen` / `setgenerate` as a miner,
  KawPoW submit helpers (`pprpcsb`, `getkawpowhash`).

## RPCs

| Command | Purpose |
| --- | --- |
| `getlotteryinfo` | Next-height draw: slot, seed, winners, split, local id |
| `getactivenodes` | Current registry |
| `registeractivenode (id)` | Heartbeat this node or a hex id |
| `generatetoaddress` | Regtest / on-demand assembly (not mining) |

## Security notes (honest TODOs)

Phase 1 is a scaffold for a distinct chain identity and a working
single-node / regtest loop. It is **not** a production consensus:

- Sybil: one operator can run many processes and win more often.
- Registry split: peers do not yet share heartbeats, so they can disagree on winners.
- Nothing yet proves a produced block’s coinbase matches the draw.
- Clock skew can delay a slot; height still maps deterministically once a block exists.

Treat the lottery as specified here; do not reintroduce PoW as the production path.
