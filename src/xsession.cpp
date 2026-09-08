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

#include <cstdio>
#include <cstring>

namespace xsession {

static CCriticalSection cs_xsession;

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

std::string ComputeProof(const std::string& userId, const std::string& username, int64_t expiresAt)
{
    std::string key;
    if (!ReadFileBytes(SecretPath(), key) || key.size() != 32)
        return "";
    const std::string msg = strprintf("xcoin-xsession|%s|%s|%lld", userId, username, (long long)expiresAt);
    unsigned char mac[CHMAC_SHA256::OUTPUT_SIZE];
    CHMAC_SHA256((const unsigned char*)key.data(), key.size())
        .Write((const unsigned char*)msg.data(), msg.size())
        .Finalize(mac);
    return HexStr(mac, mac + sizeof(mac));
}

bool SaveSession(const std::string& userId, const std::string& usernameIn, int64_t expiresAt, std::string& err)
{
    std::string username;
    if (!lottery::NormalizeXHandle(usernameIn, username, err))
        return false;
    if (userId.empty()) {
        err = "X user id missing from sign-in";
        return false;
    }
    if (!EnsureSecret(err))
        return false;
    const std::string proof = ComputeProof(userId, username, expiresAt);
    if (proof.empty()) {
        err = "could not compute session proof";
        return false;
    }
    UniValue obj(UniValue::VOBJ);
    obj.push_back(Pair("v", 1));
    obj.push_back(Pair("id", userId));
    obj.push_back(Pair("username", username));
    obj.push_back(Pair("exp", expiresAt));
    obj.push_back(Pair("proof", proof));
    LOCK(cs_xsession);
    if (!WriteFileBytes(SessionPath(), obj.write() + "\n")) {
        err = "cannot write xsession.json";
        return false;
    }
    LogPrintf("xsession: signed in as @%s (id %s)\n", username, userId);
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
    if (out.userId.empty() || out.username.empty() || out.proofHex.empty()) {
        err = "xsession.json missing fields";
        return false;
    }
    if (out.expiresAt > 0 && GetTime() > out.expiresAt) {
        err = "Sign in with X session expired; sign in again";
        return false;
    }
    const std::string want = ComputeProof(out.userId, out.username, out.expiresAt);
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

bool ParseUsersMe(const std::string& json, std::string& userId, std::string& username, std::string& err)
{
    userId.clear();
    username.clear();
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
    userId = data["id"].getValStr();
    const std::string rawName = data["username"].getValStr();
    if (userId.empty() || rawName.empty()) {
        err = "users/me missing id or username";
        return false;
    }
    if (!lottery::NormalizeXHandle(rawName, username, err))
        return false;
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

bool ApplyUsersMePayload(const std::string& payload, std::string& err, UniValue* parsedOut)
{
    if (!IsRegtest()) {
        err = "mock users/me is only allowed on -regtest";
        return false;
    }
    std::string json = payload;
    std::string trim = payload;
    while (!trim.empty() && (trim[0] == ' ' || trim[0] == '\n' || trim[0] == '\t'))
        trim.erase(trim.begin());
    if (trim.empty() || trim[0] != '{') {
        lottery::XAccount acc;
        if (!lottery::ParseXAccountSpec(payload, acc, err))
            return false;
        const std::string id = acc.userId ? strprintf("%llu", (unsigned long long)acc.userId)
                                          : SyntheticId(acc.handle);
        UniValue data(UniValue::VOBJ);
        data.push_back(Pair("id", id));
        data.push_back(Pair("username", acc.handle));
        UniValue root(UniValue::VOBJ);
        root.pushKV("data", data);
        json = root.write();
    }
    std::string userId, username;
    if (!ParseUsersMe(json, userId, username, err))
        return false;
    const int64_t exp = GetTime() + 86400 * 365;
    if (!SaveSession(userId, username, exp, err))
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
        if (!lottery::GetAllowlist().Contains(local.handle, local.userId)) {
            LogPrintf("xsession: signed in @%s but not on the verified allowlist; sync/relay only\n",
                      local.handle);
        } else {
            LogPrintf("xsession: lottery identity @%s (id %s) from Sign in with X\n",
                      local.handle, s.userId);
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

} // namespace xsession
