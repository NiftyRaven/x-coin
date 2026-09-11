# Sign in with X

**Nifty Raven** (@NFTRVN on X)

Private only. Do not publish this chain.

Signing in is what stops impersonation and **proves asset ownership**.
A typed handle is not an identity. The wallet uses OAuth 2.0 PKCE, then
`GET /2/users/me`, and binds **that** user id + username to this node's
datadir. Send and receive do **not** require that session (normal wallet).
Claiming the free root does. Only **X Verified**
signed-in handles enter the lottery.

**X Verified** is X’s blue check / X Premium (and org checks per that
policy) — a user X itself marks verified. Official meaning:
[About the blue check](https://help.x.com/en/managing-your-account/about-x-bluecheck)
per [X’s verification policy](https://help.x.com/en/rules-and-policies/verification-policy).
It is **not** “whoever the operator typed into `addxverified`.” That
command maintains an optional private-mesh **invite list**.

**This wallet is yours** when Home shows **Linked to @yourhandle**.
That state means this datadir holds a valid session proof. A typed handle
cannot steal it. Home does **not** show your X user id, tokens, or
session file paths.

The **12-word BIP39 seed is unchanged** and still required to create
or restore keys. Sign in with X does not replace the seed. Details:
[WALLET.md](WALLET.md).

## What the node stores

After a successful sign-in:

| File | Contents |
| --- | --- |
| `xsession.key` | 32-byte HMAC secret (created once per datadir, mode `0600`) |
| `xsession.json` | `id`, `username`, `exp`, `verified`, `verified_type`, HMAC-SHA256 proof (mode `0600`) |

**Not stored:** X access tokens, refresh tokens, passwords, PKCE verifiers.

Proof message: `xcoin-xsession|{id}|{username}|{exp}|{0|1}|{verified_type}`.

OAuth calls `GET /2/users/me?user.fields=verified,verified_type`.
`verified==true` (or `verified_type` of `blue` / `business` / `government`)
is X Verified.

## Session lifetime

A valid `xsession.json` HMAC proof is signed-in until the file is
deleted. `exp` is HMAC input only — `LoadSession` does **not** reject
on wall-clock. Home reads the local proof directly (not `getxsession`
RPC), so a warmup or RPC miss cannot look like a sign-out. Lottery
does not drop mid-run because `exp` elapsed.

Live GUI Sign-in writes `exp=0` (no wall-clock kick). 1.0.10 wrote a
far-future `exp` but left the `GetTime() > exp` check in place; a
leftover 2-hour file or a clock/`-mocktime` jump still signed the
user out while the process was up.

On a **clean GUI quit** (`xcoin-qt` File → Exit, a window close that
actually quits, or RPC `stop` against the GUI), the wallet deletes
`xsession.json` (the HMAC proof). The next GUI launch is not signed in
and must Sign in with X again. `xsession.key` stays in the datadir.

**Headless `xcoind` keeps the file.** There is no Qt quit hook. Seed /
operator “sign in once” still works across daemon restarts. The claimed
identity root (`getmainasset` / `linkxaccount`) is on-chain and must
still be assigned after that restart; do not re-link unless
`getmainasset` reports unassigned.

A crash, kill, or power loss **without** a clean GUI quit may leave
`xsession.json` on disk. The next launch can still be signed in. This
is not crash-proofed: no keepalive thread, no X token refresh, no
second session framework.

OAuth still has a local ~1 hour cooldown on deliberate Sign-in /
Re-link clicks (success, cancel, or fail). That is not a session TTL.

Regtest mock (`-xoauthmock` / `mockxsignin`) still writes a 365-day
`exp`. That value is not a kick.

## Privacy (login is not public)

- Tokens live in RAM for one `users/me` round-trip, then are wiped.
- Session files are this datadir only (`0600`). They are not gossiped.
- P2P lottery heartbeats carry the public @handle (lottery identity),
  never the numeric X user id, never a token, never the HMAC proof.
- The GUI shows @handle and verified yes/no. It does not show user ids
  or secret paths. Peer IPs stay hidden unless you check **Provide my
  node IP** (off by default).
- `getxsession` is localhost RPC. Do not publish that JSON.

Steal this datadir (`xsession.key` + `xsession.json`) and you steal
**this node's session**, not someone else's `wallet.dat`. The HMAC is
per-datadir; it cannot spend keys that are not in this wallet.

`issue` / `reissue` / `linkxaccount`, `registeractivenode`, and
lottery identity require a valid proof. **Authentication is the key
to asset ownership — it proves it is you.** Send and receive
(`getnewaddress`, `sendtoaddress`, `sendrawtransaction`, `transfer`)
do not. Lottery also requires
**X Verified** from that `users/me` response. A signed-in handle that is
not X Verified can still send and receive, but has **zero lottery chance**.
An X Verified session plus a running wallet **cannot be excluded** from
the draw. The operator invite list (`addxverified` / `-xverified` /
`-xallowlist`) is optional pins / invites, not a lottery gate.
`linkxaccount otherperson` is rejected if the
session is not `otherperson`. Typed `-xaccount=` is ignored without a
matching session. RPC itself still needs the cookie or `rpcpassword`
(no unauthenticated spend).

The node never pretends login succeeded without a real access token
(or, on `-regtest` only, a mock `users/me` payload).

## Operator only (developer portal)

Users do **not** open developer.x.com and do **not** paste a Client
ID. Packaged 1.0.11 `xcoin.conf` already has `xoauthclientid=` set.
Empty Client ID → Sign in with X says the operator has not baked one;
it does not fake success. There is no GUI paste box.

If an operator must register a new loopback app: Native App / public
client (PKCE), scopes `users.read` `tweet.read` only (no
`offline.access`), callback exactly `http://127.0.0.1:18791/callback`.
Bake the id into package `xcoin.conf` (`xoauthclientid=`). Do not
invent a different id on launch night.

## Happy path (GUI, no terminal)

1. Create / open wallet (first-run **12 secret words** — still required).
2. **Sign in with X** (browser redirect; binds this node to your X account; does not replace the seed). The session lasts until a clean GUI quit deletes `xsession.json` (`exp` is not a kick). Home does **not** call `users/me` again while Linked. **Re-link** asks first and shares a local ~1 hour cooldown with failed or cancelled attempts.
3. Home shows **this wallet linked to @handle**. There is no Client ID paste field.
4. **Allowlist my handle** (optional operator invite list — not a blue check)
   then **Claim my root asset**. An empty wallet can claim that one free root
   (identity asset, 0 XFER). Unsigned-in wallets cannot claim a root
   and cannot `issue` a main. Subs/uniques are issued under that root.
5. **Receive** (address + Copy) / **Send** (paste address, amount, Send).
6. **Activity** (history). Lottery status is on Home.
7. Left-nav **Assets** (or Home **Issue sub** / **Issue unique**) opens
   the Create Asset form: sub (quantity, units, IPFS, reissuable) or
   unique (qty 1, same IPFS). Main/root is Claim only — typed MAIN names
   fail. Transfer assets is a Home button (Advanced).
8. Home shows **this wallet is yours / linked to @handle**.
   **Provide my node IP** is off by default.

This wallet + this X session = you. Another user cannot send from inside
your wallet. RPC console is under **Advanced** (not required for the
happy path).

## Regtest (no live X login)

This VM / CI cannot complete a live X OAuth round-trip. Tests inject a
mock `users/me` **only** on `-regtest`:

```bash
xcoind -regtest -xoauthmock=alice:verified
# or:
xcoin-cli -regtest mockxsignin '{"data":{"id":"99","username":"alice","verified":true,"verified_type":"blue"}}'
# unsigned-in (send/receive only, no lottery):
xcoin-cli -regtest mockxsignin ghost:unverified
# regtest without live OAuth: NFTRVN defaults to verified=true
xcoind -regtest -xoauthmock=NFTRVN
```

The GUI **Simulate sign-in (regtest)** button is hidden on mainnet.
