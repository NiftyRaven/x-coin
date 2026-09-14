# X Coin 1.0.14

User wallets **1.0.14** from
[Releases](https://github.com/NiftyRaven/x-coin/releases/tag/v1.0.14).

Consensus is unchanged. Same genesis, same lottery, same `XHB1` / `XVA1` /
`XSD1`. 1.0.13 peers still follow this chain. The baked seed does not
need this build to print.

## What changed

An X Verified host can **Share lottery wins** on Home. They set a
percent of **their** mature lottery output and invite guests by X
handle. That percent is split equally among guests who have a claimed
root. Guests do not need a blue check and never enter the hat.

First enable writes `assetindex=1` and asks for a restart (reindex) so
`listaddressesbyasset` can find each guest’s current root address.
Existing roots stay valid.

After a halving, guests get a percent of this host’s slice (for example
20% of 1250 XFER), not of the whole 2500 subsidy.

This is a wallet send. A recoded or offline host can keep the win.
Fake handles still cannot win the lottery.

## Package

Windows: [X-Coin-1.0.14-Windows.zip](https://github.com/NiftyRaven/x-coin/releases/download/v1.0.14/X-Coin-1.0.14-Windows.zip)
→ **X Coin Wallet.exe**

Linux: [X-Coin-1.0.14-Linux-x86_64.tar.gz](https://github.com/NiftyRaven/x-coin/releases/download/v1.0.14/X-Coin-1.0.14-Linux-x86_64.tar.gz)
→ **X Coin Wallet**

Do not use **Code → Download ZIP**.
