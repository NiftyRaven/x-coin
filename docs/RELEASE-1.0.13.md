# X Coin 1.0.13

User wallets **1.0.13** from
[Releases](https://github.com/NiftyRaven/x-coin/releases/tag/v1.0.13).
Do not run 1.0.12 or older if you need **Claim My Asset** to confirm.

Consensus is unchanged. Same genesis, same lottery, same dummy-vin `XID1`.
1.0.12 peers still follow this chain. The baked seed can stay 1.0.12
for printing; it already includes identity claims once they are in its
mempool.

## What changed

1.0.12 Claim built a valid 0-fee `XID1` root on a signed-in wallet,
then BIP133 `feefilter` hid it from the seed. The only printer never
saw the tx. GUI stayed `0/unconfirmed`.

1.0.13 announces identity claims even at 0 fee. `sendrawtransaction` of
a well-formed `XID1` no longer requires Sign-in on the submitting node
(the seed has none). Sign-in is still required on the wallet that
*builds* the claim.

PR [#47](https://github.com/NiftyRaven/x-coin/pull/47).

## Package

Windows: [X-Coin-1.0.13-Windows.zip](https://github.com/NiftyRaven/x-coin/releases/download/v1.0.13/X-Coin-1.0.13-Windows.zip)
→ **X Coin Wallet.exe**

Linux: [X-Coin-1.0.13-Linux-x86_64.tar.gz](https://github.com/NiftyRaven/x-coin/releases/download/v1.0.13/X-Coin-1.0.13-Linux-x86_64.tar.gz)
→ **X Coin Wallet**

Do not use **Code → Download ZIP**.
