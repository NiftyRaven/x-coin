// Copyright (c) 2026 The X Coin developers
// Copyright (c) 2017-2021 The Raven Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "lottery.h"

#include "base58.h"
#include "chain.h"
#include "chainparams.h"
#include "chainparamsbase.h"
#include "consensus/validation.h"
#include "fs.h"
#include "hash.h"
#include "key.h"
#include "miner.h"
#include "primitives/block.h"
#include "pubkey.h"
#include "random.h"
#include "script/standard.h"
#include "util.h"
#include "utilstrencodings.h"
#include "crypto/sha256.h"
#include "validation.h"
#include "xlookup.h"
#include "xsession.h"

#include <exception>
#include <map>

#ifdef ENABLE_WALLET
#include "wallet/wallet.h"
extern std::vector<CWalletRef> vpwallets;
#endif

class CWallet;

#include <boost/bind/bind.hpp>
#include <boost/thread.hpp>

#include <atomic>
#include <algorithm>
#include <cctype>
#include <cstring>
#include <ctime>
#include <fstream>
#include <sstream>

namespace lottery {

static Registry g_registry;
static Allowlist g_allowlist;
static boost::thread_group* g_producerThreads = nullptr;
static CCriticalSection cs_producer;
static CKey g_localPayoutKey;
static CKey g_attestorKey;
static bool g_forceBakedSeedForTest = false;
static CCriticalSection cs_attest;
struct AttestRow {
    std::string handle;
    std::vector<unsigned char> sig;
};
static std::map<uint160, AttestRow> g_attest;
static std::atomic<int64_t> g_seedNotifyUntil{0};
static CWallet* FirstWalletOrNull();

Registry& GetRegistry()
{
    return g_registry;
}

Allowlist& GetAllowlist()
{
    return g_allowlist;
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

bool NormalizeXHandle(const std::string& in, std::string& out, std::string& err)
{
    out.clear();
    std::string s = TrimCopy(in);
    if (!s.empty() && s[0] == '@')
        s = s.substr(1);
    s = TrimCopy(s);
    if (s.empty()) {
        err = "X handle is empty";
        return false;
    }
    if (s.size() > MAX_X_HANDLE) {
        err = strprintf("X handle longer than %u characters", (unsigned)MAX_X_HANDLE);
        return false;
    }
    out.reserve(s.size());
    for (unsigned char c : s) {
        if (c >= 'A' && c <= 'Z')
            c = static_cast<unsigned char>(c - 'A' + 'a');
        if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_')) {
            err = "X handle must be letters, digits, or underscore";
            out.clear();
            return false;
        }
        out.push_back(static_cast<char>(c));
    }
    return true;
}

bool ParseXAccountSpec(const std::string& in, XAccount& out, std::string& err)
{
    out = XAccount();
    std::string s = TrimCopy(in);
    if (s.empty()) {
        err = "X account spec is empty";
        return false;
    }
    std::string handlePart = s;
    std::string idPart;
    size_t sep = s.find_first_of(":,");
    if (sep != std::string::npos) {
        handlePart = s.substr(0, sep);
        idPart = TrimCopy(s.substr(sep + 1));
    }
    if (!NormalizeXHandle(handlePart, out.handle, err))
        return false;
    if (!idPart.empty()) {
        if (idPart.find_first_not_of("0123456789") != std::string::npos) {
            err = "X user id must be a positive integer";
            return false;
        }
        try {
            out.userId = std::stoull(idPart);
        } catch (const std::exception&) {
            err = "X user id is not a valid integer";
            return false;
        }
    }
    return true;
}

void Allowlist::SetPersistPath(const std::string& path)
{
    LOCK(cs);
    persistPath = path;
}

std::string Allowlist::PersistPath() const
{
    LOCK(cs);
    return persistPath;
}

bool Allowlist::Add(const std::string& handle, uint64_t userId, std::string& err, bool persist,
                    const CScript& pinnedScript)
{
    std::string norm;
    if (!NormalizeXHandle(handle, norm, err))
        return false;
    {
        LOCK(cs);
        handles[norm] = userId;
        if (userId != 0)
            byUserId[userId] = norm;
        if (!pinnedScript.empty())
            pins[norm] = pinnedScript;
    }
    if (persist && !persistPath.empty() && !Persist(err))
        return false;
    err.clear();
    return true;
}

bool Allowlist::Remove(const std::string& handle, std::string& err, bool persist)
{
    std::string norm;
    if (!NormalizeXHandle(handle, norm, err))
        return false;
    {
        LOCK(cs);
        auto it = handles.find(norm);
        if (it == handles.end()) {
            err = "X handle is not on the operator invite list";
            return false;
        }
        if (it->second != 0)
            byUserId.erase(it->second);
        handles.erase(it);
        pins.erase(norm);
    }
    if (persist && !persistPath.empty() && !Persist(err))
        return false;
    err.clear();
    return true;
}

bool Allowlist::Contains(const std::string& handle, uint64_t userId) const
{
    std::string norm;
    std::string err;
    if (!NormalizeXHandle(handle, norm, err))
        return false;
    LOCK(cs);
    auto it = handles.find(norm);
    if (it == handles.end())
        return false;
    if (it->second != 0 && userId != 0 && it->second != userId)
        return false;
    return true;
}

bool Allowlist::Allows(const std::string& handle, uint64_t userId) const
{
    (void)handle;
    (void)userId;
    // Fair lottery: the invite list cannot exclude an X Verified wallet
    // that is running. Payout pins still apply via PinnedScript.
    return true;
}

CScript Allowlist::PinnedScript(const std::string& handle) const
{
    std::string norm;
    std::string err;
    if (!NormalizeXHandle(handle, norm, err))
        return CScript();
    LOCK(cs);
    auto it = pins.find(norm);
    if (it == pins.end())
        return CScript();
    return it->second;
}

void Allowlist::Reset()
{
    LOCK(cs);
    handles.clear();
    byUserId.clear();
    pins.clear();
}

std::vector<VerifiedXAccount> Allowlist::List() const
{
    LOCK(cs);
    std::vector<VerifiedXAccount> out;
    out.reserve(handles.size());
    for (const auto& kv : handles) {
        VerifiedXAccount a;
        a.handle = kv.first;
        a.userId = kv.second;
        auto pit = pins.find(kv.first);
        if (pit != pins.end())
            a.pinnedScript = pit->second;
        out.push_back(a);
    }
    return out;
}

size_t Allowlist::Size() const
{
    LOCK(cs);
    return handles.size();
}

bool Allowlist::LoadFile(const std::string& path, std::string& err)
{
    if (path.empty()) {
        err = "allowlist path is empty";
        return false;
    }
    std::ifstream in(path.c_str());
    if (!in) {
        err = strprintf("cannot open operator invite list %s", path);
        return false;
    }
    std::string line;
    int n = 0;
    while (std::getline(in, line)) {
        std::string t = TrimCopy(line);
        if (t.empty() || t[0] == '#')
            continue;
        XAccount acc;
        std::string perr;
        CScript pin;
        // Lines: handle | handle userid | handle address | handle userid address
        // also handle:userid (no pin).
        std::string spec = t;
        std::string pinTok;
        if (t.find_first_of(":,") == std::string::npos) {
            std::vector<std::string> parts;
            std::string cur;
            for (char c : t) {
                if (c == ' ' || c == '\t') {
                    if (!cur.empty()) {
                        parts.push_back(cur);
                        cur.clear();
                    }
                } else {
                    cur.push_back(c);
                }
            }
            if (!cur.empty())
                parts.push_back(cur);
            if (parts.empty())
                continue;
            spec = parts[0];
            if (parts.size() >= 2) {
                const bool secondIsId = parts[1].find_first_not_of("0123456789") == std::string::npos;
                if (secondIsId) {
                    spec = parts[0] + ":" + parts[1];
                    if (parts.size() >= 3)
                        pinTok = parts[2];
                } else {
                    pinTok = parts[1];
                }
            }
        }
        if (!ParseXAccountSpec(spec, acc, perr)) {
            err = strprintf("%s (line %s)", perr, t);
            return false;
        }
        if (!pinTok.empty()) {
            CTxDestination dest = DecodeDestination(pinTok);
            if (IsValidDestination(dest)) {
                pin = GetScriptForDestination(dest);
            } else if (IsHex(pinTok) && (pinTok.size() % 2) == 0 && pinTok.size() >= 4) {
                const std::vector<unsigned char> raw = ParseHex(pinTok);
                if (raw.empty() || raw.size() > MAX_HEARTBEAT_SCRIPT) {
                    err = strprintf("pinned payout too large (%s)", t);
                    return false;
                }
                pin = CScript(raw.begin(), raw.end());
            } else {
                err = strprintf("pinned payout is not an address or script hex (%s)", t);
                return false;
            }
        }
        std::string aerr;
        if (!Add(acc.handle, acc.userId, aerr, false, pin)) {
            err = aerr;
            return false;
        }
        n++;
    }
    err.clear();
    LogPrintf("lottery: loaded %d operator invite-list handle(s) from %s\n", n, path);
    return true;
}

bool Allowlist::Persist(std::string& err) const
{
    std::string path;
    std::vector<VerifiedXAccount> cur;
    {
        LOCK(cs);
        path = persistPath;
        if (path.empty())
            return true;
        cur.reserve(handles.size());
        for (const auto& kv : handles) {
            VerifiedXAccount a;
            a.handle = kv.first;
            a.userId = kv.second;
            auto pit = pins.find(kv.first);
            if (pit != pins.end())
                a.pinnedScript = pit->second;
            cur.push_back(a);
        }
    }
    FILE* f = fsbridge::fopen(path, "wb");
    if (!f) {
        err = strprintf("cannot write operator invite list %s", path);
        return false;
    }
    fprintf(f, "# X Coin operator invite list (private-mesh ACL, NOT X Verified)\n");
    fprintf(f, "# X Verified is X's blue check from GET /2/users/me, not this file.\n");
    fprintf(f, "# https://help.x.com/en/managing-your-account/about-x-bluecheck\n");
    fprintf(f, "# Optional extra gate when non-empty. format: handle [userid] [payout]\n");
    for (const auto& a : cur) {
        std::string pin;
        if (!a.pinnedScript.empty()) {
            CTxDestination d;
            if (ExtractDestination(a.pinnedScript, d))
                pin = EncodeDestination(d);
            else
                pin = HexStr(a.pinnedScript.begin(), a.pinnedScript.end());
        }
        if (a.userId != 0 && !pin.empty())
            fprintf(f, "%s %llu %s\n", a.handle.c_str(), (unsigned long long)a.userId, pin.c_str());
        else if (a.userId != 0)
            fprintf(f, "%s %llu\n", a.handle.c_str(), (unsigned long long)a.userId);
        else if (!pin.empty())
            fprintf(f, "%s %s\n", a.handle.c_str(), pin.c_str());
        else
            fprintf(f, "%s\n", a.handle.c_str());
    }
    fclose(f);
    err.clear();
    return true;
}

void InitEligibility()
{
    Allowlist& allow = GetAllowlist();
    const std::string persist = (GetDataDir() / "verified-x-accounts.txt").string();
    allow.SetPersistPath(persist);
    if (fs::exists(persist)) {
        std::string err;
        if (!allow.LoadFile(persist, err))
            LogPrintf("lottery: %s\n", err);
    }
    if (gArgs.IsArgSet("-xallowlist")) {
        const std::string extra = gArgs.GetArg("-xallowlist", "");
        if (!extra.empty() && extra != persist) {
            std::string err;
            if (!allow.LoadFile(extra, err))
                LogPrintf("lottery: %s\n", err);
            else {
                std::string perr;
                allow.Persist(perr);
            }
        }
    }
    for (const std::string& spec : gArgs.GetArgs("-xverified")) {
        XAccount acc;
        std::string err;
        if (!ParseXAccountSpec(spec, acc, err)) {
            LogPrintf("lottery: -xverified=%s: %s\n", spec, err);
            continue;
        }
        if (!allow.Add(acc.handle, acc.userId, err))
            LogPrintf("lottery: -xverified=%s: %s\n", spec, err);
    }

    xsession::ApplyStartupArgs();
    InitAttestation();
    xsession::BindLotteryFromSession();
}

uint160 IdFromScript(const CScript& script)
{
    return Hash160(script);
}

bool RequireXAttestation()
{
    try {
        const CChainParams& p = GetParams();
        if (p.MineBlocksOnDemand())
            return false;
        return !p.XAttestorPub().empty();
    } catch (...) {
        return false;
    }
}

CPubKey AttestorPub()
{
    CPubKey pub;
    try {
        const std::vector<unsigned char>& v = GetParams().XAttestorPub();
        if (!v.empty())
            pub.Set(v.begin(), v.end());
    } catch (...) {
    }
    return pub;
}

uint256 AttestDigest(const uint160& id, const std::string& handle)
{
    std::string norm;
    std::string err;
    if (!NormalizeXHandle(handle, norm, err))
        norm.clear();
    uint256 h;
    CSHA256()
        .Write((const unsigned char*)ATTEST_DIGEST_MAGIC, strlen(ATTEST_DIGEST_MAGIC))
        .Write(id.begin(), id.size())
        .Write((const unsigned char*)norm.data(), norm.size())
        .Finalize(h.begin());
    return h;
}

static bool LoadAttestorKeyFromPath(const std::string& path)
{
    FILE* f = fsbridge::fopen(path, "rb");
    if (!f)
        return false;
    std::string raw;
    fseek(f, 0, SEEK_END);
    const long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (n > 0 && n < 256) {
        raw.resize((size_t)n);
        if (fread(&raw[0], 1, (size_t)n, f) != (size_t)n)
            raw.clear();
    }
    fclose(f);
    while (!raw.empty() && (raw.back() == '\n' || raw.back() == '\r' || raw.back() == ' '))
        raw.pop_back();
    if (!(IsHex(raw) && raw.size() == 64))
        return false;
    const std::vector<unsigned char> secret = ParseHex(raw);
    if (secret.size() != 32)
        return false;
    g_attestorKey.Set(secret.begin(), secret.end(), true);
    return g_attestorKey.IsValid();
}

void InitAttestation()
{
    const std::string path = gArgs.GetArg("-xattestorkey",
                                          (GetDataDir() / "xattestor.key").string());
    if (fs::exists(path)) {
        if (!LoadAttestorKeyFromPath(path))
            LogPrintf("lottery: failed to load xattestor.key\n");
        else {
            const CPubKey want = AttestorPub();
            if (want.IsValid() && g_attestorKey.GetPubKey() != want)
                LogPrintf("lottery: ERROR xattestor.key does not match consensus attestor pub\n");
            else
                LogPrintf("lottery: loaded seed attestor key\n");
        }
    } else if (xlookup::LookupEnabled() && RequireXAttestation()) {
        LogPrintf("lottery: -xlookupbearer set but xattestor.key missing — "
                  "cannot stamp blue-check eligibility\n");
    }

    // Self-stamp of a local X handle is optional. The seed has no Sign-in;
    // it stamps OTHER users after X lookup when their xhb arrives.
    if (IsBakedSeed()) {
        LogPrintf("lottery: baked seed ready (no Sign-in; attestor key + "
                  "X lookup + XSD1; stamps peers, does not enter the hat)\n");
        return;
    }
    if (xlookup::LookupEnabled() && g_attestorKey.IsValid()) {
        const XAccount x = GetRegistry().LocalXAccount();
        const CScript s = GetRegistry().LocalScript();
        if (!x.handle.empty() && !s.empty() && xsession::SessionIsXVerified()) {
            std::vector<unsigned char> sig;
            if (LookupAndAttest(s, x.handle, sig))
                LogPrintf("lottery: self-attested @%s against X\n", x.handle);
            else
                LogPrintf("lottery: self-attest @%s failed (not a live blue check?)\n", x.handle);
        }
    }
}

bool SignAttestation(const uint160& id, const std::string& handle,
                     std::vector<unsigned char>& sigOut)
{
    sigOut.clear();
    if (!g_attestorKey.IsValid())
        return false;
    const uint256 digest = AttestDigest(id, handle);
    return g_attestorKey.SignCompact(digest, sigOut);
}

bool VerifyAttestationPub(const CPubKey& want, const uint160& id,
                          const std::string& handle,
                          const std::vector<unsigned char>& sig)
{
    if (sig.size() != 65 || !want.IsFullyValid())
        return false;
    const uint256 digest = AttestDigest(id, handle);
    CPubKey rec;
    if (!rec.RecoverCompact(digest, sig))
        return false;
    return rec == want;
}

bool VerifyAttestation(const uint160& id, const std::string& handle,
                       const std::vector<unsigned char>& sig)
{
    return VerifyAttestationPub(AttestorPub(), id, handle, sig);
}

void SetAttestorKeyForTest(const CKey& key)
{
    g_attestorKey = key;
}

void SetBakedSeedForTest(bool on)
{
    g_forceBakedSeedForTest = on;
}

void ResetAttestations()
{
    LOCK(cs_attest);
    g_attest.clear();
}

void StoreAttestation(const uint160& id, const std::string& handle,
                      const std::vector<unsigned char>& sig)
{
    std::string norm;
    std::string err;
    if (!NormalizeXHandle(handle, norm, err))
        return;
    if (!VerifyAttestation(id, norm, sig))
        return;
    LOCK(cs_attest);
    AttestRow row;
    row.handle = norm;
    row.sig = sig;
    g_attest[id] = row;
}

bool HasAttestation(const uint160& id)
{
    LOCK(cs_attest);
    return g_attest.count(id) > 0;
}

bool GetAttestation(const uint160& id, std::string& handle,
                    std::vector<unsigned char>& sig)
{
    LOCK(cs_attest);
    auto it = g_attest.find(id);
    if (it == g_attest.end())
        return false;
    handle = it->second.handle;
    sig = it->second.sig;
    return true;
}

bool IsXLookupNode()
{
    return xlookup::LookupEnabled() && g_attestorKey.IsValid();
}

bool IsBakedSeed()
{
    if (g_forceBakedSeedForTest)
        return true;
    const CPubKey want = AttestorPub();
    return g_attestorKey.IsValid() && want.IsFullyValid() && g_attestorKey.GetPubKey() == want;
}

bool MayProduceBlock()
{
    // Infrastructure seed: wallet keypool script + xattestor.key + XSD1.
    // No Sign-in. Lottery winners are stamped users, not the seed.
    if (IsBakedSeed())
        return true;
    if (RequireXAttestation())
        return false;
    return GetRegistry().LocalEligible();
}

void RequestSeedNotify()
{
    g_seedNotifyUntil.store(GetTime() + 20);
}

bool SeedNotifyActive()
{
    return GetTime() <= g_seedNotifyUntil.load();
}

uint256 SeedBlockDigest(int nHeight, const uint256& prevBlockHash,
                        const std::vector<uint160>& ids)
{
    uint256 h;
    unsigned char ht[4];
    ht[0] = (uint32_t)nHeight & 0xff;
    ht[1] = ((uint32_t)nHeight >> 8) & 0xff;
    ht[2] = ((uint32_t)nHeight >> 16) & 0xff;
    ht[3] = ((uint32_t)nHeight >> 24) & 0xff;
    CSHA256 w;
    w.Write((const unsigned char*)SEED_DIGEST_MAGIC, strlen(SEED_DIGEST_MAGIC));
    w.Write(prevBlockHash.begin(), prevBlockHash.size());
    w.Write(ht, 4);
    for (const auto& id : ids)
        w.Write(id.begin(), id.size());
    w.Finalize(h.begin());
    return h;
}

CScript MakeSeedBlockCommitment(int nHeight, const uint256& prevBlockHash,
                                const std::vector<uint160>& ids)
{
    std::vector<unsigned char> sig;
    const uint256 digest = SeedBlockDigest(nHeight, prevBlockHash, ids);
    if (!g_attestorKey.IsValid() || !g_attestorKey.SignCompact(digest, sig) || sig.size() != 65)
        return CScript();
    std::vector<unsigned char> data;
    data.push_back((unsigned char)SEED_BLOCK_MAGIC[0]);
    data.push_back((unsigned char)SEED_BLOCK_MAGIC[1]);
    data.push_back((unsigned char)SEED_BLOCK_MAGIC[2]);
    data.push_back((unsigned char)SEED_BLOCK_MAGIC[3]);
    data.insert(data.end(), sig.begin(), sig.end());
    return CScript() << OP_RETURN << data;
}

bool VerifySeedBlockSig(const CPubKey& pub, int nHeight, const uint256& prevBlockHash,
                        const std::vector<uint160>& ids,
                        const std::vector<unsigned char>& sig)
{
    if (sig.size() != 65 || !pub.IsFullyValid())
        return false;
    const uint256 digest = SeedBlockDigest(nHeight, prevBlockHash, ids);
    CPubKey rec;
    if (!rec.RecoverCompact(digest, sig))
        return false;
    return rec == pub;
}

bool CheckSeedBlockCommitment(int nHeight, const uint256& prevBlockHash,
                              const std::vector<uint160>& ids, const CScript& script)
{
    if (script.empty() || script[0] != OP_RETURN)
        return false;
    CScript::const_iterator pc = script.begin() + 1;
    std::vector<unsigned char> data;
    opcodetype opcode;
    if (!script.GetOp(pc, opcode, data) || data.size() != 4 + 65)
        return false;
    if (data[0] != (unsigned char)SEED_BLOCK_MAGIC[0] ||
        data[1] != (unsigned char)SEED_BLOCK_MAGIC[1] ||
        data[2] != (unsigned char)SEED_BLOCK_MAGIC[2] ||
        data[3] != (unsigned char)SEED_BLOCK_MAGIC[3])
        return false;
    std::vector<unsigned char> sig(data.begin() + 4, data.end());
    return VerifySeedBlockSig(AttestorPub(), nHeight, prevBlockHash, ids, sig);
}

bool LookupAndAttest(const CScript& script, const std::string& handle,
                     std::vector<unsigned char>& sigOut)
{
    sigOut.clear();
    std::string norm;
    std::string nerr;
    if (!NormalizeXHandle(handle, norm, nerr))
        return false;
    const uint160 id = IdFromScript(script);
    if (HasAttestation(id)) {
        std::string have;
        return GetAttestation(id, have, sigOut) && have == norm;
    }
    if (!IsXLookupNode())
        return false;
    std::string lerr;
    const xlookup::Status st = xlookup::LookupHandle(norm, lerr);
    if (st != xlookup::VERIFIED) {
        if (st == xlookup::NOT_VERIFIED)
            LogPrintf("lottery: drop @%s — X says not blue-check\n", norm);
        else
            LogPrintf("lottery: X lookup error @%s: %s\n", norm, lerr);
        return false;
    }
    if (!SignAttestation(id, norm, sigOut))
        return false;
    StoreAttestation(id, norm, sigOut);
    return true;
}

CScript MakeXvaCommitment(const std::vector<uint160>& sortedIds)
{
    std::vector<unsigned char> data;
    data.push_back((unsigned char)ATTEST_MAGIC[0]);
    data.push_back((unsigned char)ATTEST_MAGIC[1]);
    data.push_back((unsigned char)ATTEST_MAGIC[2]);
    data.push_back((unsigned char)ATTEST_MAGIC[3]);
    uint32_t n = 0;
    std::vector<unsigned char> body;
    for (const auto& id : sortedIds) {
        std::string handle;
        std::vector<unsigned char> sig;
        if (!GetAttestation(id, handle, sig) || sig.size() != 65)
            continue;
        if (handle.empty() || handle.size() > MAX_X_HANDLE)
            continue;
        n++;
        body.push_back((unsigned char)handle.size());
        body.insert(body.end(), handle.begin(), handle.end());
        body.insert(body.end(), id.begin(), id.end());
        body.insert(body.end(), sig.begin(), sig.end());
    }
    data.push_back(n & 0xff);
    data.push_back((n >> 8) & 0xff);
    data.push_back((n >> 16) & 0xff);
    data.push_back((n >> 24) & 0xff);
    data.insert(data.end(), body.begin(), body.end());
    return CScript() << OP_RETURN << data;
}

bool ParseXvaCommitment(const CScript& script,
                        std::vector<uint160>& ids,
                        std::vector<std::vector<unsigned char> >& sigs)
{
    ids.clear();
    sigs.clear();
    std::vector<std::string> handles;
    if (script.empty() || script[0] != OP_RETURN)
        return false;
    CScript::const_iterator pc = script.begin() + 1;
    std::vector<unsigned char> data;
    opcodetype opcode;
    if (!script.GetOp(pc, opcode, data))
        return false;
    if (data.size() < 8)
        return false;
    if (data[0] != (unsigned char)ATTEST_MAGIC[0] ||
        data[1] != (unsigned char)ATTEST_MAGIC[1] ||
        data[2] != (unsigned char)ATTEST_MAGIC[2] ||
        data[3] != (unsigned char)ATTEST_MAGIC[3])
        return false;
    uint32_t n = (uint32_t)data[4] | ((uint32_t)data[5] << 8) |
                 ((uint32_t)data[6] << 16) | ((uint32_t)data[7] << 24);
    if (n > 1024)
        return false;
    size_t off = 8;
    ids.resize(n);
    sigs.resize(n);
    for (uint32_t i = 0; i < n; i++) {
        if (off >= data.size())
            return false;
        const unsigned hlen = data[off++];
        if (hlen < 1 || hlen > MAX_X_HANDLE || off + hlen + 20 + 65 > data.size())
            return false;
        off += hlen;
        memcpy(ids[i].begin(), &data[off], 20);
        off += 20;
        sigs[i].assign(data.begin() + off, data.begin() + off + 65);
        off += 65;
    }
    return off == data.size();
}

bool CheckXvaForIds(const std::vector<uint160>& ids, const CScript& script)
{
    if (script.empty() || script[0] != OP_RETURN)
        return false;
    CScript::const_iterator pc = script.begin() + 1;
    std::vector<unsigned char> data;
    opcodetype opcode;
    if (!script.GetOp(pc, opcode, data) || data.size() < 8)
        return false;
    if (data[0] != (unsigned char)ATTEST_MAGIC[0] ||
        data[1] != (unsigned char)ATTEST_MAGIC[1] ||
        data[2] != (unsigned char)ATTEST_MAGIC[2] ||
        data[3] != (unsigned char)ATTEST_MAGIC[3])
        return false;
    uint32_t n = (uint32_t)data[4] | ((uint32_t)data[5] << 8) |
                 ((uint32_t)data[6] << 16) | ((uint32_t)data[7] << 24);
    if (n != ids.size() || n > 1024)
        return false;
    size_t off = 8;
    for (uint32_t i = 0; i < n; i++) {
        if (off >= data.size())
            return false;
        const unsigned hlen = data[off++];
        if (hlen < 1 || hlen > MAX_X_HANDLE || off + hlen + 20 + 65 > data.size())
            return false;
        const std::string handle(data.begin() + off, data.begin() + off + hlen);
        off += hlen;
        uint160 id;
        memcpy(id.begin(), &data[off], 20);
        off += 20;
        std::vector<unsigned char> sig(data.begin() + off, data.begin() + off + 65);
        off += 65;
        if (id != ids[i])
            return false;
        if (!VerifyAttestation(id, handle, sig))
            return false;
    }
    return off == data.size();
}

uint256 HeartbeatDigest(int64_t timestamp, const CScript& script,
                        const std::string& handle, uint64_t userId, bool xVerified)
{
    uint256 out;
    unsigned char tle[8];
    unsigned char ule[8];
    const unsigned char verifiedByte = xVerified ? 1 : 0;
    for (int i = 0; i < 8; i++) {
        tle[i] = (timestamp >> (8 * i)) & 0xff;
        ule[i] = (userId >> (8 * i)) & 0xff;
    }
    CSHA256()
        .Write(reinterpret_cast<const unsigned char*>(HEARTBEAT_MAGIC),
               sizeof(HEARTBEAT_MAGIC) - 1)
        .Write(tle, sizeof(tle))
        .Write(script.data(), script.size())
        .Write(reinterpret_cast<const unsigned char*>(handle.data()), handle.size())
        .Write(ule, sizeof(ule))
        .Write(&verifiedByte, 1)
        .Finalize(out.begin());
    return out;
}

bool SignHeartbeat(const CKey& key, int64_t timestamp, const CScript& script,
                   const std::string& handle, uint64_t userId,
                   std::vector<unsigned char>& sigOut, bool xVerified)
{
    sigOut.clear();
    if (!key.IsValid())
        return false;
    const uint256 digest = HeartbeatDigest(timestamp, script, handle, userId, xVerified);
    return key.SignCompact(digest, sigOut);
}

bool VerifyHeartbeatSig(const CScript& script, int64_t timestamp,
                        const std::string& handle, uint64_t userId,
                        const std::vector<unsigned char>& sig, bool xVerified)
{
    if (sig.empty() || sig.size() > MAX_HEARTBEAT_SIG)
        return false;
    if (script.empty() || script.size() > MAX_HEARTBEAT_SCRIPT)
        return false;
    std::string norm;
    std::string err;
    if (!NormalizeXHandle(handle, norm, err))
        return false;
    CTxDestination dest;
    if (!ExtractDestination(script, dest))
        return false;
    const CKeyID* keyID = boost::get<CKeyID>(&dest);
    if (!keyID)
        return false;
    CPubKey pub;
    const uint256 digest = HeartbeatDigest(timestamp, script, norm, userId, xVerified);
    if (!pub.RecoverCompact(digest, sig))
        return false;
    return pub.GetID() == *keyID;
}

bool SignLocalHeartbeat(int64_t timestamp, std::vector<unsigned char>& sigOut)
{
    sigOut.clear();
    const CScript script = GetRegistry().LocalScript();
    const XAccount x = GetRegistry().LocalXAccount();
    if (script.empty() || x.handle.empty())
        return false;
    CTxDestination dest;
    if (!ExtractDestination(script, dest))
        return false;
    const CKeyID* keyID = boost::get<CKeyID>(&dest);
    if (!keyID)
        return false;
    if (g_localPayoutKey.IsValid() && g_localPayoutKey.GetPubKey().GetID() == *keyID)
        // P2P xhb never carries the numeric X user id (login stays local).
        return SignHeartbeat(g_localPayoutKey, timestamp, script, x.handle, 0, sigOut,
                             xsession::SessionIsXVerified());
#ifdef ENABLE_WALLET
    CWallet* pwallet = FirstWalletOrNull();
    if (pwallet) {
        CKey wkey;
        {
            LOCK(pwallet->cs_wallet);
            if (pwallet->GetKey(*keyID, wkey) && wkey.IsValid())
                return SignHeartbeat(wkey, timestamp, script, x.handle, 0, sigOut,
                                     xsession::SessionIsXVerified());
        }
    }
#endif
    return false;
}

void Registry::SetLocalScript(const CScript& script)
{
    if (script.empty())
        return;
    const uint160 id = IdFromScript(script);
    LOCK(cs);
    if (localId != id && !localId.IsNull())
        nodes.erase(localId);
    localScript = script;
    localId = id;
}

CScript Registry::LocalScript() const
{
    LOCK(cs);
    return localScript;
}

uint160 Registry::LocalId() const
{
    LOCK(cs);
    return localId;
}

void Registry::SetLocalXAccount(const XAccount& account)
{
    LOCK(cs);
    localX = account;
}

XAccount Registry::LocalXAccount() const
{
    LOCK(cs);
    return localX;
}

bool Registry::LocalEligible() const
{
    XAccount x;
    {
        LOCK(cs);
        if (localScript.empty() || localX.handle.empty())
            return false;
        x = localX;
    }
    if (!xsession::SessionIsXVerified())
        return false;
    return true;
}

bool Registry::Heartbeat(const CScript& script, int64_t now,
                         const std::string& xHandle, uint64_t xUserId, bool xVerified)
{
    if (!xVerified)
        return false;
    if (script.empty() || script.size() > MAX_HEARTBEAT_SCRIPT)
        return false;
    const uint160 id = IdFromScript(script);
    if (id.IsNull())
        return false;
    std::string norm;
    std::string err;
    if (!NormalizeXHandle(xHandle, norm, err))
        return false;
    const CScript pin = GetAllowlist().PinnedScript(norm);
    if (!pin.empty() && pin != script)
        return false;

    LOCK(cs);
    auto hit = byHandle.find(norm);
    if (hit != byHandle.end() && hit->second != id) {
        auto existing = nodes.find(hit->second);
        if (existing != nodes.end() &&
            now - existing->second.lastSeen <= HEARTBEAT_TTL_SECONDS) {
            // Live handle already bound — unsigned/spoofed gossip cannot steal it.
            return false;
        }
        if (existing != nodes.end())
            nodes.erase(existing);
        byHandle.erase(hit);
    }
    auto nit = nodes.find(id);
    int64_t joined = now;
    if (nit != nodes.end() && nit->second.x.handle != norm) {
        if (now - nit->second.lastSeen <= HEARTBEAT_TTL_SECONDS)
            return false;
        byHandle.erase(nit->second.x.handle);
    } else if (nit != nodes.end()) {
        joined = nit->second.joined;
    }
    XAccount x;
    x.handle = norm;
    x.userId = xUserId;
    nodes[id] = ActiveNode{id, script, now, joined, x};
    byHandle[norm] = id;
    if (nodes.size() > MAX_ACTIVE_NODES) {
        auto oldest = nodes.begin();
        for (auto it = nodes.begin(); it != nodes.end(); ++it) {
            if (it->second.lastSeen < oldest->second.lastSeen)
                oldest = it;
        }
        if (oldest->first != localId) {
            byHandle.erase(oldest->second.x.handle);
            nodes.erase(oldest);
        }
    }
    return true;
}

void Registry::Reset()
{
    LOCK(cs);
    nodes.clear();
    byHandle.clear();
}

bool Registry::HeartbeatLocal(int64_t now)
{
    if (IsBakedSeed())
        return false;

    CScript s;
    XAccount x;
    {
        LOCK(cs);
        s = localScript;
        x = localX;
    }
    if (s.empty() || x.handle.empty())
        return false;
    if (!xsession::SessionIsXVerified())
        return false;
    if (RequireXAttestation()) {
        const uint160 id = IdFromScript(s);
        if (!HasAttestation(id)) {
            std::vector<unsigned char> sig;
            if (!LookupAndAttest(s, x.handle, sig))
                return false;
        }
    }
    return Heartbeat(s, now, x.handle, x.userId, true);
}

std::vector<uint160> Registry::ActiveIds(int64_t now) const
{
    std::vector<uint160> ids;
    LOCK(cs);
    ids.reserve(nodes.size());
    for (const auto& kv : nodes) {
        if (now - kv.second.lastSeen <= HEARTBEAT_TTL_SECONDS)
            ids.push_back(kv.first);
    }
    return ids;
}

std::vector<uint160> Registry::ActiveIdsForSlot(int64_t slot) const
{
    const int64_t slotStart = slot * SLOT_SECONDS;
    const int64_t now = GetTime();
    std::vector<uint160> ids;
    LOCK(cs);
    ids.reserve(nodes.size());
    for (const auto& kv : nodes) {
        const ActiveNode& n = kv.second;
        if (n.joined >= slotStart)
            continue; // arrived after the minute opened
        if (n.lastSeen < slotStart) {
            if (slotStart - n.lastSeen > HEARTBEAT_TTL_SECONDS)
                continue; // dead before the minute opened
        }
        ids.push_back(kv.first);
    }
    // First payday / first node in a slot: nobody was frozen yet.
    // Use live heartbeats so height 1 and regtest generate can commit XHB1.
    if (ids.empty()) {
        for (const auto& kv : nodes) {
            if (now - kv.second.lastSeen <= HEARTBEAT_TTL_SECONDS)
                ids.push_back(kv.first);
        }
    }
    return ids;
}

std::vector<ActiveNode> Registry::ActiveNodes(int64_t now) const
{
    std::vector<ActiveNode> out;
    LOCK(cs);
    out.reserve(nodes.size());
    for (const auto& kv : nodes) {
        if (now - kv.second.lastSeen <= HEARTBEAT_TTL_SECONDS)
            out.push_back(kv.second);
    }
    return out;
}

CScript Registry::ScriptFor(const uint160& id) const
{
    LOCK(cs);
    auto it = nodes.find(id);
    if (it == nodes.end())
        return CScript();
    return it->second.script;
}

size_t Registry::Count(int64_t now) const
{
    return ActiveIds(now).size();
}

CScript LoadOrCreateLocalScript()
{
    const fs::path keyPath = GetDataDir() / "lottery-payout.key";
    const fs::path scriptPath = GetDataDir() / "lottery-payout.dat";
    CKey key;
    if (fs::exists(keyPath)) {
        FILE* f = fsbridge::fopen(keyPath, "rb");
        if (f) {
            unsigned char raw[32];
            const size_t n = fread(raw, 1, sizeof(raw), f);
            fclose(f);
            if (n == sizeof(raw))
                key.Set(raw, raw + sizeof(raw), true);
        }
    }
    if (!key.IsValid()) {
        key.MakeNewKey(true);
        FILE* f = fsbridge::fopen(keyPath, "wb");
        if (f) {
            fwrite(key.begin(), 1, key.size(), f);
            fclose(f);
        }
    }
    g_localPayoutKey = key;
    const CScript script = GetScriptForDestination(CKeyID(key.GetPubKey().GetID()));
    FILE* sf = fsbridge::fopen(scriptPath, "wb");
    if (sf) {
        fwrite(script.data(), 1, script.size(), sf);
        fclose(sf);
    }
    return script;
}

int64_t SlotFromTime(int64_t unixTime)
{
    if (unixTime < 0)
        return 0;
    return unixTime / SLOT_SECONDS;
}

int64_t SlotFromHeight(int nHeight, int64_t genesisTime)
{
    if (nHeight <= 0)
        return SlotFromTime(genesisTime);
    return SlotFromTime(genesisTime) + nHeight;
}

int64_t SlotStartTime(int nHeight, int64_t genesisTime)
{
    return SlotFromHeight(nHeight, genesisTime) * SLOT_SECONDS;
}

bool CheckBlockTime(int nHeight, int64_t nTime, int64_t genesisTime,
                    int64_t nowLocal, bool fMineBlocksOnDemand,
                    CValidationState& state)
{
    if (fMineBlocksOnDemand)
        return true;
    if (nHeight <= 0)
        return true;

    const int64_t slot = SlotFromHeight(nHeight, genesisTime);
    const int64_t slotStart = slot * SLOT_SECONDS;
    const int64_t slotEnd = slotStart + SLOT_SECONDS;
    if (nTime < slotStart || nTime >= slotEnd) {
        return state.DoS(100, false, REJECT_INVALID, "bad-lottery-slot-time", false,
                         "block time is outside the locked 60-second lottery slot for this height");
    }
    const int64_t nowSlot = SlotFromTime(nowLocal);
    if (slot > nowSlot + MAX_FUTURE_SLOTS) {
        return state.Invalid(false, REJECT_INVALID, "time-too-new",
                             "lottery slot is too far in the future");
    }
    return true;
}

int64_t ClampTimeToSlot(int nHeight, int64_t genesisTime, int64_t now, int64_t mtp)
{
    const int64_t slotStart = SlotStartTime(nHeight, genesisTime);
    const int64_t slotEnd = slotStart + SLOT_SECONDS;
    int64_t t = now;
    if (t < slotStart)
        t = slotStart;
    if (t >= slotEnd)
        t = slotEnd - 1;
    if (t <= mtp)
        t = mtp + 1;
    return t;
}

int WinnerCount(int nHeight, int nSubsidyHalvingInterval)
{
    if (nHeight < 0)
        nHeight = 0;
    if (nSubsidyHalvingInterval <= 0)
        return 1;
    int halvings = nHeight / nSubsidyHalvingInterval;
    if (halvings > 1023)
        halvings = 1023;
    return halvings + 1;
}

uint256 Seed(const uint256& prevBlockHash, int64_t slot)
{
    uint256 out;
    uint64_t le = 0;
    for (int i = 0; i < 8; i++)
        reinterpret_cast<unsigned char*>(&le)[i] = (slot >> (8 * i)) & 0xff;

    CSHA256()
        .Write(prevBlockHash.begin(), prevBlockHash.size())
        .Write(reinterpret_cast<const unsigned char*>(&le), 8)
        .Finalize(out.begin());
    return out;
}

static uint64_t StreamU64(const uint256& seed, uint32_t counter)
{
    uint256 h;
    unsigned char ctr[4];
    ctr[0] = counter & 0xff;
    ctr[1] = (counter >> 8) & 0xff;
    ctr[2] = (counter >> 16) & 0xff;
    ctr[3] = (counter >> 24) & 0xff;
    CSHA256()
        .Write(seed.begin(), seed.size())
        .Write(ctr, 4)
        .Finalize(h.begin());
    uint64_t v = 0;
    for (int i = 0; i < 8; i++)
        v |= (uint64_t)h.begin()[i] << (8 * i);
    return v;
}

std::vector<uint160> SelectWinners(const std::vector<uint160>& sortedActive,
                                   const uint256& seed,
                                   int k)
{
    std::vector<uint160> pool = sortedActive;
    std::vector<uint160> winners;
    if (pool.empty() || k <= 0)
        return winners;
    if (k >= (int)pool.size())
        return pool;

    winners.reserve(k);
    for (int i = 0; i < k; i++) {
        uint64_t r = StreamU64(seed, (uint32_t)i);
        size_t remaining = pool.size() - (size_t)i;
        size_t idx = (size_t)(r % remaining) + (size_t)i;
        std::swap(pool[i], pool[idx]);
        winners.push_back(pool[i]);
    }
    return winners;
}

bool IsWinner(const uint160& id, const std::vector<uint160>& winners)
{
    return std::find(winners.begin(), winners.end(), id) != winners.end();
}

std::vector<CAmount> SplitReward(CAmount total, int winners)
{
    std::vector<CAmount> parts;
    if (winners <= 0 || total <= 0)
        return parts;
    parts.assign(winners, total / winners);
    CAmount rem = total % winners;
    for (int i = 0; i < rem; i++)
        parts[i] += 1;
    return parts;
}

Draw ComputeDraw(int nNextHeight,
                 const uint256& prevBlockHash,
                 int64_t genesisTime,
                 int nSubsidyHalvingInterval,
                 CAmount subsidy,
                 int64_t now)
{
    Draw d;
    d.nHeight = nNextHeight;
    d.slot = SlotFromHeight(nNextHeight, genesisTime);
    d.winnerCount = WinnerCount(nNextHeight, nSubsidyHalvingInterval);
    d.seed = Seed(prevBlockHash, d.slot);
    d.active = GetRegistry().ActiveIdsForSlot(d.slot);
    if (RequireXAttestation()) {
        std::vector<uint160> ok;
        ok.reserve(d.active.size());
        for (const auto& id : d.active) {
            if (HasAttestation(id))
                ok.push_back(id);
        }
        d.active.swap(ok);
    }
    (void)now;
    d.winners = SelectWinners(d.active, d.seed, d.winnerCount);
    d.rewards = SplitReward(subsidy, (int)d.winners.size());
    return d;
}

CScript MakeActiveSetCommitment(const std::vector<uint160>& sortedIds)
{
    std::vector<unsigned char> data;
    data.reserve(8 + sortedIds.size() * 20);
    data.push_back((unsigned char)COMMIT_MAGIC[0]);
    data.push_back((unsigned char)COMMIT_MAGIC[1]);
    data.push_back((unsigned char)COMMIT_MAGIC[2]);
    data.push_back((unsigned char)COMMIT_MAGIC[3]);
    uint32_t n = (uint32_t)sortedIds.size();
    data.push_back(n & 0xff);
    data.push_back((n >> 8) & 0xff);
    data.push_back((n >> 16) & 0xff);
    data.push_back((n >> 24) & 0xff);
    for (const auto& id : sortedIds)
        data.insert(data.end(), id.begin(), id.end());
    return CScript() << OP_RETURN << data;
}

bool ParseActiveSetCommitment(const CScript& script, std::vector<uint160>& ids)
{
    ids.clear();
    if (script.empty() || script[0] != OP_RETURN)
        return false;
    CScript::const_iterator pc = script.begin() + 1;
    std::vector<unsigned char> data;
    opcodetype opcode;
    if (!script.GetOp(pc, opcode, data))
        return false;
    if (data.size() < 8)
        return false;
    if (data[0] != (unsigned char)COMMIT_MAGIC[0] ||
        data[1] != (unsigned char)COMMIT_MAGIC[1] ||
        data[2] != (unsigned char)COMMIT_MAGIC[2] ||
        data[3] != (unsigned char)COMMIT_MAGIC[3])
        return false;
    uint32_t n = (uint32_t)data[4] | ((uint32_t)data[5] << 8) |
                 ((uint32_t)data[6] << 16) | ((uint32_t)data[7] << 24);
    if (n > 1024)
        return false;
    if (data.size() != 8 + (size_t)n * 20)
        return false;
    ids.resize(n);
    for (uint32_t i = 0; i < n; i++)
        memcpy(ids[i].begin(), &data[8 + (size_t)i * 20], 20);
    for (uint32_t i = 1; i < n; i++) {
        if (!(ids[i - 1] < ids[i]))
            return false;
    }
    return true;
}

void ApplyCoinbasePayouts(CBlock& block,
                          int nHeight,
                          const uint256& prevBlockHash,
                          int64_t genesisTime,
                          int nSubsidyHalvingInterval,
                          CAmount subsidy,
                          CAmount nFees,
                          const CScript& producerScript)
{
    if (nHeight < 1 || block.vtx.empty())
        return;

    const int64_t now = GetTime();
    if (!producerScript.empty())
        GetRegistry().SetLocalScript(producerScript);
    GetRegistry().HeartbeatLocal(now);

    Draw draw = ComputeDraw(nHeight, prevBlockHash, genesisTime,
                            nSubsidyHalvingInterval, subsidy, now);
    if (draw.winners.empty())
        return;

    std::vector<CAmount> parts = SplitReward(subsidy, (int)draw.winners.size());
    CMutableTransaction tx(*block.vtx[0]);
    tx.vout.clear();

    for (size_t i = 0; i < draw.winners.size(); i++) {
        CScript dest = GetRegistry().ScriptFor(draw.winners[i]);
        if (dest.empty())
            dest = producerScript;
        CAmount value = parts.empty() ? 0 : parts[i];
        if (i == 0)
            value += nFees;
        tx.vout.push_back(CTxOut(value, dest));
    }

    tx.vout.push_back(CTxOut(0, MakeActiveSetCommitment(draw.active)));
    if (RequireXAttestation()) {
        tx.vout.push_back(CTxOut(0, MakeXvaCommitment(draw.active)));
        const CScript seedSig = MakeSeedBlockCommitment(nHeight, prevBlockHash, draw.active);
        if (!seedSig.empty())
            tx.vout.push_back(CTxOut(0, seedSig));
    }
    block.vtx[0] = MakeTransactionRef(std::move(tx));
}

bool CheckLotteryCoinbase(const CBlock& block,
                          int nHeight,
                          const uint256& prevBlockHash,
                          int64_t genesisTime,
                          int nSubsidyHalvingInterval,
                          CAmount subsidy,
                          CValidationState& state)
{
    if (nHeight < 1)
        return true;
    if (block.vtx.empty() || !block.vtx[0]->IsCoinBase())
        return state.DoS(100, false, REJECT_INVALID, "bad-cb-lottery", false, "missing coinbase");

    const CTransaction& cb = *block.vtx[0];
    std::vector<uint160> committed;
    bool found = false;
    bool foundXva = false;
    bool foundXsd = false;
    CScript xvaScript;
    CScript xsdScript;
    for (const auto& out : cb.vout) {
        std::vector<uint160> tmp;
        if (!found && ParseActiveSetCommitment(out.scriptPubKey, tmp)) {
            committed = tmp;
            found = true;
            continue;
        }
        std::vector<uint160> xids;
        std::vector<std::vector<unsigned char> > xsigs;
        if (!foundXva && ParseXvaCommitment(out.scriptPubKey, xids, xsigs)) {
            xvaScript = out.scriptPubKey;
            foundXva = true;
            continue;
        }
        if (!foundXsd && out.scriptPubKey.size() > 5 &&
            out.scriptPubKey[0] == OP_RETURN) {
            CScript::const_iterator pc = out.scriptPubKey.begin() + 1;
            std::vector<unsigned char> data;
            opcodetype opcode;
            if (out.scriptPubKey.GetOp(pc, opcode, data) && data.size() >= 4 &&
                data[0] == (unsigned char)SEED_BLOCK_MAGIC[0] &&
                data[1] == (unsigned char)SEED_BLOCK_MAGIC[1] &&
                data[2] == (unsigned char)SEED_BLOCK_MAGIC[2] &&
                data[3] == (unsigned char)SEED_BLOCK_MAGIC[3]) {
                xsdScript = out.scriptPubKey;
                foundXsd = true;
                continue;
            }
        }
    }
    if (!found)
        return state.DoS(100, false, REJECT_INVALID, "bad-cb-lottery-commit", false,
                         "coinbase missing XHB1 active-set commitment");
    if (RequireXAttestation()) {
        if (!foundXva)
            return state.DoS(100, false, REJECT_INVALID, "bad-cb-lottery-xva", false,
                             "coinbase missing XVA1 blue-check stamps");
        if (!CheckXvaForIds(committed, xvaScript))
            return state.DoS(100, false, REJECT_INVALID, "bad-cb-lottery-xva", false,
                             "XVA1 does not stamp every XHB1 id as a live X blue check");
        if (!foundXsd || !CheckSeedBlockCommitment(nHeight, prevBlockHash, committed, xsdScript))
            return state.DoS(100, false, REJECT_INVALID, "bad-cb-lottery-xsd", false,
                             "coinbase missing baked-seed XSD1 signature");
    }

    const int64_t slot = SlotFromHeight(nHeight, genesisTime);
    const uint256 seed = Seed(prevBlockHash, slot);
    const int k = WinnerCount(nHeight, nSubsidyHalvingInterval);
    const std::vector<uint160> winners = SelectWinners(committed, seed, k);
    if (winners.empty())
        return state.DoS(100, false, REJECT_INVALID, "bad-cb-lottery-empty", false,
                         "committed active set produced no winners");

    std::vector<CTxOut> pays;
    pays.reserve(cb.vout.size());
    for (const auto& out : cb.vout) {
        if (out.scriptPubKey.empty())
            return state.DoS(100, false, REJECT_INVALID, "bad-cb-lottery-script", false,
                             "empty coinbase script");
        if (out.scriptPubKey[0] == OP_RETURN)
            continue;
        pays.push_back(out);
    }

    CAmount paid = 0;
    for (const auto& p : pays) {
        if (p.nValue < 0 || !MoneyRange(p.nValue))
            return state.DoS(100, false, REJECT_INVALID, "bad-cb-lottery-range", false,
                             "winner output value out of range");
        if (paid > MAX_MONEY - p.nValue)
            return state.DoS(100, false, REJECT_INVALID, "bad-cb-lottery-overflow", false,
                             "winner output sum overflows");
        paid += p.nValue;
    }
    if (!MoneyRange(paid) || paid < subsidy)
        return state.DoS(100, false, REJECT_INVALID, "bad-cb-lottery-amount", false,
                         "coinbase pays less than subsidy");
    const CAmount nFees = paid - subsidy;
    std::vector<CAmount> parts = SplitReward(subsidy, (int)winners.size());

    if (pays.size() != winners.size())
        return state.DoS(100, false, REJECT_INVALID, "bad-cb-lottery-count", false,
                         strprintf("expected %u winner outputs, got %u",
                                   (unsigned)winners.size(), (unsigned)pays.size()));
    for (size_t i = 0; i < winners.size(); i++) {
        if (IdFromScript(pays[i].scriptPubKey) != winners[i])
            return state.DoS(100, false, REJECT_INVALID, "bad-cb-lottery-winner", false,
                             strprintf("payout %u script does not match winner", (unsigned)i));
        CAmount expect = parts[i];
        if (i == 0)
            expect += nFees;
        if (pays[i].nValue != expect)
            return state.DoS(100, false, REJECT_INVALID, "bad-cb-lottery-split", false,
                             strprintf("payout %u amount mismatch", (unsigned)i));
    }
    return true;
}

static CWallet* FirstWalletOrNull()
{
#ifdef ENABLE_WALLET
    if (vpwallets.empty())
        return nullptr;
    return vpwallets[0];
#else
    return nullptr;
#endif
}

static bool ProduceOneBlock(const CChainParams& chainparams)
{
    if (!MayProduceBlock()) {
        if (RequireXAttestation() && !IsBakedSeed())
            LogPrintf("lottery: only the baked seed produces main/test blocks\n");
        else
            LogPrintf("lottery: refusing to produce; local node is not X Verified (blue check)\n");
        return false;
    }
#ifdef ENABLE_WALLET
    CWallet* pWallet = FirstWalletOrNull();
    if (!pWallet) {
        LogPrintf("lottery: no wallet; cannot produce a block\n");
        return false;
    }
    std::shared_ptr<CReserveScript> coinbaseScript;
    {
        LOCK(pWallet->cs_wallet);
        pWallet->GetScriptForMining(coinbaseScript);
    }
    if (!coinbaseScript || coinbaseScript->reserveScript.empty()) {
        LogPrintf("lottery: no coinbase script (empty keypool?)\n");
        return false;
    }
    GetRegistry().SetLocalScript(coinbaseScript->reserveScript);

    std::unique_ptr<CBlockTemplate> pblocktemplate;
    try {
        pblocktemplate = BlockAssembler(chainparams).CreateNewBlock(coinbaseScript->reserveScript);
    } catch (const std::exception& e) {
        LogPrintf("lottery: CreateNewBlock failed: %s\n", e.what());
        return false;
    }
    if (!pblocktemplate) {
        LogPrintf("lottery: CreateNewBlock failed\n");
        return false;
    }

    CBlock* pblock = &pblocktemplate->block;
    {
        LOCK(cs_main);
        CBlockIndex* pindexPrev = chainActive.Tip();
        if (!pindexPrev)
            return false;
        unsigned int extra = 0;
        IncrementExtraNonce(pblock, pindexPrev, extra);
        if (!chainparams.MineBlocksOnDemand()) {
            const int nHeight = pindexPrev->nHeight + 1;
            pblock->nTime = ClampTimeToSlot(nHeight,
                                            chainparams.GenesisBlock().nTime,
                                            GetTime(),
                                            pindexPrev->GetMedianTimePast());
        }
    }

    std::shared_ptr<const CBlock> shared = std::make_shared<const CBlock>(*pblock);
    if (!ProcessNewBlock(chainparams, shared, true, nullptr)) {
        LogPrintf("lottery: ProcessNewBlock rejected produced block\n");
        return false;
    }
    coinbaseScript->KeepScript();
    LogPrintf("lottery: produced block %s\n", pblock->GetHash().GetHex());
    return true;
#else
    (void)chainparams;
    LogPrintf("lottery: wallet disabled; cannot produce a block\n");
    return false;
#endif
}

static void ProducerThread(const CChainParams& chainparams)
{
    RenameThread("xcoin-lottery");
    LogPrintf("lottery: producer started\n");

    GetRegistry().SetLocalScript(LoadOrCreateLocalScript());
    int64_t lastProducedSlot = -1;
    int64_t lastProducedWallMinute = -1;
    bool fLoggedIneligible = false;

    try {
        while (true) {
            boost::this_thread::interruption_point();
            const int64_t now = GetTime();

#ifdef ENABLE_WALLET
            // Adopt the wallet coinbase/payout script once. Do not touch the keypool
            // every second from this thread (that raced and OOMed on start).
            static bool fAdoptedWallet = false;
            if (!fAdoptedWallet) {
                CWallet* pWallet = FirstWalletOrNull();
                if (pWallet) {
                    std::shared_ptr<CReserveScript> coinbaseScript;
                    {
                        LOCK(pWallet->cs_wallet);
                        pWallet->GetScriptForMining(coinbaseScript);
                    }
                    if (coinbaseScript && !coinbaseScript->reserveScript.empty()) {
                        GetRegistry().SetLocalScript(coinbaseScript->reserveScript);
                        fAdoptedWallet = true;
                    }
                }
            }
#endif
            GetRegistry().HeartbeatLocal(now);

            if (!MayProduceBlock()) {
                if (!fLoggedIneligible) {
                    LogPrintf("lottery: not X Verified (users/me.verified); sync/relay only, not producing\n");
                    fLoggedIneligible = true;
                }
                MilliSleep(1000);
                continue;
            }
            fLoggedIneligible = false;

            if (chainparams.MineBlocksOnDemand()) {
                MilliSleep(1000);
                continue;
            }

            CBlockIndex* tip = nullptr;
            {
                LOCK(cs_main);
                tip = chainActive.Tip();
            }
            if (!tip) {
                MilliSleep(1000);
                continue;
            }

            const Consensus::Params& consensus = chainparams.GetConsensus();
            const int nextHeight = tip->nHeight + 1;
            const int64_t slot = SlotFromHeight(nextHeight, chainparams.GenesisBlock().nTime);
            const int64_t currentSlot = SlotFromTime(now);

            if (currentSlot > slot + 1) {
                static int lastCatchupLogHeight = -1;
                if (nextHeight != lastCatchupLogHeight) {
                    LogPrintf("lottery: catching up height %d (%d minutes behind wall clock)\n",
                              nextHeight, (int)(currentSlot - slot));
                    lastCatchupLogHeight = nextHeight;
                }
            }

            if (chainparams.NetworkIDString() == CBaseChainParams::MAIN) {
                // Catch-up may target old lottery slots; still ≤1 emit per wall minute.
                if (GetTime() / 60 == lastProducedWallMinute) {
                    MilliSleep(1000);
                    continue;
                }
            } else {
                if (currentSlot < slot) {
                    MilliSleep(1000);
                    continue;
                }
                // Let heartbeats settle so honest nodes freeze the same set.
                if (now < slot * SLOT_SECONDS + SETTLE_SECONDS) {
                    MilliSleep(200);
                    continue;
                }
            }
            if (slot == lastProducedSlot) {
                MilliSleep(1000);
                continue;
            }

            {
                LOCK(cs_main);
                if (chainActive.Height() >= nextHeight) {
                    lastProducedSlot = slot;
                    MilliSleep(1000);
                    continue;
                }
                bool haveSibling = false;
                for (const auto& kv : mapBlockIndex) {
                    const CBlockIndex* p = kv.second;
                    if (p && p->pprev == tip && p->nHeight == nextHeight &&
                        (p->nStatus & BLOCK_HAVE_DATA)) {
                        haveSibling = true;
                        break;
                    }
                }
                if (haveSibling) {
                    LogPrintf("lottery: height %d already has a block; not emitting\n", nextHeight);
                    lastProducedSlot = slot;
                    MilliSleep(1000);
                    continue;
                }
            }

            CAmount subsidy = GetBlockSubsidy(nextHeight, consensus);
            Draw draw = ComputeDraw(nextHeight, tip->GetBlockHash(),
                                    chainparams.GenesisBlock().nTime,
                                    consensus.nSubsidyHalvingInterval,
                                    subsidy, now);

            if (draw.winners.empty()) {
                MilliSleep(1000);
                continue;
            }

            // Baked seed produces for stamped users (it is not a lottery winner).
            // On regtest, only winners[0] emits.
            if (IsBakedSeed() || GetRegistry().LocalId() == draw.winners[0]) {
                if (ProduceOneBlock(chainparams)) {
                    lastProducedSlot = slot;
                    lastProducedWallMinute = GetTime() / 60;
                }
            }
            MilliSleep(1000);
        }
    } catch (const boost::thread_interrupted&) {
        LogPrintf("lottery: producer stopped\n");
        throw;
    }
}

void StartProducer(const CChainParams& chainparams)
{
    LOCK(cs_producer);
    if (g_producerThreads)
        return;
    InitEligibility();
    GetRegistry().SetLocalScript(LoadOrCreateLocalScript());
    GetRegistry().HeartbeatLocal(GetTime());
    g_producerThreads = new boost::thread_group();
    g_producerThreads->create_thread(boost::bind(&ProducerThread, boost::cref(chainparams)));
}

void StopProducer()
{
    LOCK(cs_producer);
    if (!g_producerThreads)
        return;
    g_producerThreads->interrupt_all();
    delete g_producerThreads;
    g_producerThreads = nullptr;
}

bool IsProducerRunning()
{
    LOCK(cs_producer);
    return g_producerThreads != nullptr;
}

} // namespace lottery
