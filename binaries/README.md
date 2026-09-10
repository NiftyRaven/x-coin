Releases are **private** (this repository is private). The wallets are
the two files on the GitHub
[Releases](https://github.com/NiftyRaven/x-coin/releases/tag/v1.0.7)
page, not **Code → Download ZIP**. Do not use 1.0.2, 1.0.3, 1.0.4, 1.0.5, or 1.0.6.

- [Windows wallet](https://github.com/NiftyRaven/x-coin/releases/download/v1.0.7/X-Coin-1.0.7-Windows.zip) → double-click **X Coin Wallet.exe**
- [Linux wallet](https://github.com/NiftyRaven/x-coin/releases/download/v1.0.7/X-Coin-1.0.7-Linux-x86_64.tar.gz) → double-click **X Coin Wallet**

Practice starts are labeled **X Coin Practice Wallet** and always pass
`-regtest`. Step-by-step: [README.md](../README.md).

Pack from a GUI build (operators):

```bash
contrib/xcoin/package-linux.sh
contrib/xcoin/package-windows.sh   # after depends HOST=x86_64-w64-mingw32
```

Do not publish this repository. Do not add public DNS seeds.
Credit **Nifty Raven (@NFTRVN on X)** only.
