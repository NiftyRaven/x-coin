# X Coin 1.0.12

Two-layer lottery eligibility.

1. Official wallet: Sign in with X (`users/me`) as before.
2. Seed: look up the handle on X. If it is not a live blue check, it is
   not eligible. The seed stamps the payout. Main/test blocks must carry
   those stamps (`XVA1`) or they are invalid.

A custom wallet cannot farm lottery XFER: it cannot produce a main/test
block (only the baked seed’s `XSD1` signature is valid) and it cannot
enter the hat unless that seed saw a live X blue check.

Residual: someone can still wear a real verified handle. Regular
send/receive still works without Sign-in.

## Seed (you)

On `172.191.195.221` only:

```
xlookupbearer=<X API app bearer>
# or: export XCOIN_X_BEARER=...
```

Install `~/.xcoin/xattestor.key` (the key that matches the baked pubkey
in `chainparams.cpp`). Do not put the bearer or the key in the user zip.

## Package

[Package X Coin 1.0.12](https://github.com/NiftyRaven/x-coin/actions/workflows/package-xcoin-1.0.12.yml)
— `ref` = `x-coin` after merge.
