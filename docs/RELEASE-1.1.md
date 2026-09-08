# X Coin 1.1 (private)

**Nifty Raven** (@NFTRVN on X)  
Anonymous public identity — display name and handle only.

Private package **1.1.0** (`contrib/xcoin/package-linux.sh` →
`dist/xcoin-1.1.0-linux-x86_64.tar.gz`). This is **not** a public GitHub
Release. Do not make the repository public. 1.0 notes:
[RELEASE-1.0.md](RELEASE-1.0.md).

## What 1.1 adds on top of 1.0

- **Lottery pools from Home** — Create, join, leave, copy pool id.
  Tickets = one per **X Verified** running member. A win splits evenly
  across every member address (verified or not). Public view is pool
  **name + addresses** only. Pool id + password are yours to share.
- **X Verified = X’s blue check** from `GET /2/users/me`, not the
  operator invite list (`addxverified`). Unverified = zero lottery
  chance. Invite list cannot exclude a verified running wallet.
- **Private Sign in with X** — access tokens never written. GUI shows
  @handle only. P2P `xhb` sends user id **0**.
- **Provide my node IP** on Home, **off by default**. Uncheck and the
  IP disappears. Other people’s IPs are never listed.
- **GUI polish** — short RPC errors, async Sign in with X callback,
  lottery card on Home, Activity calls a lottery win a lottery win.
- **12-word BIP39 create/restore is unchanged.** Sign in with X does
  not replace the seed ([WALLET.md](WALLET.md)).

Still true from 1.0: desktop GUI `xcoin-qt`, same `wallet.dat` as CLI,
one free identity root per handle, no mining, fair launch, no public
DNS seeds, no explorer.

## Install the GUI (Ubuntu)

```bash
sudo apt-get install -y build-essential libtool autotools-dev automake pkg-config \
    bsdmainutils python3 libevent-dev libboost-all-dev libssl-dev libdb++-dev \
    qtbase5-dev qttools5-dev qttools5-dev-tools libqt5svg5-dev \
    libqrencode-dev protobuf-compiler libprotobuf-dev

./autogen.sh
./configure --with-gui=qt5 --disable-bench --disable-tests --with-incompatible-bdb
make -j$(nproc)
src/qt/xcoin-qt                 # main
src/qt/xcoin-qt -regtest        # local practice
```

Pack: `contrib/xcoin/package-linux.sh` → `dist/xcoin-1.1.0-linux-x86_64.tar.gz`.

## Home (honest)

- Linked wallet shows **@handle** and X Verified yes/no — not your X
  user id, not tokens, not session paths.
- **POOL** — public name, pool id, password (masked). After create, Your
  pool shows name, id (to share), tickets, even-split wording, and
  member addresses. Public pools card is name + addresses only.
- **Provide my node IP** — off until you check it.
- Receive / Send / Activity / Transfer assets. File menu or
  `xcoin-cli stop` to quit. Do not `pkill -f xcoin-qt`.

## Known limits

- Sign in with X is required to send, receive, and own assets. Lottery
  is **X Verified** (blue check) plus a running wallet.
- Live X OAuth needs a developer Client ID (`xoauthclientid=`).
  `-regtest` mock `users/me` covers tests (`-xoauthmock=NFTRVN:verified`).
- Honest nodes emit `XPL1` and split when the winner is in a known pool.
  A cheating producer can omit `XPL1`. Keep pool secrets private.
- No mobile app. No public DNS seeds. No exchange listing.
  [DEX.md](DEX.md). Not hacker-proof: [SECURITY.md](SECURITY.md).
- Restricted assets stay removed. Internal C++ names stay:
  [FORK.md](FORK.md).

## Tests

```bash
contrib/xcoin/smoke-pool.sh          # 3-member even split; unverified has 0 tickets
contrib/xcoin/smoke-xsession.sh      # typed handle rejected; session accepted
contrib/xcoin/smoke-regtest.sh
contrib/xcoin/smoke-gossip.sh
contrib/xcoin/smoke-eligibility.sh
contrib/xcoin/smoke-gui.sh           # headless Qt (sets XDG_RUNTIME_DIR)
contrib/xcoin/smoke-benchmark.sh     # lone ledger + 3-node visible send
contrib/xcoin/smoke-isolation.sh     # Bob cannot spend Alice's wallet.dat
contrib/xcoin/smoke-sabotage.sh      # unsigned sendraw / handle steal rejected
```

Paper: [whitepaper/XCOIN.md](../whitepaper/XCOIN.md).
Audit: [AUDIT.md](AUDIT.md). Lottery: [LOTTERY.md](LOTTERY.md).
