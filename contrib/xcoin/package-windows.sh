#!/usr/bin/env bash
# Pack a Windows x86_64 zip people can open: xcoin-qt.exe + DLLs + Qt plugins + two labeled starts.
# Cross-compile path: depends HOST=x86_64-w64-mingw32, then this script.
# Configure Windows in a clean out-of-tree dir (build-win) with
# --without-qtdbus. Do not reuse an in-tree Linux config.status — that
# leaves USE_DBUS=1 and the Qt notifier fails on DBus.
# Practice always passes -regtest. Default practice datadir: %APPDATA%\XCoin\regtest
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
OUT="${OUT:-$ROOT/dist}"
VER="${VER:-1.0.3}"
NAME="xcoin-${VER}-win-x86_64"
STAGE="$OUT/$NAME"
HOST="${MINGW_HOST:-x86_64-w64-mingw32}"
PREFIX="${MINGW_PREFIX:-$ROOT/depends/${HOST}}"
LAUNCHER_SRC="$ROOT/contrib/xcoin/launcher.c"

# Prefer an out-of-tree Windows build, then in-tree.
find_win_bin() {
  local name="$1"
  local candidates=(
    "$ROOT/build-win/src/qt/${name}"
    "$ROOT/build-win/src/${name}"
    "$ROOT/src/qt/${name}"
    "$ROOT/src/${name}"
    "${DESTDIR:-}/bin/${name}"
  )
  local c
  for c in "${candidates[@]}"; do
    if [[ -f "$c" ]]; then
      echo "$c"
      return 0
    fi
  done
  return 1
}

QT="$(find_win_bin xcoin-qt.exe || true)"
DAEMON="$(find_win_bin xcoind.exe || true)"
CLI="$(find_win_bin xcoin-cli.exe || true)"
TX="$(find_win_bin xcoin-tx.exe || true)"

if [[ -z "$QT" || -z "$DAEMON" || -z "$CLI" ]]; then
  echo "missing Windows binaries (xcoin-qt.exe / xcoind.exe / xcoin-cli.exe)" >&2
  echo "Build with:" >&2
  echo "  cd depends && make HOST=${HOST}" >&2
  echo "  rm -rf build-win && mkdir -p build-win && cd build-win" >&2
  echo "  CONFIG_SITE=\$PWD/../depends/${HOST}/share/config.site ../configure --prefix=/ --with-gui=qt5 --disable-bench --disable-tests --enable-reduce-exports --without-qtdbus" >&2
  echo "  make -j\$(nproc)" >&2
  exit 1
fi

CC="${HOST}-gcc"
if ! command -v "$CC" >/dev/null 2>&1; then
  echo "missing $CC (install g++-mingw-w64-x86-64)" >&2
  exit 1
fi

rm -rf "$STAGE"
mkdir -p "$STAGE/plugins/platforms" "$STAGE/docs"
install -m 0644 "$ROOT/contrib/xcoin/xcoin.conf" "$STAGE/xcoin.conf"

install -m 0644 "$QT" "$STAGE/xcoin-qt.exe"
install -m 0644 "$DAEMON" "$STAGE/xcoind.exe"
install -m 0644 "$CLI" "$STAGE/xcoin-cli.exe"
if [[ -n "$TX" && -f "$TX" ]]; then
  install -m 0644 "$TX" "$STAGE/xcoin-tx.exe"
fi

"$CC" -O2 -mwindows -o "$STAGE/X Coin Wallet.exe" "$LAUNCHER_SRC"
"$CC" -O2 -mwindows -DPRACTICE -o "$STAGE/X Coin Practice Wallet.exe" "$LAUNCHER_SRC"

# Strip PE files
if command -v "${HOST}-strip" >/dev/null 2>&1; then
  "${HOST}-strip" -s "$STAGE"/*.exe || true
fi

copy_if() {
  local src="$1" dest="$2"
  if [[ -f "$src" ]]; then
    cp -a "$src" "$dest"
    return 0
  fi
  return 1
}

# Qt plugins: static Qt already Q_IMPORT_PLUGIN(QWindowsIntegrationPlugin).
# Only copy loadable DLLs if present. Do not ship .a archives.
copy_plugin_dlls() {
  local dest="$1"
  shift
  mkdir -p "$dest"
  local d f
  for d in "$@"; do
    [[ -d "$d" ]] || continue
    for f in "$d"/*.dll "$d"/*.DLL; do
      [[ -f "$f" ]] || continue
      cp -a "$f" "$dest/"
    done
  done
}
copy_plugin_dlls "$STAGE/plugins/platforms" \
  "$PREFIX/plugins/platforms" "$PREFIX/lib/qt5/plugins/platforms" "$PREFIX/share/qt5/plugins/platforms"
for kind in imageformats platforminputcontexts iconengines styles; do
  copy_plugin_dlls "$STAGE/plugins/$kind" \
    "$PREFIX/plugins/$kind" "$PREFIX/lib/qt5/plugins/$kind"
done

# windeployqt if the depends prefix built it
WINDEPLOY=""
for c in "$PREFIX/native/bin/windeployqt" "$PREFIX/bin/windeployqt" "$PREFIX/native/bin/${HOST}-windeployqt"; do
  if [[ -x "$c" ]]; then WINDEPLOY="$c"; break; fi
done
if [[ -n "$WINDEPLOY" ]]; then
  "$WINDEPLOY" --dir "$STAGE" --compiler-runtime --no-translations "$STAGE/xcoin-qt.exe" || true
fi

cat > "$STAGE/qt.conf" <<'EOF'
[Paths]
Plugins=plugins
Prefix=.
EOF

# Collect DLL imports from every PE in the stage, search known prefixes
search_dirs=(
  "$PREFIX/bin"
  "$PREFIX/lib"
  "/usr/${HOST}/bin"
  "/usr/${HOST}/lib"
  "/usr/lib/gcc/${HOST}/13-posix"
  "/usr/lib/gcc/${HOST}/13-win32"
  "/usr/lib/gcc/${HOST}/12-posix"
  "/usr/lib/gcc/${HOST}/10-posix"
)

shopt -s nullglob
gcc_dirs=(/usr/lib/gcc/${HOST}/*posix /usr/lib/gcc/${HOST}/*)
shopt -u nullglob
search_dirs+=("${gcc_dirs[@]}")

find_dll() {
  local name="$1"
  local d
  for d in "${search_dirs[@]}"; do
    [[ -d "$d" ]] || continue
    if [[ -f "$d/$name" ]]; then
      echo "$d/$name"
      return 0
    fi
    # case-insensitive on Windows names
    local hit
    hit="$(find "$d" -maxdepth 1 -iname "$name" -print -quit 2>/dev/null || true)"
    if [[ -n "$hit" ]]; then
      echo "$hit"
      return 0
    fi
  done
  return 1
}

OBJDUMP="${HOST}-objdump"
copied_any=1
pass=0
while [[ "$copied_any" -eq 1 && "$pass" -lt 12 ]]; do
  copied_any=0
  pass=$((pass + 1))
  while IFS= read -r -d '' pe; do
    while read -r dll; do
      [[ -n "$dll" ]] || continue
      case "$dll" in
        KERNEL32.dll|kernel32.dll|USER32.dll|user32.dll|GDI32.dll|gdi32.dll|ADVAPI32.dll|advapi32.dll|SHELL32.dll|shell32.dll|ole32.dll|OLE32.dll|OLEAUT32.dll|oleaut32.dll|COMCTL32.dll|comctl32.dll|COMDLG32.dll|comdlg32.dll|IMM32.dll|imm32.dll|WS2_32.dll|ws2_32.dll|WS2_32.DLL|SHLWAPI.dll|shlwapi.dll|WINMM.dll|winmm.dll|NETAPI32.dll|netapi32.dll|IPHLAPI.dll|iphlpapi.dll|CRYPT32.dll|crypt32.dll|bcrypt.dll|BCRYPT.dll|ntdll.dll|NTDLL.dll|msvcrt.dll|MSVCRT.dll|SETUPAPI.dll|setupapi.dll|VERSION.dll|version.dll|WINSPOOL.DRV|winspool.drv|dwmapi.dll|DWMAPI.dll|UXTHEME.dll|uxtheme.dll|DNSAPI.dll|dnsapi.dll|USERENV.dll|userenv.dll|RPCRT4.dll|rpcrt4.dll|sechost.dll|SECHOST.dll|combase.dll|COMBASE.dll|WTSAPI32.dll|wtsapi32.dll)
          continue
          ;;
      esac
      base="$(basename "$dll")"
      if [[ -f "$STAGE/$base" ]]; then
        continue
      fi
      src="$(find_dll "$base" || true)"
      if [[ -n "$src" ]]; then
        cp -a "$src" "$STAGE/$base"
        copied_any=1
        echo "bundled $base"
      fi
    done < <("$OBJDUMP" -p "$pe" 2>/dev/null | awk '/DLL Name:/{print $3}')
  done < <(find "$STAGE" -iname '*.exe' -print0 -o -iname '*.dll' -print0)
done

# Always try mingw runtime DLLs (needed even with static Qt)
for dll in libgcc_s_seh-1.dll libstdc++-6.dll libwinpthread-1.dll libssp-0.dll; do
  if [[ ! -f "$STAGE/$dll" ]]; then
    src="$(find_dll "$dll" || true)"
    if [[ -n "$src" ]]; then
      cp -a "$src" "$STAGE/$dll"
      echo "bundled runtime $dll"
    fi
  fi
done

cat > "$STAGE/START HERE.txt" <<EOF
X Coin (XFER) ${VER} — Windows x86_64
Nifty Raven (@NFTRVN on X)

Open this folder. Double-click one start:

  X Coin Wallet.exe
      The real wallet. Uses %APPDATA%\\XCoin and the main ledger.
      Does not pass -regtest.

  X Coin Practice Wallet.exe
      Isolated practice. Always starts with -regtest.
      Uses %APPDATA%\\XCoin\\regtest only. It never writes the main
      ledger or the main wallet.dat. The window title says [regtest].

You do not compile anything. You do not open source code.

The chain stays private until the September 12, 2026 window.
There are no public DNS seeds and no explorer yet.

This folder's xcoin.conf is read automatically when you
double-click. If it has addnode=<host>:38443, you connect
to that node with no terminal. The operator puts that line
in before opening the GitHub repo. Do not invent a host.
EOF

cat > "$STAGE/README.txt" <<EOF
X Coin (XFER) ${VER} — Windows x86_64 (private)
Nifty Raven (@NFTRVN on X)

WHICH FILE TO OPEN
------------------
Double-click:  X Coin Wallet.exe
Practice:      X Coin Practice Wallet.exe

xcoin-qt.exe is the same GUI as X Coin Wallet.exe (no -regtest).
Use the labeled starts so practice cannot mix with the main ledger.

WHAT PRACTICE MEANS
-------------------
Practice always passes -regtest. It writes only
%APPDATA%\\XCoin\\regtest (including practice wallet.dat).
The real wallet uses %APPDATA%\\XCoin. Practice coins are not
main XFER. The practice window title includes [regtest].

REAL WALLET
-----------
X Coin Wallet.exe starts with no -regtest. First run creates
wallet.dat under %APPDATA%\\XCoin. Sign in with X is required
to send, receive, and claim your free root. Lottery needs
X Verified.

UNTIL SEPTEMBER 12, 2026
------------------------
The chain stays private until that window. No public DNS seeds.
No explorer. This folder's xcoin.conf is read automatically.
If it contains addnode=<host>:38443, this wallet connects to
that node. You do not edit a conf file. Sign in with X uses
the operator Client ID in that same xcoin.conf (xoauthclientid=).
There is no Client ID paste box.
EOF

cp "$ROOT/README.md" "$STAGE/docs/" 2>/dev/null || true
cp "$ROOT/docs/RELEASE-1.0.md" "$STAGE/docs/" 2>/dev/null || true
cp "$ROOT/whitepaper/XCOIN.md" "$STAGE/docs/WHITEPAPER.md" 2>/dev/null || true
cp "$ROOT/doc/README_windows.txt" "$STAGE/docs/" 2>/dev/null || true

ZIP="$OUT/${NAME}.zip"
mkdir -p "$OUT"
rm -f "$ZIP"
# zip from parent so the archive contains the folder
(cd "$OUT" && zip -r -X -9 "${NAME}.zip" "$NAME")
echo "wrote $ZIP"
ls -lh "$ZIP"
find "$STAGE" -maxdepth 2 -type f | sort
echo "$ZIP"
