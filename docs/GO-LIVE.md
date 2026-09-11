# X Coin go-live (wait for the order)

**Nifty Raven** (@NFTRVN on X)

This repository stays **private** until the **September 12, 2026**
window (and until you say go). There is no public GitHub Release and
no DNS seed until then.

Launch night for the operator is in [README.md](../README.md): wait
until **12 September 2026 around 3:16 AM ET** (John 3:16), then
double-click the **real** wallet (not Practice). Do not open the real
wallet before then. Do not compile that night. Opening that wallet
starts the first node. It listens on **38443**. That running wallet
is the seed.
Packaged `xcoin.conf` already has `addnode=172.191.195.221:38443` and
`seednode=172.191.195.221:38443`. Other wallets do not find you
through GitHub.

## Birth of the chain

Genesis `nTime` is **1789197360** — **2026-09-12 03:16:00
America/New_York (EDT)** = **2026-09-12 07:16:00 UTC** (John 3:16).
Height `h` is minute `floor(nTime/60)+h`. Connecting a node does
**not** rewrite height 0. Explorer timestamps will show that birth;
the MAIN producer does not abort on a late start.

MAIN producer rule: at most **one block per wall-clock minute**
(wall-minute latch). Catch-up may target an older lottery slot; the
latch still holds. There is **no** slot+120 abort and **no** two-hour
backfill refuse.

Height 0 is unspendable. Spendable coins start at height 1.

## Go-live order (when you say go)

Operator path (no compile): [README.md](../README.md) launch night.

1. Double-click **X Coin Wallet** / **X Coin Wallet.exe**. Keep it running.
2. Sign in with X. Claim the free root.
3. Home → **Provide my node IP** (off until you turn it on) to confirm
   this box is `172.191.195.221:38443`.
4. Make the GitHub repo public (GitHub settings — this tree cannot
   flip visibility by itself).

Do **not** add public DNS seeds. Do not invent a host or a Client ID.
The operator Client ID is already baked in package `xcoin.conf`.

The in-tree wallet is **1.0.11**. Last shipped zip is **1.0.10** until
the 1.0.11 package workflow is run. After the repo is public, running
wallets can show later GitHub release notes in-wallet
([UPDATES.md](UPDATES.md)).

## Related

- Third-party explorers / wallets: [THIRD-PARTY.md](THIRD-PARTY.md)
- Operator mesh: [LAUNCH.md](LAUNCH.md)
- Assets / Sign in with X: [ASSETS.md](ASSETS.md), [XSIGNIN.md](XSIGNIN.md)
