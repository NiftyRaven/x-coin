# In-wallet What's new

**Nifty Raven** (@NFTRVN on X)

Private only.

Users are told about new wallets **inside** `xcoin-qt`. They do not have
to open GitHub to read the notes.

## What the user sees

1. **This version.** Home shows a What's new card until you tap it (or
   OK). **Help → What's new** always opens the notes shipped in this
   binary (`docs/RELEASE-1.0.15.md` on Heavy / `xrelease::BundledNotes`).
   Light 1.0.14 ships its own notes (`docs/RELEASE-1.0.14.md`).
2. **A newer GitHub Release.** After the repo is public (or when
   `-xreleaseurl=` points at a reachable JSON feed), the wallet GETs
   the feed once at start. Home and the header show **New: vX.Y.Z —
   What's new**. The dialog is the GitHub release body. **Open download
   page** is optional. **Later** hides the banner for that tag.
3. **Help → Check for updates** re-fetches. Failure is not a sign-out
   and does not block the wallet.

No modal on top of the first-run 12-word setup.

## What is sent

The GET is the feed URL only (`User-Agent: XCoin-Wallet/1.0.15` on
Heavy; Light 1.0.14 sends `XCoin-Wallet/1.0.14`).
No `wallet.dat`, no seed, no `xsession`, no X tokens, no RPC cookie.

Default feed:
`https://api.github.com/repos/NiftyRaven/x-coin/releases?per_page=15`

While this repository is **private**, unauthenticated GitHub API
returns 404. The wallet stays quiet and still has What's new for the
installed version. After go-live (repo public), **Light does not treat
Heavy as an upgrade** (and Heavy does not treat Light as one). They are
parallel wallets. A Light zip whose notes say Light stays on the Light
line; a Heavy zip stays on Heavy.

Override: `-xreleaseurl=https://…` (GitHub array, `{releases:[…]}`,
or one `{tag, notes, url}` object). Off: `-nocheckupdates`.

RPC (no network): `getreleasenotes` and `getreleasenotes '<json>'`.

## Session (not an update TTL)

Sign-in lasts until a **clean GUI quit** deletes `xsession.json`.
`exp` is HMAC input only. See [XSIGNIN.md](XSIGNIN.md).
