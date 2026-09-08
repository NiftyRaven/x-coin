# X Coin 1.0 release audit

**Nifty Raven** (@NFTRVN on X) — display name and handle only.

Runner: `contrib/xcoin/run-release-audit.sh` (8 September 2026).

All runs used throwaway `-regtest` datadirs. No mainnet node. No launch
`wallet.dat`. `~/.xcoin/wallet.dat` was not created.

## Pass / fail

| Status | Test | Notes |
| --- | --- | --- |
| PASS | `smoke-regtest` | lottery, 26-char handle, assets |
| PASS | `smoke-xsession` | typed handle rejected; session required |
| PASS | `smoke-eligibility` | unverified has zero lottery chance |
| PASS | `smoke-pool` | create / join / leave, even split |
| PASS | `smoke-gossip` | |
| PASS | `smoke-gui` | headless Qt |
| PASS | `smoke-benchmark` | |
| PASS | `smoke-isolation` | |
| PASS | `smoke-sabotage` | |
| PASS | `smoke-extra` | throwaway wallet.dat, send/receive, 26+32 handles, typed handle cannot claim Verified X, pool create/leave |
| PASS | `copy-audit` | user-facing leftover Ravencoin names in GUI, docs, RPC |
| PASS | `linux-practice-flag` | `-regtest` baked into Practice launcher |
| PASS | `linux-package-practice-datadir` | packaged `xcoind -regtest` wrote only `datadir/regtest/wallet.dat` |
| PASS | `linux-package-practice-start` | Practice launcher opened `chain=regtest` under xvfb |
| PASS | `linux-real-launcher` | real launcher has no `-regtest` |
| PASS | `linux-no-terminal-start` | **X Coin Wallet** is an ELF, not a shell script |
| PASS | `linux-practice-title-strings` | `xcoin-qt` contains Practice Wallet and `[regtest]` |
| PASS | `windows-package-layout` | zip has `xcoin-qt.exe` and two labeled starts |
| PASS | `windows-practice-flag` | `-regtest` baked into Practice launcher |
| PASS | `windows-real-launcher` | real launcher has no `-regtest` |
| PASS | `windows-dlls` | static Qt; mingw runtime DLLs bundled; remaining imports are Windows APIs |
| PASS | `windows-wine-regtest` | wine `xcoind.exe -regtest` wrote only `datadir/regtest` |
| PASS | `windows-wine-practice` | wine Practice produced a regtest `debug.log` |
| PASS | `isolation-home-wallet` | no `~/.xcoin/wallet.dat` |

Totals: **24 passed**, **0 failed**, **0 skipped**.

## Coverage map

| Asked | Result |
| --- | --- |
| Existing smokes including `contrib/xcoin/smoke-pool.sh` on regtest | PASS |
| Node start | PASS (`smoke-regtest`, `smoke-extra`, wine `xcoind.exe`) |
| Wallet create on throwaway regtest datadir | PASS |
| Send/receive on regtest | PASS (`smoke-extra`, `smoke-benchmark`) |
| Asset name length ≥ 26 and 32-character handle mapping | PASS (`smoke-extra`: 26 maps 1:1, 32 fits) |
| Pool create / join / leave | PASS (`smoke-pool` join; `smoke-extra` create/leave) |
| Lottery eligibility as implemented | PASS (`smoke-eligibility`: no session / unverified = ineligible) |
| Typed handle cannot claim verified X status | PASS (`smoke-xsession`, `smoke-extra`) |
| User-facing leftover Ravencoin names | PASS (`copy-audit`) |
| Linux GUI starts without a terminal | PASS (ELF launchers + xvfb Practice) |
| Windows zip double-click pieces | PASS (PE GUI `xcoin-qt.exe` + labeled starts + mingw DLLs; wine `-regtest`) |

## Packages

| Archive | Open this |
| --- | --- |
| `dist/xcoin-1.0.0-linux-x86_64.tar.gz` | **X Coin Wallet** (ELF) |
| `dist/xcoin-1.0.0-win-x86_64.zip` | **X Coin Wallet.exe** |

Windows was produced with the repo `depends/` path: `make HOST=x86_64-w64-mingw32`, posix mingw gcc 13, static Qt 5.12.11, out-of-tree `build-win`. `xcoin-qt.exe` is a PE32+ GUI with `QWindowsIntegrationPlugin` linked in. Practice always passes `-regtest`.

## Could not run

- **Live Sign in with X (real OAuth).** No X developer Client ID callback in this environment. Tests use `-regtest` `mockxsignin` / `-xoauthmock`.
- **A physical Windows desktop double-click.** Verified here with PE headers, bundled mingw DLLs, static Windows Qt plugin, wine `xcoind.exe -regtest`, and wine Practice producing a regtest `debug.log`.
- **Mainnet node.** Not started, on purpose.

## Kept on purpose (not renamed)

`OP_RVN_ASSET`, wire markers `rvnq` / `rvnt`, BIP39 word raven, MIT/SPDX
copyright lines, genesis coinbase string, C++ identifiers such as
`RavenGUI`. Native asset identifier name `RVN` in `addressindex.h` still
means ticker **XFER**.
