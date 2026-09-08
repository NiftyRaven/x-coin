Building X Coin
===============

How to use the core wallet after you build: [README.md](README.md).
Whitepaper: [whitepaper/XCOIN.md](whitepaper/XCOIN.md).
Private package: [docs/RELEASE-1.1.md](docs/RELEASE-1.1.md).

X Coin (XFER) builds with Autotools. The supported private-test path is
**`--with-gui=qt5`** (`xcoin-qt`). CLI-only is `--without-gui`. Platform
notes: `doc/build-*.md` (use `xcoind` / `xcoin-cli` / `xcoin-qt`).

```bash
./autogen.sh
./configure --with-gui=qt5 --disable-bench --disable-tests --with-incompatible-bdb
make -j$(nproc)
src/qt/xcoin-qt -version
src/xcoind -version
src/xcoin-cli -version
```

A wallet (Berkeley DB) is required to produce lottery blocks. Legal /
opcode leftovers from the imported tree: [docs/FORK.md](docs/FORK.md).
