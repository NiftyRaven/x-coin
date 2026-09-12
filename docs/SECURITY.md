# X Coin release-day security

**Nifty Raven** (@NFTRVN on X)

Nothing here is “hacker proof.” This is a **small private / fair-launch
UTXO + lottery mesh**. Honest operators plus the checks below make
*cheap* sabotage fail. A well-resourced peer who can eclipse you, steal
your datadir, or out-vote a lone node can still lie to you.

Repo stays **private**. There are no public DNS seeds on purpose.

## Threat model (release sabotage)

Attacker goals we actually designed against:

| Goal | What we guarantee | What we do not |
| --- | --- | --- |
| Impersonate a typed `@handle` on *this* node | `RequireHandle` / `RequireSession` on root claim, `linkxaccount`, `registeractivenode`. Typed `-xaccount=` is ignored. Send/receive are a normal wallet (keys in this `wallet.dat`). | Steal `xsession.key` + `xsession.json` from this datadir → that attacker *is* the session on this node. |
| Publish login / OAuth secrets | Access tokens never written. Session files `0600`. GUI does not show user ids or secret paths. P2P `xhb` sends @handle only (no X user id, no token). | A screenshot of RPC `getxsession`, or a copied datadir. |
| Spend someone else’s wallet | Spend only keys in **this** `wallet.dat`. Session does not import another wallet. | Copy `wallet.dat` (or the 12 words) → they have the keys. |
| Steal lottery rewards with a spoofed `xhb` | Gossip `xhb` must be compact-signed by the payout key. A **live** handle cannot be rebound to another script. Allowlist match is **handle**, not “any handle + a listed userid.” Optional payout pin. | First-seen race after restart if the handle is not pinned. |
| Invent lottery eligibility with a custom wallet | The seed looks up the handle on X. No live blue check → not stamped. Main/test coinbase must carry `XVA1` stamps for every `XHB1` id. A modified binary cannot skip the stamp. | A bot can still wear a **real** verified handle (name check, not “this program is that person”). |
| Replay Ravencoin blocks / addresses | Different genesis, magic `XFER` / `XFTN` / `XFRT`, ports, address versions (`X…` / `y…`). | A malicious binary that changes those constants is a different coin. |
| Unauthenticated RPC spend | HTTP RPC requires cookie or `rpcuser`/`rpcpassword`. No auth header → 401. | Bind RPC on a public interface and leak the cookie / password. |
| Feed a lone node a fake chain | Consensus still rejects invalid lottery coinbases and wrong subsidy. | `fMiningRequiresPeers` is **false** (a lone eligible node must be able to produce). Checkpoints are empty. An eclipsed node will follow the heaviest *valid* chain its peers feed it. |

## What is tamper-resistant

- **Ledger rules:** height ≥ 1 coinbase must commit `XHB1`, pay the draw,
  and not underpay the subsidy. Height 0 genesis coinbase is unspendable.
  Invalid blocks are rejected. There is no PoW grind; `CheckProofOfWork`
  is a no-op by design (lottery, not hash). Lottery pools were removed.
- **Identity on this node:** root claim / own require a
  session HMAC (`xsession.json` + `xsession.key`). Send and receive do not.
  A typed handle cannot pass `RequireHandle`.
- **Lottery membership:** unverified or unsigned gossip is not added to the
  active set (zero chance). On main/test the seed also checks X for a
  live blue check and stamps the payout (`XVA1`). Official Sign in with
  X is still required on **user** wallets. The baked seed has no
  session. A custom app that only flips `verified=true` is not
  eligible. Replacing an online handle’s payout script is rejected.
- **Network isolation from Ravencoin:** magic, genesis, ports, versions.
- **RPC:** cookie or password. `sendrawtransaction` is a normal wallet send (no session).
- **What's new feed:** the GUI may GET GitHub Releases (or `-xreleaseurl=`).
  No wallet, session, or token is sent. 404 while the repo is private is
  expected. `-nocheckupdates` turns the GET off. [UPDATES.md](UPDATES.md).

## What an attacker can still do on a small private mesh

1. **Eclipse / fake peers.** Connect only to attacker nodes. They can
   stall you or feed another *valid* lottery chain. There is no assumevalid
   checkpoint. **Connect to a known seed** (`addnode=` / `seednode=`).
2. **Majority of eligible producers.** Whoever the committed active set
   selects can mint. Unverified accounts have zero chance. A modified
   client can still assert `xVerified` on gossip (compact-signed by the
   payout key); honest wallets only set that bit from `users/me`.
3. **First-seen handle grab** (no payout pin). After a restart, the first
   signed `xhb` for `alice` wins until TTL. Pin the payout in the allowlist
   (`handle [userid] Xaddress…`) if you need to close that window.
4. **Datadir theft.** `wallet.dat` / 12 words = coins. `xsession.key` +
   `xsession.json` = identity on that node (not spend of another machine’s
   wallet).
5. **Mempool / inv spam.** Inherited Bitcoin-family limits (`MAX_INV_SZ`,
   min relay fee, banscore). A loud peer can still waste bandwidth; ban it
   (`setban`) and lower `-maxconnections` on a seed.
6. **Host / binary / RPC exposure.** Compromise of the operator box, a
   backdoored build, or an open `38442` is out of consensus scope.

Do not claim the chain is military-grade or unhackable.

## Operator checklist (release day)

Stay private. Do **not** add public DNS seeds.

1. **One trusted seed** you control. Open **TCP 38443** only. The seed
   does **not** Sign in (no `xsession`). It needs the attestor key, an
   X API bearer for lookup, and listen:

   ```bash
   src/xcoind -listen=1 -port=38443 -server \
     -bind=0.0.0.0:38443 \
     -maxconnections=32 \
     -xlookupbearer="$XCOIN_X_BEARER" \
     -xattestorkey=/root/.xcoin/xattestor.key
   ```

   Or set `XCOIN_X_BEARER` and keep `~/.xcoin/xattestor.key` (must
   match the baked pubkey). Do not put the bearer or the key in the
   user zip. RPC stays local (default). Use the cookie or a strong
   `-rpcpassword`. Do not expose **38442**.

2. **Every other node** puts a **trusted peer IP in the config file**
   (`xcoin.conf`) — not a stranger, not a BIP39 seed, not a required
   GUI field:

   ```
   addnode=<trusted-peer-ip>:38443
   ```

   Optional: `-connect=<trusted-peer-ip>:38443` (only that peer; harder
   to eclipse *if* the seed is honest; you also cannot see a second
   honest view). The wallet GUI never lists other people's IPs. Sharing
   *your* listen address is opt-in on Home (**Provide my node IP**; off
   by default).

3. **Optional invite / payout-pin file** (not a lottery gate). Confirm
   each handle offline if you use pins. Prefer `handle userid` and, for
   the seed operators you care about, pin the payout:

   ```
   NFTRVN 123456789 XyourPinnedAddressxxxxxxxxxxxxxxxxx
   ```

4. **Sign in with X** on each **user** datadir (or `-regtest` mock).
   Do not Sign in on the baked seed. Do not type someone else’s handle
   into `linkxaccount` / `-xaccount`.

5. **Confirm you are on the same tip** as the seed (`getblockchaininfo`
   best hash / height). If you only have attacker peers, you will agree
   with *them*.

6. **Ban obvious junk.** `-banscore`, `-bantime`, `setban`. Lower
   `-maxconnections` on the seed so a connection flood cannot occupy every
   slot.

7. **Datadir permissions.** `~/.xcoin` is the wallet, session, and
   payout key (`lottery-payout.key`). Treat it like a hot wallet.

## Related

- Private-test facts: [AUDIT.md](AUDIT.md)
- Operator runbook: [LAUNCH.md](LAUNCH.md)
- Lottery / session: [LOTTERY.md](LOTTERY.md), [XSIGNIN.md](XSIGNIN.md), [WALLET.md](WALLET.md)
