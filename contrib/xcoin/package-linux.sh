#!/usr/bin/env bash
# Pack a Linux x86_64 folder people can open: GUI + bundled libs + two labeled starts.
# Practice always passes -regtest. It does not write the main ledger or ~/.xcoin/wallet.dat.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
OUT="${OUT:-$ROOT/dist}"
VER="${VER:-1.0.10}"
NAME="xcoin-${VER}-linux-x86_64"
STAGE="$OUT/$NAME"

QT="$ROOT/src/qt/xcoin-qt"
DAEMON="$ROOT/src/xcoind"
CLI="$ROOT/src/xcoin-cli"
TX="$ROOT/src/xcoin-tx"
LAUNCHER_SRC="$ROOT/contrib/xcoin/launcher.c"

for f in "$QT" "$DAEMON" "$CLI"; do
  if [[ ! -x "$f" ]]; then
    echo "missing $f — build with --with-gui=qt5 first" >&2
    exit 1
  fi
done
if [[ ! -f "$LAUNCHER_SRC" ]]; then
  echo "missing $LAUNCHER_SRC" >&2
  exit 1
fi

rm -rf "$STAGE"
mkdir -p "$STAGE/bin" "$STAGE/lib" "$STAGE/plugins" "$STAGE/docs"
install -m 0644 "$ROOT/contrib/xcoin/xcoin.conf" "$STAGE/xcoin.conf"

install -m 0755 "$QT" "$STAGE/bin/xcoin-qt"
install -m 0755 "$DAEMON" "$STAGE/bin/xcoind"
install -m 0755 "$CLI" "$STAGE/bin/xcoin-cli"
if [[ -x "$TX" ]]; then
  install -m 0755 "$TX" "$STAGE/bin/xcoin-tx"
fi

cc -O2 -o "$STAGE/X Coin Wallet" "$LAUNCHER_SRC"
cc -O2 -DPRACTICE -o "$STAGE/X Coin Practice Wallet" "$LAUNCHER_SRC"
chmod 0755 "$STAGE/X Coin Wallet" "$STAGE/X Coin Practice Wallet"

# Qt plugins (without platforms/libqxcb.so the GUI cannot start from a file manager)
QT_PLUGINS=""
if command -v qmake >/dev/null 2>&1; then
  QT_PLUGINS="$(qmake -query QT_INSTALL_PLUGINS 2>/dev/null || true)"
fi
if [[ -z "$QT_PLUGINS" || ! -d "$QT_PLUGINS" ]]; then
  for d in /usr/lib/x86_64-linux-gnu/qt5/plugins /usr/lib/qt5/plugins; do
    if [[ -d "$d" ]]; then QT_PLUGINS="$d"; break; fi
  done
fi
if [[ -z "$QT_PLUGINS" || ! -d "$QT_PLUGINS/platforms" ]]; then
  echo "could not find Qt platforms plugins" >&2
  exit 1
fi
mkdir -p "$STAGE/plugins/platforms" "$STAGE/plugins/imageformats" \
         "$STAGE/plugins/platforminputcontexts" "$STAGE/plugins/xcbglintegrations" \
         "$STAGE/plugins/iconengines"
cp -a "$QT_PLUGINS/platforms/"*.so "$STAGE/plugins/platforms/" 2>/dev/null || true
if [[ ! -f "$STAGE/plugins/platforms/libqxcb.so" ]]; then
  echo "missing libqxcb.so in $QT_PLUGINS/platforms" >&2
  exit 1
fi
cp -a "$QT_PLUGINS/imageformats/"*.so "$STAGE/plugins/imageformats/" 2>/dev/null || true
cp -a "$QT_PLUGINS/platforminputcontexts/"*.so "$STAGE/plugins/platforminputcontexts/" 2>/dev/null || true
cp -a "$QT_PLUGINS/xcbglintegrations/"*.so "$STAGE/plugins/xcbglintegrations/" 2>/dev/null || true
cp -a "$QT_PLUGINS/iconengines/"*.so "$STAGE/plugins/iconengines/" 2>/dev/null || true

cat > "$STAGE/bin/qt.conf" <<'EOF'
[Paths]
Prefix=..
Plugins=plugins
Libraries=lib
EOF
cp "$STAGE/bin/qt.conf" "$STAGE/qt.conf"

python3 - "$STAGE" <<'PY'
import os, re, subprocess, sys
from pathlib import Path
stage = Path(sys.argv[1])
libdir = stage / "lib"
skip = re.compile(
    r"(linux-vdso|ld-linux|libc\.so\.6$|libm\.so\.6$|libdl\.so|"
    r"libpthread\.so|librt\.so|libresolv|libnss_|libutil\.so|libanl\.so|"
    r"libGL\.so|libGLdispatch|libGLX|libOpenGL|libEGL\.so|libnvidia|"
    r"libdrm\.so|libcuda)"
)

def resolved_libs(path):
    try:
        out = subprocess.check_output(["ldd", str(path)], text=True, stderr=subprocess.DEVNULL)
    except subprocess.CalledProcessError:
        return []
    libs = []
    for line in out.splitlines():
        line = line.strip()
        if " => " in line:
            needed, rest = line.split(" => ", 1)
            needed = needed.strip()
            right = rest.split()[0]
            if right.startswith("/"):
                libs.append((needed, right))
        elif line.startswith("/") and ".so" in line:
            p = line.split()[0]
            libs.append((Path(p).name, p))
    return libs

seeds = []
for p in (stage / "bin").iterdir():
    if p.is_file() and os.access(p, os.X_OK):
        seeds.append(p)
for p in (stage / "plugins").rglob("*.so"):
    seeds.append(p)

copied = set()
queue = list(seeds)
while queue:
    cur = queue.pop()
    for needed, lib in resolved_libs(cur):
        if skip.search(needed) or skip.search(lib):
            continue
        src = Path(lib).resolve()
        if not src.is_file():
            continue
        # Copy as the SONAME ldd looks up (libzmq.so.5), not the
        # resolved real name (libzmq.so.5.2.5). Otherwise rpath misses it.
        dest_name = Path(needed).name if needed else src.name
        dest = libdir / dest_name
        key = (dest_name, src)
        if key in copied:
            continue
        dest.write_bytes(src.read_bytes())
        dest.chmod(0o0755)
        copied.add(key)
        queue.append(dest)
        if src.name != dest_name:
            alt = libdir / src.name
            if not alt.exists() and not alt.is_symlink():
                alt.symlink_to(dest_name)

print("bundled %d shared libraries" % len(copied))
PY

# rpath so a double-click does not need a terminal to export LD_LIBRARY_PATH
if command -v patchelf >/dev/null 2>&1; then
  for f in "$STAGE/bin/"*; do
    if [[ -f "$f" && -x "$f" ]]; then
      patchelf --force-rpath --set-rpath '$ORIGIN/../lib' "$f" 2>/dev/null || true
    fi
  done
  while IFS= read -r -d '' so; do
    patchelf --force-rpath --set-rpath '$ORIGIN/../../lib' "$so" 2>/dev/null || true
  done < <(find "$STAGE/plugins" -name '*.so' -print0)
  for so in "$STAGE/lib/"*.so*; do
    [[ -f "$so" ]] || continue
    patchelf --force-rpath --set-rpath '$ORIGIN' "$so" 2>/dev/null || true
  done
fi

for f in "$STAGE/bin/"*; do
  [[ -f "$f" && -x "$f" ]] || continue
  case "$f" in
    *.conf) continue ;;
  esac
  strip -s "$f" 2>/dev/null || strip "$f" || true
done
strip -s "$STAGE/X Coin Wallet" "$STAGE/X Coin Practice Wallet" 2>/dev/null || true

# Friendly names that sort next to the launchers
cat > "$STAGE/START HERE.txt" <<EOF
X Coin (XFER) ${VER} — Linux x86_64
Nifty Raven (@NFTRVN on X)

Open this folder. Double-click one start:

  X Coin Wallet
      The real wallet. Uses ~/.xcoin and the main ledger.
      Does not pass -regtest.

  X Coin Practice Wallet
      Isolated practice. Always starts with -regtest.
      Uses ~/.xcoin/regtest only. It never writes the main
      ledger or ~/.xcoin/wallet.dat. The window title says [regtest].

You do not compile anything. You do not open source code.

The chain stays private until the September 12, 2026 window.
There are no public DNS seeds and no explorer yet.

This folder's xcoin.conf is read automatically when you
double-click. It already has addnode=172.191.195.221:38443
and seednode=172.191.195.221:38443. You connect with no
terminal. Do not invent a host. Sign in with X uses the
operator Client ID in that same file. There is no Client
ID paste box.
EOF

cat > "$STAGE/README.txt" <<EOF
X Coin (XFER) ${VER} — Linux x86_64 (private)
Nifty Raven (@NFTRVN on X)

WHICH FILE TO OPEN
------------------
Double-click:  X Coin Wallet
Practice:      X Coin Practice Wallet

Those two starts are ELF programs, not terminal scripts. A file
manager should open the wallet window with no terminal.

WHAT PRACTICE MEANS
-------------------
Practice always passes -regtest. It writes only ~/.xcoin/regtest
(including practice wallet.dat). The real wallet uses ~/.xcoin
and is a different ledger. Practice coins are not main XFER.
The practice window title includes [regtest].

REAL WALLET
-----------
X Coin Wallet starts with no -regtest. First run creates
~/.xcoin/wallet.dat. Send and receive work without Sign in
with X. Sign in with X is required to claim your free root.
Lottery needs X Verified.

UNTIL SEPTEMBER 12, 2026
------------------------
The chain stays private until that window. No public DNS seeds.
No explorer. This folder's xcoin.conf is read automatically.
If it contains addnode=<host>:38443, this wallet connects to
that node. You do not edit a conf file. Sign in with X uses
the operator Client ID in that same xcoin.conf (xoauthclientid=).
There is no Client ID paste box.

Also in this folder: xcoin.conf, bin/xcoin-qt, bin/xcoind, bin/xcoin-cli, lib/.
EOF

cp "$ROOT/README.md" "$STAGE/docs/" 2>/dev/null || true
cp "$ROOT/docs/RELEASE-1.0.md" "$STAGE/docs/" 2>/dev/null || true
cp "$ROOT/docs/LOTTERY.md" "$STAGE/docs/" 2>/dev/null || true
cp "$ROOT/docs/GO-LIVE.md" "$STAGE/docs/" 2>/dev/null || true
cp "$ROOT/whitepaper/XCOIN.md" "$STAGE/docs/WHITEPAPER.md" 2>/dev/null || true

# desktop entries (some desktops honor these; launchers still work without them)
cat > "$STAGE/X-Coin-Wallet.desktop" <<EOF
[Desktop Entry]
Type=Application
Name=X Coin Wallet
Comment=X Coin (XFER) real wallet — not practice
Exec=$(printf '%q' "$STAGE/X Coin Wallet")
TryExec=xcoin-qt
Terminal=false
Categories=Finance;
EOF
# Relative Exec so the extracted folder works: use a tiny helper
cat > "$STAGE/X-Coin-Wallet.desktop" <<'EOF'
[Desktop Entry]
Type=Application
Name=X Coin Wallet
Comment=X Coin (XFER) real wallet — not practice. Double-click "X Coin Wallet" if this shortcut is not trusted yet.
Exec="./X Coin Wallet"
Path=.
Terminal=false
Categories=Finance;
EOF
cat > "$STAGE/X-Coin-Practice-Wallet.desktop" <<'EOF'
[Desktop Entry]
Type=Application
Name=X Coin Practice Wallet
Comment=Isolated -regtest practice. Window title says [regtest]. Never writes the main ledger.
Exec="./X Coin Practice Wallet"
Path=.
Terminal=false
Categories=Finance;
EOF
chmod 0755 "$STAGE/"*.desktop

TAR="$OUT/${NAME}.tar.gz"
mkdir -p "$OUT"
tar -C "$OUT" -czf "$TAR" "$NAME"
echo "wrote $TAR"
ls -lh "$TAR"
echo "stage $STAGE"
du -sh "$STAGE" "$STAGE/lib" "$STAGE/bin" "$STAGE/plugins"
echo "$TAR"
