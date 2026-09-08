# X Coin fork notes

## Upstream

| Field | Value |
| --- | --- |
| Upstream | [RavenProject/Ravencoin](https://github.com/RavenProject/Ravencoin) tag **v4.8.0** |
| Upstream commit | `22549129888d02e0e08fcdb9f96f3c699167e774` |
| Product home | this repository (`NiftyRaven/x-coin`). Do not push to RavenProject. |
| License | MIT — retain Bitcoin Core and Raven Core copyright / SPDX headers |

The first product commit on this branch is a verbatim import of that tag. Everything after that is intentional X Coin delta.

## What this fork is

X Coin (**XFER**) is a Ravencoin hard-fork aimed at simple peer-to-peer transfers (including between users on X) plus Ravencoin-style **user-created assets/tokens**.

- **Keep** the asset system (issue / reissue / unique / qualifier / restricted / messaging). Do not rip it out.
- **Remove** Proof-of-Work as the block-production mechanism.
- **Replace** mining with a **minute lottery** among active nodes. See [LOTTERY.md](LOTTERY.md).

## Chain identity

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
| BIP44 coin type | 3844 (unofficial; Ravencoin was 175) | 1 | 1 |

Ravencoin defaults (8767/8766, 18770/18766, 18444/18443, magic `RAVN` / `RVNT` / `CROW`, `~/.raven`) must not be reused.

**Unit:** ticker **XFER**, subunit **xferon** (1 XFER = 1e8 xferons). Display currency is `XFER` (`CURRENCY_UNIT`).

**Genesis:** timestamp string and times frozen 2026-09-08 (see [LAUNCH.md](LAUNCH.md)). Hashes are X16R of that block (not Ravencoin’s genesis and not PoW-ground). `nMinimumChainWork` and `defaultAssumeValid` are zero; checkpoints and DNS seeds are empty — publish a seed with `addnode` / `seednode`.

**Address prefixes:** main P2PKH version **76** (`X…`), P2SH **139** (`x…`); test/regtest P2PKH **140** (`y…`). Asset burn addresses were regenerated for those versions.

## Intentional deltas vs v4.8.0

1. **Rebrand** of operator-facing strings, ports, magic, datadir, client name, binaries (`xcoind`, `xcoin-cli`, `xcoin-tx`, `xcoin-qt`).
2. **PoW gutted:** `RavenMiner` hash loop deleted; `CheckProofOfWork` always succeeds; `-gen` / `setgenerate` removed as a miner; KawPoW submit RPCs unregistered.
3. **Lottery module** in `src/lottery.{h,cpp}` + `src/rpc/lottery.cpp`, started from `init.cpp`. P2P `xhb` gossip + coinbase `XHB1` commitment / multi-winner validation. Active set requires a linked X account on the operator-shared verified allowlist.
4. **Assets retained** and turned on from height 0 (`nAssetActivationHeight = 0`, messaging/restricted activation 0). Reserved root names now include `XFER` / `XCOIN` as well as `RVN` / `RAVEN` / `RAVENCOIN`. Asset script opcodes (`OP_RVN_ASSET`, `rvnq` / `rvnt` markers) are **unchanged** so the Ravencoin asset protocol still works.
5. KawPoW activation time pushed to the far future; header hashing stays on the pre-KawPoW path. Lottery does not use that hash as a work function.

## Assets (kept)

Ravencoin’s user-created asset layer is still in `src/assets/` and the `issue` / `transfer` / `listassets` RPC family. Burn *fees* are the same amounts as v4.8.0 (500 XFER to issue a root, etc.). Burn *addresses* are new X Coin base58 strings (see [LAUNCH.md](LAUNCH.md)).

See also `assets/asset_metadata_spec.md` from upstream.

## What we did not change (on purpose)

- Internal C++ type names (`CRavenAddress`, `libraven_server`, `raven-config.h`, `OP_RVN_ASSET`, …) — renaming them is a compile-wide churn with no consensus benefit.
- Internal C++ type names in Qt (`RavenGUI`, `raven.cpp` entry). The installed binary is `xcoin-qt`; source SVG is also `qt/res/src/xcoin.svg`.
- Full clean of every “Raven” comment in third-party and test trees.

## Honest leftovers (not private-launch blockers)

- Sybil / stake weighting if a free “run a process” lottery is not enough.
- Asset BIP9 windows inherited from Ravencoin are expired on the new main
  genesis; height-0 force-on covers assets, messaging, transfer-script size,
  enforce-value, coinbase-asset checks, and transfer-overflow.
- Make `make check` green against the new genesis (many upstream tests hard-code Ravencoin hashes).
- DNS seeds, explorers, X OAuth / live API keys, mobile — out of scope. Verified-X eligibility is a shared allowlist, not an X API client. See [LAUNCH.md](LAUNCH.md).

## Build

See the root README. Autotools path is unchanged: `./autogen.sh && ./configure --without-gui --disable-bench && make`.
