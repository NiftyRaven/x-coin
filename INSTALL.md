Building X Coin
===============

X Coin (XFER) builds with Autotools. The supported private-test path is
**`--without-gui`**. See the root [README.md](README.md) for the short
Ubuntu recipe, and `doc/build-*.md` for platform notes (use `xcoind` /
`xcoin-cli` / `xcoin-qt`).

```bash
./autogen.sh
./configure --without-gui --disable-bench --disable-tests --with-incompatible-bdb
make -j$(nproc)
src/xcoind -version
src/xcoin-cli -version
```

A wallet (Berkeley DB) is required to produce lottery blocks. Legal /
opcode leftovers from the imported tree: [docs/FORK.md](docs/FORK.md).
