# X Coin 1.0.23

Policy only. Consensus unchanged. Block validity, CheckTransaction, tx_verify.cpp, and ConnectBlock are the same as 1.0.22.

Claiming a name needs Sign in with X. The wallet asks the name signer for a proof and attaches it. If that signer is unreachable, only new claims pause. Coins, Launch, and existing names are unaffected. Older wallets still send and receive.

- An X name claim can carry a site proof (XPF1) as a second push on the same XID1 OP_RETURN. The proof names a signer key id. Old nodes still read the first push.
- `-xclaimproofpubkey=<id:hex>` is repeatable, or a comma list. When at least one key is set, the mempool and the lottery block assembler reject a claim with no valid proof. Empty leaves that check off.
- A structurally valid XPF1 is exempt from the 83-byte datacarrier limit, up to 160 bytes, whether or not a pubkey is set. `IsStandard()` on the script itself still rejects size over 83, which is what 1.0.19 and 1.0.20 do. Those peers do not relay the loose proof-carrying claim. They still accept a block that contains it.
- Desktop `linkxaccount` and the claim button fetch the proof from `-xclaimproofurls` after Sign in with X and attach it. Mainnet tries https://xferchain.net/api/x/claim-proof, then http://172.191.195.221:38445/claim-proof. Regtest and testnet stay unproofed unless that list is set.

Heavy and Light are both 1.0.23. Package them with the existing workflow_dispatch workflows (`package-xcoin-1.0.23.yml` and `package-xcoin-1.0.23-light.yml`). The Linux, Windows, and SHA256SUMS artifacts are the release zips. Operator cutover is `docs/XCLAIM-PROOF.md`. Do not deploy from this change.
