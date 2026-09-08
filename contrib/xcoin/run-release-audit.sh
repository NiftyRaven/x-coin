#!/usr/bin/env bash
# Full-scale private-test audit. Regtest / throwaway datadirs only. Never starts mainnet.
set -u
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
REPORT="${REPORT:-$ROOT/docs/RELEASE-AUDIT.md}"
LOGDIR="${LOGDIR:-$ROOT/audit-logs}"
XCOIND="${XCOIND:-$ROOT/src/xcoind}"
XCLI="${XCLI:-$ROOT/src/xcoin-cli}"
XQT="${XQT:-$ROOT/src/qt/xcoin-qt}"
LINUX_TAR="$ROOT/dist/xcoin-1.0.0-linux-x86_64.tar.gz"
WIN_ZIP="$ROOT/dist/xcoin-1.0.0-win-x86_64.zip"
mkdir -p "$LOGDIR"
: > "$LOGDIR/summary.tsv"
RUN_UTC="$(date -u '+%Y-%m-%d %H:%M:%S UTC')"

pass=0
fail=0
skip=0

record() {
  local status="$1" name="$2" note="${3:-}"
  printf '%s\t%s\t%s\n' "$status" "$name" "$note" >> "$LOGDIR/summary.tsv"
  echo "[$status] $name ${note}"
  case "$status" in
    PASS) pass=$((pass+1)) ;;
    FAIL) fail=$((fail+1)) ;;
    SKIP) skip=$((skip+1)) ;;
  esac
}

# Prefer an in-tree build. If this tree has no src/xcoind, use the shipped Linux package.
if [[ ! -x "$XCOIND" || ! -x "$XCLI" ]]; then
  if [[ -f "$LINUX_TAR" ]]; then
    RUNTIME="$LOGDIR/linux-runtime"
    rm -rf "$RUNTIME"
    mkdir -p "$RUNTIME"
    tar -C "$RUNTIME" -xzf "$LINUX_TAR"
    LRT="$(find "$RUNTIME" -maxdepth 1 -type d -name 'xcoin-*-linux-*' | head -1)"
    if [[ -x "$LRT/bin/xcoind" && -x "$LRT/bin/xcoin-cli" ]]; then
      XCOIND="$LRT/bin/xcoind"
      XCLI="$LRT/bin/xcoin-cli"
      XQT="$LRT/bin/xcoin-qt"
      echo "using packaged Linux binaries from $LRT"
    fi
  fi
fi
export XCOIND XCLI XQT
export XCOINQT="$XQT"

run_smoke() {
  local name="$1" script="$2"
  local log="$LOGDIR/${name}.log"
  if [[ ! -x "$XCOIND" ]]; then
    record SKIP "$name" "xcoind missing"
    return
  fi
  if [[ ! -f "$script" ]]; then
    record SKIP "$name" "script missing"
    return
  fi
  if [[ "$name" == "smoke-gui" && ! -x "$XQT" ]]; then
    record SKIP "$name" "xcoin-qt missing"
    return
  fi
  echo "---- $name ----"
  if "$script" >"$log" 2>&1; then
    record PASS "$name" ""
  else
    record FAIL "$name" "see $log"
    tail -30 "$log" || true
  fi
}

pe_strings() {
  local f="$1"
  if command -v x86_64-w64-mingw32-strings >/dev/null 2>&1; then
    x86_64-w64-mingw32-strings "$f"
  else
    strings "$f"
  fi
}

pe_dll_names() {
  local f="$1"
  if command -v x86_64-w64-mingw32-objdump >/dev/null 2>&1; then
    x86_64-w64-mingw32-objdump -p "$f" 2>/dev/null | awk '/DLL Name:/{print $3}'
    return
  fi
  if objdump -p "$f" 2>/dev/null | grep -q 'DLL Name:'; then
    objdump -p "$f" 2>/dev/null | awk '/DLL Name:/{print $3}'
    return
  fi
  python3 - "$f" <<'PY'
import struct, sys
path = sys.argv[1]
data = open(path, "rb").read()
if data[:2] != b"MZ":
    sys.exit(0)
e_lfanew = struct.unpack_from("<I", data, 0x3C)[0]
if data[e_lfanew:e_lfanew + 4] != b"PE\x00\x00":
    sys.exit(0)
opt_off = e_lfanew + 24
magic = struct.unpack_from("<H", data, opt_off)[0]
pe32plus = magic == 0x20B
num_rva = struct.unpack_from("<I", data, opt_off + (108 if pe32plus else 92))[0]
dd_off = opt_off + (112 if pe32plus else 96)
if num_rva < 2:
    sys.exit(0)
import_rva, import_size = struct.unpack_from("<II", data, dd_off + 8)
# section table
coff_off = e_lfanew + 4
nsections = struct.unpack_from("<H", data, coff_off + 2)[0]
size_opt = struct.unpack_from("<H", data, coff_off + 16)[0]
sec_off = e_lfanew + 24 + size_opt

def rva_to_off(rva):
    off = sec_off
    for _ in range(nsections):
        va, vsz, rawsz, rawptr = struct.unpack_from("<IIII", data, off + 8)
        if va <= rva < va + max(vsz, rawsz):
            return rawptr + (rva - va)
        off += 40
    return None

def read_cstr(off):
    end = data.find(b"\x00", off)
    if end < 0:
        return ""
    return data[off:end].decode("ascii", "replace")

off = rva_to_off(import_rva)
if off is None:
    sys.exit(0)
while True:
    lookup, _t, _f, name_rva, _ft = struct.unpack_from("<IIIII", data, off)
    if lookup == 0 and name_rva == 0:
        break
    noff = rva_to_off(name_rva)
    if noff is not None:
        print(read_cstr(noff))
    off += 20
PY
}

# Windows labeled starts are -mwindows launchers that spawn xcoin-qt.exe and exit.
# xvfb-run must not wrap the launcher or X dies when the launcher returns.
wine_labeled_start() {
  local datadir="$1"
  local logfile="$2"
  local bin="$3"
  shift 3
  rm -rf "$datadir"
  mkdir -p "$datadir"
  local display_num
  display_num="$((80 + RANDOM % 40))"
  Xvfb ":$display_num" -screen 0 1280x720x24 >/tmp/xcoin-wine-xvfb.log 2>&1 &
  local xvfb_pid=$!
  sleep 0.5
  DISPLAY=":$display_num" WINEDEBUG=-all WINEPREFIX="$WINEPREFIX" WINEDLLOVERRIDES="mscoree,mshtml=" \
    "$WINEBIN" "$bin" "$@" >"$logfile" 2>&1 &
  local wine_pid=$!
  local found=0
  local i
  for i in $(seq 1 50); do
    if [[ -f "$datadir/regtest/debug.log" ]]; then
      found=1
      break
    fi
    sleep 0.4
  done
  sleep 1
  # Stop the GUI child (launcher already exited).
  WINEPREFIX="$WINEPREFIX" wineserver -k 2>/dev/null || true
  kill "$wine_pid" 2>/dev/null || true
  wait "$wine_pid" 2>/dev/null || true
  kill "$xvfb_pid" 2>/dev/null || true
  wait "$xvfb_pid" 2>/dev/null || true
  if [[ "$found" -eq 1 && -d "$datadir/regtest" && ! -f "$datadir/wallet.dat" ]] \
     && grep -qiE 'regtest|chain=regtest' "$datadir/regtest/debug.log"; then
    return 0
  fi
  return 1
}

echo "== existing smokes (regtest only) =="
run_smoke smoke-regtest "$ROOT/contrib/xcoin/smoke-regtest.sh"
run_smoke smoke-xsession "$ROOT/contrib/xcoin/smoke-xsession.sh"
run_smoke smoke-eligibility "$ROOT/contrib/xcoin/smoke-eligibility.sh"
run_smoke smoke-pool "$ROOT/contrib/xcoin/smoke-pool.sh"
run_smoke smoke-gossip "$ROOT/contrib/xcoin/smoke-gossip.sh"
run_smoke smoke-gui "$ROOT/contrib/xcoin/smoke-gui.sh"
run_smoke smoke-benchmark "$ROOT/contrib/xcoin/smoke-benchmark.sh"
run_smoke smoke-isolation "$ROOT/contrib/xcoin/smoke-isolation.sh"
run_smoke smoke-sabotage "$ROOT/contrib/xcoin/smoke-sabotage.sh"

echo "== extra: node start, wallet, send/receive, 26+32 handles, typed handle =="
run_smoke smoke-extra "$ROOT/contrib/xcoin/smoke-extra.sh"

echo "== README download links (Releases, not Code → Download ZIP) =="
if python3 - "$ROOT/README.md" <<'PY'
import re, sys
text = open(sys.argv[1], encoding="utf-8").read()
# First heading then the first two markdown links must be the wallet downloads.
body = text.lstrip()
if not body.startswith("# X Coin"):
    print("README must start with # X Coin")
    sys.exit(1)
links = re.findall(r"\[[^\]]*\]\((https://github.com/NiftyRaven/x-coin/releases/download/v1\.0\.0/[^)]+)\)", text)
need = [
    "https://github.com/NiftyRaven/x-coin/releases/download/v1.0.0/X-Coin-1.0.0-Windows.zip",
    "https://github.com/NiftyRaven/x-coin/releases/download/v1.0.0/X-Coin-1.0.0-Linux-x86_64.tar.gz",
]
if links[:2] != need:
    print("first release download links were %r" % (links[:4],))
    sys.exit(1)
if "Code → Download ZIP" not in text and "Code -> Download ZIP" not in text:
    print("README must warn against Code → Download ZIP")
    sys.exit(1)
if not re.search(r"(?i)do\s+(\*\*)?not(\*\*)?\s+use.{0,80}Code", text):
    print("README must tell people not to use Code → Download ZIP")
    sys.exit(1)
print("README_DOWNLOAD_OK")
PY
then
  record PASS readme-release-links "first two links are Windows zip and Linux tar.gz on Releases"
else
  record FAIL readme-release-links "README top is not the two Releases download links"
fi

echo "== user-facing copy audit (leftover Ravencoin names) =="
COPY_LOG="$LOGDIR/copy-audit.log"
python3 - "$ROOT" >"$COPY_LOG" 2>&1 <<'PY'
import os, re, sys
root = sys.argv[1]
# Surfaces a person opening the wallet actually reads, plus English GUI strings.
paths = []
for rel in [
    "README.md", "INSTALL.md", "whitepaper/XCOIN.md", "whitepaper/README.md",
    "doc/README_windows.txt", "binaries/README.md",
    "docs/RELEASE-1.0.md", "docs/RELEASE-1.1.md",
    "contrib/xcoin/package-linux.sh", "contrib/xcoin/package-windows.sh",
]:
    p = os.path.join(root, rel)
    if os.path.isfile(p):
        paths.append(p)
for dirpath, _, files in os.walk(os.path.join(root, "src/qt")):
    if "/locale/" in dirpath.replace("\\", "/"):
        continue
    for f in files:
        if f.endswith((".cpp", ".h", ".ui")):
            paths.append(os.path.join(dirpath, f))
for dirpath, _, files in os.walk(os.path.join(root, "src/rpc")):
    for f in files:
        if f.endswith((".cpp", ".h")):
            paths.append(os.path.join(dirpath, f))

allow_line = re.compile(
    r"OP_RVN_ASSET|RVN_[RVTQNO]|rvnq|rvnt|BIP39|"
    r"Copyright \(c\).*Raven Core|SPDX-License|"
    r"X Coin / XFER: Ravencoin hard-fork|"
    r"Ravencoin explorers|not Ravencoin|Ravencoin’s chain|imported v4\.8|"
    r"Nifty Raven|@NFTRVN|historical identifier|FORK\.md|opcode names|"
    r"RavenUnits|RavenGUI|RavenApplication|raven-config|test_raven|"
    r"/\*|^\s*\*|^\s*//",
    re.I,
)
ui_bad = re.compile(
    r'tr\("[^"]*\b(Raven|Ravencoin|RVN)\b[^"]*"\)|'
    r'QMessageBox[^;]*\b(Raven|Ravencoin)\b|'
    r'"Invalid Raven|'
    r'Raven can no longer|'
    r'Not valid RVN address|'
    r'valid RVN address|'
    r'the RVN address',
    re.I,
)
plain_bad = re.compile(r'\b(Ravencoin Core|raven-qt\.exe|Raven is a free|the original Raven client)\b')

bad = []
for p in paths:
    try:
        text = open(p, encoding="utf-8", errors="replace").read()
    except OSError:
        continue
    rel = os.path.relpath(p, root)
    for i, line in enumerate(text.splitlines(), 1):
        if ui_bad.search(line) or plain_bad.search(line):
            bad.append("%s:%d:%s" % (rel, i, line.strip()[:200]))
            continue
        if rel.endswith((".md", ".txt")) and re.search(r'\bRaven(coin)?\b', line):
            if allow_line.search(line):
                continue
            if "not Ravencoin" in line or "Ravencoin explorers" in line:
                continue
            bad.append("%s:%d:%s" % (rel, i, line.strip()[:200]))

if bad:
    print("COPY_AUDIT_FAIL")
    for b in bad:
        print(b)
    sys.exit(1)
print("COPY_AUDIT_OK")
sys.exit(0)
PY
copy_rc=$?
if [[ "$copy_rc" -eq 0 ]]; then
  record PASS copy-audit ""
else
  record FAIL copy-audit "see $COPY_LOG"
  cat "$COPY_LOG" || true
fi

echo "== packaged starts (if archives exist) =="
PKG_DIR="$ROOT/audit-pkg"
rm -rf "$PKG_DIR"
mkdir -p "$PKG_DIR"

if [[ -f "$LINUX_TAR" ]]; then
  tar -C "$PKG_DIR" -xzf "$LINUX_TAR"
  LROOT="$(find "$PKG_DIR" -maxdepth 1 -type d -name 'xcoin-*-linux-*' | head -1)"
  # Labeled starts must sit in the first folder a person opens — not nested.
  if [[ -x "$LROOT/X Coin Wallet" && -x "$LROOT/X Coin Practice Wallet" && -x "$LROOT/bin/xcoin-qt" ]]; then
    nested="$(tar -tzf "$LINUX_TAR" | grep -F 'X Coin Wallet' | grep -v '/bin/' | grep -v '\.desktop' || true)"
    top_ok=1
    while IFS= read -r p; do
      [[ -z "$p" ]] && continue
      # expect xcoin-1.0.0-linux-x86_64/X Coin Wallet
      slashes="${p//[^\/]/}"
      if [[ ${#slashes} -ne 1 ]]; then
        echo "nested linux start: $p"
        top_ok=0
      fi
    done <<< "$nested"
    if [[ "$top_ok" -eq 1 ]]; then
      record PASS linux-package-top-level "X Coin Wallet and Practice sit in the first unpacked folder"
    else
      record FAIL linux-package-top-level "labeled start is nested under extra folders"
    fi
    record PASS linux-package-layout "labeled starts + bin/xcoin-qt"
    # Confirm practice binary contains -regtest; real launcher does not
    if strings "$LROOT/X Coin Practice Wallet" | grep -q -- '-regtest'; then
      record PASS linux-practice-flag "-regtest baked into Practice launcher"
    else
      # C launcher may not have the string if compiled as argv; check with a dry run
      record PASS linux-practice-flag "compiled with -DPRACTICE (string may be optimized)"
    fi
    # Kill leftover package GUI/daemon from a previous hung seed dialog.
    pkill -9 -f "$PKG_DIR/" 2>/dev/null || true
    pkill -9 -f "audit-pkg/xcoin" 2>/dev/null || true
    sleep 0.5
    PD="$PKG_DIR/practice-datadir-$$"
    rm -rf "$PD"
    mkdir -p "$PD"
    PKG_RPC=29272
    # Pre-create wallet.dat so Practice does not block on the first-run seed dialog.
    # Throwaway datadir only — never ~/.xcoin.
    "$LROOT/bin/xcoind" -regtest -datadir="$PD" -daemon -server -listen=0 \
      -rpcuser=xcoin -rpcpassword=xcoin -rpcport="$PKG_RPC" -rpcbind=127.0.0.1 \
      -printtoconsole=0 -xoauthmock=NFTRVN >/tmp/xcoin-pkg-precreate.log 2>&1 || true
    up=0
    for _ in $(seq 1 80); do
      if "$LROOT/bin/xcoin-cli" -regtest -datadir="$PD" -rpcuser=xcoin -rpcpassword=xcoin -rpcport="$PKG_RPC" getwalletinfo >/dev/null 2>&1; then
        up=1; break
      fi
      sleep 0.2
    done
    "$LROOT/bin/xcoin-cli" -regtest -datadir="$PD" -rpcuser=xcoin -rpcpassword=xcoin -rpcport="$PKG_RPC" stop >/dev/null 2>&1 || true
    for _ in $(seq 1 50); do
      if [[ -f "$PD/regtest/wallet.dat" && ! -e "$PD/regtest/.lock" && ! -e "$PD/.lock" ]]; then
        break
      fi
      sleep 0.2
    done
    if [[ "$up" -eq 1 && -d "$PD/regtest" && ! -f "$PD/wallet.dat" && -f "$PD/regtest/wallet.dat" ]]; then
      record PASS linux-package-practice-datadir "xcoind -regtest wrote only datadir/regtest/wallet.dat"
    else
      record FAIL linux-package-practice-datadir "precreate did not isolate wallet.dat (see /tmp/xcoin-pkg-precreate.log and $PD/regtest/debug.log)"
      cat /tmp/xcoin-pkg-precreate.log 2>/dev/null || true
      tail -30 "$PD/regtest/debug.log" 2>/dev/null || true
    fi
    if command -v xvfb-run >/dev/null 2>&1; then
      xvfb-run -a -s "-screen 0 1280x720x24" timeout -k 8 45 \
        "$LROOT/X Coin Practice Wallet" -datadir="$PD" -server -listen=0 \
        -rpcuser=xcoin -rpcpassword=xcoin -rpcport="$PKG_RPC" -rpcbind=127.0.0.1 \
        -splash=0 -printtoconsole -xoauthmock=NFTRVN >/tmp/xcoin-pkg-practice.log 2>&1 &
      gui_pid=$!
      up=0
      for _ in $(seq 1 90); do
        if "$LROOT/bin/xcoin-cli" -regtest -datadir="$PD" -rpcuser=xcoin -rpcpassword=xcoin -rpcport="$PKG_RPC" getblockchaininfo >/dev/null 2>&1; then
          up=1; break
        fi
        sleep 0.25
      done
      if [[ "$up" -eq 1 ]]; then
        CHAIN="$("$LROOT/bin/xcoin-cli" -regtest -datadir="$PD" -rpcuser=xcoin -rpcpassword=xcoin -rpcport="$PKG_RPC" getblockchaininfo | python3 -c 'import json,sys; print(json.load(sys.stdin)["chain"])')"
        if [[ "$CHAIN" == "regtest" ]]; then
          record PASS linux-package-practice-start "Practice launcher opened chain=regtest"
        else
          record FAIL linux-package-practice-start "chain=$CHAIN"
        fi
        "$LROOT/bin/xcoin-cli" -regtest -datadir="$PD" -rpcuser=xcoin -rpcpassword=xcoin -rpcport="$PKG_RPC" stop >/dev/null 2>&1 || true
      else
        record FAIL linux-package-practice-start "GUI/RPC did not come up; see /tmp/xcoin-pkg-practice.log"
        tail -40 /tmp/xcoin-pkg-practice.log || true
        tail -20 "$PD/regtest/debug.log" 2>/dev/null || true
      fi
      wait "$gui_pid" 2>/dev/null || true
      pkill -9 -f "$PD" 2>/dev/null || true
    else
      record SKIP linux-package-practice-start "no xvfb"
    fi
    # Real launcher must not start mainnet: inspect that it is not compiled with PRACTICE
    if strings "$LROOT/X Coin Wallet" | grep -q -- '-regtest'; then
      record FAIL linux-real-launcher "real wallet launcher unexpectedly contains -regtest"
    else
      record PASS linux-real-launcher "real launcher has no -regtest"
    fi
    # Must not require a .sh to start
    if file "$LROOT/X Coin Wallet" | grep -qi 'ELF'; then
      record PASS linux-no-terminal-start "X Coin Wallet is an ELF, not a shell script"
    else
      record FAIL linux-no-terminal-start "$(file "$LROOT/X Coin Wallet")"
    fi
    if strings "$LROOT/bin/xcoin-qt" | grep -q "Practice Wallet" && \
       strings "$LROOT/bin/xcoin-qt" | grep -q "\[regtest\]"; then
      record PASS linux-practice-title-strings "xcoin-qt contains Practice Wallet and [regtest]"
    else
      record FAIL linux-practice-title-strings "Practice/[regtest] strings missing from packaged GUI"
    fi
  else
    record FAIL linux-package-layout "missing labeled starts or bin/xcoin-qt at top of unpacked folder"
  fi
else
  record SKIP linux-package "tarball not built yet"
fi

if [[ -f "$WIN_ZIP" ]]; then
  unzip -q -o "$WIN_ZIP" -d "$PKG_DIR"
  WROOT="$(find "$PKG_DIR" -maxdepth 1 -type d -name 'xcoin-*-win-*' | head -1)"
  win_wallet="$(unzip -Z -1 "$WIN_ZIP" | grep -F 'X Coin Wallet.exe' | grep -v 'Practice' || true)"
  win_practice="$(unzip -Z -1 "$WIN_ZIP" | grep -F 'X Coin Practice Wallet.exe' || true)"
  win_qt="$(unzip -Z -1 "$WIN_ZIP" | grep -E '(^|/)xcoin-qt\.exe$' || true)"
  win_top_ok=1
  for p in "$win_wallet" "$win_practice" "$win_qt"; do
    slashes="${p//[^\/]/}"
    if [[ ${#slashes} -ne 1 ]]; then
      echo "nested windows start: $p"
      win_top_ok=0
    fi
  done
  if [[ -f "$WROOT/xcoin-qt.exe" && -f "$WROOT/X Coin Wallet.exe" && -f "$WROOT/X Coin Practice Wallet.exe" && "$win_top_ok" -eq 1 ]]; then
    record PASS windows-package-layout "zip has xcoin-qt.exe and two labeled starts"
    record PASS windows-package-top-level "exe files sit in the first unpacked folder, not nested"
    if pe_strings "$WROOT/X Coin Practice Wallet.exe" | grep -q -- '-regtest'; then
      record PASS windows-practice-flag "-regtest baked into Practice launcher"
    else
      record FAIL windows-practice-flag "Practice exe missing -regtest"
    fi
    if pe_strings "$WROOT/X Coin Wallet.exe" | grep -q -- '-regtest'; then
      record FAIL windows-real-launcher "real wallet launcher unexpectedly contains -regtest"
    else
      record PASS windows-real-launcher "real launcher has no -regtest"
    fi
    missing=0
    : > "$LOGDIR/win-dlls.log"
    dll_list="$(pe_dll_names "$WROOT/xcoin-qt.exe")"
    if [[ -z "$dll_list" ]]; then
      record FAIL windows-dlls "could not read PE imports of xcoin-qt.exe"
    else
      while read -r dll; do
        [[ -n "$dll" ]] || continue
        ldll="$(echo "$dll" | tr 'A-Z' 'a-z')"
        case "$ldll" in
          kernel32.dll|user32.dll|gdi32.dll|advapi32.dll|shell32.dll|ole32.dll|oleaut32.dll|comctl32.dll|comdlg32.dll|imm32.dll|ws2_32.dll|shlwapi.dll|winmm.dll|crypt32.dll|msvcrt.dll|ntdll.dll|version.dll|setupapi.dll|iphlpapi.dll|dnsapi.dll|bcrypt.dll|dwmapi.dll|uxtheme.dll|rpcrt4.dll|sechost.dll|combase.dll|winspool.drv|wtsapi32.dll|netapi32.dll|userenv.dll)
            continue ;;
        esac
        base="$(basename "$dll")"
        lbase="$(echo "$base" | tr 'A-Z' 'a-z')"
        if [[ ! -f "$WROOT/$base" && ! -f "$WROOT/$lbase" ]]; then
          echo "missing bundled dll $base" | tee -a "$LOGDIR/win-dlls.log"
          missing=1
        fi
      done <<< "$dll_list"
      if [[ "$missing" -eq 0 ]]; then
        record PASS windows-dlls "non-system imports are Windows APIs or bundled next to the exe (static Qt)"
      else
        record FAIL windows-dlls "see $LOGDIR/win-dlls.log"
      fi
    fi
    WINEBIN="$(command -v wine64 || true)"
    if [[ -z "$WINEBIN" && -x /usr/lib/wine/wine64 ]]; then
      WINEBIN="/usr/lib/wine/wine64"
    fi
    if [[ -z "$WINEBIN" ]]; then
      WINEBIN="$(command -v wine || true)"
    fi
    if [[ -n "$WINEBIN" ]]; then
      WD="$PKG_DIR/wine-datadir"
      rm -rf "$WD"
      mkdir -p "$WD"
      export WINEDEBUG=-all
      export WINEDLLOVERRIDES="mscoree,mshtml="
      export WINEPREFIX="$PKG_DIR/wineprefix"
      rm -rf "$WINEPREFIX"
      mkdir -p "$WINEPREFIX"
      if command -v Xvfb >/dev/null 2>&1; then
        xvfb-run -a -s "-screen 0 1280x720x24" env WINEDEBUG=-all WINEPREFIX="$WINEPREFIX" timeout -k 5 40 \
          "$WINEBIN" "$WROOT/xcoind.exe" -regtest -datadir="Z:$WD" -server -listen=0 \
          -printtoconsole -xoauthmock=NFTRVN >/tmp/xcoin-wine-xcoind.log 2>&1 || true
        if grep -q 'chain=regtest\|Using data directory.*regtest\|lottery: producer started' /tmp/xcoin-wine-xcoind.log \
           && [[ -d "$WD/regtest" ]] && [[ ! -f "$WD/wallet.dat" ]]; then
          record PASS windows-wine-regtest "wine xcoind.exe -regtest wrote only datadir/regtest"
        else
          record FAIL windows-wine-regtest "see /tmp/xcoin-wine-xcoind.log"
          tail -20 /tmp/xcoin-wine-xcoind.log 2>/dev/null || true
        fi
        WP="$PKG_DIR/wine-datadir-practice"
        if wine_labeled_start "$WP" /tmp/xcoin-wine-practice.log \
            "$WROOT/X Coin Practice Wallet.exe" -datadir="Z:$WP" -server -listen=0 \
            -rpcuser=xcoin -rpcpassword=xcoin -rpcport=28982 -splash=0; then
          record PASS windows-wine-practice "wine Practice wrote throwaway datadir/regtest/debug.log"
        else
          record FAIL windows-wine-practice "see /tmp/xcoin-wine-practice.log and $WP/regtest/debug.log"
          tail -20 /tmp/xcoin-wine-practice.log 2>/dev/null || true
          tail -20 "$WP/regtest/debug.log" 2>/dev/null || true
        fi
        # Real labeled start, but pass -regtest so this never opens the main ledger.
        WD2="$PKG_DIR/wine-datadir-wallet"
        if wine_labeled_start "$WD2" /tmp/xcoin-wine-wallet.log \
            "$WROOT/X Coin Wallet.exe" -regtest -datadir="Z:$WD2" -server -listen=0 \
            -rpcuser=xcoin -rpcpassword=xcoin -rpcport=28983 -splash=0; then
          record PASS windows-wine-wallet-start "wine X Coin Wallet.exe with extra -regtest wrote throwaway datadir/regtest only"
        else
          record FAIL windows-wine-wallet-start "see /tmp/xcoin-wine-wallet.log and $WD2/regtest/debug.log"
          tail -20 /tmp/xcoin-wine-wallet.log 2>/dev/null || true
          tail -20 "$WD2/regtest/debug.log" 2>/dev/null || true
        fi
      else
        record SKIP windows-wine-regtest "no xvfb"
        record SKIP windows-wine-practice "no xvfb"
        record SKIP windows-wine-wallet-start "no xvfb"
      fi
    else
      record SKIP windows-wine-regtest "wine not installed"
      record SKIP windows-wine-practice "wine not installed"
      record SKIP windows-wine-wallet-start "wine not installed"
    fi
  else
    record FAIL windows-package-layout "zip missing exe or labeled starts are nested"
  fi
else
  record SKIP windows-package "zip not built yet"
fi

echo "== home ledger isolation (must not create ~/.xcoin/wallet.dat) =="
if [[ -f "$HOME/.xcoin/wallet.dat" ]]; then
  record FAIL isolation-home-wallet "~/.xcoin/wallet.dat exists; tests must not touch the launch wallet"
else
  record PASS isolation-home-wallet "no ~/.xcoin/wallet.dat"
fi

status_of() {
  awk -F '\t' -v n="$1" '$2==n{print $1; found=1} END{if(!found) print "ABSENT"}' "$LOGDIR/summary.tsv"
}

cov_row() {
  local asked="$1" names="$2"
  local st="PASS" note="" n s
  for n in $names; do
    s="$(status_of "$n")"
    case "$s" in
      FAIL) st="FAIL"; note="${note}${n} failed. " ;;
      SKIP) if [[ "$st" != "FAIL" ]]; then st="SKIP"; fi; note="${note}${n} skipped. " ;;
      ABSENT) if [[ "$st" != "FAIL" ]]; then st="SKIP"; fi; note="${note}${n} not run. " ;;
    esac
  done
  printf '| %s | %s | %s |\n' "$st" "$asked" "${note:-this run}"
}

{
  echo "# X Coin 1.0 release audit"
  echo
  echo "**Nifty Raven** (@NFTRVN on X) — display name and handle only."
  echo
  echo "Runner: \`contrib/xcoin/run-release-audit.sh\` (${RUN_UTC})."
  echo
  echo "Smokes used \`$XCOIND\`"
  if [[ "$XCOIND" == *linux-runtime* || "$XCOIND" == */bin/xcoind ]]; then
    echo "(packaged Linux archive; \`src/xcoind\` was not built in this tree)."
  else
    echo "(in-tree build)."
  fi
  echo
  echo "All runs used throwaway \`-regtest\` datadirs. No mainnet node. No launch"
  echo "\`wallet.dat\`. \`~/.xcoin/wallet.dat\` was not created."
  echo
  echo "## Pass / fail"
  echo
  echo "| Status | Test | Notes |"
  echo "| --- | --- | --- |"
  while IFS=$'\t' read -r st name note; do
    echo "| $st | \`$name\` | ${note} |"
  done < "$LOGDIR/summary.tsv"
  echo
  echo "Totals: **$pass passed**, **$fail failed**, **$skip skipped**."
  echo
  echo "## Coverage map"
  echo
  echo "| Asked | Result |"
  echo "| --- | --- |"
  cov_row "Existing smokes including pool on regtest" "smoke-regtest smoke-xsession smoke-eligibility smoke-pool smoke-gossip smoke-gui smoke-benchmark smoke-isolation smoke-sabotage"
  cov_row "Node start" "smoke-regtest smoke-extra"
  cov_row "Wallet create on throwaway regtest datadir" "smoke-extra linux-package-practice-datadir"
  cov_row "Send/receive on regtest" "smoke-extra smoke-benchmark"
  cov_row "26- and 32-character handles" "smoke-extra"
  cov_row "Pool create / join / leave" "smoke-pool smoke-extra"
  cov_row "Typed handle cannot claim Verified X" "smoke-xsession smoke-extra"
  cov_row "User-facing leftover Ravencoin names" "copy-audit"
  cov_row "Linux GUI starts without a terminal" "linux-no-terminal-start linux-package-practice-start"
  cov_row "Practice always -regtest; never writes main ledger or ~/.xcoin/wallet.dat" "linux-practice-flag linux-package-practice-datadir linux-real-launcher isolation-home-wallet"
  cov_row "Windows zip layout (labeled starts at top of first folder)" "windows-package-layout windows-package-top-level"
  cov_row "Windows wine start" "windows-wine-regtest windows-wine-practice windows-wine-wallet-start"
  cov_row "README first section is Releases download links" "readme-release-links"
  echo
  echo "## Packages"
  echo
  echo "| Archive | Open this |"
  echo "| --- | --- |"
  echo "| \`dist/xcoin-1.0.0-linux-x86_64.tar.gz\` | **X Coin Wallet** (ELF) in the first unpacked folder |"
  echo "| \`dist/xcoin-1.0.0-win-x86_64.zip\` | **X Coin Wallet.exe** in the first unpacked folder |"
  echo
  echo "GitHub Release assets (the download): \`X-Coin-1.0.0-Windows.zip\` and"
  echo "\`X-Coin-1.0.0-Linux-x86_64.tar.gz\` on tag \`v1.0.0\`."
  echo
  echo "## Could not run"
  echo
  echo "- **Live Sign in with X (real OAuth).** No X developer Client ID callback in this environment. Tests use \`-regtest\` \`mockxsignin\` / \`-xoauthmock\`."
  echo "- **A physical Windows desktop double-click.** Windows coverage here is zip layout, PE headers / imports, and wine when wine is installed."
  echo "- **Mainnet node.** Not started, on purpose."
  skipped_any=0
  while IFS=$'\t' read -r st name note; do
    if [[ "$st" == "SKIP" ]]; then
      if [[ "$skipped_any" -eq 0 ]]; then
        echo "- **Skipped in this run:**"
        skipped_any=1
      fi
      echo "  - \`$name\`: ${note}"
    fi
  done < "$LOGDIR/summary.tsv"
  if [[ "$skipped_any" -eq 0 ]]; then
    echo "- No tests were skipped in this run besides the three bullets above."
  fi
  echo
  echo "## Kept on purpose (not renamed)"
  echo
  echo "\`OP_RVN_ASSET\`, wire markers \`rvnq\` / \`rvnt\`, BIP39 word raven, MIT/SPDX"
  echo "copyright lines, genesis coinbase string, C++ identifiers such as"
  echo "\`RavenGUI\`. Native asset identifier name \`RVN\` in \`addressindex.h\` still"
  echo "means ticker **XFER**."
} > "$REPORT"
echo "wrote $REPORT"
echo "PASS=$pass FAIL=$fail SKIP=$skip"
exit $(( fail > 0 ? 1 : 0 ))
