# Wallet keys vs Sign in with X

**Nifty Raven** (@NFTRVN on X)

Private test only.

X Coin kept Ravencoin’s **12-word BIP39 / BIP44 mnemonic wallet**.
Sign in with X did **not** replace it.

## Direct answer

| Question | Answer |
| --- | --- |
| Did 12-word generation change? | **No.** First-run still creates or restores a BIP39 12-word seed (`CMnemonic::Generate(128)`, `-bip44=1` default). The HD Wallet Setup modal is still there. |
| Do users still need the 12 words? | **Yes.** Create a new wallet or restore an existing one with those words. CLI: `-mnemonic=`, `getmywords`, `dumpwallet`. |
| Does Sign in with X replace the seed? | **No.** Seed = keys. X session = identity proof on this node. |

The only HD path number that differs from the imported network is
**BIP44 coin type 3844** on mainnet (imported coin used 175). That
changes derived addresses for a given seed. It does not change how
the 12 words are generated or that you still need them.

## Two layers (both visible on Home)

1. **12-word seed** — controls private keys in `wallet.dat`. Write them
   down. Anyone with the words (and optional extension word) can restore
   the keys.
2. **Sign in with X** — OAuth 2.0 PKCE → `GET /2/users/me` → HMAC proof
   in this datadir (`xsession.json` + `xsession.key`). Send, receive,
   and own require that proof. A typed handle cannot steal it.

Authentication is the key to **asset ownership on this node** — it
proves it is you. It is not a claim that the keys are “encrypted to X”.
The seed still controls the keys.

Lottery eligibility is session **plus** the X-Verified allowlist.
Every signed-in user can send and receive.

See [XSIGNIN.md](XSIGNIN.md) for the OAuth loopback. Home / Receive /
Send show the linked `@handle` and X user id when the proof is present.
