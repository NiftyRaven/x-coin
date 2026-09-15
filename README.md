# X Coin (XFER)

Two wallets. **Same chain.** Same 12-word data folder. Pick one.

| | **Light 1.0.14** | **Heavy 1.0.15** |
| --- | --- | --- |
| Feel | Simple Home buttons | Tree nav, more pages |
| Share lottery wins | On Home | **Rewards → Lottery share** |
| Market (list bags for XFER) | No | **Assets → Market** |
| Zip | [v1.0.14](https://github.com/NiftyRaven/x-coin/releases/tag/v1.0.14) | [v1.0.15](https://github.com/NiftyRaven/x-coin/releases/tag/v1.0.15) |

Stay on **Light** if you want the original wallet. Use **Heavy** only if
you want Market. Do **not** use **Code → Download ZIP**. Seed is lottery
law. Do not run 1.0.13 or older if you need Share lottery wins.

**Light (Windows):** [X-Coin-1.0.14-Windows.zip](https://github.com/NiftyRaven/x-coin/releases/download/v1.0.14/X-Coin-1.0.14-Windows.zip)

**Light (Linux):** [X-Coin-1.0.14-Linux-x86_64.tar.gz](https://github.com/NiftyRaven/x-coin/releases/download/v1.0.14/X-Coin-1.0.14-Linux-x86_64.tar.gz)

**Heavy (Windows):** [X-Coin-1.0.15-Windows.zip](https://github.com/NiftyRaven/x-coin/releases/download/v1.0.15/X-Coin-1.0.15-Windows.zip)

**Heavy (Linux):** [X-Coin-1.0.15-Linux-x86_64.tar.gz](https://github.com/NiftyRaven/x-coin/releases/download/v1.0.15/X-Coin-1.0.15-Linux-x86_64.tar.gz)

**Nifty Raven** (@NFTRVN on X)  
Anonymous public identity — display name and handle only.

Peer-to-peer coin for users on X. No mining. Minute lottery among **X
Verified** (blue check) active nodes; halvings add winners. Fair launch,
no premine. Ticker **XFER**. Subunit **xferon**.
One free root identity asset per signed-in X account. Handles up to 32
characters so a 26-character handle maps 1:1. Buttons-first desktop GUI
on **Linux** and **Windows**.

### Windows, double-click

1. Download [X-Coin-1.0.14-Windows.zip](https://github.com/NiftyRaven/x-coin/releases/download/v1.0.14/X-Coin-1.0.14-Windows.zip).
2. Right-click → Extract All.
3. Open the folder that appears. **X Coin Wallet.exe** is in that first folder.
4. Double-click **X Coin Wallet.exe**. The wallet window opens.
5. For practice, double-click **X Coin Practice Wallet.exe** instead.

`xcoin-qt.exe` is the same GUI as **X Coin Wallet.exe**. Use the labeled
starts so practice cannot mix with the main ledger.

Before **12 September 2026 around 3:16 AM ET**, use **X Coin Practice
Wallet.exe** only. Do not open the real wallet until launch night.

### Linux, double-click

1. Download [X-Coin-1.0.14-Linux-x86_64.tar.gz](https://github.com/NiftyRaven/x-coin/releases/download/v1.0.14/X-Coin-1.0.14-Linux-x86_64.tar.gz).
2. Right-click → Extract, or run `tar -xzf X-Coin-1.0.14-Linux-x86_64.tar.gz`.
3. Open the folder that appears. **X Coin Wallet** is in that first folder.
4. Double-click **X Coin Wallet**. The wallet window opens. You do not
   need a terminal.
5. For practice, double-click **X Coin Practice Wallet** instead.

If your file manager asks to trust or allow launching, allow it. The
starts are the wallet, not a setup script.

Before **12 September 2026 around 3:16 AM ET**, use **X Coin Practice
Wallet** only. Do not open the real wallet until launch night.

Whitepaper: [whitepaper/XCOIN.md](whitepaper/XCOIN.md).

---

## Practice vs real

| | Real wallet | Practice wallet |
| --- | --- | --- |
| File (Linux) | **X Coin Wallet** | **X Coin Practice Wallet** |
| File (Windows) | **X Coin Wallet.exe** | **X Coin Practice Wallet.exe** |
| Flag | none (no `-regtest`) | always `-regtest` |
| Linux data | `~/.xcoin` | `~/.xcoin/regtest` |
| Windows data | `%APPDATA%\XCoin` | `%APPDATA%\XCoin\regtest` |
| Window title | X Coin - Wallet | X Coin - Practice Wallet **[regtest]** |
| Ledger | main XFER | isolated practice coins, not main XFER |

Practice never writes the main ledger or the main `wallet.dat`. It is
still a real wallet program; only the coins and chain are the isolated
practice network.

---

## Launch night (operator)

Wait until **12 September 2026 around 3:16 AM ET**. Do not open the
real wallet before then. Practice is fine until that window. You do
not compile. You do not use **Code → Download ZIP**.

1. Double-click **X Coin Wallet.exe** (Windows) or **X Coin Wallet**
   (Linux) — the real one, not Practice.
2. Opening it starts the first node. That running wallet **is** the
   seed. It listens on P2P **38443**. There are no public DNS seeds.
   Other wallets do not find you through GitHub.
3. Sign in with X and claim the free root. Users never paste a Client
   ID. The operator Client ID is already baked in this folder's
   `xcoin.conf` (`xoauthclientid=`). Callback is
   `http://127.0.0.1:18791/callback`. Do not invent a host.
4. Home should say you are the first node and listen is on. **Provide
   my node IP** is off by default. Turn it on only to see the address
   others must use (`host:38443`). It does not publish your IP by
   itself.
5. Packaged `xcoin.conf` already has `addnode=172.191.195.221:38443`
   and `seednode=172.191.195.221:38443`. Do not invent a host. Users
   do not edit a conf file and do not use a terminal. **Provide my
   node IP** is only to confirm this box is that seed.
6. Keep this first wallet running. Then make the GitHub repo public
   so people download those packages.
7. Those people double-click and connect to this already-running
   wallet. The lottery happens once signed-in **X Verified** nodes are
   actually connected.

---

## After the repo is public

Download the Windows zip or Linux tarball from Releases (not **Code →
Download ZIP**). Double-click the real wallet. This folder's
`xcoin.conf` is read automatically and already has
`addnode=172.191.195.221:38443`. If that line is missing, the wallet
sits alone.

P2P **38443**. RPC **38442**. Mainnet addresses start with **X**.

**Provide my node IP** stays off unless you want to share *your*
listen address.

---

## Private until 12 September 2026

The chain stays private until the operator opens the repo on launch
night. There are **no public DNS seeds** and **no explorer** yet.
Keep the repository private until then. This file is not an
announcement.

---

## Using the wallet

First start creates `wallet.dat` in the data folder above. Send and
receive work without Sign in with X (normal wallet). Sign in with
X is required to claim your **one free root**. Lottery additionally
needs **X Verified** (blue check).

**Light 1.0.14** — Home is buttons first: Sign in with X, lottery,
Receive, Send, Activity, Transfer assets, Share lottery wins. Left-nav
**Assets** is Create (sub / unique). No Market.

**Heavy 1.0.15** — left tree:

| | |
| --- | --- |
| **WALLET** | Home, Receive, Send, Activity |
| **ASSETS** | Create, Transfer, Manage, **Market** |
| **REWARDS** | Lottery share, Asset dividends |
| **NODE** | My node |
| **ADVANCED** | Balances, RPC |

Home is the name card. Receive always shows an address and QR. Send
takes `@handle` or an `X…` address. **Assets → Market** lists a bag
for XFER; other Heavy wallets buy it. Light ignores those listings and
still confirms the buy.

A typed handle is not Verified X and cannot claim someone else's root.

First run still asks for a **12-word BIP39 seed** (create or restore).
Sign in with X is a separate identity proof on this node.

Light notes: [docs/RELEASE-1.0.14.md](docs/RELEASE-1.0.14.md).
Heavy notes: [docs/RELEASE-1.0.15.md](docs/RELEASE-1.0.15.md).
Market: [docs/MARKET.md](docs/MARKET.md).
Operator Client ID for live login (not a user paste field; one line in
the `xcoin.conf` shipped in the Windows and Linux folders):
[docs/XSIGNIN.md](docs/XSIGNIN.md).
Go-live clock: [docs/GO-LIVE.md](docs/GO-LIVE.md).
Paper: [whitepaper/XCOIN.md](whitepaper/XCOIN.md).

---

MIT. Consensus code descends from open upstream; [COPYING](COPYING)
still applies. Opcode / copyright notes: [docs/FORK.md](docs/FORK.md).

Developers who are compiling from source (not the path for a normal
person): [INSTALL.md](INSTALL.md).
