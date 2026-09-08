# Sign in with X

**Nifty Raven** (@NFTRVN on X)

Private test only. Do not publish this chain.

Signing in is what stops impersonation. A typed handle is not an
identity. The wallet uses OAuth 2.0 PKCE, then `GET /2/users/me`, and
binds **that** user id + username to this node's datadir.

## What the node stores

After a successful sign-in:

| File | Contents |
| --- | --- |
| `xsession.key` | 32-byte HMAC secret (created once per datadir) |
| `xsession.json` | `id`, `username`, `exp`, HMAC-SHA256 proof |

Proof message: `xcoin-xsession|{id}|{username}|{exp}`.

`linkxaccount`, `registeractivenode`, and lottery eligibility require a
valid proof. `linkxaccount otherperson` is rejected if the session is
not `otherperson`. Typed `-xaccount=` is ignored without a matching
session.

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
4. Scopes: `users.read` `tweet.read` `offline.access`.
5. Copy the **Client ID**. In `xcoin-qt` paste it on Home and press
   **Save Client ID**, or put this in `xcoin.conf`:

   ```
   xoauthclientid=YOUR_CLIENT_ID
   ```

   Never hardcode a fake client. Empty Client ID → the Sign in button
   tells you to set it; it does not fake success.

6. Click **Sign in with X**. The browser opens
   `https://twitter.com/i/oauth2/authorize`. After you approve, X
   redirects to the loopback callback; the wallet exchanges the code,
   then calls `GET https://api.twitter.com/2/users/me`.

## Happy path (GUI, no terminal)

1. Create / open wallet (first-run mnemonic).
2. **Sign in with X**.
3. **Allowlist my handle** (operator) then **Claim my root asset**.
4. **Receive** (address + Copy) / **Send** (paste address, amount, Send).
5. Issue sub / unique from Home. Lottery status is on Home.

RPC console is under **Advanced**.

## Regtest (no live X login)

This VM / CI cannot complete a live X OAuth round-trip. Tests inject a
mock `users/me` **only** on `-regtest`:

```bash
xcoind -regtest -xoauthmock=alice
# or:
xcoin-cli -regtest mockxsignin '{"data":{"id":"99","username":"alice"}}'
```

The GUI **Simulate sign-in (regtest)** button is hidden on mainnet.
