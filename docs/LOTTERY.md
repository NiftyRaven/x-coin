# X Coin lottery consensus

X Coin does **not** use Proof-of-Work. Every **minute** a lottery picks who
may produce the next block and who shares that minute’s subsidy. Only nodes
linked to a **verified X account** are lottery-eligible. An unlinked node
still syncs and relays; it cannot enter the active set, win, or produce.

This document is the source of truth for the algorithm.

## Units and schedule

| Item | Value |
| --- | --- |
| Ticker | **XFER** |
| Subunit | **xferon** — 1 XFER = 100,000,000 xferons |
| Slot length | 60 seconds (one lottery minute) |
| Height mapping | height `h` uses slot `floor(genesisTime / 60) + h` |
| Target spacing | 1 block per minute (same as Ravencoin’s clock, without hashing) |
| Subsidy | 5000 XFER at height **1**, halved every `nSubsidyHalvingInterval` blocks (2,100,000 on main/test, 150 on regtest). Height 0 pays **0**. |
| Spendable lifetime | **20,999,994,999.727 XFER** (integer `>>=` halvings; ~21B minus unpaid genesis and shift dust) |

Genesis (height 0) is not a lottery block and is not a payday. The serialized genesis coinbase is never added to the UTXO set (`ConnectBlock` skips it).

## Verified X eligibility (anti-bot)

Consensus does **not** call X.com on every block. Honest operators share one
explicit allowlist of **X-verified** (blue-check / X Premium verified)
accounts. That is how bots and anonymous process spam are excluded for the
private launch. There is no X OAuth product and no live API key is required.

A node is lottery-eligible only when **both** are true:

1. It has a linked X account (`-xaccount=handle` in `xcoin.conf` / flags;
   optional `-xuserid=`).
2. That account is on the operator-shared verified allowlist.

Unlinked or unverified heartbeats are **ignored** for the active set. The
local producer refuses to produce if this node is not linked+verified.

### Link an X account (every operator)

```bash
# xcoin.conf  (or flags)
xaccount=YourHandle
# optional numeric X user id
xuserid=123456789
```

### Publish the verified allowlist (seed / operator)

List only handles you have confirmed are X-verified. Share the same file
with every honest node so they agree on the active set.

```text
# verified-x-accounts.txt  (datadir or -xallowlist=)
# handle [userid]
YourHandle 123456789
alice
bob
```

Ways to load the same list:

| Method | Example |
| --- | --- |
| Datadir file | `~/.xcoin/verified-x-accounts.txt` (loaded on start) |
| Flag | `-xallowlist=/shared/verified-x-accounts.txt` |
| Repeatable flag | `-xverified=alice -xverified=bob:99` |
| RPC | `addxverified alice` / `listxverified` / `removexverified alice` |

Optional refresh later (no secrets): publish the text file at any HTTPS URL,
then each operator runs:

```bash
curl -fsSL https://example.com/verified-x-accounts.txt -o ~/.xcoin/verified-x-accounts.txt
src/xcoin-cli loadxverified
```

`contrib/xcoin/refresh-x-allowlist.sh` is a one-liner wrapper for that hook.

## Active node

A node is **active** if it has heartbeated a **verified X identity** within
the last **180 seconds**.

- Node **id** = `Hash160(payout script)`. The script is what coinbase will pay.
- One verified X handle maps to one active node (a later heartbeat for the
  same handle replaces the payout script).
- On start, the node loads or creates a payout script in `lottery-payout.dat`
  under the data directory (`~/.xcoin` on Unix). When a wallet is present the
  producer adopts the wallet mining script as the local payout.
- The producer thread heartbeats that script every second **only if** the
  local node is linked+verified.
- `registeractivenode` records a heartbeat for the local script or a supplied
  **address / script hex**, using the local `-xaccount` unless another
  allowlisted handle is passed. Unlinked/unverified registrations are rejected.
- Peers gossip `xhb` messages (`int64 timestamp` + `CScript` + X handle +
  user id) after `verack` and about every 30 seconds. Heartbeats missing a
  verified link are not added to the active set and are not relayed.

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
forced to 0 after 64 shifts (same cap as Bitcoin/Ravencoin; XFER’s 5000·COIN
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

- **Main / test:** the producer thread heartbeats, waits until wall-clock slot
  ≥ height slot, and if this node is a winner it calls `CreateNewBlock` and
  `ProcessNewBlock`. There is **no** nonce grind. `CheckProofOfWork` is a
  no-op.
- **Regtest:** the producer only heartbeats. Use `generatetoaddress` /
  `generate` to assemble blocks on demand (still no PoW). The destination
  script is heartbeated so it is in the active set.
- Removed / gutted: `RavenMiner` hash loop, `-gen` / `setgenerate` as a miner,
  KawPoW submit helpers (`pprpcsb`, `getkawpowhash`).

## RPCs

| Command | Purpose |
| --- | --- |
| `getlotteryinfo` | Next-height draw: slot, seed, winners, split, local id / X handle / eligibility |
| `getactivenodes` | Current registry (id, script, lastseen, xaccount, xuserid) |
| `registeractivenode (payout xaccount xuserid)` | Heartbeat this node or an address / script hex; rejects unlinked/unverified |
| `addxverified` / `listxverified` / `removexverified` | Mutate / read the verified X allowlist |
| `loadxverified (path)` | Merge a published allowlist file (no API keys) |
| `generatetoaddress` | **Regtest only** on-demand assembly (not mining; rejected on main/test) |

## Security notes

- **Sybil:** the verified-X allowlist stops anonymous/bot process spam for a
  private launch. Skip or leave it empty and anyone can heartbeat. It is not a
  bonded public lottery: a producer can still commit an active set of
  allowlisted scripts, and validation only proves the coinbase matches the
  *committed* set. Honest operators share one list.
- **xhb impersonation:** gossip carries a handle, not a signature binding that
  handle to a script. A malicious peer can claim an allowlisted handle.
  Consensus still binds coinbase to committed **scripts**, not handles.
- **Unlinked wallets:** X-link is **not** required to hold, receive, or send
  XFER (or to receive/transfer assets sent to you). Linking is lottery
  eligibility (and, when the sibling assigner lands, being given a main asset).
- Clock skew can delay a slot; height still maps 1:1 (`slot = genesisSlot + h`).
  You cannot skip or double-pay a height on one chain. Catch-up produces one
  height per loop when wall-clock is ahead; `generatetoaddress` is regtest-only
  so RPC cannot burst-mint on main/test.
- Empty committed set ⇒ invalid coinbase (no unpaid extra outputs).

Treat the lottery as specified here; do not reintroduce PoW as the production path.
