Releases are **private** (this repository is private). The wallets are
the two files on the GitHub
[Releases](https://github.com/NiftyRaven/x-coin/releases/tag/v1.0.10)
page, not **Code → Download ZIP**. In-tree is **1.0.11**. Do not use 1.0.2–1.0.9.

**Light 1.0.21** is the published zip. In-tree source is Light 1.0.23.
[v1.0.21-light](https://github.com/NiftyRaven/x-coin/releases/tag/v1.0.21-light)

- [Windows Light](https://github.com/NiftyRaven/x-coin/releases/download/v1.0.21-light/X-Coin-1.0.21-Windows.zip) → **X Coin Wallet.exe**
- [Linux Light](https://github.com/NiftyRaven/x-coin/releases/download/v1.0.21-light/X-Coin-1.0.21-Linux-x86_64.tar.gz) → **X Coin Wallet**

Practice starts are labeled **X Coin Practice Wallet** and always pass
`-regtest`. Step-by-step: [README.md](../README.md).

Pack from a GUI build (operators):

```bash
contrib/xcoin/package-linux.sh
contrib/xcoin/package-windows.sh   # after depends HOST=x86_64-w64-mingw32
```

Do not publish this repository. Do not add public DNS seeds.
Credit **Nifty Raven (@NFTRVN on X)** only.
