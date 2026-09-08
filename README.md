# X Coin (XFER)

**Nifty Raven** (@NFTRVN on X)  
Anonymous public identity — display name and handle only.

Peer-to-peer coin for users on X. No mining. Minute lottery among verified
active nodes; halvings add winners. Fair launch, no premine. ~21 billion
XFER. One free root identity asset per verified X handle.

Whitepaper: [whitepaper/XCOIN.md](whitepaper/XCOIN.md).  
Release 1.0: [docs/RELEASE-1.0.md](docs/RELEASE-1.0.md).

## How to use the wallet

**Release 1.0 ships a desktop GUI:** `xcoin-qt`. It talks to the same
node and `wallet.dat` as the CLI. There is **no** mobile app and **no**
X.com login.

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

**GUI (release 1.0):**

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

The Overview page shows **XFER balances**, **lottery status**, and **assets**.
Send / Receive / Assets are the other tabs. Help → About credits **Nifty Raven
(@NFTRVN on X)** only.

### 4. CLI (same wallet)

```bash
src/xcoind -daemon -server
src/xcoin-cli getwalletinfo
src/xcoin-cli getnewaddress
src/xcoin-cli sendtoaddress <their-X-address> 1.5
src/xcoin-cli addxverified NFTRVN
src/xcoin-cli linkxaccount NFTRVN
src/xcoin-cli getmainasset NFTRVN
src/xcoin-cli issue NFTRVN/NOTE 1
src/xcoin-cli getlotteryinfo
src/xcoin-cli stop
```

A 26-character X handle maps 1:1 to a 26-character root (max root name:
**32**). See [docs/ASSETS.md](docs/ASSETS.md).

You do not need a linked X handle to receive or send XFER.

Stop the GUI from the File menu, or `src/xcoin-cli stop` if the node is the daemon.

---

MIT. Consensus code descends from open upstream; [COPYING](COPYING) still
applies. Opcode / copyright notes: [docs/FORK.md](docs/FORK.md).
Operator runbook: [docs/LAUNCH.md](docs/LAUNCH.md).
