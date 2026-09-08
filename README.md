# X Coin (XFER)

**[Download Windows wallet](https://github.com/NiftyRaven/x-coin/releases/download/v1.0.0/X-Coin-1.0.0-Windows.zip)**  
**[Download Linux wallet](https://github.com/NiftyRaven/x-coin/releases/download/v1.0.0/X-Coin-1.0.0-Linux-x86_64.tar.gz)**

Those two links are the wallets. They live on the
[Releases](https://github.com/NiftyRaven/x-coin/releases/tag/v1.0.0)
page. Do **not** use **Code → Download ZIP** — that is the source tree,
not a wallet.

**Nifty Raven** (@NFTRVN on X)  
Anonymous public identity — display name and handle only.

Peer-to-peer coin for users on X. No mining. Minute lottery among **X
Verified** (blue check) active nodes; halvings add winners. Optional
pools. Fair launch, no premine. Ticker **XFER**. Subunit **xferon**.
One free root identity asset per signed-in X account. Handles up to 32
characters so a 26-character handle maps 1:1. Buttons-first desktop GUI
on **Linux** and **Windows**.

### Windows, double-click

1. Download [X-Coin-1.0.0-Windows.zip](https://github.com/NiftyRaven/x-coin/releases/download/v1.0.0/X-Coin-1.0.0-Windows.zip).
2. Right-click → Extract All.
3. Open the folder that appears. **X Coin Wallet.exe** is in that first folder.
4. Double-click **X Coin Wallet.exe**. The wallet window opens.
5. For practice, double-click **X Coin Practice Wallet.exe** instead.

`xcoin-qt.exe` is the same GUI as **X Coin Wallet.exe**. Use the labeled
starts so practice cannot mix with the main ledger.

### Linux, double-click

1. Download [X-Coin-1.0.0-Linux-x86_64.tar.gz](https://github.com/NiftyRaven/x-coin/releases/download/v1.0.0/X-Coin-1.0.0-Linux-x86_64.tar.gz).
2. Right-click → Extract, or run `tar -xzf X-Coin-1.0.0-Linux-x86_64.tar.gz`.
3. Open the folder that appears. **X Coin Wallet** is in that first folder.
4. Double-click **X Coin Wallet**. The wallet window opens. You do not
   need a terminal.
5. For practice, double-click **X Coin Practice Wallet** instead.

If your file manager asks to trust or allow launching, allow it. The
starts are the wallet, not a setup script.

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

## Private until 12 September 2026

The chain stays private until the **September 12, 2026** window. There
are **no public seeds** and **no explorer** yet.

Invited people add a seed by hand on port **38443**:

- In the GUI: Home → **My node** → **Provide my node IP** (off by
  default) when you want to share yours.
- Or put `addnode=<host>:38443` in `xcoin.conf`
  (Linux `~/.xcoin/xcoin.conf`, Windows `%APPDATA%\XCoin\xcoin.conf`).

P2P **38443**. RPC **38442**. Mainnet addresses start with **X**.

---

## Using the wallet

First start creates `wallet.dat` in the data folder above. Sign in with
X is required to send, receive, and claim your **one free root**.
Lottery additionally needs **X Verified** (blue check). Optional
**POOL** is on Home (create / join / leave).

Home is buttons first: Sign in with X, lottery, pool, Receive, Send,
Activity, Transfer assets. A typed handle is not Verified X and cannot
claim someone else's root.

First run still asks for a **12-word BIP39 seed** (create or restore).
Sign in with X is a separate identity proof on this node.

Operator Client ID for live login: [docs/XSIGNIN.md](docs/XSIGNIN.md).
Go-live clock: [docs/GO-LIVE.md](docs/GO-LIVE.md).
Paper: [whitepaper/XCOIN.md](whitepaper/XCOIN.md).

---

MIT. Consensus code descends from open upstream; [COPYING](COPYING)
still applies. Opcode / copyright notes: [docs/FORK.md](docs/FORK.md).

Developers who are compiling from source (not the path for a normal
person): [INSTALL.md](INSTALL.md).
