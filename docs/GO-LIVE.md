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
| Baked seed | Headless `xcoind` 1.0.12 on Azure. Attestor key + X bearer. **No Sign-in.** |
| Everyone else | **1.0.12** desktop zip from [Releases](https://github.com/NiftyRaven/x-coin/releases/tag/v1.0.12). Sign in with X. Leave the window open. |

The seed is **not** a user’s double-clicked wallet. Do not Sign in on
the seed. Do not put `xattestor.key` or the bearer in the zip.

MAIN producer rule: at most **one block per wall-clock minute**.
Catch-up may target an older lottery slot; the latch still holds.
There is **no** slot+120 abort.

## User path (launch night)

1. Download **1.0.12** (not 1.0.11, not Code → Download ZIP).
2. Double-click **X Coin Wallet** / **X Coin Wallet.exe**.
3. Sign in with X once. Claim the free root if you want the identity
   asset (blue check not required for the root; required for the hat).
4. Leave the wallet open. Local “eligible” is your session. Seed
   “eligible” is a stamp on `stamped_handles`.
5. Practice stays `-regtest` only.

## Operator path

See [SECURITY.md](SECURITY.md) and [RELEASE-1.0.12.md](RELEASE-1.0.12.md).
Confirm on the seed: `baked_seed`, `x_lookup`, empty `local_eligible`,
and at least one stamped handle before the first payday if you want a
non-empty hat.

## Related

- Whitepaper: [../whitepaper/XCOIN.md](../whitepaper/XCOIN.md)
- Lottery: [LOTTERY.md](LOTTERY.md)
- Third-party explorers / wallets: [THIRD-PARTY.md](THIRD-PARTY.md)
