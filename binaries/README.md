Releases are **private**. There is no public GitHub Release page on
purpose.

People run a folder, not this source tree:

- Linux: `dist/xcoin-1.0.0-linux-x86_64.tar.gz` → double-click **X Coin Wallet**
- Windows: `dist/xcoin-1.0.0-win-x86_64.zip` → double-click **X Coin Wallet.exe**

Practice starts are labeled **X Coin Practice Wallet** and always pass
`-regtest`. Step-by-step: [README.md](../README.md).

Pack from a GUI build:

```bash
contrib/xcoin/package-linux.sh
contrib/xcoin/package-windows.sh   # after depends HOST=x86_64-w64-mingw32
```

Do not publish this repository. Do not add public DNS seeds.
Credit **Nifty Raven (@NFTRVN on X)** only.
