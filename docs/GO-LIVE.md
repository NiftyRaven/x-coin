# X Coin go-live (wait for the order)

**Nifty Raven** (@NFTRVN on X)

This repository stays **private** until the **September 12, 2026**
window (and until you say go). There is no public GitHub Release and
no DNS seed until then.

Launch night for the operator is in [README.md](../README.md): wait
until **12 September 2026 around 9:00 PM ET**, then double-click the
**real** wallet (not Practice). Do not open the real wallet before
then. Do not compile that night. Opening that wallet starts the first
node. It listens on **38443**. That running wallet is the seed.
Other wallets do not find you through GitHub — they need
`addnode=<host>:38443` already in the public package `xcoin.conf`.

## Birth of the chain

Genesis `nTime` is the lottery clock. Height `h` is minute
`floor(nTime/60)+h`. Connecting a node does **not** rewrite height 0 by
itself.

So that **explorers show the chain being born when the first user is
online**:

1. At go-live, freeze main genesis to *that moment*:
   `contrib/xcoin/freeze-genesis.sh`
2. Start the first **X Verified** `xcoin-qt` / `xcoind` **immediately**
   (same hour). Sign in with X. Height 1 is the next minute. That
   block’s timestamp is the first payday.
3. Then make the repository public (GitHub settings — this tree cannot
   flip visibility by itself).

If genesis stays a development midnight and someone starts mainnet hours
later, the producer **refuses** to backfill more than two hours of fake
minutes. Re-run the freeze script so timestamps match first connect.

Height 0 is unspendable. Spendable coins start at height 1.

## Go-live order (when you say go)

Operator path (no compile): [README.md](../README.md) launch night.

1. Double-click **X Coin Wallet** / **X Coin Wallet.exe**. Keep it running.
2. Sign in with X. Claim the free root.
3. Home → **Provide my node IP** (off until you turn it on) → copy
   `addnode=host:38443`.
4. Put that line in the public Windows and Linux package `xcoin.conf`.
5. Make the GitHub repo public (GitHub settings — this tree cannot
   flip visibility by itself).

Do **not** add public DNS seeds. Do not invent a host or a Client ID.

Developer-only genesis freeze (not the launch-night double-click):
`contrib/xcoin/freeze-genesis.sh` is documented for a source rebuild
so explorer timestamps match first connect. The shipped 1.0.1 wallet
is what you double-click on launch night.

## Related

- Third-party explorers / wallets: [THIRD-PARTY.md](THIRD-PARTY.md)
- Operator mesh: [LAUNCH.md](LAUNCH.md)
- Assets / Sign in with X: [ASSETS.md](ASSETS.md), [XSIGNIN.md](XSIGNIN.md)
