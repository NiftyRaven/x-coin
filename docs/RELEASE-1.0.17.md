# X Coin 1.0.17 Heavy

**Superseded by [1.0.18 Heavy](RELEASE-1.0.18.md).** Sign in with X
on this zip uses the old Client ID (prior X app was suspended).

**Heavy** wallet. Same chain as **1.0.16 Light**. Consensus unchanged.

User wallets **1.0.17 Heavy** from
[Releases](https://github.com/NiftyRaven/x-coin/releases/tag/v1.0.17)
after this zip is published. Do **not** use **Code → Download ZIP**.

Protocol version stays **70028**. Same genesis, same lottery, same
`XHB1` / `XVA1` / `XSD1`. **Do not touch the live seed VM.** No
`-reindex` on the seed.

Stay on [1.0.16 Light](https://github.com/NiftyRaven/x-coin/releases/tag/v1.0.16-light)
if you want the simple Home wallet. Light and Heavy are parallel, not a
forced upgrade path.

| | Light 1.0.16 | Heavy 1.0.17 |
| --- | --- | --- |
| Home | Buttons (Receive / Send / Assets) | Name card + Unlock when locked |
| Tree nav | No | WALLET / ASSETS / REWARDS / NODE / ADVANCED |
| Market | No | **Assets → Market** |
| Share lottery wins | Home | **Rewards → Lottery share** |

Same `%APPDATA%\XCoin` / `~/.xcoin`.

## What changed

### Sync / loading

Splash, IBD overlay, status-bar progress, and rescan dialogs use the
black / white wallet theme. They were a white flash with unreadable
text.

### Advanced menu

**Advanced** is one list of same-size text items (not mixed 22px tab
icons). Combo popups use the same dark item height.

### Home: unlock when locked

When the wallet is encrypted and locked, Home shows **Unlock wallet**.
Pick how long to stay unlocked (5 min / 15 min / 1 hour / 8 hours /
until you lock / custom minutes), then enter the passphrase. That is
the GUI equivalent of `walletpassphrase` with a timeout — not CLI.

Locking stops lottery heartbeat eligibility. Unlocking restores it
(still needs Sign in with X + X Verified). Unencrypted wallets do not
show these buttons.

### Version

Home prints **Wallet 1.0.17 heavy**. Help → About and the sync overlay
still show the full version string.

### Send

After a successful send, address and amount fields clear.

### Default `assetindex=1`

Packaged `xcoin.conf` now has `assetindex=1` so lottery share can look
up guest root addresses. **Not** `txindex=1`. Prune is unchanged. This
is an extra index, not a full archival node.

New wallets index from the first block. A datadir that already ran
without the index needs a one-time `-reindex` (or **Rewards → Lottery
share** → enable, which writes the same flag and a restart marker).
Do not reindex the seed.

## Package

Windows: [X-Coin-1.0.17-Windows.zip](https://github.com/NiftyRaven/x-coin/releases/download/v1.0.17/X-Coin-1.0.17-Windows.zip)
→ **X Coin Wallet.exe**

Linux: [X-Coin-1.0.17-Linux-x86_64.tar.gz](https://github.com/NiftyRaven/x-coin/releases/download/v1.0.17/X-Coin-1.0.17-Linux-x86_64.tar.gz)
→ **X Coin Wallet**

Do not use **Code → Download ZIP**. Light:
[v1.0.16-light](https://github.com/NiftyRaven/x-coin/releases/tag/v1.0.16-light).

## Related

- Previous Heavy: [RELEASE-1.0.16.md](RELEASE-1.0.16.md)
- Market: [MARKET.md](MARKET.md)
- Lottery share / heartbeat: [LOTTERY.md](LOTTERY.md)
- Wallet pages: [WALLET.md](WALLET.md)
- Sign in: [XSIGNIN.md](XSIGNIN.md)
- Go-live: [GO-LIVE.md](GO-LIVE.md)
- In-wallet What's new: [UPDATES.md](UPDATES.md)
