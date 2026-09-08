# X Coin (XFER)

**Nifty Raven** (@NFTRVN on X)  
Anonymous public identity — display name and handle only.

Peer-to-peer coin for users on X. No mining. Minute lottery among **X
Verified** (blue check) active nodes; halvings add winners. Optional
pools: tickets from verified members, even split to everyone in the
pool. Fair launch, no premine. ~21 billion XFER. One free root identity
asset per signed-in X handle.

Whitepaper: [whitepaper/XCOIN.md](whitepaper/XCOIN.md).  
Release 1.1 (private): [docs/RELEASE-1.1.md](docs/RELEASE-1.1.md).  
Private-test audit: [docs/AUDIT.md](docs/AUDIT.md).  
Release-day threat model (not hacker-proof): [docs/SECURITY.md](docs/SECURITY.md).  
DEX listing criteria (private; do not apply): [docs/DEX.md](docs/DEX.md).

## How to use the wallet

**Release 1.1 ships a desktop GUI:** `xcoin-qt`. It talks to the same
node and `wallet.dat` as the CLI. **Sign in with X** is required to
claim a free root and to send or receive. Lottery additionally needs
**X Verified** (blue check). Optional **POOL** on Home.

Default datadir `~/.xcoin`, config `xcoin.conf`, P2P **38443**, RPC **38442**.
Mainnet addresses start with **X**.

### 1. Install dependencies (Ubuntu)

GUI (Qt 5) plus the node:

```bash
sudo apt-get update
sudo apt-get install -y build-essential libtool autotools-dev automake pkg-config \
    bsdmainutils python3 libevent-dev libboost-all-dev libssl-dev libdb++-dev \
    qtbase5-dev qttools5-dev qttools5-dev-tools libqt5svg5-dev \
    libqrencode-dev protobuf-compiler libprotobuf-dev
```

CLI-only (no wallet window): omit the `qt*` / `libqrencode` / `protobuf` lines.

### 2. Build

**GUI (release 1.1):**

```bash
./autogen.sh
./configure --with-gui=qt5 --disable-bench --disable-tests --with-incompatible-bdb
make -j$(nproc)
```

You now have `src/qt/xcoin-qt`, `src/xcoind`, and `src/xcoin-cli`.

**CLI-only:**

```bash
./autogen.sh
./configure --without-gui --disable-bench --disable-tests --with-incompatible-bdb
make -j$(nproc)
```

### 3. Start the GUI

First start creates `~/.xcoin/wallet.dat`.

```bash
mkdir -p ~/.xcoin
cat > ~/.xcoin/xcoin.conf <<'EOF'
server=1
rpcuser=xcoin
rpcpassword=change-me
EOF

src/qt/xcoin-qt
```

Local-only practice (regtest):

```bash
src/qt/xcoin-qt -regtest
```

The Home screen shows **this wallet is yours / linked to @handle** after
Sign in with X, plus lottery status, optional **POOL**, and your root
asset. Receive / Send / Activity / Transfer assets are buttons on Home
(and the left tabs).
**Activity** is this node’s wallet history (`listtransactions`).
**This wallet + this X session = you.** Spend only keys in this
`wallet.dat`. Another signed-in identity cannot send from inside your
wallet; signing in as someone else does not import their coins.
Peers on this private mesh see the same mempool and blocks. There is no
public explorer and nothing appears on Ravencoin explorers — XFER lives
on **this** ledger only (own genesis, not an imported snapshot). A
single node already stores that ledger; other people seeing the same
tip need a **trusted peer IP in the config file** (`addnode=` on port
**38443**). That is operator config, not a BIP39 seed. Home / Receive /
Send never list other people’s IPs and never show yours unless you opt
in: Home → **My node** → **Provide my node IP** (off by default). That
only then shows or copies this machine’s listen endpoint, or lets you
type the address you want to give out. Uncheck and the IP disappears
again. Help → About credits **Nifty Raven (@NFTRVN on X)** only.

First run still asks for a **12-word BIP39 seed** (create or restore).
That did not change. Sign in with X is a separate identity proof on this
node. See [docs/WALLET.md](docs/WALLET.md).

### Sign in with X (stops impersonation)

A typed handle is not an identity. Anyone could type `@someoneelse` if
that name were merely on an allowlist.

The wallet runs OAuth 2.0 PKCE, then **GET /2/users/me**. Only that
username and X user id are stored **locally** (`xsession.json` + `xsession.key`,
mode 0600). Access tokens are never saved. The GUI shows @handle, not
your user id or secrets. Peers see lottery @handles only — not login.
**Authentication is the key to asset ownership** — it proves it is you.
Send, receive, and claiming a root require that proof. Only **X Verified**
signed-in handles are lottery-eligible. **X Verified** is X’s blue check / X Premium
(and business / government org checks) from `GET /2/users/me`
([about the blue check](https://help.x.com/en/managing-your-account/about-x-bluecheck)
per [X’s verification policy](https://help.x.com/en/rules-and-policies/verification-policy)).
It is **not** whoever an operator typed into `addxverified`. That RPC
is an optional private-mesh **invite list**. `linkxaccount otherperson` is rejected
when the session is someone else.

Operator setup: paste your X app Client ID (saved as `xoauthclientid=`
in `xcoin.conf`). Callback:
`http://127.0.0.1:18791/callback`. Full steps: [docs/XSIGNIN.md](docs/XSIGNIN.md).

Regtest only: `-xoauthmock=handle` or RPC `mockxsignin` injects a fake
`users/me` payload so tests can prove typed names are rejected.
`-xoauthmock=NFTRVN` (or `NFTRVN:verified`) mocks X Verified; `ghost:unverified`
is signed-in but not lottery-eligible.

### 4. CLI (same wallet)

```bash
src/xcoind -daemon -server
src/xcoin-cli getwalletinfo
src/xcoin-cli getnewaddress
src/xcoin-cli sendtoaddress <their-X-address> 1.5
src/xcoin-cli addxverified NFTRVN
src/xcoin-cli mockxsignin NFTRVN          # regtest only
src/xcoin-cli linkxaccount                # uses the signed-in session
src/xcoin-cli getmainasset NFTRVN
src/xcoin-cli issue NFTRVN/NOTE 1
src/xcoin-cli getlotteryinfo
src/xcoin-cli createpool "Night Crew" crew1 s3cret   # or use Home → POOL
src/xcoin-cli listpools                               # name + addresses only
src/xcoin-cli getmypool                               # id if you created/joined; never the password
src/xcoin-cli stop
```

A 26-character X handle maps 1:1 to a 26-character root (max root name:
**32**). See [docs/ASSETS.md](docs/ASSETS.md).

You must Sign in with X to send or receive. Lottery additionally requires
**X Verified** (`users/me.verified` — blue check) and a running wallet.
Unverified accounts have **zero chance**. The operator invite list cannot
exclude a verified running wallet; it is optional pins / invites, not
the meaning of Verified.

Stop the GUI from the File menu, or `src/xcoin-cli stop` if the node is the daemon.

---

MIT. Consensus code descends from open upstream; [COPYING](COPYING) still
applies. Opcode / copyright notes: [docs/FORK.md](docs/FORK.md).
Operator runbook: [docs/LAUNCH.md](docs/LAUNCH.md).
