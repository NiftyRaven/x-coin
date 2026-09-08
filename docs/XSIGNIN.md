# Sign in with X

**Nifty Raven** (@NFTRVN on X)

Private test only. Do not publish this chain.

Signing in is what stops impersonation and **proves asset ownership**.
A typed handle is not an identity. The wallet uses OAuth 2.0 PKCE, then
`GET /2/users/me`, and binds **that** user id + username to this node's
datadir. Send and receive require that session. Only **X Verified**
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

`getnewaddress`, `getaccountaddress`, `sendtoaddress`, `sendrawtransaction`,
`issue` / `transfer` / `reissue`, `linkxaccount`, `registeractivenode`, and
lottery identity require a valid proof. **Authentication is the key
to asset ownership — it proves it is you.** Lottery also requires
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

## X developer portal (loopback)

1. Open [developer.x.com](https://developer.x.com/) and create a
   project + app (User authentication).
2. App type: **Native App** / public client (PKCE, no secret required).
   A confidential client may set `-xoauthclientsecret=` if the portal
   issues one.
3. Callback URI, exactly:

   ```
   http://127.0.0.1:18791/callback
   ```

   Another port: `-xoauthcallbackport=N` and register
   `http://127.0.0.1:N/callback`.
4. Scopes: `users.read` `tweet.read` only. Do **not** request `offline.access`
   (no refresh token). The access token is held in RAM for the `users/me`
   call and then discarded — it is never written to disk.
5. Copy the **Client ID**. This is an **operator bake**, not a user paste field.
   Put it in the shipped `xcoin.conf` (or `-xoauthclientid=`) before people
   run Sign in with X:

   ```
   xoauthclientid=YOUR_CLIENT_ID
   ```

   Callback, exactly: `http://127.0.0.1:18791/callback`.
   Never hardcode a fake client. Empty Client ID → Sign in with X tells
   you the operator has not baked one; it does not fake success. A typed
   handle cannot claim verified status.

6. Click **Sign in with X**. The browser opens
   `https://twitter.com/i/oauth2/authorize`. After you approve, X
   redirects to the loopback callback; the wallet exchanges the code,
   then calls `GET https://api.twitter.com/2/users/me`.

## Happy path (GUI, no terminal)

1. Create / open wallet (first-run **12 secret words** — still required).
2. **Sign in with X** (browser redirect; binds this node to your X account; does not replace the seed).
3. Home shows **this wallet linked to @handle**. There is no Client ID paste field.
4. **Allowlist my handle** (optional operator invite list — not a blue check)
   then **Claim my root asset**. An empty wallet can claim that one free root
   (identity asset, 0 XFER). Unsigned-in wallets cannot claim a root
   and cannot `issue` a main. Subs/uniques are issued under that root.
5. **Receive** (address + Copy) / **Send** (paste address, amount, Send).
6. **Activity** (history). Issue sub / unique from Home. Lottery status is on Home.
7. Optional **POOL** — create or join (id + password). Public card is name +
   addresses only. Copy pool id does not copy the password.
8. Home shows **this wallet is yours / linked to @handle**. Transfer assets is a Home button (and the left tab).
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
# private test without live OAuth: NFTRVN defaults to verified=true
xcoind -regtest -xoauthmock=NFTRVN
```

The GUI **Simulate sign-in (regtest)** button is hidden on mainnet.
