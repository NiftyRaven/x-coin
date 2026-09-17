# X Coin 1.0.16 Heavy

**Superseded by [1.0.17 Heavy](RELEASE-1.0.17.md).** Keep this page as
the 1.0.16 notes. Consensus was unchanged.

**Heavy** wallet. Same chain as **1.0.16 Light**. Consensus unchanged.

User wallets **1.0.16 Heavy** from
[Releases](https://github.com/NiftyRaven/x-coin/releases/tag/v1.0.16).
Do **not** use **Code → Download ZIP**.

Protocol version stays **70028**. Same genesis, same lottery, same
`XHB1` / `XVA1` / `XSD1`. The baked seed does not need this zip.

Stay on [1.0.16 Light](https://github.com/NiftyRaven/x-coin/releases/tag/v1.0.16-light)
if you want the simple Home wallet. This zip is Heavy (Market + tree
nav). Light and Heavy are parallel, not a forced upgrade path.

| | Light 1.0.16 | Heavy 1.0.16 |
| --- | --- | --- |
| Home | Buttons (Receive / Send / Assets) | Name card only |
| Tree nav | No | WALLET / ASSETS / REWARDS / NODE / ADVANCED |
| Market | No | **Assets → Market** |
| Share lottery wins | Home | **Rewards → Lottery share** |

Same `%APPDATA%\XCoin` / `~/.xcoin`. No reindex to switch, unless you
already enabled Lottery share (`assetindex=1` on that datadir).

## What changed

### Live lottery count

The header and Home used the slot-frozen draw set, so a wallet that
first heard a node this minute could show one fewer **active** than a
peer that had seen it earlier. Display now uses live X Verified
heartbeats. Winners still use the freeze. Consensus is unchanged.

### Master Seed Node

Home and My node name the packaged launch seed: **Connected to Master
Seed Node + 1 public node** when that is the topology, instead of
**Connected to 1 peer**.

### What's new stays on this edition

Heavy does not treat a Light GitHub release as an upgrade. Light 1.0.16
does not treat Heavy as one.

Market, tree nav, and Share lottery wins are unchanged from 1.0.15.

## Package

Windows: [X-Coin-1.0.16-Windows.zip](https://github.com/NiftyRaven/x-coin/releases/download/v1.0.16/X-Coin-1.0.16-Windows.zip)
→ **X Coin Wallet.exe**

Linux: [X-Coin-1.0.16-Linux-x86_64.tar.gz](https://github.com/NiftyRaven/x-coin/releases/download/v1.0.16/X-Coin-1.0.16-Linux-x86_64.tar.gz)
→ **X Coin Wallet**

Do not use **Code → Download ZIP**. Light:
[v1.0.16-light](https://github.com/NiftyRaven/x-coin/releases/tag/v1.0.16-light).

## Related

- Market: [MARKET.md](MARKET.md)
- Light 1.0.16: GitHub [v1.0.16-light](https://github.com/NiftyRaven/x-coin/releases/tag/v1.0.16-light)
- Lottery share: [LOTTERY.md](LOTTERY.md)
- Wallet pages: [WALLET.md](WALLET.md)
- Sign in: [XSIGNIN.md](XSIGNIN.md)
- Go-live: [GO-LIVE.md](GO-LIVE.md)
- In-wallet What's new: [UPDATES.md](UPDATES.md)
