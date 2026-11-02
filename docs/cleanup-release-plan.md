# X-Coin Core Cleanup Release Plan (`cleanup/core-modernize`)

Status: **PLANNING / PRIVATE**. Nothing from this branch is published to users until the owner explicitly approves a public release.

## Baseline (audit, 2026-11-02)
- Code base = Ravencoin **v4.8.0** (1702/1770 files identical at launch commit 5a04908); current release **1.0.27** (HEAD d0d2039).
- vs v4.8.0: 384 files modified, 199 added, 20 removed (all renames). Depends unchanged from upstream:
  Boost 1.71.0, OpenSSL 1.1.1k (EOL), Qt 5.12.11, BDB 4.8.30, miniupnpc 2.0.20170509, libevent 2.1.12, ZeroMQ 4.3.4.
- Leftovers: 156 raven-named paths, ~4963 "raven" / 202 "RVN" mentions in src, KawPoW/ethash/X16R in 120 src files, 185 TODO/FIXME/XXX in src (excl. leveldb/secp256k1/univalue).

## Invariants (must NEVER change in this release)
Consensus rules, header/PoW validation (X16R genesis, KawPoW header checks), chainparams (magic `XFER`, ports 38443/38442, 48443/48442, 28443/28442, address prefixes 76/139/204, genesis 7c790cdb…2beb, reward/halving/block time), wire protocol & protocol version, RPC names/semantics, datadir and config file names/keys, wallet.dat format (until optional Phase 8), asset rules.

## Standard safety gate (applied to EVERY phase)
1. `make check` (unit, incl. test_* suites) + full `test/functional/test_runner.py` green; compare vs 1.0.27 baseline (no new failures).
2. regtest run: mine/produce blocks, send, assets issue/transfer, lottery paths; testnet node run.
3. **Mainnet sync-from-genesis** on an isolated machine with a fresh datadir (no wallet): tip height, `getbestblockhash` and `gettxoutsetinfo` (`hash_serialized_2`, txouts, total_amount) must equal a 1.0.27 node at the same height. Also compare `getblockhash` at sampled heights.
4. **Wallet compatibility on a COPY of a test wallet only**: open copy with new build, check balance/addresses/`listtransactions`, reopen with 1.0.27. Never real/user wallets.
5. No VPS, live node or production wallet is touched during testing.

## Phases (in order)

### Phase 0 — Baseline & CI harness (no code change)
- Files: `.github/workflows/*`, `contrib/devtools/` (new compare script `utxo-compare.sh`).
- Work: record 1.0.27 test results, warnings count, sync-from-genesis reference values (tip, best hash, UTXO hash).
- Risk: none.

### Phase 1 — Stale docs & TODO triage
- Files: `doc/release-notes/*` (Raven 2.x–4.x notes → archive or delete), `doc/*.md` Raven references, TODO/FIXME comments (comments only).
- Risk: very low (no compiled code change except comments). Gate: build + unit tests.

### Phase 2 — Rebrand filenames/identifiers (non-user-facing)
- Files: `build-aux/m4/raven_*.m4`, `configure.ac`, `Makefile.am`s, `src/qt/ravengui.*`→`xcoingui.*`, `RavenGUI` class, `test_raven`→`test_xcoin`, `contrib/debian/raven-qt.*`, `.github/workflows/build-raven.yml`.
- Keep: RPC, datadir, config, wire, magic, ports, prefixes, consensus; any `raven`-named consensus/serialization strings stay.
- Risk: low–medium (build system breakage). Gate: full standard gate; Win+Linux builds.

### Phase 3 — Boost & miniupnpc bumps
- Files: `depends/packages/boost.mk` (1.71→1.8x), `miniupnpc.mk` (→2.2.x), `build-aux/m4/ax_boost_*`, src fixes for deprecated Boost APIs (`boost::filesystem`, bind placeholders), `src/net.cpp` (UPnP API).
- Risk: medium (filesystem/datadir paths, networking). Gate: standard gate + datadir path tests on Win/Linux.

### Phase 4 — OpenSSL 3.x (or drop)
- First check usage: if only BIP70 payment requests / GUI, prefer **drop** (disable BIP70, remove OpenSSL dep). Else bump `depends/packages/openssl.mk` to 3.0 LTS.
- Files: `depends/packages/openssl.mk`, `configure.ac`, `src/qt/paymentrequest*`, `src/qt/paymentserver.*`.
- Risk: medium (GUI payment URIs). Consensus crypto uses secp256k1, not OpenSSL. Gate: standard gate + `raven:`/`xcoin:` URI tests.

### Phase 5 — Qt 5.15 LTS
- Files: `depends/packages/qt.mk` + patches, `src/qt/*` deprecated API fixes, `build-aux/m4/raven_qt.m4`.
- Risk: medium (GUI only). Gate: standard gate + manual Heavy wallet QA on Windows & Linux (send/receive/assets/lottery screens, HiDPI).

### Phase 6 — Gate KawPoW miner & mining RPCs
- Keep header/PoW validation **byte-identical** (`src/pow.cpp`, `src/primitives/block.*`, `src/crypto/ethash`, X16R hashing untouched).
- Work: put internal miner and `getblocktemplate`/`generate`/`pprpcsb` behind a build or runtime flag (default per current behaviour where the seed node needs it), no removal of validation code.
- Files: `src/miner.*`, `src/rpc/mining.cpp`, `src/init.cpp`, `configure.ac`.
- Risk: medium–high (seed-node block production). Gate: standard gate + regtest block production by seed path + verify seed producer config still works on a test copy.

### Phase 7 — Version bump, packaging, private pre-release
- Version: `configure.ac` → 1.1.0 (or owner choice); `doc/release-notes.md`.
- Reproducible builds (Guix/depends, pinned toolchain), two independent builds must match.
- Artifacts: Heavy wallet (Qt) + `xcoind`/`xcoin-cli` for Windows x64 and Linux x86_64; `SHA256SUMS` + GPG signature (`SHA256SUMS.asc`); Windows Authenticode signing if cert available.
- **Private pre-release** only (draft GitHub release / private channel) for owner testing. Public release only on explicit owner approval.

### Phase 8 — (LATER / OPTIONAL) BDB 4.8 → descriptor (SQLite) wallets
- Requires migration tool (`migratewallet`), backward-compat docs, extensive tests on wallet copies. Not part of this release unless owner requests.

## Open item
- "Include this update talked about earlier" — awaiting owner confirmation which update (likely the 25/10/65 "Hat" split change). If it is a consensus change it gets its **own phase with activation height**, separate gate, and the sync-from-genesis check must still match up to the activation height. Not started until confirmed.
