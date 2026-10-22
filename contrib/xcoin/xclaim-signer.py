#!/usr/bin/env python3
# Emergency XPF1 signer. Standby key id 2 on the existing seed VM.
# Does not start or stop xcoind. Decrypts an openssl ciphertext into memory
# only. Never prints the private key.
#
# One stdout line per request (journald reads that stream under systemd):
#   XCLAIM: <time> route=<path> handle=<h> address=<a> result=ok|error error=<code> status=<n> durationMs=<ms>
# Never put tokens, cookies, OAuth codes, signatures, passphrases, or key
# material on that line. Query strings are dropped before the route is logged.
#
#   openssl enc -aes-256-cbc -pbkdf2 -iter 600000 -in key.hex -out key.hex.enc
#
# Default bind is 127.0.0.1. Use --bind 0.0.0.0 only while the live signer
# is down, then close port 38445.

import argparse
import getpass
import hashlib
import json
import os
import re
import struct
import subprocess
import sys
import threading
import time
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from urllib.request import Request, urlopen

TAG = b"xcoin-xpf-v1"
PROOF_VERSION = 1
MAX_FUTURE = 3600
EXPIRY_TTL = 900


def digest(handle, user_id, hash160, root, expiry, key_id):
    if isinstance(hash160, str):
        hash160 = bytes.fromhex(hash160)
    payload = (
        TAG
        + bytes([len(handle)])
        + handle.encode()
        + struct.pack("<Q", int(user_id))
        + hash160
        + bytes([len(root)])
        + root.encode()
        + struct.pack("<I", int(expiry))
        + bytes([int(key_id) & 0xFF])
    )
    return hashlib.sha256(hashlib.sha256(payload).digest()).digest()


def self_check(path):
    with open(path, "r", encoding="utf-8") as f:
        vec = json.load(f)
    got = digest(vec["handle"], vec["userId"], vec["hash160"], vec["root"], vec["expiry"], vec["keyId"]).hex()
    if got != vec["digest"]:
        sys.stderr.write("digest mismatch\n")
        return 1
    sys.stdout.write("xpf1 vector ok\n")
    return 0


def load_key(path, allow_plaintext):
    raw = open(path, "rb").read()
    if raw.startswith(b"Salted__"):
        password = os.environ.get("XCLAIM_KEY_PASSPHRASE")
        if not password:
            password = getpass.getpass("standby key password: ")
        proc = subprocess.run(
            [
                "openssl", "enc", "-d", "-aes-256-cbc", "-pbkdf2", "-iter", "600000",
                "-pass", "env:XCLAIM_KEY_PASSPHRASE",
            ],
            input=raw,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            env={**os.environ, "XCLAIM_KEY_PASSPHRASE": password},
        )
        password = None
        if proc.returncode != 0:
            sys.stderr.write("decrypt failed\n")
            return None
        text = proc.stdout.decode().strip()
    else:
        if not allow_plaintext:
            sys.stderr.write("refusing plaintext key (pass --allow-plaintext only for a local test)\n")
            return None
        text = raw.decode().strip()
    if len(text) == 66 and text.startswith("00"):
        text = text[2:]
    if len(text) != 64:
        sys.stderr.write("key must be 64 hex chars\n")
        return None
    try:
        key = bytes.fromhex(text)
    except ValueError:
        sys.stderr.write("key must be hex\n")
        return None
    text = None
    return key


def sign_compact(key32, digest32):
    from coincurve import PrivateKey
    pk = PrivateKey(key32)
    sig = pk.sign_recoverable(digest32, hasher=None)
    recid = sig[64]
    header = 27 + recid + 4
    return bytes([header]) + sig[:64]


def b58_hash160(address):
    alphabet = "123456789ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnopqrstuvwxyz"
    n = 0
    for c in address:
        n = n * 58 + alphabet.index(c)
    raw = n.to_bytes(25, "big")
    chk = hashlib.sha256(hashlib.sha256(raw[:-4]).digest()).digest()[:4]
    if chk != raw[-4:]:
        raise ValueError("address checksum")
    return raw[1:21]


class Limiter:
    def __init__(self):
        self.lock = threading.Lock()
        self.hits = {}

    def allow(self, key, limit, window):
        now = time.time()
        with self.lock:
            arr = [t for t in self.hits.get(key, []) if now - t < window]
            if len(arr) >= limit:
                self.hits[key] = arr
                return False
            arr.append(now)
            self.hits[key] = arr
            return True


def users_me(bearer, mock):
    if mock:
        uid, name = mock.split(":", 1)
        return uid, name
    req = Request(
        "https://api.twitter.com/2/users/me?user.fields=id,username",
        headers={"Authorization": "Bearer " + bearer, "User-Agent": "xcoin-xclaim-standby"},
    )
    with urlopen(req, timeout=8) as resp:
        body = json.loads(resp.read().decode())
    data = body.get("data") or {}
    return str(data.get("id") or ""), str(data.get("username") or "").lower()


# Same fields as the x-coin-web line, plus a UTC timestamp after the prefix.
#   XCLAIM: 2026-10-22T19:11:05.006Z route=/claim-proof handle=<h> address=<a> result=ok error=- status=200 durationMs=6
_LOG_LOCK = threading.Lock()
_HANDLE_RE = re.compile(r"[a-z0-9_]{1,15}\Z")
_ADDRESS_RE = re.compile(r"[1-9A-HJ-NP-Za-km-z]{26,35}\Z")
_ROUTE_RE = re.compile(r"/(?:[A-Za-z0-9_-]{1,24})(?:/[A-Za-z0-9_-]{1,24}){0,4}\Z")
_ERROR_RE = re.compile(r"[a-z0-9-]{1,32}\Z")
_TS_RE = re.compile(r"[0-9]{4}-[0-9]{2}-[0-9]{2}T[0-9]{2}:[0-9]{2}:[0-9]{2}\.[0-9]{3}Z\Z")
_STATUS_ERROR = {
    400: "bad-request",
    401: "sign-in-again",
    403: "mismatch",
    404: "not-found",
    408: "timeout",
    414: "bad-request",
    429: "rate-limited",
    431: "bad-request",
    500: "internal",
    501: "method",
    505: "bad-request",
}


def xclaim_timestamp(now=None):
    if now is None:
        now = time.time()
    seconds = int(now)
    millis = int((now - seconds) * 1000)
    if millis >= 1000:
        seconds += 1
        millis = 0
    return time.strftime("%Y-%m-%dT%H:%M:%S", time.gmtime(seconds)) + ".%03dZ" % millis


def xclaim_log_handle(handle):
    if not isinstance(handle, str):
        return "-"
    text = handle.strip().lower()
    if _HANDLE_RE.fullmatch(text):
        return text
    return "-"


def xclaim_log_address(address):
    # Wallet addresses only. A 64-hex key, a WIF, or a token pasted into
    # this field must not be written. Missing stays "-".
    if not isinstance(address, str):
        return "-"
    text = address.strip()
    if not text:
        return "-"
    if _ADDRESS_RE.fullmatch(text):
        return text
    return "-"


def xclaim_log_route(path):
    if not isinstance(path, str) or not path:
        return "-"
    route = path.split("?", 1)[0].split("#", 1)[0]
    if _ROUTE_RE.fullmatch(route):
        return route
    return "-"


def xclaim_log_error(error, result):
    if result == "ok":
        return "-"
    if not isinstance(error, str):
        return "error"
    text = error.strip()
    if _ERROR_RE.fullmatch(text):
        return text
    return "error"


def format_xclaim_log(route, handle, address, result, error, status, duration_ms, now=None):
    result_s = "ok" if result == "ok" else "error"
    try:
        status_i = int(status)
    except (TypeError, ValueError):
        status_i = 0
    try:
        duration_i = int(duration_ms)
    except (TypeError, ValueError):
        duration_i = 0
    if duration_i < 0:
        duration_i = 0
    ts = now if isinstance(now, str) and _TS_RE.fullmatch(now) else xclaim_timestamp()
    return (
        "XCLAIM: %s route=%s handle=%s address=%s result=%s error=%s status=%d durationMs=%d"
        % (
            ts,
            xclaim_log_route(route),
            xclaim_log_handle(handle),
            xclaim_log_address(address),
            result_s,
            xclaim_log_error(error, result_s),
            status_i,
            duration_i,
        )
    )


def write_xclaim_log(route, handle, address, result, error, status, duration_ms):
    line = format_xclaim_log(route, handle, address, result, error, status, duration_ms) + "\n"
    with _LOG_LOCK:
        sys.stdout.write(line)
        sys.stdout.flush()


def make_handler(key32, key_id, limiter, mock):
    class Handler(BaseHTTPRequestHandler):
        protocol_version = "HTTP/1.0"

        def log_message(self, fmt, *args):
            # The default line repeats the request target. A query string can
            # carry an OAuth code, so the only per-request line is write_xclaim_log.
            return

        def handle_one_request(self):
            self._xclaim_logged = False
            self._xclaim_started = time.monotonic()
            self._xclaim_status = None
            self._log_handle = ""
            self._log_address = ""
            try:
                super().handle_one_request()
            finally:
                raw = getattr(self, "raw_requestline", b"") or b""
                if raw.strip() and not self._xclaim_logged:
                    status = self._xclaim_status
                    if status is None:
                        status = 408
                        error = "timeout"
                        result = "error"
                    else:
                        error = _STATUS_ERROR.get(int(status), "error")
                        result = "ok" if int(status) < 400 else "error"
                        if result == "ok":
                            error = "-"
                    self._emit_log(result, error, status)

        def send_response(self, code, message=None):
            self._xclaim_status = int(code)
            super().send_response(code, message)

        def _emit_log(self, result, error, status):
            if getattr(self, "_xclaim_logged", False):
                return
            self._xclaim_logged = True
            started = getattr(self, "_xclaim_started", None)
            if started is None:
                duration_ms = 0
            else:
                duration_ms = int((time.monotonic() - started) * 1000)
            write_xclaim_log(
                getattr(self, "path", "") or "",
                getattr(self, "_log_handle", "") or "",
                getattr(self, "_log_address", "") or "",
                result,
                error,
                status,
                duration_ms,
            )

        def _send(self, code, obj):
            raw = json.dumps(obj).encode()
            self.send_response(code)
            self.send_header("Content-Type", "application/json")
            self.send_header("Content-Length", str(len(raw)))
            self.end_headers()
            self.wfile.write(raw)

        def _claim_proof(self):
            route = self.path.split("?", 1)[0]
            if route not in ("/claim-proof", "/api/x/claim-proof"):
                self._send(404, {"ok": False, "error": "not-found"})
                return 404, "not-found"
            ip = self.client_address[0]
            if not limiter.allow("ip:" + ip, 30, 60):
                self._send(429, {"ok": False, "error": "rate-limited"})
                return 429, "rate-limited"
            try:
                n = int(self.headers.get("Content-Length") or "0")
            except (TypeError, ValueError):
                self._send(400, {"ok": False, "error": "bad-body"})
                return 400, "bad-body"
            if n <= 0 or n > 4096:
                self._send(400, {"ok": False, "error": "bad-body"})
                return 400, "bad-body"
            try:
                req = json.loads(self.rfile.read(n).decode())
            except Exception:
                self._send(400, {"ok": False, "error": "bad-json"})
                return 400, "bad-json"
            if not isinstance(req, dict):
                self._send(400, {"ok": False, "error": "bad-json"})
                return 400, "bad-json"
            handle_v = req.get("handle")
            address_v = req.get("address")
            self._log_handle = handle_v if isinstance(handle_v, str) else ""
            self._log_address = address_v if isinstance(address_v, str) else ""
            auth = self.headers.get("Authorization") or ""
            if not auth.startswith("Bearer ") or len(auth) < 12:
                self._send(401, {"ok": False, "error": "sign-in-again"})
                return 401, "sign-in-again"
            bearer = auth[7:].strip()
            try:
                uid, username = users_me(bearer, mock)
            except Exception:
                self._send(401, {"ok": False, "error": "sign-in-again"})
                return 401, "sign-in-again"
            handle = str(req.get("handle") or "").lower()
            root = str(req.get("root") or "")
            address = str(req.get("address") or "")
            try:
                req_uid = int(req.get("userId"))
            except Exception:
                self._send(400, {"ok": False, "error": "bad-user"})
                return 400, "bad-user"
            if not uid or username != handle or str(req_uid) != uid:
                self._send(403, {"ok": False, "error": "mismatch"})
                return 403, "mismatch"
            if not limiter.allow("user:" + uid, 10, 60):
                self._send(429, {"ok": False, "error": "rate-limited"})
                return 429, "rate-limited"
            try:
                h160 = b58_hash160(address)
            except Exception:
                self._send(400, {"ok": False, "error": "bad-address"})
                return 400, "bad-address"
            expiry = int(time.time()) + EXPIRY_TTL
            if expiry - int(time.time()) > MAX_FUTURE:
                self._send(500, {"ok": False, "error": "expiry"})
                return 500, "expiry"
            sig = sign_compact(key32, digest(handle, req_uid, h160, root, expiry, key_id))
            self._send(200, {
                "ok": True,
                "keyId": key_id,
                "userId": req_uid,
                "expiry": expiry,
                "sig": sig.hex(),
                "handle": handle,
                "root": root,
                "address": address,
            })
            return 200, "-"

        def do_POST(self):
            status = 500
            error = "internal"
            try:
                status, error = self._claim_proof()
            except Exception:
                if self._xclaim_status is None:
                    try:
                        self._send(500, {"ok": False, "error": "internal"})
                    except Exception:
                        status = 500
                        error = "internal"
                status = self._xclaim_status if self._xclaim_status is not None else 500
                error = "internal"
            finally:
                result = "ok" if status == 200 and error == "-" else "error"
                self._emit_log(result, "-" if result == "ok" else error, status)

    return Handler


def main():
    ap = argparse.ArgumentParser(description="Standby XPF1 signer")
    ap.add_argument("--self-check", metavar="VECTOR", help="check the shared digest file and exit")
    ap.add_argument("--key-file", help="openssl ciphertext, or plaintext hex with --allow-plaintext")
    ap.add_argument("--key-id", type=int, default=2)
    ap.add_argument("--bind", default="127.0.0.1")
    ap.add_argument("--port", type=int, default=38445)
    ap.add_argument("--allow-plaintext", action="store_true")
    ap.add_argument("--mock-users-me", help="TEST ONLY id:handle, skips api.twitter.com")
    args = ap.parse_args()
    if args.self_check:
        return self_check(args.self_check)
    if not args.key_file or not (0 <= args.key_id <= 255):
        ap.error("--key-file and a key id 0-255 are required")
    key = load_key(args.key_file, args.allow_plaintext)
    if key is None:
        return 1
    httpd = ThreadingHTTPServer((args.bind, args.port), make_handler(key, args.key_id, Limiter(), args.mock_users_me))
    sys.stderr.write("xclaim-signer id %d listening on %s:%d\n" % (args.key_id, args.bind, args.port))
    httpd.serve_forever()
    return 0


if __name__ == "__main__":
    sys.exit(main() or 0)
