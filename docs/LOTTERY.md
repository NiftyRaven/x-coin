# X Coin lottery consensus

X Coin does **not** use Proof-of-Work. Every **minute** a lottery picks who
may produce the next block and who shares that minute’s subsidy. Only nodes
linked to an **X Verified** account are lottery-eligible. An unlinked node
still syncs and relays; it cannot enter the active set, win, or produce.

**X Verified** is X’s blue check / X Premium (and business / government
org checks): a user X itself marks verified via `GET /2/users/me`
(`user.fields=verified,verified_type`). Official meaning:
[About the blue check](https://help.x.com/en/managing-your-account/about-x-bluecheck)
per [X’s verification policy](https://help.x.com/en/rules-and-policies/verification-policy).
It is **not** the operator invite list (`addxverified`).

This document is the source of truth for the algorithm.
Lottery pools (`XPL1`, create / join / leave) were removed.

## Units and schedule

| Item | Value |
| --- | --- |
| Ticker | **XFER** |
| Subunit | **xferon** — 1 XFER = 100,000,000 xferons |
| Slot length | 60 seconds (one lottery minute) |
| Height mapping | height `h` uses slot `floor(genesisTime / 60) + h` |
| Target spacing | 1 block per minute (no hashing) |
| Subsidy | 5000 XFER at height **1**, halved every `nSubsidyHalvingInterval` blocks (2,100,000 on main/test, 150 on regtest). Height 0 pays **0**. |
| Spendable lifetime | **20,999,994,999.727 XFER** (integer `>>=` halvings; ~21B minus unpaid genesis and shift dust) |

Genesis (height 0) is not a lottery block and is not a payday. The serialized genesis coinbase is never added to the UTXO set (`ConnectBlock` skips it).

## X Verified eligibility (anti-bot)

Consensus does **not** call X.com on every block. **Sign in with X**
stores `verified` / `verified_type` from `GET /2/users/me` in
`xsession.json` with the HMAC proof. Lottery eligibility for this node:

1. A valid Sign in with X session (user id + username + proof).
2. That session is **X Verified** (`verified==true`, including `blue` /
   `business` / `government` check types X exposes).
3. This wallet is running (local payout script + heartbeat).

Fair code: nobody who meets (1)–(3) can be excluded. The operator invite
list is **not** a lottery gate. A session that is **not** X Verified has
**zero chance** (no heartbeat, not in the active set).

See [XSIGNIN.md](XSIGNIN.md).

### Operator invite list (optional pins / private mesh)

Operators may keep a closed **invite / payout-pin list** for this private
test. Confirming a public profile badge in a browser is how humans decide
whom to invite — it does **not** become X Verified inside the node, and
it cannot drop a blue-check wallet from the draw. Only `users/me` does.

1. Open the public profile `https://x.com/<handle>` in a browser
   (optional human check).
2. Write the handle (no `@`) into the shared invite file if you want
   optional payout pins.
3. Every honest node loads that same file (`-xallowlist=` or
   `~/.xcoin/verified-x-accounts.txt`) when using a closed mesh.
4. The operator **Signs in with X** on that node. Typed `-xaccount=`
   is ignored.

The owner handle for this private test is **`NFTRVN`**. On `-regtest`
without live OAuth, `-xoauthmock=NFTRVN` mocks `verified=true`.

A node is lottery-eligible only when **session + X Verified + a running
wallet** are true. Unverified heartbeats from this node are **ignored**
for the active set (zero chance). The local producer refuses to produce
if this node is not X Verified. The invite list cannot exclude a
verified running wallet.

### Link an X account (every operator)

Sign in with X in `xcoin-qt` (or, on `-regtest` only, `-xoauthmock=` /
`mockxsignin`). Then:

```bash
src/xcoin-cli addxverified YourHandle   # optional operator invite list (not X Verified)
src/xcoin-cli linkxaccount              # uses the session username
src/xcoin-cli getxsession               # verified / verified_type / x_verified
src/xcoin-cli getlotteryinfo            # local_eligible must be true (session + blue check)
```

Typed `linkxaccount OtherHandle` is rejected unless that is the signed-in
username. Headless seed: sign in once on this datadir (GUI), then run
`xcoind` against the same `~/.xcoin`.

### Publish the invite list (seed / operator, optional pins)

This file is a private-mesh invite / payout-pin list, **not** X Verified
and **not** a lottery gate. Share the same file with every honest node
if you want matching pins.

```text
# verified-x-accounts.txt  (datadir or -xallowlist=)
# handle [userid] [payout-address-or-script-hex]
NFTRVN
YourHandle 123456789
alice
bob
# optional pin — only this payout may heartbeat as NFTRVN:
# NFTRVN 123456789 XyourPinnedAddressxxxxxxxxxxxxxxxxx
```

Ways to load the same list:

| Method | Example |
| --- | --- |
| Datadir file | `~/.xcoin/verified-x-accounts.txt` (loaded on start) |
| Flag | `-xallowlist=/shared/verified-x-accounts.txt` |
| Repeatable flag | `-xverified=alice -xverified=bob:99` (invite list, not X Verified) |
| RPC | `addxverified alice` / `listxverified` / `removexverified alice` |

Optional refresh later (no secrets): publish the text file at any HTTPS URL,
then each operator runs:

```bash
curl -fsSL https://example.com/verified-x-accounts.txt -o ~/.xcoin/verified-x-accounts.txt
src/xcoin-cli loadxverified
```

`contrib/xcoin/refresh-x-allowlist.sh` is a one-liner wrapper for that hook.

## Active node

A node is **active** if it has heartbeated an **X Verified** local identity
within the last **180 seconds**. Unverified gossip is dropped (zero chance).

- Node **id** = `Hash160(payout script)`. The script is what coinbase will pay.
- One verified X handle maps to one active node (a later heartbeat for the
  same handle replaces the payout script).
- On start, the node loads or creates a payout script in `lottery-payout.dat`
  under the data directory (`~/.xcoin` on Unix). When a wallet is present the
  producer adopts the wallet’s local payout script.
- The producer thread heartbeats that script every second **only if** the
  local node is linked+verified.
- `registeractivenode` records a heartbeat for the local script or a supplied
  **address / script hex**. The handle and user id must match the signed-in
  session. Unlinked/unverified registrations are rejected.
- Peers gossip `xhb` messages (`int64 timestamp` + `CScript` + X handle +
  user id **0** + compact payout signature) after `verack` and about every 30
  seconds. The numeric X user id is never put on the wire (login stays
  local). Unsigned, unlisted, userid-only, pinned-mismatch, or
  live-handle-rebind heartbeats are not added to the active set. Forged
  signatures cost banscore.

## Deterministic seed

```
seed = SHA256( prevBlockHash || LE64(slot) )
```

`prevBlockHash` is the hash of the current tip. `slot` is the lottery minute
for the **next** height. Honest nodes that agree on the tip and the slot
therefore agree on `seed`.

## Winner count (halvings)

```
winnerCount(height) = 1 + floor(height / nSubsidyHalvingInterval)
```

| Heights (mainnet interval 2,100,000) | Winners that minute |
| --- | --- |
| 1 … 2,099,999 | 1 |
| 2,100,000 … 4,199,999 | 2 |
| 4,200,000 … 6,299,999 | 3 |
| … | +1 per completed halving |

If the active set is smaller than `winnerCount`, every active node wins.

## Selection

1. Take the active ids, already sorted as unsigned 160-bit integers (std::map order).
2. Draw `k = min(winnerCount, n)` winners with a partial Fisher–Yates shuffle.
3. At step `i`, `r = SHA256(seed || LE32(i))` (first 8 bytes, little-endian) and
   swap index `i` with `i + (r % (n - i))`.
4. The **first** selected winner is the block producer for that slot.

Same active set + same seed ⇒ same winner list on every honest node.

## Rewards and coinbase

`GetBlockSubsidy(h)` is 0 at `h < 1`, else `5000 XFER >> floor(h / interval)`,
forced to 0 after 64 shifts (same 64-shift cap as the imported engine; XFER’s 5000·COIN
already reaches 0 xferons at shift 39). That amount is split across the winners:

```
base = total / k
remainder = total % k   // extra xferons go to the first `remainder` winners
```

Fees (if any) are added to the **first** winner’s output.

The producer writes:

1. One `CTxOut` per winner, in selection order, paying `Hash160(script) == winner id`.
2. An `OP_RETURN` commitment: magic `XHB1` + uint32 LE count + concatenated
   20-byte ids (must be sorted unique). This is the active set used for the draw.

`ContextualCheckBlock` rejects a height ≥ 1 coinbase that is missing the
commitment, pays the wrong count/scripts, or splits the subsidy incorrectly.

## Block production

- **Main:** the producer thread heartbeats and, if this node is a
  winner, emits at most **one block per wall-clock minute**
  (wall-minute latch). Catch-up may target an older lottery slot; the
  latch still holds. There is no slot+120 abort. There is **no** nonce
  grind. `CheckProofOfWork` is a no-op. `fMiningRequiresPeers` is
  false: a **lone eligible node** can produce and store the ledger.
  Other people seeing the same tip still need `addnode` / `seednode`.
- **Test:** waits until wall-clock slot ≥ height slot (plus a short
  settle), then emits if this node is a winner.
- **Regtest:** the producer only heartbeats. Use `generatetoaddress` /
  `generate` to assemble blocks on demand (still no PoW). The
  destination script is heartbeated so it is in the active set.
- Removed / gutted: the legacy miner hash loop, `-gen` / `setgenerate`
  as a miner, KawPoW submit helpers (`pprpcsb`, `getkawpowhash`).
  Lottery pools (`XPL1`, `xpl`, create / join / leave) were removed.

## RPCs

| Command | Purpose |
| --- | --- |
| `getlotteryinfo` | Next-height draw: slot, seed, winners, split, local id / X handle / eligibility |
| `getactivenodes` | Current registry (id, script, lastseen, xaccount). P2P heartbeats do not carry X user id |
| `registeractivenode (payout xaccount xuserid)` | Heartbeat this node or an address / script hex; rejects unlinked/unverified. User id must match the **local** session; it is not gossiped |
| `addxverified` / `listxverified` / `removexverified` | Mutate / read the operator invite list (not X Verified) |
| `loadxverified (path)` | Merge a published invite-list file (no API keys) |
| `generatetoaddress` | **Regtest only** on-demand assembly (not mining; rejected on main/test) |

## Security notes

Honest write-up: [SECURITY.md](SECURITY.md). This is not hacker-proof.

- **Sybil:** X Verified (session `users/me.verified`) is the meaning of
  Verified. Unverified accounts have zero chance. The operator invite
  list is optional pins / invites and cannot exclude a verified running
  wallet. Remote gossip still accepts compact-signed heartbeats that
  assert verified; honest wallets only set that bit from `users/me`.
  It is not a bonded public lottery: a producer can still
  commit an active set of scripts, and validation only proves
  the coinbase matches the *committed* set.
- **xhb:** gossip is compact-signed by the payout key. A live handle cannot
  be rebound to another script. A listed userid alone cannot authorize a
  different handle. Residual: first-seen after restart unless you pin a
  payout; eclipse of a node that only talks to attacker peers.
- **Unsigned-in wallets:** Sign in with X is required to send, receive,
  and prove asset ownership (`sendrawtransaction` included). Lottery
  additionally requires the session to be **X Verified** (blue check).
  Unverified = zero chance. The invite list is not a lottery gate.
- Clock skew can delay a slot; height still maps 1:1 (`slot = genesisSlot + h`).
  You cannot skip or double-pay a height on one chain. MAIN catch-up is
  latched to one block per wall-clock minute. `generatetoaddress` is
  regtest-only so RPC cannot burst-mint on main/test.
- Empty committed set ⇒ invalid coinbase (no unpaid extra outputs).

Treat the lottery as specified here; do not reintroduce PoW as the production path.
