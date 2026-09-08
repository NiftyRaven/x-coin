// Copyright (c) 2026 The X Coin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "xsession.h"

#include "chainparams.h"
#include "chainparamsbase.h"
#include "crypto/hmac_sha256.h"
#include "crypto/sha256.h"
#include "fs.h"
#include "lottery.h"
#include "random.h"
#include "sync.h"
#include "util.h"
#include "utilstrencodings.h"
#include "utiltime.h"

#include <univalue.h>

#include <cctype>
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <vector>

namespace xsession {

static CCriticalSection cs_xsession;

static std::string AsciiLower(const std::string& in)
{
    std::string out = in;
    for (char& c : out) {
        if (c >= 'A' && c <= 'Z')
            c = static_cast<char>(c - 'A' + 'a');
    }
    return out;
}

static std::string TrimCopy(const std::string& in)
{
    size_t a = 0;
    while (a < in.size() && std::isspace(static_cast<unsigned char>(in[a])))
        a++;
    size_t b = in.size();
    while (b > a && std::isspace(static_cast<unsigned char>(in[b - 1])))
        b--;
    return in.substr(a, b - a);
}

bool IsOfficialXVerified(bool verified, const std::string& verifiedType)
{
    if (verified)
        return true;
    const std::string t = AsciiLower(verifiedType);
    return t == "blue" || t == "business" || t == "government";
}

bool Session::IsXVerified() const
{
    return IsOfficialXVerified(verified, verifiedType);
}

static bool ReadFileBytes(const fs::path& path, std::string& out)
{
    FILE* f = fsbridge::fopen(path, "rb");
    if (!f)
        return false;
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    if (n < 0) {
        fclose(f);
        return false;
    }
    fseek(f, 0, SEEK_SET);
    out.resize((size_t)n);
    if (n && fread(&out[0], 1, (size_t)n, f) != (size_t)n) {
        fclose(f);
        return false;
    }
    fclose(f);
    return true;
}

static bool WriteFileBytes(const fs::path& path, const std::string& data)
{
    FILE* f = fsbridge::fopen(path, "wb");
    if (!f)
        return false;
    bool ok = fwrite(data.data(), 1, data.size(), f) == data.size();
    fclose(f);
    return ok;
}

bool IsRegtest()
{
    return GetParams().NetworkIDString() == CBaseChainParams::REGTEST;
}

std::string SecretPath()
{
    return (GetDataDir() / "xsession.key").string();
}

std::string SessionPath()
{
    return (GetDataDir() / "xsession.json").string();
}

bool EnsureSecret(std::string& err)
{
    LOCK(cs_xsession);
    const fs::path path = SecretPath();
    if (fs::exists(path))
        return true;
    unsigned char key[32];
    GetRandBytes(key, 32);
    if (!WriteFileBytes(path, std::string(reinterpret_cast<char*>(key), 32))) {
        err = "cannot write xsession.key";
        return false;
    }
    return true;
}

std::string ComputeProof(const std::string& userId, const std::string& username, int64_t expiresAt,
                         bool verified, const std::string& verifiedType)
{
    std::string key;
    if (!ReadFileBytes(SecretPath(), key) || key.size() != 32)
        return "";
    const std::string msg = strprintf("xcoin-xsession|%s|%s|%lld|%d|%s",
                                      userId, username, (long long)expiresAt,
                                      verified ? 1 : 0, AsciiLower(verifiedType));
    unsigned char mac[CHMAC_SHA256::OUTPUT_SIZE];
    CHMAC_SHA256((const unsigned char*)key.data(), key.size())
        .Write((const unsigned char*)msg.data(), msg.size())
        .Finalize(mac);
    return HexStr(mac, mac + sizeof(mac));
}

bool SaveSession(const std::string& userId, const std::string& usernameIn, int64_t expiresAt,
                 std::string& err, bool verified, const std::string& verifiedTypeIn)
{
    std::string username;
    if (!lottery::NormalizeXHandle(usernameIn, username, err))
        return false;
    if (userId.empty()) {
        err = "X user id missing from sign-in";
        return false;
    }
    std::string verifiedType = AsciiLower(TrimCopy(verifiedTypeIn));
    if (verifiedType == "none")
        verifiedType.clear();
    // Persist the official meaning: any X checkmark type counts as verified==true.
    const bool xVerified = IsOfficialXVerified(verified, verifiedType);
    if (!EnsureSecret(err))
        return false;
    const std::string proof = ComputeProof(userId, username, expiresAt, xVerified, verifiedType);
    if (proof.empty()) {
        err = "could not compute session proof";
        return false;
    }
    UniValue obj(UniValue::VOBJ);
    obj.push_back(Pair("v", 2));
    obj.push_back(Pair("id", userId));
    obj.push_back(Pair("username", username));
    obj.push_back(Pair("exp", expiresAt));
    obj.push_back(Pair("verified", xVerified));
    obj.push_back(Pair("verified_type", verifiedType));
    obj.push_back(Pair("proof", proof));
    LOCK(cs_xsession);
    if (!WriteFileBytes(SessionPath(), obj.write() + "\n")) {
        err = "cannot write xsession.json";
        return false;
    }
    LogPrintf("xsession: signed in as @%s (id %s) X-Verified=%s type=%s\n",
              username, userId, xVerified ? "true" : "false",
              verifiedType.empty() ? "-" : verifiedType);
    return true;
}

bool LoadSession(Session& out, std::string& err)
{
    out = Session();
    std::string raw;
    if (!ReadFileBytes(SessionPath(), raw) || raw.empty()) {
        err = "no Sign in with X session";
        return false;
    }
    UniValue obj;
    if (!obj.read(raw) || !obj.isObject()) {
        err = "xsession.json is not valid JSON";
        return false;
    }
    out.userId = obj["id"].getValStr();
    out.username = obj["username"].getValStr();
    out.expiresAt = obj["exp"].isNum() ? obj["exp"].get_int64() : atoi64(obj["exp"].getValStr());
    out.proofHex = obj["proof"].getValStr();
    if (obj.exists("verified")) {
        if (obj["verified"].isBool())
            out.verified = obj["verified"].get_bool();
        else if (obj["verified"].isTrue())
            out.verified = true;
        else
            out.verified = (obj["verified"].getValStr() == "true" || obj["verified"].getValStr() == "1");
    }
    if (obj.exists("verified_type"))
        out.verifiedType = AsciiLower(obj["verified_type"].getValStr());
    if (out.verifiedType == "none")
        out.verifiedType.clear();
    out.verified = IsOfficialXVerified(out.verified, out.verifiedType);
    if (out.userId.empty() || out.username.empty() || out.proofHex.empty()) {
        err = "xsession.json missing fields";
        return false;
    }
    if (out.expiresAt > 0 && GetTime() > out.expiresAt) {
        err = "Sign in with X session expired; sign in again";
        return false;
    }
    const std::string want = ComputeProof(out.userId, out.username, out.expiresAt,
                                          out.verified, out.verifiedType);
    if (want.empty() || want != out.proofHex) {
        err = "session proof is invalid (not produced by this node's sign-in)";
        return false;
    }
    err.clear();
    return true;
}

bool HasValidSession()
{
    Session s;
    std::string err;
    return LoadSession(s, err);
}

void ClearSession()
{
    LOCK(cs_xsession);
    fs::remove(SessionPath());
}

static bool ParseVerifiedField(const UniValue& data, bool& verified, std::string& verifiedType)
{
    verified = false;
    verifiedType.clear();
    if (data.exists("verified_type") && data["verified_type"].isStr())
        verifiedType = AsciiLower(data["verified_type"].getValStr());
    else if (data.exists("verified_type"))
        verifiedType = AsciiLower(data["verified_type"].getValStr());
    if (verifiedType == "none")
        verifiedType.clear();
    if (data.exists("verified")) {
        if (data["verified"].isBool())
            verified = data["verified"].get_bool();
        else if (data["verified"].isTrue())
            verified = true;
        else if (data["verified"].isFalse())
            verified = false;
        else {
            const std::string v = AsciiLower(data["verified"].getValStr());
            verified = (v == "true" || v == "1" || v == "yes");
        }
    }
    verified = IsOfficialXVerified(verified, verifiedType);
    return true;
}

bool ParseUsersMe(const std::string& json, UsersMe& out, std::string& err)
{
    out = UsersMe();
    UniValue u;
    if (!u.read(json) || !u.isObject()) {
        err = "users/me response is not a JSON object";
        return false;
    }
    UniValue data = u["data"];
    if (!data.isObject()) {
        err = "users/me missing data";
        return false;
    }
    out.userId = data["id"].getValStr();
    const std::string rawName = data["username"].getValStr();
    if (out.userId.empty() || rawName.empty()) {
        err = "users/me missing id or username";
        return false;
    }
    if (!lottery::NormalizeXHandle(rawName, out.username, err))
        return false;
    ParseVerifiedField(data, out.verified, out.verifiedType);
    return true;
}

static std::string SyntheticId(const std::string& handle)
{
    unsigned char h[32];
    CSHA256().Write((const unsigned char*)handle.data(), handle.size()).Finalize(h);
    uint64_t v = 0;
    for (int i = 0; i < 7; i++)
        v = (v << 8) | h[i];
    if (v == 0)
        v = 1;
    return strprintf("%llu", (unsigned long long)v);
}

static bool AllDigits(const std::string& s)
{
    if (s.empty())
        return false;
    for (unsigned char c : s) {
        if (c < '0' || c > '9')
            return false;
    }
    return true;
}

static std::vector<std::string> SplitColon(const std::string& in)
{
    std::vector<std::string> parts;
    std::string cur;
    for (char c : in) {
        if (c == ':') {
            parts.push_back(cur);
            cur.clear();
        } else {
            cur.push_back(c);
        }
    }
    parts.push_back(cur);
    return parts;
}

static bool ParseMockCompact(const std::string& payload, UsersMe& out, std::string& err)
{
    out = UsersMe();
    const std::string s = TrimCopy(payload);
    if (s.empty()) {
        err = "mock users/me spec is empty";
        return false;
    }
    const std::vector<std::string> parts = SplitColon(s);
    if (!lottery::NormalizeXHandle(parts[0], out.username, err))
        return false;
    bool sawFlag = false;
    uint64_t userId = 0;
    for (size_t i = 1; i < parts.size(); ++i) {
        const std::string p = AsciiLower(TrimCopy(parts[i]));
        if (p.empty())
            continue;
        if (p == "verified") {
            out.verified = true;
            if (out.verifiedType.empty())
                out.verifiedType = "blue";
            sawFlag = true;
        } else if (p == "unverified") {
            out.verified = false;
            out.verifiedType.clear();
            sawFlag = true;
        } else if (p == "blue" || p == "business" || p == "government") {
            out.verified = true;
            out.verifiedType = p;
            sawFlag = true;
        } else if (AllDigits(p)) {
            try {
                userId = std::stoull(p);
            } catch (const std::exception&) {
                err = "X user id is not a valid integer";
                return false;
            }
        } else {
            err = strprintf("unknown mock users/me suffix '%s' (use verified, unverified, blue, business, government, or a numeric user id)", p);
            return false;
        }
    }
    if (!sawFlag) {
        // Compact mock with no flag: treat as X Verified (blue) so regtest
        // lottery smokes keep working. Use :unverified for zero-chance tests.
        // JSON payloads without verified stay unverified (honest users/me).
        out.verified = true;
        out.verifiedType = "blue";
    }
    out.userId = userId ? strprintf("%llu", (unsigned long long)userId) : SyntheticId(out.username);
    out.verified = IsOfficialXVerified(out.verified, out.verifiedType);
    return true;
}

bool ApplyUsersMePayload(const std::string& payload, std::string& err, UniValue* parsedOut)
{
    if (!IsRegtest()) {
        err = "mock users/me is only allowed on -regtest";
        return false;
    }
    std::string json = payload;
    std::string trim = TrimCopy(payload);
    UsersMe me;
    if (trim.empty() || trim[0] != '{') {
        if (!ParseMockCompact(payload, me, err))
            return false;
        UniValue data(UniValue::VOBJ);
        data.push_back(Pair("id", me.userId));
        data.push_back(Pair("username", me.username));
        data.push_back(Pair("verified", me.verified));
        if (!me.verifiedType.empty())
            data.push_back(Pair("verified_type", me.verifiedType));
        UniValue root(UniValue::VOBJ);
        root.pushKV("data", data);
        json = root.write();
    } else if (!ParseUsersMe(json, me, err)) {
        return false;
    }
    const int64_t exp = GetTime() + 86400 * 365;
    if (!SaveSession(me.userId, me.username, exp, err, me.verified, me.verifiedType))
        return false;
    if (parsedOut) {
        parsedOut->clear();
        parsedOut->read(json);
    }
    BindLotteryFromSession();
    return true;
}

void ApplyStartupArgs()
{
    if (!IsRegtest() || !gArgs.IsArgSet("-xoauthmock"))
        return;
    const std::string payload = gArgs.GetArg("-xoauthmock", "");
    if (payload.empty())
        return;
    std::string err;
    if (!ApplyUsersMePayload(payload, err, nullptr))
        LogPrintf("xsession: -xoauthmock failed: %s\n", err);
}

void BindLotteryFromSession()
{
    Session s;
    std::string err;
    lottery::XAccount local;
    if (LoadSession(s, err)) {
        local.handle = s.username;
        local.userId = atoi64(s.userId);
        lottery::GetRegistry().SetLocalXAccount(local);
        if (!s.IsXVerified()) {
            LogPrintf("xsession: signed in @%s but not X Verified (users/me.verified); lottery closed, send/receive ok\n",
                      local.handle);
        } else {
            LogPrintf("xsession: lottery identity @%s (id %s) X Verified type=%s — fair draw, invite list is not a gate\n",
                      local.handle, s.userId,
                      s.verifiedType.empty() ? "blue" : s.verifiedType);
        }
        return;
    }
    if (gArgs.IsArgSet("-xaccount")) {
        LogPrintf("xsession: ignoring typed -xaccount=%s (Sign in with X required)\n",
                  gArgs.GetArg("-xaccount", ""));
    }
    lottery::GetRegistry().SetLocalXAccount(lottery::XAccount());
    LogPrintf("xsession: no signed-in X session; this node is not lottery-eligible\n");
}

bool RequireSession(std::string& err)
{
    Session s;
    if (!LoadSession(s, err)) {
        err = "Sign in with X required to send, receive, or own assets; a typed handle is not enough";
        return false;
    }
    return true;
}

bool SessionIsXVerified()
{
    Session s;
    std::string err;
    return LoadSession(s, err) && s.IsXVerified();
}

bool RequireXVerified(std::string& err)
{
    Session s;
    if (!LoadSession(s, err)) {
        err = "Sign in with X required; lottery is X Verified (blue check) only";
        return false;
    }
    if (!s.IsXVerified()) {
        err = "X account is not X Verified. X Verified is X's blue check / X Premium "
              "(and business / government org checks) from GET /2/users/me "
              "(https://help.x.com/en/managing-your-account/about-x-bluecheck). "
              "The operator invite list is not X Verified.";
        return false;
    }
    return true;
}

bool RequireHandle(const std::string& handleIn, std::string& err)
{
    std::string handle;
    if (!lottery::NormalizeXHandle(handleIn, handle, err))
        return false;
    Session s;
    if (!LoadSession(s, err)) {
        err = "Sign in with X required; typed handles cannot claim an identity";
        return false;
    }
    if (s.username != handle) {
        err = strprintf("signed in as @%s; cannot use @%s (typed impersonation rejected)",
                        s.username, handle);
        return false;
    }
    return true;
}

std::string SignedInHandle()
{
    Session s;
    std::string err;
    if (!LoadSession(s, err))
        return "";
    return s.username;
}

std::string SignedInUserId()
{
    Session s;
    std::string err;
    if (!LoadSession(s, err))
        return "";
    return s.userId;
}

bool SignedInVerified()
{
    return SessionIsXVerified();
}

std::string SignedInVerifiedType()
{
    Session s;
    std::string err;
    if (!LoadSession(s, err))
        return "";
    return s.verifiedType;
}

} // namespace xsession
