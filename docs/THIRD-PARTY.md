# Third-party explorers and wallets

**Nifty Raven** (@NFTRVN on X)

X Coin is a Bitcoin-family UTXO daemon plus Ravencoin-style assets.
A block explorer, asset explorer, or third-party wallet talks to
`xcoind` over JSON-RPC (port **38442** locally). There is no in-tree
explorer URL (`DEFAULT_THIRD_PARTY_BROWSERS` is empty until you publish
one).

This is a **fully functional chain**: send/receive XFER, protocol
identity roots, sub/unique issue and transfer, lottery blocks, optional
pools.

## Explorer node

Use a **fresh** datadir (indexes are set at first start; changing them
later needs `-reindex`):

```bash
src/xcoind -server -txindex=1 -assetindex=1 -addressindex=1 \
  -timestampindex=1 -spentindex=1 \
  -addnode=<seed-ip>:38443
```

| Index | What it unlocks |
| --- | --- |
| `-txindex=1` | `getrawtransaction <txid> true` for any confirmed tx |
| `-assetindex=1` | `listaddressesbyasset`, asset address balances |
| `-addressindex=1` | `getaddressbalance` / `getaddresstxids` / `getaddressutxos` |
| `-timestampindex=1` | blocks by time range |
| `-spentindex=1` | spent-by for outpoints |

Any Bitcoin-family explorer (e.g. a Ravencoin/Bitcoin fork of
insight / blockbook / electrs) pointed at this RPC can index **blocks**.
Asset UIs use the asset RPCs below.

## Block explorer RPCs

```
getblockchaininfo
getblockcount
getblockhash <height>
getblock <hash> 2
getrawtransaction <txid> true
getrawmempool
getpeerinfo
getnetworkinfo
getlotteryinfo
```

`getblock` verbosity 2 includes full txs. Combined with `-txindex=1`
this is enough to render a block explorer.

## Asset explorer RPCs

```
listassets                 # all names (optional prefix*, verbose)
getassetdata <NAME>        # units, amount, reissuable, ipfs
listaddressesbyasset NAME  # needs -assetindex
listassetbalancesbyaddress <address>
getmainasset <handle>      # identity root for a linked X handle
```

Main/root assets are **not** created with `issue`. They appear when a
signed-in user calls `linkxaccount` (zero-burn `XID1`). Subs:
`NAME/CHILD`. Uniques: `NAME#tag`.

## Third-party wallets (XFER + assets)

Same RPC as `xcoin-cli`. The node you talk to must have a **Sign in
with X** session to:

- `getnewaddress` / `sendtoaddress` / `sendrawtransaction`
- `linkxaccount` (create the **main** asset — authentication only)
- `issue` / `issueunique` / `reissue` / `transfer`

**No session → no main asset.** `issue` of a new root is rejected.
Subs and uniques are issued **under that signed-in account’s root**
(`NAME!` ownership). Lottery still needs **X Verified** (blue check);
claiming a root does not.

```
getwalletinfo
getnewaddress
sendtoaddress <X…> <amount>
listtransactions
linkxaccount
issue NAME/CHILD 1
issueunique NAME '["TAG"]'
transfer NAME/CHILD 1 <X…>
listmyassets
```

A hosted wallet is its own `wallet.dat` + its own OAuth session. Signing
in as @alice on Bob’s empty wallet does not import Alice’s coins.

REST (optional): [doc/REST-interface.md](../doc/REST-interface.md)
(`-rest` with `-txindex`).

## Related

- Go-live birth clock: [GO-LIVE.md](GO-LIVE.md)
- DEX / listing RPC: [DEX.md](DEX.md)
- Assets: [ASSETS.md](ASSETS.md)
