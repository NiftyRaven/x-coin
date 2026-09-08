# X Coin go-live (wait for the order)

**Nifty Raven** (@NFTRVN on X)

This repository stays **private** until the **September 12, 2026**
window (and until you say go). There is no public GitHub Release and
no DNS seed until then. Invited people add a seed by hand on port
**38443**.

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

```bash
./autogen.sh
./configure --with-gui=qt5 --disable-bench --disable-tests --with-incompatible-bdb
make -j$(nproc)
contrib/xcoin/freeze-genesis.sh          # patches nTime + hash, rebuilds
src/qt/xcoin-qt                          # Sign in with X, X Verified, running
# first payday ≈ one minute later (getblockchaininfo / getblock)
contrib/xcoin/package-linux.sh           # private tarball 1.1.0
```

Then: make the GitHub repo public, tell operators `addnode=<seed>:38443`.
Do **not** add public DNS seeds until you intend a public mesh.

## Related

- Third-party explorers / wallets: [THIRD-PARTY.md](THIRD-PARTY.md)
- Operator mesh: [LAUNCH.md](LAUNCH.md)
- Assets / Sign in with X: [ASSETS.md](ASSETS.md), [XSIGNIN.md](XSIGNIN.md)
