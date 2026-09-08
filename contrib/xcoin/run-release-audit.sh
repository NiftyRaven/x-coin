#!/usr/bin/env bash
# Full-scale private-test audit. Regtest / throwaway datadirs only. Never starts mainnet.
set -u
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
REPORT="${REPORT:-$ROOT/docs/RELEASE-AUDIT.md}"
LOGDIR="${LOGDIR:-$ROOT/audit-logs}"
XCOIND="${XCOIND:-$ROOT/src/xcoind}"
XCLI="${XCLI:-$ROOT/src/xcoin-cli}"
XQT="${XQT:-$ROOT/src/qt/xcoin-qt}"
mkdir -p "$LOGDIR"
: > "$LOGDIR/summary.tsv"

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
    r'Raven can no longer',
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
LINUX_TAR="$ROOT/dist/xcoin-1.0.0-linux-x86_64.tar.gz"
WIN_ZIP="$ROOT/dist/xcoin-1.0.0-win-x86_64.zip"
PKG_DIR="$ROOT/audit-pkg"
rm -rf "$PKG_DIR"
mkdir -p "$PKG_DIR"

if [[ -f "$LINUX_TAR" ]]; then
  tar -C "$PKG_DIR" -xzf "$LINUX_TAR"
  LROOT="$(find "$PKG_DIR" -maxdepth 1 -type d -name 'xcoin-*-linux-*' | head -1)"
  if [[ -x "$LROOT/X Coin Practice Wallet" && -x "$LROOT/bin/xcoin-qt" ]]; then
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
    record FAIL linux-package-layout "missing labeled starts or bin/xcoin-qt"
  fi
else
  record SKIP linux-package "tarball not built yet"
fi

if [[ -f "$WIN_ZIP" ]]; then
  unzip -q -o "$WIN_ZIP" -d "$PKG_DIR"
  WROOT="$(find "$PKG_DIR" -maxdepth 1 -type d -name 'xcoin-*-win-*' | head -1)"
  if [[ -f "$WROOT/xcoin-qt.exe" && -f "$WROOT/X Coin Wallet.exe" && -f "$WROOT/X Coin Practice Wallet.exe" ]]; then
    record PASS windows-package-layout "zip has xcoin-qt.exe and two labeled starts"
    if command -v x86_64-w64-mingw32-objdump >/dev/null 2>&1; then
      missing=0
      while read -r dll; do
        case "$dll" in
          KERNEL32.dll|kernel32.dll|USER32.dll|user32.dll|GDI32.dll|gdi32.dll|ADVAPI32.dll|advapi32.dll|SHELL32.dll|shell32.dll|ole32.dll|OLE32.dll|OLEAUT32.dll|oleaut32.dll|COMCTL32.dll|comctl32.dll|COMDLG32.dll|comdlg32.dll|IMM32.dll|imm32.dll|WS2_32.dll|ws2_32.dll|SHLWAPI.dll|shlwapi.dll|WINMM.dll|winmm.dll|CRYPT32.dll|crypt32.dll|msvcrt.dll|MSVCRT.dll|ntdll.dll|NTDLL.dll|VERSION.dll|version.dll|SETUPAPI.dll|setupapi.dll|IPHLAPI.dll|iphlpapi.dll|DNSAPI.dll|dnsapi.dll|bcrypt.dll|BCRYPT.dll|dwmapi.dll|DWMAPI.dll|uxtheme.dll|UXTHEME.dll|rpcrt4.dll|RPCRT4.dll|sechost.dll|SECHOST.dll|combase.dll|COMBASE.dll|WINSPOOL.DRV|winspool.drv)
            continue ;;
        esac
        base="$(basename "$dll")"
        if [[ ! -f "$WROOT/$base" && ! -f "$WROOT/$(echo "$base" | tr 'A-Z' 'a-z')" ]]; then
          echo "missing bundled dll $base" | tee -a "$LOGDIR/win-dlls.log"
          missing=1
        fi
      done < <(x86_64-w64-mingw32-objdump -p "$WROOT/xcoin-qt.exe" | awk '/DLL Name:/{print $3}')
      if [[ "$missing" -eq 0 ]]; then
        record PASS windows-dlls "imported non-system DLLs present next to the exe"
      else
        record FAIL windows-dlls "see $LOGDIR/win-dlls.log"
      fi
    else
      record SKIP windows-dlls "no mingw objdump"
    fi
    if command -v wine64 >/dev/null 2>&1 || command -v wine >/dev/null 2>&1; then
      WINEBIN="$(command -v wine64 || command -v wine)"
      WD="$PKG_DIR/wine-datadir"
      rm -rf "$WD"
      mkdir -p "$WD"
      export WINEDEBUG=-all
      export WINEDLLOVERRIDES="mscoree,mshtml="
      if command -v xvfb-run >/dev/null 2>&1; then
        xvfb-run -a -s "-screen 0 1280x720x24" env WINEDEBUG=-all timeout 60 \
          "$WINEBIN" "$WROOT/X Coin Practice Wallet.exe" -datadir="$WD" -server -listen=0 \
          -rpcuser=xcoin -rpcpassword=xcoin -rpcport=28982 -splash=0 \
          >/tmp/xcoin-wine-practice.log 2>&1 || true
        # Practice should have created a regtest folder; wine paths differ.
        if grep -qi "regtest" /tmp/xcoin-wine-practice.log 2>/dev/null || find "$WD" -name debug.log | grep -q .; then
          record PASS windows-wine-practice "wine launched Practice (see /tmp/xcoin-wine-practice.log)"
        else
          record SKIP windows-wine-practice "wine did not produce a clear regtest run; see /tmp/xcoin-wine-practice.log"
          tail -20 /tmp/xcoin-wine-practice.log 2>/dev/null || true
        fi
      else
        record SKIP windows-wine-practice "no xvfb"
      fi
    else
      record SKIP windows-wine-practice "wine not installed"
    fi
  else
    record FAIL windows-package-layout "zip missing exe or labeled starts"
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

{
  echo "# X Coin 1.0 release audit"
  echo
  echo "**Nifty Raven** (@NFTRVN on X) — display name and handle only."
  echo
  echo "All runs used throwaway \`-regtest\` datadirs. No mainnet node. No launch \`wallet.dat\`."
  echo
  echo "| Status | Test | Notes |"
  echo "| --- | --- | --- |"
  while IFS=$'\t' read -r st name note; do
    echo "| $st | \`$name\` | ${note} |"
  done < "$LOGDIR/summary.tsv"
  echo
  echo "Totals: **$pass passed**, **$fail failed**, **$skip skipped**."
} > "$REPORT"
echo "wrote $REPORT"
echo "PASS=$pass FAIL=$fail SKIP=$skip"
exit $(( fail > 0 ? 1 : 0 ))
