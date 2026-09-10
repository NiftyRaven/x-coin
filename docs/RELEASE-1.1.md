# X Coin 1.1 notes (superseded)

**Nifty Raven** (@NFTRVN on X)  
Anonymous public identity — display name and handle only.

**1.1 pool work was removed.** Current wallets are **1.0.6** on
[Releases](https://github.com/NiftyRaven/x-coin/releases/tag/v1.0.6)
(`X-Coin-1.0.6-Windows.zip` and `X-Coin-1.0.6-Linux-x86_64.tar.gz`).
Do not make the repository public until go-live ([GO-LIVE.md](GO-LIVE.md)).
1.0 snapshot: [RELEASE-1.0.md](RELEASE-1.0.md).

What stayed after the pool excision:

- **X Verified = X’s blue check** from `GET /2/users/me`, not the
  operator invite list (`addxverified`). Unverified = zero lottery
  chance. Invite list cannot exclude a verified running wallet.
- **Private Sign in with X** — access tokens never written. GUI shows
  @handle only. P2P `xhb` sends user id **0**. Users never paste a
  Client ID; the operator id is baked in package `xcoin.conf`.
- **Provide my node IP** on Home, **off by default**.
- **12-word BIP39 create/restore is unchanged.** Sign in with X does
  not replace the seed ([WALLET.md](WALLET.md)).
- Desktop GUI `xcoin-qt`, same `wallet.dat` as CLI, one free identity
  root per **signed-in** handle, no mining, fair launch, no public DNS
  seeds, no explorer, no mobile app.

Paper: [whitepaper/XCOIN.md](../whitepaper/XCOIN.md).
Audit: [AUDIT.md](AUDIT.md). Lottery: [LOTTERY.md](LOTTERY.md).
