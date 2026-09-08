# X Coin 1.0 (private)

**Superseded for operators by [RELEASE-1.1.md](RELEASE-1.1.md)** (pools,
private login, X Verified = blue check). This page is the 1.0 snapshot.

**Nifty Raven** (@NFTRVN on X)  
Anonymous public identity — display name and handle only.

Product version **1.0.0**. This is a **private** release. Do not make
the repository public. Packages: `dist/xcoin-1.0.0-linux-x86_64.tar.gz`
and `dist/xcoin-1.0.0-win-x86_64.zip`.

## What shipped

- **Desktop GUI** `xcoin-qt` — X theme (black / white / sharp X mark).
  Home (this wallet linked to @handle, lottery, claim root,
  issue sub/unique, Receive / Send / Activity / Transfer assets),
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
- Lottery among verified-X active nodes (no mining).
- One free identity root per **signed-in** handle; user-created roots forbidden.

## Install the GUI

Do not compile. Unpack the package and double-click the labeled start.
Step-by-step: [README.md](../README.md).

- Linux: `dist/xcoin-1.0.0-linux-x86_64.tar.gz` → **X Coin Wallet**
- Windows: `dist/xcoin-1.0.0-win-x86_64.zip` → **X Coin Wallet.exe**
- Practice: **X Coin Practice Wallet** / **X Coin Practice Wallet.exe**

Pack from a developer build: `contrib/xcoin/package-linux.sh` and
`contrib/xcoin/package-windows.sh`.

## Screenshots

1.0 running-GUI captures were taken on regtest. In-tree brand stills:
[assets/brand/](../assets/brand/). Home in 1.1 shows @handle only (no X
user id) plus POOL and optional Provide my node IP.

## Known limits

- Sign in with X is required to send, receive, and own assets. Lottery
  is **X Verified** (blue check from `users/me`) plus a running wallet.
  Unverified accounts have zero chance. The operator invite list cannot
  exclude a verified wallet. See [XSIGNIN.md](XSIGNIN.md).
- Live X OAuth needs a developer Client ID (`xoauthclientid=`). This VM
  cannot complete a live login; `-regtest` mock `users/me` covers tests.
- No mobile app. No public DNS seeds. No exchange listing.
  DEX criteria (ready to *apply* vs ready to *trade*): [DEX.md](DEX.md).
  Not hacker-proof: [SECURITY.md](SECURITY.md).
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

Private-test audit: [AUDIT.md](AUDIT.md). Release-day notes: [SECURITY.md](SECURITY.md).
DEX listing criteria (do not apply while private): [DEX.md](DEX.md).
