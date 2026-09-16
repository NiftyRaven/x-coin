# X Coin 1.0.16 Light

**Light** wallet. Simple Home buttons. Same chain as **1.0.16 Heavy**.
Consensus unchanged.

User wallets **1.0.16 Light** from
[Releases](https://github.com/NiftyRaven/x-coin/releases/tag/v1.0.16-light).
Do **not** use **Code → Download ZIP**.

Protocol version stays **70028**. Same genesis, same lottery, same
`XHB1` / `XVA1` / `XSD1`. The baked seed does not need this zip.

This is the original Home wallet (Receive / Send / Assets / Share
lottery wins on Home). No Market. No tree nav. Heavy is a parallel
zip, not an upgrade.

## What changed from 1.0.14

### Live lottery count

The header and Home used the slot-frozen draw set, so a wallet that
first heard a node this minute could show one fewer **active** than
Heavy. Display now uses live X Verified heartbeats. Winners still use
the freeze. Consensus is unchanged.

### Master Seed Node

Home names the packaged launch seed: **Connected to Master Seed Node +
1 public node** when that is the topology, instead of **Connected to
1 peer**.

### What's new stays on Light

Light does not treat a Heavy GitHub release as an upgrade. Stay on
this zip if you want Home buttons.

Share lottery wins is unchanged.

## Package

Windows: [X-Coin-1.0.16-Windows.zip](https://github.com/NiftyRaven/x-coin/releases/download/v1.0.16-light/X-Coin-1.0.16-Windows.zip)
→ **X Coin Wallet.exe**

Linux: [X-Coin-1.0.16-Linux-x86_64.tar.gz](https://github.com/NiftyRaven/x-coin/releases/download/v1.0.16-light/X-Coin-1.0.16-Linux-x86_64.tar.gz)
→ **X Coin Wallet**

Do not use **Code → Download ZIP**. Heavy:
[v1.0.16](https://github.com/NiftyRaven/x-coin/releases/tag/v1.0.16).
