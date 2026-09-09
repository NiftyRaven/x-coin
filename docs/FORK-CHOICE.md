# Fork choice (one paragraph, launch law)

One height is one slot is one block. The first-drawn winner of `SHA256(parentHash || LE64(slot))` on the `XHB1` set may produce that block; every other drawn winner is a **payee on that same coinbase**, not a second producer. If two otherwise-valid blocks share a parent, keep the one whose **block hash is smaller**; equal work does not exist because there is no work. A node that already accepted a valid block for height `h` must not emit another. Competing blocks are not a subsidy tie — a tie is two **ids in one draw**, paid in one coinbase, remainder xferons and fees to the first winner by draw order (stable on coinbase txid).

## What that forbids

- Two coinbases for the same height on one chain.
- Pool splits (`XPL1`, pool id, pool password). Gone. One lottery.
- Treating “two nodes both think they won” as two blocks that later merge. They do not merge. Hash picks one. The loser’s coinbase dies.

## What a tie actually is

`winnerCount(h) = 1 + floor(h / nSubsidyHalvingInterval)`.
Until height 2,100,000 that is **one** winner. There is no split yet.
After that, `k` winners share `subsidy / k` in **one** transaction. That is the only split.
You cannot split a fork. You pick a block.

## Producer rule

```
if this node is winners[0]
   and no valid block for this height is known
   and wall-clock slot >= height slot:
      emit one block, pay winners[0..k) as specified, commit XHB1
else:
      do not emit
```

## Reorg rule (no PoW)

Longest valid lottery chain wins.
Same length, same parent: smaller `block.hash` wins.
Do not require four peers to reorg (`nMinReorganizationPeers` must be 1 at launch or a lone seed cannot heal a bad tip).

## Seed

Launch node is the public seed on **38443**. Every packaged `xcoin.conf` must ship:

```
seednode=<PUBLIC_IP>:38443
addnode=<PUBLIC_IP>:38443
```

`vSeeds` / `vFixedSeeds` in `CMainParams` are cleared today. Leftover Ravencoin entries in `chainparamsseeds.h` (port **8767**) must stay unused or be deleted. Do not point X Coin at Ravencoin seeds.

Paste the listen IP before the 12 September package. Without it, nodes do not auto-connect.


## In the binary (NFTRVN)

- Draw uses `ActiveIdsForSlot`: `joined < slot_start`, still alive at open.
- Producer waits `SETTLE_SECONDS` (5) into the minute.
- Only `winners[0]` emits.
- If this height already has a block with data, do not emit.
- `GetBlockProof` is 1. Longest chain wins. Same length: smaller block hash.
- No pools.
