# X Coin (XFER)

**Nifty Raven** (@NFTRVN on X)  
Anonymous public identity — display name and handle only.

Peer-to-peer coin for users on X. No mining. Minute lottery among verified
active nodes; halvings add winners. Fair launch, no premine. ~21 billion
XFER. One free root identity asset per verified X handle.

Whitepaper: [whitepaper/XCOIN.md](whitepaper/XCOIN.md).

## How to use the wallet

The wallet **is the core wallet**: `xcoind` (node + `wallet.dat`) and
`xcoin-cli` (commands). `xcoin-qt` is an optional desktop GUI if you
build it. There is **no** mobile app and **no** X.com login.

Default datadir `~/.xcoin`, config `xcoin.conf`, P2P **38443**, RPC **38442**.
Mainnet addresses start with **X**.

### 1. Install dependencies (Ubuntu)

```bash
sudo apt-get update
sudo apt-get install -y build-essential libtool autotools-dev automake pkg-config \
    bsdmainutils python3 libevent-dev libboost-all-dev libssl-dev libdb++-dev
```

### 2. Build

```bash
./autogen.sh
./configure --without-gui --disable-bench --disable-tests --with-incompatible-bdb
make -j$(nproc)
```

You now have `src/xcoind` and `src/xcoin-cli`.

### 3. Start xcoind (this creates the wallet)

First start creates `~/.xcoin/wallet.dat`. There is no separate create-wallet
program.

```bash
mkdir -p ~/.xcoin
cat > ~/.xcoin/xcoin.conf <<'EOF'
server=1
rpcuser=xcoin
rpcpassword=change-me
EOF

src/xcoind -daemon -server
src/xcoin-cli getwalletinfo
```

Local-only practice (regtest — add `-regtest` to every `xcoin-cli` below):

```bash
src/xcoind -regtest -daemon -server
```

### 4. Get an address and receive XFER

```bash
src/xcoin-cli getnewaddress
```

Give that address to the sender. After the payment confirms:

```bash
src/xcoin-cli getbalance
src/xcoin-cli listunspent
```

On regtest you can credit yourself (assembles blocks; not mining):

```bash
ADDR=$(src/xcoin-cli -regtest getnewaddress)
src/xcoin-cli -regtest generatetoaddress 110 "$ADDR"
src/xcoin-cli -regtest getbalance
```

### 5. Send XFER

```bash
src/xcoin-cli sendtoaddress <their-X-address> 1.5
```

You do not need a linked X handle to receive or send.

### 6. Link an X handle (free root asset)

Operators add verified handles to an allowlist (offline: open
`https://x.com/<handle>`, check the badge). Example owner handle **NFTRVN**:

```bash
src/xcoin-cli addxverified NFTRVN
src/xcoin-cli linkxaccount NFTRVN
src/xcoin-cli getmainasset NFTRVN
src/xcoin-cli listmyassets
```

That assigns the free root **NFTRVN** (and owner token `NFTRVN!`). Users
cannot `issue` a new root. Same pattern for your own handle.

### 7. Issue a sub (or unique)

Needs the root owner token and 100 XFER (5 XFER for a unique):

```bash
src/xcoin-cli issue NFTRVN/NOTE 1
src/xcoin-cli issueunique NFTRVN '["ONE"]'
```

### Stay online (lottery)

```bash
src/xcoin-cli getlotteryinfo
```

`local_eligible` is true only if this node has a linked handle on the
allowlist. On mainnet you produce at most one block per minute when you win.

Stop the node: `src/xcoin-cli stop`.

---

MIT. Consensus code descends from open upstream; [COPYING](COPYING) still
applies. Opcode / copyright notes: [docs/FORK.md](docs/FORK.md).
Operator runbook: [docs/LAUNCH.md](docs/LAUNCH.md).
