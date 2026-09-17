# In-wallet Market (1.0.15 Heavy)

**Nifty Raven** (@NFTRVN on X)

Private only. Consensus is unchanged.

Market ships in the **1.0.15 Heavy** wallet only. **1.0.14 Light** does
not show a book. Light still confirms a completed buy (normal
transaction).

Market is a **peer-to-peer listing board inside the Heavy wallet**. It
is not a shop server, not a DEX, and not a new chain. List an asset bag
for XFER. Another Heavy wallet buys it. No copy-paste for the happy
path.

## What the user sees

**Assets → Market** (Heavy tree nav).

- **For sale** — listings this node has heard.
- **Your bags** — spendable asset UTXOs. Select one or more (Ctrl-click
  for several). Same XFER price for each listing. **List selected**.
- **Your listings** — **Cancel listing** spends the bag back to this
  wallet (0.01 XFER network fee).
- **Buy with XFER** — completes the listing and broadcasts. Buyer also
  pays 0.01 XFER network fee.
- **Private trade (copy and paste)** — optional two-party OTC. Leave it
  closed unless the other person is not on the book.

Each listed row is **one whole UTXO**. Send to yourself first if you
only want to sell part of a bag. Root, `PARENT/SUB`, and `PARENT#unique`
are all listable. Owner tokens (`NAME!`) and `~` names are not.

Packaged `xcoin.conf` sets `assetindex=1` (holder lookup for lottery
share). Market does **not** use that index. The flag is an extra index,
not `txindex=1` and not an archival node. A datadir that already ran
without it needs a one-time reindex; new wallets index from genesis.

## How it works (not a fork)

A listing is a 1-in / 1-out **partial** transaction:

- Input: one asset UTXO.
- First output: the XFER price to the seller.
- Signed `SIGHASH_SINGLE | ANYONECANPAY`.

Peers gossip it as P2P command `xord`. **Light 1.0.14 ignores unknown
commands.** Listings are not mempool transactions. A completed buy is a
normal transaction: the buyer adds XFER inputs, change, and the asset
output to themselves, signs the rest, and broadcasts. The seller
signature stays valid. Light peers still confirm that buy.

RPC (wallet): `listmarket`, `postask`, `takeask`, `cancelask`.

## Limits (on purpose)

| Cap | Why |
| --- | --- |
| 200 live asks per node | RAM book, not an exchange matching engine |
| ~20 KB per listing | Drop junk |
| 30 new asks / minute / peer | Stop one connection flooding |
| 50 asks pushed on connect | Fast catch-up, not a full dump |
| RAM only | Restart empties the book until peers send some back |

There is **no timer**. A listing sits until it is bought, cancelled, the
UTXO is spent, this node’s book is full (oldest drop), or the node
restarts.

One person with many bags can occupy many slots (one UTXO per row).
Re-listing the same bag replaces that row.

## Who must run Heavy

- **Buyers and sellers** who want to see the book need **1.0.15 Heavy**
  (the Release zip, not **Code → Download ZIP**).
- The **baked seed** should run Heavy `xcoind` so listings pass through
  the hub. Light 1.0.14 still prints lottery blocks; it does not relay
  `xord`.
- Do not Sign in on the seed. Same datadir, no `-reindex`, no
  `assetindex=1`.

Two Heavy wallets that only talk through a Light seed will not see
each other’s listings. Completed buys still move as normal txs.

Light vs Heavy: [RELEASE-1.0.14.md](RELEASE-1.0.14.md),
[RELEASE-1.0.15.md](RELEASE-1.0.15.md).
