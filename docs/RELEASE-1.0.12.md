# X Coin 1.0.12

Baked seed is lottery law. User wallets **1.0.12** from
[Releases](https://github.com/NiftyRaven/x-coin/releases/tag/v1.0.12).
Do not run 1.0.11 or older.

## What changed

Two-layer lottery eligibility.

1. Official wallet: Sign in with X (`users/me`) as before. Session lasts
   until GUI quit. Help → What's new.
2. Seed: look up the handle on X. If it is not a live blue check, it is
   not eligible. The seed stamps the payout. Main/test blocks must carry
   those stamps (`XVA1`) and the seed signature (`XSD1`) or they are
   invalid.

A custom wallet cannot farm lottery XFER: it cannot produce a main/test
block and it cannot enter the hat unless that seed saw a live X blue
check. The seed is not in the hat.

Seed HTTP client **dechunks** X’s `Transfer-Encoding: chunked` body
(commit `094f159`). Without that, lookup logged `users/me response is
not a JSON object` and stamped nobody. User zips do not need this path;
only the seed daemon does.

X API bearer on the seed must be the **portal string as copied**
(keep `%2B` / `%3D`). Decoding it to `+` / `=` returns HTTP 401.

Residual: someone can still wear a **real** verified handle. The seed
can omit a live peer and peers will still accept the signed set.
Regular send/receive still works without Sign-in.

## Seed (operator)

On `172.191.195.221` only. Infrastructure: attestor key + X API lookup
+ listen. **No Sign-in.** Lottery winners are stamped users.

```
xcoind -listen=1 -port=38443 -server -bind=0.0.0.0:38443
xlookupbearer=<X API app bearer as copied from the portal>
# or: export XCOIN_X_BEARER=...
```

Install `~/.xcoin/xattestor.key` (matches the baked pubkey in
`chainparams.cpp`). Do not put the bearer or the key in the user zip.

`getlotteryinfo` on the seed: `baked_seed=true`, `x_lookup=true`,
`local_eligible=false`, `stamped_handles` = the hat.

## Package

Windows: [X-Coin-1.0.12-Windows.zip](https://github.com/NiftyRaven/x-coin/releases/download/v1.0.12/X-Coin-1.0.12-Windows.zip)
→ **X Coin Wallet.exe**

Linux: [X-Coin-1.0.12-Linux-x86_64.tar.gz](https://github.com/NiftyRaven/x-coin/releases/download/v1.0.12/X-Coin-1.0.12-Linux-x86_64.tar.gz)
→ **X Coin Wallet**

Do not use **Code → Download ZIP**.
