# X Coin release-day security

**Nifty Raven** (@NFTRVN on X)

Ship is **1.0.12**. Nothing here is “hacker proof.” This is a **small
fair-launch UTXO + lottery mesh** whose **baked seed is lottery law**
on main/test. Honest operators plus the checks below make *cheap*
sabotage fail. A stolen attestor key, an eclipsed lone node, or a
clique of **real** X Verified accounts can still hurt you.

Downloads:
[Releases v1.0.12](https://github.com/NiftyRaven/x-coin/releases/tag/v1.0.12).
There are no public DNS seeds on purpose.

## What 1.0.12 closed

The issue that was actually shipping: a modified wallet could assert
`verified=true`, enter the hat, and (on older builds) emit a block.

On **1.0.12 main/test**:

| Attack | Status |
| --- | --- |
| User app emits a height ≥ 1 block | **Dead.** Only the baked seed’s `XSD1` is valid. |
| Flip `verified=true` and join the hat | **Dead.** Seed looks the handle up on X. No live blue check → no `XVA1` stamp → not in the draw. |
| Invent `@nobody` in `XHB1` | **Dead.** Every id in `XHB1` must carry a seed `XVA1` stamp peers check against the baked pubkey. |
| Seed takes the prize | **Dead.** The printer is not in the hat. |
| Download `xattestor.key` over P2P `:38443` | **Dead.** P2P is blocks and `xhb`. RPC `:38442` is localhost. |

That is **counterfeit tickets** and **homemade mint**. It is not
personhood and it is not a trustless census of every live blue-check
node on Earth.

## What consensus actually checks

Peers verify **consistency of the paper the seed published**:

1. `XSD1` is signed by the baked attestor pubkey.
2. Every id in `XHB1` has a matching `XVA1` stamp for a handle the seed
   signed.
3. Coinbase pays that draw and does not underpay the subsidy.

Peers do **not** call X. They do **not** prove “this list is every live
X Verified node.” Eligibility is the seed’s `GET /2/users/by/username`
lookup. Consensus checks the **seal**, not attendance in the parking
lot. That completeness gap is still there. Say that plainly.

## Threat model (release sabotage)

| Goal | What we guarantee | What we do not |
| --- | --- | --- |
| Impersonate a typed `@handle` on *this* node | `RequireHandle` / `RequireSession` on root claim, `linkxaccount`, `registeractivenode`. Typed `-xaccount=` is ignored. Send/receive are a normal wallet. | Steal `xsession.key` + `xsession.json` from this datadir → that attacker *is* the session on this node. |
| Publish login / OAuth secrets | Access tokens never written. Session files `0600`. P2P `xhb` sends @handle only (no X user id, no token). | A screenshot of RPC `getxsession`, or a copied datadir. |
| Spend someone else’s wallet | Spend only keys in **this** `wallet.dat`. | Copy `wallet.dat` (or the 12 words) → they have the keys. |
| Steal lottery rewards with a spoofed `xhb` | Gossip `xhb` must be compact-signed by the payout key. A **live** handle cannot be rebound. Seed must still stamp after a live X lookup. | First-seen race after restart if the handle is not pinned. |
| Invent lottery eligibility with a custom wallet | Seed X lookup + `XVA1` + `XSD1`. A modified binary cannot skip the stamp or mint. | A bot can still wear a **real** verified handle (name check, not personhood). |
| Replay Ravencoin blocks / addresses | Different genesis, magic `XFER` / `XFTN` / `XFRT`, ports, address versions (`X…`). | A malicious binary that changes those constants is a different coin. |
| Unauthenticated RPC spend | Cookie or `rpcuser`/`rpcpassword`. No auth header → 401. | Bind RPC on a public interface and leak the cookie. |
| Feed a lone node a fake chain | Height ≥ 1 main/test blocks without valid `XHB1`+`XVA1`+`XSD1` are rejected. | Checkpoints are empty. An eclipsed node follows the heaviest *valid* seed-signed chain its peers feed it. |

## What is tamper-resistant

- **Ledger rules:** height ≥ 1 coinbase must commit `XHB1`, carry
  `XVA1` and `XSD1` on main/test, pay the draw, and not underpay the
  subsidy. Height 0 genesis coinbase is unspendable. There is no PoW
  grind; `CheckProofOfWork` is a no-op. Lottery pools were removed.
- **Identity on this node:** root claim requires a session HMAC.
  Send and receive do not. A typed handle cannot pass `RequireHandle`.
- **Lottery membership:** unsigned / unverified gossip is dropped. The
  baked seed checks X and stamps (`XVA1`). User wallets Sign in with X.
  The seed has no session and is not in the hat. A custom app that only
  flips `verified=true` is not eligible.
- **Network isolation from Ravencoin:** magic, genesis, ports, versions.
- **RPC:** cookie or password. `sendrawtransaction` is a normal send.
- **What's new feed:** GUI may GET GitHub Releases. No wallet or token
  is sent. [UPDATES.md](UPDATES.md).

## What an attacker can still do

1. **Omit / incomplete hopper.** The seed publishes whoever it stamped
   this minute. A live peer the seed did not hear (or whose lookup
   failed) is absent from `XHB1`. Honest peers still accept the block.
2. **X API 401 / 429.** A bad or rate-limited bearer means **no new
   stamps**. After a seed restart the in-memory stamp table is empty.
   An empty hat that minute pays nobody. Keep the portal bearer string
   as copied (`%2B` / `%3D` left encoded). The seed HTTP client must
   dechunk X’s body (1.0.12 `094f159`); raw `74\\r\\n{` is not JSON.
3. **Several real blue checks.** One human with many verified accounts
   and many online boxes holds many legal tickets.
4. **Eclipse.** Attacker peers can stall you. They cannot make 1.0.12
   accept a block the seed did not sign. Connect to
   `addnode=172.191.195.221:38443`.
5. **Seed offline.** Only the baked seed produces. If that process is
   down, **liveness stops**. History already written stays. Do not give
   users a “I am a backup seed” checkbox.
6. **Stolen `xattestor.key`.** That file **is** the printer. P2P cannot
   fetch it. A copied datadir / PEM / screenshot can.
7. **1.0.11 leftovers.** Old wallets share genesis and can emit a side
   fork. 1.0.12 rejects those blocks. Do not run 1.0.11.
8. **Datadir theft.** `wallet.dat` / 12 words = coins. Session files =
   identity on that node.
9. **Host / binary / open `38442`.** Out of consensus scope.

Do not claim the chain is PoW-equivalent or unhackable.

## Operator checklist (launch night)

Stay private. Do **not** add public DNS seeds.

1. **One baked seed** (`172.191.195.221`). Open **TCP 38443** only. No
   Sign-in on the seed. Attestor key + X app bearer + listen.

   ```bash
   xcoind -listen=1 -port=38443 -server \
     -bind=0.0.0.0:38443 \
     -maxconnections=32 \
     -xlookupbearer="$XCOIN_X_BEARER" \
     -xattestorkey=/home/azureuser/.xcoin/xattestor.key
   ```

   Confirm `getlotteryinfo`: `baked_seed=true`, `x_lookup=true`,
   `local_eligible=false`. `stamped_handles` is the hat. Never put the
   bearer or the key in the user zip. Do not expose **38442**.

2. Everyone else runs the **1.0.12** zip. Packaged `xcoin.conf` already
   has `addnode=172.191.195.221:38443`.

3. **Sign in with X** on each **user** datadir. Leave the GUI open
   through the first payday (File → Exit drops the session).

4. After height 1, confirm the same tip as the seed
   (`getblockchaininfo`).

5. Seed `~/.xcoin` holds `xattestor.key` and the bearer line. Mode
   `600`. Treat it like the printer.

## Related

- Lottery / session: [LOTTERY.md](LOTTERY.md), [XSIGNIN.md](XSIGNIN.md)
- 1.0.12 notes: [RELEASE-1.0.12.md](RELEASE-1.0.12.md)
- Operator runbook: [LAUNCH.md](LAUNCH.md), [GO-LIVE.md](GO-LIVE.md)
- Whitepaper: [../whitepaper/XCOIN.md](../whitepaper/XCOIN.md)
