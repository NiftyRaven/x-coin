Releases are **private**. There is no public GitHub Release page on
purpose.

Pack a Linux GUI + CLI tarball from a GUI build:

```bash
./configure --with-gui=qt5 --disable-bench --disable-tests --with-incompatible-bdb
make -j$(nproc)
contrib/xcoin/package-linux.sh
# dist/xcoin-1.1.0-linux-x86_64.tar.gz
```

Do not publish this repository. Do not add public DNS seeds.
Credit **Nifty Raven (@NFTRVN on X)** only.
