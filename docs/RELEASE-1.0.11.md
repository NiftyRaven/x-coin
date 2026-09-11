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

## Verify

```bash
src/test/test_raven --log_level=test_suite --run_test=xsession_tests
src/test/test_raven --log_level=test_suite --run_test=xrelease_tests
contrib/xcoin/smoke-xsession.sh
```

Workflow: `.github/workflows/package-xcoin-1.0.11.yml` (workflow_dispatch).
