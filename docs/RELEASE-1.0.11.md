# X Coin 1.0.11

**Nifty Raven** (@NFTRVN on X)

In-tree product notes for the 1.0.11 package. Last shipped zip before
this workflow runs is **1.0.10**. After you run **Package X Coin 1.0.11**,
attach:

- `X-Coin-1.0.11-Windows.zip` → **X Coin Wallet.exe**
- `X-Coin-1.0.11-Linux-x86_64.tar.gz` → **X Coin Wallet**

Do not use **Code → Download ZIP**. Do not use 1.0.2–1.0.10 for new
installs once 1.0.11 is on Releases.

## What's new

- **Sign-in until you close the wallet.** A valid `xsession.json` proof
  stays signed-in while the process is up. `LoadSession` does not reject
  on wall-clock `exp`. Live GUI Sign-in writes `exp=0`. Clean `xcoin-qt`
  quit still deletes the file. Headless `xcoind` still keeps it.
- **What's new in the wallet.** Home card + **Help → What's new** show
  these notes. When a newer GitHub Release is reachable, the same UI
  shows that tag's body. [UPDATES.md](UPDATES.md).
- Assets tab unchanged: sub + unique only; free MAIN via Claim after
  Sign-in. Send/receive without Sign-in. Lottery still needs X Verified.

## What did not change

- Genesis `nTime` **1789197360** (2026-09-12 03:16 AM Eastern / EDT)
- Packaged `xoauthclientid=` and `addnode=172.191.195.221:38443`
- 12-word BIP39 create/restore
- OAuth cooldown (~1 hour on deliberate Sign-in / Re-link)

## Verify (this tree)

| Test | Result |
| --- | --- |
| `xsession_tests` (6, including `wall_clock_exp_does_not_kick_live_session`) | PASS |
| `xrelease_tests` (6, parse feed / newer / draft skip / safe URL) | PASS |
| `xaccount_tests` (7) | PASS |
| `smoke-xsession.sh` (bundled notes, feed parse → v1.0.12, mocktime past `exp` stays signed in, headless keep) | PASS |
| `smoke-isolation.sh` | PASS |
| `xcoin-qt` / `xcoind` compile | PASS |

Live X OAuth and a physical Windows double-click were not run here.

Workflow: `.github/workflows/package-xcoin-1.0.11.yml` (workflow_dispatch, `ref` = `x-coin` after merge).
