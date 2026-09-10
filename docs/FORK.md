# X Coin — legal, opcode, and leftover notes

This is the **only** operator document that should still name the imported
upstream product. Everywhere else, the product is **X Coin (XFER)**.

## Upstream (required attribution)

| Field | Value |
| --- | --- |
| Imported tree | [RavenProject/Ravencoin](https://github.com/RavenProject/Ravencoin) tag **v4.8.0** |
| Upstream commit | `22549129888d02e0e08fcdb9f96f3c699167e774` |
| Product home | this repository (`NiftyRaven/x-coin`) |
| License | MIT — retain Bitcoin Core and Raven Core copyright / SPDX headers |

The first product commit on this branch is a verbatim import of that tag.
Everything after that is intentional X Coin delta. We did not write Bitcoin
or the imported core; do not strip copyright headers.

`COPYING` lists Bitcoin Core, Raven Core / Raven Developers, and X Coin
developers. Source files keep `Copyright (c) … The Raven Core developers`
and SPDX lines. That is required by the MIT license.

## What this fork is

X Coin (**XFER**) is a private hard-fork of that v4.8.0 tree:

- **Keep** the asset engine (sub / unique / transfer / messaging channels).
- **Each user is a main asset.** A **signed-in** X-link (`linkxaccount`)
  assigns one free root. No session → no main asset. Users cannot
  `issue` roots. Subs/uniques are issued under that account’s root.
- **Delete restricted assets** (qualifier / tag / freeze / restricted RPC and UI).
- **Remove** Proof-of-Work as the block-production mechanism.
- **Replace** mining with a **minute lottery** among active nodes. See [LOTTERY.md](LOTTERY.md).

## Chain identity (do not collide with the imported network)

| | Main | Testnet | Regtest |
| --- | --- | --- | --- |
| Magic | `58 46 45 52` (`XFER`) | `58 46 54 4E` (`XFTN`) | `58 46 52 54` (`XFRT`) |
| P2P port | **38443** | **48443** | **28443** |
| RPC port | **38442** | **48442** | **28442** |
| Data dir (Unix) | `~/.xcoin` | `~/.xcoin/testnet1` | `~/.xcoin/regtest` |
| Data dir (macOS) | `~/Library/Application Support/XCoin` | `…/XCoin/testnet1` | `…/XCoin/regtest` |
| Data dir (Windows) | `%APPDATA%\XCoin` | `XCoin\testnet1` | `XCoin\regtest` |
| Config / pid | `xcoin.conf` / `xcoind.pid` | same names in the network subdir | same |
| User agent | `XCoin` | | |
| Signed messages | `X Coin Signed Message:\n` | | |
| BIP44 coin type | 3844 (unofficial; imported network used 175) | 1 | 1 |

**Do not reuse** the imported network’s defaults: ports 8767/8766, 18770/18766,
18444/18443; magic `RAVN` / `RVNT` / `CROW`; datadir `~/.raven`; base58
versions 60 (`R…`) / 111 (`n…`).

**Unit:** ticker **XFER**, subunit **xferon** (1 XFER = 1e8 xferons).

**Genesis:** coinbase timestamp string and main `nTime` frozen before
any mainnet peer (see [LAUNCH.md](LAUNCH.md), [GO-LIVE.md](GO-LIVE.md)).
Main header `nTime` is **1789197360** (2026-09-12 03:16:00
America/New_York, John 3:16). The coinbase string is consensus-critical
and reads:

```
For God so loved the world, that he gave his only begotten Son, that whoso John 3:16 (KJV)
```

That is the longest prefix of the KJV John 3:16 verse that still ends
with `John 3:16 (KJV)` and fits the consensus coinbase `scriptSig`
limit of 100 bytes (`bad-cb-length` in `src/consensus/tx_verify.cpp`;
also documented on `BLOCK_VALID_TRANSACTIONS` in `src/chain.h`). The
full 157-character verse does not fit. Do **not** edit that string.
Changing it would change the genesis hash.
Hashes are X16R of that block (not the imported genesis and not PoW-ground).
`nMinimumChainWork` and `defaultAssumeValid` are zero; checkpoints and DNS
seeds are empty — publish a seed with `addnode` / `seednode`. This is **X
Coin’s own ledger** (own genesis / UTXO / assets / lottery), not an
imported Ravencoin snapshot. A lone node already stores that ledger;
other people seeing the same tip need ≥2 peers. Fair launch:
no IPO, no premine, no founder allocation. Height 0 is unspendable / not a
payday. Spendable lifetime subsidy is **20,999,994,999.727 XFER**.
`generatetoaddress` is regtest-only. Send and receive do not require a Sign in with X session. Root claim does.

**Address prefixes:** main P2PKH version **76** (`X…`), P2SH **139** (`x…`);
test/regtest P2PKH **140** (`y…`). Asset burn addresses were regenerated
for those versions.

## Intentional deltas vs v4.8.0

1. **Rebrand** of operator-facing strings, ports, magic, datadir, client name,
   binaries (`xcoind`, `xcoin-cli`, `xcoin-tx`, `xcoin-qt`).
2. **PoW gutted:** miner hash loop deleted; `CheckProofOfWork` always succeeds;
   `-gen` / `setgenerate` removed as a miner; KawPoW submit RPCs unregistered.
3. **Lottery module** in `src/lottery.{h,cpp}` + `src/rpc/lottery.cpp`, started
   from `init.cpp`. P2P `xhb` gossip (handle, user id 0) + coinbase `XHB1`
   commitment / multi-winner validation. Lottery pools (`src/pool`,
   `XPL1`, `xpl`) were removed. Active set requires a linked X session that is
   **X Verified** (users/me blue check) and a running wallet. Unverified =
   zero chance. The operator invite list cannot exclude a verified wallet.
4. **Assets** on from height 0. Main roots are protocol-assigned on a
   **signed-in** X-link (zero burn, `XID1`) — not a blue-check gate.
   User `issue` of a new root is consensus-invalid. Subs/uniques require
   the session plus `NAME!`. Restricted assets are not activated
   (`AreRestrictedAssetsDeployed()` is false).
5. KawPoW activation time pushed to the far future; header hashing stays on
   the pre-KawPoW path. Lottery does not use that hash as a work function.

## Historical opcode and reserved names (do not “fix”)

These names are **consensus / compatibility**. Renaming them would break
scripts and wallets that already speak the imported asset wire format.

| Token | Why it stays |
| --- | --- |
| `OP_RVN_ASSET` | Script opcode (CScript enum). Historical name of the asset marker. |
| `rvnq` / `rvnt` | Asset script markers on the wire. |
| Reserved roots `RVN`, `RAVEN`, `RAVENCOIN` | Not assignable as identity roots (alongside `XFER` / `XCOIN`). |
| Genesis timestamp (above) | Frozen in `src/chainparams.cpp`. |
| BIP39 English word `raven` | Standard wordlist; changing it breaks mnemonic recovery. |

Document the opcode **once** (here). Do not repeat the imported product name
in RPC help, GUI copy, man pages, or operator runbooks.

## Leftovers that are not product branding

These still match a naive `rg -i 'ravencoin|\braven\b|\brvn\b'` and are
**intentional**. They are not user-facing X Coin product names.

1. **Legal headers** — `Copyright (c) … The Raven Core developers` / `Raven Developers` / `COPYING`.
2. **Opcode / reserved names / genesis timestamp** — table above.
3. **Internal C++ identifiers** — `CRavenAddress`, `RavenGUI`, `GenerateRavens`,
   `libraven_server`, `raven-config.h`, `raven-cli.cpp`, `ravend.cpp`,
   Autotools `build_ravend` / `BUILD_RAVEND`. Renaming is compile-wide churn
   with no consensus benefit. Installed binaries are `xcoin*`.
4. **Copyright-holder guard** in `src/util.cpp` (`CopyrightHolders`) — refuses
   to drop the Raven Core line from `--version`.
5. **Qt class names** — `RavenGUI`, `RavenUnits`, `RavenAmountField`, locale
   files `raven_*.ts`. User-facing 1.0.x copy is X Coin / XFER; the X theme is
   black/white, not Ravencoin orange/green.
6. **Vendored third-party** — `src/leveldb`, `src/secp256k1`, `src/univalue`,
   `src/crypto/ctaes` (including sipa’s unrelated `raven.sipa.be` URL).
7. **Upstream `make check` vectors** — many tests hard-code imported genesis
   hashes. A red `make check` is not a private-test blocker. Use
   `contrib/xcoin/smoke-*.sh`.
8. **Packaging leftovers** — `share/pixmaps/raven*` bitmaps, some Qt widget
   object names (`ravenAtStartup`). Source SVG is `qt/res/src/xcoin.svg`;
   installed icon files are `xcoin.png` / `xcoin.icns` / `xcoin.ico`.

## Assets (simplified)

See [ASSETS.md](ASSETS.md). `issue` / `transfer` / `listassets` remain for
**sub and unique** assets under a main asset the wallet owns. Main roots are
assigned by `AssignLinkedUserMainAsset` / `linkxaccount`. Sub burn 100 XFER,
unique 5 XFER (same amounts as the imported engine; new burn addresses).
Restricted/qualifier RPCs are unregistered.

See also `assets/asset_metadata_spec.md` for the wire format.

## Honest leftovers (not private-test blockers)

- Sybil / stake weighting if a free “run a process” lottery is not enough.
- Asset BIP9 windows inherited from v4.8.0 are expired on the new main
  genesis; height-0 force-on covers assets, messaging, transfer-script size,
  enforce-value, coinbase-asset checks, and transfer-overflow.
- Public DNS seeds, public explorers, live X API keys, mobile — out of
  scope. Lottery eligibility is **X Verified** from `users/me` (blue check),
  not a shared allowlist. The invite list is optional pins. Sign in with X
  is in-tree (OAuth PKCE + `users/me`; tokens never written). Leftover imported IPs were
  removed from `chainparamsseeds.h`; arrays stay unused.
  See [LAUNCH.md](LAUNCH.md) and [AUDIT.md](AUDIT.md).

## Build

See the root README. Autotools path:

Release 1.0.x ships the Qt wallet. GUI:

```bash
./autogen.sh && ./configure --with-gui=qt5 --disable-bench --disable-tests --with-incompatible-bdb && make
```

CLI-only: `--without-gui` instead of `--with-gui=qt5`. Binary is `src/qt/xcoin-qt`.
