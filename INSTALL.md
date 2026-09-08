Building X Coin from source
===========================

A normal person does **not** compile. Unpack the Linux or Windows
package and double-click the labeled start: [README.md](README.md).

This page is only for people who are compiling. Whitepaper:
[whitepaper/XCOIN.md](whitepaper/XCOIN.md).

```bash
./autogen.sh
./configure --with-gui=qt5 --disable-bench --disable-tests --with-incompatible-bdb
make -j$(nproc)
contrib/xcoin/package-linux.sh
```

Windows zip (cross-compile from Linux with the repo `depends/` mingw
path):

```bash
sudo apt-get install g++-mingw-w64-x86-64 mingw-w64-x86-64-dev
sudo update-alternatives --set x86_64-w64-mingw32-g++ /usr/bin/x86_64-w64-mingw32-g++-posix
sudo update-alternatives --set x86_64-w64-mingw32-gcc /usr/bin/x86_64-w64-mingw32-gcc-posix
cd depends && make HOST=x86_64-w64-mingw32 -j$(nproc) && cd ..
mkdir -p build-win && cd build-win
CONFIG_SITE=$PWD/../depends/x86_64-w64-mingw32/share/config.site \
  ../configure --prefix=/ --with-gui=qt5 --disable-bench --disable-tests --enable-reduce-exports
make -j$(nproc)
cd ..
contrib/xcoin/package-windows.sh
```

A wallet (Berkeley DB) is required to produce lottery blocks. Legal /
opcode leftovers from the imported tree: [docs/FORK.md](docs/FORK.md).
