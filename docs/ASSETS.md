# X Coin assets

X Coin is the first chain where you can **tokenize the world — and yourself**.
The asset engine is the backbone, simplified.

## The user is a main asset

When a person links a **verified X account**, the protocol assigns them **one
main/root asset, free**. That asset *is* the user. Users cannot `issue` a new
root. One X account → one main asset.

```
xcoin-cli addxverified NFTRVN         # operator allowlist (offline-verified)
xcoin-cli linkxaccount NFTRVN         # assign the free identity root
# or: xcoind -xaccount=NFTRVN … then linkxaccount
```

`AssignLinkedUserMainAsset(xHandleOrId, dest)` is the C++ hook. The existing
verified-X link path (`registeractivenode` after allowlist) calls it; so does
`linkxaccount`. Idempotent.

The owner’s X handle for this private test is **`NFTRVN`**. That handle
derives the root name `NFTRVN`. Operators confirm the public profile
offline (see [LOTTERY.md](LOTTERY.md)); the node never calls X.com.

## Naming

The root name is derived from the X handle. **Exact max after this
change: 32 characters** for a root or sub name (owner token `NAME!` is
33). That is two characters above Ravencoin’s 30-character root cap, so
a 26-character handle-derived name never fails the old cap and still
has room for a collision suffix.

X handles today are `[A-Za-z0-9_]`, typically 1–15 characters. This
chain accepts 1–32 (`lottery::MAX_X_HANDLE`). A valid X handle of
length **≤26 is never rejected** for length and is **never truncated**.

### Handle → root (explicit, tested)

1. Strip `@`. Lowercase is the X id; **uppercase** is the asset.
2. Charset: X allows `A-Za-z0-9_`. Assets allow `A-Z 0-9 . _`. Every X
   character maps: letters uppercased, digits kept, `_` kept. X does
   not allow `.`, so `.` never appears from a handle.
3. **Edge `_` (the one X character assets cannot lead/trail with):** a
   leading `_` becomes `X` (`_alice` → `XALICE`); a trailing `_`
   becomes `X` (`alice_` → `ALICEX`). This is a one-for-one
   substitution, not a strip, so `_alice` does not collide with
   `alice`.
4. Consecutive `__` collapses to a single `_` (assets forbid doubled
   punctuation). Then the edge rule in (3) is applied again if needed.
5. Handles shorter than 3 characters are padded on the left with `X`
   (`ab` → `XAB`, `a` → `XXA`).
6. If the result would exceed **32** characters, mapping **fails**
   (no silent truncate). A 26-character handle always fits.

**Reserved** product names: `XFER`, `XCOIN`. Historical reserved roots
from the imported engine are listed once in [FORK.md](FORK.md) and are
not assignable.

**Collision:** if the derived name is taken (or reserved), append `_`
plus the numeric X user id when known and it still fits in 32,
otherwise `_2`, `_3`, … A suffix that would push the name over 32 is
**skipped**, never applied by truncating the handle. A 26-character
root plus `_2` is 28 ≤ 32.

The assignment transaction carries an `OP_RETURN` `XID1` + normalized
handle so consensus can enforce one X account → one root.

## What you can issue

| Kind | Who | Burn | Notes |
| --- | --- | --- | --- |
| Main / root | Protocol on X-link only | **0 XFER** | Owner token `NAME!` + **1** circulating unit (`units=0`, not reissuable). |
| Sub `NAME/CHILD` | Owner of `NAME!` | **100 XFER** | |
| Unique `NAME#tag` | Owner of `NAME!` | **5 XFER** | |
| Reissue | Owner, if reissuable | **100 XFER** | Identity roots are not reissuable. |
| Restricted / qualifier / tag / freeze | — | — | **Removed.** No create, transfer, RPC, or activation. |

Messaging channels (`NAME~chan`) remain if they are independent of restricted
assets. Restricted-only RPCs and the Qt Restricted tab are gone.

`issue` of a new root is rejected with a clear error. Consensus rejects a
500-XFER root burn and accepts only a zero-burn root that includes `XID1`.

Unlinked wallets can still **receive, hold, and transfer** XFER and any
asset sent to them. Linking is required to be assigned a main asset and to
*issue* a sub/unique under it — not to hold or spend.

## Smoke

`contrib/xcoin/smoke-regtest.sh` links/assigns `smoke1` → `SMOKE1` and the
owner handle `NFTRVN` → `NFTRVN`, then a **26-character** handle
`abcdefghijabcdefghijabcdef` → `ABCDEFGHIJABCDEFGHIJABCDEF` (exact match,
no truncation), then issues a sub (100 XFER) and a unique (5 XFER).
`issue TESTASSET` is illegal.
