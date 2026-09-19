# X Coin 1.0.18 Heavy

**Heavy** wallet. Same chain as **1.0.16 Light**. Consensus unchanged.

User wallets **1.0.18 Heavy** from
[Releases](https://github.com/NiftyRaven/x-coin/releases/tag/v1.0.18)
after this zip is published. Do **not** use **Code → Download ZIP**.

Protocol version stays **70028**. Same genesis, same lottery, same
`XHB1` / `XVA1` / `XSD1`. **Do not touch the live seed VM.** No
`-reindex` on the seed.

Stay on [1.0.16 Light](https://github.com/NiftyRaven/x-coin/releases/tag/v1.0.16-light)
if you want the simple Home wallet. Light and Heavy are parallel, not a
forced upgrade path. Light 1.0.16 still has the previous Sign in with X
Client ID.

| | Light 1.0.16 | Heavy 1.0.18 |
| --- | --- | --- |
| Home | Buttons (Receive / Send / Assets) | Name card + Unlock when locked |
| Tree nav | No | WALLET / ASSETS / REWARDS / NODE / ADVANCED |
| Market | No | **Assets → Market** |
| Share lottery wins | Home | **Rewards → Lottery share** |

Same `%APPDATA%\XCoin` / `~/.xcoin`.

## What changed

### Sign in with X Client ID

The prior X app was suspended. Packaged `xcoin.conf` now bakes the new
public OAuth 2.0 Client ID (`xoauthclientid=`). Callback is still
`http://127.0.0.1:18791/callback`. Users never paste a Client ID.

**1.0.17 Heavy and older cannot Sign in with X.** Download this zip.

No Client Secret, Consumer Secret, or Bearer is shipped.

### Version

Home prints **Wallet 1.0.18 heavy**. Help → About and the sync overlay
still show the full version string.

Everything else is unchanged from [1.0.17 Heavy](RELEASE-1.0.17.md)
(dark sync, Home unlock duration, default `assetindex=1`).

## Package

Windows: [X-Coin-1.0.18-Windows.zip](https://github.com/NiftyRaven/x-coin/releases/download/v1.0.18/X-Coin-1.0.18-Windows.zip)
→ **X Coin Wallet.exe**

Linux: [X-Coin-1.0.18-Linux-x86_64.tar.gz](https://github.com/NiftyRaven/x-coin/releases/download/v1.0.18/X-Coin-1.0.18-Linux-x86_64.tar.gz)
→ **X Coin Wallet**

Do not use **Code → Download ZIP**. Light:
[v1.0.16-light](https://github.com/NiftyRaven/x-coin/releases/tag/v1.0.16-light).

## Related

- Previous Heavy: [RELEASE-1.0.17.md](RELEASE-1.0.17.md)
- Market: [MARKET.md](MARKET.md)
- Lottery share / heartbeat: [LOTTERY.md](LOTTERY.md)
- Wallet pages: [WALLET.md](WALLET.md)
- Sign in: [XSIGNIN.md](XSIGNIN.md)
- Go-live: [GO-LIVE.md](GO-LIVE.md)
- In-wallet What's new: [UPDATES.md](UPDATES.md)
