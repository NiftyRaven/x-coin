# X Coin 1.0.14 Light

**Superseded for Heavy users by [1.0.18 Heavy](RELEASE-1.0.18.md).** Current Light zip is [1.0.16 Light](https://github.com/NiftyRaven/x-coin/releases/tag/v1.0.16-light). Historical notes.

**Light** wallet. Simple Home. No Market. No tree nav.

User wallets **1.0.14 Light** from
[Releases](https://github.com/NiftyRaven/x-coin/releases/tag/v1.0.14).
Do **not** use **Code → Download ZIP**.

Same chain as **1.0.15 Heavy**. Same genesis, same lottery, same
`XHB1` / `XVA1` / `XSD1`. Protocol version **70028**. The Heavy wallet
is a later zip with Market and tree nav:
[RELEASE-1.0.15.md](RELEASE-1.0.15.md). Light ignores Market listings
(`xord`) and still confirms a completed buy. The baked seed does not
need Light **to print**.

Stay on Light if you want the original Home buttons. Switch to Heavy
only if you want Market.

## What Light has

An X Verified host can **Share lottery wins** on Home. They set a
percent of **their** mature lottery output and invite guests by X
handle. That percent is split equally among guests who have a claimed
root. Guests do not need a blue check and never enter the hat.

First enable writes `assetindex=1` **in this wallet’s data directory**
and asks for a restart (reindex) so `listaddressesbyasset` can find
each guest’s current root address. That is opt-in. It is **not** in
the zip. Existing roots stay valid.

After a halving, guests get a percent of this host’s slice (for example
20% of 1250 XFER), not of the whole 2500 subsidy.

This is a wallet send. A recoded or offline host can keep the win.
Fake handles still cannot win the lottery.

Home is buttons first: Sign in with X, lottery, Receive, Send,
Activity, Transfer assets, Share lottery wins. Left-nav **Assets**
opens Create (sub / unique). There is no Market page.

## What Light does not have

| | Light 1.0.14 | Heavy 1.0.15 |
| --- | --- | --- |
| Home | Buttons (Receive / Send / Assets) | Name card only |
| Tree nav | No | WALLET / ASSETS / REWARDS / NODE / ADVANCED |
| Market | No | **Assets → Market** |
| Share lottery wins | Home | **Rewards → Lottery share** |

## Package

Windows: [X-Coin-1.0.14-Windows.zip](https://github.com/NiftyRaven/x-coin/releases/download/v1.0.14/X-Coin-1.0.14-Windows.zip)
→ **X Coin Wallet.exe**

Linux: [X-Coin-1.0.14-Linux-x86_64.tar.gz](https://github.com/NiftyRaven/x-coin/releases/download/v1.0.14/X-Coin-1.0.14-Linux-x86_64.tar.gz)
→ **X Coin Wallet**

Do not use **Code → Download ZIP**. Heavy:
[v1.0.15](https://github.com/NiftyRaven/x-coin/releases/tag/v1.0.15).
