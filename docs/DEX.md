# X Coin DEX listing readiness (private)

**Nifty Raven** (@NFTRVN on X)  
Anonymous public identity — display name and handle only.

This document is a **criteria checklist** for listing X Coin on a typical
UTXO / order-book DEX (TradeOgre-like, atomic-swap desks, or any venue
that wires a Bitcoin-family daemon). It is **not** an ERC-20 wrap guide
and **not** a listing application.

**Do not submit a listing. Do not publish this chain.** The repository
stays private until the owner lists. Empty `DEFAULT_THIRD_PARTY_BROWSERS`
is correct: there is no public explorer URL to ship.

## Ready to apply vs ready to trade

| | Meaning |
| --- | --- |
| **Ready to apply** | In-repo technical facts a DEX engineer can fill into a listing form: name, ticker, decimals, supply math, ports, magic, genesis, wallet RPC, whitepaper. This tree meets that bar. |
| **Ready to trade** | A public mesh, a public explorer URL, seed DNS or published `addnode`, liquidity, and a submitted application. **Missing on purpose** while the repo is private. These are listing dependencies, not code defects. |

XFER is the **base coin for pairs** (e.g. a future `XFER/BTC`). Identity
roots (`NFTRVN`, `ALICE`) and their subs/uniques are **not** the listing
ticker.

There is no in-tree ERC-20 wrap. A wrap would be a later, separate
contract + custodian/bridge decision. Do not invent one for listing.

## Criteria table

| Criterion | Status | Where / notes |
| --- | --- | --- |
| Unique name | **Met** | **X Coin** |
| Ticker | **Met** | **XFER** (`CURRENCY_UNIT`, `getblockchaininfo.currency`, `getlotteryinfo.currency`) |
| Decimals | **Met** | 8 (1 XFER = 100,000,000 xferons) |
| Max / circulating supply math | **Met** | Spendable lifetime **20,999,994,999.727 XFER**. `MAX_MONEY` = 21,000,000,000 (sanity cap). Height 0 unpaid; subsidy `5000 >> floor(h / 2,100,000)` for `h ≥ 1`. Circulating = sum of paid subsidies minus burns (subs 100, uniques 5). At height 0 circulating is **0**. |
| Own genesis / magic / ports / address version | **Met** | Not Ravencoin replay. Main genesis `db9bcd75…5347d0`, magic `XFER`, P2P **38443**, P2PKH version **76** (`X…`). [FORK.md](FORK.md), [LAUNCH.md](LAUNCH.md). |
| Working send / receive wallet (GUI + RPC) | **Met** | `xcoin-qt` Home / Receive / Send / Activity after Sign in with X. RPC `getnewaddress` / `sendtoaddress` / `sendrawtransaction` gated by session. [XSIGNIN.md](XSIGNIN.md). |
| Release-sabotage notes | **Documented** | Not hacker-proof. [SECURITY.md](SECURITY.md). |
| Ownership isolation | **Met** | Spend only keys in **this** `wallet.dat`. Session does not import another user's keys. `RequireSession` + `RequireHandle`. Smoke: [contrib/xcoin/smoke-isolation.sh](../contrib/xcoin/smoke-isolation.sh). |
| `getblockchaininfo` | **Met** | Returns `name`, `currency` (XFER), chain, height, best hash. |
| `getblock` | **Met** | By hash; verbosity 0/1/2. |
| `getrawtransaction` | **Met** | Mempool + wallet txs always. **Observer / listing nodes: `-txindex=1`.** Documented in RPC help. |
| Whitepaper | **Met** | [whitepaper/XCOIN.md](../whitepaper/XCOIN.md) |
| How-to | **Met** | [README.md](../README.md), [RELEASE-1.0.md](RELEASE-1.0.md) |
| Explorer | **Missing** (listing dependency) | No in-tree explorer. `DEFAULT_THIRD_PARTY_BROWSERS` is empty. Do not invent a public URL. A Bitcoin-family explorer pointed at `-txindex=1` `xcoind` can index blocks and assets (`listassets` / `getassetdata`) when the owner stands one up. |
| P2P port + how a seed operator publishes `addnode` | **Met** (operator docs) | Port **38443**. Join: `addnode=<trusted-peer-ip>:38443` in `xcoin.conf`. Optional Home control to share *your* listen address (off by default). Never a list of other people's IPs. [LAUNCH.md](LAUNCH.md). |
| Public DNS seeds | **Missing** (listing dependency) | `vSeeds.clear()`. Correct while private. |
| No premine / fair launch | **Met** | No IPO, no founder allocation. Genesis coinbase never enters the UTXO set. Subsidy starts at height 1. |
| Asset vs coin | **Met** | XFER = pair ticker. Identity assets are a separate layer (`XID1`, 32-char roots). |
| Linux daemon | **Met** | `xcoind` / `xcoin-cli` / `xcoin-qt` |
| Block time | **Met** | One lottery slot per minute on main/test (regtest is on-demand). |
| Confirmations (typical DEX) | **Partial** | Venue policy. Reasonable starting point: **6** confirmations (~6 minutes) for deposits; coinbase mature at **100** blocks. Not a consensus listing field. |
| Public GitHub / website | **Missing** (listing dependency) | Repo stays **private** until the owner lists. Credit **Nifty Raven (@NFTRVN on X)** only. |
| Existing liquidity | **Missing** (listing dependency) | No market, no order book, no inventory. |
| Listing application submitted | **Missing** (operator action) | Do not apply from this tree. |

## What an operator still needs (not code)

1. **Decide to go public** — flip the repo only when you intend to list.
2. **Publish a seed** — always-on host, TCP **38443**, tell operators to
   put a trusted peer IP in the config file (`addnode=`). Home can
   optionally show *your* listen address if you check **Provide my node
   IP** (off by default). Optional later: DNS `vSeeds` (do not add
   public seeds while private).
3. **Stand up an explorer** — [THIRD-PARTY.md](THIRD-PARTY.md). Any
   Bitcoin-family explorer pointed at `-txindex=1 -assetindex=1
   -addressindex=1` `xcoind`. Put that URL in the listing form and, only
   then, in `DEFAULT_THIRD_PARTY_BROWSERS` if you want a GUI "view on
   explorer" link. Until then keep it empty.
4. **Liquidity** — market-maker inventory and a BTC (or other) pair.
5. **Fill the venue's form** — name X Coin, ticker XFER, decimals 8,
   supply figures above, P2P 38443, RPC 38442, magic `XFER`, this
   whitepaper, explorer URL, seed, confirmations.

## RPC a listing engineer will hit

```bash
src/xcoind -server -txindex=1 -addnode=<seed-ip>:38443
src/xcoin-cli getblockchaininfo    # name, currency=XFER, chain, blocks
src/xcoin-cli getblock <hash>
src/xcoin-cli getrawtransaction <txid> true   # needs -txindex for non-wallet txs
src/xcoin-cli getnetworkinfo
src/xcoin-cli getlotteryinfo       # currency=XFER; not a pair ticker for assets
```

Send/receive for a **deposit wallet** still require Sign in with X on
that node (identity gate). The venue's hot wallet is its own
`wallet.dat` + its own session. Another user's session cannot spend it.

A third-party **asset wallet** uses the same RPCs (`issue`, `issueunique`,
`transfer`, `listmyassets`). It **cannot** mint a main/root: that exists
only via `linkxaccount` after Sign in with X. Subs/uniques are issued
under that session’s `NAME!`.

## Wrap path (out of scope)

If a venue only lists ERC-20s, that is a different product: a wrap
contract on some EVM chain plus custody of XFER. This repository does
not ship that. Document it here so nobody treats "deploy an ERC-20"
as the X Coin listing path.

## Related

- Private-test audit: [AUDIT.md](AUDIT.md)
- Operator runbook: [LAUNCH.md](LAUNCH.md)
- Current private package: [RELEASE-1.0.md](RELEASE-1.0.md)
