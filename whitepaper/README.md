# X Coin whitepaper

This directory **is** the X Coin whitepaper.

**Read the paper:** [XCOIN.md](XCOIN.md)

**Nifty Raven** (@NFTRVN on X)  
Anonymous public identity — display name and handle only.

---

X Coin is a peer-to-peer coin for **transfers between users on X**.

| | |
| --- | --- |
| Name | X Coin |
| Ticker | **XFER** |
| Subunit | **xferon** (1 XFER = 100,000,000 xferons) |
| Mining | **None.** Minute lottery among **X Verified** (blue check) active nodes |
| Halvings | Each halving adds one more winner that minute (1 → 2 → 3 …) |
| Pools | Optional. Tickets = verified running members. Win split evenly among every member address |
| Launch | Fair. No premine, no founder allocation |
| Genesis | Frozen at **go-live** (`freeze-genesis.sh`). Height 1 when first eligible node produces |
| Supply | About **21 billion** XFER spendable |
| Identity | Sign in with X. One **free root** per signed-in handle (no login → no main). GUI shows @handle only |
| Subs / uniques | **100 XFER** / **5 XFER** under that root |
| P2P / RPC | 38443 / 38442 (magic `XFER`) |

There is no older paper here. [XCOIN.md](XCOIN.md) is the release paper:
purpose, lottery, optional pools, supply, X Verified eligibility, assets,
privacy, network identity, Linux and Windows packages, and
practice/regtest isolation. How to open the wallet (do not compile):
[README](../README.md) — download from
[Releases](https://github.com/NiftyRaven/x-coin/releases/tag/v1.0.1),
not **Code → Download ZIP**. Private-test audit:
[docs/AUDIT.md](../docs/AUDIT.md).
