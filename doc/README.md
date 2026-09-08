X Coin
==============

Setup
---------------------
X Coin is the node and wallet for this private network. It downloads and, by
default, stores the entire history of X Coin transactions.

This tree is built from source. See the root [README.md](../README.md) for
the GUI path (`--with-gui=qt5`, `xcoin-qt` / `xcoind` / `xcoin-cli`). Legal / opcode notes:
[docs/FORK.md](../docs/FORK.md).

Running
---------------------
The following are some helpful notes on how to run X Coin on your native platform.

### Linux

1) Build with `./autogen.sh && ./configure --with-gui=qt5 --disable-bench --disable-tests --with-incompatible-bdb && make -j$(nproc)`.

2) Run the GUI (release 1.1) or the daemon:

   `./src/qt/xcoin-qt`

   `./src/xcoind -daemon`

#### Ubuntu

```
sudo apt update
sudo apt install build-essential libtool autotools-dev automake pkg-config \
    bsdmainutils python3 libevent-dev libboost-all-dev libssl-dev libdb++-dev \
    qtbase5-dev qttools5-dev qttools5-dev-tools libqt5svg5-dev \
    libqrencode-dev protobuf-compiler libprotobuf-dev
```

BDB 5.3 is accepted with `--with-incompatible-bdb`. A wallet is required to
produce lottery blocks. Man pages under `doc/man/` are imported stubs
(old version strings); use `--help` / `--version` on the binaries.

### OS X

Build notes: [build-osx.md](build-osx.md). The app bundle is `XCoin-Qt.app`.

### Windows

Build notes: [build-windows.md](build-windows.md). Launch `xcoin-qt.exe` or `xcoind.exe`.

### Need Help?

Private-test operators: [docs/LAUNCH.md](../docs/LAUNCH.md).

Building from source
---------------------
Developer notes on libraries and compile flags:

- [Dependencies](dependencies.md)
- [OS X Build Notes](build-osx.md)
- [Unix Build Notes](build-unix.md)
- [Windows Build Notes](build-windows.md)
- [OpenBSD Build Notes](build-openbsd.md)
- [Gitian Building Guide](gitian-building.md)

Development
---------------------
See the root [README](../README.md) and [docs/FORK.md](../docs/FORK.md).

- [Developer Notes](developer-notes.md)
- [Release Notes](release-notes.md)
- [Release Process](release-process.md)
- [Translation Process](translation_process.md)
- [Translation Strings Policy](translation_strings_policy.md)
- [Unauthenticated REST Interface](REST-interface.md)
- [Shared Libraries](shared-libraries.md)
- [BIPS](bips.md)
- [Dnsseed Policy](dnsseed-policy.md)
- [Benchmarking](benchmarking.md)

### Miscellaneous
- [Assets Attribution](assets-attribution.md)
- [Files](files.md)
- [Fuzz-testing](fuzzing.md)
- [Reduce Traffic](reduce-traffic.md)
- [Tor Support](tor.md)
- [Init Scripts (systemd/upstart/openrc)](init.md)
- [ZMQ](zmq.md)
- [Configuration file](xcoin-conf.md)

License
---------------------
Distributed under the [MIT software license](../COPYING).
This product includes software developed by the OpenSSL Project for use in the [OpenSSL Toolkit](https://www.openssl.org/). This product includes
cryptographic software written by Eric Young ([eay@cryptsoft.com](mailto:eay@cryptsoft.com)), and UPnP software written by Thomas Bernard.
