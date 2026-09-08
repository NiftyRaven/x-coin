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
| Impersonate a typed `@handle` on *this* node | `RequireHandle` / `RequireSession` on send, receive, own, `linkxaccount`, `registeractivenode`. Typed `-xaccount=` is ignored. | Steal `xsession.key` + `xsession.json` from this datadir → that attacker *is* the session on this node. |
| Spend someone else’s wallet | Spend only keys in **this** `wallet.dat`. Session does not import another wallet. | Copy `wallet.dat` (or the 12 words) → they have the keys. |
| Steal lottery rewards with a spoofed `xhb` | Gossip `xhb` must be compact-signed by the payout key. A **live** handle cannot be rebound to another script. Allowlist match is **handle**, not “any handle + a listed userid.” Optional payout pin. | First-seen race after restart if the handle is not pinned. A producer can still commit a set; consensus checks the coinbase matches that commitment, not “the true mesh.” |
| Replay Ravencoin blocks / addresses | Different genesis, magic `XFER` / `XFTN` / `XFRT`, ports, address versions (`X…` / `y…`). | A malicious binary that changes those constants is a different coin. |
| Unauthenticated RPC spend | HTTP RPC requires cookie or `rpcuser`/`rpcpassword`. No auth header → 401. | Bind RPC on a public interface and leak the cookie / password. |
| Feed a lone node a fake chain | Consensus still rejects invalid lottery coinbases and wrong subsidy. | `fMiningRequiresPeers` is **false** (a lone eligible node must be able to produce). Checkpoints are empty. An eclipsed node will follow the heaviest *valid* chain its peers feed it. |

## What is tamper-resistant

- **Ledger rules:** height ≥ 1 coinbase must commit `XHB1`, pay the draw,
  and not underpay the subsidy. Height 0 genesis coinbase is unspendable.
  Invalid blocks are rejected. There is no PoW grind; `CheckProofOfWork`
  is a no-op by design (lottery, not hash).
- **Identity on this node:** send / receive / own / claim require a
  session HMAC (`xsession.json` + `xsession.key`). A typed handle cannot
  pass `RequireHandle`.
- **Lottery membership:** unlisted or unsigned gossip is not added to the
  active set and is not relayed. Replacing an online handle’s payout
  script is rejected.
- **Network isolation from Ravencoin:** magic, genesis, ports, versions.
- **RPC:** cookie or password; `sendrawtransaction` also requires a session.

## What an attacker can still do on a small private mesh

1. **Eclipse / fake peers.** Connect only to attacker nodes. They can
   stall you or feed another *valid* lottery chain. There is no assumevalid
   checkpoint. **Connect to a known seed** (`addnode=` / `seednode=`).
2. **Majority of eligible producers.** Whoever the committed active set
   selects can mint. If the allowlist is empty or shared with an attacker,
   they can heartbeat (with their own signed payout) and win.
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

1. **One trusted seed** you control. Open **TCP 38443** only. Run:

   ```bash
   src/xcoind -listen=1 -port=38443 -server \
     -bind=0.0.0.0:38443 \
     -maxconnections=32 \
     -xallowlist=/shared/verified-x-accounts.txt
   ```

   RPC stays local (default). Use the cookie or a strong `-rpcpassword`.
   Do not expose **38442**.

2. **Every other node** joins that seed — not a stranger:

   ```
   addnode=<seed-ip>:38443
   ```

   Optional: `-connect=<seed-ip>:38443` (only that peer; harder to eclipse
   *if* the seed is honest; you also cannot see a second honest view).

3. **Same allowlist on every honest node.** Confirm each handle offline
   (verified / Premium badge). Prefer `handle userid` and, for the seed
   operators you care about, pin the payout:

   ```
   NFTRVN 123456789 XyourPinnedAddressxxxxxxxxxxxxxxxxx
   ```

4. **Sign in with X** on each datadir (or `-regtest` mock). Do not type
   someone else’s handle into `linkxaccount` / `-xaccount`.

5. **Confirm you are on the same tip** as the seed (`getblockchaininfo`
   best hash / height). If you only have attacker peers, you will agree
   with *them*.

6. **Ban obvious junk.** `-banscore`, `-bantime`, `setban`. Lower
   `-maxconnections` on the seed so a connection flood cannot occupy every
   slot.

7. **Datadir permissions.** `~/.xcoin` is the wallet, session, and payout
   key (`lottery-payout.key`). Treat it like a hot wallet.

## Related

- Private-test facts: [AUDIT.md](AUDIT.md)
- Operator runbook: [LAUNCH.md](LAUNCH.md)
- Lottery rules: [LOTTERY.md](LOTTERY.md)
- Session / seed: [XSIGNIN.md](XSIGNIN.md), [WALLET.md](WALLET.md)
