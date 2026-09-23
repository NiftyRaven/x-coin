Releases are **private** (this repository is private). The wallets are
the files on GitHub Releases, not **Code → Download ZIP**.

**Light 1.0.16** (simple Home):
[v1.0.16-light](https://github.com/NiftyRaven/x-coin/releases/tag/v1.0.16-light)

- [Windows Light](https://github.com/NiftyRaven/x-coin/releases/download/v1.0.16-light/X-Coin-1.0.16-Windows.zip) → **X Coin Wallet.exe**
- [Linux Light](https://github.com/NiftyRaven/x-coin/releases/download/v1.0.16-light/X-Coin-1.0.16-Linux-x86_64.tar.gz) → **X Coin Wallet**

**Heavy 1.0.19** (Market + tree nav). In-tree is Heavy:
[v1.0.19](https://github.com/NiftyRaven/x-coin/releases/tag/v1.0.19)

- [Windows Heavy](https://github.com/NiftyRaven/x-coin/releases/download/v1.0.19/X-Coin-1.0.19-Windows.zip) → **X Coin Wallet.exe**
- [Linux Heavy](https://github.com/NiftyRaven/x-coin/releases/download/v1.0.19/X-Coin-1.0.19-Linux-x86_64.tar.gz) → **X Coin Wallet**

Do not use 1.0.2–1.0.17 as current Heavy (1.0.17 Sign in with X
uses the suspended X app Client ID). Light 1.0.16 still follows
this chain.

Practice starts are labeled **X Coin Practice Wallet** and always pass
`-regtest`. Step-by-step: [README.md](../README.md).

Pack from a GUI build (operators):

```bash
contrib/xcoin/package-linux.sh
contrib/xcoin/package-windows.sh   # after depends HOST=x86_64-w64-mingw32
```

Do not publish this repository. Do not add public DNS seeds.
Credit **Nifty Raven (@NFTRVN on X)** only.
