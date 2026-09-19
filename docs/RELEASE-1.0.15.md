# X Coin 1.0.15 Heavy

**Superseded by [1.0.18 Heavy](RELEASE-1.0.18.md).** Historical notes.

**Heavy** wallet. Market + tree nav. Same chain as **1.0.14 Light**.

User wallets **1.0.15 Heavy** from
[Releases](https://github.com/NiftyRaven/x-coin/releases/tag/v1.0.15).
Do **not** use **Code → Download ZIP**.

Consensus is unchanged. Same genesis, same lottery, same `XHB1` /
`XVA1` / `XSD1`. Protocol version stays **70028**. Light 1.0.14 and
1.0.13 peers still follow this chain. The baked seed does not need
Heavy **to print**. It **does** need a Heavy `xcoind` if Market
listings should pass through the hub.

Stay on [1.0.14 Light](RELEASE-1.0.14.md) if you want the simple Home
wallet. This zip is the heavier GUI.

| | Light 1.0.14 | Heavy 1.0.15 |
| --- | --- | --- |
| Home | Buttons (Receive / Send / Assets) | Name card only |
| Tree nav | No | WALLET / ASSETS / REWARDS / NODE / ADVANCED |
| Market | No | **Assets → Market** |
| Share lottery wins | Home | **Rewards → Lottery share** |

Same `%APPDATA%\XCoin` / `~/.xcoin`. No reindex to switch, unless you
already enabled Lottery share (`assetindex=1` on that datadir).

## What changed

### Wallet layout

Tree nav, black / white pages:

| | |
| --- | --- |
| **WALLET** | Home, Receive, Send, Activity |
| **ASSETS** | Create, Transfer, Manage, **Market** |
| **REWARDS** | Lottery share, Asset dividends |
| **NODE** | My node |
| **ADVANCED** | Balances, RPC |

Home is the name card (Sign in with X, claim root, lottery). Receive
always shows an address and QR; **Create new address** is one click.
Send takes `@handle` or an `X…` address. Double-click the labeled start.
No extra flags. Packaged `xcoin.conf` is unchanged (no `assetindex=1`).

### Market

**Assets → Market.** List asset bags for XFER. Other Heavy wallets buy
them. No copy-paste on the happy path. Each row is one UTXO
(Ctrl-click several to list more than one). Owner tokens cannot be
listed.

Listings gossip as `xord` (partial `SIGHASH_SINGLE | ANYONECANPAY`
transactions). They are not mempool txs and not consensus. Completed
buys are normal transactions. Light 1.0.14 ignores `xord` and still
confirms the buy. Book cap is 200 per node, RAM only. Details:
[MARKET.md](MARKET.md).

### Share lottery wins

An X Verified host can share a percent of **their** mature lottery
output with invited guests. On Heavy that lives under **Rewards →
Lottery share** (same RPCs: `sethostshare`, `setguestpercent`,
`inviteguest`, `listguests`). Guests sign in, claim a root, and do
**not** need a blue check. They never enter the hat.

First enable of sharing writes `assetindex=1` **in this wallet’s data
directory** and asks for a restart (reindex) so `listaddressesbyasset`
can find each guest’s current root address. That is opt-in. It is
**not** in the zip. Existing roots stay valid.

After a halving, guests get a percent of this host’s slice (for example
20% of 1250 XFER), not of the whole 2500 subsidy. This is a wallet
send. A recoded or offline host can keep the win. Fake handles still
cannot win the lottery.

### Handles and children

X handles 1–32 characters map to a root of at most 32 characters. A
**26**-character handle maps 1:1 and still has room for `_2`. A
**32**-character handle can **claim** the identity root. They cannot
issue `NAME/CHILD` or `NAME#tag` (full name would exceed 33).
[ASSETS.md](ASSETS.md).

## Package

Windows: [X-Coin-1.0.15-Windows.zip](https://github.com/NiftyRaven/x-coin/releases/download/v1.0.15/X-Coin-1.0.15-Windows.zip)
→ **X Coin Wallet.exe**

Linux: [X-Coin-1.0.15-Linux-x86_64.tar.gz](https://github.com/NiftyRaven/x-coin/releases/download/v1.0.15/X-Coin-1.0.15-Linux-x86_64.tar.gz)
→ **X Coin Wallet**

Do not use **Code → Download ZIP**. Light:
[v1.0.14](https://github.com/NiftyRaven/x-coin/releases/tag/v1.0.14).

## Related

- Market: [MARKET.md](MARKET.md)
- Light 1.0.14: [RELEASE-1.0.14.md](RELEASE-1.0.14.md)
- Lottery share: [LOTTERY.md](LOTTERY.md)
- Assets / 32-character handles: [ASSETS.md](ASSETS.md)
- Wallet pages: [WALLET.md](WALLET.md)
- Sign in: [XSIGNIN.md](XSIGNIN.md)
- Go-live: [GO-LIVE.md](GO-LIVE.md)
- In-wallet What's new: [UPDATES.md](UPDATES.md)
