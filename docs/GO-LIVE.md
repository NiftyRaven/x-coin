# X Coin go-live

**Nifty Raven** (@NFTRVN on X)

Fair launch clock: genesis `nTime` **1789197360** —
**12 September 2026, 3:16:00 AM America/New_York (EDT)** /
07:16:00 UTC (John 3:16). Height `h` is minute
`floor(nTime/60)+h`. Connecting a node does **not** rewrite height 0.

Height 0 is unspendable. Spendable coins start at height 1 when the
**baked seed** (`172.191.195.221:38443`) emits the first `XHB1` +
`XVA1` + `XSD1` block.

There are no public DNS seeds. Packaged `xcoin.conf` already has
`addnode=172.191.195.221:38443`.

## Who does what

| Role | What to run |
| --- | --- |
| Baked seed | Headless Heavy `xcoind` (1.0.16 or 1.0.17, or the Market 1.0.15 binary already on the printer). Attestor key + X bearer. **No Sign-in.** Same datadir. **No `-reindex`.** Do not touch the live seed VM. |
| Light users | **1.0.16 Light** zip from [v1.0.16-light](https://github.com/NiftyRaven/x-coin/releases/tag/v1.0.16-light). Sign in with X. Leave the window open. |
| Heavy users | **1.0.17 Heavy** zip from [v1.0.17](https://github.com/NiftyRaven/x-coin/releases/tag/v1.0.17). Sign in with X. If the wallet is encrypted, Unlock on Home (duration, then passphrase) so heartbeat can run. |

The seed is **not** a user’s double-clicked wallet. Do not Sign in on
the seed. Do not put `xattestor.key` or the bearer in the zip.

Light 1.0.16 still prints lottery blocks if used as a seed. It does
**not** relay Market listings (`xord`). Two Heavy wallets that only
talk through a Light seed will not see each other’s book. Completed
buys still move as normal transactions. Packaged `xcoin.conf` now sets
`assetindex=1` (holder lookup for lottery share). It does **not** set
`txindex=1`. An existing datadir that ran without the index needs a
one-time reindex; **do not reindex the seed.**

MAIN producer rule: at most **one block per wall-clock minute**.
Catch-up may target an older lottery slot; the latch still holds.
There is **no** slot+120 abort.

## User path (launch night)

1. Download **Light 1.0.16** or **Heavy 1.0.17** from Releases (not
   1.0.16 Heavy, not **Code → Download ZIP**). Light is the simple Home
   wallet. Heavy adds Market, tree nav, and Home unlock.
2. Double-click **X Coin Wallet** / **X Coin Wallet.exe**.
3. Sign in with X once. Claim the free root if you want the identity
   asset (blue check not required for the root; required for the hat).
4. Leave the wallet open. Local “eligible” is your session. Seed
   “eligible” is a stamp on `stamped_handles`.
5. On Heavy: **Assets → Market** to list or buy bags for XFER.
   **Rewards → Lottery share** if a verified host wants guests on their
   payout. On Light: Share lottery wins is on Home. Practice stays
   `-regtest` only.

## Operator path

See [SECURITY.md](SECURITY.md) and [RELEASE-1.0.12.md](RELEASE-1.0.12.md).
Confirm on the seed: `baked_seed`, `x_lookup`, empty `local_eligible`,
and at least one stamped handle before the first payday if you want a
non-empty hat.

## Related

- Light 1.0.16: [v1.0.16-light](https://github.com/NiftyRaven/x-coin/releases/tag/v1.0.16-light)
- Heavy 1.0.17: [RELEASE-1.0.17.md](RELEASE-1.0.17.md)
- Market: [MARKET.md](MARKET.md)
- Whitepaper: [../whitepaper/XCOIN.md](../whitepaper/XCOIN.md)
- Lottery: [LOTTERY.md](LOTTERY.md)
- Third-party explorers / wallets: [THIRD-PARTY.md](THIRD-PARTY.md)
