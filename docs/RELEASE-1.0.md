# X Coin 1.0 (private)

**Nifty Raven** (@NFTRVN on X)  
Anonymous public identity — display name and handle only.

Product version **1.0.0**. Tag on this branch: `v1.0.0` (see git tags).
This is a **private** release. Do not make the repository public.

## What shipped

- **Desktop GUI** `xcoin-qt` — X theme (black / white / sharp X mark).
  Overview, Send, Receive, Assets, lottery status. Same node/wallet as CLI.
- **CLI** `xcoind` / `xcoin-cli` still work.
- Handle → root mapping: X handles `[A-Za-z0-9_]` length 1–32; **26-character
  handles map 1:1** (no truncation). Root names max **32** characters.
- Lottery among verified-X active nodes (no mining).
- One free identity root per verified handle; user-created roots forbidden.

## Install the GUI (Ubuntu)

Dependencies:

```bash
sudo apt-get install -y build-essential libtool autotools-dev automake pkg-config \
    bsdmainutils python3 libevent-dev libboost-all-dev libssl-dev libdb++-dev \
    qtbase5-dev qttools5-dev qttools5-dev-tools libqt5svg5-dev \
    libqrencode-dev protobuf-compiler libprotobuf-dev
```

Configure **with GUI** (this is the 1.0 line):

```bash
./autogen.sh
./configure --with-gui=qt5 --disable-bench --disable-tests --with-incompatible-bdb
make -j$(nproc)
```

CLI-only alternative: `./configure --without-gui --disable-bench --disable-tests --with-incompatible-bdb`

Run:

```bash
src/qt/xcoin-qt                 # main
src/qt/xcoin-qt -regtest        # local practice
```

`make install` installs `xcoin-qt`, `xcoind`, `xcoin-cli` into the prefix
(default `/usr/local`). Pack a Linux tarball with
`contrib/xcoin/package-linux.sh` — output is
`dist/xcoin-1.0.0-linux-x86_64.tar.gz` (`bin/xcoin-qt` plus CLI).

## Screenshots

Running-GUI captures (regtest) are in [docs/gui/](gui/):

- `overview.png` — balances, lottery card, NFTRVN asset list
- `send.png`
- `receive.png`
- `assets.png` — Transfer Assets with NFTRVN
- `lottery.png` — lottery card / header
- `about.png` — Help → About (Nifty Raven @NFTRVN, 1.0)

## Known limits

- No X.com OAuth inside the wallet. Operators confirm handles offline.
- No mobile app. No public DNS seeds. No exchange listing.
- Restricted assets stay removed.
- Internal C++ names (`RavenGUI`, `OP_RVN_ASSET`, copyright headers) stay;
  catalog: [FORK.md](FORK.md).
- Collision suffixes never truncate a valid handle; a suffix that would
  exceed 32 characters is skipped. A 26-character handle plus `_2` always fits.
- Leading/trailing `_` in an X handle is mapped to `X` (`_alice` → `XALICE`).
- Dark theme is the default. Light mode is still black/white, not Ravencoin
  orange/green.

## Tests

```bash
contrib/xcoin/smoke-regtest.sh
contrib/xcoin/smoke-gossip.sh
contrib/xcoin/smoke-eligibility.sh
contrib/xcoin/smoke-gui.sh          # headless Qt + screenshot helper
```

`smoke-regtest.sh` includes `linkxaccount` of a 26-character handle.
