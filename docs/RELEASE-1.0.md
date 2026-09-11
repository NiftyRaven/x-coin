# X Coin 1.0 (private)

**Current wallets: 1.0.11.** Lottery pools (`XPL1`, create / join / leave)
were removed. This page is the 1.0 product snapshot. No pool sell.

**Nifty Raven** (@NFTRVN on X)  
Anonymous public identity — display name and handle only.

Product version **1.0**. This is a **private** release. Do not make
the repository public. The **1.0.0** Windows/Linux zips were
launch-blocking (invisible create/restore choice, empty-wallet fee
wall on the free root, Sign in with X asked users to paste a Client
ID). Use **1.0.11** on
[Releases](https://github.com/NiftyRaven/x-coin/releases/tag/v1.0.11)
(`X-Coin-1.0.11-Windows.zip` and `X-Coin-1.0.11-Linux-x86_64.tar.gz`)
after the package workflow is run. Until then the last shipped zips are
**1.0.8**. Do not use **Code → Download ZIP**. Do not use 1.0.2–1.0.10.

## What shipped

- **Desktop GUI** `xcoin-qt` — X theme (black / white / sharp X mark).
  Home (this wallet linked to @handle, lottery, claim root,
  Receive / Send / Activity / Transfer assets), left-nav **Assets**
  (create sub/unique via `createassetdialog`; no MAIN),
  Receive (address + Copy), Send (paste + amount + Send), Activity.
  Same node/wallet as CLI. No CLI required for the happy path.
  **This wallet + this X session = you** — another user cannot send
  from inside your wallet. **12-word BIP39 create/restore is unchanged**
  — Sign in with X does not replace the seed ([WALLET.md](WALLET.md)).
- **Linux and Windows folders** with two labeled starts: the real
  wallet and **Practice** (always `-regtest`, isolated datadir,
  window title says `[regtest]`).
- **CLI** `xcoind` / `xcoin-cli` still work.
- Handle → root mapping: X handles `[A-Za-z0-9_]` length 1–32; **26-character
  handles map 1:1** (no truncation). Root names max **32** characters.
- Lottery among verified-X active nodes (no mining). No pools.
- One free identity root per **signed-in** handle; user-created roots forbidden.

## Install the GUI

Do not compile. Download from
[Releases](https://github.com/NiftyRaven/x-coin/releases/tag/v1.0.8),
unpack, and double-click the labeled start. Step-by-step:
[README.md](../README.md).

- Linux: [X-Coin-1.0.8-Linux-x86_64.tar.gz](https://github.com/NiftyRaven/x-coin/releases/download/v1.0.8/X-Coin-1.0.8-Linux-x86_64.tar.gz) → **X Coin Wallet**
- Windows: [X-Coin-1.0.8-Windows.zip](https://github.com/NiftyRaven/x-coin/releases/download/v1.0.8/X-Coin-1.0.8-Windows.zip) → **X Coin Wallet.exe**
- Practice: **X Coin Practice Wallet** / **X Coin Practice Wallet.exe**

Pack from a developer build: `contrib/xcoin/package-linux.sh` and
`contrib/xcoin/package-windows.sh`.

## Screenshots

1.0 running-GUI captures were taken on regtest. In-tree brand stills:
[assets/brand/](../assets/brand/). Home shows @handle only (no X
user id) plus optional Provide my node IP.

## Known limits

- Send and receive work without Sign in with X (normal wallet). Claiming
  the free root needs a session. Lottery is **X Verified** (blue check
  from `users/me`) plus a running wallet.
  Unverified accounts have zero chance. The operator invite list cannot
  exclude a verified wallet. See [XSIGNIN.md](XSIGNIN.md).
- Live X OAuth uses the operator Client ID already baked in package
  `xcoin.conf` (`xoauthclientid=`). Users never paste a Client ID.
  This VM cannot complete a live login; `-regtest` mock `users/me`
  covers tests.
- No mobile app. No public DNS seeds. No exchange listing.
  DEX criteria (ready to *apply* vs ready to *trade*): [DEX.md](DEX.md).
  Not hacker-proof: [SECURITY.md](SECURITY.md).
- Restricted assets stay removed. Lottery pools were removed.
- Internal C++ names (`RavenGUI`, `OP_RVN_ASSET`, copyright headers) stay;
  catalog: [FORK.md](FORK.md).
- Collision suffixes never truncate a valid handle; a suffix that would
  exceed 32 characters is skipped. A 26-character handle plus `_2` always fits.
- Leading/trailing `_` in an X handle is mapped to `X` (`_alice` → `XALICE`).
- Dark theme is the default. Light mode is still black/white, not the
  imported orange/green.

## Tests

```bash
contrib/xcoin/smoke-xsession.sh    # typed handle rejected; session accepted
contrib/xcoin/smoke-regtest.sh
contrib/xcoin/smoke-gossip.sh
contrib/xcoin/smoke-eligibility.sh
contrib/xcoin/smoke-gui.sh          # headless Qt (sets XDG_RUNTIME_DIR)
contrib/xcoin/smoke-benchmark.sh    # lone ledger + 3-node visible send
contrib/xcoin/smoke-isolation.sh    # Bob cannot spend Alice's wallet.dat
contrib/xcoin/smoke-sabotage.sh     # unsigned sendraw / handle steal rejected
```

`smoke-regtest.sh` includes `linkxaccount` of a 26-character handle.
`smoke-benchmark.sh` answers “are txs visible?” and “own ledger + more
than one node” on a private mesh (no public explorer).

Private audit: [AUDIT.md](AUDIT.md). Release-day notes: [SECURITY.md](SECURITY.md).
DEX listing criteria (do not apply while private): [DEX.md](DEX.md).
