# X Coin 1.0 release audit

**Nifty Raven** (@NFTRVN on X) — display name and handle only.

Historical 1.0.0 audit (2026-09-08). Current wallets are **1.0.6**.
Lottery pools recorded in this run were later removed.

Runner: `contrib/xcoin/run-release-audit.sh` (2026-09-08 14:52:23 UTC).

Smokes used `/workspace/audit-logs/linux-runtime/xcoin-1.0.0-linux-x86_64/bin/xcoind`
(packaged Linux archive; `src/xcoind` was not built in this tree).

All runs used throwaway `-regtest` datadirs. No mainnet node. No launch
`wallet.dat`. `~/.xcoin/wallet.dat` was not created.

## Pass / fail

| Status | Test | Notes |
| --- | --- | --- |
| PASS | `smoke-regtest` |  |
| PASS | `smoke-xsession` |  |
| PASS | `smoke-eligibility` |  |
| PASS | `smoke-pool` |  |
| PASS | `smoke-gossip` |  |
| PASS | `smoke-gui` |  |
| PASS | `smoke-benchmark` |  |
| PASS | `smoke-isolation` |  |
| PASS | `smoke-sabotage` |  |
| PASS | `smoke-extra` |  |
| PASS | `smoke-abuse` |  |
| PASS | `readme-release-links` | first two links are Windows zip and Linux tar.gz on Releases |
| PASS | `copy-audit` |  |
| PASS | `linux-package-top-level` | X Coin Wallet and Practice sit in the first unpacked folder |
| PASS | `linux-package-layout` | labeled starts + bin/xcoin-qt |
| PASS | `linux-practice-flag` | -regtest baked into Practice launcher |
| PASS | `linux-package-practice-datadir` | xcoind -regtest wrote only datadir/regtest/wallet.dat |
| PASS | `linux-package-practice-start` | Practice launcher opened chain=regtest |
| PASS | `linux-real-launcher` | real launcher has no -regtest |
| PASS | `linux-no-terminal-start` | X Coin Wallet is an ELF, not a shell script |
| PASS | `linux-practice-title-strings` | xcoin-qt contains Practice Wallet and [regtest] |
| PASS | `windows-package-layout` | zip has xcoin-qt.exe and two labeled starts |
| PASS | `windows-package-top-level` | exe files sit in the first unpacked folder, not nested |
| PASS | `windows-practice-flag` | -regtest baked into Practice launcher |
| PASS | `windows-real-launcher` | real launcher has no -regtest |
| PASS | `windows-dlls` | non-system imports are Windows APIs or bundled next to the exe (static Qt) |
| PASS | `windows-wine-regtest` | wine xcoind.exe -regtest wrote only datadir/regtest |
| PASS | `windows-wine-practice` | wine Practice wrote throwaway datadir/regtest/debug.log |
| PASS | `windows-wine-wallet-start` | wine X Coin Wallet.exe with extra -regtest wrote throwaway datadir/regtest only |
| PASS | `isolation-home-wallet` | no ~/.xcoin/wallet.dat |
| PASS | `github-homepage-readme` | default branch (NFTRVN) README leads with the two Releases downloads |
| PASS | `github-release-v1` | published, not draft; Windows zip + Linux tar.gz attached |

Totals: **32 passed**, **0 failed**, **0 skipped**.

## Coverage map

| Asked | Result |
| --- | --- |
| PASS | Existing smokes including pool on regtest | this run |
| PASS | Node start | this run |
| PASS | Wallet create on throwaway regtest datadir | this run |
| PASS | Send/receive on regtest | this run |
| PASS | 26- and 32-character handles | this run |
| PASS | Pool create / join / leave | this run |
| PASS | Typed handle cannot claim Verified X | this run |
| PASS | User-facing leftover Ravencoin names | this run |
| PASS | Linux GUI starts without a terminal | this run |
| PASS | Practice always -regtest; never writes main ledger or ~/.xcoin/wallet.dat | this run |
| PASS | Windows zip layout (labeled starts at top of first folder) | this run |
| PASS | Windows wine start | this run |
| PASS | README first section is Releases download links | this run |
| PASS | Bot / unverified account cannot lottery | this run |
| PASS | Typed handle cannot claim Verified X or someone else's root | this run |
| PASS | Two distinct verified wallets both eligible; send/receive | this run |
| PASS | Same verified handle on two wallets (duplicate / spoof) rejected | this run |
| PASS | Operator invite list cannot exclude a verified wallet | this run |
| PASS | Session file / mock mismatch cannot send | this run |
| PASS | GitHub default-branch README is download-first | this run |
| PASS | GitHub Release v1.0.0 is published with both wallet assets | this run |

## Packages

| Archive | Open this |
| --- | --- |
| `dist/xcoin-1.0.0-linux-x86_64.tar.gz` | **X Coin Wallet** (ELF) in the first unpacked folder |
| `dist/xcoin-1.0.0-win-x86_64.zip` | **X Coin Wallet.exe** in the first unpacked folder |

Those were the assets at this 1.0.0 run. Current wallets:
[X-Coin-1.0.6-Windows.zip](https://github.com/NiftyRaven/x-coin/releases/download/v1.0.6/X-Coin-1.0.6-Windows.zip)
and
[X-Coin-1.0.6-Linux-x86_64.tar.gz](https://github.com/NiftyRaven/x-coin/releases/download/v1.0.6/X-Coin-1.0.6-Linux-x86_64.tar.gz).

## Could not run

- **Live Sign in with X (real OAuth).** No X developer Client ID callback in this environment. Tests use `-regtest` `mockxsignin` / `-xoauthmock`.
- **A physical Windows desktop double-click.** Windows coverage here is zip layout, PE headers / imports, and wine when wine is installed.
- **Mainnet node.** Not started, on purpose.
- No tests were skipped in this run besides the three bullets above.

## Kept on purpose (not renamed)

`OP_RVN_ASSET`, wire markers `rvnq` / `rvnt`, BIP39 word raven, MIT/SPDX
copyright lines, genesis coinbase string, C++ identifiers such as
`RavenGUI`. Native asset identifier name `RVN` in `addressindex.h` still
means ticker **XFER**.
