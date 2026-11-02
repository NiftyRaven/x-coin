# TODO / FIXME triage (Phase 1, 2026-11-02)

Scope: `src/` excluding vendored `leveldb`, `secp256k1`, `univalue`, `crypto/ethash`.
Whole-word `TODO|FIXME`: **112** markers (the earlier "185" count included `XXX` padding inside the asset burn addresses in `chainparams.cpp`, which are data, not markers).

Nearly all markers are inherited from Bitcoin Core 0.15 / Ravencoin 4.8.0. None were added by X Coin work.

| Bucket | Examples | Action |
|---|---|---|
| **Consensus-adjacent, DO NOT touch in cleanup** | `consensus/tx_verify.cpp:23,301,785`, `assets/assets.cpp:623,681,1408,3015,3050,5542,5623`, `rpc/rawtransaction.cpp:452,737` (Ravencoin issue 675), `chainparams.cpp:33,715`, `validation.cpp:*`, `serialize.h`, `undo.h`, `primitives/transaction.cpp` | Leave as-is; any change is consensus review. `tx_verify.cpp:785` is a debug `LogPrintf` ("remove after testing") → candidate for a log-only change in a later phase, after review. |
| **Wallet (Phase 8 scope)** | `wallet/wallet.cpp` x10, `wallet/rpcwallet.cpp` x4, `feebumper.cpp`, `fees.cpp`, `rpcdump.cpp` | Defer to descriptor-wallet work. |
| **GUI (Phase 2/5 scope)** | `qt/ravengui.cpp:479-480,1690,1698`, `qt/rpcconsole.cpp:51-53,316`, `coincontroldialog.cpp:396` / `assetcontroldialog.cpp:424` (Qt 5.3/5.4 fixes, removable with Qt 5.15) | Fix during Qt 5.15 phase. |
| **Networking / perf (upstream)** | `net_processing.cpp`, `blockencodings.cpp`, `addrman.*`, `net.cpp`, `txmempool.cpp` | Leave; upstream design notes. |
| **Mining (Phase 6 scope)** | `miner.cpp:157,248`, `miner.h:73`, `rpc/mining.cpp:526` | Review while gating mining RPCs. |
| **Vendored hash code** | `algo/*`, `crypto/sph_*` | Leave (X16R hashing, byte-identical). |
| **Build** | `src/Makefile.am:619` | Leave. |

Phase 1 changes made (comments/docs only, no compiled code change):
- Removed `doc/release-notes/` (6 imported Bitcoin/Ravencoin notes 0.15.0–2.4.0); `Makefile.am` uses `$(wildcard ...)`, so dist still works.
- Fixed comment URLs damaged by the rebrand (`RavenProject/X Coin` → `RavenProject/Ravencoin`) in `assets/assets.cpp`, `rpc/rawtransaction.cpp`.
