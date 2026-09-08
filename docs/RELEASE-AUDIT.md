# X Coin 1.0 release audit

**Nifty Raven** (@NFTRVN on X) — display name and handle only.

All runs used throwaway `-regtest` datadirs. No mainnet node. No launch
`wallet.dat`. `~/.xcoin/wallet.dat` was not created.

Runner: `contrib/xcoin/run-release-audit.sh` (8 September 2026).

## Pass / fail

| Status | Test | Notes |
| --- | --- | --- |
| PASS | `smoke-regtest` | node start, lottery, 26-char handle, assets |
| PASS | `smoke-xsession` | typed handle rejected; session required |
| PASS | `smoke-eligibility` | unverified has zero lottery chance |
| PASS | `smoke-pool` | create / join / leave, even split |
| PASS | `smoke-gossip` | |
| PASS | `smoke-gui` | headless Qt |
| PASS | `smoke-benchmark` | |
| PASS | `smoke-isolation` | |
| PASS | `smoke-sabotage` | |
| PASS | `smoke-extra` | throwaway wallet.dat, send/receive, 26+32 handles, typed handle cannot claim Verified X, pool create/leave |
| PASS | `copy-audit` | user-facing leftover Ravencoin names in GUI, docs, RPC help |
| PASS | `linux-practice-flag` | `-regtest` baked into Practice launcher |
| PASS | `linux-package-practice-datadir` | packaged `xcoind -regtest` wrote only `datadir/regtest/wallet.dat` |
| PASS | `linux-package-practice-start` | Practice launcher opened `chain=regtest` under xvfb |
| PASS | `linux-real-launcher` | real launcher has no `-regtest` |
| PASS | `linux-no-terminal-start` | **X Coin Wallet** is an ELF, not a shell script |
| PASS | `linux-practice-title-strings` | `xcoin-qt` contains Practice Wallet and `[regtest]` |
| SKIP | `windows-package` | zip not built yet (mingw `depends` Qt still compiling) |
| PASS | `isolation-home-wallet` | no `~/.xcoin/wallet.dat` |

Totals: **18 passed**, **0 failed**, **1 skipped**.

## Coverage map

| Asked | Result |
| --- | --- |
| Existing smokes including `contrib/xcoin/smoke-pool.sh` on regtest | PASS |
| Node start | PASS (`smoke-regtest`, `smoke-extra`) |
| Wallet create on throwaway regtest datadir | PASS (`smoke-extra` + packaged Practice precreate) |
| Send/receive on regtest | PASS (`smoke-extra`, `smoke-benchmark`) |
| Asset name length ≥ 26 and 32-character handle mapping | PASS (`smoke-extra`: 26 maps 1:1, 32 fits) |
| Pool create / join / leave | PASS (`smoke-pool` join; `smoke-extra` create/leave) |
| Lottery eligibility as implemented | PASS (`smoke-eligibility`: no session / unverified = ineligible) |
| Typed handle cannot claim verified X status | PASS (`smoke-xsession`, `smoke-extra`) |
| User-facing leftover Ravencoin names | PASS (`copy-audit`) |
| Linux GUI starts without a terminal | PASS (ELF launchers + xvfb Practice) |
| Windows zip double-click | SKIP — zip not produced yet |

## Could not run

- **Windows package layout, DLL bundle, Wine Practice.** `dist/xcoin-1.0.0-win-x86_64.zip` does not exist until `depends HOST=x86_64-w64-mingw32` finishes Qt 5.12.11 and `build-win` links `xcoin-qt.exe`.
- **Live Sign in with X (real OAuth).** This environment has no X developer Client ID callback. Tests use `-regtest` `mockxsignin` / `-xoauthmock`.
- **Mainnet node.** Not started, on purpose.

## Kept on purpose (not renamed)

`OP_RVN_ASSET`, wire markers `rvnq` / `rvnt`, BIP39 word raven, MIT/SPDX
copyright lines, genesis coinbase string, C++ identifiers such as
`RavenGUI`.
