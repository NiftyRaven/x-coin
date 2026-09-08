# X Coin assets

X Coin is the first chain where you can **tokenize the world — and yourself**.
Ravencoin’s asset engine is the backbone, simplified.

## The user is a main asset

When a person links a **verified X account**, the protocol assigns them **one
main/root asset, free**. That asset *is* the user. Users cannot `issue` a new
root. One X account → one main asset.

```
xcoin-cli addxverified alice          # operator allowlist (lottery; sibling)
xcoin-cli linkxaccount alice          # assign the free identity root
# or: xcoind -xaccount=alice … then linkxaccount
```

`AssignLinkedUserMainAsset(xHandleOrId, dest)` is the C++ hook. The existing
verified-X link path (`registeractivenode` after allowlist) calls it; so does
`linkxaccount`. Idempotent.

## Naming

The root name is derived from the X handle:

1. Strip `@`, lowercase for the X id, **uppercase** for the asset.
2. Keep Ravencoin’s root charset `A-Z 0-9 . _`, length 3–30, no leading /
   trailing / doubled punctuation.
3. Handles shorter than 3 characters are padded with `X`.
4. **Reserved:** `XFER`, `XCOIN`, `RVN`, `RAVEN`, `RAVENCOIN` (and the
   qualifier forms). Those are not assignable as identity roots.
5. **Collision:** if the derived name is taken (or reserved), suffix with the
   numeric X user id when known, otherwise `2`, `3`, … truncated to fit 30
   characters.

The assignment transaction carries an `OP_RETURN` `XID1` + normalized handle
so consensus can enforce one X account → one root.

## What you can issue

| Kind | Who | Burn | Notes |
| --- | --- | --- | --- |
| Main / root | Protocol on X-link only | **0 XFER** | Owner token `NAME!` + **1** circulating unit (`units=0`, not reissuable). Smallest honest Ravencoin-valid default. |
| Sub `NAME/CHILD` | Owner of `NAME!` | **100 XFER** | Same Ravencoin sub burn. |
| Unique `NAME#tag` | Owner of `NAME!` | **5 XFER** | Same Ravencoin unique burn. |
| Reissue | Owner, if reissuable | **100 XFER** | Unchanged. Identity roots are not reissuable. |
| Restricted / qualifier / tag / freeze | — | — | **Removed.** No create, transfer, RPC, or activation. |

Messaging channels (`NAME~chan`) remain if they are independent of restricted
assets. Restricted-only RPCs and the Qt Restricted tab are gone.

`issue` of a new root is rejected with a clear error. Consensus rejects a
500-XFER root burn and accepts only a zero-burn root that includes `XID1`.

Unlinked wallets can still **receive, hold, and transfer** XFER and any
asset sent to them (same Ravencoin transfer rules). Linking is required to
be assigned a main asset and to *issue* a sub/unique under it — not to
hold or spend.

## Smoke

`contrib/xcoin/smoke-regtest.sh` links/assigns `smoke1` → `SMOKE1`, then issues
a sub (100 XFER) and a unique (5 XFER). `issue TESTASSET` is illegal.
