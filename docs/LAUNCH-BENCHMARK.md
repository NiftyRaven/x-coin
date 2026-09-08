# X Coin 1.0 launch-readiness benchmark

**Nifty Raven** (@NFTRVN on X) — display name and handle only.

Runner: `contrib/xcoin/run-release-audit.sh` (2026-09-08 14:52:23 UTC).
This is a fresh run, not a copy of an earlier table.

Verdict: visit GitHub, download, double-click without compiling — **YES — a person with repo access who opens https://github.com/NiftyRaven/x-coin (no branch picker) sees the two Releases download links first, is told not to use Code → Download ZIP, and can double-click the labeled starts. Unauthenticated visitors still 404 because the repository is private.**

Totals: **32 passed**, **0 failed**, **0 skipped**.

Detail table: [RELEASE-AUDIT.md](RELEASE-AUDIT.md).

## What a person does

1. Open the repository (default branch, no branch picker).
2. Click **Download Windows wallet** or **Download Linux wallet** (GitHub Release v1.0.0).
3. Extract. The labeled start is in that first folder.
4. Double-click **X Coin Wallet** / **X Coin Wallet.exe**. Practice is the other labeled start and always `-regtest`.
5. Do **not** use **Code → Download ZIP** and do **not** compile.

## Anti-abuse / lottery / Sign in with X (regtest mock)

| Status | Case |
| --- | --- |
| PASS | Bot / unverified = zero lottery chance; can still receive | this run |
| PASS | Typed handle cannot claim Verified X or someone else's root | this run |
| PASS | Unsigned sendraw / handle steal rejected | this run |
| PASS | Bob cannot spend Alice; stolen session does not import keys | this run |
| PASS | Two distinct verified handles, two wallets, send/receive | this run |
| PASS | Same verified handle on two wallets: observer keeps one live binding | this run |
| PASS | Operator invite list cannot exclude a verified wallet | this run |
| PASS | Tampered xsession.json / foreign session file cannot send | this run |

Live OAuth was not run (no X Client ID callback in this VM). Every mock path above was.

## Node / wallet / send / receive / pool / lottery

| Status | Case |
| --- | --- |
| PASS | Full benchmark (mesh, send visible without an explorer) | this run |
| PASS | Pool create / join / leave | this run |
| PASS | 26- and 32-character handles | this run |
| PASS | Linux ELF double-click Practice [regtest] | this run |
| PASS | Windows zip labeled starts + wine start | this run |
| PASS | Practice never writes the main ledger or ~/.xcoin/wallet.dat | this run |

## Could not run

- **Live Sign in with X (real OAuth against api.x.com).** No X developer Client ID or loopback callback in this environment. Coverage is `-regtest` `mockxsignin` / `-xoauthmock` / JSON `users/me` payloads, plus HMAC session-file tamper.
- **A physical Windows PC double-click.** This VM used zip layout, PE imports, and wine64 + Xvfb. Labeled starts did write throwaway `datadir/regtest` only.
- **Mainnet node / launch wallet.dat.** Not started and not touched, on purpose. Isolated `-regtest` practice coins are not main XFER.
- **Unauthenticated GitHub visit.** The repository is private, so a logged-out browser 404s. Download-first only applies to someone who can see the repo.
- **Public DNS seeds / explorer.** Absent on purpose (private until 12 September 2026).

## Kept on purpose (not renamed)

`OP_RVN_ASSET`, `rvnq` / `rvnt`, BIP39 word raven, MIT/SPDX copyright,
genesis coinbase string, C++ names such as `RavenGUI`.
